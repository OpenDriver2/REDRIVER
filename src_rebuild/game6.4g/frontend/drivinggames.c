/*
 * frontend/drivinggames.c - "Driving Games" menu: pick the game type
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "frontend\drivinggames.c", 0x40a770 - 0x40aa14)
 */

#include "dr_types.h"
#include "dr_globals.h"
#include "pres.h"
#include "shell.h"
#include "frontend/config.h"
#include "frontend/dirttrack.h"
#include "frontend/drivinggames.h"
#include "frontend/frontend.h"
#include "frontend/selrace.h"
#include "input.h"

void SetOptionsDrivingGames(void);
void checkpoint_selfunc(int flag);
void checkpoint_unselfunc(int flag);
void trail_selfunc(int flag);
void trail_unselfunc(int flag);
void getaway_selfunc(int flag);
void getaway_unselfunc(int flag);
void survival_selfunc(int flag);
void survival_unselfunc(int flag);
void pursuit_selfunc(int flag);
void pursuit_unselfunc(int flag);
void carnage_selfunc(int flag);
void carnage_unselfunc(int flag);
void dirttrack_selfunc(int flag);
void dirttrack_unselfunc(int flag);
int race_choosefunc(void);

//------------------------------------------------------------------------------------------
// uninitialised data (0x5d7a10)

static int selection;
// NOTE: byte_5D7A14, the bss buffer the polyitems terminator points at (as in frontend/main.c)
static char poly_end_text[4];

//------------------------------------------------------------------------------------------
// initialised data (0x54c970)

static FE_POLY polyitems[1] = {
	{ 0, 0, poly_end_text, 4, 0, 0, { 0, 0, 0, 0 }, 0 },
};
// NOTE: 4 zero bytes follow in the binary (0x54c984), as after every FE_POLY array: padding

static FE_TEXT textitems[9] = {
	{ 40, 150, 0, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 320, 190, 0, 1, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 320, 220, 0, 1, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 320, 250, 0, 1, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 320, 280, 0, 1, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 320, 310, 0, 1, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 320, 340, 0, 1, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 320, 370, 0, 1, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ -1, -1, 0, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
};

static FE_BUTTON menufunc[8] = {
	{ pursuit_selfunc, pursuit_unselfunc, race_choosefunc, 0, 0, 6, 1 },
	{ getaway_selfunc, getaway_unselfunc, race_choosefunc, 1, 1, 0, 2 },
	{ checkpoint_selfunc, checkpoint_unselfunc, race_choosefunc, 2, 2, 1, 3 },
	{ trail_selfunc, trail_unselfunc, race_choosefunc, 3, 3, 2, 4 },
	{ survival_selfunc, survival_unselfunc, race_choosefunc, 4, 4, 3, 5 },
	{ dirttrack_selfunc, dirttrack_unselfunc, race_choosefunc, 5, 5, 4, 6 },
	{ carnage_selfunc, carnage_unselfunc, race_choosefunc, 6, 6, 5, 0 },
	{ NULL, NULL, NULL, 0, 0, 0, 0 },
};

// [0x40a770] [D]
int SelectDrivingGames(void)
{
	int ret;

	SetOptionsDrivingGames();

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

	global_score.type = GAME_DEFAULT;

	if (ret != -1)
		return ret;

	CloseFrontendScreen();
	return 0;
}

// [0x40a7f6] [D]
void SetOptionsDrivingGames(void)
{
	textitems[0].text = gLanguageStrings[89];
	textitems[3].text = gLanguageStrings[92];
	textitems[4].text = gLanguageStrings[93];
	textitems[2].text = gLanguageStrings[91];
	textitems[5].text = gLanguageStrings[94];
	textitems[1].text = gLanguageStrings[90];
	textitems[6].text = gLanguageStrings[95];
	textitems[7].text = gLanguageStrings[96];
}

// [0x40a855] [D]
void checkpoint_selfunc(int flag)
{
	textitems[3].font = 1;
	global_score.type = GAME_CHECKPOINT;
}

// [0x40a86b] [D]
void checkpoint_unselfunc(int flag)
{
	if (flag == 0)
		textitems[3].font = 0;
}

// [0x40a87d] [D]
void trail_selfunc(int flag)
{
	textitems[4].font = 1;
	global_score.type = GAME_TRAILBLAZER;
}

// [0x40a893] [D]
void trail_unselfunc(int flag)
{
	if (flag == 0)
		textitems[4].font = 0;
}

// [0x40a8a5] [D]
void getaway_selfunc(int flag)
{
	textitems[2].font = 1;
	global_score.type = GAME_GETAWAY;
}

// [0x40a8bb] [D]
void getaway_unselfunc(int flag)
{
	if (flag == 0)
		textitems[2].font = 0;
}

// [0x40a8cd] [D]
void survival_selfunc(int flag)
{
	textitems[5].font = 1;
	global_score.type = GAME_SURVIVAL;
}

// [0x40a8e3] [D]
void survival_unselfunc(int flag)
{
	if (flag == 0)
		textitems[5].font = 0;
}

// [0x40a8f5] [D]
void pursuit_selfunc(int flag)
{
	textitems[1].font = 1;
	global_score.type = GAME_PURSUIT;
}

// [0x40a90b] [D]
void pursuit_unselfunc(int flag)
{
	if (flag == 0)
		textitems[1].font = 0;
}

// [0x40a91d] [D]
void carnage_selfunc(int flag)
{
	textitems[7].font = 1;
	global_score.type = GAME_CARNAGE;
}

// [0x40a933] [D]
void carnage_unselfunc(int flag)
{
	if (flag == 0)
		textitems[7].font = 0;
}

// [0x40a945] [D]
void dirttrack_selfunc(int flag)
{
	textitems[6].font = 1;
	global_score.type = GAME_DIRTTRACK;
}

// [0x40a95b] [D]
void dirttrack_unselfunc(int flag)
{
	if (flag == 0)
		textitems[6].font = 0;
}

// [0x40a96d] [D]
int race_choosefunc(void)
{
	int ret;
	int freight_train;
	int traffic_density;

	CloseFrontendScreen();

	// NOTE: ret stays uninitialised for any other game type, as in the binary (the selfuncs
	// only set the types handled here)
	switch (global_score.type)
	{
		case GAME_TRAILBLAZER:
		case GAME_CHECKPOINT:
		case GAME_SURVIVAL:
		case GAME_PURSUIT:
		case GAME_GETAWAY:
			ret = SelectRace();
			break;
		case GAME_DIRTTRACK:
			ret = SelectDirtTrackMode();
			break;
		case GAME_CARNAGE:
			// carnage is played with the freight train cheat and full traffic
			freight_train = gCheatFlags.FreightTrain;
			gCheatFlags.FreightTrain = 1;
			traffic_density = gTrafficDensity;
			gTrafficDensity = 32;
			UpdateSavedOptions();

			ret = SelectRace();

			gCheatFlags.FreightTrain = freight_train;
			gTrafficDensity = traffic_density;
			UpdateSavedOptions();
			break;
	}

	if (ret == 0)
		OpenFrontendScreen(polyitems, textitems);

	return ret;
}
