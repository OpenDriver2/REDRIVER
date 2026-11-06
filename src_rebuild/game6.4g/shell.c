/*
 * shell.c - shell: main, SDL window, event pump, global game flags
 *
 * Rebuilt from Driver_sym2_94.i64 (IDA folder "shell.c", 0x5242f0 - 0x524c62)
 *
 * NOTE: the .data block also holds unreferenced command line switch strings
 * ("/DOREP:", "/NOPLUG", "/RES640", "/NOPEDS", "/FLAT", ...) between DisplayErrorAndExit's and
 * InitWindow's literals: a command line parser stood there and was dropped by the linker
 * (nothing referenced it). It is not reconstructed.
 */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL.h>
#include <SDL_syswm.h>

// Glide is reached through function pointers, so only the types and GR_* constants are taken
// from glide.h; grSstControl() below is the engine wrapper from D3DGlide/system.c
#define FX_GLIDE_NO_FUNC_PROTO
#include <glide.h>

#include "dr_types.h"
#include "Director.h"
#include "dr_globals.h"
#include "frontend/config.h"
#include "pres.h"
#include "shell.h"
#include "input.h"
#include "ai.h"
#include "chairs.h"
#include "debris.h"
#include "D3DGlide/glide_gl.h"
#include "D3DGlide/system.h"
#include "debug.h"
#include "DirectorStorage.h"
#include "dr_time.h"
#include "Engine/3dfx/gfxinit.h"
#include "flat/flat.h"
#include "frontend/frontend.h"
#include "frontend/main.h"
#include "general.h"
#include "glaunch.h"
#include "replays.h"
#include "sound.h"
#include "MScript.h"
#include "dr_scores.h"
#include "frontend/cheats.h"
#include "frontend/gvars.h"

#define EXIT_LEAD_CAR_OUT_OF_RANGE	201

void HandleWinMessages(void);
void __declspec(noreturn) DisplayErrorAndExit(const char *text);
void Init_FileSystem(void);
void Shutdown_FileSystem(void);
void InitScreen(void);
int InitWindow(void);
void SetWindowActive(int active);
static void ToggleFullscreen(void);
void CloseGame(void);
int InitDisplay(int window_mode);
void InterruptBegin(void);
void InterruptEnd(void);
LONG WINAPI UpdateInterruptState(EXCEPTION_POINTERS *exceptionInfo);
void SetGameWindowSize(int width, int height);
int ShowMessageBox(const char *text, const char *caption, int type);

// NOTE: [A] the original title is "Driver V1.3"
#define WINDOW_TITLE		"Driver V1.3 (OpenDriverPC)"

//------------------------------------------------------------------------------------------
// initialised data (0x578240)

int gCopCarWipedOutDamage = 7000;
int gTrafficDensity = 32;
int DrawSubmergedObjects = 1;
int max_frame_skip = 1;
int gNumCitiesOpen = 1;
int gRealTimeLights = 1;
int gDetailLevel = 4;
int gTrainingMode = 2;
int gAudioMixerBits = 8;
int gAudioMixerFreq = 22050;
int gAllowCopSpeech = 1;
int gRefreshRate = 50;
int gUnknown_578278 = 2;		// NOTE: no references, original name unknown
float gMasterVolume = 1.0f;
float gMusicVolume = 1.0f;
int do_plug = 1;
int gDoDetailedShadows = 1;
int gAutoBrake = 1;
int gShowMouse = 1;
int gAutoPriority = 1;
int gDoLensFlare = 1;
int gDoInterpolation = 1;
int gDoAutoInterpolation = 1;
int gFogAllowed = 1;
int gAllowSounds = 1;
int gDoMultiColouredCars = 1;
int gAllowFMV = 1;
int gAllowCheatKeys = 1;
int gUseEAX = 1;
int gUseCopModel = 1;

//------------------------------------------------------------------------------------------
// uninitialised data (0xc06fc4)
// NOTE: this is the linker's common bss area, which mixes globals from many files (e.g. general.c's
// mem_allocated sits next to sound globals at 0xb9f6xx), so being between shell.c's statics does
// not prove these globals are shell.c's. They are defined here provisionally (none has an IDA
// @file mark); move them when their owner is found.

