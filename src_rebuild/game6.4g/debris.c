/*
 * debris.c - particle effects and lights: smoke, flames, sparks, debris, leaves, birds, car and
 * street lights, the lens flare, searchlights, car pools and pedestrian shadows
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "debris.c", 0x4ac520 - 0x4bc94f).
 *
 * The particles live in pools (smoke, debris, leaf) with free lists (smoke_alloc ...): the
 * Setup_* and Add* functions take one, HandleEffects moves them every other frame.
 * DisplayEffects draws the leaves, birds and debris, then sorts the smoke (DrawSmokeEffects)
 * and the lights added this frame (brake_lights, light_flares: see StartOfEffectsFrame) by
 * depth into smoke_table and draws them additively (DisplaySortedEffectsList). Moving lights
 * at night leave streaks (carblur_info for the cars, Known_Lamps for the traffic lights).
 * Data (0x555900 - 0x555da0): the debris colours, the lens flares, the light colours, the
 * texture names.
 */

#include "dr_types.h"
#include "debris.h"
#include "dr_globals.h"
#include "D3DGlide/glide_ptrs.h"
#include <math.h>
#include "ai.h"
#include "asmmath.h"
#include "camera.h"
#include "denting.h"
#include "effects.h"
#include "handling.h"
#include "map.h"
#include "pause.h"
#include "pedest.h"
#include "roadbits.h"
#include "shell.h"
#include "smashable.h"
#include "testmath.h"
#include "Engine/3dfx/gfxinit.h"
#include "Engine/3dfx/object.h"
#include "Engine/3dfx/polydraw.h"
#include "Engine/3dfx/tset.h"

#define MAX_SMOKE		200
#define MAX_DEBRIS		200
#define MAX_LEAVES		100

// SMOKE (and SMOKE_LIGHT) flags
#define SMOKE_ACTIVE			0x2
#define SMOKE_SMOKE				0x4
#define SMOKE_SPARK				0x8
#define SMOKE_BRAKE_LIGHT		0x20
#define SMOKE_FRONT_INDICATOR	0x40
#define SMOKE_BACK_INDICATOR	0x80
#define SMOKE_REVERSE_LIGHT		0x100
#define SMOKE_FLAME				0x1000
#define SMOKE_BLACK				0x2000		// black smoke (tyres, engine)
#define SMOKE_EXHAUST			0x4000
#define SMOKE_WATER				0x8000
#define SMOKE_DUST				0x10000		// tyres on grass
#define SMOKE_LIGHTFLARE		0x20000
#define SMOKE_HAZE				0x40000
#define SMOKE_SLOW_BLACK		0x80000		// smoking objects (Setup_SlowSmoke)
#define SMOKE_SLOW				0x100000
#define SMOKE_IN_FOG			0x1000000	// kept in the fog (DisplaySmoke)

#define DEBRIS_ACTIVE			0x2
#define LEAF_ACTIVE				0x2

// a smoke puff, flame, spark or water splash
typedef struct SMOKE
{
	VECTOR4 position;
	VECTOR4 position2;			// in camera space (DrawSmokeEffects)
	VECTOR4 drift;				// a spark: its last position
	VECTOR4 drift_change;		// a spark: its speed
	VECTOR4 final_tail_pos;		// a spark: its position before last (the tail)
	int start_w;				// its width; a spark: its type
	int final_w;
	int transparency;			// its brightness
	int t_step;
	int step;
	short life;
	short halflife;
	int flags;
	short num;
	short pos;
	struct SMOKE *prev;
	struct SMOKE *next;
} SMOKE;

// a light drawn among the smoke (smoke_table): flare, haze, car light
typedef struct SMOKE_LIGHT
{
	VECTOR4 position;			// in camera space
	VECTOR4 position2;			// in the world
	VECTOR4 drift;
	VECTOR4 drift_change;
	VECTOR4 final_tail_pos;
	int num;					// the car
	int cycle;					// its streak (carblur_info, Known_Lamps)
	int pad1;
	int pad2;
	int pad3;
	short type;
	short transparency;
	int flags;
	short dir;
	short yang;
	struct SMOKE_LIGHT *prev;
	struct SMOKE_LIGHT *next;
} SMOKE_LIGHT;

typedef struct DEBRIS
{
	IVECTOR position;
	IVECTOR direction;
	u_short life;
	u_short flags;
	u_short num;
	u_short pos;				// its rotation (debris_rot1, debris_rot2)
	int step;
	int type;
	int frame;					// the bounces
	RGB rgb;
	u_char field_38;
	u_char field_39;
	u_char field_3A;
} DEBRIS;

// a leaf or bit of litter
typedef struct LEAF
{
	VECTOR4 position;
	VECTOR4 direction;
	u_short life;
	u_short flags;
	u_short num;
	u_short pos;				// its rotation (leaf_rot, litter_rot)
	int step;
	RGB rgb;
	u_char unsigned30;
	u_char unsigned31;
	u_char unsigned32;
	u_char unsigned33;
	u_short sin_index1;
	u_short sin_index2;
	u_char sin_addition1;
	u_char sin_addition2;
	u_char type;				// 1 a leaf, 2 .. 5 litter
} LEAF;

typedef struct BIRD
{
	VECTOR4 position;
	VECTOR4 direction;
	short life;
	short step;					// its wing beat
} BIRD;

// a lens flare: its screen position in width, height
typedef struct FLARE
{
	char *name;
	TEX_INFO *texture;
	RGB color;
	float alpha;
	float width;
	float height;
	float size;
} FLARE;

// the screen positions of a moving light over the last 4 frames
typedef struct LAMP_STREAK
{
	XYPAIR light_trails[4];
	short frame;
	short set;
	int clock;					// the lamp (find_lamp_streak)
} LAMP_STREAK;

typedef struct EFFECTS_INFO
{
	int DoSmoke;
	int DoCopCarLights;
	int DoBackIndicatorLights;
	int DoTrafficLights;
} EFFECTS_INFO;

// a searchlight of level 2 (see InitialiseSpotLights, DrawSpotLight)
typedef struct SPOT_LIGHT
{
	int field_0;
	int field_4;
	int field_8;
	int field_C;
	int field_10;
	int field_14;
	float field_18;
	float field_1C;
	int field_20;
	float field_24;
	float field_28;
	int field_2C;
} SPOT_LIGHT;

//------------------------------------------------------------------------------------------
// initialised data

RGB gPlayerCarDebrisColour[18] =
{
	{ 111, 115, 145, 0 },
	{ 184, 207, 218, 0 },
	{ 226, 130, 121, 0 },
	{ 60, 50, 44, 0 },
	{ 118, 154, 197, 0 },
	{ 186, 158, 144, 0 },
	{ 97, 109, 128, 0 },
	{ 208, 148, 117, 0 },
	{ 84, 98, 136, 0 },
	{ 142, 164, 132, 0 },
	{ 109, 97, 110, 0 },
	{ 194, 208, 217, 0 },
	{ 115, 82, 58, 0 },
	{ 120, 100, 5, 0 },
	{ 208, 148, 117, 0 },
	{ 208, 148, 117, 0 },
	{ 208, 40, 10, 0 },
	{ 108, 108, 108, 0 },
};

char flare_sizes[8] = { 32, 96, 96, 32, 128, 64, 64, 96 };

u_char flame_colours[11][3] =
{
	{ 7, 7, 7 },
	{ 25, 2, 2 },
	{ 35, 0, 0 },
	{ 60, 0, 0 },
	{ 60, 25, 0 },
	{ 60, 35, 0 },
	{ 60, 55, 0 },
	{ 60, 55, 25 },
	{ 60, 55, 35 },
	{ 60, 55, 55 },
	{ 70, 70, 70 },
};

FLARE sun = { "SUN2", NULL, { 255, 240, 192, 0 } };
FLARE sunflare = { "BLANK", NULL, { 255, 240, 192, 0 } };

FLARE flares[8] =
{
	{ "CARLIGHT", NULL, { 0, 0, 207, 0 } },
	{ "CARLIGHT", NULL, { 179, 74, 8, 0 } },
	{ "CARLIGHT", NULL, { 207, 0, 0, 0 } },
	{ "CARLIGHT", NULL, { 0, 207, 0, 0 } },
	{ "CARLIGHT", NULL, { 0, 0, 207, 0 } },
	{ "CARLIGHT", NULL, { 207, 207, 0, 0 } },
	{ "CARLIGHT", NULL, { 207, 0, 0, 0 } },
	{ "CARLIGHT", NULL, { 0, 0, 207, 0 } },
};

// the brightness of an indicator over its blink
char gIndicatorLookup[16] = { 0, 63, 127, 191, 255, 191, 127, 63, 0, 63, 127, 191, 255, 191, 127, 63 };

float gFlareScale = 1.0f;
int gDoSmoke = 1;
int gDoDebris = 1;

// NOTE: no name in the database (dword_555B24); guessed: DrawLensFlare tests the sun with a
// quad (1) or by writing the frame buffer (0)
static int sun_test_by_quad = 1;

LIGHT_INFO light_colour[14] =
{
	{ 192, 192, 192, 0.0f, 0.0f, 0.0f, 0 },
	{ 255, 0, 0, 100.0f, 150.0f, 0.9f, 5 },
	{ 0, 255, 0, 100.0f, 150.0f, 0.9f, 5 },
	{ 230, 180, 10, 100.0f, 150.0f, 0.9f, 5 },
	{ 255, 255, 255, 60.0f, 110.0f, 0.5f, 15 },
	{ 200, 0, 0, 40.0f, 90.0f, 0.5f, 15 },
	{ 200, 0, 0, 0.0f, 0.0f, 0.0f, 0 },
	{ 128, 128, 80, 0.0f, 0.0f, 0.0f, 0 },
	{ 0, 128, 0, 0.0f, 0.0f, 0.0f, 0 },
	{ 255, 0, 0, 80.0f, 80.0f, 0.0f, 0 },
	{ 0, 0, 255, 80.0f, 80.0f, 0.0f, 0 },
	{ 255, 0, 0, 128.0f, 128.0f, 0.0f, 0 },
	{ 0, 0, 255, 128.0f, 128.0f, 0.0f, 0 },
	{ 255, 0, 0, 40.0f, 90.0f, 0.5f, 15 },
};

// the cop car roof lights across
float light_xpos[4] = { 48.0f, 68.0f, 90.0f, 112.0f };

//------------------------------------------------------------------------------------------
// uninitialised data

GrVertex flare_vB;
VECTOR4 flare_position;
GrVertex flare_vC;

// NOTE: no names in the database (stru_B9F318, stru_B9F398, stru_B9F418, stru_B9F3D8): the
// corners of DisplayFlareScaled
static GrVertex flare_scaled_vA;
GrVertex flare_vD;
static GrVertex flare_scaled_vB;
static GrVertex flare_scaled_vD;
static GrVertex flare_scaled_vC;
GrVertex flare_vA;
int gDoLightFlare;
static float wind_magnitude;

short smoke_alloc[MAX_SMOKE];
SMOKE smoke[MAX_SMOKE];

TEX_INFO *pedshadow_texture;
TEX_INFO *bird1_texture;
short debris_alloc[MAX_DEBRIS];

// NOTE: no name in the database (dword_117A3B0): the pixels of the sun seen last frame
static int sun_pixels;

short leaf_alloc[MAX_LEAVES];
char gLightTableNo[66];			// BUGGED: Never gets reset!
TEX_INFO *lightflare_texture;
LEAF leaf[MAX_LEAVES];
POLYFT4 gPedPoly;
SMOKE_LIGHT tlights_array[100];
int frame_count;
SPOT_LIGHT spotl[6];
int num_brake_lights;
int next_leaf;
EFFECTS_INFO EffectsInfo;
int gPedNumToShadow[64];
SMOKE *smoke_table;
int NewLamp[31];
TEX_INFO *spark_texture;
int gCarPools[65];
int num_light_flares;
SMOKE_LIGHT *lights;
TEX_INFO *sun_texture;
TRI_POINT debris_rot2[256];
TRI_POINT debris_rot1[256];
MATRIX debris_mat;
SMOKE_LIGHT *light_flares;
TEX_INFO *litter_texture[4];
SMOKE_LIGHT *tlights;
TEX_INFO *pool_texture;
TEX_INFO *smoke_texture;
LAMP_STREAK carblur_info[66][8];
TEX_INFO *sunflare_texture;
int StreakCount1;
int FlameCount[65][2];
SMOKE_LIGHT *brake_lights;
DEBRIS debris[MAX_DEBRIS];
TEX_INFO *leaf_texture;
int next_smoke;
BIRD birds[10];
int FlameOffset[65][2];
int gPedShadowsToDraw;
SMOKE_LIGHT light_flares_array[300];
int gNumBirds;
TEX_INFO *flare_texture;
int FlameLife[65][2];
int next_debris;
TRI_POINT leaf_rot[256];
TEX_INFO *haze_texture;
TEX_INFO *copflare_texture;
TRI_POINT litter_rot[256];
TEX_INFO *shadow_texture;
int LightIndex;
TEX_INFO *bird2_texture;

// NOTE: no name in the database (unk_119C700): the board's pixel counts (DrawLensFlare)
static GrSstPerfStats_t sun_perf_stats;

SMOKE_LIGHT lights_array[200];
SVECTOR debris_rotvec;
TEX_INFO *cop_texture;
int gCarPoolsToDraw;
TEX_INFO *debris_texture;
TEX_INFO *spotlight_texture;
POLYFT4 gPoolPoly;
LAMP_STREAK Known_Lamps[31];
int main_cop_light_pos;
SMOKE_LIGHT brake_lights_array[200];
int num_traffic_lights;

void sub_4AC520(void);
void InitialiseDebris(void);
void StartOfEffectsFrame(void);
void HandleEffects(void);
void DisplayEffects(void);
void DisplaySortedEffectsList(void);
void InitialiseFlares(void);
void InitialiseLitter(void);
void InitialiseLeaves(void);
void InitialiseSmoke(void);
void InitialiseFlames(void);
void InitialiseBirds(void);
void InitialiseCarPools(void);
void InitialisePedShadows(void);
void InitialiseSpotLights(void);
void GetEffectTextures(void);
int AllocateDebris(void);
void ReleaseDebris(short num);
int AllocateSmoke(void);
void ReleaseSmoke(short num);
int AllocateLeaf(void);
void ReleaseLeaf(short num);
void DrawCarPool(int id);
void DrawPedShadow(float *pos, int size);
void SwirlLeaves(IVECTOR *pos);
void SplashWater(CAR_DATA *cp);
void AddGroundDebris(void);
void AddLightFlare(VECTOR4 *position1, VECTOR4 *position2, short dir, short yang, int type, int glow, int num, int cycle);
void AddWater(int id, int height);
void AddSideLights(int id);
void AddBrakeLight(int id);
void AddHeadLights(int id);
void AddReverseLight(int id);
void AddIndicatorLight(int id, int type);
void AddSingleIndicator(int id, VECTOR4 *pos, int flags);
void sub_4B0AB8(void);
void AddTrafficLight(CELL_OBJECT *cop, float x, float y, float z, short flags);
void AddCopCarLight(int id, short *coplife, float health);
void AddDamageStreetLight(CELL_OBJECT *cop, float x, float y, float z, short type, short dir);
void AddLeaf(VECTOR4 *Position, int num_leaves, int Type);
void AddDebris(VECTOR4 *Position, VECTOR4 *Direction, RGB *colour, u_short life, int step);
void AddExhaustSmoke(int id, int WheelSpeed);
void AddSmokingEngine(int id, int BlackSmoke, int WheelSpeed);
void AddFlamingEngine(int id);
void AddHazeLight(CELL_OBJECT *cop, float x, float y, float z, short type, short yang, short transparency);
void CreateBirds(VECTOR4 *pos, int num);
void DrawLeaf(LEAF *lf, int shadow);
void DisplaySmoke(SMOKE *sm);
void DisplayWater(SMOKE *sm);
void DisplayFlame(SMOKE *sm);
void DisplaySpark(SMOKE *spark);
void DisplayBirds(void);
void DrawDebris(DEBRIS *db);
void DisplayDebris(void);
void DisplayLeaves(void);
void DrawCarPools(void);
void DrawAllPedShadows(void);
void DrawLensFlare(void);
void DisplayFlareScaled(FLARE *flare, float oow, int angle);
void DisplayFlare(FLARE *flare, float oow);
void DrawReverseLight(SMOKE_LIGHT *lt);
void DrawLightFlare(SMOKE_LIGHT *sm);
void DrawLightHaze(SMOKE_LIGHT *sm);
void DrawBrakeLight(SMOKE_LIGHT *lt);
void DrawFrontIndicatorLight(SMOKE_LIGHT *lt);
void DrawBackIndicatorLight(SMOKE_LIGHT *lt);
void Setup_Debris(IVECTOR *ipos, IVECTOR *ispeed, int num_debris, int type, int model);
void Setup_Smoke(VECTOR4 *pos, int start_w, int end_w, int SmokeType, int WheelSpeed, VECTOR4 *Drift, int Exhaust);
void Setup_SlowSmoke(VECTOR4 *pos, VECTOR4 *drift, int start_w, int end_w, int transparency, int step, int type, float drift_change, int exhaust);
void Setup_Flame(VECTOR4 *pos, int start_w, int end_w, short life);
void Setup_Sparks(IVECTOR *ipos, IVECTOR *ispeed, int num_sparks, int Type);
void GetSmokeDrift(VECTOR4 *wind, int step);
void HandleDebris(void);
void StepDebris(DEBRIS *db);
void HandleLeaves(void);
void HandleBirds(void);
void HandleWater(SMOKE *sm);
void HandleFlame(SMOKE *sm);
void HandleSpark(SMOKE *sm);
void HandleSmoke(SMOKE *sm);
void HandleSmokeEffects(void);
void DisplaySmokeEffects(void);
void DrawSmokeEffects(void);
void DrawLightHazes(void);
void DrawLightFlares(void);
void DrawSpotLights(void);
void DrawSpotLight(SPOT_LIGHT *light);
void PreLampStreak(void);
int find_lamp_streak(int clock);

// [0x4ac520] [D]
// Empty in the original.
void sub_4AC520(void)
{
}

// [0x4ac525] [D]
void InitialiseDebris(void)
{
	*(int *)CamerasChanged = 0;		// [redundant] - handled in InitCameraGlobals
	lastcameraview = 0;				// [redundant] - handled in InitCameraGlobals
	main_cop_light_pos = 0;

	GetEffectTextures();

	InitialiseFlares();
	InitialiseLitter();
	InitialiseLeaves();
	InitialiseSmoke();
	InitialiseBirds();
	InitialiseCarPools();
	InitialisePedShadows();
	InitialiseSmashableObjects();
	InitialiseSpotLights();

	EffectsInfo.DoSmoke = 1;
	EffectsInfo.DoCopCarLights = 1;
	EffectsInfo.DoBackIndicatorLights = 1;
	EffectsInfo.DoTrafficLights = 1;
}

// [0x4ac59f] [D]
// Empties the lists of lights added each frame.
void StartOfEffectsFrame(void)
{
	num_light_flares = 0;
	num_traffic_lights = 0;
	num_brake_lights = 0;
	smoke_table = NULL;
	lights = lights_array;
	tlights = tlights_array;
	brake_lights = brake_lights_array;
	light_flares = light_flares_array;
}

// [0x4ac5f4] [D]
// Moves the effects (every other frame).
void HandleEffects(void)
{
	if (GamePaused)
		return;

	if (gDoDebris)
		MoveHubcap(0);

	if ((game_frame & 1) == 0)
	{
		CurrentRotationMatrix = &inv_camera_matrix;

		HandleSmokeEffects();
		HandleLeaves();
		HandleBirds();
		HandleDebris();

		main_cop_light_pos++;
		main_cop_light_pos &= 15;

		HandleCopSirens();
	}
}

// [0x4ac666] [D]
void DisplayEffects(void)
{
	SwapPalette(0);

	if (last_state != TEXT3)
	{
		grColorCombine(GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_LOCAL, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_TEXTURE, FXFALSE);
		last_state = TEXT3;
	}

	if (gDoDebris)
		MoveHubcap(1);

	CurrentRotationMatrix = &inv_camera_matrix;

	DisplayLeaves();
	DisplayBirds();
	DisplayDebris();
	DisplaySmokeEffects();

	if (do_fogging)
		grFogMode(GR_FOG_DISABLE);

	DisplaySortedEffectsList();
	DrawSpotLights();

	if (do_fogging)
		grFogMode(GR_FOG_WITH_TABLE);
}

// [0x4ac6fa] [D]
// Draws the depth sorted smoke and lights (smoke_table, see DrawSmokeEffects), additive.
void DisplaySortedEffectsList(void)
{
	SMOKE *sm;
	int depth_mask;
	int flags;

	if (last_state != TEXT3)
	{
		grColorCombine(GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_LOCAL, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_TEXTURE, FXFALSE);
		last_state = TEXT3;
	}

	if (last_trans_on != 3)
	{
		grAlphaBlendFunction(GR_BLEND_ONE, GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_ZERO);
		last_trans_on = 3;
	}

	depth_mask = EnableDepthMask(0);

	for (sm = smoke_table; sm; sm = sm->next)
	{
		flags = sm->flags;

		if (flags & SMOKE_SMOKE)
			DisplaySmoke(sm);
		else if (flags & SMOKE_LIGHTFLARE)
			DrawLightFlare((SMOKE_LIGHT *)sm);
		else if (flags & SMOKE_HAZE)
			DrawLightHaze((SMOKE_LIGHT *)sm);
		else if (flags & SMOKE_BACK_INDICATOR)
			DrawBackIndicatorLight((SMOKE_LIGHT *)sm);
		else if (flags & SMOKE_REVERSE_LIGHT)
			DrawReverseLight((SMOKE_LIGHT *)sm);
		else if (flags & SMOKE_BRAKE_LIGHT)
			DrawBrakeLight((SMOKE_LIGHT *)sm);
		else if (flags & SMOKE_FRONT_INDICATOR)
			DrawFrontIndicatorLight((SMOKE_LIGHT *)sm);
		else if (flags & SMOKE_SPARK)
			DisplaySpark(sm);
		else if (flags & SMOKE_WATER)
			DisplayWater(sm);
		else if (flags & SMOKE_FLAME)
			DisplayFlame(sm);
	}

	EnableDepthMask(depth_mask);

	lastcameraview = cameraview;
}

