/*
 * D3DGlide/glide2x.c - Direct3D Glide emulation (DirectDraw4 / Direct3D3 interfaces)
 *
 * Rebuilt from Driver_sym2_94.i64 (IDA folder "D3DGlide\glide2x.c"); partially translated.
 */

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <ddraw.h>
#include <d3d.h>

#include "dr_types.h"
#include "D3DGlide/system.h"
#include "frontend/config.h"
#include "shell.h"

#include "d3dglide.h"
#include "glide_ptrs.h"

typedef struct EMULTEXTURE EMULTEXTURE;

struct EMULTEXTURE
{
	int tset;
	int palette;
	int unk_3;
	int unk_4;
	LPDIRECTDRAWSURFACE4 surface;
	LPDIRECT3DTEXTURE2 texture;
	EMULTEXTURE *next;
	EMULTEXTURE *prev;
};

typedef struct GLIDETEXTURE
{
	LPDIRECTDRAWSURFACE4 surface;
	LPDIRECT3DTEXTURE2 texture;
	LPDIRECTDRAWPALETTE palette;
	int depth;
	int emulated;
	int palette2;
	EMULTEXTURE *emultexture;
	LPDIRECT3DTEXTURE2 texture2;
} GLIDETEXTURE;

// one D3D capable DirectDraw driver found by EnumDevicesCallback
typedef struct DEVICE_INFO_2 DEVICE_INFO_2;
typedef struct DEVICE_INFO_THING DEVICE_INFO_THING;

struct DEVICE_INFO_2
{
	GUID field_00;
	D3DDEVICEDESC field_10;
	D3DDEVICEDESC field_100;
	DEVICE_INFO_2 *next;
	DEVICE_INFO_2 *prev;
};

struct DEVICE_INFO_THING
{
	char name[64];			// NOTE: IDA has field_00..field_3C; strcpy'd driver description
	GUID guid;
	int field_50;
	int IsDisplayDevice;
	DEVICE_INFO_2 *first;
	DEVICE_INFO_2 *last;
	DEVICE_INFO_THING *next;
	DEVICE_INFO_THING *prev;
};

struct DEVICE_3D_INFO
{
	LPGUID field_00;
	GUID field_04;
	LPGUID field_14;
	GUID field_18;
	DDCAPS_DX6 HWCaps;
	DDCAPS_DX6 SWCaps;
	D3DDEVICEDESC HWDesc;
	D3DDEVICEDESC SWDesc;
	int UseHardwareColorModel;
	int field_51c;
	int field_520;
	int field_524;
	DDPIXELFORMAT pfTex16;
	DDPIXELFORMAT pfTex8;
	DDPIXELFORMAT pfZBuffer;
	int emulate_8bit;
	int field_58c;
};

#define MAX_3D_DEVICES		8
#define MAX_TEXTURES		32		// NOTE: known constant
#define MAX_VERTICES		4500	// NOTE: known constant

#define DEVICE_TYPE_GLIDE	0
#define DEVICE_TYPE_D3D		1

#define POLY_FLAGS			(D3DDP_DONOTUPDATEEXTENTS | D3DDP_DONOTLIGHT)	// NOTE: known constant

// DEVICE_3D_INFO.field_58c: texture/z formats found by EnumPixelFormatsCallback
#define PF_FOUND_8BIT		0x01
#define PF_FOUND_16BIT		0x02
#define PF_FOUND_ZBUFFER	0x04
#define PF_HAVE_8BIT		0x10
#define PF_HAVE_RGBA5551	0x20
#define PF_HAVE_ZBUFFER		0x40

// device_flags: blending abilities of the 3D device
#define DEVFLAG_DEST_SRCALPHA		0x01
#define DEVFLAG_DEST_INVSRCCOLOR	0x02
#define DEVFLAG_DEST_ONE			0x04
#define DEVFLAG_ALWAYS				0x08
#define DEVFLAG_WBUFFER				0x10
#define DEVFLAG_SRC_DESTCOLOR		0x20

/* D3DGlide/system.c, Engine/3dfx/gfxinit.c, frontend/config.c */
// NOTE: not from Engine/3dfx/gfxinit.h: it declares gfxinit.c's glide_active, which clashes with
// this file's own static glide_active (tools/headers.py: keep)
extern int num_3d_devices;
extern GUID *device_guid;

void OutputDebugMessage(char *text);
BOOL WINAPI EnumDevicesCallback(GUID *lpGUID, LPSTR lpDriverDesc, LPSTR lpDriverName, LPVOID lpContext, HMONITOR hm);
HRESULT CALLBACK GetDeviceInfo(GUID *lpGuid, LPSTR lpDeviceDescription, LPSTR lpDeviceName, LPD3DDEVICEDESC lpHWDesc, LPD3DDEVICEDESC lpHELDesc, LPVOID lpContext);
void Find3DDevices(void);
int CanUse3DDevice(DEVICE_3D *device);
HRESULT CALLBACK EnumPixelFormatsCallback(LPDDPIXELFORMAT lpDDPixFmt, LPVOID lpContext);
int CanDeviceTexture(DEVICE_3D_INFO *info, LPDIRECTDRAW4 lpDD4);
LPDIRECTDRAW GetDirectDrawInterface(GUID *guid, BOOL fullscreen);
LPDIRECTDRAWSURFACE4 CreateDirectDrawSurface(LPDIRECTDRAW4 lpDD4, DDSURFACEDESC2 *ddsd);
LPDIRECTDRAW4 GetDirectDraw4Interface(LPDIRECTDRAW lpDD1);
LPDIRECT3D3 GetDirect3D3Interface(LPDIRECTDRAW4 lpDD4);
LPDIRECT3DDEVICE3 GetDirect3DDevice3Interface(LPDIRECT3D3 lpD3D3, LPGUID guid, LPDIRECTDRAWSURFACE4 lpSurf);
LPDIRECT3DVIEWPORT3 CreateDirect3DViewport(LPDIRECT3D3 lpD3D3);
BOOL AddDirect3DViewport(LPDIRECT3DDEVICE3 lpD3Ddev, LPDIRECT3DVIEWPORT3 lpView, int width, int height);
int ClearDirect3DViewport(LPDIRECT3DVIEWPORT3 lpView, float z);
int ClearDirectDrawSurface(LPDIRECTDRAWSURFACE4 lpSurf, GrColor_t color);
int SetDirectDrawDisplayMode(LPDIRECTDRAW4 lpDD4, DWORD width, DWORD height);
int CreateFullscreenRenderer(void);
BOOL CreateWindowedRenderer(DWORD width, DWORD height);
LPDIRECTDRAWCLIPPER CreateDirectDrawClipper(LPDIRECTDRAW lpDD1);
int AddDirectDrawClipper(LPDIRECTDRAWSURFACE4 lpSurf, LPDIRECTDRAWCLIPPER lpClip);
void RestoreSurfaces(void);
void RestoreDirectDrawDisplayMode(void);
int CreateZSurface(LPDIRECTDRAWSURFACE4 lpSurf, DWORD width, DWORD height);
int AttachDirectDrawSurface(LPDIRECTDRAWSURFACE4 lpSurf, LPDIRECTDRAWSURFACE4 lpAttach);
void SetupRenderStates(void);
BOOL AllocateTextures(void);
void FreeTextures(void);
void CreateTextureLookups(void);
int GetBits(DWORD mask);
int GetShift(DWORD mask);
BOOL Create8BitTextures(int surface);
LPDIRECT3DTEXTURE2 GetDirect3DTexture2Interface(LPDIRECTDRAWSURFACE4 lpSurf);
void ReleaseTexture(int texture);
BOOL AddPalette(int texture);
void StartD3DRender(void);
void EndD3DRender(void);
void FlushBatchedPrims(void);
void SetEmulatedTexture(int tset);
BOOL LoadMipmap8(void *image, int surface);
BOOL LoadMipmap16(void *image, int surface);
BOOL Create16BitTexture(int surface);
int GetEmulatedTexture(int tset, EMULTEXTURE **emultexture);
int AttachEmulatedTexture(EMULTEXTURE *emultexture, unsigned char *data, int palette, char *palette_data);
int CreateEmulatedTexture(int tset, unsigned char *data, int palette, char *palette_data);
int CreateEmulatedSurface(EMULTEXTURE *emultexture);
void LoadEmulatedMipmap8(EMULTEXTURE *emultexture8, void *data);
void LoadEmulatedMipmap16(EMULTEXTURE *emultexture8, void *data);
void BlitMipmap8(unsigned char *src, char *palette, unsigned short *dst, int pitch);
static void CopyMem_glide2x(void *dest, void *src, unsigned int count);
void MAKE_VERTEX(const GrVertex *gv, D3DTLVERTEX *dv);
void MAKE_VERTEX_REAL3D(const GrVertex *gv, D3DTLVERTEX *dv);

//------------------------------------------------------------------------------------------
// initialised data (0x551068)

int current_display_depth = 16;
int current_palette = -1;
int emulated_texture = -1;
int alphacompare_mode = D3DCMP_ALWAYS;
float d3d_screen_scale = 1.0f;
float d3d_screen_scale64 = 64.0f;
static GrCombineOther_t last_colorcombine_mode = -1;
static GrAlphaBlendFnc_t last_alphablend_sf = -1;
static GrAlphaBlendFnc_t last_alphablend_df = -1;
static GrCombineLocal_t last_alphacombine_local = -1;
static GrCombineOther_t last_alphacombine_other = -1;
static GrCmpFnc_t last_alphatest_function = -1;		// NOTE: unnamed in IDA (dword_551094), used by d3dgl_AlphaTestFunction
static GrCmpFnc_t last_depthbufferfunction_mode = -1;
static GrDepthBufferMode_t last_depthbuffer_mode = -1;
// NOTE: 0x5510a0..0x5510ac are function-local statics of MAKE_VERTEX / MAKE_VERTEX_REAL3D

//------------------------------------------------------------------------------------------
// uninitialised data (0x602a30, 0xaac638)
// NOTE: 0x602a30 starts this file's block (IDA mark); the 0xaac6xx part is in the common area and
// unmarked - defined here as this file is its only user

char virtual_lfb[4096];		// d3dgl_LfbLock hands it out when the surface can't be locked
PALETTEENTRY pal[256];
unsigned short texlookup16[65536];
int vbcount[MAX_TEXTURES + 1];
static int unknown_623EB4;			// NOTE: no references
GLIDETEXTURE textures[MAX_TEXTURES];
D3DTLVERTEX vb[MAX_TEXTURES + 1][MAX_VERTICES];

DEVICE_INFO_THING *first_3d_device;
DEVICE_INFO_THING *last_3d_device;
DEVICE_3D_INFO *current_3d_device;
LPDIRECTDRAW lpDDraw;
LPDIRECTDRAW4 lpDD;
LPDIRECT3D3 lpD3D;
LPDIRECT3DDEVICE3 lpD3DDev;
LPDIRECT3DVIEWPORT3 lpViewport;
LPDIRECTDRAWSURFACE4 lpPrimarySurface;
LPDIRECTDRAWSURFACE4 lpRenderSurface;
LPDIRECTDRAWSURFACE4 ZSurface;
LPDIRECTDRAWCLIPPER lpClipper;
LPDIRECT3DTEXTURE2 last_texture;
float DepthBias;
EMULTEXTURE *emulated_textures;
static int rbits;
static int rshift;
static int gbits;
static int gshift;
static int bbits;
static int bshift;
static int abits;
static int ashift;
int glide_width;
int glide_height;
int glide_clipx;
int glide_clipy;
int glide_clipw;
int glide_cliph;
HWND glide_window;
// NOTE: static; gfxinit.c has its own global char glide_active
static int glide_active;
BOOL glide_fullscreen;
int textures_allocated;
int d3d_device;
int surface_locked;
int depth_masked;
int colour_masked;
int device_flags;
int alphacompare_changed;
static int current_tset;
static int last_tset;
int no_8bit_support;
int emulate_8bit;
int triple_buffering;
int checking_display_devices;

// [0x41d360] [D]
// Finds the 3D devices: non-display DirectDraw drivers first, then secondary display drivers.
int Init3DDevices(void)
{
	int old_num_devices;

	OutputDebugMessage(NULL);

	checking_display_devices = 0;
	DirectDrawEnumerateEx(EnumDevicesCallback, NULL, DDENUM_NONDISPLAYDEVICES);

	checking_display_devices = 1;
	DirectDrawEnumerateEx(EnumDevicesCallback, NULL, DDENUM_ATTACHEDSECONDARYDEVICES | DDENUM_DETACHEDSECONDARYDEVICES);

	old_num_devices = num_3d_devices;
	Find3DDevices();

	return num_3d_devices != old_num_devices;
}

