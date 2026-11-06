/*
 * felony.c - the felony rating: felonies the cops see, the rating rising in a pursuit and falling
 * out of one, what the cops can see, cop speech
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "felony.c", 0x4d40e0 - 0x4d49a5).
 *
 * FelonyRating runs 0 - 4095; from 682 on the cops pursue (cops.c). NoteFelony adds a felony's
 * points (placid or angry, by CopSightData) when a cop sees it and the felony's reoccurrence delay
 * has run out.
 */

#include <stdio.h>
#include <stdlib.h>

#include "dr_types.h"
#include "felony.h"
#include "dr_globals.h"
#include "ai.h"
#include "cops.h"
#include "general.h"
#include "gsound.h"
#include "handling.h"
#include "pause.h"
#include "pres.h"
#include "roadbits.h"
#include "shell.h"
#include "testmath.h"
#include "frontend/cheats.h"
#include "glaunch.h"

int CopCanSeePlayer_SpecialGameModeOnly(COP_SIGHT_DATA *copSightData, COP *cop);
void PlaySpeechSeq(int id1, int id2, int id3, int id4);

// frames between two felonies of a type being noted, per felony type
int initialRecurrenceDelay[11] = { 128, 0, 128, 64, 64, 32, 32, 64, 128, 256, 64 };

// NOTE: typed FELONY_DELAY in IDA; the values are FELONY_VALUE (placid, angry) pairs
FELONY_VALUE initialFelonyValue[11] = {
	{ 682, 0 }, { 682, 75 }, { 682, 0 }, { 682, 0 }, { 682, 682 }, { 682, 682 },
	{ 682, 682 }, { 682, 25 }, { 682, 0 }, { 682, 0 }, { 682, 0 }
};

// speech for the player's heading (north, east, south, west), after 0x8004
int slu[4] = { 0x8005, 0x8007, 0x8006, 0x8008 };

int FelonyDecreaseTime;
int dword_D90C5C;						// NOTE: IDA mark @felony.c;unused
FELONY_DATA gFelonyData;
int LastFelony;
int FelonyDecreaseTimer;
COP_SIGHT_DATA CopSightData;
int cop_respawn_timer;
int FelonyIncreaseTime;
int gbCanSeePlayerInternal;
int FelonyIncreaseTimer;
int gbCanSeePlayerExternal;
int FelonyRating;

// [0x4d40e0] [D]
void InitialiseFelonyData(FELONY_DATA *pFelonyData)
{
	int i;

	FelonyIncreaseTimer = 0;
	FelonyDecreaseTime = 0;
	FelonyIncreaseTime = 0;
	cop_respawn_timer = 0;
	LastCopMessage = 0;
	dword_D90C5C = 0;
	LastFelony = -1;
	FelonyRating = 0;
	FelonyDecreaseTimer = 0;
	dword_11ACCB4 = 0;

	for (i = 0; i < 11; i++)
	{
		pFelonyData->reoccurrenceDelay[i].maximum = initialRecurrenceDelay[i];
		pFelonyData->reoccurrenceDelay[i].current = 0;
		pFelonyData->value[i].placid = initialFelonyValue[i].placid;
		pFelonyData->value[i].angry = initialFelonyValue[i].angry;
	}

	FelonyIncreaseTime = 62;
	FelonyDecreaseTime = 62;
	FelonyIncreaseTimer = 0;
	FelonyDecreaseTimer = 0;
	pFelonyData->pursuitFelonyScale = 1;
}

// [0x4d420c] [D]
// NOTE: no callers
void sub_4D420C(FELONY_DATA *pFelonyData)
{
	CheckPlayerMiscFelonies(pFelonyData);
}

