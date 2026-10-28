/*
 * frontend/selrace.c - race selection: city (and race) of a driving game, high score entry
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "frontend\selectrace.c=frontend\selrace.c",
 * 0x414250 - 0x414e1b)
 */

#include <string.h>

#include "dr_types.h"
#include "dr_globals.h"
#include "pres.h"
#include "shell.h"
#include "input.h"
#include "frontend/font.h"
#include "frontend/frontend.h"
#include "frontend/highscores.h"
#include "frontend/selrace.h"
#include "dr_scores.h"
#include "frontend/name.h"

void SetOptionsRace(void);
static void miami1_selfunc(int flag);
static void miami1_unselfunc(int flag);
static void miami2_selfunc(int flag);
static void miami2_unselfunc(int flag);
static void sanfran1_selfunc(int flag);
static void sanfran1_unselfunc(int flag);
static void sanfran2_selfunc(int flag);
static void sanfran2_unselfunc(int flag);
static void losang1_selfunc(int flag);
static void losang1_unselfunc(int flag);
static void losang2_selfunc(int flag);
static void losang2_unselfunc(int flag);
static void newyork1_selfunc(int flag);
static void newyork1_unselfunc(int flag);
static void newyork2_selfunc(int flag);
static void newyork2_unselfunc(int flag);
static int city_choosefunc(void);
void AddScore(void);
static void SetGreyedItems(int first, int last);

//------------------------------------------------------------------------------------------
// uninitialised data (0x600c20)

static FE_POLY *cur_polyitems;		// NULL: no city picture
static FE_BUTTON *cur_menu;
static FE_TEXT *cur_textitems;
static int menumode;				// 0: one race per city, 1: two races per city, 2: one, centred, no picture
static int selection;
// NOTE: dword_600C34, a bss buffer: the city picture shown before a city is selected (empty);
// IDA marks it @frontend\soundoptions.c, but it is only used here, right after selection
static char poly_text[4];

//------------------------------------------------------------------------------------------
// initialised data (0x54f5e8)

static FE_POLY polyitems[2] = {
	{ 327, 182, poly_text, 1, 0, 0, { -1, -1, -1, -1 }, 0 },
	{ 0, 0, NULL, 4, 0, 0, { 0, 0, 0, 0 }, 0 },
};

