/*
 * frontend/config.c - config.dat options, saved games and replays
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "frontend\config.c", 0x405480 - 0x406c06)
 */

#include <stdio.h>
#include <string.h>
#include <direct.h>

#include "dr_types.h"
#include "debris.h"
#include "denting.h"
#include "dr_globals.h"
#include "frontend/cheats.h"
#include "frontend/config.h"
#include "frontend/frontend.h"
#include "general.h"
#include "replays.h"
#include "frontend/gvars.h"
#include "input.h"
#include "shell.h"
#include "sound.h"
#include "asmmath.h"
#include "eax/cdaudio.h"
#include "DirectorStorage.h"
#include "dr_scores.h"
#include "effects.h"
#include "pedest.h"
#include "shadow.h"
#include "Engine/3dfx/gfxinit.h"
#include "MScript.h"
#include "glaunch.h"
#include "mirror.h"
#include "mladder/mladder.h"

typedef struct SAVEDATA_HEADER
{
	int ident;
	int checksum;
} SAVEDATA_HEADER;

// NOTE: name guessed; value from the binary
#define CONFIG_IDENT	0x93587031
#define GAME_IDENT		0x57231356	// NOTE: names guessed; values from the binary
#define REPLAY_IDENT	0x38910457

// SAVES\REPLAYn.BIN chunks
#define CHUNK_CFG		0x21474643	// 'CFG!'
#define CHUNK_CAMS		0x534D4143	// 'CAMS'
#define CHUNK_PADS		0x53444150	// 'PADS'

// NOTE: in the binary this is an inline `fld; fistp INT_FLT; mov eax, INT_FLT` sequence,
// the original macro/inline is unknown

void SetDefaultOptions(void);
void SetGraphicsLevel(int level, int keepDensity);
int GetSaveDataHash(void *data, int size);
void InitSavedData(CONFIG_DATA *data);

int CheckReplay(int slot, char *name);
int StoreReplayReal(int slot);
int RestoreReplayReal(int slot);
int AppendFile(FILE *file, char *filename, int ident);
int ExtractFile(FILE *file, char *filename, int ident);
int FileExists(char *filename);

//------------------------------------------------------------------------------------------
// initialised data (0x54bcc0, start of the file's block per IDA mark)

int wanted_display_width = 640;
int wanted_display_height = 480;

// NOTE: [A] "/RES=", wins over wanted_display_width, wanted_display_height which the options reset
int forced_display_width;
int forced_display_height;

int wanted_display_mode = 2;
int wanted_display_depth = 16;

//------------------------------------------------------------------------------------------
// zero-initialised data (0x57d8f8)
// NOTE: assigned to this file because its functions are the only users; the block may start
// earlier (0x57d8c8: ChaseCarPalette, gScreen, ..., gProcess) and end later

SAVEDATA_HEADER config_header;
int gTripleBuffering;
static int unknown_57D904;		// NOTE: no references
SAVEDATA_HEADER game_header;
char replay_data[56];
SAVEDATA_HEADER replay_header;
int gNo8BitSupport;
int wanted_display_device;
int gDisableConfig;

// NOTE: bss global in the linker's common area; defined here next to its main users
CONFIG_DATA config_data;

// NOTE: bss global in the linker's common area; only this file uses it
GAME_DATA game_data;

