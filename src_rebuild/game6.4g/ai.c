/*
 * ai.c - civilian traffic: spawning cars around the player (pings), driving them along the road
 * map lanes, junctions and traffic lights, parking, roadblocks
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "ai.c", 0x48e1e0 - 0x497db5).
 *
 * The civilian cars are car_data[1..64] (civ_ai_data_pt = &car_data[1], so civ_ai_data_pt[i] is
 * the car of civilian_ai_data[i]); car_data[65] is the replayed lead car. Every frame ControlAI
 * runs the state function of each car (CivilianStateFunction), steps its physics, removes cars
 * that got too far from the player and pings new ones into the road map cells around him
 * (GetPingPoints, ControlPings). A moving car follows its lane (currentRoad, currentLane: the
 * direction 0 - 3) to the next junction (ROADS_GetNextJunction fills RouteData), picks the road
 * to take there (SelectNextRoad), waits for traffic and lights (ControlSpeed) and turns into it
 * (CivilianState_TURNING). Cop cars use the same slots with the pursuit state from cops.c.
 *
 * The strings "North" .. "West" and dirs_old in the data block have no xrefs (left over from
 * code the linker dropped).
 */

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dr_types.h"
#include "ai.h"
#include "dr_globals.h"
#include "D3DGlide/glide_ptrs.h"
#include "asmmath.h"
#include "dr_cars.h"
#include "chairs.h"
#include "cops.h"
#include "debris.h"
#include "denting.h"
#include "felony.h"
#include "general.h"
#include "gsound.h"
#include "handling.h"
#include "map.h"
#include "MScript.h"
#include "objcoll.h"
#include "pedest.h"
#include "replays.h"
#include "roadbits.h"
#include "shell.h"
#include "sound.h"
#include "testmath.h"
#include "Engine/3dfx/gfxinit.h"
#include "flat/prescam.h"

typedef void (*CIV_STATE_CALLBACK)(CIV_STATE *ai);

/* replays.c */
extern int dword_CCC8A0;
extern int dword_CCCC34;

void CivilianState_EMPTY(CIV_STATE *ai);
void CivilianState_PARKED(CIV_STATE *ai);
void CivilianState_MOVING(CIV_STATE *ai);
void CivilianState_TURNING(CIV_STATE *ai);
void CivilianState_SWERVING_LEFT(CIV_STATE *ai);
void CivilianState_SWERVING_RIGHT(CIV_STATE *ai);
void CivilianState_LEADING(CIV_STATE *ai);
void CivilianState_LEADREC(CIV_STATE *ai);
void CivilianState_BLOCKING(CIV_STATE *ai);
int FindCarAhead(CIV_STATE *ai, int minDist, int type);
int SetWheelAngle(CIV_STATE *ai, int direction);
void ActivateCop(int id);
int JunctionIsSafeToEnter(int junction, int civ_id);
int CarsOnRoads(int road1, int road2, int road3, int road4);
int ControlMaxSpeed(CIV_STATE *ai);
int SelectNextRoad(CIV_STATE *ai);
int SlowCarInWayCheck(CIV_STATE *ai);
void DrawCarExtras(int id, CIV_STATE *ai);
void GetTargetLaneCoords(int road, int node, int direction, int fixed, unsigned char offset);
char GetLaneOffset(CIV_STATE *ai);
void InitCopState(CAR_DATA *cp, COP *cop, int id);
int CarsParkedOnRoad(int road);
void RemoveCivilianCar(int id);
int AddCivilianCar(void);
int AllocateAndPlaceCarAtPosition(int x, int y, int z, int dir, int blocking);
void TryParkingCar(CIV_STATE *ai);
void TryUnparkingCar(CIV_STATE *ai);
void TryChangingLanes(CIV_STATE *ai);
int SetupRoadBlock(void);
int IsRoadBlockPossible(int x, int z, int road, char dir);
void ResetRoadBlock(void);
void RemoveRoadBlock(void);
int PlayerCloseToCar(CAR_DATA *cp);
void BuildPreRotatedBoxes(void);
void GetPingPoints(void);
void ControlPings(int *densityControl, int numDensityControl);
void AddCarRelativeToPlayer(void);
int OccupancyGridNotBlocked(int x, int z);

//------------------------------------------------------------------------------------------
// initialised data (0x553530)

// NOTE: IDA types ping_areas as one int; the 20 bytes before max_traffic_density are one density
// control area for ControlPings (x1, z1, x2, z2, density), which no longer reads it
int ping_areas[5] = { -379000, 236000, -232000, 318000, 5 };
int max_traffic_density = 64;
int lane_max_speeds[4] = { 130, 162, 194, 226 };
int brake_distances[4] = { 3500, 3600, 3700, 3800 };
char *dir_names[4] = { "North", "East", "South", "West" };	// NOTE: no xrefs

CIV_STATE_CALLBACK CivilianStateFunction[10] = {
	CivilianState_EMPTY,
	CivilianState_PARKED,
	CivilianState_MOVING,
	CivilianState_TURNING,
	CopState_PURSUIT,
	CivilianState_SWERVING_LEFT,
	CivilianState_SWERVING_RIGHT,
	CivilianState_LEADING,
	CivilianState_LEADREC,
	CivilianState_BLOCKING,
};

static int dirs_old[4] = { 0, 200, 400, 700 };	// NOTE: no xrefs

//------------------------------------------------------------------------------------------
// uninitialised data
// NOTE: in the common area (0xcb8f60, 0x11b66b4 - 0x11c4d28) without IDA marks; defined here as
// this file is their only or main user

ROAD_LANE_INFO RoadLaneInfo[2048];	// NOTE: owner guessed (most references)
int CarsNeededForBlock;
int ExtraBlockCars;
int ReturnedY;
int ReturnedX;
int SpareCarsUsed;
int LastCarsNeeded;
int NextRoadCars[4];
PING_POINT road_ping_points[240];
int failed_pings;
int ApproachDistance[64];
int CarsNeededInitially;
int pre_rotated_boxes[1024][4][2];	// NOTE: owner guessed (most references)
char DirectionToBlock;
int succesful_pings;
int CloseCarMinGap;
int CloseCarGap;
int RoadToBlock;
int cutout_offsets[240][4];
int num_civilian_ai_cars;
int ApproachCars[64];
int MaxCloseCars;
PING_POINT path_ping_points[240];	// NOTE: owner guessed (most references)
int num_path_ping_points;
int CarsSavedForBlock;
int num_ai_cars_active;
int RequestCopCar;					// NOTE: owner guessed (most references)
int CarApproaching;
int collision_frame;
int BlockerShots[32];				// NOTE: IDA types it int[19], but the item is 128 bytes (InitialiseAI clears 0x80)
int AdjacentRoad;
short PlayerLastRoad;				// NOTE: owner guessed (most references)
int ClosestCar;
int num_road_ping_points;
int num_ai_cars_inactive;
int RoadBlockTimer;
int civilian_slots[65];
int ParkedCarMinGap;
int num_cutout_pieces;

// [0x48e1e0] [D]
// NOTE: no name in the database and no callers (only an unreferenced thunk): a debug view that
// draws a magenta triangle on the road at pos pointing along the lane direction of sub_51CAD0.
void sub_48E1E0(VECTOR4 *pos)
{
	VECTOR4 v1;
	VECTOR4 v2;
	GrVertex p1;
	GrVertex p2;
	GrVertex p3;
	float x1, y1, z1;
	float x2, y2, z2;
	float x3, y3, z3;
	float scale;
	int dir;

	if (last_state != FLAT3)
	{
		grColorCombine(GR_COMBINE_FUNCTION_LOCAL, GR_COMBINE_FACTOR_ZERO, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_CONSTANT, FXFALSE);
		last_state = FLAT3;
	}

	if (last_trans_on)
	{
		grAlphaBlendFunction(GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_ONE, GR_BLEND_ZERO);
		last_trans_on = 0;
	}

	dir = sub_51CAD0(pos->vx, pos->vz);

	if (dir == -1)
		return;

	y1 = -5.0f;

	switch (dir)
	{
		case 0:
			x1 = 0.0f;
			z1 = 750.0f;
			x2 = 300.0f;
			z2 = -750.0f;
			x3 = -300.0f;
			z3 = -750.0f;
			break;
		case 1:
			x1 = 750.0f;
			z1 = 0.0f;
			x2 = -750.0f;
			z2 = -300.0f;
			x3 = -750.0f;
			z3 = 300.0f;
			break;
		case 2:
			x1 = 0.0f;
			z1 = -750.0f;
			x2 = 300.0f;
			z2 = 750.0f;
			x3 = -300.0f;
			z3 = 750.0f;
			break;
		case 3:
			x1 = -750.0f;
			z1 = 0.0f;
			x2 = 750.0f;
			z2 = -300.0f;
			x3 = 750.0f;
			z3 = 300.0f;
			break;
	}

	p1.r = 255.0f;
	p1.g = 0.0f;
	p1.b = 255.0f;

	p2.r = 255.0f;
	p2.g = 0.0f;
	p2.b = 255.0f;

	p3.r = 255.0f;
	p3.g = 0.0f;
	p3.b = 255.0f;

	v1.vx = pos->vx - camera_position.vx;
	v1.vy = pos->vy - camera_position.vy;
	v1.vz = pos->vz - camera_position.vz;

	MatrixRotate(&v1);

	v2.vx = x1;
	v2.vy = y1;
	v2.vz = z1;

	MatrixRotate(&v2);

	x1 = v2.vx;
	y1 = v2.vy;
	z1 = v2.vz;

	v2.vx = x2;
	v2.vy = -5.0f;
	v2.vz = z2;

	MatrixRotate(&v2);

	x2 = v2.vx;
	y2 = v2.vy;
	z2 = v2.vz;

	v2.vx = x3;
	v2.vy = -5.0f;
	v2.vz = z3;

	MatrixRotate(&v2);

	x1 = x1 + v1.vx;
	x2 = x2 + v1.vx;
	x3 = v2.vx + v1.vx;

	y1 = y1 + v1.vy;
	y2 = y2 + v1.vy;
	y3 = v2.vy + v1.vy;

	z1 = z1 + v1.vz;
	z2 = z2 + v1.vz;
	z3 = v2.vz + v1.vz;

	if (z1 >= 0.0f && z2 >= 0.0f && z3 >= 0.0f)
	{
		p1.oow = ooz_table[(int)z1];
		scale = p1.oow * DIST;
		p1.x = x1 * scale + screen_xmid - VERTEX_SNAPPER;
		p1.y = y1 * scale + screen_ymid - VERTEX_SNAPPER;

		p2.oow = ooz_table[(int)z2];
		scale = p2.oow * DIST;
		p2.x = x2 * scale + screen_xmid - VERTEX_SNAPPER;
		p2.y = y2 * scale + screen_ymid - VERTEX_SNAPPER;

		p3.oow = ooz_table[(int)z3];
		scale = p3.oow * DIST;
		p3.x = x3 * scale + screen_xmid - VERTEX_SNAPPER;
		p3.y = y3 * scale + screen_ymid - VERTEX_SNAPPER;

		guDrawTriangleWithClip(&p1, &p2, &p3);
		last_state = -1;
	}
}

// [0x48e865] [D]
// Clears memory a dword at a time, then the remaining bytes.
void ClearMem(void *mem, size_t size)
{
	char *ptr;

	ptr = (char *)mem;

	if (size >> 2)
	{
		memset(mem, 0, (size >> 2) * 4);
		ptr = (char *)mem + (size >> 2) * 4;
	}

	if (size & 3)
		memset(ptr, 0, size & 3);
}

// [0x48e88e] [D]
// The corners of a car's box (360 x 900) rotated for every fourth direction: pre_rotated_boxes
// [dir / 4][corner] = { x, z }.
void BuildPreRotatedBoxes(void)
{
	int (*box)[2];
	float sin;
	float cos;
	int i;

	box = pre_rotated_boxes[0];

	for (i = 0; i < 1024; i++)
	{
		sin = fsintab[4 * i];
		cos = fsintab[(4 * i + 1024) & 0xFFF];

		(*box)[0] = float_to_int(-180.0f * cos - 450.0f * sin);
		(*box)[1] = float_to_int(-180.0f * sin + 450.0f * cos);
		box++;

		(*box)[0] = float_to_int(180.0f * cos - 450.0f * sin);
		(*box)[1] = float_to_int(180.0f * sin + 450.0f * cos);
		box++;

		(*box)[0] = float_to_int(-180.0f * cos - -450.0f * sin);
		(*box)[1] = float_to_int(-180.0f * sin + -450.0f * cos);
		box++;

		(*box)[0] = float_to_int(180.0f * cos - -450.0f * sin);
		(*box)[1] = float_to_int(180.0f * sin + -450.0f * cos);
		box++;
	}
}

// [A]
// One corner of the box (x, z) on the inner side of the plane (vx, vz, vw): vx * x + vz * z <= vw.
static int AnyCornerInside(VECTOR4 *plane, float *x, float *z)
{
	int i;

	for (i = 0; i < 4; i++)
	{
		if (plane->vx * x[i] + plane->vz * z[i] <= plane->vw)
			return 1;
	}

	return 0;
}

// [0x48eadb] [D] [A]
// A car (or the player) in the area ahead of the car: a box along the lane when the car drives
// straight north, east, south or west, else a rotated box (always while turning) tested against
// each car's pre-rotated corners.
// NOTE: the four plane tests of the rotated box are written with the helper AnyCornerInside.
int SlowCarInWayCheck(CIV_STATE *ai)
{
	VECTOR4 vA;
	VECTOR4 vB;
	VECTOR4 vC;
	VECTOR4 vD;
	float cx[4];
	float cz[4];
	float cos;
	float sin;
	float neg_cos;
	float neg_sin;
	float fx;
	float fz;
	float px;
	float pz;
	int (*box)[4][2];
	CIV_STATE *other_ai;
	CAR_DATA *cp;
	int x0, x1, x2, x3;
	int z0, z1, z2, z3;
	int xlower;
	int xupper;
	int zlower;
	int zupper;
	int xsize;
	int zsize;
	int oriented;
	int straight;
	int civ_id;
	int slot;
	int dir;
	int odir;
	int vx;
	int vz;
	int tx;
	int tz;
	int k;
	unsigned char currentLane;

	oriented = 0;
	straight = 0;
	currentLane = ai->currentLane;
	civ_id = ai->id;

	if (ai->ctrlState == CIVSTATE_TURNING)
	{
		xsize = 180;
		zsize = ai->speed + 900;
	}
	else
	{
		xsize = 450;
		zsize = 16 * ai->speed + 1800;
	}

	dir = -civ_ai_data_pt[civ_id].direction & 0xFFF;

	vx = civ_ai_data_pt[civ_id].location.vx;
	vz = civ_ai_data_pt[civ_id].location.vz;

	if (ai->ctrlState != CIVSTATE_TURNING)
	{
		straight = 1;

		if (FindCarAhead(ai, 1, 0) != -1)
			return 1;

		if (dir > 4064 || dir < 32)
		{
			xlower = vx - xsize;
			xupper = vx + xsize;
			zlower = vz + zsize;
			zupper = vz;
		}
		else if (dir > 2016 && dir < 2080)
		{
			xlower = vx - xsize;
			xupper = vx + xsize;
			zlower = vz;
			zupper = vz - zsize;
		}
		else if (dir > 3040 && dir < 3104)
		{
			xlower = vx;
			xupper = vx + zsize;
			zlower = vz + xsize;
			zupper = vz - xsize;
		}
		else if (dir > 992 && dir < 1056)
		{
			xlower = vx - zsize;
			xupper = vx;
			zlower = vz + xsize;
			zupper = vz - xsize;
		}
		else
		{
			straight = 0;
		}
	}

	// slot 0 is the player, slot n the civilian n - 1
	for (slot = 0; slot < gTrafficDensity + 1; slot++)
	{
		cp = &car_data[slot];
		other_ai = slot ? &civilian_ai_data[slot - 1] : NULL;

		if (slot && (other_ai->ctrlState == CIVSTATE_EMPTY || other_ai->id == civ_id))
			continue;

		tx = cp->location.vx;
		tz = cp->location.vz;

		if (abs(tx - vx) >= zsize || abs(tz - vz) >= zsize)
			continue;

		odir = -cp->direction & 0xFFF;
		box = (int (*)[4][2])pre_rotated_boxes[odir / 4];

		if (straight)
		{
			x0 = tx + (*box)[0][0];
			x1 = tx + (*box)[1][0];
			x2 = tx + (*box)[2][0];
			x3 = tx + (*box)[3][0];

			if ((x0 >= xlower || x1 >= xlower || x2 >= xlower || x3 >= xlower)
				&& (x0 <= xupper || x1 <= xupper || x2 <= xupper || x3 <= xupper))
			{
				z0 = tz + (*box)[0][1];
				z1 = tz + (*box)[1][1];
				z2 = tz + (*box)[2][1];
				z3 = tz + (*box)[3][1];

				if ((z0 <= zlower || z1 <= zlower || z2 <= zlower || z3 <= zlower)
					&& (z0 >= zupper || z1 >= zupper || z2 >= zupper || z3 >= zupper)
					&& (slot == 0
						|| other_ai->wrecked == 1
						|| (other_ai->ctrlState == CIVSTATE_TURNING && other_ai->turnDir)
						|| other_ai->currentLane != currentLane
						|| other_ai->ctrlState != CIVSTATE_PARKED))
				{
					return 1;
				}
			}
		}
		else
		{
			if (!oriented)
			{
				oriented = 1;

				cos = fsintab[(dir + 1024) & 0xFFF];
				sin = fsintab[dir];

				neg_cos = -cos;
				neg_sin = -sin;

				fx = (float)vx;
				fz = (float)vz;

				px = (float)-xsize * cos + fx;
				pz = (float)-xsize * sin + fz;

				vA.vx = neg_cos;
				vA.vz = neg_sin;
				vA.vw = neg_cos * px + neg_sin * pz;

				px = (float)xsize * cos + fx;
				pz = (float)xsize * sin + fz;

				vD.vx = cos;
				vD.vz = sin;
				vD.vw = cos * px + sin * pz;

				px = (float)zsize * neg_sin + fx;
				pz = (float)zsize * cos + fz;

				vC.vx = neg_sin;
				vC.vz = cos;
				vC.vw = neg_sin * px + cos * pz;

				vB.vx = sin;
				vB.vz = neg_cos;
				vB.vw = sin * fx + neg_cos * fz;
			}

			for (k = 0; k < 4; k++)
			{
				cx[k] = (float)(tx + (*box)[k][0]);
				cz[k] = (float)(tz + (*box)[k][1]);
			}

			if (AnyCornerInside(&vA, cx, cz) && AnyCornerInside(&vD, cx, cz)
				&& AnyCornerInside(&vC, cx, cz) && AnyCornerInside(&vB, cx, cz))
			{
				return 1;
			}
		}
	}

	return 0;
}

