/*
 * D3DGlide/d3dglide.h - [A] the Direct3D Glide emulation (glide2x.c)
 *
 * Not an original file: d3dgl_X / d3dgu_X implement grX / guX on Direct3D (DirectX 6 interfaces).
 * Generated once from the glide.h prototypes and UseD3DDevice; maintained by hand since.
 */

#ifndef D3DGLIDE_H
#define D3DGLIDE_H

#define FX_GLIDE_NO_FUNC_PROTO
#include <glide.h>

#include "dr_types.h"

// grLfbLock's info: a GrLfbInfo_t followed by the frame buffer's pixel format
// NOTE: not a named type in IDA (InitCPUPolyInterface passes a 28 byte buffer); name guessed
typedef struct LFB_INFO
{
	GrLfbInfo_t info;
	unsigned char depth;
	unsigned char rbits;		// GetBits() of the colour masks
	unsigned char gbits;
	unsigned char bbits;
	unsigned char rshift;		// GetShift() of the colour masks
	unsigned char gshift;
	unsigned char bshift;
	unsigned char pad;
} LFB_INFO;

int Init3DDevices(void);
FxBool d3d_LfbLock(GrBuffer_t buffer, LFB_INFO *info);
int UseD3DDevice(int device, int no8BitSupport, int tripleBuffering);
void d3d_GetHWSettings(HARDWARE_SETTINGS *settings);
void d3d_SetScreenScale(float scale);
void d3d_ReleaseEmulatedTextures(void);
void d3d_TexDownloadMipMap(GrChipID_t tmu, FxU32 startAddress, FxU32 evenOdd, GrTexInfo *info, int tset);
void d3d_TexDownloadTable(GrChipID_t tmu, GrTexTable_t type, void *data, int palette);

