/*
 * pause.c - the in game pause menu
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "pause.c"); partially translated.
 */

#include "dr_types.h"
#include "dr_globals.h"
#include "pause.h"

//------------------------------------------------------------------------------------------
// uninitialised data

static int bWantPauseGame;
static int bForcePause;

// [0x510a60] [D]
// Asks for the pause menu to open (1) or close (0) at the next CheckPause; asking for the state
// already asked for forces it.
void PauseGame(int pause)
{
	if (pause == bWantPauseGame)
		bForcePause = 1;

	bWantPauseGame = pause;
}

// [0x510a83] [D]
void CheckPause(void)
{
	if (bWantPauseGame)
	{
		if (!GamePaused || bForcePause)
			OpenPauseMenu();
	}
	else if (GamePaused || bForcePause)
	{
		ClosePauseMenu();
	}

	bForcePause = 0;
}
