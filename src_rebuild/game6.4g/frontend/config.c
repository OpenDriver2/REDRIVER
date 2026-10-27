/*
 * frontend/config.c - config.dat options, saved games and replays
 *
 * Rebuilt from Driver_sym2_94.i64 (IDA folder "frontend\config.c"); partially translated.
 */

#include <stdio.h>
#include <string.h>

#include "dr_types.h"

typedef struct SAVEDATA_HEADER
{
	int ident;
	int checksum;
} SAVEDATA_HEADER;

typedef struct DISPLAY_MODE
{
	int width;
	int height;
	char name[16];
	int field_18;
} DISPLAY_MODE;

// NOTE: name guessed; value from the binary
#define CONFIG_IDENT	0x93587031

/* input.c */
extern int SetJoystickDevice(int device);
extern int SetJoystickDeadZone(void);

/* sound.c, eax/cdaudio.c */
extern void SetEffectsVolume(float volume);
extern void SetCDVolume(int volume);
extern void SetSoundSystemFormat(int bits, int freq);
extern void SetSoundSystemMode(int mode);

// NOTE: in the binary this is an inline `fld; fistp INT_FLT; mov eax, INT_FLT` sequence,
// the original macro/inline is unknown
extern int float_to_int(float value);

// NOTE: IDA mark @frontend\gvars.c;extern
extern DISPLAY_MODE display_modes[];
// NOTE: IDA mark @frontend\gvars.c;extern
extern CONTROL_KEY_SET UserKeys;
// NOTE: IDA mark @frontend\gvars.c;extern
extern CONTROL_KEY_SET DefaultKeySet;
// NOTE: IDA mark @frontend\gvars.c;extern
extern CONTROL_JOY_SET DefaultJoySet;

extern CONTROL_JOY_SET JoySet;
extern CONTROL_KEY_SET KeySet;
extern char InputDevice;
extern int JoyIsAnalogue;
extern int JoyDeadZone;
extern int JoyIsFF;
extern int gForceFeedback;

extern int gDrawShadows;
extern int gDoSmoke;
extern int gDoDebris;
extern int gDoRearViewMirror;
extern int gDoTexturedTyreTracks;
extern int gDoTyreTracks;
extern int gDoShinyCars;
extern int gDoLensFlare;
extern int gDoLightFlare;
extern int DrawSubmergedObjects;
extern int gDrawMap;
extern int gAllowSounds;
extern int gUseCDAudio;
extern int gUseDoppler;
extern float gMasterVolume;
extern float gMusicVolume;
extern int gNumCitiesOpen;
extern float far_clip;
extern float mirror_far_clip;
extern float screen_scale;
extern int max_pedestrians;
extern int gRealTimeLights;
extern int gUltraLamps;
extern int gFogAllowed;
extern int gDoDetailedShadows;
extern int gDoMultiColouredCars;
extern int gAllowFMV;
extern int gAutoBrake;
extern int gDifficulty;
extern int gDoInterpolation;
extern int gDoAutoInterpolation;
extern int max_frame_skip;
extern int gDetailLevel;
extern int gAudioMixerBits;
extern int gAudioMixerFreq;
extern int gAudioSpeakerMode;
extern int gCheatsAllowed[11];
extern int gTrafficDensity;
extern int gNightBrightness;
extern int gSoundHardware;
extern int gSoundCompatibility;
extern int gUseEAX;
extern int num_soundcards;
extern int gDoFrontend;

void SetDefaultOptions(void);
void SetGraphicsLevel(int level, int keepDensity);
int GetSaveDataHash(void *data, int size);

//------------------------------------------------------------------------------------------
// initialised data (0x54bcc0, start of the file's block per IDA mark)

int wanted_display_width = 640;
int wanted_display_height = 480;
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
