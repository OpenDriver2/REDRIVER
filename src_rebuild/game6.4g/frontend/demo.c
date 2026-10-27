/*
 * frontend/demo.c - rolling demo timer
 *
 * Rebuilt from Driver_sym2_94.i64 (IDA folder "frontend\demo.c"); partially translated.
 */

#include <windows.h>
#include <mmsystem.h>

#include "dr_time.h"
#include "frontend/demo.h"

void ResetDemoTimer(void);
void RunDemoTimer(void);

// initialised data (0x54c1f4)
int demo_timer = -1;

// uninitialised data (0x5d79f0)
int demo_timer_count;

// [0x4088e4] [D]
// Counts seconds in demo_timer_count.
void StartDemoTimer(void)
{
	if (demo_timer != -1)
		ResetDemoTimer();

	demo_timer_count = 0;
	demo_timer = CreateTimer(1000, TIME_PERIODIC, RunDemoTimer);
}

// [0x40891a] [D]
// NOTE: demo_timer is not reset to -1 here (original behaviour)
void ResetDemoTimer(void)
{
	if (demo_timer != -1)
		DestroyTimer(demo_timer);

	demo_timer_count = 0;
}

// [0x408940] [D]
void RunDemoTimer(void)
{
	demo_timer_count++;
}
