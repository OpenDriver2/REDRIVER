/*
 * roadbits.c - the road map: roads and junctions per 1500 unit cell, surfaces, traffic lights
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "roadbits.c"); partially translated.
 *
 * Like the cell map, the road map is kept as the four regions around the player
 * (RoadMapDataRegions: the road or junction of each cell, RoadMapInfoRegions: its surface model
 * type, height and heading), unpacked from MapData / MapInfo by UnpackRegion (map.c). A cell's
 * info word: bits 0 - 15 the ground height, 16 - 25 the model type (SurfacePtrs, model_pointers),
 * 30 - 31 the heading (hdg).
 */

#include <string.h>

#include "dr_types.h"
#include "roadbits.h"
#include "dr_globals.h"
#include "general.h"
#include "map.h"

//------------------------------------------------------------------------------------------
// initialised data (0x578180)

static short hdg[4] = { 0, 1024, 2048, 3072 };

//------------------------------------------------------------------------------------------
// uninitialised data

uint8_t JunctionLightsNS;			// NOTE: owner guessed (most references)
uint8_t JunctionLightsWE;
int JunctionLightsTimer;
char JunctionLightsChanged;
char RoadBlockGap;					// NOTE: owner guessed (most references)

// the cell of the road map of the region (x, z) as region slot and index
#define ROAD_CELL(regions, ix, iz) \
	(regions)[2 * ((regions_down - 1 - (iz) / road_region_size) & 1) + (((ix) / road_region_size) & 1)] \
		[road_region_size * ((iz) % road_region_size) + (ix) % road_region_size]

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
