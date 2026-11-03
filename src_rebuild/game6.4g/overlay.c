/*
 * overlay.c - the in game overlay: damage/felony/opponent bars, speedometer, the overhead mini map
 * (with cops, targets, compass), the full screen map of the pause menu, target arrows, lap times
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "overlay.c", 0x503890 - 0x50a2d0).
 *
 * The level map (<LEVEL>.MAP, an 8 bit BMP; 16 bit when 8 bit textures are emulated) is kept in
 * map_data; GrabMapGlide copies the 128x128 piece around the player into the MAP_X texture, which
 * DisplayOverheadMap draws rotated with the player. DrawFullscreenMap draws the whole map page by
 * page through TempTPage.
 * Data (0x575f28 - 0x5766e0): the target arrow models, the compass, the map tables and the
 * strings; the zeros after arrow_polys, watch_polys, stop_polys and after each *_model are
 * padding, left out.
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "dr_types.h"
#include "overlay.h"
#include "dr_globals.h"
#include "D3DGlide/glide_ptrs.h"
#include "ai.h"
#include "asmmath.h"
#include "debug.h"
#include "Director.h"
#include "dr_scores.h"
#include "general.h"
#include "handling.h"
#include "map.h"
#include "MScript.h"
#include "pres.h"
#include "shell.h"
#include "testmath.h"
#include "Engine/3dfx/gfxinit.h"
#include "Engine/3dfx/object.h"
#include "Engine/3dfx/polydraw.h"
#include "Engine/3dfx/tset.h"
#include "D3DGlide/system.h"
#include "debris.h"
#include "felony.h"
#include "frontend/cheats.h"
#include "pause.h"

// a flat model drawn over a mission target (DrawTargetArrow): vertices x y (z w unused) in screen
// units, triangles as vertex indices
// NOTE: type name guessed; IDA has the models as float *
typedef struct TARGET_MODEL
{
	float (*verts)[4];
	u_char *tris;
	int numTris;
} TARGET_MODEL;

void DisplaySpeedometer(void);
void EnablePercentageBar(PERCENTAGE_BAR *bar);
void InitPercentageBar(PERCENTAGE_BAR *bar, float size, int color, char *text, int rightSide, int secondLine);
void SetPercentageBar(PERCENTAGE_BAR *bar, char *text);
void DrawPercentageBar(PERCENTAGE_BAR *bar);
void DrawFullscreenMap(void);
int IsNewBestLap(TIMER *prev_lap, TIMER *new_lap);
int TimeToInt(TIMER *time);
void sub_506014(TIMER *a, TIMER *b, TIMER *out);
void DrawTargetArrow(TARGET_MODEL *model, int *pos, int *offset, int *shift);
void sub_506559(char *src, char *dst);
void GrabMapGlide(int force);
void sub_5067B9(char *src, char *dst);
void DownloadPageFromBmp(int tset, int bmp, int x, int y, int w, int h, int stride);
void DownloadPageFromBmp2(int tset, int bmp, int x, int y, int w, int h, int stride);
void DisplayCopIndicators(void);
void CopIndicator(int xpos, int strength);
void DisplayOverheadMap(void);
void DrawCopsOnOverlayMap(void);
void DrawCopFrustrumOnOverlayMap(float x, float z, int civ);
void ConvertWorldToMap(float x, float z, float *map_x, float *map_y);
void sub_5083EC(void);
void sub_5084C4(int target);
void sub_508B27(IVECTOR *pos, IVECTOR *out, int count);
void DrawCentreMapDot(void);
void DrawMapTargets(void);
void DrawMapDot(int *pos, int unused, u_char r, u_char g, u_char b, float scale);
void DrawCompass(void);
void DrawN(VECTOR4 *pos);
void DrawFullscreenMapTargets(void);
void DrawFullscreenTargetDot(float x, float z, u_char r, u_char g, u_char b, float scale);
void WorldToFullscreen(float *x, float *z);
void MapToFullscreen(float *x, float *y);
void sub_50996D(void);
void DrawFullscreenCentreDot(int target);
void sub_509F02(IVECTOR *pos, IVECTOR *out, int count, VECTOR4 *centre, int angle);
void DrawMapTPage(float x, float y, float w, float h, float u0, float v0, float u1, float v1, int tset);

//------------------------------------------------------------------------------------------
// initialised data

float arrow_verts[7][4] =
{
	{ -256.0f, -384.0f, 0.0f, 0.0f },
	{ -128.0f, -512.0f, 0.0f, 0.0f },
	{ 0.0f, 0.0f, 0.0f, 0.0f },
	{ -512.0f, -128.0f, 0.0f, 0.0f },
	{ -384.0f, -256.0f, 0.0f, 0.0f },
	{ -640.0f, -512.0f, 0.0f, 0.0f },
	{ -512.0f, -640.0f, 0.0f, 0.0f },
};

u_char arrow_polys[9] = { 1, 2, 3, 4, 5, 6, 0, 4, 6 };

float exclamation_verts[12][4] =
{
	{ -96.0f, -480.0f, 0.0f, 0.0f },
	{ 96.0f, -480.0f, 0.0f, 0.0f },
	{ -96.0f, 96.0f, 0.0f, 0.0f },
	{ 96.0f, 96.0f, 0.0f, 0.0f },
	{ 88.608f, 356.672f, 0.0f, 0.0f },
	{ 36.672f, 408.608f, 0.0f, 0.0f },
	{ -36.672f, 408.608f, 0.0f, 0.0f },
	{ -88.608f, 356.672f, 0.0f, 0.0f },
	{ -88.608f, 283.328f, 0.0f, 0.0f },
	{ -36.672f, 231.392f, 0.0f, 0.0f },
	{ 36.672f, 231.392f, 0.0f, 0.0f },
	{ 88.608f, 283.328f, 0.0f, 0.0f },
};

u_char exclamation_polys[24] =
{
	0, 1, 2, 1, 2, 3, 4, 5, 6, 4, 6, 7, 11, 4, 7, 11, 7, 8, 10, 11, 8, 10, 8, 9
};

float watch_verts[16][4] =
{
	{ 118.144f, -79.104f, 0.0f, 0.0f },
	{ 48.896f, -9.856f, 0.0f, 0.0f },
	{ -48.896f, -9.856f, 0.0f, 0.0f },
	{ -118.144f, -79.104f, 0.0f, 0.0f },
	{ -118.144f, -176.896f, 0.0f, 0.0f },
	{ -48.896f, -246.144f, 0.0f, 0.0f },
	{ 48.896f, -246.144f, 0.0f, 0.0f },
	{ 118.144f, -176.896f, 0.0f, 0.0f },
	{ -32.0f, -256.0f, 0.0f, 0.0f },
	{ -32.0f, -288.0f, 0.0f, 0.0f },
	{ -64.0f, -288.0f, 0.0f, 0.0f },
	{ -64.0f, -320.0f, 0.0f, 0.0f },
	{ 64.0f, -320.0f, 0.0f, 0.0f },
	{ 64.0f, -288.0f, 0.0f, 0.0f },
	{ 32.0f, -288.0f, 0.0f, 0.0f },
	{ 32.0f, -256.0f, 0.0f, 0.0f },
};

u_char watch_polys[30] =
{
	0, 1, 2, 0, 2, 3, 7, 0, 3, 7, 3, 4, 6, 7, 4, 6, 4, 5, 8, 9, 15, 9, 14, 15, 10, 11, 12, 10, 12, 13
};

float stop_verts[8][4] =
{
	{ 177.216f, -118.656f, 0.0f, 0.0f },
	{ 73.344f, -14.784f, 0.0f, 0.0f },
	{ -73.344f, -14.784f, 0.0f, 0.0f },
	{ -177.216f, -118.656f, 0.0f, 0.0f },
	{ -177.216f, -265.344f, 0.0f, 0.0f },
	{ -73.344f, -369.216f, 0.0f, 0.0f },
	{ 73.344f, -369.216f, 0.0f, 0.0f },
	{ 177.216f, -265.344f, 0.0f, 0.0f },
};

u_char stop_polys[18] = { 0, 1, 2, 0, 2, 3, 7, 0, 3, 7, 3, 4, 6, 7, 4, 6, 4, 5 };

TARGET_MODEL arrow_model = { arrow_verts, arrow_polys, 3 };
TARGET_MODEL exclamation_model = { exclamation_verts, exclamation_polys, 8 };
TARGET_MODEL watch_model = { watch_verts, watch_polys, 10 };
TARGET_MODEL stop_model = { stop_verts, stop_polys, 6 };

// the "N" of the compass: a line strip
XYPAIR NVertex[4] = { { -2, 3 }, { -2, -3 }, { 2, 3 }, { 2, -3 } };

// the compass needle per level (north on the map)
XYPAIR north[7] = { { -20, 0 }, { 3, 20 }, { 6, -19 }, { -6, -19 }, { 20, 0 }, { 9, 18 }, { 9, 18 } };

float overlay_map_x_centre = 540.0f;
float overlay_map_y_centre = 380.0f;

// the part of the level map the full screen map shows: x0 y0 x1 y1 (-1: all of it)
int map_adjustments[7][4] =
{
	{ 163, 172, 957, 661 },
	{ 134, 288, 1066, 994 },
	{ 175, 401, 784, 734 },
	{ 142, 190, 831, 645 },
	{ -1, -1, -1, -1 },
	{ -1, -1, -1, -1 },
	{ -1, -1, -1, -1 },
};

// NOTE: the original has "LEVEL5.MAPLEVEL6.MAP" in [5] (a missing comma, the two strings are
// concatenated) and NULL in [6]; kept so
char *level_maps[7] =
{
	"MIAMI_00.MAP",
	"FRISCO_00.MAP",
	"LA_00.MAP",
	"NY_00.MAP",
	"LEVEL4.MAP",
	"LEVEL5.MAP" "LEVEL6.MAP",
};

// NOTE: read by DisplayCopIndicators (indexed by game_frame & 15), the value is not used
static int unknown_576364[16] = { 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 7, 10, 12, 14 };

//------------------------------------------------------------------------------------------
// uninitialised data

// NOTE: bss globals in the linker's common area (0xbb1c28 - 0xbd1cfc and 0xcff9c0 - 0xcffad8);
// defined here as overlay.c is their only (or main) user
int modulo;								// destination row skip of the map copies
HIGHSCORE_DATA *record_lap;
float map_factor;						// overhead map pixels per map bitmap pixel
float gFSMDotScale;						// pulse of the full screen map dots (-1 .. 1)
float gMapDotScale;						// pulse of the overhead map dots (-1 .. 1)
static GrVertex compass_from;			// NOTE: names guessed (IDA stru_BB1C40, stru_BB1C80)
static GrVertex compass_to;
int fs_map_xstart;
TIMER *best_lap;
int fs_map_ystart;
TIMER last_lap;
int overlay_last_x;
int overlay_last_y;
int fs_map_width;
char TempTPage[0x20000];				// a 256x256 texture page, 16 bit
int fs_map_height;
int src_modulo;							// source row skip of the map copies
int map_flash_timer;
HIGHSCORE_DATA *record_race;

PERCENTAGE_BAR DamageBar[2];			// NOTE: only DamageBar[0] is used ([2] is IDA's type)
int map_data;							// the level map bitmap
int map_size;
TEX_INFO *gDotTexture;
TEX_INFO *gMapTexture;
int map_bmp_width;
TEX_INFO *gFullscreenMapTexture;
DMSVariable *FelonyText;
int display_map;
int map_bmp_height;
PERCENTAGE_BAR OpponentBar;
DMSVariable *DamageText;
int arrow_phase;
int gNumLaps;
PERCENTAGE_BAR FelonyBar;
DMSVariable *OpponentText;
int gLapCount;

// [0x503890] [D]
// Loads the level map bitmap (the mission's overhead_map if set); 16 bit when 8 bit textures are
// emulated.
void LoadLevelMap(void)
{
	unsigned short palette[256];
	BITMAPFILEHEADER file_header;
	BITMAPINFOHEADER info_header;
	unsigned short *dst;
	u_char *src;
	char *name;
	char *pal;
	FILE *fp;
	int loaded;
	int r, g, b;
	int i, j;

	name = level_maps[GameLevel];

	if (map_data)
	{
		DebugFree((void *)map_data);
		map_data = 0;
	}

	if (!name)
	{
		map_data = 0;
		map_bmp_height = 0;
		map_bmp_width = 0;
		map_size = 0;
		display_map = 0;
		return;
	}

	if (overhead_map[0])
		fp = fopen(overhead_map, "rb");
	else
		fp = fopen(level_maps[GameLevel], "rb");

	if (fp)
	{
		DriverFRead(&file_header, 14, 1, fp);
		DriverFRead(&info_header, 40, 1, fp);
		DriverFRead(gPalette[23], 4, 256, fp);

		gPalettePresent[23] = 1;

		// colour 0 is black
		pal = gPalette[23];
		pal[0] = pal[1] = pal[2] = 0;

		map_bmp_width = info_header.biWidth;
		map_bmp_height = info_header.biHeight;
		display_map = 1;
		map_size = 0;

		if (hw_settings.emulate_8bit)
		{
			loaded = 0;

			if (info_header.biBitCount == 8)
			{
				map_data = (int)DebugMalloc(2 * map_bmp_height * map_bmp_width, "D:\\Driver\\game6.4g\\overlay.c", 422);

				if (map_data)
				{
					src = DebugMalloc(map_bmp_height * map_bmp_width, "D:\\Driver\\game6.4g\\overlay.c", 425);

					if (src)
					{
						DriverFRead(src, map_bmp_height, map_bmp_width, fp);

						// the BMP palette (B G R) as 16 bit pixels
						for (i = 0; i < 256; i++)
						{
							r = gPalette[23][i * 4 + 2] >> hw_settings.rbits;
							g = gPalette[23][i * 4 + 1] >> hw_settings.gbits;
							b = gPalette[23][i * 4] >> hw_settings.bbits;

							palette[i] = (b << hw_settings.bshift) | (g << hw_settings.gshift) | (r << hw_settings.rshift);
						}

						dst = (unsigned short *)map_data;

						for (j = 0; j < map_bmp_height; j++)
						{
							for (i = 0; i < map_bmp_width; i++)
								dst[i + map_bmp_width * j] = palette[src[i + map_bmp_width * j]];
						}

						DebugFree(src);
						loaded = 1;
					}
				}
			}

			if (!loaded)
			{
				map_bmp_height = 0;
				map_bmp_width = 0;
				map_size = 0;
				display_map = 0;
			}
		}
		else
		{
			map_data = (int)DebugMalloc(map_bmp_height * map_bmp_width, "D:\\Driver\\game6.4g\\overlay.c", 464);

			if (map_data && info_header.biBitCount == 8)
			{
				DriverFRead((void *)map_data, map_bmp_height, map_bmp_width, fp);
			}
			else
			{
				map_bmp_height = 0;
				map_bmp_width = 0;
				map_size = 0;
				display_map = 0;
			}
		}

		fclose(fp);
	}
	else
	{
		map_bmp_height = 0;
		map_bmp_width = 0;
		map_size = 0;
		display_map = 0;
	}

	if (map_adjustments[GameLevel][0] == -1)
	{
		fs_map_xstart = 0;
		fs_map_ystart = 0;
		fs_map_width = map_bmp_width;
		fs_map_height = map_bmp_height;
	}
	else
	{
		fs_map_xstart = map_adjustments[GameLevel][0];
		fs_map_ystart = map_adjustments[GameLevel][1];
		fs_map_width = map_adjustments[GameLevel][2] - map_adjustments[GameLevel][0];
		fs_map_height = map_adjustments[GameLevel][3] - map_adjustments[GameLevel][1];
	}
}

// [0x503d83] [D]
// Sets up the bars (their texts come from the mission's DAMAGEBAR, FELONYBAR, OPPONENTBAR
// variables) and the map textures.
void InitOverlays(void)
{
	int index;
	int tset;

	map_flash_timer = 0;

	arrow_phase = 0;

	overlay_last_x = -1;
	overlay_last_y = -1;

	if (!gCheatFlags.Invincibility)
		EnablePercentageBar(DamageBar);

	EnablePercentageBar(&FelonyBar);
	EnablePercentageBar(&OpponentBar);

	if (!gCheatFlags.Invincibility)
		InitPercentageBar(DamageBar, (float)MaxPlayerDamage, 0xFF0000, "?", 0, 0);

	index = GetVariableIndex("DAMAGEBAR");

	if (index < 0)
		DamageText = NULL;
	else
		DamageText = &DmsVars[index];

	index = GetVariableIndex("FELONYBAR");

	if (index < 0)
		FelonyText = NULL;
	else
		FelonyText = &DmsVars[index];

	index = GetVariableIndex("OPPONENTBAR");

	if (index < 0)
		OpponentText = NULL;
	else
		OpponentText = &DmsVars[index];

	switch (global_score.type)
	{
		case GAME_TRAILBLAZER:
		case GAME_CHECKPOINT:
		case GAME_SURVIVAL:
		case GAME_DIRTTRACK:
		case GAME_CARNAGE:
		case GAME_TIMETRIAL:
			break;
		default:
			InitPercentageBar(&FelonyBar, 4095.0f, 0xFF, "?", 0, 1);
			break;
	}

	if (gChaseHQMode)
		InitPercentageBar(&OpponentBar, (float)gChaseModeHitsRequired, 0xFF00, "?", 1, 0);
	else if (gDoCrates)
		InitPercentageBar(&OpponentBar, 80.0f, 0xFF00, "?", 1, 0);

	gMapTexture = GetTextureDetails("MAP_X");
	gDotTexture = GetTextureDetails("DOT");
	gFullscreenMapTexture = gMapTexture;
	gMapDotScale = 0.0f;
	gFSMDotScale = 0.0f;

	if (GameLevel >= 4)
	{
		tset = (u_char)gFontTSet;
		TextureSlots[tset].flags |= 2;
		AddTextureSet8(tset, NULL, NULL);
	}
	else if (gMapTexture)
	{
		TextureSlots[gMapTexture->tset].flags |= 2;
		AddTextureSet8(gMapTexture->tset, NULL, NULL);
	}
}

// [0x504046] [D]
// Draws a bar filled with a colour gradient, size above the centre of the screen; type 0 yellow,
// 1 blue/red, 2 green.
void DrawRainbowPercentageBar(float percent, float size, int type)
{
	float bar_w;
	float bar_h;
	float x;
	float y;
	float w;
	float h;

	bar_w = 0.40625f * (float)display_width;
	bar_h = 0.033333335f * (float)display_height;

	x = -(bar_w / 2.0f) * screen_scale;
	x = x + (float)(display_width / 2);

	y = (float)(display_height / 2) - (bar_h + size);
	y = y * screen_scale;
	y = y + (float)(display_height / 2);

	w = bar_w * screen_scale;
	h = bar_h * screen_scale;

	DrawQuad(x - 1.0f, y - 1.0f, w + 3.0f, 1.0f, 0x7F7F7F);
	DrawQuad(x - 1.0f, y + h, w + 3.0f, 1.0f, 0x7F7F7F);
	DrawQuad(x - 1.0f, y, 1.0f, h, 0x7F7F7F);
	DrawQuad(x + w + 1.0f, y, 1.0f, h, 0x7F7F7F);

	if (percent > 1.0f)
		percent = 1.0f;

	w = w * percent;

	if (type == 0)
	{
		DrawGradientBar(x, y, w, h / 2.0f, 64, 64, 0, 255, 0, 0, 255, 255, 0, 64, 0, 0);
		DrawGradientBar(x, h / 2.0f + y, w, h / 2.0f, 255, 255, 0, 64, 0, 0, 64, 64, 0, 255, 0, 0);
	}
	else if (type == 1)
	{
		DrawGradientBar(x, y, w, h / 2.0f, 0, 0, 64, 0, 0, 255, 255, 0, 0, 0, 0, 64);
		DrawGradientBar(x, h / 2.0f + y, w, h / 2.0f, 255, 0, 0, 0, 0, 64, 64, 0, 0, 0, 0, 255);
	}
	else if (type == 2)
	{
		DrawGradientBar(x, y, w, h / 2.0f, 0, 64, 0, 255, 0, 0, 0, 255, 0, 64, 0, 0);
		DrawGradientBar(x, h / 2.0f + y, w, h / 2.0f, 0, 255, 0, 64, 0, 0, 0, 64, 0, 255, 0, 0);
	}
}

// [0x5043bc] [D]
// Pulses the map dots: the full screen map's while paused, the overhead map's otherwise.
void HandleOverlays(void)
{
	if (GamePaused)
	{
		gFSMDotScale = gFSMDotScale + 0.05f;

		if (gFSMDotScale >= 1.0f)
			gFSMDotScale = -1.0f;
	}
	else
	{
		gMapDotScale = gMapDotScale + 0.05f;

		if (gMapDotScale >= 1.0f)
			gMapDotScale = -1.0f;
	}
}

// [0x50442a] [D]
void DisplayOverlays(void)
{
	float width;
	float half_width;
	float half_height;

	if (Director.Mode)
		return;

	if (DamageText && !gCheatFlags.Invincibility)
		SetPercentageBar(DamageBar, (char *)DamageText->data);

	if (FelonyText)
		SetPercentageBar(&FelonyBar, (char *)FelonyText->data);

	if (OpponentText)
		SetPercentageBar(&OpponentBar, (char *)OpponentText->data);

	// the overhead map at 540, 380 of 640x480, scaled to the screen
	width = (float)display_width;
	map_factor = 0.0022216798f * width * screen_scale;
	overlay_map_x_centre = width / 640.0f * 540.0f;
	overlay_map_y_centre = (float)display_height / 480.0f * 380.0f;

	half_width = (float)(display_width / 2);
	overlay_map_x_centre = overlay_map_x_centre - half_width;
	half_height = (float)(display_height / 2);
	overlay_map_y_centre = overlay_map_y_centre - half_height;

	overlay_map_x_centre = overlay_map_x_centre * screen_scale;
	overlay_map_y_centre = overlay_map_y_centre * screen_scale;

	overlay_map_x_centre = overlay_map_x_centre + half_width;
	overlay_map_y_centre = overlay_map_y_centre + half_height;

	if (gChaseHQMode)
		OpponentBar.position = (float)gChaseModeHits;
	else if (gDoCrates)
		OpponentBar.position = (float)ExBoxDamage;

	if (GameLevel < 4)
		FelonyBar.position = (float)FelonyRating;

	if (!gCheatFlags.Invincibility)
		DamageBar[0].position = (float)gPlayerCarDamage;

	if (gDrawMap && !gDoTraining && !gRecordTraining && !gDoInterview)
	{
		if (CopsCanSeePlayer && FelonyRating >= 682 && !gDoRearViewMirror)
			DisplayCopIndicators();

		grTexFilterMode(GR_TMU0, GR_TEXTUREFILTER_POINT_SAMPLED, GR_TEXTUREFILTER_POINT_SAMPLED);

		DisplayOverheadMap();

		grTexFilterMode(GR_TMU0, GR_TEXTUREFILTER_BILINEAR, GR_TEXTUREFILTER_BILINEAR);

		DisplaySpeedometer();
	}

	if (gDoTraining || gDoInterview || gbGameDemo)
	{
		if ((global_score.type == GAME_DIRTTRACK || global_score.type == GAME_TIMETRIAL) && !gCheatFlags.Invincibility)
		{
			DamageBar[0].max = (float)MaxPlayerDamage;
			DrawPercentageBar(DamageBar);
		}
	}
	else
	{
		if (!gCheatFlags.Invincibility)
		{
			DamageBar[0].max = (float)MaxPlayerDamage;
			DrawPercentageBar(DamageBar);
		}

		DrawPercentageBar(&FelonyBar);
		DrawPercentageBar(&OpponentBar);
	}
}

// [0x5046fc] [D]
// The player's speed; flashes when over the limit with the felony bar on.
void DisplaySpeedometer(void)
{
	char text[32];
	byte flash;

	if (gDoTraining || gDoInterview)
		return;

	sprintf(text, "%d%s", 3 * car_data[0].speed / 10, gLanguageStrings[299]);

	if (car_data[0].speed > 200 && FelonyBar.active)
		flash = game_frame * 8;
	else
		flash = 255;

	WriteDropShadowedString(605, 450, text, 0, 2, 255, flash, flash);
}

// [0x5047a0] [D]
// NOTE: clears active despite the name (InitPercentageBar sets it): a bar the game does not set
// up stays hidden
void EnablePercentageBar(PERCENTAGE_BAR *bar)
{
	bar->active = 0;
}

// [0x5047af] [D]
// Sets up a bar at the top left (or right) of the screen, on the first or second line;
// color is 0xRRGGBB.
void InitPercentageBar(PERCENTAGE_BAR *bar, float size, int color, char *text, int rightSide, int secondLine)
{
	int flags;

	if (rightSide)
	{
		bar->xpos = 624.0f;
		flags = 2;
	}
	else
	{
		bar->xpos = 16.0f;
		flags = 0;
	}

	if (secondLine)
		bar->ypos = 75.0f;
	else
		bar->ypos = 30.0f;

	bar->width = 165.0f;
	bar->height = 18.0f;
	bar->position = 0.0f;
	bar->max = size;
	bar->red = (float)((color & 0xFF0000) >> 16);
	bar->green = (float)((color & 0xFF00) >> 8);
	bar->blue = (float)(color & 0xFF);
	bar->text = text;
	bar->flags = flags;
	bar->active = 1;
}

// [0x504899] [D]
void SetPercentageBar(PERCENTAGE_BAR *bar, char *text)
{
	bar->text = text;
}

// [0x5048a7] [D]
void DrawPercentageBar(PERCENTAGE_BAR *bar)
{
	float xscale;
	float yscale;
	float xpos;
	float text_y;
	float x;
	float y;
	float w;
	float h;
	float x0, x1;
	float y0;
	float oow;
	GrVertex verts[4];

	if (!bar->active)
		return;

	xscale = (float)display_width / 640.0f;
	yscale = (float)display_height / 480.0f;

	if (bar->flags & 2)
		xpos = bar->xpos - bar->width;
	else
		xpos = bar->xpos;

	text_y = bar->ypos - bar->height;

	x = GetScaledXCoord(xpos);
	y = GetScaledYCoord(bar->ypos);
	w = xscale * bar->width * screen_scale;
	h = yscale * bar->height * screen_scale;

	DrawQuad(x - 1.0f, y - 1.0f, w + 2.0f, 1.0f, 0x808080);
	DrawQuad(x - 1.0f, y + h, w + 2.0f, 1.0f, 0x808080);
	DrawQuad(x - 1.0f, y, 1.0f, h + 1.0f, 0x808080);
	DrawQuad(x + w, y, 1.0f, h + 1.0f, 0x808080);
	DrawQuadTC(x, y, w, h, 0x303030);

	if (bar->position > bar->max)
		bar->position = bar->max;

	// the filled part: x0 .. x1 and y0 .. 1 of the bar
	if (bar->flags & 1)
	{
		x0 = 0.0f;
		x1 = 1.0f;
		y0 = 1.0f - bar->position / bar->max;
	}
	else
	{
		if (bar->flags & 2)
		{
			x0 = 1.0f - bar->position / bar->max;
			x1 = 1.0f;
		}
		else
		{
			x0 = 0.0f;
			x1 = bar->position / bar->max;
		}

		y0 = 0.0f;
	}

	if (x0 != x1 && y0 != 1.0f)
	{
		if (using_glide)
			oow = 1.0f;
		else
			oow = 0.015625f;

		verts[0].x = x0 * w + x;
		verts[0].y = y0 * h + y;
		verts[0].oow = oow;
		verts[0].r = bar->red;
		verts[0].g = bar->green;
		verts[0].b = bar->blue;

		verts[1].x = x1 * w + x;
		verts[1].y = verts[0].y;
		verts[1].oow = oow;
		verts[1].r = bar->red;
		verts[1].g = bar->green;
		verts[1].b = bar->blue;

		verts[2].x = verts[1].x;
		verts[2].y = 1.0f * h + y;
		verts[2].oow = oow;
		verts[2].r = bar->red / 4.0f;
		verts[2].g = bar->green / 4.0f;
		verts[2].b = bar->blue / 4.0f;

		verts[3].x = verts[0].x;
		verts[3].y = verts[2].y;
		verts[3].oow = oow;
		verts[3].r = bar->red / 4.0f;
		verts[3].g = bar->green / 4.0f;
		verts[3].b = bar->blue / 4.0f;

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

		grDrawPolygonVertexList(4, verts);
	}

	SwapPalette(24);
	SetTextColour(255, 255, 255);

	WriteCustomScaledString(xpos + 2.0f, text_y + 2.0f, bar->text, 0, -1);
	WriteCustomScaledString(xpos, text_y, bar->text, 0, 1);
}

// [0x504e0e] [D]
// Arrows (or !, watch, stop signs) over the mission targets; over the recorded car when replaying
// a mission.
void DrawTargetArrows(void)
{
	MISSION_TARGET *target;
	TARGET_MODEL *model;
	LONGVECTOR4 pos;
	int offset[3];
	int shift[3];
	int i;

	if (gDoTraining)
		return;

	if (gMissionReplaying)
	{
		pos[0] = car_data[65].location.vx;
		pos[1] = 0;
		pos[2] = car_data[65].location.vz;

		offset[2] = 0;
		offset[1] = 0;
		offset[0] = 0;

		shift[0] = -200;
		shift[1] = 0;
		shift[2] = 0;

		DrawTargetArrow(&arrow_model, pos, offset, shift);
	}

	if (!mission.loaded)
		return;

	target = MissionTargets;

	for (i = 0; i < 10; i++, target++)
	{
		if ((target->flags & 1) && !(target->flags & 0x20))
		{
			if (target->flags & 4)
				model = &exclamation_model;
			else if (target->flags & 2)
				model = &watch_model;
			else if (target->flags & 0x200)
				model = &stop_model;
			else
				model = &arrow_model;

			*(IVECTOR *)pos = target->position;

			shift[2] = 0;
			shift[1] = 0;
			shift[0] = 0;

			offset[0] = target->offset.vx;
			offset[1] = target->offset.vy;
			offset[2] = target->offset.vz;

			DrawTargetArrow(model, pos, offset, shift);
		}
	}
}

// [0x504f96] [D]
// Shadows under the target arrows.
void DrawTargetShadows(void)
{
	MISSION_TARGET *target;
	VECTOR4 pos;
	int x, y, z;
	int i;

	if (!pedshadow_texture || gDoTraining)
		return;

	gPedPoly.r = 255.0f;
	gPedPoly.g = 255.0f;
	gPedPoly.b = 255.0f;

	target = MissionTargets;

	if (do_fogging)
		grFogMode(GR_FOG_DISABLE);

	SwapPalette(0);

	for (i = 0; i < 10; i++, target++)
	{
		if ((target->flags & 1) && !(target->flags & 0x20))
		{
			x = target->position.vx;
			y = target->position.vy;
			z = target->position.vz;

			pos.vx = (float)(target->offset.vx + x);
			pos.vy = (float)y;
			pos.vz = (float)(target->offset.vz + z);

			if (PositionVisible(&pos, CurrentPVS, camera.current_cell_x, camera.current_cell_z) && !(target->flags & 0x108) && (target->flags & 1))
			{
				pos.vx = (float)x;
				pos.vz = (float)z;

				DrawPedShadow(&pos.vx, 2 * arrow_phase + 150);
			}
		}
	}

	if (do_fogging)
		grFogMode(GR_FOG_WITH_TABLE);
}

// [0x505102] [D]
// The full screen map (pause menu): the map bitmap drawn in 256x256 pages, then the player and
// the targets.
void DrawFullscreenMap(void)
{
	void *data;
	float page_w;
	float page_h;
	int pages_x;
	int pages_y;
	int i, j;

	if (!gFullscreenMapTexture)
	{
		LogQuick("DrawFullscreenMap: No target texture set for map data.  Procedure aborted.");
		return;
	}

	if (!map_data)
	{
		LogQuick("DrawFullscreenMap: No source map data to display.  Procedure aborted.");
		return;
	}

	if (!hw_settings.emulate_8bit)
	{
		data = TextureSlots[gFullscreenMapTexture->tset].data;
		TextureSlots[gFullscreenMapTexture->tset].data = TempTPage;
	}

	SwapPalette(23);

	pages_x = map_bmp_width >> 8;
	pages_y = map_bmp_height >> 8;

	if (map_bmp_width & 0xFF)
		pages_x++;

	if (map_bmp_height & 0xFF)
		pages_y++;

	for (i = 0; i < pages_y; i++)
	{
		page_h = (float)(map_bmp_height - (i << 8));

		if (page_h > 255.0f)
			page_h = 256.0f;

		for (j = 0; j < pages_x; j++)
		{
			page_w = (float)(map_bmp_width - (j << 8));

			if (page_w > 255.0f)
				page_w = 256.0f;

			if (hw_settings.emulate_8bit)
			{
				DownloadPageFromBmp2(gMapTexture->tset, map_data, j << 8, i << 8, (int)page_w, (int)page_h, map_bmp_width);
				DrawMapTPage((float)j * 256.0f, (float)i * 256.0f, page_w, page_h, 0.0f, 0.0f, page_w, page_h, gMapTexture->tset);
			}
			else
			{
				DownloadPageFromBmp(gFullscreenMapTexture->tset, map_data, j << 8, i << 8, (int)page_w, (int)page_h, map_bmp_width);
				DrawMapTPage((float)j * 256.0f, (float)i * 256.0f, page_w, page_h, 0.0f, 0.0f, page_w, page_h, gFullscreenMapTexture->tset);
			}
		}
	}

	if (hw_settings.emulate_8bit)
	{
		// the overhead map's piece back in the texture
		GrabMapGlide(1);
	}
	else
	{
		TextureSlots[gFullscreenMapTexture->tset].data = data;
		ReplaceTextureSet(gFullscreenMapTexture->tset, data);
	}

	DrawFullscreenMapTargets();
}

// [0x5053ee] [D]
// The texts of the driving games: progress, lap times and records.
void DrawGameSpecificOverlays(void)
{
	char text[32];

	switch (global_score.type)
	{
		case GAME_TRAILBLAZER:
			WriteDropShadowedString(470, 10, gLanguageStrings[215], 0, 0, 255, 255, 128);

			sprintf(text, "%d", mission.iTrailIndex);
			WriteDropShadowedString(570, 25, text, 1, 2, 255, 255, 255);

			sprintf(text, "/%d", mission.iTrailCount);
			WriteDropShadowedString(570, 50, text, 0, 0, 255, 255, 255);

			WriteDropShadowedString(470, 80, gLanguageStrings[217], 0, 0, 255, 255, 128);

			sprintf(text, "%s:", gLanguageStrings[216]);
			WriteDropShadowedString(470, 105, text, 0, 0, 255, 255, 128);

			sprintf(text, "-");

			if (record_race && record_race->lap_info.flags == 8)
				sprintf(text, "%d", record_race->items);

			WriteDropShadowedString(540, 105, text, 0, 0, 255, 255, 128);

			sprintf(text, "%s:", gLanguageStrings[218]);
			WriteDropShadowedString(470, 130, text, 0, 0, 255, 255, 128);

			sprintf(text, "--:--:--");

			if (record_race && record_race->lap_info.flags == 8)
				sprintf(text, "%02d:%02d:%02d", record_race->lap_info.min, record_race->lap_info.sec, record_race->lap_info.frac);

			WriteDropShadowedString(540, 130, text, 0, 0, 255, 255, 128);
			break;
		case GAME_CHECKPOINT:
			WriteDropShadowedString(550, 10, gLanguageStrings[222], 0, 1, 255, 255, 128);

			sprintf(text, "%d", mission.checkpoint->index);
			WriteDropShadowedString(550, 30, text, 1, 2, 255, 255, 255);

			sprintf(text, "/%d", mission.checkpoint->count);
			WriteDropShadowedString(550, 55, text, 0, 0, 255, 255, 255);

			sprintf(text, "%s: --:--:--", gLanguageStrings[217]);

			if (record_race && record_race->lap_info.flags == 8)
				sprintf(text, "%s: %02d:%02d:%02d", gLanguageStrings[217], record_race->lap_info.min, record_race->lap_info.sec, record_race->lap_info.frac);

			WriteDropShadowedString(550, 80, text, 0, 1, 255, 255, 128);
			break;
		case GAME_SURVIVAL:
			sprintf(text, "%s: --:--:--", gLanguageStrings[217]);

			if (record_race && record_race->lap_info.flags == 8)
				sprintf(text, "%s: %02d:%02d:%02d", gLanguageStrings[217], record_race->lap_info.min, record_race->lap_info.sec, record_race->lap_info.frac);

			WriteDropShadowedString(470, 10, text, 0, 0, 255, 255, 128);
			break;
		case GAME_DIRTTRACK:
		case GAME_TIMETRIAL:
			WriteDropShadowedString(470, 10, gLanguageStrings[223], 0, 0, 255, 255, 128);

			sprintf(text, "%d", gLapCount);
			WriteDropShadowedString(550, 30, text, 1, 2, 255, 255, 255);

			// endless laps on the dirt track
			if (global_score.type == GAME_DIRTTRACK)
				sprintf(text, "/%c", 127);
			else
				sprintf(text, "/%d", gNumLaps);

			WriteDropShadowedString(550, 55, text, 0, 0, 255, 255, 255);

			sprintf(text, "%s:", gLanguageStrings[219]);
			WriteDropShadowedString(470, 80, text, 0, 0, 255, 255, 128);

			if (last_lap.flags == 8)
				sprintf(text, "%02d:%02d:%02d", last_lap.min, last_lap.sec, last_lap.frac);
			else
				sprintf(text, "--:--:--");

			WriteDropShadowedString(540, 80, text, 0, 0, 255, 255, 128);

			sprintf(text, "%s:", gLanguageStrings[217]);
			WriteDropShadowedString(470, 105, text, 0, 0, 255, 255, 128);

			sprintf(text, "--:--:--");

			if (best_lap && best_lap->flags == 8)
				sprintf(text, "%02d:%02d:%02d", best_lap->min, best_lap->sec, best_lap->frac);

			WriteDropShadowedString(540, 105, text, 0, 0, 255, 255, 128);

			WriteDropShadowedString(16, 55, gLanguageStrings[214], 0, 0, 255, 255, 128);

			sprintf(text, "%s:", gLanguageStrings[220]);
			WriteDropShadowedString(16, 80, text, 0, 0, 255, 255, 128);

			sprintf(text, "--:--:--");

			if (record_lap && record_lap->lap_info.flags == 8)
				sprintf(text, "%02d:%02d:%02d", record_lap->lap_info.min, record_lap->lap_info.sec, record_lap->lap_info.frac);

			WriteDropShadowedString(86, 80, text, 0, 0, 255, 255, 128);

			sprintf(text, "%s:", gLanguageStrings[221]);
			WriteDropShadowedString(16, 105, text, 0, 0, 255, 255, 128);

			sprintf(text, "--:--:--");

			if (record_race && record_race->lap_info.flags == 8)
				sprintf(text, "%02d:%02d:%02d", record_race->lap_info.min, record_race->lap_info.sec, record_race->lap_info.frac);

			WriteDropShadowedString(86, 105, text, 0, 0, 255, 255, 128);
			break;
		case GAME_CARNAGE:
			sprintf(text, "%s:", gLanguageStrings[225]);
			WriteDropShadowedString(535, 80, text, 0, 2, 255, 255, 128);

			sprintf(text, "$%d", carnage_score);
			WriteDropShadowedString(545, 80, text, 0, 0, 255, 255, 255);

			WriteDropShadowedString(16, 55, gLanguageStrings[214], 0, 0, 255, 255, 128);

			sprintf(text, "%s:", gLanguageStrings[225]);
			WriteDropShadowedString(16, 80, text, 0, 0, 255, 255, 128);

			sprintf(text, "$-");

			if (record_race && record_race->lap_info.flags == 8)
				sprintf(text, "$%d", record_race->items);

			WriteDropShadowedString(120, 80, text, 0, 0, 255, 255, 128);
			break;
	}
}

// [0x505d23] [D]
// Clears the lap times and looks up the records of the driving game.
void InitLapTimes(void)
{
	HIGHSCORE_DATA *table;

	record_lap = NULL;
	record_race = NULL;
	best_lap = NULL;

	last_lap.min = 0;
	last_lap.sec = 0;
	last_lap.frac = 0;
	last_lap.flags = 0;

	global_score.complete = 0;
	global_score.items = 0;

	global_score.lap_info.min = 0;
	global_score.lap_info.sec = 0;
	global_score.lap_info.frac = 0;
	global_score.lap_info.flags = 0;

	global_score.best_lap.min = 0;
	global_score.best_lap.sec = 0;
	global_score.best_lap.frac = 0;
	global_score.best_lap.flags = 0;

	switch (global_score.type)
	{
		case GAME_TRAILBLAZER:
			global_score.complete = 1;

			table = GetHighScoreTable(global_score.type, global_score.index);

			if (table)
				record_race = table;
			break;
		case GAME_CHECKPOINT:
		case GAME_SURVIVAL:
			table = GetHighScoreTable(global_score.type, global_score.index);

			if (table)
				record_race = table;
			break;
		case GAME_DIRTTRACK:
		case GAME_TIMETRIAL:
			table = GetHighScoreTable(global_score.type, global_score.index);

			if (table)
				record_race = table;

			table = GetHighScoreTable(GAME_BESTLAP, global_score.index);

			if (table)
				record_lap = table;

			best_lap = &global_score.best_lap;
			break;
		case GAME_CARNAGE:
			table = GetHighScoreTable(global_score.type, global_score.index);

			if (table)
				record_race = table;
			break;
	}
}

// [0x505eff] [D]
// Takes the time of the lap just finished; returns 1 if it is the best lap.
int UpdateBestLap(void)
{
	if (global_score.type == GAME_DIRTTRACK)
	{
		last_lap = *mission.timer;
	}
	else
	{
		// the timer runs over the whole race: the lap is the time since the last one
		sub_506014(mission.timer, &global_score.lap_info, &last_lap);
		global_score.lap_info = *mission.timer;
	}

	last_lap.flags = 8;

	if (!IsNewBestLap(best_lap, &last_lap))
		return 0;

	*best_lap = last_lap;
	best_lap->flags = 8;

	return 1;
}

// [0x505fa3] [D]
int IsNewBestLap(TIMER *prev_lap, TIMER *new_lap)
{
	int prev;

	if (prev_lap->flags != 8)
		return 1;

	prev = TimeToInt(prev_lap);

	return TimeToInt(new_lap) < prev;
}

// [0x505fec] [D]
int TimeToInt(TIMER *time)
{
	return time->frac | (time->sec << 8) | (time->min << 16);
}

// [0x506014] [D]
// out = a - b (minutes, seconds, hundredths)
void sub_506014(TIMER *a, TIMER *b, TIMER *out)
{
	int min;
	int sec;
	int frac;

	min = a->min - b->min;
	sec = a->sec - b->sec;

	if (sec < 0)
	{
		min--;
		sec += 60;
	}

	frac = a->frac - b->frac;

	if (frac < 0)
	{
		sec--;
		frac += 100;
	}

	out->min = min;
	out->sec = sec;
	out->frac = frac;
}

// [0x506096] [D]
// Draws a target model bobbing 640 above the ground at pos + offset; shift moves it on the screen
// (camera space).
void DrawTargetArrow(TARGET_MODEL *model, int *pos, int *offset, int *shift)
{
	VECTOR4 v;
	float ooz;
	float scale;
	float (*verts)[4];
	u_char *tri;
	GrVertex p0, p1, p2;
	int i;

	if (CutSceneActive)
		return;

	v.vx = (float)pos[0];
	v.vy = (float)pos[1];
	v.vz = (float)pos[2];

	v.vx = v.vx + (float)offset[0];
	v.vz = v.vz + (float)offset[2];

	if (!PositionVisible(&v, CurrentPVS, camera.current_cell_x, camera.current_cell_z))
		return;

	v.vy = (float)(-MapHeight(&v) - 640);
	v.vy = v.vy - (float)offset[1];

	if (!GamePaused)
		arrow_phase += 2;

	if (arrow_phase >= 64)
		arrow_phase = 0;

	v.vx = v.vx - camera_position.vx;
	v.vy = v.vy - camera_position.vy;
	v.vz = v.vz - camera_position.vz;

	CurrentRotationMatrix = &inv_camera_matrix;
	MatrixRotate(&v);

	if (model == &arrow_model)
		v.vx = v.vx + (float)(arrow_phase * 4);

	v.vx = v.vx + (float)shift[0];
	v.vy = v.vy + (float)(shift[1] + arrow_phase * 4);
	v.vz = v.vz + (float)shift[2];

	if (v.vz < 50.0f)
		return;

	ooz = ooz_table[(int)v.vz];
	scale = DIST * ooz;

	p0.oow = ooz;
	p1.oow = ooz;
	p2.oow = ooz;

	p0.r = 255.0f;
	p1.r = 255.0f;
	p2.r = 255.0f;

	p0.b = 0.0f;
	p1.b = 0.0f;
	p2.b = 0.0f;

	p0.g = 0.0f;
	p1.g = 0.0f;
	p2.g = 0.0f;

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

	verts = model->verts;
	tri = model->tris;

	for (i = 0; i < model->numTris; i++)
	{
		p0.x = (v.vx + verts[*tri][0]) * scale + screen_xmid - VERTEX_SNAPPER;
		p0.y = (v.vy + verts[*tri][1]) * scale + screen_ymid - VERTEX_SNAPPER;
		tri++;

		p1.x = (v.vx + verts[*tri][0]) * scale + screen_xmid - VERTEX_SNAPPER;
		p1.y = (v.vy + verts[*tri][1]) * scale + screen_ymid - VERTEX_SNAPPER;
		tri++;

		p2.x = (v.vx + verts[*tri][0]) * scale + screen_xmid - VERTEX_SNAPPER;
		p2.y = (v.vy + verts[*tri][1]) * scale + screen_ymid - VERTEX_SNAPPER;
		tri++;

		guDrawTriangleWithClip(&p0, &p1, &p2);
	}
}

// [0x506559] [D]
// Copies 128x128 bytes of the map (8 bit) to the texture, rows skipping src_modulo / modulo.
void sub_506559(char *src, char *dst)
{
	int i, j;

	for (i = 0; i < 128; i++)
	{
		for (j = 0; j < 128; j++)
			*dst++ = *src++;

		src += src_modulo;
		dst += modulo;
	}
}

// [0x506599] [D]
// Copies the 128x128 piece of the map around the player into the MAP_X texture when the player
// moved to another map pixel (or force).
void GrabMapGlide(int force)
{
	char *dst;
	int x, y;

	if (gMissionRecording)
	{
		x = map_bmp_width / 2 + (car_data[65].location.vx + 750) / 1500;
		y = map_bmp_height / 2 + (750 - car_data[65].location.vz) / 1500;
	}
	else
	{
		x = map_bmp_width / 2 + (car_data[0].location.vx + 750) / 1500;
		y = map_bmp_height / 2 + (750 - car_data[0].location.vz) / 1500;
	}

	if (x == overlay_last_x && y == overlay_last_y && !force)
		return;

	overlay_last_x = x;
	overlay_last_y = y;

	if (hw_settings.emulate_8bit)
	{
		dst = &TempTPage[gMapTexture->v * 512 + gMapTexture->u * 2];
		modulo = 128;
		src_modulo = map_bmp_width - 128;

		sub_5067B9((char *)(map_data + map_bmp_width * 2 * (y - 64) + x * 2 - 128), dst);
		ReplaceEmulatedTexture16(gMapTexture->tset, TempTPage);
	}
	else
	{
		dst = (char *)TextureSlots[gMapTexture->tset].data + gMapTexture->v * 256 + gMapTexture->u;
		modulo = 128;
		src_modulo = map_bmp_width - 128;

		sub_506559((char *)(map_bmp_width * (y - 64) + map_data + x - 64), dst);
		ReplaceTextureSet(gMapTexture->tset, TextureSlots[gMapTexture->tset].data);
	}
}

// [0x5067b9] [D]
// Copies 128x128 pixels of the map (16 bit) to the texture, rows skipping src_modulo / modulo
// pixels.
void sub_5067B9(char *src, char *dst)
{
	int i;

	for (i = 0; i < 128; i++)
	{
		CopyMem(dst, src, 256);

		src += src_modulo * 2 + 256;
		dst += modulo * 2 + 256;
	}
}

// [0x506817] [D]
// Copies w x h pixels at x, y of an 8 bit bitmap (stride pixels a row) to TempTPage and loads it
// into the texture set.
void DownloadPageFromBmp(int tset, int bmp, int x, int y, int w, int h, int stride)
{
	char *src;
	char *dst;
	int i;

	if (!bmp)
	{
		LogQuick("DownloadPageFromBmp: Bitmap not found, operation aborted.");
		return;
	}

	src = (char *)(x + stride * y + bmp);

	for (dst = TempTPage; h--; dst += 256 - w)
	{
		for (i = w; i--; )
			*dst++ = *src++;

		src += stride - w;
	}

	ReplaceTextureSet(tset, TempTPage);
}

// [0x5068cb] [D]
// DownloadPageFromBmp for a 16 bit bitmap and an emulated texture.
void DownloadPageFromBmp2(int tset, int bmp, int x, int y, int w, int h, int stride)
{
	unsigned short *src;
	char *dst;
	int i;

	if (!bmp)
	{
		LogQuick("DownloadPageFromBmp: Bitmap not found, operation aborted.");
		return;
	}

	src = (unsigned short *)(bmp + (x + stride * y) * 2);

	for (dst = TempTPage; h--; dst += (256 - w) * 2)
	{
		for (i = w; i--; )
		{
			*(unsigned short *)dst = *src;
			dst += 2;
			src++;
		}

		src += stride - w;
	}

	ReplaceEmulatedTexture16(tset, TempTPage);
}

// [0x506986] [D]
// Red triangles at the bottom of the screen towards the pursuing cops ahead.
void DisplayCopIndicators(void)
{
	CIV_STATE *civ;
	int dx, dz;
	int side;
	int ahead;
	int c, s;
	int i;

	c = icos(car_data[0].direction);
	s = isin(car_data[0].direction);

	civ = civilian_ai_data;

	for (i = 0; i < 64; i++, civ++)
	{
		if (civ->ctrlState && civ->IsCopCar)
		{
			dx = car_data[i + 1].where.t[0] - car_data[0].where.t[0];
			dz = car_data[i + 1].where.t[2] - car_data[0].where.t[2];

			side = 3 * ((c * dx - s * dz + 2048) >> 12) / 4;
			ahead = -((s * dx + c * dz + 2048) >> 12);

			if (ahead > abs(side) && civ->ctrlState == 4)
				CopIndicator(side * 266 / ahead, 262128 / (ahead / 8 + 600));
		}
	}
}

// [0x506b2a] [D]
// A red triangle at the bottom of the screen at xpos (-256 .. 256), brightness strength.
void CopIndicator(int xpos, int strength)
{
	float x_mid;
	float y;
	int x;
	int half_width;
	int height;
	GrVertex tip, right, left;

	if (strength > 255)
		strength = 255;

	x_mid = (float)(int)((int)((float)(xpos + 256) * ((float)display_width / 512.0f)) - (display_width >> 1));
	x = (display_width >> 1) + (int)(x_mid * screen_scale);

	half_width = (int)(22.5f * ((float)display_width / 640.0f) * screen_scale);
	height = (int)(45.0f * ((float)display_height / 480.0f) * screen_scale);

	y = (float)(display_height >> 1) * screen_scale + (float)(display_height >> 1);

	tip.x = (float)x;
	tip.y = y - (float)height;
	right.x = (float)(half_width + x);
	right.y = y;
	left.x = (float)(x - half_width);
	left.y = y;

	if (using_glide)
	{
		tip.oow = 1.0f;
		right.oow = 1.0f;
		left.oow = 1.0f;
	}
	else
	{
		tip.oow = 0.015625f;
		right.oow = 0.015625f;
		left.oow = 0.015625f;
	}

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

	left.r = right.r = tip.r = (float)strength;
	tip.b = tip.g = 0.0f;
	right.b = right.g = 0.0f;
	left.b = left.g = 0.0f;

	guDrawTriangleWithClip(&tip, &right, &left);
}

// [0x506dcd] [D]
// The overhead mini map: the map texture rotated with the player (flashing when the cops know
// where the player is), the player, targets, cops and the compass.
void DisplayOverheadMap(void)
{
	float s, c;
	float r;
	float x0, y0, x1, y1, x2, y2, x3, y3;
	float level;
	float col;
	int half;
	int size;
	int border;
	int left, top, right, bottom;
	GrVertex v0, v1, v2, v3;

	SwapPalette(23);

	if (!gMapTexture || !display_map)
		return;

	GrabMapGlide(0);

	half = float_to_int(0.1f * (float)display_width * screen_scale);
	r = 0.14218751f * (float)display_width * screen_scale;
	size = half * 2;

	if (screen_scale >= 0.5f)
		border = 2;
	else
		border = 1;

	left = (int)(overlay_map_x_centre - (float)half);
	top = (int)(overlay_map_y_centre - (float)half);
	right = (int)(overlay_map_x_centre + (float)half);
	bottom = (int)(overlay_map_y_centre + (float)half);

	DrawQuad((float)(left - border), (float)(top - border), (float)(size + border * 2), (float)border, 0x111111);
	DrawQuad((float)(left - border), (float)bottom, (float)(size + border * 2), (float)border, 0x111111);
	DrawQuad((float)(left - border), (float)top, (float)border, (float)size, 0x111111);
	DrawQuad((float)right, (float)top, (float)border, (float)size, 0x111111);

	if (gMissionRecording)
	{
		s = fsin(-(car_data[65].rotation.vy + 2048) & 0xFFF);
		c = fcos(-(car_data[65].rotation.vy + 2048) & 0xFFF);
	}
	else
	{
		s = fsin(-(car_data[0].rotation.vy + 2048) & 0xFFF);
		c = fcos(-(car_data[0].rotation.vy + 2048) & 0xFFF);
	}

	if (gCheatFlags.OverheadView)
	{
		s = fsintab[2048];
		c = fsintab[3072];
	}

	// the corners (-r -r), (r -r), (r r), (-r r) rotated
	x0 = c * -r - s * -r;
	y0 = s * -r + c * -r;
	x1 = c * r - s * -r;
	y1 = s * r + c * -r;
	x2 = c * r - s * r;
	y2 = s * r + c * r;
	x3 = c * -r - s * r;
	y3 = s * -r + c * r;

	v0.x = overlay_map_x_centre + x0;
	v0.y = overlay_map_y_centre + y0;
	v1.x = overlay_map_x_centre + x1;
	v1.y = overlay_map_y_centre + y1;
	v2.x = overlay_map_x_centre + x2;
	v2.y = overlay_map_y_centre + y2;
	v3.x = overlay_map_x_centre + x3;
	v3.y = overlay_map_y_centre + y3;

	if (using_glide)
	{
		v0.oow = 1.0f;
		v1.oow = 1.0f;
		v2.oow = 1.0f;
		v3.oow = 1.0f;
	}
	else
	{
		v3.oow = 0.015625f;
		v2.oow = 0.015625f;
		v1.oow = 0.015625f;
		v0.oow = 0.015625f;
	}

	if (last_state != TEXT3)
	{
		grColorCombine(GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_LOCAL, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_TEXTURE, FXFALSE);
		last_state = TEXT3;
	}

	if (last_trans_on)
	{
		grAlphaBlendFunction(GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_ONE, GR_BLEND_ZERO);
		last_trans_on = 0;
	}

	if (FelonyRating >= 682 && player_position_known > 0)
	{
		// flashes blue, then red
		if (map_flash_timer <= 64)
		{
			level = (float)((map_flash_timer >> 1) + 32) / 64.0f;

			if (gNight)
			{
				col = level * 155.0f;
				v3.b = v2.b = v1.b = v0.b = 155.0f;
			}
			else
			{
				col = level * 255.0f;
				v3.b = v2.b = v1.b = v0.b = 255.0f;
			}

			v3.r = v2.r = v1.r = v0.r = col;
			v3.g = v2.g = v1.g = v0.g = col;
		}
		else
		{
			level = (float)(((map_flash_timer - 64) >> 1) + 32) / 64.0f;

			if (gNight)
			{
				col = level * 155.0f;
				v3.r = v2.r = v1.r = v0.r = 155.0f;
			}
			else
			{
				col = level * 255.0f;
				v3.r = v2.r = v1.r = v0.r = 255.0f;
			}

			v3.g = v2.g = v1.g = v0.g = col;
			v3.b = v2.b = v1.b = v0.b = col;
		}
	}
	else if (gNight)
	{
		v3.r = v2.r = v1.r = v0.r = 155.0f;
		v3.g = v2.g = v1.g = v0.g = 155.0f;
		v3.b = v2.b = v1.b = v0.b = 155.0f;
	}
	else
	{
		v3.r = v2.r = v1.r = v0.r = 255.0f;
		v3.g = v2.g = v1.g = v0.g = 255.0f;
		v3.b = v2.b = v1.b = v0.b = 255.0f;
	}

	if (!GamePaused)
		map_flash_timer = (map_flash_timer - 8) & 0x7F;

	// **** DrawMiniMap ****
	if (gMapTexture->tset != last_tset)
	{
		last_tset = gMapTexture->tset;
		grTexSource(GR_TMU0, TextureSlots[gMapTexture->tset].texture_ram_address, GR_MIPMAPLEVELMASK_BOTH, TextureSlots[gMapTexture->tset].TInfo);
	}

	v0.tmuvtx[0].sow = (float)(gMapTexture->w + gMapTexture->u) * v0.oow;
	v0.tmuvtx[0].tow = (float)(gMapTexture->h + gMapTexture->v) * v0.oow;
	v1.tmuvtx[0].sow = (float)gMapTexture->u * v1.oow;
	v1.tmuvtx[0].tow = (float)(gMapTexture->h + gMapTexture->v) * v1.oow;
	v2.tmuvtx[0].sow = (float)gMapTexture->u * v2.oow;
	v2.tmuvtx[0].tow = (float)gMapTexture->v * v2.oow;
	v3.tmuvtx[0].sow = (float)(gMapTexture->w + gMapTexture->u) * v3.oow;
	v3.tmuvtx[0].tow = (float)gMapTexture->v * v3.oow;

	grClipWindow(left, top, right, bottom);

	guDrawTriangleWithClip(&v0, &v1, &v2);
	guDrawTriangleWithClip(&v2, &v3, &v0);
	// ***********************

	SwapPalette(0);

	DrawCentreMapDot();
	DrawMapTargets();
	DrawCopsOnOverlayMap();
	sub_5083EC();
	DrawCompass();

	grClipWindow(float_to_int(clipxmin), float_to_int(clipymin), float_to_int(clipxmax), float_to_int(clipymax));
}

// [0x507c74] [D]
// Blue dots (and their view cones) for the cops near the player on the overhead map.
void DrawCopsOnOverlayMap(void)
{
	CIV_STATE *civ;
	CAR_DATA *cp;
	float size;
	float half;
	float s, c;
	float fx, fz;
	int dx, dz;
	int dist;
	int i;

	if (!gMapTexture || !display_map)
		return;

	size = 0.003125f * (float)display_width * screen_scale;
	half = size * 0.5f;

	if (size < 1.0f)
		return;

	s = fsintab[-car_data[0].rotation.vy & 0xFFF];
	c = fsintab[(-car_data[0].rotation.vy & 0xFFF) + 1024];

	if (gCheatFlags.OverheadView)
	{
		s = fsintab[0];
		c = fsintab[1024];
	}

	civ = civilian_ai_data;

	for (i = 0; i < 64; i++, civ++)
	{
		if (civ->ctrlState && civ->IsCopCar && civ->ctrlState != 1)
		{
			cp = &civ_ai_data_pt[civ->id];

			dx = cp->location.vx - car_data[0].location.vx;
			dz = car_data[0].location.vz - cp->location.vz;

			if (abs(dx) >= abs(dz))
				dist = abs(dz) / 2 + abs(dx);
			else
				dist = abs(dx) / 2 + abs(dz);

			if (dist < 45000)
			{
				fx = (float)dx * c - (float)dz * s;
				fz = (float)dx * s + (float)dz * c;

				fx = fx * road_recip * map_factor + overlay_map_x_centre - half;
				fz = fz * road_recip * map_factor + overlay_map_y_centre - half;

				DrawCopFrustrumOnOverlayMap((float)cp->location.vx, (float)cp->location.vz, i);
				DrawQuad(fx, fz, size, size, 0xFF);
			}
		}
	}
}

// [0x507f4b] [D]
// The view cone of a cop on the overhead map; red when the cops have seen the player.
void DrawCopFrustrumOnOverlayMap(float x, float z, int civ)
{
	CIV_STATE *cs;
	float dist;
	float x1, z1, x2, z2;
	float cx, cz;
	int alerted;
	int angle;
	GrVertex p0, p1, p2;

	alerted = 0;

	if (FelonyRating >= 682 && (CopsCanSeePlayer || player_position_known > 0))
		alerted = 1;

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

	cs = &civilian_ai_data[civ];

	dist = (float)CopSightData.frontViewDistance;

	angle = (-civ_ai_data_pt[cs->id].direction - CopSightData.frontViewAngle / 2 + 1024) & 0xFFF;
	x1 = fsintab[angle + 1024] * dist + x;
	z1 = fsintab[angle] * dist + z;

	angle = (CopSightData.frontViewAngle / 2 - civ_ai_data_pt[cs->id].direction + 1024) & 0xFFF;
	x2 = fsintab[angle + 1024] * dist + x;
	z2 = fsintab[angle] * dist + z;

	ConvertWorldToMap(x1, z1, &x1, &z1);
	ConvertWorldToMap(x2, z2, &x2, &z2);
	ConvertWorldToMap(x, z, &cx, &cz);

	p0.x = x1;
	p0.y = z1;
	p1.x = x2;
	p1.y = z2;
	p2.x = cx;
	p2.y = cz;

	p2.z = 0.0f;
	p1.z = 0.0f;
	p0.z = 0.0f;

	if (alerted)
	{
		p2.r = p1.r = p0.r = 80.0f;
		p0.b = p0.g = 0.0f;
		p1.b = p1.g = 0.0f;
		p2.b = p2.g = 0.0f;
	}
	else
	{
		p0.b = p0.g = p0.r = 80.0f;
		p1.b = p1.g = p1.r = 80.0f;
		p2.b = p2.g = p2.r = 80.0f;
	}

	if (using_glide)
	{
		p2.oow = 1.0f;
		p1.oow = 1.0f;
		p0.oow = 1.0f;
	}
	else
	{
		p2.oow = 0.015625f;
		p1.oow = 0.015625f;
		p0.oow = 0.015625f;
	}

	guDrawTriangleWithClip(&p0, &p1, &p2);
}

// [0x5082e9] [D]
// World x z to a position on the overhead map.
void ConvertWorldToMap(float x, float z, float *map_x, float *map_y)
{
	float s, c;
	float dx, dz;
	float rx, rz;

	s = fsintab[-car_data[0].rotation.vy & 0xFFF];
	c = fsintab[(-car_data[0].rotation.vy & 0xFFF) + 1024];

	if (gCheatFlags.OverheadView)
	{
		s = fsintab[0];
		c = fsintab[1024];
	}

	dx = (x + 750.0f) - ((float)car_data[0].location.vx + 750.0f);
	dz = (-z - 750.0f) - ((float)-car_data[0].location.vz - 750.0f);

	rx = dx * c - dz * s;
	rz = dx * s + dz * c;

	*map_x = rx * road_recip * map_factor + overlay_map_x_centre;
	*map_y = rz * road_recip * map_factor + overlay_map_y_centre;
}

// [0x5083ec] [D]
// Points the overhead map at the nearest target.
void sub_5083EC(void)
{
	MISSION_TARGET *target;
	float dx, dz;
	float dist;
	float nearest_dist;
	int nearest;
	int i;

	nearest = -1;
	target = MissionTargets;

	for (i = 0; i < 10; i++, target++)
	{
		if ((target->flags & 1) && !(target->flags & 0x40))
		{
			dx = (float)(target->position.vx - car_data[0].location.vx);
			dz = (float)(target->position.vz - car_data[0].location.vz);

			if (nearest == -1)
			{
				nearest = i;
				nearest_dist = dx * dx + dz * dz;
			}
			else
			{
				dist = dx * dx + dz * dz;

				if (dist < nearest_dist)
				{
					nearest = i;
					nearest_dist = dx * dx + dz * dz;
				}
			}
		}
	}

	if (nearest != -1)
		sub_5084C4(nearest);
}

// [0x5084c4] [D]
// A beam from the player towards the target on the overhead map; cut off at the map's edge when
// the target is further away.
void sub_5084C4(int target)
{
	float x, z;
	int col;
	int len;
	int dist;
	IVECTOR pts[4];
	GrVertex p0, p1, p2, p3;

	col = 128;

	x = (float)MissionTargets[target].position.vx;
	z = (float)MissionTargets[target].position.vz;

	p0.r = p0.g = p0.b = p1.r = p1.g = p1.b = (float)(col / 4);
	p2.r = p2.g = p2.b = p3.r = p3.g = p3.b = (float)col;

	if (using_glide)
	{
		p3.oow = 1.0f;
		p2.oow = 1.0f;
		p1.oow = 1.0f;
		p0.oow = 1.0f;
	}
	else
	{
		p3.oow = 0.015625f;
		p2.oow = 0.015625f;
		p1.oow = 0.015625f;
		p0.oow = 0.015625f;
	}

	// pts[2]: the target, pts[0] pts[1]: the base of the beam across the direction
	pts[2].vx = (int)(x - (float)car_data[0].location.vx);
	pts[2].vz = (int)(z - (float)car_data[0].location.vz);

	pts[0].vx = (pts[2].vz + 2) >> 2;
	pts[0].vz = (2 - pts[2].vx) >> 2;

	if (abs(pts[0].vx) >= abs(pts[0].vz))
		len = abs(pts[0].vz) / 2 + abs(pts[0].vx);
	else
		len = abs(pts[0].vx) / 2 + abs(pts[0].vz);

	if (len / 1500 > 15)
	{
		pts[0].vx = pts[0].vx * 15 / (len / 1500);
		pts[0].vz = pts[0].vz * 15 / (len / 1500);
	}

	pts[1].vx = -pts[0].vx;
	pts[1].vz = -pts[0].vz;

	if (abs(pts[2].vx) >= abs(pts[2].vz))
		dist = abs(pts[2].vz) / 2 + abs(pts[2].vx);
	else
		dist = abs(pts[2].vx) / 2 + abs(pts[2].vz);

	if (last_state)
	{
		grColorCombine(GR_COMBINE_FUNCTION_LOCAL, GR_COMBINE_FACTOR_ZERO, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_CONSTANT, FXFALSE);
		last_state = FLAT3;
	}

	if (dist <= 174000)
	{
		sub_508B27(pts, pts, 3);

		p0.x = (float)pts[0].vx;
		p0.y = (float)pts[0].vz;
		p1.x = (float)pts[1].vx;
		p1.y = (float)pts[1].vz;
		p2.x = (float)pts[2].vx;
		p2.y = (float)pts[2].vz;

		if (using_glide || (hw_settings.device_flags & 2))
		{
			if (last_trans_on != 2)
			{
				grAlphaBlendFunction(GR_BLEND_ZERO, GR_BLEND_ONE_MINUS_SRC_COLOR, GR_BLEND_ONE, GR_BLEND_ZERO);
				last_trans_on = 2;
			}

			guDrawTriangleWithClip(&p0, &p1, &p2);
		}
		else
		{
			if (last_trans_on)
			{
				grAlphaBlendFunction(GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_ONE, GR_BLEND_ZERO);
				last_trans_on = 0;
			}

			grDrawLine(&p0, &p2);
			grDrawLine(&p1, &p2);
			grDrawLine(&p0, &p1);
		}
	}
	else
	{
		len = dist / 1500;

		// (pts[3] is converted too and then overwritten)
		sub_508B27(pts, pts, 4);

		pts[3].vx = (pts[2].vx - pts[1].vx) * 116 / len + pts[1].vx;
		pts[3].vz = (pts[2].vz - pts[1].vz) * 116 / len + pts[1].vz;
		pts[2].vx = (pts[2].vx - pts[0].vx) * 116 / len + pts[0].vx;
		pts[2].vz = (pts[2].vz - pts[0].vz) * 116 / len + pts[0].vz;

		p0.x = (float)pts[0].vx;
		p0.y = (float)pts[0].vz;
		p1.x = (float)pts[1].vx;
		p1.y = (float)pts[1].vz;
		p2.x = (float)pts[2].vx;
		p2.y = (float)pts[2].vz;
		p3.x = (float)pts[3].vx;
		p3.y = (float)pts[3].vz;

		if (using_glide || (hw_settings.device_flags & 2))
		{
			if (last_trans_on != 2)
			{
				grAlphaBlendFunction(GR_BLEND_ZERO, GR_BLEND_ONE_MINUS_SRC_COLOR, GR_BLEND_ONE, GR_BLEND_ZERO);
				last_trans_on = 2;
			}

			guDrawTriangleWithClip(&p0, &p1, &p2);
			guDrawTriangleWithClip(&p3, &p1, &p2);
		}
		else
		{
			if (last_trans_on)
			{
				grAlphaBlendFunction(GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_ONE, GR_BLEND_ZERO);
				last_trans_on = 0;
			}

			grDrawLine(&p0, &p2);
			grDrawLine(&p1, &p3);
			grDrawLine(&p0, &p1);
		}
	}
}

// [0x508b27] [D]
// World offsets from the player (x z) to overhead map positions.
void sub_508B27(IVECTOR *pos, IVECTOR *out, int count)
{
	float rs, rc;
	float rx, rz;
	int angle;

	angle = -car_data[PlayerCar].direction & 0xFFF;

	rs = fsintab[angle];
	rc = fsintab[angle + 1024];

	if (gCheatFlags.OverheadView)
	{
		rs = fsintab[0];
		rc = fsintab[1024];
	}

	while (count--)
	{
		rx = (float)(pos->vx / 1500);
		rz = (float)(-pos->vz / 1500);

		out->vx = (int)(rx * rc + rz * -rs);
		out->vz = (int)(rx * rs + rz * rc);

		out->vx = (int)((float)out->vx * map_factor + overlay_map_x_centre);
		out->vz = (int)((float)out->vz * map_factor + overlay_map_y_centre);

		pos++;
		out++;
	}
}

// [0x508c6b] [D]
// The player in the middle of the overhead map.
void DrawCentreMapDot(void)
{
	float w;
	float half;
	float x, y;

	if (!gDotTexture)
		return;

	w = 0.00078125001f * (float)display_width * screen_scale;
	half = w * 0.5f;

	x = overlay_map_x_centre - (float)gDotTexture->w * half;
	y = overlay_map_y_centre - (float)gDotTexture->h * half;

	DrawTexturedQuad(x, y, w, w, gDotTexture, 255, 255, 255, 2);
}

// [0x508d1f] [D]
void DrawMapTargets(void)
{
	MISSION_TARGET *target;
	int i;

	if (gMissionReplaying)
	{
		DrawMapDot(&car_data[65].location.vx, 0, 255, 0, 0, gMapDotScale);
		return;
	}

	target = MissionTargets;

	for (i = 0; i < 10; i++, target++)
	{
		if ((target->flags & 1) && !(target->flags & 0x10))
			DrawMapDot(&target->position.vx, 0, 255, 0, 0, gMapDotScale);
	}
}

// [0x508db6] [D]
// A pulsing dot on the overhead map at a world position (x y z), if it is on the map.
void DrawMapDot(int *pos, int unused, u_char r, u_char g, u_char b, float scale)
{
	float s, c;
	float w;
	float half;
	float x, y;
	int dx, dy;
	int rx, ry;
	int px, py;

	if (!gDotTexture)
		return;

	dy = map_bmp_height / 2 + (750 - pos[2]) / 1500 - (map_bmp_height / 2 + (750 - car_data[0].location.vz) / 1500);
	dx = map_bmp_width / 2 + (pos[0] + 750) / 1500 - (map_bmp_width / 2 + (car_data[0].location.vx + 750) / 1500);

	if (dy >= 62 || dy <= -62 || dx <= -62 || dx >= 62)
		return;

	s = fsintab[car_data[0].rotation.vy];
	c = fsintab[car_data[0].rotation.vy + 1024];

	if (gCheatFlags.OverheadView)
	{
		s = fsintab[0];
		c = fsintab[1024];
	}

	rx = float_to_int((float)dx * c + (float)dy * s);
	ry = float_to_int((float)dy * c - (float)dx * s);

	px = float_to_int(overlay_map_x_centre) + (int)((float)rx * map_factor);
	py = float_to_int(overlay_map_y_centre) + (int)((float)ry * map_factor);

	w = 0.0015625f * (float)display_width * screen_scale * scale;
	half = w * 0.5f;

	y = (float)(int)(py - (int)((float)gDotTexture->h * half));
	x = (float)(int)(px - (int)((float)gDotTexture->w * half));

	DrawTexturedQuad(x, y, w, w, gDotTexture, 255 - r, 255 - g, 255 - b, 2);
	DrawTexturedQuad(x, y, w, w, gDotTexture, r, g, b, 3);
}

// [0x5090c0] [D]
// The compass needle (north of the level) and the "N" on the overhead map.
void DrawCompass(void)
{
	XYPAIR *dir;
	VECTOR4 n_pos;
	VECTOR4 pts[3];
	float s, c;
	float x, y;
	float tip_x, tip_y;
	float side_x, side_y;
	int direction;
	int i;

	dir = &north[GameLevel];
	direction = car_data[PlayerCar].direction;

	c = fsintab[direction + 1024];
	s = fsintab[direction];

	if (gCheatFlags.OverheadView)
	{
		s = fsintab[0];
		c = fsintab[1024];
	}

	x = (float)dir->x;
	y = (float)dir->y;

	tip_x = (x * c + y * s) * screen_scale;
	tip_y = (y * c - x * s) * screen_scale;

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

	if (gNight)
	{
		compass_to.b = compass_to.g = compass_to.r = 220.0f;
		compass_from.b = compass_from.g = compass_from.r = 220.0f;
	}
	else
	{
		compass_to.b = compass_to.g = compass_to.r = 255.0f;
		compass_from.b = compass_from.g = compass_from.r = 255.0f;
	}

	if (using_glide)
	{
		compass_to.oow = 1.0f;
		compass_from.oow = 1.0f;
	}
	else
	{
		compass_to.oow = 0.015625f;
		compass_from.oow = 0.015625f;
	}

	// the "N" beyond the tip, the tail and the two sides of the arrow head (171 / 1024)
	n_pos.vx = (tip_x * 20.0f + 8.0f) * 0.0625f;
	n_pos.vz = (tip_y * 20.0f + 8.0f) * 0.0625f;

	pts[0].vx = (tip_x + 2.0f) * 0.25f;
	pts[0].vz = (tip_y + 2.0f) * 0.25f;

	side_x = (tip_y - tip_x) * 171.0f * 0.0009765625f;
	side_y = (-tip_y - tip_x) * 171.0f * 0.0009765625f;

	pts[1].vx = side_x + tip_x;
	pts[1].vz = side_y + tip_y;
	pts[2].vx = side_y + tip_x;
	pts[2].vz = -side_x + tip_y;

	n_pos.vx = n_pos.vx + overlay_map_x_centre;
	n_pos.vz = n_pos.vz + overlay_map_y_centre;

	tip_x = tip_x + overlay_map_x_centre;
	tip_y = tip_y + overlay_map_y_centre;

	for (i = 0; i < 3; i++)
	{
		pts[i].vx = pts[i].vx + overlay_map_x_centre;
		pts[i].vz = pts[i].vz + overlay_map_y_centre;
	}

	for (i = 0; i < 3; i++)
	{
		compass_from.x = tip_x;
		compass_from.y = tip_y;
		compass_to.x = pts[i].vx;
		compass_to.y = pts[i].vz;

		grDrawLine(&compass_from, &compass_to);
	}

	DrawN(&n_pos);
}

// [0x509430] [D]
// The "N" of the compass at pos (x z).
void DrawN(VECTOR4 *pos)
{
	XYPAIR *v;
	int x, y;
	int i;

	v = &NVertex[1];

	// NOTE: the first point is not scaled by screen_scale (the others are)
	x = (int)((float)NVertex[0].x + pos->vx);
	y = (int)((float)NVertex[0].y + pos->vz);

	for (i = 0; i < 3; i++, v++)
	{
		compass_from.x = (float)x;
		compass_from.y = (float)y;

		x = (int)((float)v->x * screen_scale + pos->vx);
		y = (int)((float)v->y * screen_scale + pos->vz);

		compass_to.x = (float)x;
		compass_to.y = (float)y;

		grDrawLine(&compass_from, &compass_to);
	}
}

// [0x509511] [D]
// The player (white) and the targets (red) on the full screen map, and the beam to the nearest.
void DrawFullscreenMapTargets(void)
{
	MISSION_TARGET *target;
	float x, z;
	int i;

	x = (float)car_data[0].location.vx;
	z = (float)car_data[0].location.vz;

	SwapPalette(0);

	DrawFullscreenTargetDot(x, z, 255, 255, 255, (float)fabs(gFSMDotScale));

	target = MissionTargets;

	for (i = 0; i < 10; i++, target++)
	{
		if (target->flags & 1)
		{
			x = (float)target->position.vx;
			z = (float)target->position.vz;

			if (!(target->flags & 0x10))
				DrawFullscreenTargetDot(x, z, 255, 0, 0, (float)fabs(gFSMDotScale));
		}
	}

	sub_50996D();
}

// [0x509605] [D]
void DrawFullscreenTargetDot(float x, float z, u_char r, u_char g, u_char b, float scale)
{
	float w;
	float half;

	if (!gDotTexture)
		return;

	w = 0.0015625f * (float)display_width * screen_scale * scale;
	half = w * 0.5f;

	WorldToFullscreen(&x, &z);

	x = x - (float)gDotTexture->w * half;
	z = z - (float)gDotTexture->h * half;

	DrawTexturedQuad(x, z, w, w, gDotTexture, 255 - r, 255 - g, 255 - b, 2);
	DrawTexturedQuad(x, z, w, w, gDotTexture, r, g, b, 3);
}

// [0x509716] [D]
// World x z to the screen position on the full screen map.
void WorldToFullscreen(float *x, float *z)
{
	float mx, my;

	mx = *x;
	my = *z;

	mx = mx / 1500.0f + (float)map_bmp_width / 2.0f;
	my = -my / 1500.0f + (float)map_bmp_height / 2.0f;

	MapToFullscreen(&mx, &my);

	*x = mx;
	*z = my;
}

// [0x509792] [D]
// A position on the map bitmap to the screen: the maps of Miami and New York are turned a quarter,
// San Francisco's half a turn; then the map_adjustments part fills the screen.
void MapToFullscreen(float *x, float *y)
{
	float fx, fy;

	switch (GameLevel)
	{
		case 0:
		case 3:
			*x = *x - (float)map_bmp_width * 0.5f;
			*y = *y - (float)map_bmp_height * 0.5f;

			fx = (float)map_bmp_height * 0.5f + -*y;
			fy = (float)map_bmp_width * 0.5f + *x;
			break;
		case 1:
			fx = (float)map_bmp_width * 0.5f + -(*x - (float)map_bmp_width * 0.5f);
			fy = (float)map_bmp_height * 0.5f + -(*y - (float)map_bmp_height * 0.5f);
			break;
		default:
			fx = *x;
			fy = *y;
			break;
	}

	fx = fx - (float)fs_map_xstart;
	fy = fy - (float)fs_map_ystart;

	fx = 640.0f / (float)fs_map_width * fx;
	fy = 480.0f / (float)fs_map_height * fy;

	*x = ((fx / 640.0f - 0.5f) * screen_scale + 0.5f) * (float)display_width;
	*y = ((fy / 480.0f - 0.5f) * screen_scale + 0.5f) * (float)display_height;
}

// [0x50996d] [D]
// Points the full screen map at the nearest target.
void sub_50996D(void)
{
	MISSION_TARGET *target;
	float dx, dz;
	float dist;
	float nearest_dist;
	int nearest;
	int i;

	nearest = -1;
	target = MissionTargets;

	for (i = 0; i < 10; i++, target++)
	{
		if ((target->flags & 1) && !(target->flags & 0x40))
		{
			dx = (float)(target->position.vx - car_data[0].location.vx);
			dz = (float)(target->position.vz - car_data[0].location.vz);

			if (nearest == -1)
			{
				nearest = i;
				nearest_dist = dx * dx + dz * dz;
			}
			else
			{
				dist = dx * dx + dz * dz;

				if (dist < nearest_dist)
				{
					nearest = i;
					nearest_dist = dx * dx + dz * dz;
				}
			}
		}
	}

	if (nearest != -1)
		DrawFullscreenCentreDot(nearest);
}

// [0x509a45] [D]
// A beam from the player towards the target on the full screen map (up to 50 pixels wide at the
// player).
void DrawFullscreenCentreDot(int target)
{
	VECTOR4 player;
	VECTOR4 pos;
	IVECTOR pts[3];
	float dist;
	float len;
	float col;
	int dx, dy;
	int angle;
	GrVertex p0, p1, p2;

	if (using_glide)
	{
		p2.oow = 1.0f;
		p1.oow = 1.0f;
		p0.oow = 1.0f;
	}
	else
	{
		p2.oow = 0.015625f;
		p1.oow = 0.015625f;
		p0.oow = 0.015625f;
	}

	// NOTE: the screen positions are kept in vx / vz of a VECTOR4 (sub_509F02 reads them so)
	player.vx = (float)car_data[0].location.vx;
	player.vz = (float)car_data[0].location.vz;
	pos.vx = (float)MissionTargets[target].position.vx;
	pos.vz = (float)MissionTargets[target].position.vz;

	WorldToFullscreen(&player.vx, &player.vz);
	WorldToFullscreen(&pos.vx, &pos.vz);

	dx = (int)(pos.vx - player.vx);
	dy = (int)(pos.vz - player.vz);

	dist = (float)sqrt((double)(dy * dy + dx * dx));
	angle = Arc_Tan(dy, dx) & 0xFFF;

	switch (GameLevel)
	{
		case 0:
		case 2:
		case 3:
			angle = (angle - 1024) & 0xFFF;
			break;
		case 1:
			angle = (angle + 3072) & 0xFFF;
			break;
	}

	dist = dist / screen_scale;

	len = dist;

	if (dist > 50.0f)
		len = 50.0f;

	pts[0].vx = (int)(-15.0f * len / 50.0f);
	pts[0].vz = 0;
	pts[1].vx = (int)(15.0f * len / 50.0f);
	pts[1].vz = 0;
	pts[2].vx = 0;
	pts[2].vz = float_to_int(dist);

	sub_509F02(pts, pts, 3, &player, angle);

	p0.x = (float)pts[0].vx;
	p0.y = (float)pts[0].vz;
	p1.x = (float)pts[1].vx;
	p1.y = (float)pts[1].vz;
	p2.x = (float)pts[2].vx;
	p2.y = (float)pts[2].vz;

	if (last_state)
	{
		grColorCombine(GR_COMBINE_FUNCTION_LOCAL, GR_COMBINE_FACTOR_ZERO, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_CONSTANT, FXFALSE);
		last_state = FLAT3;
	}

	if (using_glide || (hw_settings.device_flags & 2))
	{
		col = 192.0f;

		p1.r = p1.g = p1.b = p0.r = p0.g = p0.b = (float)(192 / 4);
		p2.r = p2.g = p2.b = col;

		if (last_trans_on != 2)
		{
			grAlphaBlendFunction(GR_BLEND_ZERO, GR_BLEND_ONE_MINUS_SRC_COLOR, GR_BLEND_ONE, GR_BLEND_ZERO);
			last_trans_on = 2;
		}

		guDrawTriangleWithClip(&p0, &p1, &p2);
	}
	else
	{
		col = 128.0f;

		p1.r = p1.g = p1.b = p0.r = p0.g = p0.b = (float)(128 / 4);
		p2.r = p2.g = p2.b = col;

		if (last_trans_on)
		{
			grAlphaBlendFunction(GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_ONE, GR_BLEND_ZERO);
			last_trans_on = 0;
		}

		grDrawLine(&p0, &p2);
		grDrawLine(&p0, &p1);
		grDrawLine(&p1, &p2);
	}
}

// [0x509f02] [D]
// Rotates offsets (x z) by angle and puts them on the screen at centre (vx vz).
void sub_509F02(IVECTOR *pos, IVECTOR *out, int count, VECTOR4 *centre, int angle)
{
	float s, c;
	float x, z;

	s = fsintab[angle];
	c = fsintab[angle + 1024];

	if (gCheatFlags.OverheadView)
	{
		s = fsintab[0];
		c = fsintab[1024];
	}

	while (count--)
	{
		x = (float)pos->vx;
		z = (float)pos->vz;

		out->vx = (int)(x * c + z * -s);
		out->vz = (int)(x * s + z * c);

		out->vx = (int)((float)out->vx * screen_scale + centre->vx);
		out->vz = (int)((float)out->vz * screen_scale + centre->vz);

		pos++;
		out++;
	}
}

// [0x50a00a] [D]
// Draws a texture page (u0 v0 - u1 v1) of the map at x y w h of the map bitmap on the full screen
// map.
void DrawMapTPage(float x, float y, float w, float h, float u0, float v0, float u1, float v1, int tset)
{
	float oow;
	GrVertex verts[4];

	if (using_glide)
		oow = 1.0f;
	else
		oow = 0.015625f;

	u0 = u0 + 0.5f;
	v0 = v0 + 0.5f;
	u1 = u1 - 0.5f;
	v1 = v1 - 0.5f;

	if (u1 > 255.0f)
		u1 = 255.0f;

	if (v1 > 255.0f)
		v1 = 255.0f;

	verts[2].x = x;
	verts[2].y = y;
	verts[2].oow = oow;
	verts[2].r = 255.0f;
	verts[2].g = 255.0f;
	verts[2].b = 255.0f;
	verts[2].a = 255.0f;
	verts[2].tmuvtx[0].sow = u0 * oow;
	verts[2].tmuvtx[0].tow = v0 * oow;

	verts[3].x = x + w;
	verts[3].y = y;
	verts[3].oow = oow;
	verts[3].r = 255.0f;
	verts[3].g = 255.0f;
	verts[3].b = 255.0f;
	verts[3].a = 255.0f;
	verts[3].tmuvtx[0].sow = u1 * oow;
	verts[3].tmuvtx[0].tow = verts[2].tmuvtx[0].tow;

	verts[0].x = verts[3].x;
	verts[0].y = y + h;
	verts[0].oow = oow;
	verts[0].r = 255.0f;
	verts[0].g = 255.0f;
	verts[0].b = 255.0f;
	verts[0].a = 255.0f;
	verts[0].tmuvtx[0].sow = verts[3].tmuvtx[0].sow;
	verts[0].tmuvtx[0].tow = v1 * oow;

	verts[1].x = x;
	verts[1].y = verts[0].y;
	verts[1].oow = oow;
	verts[1].r = 255.0f;
	verts[1].g = 255.0f;
	verts[1].b = 255.0f;
	verts[1].a = 255.0f;
	verts[1].tmuvtx[0].sow = verts[2].tmuvtx[0].sow;
	verts[1].tmuvtx[0].tow = verts[0].tmuvtx[0].tow;

	if (tset != last_tset)
	{
		last_tset = tset;
		grTexSource(GR_TMU0, TextureSlots[tset].texture_ram_address, GR_MIPMAPLEVELMASK_BOTH, TextureSlots[tset].TInfo);
	}

	if (last_state != TEXT3)
	{
		grColorCombine(GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_LOCAL, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_TEXTURE, FXFALSE);
		last_state = TEXT3;
	}

	if (last_trans_on)
	{
		grAlphaBlendFunction(GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_ONE, GR_BLEND_ZERO);
		last_trans_on = 0;
	}

	MapToFullscreen(&verts[0].x, &verts[0].y);
	MapToFullscreen(&verts[1].x, &verts[1].y);
	MapToFullscreen(&verts[2].x, &verts[2].y);
	MapToFullscreen(&verts[3].x, &verts[3].y);

	grDrawPolygonVertexList(4, verts);
}