void FX_CALL d3dgl_AADrawLine(const GrVertex *v1, const GrVertex *v2);
void FX_CALL d3dgl_AADrawPoint(const GrVertex *pt);
void FX_CALL d3dgl_AADrawPolygon(const int nverts, const int ilist[], const GrVertex vlist[]);
void FX_CALL d3dgl_AADrawPolygonVertexList(const int nverts, const GrVertex vlist[]);
void FX_CALL d3dgl_AADrawTriangle(const GrVertex *a, const GrVertex *b, const GrVertex *c, FxBool ab_antialias, FxBool bc_antialias, FxBool ca_antialias);
void FX_CALL d3dgl_AlphaBlendFunction(GrAlphaBlendFnc_t rgb_sf, GrAlphaBlendFnc_t rgb_df, GrAlphaBlendFnc_t alpha_sf, GrAlphaBlendFnc_t alpha_df);
void FX_CALL d3dgl_AlphaCombine(GrCombineFunction_t function, GrCombineFactor_t factor, GrCombineLocal_t local, GrCombineOther_t other, FxBool invert);
void FX_CALL d3dgl_AlphaControlsITRGBLighting(FxBool enable);
void FX_CALL d3dgl_AlphaTestFunction(GrCmpFnc_t function);
void FX_CALL d3dgl_AlphaTestReferenceValue(GrAlpha_t value);
void FX_CALL d3dgl_BufferClear(GrColor_t color, GrAlpha_t alpha, FxU16 depth);
int FX_CALL d3dgl_BufferNumPending(void);
void FX_CALL d3dgl_BufferSwap(int swap_interval);
void FX_CALL d3dgl_CheckForRoom(FxI32 n);
void FX_CALL d3dgl_ChromakeyMode(GrChromakeyMode_t mode);
void FX_CALL d3dgl_ChromakeyValue(GrColor_t value);
void FX_CALL d3dgl_ClipWindow(FxU32 minx, FxU32 miny, FxU32 maxx, FxU32 maxy);
void FX_CALL d3dgl_ColorCombine(GrCombineFunction_t function, GrCombineFactor_t factor, GrCombineLocal_t local, GrCombineOther_t other, FxBool invert);
void FX_CALL d3dgl_ColorMask(FxBool rgb, FxBool a);
void FX_CALL d3dgl_ConstantColorValue4(float a, float r, float g, float b);
void FX_CALL d3dgl_ConstantColorValue(GrColor_t value);
void FX_CALL d3dgl_CullMode(GrCullMode_t mode);
void FX_CALL d3dgl_DepthBiasLevel(FxI16 level);
void FX_CALL d3dgl_DepthBufferFunction(GrCmpFnc_t function);
void FX_CALL d3dgl_DepthBufferMode(GrDepthBufferMode_t mode);
void FX_CALL d3dgl_DepthMask(FxBool mask);
void FX_CALL d3dgl_DisableAllEffects(void);
void FX_CALL d3dgl_DitherMode(GrDitherMode_t mode);
void FX_CALL d3dgl_DrawLine(const GrVertex *v1, const GrVertex *v2);
void FX_CALL d3dgl_DrawPlanarPolygon(int nverts, const int ilist[], const GrVertex vlist[]);
void FX_CALL d3dgl_DrawPlanarPolygonVertexList(int nverts, const GrVertex vlist[]);
void FX_CALL d3dgl_DrawPoint(const GrVertex *pt);
void FX_CALL d3dgl_DrawPolygon(int nverts, const int ilist[], const GrVertex vlist[]);
void FX_CALL d3dgl_DrawPolygonVertexList(int nverts, const GrVertex vlist[]);
void FX_CALL d3dgl_DrawTriangle(const GrVertex *a, const GrVertex *b, const GrVertex *c);
void FX_CALL d3dgl_FogColorValue(GrColor_t fogcolor);
void FX_CALL d3dgl_FogMode(GrFogMode_t mode);
void FX_CALL d3dgl_FogTable(const GrFog_t ft[64]);
void FX_CALL d3dgl_GammaCorrectionValue(float value);
void FX_CALL d3dgl_GlideGetState(GrState *state);
void FX_CALL d3dgl_GlideGetVersion(char version[80]);
void FX_CALL d3dgl_GlideSetState(const GrState *state);
void FX_CALL d3dgl_GlideShamelessPlug(const FxBool on);
void FX_CALL d3dgl_Hints(GrHint_t hintType, FxU32 hintMask);
void FX_CALL d3dgl_LfbConstantAlpha(GrAlpha_t alpha);
void FX_CALL d3dgl_LfbConstantDepth(FxU16 depth);
FxBool FX_CALL d3dgl_LfbLock(GrLock_t type, GrBuffer_t buffer, GrLfbWriteMode_t writeMode, GrOriginLocation_t origin, FxBool pixelPipeline, GrLfbInfo_t *info);
FxBool FX_CALL d3dgl_LfbReadRegion(GrBuffer_t src_buffer, FxU32 src_x, FxU32 src_y, FxU32 src_width, FxU32 src_height, FxU32 dst_stride, void *dst_data);
FxBool FX_CALL d3dgl_LfbUnlock(GrLock_t type, GrBuffer_t buffer);
void FX_CALL d3dgl_LfbWriteColorFormat(GrColorFormat_t colorFormat);
void FX_CALL d3dgl_LfbWriteColorSwizzle(FxBool swizzleBytes, FxBool swapWords);
FxBool FX_CALL d3dgl_LfbWriteRegion(GrBuffer_t dst_buffer, FxU32 dst_x, FxU32 dst_y, GrLfbSrcFmt_t src_format, FxU32 src_width, FxU32 src_height, FxI32 src_stride, void *src_data);
void FX_CALL d3dgl_RenderBuffer(GrBuffer_t buffer);
void FX_CALL d3dgl_ResetTriStats();
void FX_CALL d3dgl_Splash(float x, float y, float width, float height, FxU32 frame);
void FX_CALL d3dgl_SstIdle(void);
FxBool FX_CALL d3dgl_SstIsBusy(void);
void FX_CALL d3dgl_SstOrigin(GrOriginLocation_t origin);
void FX_CALL d3dgl_SstPerfStats(GrSstPerfStats_t *pStats);
FxBool FX_CALL d3dgl_SstQueryBoards(GrHwConfiguration *hwconfig);
void FX_CALL d3dgl_SstResetPerfStats(void);
FxU32 FX_CALL d3dgl_SstScreenHeight(void);
FxU32 FX_CALL d3dgl_SstScreenWidth(void);
void FX_CALL d3dgl_SstSelect(int which_sst);
FxU32 FX_CALL d3dgl_SstStatus(void);
FxBool FX_CALL d3dgl_SstVRetraceOn(void);
FxU32 FX_CALL d3dgl_SstVideoLine(void);
FxU32 FX_CALL d3dgl_TexCalcMemRequired(GrLOD_t lodmin, GrLOD_t lodmax, GrAspectRatio_t aspect, GrTextureFormat_t fmt);
void FX_CALL d3dgl_TexClampMode(GrChipID_t tmu, GrTextureClampMode_t s_clampmode, GrTextureClampMode_t t_clampmode);
void FX_CALL d3dgl_TexCombine(GrChipID_t tmu, GrCombineFunction_t rgb_function, GrCombineFactor_t rgb_factor, GrCombineFunction_t alpha_function, GrCombineFactor_t alpha_factor, FxBool rgb_invert, FxBool alpha_invert);
void FX_CALL d3dgl_TexCombineFunction(GrChipID_t tmu, GrTextureCombineFnc_t fnc);
void FX_CALL d3dgl_TexDetailControl(GrChipID_t tmu, int lod_bias, FxU8 detail_scale, float detail_max);
void FX_CALL d3dgl_TexDownloadMipMap(GrChipID_t tmu, FxU32 startAddress, FxU32 evenOdd, GrTexInfo *info);
void FX_CALL d3dgl_TexDownloadMipMapLevel(GrChipID_t tmu, FxU32 startAddress, GrLOD_t thisLod, GrLOD_t largeLod, GrAspectRatio_t aspectRatio, GrTextureFormat_t format, FxU32 evenOdd, void *data);
void FX_CALL d3dgl_TexDownloadMipMapLevelPartial(GrChipID_t tmu, FxU32 startAddress, GrLOD_t thisLod, GrLOD_t largeLod, GrAspectRatio_t aspectRatio, GrTextureFormat_t format, FxU32 evenOdd, void *data, int start, int end);
void FX_CALL d3dgl_TexDownloadTable(GrChipID_t tmu, GrTexTable_t type, void *data);
void FX_CALL d3dgl_TexDownloadTablePartial(GrChipID_t tmu, GrTexTable_t type, void *data, int start, int end);
void FX_CALL d3dgl_TexFilterMode(GrChipID_t tmu, GrTextureFilterMode_t minfilter_mode, GrTextureFilterMode_t magfilter_mode);
void FX_CALL d3dgl_TexLodBiasValue(GrChipID_t tmu, float bias);
FxU32 FX_CALL d3dgl_TexMaxAddress(GrChipID_t tmu);
FxU32 FX_CALL d3dgl_TexMinAddress(GrChipID_t tmu);
void FX_CALL d3dgl_TexMipMapMode(GrChipID_t tmu, GrMipMapMode_t mode, FxBool lodBlend);
void FX_CALL d3dgl_TexMultibase(GrChipID_t tmu, FxBool enable);
void FX_CALL d3dgl_TexMultibaseAddress(GrChipID_t tmu, GrTexBaseRange_t range, FxU32 startAddress, FxU32 evenOdd, GrTexInfo *info);
void FX_CALL d3dgl_TexNCCTable(GrChipID_t tmu, GrNCCTable_t table);
void FX_CALL d3dgl_TexSource(GrChipID_t tmu, FxU32 startAddress, FxU32 evenOdd, GrTexInfo *info);
FxU32 FX_CALL d3dgl_TexTextureMemRequired(FxU32 evenOdd, GrTexInfo *info);
void FX_CALL d3dgl_TriStats(FxU32 *trisProcessed, FxU32 *trisDrawn);
FxBool FX_CALL d3dgu_3dfGetInfo(const char *filename, Gu3dfInfo *info);
FxBool FX_CALL d3dgu_3dfLoad(const char *filename, Gu3dfInfo *data);
void FX_CALL d3dgu_AADrawTriangleWithClip(const GrVertex *a, const GrVertex *b, const GrVertex *c);
void FX_CALL d3dgu_AlphaSource(GrAlphaSource_t mode);
void FX_CALL d3dgu_ColorCombineFunction(GrColorCombineFnc_t fnc);
void FX_CALL d3dgu_DrawPolygonVertexListWithClip(int nverts, const GrVertex vlist[]);
void FX_CALL d3dgu_DrawTriangleWithClip(const GrVertex *a, const GrVertex *b, const GrVertex *c);
int FX_CALL d3dgu_EncodeRLE16(void *dst, void *src, FxU32 width, FxU32 height);
FxU16 FX_CALL d3dgu_EndianSwapBytes(FxU16 value);
FxU32 FX_CALL d3dgu_EndianSwapWords(FxU32 value);
void FX_CALL d3dgu_FogGenerateExp2(GrFog_t fogtable[64], float density);
void FX_CALL d3dgu_FogGenerateExp(GrFog_t fogtable[64], float density);
void FX_CALL d3dgu_FogGenerateLinear(GrFog_t fogtable[64], float nearZ, float farZ);
float FX_CALL d3dgu_FogTableIndexToW(int i);
GrMipMapId_t FX_CALL d3dgu_TexAllocateMemory(GrChipID_t tmu, FxU8 odd_even_mask, int width, int height, GrTextureFormat_t fmt, GrMipMapMode_t mm_mode, GrLOD_t smallest_lod, GrLOD_t largest_lod, GrAspectRatio_t aspect, GrTextureClampMode_t s_clamp_mode, GrTextureClampMode_t t_clamp_mode, GrTextureFilterMode_t minfilter_mode, GrTextureFilterMode_t magfilter_mode, float lod_bias, FxBool trilinear);
FxBool FX_CALL d3dgu_TexChangeAttributes(GrMipMapId_t mmid, int width, int height, GrTextureFormat_t fmt, GrMipMapMode_t mm_mode, GrLOD_t smallest_lod, GrLOD_t largest_lod, GrAspectRatio_t aspect, GrTextureClampMode_t s_clamp_mode, GrTextureClampMode_t t_clamp_mode, GrTextureFilterMode_t minFilterMode, GrTextureFilterMode_t magFilterMode);
void FX_CALL d3dgu_TexCombineFunction(GrChipID_t tmu, GrTextureCombineFnc_t fnc);
FxU16 * FX_CALL d3dgu_TexCreateColorMipMap(void);
void FX_CALL d3dgu_TexDownloadMipMap(GrMipMapId_t mmid, const void *src, const GuNccTable *table);
void FX_CALL d3dgu_TexDownloadMipMapLevel(GrMipMapId_t mmid, GrLOD_t lod, const void **src);
GrMipMapId_t FX_CALL d3dgu_TexGetCurrentMipMap(GrChipID_t tmu);
GrMipMapInfo * FX_CALL d3dgu_TexGetMipMapInfo(GrMipMapId_t mmid);
FxU32 FX_CALL d3dgu_TexMemQueryAvail(GrChipID_t tmu);
void FX_CALL d3dgu_TexMemReset(void);
void FX_CALL d3dgu_TexSource(GrMipMapId_t id);
void FX_CALL d3dgl_GlideInit(void);
void FX_CALL d3dgl_GlideShutdown(void);
FxBool FX_CALL d3dgl_SstQueryHardware(GrHwConfiguration *hwconfig);
FxBool FX_CALL d3dgl_SstWinOpen(FxU32 hWnd, GrScreenResolution_t screen_resolution, GrScreenRefresh_t refresh_rate, GrColorFormat_t color_format, GrOriginLocation_t origin_location, int nColBuffers, int nAuxBuffers);
void FX_CALL d3dgl_SstWinClose(void);
FxBool FX_CALL d3dgl_SstControl(FxU32 code);
void FX_CALL d3dgl_DrawLine_REAL3D(const GrVertex *v1, const GrVertex *v2);
void FX_CALL d3dgl_DrawPolygonVertexList_REAL3D(int nverts, const GrVertex vlist[]);
void FX_CALL d3dgl_DrawTriangle_REAL3D(const GrVertex *a, const GrVertex *b, const GrVertex *c);
void FX_CALL d3dgl_DrawTriangleWithClip_REAL3D(const GrVertex *a, const GrVertex *b, const GrVertex *c);

#endif
