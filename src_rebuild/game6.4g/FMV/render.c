/*
 * FMV/render.c - the full motion videos
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "FMV\render.c", 0x48aeb0 - 0x48bc7b).
 *
 * PlayRender shuts the game's drawing and sound down and plays FMVDATA\RENDERnn.MPG through
 * DirectShow, optionally followed by an answerphone message (AUDIO\MOTEL\ANSnnn.WAV). The video
 * goes through a multimedia stream that decodes each frame to an offscreen surface; StepVideo
 * blits that to the primary surface at double height. The display is 640x480, 32 bpp and then
 * 16 bpp if that fails. Escape skips the video and the message.
 */

#define COBJMACROS

#include <stdio.h>
#include <string.h>

#include <windows.h>
#include "dr_keys.h"
#include <ddraw.h>
#include <mmstream.h>
#include <amstream.h>
#include <ddstream.h>
#include <control.h>
#include <uuids.h>

#include "dr_types.h"
#include "Engine/3dfx/gfxinit.h"
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

static DDSURFACEDESC screen_desc;		// 0xb9b770
char render_filename[260];				// 0xb9b7dc
char render_filenameondisk[260];		// 0xb9b8e0
static DDSURFACEDESC video_desc;		// 0xb9b9e8

// NOTE: DDraw and DDraw4 have no symbol prefix to tell; only this file uses them
static LPDIRECTDRAW DDraw;				// 0xb9ba54
static LPDIRECTDRAW4 DDraw4;			// 0xb9ba58
LPDIRECTDRAWSURFACE video_surface;		// 0xb9ba5c
static LPDIRECTDRAWSURFACE frame_surface;	// 0xb9ba60
LPDIRECTDRAWCLIPPER video_clipper;		// 0xb9ba64

static IMultiMediaStream *video_stream;				// 0xb9ba68
static IAMMultiMediaStream *video_amstream;			// 0xb9ba6c
static IMediaStream *video_mediastream;				// 0xb9ba70
static IDirectDrawMediaStream *video_ddstream;		// 0xb9ba74
static IDirectDrawStreamSample *video_sample;		// 0xb9ba78

static IMultiMediaStream *sound_stream;				// 0xb9ba7c
static IAMMultiMediaStream *sound_amstream;			// 0xb9ba80
static IGraphBuilder *sound_graph;					// 0xb9ba84
static IMediaPosition *sound_position;				// 0xb9ba88
static double sound_duration;						// 0xb9ba90

static int render_skipped;				// 0xb9ba98

int PlayRenderSound(const char *filename);
void StopRenderSound(void);

