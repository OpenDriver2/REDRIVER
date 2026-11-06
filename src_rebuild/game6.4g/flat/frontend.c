/*
 * flat/frontend.c - motel room front end: setup, fades, frame drawing, menu keys and texts
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "motel\frontend.c=flat\frontend.c", 0x418790 - 0x4190e9)
 */

#include "dr_keys.h"

#include "dr_types.h"
#include "dr_globals.h"
#include "shell.h"
#include "input.h"
#include "sound.h"
#include "general.h"
#include "map.h"
#include "pres.h"
#include "glaunch.h"
#include "eax/cdaudio.h"
#include "Engine/3dfx/gfxinit.h"
#include "../D3DGlide/glide_ptrs.h"
#include "frontend/font.h"
#include "frontend/frontend.h"
#include "flat/fudge.h"
#include "flat/prescam.h"
#include "flat/frontend.h"
#include "Engine/3dfx/object.h"
#include "handling.h"

void UpdateMotel(void);
void StartOfMotelFrame(void);
void DrawMotelMenu(FE_TEXT *texts);
void ControlMotelFade(void);
void PlayMotelClickSound(void);

//------------------------------------------------------------------------------------------
// initialised data (0x55065c)

int flat_retcode = -1;
float motel_xscale = 1.0f;		// 640 x 480 -> display size
float motel_yscale = 1.0f;
static int unknown_550668 = 1;	// NOTE: no references

//------------------------------------------------------------------------------------------
// uninitialised data (0x600cc0)

int interp_delay;				// frames to skip before the next one is drawn
int motel_font_override;		// draw the menu texts in the "moving camera" style
float gMotelFadeIntensity;
float gMotelFadeToIntensity;
int gMotelFading;
int gDoingCarSelect;

// [0x418790] [D]
void InitFlat(void)
{
	gDoMotelRoom = 1;
	gInPresentation = 1;

	motel_xscale = (float)display_width / 640.0f;
	motel_yscale = (float)display_height / 480.0f;

	InitMotelCameras(GameLevel);
	ResetCamera();
	FlatInit();

	ReleaseSounds();
	LoadSounds("MOTEL.DSS", 0);
	SetSoundSystemEnvironment(5);

	InitFontDrawing();
	SetTextColour(0, 0, 0);

	DIST = 384.0f;

	grTexFilterMode(GR_TMU0, GR_TEXTUREFILTER_BILINEAR, GR_TEXTUREFILTER_BILINEAR);
	grDepthBufferMode(GR_DEPTHBUFFER_WBUFFER);
	EnableDepthMask(1);
	grDepthBufferFunction(GR_CMP_LEQUAL);

	if (gUseCDAudio)
		PlayCDTrack(3);

	WaitForSync(0);
	interp_delay = 1;
}

// [0x41887e] [D]
void QuitFlat(void)
{
	if (gUseCDAudio)
		StopCDTrack();

	ReleaseSounds();
	FreeMotelCameras();
	GameExit();

	gDoMotelRoom = 0;
	gInPresentation = 0;

	ClearFullDisplay(4);
}

// [0x4188be] [D]
void UpdateMotelRegion(void)
{
	FollowCameraPath();
	InitMap();
}

// [0x4188cd] [D]
void FadeInMotel(FE_TEXT *texts, void (*drawicons)(void))
{
	SetMotelIntensity(1.0f);

	while (gMotelFading)
	{
		HandleWinMessages();
		ReadInput();
		ControlMotelFade();
		DrawMotel(texts, drawicons);
	}
}

// [0x418909] [D]
void FadeOutMotel(FE_TEXT *texts, void (*drawicons)(void))
{
	SetMotelIntensity(0.0f);

	while (gMotelFading)
	{
		HandleWinMessages();
		ReadInput();
		ControlMotelFade();
		DrawMotel(texts, drawicons);
	}
}

// [0x418942] [D]
// Draws the room, then the menu texts and the icons over it without depth testing.
void DrawMotel(FE_TEXT *texts, void (*drawicons)(void))
{
	while (gScreen.active == 0)
		HandleWinMessages();

	UpdateMotel();

	if (interp_delay-- == 0)
	{
		StartOfFrame();
		StartOfMotelFrame();

		MotelDisableZ();

		if (texts)
			DrawMotelMenu(texts);

		if (drawicons)
			drawicons();

		MotelEnableZ();

		EndOfFrame();
		interp_delay = WaitForSync(0);
	}
}

// [0x4189b9] [D]
void SetMotelIntensity(float intensity)
{
	gMotelFadeToIntensity = intensity;
	gMotelFading = 1;
}

// [0x4189d0] [D]
// Moves the light level 1/60 per frame towards gMotelFadeToIntensity.
void ControlMotelFade(void)
{
	if (gMotelFading == 0)
		return;

	if (gMotelFadeIntensity < gMotelFadeToIntensity)
	{
		gMotelFadeIntensity += 1.0f / 60.0f;

		if (gMotelFadeIntensity >= gMotelFadeToIntensity)
		{
			gMotelFadeIntensity = gMotelFadeToIntensity;
			gMotelFading = 0;
		}
	}
	else if (gMotelFadeIntensity > gMotelFadeToIntensity)
	{
		gMotelFadeIntensity -= 1.0f / 60.0f;

		if (gMotelFadeIntensity <= gMotelFadeToIntensity)
		{
			gMotelFadeIntensity = gMotelFadeToIntensity;
			gMotelFading = 0;
		}
	}

	ambient_light_level = gMotelFadeIntensity;
	light_source.vx = gMotelFadeIntensity;
	light_source.vy = gMotelFadeIntensity;
	light_source.vz = gMotelFadeIntensity;
}

