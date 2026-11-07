/*
 * gsound.c - the game's sounds: engines, sirens, speech queue, alarms, thunder, passenger chatter
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "gsound.c").
 *
 * StartGameSounds locks the fixed channels: 0 the player's engine (idle), 1 its engine under load,
 * 2 the siren or horn, 4 skids, 5 / 6 the engines of the two closest pursuing cops, 7 / 8 their
 * sirens, 10 a car alarm, 11 rain, 15 speech. SoundTasks updates them every frame; the engine
 * pitch follows the revs of a simple four gear box (geard, GetEngineRevs).
 */

#include <stdlib.h>

#include "dr_types.h"
#include "gsound.h"
#include "dr_globals.h"
#include "asmmath.h"
#include "general.h"
#include "interview.h"
#include "shell.h"
#include "sound.h"
#include "felony.h"
#include "handling.h"
#include "MScript.h"
#include "effects.h"
#include "taxisnd.h"
#include "pause.h"
#include "cops.h"
#include "Engine/3dfx/polydraw.h"
#include "ai.h"
#include "glaunch.h"

void StopGameSounds(void);
void PlaySpeech(int id);
void ResetSoundQueue(void);
void ControlSoundQueue(void);
void ControlCopEngineRevs(void);
void UpdateCopEngineRevs(void);
void ControlEngineRevs(int id);
void UpdateEngineRevs(int id, int chan1, int chan2);
void ControlLightningTimer(void);
void ControlCarAlarms(void);
int GetEngineRevs(int id);
void InitAliSounds(void);
void HandleAliSounds(int pad);
void InitPresidentSounds(void);
void HandlePresidentSounds(int pad);
void InitLoseTailSounds(void);
void HandleLoseTailSounds(void);

//------------------------------------------------------------------------------------------
// initialised data (0x562630)

// wheel speed (/ 2048) to change down when idle, to change down, to change up; the revs per
// wheel speed accelerating and idle
GEAR_DESC geard[4] =
{
	{ 0, 0, 245, 96, 90 },
	{ 130, 200, 390, 60, 57 },
	{ 130, 350, 540, 40, 38 },
	{ 130, 490, 9999, 32, 30 },
};

int CarAlarmFreq[4] = { 8000, 10000, 11025, 14000 };
int SndThunderTimer = -1;
int SndCop1 = -1;
int SndCop2 = -1;

// the passengers' speech: sample, frame last played
int ali_sounds[4][2] = { { 4, 0 }, { 6, 0 }, { 7, 0 }, { 8, 0 } };
int pres_sounds[2][2] = { { 1, 0 }, { 2, 0 } };

int CarNumAlarm = -1;

//------------------------------------------------------------------------------------------
// uninitialised data

int tail_sounds[4][2];
// NOTE: IDA types it int[4]; StartGameSounds and ResetSoundQueue clear five (the unnamed word
// up to SpeechQueuePos), the last one ends the queue
int SpeechQueue[5];
int SpeechQueuePos;
int ali_sound_on;
int tail_sound_on;
int current_sound_delay;
int president_time;
int last_damage;
int covered_last;
int TimeSinceLastSpeech;
int gEnableLoseTailSounds;
int gNumLoseTailSounds;

