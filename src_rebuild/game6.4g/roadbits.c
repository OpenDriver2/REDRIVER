/*
 * roadbits.c - the road map: roads and junctions per 1500 unit cell, surfaces, traffic lights
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "roadbits.c", 0x51cad0 - 0x51e5bd).
 *
 * Like the cell map, the road map is kept as the four regions around the player
 * (RoadMapDataRegions: the road or junction of each cell, RoadMapInfoRegions: its surface model
 * type, height and heading), unpacked from MapData / MapInfo by UnpackRegion (map.c). A cell's
 * info word: bits 0 - 15 the ground height, 16 - 25 the model type (SurfacePtrs, model_pointers),
 * 30 - 31 the heading (hdg).
 */

#include <limits.h>
#include <string.h>

#include "dr_types.h"
#include "roadbits.h"
#include "dr_globals.h"
#include "ai.h"
#include "asmmath.h"
#include "general.h"
#include "handling.h"
#include "map.h"
#include "shell.h"

//------------------------------------------------------------------------------------------
// initialised data (0x578180)

static short hdg[4] = { 0, 1024, 2048, 3072 };
static char headings[4] = { 2, 3, 0, 1 };	// the junction entry of the road a car heading 0 - 3 drives to
static unsigned char lanes[9] = { 0x00, 0x01, 0x03, 0x07, 0x0F, 0x1F, 0x3F, 0x7F, 0xFF };	// the LaneDirs bits of n lanes

//------------------------------------------------------------------------------------------
// uninitialised data

uint8_t JunctionLightsNS;			// NOTE: owner guessed (most references)
uint8_t JunctionLightsWE;
int JunctionLightsTimer;
char JunctionLightsChanged;
char RoadBlockGap;					// NOTE: owner guessed (most references)
// NOTE: in the common area without IDA marks; defined here as this file is their only user
int dword_BD9BD0;					// NOTE: unnamed in IDA; the last road sub_51CAD0 looked at
XYPAIR RoadBox[2];

// the cell of the road map of the region (x, z) as region slot and index
#define ROAD_CELL(regions, ix, iz) \
	(regions)[2 * ((regions_down - 1 - (iz) / road_region_size) & 1) + (((ix) / road_region_size) & 1)] \
		[road_region_size * ((iz) % road_region_size) + (ix) % road_region_size]

// [0x51cad0] [D]
// NOTE: no name in the database; only the debug view sub_48E1E0 (ai.c) calls it. The direction
// (0 - 3) of the lane at a world position: counts the road's cells from there to its edge and
// looks up that lane's bit in LaneDirs; -1 off a road.
int sub_51CAD0(float x, float z)
{
	short road;
	char vertical;
	char lane;
	int ix;
	int iz;
	int i;

	vertical = 0;
	lane = 0;

	if (NumRoads == 0)
		return -1;

	ix = (float_to_int(x) + RoadMapUnitXMid + 750) / 1500;
	iz = (RoadMapUnitZMid + 750 - float_to_int(z)) / 1500;

	if (ix < 0 || ix >= RoadMapWidth || iz < 0 || iz >= RoadMapHeight)
		return -1;

	road = ROAD_CELL(RoadMapDataRegions, ix, iz);

	if (road & 0xC000)
		return -1;

	dword_BD9BD0 = road;

	for (i = 0; i < 4 && Junctions[Roads[road].JunctionA].JunctionEntry[i].ViaRoad != road; i++)
		vertical ^= 1;

	if (1)
	{
		while (road == ROAD_CELL(RoadMapDataRegions, ix, iz))
		{
			lane++;

			if (vertical)
				iz++;
			else
				ix++;
		}
	}

	if (Roads[road].LaneDirs & (unsigned char)(1 << (Roads[road].NumLanes - lane)))
		return vertical != 0;

	if (vertical)
		return 3;

	return 2;
}

