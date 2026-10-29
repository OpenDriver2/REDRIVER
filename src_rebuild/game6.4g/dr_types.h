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
#include <stdio.h>			// FILE in prototypes

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

typedef LONG LONGVECTOR3[3];
typedef LONG LONGVECTOR4[4];

typedef struct QUATERNION
{
	VECTOR4 v;
	float s;
} QUATERNION;

typedef LONG LONGQUATERNION[4];

typedef struct IMATRIX
{
	short m[3][3];
	int t[3];
} IMATRIX;

typedef struct BOX
{
	float xmin;
	float ymin;
	float zmin;
	float xmax;
	float ymax;
	float zmax;
	float theta;
	int iTheta;
} BOX;

typedef struct XYPAIR
{
	int x;
	int y;
} XYPAIR;

//------------------------------------------------------------------------------------------
// camera.h

typedef struct CAMERA_POINT
{
	VECTOR4 position;
	QUATERNION rotation;
	float zoom;
	int flags;
	int frame;
	int filed_30;
} CAMERA_POINT;

// the map position of the camera or the player: cell, region (regions_across x regions_down) and
// the slot of the region in the 2 x 2 "barrel" of unpacked regions
typedef struct CAMERA
{
	int current_cell_x;
	int current_cell_z;
	int current_cell;
	int current_region;
	int current_region_x;
	int current_region_z;
	int current_region_cell_x;
	int current_region_cell_z;
	int previous_region;
	char barrel_region_x;
	char barrel_region_z;
	char current_barrel_region;
	char pad;
} CAMERA;

//------------------------------------------------------------------------------------------
// cell.h

typedef struct CELL_DATA
{
	int num;		// index in cell_objects
	int next;		// next CELL_DATA of the cell, -1: last
} CELL_DATA;

typedef struct CELL_OBJECT
{
	int computed;
	int type;		// model
	VECTOR4 pos;
	int yang;
	int ghost;
} CELL_OBJECT;

//------------------------------------------------------------------------------------------
// map.h

typedef struct MOVEABLE_OBJECT
{
	VECTOR4 pos;
	SVECTOR ang;
	VECTOR4 linear_velocity;
	SVECTOR angular_velocity;
	int type;
	int flags;
	int unk_38;
	int unk_3c;
} MOVEABLE_OBJECT;

//------------------------------------------------------------------------------------------
// Engine\types.h

typedef enum POLY_TYPES
{
	FLAT3 = 0x0,
	FLAT4 = 0x1,
	GOUR3 = 0x2,
	GOUR4 = 0x3,
	TEXT3 = 0x4,
	TEXT4 = 0x5,
	GOURTEXT3 = 0x6,
	GOURTEXT4 = 0x7,
	FLAT3LIT = 0x10,
	FLAT4LIT = 0x11,
	GOUR3LIT = 0x12,
	GOUR4LIT = 0x13,
	TEXT3LIT = 0x14,
	TEXT4LIT = 0x15,
	GOURTEXT3LIT = 0x16,
	GOURTEXT4LIT = 0x17,
	FLAT3_T = 0x20,
	FLAT4_T = 0x21,
	GOUR3_T = 0x22,
	GOUR4_T = 0x23,
	TEXT3_T = 0x24,
	TEXT4_T = 0x25,
	GOURTEXT3_T = 0x26,
	GOURTEXT4_T = 0x27,
	FLAT3LIT_T = 0x30,
	FLAT4LIT_T = 0x31,
	GOUR3LIT_T = 0x32,
	GOUR4LIT_T = 0x33,
	TEXT3LIT_T = 0x34,
	TEXT4LIT_T = 0x35,
	GOURTEXT3LIT_T = 0x36,
	GOURTEXT4LIT_T = 0x37,
} POLY_TYPES;

