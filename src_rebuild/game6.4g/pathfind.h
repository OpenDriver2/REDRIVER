/*
 * pathfind.h - globals and functions of pathfind.c used by other files
 */

#ifndef PATHFIND_H
#define PATHFIND_H

#include "dr_types.h"

// NOTE: IDA has no reference from cops.c, which reads
// CostGrid[1512] through an offset)
extern int CostGrid[3025];

// globals
extern int CostValues[64];
extern int num_nodes_to_copy;
extern unsigned char num_close_cars;
// most references: pathfind.c 3, cops.c 2, general.c 2
extern char OverlayOccupancyGrid;
extern char close_cars[64];
extern int num_route_entries;

// functions
extern void DisplayOccupancyGrid(void);
extern void GenerateOccupancyGrid(int x, int z, COP *cd);
extern int GetRouteAStar(COP *cd, int x, int y, int index);
extern void InitPathFind(void);
extern void InitialiseAStar(void);
extern void ShowRoute(COP_ROUTE_ENTRY *rep, int faded);

#endif // PATHFIND_H
