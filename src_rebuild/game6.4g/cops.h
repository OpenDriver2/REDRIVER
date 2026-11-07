/*
 * cops.h - globals and functions of cops.c used by other files
 */

#ifndef COPS_H
#define COPS_H

#include "dr_types.h"

// NOTE: an unnamed IDA global; InitialiseFelonyData clears it
extern int dword_11ACCB4;

// globals
extern int LastCopMessage;
extern int LastHeading;
extern COP_DATA gCopData;
// most references: cops.c 6, MScript.c 2, ai.c 1
extern int max_active_cops;
// most references: MScript.c 5, cops.c 3
extern int CopsAllowed;
// most references: ai.c 3, cops.c 1
extern int RoadblockTimer;
// most references: cops.c 12, MScript.c 2, overlay.c 2
extern int player_position_known;
extern int gCopsSpeedScale;
extern int gCopsAutoSpeedScale;
// most references: ai.c 11, cops.c 9, glaunch.c 1
extern int num_active_cops;
// most references: cops.c 5, glaunch.c 1
extern int gBatterPlayer;
extern int gCopsPowerScale;
extern int gCopsImmortal;
// most references: cops.c 9, glaunch.c 1
extern int suggested_active_cops;
// most references: cops.c 5, MScript.c 2, felony.c 2
extern int CopsCanSeePlayer;
// most references: cops.c 9, ai.c 5, gsound.c 2
extern COP cop_data[8];
// most references: cops.c 1, glaunch.c 1
extern int gPuppyDogCop;
extern int CopsAutoPowerScale;

// functions
extern void AddFelonyPoints(int howMuch);
extern void ControlCopDetectModes(CIV_STATE *ai);
extern void ControlCops(void);
extern void CopState_PURSUIT(CIV_STATE *ai);
extern char GetCarDirectionOfTravel(CAR_DATA *cp);
extern int GetCopCarSlot(void);
extern void InitCops(void);

#endif // COPS_H
