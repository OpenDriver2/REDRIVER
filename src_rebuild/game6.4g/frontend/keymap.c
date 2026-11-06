/*
 * frontend/keymap.c - keyboard redefinition; also used in the motel room
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "frontend\keymap.c", 0x411040 - 0x412097)
 */

#include "dr_keys.h"

#include "dr_types.h"
#include "dr_globals.h"
#include "pres.h"
#include "shell.h"
#include "input.h"
#include "flat/frontend.h"
#include "frontend/frontend.h"
#include "frontend/gvars.h"
#include "frontend/keymap.h"

void SetupKeyboardRedefine(void);
static void steerleft_selfunc(int flag);
static void steerleft_unselfunc(int flag);
static int steerleft_choosefunc(void);
static void steerright_selfunc(int flag);
static void steerright_unselfunc(int flag);
static int steerright_choosefunc(void);
static void faststeer_selfunc(int flag);
static void faststeer_unselfunc(int flag);
static int faststeer_choosefunc(void);
static void accelerate_selfunc(int flag);
static void accelerate_unselfunc(int flag);
static int accelerate_choosefunc(void);
static void burnout_selfunc(int flag);
static void burnout_unselfunc(int flag);
static int burnout_choosefunc(void);
static void brake_selfunc(int flag);
static void brake_unselfunc(int flag);
static int brake_choosefunc(void);
static void handbrake_selfunc(int flag);
static void handbrake_unselfunc(int flag);
static int handbrake_choosefunc(void);
static void viewtoggle_selfunc(int flag);
static void viewtoggle_unselfunc(int flag);
static int viewtoggle_choosefunc(void);
static void lookleft_selfunc(int flag);
static void lookleft_unselfunc(int flag);
static int lookleft_choosefunc(void);
static void lookright_selfunc(int flag);
static void lookright_unselfunc(int flag);
static int lookright_choosefunc(void);
static void lookbehind_selfunc(int flag);
static void lookbehind_unselfunc(int flag);
static int lookbehind_choosefunc(void);
static void cruisecontrol_selfunc(int flag);
static void cruisecontrol_unselfunc(int flag);
static int cruisecontrol_choosefunc(void);
static void horn_selfunc(int flag);
static void horn_unselfunc(int flag);
static int horn_choosefunc(void);
static void dropcamera_selfunc(int flag);
static void dropcamera_unselfunc(int flag);
static int dropcamera_choosefunc(void);
static void resetdefaults_selfunc(int flag);
static void resetdefaults_unselfunc(int flag);
static int resetdefaults_choosefunc(void);
static void HighlightMotelItem(void);
static void UnhighlightMotelItem(void);
static int CheckKeys(void);

//------------------------------------------------------------------------------------------
// uninitialised data (0x6009f0)

static int selection;
// NOTE: dword_6009F4; the item to return to when the motel menu is left with clashing keys
static int last_selection;
char *KeyToSet;				// the UserKeys key being redefined
int SettingKey;
// NOTE: dword_600A00; the text that flashes while a key is redefined
static int flash_item;
// NOTE: dword_600A04
static int flash_timer;
// NOTE: dword_600A08, dword_600A0C; the texts of the item highlighted in the motel room (-1: none)
static int motel_item;
static int motel_item2;
// NOTE: byte_600A10, a bss buffer: the text of the end entry of polyitems
static char poly_end_text[4];

//------------------------------------------------------------------------------------------
// initialised data (0x54e450)

static FE_POLY polyitems[1] = {
	{ 0, 0, poly_end_text, 4, 0, 0, { 0, 0, 0, 0 }, 0 },
};