// [0x405480] [D]
// Clears the mission and debug switches before a game; the weather stays while exit_flag is set.
void ResetGameVariables(void)
{
	ChaseCarType = 0;
	ChaseCarPalette = 0;
	ghost_mode = 0;
	gDoChopper = 0;
	MissionNumber = 0;
	gCopRespawnTime = 0;
	gDarkNight = 0;
	gDoTraining = 0;
	gDoRave = 0;
	gRecordTraining = 0;
	gMissionRecording = 0;
	gMissionReplaying = 0;
	gMissionSetting = 0;
	gNoPedestrians = 0;
	gBuildKeyFrames = 0;
	gNoCops = 0;
	gDisableDenting = 0;
	TrailSpacing = 0;
	TrailStarted = 0;
	TestTrail = 0;
	SaveTrail = 0;
	gUseCopModel = 1;
	gUseBadGuyModel = 0;
	gChaseHQMode = 0;
	gDoCrates = 0;
	gChaseModeHits = 0;
	gPlayerCarDamage = 0;
	gPlayerForceDamage = 0;
	gChaseModeHitsRequired = 0;
	gSinking = 0;
	gDisableText = 1;
	gDoDawn = 0;
	gDoDusk = 0;
	do_plug = 0;
	gDoLightFlare = 0;
	gDoPlayerHeadlight = 0;

	if (exit_flag == 0)
	{
		gNight = 0;
		gDoRain = 0;
		gDoLightning = 0;
		do_fogging = 0;
	}
}

// [0x405618] [D]
int LoadSavedOptions(void)
{
	FILE *fp;

	if (!gDoFrontend || gDisableConfig)
		return 1;

	fp = fopen("config.dat", "rb");

	if (fp)
	{
		fread(&config_header, 1, sizeof(SAVEDATA_HEADER), fp);

		if (config_header.ident != CONFIG_IDENT)
		{
			fclose(fp);
			SetDefaultOptions();
			return 0;
		}

		fread(&config_data, 1, sizeof(CONFIG_DATA), fp);
		fclose(fp);

		if (GetSaveDataHash(&config_data, sizeof(CONFIG_DATA)) == config_header.checksum)
		{
			wanted_display_device = config_data.display_device;
			wanted_display_mode = config_data.display_mode;
			wanted_display_depth = config_data.display_depth;

			InputDevice = config_data.InputDevice;
			gForceFeedback = config_data.ForceFeedback;
			JoyIsAnalogue = config_data.JoyIsAnalogue;
			JoyDeadZone = config_data.JoyDeadZone;

			memcpy(&UserKeys, config_data.UserKeys, sizeof(UserKeys));
			memcpy(&JoySet, config_data.JoySet, sizeof(JoySet));

			if (InputDevice == 2)
			{
				if (SetJoystickDevice(config_data.JoystickDevice))
				{
					SetJoystickDeadZone();

					if (!JoyIsFF)
						gForceFeedback = 0;
				}
				else
				{
					InputDevice = 0;
				}
			}

			KeySet = UserKeys;

			gDrawShadows = config_data.DrawShadows;
			gDoSmoke = config_data.DoSmoke;
			gDoDebris = config_data.DoDebris;
			gDoRearViewMirror = config_data.DoRearViewMirror;
			gDoTexturedTyreTracks = config_data.DoTexturedTyreTracks;
			gDoTyreTracks = config_data.DoTyreTracks;
			gDoShinyCars = config_data.DoShinyCars;
			gDoLensFlare = config_data.DoLensFlare;
			gDoLightFlare = config_data.DoLightFlare;
			DrawSubmergedObjects = config_data.DrawSubmergedObjects;
			gDrawMap = config_data.DrawMap;
			gAllowSounds = config_data.AllowSounds;
			gUseCDAudio = config_data.UseCDAudio;
			gUseDoppler = config_data.UseDoppler;
			gMasterVolume = config_data.MasterVolume;
			gMusicVolume = config_data.MusicVolume;
			gNumCitiesOpen = config_data.NumCitiesOpen;
			far_clip = config_data.far_clip;
			mirror_far_clip = config_data.mirror_far_clip;
			screen_scale = config_data.screen_scale;
			max_pedestrians = config_data.max_pedestrians;
			gRealTimeLights = config_data.RealTimeLights;
			gUltraLamps = config_data.UltraLamps;
			gFogAllowed = config_data.FogAllowed;
			gDoDetailedShadows = config_data.DoDetailedShadows;
			gDoMultiColouredCars = config_data.DoMultiColouredCars;
			gAllowFMV = config_data.AllowFMV;
			gAutoBrake = config_data.AutoBrake;
			gDifficulty = config_data.Difficulty;
			gDoInterpolation = config_data.DoInterpolation;
			gDoAutoInterpolation = config_data.DoAutoInterpolation;
			max_frame_skip = config_data.max_frame_skip;
			gDetailLevel = config_data.DetailLevel;
			gAudioMixerBits = config_data.AudioMixerBits;
			gAudioMixerFreq = config_data.AudioMixerFreq;
			gAudioSpeakerMode = config_data.AudioSpeakerMode;
			memcpy(gCheatsAllowed, config_data.CheatsAllowed, sizeof(gCheatsAllowed));
			gTrafficDensity = config_data.TrafficDensity;
			gNightBrightness = config_data.NightBrightness;
			gSoundHardware = config_data.SoundHardware;
			gSoundCompatibility = config_data.SoundCompatibility;
			gUseEAX = config_data.UseEAX;
			gTripleBuffering = config_data.TripleBuffering;
			gNo8BitSupport = config_data.No8BitSupport;

			wanted_display_width = display_modes[wanted_display_mode].width;
			wanted_display_height = display_modes[wanted_display_mode].height;

			if (gSoundHardware >= num_soundcards)
				gSoundHardware = 0;

			if (gMasterVolume < 0.0f || gMasterVolume > 1.0f)
				gMasterVolume = 0.5f;

			if (gMusicVolume < 0.0f || gMusicVolume > 1.0f)
				gMusicVolume = 0.5f;

			if (gAudioMixerBits != 8 && gAudioMixerBits != 16)
				gAudioMixerBits = 8;

			if (gAudioMixerFreq != 22050 && gAudioMixerFreq != 44100)
				gAudioMixerFreq = 22050;

			SetEffectsVolume(gMasterVolume);
			SetCDVolume(float_to_int(gMusicVolume * 65535.0f));
			SetSoundSystemFormat(gAudioMixerBits, gAudioMixerFreq);
			SetSoundSystemMode(gAudioSpeakerMode);

			return 1;
		}
	}

	SetDefaultOptions();
	return 0;
}

