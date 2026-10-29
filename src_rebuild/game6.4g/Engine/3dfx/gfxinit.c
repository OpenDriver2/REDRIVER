/*
 * Engine/3dfx/gfxinit.c - graphics system initialisation
 *
 * Rebuilt from Driver_sym2_94.i64 (IDA folder "Engine\3dfx\gfxinit.c"); partially translated.
 */

#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "dr_types.h"
#include "D3DGlide/system.h"
#include "dr_globals.h"
#include "Engine/3dfx/gfxinit.h"
#include "shell.h"

#include "../../D3DGlide/glide_ptrs.h"
#include "asmmath.h"
#include "Engine/3dfx/polydraw.h"
#include "testmath.h"
#include "glaunch.h"
#include "D3DGlide/d3dglide.h"

#define MAX_3D_DEVICES			8
#define DEVICE_TYPE_GLIDE		0

#define VIEWPORT_CLEAR_FULL		1	// clear the whole screen, then clip to the viewport
#define VIEWPORT_CLEAR			2	// clip to the viewport and clear it

#define OOZ_TABLE_SIZE			56250

// NOTE: in the binary this is an inline `fld; fistp INT_FLT; mov eax, INT_FLT` sequence,
// the original macro/inline is unknown

// polygon draw function tables, D3D and Glide variants

int UseGlideDevice(void);
int LoadGlide(void);
void InitEngineGlobals(void);
void ResetViewport(int x, int y, char clear);
void SetFogType(void);
int EnableDepthMask(int enabled);

// uninitialised data (0xb9b314, start of the file's block per IDA mark)
int got_3d_hardware;
int got_3d_device;
int device_type;
int glide_origin;
int num_3d_devices;
GUID *device_guid;
HMODULE glide_dll;
int glide_loaded;
// NOTE: hwconfig (0xb9b3d0) and bDepthBufferStatus (0xb9b464) follow later in the block;
// the slots between them are not known yet
GrHwConfiguration hwconfig;
int bDepthBufferStatus;

// NOTE: IDA names it gfxinit._glide_active (a static), but frontend.c StartGame reads it directly,
// so it must be a global
char glide_active;

// [0x425100] [D]
// Adds the 3dfx card (glide2x.dll) as a device.
int InitGlideDevice(void)
{
	if (num_3d_devices < MAX_3D_DEVICES)
	{
		if (!glide_loaded)
			glide_loaded = LoadGlide();

		if (glide_loaded)
		{
			memset(&devices_3d[num_3d_devices], 0, sizeof(DEVICE_3D));
			sprintf(devices_3d[num_3d_devices].name, "3Dfx Glide");

			devices_3d[num_3d_devices].type = DEVICE_TYPE_GLIDE;
			devices_3d[num_3d_devices].hardware_8bit = 0;
			devices_3d[num_3d_devices++].field_48 = NULL;
		}
	}

	return glide_loaded;
}

// [0x425c7d] [D]
void FreeGlide(void)
{
	if (glide_dll)
	{
		FreeLibrary(glide_dll);
		glide_dll = NULL;
	}
}

// [0x425ca1] [D]
int LoadGlide(void)
{
	glide_dll = LoadLibrary("glide2x.dll");
	return glide_dll != NULL;
}

// [0x425fc0] [D]
void InitEngineGlobals(void)
{
	DIST = 1152.0f;
	ambient_light_level = 0.5f;
	cell_recip = 1.0f / 3000.0f;

	true_screen_xmid = (float)display_width / 2.0f;
	true_screen_ymid = (float)display_height / 2.0f;

	if (!gDoFrontend)
		far_clip = 42000.0f;

	true_screen_scale = 54321.0f;		// forces ResetViewport to recalculate
	ooz_far_clip = OOZ_TABLE_SIZE;

	LastPaletteNo = 0;
	first_error = 1;
	gPreviousAlpha = -1;
	gUseAlphaTests = 1;
	viewport_reset = 0;
	gCurrentPalette = 0;
	highlight = 0;
}

