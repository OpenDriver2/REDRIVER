/*
 * objanim.c - animated map objects: traffic lights, lamp haze, smoking drums, bobbing boats,
 * lamp and billboard colours, neon palette cycling, the table of smashable models
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "objanim.c", 0x500950 - 0x501cc6).
 *
 * InitAnimatingObjects marks the models of the level's ANIMATED_OBJECT list (and their instances)
 * SHAPE_FLAGS_ANIMOBJ; DrawCellObject then draws them with DrawAnimatingObject, which adds the
 * object's effect (lights, smoke, bobbing) at its position.
 * Data (0x574db0 - 0x575bf4): neon_table, smashable, the per level animated object lists and the
 * strings; the four bytes of zeros after Lev0/1/2AnimObjects and Level_NumAnimatedObjects are
 * padding, left out.
 */

#include <stdlib.h>
#include <string.h>

#include "dr_types.h"
#include "objanim.h"
#include "dr_globals.h"
#include "asmmath.h"
#include "map.h"
#include "roadbits.h"
#include "shell.h"
#include "testmath.h"
#include "Engine/3dfx/object.h"
#include "Engine/3dfx/polydraw.h"
#include "debris.h"
#include "pause.h"

// a model with an effect; internal_id says which (see DrawAnimatingObject), x y z is the effect's
// offset from the model, data its parameters
typedef struct ANIMATED_OBJECT
{
	int internal_id;
	int model_num;				// set from name by InitAnimatingObjects
	char *name;
	float x;
	float y;
	float z;
	float w;
	short data[4];
} ANIMATED_OBJECT;

// a range of palette 19 entries rotated by one every (mask + 1) frames
typedef struct NEON_CYCLE
{
	int field_00;				// first entry
	int field_04;				// last entry
	int field_08;				// frame mask
} NEON_CYCLE;

#define NUM_NEON_CYCLES		16

//------------------------------------------------------------------------------------------
// initialised data

NEON_CYCLE neon_table[NUM_NEON_CYCLES] =
{
	{ 1, 7, 1 },
	{ 8, 11, 7 },
	{ 12, 13, 7 },
	{ 14, 15, 7 },
	{ 16, 31, 1 },
	{ 32, 47, 1 },
	{ 48, 55, 3 },
	{ 56, 63, 3 },
	{ 64, 71, 3 },
	{ 72, 79, 3 },
	{ 80, 95, 1 },
	{ 96, 103, 3 },
	{ 104, 111, 3 },
	{ 112, 119, 3 },
	{ 120, 127, 3 },
	{ 0, 0, 0 },
};

