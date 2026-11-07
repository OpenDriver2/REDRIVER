/*
 * cops.c - the police: cop cars chasing the player, roadblock requests, felony
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "cops.c", 0x4a7b30 - 0x4aa567).
 *
 * The cops are civilian cars (civilian_ai_data, CIV_STATE.IsCopCar = 1 + the cop_data slot).
 * ControlCops (every frame) decides whether the cops see the player (ControlCopDetection), adjusts
 * the felony rating and asks ai.c for another cop car (RequestCopCar) while fewer are active than
 * the rating calls for. A cop out of a pursuit drives as a civilian (ControlCopDetectModes); in
 * CopState_PURSUIT it steers at a point ahead on its route (pathfind.c, recalculated every 8th
 * frame) or, when close, straight at where the player will be (CarTail).
 */

#include <stdlib.h>

#include "dr_types.h"
#include "cops.h"
#include "dr_globals.h"
#include "ai.h"
#include "felony.h"
#include "general.h"
#include "handling.h"
#include "map.h"
#include "overlay.h"
#include "pathfind.h"
#include "roadbits.h"
#include "shell.h"
#include "testmath.h"
#include "minicutscene.h"
#include "frontend/cheats.h"
#include "glaunch.h"

void PlayCopSpeechOnce(int id);

// the distance approximation the cop code uses: the longer side plus half the shorter
#define APPROX_DIST(adx, adz)	((adx) >= (adz) ? (adz) / 2 + (adx) : (adx) / 2 + (adz))

// 128 * sqrt(1 + (i / 64)^2): FindCost's distance from the longer side and the ratio of the sides
unsigned char sqtbl[66] = {
	128, 128, 128, 128, 128, 128, 128, 128, 128, 129, 129, 129, 130, 130, 131, 131,
	131, 132, 132, 133, 134, 134, 135, 136, 136, 137, 138, 138, 139, 140, 141, 142,
	143, 144, 144, 145, 146, 147, 148, 149, 150, 152, 153, 154, 155, 156, 157, 158,
	160, 161, 162, 163, 164, 166, 167, 168, 170, 171, 172, 174, 175, 176, 178, 179,
	181, 0
};

int LastCopMessage;
int pathStraight;					// NOTE: not in this file's data block; owner by its users
int LastHeading;					// NOTE: owner guessed (cops.c 2, felony.c 2 references; in this block)
COP_DATA gCopData;
int max_active_cops;
int last_known_x;
int last_known_z;
int CopsAllowed;
int RoadblockTimer;					// NOTE: owner guessed (in this block; ai.c references it more)
int player_position_known;
char byte_11AA554;					// NOTE: unnamed in IDA; only InitCops clears it
int gCopsSpeedScale;
int gCopsAutoSpeedScale;
int OutOfSightCount;
int num_active_cops;
int last_carz;
int last_carx;
int valid_target_position;
IVECTOR CarTail;
int player_next_junction;
int gBatterPlayer;
int gCopsPowerScale;
char CopWorkMem[10000];
int gCopsImmortal;
int dword_11ACCB4;					// NOTE: unnamed in IDA; only cleared (InitCops, InitialiseFelonyData)
int dword_11ACCB8;					// NOTE: unnamed in IDA; only InitCops clears it
int dword_11ACCBC;					// NOTE: unnamed in IDA; only InitCops clears it
int suggested_active_cops;
int CopsCanSeePlayer;
COP cop_data[8];
int gPuppyDogCop;
int CopsAutoPowerScale;

// [0x4a7b30] [D]
// Roadblock felony triggers: 682, 746, ... (+64), then INT_MAX.
void InitialiseCopData(COP_DATA *pCopData)
{
	int loop;
	int *pTrigger;
	int trigger;

	trigger = 682;
	pTrigger = pCopData->trigger;

	for (loop = 0; loop < 8; loop++)
	{
		*pTrigger++ = trigger;
		trigger += 64;
	}

	// NOTE: the original writes this through pTrigger, i.e. trigger[8]
	pCopData->field_38 = 0x7FFFFFFF;
}

// [0x4a7b86] [D]
// The heading (0 - 3) the car is moving in: reversed when it goes backwards.
char GetCarDirectionOfTravel(CAR_DATA *cp)
{
	char heading;

	heading = GetCarHeading(cp->direction);

	if (cp->wheel_speed < 0)
		return (heading + 2) & 3;

	return heading;
}