// [0x426082] [D]
// NOTE: IDA shows an int return (d3d_SetScreenScale's leftover); nothing uses it
void ResetViewport(int x, int y, char clear)
{
	if (true_screen_scale != screen_scale)
	{
		true_screen_scale = screen_scale;

		screen_scale = (float)display_width / 640.0f * screen_scale;
		PreCalculateOOZ();

		screen_scale = true_screen_scale;
	}

	if (gGameLoading)
	{
		clipxmin = 0.0f;
		clipymin = 0.0f;
		clipxmax = (float)display_width + 0.0f;
		clipymax = (float)display_height + 0.0f;
	}
	else
	{
		clipxmin = (float)display_width / 2.0f + (float)x - (float)display_width * screen_scale / 2.0f;
		clipymin = (float)display_height / 2.0f + (float)y - (float)display_height * screen_scale / 2.0f;
		clipxmax = (float)display_width * screen_scale + clipxmin;
		clipymax = (float)display_height * screen_scale + clipymin;
	}

	screen_xmid = (float)(display_width / 2);
	screen_ymid = (float)(display_height / 2);

	d3d_screen_xmid = screen_xmid + (float)x;
	d3d_screen_ymid = screen_ymid + (float)y;

	screen_xmid = (float)x + VERTEX_SNAPPER + screen_xmid;
	screen_ymid = (float)y + VERTEX_SNAPPER + screen_ymid;

	true_screen_xmid = (float)(display_width / 2);
	true_screen_ymid = (float)(display_height / 2);

	if (clear == VIEWPORT_CLEAR_FULL)
	{
		grClipWindow(0, 0, display_width, display_height);
		grBufferClear(0, 0, GR_WDEPTHVALUE_FARTHEST);
		grClipWindow(float_to_int(clipxmin), float_to_int(clipymin), float_to_int(clipxmax), float_to_int(clipymax));
	}
	else if (clear == VIEWPORT_CLEAR)
	{
		// NOTE: this branch converts with plain casts (_ftol) in the binary, the others with float_to_int
		grClipWindow((FxU32)clipxmin, (FxU32)clipymin, (FxU32)clipxmax, (FxU32)clipymax);
		grBufferClear(0, 0, GR_WDEPTHVALUE_FARTHEST);
	}
	else
	{
		grClipWindow(float_to_int(clipxmin), float_to_int(clipymin), float_to_int(clipxmax), float_to_int(clipymax));
	}

	d3d_SetScreenScale(screen_scale);
}

// [0x4264c2] [D]
int Detect3dfx(void)
{
	grGlideInit();
	return grSstQueryHardware(&hwconfig) == FXTRUE;
}

