/*
 * D3DGlide/system.c - device selection and engine wrappers around the Glide API
 *
 * Rebuilt from Driver_sym2_94.i64 (IDA folder "D3DGlide\system.c").
 */

#include <stdio.h>
#include <stdlib.h>

#include "dr_types.h"
#include "D3DGlide/system.h"
#include "Engine/3dfx/gfxinit.h"
#include "shell.h"

#include "d3dglide.h"
#include "glide_gl.h"

// the Glide pointer table is defined here
#define GLIDE_EXTERN
#include "glide_ptrs.h"

void grReleaseEmulatedTextures(void);

// [0x424ac0] [D] [A]
// NOTE: [A] the OpenGL device is the only one, there are none to look for and the arguments have no use
void grSetDevice(int device, int no8BitSupport, int tripleBuffering)
{
	got_3d_hardware = 1;
	got_3d_device = UseGLDevice();
	device_type = DEVICE_TYPE_GL;
}

// [0x424b72] [D]
int grGetDevice(void)
{
	return device_type;
}

// [0x424bc3] [D]
void grGlideInit(void)
{
	if (got_3d_device)
		gr_GlideInit();
}

// [0x424bd7] [D]
void grGlideShutdown(void)
{
	if (got_3d_device)
	{
		gr_SstWinClose();
		gr_GlideShutdown();
	}
}

// [0x424bf1] [D]
FxBool FX_CALL grSstQueryHardware(GrHwConfiguration *hwconfig)
{
	if (got_3d_device)
		return gr_SstQueryHardware(hwconfig);

	return FXFALSE;
}

// [0x424c0f] [D]
FxBool FX_CALL grSstWinOpen(FxU32 hWnd, GrScreenResolution_t screen_resolution, GrScreenRefresh_t refresh_rate,
	GrColorFormat_t color_format, GrOriginLocation_t origin_location, int nColBuffers, int nAuxBuffers)
{
	if (!got_3d_device)
		return FXFALSE;

	glide_origin = origin_location;

	return gr_SstWinOpen(hWnd, screen_resolution, refresh_rate, color_format, origin_location, nColBuffers, nAuxBuffers);
}

// [0x424c4d] [D]
// NOTE: unreferenced (only its thunk is left)
void FX_CALL grSstWinClose(void)
{
	if (got_3d_device)
		gr_SstWinClose();
}

// [0x424c61] [D]
FxBool grSstControl(FxU32 code)
{
	if (got_3d_device)
		return gr_SstControl(code);

	return FXTRUE;
}

// [0x424c80] [D] [A]
// NOTE: [A] no device list to free and no glide2x.dll
void grGlideReallyShutdown(void)
{
	grReleaseEmulatedTextures();

	got_3d_hardware = 0;
	got_3d_device = 0;
}

// [0x424cfc] [D] [A]
// Locks a buffer for writing and reports its pixel format in info (see LFB_INFO).
// NOTE: [A] the OpenGL device only
FxBool grLfbLock(GrBuffer_t buffer, LFB_INFO *info)
{
	if (!got_3d_device)
		return FXFALSE;

	return gl_LfbLockInfo(buffer, info);
}

// [0x424d9d] [D]
FxBool grLfbUnlock(GrBuffer_t buffer)
{
	if (got_3d_device)
		return gr_LfbUnlock(GR_LFB_WRITE_ONLY, buffer);

	return FXFALSE;
}

// [0x424dbb] [D] [A]
// The 16 bit copy of an 8 bit texture set with one palette (map.c AllocateTexture).
// NOTE: [A] the OpenGL device only
int grCreateEmulatedTexture(int tset, char *data, int palette, char *palette_data)
{
	return gl_CreateEmulatedTexture(tset, (unsigned char *)data, palette, palette_data);
}

// [0x424de8] [D] [A]
// NOTE: [A] the OpenGL device only
void grReleaseEmulatedTextures(void)
{
	gl_ReleaseEmulatedTextures();
}

// [0x424dfb] [D] [A]
// Texture download that also passes the texture set (for the emulated 8 bit textures).
// NOTE: [A] the OpenGL device only
void grTexDownloadMipMap2(GrChipID_t tmu, FxU32 startAddress, FxU32 evenOdd, GrTexInfo *info, int tset)
{
	if (got_3d_device)
		gl_TexDownloadMipMapSet(tmu, startAddress, evenOdd, info, tset);
}

// [0x424e56] [D] [A]
// New data for an 8 bit texture set in texture memory.
// NOTE: [A] the OpenGL device only
void ReplaceEmulatedTexture8(GrChipID_t tmu, FxU32 startAddress, FxU32 evenOdd, GrTexInfo *info, int tset)
{
	if (got_3d_device)
		gl_TexDownload8(tmu, startAddress, evenOdd, info, tset);
}

// [0x424eb1] [D] [A]
// Palette download that also passes the palette number (for the emulated 8 bit textures).
// NOTE: [A] the OpenGL device only
void grTexDownloadTable2(GrChipID_t tmu, GrTexTable_t type, void *data, int palette)
{
	if (got_3d_device)
		gl_TexDownloadPalette(palette, data);
}

// [0x424f04] [D] [A]
// NOTE: IDA shows a HARDWARE_SETTINGS * return (leftover register); nothing uses it
// NOTE: [A] the OpenGL device only
void GetHWSettings(HARDWARE_SETTINGS *settings)
{
	if (got_3d_device)
		gl_GetHWSettings(settings);
}

// [0x424f92] [D] [A]
// NOTE: [A] the OpenGL device only
void ReplaceEmulatedTexture16(int tset, void *data)
{
	if (got_3d_device)
		gl_TexDownload16(tset, data);
}