// [0x51cddb] [D]
// Allocates the four regions of the road map (road_region_size is at most 120).
void ROADS_InitRoadMapRegions(void)
{
	int i;

	for (i = 0; i < 4; i++)
	{
		RoadMapRegions[i] = -1;
		RoadMapDataRegions[i] = (short *)DebugMalloc(0x7080, "D:\\Driver\\game6.4g\\roadbits.c", 201);
		RoadMapInfoRegions[i] = (int *)DebugMalloc(0xE100, "D:\\Driver\\game6.4g\\roadbits.c", 202);
	}
}

// [0x51ce4d] [D]
void ROADS_DeInitRoadMapRegions(void)
{
	int i;

	for (i = 0; i < 4; i++)
	{
		DebugFree(RoadMapDataRegions[i]);
		RoadMapDataRegions[i] = NULL;

		DebugFree(RoadMapInfoRegions[i]);
		RoadMapInfoRegions[i] = NULL;
	}
}

// [0x51ceb1] [D]
// Unpacks a region's roads (run length pairs count, value in MapData); -1 everywhere when the
// region has none. Returns 1 when unpacked.
int ROADS_UnpackRoadRegion(int region, short *dest)
{
	short size;
	short count;
	int offset;
	short value;
	unsigned short *ptr;

	offset = MapData[region + 2];
	size = road_region_size * road_region_size;

	if (offset == -1)
	{
		while (size)
		{
			*dest++ = -1;
			size--;
		}

		return 0;
	}

	ptr = (unsigned short *)((char *)MapData + offset);

	while (size)
	{
		count = ptr[0];
		value = ptr[1];
		size -= ptr[0];
		ptr += 2;

		while (count)
		{
			*dest++ = value;
			count--;
		}
	}

	return 1;
}

// [0x51cf85] [D]
// Unpacks a region's cell info (run length pairs count, value in MapInfo); -1 everywhere when
// the region has none.
int ROADS_UnpackRoadInfoRegion(int region, int *dest)
{
	int size;
	unsigned int count;
	int offset;
	int value;
	unsigned int *ptr;
	unsigned int i;

	offset = ((int *)MapInfo)[region + 2];
	size = road_region_size * road_region_size;

	if (offset == -1)
	{
		memset(dest, 0xFF, 4 * size);
		return 0;
	}

	ptr = (unsigned int *)(MapInfo + offset);

	while (size)
	{
		count = ptr[0];
		value = ptr[1];
		size -= ptr[0];
		ptr += 2;

		for (i = 0; i < count; i++)
			dest[i] = value;

		dest += count;
	}

	return 1;
}

// [0x51d045] [D]
// The cells (RoadBox, road map units) of the road's lanes running in dir: the lanes with their
// LaneDirs bit set go 0 / 1 (north / east), the others 2 / 3. Returns 0 if the road does not run
// that way.
// NOTE: an IDA comment at the dir test of the north - south branch reads "if ( dir == 1 )"; the
// binary tests dir != 0 (dir is 0 or 2 there).
int sub_51D045(int id, char dir)
{
	ROAD *road;
	unsigned char laneDirs;
	unsigned char count;
	char bounddir;
	int numLanes;

	road = &Roads[id];
	bounddir = RoadBounds[id].dir;

	if (bounddir == 0 && (dir == 1 || dir == 3))
		return 0;

	if (bounddir == 1 && (dir == 0 || dir == 2))
		return 0;

	RoadBox[0].x = RoadBounds[id].x1;
	RoadBox[0].y = RoadBounds[id].y1;
	RoadBox[1].x = RoadBounds[id].x2;
	RoadBox[1].y = RoadBounds[id].y2;

	numLanes = (unsigned char)road->NumLanes;
	laneDirs = road->LaneDirs;
	count = 0;

	while (numLanes)
	{
		if (laneDirs & 1)
			count++;

		laneDirs >>= 1;
		numLanes--;
	}

	if (bounddir)
	{
		if (dir == 3)
			RoadBox[1].y -= count;
		else
			RoadBox[0].y += (unsigned char)(road->NumLanes - count);
	}
	else
	{
		if (dir != 0)
			RoadBox[1].x -= count;
		else
			RoadBox[0].x += (unsigned char)(road->NumLanes - count);
	}

	return 1;
}

