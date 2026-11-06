/*
 * FMV/render.c - the full motion videos
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "FMV\render.c", 0x48aeb0 - 0x48bc7b).
 *
 * PlayRender shuts the game's drawing and sound down and plays FMVDATA\RENDERnn.MPG, optionally
 * followed by an answerphone message (AUDIO\MOTEL\ANSnnn.WAV). Escape skips the video and the
 * message.
 *
 * [A] The original played them through DirectShow, on a multimedia stream that decoded each frame
 * to an offscreen surface and blitted that to the primary surface at double height. The same steps
 * run on FMV/mpeg.c now.
 */

#include <stdio.h>
#include <string.h>

#include <SDL.h>

#include "dr_keys.h"
#include "dr_types.h"
#include "Engine/3dfx/gfxinit.h"
#include "FMV/mpeg.h"
#include "FMV/render.h"
#include "debug.h"
#include "dr_globals.h"
#include "dr_time.h"
#include "file.h"
#include "input.h"
#include "shell.h"
#include "sound.h"

// NOTE: all the names here without a symbol are ours (the IDA ones are render_dword_B9BAxx)
int video_timer = -1;					// 0x54c7ac
static int render_sound_done = 1;		// 0x54c7b0

char render_filename[260];				// 0xb9b7dc
char render_filenameondisk[260];		// 0xb9b8e0

static double sound_duration;						// 0xb9ba90

static int render_skipped;				// 0xb9ba98

int PlayRenderSound(const char *filename);
void StopRenderSound(void);

static int StartupVideoSystem(void);
static void ShutdownVideoSystem(void);
static int OpenSound(const char *filename);
static void CloseSound(void);
static int CreateVideoTimer(void);
static void DestroyVideoTimer(void);
static void VideoTimer(void);
static int InitVideo(const char *filename);
static int OpenVideo(const char *filename);
static void CloseVideo(void);
static int ValidVideo(void);
static void FreeVideo(void);
static int StepVideo(void);

// [0x48aeb0] [D] [A]
// NOTE: [A] no window handle. The OpenGL window is full screen or not on its own, nothing to
// maximise, and no second try at 16 bpp: the display mode doesn't change for the video
void PlayRender(int render, int sound)
{
	FILE *fp;
	int i;
	int inited;
	char filename[260];

	if (render != -1 && gAllowFMV && !gNoFMV)
	{
		sprintf(filename, "FMVDATA\\RENDER%02d.MPG", render);
		strcpy(render_filename, filename);

		fp = fopen(filename, "rb");

		if (fp == NULL && FileFindOnDisk(filename, render_filenameondisk))
		{
			sprintf(render_filename, "%s%s", render_filenameondisk, filename);
			fp = fopen(render_filename, "rb");
		}

		if (fp != NULL)
		{
			fclose(fp);

			ShutdownDrawingSystem();
			ShutdownSoundSystem();

			render_skipped = 0;

			if (StartupVideoSystem())
			{
				inited = InitVideo(render_filename);

				if (inited == 0)
					LogQuick("Failed to initialise FMV, aborting...");

				if (inited && render_skipped == 0 && sound != -1)
				{
					sprintf(filename, "AUDIO\\MOTEL\\ANS%03d.WAV", sound);

					PlayRenderSound(filename);

					while (render_sound_done == 0)
					{
						HandleWinMessages();
						ReadInput();

						if (CheckKey(DIK_ESCAPE) || ControllerPressed)
						{
							render_skipped = 1;
							break;
						}
					}

					StopRenderSound();
				}

				ShutdownVideoSystem();
			}

			SDL_RaiseWindow(gWindow);
			HandleWinMessages();

			for (i = 10; i > 0 && InitDrawingSystem(gScreen.fullscreen, 0) == 0; i--)
			{
				SDL_Delay(500);
				HandleWinMessages();
			}

			if (i == 0)
				DisplayErrorAndExit("Can't reallocate display hardware");

			InitSoundSystem(gSoundHardware, gSoundCompatibility, gUseEAX);
		}
	}
}

// [0x48b158] [D]
int PlayRenderSound(const char *filename)
{
	StopRenderSound();

	if (OpenSound(filename) && CreateVideoTimer())
		return 1;

	StopRenderSound();
	return 0;
}

// [0x48b189] [D]
// NOTE: name is ours; it has a thunk, so it is not static in the original
void StopRenderSound(void)
{
	DestroyVideoTimer();
	CloseSound();
}

// [0x48b198] [D] [A]
// NOTE: [A] FMV/mpeg.c
static int StartupVideoSystem(void)
{
	return MpegStartup();
}

// [0x48b33b] [D] [A]
// NOTE: [A] FMV/mpeg.c
static void ShutdownVideoSystem(void)
{
	MpegShutdown();
}

// [0x48b44c] [D] [A]
// NOTE: name is ours, the pair of OpenVideo. Called directly, so static
// NOTE: [A] FMV/mpeg.c
static int OpenSound(const char *filename)
{
	return MpegOpenSound(filename, &sound_duration);
}

// [0x48b5cc] [D] [A]
// NOTE: name is ours, the pair of CloseVideo. Called directly, so static
// NOTE: [A] FMV/mpeg.c
static void CloseSound(void)
{
	MpegCloseSound();
}

// [0x48b647] [D] [A]
// Starts the sound, and a timer that goes off a second after its end.
// NOTE: [A] FMV/mpeg.c
static int CreateVideoTimer(void)
{
	if (!MpegPlaySound())
		return 0;

	render_sound_done = 0;

	video_timer = CreateTimer((int)((sound_duration + 1.0) * 1000.0), 0, VideoTimer);

	return video_timer != -1;
}

// [0x48b6c1] [D] [A]
// NOTE: [A] FMV/mpeg.c
static void DestroyVideoTimer(void)
{
	MpegStopSound();

	if (video_timer != -1)
	{
		DestroyTimer(video_timer);
		video_timer = -1;
	}

	render_sound_done = 1;
}

// [0x48b70d] [D]
static void VideoTimer(void)
{
	render_sound_done = 1;
}

// [0x48b71c] [D] [A]
// NOTE: [A] any controller button skips the video, not just Escape
static int InitVideo(const char *filename)
{
	int result;
	int oldInPresentation;

	result = 0;

	if (OpenVideo(filename))
	{
		if (ValidVideo())
		{
			result = 1;

			oldInPresentation = gInPresentation;
			gInPresentation = 1;

			while (StepVideo())
			{
				HandleWinMessages();
				ReadInput();

				if (CheckKey(DIK_ESCAPE) || ControllerPressed)
				{
					render_skipped = 1;
					break;
				}
			}

			gInPresentation = oldInPresentation;
		}

		FreeVideo();
	}

	CloseVideo();

	return result;
}

// [0x48b7a5] [D] [A]
// NOTE: [A] FMV/mpeg.c
static int OpenVideo(const char *filename)
{
	return MpegOpen(filename);
}

// [0x48bad5] [D] [A]
// NOTE: [A] FMV/mpeg.c
static void CloseVideo(void)
{
	MpegClose();
}

// [0x48bb90] [D] [A]
// NOTE: [A] FMV/mpeg.c needs no start
static int ValidVideo(void)
{
	return 1;
}

// [0x48bbbe] [D] [A]
// NOTE: [A] FMV/mpeg.c has nothing to stop before MpegClose
static void FreeVideo(void)
{
}

// [0x48bbdf] [D] [A]
// NOTE: [A] FMV/mpeg.c
static int StepVideo(void)
{
	return MpegStep();
}