// [0x4d78b0] [D]
// Starts the looping sounds of the game on their channels, silent until SoundTasks turns them up.
void StartGameSounds(void)
{
	int car;
	int i;

	car = 0;

	StopGameSounds();

	SpeechQueuePos = 0;

	for (i = 0; i < 5; i++)
		SpeechQueue[i] = -1;

	TimeSinceLastSpeech = 0;

	SndCop2 = -1;
	SndCop1 = -1;

	horn_volume = -10000;
	gPlayerSirenOn = 0;
	gPreviousHornState = 0;

	if (gMissionRecording)
		car = 65;

	LockSoundChannel(0);
	Play3DSound(0, 0, (float)car_data[car].location.vx, (float)-car_data[car].location.vy, (float)car_data[car].location.vz);
	SetSoundVol(0, -10000);

	LockSoundChannel(1);
	Play3DSound(34, 1, (float)car_data[car].location.vx, (float)-car_data[car].location.vy, (float)car_data[car].location.vz);
	SetSoundVol(1, -10000);

	LockSoundChannel(5);
	Play3DSound(0, 5, 0.0f, 0.0f, -20000.0f);
	SetSoundVol(5, -10000);

	LockSoundChannel(6);
	Play3DSound(0, 6, 0.0f, 0.0f, -20000.0f);
	SetSoundVol(6, -10000);

	LockSoundChannel(4);
	Play3DSound(6, 4, 0.0f, 0.0f, 0.0f);
	SetSoundVol(4, -10000);
	SetSoundFreq(4, -1);

	if (!gDoInterview && !gDoTraining)
	{
		LockSoundChannel(7);
		SimplePlaySound(7, 7, -1);
		SetSoundVol(7, -10000);

		LockSoundChannel(8);
		SimplePlaySound(33, 8, -1);
		SetSoundVol(8, -10000);
	}

	if (gDoRain)
	{
		LockSoundChannel(11);
		SimplePlaySound(21, 11, -1);
	}

	// the cop car (model 10) has a siren, the others a horn
	if (gPlayerCarModelType == 10)
		Play3DSound(7, 2, 0.0f, 0.0f, 0.0f);
	else
		Play3DSound(10, 2, 0.0f, 0.0f, 0.0f);

	SetSoundVol(2, -10000);
	SetTrackingSound(2, &car_data[0].location);

	LockSoundChannel(2);
	LockSoundChannel(3);
	LockSoundChannel(15);

	InitAliSounds();
	InitPresidentSounds();
	InitTaxiSounds();
	InitLoseTailSounds();

	covered_last = 0;

	UpdateSound();
	SetSoundVol(0, 0);
}

// [0x4d7bcc] [D]
void StopGameSounds(void)
{
	StopSounds();
	UnlockAllSoundChannels();
}

// [0x4d7bdb] [D]
void ReleaseGameSounds(void)
{
	StopGameSounds();
	ReleaseSounds();

	gNumLoseTailSounds = 0;
	gEnableLoseTailSounds = 0;
}

// [0x4d7bfe] [D]
// Every frame: the reverb under cover, the engines of the player (the ghost car 65 in the
// training replay) and the cops, the speech queue, alarms and thunder.
void SoundTasks(void)
{
	int covered;

	covered = CheckForUnderCover(camera_position.vx, camera_position.vy, camera_position.vz);

	if (covered)
	{
		if (!covered_last)
			SetSoundSystemEnvironment(6);
	}
	else if (covered_last)
	{
		SetSoundSystemEnvironment(1);
	}

	covered_last = covered;

	if (gMissionRecording || gMissionReplaying)
		ControlEngineRevs(65);

	if (!gDoTraining || gTrainingMode != 0)
	{
		ControlEngineRevs(0);
		UpdateEngineRevs(0, 0, 1);
	}
	else
	{
		UpdateEngineRevs(65, 0, 1);
	}

	if (!gMissionRecording || gMissionReplaying)
		ControlCopEngineRevs();

	ControlSoundQueue();
	ControlCarAlarms();
	ControlLightningTimer();
}

// [0x4d7ceb] [D]
// Plays a speech sample unless one is playing.
void PlaySpeech(int id)
{
	if (!ChannelInUse(15))
	{
		SimplePlaySound(id, 15, -1);
		TimeSinceLastSpeech = 0;
	}
}

// [0x4d7d1a] [D]
// Queues up to four speech samples (-1 none) and starts the first, unless speech is playing.
void PlaySpeechNow(int id1, int id2, int id3, int id4)
{
	if (!ChannelInUse(15))
	{
		SpeechQueue[0] = id1;
		SpeechQueue[1] = id2;
		SpeechQueue[2] = id3;
		SpeechQueue[3] = id4;
		SpeechQueuePos = 0;

		ControlSoundQueue();
	}
}

// [0x4d7d60] [D]
void AddToSpeechQueue(int id)
{
	int i;

	for (i = SpeechQueuePos; i < 4; i++)
	{
		if (SpeechQueue[i] == -1)
		{
			SpeechQueue[i] = id;
			return;
		}
	}
}

// [0x4d7d9f] [D]
void ResetSoundQueue(void)
{
	int i;

	for (i = 0; i < 5; i++)
		SpeechQueue[i] = -1;

	SpeechQueuePos = 0;
}

// [0x4d7dd9] [D]
int IsSpeechPlaying(void)
{
	return ChannelInUse(15) || SpeechQueue[SpeechQueuePos] != -1;
}

