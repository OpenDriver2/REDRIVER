/*
 * frontend/cars.c - cheat car selection screen
 *
 * Rebuilt from Driver_sym2_94.i64 (IDA folder "frontend\cars.c", 0x404610 - 0x404800)
 */

#include <stdio.h>

typedef unsigned char u_char;
typedef unsigned char byte;

typedef struct RGB
{
	byte r;
	byte g;
	byte b;
	byte pad;
} RGB;

typedef struct RGBA
{
	char r;
	char g;
	char b;
	char a;
} RGBA;

typedef struct FE_TEXT
{
	short x;
	short y;
	u_char font;
	u_char justify;
	char field_6;
	char field_7;
	char *text;
	RGB color;
} FE_TEXT;

typedef struct FE_POLY
{
	short x;
	short y;
	char *text;
	short flags;
	short field_0A;
	short field_0C;
	RGBA color;
	short field_12;
} FE_POLY;

typedef struct FE_BUTTON
{
	void (*selfunc)(int);
	void (*unselfunc)(int);
	int (*choosefunc)(void);
	byte l;
	byte r;
	byte u;
	byte d;
} FE_BUTTON;

typedef struct CHEATS
{
	int PlayerCar;
	int SuperFastCars;
	int Immunity;
	int Invincibility;
	int FreightTrain;
	int OverheadView;
	int RailroadCops;
} CHEATS;

/* frontend.c */
extern int on_slider;
extern void ResetMenu(FE_BUTTON *buttons, int selection);
extern int HandleMenu(FE_BUTTON *buttons, int *selection);
extern void OpenFrontendScreen(FE_POLY *polys, FE_TEXT *texts);
extern void CloseFrontendScreen(void);
extern void DrawFrontend(void);
extern void StartGameFromFrontend(int do_gamemode, int do_credits);

/* daynight.c */
extern int SelectDayNight(void);

extern void HandleWinMessages(void);
extern void ReadInput(void);

extern CHEATS gCheatFlags;
extern int GameMode;
extern char *gLanguageStrings[];

#define NUM_CHEAT_CARS	18

static void car_selfunc(int flag);
static void car_unselfunc(int flag);
static int car_choosefunc(void);
static void prev_selfunc(int flag);
static void next_selfunc(int flag);

void SetupCheatCars(void);

static char poly_end_text[2];		// defined with the bss block below

static FE_POLY polyitems[2] = {
	{ 256, 200, "PCAR1", 9, 0, 0, { 255, 255, 255, 255 }, 0 },
	{ 0, 0, poly_end_text, 4, 0, 0, { 0, 0, 0, 0 }, 0 },
};

static FE_TEXT textitems[2] = {
	{ 40, 150, 0, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ -1, -1, 0, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
};

static FE_BUTTON menufunc[4] = {
	{ car_selfunc, car_unselfunc, car_choosefunc, 1, 2, 0, 0 },
	{ prev_selfunc, NULL, NULL, 0, 0, 0, 0 },
	{ next_selfunc, NULL, NULL, 0, 0, 0, 0 },
	{ NULL, NULL, NULL, 0, 0, 0, 0 },
};

static char carname[8];
static int selection;
int current_car;
// NOTE: byte_57D7C8 (@frontend\cars.c), bss buffer the polyitems terminator points at, as in
// frontend/main.c; size by layout (CurrentJunction follows at 0x57D7CA)
static char poly_end_text[2];

// [0x404610] [D]
int SelectCheatCars(void)
{
	int ret;

	current_car = gCheatFlags.PlayerCar - 1;

	if (current_car < 0)
		current_car = 0;

	SetupCheatCars();

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

	on_slider = 0;

	if (ret != -1)
		return ret;

	CloseFrontendScreen();
	return 0;
}

// [0x4046b7] [D]
static void car_selfunc(int flag)
{
	on_slider = 1;
}

// [0x4046c6] [D]
static void car_unselfunc(int flag)
{
	if (flag == 0 || flag == 2)
		on_slider = 0;
}

// [0x4046f8] [D]
static int car_choosefunc(void)
{
	int ret;

	gCheatFlags.PlayerCar = current_car + 1;
	CloseFrontendScreen();

	if (GameMode == 48)
	{
		StartGameFromFrontend(1, 0);
		return 1;
	}

	ret = SelectDayNight();

	if (ret == 0)
		OpenFrontendScreen(polyitems, textitems);

	return ret;
}

// [0x404751] [D]
static void prev_selfunc(int flag)
{
	if (--current_car < 0)
		current_car = NUM_CHEAT_CARS - 1;

	// car 16 (index 15) is skipped
	if (current_car == 15)
		current_car = 14;

	SetupCheatCars();
}

// [0x40478e] [D]
static void next_selfunc(int flag)
{
	if (++current_car > NUM_CHEAT_CARS - 1)
		current_car = 0;

	// car 16 (index 15) is skipped
	if (current_car == 15)
		current_car = 16;

	SetupCheatCars();
}

// [0x4047cb] [D]
void SetupCheatCars(void)
{
	textitems[0].text = gLanguageStrings[149];

	sprintf(carname, "PCAR%d", current_car + 1);
	polyitems[0].text = carname;
}