static char errorMessage[256];
static int unknown_C070C8;		// NOTE: no references
char ghost_mode;
int gDodgyBrakeFactor;
int gDodgySpeedFactor;
int quit;
int exit_code;
static int unknown_C070E0;		// NOTE: no references
int gDifficulty;
int GameLevel;
int MissionNumber;
int GameMode;
int gCopRespawnTime;
int TrailSpacing;
static int unknown_C070FC;		// NOTE: no references
int gBitchingMode;
int gNightBrightness;
int gAudioSpeakerMode;
int gStoredPlayerCarType;
int gSoundHardware;
int gSoundCompatibility;
int gEventTargetRange[5];
int gUltraLamps;
int FrameCount;
int gDisableDebug;
int gDebugLevel;
int gNight;
int gDarkNight;
int gHighPriority;
int gUseCDAudio;
int gUseDoppler;
int gDoInterview;
int gSkipInterview;
int gDoTraining;
int gDoRave;
int gRecordTraining;
int gMissionRecording;
int gMissionReplaying;
int gMissionSetting;
int gDoFrontend;
int gNoPedestrians;
int gBuildKeyFrames;
int gNoCops;
int gSaveScreenShots;
int gDisableText;
int gDisableDenting;
int gDoAussie;
int TrailStarted;
int TestTrail;
int SaveTrail;
int FlyBy;
int EndofFlyBy;
int gDoMotelRoom;
int gForceFeedback;
int gNoFMV;
int gDoCredits;
int gInPresentation;
int SettingJoyKey;
int gUseBadGuyModel;
int gChaseHQMode;
int gBotchRoads;

// NOTE: two never-written buffers that WinMain logs as blank lines around the credits
static char logLine1[4];
static char logLine2[108];

// [A] windowed mode, the default; command line switch "/FULLSCREEN" turns it off. The game window
// gets a frame and a client area of display_width x display_height instead of a maximised popup.
// The renderer still takes the fullscreen DirectDraw path, so it needs a wrapper that emulates
// fullscreen in a window (dgVoodoo with FullScreenMode = false). With /GL the window has the size
// from the config whatever the resolution, the user may resize it, and Alt-Enter changes this flag.
int gWindowed;

// [A] command line switch "/GLIDE": use glide2x.dll instead of the configured device, see grSetDevice
int gUseGlide;

// [A] command line switch "/DSOUND": use the DirectSound back end instead of OpenAL, see sound.c
int gUseDSound;

// [A] command line switch "/GL": use the OpenGL device instead of the configured one, see grSetDevice
int gUseGL;

// [A] the SDL window; gScreen.handle and gScreen.module are its HWND and HINSTANCE for the code that
// still needs them (DirectDraw, DirectInput, DirectSound)
SDL_Window *gWindow;

// NOTE: moved here from the generated stubs; no IDA mark, the owner is a guess from the users
// and the neighbouring data
SCREEN gScreen;
HANDLE gProcess;
HARDWARE_SETTINGS hw_settings;
int display_depth;

