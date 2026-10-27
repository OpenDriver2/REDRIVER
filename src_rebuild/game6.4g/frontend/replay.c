/*
 * frontend/replay.c - replay menu: load a replay or view one of the four demos
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "frontend\replay.c", 0x413d70 - 0x413f5f)
 */

#include "dr_types.h"

/* frontend/frontend.c */
extern void ResetMenu(FE_BUTTON *buttons, int selection);
extern int HandleMenu(FE_BUTTON *buttons, int *selection);
extern void OpenFrontendScreen(FE_POLY *polys, FE_TEXT *texts);
extern void CloseFrontendScreen(void);
extern void DrawFrontend(void);

/* frontend/loadreplay.c, frontend/demo.c */
extern int SelectLoadReplay(void);
extern void StartRollingDemo(int demo);

extern void HandleWinMessages(void);
extern int ReadInput(void);

extern char *gLanguageStrings[];

void SetOptionsReplay(void);
static void load_selfunc(int flag);
static void load_unselfunc(int flag);
static int load_choosefunc(void);
static void demo1_selfunc(int flag);
static void demo1_unselfunc(int flag);
static int demo1_choosefunc(void);
static void demo2_selfunc(int flag);
static void demo2_unselfunc(int flag);
static int demo2_choosefunc(void);
static void demo3_selfunc(int flag);
static void demo3_unselfunc(int flag);
static int demo3_choosefunc(void);
static void demo4_selfunc(int flag);
static void demo4_unselfunc(int flag);
static int demo4_choosefunc(void);

//------------------------------------------------------------------------------------------
// uninitialised data (0x600c10)

static int selection;
// NOTE: dword_600C14, the bss buffer the polyitems terminator points at (as in frontend/main.c)
static char poly_end_text[4];

//------------------------------------------------------------------------------------------
// initialised data (0x54f3b8)

static FE_POLY polyitems[1] = {
	{ 0, 0, poly_end_text, 4, 0, 0, { 0, 0, 0, 0 }, 0 },
};
// NOTE: 4 zero bytes follow in the binary (0x54f3cc), as after every FE_POLY array: padding

static FE_TEXT textitems[7] = {
	{ 40, 150, 0, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 320, 200, 0, 1, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 320, 240, 0, 1, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 320, 280, 0, 1, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 320, 320, 0, 1, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 320, 360, 0, 1, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ -1, -1, 0, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
};

static FE_BUTTON menufunc[6] = {
	{ load_selfunc, load_unselfunc, load_choosefunc, 0, 0, 4, 1 },
	{ demo1_selfunc, demo1_unselfunc, demo1_choosefunc, 1, 1, 0, 2 },
	{ demo2_selfunc, demo2_unselfunc, demo2_choosefunc, 2, 2, 1, 3 },
	{ demo3_selfunc, demo3_unselfunc, demo3_choosefunc, 3, 3, 2, 4 },
	{ demo4_selfunc, demo4_unselfunc, demo4_choosefunc, 4, 4, 3, 0 },
	{ NULL, NULL, NULL, 0, 0, 0, 0 },
};

// [0x413d70] [D]
// NOTE: IDA types it void, but it returns the menu result in eax like the other Select*
// functions, and main.c's replay_choosefunc uses it
int SelectReplay(void)
{
	int ret;

	SetOptionsReplay();

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

	if (ret != -1)
		return ret;

	CloseFrontendScreen();
	return 0;
}

// [0x413dec] [D]
void SetOptionsReplay(void)
{
	textitems[0].text = gLanguageStrings[104];
	textitems[1].text = gLanguageStrings[105];
	textitems[2].text = gLanguageStrings[106];
	textitems[3].text = gLanguageStrings[107];
	textitems[4].text = gLanguageStrings[108];
	textitems[5].text = gLanguageStrings[109];
}

// [0x413e35] [D]
static void load_selfunc(int flag)
{
	textitems[1].font = 1;
}

// [0x413e41] [D]
static void load_unselfunc(int flag)
{
	if (flag == 0)
		textitems[1].font = 0;
}

// [0x413e53] [D]
static int load_choosefunc(void)
{
	int ret;

	CloseFrontendScreen();
	ret = SelectLoadReplay();

	if (ret == 0)
		OpenFrontendScreen(polyitems, textitems);

	return ret;
}

// [0x413e83] [D]
static void demo1_selfunc(int flag)
{
	textitems[2].font = 1;
}

// [0x413e8f] [D]
static void demo1_unselfunc(int flag)
{
	if (flag == 0)
		textitems[2].font = 0;
}

// [0x413ea1] [D]
static int demo1_choosefunc(void)
{
	CloseFrontendScreen();
	StartRollingDemo(0);
	return 1;
}

// [0x413eba] [D]
static void demo2_selfunc(int flag)
{
	textitems[3].font = 1;
}

// [0x413ec6] [D]
static void demo2_unselfunc(int flag)
{
	if (flag == 0)
		textitems[3].font = 0;
}

// [0x413ed8] [D]
static int demo2_choosefunc(void)
{
	CloseFrontendScreen();
	StartRollingDemo(1);
	return 1;
}

// [0x413ef1] [D]
static void demo3_selfunc(int flag)
{
	textitems[4].font = 1;
}

// [0x413efd] [D]
static void demo3_unselfunc(int flag)
{
	if (flag == 0)
		textitems[4].font = 0;
}

// [0x413f0f] [D]
static int demo3_choosefunc(void)
{
	CloseFrontendScreen();
	StartRollingDemo(2);
	return 1;
}

// [0x413f28] [D]
static void demo4_selfunc(int flag)
{
	textitems[5].font = 1;
}

// [0x413f34] [D]
static void demo4_unselfunc(int flag)
{
	if (flag == 0)
		textitems[5].font = 0;
}

// [0x413f46] [D]
static int demo4_choosefunc(void)
{
	CloseFrontendScreen();
	StartRollingDemo(3);
	return 1;
}
