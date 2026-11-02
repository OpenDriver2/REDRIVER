/*
 * replays.c - pad recording and playback (replays, ghost laps, mission chases)
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "replays.c"); partially translated.
 */

#include "dr_types.h"
#include "replays.h"
#include "dr_globals.h"
#include "MScript.h"
#include "handling.h"
#include "input.h"
#include "shell.h"

//------------------------------------------------------------------------------------------
// uninitialised data

int gStartGhostLap;			// NOTE: owner guessed (most references)
int gEndGhostLap;
int ChaseHQReplay;

// [0x51b440] [D]
// Feeds this frame's pad to the player's car (and the chased car 65), recorded or played back
// as ReplayMode says, and adds the cars to the physics.
void ControlCars(void)
{
	int joyX;
	int joyY;
	int pad;

	if (gStartGhostLap)
		StartOfGhostLap();

	if (gEndGhostLap)
		EndOfGhostLap(mission.best_lap);

	gStartGhostLap = 0;
	gEndGhostLap = 0;

	if (ReplayMode != 3 || (gDoTraining && gTrainingMode == 3))
	{
		if (ReplayMode == 1)
		{
			if (gMissionRecording)
			{
				GetRecordingPad(1);
				ProcessCarPad(65, controller_bits);
				StepCarPhysics(&car_data[65], 1);
			}
			else
			{
				GetRecordingPad(0);
				ProcessCarPad(0, controller_bits);
				StepCarPhysics(car_data, 1);
			}
		}
		else if (ReplayMode == 2)
		{
			pad = GetRecordedPad(0);

			ProcessCarPad(0, pad);
			StepCarPhysics(car_data, 1);
		}
	}
	else if (gDoTraining && gTrainingMode == 2)
	{
		joyX = JoyAnalogueX;
		joyY = JoyAnalogueY;

		PlayGhostLap();

		JoyAnalogueX = joyX;
		JoyAnalogueY = joyY;

		GetRecordingPad(0);
		ProcessCarPad(0, controller_bits);
		StepCarPhysics(car_data, 1);
	}
	else
	{
		if (mission.objective == 0x7FFF)
			ProcessCarPad(65, CAR_PAD_HANDBRAKE);
		else
			ProcessCarPad(65, GetRecordedPad(1));

		StepCarPhysics(&car_data[65], 1);

		if (!gDoTraining || gTrainingMode != 0)
		{
			if (ChaseHQReplay)
				controller_bits = GetRecordedPad(0);
			else
				GetRecordingPad(0);

			ProcessCarPad(0, controller_bits);
			StepCarPhysics(car_data, 1);
		}
	}
}
