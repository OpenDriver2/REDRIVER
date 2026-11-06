/*
 * pathfind.c - cop route finding: an occupancy grid of the road map around the cop's target and
 * an A* search over it
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "pathfind.c", 0x50bd60 - 0x50faee).
 *
 * The grid is 55 x 55 road map cells centred on the target cell (27, 27: offset 1512 =
 * node_offset). GenerateOccupancyGrid copies the cells from the road map info regions (bit 0 from
 * info bit 0, bits 1 - 4 from the object bits 26 - 29, 30 = fully blocked) and
 * AddOtherCarsToOccupancyGrid marks the quarter cells other cars cover (bit 5, bits 1 - 4: which
 * quarter). CostGrid (the step cost, from CostValues) and BlockedGrid (bits 1 - 4) are derived
 * from it. GetRouteAStar searches from the cop's cell to the target cell; sub_50E1A7 turns the
 * nodes found into the cop's route entries (world positions, extra nodes around the corners of
 * blocked cells, directions). ShowRoute and DisplayOccupancyGrid are debug displays
 * (OverlayOccupancyGrid).
 */

#include <stdlib.h>

#include "dr_types.h"
#include "pathfind.h"
#include "dr_globals.h"
#include "D3DGlide/glide_ptrs.h"
#include "ai.h"
#include "asmmath.h"
#include "camera.h"
#include "general.h"
#include "handling.h"
#include "map.h"
#include "roadbits.h"
#include "testmath.h"
#include "Engine/3dfx/gfxinit.h"
#include "Engine/3dfx/object.h"
#include "Engine/3dfx/polydraw.h"
#include "frontend/cheats.h"
#include "glaunch.h"

int sub_50E1A7(COP *cd);
void CopyRun(int *src, int *dst, int count);

XYPAIR xy_offset_table[8] = {
	{ -1, -1 }, { 0, -1 }, { 1, -1 }, { 1, 0 }, { 1, 1 }, { 0, 1 }, { -1, 1 }, { -1, 0 }
};

int node_offset = 1512;

// step cost of an occupancy value: 7 open, 3 road, 8 part blocked, 4096 blocked; 32 and up
// (a car in the cell) the same with 9 for part blocked
int CostValues[64] = {
	7, 3, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
	8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 4096, 4096,
	7, 3, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
	9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 4096, 4096
};

// the 3 x 3 cells around the grid centre behind a route direction (COP_ROUTE_ENTRY.flags)
char byte_5769AC[16][9] = {
	{ 0, 0, 0, 0, 0, 0, 1, 1, 1 },
	{ 0, 0, 0, 1, 0, 0, 1, 1, 0 },
	{ 1, 0, 0, 1, 0, 0, 1, 0, 0 },
	{ 1, 1, 0, 1, 0, 0, 0, 0, 0 },
	{ 1, 1, 1, 0, 0, 0, 0, 0, 0 },
	{ 0, 1, 1, 0, 0, 1, 0, 0, 0 },
	{ 0, 0, 1, 0, 0, 1, 0, 0, 1 },
	{ 0, 0, 0, 0, 0, 1, 0, 1, 1 },
};

// NOTE: in the common area without IDA marks; defined here as this file is their only or main
// user

XYPAIR open_queue[513];				// the open list, a binary heap from 1: x = dist, y = cell
int node_count;
int num_open_nodes;
int OccupancyGrid[3027];			// 55 * 55 and 2 spare: CopyRun writes an even count
COP_ROUTE_ENTRY save_path[8];
int CostGrid[3025];
int num_nodes_to_copy;				// NOTE: owner guessed (most references)
COP_ROUTE_NODE nodes[3025];
unsigned char num_close_cars;		// NOTE: owner guessed (most references)
int offset_table[8];
ASTAR_POLY polyAstar;
char OverlayOccupancyGrid;			// NOTE: owner guessed (most references)
char close_cars[64];				// NOTE: owner guessed (most references)
int dword_CFCA40;					// NOTE: unnamed in IDA; only InitPathFind clears it
int dword_CFCA44;					// NOTE: unnamed in IDA; only InitPathFind clears it
int num_route_entries;				// NOTE: owner guessed (most references)
int BlockedGrid[3025];

// [0x50bd60] [D]
void InitPathFind(void)
{
	ClearMem(open_queue, sizeof(open_queue));
	ClearMem(save_path, sizeof(save_path));
	ClearMem(nodes, sizeof(nodes));
	ClearMem(offset_table, sizeof(offset_table));
	ClearMem(OccupancyGrid, sizeof(OccupancyGrid));
	ClearMem(close_cars, sizeof(close_cars));
	ClearMem(BlockedGrid, sizeof(BlockedGrid));
	ClearMem(CostGrid, sizeof(CostGrid));

	OverlayOccupancyGrid = 0;
	num_open_nodes = 0;
	dword_CFCA44 = 0;
	dword_CFCA40 = 0;
	node_offset = 1512;
	node_count = 0;
	num_nodes_to_copy = 0;
	num_route_entries = 0;
	num_close_cars = 0;
}