// the models a car knocks over and the sound it makes; model set by FindSmashableObjects
SMASHABLE_OBJECT smashable[48] =
{
	{ NULL, "CONE", 29, -100, 9689 },
	{ NULL, "CONE1", 29, -100, 9689 },
	{ NULL, "CONE2", 29, -100, 9689 },
	{ NULL, "CONE3", 29, -100, 9689 },
	{ NULL, "TRAININGCONES1", 29, -100, 9689 },
	{ NULL, "TRAININGCONES2", 29, -100, 9689 },
	{ NULL, "TRAININGCONES3", 29, -100, 9689 },
	{ NULL, "TRAININGCONES4", 29, -100, 9689 },
	{ NULL, "BIGCONE", 29, -100, 9689 },
	{ NULL, "BALLARD", 29, -100, 9689 },
	{ NULL, "BALLARD2", 29, -100, 9689 },
	{ NULL, "SKIP", 31, -3500, 17226 },
	{ NULL, "BIN1", 31, -3500, 17226 },
	{ NULL, "BIN2", 31, -3500, 17226 },
	{ NULL, "BOX1", 28, -100, 22050 },
	{ NULL, "BOX2", 28, -100, 22050 },
	{ NULL, "BARR1", 31, -3500, 17226 },
	{ NULL, "CP_BARRIER", 30, -1300, 16149 },
	{ NULL, "CP_BARR2", 30, -1300, 16149 },
	{ NULL, "FENCE", 31, -3500, 17226 },
	{ NULL, "PHONE", 30, -1300, 12919 },
	{ NULL, "PHONE1", 30, -1300, 12919 },
	{ NULL, "PHONE2", 30, -1300, 12919 },
	{ NULL, "EBOX", 28, -100, 22050 },
	{ NULL, "SIGN1", 31, -3500, 17226 },
	{ NULL, "SIGN2", 31, -3500, 17226 },
	{ NULL, "SIGN3", 31, -3500, 17226 },
	{ NULL, "SIGN4", 31, -3500, 17226 },
	{ NULL, "SIGN5", 31, -3500, 17226 },
	{ NULL, "SIGN6", 31, -3500, 17226 },
	{ NULL, "TABLE", 31, -3500, 17226 },
	{ NULL, "TABLE2", 31, -3500, 17226 },
	{ NULL, "PARK_M", 30, -2500, 48449 },
	{ NULL, "PARASOL", 31, -3500, 17226 },
	{ NULL, "UMBRELLA", 31, -3500, 17226 },
	{ NULL, "CHAIR", 31, -3500, 17226 },
	{ NULL, "CHAIR2", 31, -3500, 17226 },
	{ NULL, "BOARD1", 31, -3500, 17226 },
	{ NULL, "BOARD2", 31, -3500, 17226 },
	{ NULL, "BOARD3", 31, -3500, 17226 },
	{ NULL, "PAPERBOX", 31, -3500, 17226 },
	{ NULL, "PAPERBOX2", 31, -3500, 17226 },
	{ NULL, "FIREH", 30, -2500, 48449 },
	{ NULL, "TAR_DRUM", 30, -1300, 16149 },
	{ NULL, "DRUM", 30, -1300, 16149 },
	{ NULL, "BENCH", 31, -3500, 17226 },
	{ NULL, "HOTDOGS", 30, -1300, 16149 },
	{ NULL, NULL, 0, 0, 0 },
};

ANIMATED_OBJECT Lev0AnimObjects[11] =
{
	{ 0, 0, "TLIGHT01", 0.0f, 0.0f, 0.0f, 0.0f, { 0, 0, 0, 0 } },
	{ 1, 0, "TLIGHT02", 0.0f, 0.0f, 0.0f, 0.0f, { 0, 0, 0, 0 } },
	{ 5, 0, "TAR_DRUM", 0.0f, -308.0f, 0.0f, 0.0f, { 4, 200, 1, 0 } },
	{ 10, 0, "LAMP", -370.0f, -2195.0f, 0.0f, 0.0f, { 0, 96, 7, 0 } },
	{ 10, 0, "LAMP2", -300.0f, -1780.0f, 0.0f, 0.0f, { 0, 96, 7, 0 } },
	{ 11, 0, "DLAMP", 1020.0f, -2400.0f, 100.0f, 0.0f, { -890, 96, 7, 0 } },
	{ 13, 0, "OLDLIGHT", 0.0f, -735.0f, 0.0f, 0.0f, { 0, 48, 0, 0 } },
	{ 7, 453, "YELLOW", 26.0f, -380.0f, 1390.0f, 0.0f, { 30, 16, 6, 0 } },
	{ 8, 452, "WHITE", 300.0f, -2737.0f, -20.0f, 0.0f, { 35, 16, 6, 0 } },
	{ 8, 0, "RED2", 1370.0f, -380.0f, -16.0f, 0.0f, { 40, 16, 8, 0 } },
	{ 8, 0, "YACHT", -120.0f, -2930.0f, -16.0f, 0.0f, { 35, 16, 6, 0 } },
};

