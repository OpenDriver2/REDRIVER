/*
 * Engine/3dfx/tset.c - texture sets and texture memory slots
 *
 * Rebuilt from Driver_sym2_94.i64 (IDA folder "Engine\3dfx\tset.c").
 */

#include <string.h>

#include "dr_types.h"
#include "Engine/3dfx/tset.h"

#include "../../D3DGlide/glide_ptrs.h"
#include "D3DGlide/system.h"
#include "dr_globals.h"
#include "map.h"

// a 64k slot of (emulated) texture memory
typedef struct TextureSlot
{
	char flags;
	unsigned char texture_set;		// 255: free
	char unk_3;
	char unk_4;
	int texture_ram_address;
	GrTexInfo TInfo;
} TextureSlot;

#define MAX_TEXTURE_SETS	256
#define MAX_TEXTURE_SLOTS	64
#define TEXTURE_SLOT_SIZE	0x10000
#define MAX_REGION_TSETS	64		// texture sets per region in gUsedTSets
#define MAX_TO_BE_LOADED	64

#define TSET_FLAG_2			0x2		// NOTE: meaning unknown; makes AddTextureSet16 mark its slots with 4
#define TSET_FLAG_8BIT		0x4		// 8 bit (paletted) set of a parent / child pair
#define TSET_FLAG_CHILD		0x8		// the child of the pair (the parent is the set before)
#define TSET_FLAG_QUEUED	0x10	// in ToBeLoaded

#define SLOT_FULL			0x1		// beyond the texture memory
#define SLOT_USED			0x2
#define SLOT_FLAG_4			0x4

int AllocateTextureSlot(void);
void LoadInRegionTSets(int region);
void LoadInTextureSets(void);
void LoadTextureSet(int tset);

// uninitialised data (0xb9b61c, start of the file's block per IDA mark)
int ToBeLoadedNum;
static int region_tsets[4];		// NOTE: unnamed in IDA (tset.dword_B9B620); name guessed
int current_region;
int ToBeLoaded[MAX_TO_BE_LOADED];

// NOTE: bss globals in the linker's common area; defined here as tset.c manages them
TextureSet TextureSlots[MAX_TEXTURE_SETS];
TextureSlot gTSlot[MAX_TEXTURE_SLOTS];

// NOTE: moved here from the generated stubs; no IDA mark, the owner is a guess from the users
// and the neighbouring data
char gUsedTSets[1024][70];

// [0x48a290] [D]
void InitTextureSpace(void)
{
	FxU32 mem_min;
	FxU32 mem_max;
	int i;

	memset(gTSlot, 0, sizeof(gTSlot));
	memset(region_tsets, 0, sizeof(region_tsets));
	memset(TextureSlots, 0, sizeof(TextureSlots));

	ToBeLoadedNum = 0;
	current_region = -1;

	for (i = 0; i < MAX_TEXTURE_SETS; i++)
	{
		TextureSlots[i].flags = 0;
		TextureSlots[i].texture_slot = 255;
		TextureSlots[i].data = NULL;
		TextureSlots[i].texture_ram_address = 0;
		TextureSlots[i].TInfo = &gTSlot[0].TInfo;
	}

	mem_min = grTexMinAddress(GR_TMU0);
	mem_max = grTexMaxAddress(GR_TMU0);

	for (i = 0; i < MAX_TEXTURE_SLOTS; i++)
	{
		gTSlot[i].flags = 0;
		gTSlot[i].texture_ram_address = mem_min;
		gTSlot[i].texture_set = 255;
		gTSlot[i].TInfo.smallLod = GR_LOD_256;
		gTSlot[i].TInfo.largeLod = GR_LOD_256;
		gTSlot[i].TInfo.aspectRatio = GR_ASPECT_1x1;
		gTSlot[i].TInfo.data = NULL;
		gTSlot[i].TInfo.format = GR_TEXFMT_P_8;

		if (mem_min >= mem_max)
			gTSlot[i].flags = SLOT_FULL;

		mem_min += TEXTURE_SLOT_SIZE;
	}

	for (i = 0; i < 4; i++)
		region_tsets[i] = -1;
}