// [0x41d3c3] [D]
int UseD3DDevice(int device, int no8BitSupport, int tripleBuffering)
{
	if (device < 0 || device >= num_3d_devices)
		return 0;

	d3d_device = device;

	grAADrawLine = d3dgl_AADrawLine;
	grAADrawPoint = d3dgl_AADrawPoint;
	grAADrawPolygon = d3dgl_AADrawPolygon;
	grAADrawPolygonVertexList = d3dgl_AADrawPolygonVertexList;
	grAADrawTriangle = d3dgl_AADrawTriangle;
	grAlphaBlendFunction = d3dgl_AlphaBlendFunction;
	grAlphaCombine = d3dgl_AlphaCombine;
	grAlphaControlsITRGBLighting = d3dgl_AlphaControlsITRGBLighting;
	grAlphaTestFunction = d3dgl_AlphaTestFunction;
	grAlphaTestReferenceValue = d3dgl_AlphaTestReferenceValue;
	grBufferClear = d3dgl_BufferClear;
	grBufferNumPending = d3dgl_BufferNumPending;
	grBufferSwap = d3dgl_BufferSwap;
	grCheckForRoom = d3dgl_CheckForRoom;
	grChromakeyMode = d3dgl_ChromakeyMode;
	grChromakeyValue = d3dgl_ChromakeyValue;
	grClipWindow = d3dgl_ClipWindow;
	grColorCombine = d3dgl_ColorCombine;
	grColorMask = d3dgl_ColorMask;
	grConstantColorValue4 = d3dgl_ConstantColorValue4;
	grConstantColorValue = d3dgl_ConstantColorValue;
	grCullMode = d3dgl_CullMode;
	grDepthBiasLevel = d3dgl_DepthBiasLevel;
	grDepthBufferFunction = d3dgl_DepthBufferFunction;
	grDepthBufferMode = d3dgl_DepthBufferMode;
	grDepthMask = d3dgl_DepthMask;
	grDisableAllEffects = d3dgl_DisableAllEffects;
	grDitherMode = d3dgl_DitherMode;
	grDrawLine = d3dgl_DrawLine;
	grDrawPlanarPolygon = d3dgl_DrawPlanarPolygon;
	grDrawPlanarPolygonVertexList = d3dgl_DrawPlanarPolygonVertexList;
	grDrawPoint = d3dgl_DrawPoint;
	grDrawPolygon = d3dgl_DrawPolygon;
	grDrawPolygonVertexList = d3dgl_DrawPolygonVertexList;
	grDrawTriangle = d3dgl_DrawTriangle;
	grFogColorValue = d3dgl_FogColorValue;
	grFogMode = d3dgl_FogMode;
	grFogTable = d3dgl_FogTable;
	grGammaCorrectionValue = d3dgl_GammaCorrectionValue;
	grGlideGetState = d3dgl_GlideGetState;
	grGlideGetVersion = d3dgl_GlideGetVersion;
	grGlideSetState = d3dgl_GlideSetState;
	grGlideShamelessPlug = d3dgl_GlideShamelessPlug;
	grHints = d3dgl_Hints;
	grLfbConstantAlpha = d3dgl_LfbConstantAlpha;
	grLfbConstantDepth = d3dgl_LfbConstantDepth;
	gr_LfbLock = d3dgl_LfbLock;
	grLfbReadRegion = d3dgl_LfbReadRegion;
	gr_LfbUnlock = d3dgl_LfbUnlock;
	grLfbWriteColorFormat = d3dgl_LfbWriteColorFormat;
	grLfbWriteColorSwizzle = d3dgl_LfbWriteColorSwizzle;
	grLfbWriteRegion = d3dgl_LfbWriteRegion;
	grRenderBuffer = d3dgl_RenderBuffer;
	grResetTriStats = d3dgl_ResetTriStats;
	grSplash = d3dgl_Splash;
	grSstIdle = d3dgl_SstIdle;
	grSstIsBusy = d3dgl_SstIsBusy;
	grSstOrigin = d3dgl_SstOrigin;
	grSstPerfStats = d3dgl_SstPerfStats;
	grSstQueryBoards = d3dgl_SstQueryBoards;
	grSstResetPerfStats = d3dgl_SstResetPerfStats;
	grSstScreenHeight = d3dgl_SstScreenHeight;
	grSstScreenWidth = d3dgl_SstScreenWidth;
	grSstSelect = d3dgl_SstSelect;
	grSstStatus = d3dgl_SstStatus;
	grSstVRetraceOn = d3dgl_SstVRetraceOn;
	grSstVideoLine = d3dgl_SstVideoLine;
	grTexCalcMemRequired = d3dgl_TexCalcMemRequired;
	grTexClampMode = d3dgl_TexClampMode;
	grTexCombine = d3dgl_TexCombine;
	grTexCombineFunction = d3dgl_TexCombineFunction;
	grTexDetailControl = d3dgl_TexDetailControl;
	grTexDownloadMipMap = d3dgl_TexDownloadMipMap;
	grTexDownloadMipMapLevel = d3dgl_TexDownloadMipMapLevel;
	grTexDownloadMipMapLevelPartial = d3dgl_TexDownloadMipMapLevelPartial;
	grTexDownloadTable = d3dgl_TexDownloadTable;
	grTexDownloadTablePartial = d3dgl_TexDownloadTablePartial;
	grTexFilterMode = d3dgl_TexFilterMode;
	grTexLodBiasValue = d3dgl_TexLodBiasValue;
	grTexMaxAddress = d3dgl_TexMaxAddress;
	grTexMinAddress = d3dgl_TexMinAddress;
	grTexMipMapMode = d3dgl_TexMipMapMode;
	grTexMultibase = d3dgl_TexMultibase;
	grTexMultibaseAddress = d3dgl_TexMultibaseAddress;
	grTexNCCTable = d3dgl_TexNCCTable;
	grTexSource = d3dgl_TexSource;
	grTexTextureMemRequired = d3dgl_TexTextureMemRequired;
	grTriStats = d3dgl_TriStats;
	gu3dfGetInfo = d3dgu_3dfGetInfo;
	gu3dfLoad = d3dgu_3dfLoad;
	guAADrawTriangleWithClip = d3dgu_AADrawTriangleWithClip;
	guAlphaSource = d3dgu_AlphaSource;
	guColorCombineFunction = d3dgu_ColorCombineFunction;
	guDrawPolygonVertexListWithClip = d3dgu_DrawPolygonVertexListWithClip;
	guDrawTriangleWithClip = d3dgu_DrawTriangleWithClip;
	guEncodeRLE16 = d3dgu_EncodeRLE16;
	guEndianSwapBytes = d3dgu_EndianSwapBytes;
	guEndianSwapWords = d3dgu_EndianSwapWords;
	guFogGenerateExp2 = d3dgu_FogGenerateExp2;
	guFogGenerateExp = d3dgu_FogGenerateExp;
	guFogGenerateLinear = d3dgu_FogGenerateLinear;
	guFogTableIndexToW = d3dgu_FogTableIndexToW;
	guTexAllocateMemory = d3dgu_TexAllocateMemory;
	guTexChangeAttributes = d3dgu_TexChangeAttributes;
	guTexCombineFunction = d3dgu_TexCombineFunction;
	guTexCreateColorMipMap = d3dgu_TexCreateColorMipMap;
	gu_TexDownloadMipMap = d3dgu_TexDownloadMipMap;
	guTexDownloadMipMapLevel = d3dgu_TexDownloadMipMapLevel;
	guTexGetCurrentMipMap = d3dgu_TexGetCurrentMipMap;
	guTexGetMipMapInfo = d3dgu_TexGetMipMapInfo;
	guTexMemQueryAvail = d3dgu_TexMemQueryAvail;
	guTexMemReset = d3dgu_TexMemReset;
	guTexSource = d3dgu_TexSource;
	gr_GlideInit = d3dgl_GlideInit;
	gr_GlideShutdown = d3dgl_GlideShutdown;
	gr_SstQueryHardware = d3dgl_SstQueryHardware;
	gr_SstWinOpen = d3dgl_SstWinOpen;
	gr_SstWinClose = d3dgl_SstWinClose;
	gr_SstControl = d3dgl_SstControl;

	gDrawLine = d3dgl_DrawLine_REAL3D;
	gDrawPolygonVertexList = d3dgl_DrawPolygonVertexList_REAL3D;
	gDrawTriangle = d3dgl_DrawTriangle_REAL3D;
	gDrawTriangleWithClip = d3dgl_DrawTriangleWithClip_REAL3D;

	no_8bit_support = no8BitSupport;
	triple_buffering = tripleBuffering;

	return 1;
}

// [0x41d8d2] [D]
// Locks a buffer and adds its pixel format (component widths and positions) to info.
// NOTE: IDA shows a __usercall with a garbage ebx argument; the real arguments are (buffer, info)
FxBool d3d_LfbLock(GrBuffer_t buffer, LFB_INFO *info)
{
	HRESULT res;
	DDPIXELFORMAT ddpf;

	if (d3dgl_LfbLock(GR_LFB_READ_ONLY, buffer, GR_LFBWRITEMODE_565, GR_ORIGIN_UPPER_LEFT, FXFALSE, &info->info) != FXTRUE)
		return FXFALSE;

	memset(&ddpf, 0, sizeof(ddpf));
	ddpf.dwSize = sizeof(DDPIXELFORMAT);

	if (buffer == GR_BUFFER_FRONTBUFFER)
		res = lpPrimarySurface->lpVtbl->GetPixelFormat(lpPrimarySurface, &ddpf);
	else if (buffer == GR_BUFFER_BACKBUFFER)
		res = lpRenderSurface->lpVtbl->GetPixelFormat(lpRenderSurface, &ddpf);
	else
		res = DDERR_CANTLOCKSURFACE;

	if (res != DD_OK)
	{
		d3dgl_LfbUnlock(GR_LFB_READ_ONLY, buffer);
		return FXFALSE;
	}

	info->depth = (unsigned char)ddpf.dwRGBBitCount;
	info->rbits = GetBits(ddpf.dwRBitMask);
	info->rshift = GetShift(ddpf.dwRBitMask);
	info->gbits = GetBits(ddpf.dwGBitMask);
	info->gshift = GetShift(ddpf.dwGBitMask);
	info->bbits = GetBits(ddpf.dwBBitMask);
	info->bshift = GetShift(ddpf.dwBBitMask);

	return FXTRUE;
}

// [0x41d9f3] [D]
// Palette download with the palette number: the emulated 8 bit textures pick their 16 bit copy
// by it (SetEmulatedTexture); a device with 8 bit textures gets the palette itself.
void d3d_TexDownloadTable(GrChipID_t tmu, GrTexTable_t type, void *data, int palette)
{
	FlushBatchedPrims();

	current_palette = palette;

	if (!emulate_8bit)
		d3dgl_TexDownloadTable(tmu, type, data);
}

// [0x41da1f] [D]
// Glide texture download with the texture set: 8 bit textures go to the emulated (16 bit) cache
// when the device can't do 8 bit.
void d3d_TexDownloadMipMap(GrChipID_t tmu, FxU32 startAddress, FxU32 evenOdd, GrTexInfo *info, int tset)
{
	EMULTEXTURE *emultexture;
	unsigned int surface;

	FlushBatchedPrims();

	if (emulate_8bit && info->format == GR_TEXFMT_P_8)
	{
		if (GetEmulatedTexture(tset, &emultexture))
		{
			surface = HIWORD(startAddress);

			if (surface < MAX_TEXTURES)
			{
				textures[surface].emulated = 1;
				textures[surface].palette2 = -1;
				textures[surface].emultexture = emultexture;
				textures[surface].texture2 = NULL;
			}
		}
		else
		{
			OutputDebugMessage("ERROR: 8Bit texture not marked as emulated");
		}
	}
	else
	{
		d3dgl_TexDownloadMipMap(tmu, startAddress, evenOdd, info);
	}
}

// [0x41daca] [D]
// New data for a texture set already in texture memory: the 16 bit copies of an emulated 8 bit
// set are converted again.
void d3d_TexDownload8(GrChipID_t tmu, FxU32 startAddress, FxU32 evenOdd, GrTexInfo *info, int tset)
{
	EMULTEXTURE *emultexture8;

	FlushBatchedPrims();

	if (info->format == GR_TEXFMT_P_8 && GetEmulatedTexture(tset, &emultexture8))
		LoadEmulatedMipmap8(emultexture8, info->data);
	else
		d3dgl_TexDownloadMipMap(tmu, startAddress, evenOdd, info);
}

// [0x41db1e] [D]
// The 16 bit copy of an 8 bit texture set with one palette (emulated 8 bit textures only): a new
// texture set, or one more palette of a known one.
int d3d_CreateEmulatedTexture(int tset, unsigned char *data, int palette, char *palette_data)
{
	EMULTEXTURE *emultexture;

	if (!emulate_8bit)
		return 1;

	for (emultexture = emulated_textures; emultexture; emultexture = emultexture->prev)
	{
		if (emultexture->tset == tset)
			return AttachEmulatedTexture(emultexture, data, palette, palette_data);
	}

	return CreateEmulatedTexture(tset, data, palette, palette_data);
}

// [0x41db8d] [D]
// emulated_textures: a list (prev) of texture sets, each a list (next) of palette copies.
void d3d_ReleaseEmulatedTextures(void)
{
	EMULTEXTURE *set;
	EMULTEXTURE *copy;
	EMULTEXTURE *next;
	EMULTEXTURE *prev;

	for (set = emulated_textures; set; set = prev)
	{
		OutputDebugMessage("Releasing emulated texture set");

		prev = set->prev;

		for (copy = set->next; copy; copy = next)
		{
			OutputDebugMessage("Releasing emulated texture");

			next = copy->next;

			if (copy->texture)
			{
				if (last_texture == copy->texture)
					last_texture = NULL;

				copy->texture->lpVtbl->Release(copy->texture);
				copy->texture = NULL;
			}

			if (copy->surface)
			{
				copy->surface->lpVtbl->Release(copy->surface);
				copy->surface = NULL;
			}

			free(copy);
		}

		if (set->texture)
		{
			if (last_texture == set->texture)
				last_texture = NULL;

			set->texture->lpVtbl->Release(set->texture);
			set->texture = NULL;
		}

		if (set->surface)
		{
			set->surface->lpVtbl->Release(set->surface);
			set->surface = NULL;
		}

		free(set);
	}

	emulated_textures = NULL;
	current_palette = -1;
	emulated_texture = -1;
}

// [0x41dcfc] [D]
void d3d_GetHWSettings(HARDWARE_SETTINGS *settings)
{
	settings->rbits = rbits;
	settings->rshift = rshift;
	settings->gbits = gbits;
	settings->gshift = gshift;
	settings->bbits = bbits;
	settings->bshift = bshift;
	settings->abits = abits;
	settings->ashift = ashift;
	settings->emulate_8bit = emulate_8bit;
	settings->texlookup16 = texlookup16;
	settings->device_flags = device_flags;
}

// [0x41dd7f] [D]
// New 16 bit data for the copies of an emulated texture set with the current palette.
void d3d_TexDownload16(int tset, void *data)
{
	EMULTEXTURE *emultexture8;

	FlushBatchedPrims();

	if (GetEmulatedTexture(tset, &emultexture8))
		LoadEmulatedMipmap16(emultexture8, data);
}

// [0x41ddb0] [D]
// NOTE: IDA shows an int return (the float left in eax); nothing uses it
void d3d_SetScreenScale(float scale)
{
	d3d_screen_scale = scale;
	d3d_screen_scale64 = scale * 64.0f;
}

// [0x41ddcc] [D]
void FX_CALL d3dgl_GlideInit(void)
{
	int i;

	// grey ramp palette for the 8 bit textures
	for (i = 0; i < 256; i++)
	{
		pal[i].peRed = i;
		pal[i].peGreen = i;
		pal[i].peBlue = i;
		pal[i].peFlags = PC_EXPLICIT;
	}

	DepthBias = 0.0f;
}

// [0x41de2d] [D]
void FX_CALL d3dgl_GlideShutdown(void)
{
	d3dgl_SstWinClose();
}

// [0x41de37] [D]
// Reports a single Voodoo board with one TMU.
FxBool FX_CALL d3dgl_SstQueryHardware(GrHwConfiguration *hwconfig)
{
	hwconfig->num_sst = 1;
	hwconfig->SSTs[0].type = GR_SSTTYPE_VOODOO;
	hwconfig->SSTs[0].sstBoard.VoodooConfig.fbRam = 2;
	hwconfig->SSTs[0].sstBoard.VoodooConfig.fbiRev = 2;
	hwconfig->SSTs[0].sstBoard.VoodooConfig.nTexelfx = 1;
	hwconfig->SSTs[0].sstBoard.VoodooConfig.sliDetect = FXFALSE;
	hwconfig->SSTs[0].sstBoard.VoodooConfig.tmuConfig[0].tmuRev = 1;
	hwconfig->SSTs[0].sstBoard.VoodooConfig.tmuConfig[0].tmuRam = 2;

	return FXTRUE;
}

