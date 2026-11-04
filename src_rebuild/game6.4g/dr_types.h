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

typedef struct PLANE
{
	float a;
	float b;
	float c;
	float d;
} PLANE;

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

// a model vertex while it is drawn: rotated, then projected
typedef struct VERTEX
{
	int computed;		// current_vertex_computed_value when done this frame
	float oow;
	float ooz;
	float vx;
	float vy;
	float vz;
	float tx;			// rotated x / y before the projection
	float ty;
} VERTEX;

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

// COLLISION_PACKET.type
#define COLLISION_BOX			0
#define COLLISION_CYLINDER		1

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

// NOTE: no name in the database: the shine texture u, v of a car point normal (DrawCarModel)
typedef struct struc_11D9060
{
	char field_0;
	char field_1;
} struc_11D9060;

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
// lamps.h

// a lamp lighting the models this frame (AddLamp); position in camera space
typedef struct GAMELAMP
{
	VECTOR position;
	int radius;
	VECTOR color;
	float sqradius;
	float scale;			// 1 / sqradius
} GAMELAMP;

typedef struct PLAMP_RGB
{
	float red;
	float green;
	float blue;
} PLAMP_RGB;

// a lamp to add later in the frame (AddPendingLamp)
typedef struct PENDING_LAMP
{
	VECTOR position;
	int range;
	PLAMP_RGB colour;
} PENDING_LAMP;

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

// per road and lane direction (0 - 3): the number of lanes and the start corner (world units)
typedef struct ROAD_LANE_INFO
{
	char lanes[4];
	XYPAIR bounds[4];
} ROAD_LANE_INFO;

typedef struct ROUTE_DATA
{
	short junction;
	short road;
	short flags;
	short type;
	int height;
	int field_C;
	short objectAngle;
	char field_12;
	int distAhead;
	int distAlongSegment;
} ROUTE_DATA;

// a polygon of a surface model (roads that are not flat), in the model's own space
typedef struct SIPOLY
{
	XYPAIR xz[4];
	PLANE normals;
	short num_vertices;
} SIPOLY;

typedef struct SURFACEINFO
{
	short type;
	short info;
	short heading;
	short numpolys;
	SIPOLY polys[];
} SURFACEINFO;

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
	signed char autoBrake;		// signed (movsx in the binary): -1 while the rear slides into the steering
	signed char analog_steer;	// signed (movsx in the binary): 1 left, -1 right
	char gear;
	char changingGear;
	short revs;
	short revsvol;
} CAR_DATA;

// the player car's hubcaps (InitHubcap, LoseHubcap)
typedef struct HUBCAP
{
	int Present[4];
	VECTOR4 Offset[4];
	MATRIX Orientation;
	MATRIX LocalOrientation;
	VECTOR4 Position;
	VECTOR4 Direction;
	float Height;
	int Duration;
} HUBCAP;

//------------------------------------------------------------------------------------------
// input.h

// the controller bits of a car (controller_bits, ProcessCarPad)
typedef enum ECarPads
{
	CAR_PAD_BRAKE = 0x1,
	CAR_PAD_ACCEL = 0x2,
	CAR_PAD_RIGHT = 0x4,
	CAR_PAD_LEFT = 0x8,
	CAR_PAD_FASTSTEER = 0x10,
	CAR_PAD_HANDBRAKE = 0x20,
	CAR_PAD_WHEELSPIN = 0x40,
	CAR_PAD_CRUISECONTROL = 0x80,
	CAR_PAD_HORN = 0x100,
} ECarPads;

//------------------------------------------------------------------------------------------
// cops.h

typedef struct COP_ROUTE_ENTRY
{
	struct COP_ROUTE_ENTRY *next;
	struct COP_ROUTE_ENTRY *prev;
	int x;
	int z;
	int offset;
	short field_14;
	short field_16;
	short value;
	char copied;
	char field_1B;
	short direction;
	char flags;
	char inserted_node;
} COP_ROUTE_ENTRY;

typedef struct COP
{
	int id;
	int last_carx;
	int last_carz;
	int direction;
	int field_10;
	int current_node;
	int field_18;
	int num_route_entries;
	int desiredSteerAngle;
	int desiredSpeed;
	int speed;
	int recalcTimer;
	int stuckTimer;
	int lastRecoverStrategy;
	int recoveryTimer;
	int speedUpTimer;
	int totalDamage;
	int hiddenTimer;
	int targetPosX;
	int targetPosZ;
	int targetX;
	int targetZ;
	int airbourneTimer;
	int field_5C;
	COP_ROUTE_ENTRY route_entries[96];
	COP_ROUTE_ENTRY *route;
	IVECTOR location;
	IVECTOR targetHistory[4];
	int active;
	int routeInMemory;
	int justPinged;
	int field_CC0;
	int dying;
	int field_CC8;
	int close_pursuit;
	CAR_DATA *pCarData;
	float health;
} COP;

