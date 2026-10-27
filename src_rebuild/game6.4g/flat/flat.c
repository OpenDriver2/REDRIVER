/*
 * flat/flat.c - motel room ("flat") main loop
 *
 * Rebuilt from Driver_sym2_94.i64 (IDA folder "motel\flat.c=flat\flat.c"); partially translated.
 */

#include <stddef.h>

#include "dr_types.h"
#include "dr_globals.h"
#include "flat/flat.h"
#include "flat/frontend.h"
#include "flat/prescam.h"
#include "Engine/3dfx/gfxinit.h"
#include "frontend/config.h"
#include "general.h"
#include "input.h"
#include "map.h"
#include "shell.h"

void FadeInOptionName(void);
void DoMotelCamera(int camera);
int ControlMotel(void);
void DrawIcons(void);	// NOTE: static in the original (flat._DrawIcons); not translated yet, stubbed

//------------------------------------------------------------------------------------------
// initialised data (0x550610)

static FE_TEXT textitems[2] = {
	{ 320, 430, 0, 1, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ -1, -1, 0, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
};

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
