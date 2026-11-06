/*
 * frontend/cheats.c - cheats menu: the cheats unlocked so far, each toggled on / off
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "frontend\cheats.c", 0x404880 - 0x4051e4)
 */

#include "dr_types.h"
#include "dr_globals.h"
#include "frontend/cheats.h"
#include "frontend/frontend.h"
#include "pres.h"
#include "shell.h"
#include "frontend/config.h"
#include "input.h"
#include "replays.h"
#include "dr_scores.h"

#define MAX_CHEATS	10

int SelectCheats(void);
void SetupCheats(void);
void cheat_selfunc(int flag);
void cheat_unselfunc(int flag);
int superfast_choosefunc(void);
int policeimmunity_choosefunc(void);
int nodamage_choosefunc(void);
int freighttrain_choosefunc(void);
int gtaview_choosefunc(void);
int skipinterview_choosefunc(void);
int evilruss_choosefunc(void);
int opencities_choosefunc(void);
int playnewcastle_choosefunc(void);
int credits_choosefunc(void);
int none_choosefunc(void);
void SetOptionsCheats(void);

//------------------------------------------------------------------------------------------
// uninitialised data (0x57d7ca, marked @frontend\cheats.c)

short CurrentJunction;
int gPlayerCarDamage;

// the cheat shown on each menu line (0 - 9 as in SetOptionsCheats), -1 ends the list
int gCheatsFrontend[MAX_CHEATS + 1];
static int unknown_57D7FC;		// NOTE: no references

static FE_BUTTON menufunc[MAX_CHEATS + 1];
static int selection;
// NOTE: unnamed in IDA (dword_57D8B4); name guessed. The selection before HandleMenu moved it,
// whose line cheat_unselfunc un-highlights.
static int last_selection;
// NOTE: dword_57D8B8, the bss buffer the polyitems terminator points at (as in frontend/main.c)
static char poly_end_text[4];
// NOTE: unnamed in IDA (dword_57D8BC, dword_57D8C0, word_57D8C4); names guessed. Empty texts
// shown instead of on / off for the cheats that are actions, not switches.
static char opencities_text[4];
static char newcastle_text[4];
static char credits_text[2];

short CurrentRoad;

//------------------------------------------------------------------------------------------
// initialised data (0x54bae8, start of the file's block per IDA mark)

static FE_POLY polyitems[1] = {
	{ 0, 0, poly_end_text, 4, 0, 0, { 0, 0, 0, 0 }, 0 },
};
// NOTE: 4 zero bytes follow in the binary (0x54bafc), as after every FE_POLY array: padding

