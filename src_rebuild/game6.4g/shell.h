/*
 * shell.h - globals and functions of shell.c used by other files
 */

#ifndef SHELL_H
#define SHELL_H

#include "dr_types.h"

// globals
extern int gCopCarWipedOutDamage;
extern int gTrafficDensity;
extern int DrawSubmergedObjects;
extern int max_frame_skip;
extern int gNumCitiesOpen;
extern int gRealTimeLights;
extern int gDetailLevel;
extern int gTrainingMode;
extern int gAudioMixerBits;
extern int gAudioMixerFreq;
extern int gAllowCopSpeech;
extern int gRefreshRate;
extern float gMasterVolume;
extern float gMusicVolume;
extern int do_plug;
extern int gDoDetailedShadows;
extern int gAutoBrake;
extern int gShowMouse;
extern int gDoLensFlare;
extern int gDoInterpolation;
extern int gDoAutoInterpolation;
extern int gFogAllowed;
extern int gAllowSounds;
extern int gDoMultiColouredCars;
extern int gAllowFMV;
extern int gAllowCheatKeys;
extern int gUseEAX;
extern int gUseCopModel;
extern SCREEN gScreen;
extern HANDLE gProcess;
extern HARDWARE_SETTINGS hw_settings;
extern char ghost_mode;
extern int gDodgyBrakeFactor;
extern int gDodgySpeedFactor;
extern int quit;
extern int exit_code;
extern int gDifficulty;
extern int GameLevel;
extern int MissionNumber;
extern int GameMode;
extern int gCopRespawnTime;
extern int TrailSpacing;
extern int gBitchingMode;
extern int gNightBrightness;
extern int gAudioSpeakerMode;
extern int gStoredPlayerCarType;
extern int gSoundHardware;
extern int gSoundCompatibility;
extern int gEventTargetRange[5];
extern int gUltraLamps;
extern int FrameCount;
extern int gDisableDebug;
extern int gNight;
extern int gDarkNight;
extern int gUseCDAudio;
extern int gUseDoppler;
extern int gDoInterview;
extern int gSkipInterview;
extern int gDoTraining;
extern int gDoRave;
extern int gRecordTraining;
extern int gMissionRecording;
extern int gMissionReplaying;
extern int gMissionSetting;
extern int gDoFrontend;
extern int gNoPedestrians;
extern int gBuildKeyFrames;
extern int gNoCops;
extern int gSaveScreenShots;
extern int gDisableText;
extern int gDisableDenting;
extern int gDoAussie;
extern int TrailStarted;
extern int TestTrail;
extern int SaveTrail;
extern int FlyBy;
extern int EndofFlyBy;
extern int gDoMotelRoom;
extern int gForceFeedback;
extern int gNoFMV;
extern int gDoCredits;
extern int gInPresentation;
extern int SettingJoyKey;
extern int gUseBadGuyModel;
extern int gChaseHQMode;
extern int gBotchRoads;
extern int display_depth;

// functions
extern void DisplayErrorAndExit(const char *text);
extern void HandleWinMessages(void);
extern void SetGameWindowSize(HWND hWnd, int width, int height);

// [A] /WINDOW: framed window instead of full screen
extern int gWindowed;

// [A] /GLIDE: use glide2x.dll instead of the configured device
extern int gUseGlide;

#endif // SHELL_H
