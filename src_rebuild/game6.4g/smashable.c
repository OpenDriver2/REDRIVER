/*
 * smashable.c - smashable objects (cones, bins, signs ...) knocked over by the cars, damaged lamps
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "smashable.c", 0x524ec0 - 0x52567a).
 *
 * damage_object takes a hit object out of the map (its x is moved far away) and keeps it in one
 * of six damaged_object slots, which MoveSmashable_object flies and DrawSmashable_sprites draws
 * tumbling until it lands. The smashable[] table of hittable models is objanim.c's.
 */

#include <stdlib.h>

#include "dr_types.h"
#include "smashable.h"
#include "dr_globals.h"
#include "D3DGlide/glide_ptrs.h"
#include "ai.h"
#include "asmmath.h"
#include "carnage.h"
#include "chairs.h"
#include "handling.h"
#include "map.h"
#include "MScript.h"
#include "Engine/3dfx/object.h"
#include "pause.h"
#include "Engine/3dfx/gfxinit.h"
#include "Engine/3dfx/polydraw.h"
#include "camera.h"

#define MAX_DAMAGED_OBJECTS	6
#define MAX_DAMAGED_LAMPS	4

// a knocked over object: its x position (the cell object's x is moved out of the map while it
// flies), velocity and tumble
typedef struct DAMAGE_OBJECT
{
	int damage;					// frames in the air
	int rot_speed;
	int field_08;				// 1 or -1: the direction of the x tumble
	int vx;
	SVECTOR velocity;
	CELL_OBJECT *cop;
} DAMAGE_OBJECT;

// NOTE: bss globals in the linker's common area (0xc5d820 - 0xc5eb20); defined here as
// smashable.c is their only user (damaged_lamp: and debris.c)
DAMAGE_OBJECT damaged_object[MAX_DAMAGED_OBJECTS];
int NextDamagedLamp;
DAMAGE_LAMP damaged_lamp[MAX_DAMAGED_LAMPS];
int giNumHiddenModels;
int NextDamagedPmeter;
int ModelTimeBonus[1152];		// seconds added to the mission timer when the model is hit
int giSmashablesHit;

// [0x524ec0] [D]
void InitialiseSmashableObjects(void)
{
	giSmashablesHit = 0;
	NextDamagedLamp = 0;
	NextDamagedPmeter = 0;
	giNumHiddenModels = 0;

	ClearMem(damaged_lamp, sizeof(damaged_lamp));
	ClearMem(damaged_object, sizeof(damaged_object));
	ClearMem(ModelTimeBonus, sizeof(ModelTimeBonus));
}

// [0x524f20] [D]
// Draws the flying objects: the opaque ones (transparent 0) or the transparent ones (1).
void DrawSmashable_sprites(char transparent)
{
	MATRIX m;
	MODEL *model;
	int clip;
	VECTOR4 pos;
	DAMAGE_OBJECT *dam;
	int i;
	CELL_OBJECT *cop;
	int shape_flags;

	dam = damaged_object;

	if (transparent)
		force_transparency = 2;

	for (i = 0; i < MAX_DAMAGED_OBJECTS; i++, dam++)
	{
		cop = dam->cop;

		if (!cop)
			continue;

		model = model_pointers[cop->type];

		if (model->shape_flags & SHAPE_FLAG_TRANS)
		{
			if (!transparent)
				continue;
		}
		else if (transparent == 1)
		{
			continue;
		}

		if ((model->shape_flags & SHAPE_FLAG_ALPHA) && gPreviousAlpha != 1)
		{
			grAlphaCombine(GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_ONE, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_TEXTURE, FXTRUE);
			grAlphaTestFunction(GR_CMP_GREATER);
			gPreviousAlpha = 1;
		}

		// put the object back in place for drawing
		cop->pos.vx = (float)dam->vx;

		if (PositionVisible(&cop->pos, CurrentPVS, camera.current_cell_x, camera.current_cell_z))
		{
			clip = FrustrumCheckFar(&cop->pos, model->bounding_sphere);

			if (clip)
			{
				m = (*matrix_mem)[-camera_angle.vy & 0xFFF];

				RotMatrixX(&m, (dam->field_08 * dam->damage * dam->rot_speed) & 0xFFF);
				RotMatrixZ(&m, (dam->damage * dam->rot_speed) & 0xFFF);

				pos.vx = cop->pos.vx - camera_position.vx;
				pos.vy = cop->pos.vy - camera_position.vy;
				pos.vz = cop->pos.vz - camera_position.vz;

				CurrentRotationMatrix = &inv_camera_matrix;
				MatrixRotate(&pos);

				// drawn as a model even if it is a sprite
				shape_flags = model->shape_flags;
				model->shape_flags &= ~SHAPE_FLAG_SPRITE;

				if (clip == 1)
					DrawObject(model, &m, &pos, 0);
				else if (clip == 2)
					DrawObject(model, &m, &pos, 1);
				else
					DrawObject(model, &m, &pos, 2);

				model->shape_flags = shape_flags;
			}
		}

		if ((model->shape_flags & SHAPE_FLAG_ALPHA) && gPreviousAlpha != 0)
		{
			grAlphaCombine(GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_ONE, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_CONSTANT, FXTRUE);
			grAlphaTestFunction(GR_CMP_ALWAYS);
			gPreviousAlpha = 0;
		}

		cop->pos.vx = -20000000.0f;
	}

	force_transparency = 0;
}