// [0x48f72c] [D]
// Collects the cars driving towards the junction on a road lane (ApproachCars, ApproachDistance:
// the distance ahead) and those in the junction (the id with 0x80, distance -1).
int ApproachCarsOnPath(int civ_id, int targetRoad, int targetLane, int targetJunction)
{
	CIV_STATE *ai;
	int slot;
	int car;

	ai = civilian_ai_data;
	car = 0;

	for (slot = 0; slot < gTrafficDensity; slot++, ai++)
	{
		if (!ai->driving || ai->id == civ_id)
			continue;

		if (ai->currentRoad == targetRoad)
		{
			if (ai->currentLane == targetLane || ai->targetLane == targetLane)
			{
				ApproachDistance[car] = ai->distAhead;
				ApproachCars[car++] = ai->id;
			}
		}
		else if (ai->InJunction && ai->currentJunction == targetJunction)
		{
			ApproachDistance[car] = -1;
			ApproachCars[car++] = ai->id | 0x80;
		}
	}

	return car;
}

// [0x48f825] [D]
// Another car on the road lane, or in the junction heading for the road.
int PathBlockedByCar(int civ_id, int targetRoad, unsigned char targetLane, int targetJunction)
{
	CIV_STATE *ai;
	int slot;

	ai = civilian_ai_data;

	for (slot = 0; slot < gTrafficDensity; slot++, ai++)
	{
		if (!ai->driving || ai->id == civ_id)
			continue;

		if (ai->currentRoad == targetRoad)
		{
			if (ai->currentLane == targetLane)
				return 1;
		}
		else if (ai->InJunction && ai->currentJunction == targetJunction && ai->targetRoad == targetRoad)
		{
			return 1;
		}
	}

	return 0;
}

// [0x48f8c8] [D]
// Pedestrians react to the cops chasing the player closely.
void CheckCopsWithPedestrians(void)
{
	COP *cop;
	int loop;

	cop = cop_data;
	loop = 8;

	while (loop--)
	{
		if (cop->active && cop->close_pursuit && !cop->dying)
			CheckHornReaction(cop->pCarData);

		cop++;
	}
}

// [0x48f934] [D] [A]
// No other car in the junction.
// NOTE: the original never initialises slot (IDA: "USUALLY DISABLED" - the garbage on the stack
// usually ends the loop at once and the junction counts as safe); starts at 0 here, so the check
// runs over the civilians 1 .. gTrafficDensity - 1.
int JunctionIsSafeToEnter(int junction, int civ_id)
{
	CIV_STATE *other_ai;
	int slot;

	slot = 0;
	other_ai = civilian_ai_data;

	while (1)
	{
		slot++;
		other_ai++;

		if (slot >= gTrafficDensity)
			break;

		if (other_ai->driving && other_ai->id != civ_id && other_ai->InJunction && other_ai->currentJunction == junction)
			return 0;
	}

	return 1;
}

// [0x48f9a3] [D]
// A slow car near the start of the road lane.
int RoadLaneIsAlreadyOccupied(int civ_id, int targetRoad, unsigned char targetLane)
{
	CIV_STATE *ai;
	int distAlongSegment;
	int slot;

	ai = civilian_ai_data;

	if (civilian_ai_data[civ_id].turnDir)
		distAlongSegment = 6000;
	else
		distAlongSegment = 3000;

	for (slot = 0; slot < gTrafficDensity; slot++, ai++)
	{
		if (ai->ctrlState != CIVSTATE_EMPTY
			&& ai->ctrlState != CIVSTATE_PARKED
			&& ai->id != civ_id
			&& ai->currentRoad == targetRoad
			&& ai->currentLane == targetLane
			&& ai->distAlongSegment < distAlongSegment
			&& ai->speed < 30)
		{
			return 1;
		}
	}

	return 0;
}

// [0x48fa6f] [D]
int RoadNotSafeToEnter(CIV_STATE *ai, int checkJunction)
{
	if (checkJunction && !JunctionIsSafeToEnter(ai->currentJunction, ai->id))
		return 1;

	if (Roads[ai->targetRoad].Length < 4 && PathBlockedByCar(ai->id, ai->targetRoad, ai->targetLane, ai->currentJunction))
		return 1;

	return RoadLaneIsAlreadyOccupied(ai->id, ai->targetRoad, ai->targetLane);
}

// [0x48faff] [D]
// At a give way junction: no car coming on the roads to the left and right that would cross the
// car's path.
int SafeToEnterRoadGiveWay(CIV_STATE *ai)
{
	int ViaRoadA;
	int ViaRoadB;
	int prevLane;
	int nextLane;
	int civ_id;
	int junc;
	int num;
	int i;

	junc = ai->currentJunction;

	prevLane = (ai->currentLane - 1) & 3;
	nextLane = (ai->currentLane + 1) & 3;

	ViaRoadA = Junctions[junc].JunctionEntry[prevLane].ViaRoad;
	ViaRoadB = Junctions[junc].JunctionEntry[nextLane].ViaRoad;

	if (ai->turnDir == 0)
	{
		if (ViaRoadA != -1)
		{
			num = ApproachCarsOnPath(ai->id, ViaRoadA, nextLane, junc);

			for (i = 0; i < num; i++)
			{
				if (ApproachDistance[i] < 15000)
				{
					civ_id = ApproachCars[i];

					if (civ_id & 0x80)
						return 0;

					if (civilian_ai_data[civ_id].speed && civilian_ai_data[civ_id].targetRoad != ai->currentRoad)
						return 0;
				}
			}
		}

		if (ViaRoadB != -1)
		{
			num = ApproachCarsOnPath(ai->id, ViaRoadB, prevLane, junc);

			for (i = 0; i < num; i++)
			{
				// BUG: no check for ApproachCars with 0x80 flag!!!
				if (ApproachDistance[i] < 15000 && civilian_ai_data[ApproachCars[i]].speed)
					return 0;
			}
		}

		if (RoadNotSafeToEnter(ai, 1))
			return 0;
	}
	else if (ai->turnDir > 0)
	{
		if (ViaRoadA != -1)
		{
			num = ApproachCarsOnPath(ai->id, ViaRoadA, nextLane, junc);

			for (i = 0; i < num; i++)
			{
				if (ApproachDistance[i] < 15000)
				{
					civ_id = ApproachCars[i];

					if (civ_id & 0x80)
						return 0;

					// BUG: CarApproaching never set, always zero!
					if (civilian_ai_data[civ_id].targetRoad == ViaRoadB && civilian_ai_data[CarApproaching].speed)
						return 0;
				}
			}
		}

		if (RoadNotSafeToEnter(ai, 1))
			return 0;
	}
	else
	{
		if (ViaRoadA != -1)
		{
			num = ApproachCarsOnPath(ai->id, ViaRoadA, nextLane, junc);

			for (i = 0; i < num; i++)
			{
				if (ApproachDistance[i] < 15000)
				{
					civ_id = ApproachCars[i];

					if (civ_id & 0x80)
						return 0;

					if (civilian_ai_data[civ_id].speed && civilian_ai_data[civ_id].targetRoad != ai->currentRoad)
						return 0;
				}
			}
		}

		if (ViaRoadB != -1)
		{
			num = ApproachCarsOnPath(ai->id, ViaRoadB, prevLane, junc);

			for (i = 0; i < num; i++)
			{
				if (ApproachDistance[i] < 15000)
				{
					civ_id = ApproachCars[i];

					if (civ_id & 0x80)
						return 0;

					if (civilian_ai_data[civ_id].targetRoad != Junctions[junc].JunctionEntry[ai->currentLane].ViaRoad
						&& civilian_ai_data[civ_id].speed)
					{
						return 0;
					}
				}
			}
		}

		if (RoadNotSafeToEnter(ai, 1))
			return 0;
	}

	return 1;
}

// [0x48fec6] [D]
// At a traffic light junction: no oncoming car that goes straight on (or turns, closer than 9000).
int SafeToSpeedUp(CIV_STATE *ai)
{
	int targetJunction;
	int targetRoad;
	int num;
	int id;
	int i;

	targetJunction = ai->currentJunction;

	if (Junctions[targetJunction].JWH)
		return 1;

	targetRoad = Junctions[targetJunction].JunctionEntry[ai->currentLane].ViaRoad;

	if (targetRoad != -1)
	{
		num = ApproachCarsOnPath(ai->id, targetRoad, (ai->currentLane + 2) & 3, targetJunction);

		for (i = 0; i < num; i++)
		{
			if (ApproachDistance[i] < 15000)
			{
				id = ApproachCars[i];

				if (id & 0x80)
					return 0;

				if (civilian_ai_data[id].turnDir != -1)
				{
					if (civilian_ai_data[id].turnDir == 0)
						return 0;

					if (ApproachDistance[i] < 9000)
						return 0;
				}
			}
		}
	}

	return !RoadNotSafeToEnter(ai, 1);
}

// [0x48ffe7] [D]
int CarsOnRoad(int road)
{
	CIV_STATE *ai;
	int slot;
	int count;

	ai = civilian_ai_data;
	count = 0;

	for (slot = 0; slot < gTrafficDensity; slot++, ai++)
	{
		if (ai->ctrlState != CIVSTATE_EMPTY && ai->currentRoad == road)
			count++;
	}

	return count;
}

// [0x49004c] [D]
int CarsHeadingToJunction(int junction)
{
	return CarsOnRoads(
		Junctions[junction].JunctionEntry[0].ViaRoad,
		Junctions[junction].JunctionEntry[1].ViaRoad,
		Junctions[junction].JunctionEntry[2].ViaRoad,
		Junctions[junction].JunctionEntry[3].ViaRoad);
}

// [0x4900b4] [D]
// Counts the cars on each of four roads into NextRoadCars.
int CarsOnRoads(int road1, int road2, int road3, int road4)
{
	CIV_STATE *ai;
	int slot;

	ai = civilian_ai_data;

	NextRoadCars[3] = 0;
	NextRoadCars[2] = 0;
	NextRoadCars[1] = 0;
	NextRoadCars[0] = 0;

	for (slot = 0; slot < gTrafficDensity; slot++, ai++)
	{
		if (ai->ctrlState == CIVSTATE_EMPTY)
			continue;

		if (ai->currentRoad == road1)
			NextRoadCars[0]++;
		else if (ai->currentRoad == road2)
			NextRoadCars[1]++;
		else if (ai->currentRoad == road3)
			NextRoadCars[2]++;
		else if (ai->currentRoad == road4)
			NextRoadCars[3]++;
	}

	return NextRoadCars[3] + NextRoadCars[2] + NextRoadCars[1] + NextRoadCars[0];
}

// [0x4901ad] [D]
// The distance (road map cells, x + z) from (x, z) to the closest parked or wrecked car into
// ClosestCar (1024 if none).
void FindClosestParkedCar(int x, int z)
{
	CIV_STATE *ai;
	int max_civs;
	int slot;
	int dist;

	ai = civilian_ai_data;

	x += 750;
	z -= 750;

	x = IntegerApplyRecip(x);
	z = IntegerApplyRecip(z);

	if (gMissionReplaying || gMissionRecording)
		max_civs = 65;
	else
		max_civs = gTrafficDensity;

	ClosestCar = 1024;

	for (slot = 0; slot < max_civs; slot++, ai++)
	{
		if (ai->ctrlState == CIVSTATE_PARKED || ai->wrecked)
		{
			dist = abs(ai->currentZ - z) + abs(ai->currentX - x);

			if (dist < ClosestCar)
				ClosestCar = dist;
		}
	}
}

// [0x4902d8] [D]
// The car closest to the player (within 10000 cells, x + z), -1 if none.
int ClosestCarToPlayer(void)
{
	CIV_STATE *ai;
	int max_dist;
	int dist;
	int x;
	int z;
	int i;

	ai = civilian_ai_data;
	max_dist = 10000;

	x = IntegerApplyRecip(car_data[0].location.vx);
	z = IntegerApplyRecip(car_data[0].location.vz);

	ClosestCar = -1;

	for (i = 0; i < gTrafficDensity; i++, ai++)
	{
		if (ai->ctrlState != CIVSTATE_EMPTY)
		{
			dist = abs(ai->currentZ - z) + abs(ai->currentX - x);

			if (dist < max_dist)
			{
				ClosestCar = i;
				max_dist = dist;
			}
		}
	}

	return ClosestCar;
}

// [0x4903c1] [D]
// The number of moving (or wrecked) cars within max_dist road map cells of (x, z); the closest
// distance into ClosestCar.
int CloseCars(int x, int z, int max_dist)
{
	CIV_STATE *ai;
	int index;
	int slot;
	int dist;
	int dx;
	int dz;

	ai = civilian_ai_data;
	index = 0;

	x += 750;
	z -= 750;

	x = IntegerApplyRecip(x);
	z = IntegerApplyRecip(z);

	ClosestCar = 1024;

	for (slot = 0; slot < gTrafficDensity; slot++, ai++)
	{
		if (ai->ctrlState != CIVSTATE_EMPTY && (ai->ctrlState != CIVSTATE_PARKED || ai->wrecked))
		{
			dx = abs(ai->currentX - x);
			dz = abs(ai->currentZ - z);

			if (dx < max_dist && dz < max_dist)
			{
				dist = dz + dx;

				if (dist < ClosestCar)
					ClosestCar = dist;

				index++;
			}
		}
	}

	return index;
}

// [0x4904fe] [D]
// How the car may enter the junction ahead: 0 free (no lights), 1 give way, 2 stop (red light),
// 3 green light.
int GetThrustState(CIV_STATE *ai)
{
	int phase;

	if (Junctions[ai->currentJunction].JWH)
		return 0;

	if (ai->turnDir == 1)
		return 1;

	if (ai->pathFlags & 1)
		return 1;

	if (!(ai->pathFlags & 4))
		return 0;

	if (ai->currentLane & 1)
		phase = JunctionLightsWE;
	else
		phase = JunctionLightsNS;

	if (phase == 3)
		return 3;

	if (ai->InJunction)
		return 0;

	return 2;
}