// [0x50be47] [D]
// A flat line on the screen in colour 0xRRGGBB, shifted 2 pixels right and down.
void DrawLine(int x0, int y0, int x1, int y1, int colour)
{
	GrVertex a;
	GrVertex b;

	if (last_state)
	{
		grColorCombine(GR_COMBINE_FUNCTION_LOCAL, GR_COMBINE_FACTOR_ZERO, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_CONSTANT, FXFALSE);
		last_state = FLAT3;
	}

	if (last_trans_on)
	{
		grAlphaBlendFunction(GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_ONE, GR_BLEND_ZERO);
		last_trans_on = 0;
	}

	a.x = (float)(x0 + 2);
	a.y = (float)(y0 + 2);
	b.r = (float)((colour >> 16) & 0xFF);
	a.r = b.r;
	b.g = (float)((colour >> 8) & 0xFF);
	a.g = b.g;
	b.b = (float)(colour & 0xFF);
	a.b = b.b;

	if (using_glide)
	{
		a.oow = 1.0f;
		b.oow = 1.0f;
	}
	else
	{
		a.oow = 0.015625f;
		b.oow = 0.015625f;
	}

	b.x = (float)(x1 + 2);
	b.y = (float)(y1 + 2);

	grDrawLine(&a, &b);

	last_state = -1;
	last_trans_on = -1;
}

// [0x50bf84] [D]
// NOTE: no name in the database and no callers; the argument types are guessed (pos: x at +0,
// z at +8). Walks the occupancy grid from the player's cell + (dx, dz) to the cell of pos: 0 if
// a fully blocked cell is on the way. The walk starts at grid cell (27 + dx, 27 + dz), and the
// steps are close to Bresenham but not exact (as in the binary).
int sub_50BF84(IVECTOR *pos, int dx, int dz)
{
	int px;
	int pz;
	int tx;
	int tz;
	int ddx;
	int ddz;
	int adx;
	int adz;
	int stepx;
	int stepz;
	int err;
	int i;
	int *cell;

	px = car_data[0].location.vx + RoadMapUnitXMid + 750;
	pz = RoadMapUnitZMid + 750 - car_data[0].location.vz;
	tx = pos->vx + RoadMapUnitXMid + 750;
	tz = RoadMapUnitZMid + 750 - pos->vz;

	px = dx + IntegerApplyRecip(px);
	pz = dz + IntegerApplyRecip(pz);
	tx = IntegerApplyRecip(tx);
	tz = IntegerApplyRecip(tz);

	ddx = tx - px;
	ddz = tz - pz;
	adx = abs(ddx);
	adz = abs(ddz);

	cell = &OccupancyGrid[55 * dz + 1512 + dx];

	stepx = ddx < 0 ? -1 : 1;
	stepz = ddz < 0 ? -1 : 1;

	if (adx > adz)
	{
		err = adz;

		for (i = 0; i != ddx; i += stepx)
		{
			if (err > adx)
			{
				err -= adx;
				cell += 55 * stepz;
			}

			if (*cell == 30)
				return 0;

			cell += stepx;
			err += adz;
		}
	}
	else if (adx == adz)
	{
		for (i = 0; i != ddx; i += stepx)
		{
			if (*cell == 30)
				return 0;

			cell += stepx + 55 * stepz;
		}
	}
	else
	{
		err = adx;

		for (i = 0; i != ddz; i += stepz)
		{
			if (err > adz)
			{
				cell += stepx;
				err -= adz;
			}

			if (*cell == 30)
				return 0;

			cell += 55 * stepz;
			err += adx;
		}
	}

	return 1;
}