// [0x4ac89f] [D]
void InitialiseFlares(void)
{
	int i;

	for (i = 0; i < 8; i++)
		flares[i].texture = flare_texture;

	sun.texture = sun_texture;
	sun_pixels = 64;
}

// [0x4ac8e7] [D]
// Clears the debris and makes the 256 rotations of the debris and litter triangles.
void InitialiseLitter(void)
{
	int i;

	ClearMem(debris, sizeof(debris));
	ClearMem(debris_alloc, sizeof(debris_alloc));

	for (i = 0; i < 16; i++)
		ground_debris[i] = NULL;

	for (i = 0; i < MAX_DEBRIS; i++)
	{
		debris[i].pos = 0;
		debris[i].num = i;
		debris_alloc[i] = i;
	}

	debris_rotvec.vx = 0;
	debris_rotvec.vy = 0;
	debris_rotvec.vz = 0;

	for (i = 0; i < 256; i++)
	{
		CalcMatrixXYZ(&debris_mat, &debris_rotvec);
		CurrentRotationMatrix = &debris_mat;

		debris_rot1[i].v0.vx = -10.0f;
		debris_rot1[i].v0.vy = 10.0f;
		debris_rot1[i].v0.vz = 0.0f;
		debris_rot1[i].v1.vx = 0.0f;
		debris_rot1[i].v1.vy = -10.0f;
		debris_rot1[i].v1.vz = 0.0f;
		debris_rot1[i].v2.vx = 10.0f;
		debris_rot1[i].v2.vy = 10.0f;
		debris_rot1[i].v2.vz = 0.0f;

		MatrixRotate(&debris_rot1[i].v0);
		MatrixRotate(&debris_rot1[i].v1);
		MatrixRotate(&debris_rot1[i].v2);

		debris_rot2[i].v0.vx = -7.0f;
		debris_rot2[i].v0.vy = 7.0f;
		debris_rot2[i].v0.vz = 0.0f;
		debris_rot2[i].v1.vx = 0.0f;
		debris_rot2[i].v1.vy = -7.0f;
		debris_rot2[i].v1.vz = 0.0f;
		debris_rot2[i].v2.vx = 7.0f;
		debris_rot2[i].v2.vy = 7.0f;
		debris_rot2[i].v2.vz = 0.0f;

		MatrixRotate(&debris_rot2[i].v0);
		MatrixRotate(&debris_rot2[i].v1);
		MatrixRotate(&debris_rot2[i].v2);

		litter_rot[i].v0.vx = -35.0f;
		litter_rot[i].v0.vy = 0.0f;
		litter_rot[i].v0.vz = 40.0f;
		litter_rot[i].v1.vx = 40.0f;
		litter_rot[i].v1.vy = 0.0f;
		litter_rot[i].v1.vz = 35.0f;
		litter_rot[i].v2.vx = -30.0f;
		litter_rot[i].v2.vy = 0.0f;
		litter_rot[i].v2.vz = -35.0f;

		MatrixRotate(&litter_rot[i].v0);
		MatrixRotate(&litter_rot[i].v1);
		MatrixRotate(&litter_rot[i].v2);

		// NOTE: vy is stepped twice and vz never, as in the original
		debris_rotvec.vx += 16;
		debris_rotvec.vy += 32;
		debris_rotvec.vy += 64;
		debris_rotvec.vx &= 0xFFF;
		debris_rotvec.vy &= 0xFFF;
		debris_rotvec.vz &= 0xFFF;
	}

	next_debris = 0;
}

// [0x4accc0] [D]
// Clears the leaves and makes the 256 rotations of the leaf triangle.
void InitialiseLeaves(void)
{
	int i;

	ClearMem(leaf, sizeof(leaf));
	ClearMem(leaf_alloc, sizeof(leaf_alloc));

	debris_rotvec.vx = 0;
	debris_rotvec.vy = 0;
	debris_rotvec.vz = 0;

	for (i = 0; i < 256; i++)
	{
		CalcMatrixXYZ(&debris_mat, &debris_rotvec);
		CurrentRotationMatrix = &debris_mat;

		leaf_rot[i].v0.vx = -30.0f;
		leaf_rot[i].v0.vy = 7.0f;
		leaf_rot[i].v0.vz = 0.0f;
		leaf_rot[i].v1.vx = 0.0f;
		leaf_rot[i].v1.vy = -7.0f;
		leaf_rot[i].v1.vz = 0.0f;
		leaf_rot[i].v2.vx = 30.0f;
		leaf_rot[i].v2.vy = 7.0f;
		leaf_rot[i].v2.vz = 0.0f;

		MatrixRotate(&leaf_rot[i].v0);
		MatrixRotate(&leaf_rot[i].v1);
		MatrixRotate(&leaf_rot[i].v2);

		debris_rotvec.vx += 16;
		debris_rotvec.vy += 32;
		debris_rotvec.vy += 64;
		debris_rotvec.vx &= 0xFFF;
		debris_rotvec.vy &= 0xFFF;
		debris_rotvec.vz &= 0xFFF;
	}

	for (i = 0; i < MAX_LEAVES; i++)
	{
		leaf[i].pos = 0;
		leaf[i].num = i;
		leaf_alloc[i] = i;
	}

	next_leaf = 0;
}

// [0x4aced2] [D]
void InitialiseSmoke(void)
{
	int i;

	for (i = 0; i < MAX_SMOKE; i++)
	{
		smoke[i].pos = 0;
		smoke[i].num = i;
		smoke[i].flags = 0;
		smoke_alloc[i] = i;
	}

	next_smoke = 0;
}

// [0x4acf40] [D]
// NOTE: no callers
void InitialiseFlames(void)
{
	int i;
	int j;

	for (i = 0; i < 65; i++)
	{
		for (j = 0; j < 2; j++)
		{
			FlameCount[i][j] = 0;
			FlameOffset[i][j] = 0;
			FlameLife[i][j] = 0;
		}
	}
}

// [0x4acfba] [D]
void InitialiseBirds(void)
{
	birds[0].life = 0;
}

// [0x4acfc8] [D]
void InitialiseCarPools(void)
{
	pool_texture = GetTextureDetails("CARPOOL6");

	if (pool_texture)
	{
		gPoolPoly.v0 = 0;
		gPoolPoly.v1 = 1;
		gPoolPoly.v2 = 2;
		gPoolPoly.v3 = 3;
		gPoolPoly.texture_set = pool_texture->tset;
		gPoolPoly.s0 = (float)(pool_texture->u + 1);
		gPoolPoly.t0 = (float)(pool_texture->h + pool_texture->v);
		gPoolPoly.s1 = (float)(pool_texture->w + pool_texture->u);
		gPoolPoly.t1 = (float)(pool_texture->h + pool_texture->v);
		gPoolPoly.s2 = (float)(pool_texture->w + pool_texture->u);
		gPoolPoly.t2 = (float)(pool_texture->v + 1);
		gPoolPoly.s3 = (float)(pool_texture->u + 1);
		gPoolPoly.t3 = (float)(pool_texture->v + 1);
	}
}

// [0x4ad11c] [D]
// NOTE: the texture's w and h are used as its right and bottom: GetEffectTextures made them so
void InitialisePedShadows(void)
{
	gPedPoly.v0 = 0;
	gPedPoly.v1 = 1;
	gPedPoly.v2 = 2;
	gPedPoly.v3 = 3;
	gPedPoly.texture_set = pedshadow_texture->tset;
	gPedPoly.s0 = (float)(pedshadow_texture->u + 1);
	gPedPoly.t0 = (float)pedshadow_texture->h;
	gPedPoly.s1 = (float)pedshadow_texture->w;
	gPedPoly.t1 = (float)pedshadow_texture->h;
	gPedPoly.s2 = (float)pedshadow_texture->w;
	gPedPoly.t2 = (float)(pedshadow_texture->v + 1);
	gPedPoly.s3 = (float)(pedshadow_texture->u + 1);
	gPedPoly.t3 = (float)(pedshadow_texture->v + 1);
	gPedPoly.r = 255.0f;
	gPedPoly.g = 255.0f;
	gPedPoly.b = 255.0f;
}

// [0x4ad23c] [D]
// The searchlights of level 2 (Los Angeles).
void InitialiseSpotLights(void)
{
	if (GameLevel != 2)
		return;

	frame_count = 0;

	spotl[0].field_0 = 300;
	spotl[0].field_4 = 10;
	spotl[0].field_8 = 350;
	spotl[0].field_C = 800;
	spotl[0].field_10 = 547;
	spotl[0].field_14 = 600;
	spotl[0].field_18 = 15.0f;
	spotl[0].field_1C = 25.0f;
	spotl[0].field_20 = 1600;
	spotl[0].field_24 = 15.0f;
	spotl[0].field_28 = 0.45f;
	spotl[0].field_2C = 108;

	spotl[1].field_0 = 100;
	spotl[1].field_4 = 15;
	spotl[1].field_8 = 300;
	spotl[1].field_C = 650;
	spotl[1].field_10 = 1312;
	spotl[1].field_14 = 400;
	spotl[1].field_18 = 10.0f;
	spotl[1].field_1C = 12.0f;
	spotl[1].field_20 = 0;
	spotl[1].field_24 = 1.0f;
	spotl[1].field_28 = 0.45f;
	spotl[1].field_2C = 90;

	spotl[2].field_0 = 4000;
	spotl[2].field_4 = 12;
	spotl[2].field_8 = 100;
	spotl[2].field_C = 700;
	spotl[2].field_10 = 2647;
	spotl[2].field_14 = 400;
	spotl[2].field_18 = 7.0f;
	spotl[2].field_1C = 8.0f;
	spotl[2].field_20 = 300;
	spotl[2].field_24 = 0.0f;
	spotl[2].field_28 = 0.5f;
	spotl[2].field_2C = 120;

	spotl[3].field_0 = 3946;
	spotl[3].field_4 = 20;
	spotl[3].field_8 = 3800;
	spotl[3].field_C = 900;
	spotl[3].field_10 = 0;
	spotl[3].field_14 = 450;
	spotl[3].field_18 = 13.0f;
	spotl[3].field_1C = 40.0f;
	spotl[3].field_20 = 1700;
	spotl[3].field_24 = 30.0f;
	spotl[3].field_28 = 0.3f;
	spotl[3].field_2C = 90;

	spotl[4].field_0 = 3920;
	spotl[4].field_4 = 5;
	spotl[4].field_8 = 3350;
	spotl[4].field_C = 600;
	spotl[4].field_10 = 2567;
	spotl[4].field_14 = 250;
	spotl[4].field_18 = 13.0f;
	spotl[4].field_1C = 15.0f;
	spotl[4].field_20 = 1024;
	spotl[4].field_24 = 4.0f;
	spotl[4].field_28 = 0.55f;
	spotl[4].field_2C = 90;

	spotl[5].field_0 = 3800;
	spotl[5].field_4 = 5;
	spotl[5].field_8 = 3600;
	spotl[5].field_C = 600;
	spotl[5].field_10 = 1024;
	spotl[5].field_14 = 300;
	spotl[5].field_18 = 10.0f;
	spotl[5].field_1C = 20.0f;
	spotl[5].field_20 = 2000;
	spotl[5].field_24 = 12.0f;
	spotl[5].field_28 = 0.45f;
	spotl[5].field_2C = 105;
}

// [A]
// An effect texture: u, v moved in by a texel; w, h made the right and bottom (not the size).
// NOTE: not a function in the original: GetEffectTextures repeats it for each texture
static TEX_INFO *GetEffectTexture(char *name)
{
	TEX_INFO *tex;

	tex = GetTextureDetails(name);

	if (tex)
	{
		tex->u++;
		tex->v++;
		tex->w += tex->u - 1;
		tex->h += tex->v - 1;
	}

	return tex;
}

// [0x4ad529] [D]
void GetEffectTextures(void)
{
	debris_texture = GetEffectTexture("DEBRIS");
	spark_texture = GetEffectTexture("SPARK");
	smoke_texture = GetEffectTexture("SMOKE");
	bird1_texture = GetEffectTexture("SWBIRD1");
	bird2_texture = GetEffectTexture("SWBIRD2");
	litter_texture[0] = GetEffectTexture("JLITTER1");
	litter_texture[1] = GetEffectTexture("JLITTER2");
	litter_texture[2] = GetEffectTexture("JLITTER3");
	litter_texture[3] = GetEffectTexture("JLITTER4");
	cop_texture = GetEffectTexture("LIGHT");
	lightflare_texture = GetEffectTexture("CARLYT");
	haze_texture = GetEffectTexture("SMALLFLR");
	spotlight_texture = GetEffectTexture("SPOTL");
	leaf_texture = GetEffectTexture("YLEAF");
	flare_texture = GetEffectTexture("CARLIGHT");
	sun_texture = GetEffectTexture("SUN2");

	sunflare_texture = lightflare_texture;
	shadow_texture = cop_texture;
	haze_texture = flare_texture;
	pedshadow_texture = flare_texture;

	copflare_texture = GetEffectTexture("COPFLARE");
}

// [0x4ade31] [D]
int AllocateDebris(void)
{
	if (!gDoDebris || next_debris >= MAX_DEBRIS)
		return -1;

	return debris_alloc[next_debris++];
}

// [0x4ade6c] [D]
void ReleaseDebris(short num)
{
	debris_alloc[--next_debris] = num;
}

// [0x4ade90] [D]
int AllocateSmoke(void)
{
	if (next_smoke >= MAX_SMOKE)
		return -1;

	return smoke_alloc[next_smoke++];
}

// [0x4adec2] [D]
void ReleaseSmoke(short num)
{
	smoke_alloc[--next_smoke] = num;
}

// [0x4adee6] [D]
int AllocateLeaf(void)
{
	if (next_leaf >= MAX_LEAVES)
		return -1;

	return leaf_alloc[next_leaf++];
}

// [0x4adf15] [D]
void ReleaseLeaf(short num)
{
	leaf_alloc[--next_leaf] = num;
}

// [A]
// Rotates the ground quad in conv_points 0..3 (relative to the camera) to the camera and draws
// it with poly (DrawCarPool, DrawPedShadow).
// NOTE: not a function in the original: both functions have this block
static void DrawGroundQuad(POLYFT4 *poly, int trans, int fade)
{
	MATRIX mat;
	float scale;
	int i;

	if (FlipX)
	{
		mat = inv_camera_matrix;

		mat.m[0][0] = -inv_camera_matrix.m[0][0];
		mat.m[1][0] = -inv_camera_matrix.m[1][0];
		mat.m[2][0] = -inv_camera_matrix.m[2][0];

		CurrentRotationMatrix = &mat;
	}
	else
	{
		CurrentRotationMatrix = &inv_camera_matrix;
	}

	for (i = 0; i < 4; i++)
	{
		MatrixRotate((VECTOR4 *)&conv_points[i].vx);

		if (conv_points[i].vz > 45000.0f)
		{
			current_vertex_computed_value++;
			return;
		}
	}

	if (fade)
	{
		// faded out from 10240 to 15000 away
		scale = (conv_points[2].vz + conv_points[3].vz) / 2.0f - 10240.0f;

		if (scale < 0.0f)
			scale = 1.0f;
		else if (scale - 4760.0f <= 0.0f)
			scale = 1.0f - scale / 4760.0f;
		else
			scale = 0.0f;

		poly->r = poly->r * scale;
		poly->g = poly->g * scale;
		poly->b = poly->b * scale;
	}

	polyblock = (int)poly;

	if (conv_points[0].vz >= 50.0f && conv_points[1].vz >= 50.0f && conv_points[2].vz >= 50.0f && conv_points[3].vz >= 50.0f)
	{
		do_clipping = 1;

		VertexPers(&conv_points[0]);
		VertexPers(&conv_points[1]);
		VertexPers(&conv_points[2]);
		VertexPers(&conv_points[3]);

		DrawPOLYFT4(trans);
	}
	else
	{
		DrawClippedPOLYFT4(trans);
	}

	current_vertex_computed_value++;
}

// [A]
// Puts the ground point pos (world) into conv_points[i], relative to the camera.
static void SetGroundPoint(int i, VECTOR4 *pos)
{
	conv_points[i].vx = pos->vx - camera_position.vx;
	conv_points[i].vy = pos->vy - camera_position.vy;
	conv_points[i].vz = pos->vz - camera_position.vz;
}

// [0x4adf39] [D]
// The pool of light on the road in front of a car (its headlights at night), 20 above the
// ground, additive.
void DrawCarPool(int id)
{
	CAR_DATA *cp;
	VECTOR4 p0, p1, p2, p3;

	cp = &car_data[id];

	p3.vy = 0.0f;
	p2.vy = 0.0f;
	p1.vy = 0.0f;
	p0.vy = 0.0f;
	p3.vx = -350.0f;
	p0.vx = -350.0f;
	p1.vz = -500.0f;
	p0.vz = -500.0f;
	p1.vx = 350.0f;
	p2.vx = 350.0f;
	p3.vz = -1500.0f;
	p2.vz = -1500.0f;

	CurrentRotationMatrix = &float_body_mat[id];

	MatrixRotateB(&p0);
	MatrixRotateB(&p1);
	MatrixRotateB(&p2);
	MatrixRotateB(&p3);

	p0.vx = p0.vx + (float)cp->location.vx;
	p0.vz = p0.vz + (float)cp->location.vz;
	p1.vx = p1.vx + (float)cp->location.vx;
	p1.vz = p1.vz + (float)cp->location.vz;
	p2.vx = p2.vx + (float)cp->location.vx;
	p2.vz = p2.vz + (float)cp->location.vz;
	p3.vx = p3.vx + (float)cp->location.vx;
	p3.vz = p3.vz + (float)cp->location.vz;

	p0.vy = (float)(-MapHeight(&p0) - 20);
	p1.vy = (float)(-MapHeight(&p1) - 20);
	p2.vy = (float)(-MapHeight(&p2) - 20);
	p3.vy = (float)(-MapHeight(&p3) - 20);

	SetGroundPoint(0, &p0);
	SetGroundPoint(1, &p1);
	SetGroundPoint(2, &p2);
	SetGroundPoint(3, &p3);

	conv_points[3].computed = current_vertex_computed_value;
	conv_points[2].computed = current_vertex_computed_value;
	conv_points[1].computed = current_vertex_computed_value;
	conv_points[0].computed = current_vertex_computed_value;

	DrawGroundQuad(&gPoolPoly, 3, 1);
}

// [0x4ae4b0] [D]
// A pedestrian's shadow: a square of half size size at pos (x, -, z), 2 above the ground.
void DrawPedShadow(float *pos, int size)
{
	VECTOR4 p0, p1, p2, p3;
	float s;

	p3.vy = 0.0f;
	p2.vy = 0.0f;
	p1.vy = 0.0f;
	p0.vy = 0.0f;

	s = (float)size;

	p0.vx = -s + pos[0];
	p0.vz = s + pos[2];
	p1.vx = s + pos[0];
	p1.vz = s + pos[2];
	p2.vx = s + pos[0];
	p2.vz = -s + pos[2];
	p3.vx = -s + pos[0];
	p3.vz = -s + pos[2];

	p0.vy = (float)(-MapHeight(&p0) - 2);
	p1.vy = (float)(-MapHeight(&p1) - 2);
	p2.vy = (float)(-MapHeight(&p2) - 2);
	p3.vy = (float)(-MapHeight(&p3) - 2);

	SetGroundPoint(0, &p0);
	SetGroundPoint(1, &p1);
	SetGroundPoint(2, &p2);
	SetGroundPoint(3, &p3);

	conv_points[3].computed = current_vertex_computed_value;
	conv_points[2].computed = current_vertex_computed_value;
	conv_points[1].computed = current_vertex_computed_value;
	conv_points[0].computed = current_vertex_computed_value;

	DrawGroundQuad(&gPedPoly, 2, 0);
}

// [0x4ae8e8] [D]
// Lifts the leaves on the ground close to pos (a car driving past).
void SwirlLeaves(IVECTOR *pos)
{
	LEAF *lf;
	int count;
	int dx;
	int dz;

	if (GamePaused)
		return;

	count = MAX_LEAVES;

	for (lf = leaf; count--; lf++)
	{
		if ((lf->flags & LEAF_ACTIVE) == 0)
			continue;

		if ((float)pos->vy + lf->position.vy <= -200.0f)
			continue;

		dx = pos->vx - float_to_int(lf->position.vx);
		dz = pos->vz - float_to_int(lf->position.vz);

		if (dx < 180 && dx > -180 && dz < 180 && dz > -180)
		{
			lf->direction.vy = (float)(-20 - (rand() & 63));
			lf->position.vy = lf->position.vy - 1.0f;
		}
	}
}

