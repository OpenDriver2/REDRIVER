/*
 * pres.c - text presentation, fonts and language strings
 *
 * Rebuilt from Driver_sym2_94.i64 (IDA folder "pres.c").
 */

#include <stdio.h>

#include "dr_globals.h"
#include "pres.h"
#include "file.h"
#include "general.h"
#include "frontend/font.h"
#include "Engine/3dfx/gfxinit.h"
#include "frontend/gvars.h"

#define MAX_LANGUAGE_TEXT	350		// NOTE: known constant

void InitStringLanguage(char *filename, int mission);

// initialised data (0x577d1c)
char *GameLanguageNames[] = {
	"ENG",
	"FRE",
	"GER",
	"SPA",
	"ITA",
	"USA",
};

// [A]
// Picks the language of the game from the LANG file next to it: the first name of
// GameLanguageNames that has a LANG.<name>, USA when there is none (the language of this release).
int DetectGameLanguage(void)
{
	char filename[256];
	FILE *f;
	int i;

	for (i = 0; i < 6; i++)
	{
		sprintf(filename, "LANG.%s", GameLanguageNames[i]);

		f = fopen(filename, "rb");

		if (f)
		{
			fclose(f);
			return i;
		}
	}

	return 5;
}

// NOTE: bss globals in the linker's common area; defined here next to their users
char *gMissionStringBuffer;
char *gLangStringBuffer;
char *gMissionStrings[MAX_LANGUAGE_TEXT];
char *gLanguageStrings[MAX_LANGUAGE_TEXT];

RGB gFontColour;
RGB gFontFadeColour;
int gFontTSet;
int gFontFadeTarget;
int gFontFadeTick;

// [0x518f80] [D]
// NOTE: REDRIVER2 dr2locale.c has a port of this function (different file names)
void InitStringMng(void)
{
	char filename[256];

	if (gUserLanguage < 0 || gUserLanguage >= 6)
		gUserLanguage = 0;

	sprintf(filename, "LANG.%s", GameLanguageNames[gUserLanguage]);
	InitStringLanguage(filename, 0);

	sprintf(filename, "MISSION.%s", GameLanguageNames[gUserLanguage]);
	InitStringLanguage(filename, 1);
}

// [0x51900e] [D]
void DeinitStringMng(void)
{
	if (gLangStringBuffer)
	{
		DebugFree(gLangStringBuffer);
		gLangStringBuffer = NULL;
	}

	if (gMissionStringBuffer)
	{
		DebugFree(gMissionStringBuffer);
		gMissionStringBuffer = NULL;
	}
}

// [0x519056] [D]
void InitFontDrawing(void)
{
	gFontTSet = LoadFont("FONT", 0, -1, 5, 24);
	LoadFont("BIG", 1, -1, 5, 24);
	InitTimerFont(1);
}

// [0x519094] [D]
void SetTextColour(u_char r, u_char g, u_char b)
{
	gFontColour.r = r;
	gFontColour.g = g;
	gFontColour.b = b;
}

// [0x5190b3] [D]
// Writes text at x y of the display (font 0, mode 4), scaled by xscale yscale and screen_scale
// about the screen centre.
// NOTE: unreferenced (only its thunk is left)
void sub_5190B3(float x, float y, float xscale, float yscale, char *text)
{
	float xmid;
	float ymid;

	xmid = (float)(display_width >> 1);
	ymid = (float)(display_height >> 1);

	x = (x - xmid) * screen_scale + xmid;
	y = (y - ymid) * screen_scale + ymid;

	PrintReset();
	PrintScale(xscale * screen_scale, yscale * screen_scale);
	PrintColour(gFontColour.r, gFontColour.g, gFontColour.b);
	PrintAt(x, y, 0, 4);
	PrintText(text);
}