// [0x4a7bc3] [D] [A]
// player_position_known: 2 seen this frame or last, 1 newly seen, -1 / 0 lost (512 frames out of
// sight). Cops that cannot see the player and are hidden for 100 frames drop out of the pursuit.
// NOTE: [A] the map clears with a white flash when the pursuit ends, as in Driver 2
void ControlCopDetection(void)
{
	int ccz;
	int ccx;
	COP *cop;
	VECTOR4 vec;
	int loop;
	int spotted;
	CIV_STATE *ai;
	CAR_DATA *pCarData;
	char *vis;

	vis = CopWorkMem;

	if (gCheatFlags.RailroadCops)
	{
		player_position_known = 2;
		CopsCanSeePlayer = 1;
		return;
	}

	spotted = 0;
	cop = cop_data;

	vec.vx = (float)car_data[0].location.vx;
	vec.vz = (float)car_data[0].location.vz;

	GetVisSetAtPosition(&vec, vis, &ccx, &ccz);

	ManageCopSightData(&CopSightData);

	if (player_position_known > 0)
		player_position_known = 2;
	else
		player_position_known = 1;

	CopsCanSeePlayer = 0;

	for (loop = 0; loop < 8; loop++, cop++)
	{
		if (cop->active && cop->dying == 0)
		{
			civilian_ai_data[cop->pCarData->id - 1].wrecked = 0;

			vec.vx = (float)cop->location.vx;
			vec.vz = (float)cop->location.vz;

			if (cop->close_pursuit ||
				(PositionVisible(&vec, vis, ccx, ccz) && CopCanSeePlayer(&CopSightData, cop)))
			{
				spotted = 1;
				CopsCanSeePlayer = 1;
				break;
			}
		}
	}

	if (spotted == 1)
	{
		OutOfSightCount = 0;
	}
	else if (OutOfSightCount < 512)
	{
		OutOfSightCount++;
	}
	else if (OutOfSightCount == 512)
	{
		player_position_known = -1;
		OutOfSightCount = 513;

		if (FelonyRating >= 682)
		{
			PlayCopSpeech(0x800A);
			ClearMapFlash();
		}
	}
	else
	{
		player_position_known = 0;
	}

	if (player_position_known <= 0)
	{
		cop = cop_data;

		for (loop = 0; loop < 8; loop++, cop++)
		{
			if (cop->active == 0)
				continue;

			pCarData = cop->pCarData;

			vec.vx = (float)pCarData->location.vx;
			vec.vz = (float)pCarData->location.vz;

			if (cop->num_route_entries <= 4 && cop->dying == 0 && PositionVisible(&vec, vis, ccx, ccz) == 0 &&
				++cop->hiddenTimer > 100)
			{
				ai = &civilian_ai_data[(unsigned char)pCarData->id - 1];

				cop->hiddenTimer = 0;

				if (ai->ctrlState == CIVSTATE_PURSUIT)
					ai->ctrlState = CIVSTATE_MOVING;
			}
		}
	}
}

// [0x4a7e98] [D]
// NOTE: no callers
int CanAddCop(void)
{
	return num_active_cops < 8;
}

// [0x4a7eaf] [D]
// NOTE: no callers
void RemoveCop(COP *cop)
{
	civilian_ai_data[cop->pCarData->id - 1].IsCopCar = 0;

	cop->active = 0;

	if (--num_active_cops == 0 && OutOfSightCount < 512)
		OutOfSightCount = 512;
}

// [0x4a7f09] [D]
// A free cop slot (1 - 8, cleared), -1 if all 8 are active.
int GetCopCarSlot(void)
{
	COP *cop;
	int i;

	cop = cop_data;

	for (i = 1; i <= 8; i++)
	{
		if (!cop->active)
		{
			ClearMem(cop, sizeof(COP));
			cop->id = i - 1;

			return i;
		}

		cop++;
	}

	return -1;
}

// [0x4a7f70] [D]
// NOTE: no callers. Unlike ai.c it uses cop_data[slot] (not slot - 1) and IsCopCar = slot + 1,
// so slot 8 would be past cop_data (as in the original)
COP *AddCop(CIV_STATE *ai, CAR_DATA *cp)
{
	int slot;
	COP *cop;

	cop = NULL;

	slot = GetCopCarSlot();

	if (slot != -1)
	{
		cop = &cop_data[slot];

		ai->IsCopCar = slot + 1;

		InitCopState(cp, cop, slot);

		num_active_cops++;
		cop_respawn_timer = 0;
	}

	return cop;
}

// [0x4a7fda] [D]
void InitCops(void)
{
	COP *cop;
	int i;

	cop = cop_data;

	InitPathFind();

	ClearMem(CopWorkMem, sizeof(CopWorkMem));

	CarTail.vz = 0;
	CarTail.vy = 0;
	CarTail.vx = 0;

	pathStraight = 0;
	byte_11AA554 = 0;
	player_next_junction = 0;
	OutOfSightCount = 0;
	dword_11ACCBC = 0;
	dword_11ACCB8 = 0;
	suggested_active_cops = 0;
	last_known_z = 0;
	last_known_x = 0;
	max_active_cops = 0;
	last_carz = 0;
	last_carx = 0;
	dword_11ACCB4 = 0;
	valid_target_position = 0;
	player_position_known = 0;
	num_active_cops = 0;
	cop_respawn_timer = 0;
	LastHeading = -1;
	CopsCanSeePlayer = 0;
	dword_D90C5C = 0;
	suggested_active_cops = gMissionRecording == 0 && gMissionReplaying == 0;
	CopsAllowed = 1;
	last_carx = 0;
	last_carz = 0;
	OverlayOccupancyGrid = 0;

	for (i = 0; i < 8; i++)
	{
		ClearMem(cop, sizeof(COP));

		cop->id = i;
		cop->active = 0;
		cop->routeInMemory = 0;

		cop++;
	}

	player_position_known = 0;
	OutOfSightCount = 513;
	valid_target_position = 0;
	LastCopMessage = -1;

	InitialiseCopData(&gCopData);
	InitialiseFelonyData(&gFelonyData);
	ManageCopSightData(&CopSightData);

	max_active_cops = 0;
	InitialiseAStar();
}

