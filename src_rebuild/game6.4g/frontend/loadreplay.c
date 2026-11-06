/*
 * frontend/loadreplay.c - load replay menu: the eight replay slots
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "frontend\loadreplay.c", 0x4128b0 - 0x412bf6)
 */

#include <string.h>
#include <dinput.h>

#include "dr_types.h"
#include "dr_globals.h"
#include "pres.h"
#include "shell.h"
#include "input.h"
#include "Director.h"
#include "frontend/frontend.h"
#include "frontend/config.h"
#include "frontend/loadreplay.h"
#include "glaunch.h"

void SetOptionsLoadRep(void);
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
static void GetReplayNames(void);
static void SelectFile(void);
static void UnselectFile(void);
static void UnselectAllFiles(void);
static void ShowLoadError(void);
static void HideLoadError(void);

//------------------------------------------------------------------------------------------
// uninitialised data (0x600af4)

int gotreplay[8];
char replaynames[8][16];
static int selection;
int replay;					// the slot under the cursor
static int display_error;
// NOTE: byte_600BA0, a bss buffer: the text of the end entry of polyitems
static char poly_end_text[4];
// NOTE: byte_600BA4, byte_600BA8; empty texts of the two error lines
static char error_text1[4];
static char error_text2[4];

//------------------------------------------------------------------------------------------
// initialised data (0x54ea68)

static FE_POLY polyitems[1] = {
	{ 0, 0, poly_end_text, 4, 0, 0, { 0, 0, 0, 0 }, 0 },
};

// [0] title, [1], [2] the error message, [3 + 2 * i] slot number, [4 + 2 * i] replay name
static FE_TEXT textitems[20] = {
	{ 40, 150, 0, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 320, 240, 0, 1, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 320, 240, 0, 1, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 190, 200, 2, 0, 0, 0, "1.", { 255, 255, 255, 255 } },
	{ 230, 200, 2, 0, 0, 0, replaynames[0], { 255, 255, 255, 255 } },
	{ 190, 225, 2, 0, 0, 0, "2.", { 255, 255, 255, 255 } },
	{ 230, 225, 2, 0, 0, 0, replaynames[1], { 255, 255, 255, 255 } },
	{ 190, 250, 2, 0, 0, 0, "3.", { 255, 255, 255, 255 } },
	{ 230, 250, 2, 0, 0, 0, replaynames[2], { 255, 255, 255, 255 } },
	{ 190, 275, 2, 0, 0, 0, "4.", { 255, 255, 255, 255 } },
	{ 230, 275, 2, 0, 0, 0, replaynames[3], { 255, 255, 255, 255 } },
	{ 190, 300, 2, 0, 0, 0, "5.", { 255, 255, 255, 255 } },
	{ 230, 300, 2, 0, 0, 0, replaynames[4], { 255, 255, 255, 255 } },
	{ 190, 325, 2, 0, 0, 0, "6.", { 255, 255, 255, 255 } },
	{ 230, 325, 2, 0, 0, 0, replaynames[5], { 255, 255, 255, 255 } },
	{ 190, 350, 2, 0, 0, 0, "7.", { 255, 255, 255, 255 } },
	{ 230, 350, 2, 0, 0, 0, replaynames[6], { 255, 255, 255, 255 } },
	{ 190, 375, 2, 0, 0, 0, "8.", { 255, 255, 255, 255 } },
	{ 230, 375, 2, 0, 0, 0, replaynames[7], { 255, 255, 255, 255 } },
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

// [0x4128b0] [D]
// While an error is shown, Escape or Return hides it; otherwise the menu runs.
int SelectLoadReplay(void)
{
	int ret;

	SetOptionsLoadRep();
	UnselectAllFiles();

	selection = 0;
	replay = 0;
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

// [0x412967] [D]
void SetOptionsLoadRep(void)
{
	textitems[0].text = gLanguageStrings[119];
	textitems[1].text = error_text1;
	textitems[2].text = error_text2;

	GetReplayNames();
}

// [0x41298f] [D]
static int file_choosefunc(void)
{
	if (gotreplay[replay])
	{
		if (LoadSavedReplay(replay))
		{
			CloseFrontendScreen();

			Director.DoingReplay = 1;
			StartGameFromFrontend(1, 1);
			Director.DoingReplay = 0;

			return 1;
		}

		ShowLoadError();
	}

	return 0;
}

// [0x4129e9] [D]
static void file_unselfunc(int flag)
{
	if (flag == 0)
		UnselectFile();
}

// [0x4129f9] [D]
static void file1_selfunc(int flag)
{
	replay = 0;
	SelectFile();
}

// [0x412a0d] [D]
static void file2_selfunc(int flag)
{
	replay = 1;
	SelectFile();
}

// [0x412a21] [D]
static void file3_selfunc(int flag)
{
	replay = 2;
	SelectFile();
}

// [0x412a35] [D]
static void file4_selfunc(int flag)
{
	replay = 3;
	SelectFile();
}

// [0x412a49] [D]
static void file5_selfunc(int flag)
{
	replay = 4;
	SelectFile();
}

// [0x412a5d] [D]
static void file6_selfunc(int flag)
{
	replay = 5;
	SelectFile();
}

// [0x412a71] [D]
static void file7_selfunc(int flag)
{
	replay = 6;
	SelectFile();
}

// [0x412a85] [D]
static void file8_selfunc(int flag)
{
	replay = 7;
	SelectFile();
}

// [0x412a99] [D]
// NOTE: sub_412A99, the name is a guess
// The name of each saved replay, or the "empty" text for a free slot.
static void GetReplayNames(void)
{
	char name[32];
	int i;

	for (i = 0; i < 8; i++)
	{
		if (CheckReplay(i, name))
		{
			strcpy(replaynames[i], name);
			gotreplay[i] = 1;
		}
		else
		{
			strcpy(replaynames[i], gLanguageStrings[13]);
			gotreplay[i] = 0;
		}
	}
}

// [0x412b23] [D]
// NOTE: sub_412B23, the name is a guess
static void SelectFile(void)
{
	textitems[2 * replay + 3].font = 3;
	textitems[2 * replay + 4].font = 3;
}

// [0x412b4f] [D]
// NOTE: sub_412B4F, the name is a guess
static void UnselectFile(void)
{
	textitems[2 * replay + 3].font = 2;
	textitems[2 * replay + 4].font = 2;
}

// [0x412b7b] [D]
// NOTE: sub_412B7B, the name is a guess
static void UnselectAllFiles(void)
{
	int i;

	for (i = 0; i < 8; i++)
	{
		textitems[2 * i + 3].font = 2;
		textitems[2 * i + 4].font = 2;
	}
}

// [0x412bbf] [D]
// NOTE: sub_412BBF, the name is a guess
static void ShowLoadError(void)
{
	textitems[1].text = gLanguageStrings[0];
	textitems[2].text = NULL;
	display_error = 1;
}

// [0x412be2] [D]
// NOTE: sub_412BE2, the name is a guess
static void HideLoadError(void)
{
	display_error = 0;
	SetOptionsLoadRep();
}
