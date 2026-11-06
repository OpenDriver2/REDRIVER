/*
 * sound.c - sound system front end (channels, samples, volume) over the OpenAL ([A]) back end
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "sound.c", 0x525870 - 0x52711f)
 */

#include <string.h>
#include <math.h>
#include <stdio.h>

#include "dr_types.h"
#include "dr_globals.h"
#include "shell.h"
#include "sound.h"
#include "debug.h"
#include "eax/alsound.h"
#include "Engine/3dfx/polydraw.h"
#include "camera.h"

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

enum ESampleFlags
{
	SAMP_LOOP = 0x1,
	SAMP_2D = 0x2,
	SAMP_INVALID = 0x4,
};

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
void ReleaseSample(int sample);
void StopSound(int channel);
void SetSoundFreq(int channel, int frequency);
void StartSound(int channel, int sample, float x, float y, float z);
void LSProcessLine(char *buffer);
int LSReadInt(char *buffer);
int LSReadLine(FILE *f, char *buffer);

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

// [0x525870] [D] [A]
// NOTE: [A] OpenAL
void InitSoundDevices(void)
{
	if (!SoundEnabled && !num_soundcards)
		OpenAL_EnumerateSoundCards();
}

// [0x52588e] [D] [A]
// NOTE: [A] OpenAL
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
		inited = InitOpenALSystem(device);

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

// [0x525b12] [D]
void SetSoundSystemEnvironment(int environment)
{
	if (SoundEnabled)
		SoundCalls.SetEnvironment(environment);
}

// [0x525b2d] [D]
void ReleaseSounds(void)
{
	int i;

	if (SoundEnabled == 0)
		return;

	StopSounds();

	for (i = 0; i < NumSamples; i++)
		ReleaseSample(i);

	for (i = 0; i < 16; i++)
		SoundIDFixups[i] = 0;

	NumSamples = 0;
}

