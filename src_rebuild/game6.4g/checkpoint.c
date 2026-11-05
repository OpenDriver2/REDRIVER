/*
 * checkpoint.c - the mission checkpoint, the race countdown and their 3D words
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "checkpoint.c", 0x4a4df0 - 0x4a5397).
 *
 * The mission has one CHECKPOINT_DATA, Checkpoint (mission.checkpoint), set up by the script's
 * CHECKPOINT and INITIALISERACE commands. type 1 is the countdown, 2 a checkpoint to reach.
 * display_flags 0x100 shows the CHECKPOINT word, 0x200 the checkpoint itself, 4 is set when the
 * countdown ends; flags 0x80 means the checkpoint was reached. step counts the frames of the word
 * on screen.
 */

#include <stdlib.h>
#include <string.h>

#include "dr_types.h"
#include "checkpoint.h"
#include "dr_globals.h"
#include "effects.h"
#include "handling.h"
#include "map.h"
#include "MScript.h"
#include "sound.h"

// NOTE: these sit in the linker's common area; owner by their only writer, InitCheckPointMode
MODEL *gTwoModel;
MODEL *gRaceOverModel;
MODEL *gCheckPointModel;
MODEL *gOneModel;
MODEL *gThreeModel;
MODEL *gGoModel;

static void HandleCheckpointLogo(CHECKPOINT_DATA *checkpoint);
static void HandleCountdown(CHECKPOINT_DATA *checkpoint);
static void DisplayCountdown(CHECKPOINT_DATA *checkpoint);

// [0x4a4df0] [D]
// Finds the word models in the player's language, clears the checkpoint.
void InitCheckPointMode(void)
{
	gThreeModel = FindModelPtrWithName("3");
	gTwoModel = FindModelPtrWithName("2");
	gOneModel = FindModelPtrWithName("1");

	switch (gUserLanguage)
	{
		case 0:
		case 5:
			gGoModel = FindModelPtrWithName("GO");
			gCheckPointModel = FindModelPtrWithName("CHECKPOINT");
			gRaceOverModel = FindModelPtrWithName("FINISHED");
			break;
		case 1:
			gGoModel = FindModelPtrWithName("GO");
			gCheckPointModel = FindModelPtrWithName("CHECKPOINT");
			gRaceOverModel = FindModelPtrWithName("FR_TERMINE");
			break;
		case 2:
			gGoModel = FindModelPtrWithName("GR_LOS");
			gCheckPointModel = FindModelPtrWithName("GR_KONTROLLPUNKT");
			gRaceOverModel = FindModelPtrWithName("GR_GESCHAFFT");
			break;
		case 3:
			gGoModel = FindModelPtrWithName("SP_ADELANTE");
			gCheckPointModel = FindModelPtrWithName("SP_PUNTO-DE-CONTROL");
			gRaceOverModel = FindModelPtrWithName("SP_SE-ACABO");
			break;
		case 4:
			gGoModel = FindModelPtrWithName("IT_VIA");
			gCheckPointModel = FindModelPtrWithName("IT_POSTO-DI-CONTROLLO");
			gRaceOverModel = FindModelPtrWithName("IT_FINE");
			break;
	}

	memset(&Checkpoint, 0, sizeof(Checkpoint));

	Checkpoint.timer.flags = 0x8C;
}

// [0x4a4f99] [D]
// Places the checkpoint at x, z and shows it.
void AddCheckPoint(int x, int z)
{
	Checkpoint.position.vx = x;
	Checkpoint.position.vz = z;
	Checkpoint.type = 2;
	Checkpoint.display_flags |= 0x200;
}

