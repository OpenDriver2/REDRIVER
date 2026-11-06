/*
 * Engine/3dfx/gfxinit.c - graphics system initialisation
 *
 * Rebuilt from Driver_sym2_94.i64 (IDA folder "Engine\3dfx\gfxinit.c").
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

// sets a Glide pointer from glide2x.dll (the original casts each result to the pointer type)
#define GET_GLIDE_PROC(ptr, name)	(*(FARPROC *)&(ptr) = GetProcAddress(glide_dll, (name)))

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

// NOTE: moved here from the generated stubs; no IDA mark, the owner is a guess from the users
// and the neighbouring data
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
int gUseAlphaTests;

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

// [0x4251b1] [D]
// Takes the Glide entry points from glide2x.dll; the generic draw functions are cleared.
int UseGlideDevice(void)
{
	if (!glide_loaded)
		return 0;

	GET_GLIDE_PROC(grAADrawLine, "_grAADrawLine@8");
	GET_GLIDE_PROC(grAADrawPoint, "_grAADrawPoint@4");
	GET_GLIDE_PROC(grAADrawPolygon, "_grAADrawPolygon@12");
	GET_GLIDE_PROC(grAADrawPolygonVertexList, "_grAADrawPolygonVertexList@8");
	GET_GLIDE_PROC(grAADrawTriangle, "_grAADrawTriangle@24");
	GET_GLIDE_PROC(grAlphaBlendFunction, "_grAlphaBlendFunction@16");
	GET_GLIDE_PROC(grAlphaCombine, "_grAlphaCombine@20");
	GET_GLIDE_PROC(grAlphaControlsITRGBLighting, "_grAlphaControlsITRGBLighting@4");
	GET_GLIDE_PROC(grAlphaTestFunction, "_grAlphaTestFunction@4");
	GET_GLIDE_PROC(grAlphaTestReferenceValue, "_grAlphaTestReferenceValue@4");
	GET_GLIDE_PROC(grBufferClear, "_grBufferClear@12");
	GET_GLIDE_PROC(grBufferNumPending, "_grBufferNumPending@0");
	GET_GLIDE_PROC(grBufferSwap, "_grBufferSwap@4");
	GET_GLIDE_PROC(grCheckForRoom, "_grCheckForRoom@4");
	GET_GLIDE_PROC(grChromakeyMode, "_grChromakeyMode@4");
	GET_GLIDE_PROC(grChromakeyValue, "_grChromakeyValue@4");
	GET_GLIDE_PROC(grClipWindow, "_grClipWindow@16");
	GET_GLIDE_PROC(grColorCombine, "_grColorCombine@20");
	GET_GLIDE_PROC(grColorMask, "_grColorMask@8");
	GET_GLIDE_PROC(grConstantColorValue4, "_grConstantColorValue4@16");
	GET_GLIDE_PROC(grConstantColorValue, "_grConstantColorValue@4");
	GET_GLIDE_PROC(grCullMode, "_grCullMode@4");
	GET_GLIDE_PROC(grDepthBiasLevel, "_grDepthBiasLevel@4");
	GET_GLIDE_PROC(grDepthBufferFunction, "_grDepthBufferFunction@4");
	GET_GLIDE_PROC(grDepthBufferMode, "_grDepthBufferMode@4");
	GET_GLIDE_PROC(grDepthMask, "_grDepthMask@4");
	GET_GLIDE_PROC(grDisableAllEffects, "_grDisableAllEffects@0");
	GET_GLIDE_PROC(grDitherMode, "_grDitherMode@4");
	GET_GLIDE_PROC(grDrawLine, "_grDrawLine@8");
	GET_GLIDE_PROC(grDrawPlanarPolygon, "_grDrawPlanarPolygon@12");
	GET_GLIDE_PROC(grDrawPlanarPolygonVertexList, "_grDrawPlanarPolygonVertexList@8");
	GET_GLIDE_PROC(grDrawPoint, "_grDrawPoint@4");
	GET_GLIDE_PROC(grDrawPolygon, "_grDrawPolygon@12");
	GET_GLIDE_PROC(grDrawPolygonVertexList, "_grDrawPolygonVertexList@8");
	GET_GLIDE_PROC(grDrawTriangle, "_grDrawTriangle@12");
	GET_GLIDE_PROC(grFogColorValue, "_grFogColorValue@4");
	GET_GLIDE_PROC(grFogMode, "_grFogMode@4");
	GET_GLIDE_PROC(grFogTable, "_grFogTable@4");
	GET_GLIDE_PROC(grGammaCorrectionValue, "_grGammaCorrectionValue@4");
	GET_GLIDE_PROC(grGlideGetState, "_grGlideGetState@4");
	GET_GLIDE_PROC(grGlideGetVersion, "_grGlideGetVersion@4");
	GET_GLIDE_PROC(grGlideSetState, "_grGlideSetState@4");
	GET_GLIDE_PROC(grGlideShamelessPlug, "_grGlideShamelessPlug@4");
	GET_GLIDE_PROC(grHints, "_grHints@8");
	GET_GLIDE_PROC(grLfbConstantAlpha, "_grLfbConstantAlpha@4");
	GET_GLIDE_PROC(grLfbConstantDepth, "_grLfbConstantDepth@4");
	GET_GLIDE_PROC(gr_LfbLock, "_grLfbLock@24");
	GET_GLIDE_PROC(grLfbReadRegion, "_grLfbReadRegion@28");
	GET_GLIDE_PROC(gr_LfbUnlock, "_grLfbUnlock@8");
	GET_GLIDE_PROC(grLfbWriteColorFormat, "_grLfbWriteColorFormat@4");
	GET_GLIDE_PROC(grLfbWriteColorSwizzle, "_grLfbWriteColorSwizzle@8");
	GET_GLIDE_PROC(grLfbWriteRegion, "_grLfbWriteRegion@32");
	GET_GLIDE_PROC(grRenderBuffer, "_grRenderBuffer@4");
	GET_GLIDE_PROC(grResetTriStats, "_grResetTriStats@0");
	GET_GLIDE_PROC(grSplash, "_grSplash@20");
	GET_GLIDE_PROC(grSstIdle, "_grSstIdle@0");
	GET_GLIDE_PROC(grSstIsBusy, "_grSstIsBusy@0");
	GET_GLIDE_PROC(grSstOrigin, "_grSstOrigin@4");
	GET_GLIDE_PROC(grSstPerfStats, "_grSstPerfStats@4");
	GET_GLIDE_PROC(grSstQueryBoards, "_grSstQueryBoards@4");
	GET_GLIDE_PROC(grSstResetPerfStats, "_grSstResetPerfStats@0");
	GET_GLIDE_PROC(grSstScreenHeight, "_grSstScreenHeight@0");
	GET_GLIDE_PROC(grSstScreenWidth, "_grSstScreenWidth@0");
	GET_GLIDE_PROC(grSstSelect, "_grSstSelect@4");
	GET_GLIDE_PROC(grSstStatus, "_grSstStatus@0");
	GET_GLIDE_PROC(grSstVRetraceOn, "_grSstVRetraceOn@0");
	GET_GLIDE_PROC(grSstVideoLine, "_grSstVideoLine@0");
	GET_GLIDE_PROC(grTexCalcMemRequired, "_grTexCalcMemRequired@16");
	GET_GLIDE_PROC(grTexClampMode, "_grTexClampMode@12");
	GET_GLIDE_PROC(grTexCombine, "_grTexCombine@28");
	GET_GLIDE_PROC(grTexCombineFunction, "_grTexCombineFunction@8");
	GET_GLIDE_PROC(grTexDetailControl, "_grTexDetailControl@16");
	GET_GLIDE_PROC(grTexDownloadMipMap, "_grTexDownloadMipMap@16");
	GET_GLIDE_PROC(grTexDownloadMipMapLevel, "_grTexDownloadMipMapLevel@32");
	GET_GLIDE_PROC(grTexDownloadMipMapLevelPartial, "_grTexDownloadMipMapLevelPartial@40");
	GET_GLIDE_PROC(grTexDownloadTable, "_grTexDownloadTable@12");
	GET_GLIDE_PROC(grTexDownloadTablePartial, "_grTexDownloadTablePartial@20");
	GET_GLIDE_PROC(grTexFilterMode, "_grTexFilterMode@12");
	GET_GLIDE_PROC(grTexLodBiasValue, "_grTexLodBiasValue@8");
	GET_GLIDE_PROC(grTexMaxAddress, "_grTexMaxAddress@4");
	GET_GLIDE_PROC(grTexMinAddress, "_grTexMinAddress@4");
	GET_GLIDE_PROC(grTexMipMapMode, "_grTexMipMapMode@12");
	GET_GLIDE_PROC(grTexMultibase, "_grTexMultibase@8");
	GET_GLIDE_PROC(grTexMultibaseAddress, "_grTexMultibaseAddress@20");
	GET_GLIDE_PROC(grTexNCCTable, "_grTexNCCTable@8");
	GET_GLIDE_PROC(grTexSource, "_grTexSource@16");
	GET_GLIDE_PROC(grTexTextureMemRequired, "_grTexTextureMemRequired@8");
	GET_GLIDE_PROC(grTriStats, "_grTriStats@8");
	GET_GLIDE_PROC(gu3dfGetInfo, "_gu3dfGetInfo@8");
	GET_GLIDE_PROC(gu3dfLoad, "_gu3dfLoad@8");
	GET_GLIDE_PROC(guAADrawTriangleWithClip, "_guAADrawTriangleWithClip@12");
	GET_GLIDE_PROC(guAlphaSource, "_guAlphaSource@4");
	GET_GLIDE_PROC(guColorCombineFunction, "_guColorCombineFunction@4");
	GET_GLIDE_PROC(guDrawPolygonVertexListWithClip, "_guDrawPolygonVertexListWithClip@8");
	GET_GLIDE_PROC(guDrawTriangleWithClip, "_guDrawTriangleWithClip@12");
	GET_GLIDE_PROC(guEncodeRLE16, "_guEncodeRLE16@16");
	GET_GLIDE_PROC(guEndianSwapBytes, "_guEndianSwapBytes@4");
	GET_GLIDE_PROC(guEndianSwapWords, "_guEndianSwapWords@4");
	GET_GLIDE_PROC(guFogGenerateExp2, "_guFogGenerateExp2@8");
	GET_GLIDE_PROC(guFogGenerateExp, "_guFogGenerateExp@8");
	GET_GLIDE_PROC(guFogGenerateLinear, "_guFogGenerateLinear@12");
	GET_GLIDE_PROC(guFogTableIndexToW, "_guFogTableIndexToW@4");
	GET_GLIDE_PROC(guTexAllocateMemory, "_guTexAllocateMemory@60");
	GET_GLIDE_PROC(guTexChangeAttributes, "_guTexChangeAttributes@48");
	GET_GLIDE_PROC(guTexCombineFunction, "_guTexCombineFunction@8");
	GET_GLIDE_PROC(guTexCreateColorMipMap, "_guTexCreateColorMipMap@0");
	GET_GLIDE_PROC(gu_TexDownloadMipMap, "_guTexDownloadMipMap@12");
	GET_GLIDE_PROC(guTexDownloadMipMapLevel, "_guTexDownloadMipMapLevel@12");
	GET_GLIDE_PROC(guTexGetCurrentMipMap, "_guTexGetCurrentMipMap@4");
	GET_GLIDE_PROC(guTexGetMipMapInfo, "_guTexGetMipMapInfo@4");
	GET_GLIDE_PROC(guTexMemQueryAvail, "_guTexMemQueryAvail@4");
	GET_GLIDE_PROC(guTexMemReset, "_guTexMemReset@0");
	GET_GLIDE_PROC(guTexSource, "_guTexSource@4");
	GET_GLIDE_PROC(gr_GlideInit, "_grGlideInit@0");
	GET_GLIDE_PROC(gr_GlideShutdown, "_grGlideShutdown@0");
	GET_GLIDE_PROC(gr_SstQueryHardware, "_grSstQueryHardware@4");
	GET_GLIDE_PROC(gr_SstWinOpen, "_grSstWinOpen@28");
	GET_GLIDE_PROC(gr_SstWinClose, "_grSstWinClose@0");
	GET_GLIDE_PROC(gr_SstControl, "_grSstControl@4");

	gDrawLine = NULL;
	gDrawPolygonVertexList = NULL;
	gDrawTriangle = NULL;
	gDrawTriangleWithClip = NULL;

	return 1;
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
