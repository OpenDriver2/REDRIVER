/*
 * general.c - memory debugging, frame sync, misc helpers
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "general.c", 0x4d90b0 - 0x4dbae7).
 *
 * The debug text is collected in a list while the frame is built and drawn to the frame buffer at
 * the end of it, in 8x8 cells.
 */

#include <windows.h>
#include "dr_keys.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dr_types.h"
#include "dr_globals.h"
#include "input.h"
#include "shell.h"
#include "dr_time.h"
#include "ai.h"
#include "asmmath.h"
#include "general.h"
#include "map.h"
#include "D3DGlide/glide_ptrs.h"
#include "Engine/3dfx/gfxinit.h"
#include "Engine/3dfx/tset.h"
#include "Director.h"
#include "camera.h"
#include "debug.h"
#include "denting.h"
#include "handling.h"
#include "mirror.h"
#include "pathfind.h"
#include "pause.h"
#include "replays.h"
#include "Engine/3dfx/polydraw.h"
#include "glaunch.h"

void CheckCameraKeys(void);
void GetScaledXYCoords(float *x, float *y);
void TextDraw(char *text);
void TextReset(void);
int CopyVisibleScreen(void *buffer);

typedef struct MEM_RECORD
{
	unsigned long mem_addr;
	unsigned long size;
	char *filename;
} MEM_RECORD;

#define MAX_MEM_PACKETS		4096	// NOTE: known constant

#define FRAME_TIME			20	// ms per game frame (50 fps)
#define MAX_FRAMES_PER_SYNC	10

#define LOADING_BAR_STEP	0x20000	// bytes read between loading bar updates

#define MAX_TEXT_LIST_ENTRIES	128

typedef struct TEXT_LIST_ENTRY
{
	char *text;
	int color;
	int x;
	int y;
} TEXT_LIST_ENTRY;

void UpdateLoadingBar(void);

// the headers of the BMP files; width and height are patched in
static u_char bmphdr[54] = {
	0x42, 0x4d, 0x62, 0xcb, 0x0e, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x36, 0x04, 0x00, 0x00, 0x28, 0x00,
	0x00, 0x00, 0xe8, 0x03, 0x00, 0x00, 0xe8, 0x03,
	0x00, 0x00, 0x01, 0x00, 0x08, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x2c, 0xcb, 0x0e, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static u_char bmppal[64] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00,
	0x00, 0x00, 0xff, 0x00, 0xff, 0x00, 0x00, 0x00,
	0xff, 0xff, 0xff, 0x00, 0x00, 0xff, 0x00, 0x00,
	0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00,
	0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00,
	0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00,
};