// [0x4a8217] [D]
// Whether the civilian car is in the cop's way: within 15000, not a cop (or a dying one), ahead
// within 90 degrees and getting within 1200 of the cop in the next (at most 20) steps.
int AddCopCar(COP *cop, CIV_STATE *ai)
{
	CAR_DATA *cp;
	int cx, cz;
	int copx, copz;
	int dist;
	int steps;
	int copSpeed, copDir;
	int carSpeed, carDir;
	int copdx, copdz;
	int cardx, cardz;
	int adx, adz;

	cp = &civ_ai_data_pt[ai->id];

	cx = cp->location.vx;
	cz = cp->location.vz;

	copx = cop->location.vx;
	copz = cop->location.vz;

	adx = abs(cx - copx);
	adz = abs(cz - copz);
	dist = APPROX_DIST(adx, adz);

	if (dist >= 15000)
		return 0;

	if (ai->IsCopCar == 0)
		return 1;

	if (cop_data[(unsigned char)ai->IsCopCar - 1].dying == 1)
		return 1;

	if (abs(DIFF_ANGLES(cop->direction, Arc_Tan(cx - copx, cz - copz))) >= 1024)
		return 0;

	copSpeed = cop->speed;
	copDir = cop->direction;
	carSpeed = cp->speed;
	carDir = cp->direction;

	if (copSpeed != 0)
		steps = dist / copSpeed;
	else
		steps = 0;

	if (steps >= 4)
	{
		steps /= 4;
		copdx = (isin(copDir) * copSpeed) >> 10;
		copdz = (icos(copDir) * copSpeed) >> 10;
		cardx = (isin(carDir) * carSpeed) >> 10;
		cardz = (icos(carDir) * carSpeed) >> 10;
	}
	else
	{
		copdx = (isin(copDir) * copSpeed) >> 12;
		copdz = (icos(copDir) * copSpeed) >> 12;
		cardx = (isin(carDir) * carSpeed) >> 12;
		cardz = (icos(carDir) * carSpeed) >> 12;
	}

	if (steps > 20)
		steps = 20;

	while (steps != 0)
	{
		copx += copdx;
		copz += copdz;

		cx += cardx;
		cz += cardz;

		adx = abs(copx - cx);
		adz = abs(copz - cz);

		if (APPROX_DIST(adx, adz) < 1200)
		{
			cop->speedUpTimer = 20;
			return 1;
		}

		steps--;
	}

	return 0;
}

// [0x4a858d] [D]
// close_cars: the civilian cars in the cop's way (AddCopCar), for the occupancy grid.
int AddClosestCopCars(COP *cop)
{
	CAR_DATA *cp;
	int i;
	CIV_STATE *ai;

	ai = civilian_ai_data;
	cp = &car_data[1];

	num_close_cars = 0;

	for (i = 0; i < 64; i++)
	{
		if (ai->ctrlState != 0 && (unsigned char)ai->IsCopCar != cop->id + 1 && AddCopCar(cop, ai))
			close_cars[num_close_cars++] = i;

		cp++;
		ai++;
	}

	return num_close_cars != 0;
}

// [0x4a8644] [D]
// A far cop heads for the junction ahead of the player instead, unless half of the other cops do
// already (field_CC8).
int ControlCopTargetting(COP *cop)
{
	int jx, jz;
	int tx, tz;
	int adx, adz;
	int i;
	int num_cops;
	int num_cutting;
	COP *pCop;

	// BUG: variable is never updated!
	// (player_next_junction stays 0 from InitCops, so this is junction 0)
	if (player_next_junction != -1)
	{
		jx = JunctionPositions[player_next_junction].x;
		jz = -JunctionPositions[player_next_junction].y;

		tx = car_data[0].location.vx;
		tz = car_data[0].location.vz;

		adx = abs(jx - cop->location.vx);
		adz = abs(jz - cop->location.vz);

		if (APPROX_DIST(adx, adz) < 40500)
		{
			adx = abs(tx - cop->location.vx);
			adz = abs(tz - cop->location.vz);

			if (APPROX_DIST(adx, adz) > 45000)
			{
				pCop = cop_data;
				num_cops = 0;
				num_cutting = 0;

				for (i = 0; i < 8; i++)
				{
					if (pCop->active)
					{
						num_cops++;

						if (pCop->id != cop->id && pCop->field_CC8)
							num_cutting++;
					}

					pCop++;
				}

				if (num_cutting < num_cops >> 1)
				{
					cop->targetPosX = jx;
					cop->targetPosZ = jz;
					cop->field_CC8 = 1;
					return 1;
				}
			}
		}
	}

	cop->field_CC8 = 0;
	return 0;
}