// [0x4aea04] [D]
// [unused] - doesn't seem to work
void SplashWater(CAR_DATA *cp)
{
	VECTOR4 pos;
	float s;
	float c;
	int angle;
	int rx;
	int rz;

	if (gDoInterview || next_leaf >= 80)
		return;

	if (game_frame & 2)
		angle = (cp->rotation.vy + 100) & 0xFFF;
	else
		angle = (cp->rotation.vy - 100) & 0xFFF;

	s = fsin(angle);
	c = fcos(angle);

	pos.vx = 10000.0f * s + (float)cp->location.vx;
	pos.vz = 10000.0f * c + (float)cp->location.vz;

	rx = float_to_int(pos.vx);
	rz = float_to_int(pos.vz);

	if (ROADS_GetRoadOrJunction(rx, rz) == -1 && ROADS_GetRoadInfo(rx, rz) && model_pointers[RouteData.type] &&
		(model_pointers[RouteData.type]->shape_flags & SHAPE_FLAG_WATER) == 0)
	{
		pos.vy = (float)-RouteData.height;

		AddLeaf(&pos, rand() & 7, 1);
	}
}

// [0x4aeb75] [A]
// Blows leaves (trees) or litter from the ground debris objects 12000 - 15000 from the camera
// when the player drives fast.
// NOTE: the original never sets pos.vz (stack garbage); set to the z it looks the road up at
void AddGroundDebris(void)
{
	CELL_OBJECT *cop;
	MODEL *model;
	VECTOR4 pos;
	short rnd;
	int count;
	int dx;
	int dz;

	if (!gDoDebris || gDoInterview || (car_data[0].wheel_speed < 100000 && car_data[0].wheel_speed > -100000))
		return;

	count = 0;

	for (ground_debris_ptr = ground_debris; count < 16; ground_debris_ptr++)
	{
		cop = *ground_debris_ptr;

		if (!cop)
			break;

		model = model_pointers[cop->type];

		dx = (int)(cop->pos.vx - camera_position.vx);
		dz = (int)(cop->pos.vz - camera_position.vz);

		if (dx < 0)
			dx = -dx;

		if (dz < 0)
			dz = -dz;

		if (dx > 15000 || dz > 15000 || (dx < 12000 && dz < 12000) || next_leaf > 80)
			break;

		rnd = rand();
		pos.vx = cop->pos.vx - 1024.0f + (float)(rnd & 0x7FF);
		pos.vz = cop->pos.vz - 1024.0f + (float)(rand() & 0x7FF);

		ROADS_GetRoadInfo(float_to_int(pos.vx), float_to_int(pos.vz));
		pos.vy = (float)-RouteData.height;

		if (model->flags2 & MODEL_FLAG_TREE)
			AddLeaf(&pos, (rnd & 3) + 1, 1);
		else
			AddLeaf(&pos, (rnd & 3) + 1, (rand() & 3) + 2);

		count++;
	}
}

// [0x4aed92] [D]
// A light flare (light_flares) at position1 (camera space); position2 is its world position.
// glow: type | 0x80. type bits 0xe00000 also go to the flags.
void AddLightFlare(VECTOR4 *position1, VECTOR4 *position2, short dir, short yang, int type, int glow, int num, int cycle)
{
	if (!lightflare_texture || !gDoLightFlare)
		return;

	light_flares->position.vx = position1->vx;
	light_flares->position.vy = position1->vy;
	light_flares->position.vz = position1->vz - 10.0f;
	light_flares->position2 = *position2;
	light_flares->flags = SMOKE_LIGHTFLARE | SMOKE_ACTIVE;
	light_flares->next = NULL;
	light_flares->prev = NULL;

	if (glow)
		light_flares->type = type | 0x80;
	else
		light_flares->type = type;

	light_flares->dir = dir;
	light_flares->yang = yang;
	light_flares->num = num;
	light_flares->cycle = cycle;
	light_flares->flags |= type & 0xE00000;

	light_flares++;
	num_light_flares++;
}

// [0x4aeeb5] [D]
// Water splashes around a car (in water at height).
void AddWater(int id, int height)
{
	SMOKE *sm;
	VECTOR4 pos;
	int count;
	int num;

	CurrentRotationMatrix = &float_body_mat[id];

	count = 20;

	while (count--)
	{
		num = AllocateSmoke();

		if (num < 0)
			break;

		pos.vx = (float)((rand() & 0x1FF) - 256);
		pos.vz = (float)((rand() & 0x3FF) - 512);

		MatrixRotateB(&pos);

		pos.vx = pos.vx + (float)car_data[id].location.vx;
		pos.vy = (float)-height;
		pos.vz = pos.vz + (float)car_data[id].location.vz;

		sm = &smoke[num];

		sm->position = pos;
		sm->flags = SMOKE_WATER | SMOKE_ACTIVE;
		sm->transparency = 200;
		sm->t_step = (rand() & 15) + 10;
		sm->start_w = 100;
		sm->final_w = 400;
		sm->step = 10;
		sm->life = 100;
	}
}

// [A]
// A light of car id at local position pos: world position (world) and camera space (cam).
// NOTE: not a function in the original: the car light functions repeat it
static void CarLightPosition(int id, VECTOR4 *pos, VECTOR4 *world, VECTOR4 *cam)
{
	CurrentRotationMatrix = &float_body_mat[id];
	MatrixRotateB(pos);

	pos->vx = pos->vx + (float)car_data[id].location.vx;
	pos->vy = pos->vy + (float)-car_data[id].location.vy;
	pos->vz = pos->vz + (float)car_data[id].location.vz;

	if (world)
		*world = *pos;

	cam->vx = pos->vx - camera_position.vx;
	cam->vy = pos->vy - camera_position.vy;
	cam->vz = pos->vz - camera_position.vz;

	CurrentRotationMatrix = &inv_camera_matrix;
	MatrixRotate(cam);
}

// [A]
// Adds a brake light (brake_lights) at pos (camera space).
static void AddBrakeLightAt(VECTOR4 *pos, VECTOR4 *pos2, int dir, int yang, int id, int cycle)
{
	brake_lights->position = *pos;
	brake_lights->position2 = *pos2;
	brake_lights->flags = SMOKE_BRAKE_LIGHT;
	brake_lights->prev = NULL;
	brake_lights->next = NULL;
	brake_lights->dir = dir;
	brake_lights->yang = yang;
	brake_lights->num = id;
	brake_lights->cycle = (short)cycle;

	brake_lights++;
	num_brake_lights++;
}

// [A]
// The two points of a double light (side by side, or one above the other when vertical) at
// pLight less the damage, offset apart.
static void DoubleLightPoints(VECTOR4 *pLight, VECTOR4 *p1, VECTOR4 *p2, int vertical, int offset, float dz)
{
	if (vertical)
	{
		p1->vx = p2->vx = pLight->vx;
		p1->vz = p2->vz = pLight->vz + dz;
		p1->vy = pLight->vy - (float)offset;
		p2->vy = (float)offset + pLight->vy;
	}
	else
	{
		p1->vy = p2->vy = pLight->vy;
		p1->vz = p2->vz = pLight->vz + dz;
		p1->vx = pLight->vx - (float)offset;
		p2->vx = (float)offset + pLight->vx;
	}
}

// [0x4aeff7] [D]
// The side (tail) lights of a car seen from behind: brake lights and, at night, flares.
void AddSideLights(int id)
{
	CAR_COSMETICS *car_cos;
	VECTOR4 *pLight;
	VECTOR4 position1;
	VECTOR4 position2;
	VECTOR4 position3;
	VECTOR4 world;
	int doubleFlag;
	int verticalFlag;
	int doLights;
	int offset;
	int damage;
	int angle;
	int dir;
	int yang;
	int cycle;
	int i;

	if (num_brake_lights > 200)
		return;

	doLights = 1;
	car_cos = &car_cosmetics[(u_char)car_data[id].model];

	dir = car_data[id].rotation.vx;
	yang = car_data[id].direction;

	angle = DIFF_ANGLES(yang, -camera_angle.vy & 0xFFF);

	if (angle > 800 || angle < -800)
		return;

	doubleFlag = car_cos->extraInfo & 0x4000;
	verticalFlag = car_cos->extraInfo & 0x1000;

	for (i = 0; i < 2; i++)
	{
		if (i)
		{
			damage = car_data[id].damage[3];
			pLight = &car_cos->brakeLight[0];
			cycle = 6;
		}
		else
		{
			damage = car_data[id].damage[4];
			pLight = &car_cos->brakeLight[1];
			cycle = 4;
		}

		if (damage >= 500)
			continue;

		if (doubleFlag)
		{
			offset = ((car_cos->extraInfo & 0x300) >> 6) + 10;

			DoubleLightPoints(pLight, &position1, &position2, verticalFlag, offset, -(float)(damage >> 7));

			CarLightPosition(id, &position1, &position3, &position1);
			CarLightPosition(id, &position2, &world, &position2);

			if (position1.vz < 50.0f)
				continue;

			if (gNight)
			{
				AddLightFlare(&position1, &position3, dir, yang, 5, 1, -1, -1);
				AddLightFlare(&position2, &position3, dir, yang, 5, 1, -1, -1);
			}

			if (doLights)
			{
				AddBrakeLightAt(&position1, &position3, dir, yang, id, cycle);

				// NOTE: position2 is the second light's camera space position, as in the original
				AddBrakeLightAt(&position2, &position2, dir, yang, id, cycle + 1);
			}
		}
		else
		{
			position1.vx = pLight->vx;
			position1.vy = pLight->vy;
			position1.vz = pLight->vz - (float)(damage >> 7);

			CarLightPosition(id, &position1, &position3, &position1);

			if (position1.vz < 50.0f)
				continue;

			if (gNight)
				AddLightFlare(&position1, &position3, dir, yang, 5, 1, -1, -1);

			if (doLights)
				AddBrakeLightAt(&position1, &position3, dir, yang, id, cycle);
		}
	}
}

// [0x4af78c] [D]
// The brake lights of a car at night (not parked; the player's with its lights on).
void AddBrakeLight(int id)
{
	CAR_COSMETICS *car_cos;
	VECTOR4 *vec;
	VECTOR4 v1;
	VECTOR4 v2;
	VECTOR4 v3;
	VECTOR4 world;
	int doubleFlag;
	int verticalFlag;
	int doLights;
	int offset;
	int damage;
	int rotx;
	int direction;
	int cycle;
	int i;

	car_cos = &car_cosmetics[(u_char)car_data[id].model];

	if (num_brake_lights > 200 || !gNight)
		return;

	doLights = 1;

	if (id)
	{
		if (civilian_ai_data[id - 1].ctrlState == CIVSTATE_PARKED)
			return;
	}
	else if (!gDoPlayerHeadlight)
	{
		return;
	}

	rotx = car_data[id].rotation.vx;
	direction = car_data[id].direction;

	doubleFlag = car_cos->extraInfo & 0x4000;
	verticalFlag = car_cos->extraInfo & 0x1000;

	for (i = 0; i < 2; i++)
	{
		if (i)
		{
			damage = car_data[id].damage[3];
			vec = &car_cos->brakeLight[0];
			cycle = 6;
		}
		else
		{
			damage = car_data[id].damage[4];
			vec = &car_cos->brakeLight[1];
			cycle = 4;
		}

		if (damage >= 500)
			continue;

		if (doubleFlag)
		{
			offset = ((car_cos->extraInfo & 0x300) >> 6) + 10;

			DoubleLightPoints(vec, &v2, &v1, verticalFlag, offset, -(float)(damage >> 7));

			CarLightPosition(id, &v2, &v3, &v2);
			CarLightPosition(id, &v1, &world, &v1);

			if (v2.vz >= 50.0f && doLights)
			{
				AddBrakeLightAt(&v2, &v3, rotx, direction, id, cycle);

				// NOTE: the second light's position2 is its camera space position, as in the original
				AddBrakeLightAt(&v1, &v1, rotx, direction, id, cycle + 1);
			}
		}
		else
		{
			// no double flag
			v2.vx = vec->vx;
			v2.vy = vec->vy;
			v2.vz = vec->vz - (float)(damage >> 7);

			CarLightPosition(id, &v2, &v3, &v2);

			if (v2.vz >= 50.0f && doLights)
				AddBrakeLightAt(&v2, &v3, rotx, direction, id, cycle);
		}
	}
}

// [0x4afebb] [D]
// The headlight flares of a car facing the camera at night.
void AddHeadLights(int id)
{
	CAR_COSMETICS *car_cos;
	VECTOR4 *pLight;
	VECTOR4 position1;
	VECTOR4 position2;
	VECTOR4 position3;
	VECTOR4 world;
	int doubleFlag;
	int verticalFlag;
	int offset;
	int damage;
	int maxDamage;
	int angle;
	int dir;
	int cycle;
	short rotx;
	int i;

	maxDamage = 1000;
	car_cos = &car_cosmetics[(u_char)car_data[id].model];

	if (id)
	{
		if (civilian_ai_data[id - 1].ctrlState == CIVSTATE_PARKED)
			return;
	}
	else if (!gDoPlayerHeadlight)
	{
		return;
	}

	rotx = car_data[id].rotation.vx;
	dir = ((u_short)car_data[id].direction + 2048) & 0xFFF;
	angle = DIFF_ANGLES(dir, -camera_angle.vy);

	if (angle > 1536 || angle < -1536)
		return;

	doubleFlag = car_cos->extraInfo & 0x8000;
	verticalFlag = car_cos->extraInfo & 0x2000;

	for (i = 0; i < 2; i++)
	{
		if (i)
		{
			damage = car_data[id].damage[1];
			pLight = &car_cos->headLight[0];
			cycle = 2;
		}
		else
		{
			damage = car_data[id].damage[0];
			pLight = &car_cos->headLight[1];
			cycle = 0;
		}

		if (damage >= maxDamage)
			continue;

		if (doubleFlag)
		{
			offset = ((car_cos->extraInfo & 0xC00) >> 8) + 10;

			DoubleLightPoints(pLight, &position1, &position2, verticalFlag, offset, (float)(damage >> 6));

			CarLightPosition(id, &position1, &position3, &position1);
			CarLightPosition(id, &position2, &world, &position2);

			if (position1.vz >= 50.0f && gNight)
			{
				AddLightFlare(&position1, &position3, rotx, dir, 0x800004, 1, id, cycle);
				AddLightFlare(&position2, &position3, rotx, (dir + 50) & 0xFFF, 0x800004, 1, id, cycle + 1);
			}
		}
		else
		{
			position1.vx = pLight->vx;
			position1.vy = pLight->vy;
			position1.vz = (float)(damage >> 6) + pLight->vz;

			CarLightPosition(id, &position1, &position3, &position1);

			if (position1.vz >= 50.0f && gNight)
				AddLightFlare(&position1, &position3, rotx, dir, 0x800004, 1, id, cycle);
		}
	}
}

// [0x4b0471] [D]
// The reverse lights of a car.
void AddReverseLight(int id)
{
	CAR_COSMETICS *car_cos;
	VECTOR4 *pLight;
	float vx;
	int damage;
	int i;

	car_cos = &car_cosmetics[(u_char)car_data[id].model];

	if (num_brake_lights > 200)
		return;

	// doLights = 1;
	// angle = DIFF_ANGLES(...);
	//
	// if (angle > 900 || angle < -900)
	//   doLights = 0;
	// NOTE: the original computes the angle (DIFF_ANGLES(direction, -camera_angle.vy & 0xfff))
	// and does not use it

	pLight = &car_cos->revLight;

	for (i = 0; i < 2; i++)
	{
		if (i)
		{
			damage = car_data[id].damage[3];
			vx = -pLight->vx;
		}
		else
		{
			damage = car_data[id].damage[4];
			vx = pLight->vx;
		}

		if (damage >= 500)
			continue;

		brake_lights->position.vx = vx;
		brake_lights->position.vy = car_cos->revLight.vy;
		brake_lights->position.vz = car_cos->revLight.vz - (float)(damage >> 7);

		CarLightPosition(id, &brake_lights->position, NULL, &brake_lights->position);

		if (brake_lights->position.vz >= 50.0f)
		{
			brake_lights->position2.vz = brake_lights->position.vz;
			brake_lights->flags = SMOKE_REVERSE_LIGHT;
			brake_lights->next = NULL;
			brake_lights->prev = NULL;

			brake_lights++;
			num_brake_lights++;
		}
	}
}

// [0x4b0717] [D]
// The indicator lights of a car: type 0 left, 1 right, 2 both (hazard).
void AddIndicatorLight(int id, int type)
{
	CAR_COSMETICS *car_cos;

	car_cos = &car_cosmetics[(u_char)car_data[id].model];

	if (type == 0 || type == 2)
	{
		if (car_data[id].damage[3] < 500)
			AddSingleIndicator(id, &car_cos->backInd[0], SMOKE_BACK_INDICATOR);

		if (car_data[id].damage[1] < 500)
			AddSingleIndicator(id, &car_cos->frontInd[0], SMOKE_FRONT_INDICATOR);
	}

	if (type == 1 || type == 2)
	{
		if (car_data[id].damage[4] < 500)
			AddSingleIndicator(id, &car_cos->backInd[1], SMOKE_BACK_INDICATOR);

		if (car_data[id].damage[0] < 500)
			AddSingleIndicator(id, &car_cos->frontInd[1], SMOKE_FRONT_INDICATOR);
	}

	if (!GamePaused)
	{
		gLightTableNo[id]++;
		gLightTableNo[id] &= 31;
	}
}

// [0x4b0836] [D]
// One indicator light (flags: SMOKE_BACK_INDICATOR or SMOKE_FRONT_INDICATOR) at pos, its
// brightness from the blink cycle (gIndicatorLookup).
void AddSingleIndicator(int id, VECTOR4 *pos, int flags)
{
	int doLights;
	int index;
	int direction;
	int angle;
	short rotx;

	doLights = 1;
	index = gLightTableNo[id] & 31;
	rotx = car_data[id].rotation.vx;

	if (flags == SMOKE_BACK_INDICATOR)
		direction = car_data[id].direction;
	else
		direction = (car_data[id].direction + 2048) & 0xFFF;

	// DIFF_ANGLES...
	angle = DIFF_ANGLES(direction, -camera_angle.vy & 0xFFF);

	if (angle > 900 || angle < -900)
		doLights = 0;

	brake_lights->position = *pos;

	CarLightPosition(id, &brake_lights->position, NULL, &brake_lights->position);

	if (brake_lights->position.vz >= 50.0f && doLights)
	{
		brake_lights->position2 = brake_lights->position;
		brake_lights->flags = flags;
		brake_lights->prev = NULL;
		brake_lights->next = NULL;
		brake_lights->type = (u_char)gIndicatorLookup[index >> 1];
		brake_lights->dir = rotx;
		brake_lights->yang = direction;

		brake_lights++;
		num_brake_lights++;
	}
}

// [0x4b0ab8] [D]
// Empty in the original.
void sub_4B0AB8(void)
{
}

// [0x4b0abd] [D]
// A traffic light of object cop at (x, y, z) in its space; flags: 0x200 red, 0x400 amber,
// 0x800 green. Adds its flare (always, whatever the light flare option) and its light
// (tlights).
void AddTrafficLight(CELL_OBJECT *cop, float x, float y, float z, short flags)
{
	VECTOR4 position2;
	int oldLightFlare;
	int type;
	int yang;
	int angle;

	if (!EffectsInfo.DoTrafficLights)
		return;

	yang = cop->yang;
	angle = (-camera_angle.vy & 0xFFF) - 2048;	// NOTE: not used
	oldLightFlare = gDoLightFlare;
	gDoLightFlare = 1;

	tlights->position.vx = x;
	tlights->position.vy = y;
	tlights->position.vz = z;

	CurrentRotationMatrix = &(*matrix_mem)[-cop->yang & 0xFFF];
	MatrixRotate(&tlights->position);

	tlights->position.vx = tlights->position.vx + cop->pos.vx;
	tlights->position.vy = tlights->position.vy + cop->pos.vy;
	tlights->position.vz = tlights->position.vz + cop->pos.vz;

	position2 = tlights->position;

	tlights->position.vx = tlights->position.vx - camera_position.vx;
	tlights->position.vy = tlights->position.vy - camera_position.vy;
	tlights->position.vz = tlights->position.vz - camera_position.vz;

	CurrentRotationMatrix = &inv_camera_matrix;
	MatrixRotate(&tlights->position);

	if (tlights->position.vz < 50.0f)
	{
		gDoLightFlare = oldLightFlare;
		return;
	}

	LightIndex = find_lamp_streak((int)cop + float_to_int(x) + float_to_int(y));

	if (LightIndex < 0)
		type = 0;
	else
		type = 0x200000;

	switch (flags)
	{
		case 0x200:
			AddLightFlare(&tlights->position, &position2, 0, yang, type + 1, 1, -1, LightIndex);
			break;
		case 0x400:
			AddLightFlare(&tlights->position, &position2, 0, yang, type + 3, 1, -1, LightIndex);
			break;
		case 0x800:
			AddLightFlare(&tlights->position, &position2, 0, yang, type + 2, 1, -1, LightIndex);
			break;
	}

	tlights->position2 = tlights->position;
	tlights->flags = flags;
	tlights->type = 100;
	tlights->dir = 0;
	tlights->yang = yang;

	tlights++;
	num_traffic_lights++;

	gDoLightFlare = oldLightFlare;
}

