/*
 * pres.c - text presentation, fonts and language strings
 *
 * Rebuilt from Driver_sym2_94.i64 (IDA folder "pres.c"); partially translated.
 */

#include <stdio.h>

#define MAX_LANGUAGE_TEXT	350		// NOTE: known constant

/* general.c, file.c */
extern void DebugFree(void *mem);
extern void *FileReadGetSize(char *filename, int *size);

extern int gUserLanguage;

void InitStringLanguage(char *filename, int mission);

// initialised data (0x577d1c)
char *GameLanguageNames[] = {
	"ENG",
	"FRE",
	"GER",
	"SPA",
	"ITA",
	"USA",
};

// NOTE: bss globals in the linker's common area; defined here next to their users
char *gMissionStringBuffer;
char *gLangStringBuffer;
char *gMissionStrings[MAX_LANGUAGE_TEXT];
char *gLanguageStrings[MAX_LANGUAGE_TEXT];

// [0x518f80] [D]
// NOTE: REDRIVER2 dr2locale.c has a port of this function (different file names)
void InitStringMng(void)
{
	char filename[256];

	if (gUserLanguage < 0 || gUserLanguage >= 6)
		gUserLanguage = 0;

	sprintf(filename, "LANG.%s", GameLanguageNames[gUserLanguage]);
	InitStringLanguage(filename, 0);

	sprintf(filename, "MISSION.%s", GameLanguageNames[gUserLanguage]);
	InitStringLanguage(filename, 1);
}

// [0x51900e] [D]
void DeinitStringMng(void)
{
	if (gLangStringBuffer)
	{
		DebugFree(gLangStringBuffer);
		gLangStringBuffer = NULL;
	}

	if (gMissionStringBuffer)
	{
		DebugFree(gMissionStringBuffer);
		gMissionStringBuffer = NULL;
	}
}

// [0x51981a] [D]
// Splits a loaded LANG / MISSION file into MAX_LANGUAGE_TEXT lines (in place).
void InitStringLanguage(char *filename, int mission)
{
	char *buffer;
	char *end;
	int i;
	int size;

	size = 0;

	if (mission)
	{
		gMissionStringBuffer = (char *)FileReadGetSize(filename, &size);
		buffer = gMissionStringBuffer;
	}
	else
	{
		gLangStringBuffer = (char *)FileReadGetSize(filename, &size);
		buffer = gLangStringBuffer;
	}

	for (i = 0; i < MAX_LANGUAGE_TEXT; i++)
	{
		if (size <= 0)
		{
			if (mission)
				gMissionStrings[i] = "Mission.? missing!";
			else
				gLanguageStrings[i] = "Lang.? missing!";
		}
		else
		{
			if (mission)
				gMissionStrings[i] = buffer;
			else
				gLanguageStrings[i] = buffer;

			// cut the line at the newline, dropping trailing control characters (CR)
			end = buffer;

			do
			{
				if (*buffer == '\n')
					break;

				if (*buffer >= ' ')
					end = buffer + 1;

				size--;
				buffer++;
			} while (size > 1);

			*end = 0;

			buffer++;
			size--;
		}
	}
}
