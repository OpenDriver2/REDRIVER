/*
 * dr_time.h - globals and functions of time.c used by other files
 */

#ifndef DR_TIME_H
#define DR_TIME_H

#include "dr_types.h"

// functions
extern int CreateTimer(UINT delay, UINT event, void (*callback)(void));
extern void DestroyTimer(int timer);
extern int InitTimers(void);
extern void StopTimers(void);

#endif // DR_TIME_H
