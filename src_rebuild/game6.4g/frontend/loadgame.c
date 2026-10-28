/*
 * frontend/loadgame.c - load game menu: the eight save slots
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "frontend\loadgame.c", 0x4124b0 - 0x4127db)
 */

#include <string.h>
#include <dinput.h>

#include "dr_types.h"
#include "dr_globals.h"
#include "pres.h"
#include "shell.h"
#include "input.h"
#include "frontend/frontend.h"
#include "frontend/config.h"
#include "frontend/loadgame.h"

void SetOptionsLoadGame(void);
static int file_choosefunc(void);
static void file_unselfunc(int flag);
static void file1_selfunc(int flag);
static void file2_selfunc(int flag);
static void file3_selfunc(int flag);
static void file4_selfunc(int flag);
static void file5_selfunc(int flag);
static void file6_selfunc(int flag);
static void file7_selfunc(int flag);
static void file8_selfunc(int flag);
static void GetSaveGameNames(void);
static void SelectFile(void);
static void UnselectFile(void);
static void UnselectAllFiles(void);
static void ShowLoadError(void);
static void HideLoadError(void);

//------------------------------------------------------------------------------------------
// uninitialised data (0x600a18)

static int gotgame[8];
static char gamenames[8][16];
static int selection;
// NOTE: dword_600ABC; the slot under the cursor
static int current_file;
static int display_error;
// NOTE: dword_600AC4, a bss buffer: the text of the end entry of polyitems
static char poly_end_text[4];
// NOTE: dword_600AC8, dword_600ACC; empty texts of the two error lines
static char error_text1[4];
static char error_text2[4];

//------------------------------------------------------------------------------------------
// initialised data (0x54e7f8)

static FE_POLY polyitems[1] = {
	{ 0, 0, poly_end_text, 4, 0, 0, { 0, 0, 0, 0 }, 0 },
};

// [0] title, [1], [2] the error message, [3 + 2 * i] slot number, [4 + 2 * i] saved game name
static FE_TEXT textitems[20] = {
	{ 40, 150, 0, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 320, 240, 0, 1, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 320, 240, 0, 1, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 190, 200, 2, 0, 0, 0, "1.", { 255, 255, 255, 255 } },
	{ 230, 200, 2, 0, 0, 0, gamenames[0], { 255, 255, 255, 255 } },
	{ 190, 225, 2, 0, 0, 0, "2.", { 255, 255, 255, 255 } },
	{ 230, 225, 2, 0, 0, 0, gamenames[1], { 255, 255, 255, 255 } },
	{ 190, 250, 2, 0, 0, 0, "3.", { 255, 255, 255, 255 } },
	{ 230, 250, 2, 0, 0, 0, gamenames[2], { 255, 255, 255, 255 } },
	{ 190, 275, 2, 0, 0, 0, "4.", { 255, 255, 255, 255 } },
	{ 230, 275, 2, 0, 0, 0, gamenames[3], { 255, 255, 255, 255 } },
	{ 190, 300, 2, 0, 0, 0, "5.", { 255, 255, 255, 255 } },
	{ 230, 300, 2, 0, 0, 0, gamenames[4], { 255, 255, 255, 255 } },
	{ 190, 325, 2, 0, 0, 0, "6.", { 255, 255, 255, 255 } },
	{ 230, 325, 2, 0, 0, 0, gamenames[5], { 255, 255, 255, 255 } },
	{ 190, 350, 2, 0, 0, 0, "7.", { 255, 255, 255, 255 } },
	{ 230, 350, 2, 0, 0, 0, gamenames[6], { 255, 255, 255, 255 } },
	{ 190, 375, 2, 0, 0, 0, "8.", { 255, 255, 255, 255 } },
	{ 230, 375, 2, 0, 0, 0, gamenames[7], { 255, 255, 255, 255 } },
	{ -1, -1, 0, 0, 0, 0, NULL, { 0, 0, 0, 0 } },
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

// [0x4124b0] [D]
// While an error is shown, Escape or Return hides it; otherwise the menu runs.
int SelectLoadGame(void)
{
	int ret;

	SetOptionsLoadGame();
	UnselectAllFiles();

	selection = 0;
	current_file = 0;
	ResetMenu(menufunc, 0);
	OpenFrontendScreen(polyitems, textitems);

	for (;;)
	{
		HandleWinMessages();
		ReadInput();

		if (display_error)
		{
			if (CheckKey(DIK_ESCAPE) || CheckKey(DIK_RETURN))
				HideLoadError();
		}
		else
		{
			ret = HandleMenu(menufunc, &selection);

			if (ret != 0)
				break;
		}

		DrawFrontend();
	}

	if (ret != -1)
		return ret;

	CloseFrontendScreen();
	return 0;
}

// [0x412567] [D]
void SetOptionsLoadGame(void)
{
	textitems[0].text = gLanguageStrings[118];
	textitems[1].text = error_text1;
	textitems[2].text = error_text2;

	GetSaveGameNames();
}

// [0x41258f] [D]
static int file_choosefunc(void)
{
	if (gotgame[current_file])
	{
		if (LoadSavedGame(current_file))
		{
			StartMissionFromFrontend(1);
			return 1;
		}

		ShowLoadError();
	}

	return 0;
}

// [0x4125ce] [D]
static void file_unselfunc(int flag)
{
	if (flag == 0)
		UnselectFile();
}

// [0x4125de] [D]
static void file1_selfunc(int flag)
{
	current_file = 0;
	SelectFile();
}

// [0x4125f2] [D]
static void file2_selfunc(int flag)
{
	current_file = 1;
	SelectFile();
}

// [0x412606] [D]
static void file3_selfunc(int flag)
{
	current_file = 2;
	SelectFile();
}

// [0x41261a] [D]
static void file4_selfunc(int flag)
{
	current_file = 3;
	SelectFile();
}

// [0x41262e] [D]
static void file5_selfunc(int flag)
{
	current_file = 4;
	SelectFile();
}

// [0x412642] [D]
static void file6_selfunc(int flag)
{
	current_file = 5;
	SelectFile();
}

// [0x412656] [D]
static void file7_selfunc(int flag)
{
	current_file = 6;
	SelectFile();
}

// [0x41266a] [D]
static void file8_selfunc(int flag)
{
	current_file = 7;
	SelectFile();
}

// [0x41267e] [D]
// NOTE: sub_41267E, the name is a guess
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

// [0x412708] [D]
// NOTE: sub_412708, the name is a guess
static void SelectFile(void)
{
	textitems[2 * current_file + 3].font = 3;
	textitems[2 * current_file + 4].font = 3;
}

// [0x412734] [D]
// NOTE: sub_412734, the name is a guess
static void UnselectFile(void)
{
	textitems[2 * current_file + 3].font = 2;
	textitems[2 * current_file + 4].font = 2;
}

// [0x412760] [D]
// NOTE: sub_412760, the name is a guess
static void UnselectAllFiles(void)
{
	int i;

	for (i = 0; i < 8; i++)
	{
		textitems[2 * i + 3].font = 2;
		textitems[2 * i + 4].font = 2;
	}
}

// [0x4127a4] [D]
// NOTE: sub_4127A4, the name is a guess
static void ShowLoadError(void)
{
	textitems[1].text = gLanguageStrings[0];
	textitems[2].text = NULL;
	display_error = 1;
}

// [0x4127c7] [D]
// NOTE: sub_4127C7, the name is a guess
static void HideLoadError(void)
{
	display_error = 0;
	SetOptionsLoadGame();
}