// MODEL.shape_flags
typedef enum SHAPE_FLAGS
{
	SHAPE_FLAG_LIT = 0x1,
	SHAPE_FLAGS_ANIMOBJ = 0x4,
	SHAPE_FLAG_TRANS = 0x8,
	SHAPE_FLAG_AXIS2 = 0x10,
	SHAPE_FLAG_AXIS1 = 0x20,
	SHAPE_FLAG_SUBMERGED = 0x40,
	SHAPE_FLAG_WATER = 0x80,
	SHAPE_FLAG_AMBIENT2 = 0x100,
	SHAPE_FLAG_AMBIENT1 = 0x200,
	SHAPE_FLAG_TILE = 0x400,
	SHAPE_FLAG_SHADOW = 0x800,
	SHAPE_FLAG_ALPHA = 0x1000,
	SHAPE_FLAG_ROAD = 0x2000,
	SHAPE_FLAG_SPRITE = 0x4000,
	SHAPE_FLAG_RELOCATED = 0x8000,
} SHAPE_FLAGS;

// MODEL.flags2
typedef enum MODEL_FLAGS
{
	MODEL_FLAG_NEON = 0x2,
	MODEL_FLAG_JUNCTION = 0x40,
	MODEL_FLAG_ALLEY = 0x80,
	MODEL_FLAG_INDOORS = 0x100,
	MODEL_FLAG_CHAIR = 0x200,
	MODEL_FLAG_SMASHABLE = 0x400,
	MODEL_FLAG_NIGHT_ONLY = 0x800,
	MODEL_FLAG_LAMP = 0x1000,
	MODEL_FLAG_TREE = 0x2000,
	MODEL_FLAG_GRASS = 0x4000,
	MODEL_FLAG_PATH = 0x8000,
} MODEL_FLAGS;

// a model as stored in the level file; the int members are offsets until RelocateModel
typedef struct MODEL
{
	short version_number;
	unsigned short shape_flags;
	int instance_number;		// -1: not an instance of another model
	int flags2;
	float bounding_sphere;
	float bounding_sphere2d;
	short num_vertices;
	short num_polys;
	short num_point_normals;
	short num_texture_sets;
	int texture_set_info;
	int vertices;
	int normals;
	int point_normals;
	int poly_block;
	int BSP_data;
	int collision_block;
} MODEL;

// polygons of a converted model (ConvertModelToInternalFormat). All start with the type byte
// (POLY_TYPES), a flags byte (axis alignment, 8: first of its normal) and, for textured ones,
// the texture set; the int at +4 is the normal index

typedef struct POLYF3
{
	int id;
	int pad;
	int v0;
	int v1;
	int v2;
	float r;
	float g;
	float b;
} POLYF3;

typedef struct POLYF3LIT
{
	char id;
	char pad;
	int normal;
	int v0;
	int v1;
	int v2;
	float r;
	float g;
	float b;
} POLYF3LIT;

typedef struct POLYF4
{
	int id;
	int pad;
	int v0;
	int v1;
	int v2;
	int v3;
	float r;
	float g;
	float b;
} POLYF4;

typedef struct POLYF4LIT
{
	char id;
	char pad;
	int normal;
	int v0;
	int v1;
	int v2;
	int v3;
	float r;
	float g;
	float b;
} POLYF4LIT;

typedef struct POLYG3
{
	int id;
	int pad;
	int v0;
	int v1;
	int v2;
	float r0;
	float g0;
	float b0;
	float r1;
	float g1;
	float b1;
	float r2;
	float g2;
	float b2;
} POLYG3;

typedef struct POLYG3LIT
{
	char id;
	char pad;
	int normal;
	int v0;
	int v1;
	int v2;
	int pn0;
	int pn1;
	int pn2;
	float r;
	float g;
	float b;
} POLYG3LIT;

typedef struct POLYG4
{
	int id;
	int pad;
	int v0;
	int v1;
	int v2;
	int v3;
	float r0;
	float g0;
	float b0;
	float r1;
	float g1;
	float b1;
	float r2;
	float g2;
	float b2;
	float r3;
	float g3;
	float b3;
} POLYG4;

typedef struct POLYG4LIT
{
	char id;
	char pad;
	int normal;
	int v0;
	int v1;
	int v2;
	int v3;
	int pn0;
	int pn1;
	int pn2;
	int pn3;
	float r;
	float g;
	float b;
} POLYG4LIT;

