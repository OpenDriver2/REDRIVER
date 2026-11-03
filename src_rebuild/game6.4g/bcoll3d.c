/*
 * bcoll3d.c - car to car collision test of two oriented boxes
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "bcoll3d.c", 0x49a4c0 - 0x49ab2e).
 *
 * Each car is a box (TestBoundBox) from its matrix and the collision box of its cosmetics.
 * collided3d tests each face of one box against the deepest corner of the other (separating
 * axes of the six faces only) and keeps the least penetration.
 */

#include "dr_types.h"
#include "bcoll3d.h"
#include "dr_globals.h"
#include "handling.h"

// a car's box: the axes (x, z, y rows of the matrix), the centre, the half sizes in
// bounds[0][1..3] and the axes scaled by them in bounds[1..3]
typedef struct TestBoundBox
{
	int where[3][4];
	int trans[3];
	int bounds[4][4];
} TestBoundBox;

typedef struct TestResult
{
	int depth;
	IVECTOR location;
	IVECTOR normal;
	int field_24;
} TestResult;

void PointFaceCheck(TestBoundBox *bb0, TestBoundBox *bb1, int i, TestResult *least, int nSign);

// [0x49a4c0] [D]
int collided3d(TestBoundBox *cp0, TestBoundBox *cp1, TestResult *least)
{
	int i;

	least->depth = 0x40000000;

	for (i = 0; i < 3; i++)
	{
		cp0->bounds[i + 1][0] = (cp0->bounds[0][i + 1] * cp0->where[i][0] + 2048) >> 12;
		cp0->bounds[i + 1][1] = (cp0->bounds[0][i + 1] * cp0->where[i][1] + 2048) >> 12;
		cp0->bounds[i + 1][2] = (cp0->bounds[0][i + 1] * cp0->where[i][2] + 2048) >> 12;

		cp1->bounds[i + 1][0] = (cp1->bounds[0][i + 1] * cp1->where[i][0] + 2048) >> 12;
		cp1->bounds[i + 1][1] = (cp1->bounds[0][i + 1] * cp1->where[i][1] + 2048) >> 12;
		cp1->bounds[i + 1][2] = (cp1->bounds[0][i + 1] * cp1->where[i][2] + 2048) >> 12;
	}

	for (i = 0; i < 3; i++)
	{
		PointFaceCheck(cp0, cp1, i, least, 1);

		if (least->depth < 0)
			return 0;

		PointFaceCheck(cp1, cp0, i, least, -1);

		if (least->depth < 0)
			return 0;
	}

	return 1;
}

// [0x49a671] [D]
void PointFaceCheck(TestBoundBox *bb0, TestBoundBox *bb1, int i, TestResult *least, int nSign)
{
	int partialDepth;
	int normal_vx, normal_vy, normal_vz;
	int point_vx, point_vy, point_vz;
	int k;
	int side;
	int depth;

	normal_vx = bb0->where[i][0];
	normal_vy = bb0->where[i][1];
	normal_vz = bb0->where[i][2];

	point_vx = bb1->trans[0];
	point_vy = bb1->trans[1];
	point_vz = bb1->trans[2];

	depth = (normal_vy * (point_vy - bb0->trans[1])
		+ normal_vx * (point_vx - bb0->trans[0])
		+ normal_vz * (point_vz - bb0->trans[2])
		+ 2048) >> 12;

	if (depth >= 0)
	{
		depth = -depth;
	}
	else
	{
		normal_vx = -normal_vx;
		normal_vy = -normal_vy;
		normal_vz = -normal_vz;
	}

	depth += bb0->bounds[0][i + 1];

	for (k = 0; k < 3; k++)
	{
		partialDepth = (normal_vy * bb1->bounds[k + 1][1]
			+ normal_vx * bb1->bounds[k + 1][0]
			+ normal_vz * bb1->bounds[k + 1][2]
			+ 2048) >> 12;

		if (partialDepth >= 0)
		{
			depth += partialDepth;

			point_vx -= bb1->bounds[k + 1][0];
			point_vy -= bb1->bounds[k + 1][1];
			point_vz -= bb1->bounds[k + 1][2];

			if (k == 2)
				side = 0;
		}
		else
		{
			depth -= partialDepth;

			point_vx += bb1->bounds[k + 1][0];
			point_vy += bb1->bounds[k + 1][1];
			point_vz += bb1->bounds[k + 1][2];

			if (k == 2)
				side = 1;
		}
	}

	if (depth < least->depth)
	{
		least->depth = depth;

		if (depth >= 0)
		{
			least->location.vx = point_vx;
			least->location.vy = point_vy;
			least->location.vz = point_vz;

			if (nSign < 0)
			{
				normal_vx = -normal_vx;
				normal_vy = -normal_vy;
				normal_vz = -normal_vz;
			}

			least->normal.vx = normal_vx;
			least->normal.vy = normal_vy;
			least->normal.vz = normal_vz;

			least->field_24 = side;
		}
	}
}

// [0x49a8d2] [D]
int CarCarCollision3(CAR_DATA *c0, CAR_DATA *c1, int *depth, IVECTOR *where, IVECTOR *normal, IVECTOR *offset)
{
	TestBoundBox boxes[2];
	TestResult tr;
	TestBoundBox *bbox;
	CAR_DATA *cp0, *cp1;
	int i;
	int collided;

	bbox = &boxes[1];
	cp0 = c0;
	cp1 = c1;

	for (i = 0; i < 2; i++)
	{
		bbox->where[0][0] = cp0->where.m[0][0];
		bbox->where[0][1] = cp0->where.m[1][0];
		bbox->where[0][2] = cp0->where.m[2][0];

		bbox->where[2][0] = cp0->where.m[0][1];
		bbox->where[2][1] = cp0->where.m[1][1];
		bbox->where[2][2] = cp0->where.m[2][1];

		bbox->where[1][0] = cp0->where.m[0][2];
		bbox->where[1][1] = cp0->where.m[1][2];
		bbox->where[1][2] = cp0->where.m[2][2];

		bbox->trans[0] = cp0->where.t[0];
		bbox->trans[1] = cp0->where.t[1];
		bbox->trans[2] = cp0->where.t[2];

		if (cp0->fourWheelDrive == 1 && cp1->id != 0 && cp1->fourWheelDrive != 1)
		{
			// pursuer hits civilian
			bbox->bounds[0][1] = 7 * car_cosmetics[cp0->model].colBox.vx / 8;
			bbox->bounds[0][2] = 9 * car_cosmetics[cp0->model].colBox.vz / 10;
		}
		else
		{
			bbox->bounds[0][1] = car_cosmetics[cp0->model].colBox.vx;
			bbox->bounds[0][2] = car_cosmetics[cp0->model].colBox.vz;
		}

		bbox->bounds[0][3] = car_cosmetics[cp0->model].colBox.vy;

		bbox = &boxes[0];
		cp0 = c1;
		cp1 = c0;
	}

	collided = collided3d(&boxes[1], &boxes[0], &tr);

	if (collided)
	{
		*depth = tr.depth;

		where->vx = tr.location.vx;
		where->vy = tr.location.vy;
		where->vz = tr.location.vz;

		normal->vx = tr.normal.vx;
		normal->vy = tr.normal.vy;
		normal->vz = tr.normal.vz;

		if (tr.field_24)
		{
			offset->vx = boxes[1].bounds[3][0];
			offset->vy = boxes[1].bounds[3][1];
			offset->vz = boxes[1].bounds[3][2];
		}
		else
		{
			offset->vz = 0;
			offset->vy = 0;
			offset->vx = 0;
		}
	}

	return collided;
}