ANIMATED_OBJECT Lev1AnimObjects[9] =
{
	{ 2, 0, "TLIGHT01", 0.0f, 0.0f, 0.0f, 0.0f, { 0, 0, 0, 0 } },
	{ 5, 0, "TAR_DRUM", 0.0f, -308.0f, 0.0f, 0.0f, { 4, 200, 1, 0 } },
	{ 10, 0, "LAMP", -370.0f, -2195.0f, 0.0f, 0.0f, { 0, 96, 7, 0 } },
	{ 10, 0, "LAMP2", -420.0f, -2680.0f, 0.0f, 0.0f, { 0, 96, 7, 0 } },
	{ 11, 0, "DLAMP", 2200.0f, -5450.0f, 100.0f, 0.0f, { 0, 128, 0, 0 } },
	{ 13, 0, "OLDLIGHT", 0.0f, -670.0f, 0.0f, 0.0f, { 0, 96, 0, 0 } },
	{ 14, 0, "OLIGHT2", -20.0f, -550.0f, 0.0f, 0.0f, { 0, 80, 0, 0 } },
	{ 7, 0, "BOAT1", 26.0f, -380.0f, 1348.0f, 0.0f, { 30, 24, 6, 0 } },
	{ 8, 0, "BOAT2", 300.0f, -2737.0f, -20.0f, 0.0f, { 35, 24, 6, 0 } },
};

ANIMATED_OBJECT Lev2AnimObjects[7] =
{
	{ 3, 0, "TLIGHT01", 0.0f, 0.0f, 0.0f, 0.0f, { 0, 0, 0, 0 } },
	{ 10, 0, "LAMP", -500.0f, -2320.0f, 0.0f, 0.0f, { 0, 96, 0, 0 } },
	{ 10, 0, "LAMP2", -500.0f, -2320.0f, 0.0f, 0.0f, { 0, 96, 0, 0 } },
	{ 10, 0, "LAMP3", -500.0f, -2320.0f, 0.0f, 0.0f, { 0, 96, 0, 0 } },
	{ 11, 0, "DLAMP", 1520.0f, -3920.0f, 100.0f, 0.0f, { -1520, 96, 0, 0 } },
	{ 11, 0, "DLAMP2", 1520.0f, -3920.0f, 100.0f, 0.0f, { -1520, 96, 0, 0 } },
	{ 13, 0, "OLDLIGHT", 0.0f, -670.0f, 0.0f, 0.0f, { 0, 96, 0, 0 } },
};

ANIMATED_OBJECT Lev3AnimObjects[13] =
{
	{ 4, 0, "TLIGHT01", 0.0f, 0.0f, 0.0f, 0.0f, { 0, 0, 0, 0 } },
	{ 4, 0, "TLIGHT02", 0.0f, 0.0f, 0.0f, 0.0f, { 0, 0, 0, 0 } },
	{ 10, 0, "LAMP", -500.0f, -2280.0f, 0.0f, 0.0f, { 0, 96, 0, 0 } },
	{ 11, 0, "DLAMP", 1520.0f, -3920.0f, 100.0f, 0.0f, { -1520, 96, 0, 0 } },
	{ 13, 0, "OLDLIGHT", 0.0f, -1190.0f, 0.0f, 0.0f, { 0, 96, 0, 0 } },
	{ 15, 0, "MANHOLE", 0.0f, 30.0f, 0.0f, 0.0f, { 0, 0, 2, 0 } },
	{ 12, 0, "GRENPOLE", 0.0f, -688.0f, 0.0f, 0.0f, { 0, 64, 8, 0 } },
	{ 12, 0, "REDPOLE", 0.0f, -688.0f, 0.0f, 0.0f, { 0, 64, 6, 0 } },
	{ 6, 0, "CONE3", 0.0f, -610.0f, 0.0f, 0.0f, { 0, 0, 2, 0 } },
	{ 5, 0, "TAR_DRUM", 0.0f, -200.0f, 0.0f, 0.0f, { 3, 200, 1, 0 } },
	{ 7, 0, "SPEEDBT", -16.0f, -380.0f, 1326.0f, 0.0f, { 40, 24, 8, 0 } },
	{ 8, 0, "YACHT", 300.0f, -2737.0f, -20.0f, 0.0f, { 35, 24, 6, 0 } },
	{ 9, 0, "TUGBOAT", 275.0f, -1155.0f, 0.0f, 0.0f, { 40, 0, 1, 0 } },
};