// One clipping pass of sub_50C27B (Sutherland - Hodgman): keeps the part of the polygon in on the
// inner side of x (or z with alongZ) = bound (>= bound, <= with upper) and writes it to out as a
// linked list; returns its corner count. The crossing point is interpolated from the corner with
// the lower coordinate.
static int ClipPolygon(ASTAR_NODE *in, ASTAR_NODE *out, int bound, int alongZ, int upper)
{
	ASTAR_NODE *head;
	ASTAR_NODE *last;
	ASTAR_NODE *next;
	ASTAR_NODE *lo;
	ASTAR_NODE *hi;
	ASTAR_NODE from;
	int count;
	int again;
	int a;
	int b;
	int aIn;
	int bIn;
	float d;
	float t;
	float o;

	head = in;
	last = NULL;
	count = 0;
	again = 0;

	do
	{
		// after entering the inside, the edge is walked again from the crossing point
		if (again)
		{
			from.x = last->x;
			from.z = last->z;
		}
		else
		{
			from.x = in->x;
			from.z = in->z;
		}

		next = in->next ? in->next : head;

		a = alongZ ? from.z : from.x;
		b = alongZ ? next->z : next->x;

		if (upper)
		{
			aIn = a <= bound;
			bIn = b <= bound;
		}
		else
		{
			aIn = a >= bound;
			bIn = b >= bound;
		}

		if (aIn || bIn)
		{
			if (aIn && bIn)
			{
				out->x = next->x;
				out->z = next->z;
			}
			else
			{
				if (a < b)
				{
					lo = &from;
					hi = next;
				}
				else
				{
					lo = next;
					hi = &from;
				}

				if (alongZ)
				{
					d = (float)(hi->z - lo->z);
					t = (float)(bound - lo->z);
					o = (float)(hi->x - lo->x);
					out->x = float_to_int(o / d * t) + lo->x;
					out->z = bound;
				}
				else
				{
					d = (float)(hi->x - lo->x);
					t = (float)(bound - lo->x);
					o = (float)(hi->z - lo->z);
					out->x = bound;
					out->z = float_to_int(o / d * t) + lo->z;
				}
			}

			out->next = NULL;

			if (last)
				last->next = out;

			last = out++;
			count++;

			again = !aIn;
		}

		if (!again)
			in = in->next;
	}
	while (in);

	return count;
}

// [0x50c27b] [A]
// NOTE: the four clipping passes, the same code with another bound in the original, are one
// static helper (ClipPolygon).
// The area of the part of the polygon poly inside the rectangle box[0] - box[1] (n: x, t: z),
// 0 if none.
int sub_50C27B(ASTAR_NODE *poly, iVectNT *box)
{
	ASTAR_NODE a[16];
	ASTAR_NODE b[16];
	ASTAR_NODE *p;
	int count;
	int sum;
	int i;

	if (ClipPolygon(poly, a, box[0].n, 0, 0) == 0)
		return 0;

	if (ClipPolygon(a, b, box[1].n, 0, 1) == 0)
		return 0;

	if (ClipPolygon(b, a, box[0].t, 1, 0) == 0)
		return 0;

	count = ClipPolygon(a, b, box[1].t, 1, 1);

	if (count == 0)
		return 0;

	sum = 0;
	p = b;

	for (i = 0; i < count - 1; i++)
	{
		sum += p->next->z * p->x - p->z * p->next->x;
		p++;
	}

	return abs((b[0].z * p->x - p->z * b[0].x + sum) / 2);
}

// [0x50d1a8] [D]
// Marks the quarter cells the close cars cover (with 450 units plus their speed ahead of them,
// 900 behind, 180 to each side) in the occupancy grid; the cop's own car is left out.
void AddOtherCarsToOccupancyGrid(COP *cd)
{
	CAR_DATA *cp;
	CIV_STATE *ai;
	iVectNT box[2];
	int cx[4];
	int cz[4];
	int i;
	int j;
	int k;
	int slot;
	int x;
	int z;
	int speed;
	int dir;
	int minx;
	int maxx;
	int minz;
	int maxz;
	int x0;
	int x1;
	int z0;
	int z1;
	float s;
	float c;

	for (i = 0; i < num_close_cars; i++)
	{
		slot = (unsigned char)close_cars[i];
		cp = &car_data[slot + 1];
		ai = &civilian_ai_data[slot];

		if (ai->ctrlState == 0 || (unsigned char)ai->IsCopCar == cd->id + 1)
			continue;

		x = cp->location.vx + RoadMapUnitXMid + 750;
		z = RoadMapUnitZMid + 750 - cp->location.vz;

		speed = cp->speed * 16;
		speed += speed >> 2;
		speed += 450;

		dir = -cp->direction & 0xFFF;
		s = fsintab[dir];
		c = fsintab[(dir + 1024) & 0xFFF];

		cx[0] = x + float_to_int(-180.0f * c - (float)speed * s);
		cz[0] = z - float_to_int(-180.0f * s + (float)speed * c);
		cx[1] = x + float_to_int(180.0f * c - (float)speed * s);
		cz[1] = z - float_to_int(180.0f * s + (float)speed * c);
		cx[2] = x + float_to_int(-180.0f * c - -900.0f * s);
		cz[2] = z - float_to_int(-180.0f * s + -900.0f * c);
		cx[3] = x + float_to_int(180.0f * c - -900.0f * s);
		cz[3] = z - float_to_int(180.0f * s + -900.0f * c);

		minx = maxx = cx[0];
		minz = maxz = cz[0];

		for (j = 1; j < 4; j++)
		{
			if (cx[j] < minx)
				minx = cx[j];

			if (cx[j] > maxx)
				maxx = cx[j];

			if (cz[j] < minz)
				minz = cz[j];

			if (cz[j] > maxz)
				maxz = cz[j];
		}

		x0 = IntegerApplyRecip(minx) - 1;
		x1 = IntegerApplyRecip(maxx) + 1;
		z0 = IntegerApplyRecip(minz) - 1;
		z1 = IntegerApplyRecip(maxz) + 1;

		// the corners relative to the first cell of the box
		for (j = 0; j < 4; j++)
		{
			polyAstar[j].x = cx[j] - 1500 * x0;
			polyAstar[j].z = cz[j] - 1500 * z0;
		}

		x0 += 27 - cd->targetX;
		x1 += 27 - cd->targetX;
		z0 += 27 - cd->targetZ;
		z1 += 27 - cd->targetZ;

		if (x0 < 0)
			x0 = 0;

		if (z0 < 0)
			z0 = 0;

		if (x1 >= 55)
			x1 = 54;

		if (z1 >= 55)
			z1 = 54;

		// half cells (quarters of a cell): covered by more than 50000 square units
		box[0].t = 0;
		box[1].t = 750;

		for (j = z0 * 2; j <= z1 * 2; j++)
		{
			box[0].n = 0;
			box[1].n = 750;

			for (k = x0 * 2; k <= x1 * 2; k++)
			{
				if (sub_50C27B(polyAstar, box) > 50000)
					OccupancyGrid[55 * (j >> 1) + (k >> 1)] |= 32 | (2 << ((k & 1) | ((j & 1) << 1)));

				box[0].n += 750;
				box[1].n += 750;
			}

			box[0].t += 750;
			box[1].t += 750;
		}
	}
}