// [0x51919e] [D]
// Writes text centred on the screen at y of the display (font 0, mode 5).
// NOTE: unreferenced (only its thunk is left)
void sub_51919E(float y, float xscale, float yscale, char *text)
{
	float ymid;

	ymid = (float)(display_height >> 1);
	y = (y - ymid) * screen_scale + ymid;

	PrintReset();
	PrintScale(xscale * screen_scale, yscale * screen_scale);
	PrintColour(gFontColour.r, gFontColour.g, gFontColour.b);
	PrintAt((float)(display_width >> 1), y, 0, 5);
	PrintText(text);
}

// [0x51925a] [D]
// Starts fading the text colour to r, g, b over frames (FadeTextToFadeColour).
void SetFade(u_char r, u_char g, u_char b, int frames)
{
	gFontFadeColour.r = r;
	gFontFadeColour.g = g;
	gFontFadeColour.b = b;

	gFontFadeTarget = frames;
	gFontFadeTick = 0;
}

// [0x51928b] [D]
// One step of the text colour fade; 1 when it is done.
int FadeTextToFadeColour(void)
{
	if (gFontFadeTick == gFontFadeTarget)
	{
		SetTextColour(gFontFadeColour.r, gFontFadeColour.g, gFontFadeColour.b);
		return 1;
	}

	gFontColour.r += gFontFadeTick * (gFontFadeColour.r - gFontColour.r) / gFontFadeTarget;
	gFontColour.g += gFontFadeTick * (gFontFadeColour.g - gFontColour.g) / gFontFadeTarget;
	gFontColour.b += gFontFadeTick * (gFontFadeColour.b - gFontColour.b) / gFontFadeTarget;

	gFontFadeTick++;

	return 0;
}

// [0x51936a] [D] [A]
// Writes text at x y of 640x480 in gFontColour; type -1 the shadow (mode 16), 1 mode 4, else 0.
// NOTE: [A] wider than 4:3 the HUD keeps its proportions, see wide_aspect
void WriteCustomScaledString(float x, float y, char *text, char font, int type)
{
	float width;
	float height;
	float xscale;
	float yscale;
	float xmid;
	float ymid;

	width = (float)display_width;
	xscale = HUD_WIDTH / 640.0f;

	height = (float)display_height;
	yscale = height / 480.0f;

	xmid = width / 2.0f;
	ymid = height / 2.0f;

	x = GetScaledXCoord(x);
	y = (y * yscale - ymid) * screen_scale + ymid;

	PrintReset();
	PrintScale(xscale * screen_scale, yscale * screen_scale);
	PrintColour(gFontColour.r, gFontColour.g, gFontColour.b);

	if (type == -1)
		PrintAt(x, y, font, 16);
	else if (type == 1)
		PrintAt(x, y, font, 4);
	else
		PrintAt(x, y, font, 0);

	PrintText(text);
}

// [0x51949b] [D] [A]
// Writes text centred on the screen at y of 480 in gFontColour; type -1 the shadow (mode 17),
// 1 mode 5, else 1.
// NOTE: unreferenced (only its thunk is left); IDA names the first argument x, but it is y
// NOTE: [A] wider than 4:3 the HUD keeps its proportions, see wide_aspect
void WriteCenteredScaledString(float y, char *text, char font, int type)
{
	float width;
	float height;
	float xscale;
	float yscale;
	float xmid;
	float ymid;

	width = (float)display_width;
	xscale = HUD_WIDTH / 640.0f;

	height = (float)display_height;
	yscale = height / 480.0f;

	xmid = width / 2.0f;
	ymid = height / 2.0f;

	y = (y * yscale - ymid) * screen_scale + ymid;

	PrintReset();
	PrintScale(xscale * screen_scale, yscale * screen_scale);
	PrintColour(gFontColour.r, gFontColour.g, gFontColour.b);

	if (type == -1)
		PrintAt(xmid, y, font, 17);
	else if (type == 1)
		PrintAt(xmid, y, font, 5);
	else
		PrintAt(xmid, y, font, 1);

	PrintText(text);
}

