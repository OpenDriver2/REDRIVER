/*
 * frontend/joymap.c - joystick button redefinition; also used in the motel room
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "frontend\joymap.c", 0x40fda0 - 0x410c83)
 *
 * NOTE: the Look Left / Look Right / Look Behind items set and show JoySet.LookRight /
 * LookBehind / LookLeft (as in the original; the IDA field names of CONTROL_JOY_SET may be off)
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
#include "frontend/joymap.h"

void SetupJoystickRedefine(void);
static void accelerate_selfunc(int flag);
static void accelerate_unselfunc(int flag);
static int accelerate_choosefunc(void);
static void brake_selfunc(int flag);
static void brake_unselfunc(int flag);
static int brake_choosefunc(void);
static void faststeer_selfunc(int flag);
static void faststeer_unselfunc(int flag);
static int faststeer_choosefunc(void);
static void burnout_selfunc(int flag);
static void burnout_unselfunc(int flag);
static int burnout_choosefunc(void);
static void handbrake_selfunc(int flag);
static void handbrake_unselfunc(int flag);
static int handbrake_choosefunc(void);
static void lookleft_selfunc(int flag);
static void lookleft_unselfunc(int flag);
static int lookleft_choosefunc(void);
static void lookright_selfunc(int flag);
static void lookright_unselfunc(int flag);
static int lookright_choosefunc(void);
static void lookbehind_selfunc(int flag);
static void lookbehind_unselfunc(int flag);
static int lookbehind_choosefunc(void);
static void dropcamera_selfunc(int flag);
static void dropcamera_unselfunc(int flag);
static int dropcamera_choosefunc(void);
static void cruisecontrol_selfunc(int flag);
static void cruisecontrol_unselfunc(int flag);
static int cruisecontrol_choosefunc(void);
static void horn_selfunc(int flag);
static void horn_unselfunc(int flag);
static int horn_choosefunc(void);
static void viewtoggle_selfunc(int flag);
static void viewtoggle_unselfunc(int flag);
static int viewtoggle_choosefunc(void);
static void resetdefaults_selfunc(int flag);
static void resetdefaults_unselfunc(int flag);
static int resetdefaults_choosefunc(void);
static void HighlightMotelItem(void);
static void UnhighlightMotelItem(void);
static int CheckJoyKeys(void);

//------------------------------------------------------------------------------------------
// uninitialised data (0x6009cc)

static int selection;
// NOTE: dword_6009D0; the item to return to when the motel menu is left with clashing buttons
static int last_selection;
char *JoyKeyToSet;			// the JoySet button being redefined
// NOTE: dword_6009D8; the text that flashes while a button is redefined
static int flash_item;
// NOTE: dword_6009DC
static int flash_timer;
// NOTE: dword_6009E0, dword_6009E4; the texts of the item highlighted in the motel room (-1: none)
static int motel_item;
static int motel_item2;
// NOTE: byte_6009E8, a bss buffer: the text of the end entry of polyitems
static char poly_end_text[4];

//------------------------------------------------------------------------------------------
// initialised data (0x54e108)

static FE_POLY polyitems[1] = {
	{ 0, 0, poly_end_text, 4, 0, 0, { 0, 0, 0, 0 }, 0 },
};

// [0] title, [1 + 2 * i] action, [2 + 2 * i] its button, [25] reset to defaults, [26] the hint
static FE_TEXT textitems[28] = {
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
	{ 40, 370, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 320, 395, 2, 1, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ -1, -1, 0, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
};

static FE_BUTTON menufunc[14] = {
	{ accelerate_selfunc, accelerate_unselfunc, accelerate_choosefunc, 6, 6, 12, 1 },
	{ brake_selfunc, brake_unselfunc, brake_choosefunc, 7, 7, 0, 2 },
	{ burnout_selfunc, burnout_unselfunc, burnout_choosefunc, 8, 8, 1, 3 },
	{ handbrake_selfunc, handbrake_unselfunc, handbrake_choosefunc, 9, 9, 2, 4 },
	{ faststeer_selfunc, faststeer_unselfunc, faststeer_choosefunc, 10, 10, 3, 5 },
	{ cruisecontrol_selfunc, cruisecontrol_unselfunc, cruisecontrol_choosefunc, 11, 11, 4, 12 },
	{ horn_selfunc, horn_unselfunc, horn_choosefunc, 0, 0, 11, 7 },
	{ viewtoggle_selfunc, viewtoggle_unselfunc, viewtoggle_choosefunc, 1, 1, 6, 8 },
	{ dropcamera_selfunc, dropcamera_unselfunc, dropcamera_choosefunc, 2, 2, 7, 9 },
	{ lookleft_selfunc, lookleft_unselfunc, lookleft_choosefunc, 3, 3, 8, 10 },
	{ lookright_selfunc, lookright_unselfunc, lookright_choosefunc, 4, 4, 9, 11 },
	{ lookbehind_selfunc, lookbehind_unselfunc, lookbehind_choosefunc, 5, 5, 10, 6 },
	{ resetdefaults_selfunc, resetdefaults_unselfunc, resetdefaults_choosefunc, 12, 12, 5, 0 },
	{ NULL, NULL, NULL, 0, 0, 0, 0 },
};

// [A] the button each entry of menufunc sets, as their choosefuncs do
static char *const menu_keys[12] = {
	(char *)&JoySet.Accelerate, (char *)&JoySet.Brake, (char *)&JoySet.Burnout, (char *)&JoySet.Handbrake,
	(char *)&JoySet.FastSteer, (char *)&JoySet.CruiseControl, (char *)&JoySet.Horn, (char *)&JoySet.ViewToggle,
	(char *)&JoySet.DropCamera, (char *)&JoySet.LookRight, (char *)&JoySet.LookBehind, (char *)&JoySet.LookLeft,
};

// [0x40fda0] [D] [A]
// While a button is being redefined its text flashes until a joystick button (or Space: none)
// is pressed; Escape cancels. The menu can only be left when no two actions share a button.
// NOTE: [A] Backspace (B on a controller) clears the selected action: while redefining, every
// controller button binds, so B couldn't do it from there
int SelectJoystickRedefine(void)
{
	int ret;
	int button;

	SetupJoystickRedefine();

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

		if (SettingJoyKey)
		{
			if (CheckKey(DIK_ESCAPE))
			{
				FE_PlayClickSound();
				SettingJoyKey = 0;
				menufunc[selection].selfunc(gDoMotelRoom ? 2 : 0);
			}
			else
			{
				if (--flash_timer <= 0)
				{
					flash_timer = 10;
					textitems[flash_item].font ^= 1;
				}

				button = GetJoyKeyInput();

				if (button >= 0)
				{
					FE_PlayClickSound();
					*JoyKeyToSet = button;
					SettingJoyKey = 0;
					menufunc[selection].selfunc(gDoMotelRoom ? 2 : 0);
					SetupJoystickRedefine();
				}
			}
		}
		else if (CheckKey(DIK_BACK) && selection < 12)
		{
			FE_PlayClickSound();
			*menu_keys[selection] = 34;
			SetupJoystickRedefine();
		}
		else if (gDoMotelRoom)
		{
			last_selection = selection;
			selection = MotelMenuKeyControl(menufunc, selection);

			if (selection == -1)
			{
				if (CheckJoyKeys())
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
				if (CheckJoyKeys())
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

	if (ret != -1)
		return ret;

	if (gDoMotelRoom == 0)
		CloseFrontendScreen();

	return 0;
}

// [0x40ffda] [D]
void SetupJoystickRedefine(void)
{
	textitems[0].text = gLanguageStrings[66];		// Redefine:
	textitems[1].text = gLanguageStrings[70];		// Accelerate
	textitems[3].text = gLanguageStrings[72];		// Brake
	textitems[5].text = gLanguageStrings[71];		// Burn Out
	textitems[9].text = gLanguageStrings[69];		// Fast Steer
	textitems[7].text = gLanguageStrings[73];		// Handbrake
	textitems[11].text = gLanguageStrings[77];		// Cruise Control
	textitems[13].text = gLanguageStrings[78];		// Horn
	textitems[15].text = gLanguageStrings[230];		// View Toggle
	textitems[17].text = gLanguageStrings[294];		// Drop Camera
	textitems[19].text = gLanguageStrings[74];		// Look Left
	textitems[21].text = gLanguageStrings[75];		// Look Right
	textitems[23].text = gLanguageStrings[76];		// Look Behind

	textitems[2].text = GetJoyKeyName(JoySet.Accelerate);
	textitems[4].text = GetJoyKeyName(JoySet.Brake);
	textitems[10].text = GetJoyKeyName(JoySet.FastSteer);
	textitems[6].text = GetJoyKeyName(JoySet.Burnout);
	textitems[8].text = GetJoyKeyName(JoySet.Handbrake);
	textitems[12].text = GetJoyKeyName(JoySet.CruiseControl);
	textitems[14].text = GetJoyKeyName(JoySet.Horn);
	textitems[16].text = GetJoyKeyName(JoySet.ViewToggle);
	textitems[18].text = GetJoyKeyName(JoySet.DropCamera);
	textitems[20].text = GetJoyKeyName(JoySet.LookRight);
	textitems[22].text = GetJoyKeyName(JoySet.LookBehind);
	textitems[24].text = GetJoyKeyName(JoySet.LookLeft);

	textitems[25].text = gLanguageStrings[61];		// Reset to defaults
	textitems[26].text = gLanguageStrings[209];		// *Space to clear
}

// [0x410175] [D]
static void accelerate_selfunc(int flag)
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

// [0x4101bb] [D]
static void accelerate_unselfunc(int flag)
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

// [0x4101ec] [D]
static int accelerate_choosefunc(void)
{
	SettingJoyKey = 1;
	JoyKeyToSet = (char *)&JoySet.Accelerate;
	flash_item = 2;
	ReadInput();

	UnhighlightMotelItem();
	motel_item = flash_item;
	motel_item2 = -1;

	return 0;
}

// [0x41022f] [D]
static void brake_selfunc(int flag)
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

// [0x410275] [D]
static void brake_unselfunc(int flag)
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

// [0x4102a6] [D]
static int brake_choosefunc(void)
{
	SettingJoyKey = 1;
	JoyKeyToSet = (char *)&JoySet.Brake;
	flash_item = 4;
	ReadInput();

	UnhighlightMotelItem();
	motel_item = flash_item;
	motel_item2 = -1;

	return 0;
}

// [0x4102e9] [D]
static void faststeer_selfunc(int flag)
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

// [0x41032f] [D]
static void faststeer_unselfunc(int flag)
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

// [0x410360] [D]
static int faststeer_choosefunc(void)
{
	SettingJoyKey = 1;
	JoyKeyToSet = (char *)&JoySet.FastSteer;
	flash_item = 10;
	ReadInput();

	UnhighlightMotelItem();
	motel_item = flash_item;
	motel_item2 = -1;

	return 0;
}

// [0x4103a3] [D]
static void burnout_selfunc(int flag)
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

// [0x4103e9] [D]
static void burnout_unselfunc(int flag)
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

// [0x41041a] [D]
static int burnout_choosefunc(void)
{
	SettingJoyKey = 1;
	JoyKeyToSet = (char *)&JoySet.Burnout;
	flash_item = 6;
	ReadInput();

	UnhighlightMotelItem();
	motel_item = flash_item;
	motel_item2 = -1;

	return 0;
}

// [0x41045d] [D]
static void handbrake_selfunc(int flag)
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

// [0x4104a3] [D]
static void handbrake_unselfunc(int flag)
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

// [0x4104d4] [D]
static int handbrake_choosefunc(void)
{
	SettingJoyKey = 1;
	JoyKeyToSet = (char *)&JoySet.Handbrake;
	flash_item = 8;
	ReadInput();

	UnhighlightMotelItem();
	motel_item = flash_item;
	motel_item2 = -1;

	return 0;
}

// [0x410517] [D]
static void lookleft_selfunc(int flag)
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

// [0x41055d] [D]
static void lookleft_unselfunc(int flag)
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

// [0x41058e] [D]
static int lookleft_choosefunc(void)
{
	SettingJoyKey = 1;
	JoyKeyToSet = (char *)&JoySet.LookRight;
	flash_item = 20;
	ReadInput();

	UnhighlightMotelItem();
	motel_item = flash_item;
	motel_item2 = -1;

	return 0;
}

// [0x4105d1] [D]
static void lookright_selfunc(int flag)
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

// [0x410617] [D]
static void lookright_unselfunc(int flag)
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

// [0x410648] [D]
static int lookright_choosefunc(void)
{
	SettingJoyKey = 1;
	JoyKeyToSet = (char *)&JoySet.LookBehind;
	flash_item = 22;
	ReadInput();

	UnhighlightMotelItem();
	motel_item = flash_item;
	motel_item2 = -1;

	return 0;
}

// [0x41068b] [D]
static void lookbehind_selfunc(int flag)
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

// [0x4106d1] [D]
static void lookbehind_unselfunc(int flag)
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

// [0x410702] [D]
static int lookbehind_choosefunc(void)
{
	SettingJoyKey = 1;
	JoyKeyToSet = (char *)&JoySet.LookLeft;
	flash_item = 24;
	ReadInput();

	UnhighlightMotelItem();
	motel_item = flash_item;
	motel_item2 = -1;

	return 0;
}

// [0x410745] [D]
static void dropcamera_selfunc(int flag)
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

// [0x41078b] [D]
static void dropcamera_unselfunc(int flag)
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

// [0x4107bc] [D]
static int dropcamera_choosefunc(void)
{
	SettingJoyKey = 1;
	JoyKeyToSet = (char *)&JoySet.DropCamera;
	flash_item = 18;
	ReadInput();

	UnhighlightMotelItem();
	motel_item = flash_item;
	motel_item2 = -1;

	return 0;
}

// [0x4107ff] [D]
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

// [0x410845] [D]
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

// [0x410876] [D]
static int cruisecontrol_choosefunc(void)
{
	SettingJoyKey = 1;
	JoyKeyToSet = (char *)&JoySet.CruiseControl;
	flash_item = 12;
	ReadInput();

	UnhighlightMotelItem();
	motel_item = flash_item;
	motel_item2 = -1;

	return 0;
}

// [0x4108b9] [D]
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

// [0x4108ff] [D]
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

// [0x410930] [D]
static int horn_choosefunc(void)
{
	SettingJoyKey = 1;
	JoyKeyToSet = (char *)&JoySet.Horn;
	flash_item = 14;
	ReadInput();

	UnhighlightMotelItem();
	motel_item = flash_item;
	motel_item2 = -1;

	return 0;
}

// [0x410973] [D]
static void viewtoggle_selfunc(int flag)
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

// [0x4109b9] [D]
static void viewtoggle_unselfunc(int flag)
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

// [0x4109ea] [D]
static int viewtoggle_choosefunc(void)
{
	SettingJoyKey = 1;
	JoyKeyToSet = (char *)&JoySet.ViewToggle;
	flash_item = 16;
	ReadInput();

	UnhighlightMotelItem();
	motel_item = flash_item;
	motel_item2 = -1;

	return 0;
}

// [0x410a2d] [D]
static void resetdefaults_selfunc(int flag)
{
	if (flag == 0 || flag == 1)
	{
		textitems[25].font = 3;
	}
	else if (flag == 2)
	{
		motel_item = 25;
		motel_item2 = -1;
	}
}

// [0x410a6c] [D]
static void resetdefaults_unselfunc(int flag)
{
	if (flag == 0)
	{
		textitems[25].font = 2;
	}
	else if (flag == 2)
	{
		UnhighlightMotelItem();
	}
}

// [0x410a96] [D]
static int resetdefaults_choosefunc(void)
{
	JoySet = DefaultJoySet;

	SetupJoystickRedefine();
	return 0;
}

// [0x410ac4] [D]
// NOTE: sub_410AC4, the name is a guess
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

// [0x410b30] [D]
// NOTE: sub_410B30, the name is a guess
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

// [0x410b9c] [D]
// NOTE: sub_410B9C, the name is a guess
// 1 when no two actions share a button (34: none).
static int CheckJoyKeys(void)
{
	u_char keys[12];
	int i;
	int j;

	keys[0] = JoySet.Accelerate;
	keys[1] = JoySet.Brake;
	keys[2] = JoySet.FastSteer;
	keys[3] = JoySet.Handbrake;
	keys[4] = JoySet.Burnout;
	keys[5] = JoySet.LookRight;
	keys[6] = JoySet.LookBehind;
	keys[7] = JoySet.LookLeft;
	keys[8] = JoySet.CruiseControl;
	keys[9] = JoySet.Horn;
	keys[10] = JoySet.ViewToggle;
	keys[11] = JoySet.DropCamera;

	for (i = 0; i < 12; i++)
	{
		if (keys[i] == 34)
			continue;

		for (j = i + 1; j < 12; j++)
		{
			if (keys[j] != 34 && keys[i] == keys[j])
				return 0;
		}
	}

	return 1;
}