// [0x50da77] [D]
// Fills the occupancy grid with the 55 x 55 road map cells around cell (x, z) - a row may span
// two regions - adds the other cars and derives BlockedGrid and CostGrid.
void GenerateOccupancyGrid(int x, int z, COP *cd)
{
	int *dst;
	int col;
	int colOfs;
	int rowOfs;
	int region;
	int cell;
	int first;
	int rest;
	int i;

	dst = OccupancyGrid;

	x -= 27;
	z -= 27;

	col = x / road_region_size;
	colOfs = x % road_region_size;
	rowOfs = z % road_region_size;
	region = (2 * (regions_down - 1 - z / road_region_size)) & 2;
	cell = road_region_size * rowOfs;

	if (colOfs + 55 <= road_region_size - 1)
	{
		for (i = 0; i < 55; i++)
		{
			region = (col & 1) | (region & 2);

			CopyRun(&RoadMapInfoRegions[region][cell + colOfs], dst, 55);
			dst += 55;

			if (rowOfs + i == road_region_size - 1)
			{
				cell = 0;
				region ^= 2;
			}
			else
			{
				cell += road_region_size;
			}
		}
	}
	else
	{
		for (i = 0; i < 55; i++)
		{
			region = (col & 1) | (region & 2);

			first = road_region_size - colOfs;

			if (first != 0)
				CopyRun(&RoadMapInfoRegions[region][cell + colOfs], dst, first);

			dst += first;

			rest = colOfs + 55 - road_region_size;

			if (rest != 0)
				CopyRun(&RoadMapInfoRegions[region ^ 1][cell], dst, rest);

			dst += rest;

			if (rowOfs + i == road_region_size - 1)
			{
				cell = 0;
				region ^= 2;
			}
			else
			{
				cell += road_region_size;
			}
		}
	}

	AddOtherCarsToOccupancyGrid(cd);

	// NOTE: assembly in the original, 4 cells per step for 756 steps: the last cell (3024) is
	// left as it was
	for (i = 0; i < 3024; i++)
		BlockedGrid[i] = OccupancyGrid[i] & 0x1E;

	for (i = 0; i < 3024; i++)
		CostGrid[i] = CostValues[OccupancyGrid[i]];
}

// [0x50dd4b] [D]
// NOTE: no name in the database and no callers; what it is for is a guess. Makes the 3 x 3 cells
// around the grid centre behind the cop's current route direction 32 dearer.
void sub_50DD4B(COP *cd)
{
	COP_ROUTE_ENTRY *entry;
	char *behind;
	int n;
	int j;

	n = cd->current_node + num_nodes_to_copy - 1;

	for (entry = cd->route_entries; n-- != 0; entry = entry->next)
		;

	behind = byte_5769AC[(unsigned char)entry->flags];

	for (j = 0; j < 9; j++)
	{
		if (behind[j])
			CostGrid[55 * (j / 3) + 1456 + j % 3] += 32;
	}
}

