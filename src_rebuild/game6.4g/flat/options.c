/*
 * flat/options.c - motel room options menu (detail, audio, controller, gameplay) and its
 * gameplay options
 *
 * Rebuilt from Driver_sym2.i64: IDA folders "motel\gameplayoptions.c=flat\options.c"
 * (0x419f80 - 0x41a2ef) and "motel\options.c=flat\options.c" (0x41a3d0 - 0x41a589).
 * NOTE: two original files in one placeholder: the statics of each keep its IDA prefix
 * (gameplayoptions_, moteloptions_) as their names would clash
 */

#include <stdio.h>

#include "dr_types.h"
#include "dr_globals.h"
#include "pres.h"
#include "shell.h"
#include "input.h"
#include "frontend/config.h"
#include "frontend/controller.h"
#include "frontend/soundoptions.h"
#include "flat/detailoptions.h"
#include "flat/frontend.h"
#include "flat/options.h"

void SetupMotelGameplayOptions(void);
static void gameplayoptions_autobrake_selfunc(int flag);
static void gameplayoptions_autobrake_unselfunc(int flag);
static int gameplayoptions_autobrake_choosefunc(void);
static void gameplayoptions_frameskip_selfunc(int flag);
static void gameplayoptions_frameskip_unselfunc(int flag);
static int gameplayoptions_frameskip_choosefunc(void);
static void gameplayoptions_allowfmv_selfunc(int flag);
static void gameplayoptions_allowfmv_unselfunc(int flag);
static int gameplayoptions_allowfmv_choosefunc(void);
static void gameplayoptions_HighlightItem(void);
static void gameplayoptions_UnhighlightItem(void);

void SetupMotelOptions(void);
static void moteloptions_detail_selfunc(int flag);
static void moteloptions_detail_unselfunc(int flag);
static int moteloptions_detail_choosefunc(void);
static void moteloptions_audio_selfunc(int flag);
static void moteloptions_audio_unselfunc(int flag);
static int moteloptions_audio_choosefunc(void);
static void moteloptions_control_selfunc(int flag);
static void moteloptions_control_unselfunc(int flag);
static int moteloptions_control_choosefunc(void);
static void moteloptions_gameplay_selfunc(int flag);
static void moteloptions_gameplay_unselfunc(int flag);
static int moteloptions_gameplay_choosefunc(void);
static void moteloptions_HighlightItem(void);
static void moteloptions_UnhighlightItem(void);

//------------------------------------------------------------------------------------------
// uninitialised data (0x600ce4)

// gameplayoptions.c
static char gameplayoptions_frameskip_text[8];
static int gameplayoptions_selection;
// NOTE: dword_600CF0; the first text of the item drawn red
static int gameplayoptions_motel_item;
// NOTE: dword_600CF4, a bss buffer: the text of the end entry of gameplayoptions_polyitems
static char gameplayoptions_poly_end_text[4];

// options.c
static int moteloptions_selection;
// NOTE: dword_600CFC; the text drawn red
static int moteloptions_motel_item;

//------------------------------------------------------------------------------------------
// initialised data (0x550688), gameplayoptions.c

// NOTE: not referenced by code
static FE_POLY gameplayoptions_polyitems[1] = {
	{ 0, 0, gameplayoptions_poly_end_text, 4, 0, 0, { 0, 0, 0, 0 }, 0 },
};