// [0x405b19] [D]
void SetDefaultOptions(void)
{
	wanted_display_mode = 2;
	wanted_display_width = display_modes[2].width;
	wanted_display_height = display_modes[2].height;

	InputDevice = 0;

	KeySet = DefaultKeySet;
	UserKeys = DefaultKeySet;
	memcpy(&JoySet, &DefaultJoySet, sizeof(JoySet));

	gAllowSounds = 1;
	gUseCDAudio = 1;
	gUseDoppler = 1;
	gMasterVolume = 1.0f;
	gMusicVolume = 1.0f;
	gNumCitiesOpen = 2;
	screen_scale = 1.0f;
	gForceFeedback = 0;
	max_frame_skip = 1;
	gAllowFMV = 1;
	gAutoBrake = 1;
	gDifficulty = 1;
	gDoInterpolation = 1;
	gDoAutoInterpolation = 1;
	gAudioMixerBits = 8;
	gAudioMixerFreq = 22050;
	gAudioSpeakerMode = 0;

	SetGraphicsLevel(1, 1);
}

// [0x405c55] [D]
void UpdateSavedOptions(void)
{
	FILE *fp;

	if (!gDoFrontend || gDisableConfig)
		return;

	InitSavedData(&config_data);

	config_header.ident = CONFIG_IDENT;
	config_header.checksum = GetSaveDataHash(&config_data, sizeof(CONFIG_DATA));

	fp = fopen("config.dat", "wb");

	if (fp)
	{
		fwrite(&config_header, 1, sizeof(SAVEDATA_HEADER), fp);
		fwrite(&config_data, 1, sizeof(CONFIG_DATA), fp);
		fclose(fp);
	}
}