// [0] title, [1 + 2 * i] action, [2 + 2 * i] its key, [29] reset to defaults
static FE_TEXT textitems[31] = {
	{ 40, 150, 0, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 40, 210, 3, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 305, 210, 2, 2, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 40, 235, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 305, 235, 2, 2, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 40, 260, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 305, 260, 2, 2, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 40, 285, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 305, 285, 2, 2, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 40, 310, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 305, 310, 2, 2, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 40, 335, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 305, 335, 2, 2, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 40, 360, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 305, 360, 2, 2, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 325, 210, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 600, 210, 2, 2, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 325, 235, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 600, 235, 2, 2, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 325, 260, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 600, 260, 2, 2, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 325, 285, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 600, 285, 2, 2, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 325, 310, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 600, 310, 2, 2, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 325, 335, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 600, 335, 2, 2, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 325, 360, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 600, 360, 2, 2, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 40, 395, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ -1, -1, 0, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
};

static FE_BUTTON menufunc[16] = {
	{ steerleft_selfunc, steerleft_unselfunc, steerleft_choosefunc, 7, 7, 14, 1 },
	{ steerright_selfunc, steerright_unselfunc, steerright_choosefunc, 8, 8, 0, 2 },
	{ faststeer_selfunc, faststeer_unselfunc, faststeer_choosefunc, 9, 9, 1, 3 },
	{ accelerate_selfunc, accelerate_unselfunc, accelerate_choosefunc, 10, 10, 2, 4 },
	{ brake_selfunc, brake_unselfunc, brake_choosefunc, 11, 11, 3, 5 },
	{ cruisecontrol_selfunc, cruisecontrol_unselfunc, cruisecontrol_choosefunc, 12, 12, 4, 6 },
	{ horn_selfunc, horn_unselfunc, horn_choosefunc, 13, 13, 5, 14 },
	{ burnout_selfunc, burnout_unselfunc, burnout_choosefunc, 0, 0, 13, 8 },
	{ handbrake_selfunc, handbrake_unselfunc, handbrake_choosefunc, 1, 1, 7, 9 },
	{ viewtoggle_selfunc, viewtoggle_unselfunc, viewtoggle_choosefunc, 2, 2, 8, 10 },
	{ dropcamera_selfunc, dropcamera_unselfunc, dropcamera_choosefunc, 3, 3, 9, 11 },
	{ lookleft_selfunc, lookleft_unselfunc, lookleft_choosefunc, 4, 4, 10, 12 },
	{ lookright_selfunc, lookright_unselfunc, lookright_choosefunc, 5, 5, 11, 13 },
	{ lookbehind_selfunc, lookbehind_unselfunc, lookbehind_choosefunc, 6, 6, 12, 7 },
	{ resetdefaults_selfunc, resetdefaults_unselfunc, resetdefaults_choosefunc, 14, 14, 6, 0 },
	{ NULL, NULL, NULL, 0, 0, 0, 0 },
};

// [0x411040] [D]
// While a key is being redefined its text flashes until a named key is pressed; Escape cancels.
// The menu can only be left when no two actions share a key; then the keys are taken into use.
int SelectKeyboardRedefine(void)
{
	int ret;
	int key;

	SetupKeyboardRedefine();

	SettingKey = 0;
	selection = 0;
	motel_item = 1;
	motel_item2 = 2;

	ResetMenu(menufunc, 0);

	if (gDoMotelRoom == 0)
		OpenFrontendScreen(polyitems, textitems);

	for (;;)
	{
		HandleWinMessages();
		ReadInput();

		if (SettingKey)
		{
			if (CheckKey(DIK_ESCAPE))
			{
				FE_PlayClickSound();
				SettingKey = 0;
				menufunc[selection].selfunc(gDoMotelRoom ? 2 : 0);
			}
			else
			{
				if (--flash_timer <= 0)
				{
					flash_timer = 10;
					textitems[flash_item].font ^= 1;
				}

				key = GetKeyInput();

				if (key >= 0)
				{
					FE_PlayClickSound();
					*KeyToSet = key;
					SettingKey = 0;
					menufunc[selection].selfunc(gDoMotelRoom ? 2 : 0);
					SetupKeyboardRedefine();
				}
			}
		}
		else if (gDoMotelRoom)
		{
			last_selection = selection;
			selection = MotelMenuKeyControl(menufunc, selection);

			if (selection == -1)
			{
				if (CheckKeys())
				{
					ret = 0;
					break;
				}

				selection = last_selection;
				FE_PlayErrorSound();
			}
		}
		else
		{
			ret = HandleMenu(menufunc, &selection);

			if (ret != 0)
			{
				if (CheckKeys())
					break;

				FE_PlayErrorSound();
			}
		}

		if (gDoMotelRoom)
		{
			HighlightMotelItem();
			DrawMotel(textitems, NULL);
		}
		else
		{
			DrawFrontend();
		}
	}

	KeySet = UserKeys;

	if (ret != -1)
		return ret;

	if (gDoMotelRoom == 0)
		CloseFrontendScreen();

	return 0;
}

// [0x411299] [D]
void SetupKeyboardRedefine(void)
{
	textitems[0].text = gLanguageStrings[66];		// Redefine:
	textitems[1].text = gLanguageStrings[67];		// Steer Left
	textitems[3].text = gLanguageStrings[68];		// Steer Right
	textitems[5].text = gLanguageStrings[69];		// Fast Steer
	textitems[7].text = gLanguageStrings[70];		// Accelerate
	textitems[9].text = gLanguageStrings[72];		// Brake
	textitems[11].text = gLanguageStrings[77];		// Cruise Control
	textitems[13].text = gLanguageStrings[78];		// Horn
	textitems[15].text = gLanguageStrings[71];		// Burn Out
	textitems[17].text = gLanguageStrings[73];		// Handbrake
	textitems[19].text = gLanguageStrings[230];		// View Toggle
	textitems[23].text = gLanguageStrings[74];		// Look Left
	textitems[25].text = gLanguageStrings[75];		// Look Right
	textitems[27].text = gLanguageStrings[76];		// Look Behind
	textitems[21].text = gLanguageStrings[294];		// Drop Camera
	textitems[29].text = gLanguageStrings[61];		// Reset to defaults

	textitems[2].text = GetKeyName(UserKeys.SteerLeft);
	textitems[4].text = GetKeyName(UserKeys.SteerRight);
	textitems[6].text = GetKeyName(UserKeys.FastSteer);
	textitems[8].text = GetKeyName(UserKeys.Accelerate);
	textitems[10].text = GetKeyName(UserKeys.Brake);
	textitems[12].text = GetKeyName(UserKeys.CruiseControl);
	textitems[14].text = GetKeyName(UserKeys.Horn);
	textitems[16].text = GetKeyName(UserKeys.Burnout);
	textitems[18].text = GetKeyName(UserKeys.Handbrake);
	textitems[20].text = GetKeyName(UserKeys.ViewToggle);
	textitems[24].text = GetKeyName(UserKeys.LookLeft);
	textitems[26].text = GetKeyName(UserKeys.LookRight);
	textitems[28].text = GetKeyName(UserKeys.LookBehind);
	textitems[22].text = GetKeyName(UserKeys.DropCamera);
}

// [0x411466] [D]
static void steerleft_selfunc(int flag)
{
	if (flag == 0 || flag == 1)
	{
		textitems[1].font = 3;
		textitems[2].font = 3;
	}
	else if (flag == 2)
	{
		motel_item = 1;
		motel_item2 = 2;
	}
}

// [0x4114ac] [D]
static void steerleft_unselfunc(int flag)
{
	if (flag == 0)
	{
		textitems[1].font = 2;
		textitems[2].font = 2;
	}
	else if (flag == 2)
	{
		UnhighlightMotelItem();
	}
}

// [0x4114dd] [D]
static int steerleft_choosefunc(void)
{
	SettingKey = 1;
	KeyToSet = (char *)&UserKeys.SteerLeft;
	flash_item = 2;

	UnhighlightMotelItem();
	motel_item = flash_item;
	motel_item2 = -1;

	return 0;
}

// [0x41151b] [D]
static void steerright_selfunc(int flag)
{
	if (flag == 0 || flag == 1)
	{
		textitems[3].font = 3;
		textitems[4].font = 3;
	}
	else if (flag == 2)
	{
		motel_item = 3;
		motel_item2 = 4;
	}
}

// [0x411561] [D]
static void steerright_unselfunc(int flag)
{
	if (flag == 0)
	{
		textitems[3].font = 2;
		textitems[4].font = 2;
	}
	else if (flag == 2)
	{
		UnhighlightMotelItem();
	}
}

// [0x411592] [D]
static int steerright_choosefunc(void)
{
	SettingKey = 1;
	KeyToSet = (char *)&UserKeys.SteerRight;
	flash_item = 4;

	UnhighlightMotelItem();
	motel_item = flash_item;
	motel_item2 = -1;

	return 0;
}

// [0x4115d0] [D]
static void faststeer_selfunc(int flag)
{
	if (flag == 0 || flag == 1)
	{
		textitems[5].font = 3;
		textitems[6].font = 3;
	}
	else if (flag == 2)
	{
		motel_item = 5;
		motel_item2 = 6;
	}
}

// [0x411616] [D]
static void faststeer_unselfunc(int flag)
{
	if (flag == 0)
	{
		textitems[5].font = 2;
		textitems[6].font = 2;
	}
	else if (flag == 2)
	{
		UnhighlightMotelItem();
	}
}

// [0x411647] [D]
static int faststeer_choosefunc(void)
{
	SettingKey = 1;
	KeyToSet = (char *)&UserKeys.FastSteer;
	flash_item = 6;

	UnhighlightMotelItem();
	motel_item = flash_item;
	motel_item2 = -1;

	return 0;
}

// [0x411685] [D]
static void accelerate_selfunc(int flag)
{
	if (flag == 0 || flag == 1)
	{
		textitems[7].font = 3;
		textitems[8].font = 3;
	}
	else if (flag == 2)
	{
		motel_item = 7;
		motel_item2 = 8;
	}
}

// [0x4116cb] [D]
static void accelerate_unselfunc(int flag)
{
	if (flag == 0)
	{
		textitems[7].font = 2;
		textitems[8].font = 2;
	}
	else if (flag == 2)
	{
		UnhighlightMotelItem();
	}
}

// [0x4116fc] [D]
static int accelerate_choosefunc(void)
{
	SettingKey = 1;
	KeyToSet = (char *)&UserKeys.Accelerate;
	flash_item = 8;

	UnhighlightMotelItem();
	motel_item = flash_item;
	motel_item2 = -1;

	return 0;
}

// [0x41173a] [D]
static void burnout_selfunc(int flag)
{
	if (flag == 0 || flag == 1)
	{
		textitems[15].font = 3;
		textitems[16].font = 3;
	}
	else if (flag == 2)
	{
		motel_item = 15;
		motel_item2 = 16;
	}
}

// [0x411780] [D]
static void burnout_unselfunc(int flag)
{
	if (flag == 0)
	{
		textitems[15].font = 2;
		textitems[16].font = 2;
	}
	else if (flag == 2)
	{
		UnhighlightMotelItem();
	}
}

// [0x4117b1] [D]
static int burnout_choosefunc(void)
{
	SettingKey = 1;
	KeyToSet = (char *)&UserKeys.Burnout;
	flash_item = 16;

	UnhighlightMotelItem();
	motel_item = flash_item;
	motel_item2 = -1;

	return 0;
}

// [0x4117ef] [D]
static void brake_selfunc(int flag)
{
	if (flag == 0 || flag == 1)
	{
		textitems[9].font = 3;
		textitems[10].font = 3;
	}
	else if (flag == 2)
	{
		motel_item = 9;
		motel_item2 = 10;
	}
}

// [0x411835] [D]
static void brake_unselfunc(int flag)
{
	if (flag == 0)
	{
		textitems[9].font = 2;
		textitems[10].font = 2;
	}
	else if (flag == 2)
	{
		UnhighlightMotelItem();
	}
}

// [0x411866] [D]
static int brake_choosefunc(void)
{
	SettingKey = 1;
	KeyToSet = (char *)&UserKeys.Brake;
	flash_item = 10;

	UnhighlightMotelItem();
	motel_item = flash_item;
	motel_item2 = -1;

	return 0;
}

// [0x4118a4] [D]
static void handbrake_selfunc(int flag)
{
	if (flag == 0 || flag == 1)
	{
		textitems[17].font = 3;
		textitems[18].font = 3;
	}
	else if (flag == 2)
	{
		motel_item = 17;
		motel_item2 = 18;
	}
}

// [0x4118ea] [D]
static void handbrake_unselfunc(int flag)
{
	if (flag == 0)
	{
		textitems[17].font = 2;
		textitems[18].font = 2;
	}
	else if (flag == 2)
	{
		UnhighlightMotelItem();
	}
}

// [0x41191b] [D]
static int handbrake_choosefunc(void)
{
	SettingKey = 1;
	KeyToSet = (char *)&UserKeys.Handbrake;
	flash_item = 18;

	UnhighlightMotelItem();
	motel_item = flash_item;
	motel_item2 = -1;

	return 0;
}

// [0x411959] [D]
static void viewtoggle_selfunc(int flag)
{
	if (flag == 0 || flag == 1)
	{
		textitems[19].font = 3;
		textitems[20].font = 3;
	}
	else if (flag == 2)
	{
		motel_item = 19;
		motel_item2 = 20;
	}
}

// [0x41199f] [D]
static void viewtoggle_unselfunc(int flag)
{
	if (flag == 0)
	{
		textitems[19].font = 2;
		textitems[20].font = 2;
	}
	else if (flag == 2)
	{
		UnhighlightMotelItem();
	}
}

// [0x4119d0] [D]
static int viewtoggle_choosefunc(void)
{
	SettingKey = 1;
	KeyToSet = (char *)&UserKeys.ViewToggle;
	flash_item = 20;

	UnhighlightMotelItem();
	motel_item = flash_item;
	motel_item2 = -1;

	return 0;
}

// [0x411a0e] [D]
static void lookleft_selfunc(int flag)
{
	if (flag == 0 || flag == 1)
	{
		textitems[23].font = 3;
		textitems[24].font = 3;
	}
	else if (flag == 2)
	{
		motel_item = 23;
		motel_item2 = 24;
	}
}

// [0x411a54] [D]
static void lookleft_unselfunc(int flag)
{
	if (flag == 0)
	{
		textitems[23].font = 2;
		textitems[24].font = 2;
	}
	else if (flag == 2)
	{
		UnhighlightMotelItem();
	}
}

// [0x411a85] [D]
static int lookleft_choosefunc(void)
{
	SettingKey = 1;
	KeyToSet = (char *)&UserKeys.LookLeft;
	flash_item = 24;

	UnhighlightMotelItem();
	motel_item = flash_item;
	motel_item2 = -1;

	return 0;
}

// [0x411ac3] [D]
static void lookright_selfunc(int flag)
{
	if (flag == 0 || flag == 1)
	{
		textitems[25].font = 3;
		textitems[26].font = 3;
	}
	else if (flag == 2)
	{
		motel_item = 25;
		motel_item2 = 26;
	}
}

// [0x411b09] [D]
static void lookright_unselfunc(int flag)
{
	if (flag == 0)
	{
		textitems[25].font = 2;
		textitems[26].font = 2;
	}
	else if (flag == 2)
	{
		UnhighlightMotelItem();
	}
}

// [0x411b3a] [D]
static int lookright_choosefunc(void)
{
	SettingKey = 1;
	KeyToSet = (char *)&UserKeys.LookRight;
	flash_item = 26;

	UnhighlightMotelItem();
	motel_item = flash_item;
	motel_item2 = -1;

	return 0;
}

// [0x411b78] [D]
static void lookbehind_selfunc(int flag)
{
	if (flag == 0 || flag == 1)
	{
		textitems[27].font = 3;
		textitems[28].font = 3;
	}
	else if (flag == 2)
	{
		motel_item = 27;
		motel_item2 = 28;
	}
}

// [0x411bbe] [D]
static void lookbehind_unselfunc(int flag)
{
	if (flag == 0)
	{
		textitems[27].font = 2;
		textitems[28].font = 2;
	}
	else if (flag == 2)
	{
		UnhighlightMotelItem();
	}
}

// [0x411bef] [D]
static int lookbehind_choosefunc(void)
{
	SettingKey = 1;
	KeyToSet = (char *)&UserKeys.LookBehind;
	flash_item = 28;

	UnhighlightMotelItem();
	motel_item = flash_item;
	motel_item2 = -1;

	return 0;
}

// [0x411c2d] [D]
static void cruisecontrol_selfunc(int flag)
{
	if (flag == 0 || flag == 1)
	{
		textitems[11].font = 3;
		textitems[12].font = 3;
	}
	else if (flag == 2)
	{
		motel_item = 11;
		motel_item2 = 12;
	}
}

// [0x411c73] [D]
static void cruisecontrol_unselfunc(int flag)
{
	if (flag == 0)
	{
		textitems[11].font = 2;
		textitems[12].font = 2;
	}
	else if (flag == 2)
	{
		UnhighlightMotelItem();
	}
}

// [0x411ca4] [D]
static int cruisecontrol_choosefunc(void)
{
	SettingKey = 1;
	KeyToSet = (char *)&UserKeys.CruiseControl;
	flash_item = 12;

	UnhighlightMotelItem();
	motel_item = flash_item;
	motel_item2 = -1;

	return 0;
}

// [0x411ce2] [D]
static void horn_selfunc(int flag)
{
	if (flag == 0 || flag == 1)
	{
		textitems[13].font = 3;
		textitems[14].font = 3;
	}
	else if (flag == 2)
	{
		motel_item = 13;
		motel_item2 = 14;
	}
}

// [0x411d28] [D]
static void horn_unselfunc(int flag)
{
	if (flag == 0)
	{
		textitems[13].font = 2;
		textitems[14].font = 2;
	}
	else if (flag == 2)
	{
		UnhighlightMotelItem();
	}
}

// [0x411d59] [D]
static int horn_choosefunc(void)
{
	SettingKey = 1;
	KeyToSet = (char *)&UserKeys.Horn;
	flash_item = 14;

	UnhighlightMotelItem();
	motel_item = flash_item;
	motel_item2 = -1;

	return 0;
}

// [0x411d97] [D]
static void dropcamera_selfunc(int flag)
{
	if (flag == 0 || flag == 1)
	{
		textitems[21].font = 3;
		textitems[22].font = 3;
	}
	else if (flag == 2)
	{
		motel_item = 21;
		motel_item2 = 22;
	}
}

// [0x411ddd] [D]
static void dropcamera_unselfunc(int flag)
{
	if (flag == 0)
	{
		textitems[21].font = 2;
		textitems[22].font = 2;
	}
	else if (flag == 2)
	{
		UnhighlightMotelItem();
	}
}

// [0x411e0e] [D]
static int dropcamera_choosefunc(void)
{
	SettingKey = 1;
	KeyToSet = (char *)&UserKeys.DropCamera;
	flash_item = 22;

	UnhighlightMotelItem();
	motel_item = flash_item;
	motel_item2 = -1;

	return 0;
}

// [0x411e4c] [D]
static void resetdefaults_selfunc(int flag)
{
	if (flag == 0 || flag == 1)
	{
		textitems[29].font = 3;
	}
	else if (flag == 2)
	{
		motel_item = 29;
		motel_item2 = -1;
	}
}

// [0x411e8b] [D]
static void resetdefaults_unselfunc(int flag)
{
	if (flag == 0)
	{
		textitems[29].font = 2;
	}
	else if (flag == 2)
	{
		UnhighlightMotelItem();
	}
}

// [0x411eb5] [D]
static int resetdefaults_choosefunc(void)
{
	KeySet = DefaultKeySet;
	UserKeys = DefaultKeySet;

	SetupKeyboardRedefine();
	return 0;
}

// [0x411ee7] [D]
// NOTE: sub_411EE7, the name is a guess
// Draws the texts of the motel room item in red.
static void HighlightMotelItem(void)
{
	textitems[motel_item].color.r = 255;
	textitems[motel_item].color.g = 0;
	textitems[motel_item].color.b = 0;

	if (motel_item2 != -1)
	{
		textitems[motel_item2].color.r = 255;
		textitems[motel_item2].color.g = 0;
		textitems[motel_item2].color.b = 0;
	}
}

// [0x411f53] [D]
// NOTE: sub_411F53, the name is a guess
static void UnhighlightMotelItem(void)
{
	textitems[motel_item].color.r = 255;
	textitems[motel_item].color.g = 255;
	textitems[motel_item].color.b = 255;

	if (motel_item2 != -1)
	{
		textitems[motel_item2].color.r = 255;
		textitems[motel_item2].color.g = 255;
		textitems[motel_item2].color.b = 255;
	}
}

// [0x411fbf] [D]
// NOTE: sub_411FBF, the name is a guess
// 1 when no two actions share a key.
static int CheckKeys(void)
{
	u_char keys[16];
	int i;
	int j;

	keys[0] = UserKeys.SteerLeft;
	keys[1] = UserKeys.SteerRight;
	keys[2] = UserKeys.Accelerate;
	keys[3] = UserKeys.Brake;
	keys[4] = UserKeys.FastSteer;
	keys[5] = UserKeys.Handbrake;
	keys[6] = UserKeys.Burnout;
	keys[7] = UserKeys.LookLeft;
	keys[8] = UserKeys.LookRight;
	keys[9] = UserKeys.LookBehind;
	keys[10] = UserKeys.CruiseControl;
	keys[11] = UserKeys.Horn;
	keys[12] = UserKeys.ViewToggle;
	keys[13] = UserKeys.DropCamera;

	for (i = 0; i < 14; i++)
	{
		for (j = i + 1; j < 14; j++)
		{
			if (keys[i] == keys[j])
				return 0;
		}
	}

	return 1;
}