// [0x51d266] [D]
// NOTE: no name in the database and no callers: sub_51D045 relative to the road map's corner
// (RoadMapXMid, RoadMapZMid), without the return value.
void sub_51D266(int id, char dir)
{
	ROAD *road;
	unsigned char laneDirs;
	unsigned char count;
	char rdir;
	int numLanes;

	road = &Roads[id];
	rdir = RoadBounds[id].dir;

	if ((rdir == 0 && (dir == 1 || dir == 3)) || (rdir == 1 && (dir == 0 || dir == 2)))
		return;

	RoadBox[0].x = RoadMapXMid + RoadBounds[id].x1;
	RoadBox[0].y = RoadMapZMid + RoadBounds[id].y1;
	RoadBox[1].x = RoadMapXMid + RoadBounds[id].x2;
	RoadBox[1].y = RoadMapZMid + RoadBounds[id].y2;

	numLanes = (unsigned char)road->NumLanes;
	laneDirs = road->LaneDirs;
	count = 0;

	while (numLanes)
	{
		if (laneDirs & 1)
			count++;

		laneDirs >>= 1;
		numLanes--;
	}

	if (rdir)
	{
		if (dir == 3)
			RoadBox[1].y -= count;
		else
			RoadBox[0].y += (unsigned char)(road->NumLanes - count);
	}
	else
	{
		if (dir != 0)
			RoadBox[1].x -= count;
		else
			RoadBox[0].x += (unsigned char)(road->NumLanes - count);
	}
}

// [0x51d491] [D]
// The road (or junction, 0x4000 / 0x8000 set) at a world position; -1 off the map.
short ROADS_GetRoadOrJunction(int x, int z)
{
	int ix;
	int iz;

	ix = (x + RoadMapUnitXMid + 750) / 1500;
	iz = (RoadMapUnitZMid + 750 - z) / 1500;

	if (ix < 0 || ix >= RoadMapWidth || iz < 0 || iz >= RoadMapHeight)
		return -1;

	return ROAD_CELL(RoadMapDataRegions, ix, iz);
}

// [0x51d588] [D]
// The road beside the one at a world position, across its width for a block in dir (only within
// the cell's region); RoadBlockGap 1 if it is on the far side (east / south), 0 on the near side,
// -1 if none.
int ROADS_FindAdjacentRoad(int x, int z, char dir)
{
	short road;
	short next;
	int maxRoad;
	int region;
	int ix;
	int iz;
	int i;

	ix = (x + RoadMapUnitXMid + 750) / 1500;
	iz = (RoadMapUnitZMid + 750 - z) / 1500;

	RoadBlockGap = -1;

	if (ix < 0 || ix >= RoadMapWidth || iz < 0 || iz >= RoadMapHeight)
		return -1;

	region = ((ix / road_region_size) & 1) + 2 * ((regions_down - 1 - iz / road_region_size) & 1);

	ix %= road_region_size;
	iz %= road_region_size;

	road = RoadMapDataRegions[region][road_region_size * iz + ix];

	if (road == -1)
		return -1;

	if (gBotchRoads)
		maxRoad = 0x3FFF;
	else
		maxRoad = NumRoads;

	if (dir & 1)
	{
		for (i = iz; RoadMapDataRegions[region][road_region_size * i + ix] == road; i++)
			;

		next = RoadMapDataRegions[region][road_region_size * (i + 1) + ix];

		if (next >= 0 && next < maxRoad)
		{
			RoadBlockGap = 1;
			return next;
		}

		for (i = iz; RoadMapDataRegions[region][road_region_size * i + ix] == road; i--)
			;

		next = RoadMapDataRegions[region][road_region_size * (i - 1) + ix];

		if (next >= 0 && next < maxRoad)
		{
			RoadBlockGap = 0;
			return next;
		}
	}
	else
	{
		for (i = ix; RoadMapDataRegions[region][road_region_size * iz + i] == road; i--)
			;

		next = RoadMapDataRegions[region][road_region_size * iz - 1 + i];

		if (next >= 0 && next < maxRoad)
		{
			RoadBlockGap = 1;
			return next;
		}

		for (i = ix; RoadMapDataRegions[region][road_region_size * iz + i] == road; i++)
			;

		next = RoadMapDataRegions[region][road_region_size * iz + 1 + i];

		if (next >= 0 && next < maxRoad)
		{
			RoadBlockGap = 0;
			return next;
		}
	}

	return -1;
}