// [0x5242f0] [D] [A]
// NOTE: [A] main instead of WinMain, the command line comes joined from argv
int main(int argc, char *argv[])
{
	char cmdLine[256] = { 0 };	// NOTE: [A]
	int motel;					// NOTE: [A]
	int interview;				// NOTE: [A]
	int credits;				// NOTE: [A]
	int mission_number;			// NOTE: [A]
	char *arg;					// NOTE: [A]
	int i;						// NOTE: [A]

	SDL_SetMainReady();

	gDoFrontend = 1;
	gUserLanguage = 5;

	// NOTE: [A] not in the original (its command line parser was dropped by the linker);
	// a switch in the style of its "/RES640", "/NOPEDS" ...
	for (i = 1; i < argc; i++)
	{
		strncat(cmdLine, argv[i], sizeof(cmdLine) - strlen(cmdLine) - 1);
		strncat(cmdLine, " ", sizeof(cmdLine) - strlen(cmdLine) - 1);
	}

	_strupr(cmdLine);

	// NOTE: [A] the game runs in a window, full screen only when asked for
	gWindowed = strstr(cmdLine, "/FULLSCREEN") == NULL;

	gUseGlide = strstr(cmdLine, "/GLIDE") != NULL;
	gUseDSound = strstr(cmdLine, "/DSOUND") != NULL;

	// each switch is followed by a space, "/GL" alone would match /GLIDE too
	gUseGL = strstr(cmdLine, "/GL ") != NULL;

	// the game doesn't pump messages while it waits (FadeInScreen, loading); a framed window
	// would be replaced with a "Not responding" ghost after 5 seconds
	if (gWindowed || gUseGL)
		DisableProcessWindowsGhosting();

	// NOTE: [A] "/MOTEL": start in the motel room (DoFlat) instead of the frontend, for testing
	motel = strstr(cmdLine, "/MOTEL") != NULL;

	// NOTE: [A] "/INTERVIEW": start straight in the interview (the car park in IVIEW.LEV), as the
	// mission ladder's INTERVIEW command does (RunMissionLadder); for testing
	interview = strstr(cmdLine, "/INTERVIEW") != NULL;

	// NOTE: [A] "/CREDITS": play the end credits and quit, for testing
	credits = strstr(cmdLine, "/CREDITS") != NULL;

	// NOTE: [A] "/MISSION<n>": start straight in mission script n (Scripts\Missions\Mission<n>.dms,
	// e.g. /MISSION201 for the interview training), as the mission ladder's MISSION command does
	mission_number = 0;
	arg = strstr(cmdLine, "/MISSION");

	if (arg)
		mission_number = atoi(arg + 8);

	memset(gCheatsAllowed, 0, sizeof(gCheatsAllowed));

	gCheatFlags.PlayerCar = 0;
	gCheatFlags.SuperFastCars = 0;
	gCheatFlags.Invincibility = 0;
	gCheatFlags.Immunity = 0;
	gCheatFlags.FreightTrain = 0;
	gCheatFlags.OverheadView = 0;
	gCheatFlags.RailroadCops = 0;

	quit = 0;

	InitMemDebug();

	display_width = 640;
	display_height = 480;
	display_depth = 16;

	LogQuick("Driver - Log File -");
	LogQuick("Driver (c)1998-1999 Reflections Interactive");
	LogQuick(logLine1);
	LogQuick("Mucho respectomondo to Driver PC coder(s): ");
	LogQuick("Dirty Wayne, Evil Russ, Gentleman Jim, Guru Naif, Hodgy Hax, Steve Royston, Slurpy Chris.");
	LogQuick(logLine2);

	InitScreen();
	Init_FileSystem();

	gDisableText = 1;
	gDoFrontend = 1;
	gUseCDAudio = 0;
	gDebugLevel = 0;

	ChaseCarPalette = 4;

	do_plug = 0;

	if (gDisableDebug)
		do_plug = 0;

	if (gDoFrontend && !LoadSavedOptions())
	{
		InitStringMng();
		strcpy(errorMessage, gLanguageStrings[306]);
		DeinitStringMng();

		Shutdown_FileSystem();
		DisplayErrorAndExit(errorMessage);
	}

	// NOTE: [A] after the options, the OpenGL window takes its size from them; the original
	// opened the window before them
	if (!InitWindow())
		DisplayErrorAndExit("[WinMain] InitWindow failed");

	if (!InitDisplay(2))
	{
		InitStringMng();
		strcpy(errorMessage, gLanguageStrings[306]);
		DeinitStringMng();

		Shutdown_FileSystem();
		DisplayErrorAndExit(errorMessage);
	}

	if (!InitTimers())
	{
		Shutdown_FileSystem();
		DisplayErrorAndExit("[WinMain] Failed to initialise timers.");
	}

	gProcess = GetCurrentProcess();

	if (gAutoPriority && gProcess)
	{
		if (gHighPriority)
			SetPriorityClass(gProcess, REALTIME_PRIORITY_CLASS);
		else
			SetPriorityClass(gProcess, HIGH_PRIORITY_CLASS);
	}

	// NOTE: [A] the original turns the screen saver off here with SystemParametersInfo; SDL keeps
	// it off while the video subsystem is up. The cursor is hidden over the window only in full
	// screen, the game doesn't use it; the original hid it always (WM_SETCURSOR).
	if (!gWindowed)
		SDL_ShowCursor(SDL_DISABLE);

	// NOTE: [A] /MOTEL: the original's own WinMain branch for gDoMotelRoom
	if (motel)
	{
		gDoFrontend = 0;
		gDoMotelRoom = 1;
	}

	// NOTE: [A] /INTERVIEW, /MISSION<n>: what RunMissionLadder sets before StartGame; the game
	// starts in WinMain's own StartGame branch (gDoFrontend 0)
	if (credits)
	{
		gDoFrontend = 0;
	}
	else if (interview)
	{
		gDoFrontend = 0;

		GameLevel = 4;
		GameMode = 0;
		gDoTraining = 0;
		gDoInterview = 1;
		gDoLightFlare = 1;
		gNight = 1;
	}
	else if (mission_number > 0)
	{
		gDoFrontend = 0;

		GameMode = mission_number;
		global_score.type = GAME_MISSION;
	}

	if (MainInit())
	{
		InterruptBegin();

		if (gDoFrontend)
		{
			gDisableDebug = 1;
			gAllowCheatKeys = 0;
			DoFrontend();
		}
		else if (gDoMotelRoom)
		{
			DoFlat(NULL);
		}
		else if (credits)
		{
			// NOTE: [A] /CREDITS: what credits_choosefunc does from the cheats menu; the debug
			// settings are the frontend branch's above
			gDisableDebug = 1;
			gAllowCheatKeys = 0;

			gDoCredits = 1;
			gbPlayingDemo = 1;

			InitReplay("CREDITS");
			StartGame(1);

			gbPlayingDemo = 0;
			gDoCredits = 0;
		}
		else
		{
			quit = 1;
			StartGame(0);
		}

		HandleWinMessages();
		StopTimers();

		SetPriorityClass(gProcess, NORMAL_PRIORITY_CLASS);

		InterruptEnd();

		MainExit();
		ShutdownDrawingSystem();

		SDL_ShowCursor(SDL_ENABLE);

		HandleWinMessages();
		grGlideReallyShutdown();
		CloseMemDebug();
		Shutdown_FileSystem();

		if (exit_code == EXIT_LEAD_CAR_OUT_OF_RANGE)
			ShowMessageBox("The lead car went out of range!", "Exit!", MESSAGE_INFO);

		return exit_code;
	}
	else
	{
		ShutdownDrawingSystem();
		grGlideReallyShutdown();
		CloseMemDebug();
		SDL_ShowCursor(SDL_ENABLE);
		Shutdown_FileSystem();

		return exit_code;
	}
}

