/*
 * felony.h - globals and functions of felony.c used by other files
 */

#ifndef FELONY_H
#define FELONY_H

#include "dr_types.h"

// NOTE: IDA mark @felony.c;unused; InitCops clears it
extern int dword_D90C5C;

// globals
extern FELONY_DATA gFelonyData;
extern int LastFelony;
extern COP_SIGHT_DATA CopSightData;
extern int cop_respawn_timer;
extern int FelonyRating;

// functions
extern void AdjustFelony(FELONY_DATA *pFelonyData);
extern void CheckPlayerMiscFelonies(FELONY_DATA *pFelonyData);
extern int CopCanSeePlayer(COP_SIGHT_DATA *copSightData, COP *cop);
extern void IncreaseFelonyRating(int howMuch);
extern void InitialiseFelonyData(FELONY_DATA *pFelonyData);
extern void ManageCopSightData(COP_SIGHT_DATA *copSightData);
extern void NoteFelony(FELONY_DATA *pFelonyData, char type, int scale);
extern void PlayCopSpeech(int id);

#endif // FELONY_H