// [0x4b0dd9] [D]
// The flashing lights on the roof of a cop car (upright, not wrecked): two rows of four,
// red and blue, by the phase of its siren life.
// NOTE: lights is never advanced: all the lights are worked out in lights_array[0]
void AddCopCarLight(int id, short *coplife, float health)
{
	CAR_COSMETICS *car_cos;
	int oldLightFlare;
	int phase;
	int dir;
	int loop;
	int i;
	short xrot;
	short angle;

	car_cos = &car_cosmetics[(u_char)car_data[id].model];

	if (!EffectsInfo.DoCopCarLights || car_data[id].where.m[1][1] < 100 || health < 0.01f)
		return;

	oldLightFlare = gDoLightFlare;
	gDoLightFlare = 1;

	xrot = car_data[id].rotation.vx;
	angle = (car_data[id].direction + 2048) & 0xFFF;

	loop = 2;
	dir = 1;

	phase = ((*coplife + ((short)id << 9)) & 0xFFF) >> 9;

	while (loop--)
	{
		i = 4;

		while (i--)
		{
			lights->position.vx = (float)dir * light_xpos[i];
			lights->position.vy = car_cos->policeLight[1].vy;
			lights->dir = car_data[id].rotation.vx;
			lights->yang = (car_data[id].direction + 2048) & 0xFFF;
			lights->position.vz = 0.0f;

			CurrentRotationMatrix = &float_body_mat[id];
			MatrixRotateB(&lights->position);

			CurrentRotationMatrix = &inv_camera_matrix;

			lights->position.vx = (float)car_data[id].location.vx - camera_position.vx + lights->position.vx;
			lights->position.vy = (float)-car_data[id].location.vy - camera_position.vy + lights->position.vy;
			lights->position.vz = (float)car_data[id].location.vz - camera_position.vz + lights->position.vz;

			MatrixRotate(&lights->position);

			if (phase < 3)
			{
				if (loop == 1)
					AddLightFlare(&lights->position, &lights->position, xrot, angle, phase == 1 ? 11 : 9, 0, -1, -1);
				else
					AddLightFlare(&lights->position, &lights->position, xrot, angle, phase == 1 ? 12 : 10, 0, -1, -1);
			}

			phase = (phase & 7) + 1;
		}

		dir = -dir;
	}

	gDoLightFlare = oldLightFlare;
}

// [0x4b10fc] [D]
// A street lamp at night, dimmer (and flickering) the more it was hit (damaged_lamp); out
// after 3 hits.
void AddDamageStreetLight(CELL_OBJECT *cop, float x, float y, float z, short type, short dir)
{
	DAMAGE_LAMP *dam;
	VECTOR4 pos;
	short transparency;
	int i;

	transparency = 255;
	dam = damaged_lamp;

	if (!gNight)
		return;

	for (i = 0; i < 4; i++, dam++)
	{
		if (cop == dam->cop)
		{
			if ((u_char)dam->hits >= 3)
				return;

			if (GamePaused)
				transparency -= (u_char)dam->hits << 6;
			else
				transparency -= (rand() & 127) + ((u_char)dam->hits << 6);

			break;
		}
	}

	pos.vx = cop->pos.vx;
	pos.vy = y + cop->pos.vy;
	pos.vz = cop->pos.vz;

	AddHazeLight(cop, x, y, z, type, dir, transparency);
	AddPendingLamp(&pos, 2000, transparency, transparency, transparency);
}

// [0x4b120c] [D]
// num_leaves leaves (Type 1) or bits of litter (2 .. 5: litter_texture) blown up around
// Position.
void AddLeaf(VECTOR4 *Position, int num_leaves, int Type)
{
	LEAF *myleaf;
	int temprand;
	int num;
	int i;

	if (!gDoDebris || !debris_texture)
		return;

	for (i = 0; i < num_leaves; i++)
	{
		num = AllocateLeaf();

		if (num < 0)
			break;

		myleaf = &leaf[num];

		myleaf->position.vx = (float)(rand() & 0xFE) + Position->vx - 128.0f;
		myleaf->position.vy = Position->vy;
		myleaf->position.vz = (float)(rand() & 0xFE) + Position->vz - 128.0f;

		myleaf->direction.vz = 0.0f;
		myleaf->direction.vy = 0.0f;
		myleaf->direction.vx = 0.0f;

		myleaf->life = 700;
		myleaf->flags = LEAF_ACTIVE;
		myleaf->step = (rand() & 3) + 2;

		if (Type == 1)
		{
			// leaves: white, green-yellow, or orange
			temprand = rand() & 3;

			if (temprand == 0)
			{
				myleaf->rgb.r = 255;
				myleaf->rgb.g = 255;
				myleaf->rgb.b = 255;
			}
			else
			{
				if (temprand == 1)
				{
					myleaf->rgb.r = (rand() & 63) + 110;
					myleaf->rgb.g = 170;
				}
				else
				{
					myleaf->rgb.r = 190;
					myleaf->rgb.g = (rand() & 63) + 130;
				}

				myleaf->rgb.b = 130;
			}

			myleaf->pos = rand() & 0xFE;
		}
		else
		{
			myleaf->rgb.r = 255;
			myleaf->rgb.g = 255;
			myleaf->rgb.b = (rand() & 63) + 192;
			myleaf->pos = (u_char)((rand() & 31) - 16);
		}

		if (ambient_light_level <= 0.7f)
		{
			myleaf->rgb.r = (int)((ambient_light_level + 0.3f) * (float)myleaf->rgb.r);
			myleaf->rgb.g = (int)((ambient_light_level + 0.3f) * (float)myleaf->rgb.g);
			myleaf->rgb.b = (int)((ambient_light_level + 0.3f) * (float)myleaf->rgb.b);
		}

		myleaf->sin_index1 = rand() & 0xFFF;
		myleaf->sin_index2 = rand() & 0xFFF;
		myleaf->sin_addition1 = (rand() & 63) - 64;
		myleaf->sin_addition2 = (rand() & 63) - 64;
		myleaf->type = Type;
	}
}

// [0x4b14b7] [D]
// NOTE: no callers
void AddDebris(VECTOR4 *Position, VECTOR4 *Direction, RGB *colour, u_short life, int step)
{
	DEBRIS *mydebris;
	int num;

	num = AllocateDebris();

	if (num < 0)
		return;

	mydebris = &debris[num];

	mydebris->position.vx = float_to_int(Position->vx);
	mydebris->position.vy = float_to_int(Position->vy);
	mydebris->position.vz = float_to_int(Position->vz);
	mydebris->direction.vx = float_to_int(Direction->vx);
	mydebris->direction.vy = float_to_int(Direction->vy);
	mydebris->direction.vz = float_to_int(Direction->vz);
	mydebris->life = life;
	mydebris->flags = DEBRIS_ACTIVE;
	mydebris->pos = 0;
	mydebris->step = step;
	mydebris->rgb.r = colour->r;
	mydebris->rgb.g = colour->g;
	mydebris->rgb.b = colour->b;
}

// [A]
// A point of car id at local position pos, to the world.
// NOTE: not a function in the original
static void CarPointToWorld(int id, VECTOR4 *pos)
{
	CurrentRotationMatrix = &float_body_mat[id];
	MatrixRotateB(pos);

	pos->vx = pos->vx + (float)car_data[id].location.vx;
	pos->vy = pos->vy - (float)car_data[id].location.vy;
	pos->vz = pos->vz + (float)car_data[id].location.vz;
}

// [0x4b15fc] [D]
// Exhaust smoke of a car; with the super fast cars cheat the player's exhaust flames too.
void AddExhaustSmoke(int id, int WheelSpeed)
{
	CAR_COSMETICS *car_cos;
	VECTOR4 exhaust;
	VECTOR4 drift;

	if (GamePaused)
		return;

	car_cos = &car_cosmetics[(u_char)car_data[id].model];

	exhaust = car_cos->exhaust;
	exhaust.vz = exhaust.vz - (float)(car_data[id].damage[3] >> 6);

	CarPointToWorld(id, &exhaust);

	drift.vx = 0.0f;
	drift.vy = 0.0f;
	drift.vz = 5.0f;

	MatrixRotateB(&drift);

	Setup_Smoke(&exhaust, 20, 50, 2, WheelSpeed, &drift, 1);

	if (id == 0 && gCheatFlags.SuperFastCars)
		Setup_Flame(&exhaust, 50, 200, (rand() & 3) + 8);
}

// [0x4b1752] [D]
// Smoke from a damaged engine: black or white.
void AddSmokingEngine(int id, int BlackSmoke, int WheelSpeed)
{
	VECTOR4 smoke_pos;
	VECTOR4 drift;

	if (GamePaused)
		return;

	smoke_pos = car_cosmetics[(u_char)car_data[id].model].smoke;

	CarPointToWorld(id, &smoke_pos);

	GetSmokeDrift(&drift, 0);

	if (BlackSmoke)
		Setup_Smoke(&smoke_pos, 70, 250, 1, WheelSpeed, &drift, 0);
	else
		Setup_Smoke(&smoke_pos, 70, 250, 2, WheelSpeed, &drift, 0);
}

// [0x4b186d] [D]
// Two flames from a burning engine, each moving about its fire point.
void AddFlamingEngine(int id)
{
	CAR_COSMETICS *car_cos;
	VECTOR4 fire;
	int i;

	if (GamePaused)
		return;

	car_cos = &car_cosmetics[(u_char)car_data[id].model];

	// IDA fucked up the iterator, per usual
	i = 2;

	while (i--)
	{
		fire = car_cos->fire;
		fire.vx = fire.vx + (float)FlameOffset[id][i];

		if (--FlameCount[id][i] < 1)
		{
			FlameCount[id][i] = rand() & 7;
			FlameLife[id][i] = (rand() & 3) + 8;
			FlameOffset[id][i] = (rand() & 63) - 32;
		}

		CarPointToWorld(id, &fire);

		Setup_Flame(&fire, 50, 200, FlameLife[id][i]);
	}
}

// [0x4b1a16] [D]
// The haze around a lamp of object cop at (x, y, z) in its space, at night or in fog; in the
// night rain also its reflection on the road.
void AddHazeLight(CELL_OBJECT *cop, float x, float y, float z, short type, short yang, short transparency)
{
	VECTOR4 pos;
	int old_lf;
	int dir;

	if (!lightflare_texture || !gDoLightFlare || (!gNight && !do_fogging))
		return;

	dir = cop->yang;
	old_lf = gDoLightFlare;
	gDoLightFlare = 1;

	pos.vx = x;
	pos.vy = y;
	pos.vz = z;

	CurrentRotationMatrix = &(*matrix_mem)[-dir & 0xFFF];
	MatrixRotate(&pos);

	pos.vx = pos.vx + cop->pos.vx;
	pos.vy = pos.vy + cop->pos.vy;
	pos.vz = pos.vz + cop->pos.vz;

	if (gDoRain && gNight)
	{
		if (transparency > 255)
			transparency = 255;

		switch (type)
		{
			case 6:
				StoreLightReflection(&pos, 6, transparency);
				break;
			case 7:
				StoreLightReflection(&pos, 7, transparency);
				break;
			case 8:
				StoreLightReflection(&pos, 8, transparency);
				break;
			default:
				StoreLightReflection(&pos, 0, transparency);
				break;
		}
	}

	pos.vx = pos.vx - camera_position.vx;
	pos.vy = pos.vy - camera_position.vy;
	pos.vz = pos.vz - camera_position.vz;

	CurrentRotationMatrix = &inv_camera_matrix;
	MatrixRotate(&pos);

	if (pos.vz >= 50.0f && haze_texture)
	{
		light_flares->position.vx = pos.vx;
		light_flares->position.vy = pos.vy;
		light_flares->position.vz = pos.vz;
		light_flares->position2.vz = light_flares->position.vz;
		light_flares->flags = SMOKE_HAZE | SMOKE_ACTIVE;
		light_flares->next = NULL;
		light_flares->prev = NULL;
		light_flares->type = type;
		light_flares->yang = yang;
		light_flares->dir = dir;
		light_flares->transparency = transparency;

		light_flares++;
		num_light_flares++;
	}

	gDoLightFlare = old_lf;
}

// [0x4b1c80] [D]
// A flock of num birds flying up from pos (one flock at a time).
void CreateBirds(VECTOR4 *pos, int num)
{
	BIRD *bd;
	int count;

	count = num;
	bd = birds;

	if (!bird1_texture || !bird2_texture || birds[0].life)
		return;

	gNumBirds = num;

	while (count--)
	{
		bd->position.vx = (float)((rand() & 63) - 32) + pos->vx;
		bd->position.vy = (float)((rand() & 63) - 32) + pos->vy;
		bd->position.vz = (float)((rand() & 63) - 32) + pos->vz;
		bd->direction.vx = (float)((rand() & 63) - 32);
		bd->direction.vy = (float)(-5 - (rand() & 15));
		bd->direction.vz = (float)((rand() & 63) - 32);
		bd->life = 60;
		bd->step = 10;

		bd++;
	}
}

// [A]
// Projects v (camera space) to the screen: x, y, oow.
// NOTE: not a function in the original: the effect draw functions repeat it
static void ProjectVertex(GrVertex *out, VECTOR4 *v)
{
	float scale;

	out->oow = ooz_table[(int)v->vz];
	scale = DIST * out->oow;

	out->x = v->vx * scale + screen_xmid - VERTEX_SNAPPER;
	out->y = v->vy * scale + screen_ymid - VERTEX_SNAPPER;
}

// [A]
static void SetVertexColour(GrVertex *out, float r, float g, float b)
{
	out->r = r;
	out->g = g;
	out->b = b;
}

// [A]
static void SetVertexTexCoord(GrVertex *out, float s, float t)
{
	out->tmuvtx[0].sow = s * out->oow;
	out->tmuvtx[0].tow = t * out->oow;
}

// [A]
static void SetTexture(TEX_INFO *tex)
{
	if (tex->tset != last_tset)
	{
		last_tset = tex->tset;
		grTexSource(GR_TMU0, TextureSlots[tex->tset].texture_ram_address, GR_MIPMAPLEVELMASK_BOTH, TextureSlots[tex->tset].TInfo);
	}
}

// [0x4b1daa] [D]
// A leaf or bit of litter: a quad of its rotated triangle; with shadow its grey shadow on the
// ground (the colour combine is set by DisplayLeaves).
void DrawLeaf(LEAF *lf, int shadow)
{
	TRI_POINT *point_rot;
	TEX_INFO *tinfo;
	GrVertex vA, vB, vC, vD;
	VECTOR4 p0, p1, p2, p3;
	int pos;

	if (!gDoDebris)
		return;

	pos = lf->pos;

	if (lf->type == 1)
	{
		tinfo = leaf_texture;
		point_rot = &leaf_rot[pos];
	}
	else
	{
		tinfo = litter_texture[lf->type - 2];
		point_rot = &litter_rot[pos];
	}

	if (!tinfo)
		return;

	SetTexture(tinfo);

	if (shadow)
	{
		if (!gDrawShadows)
			return;

		// the triangle flat on the ground, 10 above it
		p0.vx = point_rot->v0.vx + lf->position.vx - camera_position.vx;
		p0.vz = point_rot->v0.vz + lf->position.vz - camera_position.vz;
		p2.vy = (float)(-MapHeight(&lf->position) - 10) - camera_position.vy;
		p1.vy = p2.vy;
		p0.vy = p2.vy;

		MatrixRotate(&p0);

		if (p0.vz < 50.0f)
			return;

		p1.vx = point_rot->v1.vx + lf->position.vx - camera_position.vx;
		p1.vz = point_rot->v1.vz + lf->position.vz - camera_position.vz;
		p2.vx = point_rot->v2.vx + lf->position.vx - camera_position.vx;
		p2.vz = point_rot->v2.vz + lf->position.vz - camera_position.vz;

		MatrixRotate(&p1);
		MatrixRotate(&p2);

		if (FlipX)
		{
			p0.vx = -p0.vx;
			p1.vx = -p1.vx;
			p2.vx = -p2.vx;
		}

		ProjectVertex(&vA, &p0);
		ProjectVertex(&vB, &p1);
		ProjectVertex(&vC, &p2);

		SetVertexColour(&vA, 96.0f, 96.0f, 96.0f);
		SetVertexColour(&vB, 96.0f, 96.0f, 96.0f);
		SetVertexColour(&vC, 96.0f, 96.0f, 96.0f);

		// NOTE: the texture coordinates are not set (stack garbage in the original); the
		// shadow is drawn untextured
		guDrawTriangleWithClip(&vA, &vB, &vC);
		return;
	}

	p0.vx = point_rot->v0.vx + lf->position.vx - camera_position.vx;
	p0.vy = point_rot->v0.vy + lf->position.vy - camera_position.vy;
	p0.vz = point_rot->v0.vz + lf->position.vz - camera_position.vz;

	MatrixRotate(&p0);

	p1.vx = point_rot->v1.vx + lf->position.vx - camera_position.vx;
	p1.vy = point_rot->v1.vy + lf->position.vy - camera_position.vy;
	p1.vz = point_rot->v1.vz + lf->position.vz - camera_position.vz;

	p2.vx = point_rot->v2.vx + lf->position.vx - camera_position.vx;
	p2.vy = point_rot->v2.vy + lf->position.vy - camera_position.vy;
	p2.vz = point_rot->v2.vz + lf->position.vz - camera_position.vz;

	MatrixRotate(&p1);
	MatrixRotate(&p2);

	// the triangle made a quad: p1 and p3 on either side of its middle
	p3.vx = (p2.vx - p0.vx) / 2.0f + p1.vx;
	p3.vy = (p2.vy - p0.vy) / 2.0f + p1.vy;
	p3.vz = (p2.vz - p0.vz) / 2.0f + p1.vz;

	p1.vx = (p0.vx - p2.vx) / 2.0f + p1.vx;
	p1.vy = (p0.vy - p2.vy) / 2.0f + p1.vy;
	p1.vz = (p0.vz - p2.vz) / 2.0f + p1.vz;

	if (FlipX)
	{
		p0.vx = -p0.vx;
		p1.vx = -p1.vx;
		p2.vx = -p2.vx;
		p3.vx = -p3.vx;
	}

	if (p0.vz < 50.0f || p0.vz > 45000.0f || p1.vz < 50.0f || p1.vz > 45000.0f || p2.vz < 50.0f || p2.vz > 45000.0f)
		return;

	ProjectVertex(&vA, &p0);
	ProjectVertex(&vB, &p1);
	ProjectVertex(&vC, &p2);
	ProjectVertex(&vD, &p3);

	SetVertexTexCoord(&vA, (float)tinfo->u, (float)tinfo->v);
	SetVertexTexCoord(&vB, (float)tinfo->w, (float)tinfo->v);
	SetVertexTexCoord(&vC, (float)tinfo->u, (float)tinfo->h);
	SetVertexTexCoord(&vD, (float)tinfo->w, (float)tinfo->h);

	SetVertexColour(&vA, (float)lf->rgb.r, (float)lf->rgb.g, (float)lf->rgb.b);
	SetVertexColour(&vB, (float)lf->rgb.r, (float)lf->rgb.g, (float)lf->rgb.b);
	SetVertexColour(&vC, (float)lf->rgb.r, (float)lf->rgb.g, (float)lf->rgb.b);
	SetVertexColour(&vD, (float)lf->rgb.r, (float)lf->rgb.g, (float)lf->rgb.b);

	guDrawTriangleWithClip(&vA, &vB, &vC);
	guDrawTriangleWithClip(&vD, &vB, &vC);
}

// [A]
// The four corners of a sprite of width w, height h standing on pos (camera space): A, B the
// top left, right; C, D the bottom right, left. Mirrored with FlipX.
static void SpriteCorners(VECTOR4 *pos, float w, float h, VECTOR4 *a, VECTOR4 *b, VECTOR4 *c, VECTOR4 *d)
{
	a->vx = pos->vx - w / 2.0f;
	b->vx = pos->vx + w / 2.0f;
	c->vx = b->vx;
	d->vx = a->vx;

	a->vy = pos->vy - h;
	b->vy = a->vy;
	c->vy = pos->vy;
	d->vy = pos->vy;

	a->vz = b->vz = c->vz = d->vz = pos->vz;

	if (FlipX)
	{
		a->vx = -a->vx;
		b->vx = -b->vx;
		c->vx = -c->vx;
		d->vx = -d->vx;
	}
}

