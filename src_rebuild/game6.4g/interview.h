/*
 * interview.h - globals and functions of interview.c used by other files
 */

#ifndef INTERVIEW_H
#define INTERVIEW_H

#include "dr_types.h"

// globals
extern int giInterviewState;

// functions
extern void DisplayTrainingData(void);
extern void DoTrainingMode(char pad);
extern void DrawScaledTexturedQuadSize(float x, float y, float xscale, float yscale, TEX_INFO *tex, int rgb);
extern void EndOfTrainingReplay(void);
extern int GetTotalDamage(CAR_DATA *cp);
extern void HandleCrossouts(void);
extern void InitInterview(void);
extern void InitRecTrainingModeCounter(void);

#endif // INTERVIEW_H
