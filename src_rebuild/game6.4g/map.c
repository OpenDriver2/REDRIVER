/*
 * map.c - level file (LEV) loading, map regions and cells, drawing of the map
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "map.c", 0x4ed4e0 - 0x4f1e52).
 *
 * A LEV file is a list of lumps (type, size, data): textures, models, the cell map, roads ...
 * The cell map is split into regions (region_size x region_size cells); four regions around the
 * player are unpacked at a time into a 2 x 2 "barrel" (slot = (region_x & 1) + 2 * (region_z & 1)).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dr_types.h"
#include "map.h"
#include "shell.h"
#include "D3DGlide/glide_ptrs.h"
#include "asmmath.h"
#include "camera.h"
#include "chairs.h"
#include "D3DGlide/system.h"
#include "debug.h"
#include "denting.h"
#include "dr_globals.h"
#include "MScript.h"
#include "effects.h"
#include "Engine/3dfx/gfxinit.h"
#include "Engine/3dfx/object.h"
#include "Engine/3dfx/polydraw.h"
#include "Engine/3dfx/tset.h"
#include "event.h"
#include "general.h"
#include "handling.h"
#include "lamps.h"
#include "objanim.h"
#include "overlay.h"
#include "pres.h"
#include "roadbits.h"
#include "testmath.h"

#define TEX_NAME_LEN			8

#define MAX_REGION_DATA_SLOTS	20480	// CELL_DATA slots of one unpacked region
#define MAX_STRADDLERS			8192	// CELL_OBJECT slots of one unpacked region (after the straddlers)
#define MAX_REGION_OBJECTS		8192

#define MAX_MODELS				1152

#define MAX_DRAWN_OBJECTS		1024
#define MAX_NEON_OBJECTS		512

#define TSET_SIZE_8BIT			0x10000
#define TSET_SIZE_16BIT			0x20000

// tset flags in the texture lump
#define TSET_PERMANENT			0x1
#define TSET_DAMAGE				0x2		// player car damage page
#define TSET_CAR				0x4		// a car's page (ParentData = car model)
#define TSET_CAR_DAMAGE			0x8
#define TSET_8BIT				0x10	// parent and child 8 bit pages

// lump types
#define LUMP_TEXTURES			0
#define LUMP_MODELS				1
#define LUMP_MAP				2
#define LUMP_MOVEABLE_OBJECTS	4
#define LUMP_TEXTURE_INFO		5
#define LUMP_ROAD_INFO			6
#define LUMP_ROAD_MAP			7
#define LUMP_ROADS				8
#define LUMP_JUNCTIONS			9
#define LUMP_SURFACE			10
#define LUMP_MAP_INFO			11
#define LUMP_MODEL_NAMES		12
#define LUMP_EVENT_MODELS		13
#define LUMP_PVS_REGIONS		14
#define LUMP_REGION_TEXTURES	15
#define LUMP_ROAD_BOUNDS		16
#define LUMP_JUNC_BOUNDS		17
#define LUMP_CAMERA_PATHS		18
#define LUMP_LAMPS				19
#define LUMP_CHAIRS				20

// draw_type: how DrawMap draws a model
#define DRAW_NONE				0		// night only model by day
#define DRAW_NORMAL				1
#define DRAW_WATER				2
#define DRAW_SHADOW				3
#define DRAW_ALPHA				4
#define DRAW_SUBMERGED			5
#define DRAW_TRANS				6
#define DRAW_NEON				7

#define NEON_PALETTE			19

typedef struct D3D_TEXTURE
{
	int tset;
	int *palettes;
	int num_palettes;
} D3D_TEXTURE;

typedef struct OUT_CELL_FILE_HEADER
{
	int cells_across;
	int cells_down;
	int cell_size;
	int num_regions;
	int region_size;
	int num_cell_objects;
	int num_cell_data;
	float ambient_light_level;
	VECTOR light_source;
} OUT_CELL_FILE_HEADER;

typedef struct OUT_CELL_OBJECT
{
	unsigned short computed;
	unsigned short type;
	VECTOR pos;
	int yang;
} OUT_CELL_OBJECT;

typedef struct PACKED_CELL_OBJECT
{
	unsigned short type;		// 0x8000: ghost
	VECTOR pos;
	short yang;
} PACKED_CELL_OBJECT;

typedef struct PACKED_CELL_DATA
{
	unsigned short num;
	unsigned short next;
} PACKED_CELL_DATA;

typedef struct OUT_MOVEABLE_OBJECT
{
	VECTOR pos;
	SVECTOR ang;
	VECTOR linear_velocity;
	SVECTOR angular_velocity;
	unsigned short type;
	unsigned short flags;
} OUT_MOVEABLE_OBJECT;

typedef struct AREA_LOAD_INFO
{
	int xoffset;
	int zoffset;
} AREA_LOAD_INFO;

//------------------------------------------------------------------------------------------
// initialised data (0x564fb4)

int cells_across = 1000;
int cells_down = 1000;
int cell_size = 2;

char *level_files[7] =
{
	"MIAMI_01.LEV",
	"FRISCO_01.LEV",
	"LA_01.LEV",
	"NY_01.LEV",
	"IVIEW.LEV",
	"TRAIN.LEV",
	"NC_01.LEV",
};

char *level_cos_files[7] =
{
	"LEVELS\\MIAMI_01.COS",
	"LEVELS\\FRISCO_01.COS",
	"LEVELS\\LA_01.COS",
	"LEVELS\\NY_01.COS",
	"LEVELS\\IVIEW.COS",
	"LEVELS\\TRAIN.COS",
	"LEVELS\\NC_01.COS",
};

CELL_OBJECT *ground_debris[16];
CELL_OBJECT **ground_debris_ptr = ground_debris;

// the texture set of each level that holds the sky (kept in RAM for InitHorizon)
int SkyTextureSlots[7] = { 28, 25, 29, 121, 28, 28, 25 };

int gAltSkyPalettes[7][5] =
{
	{ 2, 20, 21, 20, 22 },
	{ 2, 20, 21, 20, 22 },
	{ 2, 2, 2, 2, 2 },
	{ 2, 20, 21, 20, 22 },
	{ 2, 2, 2, 2, 2 },
	{ 2, 2, 2, 2, 2 },
	{ 2, 2, 2, 2, 2 },
};

// [level * 10 + variant * 2 + half]: variant 0 day, 1 night, 2 rain, 3 rain at night, 4 dawn / dusk;
// the first half of each level is the one in the level's textures, the others are copied over it
char *SkyTextures[7 * 10] =
{
	"PCSKY", NULL, "MIANIGHT", NULL, "MIAMRAIN", NULL, "MIANIGHT", NULL, "MIAMDAWN", NULL,
	"SFRANSKY", "SFRANSK+", "NIGHTSKY", "NIGHTSK+", "SF_RAIN", "SF_RAIN+", "NIGHTSKY", "NIGHTSK+", "SF_DAWN", "SF_DAWN+",
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	"NY_SKY", NULL, "NY_NIGHT", NULL, "NY_RAIN", NULL, "NY_NIGHT", NULL, "NY_DUSK", NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
};

//------------------------------------------------------------------------------------------
// uninitialised data
// NOTE: bss globals in the linker's common area (0xbb1184 ..., 0xd16ba0 - 0xd81488, mixed with
// motion capture globals); defined here provisionally, their main users are in map.c

TEX_INFO *tinfo_block;
int TSetsToLoad;
int num_textures;
char *model_names;
int gAlleyCount;
int gCurrentRamBytesUsed;
char *PackedPVSRegions;
D3D_TEXTURE *d3d_textures_mem;
int num_d3d_textures;

int gSky16Bit;

CAMERA camera;
int num_cell_data;
MOVEABLE_OBJECT *moveable_object_mem_originals;
CELL_OBJECT *shadow_objects[MAX_DRAWN_OBJECTS];
char draw_type[1156];
int region_size;
char *modelmem;
char ParentTextureMem[TSET_SIZE_16BIT];
float gAmbientLightDay;
int regions_down;
int cells_across_halved;
short view_arc[256][64];
CELL_DATA *cells;
int alpha_count;
int units_across_halved;
int units_down_halved;
CELL_OBJECT *water_objects[MAX_DRAWN_OBJECTS];
static char ChildTextureMem[TSET_SIZE_8BIT];
char gLoadAllRegionTSets;
CELL_OBJECT *alpha_objects[MAX_DRAWN_OBJECTS];
int horizon_angle;
short *arc_view;
int water_count;
int num_models_in_pack;
int cells_down_halved;
char CurrentPVS[10004];
int view_dist;
BOX model_bounding_box[MAX_MODELS];
int num_regions;
int shadow_count;
MODEL *model_pointers[1152];		// NOTE: IDA types it [1632]; its size is 1152 entries
CAMERA player;
CELL_OBJECT *neon_objects[MAX_NEON_OBJECTS];
int num_straddlers;
VECTOR4 gLightSourceDay;
int num_moveable_objects;
int pvs_square_sq;
CELL_OBJECT *cell_objects;
MOVEABLE_OBJECT *moveable_object_mem;
int neon_count;
int regions_unpacked[4];
char *packed_region_mem;
VECTOR4 gLightSourceNight;
int regions_across;
int pvs_square;
CELL_OBJECT *trans_objects[MAX_DRAWN_OBJECTS];
char *gRamTextureRawDataDump;
float gAmbientLightNight;
int *cell_ptrs;
int num_cell_objects;
char *packed_region_sizes[1024];
int gTreeCount;
int trans_count;

void ProcessMapLump(FILE *lump);
void ProcessModelLump(FILE *f);
void ProcessTextureLump(FILE *handle, int lump_size);
void LoadTexturePalettes(FILE *f);
void AllocateTexture(int TSet, char *TSetMem);
void InitCellData(void);
void InitModelMem(void);
void GetPVSRegionCell(int region, int cell, char *output);
void SendFullCell(char *output);
void UnpackRegion(int region_to_unpack, int target_barrel_region);
void CheckUnpackNewRegions(void);
void ControlMap(int update_cells);
void DrawCellObject(CELL_OBJECT *cop);
void DrawCellObjectCheckIfRoad(CELL_OBJECT *cop);

// [0x4ed4e0] [D]
// FRUSTRUM.DAT: the cells in view for each of 256 camera angles (see DrawMap)
int LoadViewArc(void)
{
	FILE *f;
	int i;

	f = fopen("FRUSTRUM.DAT", "rb");

	if (!f)
		return -1;

	DriverFRead(&i, 4, 1, f);

	for (i = 0; i < 256; i++)
		DriverFRead(view_arc[i], 1, 128, f);

	fclose(f);

	return 0;
}

// [0x4ed569] [D]
void InitModelMem(void)
{
	modelmem = (char *)DebugMalloc(0x400000, "D:\\Driver\\game6.4g\\map.c", 247);
}

// [0x4ed58a] [D]
void KillModelMem(void)
{
	if (modelmem)
	{
		DebugFree(modelmem);
		modelmem = NULL;
	}
}

// [0x4ed5b0] [D]
void ProcessModelLump(FILE *f)
{
	BOX box;
	int size;
	int i;
	MODEL *in;
	MODEL *model;
	MODEL *out;

	in = (MODEL *)DebugMalloc(0x20000, "D:\\Driver\\game6.4g\\map.c", 281);
	InitModelMem();
	out = (MODEL *)modelmem;

	if (DriverFRead(&num_models_in_pack, 4, 1, f))
	{
		for (i = 0; i < num_models_in_pack; i++)
		{
			DriverFRead(&size, 4, 1, f);
			DriverFRead(in, size, 1, f);

			size = ConvertModelToInternalFormat(in, out);

			model_pointers[i] = out;
			out = (MODEL *)((char *)out + size);
		}

		// the models first, then their instances
		for (i = 0; i < num_models_in_pack; i++)
		{
			model = model_pointers[i];

			if (model->instance_number == -1)
			{
				RelocateModel(model);
				CalcBoundingSphere(model, 0);
				CalcBoundingSphere(model, 1);
			}
		}

		for (i = 0; i < num_models_in_pack; i++)
		{
			model = model_pointers[i];

			if (model->instance_number != -1)
			{
				RelocateModel(model);
				CalcBoundingSphere(model, 0);
				CalcBoundingSphere(model, 1);
			}
		}

		for (i = 0; i < num_models_in_pack; i++)
		{
			model_bounding_box[i] = *CalcBoundingBox(&box, i);

			SetAxisBits(model_pointers[i]);
			CalcDuplicateNormals(model_pointers[i]);

			model = model_pointers[i];

			if (model->shape_flags & SHAPE_FLAG_SHADOW)
				draw_type[i] = DRAW_SHADOW;
			else if (model->shape_flags & SHAPE_FLAG_TRANS)
				draw_type[i] = DRAW_TRANS;
			else if (model->shape_flags & SHAPE_FLAG_ALPHA)
				draw_type[i] = DRAW_ALPHA;
			else if (model->shape_flags & SHAPE_FLAG_WATER)
				draw_type[i] = DRAW_WATER;
			else if (model->shape_flags & SHAPE_FLAG_SUBMERGED)
				draw_type[i] = DRAW_SUBMERGED;
			else if (model->flags2 & MODEL_FLAG_NEON)
				draw_type[i] = DRAW_NEON;
			else
				draw_type[i] = !(model->flags2 & MODEL_FLAG_NIGHT_ONLY) || gNight;
		}
	}

	DebugFree(in);
}

// [0x4ed8ef] [D]
void ProcessRegionTexturesLump(FILE *f)
{
	int i;

	for (i = 0; i < 1024; i++)
		DriverFRead(gUsedTSets[i], 70, 1, f);
}

// [0x4ed931] [D]
void ProcessMoveableObjectsLump(FILE *lump_file, unsigned int lump_size)
{
	MOVEABLE_OBJECT *mo;
	int i;
	OUT_MOVEABLE_OBJECT omo;
	size_t size;

	num_moveable_objects = lump_size / sizeof(OUT_MOVEABLE_OBJECT);

	size = num_moveable_objects * sizeof(MOVEABLE_OBJECT);
	moveable_object_mem = (MOVEABLE_OBJECT *)DebugMalloc(size, "D:\\Driver\\game6.4g\\map.c", 408);
	moveable_object_mem_originals = (MOVEABLE_OBJECT *)DebugMalloc(size, "D:\\Driver\\game6.4g\\map.c", 409);

	num_cars = 1;

	mo = moveable_object_mem_originals;

	for (i = 0; i < num_moveable_objects; i++)
	{
		DriverFRead(&omo, sizeof(OUT_MOVEABLE_OBJECT), 1, lump_file);

		mo->pos.vx = omo.pos.vx;
		mo->pos.vy = omo.pos.vy;
		mo->pos.vz = omo.pos.vz;
		mo->pos.vw = 1.0f;

		mo->ang.vx = omo.ang.vx;
		mo->ang.vy = omo.ang.vy;
		mo->ang.vz = omo.ang.vz;

		mo->linear_velocity.vx = omo.linear_velocity.vx;
		mo->linear_velocity.vy = omo.linear_velocity.vy;
		mo->linear_velocity.vz = omo.linear_velocity.vz;
		mo->linear_velocity.vw = 1.0f;

		mo->angular_velocity.vx = omo.angular_velocity.vx;
		mo->angular_velocity.vy = omo.angular_velocity.vy;
		mo->angular_velocity.vz = omo.angular_velocity.vz;

		mo->type = omo.type;
		mo->flags = omo.flags;

		mo++;
	}
}

// [0x4eda95] [D]
// Texture sets: a 16 bit page, or an 8 bit parent and child pair (two slots). Permanent pages go
// to the card; pages that change at run time (the sky, the interview paper, the player car and
// its damage) are also kept in RAM (gRamTextureRawDataDump).
void ProcessTextureLump(FILE *handle, int lump_size)
{
	short ParentFlags;
	short ChildData;
	int sky_slot;
	int TexSetNo;
	short ParentData;
	long StartOfTextures;
	size_t SizeRamTSets;
	int num_tsets;
	int Parent;
	int paper_slot;
	short ChildFlags;
	int Child;
	int BitDepth8;

	// If there are some palettes then load them
	LoadTexturePalettes(handle);

	// Get num of tsets in file
	DriverFRead(&num_tsets, 4, 1, handle);

	// SETUP
	InitTextureSpace();
	gCurrentRamBytesUsed = 0;

	sky_slot = SkyTextureSlots[GameLevel];

	if (GameLevel == 4)
		paper_slot = 17;
	else
		paper_slot = -1;

	// ===============MALLOC RAM DUMP=======================
	// Get size of all ram tsets (Ie. Swappable ones or car texture for chosen car)
	StartOfTextures = ftell(handle);
	SizeRamTSets = 0;

	TexSetNo = 0;
	while (TexSetNo < num_tsets)
	{
		fread(&ParentFlags, 2, 1, handle);
		fread(&ParentData, 2, 1, handle);

		Parent = TexSetNo;

		if (ParentFlags & TSET_8BIT)
		{
			fread(ParentTextureMem, TSET_SIZE_8BIT, 1, handle);

			fread(&ChildFlags, 2, 1, handle);
			fread(&ChildData, 2, 1, handle);

			fseek(handle, TSET_SIZE_8BIT, SEEK_CUR);

			BitDepth8 = 1;
		}
		else
		{
			fseek(handle, TSET_SIZE_16BIT, SEEK_CUR);
			BitDepth8 = 0;
		}

		if (ParentFlags & TSET_PERMANENT)
		{
			if (TexSetNo == sky_slot || TexSetNo == paper_slot)
			{
				if (BitDepth8)
					SizeRamTSets += TSET_SIZE_8BIT;
				else
					SizeRamTSets += TSET_SIZE_16BIT;
			}
		}
		else if ((ParentFlags & TSET_CAR) && ParentData == gPlayerCarModelType)
		{
			SizeRamTSets += TSET_SIZE_16BIT;
		}
		else if ((ParentFlags & TSET_CAR_DAMAGE) && ParentData == gPlayerCarModelType || (ParentFlags & TSET_DAMAGE))
		{
			SizeRamTSets += TSET_SIZE_16BIT;
		}

		// next texture
		if (BitDepth8)
			TexSetNo += 2;
		else
			TexSetNo++;
	}

	if (SizeRamTSets)
	{
		gRamTextureRawDataDump = (char *)DebugMalloc(SizeRamTSets, "D:\\Driver\\game6.4g\\map.c", 549);

		if (!gRamTextureRawDataDump)
			MessageBoxA(gScreen.handle, "Could not malloc space for RAM texture pages", "ERROR", MB_OK);
	}

	// Set file pointer to first tset again  (skip num_tset + 2 palettes)
	fseek(handle, StartOfTextures, SEEK_SET);

	// Loop round tsets
	TexSetNo = 0;
	while (TexSetNo < num_tsets)
	{
		// Read data for this tpage
		DriverFRead(&ParentFlags, 2, 1, handle);
		DriverFRead(&ParentData, 2, 1, handle);
		Parent = TexSetNo;

		if (ParentFlags & TSET_8BIT)
		{
			// --- 8 BIT ---
			// Parent
			DriverFRead(ParentTextureMem, TSET_SIZE_8BIT, 1, handle);

			// Child
			DriverFRead(&ChildFlags, 2, 1, handle);
			DriverFRead(&ChildData, 2, 1, handle);
			DriverFRead(ChildTextureMem, TSET_SIZE_8BIT, 1, handle);
			BitDepth8 = 1;
			Child = Parent + 1;
		}
		else
		{
			// --- 16 BIT ---
			// Read parent only
			DriverFRead(ParentTextureMem, TSET_SIZE_16BIT, 1, handle);
			BitDepth8 = 0;
		}

		// What type of tset is it?
		if (ParentFlags & TSET_PERMANENT)
		{
			// -------------PERMENANT TEXTURE PAGE ----------------
			// [Add to HW]
			if (BitDepth8)
			{
				// --- 8 BIT ---
				AllocateTexture(Parent, ParentTextureMem);
				AllocateTexture(Child, ChildTextureMem);

				TextureSlots[Parent].flags = 6;
				TextureSlots[Child].flags = 14;

				AddTextureSet8(Parent, ParentTextureMem, ChildTextureMem);

				if (Parent == sky_slot || Parent == paper_slot)
				{
					TextureSlots[Parent].data = gRamTextureRawDataDump + gCurrentRamBytesUsed;
					gCurrentRamBytesUsed += TSET_SIZE_8BIT;

					CopyMem(TextureSlots[Parent].data, ParentTextureMem, TSET_SIZE_8BIT);

					if (Parent == sky_slot)
						gSky16Bit = 0;
				}

				if (Child == sky_slot || Child == paper_slot)
				{
					TextureSlots[Child].data = gRamTextureRawDataDump + gCurrentRamBytesUsed;
					gCurrentRamBytesUsed += TSET_SIZE_8BIT;

					CopyMem(TextureSlots[Child].data, ChildTextureMem, TSET_SIZE_8BIT);

					if (Child == sky_slot)
						gSky16Bit = 0;
				}
			}
			else
			{
				// --- 16 BIT ---
				// Parent
				TextureSlots[Parent].flags = 2;

				AddTextureSet16(Parent, ParentTextureMem);

				if (Parent == sky_slot || Parent == paper_slot)
				{
					TextureSlots[Parent].data = gRamTextureRawDataDump + gCurrentRamBytesUsed;
					gCurrentRamBytesUsed += TSET_SIZE_16BIT;

					CopyMem(TextureSlots[Parent].data, ParentTextureMem, TSET_SIZE_16BIT);

					if (Parent == sky_slot)
						gSky16Bit = 1;
				}
			}
		}
		else if (ParentFlags & TSET_CAR)
		{
			if (ParentData == gPlayerCarModelType)
			{
				// -------------------PLAYER CAR PAGE----------------
				if (BitDepth8)
				{
					// --- 8 BIT ---
					// -[Add to HW]-
					AllocateTexture(Parent, ParentTextureMem);
					AllocateTexture(Child, ChildTextureMem);

					TextureSlots[Parent].flags = 6;
					TextureSlots[Child].flags = 14;

					AddTextureSet8(Parent, ParentTextureMem, ChildTextureMem);

					// -[Add to RAM]-
					// Parent
					TextureSlots[Parent].data = gRamTextureRawDataDump + gCurrentRamBytesUsed;
					gCurrentRamBytesUsed += TSET_SIZE_8BIT;
					CopyMem(TextureSlots[Parent].data, ParentTextureMem, TSET_SIZE_8BIT);

					// Child
					TextureSlots[Child].data = gRamTextureRawDataDump + gCurrentRamBytesUsed;
					gCurrentRamBytesUsed += TSET_SIZE_8BIT;
					CopyMem(TextureSlots[Child].data, ChildTextureMem, TSET_SIZE_8BIT);
				}
				else
				{
					// --- 16 BIT ---
					// -[Add to HW]-
					// Parent
					TextureSlots[Parent].flags = 2;

					AddTextureSet16(Parent, ParentTextureMem);

					// -[Add to RAM]-
					// Parent
					TextureSlots[Parent].data = gRamTextureRawDataDump + gCurrentRamBytesUsed;
					gCurrentRamBytesUsed += TSET_SIZE_16BIT;
					CopyMem(TextureSlots[Parent].data, ParentTextureMem, TSET_SIZE_16BIT);
				}
			}
		}
		else if ((ParentFlags & TSET_CAR_DAMAGE) && ParentData == gPlayerCarModelType || (ParentFlags & TSET_DAMAGE))
		{
			// -------------------PLAYER CAR DAMAGE PAGE----------------
			if (BitDepth8)
			{
				// --- 8 BIT ---
				// -[Add to HW]-
				AllocateTexture(Parent, ParentTextureMem);
				AllocateTexture(Child, ChildTextureMem);

				// -[Add to RAM]-
				// Parent
				TextureSlots[Parent].flags = 4;
				TextureSlots[Parent].data = gRamTextureRawDataDump + gCurrentRamBytesUsed;
				gCurrentRamBytesUsed += TSET_SIZE_8BIT;
				CopyMem(TextureSlots[Parent].data, ParentTextureMem, TSET_SIZE_8BIT);

				// Child
				TextureSlots[Child].flags = 12;
				TextureSlots[Child].data = gRamTextureRawDataDump + gCurrentRamBytesUsed;
				gCurrentRamBytesUsed += TSET_SIZE_8BIT;
				CopyMem(TextureSlots[Child].data, ChildTextureMem, TSET_SIZE_8BIT);
			}
			else
			{
				// --- 16 BIT ---
				// Parent
				TextureSlots[Parent].flags = 0;
				TextureSlots[Parent].data = gRamTextureRawDataDump + gCurrentRamBytesUsed;
				gCurrentRamBytesUsed += TSET_SIZE_16BIT;
				CopyMem(TextureSlots[Parent].data, ParentTextureMem, TSET_SIZE_16BIT);
			}
		}

		// Next tset
		if (BitDepth8)
			TexSetNo += 2;
		else
			TexSetNo++;
	}
}

// [0x4ee274] [D]
void LoadTexturePalettes(FILE *f)
{
	char text[256];
	int PaletteLoop;
	short NumPalettes;
	short SlotNo;

	// Reset PalettesPresent
	PaletteLoop = 50;

	while (PaletteLoop--)
		gPalettePresent[PaletteLoop] = 0;

	// Get num of palettes in file
	DriverFRead(&NumPalettes, 2, 1, f);

	// If there are some palettes then load them
	PaletteLoop = NumPalettes;

	while (PaletteLoop--)
	{
		// Read slot no
		DriverFRead(&SlotNo, 2, 1, f);

		// Invalid slot no check
		if (SlotNo < 0 || SlotNo >= 50)
		{
			// Error
			sprintf(text, "Illegal palette slot load access (%d).", SlotNo);
			LogQuick(text);

			sprintf(text, "Tried to store palette to slot %d", SlotNo);
			MessageBoxA(gScreen.handle, text, "Error", MB_OK);

			// Skip palette
			fseek(f, 1024, SEEK_CUR);
		}
		else
		{
			// Read palette
			DriverFRead(gPalette[SlotNo], 1024, 1, f);

			// Mark this as present
			gPalettePresent[SlotNo] = 1;
		}
	}
}

// [0x4ee3a0] [D]
void ProcessTextureInfoLump(FILE *f, int size)
{
	int block_size;

	DriverFRead(&num_textures, 4, 1, f);

	block_size = size - 4;

	tinfo_block = (TEX_INFO *)DebugMalloc(block_size, "D:\\Driver\\game6.4g\\map.c", 812);

	if (tinfo_block)
		DriverFRead(tinfo_block, block_size, 1, f);
	else
		num_textures = 0;
}

// [0x4ee40c] [D]
// The road info is read and thrown away.
void ProcessRoadInfoLump(FILE *f, int lump_size)
{
	void *data;

	data = DebugMalloc(lump_size, "D:\\Driver\\game6.4g\\map.c", 839);
	DriverFRead(data, 1, lump_size, f);
	DebugFree(data);
}

// [0x4ee456] [D]
void ProcessRoadMapLump(FILE *f, int lump_size)
{
	DriverFRead(&RoadMapWidth, 4, 1, f);
	DriverFRead(&RoadMapHeight, 4, 1, f);

	RoadMapUnitWidth = RoadMapWidth * 1500;
	RoadMapUnitHeight = RoadMapHeight * 1500;
	RoadMapUnitXMid = RoadMapWidth * 1500 / 2;
	RoadMapUnitZMid = RoadMapHeight * 1500 / 2;
	RoadMapXMid = RoadMapWidth / 2;
	RoadMapZMid = RoadMapHeight / 2;

	MapData = (int *)DebugMalloc(lump_size - 8, "D:\\Driver\\game6.4g\\map.c", 863);
	DriverFRead(MapData, 1, lump_size - 8, f);
}

// [0x4ee51d] [D]
void ProcessMapInfoLump(FILE *lump_file, int lump_size)
{
	DriverFRead(&RoadMapWidth, 4, 1, lump_file);
	DriverFRead(&RoadMapHeight, 4, 1, lump_file);

	// BUG: no out of memory check!
	MapInfo = (int)DebugMalloc(lump_size - 8, "D:\\Driver\\game6.4g\\map.c", 881);

	DriverFRead((void *)MapInfo, 1, lump_size - 8, lump_file);
}

// [0x4ee586] [D]
void ProcessRoadsLump(FILE *lump_file, int lump_size)
{
	DriverFRead(&NumRoads, 1, 4, lump_file);
	DriverFRead(Roads, NumRoads, sizeof(ROAD), lump_file);
}

// [0x4ee5ba] [D]
void ProcessJunctionsLump(FILE *lump_file, int lump_size)
{
	DriverFRead(&NumJunctions, 1, 4, lump_file);
	DriverFRead(Junctions, NumJunctions, sizeof(JUNCTION), lump_file);
}

// [0x4ee5ee] [D]
void ProcessSurfaceLump(FILE *lump_file, int lump_size)
{
	SurfaceData = (int)DebugMalloc(lump_size, "D:\\Driver\\game6.4g\\map.c", 927);

	if (SurfaceData)
	{
		DriverFRead((void *)SurfaceData, lump_size, 1, lump_file);

		OrigSurfaceData = SurfaceData;

		ROADS_InitSurfaceLookup();
	}
}

// [0x4ee642] [D]
void ProcessRoadBoundsLump(FILE *lump_file, int lump_size)
{
	DriverFRead(&NumRoads, 1, 4, lump_file);
	DriverFRead(RoadBounds, NumRoads, sizeof(ROADBOUNDS), lump_file);
}

// [0x4ee676] [D]
void ProcessJuncBoundsLump(FILE *lump_file, int lump_size)
{
	DriverFRead(&NumJunctions, 1, 4, lump_file);
	DriverFRead(JunctionPositions, NumJunctions, sizeof(XYPAIR), lump_file);
}

// [0x4ee6aa] [D]
void ProcessEventModelLump(FILE *lump_file, int lump_size)
{
	MODEL *out;
	int size;
	int i;
	MODEL *in;

	in = (MODEL *)DebugMalloc(0x80000, "D:\\Driver\\game6.4g\\map.c", 980);
	out = (MODEL *)DebugMalloc(0x80000, "D:\\Driver\\game6.4g\\map.c", 981);

	for (i = 0; i < num_event_models; i++)
	{
		if (event_model_list[i])
		{
			DebugFree(event_model_list[i]);
			event_model_list[i] = NULL;
		}
	}

	DriverFRead(&num_event_models, 4, 1, lump_file);

	for (i = 0; i < num_event_models; i++)
	{
		DriverFRead(&size, 4, 1, lump_file);
		DriverFRead(in, size, 1, lump_file);

		size = ConvertModelToInternalFormat(in, out);

		event_model_list[i] = (MODEL *)DebugMalloc(size, "D:\\Driver\\game6.4g\\map.c", 1005);

		if (!event_model_list[i])
		{
			num_event_models = 0;
			MessageBoxA(gScreen.handle, "Not enogh memory for event models!", "Memory Error!", MB_ICONEXCLAMATION);
			DebugFree(out);
			DebugFree(in);
			return;
		}

		CopyMem(event_model_list[i], out, size);
		RelocateModel(event_model_list[i]);
		CalcBoundingSphere(event_model_list[i], 0);
		CalcBoundingSphere(event_model_list[i], 1);
		SetAxisBits(event_model_list[i]);
		CalcDuplicateNormals(event_model_list[i]);
	}

	DebugFree(out);
	DebugFree(in);
}

// [0x4ee8cc] [D]
void ProcessModelNamesLump(FILE *lump_file, int lump_size)
{
	char *names;

	names = (char *)DebugMalloc(lump_size, "D:\\Driver\\game6.4g\\map.c", 1045);

	if (names)
	{
		DriverFRead(names, lump_size, 1, lump_file);
		model_names = names;
	}
}

// [0x4ee914] [D]
void ProcessPVSRegionsLump(FILE *lump_file, int lump_size)
{
	PackedPVSRegions = (char *)DebugMalloc(lump_size, "D:\\Driver\\game6.4g\\map.c", 1067);

	if (PackedPVSRegions)
		DriverFRead(PackedPVSRegions, lump_size, 1, lump_file);
}

// [0x4ee957] [D]
// model_names: one zero terminated name per model
int FindModelWithName(char *name)
{
	char *str;
	int i;

	if (!model_names)
		return -1;

	str = model_names;

	for (i = 0; i < num_models_in_pack; i++)
	{
		if (!_strcmpi(str, name))
			return i;

		while (*str++)
			;
	}

	return -1;
}

// [0x4ee9c8] [D]
MODEL *FindModelPtrWithName(char *name)
{
	char *str;
	int i;

	if (!model_names)
		return NULL;

	str = model_names;

	for (i = 0; i < num_models_in_pack; i++)
	{
		if (!strcmp(str, name))
			return model_pointers[i];

		while (*str++)
			;
	}

	return NULL;
}

// [0x4eea3e] [D]
TEX_INFO *GetTextureDetails(char *tex_name)
{
	TEX_INFO *info;
	int idx;
	int name_len;
	int valid;
	int loop;

	if (!num_textures)
		return NULL;

	name_len = strlen(tex_name);

	if (!name_len)
		return NULL;

	if (name_len > TEX_NAME_LEN)
		name_len = TEX_NAME_LEN;

	loop = num_textures;
	info = tinfo_block;

	if (!info)
		return NULL;

	while (loop--)
	{
		valid = 1;

		for (idx = 0; idx < name_len; idx++)
		{
			if (tex_name[idx] != info->name[idx])
			{
				valid = 0;
				break;
			}
		}

		// names are 8 chars, zero padded
		if (valid && (idx == TEX_NAME_LEN || info->name[idx] == 0))
			return info;

		info++;
	}

	return NULL;
}

// [0x4eeb28] [D]
FILE *GetLumpFile(void)
{
	char name[256];

	if (gDoMotelRoom)
		return fopen("Levels\\flat.lev", "rb");

	if (GameLevel == 4 || gDoInterview)
		return fopen("Levels\\iview.lev", "rb");

	if (GameLevel == 5 || gRecordTraining)
		return fopen("Levels\\train.lev", "rb");

	if (mission.map_name)
		sprintf(name, "Levels\\%s", mission.map_name);
	else
		sprintf(name, "Levels\\Miami_01.lev");

	return fopen(name, "rb");
}

// [0x4eebfe] [D]
int ReadLevelFile(void)
{
	int num_lumps;
	int lumpType;
	int lumpSize;
	int i;
	FILE *file;

	SwapPalette(-1);

	file = GetLumpFile();

	model_names = NULL;

	if (!file)
		return 0;

	DriverFRead(&num_lumps, 4, 1, file);

	for (i = 0; i < num_lumps; i++)
	{
		DriverFRead(&lumpType, 4, 1, file);
		DriverFRead(&lumpSize, 4, 1, file);

		switch (lumpType)
		{
			case LUMP_TEXTURES:
				ProcessTextureLump(file, lumpSize);
				break;
			case LUMP_MODELS:
				ProcessModelLump(file);
				break;
			case LUMP_MAP:
				ProcessMapLump(file);
				break;
			case LUMP_MOVEABLE_OBJECTS:
				ProcessMoveableObjectsLump(file, lumpSize);
				break;
			case LUMP_TEXTURE_INFO:
				ProcessTextureInfoLump(file, lumpSize);
				break;
			case LUMP_ROAD_INFO:
				ProcessRoadInfoLump(file, lumpSize);
				break;
			case LUMP_ROAD_MAP:
				ProcessRoadMapLump(file, lumpSize);
				break;
			case LUMP_ROADS:
				ProcessRoadsLump(file, lumpSize);
				break;
			case LUMP_JUNCTIONS:
				ProcessJunctionsLump(file, lumpSize);
				break;
			case LUMP_SURFACE:
				ProcessSurfaceLump(file, lumpSize);
				break;
			case LUMP_MAP_INFO:
				ProcessMapInfoLump(file, lumpSize);
				break;
			case LUMP_MODEL_NAMES:
				ProcessModelNamesLump(file, lumpSize);
				break;
			case LUMP_EVENT_MODELS:
				ProcessEventModelLump(file, lumpSize);
				break;
			case LUMP_PVS_REGIONS:
				ProcessPVSRegionsLump(file, lumpSize);
				break;
			case LUMP_REGION_TEXTURES:
				ProcessRegionTexturesLump(file);
				break;
			case LUMP_ROAD_BOUNDS:
				ProcessRoadBoundsLump(file, lumpSize);
				break;
			case LUMP_JUNC_BOUNDS:
				ProcessJuncBoundsLump(file, lumpSize);
				break;
			case LUMP_CAMERA_PATHS:
				// camera paths - unused
				// fly-by data used instead
				fseek(file, lumpSize, SEEK_CUR);
				break;
			case LUMP_LAMPS:
				ProcessLampsLump(file);
				break;
			case LUMP_CHAIRS:
				ProcessChairLump(file, lumpSize);
				break;
		}
	}

	fclose(file);

	SwapPalette(1);

	InitModelNames();
	InitFontDrawing();
	InitOverlays();

	LoadLevelMap();
	InitWater();

	return 1;
}

// [0x4eeec8] [D]
void InitCellData(void)
{
	cell_ptrs = (int *)DebugMalloc(region_size * region_size * 16, "D:\\Driver\\game6.4g\\map.c", 1378);
	cell_objects = (CELL_OBJECT *)DebugMalloc((num_straddlers + MAX_STRADDLERS * 4) * sizeof(CELL_OBJECT), "D:\\Driver\\game6.4g\\map.c", 1379);
	cells = (CELL_DATA *)DebugMalloc(MAX_REGION_DATA_SLOTS * 4 * sizeof(CELL_DATA), "D:\\Driver\\game6.4g\\map.c", 1380);
}

// [0x4eef3a] [D]
void KillCellData(void)
{
	if (cell_ptrs)
	{
		DebugFree(cell_ptrs);
		cell_ptrs = NULL;
	}

	if (cell_objects)
	{
		DebugFree(cell_objects);
		cell_objects = NULL;
	}

	if (cells)
	{
		DebugFree(cells);
		cells = NULL;
	}
}

// [0x4eefa4] [D]
// NOTE: no name in the database; the cell number of a world position
int sub_4EEFA4(VECTOR4 *pos)
{
	float x;
	float z;

	x = ((float)units_across_halved + pos->vx) * cell_recip;
	z = ((float)units_down_halved + pos->vz) * cell_recip;

	return cells_across * (int)z + (int)x;
}

// [0x4ef008] [D]
int PositionVisible(VECTOR4 *pos, char *vis, int ccx, int ccz)
{
	float x;
	float z;
	int dx;
	int dz;

	x = ((float)units_across_halved + pos->vx) * cell_recip;
	z = ((float)units_down_halved + pos->vz) * cell_recip;

	dx = (int)x - ccx;
	dz = (int)z - ccz;

	return abs(dx) <= view_dist && abs(dz) <= view_dist && vis[pvs_square * (dz + 15) + 15 + dx];
}

// [0x4ef0dd] [D]
int newPositionVisible(int pos, char *pvs, int ccx, int ccz)
{
	int dx;
	int dz;

	dx = pos % cells_across - ccx;
	dz = pos / cells_across - ccz;

	return abs(dx) <= view_dist && abs(dz) <= view_dist && pvs[pvs_square * (dz + 15) + 15 + dx];
}

// [0x4ef175] [D]
void ProcessMapLump(FILE *lump)
{
	OUT_CELL_OBJECT oco;
	int i;
	CELL_OBJECT *cop;
	OUT_CELL_FILE_HEADER ci;
	int count;
	char *region_ptr;
	float scale;

	region_ptr = packed_region_mem;

	road_recip = 1.0f / 1500.0f;

	DriverFRead(&ci, sizeof(OUT_CELL_FILE_HEADER), 1, lump);

	cells_across = ci.cells_across;
	cells_down = ci.cells_down;
	cell_size = ci.cell_size;

	num_cell_objects = ci.num_cell_objects;
	num_cell_data = ci.num_cell_data;

	ambient_light_level = ci.ambient_light_level;

	num_regions = ci.num_regions;
	region_size = ci.region_size;

	view_dist = 45000 / ci.cell_size;

	pvs_square = 2 * view_dist + 1;
	pvs_square_sq = pvs_square * pvs_square;

	cells_across_halved = ci.cells_across / 2;
	cells_down_halved = ci.cells_down / 2;

	units_across_halved = ci.cell_size * cells_across_halved;
	units_down_halved = ci.cell_size * cells_down_halved;

	road_region_size = ci.cell_size / 1500 * ci.region_size;
	road_region_size_recip = 1.0f / (float)road_region_size;

	regions_across = ci.cells_across / ci.region_size;
	regions_down = ci.cells_down / ci.region_size;

	SetLightVector(ci.light_source.vx, ci.light_source.vy, ci.light_source.vz);

	gLightSourceDay = light_source;
	gAmbientLightDay = ambient_light_level;

	gLightSourceNight.vw = light_source.vw;
	scale = (float)(gNightBrightness * 2) / 45.0f + 0.2f;
	gLightSourceNight.vx = light_source.vx * scale;
	gLightSourceNight.vy = light_source.vy * scale;
	gLightSourceNight.vz = light_source.vz * scale;

	gAmbientLightNight = ambient_light_level * scale;

	if (gDoDawn)
	{
		light_source = gLightSourceNight;
		ambient_light_level = gAmbientLightNight;
		gDawnTimer = 0;
	}
	else if (gDoDusk)
	{
		light_source = gLightSourceDay;
		ambient_light_level = gAmbientLightDay;
		gDawnTimer = 0;
	}
	else if (gNight)
	{
		light_source = gLightSourceNight;
		ambient_light_level = gAmbientLightNight;
	}
	else
	{
		light_source = gLightSourceDay;
		ambient_light_level = gAmbientLightDay;

		if (gDoRain)
		{
			ambient_light_level *= 0.5f;
			light_source.vx *= 0.45f;
			light_source.vy *= 0.45f;
			light_source.vz *= 0.45f;
		}
	}

	ChangeLightLevel();

	// the objects that straddle regions, always present
	DriverFRead(&num_straddlers, 4, 1, lump);
	InitCellData();

	cop = cell_objects;

	for (i = 0; i < num_straddlers; i++)
	{
		DriverFRead(&oco, sizeof(OUT_CELL_OBJECT), 1, lump);

		cop->computed = 0;
		cop->type = oco.type;
		cop->pos.vx = oco.pos.vx;
		cop->pos.vy = oco.pos.vy;
		cop->pos.vz = oco.pos.vz;
		cop->pos.vw = 1.0f;
		cop->yang = oco.yang;

		cop++;
	}

	// the packed regions: cell data, cell pointers, cell objects, each with its count
	for (i = 0; i < num_regions; i++)
	{
		packed_region_sizes[i] = region_ptr;

		DriverFRead(&count, 4, 1, lump);

		*(int *)region_ptr = count;
		region_ptr += 4;

		if (count)
		{
			DriverFRead(region_ptr, sizeof(PACKED_CELL_DATA), count, lump);
			region_ptr += sizeof(PACKED_CELL_DATA) * count;
		}

		DriverFRead(&count, 4, 1, lump);

		*(int *)region_ptr = count;
		region_ptr += 4;

		DriverFRead(region_ptr, 1, count, lump);

		region_ptr += count;

		DriverFRead(&count, 4, 1, lump);

		*(int *)region_ptr = count;
		region_ptr += 4;

		if (count)
		{
			DriverFRead(region_ptr, sizeof(PACKED_CELL_OBJECT), count, lump);
			region_ptr += sizeof(PACKED_CELL_OBJECT) * count;
		}
	}
}

// [0x4ef6d1] [D]
void InitPackedRegionMem(void)
{
	packed_region_mem = (char *)DebugMalloc(0x300000, "D:\\Driver\\game6.4g\\map.c", 1745);
}

// [0x4ef6f2] [D]
void FreePackedRegionMem(void)
{
	if (packed_region_mem)
	{
		DebugFree(packed_region_mem);
		packed_region_mem = NULL;
	}
}

// [0x4ef718] [D]
void UnpackRegion(int region_to_unpack, int target_barrel_region)
{
	char *region_ptr;
	CELL_DATA *cdp;
	CELL_DATA *region_cell_data;
	int num;
	short bits;
	int region_ptr_slot;
	PACKED_CELL_DATA *pcd;
	int i;
	PACKED_CELL_OBJECT *pco;
	char *ptr;
	char length;
	CELL_OBJECT *cop;
	int bitpos;
	char *data;
	CELL_DATA *region_cells;
	int count;
	int size;

	// NOTE: <= num_regions as in the original
	if (region_to_unpack < 0 || region_to_unpack > num_regions)
		return;

	regions_unpacked[target_barrel_region] = region_to_unpack;
	region_ptr = packed_region_sizes[region_to_unpack];
	count = *(int *)region_ptr;

	if (count)
	{
		//
		// 1. packed cell data
		//
		region_cell_data = &cells[MAX_REGION_DATA_SLOTS * target_barrel_region];
		pcd = (PACKED_CELL_DATA *)(region_ptr + 4);

		region_cells = region_cell_data;

		for (i = 0; i < count; i++)
		{
			region_cells->num = pcd->num;

			if (pcd->next == 0xFFFF)
				region_cells->next = -1;
			else
				region_cells->next = pcd->next;

			region_cells++;
			pcd++;
		}

		cdp = region_cell_data;

		// resolve true slot
		for (i = 0; i < count; i++)
		{
			if (cdp->num >= num_straddlers)
				cdp->num += MAX_REGION_OBJECTS * target_barrel_region;

			if (cdp->next != -1)
				cdp->next += MAX_REGION_DATA_SLOTS * target_barrel_region;

			cdp++;
		}
	}

	//
	// 2. cell pointers: none, a full list, or a bit mask of the cells that have one
	//
	ptr = region_ptr + count * sizeof(PACKED_CELL_DATA) + 4;

	size = *(int *)ptr;
	ptr += 4;

	length = *ptr;
	data = ptr + 1;

	region_ptr_slot = region_size * region_size * target_barrel_region;

	ptr += size;

	if (length == 0)
	{
		for (i = 0; i < region_size * region_size; i++)
			cell_ptrs[region_ptr_slot + i] = -1;
	}
	else if (length == 1)
	{
		for (i = 0; i < region_size * region_size; i++)
		{
			num = *(unsigned short *)data;
			data += 2;

			if (num == 0xFFFF)
				num = -1;
			else
				num += MAX_REGION_DATA_SLOTS * target_barrel_region;

			cell_ptrs[region_ptr_slot + i] = num;
		}
	}
	else if (length == 2)
	{
		bitpos = 0x8000;
		bits = *(short *)data;
		data += 2;

		for (i = 0; i < region_size * region_size; i++)
		{
			if (bitpos & bits)
			{
				num = *(unsigned short *)data;
				data += 2;

				num += MAX_REGION_DATA_SLOTS * target_barrel_region;
			}
			else
			{
				num = -1;
			}

			cell_ptrs[region_ptr_slot + i] = num;
			bitpos >>= 1;

			if (!bitpos)
			{
				bitpos = 0x8000;
				bits = *(short *)data;
				data += 2;
			}
		}
	}

	//
	// 3. packed cell objects
	//
	count = *(int *)ptr;
	ptr += 4;

	if (count)
	{
		pco = (PACKED_CELL_OBJECT *)ptr;
		cop = &cell_objects[MAX_STRADDLERS * target_barrel_region + num_straddlers];

		for (i = 0; i < count; i++)
		{
			cop->computed = current_vertex_computed_value;
			cop->type = pco->type & 0x7FFF;
			cop->pos.vx = pco->pos.vx;
			cop->pos.vy = pco->pos.vy;
			cop->pos.vz = pco->pos.vz;
			cop->yang = pco->yang;
			cop->ghost = (pco->type & 0x8000) != 0;

			pco++;
			cop++;
		}
	}

	RoadMapRegions[target_barrel_region] = region_to_unpack;

	// the road map counts regions from the bottom
	region_to_unpack = region_to_unpack % regions_across
		+ regions_across * (regions_down - 1 - (region_to_unpack - region_to_unpack % regions_across) / regions_across);

	ROADS_UnpackRoadRegion(region_to_unpack, RoadMapDataRegions[target_barrel_region]);
	ROADS_UnpackRoadInfoRegion(region_to_unpack, RoadMapInfoRegions[target_barrel_region]);
}

// [0x4efbc2] [D]
// Unpacks the regions next to the player when it gets within 20 cells of a region edge.
void CheckUnpackNewRegions(void)
{
	int region_to_unpack;
	int num_regions_to_unpack;
	int z;
	int x;
	AREA_LOAD_INFO regions_to_unpack[3];
	int i;
	int topbottom_unpack;
	int leftright_unpack;

	leftright_unpack = 0;
	topbottom_unpack = 0;
	num_regions_to_unpack = 0;

	if (player.current_region_cell_x < 20)
	{
		if (player.current_region_x != 0)
		{
			regions_to_unpack[0].xoffset = -1;
			regions_to_unpack[0].zoffset = 0;
			num_regions_to_unpack = 1;
			leftright_unpack = 1;
		}
	}
	else if (player.current_region_cell_x > region_size - 20 && player.current_region_x < cells_across / region_size)
	{
		regions_to_unpack[0].xoffset = 1;
		regions_to_unpack[0].zoffset = 0;
		num_regions_to_unpack = 1;
		leftright_unpack = 2;
	}

	if (player.current_region_cell_z < 20)
	{
		if (player.current_region_z != 0)
		{
			regions_to_unpack[num_regions_to_unpack].xoffset = 0;
			regions_to_unpack[num_regions_to_unpack].zoffset = -1;
			num_regions_to_unpack++;
			topbottom_unpack = 1;
		}
	}
	else if (player.current_region_cell_z > region_size - 20 && player.current_region_z != 0)
	{
		regions_to_unpack[num_regions_to_unpack].xoffset = 0;
		regions_to_unpack[num_regions_to_unpack].zoffset = 1;
		num_regions_to_unpack++;
		topbottom_unpack = 2;
	}

	// and the corner region
	if (num_regions_to_unpack == 2)
	{
		if (leftright_unpack == 1)
			regions_to_unpack[2].xoffset = -1;
		else
			regions_to_unpack[2].xoffset = 1;

		if (topbottom_unpack == 1)
			regions_to_unpack[2].zoffset = -1;
		else
			regions_to_unpack[2].zoffset = 1;

		num_regions_to_unpack = 3;
	}

	for (i = 0; i < num_regions_to_unpack; i++)
	{
		x = regions_to_unpack[i].xoffset;
		z = regions_to_unpack[i].zoffset;

		region_to_unpack = player.current_region + x + cells_across / region_size * z;

		if (region_to_unpack != regions_unpacked[0] &&
			region_to_unpack != regions_unpacked[1] &&
			region_to_unpack != regions_unpacked[2] &&
			region_to_unpack != regions_unpacked[3])
		{
			UnpackRegion(region_to_unpack, ((x + player.current_region_x) & 1) + 2 * ((z + player.current_region_z) & 1));
		}
	}
}

// [0x4efe28] [D]
void ControlMap(int update_cells)
{
	float x;
	float z;

	if (update_cells)
	{
		CheckUnpackRegionTSets();
		return;
	}

	x = (camera_position.vx + (float)units_across_halved) * cell_recip;
	z = (camera_position.vz + (float)units_down_halved) * cell_recip;

	camera.current_cell_x = (int)x;
	camera.current_cell_z = (int)z;
	camera.current_cell = cells_across * camera.current_cell_z + camera.current_cell_x;
	camera.current_region_x = camera.current_cell_x / region_size;
	camera.current_region_z = camera.current_cell_z / region_size;
	camera.previous_region = camera.current_region;
	camera.current_region = cells_across / region_size * camera.current_region_z + camera.current_region_x;
	camera.barrel_region_x = camera.current_region_x & 1;
	camera.barrel_region_z = camera.current_region_z & 1;
	camera.current_barrel_region = camera.barrel_region_x + 2 * camera.barrel_region_z;
	camera.current_region_cell_x = camera.current_cell_x - region_size * camera.current_region_x;
	camera.current_region_cell_z = camera.current_cell_z - region_size * camera.current_region_z;

	if (gDoMotelRoom)
	{
		player = camera;
	}
	else
	{
		if (FlyBy)
		{
			x = (camera_position.vx + (float)units_across_halved) * cell_recip;
			z = (camera_position.vz + (float)units_down_halved) * cell_recip;
		}
		else if (!gMissionRecording && !gMissionReplaying || gDoTraining && gTrainingMode == 2)
		{
			x = (float)(units_across_halved + car_data[0].location.vx) * cell_recip;
			z = (float)(units_down_halved + car_data[0].location.vz) * cell_recip;
		}
		else
		{
			x = (float)(units_across_halved + car_data[65].location.vx) * cell_recip;
			z = (float)(units_down_halved + car_data[65].location.vz) * cell_recip;
		}

		player.current_cell_x = (int)x;
		player.current_cell_z = (int)z;
		player.current_cell = cells_across * player.current_cell_z + player.current_cell_x;
		player.current_region_x = player.current_cell_x / region_size;
		player.current_region_z = player.current_cell_z / region_size;
		player.previous_region = player.current_region;
		player.current_region = cells_across / region_size * player.current_region_z + player.current_region_x;
		player.barrel_region_x = player.current_region_x & 1;
		player.barrel_region_z = player.current_region_z & 1;
		player.current_barrel_region = player.barrel_region_x + 2 * player.barrel_region_z;
		player.current_region_cell_x = player.current_cell_x - region_size * player.current_region_x;
		player.current_region_cell_z = player.current_cell_z - region_size * player.current_region_z;
	}

	CheckUnpackNewRegions();
}

// [0x4f0185] [D]
void InitMap(void)
{
	ground_debris_ptr = ground_debris;

	gAlleyCount = 0;
	gTreeCount = 0;
	gCurrentRamBytesUsed = 0;
	gLoadAllRegionTSets = 1;
	TSetsToLoad = 0;

	player.previous_region = -1;
	camera.previous_region = -1;

	memset(ModelActive, 1, sizeof(ModelActive));

	ControlMap(0);
	ControlMap(1);

	regions_unpacked[3] = -1;
	regions_unpacked[2] = -1;
	regions_unpacked[1] = -1;
	regions_unpacked[0] = -1;

	UnpackRegion(player.current_region, player.current_barrel_region);
	CheckUnpackNewRegions();
}

// [0x4f0249] [D]
void ResetMap(void)
{
	player.previous_region = -1;
	camera.previous_region = -1;

	regions_unpacked[3] = -1;
	regions_unpacked[2] = -1;
	regions_unpacked[1] = -1;
	regions_unpacked[0] = -1;

	UnpackRegion(player.current_region, player.current_barrel_region);
	CheckUnpackNewRegions();
}

// [0x4f02ab] [D]
void DrawCellObject(CELL_OBJECT *cop)
{
	VECTOR4 pos;
	MODEL *model;
	int flags;
	int clip;

	model = model_pointers[cop->type];
	flags = model->shape_flags;

	clip = FrustrumCheckFar(&cop->pos, model->bounding_sphere);

	if (!clip)
		return;

	if (flags & SHAPE_FLAGS_ANIMOBJ)
	{
		DrawAnimatingObject(model, cop, clip);
		CurrentRotationMatrix = &inv_camera_matrix;
		return;
	}

	// every 16th tree and alley tile gets ground debris (leaves, litter)
	if ((flags & SHAPE_FLAG_SPRITE) && (model->flags2 & MODEL_FLAG_TREE))
	{
		if ((gTreeCount++ & 0xF) == 0 && ground_debris_ptr < &ground_debris[16])
			*ground_debris_ptr++ = cop;
	}
	else if ((flags & SHAPE_FLAG_TILE) && (model->flags2 & MODEL_FLAG_ALLEY))
	{
		if ((gAlleyCount++ & 0xF) == 0 && ground_debris_ptr < &ground_debris[16])
			*ground_debris_ptr++ = cop;
	}

	pos.vx = cop->pos.vx - camera_position.vx;
	pos.vy = cop->pos.vy - camera_position.vy;
	pos.vz = cop->pos.vz - camera_position.vz;

	MatrixRotate(&pos);

	flags = model->shape_flags;

	if (flags & SHAPE_FLAG_WATER)
	{
		DrawBoatModel(model, cop->yang, &pos, clip == 2);
	}
	else if (!(flags & SHAPE_FLAG_SPRITE) && (cop->yang & 0x3FF) == 0 && (flags & (SHAPE_FLAG_AXIS2 | SHAPE_FLAG_AXIS1)))
	{
		if (clip == 2)
			DrawObjectOSUseAxisChecks(model, cop->yang, &pos, 1);
		else if (clip != 1 && model_bounding_box[cop->type].iTheta >= 200)
			DrawObjectOSUseAxisChecks(model, cop->yang, &pos, 2);
		else
			DrawObjectOSUseAxisChecks(model, cop->yang, &pos, 0);
	}
	else
	{
		if (clip == 2)
			DrawObjectOSReuseMatrices(model, cop->yang, &pos, 1);
		else if (clip != 1 && model_bounding_box[cop->type].iTheta >= 200)
			DrawObjectOSReuseMatrices(model, cop->yang, &pos, 2);
		else
			DrawObjectOSReuseMatrices(model, cop->yang, &pos, 0);
	}

	CurrentRotationMatrix = &inv_camera_matrix;
}

// [0x4f05d7] [D]
// As DrawCellObject; single polygon road and tile models use the simple DrawObjectOS.
void DrawCellObjectCheckIfRoad(CELL_OBJECT *cop)
{
	VECTOR4 pos;
	MODEL *model;
	int flags;
	int clip;

	model = model_pointers[cop->type];

	clip = FrustrumCheckFar(&cop->pos, model->bounding_sphere);

	if (!clip)
		return;

	flags = model->shape_flags;

	if (flags & SHAPE_FLAGS_ANIMOBJ)
	{
		DrawAnimatingObject(model, cop, clip);
		CurrentRotationMatrix = &inv_camera_matrix;
		return;
	}

	if ((flags & SHAPE_FLAG_SPRITE) && (model->flags2 & MODEL_FLAG_TREE))
	{
		if ((gTreeCount++ & 0xF) == 0 && ground_debris_ptr < &ground_debris[16])
			*ground_debris_ptr++ = cop;
	}
	else if ((flags & SHAPE_FLAG_TILE) && (model->flags2 & MODEL_FLAG_ALLEY))
	{
		if ((gAlleyCount++ & 0xF) == 0 && ground_debris_ptr < &ground_debris[16])
			*ground_debris_ptr++ = cop;
	}

	pos.vx = cop->pos.vx - camera_position.vx;
	pos.vy = cop->pos.vy - camera_position.vy;
	pos.vz = cop->pos.vz - camera_position.vz;

	MatrixRotate(&pos);

	if (!(flags & SHAPE_FLAG_SPRITE))
	{
		if ((flags & (SHAPE_FLAG_ROAD | SHAPE_FLAG_TILE)) && model->num_polys == 1)
		{
			DrawObjectOS(model, cop->yang, &pos, clip == 2);
			CurrentRotationMatrix = &inv_camera_matrix;
			return;
		}

		if ((cop->yang & 0x3FF) == 0 && (flags & (SHAPE_FLAG_AXIS2 | SHAPE_FLAG_AXIS1)))
		{
			if (clip == 2)
				DrawObjectOSUseAxisChecks(model, cop->yang, &pos, 1);
			else if (clip != 1 && model_bounding_box[cop->type].iTheta >= 200)
				DrawObjectOSUseAxisChecks(model, cop->yang, &pos, 2);
			else
				DrawObjectOSUseAxisChecks(model, cop->yang, &pos, 0);

			CurrentRotationMatrix = &inv_camera_matrix;
			return;
		}
	}

	if (clip == 2)
		DrawObjectOSReuseMatrices(model, cop->yang, &pos, 1);
	else if (clip != 1 && model_bounding_box[cop->type].iTheta >= 200)
		DrawObjectOSReuseMatrices(model, cop->yang, &pos, 2);
	else
		DrawObjectOSReuseMatrices(model, cop->yang, &pos, 0);

	CurrentRotationMatrix = &inv_camera_matrix;
}

// [0x4f0933] [D]
void GetVisSetAtPosition(VECTOR4 *pos, char *vis, int *p_ccx, int *p_ccz)
{
	float x;
	float z;
	int ccx;
	int ccz;

	x = ((float)units_across_halved + pos->vx) * cell_recip;
	z = ((float)units_down_halved + pos->vz) * cell_recip;

	ccx = (int)x;
	*p_ccx = ccx;

	ccz = (int)z;
	*p_ccz = ccz;

	GetPVSRegionCell(cells_across / region_size * (ccz / region_size) + ccx / region_size,
		ccx % region_size + region_size * (ccz % region_size), vis);
}

// [0x4f0a14] [D]
// Sorts the objects of the visible cells: normal ones are drawn at once, the others are collected
// and drawn after (neon, alpha, shadows) or later in the frame (water, transparent).
void DrawMap(int update_cells)
{
	int cellz;
	int hpos;
	int i;
	int cellxpos;
	int cell_region_size;
	char *PVS_ptr;
	short *arc;
	int cellx;
	int vloop;
	int depth_mask;
	CELL_DATA *cdp;
	int palette;
	int j;
	int hloop;
	CELL_OBJECT *cop;
	int camz;
	int vcount;
	int hcount;
	int vpos;
	int camx;

	gTreeCount = 0;

	if (update_cells)
	{
		GenerateAxisPrecalcs();

		if (!gDoMotelRoom)
			SwapPalette(1);
	}

	ControlMap(update_cells);

	GetPVSRegionCell(camera.current_region,
		camera.current_cell_x % region_size + region_size * (camera.current_cell_z % region_size), CurrentPVS);

	if (!update_cells)
		return;

	// arc_view: per 16th of the camera angle, the first row and number of rows, then the
	// first cell and number of cells of each row
	arc = &arc_view[64 * (camera_angle.vy / 16)];

	cellxpos = camera.current_cell_x - 15;
	cellz = camera.current_cell_z + arc[0] - 15;

	neon_count = 0;
	trans_count = 0;
	alpha_count = 0;
	water_count = 0;
	shadow_count = 0;

	camera.barrel_region_z = cellz / region_size;
	camz = cellz - region_size * camera.barrel_region_z;

	cell_region_size = region_size * region_size;

	vpos = arc[0];
	vcount = arc[1] + vpos;

	PVS_ptr = &CurrentPVS[pvs_square * vpos];

	CurrentRotationMatrix = &inv_camera_matrix;

	// top-down view!
	if (cameraview == 6)
	{
		cellxpos = camera.current_cell_x;
		cellz = camera.current_cell_z - 15;

		camera.barrel_region_z = (camera.current_cell_z - 15) / region_size;
		camz = camera.current_cell_z - 15 - region_size * camera.barrel_region_z;

		for (vloop = -15; vloop < 15; vloop++)
		{
			for (hloop = -15; hloop < 15; hloop++)
			{
				camera.barrel_region_x = (cellxpos + hloop) / region_size;
				camx = cellxpos + hloop - region_size * camera.barrel_region_x;

				if (cellxpos + hloop >= 0 && cellz >= 0 && cellxpos + hloop < cells_across && cellz < cells_down)
				{
					camera.current_barrel_region = (camera.barrel_region_x & 1) + 2 * (camera.barrel_region_z & 1);

					// not a for loop
					for (i = cell_ptrs[region_size * camz + camx + cell_region_size * camera.current_barrel_region]; i != -1; i = cdp->next)
					{
						cdp = &cells[i];
						cop = &cell_objects[cdp->num];

						if (cop->computed == current_object_computed_value || cop->ghost)
							continue;

						if (ModelActive[cop->type])
						{
							switch (draw_type[cop->type])
							{
								case DRAW_NORMAL:
									DrawCellObjectCheckIfRoad(cop);
									break;
								case DRAW_WATER:
									if (water_count < MAX_DRAWN_OBJECTS)
										water_objects[water_count++] = cop;
									break;
								case DRAW_SHADOW:
									if (shadow_count < MAX_DRAWN_OBJECTS)
										shadow_objects[shadow_count++] = cop;
									break;
								case DRAW_ALPHA:
									if (alpha_count < MAX_DRAWN_OBJECTS)
										alpha_objects[alpha_count++] = cop;
									break;
								case DRAW_SUBMERGED:
									if (DrawSubmergedObjects)
										DrawCellObjectCheckIfRoad(cop);
									break;
								case DRAW_TRANS:
									if (trans_count < MAX_DRAWN_OBJECTS)
										trans_objects[trans_count++] = cop;
									break;
								case DRAW_NEON:
									if (neon_count < MAX_NEON_OBJECTS)
										neon_objects[neon_count++] = cop;
									break;
							}
						}

						cop->computed = current_object_computed_value;
					}
				}

				if (camx + 1 == region_size)
					camera.barrel_region_x++;
			}

			if (++camz == region_size)
			{
				camz = 0;
				camera.barrel_region_z++;
			}

			PVS_ptr += pvs_square;
			cellz++;
		}
	}
	else
	{
		// draw normally
		for (vloop = vpos; vloop < vcount; vloop++)
		{
			hpos = arc[2 * vloop + 2];

			if (hpos != -1)
			{
				cellx = cellxpos + hpos;
				hcount = arc[2 * vloop + 3] + hpos;

				camera.barrel_region_x = (cellxpos + hpos) / region_size;
				camx = cellxpos + hpos - region_size * camera.barrel_region_x;

				for (hloop = hpos; hloop < hcount; hloop++)
				{
					if (cellx >= 0 && cellz >= 0 && cellx < cells_across && cellz < cells_down && PVS_ptr[hloop])
					{
						camera.current_barrel_region = (camera.barrel_region_x & 1) + 2 * (camera.barrel_region_z & 1);

						// not a for loop
						for (i = cell_ptrs[region_size * camz + camx + cell_region_size * camera.current_barrel_region]; i != -1; i = cdp->next)
						{
							cdp = &cells[i];
							cop = &cell_objects[cdp->num];

							if (cop->computed == current_object_computed_value || cop->ghost)
								continue;

							// NOTE: the lists are not bounds checked here, unlike the top-down view
							if (ModelActive[cop->type])
							{
								switch (draw_type[cop->type])
								{
									case DRAW_NORMAL:
										DrawCellObjectCheckIfRoad(cop);
										break;
									case DRAW_WATER:
										water_objects[water_count++] = cop;
										break;
									case DRAW_SHADOW:
										shadow_objects[shadow_count++] = cop;
										break;
									case DRAW_ALPHA:
										alpha_objects[alpha_count++] = cop;
										break;
									case DRAW_SUBMERGED:
										if (DrawSubmergedObjects)
											DrawCellObjectCheckIfRoad(cop);
										break;
									case DRAW_TRANS:
										trans_objects[trans_count++] = cop;
										break;
									case DRAW_NEON:
										neon_objects[neon_count++] = cop;
										break;
								}
							}

							cop->computed = current_object_computed_value;
						}
					}

					if (++camx == region_size)
					{
						camx = 0;
						camera.barrel_region_x++;
					}

					cellx++;
				}
			}

			if (++camz == region_size)
			{
				camz = 0;
				camera.barrel_region_z++;
			}

			PVS_ptr += pvs_square;
			cellz++;
		}
	}

	// draw neon objects
	if (neon_count)
	{
		palette = LastPaletteNo;
		SwapPalette(NEON_PALETTE);

		for (j = 0; j < neon_count; j++)
			DrawCellObjectCheckIfRoad(neon_objects[j]);

		SwapPalette(palette);
	}

	// draw alpha objects
	if (alpha_count)
	{
		last_state = -1;
		last_trans_on = -1;
		gPreviousAlpha = -1;

		grAlphaCombine(GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_ONE, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_TEXTURE, FXTRUE);
		grAlphaTestFunction(GR_CMP_GREATER);
		gPreviousAlpha = 1;

		for (j = 0; j < alpha_count; j++)
			DrawCellObject(alpha_objects[j]);
	}

	// draw shadow objects
	if (gDrawShadows && shadow_count && !gNight && !do_fogging)
	{
		last_state = -1;
		last_trans_on = -1;
		gPreviousAlpha = -1;

		grAlphaCombine(GR_COMBINE_FUNCTION_SCALE_OTHER_ADD_LOCAL, GR_COMBINE_FACTOR_ONE, GR_COMBINE_LOCAL_CONSTANT, GR_COMBINE_OTHER_TEXTURE, FXTRUE);
		grAlphaTestFunction(GR_CMP_ALWAYS);
		gPreviousAlpha = 2;

		depth_mask = EnableDepthMask(0);

		// lifted a little above the ground they lie on
		for (j = 0; j < shadow_count; j++)
		{
			shadow_objects[j]->pos.vy -= 4.0f;
			DrawCellObject(shadow_objects[j]);
			shadow_objects[j]->pos.vy += 4.0f;
		}

		EnableDepthMask(depth_mask);
	}

	// restore alpha state
	if (gPreviousAlpha)
	{
		grAlphaCombine(GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_ONE, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_CONSTANT, FXTRUE);
		grAlphaTestFunction(GR_CMP_ALWAYS);
		gPreviousAlpha = 0;
	}
}

// [0x4f1429] [D]
void DrawWater(void)
{
	CELL_OBJECT **list;
	int count;

	if (water_count)
	{
		CurrentRotationMatrix = &inv_camera_matrix;

		count = water_count;
		list = water_objects;

		while (count--)
			DrawCellObject(*list++);
	}
}

// [0x4f1484] [D]
void DrawTransparentObjects(void)
{
	CELL_OBJECT **list;
	int count;

	if (trans_count)
	{
		CurrentRotationMatrix = &inv_camera_matrix;

		count = trans_count;
		list = trans_objects;

		force_transparency = 2;

		while (count--)
			DrawCellObject(*list++);

		force_transparency = 0;
	}
}

// [0x4f14ed] [D]
// The horizon is models 0 - 3; the sky of the night, rain and dawn variants is copied over the
// day sky in the sky texture set.
void InitHorizon(void)
{
	void *src;
	int tset;
	int i;
	TEX_INFO *dst_info;
	void *dst;
	int variant;
	TEX_INFO *src_info;
	int day;

	ScaleVertices(model_pointers[0], 1.7f, 1.7f, 1.7f);
	ScaleVertices(model_pointers[1], 1.7f, 1.7f, 1.7f);
	ScaleVertices(model_pointers[2], 1.7f, 1.7f, 1.7f);
	ScaleVertices(model_pointers[3], 1.7f, 1.7f, 1.7f);

	ScaleNormals(model_pointers[0], 1.7f);
	ScaleNormals(model_pointers[1], 1.7f);
	ScaleNormals(model_pointers[2], 1.7f);
	ScaleNormals(model_pointers[3], 1.7f);

	if (GameLevel > 3)
		return;

	day = GameLevel * 10;

	if (gDoDawn || gDoDusk)
		variant = day + 8;
	else
		variant = day + gDoRain * 4 + gNight * 2;

	if (variant == day)
		return;

	for (i = 0; i < 2 && SkyTextures[variant]; i++)
	{
		dst_info = GetTextureDetails(SkyTextures[day]);
		tset = dst_info->tset;

		src_info = GetTextureDetails(SkyTextures[variant]);

		if (!src_info)
			return;

		src = TextureSlots[src_info->tset].data;
		dst = TextureSlots[tset].data;

		if (gSky16Bit)
			BlitTexture(src, src_info->u, src_info->v, src_info->w + 1, src_info->h + 1, dst, dst_info->u, dst_info->v);
		else
			BlitTexture32(src, src_info->u, src_info->v, src_info->w + 1, src_info->h + 1, dst, dst_info->u, dst_info->v);

		variant++;
		day++;
	}

	if (i)
		ReplaceTextureSet(tset, TextureSlots[tset].data);
}

// [0x4f178a] [D]
void DrawHorizon(void)
{
	int depth_mask;
	VECTOR4 pos;
	int sky;
	MATRIX *matrix;

	pos.vx = 0.0f;
	pos.vy = 40.0f;
	pos.vz = 36.0f;
	pos.vw = 0.0f;

	if (gDoDawn || gDoDusk)
		sky = 4;
	else
		sky = gNight + gDoRain * 2;

	SwapPalette(gAltSkyPalettes[GameLevel][sky]);

	CurrentRotationMatrix = &inv_camera_matrix;
	MatrixRotate(&pos);

	matrix = &(*matrix_mem)[horizon_angle];

	highlight = 0;

	depth_mask = EnableDepthMask(0);
	grDepthBiasLevel(0x7FFF);

	DrawObject(model_pointers[0], matrix, &pos, 1);
	DrawObject(model_pointers[1], matrix, &pos, 1);
	DrawObject(model_pointers[2], matrix, &pos, 1);
	DrawObject(model_pointers[3], matrix, &pos, 1);

	grDepthBiasLevel(0);
	EnableDepthMask(depth_mask);
}

// [0x4f18b3] [D]
// Unpacks the potentially visible set of a cell: pvs_square x pvs_square flags, run length coded
// (a byte of 8-cell runs that are all visible, then a byte count of bit mask bytes).
void GetPVSRegionCell(int region, int cell, char *output)
{
	char *bufptr;
	int k;
	int j;
	unsigned char workbyte;
	char *tbp;
	unsigned char bits;
	int i;

	if (!PackedPVSRegions)
	{
		for (j = 0; j < pvs_square_sq; j++)
			*output++ = 1;

		return;
	}

	bufptr = PackedPVSRegions + *(int *)(PackedPVSRegions + region * 4 + 16);
	tbp = bufptr + (*(int *)(bufptr + cell * 4) & 0x3FFFFFFF);

	if (tbp == bufptr)
	{
		SendFullCell(output);
		return;
	}

	k = pvs_square_sq;

	if (pvs_square_sq & 7)
		k++;

	while (k)
	{
		workbyte = *tbp++;
		k -= workbyte * 8;

		while (workbyte)
		{
			*output++ = 1;
			*output++ = 1;
			*output++ = 1;
			*output++ = 1;
			*output++ = 1;
			*output++ = 1;
			*output++ = 1;
			*output++ = 1;
			workbyte--;
		}

		if (!k)
			break;

		for (workbyte = *tbp++; workbyte; workbyte--)
		{
			bits = *tbp++;

			for (i = 0; i < 8; i++)
			{
				*output++ = bits & 1;

				if (--k == 0)
					break;

				bits >>= 1;
			}
		}
	}
}

// [0x4f1ab7] [D]
void SendFullCell(char *output)
{
	int i;

	for (i = 0; i < pvs_square_sq; i++)
		*output++ = 1;
}

// [0x4f1aed] [D]
void LoadCosmeticsAndDentData(void)
{
	FILE *f;

	LogQuick("*** Loading cosmetics data ***");

	f = fopen(level_cos_files[GameLevel], "rb");

	if (f)
	{
		fread(car_cosmetics, sizeof(car_cosmetics), 1, f);
		fclose(f);
		LogQuick("*** Cosmetics data loaded ***");
	}
	else
	{
		LogQuick("XXX Cosmetics data failed to load!");
	}

	LogQuick("*** Loading denting data ***");

	if (GameLevel == 6)
		f = fopen("LEVELS\\NC_01.DEN", "rb");
	else
		f = fopen("LEVELS\\miami_01.DEN", "rb");

	if (f)
	{
		fread(gDamageZoneVerts, sizeof(gDamageZoneVerts), 1, f);
		fread(gCarShinyPolyLookup, sizeof(gCarShinyPolyLookup), 1, f);
		fread(gCarPolyDentRegionLookup, sizeof(gCarPolyDentRegionLookup), 1, f);
		fclose(f);
		LogQuick("*** Dentings data loaded ***");
	}
	else
	{
		LogQuick("XXX Dentings data failed to load!");
	}
}

// [0x4f1c1d] [D]
// <level>.D3D: the palettes each texture set is drawn with (an emulated texture per palette)
void LoadPaletteData(char *filename)
{
	FILE *f;
	D3D_TEXTURE *tex;
	int i;

	f = fopen(filename, "rb");

	if (!f)
		return;

	fread(&num_d3d_textures, 1, 4, f);

	d3d_textures_mem = (D3D_TEXTURE *)DebugMalloc(num_d3d_textures * sizeof(D3D_TEXTURE), "D:\\Driver\\game6.4g\\map.c", 3263);

	tex = d3d_textures_mem;

	for (i = 0; i < num_d3d_textures; i++)
	{
		fread(&tex->tset, 1, 4, f);
		fread(&tex->num_palettes, 1, 4, f);

		tex->palettes = (int *)DebugMalloc(tex->num_palettes * sizeof(int), "D:\\Driver\\game6.4g\\map.c", 3269);

		fread(tex->palettes, tex->num_palettes, 4, f);

		tex++;
	}

	fclose(f);
}

// [0x4f1d23] [D]
void FreePaletteData(void)
{
	D3D_TEXTURE *tex;
	int i;

	grReleaseEmulatedTextures();

	if (d3d_textures_mem)
	{
		tex = d3d_textures_mem;

		for (i = 0; i < num_d3d_textures; i++)
		{
			DebugFree(tex->palettes);
			tex++;
		}

		DebugFree(d3d_textures_mem);

		d3d_textures_mem = NULL;
		num_d3d_textures = 0;
	}
}

// [0x4f1d9c] [D]
// Creates the emulated (paletted) textures of a texture set: one per palette listed in the D3D
// file, else with palette 1.
void AllocateTexture(int TSet, char *TSetMem)
{
	D3D_TEXTURE *tex;
	int *pal;
	int j;
	int i;

	tex = d3d_textures_mem;

	for (i = 0; i < num_d3d_textures; i++)
	{
		if (tex->tset == TSet)
		{
			pal = tex->palettes;

			for (j = 0; j < tex->num_palettes; j++)
			{
				grCreateEmulatedTexture(TSet, TSetMem, *pal, gPalette[*pal]);
				pal++;
			}

			return;
		}

		tex++;
	}

	grCreateEmulatedTexture(TSet, TSetMem, 1, gPalette[1]);
}