// [0x4d7e0b] [D]
// The passenger's chatter of the mission (gBitchingMode 1 Ali, 2 the president, 3 the taxi
// fare) and the "lose the tail" speech.
void ControlBitchingSounds(int pad)
{
	if (GamePaused || gStopPadReads)
		return;

	switch (gBitchingMode)
	{
		case 1:
			HandleAliSounds(pad);
			break;
		case 2:
			HandlePresidentSounds(pad);
			break;
		case 3:
			HandleTaxiSounds(pad);
			break;
	}

	HandleLoseTailSounds();
}

// [0x4d7e6e] [D]
// Thunder after the lightning: delay frames from now.
int TriggerThunder(int delay)
{
	SndThunderTimer = delay + 15;
	return delay + 15;
}

// [0x4d7e7e] [D] [A]
// Starts the alarm of car id (not when wrecked).
// NOTE: [A] y is negated: the original puts the sound below the ground by the car's height
void TriggerAlarm(int id)
{
	if (civilian_ai_data[id - 1].wrecked != 1)
	{
		Play3DSound(11, 10, (float)car_data[id].location.vx, (float)-car_data[id].location.vy, (float)car_data[id].location.vz);
		SetSoundVol(10, 0x7FFF);
		SetSoundFreq(10, CarAlarmFreq[game_frame & 3]);

		CarNumAlarm = id;
	}
}

// [0x4d7f24] [D]
// Plays the next queued speech sample once the last one is done.
void ControlSoundQueue(void)
{
	int id;

	if (ChannelInUse(15))
		return;

	if (SpeechQueue[SpeechQueuePos] == -1)
	{
		ResetSoundQueue();
		TimeSinceLastSpeech++;
	}
	else
	{
		id = SpeechQueue[SpeechQueuePos++];
		PlaySpeech(id);
	}
}

// [0x4d7f8c] [D]
// The sirens (pitch by health) and engines of the two closest pursuing cops.
// NOTE: like the original, does not check that SndCop1 / SndCop2 are cop cars (IsCopCar)
void ControlCopEngineRevs(void)
{
	COP *cd;
	int cop1_on;
	int cop2_on;

	cop1_on = 0;
	cop2_on = 0;

	if ((game_frame & 0x3F) == 0)
	{
		UpdateCopEngineRevs();

		if (SndCop1 != -1)
		{
			cd = &cop_data[(unsigned char)civilian_ai_data[SndCop1 - 1].IsCopCar - 1];

			if (cd->health > 0.01f)
			{
				SetTrackingSound(7, &car_data[SndCop1].location);
				SetSoundVol(7, -100);
				SetSoundFreq(7, (int)(cd->health * 11025.0));
				cop1_on = 1;
			}
		}

		if (SndCop2 != -1)
		{
			cd = &cop_data[(unsigned char)civilian_ai_data[SndCop2 - 1].IsCopCar - 1];

			if (cd->health > 0.01f)
			{
				SetTrackingSound(8, &car_data[SndCop2].location);
				SetSoundVol(8, -100);
				SetSoundFreq(8, (int)(cd->health * 11025.0));
				cop2_on = 1;
			}
		}

		if (!cop1_on)
		{
			SetTrackingSound(7, NULL);
			SetSoundVol(7, -10000);
		}

		if (!cop2_on)
		{
			SetTrackingSound(8, NULL);
			SetSoundVol(8, -10000);
		}
	}

	if (SndCop1 == -1)
	{
		SetSoundVol(5, -10000);
	}
	else
	{
		ControlEngineRevs(SndCop1);
		UpdateEngineRevs(SndCop1, 5, -1);
		SetSoundVol(5, 0);
	}

	if (SndCop2 == -1)
	{
		SetSoundVol(6, -10000);
	}
	else
	{
		ControlEngineRevs(SndCop2);
		UpdateEngineRevs(SndCop2, 6, -1);
		SetSoundVol(6, 0);
	}
}

