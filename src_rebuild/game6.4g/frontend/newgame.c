/*
 * frontend/newgame.c - "Undercover" menu: new game or load game
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "frontend\newgame.c", 0x413980 - 0x413aa6)
 */

#include "dr_types.h"
#include "dr_globals.h"
#include "pres.h"
#include "frontend/frontend.h"
#include "frontend/loadgame.h"
#include "frontend/newgame.h"
#include "input.h"
#include "shell.h"
#include "dr_scores.h"

void SetOptionsNewGame(void);
static void newgame_selfunc(int flag);
static void newgame_unselfunc(int flag);
static int newgame_choosefunc(void);
static void loadgame_selfunc(int flag);
static void loadgame_unselfunc(int flag);
static int loadgame_choosefunc(void);

//------------------------------------------------------------------------------------------
// uninitialised data (0x600c00)

static int selection;
// NOTE: dword_600C04, the bss buffer the polyitems terminator points at (as in frontend/main.c)
static char poly_end_text[4];

//------------------------------------------------------------------------------------------
// initialised data (0x54f220)

static FE_POLY polyitems[1] = {
	{ 0, 0, poly_end_text, 4, 0, 0, { 0, 0, 0, 0 }, 0 },
};
// NOTE: 4 zero bytes follow in the binary (0x54f234), as after every FE_POLY array: padding

static FE_TEXT textitems[4] = {
	{ 40, 150, 0, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 320, 200, 0, 1, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 320, 240, 0, 1, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ -1, -1, 0, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
};

static FE_BUTTON menufunc[3] = {
	{ newgame_selfunc, newgame_unselfunc, newgame_choosefunc, 0, 0, 1, 1 },
	{ loadgame_selfunc, loadgame_unselfunc, loadgame_choosefunc, 1, 1, 0, 0 },
	{ NULL, NULL, NULL, 0, 0, 0, 0 },
};

// [0x413980] [D]
int SelectNewGame(void)
{
	int ret;

	SetOptionsNewGame();

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

// [0x4139fc] [D]
void SetOptionsNewGame(void)
{
	textitems[0].text = gLanguageStrings[22];
	textitems[1].text = gLanguageStrings[23];
	textitems[2].text = gLanguageStrings[24];
}

// [0x413a23] [D]
static void newgame_selfunc(int flag)
{
	textitems[1].font = 1;
}

// [0x413a2f] [D]
static void newgame_unselfunc(int flag)
{
	textitems[1].font = 0;
}

// [0x413a3b] [D]
static int newgame_choosefunc(void)
{
	CloseFrontendScreen();

	global_score.type = GAME_MISSION;
	StartMissionFromFrontend(0);

	return 1;
}

// [0x413a5e] [D]
static void loadgame_selfunc(int flag)
{
	textitems[2].font = 1;
}

// [0x413a6a] [D]
static void loadgame_unselfunc(int flag)
{
	textitems[2].font = 0;
}

// [0x413a76] [D]
static int loadgame_choosefunc(void)
{
	int ret;

	CloseFrontendScreen();
	ret = SelectLoadGame();

	if (ret == 0)
		OpenFrontendScreen(polyitems, textitems);

	return ret;
}
