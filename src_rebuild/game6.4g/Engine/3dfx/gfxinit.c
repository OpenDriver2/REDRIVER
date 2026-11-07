/*
 * Engine/3dfx/gfxinit.c - graphics system initialisation
 *
 * Rebuilt from Driver_sym2_94.i64 (IDA folder "Engine\3dfx\gfxinit.c").
 */

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
#include "D3DGlide/glide_gl.h"

#define VIEWPORT_CLEAR_FULL		1	// clear the whole screen, then clip to the viewport
#define VIEWPORT_CLEAR			2	// clip to the viewport and clear it

#define OOZ_TABLE_SIZE			56250

// NOTE: in the binary this is an inline `fld; fistp INT_FLT; mov eax, INT_FLT` sequence,
// the original macro/inline is unknown

// polygon draw function tables, D3D and Glide variants

void InitEngineGlobals(void);
void ResetViewport(int x, int y, char clear);
void SetFogType(void);
int EnableDepthMask(int enabled);

// uninitialised data (0xb9b314, start of the file's block per IDA mark)
int got_3d_hardware;
int got_3d_device;
int device_type;
int glide_origin;
// NOTE: hwconfig (0xb9b3d0) and bDepthBufferStatus (0xb9b464) follow later in the block;
// the slots between them are not known yet
GrHwConfiguration hwconfig;
int bDepthBufferStatus;

// NOTE: IDA names it gfxinit._glide_active (a static), but frontend.c StartGame reads it directly,
// so it must be a global
char glide_active;

// NOTE: moved here from the generated stubs; no IDA mark, the owner is a guess from the users
// and the neighbouring data
// NOTE: [A] always 0: the OpenGL device is on the _D3D polygon tables and its vertices
int using_glide;
void (**PolygonDrawFunction)(int);
void (**ClipFarPolygonDrawFunction)(int);
void (**ClipNearPolygonDrawFunction)(int);
float d3d_screen_xmid;
float DIST;
float ambient_light_level;
int first_error;
float clipxmin;
float clipxmax;
float clipymin;
float clipymax;
float true_screen_xmid;
int gPalettePresent[50];
int gCurrentPalette;
float screen_ymid;
float d3d_screen_ymid;
int LastPaletteNo;
int gPreviousAlpha;
float screen_xmid;
char shitfog[64];
int last_filter_state;
char gPalette[50][1024];
float screen_scale;
char last_trans_on;
float cell_recip;
int last_tset;
int display_height;
float true_screen_ymid;
float mirror_far_clip;
char highlight;
float true_screen_scale;
int current_vertex_computed_value;
float far_clip;
int display_width;
int current_object_computed_value;
int last_state;
int viewport_reset;
int ooz_far_clip;

// NOTE: [A] how much wider the screen is than 4:3, 1 at 4:3 and narrower. It only widens the view:
// the height of the picture stays as at 4:3.
float wide_aspect = 1.0f;

// NOTE: [A] set while the HUD draws: what is near the sides of 640x480 goes to the sides of the screen
int wide_hud_edges;
int gUseAlphaTests;

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

