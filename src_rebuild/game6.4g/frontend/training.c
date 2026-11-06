/*
 * frontend/training.c - training menu: car park or desert
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "frontend\training.c", 0x416870 - 0x4169c1)
 */

#include "dr_types.h"
#include "dr_globals.h"
#include "pres.h"
#include "shell.h"
#include "frontend/frontend.h"
#include "frontend/training.h"
#include "input.h"
#include "dr_scores.h"

void SetOptionsTraining(void);
static void carpark_selfunc(int flag);
static void carpark_unselfunc(int flag);
static int carpark_choosefunc(void);
static void desert_selfunc(int flag);
static void desert_unselfunc(int flag);
static int desert_choosefunc(void);

//------------------------------------------------------------------------------------------
// uninitialised data (0x600c58)

static int selection;
// NOTE: dword_600C5C, a bss buffer: the picture shown before an item is selected (empty)
static char poly_text[4];

//------------------------------------------------------------------------------------------
// initialised data (0x5502a8)

static FE_POLY polyitems[2] = {
	{ 327, 201, poly_text, 1, 0, 0, { -1, -1, -1, -1 }, 0 },
	{ 0, 0, NULL, 4, 0, 0, { 0, 0, 0, 0 }, 0 },
};

static FE_TEXT textitems[4] = {
	{ 40, 150, 0, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 26, 210, 0, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 26, 250, 0, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ -1, -1, 0, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
};

static FE_BUTTON menufunc[3] = {
	{ carpark_selfunc, carpark_unselfunc, carpark_choosefunc, 0, 0, 1, 1 },
	{ desert_selfunc, desert_unselfunc, desert_choosefunc, 1, 1, 0, 0 },
	{ NULL, NULL, NULL, 0, 0, 0, 0 },
};

// [0x416870] [D]
int SelectTraining(void)
{
	int ret;

	SetOptionsTraining();

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

// [0x4168ec] [D]
void SetOptionsTraining(void)
{
	textitems[0].text = gLanguageStrings[101];
	textitems[1].text = gLanguageStrings[102];
	textitems[2].text = gLanguageStrings[103];
}

// [0x416913] [D]
static void carpark_selfunc(int flag)
{
	polyitems[0].text = "GARAGE";
	textitems[1].font = 1;
}

// [0x416929] [D]
static void carpark_unselfunc(int flag)
{
	if (flag == 0)
		textitems[1].font = 0;
}

// [0x41693b] [D]
static int carpark_choosefunc(void)
{
	CloseFrontendScreen();

	global_score.type = GAME_CARPARK;
	GameMode = 200;
	StartGameFromFrontend(1, 0);

	return 1;
}

// [0x41696a] [D]
static void desert_selfunc(int flag)
{
	polyitems[0].text = "DESERT";
	textitems[2].font = 1;
}

// [0x416980] [D]
static void desert_unselfunc(int flag)
{
	if (flag == 0)
		textitems[2].font = 0;
}

// [0x416992] [D]
static int desert_choosefunc(void)
{
	CloseFrontendScreen();

	global_score.type = GAME_CARPARK;
	GameMode = 250;
	StartGameFromFrontend(1, 0);

	return 1;
}
