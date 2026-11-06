/*
 * eax/alsound.c - [A] OpenAL Soft back end of the sound system (SOUND_CALLBACKS), the replacement
 * of eax/directsound.c; the EAX reverb presets go through EFX
 *
 * Not in the original. The structure follows eax/directsound.c: one source per channel, one buffer
 * per sample. Differences from DirectSound worth remembering:
 * - DirectSound is left handed and OpenAL right handed, so z is negated on the way in. Without that
 *   left and right swap.
 * - the distance model of DirectSound is AL_INVERSE_DISTANCE_CLAMPED, with the minimum distance as
 *   the reference distance; distance factor becomes the speed of sound in world units.
 * - gSoundCompatibility (hardware buffers, voice manager) means nothing here and is ignored.
 */

#include <stdio.h>
#include <string.h>
#include <math.h>

#define AL_ALEXT_PROTOTYPES
#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>
#include <AL/efx.h>
#include <AL/efx-presets.h>

#include "dr_types.h"
#include "dr_globals.h"
#include "debug.h"
#include "general.h"
#include "shell.h"
#include "sound.h"
#include "eax/alsound.h"

#define MAX_AL_CHANNELS		20
#define MAX_AL_BUFFERS		80
#define MAX_AL_DEVICES		8

#define SPEED_OF_SOUND		343.3f	// m/s, what DirectSound assumes

// NOTE: a mono source in the middle comes out of OpenAL at -3 dB on each side, DirectSound plays it
// at full volume on both. The value is calculated, not measured against DirectSound.
#define MONO_CENTRE_GAIN	1.4142f

// Velocities are differences between frames, so a camera cut gives a huge one. At the speed of sound
// OpenAL silences the source or sends its pitch to infinity for that frame, so velocities stop at
// this part of it (about 600 km/h, well over what a car does).
#define MAX_SPEED_PART		0.5f

typedef struct AL_SOUND_BUFFER
{
	ALuint buffer;
	int rate;		// of the wave data; AL_PITCH is relative to this
} AL_SOUND_BUFFER;

typedef struct AL_SOUND_CHANNEL
{
	ALuint source;
	int sample;		// -1: nothing attached, what a NULL lpDSBuffer is in eax/directsound.c
} AL_SOUND_CHANNEL;

void InitOpenAL(void);
void OpenAL_CloseAudio(void);
void OpenAL_StartFrame(void);
void OpenAL_EndFrame(void);
void OpenAL_SetMasterVolume(int volume);
void OpenAL_SetSoundFormat(int bits, int frequency);
void OpenAL_SetSpeakerMode(int mode);
void OpenAL_SetWorldStats(float distanceFactor, float minDistance, float maxDistance, float rolloffFactor, float dopplerFactor);
void OpenAL_SetEnvironment(int environment);
int OpenAL_LoadSoundBuffer(int sample, char *filename);
int OpenAL_DuplicateSample(int sample, int channel);
void OpenAL_ReleaseSample(int sample);
void OpenAL_SetListenerPosition(float x, float y, float z);
void OpenAL_SetListenerOrientation(float frontX, float frontY, float frontZ, float topX, float topY, float topZ);
void OpenAL_SetListenerVelocity(float x, float y, float z);
int OpenAL_PlayChannel(int channel, int looping);
void OpenAL_StopChannel(int channel);
void OpenAL_RestartChannel(int channel);
void OpenAL_SetChannelVolume(int channel, int volume);
void OpenAL_SetChannelFrequency(int channel, int frequency);
void OpenAL_SetChannelPosition(int channel, float x, float y, float z);
void OpenAL_SetChannelVelocity(int channel, float x, float y, float z);
void OpenAL_SetChannelMode(int channel, int mode);
void OpenAL_GetChannelStatus(int channel, void *status);

static LPALDEFERUPDATESSOFT DeferUpdates;
static LPALPROCESSUPDATESSOFT ProcessUpdates;
static LPALCRESETDEVICESOFT ResetDevice;

static char device_names[MAX_AL_DEVICES][256];	// full names: SoundDevices[].name are cut short

static ALCdevice *al_device;
static ALCcontext *al_context;
static AL_SOUND_CHANNEL SoundChannels[MAX_AL_CHANNELS];
static AL_SOUND_BUFFER SoundBuffers[MAX_AL_BUFFERS];
static ALuint reverb_effect;
static ALuint reverb_slot;