typedef struct POLYFT3
{
	char id;
	char pad;
	char texture_set;
	int unknown;
	int v0;
	int v1;
	int v2;
	float s0;
	float t0;
	float s1;
	float t1;
	float s2;
	float t2;
	float r;
	float g;
	float b;
} POLYFT3;

typedef struct POLYFT3LIT
{
	char type;
	char pad;
	char texture_set;
	int normal;
	int v0;
	int v1;
	int v2;
	float s0;
	float t0;
	float s1;
	float t1;
	float s2;
	float t2;
	float r;
	float g;
	float b;
} POLYFT3LIT;

typedef struct POLYFT4
{
	char id;
	char pad;
	char texture_set;
	int unknown;
	int v0;
	int v1;
	int v2;
	int v3;
	float s0;
	float t0;
	float s1;
	float t1;
	float s2;
	float t2;
	float s3;
	float t3;
	float r;
	float g;
	float b;
} POLYFT4;

typedef struct POLYFT4LIT
{
	char id;
	char pad;
	char texture_set;
	int normal;
	int v0;
	int v1;
	int v2;
	int v3;
	float s0;
	float t0;
	float s1;
	float t1;
	float s2;
	float t2;
	float s3;
	float t3;
	float r;
	float g;
	float b;
} POLYFT4LIT;

typedef struct POLYGT3
{
	char id;
	char pad;
	char texture_set;
	int unknown;
	int v0;
	int v1;
	int v2;
	float s0;
	float t0;
	float s1;
	float t1;
	float s2;
	float t2;
	float r0;
	float g0;
	float b0;
	float r1;
	float g1;
	float b1;
	float r2;
	float g2;
	float b2;
} POLYGT3;

typedef struct POLYGT3LIT
{
	char type;
	char pad;
	char texture_set;
	int normal;
	int v0;
	int v1;
	int v2;
	int pn0;
	int pn1;
	int pn2;
	float s0;
	float t0;
	float s1;
	float t1;
	float s2;
	float t2;
	float r;
	float g;
	float b;
} POLYGT3LIT;

typedef struct POLYGT4
{
	char id;
	char pad;
	char texture_set;
	int unknown;
	int v0;
	int v1;
	int v2;
	int v3;
	float s0;
	float t0;
	float s1;
	float t1;
	float s2;
	float t2;
	float s3;
	float t3;
	float r0;
	float g0;
	float b0;
	float r1;
	float g1;
	float b1;
	float r2;
	float g2;
	float b2;
	float r3;
	float g3;
	float b3;
} POLYGT4;

typedef struct POLYGT4LIT
{
	char type;
	char pad;
	char texture_set;
	int normal;
	int v0;
	int v1;
	int v2;
	int v3;
	int pn0;
	int pn1;
	int pn2;
	int pn3;
	float s0;
	float t0;
	float s1;
	float t1;
	float s2;
	float t2;
	float s3;
	float t3;
	float r;
	float g;
	float b;
} POLYGT4LIT;

typedef struct COLLISION_PACKET
{
	short type;
	short xpos;
	short ypos;
	short zpos;
	short flags;
	short yang;
	short empty;
	short xsize;
	short ysize;
	short zsize;
} COLLISION_PACKET;

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

typedef struct DISPLAY_MODE
{
	int width;
	int height;
	char name[16];
	int field_18;
} DISPLAY_MODE;

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
// roadbits.h

typedef struct ROAD
{
	short ID;
	short JunctionA;
	short JunctionB;
	char NumLanes;
	char LaneDirs;
	short Length;
	char Name[50];
	char Flags;
	char IsOneWay;
	short x;
	short z;
} ROAD;

typedef struct ROADBOUNDS
{
	int x1;
	int y1;
	int x2;
	int y2;
	char dir;
	char pad[3];
} ROADBOUNDS;

