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

typedef struct SCREEN
{
	HWND handle;
	HINSTANCE module;
	int fullscreen;
	int active;
} SCREEN;

typedef struct CHEATS
{
	int PlayerCar;
	int SuperFastCars;
	int Immunity;
	int Invincibility;
	int FreightTrain;
	int OverheadView;
	int RailroadCops;
} CHEATS;

typedef struct CONFIG_DATA
{
	int display_device;
	int display_mode;
	int display_depth;
	int InputDevice;
	char UserKeys[20];
	char JoySet[12];
	int JoystickDevice;
	int DrawShadows;
	int DoSmoke;
	int DoDebris;
	int DoRearViewMirror;
	int DoTexturedTyreTracks;
	int DoTyreTracks;
	int DoShinyCars;
	int DoLensFlare;
	int DoLightFlare;
	int DrawSubmergedObjects;
	int DrawMap;
	int AllowSounds;
	int UseCDAudio;
	int UseDoppler;
	float MasterVolume;
	float MusicVolume;
	char NumCitiesOpen;
	char JoyIsAnalogue;
	char JoyDeadZone;
	char NightBrightness;
	float far_clip;
	float mirror_far_clip;
	float screen_scale;
	int max_pedestrians;
	int ForceFeedback;
	int RealTimeLights;
	int FogAllowed;
	int DoDetailedShadows;
	int DoMultiColouredCars;
	int AllowFMV;
	int AutoBrake;
	int Difficulty;
	int DoInterpolation;
	int DoAutoInterpolation;
	int max_frame_skip;
	int DetailLevel;
	int UltraLamps;
	int AudioMixerBits;
	int AudioMixerFreq;
	int AudioSpeakerMode;
	int CheatsAllowed[11];
	int TrafficDensity;
	int SoundHardware;
	int SoundCompatibility;
	int UseEAX;
	int TripleBuffering;
	int No8BitSupport;
} CONFIG_DATA;

typedef struct HARDWARE_SETTINGS HARDWARE_SETTINGS;
typedef struct DIRECTOR DIRECTOR;
typedef struct MLADDER_ROOM MLADDER_ROOM;

// Glide is reached through function pointers, so only the types and GR_* constants are taken
// from glide.h; grSstControl() below is the engine wrapper from D3DGlide/system.c
#define FX_GLIDE_NO_FUNC_PROTO
#include <glide.h>

#define EXIT_LEAD_CAR_OUT_OF_RANGE	201

/* D3DGlide/system.c */
extern void grSetDevice(int device, int no8BitSupport, int tripleBuffering);
extern int grGetDevice(void);
extern int grSstControl(unsigned long code);
extern void grGlideReallyShutdown(void);
extern void GetHWSettings(HARDWARE_SETTINGS *settings);

/* Engine/3dfx/gfxinit.c */
extern int Detect3dfx(void);
extern int InitDrawingSystem(HWND hWnd, int fullscreen, int flags);
extern void ShutdownDrawingSystem(void);

/* general.c, debug.c, time.c, sound.c, chairs.c, ai.c */
extern void InitMemDebug(void);
extern void CloseMemDebug(void);
extern void LogQuick(char *text);
extern int InitTimers(void);
extern void StopTimers(void);
extern void ShutdownSoundSystem(void);
extern void InitialiseChairs(void);
extern void ClearMem(void *mem, size_t size);

/* Director.c, DirectorStorage.c */
extern void DeleteWaypoints(void);
extern void DestroyCameras(void);
extern void DestroyWaypoints(void);
extern int SetDirectorMode(int mode, int selection, int debug);
extern DIRECTOR Director;

/* glaunch.c, pres.c */
extern int MainInit(void);
extern void MainExit(void);
extern void InitStringMng(void);
extern void DeinitStringMng(void);

/* frontend */
extern int LoadSavedOptions(void);
extern void DoFrontend(void);
extern void StartGame(int do_credits);
extern int DoFlat(MLADDER_ROOM *room);

extern CONFIG_DATA config_data;
extern HARDWARE_SETTINGS hw_settings;
extern int wanted_display_device;
extern int gUserLanguage;
extern char *gLanguageStrings[];
extern int gCheatsAllowed[11];
extern CHEATS gCheatFlags;
extern int display_width;
extern int display_height;
extern int display_depth;

// NOTE: these sit in a zero-initialised block (0x57d8c8) between cheats.c and config.c data,
// so they are not treated as shell.c's own even though shell.c is their main user.
extern int ChaseCarPalette;
extern SCREEN gScreen;
extern int using_glide;
extern HANDLE gProcess;

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
void SetGameWindowSize(HWND hWnd, int width, int height);

// NOTE: [A] the original title is "Driver V1.3"
#define WINDOW_TITLE		"Driver V1.3 (OpenDriverPC)"

// [A] windowed mode: a fixed size frame (the renderer does not follow resizing)
#define WINDOWED_STYLE		(WS_OVERLAPPEDWINDOW & ~(WS_THICKFRAME | WS_MAXIMIZEBOX))

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

// [A] windowed mode, command line switch "/WINDOW": the game window gets a frame and a client
// area of display_width x display_height instead of a maximised popup. The renderer still takes
// the fullscreen DirectDraw path, so it needs a wrapper that emulates fullscreen in a window
// (dgVoodoo with FullScreenMode = false).
int gWindowed;

// [0x5242f0] [D] [A]
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	HCURSOR oldCursor;
	char cmdLine[256] = { 0 };	// NOTE: [A]

	gDoFrontend = 1;
	gUserLanguage = 5;

	// NOTE: [A] not in the original (its command line parser was dropped by the linker);
	// a switch in the style of its "/RES640", "/NOPEDS" ...
	strncpy(cmdLine, lpCmdLine, sizeof(cmdLine) - 1);
	_strupr(cmdLine);

	if (strstr(cmdLine, "/WINDOW"))
	{
		gWindowed = 1;

		// the game doesn't pump messages while it waits (FadeInScreen, loading); a framed window
		// would be replaced with a "Not responding" ghost after 5 seconds
		DisableProcessWindowsGhosting();
	}

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

// [0x5247d5] [D] [A]
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

	// NOTE: [A] windowed mode
	if (gWindowed)
	{
		gScreen.handle = CreateWindowEx(0, szAppName, WINDOW_TITLE, WINDOWED_STYLE,
			CW_USEDEFAULT, CW_USEDEFAULT, display_width, display_height, NULL, NULL, hInstance, NULL);

		if (gScreen.handle)
			SetGameWindowSize(gScreen.handle, display_width, display_height);
	}
	else
	{
		gScreen.handle = CreateWindowEx(0, szAppName, WINDOW_TITLE, WS_POPUP,
			0, 0, display_width, display_height, NULL, NULL, hInstance, NULL);
	}

	if (!gScreen.handle)
		return 0;

	ShowWindow(gScreen.handle, nCmdShow);
	return 1;
}

// [A]
// Windowed mode: keeps the window where it is and sizes its client area to width x height.
void SetGameWindowSize(HWND hWnd, int width, int height)
{
	RECT rect;

	rect.left = 0;
	rect.top = 0;
	rect.right = width;
	rect.bottom = height;

	AdjustWindowRect(&rect, WINDOWED_STYLE, FALSE);

	SetWindowPos(hWnd, NULL, 0, 0, rect.right - rect.left, rect.bottom - rect.top, SWP_NOMOVE | SWP_NOZORDER);
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
