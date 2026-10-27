/*
 * dr_types.h - [A] types shared by several source files
 *
 * Not an original file: the original headers are not known beyond the IDA local types folders
 * (math.h, frontend\frontend.h, D3DGlide\types.h, ...), which group the types below. A type only
 * one file needs is defined in that file; where a type belongs: `python tools/progress.py --types`.
 * Layouts are the IDA local types.
 */

#ifndef DR_TYPES_H
#define DR_TYPES_H

#include <windows.h>		// SCREEN, DSOUND_CARD_INFO
#include <stdint.h>

#define FX_GLIDE_NO_FUNC_PROTO
#include <glide.h>			// TextureSet

typedef unsigned char u_char;
typedef unsigned char byte;

//------------------------------------------------------------------------------------------
// math.h

typedef struct SVECTOR
{
	short vx;
	short vy;
	short vz;
	short pad;
} SVECTOR;

typedef struct VECTOR
{
	float vx;
	float vy;
	float vz;
} VECTOR;

typedef struct IVECTOR
{
	int vx;
	int vy;
	int vz;
	int vw;
} IVECTOR;

typedef struct VECTOR4
{
	float vx;
	float vy;
	float vz;
	float vw;
} VECTOR4;

typedef struct MATRIX
{
	float m[4][3];
	float t[4];
} MATRIX;

typedef struct RGB
{
	byte r;
	byte g;
	byte b;
	byte pad;
} RGB;

typedef struct RGBA
{
	char r;
	char g;
	char b;
	char a;
} RGBA;

//------------------------------------------------------------------------------------------
// tsd.h

typedef struct TEX_INFO
{
	char name[8];
	uint8_t tset;
	uint8_t u;
	uint8_t v;
	uint8_t w;
	uint8_t h;
	uint8_t pad1;
	uint8_t pad2;
	uint8_t pad3;
} TEX_INFO;

//------------------------------------------------------------------------------------------
// D3DGlide\types.h

typedef struct SCREEN
{
	HWND handle;
	HINSTANCE module;
	int fullscreen;
	int active;
} SCREEN;

// a 256x256 texture set (texture page) of the game
typedef struct TextureSet
{
	char flags;
	unsigned char texture_slot;		// 255: not in texture memory
	char unk_3;
	char unk_4;
	void *data;
	int texture_ram_address;
	GrTexInfo *TInfo;
} TextureSet;

typedef struct HARDWARE_SETTINGS
{
	int rbits;
	int rshift;
	int gbits;
	int gshift;
	int bbits;
	int bshift;
	int abits;
	int ashift;
	int emulate_8bit;
	int device_flags;
	unsigned short *texlookup16;
} HARDWARE_SETTINGS;

// NOTE: defined in D3DGlide/glide2x.c (needs the DirectDraw/Direct3D headers); elsewhere only by pointer
typedef struct DEVICE_3D_INFO DEVICE_3D_INFO;

typedef struct DEVICE_3D
{
	int type;
	char name[64];
	int hardware_8bit;
	DEVICE_3D_INFO *field_48;
} DEVICE_3D;

//------------------------------------------------------------------------------------------
// frontend\frontend.h

typedef struct FE_TEXT
{
	short x;
	short y;
	u_char font;
	u_char justify;
	char field_6;
	char field_7;
	char *text;
	RGB color;
} FE_TEXT;

typedef struct FE_POLY
{
	short x;
	short y;
	char *text;
	short flags;
	short field_0A;		// width (untextured)
	short field_0C;		// height (untextured)
	RGBA color;
	short field_12;
} FE_POLY;

typedef struct FE_BUTTON
{
	void (*selfunc)(int);
	void (*unselfunc)(int);
	int (*choosefunc)(void);
	byte l;
	byte r;
	byte u;
	byte d;
} FE_BUTTON;

//------------------------------------------------------------------------------------------
// cheats.h

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

//------------------------------------------------------------------------------------------
// gameapi.h

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

//------------------------------------------------------------------------------------------
// input.h

