/*
 * checkpoint.h - globals and functions of checkpoint.c used by other files
 */

#ifndef CHECKPOINT_H
#define CHECKPOINT_H

#include "dr_types.h"

// globals
// most references: checkpoint.c 1, effects.c 1
extern MODEL *gTwoModel;
// most references: checkpoint.c 5, effects.c 1
extern MODEL *gRaceOverModel;
extern MODEL *gCheckPointModel;
extern MODEL *gOneModel;
extern MODEL *gThreeModel;
extern MODEL *gGoModel;

// functions
extern void AddCheckPoint(int x, int z);
extern void DisplayCheckpointData(CHECKPOINT_DATA *checkpoint);
extern void DisplayCheckpointText(CHECKPOINT_DATA *checkpoint);
extern void HandleCheckpoint(CHECKPOINT_DATA *checkpoint);
extern void InitCheckPointMode(void);

#endif // CHECKPOINT_H