typedef struct COP_DATA
{
	int field_0;
	int field_4;
	int field_8;
	int field_C;
	int field_10;
	int roadblockTrigger;
	int trigger[8];
	int field_38;
} COP_DATA;

// how far and how wide the cops see (felony.c CopSightData)
typedef struct COP_SIGHT_DATA
{
	short surroundViewDistance;
	short frontViewDistance;
	short frontViewAngle;
} COP_SIGHT_DATA;

//------------------------------------------------------------------------------------------
// pathfind.h

// a cell of the A* search (pathfind.c nodes): x, y relative to the target cell
typedef struct COP_ROUTE_NODE
{
	int index;			// cost from the start
	int cost;			// estimate to the target
	int dist;			// index + cost
	int offset;			// the cell it was reached from, -1 at the start
	int x;
	int y;
	int prevCookie;
	int nextCookie;
} COP_ROUTE_NODE;

// a polygon corner (x, z) for the clipping in AddOtherCarsToOccupancyGrid
typedef struct ASTAR_NODE
{
	int x;
	int z;
	struct ASTAR_NODE *next;
} ASTAR_NODE;

typedef ASTAR_NODE ASTAR_POLY[4];

// a corner of a clip rectangle: n along x, t along z
typedef struct iVectNT
{
	int n;
	int t;
} iVectNT;

//------------------------------------------------------------------------------------------
// lamps.h

typedef struct LAMP_RGB
{
	int red;
	int green;
	int blue;
	int pad;
} LAMP_RGB;

typedef struct LAMP_OUTPUT
{
	int type;
	float range;
	VECTOR4 position;
	LAMP_RGB color;
} LAMP_OUTPUT;

//------------------------------------------------------------------------------------------
// felony.h

typedef struct FELONY_DELAY
{
	int current;
	int maximum;
} FELONY_DELAY;

typedef struct FELONY_VALUE
{
	int placid;
	int angry;
} FELONY_VALUE;

typedef struct FELONY_DATA
{
	FELONY_DELAY reoccurrenceDelay[11];
	FELONY_VALUE value[11];
	int pursuitFelonyScale;
} FELONY_DATA;

//------------------------------------------------------------------------------------------
// effects.h

typedef struct RAIN_DROP
{
	LONGVECTOR4 position;
	int field_10;
	int field_14;
	int field_18;
	int field_1C;
	VECTOR4 field_20;
	char field_30;
	unsigned char field_31;
	short index;
} RAIN_DROP;

typedef struct RAIN_LIST
{
	int count;
	int field_04[100];
	RAIN_DROP drops[100];
} RAIN_LIST;

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

// CIV_STATE.ctrlState
typedef enum CIVSTATE
{
	CIVSTATE_EMPTY = 0,
	CIVSTATE_PARKED = 1,
	CIVSTATE_MOVING = 2,
	CIVSTATE_TURNING = 3,
	CIVSTATE_PURSUIT = 4,
	CIVSTATE_SWERVING_LEFT = 5,
	CIVSTATE_SWERVING_RIGHT = 6,
	CIVSTATE_LEADING = 7,
	CIVSTATE_LEADREC = 8,
	CIVSTATE_BLOCKING = 9,
} CIVSTATE;

// CIV_STATE.stateFlags
typedef enum CIVSTATE_FLAGS
{
	CVST_TURNING = 0x1,
	CVST_DRIVING = 0x2,
	CVST_FORWARD = 0x4,
	CVST_PARKING = 0x8,
	CVST_INDICATE_RT = 0x10,
	CVST_INDICATE_LT = 0x20,
} CIVSTATE_FLAGS;

// a road map cell around the player where a car (road_ping_points) or a pedestrian
// (path_ping_points) can be added (GetPingPoints)
typedef struct PING_POINT
{
	short field_00;					// the cell's road, -1 if bit 1 of its info word is set
	char field_02;					// info bits 30 - 31 (the heading)
	char field_03;
	int x;
	int z;
} PING_POINT;

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

// a mission script variable (DmsVars): data points at the game global it sets
typedef struct DMSVariable
{
	char name[32];
	int type;			// DMSVariableType
	void *data;
	char *default_str;
	int default_val;
} DMSVariable;

typedef struct MISSION_TARGET
{
	int radius;
	int target_damage;
	int ping_slot;
	int event;
	int flags;
	IVECTOR position;
	IVECTOR offset;
} MISSION_TARGET;

// a cone of a trail blazer course (pTrailData)
typedef struct TRAIL_DATA
{
	int cone;
	int x;
	int y;
	int z;
	int field_10;
} TRAIL_DATA;

// the player's state saved between missions (MStart, MNext, MLast)
typedef struct MISSION_DATA
{
	VECTOR4 position;
	int field_10;
	int field_14;
	int field_1c;
	int damage[6];
	int totalDamage;
	int felony;
	int direction;
} MISSION_DATA;

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
// gsound.h