// [0x5246c8] [D]
void Init_FileSystem(void)
{
	DeleteWaypoints();

	remove("REPLAYDATA\\USER\\$TEMP$.CAMS");

	ClearMem(&Director, 404);	// sizeof(DIRECTOR)

	SetDirectorMode(0, 0, 0);
}

// [0x5246ff] [D]
void Shutdown_FileSystem(void)
{
	DestroyCameras();
	DestroyWaypoints();
	SetDirectorMode(0, 0, 0);
}

// [0x52471c] [D] [A]
// NOTE: [A] the SDL event pump; it does what WndProc (0x5248a1) did for each message. Keyboard
// messages were ignored there too, input still comes from DirectInput.
void HandleWinMessages(void)
{
	SDL_Event event;

	while (SDL_PollEvent(&event))
	{
		if (event.type == SDL_QUIT)
			CloseGame();

		if (event.type == SDL_KEYDOWN && gUseGL && !event.key.repeat &&
			event.key.keysym.scancode == SDL_SCANCODE_RETURN && (event.key.keysym.mod & KMOD_ALT))
		{
			ToggleFullscreen();
		}

		if (event.type == SDL_CONTROLLERDEVICEADDED)
			ControllerAdded(event.cdevice.which);
		else if (event.type == SDL_CONTROLLERDEVICEREMOVED)
			ControllerRemoved(event.cdevice.which);

		if (event.type != SDL_WINDOWEVENT)
			continue;

		switch (event.window.event)
		{
			case SDL_WINDOWEVENT_MOVED:
				if (!grSstControl(GR_CONTROL_MOVE))
					CloseGame();
				break;

			case SDL_WINDOWEVENT_FOCUS_GAINED:
				SetWindowActive(1);
				break;

			case SDL_WINDOWEVENT_FOCUS_LOST:
			case SDL_WINDOWEVENT_MINIMIZED:
				SetWindowActive(0);
				break;

			case SDL_WINDOWEVENT_RESTORED:
				if (SDL_GetWindowFlags(gWindow) & SDL_WINDOW_INPUT_FOCUS)
					SetWindowActive(1);
				break;
		}
	}
}