// [0x490599] [D]
// A collision between two cars (0 the player, 65 the lead car): wakes up a parked cop the player
// hit, ends the chase when the player hits the lead car, wrecks the civilians and notes felonies.
void NotifyAIOfCollision(int c1, int c2, int howHard)
{
	// NOTE: BUG: for c1 == 0 the model is read from civilian_ai_data[c1 - 1] (the [-1] entry), but
	// should be [c2 - 1]
	if ((c1 == 0 && !civilian_ai_data[c2 - 1].IsCopCar && civilian_ai_data[c1 - 1].model == 1)
		|| (c2 == 0 && !civilian_ai_data[c1 - 1].IsCopCar && civilian_ai_data[c1 - 1].model == 1))
	{
		// collision with inactive cop
		if (c1)
			ActivateCop(c1 - 1);
		else
			ActivateCop(c2 - 1);
	}

	if (gMissionReplaying
		&& ((c1 == 0 && c2 == 65) || (c1 == 65 && c2 == 0))
		&& !gChaseHQMode
		&& !LeadCarLost
		&& !LeadCarHit
		&& !LeadCarFinished)
	{
		// collision with lead car
		LeadCarHit = 1;
		ReplayCarEndPosition.vx = (float)car_data[65].location.vx;
		ReplayCarEndPosition.vy = (float)car_data[65].location.vy;
		ReplayCarEndPosition.vz = (float)car_data[65].location.vz;
	}

	// car 1 is civilian/cop
	if (c1 && c1 < 65)
	{
		// car 2 is civilian/cop
		if (c2 && c2 < 65)
		{
			// if a cop or wrecked civilian hits a civilian
			if (civilian_ai_data[c1 - 1].wrecked == 1
				|| civilian_ai_data[c2 - 1].wrecked == 1
				|| civilian_ai_data[c1 - 1].IsCopCar
				|| civilian_ai_data[c2 - 1].IsCopCar)
			{
				if (civilian_ai_data[c1 - 1].parked)
					TriggerAlarm(c1);

				if (civilian_ai_data[c2 - 1].parked)
					TriggerAlarm(c2);

				civilian_ai_data[c1 - 1].wrecked = 1;
				civilian_ai_data[c1 - 1].driving = 0;
				civilian_ai_data[c2 - 1].wrecked = 1;
				civilian_ai_data[c2 - 1].driving = 0;
			}

			// civilian/civilian collision, do nothing
		}
		else
		{
			// civilian hits player/lead car
			if (civilian_ai_data[c1 - 1].parked)
				TriggerAlarm(c1);

			civilian_ai_data[c1 - 1].wrecked = 1;
			civilian_ai_data[c1 - 1].driving = 0;

			if (civilian_ai_data[c1 - 1].IsCopCar)
			{
				// NOTE: the original tests howHard < 16 and makes the same call either way
				if (civilian_ai_data[c1 - 1].ctrlState != CIVSTATE_PURSUIT)
					NoteFelony(&gFelonyData, 5, 4096);
			}
			else
			{
				NoteFelony(&gFelonyData, 4, 4096);
			}

			if (c2 == 0)
				collision_frame = game_frame;	// player's last collision frame
		}
	}
	else
	{
		// player/lead car hits civilian
		if (civilian_ai_data[c2 - 1].parked)
			TriggerAlarm(c2);

		civilian_ai_data[c2 - 1].wrecked = 1;
		civilian_ai_data[c2 - 1].driving = 0;
	}
}

// [0x4908ba] [D]
void NotifyAIRunAFewFrames(int car)
{
	civilian_ai_data[car - 1].pingOutTimer = 0;
}

// [0x4908cf] [D]
// The thrust for the car's speed: the lane's maximum speed, slowing for the junction ahead and
// cars in the way, waiting at give way junctions and lights; parks the car when it has stopped
// with CVST_PARKING.
void ControlSpeed(CIV_STATE *ai, CAR_DATA *cp)
{
	int interp_speed;
	int target_speed;

	cp->thrust = 0;

	if (ai->wrecked == 1 || ai->ctrlState == CIVSTATE_PARKED || ai->ctrlState == CIVSTATE_BLOCKING)
	{
		ai->speedDamperTimer = 0;
		return;
	}

	if (!ai->InJunction)
		ai->maxSpeed = ControlMaxSpeed(ai);

	if ((ai->stateFlags & CVST_PARKING) && cp->speed == 0 && ai->maxSpeed == 0)
	{
		ai->stateFlags &= ~CVST_PARKING;
		ai->ctrlState = CIVSTATE_PARKED;
		ai->driving = 0;
		ai->parkedTimer = -1;

		if (Random() & 3)
			ai->parked = 1;

		return;
	}

	if (ai->distAhead < 3000 && (ai->stateFlags & CVST_DRIVING))
	{
		ai->targetZ = INT_MAX;
		ai->targetX = INT_MAX;

		ai->stateFlags &= ~CVST_DRIVING;

		ROADS_GetNextJunction(civ_ai_data_pt[ai->id].location.vx, civ_ai_data_pt[ai->id].location.vz, ai->currentLane);

		SelectNextRoad(ai);
	}

	switch (GetThrustState(ai))
	{
		case 0:
		case 3:
			if (!ai->InJunction && ai->distAhead < 650 && (game_frame & 3) == ai->currentLane)
			{
				if (ai->turnDir == -1)
				{
					if (SafeToSpeedUp(ai))
					{
						ai->maxSpeed = 60;
						ai->InJunction = 1;
					}
					else if ((ai->pathFlags & 4) && ai->trafficLightPhaseId <= JunctionLightsChanged && !ai->InJunction)
					{
						ROADS_GetNextJunction(civ_ai_data_pt[ai->id].location.vx, civ_ai_data_pt[ai->id].location.vz, ai->currentLane);

						SelectNextRoad(ai);
					}
				}
				else if (ai->turnDir == 0)
				{
					if (!RoadNotSafeToEnter(ai, 0))
					{
						ai->maxSpeed = 130;
						ai->InJunction = 1;
					}
					else if ((ai->pathFlags & 4) && ai->trafficLightPhaseId <= JunctionLightsChanged && !ai->InJunction)
					{
						ROADS_GetNextJunction(civ_ai_data_pt[ai->id].location.vx, civ_ai_data_pt[ai->id].location.vz, ai->currentLane);

						SelectNextRoad(ai);
					}
				}
				else if (ai->turnDir == 1)
				{
					if (!RoadNotSafeToEnter(ai, 1))
					{
						ai->maxSpeed = 60;
						ai->InJunction = 1;
					}
					else if ((ai->pathFlags & 4) && ai->trafficLightPhaseId <= JunctionLightsChanged && !ai->InJunction)
					{
						ROADS_GetNextJunction(civ_ai_data_pt[ai->id].location.vx, civ_ai_data_pt[ai->id].location.vz, ai->currentLane);

						SelectNextRoad(ai);
					}
				}
			}
			break;
		case 1:
			if (!ai->InJunction && ai->distAhead < 650 && SafeToEnterRoadGiveWay(ai))
			{
				ai->InJunction = 1;

				if (ai->turnDir)
					ai->maxSpeed = 60;
				else
					ai->maxSpeed = 130;
			}
			break;
		case 2:
			if (ai->ctrlState != CIVSTATE_TURNING && (ai->turnDir || !ai->InJunction))
			{
				ai->InJunction = 0;

				if (ai->maxSpeed > 90)
					ai->maxSpeed = 90;

				if (ai->distAhead < 650)
				{
					ai->stateFlags &= ~(CVST_TURNING | CVST_INDICATE_LT);
					ai->maxSpeed = 0;
				}
			}
			break;
	}

	target_speed = ai->maxSpeed;

	if ((ai->id & 3) == (game_frame & 3))
		ai->blocked = SlowCarInWayCheck(ai);

	if (ai->blocked)
		target_speed = 0;

	interp_speed = target_speed - ai->speed;

	if (cp->wheelsOnGround < 4 && interp_speed > 0)
	{
		ai->speedDamperTimer = 0;
	}
	else if (interp_speed > 0)
	{
		ai->speedDamperTimer = 0;

		if (interp_speed > 32)
			interp_speed = 32;

		cp->thrust = 2500 * interp_speed / 32;
	}
	else if (interp_speed < 0)
	{
		if (ai->speedDamperTimer < 128)
			ai->speedDamperTimer++;

		if (ai->speedDamperTimer < 2)
			interp_speed = -8;

		if (interp_speed < -32)
			interp_speed = -32;

		cp->thrust = 3500 * interp_speed / 32;
	}
}

// [0x490ec0] [D]
// The car's maximum speed: 0 close to the junction or behind a car, slowing down within the
// lane's brake distance, else the lane's speed.
int ControlMaxSpeed(CIV_STATE *ai)
{
	if (ai->stateFlags & CVST_PARKING)
	{
		ai->maxSpeed -= 16;

		if (ai->maxSpeed < 0)
			ai->maxSpeed = 0;

		return ai->maxSpeed;
	}

	if (ai->distAhead < 650)
	{
		ai->maxSpeed = 0;
		return 0;
	}

	if (FindCarAhead(ai, 1, 0) != -1)
	{
		ai->maxSpeed = 0;
		return 0;
	}

	if (ai->distAhead >= brake_distances[ai->laneOffset & 3])
	{
		ai->maxSpeed = lane_max_speeds[ai->laneOffset & 3];
		return ai->maxSpeed;
	}

	ai->maxSpeed = ai->distAhead / 24;

	if (ai->maxSpeed < 5)
		ai->maxSpeed = 5;

	return ai->maxSpeed;
}

// [0x490fa8] [D]
// Collects the road map cells on the ring around the player (or the lead car) where cars
// (road_ping_points: roads at least 4 cells long, or info bit 1) and pedestrians
// (path_ping_points: 0x8000 cells) can be added.
void GetPingPoints(void)
{
	PING_POINT *road_ping;
	PING_POINT *path_ping;
	int *offset;
	int region;
	int index;
	int info;
	int cx;
	int cz;
	int x;
	int z;
	int i;
	short road;

	offset = cutout_offsets[0];
	road_ping = road_ping_points;
	path_ping = path_ping_points;

	num_road_ping_points = 0;
	num_path_ping_points = 0;

	if (gMissionReplaying || gMissionRecording)
	{
		x = RoadMapUnitXMid + car_data[65].location.vx;
		z = RoadMapUnitZMid - car_data[65].location.vz;
	}
	else
	{
		x = RoadMapUnitXMid + car_data[0].location.vx;
		z = RoadMapUnitZMid - car_data[0].location.vz;
	}

	x = IntegerApplyRecip(x);
	z = IntegerApplyRecip(z);

	for (i = 0; i < num_cutout_pieces; i++, offset += 4)
	{
		cx = offset[0] + x;
		cz = offset[1] + z;

		if (cx < 0 || cx >= RoadMapWidth || cz < 0 || cz >= RoadMapHeight)
			continue;

		region = 2 * ((regions_down - 1 - cz / road_region_size) & 1) + ((cx / road_region_size) & 1);
		index = road_region_size * (cz % road_region_size) + cx % road_region_size;

		road = RoadMapDataRegions[region][index];

		if (road == -32768)
		{
			path_ping->x = 1500 * cx - RoadMapUnitXMid;
			path_ping->z = RoadMapUnitZMid - 1500 * cz;
			num_path_ping_points++;
			path_ping++;
			continue;
		}

		info = RoadMapInfoRegions[region][index];

		road_ping->field_02 = info >> 30;

		if (info & 2)
			road_ping->field_00 = -1;
		else if (road >= 0 && road <= NumRoads && Roads[road].Length >= 4)
			road_ping->field_00 = road;
		else
			continue;

		road_ping->x = 1500 * cx - RoadMapUnitXMid;
		road_ping->z = RoadMapUnitZMid - 1500 * cz;
		num_road_ping_points++;
		road_ping++;
	}
}

// [0x491291] [D]
// The ring of road map cells 23 cells around the player (cutout_offsets: x, z, x + z * width).
void BuildJunctionPingInfo(void)
{
	int pieces[240][2];
	int angle;
	int x;
	int z;
	int i;

	angle = 0;
	num_cutout_pieces = 0;

	while (angle < 4096)
	{
		x = 23 * isin(angle) / 4096;
		z = 23 * icos(angle) / 4096;

		for (i = 0; i < num_cutout_pieces && (x != pieces[i][0] || z != pieces[i][1]); i++)
			;

		if (i == num_cutout_pieces)
		{
			pieces[num_cutout_pieces][0] = x;
			pieces[num_cutout_pieces][1] = z;
			num_cutout_pieces++;
		}

		angle += 32;
	}

	for (i = 0; i < num_cutout_pieces; i++)
	{
		cutout_offsets[i][0] = pieces[i][0];
		cutout_offsets[i][1] = pieces[i][1];
		cutout_offsets[i][2] = RoadMapWidth * pieces[i][1] + pieces[i][0];
	}
}

// [0x4913f9] [D]
// Places the player's car: the motel camera, the interview start, a mission's start, the saved
// start position (rposname) or the replayed chase's cars.
void InitPlayerCarPosition(void)
{
	FILE *fp;
	LONGVECTOR4 startpos;
	char filename[64];

	if (gDoMotelRoom)
	{
		SetMotelCamera(&startpos);

		car_data[PlayerCar].location.vx = startpos[0];
		car_data[PlayerCar].location.vy = startpos[1];
		car_data[PlayerCar].location.vz = startpos[2];

		InitCarPhysics(&car_data[PlayerCar], &startpos);

		car_data[0].direction = 1024;
	}
	else if (gDoInterview)
	{
		if (GameMode == 201)
		{
			sprintf(filename, "ReplayData\\Chase\\%s.LEADER", MissionName);
			fp = fopen(filename, "rb");
			DriverFRead(&car_data[65], 1, sizeof(CAR_DATA), fp);
			fclose(fp);

			car_data[65].model = 0;

			sprintf(filename, "ReplayData\\Chase\\%s.PLAYER", MissionName);
			fp = fopen(filename, "rb");
			DriverFRead(car_data, 1, sizeof(CAR_DATA), fp);
			fclose(fp);

			car_data[0].model = ChaseCarType;
		}
		else
		{
			// setup interview spawn
			car_data[PlayerCar].location.vx = -10240;
			car_data[PlayerCar].location.vy = -500;
			car_data[PlayerCar].location.vz = -21504;

			startpos[0] = car_data[PlayerCar].location.vx;
			startpos[1] = car_data[PlayerCar].location.vy;
			startpos[2] = car_data[PlayerCar].location.vz;

			car_data[0].model = 0;

			InitCarPhysics(&car_data[PlayerCar], &startpos);

			car_data[0].direction = 1024;
		}
	}
	else if ((!gMissionRecording && !gMissionReplaying) || (gDoTraining && gTrainingMode == 2))
	{
		if (mission.useLastPlayerPosition)
		{
			startpos[0] = mission.player_position[0];
			startpos[1] = mission.player_position[1];
			startpos[2] = mission.player_position[2];

			InitCarPhysics(car_data, &startpos);

			car_data[0].direction = mission.player_rotation;
			car_data[0].model = ChaseCarType;

			TempBuildHandlingMatrix(car_data, 0);
		}
		else
		{
			fp = fopen(rposname, "rb");

			if (fp)
			{
				ClearMem(car_data, sizeof(CAR_DATA));

				DriverFRead(car_data, 1, sizeof(CAR_DATA), fp);
				fclose(fp);

				car_data[0].model = 0;
			}
			else
			{
				memset(startpos, 0, 12);

				car_data[0].model = 0;

				InitCarPhysics(car_data, &startpos);

				car_data[0].direction = 1024;

				TempBuildHandlingMatrix(car_data, 0);
			}
		}
	}
	else
	{
		sprintf(filename, "ReplayData\\Chase\\%s.LEADER", MissionName);
		fp = fopen(filename, "rb");
		DriverFRead(&car_data[65], 1, sizeof(CAR_DATA), fp);
		fclose(fp);

		car_data[65].model = 0;

		sprintf(filename, "ReplayData\\Chase\\%s.PLAYER", MissionName);
		fp = fopen(filename, "rb");
		DriverFRead(car_data, 1, sizeof(CAR_DATA), fp);
		fclose(fp);

		car_data[0].model = ChaseCarType;
	}

	if (gDoTraining && gTrainingMode == 2)
		car_data[65] = car_data[0];

	if (gMissionRecording || gMissionReplaying)
	{
		camera_position.vx = (float)car_data[65].location.vx;
		camera_position.vy = -400.0f;
		camera_position.vz = (float)car_data[65].location.vz;
	}
	else
	{
		camera_position.vx = (float)car_data[PlayerCar].location.vx;
		camera_position.vy = -400.0f;
		camera_position.vz = (float)car_data[PlayerCar].location.vz;
	}

	car_data[0].totalDamage = 0;
	car_data[0].damage[5] = 0;
	car_data[0].damage[4] = 0;
	car_data[0].damage[3] = 0;
	car_data[0].damage[2] = 0;
	car_data[0].damage[1] = 0;
	car_data[0].damage[0] = 0;
}

