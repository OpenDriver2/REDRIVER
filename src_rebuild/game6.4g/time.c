/*
 * time.c - timers
 *
 * Rebuilt from Driver_sym2_94.i64 (IDA folder "time.c").
 */

#include <SDL.h>

#include "debug.h"
#include "dr_time.h"

typedef struct TIMER_INFO
{
	SDL_TimerID id;
	void (*callback)(void);
	int periodic;				// NOTE: [A] SDL has no TIME_PERIODIC, RunTimer decides
} TIMER_INFO;

#define MAX_TIMERS			4

void DestroyTimer(int timer);
Uint32 SDLCALL RunTimer(Uint32 interval, void *param);

// uninitialised data (0xc13190)
// NOTE: unmarked; defined here as time.c is the only user
TIMER_INFO Timers[MAX_TIMERS];
// NOTE: unnamed in IDA (dword_C131B0); count of running timers (DestroyTimer decrements it) — name guessed
static int num_timers;

// [0x52a000] [D] [A]
// NOTE: [A] SDL timers instead of the multimedia timers. The system timer period is SDL's 1 ms
// instead of the original's 5 ms; the timers here are all long ones and frames are paced by
// WaitForSync.
int InitTimers(void)
{
	if (SDL_InitSubSystem(SDL_INIT_TIMER) != 0)
	{
		LogQuick("<* Init Timers failed *>");
		return 0;
	}

	num_timers = 0;
	return 1;
}

// [0x52a031] [D] [A]
void StopTimers(void)
{
	int i;

	for (i = 0; i < MAX_TIMERS; i++)
		DestroyTimer(i);

	SDL_QuitSubSystem(SDL_INIT_TIMER);
	num_timers = 0;
}

// [0x52a071] [D] [A]
// Starts a timer calling callback after delay ms, and every delay ms if event is non zero
// (TIME_PERIODIC, the original passes it to timeSetEvent); returns the timer slot or -1.
int CreateTimer(UINT delay, UINT event, void (*callback)(void))
{
	int i;

	for (i = 0; i < MAX_TIMERS; i++)
	{
		if (!Timers[i].id)
		{
			Timers[i].callback = callback;
			Timers[i].periodic = event != 0;
			Timers[i].id = SDL_AddTimer(delay, RunTimer, &Timers[i]);

			if (Timers[i].id)
				return i;
		}
	}

	return -1;
}

// [0x52a0e3] [D] [A]
void DestroyTimer(int timer)
{
	if (Timers[timer].id)
	{
		SDL_RemoveTimer(Timers[timer].id);

		Timers[timer].id = 0;
		Timers[timer].callback = NULL;

		num_timers--;
	}
}

// [0x52a131] [D] [A]
// NOTE: [A] the slot comes through param, the original looked it up by the timer id
Uint32 SDLCALL RunTimer(Uint32 interval, void *param)
{
	TIMER_INFO *timer;

	timer = (TIMER_INFO *)param;

	if (timer->callback)
		timer->callback();

	return timer->periodic ? interval : 0;
}

// [A]
// Replaces timeGetTime: milliseconds from a fixed start.
unsigned int GetTicksMs(void)
{
	return SDL_GetTicks();
}