// [0x405d0f] [D]
// Loads SAVES\GAMEn.BIN into game_data; 1 when it is a valid saved game.
int LoadSavedGame(int slot)
{
	FILE *f;
	char filename[20];

	sprintf(filename, "SAVES\\GAME%d.BIN", slot + 1);

	f = fopen(filename, "rb");

	if (f == NULL)
		return 0;

	fread(&game_header, 1, sizeof(SAVEDATA_HEADER), f);
	fread(&game_data, 1, sizeof(GAME_DATA), f);
	fclose(f);

	return game_header.ident == GAME_IDENT && GetSaveDataHash(&game_data, sizeof(GAME_DATA)) == game_header.checksum;
}

// [0x405dbb] [D]
// Saves the progress under the given name to SAVES\GAMEn.BIN.
// NOTE: name is overwritten with the file name (as in the original)
void UpdateSavedGame(int slot, char *name)
{
	FILE *f;
	int checksum;

	memset(&game_data, 0, sizeof(GAME_DATA));
	strcpy(game_data.name, name);

	game_data.gNumCitiesOpen = gNumCitiesOpen;
	game_data.mladder_index = mladder.command - mladder.command_list;
	game_data.mladder_level = mladder.level;
	game_data.mladder_flags = mladder.flags;
	game_data.gStoredPlayerCarType = gStoredPlayerCarType;

	checksum = GetSaveDataHash(&game_data, sizeof(GAME_DATA));
	game_header.ident = GAME_IDENT;
	game_header.checksum = checksum;

	_mkdir("SAVES");
	sprintf(name, "SAVES\\GAME%d.BIN", slot + 1);

	f = fopen(name, "wb");

	if (f)
	{
		fwrite(&game_header, 1, sizeof(SAVEDATA_HEADER), f);
		fwrite(&game_data, 1, sizeof(GAME_DATA), f);
		fclose(f);
	}
}

// [0x405ed4] [D]
// 1 and the saved game's name when SAVES\GAMEn.BIN is a valid saved game.
int CheckSaveGame(int slot, char *name)
{
	FILE *f;
	char filename[20];
	int checksum;

	sprintf(filename, "SAVES\\GAME%d.BIN", slot + 1);

	f = fopen(filename, "rb");

	if (f == NULL)
		return 0;

	fread(&game_header, 1, sizeof(SAVEDATA_HEADER), f);
	fread(&game_data, 1, sizeof(GAME_DATA), f);
	fclose(f);

	if (game_header.ident != GAME_IDENT)
		return 0;

	checksum = GetSaveDataHash(&game_data, sizeof(GAME_DATA));

	if (checksum != game_header.checksum)
		return 0;

	strcpy(name, game_data.name);
	return 1;
}

// [0x405f89] [D]
// Unpacks SAVES\REPLAYn.BIN into the $TEMP$ replay files and loads them.
int LoadSavedReplay(int slot)
{
	if (CheckReplay(slot, NULL) == 0)
		return 0;

	if (RestoreReplayReal(slot) == 0)
		return 0;

	InitReplay("$TEMP$");
	return 1;
}

// [0x405fca] [D]
// Packs the $TEMP$ replay files under the given name into SAVES\REPLAYn.BIN.
int UpdateSavedReplay(int slot, char *name)
{
	int checksum;

	memset(replay_data, 0, sizeof(replay_data));
	strcpy(replay_data, name);

	checksum = GetSaveDataHash(replay_data, sizeof(replay_data));
	replay_header.ident = REPLAY_IDENT;
	replay_header.checksum = checksum;

	return StoreReplayReal(slot);
}