// [0x4918cb] [D]
// Draws the civilian cars within the far clip distance, grouped by palette (one SwapPalette per
// palette), with their lights.
void DrawCivilianCars(void)
{
	CIV_STATE *ai;
	CAR_DATA *cp;
	char cars[68];
	unsigned char num_cars;
	unsigned char palette;
	char swapped;
	int camera_xpos;
	int camera_zpos;
	int max_cars;
	int civ_id;
	int car_id;
	int dist;
	int dx;
	int dz;
	int i;

	cp = civ_ai_data_pt;
	ai = civilian_ai_data;
	num_cars = 0;

	if (gDoTraining && gTrainingMode == 2 && (!dword_CCCC34 || !dword_CCC8A0))
		return;

	if (gMissionRecording || gMissionReplaying)
		max_cars = 65;
	else
		max_cars = 64;

	camera_xpos = float_to_int(camera_position.vx);
	camera_zpos = float_to_int(camera_position.vz);

	for (i = 0; i < max_cars; i++, ai++, cp++)
	{
		if (ai->ctrlState == CIVSTATE_EMPTY)
			continue;

		dx = abs(camera_xpos - cp->location.vx);
		dz = abs(camera_zpos - cp->location.vz);

		if (dx >= dz)
			dist = dz / 2 + dx;
		else
			dist = dx / 2 + dz;

		if ((float)dist < far_clip)
			cars[num_cars++] = i;
	}

	if (num_cars == 0)
		return;

	if (gDoMultiColouredCars)
	{
		for (palette = 0; palette < 50; palette++)
		{
			swapped = 0;

			for (i = 0; i < num_cars; i++)
			{
				civ_id = (unsigned char)cars[i];
				car_id = civ_id + 1;

				if ((unsigned char)civilian_ai_data[civ_id].palette == palette)
				{
					if (!swapped)
					{
						swapped = 1;
						SwapPalette(palette);
					}

					DrawCar(car_id, civ_id);
					DrawCarExtras(car_id, &civilian_ai_data[civ_id]);
				}
			}
		}
	}
	else
	{
		SwapPalette(4);

		for (i = 0; i < num_cars; i++)
		{
			civ_id = (unsigned char)cars[i];
			car_id = civ_id + 1;

			if (civilian_ai_data[civ_id].palette != 25)
			{
				DrawCar(car_id, civ_id);
				DrawCarExtras(car_id, &civilian_ai_data[civ_id]);
			}
		}

		SwapPalette(25);

		for (i = 0; i < num_cars; i++)
		{
			civ_id = (unsigned char)cars[i];
			car_id = civ_id + 1;

			if (civilian_ai_data[civ_id].palette == 25)
			{
				DrawCar(car_id, civ_id);
				DrawCarExtras(car_id, &civilian_ai_data[civ_id]);
			}
		}
	}
}

// [0x491c3a] [D]
// The cop lights of a chasing cop or a roadblock car, the indicators of a car changing lanes,
// turning or parking, and the brake or side lights.
void DrawCarExtras(int id, CIV_STATE *ai)
{
	CAR_DATA *cp;
	int dist;

	if ((ai->model == 1 && ai->IsCopCar && ai->ctrlState == CIVSTATE_PURSUIT) || ai->ctrlState == CIVSTATE_BLOCKING)
	{
		if (ai->ctrlState == CIVSTATE_BLOCKING)
			AddCopCarLight(id, &ai->coplife, 1.0f);
		else
			AddCopCarLight(id, &ai->coplife, cop_data[(unsigned char)ai->IsCopCar - 1].health);
	}

	if (ai->stateFlags & CVST_TURNING)
	{
		cp = &civ_ai_data_pt[ai->id];

		if (ai->currentLane & 1)
		{
			if (ai->targetZ != INT_MAX)
			{
				dist = ai->targetZ - cp->where.t[2];

				if (dist < -256)
				{
					if (ai->currentLane == 3)
						AddIndicatorLight(id, 1);
					else
						AddIndicatorLight(id, 0);
				}
				else if (dist > 256)
				{
					if (ai->currentLane == 1)
						AddIndicatorLight(id, 1);
					else
						AddIndicatorLight(id, 0);
				}
			}
		}
		else if (ai->targetX != INT_MAX)
		{
			dist = ai->targetX - cp->where.t[0];

			if (dist < -256)
			{
				if (ai->currentLane == 0)
					AddIndicatorLight(id, 1);
				else
					AddIndicatorLight(id, 0);
			}
			else if (dist > 256)
			{
				if (ai->currentLane == 2)
					AddIndicatorLight(id, 1);
				else
					AddIndicatorLight(id, 0);
			}
		}
	}
	else if (ai->ctrlState != CIVSTATE_PARKED
		&& ((ai->currentRoad & 0x4000) || ai->ctrlState == CIVSTATE_TURNING || ai->distAhead < 5000))
	{
		if (ai->turnDir == -1)
			AddIndicatorLight(id, 1);
		else if (ai->turnDir == 1)
			AddIndicatorLight(id, 0);
	}

	if (ai->stateFlags & CVST_INDICATE_RT)
		AddIndicatorLight(id, 0);

	if (ai->stateFlags & CVST_INDICATE_LT)
		AddIndicatorLight(id, 1);

	if (ai->speedDamperTimer <= 4)
		AddBrakeLight(id);
	else
		AddSideLights(id);
}

// [0x491ed9] [D]
// Turns the siren lights of the player's cop car and of the chasing cops and roadblock cars
// (faster the healthier the cop).
void HandleCopSirens(void)
{
	CIV_STATE *ai;
	char cops[68];
	unsigned char num_cops;
	float health;
	int max_cars;
	int i;

	ai = civilian_ai_data;
	num_cops = 0;

	if (gPlayerSirenOn)
		gPlayerSirenClock = (gPlayerSirenClock + 512) & 0xFFF;

	if (gMissionRecording || gMissionReplaying)
		max_cars = 65;
	else
		max_cars = 64;

	for (i = 0; i < max_cars; i++, ai++)
	{
		if (ai->ctrlState != CIVSTATE_EMPTY
			&& ((ai->model == 1 && ai->IsCopCar && ai->ctrlState == CIVSTATE_PURSUIT) || ai->ctrlState == CIVSTATE_BLOCKING))
		{
			cops[num_cops++] = i;
		}
	}

	for (i = 0; i < num_cops; i++)
	{
		ai = &civilian_ai_data[(unsigned char)cops[i]];

		if (ai->ctrlState == CIVSTATE_BLOCKING)
			health = 1.0f;
		else
			health = cop_data[(unsigned char)ai->IsCopCar - 1].health;

		ai->coplife = ((int)(512.0f * health) + ai->coplife) & 0xFFF;
	}
}

// [0x492060] [D]
void CheckCivilianScenaryCollisions(void)
{
	CIV_STATE *ai;
	int i;

	ai = civilian_ai_data;

	for (i = 0; i < 64; i++, ai++)
	{
		// only wrecked cars and cops
		if (ai->ctrlState && (ai->wrecked == 1 || ai->IsCopCar))
			CheckScenaryCollisions(ai->id + 1);
	}
}

// [0x4920cc] [D]
// A position in the road's lane (node: the direction) into ReturnedX, ReturnedY: fixed 1 the
// outermost lane, else lane offset (255 a random lane, counted from the other side for
// direction != 0), randomised by up to 128 (pulled to the kerb on roads with parking, Flags & 1)
// unless fixed is 1.
void GetTargetLaneCoords(int road, int node, int direction, int fixed, unsigned char offset)
{
	int lastLane;
	int lanePos;
	int rnd;
	int x;
	int y;

	lastLane = (unsigned char)RoadLaneInfo[road].lanes[node] - 1;

	if (fixed == 1)
	{
		lanePos = 1500 * lastLane;
	}
	else if (offset == 255)
	{
		lanePos = 1500 * (Random() % (lastLane + 1));
	}
	else
	{
		if (offset > lastLane)
			offset = RoadLaneInfo[road].lanes[node] - 1;

		if (direction)
			offset = lastLane - offset;

		lanePos = 1500 * offset;
	}

	switch (node)
	{
		case 0:
			x = lanePos + RoadLaneInfo[road].bounds[0].x;
			y = RoadLaneInfo[road].bounds[0].y;
			break;
		case 1:
			x = RoadLaneInfo[road].bounds[1].x;
			y = RoadLaneInfo[road].bounds[1].y - lanePos;
			break;
		case 2:
			x = RoadLaneInfo[road].bounds[2].x - lanePos;
			y = RoadLaneInfo[road].bounds[2].y;
			break;
		case 3:
			x = RoadLaneInfo[road].bounds[3].x;
			y = lanePos + RoadLaneInfo[road].bounds[3].y;
			break;
	}

	if (fixed != 1)
	{
		rnd = (unsigned char)Random() - 128;

		switch (node)
		{
			case 0:
				if (Roads[road].Flags & 1)
					x -= (Random() & 0x7F) + 128;
				else
					x += rnd;
				break;
			case 1:
				if (Roads[road].Flags & 1)
					y += (Random() & 0x7F) + 128;
				else
					y += rnd;
				break;
			case 2:
				if (Roads[road].Flags & 1)
					x += (Random() & 0x7F) + 128;
				else
					y += rnd;
				break;
			case 3:
				if (Roads[road].Flags & 1)
					y -= (Random() & 0x7F) + 128;
				else
					y += rnd;
				break;
		}
	}

	ReturnedX = x;
	ReturnedY = y;
}

// [0x492350] [D]
// The car's lane counted from the centre of the road (0 the innermost).
char GetLaneOffset(CIV_STATE *ai)
{
	CAR_DATA *cp;
	int numLanes;
	int lane;

	cp = &civ_ai_data_pt[ai->id];

	if ((ai->currentRoad & 0xC000) || ai->currentRoad < 0)
		return 0;

	numLanes = (unsigned char)RoadLaneInfo[ai->currentRoad].lanes[ai->currentLane];

	if (numLanes == 1)
		return 0;

	switch (ai->currentLane)
	{
		case 0:
			lane = (cp->location.vx + 750 - RoadLaneInfo[ai->currentRoad].bounds[0].x) / 1500;
			break;
		case 1:
			lane = (RoadLaneInfo[ai->currentRoad].bounds[1].y - (cp->location.vz - 750)) / 1500;
			break;
		case 2:
			lane = (RoadLaneInfo[ai->currentRoad].bounds[2].x - (cp->location.vx - 750)) / 1500;
			break;
		case 3:
			lane = (cp->location.vz + 750 - RoadLaneInfo[ai->currentRoad].bounds[3].y) / 1500;
			break;
	}

	return numLanes - lane - 1;
}

// [0x4924b4] [D]
// The player's lane offset for a direction on his road; -1 off a road, -2 no lanes that way.
char GetPlayerLaneOffset(unsigned char direction)
{
	unsigned short road;
	int numLanes;
	int lane;
	int vx;
	int vz;

	vx = car_data[0].location.vx;
	vz = car_data[0].location.vz;

	road = ROADS_GetRoadOrJunction(car_data[0].location.vx, car_data[0].location.vz);

	if (road & 0xC000)
		return -1;

	numLanes = (unsigned char)RoadLaneInfo[road].lanes[direction];

	if (RoadLaneInfo[road].lanes[direction] == 0)
		return -2;

	if (numLanes == 1)
		return 0;

	switch (direction)
	{
		case 0:
			lane = (vx + 750 - RoadLaneInfo[road].bounds[0].x) / 1500;
			break;
		case 1:
			lane = (RoadLaneInfo[road].bounds[1].y - (vz - 750)) / 1500;
			break;
		case 2:
			lane = (RoadLaneInfo[road].bounds[2].x - (vx - 750)) / 1500;
			break;
		case 3:
			lane = (vz + 750 - RoadLaneInfo[road].bounds[3].y) / 1500;
			break;
	}

	return numLanes - lane - 1;
}

// [0x492628] [D]
void InitCopState(CAR_DATA *cp, COP *cop, int id)
{
	ClearMem(cop, sizeof(COP));

	cop->id = id;
	cop->active = 1;
	cop->routeInMemory = 0;
	cop->justPinged = 1;
	cop->totalDamage = 0;
	cop->pCarData = cp;
	cop->field_5C = 0;
	cop->health = 1.0f;
}

// [0x49269a] [D]
// Turns a civilian into a cop (one the player hit).
void ActivateCop(int id)
{
	CIV_STATE *ai;
	int cop_slot;
	int cop_id;

	if (num_active_cops >= max_active_cops)
		return;

	ai = &civilian_ai_data[id];
	cop_slot = GetCopCarSlot();

	if (cop_slot != -1)
	{
		ai->IsCopCar = cop_slot;
		cop_id = cop_slot - 1;
		ai->palette = 25;
		ai->coplife = 0;

		InitCopState(&civ_ai_data_pt[id], &cop_data[cop_id], cop_id);
		num_active_cops++;

		cop_data[cop_id].justPinged = 0;
		cop_data[cop_id].field_5C = 1;
	}

	RequestCopCar = 0;
}