// [0x4b288e] [D]
// A smoke puff (position2: its camera space position, see DrawSmokeEffects), additive; black
// and dust smoke subtractive. In fog it fades with the distance (a random puff further away
// is dropped).
void DisplaySmoke(SMOKE *sm)
{
	GrVertex vA, vB, vC, vD;
	VECTOR4 a, b, c, d;
	float near_fog;
	float far_fog;
	float fade;
	float col;
	int rnd;

	if (!EffectsInfo.DoSmoke || sm->position2.vz <= 300.0f || sm->position2.vz >= (float)ooz_far_clip || !smoke_texture)
		return;

	// NOTE: the width is start_w >> 1 each side (integer), the height start_w
	SpriteCorners(&sm->position2, (float)((sm->start_w >> 1) * 2), (float)sm->start_w, &a, &b, &c, &d);

	ProjectVertex(&vA, &a);
	ProjectVertex(&vB, &b);
	ProjectVertex(&vC, &c);
	ProjectVertex(&vD, &d);

	if (vB.x - vA.x > 320.0f || vA.x - vB.x > 320.0f)
		return;

	if (do_fogging)
	{
		rnd = (u_char)rand();

		near_fog = vA.oow * 4000.0f;
		far_fog = vA.oow * 8000.0f;

		if ((sm->flags & SMOKE_IN_FOG) == 0 && rnd > float_to_int((far_fog - near_fog) * 255.0f / (far_fog + 1.0f - 2.0f * near_fog)))
		{
			sm->transparency = 0;
			return;
		}

		sm->flags |= SMOKE_IN_FOG;

		if (near_fog <= 1.0f)
			fade = (far_fog - near_fog) / (far_fog + 1.0f - 2.0f * near_fog);
		else
			fade = 1.0f;

		col = (float)sm->transparency * fade;
	}
	else
	{
		col = (float)sm->transparency;
	}

	SetTexture(smoke_texture);

	if (sm->flags & SMOKE_EXHAUST)
	{
		SetVertexColour(&vA, col, col, col);
		SetVertexColour(&vB, col, col, col);
		SetVertexColour(&vC, col, col, col);
		SetVertexColour(&vD, col, col, col);
	}
	else if (sm->flags & (SMOKE_BLACK | SMOKE_SLOW_BLACK))
	{
		SetVertexColour(&vA, col, col, col);
		SetVertexColour(&vB, col, col, col);
		SetVertexColour(&vC, col, col, col);
		SetVertexColour(&vD, col, col, col);

		if (last_trans_on != 2)
		{
			grAlphaBlendFunction(GR_BLEND_ZERO, GR_BLEND_ONE_MINUS_SRC_COLOR, GR_BLEND_ONE, GR_BLEND_ZERO);
			last_trans_on = 2;
		}
	}
	else if (sm->flags & SMOKE_DUST)
	{
		// takes out more blue than red: brown
		SetVertexColour(&vA, col * 0.6f, col * 1.29f, col * 1.69f);
		SetVertexColour(&vB, col * 0.6f, col * 1.29f, col * 1.69f);
		SetVertexColour(&vC, col * 0.6f, col * 1.29f, col * 1.69f);
		SetVertexColour(&vD, col * 0.6f, col * 1.29f, col * 1.69f);

		if (last_trans_on != 2)
		{
			grAlphaBlendFunction(GR_BLEND_ZERO, GR_BLEND_ONE_MINUS_SRC_COLOR, GR_BLEND_ONE, GR_BLEND_ZERO);
			last_trans_on = 2;
		}
	}
	else
	{
		SetVertexColour(&vA, col, col, col + 8.0f);
		SetVertexColour(&vB, col, col, col + 8.0f);
		SetVertexColour(&vC, col, col, col + 8.0f);
		SetVertexColour(&vD, col, col, col + 8.0f);
	}

	SetVertexTexCoord(&vA, (float)smoke_texture->u, (float)smoke_texture->v);
	SetVertexTexCoord(&vB, (float)smoke_texture->w, (float)smoke_texture->v);
	SetVertexTexCoord(&vC, (float)smoke_texture->w, (float)smoke_texture->h);
	SetVertexTexCoord(&vD, (float)smoke_texture->u, (float)smoke_texture->h);

	guDrawTriangleWithClip(&vA, &vB, &vC);
	guDrawTriangleWithClip(&vC, &vD, &vA);

	if ((sm->flags & (SMOKE_BLACK | SMOKE_DUST | SMOKE_SLOW_BLACK)) && last_trans_on != 3)
	{
		grAlphaBlendFunction(GR_BLEND_ONE, GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_ZERO);
		last_trans_on = 3;
	}
}

// [0x4b3153] [D]
// A water splash (see AddWater), additive, a little blue.
void DisplayWater(SMOKE *sm)
{
	GrVertex vA, vB, vC, vD;
	VECTOR4 a, b, c, d;

	if (sm->position2.vz <= 300.0f || sm->position2.vz >= (float)ooz_far_clip || !smoke_texture)
		return;

	SpriteCorners(&sm->position2, (float)((sm->start_w >> 1) * 2), (float)sm->start_w, &a, &b, &c, &d);

	ProjectVertex(&vA, &a);
	ProjectVertex(&vB, &b);
	ProjectVertex(&vC, &c);
	ProjectVertex(&vD, &d);

	if (vB.x - vA.x > 320.0f || vA.x - vB.x > 320.0f)
		return;

	SetTexture(smoke_texture);

	SetVertexColour(&vA, (float)sm->transparency, (float)sm->transparency, (float)(sm->transparency + 50));
	SetVertexColour(&vB, (float)sm->transparency, (float)sm->transparency, (float)(sm->transparency + 50));
	SetVertexColour(&vC, (float)sm->transparency, (float)sm->transparency, (float)(sm->transparency + 50));
	SetVertexColour(&vD, (float)sm->transparency, (float)sm->transparency, (float)(sm->transparency + 50));

	// NOTE: B's s is scaled by D's oow in the original (all the corners have the same)
	SetVertexTexCoord(&vA, (float)smoke_texture->u, (float)smoke_texture->v);
	SetVertexTexCoord(&vB, (float)smoke_texture->w, (float)smoke_texture->v);
	SetVertexTexCoord(&vC, (float)smoke_texture->w, (float)smoke_texture->h);
	SetVertexTexCoord(&vD, (float)smoke_texture->u, (float)smoke_texture->h);

	guDrawTriangleWithClip(&vA, &vB, &vC);
	guDrawTriangleWithClip(&vC, &vD, &vA);
}

// [0x4b3660] [D]
// A flame, its colour from its life (flame_colours), additive.
void DisplayFlame(SMOKE *sm)
{
	GrVertex vA, vB, vC, vD;
	VECTOR4 a, b, c, d;
	u_char *col;

	if (!smoke_texture || sm->position2.vz <= 300.0f || sm->position2.vz >= (float)ooz_far_clip)
		return;

	SpriteCorners(&sm->position2, (float)((sm->start_w >> 1) * 2), (float)sm->start_w, &a, &b, &c, &d);

	ProjectVertex(&vA, &a);
	ProjectVertex(&vB, &b);
	ProjectVertex(&vC, &c);
	ProjectVertex(&vD, &d);

	col = flame_colours[(u_short)sm->life];

	SetVertexColour(&vA, (float)col[0], (float)col[1], (float)col[2]);
	SetVertexColour(&vB, (float)col[0], (float)col[1], (float)col[2]);
	SetVertexColour(&vC, (float)col[0], (float)col[1], (float)col[2]);
	SetVertexColour(&vD, (float)col[0], (float)col[1], (float)col[2]);

	if (vB.x - vA.x > 320.0f || vA.x - vB.x > 320.0f)
		return;

	SetTexture(smoke_texture);

	SetVertexTexCoord(&vA, (float)smoke_texture->u, (float)smoke_texture->v);
	SetVertexTexCoord(&vB, (float)smoke_texture->w, (float)smoke_texture->v);
	SetVertexTexCoord(&vC, (float)smoke_texture->w, (float)smoke_texture->h);
	SetVertexTexCoord(&vD, (float)smoke_texture->u, (float)smoke_texture->h);

	guDrawTriangleWithClip(&vA, &vB, &vC);
	guDrawTriangleWithClip(&vC, &vD, &vA);
}

// [0x4b3bb8] [D]
// A spark: a streak from its position to its tail. start_w is its type: 0 hot (red), 1 a lamp's
// (white), 2 grass (subtractive, green or red-green).
// NOTE: not mirrored with FlipX, but drawn whatever its distance then
void DisplaySpark(SMOKE *spark)
{
	GrVertex vA, vB, vC, vD;
	VECTOR4 TrailPos;
	VECTOR4 head;
	VECTOR4 side;
	float t;

	if (!spark_texture || (!FlipX && spark->position2.vz <= 300.0f) || spark->position2.vz >= (float)ooz_far_clip)
		return;

	TrailPos.vx = spark->final_tail_pos.vx - camera_position.vx;
	TrailPos.vy = spark->final_tail_pos.vy - camera_position.vy;
	TrailPos.vz = spark->final_tail_pos.vz - camera_position.vz;

	CurrentRotationMatrix = &inv_camera_matrix;
	MatrixRotate(&TrailPos);

	head = spark->position2;

	ProjectVertex(&vA, &head);

	side = head;
	side.vx = head.vx + 5.0f;
	ProjectVertex(&vB, &side);

	ProjectVertex(&vC, &TrailPos);

	side = TrailPos;
	side.vx = TrailPos.vx + 10.0f;
	ProjectVertex(&vD, &side);

	if (vB.x - vA.x > 320.0f || vA.x - vB.x > 320.0f)
		return;

	t = (float)spark->transparency;

	switch (spark->start_w)
	{
		case 0:
			SetVertexColour(&vA, (float)(spark->transparency + 30), t, t);
			SetVertexColour(&vB, (float)(spark->transparency + 30), t, t);
			SetVertexColour(&vC, (float)(spark->transparency + 30), t, t);
			SetVertexColour(&vD, (float)(spark->transparency + 30), t, t);
			break;
		case 1:
			SetVertexColour(&vA, t, t, t);
			SetVertexColour(&vB, t, t, t);
			SetVertexColour(&vC, t, t, t);
			SetVertexColour(&vD, t, t, t);
			break;
		case 2:
			if (last_trans_on != 2)
			{
				grAlphaBlendFunction(GR_BLEND_ZERO, GR_BLEND_ONE_MINUS_SRC_COLOR, GR_BLEND_ONE, GR_BLEND_ZERO);
				last_trans_on = 2;
			}

			if (spark->num & 1)
			{
				SetVertexColour(&vA, (float)(spark->transparency / 2), t, 255.0f);
				SetVertexColour(&vB, (float)(spark->transparency / 2), t, 255.0f);
				SetVertexColour(&vC, (float)(spark->transparency / 2), t, 255.0f);
				SetVertexColour(&vD, (float)(spark->transparency / 2), t, 255.0f);
			}
			else
			{
				SetVertexColour(&vA, t, (float)(spark->transparency / 2), 255.0f);
				SetVertexColour(&vB, t, (float)(spark->transparency / 2), 255.0f);
				SetVertexColour(&vC, t, (float)(spark->transparency / 2), 255.0f);
				SetVertexColour(&vD, t, (float)(spark->transparency / 2), 255.0f);
			}
			break;
	}

	SetTexture(spark_texture);

	SetVertexTexCoord(&vD, (float)spark_texture->u, (float)spark_texture->v);
	SetVertexTexCoord(&vA, (float)spark_texture->w, (float)spark_texture->v);
	SetVertexTexCoord(&vB, (float)spark_texture->w, (float)spark_texture->h);
	SetVertexTexCoord(&vC, (float)spark_texture->u, (float)spark_texture->h);

	guDrawTriangleWithClip(&vA, &vB, &vC);
	guDrawTriangleWithClip(&vA, &vC, &vD);

	if ((spark->start_w & 2) && last_trans_on != 3)
	{
		grAlphaBlendFunction(GR_BLEND_ONE, GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_ZERO);
		last_trans_on = 3;
	}
}

// [A]
// The alpha test of the cut out sprites (birds, leaves): on with 1, off with 0.
static void SetAlphaTest(int on)
{
	if (on)
	{
		if (gPreviousAlpha != 1)
		{
			grAlphaCombine(GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_ONE, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_TEXTURE, FXTRUE);
			grAlphaTestFunction(GR_CMP_GREATER);
			gPreviousAlpha = 1;
		}
	}
	else
	{
		if (gPreviousAlpha != 0)
		{
			grAlphaCombine(GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_ONE, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_CONSTANT, FXTRUE);
			grAlphaTestFunction(GR_CMP_ALWAYS);
			gPreviousAlpha = 0;
		}
	}
}

// [0x4b440b] [D]
// The birds of the flock, flapping (two textures).
// NOTE: a bird too close or too far ends it and stops the drawing of the rest this frame
void DisplayBirds(void)
{
	BIRD *bd;
	TEX_INFO *tex;
	GrVertex vA, vB, vC, vD;
	VECTOR4 pos;
	VECTOR4 a, b, c, d;
	int count;

	SetAlphaTest(1);

	bd = birds;
	count = gNumBirds;

	while (count--)
	{
		if (bd->life > 0)
		{
			pos.vx = bd->position.vx - camera_position.vx - 40.0f;
			pos.vy = bd->position.vy - camera_position.vy - 40.0f;
			pos.vz = bd->position.vz - camera_position.vz;

			MatrixRotate(&pos);

			if (pos.vz <= 224.0f || far_clip / 2.0f <= pos.vz)
			{
				bd->life = 0;

				SetAlphaTest(0);
				return;
			}

			SpriteCorners(&pos, 80.0f, 40.0f, &a, &b, &c, &d);

			ProjectVertex(&vA, &a);
			ProjectVertex(&vB, &b);
			ProjectVertex(&vC, &c);
			ProjectVertex(&vD, &d);

			SetVertexColour(&vA, 255.0f, 255.0f, 255.0f);
			SetVertexColour(&vB, 255.0f, 255.0f, 255.0f);
			SetVertexColour(&vC, 255.0f, 255.0f, 255.0f);
			SetVertexColour(&vD, 255.0f, 255.0f, 255.0f);

			if (bd->step <= 3)
				tex = bird2_texture;
			else
				tex = bird1_texture;

			SetTexture(tex);

			SetVertexTexCoord(&vA, (float)tex->u, (float)tex->v);
			SetVertexTexCoord(&vB, (float)tex->w, (float)tex->v);
			SetVertexTexCoord(&vC, (float)tex->w, (float)tex->h);
			SetVertexTexCoord(&vD, (float)tex->u, (float)tex->h);

			if (last_trans_on != 0)
			{
				grAlphaBlendFunction(GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_ONE, GR_BLEND_ZERO);
				last_trans_on = 0;
			}

			guDrawTriangleWithClip(&vA, &vB, &vC);
			guDrawTriangleWithClip(&vA, &vC, &vD);
		}

		bd++;
	}

	SetAlphaTest(0);
}

// [0x4b4b98] [D]
// A bit of debris: its rotated triangle (the smaller one for odd types). Ended (life 1) when
// too close or far.
void DrawDebris(DEBRIS *db)
{
	TRI_POINT *point_rot;
	GrVertex vA, vB, vC;
	VECTOR4 p0, p1, p2;
	int pos;

	pos = db->pos;

	MapHeightInt(&db->position);	// NOTE: the result is not used

	if (db->type & 1)
		point_rot = &debris_rot2[pos];
	else
		point_rot = &debris_rot1[pos];

	p0.vx = (float)db->position.vx + point_rot->v0.vx - camera_position.vx;
	p0.vy = (float)db->position.vy + point_rot->v0.vy - camera_position.vy;
	p0.vz = (float)db->position.vz + point_rot->v0.vz - camera_position.vz;

	MatrixRotate(&p0);

	if (p0.vz <= 224.0f || far_clip / 2.0f <= p0.vz)
	{
		db->life = 1;
		return;
	}

	p1.vx = (float)db->position.vx + point_rot->v1.vx - camera_position.vx;
	p1.vy = (float)db->position.vy + point_rot->v1.vy - camera_position.vy;
	p1.vz = (float)db->position.vz + point_rot->v1.vz - camera_position.vz;

	MatrixRotate(&p1);

	if (p1.vz <= 224.0f || far_clip / 2.0f <= p1.vz)
	{
		db->life = 1;
		return;
	}

	p2.vx = (float)db->position.vx + point_rot->v2.vx - camera_position.vx;
	p2.vy = (float)db->position.vy + point_rot->v2.vy - camera_position.vy;
	p2.vz = (float)db->position.vz + point_rot->v2.vz - camera_position.vz;

	MatrixRotate(&p2);

	if (p2.vz <= 224.0f || far_clip / 2.0f <= p2.vz)
	{
		db->life = 1;
		return;
	}

	if (FlipX)
	{
		p0.vx = -p0.vx;
		p1.vx = -p1.vx;
		p2.vx = -p2.vx;
	}

	ProjectVertex(&vA, &p0);
	ProjectVertex(&vB, &p1);
	ProjectVertex(&vC, &p2);

	SetVertexColour(&vA, (float)db->rgb.r, (float)db->rgb.g, (float)db->rgb.b);
	SetVertexColour(&vB, (float)db->rgb.r, (float)db->rgb.g, (float)db->rgb.b);
	SetVertexColour(&vC, (float)db->rgb.r, (float)db->rgb.g, (float)db->rgb.b);

	SetTexture(debris_texture);

	SetVertexTexCoord(&vA, (float)debris_texture->u, (float)debris_texture->v);
	SetVertexTexCoord(&vB, (float)debris_texture->w, (float)debris_texture->v);
	SetVertexTexCoord(&vC, (float)debris_texture->u, (float)debris_texture->h);

	if (last_trans_on != 0)
	{
		grAlphaBlendFunction(GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_ONE, GR_BLEND_ZERO);
		last_trans_on = 0;
	}

	guDrawTriangleWithClip(&vA, &vB, &vC);
}

// [0x4b559b] [D]
void DisplayDebris(void)
{
	DEBRIS *db;
	int count;

	if (last_trans_on != 0)
	{
		grAlphaBlendFunction(GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_ONE, GR_BLEND_ZERO);
		last_trans_on = 0;
	}

	if (!debris_texture)
		return;

	db = debris;
	count = MAX_DEBRIS;

	while (count--)
	{
		if (db->flags & DEBRIS_ACTIVE)
			DrawDebris(db);

		db++;
	}
}

// [0x4b5615] [D]
// The leaves (cut out), then their shadows (subtractive).
void DisplayLeaves(void)
{
	LEAF *lf;
	int count;

	if (last_state != TEXT3)
	{
		grColorCombine(GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_LOCAL, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_TEXTURE, FXFALSE);
		last_state = TEXT3;
	}

	if (last_trans_on != 0)
	{
		grAlphaBlendFunction(GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_ONE, GR_BLEND_ZERO);
		last_trans_on = 0;
	}

	SetAlphaTest(1);

	lf = leaf;
	count = MAX_LEAVES;

	while (count--)
	{
		if (lf->flags & LEAF_ACTIVE)
			DrawLeaf(lf, 0);

		lf++;
	}

	SetAlphaTest(0);

	if (gDrawShadows)
	{
		if (last_trans_on != 2)
		{
			grAlphaBlendFunction(GR_BLEND_ZERO, GR_BLEND_ONE_MINUS_SRC_COLOR, GR_BLEND_ONE, GR_BLEND_ZERO);
			last_trans_on = 2;
		}

		lf = leaf;
		count = MAX_LEAVES;

		while (count--)
		{
			if (lf->flags & LEAF_ACTIVE)
				DrawLeaf(lf, 1);

			lf++;
		}
	}
}

// [0x4b5771] [D]
// The pools of light of the cars listed this frame (gCarPools), dimmer at dawn.
void DrawCarPools(void)
{
	float col;
	int depth_mask;

	if (!pool_texture)
	{
		gCarPoolsToDraw = 0;
		return;
	}

	col = (float)((120 * gLampDawnScale) >> 8);

	if (do_fogging)
		grFogMode(GR_FOG_DISABLE);

	SwapPalette(0);

	depth_mask = EnableDepthMask(0);

	while (gCarPoolsToDraw--)
	{
		gPoolPoly.r = col;
		gPoolPoly.g = col;
		gPoolPoly.b = col;

		DrawCarPool(gCarPools[gCarPoolsToDraw]);
	}

	grDepthBiasLevel(0);
	gCarPoolsToDraw = 0;
	grDepthBufferMode(GR_DEPTHBUFFER_WBUFFER);

	EnableDepthMask(depth_mask);

	if (do_fogging)
		grFogMode(GR_FOG_WITH_TABLE);
}

// [0x4b5855] [D]
// The shadows of the pedestrians listed this frame (gPedNumToShadow), lighter at dawn.
void DrawAllPedShadows(void)
{
	float col;
	int depth_mask;

	if (!pedshadow_texture)
	{
		gPedShadowsToDraw = 0;
		return;
	}

	col = (float)((255 * (255 - gLampDawnScale)) >> 8);

	gPedPoly.r = col;
	gPedPoly.g = col;
	gPedPoly.b = col;

	if (do_fogging)
		grFogMode(GR_FOG_DISABLE);

	SwapPalette(0);

	depth_mask = EnableDepthMask(0);

	while (gPedShadowsToDraw--)
		DrawPedShadow(&pedestrians[gPedNumToShadow[gPedShadowsToDraw]].position.vx, 100);

	gPedShadowsToDraw = 0;
	grDepthBufferMode(GR_DEPTHBUFFER_WBUFFER);

	EnableDepthMask(depth_mask);

	if (do_fogging)
		grFogMode(GR_FOG_WITH_TABLE);
}