static FE_TEXT gameplayoptions_textitems[10] = {
	{ 40, 150, 0, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 120, 200, 0, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 520, 200, 0, 2, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 120, 225, 0, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 520, 225, 0, 2, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 120, 250, 0, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 520, 250, 0, 2, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 120, 275, 0, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 520, 275, 0, 2, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ -1, -1, 0, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
};

static FE_BUTTON gameplayoptions_menufunc[4] = {
	{ gameplayoptions_autobrake_selfunc, gameplayoptions_autobrake_unselfunc, gameplayoptions_autobrake_choosefunc, 0, 0, 2, 1 },
	{ gameplayoptions_frameskip_selfunc, gameplayoptions_frameskip_unselfunc, gameplayoptions_frameskip_choosefunc, 1, 1, 0, 2 },
	{ gameplayoptions_allowfmv_selfunc, gameplayoptions_allowfmv_unselfunc, gameplayoptions_allowfmv_choosefunc, 2, 2, 1, 0 },
	{ NULL, NULL, NULL, 0, 0, 0, 0 },
};

// gLanguageStrings indices: off, on; the difficulty levels
static int gameplayoptions_onoff[2] = { 2, 1 };
static int gameplayoptions_difficulty_names[3] = { 9, 10, 11 };
// NOTE: dword_550794; the second text of the item drawn red, -1: none
static int gameplayoptions_motel_item2 = -1;

//------------------------------------------------------------------------------------------
// initialised data (0x5507e0), options.c

static FE_TEXT moteloptions_textitems[6] = {
	{ 40, 150, 0, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 320, 200, 0, 1, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 320, 240, 0, 1, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 320, 280, 0, 1, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 320, 320, 0, 1, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ -1, -1, 0, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
};

static FE_BUTTON moteloptions_menufunc[5] = {
	{ moteloptions_detail_selfunc, moteloptions_detail_unselfunc, moteloptions_detail_choosefunc, 0, 0, 3, 1 },
	{ moteloptions_audio_selfunc, moteloptions_audio_unselfunc, moteloptions_audio_choosefunc, 1, 1, 0, 2 },
	{ moteloptions_control_selfunc, moteloptions_control_unselfunc, moteloptions_control_choosefunc, 2, 2, 1, 3 },
	{ moteloptions_gameplay_selfunc, moteloptions_gameplay_unselfunc, moteloptions_gameplay_choosefunc, 3, 3, 2, 0 },
	{ NULL, NULL, NULL, 0, 0, 0, 0 },
};

//------------------------------------------------------------------------------------------
// gameplayoptions.c

// [0x419f80] [D]
int DoMotelGameplayOptions(void)
{
	SetupMotelGameplayOptions();

	gameplayoptions_selection = 0;
	gameplayoptions_motel_item = 3;
	gameplayoptions_motel_item2 = 4;

	ResetMotelMenu(gameplayoptions_menufunc, 0, gameplayoptions_textitems);

	while (gameplayoptions_selection != -1)
	{
		HandleWinMessages();
		ReadInput();

		gameplayoptions_selection = MotelMenuKeyControl(gameplayoptions_menufunc, gameplayoptions_selection);

		gameplayoptions_HighlightItem();
		DrawMotel(gameplayoptions_textitems, NULL);
	}

	return -1;
}

// [0x41a005] [D]
// NOTE: the difficulty is shown but has no item here
void SetupMotelGameplayOptions(void)
{
	if (gDoInterpolation)
	{
		if (gDoAutoInterpolation)
			sprintf(gameplayoptions_frameskip_text, "%s", gLanguageStrings[213]);
		else
			sprintf(gameplayoptions_frameskip_text, "%dfps", 50 / max_frame_skip);
	}
	else
	{
		sprintf(gameplayoptions_frameskip_text, "%s", gLanguageStrings[2]);
	}

	gameplayoptions_textitems[0].text = gLanguageStrings[84];
	gameplayoptions_textitems[1].text = gLanguageStrings[85];
	gameplayoptions_textitems[3].text = gLanguageStrings[86];
	gameplayoptions_textitems[5].text = gLanguageStrings[87];
	gameplayoptions_textitems[7].text = gLanguageStrings[88];
	gameplayoptions_textitems[2].text = gLanguageStrings[gameplayoptions_difficulty_names[gDifficulty]];
	gameplayoptions_textitems[4].text = gLanguageStrings[gameplayoptions_onoff[gAutoBrake]];
	gameplayoptions_textitems[6].text = gameplayoptions_frameskip_text;
	gameplayoptions_textitems[8].text = gLanguageStrings[gameplayoptions_onoff[gAllowFMV]];
}

// [0x41a0fd] [D]
static void gameplayoptions_autobrake_selfunc(int flag)
{
	gameplayoptions_motel_item = 3;
	gameplayoptions_motel_item2 = 4;
}

// [0x41a116] [D]
static void gameplayoptions_autobrake_unselfunc(int flag)
{
	gameplayoptions_UnhighlightItem();
}

// [0x41a120] [D]
static int gameplayoptions_autobrake_choosefunc(void)
{
	gAutoBrake ^= 1;

	SetupMotelGameplayOptions();
	return 0;
}

// [0x41a139] [D]
static void gameplayoptions_frameskip_selfunc(int flag)
{
	gameplayoptions_motel_item = 5;
	gameplayoptions_motel_item2 = 6;
}

// [0x41a152] [D]
static void gameplayoptions_frameskip_unselfunc(int flag)
{
	gameplayoptions_UnhighlightItem();
}

// [0x41a15c] [D]
// Cycles off -> 50 fps -> 25 fps -> ... -> 10 fps -> auto -> off.
static int gameplayoptions_frameskip_choosefunc(void)
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

	SetupMotelGameplayOptions();
	return 0;
}

// [0x41a1d2] [D]
static void gameplayoptions_allowfmv_selfunc(int flag)
{
	gameplayoptions_motel_item = 7;
	gameplayoptions_motel_item2 = 8;
}

// [0x41a1eb] [D]
static void gameplayoptions_allowfmv_unselfunc(int flag)
{
	gameplayoptions_UnhighlightItem();
}

// [0x41a1f5] [D]
static int gameplayoptions_allowfmv_choosefunc(void)
{
	gAllowFMV ^= 1;

	SetupMotelGameplayOptions();
	return 0;
}

// [0x41a20e] [D]
// NOTE: sub_41A20E, the name is a guess
static void gameplayoptions_HighlightItem(void)
{
	if (gameplayoptions_selection == -1)
		return;

	gameplayoptions_textitems[gameplayoptions_motel_item].color.r = 255;
	gameplayoptions_textitems[gameplayoptions_motel_item].color.g = 0;
	gameplayoptions_textitems[gameplayoptions_motel_item].color.b = 0;

	if (gameplayoptions_motel_item2 != -1)
	{
		gameplayoptions_textitems[gameplayoptions_motel_item2].color.r = 255;
		gameplayoptions_textitems[gameplayoptions_motel_item2].color.g = 0;
		gameplayoptions_textitems[gameplayoptions_motel_item2].color.b = 0;
	}
}

// [0x41a283] [D]
// NOTE: sub_41A283, the name is a guess
static void gameplayoptions_UnhighlightItem(void)
{
	gameplayoptions_textitems[gameplayoptions_motel_item].color.r = 255;
	gameplayoptions_textitems[gameplayoptions_motel_item].color.g = 255;
	gameplayoptions_textitems[gameplayoptions_motel_item].color.b = 255;

	if (gameplayoptions_motel_item2 != -1)
	{
		gameplayoptions_textitems[gameplayoptions_motel_item2].color.r = 255;
		gameplayoptions_textitems[gameplayoptions_motel_item2].color.g = 255;
		gameplayoptions_textitems[gameplayoptions_motel_item2].color.b = 255;
	}
}

//------------------------------------------------------------------------------------------
// options.c

// [0x41a3d0] [D]
// The options are saved when the menu is left.
int DoMotelOptions(void)
{
	SetupMotelOptions();

	moteloptions_selection = 0;
	moteloptions_motel_item = 1;

	ResetMotelMenu(moteloptions_menufunc, 0, moteloptions_textitems);

	while (moteloptions_selection != -1)
	{
		HandleWinMessages();
		ReadInput();

		moteloptions_selection = MotelMenuKeyControl(moteloptions_menufunc, moteloptions_selection);

		moteloptions_HighlightItem();
		DrawMotel(moteloptions_textitems, NULL);
	}

	UpdateSavedOptions();
	return -1;
}

// [0x41a450] [D]
void SetupMotelOptions(void)
{
	moteloptions_textitems[0].text = gLanguageStrings[37];
	moteloptions_textitems[1].text = gLanguageStrings[38];
	moteloptions_textitems[2].text = gLanguageStrings[39];
	moteloptions_textitems[3].text = gLanguageStrings[40];
	moteloptions_textitems[4].text = gLanguageStrings[41];
}

// [0x41a48d] [D]
static void moteloptions_detail_selfunc(int flag)
{
	moteloptions_motel_item = 1;
}

// [0x41a49c] [D]
static void moteloptions_detail_unselfunc(int flag)
{
	moteloptions_UnhighlightItem();
}

// [0x41a4a6] [D]
static int moteloptions_detail_choosefunc(void)
{
	DoMotelDetailOptions();
	return 0;
}

// [0x41a4b2] [D]
static void moteloptions_audio_selfunc(int flag)
{
	moteloptions_motel_item = 2;
}

// [0x41a4c1] [D]
static void moteloptions_audio_unselfunc(int flag)
{
	moteloptions_UnhighlightItem();
}

// [0x41a4cb] [D]
static int moteloptions_audio_choosefunc(void)
{
	SelectAudioOptions();
	return 0;
}

// [0x41a4d7] [D]
static void moteloptions_control_selfunc(int flag)
{
	moteloptions_motel_item = 3;
}

// [0x41a4e6] [D]
static void moteloptions_control_unselfunc(int flag)
{
	moteloptions_UnhighlightItem();
}

// [0x41a4f0] [D]
static int moteloptions_control_choosefunc(void)
{
	SelectController();
	return 0;
}

// [0x41a4fc] [D]
static void moteloptions_gameplay_selfunc(int flag)
{
	moteloptions_motel_item = 4;
}

// [0x41a50b] [D]
static void moteloptions_gameplay_unselfunc(int flag)
{
	moteloptions_UnhighlightItem();
}

// [0x41a515] [D]
static int moteloptions_gameplay_choosefunc(void)
{
	DoMotelGameplayOptions();
	return 0;
}

// [0x41a521] [D]
// NOTE: sub_41A521, the name is a guess
static void moteloptions_HighlightItem(void)
{
	moteloptions_textitems[moteloptions_motel_item].color.r = 255;
	moteloptions_textitems[moteloptions_motel_item].color.g = 0;
	moteloptions_textitems[moteloptions_motel_item].color.b = 0;
}

// [0x41a555] [D]
// NOTE: sub_41A555, the name is a guess
static void moteloptions_UnhighlightItem(void)
{
	moteloptions_textitems[moteloptions_motel_item].color.r = 255;
	moteloptions_textitems[moteloptions_motel_item].color.g = 255;
	moteloptions_textitems[moteloptions_motel_item].color.b = 255;
}