typedef struct JENTRY
{
	short FromJunction;
	short ViaRoad;
	short JunctionFlags;
	short ExtraInfo;
} JENTRY;

typedef struct JUNCTION
{
	short ID;
	char Type;
	char JWH;
	short LightTimer;
	char LightsNS;
	char LightsWE;
	JENTRY JunctionEntry[4];
	short x;
	short z;
} JUNCTION;

//------------------------------------------------------------------------------------------
// cars.h

typedef struct WHEEL
{
	char susCompression;
	char locked;
	char onGrass;
	unsigned char surface;
} WHEEL;

typedef union RigidBodyState
{
	int v[13];
	struct
	{
		LONGVECTOR3 fposition;
		LONGQUATERNION orientation;
		LONGVECTOR3 linearVelocity;
		LONGVECTOR3 angularVelocity;
	} n;
} RigidBodyState;

typedef struct CAR_COSMETICS
{
	VECTOR4 headLight[2];
	VECTOR4 frontInd[2];
	VECTOR4 backInd[2];
	VECTOR4 brakeLight[2];
	VECTOR4 revLight;
	VECTOR4 policeLight[2];
	VECTOR4 exhaust;
	VECTOR4 smoke;
	VECTOR4 fire;
	SVECTOR wheelDisp[4];
	short powerRatio;
	short extraInfo;
	SVECTOR cPoints[12];
	SVECTOR colBox;
} CAR_COSMETICS;

typedef struct _CAR_DATA
{
	char id;
	char field_1;
	char model;
	char phys;
	IMATRIX where;
	RigidBodyState st;
	int acc[4];
	int aacc[4];
	WHEEL wheel[4];
	IVECTOR shadowPoints[4];
	IVECTOR location;
	IVECTOR velocity;
	SVECTOR rotation;
	int wheelsOnGround;
	int inWater;
	int thrust;
	int wheel_angle;
	int direction;
	int handbrake;
	int wheelspin;
	int wheel_speed;
	int speed;
	int front_vel;
	int rear_vel;
	int damage[6];
	int savedDamage;
	int totalDamage;
	short mayBeColliding;
	char fourWheelDrive;
	char needsDenting;
	char autoBrake;
	char analog_steer;
	char gear;
	char changingGear;
	short revs;
	short revsvol;
} CAR_DATA;

//------------------------------------------------------------------------------------------
// ai.h

typedef struct CIV_STATE
{
	unsigned char id;
	unsigned char ctrlState;
	char wrecked;
	char InJunction;
	char trafficLightPhaseId;
	char IsCopCar;
	char ping_slot;
	char parked;
	char driving;
	char laneOffset;
	char pingOutTimer;
	char parkedTimer;
	char model;
	char palette;
	short pathFlags;
	char field_10;
	uint8_t speedDamperTimer;
	char blocked;
	char stateFlags;
	char pinged;
	char isGangCar;
	short coplife;
	int distAhead;
	int distAlongSegment;
	int currentJunction;
	int currentLane;
	int currentRoad;
	int targetRoad;
	int targetLane;
	int turnDir;
	int speed;
	int maxSpeed;
	int targetX;
	int targetZ;
	int currentX;
	int currentZ;
	int hornTimer;
} CIV_STATE;

//------------------------------------------------------------------------------------------
// checkpoint.h

typedef struct CHECKPOINT_DATA
{
	int count;
	int index;
	int flags;
	int step;
	int display_flags;
	int type;
	int model;
	int radius;
	IVECTOR position;
	TIMER timer;
} CHECKPOINT_DATA;

//------------------------------------------------------------------------------------------
// MScript.h