// [0x426082] [D] [A]
// NOTE: IDA shows an int return (d3d_SetScreenScale's leftover); nothing uses it
// NOTE: [A] tells the OpenGL device the scale
void ResetViewport(int x, int y, char clear)
{
	if (true_screen_scale != screen_scale)
	{
		true_screen_scale = screen_scale;

		screen_scale = (float)display_width / 640.0f / wide_aspect * screen_scale;
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

	gl_SetScreenScale(screen_scale);
}

// [0x42640e] [D]
// Shrinks the 3D view by 5 % (down to 20 %).
void ReduceViewport(void)
{
	if (screen_scale > 0.2)
	{
		screen_scale = screen_scale - 0.05f;

		ResetViewport(0, 0, 1);
		viewport_reset = 2;
	}
}

// [0x426450] [D]
// Grows the 3D view by 5 % (up to the full screen).
void ExpandViewport(void)
{
	if (screen_scale < 1.0f)
	{
		screen_scale = screen_scale + 0.05f;

		if (screen_scale > 0.99f && screen_scale < 1.01)
			screen_scale = 1.0f;

		ResetViewport(0, 0, 1);
		viewport_reset = 2;
	}
}

// [0x4264c2] [D]
int Detect3dfx(void)
{
	grGlideInit();
	return grSstQueryHardware(&hwconfig) == FXTRUE;
}

// [0x4264eb] [D] [A]
// Opens the 3D window (fullscreen at display_width or windowed) and sets the default render state.
// NOTE: [A] no window handle, the OpenGL device has the SDL window
int InitDrawingSystem(int fullscreen, int keepViewport)
{
	BOOL flipped;
	GrScreenResolution_t res;

	// NOTE: [A] the sizes the device opens at, display_height changes later for the fly by
	wide_aspect = 1.0f;

	if (display_width * 3 > display_height * 4)
		wide_aspect = (float)(display_width * 3) / (float)(display_height * 4);

	// NOTE: [A] windowed mode keeps the frame and the window position
	SetGameWindowSize(display_width, display_height);

	flipped = (gDoAussie != 0);	// upside down for Australia

	InitEngineGlobals();
	FPUInit();

	grSstSelect(0);

	if (!fullscreen)
	{
		if (!grSstWinOpen(0, GR_RESOLUTION_NONE, GR_REFRESH_60Hz, GR_COLORFORMAT_ARGB, flipped, 2, 1))
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
				// NOTE: [A] the OpenGL device takes any size, /RES
				res = GR_RESOLUTION_NONE;
				break;
		}

		if (!grSstWinOpen(0, res, GR_REFRESH_60Hz, GR_COLORFORMAT_ARGB, flipped, 2, 1))
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

// [0x42683e] [D]
// Draws a flat line from x1 y1 to x2 y2 of the 640x480 screen (scaled by screen_scale about the
// centre) in colour 0xRRGGBB.
// NOTE: z, ooz and a of the vertices are left unset, as in the original
void DrawScaledLine(float x1, float y1, float x2, float y2, int colour)
{
	GrVertex v2;
	GrVertex v1;
	float width;
	float height;

	if (last_state != FLAT3)
	{
		grColorCombine(GR_COMBINE_FUNCTION_LOCAL, GR_COMBINE_FACTOR_ZERO, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_CONSTANT, FXFALSE);
		last_state = FLAT3;
	}

	if (last_trans_on)
	{
		grAlphaBlendFunction(GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_ONE, GR_BLEND_ZERO);
		last_trans_on = 0;
	}

	x2 = x2 / 640.0f;
	y2 = y2 / 480.0f;
	x1 = x1 / 640.0f;
	y1 = y1 / 480.0f;

	width = (float)display_width;
	x2 = ((x2 - 0.5f) * screen_scale + 0.5f) * width;
	height = (float)display_height;
	y2 = ((y2 - 0.5f) * screen_scale + 0.5f) * height;
	x1 = ((x1 - 0.5f) * screen_scale + 0.5f) * width;
	y1 = ((y1 - 0.5f) * screen_scale + 0.5f) * height;

	v2.r = (float)((colour >> 16) & 0xFF);
	v1.r = v2.r;
	v2.g = (float)((colour >> 8) & 0xFF);
	v1.g = v2.g;
	v2.b = (float)(colour & 0xFF);
	v1.b = v2.b;

	if (using_glide)
	{
		v1.oow = 1.0f;
		v2.oow = 1.0f;
	}
	else
	{
		v1.oow = 0.015625f;
		v2.oow = 0.015625f;
	}

	v1.x = x1;
	v1.y = y1;
	v2.x = x2;
	v2.y = y2;

	grDrawLine(&v1, &v2);

	last_state = -1;
	last_trans_on = -1;
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