static float al_speed_of_sound = SPEED_OF_SOUND;
static float al_min_distance = 100.0f;
static float al_max_distance = 10000.0f;
static float al_rolloff = 1.0f;

// The EFX forms of the EAX environments in the table of eax/directsound.c
static const EFXEAXREVERBPROPERTIES environments[7] = {
	EFX_REVERB_PRESET_AUDITORIUM,
	EFX_REVERB_PRESET_CITY,
	EFX_REVERB_PRESET_ALLEY,
	EFX_REVERB_PRESET_PARKINGLOT,
	EFX_REVERB_PRESET_UNDERWATER,
	EFX_REVERB_PRESET_ROOM,
	EFX_REVERB_PRESET_SEWERPIPE,
};

// volume, decay time, damping of that table
static const float environment_params[7][3] = {
	{ 0.403f, 4.279f, 0.5f },
	{ 0.111f, 2.767f, 0.224f },
	{ 0.25f, 1.752f, 0.776f },
	{ 0.208f, 1.652f, 1.5f },
	{ 1.0f, 1.499f, 0.0f },
	{ 0.417f, 0.4f, 0.666f },
	{ 0.652f, 2.886f, 0.25f },
};

// [A]
// Adds a device to SoundDevices, the name cut to 31 characters with "..." as in
// DirectSound_EnumerateDevice. Device names are UTF-8 and the frontend font has no such characters.
static void AddDevice(const char *name, const char *shown)
{
	char *out;
	int i;

	if (num_soundcards >= MAX_AL_DEVICES)
		return;

	strncpy(device_names[num_soundcards], name, sizeof(device_names[0]) - 1);

	out = SoundDevices[num_soundcards].name;

	for (i = 0; i < 31 && shown[i]; i++)
		out[i] = (shown[i] & 0x80) ? '?' : shown[i];

	out[i] = 0;

	if (shown[i])
		strcpy(&out[28], "...");

	num_soundcards++;
}

// [A]
// As with DirectSound the first device is the system's default one, which follows the system when
// that changes.
void OpenAL_EnumerateSoundCards(void)
{
	const ALCchar *devices;
	const char *name;

	AddDevice("", "Default");

	if (alcIsExtensionPresent(NULL, "ALC_ENUMERATE_ALL_EXT"))
		devices = alcGetString(NULL, ALC_ALL_DEVICES_SPECIFIER);
	else
		devices = alcGetString(NULL, ALC_DEVICE_SPECIFIER);

	// a list of strings ended by an empty one
	for (; devices && *devices; devices += strlen(devices) + 1)
	{
		name = devices;

		if (strncmp(name, "OpenAL Soft on ", 15) == 0)
			name += 15;

		AddDevice(devices, name);
	}
}

// [A]
// gAudioSpeakerMode as an OpenAL output mode: 0 stereo, 1 quad, 2 mono, 3 headphones, 4 surround.
static ALCint SpeakerModeToAL(int mode)
{
	switch (mode)
	{
		case 1:
			return ALC_QUAD_SOFT;
		case 2:
			return ALC_MONO_SOFT;
		case 3:
			return ALC_STEREO_HRTF_SOFT;
		case 4:
			return ALC_SURROUND_5_1_SOFT;
		default:
			return ALC_STEREO_SOFT;
	}
}

// [A]
// The reverb is optional: without it the game just sounds dry, as DirectSound does without EAX.
static void CreateReverb(void)
{
	alGetError();

	alGenEffects(1, &reverb_effect);
	alEffecti(reverb_effect, AL_EFFECT_TYPE, AL_EFFECT_EAXREVERB);
	alGenAuxiliaryEffectSlots(1, &reverb_slot);

	if (alGetError() == AL_NO_ERROR)
		return;

	LogQuick("OpenAL: no EFX reverb");

	if (reverb_slot)
		alDeleteAuxiliaryEffectSlots(1, &reverb_slot);

	if (reverb_effect)
		alDeleteEffects(1, &reverb_effect);

	reverb_slot = 0;
	reverb_effect = 0;
}