// [0x4b594d] [D]
// The sun and its lens flare (day, clear weather). Under Glide the flare is scaled by how much
// of an 8 x 8 square at the sun passes the depth test (the pixel count of the board).
void DrawLensFlare(void)
{
	GrVertex v1A, v1B, v1C;
	GrVertex v2A, v2B, v2C;
	GrLfbInfo_t info;
	u_short *line;
	u_short *pixel;
	float scr_w;
	float visible;
	float dist;
	float alpha;
	float col;
	float step_x, step_y;
	float d;
	int width;
	int height;
	int pixels;
	int i, j;

	if (FlipX || gNight || !gDoLensFlare || gDoDawn || gDoDusk || gDoRain)
		return;

	scr_w = screen_scale * (float)display_width;

	SwapPalette(0);

	if (last_state != TEXT3)
	{
		grColorCombine(GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_LOCAL, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_TEXTURE, FXFALSE);
		last_state = TEXT3;
	}

	if (last_trans_on != 3)
	{
		grAlphaBlendFunction(GR_BLEND_ONE, GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_ZERO);
		last_trans_on = 3;
	}

	CurrentRotationMatrix = &inv_camera_matrix;

	flare_position.vx = 0.0f;
	flare_position.vy = -10000.0f;
	flare_position.vz = 32000.0f;

	MatrixRotate(&flare_position);

	if (flare_position.vz < 50.0f)
		return;

	// := DIST * ooz_table[sun_position.vz];
	d = DIST * ooz_table[(int)flare_position.vz];

	flare_position.vx = flare_position.vx * d;
	flare_position.vy = flare_position.vy * d;

	width = float_to_int(screen_xmid - VERTEX_SNAPPER + flare_position.vx);
	height = float_to_int(screen_ymid - VERTEX_SNAPPER + flare_position.vy);

	sun.size = 24.0f;
	sun.width = (float)width;
	sun.height = (float)height;
	sun.alpha = 1.0f;

	DisplayFlare(&sun, 1.0f / (32000 * 2.0415f));	// 1.0f / (32000 * 2.0415f)
	grLfbConstantDepth(2);

	info.size = sizeof(GrLfbInfo_t);

	if (using_glide)
	{
		if (width <= 3 || (screen_xmid - VERTEX_SNAPPER) + (screen_xmid - VERTEX_SNAPPER) - 3.0f <= (float)width ||
			height <= 3 || (screen_ymid - VERTEX_SNAPPER) + (screen_ymid - VERTEX_SNAPPER) - 3.0f <= (float)height)
		{
			// at the edge of the screen: the last count
			pixels = sun_pixels;
		}
		else
		{
			grSstResetPerfStats();

			if (sun_test_by_quad)
			{
				if (last_state != FLAT3)
				{
					grColorCombine(GR_COMBINE_FUNCTION_LOCAL, GR_COMBINE_FACTOR_ZERO, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_CONSTANT, FXFALSE);
					last_state = FLAT3;
				}

				if (last_trans_on != 3)
				{
					grAlphaBlendFunction(GR_BLEND_ONE, GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_ZERO);
					last_trans_on = 3;
				}

				// set rgb individually...
				SetVertexColour(&v1C, 0.0f, 0.0f, 0.0f);
				SetVertexColour(&v1B, 0.0f, 0.0f, 0.0f);
				SetVertexColour(&v1A, 0.0f, 0.0f, 0.0f);

				v1A.x = (float)(width + 4);
				v1A.y = (float)(height - 4);
				v1B.x = (float)(width - 4);
				v1B.y = (float)(height + 4);
				v1C.x = v1B.x;
				v1C.y = v1A.y;

				v1C.oow = 1.0f / 65320.0f;
				v1B.oow = 1.0f / 65320.0f;
				v1A.oow = 1.0f / 65320.0f;

				guDrawTriangleWithClip(&v1A, &v1B, &v1C);

				v1C.x = (float)(width + 4);
				v1C.y = (float)(height + 4);

				guDrawTriangleWithClip(&v1A, &v1B, &v1C);

				if (last_state != TEXT3)
				{
					grColorCombine(GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_LOCAL, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_TEXTURE, FXFALSE);
					last_state = TEXT3;
				}

				last_state = -1;
				grSstIdle();
			}
			else
			{
				// writes the 8 x 8 square through the pixel pipeline
				gr_LfbLock(GR_LFB_WRITE_ONLY, GR_BUFFER_BACKBUFFER, GR_LFBWRITEMODE_565, GR_ORIGIN_UPPER_LEFT, FXTRUE, &info);

				pixel = (u_short *)info.lfbPtr + (info.strideInBytes >> 1) * (height - 4) + width - 4;

				for (i = 0; i < 8; i++)
				{
					line = pixel;

					for (j = 0; j < 8; j++)
						*pixel++ = 0xFF98;

					pixel = &line[info.strideInBytes >> 1];
				}

				gr_LfbUnlock(GR_LFB_WRITE_ONLY, GR_BUFFER_BACKBUFFER);
			}

			grSstPerfStats(&sun_perf_stats);

			pixels = sun_perf_stats.pixelsOut;
			sun_pixels = sun_perf_stats.pixelsOut;
		}

		visible = (float)pixels / 64.0f;
	}
	else
	{
		visible = 1.0f;
	}

	dist = (float)sqrt(flare_position.vy * flare_position.vy + flare_position.vx * flare_position.vx);

	// the closer the sun to the centre of the screen, the brighter the flare
	alpha = (scr_w - (dist < scr_w ? dist : scr_w)) / scr_w * visible;

	sunflare.size = 120.0f * gFlareScale;
	sunflare.width = (float)width;
	sunflare.height = (float)height;
	sunflare.alpha = alpha;
	sunflare.texture = sunflare_texture;

	DisplayFlareScaled(&sunflare, 1.0f / (32000 * 2.0415f), (int)((float)(width << 10) / (640.0f * screen_scale)));

	grDepthBufferMode(GR_DEPTHBUFFER_DISABLE);

	if (dist < scr_w)
	{
		// the flares on the line from the sun through the centre of the screen
		alpha = (scr_w - dist) / (2.0f * scr_w) * visible;

		step_x = flare_position.vx / 8.0f;
		step_y = flare_position.vy / 8.0f;

		for (i = 0; i < 8; i++)
		{
			flares[i].size = (float)(u_char)flare_sizes[i] * gFlareScale;

			d = (float)(2 * i - 8);

			flares[i].width = screen_xmid - VERTEX_SNAPPER + (step_x * d - step_x);
			flares[i].height = screen_ymid - VERTEX_SNAPPER + (step_y * d - step_y);
			flares[i].alpha = alpha;

			if (flares[i].alpha > 0.0f)
				DisplayFlare(&flares[i], 1.0f);
		}

		// close to the centre: the screen whitens
		if (scr_w / 2.0f > dist)
		{
			col = (140.0f - dist / (scr_w / 2.0f) * 140.0f) * visible;

			if (last_state != FLAT3)
			{
				grColorCombine(GR_COMBINE_FUNCTION_LOCAL, GR_COMBINE_FACTOR_ZERO, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_CONSTANT, FXFALSE);
				last_state = FLAT3;
			}

			if (last_trans_on != 3)
			{
				grAlphaBlendFunction(GR_BLEND_ONE, GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_ZERO);
				last_trans_on = 3;
			}

			// R = G = B =
			SetVertexColour(&v2C, col, col, col);
			SetVertexColour(&v2B, col, col, col);
			SetVertexColour(&v2A, col, col, col);

			v2A.x = clipxmax;
			v2A.y = clipymin;

			v2B.x = clipxmin;
			v2B.y = clipymax;

			v2C.x = clipxmin;
			v2C.y = clipymin;

			if (using_glide)
			{
				// A.oow = B.oow = C.oow = 1.0
				v2C.oow = 1.0f;
				v2B.oow = 1.0f;
				v2A.oow = 1.0f;
			}
			else
			{
				// A.oow = B.oow = C.oow = (1.0f / 64)
				v2C.oow = 1.0f / 64;
				v2B.oow = 1.0f / 64;
				v2A.oow = 1.0f / 64;
			}

			guDrawTriangleWithClip(&v2A, &v2B, &v2C);

			v2C.x = clipxmax;
			v2C.y = clipymax;

			guDrawTriangleWithClip(&v2A, &v2B, &v2C);
		}
	}

	grDepthBufferMode(GR_DEPTHBUFFER_WBUFFER);
	last_state = -1;
}

// [0x4b62e7] [D]
// A flare (screen position width, height) turned by angle (4096 to a turn).
void DisplayFlareScaled(FLARE *flare, float oow, int angle)
{
	float s;
	float c;
	float half;

	if (!flare->texture)
		return;

	half = flare->size / 2.0f * screen_scale * ((float)display_width / 640.0f);

	if (half + flare->width < 0.0f || flare->width - half >= (float)display_width ||
		half + flare->height < 0.0f || flare->height - half >= (float)display_height)
		return;

	s = fsintab[angle & 0xFFF];
	c = fsintab[(angle + 1024) & 0xFFF];

	// the corners (-half, half), (half, half), (half, -half), (-half, -half) turned
	flare_scaled_vA.x = (-half * c - half * s) + flare->width;
	flare_scaled_vA.y = (-half * s + half * c) + flare->height;
	flare_scaled_vB.x = (half * c - half * s) + flare->width;
	flare_scaled_vB.y = (half * s + half * c) + flare->height;
	flare_scaled_vC.x = (half * c + half * s) + flare->width;
	flare_scaled_vC.y = (half * s - half * c) + flare->height;
	flare_scaled_vD.x = (-half * c + half * s) + flare->width;
	flare_scaled_vD.y = (-half * s - half * c) + flare->height;

	flare_scaled_vD.oow = oow;
	flare_scaled_vC.oow = oow;
	flare_scaled_vB.oow = oow;
	flare_scaled_vA.oow = oow;

	if (FlipX)
	{
		flare_scaled_vA.x = -flare_scaled_vA.x;
		flare_scaled_vB.x = -flare_scaled_vB.x;
		flare_scaled_vC.x = -flare_scaled_vC.x;
		flare_scaled_vD.x = -flare_scaled_vD.x;
	}

	SetVertexColour(&flare_scaled_vA, (float)flare->color.r * flare->alpha, (float)flare->color.g * flare->alpha, (float)flare->color.b * flare->alpha);
	flare_scaled_vB.r = flare_scaled_vC.r = flare_scaled_vD.r = flare_scaled_vA.r;
	flare_scaled_vB.g = flare_scaled_vC.g = flare_scaled_vD.g = flare_scaled_vA.g;
	flare_scaled_vB.b = flare_scaled_vC.b = flare_scaled_vD.b = flare_scaled_vA.b;

	SetTexture(flare->texture);

	SetVertexTexCoord(&flare_scaled_vA, (float)(flare->texture->u + 1), (float)(flare->texture->v + 1));
	SetVertexTexCoord(&flare_scaled_vB, (float)flare->texture->w, (float)(flare->texture->v + 1));
	SetVertexTexCoord(&flare_scaled_vC, (float)flare->texture->w, (float)flare->texture->h);
	SetVertexTexCoord(&flare_scaled_vD, (float)(flare->texture->u + 1), (float)flare->texture->h);

	guDrawTriangleWithClip(&flare_scaled_vA, &flare_scaled_vB, &flare_scaled_vC);
	guDrawTriangleWithClip(&flare_scaled_vC, &flare_scaled_vD, &flare_scaled_vA);
}

// [0x4b6809] [D]
// A flare: a square of its size at its screen position (width, height).
void DisplayFlare(FLARE *flare, float oow)
{
	float half;

	if (!flare->texture || !gDoLensFlare)
		return;

	half = flare->size / 2.0f * screen_scale * ((float)display_width / 640.0f);

	if (half + flare->width < 0.0f || flare->width - half >= (float)display_width ||
		half + flare->height < 0.0f || flare->height - half >= (float)display_height)
		return;

	// A.x = D.x =
	flare_vD.x = flare->width - half;
	flare_vA.x = flare_vD.x;

	// B.x = C.x =
	flare_vC.x = half + flare->width;
	flare_vB.x = flare_vC.x;

	// A.y = B.y =
	flare_vB.y = flare->height - half;
	flare_vA.y = flare_vB.y;

	// C.y = D.y =
	flare_vD.y = half + flare->height;
	flare_vC.y = flare_vD.y;

	// A.oow = B.oow = C.oow = D.oow =
	flare_vD.oow = oow;
	flare_vC.oow = oow;
	flare_vB.oow = oow;
	flare_vA.oow = oow;

	if (FlipX)
	{
		flare_vA.x = -flare_vA.x;
		flare_vB.x = -flare_vB.x;
		flare_vC.x = -flare_vC.x;
		flare_vD.x = -flare_vD.x;
	}

	// A.r = B.r = C.r = D.r =
	flare_vD.r = (float)flare->color.r * flare->alpha;
	flare_vC.r = flare_vD.r;
	flare_vB.r = flare_vD.r;
	flare_vA.r = flare_vD.r;

	// A.g = B.g = C.g = D.g
	flare_vD.g = (float)flare->color.g * flare->alpha;
	flare_vC.g = flare_vD.g;
	flare_vB.g = flare_vD.g;
	flare_vA.g = flare_vD.g;

	// A.b = B.b = C.b = D.b =
	flare_vD.b = (float)flare->color.b * flare->alpha;
	flare_vC.b = flare_vD.b;
	flare_vB.b = flare_vD.b;
	flare_vA.b = flare_vD.b;

	SetTexture(flare->texture);

	SetVertexTexCoord(&flare_vA, (float)(flare->texture->u + 1), (float)(flare->texture->v + 1));
	SetVertexTexCoord(&flare_vB, (float)flare->texture->w, (float)(flare->texture->v + 1));
	SetVertexTexCoord(&flare_vC, (float)flare->texture->w, (float)flare->texture->h);
	SetVertexTexCoord(&flare_vD, (float)(flare->texture->u + 1), (float)flare->texture->h);

	guDrawTriangleWithClip(&flare_vA, &flare_vB, &flare_vC);
	guDrawTriangleWithClip(&flare_vC, &flare_vD, &flare_vA);
}

// [A]
// A square light of cop_texture, 40 across, centred on pos (camera space), drawn into v[4].
// NOTE: not a function in the original: the car light draw functions repeat it
static void DrawLightSquare(VECTOR4 *pos, GrVertex *v, float r, float g, float b)
{
	VECTOR4 a, b2, c, d;
	int i;

	a.vx = pos->vx - 20.0f;
	a.vy = pos->vy - 20.0f;
	b2.vx = pos->vx + 20.0f;
	b2.vy = a.vy;
	c.vx = b2.vx;
	c.vy = pos->vy + 20.0f;
	d.vx = a.vx;
	d.vy = c.vy;

	a.vz = b2.vz = c.vz = d.vz = pos->vz;

	if (FlipX)
	{
		a.vx = -a.vx;
		b2.vx = -b2.vx;
		c.vx = -c.vx;
		d.vx = -d.vx;
	}

	ProjectVertex(&v[0], &a);
	ProjectVertex(&v[1], &b2);
	ProjectVertex(&v[2], &c);
	ProjectVertex(&v[3], &d);

	for (i = 0; i < 4; i++)
		SetVertexColour(&v[i], r, g, b);

	SetTexture(cop_texture);

	SetVertexTexCoord(&v[0], (float)cop_texture->u, (float)cop_texture->v);
	SetVertexTexCoord(&v[1], (float)cop_texture->w, (float)cop_texture->v);
	SetVertexTexCoord(&v[2], (float)cop_texture->w, (float)cop_texture->h);
	SetVertexTexCoord(&v[3], (float)cop_texture->u, (float)cop_texture->h);

	grDepthBiasLevel(-90);

	guDrawTriangleWithClip(&v[0], &v[1], &v[2]);
	guDrawTriangleWithClip(&v[2], &v[3], &v[0]);

	grDepthBiasLevel(0);
}

// [0x4b6b9e] [D]
// A reverse light: a grey square.
void DrawReverseLight(SMOKE_LIGHT *lt)
{
	GrVertex v[4];

	if (lt->position.vz < 50.0f || lt->position.vz >= (float)ooz_far_clip || !cop_texture)
		return;

	DrawLightSquare(&lt->position, v, 100.0f, 100.0f, 100.0f);
}

// [A]
// The streak a moving light leaves (DrawLightFlare, DrawBrakeLight): its screen position this
// frame is the middle of vA and vC; seen last frame too it draws a quad of width len from
// there back to where it was two frames ago, r g b at the new end, a quarter at the old.
// NOTE: not a function in the original: both functions have this block
static void DrawLightStreak(LAMP_STREAK *streak, GrVertex *vA, GrVertex *vB, GrVertex *vC, GrVertex *vD, float oow, int len, float r, float g, float b)
{
	float centre_x, centre_y;
	float tail_x, tail_y;
	float dx, dy;
	int angle;
	int abs_x, abs_y;
	short old_x, old_y;
	short new_x, new_y;
	short side1, side2;
	int i;

	centre_x = vA->x;
	centre_y = vA->y;
	tail_x = vC->x;
	tail_y = vC->y;

	if (streak->frame != (u_short)DisplayedFrames - 1 || cameraview != lastcameraview || camera_has_changed)
	{
		// not seen last frame: starts again here
		for (i = 0; i < 4; i++)
		{
			streak->light_trails[i].x = (int)((centre_x + tail_x) * 0.5f);
			streak->light_trails[i].y = (int)((centre_y + tail_y) * 0.5f);
		}

		streak->frame = DisplayedFrames;
		return;
	}

	old_x = streak->light_trails[(DisplayedFrames - 2) & 3].x;
	old_y = streak->light_trails[(DisplayedFrames - 2) & 3].y;

	new_x = (short)(int)((centre_x + tail_x) * 0.5f);
	new_y = (short)(int)((centre_y + tail_y) * 0.5f);

	streak->frame = DisplayedFrames;
	streak->light_trails[DisplayedFrames & 3].x = new_x;
	streak->light_trails[DisplayedFrames & 3].y = new_y;

	abs_x = abs(old_x - new_x);
	abs_y = abs(old_y - new_y);

	if (abs_y + abs_x <= 4 || abs_y + abs_x >= 200 || len < 1)
		return;

	// the new end across the movement
	angle = Arc_Tan(old_y - new_y, old_x - new_x);

	dx = (float)len * fsintab[angle & 0xFFF];
	dy = (float)len * fsintab[((angle & 0xFFF) + 1024) & 0xFFF];

	side1 = (short)(int)(dy - dx * 0.5f);
	side2 = (short)(int)(dy * 0.5f + dx);

	vA->oow = oow;
	vA->x = (float)new_x + dx;
	vA->y = (float)new_y - dy;
	vB->oow = oow;
	vB->x = (float)(side2 + old_x);
	vB->y = (float)(old_y - side1);
	vC->oow = oow;
	vC->x = (float)(side1 + old_x);
	vC->y = (float)(side2 + old_y);
	vD->oow = oow;
	vD->x = (float)new_x - dx;
	vD->y = (float)new_y + dy;

	SetVertexColour(vA, r, g, b);
	SetVertexColour(vD, r, g, b);
	SetVertexColour(vB, r * 0.25f, g * 0.25f, b * 0.25f);
	SetVertexColour(vC, r * 0.25f, g * 0.25f, b * 0.25f);

	SetTexture(cop_texture);

	vB->tmuvtx[0].sow = (float)cop_texture->w * vA->oow;
	vA->tmuvtx[0].sow = vB->tmuvtx[0].sow;
	vD->tmuvtx[0].tow = ((float)(cop_texture->h - cop_texture->v) / 2.0f + (float)cop_texture->v) * vA->oow;
	vA->tmuvtx[0].tow = vD->tmuvtx[0].tow;
	vC->tmuvtx[0].tow = (float)cop_texture->h * vA->oow;
	vB->tmuvtx[0].tow = vC->tmuvtx[0].tow;
	vD->tmuvtx[0].sow = (float)cop_texture->u * vA->oow;
	vC->tmuvtx[0].sow = vD->tmuvtx[0].sow;

	grDepthBiasLevel((short)(int)(-1000.0f * oow));
	guDrawTriangleWithClip(vA, vB, vC);
	guDrawTriangleWithClip(vC, vD, vA);
	grDepthBiasLevel(0);
}

