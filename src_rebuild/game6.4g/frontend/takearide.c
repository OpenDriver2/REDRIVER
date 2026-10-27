/*
 * frontend/takearide.c - "Take a Ride" menu: pick an open city
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "frontend\takearide.c", 0x4163a0 - 0x41676f)
 */

#include "dr_types.h"

/* frontend/frontend.c */
extern void ResetMenu(FE_BUTTON *buttons, int selection);
extern int HandleMenu(FE_BUTTON *buttons, int *selection);
extern void OpenFrontendScreen(FE_POLY *polys, FE_TEXT *texts);
extern void CloseFrontendScreen(void);
extern void DrawFrontend(void);
extern void StartGameFromFrontend(int do_gamemode, int do_credits);

/* frontend/cars.c, frontend/daynight.c */
extern int SelectCheatCars(void);
extern int SelectDayNight(void);

extern void HandleWinMessages(void);
extern int ReadInput(void);

extern SCORE_DATA global_score;
extern CHEATS gCheatFlags;
extern int gCheatsAllowed[11];
extern int gNumCitiesOpen;
extern int GameMode;
extern char *gLanguageStrings[];

// FE_TEXT justify is PrintAt's mode (frontend/font.c): cities not open yet are drawn translucent
#define PRINT_TRANSLUCENT	0x8

void SetOptionsTakeARide(void);
static void miami_selfunc(int flag);
static void miami_unselfunc(int flag);
static void sanfran_selfunc(int flag);
static void sanfran_unselfunc(int flag);
static void losang_selfunc(int flag);
static void losang_unselfunc(int flag);
static void newyork_selfunc(int flag);
static void newyork_unselfunc(int flag);
static int city_choosefunc(void);
static void SetGreyedItems(int first, int last);

//------------------------------------------------------------------------------------------
// uninitialised data (0x600c50)

static int selection;
// NOTE: dword_600C54, a bss buffer: the picture shown before a city is selected (empty)
static char poly_text[4];

//------------------------------------------------------------------------------------------
// initialised data (0x5500d8)

static FE_POLY polyitems[2] = {
	{ 327, 182, poly_text, 1, 0, 0, { -1, -1, -1, -1 }, 0 },
	{ 0, 0, NULL, 4, 0, 0, { 0, 0, 0, 0 }, 0 },
};