// [0x48a446] [D]
// When the player enters another region, marks the slots of its texture sets used and loads
// the ones not in texture memory.
void CheckUnpackRegionTSets(void)
{
	int slot;

	if (player.current_region != current_region)
	{
		ToBeLoadedNum = 0;

		for (slot = 0; slot < MAX_TEXTURE_SLOTS; slot++)
			gTSlot[slot].flags &= ~SLOT_USED;

		LoadInRegionTSets(player.current_region);

		if (ToBeLoadedNum)
			LoadInTextureSets();
	}

	current_region = player.current_region;
}

// [0x48a4c0] [D]
// Loads an 8 bit texture set pair (parent and child, one slot each) into a pair of slots;
// either set of the pair may be given. NULL data: the set's own data.
void AddTextureSet8(int Parent, char *ParentData, char *ChildData)
{
	int other;
	int slot;
	int Child;
	char flags;

	flags = 0;

	if (TextureSlots[Parent].flags & TSET_FLAG_CHILD)
	{
		if (TextureSlots[Parent].flags & TSET_FLAG_2)
			flags = SLOT_FLAG_4;

		Parent--;
	}

	if (TextureSlots[Parent].texture_slot != 255)
		return;

	slot = AllocateTextureSlot();

	if (slot == -1)
		return;

	if (TextureSlots[Parent].flags & TSET_FLAG_2)
		flags = SLOT_FLAG_4;

	// evict the texture set that was there
	other = gTSlot[slot].texture_set;

	if (other != 255)
		TextureSlots[other].texture_slot = 255;

	if (!ParentData)
		ParentData = (char *)TextureSlots[Parent].data;

	gTSlot[slot].TInfo.smallLod = GR_LOD_256;
	gTSlot[slot].TInfo.largeLod = GR_LOD_256;
	gTSlot[slot].TInfo.aspectRatio = GR_ASPECT_1x1;
	gTSlot[slot].TInfo.data = ParentData;
	gTSlot[slot].TInfo.format = GR_TEXFMT_P_8;
	gTSlot[slot].texture_set = Parent;
	gTSlot[slot].flags |= flags | SLOT_USED;

	grTexDownloadMipMap2(GR_TMU0, gTSlot[slot].texture_ram_address, GR_MIPMAPLEVELMASK_BOTH, &gTSlot[slot].TInfo, Parent);

	TextureSlots[Parent].texture_slot = slot;
	TextureSlots[Parent].texture_ram_address = gTSlot[slot].texture_ram_address;
	TextureSlots[Parent].TInfo = &gTSlot[slot].TInfo;

	// the child in the following slot
	slot++;
	Child = Parent + 1;

	other = gTSlot[slot].texture_set;

	if (other != 255)
		TextureSlots[other].texture_slot = 255;

	if (!ChildData)
		ChildData = (char *)TextureSlots[Child].data;

	gTSlot[slot].TInfo.smallLod = GR_LOD_256;
	gTSlot[slot].TInfo.largeLod = GR_LOD_256;
	gTSlot[slot].TInfo.aspectRatio = GR_ASPECT_1x1;
	gTSlot[slot].TInfo.data = ChildData;
	gTSlot[slot].TInfo.format = GR_TEXFMT_P_8;
	gTSlot[slot].texture_set = Child;
	gTSlot[slot].flags |= flags | SLOT_USED;

	grTexDownloadMipMap2(GR_TMU0, gTSlot[slot].texture_ram_address, GR_MIPMAPLEVELMASK_BOTH, &gTSlot[slot].TInfo, Child);

	TextureSlots[Child].texture_slot = slot;
	TextureSlots[Child].texture_ram_address = gTSlot[slot].texture_ram_address;
	TextureSlots[Child].TInfo = &gTSlot[slot].TInfo;
}

