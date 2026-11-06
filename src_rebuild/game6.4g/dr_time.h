/*
 * dr_time.h - globals and functions of time.c used by other files
 */

#ifndef DR_TIME_H
#define DR_TIME_H

#include "dr_types.h"

// functions
extern int CreateTimer(UINT delay, UINT event, void (*callback)(void));

// [A] from mmsystem.h, what CreateTimer takes for event
#define TIME_PERIODIC	1
extern void DestroyTimer(int timer);
extern unsigned int GetTicksMs(void);
extern int InitTimers(void);
extern void StopTimers(void);

#endif // DR_TIME_H