// [0x4264eb] [D] [A]
// Opens the 3D window (fullscreen at display_width or windowed) and sets the default render state.
int InitDrawingSystem(HWND hWnd, int fullscreen, int keepViewport)
{
	BOOL flipped;
	GrScreenResolution_t res;

	// NOTE: [A] windowed mode keeps the frame and the window position
	if (gWindowed)
		SetGameWindowSize(hWnd, display_width, display_height);
	else
		MoveWindow(hWnd, 0, 0, display_width, display_height, TRUE);

	flipped = (gDoAussie != 0);	// upside down for Australia

	InitEngineGlobals();
	FPUInit();

	grSstSelect(0);

	if (!fullscreen)
	{
		if (!grSstWinOpen((FxU32)hWnd, GR_RESOLUTION_NONE, GR_REFRESH_60Hz, GR_COLORFORMAT_ARGB, flipped, 2, 1))
			return 0;
	}
	else if (fullscreen == 1)
	{
		switch (display_width)
		{
			case 320:
				res = GR_RESOLUTION_320x240;
				break;
			case 512:
				res = GR_RESOLUTION_512x384;
				break;
			case 640:
				res = GR_RESOLUTION_640x480;
				break;
			case 800:
				res = GR_RESOLUTION_800x600;
				break;
			case 1024:
				res = GR_RESOLUTION_1024x768;
				break;
			case 1280:
				res = GR_RESOLUTION_1280x1024;
				break;
			case 1600:
				res = GR_RESOLUTION_1600x1200;
				break;
			default:
				return 0;
		}

		if (!grSstWinOpen((FxU32)hWnd, res, GR_REFRESH_60Hz, GR_COLORFORMAT_ARGB, flipped, 2, 1))
			return 0;
	}

	if (using_glide)
	{
		PolygonDrawFunction = PolygonDrawFunction_Glide;
		ClipNearPolygonDrawFunction = ClipNearPolygonDrawFunction_Glide;
		ClipFarPolygonDrawFunction = ClipFarPolygonDrawFunction_Glide;
	}
	else
	{
		PolygonDrawFunction = PolygonDrawFunction_D3D;
		ClipNearPolygonDrawFunction = ClipNearPolygonDrawFunction_D3D;
		ClipFarPolygonDrawFunction = ClipFarPolygonDrawFunction_D3D;
	}

	grDepthBufferMode(GR_DEPTHBUFFER_WBUFFER);
	EnableDepthMask(1);
	grDepthBufferFunction(GR_CMP_LEQUAL);
	grCullMode(GR_CULL_DISABLE);

	if (!keepViewport)
	{
		screen_scale = 1.0f;
		ResetViewport(0, 0, VIEWPORT_CLEAR_FULL);

		current_vertex_computed_value = 28011972;	// January 28th, 1972 - someone's birthday?!
		current_object_computed_value = 2504;
	}

	last_tset = -1;
	last_state = -1;

	grAlphaTestReferenceValue(0x82);
	grConstantColorValue(0xBB000000);

	SetFogType();

	grTexCombineFunction(GR_TMU0, GR_TEXTURECOMBINE_DECAL);

	glide_active = 1;
	last_filter_state = -1;

	return 1;
}

// [0x4267de] [D]
void SetFogType(void)
{
	if (!gDarkNight || do_fogging)
	{
		grFogColorValue(0xAAAAAA);
		guFogGenerateExp(shitfog, 0.00012f);
	}
	else
	{
		grFogColorValue(0);
		guFogGenerateExp(shitfog, 0.0001f);
		do_fogging = 1;
	}

	grFogTable(shitfog);
}

// [0x426a17] [D]
void ShutdownDrawingSystem(void)
{
	glide_active = 0;
	grGlideShutdown();
	FPURestore();
}

// [0x426a2d] [D]
void StartOfFrame(void)
{
	if (do_fogging)
		grFogMode(GR_FOG_WITH_TABLE);
	else
		grFogMode(GR_FOG_DISABLE);

	// without do_clear only the depth buffer is cleared
	if (!do_clear)
		grColorMask(FXFALSE, FXFALSE);

	grBufferClear(0, 0, GR_WDEPTHVALUE_FARTHEST);

	if (!do_clear)
		grColorMask(FXTRUE, FXFALSE);
}

// [0x426a8a] [D]
void EndOfFrame(void)
{
	grBufferSwap(0);

	if (viewport_reset)
	{
		ResetViewport(0, 0, 1);
		viewport_reset--;
	}

	current_object_computed_value++;
}

// [0x426aca] [D]
int EnableDepthMask(int enabled)
{
	int old_status;

	old_status = bDepthBufferStatus;
	bDepthBufferStatus = enabled;

	grDepthMask(enabled);

	return old_status;
}

// [0x426af0] [D]
// Downloads a texture palette (-1 forgets the current one).
void SwapPalette(int PaletteNo)
{
	if (PaletteNo == -1)
	{
		LastPaletteNo = -1;
	}
	else if (PaletteNo != LastPaletteNo && gPalettePresent[PaletteNo])
	{
		grTexDownloadTable2(GR_TMU0, GR_TEXTABLE_PALETTE, gPalette[PaletteNo], PaletteNo);
		LastPaletteNo = PaletteNo;
	}
}
