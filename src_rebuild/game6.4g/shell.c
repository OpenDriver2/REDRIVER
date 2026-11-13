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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include <SDL.h>

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

#ifdef _WIN32
// NOTE: [A] from user32, without windows.h; SDL has nothing for it
__declspec(dllimport) void __stdcall DisableProcessWindowsGhosting(void);
#endif

void HandleWinMessages(void);
void __declspec(noreturn) DisplayErrorAndExit(const char *text);
void Init_FileSystem(void);
void Shutdown_FileSystem(void);
void InitScreen(void);
int InitWindow(void);
void SetWindowActive(int active);
static void RememberWindowSize(void);
static void ToggleFullscreen(void);
void CloseGame(void);
int InitDisplay(int window_mode);
void InterruptBegin(void);
void InterruptEnd(void);
void UpdateInterruptState(int sig);
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

static char errorMessage[256];	// NOTE: [A] no references, the messages it took are gone from main
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

// [A] windowed mode, the default; config.dat keeps it, command line switch "/FULLSCREEN" turns it
// off. The game window gets a frame instead of a maximised popup. The user may resize it, and
// Alt-Enter changes this flag; the game display takes the size of the window (FollowWindowSize).
int gWindowed = 1;

// [A] the size of the framed window, kept in config.dat; 0 for the size of the display mode
int gWindowWidth;
int gWindowHeight;

// NOTE: [A] command line switch "/NOFADE": polygons are cut at far_clip as in the original, instead
// of fading out through a pattern
int gNoFarFade;

// [A] the SDL window
SDL_Window *gWindow;

// NOTE: moved here from the generated stubs; no IDA mark, the owner is a guess from the users
// and the neighbouring data
SCREEN gScreen;
HARDWARE_SETTINGS hw_settings;
int display_depth;

// [0x5242f0] [D] [A]
// NOTE: [A] main instead of WinMain, the command line comes joined from argv
int main(int argc, char *argv[])
{
	char cmdLine[256] = { 0 };	// NOTE: [A]
	int res_width;
	int res_height;
	int motel;					// NOTE: [A]
	int interview;				// NOTE: [A]
	int credits;				// NOTE: [A]
	int mission_number;			// NOTE: [A]
	char *arg;					// NOTE: [A]
	int i;						// NOTE: [A]

	SDL_SetMainReady();

	gDoFrontend = 1;

	// NOTE: [A] the language of the game from its LANG file, not fixed to USA
	gUserLanguage = DetectGameLanguage();

	// NOTE: [A] not in the original (its command line parser was dropped by the linker);
	// a switch in the style of its "/RES640", "/NOPEDS" ...
	for (i = 1; i < argc; i++)
	{
		strncat(cmdLine, argv[i], sizeof(cmdLine) - strlen(cmdLine) - 1);
		strncat(cmdLine, " ", sizeof(cmdLine) - strlen(cmdLine) - 1);
	}

	_strupr(cmdLine);

	// NOTE: [A] "/AUSSIE": the string is in the binary, but nothing reads it and nothing sets
	// gDoAussie there. The picture upside down, see InitDrawingSystem
	gDoAussie = strstr(cmdLine, "/AUSSIE") != NULL;

	gNoFarFade = strstr(cmdLine, "/NOFADE") != NULL;

#ifdef _WIN32
	// the game doesn't pump messages while it waits (FadeInScreen, loading); a framed window
	// would be replaced with a "Not responding" ghost after 5 seconds
	DisableProcessWindowsGhosting();
#endif

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

	// NOTE: [A] without config.dat ("Please run Driver Configuration." in the original, which then
	// quit) the game starts with the default options and writes them
	if (gDoFrontend && !LoadSavedOptions())
		UpdateSavedOptions();

	// NOTE: [A] the game runs in a window, full screen as config.dat has it or when asked for
	if (strstr(cmdLine, "/FULLSCREEN"))
		gWindowed = 0;

	// NOTE: [A] "/RES=1280x720": the size of the window to start with, saved with it. The game display
	// takes the size of the window; wider than 4:3 the view gets wider and the HUD goes to the sides,
	// see wide_aspect
	arg = strstr(cmdLine, "/RES=");

	if (arg && sscanf(arg, "/RES=%dX%d", &res_width, &res_height) == 2 &&
		res_width >= WINDOW_MIN_WIDTH && res_height >= WINDOW_MIN_HEIGHT)
	{
		gWindowWidth = res_width;
		gWindowHeight = res_height;
	}

	// NOTE: [A] after the options, the OpenGL window takes its size from them; the original
	// opened the window before them
	if (!InitWindow())
		DisplayErrorAndExit("[WinMain] InitWindow failed");

	// NOTE: [A] the original told to run Driver Configuration here too; the display is OpenGL now,
	// the configuration has nothing to set up for it
	if (!InitDisplay(2))
	{
		Shutdown_FileSystem();
		DisplayErrorAndExit("FATAL: Can't open the OpenGL 3.1 display");
	}

	if (!InitTimers())
	{
		Shutdown_FileSystem();
		DisplayErrorAndExit("[WinMain] Failed to initialise timers.");
	}

	// NOTE: [A] the priority of this thread, the original set that of the process
	if (gAutoPriority)
		SDL_SetThreadPriority(gHighPriority ? SDL_THREAD_PRIORITY_TIME_CRITICAL : SDL_THREAD_PRIORITY_HIGH);

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

		SaveWindowOptions();	// NOTE: [A]

		SDL_SetThreadPriority(SDL_THREAD_PRIORITY_NORMAL);

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

		if (event.type == SDL_KEYDOWN && !event.key.repeat &&
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
			case SDL_WINDOWEVENT_SIZE_CHANGED:
				RememberWindowSize();
				break;

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

		SDL_SetThreadPriority(gHighPriority ? SDL_THREAD_PRIORITY_TIME_CRITICAL : SDL_THREAD_PRIORITY_HIGH);
	}
	else
	{
		gScreen.active = 0;
		grSstControl(GR_CONTROL_DEACTIVATE);
		SDL_SetThreadPriority(SDL_THREAD_PRIORITY_LOW);
	}
}