// [0x492756] [D]
// Places a new car in the slot on a road (lane by the side of the road the player is on, one way
// roads their own) or at a junction (road_id -1): parked at the kerb sometimes, else driving to
// the next junction; with a random model and palette, or the requested cop car. Returns 0 if
// the place is too close to other cars (the slot is freed).
int AllocateAndPlaceCar(int civ_id, int road_id, unsigned char direction, int posX, int posZ)
{
	static char bad_guy_colours[6][4] = {
		{ 2, 17, 8, 0 },
		{ 3, 10, 4, 7 },
		{ 0, 0, 0, 0 },
		{ 3, 5, 12, 8 },
		{ 0, 0, 0, 0 },
		{ 0, 0, 0, 0 },
	};

	CIV_STATE *ai;
	unsigned char max_colours;
	unsigned char lane;
	unsigned char otherLane;
	char boundDir;
	char isOneWay;
	char parked;
	int cop_slot;

	parked = 0;
	ai = &civilian_ai_data[civ_id];

	ai->ctrlState = CIVSTATE_EMPTY;

	if (road_id != -1)
		boundDir = RoadBounds[road_id].dir;

	ClearMem(&civ_ai_data_pt[civ_id], sizeof(CAR_DATA));

	civ_ai_data_pt[civ_id].id = civ_id + 1;

	if (road_id != -1)
	{
		if (boundDir == 1)
		{
			if (gMissionReplaying || gMissionRecording)
				lane = 1500 * RoadBounds[road_id].x1 >= car_data[65].location.vx ? 3 : 1;
			else
				lane = 1500 * RoadBounds[road_id].x1 >= car_data[0].location.vx ? 3 : 1;

			if (lane == 1)
				otherLane = 3;
			else
				otherLane = 1;

			isOneWay = Roads[road_id].IsOneWay;

			if (isOneWay == (char)0x81)
			{
				direction = 1;
				otherLane = 1;
			}
			else if (isOneWay == (char)0x83)
			{
				direction = 1;
				otherLane = 3;
			}

			if (direction == 1)
				ai->currentLane = otherLane;
			else
				ai->currentLane = lane;
		}
		else
		{
			if (gMissionReplaying || gMissionRecording)
				lane = -1500 * RoadBounds[road_id].y1 >= car_data[65].location.vz ? 0 : 2;
			else
				lane = -1500 * RoadBounds[road_id].y1 >= car_data[0].location.vz ? 0 : 2;

			if (lane)
				otherLane = 0;
			else
				otherLane = 2;

			isOneWay = Roads[road_id].IsOneWay;

			if (isOneWay == (char)0x80)
			{
				direction = 1;
				otherLane = 0;
			}
			else if (isOneWay == (char)0x82)
			{
				direction = 1;
				otherLane = 2;
			}

			if (direction == 1)
				ai->currentLane = otherLane;
			else
				ai->currentLane = lane;
		}
	}

	ai->parked = 0;
	ai->driving = 0;
	ai->wrecked = 0;
	ai->currentRoad = road_id;

	if (road_id == -1)
		parked = 1;
	else if ((Roads[road_id].Flags & 1) && (Random() & 7) == 0 && CarsParkedOnRoad(road_id) == 0)
		parked = 1;

	if (parked)
	{
		if (road_id == -1)
		{
			FindClosestParkedCar(posX, posZ);

			if (ClosestCar < ParkedCarMinGap)
			{
				RemoveCivilianCar(civ_id);
				return 0;
			}
		}
		else
		{
			GetTargetLaneCoords(road_id, ai->currentLane, ai->turnDir, 1, 255);

			if (ai->currentLane & 1)
				posZ = ReturnedY;
			else
				posX = ReturnedX;

			switch (ai->currentLane)
			{
				case 0:
					posX += 470;
					break;
				case 1:
					posZ -= 470;
					break;
				case 2:
					posX -= 470;
					break;
				case 3:
					posZ += 470;
					break;
			}

			FindClosestParkedCar(posX, posZ);

			if (ClosestCar < ParkedCarMinGap
				|| ROADS_GetNextJunction(posX, posZ, ai->currentLane) != 0
				|| RouteData.distAlongSegment < 4500
				|| RouteData.distAhead < 3000)
			{
				RemoveCivilianCar(civ_id);
				return 0;
			}
		}

		if (Random() & 3)
			ai->parked = 1;

		ai->ctrlState = CIVSTATE_PARKED;
		ai->pingOutTimer = 0;
		ai->parkedTimer = -1;

		ai->currentX = IntegerApplyRecip(civ_ai_data_pt[ai->id].location.vx + 750);
		ai->currentZ = IntegerApplyRecip(civ_ai_data_pt[ai->id].location.vz - 750);
	}
	else
	{
		ai->ctrlState = CIVSTATE_MOVING;
		ai->driving = 1;

		ROADS_GetNextJunction(posX, posZ, ai->currentLane);

		if (!SelectNextRoad(ai))
			return 0;

		GetTargetLaneCoords(ai->currentRoad, ai->currentLane, ai->turnDir, 0, 255);

		if (ai->currentLane & 1)
			posZ = ReturnedY;
		else
			posX = ReturnedX;

		if (CloseCars(posX, posZ, CloseCarGap) > MaxCloseCars || ClosestCar < CloseCarMinGap)
		{
			RemoveCivilianCar(civ_id);
			return 0;
		}

		ai->distAhead = RouteData.distAhead;
		ai->distAlongSegment = RouteData.distAlongSegment;
		ai->currentJunction = RouteData.junction;
		ai->pathFlags = RouteData.flags;
	}

	if (road_id == -1)
		civ_ai_data_pt[civ_id].direction = direction << 10;
	else
		civ_ai_data_pt[civ_id].direction = ai->currentLane << 10;

	ai->targetZ = INT_MAX;
	ai->targetX = INT_MAX;
	ai->speed = 0;
	ai->maxSpeed = 130;

	if (GameLevel == 6)
		ai->model = Random() % 2 + 3;	// newcastle cars
	else
		ai->model = Random() % 9 + 3;

	if (ai->model == 9)
		ai->palette = 16;				// HEY! TAXI!
	else
		ai->palette = Random() % 10 + 4;

	ai->IsCopCar = 0;

	if (RequestCopCar == 1 && !parked)
	{
		// request a non-parked pursuer
		cop_slot = GetCopCarSlot();

		if (cop_slot != -1)
		{
			ai->IsCopCar = cop_slot;
			ai->palette = 25;
			ai->coplife = 0;

			InitCopState(&civ_ai_data_pt[civ_id], &cop_data[cop_slot - 1], cop_slot - 1);

			num_active_cops++;
		}

		RequestCopCar = 0;
	}

	if (ai->IsCopCar)
	{
		// initialize pursuer
		if (gUseBadGuyModel == 1)
		{
			if (gUseCopModel == 1 && (Random() & 1))
			{
				// randomly spawn a cop
				ai->model = 1;
			}
			else
			{
				// spawn a bad guy with a random color
				ai->model = 2;

				max_colours = bad_guy_colours[GameLevel][0];

				ai->palette = bad_guy_colours[GameLevel][abs(Random()) % max_colours + 1];
			}
		}
		else
		{
			// spawn a cop
			ai->model = 1;
		}
	}

	civ_ai_data_pt[civ_id].model = ai->model;
	civ_ai_data_pt[civ_id].location.vx = posX;
	civ_ai_data_pt[civ_id].location.vz = posZ;

	InitPingedCar(&civ_ai_data_pt[civ_id], ai);

	ai->currentX = IntegerApplyRecip(civ_ai_data_pt[ai->id].location.vx + 750);
	ai->currentZ = IntegerApplyRecip(civ_ai_data_pt[ai->id].location.vz - 750);

	ai->laneOffset = GetLaneOffset(ai);
	ai->stateFlags = CVST_DRIVING;

	CreateDentableCar(civ_id);
	TempBuildHandlingMatrix(&civ_ai_data_pt[civ_id], 1);

	return 1;
}

// [0x493098] [D]
// The mission's target car (ping slot targetId + 1) is more than targetRange from the player.
int IsMissionCarLost(int targetId, int targetRange)
{
	CIV_STATE *ai;
	int ping_slot;
	int dx;
	int dz;
	int i;

	ai = civilian_ai_data;

	if (targetRange == 0)
		return 0;

	ping_slot = targetId + 1;

	for (i = 0; i < 64 && (unsigned char)ai->ping_slot != ping_slot; i++)
		ai++;

	if (i >= 64)
		return 0;

	dx = civ_ai_data_pt[i].location.vx - car_data[0].location.vx;
	dz = civ_ai_data_pt[i].location.vz - car_data[0].location.vz;

	// HMM: is the lack of ABS a bug?
	return dz * dz + dx * dx > targetRange * targetRange;
}

// [0x49315a] [D]
// The mission's target car has at least maxDamage damage; its position into pos.
int IsMissionCarWrecked(int targetId, int maxDamage, IVECTOR *pos)
{
	CIV_STATE *ai;
	CAR_DATA *civcar;
	int ping_slot;
	int i;

	ai = civilian_ai_data;

	ping_slot = targetId + 1;

	for (i = 0; i < 64 && (unsigned char)ai->ping_slot != ping_slot; i++)
		ai++;

	civcar = &civ_ai_data_pt[i];

	if (pos)
	{
		pos->vx = civcar->location.vx;
		pos->vy = civcar->location.vy;
		pos->vz = civcar->location.vz;
	}

	return civcar->totalDamage >= maxDamage;
}

// [0x493208] [D]
// One of the slots 20 - 27 for a mission's cars while a chase is replayed; -1 when all are used.
int AddSpareCar(void)
{
	if (SpareCarsUsed == 8)
		return -1;

	return ++SpareCarsUsed + 19;
}

// [0x493230] [D]
// A mission's car (index: its ping slot - 1), parked at a position; model 1 and 2 (a cop or the
// bad guy) get a cop slot. Without a free slot the car furthest north east is removed.
void InstantCivilianCar(char index, int model, char palette, int dir, int xpos, int zpos)
{
	CIV_STATE *ai;
	COP *cd;
	int free_slot;
	int cop_slot;
	int max_pos;
	int slot;
	int i;

	if (gMissionReplaying)
	{
		slot = AddSpareCar();

		if (slot == -1)
			return;
	}
	else
	{
		slot = AddCivilianCar();
	}

	if (slot == -1)
	{
		// find a free slot
		max_pos = 0;
		free_slot = 0;

		for (i = 0; i < gTrafficDensity; i++)
		{
			if (civ_ai_data_pt[i].location.vz + civ_ai_data_pt[i].location.vx > max_pos)
			{
				free_slot = i;
				max_pos = civ_ai_data_pt[i].location.vz + civ_ai_data_pt[i].location.vx;
			}
		}

		RemoveCivilianCar(free_slot);

		slot = AddCivilianCar();
	}

	ai = &civilian_ai_data[slot];

	ClearMem(&civ_ai_data_pt[slot], sizeof(CAR_DATA));

	civ_ai_data_pt[slot].id = slot + 1;

	ai->ctrlState = CIVSTATE_PARKED;
	ai->ping_slot = index + 1;
	ai->parked = 0;
	ai->driving = 0;
	ai->wrecked = 0;
	ai->currentRoad = -1;

	civ_ai_data_pt[slot].direction = dir;
	civ_ai_data_pt[slot].model = model;
	civ_ai_data_pt[slot].location.vx = xpos;
	civ_ai_data_pt[slot].location.vz = zpos;

	InitPingedCar(&civ_ai_data_pt[slot], ai);

	ai->targetZ = INT_MAX;
	ai->targetX = INT_MAX;
	ai->maxSpeed = 0;
	ai->speed = 0;
	ai->palette = palette + 4;
	ai->model = model;

	if (model == 1)
	{
		ai->IsCopCar = 1;
		ai->palette = 25;
	}
	else if (model == 2)
	{
		ai->IsCopCar = 1;
		ai->model = 2;
		ai->palette = 4;				// BUG: can't change color of bad guy!
	}
	else
	{
		ai->IsCopCar = 0;
	}

	if (ai->IsCopCar)
	{
		cop_slot = GetCopCarSlot();

		ai->IsCopCar = cop_slot;

		cd = &cop_data[cop_slot - 1];

		ClearMem(cd, sizeof(COP));

		cd->id = cop_slot - 1;
		cd->active = 1;
		cd->pCarData = &civ_ai_data_pt[ai->id];
		cd->justPinged = 1;

		num_active_cops++;
	}

	CreateDentableCar(slot);
	TempBuildHandlingMatrix(&civ_ai_data_pt[slot], 1);

	ai->currentX = IntegerApplyRecip(civ_ai_data_pt[ai->id].location.vx + 750);
	ai->currentZ = IntegerApplyRecip(civ_ai_data_pt[ai->id].location.vz - 750);

	ai->stateFlags = 0;
}

// [0x4935c9] [D]
// A mission's cop car chasing the player from a position.
void InstantCopCar(int dir, int x, int z)
{
	CIV_STATE *ai;
	int copslot;
	int civslot;

	civslot = AddCivilianCar();

	if (civslot == -1)
		return;

	ai = &civilian_ai_data[civslot];

	ClearMem(&civ_ai_data_pt[civslot], sizeof(CAR_DATA));
	civ_ai_data_pt[civslot].id = civslot + 1;

	ai->parked = 0;
	ai->driving = 0;
	ai->wrecked = 0;
	ai->currentRoad = -1;
	ai->ctrlState = CIVSTATE_MOVING;
	ai->driving = 1;

	if (!SelectNextRoad(ai))
		return;

	civ_ai_data_pt[civslot].direction = dir;

	ai->targetZ = INT_MAX;
	ai->targetX = INT_MAX;
	ai->speed = 0;
	ai->maxSpeed = 130;
	ai->palette = Random() % 10 + 4;
	ai->model = 1;
	ai->IsCopCar = 0;

	copslot = GetCopCarSlot();

	if (copslot == -1)
		return;

	ai->IsCopCar = copslot;
	ai->palette = 25;
	ai->coplife = 0;

	InitCopState(&civ_ai_data_pt[civslot], &cop_data[copslot - 1], copslot - 1);

	num_active_cops++;

	ai->ctrlState = CIVSTATE_PURSUIT;

	civ_ai_data_pt[civslot].model = ai->model;
	civ_ai_data_pt[civslot].location.vx = x;
	civ_ai_data_pt[civslot].location.vz = z;

	InitPingedCar(&civ_ai_data_pt[civslot], ai);
	CreateDentableCar(civslot);
	TempBuildHandlingMatrix(&civ_ai_data_pt[civslot], 1);

	ai->currentX = IntegerApplyRecip(civ_ai_data_pt[ai->id].location.vx + 750);
	ai->currentZ = IntegerApplyRecip(civ_ai_data_pt[ai->id].location.vz - 750);

	ai->laneOffset = GetLaneOffset(ai);
	ai->stateFlags = CVST_DRIVING;
}

// [0x493898] [D]
// A car at a position off the roads: a parked civilian, or a roadblock car (blocking). Returns
// the slot, -1 if none is free.
int AllocateAndPlaceCarAtPosition(int x, int y, int z, int dir, int blocking)
{
	CIV_STATE *ai;
	int civslot;

	civslot = AddCivilianCar();

	if (civslot == -1)
		return -1;

	ai = &civilian_ai_data[civslot];

	ClearMem(&civ_ai_data_pt[civslot], sizeof(CAR_DATA));
	civ_ai_data_pt[civslot].id = civslot + 1;

	ai->parked = 0;
	ai->driving = 0;
	ai->wrecked = 0;

	ai->currentRoad = -1;

	if (!blocking)
		FindClosestParkedCar(x, z);

	if ((Random() & 1) && !blocking)
		ai->parked = 1;

	ai->pingOutTimer = 0;
	ai->parkedTimer = -1;

	civ_ai_data_pt[civslot].direction = dir;

	ai->targetZ = INT_MAX;
	ai->targetX = INT_MAX;

	ai->speed = 0;

	if (blocking)
	{
		ai->model = 1;
		ai->palette = 25;
		ai->coplife = 0;
		ai->maxSpeed = 0;
		ai->ctrlState = CIVSTATE_BLOCKING;
	}
	else
	{
		ai->model = Random() % 9 + 3;
		ai->palette = Random() % 10 + 4;
		ai->maxSpeed = 130;
		ai->ctrlState = CIVSTATE_PARKED;
	}

	ai->IsCopCar = 0;

	civ_ai_data_pt[civslot].model = ai->model;
	civ_ai_data_pt[civslot].location.vx = x;
	civ_ai_data_pt[civslot].location.vz = z;

	InitPingedCar(&civ_ai_data_pt[civslot], ai);
	CreateDentableCar(civslot);
	TempBuildHandlingMatrix(&civ_ai_data_pt[civslot], 1);

	ai->currentX = IntegerApplyRecip(civ_ai_data_pt[ai->id].location.vx + 750);
	ai->currentZ = IntegerApplyRecip(civ_ai_data_pt[ai->id].location.vz - 750);

	return civslot;
}

// [0x493b42] [D]
// Pings a new car into a random road ping point; first saves the cars a requested roadblock
// needs (BlockerShots) and sets it up.
void AddCarRelativeToPlayer(void)
{
	PING_POINT *point;
	int civ_car;
	int civ_id;

	if (num_road_ping_points == 0)
		return;

	// fail-safe/old logic that never gets hit
	// NOTE: when replaying civ_id is used uninitialised, as in the original (ControlPings pings the
	// recorded cars then and never calls this function)
	if (!gMissionReplaying)
	{
		if (CarsSavedForBlock < CarsNeededInitially)
		{
			// allocate a roadblock car
			civ_id = AddCivilianCar();

			if (civ_id != -1)
				BlockerShots[CarsSavedForBlock++] = civ_id;

			return;
		}

		if (CarsSavedForBlock)
			SetupRoadBlock();			// do a roadblock

		civ_id = AddCivilianCar();

		if (civ_id == -1)
			return;
	}

	point = &road_ping_points[Random() % num_road_ping_points];

	if (point->field_00 == -1)
		civ_car = AllocateAndPlaceCar(civ_id, point->field_00, point->field_02 & 3, point->x, point->z);
	else
		civ_car = AllocateAndPlaceCar(civ_id, point->field_00, Random() & 1, point->x, point->z);

	if (civ_car)
		succesful_pings++;

	if (gMissionRecording && civ_car)
		SaveControlledPing(civ_id);
}

// [0x493c7d] [D]
void InitialiseAI(void)
{
	CIV_STATE *ai;
	int i;

	ai = civilian_ai_data;

	chair_count = 0;
	collision_frame = 0;
	succesful_pings = 0;
	failed_pings = 0;
	ParkedCarMinGap = 0;
	CloseCarMinGap = 0;
	MaxCloseCars = 0;
	CloseCarGap = 0;

	ClearMem(road_ping_points, sizeof(road_ping_points));
	ClearMem(path_ping_points, sizeof(path_ping_points));
	ClearMem(ApproachCars, sizeof(ApproachCars));
	ClearMem(ApproachDistance, sizeof(ApproachDistance));
	ClearMem(BlockerShots, sizeof(BlockerShots));

	ReturnedY = 0;
	ReturnedX = 0;
	ClosestCar = 0;
	CarApproaching = 0;
	num_road_ping_points = 0;
	num_civilian_ai_cars = 0;
	num_path_ping_points = 0;
	num_ai_cars_inactive = 0;
	num_ai_cars_active = 0;
	NextRoadCars[3] = 0;
	NextRoadCars[2] = 0;
	NextRoadCars[1] = 0;
	NextRoadCars[0] = 0;
	DirectionToBlock = 0;
	RoadBlockGap = 0;
	CloseCarGap = 5;
	MaxCloseCars = 10;
	CloseCarMinGap = 4;
	ParkedCarMinGap = 2;
	failed_pings = 0;
	succesful_pings = 0;
	max_traffic_density = gTrafficDensity;
	RoadBlockTimer = 0;
	LastCarsNeeded = 0;
	CarsNeededForBlock = 0;
	CarsSavedForBlock = 0;
	CarsNeededInitially = 0;
	AdjacentRoad = -1;
	RoadToBlock = -1;
	PlayerLastRoad = -1;
	LeadCarFinished = 0;
	LeadCarHit = 0;
	LeadCarLost = 0;
	RequestCopCar = 0;
	SpareCarsUsed = 0;

	BuildPreRotatedBoxes();
	ROADS_InitOneWayRoads();
	ROADS_InitLaneLookup();
	ROADS_CalculateBends();

	civ_ai_data_pt = &car_data[1];
	ClearMem(&car_data[1], 64 * sizeof(CAR_DATA));
	ClearMem(civilian_ai_data, 64 * sizeof(CIV_STATE));
	num_civilian_ai_cars = 0;

	for (i = 0; i < 64; i++, ai++)
	{
		civilian_slots[i] = i;
		ai->driving = 0;
		ai->id = i;
		ai->ctrlState = CIVSTATE_EMPTY;
	}
}