// [A]
// All sounds, 2D ones too: the frontend's are 2D and EAX reverberates them. The send leaves out
// MONO_CENTRE_GAIN, which makes up for panning that the reverb doesn't go through.
static void SendToReverb(const ALuint *sources)
{
	ALuint filter;
	int i;

	alGenFilters(1, &filter);
	alFilteri(filter, AL_FILTER_TYPE, AL_FILTER_LOWPASS);
	alFilterf(filter, AL_LOWPASS_GAIN, 1.0f / MONO_CENTRE_GAIN);
	alFilterf(filter, AL_LOWPASS_GAINHF, 1.0f);

	for (i = 0; i < MAX_AL_CHANNELS; i++)
		alSource3i(sources[i], AL_AUXILIARY_SEND_FILTER, reverb_slot, 0, filter);

	// a source keeps a copy of the filter
	alDeleteFilters(1, &filter);
}

// [A]
int InitOpenALSystem(int device)
{
	ALuint sources[MAX_AL_CHANNELS];
	ALCint attribs[3];
	int i;

	if (device < 0 || device >= num_soundcards)
		return 0;

	al_device = alcOpenDevice(device_names[device][0] ? device_names[device] : NULL);

	if (al_device == NULL)
		return 0;

	// a new device has to be told the speakers again, where DirectSound kept them in the system
	attribs[0] = ALC_OUTPUT_MODE_SOFT;
	attribs[1] = SpeakerModeToAL(gAudioSpeakerMode);
	attribs[2] = 0;

	al_context = alcCreateContext(al_device, attribs);

	if (al_context == NULL || !alcMakeContextCurrent(al_context))
	{
		OpenAL_CloseAudio();
		return 0;
	}

	// not in the import library
	DeferUpdates = (LPALDEFERUPDATESSOFT)alGetProcAddress("alDeferUpdatesSOFT");
	ProcessUpdates = (LPALPROCESSUPDATESSOFT)alGetProcAddress("alProcessUpdatesSOFT");
	ResetDevice = (LPALCRESETDEVICESOFT)alcGetProcAddress(al_device, "alcResetDeviceSOFT");

	alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);

	alGetError();
	alGenSources(MAX_AL_CHANNELS, sources);

	if (alGetError() != AL_NO_ERROR)
	{
		OpenAL_CloseAudio();
		return 0;
	}

	for (i = 0; i < MAX_AL_CHANNELS; i++)
	{
		SoundChannels[i].source = sources[i];
		SoundChannels[i].sample = -1;

		alSourcef(sources[i], AL_MAX_GAIN, MONO_CENTRE_GAIN);
	}

	if (EnableEAX)
		CreateReverb();

	if (reverb_slot)
		SendToReverb(sources);

	InitOpenAL();

	return 1;
}

// [A]
void InitOpenAL(void)
{
	SoundCalls.CloseAudio = OpenAL_CloseAudio;
	SoundCalls.StartFrame = OpenAL_StartFrame;
	SoundCalls.EndFrame = OpenAL_EndFrame;
	SoundCalls.SetMasterVolume = OpenAL_SetMasterVolume;
	SoundCalls.SetSoundFormat = OpenAL_SetSoundFormat;
	SoundCalls.SetSpeakerMode = OpenAL_SetSpeakerMode;
	SoundCalls.SetWorldStats = OpenAL_SetWorldStats;
	SoundCalls.SetEnvironment = OpenAL_SetEnvironment;
	SoundCalls.LoadSoundBuffer = OpenAL_LoadSoundBuffer;
	SoundCalls.DuplicateSample = OpenAL_DuplicateSample;
	SoundCalls.ReleaseSample = OpenAL_ReleaseSample;
	SoundCalls.SetListenerPosition = OpenAL_SetListenerPosition;
	SoundCalls.SetListenerOrientation = OpenAL_SetListenerOrientation;
	SoundCalls.SetListenerVelocity = OpenAL_SetListenerVelocity;
	SoundCalls.PlayChannel = OpenAL_PlayChannel;
	SoundCalls.StopChannel = OpenAL_StopChannel;
	SoundCalls.RestartChannel = OpenAL_RestartChannel;
	SoundCalls.SetChannelVolume = OpenAL_SetChannelVolume;
	SoundCalls.SetChannelFrequency = OpenAL_SetChannelFrequency;
	SoundCalls.SetChannelPosition = OpenAL_SetChannelPosition;
	SoundCalls.SetChannelVelocity = OpenAL_SetChannelVelocity;
	SoundCalls.SetChannelMode = OpenAL_SetChannelMode;
	SoundCalls.GetChannelStatus = OpenAL_GetChannelStatus;
}