ANIMATED_OBJECT *Level_AnimatingObjectsPtrs[5] =
{
	Lev0AnimObjects,
	Lev1AnimObjects,
	Lev2AnimObjects,
	Lev3AnimObjects,
	NULL,
};

int Level_NumAnimatedObjects[5] = { 11, 9, 7, 13, 0 };

//------------------------------------------------------------------------------------------
// uninitialised data
// NOTE: a bss global in the linker's common area; defined here as objanim.c is its only user
int num_anim_objects;

// [0x500950] [D]
void FindSmashableObjects(void)
{
	SMASHABLE_OBJECT *sip;

	for (sip = smashable; sip->name; sip++)
		sip->model = FindModelPtrWithName(sip->name);
}

// [0x500989] [D]
// Colours the lamp glass polygons (and those of the lamp models' instances): yellowish white at
// night, grey by day; the lit flat polygons become unlit.
void SetStreetLampPolys(int night)
{
	static char *lamps[5] = { "LAMP", "LAMP2", "LAMP3", "DLAMP", "DLAMP2" };
	unsigned char r, g, b;
	int num_polys;
	int i;
	int k;
	int model_idx;
	char *poly;

	if (night == 1)
	{
		r = 255;
		g = 255;
		b = 240;
	}
	else
	{
		r = 48;
		g = 48;
		b = 45;
	}

	for (i = 0; i < 5; i++)
	{
		model_idx = FindModelWithName(lamps[i]);

		if (model_idx == -1)
			continue;

		num_polys = (unsigned short)model_pointers[model_idx]->num_polys;
		poly = (char *)model_pointers[model_idx]->poly_block;

		while (num_polys--)
		{
			if (*poly == FLAT3LIT)
			{
				((POLYF3LIT *)poly)->r = r;
				((POLYF3LIT *)poly)->g = g;
				((POLYF3LIT *)poly)->b = b;
				((POLYF3LIT *)poly)->id = FLAT3;
			}
			else if (*poly == FLAT4LIT || *poly == FLAT4)
			{
				((POLYF4LIT *)poly)->r = r;
				((POLYF4LIT *)poly)->g = g;
				((POLYF4LIT *)poly)->b = b;
				((POLYF4LIT *)poly)->id = FLAT4;
			}

			poly += PolySizes[*(unsigned char *)poly];
		}

		for (k = 0; k < num_models_in_pack; k++)
		{
			if (model_pointers[k]->instance_number != model_idx)
				continue;

			num_polys = (unsigned short)model_pointers[k]->num_polys;
			poly = (char *)model_pointers[k]->poly_block;

			while (num_polys--)
			{
				// (also FLAT3 here, which the model itself has become)
				if (*poly == FLAT3LIT || *poly == FLAT3)
				{
					((POLYF3LIT *)poly)->r = r;
					((POLYF3LIT *)poly)->g = g;
					((POLYF3LIT *)poly)->b = b;
					((POLYF3LIT *)poly)->id = FLAT3;
				}
				else if (*poly == FLAT4LIT || *poly == FLAT4)
				{
					((POLYF4LIT *)poly)->r = r;
					((POLYF4LIT *)poly)->g = g;
					((POLYF4LIT *)poly)->b = b;
					((POLYF4LIT *)poly)->id = FLAT4;
				}

				poly += PolySizes[*(unsigned char *)poly];
			}
		}
	}
}