typedef struct CONTROL_KEY_SET
{
	uint8_t LessFarClip;		// DIK_*
	uint8_t MoreFarClip;
	uint8_t LessMirrorFarClip;
	uint8_t MoreMirrorFarClip;
	uint8_t SteerLeft;
	uint8_t SteerRight;
	uint8_t Accelerate;
	uint8_t Brake;
	uint8_t FastSteer;
	uint8_t Handbrake;
	uint8_t Burnout;
	uint8_t LookLeft;
	uint8_t LookRight;
	uint8_t LookBehind;
	uint8_t CruiseControl;
	uint8_t Horn;
	uint8_t ViewToggle;
	uint8_t DropCamera;
	uint8_t Unused1;
	uint8_t Unused2;
} CONTROL_KEY_SET;

typedef struct CONTROL_JOY_SET
{
	uint8_t Accelerate;
	uint8_t Brake;
	uint8_t FastSteer;
	uint8_t Handbrake;
	uint8_t Burnout;
	uint8_t LookLeft;
	uint8_t LookRight;
	uint8_t LookBehind;
	uint8_t CruiseControl;
	uint8_t Horn;
	uint8_t ViewToggle;
	uint8_t DropCamera;
} CONTROL_JOY_SET;

//------------------------------------------------------------------------------------------
// scores.h

typedef enum GAMETYPE
{
	GAME_DEFAULT = 0,
	GAME_TRAILBLAZER = 1,
	GAME_CHECKPOINT = 2,
	GAME_SURVIVAL = 3,
	GAME_PURSUIT = 4,
	GAME_GETAWAY = 5,
	GAME_DIRTTRACK = 6,
	GAME_CARNAGE = 7,
	GAME_TAKEADRIVE = 8,
	GAME_MISSION = 9,
	GAME_CARPARK = 10,
	GAME_TIMETRIAL = 11,
	GAME_BESTLAP = 12,
} GAMETYPE;

typedef struct TIMER
{
	int count;
	int flags;
	int ticks;
	int frac;
	int sec;
	int min;
} TIMER;

typedef struct HIGHSCORE_DATA
{
	int field_00;
	char name[12];
	int items;
	TIMER lap_info;
} HIGHSCORE_DATA;

typedef struct SCORE_DATA
{
	GAMETYPE type;
	int index;
	int items;
	HIGHSCORE_DATA *table;
	TIMER lap_info;
	TIMER best_lap;
	int complete;
} SCORE_DATA;

//------------------------------------------------------------------------------------------
// chairs.h

typedef struct CHAIR
{
	int x;
	int z;
	int flags;
	SVECTOR ang;
} CHAIR;

//------------------------------------------------------------------------------------------
// sound.h

typedef struct SOUND_CALLBACKS
{
	void (*CloseAudio)(void);
	int (*EnableDoppler)(int);
	void (*StartFrame)(void);
	void (*EndFrame)(void);
	void (*SetMasterVolume)(int);
	void (*SetSoundFormat)(int, int);
	void (*SetSpeakerMode)(int);
	void (*SetWorldStats)(float, float, float, float, float);
	void (*SetEnvironment)(int);
	int (*LoadSoundBuffer)(int, char *);
	int (*DuplicateSample)(int, int);
	void (*ReleaseSample)(int);
	void (*SetListenerPosition)(float, float, float);
	void (*SetListenerOrientation)(float, float, float, float, float, float);
	void (*SetListenerVelocity)(float, float, float);
	int (*PlayChannel)(int, int);
	void (*StopChannel)(int);
	void (*RestartChannel)(int);
	void (*SetChannelVolume)(int, int);
	void (*SetChannelFrequency)(int, int);
	void (*SetChannelPosition)(int, float, float, float);
	void (*SetChannelVelocity)(int, float, float, float);
	void (*SetChannelMode)(int, int);
	void (*GetChannelStatus)(int, void *);
} SOUND_CALLBACKS;

// NOTE: not in an IDA types folder; used by sound.c, eax/directsound.c, frontend/soundoptions.c
typedef struct DSOUND_CARD_INFO
{
	int field_00;
	GUID *field_04;
	GUID field_08;
	char name[32];
} DSOUND_CARD_INFO;

//------------------------------------------------------------------------------------------
// types not translated yet (used by pointer)

typedef struct DIRECTOR DIRECTOR;			// Director.h
typedef struct MLADDER_ROOM MLADDER_ROOM;	// mladder\mladder.h

#endif // DR_TYPES_H