// [A]
// Sets the road and turn direction for the target lane SelectNextRoad picked.
static void SetTargetLane(CIV_STATE *ai, short *target_roads, unsigned char lane)
{
	ai->targetLane = lane;
	ai->targetRoad = target_roads[ai->targetLane];

	if (ai->targetLane == ai->currentLane)
		ai->turnDir = 0;
	else
		ai->turnDir = DIFF_ANGLES(ai->currentLane << 10, ai->targetLane << 10) < 0 ? -1 : 1;
}

// [0x493fd5] [D] [A]
// Picks the road the car takes at the junction ahead (RouteData): from the outer lane of a
// multi lane road also a turn, from the inner one straight on or the other turn; never back the
// way it came, a closed entry or against a one way road. Without a choice any open road; without
// any the car is removed.
// NOTE: the three blocks setting the target lane are the helper SetTargetLane.
int SelectNextRoad(CIV_STATE *ai)
{
	static const unsigned char order[4] = { 0, 2, 1, 3 };

	JENTRY *entries;
	short target_roads[4];
	char target_lanes[4];
	char allowed[4];
	unsigned char numLanes;
	unsigned char laneOffset;
	unsigned char lastLane;
	unsigned char nextLane;
	unsigned char lane;
	unsigned char i;
	int road;

	numLanes = 0;

	if (RouteData.junction == -1 || RouteData.junction == 0x3FFF)
	{
		// invalid junction, bye-bye!
		RemoveCivilianCar(ai->id);
		return 0;
	}

	if (Junctions[RouteData.junction].JWH)
		allowed[0] = allowed[1] = allowed[2] = allowed[3] = 1;
	else
		allowed[0] = allowed[1] = allowed[2] = allowed[3] = 0;

	if ((unsigned int)ai->currentRoad >= 0x4000)
	{
		allowed[0] = allowed[1] = allowed[2] = allowed[3] = 1;
	}
	else
	{
		laneOffset = ai->laneOffset;
		lastLane = RoadLaneInfo[ai->currentRoad].lanes[ai->currentLane] - 1;

		switch (ai->currentLane)
		{
			case 0:
				if (lastLane == 0)
				{
					allowed[0] = 1;
					allowed[1] = 1;
					allowed[3] = 1;
				}
				else if (laneOffset == 0)
				{
					allowed[0] = 1;
					allowed[1] = 1;
				}
				else
				{
					allowed[0] = 1;

					if (laneOffset == lastLane)
						allowed[3] = 1;
				}
				break;
			case 1:
				if (lastLane == 0)
				{
					allowed[0] = 1;
					allowed[1] = 1;
					allowed[2] = 1;
				}
				else if (laneOffset == 0)
				{
					allowed[2] = 1;
					allowed[1] = 1;
				}
				else
				{
					allowed[1] = 1;

					if (laneOffset == lastLane)
						allowed[0] = 1;
				}
				break;
			case 2:
				if (lastLane == 0)
				{
					allowed[2] = 1;
					allowed[1] = 1;
					allowed[3] = 1;
				}
				else if (laneOffset == 0)
				{
					allowed[2] = 1;
					allowed[3] = 1;
				}
				else
				{
					allowed[2] = 1;

					if (laneOffset == lastLane)
						allowed[1] = 1;
				}
				break;
			case 3:
				if (lastLane == 0)
				{
					allowed[0] = 1;
					allowed[3] = 1;
					allowed[2] = 1;
				}
				else if (laneOffset == 0)
				{
					allowed[3] = 1;
					allowed[0] = 1;
				}
				else
				{
					if (laneOffset == lastLane)
						allowed[2] = 1;

					allowed[3] = 1;
				}
				break;
		}
	}

	ai->maxSpeed = 130;
	ai->trafficLightPhaseId = JunctionLightsChanged + 2;
	ai->InJunction = 0;

	entries = Junctions[RouteData.junction].JunctionEntry;

	target_roads[0] = entries[0].ViaRoad;
	target_roads[1] = entries[1].ViaRoad;
	target_roads[2] = entries[2].ViaRoad;
	target_roads[3] = entries[3].ViaRoad;

	// the entries in the order 0, 2, 1, 3; a one way road is closed to the direction opposite to
	// its own (0x80 + (lane + 2) & 3)
	for (i = 0; i < 4; i++)
	{
		lane = order[i];
		road = target_roads[lane];

		if (!(entries[lane].JunctionFlags & 2)
			&& road != -1
			&& road != ai->currentRoad
			&& (unsigned char)Roads[road].IsOneWay != ((lane + 2) & 3) + 0x80
			&& allowed[lane])
		{
			target_lanes[numLanes++] = lane;
			lastLane = RoadLaneInfo[road].lanes[lane] - 1;

			if (ai->currentLane == lane && lastLane < (unsigned char)ai->laneOffset)
				numLanes--;
		}
	}

	if (numLanes == 0)
	{
		for (i = 0; i < 4; i++)
		{
			road = target_roads[i];

			if (!(entries[i].JunctionFlags & 2)
				&& road != -1
				&& road != ai->currentRoad
				&& (unsigned char)Roads[road].IsOneWay != ((i + 2) & 3) + 0x80)
			{
				target_lanes[numLanes++] = i;
			}
		}

		if (numLanes == 0)
		{
			RemoveCivilianCar(ai->id);
			return 0;
		}
	}

	if (numLanes == 1)
		ai->stateFlags |= CVST_FORWARD;

	nextLane = Random() % numLanes;

	SetTargetLane(ai, target_roads, target_lanes[nextLane]);

	if (numLanes == 3 && ai->turnDir < 0)
	{
		if (RouteData.junction & 1)
		{
			if (ai->targetLane == 2 || ai->targetLane == 3)
			{
				if (++nextLane >= numLanes)
					nextLane -= numLanes;

				SetTargetLane(ai, target_roads, target_lanes[nextLane]);
			}
		}
		else if (ai->targetLane <= 1)
		{
			if (++nextLane >= numLanes)
				nextLane -= numLanes;

			SetTargetLane(ai, target_roads, target_lanes[nextLane]);
		}
	}

	return 1;
}

// [0x494851] [D]
// Steers the car onto its target x (lanes 0, 2) or z (lanes 1, 3); snaps to it within 32 units
// and ends the lane change. Returns 1 if there was nothing to steer to.
int SteerCarIntoAllignment(CIV_STATE *ai, IVECTOR *pos, int *pos_z, char lane)
{
	CAR_DATA *cp;
	unsigned char confused;
	int dx;
	int dz;

	cp = &civ_ai_data_pt[ai->id];
	confused = 1;

	if (ai->targetX != INT_MAX && ai->speed && (lane == 0 || lane == 2))
	{
		dx = ai->targetX - cp->where.t[0];

		if (abs(dx) > 32)
		{
			if (ai->currentLane == 2)
			{
				if (ai->stateFlags & (CVST_INDICATE_RT | CVST_INDICATE_LT))
					cp->wheel_angle = dx / -8;
				else
					cp->wheel_angle = dx / -32;
			}
			else if (ai->stateFlags & (CVST_INDICATE_RT | CVST_INDICATE_LT))
			{
				cp->wheel_angle = dx / 8;
			}
			else
			{
				cp->wheel_angle = dx / 32;
			}

			confused = 0;
		}
		else
		{
			cp->where.t[0] = ai->targetX;
			ai->targetX = INT_MAX;
			ai->stateFlags &= ~(CVST_TURNING | CVST_INDICATE_LT);
			cp->wheel_angle = 0;
			cp->rotation.vy = 0;

			// BUG: right turns cause cars to slow down!
			if (ai->stateFlags & CVST_INDICATE_RT)
			{
				ai->stateFlags &= ~CVST_INDICATE_RT;
				ai->stateFlags |= CVST_PARKING;
			}
		}
	}

	if (ai->targetZ != INT_MAX && ai->speed && (lane == 1 || lane == 3))
	{
		dz = ai->targetZ - cp->where.t[2];

		if (abs(dz) > 32)
		{
			if (ai->currentLane == 1)
			{
				if (ai->stateFlags & (CVST_INDICATE_RT | CVST_INDICATE_LT))
					cp->wheel_angle = dz / -8;
				else
					cp->wheel_angle = dz / -32;
			}
			else if (ai->stateFlags & (CVST_INDICATE_RT | CVST_INDICATE_LT))
			{
				cp->wheel_angle = dz / 8;
			}
			else
			{
				cp->wheel_angle = dz / 32;
			}

			confused = 0;
		}
		else
		{
			cp->where.t[2] = ai->targetZ;
			ai->targetZ = INT_MAX;
			ai->stateFlags &= ~(CVST_TURNING | CVST_INDICATE_LT);
			cp->wheel_angle = 0;
			cp->rotation.vy = 0;

			// BUG: right turns cause cars to slow down!
			if (ai->stateFlags & CVST_INDICATE_RT)
			{
				ai->stateFlags &= ~CVST_INDICATE_RT;
				ai->stateFlags |= CVST_PARKING;
			}
		}
	}

	if (confused)
	{
		ai->targetX = INT_MAX;
		ai->targetZ = INT_MAX;
	}

	return confused;
}

// [0x494b3d] [D]
// Starts the turn into the target road at the junction.
void TurnToTarget(CIV_STATE *ai)
{
	GetTargetLaneCoords(ai->targetRoad, ai->targetLane, ai->turnDir, 2, ai->laneOffset);

	ai->stateFlags &= ~CVST_FORWARD;
	ai->targetX = ReturnedX;
	ai->targetZ = ReturnedY;
	ai->ctrlState = CIVSTATE_TURNING;
	ai->field_10 = 0;
}

// [0x494b9b] [D]
// A free civilian slot, -1 when gTrafficDensity are in use.
int AddCivilianCar(void)
{
	if (num_civilian_ai_cars == gTrafficDensity)
		return -1;

	return civilian_slots[num_civilian_ai_cars++];
}

// [0x494bce] [D]
// Frees the civilian slot (and its cop slot); a roadblock car ends the roadblock.
void RemoveCivilianCar(int id)
{
	unsigned char cop_slot;

	cop_slot = civilian_ai_data[id].IsCopCar;

	if (cop_slot)
	{
		cop_data[cop_slot - 1].active = 0;
		num_active_cops--;
		civilian_ai_data[id].IsCopCar = 0;
	}

	if (civilian_ai_data[id].ctrlState == CIVSTATE_BLOCKING)
		ResetRoadBlock();

	civilian_ai_data[id].ctrlState = CIVSTATE_EMPTY;
	civilian_ai_data[id].driving = 0;
	civilian_ai_data[id].isGangCar = 0;

	civilian_slots[--num_civilian_ai_cars] = id;
}

// [0x494c95] [D]
void CivilianState_EMPTY(CIV_STATE *ai)
{
}

// [0x494c9a] [D]
// A parked car pulls out now and then once its timer ran out.
void CivilianState_PARKED(CIV_STATE *ai)
{
	ai->driving = 0;

	if (ai->parkedTimer)
		ai->parkedTimer--;
	else if (ai->currentRoad != -1 && (Random() & 0x1F) == ai->id)
		TryUnparkingCar(ai);
}

// [0x494cf7] [D]
// Drives along the lane: keeps the heading of the lane (or steers onto the target of a lane
// change), starts the turn on reaching the junction, removes the car on an invalid junction,
// horns at the player driving past.
void CivilianState_MOVING(CIV_STATE *ai)
{
	CAR_DATA *cp;
	IVECTOR *pos;
	int *pos_z;
	int confused;
	int junc;
	int chan;
	int horn;

	if (ai->speed != 0)
	{
		if (ai->wrecked == 1)
		{
			if (ROADS_GetNextJunction(civ_ai_data_pt[ai->id].location.vx, civ_ai_data_pt[ai->id].location.vz, ai->currentLane) == 0)
			{
				ai->distAhead = RouteData.distAhead;
				ai->distAlongSegment = RouteData.distAlongSegment;
				ai->currentJunction = RouteData.junction;
				ai->pathFlags = RouteData.flags;
			}

			return;
		}

		pos = &civ_ai_data_pt[ai->id].location;
		pos_z = &civ_ai_data_pt[ai->id].location.vz;

		if (SetWheelAngle(ai, ai->currentLane << 10) >= 32)
			confused = 0;
		else
			confused = SteerCarIntoAllignment(ai, pos, pos_z, ai->currentLane);

		junc = ROADS_GetNextJunction(pos->vx, *pos_z, ai->currentLane);

		if ((confused && RouteData.junction == -1) || RouteData.junction == 0x3FFF)
		{
			// invalid junction - bye bye!
			RemoveCivilianCar(ai->id);
		}
		else if (junc == 1 && ai->InJunction)
		{
			// in junction
			TurnToTarget(ai);
		}
		else if (junc == 0)
		{
			// on road
			ai->distAhead = RouteData.distAhead;
			ai->distAlongSegment = RouteData.distAlongSegment;
			ai->currentJunction = RouteData.junction;
			ai->pathFlags = RouteData.flags;
			ai->laneOffset = GetLaneOffset(ai);

			if (ai->hornTimer)
				ai->hornTimer--;

			if (car_data[0].speed > 30 && ai->speed > 10 && PlayerCloseToCar(&civ_ai_data_pt[ai->id]) && ai->hornTimer == 0)
			{
				// BEEP BEEP!
				horn = ai->id & 3;

				if (horn == 3)
					horn = 2;

				ai->hornTimer = 20;

				cp = &civ_ai_data_pt[ai->id];

				chan = Play3DSound(horn + 25, -1, (float)cp->location.vx, (float)-cp->location.vy, (float)cp->location.vz);

				if (chan != -1)
					SetTrackingSound(chan, &cp->location);
			}
		}
	}
}

// [0x49501d] [D]
// Steers the car towards a direction (half the difference, at most 512); returns the remaining
// difference.
int SetWheelAngle(CIV_STATE *ai, int direction)
{
	int wheel_angle;
	int angle;
	int dir;

	wheel_angle = 0;
	dir = civ_ai_data_pt[ai->id].direction & 0xFFF;
	angle = DIFF_ANGLES(dir, direction);

	if (dir != direction)
	{
		if (abs(angle) >= 128)
			wheel_angle = (angle < 0 ? -1 : 1) << 9;
		else
			wheel_angle = angle / 2;
	}

	civ_ai_data_pt[ai->id].wheel_angle = wheel_angle;

	return abs(angle);
}

// [0x4950dc] [D]
// Turns into the target road: waits until close to the target lane (field_10), steers to its
// direction, then drives on in the new road.
void CivilianState_TURNING(CIV_STATE *ai)
{
	IVECTOR *pos;
	int *pos_z;
	int junc;

	if (ai->wrecked == 1 || ai->speed == 0)
		return;

	pos = &civ_ai_data_pt[ai->id].location;
	pos_z = &civ_ai_data_pt[ai->id].location.vz;

	junc = ROADS_GetRoadOrJunction(pos->vx, *pos_z);

	ai->currentRoad = junc;

	if (ai->field_10 == 0)
	{
		switch (ai->currentLane)
		{
			case 0:
			case 2:
				if (abs(*pos_z - ai->targetZ) >= ai->speed + 850)
					return;

				ai->field_10 = 1;
				break;
			case 1:
			case 3:
				if (abs(pos->vx - ai->targetX) >= ai->speed + 850)
					return;

				ai->field_10 = 1;
				break;
		}
	}

	if (SetWheelAngle(ai, ai->targetLane << 10) >= 32)
		return;

	if (junc & 0x4000)
	{
		SteerCarIntoAllignment(ai, pos, pos_z, ai->targetLane);
		return;
	}

	ai->ctrlState = CIVSTATE_MOVING;

	ai->currentLane = ai->targetLane;
	ai->currentRoad = ai->targetRoad;

	ROADS_GetNextJunction(pos->vx, *pos_z, ai->currentLane);

	ai->distAhead = RouteData.distAhead;
	ai->distAlongSegment = RouteData.distAlongSegment;
	ai->currentJunction = RouteData.junction;
	ai->pathFlags = RouteData.flags;

	SelectNextRoad(ai);

	if (ai->ctrlState == CIVSTATE_MOVING)
	{
		GetTargetLaneCoords(ai->currentRoad, ai->currentLane, ai->turnDir, 2, ai->laneOffset);

		ai->targetZ = ReturnedY;
		ai->targetX = ReturnedX;
	}
}