typedef struct MISSION
{
	int id;
	int objective;
	int field_8;
	int field_C;
	int inform_count;
	int wait_count;
	int flags;
	int window;
	int window_radius;
	int field_24;
	int failed_reason;
	int outer_radius;
	int event_timer;
	int event_id;
	int field_38;
	int player_rotation;
	int pause_timer;
	int max_speed;
	int iTrailCount;
	int iTrailIndex;
	int target_damage;
	int trail_model;
	int field_58;
	int field_5C;
	int payback_radius;
	int field_64;
	int gangCarId;
	int gangCarMaxDamage;
	int ghost_car_type;
	int targetCarRange;
	int targetCarId;
	int user_var_1;
	int user_var_2;
	int user_var_3;
	int user_var_4;
	int user_var_5;
	int field_90;
	int field_94;
	int field_98;
	int field_9C;
	int field_A0;
	char *map_name;
	char text[256];
	int best_lap;
	int payback;
	int loaded;
	int maxSpeedEnable;
	int useLastPlayerPosition;
	int debugInfo;
	int canBeFailed;
	char gap1C4[52];
	CHECKPOINT_DATA *checkpoint;
	TIMER *timer;
	LONGVECTOR4 player_position;
} MISSION;

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
// DirectorStorage.h

typedef struct WAYPOINT
{
	int x;
	int z;
	int frame;
} WAYPOINT;

//------------------------------------------------------------------------------------------
// Director.h

typedef struct DIRECTOR_CAMERA
{
	int type;
	int index;
	int distance;
	int max_distance;
	int target;
	int frame_index;
	int turnAngle;
	int yang;
	int xang;
	int field_24[8];
	int field_44;
	int perspective;
	LONGVECTOR4 position;
	LONGVECTOR4 holding_position;
	LONGVECTOR4 camerapos;
	SVECTOR angle;
	int field_84;
	int field_88;
} DIRECTOR_CAMERA;

typedef struct DIRECTOR_TEXT
{
	int text;
	char *icon;
	int field_08;
	int offsetX;
	int offsetY;
	int flags;
	int prev;
	int next;
	int extra;
	TEX_INFO *texture;
	int index;
} DIRECTOR_TEXT;

typedef struct DIRECTOR
{
	int NumCameras;
	int CameraIndex;
	int NumFrames;
	int FrameIndex;
	int NumWaypoints;
	int FlashPhase;
	int FlashTimer;
	int Selection;
	int DemoTimer;
	int DemoType;
	int field_28;
	int IconColor;
	int DoingReplay;
	int Moving;
	int EditCamera;
	int field_3C;
	int Debounce;
	int Selected;
	int field_48;
	int Attract;
	int Preview;
	int Editing;
	int field_58;
	int FollowCar;
	int Mode;
	int Debug;
	DIRECTOR_CAMERA WorkCamera;
	DIRECTOR_CAMERA *Camera;
	DIRECTOR_CAMERA **CameraList;
	DIRECTOR_TEXT *Menu;
	WAYPOINT *Waypoints;
	int LastSelection;
	DIRECTOR_CAMERA SaveCamera;
} DIRECTOR;

//------------------------------------------------------------------------------------------
// mladder\mladder.h

typedef struct MLADDER_COMMAND
{
	int type;
	int data;
	struct MLADDER_COMMAND *next_cmd;
	struct MLADDER_COMMAND *else_cmd;
} MLADDER_COMMAND;

typedef struct MLADDER_MESSAGE
{
	short type;
	short data;
	MLADDER_COMMAND *command;
} MLADDER_MESSAGE;

typedef struct MLADDER_ROOM
{
	MLADDER_MESSAGE messages[4];
} MLADDER_ROOM;

typedef struct MISSION_LADDER
{
	MLADDER_COMMAND *command_list;
	MLADDER_ROOM *room_list;
	MLADDER_COMMAND *command;
	int flags;
	int level;
} MISSION_LADDER;

//------------------------------------------------------------------------------------------
// saved game (NOTE: not in an IDA types folder; used by frontend/config.c, frontend/frontend.c,
// mladder/mladder.c)

typedef struct GAME_DATA
{
	char name[16];
	int gNumCitiesOpen;
	int mladder_index;		// mladder.command - mladder.command_list
	int mladder_level;
	int mladder_flags;
	int gStoredPlayerCarType;
	int field_24;
	int field_28;
	int field_2c;
	int field_30;
	int field_34;
} GAME_DATA;

#endif // DR_TYPES_H
