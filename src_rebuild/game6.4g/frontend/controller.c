/*
 * frontend/controller.c - controller options: input device, key and joystick maps, analogue,
 * dead zone, force feedback; also used in the motel room
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "frontend\controller.c", 0x4071f0 - 0x407927)
 */

#include <stdio.h>

#include "dr_types.h"
#include "dr_globals.h"
#include "pres.h"
#include "shell.h"
#include "input.h"
#include "flat/frontend.h"
#include "frontend/frontend.h"
#include "frontend/controller.h"
#include "frontend/joymap.h"
#include "frontend/keymap.h"

void SetupController(void);
void SetOptionsController(void);
static void device_selfunc(int flag);
static void device_unselfunc(int flag);
static int device_choosefunc(void);
static void keys_selfunc(int flag);
static void keys_unselfunc(int flag);
static int keys_choosefunc(void);
static void joykeys_selfunc(int flag);
static void joykeys_unselfunc(int flag);
static int joykeys_choosefunc(void);
static void forcefeedback_selfunc(int flag);
static void forcefeedback_unselfunc(int flag);
static int forcefeedback_choosefunc(void);
static void deadzone_selfunc(int flag);
static void deadzone_unselfunc(int flag);
static int deadzone_choosefunc(void);
static void analogue_selfunc(int flag);
static void analogue_unselfunc(int flag);
static int analogue_choosefunc(void);
static void HighlightMotelItem(void);
static void UnhighlightMotelItem(void);

//------------------------------------------------------------------------------------------
// zero-initialised data (0x57d970)

static char device_name[40];
static int selection;
// NOTE: dword_57D99C; 0: keyboard, n: joystick n - 1
static int device;
// NOTE: dword_57D9A0; the (first) text of the item highlighted in the motel room
static int motel_item;
// NOTE: unk_57D9A4: the text of the end entry of polyitems
static char poly_end_text[4];

//------------------------------------------------------------------------------------------
// initialised data (0x54bf68)

static FE_POLY polyitems[1] = {
	{ 0, 0, poly_end_text, 4, 0, 0, { 0, 0, 0, 0 }, 0 },
};

static FE_TEXT textitems[13] = {
	{ 40, 150, 0, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 60, 200, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 580, 200, 2, 2, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 40, 240, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 60, 265, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 60, 290, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 60, 315, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 580, 315, 2, 2, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 60, 340, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 580, 340, 2, 2, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 60, 365, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 580, 365, 2, 2, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ -1, -1, 0, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
};

static FE_BUTTON menufunc[7] = {
	{ device_selfunc, device_unselfunc, device_choosefunc, 0, 0, 5, 1 },
	{ keys_selfunc, keys_unselfunc, keys_choosefunc, 1, 1, 0, 2 },
	{ joykeys_selfunc, joykeys_unselfunc, joykeys_choosefunc, 2, 2, 1, 3 },
	{ analogue_selfunc, analogue_unselfunc, analogue_choosefunc, 3, 3, 2, 4 },
	{ deadzone_selfunc, deadzone_unselfunc, deadzone_choosefunc, 4, 4, 3, 5 },
	{ forcefeedback_selfunc, forcefeedback_unselfunc, forcefeedback_choosefunc, 5, 5, 4, 0 },
	{ NULL, NULL, NULL, 0, 0, 0, 0 },
};

// gLanguageStrings indices: off, on
static int onoff[2] = { 2, 1 };
static char *deadzone_names[5] = { "10%", "20%", "30%", "40%", "50%" };
// the second text of the item highlighted in the motel room, -1: none
static int motel_item2 = -1;

// [0x4071f0] [D]
// In the motel room the menu is drawn over the room and leaving it returns 0.
int SelectController(void)
{
	int ret;

	SetupController();

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

		if (gDoMotelRoom)
		{
			selection = MotelMenuKeyControl(menufunc, selection);

			if (selection == -1)
			{
				ret = 0;
				break;
			}

			HighlightMotelItem();
			DrawMotel(textitems, NULL);
		}
		else
		{
			ret = HandleMenu(menufunc, &selection);

			if (ret != 0)
				break;

			DrawFrontend();
		}
	}

	if (ret != -1)
		return ret;

	if (gDoMotelRoom == 0)
		CloseFrontendScreen();

	return 0;
}

// [0x4072dc] [D]
void SetupController(void)
{
	if (InputDevice != INPUTDEVICE_KEYBOARD)
		device = GetJoystickDevice() + 1;
	else
		device = 0;

	textitems[0].text = gLanguageStrings[58];
	textitems[1].text = gLanguageStrings[59];
	textitems[3].text = gLanguageStrings[60];
	textitems[4].text = gLanguageStrings[62];
	textitems[5].text = gLanguageStrings[308];
	textitems[10].text = gLanguageStrings[63];
	textitems[6].text = gLanguageStrings[309];
	textitems[8].text = gLanguageStrings[310];

	SetOptionsController();
}

// [0x407366] [D]
// NOTE: IDA types it char *, but nothing uses the value left in eax
void SetOptionsController(void)
{
	if (device)
	{
		InputDevice = INPUTDEVICE_JOYSTICK;

		sprintf(device_name, "%s (J%d)", input_device_names[device - 1], device);
		textitems[2].text = device_name;
		textitems[11].text = gLanguageStrings[onoff[gForceFeedback]];
		textitems[7].text = gLanguageStrings[onoff[JoyIsAnalogue]];
		textitems[9].text = deadzone_names[JoyDeadZone];
	}
	else
	{
		InputDevice = INPUTDEVICE_KEYBOARD;

		textitems[2].text = gLanguageStrings[64];
		textitems[11].text = gLanguageStrings[12];
		textitems[7].text = gLanguageStrings[12];
		textitems[9].text = gLanguageStrings[12];
	}
}