// [0x525ba0] [D]
// Switching Doppler off stops the listener and the channels moving.
void SetSoundSystemDoppler(int enabled)
{
	int i;

	if (SoundEnabled == 0 || enabled)
		return;

	SoundCalls.StartFrame();
	SoundCalls.SetListenerVelocity(0.0f, 0.0f, 0.0f);

	for (i = 0; i < MAX_CHANNELS; i++)
	{
		if (Channels[i].flags & CHAN_ACTIVE)
			SoundCalls.SetChannelVelocity(i, 0.0f, 0.0f, 0.0f);
	}

	SoundCalls.EndFrame();
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

// [0x525cb6] [D]
void StopSounds(void)
{
	int i;

	if (SoundEnabled == 0)
		return;

	for (i = 0; i < MAX_CHANNELS; i++)
		StopSound(i);

	SoundPaused = 0;
	SetEffectsVolume(gMasterVolume);

	FadeSoundCount = 0;
	FadeSoundCurrent = 0.0f;
	FadeSoundTarget = 0.0f;
	FadeSoundStep = 0.0f;
}

// [0x525d2e] [D]
// Once per frame: fades the volume while paused, moves the listener to the camera, sends the
// changed channel settings and starts the queued channels; a channel that stopped playing is
// freed.
void UpdateSound(void)
{
	CHANNEL_SAMPLE_DATA *info;
	int status;
	int i;

	if (SoundEnabled == 0 || gAllowSounds == 0)
		return;

	if (SoundPaused)
	{
		if (FadeSoundCount == 0)
			return;

		if (--FadeSoundCount)
		{
			if (FadeSoundTarget >= FadeSoundCurrent)
				FadeSoundCurrent = FadeSoundCurrent + FadeSoundStep;
			else
				FadeSoundCurrent = FadeSoundCurrent - FadeSoundStep;
		}
		else
		{
			FadeSoundCurrent = FadeSoundTarget;
		}

		SetEffectsVolume(FadeSoundCurrent);
	}

	Listener[1][0] = Listener[0][0];
	Listener[1][1] = Listener[0][1];
	Listener[1][2] = Listener[0][2];

	Listener[0][0] = camera_position.vx;
	Listener[0][1] = camera_position.vy;
	Listener[0][2] = camera_position.vz;

	// front: the camera's z axis, top: its y axis
	SoundCalls.SetListenerOrientation(inv_camera_matrix.m[0][2], inv_camera_matrix.m[1][2], inv_camera_matrix.m[2][2],
									  inv_camera_matrix.m[0][1], inv_camera_matrix.m[1][1], inv_camera_matrix.m[2][1]);

	SoundCalls.SetListenerPosition(Listener[0][0], Listener[0][1], Listener[0][2]);

	if (gUseDoppler)
		SoundCalls.SetListenerVelocity(Listener[0][0] - Listener[1][0], Listener[0][1] - Listener[1][1], Listener[0][2] - Listener[1][2]);

	for (i = 0; i < MAX_CHANNELS; i++)
	{
		info = &Channels[i].info;

		if (Channels[i].flags & CHAN_ACTIVE)
		{
			status = 0;
			SoundCalls.GetChannelStatus(i, &status);

			if ((status & CHANNEL_STATUS_PLAYING) == 0)
			{
				StopSound(i);
				continue;
			}
		}

		if (Channels[i].flags & CHAN_TRACKING_POS)
		{
			info->position.vx = info->srcposition.vx;
			info->position.vy = info->srcposition.vy;
			info->position.vz = info->srcposition.vz;

			info->srcposition.vx = (float)info->trackingPosition->vx;
			info->srcposition.vy = (float)-info->trackingPosition->vy;
			info->srcposition.vz = (float)info->trackingPosition->vz;

			Channels[i].flags |= CHAN_POSITION;
		}

		if (Channels[i].flags & CHAN_PITCH)
		{
			SoundCalls.SetChannelFrequency(i, info->pitch);
			Channels[i].flags &= ~CHAN_PITCH;
		}

		if (Channels[i].flags & CHAN_VOLUME)
		{
			SoundCalls.SetChannelVolume(i, info->volumeScale);
			Channels[i].flags &= ~CHAN_VOLUME;
		}

		if (Channels[i].flags & CHAN_POSITION)
		{
			SoundCalls.SetChannelPosition(i, info->srcposition.vx, info->srcposition.vy, info->srcposition.vz);

			if (gUseDoppler)
			{
				SoundCalls.SetChannelVelocity(i, info->srcposition.vx - info->position.vx, info->srcposition.vy - info->position.vy,
											  info->srcposition.vz - info->position.vz);
			}

			Channels[i].flags &= ~CHAN_POSITION;
		}

		if (Channels[i].flags & CHAN_QUEUED)
		{
			if (SoundCalls.PlayChannel(i, info->looping))
			{
				Channels[i].flags &= ~CHAN_QUEUED;
				Channels[i].flags |= CHAN_ACTIVE;
			}
			else
			{
				StopSound(i);
			}
		}
	}

	SoundCalls.EndFrame();
	SoundCalls.StartFrame();
}

// [0x52615b] [D]
// With sound off every channel is silent.
void ReallyUpdateSound(void)
{
	int i;

	if (SoundEnabled == 0 || gAllowSounds == 0)
	{
		for (i = 0; i < MAX_CHANNELS; i++)
			Channels[i].info.volumeScale = VOLUME_MIN;
	}
}

// [0x52619e] [D]
void SetListenerPosition(float x, float y, float z)
{
	Listener[1][0] = Listener[0][0];
	Listener[1][1] = Listener[0][1];
	Listener[1][2] = Listener[0][2];

	Listener[0][0] = x;
	Listener[0][1] = y;
	Listener[0][2] = z;
}

// [0x5261df] [D]
// Plays the sample (a bank sound id >= 4096 is mapped through SoundIDFixups) on the channel,
// or with channel -1 on a free one, else on the last unlocked one playing a sample of no higher
// priority. Returns the channel or -1.
int Play3DSound(int sample, int channel, float x, float y, float z)
{
	int lowest;

	if (SoundEnabled == 0 || gAllowSounds == 0)
		return -1;

	if (sample >= 4096)
		sample = SoundIDFixups[sample >> 12] + (u_char)sample;

	if (sample < 0 || sample >= NumSamples)
		return -1;

	if (Samples[sample].flags & SAMP_INVALID)
		return -1;

	if (channel == -1)
	{
		lowest = -1;

		for (channel = MAX_CHANNELS - 1; channel >= 0; channel--)
		{
			if (Channels[channel].locked)
				continue;

			if ((Channels[channel].flags & (CHAN_ACTIVE | CHAN_QUEUED)) == 0)
				break;

			if ((u_char)Samples[sample].priority <= (u_char)Channels[channel].info.priority)
				lowest = channel;
		}

		if (channel == -1)
			channel = lowest;
	}

	if (channel == -1)
		return -1;

	StartSound(channel, sample, x, y, z);

	return channel;
}

// [0x52630c] [D]
void StopSound(int channel)
{
	if (SoundEnabled == 0)
		return;

	if (Channels[channel].flags)
		SoundCalls.StopChannel(channel);

	Channels[channel].flags = 0;
	Channels[channel].sample = -1;
}

// [0x526355] [D]
void SimplePlaySound(int sample, int channel, int frequency)
{
	int chan;

	if (SoundEnabled == 0 || gAllowSounds == 0)
		return;

	if (sample >= 4096)
		sample = SoundIDFixups[sample >> 12] + (u_char)sample;

	if (sample < NumSamples)
	{
		chan = Play3DSound(sample, channel, 0.0f, 0.0f, 0.0f);

		if (chan != -1)
			SetSoundFreq(chan, frequency);
	}
}

// [0x5263cb] [D]
void SetSoundPos(int channel, float x, float y, float z)
{
	CHANNEL_SAMPLE_DATA *info;

	if (SoundEnabled == 0 || gAllowSounds == 0 || (Channels[channel].flags & (CHAN_ACTIVE | CHAN_QUEUED)) == 0)
		return;

	info = &Channels[channel].info;

	if (Channels[channel].flags & CHAN_TRACKING_POS)
		Channels[channel].flags &= ~CHAN_TRACKING_POS;

	info->position.vx = info->srcposition.vx;
	info->position.vy = info->srcposition.vy;
	info->position.vz = info->srcposition.vz;

	info->srcposition.vx = x;
	info->srcposition.vy = y;
	info->srcposition.vz = z;

	Channels[channel].flags |= CHAN_POSITION;
}

// [0x52648f] [D]
// frequency -1: the sample's own.
void SetSoundFreq(int channel, int frequency)
{
	CHANNEL_SAMPLE_DATA *info;

	if (SoundEnabled == 0 || gAllowSounds == 0 || (Channels[channel].flags & (CHAN_ACTIVE | CHAN_QUEUED)) == 0)
		return;

	info = &Channels[channel].info;

	if (frequency == -1)
		frequency = info->srcpitch;

	info->pitch = frequency;

	Channels[channel].flags |= CHAN_PITCH;
}

// [0x526502] [D]
// volume in hundredths of a dB (-10000 - 0), 0x7fff: the sample's own; scaled by sw_volume.
void SetSoundVol(int channel, int volume)
{
	CHANNEL_SAMPLE_DATA *info;

	if (SoundEnabled == 0 || gAllowSounds == 0 || (Channels[channel].flags & (CHAN_ACTIVE | CHAN_QUEUED)) == 0)
		return;

	info = &Channels[channel].info;

	if (volume == 0x7fff)
		volume = info->srcvolume;

	if (volume < VOLUME_MIN)
		volume = VOLUME_MIN;

	if (volume > VOLUME_MAX)
		volume = VOLUME_MAX;

	info->volume = volume;
	info->volumeScale = volume_table[(int)((float)(volume + 10000) * sw_volume)];

	Channels[channel].flags |= CHAN_VOLUME;
}

// [0x5265d5] [D]
// Position (y is flipped), pitch and volume at once; -1 / 0x7fff keep the sample's own.
void SetSoundChannel(int channel, LONGVECTOR3 *position, int pitch, int volume)
{
	CHANNEL_SAMPLE_DATA *info;

	if (SoundEnabled == 0 || gAllowSounds == 0 || (Channels[channel].flags & (CHAN_ACTIVE | CHAN_QUEUED)) == 0)
		return;

	info = &Channels[channel].info;

	if (Channels[channel].flags & CHAN_TRACKING_POS)
		Channels[channel].flags &= ~CHAN_TRACKING_POS;

	info->position.vx = info->srcposition.vx;
	info->position.vy = info->srcposition.vy;
	info->position.vz = info->srcposition.vz;

	info->srcposition.vx = (float)(*position)[0];
	info->srcposition.vy = (float)-(*position)[1];
	info->srcposition.vz = (float)(*position)[2];

	if (pitch == -1)
		pitch = info->srcpitch;

	info->pitch = pitch;

	if (volume == 0x7fff)
		volume = info->srcvolume;

	if (volume < VOLUME_MIN)
		volume = VOLUME_MIN;

	if (volume > VOLUME_MAX)
		volume = VOLUME_MAX;

	info->volume = volume;
	info->volumeScale = volume_table[(int)((float)(volume + 10000) * sw_volume)];

	Channels[channel].flags |= CHAN_POSITION | CHAN_PITCH | CHAN_VOLUME;
}

// [0x526732] [D]
// The channel follows the position each frame (NULL: no longer).
void SetTrackingSound(int channel, IVECTOR *position)
{
	if (SoundEnabled == 0 || gAllowSounds == 0 || (Channels[channel].flags & (CHAN_ACTIVE | CHAN_QUEUED)) == 0)
		return;

	if (position)
	{
		Channels[channel].info.trackingPosition = position;
		Channels[channel].flags |= CHAN_TRACKING_POS;
	}
	else
	{
		Channels[channel].flags &= ~CHAN_TRACKING_POS;
	}
}

// [0x5267ac] [D]
int GetChannelPitch(int channel)
{
	if (SoundEnabled == 0 || gAllowSounds == 0)
		return 0;

	if (Channels[channel].flags & CHAN_ACTIVE)
		return Channels[channel].info.pitch;

	return 0;
}

// [0x5267ec] [D]
short GetChannelVolume(int channel)
{
	if (SoundEnabled == 0 || gAllowSounds == 0)
		return 0;

	if (Channels[channel].flags & CHAN_ACTIVE)
		return Channels[channel].info.volume;

	return 0;
}

// [0x52682f] [D]
// A locked channel is not taken by Play3DSound(-1).
void LockSoundChannel(int channel)
{
	if ((unsigned int)channel < MAX_CHANNELS)
		Channels[channel].locked = 1;
}

// [0x52684f] [D]
void UnlockSoundChannel(int channel)
{
	if ((unsigned int)channel < MAX_CHANNELS)
		Channels[channel].locked = 0;
}

// [0x52686f] [D]
void UnlockAllSoundChannels(void)
{
	int i;

	for (i = 0; i < MAX_CHANNELS; i++)
		UnlockSoundChannel(i);
}

// [0x52689d] [D]
// Fades the volume out over 10 frames (UpdateSound).
void PauseSounds(void)
{
	if (SoundEnabled == 0 || SoundPaused)
		return;

	FadeSoundCurrent = sw_volume;
	FadeSoundTarget = 0.0f;
	FadeSoundStep = sw_volume / 10.0f;
	FadeSoundCount = 10;
	SoundPaused = 1;
}

// [0x5268f0] [D]
void UnPauseSounds(void)
{
	int i;

	if (SoundEnabled == 0 || SoundPaused == 0)
		return;

	SetEffectsVolume(gMasterVolume);

	for (i = 0; i < MAX_CHANNELS; i++)
	{
		if (Channels[i].flags & CHAN_ACTIVE)
			SetSoundVol(i, Channels[i].info.volume);
	}

	SoundPaused = 0;
}

// [0x526978] [D]
int ChannelInUse(int channel)
{
	return (Channels[channel].flags & (CHAN_ACTIVE | CHAN_QUEUED)) != 0;
}

// [0x52699b] [D]
// Loads the samples listed in a .DSS file (one wave file and its options per line, ';' starts a
// comment). bank: the sound id base (n << 12) that maps to the first of them.
// NOTE: returns 0 (NumSamples - sample, both counted per line), as in the original
int LoadSounds(char *filename, unsigned short bank)
{
	char buffer[128];
	int sample;
	FILE *f;

	if (SoundEnabled == 0)
		return 0;

	sample = NumSamples;

	if (bank)
		SoundIDFixups[(int)bank >> 12] = NumSamples;

	f = fopen(filename, "r");

	if (f == NULL)
		return 0;

	while (LSReadLine(f, buffer) != -1)
	{
		if (buffer[0] != ';')
		{
			LSProcessLine(buffer);
			sample++;
		}
	}

	fclose(f);

	return NumSamples - sample;
}

// [0x526a64] [D]
// Replaces the sample with another wave file (a 2D sound); 1 when loaded.
// NOTE: returns 1 when the sound system is off
int ChangeSample(char *filename, int sample)
{
	if (SoundEnabled == 0)
		return 1;

	if (sample < 0 || sample >= NumSamples)
	{
		LogQuick("ChangeSample: Invalid sample number");
		return 0;
	}

	ReleaseSample(sample);

	Samples[sample].frequency = SoundCalls.LoadSoundBuffer(sample, filename);

	if (Samples[sample].frequency == -1)
	{
		LogQuick("ChangeSample: Error loading sample");
		Samples[sample].flags = SAMP_INVALID;
		return 0;
	}

	Samples[sample].flags = SAMP_2D;
	Samples[sample].volume = 0;
	Samples[sample].priority = 8;

	return 1;
}

// [0x526b16] [D]
// Sets the channel up for the sample (restarting it if it already has it); it plays from the
// next UpdateSound.
void StartSound(int channel, int sample, float x, float y, float z)
{
	CHANNEL_SAMPLE_DATA *info;
	int looped;
	int volume;

	looped = 0;

	if (SoundEnabled == 0 || channel == -1 || gAllowSounds == 0)
		return;

	if ((unsigned short)Channels[channel].sample == sample)
	{
		Channels[channel].flags = 0;
		SoundCalls.RestartChannel(channel);
	}
	else
	{
		StopSound(channel);
		SoundCalls.DuplicateSample(sample, channel);
	}

	volume = Samples[sample].volume;

	if (volume < VOLUME_MIN)
		volume = VOLUME_MIN;

	if (volume > VOLUME_MAX)
		volume = VOLUME_MAX;

	info = &Channels[channel].info;

	// NOTE: clears only the first 4 bytes (srcposition.vx), as in the original
	memset(info, 0, 4);

	info->flags = Samples[sample].flags;
	info->priority = Samples[sample].priority;
	info->srcvolume = volume;
	info->volume = volume;
	info->volumeScale = volume_table[(int)((float)(volume + 10000) * sw_volume)];
	info->srcpitch = Samples[sample].frequency;
	info->pitch = Samples[sample].frequency;
	info->position.vx = x;
	info->srcposition.vx = x;
	info->position.vy = y;
	info->srcposition.vy = y;
	info->position.vz = z;
	info->srcposition.vz = z;

	if (info->flags & SAMP_LOOP)
		looped = 1;

	info->looping = looped;

	if (info->flags & SAMP_2D)
		SoundCalls.SetChannelMode(channel, CHANNEL_MODE_DISABLE);
	else
		SoundCalls.SetChannelMode(channel, CHANNEL_MODE_NORMAL);

	Channels[channel].sample = sample;
	Channels[channel].flags = CHAN_POSITION | CHAN_PITCH | CHAN_VOLUME | CHAN_QUEUED;
}

// [0x526d06] [D]
void ReleaseSample(int sample)
{
	int i;

	for (i = 0; i < MAX_CHANNELS; i++)
	{
		if ((unsigned short)Channels[i].sample == sample)
			StopSound(i);
	}

	SoundCalls.ReleaseSample(sample);
	Samples[sample].flags = SAMP_INVALID;
}

// [0x526d60] [D]
// One .DSS line: "<wave file> [/LOOP] [/2D] [/FREQ=n] [/VOL=n] [/PRI=n]".
void LSProcessLine(char *buffer)
{
	char filename[256];
	char *option;
	int i;

	i = 0;

	_strupr(buffer);

	while (buffer[i] != ' ')
	{
		filename[i] = buffer[i];
		i++;
	}

	filename[i] = 0;

	Samples[NumSamples].frequency = SoundCalls.LoadSoundBuffer(NumSamples, filename);

	if (Samples[NumSamples].frequency == -1)
	{
		LogQuick("Error while loading sample");
		Samples[NumSamples++].flags = SAMP_INVALID;
		return;
	}

	Samples[NumSamples].flags = 0;
	Samples[NumSamples].volume = 0;
	Samples[NumSamples].priority = 8;

	if (strstr(buffer, "/LOOP"))
		Samples[NumSamples].flags |= SAMP_LOOP;

	if (strstr(buffer, "/2D"))
		Samples[NumSamples].flags |= SAMP_2D;

	option = strstr(buffer, "/FREQ=");

	if (option)
		Samples[NumSamples].frequency = LSReadInt(&option[strlen("/FREQ=")]);

	option = strstr(buffer, "/VOL=");

	if (option)
		Samples[NumSamples].volume = LSReadInt(&option[strlen("/VOL=")]);

	option = strstr(buffer, "/PRI=");

	if (option)
		Samples[NumSamples].priority = LSReadInt(&option[strlen("/PRI=")]);

	NumSamples++;
}

// [0x526faa] [D]
// A decimal number up to a space or the end; 0 if it has other characters.
int LSReadInt(char *buffer)
{
	int value;
	u_char c;
	char negative;

	value = 0;
	negative = 0;

	if (*buffer == '-')
	{
		negative = 1;
		buffer++;
	}

	while (*buffer && *buffer != ' ')
	{
		c = *buffer++;

		if (c < '0' || c > '9')
			return 0;

		value = 10 * value + c - '0';
	}

	if (negative)
		return -value;

	return value;
}

// [0x527059] [D]
// Reads a non-empty line, tabs and space runs turned into one space. Returns its length, -1 at
// the end of the file.
// NOTE: a last line without a newline is not terminated (as in the original)
int LSReadLine(FILE *f, char *buffer)
{
	int length;
	int space;
	int c;

	length = 0;
	space = 0;

	for (;;)
	{
		c = fgetc(f);

		if (c == EOF)
			break;

		if (c == '\n' || c == '\r')
		{
			if (length)
			{
				*buffer = 0;
				return length;
			}
		}
		else if (c == '\t' || c == ' ')
		{
			if (space == 0 && length > 0)
			{
				*buffer++ = ' ';
				space = 1;
				length++;
			}
		}
		else
		{
			*buffer++ = c;
			space = 0;
			length++;
		}
	}

	if (length)
		return length;

	return -1;
}