// [0x41deb5] [D]
// Maps the Glide blend modes the game uses onto what the device can do.
void FX_CALL d3dgl_AlphaBlendFunction(GrAlphaBlendFnc_t rgb_sf, GrAlphaBlendFnc_t rgb_df, GrAlphaBlendFnc_t alpha_sf, GrAlphaBlendFnc_t alpha_df)
{
	if (rgb_sf == last_alphablend_sf && rgb_df == last_alphablend_df)
		return;

	last_alphablend_sf = rgb_sf;
	last_alphablend_df = rgb_df;

	FlushBatchedPrims();

	if (alphacompare_changed)
	{
		lpD3DDev->lpVtbl->SetRenderState(lpD3DDev, D3DRENDERSTATE_ALPHAFUNC, alphacompare_mode);
		lpD3DDev->lpVtbl->SetTextureStageState(lpD3DDev, 0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		alphacompare_changed = 0;
	}

	if (rgb_sf == GR_BLEND_SRC_COLOR)
	{
		if (device_flags & DEVFLAG_SRC_DESTCOLOR)
		{
			lpD3DDev->lpVtbl->SetRenderState(lpD3DDev, D3DRENDERSTATE_SRCBLEND, D3DBLEND_DESTCOLOR);
			lpD3DDev->lpVtbl->SetRenderState(lpD3DDev, D3DRENDERSTATE_DESTBLEND, D3DBLEND_ONE);
			lpD3DDev->lpVtbl->SetRenderState(lpD3DDev, D3DRENDERSTATE_ALPHABLENDENABLE, TRUE);
			lpD3DDev->lpVtbl->SetRenderState(lpD3DDev, D3DRENDERSTATE_ALPHATESTENABLE, TRUE);
		}
		else
		{
			rgb_sf = GR_BLEND_ONE;
		}
	}

	if (rgb_sf == GR_BLEND_ONE)
	{
		if (rgb_df == GR_BLEND_ZERO)
		{
			lpD3DDev->lpVtbl->SetRenderState(lpD3DDev, D3DRENDERSTATE_ALPHABLENDENABLE, FALSE);
			return;
		}

		if (rgb_df == GR_BLEND_ONE)
		{
			// additive
			if (device_flags & DEVFLAG_DEST_ONE)
			{
				lpD3DDev->lpVtbl->SetRenderState(lpD3DDev, D3DRENDERSTATE_SRCBLEND, D3DBLEND_ONE);
				lpD3DDev->lpVtbl->SetRenderState(lpD3DDev, D3DRENDERSTATE_DESTBLEND, D3DBLEND_ONE);
				lpD3DDev->lpVtbl->SetRenderState(lpD3DDev, D3DRENDERSTATE_ALPHABLENDENABLE, TRUE);
				lpD3DDev->lpVtbl->SetRenderState(lpD3DDev, D3DRENDERSTATE_ALPHATESTENABLE, TRUE);
			}
			else
			{
				lpD3DDev->lpVtbl->SetRenderState(lpD3DDev, D3DRENDERSTATE_ALPHABLENDENABLE, FALSE);
			}
		}
	}
	else if (rgb_sf == GR_BLEND_ZERO)
	{
		if (rgb_df == GR_BLEND_ONE_MINUS_SRC_ALPHA)
		{
			if (!(device_flags & DEVFLAG_DEST_SRCALPHA))
			{
				lpD3DDev->lpVtbl->SetRenderState(lpD3DDev, D3DRENDERSTATE_ALPHABLENDENABLE, FALSE);
				return;
			}

			if (device_flags & DEVFLAG_ALWAYS)
			{
				lpD3DDev->lpVtbl->SetTextureStageState(lpD3DDev, 0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
				lpD3DDev->lpVtbl->SetRenderState(lpD3DDev, D3DRENDERSTATE_ALPHAFUNC, D3DCMP_LESS);
				alphacompare_changed = 1;

				lpD3DDev->lpVtbl->SetRenderState(lpD3DDev, D3DRENDERSTATE_SRCBLEND, D3DBLEND_ZERO);
				lpD3DDev->lpVtbl->SetRenderState(lpD3DDev, D3DRENDERSTATE_DESTBLEND, D3DBLEND_INVSRCCOLOR);
			}
			else
			{
				lpD3DDev->lpVtbl->SetRenderState(lpD3DDev, D3DRENDERSTATE_SRCBLEND, D3DBLEND_ZERO);
				lpD3DDev->lpVtbl->SetRenderState(lpD3DDev, D3DRENDERSTATE_DESTBLEND, D3DBLEND_SRCALPHA);
			}
		}
		else if (device_flags & DEVFLAG_DEST_INVSRCCOLOR)
		{
			lpD3DDev->lpVtbl->SetRenderState(lpD3DDev, D3DRENDERSTATE_SRCBLEND, D3DBLEND_ZERO);
			lpD3DDev->lpVtbl->SetRenderState(lpD3DDev, D3DRENDERSTATE_DESTBLEND, D3DBLEND_INVSRCCOLOR);
		}
		else
		{
			lpD3DDev->lpVtbl->SetRenderState(lpD3DDev, D3DRENDERSTATE_SRCBLEND, D3DBLEND_ZERO);
			lpD3DDev->lpVtbl->SetRenderState(lpD3DDev, D3DRENDERSTATE_DESTBLEND, D3DBLEND_ONE);
		}

		lpD3DDev->lpVtbl->SetRenderState(lpD3DDev, D3DRENDERSTATE_ALPHABLENDENABLE, TRUE);
		lpD3DDev->lpVtbl->SetRenderState(lpD3DDev, D3DRENDERSTATE_ALPHATESTENABLE, TRUE);
	}
}

// [0x41e21a] [D]
// Only the alpha test follows: on unless the alpha is the constant colour.
void FX_CALL d3dgl_AlphaCombine(GrCombineFunction_t function, GrCombineFactor_t factor, GrCombineLocal_t local, GrCombineOther_t other, FxBool invert)
{
	if (local == last_alphacombine_local && other == last_alphacombine_other)
		return;

	last_alphacombine_local = local;
	last_alphacombine_other = other;

	FlushBatchedPrims();

	if (other == GR_COMBINE_OTHER_CONSTANT)
		lpD3DDev->lpVtbl->SetRenderState(lpD3DDev, D3DRENDERSTATE_ALPHATESTENABLE, FALSE);
	else
		lpD3DDev->lpVtbl->SetRenderState(lpD3DDev, D3DRENDERSTATE_ALPHATESTENABLE, TRUE);
}

// [0x41e2a8] [D]
// NOTE: every function other than GR_CMP_ALWAYS becomes D3DCMP_LESS, as in the original
void FX_CALL d3dgl_AlphaTestFunction(GrCmpFnc_t function)
{
	if (function == last_alphatest_function)
		return;

	last_alphatest_function = function;

	FlushBatchedPrims();

	if (function == GR_CMP_ALWAYS)
		alphacompare_mode = D3DCMP_ALWAYS;
	else
		alphacompare_mode = D3DCMP_LESS;

	lpD3DDev->lpVtbl->SetRenderState(lpD3DDev, D3DRENDERSTATE_ALPHAFUNC, alphacompare_mode);
}

// [0x41e300] [D]
void FX_CALL d3dgl_AlphaTestReferenceValue(GrAlpha_t value)
{
	lpD3DDev->lpVtbl->SetRenderState(lpD3DDev, D3DRENDERSTATE_ALPHAREF, value);
}

// [0x41e329] [D]
void FX_CALL d3dgl_BufferClear(GrColor_t color, GrAlpha_t alpha, FxU16 depth)
{
	if (glide_active)
	{
		if (!colour_masked)
			ClearDirectDrawSurface(lpRenderSurface, color);

		if (!depth_masked)
		{
			if (depth != GR_WDEPTHVALUE_FARTHEST && depth != 0)
				ClearDirect3DViewport(lpViewport, 0.0f);
			else
				ClearDirect3DViewport(lpViewport, 1.0f);
		}
	}
}

// [0x41e3ab] [D]
void FX_CALL d3dgl_BufferSwap(int swap_interval)
{
	RECT dst;
	RECT rect;
	DWORD flags;
	HRESULT res;

	if (glide_active)
	{
		FlushBatchedPrims();
		EndD3DRender();

		if (glide_fullscreen)
		{
			if (swap_interval)
				flags = DDFLIP_WAIT;
			else
				flags = DDFLIP_WAIT | DDFLIP_NOVSYNC;

			res = lpPrimarySurface->lpVtbl->Flip(lpPrimarySurface, NULL, flags);

			if (res == DDERR_SURFACELOST)
				RestoreSurfaces();
		}
		else
		{
			// centre the rendered image in the window client area
			GetClientRect(glide_window, &rect);

			dst = rect;

			if (rect.right > glide_width)
			{
				rect.right = glide_width;
				dst.left = (dst.right - glide_width) / 2;
				dst.right = glide_width + dst.left;
			}

			if (rect.bottom > glide_height)
			{
				rect.bottom = glide_height;
				dst.top = (dst.bottom - glide_height) / 2;
				dst.bottom = glide_height + dst.top;
			}

			ClientToScreen(glide_window, (POINT *)&dst.left);
			ClientToScreen(glide_window, (POINT *)&dst.right);

			lpClipper->lpVtbl->SetHWnd(lpClipper, 0, glide_window);

			res = lpPrimarySurface->lpVtbl->Blt(lpPrimarySurface, &dst, lpRenderSurface, &rect, 0, NULL);

			if (res != DD_OK)
				RestoreSurfaces();
		}

		StartD3DRender();
	}
}

// [0x41e51a] [D]
void StartD3DRender(void)
{
	if (glide_active)
		lpD3DDev->lpVtbl->BeginScene(lpD3DDev);
}

// [0x41e539] [D]
void EndD3DRender(void)
{
	if (glide_active)
		lpD3DDev->lpVtbl->EndScene(lpD3DDev);
}

// [0x41e566] [D]
void FX_CALL d3dgl_ChromakeyValue(GrColor_t value)
{
}

// [0x41e56d] [D]
void FX_CALL d3dgl_ClipWindow(FxU32 minx, FxU32 miny, FxU32 maxx, FxU32 maxy)
{
	D3DVIEWPORT2 vp;

	if (glide_active)
	{
		FlushBatchedPrims();

		glide_clipx = minx;
		glide_clipy = miny;
		glide_clipw = maxx - minx;
		glide_cliph = maxy - miny;

		vp.dwSize = sizeof(D3DVIEWPORT2);
		vp.dwX = minx;
		vp.dwY = miny;
		vp.dwWidth = maxx - minx;
		vp.dwHeight = maxy - miny;
		vp.dvClipX = 0.0f;
		vp.dvClipY = 0.0f;
		vp.dvClipWidth = (float)(int)(maxx - minx);
		vp.dvClipHeight = (float)(int)(maxy - miny);
		vp.dvMinZ = 0.0f;
		vp.dvMaxZ = 1.0f;

		lpViewport->lpVtbl->SetViewport2(lpViewport, &vp);
	}
}

// [0x41e632] [D]
// Only "texture or not" is emulated: untextured polygons go to the extra batch vb[MAX_TEXTURES].
void FX_CALL d3dgl_ColorCombine(GrCombineFunction_t function, GrCombineFactor_t factor, GrCombineLocal_t local, GrCombineOther_t other, FxBool invert)
{
	if (glide_active && other != last_colorcombine_mode)
	{
		last_colorcombine_mode = other;

		FlushBatchedPrims();

		if (other == GR_COMBINE_OTHER_TEXTURE)
		{
			lpD3DDev->lpVtbl->SetTextureStageState(lpD3DDev, 0, D3DTSS_COLOROP, D3DTOP_MODULATE);
			current_tset = last_tset;
		}
		else
		{
			lpD3DDev->lpVtbl->SetTextureStageState(lpD3DDev, 0, D3DTSS_COLOROP, D3DTOP_DISABLE);

			if (current_tset != MAX_TEXTURES)
			{
				last_tset = current_tset;
				current_tset = MAX_TEXTURES;
			}
		}
	}
}

// [0x41e6c7] [D]
void FX_CALL d3dgl_ColorMask(FxBool rgb, FxBool a)
{
	colour_masked = rgb != FXTRUE;
}

// [0x41e6f1] [D]
void FX_CALL d3dgl_ConstantColorValue(GrColor_t value)
{
}

// [0x41e6f8] [D]
void FX_CALL d3dgl_CullMode(GrCullMode_t mode)
{
	if (glide_active)
	{
		if (mode == GR_CULL_DISABLE)
			lpD3DDev->lpVtbl->SetRenderState(lpD3DDev, D3DRENDERSTATE_CULLMODE, D3DCULL_NONE);
		else if (mode == GR_CULL_NEGATIVE)
			lpD3DDev->lpVtbl->SetRenderState(lpD3DDev, D3DRENDERSTATE_CULLMODE, D3DCULL_CCW);
		else if (mode == GR_CULL_POSITIVE)
			lpD3DDev->lpVtbl->SetRenderState(lpD3DDev, D3DRENDERSTATE_CULLMODE, D3DCULL_CW);
	}
}

// [0x41e76d] [D]
// NOTE: Hex-Rays fails on the switch; translated from the disassembly
void FX_CALL d3dgl_DepthBufferFunction(GrCmpFnc_t function)
{
	if (function == last_depthbufferfunction_mode)
		return;

	last_depthbufferfunction_mode = function;

	FlushBatchedPrims();

	switch (function)
	{
		case GR_CMP_NEVER:
			lpD3DDev->lpVtbl->SetRenderState(lpD3DDev, D3DRENDERSTATE_ZFUNC, D3DCMP_NEVER);
			break;
		case GR_CMP_LESS:
			lpD3DDev->lpVtbl->SetRenderState(lpD3DDev, D3DRENDERSTATE_ZFUNC, D3DCMP_LESS);
			break;
		case GR_CMP_EQUAL:
			lpD3DDev->lpVtbl->SetRenderState(lpD3DDev, D3DRENDERSTATE_ZFUNC, D3DCMP_EQUAL);
			break;
		case GR_CMP_LEQUAL:
			lpD3DDev->lpVtbl->SetRenderState(lpD3DDev, D3DRENDERSTATE_ZFUNC, D3DCMP_LESSEQUAL);
			break;
		case GR_CMP_GREATER:
			lpD3DDev->lpVtbl->SetRenderState(lpD3DDev, D3DRENDERSTATE_ZFUNC, D3DCMP_GREATER);
			break;
		case GR_CMP_NOTEQUAL:
			lpD3DDev->lpVtbl->SetRenderState(lpD3DDev, D3DRENDERSTATE_ZFUNC, D3DCMP_NOTEQUAL);
			break;
		case GR_CMP_GEQUAL:
			lpD3DDev->lpVtbl->SetRenderState(lpD3DDev, D3DRENDERSTATE_ZFUNC, D3DCMP_GREATEREQUAL);
			break;
		case GR_CMP_ALWAYS:
			lpD3DDev->lpVtbl->SetRenderState(lpD3DDev, D3DRENDERSTATE_ZFUNC, D3DCMP_ALWAYS);
			break;
	}
}

// [0x41e893] [D]
void FX_CALL d3dgl_DepthBufferMode(GrDepthBufferMode_t mode)
{
	if (mode != last_depthbuffer_mode)
	{
		last_depthbuffer_mode = mode;

		FlushBatchedPrims();

		if (mode != GR_DEPTHBUFFER_DISABLE)
			lpD3DDev->lpVtbl->SetRenderState(lpD3DDev, D3DRENDERSTATE_ZENABLE, TRUE);
		else
			lpD3DDev->lpVtbl->SetRenderState(lpD3DDev, D3DRENDERSTATE_ZENABLE, FALSE);
	}
}

// [0x41e8e7] [D]
void FX_CALL d3dgl_DepthMask(FxBool mask)
{
	FlushBatchedPrims();

	if (mask == FXTRUE)
	{
		lpD3DDev->lpVtbl->SetRenderState(lpD3DDev, D3DRENDERSTATE_ZWRITEENABLE, TRUE);
		depth_masked = 0;
	}
	else
	{
		lpD3DDev->lpVtbl->SetRenderState(lpD3DDev, D3DRENDERSTATE_ZWRITEENABLE, FALSE);
		depth_masked = 1;
	}
}

// [0x41ea0f] [D]
// Batches a quad as two triangles into the current texture set's vertex buffer.
// NOTE: nverts is ignored, the game only passes quads here
void FX_CALL d3dgl_DrawPolygonVertexList(int nverts, const GrVertex vlist[])
{
	if (glide_active)
	{
		MAKE_VERTEX(&vlist[0], &vb[current_tset][vbcount[current_tset]++]);
		MAKE_VERTEX(&vlist[1], &vb[current_tset][vbcount[current_tset]++]);
		MAKE_VERTEX(&vlist[2], &vb[current_tset][vbcount[current_tset]++]);

		MAKE_VERTEX(&vlist[0], &vb[current_tset][vbcount[current_tset]++]);
		MAKE_VERTEX(&vlist[2], &vb[current_tset][vbcount[current_tset]++]);
		MAKE_VERTEX(&vlist[3], &vb[current_tset][vbcount[current_tset]++]);
	}
}

// [0x41ee2d] [D]
// Batched with the current texture.
void FX_CALL d3dgl_DrawTriangle(const GrVertex *a, const GrVertex *b, const GrVertex *c)
{
	if (!glide_active)
		return;

	MAKE_VERTEX(a, &vb[current_tset][vbcount[current_tset]++]);
	MAKE_VERTEX(b, &vb[current_tset][vbcount[current_tset]++]);
	MAKE_VERTEX(c, &vb[current_tset][vbcount[current_tset]++]);
}

// [0x41ef3a] [D]
// gDrawTriangle of the D3D renderer: as d3dgl_DrawTriangle with MAKE_VERTEX_REAL3D.
void FX_CALL d3dgl_DrawTriangle_REAL3D(const GrVertex *a, const GrVertex *b, const GrVertex *c)
{
	if (!glide_active)
		return;

	MAKE_VERTEX_REAL3D(a, &vb[current_tset][vbcount[current_tset]++]);
	MAKE_VERTEX_REAL3D(b, &vb[current_tset][vbcount[current_tset]++]);
	MAKE_VERTEX_REAL3D(c, &vb[current_tset][vbcount[current_tset]++]);
}

// [0x41f047] [D]
void FX_CALL d3dgl_FogColorValue(GrColor_t fogcolor)
{
}

// [0x41f04e] [D]
void FX_CALL d3dgl_FogMode(GrFogMode_t mode)
{
}

// [0x41f055] [D]
void FX_CALL d3dgl_FogTable(const GrFog_t ft[GR_FOG_TABLE_SIZE])
{
}

// [0x41f0a5] [D]
// NOTE: the surface stays locked until d3dgl_LfbUnlock; if it can't be locked, writes go to
// virtual_lfb (with a stride of 0)
FxBool FX_CALL d3dgl_LfbLock(GrLock_t type, GrBuffer_t buffer, GrLfbWriteMode_t writeMode, GrOriginLocation_t origin, FxBool pixelPipeline, GrLfbInfo_t *info)
{
	HRESULT res;
	DDSURFACEDESC2 ddsd;

	if (!glide_active)
		return FXFALSE;

	FlushBatchedPrims();

	EndD3DRender();

	memset(&ddsd, 0, sizeof(ddsd));
	ddsd.dwSize = sizeof(DDSURFACEDESC2);

	if (buffer == GR_BUFFER_FRONTBUFFER)
		res = lpPrimarySurface->lpVtbl->Lock(lpPrimarySurface, NULL, &ddsd, DDLOCK_WAIT, NULL);
	else if (buffer == GR_BUFFER_BACKBUFFER)
		res = lpRenderSurface->lpVtbl->Lock(lpRenderSurface, NULL, &ddsd, DDLOCK_WAIT, NULL);
	else
		return FXFALSE;

	if (res != DD_OK)
	{
		ddsd.lpSurface = virtual_lfb;
		ddsd.lPitch = 0;
	}
	else
	{
		surface_locked = 1;
	}

	info->lfbPtr = ddsd.lpSurface;
	info->strideInBytes = ddsd.lPitch;
	info->writeMode = GR_LFBWRITEMODE_555;
	info->origin = GR_ORIGIN_UPPER_LEFT;

	return FXTRUE;
}

// [0x41f19b] [D]
FxBool FX_CALL d3dgl_LfbUnlock(GrLock_t type, GrBuffer_t buffer)
{
	if (!glide_active)
		return FXFALSE;

	if (surface_locked)
	{
		if (buffer == GR_BUFFER_FRONTBUFFER)
			lpPrimarySurface->lpVtbl->Unlock(lpPrimarySurface, NULL);
		else if (buffer == GR_BUFFER_BACKBUFFER)
			lpRenderSurface->lpVtbl->Unlock(lpRenderSurface, NULL);
		else
			return FXFALSE;

		surface_locked = 0;
	}

	StartD3DRender();
	return FXTRUE;
}

// [0x41f246] [D]
FxBool FX_CALL d3dgl_SstControl(FxU32 code)
{
	return FXTRUE;
}

// [0x41f295] [D]
void FX_CALL d3dgl_SstSelect(int which_sst)
{
}

// [0x41f2b1] [D] [A]
// NOTE: Hex-Rays fails on the resolution switch; translated from the disassembly
FxBool FX_CALL d3dgl_SstWinOpen(FxU32 hWnd, GrScreenResolution_t screen_resolution, GrScreenRefresh_t refresh_rate,
	GrColorFormat_t color_format, GrOriginLocation_t origin_location, int nColBuffers, int nAuxBuffers)
{
	HRESULT ddrval;
	RECT rect;
	int width;
	int height;
	DWORD srcBlendCaps;
	DWORD destBlendCaps;

	if (glide_active)
	{
		OutputDebugMessage("WARNING: Hardware already allocated... closing it down first");
		d3dgl_SstWinClose();
	}

	glide_window = (HWND)hWnd;

	if (!glide_window)
		glide_window = GetActiveWindow();

	if (screen_resolution != GR_RESOLUTION_NONE)
	{
		// NOTE: [A] windowed mode doesn't maximise the window
		if (!gWindowed)
			ShowWindow(glide_window, SW_MAXIMIZE);

		glide_fullscreen = TRUE;
	}
	else
	{
		glide_fullscreen = FALSE;
	}

	current_3d_device = devices_3d[d3d_device].field_48;

	lpDDraw = GetDirectDrawInterface(current_3d_device->field_00, glide_fullscreen);

	if (!lpDDraw)
	{
		d3dgl_SstWinClose();
		return FXFALSE;
	}

	lpDD = GetDirectDraw4Interface(lpDDraw);

	if (!lpDD)
		return FXFALSE;

	lpD3D = GetDirect3D3Interface(lpDD);

	if (!lpD3D)
		return FXFALSE;

	if (glide_fullscreen)
	{
		ddrval = lpDD->lpVtbl->SetCooperativeLevel(lpDD, glide_window, DDSCL_FULLSCREEN | DDSCL_EXCLUSIVE);

		if (ddrval != DD_OK)
		{
			d3dgl_SstWinClose();
			return FXFALSE;
		}

		switch (screen_resolution)
		{
			case GR_RESOLUTION_320x240:
				width = 320;
				height = 240;
				break;

			case GR_RESOLUTION_512x384:
				width = 512;
				height = 384;
				break;

			case GR_RESOLUTION_640x480:
				width = 640;
				height = 480;
				break;

			case GR_RESOLUTION_800x600:
				width = 800;
				height = 600;
				break;

			case GR_RESOLUTION_1024x768:
				width = 1024;
				height = 768;
				break;

			case GR_RESOLUTION_1280x1024:
				width = 1280;
				height = 1024;
				break;

			case GR_RESOLUTION_1600x1200:
				width = 1600;
				height = 1200;
				break;

			default:
				d3dgl_SstWinClose();
				return FXFALSE;
		}

		if (!SetDirectDrawDisplayMode(lpDD, width, height))
		{
			d3dgl_SstWinClose();
			return FXFALSE;
		}

		if (!CreateFullscreenRenderer())
		{
			d3dgl_SstWinClose();
			return FXFALSE;
		}
	}
	else
	{
		ddrval = lpDD->lpVtbl->SetCooperativeLevel(lpDD, glide_window, DDSCL_NORMAL);

		if (ddrval != DD_OK)
		{
			d3dgl_SstWinClose();
			return FXFALSE;
		}

		GetClientRect(glide_window, &rect);

		width = rect.right;
		height = rect.bottom;

		if (!CreateWindowedRenderer(width, height))
		{
			d3dgl_SstWinClose();
			return FXFALSE;
		}
	}

	if (!CreateZSurface(lpRenderSurface, width, height))
	{
		d3dgl_SstWinClose();
		return FXFALSE;
	}

	lpD3DDev = GetDirect3DDevice3Interface(lpD3D, current_3d_device->field_14, lpRenderSurface);

	if (!lpD3DDev)
	{
		d3dgl_SstWinClose();
		return FXFALSE;
	}

	// what the hardware can blend
	srcBlendCaps = current_3d_device->HWDesc.dpcTriCaps.dwSrcBlendCaps;
	destBlendCaps = current_3d_device->HWDesc.dpcTriCaps.dwDestBlendCaps;

	device_flags = 0;

	if (destBlendCaps & D3DPBLENDCAPS_SRCALPHA)
		device_flags |= DEVFLAG_DEST_SRCALPHA;

	if (destBlendCaps & D3DPBLENDCAPS_INVSRCCOLOR)
		device_flags |= DEVFLAG_DEST_INVSRCCOLOR;

	if (destBlendCaps & D3DPBLENDCAPS_ONE)
		device_flags |= DEVFLAG_DEST_ONE;

	if (srcBlendCaps & D3DPBLENDCAPS_DESTCOLOR)
		device_flags |= DEVFLAG_SRC_DESTCOLOR;

	device_flags |= DEVFLAG_ALWAYS;

	if ((current_3d_device->HWDesc.dpcTriCaps.dwRasterCaps & D3DPRASTERCAPS_WBUFFER) &&
		(current_3d_device->HWDesc.dpcLineCaps.dwRasterCaps & D3DPRASTERCAPS_WBUFFER))
	{
		device_flags |= DEVFLAG_WBUFFER;
	}

	lpViewport = CreateDirect3DViewport(lpD3D);

	if (!lpViewport)
	{
		d3dgl_SstWinClose();
		return FXFALSE;
	}

	if (!AddDirect3DViewport(lpD3DDev, lpViewport, width, height))
	{
		d3dgl_SstWinClose();
		return FXFALSE;
	}

	if (no_8bit_support)
	{
		emulate_8bit = 1;
		OutputDebugMessage("User requested no 8bit support");
	}
	else
	{
		emulate_8bit = current_3d_device->emulate_8bit;
	}

	if (!AllocateTextures())
	{
		d3dgl_SstWinClose();
		return FXFALSE;
	}

	SetupRenderStates();

	glide_clipx = 0;
	glide_clipy = 0;
	glide_clipw = width;
	glide_cliph = height;

	glide_width = width;
	glide_height = height;

	glide_active = 1;

	grBufferClear(0, 0, GR_WDEPTHVALUE_FARTHEST);
	grBufferSwap(0);
	grBufferClear(0, 0, GR_WDEPTHVALUE_FARTHEST);
	grBufferSwap(0);

	device_guid = current_3d_device->field_00;

	return FXTRUE;
}

// [0x41f7c9] [D]
void FX_CALL d3dgl_SstWinClose(void)
{
	EndD3DRender();
	FreeTextures();

	if (lpViewport)
	{
		lpViewport->lpVtbl->Release(lpViewport);
		lpViewport = NULL;
	}

	if (lpD3DDev)
	{
		lpD3DDev->lpVtbl->Release(lpD3DDev);
		lpD3DDev = NULL;
	}

	if (lpClipper)
	{
		lpClipper->lpVtbl->Release(lpClipper);
		lpClipper = NULL;
	}

	if (ZSurface)
	{
		ZSurface->lpVtbl->Release(ZSurface);
		ZSurface = NULL;
	}

	if (lpRenderSurface)
	{
		lpRenderSurface->lpVtbl->Release(lpRenderSurface);
		lpRenderSurface = NULL;
	}

	if (lpPrimarySurface)
	{
		lpPrimarySurface->lpVtbl->Release(lpPrimarySurface);
		lpPrimarySurface = NULL;
	}

	if (lpD3D)
	{
		lpD3D->lpVtbl->Release(lpD3D);
		lpD3D = NULL;
	}

	if (lpDD)
	{
		if (glide_fullscreen)
			lpDD->lpVtbl->SetCooperativeLevel(lpDD, glide_window, DDSCL_NORMAL);

		lpDD->lpVtbl->Release(lpDD);
		lpDD = NULL;
	}

	if (lpDDraw)
	{
		lpDDraw->lpVtbl->Release(lpDDraw);
		lpDDraw = NULL;
	}

	glide_width = 0;
	glide_height = 0;
	glide_clipx = 0;
	glide_clipy = 0;
	glide_clipw = 0;
	glide_cliph = 0;
	glide_window = NULL;
	glide_active = 0;
	glide_fullscreen = FALSE;
	textures_allocated = 0;
}

// [0x41f9bc] [D]
void FX_CALL d3dgl_TexCombineFunction(GrChipID_t tmu, GrTextureCombineFnc_t fnc)
{
}

// [0x41f9ca] [D]
// Texture memory is emulated: the address selects the surface (one 256x256 surface per 64k for
// 8 bit, per 128k for 16 bit).
void FX_CALL d3dgl_TexDownloadMipMap(GrChipID_t tmu, FxU32 startAddress, FxU32 evenOdd, GrTexInfo *info)
{
	FxU32 surface;

	if (!glide_active)
		return;

	FlushBatchedPrims();

	if (info->format == GR_TEXFMT_P_8)
	{
		surface = HIWORD(startAddress);

		if (surface >= MAX_TEXTURES || info->largeLod != GR_LOD_256)
			return;

		LoadMipmap8(info->data, surface);
	}
	else
	{
		surface = 2 * (startAddress >> 17);

		if (surface >= MAX_TEXTURES || info->largeLod != GR_LOD_256)
			return;

		LoadMipmap16(info->data, surface);
	}

	textures[surface].emulated = 0;
	textures[surface].palette2 = -1;
	textures[surface].emultexture = NULL;
	textures[surface].texture2 = NULL;
}

// [0x41faa6] [D]
// The palette of the 8 bit textures (Glide 0x00RRGGBB entries).
void FX_CALL d3dgl_TexDownloadTable(GrChipID_t tmu, GrTexTable_t type, void *data)
{
	unsigned char *entry;
	int ct;

	FlushBatchedPrims();

	if (type != GR_TEXTABLE_PALETTE)
		return;

	entry = (unsigned char *)data;

	for (ct = 0; ct < 256; ct++)
	{
		pal[ct].peRed = entry[2];
		pal[ct].peGreen = entry[1];
		pal[ct].peBlue = entry[0];
		pal[ct].peFlags = PC_EXPLICIT;

		entry += 4;
	}

	lpD3DDev->lpVtbl->SetRenderState(lpD3DDev, D3DRENDERSTATE_FLUSHBATCH, 0);

	for (ct = 0; ct < MAX_TEXTURES; ct++)
	{
		if (textures[ct].palette)
		{
			textures[ct].palette->lpVtbl->SetEntries(textures[ct].palette, 0, 0, 256, pal);
			textures[ct].texture->lpVtbl->PaletteChanged(textures[ct].texture, 0, 256);
		}
	}
}

// [0x41fbd2] [D]
// NOTE: written from the disassembly (Hex-Rays loses the arguments)
void FX_CALL d3dgl_TexFilterMode(GrChipID_t tmu, GrTextureFilterMode_t minfilter_mode, GrTextureFilterMode_t magfilter_mode)
{
	FlushBatchedPrims();

	if (minfilter_mode == GR_TEXTUREFILTER_POINT_SAMPLED)
		lpD3DDev->lpVtbl->SetTextureStageState(lpD3DDev, 0, D3DTSS_MINFILTER, D3DTFN_POINT);
	else
		lpD3DDev->lpVtbl->SetTextureStageState(lpD3DDev, 0, D3DTSS_MINFILTER, D3DTFN_LINEAR);

	if (magfilter_mode == GR_TEXTUREFILTER_POINT_SAMPLED)
		lpD3DDev->lpVtbl->SetTextureStageState(lpD3DDev, 0, D3DTSS_MAGFILTER, D3DTFG_POINT);
	else
		lpD3DDev->lpVtbl->SetTextureStageState(lpD3DDev, 0, D3DTSS_MAGFILTER, D3DTFG_LINEAR);
}

// [0x41fc5d] [D]
FxU32 FX_CALL d3dgl_TexMaxAddress(GrChipID_t tmu)
{
	if (tmu != GR_TMU0)
		return 0;

	return 0x1FFFF8;	// 32 surfaces of 64k
}

// [0x41fc73] [D]
FxU32 FX_CALL d3dgl_TexMinAddress(GrChipID_t tmu)
{
	return 0;
}

// [0x41fc98] [D]
void FX_CALL d3dgl_TexSource(GrChipID_t tmu, FxU32 startAddress, FxU32 evenOdd, GrTexInfo *info)
{
	FxU32 surface;

	if (glide_active)
	{
		if (info->format == GR_TEXFMT_P_8)
			surface = HIWORD(startAddress);
		else
			surface = 2 * (startAddress >> 17);

		if (surface < MAX_TEXTURES)
		{
			current_tset = surface;
			last_tset = surface;
			last_texture = textures[surface].texture;
			emulated_texture = -1;
		}
		else
		{
			OutputDebugMessage("ERROR: Invalid texture source address");
		}
	}
}

// [0x41fd4e] [D]
// Batched with the current texture; Direct3D clips.
void FX_CALL d3dgu_DrawTriangleWithClip(const GrVertex *a, const GrVertex *b, const GrVertex *c)
{
	if (!glide_active)
		return;

	MAKE_VERTEX(a, &vb[current_tset][vbcount[current_tset]++]);
	MAKE_VERTEX(b, &vb[current_tset][vbcount[current_tset]++]);
	MAKE_VERTEX(c, &vb[current_tset][vbcount[current_tset]++]);
}

// [0x41fe5b] [D]
// gDrawTriangleWithClip of the D3D renderer: the same, Direct3D clips.
void FX_CALL d3dgl_DrawTriangleWithClip_REAL3D(const GrVertex *a, const GrVertex *b, const GrVertex *c)
{
	if (!glide_active)
		return;

	MAKE_VERTEX_REAL3D(a, &vb[current_tset][vbcount[current_tset]++]);
	MAKE_VERTEX_REAL3D(b, &vb[current_tset][vbcount[current_tset]++]);
	MAKE_VERTEX_REAL3D(c, &vb[current_tset][vbcount[current_tset]++]);
}

// [0x41ff8b] [D]
void FX_CALL d3dgu_FogGenerateExp(GrFog_t fogtable[GR_FOG_TABLE_SIZE], float density)
{
}

// [0x41fffb] [D]
// DirectDrawEnumerateEx callback: collects the drivers that have Direct3D devices.
BOOL WINAPI EnumDevicesCallback(GUID *lpGUID, LPSTR lpDriverDesc, LPSTR lpDriverName, LPVOID lpContext, HMONITOR hm)
{
	LPDIRECTDRAW4 lpDD4;
	LPDIRECTDRAW lpDD1;
	DEVICE_INFO_THING *info;
	HRESULT res;
	LPDIRECT3D3 lpD3D3;

	// the primary display was already found in the first pass
	if (checking_display_devices && !lpGUID)
		return DDENUMRET_OK;

	info = (DEVICE_INFO_THING *)malloc(sizeof(DEVICE_INFO_THING));

	if (!info)
		return DDENUMRET_OK;

	memset(info, 0, sizeof(DEVICE_INFO_THING));
	strcpy(info->name, lpDriverDesc);

	res = DirectDrawCreate(lpGUID, &lpDD1, NULL);

	if (res != DD_OK)
	{
		free(info);
		return DDENUMRET_OK;
	}

	lpDD4 = GetDirectDraw4Interface(lpDD1);

	if (!lpDD4)
	{
		lpDD1->lpVtbl->Release(lpDD1);
		free(info);
		return DDENUMRET_OK;
	}

	lpD3D3 = GetDirect3D3Interface(lpDD4);

	if (!lpD3D3)
	{
		lpDD4->lpVtbl->Release(lpDD4);
		lpDD1->lpVtbl->Release(lpDD1);
		free(info);
		return DDENUMRET_OK;
	}

	res = lpD3D3->lpVtbl->EnumDevices(lpD3D3, GetDeviceInfo, info);

#ifdef OPENDRIVER_DEBUG
	{
		// [A] diagnostics
		char text[256];
		sprintf(text, "DirectDraw driver '%s' (guid %s): EnumDevices 0x%08x, display pass %d", lpDriverDesc,
			lpGUID ? "yes" : "null", (unsigned int)res, checking_display_devices);
		OutputDebugMessage(text);
	}
#endif

	if (res != D3D_OK)
	{
		lpD3D3->lpVtbl->Release(lpD3D3);
		lpDD4->lpVtbl->Release(lpDD4);
		lpDD1->lpVtbl->Release(lpDD1);
		free(info);
		return DDENUMRET_OK;
	}

	if (lpGUID)
	{
		info->guid = *lpGUID;
		info->field_50 = 0;
	}
	else
	{
		info->field_50 = 1;
	}

	if (first_3d_device)
	{
		info->next = NULL;
		info->prev = last_3d_device;
		last_3d_device->next = info;
	}
	else
	{
		info->next = NULL;
		info->prev = NULL;
		first_3d_device = info;
	}

	last_3d_device = info;

	info->IsDisplayDevice = checking_display_devices;

	lpD3D3->lpVtbl->Release(lpD3D3);
	lpDD4->lpVtbl->Release(lpDD4);
	lpDD1->lpVtbl->Release(lpDD1);

	return DDENUMRET_OK;
}

// [0x420227] [D]
// IDirect3D3::EnumDevices callback: keeps the hardware devices of a driver.
HRESULT CALLBACK GetDeviceInfo(GUID *lpGuid, LPSTR lpDeviceDescription, LPSTR lpDeviceName, LPD3DDEVICEDESC lpHWDesc, LPD3DDEVICEDESC lpHELDesc, LPVOID lpContext)
{
	DEVICE_INFO_THING *info;
	DEVICE_INFO_2 *dev;

	info = (DEVICE_INFO_THING *)lpContext;

#ifdef OPENDRIVER_DEBUG
	{
		// [A] diagnostics
		char text[256];
		sprintf(text, "  D3D device '%s' '%s': HW colormodel %d, HEL colormodel %d", lpDeviceDescription, lpDeviceName,
			lpHWDesc ? (int)lpHWDesc->dcmColorModel : -1, lpHELDesc ? (int)lpHELDesc->dcmColorModel : -1);
		OutputDebugMessage(text);
	}
#endif

	if (!lpGuid)
		return D3DENUMRET_OK;

	if (!lpHWDesc->dcmColorModel)
		return D3DENUMRET_OK;

	if (!info)
		return D3DENUMRET_OK;

	dev = (DEVICE_INFO_2 *)malloc(sizeof(DEVICE_INFO_2));

	if (!dev)
		return D3DENUMRET_OK;

	memset(dev, 0, sizeof(DEVICE_INFO_2));
	memcpy(&dev->field_10, lpHWDesc, sizeof(dev->field_10));
	memcpy(&dev->field_100, lpHELDesc, sizeof(dev->field_100));
	dev->field_00 = *lpGuid;

	if (info->first)
	{
		dev->next = NULL;
		dev->prev = info->last;
		info->last->next = dev;
	}
	else
	{
		dev->next = NULL;
		dev->prev = info->last;
		info->first = dev;
	}

	info->last = dev;

	return D3DENUMRET_OK;
}

// [0x420354] [D]
// Turns the collected driver/device lists into devices_3d[] (the ones CanUse3DDevice accepts).
void Find3DDevices(void)
{
	DEVICE_INFO_THING *drv;
	DEVICE_INFO_THING *next_drv;
	DEVICE_INFO_2 *dev;
	DEVICE_INFO_2 *next_dev;
	DEVICE_3D_INFO *info;

	for (drv = first_3d_device; drv; drv = next_drv)
	{
		next_drv = drv->next;

		for (dev = drv->first; dev; dev = next_dev)
		{
			next_dev = dev->next;

			if (num_3d_devices < MAX_3D_DEVICES)
			{
				info = (DEVICE_3D_INFO *)malloc(sizeof(DEVICE_3D_INFO));
				memset(info, 0, sizeof(DEVICE_3D_INFO));
				memset(&devices_3d[num_3d_devices], 0, sizeof(DEVICE_3D));

				if (drv->IsDisplayDevice)
					sprintf(devices_3d[num_3d_devices].name, "%s ( Multi )", drv->name);
				else
					sprintf(devices_3d[num_3d_devices].name, drv->name);

				devices_3d[num_3d_devices].type = DEVICE_TYPE_D3D;
				devices_3d[num_3d_devices].field_48 = info;		// WARNING: potential for use after free!

				memcpy(&info->HWDesc, &dev->field_10, sizeof(info->HWDesc));
				memcpy(&info->SWDesc, &dev->field_100, sizeof(info->SWDesc));

				info->field_00 = NULL;

				if (!drv->field_50)
				{
					info->field_04 = drv->guid;
					info->field_00 = &info->field_04;
				}

				info->field_18 = dev->field_00;
				info->field_14 = &info->field_18;

				if (CanUse3DDevice(&devices_3d[num_3d_devices]))
					devices_3d[num_3d_devices++].hardware_8bit = !info->emulate_8bit;
				else
					free(info);		// WARNING: potential for use after free!
			}

			free(dev);
		}

		free(drv);
	}
}

// [0x420588] [D]
// Checks a device can do what the emulation needs and finds its texture / z buffer formats.
int CanUse3DDevice(DEVICE_3D *device)
{
	LPDIRECT3DDEVICE3 lpD3DDev3;
	DEVICE_3D_INFO *info;
	LPDIRECTDRAW4 lpDD4;
	LPDIRECTDRAW lpDD1;
	HRESULT res;
	DWORD caps2;
	LPDIRECT3D3 lpD3D3;
	LPDIRECTDRAWSURFACE4 lpPrimary;
	DDSURFACEDESC2 ddsd;
	LPD3DDEVICEDESC desc;
	DWORD devCaps;
	DWORD flags;

	info = device->field_48;

	OutputDebugMessage("Checking 3D device:");
	OutputDebugMessage(device->name);

	desc = &info->HWDesc;

	if (info->HWDesc.dcmColorModel)
	{
		info->UseHardwareColorModel = 1;
	}
	else
	{
		desc = &info->SWDesc;
		info->UseHardwareColorModel = 0;
	}

	flags = desc->dwFlags;
	devCaps = desc->dwDevCaps;

	if (!(flags & D3DDD_COLORMODEL) || !(flags & D3DDD_DEVCAPS))
	{
		OutputDebugMessage("ERROR: Get device info failed");
		return 0;
	}

	if (desc->dcmColorModel != D3DCOLOR_RGB)
	{
		OutputDebugMessage("ERROR: Device is software renderer");
		return 0;
	}

	if (!(devCaps & D3DDEVCAPS_FLOATTLVERTEX))
	{
		OutputDebugMessage("ERROR: TLVERTEX not supported");
		return 0;
	}

	if (!(devCaps & D3DDEVCAPS_DRAWPRIMTLVERTEX))
	{
		OutputDebugMessage("ERROR: DrawPrimitive not supported");
		return 0;
	}

	if (desc->dwMinTextureWidth > 256 || desc->dwMinTextureHeight > 256 ||
		desc->dwMaxTextureWidth < 256 || desc->dwMaxTextureHeight < 256)
	{
		OutputDebugMessage("ERROR: Can't do 256x256 textures");
		return 0;
	}

	info->field_51c = 0;
	info->field_520 = DDSCAPS_TEXTURE;
	info->field_524 = DDSCAPS2_TEXTUREMANAGE;

	res = DirectDrawCreate(info->field_00, &lpDD1, NULL);

	if (res != DD_OK)
	{
		OutputDebugMessage("ERROR: Failed to create ( lpDD )");
		return 0;
	}

	lpDD4 = GetDirectDraw4Interface(lpDD1);

	if (!lpDD4)
	{
		OutputDebugMessage("ERROR: Failed to create ( lpDD4 )");
		lpDD1->lpVtbl->Release(lpDD1);
		return 0;
	}

	res = lpDD4->lpVtbl->SetCooperativeLevel(lpDD4, GetActiveWindow(), DDSCL_FULLSCREEN | DDSCL_EXCLUSIVE);

	if (res != DD_OK)
	{
		OutputDebugMessage("ERROR: SetcooperativeLevel failed");
		lpDD4->lpVtbl->Release(lpDD4);
		lpDD1->lpVtbl->Release(lpDD1);
		return 0;
	}

	if (!SetDirectDrawDisplayMode(lpDD4, 640, 480))
	{
		OutputDebugMessage("ERROR: Failed to set display mode");
		lpDD4->lpVtbl->Release(lpDD4);
		lpDD1->lpVtbl->Release(lpDD1);
		return 0;
	}

	memset(&info->HWCaps, 0, sizeof(info->HWCaps));
	memset(&info->SWCaps, 0, sizeof(info->SWCaps));
	info->HWCaps.dwSize = sizeof(DDCAPS_DX6);
	info->SWCaps.dwSize = sizeof(DDCAPS_DX6);

	res = lpDD4->lpVtbl->GetCaps(lpDD4, (LPDDCAPS)&info->HWCaps, (LPDDCAPS)&info->SWCaps);

	if (res != DD_OK)
	{
		OutputDebugMessage("ERROR: GetCaps failed");
		lpDD4->lpVtbl->Release(lpDD4);
		lpDD1->lpVtbl->Release(lpDD1);
		return 0;
	}

	caps2 = info->SWCaps.dwCaps2 | info->HWCaps.dwCaps2;

	if (caps2 & DDCAPS2_NO2DDURING3DSCENE)
	{
		OutputDebugMessage("ERROR: Can't load textures during rendering");
		lpDD4->lpVtbl->Release(lpDD4);
		lpDD1->lpVtbl->Release(lpDD1);
		return 0;
	}

	lpD3D3 = GetDirect3D3Interface(lpDD4);

	if (!lpD3D3)
	{
		OutputDebugMessage("ERROR: Failed to create ( lpD3D3 )");
		lpDD4->lpVtbl->Release(lpDD4);
		lpDD1->lpVtbl->Release(lpDD1);
		return 0;
	}

	memset(&ddsd, 0, sizeof(ddsd));
	ddsd.dwSize = sizeof(DDSURFACEDESC2);
	ddsd.dwFlags = DDSD_CAPS;
	ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE | DDSCAPS_3DDEVICE;

	lpPrimary = CreateDirectDrawSurface(lpDD4, &ddsd);

	if (!lpPrimary)
	{
		OutputDebugMessage("ERROR: Failed to create ( lpPrimary )");
		lpD3D3->lpVtbl->Release(lpD3D3);
		lpDD4->lpVtbl->Release(lpDD4);
		lpDD1->lpVtbl->Release(lpDD1);
		return 0;
	}

	lpD3DDev3 = GetDirect3DDevice3Interface(lpD3D3, info->field_14, lpPrimary);

	if (!lpD3DDev3)
	{
		OutputDebugMessage("ERROR: Failed to create ( lpD3DDev3 )");
		lpPrimary->lpVtbl->Release(lpPrimary);
		lpD3D3->lpVtbl->Release(lpD3D3);
		lpDD4->lpVtbl->Release(lpDD4);
		lpDD1->lpVtbl->Release(lpDD1);
		return 0;
	}

	res = lpD3DDev3->lpVtbl->EnumTextureFormats(lpD3DDev3, EnumPixelFormatsCallback, info);

	if (res != D3D_OK)
	{
		OutputDebugMessage("ERROR: Enum textures failed");
		lpD3DDev3->lpVtbl->Release(lpD3DDev3);
		lpPrimary->lpVtbl->Release(lpPrimary);
		lpD3D3->lpVtbl->Release(lpD3D3);
		lpDD4->lpVtbl->Release(lpDD4);
		lpDD1->lpVtbl->Release(lpDD1);
		return 0;
	}

	res = lpD3D3->lpVtbl->EnumZBufferFormats(lpD3D3, info->field_14, EnumPixelFormatsCallback, info);

	if (res != D3D_OK)
	{
		OutputDebugMessage("ERROR: Enum zbuffer failed");
		lpD3DDev3->lpVtbl->Release(lpD3DDev3);
		lpPrimary->lpVtbl->Release(lpPrimary);
		lpD3D3->lpVtbl->Release(lpD3D3);
		lpDD4->lpVtbl->Release(lpDD4);
		lpDD1->lpVtbl->Release(lpDD1);
		return 0;
	}

	if ((info->field_58c & (PF_FOUND_8BIT | PF_FOUND_16BIT | PF_FOUND_ZBUFFER)) == (PF_FOUND_8BIT | PF_FOUND_16BIT | PF_FOUND_ZBUFFER))
	{
		info->emulate_8bit = 0;
		OutputDebugMessage("Card OK.");
	}
	else
	{
		OutputDebugMessage("WARNING: No 8bit texture support!!!!!!!");
		info->emulate_8bit = 1;
		memcpy(&info->pfTex8, &info->pfTex16, sizeof(info->pfTex8));
	}

	if (!CanDeviceTexture(info, lpDD4))
	{
		lpD3DDev3->lpVtbl->Release(lpD3DDev3);
		lpPrimary->lpVtbl->Release(lpPrimary);
		lpD3D3->lpVtbl->Release(lpD3D3);
		lpDD4->lpVtbl->Release(lpDD4);
		lpDD1->lpVtbl->Release(lpDD1);
		return 0;
	}

	lpD3DDev3->lpVtbl->Release(lpD3DDev3);
	lpPrimary->lpVtbl->Release(lpPrimary);
	lpD3D3->lpVtbl->Release(lpD3D3);
	lpDD4->lpVtbl->Release(lpDD4);
	lpDD1->lpVtbl->Release(lpDD1);

	OutputDebugMessage("3D device OK\n");
	return 1;
}

// [0x420dfc] [D]
// Texture and z buffer format enumeration: takes the first 16 bit RGB (preferring RGBA5551),
// the first 8 bit palettised and the first z buffer format.
HRESULT CALLBACK EnumPixelFormatsCallback(LPDDPIXELFORMAT lpDDPixFmt, LPVOID lpContext)
{
	DEVICE_3D_INFO *info;

	info = (DEVICE_3D_INFO *)lpContext;

	if (!info)
		return D3DENUMRET_OK;

	if ((lpDDPixFmt->dwFlags & DDPF_RGB) && !(info->field_58c & PF_HAVE_RGBA5551))
	{
		if (lpDDPixFmt->dwRGBBitCount == 16)
		{
			// setup DDPF_RGB
			info->field_58c |= PF_FOUND_16BIT;
			memcpy(&info->pfTex16, lpDDPixFmt, sizeof(info->pfTex16));

			rbits = 8 - GetBits(lpDDPixFmt->dwRBitMask);
			gbits = 8 - GetBits(lpDDPixFmt->dwGBitMask);
			bbits = 8 - GetBits(lpDDPixFmt->dwBBitMask);

			if (lpDDPixFmt->dwRGBAlphaBitMask)
				abits = 8 - GetBits(lpDDPixFmt->dwRGBAlphaBitMask);

			// RGBA5551 supported?
			if (rbits == 5 && gbits == 5 && bbits == 5 && abits == 1)
				info->field_58c |= PF_HAVE_RGBA5551;
		}
	}
	else if ((lpDDPixFmt->dwFlags & DDPF_PALETTEINDEXED8) && !(info->field_58c & PF_HAVE_8BIT))
	{
		// setup DDPF_PALETTEINDEXED8
		info->field_58c |= PF_HAVE_8BIT | PF_FOUND_8BIT;
		memcpy(&info->pfTex8, lpDDPixFmt, sizeof(info->pfTex8));
	}
	else if (lpDDPixFmt->dwFlags == DDPF_ZBUFFER && !(info->field_58c & PF_HAVE_ZBUFFER))
	{
		// setup DDPF_ZBUFFER
		info->field_58c |= PF_HAVE_ZBUFFER | PF_FOUND_ZBUFFER;
		memcpy(&info->pfZBuffer, lpDDPixFmt, sizeof(info->pfZBuffer));
	}

	return D3DENUMRET_OK;
}

// [0x420fc9] [D]
int CanDeviceTexture(DEVICE_3D_INFO *info, LPDIRECTDRAW4 lpDD4)
{
	LPDIRECTDRAWSURFACE4 lpSurf;
	DDSURFACEDESC2 ddsd;

	memset(&ddsd, 0, sizeof(ddsd));
	ddsd.dwSize = sizeof(DDSURFACEDESC2);
	ddsd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT;
	ddsd.ddsCaps.dwCaps = info->field_520;
	ddsd.ddsCaps.dwCaps2 = info->field_524;
	ddsd.dwWidth = 256;
	ddsd.dwHeight = 256;
	ddsd.ddpfPixelFormat = info->pfTex16;

	lpSurf = CreateDirectDrawSurface(lpDD4, &ddsd);

	if (!lpSurf)
	{
		OutputDebugMessage("CanDeviceTexture failed - Device can't create 256x256 16bit texture");
		return 0;
	}

	lpSurf->lpVtbl->Release(lpSurf);

	if (!info->emulate_8bit)
	{
		ddsd.ddpfPixelFormat = info->pfTex8;

		lpSurf = CreateDirectDrawSurface(lpDD4, &ddsd);

		if (!lpSurf)
		{
			OutputDebugMessage("CanDeviceTexture failed - Device can't create 256x256 8bit texture");
			return 0;
		}

		lpSurf->lpVtbl->Release(lpSurf);
	}

	return 1;
}

// [0x4210c1] [D]
LPDIRECTDRAW GetDirectDrawInterface(GUID *guid, BOOL fullscreen)
{
	LPDIRECTDRAW lpDD1;
	HRESULT res;
	DDSURFACEDESC ddsd;

	OutputDebugMessage("GetDirectDrawInterface( )");

	res = DirectDrawCreate(guid, &lpDD1, NULL);

	if (res != DD_OK)
	{
		OutputDebugMessage("DirectDrawCreate failed");
		return NULL;
	}

	memset(&ddsd, 0, sizeof(ddsd));
	ddsd.dwSize = sizeof(DDSURFACEDESC);
	ddsd.dwFlags = DDSD_PIXELFORMAT;

	res = lpDD1->lpVtbl->GetDisplayMode(lpDD1, &ddsd);

	if (res != DD_OK)
	{
		OutputDebugMessage("GetDisplayMode failed");
		return NULL;
	}

	if (ddsd.ddpfPixelFormat.dwFlags & DDPF_RGB)
	{
		current_display_depth = wanted_display_depth;

		if (wanted_display_depth == 16 || current_display_depth == 32)
			return lpDD1;
	}

	MessageBox(glide_window, "To run, please switch to a\n16/32bit display mode.", "Sorry!", MB_ICONHAND);
	return NULL;
}

// [0x4211af] [D]
LPDIRECTDRAWSURFACE4 CreateDirectDrawSurface(LPDIRECTDRAW4 lpDD4, DDSURFACEDESC2 *ddsd)
{
	LPDIRECTDRAWSURFACE4 lpSurf;

	OutputDebugMessage("CreateDirectDrawSurface( )");

	if (lpDD4->lpVtbl->CreateSurface(lpDD4, ddsd, &lpSurf, NULL) == DD_OK)
		return lpSurf;

	OutputDebugMessage("CreateDirectDrawSurface failed");
	return NULL;
}

// [0x4211f9] [D]
LPDIRECTDRAW4 GetDirectDraw4Interface(LPDIRECTDRAW lpDD1)
{
	LPDIRECTDRAW4 lpDD4;
	HRESULT res;

	OutputDebugMessage("GetDirectDraw4Interface( )");

	if (!lpDD1)
		return NULL;

	res = lpDD1->lpVtbl->QueryInterface(lpDD1, &IID_IDirectDraw4, (LPVOID *)&lpDD4);

	if (res == DD_OK)
		return lpDD4;

	OutputDebugMessage("QueryInterface( IID_IDirectDraw4 ) failed");
	return NULL;
}

// [0x42124b] [D]
LPDIRECT3D3 GetDirect3D3Interface(LPDIRECTDRAW4 lpDD4)
{
	LPDIRECT3D3 lpD3D3;

	OutputDebugMessage("GetDirect3D3Interface( )");

	if (!lpDD4)
		return NULL;

	if (lpDD4->lpVtbl->QueryInterface(lpDD4, &IID_IDirect3D3, (LPVOID *)&lpD3D3) == DD_OK)
		return lpD3D3;

	OutputDebugMessage("QueryInterface( IID_IDirect3D2 ) failed");
	return NULL;
}

// [0x42129d] [D]
LPDIRECT3DDEVICE3 GetDirect3DDevice3Interface(LPDIRECT3D3 lpD3D3, LPGUID guid, LPDIRECTDRAWSURFACE4 lpSurf)
{
	LPDIRECT3DDEVICE3 lpDev;
	HRESULT res;

	lpDev = NULL;

	OutputDebugMessage("GetDirect3DDevice3Interface( )");

	if (!lpD3D3)
		return NULL;

	res = lpD3D3->lpVtbl->CreateDevice(lpD3D3, guid, lpSurf, &lpDev, NULL);

	if (res != D3D_OK)
	{
		OutputDebugMessage("CreateDevice( 3D3 ) failed");
		return NULL;
	}

	return lpDev;
}

// [0x4212fc] [D]
LPDIRECT3DVIEWPORT3 CreateDirect3DViewport(LPDIRECT3D3 lpD3D3)
{
	LPDIRECT3DVIEWPORT3 lpView;

	OutputDebugMessage("CreateDirect3DViewport( )");

	if (!lpD3D3)
		return NULL;

	if (lpD3D3->lpVtbl->CreateViewport(lpD3D3, &lpView, NULL) == D3D_OK)
		return lpView;

	OutputDebugMessage("CreateDirect3DViewport failed");
	return NULL;
}

// [0x42134c] [D]
BOOL AddDirect3DViewport(LPDIRECT3DDEVICE3 lpD3Ddev, LPDIRECT3DVIEWPORT3 lpView, int width, int height)
{
	HRESULT res;
	D3DVIEWPORT2 vp;

	OutputDebugMessage("AddDirect3DViewport( )");

	if (!lpD3Ddev)
		return FALSE;

	if (!lpView)
		return FALSE;

	res = lpD3Ddev->lpVtbl->AddViewport(lpD3Ddev, lpView);

	if (res != D3D_OK)
	{
		OutputDebugMessage("AddDirect3DViewport ( AddViewport ) failed");
		return FALSE;
	}

	memset(&vp, 0, sizeof(vp));
	vp.dwSize = sizeof(D3DVIEWPORT2);
	vp.dwX = 0;
	vp.dwY = 0;
	vp.dwWidth = width;
	vp.dwHeight = height;
	vp.dvClipX = 0.0f;
	vp.dvClipY = 0.0f;
	vp.dvClipWidth = (float)width;
	vp.dvClipHeight = (float)height;
	vp.dvMinZ = 0.0f;
	vp.dvMaxZ = 1.0f;

	res = lpView->lpVtbl->SetViewport2(lpView, &vp);

	if (res != D3D_OK)
	{
		OutputDebugMessage("AddDirect3DViewport ( SetViewport2 ) failed");
		return FALSE;
	}

	return TRUE;
}

// [0x421432] [D]
// Clears the z buffer inside the clip window.
int ClearDirect3DViewport(LPDIRECT3DVIEWPORT3 lpView, float z)
{
	D3DRECT rect;

	if (!lpView)
		return 0;

	rect.x1 = glide_clipx;
	rect.y1 = glide_clipy;
	rect.x2 = glide_clipx + glide_clipw;
	rect.y2 = glide_clipy + glide_cliph;

	lpView->lpVtbl->Clear2(lpView, 1, &rect, D3DCLEAR_ZBUFFER, 0, z, 0);

	return 1;
}

// [0x421495] [D]
// Fills the clip window with a Glide colour (0xRRGGBB) converted to the surface format.
int ClearDirectDrawSurface(LPDIRECTDRAWSURFACE4 lpSurf, GrColor_t color)
{
	HRESULT res;
	int tries;
	DDBLTFX fx;
	unsigned int g;
	unsigned int r;
	int b;
	RECT rect;

	rect.left = glide_clipx;
	rect.top = glide_clipy;
	rect.right = glide_clipx + glide_clipw;
	rect.bottom = glide_clipy + glide_cliph;

	r = ((color & 0xFF0000) >> 16) >> rbits;
	g = ((color & 0xFF00) >> 8) >> gbits;
	b = (color & 0xFF) >> bbits;

	memset(&fx, 0, sizeof(fx));
	fx.dwSize = sizeof(DDBLTFX);
	fx.dwFillColor = (b << bshift) | (g << gshift) | (r << rshift);

	for (tries = 0; tries < 100; tries++)
	{
		res = lpSurf->lpVtbl->Blt(lpSurf, &rect, NULL, NULL, DDBLT_COLORFILL | DDBLT_WAIT, &fx);

		if (res == DD_OK)
			break;

		if (res == DDERR_SURFACELOST)
			RestoreSurfaces();
	}

	return 1;
}

// [0x4215ba] [D]
int SetDirectDrawDisplayMode(LPDIRECTDRAW4 lpDD4, DWORD width, DWORD height)
{
	OutputDebugMessage("SetDirectDrawDisplayMode( )");

	if (lpDD4->lpVtbl->SetDisplayMode(lpDD4, width, height, current_display_depth, 0, 0) == DD_OK)
		return 1;

	OutputDebugMessage("SetDisplayMode failed");
	return 0;
}

// [0x42160c] [D]
// Fullscreen: a flipping primary surface; the back buffer is rendered to.
int CreateFullscreenRenderer(void)
{
	DDSCAPS2 caps;
	DDSURFACEDESC2 ddsd;

	memset(&ddsd, 0, sizeof(ddsd));
	ddsd.dwSize = sizeof(DDSURFACEDESC2);
	ddsd.dwFlags = DDSD_CAPS | DDSD_BACKBUFFERCOUNT;

	if (triple_buffering)
		ddsd.dwBackBufferCount = 2;
	else
		ddsd.dwBackBufferCount = 1;

	ddsd.ddsCaps.dwCaps = DDSCAPS_COMPLEX | DDSCAPS_FLIP | DDSCAPS_PRIMARYSURFACE | DDSCAPS_3DDEVICE;

	lpPrimarySurface = CreateDirectDrawSurface(lpDD, &ddsd);

	if (!lpPrimarySurface)
	{
		OutputDebugMessage("CreatePrimarySurface failed");
		return 0;
	}

	memset(&caps, 0, sizeof(caps));		// NOTE: the binary sets dwCaps only; the rest is stack garbage
	caps.dwCaps = DDSCAPS_BACKBUFFER;

	if (lpPrimarySurface->lpVtbl->GetAttachedSurface(lpPrimarySurface, &caps, &lpRenderSurface) != DD_OK)
	{
		OutputDebugMessage("CreateRenderSurface failed");
		lpPrimarySurface->lpVtbl->Release(lpPrimarySurface);
		return 0;
	}

	return 1;
}

// [0x4216e8] [D]
// Windowed: render to an offscreen surface, BufferSwap blits it to the clipped primary.
BOOL CreateWindowedRenderer(DWORD width, DWORD height)
{
	DDSURFACEDESC2 ddsd;

	memset(&ddsd, 0, sizeof(ddsd));
	ddsd.dwSize = sizeof(DDSURFACEDESC2);
	ddsd.dwFlags = DDSD_CAPS;
	ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;

	lpPrimarySurface = CreateDirectDrawSurface(lpDD, &ddsd);

	if (!lpPrimarySurface)
		return FALSE;

	memset(&ddsd, 0, sizeof(ddsd));
	ddsd.dwSize = sizeof(DDSURFACEDESC2);
	ddsd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
	ddsd.dwWidth = width;
	ddsd.dwHeight = height;

	if (current_3d_device->UseHardwareColorModel)
		ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_3DDEVICE | DDSCAPS_VIDEOMEMORY;
	else
		ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY | DDSCAPS_3DDEVICE;

	lpRenderSurface = CreateDirectDrawSurface(lpDD, &ddsd);

	if (!lpRenderSurface)
		return FALSE;

	lpClipper = CreateDirectDrawClipper((LPDIRECTDRAW)lpDD);

	return lpClipper && AddDirectDrawClipper(lpPrimarySurface, lpClipper);
}

// [0x4217ef] [D]
// NOTE: called with the IDirectDraw4 cast to IDirectDraw; CreateClipper is at the same vtable slot
LPDIRECTDRAWCLIPPER CreateDirectDrawClipper(LPDIRECTDRAW lpDD1)
{
	LPDIRECTDRAWCLIPPER lpClip;
	HRESULT res;

	OutputDebugMessage("CreateDirectDrawClipper( )");

	res = lpDD1->lpVtbl->CreateClipper(lpDD1, 0, &lpClip, NULL);

	if (res == DD_OK)
		return lpClip;

	OutputDebugMessage("CreateClipper failed");
	return NULL;
}

// [0x421837] [D]
int AddDirectDrawClipper(LPDIRECTDRAWSURFACE4 lpSurf, LPDIRECTDRAWCLIPPER lpClip)
{
	OutputDebugMessage("AddDirectDrawClipper( )");

	if (lpSurf->lpVtbl->SetClipper(lpSurf, lpClip) == DD_OK)
		return 1;

	OutputDebugMessage("SetClipper failed");
	return 0;
}

// [0x42187b] [D]
void RestoreSurfaces(void)
{
	if (lpPrimarySurface && lpPrimarySurface->lpVtbl->IsLost(lpPrimarySurface) != DD_OK &&
		lpPrimarySurface->lpVtbl->Restore(lpPrimarySurface) == DDERR_WRONGMODE)
	{
		RestoreDirectDrawDisplayMode();
	}

	if (lpRenderSurface && lpRenderSurface->lpVtbl->IsLost(lpRenderSurface) != DD_OK &&
		lpRenderSurface->lpVtbl->Restore(lpRenderSurface) == DDERR_WRONGMODE)
	{
		RestoreDirectDrawDisplayMode();
	}

	if (ZSurface && ZSurface->lpVtbl->IsLost(ZSurface) != DD_OK &&
		ZSurface->lpVtbl->Restore(ZSurface) == DDERR_WRONGMODE)
	{
		RestoreDirectDrawDisplayMode();
	}
}

// [0x421943] [D] [A]
// NOTE: IDA shows an int return (SetDirectDrawDisplayMode's, garbage when windowed); nothing uses it
void RestoreDirectDrawDisplayMode(void)
{
	if (glide_fullscreen)
	{
		// NOTE: [A] windowed mode doesn't maximise the window
		if (!gWindowed)
			ShowWindow(glide_window, SW_MAXIMIZE);

		SetDirectDrawDisplayMode(lpDD, glide_width, glide_height);
	}
}

// [0x42197b] [D]
// NOTE: Hex-Rays mangles this one; translated from the disassembly. The size parameters are
// overwritten with the surface's own size.
int CreateZSurface(LPDIRECTDRAWSURFACE4 lpSurf, DWORD width, DWORD height)
{
	DDSURFACEDESC2 ddsd;

	OutputDebugMessage("CreateZSurface( )");

	memset(&ddsd, 0, sizeof(ddsd));
	ddsd.dwSize = sizeof(DDSURFACEDESC2);

	lpSurf->lpVtbl->GetSurfaceDesc(lpSurf, &ddsd);

	width = ddsd.dwWidth;
	height = ddsd.dwHeight;

	memset(&ddsd, 0, sizeof(ddsd));
	ddsd.dwSize = sizeof(DDSURFACEDESC2);
	ddsd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT;
	ddsd.dwWidth = width;
	ddsd.dwHeight = height;
	ddsd.ddpfPixelFormat = current_3d_device->pfZBuffer;

	if (current_3d_device->UseHardwareColorModel)
		ddsd.ddsCaps.dwCaps = DDSCAPS_ZBUFFER | DDSCAPS_VIDEOMEMORY;
	else
		ddsd.ddsCaps.dwCaps = DDSCAPS_ZBUFFER | DDSCAPS_SYSTEMMEMORY;

	ZSurface = CreateDirectDrawSurface(lpDD, &ddsd);

	if (!ZSurface)
		return 0;

	if (!AttachDirectDrawSurface(lpRenderSurface, ZSurface))
	{
		OutputDebugMessage("AttachSurface( zbuffer ) failed");
		return 0;
	}

	return 1;
}

// [0x421a7c] [D]
int AttachDirectDrawSurface(LPDIRECTDRAWSURFACE4 lpSurf, LPDIRECTDRAWSURFACE4 lpAttach)
{
	OutputDebugMessage("AttachDirectDrawSurface( )");

	if (lpSurf->lpVtbl->AddAttachedSurface(lpSurf, lpAttach) == DD_OK)
		return 1;

	OutputDebugMessage("AddAttachedSurface failed");
	return 0;
}

// [0x421ac0] [D]
void SetupRenderStates(void)
{
	StartD3DRender();

	lpD3DDev->lpVtbl->SetCurrentViewport(lpD3DDev, lpViewport);

	lpD3DDev->lpVtbl->SetRenderState(lpD3DDev, D3DRENDERSTATE_ZENABLE, TRUE);
	lpD3DDev->lpVtbl->SetRenderState(lpD3DDev, D3DRENDERSTATE_ZWRITEENABLE, TRUE);
	lpD3DDev->lpVtbl->SetRenderState(lpD3DDev, D3DRENDERSTATE_ZFUNC, D3DCMP_LESSEQUAL);
	lpD3DDev->lpVtbl->SetRenderState(lpD3DDev, D3DRENDERSTATE_SHADEMODE, D3DSHADE_GOURAUD);
	lpD3DDev->lpVtbl->SetRenderState(lpD3DDev, D3DRENDERSTATE_TEXTUREMAG, D3DFILTER_NEAREST);
	lpD3DDev->lpVtbl->SetRenderState(lpD3DDev, D3DRENDERSTATE_TEXTUREMIN, D3DFILTER_NEAREST);
	lpD3DDev->lpVtbl->SetRenderState(lpD3DDev, D3DRENDERSTATE_ALPHABLENDENABLE, FALSE);
	lpD3DDev->lpVtbl->SetRenderState(lpD3DDev, D3DRENDERSTATE_FILLMODE, D3DFILL_SOLID);
	lpD3DDev->lpVtbl->SetRenderState(lpD3DDev, D3DRENDERSTATE_DITHERENABLE, TRUE);
	lpD3DDev->lpVtbl->SetRenderState(lpD3DDev, D3DRENDERSTATE_SPECULARENABLE, FALSE);
	lpD3DDev->lpVtbl->SetRenderState(lpD3DDev, D3DRENDERSTATE_ANTIALIAS, D3DANTIALIAS_NONE);
	lpD3DDev->lpVtbl->SetRenderState(lpD3DDev, D3DRENDERSTATE_CULLMODE, D3DCULL_NONE);
	lpD3DDev->lpVtbl->SetRenderState(lpD3DDev, D3DRENDERSTATE_ALPHAREF, 0);
	lpD3DDev->lpVtbl->SetRenderState(lpD3DDev, D3DRENDERSTATE_SUBPIXEL, TRUE);

	lpD3DDev->lpVtbl->SetTextureStageState(lpD3DDev, 0, D3DTSS_COLOROP, D3DTOP_MODULATE);
	lpD3DDev->lpVtbl->SetTextureStageState(lpD3DDev, 0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	lpD3DDev->lpVtbl->SetTextureStageState(lpD3DDev, 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
	lpD3DDev->lpVtbl->SetTextureStageState(lpD3DDev, 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
	lpD3DDev->lpVtbl->SetTextureStageState(lpD3DDev, 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
	lpD3DDev->lpVtbl->SetTextureStageState(lpD3DDev, 1, D3DTSS_COLOROP, D3DTOP_DISABLE);
}

// [0x421cb8] [D]
// Uploads a 256x256 8 bit image; the first of a pair creates the 8 bit surfaces.
BOOL LoadMipmap8(void *image, int surface)
{
	char *surf;
	char *src;
	int y;
	DDSURFACEDESC2 ddsd;

	if (surface & 1)
	{
		if (textures[surface].depth != 8)
		{
			OutputDebugMessage("Can't load 8bit texture into 16bit page");
			return FALSE;
		}
	}
	else if (!Create8BitTextures(surface))
	{
		return FALSE;
	}

	memset(&ddsd, 0, sizeof(ddsd));
	ddsd.dwSize = sizeof(DDSURFACEDESC2);

	if (textures[surface].surface->lpVtbl->Lock(textures[surface].surface, NULL, &ddsd, DDLOCK_WAIT, NULL) != DD_OK)
	{
		OutputDebugMessage("Lock( texture8 ) failed");
		return FALSE;
	}

	surf = (char *)ddsd.lpSurface;
	src = (char *)image;

	for (y = 0; y < 256; y++)
	{
		CopyMem_glide2x(surf, src, 256);

		surf += ddsd.lPitch;
		src += 256;
	}

	textures[surface].surface->lpVtbl->Unlock(textures[surface].surface, NULL);

	return TRUE;
}

// [0x421e00] [D]
// Uploads a 256x256 Glide ARGB1555 image, converting it through texlookup16.
BOOL LoadMipmap16(void *image, int surface)
{
	unsigned short *dst;
	unsigned short pixel;
	unsigned short *line;
	int y;
	int x;
	DDSURFACEDESC2 ddsd;
	unsigned short *src16;

	if (!textures[surface].emulated && !Create16BitTexture(surface))
		return FALSE;

	memset(&ddsd, 0, sizeof(ddsd));
	ddsd.dwSize = sizeof(DDSURFACEDESC2);

	if (textures[surface].surface->lpVtbl->Lock(textures[surface].surface, NULL, &ddsd, DDLOCK_WAIT, NULL) != DD_OK)
	{
		OutputDebugMessage("Lock( texture16 ) failed");
		return FALSE;
	}

	line = (unsigned short *)ddsd.lpSurface;
	src16 = (unsigned short *)image;

	for (y = 0; y < 256; y++)
	{
		dst = line;

		for (x = 0; x < 256; x++)
		{
			pixel = *src16++;
			*dst++ = texlookup16[pixel];
		}

		line += ddsd.lPitch >> 1;
	}

	textures[surface].surface->lpVtbl->Unlock(textures[surface].surface, NULL);

	return TRUE;
}

// [0x421f87] [D]
BOOL AllocateTextures(void)
{
	int surface;

	CreateTextureLookups();

	for (surface = 0; surface < MAX_TEXTURES; surface += 2)
	{
		if (!Create8BitTextures(surface))
			return FALSE;
	}

	d3dgl_ChromakeyValue(0);

	textures_allocated = 1;
	return TRUE;
}

// [0x421fd8] [D]
void FreeTextures(void)
{
	int i;

	d3d_ReleaseEmulatedTextures();

	for (i = 0; i < MAX_TEXTURES; i++)
		ReleaseTexture(i);

	last_texture = NULL;
	textures_allocated = 0;
}

// [0x42201f] [D]
void CreateTextureLookups(void)
{
	DDPIXELFORMAT *ddpf;
	int ct;
	int a;

	ddpf = &current_3d_device->pfTex16;

	rbits = GetBits(ddpf->dwRBitMask);
	rshift = GetShift(ddpf->dwRBitMask);
	gbits = GetBits(ddpf->dwGBitMask);
	gshift = GetShift(ddpf->dwGBitMask);
	bbits = GetBits(ddpf->dwBBitMask);
	bshift = GetShift(ddpf->dwBBitMask);

	if (ddpf->dwRGBAlphaBitMask)
	{
		abits = GetBits(ddpf->dwRGBAlphaBitMask);
		ashift = GetShift(ddpf->dwRGBAlphaBitMask);
	}
	else
	{
		abits = 32;
		ashift = 0;
	}

	// Create 16bit texture upload lookup table! (Glide ARGB1555 -> surface format)
	for (ct = 0; ct < 0x10000; ct++)
	{
		if (ct & 0x8000)
			a = 255 >> abits;
		else
			a = 0;

		texlookup16[ct] = (unsigned short)((a << ashift) |
			((((ct & 0x1F) << 3) >> bbits) << bshift) |
			((((ct & 0x3E0) >> 2) >> gbits) << gshift) |
			((((ct & 0x7C00) >> 7) >> rbits) << rshift));
	}
}

// [0x4221bc] [D]
// Returns how many bits an 8 bit component loses in the mask (8 - mask width).
int GetBits(DWORD mask)
{
	int count;

	if (!mask)
		return 8;

	while (!(mask & 1))
		mask >>= 1;

	count = 0;

	while (mask & 1)
	{
		mask >>= 1;
		count++;
	}

	return 8 - count;
}

// [0x422211] [D]
int GetShift(DWORD mask)
{
	int shift;

	if (!mask)
		return 0;

	shift = 0;

	while (!(mask & 1))
	{
		mask >>= 1;
		shift++;
	}

	return shift;
}

// [0x42224a] [D]
// Glide units to Direct3D z; negative biases (towards the viewer) are scaled down.
void FX_CALL d3dgl_DepthBiasLevel(FxI16 level)
{
	if (level)
	{
		DepthBias = (float)level;

		if (DepthBias < 0.0f)
			DepthBias *= 0.03846f;
	}
	else
	{
		DepthBias = 0.0f;
	}
}

// [0x42229d] [D]
// Glide vertex -> D3D TL vertex. z comes from oow (w buffer emulation).
void MAKE_VERTEX(const GrVertex *gv, D3DTLVERTEX *dv)
{
	static float recip = 1.0f / 256.0f;	// texture coords: 0..256 -> 0..1
	static float six_four = 64.0f;
	float tscale;
	float z;

	tscale = recip / gv->oow;

	dv->sx = gv->x;
	dv->sy = gv->y;
	dv->specular = 0;
	dv->rhw = gv->oow;
	dv->color = ((int)gv->g << 8) | (int)gv->b | ((int)gv->r << 16);

	if (DepthBias != 0.0f)
	{
		z = d3d_screen_scale / gv->oow + DepthBias;
		dv->sz = 1.0f - d3d_screen_scale64 / z;
	}
	else
	{
		dv->sz = 1.0f - gv->oow * six_four;
	}

	dv->tu = gv->tmuvtx[0].sow * tscale;
	dv->tv = gv->tmuvtx[0].tow * tscale;
}

// [0x422357] [D]
// MAKE_VERTEX for the game's own polygons: with a depth bias z is oow + DepthBias (not
// d3d_screen_scale / oow + DepthBias).
void MAKE_VERTEX_REAL3D(const GrVertex *gv, D3DTLVERTEX *dv)
{
	static float six_four = 64.0f;
	static float recip = 1.0f / 256.0f;	// texture coords: 0..256 -> 0..1
	float tscale;
	float z;

	tscale = recip / gv->oow;

	dv->sx = gv->x;
	dv->sy = gv->y;
	dv->specular = 0;
	dv->rhw = gv->oow;
	dv->color = ((int)gv->g << 8) | (int)gv->b | ((int)gv->r << 16);

	if (DepthBias != 0.0f)
	{
		z = gv->oow + DepthBias;
		dv->sz = 1.0f - d3d_screen_scale64 / z;
	}
	else
	{
		dv->sz = 1.0f - gv->oow * six_four;
	}

	dv->tu = gv->tmuvtx[0].sow * tscale;
	dv->tv = gv->tmuvtx[0].tow * tscale;
}

// [0x42240b] [D]
// Creates a pair of 256x256 8 bit (or 16 bit when emulated) texture surfaces.
BOOL Create8BitTextures(int surface)
{
	int ct;
	DDSURFACEDESC2 ddsd;

	for (ct = surface; ct < surface + 2; ct++)
	{
		if (textures[ct].depth != 8)
		{
			ReleaseTexture(ct);

			memset(&ddsd, 0, sizeof(ddsd));
			ddsd.dwSize = sizeof(DDSURFACEDESC2);
			ddsd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT;
			ddsd.ddsCaps.dwCaps = current_3d_device->field_520;
			ddsd.ddsCaps.dwCaps2 = current_3d_device->field_524;

			if (emulate_8bit)
				ddsd.ddpfPixelFormat = current_3d_device->pfTex16;
			else
				ddsd.ddpfPixelFormat = current_3d_device->pfTex8;

			ddsd.dwWidth = 256;
			ddsd.dwHeight = 256;

			textures[ct].surface = CreateDirectDrawSurface(lpDD, &ddsd);

			if (!textures[ct].surface)
				return FALSE;

			textures[ct].texture = GetDirect3DTexture2Interface(textures[ct].surface);

			if (!textures[ct].texture)
				return FALSE;

			textures[ct].depth = 8;
			textures[ct].emulated = emulate_8bit;
			textures[ct].palette2 = -1;
			textures[ct].emultexture = NULL;
			textures[ct].texture2 = NULL;

			AddPalette(ct);
		}
	}

	return TRUE;
}

// [0x4225ab] [D]
LPDIRECT3DTEXTURE2 GetDirect3DTexture2Interface(LPDIRECTDRAWSURFACE4 lpSurf)
{
	LPVOID ppvObj;

	OutputDebugMessage("GetDirect3DTexture2Interface( )");

	if (!lpSurf)
		return NULL;

	if (lpSurf->lpVtbl->QueryInterface(lpSurf, &IID_IDirect3DTexture2, &ppvObj) == DD_OK)
		return (LPDIRECT3DTEXTURE2)ppvObj;

	OutputDebugMessage("QueryInterface( IID_IDirect3DTexture2 ) failed");
	return NULL;
}

// [0x4225fd] [D]
BOOL Create16BitTexture(int surface)
{
	DDSURFACEDESC2 ddsd;

	if (textures[surface].depth != 16)
	{
		ReleaseTexture(surface);

		// a 16 bit texture takes the memory of two 8 bit ones
		if (!emulate_8bit)
			ReleaseTexture(surface + 1);

		memset(&ddsd, 0, sizeof(ddsd));
		ddsd.dwSize = sizeof(DDSURFACEDESC2);
		ddsd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT;
		ddsd.ddsCaps.dwCaps = current_3d_device->field_520;
		ddsd.ddsCaps.dwCaps2 = current_3d_device->field_524;
		ddsd.ddpfPixelFormat = current_3d_device->pfTex16;
		ddsd.dwWidth = 256;
		ddsd.dwHeight = 256;

		textures[surface].surface = CreateDirectDrawSurface(lpDD, &ddsd);

		if (!textures[surface].surface)
			return FALSE;

		textures[surface].texture = GetDirect3DTexture2Interface(textures[surface].surface);

		if (!textures[surface].texture)
			return FALSE;

		textures[surface].depth = 16;
		textures[surface].emulated = 0;
		textures[surface].palette2 = -1;
		textures[surface].emultexture = NULL;
		textures[surface].texture2 = NULL;
	}

	return TRUE;
}

// [0x42275e] [D]
// NOTE: IDA shows an int return (a leftover register value); nothing uses it
void ReleaseTexture(int texture)
{
	if (textures[texture].depth == -1)
		return;

	OutputDebugMessage("Releasing texture");

	if (textures[texture].palette)
	{
		textures[texture].palette->lpVtbl->Release(textures[texture].palette);
		textures[texture].palette = NULL;
	}

	if (textures[texture].texture)
	{
		if (last_texture == textures[texture].texture)
			last_texture = NULL;

		textures[texture].texture->lpVtbl->Release(textures[texture].texture);
		textures[texture].texture = NULL;
	}

	if (textures[texture].surface)
	{
		textures[texture].surface->lpVtbl->Release(textures[texture].surface);
		textures[texture].surface = NULL;
	}

	textures[texture].depth = -1;
	textures[texture].emulated = 0;
	textures[texture].palette2 = -1;
	textures[texture].emultexture = NULL;
	textures[texture].texture2 = NULL;
}

// [0x4228a8] [D]
BOOL AddPalette(int texture)
{
	if (emulate_8bit)
		return TRUE;

	if (textures[texture].depth == 8)
	{
		if (!textures[texture].palette &&
			lpDD->lpVtbl->CreatePalette(lpDD, DDPCAPS_8BIT | DDPCAPS_ALLOW256, pal, &textures[texture].palette, NULL) != DD_OK)
		{
			OutputDebugMessage("CreatePalette failed");
			return FALSE;
		}

		if (textures[texture].surface->lpVtbl->SetPalette(textures[texture].surface, textures[texture].palette) != DD_OK)
		{
			OutputDebugMessage("SetPalette failed");
			return FALSE;
		}

		if (textures[texture].palette->lpVtbl->SetEntries(textures[texture].palette, 0, 0, 256, pal) != DD_OK)
		{
			OutputDebugMessage("SetEntries failed");
			return FALSE;
		}

		textures[texture].texture->lpVtbl->PaletteChanged(textures[texture].texture, 0, 256);
	}

	return TRUE;
}

// [0x4229de] [D] [A]
// Debug output, compiled out in the original build.
// NOTE: [A] logs to debug.log when built with OPENDRIVER_DEBUG (Debug configuration)
void OutputDebugMessage(char *text)
{
#ifdef OPENDRIVER_DEBUG
	extern void LogQuick(char *text);

	if (text)
		LogQuick(text);
#endif
}

// [0x4229e3] [D]
int GetEmulatedTexture(int tset, EMULTEXTURE **emultexture)
{
	EMULTEXTURE *et;

	if (emulate_8bit)
	{
		for (et = emulated_textures; et; et = et->prev)
		{
			if (et->tset == tset)
			{
				*emultexture = et;
				return 1;
			}
		}
	}

	return 0;
}

// [0x422a2a] [D]
// Adds a palette copy at the end of a texture set's list.
// NOTE: IDA shows a __usercall with a garbage ebx argument
int AttachEmulatedTexture(EMULTEXTURE *emultexture, unsigned char *data, int palette, char *palette_data)
{
	EMULTEXTURE *emultexture8;
	DDSURFACEDESC2 ddsd;

	while (emultexture->next)
		emultexture = emultexture->next;

	emultexture8 = (EMULTEXTURE *)malloc(sizeof(EMULTEXTURE));

	if (!emultexture8)
	{
		OutputDebugMessage("Out of memory");
		return 0;
	}

	memset(emultexture8, 0, 4);

	if (!CreateEmulatedSurface(emultexture8))
	{
		OutputDebugMessage("CreateEmulatedTexture( ) failed");
		free(emultexture8);
		return 0;
	}

	emultexture8->tset = emultexture->tset;
	emultexture8->palette = palette;
	emultexture8->unk_4 = (int)palette_data;
	emultexture8->next = NULL;
	emultexture8->prev = NULL;

	emultexture->next = emultexture8;

	memset(&ddsd, 0, sizeof(ddsd));
	ddsd.dwSize = sizeof(DDSURFACEDESC2);

	if (emultexture8->surface->lpVtbl->Lock(emultexture8->surface, NULL, &ddsd, DDLOCK_WAIT, NULL) != DD_OK)
	{
		OutputDebugMessage("Lock( emultexture8 ) failed");
		return 0;
	}

	BlitMipmap8(data, palette_data, (unsigned short *)ddsd.lpSurface, ddsd.lPitch);

	emultexture8->surface->lpVtbl->Unlock(emultexture8->surface, NULL);

	return 1;
}

// [0x422b8a] [D]
// A new texture set at the head of emulated_textures, with its first palette copy.
// NOTE: IDA shows a __usercall with a garbage ebx argument
int CreateEmulatedTexture(int tset, unsigned char *data, int palette, char *palette_data)
{
	EMULTEXTURE *emultexture8;
	DDSURFACEDESC2 ddsd;

	emultexture8 = (EMULTEXTURE *)malloc(sizeof(EMULTEXTURE));

	if (!emultexture8)
	{
		OutputDebugMessage("Out of memory");
		return 0;
	}

	memset(emultexture8, 0, 4);

	if (!CreateEmulatedSurface(emultexture8))
	{
		OutputDebugMessage("CreateEmulatedTexture( ) failed");
		free(emultexture8);
		return 0;
	}

	emultexture8->tset = tset;
	emultexture8->palette = palette;
	emultexture8->unk_4 = (int)palette_data;
	emultexture8->next = NULL;
	emultexture8->prev = emulated_textures;

	emulated_textures = emultexture8;

	memset(&ddsd, 0, sizeof(ddsd));
	ddsd.dwSize = sizeof(DDSURFACEDESC2);

	if (emultexture8->surface->lpVtbl->Lock(emultexture8->surface, NULL, &ddsd, DDLOCK_WAIT, NULL) != DD_OK)
	{
		OutputDebugMessage("Lock( emultexture8 ) failed");
		return 0;
	}

	BlitMipmap8(data, palette_data, (unsigned short *)ddsd.lpSurface, ddsd.lPitch);

	emultexture8->surface->lpVtbl->Unlock(emultexture8->surface, NULL);

	return 1;
}

// [0x422cd5] [D]
// A 256x256 16 bit texture surface for an emulated 8 bit texture.
int CreateEmulatedSurface(EMULTEXTURE *emultexture)
{
	DDSURFACEDESC2 ddsd;

	memset(&ddsd, 0, sizeof(ddsd));
	ddsd.dwSize = sizeof(DDSURFACEDESC2);
	ddsd.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT;
	ddsd.ddsCaps.dwCaps = current_3d_device->field_520;
	ddsd.ddsCaps.dwCaps2 = current_3d_device->field_524;
	ddsd.ddpfPixelFormat = current_3d_device->pfTex16;
	ddsd.dwWidth = 256;
	ddsd.dwHeight = 256;

	emultexture->surface = CreateDirectDrawSurface(lpDD, &ddsd);

	if (!emultexture->surface)
	{
		OutputDebugMessage("CreateEmulatedSurface( ) failed");
		return 0;
	}

	emultexture->unk_3 = 0;
	emultexture->texture = GetDirect3DTexture2Interface(emultexture->surface);

	if (!emultexture->texture)
	{
		emultexture->surface->lpVtbl->Release(emultexture->surface);
		emultexture->surface = NULL;

		OutputDebugMessage("CreateEmulatedSurface( ) failed ( 2 )");
		return 0;
	}

	return 1;
}

// [0x422dce] [D]
// Converts new 8 bit data into every palette copy of a texture set.
void LoadEmulatedMipmap8(EMULTEXTURE *emultexture8, void *data)
{
	DDSURFACEDESC2 ddsd;

	while (emultexture8)
	{
		memset(&ddsd, 0, sizeof(ddsd));
		ddsd.dwSize = sizeof(DDSURFACEDESC2);

		if (emultexture8->surface->lpVtbl->Lock(emultexture8->surface, NULL, &ddsd, DDLOCK_WAIT, NULL) != DD_OK)
		{
			OutputDebugMessage("Lock( emultexture8 ) failed");
		}
		else
		{
			BlitMipmap8((unsigned char *)data, (char *)emultexture8->unk_4, (unsigned short *)ddsd.lpSurface, ddsd.lPitch);
			emultexture8->surface->lpVtbl->Unlock(emultexture8->surface, NULL);
		}

		emultexture8 = emultexture8->next;
	}
}

// [0x422e8a] [D]
// Copies 16 bit data into the palette copies of the current palette.
void LoadEmulatedMipmap16(EMULTEXTURE *emultexture8, void *data)
{
	char *dst;
	char *src;
	int i;
	DDSURFACEDESC2 ddsd;

	src = (char *)data;

	while (emultexture8)
	{
		if (emultexture8->palette == current_palette)
		{
			memset(&ddsd, 0, sizeof(ddsd));
			ddsd.dwSize = sizeof(DDSURFACEDESC2);

			if (emultexture8->surface->lpVtbl->Lock(emultexture8->surface, NULL, &ddsd, DDLOCK_WAIT, NULL) != DD_OK)
			{
				OutputDebugMessage("Lock( emultexture8 ) failed");
			}
			else
			{
				dst = (char *)ddsd.lpSurface;

				for (i = 0; i < 256; i++)
				{
					CopyMem_glide2x(dst, src, 512);

					dst += 2 * (ddsd.lPitch >> 1);
					src += 512;
				}

				emultexture8->surface->lpVtbl->Unlock(emultexture8->surface, NULL);
			}
		}

		emultexture8 = emultexture8->next;
	}
}

// [0x422f99] [D]
// Converts a 256x256 8 bit image through a palette (0x00RRGGBB entries) into the 16 bit format
// of the device (rbits / rshift ...: bits dropped from 8, position).
// NOTE: assembly in the original; it also ORs in the alpha bits for a palette index of -1,
// which a byte never is
void BlitMipmap8(unsigned char *src, char *palette, unsigned short *dst, int pitch)
{
	unsigned char *entry;
	int y;
	int x;

	for (y = 256; y; y--)
	{
		for (x = 256; x; x--)
		{
			entry = (unsigned char *)palette + *src++ * 4;

			*dst = 0;
			*dst |= (entry[2] >> rbits) << rshift;
			*dst |= (entry[1] >> gbits) << gshift;
			*dst |= (entry[0] >> bbits) << bshift;
			dst++;
		}

		dst = (unsigned short *)((char *)dst + pitch - 512);
	}
}

// [0x42305f] [D]
// NOTE: "CopyMem" in IDA, a local copy of asmmath's (rep movsd, then rep movsb); memcpy does the
// same
static void CopyMem_glide2x(void *dest, void *src, unsigned int count)
{
	memcpy(dest, src, count);
}

// [0x423086] [D]
// Draws the triangles batched per texture set (vb[32] holds untextured ones).
void FlushBatchedPrims(void)
{
	LPDIRECT3DTEXTURE2 old_texture;
	int old_palette;
	DWORD state;
	int old_emulated;
	int tset;

	old_texture = last_texture;
	old_emulated = emulated_texture;
	old_palette = current_palette;

	lpD3DDev->lpVtbl->GetTextureStageState(lpD3DDev, 0, D3DTSS_COLOROP, &state);

	for (tset = 0; tset < MAX_TEXTURES + 1; tset++)
	{
		if (vbcount[tset])
		{
			if (tset == MAX_TEXTURES)
			{
				lpD3DDev->lpVtbl->SetTextureStageState(lpD3DDev, 0, D3DTSS_COLOROP, D3DTOP_DISABLE);
				lpD3DDev->lpVtbl->SetTexture(lpD3DDev, 0, NULL);
			}
			else if (textures[tset].emulated)
			{
				SetEmulatedTexture(tset);
			}
			else
			{
				lpD3DDev->lpVtbl->SetTexture(lpD3DDev, 0, textures[tset].texture);
				emulated_texture = -1;
			}

			lpD3DDev->lpVtbl->DrawPrimitive(lpD3DDev, D3DPT_TRIANGLELIST, D3DFVF_TLVERTEX, vb[tset], vbcount[tset], POLY_FLAGS);
			vbcount[tset] = 0;
		}
	}

	last_texture = old_texture;
	emulated_texture = old_emulated;
	current_palette = old_palette;

	lpD3DDev->lpVtbl->SetTextureStageState(lpD3DDev, 0, D3DTSS_COLOROP, state);
}

// [0x4231f7] [D]
// Selects the emulated 16 bit copy of an 8 bit texture for the current palette.
void SetEmulatedTexture(int tset)
{
	EMULTEXTURE *emultexture8;
	EMULTEXTURE *parent;

	emulated_texture = tset;

	if (textures[tset].palette2 != current_palette)
	{
		textures[tset].texture2 = NULL;

		emultexture8 = textures[tset].emultexture;
		parent = emultexture8;

		while (emultexture8)
		{
			if (emultexture8->palette == current_palette)
			{
				textures[tset].texture2 = emultexture8->texture;
				break;
			}

			emultexture8 = emultexture8->next;
		}

		if (!textures[tset].texture2)
			textures[tset].texture2 = parent->texture;

		textures[tset].palette2 = current_palette;
	}

	lpD3DDev->lpVtbl->SetTexture(lpD3DDev, 0, textures[tset].texture2);
	last_texture = textures[tset].texture2;
}
