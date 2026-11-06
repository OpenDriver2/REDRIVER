/*
 * frontend/details.c - graphics detail options: detail level, effects, view distances,
 * pedestrian and traffic density, night brightness; also used in the motel room
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "frontend\details.c", 0x4089a0 - 0x40a023)
 */

#include "dr_types.h"
#include "dr_globals.h"
#include "pres.h"
#include "shell.h"
#include "input.h"
#include "asmmath.h"
#include "flat/frontend.h"
#include "frontend/frontend.h"
#include "frontend/config.h"
#include "frontend/details.h"
#include "flat/detailoptions.h"
#include "debris.h"
#include "denting.h"
#include "pedest.h"
#include "shadow.h"
#include "Engine/3dfx/gfxinit.h"
#include "glaunch.h"
#include "mirror.h"

void UpdateDetailOptions(void);
void SetupDetailOptions(void);
static void detaillevel_selfunc(int flag);
static void detaillevel_unselfunc(int flag);
static int detaillevel_choosefunc(void);
static void shadows_selfunc(int flag);
static void shadows_unselfunc(int flag);
static int shadows_choosefunc(void);
static void smoke_selfunc(int flag);
static void smoke_unselfunc(int flag);
static int smoke_choosefunc(void);
static void debris_selfunc(int flag);
static void debris_unselfunc(int flag);
static int debris_choosefunc(void);
static void rearviewmirror_selfunc(int flag);
static void rearviewmirror_unselfunc(int flag);
static int rearviewmirror_choosefunc(void);
static void tiretracks_selfunc(int flag);
static void tiretracks_unselfunc(int flag);
static int tiretracks_choosefunc(void);
static void coloredcars_selfunc(int flag);
static void coloredcars_unselfunc(int flag);
static int coloredcars_choosefunc(void);
static void shinycars_selfunc(int flag);
static void shinycars_unselfunc(int flag);
static int shinycars_choosefunc(void);
static void lensflare_selfunc(int flag);
static void lensflares_unselfunc(int flag);
static int lensflares_choosefunc(void);
static void lightflares_selfunc(int flag);
static void lightflares_unselfunc(int flag);
static int lightflares_choosefunc(void);
static void transparentwater_selfunc(int flag);
static void transparentwater_unselfunc(int flag);
static int transparentwater_choosefunc(void);
static void fog_selfunc(int flag);
static void fog_unselfunc(int flag);
static int fog_choosefunc(void);
static void overlaymap_selfunc(int flag);
static void overlaymap_unselfunc(int flag);
static int overlaymap_choosefunc(void);
static void realtimelights_selfunc(int flag);
static void realtimelights_unselfunc(int flag);
static int realtimelights_choosefunc(void);
static void viewdistance_selfunc(int flag);
static void viewdistance_unselfunc(int flag);
static int viewdistance_choosefunc(void);
static void mirrorviewdistance_selfunc(int flag);
static void mirrorviewdistance_unselfunc(int flag);
static int mirrorviewdistance_choosefunc(void);
static void pedestriandensity_selfunc(int flag);
static void pedestriandensity_unselfunc(int flag);
static int pedestriandensity_choosefunc(void);
static void trafficdensity_selfunc(int flag);
static void trafficdensity_unselfunc(int flag);
static int trafficdensity_choosefunc(void);
static void nightbrightness_selfunc(int flag);
static void nightbrightness_unselfunc(int flag);
static int nightbrightness_choosefunc(void);
static void viewdistance_next(int flag);
static void viewdistance_prev(int flag);
static void mirrorviewdistance_next(int flag);
static void mirrorviewdistance_prev(int flag);
static void pedestriandensity_next(int flag);
static void pedestriandensity_prev(int flag);
static void trafficdensity_next(int flag);
static void trafficdensity_prev(int flag);
static void nightbrightness_next(int flag);
static void nightbrightness_prev(int flag);
static void HighlightMotelItem(void);
static void UnhighlightMotelItem(void);
void DrawMotelDetailOptions(void);

//------------------------------------------------------------------------------------------
// uninitialised data (0x5d79fc)

static int selection;
// NOTE: dword_5D7A00; the (first) text of the item highlighted in the motel room
static int motel_item;
// NOTE: byte_5D7A04, a bss buffer: the text of the end entry of polyitems
static char poly_end_text[4];

