/*
 * glaunch.c - game launch: system init/exit, game init, main game loop
 *
 * Rebuilt from Driver_sym2_94.i64 (IDA folder "glaunch.c").
 *
 * The data chunk at 0x562334 holds a table of the compass point names with no references (left
 * over from a function the linker dropped).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "shell.h"
#include "dr_time.h"
#include "chairs.h"
#include "eax/cdaudio.h"
#include "eax/alsound.h"
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
#include "handling.h"
#include "Director.h"
#include "frontend/config.h"
#include "D3DGlide/glide_ptrs.h"
#include "carnage.h"
#include "checkpoint.h"
#include "compiler.h"
#include "cops.h"
#include "credits.h"
#include "debris.h"
#include "denting.h"
#include "DirectorStorage.h"
#include "dr_cars.h"
#include "effects.h"
#include "event.h"
#include "felony.h"
#include "FMV/render.h"
#include "gsound.h"
#include "interview.h"
#include "job_fx.h"
#include "minicutscene.h"
#include "mirror.h"
#include "motion_c.h"
#include "MScript.h"
#include "objanim.h"
#include "objcoll.h"
#include "overlay.h"
#include "pause.h"
#include "pedest.h"
#include "replays.h"
#include "shadow.h"
#include "smashable.h"
#include "frontend/cheats.h"
#include "frontend/gvars.h"

void Game_Control(void);
void Game_Draw(void);
void DisplayFrameRate(void);

// the loading bar of FlatInit counts this many bytes (about the size of LEVELS\FLAT.LEV)
#define FLAT_LOADING_BYTES		0xE08543

//------------------------------------------------------------------------------------------
// initialised data (0x562334)

char do_clear = 1;
static char *unknown_562338[4] = { "North", "East", "South", "West" };	// NOTE: no references

// the felony names of the debug text (Game_Draw)
// NOTE: defined at 0x5620d8, before glaunch.c's data; only Game_Draw uses it
static char *felony_names[11] =
{
	"Driving on the sidewalk",
	"Terrorising pedestrians",
	"Speeding",
	"Shooting a red light",
	"Collision with another vehicle",
	"Collision with a police vehicle",
	"Destruction of property",
	"FEDERAL OFFENCE",
	"Driving the wrong way",
	"Driving without lights",
	"Driving recklessly",
};

// NOTE: no name or other references; GameInit clears it with the frontend's options
int dword_B9F5BC;

// NOTE: moved here from the generated stubs; no IDA mark, the owner is a guess from the users
// and the neighbouring data
int gGameLoading;
int gDoLightning;
int gDoDawn;
int gDoDusk;
int gDoRain;
int gDoSnow;
int dwFrameTime;
int dwFrames;
int dwFrameCount;
char do_fogging;
int DoingReplay;
int drawing_cut_scene;
int last_camera_angle;
int speed;
int PlayerCar;
int controller_bits;
int gDrawShadows;
int gStopPadReads;
int game_frame;
int ghost_controller_bits;
int gDrawMap;
int LoadedSounds2;
int LoadedSounds1;
int gBobIndex;
int frame_skip;
char ReplayConfigName[32];
int gSinking;
int DisplayedFrames;
int gSinkingTimer;
int gPlayerCarModelType;
int InitialFrames;
int gbGameDemo;

// [0x4d4fa0] [D] [A]
// NOTE: [A] no CoInitialize, nothing is left that needs COM
int MainInit(void)
{
	InitDirectInput();
	InitStringMng();
	InitSoundDevices();
	InitSoundSystem(gSoundHardware, gSoundCompatibility, gUseEAX);
	InitCDAudio();

	return 1;
}

// [0x4d4fe2] [D] [A]
// NOTE: [A] no CoUninitialize, see MainInit. The OpenAL device stays open after the sound system,
// for the music and the videos, so it is closed here once the music is.
void MainExit(void)
{
	ShutdownChairs();
	ShutdownDevices();
	ShutdownSoundSystem();
	ShutdownCDAudio();
	OpenAL_CloseDevice();
	DeinitStringMng();
}

// [0x4d5006] [D]
// NOTE: no callers in the binary: loads level 0 (for a splash screen?)
void SplashInit(void)
{
	GameLevel = 0;

	InitPackedRegionMem();
	ROADS_InitRoadMapRegions();
	ReadLevelFile();
}

// [0x4d5024] [D]
void SplashExit(void)
{
	KillModelMem();
	KillCellData();
	DestroyLamps();
	FreePackedRegionMem();
	ROADS_DeInitRoadMapRegions();

	if (SurfaceData)
	{
		DebugFree((void *)SurfaceData);
		SurfaceData = 0;
	}

	if (tinfo_block)
	{
		DebugFree(tinfo_block);
		tinfo_block = NULL;
	}

	if (gRamTextureRawDataDump)
	{
		DebugFree(gRamTextureRawDataDump);
		gRamTextureRawDataDump = NULL;
	}

	if (MapInfo)
	{
		DebugFree((void *)MapInfo);
		MapInfo = 0;
	}

	if (MapData)
	{
		DebugFree(MapData);
		MapData = NULL;
	}

	if (PackedPVSRegions)
	{
		DebugFree(PackedPVSRegions);
		PackedPVSRegions = NULL;
	}

	if (model_names)
	{
		DebugFree(model_names);
		model_names = NULL;
	}

	if (moveable_object_mem)
	{
		DebugFree(moveable_object_mem);
		moveable_object_mem = NULL;
	}

	if (moveable_object_mem_originals)
	{
		DebugFree(moveable_object_mem_originals);
		moveable_object_mem_originals = NULL;
	}

	if (map_data)
	{
		DebugFree((void *)map_data);
		map_data = 0;
	}
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

// [0x4d5497] [D]
// Sets up a game: replay / director mode, the mission script, sounds, the level, the cars and
// every game system. Returns 0 when a replay or the level fails to load.
int GameInit(void)
{
	char text[256];
	int damage;
	float x;
	float y;
	float x2;
	float y2;
	int bytesToRead;
	int tryItScript;
	int nextScript;
	char saved_clear;
	int demoScript;
	char *demofile;

	if (!exit_flag)
		DeleteWaypoints();

	if (gMissionRecording && (GameLevel == 4 || GameLevel == 5))
		gDoTraining = 1;

	gGameLoading = 1;
	dwFrames = 0;
	dwFrameTime = 0;
	dwFrameCount = 0;
	game_frame = 0;
	DisplayedFrames = 0;
	gPuppyDogCop = 0;
	gBatterPlayer = 0;
	gHeadTurnTarget = 0;

	ReleaseSounds();

	demofile = NULL;

	// the rolling demos
	switch (GameMode)
	{
		case 200:
			gbGameDemo = 1;
			gDoTraining = 1;
			exit_flag = 4;
			demofile = "I_TRAINDEMO0";
			break;
		case 250:
			gbGameDemo = 1;
			gDoTraining = 1;
			exit_flag = 4;
			demofile = "DESERTDEMO1";
			break;
		case 251:
			gbGameDemo = 1;
			gDoTraining = 1;
			exit_flag = 4;
			demofile = "DESERTDEMO2";
			break;
		case 252:
			gbGameDemo = 1;
			gDoTraining = 1;
			exit_flag = 4;
			demofile = "DESERTDEMO3";
			break;
		default:
			gbGameDemo = 0;
			break;
	}

	if (gbGameDemo && !InitReplay(demofile))
		return 0;

	InitialiseDirector();

	if (gbPlayingDemo)
	{
		if (gDoCredits)
			SetDirectorMode(14, 0, 0);
		else
			SetDirectorMode(7, 0, 0);
	}
	else if (exit_flag == 4)
	{
		if (gbGameDemo)
			SetDirectorMode(15, 0, 0);
		else if (Director.DoingReplay)
			SetDirectorMode(13, 0, 0);
		else
			SetDirectorMode(11, 0, 0);
	}
	else if (exit_flag == 5)
	{
		SetDirectorMode(1, 1, 0);
	}
	else
	{
		SetDirectorMode(0, 0, 0);
	}

	gBobIndex = 0;
	saved_clear = do_clear;
	do_clear = 0;

	if (gDoFrontend && !gbGameDemo)
	{
		ResetGameVariables();
		dword_B9F5BC = 0;
		LoadSavedOptions();
		gDoTraining = 0;
		gNight = 0;
		gDoLightning = 0;
	}

	InitLapTimes();

	Reuse_player_data = 0;

	InitReplayGlobals();

	if (DoingReplay && !InitReplay(ReplayConfigName))
		return 0;

	nextScript = gNextScript;
	demoScript = giDemoScript;
	tryItScript = gTryItScript;

	// NOTE: the result is not checked
	LoadMission(GameMode);

	if (gMissionRecording && (GameLevel == 4 || GameLevel == 5))
		gDoTraining = 1;

	if (GameLevel == 4)
	{
		gNextScript = nextScript;
		giDemoScript = demoScript;
		gTryItScript = tryItScript;
	}

	if (GameLevel == 2 || GameLevel == 4)
		gNight = 1;

	LoadedSounds1 = LoadSounds("master.dss", 0);
	LoadedSounds2 = LoadSounds("cops.dss", 0x8000);

	SetSoundSystemEnvironment(1);

	FreePaletteData();

	switch (GameLevel)
	{
		case 0:
			LoadPaletteData("LEVELS\\MIAMI.D3D");
			break;
		case 1:
			LoadPaletteData("LEVELS\\FRISCO.D3D");
			break;
		case 2:
			LoadPaletteData("LEVELS\\LA.D3D");
			break;
		case 3:
			LoadPaletteData("LEVELS\\NEWYORK.D3D");
			break;
		case 4:
			LoadPaletteData("LEVELS\\IVIEW.D3D");
			break;
		case 5:
			LoadPaletteData("LEVELS\\TRAIN.D3D");
			break;
		case 6:
			LoadPaletteData("LEVELS\\NC_01.D3D");
			break;
	}

	if (gUserLanguage == 5)
		FadeInHiresScreen("DATA\\USLOADING.BMP", 0);
	else
		FadeInHiresScreen("DATA\\LOADING.BMP", 0);

	InitCameraGlobals();
	InitEffectGlobals();
	LoadCosmeticsAndDentData();
	InitCarCosmetics();
	InitEventGlobals();

	CutSceneActive = 0;
	StartCutScene = 0;
	CutToCar = 0;

	gPlayerCarDamage = 0;
	player_damage_factor = 100;

	controller_bits = CAR_PAD_HANDBRAKE;
	ghost_controller_bits = 0;

	speed = 0;
	frame_skip = 1;

	PlayerCar = 0;

	gStopPadReads = 0;
	gSinkingTimer = 180;

	CurrentRoad = -1;
	CurrentJunction = -1;

	gTicksTillKeysActive = 0;
	NextSyncFrame = 0;
	ifps = 0;
	accumg = 0;
	accumf = 0;

	InitEngineGlobals();

	moveable_object_mem_originals = NULL;
	moveable_object_mem = NULL;
	num_moveable_objects = 0;

	gLastModelCollisionCheck = 0;

	srand(12345);
	RandomInit(54321, 217265);

	if (gDoCrates)
		gPlayerCarModelType = 1;

	DestroyLamps();

	if (!gDoFrontend)
	{
		gDoTexturedTyreTracks = 1;
		gDoTyreTracks = 1;
		gDoSmoke = 1;
		gDrawMap = 1;
		gDoShinyCars = 1;
		gDrawShadows = 1;

		DrawSubmergedObjects = 1;

		gDoDebris = 1;

		far_clip = 27000.0f;
		mirror_far_clip = 27000.0f;
		screen_scale = 1.0f;
	}

	ResetViewport(0, 0, 0);

	// the bar of the 640x480 loading screen, scaled
	x = 65.0f * (float)display_width / 640.0f;
	y = 430.0f * (float)display_height / 480.0f;
	x2 = 575.0f * (float)display_width / 640.0f;
	y2 = 460.0f * (float)display_height / 480.0f;

	// the loading bar counts this many bytes (about the size of the level's LEV file)
	switch (GameLevel)
	{
		case 1:
			bytesToRead = 0x1B31C4C;
			break;
		case 2:
			bytesToRead = 0x160D04A;
			break;
		case 3:
			bytesToRead = 0x19B16A9;
			break;
		case 4:
			bytesToRead = 0x9F3750;
			break;
		case 5:
			bytesToRead = 0xC68F60;
			break;
		case 6:
			bytesToRead = 0x528FB4;
			break;
		default:
			bytesToRead = 0x161712A;
			break;
	}

	StartLoadingBarNormal(bytesToRead, float_to_int(x), float_to_int(y), (int)(x2 - x), (int)(y2 - y));

	LoadSineTable();

	// NOTE: IDA passes the result of LoadViewArc to InitPackedRegionMem, which takes no arguments
	LoadViewArc();
	InitPackedRegionMem();

	ROADS_InitRoadMapRegions();

	if (!ReadLevelFile())
		return 0;

	InitialiseDirectorMenus();

	DBGPrintCol(240, 240, 160);
	DBGPrintSize(1);

	CalcObjectRotationMatrices(1);

	BuildJunctionPingInfo();

	InitTyreTracks();
	InitialiseDebris();
	InitShadow();

	MissionNumber = GameLevel;

	InitReplayFiles(GameLevel, GameLevel);
	InitPlayerCarPosition();
	InitCops();
	InitialiseDenting();
	InitHubcap();
	ROADS_InitTrafficLights();
	ResetBoxes();
	InitMotionCaptureData();
	InitAnimatingObjects();
	InitCarnage();

	last_camera_angle = 0;
	horizon_angle = 0;

	FrameCount = 0;
	game_frame = 0;

	if (ReplayMode == 2)
		InitReplayCars();

	if (gDoRain || gDoSnow)
		wetness = 10000;
	else
		wetness = 0;

	if (gMissionRecording || gMissionSetting || gMissionReplaying)
		max_pedestrians = 16;

	InitPedestrians();

	if (Director.Mode == 0)
		InitialiseFlyBy();

	InitMap();
	InitialiseAI();
	InitialiseCarHandling();

	if (GameLevel == 4 || GameLevel == 5)
		InitInterview();

	InitCheckPointMode();

	if (gRecordTraining)
		InitRecTrainingModeCounter();

	if (gNight)
		SetupLightning();

	if (gDoRain)
	{
		SetupRain(&gNormalRainList);

		gRainList = &gNormalRainList;
		gDoSnow = 0;
	}
	else if (gDoSnow)
	{
		SetupSnow(&gNormalRainList);

		gRainList = &gNormalRainList;
	}

	InitHorizon();

	if (gMissionReplaying)
		InitMissionReplay();

	TextColour(255, 255, 0);

	drawing_cut_scene = 0;

	StopLoadingBar();

	CalculateMirrorViewArc(15.0f);
	CalculateNormalViewArc();

	MissionHandler();

	CompileEventScript();

	if (gDoDawn)
	{
		ChangeSkyRGB(gDawnRed, gDawnGreen, gDawnBlue);
	}
	else if (gDoDusk)
	{
		ChangeSkyRGB(gDuskRed, gDuskGreen, gDuskBlue);
	}
	else if (gNight)
	{
		if (GameLevel == 4)
			ChangeSkyRGB(0, 0, 0);
		else
			ChangeSkyRGB(gNightRed, gNightGreen, gNightBlue);
	}
	else if (gDoRain)
	{
		ChangeSkyRGB(154, 154, 154);
	}

	InitJobFXGlobals();

	if (gPlayerForceDamage)
	{
		// the forced damage spread over the six damage zones
		damage = gPlayerForceDamage / 6;

		car_data[0].damage[0] += damage;
		car_data[0].damage[1] += damage;
		car_data[0].damage[2] += damage;
		car_data[0].damage[3] += damage;
		car_data[0].damage[4] += damage;
		car_data[0].damage[5] += damage;
		car_data[0].totalDamage += 6 * damage;

		gPlayerForceDamage = 0;

		ApplyDentingOnCar(-1);
	}
	else if (Reuse_player_data)
	{
		if (!exit_flag)
		{
			LogQuick("GAMEMAIN: Reusing player position (MStart = MNext)");
			MStart = MNext;
		}

		memcpy(car_data[0].damage, MStart.damage, sizeof(car_data[0].damage));
		car_data[0].totalDamage = MStart.totalDamage;

		ApplyDentingOnCar(-1);
	}

	do_clear = saved_clear;
	viewport_reset = 1;

	if (SaveTrail)
		InitSaveTrail();

	if (TestTrail)
		InitTestTrail();

	SetFogType();

	if (ReplayMode == 0)
		ReplayMode = 1;

	if (ReplayMode == 1)
	{
		if (gMissionRecording)
		{
			TargetCar = 65;
			RecordingCar = 1;
		}
		else
		{
			TargetCar = 0;
			RecordingCar = 0;
			gBuildKeyFrames = 1;
		}

		InitPadRecording();
	}

	if (gDoCredits)
		InitCredits();

	if (gMissionReplaying && !gDoTraining && !gDoInterview)
		DMSInitTimerFrames(mission.timer, NumReplayPads);

	if (gUseCDAudio)
		PlayCDTrack(GameLevel + 4);

	UnPauseSounds();
	StartGameSounds();

	if (GamePaused)
		PauseSounds();

	WaitForSync(0);

	gShinyType = 0;
	grTexFilterMode(GR_TMU0, GR_TEXTUREFILTER_BILINEAR, GR_TEXTUREFILTER_BILINEAR);

	gGameLoading = 0;
	viewport_reset = 2;

	if (gMissionRecording)
		gAutoBrake = 1;

	sprintf(text, "Interview %d, Training %d, mode %d, chaseHQmode %d", gDoInterview, gDoTraining, gTrainingMode, gChaseHQMode);
	LogQuick(text);

	return 1;
}

// [0x4d625a] [D]
// Frees the level and the game systems; saves the player's car (at rest) to STARTPOS.X.
void GameExit(void)
{
	FILE *fp;

	fp = fopen("STARTPOS.X", "wb");

	if (fp)
	{
		car_data[0].totalDamage = 0;
		car_data[0].damage[5] = 0;
		car_data[0].damage[4] = 0;
		car_data[0].damage[3] = 0;
		car_data[0].damage[2] = 0;
		car_data[0].damage[1] = 0;
		car_data[0].damage[0] = 0;
		car_data[0].wheel[3].susCompression = 0;
		car_data[0].wheel[2].susCompression = 0;
		car_data[0].wheel[1].susCompression = 0;
		car_data[0].wheel[0].susCompression = 0;
		car_data[0].thrust = 0;
		car_data[0].wheel_angle = 0;
		car_data[0].wheel_speed = 0;
		car_data[0].front_vel = 0;
		car_data[0].rear_vel = 0;
		car_data[0].speed = 0;
		car_data[0].st.n.linearVelocity[0] = 0;
		car_data[0].st.n.linearVelocity[1] = 0;
		car_data[0].st.n.linearVelocity[2] = 0;
		car_data[0].st.n.angularVelocity[0] = 0;
		car_data[0].st.n.angularVelocity[1] = 0;
		car_data[0].st.n.angularVelocity[2] = 0;
		car_data[0].acc[0] = 0;
		car_data[0].acc[1] = GRAVITY;
		car_data[0].acc[2] = 0;
		car_data[0].aacc[0] = 0;
		car_data[0].aacc[1] = 0;
		car_data[0].aacc[2] = 0;

		fwrite(car_data, 1, sizeof(CAR_DATA), fp);

		fclose(fp);
	}

	FreePaletteData();
	KillModelMem();
	KillCellData();
	FreePackedRegionMem();
	ROADS_DeInitRoadMapRegions();

	if (ReplayMode || gDoTraining)
		DeInitPadPlayRec();

	if (gUseCDAudio)
		StopCDTrack();

	FreeTextDents();

	if (SurfaceData)
	{
		DebugFree((void *)SurfaceData);
		SurfaceData = 0;
	}

	if (tinfo_block)
	{
		DebugFree(tinfo_block);
		tinfo_block = NULL;
	}

	if (gRamTextureRawDataDump)
	{
		DebugFree(gRamTextureRawDataDump);
		gRamTextureRawDataDump = NULL;
	}

	if (MapInfo)
	{
		DebugFree((void *)MapInfo);
		MapInfo = 0;
	}

	if (MapData)
	{
		DebugFree(MapData);
		MapData = NULL;
	}

	if (PackedPVSRegions)
	{
		DebugFree(PackedPVSRegions);
		PackedPVSRegions = NULL;
	}

	if (matrix_mem)
	{
		DebugFree(matrix_mem);
		matrix_mem = NULL;
	}

	if (model_names)
	{
		DebugFree(model_names);
		model_names = NULL;
	}

	if (moveable_object_mem)
	{
		DebugFree(moveable_object_mem);
		moveable_object_mem = NULL;
	}

	if (moveable_object_mem_originals)
	{
		DebugFree(moveable_object_mem_originals);
		moveable_object_mem_originals = NULL;
	}

	if (map_data)
	{
		DebugFree((void *)map_data);
		map_data = 0;
	}

	if (normal_view_arc)
	{
		DebugFree(normal_view_arc);
		normal_view_arc = NULL;
	}

	DestroyLamps();
	FreeMotionCaptureData();
	DestroyEvents();
	ShutdownDirector();
	ReleaseGameSounds();
	KillMirrorViewArc();

	if (mission.objective == 0x8000)
		PlayRender(MissionNotSoQuickRender[0], MissionNotSoQuickRender[1]);

	MissionNotSoQuickRender[0] = -1;
	MissionNotSoQuickRender[1] = -1;

	DestroyMissionScript();

	if (SaveTrail)
		ExitSaveTrail();

	if (TestTrail)
		ExitTestTrail();

	steeringTorque = 0;

	UpdateForces();
}

// [0x4d6655] [D]
// One pass of the game loop: the first call runs two frames to start; then control frames to
// catch up with the frame rate (five times as many in fast forward) and one drawn frame.
void GameLoop(void)
{
	int paused;
	int frames;

	CheckPause();

	if (game_frame == 0)
	{
		paused = GamePaused;
		InitialFrames = 1;

		PlaceCameraInCar(0);

		PauseGame(0);
		CheckPause();

		StartOfEffectsFrame();
		Game_Control();
		Game_Draw();

		game_frame = 1;
		DisplayedFrames = 1;

		StartOfEffectsFrame();
		Game_Control();
		Game_Draw();

		game_frame = 2;
		DisplayedFrames = 2;
		GamePaused = paused;
		InitialFrames = 0;
		return;
	}

	CheckPause();

	StartOfEffectsFrame();
	Game_Control();

	if (!quit)
		return;

	frames = WaitForSync(0);

	if (Director.Mode == 6)
		frames *= 5;

	while (frames > 1)
	{
		if (!quit)
			return;

		StartOfEffectsFrame();
		Game_Control();

		if (!quit)
			return;

		frames--;
	}

	if (gUseCDAudio && !gMusicPlaying && !GamePaused)
		PlayCDTrack(GameLevel + 4);

	ReallyUpdateSound();

	if (gScreen.active)
	{
		Game_Draw();
		DisplayedFrames++;
	}

	if (EndofFlyBy)
		FlyBy = 0;
}

// [0x4d67cc] [D]
// One game frame: input, camera, the map's objects, events, pedestrians, the cars and their
// physics, the AI and cops, effects, the mission script and the sound.
void Game_Control(void)
{
	FPUMaintain();
	ReadInput();

	if (!FlyBy)
	{
		if (game_frame >= 2)
			CheckMiscKeys();

		HandleDirector();
	}

	if (GamePaused)
	{
		arc_view = view_arc[0];

		ControlCamera(0);
		SetupFrustrum();
		DrawMap(0);
		HandleOverlays();
		PauseKeyControl();
		UpdateSound();
	}
	else
	{
		if (!gStopPadReads && (gDoTraining || ReplayMode != 2))
			FillPad(&controller_bits);

		// the player's car stops once the pad is off or the chase is over
		if (gStopPadReads || LeadCarFinished || LeadCarLost || LeadCarHit)
		{
			controller_bits = CAR_PAD_HANDBRAKE;

			if (car_data[0].wheel_speed > 0)
				controller_bits |= CAR_PAD_BRAKE;
			else if (car_data[0].wheel_speed < 0)
				controller_bits |= CAR_PAD_ACCEL;
		}

		arc_view = view_arc[0];
		num_active_cars = 0;

		if (mission.pause_timer && --mission.pause_timer == 0)
		{
			if (gDoInterview && IsSpeechPlaying())
				mission.pause_timer = 50;
			else
				PauseGame(1);
		}

		ControlMiniCutScene(0);

		if (FlyBy)
			RunFlyBy();
		else
			ControlCamera(0);

		SetupFrustrum();

		if (gDoCredits)
			ControlCredits();

		DrawMap(0);

		if (gDoDawn)
			DoDawn();

		if (gDoDusk)
			DoDusk();

		AddGroundDebris();

		if (!FlyBy || InitialFrames)
			EventHandler();

		MoveSmashable_object();

		if (gDoChopper)
			ControlChopper(0);

		if (gDoCrates)
			ControlCrate();

		if (!FlyBy || InitialFrames)
		{
			HandleChairs();
			HandlePedestrians();
			CheckCopsWithPedestrians();
		}

		if (gDoSnow)
		{
			AddSnow();
			HandleSnow();
		}

		gBobIndex = (gBobIndex + 60) & 0xFFF;

		if (gSinking && mission.objective != 4080)
		{
			gSinkingTimer--;

			gCameraAngle = (gCameraAngle - 22) & 0xFFF;
			cameraview = 0;

			if (gSinkingTimer < 0)
			{
				exit_code = 100;

				mission.objective = 4080;
				mission.failed_reason = 5;

				PauseGame(1);
			}
		}

		if (!FlyBy || InitialFrames)
		{
			ControlCars();

			if (gDoTraining)
			{
				DoScenaryCollisions();
			}
			else
			{
				ControlAI();
				DoScenaryCollisions();
				ControlCops();
				CheckPlayerMiscFelonies(&gFelonyData);
				ROADS_ControlTrafficLights();
			}
		}

		CheckCarEffects(0);

		if (gMissionRecording || gMissionReplaying)
			CheckOtherCarEffects(65);

		if (FlyBy && !InitialFrames)
			steeringTorque = 0;
		else
			GlobalTimeStep();

		FailIfPlayerUpsideDown();
		ProcessDentQueue();
		CalcPlayerCarDamage();
		DoNeonColourCycle();

		if (gDoRain)
		{
			AddRainDrops();
			HandleRainDrops();
		}

		HandleEffects();

		if (!FlyBy || InitialFrames)
			MissionHandler();

		HandleTrailBlazer(pTrailData);
		HandleOverlays();

		if (SaveTrail)
			ControlSaveTrail();

		HandleCrossouts();

		if (!FlyBy)
			game_frame++;

		SoundTasks();
		UpdateSound();
	}

	CheckPause();
}

// [0x4d6b96] [D]
// Draws a game frame: the map, cars, effects, overlays and the debug text.
void Game_Draw(void)
{
	char text[256];
	int submerged;
	char fogging;

	fogging = do_fogging;

	if (!gFogAllowed)
		do_fogging = 0;

	arc_view = view_arc[0];
	StartOfFrame();

	if (!gbFullscreenMap)
	{
		FlushLamps();

		if (gDoRain)
			gNumLightReflections = 0;

		if (!FlyBy)
			ControlCamera(1);

		SetupFrustrum();

		if (gNight)
			DoNightSpecificStuff();

		if (TriggerBomb)
			DoExplosionLamps();

		if (gRealTimeLights == 1)
			ProcessLamps();

		DrawTargetArrows();
		DrawMap(1);
		DrawMoveableObjects();
		DrawSmashable_sprites(0);

		SetupCarShine();
		DrawCar(PlayerCar, -1);

		if (gDoCrates)
			DrawCrate();

		if (do_fogging)
			grFogMode(GR_FOG_DISABLE);

		DrawTyreTracks();

		if (gMissionRecording || gMissionReplaying)
			DrawOtherTyreTracks();

		if (do_fogging)
			grFogMode(GR_FOG_WITH_TABLE);

		DrawCivilianCars();
		DrawCarShine();

		ControlMiniCutScene(1);

		DisplayEvents();

		if (!TriggerBomb)
		{
			DisplayPedestrians();
			DrawAllPedShadows();
		}

		DrawTargetShadows();

		submerged = 0;

		if (do_fogging)
		{
			submerged = DrawSubmergedObjects;
			DrawSubmergedObjects = 0;
		}

		DrawWater();

		if (do_fogging)
			DrawSubmergedObjects = submerged;

		DrawHorizon();
		DrawSmashable_sprites(1);

		if (do_fogging)
			grFogMode(GR_FOG_DISABLE);

		if (gDoSnow)
			DrawSnow();

		if (gDoRain)
			DrawRainDrops();

		if (do_fogging)
			grFogMode(GR_FOG_WITH_TABLE);

		if (gDoChopper)
			ControlChopper(1);

		DisplayEffects();
		DrawCarPools();

		if (gDoRain)
			DisplayLightReflections();

		if (TriggerBomb)
			DrawExplosion();

		DrawTransparentObjects();
		DrawTrailblazer(pTrailData);

		if (mission.loaded)
		{
			DisplayCheckpointData(mission.checkpoint);
			Draw3dText();
		}

		if (!do_fogging)
			DrawLensFlare();

		if (!FlyBy || InitialFrames)
			DisplayMissionData();

		if (gDoRearViewMirror && !exit_flag)
			DrawRearViewMirror();

		if (mission.loaded)
			DisplayCheckpointText(mission.checkpoint);

		if (Director.Mode == 0)
			DrawGameSpecificOverlays();

		DisplayTrainingData();

		if (!FlyBy)
			DisplayOverlays();

		if (ReplayMode)
			ShowReplayStatus();

		DrawReplayInterface();
	}

	if (GamePaused)
		RenderInput();

	if (!gbFullscreenMap)
	{
		if (gDoCredits)
			DrawCredits();

		if (!gDisableDebug)
		{
			TextOffset(0, 0);
			AddTextToList("\n\n\n");

			sprintf(text, "Felony rating: %d (%d active cops %d sugg. %d max. )\n", FelonyRating, num_active_cops, suggested_active_cops, max_active_cops);
			AddTextToList(text);

			if (LastFelony != -1)
			{
				if (LastFelony == 2)
					sprintf(text, "Last observed felony: %s (%d)\n", felony_names[2], car_data[0].speed);
				else
					sprintf(text, "Last observed felony: %s\n", felony_names[LastFelony]);

				AddTextToList(text);
			}

			if (gCruiseControlSpeed)
				AddTextToList("\nCruise control active.\n");
		}

		if (!FlyBy)
		{
			DisplayCurrentTime();
			DisplayFrameRate();
		}

		WriteTextFromList();

		if (gSaveScreenShots)
			SaveScreenShot();

		PreLampStreak();
	}

	if (FlyBy)
		FBEndOfFrame();
	else
		EndOfFrame();

	do_fogging = fogging;
}

// [0x4d6fc2] [D]
// The frames per second (counted over a second) in the top left corner.
void DisplayFrameRate(void)
{
	DWORD time;
	char text[32];

	if (gDisableDebug)
		return;

	dwFrameCount++;

	time = GetTicksMs() - dwFrames;

	if (time > 1000)
	{
		dwFrameTime = 1000 * dwFrameCount / time;
		dwFrames = GetTicksMs();
		dwFrameCount = 0;
	}

	sprintf(text, "%dfps", dwFrameTime);

	TextOffset(0, display_height / 8 - 1);
	AddTextToList(text);
}
