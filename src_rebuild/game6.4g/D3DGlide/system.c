/*
 * D3DGlide/system.c - Glide / Direct3D device selection and engine wrappers around the Glide API
 *
 * Rebuilt from Driver_sym2_94.i64 (IDA folder "D3DGlide\system.c").
 */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#include "dr_types.h"
#include "D3DGlide/system.h"
#include "Engine/3dfx/gfxinit.h"
#include "shell.h"

#include "d3dglide.h"

// the Glide pointer table is defined here
// NOTE: owner by layout not established; devices_3d sits in the middle of the table (0x5cada0),
// so they belong to the same object
#define GLIDE_EXTERN
#include "glide_ptrs.h"

#define MAX_3D_DEVICES		8

#define DEVICE_TYPE_GLIDE	0	// 3dfx glide2x.dll
#define DEVICE_TYPE_D3D		1	// Direct3D emulation

DEVICE_3D devices_3d[MAX_3D_DEVICES];

void grReleaseEmulatedTextures(void);

// [0x424ac0] [D]
// Finds the 3D devices once, then selects one (falls back to the first).
void grSetDevice(int device, int no8BitSupport, int tripleBuffering)
{
	int type;

	if (!got_3d_hardware)
	{
		Init3DDevices();
		InitGlideDevice();

		if (num_3d_devices > 0)
			got_3d_hardware = 1;
	}

	if (device < 0 || device >= num_3d_devices)
		device = 0;

	type = devices_3d[device].type;

	if (type == DEVICE_TYPE_GLIDE)
	{
		got_3d_device = UseGlideDevice();
		device_type = DEVICE_TYPE_GLIDE;
	}
	else if (type == DEVICE_TYPE_D3D)
	{
		got_3d_device = UseD3DDevice(device, no8BitSupport, tripleBuffering);
		device_type = DEVICE_TYPE_D3D;
	}
	else
	{
		got_3d_device = 0;
		device_type = DEVICE_TYPE_GLIDE;
	}
}

// [0x424b72] [D]
int grGetDevice(void)
{
	return device_type;
}

// [0x424b7c] [D]
// Copies the name of 3D device `device` (the first if out of range); returns the device used,
// -1 without a device.
// NOTE: unreferenced (only its thunk is left)
int grGetDeviceName(int device, char *name)
{
	if (!got_3d_device)
		return -1;

	if (device < 0 || device >= num_3d_devices)
		device = 0;

	sprintf(name, devices_3d[device].name);
	return device;
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

// [0x424c80] [D]
void grGlideReallyShutdown(void)
{
	int i;

	grReleaseEmulatedTextures();

	if (got_3d_hardware)
	{
		for (i = 0; i < num_3d_devices; i++)
		{
			if (devices_3d[i].field_48)
				free(devices_3d[i].field_48);
		}

		got_3d_hardware = 0;
		got_3d_device = 0;
		num_3d_devices = 0;
	}

	FreeGlide();
}

// [0x424cfc] [D]
// Locks a buffer for writing and reports its pixel format in info (see LFB_INFO).
FxBool grLfbLock(GrBuffer_t buffer, LFB_INFO *info)
{
	FxBool res;

	if (!got_3d_device)
		return FXFALSE;

	if (device_type == DEVICE_TYPE_GLIDE)
	{
		res = gr_LfbLock(GR_LFB_WRITE_ONLY, buffer, GR_LFBWRITEMODE_565, glide_origin, FXFALSE, &info->info);

		// RGB565
		info->depth = 16;
		info->rbits = 3;
		info->gbits = 2;
		info->bbits = 3;
		info->rshift = 11;
		info->gshift = 5;
		info->bshift = 0;

		return res;
	}

	if (device_type == DEVICE_TYPE_D3D)
		return d3d_LfbLock(buffer, info);

	return FXFALSE;
}

// [0x424d9d] [D]
FxBool grLfbUnlock(GrBuffer_t buffer)
{
	if (got_3d_device)
		return gr_LfbUnlock(GR_LFB_WRITE_ONLY, buffer);

	return FXFALSE;
}

// [0x424dbb] [D]
// The 16 bit copy of an 8 bit texture set with one palette, for Direct3D devices without 8 bit
// textures (map.c AllocateTexture).
int grCreateEmulatedTexture(int tset, char *data, int palette, char *palette_data)
{
	if (device_type == DEVICE_TYPE_D3D)
		return d3d_CreateEmulatedTexture(tset, (unsigned char *)data, palette, palette_data);

	return 1;
}

// [0x424de8] [D]
void grReleaseEmulatedTextures(void)
{
	if (device_type == DEVICE_TYPE_D3D)
		d3d_ReleaseEmulatedTextures();
}

// [0x424dfb] [D]
// Texture download that also passes the texture set (for the emulated 8 bit textures).
void grTexDownloadMipMap2(GrChipID_t tmu, FxU32 startAddress, FxU32 evenOdd, GrTexInfo *info, int tset)
{
	if (got_3d_device)
	{
		if (device_type == DEVICE_TYPE_GLIDE)
			grTexDownloadMipMap(tmu, startAddress, evenOdd, info);
		else if (device_type == DEVICE_TYPE_D3D)
			d3d_TexDownloadMipMap(tmu, startAddress, evenOdd, info, tset);
	}
}

// [0x424e56] [D]
// New data for an 8 bit texture set in texture memory.
void ReplaceEmulatedTexture8(GrChipID_t tmu, FxU32 startAddress, FxU32 evenOdd, GrTexInfo *info, int tset)
{
	if (!got_3d_device)
		return;

	if (device_type == DEVICE_TYPE_GLIDE)
		grTexDownloadMipMap(tmu, startAddress, evenOdd, info);
	else if (device_type == DEVICE_TYPE_D3D)
		d3d_TexDownload8(tmu, startAddress, evenOdd, info, tset);
}

// [0x424eb1] [D]
// Palette download that also passes the palette number (for the emulated 8 bit textures).
void grTexDownloadTable2(GrChipID_t tmu, GrTexTable_t type, void *data, int palette)
{
	if (got_3d_device)
	{
		if (device_type == DEVICE_TYPE_GLIDE)
			grTexDownloadTable(tmu, type, data);
		else if (device_type == DEVICE_TYPE_D3D)
			d3d_TexDownloadTable(tmu, type, data, palette);
	}
}

// [0x424f04] [D]
// NOTE: IDA shows a HARDWARE_SETTINGS * return (leftover register); nothing uses it
void GetHWSettings(HARDWARE_SETTINGS *settings)
{
	if (got_3d_device)
	{
		if (device_type == DEVICE_TYPE_GLIDE)
		{
			// 3dfx: ARGB1555
			settings->rbits = 3;
			settings->rshift = 10;
			settings->gbits = 3;
			settings->gshift = 5;
			settings->bbits = 3;
			settings->bshift = 0;
			settings->emulate_8bit = 0;
			settings->device_flags = -1;
			settings->texlookup16 = NULL;
		}
		else if (device_type == DEVICE_TYPE_D3D)
		{
			d3d_GetHWSettings(settings);
		}
	}
}

// [0x424f92] [D]
void ReplaceEmulatedTexture16(int tset, void *data)
{
	if (got_3d_device && device_type == DEVICE_TYPE_D3D)
		d3d_TexDownload16(tset, data);
}
