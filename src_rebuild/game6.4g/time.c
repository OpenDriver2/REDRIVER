/*
 * time.c - multimedia timers
 *
 * Rebuilt from Driver_sym2_94.i64 (IDA folder "time.c"); partially translated.
 */

#include <windows.h>
#include <mmsystem.h>

#include "debug.h"
#include "dr_time.h"

typedef struct TIMER_INFO
{
	UINT id;
	void (*callback)(void);
} TIMER_INFO;

#define MAX_TIMERS			4
#define TIMER_RESOLUTION	5	// ms, timeBeginPeriod / timeEndPeriod

void DestroyTimer(int timer);
void CALLBACK RunTimer(UINT uTimerID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2);

// uninitialised data (0xc13190)
// NOTE: unmarked; defined here as time.c is the only user
TIMER_INFO Timers[MAX_TIMERS];
// NOTE: unnamed in IDA (dword_C131B0); count of running timers (DestroyTimer decrements it) — name guessed
static int num_timers;

// [0x52a000] [D]
int InitTimers(void)
{
	if (timeBeginPeriod(TIMER_RESOLUTION) != TIMERR_NOERROR)
	{
		LogQuick("<* Init Timers failed *>");
		return 0;
	}

	num_timers = 0;
	return 1;
}

// [0x52a031] [D]
void StopTimers(void)
{
	int i;

	for (i = 0; i < MAX_TIMERS; i++)
		DestroyTimer(i);

	timeEndPeriod(TIMER_RESOLUTION);
	num_timers = 0;
}

// [0x52a071] [D]
// Starts a multimedia timer calling callback every delay ms; returns the timer slot or -1.
int CreateTimer(UINT delay, UINT event, void (*callback)(void))
{
	int i;

	for (i = 0; i < MAX_TIMERS; i++)
	{
		if (!Timers[i].id)
		{
			Timers[i].callback = callback;
			Timers[i].id = timeSetEvent(delay, TIMER_RESOLUTION, RunTimer, 0, event);

			if (Timers[i].id)
				return i;
		}
	}

	return -1;
}

// [0x52a0e3] [D]
void DestroyTimer(int timer)
{
	if (Timers[timer].id)
	{
		timeKillEvent(Timers[timer].id);

		Timers[timer].id = 0;
		Timers[timer].callback = NULL;

		num_timers--;
	}
}

// [0x52a131] [D]
void CALLBACK RunTimer(UINT uTimerID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2)
{
	int i;

	for (i = 0; i < MAX_TIMERS; i++)
	{
		if (Timers[i].id == uTimerID && Timers[i].callback)
		{
			Timers[i].callback();
			return;
		}
	}
}
