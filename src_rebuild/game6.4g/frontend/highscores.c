/*
 * frontend/highscores.c - high score tables, paged with previous / next
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "frontend\highscores.c=frontend\highscore.c",
 * 0x40f610 - 0x40fbf5)
 */

#include <stdio.h>

#include "dr_types.h"
#include "dr_globals.h"
#include "pres.h"
#include "shell.h"
#include "input.h"
#include "frontend/frontend.h"
#include "frontend/highscores.h"
#include "dr_scores.h"

void SetOptionsHighScore(void);
static void prev_selfunc(int flag);
static void prev_unselfunc(int flag);
static int prev_choosefunc(void);
static void next_selfunc(int flag);
static void next_unselfunc(int flag);
static int next_choosefunc(void);

//------------------------------------------------------------------------------------------
// uninitialised data (0x600764)

char caption[64];
char misctext2[5][32];		// score or time of each entry
char misctext3[32];			// time trial: the best lap holder
char misctext1[5][32];		// trailblazer: cones of each entry
int max_tables;
char misctext4[32];			// time trial: the best lap time
static int selection;
int highscore_index;
// NOTE: byte_600930, a bss buffer: the text of the end entry of polyitems
static char poly_end_text[4];
// NOTE: byte_600934 - byte_600964, empty bss texts (one per use, in address order)
static char empty_text[13][4];

//------------------------------------------------------------------------------------------
// initialised data (0x54dde0)

static FE_POLY polyitems[1] = {
	{ 0, 0, poly_end_text, 4, 0, 0, { 0, 0, 0, 0 }, 0 },
};

