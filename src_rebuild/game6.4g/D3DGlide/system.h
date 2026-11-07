/*
 * D3DGlide/system.h - globals and functions of D3DGlide/system.c used by other files
 */

#ifndef D3DGLIDE_SYSTEM_H
#define D3DGLIDE_SYSTEM_H

#include "dr_types.h"

#include "D3DGlide/d3dglide.h"

// globals
extern DEVICE_3D devices_3d[8];

// functions
extern void GetHWSettings(HARDWARE_SETTINGS *settings);
extern void ReplaceEmulatedTexture16(int tset, void *data);
extern void ReplaceEmulatedTexture8(GrChipID_t tmu, FxU32 startAddress, FxU32 evenOdd, GrTexInfo *info, int tset);
// NOTE: IDA types all four arguments int: tset, texture data, palette number, palette data
extern int grCreateEmulatedTexture(int tset, char *data, int palette, char *palette_data);
extern int grGetDevice(void);
extern void grGlideInit(void);
extern void grGlideReallyShutdown(void);
extern void grGlideShutdown(void);
extern FxBool grLfbLock(GrBuffer_t buffer, LFB_INFO *info);
extern FxBool grLfbUnlock(GrBuffer_t buffer);
extern void grReleaseEmulatedTextures(void);
extern void grSetDevice(int device, int no8BitSupport, int tripleBuffering);
extern FxBool grSstControl(FxU32 code);
extern FxBool FX_CALL grSstQueryHardware(GrHwConfiguration *hwconfig);
extern FxBool FX_CALL grSstWinOpen(FxU32 hWnd, GrScreenResolution_t screen_resolution, GrScreenRefresh_t refresh_rate, GrColorFormat_t color_format, GrOriginLocation_t origin_location, int nColBuffers, int nAuxBuffers);
extern void grTexDownloadMipMap2(GrChipID_t tmu, FxU32 startAddress, FxU32 evenOdd, GrTexInfo *info, int tset);
extern void grTexDownloadTable2(GrChipID_t tmu, GrTexTable_t type, void *data, int palette);

#endif // D3DGLIDE_SYSTEM_H