// [A]
void OpenAL_CloseAudio(void)
{
	int i;

	for (i = 0; i < MAX_AL_CHANNELS; i++)
	{
		if (SoundChannels[i].source)
		{
			alSourceStop(SoundChannels[i].source);
			alSourcei(SoundChannels[i].source, AL_BUFFER, 0);
			alSource3i(SoundChannels[i].source, AL_AUXILIARY_SEND_FILTER, AL_EFFECTSLOT_NULL, 0, AL_FILTER_NULL);
			alDeleteSources(1, &SoundChannels[i].source);

			SoundChannels[i].source = 0;
		}
	}

	if (reverb_slot)
		alDeleteAuxiliaryEffectSlots(1, &reverb_slot);

	if (reverb_effect)
		alDeleteEffects(1, &reverb_effect);

	reverb_slot = 0;
	reverb_effect = 0;

	alcMakeContextCurrent(NULL);

	if (al_context)
		alcDestroyContext(al_context);

	if (al_device)
		alcCloseDevice(al_device);

	al_context = NULL;
	al_device = NULL;
}

// [A]
// Changes are held back until EndFrame, like the deferred 3D settings of DirectSound; here that takes
// in volume and pitch too, which sound.c only sends along with them in UpdateSound anyway.
void OpenAL_StartFrame(void)
{
	if (DeferUpdates)
		DeferUpdates();
}

// [A]
void OpenAL_EndFrame(void)
{
	if (ProcessUpdates)
		ProcessUpdates();
}

// [A]
// 0 - 0xffff. Nothing calls this, in the original either.
void OpenAL_SetMasterVolume(int volume)
{
	if (volume < 0)
		volume = 0;

	if (volume > 0xffff)
		volume = 0xffff;

	alListenerf(AL_GAIN, (float)volume / 65535.0f);
}

// [A]
// NOTE: does nothing: the mixer bits and frequency options set the format of the primary buffer,
// and OpenAL mixes at whatever suits the device
void OpenAL_SetSoundFormat(int bits, int frequency)
{
}

// [A]
// mode (gAudioSpeakerMode): 0 stereo, 1 quad, 2 mono, 3 headphones (HRTF), 4 surround (5.1).
void OpenAL_SetSpeakerMode(int mode)
{
	ALCint attribs[3];

	if (ResetDevice == NULL)
		return;

	attribs[0] = ALC_OUTPUT_MODE_SOFT;
	attribs[1] = SpeakerModeToAL(mode);
	attribs[2] = 0;

	ResetDevice(al_device, attribs);
}

// [A]
// distanceFactor is metres per world unit, and velocities are in world units.
void OpenAL_SetWorldStats(float distanceFactor, float minDistance, float maxDistance, float rolloffFactor, float dopplerFactor)
{
	al_min_distance = minDistance;
	al_max_distance = maxDistance;
	al_rolloff = rolloffFactor;
	al_speed_of_sound = SPEED_OF_SOUND / distanceFactor;

	alSpeedOfSound(al_speed_of_sound);
	alDopplerFactor(dopplerFactor);
}