// [0x48a7a9] [D]
// Loads a 16 bit (ARGB1555) texture set into a pair of slots.
void AddTextureSet16(int tset, void *data)
{
	int other;
	int slot;
	char flags;

	flags = 0;

	if (TextureSlots[tset].texture_slot != 255)
		return;

	slot = AllocateTextureSlot();

	if (slot == -1)
		return;

	if (TextureSlots[tset].flags & TSET_FLAG_2)
		flags = SLOT_FLAG_4;

	// evict the texture set that was there
	other = gTSlot[slot].texture_set;

	if (other != 255)
		TextureSlots[other].texture_slot = 255;

	if (!data)
		data = TextureSlots[tset].data;

	gTSlot[slot].TInfo.smallLod = GR_LOD_256;
	gTSlot[slot].TInfo.largeLod = GR_LOD_256;
	gTSlot[slot].TInfo.aspectRatio = GR_ASPECT_1x1;
	gTSlot[slot].TInfo.data = data;
	gTSlot[slot].TInfo.format = GR_TEXFMT_ARGB_1555;
	gTSlot[slot].texture_set = tset;
	gTSlot[slot].flags |= flags | SLOT_USED;

	grTexDownloadMipMap2(GR_TMU0, gTSlot[slot].texture_ram_address, GR_MIPMAPLEVELMASK_BOTH, &gTSlot[slot].TInfo, tset);

	TextureSlots[tset].texture_slot = slot;
	TextureSlots[tset].texture_ram_address = gTSlot[slot].texture_ram_address;
	TextureSlots[tset].TInfo = &gTSlot[slot].TInfo;

	// a 16 bit set takes the following slot too
	slot++;

	other = gTSlot[slot].texture_set;

	if (other != 255)
		TextureSlots[other].texture_slot = 255;

	gTSlot[slot].TInfo.smallLod = GR_LOD_256;
	gTSlot[slot].TInfo.largeLod = GR_LOD_256;
	gTSlot[slot].TInfo.aspectRatio = GR_ASPECT_1x1;
	gTSlot[slot].TInfo.data = TextureSlots[tset].data;
	gTSlot[slot].TInfo.format = GR_TEXFMT_ARGB_1555;
	gTSlot[slot].texture_set = tset;
	gTSlot[slot].flags |= flags | SLOT_USED;
}

// [0x48a9db] [D]
// New data for an 8 bit texture set in texture memory (the sky of InitHorizon).
void ReplaceTextureSet(int num, void *tmem)
{
	int slot;

	if (TextureSlots[num].texture_slot == 255)
		return;

	slot = TextureSlots[num].texture_slot;

	TextureSlots[num].data = tmem;
	gTSlot[slot].TInfo.data = tmem;

	ReplaceEmulatedTexture8(GR_TMU0, gTSlot[slot].texture_ram_address, GR_MIPMAPLEVELMASK_BOTH, &gTSlot[slot].TInfo, num);
}

// [0x48aa51] [D]
// NOTE: no name in the database; an empty function
void sub_48AA51(void)
{
}

// [0x48aa56] [D]
// Finds a free pair of slots.
int AllocateTextureSlot(void)
{
	int i;

	for (i = 0; i < MAX_TEXTURE_SLOTS; i += 2)
	{
		if (!(gTSlot[i].flags & (SLOT_FULL | SLOT_USED | SLOT_FLAG_4)))
			return i;
	}

	return -1;
}

// [0x48aa95] [D]
// Queues the texture sets of a region (gUsedTSets, 255 ends the list) that are not in texture
// memory; marks the slots of the others used.
void LoadInRegionTSets(int region)
{
	int TSetCount;
	int NewTSet;

	if (region < 0 || region >= num_regions)
		return;

	for (TSetCount = 0; TSetCount < MAX_REGION_TSETS; TSetCount++)
	{
		NewTSet = (unsigned char)gUsedTSets[region][TSetCount];

		if (NewTSet == 255)
			break;

		if (TextureSlots[NewTSet].texture_slot == 255)
		{
			if (!(TextureSlots[NewTSet].flags & TSET_FLAG_QUEUED) && ToBeLoadedNum < MAX_TO_BE_LOADED)
			{
				TextureSlots[NewTSet].flags |= TSET_FLAG_QUEUED;
				ToBeLoaded[ToBeLoadedNum++] = NewTSet;
			}
		}
		else
		{
			gTSlot[TextureSlots[NewTSet].texture_slot].flags |= SLOT_USED;
		}
	}
}

// [0x48ab97] [D]
void LoadInTextureSets(void)
{
	int i;
	int NewTSet;

	for (i = 0; i < ToBeLoadedNum; i++)
	{
		NewTSet = ToBeLoaded[i];

		TextureSlots[NewTSet].flags &= ~TSET_FLAG_QUEUED;

		LoadTextureSet(NewTSet);
	}

	ToBeLoadedNum = 0;
}

// [0x48abfe] [D]
void LoadTextureSet(int tset)
{
	if (TextureSlots[tset].flags & TSET_FLAG_8BIT)
		AddTextureSet8(tset, NULL, NULL);
	else
		AddTextureSet16(tset, NULL);
}