// [0x4b704e] [A]
// A light flare (light_colour[type & 0x7f]): a turned square of its size. A glowing one
// (type & 0x80) is brighter and bigger the more its light (dir, yang) points at the camera.
// In the night rain it also lights the road; at night a moving one (flags 0xe00000: a car's,
// or a traffic light's) leaves a streak from where it was last frame (carblur_info,
// Known_Lamps).
// NOTE: for a flare that does not glow the original turns it by an unset angle (stack
// garbage) and may scale its blue by an unset value: here 0 and the brightness
void DrawLightFlare(SMOKE_LIGHT *sm)
{
	LIGHT_INFO *lt;
	LAMP_STREAK *streak;
	GrVertex vA, vB, vC, vD;
	float oow;
	float scale;
	float size;
	float bright;
	float blue_bright;
	float s, c;
	float a, b;
	float d;
	float streak_w;
	int dir_angle;
	int yang_angle;
	int dir_diff;
	int yang_diff;
	int angle;
	int type;
	short num;
	short cycle;
	short bias;

	if (sm->position.vz < 50.0f || sm->position.vz >= (float)ooz_far_clip)
		return;

	// NB: should be optimized
	dir_angle = DIFF_ANGLES((u_short)sm->dir, (camera_angle.vx - (u_short)Arc_Tan(float_to_int(sm->position.vy), float_to_int(sm->position.vz))) & 0xFFF);
	dir_diff = abs(dir_angle);

	yang_angle = DIFF_ANGLES((u_short)sm->yang, ((u_short)Arc_Tan(float_to_int(sm->position.vx), float_to_int(sm->position.vz)) - camera_angle.vy) & 0xFFF);
	yang_diff = abs(yang_angle);

	type = sm->type & 0x7F;
	lt = &light_colour[type];

	angle = 0;
	blue_bright = 1.0f;

	if (sm->type & 0x80)
	{
		if (dir_diff >= 1024 || yang_diff >= 1174)
			return;

		if (sm->position.vz <= 10240.0f)
		{
			if (lt->flags & 1)
			{
				d = 0.8f * lt->field_14;

				size = fsintab[dir_diff + 1024] * fsintab[yang_diff + 1024] * fsintab[float_to_int(sm->position.vz * 0.1f) + 1024];
				scale = size * size * size;

				size = (lt->field_10 - lt->field_0c) * scale + lt->field_0c;
				blue_bright = (1.0f - d) * scale + d;
				bright = (1.0f - lt->field_14) * scale + lt->field_14;

				// fades out at the side
				if ((lt->flags & 4) && yang_diff > 1024)
				{
					scale = (float)(1174 - yang_diff) / 150.0f;
					scale = scale * scale * scale;

					bright = bright * scale;
					blue_bright = blue_bright * scale;
					size = size * scale;
				}
			}
			else
			{
				scale = (float)(yang_diff + dir_diff);

				if (scale > 1024.0f)
					scale = 1024.0f;

				size = (lt->field_10 - lt->field_0c) * scale * (1.0f / 1024.0f) + lt->field_0c;
				bright = (1.0f - lt->field_14) * scale * (1.0f / 1024.0f) + lt->field_14;
			}
		}
		else
		{
			if (!CamPerspectiveMode && (lt->flags & 8))
				bright = (15000.0f - sm->position.vz) * lt->field_14 * (1.0f / 4760.0f);
			else
				bright = (45000.0f - sm->position.vz) * lt->field_14 * (1.0f / 34760.0f);

			size = lt->field_0c;
			blue_bright = 0.8f * bright;
		}

		angle = (float_to_int(sm->position.vz) >> 4) + yang_angle + dir_angle;
	}
	else
	{
		size = lt->field_10;
		bright = 1.0f;
	}

	if (gDoRain && gNight)
	{
		if (type == 5)
			StoreLightReflection(&sm->position2, 9, (char)(int)(bright * 255.0f));
		else
			StoreLightReflection(&sm->position2, type, (char)(int)(bright * 255.0f));
	}

	oow = ooz_table[(int)sm->position.vz];
	scale = DIST * oow;
	streak_w = size * scale;

	// the square (-size, size) turned: its corners (a, b), (b, -a), (-a, -b), (-b, a)
	angle = (2 * (short)angle) & 0xFFF;		// BUG: may not be assigned

	s = fsintab[angle];
	c = fsintab[(angle + 1024) & 0xFFF];

	a = -size * c - size * s;
	b = -size * s + size * c;

	vA.x = a + sm->position.vx;
	vA.y = b + sm->position.vy;
	vB.x = b + sm->position.vx;
	vB.y = -a + sm->position.vy;
	vC.x = -a + sm->position.vx;
	vC.y = -b + sm->position.vy;
	vD.x = -b + sm->position.vx;
	vD.y = a + sm->position.vy;

	if (FlipX)
	{
		vA.x = -vA.x;
		vB.x = -vB.x;
		vC.x = -vC.x;
		vD.x = -vD.x;
	}

	vA.oow = oow;
	vA.x = vA.x * scale + screen_xmid - VERTEX_SNAPPER;
	vA.y = vA.y * scale + screen_ymid - VERTEX_SNAPPER;
	vB.oow = oow;
	vB.x = vB.x * scale + screen_xmid - VERTEX_SNAPPER;
	vB.y = vB.y * scale + screen_ymid - VERTEX_SNAPPER;
	vC.oow = oow;
	vC.x = vC.x * scale + screen_xmid - VERTEX_SNAPPER;
	vC.y = vC.y * scale + screen_ymid - VERTEX_SNAPPER;
	vD.oow = oow;
	vD.x = vD.x * scale + screen_xmid - VERTEX_SNAPPER;
	vD.y = vD.y * scale + screen_ymid - VERTEX_SNAPPER;

	vD.r = vC.r = vB.r = vA.r = bright * (float)lt->red;
	vD.g = vC.g = vB.g = vA.g = bright * (float)lt->green;

	if (lt->flags & 2)
		bright = blue_bright;

	vD.b = vC.b = vB.b = vA.b = bright * (float)lt->blue;

	if (copflare_texture && (type == 9 || type == 10 || type == 11 || type == 12))
	{
		SetTexture(copflare_texture);

		bias = -2500;

		SetVertexTexCoord(&vA, (float)copflare_texture->u, (float)copflare_texture->v);
		SetVertexTexCoord(&vB, (float)copflare_texture->w, (float)copflare_texture->v);
		SetVertexTexCoord(&vC, (float)copflare_texture->w, (float)copflare_texture->h);
		SetVertexTexCoord(&vD, (float)copflare_texture->u, (float)copflare_texture->h);
	}
	else
	{
		SetTexture(lightflare_texture);

		bias = -200;

		SetVertexTexCoord(&vA, (float)lightflare_texture->u, (float)lightflare_texture->v);
		SetVertexTexCoord(&vB, (float)lightflare_texture->w, (float)lightflare_texture->v);
		SetVertexTexCoord(&vC, (float)lightflare_texture->w, (float)lightflare_texture->h);
		SetVertexTexCoord(&vD, (float)lightflare_texture->u, (float)lightflare_texture->h);
	}

	grDepthBiasLevel(bias);
	guDrawTriangleWithClip(&vA, &vB, &vC);
	guDrawTriangleWithClip(&vC, &vD, &vA);
	grDepthBiasLevel(0);

	if (!gNight || (sm->flags & 0xE00000) == 0 || FlipX)
		return;

	num = sm->num;
	cycle = sm->cycle;

	if (cycle == -1)
		return;

	if (sm->flags & 0x800000)
		streak = &carblur_info[num][cycle];
	else
		streak = &Known_Lamps[cycle];

	DrawLightStreak(streak, &vA, &vB, &vC, &vD, oow, (int)(streak_w * 0.35f), vA.r, vA.g, vA.b);
}

// [0x4b85ab] [D]
// The haze of a lamp (see AddHazeLight): a square of half size yang * 4, its colour
// light_colour[type] by its transparency.
void DrawLightHaze(SMOKE_LIGHT *sm)
{
	GrVertex vA, vB, vC, vD;
	VECTOR4 a, b, c, d;
	float size;
	int transparency;
	int type;
	int yang;

	if (sm->position.vz < 50.0f || sm->position.vz >= (float)ooz_far_clip)
		return;

	type = (u_short)sm->type;
	yang = (u_short)sm->yang;
	transparency = (u_short)sm->transparency;

	size = (float)(yang * 4);

	a.vx = sm->position.vx - size;
	a.vy = sm->position.vy - size;
	b.vx = sm->position.vx + size;
	b.vy = a.vy;
	c.vx = b.vx;
	c.vy = sm->position.vy + size;
	d.vx = a.vx;
	d.vy = c.vy;

	a.vz = b.vz = c.vz = d.vz = sm->position.vz;

	if (FlipX)
	{
		a.vx = -a.vx;
		b.vx = -b.vx;
		c.vx = -c.vx;
		d.vx = -d.vx;
	}

	ProjectVertex(&vA, &a);
	ProjectVertex(&vB, &b);
	ProjectVertex(&vC, &c);
	ProjectVertex(&vD, &d);

	if (vB.x - vA.x > 320.0f || vA.x - vB.x > 320.0f)
		return;

	vD.r = vC.r = vB.r = vA.r = (float)(transparency * light_colour[type].red / 256);
	vD.g = vC.g = vB.g = vA.g = (float)(transparency * light_colour[type].green / 256);
	vD.b = vC.b = vB.b = vA.b = (float)(transparency * light_colour[type].blue / 256);

	SetTexture(haze_texture);

	SetVertexTexCoord(&vA, (float)haze_texture->u + 0.5f, (float)haze_texture->v + 0.5f);
	SetVertexTexCoord(&vB, (float)haze_texture->w, (float)haze_texture->v + 0.5f);
	SetVertexTexCoord(&vC, (float)haze_texture->w, (float)haze_texture->h);
	SetVertexTexCoord(&vD, (float)haze_texture->u + 0.5f, (float)haze_texture->h);

	grDepthBiasLevel(-250);

	guDrawTriangleWithClip(&vA, &vB, &vC);
	guDrawTriangleWithClip(&vC, &vD, &vA);

	grDepthBiasLevel(0);
}

// [0x4b8bb3] [D]
// A brake (tail) light: red, brighter the more it faces the camera; at night it leaves a
// streak when moving (carblur_info).
void DrawBrakeLight(SMOKE_LIGHT *lt)
{
	GrVertex v[4];
	float bright;
	int dir_diff;
	int yang_diff;
	int len;

	if (lt->position.vz < 50.0f || lt->position.vz >= (float)ooz_far_clip || !cop_texture)
		return;

	dir_diff = abs(DIFF_ANGLES((u_short)lt->dir, (camera_angle.vx - (u_short)Arc_Tan(float_to_int(lt->position.vy), float_to_int(lt->position.vz))) & 0xFFF));
	yang_diff = abs(DIFF_ANGLES((u_short)lt->yang, ((u_short)Arc_Tan(float_to_int(lt->position.vx), float_to_int(lt->position.vz)) - camera_angle.vy) & 0xFFF));

	if (dir_diff >= 1024 || yang_diff >= 1024)
		return;

	// fcos(...) * fcos(...)
	bright = fsintab[dir_diff + 1024] * fsintab[yang_diff + 1024];

	if (gDoRain && gNight)
		StoreLightReflection(&lt->position2, 5, (char)(int)(bright * 255.0f));

	DrawLightSquare(&lt->position, v, bright * 255.0f, bright * 10.0f, bright * 10.0f);

	if (!gNight || FlipX)
		return;

	len = abs((int)(v[0].x - v[2].x) >> 1);

	DrawLightStreak(&carblur_info[lt->num][lt->cycle], &v[0], &v[1], &v[2], &v[3], v[0].oow, len, bright * 160.0f, bright * 6.0f, bright * 6.0f);
}

// [0x4b9a72] [D]
// A front indicator light: orange-red by its blink (type), dimmer the less it faces the camera.
void DrawFrontIndicatorLight(SMOKE_LIGHT *lt)
{
	GrVertex v[4];
	float bright;
	int dir_angle;
	int yang_angle;
	int dir_diff;
	int yang_diff;

	if (lt->position.vz < 50.0f || lt->position.vz >= (float)ooz_far_clip || !cop_texture)
		return;

	dir_angle = DIFF_ANGLES((u_short)lt->dir, camera_angle.vx);
	dir_diff = abs(dir_angle);
	yang_angle = DIFF_ANGLES((u_short)lt->yang, -camera_angle.vy & 0xFFF);
	yang_diff = abs(yang_angle);

	if (dir_diff >= 800 || yang_diff >= 800)
		return;

	bright = (float)(1024 - (yang_diff + dir_diff)) / 1024.0f;

	DrawLightSquare(&lt->position, v, bright * (float)(u_short)lt->type, 0.0f, 0.0f);
}

// [0x4ba031] [D]
// A back indicator light: red by its blink (type).
void DrawBackIndicatorLight(SMOKE_LIGHT *lt)
{
	GrVertex v[4];

	if (!EffectsInfo.DoBackIndicatorLights || lt->position.vz < 50.0f || lt->position.vz >= (float)ooz_far_clip || !cop_texture)
		return;

	DrawLightSquare(&lt->position, v, (float)(u_short)lt->type, 0.0f, 0.0f);
}

// [0x4ba509] [D]
// num_debris bits of debris thrown from ipos (y up) at ispeed. type 5 and up: grey bits at
// ispeed; below: thrown up and spread out, in the player car's colour (model 0) or
// blue-grey.
void Setup_Debris(IVECTOR *ipos, IVECTOR *ispeed, int num_debris, int type, int model)
{
	DEBRIS *mydebris;
	short rnd;
	int speed_x;
	int speed_up;
	int speed_z;
	int num;
	int i;

	if (GamePaused || !debris_texture)
		return;

	for (i = 0; i < num_debris; i++)
	{
		num = AllocateDebris();

		if (num < 0)
			break;

		mydebris = &debris[num];

		mydebris->position.vx = ipos->vx;
		mydebris->position.vy = -ipos->vy;
		mydebris->position.vz = ipos->vz;
		mydebris->frame = 0;

		rnd = rand();

		if (type >= 5)
		{
			speed_up = ispeed->vy;
			speed_x = ispeed->vx;
			speed_z = ispeed->vz;

			mydebris->step = 2;
			mydebris->rgb.b = 60;
			mydebris->rgb.g = 60;
			mydebris->rgb.r = 60;
		}
		else
		{
			speed_up = 2 * num_debris * (rnd & 0x7F00);
			speed_x = ((num_debris * ((rnd & 0x7F) - 64)) << 10) + ispeed->vx;
			speed_z = ispeed->vz + 2 * num_debris * ((rand() & 0x7FFF) - 0x4000);

			mydebris->step = (u_char)rnd;

			if (model)
			{
				mydebris->rgb.r = (rnd & 15) + 111;
				mydebris->rgb.g = 115;
				mydebris->rgb.b = 145;
			}
			else
			{
				mydebris->rgb.r = gPlayerCarDebrisColour[gPlayerCarModelType].r;
				mydebris->rgb.g = gPlayerCarDebrisColour[gPlayerCarModelType].g;
				mydebris->rgb.b = gPlayerCarDebrisColour[gPlayerCarModelType].b;
			}
		}

		mydebris->type = type & 7;
		mydebris->direction.vx = speed_x >> 12;
		mydebris->direction.vy = -(speed_up >> 12);
		mydebris->direction.vz = speed_z >> 12;
		mydebris->life = 100;
		mydebris->flags = DEBRIS_ACTIVE;
		mydebris->pos = (u_char)(i + game_frame) & 31;
	}
}

// [0x4ba70e] [D]
// A smoke puff at pos (every other frame). SmokeType 1: black (thinner the faster the
// wheels), 2: white (exhaust: thin and small), else: dust.
void Setup_Smoke(VECTOR4 *pos, int start_w, int end_w, int SmokeType, int WheelSpeed, VECTOR4 *Drift, int Exhaust)
{
	SMOKE *mysmoke;
	float scale;
	int num;

	if (GamePaused || !gDoSmoke || (game_frame & 1))
		return;

	num = AllocateSmoke();

	if (num < 0)
		return;

	mysmoke = &smoke[num];

	if (SmokeType == 1)
	{
		mysmoke->position.vx = (float)(rand() & 127) + pos->vx;
		mysmoke->position.vy = pos->vy;
		mysmoke->position.vz = (float)(rand() & 127) + pos->vz;
		mysmoke->start_w = start_w;
		mysmoke->final_w = end_w;
		mysmoke->step = 25;
		mysmoke->flags = SMOKE_BLACK | SMOKE_SMOKE | SMOKE_ACTIVE;
		mysmoke->life = 40;
		mysmoke->halflife = 20;

		if (WheelSpeed <= 200000)
			mysmoke->transparency = 255;
		else
			mysmoke->transparency = (800000 - WheelSpeed) >> 11;

		mysmoke->t_step = 10;
	}
	else if (SmokeType == 2)
	{
		if (Exhaust)
		{
			mysmoke->position.vx = (float)(rand() & 0x15) + pos->vx;
			mysmoke->position.vy = pos->vy;
			mysmoke->position.vz = (float)(rand() & 0x15) + pos->vz;
			mysmoke->flags = SMOKE_EXHAUST | SMOKE_SMOKE | SMOKE_ACTIVE;
			mysmoke->step = 2;
			mysmoke->transparency = 40;
			mysmoke->t_step = 1;
		}
		else
		{
			mysmoke->position.vx = (float)(rand() & 63) + pos->vx;
			mysmoke->position.vy = pos->vy;
			mysmoke->position.vz = (float)(rand() & 63) + pos->vz;
			mysmoke->flags = SMOKE_SMOKE | SMOKE_ACTIVE;
			mysmoke->step = 17;
			mysmoke->transparency = 80;
			mysmoke->t_step = 4;
		}

		mysmoke->start_w = start_w;
		mysmoke->final_w = end_w;
		mysmoke->life = 100;
		mysmoke->halflife = 50;
	}
	else
	{
		mysmoke->position.vx = (float)(rand() & 63) + pos->vx;
		mysmoke->position.vy = pos->vy;
		mysmoke->position.vz = (float)(rand() & 63) + pos->vz;
		mysmoke->flags = SMOKE_DUST | SMOKE_SMOKE | SMOKE_ACTIVE;
		mysmoke->transparency = 150;
		mysmoke->t_step = 10;
		mysmoke->start_w = start_w;
		mysmoke->final_w = end_w;
		mysmoke->step = 25;
		mysmoke->life = 100;
		mysmoke->halflife = 50;
	}

	// the drift dies away over the first half of its life
	mysmoke->drift.vx = Drift->vx;
	mysmoke->drift.vy = Drift->vy;
	mysmoke->drift.vz = Drift->vz;

	scale = 1.0f / (float)((u_short)mysmoke->life >> 1);

	mysmoke->drift_change.vx = scale * Drift->vx;
	mysmoke->drift_change.vy = scale * Drift->vy;
	mysmoke->drift_change.vz = scale * Drift->vz;
}

// [0x4baa47] [D]
// A slow smoke puff of a smoking object (odd frames), growing from start_w to end_w.
// type 1: black.
// NOTE: transparency is the width step and step the transparency (the names swapped)
void Setup_SlowSmoke(VECTOR4 *pos, VECTOR4 *drift, int start_w, int end_w, int transparency, int step, int type, float drift_change, int exhaust)
{
	SMOKE *mysmoke;
	int num;

	if (GamePaused || !gDoSmoke || (game_frame & 1) == 0)
		return;

	num = AllocateSmoke();

	if (num < 0)
		return;

	mysmoke = &smoke[num];

	mysmoke->position.vx = (float)((rand() & 15) - 7) + pos->vx;
	mysmoke->position.vy = pos->vy;
	mysmoke->position.vz = (float)((rand() & 15) - 7) + pos->vz;

	if (exhaust)
	{
		mysmoke->drift.vx = (float)((rand() & 3) - 1) + drift->vx;
		mysmoke->drift.vy = drift->vy;
		mysmoke->drift.vz = (float)((rand() & 3) - 1) + drift->vz;
	}
	else
	{
		mysmoke->drift.vx = drift->vx;
		mysmoke->drift.vy = drift->vy;
		mysmoke->drift.vz = drift->vz;
	}

	mysmoke->drift_change.vy = drift_change;
	mysmoke->start_w = start_w;
	mysmoke->final_w = end_w;

	if (end_w >= start_w)
		mysmoke->step = transparency;
	else
		mysmoke->step = -transparency;

	mysmoke->transparency = step;
	mysmoke->life = abs((end_w - start_w) / transparency);
	mysmoke->halflife = (u_short)mysmoke->life >> 1;

	if (type == 1)
		mysmoke->flags = SMOKE_SLOW_BLACK | SMOKE_SMOKE | SMOKE_ACTIVE;
	else
		mysmoke->flags = SMOKE_SLOW | SMOKE_SMOKE | SMOKE_ACTIVE;

	mysmoke->t_step = 16 * step / (u_short)mysmoke->life;
}

// [0x4bac24] [D]
// A flame at pos (every other frame) living life frames.
void Setup_Flame(VECTOR4 *pos, int start_w, int end_w, short life)
{
	SMOKE *myflame;
	int num;

	if (GamePaused || (game_frame & 1))
		return;

	num = AllocateSmoke();

	if (num < 0)
		return;

	myflame = &smoke[num];

	myflame->flags = SMOKE_FLAME | SMOKE_ACTIVE;
	myflame->position.vx = (float)(rand() & 3) + pos->vx;
	myflame->position.vy = pos->vy;
	myflame->position.vz = (float)(rand() & 3) + pos->vz;
	myflame->start_w = start_w;
	myflame->final_w = end_w;
	myflame->life = life;
	myflame->drift.vz = 0.0f;
	myflame->drift.vy = 0.0f;
	myflame->drift.vx = 0.0f;
	myflame->step = 2;
	myflame->transparency = 80;
	myflame->t_step = 2;
}

// [0x4bad1c] [D]
// num_sparks sparks from ipos at ispeed (spread out). Type (start_w): 0 hot, short lived;
// 1 a lamp's (sparks again on the ground); 2 grass.
// drift and final_tail_pos keep the last two positions (the tail).
void Setup_Sparks(IVECTOR *ipos, IVECTOR *ispeed, int num_sparks, int Type)
{
	SMOKE *myspark;
	int speed_x;
	int speed_y;
	int speed_z;
	int num;
	int i;

	if (GamePaused || !spark_texture)
		return;

	for (i = 0; i < num_sparks; i++)
	{
		num = AllocateSmoke();

		if (num < 0)
			break;

		myspark = &smoke[num];

		myspark->position.vx = (float)ipos->vx;
		myspark->position.vy = (float)ipos->vy;
		myspark->position.vz = (float)ipos->vz;

		if (Type == 2)
		{
			myspark->position.vx = (float)(rand() & 63) + myspark->position.vx;
			myspark->position.vy = (float)(rand() & 31) + myspark->position.vy;
			myspark->position.vz = (float)(rand() & 63) + myspark->position.vz;
		}

		myspark->drift = myspark->position;
		myspark->final_tail_pos = myspark->position;

		myspark->flags = SMOKE_SPARK | SMOKE_ACTIVE;
		myspark->pos = (u_char)(4 * i);
		myspark->step = rand() & 63;
		myspark->start_w = Type;

		if (Type)
		{
			speed_x = 5 * ((rand() & 0x7FFF) - 0x4000) + ispeed->vx;
			speed_y = 5 * (rand() & 0x7FFF) + ispeed->vy;
			speed_z = (5 * ((rand() & 0x7FFF) - 0x4000) + ispeed->vz) >> 12;

			myspark->drift_change.vx = (float)(speed_x >> 12);
			myspark->drift_change.vy = (float)-(speed_y >> 12);
			myspark->drift_change.vz = (float)speed_z;
			myspark->life = 40;

			if (Type == 2)
				myspark->transparency = 60;
			else
				myspark->transparency = 120;

			myspark->t_step = 5;
		}
		else
		{
			speed_x = 5 * ((rand() & 0x7FFF) - 0x4000) + ispeed->vx;
			speed_y = ispeed->vy + 4 * (rand() & 0x7FFF);
			speed_z = (5 * ((rand() & 0x7FFF) - 0x4000) + ispeed->vz) >> 12;

			myspark->drift_change.vx = (float)(speed_x >> 12);
			myspark->drift_change.vy = (float)-(speed_y >> 12);
			myspark->drift_change.vz = (float)speed_z;
			myspark->life = 10;
			myspark->transparency = 30;
			myspark->t_step = 2;
		}
	}
}