// a gear of the engine sound (geard): wheel speeds (/ 2048) to change down when idle, to change
// down, to change up; revs per wheel speed accelerating and idle
typedef struct GEAR_DESC
{
	int lowidl_ws;
	int low_ws;
	int hi_ws;
	int ratio_ac;
	int ratio_id;
} GEAR_DESC;

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

//------------------------------------------------------------------------------------------
// objcoll.h

// a collision box of a model in the world (CheckCollisionWithObject); sizes are half sizes
typedef struct BUILDING_BOX
{
	IVECTOR pos;
	int xsize;
	int zsize;
	int theta;
	int height;
} BUILDING_BOX;

//------------------------------------------------------------------------------------------
// smashable.h

typedef struct SMASHABLE_OBJECT
{
	MODEL *model;
	char *name;
	int sound;
	int volume;
	int pitch;
} SMASHABLE_OBJECT;

// a street lamp the cars hit (damage_lamp); it breaks on the third hit
typedef struct DAMAGE_LAMP
{
	char hits;
	CELL_OBJECT *cop;
} DAMAGE_LAMP;

//------------------------------------------------------------------------------------------
// event.h

typedef struct EVENT_MODEL
{
	VECTOR4 position;
	SVECTOR rotation;
	VECTOR4 direction;
	int field_28;
	int field_2C;
	int field_30;
	int field_34;
	int field_38;
	short field_3C;
	short field_3e;
	short palette;
	short field_42;
	float velocity;
	short flags;
	short model_id;
	int commands_size;
	short *commands_begin;
	short *commands_end;
	struct EVENT_MODEL *next;
	float field_5C;
} EVENT_MODEL;

typedef struct _EVENT
{
	short flags;
	short radius;
	short rotation;
	short id;
	VECTOR4 position;
	int *node;
	EVENT_MODEL *model;
	struct _EVENT *next;
} EVENT;

//------------------------------------------------------------------------------------------
// job_fx.h

typedef struct Helicopter
{
	VECTOR4 position;
	VECTOR4 field_10;
	MATRIX draw_mat;
	SVECTOR angle;
	float field_68;
	int rotorrot;
	int frame;
	int start_ypos;
	int start_yang;
	int start;
} Helicopter;

//------------------------------------------------------------------------------------------
// overlay.h

// a bar of the in game overlay (damage, felony, opponent); flags 1 fills from the bottom, 2 is
// right aligned at xpos and fills from the right
typedef struct PERCENTAGE_BAR
{
	float xpos;
	float ypos;
	float width;
	float height;
	float position;
	float max;
	float red;
	float green;
	float blue;
	char *text;
	int flags;
	int active;
} PERCENTAGE_BAR;

//------------------------------------------------------------------------------------------
// pedestrian.h

// NOTE: only position is used so far (DrawAllPedShadows); the other field names are IDA's
typedef struct PEDESTRIAN
{
	short type;
	short index;
	short motion_type_1;
	short field_06;
	int pallet;
	int flags_1;
	int flags_2;
	int field_14;
	int field_18;
	int motion_type_2;
	int field_20;
	int field_24;
	int field_28;
	int num_frames;
	int speed;
	int frame;
	int last_dir;
	int field_3c;
	int interp_frame;
	int model_base;
	int field_48;
	int interest;
	int head_rotation;
	int field_54;
	int field_58;
	SVECTOR direction;
	VECTOR4 velocity;
	VECTOR4 field_74;
	VECTOR4 position;
	VECTOR4 target;
	VECTOR4 field_a4;
} PEDESTRIAN;

//------------------------------------------------------------------------------------------
// motion_c.h

// a bone of the motion capture skeleton: its object and up to 6 children; num_children 255 is
// a model (unsigned: movzx in the binary)
typedef struct HIERARCHY_NODE
{
	unsigned char object_num;
	unsigned char num_children;
	unsigned char children[6];
} HIERARCHY_NODE;

// a frame of motion capture data: the root position, then num_objects rotations
typedef struct MOTION_PACKET
{
	VECTOR4 root;
	QUATERNION objects[1];
} MOTION_PACKET;

//------------------------------------------------------------------------------------------
// debris.h

// the colour and flare size of a kind of light (light_colour); flags 1: brighter facing the
// camera, 2: blue by field_14 too, 4: fades out at the side, 8: fades out nearer (15000)
typedef struct LIGHT_INFO
{
	int red;
	int green;
	int blue;
	float field_0c;		// the size away from the camera
	float field_10;		// the size facing it
	float field_14;		// the brightness away from it
	int flags;
} LIGHT_INFO;

// a triangle (the debris, litter and leaf shapes)
typedef struct TRI_POINT
{
	VECTOR4 v0;
	VECTOR4 v1;
	VECTOR4 v2;
} TRI_POINT;

#endif // DR_TYPES_H
