/*
 * frontend/gameplay.c - gameplay options: difficulty, auto brake, frame skip, FMV
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "frontend\gameplay.c", 0x40eab0 - 0x40eda8)
 */

#include <stdio.h>

#include "dr_types.h"
#include "dr_globals.h"
#include "pres.h"
#include "shell.h"
#include "input.h"
#include "frontend/frontend.h"
#include "frontend/gameplay.h"

void SetupGameplayOptions(void);
static void difficulty_selfunc(int flag);
static void difficulty_unselfunc(int flag);
static int difficulty_choosefunc(void);
static void autobrake_selfunc(int flag);
static void autobrake_unselfunc(int flag);
static int autobrake_choosefunc(void);
static void frameskip_selfunc(int flag);
static void frameskip_unselfunc(int flag);
static int frameskip_choosefunc(void);
static void allowfmv_selfunc(int flag);
static void allowfmv_unselfunc(int flag);
static int allowfmv_choosefunc(void);

//------------------------------------------------------------------------------------------
// uninitialised data (0x600754)

static char frameskip_text[8];
static int selection;
// NOTE: byte_600760, a bss buffer: the text of the end entry of polyitems
static char poly_end_text[4];

//------------------------------------------------------------------------------------------
// initialised data (0x54cd70)

static FE_POLY polyitems[1] = {
	{ 0, 0, poly_end_text, 4, 0, 0, { 0, 0, 0, 0 }, 0 },
};

static FE_TEXT textitems[10] = {
	{ 40, 150, 0, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 120, 200, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 520, 200, 2, 2, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 120, 225, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 520, 225, 2, 2, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 120, 250, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 520, 250, 2, 2, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 120, 275, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 520, 275, 2, 2, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ -1, -1, 0, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
};

static FE_BUTTON menufunc[5] = {
	{ difficulty_selfunc, difficulty_unselfunc, difficulty_choosefunc, 0, 0, 3, 1 },
	{ autobrake_selfunc, autobrake_unselfunc, autobrake_choosefunc, 1, 1, 0, 2 },
	{ frameskip_selfunc, frameskip_unselfunc, frameskip_choosefunc, 2, 2, 1, 3 },
	{ allowfmv_selfunc, allowfmv_unselfunc, allowfmv_choosefunc, 3, 3, 2, 0 },
	{ NULL, NULL, NULL, 0, 0, 0, 0 },
};

// gLanguageStrings indices: off, on
static int onoff[2] = { 2, 1 };
// NOTE: dword_54CE80; gLanguageStrings indices of the difficulty levels
static int difficulty_names[3] = { 9, 10, 11 };

// [0x40eab0] [D]
int SelectGameplayOptions(void)
{
	int ret;

	SetupGameplayOptions();

	selection = 0;
	ResetMenu(menufunc, 0);
	OpenFrontendScreen(polyitems, textitems);

	for (;;)
	{
		HandleWinMessages();
		ReadInput();

		ret = HandleMenu(menufunc, &selection);

		if (ret != 0)
			break;

		DrawFrontend();
	}

	if (ret != -1)
		return ret;

	CloseFrontendScreen();
	return 0;
}

// [0x40eb2c] [D]
static void difficulty_selfunc(int flag)
{
	textitems[1].font = 3;
	textitems[2].font = 3;
}

// [0x40eb3f] [D]
static void difficulty_unselfunc(int flag)
{
	if (flag == 0)
	{
		textitems[1].font = 2;
		textitems[2].font = 2;
	}
}

// [0x40eb58] [D]
static int difficulty_choosefunc(void)
{
	if (++gDifficulty == 3)
		gDifficulty = 0;

	SetupGameplayOptions();
	return 0;
}

// [0x40eb84] [D]
static void autobrake_selfunc(int flag)
{
	textitems[3].font = 3;
	textitems[4].font = 3;
}

// [0x40eb97] [D]
static void autobrake_unselfunc(int flag)
{
	if (flag == 0)
	{
		textitems[3].font = 2;
		textitems[4].font = 2;
	}
}

// [0x40ebb0] [D]
static int autobrake_choosefunc(void)
{
	gAutoBrake ^= 1;

	SetupGameplayOptions();
	return 0;
}

// [0x40ebc9] [D]
static void frameskip_selfunc(int flag)
{
	textitems[5].font = 3;
	textitems[6].font = 3;
}

// [0x40ebdc] [D]
static void frameskip_unselfunc(int flag)
{
	if (flag == 0)
	{
		textitems[5].font = 2;
		textitems[6].font = 2;
	}
}

// [0x40ebf5] [D]
// Cycles off -> 50 fps -> 25 fps -> ... -> 10 fps -> auto -> off.
static int frameskip_choosefunc(void)
{
	if (gDoInterpolation)
	{
		if (gDoAutoInterpolation == 1)
		{
			gDoAutoInterpolation = 0;
			gDoInterpolation = 0;
		}
		else if (max_frame_skip == 5)
		{
			gDoAutoInterpolation = 1;
		}
		else
		{
			max_frame_skip++;
		}
	}
	else
	{
		gDoInterpolation = 1;
		gDoAutoInterpolation = 0;
		max_frame_skip = 1;
	}

	SetupGameplayOptions();
	return 0;
}

// [0x40ec6b] [D]
static void allowfmv_selfunc(int flag)
{
	textitems[7].font = 3;
	textitems[8].font = 3;
}

// [0x40ec7e] [D]
static void allowfmv_unselfunc(int flag)
{
	if (flag == 0)
	{
		textitems[7].font = 2;
		textitems[8].font = 2;
	}
}

// [0x40ec97] [D]
static int allowfmv_choosefunc(void)
{
	gAllowFMV ^= 1;

	SetupGameplayOptions();
	return 0;
}

// [0x40ecb0] [D]
void SetupGameplayOptions(void)
{
	if (gDoInterpolation)
	{
		if (gDoAutoInterpolation)
			sprintf(frameskip_text, "%s", gLanguageStrings[213]);
		else
			sprintf(frameskip_text, "%dfps", 50 / max_frame_skip);
	}
	else
	{
		sprintf(frameskip_text, "%s", gLanguageStrings[2]);
	}

	textitems[0].text = gLanguageStrings[84];
	textitems[1].text = gLanguageStrings[85];
	textitems[3].text = gLanguageStrings[86];
	textitems[5].text = gLanguageStrings[87];
	textitems[7].text = gLanguageStrings[88];
	textitems[2].text = gLanguageStrings[difficulty_names[gDifficulty]];
	textitems[4].text = gLanguageStrings[onoff[gAutoBrake]];
	textitems[6].text = frameskip_text;
	textitems[8].text = gLanguageStrings[onoff[gAllowFMV]];
}