// [0x51d8e6] [D]
// The cell info word at a world position; 0 off the map.
int ROADS_GetMapInfo(int x, int z)
{
	int ix;
	int iz;

	ix = (x + RoadMapUnitXMid + 750) / 1500;
	iz = (RoadMapUnitZMid + 750 - z) / 1500;

	if (ix < 0 || ix >= RoadMapWidth || iz < 0 || iz >= RoadMapHeight)
		return 0;

	return ROAD_CELL(RoadMapInfoRegions, ix, iz);
}

// [0x51d9da] [D]
// The surface type, height and heading of the cell at a world position into RouteData; 0 off
// the map or for an empty cell.
int ROADS_GetRoadInfo(int x, int z)
{
	unsigned int info;
	int ix;
	int iz;

	RouteData.height = 0;

	ix = (x + RoadMapUnitXMid + 750) / 1500;
	iz = (RoadMapUnitZMid + 750 - z) / 1500;

	if (ix < 0 || ix >= RoadMapWidth || iz < 0 || iz >= RoadMapHeight)
		return 0;

	info = ROAD_CELL(RoadMapInfoRegions, ix, iz);

	if (!info)
		return 0;

	RouteData.type = (info >> 16) & 0x3FF;
	RouteData.height = (short)info;
	RouteData.objectAngle = hdg[info >> 30];

	return 1;
}

// [0x51db23] [D]
// The junction the car drives towards on its road (by its heading), -1 off a road.
int ROADS_GetTargetJunction(char car)
{
	unsigned char dir;
	short junctionA;
	short rj;
	int region;
	int ix;
	int iz;

	dir = GetCarHeading(car_data[(unsigned char)car].direction);

	ix = (car_data[(unsigned char)car].location.vx + RoadMapUnitXMid + 750) / 1500;
	iz = (RoadMapUnitZMid + 750 - car_data[(unsigned char)car].location.vz) / 1500;

	if (ix < 0 || ix >= RoadMapWidth || iz < 0 || iz >= RoadMapHeight)
		return -1;

	region = regions_down - 1 - iz / road_region_size;

	rj = RoadMapDataRegions[2 * (region & 1) + ((ix / road_region_size) & 1)][ix % road_region_size + road_region_size * (iz % road_region_size)];

	if (rj == -1 || (rj & 0xC000))
		return -1;

	junctionA = Roads[rj].JunctionA;

	if (Junctions[junctionA].JunctionEntry[(unsigned char)headings[dir]].ViaRoad == rj)
		return junctionA;

	return Roads[rj].JunctionB;
}