// [0x4d421d] [D]
// NOTE: no callers (debug display)
void DisplayCopSightInfo(void)
{
	char text[256];

	if (gbCanSeePlayerInternal)
	{
		sprintf(text, "Cops can see player - Internal 360 radius!");
		WriteDropShadowedString(16, 320, text, 0, 0, 255, 255, 255);
	}

	if (gbCanSeePlayerExternal)
	{
		sprintf(text, "Cops can see player - View frustrum!");
		WriteDropShadowedString(16, 336, text, 0, 0, 255, 255, 255);
	}

	gbCanSeePlayerInternal = 0;
	gbCanSeePlayerExternal = 0;
}

// [0x4d42ca] [D]
// Out of a pursuit the rating falls by 1 every FelonyDecreaseTime frames; in one, while the cops
// see the player, it rises by 1 every FelonyIncreaseTime frames, calls a roadblock and counts the
// felony reoccurrence delays down.
void AdjustFelony(FELONY_DATA *pFelonyData)
{
	int dir;
	int loop;

	if (GamePaused)
		return;

	if (FelonyRating != 0 && FelonyRating < 682)
	{
		if (FelonyDecreaseTimer == FelonyDecreaseTime)
		{
			FelonyRating--;
			FelonyDecreaseTimer = 0;
		}
		else
		{
			FelonyDecreaseTimer++;
		}
	}
	else if (CopsCanSeePlayer && FelonyRating >= 682)
	{
		if (TimeSinceLastSpeech > 240 && car_data[0].speed > 20)
		{
			dir = (unsigned char)GetCarDirectionOfTravel(car_data);

			// NB: looks like frisco is fucked up
			if (GameLevel == 0)
				dir++;
			else if (GameLevel == 1)
				dir += 2;

			if (dir != LastHeading)
				PlaySpeechSeq(0x8004, slu[dir & 3], -1, -1);

			LastHeading = dir;
		}

		if (FelonyIncreaseTimer == FelonyIncreaseTime)
		{
			if (++FelonyRating > 4095)
				FelonyRating = 4095;

			FelonyIncreaseTimer = 0;
		}
		else
		{
			FelonyIncreaseTimer++;
		}

		if (FelonyRating >= gCopData.roadblockTrigger)
			RequestRoadBlock(GetCarDirectionOfTravel(car_data));

		for (loop = 0; loop < 11; loop++)
		{
			if (pFelonyData->reoccurrenceDelay[loop].current)
				pFelonyData->reoccurrenceDelay[loop].current--;
		}
	}
}

// [0x4d4483] [D]
void CheckPlayerMiscFelonies(FELONY_DATA *pFelonyData)
{
	unsigned short rj;

	// NOTE: the heading is not used (unassigned in the original as well)
	GetCarHeading(car_data[0].direction);

	rj = ROADS_GetRoadOrJunction(car_data[0].location.vx, car_data[0].location.vz);

	if (rj == 0xFFFF)
	{
		PlayerLastRoad = -1;
	}
	else if (rj < 0x4000 && rj != PlayerLastRoad)
	{
		// Run a red light?
		if (CheckRedLights(rj))
			NoteFelony(pFelonyData, 3, 4096);

		PlayerLastRoad = rj;
	}

	// if lights are off (broken)
	if (gNight && car_data[0].damage[0] > 1000 && car_data[0].damage[1] > 1000)
		NoteFelony(pFelonyData, 9, 4096);

	// reckless driving.
	if (car_data[0].handbrake || car_data[0].wheelspin)
		NoteFelony(pFelonyData, 10, 4096);

	// speeding
	if (car_data[0].speed > 200)
		NoteFelony(pFelonyData, 2, 4096);
}

// [0x4d45c1] [D]
void NoteFelony(FELONY_DATA *pFelonyData, char type, int scale)
{
	unsigned char t;
	int additionalFelonyPoints;

	t = type;

	if (gCheatFlags.Immunity == 0 && gUseCopModel && pFelonyData->reoccurrenceDelay[t].current == 0 &&
		CopsCanSeePlayer)
	{
		LastFelony = t;

		pFelonyData->reoccurrenceDelay[t].current = pFelonyData->reoccurrenceDelay[t].maximum;

		if (CopSightData.surroundViewDistance == 8000)
			additionalFelonyPoints = pFelonyData->value[t].angry;
		else
			additionalFelonyPoints = pFelonyData->value[t].placid;

		IncreaseFelonyRating((scale * additionalFelonyPoints) >> 12);

		if (type == 4)
			PlayCopSpeech(0x8009);

		if (type == 3)
			PlayCopSpeech(0x800B);
	}
}

