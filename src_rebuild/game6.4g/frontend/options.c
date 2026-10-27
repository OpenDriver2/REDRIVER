/*
 * frontend/options.c - options menu: detail, audio, controller, gameplay
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "frontend\options.c", 0x413af0 - 0x413ce6)
 */

#include "dr_types.h"
#include "pres.h"
#include "frontend/config.h"
#include "frontend/controller.h"
#include "frontend/details.h"
#include "frontend/frontend.h"
#include "frontend/gameplay.h"
#include "frontend/options.h"
#include "frontend/soundoptions.h"
#include "input.h"
#include "shell.h"

void SetupOptions(void);
static void detail_selfunc(int flag);
static void detail_unselfunc(int flag);
static int detail_choosefunc(void);
static void audio_selfunc(int flag);
static void audio_unselfunc(int flag);
static int audio_choosefunc(void);
static void control_selfunc(int flag);
static void control_unselfunc(int flag);
static int control_choosefunc(void);
static void gameplay_selfunc(int flag);
static void gameplay_unselfunc(int flag);
static int gameplay_choosefunc(void);

//------------------------------------------------------------------------------------------
// uninitialised data (0x600c08)

static int selection;
// NOTE: dword_600C0C, the bss buffer the polyitems terminator points at (as in frontend/main.c)
static char poly_end_text[4];

//------------------------------------------------------------------------------------------
// initialised data (0x54f2c8)

static FE_POLY polyitems[1] = {
	{ 0, 0, poly_end_text, 4, 0, 0, { 0, 0, 0, 0 }, 0 },
};
// NOTE: 4 zero bytes follow in the binary (0x54f2dc), as after every FE_POLY array: padding

static FE_TEXT textitems[6] = {
	{ 40, 150, 0, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 320, 200, 0, 1, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 320, 240, 0, 1, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 320, 280, 0, 1, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 320, 320, 0, 1, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ -1, -1, 0, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
};

static FE_BUTTON menufunc[5] = {
	{ detail_selfunc, detail_unselfunc, detail_choosefunc, 0, 0, 3, 1 },
	{ audio_selfunc, audio_unselfunc, audio_choosefunc, 1, 1, 0, 2 },
	{ control_selfunc, control_unselfunc, control_choosefunc, 2, 2, 1, 3 },
	{ gameplay_selfunc, gameplay_unselfunc, gameplay_choosefunc, 3, 3, 2, 0 },
	{ NULL, NULL, NULL, 0, 0, 0, 0 },
};

// [0x413af0] [D]
int SelectOptions(void)
{
	int ret;

	SetupOptions();

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

	UpdateSavedOptions();

	if (ret != -1)
		return ret;

	CloseFrontendScreen();
	return 0;
}

// [0x413b71] [D]
void SetupOptions(void)
{
	textitems[0].text = gLanguageStrings[37];
	textitems[1].text = gLanguageStrings[38];
	textitems[2].text = gLanguageStrings[39];
	textitems[3].text = gLanguageStrings[40];
	textitems[4].text = gLanguageStrings[41];
}

// [0x413bae] [D]
static void detail_selfunc(int flag)
{
	textitems[1].font = 1;
}

// [0x413bba] [D]
static void detail_unselfunc(int flag)
{
	if (flag == 0)
		textitems[1].font = 0;
}

// [0x413bcc] [D]
static int detail_choosefunc(void)
{
	int ret;

	CloseFrontendScreen();
	ret = SelectDetailOptions();

	if (ret == 0)
		OpenFrontendScreen(polyitems, textitems);

	return ret;
}

// [0x413bfc] [D]
static void audio_selfunc(int flag)
{
	textitems[2].font = 1;
}

// [0x413c08] [D]
static void audio_unselfunc(int flag)
{
	if (flag == 0)
		textitems[2].font = 0;
}

// [0x413c1a] [D]
static int audio_choosefunc(void)
{
	int ret;

	CloseFrontendScreen();
	ret = SelectAudioOptions();

	if (ret == 0)
		OpenFrontendScreen(polyitems, textitems);

	return ret;
}

// [0x413c4a] [D]
static void control_selfunc(int flag)
{
	textitems[3].font = 1;
}

// [0x413c56] [D]
static void control_unselfunc(int flag)
{
	if (flag == 0)
		textitems[3].font = 0;
}

// [0x413c68] [D]
static int control_choosefunc(void)
{
	int ret;

	CloseFrontendScreen();
	ret = SelectController();

	if (ret == 0)
		OpenFrontendScreen(polyitems, textitems);

	return ret;
}

// [0x413c98] [D]
static void gameplay_selfunc(int flag)
{
	textitems[4].font = 1;
}

// [0x413ca4] [D]
static void gameplay_unselfunc(int flag)
{
	if (flag == 0)
		textitems[4].font = 0;
}

// [0x413cb6] [D]
static int gameplay_choosefunc(void)
{
	int ret;

	CloseFrontendScreen();
	ret = SelectGameplayOptions();

	if (ret == 0)
		OpenFrontendScreen(polyitems, textitems);

	return ret;
}
