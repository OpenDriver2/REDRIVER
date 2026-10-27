/*
 * frontend/scores.c - high score tables menu: pick the game type
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "frontend\scores.c", 0x413fe0 - 0x4141cf)
 */

#include "dr_types.h"
#include "frontend/highscores.h"
#include "pres.h"
#include "frontend/frontend.h"
#include "frontend/scores.h"
#include "input.h"
#include "shell.h"

void SetupScores(void);
static void checkpoint_selfunc(int flag);
static void checkpoint_unselfunc(int flag);
static void trail_selfunc(int flag);
static void trail_unselfunc(int flag);
static void survival_selfunc(int flag);
static void survival_unselfunc(int flag);
static void dirttrack_selfunc(int flag);
static void dirttrack_unselfunc(int flag);
static void carnage_selfunc(int flag);
static void carnage_unselfunc(int flag);
int highscore_choosefunc(void);

//------------------------------------------------------------------------------------------
// uninitialised data (0x600c18)

static int selection;
// NOTE: dword_600C1C, the bss buffer the polyitems terminator points at (as in frontend/main.c)
static char poly_end_text[4];

//------------------------------------------------------------------------------------------
// initialised data (0x54f4d0)

static FE_POLY polyitems[1] = {
	{ 0, 0, poly_end_text, 4, 0, 0, { 0, 0, 0, 0 }, 0 },
};
// NOTE: 4 zero bytes follow in the binary (0x54f4e4), as after every FE_POLY array: padding

static FE_TEXT textitems[7] = {
	{ 40, 150, 0, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 320, 200, 0, 1, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 320, 235, 0, 1, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 320, 270, 0, 1, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 320, 305, 0, 1, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 320, 340, 0, 1, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ -1, -1, 0, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
};

static FE_BUTTON menufunc[6] = {
	{ checkpoint_selfunc, checkpoint_unselfunc, highscore_choosefunc, 0, 0, 4, 1 },
	{ trail_selfunc, trail_unselfunc, highscore_choosefunc, 1, 1, 0, 2 },
	{ survival_selfunc, survival_unselfunc, highscore_choosefunc, 2, 2, 1, 3 },
	{ dirttrack_selfunc, dirttrack_unselfunc, highscore_choosefunc, 3, 3, 2, 4 },
	{ carnage_selfunc, carnage_unselfunc, highscore_choosefunc, 4, 4, 3, 0 },
	{ NULL, NULL, NULL, 0, 0, 0, 0 },
};

// [0x413fe0] [D]
int SelectScores(void)
{
	int ret;

	SetupScores();

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

// [0x41405c] [D]
void SetupScores(void)
{
	textitems[0].text = gLanguageStrings[25];
	textitems[1].text = gLanguageStrings[92];
	textitems[2].text = gLanguageStrings[93];
	textitems[3].text = gLanguageStrings[94];
	textitems[4].text = gLanguageStrings[95];
	textitems[5].text = gLanguageStrings[96];
}

// [0x4140a5] [D]
static void checkpoint_selfunc(int flag)
{
	textitems[1].font = 1;
	highscore_type = GAME_CHECKPOINT;
	highscore_index = 0;
}

// [0x4140c5] [D]
static void checkpoint_unselfunc(int flag)
{
	if (flag == 0)
		textitems[1].font = 0;
}

// [0x4140d7] [D]
static void trail_selfunc(int flag)
{
	textitems[2].font = 1;
	highscore_type = GAME_TRAILBLAZER;
	highscore_index = 0;
}

// [0x4140f7] [D]
static void trail_unselfunc(int flag)
{
	if (flag == 0)
		textitems[2].font = 0;
}

// [0x414109] [D]
static void survival_selfunc(int flag)
{
	textitems[3].font = 1;
	highscore_type = GAME_SURVIVAL;
	highscore_index = 0;
}

// [0x414129] [D]
static void survival_unselfunc(int flag)
{
	if (flag == 0)
		textitems[3].font = 0;
}

// [0x41413b] [D]
// the dirt track tables are the time trial ones
static void dirttrack_selfunc(int flag)
{
	textitems[4].font = 1;
	highscore_type = GAME_TIMETRIAL;
	highscore_index = 0;
}

// [0x41415b] [D]
static void dirttrack_unselfunc(int flag)
{
	if (flag == 0)
		textitems[4].font = 0;
}

// [0x41416d] [D]
static void carnage_selfunc(int flag)
{
	textitems[5].font = 1;
	highscore_type = GAME_CARNAGE;
	highscore_index = 0;
}

// [0x41418d] [D]
static void carnage_unselfunc(int flag)
{
	if (flag == 0)
		textitems[5].font = 0;
}

// [0x41419f] [D]
int highscore_choosefunc(void)
{
	int ret;

	CloseFrontendScreen();
	ret = SelectShowHighscore();

	if (ret == 0)
		OpenFrontendScreen(polyitems, textitems);

	return ret;
}
