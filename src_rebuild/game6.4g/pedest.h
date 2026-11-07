/*
 * pedest.h - globals and functions of pedest.c used by other files
 */

#ifndef PEDEST_H
#define PEDEST_H

#include "dr_types.h"

// globals
// most references: frontend/details.c 12, pedest.c 8, frontend/config.c 6
extern int max_pedestrians;
// NOTE: untyped in IDA (4 bytes)
// most references: pedest.c 13, MScript.c 1
extern int pedestrian_count;
extern PEDESTRIAN pedestrians[64];

// functions
extern void CheckHornReaction(CAR_DATA *cp);
extern void DisplayPedestrians(void);
extern void HandlePedestrians(void);
extern void InitPedestrians(void);

#endif // PEDEST_H