//------------------------------------------------------------------------------------------
// initialised data (0x54c220)

// the sliders: mirror view distance, view distance, pedestrians, traffic, night brightness;
// SetupDetailOptions moves the boxes
static FE_POLY polyitems[11] = {
	{ 467, 311, "SLIDER", 1, 0, 0, { -1, -1, -1, -1 }, 0 },
	{ 469, 311, "SLIDEBOX", 1, 0, 0, { -1, -1, -1, -1 }, 0 },
	{ 467, 335, "SLIDER", 1, 0, 0, { -1, -1, -1, -1 }, 0 },
	{ 469, 335, "SLIDEBOX", 1, 0, 0, { -1, -1, -1, -1 }, 0 },
	{ 467, 359, "SLIDER", 1, 0, 0, { -1, -1, -1, -1 }, 0 },
	{ 469, 359, "SLIDEBOX", 1, 0, 0, { -1, -1, -1, -1 }, 0 },
	{ 467, 383, "SLIDER", 1, 0, 0, { -1, -1, -1, -1 }, 0 },
	{ 469, 383, "SLIDEBOX", 1, 0, 0, { -1, -1, -1, -1 }, 0 },
	{ 467, 407, "SLIDER", 1, 0, 0, { -1, -1, -1, -1 }, 0 },
	{ 469, 407, "SLIDEBOX", 1, 0, 0, { -1, -1, -1, -1 }, 0 },
	{ 0, 0, poly_end_text, 4, 0, 0, { 0, 0, 0, 0 }, 0 },
};