// [0x50de29] [D]
// Debug display of the occupancy grid, 4 x 4 pixels a cell: cost 3 dark blue, cost 7 green,
// blocked dark red, part blocked cells as quarters (blocked quarters red, or yellow with a car;
// free ones green), the centre grey.
void DisplayOccupancyGrid(void)
{
	int *cell;
	int value;
	int cost;
	int colour;
	int blocked;
	int quarter;
	unsigned int i;
	unsigned int j;

	cell = OccupancyGrid;

	// NOTE: uninitialised in the original; every cost in CostValues sets it below
	colour = -1;

	for (i = 0; i < 55; i++)
	{
		for (j = 0; j < 55; j++)
		{
			value = *cell++;
			cost = CostValues[value];

			if (cost > 9)
			{
				if (cost == 32)
					colour = 0x407F7F;
				else if (cost == 4096)
					colour = 0x7F0000;
			}
			else if (cost >= 8)
			{
				blocked = CostValues[value] == 9 ? 0x7F7F00 : 0x7F0000;

				quarter = (value & 2) ? blocked : 0x7F00;
				DrawQuad((float)(4 * j), (float)(4 * i), 2.0f, 2.0f, quarter);

				quarter = (value & 4) ? blocked : 0x7F00;
				DrawQuad((float)(4 * j + 2), (float)(4 * i), 2.0f, 2.0f, quarter);

				quarter = (value & 8) ? blocked : 0x7F00;
				DrawQuad((float)(4 * j), (float)(4 * i + 2), 2.0f, 2.0f, quarter);

				quarter = (value & 0x10) ? blocked : 0x7F00;
				DrawQuad((float)(4 * j + 2), (float)(4 * i + 2), 2.0f, 2.0f, quarter);

				colour = -1;
			}
			else if (cost == 3)
			{
				colour = 0x40;
			}
			else if (cost == 7)
			{
				colour = 0x7F00;
			}

			if (j == 27 && i == 27)
				colour = 0x7F7F7F;

			if (colour != -1)
				DrawQuad((float)(4 * j), (float)(4 * i), 4.0f, 4.0f, colour);
		}
	}
}

// [0x50e130] [D]
void InitialiseAStar(void)
{
	int i;

	for (i = 0; i < 8; i++)
		offset_table[i] = 55 * xy_offset_table[i].y + xy_offset_table[i].x;

	polyAstar[0].next = &polyAstar[1];
	polyAstar[1].next = &polyAstar[2];
	polyAstar[2].next = &polyAstar[3];
	polyAstar[3].next = NULL;

	num_open_nodes = 0;
}

// A node in sub_50E1A7: a copy of entry between entry and next, moved by (dx, dz) in the world
// and (gx, gz) on the debug grid.
static void InsertRouteNode(COP_ROUTE_ENTRY *route, COP_ROUTE_ENTRY *entry, COP_ROUTE_ENTRY *next, int gx, int gz, int dx, int dz)
{
	COP_ROUTE_ENTRY *node;

	node = &route[num_route_entries++];
	*node = *entry;
	entry->next = node;

	node->inserted_node = 1;
	node->copied = 0;
	node->value = 0;
	node->next = next;
	node->prev = entry;
	next->prev = node;

	node->field_14 += gx;
	node->field_16 += gz;
	node->x += dx;
	node->z += dz;
}