// [0x495320] [D]
void CivilianState_SWERVING_LEFT(CIV_STATE *ai)
{
	if (ai->parkedTimer)
	{
		civ_ai_data_pt[ai->id].wheel_angle = -575;
		ai->parkedTimer--;
	}
	else
	{
		ai->ctrlState = CIVSTATE_MOVING;
		ai->wrecked = 1;
	}
}

// [0x495378] [D]
void CivilianState_SWERVING_RIGHT(CIV_STATE *ai)
{
	if (ai->parkedTimer)
	{
		civ_ai_data_pt[ai->id].wheel_angle = 575;
		ai->parkedTimer--;
	}
	else
	{
		ai->ctrlState = CIVSTATE_MOVING;
		ai->wrecked = 1;
	}
}

// [0x4953d0] [D]
void CivilianState_LEADING(CIV_STATE *ai)
{
}

// [0x4953d5] [D]
void CivilianState_LEADREC(CIV_STATE *ai)
{
}

// [0x4953da] [D]
void CivilianState_BLOCKING(CIV_STATE *ai)
{
	ai->maxSpeed = 0;
	ai->speed = 0;
	ai->speedDamperTimer = 1;
	ai->blocked = 1;
}

// [0x495401] [D]
// Now and then a car in the outer lane of a long road with parking pulls over to the kerb
// (CVST_TURNING | CVST_INDICATE_RT), if no car is parked within 8 cells there.
void TryParkingCar(CIV_STATE *ai)
{
	CIV_STATE *other_ai;
	int kerbX;
	int kerbZ;
	int i;

	other_ai = civilian_ai_data;

	if ((ai->stateFlags & (CVST_TURNING | CVST_PARKING))
		|| (Random() & 7) != 0
		|| !(Roads[ai->currentRoad].Flags & 1)
		|| ai->laneOffset != 0
		|| ai->distAhead < 18000
		|| ai->distAlongSegment < 3000
		|| CarsParkedOnRoad(ai->currentRoad) != 0)
	{
		return;
	}

	GetTargetLaneCoords(ai->currentRoad, ai->currentLane, 0, 1, 0);

	kerbX = IntegerApplyRecip(ReturnedX);
	kerbZ = IntegerApplyRecip(ReturnedY);

	if (ai->currentLane & 1)
	{
		for (i = 0; i < gTrafficDensity; i++, other_ai++)
		{
			if (other_ai->ctrlState != CIVSTATE_EMPTY && ai->id != other_ai->id && other_ai->currentZ == kerbZ)
			{
				if (other_ai->currentX < ai->currentX)
				{
					if (abs(ai->currentX - other_ai->currentX) < 8)
						return;
				}
				else if (abs(other_ai->currentX - ai->currentX) < 8)
				{
					return;
				}
			}
		}
	}
	else
	{
		for (i = 0; i < gTrafficDensity; i++, other_ai++)
		{
			if (other_ai->ctrlState != CIVSTATE_EMPTY && ai->id != other_ai->id && other_ai->currentX == kerbX)
			{
				if (other_ai->currentZ < ai->currentZ)
				{
					if (abs(ai->currentZ - other_ai->currentZ) < 8)
						return;
				}
				else if (abs(other_ai->currentZ - ai->currentZ) < 8)
				{
					return;
				}
			}
		}
	}

	switch (ai->currentLane)
	{
		case 0:
			ai->targetX = ReturnedX + 494;
			break;
		case 1:
			ai->targetZ = ReturnedY - 446;
			break;
		case 2:
			ai->targetX = ReturnedX - 494;
			break;
		case 3:
			ai->targetZ = ReturnedY + 446;
			break;
	}

	ai->stateFlags |= CVST_TURNING | CVST_INDICATE_RT;
}

// [0x4956f0] [D]
// A parked car pulls out into the lane (650 units over) if no car is within 5 cells behind it.
void TryUnparkingCar(CIV_STATE *ai)
{
	CIV_STATE *other_ai;
	int xpos;
	int zpos;
	int laneX;
	int slot;

	other_ai = civilian_ai_data;

	xpos = civ_ai_data_pt[ai->id].location.vx;
	zpos = civ_ai_data_pt[ai->id].location.vz;

	switch (ai->currentLane)
	{
		case 0:
			xpos -= 650;
			break;
		case 1:
			zpos += 650;
			break;
		case 2:
			xpos += 650;
			break;
		case 3:
			zpos -= 650;
			break;
	}

	laneX = IntegerApplyRecip(xpos);

	ai->currentX = IntegerApplyRecip(civ_ai_data_pt[ai->id].location.vx + 750);
	ai->currentZ = IntegerApplyRecip(civ_ai_data_pt[ai->id].location.vz - 750);

	if (ai->currentLane & 1)
	{
		for (slot = 0; slot < gTrafficDensity; slot++, other_ai++)
		{
			if (ai->id == other_ai->id || other_ai->ctrlState == CIVSTATE_EMPTY)
				continue;

			if (ai->currentLane == 1)
			{
				if (ai->currentRoad == other_ai->currentRoad
					&& other_ai->currentZ == ai->currentZ
					&& other_ai->currentX >= ai->currentX
					&& abs(other_ai->currentX - ai->currentX) < 5)
				{
					return;
				}
			}
			else if (ai->currentRoad == other_ai->currentRoad
				&& other_ai->currentZ == ai->currentZ
				&& ai->currentX >= other_ai->currentX
				&& abs(ai->currentX - other_ai->currentX) < 5)
			{
				return;
			}
		}
	}
	else
	{
		for (slot = 0; slot < gTrafficDensity; slot++, other_ai++)
		{
			if (ai->id == other_ai->id || other_ai->ctrlState == CIVSTATE_EMPTY)
				continue;

			if (ai->currentLane != 0)
			{
				if (ai->currentRoad == other_ai->currentRoad
					&& other_ai->currentX == laneX
					&& ai->currentZ >= other_ai->currentZ
					&& abs(ai->currentZ - other_ai->currentZ) < 5)
				{
					return;
				}
			}
			else if (ai->currentRoad == other_ai->currentRoad
				&& other_ai->currentX == laneX
				&& other_ai->currentZ >= ai->currentZ
				&& abs(other_ai->currentZ - ai->currentZ) < 5)
			{
				return;
			}
		}
	}

	if (ai->currentLane & 1)
	{
		ai->targetX = INT_MAX;
		ai->targetZ = zpos;
	}
	else
	{
		ai->targetX = xpos;
		ai->targetZ = INT_MAX;
	}

	ai->stateFlags = CVST_TURNING | CVST_DRIVING | CVST_INDICATE_LT;
	ai->ctrlState = CIVSTATE_MOVING;
	ai->driving = 1;

	ai->maxSpeed = 130;
	ai->parked = 0;

	ROADS_GetNextJunction(xpos, zpos, ai->currentLane);

	ai->distAhead = RouteData.distAhead;

	SelectNextRoad(ai);
}

// [0x495b01] [D]
// Now and then a car moves over to a random lane one cell away, if no car is within 5 cells there.
void TryChangingLanes(CIV_STATE *ai)
{
	CIV_STATE *other_ai;
	int laneX;
	int laneZ;
	int i;

	other_ai = civilian_ai_data;

	if ((ai->stateFlags & (CVST_TURNING | CVST_PARKING)) || (Random() & 3) != 0)
		return;

	GetTargetLaneCoords(ai->currentRoad, ai->currentLane, 0, 2, 255);

	laneX = IntegerApplyRecip(ReturnedX);
	laneZ = IntegerApplyRecip(ReturnedY);

	// NOTE: redundant, handled in ControlAI
	ai->currentX = IntegerApplyRecip(civ_ai_data_pt[ai->id].location.vx + 750);
	ai->currentZ = IntegerApplyRecip(civ_ai_data_pt[ai->id].location.vz - 750);

	if (ai->currentLane & 1)
	{
		if (abs(ai->currentZ - laneZ) == 1)
		{
			for (i = 0; i < gTrafficDensity; i++, other_ai++)
			{
				if (ai->id != other_ai->id
					&& ai->currentRoad == other_ai->currentRoad
					&& other_ai->currentZ == laneZ
					&& abs(ai->currentX - other_ai->currentX) < 5)
				{
					return;
				}
			}

			ai->stateFlags |= CVST_TURNING;
			ai->targetZ = ReturnedY;
		}
	}
	else if (abs(ai->currentX - laneX) == 1)
	{
		for (i = 0; i < gTrafficDensity; i++, other_ai++)
		{
			if (ai->id != other_ai->id
				&& ai->currentRoad == other_ai->currentRoad
				&& other_ai->currentX == laneX
				&& abs(ai->currentZ - other_ai->currentZ) < 5)
			{
				return;
			}
		}

		ai->stateFlags |= CVST_TURNING;
		ai->targetX = ReturnedX;
	}
}

// [0x495d9a] [D]
// BUGGED: cars will pretty much never park since turning cars count as parked cars
int CarsParkedOnRoad(int road)
{
	CIV_STATE *ai;
	int count;
	int slot;

	count = 0;
	ai = civilian_ai_data;

	for (slot = 0; slot < gTrafficDensity; slot++, ai++)
	{
		if (ai->currentRoad == road
			&& (ai->ctrlState == CIVSTATE_PARKED
				|| (ai->stateFlags & CVST_INDICATE_RT)
				|| (ai->stateFlags & CVST_INDICATE_LT)
				|| (ai->stateFlags & CVST_PARKING)))
		{
			count++;
		}
	}

	return count;
}

// [0x495e2b] [D]
// Pings in new cars while fewer than the traffic density are active (20 while recording a chase);
// when replaying one, the recorded cars of this frame.
void ControlPings(int *densityControl, int numDensityControl)
{
	int civ_id;
	int i;

	if (LeadCarHit)
		return;

	if (gMissionReplaying)
	{
		civ_id = 0;

		if (mission.objective != 0xFF0 && mission.objective != 0x7FFF)
		{
			while (civ_id != -1)
			{
				civ_id = DoControlledPing();

				if (civ_id != -1)
					succesful_pings++;
			}
		}

		return;
	}

	// cool feature from rolling demo that's no longer present -
	// set specific areas where density is controlled
	// if ( numDensityControl )
	// {
	//   for ( i = 0; i < numDensityControl; ++i )
	//   {
	//     if ( densityControl[0] < camera_position.vx
	//       && densityControl[2] > camera_position.vx
	//       && densityControl[1] < camera_position.vz
	//       && densityControl[3] > camera_position.vz )
	//     {
	//       max_traffic_density = densityControl[4];
	//       break;
	//     }
	//     densityControl += 5;
	//   }
	// }
	// however, the following code basically undoes everything above :P
	// ( and yes, this is how it is in the rolling demo )
	if (gMissionRecording)
		max_traffic_density = 20;
	else
		max_traffic_density = gTrafficDensity;

	if (num_ai_cars_active < max_traffic_density)
	{
		if (num_ai_cars_inactive <= max_traffic_density)
		{
			for (i = 0; i < num_ai_cars_inactive; i++)
				AddCarRelativeToPlayer();
		}
		else
		{
			for (i = 0; i < max_traffic_density; i++)
				AddCarRelativeToPlayer();
		}
	}
}

// [0x495f1e] [D]
// The first car in the same road, lane and lane offset at most minDist road map cells ahead
// (type 0: moving cars, distance either way; else any car ahead), -1 if none.
int FindCarAhead(CIV_STATE *ai, int minDist, int type)
{
	CIV_STATE *other_ai;
	int dist;
	int i;

	dist = 1000;
	other_ai = civilian_ai_data;

	if (ai->field_10)
		return -1;

	ai->currentX = IntegerApplyRecip(civ_ai_data_pt[ai->id].location.vx + 750);
	ai->currentZ = IntegerApplyRecip(civ_ai_data_pt[ai->id].location.vz - 750);

	if (type)
	{
		// this kind of check goes unused
		for (i = 0; i < gTrafficDensity; i++, other_ai++)
		{
			if (ai->id != other_ai->id
				&& other_ai->ctrlState != CIVSTATE_EMPTY
				&& ai->currentRoad == other_ai->currentRoad
				&& ai->currentLane == other_ai->currentLane
				&& ai->laneOffset == other_ai->laneOffset)
			{
				switch (ai->currentLane)
				{
					case 0:
						if (ai->currentZ > other_ai->currentZ)
							dist = ai->currentZ - other_ai->currentZ;
						break;
					case 1:
						if (other_ai->currentX > ai->currentX)
							dist = other_ai->currentX - ai->currentX;
						break;
					case 2:
						if (other_ai->currentZ > ai->currentZ)
							dist = other_ai->currentZ - ai->currentZ;
						break;
					case 3:
						if (ai->currentX > other_ai->currentX)
							dist = ai->currentX - other_ai->currentX;
						break;
				}

				if (dist >= 0 && dist <= minDist)
					return i;
			}
		}

		return -1;
	}

	for (i = 0; i < gTrafficDensity; i++, other_ai++)
	{
		if (ai->id != other_ai->id
			&& other_ai->ctrlState != CIVSTATE_PARKED
			&& other_ai->ctrlState != CIVSTATE_EMPTY
			&& ai->currentRoad == other_ai->currentRoad
			&& ai->currentLane == other_ai->currentLane
			&& ai->laneOffset == other_ai->laneOffset)
		{
			other_ai->currentX = IntegerApplyRecip(civ_ai_data_pt[other_ai->id].location.vx + 750);
			other_ai->currentZ = IntegerApplyRecip(civ_ai_data_pt[other_ai->id].location.vz - 750);

			switch (ai->currentLane)
			{
				case 0:
					if (other_ai->currentZ > ai->currentZ)
						dist = other_ai->currentZ - ai->currentZ;
					break;
				case 1:
					if (other_ai->currentX > ai->currentX)
						dist = other_ai->currentX - ai->currentX;
					break;
				case 2:
					if (ai->currentZ > other_ai->currentZ)
						dist = ai->currentZ - other_ai->currentZ;
					break;
				case 3:
					if (ai->currentX > other_ai->currentX)
						dist = ai->currentX - other_ai->currentX;
					break;
			}

			if (abs(dist) <= minDist)
				return i;
		}
	}

	return -1;
}

// [0x49635b] [D]
// Plans a roadblock across the player's road (at least 20 cells long, running in dir) and the
// road next to it: the cars it needs are saved by AddCarRelativeToPlayer, then SetupRoadBlock
// places them.
int RequestRoadBlock(unsigned char dir)
{
	unsigned short road;
	int vx;
	int vz;

	if (RoadBlockTimer)
		return 0;

	if (RoadToBlock != -1)
		return 0;

	vx = car_data[0].location.vx;
	vz = car_data[0].location.vz;

	road = ROADS_GetRoadOrJunction(car_data[0].location.vx, car_data[0].location.vz);

	if (road & 0xC000)
		return 0;

	if (Roads[road].Length < 20)
		return 0;

	if (dir & 1)
	{
		if (RoadBounds[road].dir == 0)
			return 0;
	}
	else if (RoadBounds[road].dir == 1)
	{
		return 0;
	}

	CarsNeededInitially = (unsigned char)RoadLaneInfo[road].lanes[(dir + 2) & 3] + (unsigned char)RoadLaneInfo[road].lanes[dir];

	CarsNeededForBlock = CarsNeededInitially;
	AdjacentRoad = ROADS_FindAdjacentRoad(vx, vz, dir);

	// NOTE: overwrites the gap ROADS_FindAdjacentRoad just set, so the extra car below is never
	// needed (as in the binary)
	RoadBlockGap = -1;

	if (AdjacentRoad != -1)
	{
		CarsNeededInitially += (unsigned char)RoadLaneInfo[AdjacentRoad].lanes[dir] + (unsigned char)RoadLaneInfo[AdjacentRoad].lanes[(dir + 2) & 3];

		if ((unsigned char)RoadBlockGap != 255)
			CarsNeededInitially++;
	}

	ExtraBlockCars = CarsNeededInitially + 1;
	CarsNeededInitially += 1 + CarsNeededInitially;

	if (CarsNeededInitially == 0)
		return 0;

	LastCarsNeeded = CarsNeededInitially;
	CarsSavedForBlock = 0;
	RoadToBlock = road;
	DirectionToBlock = dir;
	RoadBlockTimer = RoadblockTimer;

	return 1;
}

