/*
 * frontend/gvars.c - key names and character tables, text and key input for the menus
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "frontend\gvars.c", 0x40ee70 - 0x40f481)
 */

#include <stdio.h>
#include <string.h>
#include <dinput.h>

#include "dr_types.h"
#include "dr_globals.h"
#include "general.h"
#include "input.h"
#include "frontend/gvars.h"

//------------------------------------------------------------------------------------------
// initialised data (0x54ced8)

// DIK_* -> character, for text input
char DIK_CharLookup[256] = {
	0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 0, 0,
	'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', 0, 0, 'a', 's',
	'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', 0, 0, '\\', 'z', 'x', 'c', 'v',
	'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ', 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, '7', '8', '9', '-', '4', '5', '6', '+', '1',
	'2', '3', '0', '.', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '=', 0, 0,
	0, 0, ':', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, ',', 0, '/', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

// DIK_* -> character with Shift or Caps Lock
char DIK_ShiftLookup[256] = {
	0, 0, '!', '2', '3', '4', '5', '6', '7', '8', '(', ')', '_', '+', 0, 0,
	'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '[', ']', 0, 0, 'A', 'S',
	'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', 0, 0, '\\', 'Z', 'X', 'C', 'V',
	'B', 'N', 'M', ',', '.', '?', 0, '*', 0, ' ', 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, '7', '8', '9', '-', '4', '5', '6', '+', '1',
	'2', '3', '0', '.', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '=', 0, 0,
	0, 0, ':', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, ',', 0, '/', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

// DIK_* -> key name; ReadKeyTextFile replaces the names of the special keys
char *DIK_NameLookup[256] = {
	[0x02] = "1",
	[0x03] = "2",
	[0x04] = "3",
	[0x05] = "4",
	[0x06] = "5",
	[0x07] = "6",
	[0x08] = "7",
	[0x09] = "8",
	[0x0A] = "9",
	[0x0B] = "0",
	[0x0C] = "-",
	[0x0D] = "=",
	[0x10] = "Q",
	[0x11] = "W",
	[0x12] = "E",
	[0x13] = "R",
	[0x14] = "T",
	[0x15] = "Y",
	[0x16] = "U",
	[0x17] = "I",
	[0x18] = "O",
	[0x19] = "P",
	[0x1A] = "[",
	[0x1B] = "]",
	[0x1C] = "Return",
	[0x1D] = "L Ctrl",
	[0x1E] = "A",
	[0x1F] = "S",
	[0x20] = "D",
	[0x21] = "F",
	[0x22] = "G",
	[0x23] = "H",
	[0x24] = "J",
	[0x25] = "K",
	[0x26] = "L",
	[0x27] = ";",
	[0x28] = "'",
	[0x2A] = "L Shift",
	[0x2B] = "\\",
	[0x2C] = "Z",
	[0x2D] = "X",
	[0x2E] = "C",
	[0x2F] = "V",
	[0x30] = "B",
	[0x31] = "N",
	[0x32] = "M",
	[0x33] = ",",
	[0x34] = ".",
	[0x35] = "/",
	[0x36] = "R Shift",
	[0x37] = "Num *",
	[0x38] = "L Alt",
	[0x39] = "Space",
	[0x45] = "Num Lock",
	[0x47] = "Num 7",
	[0x48] = "Num 8",
	[0x49] = "Num 9",
	[0x4A] = "Num -",
	[0x4B] = "Num 4",
	[0x4C] = "Num 5",
	[0x4D] = "Num 6",
	[0x4E] = "Num +",
	[0x4F] = "Num 1",
	[0x50] = "Num 2",
	[0x51] = "Num 3",
	[0x52] = "Num 0",
	[0x53] = "Num .",
	[0x8D] = "Num =",
	[0x92] = ":",
	[0x9C] = "Num Enter",
	[0x9D] = "R Ctrl",
	[0xB3] = "Num ,",
	[0xB5] = "Num /",
	[0xB8] = "R Alt",
	[0xC8] = "Up",
	[0xCB] = "Left",
	[0xCD] = "Right",
	[0xD0] = "Down",
};

// joystick button -> name; 32 and 33 are the hat up and down, 34 is "none"
char *DIJ_ButtonLookup[35] = {
	"B1", "B2", "B3", "B4", "B5", "B6", "B7", "B8", "B9", "B10", "B11", "B12", "B13", "B14", "B15", "B16",
	"B17", "B18", "B19", "B20", "B21", "B22", "B23", "B24", "B25", "B26", "B27", "B28", "B29", "B30", "B31", "B32",
	"Up", "Down", "None",
};

char *KeyLanguageNames[6] = { "ENG", "FRE", "GER", "SPA", "ITA", "USA" };

// names of the special keys, replaced from KEYS.<language>
char KeyStrings[33][16] = {
	"Return", "L Ctrl", "L Shift", "R Shift", "Num *", "L Alt", "Space", "Num Lock",
	"Num 7", "Num 8", "Num 9", "Num -", "Num 4", "Num 5", "Num 6", "Num +",
	"Num 1", "Num 2", "Num 3", "Num 0", "Num .", "Num =", "Num Enter", "R Ctrl",
	"Num ,", "Num /", "R Alt", "Up", "Left", "Right", "Down", "Error",
	"None",
};

// NOTE: moved here from the generated stubs; no IDA mark, the owner is a guess from the users
// and the neighbouring data
DISPLAY_MODE display_modes[8] = {
	{ 320, 240, "320 x 240", 1 },
	{ 512, 384, "512 x 384", 3 },
	{ 640, 480, "640 x 480", 7 },
	{ 800, 600, "800 x 600", 8 },
	{ 1024, 768, "1024 x 768", 12 },
	{ 1280, 1024, "1280 x 1024", 13 },
	{ 1600, 1200, "1600 x 1200", 14 },
	{ -1, -1, "", -1 },
};
CONTROL_KEY_SET DefaultKeySet = {
	0x3b, 0x3c, 0x3d, 0x3e, 0x2c, 0x2d, 0x28, 0x35, 0x39, 0x1c, 0x2b, 0x24, 0x25, 0x26, 0x2e, 0x23, 0x02, 0x03, 0x00, 0x00
};
CONTROL_KEY_SET UserKeys = {
	0x3b, 0x3c, 0x3d, 0x3e, 0x2c, 0x2d, 0x28, 0x35, 0x39, 0x1c, 0x2b, 0x24, 0x25, 0x26, 0x2e, 0x23, 0x02, 0x03, 0x00, 0x00
};
CONTROL_JOY_SET DefaultJoySet = {
	0x20, 0x21, 0x00, 0x02, 0x01, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09
};
int gUserLanguage;

// [0x40ee70] [D]
// NOTE: BUG: strncat appends to the default name, which overflows into the next one ("Num Enter"
// twice is 18 characters, so "R Ctrl" becomes "erR Ct")
// Reads the names of the special keys from KEYS.<language> (one per line) and adapts the
// character tables to the French and German keyboard layouts.
void ReadKeyTextFile(void)
{
	FILE *f;
	char *line;
	char *str;
	char *str_end;
	char *buffer;
	int length;
	char filename[256];
	int i;
	int fsize;

	fsize = 0;

	if ((unsigned int)gUserLanguage >= 6)
		gUserLanguage = 0;

	sprintf(filename, "KEYS.%s", KeyLanguageNames[gUserLanguage]);

	f = fopen(filename, "r");

	if (f == NULL)
		return;

	fseek(f, 0, SEEK_END);
	fsize = ftell(f);
	fseek(f, 0, SEEK_SET);

	buffer = (char *)DebugMalloc(fsize + 1, "D:\\Driver\\game6.4g\\frontend\\gvars.c", 235);

	if (buffer == NULL)
	{
		fclose(f);
		return;
	}

	fread(buffer, 1, fsize, f);
	fclose(f);

	buffer[fsize] = 0;
	line = buffer;

	for (i = 0; i < 33; i++)
	{
		if (fsize <= 0)
			continue;

		str_end = line;
		str = line;

		// the line without trailing control characters
		do
		{
			if (*line == '\n')
				break;

			if (*line >= ' ')
				str_end = line + 1;

			fsize--;
			line++;
		} while (fsize > 1);

		*str_end = 0;

		length = strlen(str);

		if (length >= 16)
			length = 15;

		strncat(KeyStrings[i], str, length);
		KeyStrings[i][length] = 0;

		line++;
		fsize--;
	}

	DebugFree(buffer);

	DIK_NameLookup[DIK_RETURN] = KeyStrings[0];
	DIK_NameLookup[DIK_LCONTROL] = KeyStrings[1];
	DIK_NameLookup[DIK_LSHIFT] = KeyStrings[2];
	DIK_NameLookup[DIK_RSHIFT] = KeyStrings[3];
	DIK_NameLookup[DIK_MULTIPLY] = KeyStrings[4];
	DIK_NameLookup[DIK_LMENU] = KeyStrings[5];
	DIK_NameLookup[DIK_SPACE] = KeyStrings[6];
	DIK_NameLookup[DIK_NUMLOCK] = KeyStrings[7];
	DIK_NameLookup[DIK_NUMPAD7] = KeyStrings[8];
	DIK_NameLookup[DIK_NUMPAD8] = KeyStrings[9];
	DIK_NameLookup[DIK_NUMPAD9] = KeyStrings[10];
	DIK_NameLookup[DIK_SUBTRACT] = KeyStrings[11];
	DIK_NameLookup[DIK_NUMPAD4] = KeyStrings[12];
	DIK_NameLookup[DIK_NUMPAD5] = KeyStrings[13];
	DIK_NameLookup[DIK_NUMPAD6] = KeyStrings[14];
	DIK_NameLookup[DIK_ADD] = KeyStrings[15];
	DIK_NameLookup[DIK_NUMPAD1] = KeyStrings[16];
	DIK_NameLookup[DIK_NUMPAD2] = KeyStrings[17];
	DIK_NameLookup[DIK_NUMPAD3] = KeyStrings[18];
	DIK_NameLookup[DIK_NUMPAD0] = KeyStrings[19];
	DIK_NameLookup[DIK_DECIMAL] = KeyStrings[20];
	DIK_NameLookup[DIK_NUMPADEQUALS] = KeyStrings[21];
	DIK_NameLookup[DIK_NUMPADENTER] = KeyStrings[22];
	DIK_NameLookup[DIK_RCONTROL] = KeyStrings[23];
	DIK_NameLookup[DIK_NUMPADCOMMA] = KeyStrings[24];
	DIK_NameLookup[DIK_DIVIDE] = KeyStrings[25];
	DIK_NameLookup[DIK_RMENU] = KeyStrings[26];
	DIK_NameLookup[DIK_UP] = KeyStrings[27];
	DIK_NameLookup[DIK_LEFT] = KeyStrings[28];
	DIK_NameLookup[DIK_RIGHT] = KeyStrings[29];
	DIK_NameLookup[DIK_DOWN] = KeyStrings[30];

	DIJ_ButtonLookup[32] = KeyStrings[27];
	DIJ_ButtonLookup[33] = KeyStrings[30];
	DIJ_ButtonLookup[34] = KeyStrings[32];

	if (gUserLanguage == 1)
	{
		// French keyboard (AZERTY)
		DIK_CharLookup[DIK_Q] = 'a';
		DIK_CharLookup[DIK_W] = 'z';
		DIK_CharLookup[DIK_A] = 'q';
		DIK_CharLookup[DIK_SEMICOLON] = 'm';
		DIK_CharLookup[DIK_Z] = 'w';

		DIK_ShiftLookup[DIK_Q] = 'A';
		DIK_ShiftLookup[DIK_W] = 'Z';
		DIK_ShiftLookup[DIK_A] = 'Q';
		DIK_ShiftLookup[DIK_SEMICOLON] = 'M';
		DIK_ShiftLookup[DIK_Z] = 'W';

		DIK_NameLookup[DIK_Q] = "A";
		DIK_NameLookup[DIK_W] = "Z";
		DIK_NameLookup[DIK_A] = "Q";
		DIK_NameLookup[DIK_SEMICOLON] = "M";
		DIK_NameLookup[DIK_Z] = "W";
	}
	else if (gUserLanguage == 2)
	{
		// German keyboard (QWERTZ)
		DIK_CharLookup[DIK_Y] = 'z';
		DIK_CharLookup[DIK_Z] = 'y';

		DIK_ShiftLookup[DIK_Y] = 'Z';
		DIK_ShiftLookup[DIK_Z] = 'Y';

		DIK_NameLookup[DIK_Y] = "Z";
		DIK_NameLookup[DIK_Z] = "Y";
	}
}

// [0x40f2e2] [D]
// Returns -3 for Escape, -2 for Return, -1 for Backspace, the character of a pressed key or 0.
int GetASCIIInput(void)
{
	char *keys;
	int i;

	keys = DIK_CharLookup;

	if (CheckKey(DIK_ESCAPE))
		return -3;

	if (CheckKey(DIK_RETURN))
		return -2;

	if (CheckKey(DIK_BACK))
		return -1;

	if (KeyMap[DIK_CAPITAL] || KeyMap[DIK_LSHIFT] || KeyMap[DIK_RSHIFT])
		keys = DIK_ShiftLookup;

	for (i = 0; i < 256; i++)
	{
		if (keys[i] && CheckKey(i))
			return (u_char)keys[i];
	}

	return 0;
}

// [0x40f3a9] [D]
// Returns the first pressed key that has a name, or -1.
int GetKeyInput(void)
{
	int i;

	for (i = 0; i < 256; i++)
	{
		if (DIK_NameLookup[i] && CheckKey(i))
			return i;
	}

	return -1;
}

// [0x40f3f5] [D]
char *GetKeyName(u_char key)
{
	return DIK_NameLookup[key];
}

// [0x40f409] [D]
// Returns the first pressed joystick button, 34 for Space (no button) or -1.
int GetJoyKeyInput(void)
{
	int i;

	for (i = 0; i < 34; i++)
	{
		if (IsJoyKeyPressed(i))
			return i;
	}

	if (CheckKey(DIK_SPACE))
		return 34;

	return -1;
}

// [0x40f458] [D]
char *GetJoyKeyName(u_char button)
{
	if (button > 34)
		return KeyStrings[31];

	return DIJ_ButtonLookup[button];
}