static FE_TEXT textitems_1[6] = {
	{ 40, 150, 0, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 26, 230, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 26, 260, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 26, 290, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 26, 320, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ -1, -1, 0, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
};

static FE_TEXT textitems_1a[6] = {
	{ 40, 150, 0, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 320, 230, 2, 1, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 320, 260, 2, 1, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 320, 290, 2, 1, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 320, 320, 2, 1, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ -1, -1, 0, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
};

// one race per city, for 1 - 4 cities open
static FE_BUTTON menufunc1_1[2] = {
	{ miami1_selfunc, miami1_unselfunc, city_choosefunc, 0, 0, 0, 0 },
	{ NULL, NULL, NULL, 0, 0, 0, 0 },
};

static FE_BUTTON menufunc2_1[3] = {
	{ miami1_selfunc, miami1_unselfunc, city_choosefunc, 0, 0, 1, 1 },
	{ sanfran1_selfunc, sanfran1_unselfunc, city_choosefunc, 1, 1, 0, 0 },
	{ NULL, NULL, NULL, 0, 0, 0, 0 },
};

static FE_BUTTON menufunc3_1[4] = {
	{ miami1_selfunc, miami1_unselfunc, city_choosefunc, 0, 0, 2, 1 },
	{ sanfran1_selfunc, sanfran1_unselfunc, city_choosefunc, 1, 1, 0, 2 },
	{ losang1_selfunc, losang1_unselfunc, city_choosefunc, 2, 2, 1, 0 },
	{ NULL, NULL, NULL, 0, 0, 0, 0 },
};

static FE_BUTTON menufunc4_1[5] = {
	{ miami1_selfunc, miami1_unselfunc, city_choosefunc, 0, 0, 3, 1 },
	{ sanfran1_selfunc, sanfran1_unselfunc, city_choosefunc, 1, 1, 0, 2 },
	{ losang1_selfunc, losang1_unselfunc, city_choosefunc, 2, 2, 1, 3 },
	{ newyork1_selfunc, newyork1_unselfunc, city_choosefunc, 3, 3, 2, 0 },
	{ NULL, NULL, NULL, 0, 0, 0, 0 },
};

static FE_TEXT textitems_2[10] = {
	{ 40, 150, 0, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 26, 195, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 26, 220, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 26, 245, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 26, 270, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 26, 295, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 26, 320, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 26, 345, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 26, 370, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ -1, -1, 0, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
};

// two races per city, for 1 - 4 cities open
static FE_BUTTON menufunc1_2[3] = {
	{ miami1_selfunc, miami1_unselfunc, city_choosefunc, 0, 0, 1, 1 },
	{ miami2_selfunc, miami2_unselfunc, city_choosefunc, 1, 1, 0, 0 },
	{ NULL, NULL, NULL, 0, 0, 0, 0 },
};

static FE_BUTTON menufunc2_2[5] = {
	{ miami1_selfunc, miami1_unselfunc, city_choosefunc, 0, 0, 3, 1 },
	{ miami2_selfunc, miami2_unselfunc, city_choosefunc, 1, 1, 0, 2 },
	{ sanfran1_selfunc, sanfran1_unselfunc, city_choosefunc, 2, 2, 1, 3 },
	{ sanfran2_selfunc, sanfran2_unselfunc, city_choosefunc, 3, 3, 2, 0 },
	{ NULL, NULL, NULL, 0, 0, 0, 0 },
};

static FE_BUTTON menufunc3_2[7] = {
	{ miami1_selfunc, miami1_unselfunc, city_choosefunc, 0, 0, 5, 1 },
	{ miami2_selfunc, miami2_unselfunc, city_choosefunc, 1, 1, 0, 2 },
	{ sanfran1_selfunc, sanfran1_unselfunc, city_choosefunc, 2, 2, 1, 3 },
	{ sanfran2_selfunc, sanfran2_unselfunc, city_choosefunc, 3, 3, 2, 4 },
	{ losang1_selfunc, losang1_unselfunc, city_choosefunc, 4, 4, 3, 5 },
	{ losang2_selfunc, losang2_unselfunc, city_choosefunc, 5, 5, 4, 0 },
	{ NULL, NULL, NULL, 0, 0, 0, 0 },
};

static FE_BUTTON menufunc4_2[9] = {
	{ miami1_selfunc, miami1_unselfunc, city_choosefunc, 0, 0, 7, 1 },
	{ miami2_selfunc, miami2_unselfunc, city_choosefunc, 1, 1, 0, 2 },
	{ sanfran1_selfunc, sanfran1_unselfunc, city_choosefunc, 2, 2, 1, 3 },
	{ sanfran2_selfunc, sanfran2_unselfunc, city_choosefunc, 3, 3, 2, 4 },
	{ losang1_selfunc, losang1_unselfunc, city_choosefunc, 4, 4, 3, 5 },
	{ losang2_selfunc, losang2_unselfunc, city_choosefunc, 5, 5, 4, 6 },
	{ newyork1_selfunc, newyork1_unselfunc, city_choosefunc, 6, 6, 5, 7 },
	{ newyork2_selfunc, newyork2_unselfunc, city_choosefunc, 7, 7, 6, 0 },
	{ NULL, NULL, NULL, 0, 0, 0, 0 },
};

// GameMode of each game type (GAMETYPE) and race (global_score.index)
int mission_lookup[12][8] = {
	{ 0, 0, 0, 0, 0, 0, 0, 0 },
	{ 59, 91, 62, 93, 61, 92, 63, 94 },			// GAME_TRAILBLAZER
	{ 71, 72, 73, 74, 75, 76, 77, 78 },			// GAME_CHECKPOINT
	{ 45, 81, 83, 85, 0, 0, 0, 0 },				// GAME_SURVIVAL
	{ 95, 96, 97, 98, 99, 100, 101, 102 },		// GAME_PURSUIT
	{ 66, 87, 67, 88, 68, 89, 69, 90 },			// GAME_GETAWAY
	{ 253, 254, 255, 256, 0, 0, 0, 0 },			// GAME_DIRTTRACK
	{ 111, 113, 115, 117, 0, 0, 0, 0 },			// GAME_CARNAGE
	{ 0, 0, 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0, 0 },
	{ 257, 258, 259, 260, 0, 0, 0, 0 },			// GAME_TIMETRIAL
};

int want_score = 1;

// [0x414250] [D]
int SelectRace(void)
{
	int ret;

	SetOptionsRace();

	selection = 0;
	ResetMenu(cur_menu, 0);
	OpenFrontendScreen(cur_polyitems, cur_textitems);

	for (;;)
	{
		HandleWinMessages();
		ReadInput();

		ret = HandleMenu(cur_menu, &selection);

		if (ret != 0)
			break;

		DrawFrontend();
	}

	if (ret != -1)
		return ret;

	CloseFrontendScreen();
	return 0;
}

// [0x4142d3] [D]
// A score made with these cheats is not entered in the high score tables.
int CheckIfCheating(void)
{
	return gCheatFlags.SuperFastCars
		|| gCheatFlags.Immunity
		|| gCheatFlags.Invincibility
		|| (gCheatFlags.FreightTrain && global_score.type != GAME_CARNAGE)
		|| gCheatFlags.RailroadCops;
}

// [0x414317] [D]
// Picks the layout for the game type and the menu for the number of cities open; the races of
// the cities not open yet are greyed out.
void SetOptionsRace(void)
{
	int name;

	switch (global_score.type)
	{
		case GAME_TRAILBLAZER:
		case GAME_CHECKPOINT:
		case GAME_PURSUIT:
		case GAME_GETAWAY:
			menumode = 1;
			cur_textitems = textitems_2;
			cur_polyitems = polyitems;
			name = 122;
			ResetMenu(menufunc4_2, 0);
			break;
		case GAME_SURVIVAL:
		case GAME_CARNAGE:
			menumode = 0;
			cur_textitems = textitems_1;
			cur_polyitems = polyitems;
			name = 130;
			ResetMenu(menufunc4_1, 0);
			break;
		case GAME_DIRTTRACK:
		case GAME_TIMETRIAL:
			menumode = 2;
			cur_textitems = textitems_1a;
			cur_polyitems = NULL;
			name = 134;
			ResetMenu(menufunc4_1, 0);
			break;
	}

	// NOTE: name is left unset for the other game types (as in the original)
	if (menumode == 1)
	{
		switch (gNumCitiesOpen)
		{
			default:
				gNumCitiesOpen = 1;
				// fall through
			case 1:
				cur_menu = menufunc1_2;
				SetGreyedItems(3, 8);
				break;
			case 2:
				cur_menu = menufunc2_2;
				SetGreyedItems(5, 8);
				break;
			case 3:
				cur_menu = menufunc3_2;
				SetGreyedItems(7, 8);
				break;
			case 4:
				cur_menu = menufunc4_2;
				SetGreyedItems(9, 8);
				break;
		}

		cur_textitems[0].text = gLanguageStrings[111];
		cur_textitems[1].text = gLanguageStrings[name];
		cur_textitems[2].text = gLanguageStrings[name + 1];
		cur_textitems[3].text = gLanguageStrings[name + 2];
		cur_textitems[4].text = gLanguageStrings[name + 3];
		cur_textitems[5].text = gLanguageStrings[name + 4];
		cur_textitems[6].text = gLanguageStrings[name + 5];
		cur_textitems[7].text = gLanguageStrings[name + 6];
		cur_textitems[8].text = gLanguageStrings[name + 7];
	}
	else
	{
		switch (gNumCitiesOpen)
		{
			default:
				gNumCitiesOpen = 1;
				// fall through
			case 1:
				cur_menu = menufunc1_1;
				SetGreyedItems(2, 4);
				break;
			case 2:
				cur_menu = menufunc2_1;
				SetGreyedItems(3, 4);
				break;
			case 3:
				cur_menu = menufunc3_1;
				SetGreyedItems(4, 4);
				break;
			case 4:
				cur_menu = menufunc4_1;
				SetGreyedItems(5, 4);
				break;
		}

		cur_textitems[0].text = gLanguageStrings[111];
		cur_textitems[1].text = gLanguageStrings[name];
		cur_textitems[2].text = gLanguageStrings[name + 1];
		cur_textitems[3].text = gLanguageStrings[name + 2];
		cur_textitems[4].text = gLanguageStrings[name + 3];
	}
}

// [0x4146ca] [D]
static void miami1_selfunc(int flag)
{
	if (cur_polyitems)
	{
		cur_polyitems[0].text = "MIAMI";
		cur_polyitems[0].x = 327;
		cur_polyitems[0].y = 182;
	}

	switch (menumode)
	{
		case 0:
		case 2:
		case 1:
			cur_textitems[1].font = 3;
			global_score.index = 0;
			break;
	}
}

// [0x414744] [D]
static void miami1_unselfunc(int flag)
{
	if (flag)
		return;

	switch (menumode)
	{
		case 0:
		case 2:
		case 1:
			cur_textitems[1].font = 2;
			break;
	}
}

// [0x414784] [D]
static void miami2_selfunc(int flag)
{
	if (cur_polyitems)
	{
		cur_polyitems[0].text = "MIAMI";
		cur_polyitems[0].x = 327;
		cur_polyitems[0].y = 182;
	}

	cur_textitems[2].font = 3;
	global_score.index = 1;
}

// [0x4147c8] [D]
static void miami2_unselfunc(int flag)
{
	if (flag == 0)
		cur_textitems[2].font = 2;
}

// [0x4147dc] [D]
static void sanfran1_selfunc(int flag)
{
	if (cur_polyitems)
	{
		cur_polyitems[0].text = "SF";
		cur_polyitems[0].x = 328;
		cur_polyitems[0].y = 154;
	}

	switch (menumode)
	{
		case 0:
		case 2:
			cur_textitems[2].font = 3;
			global_score.index = 1;
			break;
		case 1:
			cur_textitems[3].font = 3;
			global_score.index = 2;
			break;
	}
}

// [0x414856] [D]
static void sanfran1_unselfunc(int flag)
{
	if (flag)
		return;

	switch (menumode)
	{
		case 0:
		case 2:
			cur_textitems[2].font = 2;
			break;
		case 1:
			cur_textitems[3].font = 2;
			break;
	}
}

// [0x414896] [D]
static void sanfran2_selfunc(int flag)
{
	if (cur_polyitems)
	{
		cur_polyitems[0].text = "SF";
		cur_polyitems[0].x = 328;
		cur_polyitems[0].y = 154;
	}

	cur_textitems[4].font = 3;
	global_score.index = 3;
}

// [0x4148da] [D]
static void sanfran2_unselfunc(int flag)
{
	if (flag == 0)
		cur_textitems[4].font = 2;
}

// [0x4148ee] [D]
static void losang1_selfunc(int flag)
{
	if (cur_polyitems)
	{
		cur_polyitems[0].text = "LA";
		cur_polyitems[0].x = 328;
		cur_polyitems[0].y = 200;
	}

	switch (menumode)
	{
		case 0:
		case 2:
			cur_textitems[3].font = 3;
			global_score.index = 2;
			break;
		case 1:
			cur_textitems[5].font = 3;
			global_score.index = 4;
			break;
	}
}

// [0x414968] [D]
static void losang1_unselfunc(int flag)
{
	if (flag)
		return;

	switch (menumode)
	{
		case 0:
		case 2:
			cur_textitems[3].font = 2;
			break;
		case 1:
			cur_textitems[5].font = 2;
			break;
	}
}

// [0x4149a8] [D]
static void losang2_selfunc(int flag)
{
	if (cur_polyitems)
	{
		cur_polyitems[0].text = "LA";
		cur_polyitems[0].x = 328;
		cur_polyitems[0].y = 200;
	}

	cur_textitems[6].font = 3;
	global_score.index = 5;
}

// [0x4149ec] [D]
static void losang2_unselfunc(int flag)
{
	if (flag == 0)
		cur_textitems[6].font = 2;
}

// [0x414a00] [D]
static void newyork1_selfunc(int flag)
{
	if (cur_polyitems)
	{
		cur_polyitems[0].text = "NY";
		cur_polyitems[0].x = 327;
		cur_polyitems[0].y = 138;
	}

	switch (menumode)
	{
		case 0:
		case 2:
			cur_textitems[4].font = 3;
			global_score.index = 3;
			break;
		case 1:
			cur_textitems[7].font = 3;
			global_score.index = 6;
			break;
	}
}

// [0x414a7a] [D]
static void newyork1_unselfunc(int flag)
{
	if (flag)
		return;

	switch (menumode)
	{
		case 0:
		case 2:
			cur_textitems[4].font = 2;
			break;
		case 1:
			cur_textitems[7].font = 2;
			break;
	}
}

// [0x414aba] [D]
static void newyork2_selfunc(int flag)
{
	if (cur_polyitems)
	{
		cur_polyitems[0].text = "NY";
		cur_polyitems[0].x = 327;
		cur_polyitems[0].y = 138;
	}

	cur_textitems[8].font = 3;
	global_score.index = 7;
}

// [0x414b01] [D]
static void newyork2_unselfunc(int flag)
{
	if (flag == 0)
		cur_textitems[8].font = 2;
}

// [0x414b18] [D]
// Plays the race; the game types with high score tables then offer the score entry.
static int city_choosefunc(void)
{
	CloseFrontendScreen();

	switch (global_score.type)
	{
		case GAME_TRAILBLAZER:
		case GAME_CHECKPOINT:
		case GAME_SURVIVAL:
		case GAME_CARNAGE:
		case GAME_TIMETRIAL:
			GameMode = mission_lookup[global_score.type][global_score.index];
			StartGameFromFrontend(1, 0);

			if (want_score)
				AddScore();
			break;
		case GAME_PURSUIT:
		case GAME_GETAWAY:
		case GAME_DIRTTRACK:
			GameMode = mission_lookup[global_score.type][global_score.index];
			StartGameFromFrontend(1, 0);
			break;
	}

	return 1;
}

// [0x414bdb] [D]
// Enters global_score in its high score table and the best lap in the best lap table if they
// made it, asking for the player's name, then shows the table.
void AddScore(void)
{
	HIGHSCORE_DATA *score;
	HIGHSCORE_DATA *best_lap;
	char playername[12];
	int scorepos;
	int i;

	score = NULL;
	best_lap = NULL;

	if (CheckIfCheating())
		return;

	score = CheckForRecord(&scorepos, 0);

	if (score)
	{
		SetupEnterPlayerName(playername, 10);

		for (i = 4; i > scorepos; i--)
			score[i] = score[i - 1];

		strcpy(score[i].name, playername);
		score[i].items = global_score.items;
		score[i].lap_info = global_score.lap_info;
		score[i].lap_info.flags = 8;

		best_lap = CheckForRecord(&scorepos, 1);

		if (best_lap)
		{
			strcpy(best_lap->name, playername);
			best_lap->items = 0;
			best_lap->lap_info = global_score.best_lap;
			best_lap->lap_info.flags = 8;
		}
	}
	else
	{
		score = CheckForRecord(&scorepos, 1);

		if (score)
		{
			SetupEnterPlayerName(playername, 10);

			strcpy(score->name, playername);
			score->items = 0;
			score->lap_info = global_score.best_lap;
			score->lap_info.flags = 8;
		}
	}

	if (score || best_lap)
	{
		highscore_type = global_score.type;
		highscore_index = global_score.index;
		SelectShowHighscore();
	}
}

// [0x414d99] [D]
// NOTE: sub_414D99, the name is a guess (like takearide.c's SetGreyedItems)
// Items first .. last are drawn translucent (greyed out), the ones before them normally.
static void SetGreyedItems(int first, int last)
{
	int i;

	for (i = 0; i < first; i++)
		cur_textitems[i].justify &= ~PRINT_TRANSLUCENT;

	for (i = first; i <= last; i++)
		cur_textitems[i].justify |= PRINT_TRANSLUCENT;
}