// NOTE: named takearide._menufunc in IDA, but it is the screen's FE_TEXT list (textitems in the
// other menus)
static FE_TEXT textitems[6] = {
	{ 40, 150, 0, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 26, 210, 0, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 26, 250, 0, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 26, 290, 0, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 26, 330, 0, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ -1, -1, 0, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
};

// one menu per number of open cities
static FE_BUTTON menufunc1[2] = {
	{ miami_selfunc, miami_unselfunc, city_choosefunc, 0, 0, 0, 0 },
	{ NULL, NULL, NULL, 0, 0, 0, 0 },
};

static FE_BUTTON menufunc2[3] = {
	{ miami_selfunc, miami_unselfunc, city_choosefunc, 0, 0, 1, 1 },
	{ sanfran_selfunc, sanfran_unselfunc, city_choosefunc, 1, 1, 0, 0 },
	{ NULL, NULL, NULL, 0, 0, 0, 0 },
};

static FE_BUTTON menufunc3[4] = {
	{ miami_selfunc, miami_unselfunc, city_choosefunc, 0, 0, 2, 1 },
	{ sanfran_selfunc, sanfran_unselfunc, city_choosefunc, 1, 1, 0, 2 },
	{ losang_selfunc, losang_unselfunc, city_choosefunc, 2, 2, 1, 0 },
	{ NULL, NULL, NULL, 0, 0, 0, 0 },
};

static FE_BUTTON menufunc4[5] = {
	{ miami_selfunc, miami_unselfunc, city_choosefunc, 0, 0, 3, 1 },
	{ sanfran_selfunc, sanfran_unselfunc, city_choosefunc, 1, 1, 0, 2 },
	{ losang_selfunc, losang_unselfunc, city_choosefunc, 2, 2, 1, 3 },
	{ newyork_selfunc, newyork_unselfunc, city_choosefunc, 3, 3, 2, 0 },
	{ NULL, NULL, NULL, 0, 0, 0, 0 },
};

static FE_BUTTON *cur_menu = menufunc1;

// [0x4163a0] [D]
int SelectTakeARide(void)
{
	int ret;

	selection = 0;
	ResetMenu(menufunc4, 0);

	SetOptionsTakeARide();

	ResetMenu(cur_menu, selection);
	OpenFrontendScreen(polyitems, textitems);

	for (;;)
	{
		HandleWinMessages();
		ReadInput();

		ret = HandleMenu(cur_menu, &selection);

		if (ret != 0)
			break;

		DrawFrontend();
	}

	global_score.type = GAME_DEFAULT;
	gCheatFlags.PlayerCar = 0;

	if (ret != -1)
		return ret;

	CloseFrontendScreen();
	return 0;
}

// [0x416447] [D]
void SetOptionsTakeARide(void)
{
	switch (gNumCitiesOpen)
	{
		case 2:
			cur_menu = menufunc2;
			SetGreyedItems(3, 4);
			break;
		case 3:
			cur_menu = menufunc3;
			SetGreyedItems(4, 4);
			break;
		case 4:
			cur_menu = menufunc4;
			SetGreyedItems(5, 4);
			break;
		default:
			gNumCitiesOpen = 1;
			// fall through
		case 1:
			cur_menu = menufunc1;
			SetGreyedItems(2, 4);
			break;
	}

	textitems[0].text = gLanguageStrings[110];
	textitems[1].text = gLanguageStrings[112];
	textitems[2].text = gLanguageStrings[113];
	textitems[3].text = gLanguageStrings[114];
	textitems[4].text = gLanguageStrings[115];
}

// [0x416520] [D]
static void miami_selfunc(int flag)
{
	polyitems[0].text = "MIAMI";
	polyitems[0].x = 327;
	polyitems[0].y = 182;

	textitems[1].font = 1;
	GameMode = 46;
}

// [0x416552] [D]
static void miami_unselfunc(int flag)
{
	if (flag == 0)
		textitems[1].font = 0;
}

// [0x416564] [D]
static void sanfran_selfunc(int flag)
{
	if (gNumCitiesOpen >= 2)
	{
		polyitems[0].text = "SF";
		polyitems[0].x = 328;
		polyitems[0].y = 166;

		textitems[2].font = 1;
		GameMode = 47;
	}
	else
	{
		selection = 0;
		miami_selfunc(flag);
	}
}

// [0x4165b7] [D]
static void sanfran_unselfunc(int flag)
{
	if (flag == 0)
		textitems[2].font = 0;
}

// [0x4165c9] [D]
static void losang_selfunc(int flag)
{
	if (gNumCitiesOpen >= 3)
	{
		polyitems[0].text = "LA";
		polyitems[0].x = 330;
		polyitems[0].y = 192;

		textitems[3].font = 1;
		GameMode = 48;
	}
	else
	{
		selection = 1;
		sanfran_selfunc(flag);
	}
}

// [0x41661c] [D]
static void losang_unselfunc(int flag)
{
	if (flag == 0)
		textitems[3].font = 0;
}

// [0x41662e] [D]
static void newyork_selfunc(int flag)
{
	if (gNumCitiesOpen >= 4)
	{
		polyitems[0].text = "NY";
		polyitems[0].x = 327;
		polyitems[0].y = 154;

		textitems[4].font = 1;
		GameMode = 49;
	}
	else
	{
		selection = 2;
		losang_selfunc(flag);
	}
}

// [0x416681] [D]
static void newyork_unselfunc(int flag)
{
	if (flag == 0)
		textitems[4].font = 0;
}

// [0x416693] [D]
static int city_choosefunc(void)
{
	int ret;

	CloseFrontendScreen();

	global_score.type = GAME_TAKEADRIVE;

	if (gCheatsAllowed[5])
	{
		ret = SelectCheatCars();
	}
	else
	{
		if (GameMode == 48)
		{
			// Los Angeles selected
			StartGameFromFrontend(1, 0);
			return 1;
		}

		ret = SelectDayNight();
	}

	if (ret == 0)
		OpenFrontendScreen(polyitems, textitems);

	return ret;
}

// [0x4166fc] [D]
// NOTE: unnamed in IDA (sub_4166FC); name guessed. Draws the city texts first..last (not open
// yet) translucent and the ones before normally.
static void SetGreyedItems(int first, int last)
{
	int i;

	for (i = 0; i < first; i++)
		textitems[i].justify &= ~PRINT_TRANSLUCENT;

	for (i = first; i <= last; i++)
		textitems[i].justify |= PRINT_TRANSLUCENT;
}
