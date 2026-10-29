/*
 * glaunch.c - game launch: system init/exit, game init, main game loop
 *
 * Rebuilt from Driver_sym2_94.i64 (IDA folder "glaunch.c"); partially translated.
 *
 * The data chunk at 0x562334 holds a table of the compass point names with no references (left
 * over from a function the linker dropped).
 */

#include <windows.h>
#include <objbase.h>
#include <stdlib.h>

#include "shell.h"
#include "chairs.h"
#include "eax/cdaudio.h"
#include "glaunch.h"
#include "input.h"
#include "pres.h"
#include "sound.h"
#include "map.h"
#include "ai.h"
#include "asmmath.h"
#include "camera.h"
#include "debug.h"
#include "dr_globals.h"
#include "Engine/3dfx/gfxinit.h"
#include "Engine/3dfx/object.h"
#include "Engine/3dfx/polydraw.h"
#include "frontend/frontend.h"
#include "general.h"
#include "lamps.h"
#include "roadbits.h"
#include "testmath.h"

// the loading bar of FlatInit counts this many bytes (about the size of LEVELS\FLAT.LEV)
#define FLAT_LOADING_BYTES		0xE08543

//------------------------------------------------------------------------------------------
// initialised data (0x562334)

char do_clear = 1;
static char *unknown_562338[4] = { "North", "East", "South", "West" };	// NOTE: no references

// [0x4d4fa0] [D]
int MainInit(void)
{
	CoInitialize(NULL);

	InitDirectInput();
	InitStringMng();
	InitSoundDevices();
	InitSoundSystem(gSoundHardware, gSoundCompatibility, gUseEAX);
	InitCDAudio();

	return 1;
}

// [0x4d4fe2] [D]
void MainExit(void)
{
	ShutdownChairs();
	ShutdownDevices();
	ShutdownSoundSystem();
	ShutdownCDAudio();
	DeinitStringMng();

	CoUninitialize();
}

// [0x4d5192] [D]
// Loads the motel room level (LEVELS\FLAT.LEV) behind a loading screen.
int FlatInit(void)
{
	char clear;
	float x;
	float y;
	float x2;
	float y2;

	clear = do_clear;
	do_clear = 0;

	if (gUserLanguage == 5)
		FadeInHiresScreen("DATA\\USLOADING.BMP", 0);
	else
		FadeInHiresScreen("DATA\\LOADING.BMP", 0);

	FreePaletteData();
	LoadPaletteData("LEVELS\\FLAT.D3D");

	gDoInterview = 0;
	gDoTraining = 0;
	gDoLightFlare = 0;
	gNight = 0;
	gDarkNight = 0;
	gDoDawn = 0;
	gDoDusk = 0;

	MissionQuickRender[0] = -1;
	MissionQuickRender[1] = -1;
	MissionNotSoQuickRender[0] = -1;
	MissionNotSoQuickRender[1] = -1;

	frame_skip = 1;
	PlayerCar = 0;
	gPlayerCarModelType = 0;
	gStopPadReads = 0;
	gTicksTillKeysActive = 0;
	NextSyncFrame = 0;
	ifps = 0;
	accumg = 0;
	accumf = 0;

	InitEngineGlobals();
	InitCameraGlobals();

	dwFrames = 0;
	dwFrameTime = 0;
	dwFrameCount = 0;
	game_frame = 0;
	DisplayedFrames = 0;

	moveable_object_mem_originals = NULL;
	moveable_object_mem = NULL;

	srand(12345);
	RandomInit(54321, 217265);

	DestroyLamps();

	// the bar of the 640x480 loading screen, scaled
	x = 65.0f * (float)display_width / 640.0f;
	y = 430.0f * (float)display_height / 480.0f;
	x2 = 575.0f * (float)display_width / 640.0f;
	y2 = 460.0f * (float)display_height / 480.0f;

	// NOTE: IDA shows the byte count as the address of an unnamed bss byte (unk_E08543)
	StartLoadingBarNormal(FLAT_LOADING_BYTES, float_to_int(x), float_to_int(y), (int)(x2 - x), (int)(y2 - y));

	LoadSineTable();

	// NOTE: IDA passes the result of LoadViewArc to InitPackedRegionMem, which takes no arguments
	LoadViewArc();
	InitPackedRegionMem();

	ROADS_InitRoadMapRegions();

	if (!ReadLevelFile())
		return 0;

	DBGPrintCol(240, 240, 160);
	DBGPrintSize(1);

	CalcObjectRotationMatrices(1);
	InitPlayerCarPosition();

	last_camera_angle = 0;
	horizon_angle = 0;
	FrameCount = 0;
	game_frame = 0;

	InitMap();
	InitialiseAI();

	StopLoadingBar();

	gShinyType = 0;
	do_clear = clear;

	return 1;
}
