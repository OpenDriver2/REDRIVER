/*
 * frontend/main.c - frontend main menu
 *
 * Rebuilt from Driver_sym2_94.i64 (IDA folder "frontend\main.c").
 */

#include <windows.h>

#include "dr_types.h"
#include "dr_globals.h"
#include "frontend/demo.h"
#include "frontend/frontend.h"
#include "pres.h"
#include "FMV/render.h"
#include "frontend/cheats.h"
#include "frontend/config.h"
#include "frontend/drivinggames.h"
#include "frontend/gvars.h"
#include "frontend/main.h"
#include "frontend/newgame.h"
#include "frontend/options.h"
#include "frontend/replay.h"
#include "frontend/scores.h"
#include "frontend/takearide.h"
#include "frontend/training.h"
#include "general.h"
#include "input.h"
#include "shell.h"

#define DEMO_START_TIME		45

/* sub-menus: frontend/newgame.c, scores.c, options.c, takearide.c, drivinggames.c, cheats.c,
   training.c, replay.c */

void DoRollingDemo(int demo);
void SetupQuit(int on);

static void newgame_selfunc(int flag);
static void newgame_unselfunc(int flag);
static int newgame_choosefunc(void);
static void scores_selfunc(int flag);
static void scores_unselfunc(int flag);
static int scores_choosefunc(void);
static void options_selfunc(int flag);
static void options_unselfunc(int flag);
static int options_choosefunc(void);
static void takearide_selfunc(int flag);
static void takearide_unselfunc(int flag);
static int takearide_choosefunc(void);
static void driving_selfunc(int flag);
static void driving_unselfunc(int flag);
static int driving_choosefunc(void);
static void cheats_selfunc(int flag);
static void cheats_unselfunc(int flag);
static int cheats_choosefunc(void);
static void training_selfunc(int flag);
static void training_unselfunc(int flag);
static int training_choosefunc(void);
static void replay_selfunc(int flag);
static void replay_unselfunc(int flag);
static int replay_choosefunc(void);

// quit question handlers (unnamed in IDA)
static void quitno_selfunc(int flag);
static void quitno_unselfunc(int flag);
static int quitno_choosefunc(void);
static void quityes_selfunc(int flag);
static void quityes_unselfunc(int flag);
static int quityes_choosefunc(void);

//------------------------------------------------------------------------------------------
// uninitialised data (0x600bd0)

static int selection;
int quit_selection;
int ask_quit;
static int unknown_600BDC;		// NOTE: no references
// NOTE: polyitems text buffers in bss (byte_600BE0, byte_600BE4), never written by name; sizes by layout
static char poly_text[4];
static char poly_end_text[8];

//------------------------------------------------------------------------------------------
// initialised data (0x54ecd8)

static FE_POLY polyitems[9] = {
	{ 223, 140, poly_text, 0, 192, 142, { 0, 0, 0, -1 }, 0 },
	{ 145, 107, "FLAGICO1", 1, 0, 0, { -1, -1, -1, -1 }, 0 },
	{ 215, 69, "TANRICO1", 1, 0, 0, { -1, -1, -1, -1 }, 0 },
	{ 366, 132, "RUSHICO1", 1, 0, 0, { -1, -1, -1, -1 }, 0 },
	{ 402, 189, "CONEICO1", 1, 0, 0, { -1, -1, -1, -1 }, 0 },
	{ 310, 260, "TYREICO1", 1, 0, 0, { -1, -1, -1, -1 }, 0 },
	{ 304, 218, "PCARICO1", 1, 0, 0, { -1, -1, -1, -1 }, 0 },
	{ 109, 193, "MCARICO1", 1, 0, 0, { -1, -1, -1, -1 }, 0 },
	{ 0, 0, poly_end_text, 4, 0, 0, { 0, 0, 0, 0 }, 0 },
};

static int unknown_54ED8C = 0;	// NOTE: 4 zero bytes, no references (a variable or padding)

