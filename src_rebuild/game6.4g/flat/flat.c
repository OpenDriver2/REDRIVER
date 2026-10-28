/*
 * flat/flat.c - motel room ("flat") main loop
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "motel\flat.c=flat\flat.c", 0x418060 - 0x418614)
 */

#include <stddef.h>
#include <dinput.h>

#include "dr_types.h"
#include "dr_globals.h"
#include "flat/flat.h"
#include "flat/frontend.h"
#include "flat/prescam.h"
#include "flat/fudge.h"
#include "pres.h"
#include "Engine/3dfx/gfxinit.h"
#include "frontend/config.h"
#include "general.h"
#include "input.h"
#include "map.h"
#include "shell.h"
#include "flat/answerphone.h"
#include "flat/exit.h"
#include "flat/options.h"
#include "flat/savegame.h"

void FadeInOptionName(void);
void DoMotelCamera(int camera);
int ControlMotel(void);
int DoMotelSelect(void);
int DriveFunc(void);
void DoMotelMoveRight(void);
void DoMotelMoveLeft(void);
static void DrawIcons(void);

//------------------------------------------------------------------------------------------
// initialised data (0x550610)

static FE_TEXT textitems[2] = {
	{ 320, 430, 0, 1, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ -1, -1, 0, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
};

static int icon_flash_value = 50;
static int icon_flash_direction = 20;

//------------------------------------------------------------------------------------------
// uninitialised data (0x600ca8)
// NOTE: the slots right before (0x600ca4) and after (0x600cbc) are unmarked and unreferenced;
// their owner (exit.c / flat.c / frontend.c) is unknown, so they are not declared here

int queuecam;
TEX_INFO *LeftArrowTexture;
TEX_INFO *RightArrowTexture;
int (*queuefunc)(void);
MLADDER_ROOM *room_data;

// [0x418060] [D]
int DoFlat(MLADDER_ROOM *room)
{
	int ret;
	float old_screen_scale;

	LoadSavedOptions();

	room_data = room;
	motel_font_override = 0;

	old_screen_scale = screen_scale;
	screen_scale = 1.0f;

	ResetViewport(0, 0, 0);

	queuefunc = NULL;

	InitFlat();

	LeftArrowTexture = GetTextureDetails("LEFTARRO");
	RightArrowTexture = GetTextureDetails("RIGHTARR");

	gCameraOption = 4;
	PlaceCameraAt(9);

	gMotelFadeIntensity = 0.0f;
	SetMotelIntensity(1.0f);

	FadeInOptionName();
	DoMotelCamera(4);

	flat_retcode = -1;

	WaitForSync(0);

	for (;;)
	{
		HandleWinMessages();
		ReadInput();

		if (ControlMotel() == -1)
			break;

		if (queuefunc && !gCameraMoving)
		{
			ret = queuefunc();

			if (ret == 1)
				break;

			DoMotelCamera(queuecam);
			queuefunc = NULL;
		}

		DrawMotel(textitems, DrawIcons);
	}

	QuitFlat();

	screen_scale = old_screen_scale;
	return flat_retcode;
}

// [0x41819b] [D]
// Shows the name of the option the camera looks at, fading it in.
void FadeInOptionName(void)
{
	switch (gCameraOption)
	{
		case 0:
			textitems[0].text = gLanguageStrings[147];
			break;
		case 1:
			textitems[0].text = gLanguageStrings[143];
			break;
		case 2:
			textitems[0].text = gLanguageStrings[144];
			break;
		case 3:
			textitems[0].text = gLanguageStrings[145];
			break;
		case 4:
			textitems[0].text = gLanguageStrings[146];
			break;
	}

	SetTextColour(0, 0, 0);
	SetFade(255, 255, 128, 50);
}

// [0x418236] [D]
// Left / right turn to the next option, Return selects it; Escape (when the camera is still)
// asks to leave. -1 when the room is left.
int ControlMotel(void)
{
	if (CheckKey(DIK_ESCAPE) && gCameraMoving == 0 && queuefunc == NULL)
	{
		PlayMotelClickSound();
		motel_font_override = 0;

		if (DoMotelExit() == 1)
			return -1;
	}
	else if (CheckKey(DIK_LEFT))
	{
		queuefunc = NULL;
		PlayMotelClickSound();
		DoMotelMoveLeft();
	}
	else if (CheckKey(DIK_RIGHT))
	{
		queuefunc = NULL;
		PlayMotelClickSound();
		DoMotelMoveRight();
	}
	else if (CheckKey(DIK_RETURN))
	{
		PlayMotelClickSound();
		motel_font_override = 0;
		DoMotelSelect();
	}

	return 0;
}

// [0x4182ed] [D]
// Moves the camera close to the selected option; its menu runs (queuefunc) when it got there.
int DoMotelSelect(void)
{
	switch (gCameraOption)
	{
		case 0:
			SetFade(0, 0, 0, 50);
			DoMotelCamera(5);
			queuefunc = DoMotelSavegame;
			queuecam = 0;
			break;
		case 1:
			SetFade(0, 0, 0, 50);
			DoMotelCamera(6);
			queuefunc = DoMotelExit;
			queuecam = 1;
			break;
		case 2:
			SetFade(0, 0, 0, 50);
			SetMotelIntensity(0.0f);
			DoMotelCamera(7);
			queuefunc = DriveFunc;
			queuecam = 2;
			break;
		case 3:
			SetFade(0, 0, 0, 50);
			DoMotelCamera(8);
			queuefunc = DoMotelOptions;
			queuecam = 3;
			break;
		case 4:
			SetFade(0, 0, 0, 50);
			DoMotelCamera(9);
			queuefunc = DoMotelAnswerphone;
			queuecam = 4;
			break;
	}

	return 0;
}

// [0x418425] [D]
// The car: leave the room to play.
int DriveFunc(void)
{
	flat_retcode = -2;
	return 1;
}

// [0x418439] [D]
void DoMotelMoveRight(void)
{
	SetFade(0, 0, 0, 50);

	if (++gCameraOption > 4)
		gCameraOption = 0;

	DoMotelCamera(gCameraOption);
	FadeInOptionName();
	motel_font_override = 1;
}

// [0x41848c] [D]
void DoMotelMoveLeft(void)
{
	SetFade(0, 0, 0, 50);

	if (--gCameraOption < 0)
		gCameraOption = 4;

	DoMotelCamera(gCameraOption);
	FadeInOptionName();
	motel_font_override = 1;
}

// [0x4184df] [D]
// The pulsing left / right arrows (a subtractive shadow, then additive).
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

	if (LeftArrowTexture)
	{
		MyDrawIcon(32.0f, 422.0f, LeftArrowTexture, value, value, value, 0, 2);
		MyDrawIcon(30.0f, 420.0f, LeftArrowTexture, value, value, value, 0, 3);
	}

	if (RightArrowTexture)
	{
		MyDrawIcon(582.0f, 422.0f, RightArrowTexture, value, value, value, 0, 2);
		MyDrawIcon(580.0f, 420.0f, RightArrowTexture, value, value, value, 0, 3);
	}
}

// [0x4185f9] [D]
void DoMotelCamera(int camera)
{
	motel_font_override = 1;
	MoveCameraTo(camera);
}
