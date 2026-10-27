/*
 * Engine/3dfx/tset.c - texture sets and texture memory slots
 *
 * Rebuilt from Driver_sym2_94.i64 (IDA folder "Engine\3dfx\tset.c"); partially translated.
 */

#include <string.h>

#include "dr_types.h"
#include "Engine/3dfx/tset.h"

#include "../../D3DGlide/glide_ptrs.h"
#include "D3DGlide/system.h"

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

#define TSET_FLAG_2			0x2		// NOTE: meaning unknown; makes AddTextureSet16 mark its slots with 4

#define SLOT_FULL			0x1		// beyond the texture memory
#define SLOT_USED			0x2
#define SLOT_FLAG_4			0x4

int AllocateTextureSlot(void);

// uninitialised data (0xb9b61c, start of the file's block per IDA mark)
int ToBeLoadedNum;
static int region_tsets[4];		// NOTE: unnamed in IDA (tset.dword_B9B620); name guessed
int current_region;

// NOTE: bss globals in the linker's common area; defined here as tset.c manages them
TextureSet TextureSlots[MAX_TEXTURE_SETS];
TextureSlot gTSlot[MAX_TEXTURE_SLOTS];

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