// [0x40742d] [D]
// flag 0 / 1: from HandleMenu; 2: from the motel room (the item is highlighted by colour)
static void device_selfunc(int flag)
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

// [0x407473] [D]
static void device_unselfunc(int flag)
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

// [0x4074a4] [D]
// Moves on to the next joystick, after the last one back to the keyboard.
static int device_choosefunc(void)
{
	if (SetJoystickDevice(device++))
	{
		if (gForceFeedback)
			gForceFeedback = JoyIsFF;
	}
	else
	{
		device = 0;
	}

	SetOptionsController();
	return 0;
}

// [0x4074f4] [D]
static void keys_selfunc(int flag)
{
	if (flag == 0 || flag == 1)
	{
		textitems[4].font = 3;
	}
	else if (flag == 2)
	{
		motel_item = 4;
		motel_item2 = -1;
	}
}

// [0x407533] [D]
static void keys_unselfunc(int flag)
{
	if (flag == 0)
		textitems[4].font = 2;
	else if (flag == 2)
		UnhighlightMotelItem();
}

// [0x40755d] [D]
static int keys_choosefunc(void)
{
	int ret;

	if (gDoMotelRoom == 0)
		CloseFrontendScreen();

	ret = SelectKeyboardRedefine();

	if (ret == 0 && gDoMotelRoom == 0)
		OpenFrontendScreen(polyitems, textitems);

	return ret;
}

// [0x4075a6] [D]
static void joykeys_selfunc(int flag)
{
	if (flag == 0 || flag == 1)
	{
		textitems[5].font = 3;
	}
	else if (flag == 2)
	{
		motel_item = 5;
		motel_item2 = -1;
	}
}

// [0x4075e5] [D]
static void joykeys_unselfunc(int flag)
{
	if (flag == 0)
		textitems[5].font = 2;
	else if (flag == 2)
		UnhighlightMotelItem();
}

// [0x40760f] [D]
static int joykeys_choosefunc(void)
{
	int ret;

	ret = 0;

	if (device)
	{
		if (gDoMotelRoom == 0)
			CloseFrontendScreen();

		ret = SelectJoystickRedefine();

		if (ret == 0 && gDoMotelRoom == 0)
			OpenFrontendScreen(polyitems, textitems);
	}

	return ret;
}

// [0x407661] [D]
static void forcefeedback_selfunc(int flag)
{
	if (flag == 0 || flag == 1)
	{
		textitems[10].font = 3;
		textitems[11].font = 3;
	}
	else if (flag == 2)
	{
		motel_item = 10;
		motel_item2 = 11;
	}
}

// [0x4076a7] [D]
static void forcefeedback_unselfunc(int flag)
{
	if (flag == 0)
	{
		textitems[10].font = 2;
		textitems[11].font = 2;
	}
	else if (flag == 2)
	{
		UnhighlightMotelItem();
	}
}

// [0x4076d8] [D]
static int forcefeedback_choosefunc(void)
{
	if (device)
	{
		if (JoyIsFF)
			gForceFeedback ^= 1;
		else
			gForceFeedback = 0;

		SetOptionsController();
	}

	return 0;
}

// [0x40770f] [D]
static void deadzone_selfunc(int flag)
{
	if (flag == 0 || flag == 1)
	{
		textitems[8].font = 3;
		textitems[9].font = 3;
	}
	else if (flag == 2)
	{
		motel_item = 8;
		motel_item2 = 9;
	}
}

// [0x407755] [D]
static void deadzone_unselfunc(int flag)
{
	if (flag == 0)
	{
		textitems[8].font = 2;
		textitems[9].font = 2;
	}
	else if (flag == 2)
	{
		UnhighlightMotelItem();
	}
}

// [0x407786] [D]
static int deadzone_choosefunc(void)
{
	if (device)
	{
		JoyDeadZone++;		// SetJoystickDeadZone wraps it around
		SetJoystickDeadZone();
		SetOptionsController();
	}

	return 0;
}

// [0x4077ad] [D]
static void analogue_selfunc(int flag)
{
	if (flag == 0 || flag == 1)
	{
		textitems[6].font = 3;
		textitems[7].font = 3;
	}
	else if (flag == 2)
	{
		motel_item = 6;
		motel_item2 = 7;
	}
}

// [0x4077f3] [D]
static void analogue_unselfunc(int flag)
{
	if (flag == 0)
	{
		textitems[6].font = 2;
		textitems[7].font = 2;
	}
	else if (flag == 2)
	{
		UnhighlightMotelItem();
	}
}

// [0x407824] [D]
static int analogue_choosefunc(void)
{
	if (device)
	{
		JoyIsAnalogue ^= 1;
		SetOptionsController();
	}

	return 0;
}

// [0x407846] [D]
// NOTE: sub_407846, the name is a guess
// Draws the texts of the motel room item in red.
static void HighlightMotelItem(void)
{
	if (selection == -1)
		return;

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

// [0x4078bb] [D]
// NOTE: sub_4078BB, the name is a guess
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