// [0x418aad] [D]
void ResetMotelMenu(FE_BUTTON *buttons, int selection, FE_TEXT *texts)
{
	int i;

	for (i = 0; texts[i].x != -1; i++)
	{
		texts[i].color.r = 255;
		texts[i].color.g = 255;
		texts[i].color.b = 255;
	}

	for (i = 0; buttons[i].choosefunc; i++)
		buttons[i].unselfunc(2);

	buttons[selection].selfunc(2);
}

// [0x418b50] [D]
// HandleMenu for the motel room: the items are highlighted with selfunc(2) / unselfunc(2), on a
// slider holding left / right repeats. Returns the new selection, -1 on Escape.
int MotelMenuKeyControl(FE_BUTTON *buttons, int selection)
{
	static int delay;
	static int delval = 10;
	int old;

	old = selection;

	if (CheckKey(DIK_ESCAPE))
	{
		PlayMotelClickSound();
		buttons[selection].unselfunc(2);
		return -1;
	}

	if (on_slider)
	{
		if (((KeyMap[DIK_RIGHT] | KeyMap[DIK_LEFT]) & 0x80) == 0)
		{
			delay = 0;
			delval = 40;
		}

		if (delay)
		{
			delay--;
		}
		else if (KeyMap[DIK_LEFT] & 0x80)
		{
			buttons[buttons[selection].l].selfunc(2);
			delay = delval;
			delval = 3;
		}
		else if (KeyMap[DIK_RIGHT] & 0x80)
		{
			buttons[buttons[selection].r].selfunc(2);
			delay = delval;
			delval = 3;
		}
	}
	else if (CheckKey(DIK_LEFT))
	{
		selection = buttons[selection].l;
	}
	else if (CheckKey(DIK_RIGHT))
	{
		selection = buttons[selection].r;
	}

	if (CheckKey(DIK_UP))
	{
		selection = buttons[selection].u;
	}
	else if (CheckKey(DIK_DOWN))
	{
		selection = buttons[selection].d;
	}
	else if (CheckKey(DIK_RETURN))
	{
		PlayMotelClickSound();
		buttons[selection].choosefunc();
	}

	if (old != selection)
	{
		PlayMotelClickSound();
		buttons[old].unselfunc(2);
		buttons[selection].selfunc(2);
	}

	return selection;
}

// [0x418d61] [D]
void PlayMotelClickSound(void)
{
	SimplePlaySound(0, 9, 22050);
}

// [0x418d77] [D]
void UpdateMotel(void)
{
	ControlMotelFade();
	FadeTextToFadeColour();
	FollowCameraPath();
}

// [0x418d8b] [D]
// Draws the room (the map) with the palette of the city, or of the car select.
void StartOfMotelFrame(void)
{
	arc_view = view_arc[0];
	num_active_cars = 0;

	if (gUseCDAudio && gMusicPlaying == 0)
		PlayCDTrack(3);

	UpdateSound();
	ReallyUpdateSound();

	FlushLamps();
	SetupFrustrum();

	if (gDoingCarSelect)
	{
		SwapPalette(1);
	}
	else
	{
		switch (GameLevel)
		{
			case 1:
				SwapPalette(4);
				break;
			case 2:
				SwapPalette(6);
				break;
			case 3:
				SwapPalette(5);
				break;
			default:
				SwapPalette(7);
				break;
		}
	}

	DrawMap(0);
	DrawMap(1);

	SwapPalette(24);
}

// [0x418e52] [D]
// The menu texts: drop shadowed in their colour faded with the room (the title in yellow), or
// while the camera moves as an outline (subtractive twice, then additive).
void DrawMotelMenu(FE_TEXT *texts)
{
	byte r;
	byte g;
	byte b;
	int i;

	for (i = 0; texts[i].text; i++)
	{
		if (gCameraMoving || motel_font_override)
		{
			WriteScaledStringWithMode((float)texts[i].x, (float)texts[i].y, texts[i].text, 0, texts[i].justify | PRINT_SUBTRACTIVE);
			WriteScaledStringWithMode((float)texts[i].x + 2.0f, (float)texts[i].y + 2.0f, texts[i].text, 0, texts[i].justify | PRINT_SUBTRACTIVE);
			WriteScaledStringWithMode((float)texts[i].x, (float)texts[i].y, texts[i].text, 0, texts[i].justify | PRINT_ADDITIVE);
		}
		else
		{
			if (i)
			{
				r = (int)((float)texts[i].color.r * gMotelFadeIntensity);
				g = (int)((float)texts[i].color.g * gMotelFadeIntensity);
				b = (int)((float)texts[i].color.b * gMotelFadeIntensity);
			}
			else
			{
				r = (int)(255.0f * gMotelFadeIntensity);
				g = r;
				b = (int)(128.0f * gMotelFadeIntensity);
			}

			WriteDropShadowedString(texts[i].x, texts[i].y, texts[i].text, 0, texts[i].justify, r, g, b);
		}
	}
}