// [0x4a4fc8] [D]
// Every frame: the checkpoint's map target, timer and countdown, and whether the player reached it.
void HandleCheckpoint(CHECKPOINT_DATA *checkpoint)
{
	int dx;
	int dz;

	if (!checkpoint->display_flags)
		return;

	if (checkpoint->display_flags & 0x200)
	{
		MissionTargets[0].position = checkpoint->position;
		MissionTargets[0].flags = 1;
	}
	else
	{
		MissionTargets[0].flags = 0;
	}

	DMSHandleTimer(&checkpoint->timer);
	checkpoint->step++;

	if (checkpoint->type == 1)
	{
		HandleCountdown(checkpoint);
		return;
	}

	if (checkpoint->display_flags & 0x100)
		HandleCheckpointLogo(checkpoint);

	if ((checkpoint->flags & 0x80) || !(checkpoint->display_flags & 0x200))
		return;

	dx = checkpoint->position.vx - car_data[0].location.vx;
	dz = checkpoint->position.vz - car_data[0].location.vz;

	if (abs(dx) < 500 && abs(dz) < 500 && dz * dz + dx * dx < checkpoint->radius)
	{
		MissionTargets[0].flags = 0;

		checkpoint->flags |= 0x80;
		checkpoint->display_flags &= ~0x200;
		checkpoint->step = 0;

		if (mission.objective == 1)
			mission.objective = 0x7FFF;
	}
}

// [0x4a515a] [D]
// Draws the checkpoint model on the road and the countdown.
void DisplayCheckpointData(CHECKPOINT_DATA *checkpoint)
{
	if (!checkpoint || !checkpoint->display_flags)
		return;

	checkpoint->position.vy = -MapHeightInt(&checkpoint->position);

	if (checkpoint->display_flags & 0x200)
		DrawCheckpoint(checkpoint);

	if (checkpoint->type == 1)
		DisplayCountdown(checkpoint);
}

// [0x4a51b6] [D]
// Draws the timer and the word on screen: CHECKPOINT, race over or GO.
void DisplayCheckpointText(CHECKPOINT_DATA *checkpoint)
{
	if (!checkpoint || !checkpoint->display_flags)
		return;

	DisplayTimer(&checkpoint->timer);

	if (checkpoint->type == 1)
		return;

	if (checkpoint->display_flags & 0x100)
		DrawWord(4, checkpoint->step);
	else if (mission.objective == 0x80F1)
		DrawWord(5, checkpoint->step);
	else if (checkpoint->timer.count < 30)
		DrawWord(3, checkpoint->step);
}

// [0x4a5240] [D]
// Takes the CHECKPOINT word off after 30 frames.
static void HandleCheckpointLogo(CHECKPOINT_DATA *checkpoint)
{
	if (checkpoint->step >= 30)
	{
		checkpoint->display_flags &= ~0x100;
		checkpoint->step = 0;
	}
}

// [0x4a5267] [D]
// Counts 3, 2, 1 with a beep every 30 frames, then starts the race. The timer's count is the
// frame counter here.
static void HandleCountdown(CHECKPOINT_DATA *checkpoint)
{
	if (++checkpoint->timer.count == 1)
	{
		SimplePlaySound(16, 16, -1);
	}
	else if (checkpoint->timer.count == 30 || checkpoint->timer.count == 60)
	{
		checkpoint->step = 0;
		SimplePlaySound(16, 16, -1);
	}
	else if (checkpoint->timer.count >= 90)
	{
		SimplePlaySound(17, 16, -1);

		checkpoint->step = 0;
		checkpoint->type = 0;
		checkpoint->display_flags |= 4;
		checkpoint->timer.count = 0;
		checkpoint->timer.flags = 0x8C;

		if (mission.objective == 3)
			mission.objective = 0x7FFF;
	}
}

// [0x4a5340] [D]
// Draws 3, 2 or 1.
static void DisplayCountdown(CHECKPOINT_DATA *checkpoint)
{
	if (checkpoint->timer.count < 30)
		DrawWord(0, checkpoint->step);
	else if (checkpoint->timer.count < 60)
		DrawWord(1, checkpoint->step);
	else if (checkpoint->timer.count < 90)
		DrawWord(2, checkpoint->step);
}