// [0x4d81ca] [D]
// Picks the two pursuing cars (not model 2) closest to the camera, in cells of 1500.
void UpdateCopEngineRevs(void)
{
	CIV_STATE *ai;
	int closest;
	int furthest;
	int dist;
	int x;
	int z;
	int i;

	ai = civilian_ai_data;

	closest = 10000;
	furthest = 10000;

	SndCop2 = -1;
	SndCop1 = -1;

	x = float_to_int(camera_position.vx) / 1500;
	z = float_to_int(camera_position.vz) / 1500;

	for (i = 0; i < 64; i++)
	{
		if (ai->ctrlState == CIVSTATE_PURSUIT && ai->model != 2)
		{
			dist = abs(ai->currentX - x);
			dist += abs(ai->currentZ - z);

			if (dist < closest)
			{
				furthest = closest;
				SndCop2 = SndCop1;
				SndCop1 = i + 1;
				closest = dist;
			}
			else if (dist < furthest)
			{
				SndCop2 = i + 1;
				furthest = dist;
			}
		}

		ai++;
	}
}

// [0x4d8310] [D]
// Moves a car's revs towards those of its gear (wheelspin: 20000), down 720 or up 800 a frame
// (dropping fast is a gear change, no thrust), and the volume of the engine under load (revsvol
// 0 loud .. 10000 silent).
void ControlEngineRevs(int id)
{
	CAR_DATA *cp;
	unsigned short oldRevs;
	unsigned short newRevs;
	unsigned short desiredRevs;
	int thrust;
	int minvol;

	cp = &car_data[id];

	thrust = cp->thrust;
	oldRevs = cp->revs;

	cp->changingGear = 0;

	if (cp->wheelspin)
	{
		desiredRevs = 20000;
		cp->gear = 0;

		if (oldRevs < 8000)
			oldRevs = 8000;
	}
	else
	{
		desiredRevs = GetEngineRevs(id);
	}

	newRevs = desiredRevs;

	if (oldRevs - desiredRevs > 720)
	{
		newRevs = oldRevs - 720;
		thrust = 0;
		cp->changingGear = 1;
	}
	else if (desiredRevs - oldRevs > 800)
	{
		newRevs = oldRevs + 800;
	}

	cp->revs = newRevs;

	if (thrust || cp->speed > 30)
	{
		minvol = 0;

		if (!thrust)
			minvol = 2000;

		cp->revsvol -= 2000;

		if (cp->revsvol < minvol)
			cp->revsvol = minvol;
	}
	else
	{
		cp->revsvol += 100;

		if (cp->revsvol > 10000)
			cp->revsvol = 10000;
	}
}

// [0x4d84aa] [D]
// Places a car's engine sounds: chan1 (idle) pitched by the revs, chan2 (load, -1 none) at the
// load volume.
void UpdateEngineRevs(int id, int chan1, int chan2)
{
	CAR_DATA *cp;
	int pitch;

	cp = &car_data[id];

	pitch = (car_data[id].revs >> 2) + 1000 - (cp->revsvol >> 7);

	SetSoundPos(chan1, (float)cp->location.vx, (float)-cp->location.vy, (float)cp->location.vz);
	SetSoundFreq(chan1, 22050 * pitch / 4096);

	if (chan2 != -1)
	{
		SetSoundPos(chan2, (float)cp->location.vx, (float)-cp->location.vy, (float)cp->location.vz);
		SetSoundVol(chan2, cp->revsvol - 10000);
	}
}

// [0x4d85bd] [D]
void ControlLightningTimer(void)
{
	if (SndThunderTimer == -1)
		return;

	if (SndThunderTimer == 0)
		SimplePlaySound(22, -1, (short)((rand() & 0x1FFF) - 4096 + 16000));

	SndThunderTimer--;
}

// [0x4d8617] [D]
// Moves the alarm with its car.
// NOTE: as in the original the alarm stops when the car is not wrecked, so (TriggerAlarm only
// starts it for cars that are not) it is stopped the frame after it starts
void ControlCarAlarms(void)
{
	if (CarNumAlarm == -1)
		return;

	SetSoundPos(10, (float)car_data[CarNumAlarm].location.vx, (float)-car_data[CarNumAlarm].location.vy, (float)car_data[CarNumAlarm].location.vz);

	if (civilian_ai_data[CarNumAlarm - 1].wrecked != 1)
	{
		StopSound(10);
		CarNumAlarm = -1;
	}
}