// [A]
// The preset with the three values EAX 1.0 sets on top of it, translated as OpenAL Soft translates
// EAX 1.0: volume is the late reverb gain, damping the HF decay ratio.
void OpenAL_SetEnvironment(int environment)
{
	const EFXEAXREVERBPROPERTIES *reverb;
	const float *params;
	float damping;

	if (reverb_effect == 0)
		return;

	reverb = &environments[environment];
	params = environment_params[environment];

	// 0 in the underwater environment, below what EFX takes: the effect would keep the last value
	damping = params[2] < AL_EAXREVERB_MIN_DECAY_HFRATIO ? AL_EAXREVERB_MIN_DECAY_HFRATIO : params[2];

	alEffectf(reverb_effect, AL_EAXREVERB_DENSITY, reverb->flDensity);
	alEffectf(reverb_effect, AL_EAXREVERB_DIFFUSION, reverb->flDiffusion);
	alEffectf(reverb_effect, AL_EAXREVERB_GAIN, reverb->flGain);
	alEffectf(reverb_effect, AL_EAXREVERB_GAINHF, reverb->flGainHF);
	alEffectf(reverb_effect, AL_EAXREVERB_GAINLF, reverb->flGainLF);
	alEffectf(reverb_effect, AL_EAXREVERB_DECAY_TIME, params[1]);
	alEffectf(reverb_effect, AL_EAXREVERB_DECAY_HFRATIO, damping);
	alEffectf(reverb_effect, AL_EAXREVERB_DECAY_LFRATIO, reverb->flDecayLFRatio);
	alEffectf(reverb_effect, AL_EAXREVERB_REFLECTIONS_GAIN, reverb->flReflectionsGain);
	alEffectf(reverb_effect, AL_EAXREVERB_REFLECTIONS_DELAY, reverb->flReflectionsDelay);
	alEffectfv(reverb_effect, AL_EAXREVERB_REFLECTIONS_PAN, reverb->flReflectionsPan);
	alEffectf(reverb_effect, AL_EAXREVERB_LATE_REVERB_GAIN, params[0]);
	alEffectf(reverb_effect, AL_EAXREVERB_LATE_REVERB_DELAY, reverb->flLateReverbDelay);
	alEffectfv(reverb_effect, AL_EAXREVERB_LATE_REVERB_PAN, reverb->flLateReverbPan);
	alEffectf(reverb_effect, AL_EAXREVERB_ECHO_TIME, reverb->flEchoTime);
	alEffectf(reverb_effect, AL_EAXREVERB_ECHO_DEPTH, reverb->flEchoDepth);
	alEffectf(reverb_effect, AL_EAXREVERB_MODULATION_TIME, reverb->flModulationTime);
	alEffectf(reverb_effect, AL_EAXREVERB_MODULATION_DEPTH, reverb->flModulationDepth);
	alEffectf(reverb_effect, AL_EAXREVERB_AIR_ABSORPTION_GAINHF, reverb->flAirAbsorptionGainHF);
	alEffectf(reverb_effect, AL_EAXREVERB_HFREFERENCE, reverb->flHFReference);
	alEffectf(reverb_effect, AL_EAXREVERB_LFREFERENCE, reverb->flLFReference);
	alEffectf(reverb_effect, AL_EAXREVERB_ROOM_ROLLOFF_FACTOR, reverb->flRoomRolloffFactor);
	alEffecti(reverb_effect, AL_EAXREVERB_DECAY_HFLIMIT, reverb->iDecayHFLimit);

	// the slot takes a copy of the effect
	alAuxiliaryEffectSloti(reverb_slot, AL_EFFECTSLOT_EFFECT, reverb_effect);
}

// [A]
// Finds the "fmt " and "data" chunks of a RIFF WAVE file in memory.
static int ReadWaveData(const char *wave, int size, const char **fmt, const char **data, int *dataSize)
{
	int pos;
	int length;

	*fmt = NULL;
	*data = NULL;

	if (size < 12 || memcmp(wave, "RIFF", 4) != 0 || memcmp(wave + 8, "WAVE", 4) != 0)
		return 0;

	pos = 12;

	while (pos + 8 <= size)
	{
		length = *(const int *)(wave + pos + 4);
		pos += 8;

		if (length < 0 || length > size - pos)
			length = size - pos;

		if (memcmp(wave + pos - 8, "fmt ", 4) == 0 && length >= 16)
			*fmt = wave + pos;
		else if (memcmp(wave + pos - 8, "data", 4) == 0)
		{
			*data = wave + pos;
			*dataSize = length;
		}

		if (*fmt && *data)
			return 1;

		// chunks are 16 bit aligned
		pos += (length + 1) & ~1;
	}

	return 0;
}

