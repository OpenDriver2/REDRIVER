/*
 * flat/savegame.c - motel room save game: pick a slot and type the save name
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "motel\savegame.c=flat\savegame.c", 0x41b030 - 0x41b540)
 */

#include <string.h>

#include "dr_types.h"
#include "dr_globals.h"
#include "pres.h"
#include "shell.h"
#include "input.h"
#include "frontend/config.h"
#include "frontend/gvars.h"
#include "flat/frontend.h"
#include "flat/savegame.h"

void SetupMotelSavegame(void);
static void HighlightFile(void);
static void file_unselfunc(int flag);
static void file1_selfunc(int flag);
static void file2_selfunc(int flag);
static void file3_selfunc(int flag);
static void file4_selfunc(int flag);
static void file5_selfunc(int flag);
static void file6_selfunc(int flag);
static void file7_selfunc(int flag);
static void file8_selfunc(int flag);
static int file_choosefunc(void);
static void UnhighlightTexts(int text);
static void SaveGame(void);
static void GetSaveGameNames(void);
static void SelectFile(void);
static void UnselectFile(void);

//------------------------------------------------------------------------------------------
// uninitialised data (0x6010b0)

static int gotgame[8];
static char gamenames[8][17];
static int selection;
static int game;				// the slot under the cursor
int want_gamename;				// the name of the slot is being typed
int got_gamename;
int gamename_index;				// length of the name typed
int underscore_on;				// the cursor blinks

//------------------------------------------------------------------------------------------
// initialised data (0x550968)