// [0] caption, [1 + 4 * i] position, name, time / cones, score of the five entries,
// [21] previous, [22] "/", [23] next, [24 - 26] column titles, [27], [28] the best lap
static FE_TEXT textitems[30] = {
	{ 40, 150, 0, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 80, 210, 2, 0, 0, 0, "1:", { 255, 255, 255, 255 } },
	{ 105, 210, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 350, 210, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 560, 210, 2, 2, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 80, 240, 2, 0, 0, 0, "2:", { 255, 255, 255, 255 } },
	{ 105, 240, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 350, 240, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 560, 240, 2, 2, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 80, 270, 2, 0, 0, 0, "3:", { 255, 255, 255, 255 } },
	{ 105, 270, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 350, 270, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 560, 270, 2, 2, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 80, 300, 2, 0, 0, 0, "4:", { 255, 255, 255, 255 } },
	{ 105, 300, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 350, 300, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 560, 300, 2, 2, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 80, 330, 2, 0, 0, 0, "5:", { 255, 255, 255, 255 } },
	{ 105, 330, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 350, 330, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 560, 330, 2, 2, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 310, 400, 2, 2, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 310, 400, 2, 0, 0, 0, "/", { 255, 255, 255, 255 } },
	{ 330, 400, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 80, 185, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 350, 185, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 560, 185, 2, 2, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 80, 370, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 560, 370, 2, 2, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ -1, -1, 0, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
};

static FE_BUTTON menuitems[3] = {
	{ next_selfunc, next_unselfunc, next_choosefunc, 1, 1, 0, 0 },
	{ prev_selfunc, prev_unselfunc, prev_choosefunc, 0, 0, 1, 1 },
	{ NULL, NULL, NULL, 0, 0, 0, 0 },
};

GAMETYPE highscore_type = GAME_TRAILBLAZER;

// [0x40f610] [D]
int SelectShowHighscore(void)
{
	int ret;

	SetOptionsHighScore();

	selection = 0;
	ResetMenu(menuitems, 0);
	OpenFrontendScreen(polyitems, textitems);

	for (;;)
	{
		HandleWinMessages();
		ReadInput();

		ret = HandleMenu(menuitems, &selection);

		if (ret != 0)
			break;

		DrawFrontend();
	}

	if (ret != -1)
		return ret;

	CloseFrontendScreen();
	return 0;
}

// [0x40f68c] [D]
static void prev_selfunc(int flag)
{
	textitems[21].font = 3;
}

// [0x40f698] [D]
static void prev_unselfunc(int flag)
{
	textitems[21].font = 2;
}

// [0x40f6a4] [D]
static int prev_choosefunc(void)
{
	if (--highscore_index < 0)
		highscore_index = max_tables - 1;

	SetOptionsHighScore();
	return 0;
}

// [0x40f6d5] [D]
static void next_selfunc(int flag)
{
	textitems[23].font = 3;
}

// [0x40f6e1] [D]
static void next_unselfunc(int flag)
{
	textitems[23].font = 2;
}

// [0x40f6ed] [D]
static int next_choosefunc(void)
{
	if (++highscore_index >= max_tables)
		highscore_index = 0;

	SetOptionsHighScore();
	return 0;
}

// [0x40f71e] [D]
// Fills the texts with the table highscore_index of the game highscore_type.
void SetOptionsHighScore(void)
{
	HIGHSCORE_DATA *table;
	int game_name;
	int table_name;
	int show_items;
	int show_time;
	int i;

	switch (highscore_type)
	{
		case GAME_TRAILBLAZER:
			game_name = 29;
			table_name = highscore_index + 122;
			max_tables = 8;
			show_items = 1;
			show_time = 1;
			textitems[24].text = gLanguageStrings[33];
			textitems[25].text = gLanguageStrings[35];
			textitems[26].text = gLanguageStrings[36];
			textitems[27].text = empty_text[3];
			textitems[28].text = empty_text[4];
			break;
		case GAME_CHECKPOINT:
			game_name = 28;
			table_name = highscore_index + 122;
			max_tables = 8;
			show_items = 0;
			show_time = 1;
			textitems[24].text = gLanguageStrings[33];
			textitems[25].text = empty_text[0];
			textitems[26].text = gLanguageStrings[36];
			textitems[27].text = empty_text[1];
			textitems[28].text = empty_text[2];
			break;
		case GAME_SURVIVAL:
			game_name = 30;
			table_name = highscore_index + 130;
			max_tables = 4;
			show_items = 0;
			show_time = 1;
			textitems[24].text = gLanguageStrings[33];
			textitems[25].text = empty_text[5];
			textitems[26].text = gLanguageStrings[36];
			textitems[27].text = empty_text[6];
			textitems[28].text = empty_text[7];
			break;
		case GAME_CARNAGE:
			game_name = 32;
			table_name = highscore_index + 130;
			max_tables = 4;
			show_items = 0;
			show_time = 0;
			textitems[24].text = gLanguageStrings[33];
			textitems[25].text = empty_text[9];
			textitems[26].text = gLanguageStrings[34];
			textitems[27].text = empty_text[10];
			textitems[28].text = empty_text[11];
			break;
		case GAME_TIMETRIAL:
			game_name = 31;
			table_name = highscore_index + 134;
			max_tables = 4;
			show_items = 0;
			show_time = 1;
			textitems[24].text = gLanguageStrings[33];
			textitems[25].text = empty_text[8];
			textitems[26].text = gLanguageStrings[36];

			// the best lap is kept in the GAME_BESTLAP table
			table = GetHighScoreTable(GAME_BESTLAP, highscore_index);
			sprintf(misctext3, "%s: %s", gLanguageStrings[212], table->name);

			if (table->lap_info.flags == 8)
				sprintf(misctext4, "%d:%02d:%02d", table->lap_info.min, table->lap_info.sec, table->lap_info.frac);
			else
				sprintf(misctext4, "--:--:--");

			textitems[27].text = misctext3;
			textitems[28].text = misctext4;
			break;
	}

	// NOTE: game_name and table_name are left unset for the other game types (as in the original)
	sprintf(caption, "%s %s", gLanguageStrings[game_name], gLanguageStrings[table_name]);
	textitems[0].text = caption;
	textitems[23].text = gLanguageStrings[26];
	textitems[21].text = gLanguageStrings[27];

	table = GetHighScoreTable(highscore_type, highscore_index);

	for (i = 0; i < 5; i++, table++)
	{
		textitems[4 * i + 2].text = table->name;
		textitems[4 * i + 3].text = empty_text[12];

		if (show_time)
		{
			if (table->lap_info.flags == 8)
				sprintf(misctext2[i], "%02d:%02d:%02d", table->lap_info.min, table->lap_info.sec, table->lap_info.frac);
			else
				sprintf(misctext2[i], "--:--:--");
		}
		else if (table->lap_info.flags == 8)
		{
			if (highscore_type == GAME_CARNAGE)
				sprintf(misctext2[i], "$%d", table->items);
			else
				sprintf(misctext2[i], "%d", table->items);
		}
		else
		{
			sprintf(misctext2[i], "-");
		}

		textitems[4 * i + 4].text = misctext2[i];

		if (show_items)
		{
			if (table->lap_info.flags == 8)
			{
				sprintf(misctext1[i], "%d", table->items);
				textitems[4 * i + 3].text = misctext1[i];
			}
			else
			{
				textitems[4 * i + 3].text = "-";
			}
		}
	}
}