// [0x406036] [D]
// 1 (and the replay's name unless name is NULL) when SAVES\REPLAYn.BIN is a valid replay.
int CheckReplay(int slot, char *name)
{
	FILE *f;
	char filename[32];
	int checksum;

	sprintf(filename, "SAVES\\REPLAY%d.BIN", slot + 1);

	f = fopen(filename, "rb");

	if (f == NULL)
		return 0;

	fread(&replay_header, 1, sizeof(SAVEDATA_HEADER), f);
	fread(replay_data, 1, sizeof(replay_data), f);
	fclose(f);

	if (replay_header.ident != REPLAY_IDENT)
		return 0;

	checksum = GetSaveDataHash(replay_data, sizeof(replay_data));

	if (checksum != replay_header.checksum)
		return 0;

	if (name)
		strcpy(name, replay_data);

	return 1;
}

// [0x4060f1] [D]
// SAVES\REPLAYn.BIN: header, name, then the config, cameras and pads files as chunks; a failed
// write removes the file.
int StoreReplayReal(int slot)
{
	FILE *f;
	char filename[32];

	sprintf(filename, "SAVES\\REPLAY%d.BIN", slot + 1);

	f = fopen(filename, "wb");

	if (f == NULL)
		return 0;

	fwrite(&replay_header, 1, sizeof(SAVEDATA_HEADER), f);
	fwrite(replay_data, 1, sizeof(replay_data), f);

	if (AppendFile(f, "REPLAYDATA\\USER\\$TEMP$.CFG", CHUNK_CFG))
	{
		if (FileExists("REPLAYDATA\\USER\\$TEMP$.CAMS") == 0)
			SaveDirectorCameras("REPLAYDATA\\USER\\$TEMP$.CAMS");

		if (AppendFile(f, "REPLAYDATA\\USER\\$TEMP$.CAMS", CHUNK_CAMS) &&
			AppendFile(f, "REPLAYDATA\\USER\\$TEMP$.PADS", CHUNK_PADS))
		{
			fclose(f);
			return 1;
		}
	}

	fclose(f);
	remove(filename);

	return 0;
}

// [0x4061f4] [D]
int RestoreReplayReal(int slot)
{
	FILE *f;
	char filename[32];

	sprintf(filename, "SAVES\\REPLAY%d.BIN", slot + 1);

	f = fopen(filename, "rb");

	if (f == NULL)
		return 0;

	fread(&replay_header, 1, sizeof(SAVEDATA_HEADER), f);
	fread(replay_data, 1, sizeof(replay_data), f);

	if (ExtractFile(f, "REPLAYDATA\\USER\\$TEMP$.CFG", CHUNK_CFG) &&
		ExtractFile(f, "REPLAYDATA\\USER\\$TEMP$.CAMS", CHUNK_CAMS) &&
		ExtractFile(f, "REPLAYDATA\\USER\\$TEMP$.PADS", CHUNK_PADS))
	{
		fclose(f);
		return 1;
	}

	fclose(f);
	return 0;
}

// [0x4062cd] [D]
// Writes the whole file to file as a chunk: ident, size, data.
int AppendFile(FILE *file, char *filename, int ident)
{
	void *data;
	size_t size;
	FILE *f;

	f = fopen(filename, "rb");

	if (f == NULL)
		return 0;

	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fseek(f, 0, SEEK_SET);

	data = DebugMalloc(size, "D:\\Driver\\game6.4g\\frontend\\config.c", 569);

	if (data == NULL)
	{
		fclose(f);
		return 0;
	}

	fread(data, 1, size, f);
	fclose(f);

	fwrite(&ident, 1, 4, file);
	fwrite(&size, 1, 4, file);
	fwrite(data, 1, size, file);

	DebugFree(data);
	return 1;
}

// [0x4063c4] [D]
// Reads the next chunk of file into filename if it has the given ident.
int ExtractFile(FILE *file, char *filename, int ident)
{
	int chunk;
	size_t size;
	void *data;
	FILE *f;

	fread(&chunk, 1, 4, file);

	if (ident != chunk)
		return 0;

	fread(&size, 1, 4, file);

	data = DebugMalloc(size, "D:\\Driver\\game6.4g\\frontend\\config.c", 598);

	if (data == NULL)
		return 0;

	fread(data, 1, size, file);

	f = fopen(filename, "wb");

	if (f == NULL)
	{
		DebugFree(data);
		return 0;
	}

	fwrite(data, 1, size, f);
	fclose(f);

	DebugFree(data);
	return 1;
}