// [0x500c79] [D]
// Shades the billboards' faces: bright at the top at night, flatter by day.
void LightBillBoards(int night)
{
	static char *boards[7] = { "BILBRD1", "BILBRD2", "BILBRD3", "BILBRD4", "BILBRD5", "BILBRD6", "BILBRD7" };
	POLYGT4 *gt4;
	int num_polys;
	unsigned char hi;
	unsigned char lo;
	int i;
	int model_idx;
	char *poly;

	if (night == 1)
	{
		hi = 255;
		lo = 40;
	}
	else
	{
		hi = 192;
		lo = 176;
	}

	for (i = 0; i < 7; i++)
	{
		model_idx = FindModelWithName(boards[i]);

		if (model_idx == -1)
			continue;

		num_polys = (unsigned short)model_pointers[model_idx]->num_polys;
		poly = (char *)model_pointers[model_idx]->poly_block;

		while (num_polys--)
		{
			if (*poly == GOURTEXT4)
			{
				gt4 = (POLYGT4 *)poly;

				if (i != 0)
				{
					gt4->r0 = gt4->r1 = hi;
					gt4->g0 = gt4->g1 = hi;
					gt4->b0 = gt4->b1 = hi;

					gt4->r2 = gt4->r3 = lo;
					gt4->g2 = gt4->g3 = lo;
					gt4->b2 = gt4->b3 = lo;
				}
				else
				{
					// the first board's vertices go the other way round
					gt4->r0 = gt4->r3 = hi;
					gt4->g0 = gt4->g3 = hi;
					gt4->b0 = gt4->b3 = hi;

					gt4->r1 = gt4->r2 = lo;
					gt4->g1 = gt4->g2 = lo;
					gt4->b1 = gt4->b2 = lo;
				}
			}

			poly += PolySizes[*(unsigned char *)poly];
		}
	}
}

// [0x500f05] [D]
void InitAnimatingObjects(void)
{
	ANIMATED_OBJECT *aop;
	int i;
	int loop;
	int model_idx;

	FindSmashableObjects();
	SetStreetLampPolys(gNight);
	LightBillBoards(gNight);

	if (GameLevel > 4)
		return;

	num_anim_objects = Level_NumAnimatedObjects[GameLevel];

	if (!num_anim_objects)
		return;

	aop = Level_AnimatingObjectsPtrs[GameLevel];

	for (loop = 0; loop < num_anim_objects; loop++, aop++)
	{
		if (aop->name)
			model_idx = FindModelWithName(aop->name);
		else
			model_idx = aop->model_num;

		if (model_idx == -1)
			continue;

		aop->model_num = model_idx;

		model_pointers[model_idx]->shape_flags |= SHAPE_FLAGS_ANIMOBJ;

		for (i = 0; i < num_models_in_pack; i++)
		{
			if (model_pointers[i]->instance_number == model_idx)
				model_pointers[i]->shape_flags |= SHAPE_FLAGS_ANIMOBJ;
		}
	}
}

