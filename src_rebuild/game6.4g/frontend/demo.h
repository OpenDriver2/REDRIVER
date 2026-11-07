/*
 * frontend/demo.h - globals and functions of frontend/demo.c used by other files
 */

#ifndef FRONTEND_DEMO_H
#define FRONTEND_DEMO_H

#include "dr_types.h"

// globals
extern int demo_timer_count;

// functions
extern void ResetDemoTimer(void);
extern void StartDemoTimer(void);
extern void StartRollingDemo(int n);

#endif // FRONTEND_DEMO_H
