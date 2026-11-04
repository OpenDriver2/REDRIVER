/*
 * frontend/demo.c - rolling demos (the Film Director's "Play Chase" replays), demo timer
 *
 * Rebuilt from Driver_sym2_94.i64 (IDA folder "frontend\demo.c").
 */

#include <windows.h>
#include <mmsystem.h>

#include "dr_time.h"
#include "frontend/demo.h"
#include "frontend/frontend.h"
#include "replays.h"
#include "shell.h"

void ResetDemoTimer(void);
void RunDemoTimer(void);

// initialised data (0x54c1e0)

// the rolling demo replays (ReplayData\User\DEMOn.CFG ...)
// NOTE: IDA types it char *[4]; the NULL after it (0x54c1f0) ends the list StartRollingDemo walks
char *demos[5] = { "DEMO0", "DEMO1", "DEMO2", "DEMO3", NULL };

int demo_timer = -1;

// uninitialised data (0x5d79f0)
int demo_timer_count;
int demo;					// the next demo StartRollingDemo(-1) plays

// [0x408830] [D]
// Plays replay demos[n], or with -1 the next one in turn.
void StartRollingDemo(int n)
{
	gBotchRoads = 1;
	gbPlayingDemo = 1;

	if (n == -1)
		InitReplay(demos[demo]);
	else
		InitReplay(demos[n]);

	StartGameFromFrontend(1, 1);

	gbPlayingDemo = 0;
	gBotchRoads = 0;

	if (n == -1 && demos[++demo] == NULL)
		demo = 0;
}

// [0x4088c9] [D]
// NOTE: unreferenced; starts game mode 661
void sub_4088C9(void)
{
	GameMode = 661;
	StartGameFromFrontend(1, 0);
}

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