static FE_TEXT textitems[4] = {
	{ 320, 395, 1, 1, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 330, 235, 0, 2, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 345, 235, 0, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ -1, -1, 0, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
};

static FE_BUTTON menufunc[9] = {
	{ newgame_selfunc, newgame_unselfunc, newgame_choosefunc, 1, 7, 0, 0 },
	{ scores_selfunc, scores_unselfunc, scores_choosefunc, 2, 0, 1, 1 },
	{ options_selfunc, options_unselfunc, options_choosefunc, 3, 1, 2, 2 },
	{ takearide_selfunc, takearide_unselfunc, takearide_choosefunc, 4, 2, 3, 3 },
	{ driving_selfunc, driving_unselfunc, driving_choosefunc, 5, 3, 4, 4 },
	{ cheats_selfunc, cheats_unselfunc, cheats_choosefunc, 6, 4, 5, 5 },
	{ training_selfunc, training_unselfunc, training_choosefunc, 7, 5, 6, 6 },
	{ replay_selfunc, replay_unselfunc, replay_choosefunc, 0, 6, 7, 7 },
	{ NULL, NULL, NULL, 0, 0, 0, 0 },
};

FE_BUTTON quitmenufunc[3] = {
	{ quitno_selfunc, quitno_unselfunc, quitno_choosefunc, 1, 1, 0, 0 },
	{ quityes_selfunc, quityes_unselfunc, quityes_choosefunc, 0, 0, 1, 1 },
	{ NULL, NULL, NULL, 0, 0, 0, 0 },
};

// [0x412cd0] [D]
void DoFrontend(void)
{
	int ret;

	ReadKeyTextFile();

	ClearFullDisplay(4);

	if (gUserLanguage == 5)
		FadeInScreen("DATA\\USCOPYRIGHT.BMP", 5);
	else
		FadeInScreen("DATA\\COPYRIGHT.BMP", 5);

	ClearFullDisplay(4);
	FadeInScreen("DATA\\GT.BMP", 5);

	ClearFullDisplay(4);
	FadeInScreen("DATA\\REFLECT.BMP", 5);

	PlayRender(gScreen.handle, 0, -1);

	if (!InitFrontend())
	{
		QuitFrontend();
		return;
	}

	global_score.type = GAME_DEFAULT;

	selection = 0;
	ResetMenu(menufunc, 0);
	OpenFrontendScreen(polyitems, textitems);

	for (;;)
	{
		// wait while the window is inactive
		for (;;)
		{
			HandleWinMessages();
			ReadInput();

			if (gScreen.active)
				break;

			demo_timer_count = 0;
			WaitForSync(0);
		}

		if (demo_timer_count >= DEMO_START_TIME)
			DoRollingDemo(-1);

		if (ask_quit)
		{
			ret = HandleMenu(quitmenufunc, &quit_selection);

			if (ret == 1)
				break;

			if (ret == -1)
			{
				ask_quit = 0;
				SetupQuit(0);
				ResetMenu(menufunc, selection);
			}
		}
		else
		{
			ret = HandleMenu(menufunc, &selection);

			if (ret == -1)
			{
				ask_quit = 1;
				quit_selection = 0;
				SetupQuit(1);
				ResetMenu(quitmenufunc, quit_selection);
			}
			else if (ret == 1)
			{
				OpenFrontendScreen(polyitems, textitems);
			}
		}

		global_score.type = GAME_DEFAULT;

		DrawFrontend();
	}

	CloseFrontendScreen();
	QuitFrontend();
	UpdateSavedOptions();
}

// [0x412ebf] [D]
// Plays a rolling demo (-1: the next one, after the main menu sat idle) and reopens the menu.
void DoRollingDemo(int demo)
{
	CloseFrontendScreen();
	StartRollingDemo(demo);

	if (demo == -1)
		OpenFrontendScreen(polyitems, textitems);
}

// [0x412eed] [D]
// Selected: lit icon and the item's name (the flashing calls it with 1).
static void scores_selfunc(int flash)
{
	polyitems[1].text = "FLAGICO2";
	textitems[0].text = gLanguageStrings[15];

	if (!flash)
		demo_timer_count = 0;
}

// [0x412f16] [D]
static void scores_unselfunc(int flash)
{
	if (!flash)
		polyitems[1].text = "FLAGICO1";
}

// [0x412f2b] [D]
static int scores_choosefunc(void)
{
	int ret;

	CloseFrontendScreen();
	ret = SelectScores();

	if (ret == 0)
		OpenFrontendScreen(polyitems, textitems);

	return ret;
}

// [0x412f5b] [D]
// Selected: lit icon and the item's name (the flashing calls it with 1).
static void newgame_selfunc(int flash)
{
	polyitems[2].text = "TANRICO2";
	textitems[0].text = gLanguageStrings[14];

	if (!flash)
		demo_timer_count = 0;
}

// [0x412f84] [D]
static void newgame_unselfunc(int flash)
{
	if (!flash)
		polyitems[2].text = "TANRICO1";
}

// [0x412f99] [D]
static int newgame_choosefunc(void)
{
	int ret;

	CloseFrontendScreen();
	ret = SelectNewGame();

	if (ret == 0)
		OpenFrontendScreen(polyitems, textitems);

	return ret;
}

// [0x412fc9] [D]
// Selected: lit icon and the item's name (the flashing calls it with 1).
static void replay_selfunc(int flash)
{
	polyitems[3].text = "RUSHICO2";
	textitems[0].text = gLanguageStrings[21];

	if (!flash)
		demo_timer_count = 0;
}

// [0x412ff2] [D]
static void replay_unselfunc(int flash)
{
	if (!flash)
		polyitems[3].text = "RUSHICO1";
}

// [0x413007] [D]
static int replay_choosefunc(void)
{
	int ret;

	CloseFrontendScreen();
	ret = SelectReplay();

	if (ret == 0)
		OpenFrontendScreen(polyitems, textitems);

	return ret;
}

// [0x413037] [D]
// Selected: lit icon and the item's name (the flashing calls it with 1).
static void training_selfunc(int flash)
{
	polyitems[4].text = "CONEICO2";
	textitems[0].text = gLanguageStrings[20];

	if (!flash)
		demo_timer_count = 0;
}

// [0x413060] [D]
static void training_unselfunc(int flash)
{
	if (!flash)
		polyitems[4].text = "CONEICO1";
}

// [0x413075] [D]
static int training_choosefunc(void)
{
	int ret;

	CloseFrontendScreen();
	ret = SelectTraining();

	if (ret == 0)
		OpenFrontendScreen(polyitems, textitems);

	return ret;
}

// [0x4130a5] [D]
// Selected: lit icon and the item's name (the flashing calls it with 1).
static void driving_selfunc(int flash)
{
	polyitems[5].text = "TYREICO2";
	textitems[0].text = gLanguageStrings[18];

	if (!flash)
		demo_timer_count = 0;
}

// [0x4130ce] [D]
static void driving_unselfunc(int flash)
{
	if (!flash)
		polyitems[5].text = "TYREICO1";
}

// [0x4130e3] [D]
static int driving_choosefunc(void)
{
	int ret;

	CloseFrontendScreen();
	ret = SelectDrivingGames();

	if (ret == 0)
		OpenFrontendScreen(polyitems, textitems);

	return ret;
}

// [0x413113] [D]
// Selected: lit icon and the item's name (the flashing calls it with 1).
static void cheats_selfunc(int flash)
{
	polyitems[6].text = "PCARICO2";
	textitems[0].text = gLanguageStrings[19];

	if (!flash)
		demo_timer_count = 0;
}

// [0x41313c] [D]
static void cheats_unselfunc(int flash)
{
	if (!flash)
		polyitems[6].text = "PCARICO1";
}

// [0x413151] [D]
static int cheats_choosefunc(void)
{
	int ret;

	CloseFrontendScreen();
	ret = SelectCheats();

	if (ret == 0)
		OpenFrontendScreen(polyitems, textitems);

	return ret;
}

// [0x413181] [D]
// Selected: lit icon and the item's name (the flashing calls it with 1).
static void takearide_selfunc(int flash)
{
	polyitems[7].text = "MCARICO2";
	textitems[0].text = gLanguageStrings[17];

	if (!flash)
		demo_timer_count = 0;
}

// [0x4131aa] [D]
static void takearide_unselfunc(int flash)
{
	if (!flash)
		polyitems[7].text = "MCARICO1";
}

// [0x4131bf] [D]
static int takearide_choosefunc(void)
{
	int ret;

	CloseFrontendScreen();
	ret = SelectTakeARide();

	if (ret == 0)
		OpenFrontendScreen(polyitems, textitems);

	return ret;
}

// [0x4131ef] [D]
// Selected: lit icon and the item's name (the flashing calls it with 1).
static void options_selfunc(int flash)
{
	polyitems[7].text = "MCARICO3";
	textitems[0].text = gLanguageStrings[16];

	if (!flash)
		demo_timer_count = 0;
}

// [0x413218] [D]
static void options_unselfunc(int flash)
{
	if (!flash)
		polyitems[7].text = "MCARICO1";
}

// [0x41322d] [D]
static int options_choosefunc(void)
{
	int ret;

	CloseFrontendScreen();
	ret = SelectOptions();

	if (ret == 0)
		OpenFrontendScreen(polyitems, textitems);

	return ret;
}

// [0x41325d] [D]
// Shows (on) or hides the quit question, greying out the rest of the main menu.
void SetupQuit(int on)
{
	int i;

	FE_PlayClickSound();

	if (on)
	{
		textitems[1].text = gLanguageStrings[3];
		textitems[2].text = gLanguageStrings[4];

		for (i = 1; i < 8; i++)
		{
			polyitems[i].color.r = 64;
			polyitems[i].color.g = 64;
			polyitems[i].color.b = 64;
		}

		textitems[0].color.r = 64;
		textitems[0].color.g = 64;
		textitems[0].color.b = 64;

		backcol.r = 64;
		backcol.g = 64;
		backcol.b = 64;
	}
	else
	{
		textitems[1].text = NULL;
		textitems[2].text = NULL;

		for (i = 1; i < 8; i++)
		{
			polyitems[i].color.r = -1;
			polyitems[i].color.g = -1;
			polyitems[i].color.b = -1;
		}

		textitems[0].color.r = -1;
		textitems[0].color.g = -1;
		textitems[0].color.b = -1;

		backcol.r = -1;
		backcol.g = -1;
		backcol.b = -1;
	}
}

// [0x41341a] [D]
// NOTE: unnamed in IDA (sub_41341A); name guessed. The quit question's first answer (cancels).
static void quitno_selfunc(int flash)
{
	textitems[1].font = 1;

	if (!flash)
		demo_timer_count = 0;
}

// [0x413436] [D]
// NOTE: unnamed in IDA (sub_413436); name guessed
static void quitno_unselfunc(int flash)
{
	textitems[1].font = 0;
}

// [0x413442] [D]
// NOTE: unnamed in IDA (sub_413442); name guessed
static int quitno_choosefunc(void)
{
	demo_timer_count = 0;
	return -1;
}

// [0x413454] [D]
// NOTE: unnamed in IDA (sub_413454); name guessed. The quit question's second answer (quits).
static void quityes_selfunc(int flash)
{
	textitems[2].font = 1;

	if (!flash)
		demo_timer_count = 0;
}

// [0x413470] [D]
// NOTE: unnamed in IDA (sub_413470); name guessed
static void quityes_unselfunc(int flash)
{
	textitems[2].font = 0;
}

// [0x41347c] [D]
// NOTE: unnamed in IDA (sub_41347C); name guessed
static int quityes_choosefunc(void)
{
	demo_timer_count = 0;
	return 1;
}