// [0x50e1a7] [A]
// NOTE: no name in the database (called by GetRouteAStar when the search reaches the target).
// The eight node insertions, the same code with other offsets in the original, are one static
// helper (InsertRouteNode).
// Builds the cop's route entries from the nodes found (with the saved part of the old route in
// front when routeInMemory): world positions of the cells, the direction to the next one (flags:
// 0 - 7 as xy_offset_table), an extra node before a diagonal step past a blocked cell, then drops
// needless nodes, moves the nodes in cells with a car aside, removes doubles and sets the
// directions. 0 if the route is empty or turns by 90 degrees or more at a node.
int sub_50E1A7(COP *cd)
{
	COP_ROUTE_ENTRY *route;
	COP_ROUTE_ENTRY *entry;
	COP_ROUTE_ENTRY *next;
	COP_ROUTE_ENTRY *next2;
	COP_ROUTE_ENTRY *prev;
	COP_ROUTE_NODE *node;
	int offset;
	int x;
	int z;
	int gx;
	int gz;
	int dx;
	int dz;
	int dir;
	int i;

	offset = node_offset;
	route = cd->route_entries;

	// the target position rounded to the nearest cell corner
	x = 750 * (cd->targetPosX < 0 ? -1 : 1) + cd->targetPosX;
	z = 750 * (cd->targetPosZ < 0 ? -1 : 1) + cd->targetPosZ;
	x -= x % 1500;
	z -= z % 1500;

	while (offset != -1)
	{
		offset = nodes[offset].offset;
		num_route_entries++;
	}

	// the nodes run from the target back to the start: fill the entries from the end
	offset = node_offset;
	entry = &route[num_route_entries - 1];

	if (cd->routeInMemory == 1)
		entry += num_nodes_to_copy;

	for (i = 0; i < num_route_entries; i++)
	{
		node = &nodes[offset];

		entry->x = node->x;
		entry->z = -node->y;
		entry->offset = offset;
		entry->value = CostGrid[offset];

		if (i != 0)
			entry->next = entry + 1;
		else
			entry->next = NULL;

		if (i == num_route_entries - 1)
			entry->prev = NULL;
		else
			entry->prev = entry - 1;

		offset = node->offset;
		entry->copied = 0;
		entry->inserted_node = 0;
		entry--;
	}

	if (cd->routeInMemory == 1)
	{
		entry = cd->route_entries;

		for (i = 0; i < num_nodes_to_copy; i++)
		{
			*entry = save_path[i];

			if (i != 0)
				entry->prev = entry - 1;
			else
				entry->prev = NULL;

			entry->next = entry + 1;
			entry->copied = 1;
			entry++;
		}

		entry->prev = entry - 1;

		num_route_entries += num_nodes_to_copy;
	}

	gx = 4 * (cd->route_entries[0].x + 27);
	gz = 4 * (27 - cd->route_entries[0].z);

	if (cd->routeInMemory == 1)
		entry = &route[num_nodes_to_copy];
	else
		entry = cd->route_entries;

	// NOTE: uninitialised in the original; kept from the previous entry when two entries are in
	// the same cell
	dir = 0;

	while (entry)
	{
		next = entry->next;

		if (entry->inserted_node == 0)
		{
			if (OverlayOccupancyGrid)
				DrawQuad((float)gx, (float)gz, 4.0f, 4.0f, 0x1F1F1F);

			if (next)
			{
				dx = entry->x - next->x;
				dz = next->z - entry->z;

				if (dz < 0)
				{
					if (dx < 0)
						dir = 7;
					else
						dir = dx > 0;
				}
				else if (dz == 0)
				{
					if (dx < 0)
						dir = 6;
					else if (dx > 0)
						dir = 2;
				}
				else
				{
					if (dx < 0)
						dir = 5;
					else if (dx == 0)
						dir = 4;
					else
						dir = 3;
				}

				entry->flags = dir;

				if (entry->copied == 0)
				{
					entry->x = x + 1500 * entry->x;
					entry->z = z + 1500 * entry->z;
					entry->field_14 = gx;
					entry->field_16 = gz;
				}

				// a diagonal step past a blocked cell goes round its corner
				if (entry->value != 9)
				{
					switch (dir)
					{
						case 1:
							if (BlockedGrid[entry->offset + 55])
								InsertRouteNode(route, entry, next, -3, 1, -1125, -375);
							else if (BlockedGrid[entry->offset - 1])
								InsertRouteNode(route, entry, next, -1, 3, -375, -1125);
							break;
						case 3:
							if (BlockedGrid[entry->offset - 55])
								InsertRouteNode(route, entry, next, -3, -1, -1125, 375);
							else if (BlockedGrid[entry->offset - 1])
								InsertRouteNode(route, entry, next, -1, -3, -375, 1125);
							break;
						case 5:
							if (BlockedGrid[entry->offset - 55])
								InsertRouteNode(route, entry, next, 3, -1, 1125, 375);
							else if (BlockedGrid[entry->offset + 1])
								InsertRouteNode(route, entry, next, 1, -3, 375, 1125);
							break;
						case 7:
							if (BlockedGrid[entry->offset + 55])
								InsertRouteNode(route, entry, next, 3, 1, 1125, -375);
							else if (BlockedGrid[entry->offset + 1])
								InsertRouteNode(route, entry, next, 1, 3, 375, -1125);
							break;
					}
				}

				gx -= 4 * dx;
				gz -= 4 * dz;
			}
			else
			{
				entry->x = x + 1500 * entry->x;
				entry->z = z + 1500 * entry->z;
			}

			if (next == NULL)
			{
				entry->field_14 = gx;
				entry->field_16 = gz;
			}
		}

		entry = next;
	}

	// drop the nodes the inserted ones make needless
	// NOTE: from the disassembly; Hex-Rays mixes up next and next->next here
	if (cd->routeInMemory == 1)
		entry = &route[num_nodes_to_copy];
	else
		entry = cd->route_entries;

	while (entry)
	{
		next = entry->next;
		prev = entry->prev;

		if (prev && prev->inserted_node == 0 && next)
		{
			if (next->inserted_node)
			{
				prev->next = next;
				next->prev = prev;
				num_route_entries--;
			}
			else
			{
				next2 = next->next;

				if (next2)
				{
					if (entry->inserted_node)
					{
						if (next2->inserted_node == 0 || entry->flags == next2->flags)
						{
							next = next2;
							entry->next = next;
							next->prev = entry;
							num_route_entries--;
						}
					}
					else if ((entry->flags & 1) != (next->flags & 1))
					{
						if (entry->flags == next2->flags && next2->inserted_node == 0)
						{
							next = next2->next;
							entry->next = next;

							if (next)
								next->prev = entry;

							num_route_entries -= 2;
						}
						else
						{
							next = next2;
							entry->next = next;
							next->prev = entry;
							num_route_entries--;
						}
					}
				}
			}
		}

		entry = next;
	}

	// a node in a cell with a car (cost 9) moves to the free side of the cell
	for (entry = cd->route_entries; entry; entry = next)
	{
		next = entry->next;

		if (entry->value != 9 || entry->prev == NULL)
			continue;

		switch (entry->prev->flags)
		{
			case 0:
			case 4:
				if (OccupancyGrid[entry->offset] & 0xA)
				{
					entry->field_14++;
					entry->x += 375;
				}
				else
				{
					entry->field_14--;
					entry->x -= 375;
				}
				break;
			case 2:
			case 6:
				if (OccupancyGrid[entry->offset] & 6)
				{
					entry->field_16++;
					entry->z -= 375;
				}
				else
				{
					entry->field_16--;
					entry->z += 375;
				}
				break;
		}
	}

	for (entry = cd->route_entries; entry; entry = next)
	{
		next = entry->next;

		if (next && entry->x == next->x && entry->z == next->z)
		{
			next = next->next;
			entry->next = next;

			if (next)
				next->prev = entry;

			num_route_entries--;
			next = entry;
		}
	}

	if (num_route_entries < 1)
		return 0;

	for (entry = cd->route_entries; entry; entry = next)
	{
		next = entry->next;

		if (next)
			entry->direction = Arc_Tan(next->x - entry->x, next->z - entry->z) & 0xFFF;
		else
			entry->direction = -1;
	}

	for (entry = cd->route_entries; entry; entry = next)
	{
		next = entry->next;

		if (next && next->direction != -1 && abs(DIFF_ANGLES(next->direction, entry->direction)) >= 1024)
			return 0;
	}

	return 1;
}