// [0x4baff5] [D]
// The wind: a slowly turning drift (step: moves it on).
void GetSmokeDrift(VECTOR4 *wind, int step)
{
	static int index1 = 10;
	static int index2;
	static int index3 = 5;
	float s;
	float c;

	if (step)
	{
		index1 += 3;
		index2 += 5;
		index3 += 7;
		index1 &= 0xFFF;
		index2 &= 0xFFF;
		index3 &= 0xFFF;
	}

	s = fsintab[index1] + fsintab[index2];
	c = fsintab[(index1 + 1024) & 0xFFF] + fsintab[(index2 + 1024) & 0xFFF];

	wind_magnitude = fsintab[index3] * 10.0f;

	wind->vx = wind_magnitude * c;
	wind->vy = 0.0f;
	wind->vz = -wind_magnitude * s;
}

// [0x4bb0f3] [D]
void HandleDebris(void)
{
	DEBRIS *db;
	int loop;

	if (next_debris == 0)
		return;

	db = debris;
	loop = MAX_DEBRIS;

	while (loop--)
	{
		if (db->flags & DEBRIS_ACTIVE)
			StepDebris(db);

		db++;
	}
}

// [0x4bb14b] [D]
// Moves a bit of debris (falling), bouncing it on the ground; it ends after 3 bounces.
void StepDebris(DEBRIS *db)
{
	float ground;

	if (--db->life == 0)
	{
		db->flags = 0;
		ReleaseDebris(db->num);
		return;
	}

	db->position.vx += db->direction.vx;
	db->position.vy += db->direction.vy;
	db->position.vz += db->direction.vz;

	db->direction.vy += 6;

	// FUCKED: bad casts!
	db->pos += (short)db->step;
	db->pos = (u_char)db->pos;

	ground = (float)-MapHeightInt(&db->position);

	if ((float)db->position.vy > ground)
	{
		db->direction.vy = -float_to_int((float)db->direction.vy * 0.7f);
		db->position.vy = float_to_int(ground);

		if (++db->frame > 2)
			db->life = 1;
	}
}

// [0x4bb2ac] [D]
// Blows the leaves about (13500 around the player; further away they end).
void HandleLeaves(void)
{
	LEAF *lf;
	VECTOR4 wind;
	float sx;
	float sz;
	int ground;
	int dx;
	int dz;
	int dist;
	int count;

	count = MAX_LEAVES;

	for (lf = leaf; count--; lf++)
	{
		if ((lf->flags & LEAF_ACTIVE) == 0)
			continue;

		dx = abs(float_to_int(lf->position.vx) - car_data[0].location.vx);
		dz = abs(float_to_int(lf->position.vz) - car_data[0].location.vz);

		if (dx >= dz)
			dist = dz / 2 + dx;
		else
			dist = dx / 2 + dz;

		if (dist > 13500)
		{
			lf->flags = 0;
			ReleaseLeaf(lf->num);
			continue;
		}

		lf->sin_index1 += lf->sin_addition1;
		lf->sin_index2 += lf->sin_addition2;
		lf->sin_index1 &= 0xFFF;
		lf->sin_index2 &= 0xFFF;

		sx = fsintab[(short)lf->sin_index1] + fsintab[(short)lf->sin_index2];
		sz = fsintab[(lf->sin_index1 + 1024) & 0xFFF] + fsintab[(lf->sin_index2 + 1024) & 0xFFF];

		lf->direction.vx = sx * 10.0f;
		lf->direction.vz = sz * 10.0f;

		GetSmokeDrift(&wind, 0);

		ground = -MapHeight(&lf->position) - 10;

		if (lf->position.vy < (float)ground)
		{
			lf->position.vx = wind.vx + lf->direction.vx + lf->position.vx;
			lf->position.vy = wind.vy + lf->direction.vy + lf->position.vy;
			lf->position.vz = wind.vz + lf->direction.vz + lf->position.vz;

			lf->pos += (short)lf->step;
			lf->pos = (u_char)lf->pos;
		}
		else
		{
			lf->position.vy = (float)ground;
		}

		lf->direction.vy = sx + sx + 3.0f;
	}
}

// [0x4bb539] [D]
void HandleBirds(void)
{
	BIRD *bd;
	int loop;

	bd = birds;
	loop = gNumBirds;

	while (loop--)
	{
		if (bd->life > 0)
		{
			bd->position.vx = bd->position.vx + bd->direction.vx;
			bd->position.vy = bd->position.vy + bd->direction.vy;
			bd->position.vz = bd->position.vz + bd->direction.vz;

			if (bd->step-- == 0)
				bd->step = 10;

			bd->life--;
		}

		bd++;
	}
}

// [0x4bb5eb] [D]
void HandleWater(SMOKE *sm)
{
	sm->position.vy = sm->position.vy - 1.0f;
	sm->start_w += sm->step;
}

// [0x4bb614] [D]
// A flame rises, growing then (the last 4 frames) shrinking.
void HandleFlame(SMOKE *sm)
{
	sm->position.vy = sm->position.vy - 3.0f;

	if ((u_short)sm->life <= 4)
		sm->start_w -= sm->step;
	else
		sm->start_w += sm->step;
}

// [0x4bb65f] [D]
// Moves a spark (falling). On the ground a hot one bounces, a lamp's makes 5 hot sparks and
// ends, a grass one vanishes.
void HandleSpark(SMOKE *sm)
{
	IVECTOR pos;
	IVECTOR speed;
	float ground;

	sm->final_tail_pos = sm->drift;
	sm->drift = sm->position;

	sm->position.vx = sm->position.vx + sm->drift_change.vx;
	sm->position.vy = sm->position.vy + sm->drift_change.vy;
	sm->position.vz = sm->position.vz + sm->drift_change.vz;

	sm->drift_change.vy = sm->drift_change.vy + 6.0f;

	ground = (float)-MapHeight(&sm->position);

	if (sm->position.vy > ground)
	{
		if (sm->start_w == 1)
		{
			pos.vx = float_to_int(sm->position.vx);
			pos.vy = float_to_int(ground);
			pos.vz = float_to_int(sm->position.vz);

			speed.vz = 0;
			speed.vx = 0;
			speed.vy = -5;

			Setup_Sparks(&pos, &speed, 5, 0);

			// NOTE: released here and, if its life runs out below, again
			sm->flags = 0;
			ReleaseSmoke(sm->num);
		}
		else if (sm->start_w)
		{
			sm->transparency = 0;
		}
		else
		{
			sm->position.vy = ground;
			sm->drift_change.vy = -(sm->drift_change.vy * 0.5f);
		}
	}

	sm->transparency -= sm->t_step;

	if (sm->transparency <= 0)
	{
		sm->transparency = 0;
		sm->life = 1;
	}

	if (--sm->life == 0)
	{
		sm->flags = 0;
		ReleaseSmoke(sm->num);
	}
}

// [0x4bb853] [D]
// Moves a smoke puff with the wind and its drift, growing and fading.
void HandleSmoke(SMOKE *sm)
{
	VECTOR4 wind;
	int flags;

	GetSmokeDrift(&wind, 1);

	flags = sm->flags;

	if ((flags & SMOKE_ACTIVE) == 0)
		return;

	if (flags & (SMOKE_SLOW_BLACK | SMOKE_SLOW))
	{
		sm->position.vy = sm->drift_change.vy + sm->drift.vy + sm->position.vy;

		if ((u_short)sm->life <= (u_short)sm->halflife)
		{
			sm->position.vx = sm->drift.vx / 2.0f + sm->position.vx;
			sm->position.vz = sm->drift.vz / 2.0f + sm->position.vz;
		}
		else
		{
			sm->position.vx = sm->position.vx + sm->drift.vx;
			sm->position.vz = sm->position.vz + sm->drift.vz;
		}

		sm->start_w += sm->step;
		sm->drift_change.vy = sm->drift_change.vy * 0.99f;

		// t_step is in 16ths
		sm->transparency = (16 * sm->transparency - sm->t_step) >> 4;
	}
	else
	{
		// black smoke sinks, the rest rises
		if (flags & SMOKE_BLACK)
			sm->position.vy = sm->position.vy + 2.0f;
		else
			sm->position.vy = sm->position.vy - 0.8f;

		sm->position.vx = sm->position.vx + sm->drift.vx;
		sm->position.vy = sm->position.vy + sm->drift.vy;
		sm->position.vz = sm->position.vz + sm->drift.vz;

		if ((u_short)sm->life > (u_short)sm->halflife)
		{
			sm->drift.vx = sm->drift.vx - sm->drift_change.vx;
			sm->drift.vy = sm->drift.vy - sm->drift_change.vy;
			sm->drift.vz = sm->drift.vz - sm->drift_change.vz;
		}

		sm->start_w += sm->step;
		sm->transparency -= sm->t_step;
	}

	if (sm->transparency <= 0)
	{
		sm->transparency = 0;
		sm->life = 1;
	}

	if (--sm->life == 0)
	{
		sm->flags = 0;
		ReleaseSmoke(sm->num);
	}
}

// [0x4bbadb] [D]
void HandleSmokeEffects(void)
{
	SMOKE *sm;
	int flags;
	int num;

	sm = smoke;
	num = MAX_SMOKE;

	while (num--)
	{
		flags = sm->flags;

		if (flags & SMOKE_ACTIVE)
		{
			if (flags & SMOKE_SMOKE)
				HandleSmoke(sm);
			else if (flags & SMOKE_SPARK)
				HandleSpark(sm);
			else if (flags & SMOKE_WATER)
				HandleWater(sm);
			else if (flags & SMOKE_FLAME)
				HandleFlame(sm);

			if (flags & (SMOKE_WATER | SMOKE_FLAME))
			{
				sm->transparency -= sm->t_step;

				if (sm->transparency <= 0)
				{
					sm->transparency = 0;
					sm->life = 1;
				}

				if (--sm->life == 0)
				{
					sm->flags = 0;
					ReleaseSmoke(sm->num);
				}
			}
		}

		sm++;
	}
}

// [0x4bbc03] [D]
// Sorts the smoke and the lights of this frame by depth (smoke_table).
void DisplaySmokeEffects(void)
{
	DrawSmokeEffects();
	DrawLightHazes();
	DrawLightFlares();
}

// [A]
// Adds sm to smoke_table after the effects further away: the depth of the smoke is in its
// position2 (smoke), of a light in its position (lights).
// NOTE: not a function in the original: DrawSmokeEffects, DrawLightHazes, DrawLightFlares
// repeat it. The first effect added stays the head, whatever its depth; a light is compared
// with the position of the smoke in the table (its world position)
static void AddToSmokeTable(SMOKE *sm, int lights)
{
	SMOKE *next_smoke;

	sm->next = NULL;
	sm->prev = NULL;

	if (smoke_table)
	{
		if (lights)
		{
			for (next_smoke = smoke_table; next_smoke->next && sm->position.vz < next_smoke->position.vz; next_smoke = next_smoke->next)
				;
		}
		else
		{
			for (next_smoke = smoke_table; next_smoke->next && sm->position2.vz < next_smoke->position2.vz; next_smoke = next_smoke->next)
				;
		}

		sm->next = next_smoke->next;
		sm->prev = next_smoke;

		next_smoke->next = sm;
	}
	else
	{
		smoke_table = sm;
	}
}

// [0x4bbc17] [D]
// The smoke puffs in front of the camera into smoke_table (position2: in camera space).
void DrawSmokeEffects(void)
{
	SMOKE *sm;
	int num;

	sm = smoke;
	num = MAX_SMOKE;

	CurrentRotationMatrix = &inv_camera_matrix;

	while (num--)
	{
		if (sm->flags & SMOKE_ACTIVE)
		{
			sm->position2.vx = sm->position.vx - camera_position.vx;
			sm->position2.vy = sm->position.vy - camera_position.vy;
			sm->position2.vz = sm->position.vz - camera_position.vz;

			MatrixRotate(&sm->position2);

			sm->next = NULL;
			sm->prev = NULL;

			if (sm->position2.vz >= 50.0f)
				AddToSmokeTable(sm, 0);
		}

		sm++;
	}
}

// [0x4bbd3a] [D]
// The car lights (brake_lights_array) into smoke_table.
void DrawLightHazes(void)
{
	SMOKE *sm;
	int num;

	if (num_brake_lights == 0)
		return;

	sm = (SMOKE *)brake_lights_array;
	num = num_brake_lights;

	while (num--)
	{
		if (sm->position.vz >= 50.0f)
			AddToSmokeTable(sm, 1);

		sm++;
	}
}

// [0x4bbe0d] [D]
// The light flares and hazes (light_flares_array) into smoke_table.
void DrawLightFlares(void)
{
	SMOKE *flare;
	int num;

	if (num_light_flares == 0)
		return;

	flare = (SMOKE *)light_flares_array;
	num = num_light_flares;

	while (num--)
	{
		if (flare->position.vz >= 50.0f)
			AddToSmokeTable(flare, 1);

		flare++;
	}
}

// [0x4bbee0] [D]
// The searchlights in the sky of level 2.
void DrawSpotLights(void)
{
	int depth_mask;
	int i;

	if (GameLevel != 2)
		return;

	if (last_state != TEXT3)
	{
		grColorCombine(GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_LOCAL, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_TEXTURE, FXFALSE);
		last_state = TEXT3;
	}

	SetTexture(spotlight_texture);

	if (last_trans_on != 4)
	{
		grAlphaBlendFunction(GR_BLEND_ONE, GR_BLEND_ONE_MINUS_SRC_COLOR, GR_BLEND_ONE, GR_BLEND_ZERO);
		last_trans_on = 4;
	}

	depth_mask = EnableDepthMask(0);

	for (i = 0; i < 6; i++)
		DrawSpotLight(&spotl[i]);

	EnableDepthMask(depth_mask);
}

// [0x4bbfdf] [D]
// A searchlight beam: a fan from its source (on the horizon in direction field_0, up
// field_4) swinging about (field_8, field_10, field_14 over field_C frames) to its end, fading
// with the distance of the camera from (-43000, 170000).
void DrawSpotLight(SPOT_LIGHT *light)
{
	GrVertex vA, vB, vC, vD, vE;
	VECTOR4 dir;
	float sway;
	float scale;
	float src_w;
	float end_w;
	float d;
	int angle;
	int swing;
	int dx, dz;
	int dist;
	int bright;

	frame_count = game_frame;

	angle = DIFF_ANGLES(camera_angle.vy, (u_short)light->field_0);

	if (angle > 1024 || angle < -1024)
		return;

	scale = screen_scale * (float)display_width / 640.0f;

	dir.vx = -fsintab[light->field_0 & 0xFFF];
	dir.vz = fsintab[(light->field_0 & 0xFFF) + 1024];
	dir.vy = fsintab[light->field_4 & 0xFFF];

	CurrentRotationMatrix = &inv_camera_matrix;
	MatrixRotate(&dir);

	sway = fsintab[(((light->field_20 + frame_count) << 12) / light->field_C) & 0xFFF];

	src_w = light->field_18 / 1000.0f;
	end_w = (sway * light->field_24 + light->field_1C) / 1000.0f;

	swing = ((u_short)(light->field_14 * isin(((light->field_10 + frame_count) << 12) / light->field_C) / 4096) + (u_short)light->field_8) & 0xFFF;

	d = DIST * scale / dir.vz;

	vA.x = dir.vx;
	vA.y = dir.vy;
	vA.oow = 0.000001f;

	vB.x = dir.vx - fsintab[swing + 1024] * src_w;
	vB.y = dir.vy - fsintab[swing] * src_w;
	vB.oow = 0.000001f;

	vE.x = fsintab[swing] * light->field_28 + dir.vx;
	vE.y = dir.vy - (sway * 0.01f * light->field_24 + 1.0f) * light->field_28;
	vE.oow = 0.000001f;

	dx = abs(float_to_int(camera_position.vx) + 43000);
	dz = abs(float_to_int(camera_position.vz) - 170000);

	if (dx >= dz)
		dist = dz / 2 + dx;
	else
		dist = dx / 2 + dz;

	if (dist <= 100000)
	{
		bright = light->field_2C;
	}
	else
	{
		bright = light->field_2C - (dist - 100000) / 2000;
		vE.y = (float)(dist - 100000) / 400000.0f + vE.y;

		if (bright <= 0)
			return;
	}

	vC.x = vE.x - fsintab[swing + 1024] * end_w;
	vC.y = vE.y - fsintab[swing] * end_w;
	vC.oow = 0.000001f;

	vD.x = 2.0f * vE.x - vC.x;
	vD.y = 2.0f * vE.y - vC.y;
	vD.oow = 0.000001f;

	vE.x = 2.0f * vA.x - vB.x;
	vE.y = 2.0f * vA.y - vB.y;

	if (FlipX)
	{
		vA.x = -vA.x;
		vB.x = -vB.x;
		vC.x = -vC.x;
		vD.x = -vD.x;
		vE.x = -vE.x;
	}

	SetVertexColour(&vA, (float)bright, (float)bright, (float)bright);
	SetVertexColour(&vB, (float)bright, (float)bright, (float)bright);
	SetVertexColour(&vC, (float)bright, (float)bright, (float)bright);
	SetVertexColour(&vD, (float)bright, (float)bright, (float)bright);
	SetVertexColour(&vE, (float)bright, (float)bright, (float)bright);

	vA.x = vA.x * d + screen_xmid - VERTEX_SNAPPER;
	vA.y = vA.y * d + screen_ymid - VERTEX_SNAPPER;
	vB.x = vB.x * d + screen_xmid - VERTEX_SNAPPER;
	vB.y = vB.y * d + screen_ymid - VERTEX_SNAPPER;
	vC.x = vC.x * d + screen_xmid - VERTEX_SNAPPER;
	vC.y = vC.y * d + screen_ymid - VERTEX_SNAPPER;
	vD.x = vD.x * d + screen_xmid - VERTEX_SNAPPER;
	vD.y = vD.y * d + screen_ymid - VERTEX_SNAPPER;
	vE.x = vE.x * d + screen_xmid - VERTEX_SNAPPER;
	vE.y = vE.y * d + screen_ymid - VERTEX_SNAPPER;

	vD.tmuvtx[0].sow = (float)spotlight_texture->w * 0.000001f;
	vC.tmuvtx[0].sow = vD.tmuvtx[0].sow;
	vB.tmuvtx[0].tow = (float)spotlight_texture->v * 0.000001f;
	vC.tmuvtx[0].tow = vB.tmuvtx[0].tow;
	vE.tmuvtx[0].tow = (float)spotlight_texture->h * 0.000001f;
	vD.tmuvtx[0].tow = vE.tmuvtx[0].tow;
	vB.tmuvtx[0].sow = (float)spotlight_texture->u * 0.000001f;
	vE.tmuvtx[0].sow = vB.tmuvtx[0].sow;
	vA.tmuvtx[0].sow = (vB.tmuvtx[0].sow + vB.tmuvtx[0].sow) * 0.5f;
	vA.tmuvtx[0].tow = (vB.tmuvtx[0].tow + vE.tmuvtx[0].tow) * 0.5f;

	guDrawTriangleWithClip(&vA, &vB, &vC);
	guDrawTriangleWithClip(&vA, &vC, &vD);
	guDrawTriangleWithClip(&vA, &vD, &vE);
}

// [0x4bc820] [D]
// Gives the new lamps found last frame (NewLamp) a free streak record, then clears the
// marks for this frame.
void PreLampStreak(void)
{
	int i;

	for (i = 0; i < 30 && StreakCount1; i++)
	{
		if (Known_Lamps[i].set == 0)
		{
			Known_Lamps[i].clock = NewLamp[--StreakCount1];
			Known_Lamps[i].frame = DisplayedFrames - 1;
		}
	}

	for (i = 0; i < 30; i++)
	{
		NewLamp[i] = 0;
		Known_Lamps[i].set = 0;
	}

	StreakCount1 = 0;
}

// [0x4bc8df] [D]
// The streak record of the lamp clock (an id), or -1 and remembered for the next frame.
int find_lamp_streak(int clock)
{
	int i;

	for (i = 0; i < 30; i++)
	{
		if (Known_Lamps[i].clock == clock)
		{
			Known_Lamps[i].set = 1;
			return i;
		}
	}

	if (StreakCount1 < 30)
		NewLamp[StreakCount1++] = clock;

	return -1;
}