// [0x51dcf4] [D]
// The junction ahead of a position driving in lane (direction 0 - 3) into RouteData (junction,
// its entry flags - 4 for traffic lights -, the distance ahead and along the road, the cell's
// type and height). Returns 0 on a road, 1 in a junction (RouteData.junction only), -1 off the
// road map.
int ROADS_GetNextJunction(int x, int z, char lane)
{
	short road;
	int region;
	int index;
	int info;
	int dist;
	int ix;
	int iz;

	RouteData.junction = -1;
	RouteData.road = -1;
	RouteData.field_12 = 0;
	RouteData.flags = 0;

	ix = (x + RoadMapUnitXMid + 750) / 1500;
	iz = (RoadMapUnitZMid + 750 - z) / 1500;

	if (ix < 0 || ix >= RoadMapWidth || iz < 0 || iz >= RoadMapHeight)
		return -1;

	region = regions_down - 1 - iz / road_region_size;
	index = ix % road_region_size + road_region_size * (iz % road_region_size);

	road = RoadMapDataRegions[2 * (region & 1) + ((ix / road_region_size) & 1)][index];
	info = RoadMapInfoRegions[2 * (region & 1) + ((ix / road_region_size) & 1)][index];

	RouteData.distAlongSegment = INT_MAX;
	RouteData.distAhead = INT_MAX;

	if (road == -1)
		return -1;

	if (road & 0xC000)
	{
		RouteData.junction = road & 0x3FFF;
		return 1;
	}

	RouteData.type = (info >> 16) & 0xC3FF;
	RouteData.height = (unsigned short)info;
	RouteData.road = road;

	if (Junctions[Roads[road].JunctionA].JunctionEntry[(lane + 2) & 3].ViaRoad == road)
		RouteData.junction = Roads[road].JunctionA;
	else
		RouteData.junction = Roads[road].JunctionB;

	RouteData.flags = Junctions[RouteData.junction].JunctionEntry[(lane + 2) & 3].JunctionFlags;

	if (Junctions[RouteData.junction].Type == 1)
		RouteData.flags = 4;

	switch (lane)
	{
		case 0:
			dist = 750 - (1500 * RoadBounds[road].y1 + z);
			break;
		case 1:
			dist = 1500 * (RoadBounds[road].x2 + 1) - (x + 750);
			break;
		case 2:
			dist = 1500 * (RoadBounds[road].y2 + 1) + z - 750;
			break;
		case 3:
			dist = x + 750 - 1500 * RoadBounds[road].x1;
			break;
	}

	RouteData.distAhead = dist;
	RouteData.distAlongSegment = 1500 * Roads[road].Length - dist;

	return 0;
}

// [0x51e040] [D]
// A road not entered from the north or south of its junction A.
int ROADS_IsOneWayRoad(int road)
{
	short junc;

	junc = Roads[road].JunctionA;

	return Junctions[junc].JunctionEntry[0].ViaRoad != road && Junctions[junc].JunctionEntry[2].ViaRoad != road;
}

// [0x51e089] [D]
// The entry (0 - 3) of the junction that a road joins, -1 if none.
char sub_51E089(short junc, short road)
{
	if (Junctions[junc].JunctionEntry[0].ViaRoad == road)
		return 0;

	if (Junctions[junc].JunctionEntry[1].ViaRoad == road)
		return 1;

	if (Junctions[junc].JunctionEntry[2].ViaRoad == road)
		return 2;

	if (Junctions[junc].JunctionEntry[3].ViaRoad == road)
		return 3;

	return -1;
}

// [0x51e0f8] [D]
void ROADS_InitTrafficLights(void)
{
	JunctionLightsTimer = 300;
	JunctionLightsNS = 1;
	JunctionLightsWE = 3;
	JunctionLightsChanged = 0;
}

// [0x51e11c] [D]
// Steps the traffic lights of all junctions: phases 1 - 4, the green phases (1, 3) last 300
// frames, the amber ones 100.
void ROADS_ControlTrafficLights(void)
{
	if (--JunctionLightsTimer != 0)
		return;

	if (++JunctionLightsNS > 4)
		JunctionLightsNS = 1;

	if (++JunctionLightsWE > 4)
		JunctionLightsWE = 1;

	if (JunctionLightsNS == 1 || JunctionLightsNS == 3)
		JunctionLightsTimer = 300;
	else
		JunctionLightsTimer = 100;

	JunctionLightsChanged++;
}