// [0x5195b7] [D] [A]
// Writes text at x, y of the 640x480 screen, scaled to the display and screen_scale (about
// the screen centre).
// NOTE: [A] wider than 4:3 the HUD keeps its proportions, see wide_aspect
void WriteScaledStringWithMode(float x, float y, char *text, char font, char mode)
{
	float width;
	float height;
	float xscale;
	float yscale;
	float xmid;
	float ymid;

	width = (float)display_width;
	xscale = HUD_WIDTH / 640.0f;

	height = (float)display_height;
	yscale = height / 480.0f;

	xmid = width / 2.0f;
	ymid = height / 2.0f;

	x = GetScaledXCoord(x);
	y = (y * yscale - ymid) * screen_scale + ymid;

	PrintReset();
	PrintScale(xscale * screen_scale, yscale * screen_scale);
	PrintColour(gFontColour.r, gFontColour.g, gFontColour.b);
	PrintAt(x, y, font, mode);
	PrintText(text);
}

// How far a HUD item at x of 640 moves, to stay at its side of a screen wider than 4:3
static float WideHudShift(float x)
{
	float shift;

	if (!wide_hud_edges)
		return 0.0f;

	shift = ((float)display_width - HUD_WIDTH) / 2.0f;

	if (x < 213.0f)
		return -shift;

	return x > 426.0f ? shift : 0.0f;
}

// [0x5196a6] [D] [A]
// x of 640 to the screen.
// NOTE: [A] wider than 4:3 the HUD keeps its proportions, see wide_aspect
float GetScaledXCoord(float x)
{
	float width;
	float xmid;

	width = (float)display_width;
	xmid = width / 2.0f;

	return ((x - 320.0f) * (HUD_WIDTH / 640.0f) + WideHudShift(x)) * screen_scale + xmid;
}

// [0x5196f2] [D]
// y of 480 to the screen.
float GetScaledYCoord(float y)
{
	float height;
	float ymid;

	height = (float)display_height;
	ymid = height / 2.0f;

	return (y * (height / 480.0f) - ymid) * screen_scale + ymid;
}

// [0x51973e] [D]
// Writes text in r, g, b over a white drop shadow (offset 2, 3 for the big font).
void WriteDropShadowedString(int x, int y, char *string, int type, char mode, byte r, byte g, byte b)
{
	int offset;

	if (type)
		offset = 3;
	else
		offset = 2;

	SetTextColour(255, 255, 255);

	WriteScaledStringWithMode((float)x, (float)y, string, type, mode | 0x10);
	WriteScaledStringWithMode((float)(offset + x), (float)(offset + y), string, type, mode | 0x10);

	SetTextColour(r, g, b);
	WriteScaledStringWithMode((float)x, (float)y, string, type, mode | 4);
}

// [0x51981a] [D]
// Splits a loaded LANG / MISSION file into MAX_LANGUAGE_TEXT lines (in place).
void InitStringLanguage(char *filename, int mission)
{
	char *buffer;
	char *end;
	int i;
	int size;

	size = 0;

	if (mission)
	{
		gMissionStringBuffer = (char *)FileReadGetSize(filename, &size);
		buffer = gMissionStringBuffer;
	}
	else
	{
		gLangStringBuffer = (char *)FileReadGetSize(filename, &size);
		buffer = gLangStringBuffer;
	}

	for (i = 0; i < MAX_LANGUAGE_TEXT; i++)
	{
		if (size <= 0)
		{
			if (mission)
				gMissionStrings[i] = "Mission.? missing!";
			else
				gLanguageStrings[i] = "Lang.? missing!";
		}
		else
		{
			if (mission)
				gMissionStrings[i] = buffer;
			else
				gLanguageStrings[i] = buffer;

			// cut the line at the newline, dropping trailing control characters (CR)
			end = buffer;

			do
			{
				if (*buffer == '\n')
					break;

				if (*buffer >= ' ')
					end = buffer + 1;

				size--;
				buffer++;
			} while (size > 1);

			*end = 0;

			buffer++;
			size--;
		}
	}
}
