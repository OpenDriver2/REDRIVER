/*
 * frontend/name.c - player name entry and the cheat names
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "frontend\name.c", 0x413680 - 0x4138dd)
 */

#include <string.h>

#include "dr_types.h"
#include "dr_globals.h"
#include "pres.h"
#include "shell.h"
#include "input.h"
#include "frontend/frontend.h"
#include "frontend/font.h"
#include "frontend/gvars.h"
#include "frontend/config.h"
#include "frontend/name.h"
#include "frontend/cheats.h"

void CheckCheatName(char *name);

typedef struct CHEAT_NAME
{
	const char *name;
	int *cheat;
} CHEAT_NAME;

//------------------------------------------------------------------------------------------
// uninitialised data (0x600bec)

static char cursor_text[4];		// "_" while the cursor blinks on
static int name_length;
static int blink_timer;
static int blink_on;
// NOTE: dword_600BFC, a bss buffer: the text of the end entry of polyitems
static char poly_end_text[4];

//------------------------------------------------------------------------------------------
// initialised data (0x54f0c0)

static FE_POLY polyitems[1] = {
	{ 0, 0, poly_end_text, 4, 0, 0, { 0, 0, 0, 0 }, 0 },
};

static FE_TEXT textitems[4] = {
	{ 40, 150, 0, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 320, 200, 2, 1, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 320, 200, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ -1, -1, 0, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
};

// a name entered as the player name unlocks the cheat
static CHEAT_NAME cheat_names[12] = {
	{ "NJW280172", &gCheatsAllowed[0] },
	{ "WAC271074", &gCheatsAllowed[1] },
	{ "RUS3L", &gCheatsAllowed[2] },
	{ "D0UG1As", &gCheatsAllowed[3] },
	{ "N3WJ0rK", &gCheatsAllowed[4] },
	{ "S3l443w", &gCheatsAllowed[6] },
	{ "P7ttyRz", &gCheatsAllowed[7] },
	{ "C1J2P3", &gCheatsAllowed[8] },
	{ "TMR300866", &gCheatsAllowed[9] },
	{ "We4rrqdz", &gCheatsAllowed[5] },
	{ "ANJW16696", &gCheatsAllowed[10] },
	{ NULL, NULL },
};

// [0x413680] [D]
// Lets the player type a name of up to max_length characters into buffer; an empty name becomes
// the default name. The name may unlock cheats.
int SetupEnterPlayerName(char *buffer, int max_length)
{
	int input;
	int editing;

	editing = 1;

	textitems[0].text = gLanguageStrings[120];
	textitems[1].text = buffer;
	textitems[2].text = cursor_text;

	*buffer = 0;
	name_length = 0;

	OpenFrontendScreen(polyitems, textitems);

	while (editing)
	{
		HandleWinMessages();
		ReadInput();

		input = GetASCIIInput();

		switch (input)
		{
			case -3:
				FE_PlayClickSound();
				editing = 0;
				break;
			case -2:
				if (name_length)
				{
					FE_PlayClickSound();
					editing = 0;
				}
				break;
			case -1:
				if (name_length > 0)
				{
					FE_PlayClickSound();
					buffer[--name_length] = 0;
				}
				break;
			case 0:
				break;
			default:
				if (name_length < max_length)
				{
					FE_PlayClickSound();
					buffer[name_length] = input;
					buffer[++name_length] = 0;
				}
				break;
		}

		if (--blink_timer < 0)
		{
			blink_timer = 10;
			blink_on ^= 1;

			if (blink_on)
				cursor_text[0] = '_';
			else
				cursor_text[0] = 0;
		}

		PrintFont(textitems[2].font);
		textitems[2].x = (int)(StringWidth(buffer) / 2.0) + 321;

		DrawFrontend();
	}

	CloseFrontendScreen();

	if (*buffer == 0)
		strcpy(buffer, gLanguageStrings[307]);

	CheckCheatName(buffer);

	return 0;
}

// [0x413861] [D]
void UnlockAllCheats(void)
{
	CHEAT_NAME *cheat;

	for (cheat = cheat_names; cheat->name; cheat++)
		*cheat->cheat = 1;

	UpdateSavedOptions();
}

// [0x413894] [D]
// NOTE: IDA types it int, but it returns nothing (eax is left over)
void CheckCheatName(char *name)
{
	CHEAT_NAME *cheat;

	for (cheat = cheat_names; cheat->name; cheat++)
	{
		if (strcmp(name, cheat->name) == 0)
			*cheat->cheat = 1;
	}

	UpdateSavedOptions();
}