// [A]
// Loads a wave file into the sample's buffer. Returns the frequency, -1 on failure.
// NOTE: like DirectSound_LoadSoundBuffer this returns the 4 bytes at file offset 28, which are
// nAvgBytesPerSec (= the frequency for the game's 8 bit mono files). Pitches are relative to the
// real rate, so any file of another format would play as wrongly as it does there.
// Only mono files: DirectSound can't make a 3D buffer of anything else either.
int OpenAL_LoadSoundBuffer(int sample, char *filename)
{
	const char *fmt;
	const char *data;
	char *buffer;
	FILE *f;
	long size;
	int dataSize;
	int frequency;
	int format;
	int channels;
	int bits;

	if (al_context == NULL)
		return -1;

	f = fopen(filename, "rb");

	if (f == NULL)
		return -1;

	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fseek(f, 0, SEEK_SET);

	if (size < 32)
	{
		fclose(f);
		return -1;
	}

	buffer = DebugMalloc(size, __FILE__, __LINE__);

	if (buffer == NULL)
	{
		fclose(f);
		return -1;
	}

	fread(buffer, 1, size, f);
	fclose(f);

	frequency = *(int *)(buffer + 28);

	if (!ReadWaveData(buffer, size, &fmt, &data, &dataSize))
	{
		DebugFree(buffer);
		return -1;
	}

	channels = *(const unsigned short *)(fmt + 2);
	bits = *(const unsigned short *)(fmt + 14);

	if (*(const unsigned short *)fmt != 1 || channels != 1 || (bits != 8 && bits != 16))
	{
		DebugFree(buffer);
		return -1;
	}

	format = bits == 8 ? AL_FORMAT_MONO8 : AL_FORMAT_MONO16;

	SoundBuffers[sample].rate = *(const int *)(fmt + 4);

	alGetError();
	alGenBuffers(1, &SoundBuffers[sample].buffer);
	alBufferData(SoundBuffers[sample].buffer, format, data, dataSize, SoundBuffers[sample].rate);

	DebugFree(buffer);

	if (alGetError() != AL_NO_ERROR)
	{
		OpenAL_ReleaseSample(sample);
		return -1;
	}

	return frequency;
}

// [A]
// Attaches the sample's buffer to the channel's source, and sets that as a new DirectSound buffer
// would be.
int OpenAL_DuplicateSample(int sample, int channel)
{
	ALuint source;

	if (SoundBuffers[sample].buffer == 0)
		return 0;

	source = SoundChannels[channel].source;
	SoundChannels[channel].sample = sample;

	alSourcei(source, AL_BUFFER, SoundBuffers[sample].buffer);
	alSourcei(source, AL_LOOPING, AL_FALSE);
	alSourcef(source, AL_PITCH, 1.0f);
	alSourcef(source, AL_GAIN, MONO_CENTRE_GAIN);
	alSource3f(source, AL_POSITION, 0.0f, 0.0f, 0.0f);
	alSource3f(source, AL_VELOCITY, 0.0f, 0.0f, 0.0f);
	alSourcef(source, AL_REFERENCE_DISTANCE, al_min_distance);
	alSourcef(source, AL_MAX_DISTANCE, al_max_distance);

	return 1;
}

// [A]
// sound.c has stopped the channels playing it.
void OpenAL_ReleaseSample(int sample)
{
	if (SoundBuffers[sample].buffer)
	{
		alDeleteBuffers(1, &SoundBuffers[sample].buffer);
		SoundBuffers[sample].buffer = 0;
	}
}

// [A]
// Scales a velocity given per frame to per second, see MAX_SPEED_PART.
static void ScaleVelocity(float *x, float *y, float *z)
{
	float speed;
	float max;

	*x *= 25.0f;
	*y *= 25.0f;
	*z *= 25.0f;

	speed = sqrtf(*x * *x + *y * *y + *z * *z);
	max = al_speed_of_sound * MAX_SPEED_PART;

	if (speed > max)
	{
		*x *= max / speed;
		*y *= max / speed;
		*z *= max / speed;
	}
}

// [A]
void OpenAL_SetListenerPosition(float x, float y, float z)
{
	alListener3f(AL_POSITION, x, y, -z);
}

// [A]
void OpenAL_SetListenerOrientation(float frontX, float frontY, float frontZ, float topX, float topY, float topZ)
{
	float orientation[6];

	orientation[0] = frontX;
	orientation[1] = frontY;
	orientation[2] = -frontZ;
	orientation[3] = topX;
	orientation[4] = topY;
	orientation[5] = -topZ;

	alListenerfv(AL_ORIENTATION, orientation);
}

// [A]
void OpenAL_SetListenerVelocity(float x, float y, float z)
{
	ScaleVelocity(&x, &y, &z);

	alListener3f(AL_VELOCITY, x, y, -z);
}