// [0x4d46be] [D]
void IncreaseFelonyRating(int howMuch)
{
	FelonyRating += howMuch;

	if (FelonyRating > 4095)
		FelonyRating = 4095;
}

// [0x4d46f6] [D]
// In a pursuit the cops see twice as far and wide.
void ManageCopSightData(COP_SIGHT_DATA *copSightData)
{
	if (FelonyRating < 682)
	{
		copSightData->surroundViewDistance = 4000;
		copSightData->frontViewDistance = 12500;
		copSightData->frontViewAngle = 512;
	}
	else
	{
		copSightData->surroundViewDistance = 8000;
		copSightData->frontViewDistance = 25000;
		copSightData->frontViewAngle = 1024;
	}
}

// [0x4d473d] [D]
// The player is inside the surround distance, or inside the front view distance and angle.
// Distances in 1/16 units so the squares fit in an int.
int CopCanSeePlayer(COP_SIGHT_DATA *copSightData, COP *cop)
{
	int frontDist;
	int dx;
	int dz;
	int distSqr;

	frontDist = copSightData->frontViewDistance >> 4;

	if (GameMode == 346 || GameMode == 347 || GameMode == 446 || GameMode == 447)
		return CopCanSeePlayer_SpecialGameModeOnly(copSightData, cop);

	dx = (car_data[0].where.t[0] - cop->pCarData->where.t[0]) >> 4;
	dz = (car_data[0].where.t[2] - cop->pCarData->where.t[2]) >> 4;
	distSqr = dz * dz + dx * dx;

	if (distSqr < (copSightData->surroundViewDistance >> 4) * (copSightData->surroundViewDistance >> 4))
	{
		gbCanSeePlayerInternal = 1;
		return 1;
	}

	if (abs(DIFF_ANGLES(Arc_Tan(dx, dz), cop->direction)) >= copSightData->frontViewAngle / 2 ||
		distSqr >= frontDist * frontDist)
	{
		return 0;
	}

	gbCanSeePlayerExternal = 1;
	return 1;
}

// [0x4d4875] [D]
// CopCanSeePlayer for game modes 346, 347, 446, 447: full distances, and the angle taken from
// Arc_Tan(dz, dx) + 1024.
int CopCanSeePlayer_SpecialGameModeOnly(COP_SIGHT_DATA *copSightData, COP *cop)
{
	int dx;
	int dz;
	int distSqr;

	dx = car_data[0].location.vx - cop->pCarData->where.t[0];
	dz = car_data[0].location.vz - cop->pCarData->where.t[2];
	distSqr = dz * dz + dx * dx;

	if (distSqr < copSightData->surroundViewDistance * copSightData->surroundViewDistance)
	{
		gbCanSeePlayerInternal = 1;
		return 1;
	}

	if (abs(DIFF_ANGLES(Arc_Tan(dz, dx) + 1024, cop->direction)) >= copSightData->frontViewAngle / 2 ||
		distSqr >= copSightData->frontViewDistance * copSightData->frontViewDistance)
	{
		return 0;
	}

	gbCanSeePlayerExternal = 1;
	return 1;
}

// [0x4d4965] [D]
void PlayCopSpeech(int id)
{
	if (gAllowCopSpeech)
		PlaySpeech(id);
}

// [0x4d497f] [D]
void PlaySpeechSeq(int id1, int id2, int id3, int id4)
{
	if (gAllowCopSpeech)
		PlaySpeechNow(id1, id2, id3, id4);
}