// [A]
// The original's WM_ACTIVATE and WM_ACTIVATEAPP handlers in one, SDL gives one event for both.
void SetWindowActive(int active)
{
	if (active == gScreen.active)
		return;

	if (active)
	{
		grSstControl(GR_CONTROL_ACTIVATE);
		gScreen.active = 1;

		if (gHighPriority)
			SetPriorityClass(gProcess, REALTIME_PRIORITY_CLASS);
		else
			SetPriorityClass(gProcess, HIGH_PRIORITY_CLASS);
	}
	else
	{
		gScreen.active = 0;
		grSstControl(GR_CONTROL_DEACTIVATE);
		SetPriorityClass(gProcess, IDLE_PRIORITY_CLASS);
	}
}

// [A]
// Alt-Enter. The window stays and so does the context, only the picture is scaled to another size.
static void ToggleFullscreen(void)
{
	if (SDL_SetWindowFullscreen(gWindow, gWindowed ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0) != 0)
		return;

	gWindowed = !gWindowed;

	SDL_ShowCursor(gWindowed ? SDL_ENABLE : SDL_DISABLE);
}

// [A]
// Dragging the frame runs a loop of its own inside the event pump, so the game stands still and
// draws nothing until it ends. A watch sees the events as they come, the queue would give them out
// only afterwards. It may be called from other threads, for events of other kinds.
static int SDLCALL WindowEventWatch(void *userdata, SDL_Event *event)
{
	if (event->type == SDL_WINDOWEVENT && (event->window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
		event->window.event == SDL_WINDOWEVENT_EXPOSED))
	{
		gl_RepaintWindow();
	}

	return 0;
}

// [A]
// The window was closed. In the original WM_CLOSE only destroyed the window and the game went on
// without it. This shuts down like DisplayErrorAndExit; CloseMemDebug is left out, it would report
// all the memory still in use as leaks.
void CloseGame(void)
{
	StopTimers();
	MainExit();
	ShutdownDrawingSystem();
	grGlideReallyShutdown();
	InterruptEnd();

	exit(0);
}

// [0x524762] [D] [A]
void DisplayErrorAndExit(const char *text)
{
	StopTimers();
	MainExit();
	ShutdownDrawingSystem();
	grGlideReallyShutdown();
	InterruptEnd();

	ShowMessageBox(text, "ERROR", MESSAGE_INFO);

	CloseMemDebug();

	exit(0);
}

// [0x5247b7] [D]
void InitScreen(void)
{
	gScreen.handle = NULL;
	gScreen.module = NULL;

	// NB: I've always wondered why this was put here
	InitialiseChairs();
}