// [0x50f10b] [D]
// Debug display of a cop route in the world (with cameraview 6 or OverlayOccupancyGrid): a square
// at each node, nodes in a cell with a car coloured by the direction they come from, and lines
// between them; faded (dimmer colours) for 1.
void ShowRoute(COP_ROUTE_ENTRY *rep, int faded)
{
	VECTOR4 v;
	float scale;
	float sx;
	float sy;
	float size;
	float lastx;
	float lasty;
	int depth;
	int flags;
	int colour;

	if (gCheatFlags.OverheadView || (cameraview != 6 && !OverlayOccupancyGrid))
		return;

	lasty = -1.0f;
	lastx = (float)rep->field_14;

	while (rep)
	{
		v.vx = (float)rep->x - camera_position.vx;
		v.vy = 8.0f - camera_position.vy;
		v.vz = (float)rep->z - camera_position.vz;

		CurrentRotationMatrix = &camera_matrix;
		MatrixRotateB(&v);

		depth = (int)v.vz;

		if (depth > 50 && depth < 45000)
		{
			scale = DIST * ooz_table[depth];
			sx = v.vx * scale + screen_xmid - 1048576.0f;
			sy = v.vy * scale + screen_ymid - 1048576.0f;
			size = 500.0f * scale;

			if (rep->value == 9)
			{
				if (rep->prev)
				{
					flags = (unsigned char)rep->prev->flags;

					if (flags == 2 || flags == 6)
						colour = faded == 1 ? 0x7F8000 : 0xFF0000;
					else if (flags != 0 && flags != 4)
						colour = faded == 1 ? 0x7F : 0xFF;
					else
						colour = faded == 1 ? 0x7F80 : 0xFF00;

					DrawQuad(sx - size / 2.0f, sy - size / 2.0f, size, size, colour);
				}
			}
			else
			{
				colour = faded == 1 ? 0x7FC422 : 0xFF8844;
				DrawQuad(sx - size / 2.0f, sy - size / 2.0f, size, size, colour);
			}

			if (lastx != -1.0f && lasty != -1.0f)
			{
				colour = faded == 1 ? 0x7FFFFF : 0xFFFFFF;
				DrawLine(float_to_int(lastx), float_to_int(lasty), float_to_int(sx), float_to_int(sy), colour);
			}

			lastx = sx;
			lasty = sy;
		}

		rep = rep->next;
	}
}