// [0x406494] [D]
void LoadSavedScores(void)
{
	FILE *f;

	InitialiseHighScores();

	f = fopen("SAVES\\SCORES.HI", "rb");

	if (f)
	{
		LoadHighScores(f);
		fclose(f);
	}
}

// [0x4064d4] [D]
void UpdateSavedScores(void)
{
	FILE *f;

	f = fopen("SAVES\\SCORES.HI", "wb");

	if (f)
	{
		SaveHighScores(f);
		fclose(f);
	}
}

// [0x40650f] [D]
// NOTE: Hex-Rays fails on the switch; translated from the disassembly
void SetGraphicsLevel(int level, int keepDensity)
{
	switch (level)
	{
		case 0:
			gDrawShadows = 1;
			gDoDetailedShadows = 0;
			gDoSmoke = 0;
			gDoDebris = 0;
			gDoLensFlare = 0;
			gDoLightFlare = 0;
			gDoRearViewMirror = 0;
			gDoShinyCars = 0;
			gDoMultiColouredCars = 0;
			gDoTexturedTyreTracks = 0;
			gDoTyreTracks = 0;
			gRealTimeLights = 0;
			gUltraLamps = 0;
			gDrawMap = 1;
			gFogAllowed = 0;
			DrawSubmergedObjects = 0;
			mirror_far_clip = 15000.0f;
			far_clip = 15000.0f;

			if (!keepDensity)
			{
				max_pedestrians = 8;
				gTrafficDensity = 16;
			}
			break;

		case 1:
			gDrawShadows = 1;
			gDoDetailedShadows = 1;
			gDoSmoke = 1;
			gDoDebris = 1;
			gDoLensFlare = 0;
			gDoLightFlare = 0;
			gDoRearViewMirror = 0;
			gDoShinyCars = 1;
			gDoMultiColouredCars = 1;
			gDoTexturedTyreTracks = 1;
			gDoTyreTracks = 1;
			gRealTimeLights = 0;
			gUltraLamps = 0;
			gDrawMap = 1;
			gFogAllowed = 0;
			DrawSubmergedObjects = 0;
			mirror_far_clip = 15000.0f;
			far_clip = 30000.0f;

			if (!keepDensity)
			{
				max_pedestrians = 16;
				gTrafficDensity = 32;
			}
			break;

		case 2:
			gDrawShadows = 1;
			gDoDetailedShadows = 1;
			gDoSmoke = 1;
			gDoDebris = 1;
			gDoLensFlare = 1;
			gDoLightFlare = 1;
			gDoRearViewMirror = 1;
			gDoShinyCars = 1;
			gDoMultiColouredCars = 1;
			gDoTexturedTyreTracks = 1;
			gDoTyreTracks = 1;
			gRealTimeLights = 1;
			gUltraLamps = 0;
			gDrawMap = 1;
			gFogAllowed = 1;
			DrawSubmergedObjects = 1;
			mirror_far_clip = 30000.0f;
			far_clip = 45000.0f;

			if (!keepDensity)
			{
				max_pedestrians = 32;
				gTrafficDensity = 48;
			}
			break;

		case 3:
			gDrawShadows = 1;
			gDoDetailedShadows = 1;
			gDoSmoke = 1;
			gDoDebris = 1;
			gDoLensFlare = 1;
			gDoLightFlare = 1;
			gDoRearViewMirror = 1;
			gDoShinyCars = 1;
			gDoMultiColouredCars = 1;
			gDoTexturedTyreTracks = 1;
			gDoTyreTracks = 1;
			gRealTimeLights = 1;
			gUltraLamps = 1;
			gDrawMap = 1;
			gFogAllowed = 1;
			DrawSubmergedObjects = 1;
			mirror_far_clip = 42000.0f;
			far_clip = 45000.0f;

			if (!keepDensity)
			{
				max_pedestrians = 64;
				gTrafficDensity = 64;
			}
			break;
	}
}