// [0x4d86b7] [D]
// The revs of a car's engine: picks the gear for the wheel speed (reverse in gear 0).
int GetEngineRevs(int id)
{
	int gear;
	int lastgear;
	int acc;
	int ws;
	int lws;
	int ratio;

	gear = (unsigned char)car_data[id].gear;
	acc = car_data[id].thrust;
	ws = car_data[id].wheel_speed;

	if (ws <= 0)
	{
		ws = -ws / 2048;
		gear = 0;
	}
	else
	{
		ws = car_data[id].wheel_speed / 2048;

		if ((unsigned char)car_data[id].gear > 3)
			gear = 3;

		do
		{
			lastgear = gear;

			if (acc <= 0)
				lws = geard[gear].lowidl_ws;
			else
				lws = geard[gear].low_ws;

			if (lws > ws)
				gear--;

			if (geard[gear].hi_ws < ws)
				gear++;
		} while (lastgear != gear);
	}

	car_data[id].gear = gear;

	if (acc)
		ratio = geard[gear].ratio_ac;
	else
		ratio = geard[gear].ratio_id;

	return ws * ratio;
}

// [0x4d87f0] [D]
void InitAliSounds(void)
{
	int i;

	for (i = 0; i < 4; i++)
		ali_sounds[i][1] = 0;

	ali_sound_on = 0;
}

// [0x4d882a] [D]
// Ali's chatter: one of four samples now and then, each at most every 2250 frames.
void HandleAliSounds(int pad)
{
	char r;
	int played;
	int i;

	played = 0;

	if (!ali_sound_on)
	{
		r = Random();
		ali_sound_on = 1;
		current_sound_delay = 50 * (r & 7) + (r & 0xF) + 500;
		return;
	}

	if (--current_sound_delay > 0)
		return;

	i = game_frame & 3;

	if (game_frame - ali_sounds[i][1] > 2250)
	{
		SimplePlaySound(ali_sounds[i][0] + 0xF000, 15, -1);
		played = 1;
		ali_sounds[i][1] = game_frame;
	}

	r = Random();

	if (played)
		current_sound_delay = 50 * (r & 0xF) + (r & 0x1F) + 500;
	else
		current_sound_delay = (r & 0xF) + 50 * (r & 7);
}

// [0x4d8938] [D]
void InitPresidentSounds(void)
{
	int i;

	for (i = 0; i < 2; i++)
		pres_sounds[i][1] = 0;

	last_damage = 0;
	president_time = 0;
}

// [0x4d897c] [D]
// The president complains when the car is damaged, and after 1000 frames of driving without.
void HandlePresidentSounds(int pad)
{
	char r;
	int damage;
	int i;

	damage = GetTotalDamage(car_data) >> 3;

	if (damage == last_damage)
	{
		if (car_data[0].speed > 30 && ++president_time == 1000)
		{
			PlaySpeech(0xF000);
			president_time = 0;
		}
	}
	else
	{
		president_time = 0;

		i = (game_frame & 4) >> 2;

		if (game_frame - pres_sounds[i][1] > current_sound_delay)
		{
			PlaySpeech(pres_sounds[i][0] + 0xF000);
			pres_sounds[i][1] = game_frame;
		}

		r = Random();
		current_sound_delay = 50 * (r & 3) + (r & 0x1F) + 350;
	}

	last_damage = damage;
}

// [0x4d8a6f] [D]
void InitLoseTailSounds(void)
{
	int i;

	for (i = 0; i < gNumLoseTailSounds; i++)
	{
		tail_sounds[i][0] = gLoseTailSounds[i];
		tail_sounds[i][1] = 0;
	}

	tail_sound_on = 0;
}

// [0x4d8ac2] [D]
// "Lose the tail" speech of the mission while the cops know where the player is.
void HandleLoseTailSounds(void)
{
	char r;
	int i;

	if (!gEnableLoseTailSounds)
		return;

	if (FelonyRating < 682 || player_position_known <= 0 || gNumLoseTailSounds <= 0)
	{
		tail_sound_on = 0;
	}
	else if (tail_sound_on)
	{
		if (--current_sound_delay <= 0)
		{
			i = game_frame % gNumLoseTailSounds;

			if (game_frame - tail_sounds[i][1] > current_sound_delay)
			{
				PlaySpeech(tail_sounds[i][0] + 0xF000);
				tail_sounds[i][1] = game_frame;
			}

			r = Random();
			current_sound_delay = 50 * (r & 7) + (r & 0x1F) + 750;
		}
	}
	else
	{
		r = Random();
		tail_sound_on = 1;
		current_sound_delay = 50 * (r & 3) + (r & 7) + 200;
	}
}
