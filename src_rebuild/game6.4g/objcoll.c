/*
 * objcoll.c - collisions of cars with the scenery
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "objcoll.c", 0x5021b0 - 0x5033fc).
 *
 * For the player car (and the chase car 65 when recording or replaying a mission) and the civilian
 * cars (CheckCivilianScenaryCollisions in ai.c), the collision packets of the models in the car's
 * cell, of the event models and of the helicopter become building boxes (BUILDING_BOX). Boxes of
 * the player car next to each other are merged into one (MergeAllBoxes) so the car doesn't catch
 * the joins of a wall; CarBuildingCollision (bcollide.c) does the response.
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dr_types.h"
#include "objcoll.h"
#include "dr_globals.h"
#include "asmmath.h"
#include "bcollide.h"
#include "denting.h"
#include "event.h"
#include "general.h"
#include "handling.h"
#include "job_fx.h"
#include "map.h"
#include "shell.h"
#include "Engine/3dfx/object.h"
#include "ai.h"

void CheckCollisionWithObject(CELL_OBJECT *cop, VECTOR4 *pos, CAR_DATA *cp, int id, MODEL *model, int merge);

// the boxes are printed with this on (if (0) in the binary)
#define DEBUG_BBOX				0

#define MAX_BBOXES				60

//------------------------------------------------------------------------------------------
// uninitialised data

// the boxes of the models around the car, merged before the collision response
static BUILDING_BOX pbbox[MAX_BBOXES];
static CELL_OBJECT *pcop[MAX_BBOXES];
static int bboxcount;

//------------------------------------------------------------------------------------------

// [0x5021b0] [D]
void CheckCollisionWithEvents(VECTOR4 *pos, CAR_DATA *cp, int id)
{
	EVENT *ev;
	EVENT_MODEL *em;
	CELL_OBJECT cop;

	for (ev = FirstEvent; ev; ev = ev->next)
	{
		if (!(ev->flags & 2))
			continue;

		for (em = ev->model; em; em = em->next)
		{
			if (em->flags & 0x200)
				continue;

			cop.type = (unsigned short)em->model_id;
			cop.pos = em->position;
			cop.yang = em->rotation.vy;

			CheckCollisionWithObject(&cop, pos, cp, id, event_model_list[(unsigned short)em->model_id], 0);
		}
	}
}

// [0x502274] [D]
void CheckCollisionWithChopper(VECTOR4 *pos, CAR_DATA *cp, int id)
{
	CELL_OBJECT cop;

	cop.type = ChopperBodyNum;
	cop.pos = chopper.position;
	cop.yang = -chopper.angle.vy & 0xFFF;

	CheckCollisionWithObject(&cop, pos, cp, id, ChopperBody, 0);
}

// [0x5022db] [D]
int MergeAllBoxes(int a, int b, int id)
{
	BUILDING_BOX *bb0, *bb1;
	int carx, carz;
	int sign, dist, t;
	int xmin, xmax, ymin, ymax, zmin, zmax;

	if (id && id < 65)
		return 0;

	bb0 = &pbbox[a];
	bb1 = &pbbox[b];

	// axis aligned boxes only
	if ((bb0->theta & 0x3FF) || (bb1->theta & 0x3FF))
		return 0;

	if (abs(car_data[id].where.t[1] - bb0->pos.vy) > bb0->height + 125)
		return 0;

	if (abs(car_data[id].where.t[1] - bb1->pos.vy) > bb1->height + 125)
		return 0;

	if (!(bb0->theta & 0x400))
	{
		t = bb0->xsize;
		bb0->xsize = bb0->zsize;
		bb0->zsize = t;
		bb0->theta = 1024;
	}

	if (!(bb1->theta & 0x400))
	{
		t = bb1->xsize;
		bb1->xsize = bb1->zsize;
		bb1->zsize = t;
		bb1->theta = 1024;
	}

	// the faces towards the car in line, along x or else along z
	carx = car_data[id].where.t[0];
	sign = (carx - bb0->pos.vx <= 0) ? -1 : 1;

	if (sign != ((carx - bb1->pos.vx <= 0) ? -1 : 1)
		|| (dist = bb0->xsize * sign + bb0->pos.vx - bb1->pos.vx - bb1->xsize * sign) >= 50
		|| dist <= -50
		|| abs(bb0->pos.vz - bb1->pos.vz) >= bb0->zsize + bb1->zsize + 50)
	{
		carz = car_data[id].where.t[2];
		sign = (carz - bb0->pos.vz <= 0) ? -1 : 1;

		if (sign != ((carz - bb1->pos.vz <= 0) ? -1 : 1)
			|| (dist = bb0->zsize * sign + bb0->pos.vz - bb1->pos.vz - bb1->zsize * sign) >= 50
			|| dist <= -50
			|| abs(bb0->pos.vx - bb1->pos.vx) >= bb0->xsize + bb1->xsize + 50)
		{
			return 0;
		}
	}

	// bb0 becomes the box around both
	if (bb0->xsize + bb0->pos.vx <= bb1->xsize + bb1->pos.vx)
		xmax = bb1->xsize + bb1->pos.vx;
	else
		xmax = bb0->xsize + bb0->pos.vx;

	if (bb0->zsize + bb0->pos.vz <= bb1->zsize + bb1->pos.vz)
		zmax = bb1->zsize + bb1->pos.vz;
	else
		zmax = bb0->zsize + bb0->pos.vz;

	if (bb0->height / 2 + bb0->pos.vy <= bb1->height / 2 + bb1->pos.vy)
		ymax = bb1->height / 2 + bb1->pos.vy;
	else
		ymax = bb0->height / 2 + bb0->pos.vy;

	if (bb0->pos.vx - bb0->xsize >= bb1->pos.vx - bb1->xsize)
		xmin = bb1->pos.vx - bb1->xsize;
	else
		xmin = bb0->pos.vx - bb0->xsize;

	if (bb0->pos.vz - bb0->zsize >= bb1->pos.vz - bb1->zsize)
		zmin = bb1->pos.vz - bb1->zsize;
	else
		zmin = bb0->pos.vz - bb0->zsize;

	if (bb0->pos.vy - bb0->height / 2 >= bb1->pos.vy - bb1->height / 2)
		ymin = bb1->pos.vy - bb1->height / 2;
	else
		ymin = bb0->pos.vy - bb0->height / 2;

	bb0->pos.vx = (xmin + xmax) >> 1;
	bb0->pos.vy = (ymin + ymax) >> 1;
	bb0->pos.vz = (zmin + zmax) >> 1;

	bb0->xsize = (xmax - xmin) >> 1;
	bb0->zsize = (zmax - zmin) >> 1;

	bb0->height = abs(ymax - ymin);

	bb1->xsize = 0;

	return 1;
}

// [0x502a57] [D]
void CheckCollisionWithObject(CELL_OBJECT *cop, VECTOR4 *pos, CAR_DATA *cp, int id, MODEL *model, int merge)
{
	COLLISION_PACKET *collide;
	BUILDING_BOX box;
	MATRIX *m;
	float x, z, rx, rz;
	int radius, dx, dz;
	int num_cb;
	int i;

	if (!model->collision_block)
		return;

	radius = float_to_int(model->bounding_sphere2d) + cp->speed + 1160;

	dx = (int)(float)fabs(cop->pos.vx - pos->vx);
	dz = (int)(float)fabs(cop->pos.vz - pos->vz);

	if (radius * radius <= dz * dz + dx * dx)
		return;

	// NOTE: not initialised in the binary; a packet of another type than a box or a cylinder
	// uses the box of the packet before it
	memset(&box, 0, sizeof(box));

	collide = (COLLISION_PACKET *)(model->collision_block + 4);
	num_cb = *(int *)model->collision_block;

	for (i = 0; i < num_cb; i++, collide++)
	{
		if (collide->type == COLLISION_BOX)
		{
			m = &(*matrix_mem)[-cop->yang & 0xFFF];

			x = (float)collide->xpos;
			z = (float)collide->zpos;

			rx = x * m->m[0][0] + z * m->m[2][0];
			rz = x * m->m[0][2] + z * m->m[2][2];

			box.pos.vx = float_to_int(rx + cop->pos.vx);
			box.pos.vy = float_to_int(-((float)collide->ypos + cop->pos.vy));
			box.pos.vz = float_to_int(rz + cop->pos.vz);

			box.xsize = collide->zsize / 2;
			box.zsize = collide->xsize / 2;
			box.theta = (collide->yang + cop->yang) & 0xFFF;
			box.height = collide->ysize / 2;

			gLastModelCollisionCheck = cop->type;
		}
		else if (collide->type == COLLISION_CYLINDER)
		{
			m = &(*matrix_mem)[-cop->yang & 0xFFF];

			x = (float)collide->xpos;
			z = (float)collide->zpos;

			rx = x * m->m[0][0] + z * m->m[2][0];
			rz = x * m->m[0][2] + z * m->m[2][2];

			box.pos.vx = float_to_int(rx + cop->pos.vx);
			box.pos.vy = float_to_int(-((float)collide->ypos + cop->pos.vy));
			box.pos.vz = float_to_int(rz + cop->pos.vz);

			box.xsize = collide->xsize / 2;
			box.zsize = collide->xsize / 2;
			box.theta = cop->yang & 0xFFF;
			box.height = collide->ysize / 2;

			gLastModelCollisionCheck = cop->type;
		}

		if (!merge || bboxcount >= MAX_BBOXES || (model_pointers[cop->type]->flags2 & MODEL_FLAG_SMASHABLE))
		{
			if (CarBuildingCollision(cp, &box, cop))
				QueueDentCar(id - 1);
		}
		else
		{
			pbbox[bboxcount] = box;
			pcop[bboxcount++] = cop;
		}
	}
}

// [0x502f3d] [D]
void CheckScenaryCollisions(int id)
{
	CAR_DATA *cp;
	VECTOR4 pos;
	CELL_DATA *cdp;
	CELL_OBJECT *cop;
	float dx, dz;
	int cellx, cellz;
	int barrel;
	int index;
	int cell;
	int count;
	int i, j;
	char text[200];

	if (ghost_mode)
		return;

	cp = &car_data[id];

	pos.vx = (float)cp->location.vx;
	pos.vy = (float)cp->location.vy;
	pos.vz = (float)cp->location.vz;

	// replayed cars far from the camera don't collide
	if (gMissionReplaying && id >= 20)
	{
		dx = (float)fabs(pos.vx - camera_position.vx);
		dz = (float)fabs(pos.vz - camera_position.vz);

		if (dx * dx + dz * dz > 2025000000.0f)
			return;
	}

	cellx = (int)((pos.vx + (float)units_across_halved) * cell_recip);
	cellz = (int)((pos.vz + (float)units_down_halved) * cell_recip);

	barrel = ((cellx / region_size) & 1) + 2 * ((cellz / region_size) & 1);
	index = region_size * (cellz % region_size) + cellx % region_size + region_size * region_size * barrel;

	if (index < 0 || index >= 4 * region_size * region_size)
		return;

	cell = cell_ptrs[index];

	bboxcount = 0;

	CheckCollisionWithEvents(&pos, cp, id);

	if (gDoChopper)
		CheckCollisionWithChopper(&pos, cp, id);

	while (cell != -1)
	{
		cdp = &cells[cell];
		cop = &cell_objects[cdp->num];

		CheckCollisionWithObject(cop, &pos, cp, id, model_pointers[cop->type], 1);

		cell = cdp->next;
	}

	for (i = 0; i < bboxcount; i++)
	{
		if (DEBUG_BBOX)
		{
			TextOffset(30, i + 10);
			sprintf(text, "bbox[%d]={%5d,%5d,%5d, %5d, %4d,%4d}", i, pbbox[i].pos.vx, pbbox[i].pos.vy, pbbox[i].pos.vz,
				pbbox[i].theta, pbbox[i].xsize, pbbox[i].zsize);
			AddTextToList(text);
		}

		if (!pbbox[i].xsize)
			continue;

		for (j = i + 1; j < bboxcount; j++)
		{
			if (pbbox[j].xsize && MergeAllBoxes(j, i, id))
				break;
		}
	}

	if (DEBUG_BBOX)
	{
		TextOffset(2, 10);
		sprintf(text, "bboxcount: %d", bboxcount);
		AddTextToList(text);

		count = 0;

		for (i = 0; i < bboxcount; i++)
			count += pbbox[i].xsize != 0;

		TextOffset(2, 12);
		sprintf(text, "bboxcount2: %d", count);
		AddTextToList(text);
	}

	for (i = 0; i < bboxcount; i++)
	{
		if (!pbbox[i].xsize)
			continue;

		gLastModelCollisionCheck = pcop[i]->type;

		if (CarBuildingCollision(cp, &pbbox[i], pcop[i]))
			QueueDentCar(id - 1);
	}
}

// [0x5033c3] [D]
void DoScenaryCollisions(void)
{
	CheckScenaryCollisions(0);

	if (gMissionRecording || gMissionReplaying)
		CheckScenaryCollisions(65);

	if (!gDoTraining)
		CheckCivilianScenaryCollisions();
}