// [0] title, [1 + 2 * i] slot number, [2 + 2 * i] saved game name
static FE_TEXT textitems[18] = {
	{ 40, 120, 0, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 190, 160, 0, 0, 0, 0, "1.", { 255, 255, 255, 255 } },
	{ 230, 160, 0, 0, 0, 0, gamenames[0], { 255, 255, 255, 255 } },
	{ 190, 185, 0, 0, 0, 0, "2.", { 255, 255, 255, 255 } },
	{ 230, 185, 0, 0, 0, 0, gamenames[1], { 255, 255, 255, 255 } },
	{ 190, 210, 0, 0, 0, 0, "3.", { 255, 255, 255, 255 } },
	{ 230, 210, 0, 0, 0, 0, gamenames[2], { 255, 255, 255, 255 } },
	{ 190, 235, 0, 0, 0, 0, "4.", { 255, 255, 255, 255 } },
	{ 230, 235, 0, 0, 0, 0, gamenames[3], { 255, 255, 255, 255 } },
	{ 190, 260, 0, 0, 0, 0, "5.", { 255, 255, 255, 255 } },
	{ 230, 260, 0, 0, 0, 0, gamenames[4], { 255, 255, 255, 255 } },
	{ 190, 285, 0, 0, 0, 0, "6.", { 255, 255, 255, 255 } },
	{ 230, 285, 0, 0, 0, 0, gamenames[5], { 255, 255, 255, 255 } },
	{ 190, 310, 0, 0, 0, 0, "7.", { 255, 255, 255, 255 } },
	{ 230, 310, 0, 0, 0, 0, gamenames[6], { 255, 255, 255, 255 } },
	{ 190, 335, 0, 0, 0, 0, "8.", { 255, 255, 255, 255 } },
	{ 230, 335, 0, 0, 0, 0, gamenames[7], { 255, 255, 255, 255 } },
	{ -1, -1, 0, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
};

static FE_BUTTON menufunc[9] = {
	{ file1_selfunc, file_unselfunc, file_choosefunc, 0, 0, 7, 1 },
	{ file2_selfunc, file_unselfunc, file_choosefunc, 1, 1, 0, 2 },
	{ file3_selfunc, file_unselfunc, file_choosefunc, 2, 2, 1, 3 },
	{ file4_selfunc, file_unselfunc, file_choosefunc, 3, 3, 2, 4 },
	{ file5_selfunc, file_unselfunc, file_choosefunc, 4, 4, 3, 5 },
	{ file6_selfunc, file_unselfunc, file_choosefunc, 5, 5, 4, 6 },
	{ file7_selfunc, file_unselfunc, file_choosefunc, 6, 6, 5, 7 },
	{ file8_selfunc, file_unselfunc, file_choosefunc, 7, 7, 6, 0 },
	{ NULL, NULL, NULL, 0, 0, 0, 0 },
};

static int underscore_delay = 10;

// [0x41b030] [D]
// Return on a slot starts typing its name (Escape cancels, Return saves); a blinking '.'
// is the cursor.
int DoMotelSavegame(void)
{
	int input;

	SetupMotelSavegame();

	selection = 0;
	game = 0;

	ResetMotelMenu(menufunc, 0, textitems);

	while (selection != -1)
	{
		HandleWinMessages();
		ReadInput();

		if (want_gamename)
		{
			input = GetASCIIInput();

			switch (input)
			{
				case -3:
					PlayMotelClickSound();
					want_gamename = 0;
					got_gamename = 0;
					GetSaveGameNames();
					break;
				case -2:
					if (gamename_index)
					{
						PlayMotelClickSound();
						got_gamename = 1;
						SaveGame();
					}
					break;
				case -1:
					if (gamename_index > 0)
					{
						PlayMotelClickSound();
						gamenames[game][--gamename_index] = 0;
					}
					break;
				case 0:
					break;
				default:
					if (gamename_index < 15)
					{
						PlayMotelClickSound();
						gamenames[game][gamename_index++] = input;
						gamenames[game][gamename_index] = 0;
					}
					break;
			}
		}
		else
		{
			HighlightFile();
			selection = MotelMenuKeyControl(menufunc, selection);
		}

		if (--underscore_delay < 0)
		{
			underscore_delay = 10;
			underscore_on ^= 1;
		}

		if (want_gamename && underscore_on)
		{
			gamenames[game][gamename_index] = '.';
			gamenames[game][gamename_index + 1] = 0;

			DrawMotel(textitems, NULL);

			gamenames[game][gamename_index] = 0;
		}
		else
		{
			DrawMotel(textitems, NULL);
		}
	}

	return 0;
}

// [0x41b25d] [D]
void SetupMotelSavegame(void)
{
	textitems[0].text = gLanguageStrings[148];
	GetSaveGameNames();
}

// [0x41b271] [D]
// NOTE: sub_41B271, the name is a guess
static void HighlightFile(void)
{
	if (selection == -1)
		return;

	textitems[2 * game + 1].color.r = 255;
	textitems[2 * game + 1].color.g = 0;
	textitems[2 * game + 1].color.b = 0;

	textitems[2 * game + 2].color.r = 255;
	textitems[2 * game + 2].color.g = 0;
	textitems[2 * game + 2].color.b = 0;
}

// [0x41b2f5] [D]
static void file_unselfunc(int flag)
{
	UnselectFile();
}

// [0x41b2ff] [D]
static void file1_selfunc(int flag)
{
	game = 0;
	SelectFile();
}

// [0x41b313] [D]
static void file2_selfunc(int flag)
{
	game = 1;
	SelectFile();
}

// [0x41b327] [D]
static void file3_selfunc(int flag)
{
	game = 2;
	SelectFile();
}

// [0x41b33b] [D]
static void file4_selfunc(int flag)
{
	game = 3;
	SelectFile();
}

// [0x41b34f] [D]
static void file5_selfunc(int flag)
{
	game = 4;
	SelectFile();
}

// [0x41b363] [D]
static void file6_selfunc(int flag)
{
	game = 5;
	SelectFile();
}

// [0x41b377] [D]
static void file7_selfunc(int flag)
{
	game = 6;
	SelectFile();
}

// [0x41b38b] [D]
static void file8_selfunc(int flag)
{
	game = 7;
	SelectFile();
}

// [0x41b39f] [D]
// Starts typing the name; an existing save's name is kept to edit.
static int file_choosefunc(void)
{
	UnhighlightTexts(2 * game + 1);
	want_gamename = 1;

	if (gotgame[game])
		gamename_index = strlen(gamenames[game]);
	else
		gamename_index = 0;

	return 0;
}

// [0x41b3f9] [D]
// NOTE: sub_41B3F9, the name is a guess
static void UnhighlightTexts(int text)
{
	textitems[text].color.r = 255;
	textitems[text].color.g = 255;
	textitems[text].color.b = 255;

	textitems[text + 1].color.r = 255;
	textitems[text + 1].color.g = 255;
	textitems[text + 1].color.b = 255;
}

// [0x41b455] [D]
// NOTE: sub_41B455, the name is a guess
static void SaveGame(void)
{
	want_gamename = 0;
	got_gamename = 0;
	gamename_index = 0;

	UpdateSavedGame(game, gamenames[game]);
	GetSaveGameNames();
}

// [0x41b49a] [D]
// NOTE: sub_41B49A, the name is a guess
// The name of each saved game, or the "empty" text for a free slot.
static void GetSaveGameNames(void)
{
	char name[20];
	int i;

	for (i = 0; i < 8; i++)
	{
		if (CheckSaveGame(i, name))
		{
			strcpy(gamenames[i], name);
			gotgame[i] = 1;
		}
		else
		{
			strcpy(gamenames[i], gLanguageStrings[13]);
			gotgame[i] = 0;
		}
	}
}

// [0x41b524] [D]
// NOTE: sub_41B524, the name is a guess; empty
static void SelectFile(void)
{
}

// [0x41b529] [D]
// NOTE: sub_41B529, the name is a guess
static void UnselectFile(void)
{
	UnhighlightTexts(2 * game + 1);
}