// [0x496586] [D]
// No object in the road map cell (info bits 26 - 29).
int OccupancyGridNotBlocked(int x, int z)
{
	return (ROADS_GetMapInfo(x, z) & 0x3C000000) == 0;
}

// [0x4965b7] [D]
// Places the planned roadblock 30000 units ahead of the player: a row of cop cars across the
// road (and the adjacent road), turned at random; on higher difficulties a second row behind
// them across the whole block.
int SetupRoadBlock(void)
{
	static int dirs[4] = { 0, 1024, 2048, 3072 };

	CIV_STATE *ai;
	IVECTOR blockers[16];
	IVECTOR tmp;
	int oldRequest;
	int num;
	int vx;
	int vz;
	int i;
	int j;
	int k;
	char dir;

	num = 0;

	CarsNeededInitially = 0;

	vx = car_data[0].location.vx;
	vz = car_data[0].location.vz;
	dir = DirectionToBlock;

	// NOTE: redundant, doesn't use anything from this function (also missing variable assignment)
	ROADS_GetNextJunction(car_data[0].location.vx, car_data[0].location.vz, DirectionToBlock);

	switch (dir)
	{
		case 0:
			vx = 1500 * RoadBounds[RoadToBlock].x1;
			vz += 30000;
			break;
		case 1:
			vx += 30000;
			vz = -1500 * RoadBounds[RoadToBlock].y2;
			break;
		case 2:
			vx = 1500 * RoadBounds[RoadToBlock].x1;
			vz -= 30000;
			break;
		case 3:
			vx -= 30000;
			vz = -1500 * RoadBounds[RoadToBlock].y2;
			break;
	}

	if (!IsRoadBlockPossible(vx, vz, RoadToBlock, dir))
		return 0;

	if (AdjacentRoad != -1 && !IsRoadBlockPossible(vx, vz, AdjacentRoad, dir))
		return 0;

	oldRequest = RequestCopCar;
	RequestCopCar = 0;

	for (i = 0; i < CarsSavedForBlock; i++)
		RemoveCivilianCar(BlockerShots[i]);

	CarsSavedForBlock -= ExtraBlockCars;

	while (CarsNeededForBlock)
	{
		if (OccupancyGridNotBlocked(vx, vz))
		{
			i = AllocateAndPlaceCarAtPosition(vx, -250, vz, dirs[(dir + 2) & 3] + 512 - (Random() & 0x3FF) + 1024, 1);

			ai = &civilian_ai_data[i];
			ai->model = 1;
			ai->currentRoad = RoadToBlock;

			blockers[num].vx = vx;
			blockers[num++].vz = vz;
		}

		if (dir & 1)
		{
			vz += 1500;
			vx += 128 - (unsigned char)Random();
		}
		else
		{
			vx += 1500;
			vz += 128 - (unsigned char)Random();
		}

		CarsSavedForBlock--;
		CarsNeededForBlock--;
	}

	if (AdjacentRoad == -1)
	{
		RoadBlockTimer = RoadblockTimer;
		RequestCopCar = oldRequest;

		return 1;
	}

	if (RoadBlockGap == 0)
	{
		if (OccupancyGridNotBlocked(vx, vz))
		{
			i = AllocateAndPlaceCarAtPosition(vx, -250, vz, dirs[(dir + 2) & 3] + 512 - (Random() & 0x3FF) + 1024, 1);

			ai = &civilian_ai_data[i];
			ai->model = 1;
			ai->currentRoad = -1;
			ai->wrecked = 1;

			blockers[num].vx = vx;
			blockers[num++].vz = vz;
		}

		CarsSavedForBlock--;
	}

	switch (dir)
	{
		case 0:
			vx = 1500 * RoadBounds[AdjacentRoad].x1;
			break;
		case 1:
			vz = -1500 * RoadBounds[AdjacentRoad].y2;
			break;
		case 2:
			vx = 1500 * RoadBounds[AdjacentRoad].x1;
			break;
		case 3:
			vz = -1500 * RoadBounds[AdjacentRoad].y2;
			break;
	}

	while (CarsSavedForBlock)
	{
		if (OccupancyGridNotBlocked(vx, vz))
		{
			i = AllocateAndPlaceCarAtPosition(vx, -250, vz, dirs[(dir + 2) & 3] + 512 - (Random() & 0x3FF) + 1024, 1);

			ai = &civilian_ai_data[i];
			ai->model = 1;
			ai->currentRoad = AdjacentRoad;

			blockers[num].vx = vx;
			blockers[num++].vz = vz;
		}

		if (dir & 1)
		{
			vz += 1500;
			vx += 128 - (unsigned char)Random();
		}
		else
		{
			vx += 1500;
			vz += 128 - (unsigned char)Random();
		}

		CarsSavedForBlock--;
	}

	if (gDifficulty)
	{
		// sort the cars along the block
		for (j = 0; j < num - 1; j++)
		{
			for (k = 0; k < num - 1; k++)
			{
				if ((dir & 1) ? blockers[k].vz > blockers[k + 1].vz : blockers[k].vx > blockers[k + 1].vx)
				{
					tmp = blockers[k];
					blockers[k] = blockers[k + 1];
					blockers[k + 1] = tmp;
				}
			}
		}

		if (num > 1)
		{
			if (dir & 1)
			{
				vx = blockers[0].vx - 950;

				for (vz = blockers[0].vz - 750; vz < blockers[num - 1].vz; vz += 1500)
				{
					if (OccupancyGridNotBlocked(vx, vz))
					{
						if (gDifficulty == 1)
							i = AllocateAndPlaceCarAtPosition(vx, -250, vz, 64 - (Random() & 0x1F) + dirs[(dir + 2) & 3], 1);
						else
							i = AllocateAndPlaceCarAtPosition(vx, -250, vz, dirs[(dir + 2) & 3] + 64 - (Random() & 0x1F) + 1024, 1);

						ai = &civilian_ai_data[i];
						ai->model = 1;
					}
				}
			}
			else
			{
				vx = blockers[0].vx + 750;
				vz = blockers[0].vz - 950;

				while (vx < blockers[num - 1].vx)
				{
					if (OccupancyGridNotBlocked(vx, vz))
					{
						if (gDifficulty == 1)
							i = AllocateAndPlaceCarAtPosition(vx, -250, vz, 64 - (Random() & 0x1F) + dirs[(dir + 2) & 3], 1);
						else
							i = AllocateAndPlaceCarAtPosition(vx, -250, vz, dirs[(dir + 2) & 3] + 64 - (Random() & 0x1F) + 1024, 1);

						ai = &civilian_ai_data[i];
						ai->model = 1;
					}

					vx += 1500;
				}
			}
		}
	}

	RoadBlockTimer = RoadblockTimer;
	RequestCopCar = oldRequest;

	return 1;
}

// [0x497072] [D]
// The roadblock position is on the planned road (or a junction), 30000 units clear ahead, and no
// car on the road within 2000 of it; cancels the roadblock when the road changed.
int IsRoadBlockPossible(int x, int z, int road, char dir)
{
	CIV_STATE *ai;
	int i;

	ai = civilian_ai_data;

	if (ROADS_GetNextJunction(x, z, GetCarHeading(car_data[0].direction)) != 0)
		return 0;

	if ((RouteData.road != -1 && RouteData.road != RoadToBlock) || RouteData.distAhead < 30000)
	{
		RemoveRoadBlock();
		return 0;
	}

	for (i = 0; i < gTrafficDensity; i++, ai++)
	{
		if (ai->ctrlState != CIVSTATE_EMPTY && ai->currentRoad == road)
		{
			if (dir & 1)
			{
				if (abs(x - civ_ai_data_pt[i].location.vx) < 2000)
					return 0;
			}
			else if (abs(z - civ_ai_data_pt[i].location.vz) < 2000)
			{
				return 0;
			}
		}
	}

	return 1;
}

// [0x4971c7] [D]
void ControlRoadBlock(void)
{
	if (RoadBlockTimer)
		RoadBlockTimer--;
}

// [0x4971e2] [D]
void ResetRoadBlock(void)
{
	RoadToBlock = -1;
	CarsNeededInitially = 0;
	CarsSavedForBlock = 0;
}

// [0x497205] [D]
// Frees the cars saved for the roadblock and cancels it.
void RemoveRoadBlock(void)
{
	int i;

	for (i = 0; i < CarsSavedForBlock; i++)
		RemoveCivilianCar(BlockerShots[i]);

	RoadBlockTimer = 0;
	RoadToBlock = -1;
	CarsSavedForBlock = 0;
	CarsNeededInitially = 0;
}

// [0x497269] [D]
// The player drives the opposite way past the car: within 5000 units, heading within +-356 of
// the opposite direction, and inside the area ahead of the player (three planes).
int PlayerCloseToCar(CAR_DATA *cp)
{
	int dx;
	int dz;

	if (((car_data[0].direction - cp->direction) & 0xFFF) > 2404 || ((car_data[0].direction - cp->direction) & 0xFFF) < 1692)
		return 0;

	dx = cp->location.vx - car_data[0].location.vx;
	dz = cp->location.vz - car_data[0].location.vz;

	if (dx < -5000 || dx > 5000 || dz < -5000 || dz > 5000)
		return 0;

	if (dx * isin(car_data[0].direction) + dz * icos(car_data[0].direction) < -3686400)
		return 0;

	// NB: can't prettyify these in the decompiler
	if (dx * isin(car_data[0].direction + 974) + dz * isin(car_data[0].direction + 1998) < -2457600)
		return 0;

	return dx * isin(car_data[0].direction - 974) + dz * isin(car_data[0].direction + 50) >= -2457600;
}

// [0x4973fd] [D]
// NOTE: no name in the database and no callers (only an unreferenced thunk): the car's position
// on the inner side of four planes (vx, vz, vw), as built in SlowCarInWayCheck. IDA types the
// argument MATRIX *.
int sub_4973FD(CAR_DATA *cp, VECTOR4 *planes)
{
	float vx;
	float vz;

	vx = (float)cp->location.vx;
	vz = (float)cp->location.vz;

	if (vx * planes[0].vx + vz * planes[0].vz > planes[0].vw)
		return 0;

	if (vx * planes[1].vx + vz * planes[1].vz > planes[1].vw)
		return 0;

	if (vx * planes[2].vx + vz * planes[2].vz > planes[2].vw)
		return 0;

	return vx * planes[3].vx + vz * planes[3].vz <= planes[3].vw;
}

// [0x4974d2] [D]
// Runs the traffic each frame: the state of each car, its physics, removes cars that got more
// than 27 cells from the player (or the lead car), counts the cars and pings in new ones. While
// replaying a chase, fails the mission when the player is 19 cells from the lead car.
void ControlAI(void)
{
	CIV_STATE *ai;
	CAR_DATA *cp;
	int ping_pos_x;
	int ping_pos_z;
	int max_civs;
	int slot;
	int dir;
	int dx;
	int dz;

	cp = civ_ai_data_pt;
	ai = civilian_ai_data;

	if (gMissionRecording || gMissionReplaying)
	{
		gTrafficDensity = 32;
		ping_pos_x = car_data[65].location.vx;
		ping_pos_z = car_data[65].location.vz;
	}
	else
	{
		ping_pos_x = car_data[0].location.vx;
		ping_pos_z = car_data[0].location.vz;
	}

	GetPingPoints();

	max_civs = gTrafficDensity;

	if (gMissionReplaying && !LeadCarHit)
	{
		dx = IntegerApplyRecip(abs(car_data[0].location.vx - ping_pos_x));
		dz = IntegerApplyRecip(abs(car_data[0].location.vz - ping_pos_z));

		if (dz * dz + dx * dx > 363 && !LeadCarLost && !LeadCarHit && !LeadCarFinished)
		{
			// lead car got too far away from player
			LeadCarLost = 1;
			mission.objective = 4080;
			mission.failed_reason = 3;
			mission.pause_timer = 150;
		}
	}

	if (gDoTraining)
		return;

	for (slot = 0; slot < max_civs; slot++, ai++, cp++)
	{
		dir = cp->direction;

		CivilianStateFunction[ai->ctrlState](ai);

		if (ai->ctrlState == CIVSTATE_EMPTY)
			continue;

		if (ai->ctrlState != CIVSTATE_PARKED)
		{
			cp->location.vx += 450 * isin(dir) / 4096;
			cp->location.vz += 450 * icos(dir) / 4096;
		}

		dx = IntegerApplyRecip(abs(cp->location.vx - ping_pos_x));
		dz = IntegerApplyRecip(abs(cp->location.vz - ping_pos_z));

		if (ai->ping_slot)
		{
			if (dz * dz + dx * dx <= 729 && !ai->pinged)
				InitPingedCar(cp, ai);
		}
		else if (dz * dz + dx * dx > 729)
		{
			if (!gMissionReplaying)
				RemoveCivilianCar(slot);

			if (!gMissionReplaying || (!LeadCarLost && !LeadCarHit && !LeadCarFinished))
				continue;
		}

		ai->currentX = IntegerApplyRecip(civ_ai_data_pt[ai->id].location.vx + 750);
		ai->currentZ = IntegerApplyRecip(civ_ai_data_pt[ai->id].location.vz - 750);

		if (ai->ctrlState == CIVSTATE_PARKED && !ai->wrecked && !ai->ping_slot)
		{
			if ((unsigned char)ai->pingOutTimer >= 100)
				continue;

			ai->pingOutTimer++;
		}

		if (ai->ctrlState == CIVSTATE_PURSUIT)
		{
			// update pursuer
			StepCarPhysics(cp, 0);
		}
		else
		{
			if (cp->wheel_speed <= 0)
				ai->speed = -cp->speed;
			else
				ai->speed = cp->speed;

			if (slot < 64)
			{
				// update civilian
				ControlSpeed(ai, cp);
				StepCarPhysics(cp, 0);
			}

			if (ai->ctrlState == CIVSTATE_MOVING && ai->wrecked != 1 && (game_frame & 1) && ai->currentRoad != -1)
			{
				if (ai->distAhead > 18000 && ai->targetX == INT_MAX && ai->targetZ == INT_MAX)
					TryChangingLanes(ai);
				else
					TryParkingCar(ai);
			}
		}

		if (ai->IsCopCar)
			ControlCopDetectModes(ai);
	}

	num_ai_cars_active = 0;
	num_ai_cars_inactive = 0;

	ai = civilian_ai_data;

	for (slot = 0; slot < 64; slot++, ai++)
	{
		if (ai->ctrlState != CIVSTATE_EMPTY)
			num_ai_cars_active++;
		else
			num_ai_cars_inactive++;
	}

	if (gMissionReplaying)
		num_civilian_ai_cars = num_ai_cars_active;

	ControlPings(ping_areas, 0);
}

// [0x497bd3] [D]
// The player drove from PlayerLastRoad into the road through a traffic light junction on red.
// NOTE: going from entry i to entry i - 1 is never an offence - by the entry order (0 north,
// 1 east, ...) a right turn, allowed on red.
int CheckRedLights(int road)
{
	short junction;
	short JunctionB;
	int i;
	int j;

	junction = -1;

	if (PlayerLastRoad == -1)
		return 0;

	if (PlayerLastRoad == road)
		return 0;

	JunctionB = Roads[road].JunctionB;

	for (i = 0; i < 4; i++)
	{
		if (Junctions[Roads[road].JunctionA].JunctionEntry[i].ViaRoad == PlayerLastRoad)
		{
			junction = Roads[road].JunctionA;
			break;
		}
	}

	if (junction == -1)
	{
		if (JunctionB == -1)
			return 0;

		for (i = 0; i < 4; i++)
		{
			if (Junctions[JunctionB].JunctionEntry[i].ViaRoad == PlayerLastRoad)
			{
				junction = Roads[road].JunctionB;
				break;
			}
		}

		if (junction == -1)
			return 0;
	}

	if (Junctions[junction].Type != 1)
		return 0;

	for (j = 0; j < 4 && Junctions[junction].JunctionEntry[j].ViaRoad != road; j++)
		;

	if (j == ((i + 3) & 3))
		return 0;

	if (i & 1)
	{
		if (JunctionLightsWE == 1 && (unsigned int)JunctionLightsTimer < 295)
			return 1;
	}
	else if (JunctionLightsNS == 1 && (unsigned int)JunctionLightsTimer < 295)
	{
		return 1;
	}

	return 0;
}
