/*
 * shell.c - Windows shell: WinMain, window, message pump, global game flags
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
#include "ai.h"
#include "chairs.h"
#include "debris.h"
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
int InitWindow(HINSTANCE hInstance, int nCmdShow);
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
int InitDisplay(HINSTANCE hInstance, int nCmdShow, int window_mode);
void InterruptBegin(void);
void InterruptEnd(void);
LONG WINAPI UpdateInterruptState(EXCEPTION_POINTERS *exceptionInfo);

//------------------------------------------------------------------------------------------
// initialised data (0x578240)

char szAppName[] = "Driver";	// NOTE: name guessed; window class name

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
static UINT screenSaverActive;
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

// NOTE: moved here from the generated stubs; no IDA mark, the owner is a guess from the users
// and the neighbouring data
SCREEN gScreen;
HANDLE gProcess;
HARDWARE_SETTINGS hw_settings;
int display_depth;

// [0x5242f0] [D]
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	HCURSOR oldCursor;

	gDoFrontend = 1;
	gUserLanguage = 5;

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

	if (!InitWindow(hInstance, nCmdShow))
		DisplayErrorAndExit("[WinMain] InitWindow failed");

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

	if (!InitDisplay(hInstance, nCmdShow, 2))
	{
		InitStringMng();
		strcpy(errorMessage, gLanguageStrings[306]);
		DeinitStringMng();

		Shutdown_FileSystem();
		DisplayErrorAndExit(errorMessage);
	}

	LoadAccelerators(gScreen.module, "ACCELERATOR");

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

	oldCursor = SetCursor(NULL);

	// NB: this seems to be related to "power saving mode" and forcing it to be disabled :P
	SystemParametersInfo(SPI_GETSCREENSAVEACTIVE, 0, &screenSaverActive, 0);
	SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, FALSE, NULL, 0);

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

		SetCursor(oldCursor);

		HandleWinMessages();
		grGlideReallyShutdown();
		CloseMemDebug();
		Shutdown_FileSystem();

		if (exit_code == EXIT_LEAD_CAR_OUT_OF_RANGE)
			MessageBox(gScreen.handle, "The lead car went out of range!", "Exit!", MB_OK);

		SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, screenSaverActive, NULL, 0);
		return exit_code;
	}
	else
	{
		ShutdownDrawingSystem();
		grGlideReallyShutdown();
		CloseMemDebug();
		SetCursor(oldCursor);
		Shutdown_FileSystem();

		SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, screenSaverActive, NULL, 0);
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

// [0x52471c] [D]
void HandleWinMessages(void)
{
	MSG msg;

	while (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE))
	{
		GetMessage(&msg, NULL, 0, 0);
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}

// [0x524762] [D]
void DisplayErrorAndExit(const char *text)
{
	StopTimers();
	MainExit();
	ShutdownDrawingSystem();
	grGlideReallyShutdown();
	InterruptEnd();

	MessageBox(gScreen.handle, text, "ERROR", MB_OK);

	CloseMemDebug();

	SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, screenSaverActive, NULL, 0);
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

// [0x5247d5] [D]
int InitWindow(HINSTANCE hInstance, int nCmdShow)
{
	WNDCLASS wc;

	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance;
	wc.hIcon = LoadIcon(hInstance, IDI_APPLICATION);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = CreateSolidBrush(0);
	wc.lpszMenuName = NULL;
	wc.lpszClassName = szAppName;

	RegisterClass(&wc);

	gScreen.handle = CreateWindowEx(0, szAppName, "Driver V1.3", WS_POPUP, 0, 0, display_width, display_height,
		NULL, NULL, hInstance, NULL);

	if (!gScreen.handle)
		return 0;

	ShowWindow(gScreen.handle, nCmdShow);
	return 1;
}

// [0x5248a1] [D]
// NOTE: Hex-Rays fails on this function (two-level switch); translated from the disassembly
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
		case WM_CREATE:
			gScreen.module = ((LPCREATESTRUCT)lParam)->hInstance;
			break;

		case WM_DESTROY:
			PostQuitMessage(0);
			break;

		case WM_MOVE:
			if (!grSstControl(GR_CONTROL_MOVE))
				PostMessage(hWnd, WM_CLOSE, 0, 0);
			break;

		case WM_ACTIVATE:
		{
			WORD state = LOWORD(wParam);
			WORD minimized = HIWORD(wParam);

			if (state == WA_INACTIVE || minimized)
			{
				gScreen.active = 0;
				grSstControl(GR_CONTROL_DEACTIVATE);
			}
			else
			{
				grSstControl(GR_CONTROL_ACTIVATE);
				gScreen.active = 1;
			}
			break;
		}

		case WM_ACTIVATEAPP:
			gScreen.active = wParam;

			if (gScreen.active)
			{
				grSstControl(GR_CONTROL_ACTIVATE);

				if (gHighPriority)
					SetPriorityClass(gProcess, REALTIME_PRIORITY_CLASS);
				else
					SetPriorityClass(gProcess, HIGH_PRIORITY_CLASS);
			}
			else
			{
				grSstControl(GR_CONTROL_DEACTIVATE);
				SetPriorityClass(gProcess, IDLE_PRIORITY_CLASS);
			}
			break;

		case WM_SETCURSOR:
			if (gScreen.fullscreen == 1)
				SetCursor(NULL);
			else
				SetCursor(LoadCursor(NULL, IDC_ARROW));
			return TRUE;

		case WM_KEYDOWN:
		case WM_KEYUP:
		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
			return 0;
	}

	return DefWindowProc(hWnd, message, wParam, lParam);
}

// [0x524b57] [D]
int InitDisplay(HINSTANCE hInstance, int nCmdShow, int window_mode)
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