// [0x51e1bc] [D]
// Points SurfacePtrs[type] at each surface model of the surface lump (OrigSurfaceData: a count,
// then the SURFACEINFO records with their polygons).
void ROADS_InitSurfaceLookup(void)
{
	SURFACEINFO *surface;
	int count;
	int i;

	for (i = 0; i < 0x4000; i++)
		SurfacePtrs[i] = NULL;

	surface = (SURFACEINFO *)(OrigSurfaceData + 4);
	count = *(int *)OrigSurfaceData;

	for (i = 0; i < count; i++)
	{
		SurfacePtrs[surface->type] = surface;
		surface = (SURFACEINFO *)((char *)surface + sizeof(SIPOLY) * surface->numpolys + sizeof(SURFACEINFO));
	}
}

// [0x51e255] [D]
// Marks the junctions with two roads (bends).
void ROADS_CalculateBends(void)
{
	JUNCTION *junc;
	int numViaRoads;
	int i;

	junc = Junctions;

	for (i = 0; i < NumJunctions; i++)
	{
		numViaRoads = junc->JunctionEntry[0].ViaRoad != -1;

		if (junc->JunctionEntry[1].ViaRoad != -1)
			numViaRoads++;

		if (junc->JunctionEntry[2].ViaRoad != -1)
			numViaRoads++;

		if (junc->JunctionEntry[3].ViaRoad != -1)
			numViaRoads++;

		junc->JWH = numViaRoads == 2;

		junc++;
	}
}

// [0x51e302] [D]
// Marks the one way roads: 0x80 + the direction (0 - 3) of all lanes, 0 for two way roads.
void ROADS_InitOneWayRoads(void)
{
	unsigned char mask;
	int road;

	for (road = 0; road < NumRoads; road++)
	{
		mask = lanes[(unsigned char)Roads[road].NumLanes];

		if ((mask & Roads[road].LaneDirs) == 0)
		{
			if (ROADS_IsOneWayRoad(road))
				Roads[road].IsOneWay = 0x83;
			else
				Roads[road].IsOneWay = 0x82;
		}
		else if ((unsigned char)(mask & Roads[road].LaneDirs) == mask)
		{
			if (ROADS_IsOneWayRoad(road))
				Roads[road].IsOneWay = 0x81;
			else
				Roads[road].IsOneWay = 0x80;
		}
		else
		{
			Roads[road].IsOneWay = 0;
		}
	}
}

// [0x51e3fa] [D]
// The number of lanes and the start corner of each road for each direction (RoadLaneInfo).
void ROADS_InitLaneLookup(void)
{
	ROAD_LANE_INFO *info;
	int isOneWay;
	int road;

	info = RoadLaneInfo;

	for (road = 0; road < NumRoads; road++, info++)
	{
		isOneWay = ROADS_IsOneWayRoad(road);

		sub_51D045(road, 0);

		if (isOneWay)
			info->lanes[0] = 0;
		else
			info->lanes[0] = RoadBox[1].x - RoadBox[0].x + 1;

		info->bounds[0].x = 1500 * RoadBox[0].x;
		info->bounds[0].y = -1500 * RoadBox[1].y;

		sub_51D045(road, 1);

		if (isOneWay)
			info->lanes[1] = RoadBox[1].y - RoadBox[0].y + 1;
		else
			info->lanes[1] = 0;

		info->bounds[1].x = 1500 * RoadBox[0].x;
		info->bounds[1].y = -1500 * RoadBox[0].y;

		sub_51D045(road, 2);

		if (isOneWay)
			info->lanes[2] = 0;
		else
			info->lanes[2] = RoadBox[1].x - RoadBox[0].x + 1;

		info->bounds[2].x = 1500 * RoadBox[1].x;
		info->bounds[2].y = -1500 * RoadBox[0].y;

		sub_51D045(road, 3);

		if (isOneWay)
			info->lanes[3] = RoadBox[1].y - RoadBox[0].y + 1;
		else
			info->lanes[3] = 0;

		info->bounds[3].x = 1500 * RoadBox[1].x;
		info->bounds[3].y = -1500 * RoadBox[1].y;
	}
}