static int StartupVideoSystem(HWND hWnd, DWORD bpp);
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
// NOTE: [A] nothing with /GL: the video is DirectShow on DirectDraw until the port gets to it
void PlayRender(HWND hWnd, int render, int sound)
{
	FILE *fp;
	int i;
	int inited;
	char filename[260];

	if (gUseGL)
		return;

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

			// NOTE: [A] windowed mode doesn't maximise the window
			if (gScreen.fullscreen && !gWindowed)
			{
				ShowWindow(hWnd, SW_MAXIMIZE);
				UpdateWindow(hWnd);
			}

			ShutdownDrawingSystem();
			ShutdownSoundSystem();

			render_skipped = 0;

			if (StartupVideoSystem(hWnd, 32))
			{
				inited = InitVideo(render_filename);

				if (inited == 0)
				{
					LogQuick("Failed to initialise FMV in 32bpp, trying 16bpp");

					ShutdownVideoSystem();

					if (StartupVideoSystem(hWnd, 16))
					{
						inited = InitVideo(render_filename);

						if (inited == 0)
							LogQuick("Failed to initialise FMV in 16bpp, aborting...");
					}
				}

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

			SetForegroundWindow(hWnd);
			HandleWinMessages();

			for (i = 10; i > 0 && InitDrawingSystem(gScreen.handle, gScreen.fullscreen, 0) == 0; i--)
			{
				Sleep(500);
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

// [0x48b198] [D]
static int StartupVideoSystem(HWND hWnd, DWORD bpp)
{
	HRESULT hr;
	DDSURFACEDESC desc;

	if (DirectDrawCreate(device_guid, &DDraw, NULL) != DD_OK)
		return 0;

	if (IDirectDraw_QueryInterface(DDraw, &IID_IDirectDraw4, (void **)&DDraw4) != DD_OK)
		return 0;

	if (gScreen.fullscreen)
		hr = IDirectDraw4_SetCooperativeLevel(DDraw4, hWnd, DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN);
	else
		hr = IDirectDraw4_SetCooperativeLevel(DDraw4, hWnd, DDSCL_NORMAL);

	if (hr != DD_OK)
		return 0;

	if (gScreen.fullscreen && IDirectDraw4_SetDisplayMode(DDraw4, 640, 480, bpp, 0, 0) != DD_OK)
		return 0;

	memset(&desc, 0, sizeof(desc));

	desc.dwSize = sizeof(desc);
	desc.dwFlags = DDSD_CAPS;
	desc.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;

	if (IDirectDraw_CreateSurface(DDraw, &desc, &video_surface, NULL) != DD_OK)
		return 0;

	if (IDirectDraw4_CreateClipper(DDraw4, 0, &video_clipper, NULL) != DD_OK)
		return 0;

	if (IDirectDrawSurface_SetClipper(video_surface, video_clipper) != DD_OK)
		return 0;

	if (IDirectDrawClipper_SetHWnd(video_clipper, 0, hWnd) != DD_OK)
		return 0;

	return 1;
}

// [0x48b33b] [D]
static void ShutdownVideoSystem(void)
{
	DDBLTFX fx;

	if (frame_surface != NULL)
	{
		IDirectDrawSurface_Release(frame_surface);
		frame_surface = NULL;
	}

	if (video_clipper != NULL)
	{
		IDirectDrawClipper_Release(video_clipper);
		video_clipper = NULL;
	}

	if (video_surface != NULL)
	{
		// clear the screen to black
		memset(&fx, 0, sizeof(fx));

		fx.dwSize = sizeof(fx);
		fx.dwFillColor = 0;

		IDirectDrawSurface_Blt(video_surface, NULL, NULL, NULL, DDBLT_COLORFILL | DDBLT_WAIT, &fx);

		IDirectDrawSurface_Release(video_surface);
		video_surface = NULL;
	}

	if (DDraw4 != NULL)
	{
		IDirectDraw4_SetCooperativeLevel(DDraw4, NULL, DDSCL_NORMAL);

		IDirectDraw4_Release(DDraw4);
		DDraw4 = NULL;
	}

	if (DDraw != NULL)
	{
		IDirectDraw_Release(DDraw);
		DDraw = NULL;
	}
}

// [0x48b44c] [D]
// NOTE: name is ours, the pair of OpenVideo. Called directly, so static
static int OpenSound(const char *filename)
{
	WCHAR name[260];

	MultiByteToWideChar(CP_ACP, 0, filename, -1, name, 260);

	if (CoCreateInstance(&CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, &IID_IGraphBuilder, (void **)&sound_graph) != S_OK)
		return 0;

	if (IGraphBuilder_QueryInterface(sound_graph, &IID_IMediaPosition, (void **)&sound_position) != S_OK)
		return 0;

	if (CoCreateInstance(&CLSID_AMMultiMediaStream, NULL, CLSCTX_INPROC_SERVER, &IID_IAMMultiMediaStream, (void **)&sound_amstream) != S_OK)
		return 0;

	if (IAMMultiMediaStream_Initialize(sound_amstream, STREAMTYPE_READ, AMMSF_NOGRAPHTHREAD, sound_graph) != S_OK)
		return 0;

	if (IAMMultiMediaStream_AddMediaStream(sound_amstream, NULL, &MSPID_PrimaryAudio, AMMSF_ADDDEFAULTRENDERER, NULL) != S_OK)
		return 0;

	if (IAMMultiMediaStream_OpenFile(sound_amstream, name, 0) != S_OK)
		return 0;

	sound_stream = (IMultiMediaStream *)sound_amstream;

	if (IMediaPosition_get_Duration(sound_position, &sound_duration) != S_OK)
		return 0;

	return 1;
}

// [0x48b5cc] [D]
// NOTE: name is ours, the pair of CloseVideo. Called directly, so static
static void CloseSound(void)
{
	if (sound_amstream != NULL)
	{
		IAMMultiMediaStream_Release(sound_amstream);
		sound_amstream = NULL;
		sound_stream = NULL;
	}

	if (sound_position != NULL)
	{
		IMediaPosition_Release(sound_position);
		sound_position = NULL;
	}

	if (sound_graph != NULL)
	{
		IGraphBuilder_Release(sound_graph);
		sound_graph = NULL;
	}
}

// [0x48b647] [D]
// Starts the sound, and a timer that goes off a second after its end.
static int CreateVideoTimer(void)
{
	if (sound_stream == NULL || IMultiMediaStream_SetState(sound_stream, STREAMSTATE_RUN) != S_OK)
		return 0;

	render_sound_done = 0;

	video_timer = CreateTimer((int)((sound_duration + 1.0) * 1000.0), 0, VideoTimer);

	return video_timer != -1;
}

// [0x48b6c1] [D]
static void DestroyVideoTimer(void)
{
	if (sound_stream != NULL)
		IMultiMediaStream_SetState(sound_stream, STREAMSTATE_STOP);

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

// [0x48b7a5] [D]
static int OpenVideo(const char *filename)
{
	DDSURFACEDESC desc;
	RECT rect;
	WCHAR name[260];

	if (CoCreateInstance(&CLSID_AMMultiMediaStream, NULL, CLSCTX_INPROC_SERVER, &IID_IAMMultiMediaStream, (void **)&video_amstream) != S_OK)
		return 0;

	MultiByteToWideChar(CP_ACP, 0, filename, -1, name, 260);

	if (IAMMultiMediaStream_Initialize(video_amstream, STREAMTYPE_READ, AMMSF_NOGRAPHTHREAD, NULL) != S_OK)
		return 0;

	if (IAMMultiMediaStream_AddMediaStream(video_amstream, (IUnknown *)DDraw, &MSPID_PrimaryVideo, 0, NULL) != S_OK)
		return 0;

	if (IAMMultiMediaStream_AddMediaStream(video_amstream, NULL, &MSPID_PrimaryAudio, AMMSF_ADDDEFAULTRENDERER, NULL) != S_OK)
		LogQuick("OpenVideo: Audio initialisation error");

	if (IAMMultiMediaStream_OpenFile(video_amstream, name, 0) != S_OK)
		return 0;

	video_stream = (IMultiMediaStream *)video_amstream;

	if (IMultiMediaStream_GetMediaStream(video_stream, &MSPID_PrimaryVideo, &video_mediastream) != S_OK)
		return 0;

	if (IMediaStream_QueryInterface(video_mediastream, &IID_IDirectDrawMediaStream, (void **)&video_ddstream) != S_OK)
		return 0;

	memset(&video_desc, 0, sizeof(video_desc));
	video_desc.dwSize = sizeof(video_desc);

	if (IDirectDrawMediaStream_GetFormat(video_ddstream, &video_desc, NULL, NULL, NULL) != S_OK)
		return 0;

	memset(&screen_desc, 0, sizeof(screen_desc));
	screen_desc.dwSize = sizeof(screen_desc);

	if (IDirectDrawSurface_GetSurfaceDesc(video_surface, &screen_desc) != DD_OK)
		return 0;

	// the frames are decoded to an offscreen surface in the pixel format of the screen
	memset(&desc, 0, sizeof(desc));

	desc.dwSize = sizeof(desc);
	desc.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT;
	desc.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN;
	desc.dwWidth = video_desc.dwWidth;
	desc.dwHeight = video_desc.dwHeight;
	desc.ddpfPixelFormat = screen_desc.ddpfPixelFormat;
	desc.ddpfPixelFormat.dwFlags &= ~DDPF_ALPHAPIXELS;

	if (IDirectDraw_CreateSurface(DDraw, &desc, &frame_surface, NULL) != DD_OK)
		return 0;

	rect.left = 0;
	rect.top = 0;
	rect.right = video_desc.dwWidth;
	rect.bottom = video_desc.dwHeight;

	if (IDirectDrawMediaStream_CreateSample(video_ddstream, frame_surface, &rect, 0, &video_sample) != S_OK)
		return 0;

	return 1;
}

// [0x48bad5] [D]
static void CloseVideo(void)
{
	if (video_sample != NULL)
	{
		IDirectDrawStreamSample_Release(video_sample);
		video_sample = NULL;
	}

	if (video_ddstream != NULL)
	{
		IDirectDrawMediaStream_Release(video_ddstream);
		video_ddstream = NULL;
	}

	if (video_mediastream != NULL)
	{
		IMediaStream_Release(video_mediastream);
		video_mediastream = NULL;
	}

	if (video_amstream != NULL)
	{
		if (video_stream != NULL)
		{
			IMultiMediaStream_Release(video_stream);
			video_stream = NULL;
		}
		else
		{
			IAMMultiMediaStream_Release(video_amstream);
		}

		video_amstream = NULL;
	}
}

// [0x48bb90] [D]
static int ValidVideo(void)
{
	return video_stream != NULL && IMultiMediaStream_SetState(video_stream, STREAMSTATE_RUN) == S_OK;
}

// [0x48bbbe] [D]
static void FreeVideo(void)
{
	if (video_stream != NULL)
		IMultiMediaStream_SetState(video_stream, STREAMSTATE_STOP);
}

// [0x48bbdf] [D]
static int StepVideo(void)
{
	RECT rect;

	if (video_sample == NULL)
		return 0;

	if (IDirectDrawStreamSample_Update(video_sample, 0, NULL, NULL, 0) != S_OK)
		return 0;

	rect.left = 0;
	rect.top = (480 - 2 * video_desc.dwHeight) >> 1;
	rect.right = 640;
	rect.bottom = rect.top + 2 * video_desc.dwHeight;

	return IDirectDrawSurface_Blt(video_surface, &rect, frame_surface, NULL, DDBLT_WAIT, NULL) == DD_OK;
}
