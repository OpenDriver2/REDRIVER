/*
 * eax/psxmusic.c - [A] the music of the PSX version: the XM songs of MUSIC.BIN played by eax/xmplay.c
 * through OpenAL, on the thread of the music stream (eax/alstream.cpp)
 *
 * Not in the PC original, which plays the CD tracks during the game too. The PSX version plays one
 * song of MUSIC.BIN per mission and jumps to the pursuit part of it once the cops chase the player
 * (FunkUpDaBGMTunez, gamesnd.c in REDRIVER2). Driver 1 stays there after the pursuit, Driver 2 goes
 * back to the start.
 *
 * MUSIC.BIN: a table of offsets, one pair per song (the XM file, the bank of its samples) and the
 * end of the file. A bank is a count of samples, a SAMPLE_INFO per sample and the samples in PSX
 * ADPCM, as they are written to the SPU RAM.
 *
 * The SPU is the basics of the OpenAL one of PsyCross (PsyX_SPUAL.cpp): a source per voice, its
 * pitch, gain and pan. Unlike there, the bank is converted once when the song is loaded, a buffer per
 * sample with the loop of its ADPCM flags, and a voice finds its sample by the address.
 *
 * Everything but the requests runs in PSXMusic_Tick, on the stream thread where the OpenAL context of
 * the music is current: loading the song, XM_Update 60 times a second as the vertical blank of an NTSC
 * PSX, the voices. The game thread only leaves the song (music_request) and the pursuit position
 * (music_setpos) there.
 *
 * The reverb of the SPU is an EFX reverb on an effect slot of the music context; every voice sends
 * to it (REVERB_PRESET, REVERB_GAIN).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define AL_ALEXT_PROTOTYPES
#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>
#include <AL/efx.h>
#include <AL/efx-presets.h>

#include <SDL.h>

#include "dr_types.h"
#include "debug.h"
#include "eax/alstream.h"
#include "eax/xmplay.h"
#include "eax/psxmusic.h"

#define MUSIC_FILE			"Music\\MUSIC.BIN"
#define MAX_SONGS			32
#define MAX_SAMPLES			128

#define XM_UPDATE_RATE		60		// XM_Update per second (XM_NTSC)

#define SPU_VOICES			24
#define BLOCK_SIZE			16		// bytes of ADPCM
#define BLOCK_SAMPLES		28

// the flags of an ADPCM block
#define ADPCM_LOOP_END		0x1		// jump to the repeat address after this block
#define ADPCM_REPEAT		0x2		// with ADPCM_LOOP_END: loop; without: the voice stops
#define ADPCM_LOOP_START	0x4		// this block is the repeat address

#define STEREO_FACTOR		3.0f	// as PsyCross

// NOTE: the reverb of the SPU (AllocateReverb of REDRIVER2: mode 3, ROOM | STUDIO_A, depth 0x4000 of
// 0x7FFF) as an EFX preset; a guess by ear, not the SPU one. ROOM (0.4 s) was not heard at all
#define REVERB_PRESET		EFX_REVERB_PRESET_QUARRY
#define REVERB_GAIN			1.0f

// the music of the cops, the position in the order of each song (xm_coptrackpos_d1 in REDRIVER2)
static const int xm_coptrackpos[8] = {
	0xB, 0xC, 0xB, 0xB, 8, 0xB, 0xF, 0xC,
};

typedef struct SAMPLE_INFO
{
	unsigned int address;
	unsigned int length;
	unsigned int loop;
	unsigned int samplerate;
} SAMPLE_INFO;

typedef struct XMSPU_VOICE
{
	ALuint source;
	int addr;			// for the next key on
	int pitch;			// 4096: PSXMUSIC_RATE
	int lvol;			// -0x4000 - 0x3FFF
	int rvol;
	int paused;			// by PSXMusic_Tick
} XMSPU_VOICE;

// the game thread
static int music_offsets[MAX_SONGS * 2 + 1];
static int music_songs;				// 0: no MUSIC.BIN
static int music_copmusic;
static int music_song = -1;

static SDL_atomic_t music_request;	// the song << 16 | a serial, one up for every PSXMusic_Load
static SDL_atomic_t music_setpos;	// the position the song goes to, -1: none

// the stream thread
static int music_loaded = -1;		// the music_request loaded
static unsigned char *music_xm;		// the XM file of the song, InitXMData keeps pointers into it
static void *music_header;
static void *music_xmsong;
static float music_volume = 1.0f;
static int music_paused;
static Uint64 music_next;			// the time of the next XM_Update

static SAMPLE_INFO spu_info[MAX_SAMPLES];
static ALuint spu_buffers[MAX_SAMPLES];
static int spu_samples;

static XMSPU_VOICE spu_voices[SPU_VOICES];

static ALuint reverb_slot;			// 0: no EFX, the music plays dry
static ALuint reverb_effect;

static LPALGENEFFECTS palGenEffects;
static LPALDELETEEFFECTS palDeleteEffects;
static LPALEFFECTI palEffecti;
static LPALEFFECTF palEffectf;
static LPALEFFECTFV palEffectfv;
static LPALGENAUXILIARYEFFECTSLOTS palGenAuxiliaryEffectSlots;
static LPALDELETEAUXILIARYEFFECTSLOTS palDeleteAuxiliaryEffectSlots;
static LPALAUXILIARYEFFECTSLOTI palAuxiliaryEffectSloti;
static LPALAUXILIARYEFFECTSLOTF palAuxiliaryEffectSlotf;

//------------------------------------------------------------------------------------------
// The reverb

// [A]
// Makes the reverb slot in the current (music) context; without EFX the music plays dry
static void InitReverb(void)
{
	EFXEAXREVERBPROPERTIES r = REVERB_PRESET;
	ALCdevice *device;

	device = alcGetContextsDevice(alcGetCurrentContext());

	if (device == NULL || !alcIsExtensionPresent(device, "ALC_EXT_EFX"))
	{
		LogQuick("InitReverb: no EFX");
		return;
	}

	palGenEffects = (LPALGENEFFECTS)alGetProcAddress("alGenEffects");
	palDeleteEffects = (LPALDELETEEFFECTS)alGetProcAddress("alDeleteEffects");
	palEffecti = (LPALEFFECTI)alGetProcAddress("alEffecti");
	palEffectf = (LPALEFFECTF)alGetProcAddress("alEffectf");
	palEffectfv = (LPALEFFECTFV)alGetProcAddress("alEffectfv");
	palGenAuxiliaryEffectSlots = (LPALGENAUXILIARYEFFECTSLOTS)alGetProcAddress("alGenAuxiliaryEffectSlots");
	palDeleteAuxiliaryEffectSlots = (LPALDELETEAUXILIARYEFFECTSLOTS)alGetProcAddress("alDeleteAuxiliaryEffectSlots");
	palAuxiliaryEffectSloti = (LPALAUXILIARYEFFECTSLOTI)alGetProcAddress("alAuxiliaryEffectSloti");
	palAuxiliaryEffectSlotf = (LPALAUXILIARYEFFECTSLOTF)alGetProcAddress("alAuxiliaryEffectSlotf");

	if (!palGenEffects || !palDeleteEffects || !palEffecti || !palEffectf || !palEffectfv ||
		!palGenAuxiliaryEffectSlots || !palDeleteAuxiliaryEffectSlots || !palAuxiliaryEffectSloti ||
		!palAuxiliaryEffectSlotf)
	{
		LogQuick("InitReverb: no EFX functions");
		return;
	}

	alGetError();

	palGenEffects(1, &reverb_effect);

	if (alGetError() != AL_NO_ERROR)
	{
		reverb_effect = 0;
		return;
	}

	palEffecti(reverb_effect, AL_EFFECT_TYPE, AL_EFFECT_EAXREVERB);

	if (alGetError() == AL_NO_ERROR)
	{
		palEffectf(reverb_effect, AL_EAXREVERB_DENSITY, r.flDensity);
		palEffectf(reverb_effect, AL_EAXREVERB_DIFFUSION, r.flDiffusion);
		palEffectf(reverb_effect, AL_EAXREVERB_GAIN, r.flGain);
		palEffectf(reverb_effect, AL_EAXREVERB_GAINHF, r.flGainHF);
		palEffectf(reverb_effect, AL_EAXREVERB_GAINLF, r.flGainLF);
		palEffectf(reverb_effect, AL_EAXREVERB_DECAY_TIME, r.flDecayTime);
		palEffectf(reverb_effect, AL_EAXREVERB_DECAY_HFRATIO, r.flDecayHFRatio);
		palEffectf(reverb_effect, AL_EAXREVERB_DECAY_LFRATIO, r.flDecayLFRatio);
		palEffectf(reverb_effect, AL_EAXREVERB_REFLECTIONS_GAIN, r.flReflectionsGain);
		palEffectf(reverb_effect, AL_EAXREVERB_REFLECTIONS_DELAY, r.flReflectionsDelay);
		palEffectfv(reverb_effect, AL_EAXREVERB_REFLECTIONS_PAN, r.flReflectionsPan);
		palEffectf(reverb_effect, AL_EAXREVERB_LATE_REVERB_GAIN, r.flLateReverbGain);
		palEffectf(reverb_effect, AL_EAXREVERB_LATE_REVERB_DELAY, r.flLateReverbDelay);
		palEffectfv(reverb_effect, AL_EAXREVERB_LATE_REVERB_PAN, r.flLateReverbPan);
		palEffectf(reverb_effect, AL_EAXREVERB_ECHO_TIME, r.flEchoTime);
		palEffectf(reverb_effect, AL_EAXREVERB_ECHO_DEPTH, r.flEchoDepth);
		palEffectf(reverb_effect, AL_EAXREVERB_MODULATION_TIME, r.flModulationTime);
		palEffectf(reverb_effect, AL_EAXREVERB_MODULATION_DEPTH, r.flModulationDepth);
		palEffectf(reverb_effect, AL_EAXREVERB_AIR_ABSORPTION_GAINHF, r.flAirAbsorptionGainHF);
		palEffectf(reverb_effect, AL_EAXREVERB_HFREFERENCE, r.flHFReference);
		palEffectf(reverb_effect, AL_EAXREVERB_LFREFERENCE, r.flLFReference);
		palEffectf(reverb_effect, AL_EAXREVERB_ROOM_ROLLOFF_FACTOR, r.flRoomRolloffFactor);
		palEffecti(reverb_effect, AL_EAXREVERB_DECAY_HFLIMIT, r.iDecayHFLimit);
	}
	else
	{
		// no EAX reverb: the standard one
		alGetError();
		palEffecti(reverb_effect, AL_EFFECT_TYPE, AL_EFFECT_REVERB);
		palEffectf(reverb_effect, AL_REVERB_DENSITY, r.flDensity);
		palEffectf(reverb_effect, AL_REVERB_DIFFUSION, r.flDiffusion);
		palEffectf(reverb_effect, AL_REVERB_GAIN, r.flGain);
		palEffectf(reverb_effect, AL_REVERB_GAINHF, r.flGainHF);
		palEffectf(reverb_effect, AL_REVERB_DECAY_TIME, r.flDecayTime);
		palEffectf(reverb_effect, AL_REVERB_DECAY_HFRATIO, r.flDecayHFRatio);
		palEffectf(reverb_effect, AL_REVERB_REFLECTIONS_GAIN, r.flReflectionsGain);
		palEffectf(reverb_effect, AL_REVERB_REFLECTIONS_DELAY, r.flReflectionsDelay);
		palEffectf(reverb_effect, AL_REVERB_LATE_REVERB_GAIN, r.flLateReverbGain);
		palEffectf(reverb_effect, AL_REVERB_LATE_REVERB_DELAY, r.flLateReverbDelay);
		palEffectf(reverb_effect, AL_REVERB_AIR_ABSORPTION_GAINHF, r.flAirAbsorptionGainHF);
		palEffectf(reverb_effect, AL_REVERB_ROOM_ROLLOFF_FACTOR, r.flRoomRolloffFactor);
		palEffecti(reverb_effect, AL_REVERB_DECAY_HFLIMIT, r.iDecayHFLimit);
	}

	palGenAuxiliaryEffectSlots(1, &reverb_slot);

	if (alGetError() != AL_NO_ERROR)
	{
		palDeleteEffects(1, &reverb_effect);
		reverb_effect = 0;
		reverb_slot = 0;
		return;
	}

	palAuxiliaryEffectSloti(reverb_slot, AL_EFFECTSLOT_EFFECT, reverb_effect);
	palAuxiliaryEffectSlotf(reverb_slot, AL_EFFECTSLOT_GAIN, REVERB_GAIN);

	if (alGetError() != AL_NO_ERROR)
		LogQuick("InitReverb: the reverb slot failed");
	else
		LogQuick("InitReverb: the music reverb is on");
}

// [A]
static void FreeReverb(void)
{
	if (reverb_slot)
		palDeleteAuxiliaryEffectSlots(1, &reverb_slot);

	if (reverb_effect)
		palDeleteEffects(1, &reverb_effect);

	reverb_slot = 0;
	reverb_effect = 0;
}

//------------------------------------------------------------------------------------------
// The bank (vagToPcm and decodeSound of PsyCross)

static const float adpcm_k0[5] = { 0.0f, 0.9375f, 1.796875f, 1.53125f, 1.90625f };
static const float adpcm_k1[5] = { 0.0f, 0.0f, -0.8125f, -0.859375f, -0.9375f };

// [A]
// One sample of 4 bits
static short VagToPcm(unsigned char param, int data, float *prev1, float *prev2)
{
	float value;
	int filter;
	int result;

	if (data > 7)
		data -= 16;

	filter = (param >> 4) & 0xF;

	if (filter > 4)
		filter = 4;

	value = (float)data * (float)(1 << (12 - min(param & 0xF, 12)));
	value += *prev1 * adpcm_k0[filter] + *prev2 * adpcm_k1[filter];

	*prev2 = *prev1;
	*prev1 = value;

	result = (int)floorf(value + 0.5f);

	if (result > 32767)
		result = 32767;
	else if (result < -32768)
		result = -32768;

	return (short)result;
}

// [A]
// A sample up to the block that ends it (ADPCM_LOOP_END), and its loop in samples. Returns the
// samples written; loopLength is 0 for a sample that doesn't loop.
static int DecodeSample(const unsigned char *data, int size, short *out, int *loopStart, int *loopLength)
{
	float prev1;
	float prev2;
	int flags;
	int start;
	int count;
	int i;

	prev1 = 0.0f;
	prev2 = 0.0f;
	start = 0;
	count = 0;

	*loopStart = 0;
	*loopLength = 0;

	for (; size >= BLOCK_SIZE; size -= BLOCK_SIZE, data += BLOCK_SIZE)
	{
		flags = data[1];

		if (flags & ADPCM_LOOP_START)
			start = count;

		for (i = 0; i < BLOCK_SAMPLES; i++)
			out[count++] = VagToPcm(data[0], (data[2 + i / 2] >> ((i & 1) * 4)) & 0xF, &prev1, &prev2);

		if (flags & ADPCM_LOOP_END)
		{
			if (flags & ADPCM_REPEAT)
			{
				*loopStart = start;
				*loopLength = count - start;
			}

			break;
		}
	}

	return count;
}

// [A]
// A buffer per sample of the bank
static int LoadBank(const unsigned char *bank, int length)
{
	const unsigned char *data;
	short *pcm;
	int loopStart;
	int loopLength;
	int header;
	int count;
	int size;
	int i;

	spu_samples = *(const int *)bank;
	header = 4 + spu_samples * (int)sizeof(SAMPLE_INFO);

	if (spu_samples < 0 || spu_samples > MAX_SAMPLES || header > length)
	{
		spu_samples = 0;
		return 0;
	}

	memcpy(spu_info, bank + 4, spu_samples * sizeof(SAMPLE_INFO));
	data = bank + header;
	length -= header;

	pcm = (short *)malloc((length / BLOCK_SIZE) * BLOCK_SAMPLES * sizeof(short));

	if (pcm == NULL)
	{
		spu_samples = 0;
		return 0;
	}

	alGenBuffers(spu_samples, spu_buffers);

	for (i = 0; i < spu_samples; i++)
	{
		count = 0;

		if (spu_info[i].address < (unsigned int)length)
		{
			size = min((int)spu_info[i].length, length - (int)spu_info[i].address);
			count = DecodeSample(data + spu_info[i].address, size, pcm, &loopStart, &loopLength);
		}

		if (count == 0)
		{
			pcm[0] = 0;
			count = 1;
			loopLength = 0;
		}

		alBufferData(spu_buffers[i], AL_FORMAT_MONO16, pcm, count * sizeof(short), PSXMUSIC_RATE);

		// the loop of the whole sample needs no points
		if (loopLength > 0 && loopStart > 0)
		{
			int points[2] = { loopStart, loopStart + loopLength };
			alBufferiv(spu_buffers[i], AL_LOOP_POINTS_SOFT, points);
		}

		spu_info[i].loop = loopLength > 0;

		XM_SetVAGAddress(0, i, spu_info[i].address);
	}

	free(pcm);

	return 1;
}

// [A]
// Stops the voices and frees the song
static void FreeSong(void)
{
	int i;

	for (i = 0; i < SPU_VOICES; i++)
	{
		if (spu_voices[i].source)
		{
			alSourceStop(spu_voices[i].source);
			alSourcei(spu_voices[i].source, AL_BUFFER, 0);
		}

		spu_voices[i].paused = 0;
	}

	if (spu_samples)
		alDeleteBuffers(spu_samples, spu_buffers);

	free(music_xm);

	spu_samples = 0;
	music_xm = NULL;
	music_paused = 0;
}

// [A]
// Reads the song, builds its buffers and starts it from the beginning
static int LoadSong(int song)
{
	unsigned char *bank;
	FILE *fp;
	int xm_length;
	int bank_length;
	int ok;
	int i;

	FreeSong();

	if (spu_voices[0].source == 0)
		InitReverb();

	for (i = 0; i < SPU_VOICES; i++)
	{
		if (spu_voices[i].source == 0)
		{
			alGenSources(1, &spu_voices[i].source);
			alSourcei(spu_voices[i].source, AL_SOURCE_RELATIVE, AL_TRUE);
			alSourcef(spu_voices[i].source, AL_ROLLOFF_FACTOR, 0.0f);

			if (reverb_slot)
				alSource3i(spu_voices[i].source, AL_AUXILIARY_SEND_FILTER, reverb_slot, 0, AL_FILTER_NULL);
		}
	}

	xm_length = music_offsets[song * 2 + 1] - music_offsets[song * 2];
	bank_length = music_offsets[song * 2 + 2] - music_offsets[song * 2 + 1];

	ok = 0;
	bank = NULL;
	fp = fopen(MUSIC_FILE, "rb");

	if (fp && xm_length > 0 && bank_length > 0)
	{
		music_xm = (unsigned char *)malloc(xm_length);
		bank = (unsigned char *)malloc(bank_length);

		if (music_xm && bank && fseek(fp, music_offsets[song * 2], SEEK_SET) == 0 &&
			fread(music_xm, xm_length, 1, fp) == 1 && fread(bank, bank_length, 1, fp) == 1)
		{
			ok = LoadBank(bank, bank_length);
		}
	}

	if (fp)
		fclose(fp);

	free(bank);

	if (!ok)
	{
		FreeSong();
		return 0;
	}

	InitXMData(music_xm, 0, XM_UseXMPanning);

	// all the XM channels from the first voice, playing at once
	XM_Init(0, 0, 0, 0, XM_Loop, -1, XM_Music, 0);

	music_next = SDL_GetPerformanceCounter();

	return 1;
}

//------------------------------------------------------------------------------------------
// The interface for the game

// [A]
// Reads the table of MUSIC.BIN. Returns 0 when there is none.
int PSXMusic_Init(void)
{
	FILE *fp;
	int first;

	if (music_songs)
		return 1;

	fp = fopen(MUSIC_FILE, "rb");

	if (fp == NULL)
		return 0;

	// the table ends where the first song starts
	if (fread(&first, 4, 1, fp) == 1 && first >= 12 && first <= (int)sizeof(music_offsets) && (first & 3) == 0)
	{
		music_offsets[0] = first;

		if (fread(music_offsets + 1, first - 4, 1, fp) == 1)
			music_songs = (first / 4 - 1) / 2;
	}

	fclose(fp);

	if (music_songs == 0)
	{
		LogQuick("PSXMusic_Init: bad " MUSIC_FILE);
		return 0;
	}

	SDL_AtomicSet(&music_request, 0);
	SDL_AtomicSet(&music_setpos, -1);

	XM_OnceOffInit(XM_NTSC);

	music_header = calloc(1, XM_GetFileHeaderSize());
	music_xmsong = calloc(1, XM_GetSongSize());

	XM_SetFileHeaderAddress((u_char *)music_header);
	XM_SetSongAddress((u_char *)music_xmsong);

	return 1;
}

// [A]
// Asks the stream for a song, from its beginning; the number goes round the songs of the file.
// Returns 0 without MUSIC.BIN. The stream plays it once it runs PSXMusic_Tick.
int PSXMusic_Load(int song)
{
	int serial;

	if (music_songs == 0)
		return 0;

	song %= music_songs;

	if (song < 0)
		song += music_songs;

	music_song = song;
	music_copmusic = 0;
	SDL_AtomicSet(&music_setpos, -1);

	serial = (SDL_AtomicGet(&music_request) + 1) & 0xFFFF;
	SDL_AtomicSet(&music_request, song << 16 | serial);

	return 1;
}

// [A]
// 1: the cops chase the player, the song goes to its pursuit part (once, as in Driver 1: nothing
// brings it back). 0: the pursuit is over, which changes nothing here.
// NOTE: REDRIVER2 (FunkUpDaBGMTunez, gamesnd.c) for Driver 2, where 0 goes back to the start.
void FunkUpDaBGMTunez(int funk)
{
	if (music_song < 0)
		return;

	if (funk && !music_copmusic)
	{
		music_copmusic = 1;
		SDL_AtomicSet(&music_setpos, xm_coptrackpos[music_song % 8]);
	}
}

// [A]
// The ticker of the music stream (MusicStream_PlayTicker)
void PSXMusic_Tick(int tick, float volume)
{
	Uint64 period;
	Uint64 now;
	int request;
	int setpos;
	int i;

	if (tick == MUSIC_TICK_STOP)
	{
		FreeSong();

		for (i = 0; i < SPU_VOICES; i++)
		{
			if (spu_voices[i].source)
				alDeleteSources(1, &spu_voices[i].source);

			spu_voices[i].source = 0;
		}

		FreeReverb();

		music_loaded = -1;
		return;
	}

	request = SDL_AtomicGet(&music_request);

	if (request != music_loaded)
	{
		music_loaded = request;

		if (!LoadSong(request >> 16))
			LogQuick("PSXMusic_Tick: can't read " MUSIC_FILE);
	}

	if (spu_samples == 0)
		return;

	if (volume != music_volume)
	{
		music_volume = volume;

		for (i = 0; i < SPU_VOICES; i++)
			XMSpu_SetVoiceVolume(i, spu_voices[i].lvol, spu_voices[i].rvol);
	}

	if (tick == MUSIC_TICK_PAUSE)
	{
		if (!music_paused)
		{
			for (i = 0; i < SPU_VOICES; i++)
			{
				ALint state;
				alGetSourcei(spu_voices[i].source, AL_SOURCE_STATE, &state);

				spu_voices[i].paused = state == AL_PLAYING;

				if (spu_voices[i].paused)
					alSourcePause(spu_voices[i].source);
			}

			music_paused = 1;
		}

		return;
	}

	now = SDL_GetPerformanceCounter();
	period = SDL_GetPerformanceFrequency() / XM_UPDATE_RATE;

	if (music_paused)
	{
		for (i = 0; i < SPU_VOICES; i++)
		{
			if (spu_voices[i].paused)
				alSourcePlay(spu_voices[i].source);

			spu_voices[i].paused = 0;
		}

		music_paused = 0;
		music_next = now;
	}

	// catches up a few, not the time the thread stood still
	if (now > music_next + period * 6)
		music_next = now;

	while (now >= music_next)
	{
		setpos = SDL_AtomicSet(&music_setpos, -1);

		if (setpos != -1)
			XM_SetSongPos(0, (u_short)setpos);

		XM_Update();
		music_next += period;
	}
}

// [A]
// After the music stream: nothing ticks any more.
void PSXMusic_Shutdown(void)
{
	free(music_header);
	free(music_xmsong);

	music_header = NULL;
	music_xmsong = NULL;
	music_songs = 0;
	music_song = -1;
}

//------------------------------------------------------------------------------------------
// The SPU, only what the player uses (PsyX_SPUAL.cpp of PsyCross)

// [A]
void XMSpu_InitVoice(int channel)
{
	if (channel >= 0 && channel < SPU_VOICES && spu_voices[channel].source)
		alSourceStop(spu_voices[channel].source);
}

// [A]
// The sample, its pitch and volume for the next key on
void XMSpu_SetVoice(int channel, int addr, int pitch, int lvol, int rvol)
{
	if (channel < 0 || channel >= SPU_VOICES)
		return;

	spu_voices[channel].addr = addr;

	XMSpu_SetVoicePitch(channel, pitch);
	XMSpu_SetVoiceVolume(channel, lvol, rvol);
}

// [A]
// The volume as a gain and a pan, as PsyCross. A negative volume (the Dolby surround of the
// player) is the same here.
void XMSpu_SetVoiceVolume(int channel, int lvol, int rvol)
{
	XMSPU_VOICE *voice;
	float left;
	float right;
	float pan;

	if (channel < 0 || channel >= SPU_VOICES)
		return;

	voice = &spu_voices[channel];
	voice->lvol = lvol;
	voice->rvol = rvol;

	if (voice->source == 0)
		return;

	left = min((float)abs(lvol) / 16384.0f, 1.0f);
	right = min((float)abs(rvol) / 16384.0f, 1.0f);

	pan = (acosf(left) + asinf(right)) / 3.14159265f;	// [0, 1]
	pan = (2.0f * pan - 1.0f) * 0.5f;					// a +/- 30 degree arc

	alSource3f(voice->source, AL_POSITION, pan * STEREO_FACTOR, 0.0f, -sqrtf(1.0f - pan * pan));
	alSourcef(voice->source, AL_GAIN, (left + right) * 0.5f * music_volume);
}

// [A]
// Pitch 0 holds the voice where it is (XM_Pause)
void XMSpu_SetVoicePitch(int channel, int pitch)
{
	XMSPU_VOICE *voice;
	ALint state;

	if (channel < 0 || channel >= SPU_VOICES)
		return;

	voice = &spu_voices[channel];

	if (voice->source == 0)
		return;

	alGetSourcei(voice->source, AL_SOURCE_STATE, &state);

	if (pitch == 0 && state == AL_PLAYING)
		alSourcePause(voice->source);
	else if (pitch != 0 && voice->pitch == 0 && state == AL_PAUSED)
		alSourcePlay(voice->source);

	voice->pitch = pitch;

	if (pitch != 0)
		alSourcef(voice->source, AL_PITCH, (float)pitch / 4096.0f);
}

// [A]
// Key on: the sample whose bank holds the address, from that point of it
void XMSpu_SetKey(int on, unsigned int voices)
{
	XMSPU_VOICE *voice;
	unsigned int offset;
	int sample;
	int i;
	int j;

	for (i = 0; i < SPU_VOICES; i++)
	{
		if ((voices & (1u << i)) == 0)
			continue;

		voice = &spu_voices[i];

		if (voice->source == 0)
			continue;

		alSourceStop(voice->source);

		if (!on)
			continue;

		sample = -1;

		for (j = 0; j < spu_samples; j++)
		{
			if ((unsigned int)voice->addr >= spu_info[j].address &&
				(unsigned int)voice->addr < spu_info[j].address + spu_info[j].length)
			{
				sample = j;
				break;
			}
		}

		if (sample < 0)
			continue;

		offset = ((unsigned int)voice->addr - spu_info[sample].address) / BLOCK_SIZE * BLOCK_SAMPLES;

		alSourcei(voice->source, AL_BUFFER, spu_buffers[sample]);
		alSourcei(voice->source, AL_LOOPING, spu_info[sample].loop ? AL_TRUE : AL_FALSE);
		alSourcei(voice->source, AL_SAMPLE_OFFSET, offset);

		if (voice->pitch != 0)
			alSourcePlay(voice->source);
	}
}

// [A]
// As SpuGetAllKeysStatus: 1 while the voice plays (or is held by pitch 0), 0 once it has stopped
void XMSpu_GetAllKeysStatus(char *status)
{
	ALint state;
	int i;

	for (i = 0; i < SPU_VOICES; i++)
	{
		status[i] = 0;

		if (spu_voices[i].source)
		{
			alGetSourcei(spu_voices[i].source, AL_SOURCE_STATE, &state);
			status[i] = state == AL_PLAYING || state == AL_PAUSED;
		}
	}
}