// [0x406888] [D]
// Checksum of saved data: the sum of all bytes.
int GetSaveDataHash(void *data, int size)
{
	int i;
	int hash;
	u_char *ptr;

	hash = 0;
	ptr = (u_char *)data;

	for (i = 0; i < size; i++)
		hash += *ptr++;

	return hash;
}

// [0x4068d0] [D]
void InitSavedData(CONFIG_DATA *data)
{
	memset(data, 0, sizeof(CONFIG_DATA));

	data->display_device = wanted_display_device;
	data->display_mode = wanted_display_mode;
	data->display_depth = wanted_display_depth;
	data->InputDevice = InputDevice;

	memcpy(data->UserKeys, &UserKeys, sizeof(data->UserKeys));

	*(CONTROL_JOY_SET *)data->JoySet = JoySet;
	data->JoystickDevice = GetJoystickDevice();
	data->JoyIsAnalogue = JoyIsAnalogue;
	data->JoyDeadZone = JoyDeadZone;

	data->DrawShadows = gDrawShadows;
	data->DoSmoke = gDoSmoke;
	data->DoDebris = gDoDebris;
	data->DoRearViewMirror = gDoRearViewMirror;
	data->DoTexturedTyreTracks = gDoTexturedTyreTracks;
	data->DoTyreTracks = gDoTyreTracks;
	data->DoShinyCars = gDoShinyCars;
	data->DoLensFlare = gDoLensFlare;
	data->DoLightFlare = gDoLightFlare;
	data->DrawSubmergedObjects = DrawSubmergedObjects;
	data->DrawMap = gDrawMap;

	data->AllowSounds = gAllowSounds;
	data->UseCDAudio = gUseCDAudio;
	data->UseDoppler = gUseDoppler;
	data->MasterVolume = gMasterVolume;
	data->MusicVolume = gMusicVolume;

	data->NumCitiesOpen = gNumCitiesOpen;
	data->far_clip = far_clip;
	data->mirror_far_clip = mirror_far_clip;
	data->screen_scale = screen_scale;
	data->max_pedestrians = max_pedestrians;
	data->ForceFeedback = gForceFeedback;
	data->RealTimeLights = gRealTimeLights;
	data->UltraLamps = gUltraLamps;
	data->FogAllowed = gFogAllowed;
	data->DoDetailedShadows = gDoDetailedShadows;
	data->DoMultiColouredCars = gDoMultiColouredCars;
	data->AllowFMV = gAllowFMV;
	data->AutoBrake = gAutoBrake;
	data->Difficulty = gDifficulty;
	data->DoInterpolation = gDoInterpolation;
	data->DoAutoInterpolation = gDoAutoInterpolation;
	data->max_frame_skip = max_frame_skip;
	data->DetailLevel = gDetailLevel;

	data->AudioMixerBits = gAudioMixerBits;
	data->AudioMixerFreq = gAudioMixerFreq;
	data->AudioSpeakerMode = gAudioSpeakerMode;

	memcpy(data->CheatsAllowed, gCheatsAllowed, sizeof(data->CheatsAllowed));

	data->TrafficDensity = gTrafficDensity;
	data->NightBrightness = gNightBrightness;

	data->SoundHardware = gSoundHardware;
	data->SoundCompatibility = gSoundCompatibility;
	data->UseEAX = gUseEAX;

	data->TripleBuffering = gTripleBuffering;
	data->No8BitSupport = gNo8BitSupport;
}

// [0x406bcf] [D]
int FileExists(char *filename)
{
	FILE *f;

	f = fopen(filename, "rb");

	if (f == NULL)
		return 0;

	fclose(f);
	return 1;
}