// [0x50f5c2] [D]
// A* from cell (x, y) (relative to the target cell, at grid offset index) to the target
// (node_offset); a step costs CostGrid of the cell, a diagonal step 2 more and only past a free
// orthogonal neighbour. Keeps num_nodes_to_copy entries of the current route from current_node in
// save_path first (routeInMemory). Returns sub_50E1A7 for the route found, 0 if none.
int GetRouteAStar(COP *cd, int x, int y, int index)
{
	COP_ROUTE_ENTRY *entry;
	COP_ROUTE_ENTRY *saved;
	COP_ROUTE_NODE *nodeA;
	COP_ROUTE_NODE *nodeB;
	XYPAIR tmp;
	int n;
	int nextCookie;
	int prevCookie;
	int offset;
	int cell;
	int cost;
	int dist;
	int nx;
	int ny;
	int wasOpen;
	int child;
	int parent;

	if (abs(x) > 27 || abs(y) > 27)
		return 0;

	if (cd->routeInMemory == 1)
	{
		entry = cd->route_entries;
		n = cd->current_node;
		saved = save_path;

		while (n-- != 0)
			entry = entry->next;

		for (n = 0; n < num_nodes_to_copy; n++)
		{
			*saved++ = *entry;
			entry = entry->next;
		}
	}

	// open: nextCookie == this search, closed: prevCookie == this search
	nextCookie = cd->id + 32 * game_frame;
	prevCookie = nextCookie - 16;

	num_open_nodes = 0;

	nodeB = &nodes[index];
	nodeB->x = x;
	nodeB->y = y;
	nodeB->index = 0;
	nodeB->cost = 2 * (abs(y) + abs(x));
	nodeB->dist = nodeB->cost + nodeB->index;
	nodeB->offset = -1;
	nodeB->nextCookie = nextCookie;

	open_queue[1].y = index;
	open_queue[1].x = nodeB->dist;
	num_open_nodes = 1;

	while (num_open_nodes != 0)
	{
		// take the cheapest open node
		offset = open_queue[1].y;
		open_queue[1] = open_queue[num_open_nodes--];

		parent = 1;
		child = 2;

		while (child < num_open_nodes)
		{
			if (child + 1 < num_open_nodes && open_queue[child].x > open_queue[child + 1].x)
				child++;

			if (open_queue[parent].x <= open_queue[child].x)
				break;

			tmp = open_queue[child];
			open_queue[child] = open_queue[parent];
			open_queue[parent] = tmp;

			parent = child;
			child *= 2;
		}

		if (offset == node_offset)
			return sub_50E1A7(cd);

		nodeA = &nodes[offset];

		node_count = 7;

		do
		{
			cell = offset_table[node_count] + offset;
			cost = CostGrid[cell];

			if (cost >= 4096)
				continue;

			// a diagonal step (even node_count)
			if ((node_count & 1) == 0)
			{
				switch (node_count)
				{
					case 0:
						if (BlockedGrid[offset - 1] && BlockedGrid[offset - 55])
							continue;
						break;
					case 2:
						if (BlockedGrid[offset + 1] && BlockedGrid[offset - 55])
							continue;
						break;
					case 4:
						if (BlockedGrid[offset + 1] && BlockedGrid[offset + 55])
							continue;
						break;
					case 6:
						if (BlockedGrid[offset - 1] && BlockedGrid[offset + 55])
							continue;
						break;
				}

				cost += 2;
			}

			nodeB = &nodes[cell];
			wasOpen = nodeB->nextCookie == nextCookie;
			dist = cost + nodeA->index;

			if (nodeB->index < dist && (nodeB->nextCookie == nextCookie || nodeB->prevCookie == nextCookie))
				continue;

			nx = xy_offset_table[node_count].x + nodeA->x;

			if (nx <= -27 || nx >= 27)
				continue;

			ny = xy_offset_table[node_count].y + nodeA->y;

			if (ny <= -27 || ny >= 27)
				continue;

			nodeB->offset = offset;
			nodeB->index = dist;
			nodeB->prevCookie = prevCookie;
			nodeB->x = nx;
			nodeB->y = ny;
			nodeB->dist = dist;
			nodeB->cost = 2 * (abs(nx) + abs(ny));
			nodeB->dist += nodeB->cost;

			if (!wasOpen)
			{
				open_queue[++num_open_nodes].y = cell;
				open_queue[num_open_nodes].x = nodeB->dist;

				child = num_open_nodes;

				while (child != 1)
				{
					parent = child >> 1;

					if (open_queue[parent].x <= open_queue[child].x)
						break;

					tmp = open_queue[child];
					open_queue[child] = open_queue[parent];
					open_queue[parent] = tmp;

					child = parent;
				}

				nodeB->nextCookie = nextCookie;
			}
		}
		while (node_count-- != 0);

		nodeA->nextCookie = prevCookie;
		nodeA->prevCookie = nextCookie;
	}

	return 0;
}

// [0x50fa9a] [D]
// NOTE: assembly in the original, which gets the cell address + 2 bytes and reads unaligned
// dwords; here src is the cell itself. Road map info cells to occupancy values: bit 0 from bit
// 0, bits 1 - 4 from bits 26 - 29. Two cells a step, so an odd count writes one cell more.
void CopyRun(int *src, int *dst, int count)
{
	do
	{
		dst[0] = (src[0] & 1) | ((src[0] >> 25) & 0x1E);
		dst[1] = (src[1] & 1) | ((src[1] >> 25) & 0x1E);

		src += 2;
		dst += 2;
		count -= 2;
	}
	while (count > 0);
}
