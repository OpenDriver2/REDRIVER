/*
 * mladder/mladder.h - globals and functions of mladder/mladder.c used by other files
 */

#ifndef MLADDER_MLADDER_H
#define MLADDER_MLADDER_H

#include "dr_types.h"

// globals
extern MISSION_LADDER mladder;

// functions
extern void FreeMissionLadder(void);
extern int InitMissionLadder(void);
extern void RunMissionLadder(void);

#endif // MLADDER_MLADDER_H
