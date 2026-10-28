/*
 * flat/exit.c - motel room door: leave the game (to the main menu) or cancel
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "motel\exit.c=flat\exit.c", 0x417e50 - 0x417ff7)
 */

#include "dr_types.h"
#include "dr_globals.h"
#include "pres.h"
#include "shell.h"
#include "input.h"
#include "flat/frontend.h"
#include "flat/exit.h"

void SetupMotelExit(void);
static void HighlightItem(void);
static void cancel_selfunc(int flag);
static void cancel_unselfunc(int flag);
static void UnhighlightItem(void);
static int cancel_choosefunc(void);
static void exit_selfunc(int flag);
static void exit_unselfunc(int flag);
static int exit_choosefunc(void);

//------------------------------------------------------------------------------------------
// initialised data (0x550580)

static FE_TEXT textitems[4] = {
	{ 40, 150, 0, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 320, 200, 0, 1, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 320, 230, 0, 1, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ -1, -1, 0, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
};

static FE_BUTTON menufunc[3] = {
	{ cancel_selfunc, cancel_unselfunc, cancel_choosefunc, 0, 0, 1, 1 },
	{ exit_selfunc, exit_unselfunc, exit_choosefunc, 1, 1, 0, 0 },
	{ NULL, NULL, NULL, 0, 0, 0, 0 },
};

int retcode = -1;			// 1: leave, 0: stay

//------------------------------------------------------------------------------------------
// uninitialised data (0x600c9c)

static int selection;
int flashsel;				// the text drawn red

// [0x417e50] [D]
// 1 when the player leaves the game (the room fades out), else 0.
int DoMotelExit(void)
{
	SetupMotelExit();

	selection = 0;
	flashsel = 1;
	retcode = -1;

	ResetMotelMenu(menufunc, 0, textitems);

	while (retcode == -1)
	{
		HandleWinMessages();
		ReadInput();

		selection = MotelMenuKeyControl(menufunc, selection);

		if (selection == -1)
		{
			retcode = 0;
			break;
		}

		HighlightItem();
		DrawMotel(textitems, NULL);
	}

	if (retcode)
	{
		flat_retcode = -1;
		FadeOutMotel(textitems, NULL);
	}

	return retcode;
}

// [0x417f0e] [D]
void SetupMotelExit(void)
{
	textitems[0].text = gLanguageStrings[143];
	textitems[1].text = gLanguageStrings[3];
	textitems[2].text = gLanguageStrings[4];
}

// [0x417f35] [D]
// NOTE: sub_417F35, the name is a guess
static void HighlightItem(void)
{
	textitems[flashsel].color.r = 255;
	textitems[flashsel].color.g = 0;
	textitems[flashsel].color.b = 0;
}

// [0x417f69] [D]
static void cancel_selfunc(int flag)
{
	flashsel = 1;
}

// [0x417f78] [D]
static void cancel_unselfunc(int flag)
{
	UnhighlightItem();
}

// [0x417f82] [D]
// NOTE: sub_417F82, the name is a guess
static void UnhighlightItem(void)
{
	textitems[flashsel].color.r = 255;
	textitems[flashsel].color.g = 255;
	textitems[flashsel].color.b = 255;
}

// [0x417fb6] [D]
static int cancel_choosefunc(void)
{
	retcode = 0;
	return 1;
}

// [0x417fca] [D]
static void exit_selfunc(int flag)
{
	flashsel = 2;
}

// [0x417fd9] [D]
static void exit_unselfunc(int flag)
{
	UnhighlightItem();
}

// [0x417fe3] [D]
static int exit_choosefunc(void)
{
	retcode = 1;
	return 1;
}