// [0x5247d5] [D] [A]
// NOTE: [A] an SDL window instead of RegisterClass and CreateWindowEx
// NOTE: [A] the OpenGL window always has a frame and the size from the config: full screen is a
// state of it, so Alt-Enter has that size to go back to
int InitWindow(void)
{
	SDL_SysWMinfo info;
	Uint32 flags;

	if (SDL_Init(SDL_INIT_VIDEO) != 0)
		return 0;

	atexit(SDL_Quit);

	flags = SDL_WINDOW_SHOWN;

	if (gUseGL)
	{
		flags |= SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;

		if (!gWindowed)
			flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;

		gWindow = SDL_CreateWindow(WINDOW_TITLE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
			wanted_display_width, wanted_display_height, flags);
	}
	else if (gWindowed)
	{
		// NOTE: [A] windowed mode, a fixed size frame (the renderer does not follow resizing)
		gWindow = SDL_CreateWindow(WINDOW_TITLE, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
			display_width, display_height, flags);
	}
	else
	{
		gWindow = SDL_CreateWindow(WINDOW_TITLE, 0, 0, display_width, display_height,
			flags | SDL_WINDOW_BORDERLESS);
	}

	if (!gWindow)
		return 0;

	SDL_VERSION(&info.version);

	if (!SDL_GetWindowWMInfo(gWindow, &info))
		return 0;

	gScreen.handle = info.info.win.window;
	gScreen.module = info.info.win.hinstance;

	if (gUseGL)
		SDL_AddEventWatch(WindowEventWatch, NULL);

	// the original got WM_ACTIVATE from ShowWindow
	HandleWinMessages();

	return 1;
}

// [A]
// Sizes the window for the display resolution: windowed mode keeps it where it is, the full screen
// window goes to the corner of the screen. The OpenGL window scales the picture instead.
void SetGameWindowSize(int width, int height)
{
	if (gUseGL)
		return;

	if (!gWindowed)
		SDL_SetWindowPosition(gWindow, 0, 0);

	SDL_SetWindowSize(gWindow, width, height);
}

// [A]
// Replaces MessageBox(gScreen.handle, ...). Returns 1, IDOK, the only button.
int ShowMessageBox(const char *text, const char *caption, int type)
{
	Uint32 flags;

	if (type == MESSAGE_ERROR)
		flags = SDL_MESSAGEBOX_ERROR;
	else if (type == MESSAGE_WARNING)
		flags = SDL_MESSAGEBOX_WARNING;
	else
		flags = SDL_MESSAGEBOX_INFORMATION;

	SDL_ShowSimpleMessageBox(flags, caption, text, gWindow);
	return 1;
}

// [0x524b57] [D] [A]
int InitDisplay(int window_mode)
{
	gScreen.fullscreen = (window_mode != 1);

	grSetDevice(wanted_display_device, config_data.No8BitSupport, config_data.TripleBuffering);

	if (Detect3dfx())
	{
		using_glide = (grGetDevice() == 0);

		if (InitDrawingSystem(gScreen.handle, gScreen.fullscreen, 0))
		{
			GetHWSettings(&hw_settings);
			return 1;
		}

		ShutdownDrawingSystem();
	}

	return 0;
}

// [0x524be2] [D]
void InterruptBegin(void)
{
	SetUnhandledExceptionFilter(UpdateInterruptState);
}

// [0x524bf2] [D]
void InterruptEnd(void)
{
	SetUnhandledExceptionFilter(NULL);
}

// [0x524bff] [D]
LONG WINAPI UpdateInterruptState(EXCEPTION_POINTERS *exceptionInfo)
{
	char text[256];

	SetUnhandledExceptionFilter(NULL);

	StopTimers();
	ShutdownDrawingSystem();
	grGlideReallyShutdown();
	ShutdownSoundSystem();

	// NOTE: the original reads exceptionInfo->ExceptionRecord->ExceptionAddress into an unused
	// local here (probably left from printing the address); dropped as dead code
	sprintf(text, "Application Error\nClick OK to close.");
	MessageBox(NULL, text, "Driver Exception Handler", MB_ICONHAND);

	return EXCEPTION_EXECUTE_HANDLER;
}