// [A]
int OpenAL_PlayChannel(int channel, int looping)
{
	if (SoundChannels[channel].sample == -1)
		return 0;

	alSourcei(SoundChannels[channel].source, AL_LOOPING, looping ? AL_TRUE : AL_FALSE);
	alSourcePlay(SoundChannels[channel].source);

	return 1;
}

// [A]
void OpenAL_StopChannel(int channel)
{
	if (SoundChannels[channel].sample == -1)
		return;

	alSourceStop(SoundChannels[channel].source);
	alSourcei(SoundChannels[channel].source, AL_BUFFER, 0);

	SoundChannels[channel].sample = -1;
}

// [A]
void OpenAL_RestartChannel(int channel)
{
	if (SoundChannels[channel].sample == -1)
		return;

	alSourceStop(SoundChannels[channel].source);
	alSourceRewind(SoundChannels[channel].source);
	alSourcef(SoundChannels[channel].source, AL_GAIN, MONO_CENTRE_GAIN);
	alSource3f(SoundChannels[channel].source, AL_VELOCITY, 0.0f, 0.0f, 0.0f);
}

// [A]
// volume in hundredths of a dB, -10000 (silence) - 0.
void OpenAL_SetChannelVolume(int channel, int volume)
{
	float gain;

	if (SoundChannels[channel].sample == -1)
		return;

	gain = volume <= -10000 ? 0.0f : powf(10.0f, (float)volume / 2000.0f);

	alSourcef(SoundChannels[channel].source, AL_GAIN, gain * MONO_CENTRE_GAIN);
}

// [A]
// frequency in Hz, 0 for the sample's own (DSBFREQUENCY_ORIGINAL).
void OpenAL_SetChannelFrequency(int channel, int frequency)
{
	int sample;

	sample = SoundChannels[channel].sample;

	if (sample == -1 || frequency < 0)
		return;

	alSourcef(SoundChannels[channel].source, AL_PITCH, frequency ? (float)frequency / (float)SoundBuffers[sample].rate : 1.0f);
}

// [A]
// A 2D source is head relative and stays at the listener, see OpenAL_SetChannelMode.
void OpenAL_SetChannelPosition(int channel, float x, float y, float z)
{
	ALint relative;

	if (SoundChannels[channel].sample == -1)
		return;

	alGetSourcei(SoundChannels[channel].source, AL_SOURCE_RELATIVE, &relative);

	if (!relative)
		alSource3f(SoundChannels[channel].source, AL_POSITION, x, y, -z);
}

// [A]
void OpenAL_SetChannelVelocity(int channel, float x, float y, float z)
{
	ALint relative;

	if (SoundChannels[channel].sample == -1)
		return;

	alGetSourcei(SoundChannels[channel].source, AL_SOURCE_RELATIVE, &relative);

	if (relative)
		return;

	ScaleVelocity(&x, &y, &z);

	alSource3f(SoundChannels[channel].source, AL_VELOCITY, x, y, -z);
}

// [A]
// mode: CHANNEL_MODE_NORMAL or CHANNEL_MODE_DISABLE, no 3D: at the listener, no distance or Doppler.
void OpenAL_SetChannelMode(int channel, int mode)
{
	ALuint source;

	if (SoundChannels[channel].sample == -1)
		return;

	source = SoundChannels[channel].source;

	if (mode == CHANNEL_MODE_DISABLE)
	{
		alSourcei(source, AL_SOURCE_RELATIVE, AL_TRUE);
		alSource3f(source, AL_POSITION, 0.0f, 0.0f, 0.0f);
		alSource3f(source, AL_VELOCITY, 0.0f, 0.0f, 0.0f);
		alSourcef(source, AL_ROLLOFF_FACTOR, 0.0f);
		alSourcef(source, AL_DOPPLER_FACTOR, 0.0f);
	}
	else
	{
		alSourcei(source, AL_SOURCE_RELATIVE, AL_FALSE);
		alSourcef(source, AL_ROLLOFF_FACTOR, al_rolloff);
		alSourcef(source, AL_DOPPLER_FACTOR, 1.0f);
	}
}

// [A]
void OpenAL_GetChannelStatus(int channel, void *status)
{
	ALint state;

	if (SoundChannels[channel].sample == -1)
		return;

	alGetSourcei(SoundChannels[channel].source, AL_SOURCE_STATE, &state);

	*(int *)status = state == AL_PLAYING ? CHANNEL_STATUS_PLAYING : 0;
}