// [0x4a882a] [D]
// Sets the cop's target (the player, or where the player was last seen) and builds the occupancy
// grid around it; *dvx, *dvz: the target cell relative to the cop's cell.
void ControlGridGeneration(COP *cop, int *dvx, int *dvz, int targetPlayer)
{
	unsigned short *info;
	int cost;
	int x, z;
	int carx, carz;
	int region;
	int cell;
	int lostTarget;
	int px, pz;

	x = car_data[0].location.vx + RoadMapUnitXMid + 750;
	z = RoadMapUnitZMid + 750 - car_data[0].location.vz;

	x = IntegerApplyRecip(x);
	z = IntegerApplyRecip(z);

	region = ((x / road_region_size) & 1) | ((2 * (regions_down - 1 - z / road_region_size)) & 2);
	cell = road_region_size * (z % road_region_size) + x % road_region_size;

	// NOTE: the high half of the player's road info cell and the low half of the next cell: bit 0
	// comes from the next cell (CopyRun takes both from the same cell)
	info = (unsigned short *)&RoadMapInfoRegions[region][cell] + 1;

	lostTarget = 0;

	if (targetPlayer)
	{
		cop->targetPosX = car_data[0].location.vx;
		cop->targetPosZ = car_data[0].location.vz;
	}
	else
	{
		cost = CostValues[(info[1] & 1) | ((info[0] & 0x3C00) >> 9)];

		cop->targetPosX = last_known_x;
		cop->targetPosZ = last_known_z;

		if (player_position_known == 0 || (valid_target_position && cost == 4096))
		{
			lostTarget = 1;
		}
		else
		{
			cop->targetPosX = car_data[0].location.vx;
			last_known_x = car_data[0].location.vx;

			cop->targetPosZ = car_data[0].location.vz;
			last_known_z = car_data[0].location.vz;

			valid_target_position = 1;
		}

		if (ControlCopTargetting(cop))
			lostTarget = 1;
	}

	x = cop->targetPosX + RoadMapUnitXMid + 750;
	z = RoadMapUnitZMid + 750 - cop->targetPosZ;

	cop->targetX = IntegerApplyRecip(x);
	x = cop->targetX;

	cop->targetZ = IntegerApplyRecip(z);
	z = cop->targetZ;

	// NOTE: the player's cell relative to the target is computed but not used
	if (lostTarget == 1)
	{
		px = car_data[0].location.vx + RoadMapUnitXMid + 750;
		pz = RoadMapUnitZMid + 750 - car_data[0].location.vz;

		px = IntegerApplyRecip(px) - x;
		pz = IntegerApplyRecip(pz) - z;
	}
	else
	{
		pz = 0;
		px = 0;
	}

	(void)px;
	(void)pz;

	carx = cop->location.vx + RoadMapUnitXMid + 750;
	carz = RoadMapUnitZMid + 750 - cop->location.vz;

	carx = IntegerApplyRecip(carx);
	carz = IntegerApplyRecip(carz);

	last_carx = x;
	last_carz = z;

	GenerateOccupancyGrid(x, z, cop);

	cop->field_CC0 = 1;

	*dvx = x - carx;
	*dvz = z - carz;
}

// [0x4a8bea] [D]
// Every 8th frame per cop: a new route (A* on a new grid) when the cop has none, has reached its
// end or the target moved over 3000. A route in memory is kept up to its second original node
// past the current one and the search restarts from there.
void CalculateRoadRouteToPlayer(COP *cop)
{
	int adx, adz;
	COP_ROUTE_ENTRY *next;
	int found;
	int closeCars;
	int dx, dz;
	int count;
	int original;
	COP_ROUTE_ENTRY *rep;
	int dvz;
	int recalc;
	int dvx;
	int offset;

	closeCars = AddClosestCopCars(cop);

	recalc = 0;

	if (cop->routeInMemory)
	{
		if (cop->current_node == cop->field_18 || (cop->current_node <= 1 && !closeCars))
		{
			if (cop->field_CC8 == 0)
			{
				adx = abs(cop->targetPosX - car_data[0].location.vx);
				adz = abs(cop->targetPosZ - car_data[0].location.vz);

				if (APPROX_DIST(adx, adz) > 3000)
					recalc = 1;
			}
		}
		else
		{
			recalc = 1;
		}
	}
	else
	{
		recalc = 1;
	}

	if (recalc != 1)
		return;

	ControlGridGeneration(cop, &dvx, &dvz, 0);

	num_route_entries = 0;

	if (CostGrid[1512] == 4096)
		return;

	if (cop->current_node > cop->num_route_entries - 2)
		cop->routeInMemory = 0;

	if (cop->routeInMemory == 1)
	{
		original = 0;
		num_nodes_to_copy = 0;

		count = --cop->current_node;

		for (rep = cop->route_entries; count != 0; count--)
			rep = rep->next;

		while (rep)
		{
			num_nodes_to_copy++;

			next = rep->next;

			if (rep->inserted_node == 0)
				original++;

			if (original >= 2 && (next == NULL || next->inserted_node == 0))
				break;

			rep = rep->next;
		}

		if (cop->num_route_entries - cop->current_node <= num_nodes_to_copy)
			cop->routeInMemory = 0;
	}

	if (cop->routeInMemory == 1)
	{
		count = num_nodes_to_copy + cop->current_node;
		rep = cop->route_entries;

		while (count-- != 0)
		{
			if (rep->next)
				rep = rep->next;
		}

		// the last kept node in the new grid
		offset = rep->offset;

		dx = offset % 55 - 27 - (last_carx - cop->last_carx);
		dz = offset / 55 - 27 - (last_carz - cop->last_carz);

		found = GetRouteAStar(cop, dx, dz, dx + 55 * (dz + 27) + 27);
	}
	else
	{
		offset = 55 * (27 - dvz) - dvx + 27;
		found = GetRouteAStar(cop, offset % 55 - 27, offset / 55 - 27, offset);
	}

	if (found == 1 && num_route_entries > 1)
	{
		cop->recalcTimer = 120;
		cop->current_node = 1;
		cop->field_18 = 1;
		cop->num_route_entries = num_route_entries;
		cop->routeInMemory = 1;
	}
	else
	{
		cop->routeInMemory = 0;
	}

	cop->last_carx = last_carx;
	cop->last_carz = last_carz;
}