// title, then a name (left) and its state (right) per line
static FE_TEXT textitems[22] = {
	{ 40, 150, 0, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 45, 190, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 595, 190, 2, 2, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 45, 214, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 595, 214, 2, 2, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 45, 238, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 595, 238, 2, 2, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 45, 262, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 595, 262, 2, 2, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 45, 286, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 595, 286, 2, 2, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 45, 310, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 595, 310, 2, 2, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 45, 334, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 595, 334, 2, 2, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 45, 358, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 595, 358, 2, 2, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 45, 382, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 595, 382, 2, 2, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 45, 406, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 595, 406, 2, 2, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ -1, -1, 0, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
};

// language string of a cheat's state: [0] off, [1] on
static int onoff[2] = { 2, 1 };

// NOTE: moved here from the generated stubs; no IDA mark, the owner is a guess from the users
// and the neighbouring data
int gCheatsAllowed[11];
CHEATS gCheatFlags;

// [0x404880] [D]
int SelectCheats(void)
{
	int ret;

	SetupCheats();

	selection = 0;

	ResetMenu(menufunc, 0);
	OpenFrontendScreen(polyitems, textitems);

	for (;;)
	{
		HandleWinMessages();
		ReadInput();

		last_selection = selection;

		ret = HandleMenu(menufunc, &selection);

		if (ret != 0)
			break;

		DrawFrontend();
	}

	UpdateSavedOptions();

	if (ret != -1)
		return ret;

	CloseFrontendScreen();

	return 0;
}

// [0x40490d] [D]
// Builds the menu from the cheats unlocked (gCheatsAllowed), one line each.
void SetupCheats(void)
{
	int count;
	int i;

	count = 0;

	textitems[0].text = gLanguageStrings[99];

	if (gCheatsAllowed[0])
	{
		menufunc[0].selfunc = cheat_selfunc;
		menufunc[0].unselfunc = cheat_unselfunc;
		menufunc[0].choosefunc = superfast_choosefunc;
		textitems[1].text = gLanguageStrings[241];
		gCheatsFrontend[0] = 0;
		count = 1;
	}

	if (gCheatsAllowed[1])
	{
		menufunc[count].selfunc = cheat_selfunc;
		menufunc[count].unselfunc = cheat_unselfunc;
		menufunc[count].choosefunc = policeimmunity_choosefunc;
		textitems[count * 2 + 1].text = gLanguageStrings[242];
		gCheatsFrontend[count++] = 1;
	}

	if (gCheatsAllowed[2])
	{
		menufunc[count].selfunc = cheat_selfunc;
		menufunc[count].unselfunc = cheat_unselfunc;
		menufunc[count].choosefunc = nodamage_choosefunc;
		textitems[count * 2 + 1].text = gLanguageStrings[243];
		gCheatsFrontend[count++] = 2;
	}

	if (gCheatsAllowed[3])
	{
		menufunc[count].selfunc = cheat_selfunc;
		menufunc[count].unselfunc = cheat_unselfunc;
		menufunc[count].choosefunc = freighttrain_choosefunc;
		textitems[count * 2 + 1].text = gLanguageStrings[244];
		gCheatsFrontend[count++] = 3;
	}

	if (gCheatsAllowed[4])
	{
		menufunc[count].selfunc = cheat_selfunc;
		menufunc[count].unselfunc = cheat_unselfunc;
		menufunc[count].choosefunc = gtaview_choosefunc;
		textitems[count * 2 + 1].text = gLanguageStrings[245];
		gCheatsFrontend[count++] = 4;
	}

	if (gCheatsAllowed[7])
	{
		menufunc[count].selfunc = cheat_selfunc;
		menufunc[count].unselfunc = cheat_unselfunc;
		menufunc[count].choosefunc = skipinterview_choosefunc;
		textitems[count * 2 + 1].text = gLanguageStrings[175];
		gCheatsFrontend[count++] = 5;
	}

	if (gCheatsAllowed[10])
	{
		menufunc[count].selfunc = cheat_selfunc;
		menufunc[count].unselfunc = cheat_unselfunc;
		menufunc[count].choosefunc = evilruss_choosefunc;
		textitems[count * 2 + 1].text = gLanguageStrings[271];
		gCheatsFrontend[count++] = 6;
	}

	if (gCheatsAllowed[6])
	{
		menufunc[count].selfunc = cheat_selfunc;
		menufunc[count].unselfunc = cheat_unselfunc;
		menufunc[count].choosefunc = opencities_choosefunc;
		textitems[count * 2 + 1].text = gLanguageStrings[100];
		gCheatsFrontend[count++] = 7;
	}

	if (gCheatsAllowed[8])
	{
		menufunc[count].selfunc = cheat_selfunc;
		menufunc[count].unselfunc = cheat_unselfunc;
		menufunc[count].choosefunc = playnewcastle_choosefunc;
		textitems[count * 2 + 1].text = gLanguageStrings[176];
		gCheatsFrontend[count++] = 8;
	}

	if (gCheatsAllowed[9])
	{
		menufunc[count].selfunc = cheat_selfunc;
		menufunc[count].unselfunc = cheat_unselfunc;
		menufunc[count].choosefunc = credits_choosefunc;
		textitems[count * 2 + 1].text = gLanguageStrings[177];
		gCheatsFrontend[count++] = 9;
	}

	if (count)
	{
		for (i = 0; i < count; i++)
		{
			menufunc[i].l = i;
			menufunc[i].r = i;
			menufunc[i].u = i - 1;
			menufunc[i].d = i + 1;
		}

		menufunc[0].u = count - 1;
		menufunc[count - 1].d = 0;
	}
	else
	{
		// no cheats yet: a single line saying so
		menufunc[0].selfunc = cheat_selfunc;
		menufunc[0].unselfunc = cheat_unselfunc;
		menufunc[0].choosefunc = none_choosefunc;
		menufunc[0].l = 0;
		menufunc[0].r = 0;
		menufunc[0].u = 0;
		menufunc[0].d = 0;
		textitems[1].text = gLanguageStrings[211];
		gCheatsFrontend[0] = -1;
		count = 1;
	}

	menufunc[count].selfunc = NULL;
	menufunc[count].unselfunc = NULL;
	menufunc[count].choosefunc = NULL;
	menufunc[count].l = 0;
	menufunc[count].r = 0;
	menufunc[count].u = 0;
	menufunc[count].d = 0;
	textitems[count * 2 + 1].text = NULL;
	gCheatsFrontend[count] = -1;

	SetOptionsCheats();
}

// [0x404e82] [D]
void cheat_selfunc(int flag)
{
	textitems[selection * 2 + 1].font = 3;
	textitems[selection * 2 + 2].font = 3;
}

// [0x404eae] [D]
void cheat_unselfunc(int flag)
{
	if (flag == 0)
	{
		textitems[last_selection * 2 + 1].font = 2;
		textitems[last_selection * 2 + 2].font = 2;
	}
}

// [0x404ee0] [D]
int superfast_choosefunc(void)
{
	gCheatFlags.SuperFastCars ^= 1;
	SetOptionsCheats();
	return 0;
}

// [0x404ef9] [D]
int policeimmunity_choosefunc(void)
{
	gCheatFlags.Immunity ^= 1;
	SetOptionsCheats();
	return 0;
}

// [0x404f12] [D]
int nodamage_choosefunc(void)
{
	gCheatFlags.Invincibility ^= 1;
	SetOptionsCheats();
	return 0;
}

// [0x404f2b] [D]
int freighttrain_choosefunc(void)
{
	gCheatFlags.FreightTrain ^= 1;
	SetOptionsCheats();
	return 0;
}

// [0x404f44] [D]
int gtaview_choosefunc(void)
{
	gCheatFlags.OverheadView ^= 1;
	SetOptionsCheats();
	return 0;
}

// [0x404f5d] [D]
int skipinterview_choosefunc(void)
{
	gSkipInterview ^= 1;
	SetOptionsCheats();
	return 0;
}

// [0x404f76] [D]
int evilruss_choosefunc(void)
{
	gCheatFlags.RailroadCops ^= 1;
	SetOptionsCheats();
	return 0;
}

// [0x404f8f] [D]
int opencities_choosefunc(void)
{
	gNumCitiesOpen = 4;
	return 0;
}

// [0x404fa0] [D]
int playnewcastle_choosefunc(void)
{
	CloseFrontendScreen();

	GameMode = 261;
	global_score.type = GAME_TAKEADRIVE;
	StartGameFromFrontend(1, 0);

	OpenFrontendScreen(polyitems, textitems);
	return 0;
}

// [0x404fde] [D]
int credits_choosefunc(void)
{
	CloseFrontendScreen();

	gDoCredits = 1;
	gbPlayingDemo = 1;

	InitReplay("CREDITS");
	StartGameFromFrontend(1, 1);

	gbPlayingDemo = 0;
	gDoCredits = 0;

	return 1;
}

// [0x40502e] [D]
int none_choosefunc(void)
{
	CloseFrontendScreen();
	return 1;
}

// [0x40503d] [D]
// Sets the state text (on / off) of every line.
void SetOptionsCheats(void)
{
	int i;

	for (i = 0; ; i++)
	{
		switch (gCheatsFrontend[i])
		{
			case -1:
				return;
			case 0:
				textitems[i * 2 + 2].text = gLanguageStrings[onoff[gCheatFlags.SuperFastCars]];
				break;
			case 1:
				textitems[i * 2 + 2].text = gLanguageStrings[onoff[gCheatFlags.Immunity]];
				break;
			case 2:
				textitems[i * 2 + 2].text = gLanguageStrings[onoff[gCheatFlags.Invincibility]];
				break;
			case 3:
				textitems[i * 2 + 2].text = gLanguageStrings[onoff[gCheatFlags.FreightTrain]];
				break;
			case 4:
				textitems[i * 2 + 2].text = gLanguageStrings[onoff[gCheatFlags.OverheadView]];
				break;
			case 5:
				textitems[i * 2 + 2].text = gLanguageStrings[onoff[gSkipInterview]];
				break;
			case 6:
				textitems[i * 2 + 2].text = gLanguageStrings[onoff[gCheatFlags.RailroadCops]];
				break;
			case 7:
				textitems[i * 2 + 2].text = opencities_text;
				break;
			case 8:
				textitems[i * 2 + 2].text = newcastle_text;
				break;
			case 9:
				textitems[i * 2 + 2].text = credits_text;
				break;
		}
	}
}