// [0x5251d2] [D]
// Flies the knocked over objects under gravity; one bounces on landing, then it is gone.
void MoveSmashable_object(void)
{
	int height;
	VECTOR4 pos;
	DAMAGE_OBJECT *dam;
	int count;
	CELL_OBJECT *cop;

	dam = damaged_object;

	if (GamePaused)
		return;

	for (count = 0; count < MAX_DAMAGED_OBJECTS; count++, dam++)
	{
		cop = dam->cop;

		if (!cop)
			continue;

		pos.vx = (float)dam->vx;
		pos.vz = cop->pos.vz;

		height = 50 - MapHeight(&pos);

		if (cop->pos.vy >= (float)height)
		{
			if (dam->velocity.vy <= 20)
			{
				dam->cop = NULL;
			}
			else
			{
				cop->pos.vy = (float)(height - 1);
				dam->velocity.vy /= -4;
			}
		}
		else
		{
			dam->vx += dam->velocity.vx / 2;
			cop->pos.vy += (float)(dam->velocity.vy / 2);
			cop->pos.vz += (float)(dam->velocity.vz / 2);

			dam->damage++;
			dam->velocity.vy += 8;
		}
	}
}

// [0x525334] [D]
// Counts a hit on a lamp; returns 1 on the hit that breaks it (the third).
// NOTE: BUG: wraps NextDamagedLamp only after 4 (> 4), so the fifth new lamp goes to
// damaged_lamp[4], past the end (over giNumHiddenModels and NextDamagedPmeter in the original)
int damage_lamp(CELL_OBJECT *cop)
{
	int i;
	int old;

	for (i = 0; i < MAX_DAMAGED_LAMPS; i++)
	{
		if (cop == damaged_lamp[i].cop)
		{
			old = damaged_lamp[i].hits++;

			return old < 3 && damaged_lamp[i].hits >= 3;
		}
	}

	damaged_lamp[NextDamagedLamp].hits = 0;
	damaged_lamp[NextDamagedLamp++].cop = cop;

	if (NextDamagedLamp > MAX_DAMAGED_LAMPS)
		NextDamagedLamp = 0;

	return 0;
}

// [0x5253fa] [D]
// Shows or hides a model on the map (mission scripts); returns its previous state.
int EnableModel(int model, int active)
{
	int old;

	old = ModelActive[model];
	ModelActive[model] = active;

	return old;
}

// [0x52541f] [D]
// Knocks a hit object over: carnage, time bonus, and it flies off with the car's velocity.
// Returns 1 if it is flying already.
int damage_object(CELL_OBJECT *cop, IVECTOR *velocity)
{
	char tmp;
	DAMAGE_OBJECT *dam;
	int i;

	dam = damaged_object;

	for (i = 0; i < MAX_DAMAGED_OBJECTS; i++, dam++)
	{
		if (cop == dam->cop)
		{
			dam->damage++;
			return 1;
		}
	}

	if (carnage_flags & 1)
		ReportCarnage(cop->type);

	// (outside carnage mode as well)
	AddTimeBonus(&mission, ModelTimeBonus[cop->type]);

	if (mission.payback && NearestTargetDistance() < mission.payback_radius)
		AddTimeBonus(&mission, 1);

	giSmashablesHit++;

	dam = &damaged_object[NextDamagedPmeter++];

	dam->damage = 0;
	dam->cop = cop;
	dam->vx = float_to_int(cop->pos.vx);

	dam->velocity.vx = velocity->vx >> 10;
	dam->velocity.vz = velocity->vz >> 10;

	// up by the car's speed: -(|x| + |z|) / 2 (16 bit, of the full velocity)
	if (dam->velocity.vx < 0)
		dam->velocity.vy = velocity->vx;
	else
		dam->velocity.vy = -velocity->vx;

	if (dam->velocity.vz < 0)
		dam->velocity.vy += velocity->vz;
	else
		dam->velocity.vy -= velocity->vz;

	dam->velocity.vy >>= 1;

	tmp = rand();

	if (tmp & 1)
		dam->rot_speed = dam->velocity.vy / 2;
	else
		dam->rot_speed = -dam->velocity.vy / 2;

	if (tmp & 2)
		dam->field_08 = 1;
	else
		dam->field_08 = -1;

	if (dam->velocity.vy < -150)
		dam->velocity.vy = -150;

	if (NextDamagedPmeter >= MAX_DAMAGED_OBJECTS)
		NextDamagedPmeter = 0;

	SmashChair(cop->pos.vx, cop->pos.vz);

	// out of the map while it flies
	cop->pos.vx = -20000000.0f;

	return 0;
}