static FE_TEXT textitems[34] = {
	{ 45, 138, 3, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 295, 138, 2, 2, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 45, 162, 3, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 295, 162, 2, 2, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 45, 186, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 295, 186, 2, 2, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 45, 210, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 295, 210, 2, 2, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 45, 234, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 295, 234, 2, 2, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 45, 258, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 295, 258, 2, 2, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 45, 282, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 295, 282, 2, 2, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 330, 138, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 595, 138, 2, 2, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 330, 162, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 595, 162, 2, 2, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 330, 186, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 595, 186, 2, 2, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 330, 210, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 595, 210, 2, 2, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 330, 234, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 595, 234, 2, 2, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 330, 258, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 595, 258, 2, 2, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 330, 282, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 595, 282, 2, 2, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 45, 306, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 45, 330, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 45, 354, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 45, 378, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 45, 402, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ -1, -1, 0, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
};

// [19] - [28]: left / right on the sliders (HandleMenu calls their selfunc); the menu ends at
// the first entry without a choosefunc
static FE_BUTTON menufunc[29] = {
	{ detaillevel_selfunc, detaillevel_unselfunc, detaillevel_choosefunc, 7, 7, 18, 1 },
	{ shadows_selfunc, shadows_unselfunc, shadows_choosefunc, 8, 8, 0, 2 },
	{ smoke_selfunc, smoke_unselfunc, smoke_choosefunc, 9, 9, 1, 3 },
	{ debris_selfunc, debris_unselfunc, debris_choosefunc, 10, 10, 2, 4 },
	{ lensflare_selfunc, lensflares_unselfunc, lensflares_choosefunc, 11, 11, 3, 5 },
	{ lightflares_selfunc, lightflares_unselfunc, lightflares_choosefunc, 12, 12, 4, 6 },
	{ rearviewmirror_selfunc, rearviewmirror_unselfunc, rearviewmirror_choosefunc, 13, 13, 5, 14 },
	{ shinycars_selfunc, shinycars_unselfunc, shinycars_choosefunc, 0, 0, 13, 8 },
	{ coloredcars_selfunc, coloredcars_unselfunc, coloredcars_choosefunc, 1, 1, 7, 9 },
	{ tiretracks_selfunc, tiretracks_unselfunc, tiretracks_choosefunc, 2, 2, 8, 10 },
	{ realtimelights_selfunc, realtimelights_unselfunc, realtimelights_choosefunc, 3, 3, 9, 11 },
	{ overlaymap_selfunc, overlaymap_unselfunc, overlaymap_choosefunc, 4, 4, 10, 12 },
	{ fog_selfunc, fog_unselfunc, fog_choosefunc, 5, 5, 11, 13 },
	{ transparentwater_selfunc, transparentwater_unselfunc, transparentwater_choosefunc, 6, 6, 12, 7 },
	{ mirrorviewdistance_selfunc, mirrorviewdistance_unselfunc, mirrorviewdistance_choosefunc, 19, 20, 6, 15 },
	{ viewdistance_selfunc, viewdistance_unselfunc, viewdistance_choosefunc, 21, 22, 14, 16 },
	{ pedestriandensity_selfunc, pedestriandensity_unselfunc, pedestriandensity_choosefunc, 23, 24, 15, 17 },
	{ trafficdensity_selfunc, trafficdensity_unselfunc, trafficdensity_choosefunc, 25, 26, 16, 18 },
	{ nightbrightness_selfunc, nightbrightness_unselfunc, nightbrightness_choosefunc, 27, 28, 17, 0 },
	{ mirrorviewdistance_prev, NULL, NULL, 0, 0, 0, 0 },
	{ mirrorviewdistance_next, NULL, NULL, 0, 0, 0, 0 },
	{ viewdistance_prev, NULL, NULL, 0, 0, 0, 0 },
	{ viewdistance_next, NULL, NULL, 0, 0, 0, 0 },
	{ pedestriandensity_prev, NULL, NULL, 0, 0, 0, 0 },
	{ pedestriandensity_next, NULL, NULL, 0, 0, 0, 0 },
	{ trafficdensity_prev, NULL, NULL, 0, 0, 0, 0 },
	{ trafficdensity_next, NULL, NULL, 0, 0, 0, 0 },
	{ nightbrightness_prev, NULL, NULL, 0, 0, 0, 0 },
	{ nightbrightness_next, NULL, NULL, 0, 0, 0, 0 },
};

// gLanguageStrings indices of the option values
static int onoff[2] = { 2, 1 };
static int tiretracks_names[3] = { 2, 6, 7 };
static int realtimelights_names[3] = { 8, 7, 232 };
static int shadows_names[3] = { 2, 5, 7 };
static int detaillevel_names[5] = { 5, 10, 226, 232, 227 };

// the slider positions 0..1 (also drawn in the motel room)
static float peds_slider = 1.0f;
static float traffic_slider = 1.0f;
static float view_slider = 1.0f;
static float mirror_slider = 1.0f;
static float night_slider = 1.0f;

// NOTE: dword_54C744; the second text of the item highlighted in the motel room, -1: none
static int motel_item2 = -1;

// [0x4089a0] [D]
// In the motel room the menu is drawn over the room and leaving it returns 0.
int SelectDetailOptions(void)
{
	int ret;

	UpdateDetailOptions();
	SetupDetailOptions();

	selection = 0;
	motel_item = 0;
	motel_item2 = 1;

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
			DrawMotel(textitems, DrawMotelDetailOptions);
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

// [0x408a94] [D]
static void detaillevel_selfunc(int flag)
{
	if (flag == 0 || flag == 1)
	{
		textitems[0].font = 3;
		textitems[1].font = 3;
	}
	else if (flag == 2)
	{
		motel_item = 0;
		motel_item2 = 1;
	}
}

// [0x408ada] [D]
static void detaillevel_unselfunc(int flag)
{
	if (flag == 0)
	{
		textitems[0].font = 2;
		textitems[1].font = 2;
	}
	else if (flag == 2)
	{
		UnhighlightMotelItem();
	}
}

// [0x408b0b] [D]
static int detaillevel_choosefunc(void)
{
	if (++gDetailLevel > 3)
		gDetailLevel = 0;

	SetGraphicsLevel(gDetailLevel, 0);

	UpdateDetailOptions();
	SetupDetailOptions();
	return 0;
}

// [0x408b4d] [D]
static void shadows_selfunc(int flag)
{
	if (flag == 0 || flag == 1)
	{
		textitems[2].font = 3;
		textitems[3].font = 3;
	}
	else if (flag == 2)
	{
		motel_item = 2;
		motel_item2 = 3;
	}
}

// [0x408b93] [D]
static void shadows_unselfunc(int flag)
{
	if (flag == 0)
	{
		textitems[2].font = 2;
		textitems[3].font = 2;
	}
	else if (flag == 2)
	{
		UnhighlightMotelItem();
	}
}

// [0x408bc4] [D]
static int shadows_choosefunc(void)
{
	if (gDrawShadows)
	{
		if (gDoDetailedShadows)
		{
			gDrawShadows = 0;
			gDoDetailedShadows = 0;
		}
		else
		{
			gDoDetailedShadows = 1;
		}
	}
	else
	{
		gDrawShadows = 1;
		gDoDetailedShadows = 0;
	}

	gDetailLevel = 4;
	SetupDetailOptions();
	return 0;
}

// [0x408c22] [D]
static void smoke_selfunc(int flag)
{
	if (flag == 0 || flag == 1)
	{
		textitems[4].font = 3;
		textitems[5].font = 3;
	}
	else if (flag == 2)
	{
		motel_item = 4;
		motel_item2 = 5;
	}
}

// [0x408c68] [D]
static void smoke_unselfunc(int flag)
{
	if (flag == 0)
	{
		textitems[4].font = 2;
		textitems[5].font = 2;
	}
	else if (flag == 2)
	{
		UnhighlightMotelItem();
	}
}

// [0x408c99] [D]
static int smoke_choosefunc(void)
{
	gDoSmoke ^= 1;

	gDetailLevel = 4;
	SetupDetailOptions();
	return 0;
}

// [0x408cbc] [D]
static void debris_selfunc(int flag)
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

// [0x408d02] [D]
static void debris_unselfunc(int flag)
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

// [0x408d33] [D]
static int debris_choosefunc(void)
{
	gDoDebris ^= 1;

	gDetailLevel = 4;
	SetupDetailOptions();
	return 0;
}

// [0x408d56] [D]
static void rearviewmirror_selfunc(int flag)
{
	if (flag == 0 || flag == 1)
	{
		textitems[12].font = 3;
		textitems[13].font = 3;
	}
	else if (flag == 2)
	{
		motel_item = 12;
		motel_item2 = 13;
	}
}

// [0x408d9c] [D]
static void rearviewmirror_unselfunc(int flag)
{
	if (flag == 0)
	{
		textitems[12].font = 2;
		textitems[13].font = 2;
	}
	else if (flag == 2)
	{
		UnhighlightMotelItem();
	}
}

// [0x408dcd] [D]
static int rearviewmirror_choosefunc(void)
{
	gDoRearViewMirror ^= 1;

	gDetailLevel = 4;
	SetupDetailOptions();
	return 0;
}

// [0x408df0] [D]
static void tiretracks_selfunc(int flag)
{
	if (flag == 0 || flag == 1)
	{
		textitems[18].font = 3;
		textitems[19].font = 3;
	}
	else if (flag == 2)
	{
		motel_item = 18;
		motel_item2 = 19;
	}
}

// [0x408e36] [D]
static void tiretracks_unselfunc(int flag)
{
	if (flag == 0)
	{
		textitems[18].font = 2;
		textitems[19].font = 2;
	}
	else if (flag == 2)
	{
		UnhighlightMotelItem();
	}
}

// [0x408e67] [D]
static int tiretracks_choosefunc(void)
{
	if (gDoTyreTracks)
	{
		if (gDoTexturedTyreTracks)
		{
			gDoTyreTracks = 0;
			gDoTexturedTyreTracks = 0;
		}
		else
		{
			gDoTexturedTyreTracks = 1;
		}
	}
	else
	{
		gDoTyreTracks = 1;
		gDoTexturedTyreTracks = 0;
	}

	gDetailLevel = 4;
	SetupDetailOptions();
	return 0;
}

// [0x408ec5] [D]
static void coloredcars_selfunc(int flag)
{
	if (flag == 0 || flag == 1)
	{
		textitems[16].font = 3;
		textitems[17].font = 3;
	}
	else if (flag == 2)
	{
		motel_item = 16;
		motel_item2 = 17;
	}
}

// [0x408f0b] [D]
static void coloredcars_unselfunc(int flag)
{
	if (flag == 0)
	{
		textitems[16].font = 2;
		textitems[17].font = 2;
	}
	else if (flag == 2)
	{
		UnhighlightMotelItem();
	}
}

// [0x408f3c] [D]
static int coloredcars_choosefunc(void)
{
	gDoMultiColouredCars ^= 1;

	gDetailLevel = 4;
	SetupDetailOptions();
	return 0;
}

// [0x408f5f] [D]
static void shinycars_selfunc(int flag)
{
	if (flag == 0 || flag == 1)
	{
		textitems[14].font = 3;
		textitems[15].font = 3;
	}
	else if (flag == 2)
	{
		motel_item = 14;
		motel_item2 = 15;
	}
}

// [0x408fa5] [D]
static void shinycars_unselfunc(int flag)
{
	if (flag == 0)
	{
		textitems[14].font = 2;
		textitems[15].font = 2;
	}
	else if (flag == 2)
	{
		UnhighlightMotelItem();
	}
}

// [0x408fd6] [D]
static int shinycars_choosefunc(void)
{
	gDoShinyCars ^= 1;

	gDetailLevel = 4;
	SetupDetailOptions();
	return 0;
}

// [0x408ff9] [D]
static void lensflare_selfunc(int flag)
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

// [0x40903f] [D]
static void lensflares_unselfunc(int flag)
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

// [0x409070] [D]
static int lensflares_choosefunc(void)
{
	gDoLensFlare ^= 1;

	gDetailLevel = 4;
	SetupDetailOptions();
	return 0;
}

// [0x409093] [D]
static void lightflares_selfunc(int flag)
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

// [0x4090d9] [D]
static void lightflares_unselfunc(int flag)
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

// [0x40910a] [D]
static int lightflares_choosefunc(void)
{
	gDoLightFlare ^= 1;

	gDetailLevel = 4;
	SetupDetailOptions();
	return 0;
}

// [0x40912d] [D]
static void transparentwater_selfunc(int flag)
{
	if (flag == 0 || flag == 1)
	{
		textitems[26].font = 3;
		textitems[27].font = 3;
	}
	else if (flag == 2)
	{
		motel_item = 26;
		motel_item2 = 27;
	}
}

// [0x409173] [D]
static void transparentwater_unselfunc(int flag)
{
	if (flag == 0)
	{
		textitems[26].font = 2;
		textitems[27].font = 2;
	}
	else if (flag == 2)
	{
		UnhighlightMotelItem();
	}
}

// [0x4091a4] [D]
static int transparentwater_choosefunc(void)
{
	DrawSubmergedObjects ^= 1;

	gDetailLevel = 4;
	SetupDetailOptions();
	return 0;
}

// [0x4091c7] [D]
static void fog_selfunc(int flag)
{
	if (flag == 0 || flag == 1)
	{
		textitems[24].font = 3;
		textitems[25].font = 3;
	}
	else if (flag == 2)
	{
		motel_item = 24;
		motel_item2 = 25;
	}
}

// [0x40920d] [D]
static void fog_unselfunc(int flag)
{
	if (flag == 0)
	{
		textitems[24].font = 2;
		textitems[25].font = 2;
	}
	else if (flag == 2)
	{
		UnhighlightMotelItem();
	}
}

// [0x40923e] [D]
static int fog_choosefunc(void)
{
	gFogAllowed ^= 1;

	gDetailLevel = 4;
	SetupDetailOptions();
	return 0;
}

// [0x409261] [D]
static void overlaymap_selfunc(int flag)
{
	if (flag == 0 || flag == 1)
	{
		textitems[22].font = 3;
		textitems[23].font = 3;
	}
	else if (flag == 2)
	{
		motel_item = 22;
		motel_item2 = 23;
	}
}

// [0x4092a7] [D]
static void overlaymap_unselfunc(int flag)
{
	if (flag == 0)
	{
		textitems[22].font = 2;
		textitems[23].font = 2;
	}
	else if (flag == 2)
	{
		UnhighlightMotelItem();
	}
}

// [0x4092d8] [D]
static int overlaymap_choosefunc(void)
{
	gDrawMap ^= 1;

	gDetailLevel = 4;
	SetupDetailOptions();
	return 0;
}

// [0x4092fb] [D]
static void realtimelights_selfunc(int flag)
{
	if (flag == 0 || flag == 1)
	{
		textitems[20].font = 3;
		textitems[21].font = 3;
	}
	else if (flag == 2)
	{
		motel_item = 20;
		motel_item2 = 21;
	}
}

// [0x409341] [D]
static void realtimelights_unselfunc(int flag)
{
	if (flag == 0)
	{
		textitems[20].font = 2;
		textitems[21].font = 2;
	}
	else if (flag == 2)
	{
		UnhighlightMotelItem();
	}
}

// [0x409372] [D]
static int realtimelights_choosefunc(void)
{
	if (gRealTimeLights)
	{
		if (gUltraLamps)
		{
			gRealTimeLights = 0;
			gUltraLamps = 0;
		}
		else
		{
			gUltraLamps = 1;
		}
	}
	else
	{
		gRealTimeLights = 1;
		gUltraLamps = 0;
	}

	gDetailLevel = 4;
	SetupDetailOptions();
	return 0;
}

// [0x4093d0] [D]
static void viewdistance_selfunc(int flag)
{
	if (flag == 0 || flag == 1)
	{
		textitems[29].font = 3;
		on_slider = 1;
	}
	else if (flag == 2)
	{
		motel_item = 29;
		motel_item2 = -1;
		on_slider = 1;
	}
}

// [0x409423] [D]
static void viewdistance_unselfunc(int flag)
{
	if (flag == 0)
	{
		textitems[29].font = 2;
		on_slider = 0;
	}
	else if (flag == 2)
	{
		UnhighlightMotelItem();
		on_slider = 0;
	}
}

// [0x409461] [D]
static int viewdistance_choosefunc(void)
{
	gDetailLevel = 4;
	SetupDetailOptions();
	return 0;
}

// [0x409477] [D]
static void mirrorviewdistance_selfunc(int flag)
{
	if (flag == 0 || flag == 1)
	{
		textitems[28].font = 3;
		on_slider = 1;
	}
	else if (flag == 2)
	{
		motel_item = 28;
		motel_item2 = -1;
		on_slider = 1;
	}
}

// [0x4094ca] [D]
static void mirrorviewdistance_unselfunc(int flag)
{
	if (flag == 0)
	{
		textitems[28].font = 2;
		on_slider = 0;
	}
	else if (flag == 2)
	{
		UnhighlightMotelItem();
		on_slider = 0;
	}
}

// [0x409508] [D]
static int mirrorviewdistance_choosefunc(void)
{
	gDetailLevel = 4;
	SetupDetailOptions();
	return 0;
}

// [0x40951e] [D]
static void pedestriandensity_selfunc(int flag)
{
	if (flag == 0 || flag == 1)
	{
		textitems[30].font = 3;
		on_slider = 1;
	}
	else if (flag == 2)
	{
		motel_item = 30;
		motel_item2 = -1;
		on_slider = 1;
	}
}

// [0x409571] [D]
static void pedestriandensity_unselfunc(int flag)
{
	if (flag == 0)
	{
		textitems[30].font = 2;
		on_slider = 0;
	}
	else if (flag == 2)
	{
		UnhighlightMotelItem();
		on_slider = 0;
	}
}

// [0x4095af] [D]
static int pedestriandensity_choosefunc(void)
{
	gDetailLevel = 4;
	SetupDetailOptions();
	return 0;
}

// [0x4095c5] [D]
static void trafficdensity_selfunc(int flag)
{
	if (flag == 0 || flag == 1)
	{
		textitems[31].font = 3;
		on_slider = 1;
	}
	else if (flag == 2)
	{
		motel_item = 31;
		motel_item2 = -1;
		on_slider = 1;
	}
}

// [0x409618] [D]
static void trafficdensity_unselfunc(int flag)
{
	if (flag == 0)
	{
		textitems[31].font = 2;
		on_slider = 0;
	}
	else if (flag == 2)
	{
		UnhighlightMotelItem();
		on_slider = 0;
	}
}

// [0x409656] [D]
static int trafficdensity_choosefunc(void)
{
	gDetailLevel = 4;
	SetupDetailOptions();
	return 0;
}

// [0x40966c] [D]
static void nightbrightness_selfunc(int flag)
{
	if (flag == 0 || flag == 1)
	{
		textitems[32].font = 3;
		on_slider = 1;
	}
	else if (flag == 2)
	{
		motel_item = 32;
		motel_item2 = -1;
		on_slider = 1;
	}
}

// [0x4096bf] [D]
static void nightbrightness_unselfunc(int flag)
{
	if (flag == 0)
	{
		textitems[32].font = 2;
		on_slider = 0;
	}
	else if (flag == 2)
	{
		UnhighlightMotelItem();
		on_slider = 0;
	}
}

// [0x4096fd] [D]
static int nightbrightness_choosefunc(void)
{
	gDetailLevel = 4;
	SetupDetailOptions();
	return 0;
}

// [0x409713] [D]
// Clamps the slider options to their ranges.
void UpdateDetailOptions(void)
{
	int peds;
	float clip;

	peds = max_pedestrians <= 64 ? max_pedestrians : 64;
	max_pedestrians = peds >= 0 ? peds : 0;

	clip = far_clip <= 45000.0f ? far_clip : 45000.0f;
	far_clip = clip >= 15000.0f ? clip : 15000.0f;

	clip = mirror_far_clip <= 42000.0f ? mirror_far_clip : 42000.0f;
	mirror_far_clip = clip >= 15000.0f ? clip : 15000.0f;
}

// [0x409852] [D]
// Sets the texts and slider boxes from the options.
void SetupDetailOptions(void)
{
	int lights;
	int tracks;
	int shadows;

	if (gRealTimeLights)
		lights = gUltraLamps ? 2 : 1;
	else
		lights = 0;

	if (gDoTyreTracks)
		tracks = gDoTexturedTyreTracks ? 2 : 1;
	else
		tracks = 0;

	if (gDrawShadows)
		shadows = gDoDetailedShadows ? 2 : 1;
	else
		shadows = 0;

	textitems[0].text = gLanguageStrings[228];		// Detail Level
	textitems[2].text = gLanguageStrings[43];		// Shadows
	textitems[4].text = gLanguageStrings[44];		// Smoke
	textitems[6].text = gLanguageStrings[45];		// Debris
	textitems[8].text = gLanguageStrings[49];		// Lens Flare
	textitems[10].text = gLanguageStrings[229];		// Light Flares
	textitems[12].text = gLanguageStrings[46];		// Rear View Mirror
	textitems[14].text = gLanguageStrings[48];		// Shiny Cars
	textitems[16].text = gLanguageStrings[57];		// Colored cars
	textitems[18].text = gLanguageStrings[47];		// Tire Tracks
	textitems[20].text = gLanguageStrings[52];		// Real Time Lights
	textitems[22].text = gLanguageStrings[51];		// Overlay Map
	textitems[24].text = gLanguageStrings[55];		// Fog
	textitems[26].text = gLanguageStrings[50];		// Transparent Water
	textitems[28].text = gLanguageStrings[56];		// Mirror Viewing Distance
	textitems[29].text = gLanguageStrings[54];		// Viewing Distance
	textitems[30].text = gLanguageStrings[53];		// Pedestrian Density
	textitems[31].text = gLanguageStrings[247];		// Traffic Density
	textitems[32].text = gLanguageStrings[231];		// Night Brightness

	textitems[1].text = gLanguageStrings[detaillevel_names[gDetailLevel]];
	textitems[3].text = gLanguageStrings[shadows_names[shadows]];
	textitems[5].text = gLanguageStrings[onoff[gDoSmoke]];
	textitems[7].text = gLanguageStrings[onoff[gDoDebris]];
	textitems[9].text = gLanguageStrings[onoff[gDoLensFlare]];
	textitems[11].text = gLanguageStrings[onoff[gDoLightFlare]];
	textitems[13].text = gLanguageStrings[onoff[gDoRearViewMirror]];
	textitems[15].text = gLanguageStrings[onoff[gDoShinyCars]];
	textitems[17].text = gLanguageStrings[onoff[gDoMultiColouredCars]];
	textitems[19].text = gLanguageStrings[tiretracks_names[tracks]];
	textitems[21].text = gLanguageStrings[realtimelights_names[lights]];
	textitems[23].text = gLanguageStrings[onoff[gDrawMap]];
	textitems[25].text = gLanguageStrings[onoff[gFogAllowed]];
	textitems[27].text = gLanguageStrings[onoff[DrawSubmergedObjects]];

	mirror_slider = mirror_far_clip - 15000.0f;
	mirror_slider /= 27000.0f;
	polyitems[1].x = float_to_int(mirror_slider * 116.0f) + 469;

	view_slider = far_clip - 15000.0f;
	view_slider /= 30000.0f;
	polyitems[3].x = float_to_int(view_slider * 116.0f) + 469;

	peds_slider = (float)max_pedestrians / 64.0f;
	polyitems[5].x = float_to_int(peds_slider * 116.0f) + 469;

	traffic_slider = (float)(gTrafficDensity - 16);
	traffic_slider /= 48.0f;
	polyitems[7].x = float_to_int(traffic_slider * 116.0f) + 469;

	night_slider = (float)gNightBrightness / 3.0f;
	polyitems[9].x = float_to_int(night_slider * 116.0f) + 469;
}

// [0x409cc1] [D]
static void viewdistance_next(int flag)
{
	if (far_clip < 45000.0f)
	{
		far_clip += 3000.0f;
		gDetailLevel = 4;
		SetupDetailOptions();
	}
}

// [0x409cfa] [D]
static void viewdistance_prev(int flag)
{
	if (far_clip > 15000.0f)
	{
		far_clip -= 3000.0f;
		gDetailLevel = 4;
		SetupDetailOptions();
	}
}

// [0x409d33] [D]
static void mirrorviewdistance_next(int flag)
{
	if (mirror_far_clip < 42000.0f)
	{
		mirror_far_clip += 3000.0f;
		gDetailLevel = 4;
		SetupDetailOptions();
	}
}

// [0x409d6c] [D]
static void mirrorviewdistance_prev(int flag)
{
	if (mirror_far_clip > 15000.0f)
	{
		mirror_far_clip -= 3000.0f;
		gDetailLevel = 4;
		SetupDetailOptions();
	}
}

// [0x409da5] [D]
static void pedestriandensity_next(int flag)
{
	if (max_pedestrians < 64)
	{
		max_pedestrians++;
		gDetailLevel = 4;
		SetupDetailOptions();
	}
}

// [0x409dcf] [D]
static void pedestriandensity_prev(int flag)
{
	if (max_pedestrians > 0)
	{
		max_pedestrians--;
		gDetailLevel = 4;
		SetupDetailOptions();
	}
}

// [0x409df9] [D]
static void trafficdensity_next(int flag)
{
	gTrafficDensity += 16;

	if (gTrafficDensity > 64)
		gTrafficDensity = 64;

	gDetailLevel = 4;
	SetupDetailOptions();
}

// [0x409e2d] [D]
static void trafficdensity_prev(int flag)
{
	gTrafficDensity -= 16;

	if (gTrafficDensity < 16)
		gTrafficDensity = 16;

	gDetailLevel = 4;
	SetupDetailOptions();
}

// [0x409e61] [D]
static void nightbrightness_next(int flag)
{
	if (gNightBrightness < 3)
	{
		gNightBrightness++;
		gDetailLevel = 4;
		SetupDetailOptions();
	}
}

// [0x409e8b] [D]
static void nightbrightness_prev(int flag)
{
	if (gNightBrightness > 0)
	{
		gNightBrightness--;
		gDetailLevel = 4;
		SetupDetailOptions();
	}
}

// [0x409eb5] [D]
// NOTE: sub_409EB5, the name is a guess
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

// [0x409f2a] [D]
// NOTE: sub_409F2A, the name is a guess
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

// [0x409f96] [D]
// The sliders in the motel room; selected: the one highlighted (0 - 4), -1: none.
void DrawMotelDetailOptions(void)
{
	int selected;

	selected = -1;

	if (motel_item == 28)
		selected = 0;

	if (motel_item == 29)
		selected = 1;

	if (motel_item == 30)
		selected = 2;

	if (motel_item == 31)
		selected = 3;

	if (motel_item == 32)
		selected = 4;

	DrawMotelDetailOptionsIcons(peds_slider, view_slider, mirror_slider, traffic_slider, night_slider, selected);
}