// [0x501046] [D]
// Draws an animated object with its effect; type is the frustum clip result (1, 2, else).
void DrawAnimatingObject(MODEL *model, CELL_OBJECT *cop, int type)
{
	VECTOR4 drift;
	VECTOR4 smoke_pos;
	VECTOR4 pos;
	VECTOR4 view_pos;
	MATRIX *m;
	float ofs_x, ofs_y, ofs_z;
	float bob;
	float drift_change;
	char jlight;
	int angle;
	int start_w;
	int num;
	int loop;
	ANIMATED_OBJECT *animObj;

	if (GameLevel > 4)
		return;

	animObj = Level_AnimatingObjectsPtrs[GameLevel];

	if (model_pointers[cop->type]->instance_number == -1)
		num = cop->type;
	else
		num = model_pointers[cop->type]->instance_number;

	pos = cop->pos;
	angle = cop->yang & 0xFFF;

	for (loop = 0; loop < num_anim_objects; loop++, animObj++)
	{
		if (num != animObj->model_num)
			continue;

		switch (animObj->internal_id)
		{
			case 0:
				// traffic lights: the junction's phase for the way the model faces
				if ((angle > -10 && angle < 10) || (angle > 2038 && angle < 2058))
					jlight = JunctionLightsNS;
				else
					jlight = JunctionLightsWE;

				switch (jlight)
				{
					case 1:
					case 2:
						AddTrafficLight(cop, -648.0f, -944.0f, -50.0f, 512);
						AddTrafficLight(cop, 136.0f, -944.0f, -50.0f, 512);
						break;
					case 3:
						AddTrafficLight(cop, -460.0f, -944.0f, -50.0f, 2048);
						AddTrafficLight(cop, 324.0f, -944.0f, -50.0f, 2048);
						break;
					case 4:
						AddTrafficLight(cop, -554.0f, -944.0f, -50.0f, 1024);
						AddTrafficLight(cop, 230.0f, -944.0f, -50.0f, 1024);
						break;
				}
				break;
			case 1:
				if ((angle > -10 && angle < 10) || (angle > 2038 && angle < 2058))
					jlight = JunctionLightsNS;
				else
					jlight = JunctionLightsWE;

				switch (jlight)
				{
					case 1:
					case 2:
						AddTrafficLight(cop, -648.0f, -944.0f, -50.0f, 512);
						AddTrafficLight(cop, 348.0f, -944.0f, -50.0f, 512);
						break;
					case 3:
						AddTrafficLight(cop, -460.0f, -944.0f, -50.0f, 2048);
						AddTrafficLight(cop, 536.0f, -944.0f, -50.0f, 2048);
						break;
					case 4:
						AddTrafficLight(cop, -554.0f, -944.0f, -50.0f, 1024);
						AddTrafficLight(cop, 442.0f, -944.0f, -50.0f, 1024);
						break;
				}
				break;
			case 2:
			case 3:
			case 4:
				if ((angle > -10 && angle < 10) || (angle > 2038 && angle < 2058))
					jlight = JunctionLightsNS;
				else
					jlight = JunctionLightsWE;

				switch (jlight)
				{
					case 1:
					case 2:
						AddTrafficLight(cop, -717.0f, -1470.0f, -78.0f, 512);
						break;
					case 3:
						AddTrafficLight(cop, -716.0f, -1239.0f, -78.0f, 2048);
						break;
					case 4:
						AddTrafficLight(cop, -717.0f, -1355.0f, -78.0f, 1024);
						break;
				}
				break;
			case 5:
				// smoking drum
				memset(&drift, 0, sizeof(drift));

				start_w = (rand() & 0x3F) + 80;

				smoke_pos.vx = pos.vx + animObj->x;
				smoke_pos.vy = pos.vy + animObj->y;
				smoke_pos.vz = pos.vz + animObj->z;

				if ((game_frame & 7) == 7)
					Setup_SlowSmoke(&smoke_pos, &drift, start_w, 300, animObj->data[0], animObj->data[1], animObj->data[2], -2.0f, 1);
				break;
			case 6:
				memset(&drift, 0, sizeof(drift));

				start_w = (rand() & 0x1F) + 40;

				smoke_pos.vx = pos.vx + animObj->x;
				smoke_pos.vy = pos.vy + animObj->y;
				smoke_pos.vz = pos.vz + animObj->z;

				if ((game_frame & 7) == 3)
					Setup_SlowSmoke(&smoke_pos, &drift, start_w, 6 * start_w, 2, 120, animObj->data[2], -1.5f, 1);
				break;
			case 7:
				// a boat bobbing on the water, with its light
				bob = fsintab[(16 * ((unsigned char)((int)(pos.vx + pos.vz) + game_frame) - 128)) & 0xFFF] * (float)animObj->data[0];
				pos.vy += bob;

				AddHazeLight(cop, animObj->x, bob + animObj->y, animObj->z, animObj->data[2], animObj->data[1], 256);
				break;
			case 8:
				// ... with a flashing light
				bob = fsintab[(16 * ((unsigned char)((int)(pos.vx + pos.vz) + game_frame) - 128)) & 0xFFF] * (float)animObj->data[0];
				pos.vy += bob;

				if (((int)(pos.vx + pos.vz) + game_frame) & 0x20)
					AddHazeLight(cop, animObj->x, bob + animObj->y, animObj->z, animObj->data[2], animObj->data[1], 256);
				break;
			case 9:
				// ... with a smoking funnel
				start_w = (rand() & 0x3F) + 200;

				memset(&drift, 0, sizeof(drift));

				m = &(*matrix_mem)[angle];

				ofs_x = animObj->x * m->m[0][0] + animObj->z * m->m[2][0];
				ofs_y = animObj->y;
				ofs_z = animObj->x * m->m[0][2] + animObj->z * m->m[2][2];

				bob = fsintab[(16 * ((unsigned char)((int)(pos.vx + pos.vz) + game_frame) - 128)) & 0xFFF] * (float)animObj->data[0];
				pos.vy += bob;

				smoke_pos.vx = pos.vx + ofs_x;
				smoke_pos.vy = pos.vy + ofs_y;
				smoke_pos.vz = pos.vz + ofs_z;

				if ((game_frame & 7) == 7)
					Setup_SlowSmoke(&smoke_pos, &drift, start_w, 500, 7, 200, animObj->data[2], -1.0f, 1);
				break;
			case 10:
			case 12:
				// street lamp, pole light
				AddHazeLight(cop, animObj->x, animObj->y, animObj->z, animObj->data[2], animObj->data[1], 256);
				break;
			case 11:
				// double lamp: the second light at x = data[0]
				AddHazeLight(cop, animObj->x, animObj->y, animObj->z, animObj->data[2], animObj->data[1], 256);
				AddHazeLight(cop, (float)animObj->data[0], animObj->y, animObj->z, animObj->data[2], animObj->data[1], 256);
				break;
			case 13:
				// old street light, flickers when damaged
				AddDamageStreetLight(cop, animObj->x, animObj->y, animObj->z, animObj->data[2], animObj->data[1]);
				break;
			case 14:
				AddHazeLight(cop, animObj->x, animObj->y, animObj->z, animObj->data[2], animObj->data[1], 256);
				break;
			case 15:
				// steaming manhole
				memset(&drift, 0, sizeof(drift));

				start_w = (rand() & 0x1F) + 50;
				drift_change = (float)(rand() & 0xF) / 15.0f + 0.1f;

				smoke_pos.vx = pos.vx + animObj->x;
				smoke_pos.vy = pos.vy + animObj->y;
				smoke_pos.vz = pos.vz + animObj->z;

				if ((game_frame & 7) == 3)
					Setup_SlowSmoke(&smoke_pos, &drift, start_w, 5 * start_w, 4, 50, 2, -drift_change, 1);
				break;
		}
	}

	view_pos.vx = pos.vx - camera_position.vx;
	view_pos.vy = pos.vy - camera_position.vy;
	view_pos.vz = pos.vz - camera_position.vz;

	CurrentRotationMatrix = &inv_camera_matrix;
	MatrixRotate(&view_pos);

	if (type == 1)
		DrawObjectOSReuseMatrices(model, angle, &view_pos, 0);
	else if (type == 2)
		DrawObjectOSReuseMatrices(model, angle, &view_pos, 1);
	else
		DrawObjectOSReuseMatrices(model, angle, &view_pos, 2);
}

// [0x501bf8] [D]
// Rotates the neon ranges of palette 19 by one entry (light flares on).
void DoNeonColourCycle(void)
{
	unsigned int *entry;
	unsigned int last;
	int count;
	int i;

	if (GamePaused || !gDoLightFlare)
		return;

	for (i = 0; i < NUM_NEON_CYCLES; i++)
	{
		if (!neon_table[i].field_00 || (neon_table[i].field_08 & game_frame))
			continue;

		count = neon_table[i].field_04 - neon_table[i].field_00;
		entry = (unsigned int *)&gPalette[19][4 * neon_table[i].field_04];
		last = *entry;

		while (count--)
		{
			entry--;
			entry[1] = entry[0];
		}

		*entry = last;
	}
}
