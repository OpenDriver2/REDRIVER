/*
 * frontend/daynight.c - day or night choice before a game
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "frontend\daynight.c", 0x4084d0 - 0x4086fd)
 */

#include "dr_types.h"
#include "dr_globals.h"
#include "pres.h"
#include "shell.h"
#include "input.h"
#include "frontend/frontend.h"
#include "frontend/daynight.h"

void SetupDayNight(void);
static void day_selfunc(int flag);
static void day_unselfunc(int flag);
static int day_choosefunc(void);
static void night_selfunc(int flag);
static void night_unselfunc(int flag);
static int night_choosefunc(void);

//------------------------------------------------------------------------------------------
// uninitialised data (0x5d79e8)

static int selection;
// NOTE: byte_5D79EC, a bss buffer: the text of the end entry of polyitems
static char poly_end_text[4];

//------------------------------------------------------------------------------------------
// initialised data (0x54c150)

static FE_POLY polyitems[1] = {
	{ 0, 0, poly_end_text, 4, 0, 0, { 0, 0, 0, 0 }, 0 },
};

static FE_TEXT textitems[3] = {
	{ 320, 200, 0, 1, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 320, 240, 0, 1, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ -1, -1, 0, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
};

static FE_BUTTON menufunc[3] = {
	{ day_selfunc, day_unselfunc, day_choosefunc, 0, 0, 1, 1 },
	{ night_selfunc, night_unselfunc, night_choosefunc, 1, 1, 0, 0 },
	{ NULL, NULL, NULL, 0, 0, 0, 0 },
};

// [0x4084d0] [D]
int SelectDayNight(void)
{
	int ret;

	SetupDayNight();

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

// [0x40854c] [D]
void SetupDayNight(void)
{
	textitems[0].text = gLanguageStrings[97];
	textitems[1].text = gLanguageStrings[98];
}

// [0x408567] [D]
static void day_selfunc(int flag)
{
	textitems[0].font = 1;
}

// [0x408573] [D]
static void day_unselfunc(int flag)
{
	textitems[0].font = 0;
}

// [0x40857f] [D]
// The game modes 46 - 49 are played by day, 146 - 149 the same at night.
static int day_choosefunc(void)
{
	switch (GameMode)
	{
		case 46:
		case 146:
			GameMode = 46;
			break;
		case 47:
		case 147:
			GameMode = 47;
			break;
		case 48:
		case 148:
			GameMode = 48;
			break;
		case 49:
		case 149:
			GameMode = 49;
			break;
	}

	CloseFrontendScreen();
	StartGameFromFrontend(1, 0);

	return 1;
}

// [0x408670] [D]
static void night_selfunc(int flag)
{
	textitems[1].font = 1;
}

// [0x40867c] [D]
static void night_unselfunc(int flag)
{
	textitems[1].font = 0;
}

// [0x408688] [D]
static int night_choosefunc(void)
{
	switch (GameMode)
	{
		case 46:
		case 146:
			GameMode = 146;
			break;
		case 47:
		case 147:
			GameMode = 147;
			break;
		case 48:
		case 148:
			GameMode = 148;
			break;
		case 49:
		case 149:
			GameMode = 149;
			break;
	}

	CloseFrontendScreen();
	StartGameFromFrontend(1, 0);

	return 1;
}