static u_char bmp24hdr[54] = {
	0x42, 0x4d, 0x36, 0x60, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x36, 0x00, 0x00, 0x00, 0x28, 0x00,
	0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x40, 0x00,
	0x00, 0x00, 0x01, 0x00, 0x18, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x60, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static char *heading_names[4] = { "NORTH", "EAST", "SOUTH", "WEST" };

int grabnum = -1;

// NOTE: bss globals in the linker's common area, owner by layout unknown; defined here next to
// their only users
MEM_RECORD mem_info[MAX_MEM_PACKETS];
long mem_allocated, mem_released, current_mem_usage, peak_mem_usage;
int LastFrameCount;
int gTotalBytesRead;
int gNextUpdate;
int gDoLoadingBar;
int gTotalBytesToRead;
int gLoadingBarX;
int gLoadingBarY;
int gLoadingBarWidth;
int gLoadingBarHeight;
int fc;
int gc;
short TextColourVal;
int NextSyncFrame;
int MaxSyncFrames;
int NumSyncFrames;
int TextOffsetX;
int TextOffsetY;
char TextListBuffer[4096];
TEXT_LIST_ENTRY TextDrawList[MAX_TEXT_LIST_ENTRIES];
int gTicksTillKeysActive;
int accumg;
int accumf;
char current_time[16];
int CurrTextListEntry;
int NextTextListEntry;
MODEL *gHubcapModelPtr;
MODEL *gCleanWheelModelPtr;
MODEL *gFastWheelModelPtr;
MODEL *gDamWheelModelPtr;

// NOTE: moved here from the generated stubs; no IDA mark, the owner is a guess from the users
// and the neighbouring data
int ifps;

// [0x4d90b0] [D]
// The in game keys of every frame: viewport size (- =), screenshot (F12), pause (Escape), the
// camera keys, far clip distances and, with gAllowCheatKeys, the debug keys (A reset the car,
// G ghost mode, F repair, O occupancy grid).
void CheckMiscKeys(void)
{
	int i;

	if (KeyMap[DIK_MINUS])
		ReduceViewport();

	if (KeyMap[DIK_EQUALS])
		ExpandViewport();

	if (CheckKey(DIK_F12))
		SaveScreenShot();

	if (GamePaused)
		return;

	if (CheckKey(DIK_ESCAPE) && Director.Mode == 0)
		PauseGame(1);

	CheckCameraKeys();

	if (KeyMap[KeySet.LessFarClip] && far_clip > 15000.0f)
	{
		far_clip = far_clip - 3000.0f;
		CalculateNormalViewArc();
		gDetailLevel = 4;
	}

	if (KeyMap[KeySet.MoreFarClip] && far_clip < 45000.0f)
	{
		far_clip = far_clip + 3000.0f;
		CalculateNormalViewArc();
		gDetailLevel = 4;
	}

	if (KeyMap[KeySet.LessMirrorFarClip] && mirror_far_clip > 15000.0f)
	{
		mirror_far_clip = mirror_far_clip - 3000.0f;
		CalculateMirrorViewArc(mirror_far_clip);
		gDetailLevel = 4;
	}

	if (KeyMap[KeySet.MoreMirrorFarClip] && mirror_far_clip < 42000.0f)
	{
		mirror_far_clip = mirror_far_clip + 3000.0f;
		CalculateMirrorViewArc(mirror_far_clip);
		gDetailLevel = 4;
	}

	if (!gAllowCheatKeys)
		return;

	if (CheckKey(DIK_A))
		ResetPlayerCar();

	if (CheckKey(DIK_G))
		ghost_mode ^= 1;

	if (KeyMap[DIK_F])
	{
		car_data[0].totalDamage = 0;

		for (i = 0; i < 6; i++)
		{
			if (car_data[0].damage[i] <= 100)
				car_data[0].damage[i] = 0;
			else
				car_data[0].damage[i] -= 100;

			car_data[0].totalDamage += car_data[0].damage[i];
		}

		QueueDentCar(-1);
		FixHubcaps();
	}

	if (!PrevKeyMap[DIK_O] && KeyMap[DIK_O])
		OverlayOccupancyGrid ^= 1;
}

// [0x4d9336] [D] [A]
// The camera keys (not while the director has the camera): view toggle (in car 0 -> 2 -> 8
// -> 0), looking left, right and behind, dropping a static camera (17; again: perspective mode).
// NOTE: [A] left and right together look behind, as in Driver 2. As there, looking to a side waits
// two calls, so that the two keys needn't go down (or up) on the same frame.
void CheckCameraKeys(void)
{
	static int look_timer;

	int changed;

	changed = 0;

	if (Director.Mode != 0)
		return;

	if (CheckKey(KeySet.ViewToggle))
	{
		if (cameraview == 0)
			cameraview = 2;
		else if (cameraview == 2)
			cameraview = 8;
		else
			cameraview = 0;

		changed = 1;
	}

	if (KeyMap[KeySet.LookLeft] && !KeyMap[KeySet.LookRight])
	{
		if (look_timer > 1)
			gHeadTurnTarget = 0x4000000;
		else
			look_timer++;
	}
	else if (KeyMap[KeySet.LookRight] && !KeyMap[KeySet.LookLeft])
	{
		if (look_timer > 1)
			gHeadTurnTarget = -0x4000000;
		else
			look_timer++;
	}
	else if ((KeyMap[KeySet.LookLeft] && KeyMap[KeySet.LookRight]) || KeyMap[KeySet.LookBehind])
	{
		look_timer = 0;

		if (gHeadTurnTarget > 0)
			gHeadTurnTarget = 0x8000000;
		else
			gHeadTurnTarget = -0x8000000;
	}
	else
	{
		look_timer = 0;
		gHeadTurnTarget = 0;
	}

	if (changed)
	{
		if (cameraview)
		{
			CameraInCar = 0;
		}
		else
		{
			CameraPos.vy = -256.0f;
			gCameraDistance = 1280.0f;
			gCameraAngle = 2048;
		}
	}

	if (CheckKey(KeySet.DropCamera))
	{
		holding_camera_position = camera_position;

		if (cameraview == 17)
		{
			CamPerspectiveMode ^= 1;
		}
		else
		{
			cameraview = 17;
			CamPerspectiveMode = 0;
		}
	}
}

// [0x4d94c3] [D]
// Waits for the next 20 ms frame tick; returns how many frames passed (at most 10).
int WaitForSync(int delta)
{
	DWORD frames;

	frames = 0;

	HandleWinMessages();

	if (gDoAutoInterpolation)
	{
		while (frames == 0)
			frames = (GetTicksMs() - LastFrameCount) / FRAME_TIME;

		LastFrameCount += FRAME_TIME * frames;

		if (frames > MAX_FRAMES_PER_SYNC)
			frames = MAX_FRAMES_PER_SYNC;

		FrameCount += frames;
		return frames;
	}
	else if (gDoInterpolation)
	{
		while (frames < (DWORD)max_frame_skip)
			frames = (GetTicksMs() - LastFrameCount) / FRAME_TIME;

		LastFrameCount += FRAME_TIME * frames;

		if (frames > MAX_FRAMES_PER_SYNC)
			frames = MAX_FRAMES_PER_SYNC;

		FrameCount += frames;
		return frames;
	}

	FrameCount++;
	return 1;
}

// [0x4d95f1] [D]
// NOTE: unnamed in IDA (sub_4D95F1); name guessed. It has a thunk, so it is not static.
void DisplaySyncRate(void)
{
	float num_frames;
	float max_frames;
	float refresh_rate;
	char buffer[80];

	if (FrameCount == 0)
		return;

	if ((u_int)FrameCount % 15 == 0 && (u_int)FrameCount > (u_int)accumf)
	{
		MaxSyncFrames = FrameCount - accumf;
		NumSyncFrames = game_frame - accumg;
		accumf = FrameCount;
		accumg = game_frame;
		num_frames = (float)(u_int)NumSyncFrames;
		max_frames = (float)(u_int)MaxSyncFrames;
		refresh_rate = (float)(u_int)gRefreshRate;
		ifps = float_to_int(num_frames / max_frames * refresh_rate);
	}

	sprintf(buffer, "%d Hz, %d FPS.", gRefreshRate, ifps);

	DBGPrintString(buffer);
	DBGCR();
}

// [0x4d9706] [D]
// NOTE: unnamed in IDA (sub_4D9706); name guessed. It has a thunk, so it is not static.
void NextPlayerCar(void)
{
	if (++PlayerCar == num_cars)
		PlayerCar = 0;
}

// [0x4d9730] [D]
// The car wheel and hubcap models every level has.
void InitModelNames(void)
{
	gHubcapModelPtr = FindModelPtrWithName("HUBCAP");

	if (!gHubcapModelPtr)
		ShowMessageBox("Could not find hubcap model.\nThis is NOT a code bug.  Change name in LEV file.", "ERROR IN _LEVEL_ DATA", MESSAGE_INFO);

	gCleanWheelModelPtr = FindModelPtrWithName("CLEANWHEEL");

	if (!gCleanWheelModelPtr)
		ShowMessageBox("Could not find clean wheel model.\nThis is NOT a code bug.  Change name in LEV file.", "ERROR IN _LEVEL_ DATA", MESSAGE_INFO);

	gFastWheelModelPtr = FindModelPtrWithName("FASTWHEEL");

	if (!gFastWheelModelPtr)
		ShowMessageBox("Could not find fast wheel model.\nThis is NOT a code bug.  Change name in LEV file.", "ERROR IN _LEVEL_ DATA", MESSAGE_INFO);

	gDamWheelModelPtr = FindModelPtrWithName("DAMWHEEL");

	if (!gDamWheelModelPtr)
		ShowMessageBox("Could not find damaged wheel model.\nThis is NOT a code bug.  Change name in LEV file.", "ERROR IN _LEVEL_ DATA", MESSAGE_INFO);
}

// [0x4d9803] [D]
// A flat quad in screen pixels; color is 0xRRGGBB.
void DrawQuad(float x, float y, float w, float h, int color)
{
	GrVertex v0, v1, v2;

	if (last_state)
	{
		grColorCombine(GR_COMBINE_FUNCTION_LOCAL, GR_COMBINE_FACTOR_ZERO, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_CONSTANT, FXFALSE);
		last_state = FLAT3;
	}

	if (last_trans_on)
	{
		grAlphaBlendFunction(GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_ONE, GR_BLEND_ZERO);
		last_trans_on = 0;
	}

	v0.r = (float)((color & 0xFF0000) >> 16);
	v0.g = (float)((color & 0xFF00) >> 8);
	v0.b = (float)(color & 0xFF);
	v1.r = v0.r;
	v1.g = v0.g;
	v1.b = v0.b;
	v2.r = v0.r;
	v2.g = v0.g;
	v2.b = v0.b;

	v0.x = x;
	v0.y = y;
	v1.x = x + w;
	v1.y = y + h;
	v2.x = x;
	v2.y = v1.y;

	if (using_glide)
	{
		v0.oow = 1.0f;
		v1.oow = 1.0f;
		v2.oow = 1.0f;
	}
	else
	{
		v0.oow = 0.015625f;
		v1.oow = 0.015625f;
		v2.oow = 0.015625f;
	}

	guDrawTriangleWithClip(&v0, &v1, &v2);

	v0.x = x;
	v0.y = y;
	v1.x = x + w;
	v1.y = y;
	v2.x = v1.x;
	v2.y = y + h;

	guDrawTriangleWithClip(&v0, &v1, &v2);

	last_state = -1;
}

// [0x4d99a8] [D]
// DrawQuad added to the screen (transparent).
void DrawQuadTC(float x, float y, float w, float h, int color)
{
	GrVertex v0, v1, v2;

	if (last_state)
	{
		grColorCombine(GR_COMBINE_FUNCTION_LOCAL, GR_COMBINE_FACTOR_ZERO, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_CONSTANT, FXFALSE);
		last_state = FLAT3;
	}

	if (last_trans_on != 3)
	{
		grAlphaBlendFunction(GR_BLEND_ONE, GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_ZERO);
		last_trans_on = 3;
	}

	v0.r = (float)((color & 0xFF0000) >> 16);
	v0.g = (float)((color & 0xFF00) >> 8);
	v0.b = (float)(color & 0xFF);
	v1.r = v0.r;
	v1.g = v0.g;
	v1.b = v0.b;
	v2.r = v0.r;
	v2.g = v0.g;
	v2.b = v0.b;

	v0.x = x;
	v0.y = y;
	v1.x = x + w;
	v1.y = y + h;
	v2.x = x;
	v2.y = v1.y;

	if (using_glide)
	{
		v0.oow = 1.0f;
		v1.oow = 1.0f;
		v2.oow = 1.0f;
	}
	else
	{
		v0.oow = 0.015625f;
		v1.oow = 0.015625f;
		v2.oow = 0.015625f;
	}

	guDrawTriangleWithClip(&v0, &v1, &v2);

	v0.x = x;
	v0.y = y;
	v1.x = x + w;
	v1.y = y;
	v2.x = v1.x;
	v2.y = y + h;

	guDrawTriangleWithClip(&v0, &v1, &v2);

	last_state = -1;
}

// [0x4d9b4e] [D]
// A flat quad with a colour per corner: 1 top left, 2 bottom right, 3 bottom left, 4 top right.
void DrawGradientBar(float x, float y, float w, float h, u_char r1, u_char g1, u_char b1, u_char r2, u_char g2, u_char b2, u_char r3, u_char g3, u_char b3, u_char r4, u_char g4, u_char b4)
{
	GrVertex v1, v2, v3, v4;

	if (last_state)
	{
		grColorCombine(GR_COMBINE_FUNCTION_LOCAL, GR_COMBINE_FACTOR_ZERO, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_CONSTANT, FXFALSE);
		last_state = FLAT3;
	}

	if (last_trans_on)
	{
		grAlphaBlendFunction(GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_ONE, GR_BLEND_ZERO);
		last_trans_on = 0;
	}

	v1.r = (float)r1;
	v1.g = (float)g1;
	v1.b = (float)b1;
	v2.r = (float)r2;
	v2.g = (float)g2;
	v2.b = (float)b2;
	v3.r = (float)r3;
	v3.g = (float)g3;
	v3.b = (float)b3;
	v4.r = (float)r4;
	v4.g = (float)g4;
	v4.b = (float)b4;

	v1.x = x;
	v1.y = y;
	v2.x = x + w;
	v2.y = y + h;
	v3.x = x;
	v3.y = v2.y;

	if (using_glide)
	{
		v1.oow = 1.0f;
		v2.oow = 1.0f;
		v3.oow = 1.0f;
		v4.oow = 1.0f;
	}
	else
	{
		v1.oow = 0.015625f;
		v2.oow = 0.015625f;
		v3.oow = 0.015625f;
		v4.oow = 0.015625f;
	}

	guDrawTriangleWithClip(&v1, &v2, &v3);

	v4.x = x + w;
	v4.y = y;

	guDrawTriangleWithClip(&v1, &v2, &v4);

	last_state = -1;
}

// [0x4d9d97] [D]
// A bar shaded from the top and bottom colours (1, 4) to the middle (2, 3).
void DrawGouraudBar(float x, float y, float w, float h,
	u_char r1, u_char g1, u_char b1, u_char r2, u_char g2, u_char b2,
	u_char r3, u_char g3, u_char b3, u_char r4, u_char g4, u_char b4)
{
	GrVertex v4;
	GrVertex v3;
	GrVertex v2;
	GrVertex v1;

	if (last_state)
	{
		grColorCombine(GR_COMBINE_FUNCTION_LOCAL, GR_COMBINE_FACTOR_ZERO, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_CONSTANT, FXFALSE);
		last_state = FLAT3;
	}

	if (last_trans_on)
	{
		grAlphaBlendFunction(GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_ONE, GR_BLEND_ZERO);
		last_trans_on = 0;
	}

	v1.r = (float)r1;
	v1.g = (float)g1;
	v1.b = (float)b1;
	v2.r = (float)r2;
	v2.g = (float)g2;
	v2.b = (float)b2;
	v3.r = (float)r3;
	v3.g = (float)g3;
	v3.b = (float)b3;
	v4.r = (float)r4;
	v4.g = (float)g4;
	v4.b = (float)b4;

	// top half
	v1.x = x;
	v1.y = y;
	v2.x = x + w;
	v2.y = h / 2.0f + y;
	v3.x = x;
	v3.y = v2.y;

	if (using_glide)
	{
		v1.oow = 1.0f;
		v2.oow = 1.0f;
		v3.oow = 1.0f;
		v4.oow = 1.0f;
	}
	else
	{
		v1.oow = 1.0f / 64.0f;
		v2.oow = 1.0f / 64.0f;
		v3.oow = 1.0f / 64.0f;
		v4.oow = 1.0f / 64.0f;
	}

	guDrawTriangleWithClip(&v1, &v2, &v3);

	v4.x = x + w;
	v4.y = y;

	guDrawTriangleWithClip(&v1, &v2, &v4);

	// bottom half
	v1.x = x;
	v1.y = y + h;
	v2.x = x + w;
	v2.y = h / 2.0f + y;
	v3.x = x;
	v3.y = v2.y;

	if (using_glide)
	{
		v1.oow = 1.0f;
		v2.oow = 1.0f;
		v3.oow = 1.0f;
		v4.oow = 1.0f;
	}
	else
	{
		v1.oow = 1.0f / 64.0f;
		v2.oow = 1.0f / 64.0f;
		v3.oow = 1.0f / 64.0f;
		v4.oow = 1.0f / 64.0f;
	}

	guDrawTriangleWithClip(&v1, &v2, &v3);

	v4.x = x + w;
	v4.y = y + h;

	guDrawTriangleWithClip(&v1, &v2, &v4);

	last_state = -1;
}

// [0x4da0bf] [D]
void DrawScaledGouraudBar(float x, float y, float w, float h, int color1, int color2, int color3, int color4)
{
	GrVertex v1;
	GrVertex v2;
	GrVertex v3;
	GrVertex v4;

	if (last_state != 0)
	{
		grColorCombine(GR_COMBINE_FUNCTION_LOCAL, GR_COMBINE_FACTOR_ZERO, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_CONSTANT, FXFALSE);
		last_state = FLAT3;
	}

	if (last_trans_on != 0)
	{
		grAlphaBlendFunction(GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_ONE, GR_BLEND_ZERO);
		last_trans_on = 0;
	}

	v1.r = (float)((color1 >> 16) & 0xFF);
	v1.g = (float)((color1 >> 8) & 0xFF);
	v1.b = (float)(color1 & 0xFF);
	v2.r = (float)((color3 >> 16) & 0xFF);
	v2.g = (float)((color3 >> 8) & 0xFF);
	v2.b = (float)(color3 & 0xFF);
	v3.r = (float)((color4 >> 16) & 0xFF);
	v3.g = (float)((color4 >> 8) & 0xFF);
	v3.b = (float)(color4 & 0xFF);
	v4.r = (float)((color2 >> 16) & 0xFF);
	v4.g = (float)((color2 >> 8) & 0xFF);
	v4.b = (float)(color2 & 0xFF);

	v1.x = x;
	v1.y = y;
	v2.x = x + w;
	v2.y = y + h;
	v3.x = x;
	v3.y = v2.y;

	GetScaledXYCoords(&v1.x, &v1.y);
	GetScaledXYCoords(&v2.x, &v2.y);
	GetScaledXYCoords(&v3.x, &v3.y);

	if (using_glide)
	{
		v1.oow = 1.0f;
		v2.oow = 1.0f;
		v3.oow = 1.0f;
		v4.oow = 1.0f;
	}
	else
	{
		v1.oow = 0.015625f;
		v2.oow = 0.015625f;
		v3.oow = 0.015625f;
		v4.oow = 0.015625f;
	}

	guDrawTriangleWithClip(&v1, &v2, &v3);

	v4.x = x + w;
	v4.y = y;

	GetScaledXYCoords(&v4.x, &v4.y);

	guDrawTriangleWithClip(&v1, &v2, &v4);

	last_state = -1;
}

// [0x4da36c] [D]
// 640x480 coordinates to the display, shrunk around the centre by screen_scale.
void GetScaledXYCoords(float *x, float *y)
{
	float sx;
	float sy;

	sx = (*x / 640.0f - 0.5f) * screen_scale + 0.5f;
	sy = (*y / 480.0f - 0.5f) * screen_scale + 0.5f;

	*x = sx * (float)display_width;
	*y = sy * (float)display_height;
}

// [0x4da3f6] [D]
// A texture at x y, w h times its size, coloured r g b; type as last_trans_on.
void DrawTexturedQuad(float x, float y, float w, float h, TEX_INFO *texture, u_char r, u_char g, u_char b, char type)
{
	float u0, v0, u1, v1;
	GrVertex p0, p1, p2, p3;

	if (last_state != TEXT3)
	{
		grColorCombine(GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_LOCAL, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_TEXTURE, FXFALSE);
		last_state = TEXT3;
	}

	switch (type)
	{
		case 0:
			if (last_trans_on)
			{
				grAlphaBlendFunction(GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_ONE, GR_BLEND_ZERO);
				last_trans_on = 0;
			}
			break;
		case 1:
			if (last_trans_on != 1)
			{
				grAlphaBlendFunction(GR_BLEND_ZERO, GR_BLEND_ONE_MINUS_SRC_ALPHA, GR_BLEND_ONE, GR_BLEND_ZERO);
				last_trans_on = 1;
			}
			break;
		case 2:
			if (last_trans_on != 2)
			{
				grAlphaBlendFunction(GR_BLEND_ZERO, GR_BLEND_ONE_MINUS_SRC_COLOR, GR_BLEND_ONE, GR_BLEND_ZERO);
				last_trans_on = 2;
			}
			break;
		case 3:
			if (last_trans_on != 3)
			{
				grAlphaBlendFunction(GR_BLEND_ONE, GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_ZERO);
				last_trans_on = 3;
			}
			break;
	}

	if (texture->tset != last_tset)
	{
		last_tset = texture->tset;
		grTexSource(GR_TMU0, TextureSlots[texture->tset].texture_ram_address, GR_MIPMAPLEVELMASK_BOTH, TextureSlots[texture->tset].TInfo);
	}

	w = w * (float)texture->w;
	h = h * (float)texture->h;

	u0 = (float)texture->u + 0.5f;
	v0 = (float)texture->v + 0.5f;
	u1 = (float)(texture->w + texture->u) - 0.5f;
	v1 = (float)(texture->h + texture->v) - 0.5f;

	if (using_glide)
	{
		p0.oow = 1.0f;
		p1.oow = 1.0f;
		p2.oow = 1.0f;
		p3.oow = 1.0f;
	}
	else
	{
		p0.oow = 0.015625f;
		p1.oow = 0.015625f;
		p2.oow = 0.015625f;
		p3.oow = 0.015625f;
	}

	p0.r = (float)r;
	p0.g = (float)g;
	p0.b = (float)b;
	p1.r = p0.r;
	p1.g = p0.g;
	p1.b = p0.b;
	p2.r = p0.r;
	p2.g = p0.g;
	p2.b = p0.b;
	p3.r = p0.r;
	p3.g = p0.g;
	p3.b = p0.b;

	p0.tmuvtx[0].sow = u0 * p0.oow;
	p0.tmuvtx[0].tow = v0 * p0.oow;
	p1.tmuvtx[0].sow = u1 * p1.oow;
	p1.tmuvtx[0].tow = v0 * p1.oow;
	p2.tmuvtx[0].sow = u1 * p2.oow;
	p2.tmuvtx[0].tow = v1 * p2.oow;
	p3.tmuvtx[0].sow = u0 * p2.oow;
	p3.tmuvtx[0].tow = p2.tmuvtx[0].tow;

	p0.x = x;
	p0.y = y;
	p1.x = x + w;
	p1.y = y;
	p2.x = p1.x;
	p2.y = y + h;
	p3.x = x;
	p3.y = p2.y;

	guDrawTriangleWithClip(&p0, &p1, &p2);
	guDrawTriangleWithClip(&p2, &p3, &p0);
}

// [0x4da8c7] [D]
void MakeRoadMapBMP(char *filename, short *map_data, int width, int height)
{
	char *TempBMP;
	short chunk;
	FILE *f;
	int i;
	int iy;
	int ix;

	TempBMP = (char *)DebugMalloc(height * width, __FILE__, __LINE__);

	if (TempBMP == NULL)
		return;

	memset(TempBMP, 0, height * width);

	for (iy = 0; iy < height; iy++)
	{
		for (ix = 0; ix < width; ix++)
		{
			chunk = map_data[width * iy + ix];

			if ((u_int)chunk < 0x4000)
				TempBMP[width * iy + ix] = 1;
			else if (chunk >= 0x4000)
				TempBMP[width * iy + ix] = 2;

			if (chunk == 0x4000)
				TempBMP[width * iy + ix] = 4;
		}
	}

	*(int *)&bmphdr[18] = width;
	*(int *)&bmphdr[22] = height;

	f = fopen(filename, "wb");

	if (f != NULL)
	{
		fwrite(bmphdr, sizeof(bmphdr), 1, f);

		for (i = 0; i < 16; i++)
			fwrite(bmppal, sizeof(bmppal), 1, f);

		fwrite(TempBMP, height * width, 1, f);
		fclose(f);
	}

	DebugFree(TempBMP);
}

// [0x4dab08] [D]
void InitMemDebug(void)
{
	int loop;

	for (loop = 0; loop < MAX_MEM_PACKETS; loop++)
		mem_info[loop].mem_addr = 0;

	mem_allocated = 0;
	current_mem_usage = 0;
	mem_released = 0;
	peak_mem_usage = 0;
}

// [0x4dab65] [D]
void CloseMemDebug(void)
{
	if (mem_allocated != mem_released)
	{
		int loop;
		char text[256];

		for (loop = 0; loop < MAX_MEM_PACKETS; loop++)
		{
			if (mem_info[loop].mem_addr)
			{
				if (mem_info[loop].filename)
				{
					sprintf(text, "%d bytes of memory were not released from address $%x!\n"
									"This is due to poor use of malloc / free - resolve immediately!\n"
									"Memory Log: %s",
										mem_info[loop].size, mem_info[loop].mem_addr, mem_info[loop].filename);

					free(mem_info[loop].filename);
					mem_info[loop].filename = NULL;
				}
				else
				{
					sprintf(text, "%d bytes of memory were not released from address $%x!\n"
									"This is due to poor use of malloc / free - resolve immediately!",
										mem_info[loop].size, mem_info[loop].mem_addr);
				}

				ShowMessageBox(text, "MEMORY FAULT", MESSAGE_WARNING);

				free((void *)mem_info[loop].mem_addr);
				mem_info[loop].mem_addr = 0;
			}
		}
	}
}

// [0x4dac99] [D]
// malloc with a 4 byte "NAIF" guard after the block and an allocation log entry.
void *DebugMalloc(size_t size, const char *filename, int line)
{
	char *mem;
	char text[128];
	int i;

	sprintf(text, "File: %s (Line: %d)", filename, line);

	mem = (char *)malloc(size + 4);

	if (size)
		ClearMem(mem, size);

	memcpy(&mem[size], "NAIF", 4);

	if (mem)
	{
		for (i = 0; i < MAX_MEM_PACKETS; i++)
		{
			if (mem_info[i].mem_addr == 0)
			{
				mem_info[i].mem_addr = (unsigned long)mem;
				mem_info[i].size = size;

				mem_info[i].filename = (char *)malloc(strlen(text) + 1);

				if (mem_info[i].filename)
					strcpy(mem_info[i].filename, text);

				break;
			}
		}

		mem_allocated += size;
		current_mem_usage += size;

		if (current_mem_usage > peak_mem_usage)
			peak_mem_usage = current_mem_usage;
	}

	return mem;
}

// [0x4dae15] [D]
void DebugFree(void *mem)
{
	int loop;

	if (mem)
	{
		for (loop = 0; loop < MAX_MEM_PACKETS && mem_info[loop].mem_addr != (unsigned long)mem; loop++)
			;

		// NOTE: the original also computes mem + size here into an unused local; dropped

		if (mem_info[loop].filename)
			free(mem_info[loop].filename);

		mem_info[loop].filename = NULL;

		mem_released += mem_info[loop].size;
		current_mem_usage -= mem_info[loop].size;

		mem_info[loop].mem_addr = 0;

		free(mem);
	}
}

// [0x4daef8] [D]
// The direction (0 north - 3 west) nearest to an angle.
int GetCarHeading(short dir)
{
	return ((dir + 512) & 0xFFF) >> 10;
}

// [0x4daf0d] [D]
void RandomInit(int i1, int i2)
{
	if (i1)
		fc = i1 | 1;
	else
		fc = 0;

	if (i2)
		gc = (i2 & 0x7FF) | 1;
	else
		gc = 0;
}

// [0x4daf5d] [D]
// A random number (31 bits); recorded and replayed missions use the frame number so the replay
// stays the same.
int Random(void)
{
	if (gMissionRecording || gMissionReplaying)
		return game_frame & 0x7FFFFFFF;

	gc ^= ((unsigned int)gc >> 15) ^ ((((unsigned int)gc >> 15) ^ gc) << 17);
	fc *= 69069;

	return (gc ^ fc) & 0x7FFFFFFF;
}

// [0x4dafd9] [D]
void AddTextToList(char *text)
{
	if (NextTextListEntry >= MAX_TEXT_LIST_ENTRIES)
		return;

	TextDrawList[NextTextListEntry].color = (u_short)TextColourVal;
	TextDrawList[NextTextListEntry].x = TextOffsetX;
	TextDrawList[NextTextListEntry].y = TextOffsetY;
	TextDrawList[NextTextListEntry].text = &TextListBuffer[CurrTextListEntry];

	while (*text != 0)
	{
		TextListBuffer[CurrTextListEntry++] = *text;

		if (*text == '\n')
		{
			TextOffsetX = 0;
			TextOffsetY++;
		}
		else if (++TextOffsetX == display_width / 8)
		{
			TextOffsetX = 0;
			TextOffsetY++;
		}

		text++;
	}

	TextListBuffer[CurrTextListEntry++] = 0;
	NextTextListEntry++;
}

// [0x4db11c] [D]
void WriteTextFromList(void)
{
	while (NextTextListEntry != 0)
	{
		NextTextListEntry--;

		TextColourVal = TextDrawList[NextTextListEntry].color;
		TextOffsetX = TextDrawList[NextTextListEntry].x;
		TextOffsetY = TextDrawList[NextTextListEntry].y;

		if (!gDisableText)
			TextDraw(TextDrawList[NextTextListEntry].text);
	}

	TextOffsetY = 0;
	TextOffsetX = 0;
	CurrTextListEntry = 0;
}

// [0x4db1b7] [D]
void TextDraw(char *text)
{
	GrLfbInfo_t info;
	short *dest;
	char *line;
	char chr;
	char row;
	int stride;
	int i;
	int j;

	info.size = sizeof(GrLfbInfo_t);

	gr_LfbLock(GR_LFB_WRITE_ONLY, GR_BUFFER_BACKBUFFER, GR_LFBWRITEMODE_ANY, GR_ORIGIN_UPPER_LEFT, FXFALSE, &info);

	stride = info.strideInBytes >> 1;

	line = (char *)info.lfbPtr + 2 * stride * 8 * TextOffsetY + 16 * TextOffsetX;

	while ((chr = *text++) != 0)
	{
		if (chr == '\n')
		{
			TextReset();

			line = (char *)info.lfbPtr + 2 * stride * 8 * TextOffsetY + 16 * TextOffsetX;
		}
		else
		{
			if (chr >= 'a' && chr <= 'z')
				chr -= 0x60;

			dest = (short *)line;

			for (i = 0; i < 8; i++)
			{
				row = DebugFont[8 * chr + i];

				for (j = 0; j < 8; j++)
				{
					if (row & 0x80)
						*dest = TextColourVal;

					dest++;
					row *= 2;
				}

				dest += stride - 8;
			}

			line += 16;
			TextOffsetX++;
		}
	}

	gr_LfbUnlock(GR_LFB_WRITE_ONLY, GR_BUFFER_BACKBUFFER);
}

// [0x4db35d] [D]
void TextReset(void)
{
	TextOffsetX = 0;

	if (++TextOffsetY > (display_height - 8) / 8)
		TextOffsetY = 0;
}

// [0x4db39c] [D]
void TextOffset(int x, int y)
{
	// BUG: tests the previous offset where x and y were meant
	if (TextOffsetX < 0)
		TextOffsetX = display_width / 8 - x;
	else
		TextOffsetX = x;

	if (TextOffsetY < 0)
		TextOffsetY = display_height / 8 - y;
	else
		TextOffsetY = y;
}

// [0x4db3f4] [D]
// RGB 5:6:5
void TextColour(short r, short g, short b)
{
	TextColourVal = (b >> 3) | ((g >> 2) << 5) | ((r >> 3) << 11);
}

// [0x4db41e] [D]
// The loading bar is drawn by DriverFRead every 128K read (UpdateLoadingBar).
void StartLoadingBarNormal(int bytesToRead, int xpos, int ypos, int width, int height)
{
	gLoadingBarX = xpos;
	gLoadingBarY = ypos;
	gLoadingBarWidth = width;
	gLoadingBarHeight = height;
	gTotalBytesToRead = bytesToRead;
	gTotalBytesRead = 0;
	gNextUpdate = LOADING_BAR_STEP;
	gDoLoadingBar = 1;
}

// [0x4db46c] [D]
void StopLoadingBar(void)
{
	gDoLoadingBar = 0;
}

// [0x4db47b] [D]
char DriverFRead(void *dst, int size, int count, FILE *file)
{
	char *ptr;

	ptr = (char *)dst;

	if (count == 1)
	{
		// split up big reads into many smaller ones
		while (size > 65535)
		{
			fread(ptr, 65535, 1, file);

			ptr += 65535;
			size -= 65535;
			gTotalBytesRead += 65535;
		}

		fread(ptr, size, 1, file);
		gTotalBytesRead += size;
	}
	else
	{
		fread(dst, size, count, file);
		gTotalBytesRead += count * size;
	}

	if (gNextUpdate < gTotalBytesRead && gDoLoadingBar)
		UpdateLoadingBar();

	return 1;
}

// [0x4db548] [D]
void UpdateLoadingBar(void)
{
	float done;

	if (gTotalBytesRead >= gTotalBytesToRead)
		return;

	gNextUpdate = gTotalBytesRead + LOADING_BAR_STEP;

	if (gNextUpdate > gTotalBytesToRead)
		gNextUpdate = gTotalBytesToRead;

	// NOTE: the original also computes 30 * done and 255 * done as ints and does not use them
	if (!gTotalBytesRead)
		done = 0.0f;
	else if (gTotalBytesRead >= gTotalBytesToRead)
		done = 1.0f;
	else
		done = (float)gTotalBytesRead / (float)gTotalBytesToRead;

	StartOfFrame();

	DrawGouraudBar((float)gLoadingBarX, (float)gLoadingBarY, done * (float)gLoadingBarWidth, (float)gLoadingBarHeight,
		0, 0, 0, 0, 255, 0, 0, 255, 0, 0, 0, 0);

	EndOfFrame();
}

// [0x4db697] [D]
int DoKeysActiveDelayTick(void)
{
	if (gTicksTillKeysActive <= 0)
		return 0;

	gTicksTillKeysActive--;

	return 1;
}

// [0x4db6bb] [D]
void SetKeysActiveDelay(int ticks)
{
	gTicksTillKeysActive = ticks;
}

// [0x4db6c8] [D] [A]
// NOTE: [A] controllers count: the original only sees the keyboard here
int AnyKeyPressed(void)
{
	int i;

	ReadInput();

	if (ControllerDown)
		return 1;

	i = 255;

	while (i--)
	{
		if (KeyMap[i])
			return 1;
	}

	return 0;
}

// [0x4db706] [D]
void ShowCarDirectionText(short dir)
{
	char text[64];

	sprintf(text, "Dir: %s\n", heading_names[(u_char)GetCarHeading(dir)]);
	AddTextToList(text);
}

// [0x4db74d] [D]
void DisplayCurrentTime(void)
{
	SYSTEMTIME time;

	if (!gDisableDebug)
	{
		GetLocalTime(&time);
		sprintf(current_time, "%02d:%02d.%02d", time.wHour, time.wMinute, time.wSecond);

		TextOffset(display_width / 8 - strlen(current_time), display_height / 8 - 1);
		AddTextToList(current_time);
	}
}

// [0x4db7e7] [D]
void ShowCameraStatus(void)
{
	char text[32];

	sprintf(text, "CameraMode: %d, %d, %d", cameraview, RepCamRecNum, FinalRepCamRec);
	TextOffset(0, 0);
	AddTextToList(text);
}

// [0x4db830] [D]
void SaveScreenShot(void)
{
	FILE *f;
	char name[32];
	void *buffer;

	if (grabnum == -1)
	{
		for (grabnum = 1; ; grabnum++)
		{
			sprintf(name, "PIC_%#04d.BMP", grabnum);

			f = fopen(name, "rb");

			if (f == NULL)
				break;

			fclose(f);
		}
	}

	*(short *)&bmp24hdr[18] = display_width;
	*(short *)&bmp24hdr[22] = display_height;

	sprintf(name, "PIC_%#04d.BMP", grabnum);

	f = fopen(name, "wb");

	if (f != NULL)
	{
		buffer = DebugMalloc(3 * display_height * display_width, __FILE__, __LINE__);

		if (buffer != NULL)
		{
			CopyVisibleScreen(buffer);

			fwrite(bmp24hdr, 1, sizeof(bmp24hdr), f);
			fwrite(buffer, 3, display_height * display_width, f);

			DebugFree(buffer);
		}

		fclose(f);
		grabnum++;
	}
}

// [0x4db9b3] [D]
// Copies the front buffer as bottom-up BGR.
int CopyVisibleScreen(void *buffer)
{
	GrLfbInfo_t info;
	u_short *src;
	u_short pixel;
	char *line;
	char *dest;
	int stride;
	int i;
	int j;

	dest = (char *)buffer;

	info.size = sizeof(GrLfbInfo_t);

	gr_LfbLock(GR_LFB_READ_ONLY, GR_BUFFER_FRONTBUFFER, GR_LFBWRITEMODE_ANY, GR_ORIGIN_UPPER_LEFT, FXFALSE, &info);

	stride = info.strideInBytes >> 1;

	line = (char *)info.lfbPtr + 2 * stride * (display_height - 1);

	for (i = 0; i < display_height; i++)
	{
		src = (u_short *)line;

		for (j = 0; j < display_width; j++)
		{
			pixel = *src++;

			*dest++ = (pixel & 0x1F) << 3;
			*dest++ = (pixel & 0x7E0) >> 3;
			*dest++ = (pixel & 0xF800) >> 8;
		}

		line -= 2 * stride;
	}

	return gr_LfbUnlock(GR_LFB_READ_ONLY, GR_BUFFER_FRONTBUFFER);
}