// [0x4a8f70] [D]
void ControlCops(void)
{
	if (gCheatFlags.Immunity)
		FelonyRating = 0;

	if (gCheatFlags.RailroadCops)
	{
		FelonyRating = 4095;
		RoadblockTimer = 300;
		gCopData.roadblockTrigger = 0;
	}

	if (CopsAllowed == 0 || gMissionSetting || gMissionRecording || gMissionReplaying || gDoTraining)
	{
		player_position_known = 0;
		CopsCanSeePlayer = 0;
		RequestCopCar = 0;
		return;
	}

	if (FelonyRating == 4095)
		gBatterPlayer = 1;

	ControlCopDetection();
	AdjustFelony(&gFelonyData);
	ControlRoadBlock();

	// one cop below the pursuit rating, then one more per 853 points up to max_active_cops
	if (FelonyRating < 682)
	{
		suggested_active_cops = 1;
	}
	else
	{
		suggested_active_cops = (FelonyRating - 682) / 853 + 1;

		if (suggested_active_cops > max_active_cops)
			suggested_active_cops = max_active_cops;
	}

	if (num_active_cops < suggested_active_cops && max_active_cops != 0)
	{
		if (++cop_respawn_timer > gCopRespawnTime)
			RequestCopCar = 1;
	}
	else
	{
		cop_respawn_timer = 0;
	}

	if (OverlayOccupancyGrid)
		DisplayOccupancyGrid();
}

// [0x4a90e0] [D]
// Moves the cop's route position (current_node, route) to the segment it is on; returns the
// distance along it, 0 (and drops the route) past the last but one segment.
int sub_4A90E0(COP *cd)
{
	int adx, adz;
	int dnx, dnz;
	int rx, rz;
	int seglen;
	int along;
	int current_node;
	COP_ROUTE_ENTRY *next;
	int x, z;
	COP_ROUTE_ENTRY *rep;

	x = cd->location.vx;
	z = cd->location.vz;

	if (cd->routeInMemory == 0)
		return 0;

	rep = cd->route_entries;

	for (current_node = cd->current_node; current_node != 0; current_node--)
		rep = rep->next;

	while ((next = rep->next) != NULL)
	{
		rx = rep->x;
		rz = rep->z;

		dnx = next->x - rx;
		dnz = next->z - rz;

		adx = abs(dnx);
		adz = abs(dnz);

		if (adx < adz)
		{
			int t = adx;
			adx = adz;
			adz = t;
		}

		// the segment length: longer side + 13/32 of the shorter
		seglen = ((13 * adz) >> 5) + adx;

		if (seglen > 0)
		{
			along = (dnz * (z - rz) + dnx * (x - rx)) / seglen;

			if (along < seglen)
			{
				cd->route = rep;
				return along;
			}
		}

		if (cd->current_node > cd->num_route_entries - 3)
			break;

		cd->current_node++;

		rep = next;
	}

	cd->routeInMemory = 0;

	return 0;
}

