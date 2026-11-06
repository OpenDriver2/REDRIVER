/*
 * frontend/dirttrack.c - dirt track menu: practice or time trial
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "frontend\dirttrack.c", 0x40a5d0 - 0x40a717)
 */

#include "dr_types.h"
#include "dr_globals.h"
#include "pres.h"
#include "shell.h"
#include "input.h"
#include "frontend/frontend.h"
#include "frontend/selrace.h"
#include "frontend/dirttrack.h"
#include "dr_scores.h"

void SetOptionsDirtTrackMode(void);
static void practise_selfunc(int flag);
static void practise_unselfunc(int flag);
static int practise_choosefunc(void);
static void timetrial_selfunc(int flag);
static void timetrial_unselfunc(int flag);
static int timetrial_choosefunc(void);

//------------------------------------------------------------------------------------------
// uninitialised data (0x5d7a08)

static int selection;
// NOTE: byte_5D7A0C, a bss buffer: the text of the end entry of polyitems
static char poly_end_text[4];

//------------------------------------------------------------------------------------------
// initialised data (0x54c8c8)

static FE_POLY polyitems[1] = {
	{ 0, 0, poly_end_text, 4, 0, 0, { 0, 0, 0, 0 }, 0 },
};

static FE_TEXT textitems[4] = {
	{ 40, 150, 0, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 320, 200, 0, 1, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 320, 240, 0, 1, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ -1, -1, 0, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
};

static FE_BUTTON menufunc[3] = {
	{ practise_selfunc, practise_unselfunc, practise_choosefunc, 0, 0, 1, 1 },
	{ timetrial_selfunc, timetrial_unselfunc, timetrial_choosefunc, 1, 1, 0, 0 },
	{ NULL, NULL, NULL, 0, 0, 0, 0 },
};

// [0x40a5d0] [D]
int SelectDirtTrackMode(void)
{
	int ret;

	SetOptionsDirtTrackMode();

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

// [0x40a64c] [D]
void SetOptionsDirtTrackMode(void)
{
	textitems[0].text = gLanguageStrings[121];
	textitems[1].text = gLanguageStrings[116];
	textitems[2].text = gLanguageStrings[117];
}

// [0x40a673] [D]
static void practise_selfunc(int flag)
{
	textitems[1].font = 1;
}

// [0x40a67f] [D]
static void practise_unselfunc(int flag)
{
	textitems[1].font = 0;
}

// [0x40a68b] [D]
static int practise_choosefunc(void)
{
	int ret;

	CloseFrontendScreen();

	global_score.type = GAME_DIRTTRACK;
	ret = SelectRace();

	if (ret == 0)
		OpenFrontendScreen(polyitems, textitems);

	return ret;
}

// [0x40a6c5] [D]
static void timetrial_selfunc(int flag)
{
	textitems[2].font = 1;
}

// [0x40a6d1] [D]
static void timetrial_unselfunc(int flag)
{
	textitems[2].font = 0;
}

// [0x40a6dd] [D]
static int timetrial_choosefunc(void)
{
	int ret;

	CloseFrontendScreen();

	global_score.type = GAME_TIMETRIAL;
	ret = SelectRace();

	if (ret == 0)
		OpenFrontendScreen(polyitems, textitems);

	return ret;
}