// [A]
// The size of the framed window for config.dat: neither full screen nor maximised, those have the
// size of the screen.
static void RememberWindowSize(void)
{
	int width;
	int height;

	if (SDL_GetWindowFlags(gWindow) & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_MAXIMIZED | SDL_WINDOW_MINIMIZED))
		return;

	SDL_GetWindowSize(gWindow, &width, &height);

	if (width >= WINDOW_MIN_WIDTH && height >= WINDOW_MIN_HEIGHT)
	{
		gWindowWidth = width;
		gWindowHeight = height;
	}
}

// [A]
// Alt-Enter. The window stays and so does the context; the game display takes the new size of the
// window (FollowWindowSize), the frontend is scaled to it.
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
// all the memory still in use as leaks. The window goes to config.dat.
void CloseGame(void)
{
	SaveWindowOptions();

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

// [0x5247b7] [D] [A]
// NOTE: [A] no window handles in SCREEN
void InitScreen(void)
{
	// NB: I've always wondered why this was put here
	InitialiseChairs();
}

// [0x5247d5] [D] [A]
// NOTE: [A] an SDL window instead of RegisterClass and CreateWindowEx
// NOTE: [A] the window always has a frame and the size from config.dat (else of the display mode):
// full screen is a state of it, so Alt-Enter has that size to go back to
int InitWindow(void)
{
	Uint32 flags;
	int width;
	int height;

	if (SDL_Init(SDL_INIT_VIDEO) != 0)
		return 0;

	atexit(SDL_Quit);

	flags = SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;

	if (!gWindowed)
		flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;

	width = gWindowWidth ? gWindowWidth : wanted_display_width;
	height = gWindowHeight ? gWindowHeight : wanted_display_height;

	gWindow = SDL_CreateWindow(WINDOW_TITLE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		width, height, flags);

	if (!gWindow)
		return 0;

	SDL_SetWindowMinimumSize(gWindow, WINDOW_MIN_WIDTH, WINDOW_MIN_HEIGHT);

	gWindowWidth = width;
	gWindowHeight = height;

	SDL_AddEventWatch(WindowEventWatch, NULL);

	// the original got WM_ACTIVATE from ShowWindow
	HandleWinMessages();

	return 1;
}

// [A]
// The window doesn't follow the display resolution, the OpenGL device scales the picture
void SetGameWindowSize(int width, int height)
{
}

// [A]
// Replaces MessageBox(window, ...). Returns 1, IDOK, the only button.
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
		if (InitDrawingSystem(gScreen.fullscreen, 0))
		{
			GetHWSettings(&hw_settings);
			return 1;
		}

		ShutdownDrawingSystem();
	}

	return 0;
}

// [0x524be2] [D] [A]
// NOTE: [A] C signals instead of SetUnhandledExceptionFilter
void InterruptBegin(void)
{
	signal(SIGSEGV, UpdateInterruptState);
	signal(SIGILL, UpdateInterruptState);
	signal(SIGFPE, UpdateInterruptState);
}

// [0x524bf2] [D] [A]
// NOTE: [A] C signals instead of SetUnhandledExceptionFilter
void InterruptEnd(void)
{
	signal(SIGSEGV, SIG_DFL);
	signal(SIGILL, SIG_DFL);
	signal(SIGFPE, SIG_DFL);
}

// [0x524bff] [D] [A]
// NOTE: [A] a signal handler, and it ends the game itself: EXCEPTION_EXECUTE_HANDLER did that
void UpdateInterruptState(int sig)
{
	char text[256];

	InterruptEnd();

	StopTimers();
	ShutdownDrawingSystem();
	grGlideReallyShutdown();
	ShutdownSoundSystem();

	// NOTE: the original reads exceptionInfo->ExceptionRecord->ExceptionAddress into an unused
	// local here (probably left from printing the address); dropped as dead code
	sprintf(text, "Application Error\nClick OK to close.");
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Driver Exception Handler", text, NULL);

	_Exit(1);
}