// [0x4a9267] [D]
// The point distance along the route from cd->route; past its end, towards CarTail.
// pathStraight: the segment and the two after it have the same direction.
void FindPointOnPath(COP *cd, int distance, IVECTOR *target)
{
	int dvx, dvz;
	int tx, tz;
	int stepsize;
	int rdvx, rdvz;
	int rdx, rdz;
	int rx, rz;
	COP_ROUTE_ENTRY *next;
	COP_ROUTE_ENTRY *rep;

	rep = cd->route;

	if (rep == NULL)
	{
		cd->routeInMemory = 0;
		return;
	}

	while ((next = rep->next) != NULL)
	{
		rx = rep->x;
		rz = rep->z;
		rdx = next->x - rx;
		rdz = next->z - rz;

		rdvx = abs(rdx);
		rdvz = abs(rdz);

		if (rdvx < rdvz)
		{
			int t = rdvx;
			rdvx = rdvz;
			rdvz = t;
		}

		stepsize = ((13 * rdvz) >> 5) + rdvx;

		if (distance < stepsize)
		{
			if (distance < 0)
				distance = 0;

			target->vx = distance * rdx / stepsize + rx;
			target->vz = distance * rdz / stepsize + rz;

			pathStraight = rep != NULL && next != NULL && rep->direction == next->direction && next->next != NULL &&
				next->direction == next->next->direction;
			return;
		}

		distance -= stepsize;
		rep = rep->next;
	}

	tx = rep->x;
	tz = rep->z;

	dvx = abs(CarTail.vx - tx);
	dvz = abs(CarTail.vz - tz);

	if (dvx < dvz)
	{
		int t = dvx;
		dvx = dvz;
		dvz = t;
	}

	stepsize = ((13 * dvz) >> 5) + dvx;

	if (distance < 0)
		distance = 0;

	// NOTE: divides by 0 when CarTail is on the last node (as in the original)
	target->vx = distance * (CarTail.vx - tx) / stepsize + tx;
	target->vz = distance * (CarTail.vz - tz) / stepsize + tz;

	pathStraight = 0;
}

// [0x4a9540] [D]
// A cop out of a pursuit: joins it once the rating is 682 and the player's position is known;
// until then its position is tracked and the grid kept around the player.
void ControlCopDetectModes(CIV_STATE *ai)
{
	CAR_DATA *cp;
	int dvz;
	COP *cop;
	int dvx;

	if (FelonyRating >= 682 && player_position_known > 0)
		ai->ctrlState = CIVSTATE_PURSUIT;

	if (ai->ctrlState == CIVSTATE_PURSUIT)
		return;

	cop = &cop_data[(unsigned char)ai->IsCopCar - 1];
	cp = &civ_ai_data_pt[ai->id];

	cop->location.vx = cp->location.vx;
	cop->location.vz = cp->location.vz;
	cop->speed = cp->speed;
	cop->direction = cp->direction;
	cop->justPinged = 0;

	if (game_frame % 8 == (unsigned char)ai->IsCopCar - 1)
		ControlGridGeneration(cop, &dvx, &dvz, 1);
}

// [0x4a962e] [D]
void AddFelonyPoints(int howMuch)
{
	IncreaseFelonyRating(howMuch);

	if (FelonyRating >= 3241)
		PlayCopSpeechOnce(0x8003);
	else if (FelonyRating >= 2388)
		PlayCopSpeechOnce(0x8002);
	else if (FelonyRating >= 1535)
		PlayCopSpeechOnce(0x8001);
	else if (FelonyRating >= 682)
		PlayCopSpeechOnce(0x8000);
}

// [0x4a96a9] [D]
void PlayCopSpeechOnce(int id)
{
	if (id != LastCopMessage)
	{
		LastCopMessage = id;
		PlayCopSpeech(id);
	}
}

// [0x4a96ce] [D]
// The distance from (x, z) to the player moved by the relative velocity / 256.
int FindCost(int x, int z, int dvx, int dvz)
{
	int tx, tz;
	int dx, dz;

	tx = abs(x - (dvx / 256 + car_data[0].location.vx));
	tz = abs(z - (dvz / 256 + car_data[0].location.vz));

	dx = tx;
	dz = tz;

	if (tx < tz)
	{
		dx = tz;
		dz = tx;
	}

	if (dx != 0)
		return sqtbl[(dz << 6) / dx] * dx / 128;

	return 0;
}

