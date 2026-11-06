/*
 * FMV/mpeg.c - [A] the videos: pl_mpeg for the picture and the sound of
 * FMVDATA\RENDERnn.MPG, OpenAL for the sound and the answerphone messages
 *
 * Not in the original, it stands in for DirectShow and DirectDraw of FMV/render.c. PlayRender has
 * shut the game's drawing and sound down by now, so there is a device of its own for each.
 * The OpenAL one goes to the default device and its context is current on this thread only: the
 * sound system makes its own current again later, and the music keeps its own all along.
 *
 * pl_mpeg runs on the game thread between the frames, which is safe for the FPU mode: it is back to
 * normal since ShutdownDrawingSystem.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AL_ALEXT_PROTOTYPES
#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>

#include <SDL.h>

#define PL_MPEG_IMPLEMENTATION
#include "pl_mpeg/pl_mpeg.h"

#include "D3DGlide/glide_gl.h"
#include "FMV/mpeg.h"

// Decoded this far ahead of the picture. 1152 samples of 32 to 48 kHz are 24 to 36 ms, so there are
// buffers for three times that much.
#define AUDIO_LEAD_TIME		0.2
#define NUM_AUDIO_BUFFERS	24
#define MAX_SAMPLE_RATE		48000

// the picture stands still for longer while the window is dragged, it carries on from there
#define MAX_STEP_TIME		0.1

static ALCdevice *al_device;
static ALCcontext *al_context;
static ALuint al_source;
static ALuint al_buffers[NUM_AUDIO_BUFFERS];
static ALuint al_free[NUM_AUDIO_BUFFERS];
static int al_num_free;
static ALuint al_sound_buffer;

static plm_t *mpeg;
static unsigned char *mpeg_frame;
static int mpeg_new_frame;
static int mpeg_rate;
static Uint64 mpeg_last_time;

static float silence[2 * (int)(MAX_SAMPLE_RATE * AUDIO_LEAD_TIME)];

//------------------------------------------------------------------------------------------
// Devices

static void OpenAudio(void)
{
	al_device = alcOpenDevice(NULL);

	if (al_device == NULL)
		return;

	al_context = alcCreateContext(al_device, NULL);

	if (al_context == NULL)
	{
		alcCloseDevice(al_device);
		al_device = NULL;
		return;
	}

	alcSetThreadContext(al_context);

	alGenSources(1, &al_source);
	alGenBuffers(NUM_AUDIO_BUFFERS, al_buffers);

	// plays as it is, as the music does
	alSourcei(al_source, AL_SOURCE_RELATIVE, AL_TRUE);
	alSource3f(al_source, AL_POSITION, 0.0f, 0.0f, 0.0f);
	alSourcef(al_source, AL_ROLLOFF_FACTOR, 0.0f);
	alSourcei(al_source, AL_DIRECT_CHANNELS_SOFT, AL_TRUE);
}

static void CloseAudio(void)
{
	if (al_context == NULL)
		return;

	alDeleteSources(1, &al_source);
	alDeleteBuffers(NUM_AUDIO_BUFFERS, al_buffers);

	alcSetThreadContext(NULL);
	alcDestroyContext(al_context);
	alcCloseDevice(al_device);

	al_context = NULL;
	al_device = NULL;
}

int MpegStartup(void)
{
	if (!gl_StartVideo())
		return 0;

	OpenAudio();

	return 1;
}

void MpegShutdown(void)
{
	CloseAudio();
	gl_StopVideo();
}

//------------------------------------------------------------------------------------------
// Video

static void DecodedVideo(plm_t *plm, plm_frame_t *frame, void *user)
{
	plm_frame_to_bgra(frame, mpeg_frame, frame->width * 4);
	mpeg_new_frame = 1;
}

static void QueueAudio(const void *data, int size)
{
	ALuint buffer;

	if (al_num_free == 0)
		return;

	buffer = al_free[--al_num_free];

	alBufferData(buffer, AL_FORMAT_STEREO_FLOAT32, data, size, mpeg_rate);
	alSourceQueueBuffers(al_source, 1, &buffer);
}

static void DecodedAudio(plm_t *plm, plm_samples_t *samples, void *user)
{
	QueueAudio(samples->interleaved, samples->count * 2 * sizeof(float));
}

static void ReclaimAudioBuffers(void)
{
	ALint processed;
	ALuint buffer;

	alGetSourcei(al_source, AL_BUFFERS_PROCESSED, &processed);

	while (processed-- > 0)
	{
		alSourceUnqueueBuffers(al_source, 1, &buffer);
		al_free[al_num_free++] = buffer;
	}
}

int MpegOpen(const char *filename)
{
	int width;
	int height;
	int i;

	mpeg = plm_create_with_filename(filename);

	if (mpeg == NULL)
		return 0;

	if (!plm_has_headers(mpeg) || plm_get_num_video_streams(mpeg) == 0)
		return 0;

	width = plm_get_width(mpeg);
	height = plm_get_height(mpeg);

	mpeg_frame = malloc(width * height * 4);

	if (mpeg_frame == NULL || !gl_SetVideoSize(width, height))
		return 0;

	plm_set_video_decode_callback(mpeg, DecodedVideo, NULL);

	mpeg_rate = plm_get_samplerate(mpeg);

	if (al_context && plm_get_num_audio_streams(mpeg) != 0 && mpeg_rate <= MAX_SAMPLE_RATE)
	{
		plm_set_audio_decode_callback(mpeg, DecodedAudio, NULL);
		plm_set_audio_lead_time(mpeg, AUDIO_LEAD_TIME);

		for (i = 0; i < NUM_AUDIO_BUFFERS; i++)
			al_free[i] = al_buffers[i];

		al_num_free = NUM_AUDIO_BUFFERS;
	}
	else
	{
		plm_set_audio_enabled(mpeg, 0);
	}

	mpeg_new_frame = 0;
	mpeg_last_time = SDL_GetPerformanceCounter();

	return 1;
}

void MpegClose(void)
{
	if (mpeg != NULL)
	{
		if (plm_get_audio_enabled(mpeg))
		{
			alSourceStop(al_source);
			alSourcei(al_source, AL_BUFFER, 0);
		}

		plm_destroy(mpeg);
		mpeg = NULL;
	}

	free(mpeg_frame);
	mpeg_frame = NULL;
}

// One go round the loop of InitVideo. Returns 0 when the video has ended.
int MpegStep(void)
{
	Uint64 now;
	double elapsed;
	ALint state;
	int audio;

	audio = plm_get_audio_enabled(mpeg);
	state = AL_STOPPED;

	if (audio)
	{
		ReclaimAudioBuffers();
		alGetSourcei(al_source, AL_SOURCE_STATE, &state);
	}

	if (plm_has_ended(mpeg))
	{
		// what was decoded ahead still has to play
		if (state == AL_PLAYING)
		{
			SDL_Delay(1);
			return 1;
		}

		return 0;
	}

	// The queue ran dry. What is decoded next is ahead of the picture by the lead time, so that
	// much silence keeps them together.
	if (audio && state == AL_STOPPED)
		QueueAudio(silence, (int)(mpeg_rate * AUDIO_LEAD_TIME) * 2 * sizeof(float));

	now = SDL_GetPerformanceCounter();
	elapsed = (double)(now - mpeg_last_time) / (double)SDL_GetPerformanceFrequency();
	mpeg_last_time = now;

	if (elapsed > MAX_STEP_TIME)
		elapsed = MAX_STEP_TIME;

	plm_decode(mpeg, elapsed);

	if (audio)
	{
		alGetSourcei(al_source, AL_SOURCE_STATE, &state);

		if (state != AL_PLAYING)
			alSourcePlay(al_source);
	}

	if (mpeg_new_frame)
	{
		mpeg_new_frame = 0;
		gl_ShowVideoFrame(mpeg_frame);
	}
	else
	{
		SDL_Delay(1);
	}

	return 1;
}

//------------------------------------------------------------------------------------------
// The answerphone message

int MpegOpenSound(const char *filename, double *duration)
{
	SDL_AudioSpec spec;
	Uint8 *data;
	Uint32 size;
	ALenum format;

	if (al_context == NULL || SDL_LoadWAV(filename, &spec, &data, &size) == NULL)
		return 0;

	if (spec.format == AUDIO_U8)
		format = spec.channels == 2 ? AL_FORMAT_STEREO8 : AL_FORMAT_MONO8;
	else if (spec.format == AUDIO_S16LSB)
		format = spec.channels == 2 ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16;
	else
		format = AL_NONE;

	if (format != AL_NONE && spec.channels <= 2)
	{
		alGenBuffers(1, &al_sound_buffer);
		alBufferData(al_sound_buffer, format, data, size, spec.freq);
		alSourcei(al_source, AL_BUFFER, al_sound_buffer);

		*duration = (double)size / (spec.freq * spec.channels * (SDL_AUDIO_BITSIZE(spec.format) / 8));
	}

	SDL_FreeWAV(data);

	return al_sound_buffer != 0;
}

void MpegCloseSound(void)
{
	if (al_sound_buffer != 0)
	{
		alSourceStop(al_source);
		alSourcei(al_source, AL_BUFFER, 0);
		alDeleteBuffers(1, &al_sound_buffer);
		al_sound_buffer = 0;
	}
}

int MpegPlaySound(void)
{
	if (al_sound_buffer == 0)
		return 0;

	alSourcePlay(al_source);

	return 1;
}

void MpegStopSound(void)
{
	if (al_sound_buffer != 0)
		alSourceStop(al_source);
}
