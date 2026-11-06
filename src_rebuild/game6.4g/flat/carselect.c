/*
 * flat/carselect.c - motel room car select: the camera looks at the cars of a car park
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "motel\carselect.c=flat\carselect.c", 0x4176d0 - 0x417a22)
 */

#include <stdio.h>
#include "dr_keys.h"

#include "dr_types.h"
#include "dr_globals.h"
#include "pres.h"
#include "shell.h"
#include "input.h"
#include "map.h"
#include "camera.h"
#include "flat/frontend.h"
#include "flat/fudge.h"
#include "flat/prescam.h"
#include "flat/carselect.h"

void SetupMotelCarSelect(void);
int CheckMotelCarSelectKeys(int carpark);
static void DrawIcons(void);

//------------------------------------------------------------------------------------------
// initialised data (0x550440)

static FE_TEXT textitems[2] = {
	{ 320, 430, 0, 1, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ -1, -1, 0, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
};

// each view of a car park: camera, car (gMotelCarLookup column), view on left, view on right
char carpark1_campaths[2][4] = { { 10, 1, 1, 1 }, { 11, 0, 0, 0 } };
char carpark2_campaths[2][4] = { { 12, 1, 1, 1 }, { 13, 0, 0, 0 } };
char carpark3_campaths[3][4] = { { 14, 1, 1, 2 }, { 15, 2, 2, 0 }, { 16, 0, 0, 1 } };

// the cars (gStoredPlayerCarType) of each car park
int gMotelCarLookup[3][3] = {
	{ 8, 12, -1 },
	{ 3, 5, -1 },
	{ 8, 9, 12 },
};

static int icon_flash_value = 50;
static int icon_flash_direction = 20;

//------------------------------------------------------------------------------------------
// uninitialised data (0x600c7c)

TEX_INFO *LeftArrowTextureCarSelect;
TEX_INFO *RightArrowTextureCarSelect;
char (*carpark_data)[4];
int cur_camera;

// [0x4176d0] [D]
// Loads the car park's camera views, lets the player look at the cars and choose one, then
// loads the room's views back.
int DoMotelCarSelect(int carpark)
{
	char filename[32];
	int ret;

	SetupMotelCarSelect();

	gDoingCarSelect = 1;

	sprintf(filename, "SCRIPTS\\FLYBY\\CARSEL%d.FBD", carpark);

	if (LoadFlyByData(filename) == 0)
		DisplayErrorAndExit("Files missing, please reinstall");

	switch (carpark)
	{
		case 1:
			carpark_data = carpark2_campaths;
			break;
		case 2:
			carpark_data = carpark3_campaths;
			break;
		default:
			carpark = 0;
			// fall through
		case 0:
			carpark_data = carpark1_campaths;
			break;
	}

	cur_camera = 0;

	PlaceCameraAt((u_char)carpark_data[0][0]);
	MoveCameraTo((u_char)carpark_data[0][0]);

	UpdateMotelRegion();

	FadeInMotel(textitems, DrawIcons);

	for (;;)
	{
		HandleWinMessages();
		ReadInput();

		if (gCameraMoving == 0)
		{
			ret = CheckMotelCarSelectKeys(carpark);

			if (ret != 0)
				break;
		}

		DrawMotel(textitems, DrawIcons);
	}

	sprintf(filename, "SCRIPTS\\FLYBY\\FLAT%d.FBD", GameLevel);

	if (LoadFlyByData(filename) == 0)
		DisplayErrorAndExit("Files missing, please reinstall");

	FadeOutMotel(textitems, DrawIcons);

	gDoingCarSelect = 0;

	return ret;
}

// [0x417833] [D]
void SetupMotelCarSelect(void)
{
	LeftArrowTextureCarSelect = GetTextureDetails("LEFTARRO");
	RightArrowTextureCarSelect = GetTextureDetails("RIGHTARR");

	textitems[0].text = gLanguageStrings[149];
}

// [0x417866] [D]
// Left / right look at the next car; Return takes it. -1 when done.
int CheckMotelCarSelectKeys(int carpark)
{
	if (CheckKey(DIK_ESCAPE))
	{
		PlayMotelClickSound();
		return -1;
	}

	if (CheckKey(DIK_LEFT))
	{
		PlayMotelClickSound();
		cur_camera = (u_char)carpark_data[cur_camera][2];
		MoveCameraTo((u_char)carpark_data[cur_camera][0]);
	}
	else if (CheckKey(DIK_RIGHT))
	{
		PlayMotelClickSound();
		cur_camera = (u_char)carpark_data[cur_camera][3];
		MoveCameraTo((u_char)carpark_data[cur_camera][0]);
	}
	else if (CheckKey(DIK_RETURN))
	{
		PlayMotelClickSound();
		gStoredPlayerCarType = gMotelCarLookup[carpark][(u_char)carpark_data[cur_camera][1]];
		return -1;
	}

	return 0;
}

// [0x417959] [D]
// The pulsing left / right arrows.
static void DrawIcons(void)
{
	u_char value;

	icon_flash_value += icon_flash_direction;

	if (icon_flash_value > 255)
	{
		icon_flash_value = 255;
		icon_flash_direction = -icon_flash_direction;
	}

	if (icon_flash_value < 0)
	{
		icon_flash_value = 0;
		icon_flash_direction = -icon_flash_direction;
	}

	value = icon_flash_value;

	if (LeftArrowTextureCarSelect)
		MyDrawIcon(30.0f, 420.0f, LeftArrowTextureCarSelect, value, value, value, 0, 3);

	if (RightArrowTextureCarSelect)
		MyDrawIcon(580.0f, 420.0f, RightArrowTextureCarSelect, value, value, value, 0, 3);
}