// [0x4a97ac] [D]
void CopState_PURSUIT(CIV_STATE *ai)
{
	static int speed1[3] = { 290, 300, 310 };
	static int speed2[3] = { 340, 380, 420 };

	int maxPower;
	int speedDif;
	int steerdif;
	int b;
	int slidevel;
	iVectNT path;
	int framesToImpact;
	int targetZone;
	int _cz;
	IVECTOR targetPoint;
	int _cx;
	int dvz;
	int targetFound;
	int dvx;
	int dist;
	int along;
	int pdx, pdz;
	int fvx, fvz;
	IVECTOR doordir;
	COP *cop;
	CAR_DATA *cp;
	int i;
	int currentSpeed;

	cop = &cop_data[(unsigned char)ai->IsCopCar - 1];
	cp = &civ_ai_data_pt[ai->id];

	if (CopsAllowed == 0 || CutSceneActive)
	{
		cp->thrust = 0;
		return;
	}

	if (cop->dying == 1)
	{
		cp->thrust = 0;

		if (cop->health > 0.01f)
			cop->health -= 0.01f;

		return;
	}

	if (gCopsImmortal)
	{
		cop->totalDamage = 0;
	}
	else
	{
		cop->totalDamage = 0;

		// NB: this explains why cops die so easily... WTF!
		for (i = 0; i < 6; i++)
			cop->totalDamage += cp->damage[i];
	}

	if (cop->totalDamage > gCopCarWipedOutDamage)
	{
		cop->dying = 1;
		cp->thrust = 0;
		return;
	}

	cop->dying = 0;

	cop->location.vx = cp->location.vx;
	cop->location.vz = cp->location.vz;

	cop->speed = cp->speed;
	cop->direction = cp->direction;

	currentSpeed = (cp->wheel_speed + 2048) >> 12;

	_cx = cp->location.vx;
	_cz = cp->location.vz;

	dvx = car_data[0].st.n.linearVelocity[0] - cp->st.n.linearVelocity[0];
	dvz = car_data[0].st.n.linearVelocity[2] - cp->st.n.linearVelocity[2];

	doordir.vx = cp->where.m[0][0];
	doordir.vy = cp->where.m[1][0];
	doordir.vz = cp->where.m[2][0];

	targetFound = 0;
	pathStraight = 0;

	if (cp->speed < 5 && cop->desiredSpeed != 0)
		cop->stuckTimer++;
	else
		cop->stuckTimer = 0;

	if (cp->wheelsOnGround >= 3)
		cop->airbourneTimer = 0;
	else
		cop->airbourneTimer++;

	if (cop->airbourneTimer > 200)
	{
		cop->dying = 1;
		return;
	}

	// stuck: back off (or drive on) for 22 frames, steering the next of 8 ways
	if (cop->stuckTimer > 10 && cop->recoveryTimer == 0)
	{
		cop->recoveryTimer = 22;

		cop->lastRecoverStrategy &= 7;
		cop->lastRecoverStrategy++;

		cop->desiredSpeed = cop->desiredSpeed >= 0 ? -300 : 300;
		cop->desiredSteerAngle = (2 * (((5 * cop->lastRecoverStrategy) >> 1) & 3) - 3) << 7;

		cop->stuckTimer = 0;
	}

	dist = FindCost(_cx, _cz, dvx, dvz);

	cop->close_pursuit = dist < 2000 || (cop->routeInMemory == 0 && dist < 8400);

	if (cop->recoveryTimer > 0)
	{
		cop->recoveryTimer--;
	}
	else
	{
		if (dist >= 12000)
		{
			CarTail.vx = car_data[0].location.vx;
			CarTail.vz = car_data[0].location.vz;
		}
		else
		{
			// the frames until the cop is closest to the player, 0 - 60
			pdx = car_data[0].location.vx - _cx;
			pdz = car_data[0].location.vz - _cz;
			fvx = (dvx + 2048) >> 12;
			fvz = (dvz + 2048) >> 12;

			framesToImpact = (-pdx * fvx - pdz * fvz) / (fvx * fvx + fvz * fvz + 1);

			if (gBatterPlayer == 0)
				framesToImpact = 3 * framesToImpact / 4;

			if (framesToImpact < 0)
				framesToImpact = 0;
			else if (framesToImpact > 60)
				framesToImpact = 60;

			CarTail.vx = ((framesToImpact * car_data[0].st.n.linearVelocity[0] + 2048) >> 12) + car_data[0].location.vx;
			CarTail.vz = ((framesToImpact * car_data[0].st.n.linearVelocity[2] + 2048) >> 12) + car_data[0].location.vz;

			// ram the player's side, or aim behind the player
			if (gBatterPlayer)
			{
				CarTail.vx += car_data[0].where.m[0][0] / 32;
				CarTail.vz += car_data[0].where.m[2][0] / 32;
			}
			else
			{
				CarTail.vx -= car_data[0].where.m[0][2] / 16;
				CarTail.vz -= car_data[0].where.m[2][2] / 16;
			}
		}

		if (cop->close_pursuit)
		{
			targetPoint.vx = CarTail.vx;
			targetPoint.vz = CarTail.vz;

			cop->routeInMemory = 0;
			cop->targetPosX = car_data[0].location.vx;
			cop->targetPosZ = car_data[0].location.vz;

			cop->targetX = (cop->targetPosX + RoadMapUnitXMid + 750) / 1500;
			cop->targetZ = (RoadMapUnitZMid + 750 - cop->targetPosZ) / 1500;

			targetFound = 1;
		}
		else if (cop->routeInMemory)
		{
			ShowRoute(cop->route_entries, 0);

			along = sub_4A90E0(cop);

			FindPointOnPath(cop, along, &targetPoint);

			// off the route sideways: drop it
			if (abs((doordir.vz * (targetPoint.vz - _cz) + doordir.vx * (targetPoint.vx - _cx)) >> 12) > 3400)
			{
				cop->routeInMemory = 0;
			}
			else if (cop->routeInMemory)
			{
				FindPointOnPath(cop, 10 * cp->speed + 1000 + along, &targetPoint);
				targetFound = 1;
			}
		}
	}

	if (targetFound == 0)
		targetPoint = CarTail;

	targetPoint.vx = (targetPoint.vx + 2 * cop->targetHistory[1].vx - cop->targetHistory[3].vx) >> 1;
	targetPoint.vz = (targetPoint.vz + 2 * cop->targetHistory[1].vz - cop->targetHistory[3].vz) >> 1;

	cop->targetHistory[3] = cop->targetHistory[2];
	cop->targetHistory[2] = cop->targetHistory[1];
	cop->targetHistory[1] = cop->targetHistory[0];
	cop->targetHistory[0] = targetPoint;

	if (gTimeInWater == 0)
		targetFound = 0;

	if (targetFound)
	{
		// n: the target to the side (doordir), less the sideways slide; t: ahead
		slidevel = (cp->st.n.linearVelocity[0] * doordir.vx + cp->st.n.linearVelocity[2] * doordir.vz + 2048) >> 12;

		path.n = (doordir.vx * (targetPoint.vx - _cx) + doordir.vz * (targetPoint.vz - _cz)) >> 12;
		path.t = (-(targetPoint.vx - _cx) * doordir.vz + (targetPoint.vz - _cz) * doordir.vx) >> 12;

		path.n -= slidevel / 198;

		// 0 ahead, 1 behind (close pursuit), 2 left, 3 right
		if (path.n > 0)
		{
			if (path.t > 2 * path.n)
				targetZone = 0;
			else if (4 * path.t > path.n && cop->close_pursuit)
				targetZone = 1;
			else
				targetZone = 3;
		}
		else
		{
			if (path.t > -2 * path.n)
				targetZone = 0;
			else if (4 * path.t > -path.n && cop->close_pursuit)
				targetZone = 1;
			else
				targetZone = 2;
		}

		path.n += slidevel / 198;

		cp->fourWheelDrive = (cop->desiredSpeed ^ currentSpeed) > 0;

		switch (targetZone)
		{
			case 0:
				// zoneFrnt
				if (pathStraight)
					cop->desiredSpeed = speed2[gDifficulty];
				else
					cop->desiredSpeed = speed1[gDifficulty];

				cop->desiredSpeed = ((((gCopsAutoSpeedScale * FelonyRating) >> 12) + gCopsSpeedScale) * cop->desiredSpeed) >> 12;

				if (gPuppyDogCop && cop->close_pursuit)
				{
					if (dist <= 4000)
						cop->desiredSpeed = car_data[0].speed + 10;
					else
						cop->desiredSpeed = (dist - 4000) * (cop->desiredSpeed - (car_data[0].speed + 10)) / 4000 +
							car_data[0].speed + 10;
				}

				cop->desiredSteerAngle = (path.n << 9) / path.t;
				break;
			case 1:
				// zoneBack
				cop->desiredSpeed = -100;
				cop->desiredSteerAngle = (currentSpeed ^ path.n) >= 0 ? 284 : -284;
				break;
			case 2:
			case 3:
				// zoneLeft | zoneRight
				if (gBatterPlayer && cop->close_pursuit && cop->desiredSpeed < 0)
					cop->desiredSpeed = -110;
				else
					cop->desiredSpeed = 110;

				cop->desiredSteerAngle = (currentSpeed ^ path.n) >= 0 ? 512 : -512;
				break;
		}

		cp->fourWheelDrive = 1;
	}
	else if (cop->recoveryTimer < 1)
	{
		cop->desiredSpeed = 0;
		cop->desiredSteerAngle = 0;
	}

	if (cop->speedUpTimer)
		cop->speedUpTimer--;

	// calculate rubberbanding
	if (dist < 4096 && cop->desiredSpeed > 0 && gBatterPlayer == 0)
	{
		b = car_data[0].speed - 20;

		if (b < 20)
			b = 20;

		cop->desiredSpeed = (cop->desiredSpeed * dist + b * (4096 - dist) + 2048) >> 12;
	}

	speedDif = cop->desiredSpeed - currentSpeed;
	maxPower = (gDifficulty + 8) << 10;

	if (pathStraight)
		maxPower += (gDifficulty + 4) << 10;

	maxPower = ((((CopsAutoPowerScale * FelonyRating) >> 12) + gCopsPowerScale) * maxPower) >> 12;

	if (cop->field_5C)
		maxPower /= 2;

	if (speedDif < -50)
		cp->thrust = -maxPower;
	else if (speedDif > 50)
		cp->thrust = maxPower;
	else
		cp->thrust = maxPower * speedDif / 50;

	if (cp->speed < 48)
		cp->thrust = ((cp->speed + 80) * cp->thrust) >> 7;

	if (cp->fourWheelDrive == 1)
		cp->thrust >>= 1;

	steerdif = cop->desiredSteerAngle - cp->wheel_angle;

	if (steerdif < -200)
		steerdif = -200;
	else if (steerdif > 200)
		steerdif = 200;

	cp->wheel_angle += steerdif;

	cop->recalcTimer--;

	if (game_frame % 8 == (unsigned char)ai->IsCopCar - 1)
	{
		if (cop->recalcTimer < 0)
			cop->routeInMemory = 0;

		CalculateRoadRouteToPlayer(cop);

		if (cop->routeInMemory && cop->justPinged == 1)
		{
			cp->direction = cop->route_entries[0].direction;

			TempBuildHandlingMatrix(cp, 0);

			cop->justPinged = 0;
			cop->speedUpTimer = 0;
		}
	}
}
