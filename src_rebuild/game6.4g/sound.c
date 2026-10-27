/*
 * sound.c - sound system front end (channels, samples, volume) over the DirectSound back end
 *
 * Rebuilt from Driver_sym2_94.i64 (IDA folder "sound.c"); partially translated.
 */

#include <windows.h>
#include <string.h>
#include <math.h>

#include "dr_types.h"
#include "dr_globals.h"
#include "sound.h"
#include "debug.h"
#include "eax/directsound.h"

typedef struct CHANNEL_SAMPLE_DATA
{
	VECTOR srcposition;
	VECTOR position;
	IVECTOR *trackingPosition;
	int looping;
	int srcpitch;
	int pitch;
	short srcvolume;
	short volumeScale;
	short volume;
	char flags;
	char priority;
} CHANNEL_SAMPLE_DATA;

typedef struct CHANNEL_DATA
{
	char flags;
	char locked;
	short sample;
	CHANNEL_SAMPLE_DATA info;
} CHANNEL_DATA;

typedef struct SAMPLE_DATA
{
	int frequency;
	short volume;
	char flags;
	char priority;
} SAMPLE_DATA;

enum EChannelFlags
{
	CHAN_ACTIVE = 0x1,
	CHAN_POSITION = 0x2,
	CHAN_PITCH = 0x4,
	CHAN_VOLUME = 0x8,
	CHAN_QUEUED = 0x10,
	CHAN_TRACKING_POS = 0x20,
};

#define MAX_CHANNELS		20
#define MAX_SAMPLES			80
#define MAX_SOUND_DEVICES	8

// volume_table[] maps 0..10000 to DirectSound attenuation in hundredths of a dB
#define VOLUME_STEPS		10000
#define VOLUME_MIN			-10000	// NOTE: known value (DSBVOLUME_MIN)
#define VOLUME_MAX			0		// NOTE: known value (DSBVOLUME_MAX)

void ShutdownSoundSystem(void);
void StopSounds(void);
void ReleaseSounds(void);
void SetSoundVol(int channel, int volume);

//------------------------------------------------------------------------------------------
// initialised data (0x5787dc)

float sw_volume = 1.0f;

//------------------------------------------------------------------------------------------
// uninitialised data (0x5c89e0, 0xc07238, 0xc10e7c)
// NOTE: all unmarked; defined here provisionally as sound.c is their only user

CHANNEL_DATA Channels[MAX_CHANNELS];
static int unknown_5C8DF0[4];		// NOTE: 16 bytes, no references
SAMPLE_DATA Samples[MAX_SAMPLES];
SOUND_CALLBACKS SoundCalls;
DSOUND_CARD_INFO SoundDevices[MAX_SOUND_DEVICES];

int volume_table[VOLUME_STEPS + 1];	// NOTE: IDA type says int[8113], the item size is 10001 ints

int SoundIDFixups[16];				// NOTE: IDA type says int[17], InitSoundSystem clears 64 bytes
float Listener[2][3];				// NOTE: IDA type says float[3][3], the item size is 24 bytes
int SoundEnabled;
int SoundPaused;
int FadeSoundCount;
float FadeSoundCurrent;
float FadeSoundTarget;
float FadeSoundStep;
int num_soundcards;
int EnableEAX;
int NumSamples;

// [0x525870] [D]
void InitSoundDevices(void)
{
	if (!SoundEnabled && !num_soundcards)
		EnumerateSoundCards();
}

// [0x52588e] [D]
int InitSoundSystem(int device, int mode, int useEAX)
{
	int i;
	int inited;

	if (SoundEnabled)
		ShutdownSoundSystem();

	if (device < 0 || device >= num_soundcards)
		return 0;

	sw_volume = 1.0f;
	EnableEAX = useEAX;

	FadeSoundCount = 0;
	FadeSoundCurrent = 0.0f;
	FadeSoundTarget = 0.0f;
	FadeSoundStep = 0.0f;

	memset(Samples, 0, sizeof(Samples));
	memset(Channels, 0, sizeof(Channels));
	memset(SoundIDFixups, 0, sizeof(SoundIDFixups));

	Listener[0][0] = 0.0f;
	Listener[0][1] = 0.0f;
	Listener[0][2] = 0.0f;
	Listener[1][0] = 0.0f;
	Listener[1][1] = 0.0f;
	Listener[1][2] = 0.0f;

	volume_table[0] = VOLUME_MIN;

	for (i = 1; i < VOLUME_STEPS; i++)
	{
		volume_table[i] = (int)(log10((double)i / 10000.0) * 2000.0);

		if (volume_table[i] < VOLUME_MIN)
			volume_table[i] = VOLUME_MIN;

		if (volume_table[i] > VOLUME_MAX)
			volume_table[i] = VOLUME_MAX;
	}

	volume_table[VOLUME_STEPS] = VOLUME_MAX;

	if (SoundDevices[device].field_00)
		inited = 0;
	else
		inited = InitDirectSoundSystem(device, mode, gScreen.handle);

	if (inited)
	{
		SoundCalls.SetWorldStats(0.005f, 1000.0f, 20000.0f, 1.0f, 1.0f);
		SoundEnabled = 1;
		return 1;
	}

	SoundEnabled = 0;
	LogQuick("=> Error allocating sound hardware <=");
	return 0;
}

// [0x525a9a] [D]
void ShutdownSoundSystem(void)
{
	if (SoundEnabled)
	{
		StopSounds();
		ReleaseSounds();

		SoundCalls.CloseAudio();

		NumSamples = 0;
		EnableEAX = 0;
		SoundEnabled = 0;
	}
}

// [0x525ad8] [D]
void SetSoundSystemFormat(int bits, int freq)
{
	if (SoundEnabled)
		SoundCalls.SetSoundFormat(bits, freq);
}

// [0x525af7] [D]
void SetSoundSystemMode(int mode)
{
	if (SoundEnabled)
		SoundCalls.SetSpeakerMode(mode);
}

// [0x525c16] [D]
void SetEffectsVolume(float volume)
{
	int i;

	if (SoundEnabled)
	{
		sw_volume = volume;

		if (volume < 0.0f)
			sw_volume = 0.0f;

		if (sw_volume > 1.0f)
			sw_volume = 1.0f;

		for (i = 0; i < MAX_CHANNELS; i++)
		{
			if (Channels[i].flags & (CHAN_ACTIVE | CHAN_QUEUED))
				SetSoundVol(i, Channels[i].info.volume);
		}
	}
}
