/*
 * D3DGlide/glide_ptrs.h - [A] the Glide API as function pointers
 *
 * Not an original file: the game calls Glide through pointers, filled by the OpenGL device
 * (UseGLDevice). Generated once from the glide.h prototypes; maintained by hand since.
 * Define GLIDE_EXTERN as empty before including to get the definitions (done in system.c).
 */

#ifndef GLIDE_PTRS_H
#define GLIDE_PTRS_H

#define FX_GLIDE_NO_FUNC_PROTO
#include <glide.h>

#ifndef GLIDE_EXTERN
#define GLIDE_EXTERN extern
#endif

GLIDE_EXTERN void (FX_CALL *grAADrawLine)(const GrVertex *v1, const GrVertex *v2);
GLIDE_EXTERN void (FX_CALL *grAADrawPoint)(const GrVertex *pt);
GLIDE_EXTERN void (FX_CALL *grAADrawPolygon)(const int nverts, const int ilist[], const GrVertex vlist[]);
GLIDE_EXTERN void (FX_CALL *grAADrawPolygonVertexList)(const int nverts, const GrVertex vlist[]);
GLIDE_EXTERN void (FX_CALL *grAADrawTriangle)(const GrVertex *a, const GrVertex *b, const GrVertex *c, FxBool ab_antialias, FxBool bc_antialias, FxBool ca_antialias);
GLIDE_EXTERN void (FX_CALL *grAlphaBlendFunction)(GrAlphaBlendFnc_t rgb_sf, GrAlphaBlendFnc_t rgb_df, GrAlphaBlendFnc_t alpha_sf, GrAlphaBlendFnc_t alpha_df);
GLIDE_EXTERN void (FX_CALL *grAlphaCombine)(GrCombineFunction_t function, GrCombineFactor_t factor, GrCombineLocal_t local, GrCombineOther_t other, FxBool invert);
GLIDE_EXTERN void (FX_CALL *grAlphaControlsITRGBLighting)(FxBool enable);
GLIDE_EXTERN void (FX_CALL *grAlphaTestFunction)(GrCmpFnc_t function);
GLIDE_EXTERN void (FX_CALL *grAlphaTestReferenceValue)(GrAlpha_t value);
GLIDE_EXTERN void (FX_CALL *grBufferClear)(GrColor_t color, GrAlpha_t alpha, FxU16 depth);
GLIDE_EXTERN int (FX_CALL *grBufferNumPending)(void);
GLIDE_EXTERN void (FX_CALL *grBufferSwap)(int swap_interval);
GLIDE_EXTERN void (FX_CALL *grCheckForRoom)(FxI32 n);
GLIDE_EXTERN void (FX_CALL *grChromakeyMode)(GrChromakeyMode_t mode);
GLIDE_EXTERN void (FX_CALL *grChromakeyValue)(GrColor_t value);
GLIDE_EXTERN void (FX_CALL *grClipWindow)(FxU32 minx, FxU32 miny, FxU32 maxx, FxU32 maxy);
GLIDE_EXTERN void (FX_CALL *grColorCombine)(GrCombineFunction_t function, GrCombineFactor_t factor, GrCombineLocal_t local, GrCombineOther_t other, FxBool invert);
GLIDE_EXTERN void (FX_CALL *grColorMask)(FxBool rgb, FxBool a);
GLIDE_EXTERN void (FX_CALL *grConstantColorValue4)(float a, float r, float g, float b);
GLIDE_EXTERN void (FX_CALL *grConstantColorValue)(GrColor_t value);
GLIDE_EXTERN void (FX_CALL *grCullMode)(GrCullMode_t mode);
GLIDE_EXTERN void (FX_CALL *grDepthBiasLevel)(FxI16 level);
GLIDE_EXTERN void (FX_CALL *grDepthBufferFunction)(GrCmpFnc_t function);
GLIDE_EXTERN void (FX_CALL *grDepthBufferMode)(GrDepthBufferMode_t mode);
GLIDE_EXTERN void (FX_CALL *grDepthMask)(FxBool mask);
GLIDE_EXTERN void (FX_CALL *grDisableAllEffects)(void);
GLIDE_EXTERN void (FX_CALL *grDitherMode)(GrDitherMode_t mode);
GLIDE_EXTERN void (FX_CALL *grDrawLine)(const GrVertex *v1, const GrVertex *v2);
GLIDE_EXTERN void (FX_CALL *grDrawPlanarPolygon)(int nverts, const int ilist[], const GrVertex vlist[]);
GLIDE_EXTERN void (FX_CALL *grDrawPlanarPolygonVertexList)(int nverts, const GrVertex vlist[]);
GLIDE_EXTERN void (FX_CALL *grDrawPoint)(const GrVertex *pt);
GLIDE_EXTERN void (FX_CALL *grDrawPolygon)(int nverts, const int ilist[], const GrVertex vlist[]);
GLIDE_EXTERN void (FX_CALL *grDrawPolygonVertexList)(int nverts, const GrVertex vlist[]);
GLIDE_EXTERN void (FX_CALL *grDrawTriangle)(const GrVertex *a, const GrVertex *b, const GrVertex *c);
GLIDE_EXTERN void (FX_CALL *grFogColorValue)(GrColor_t fogcolor);
GLIDE_EXTERN void (FX_CALL *grFogMode)(GrFogMode_t mode);
GLIDE_EXTERN void (FX_CALL *grFogTable)(const GrFog_t ft[64]);
GLIDE_EXTERN void (FX_CALL *grGammaCorrectionValue)(float value);
GLIDE_EXTERN void (FX_CALL *grGlideGetState)(GrState *state);
GLIDE_EXTERN void (FX_CALL *grGlideGetVersion)(char version[80]);
GLIDE_EXTERN void (FX_CALL *grGlideSetState)(const GrState *state);
GLIDE_EXTERN void (FX_CALL *grGlideShamelessPlug)(const FxBool on);
GLIDE_EXTERN void (FX_CALL *grHints)(GrHint_t hintType, FxU32 hintMask);
GLIDE_EXTERN void (FX_CALL *grLfbConstantAlpha)(GrAlpha_t alpha);
GLIDE_EXTERN void (FX_CALL *grLfbConstantDepth)(FxU16 depth);
GLIDE_EXTERN FxBool (FX_CALL *gr_LfbLock)(GrLock_t type, GrBuffer_t buffer, GrLfbWriteMode_t writeMode, GrOriginLocation_t origin, FxBool pixelPipeline, GrLfbInfo_t *info);
GLIDE_EXTERN FxBool (FX_CALL *grLfbReadRegion)(GrBuffer_t src_buffer, FxU32 src_x, FxU32 src_y, FxU32 src_width, FxU32 src_height, FxU32 dst_stride, void *dst_data);
GLIDE_EXTERN FxBool (FX_CALL *gr_LfbUnlock)(GrLock_t type, GrBuffer_t buffer);
GLIDE_EXTERN void (FX_CALL *grLfbWriteColorFormat)(GrColorFormat_t colorFormat);
GLIDE_EXTERN void (FX_CALL *grLfbWriteColorSwizzle)(FxBool swizzleBytes, FxBool swapWords);
GLIDE_EXTERN FxBool (FX_CALL *grLfbWriteRegion)(GrBuffer_t dst_buffer, FxU32 dst_x, FxU32 dst_y, GrLfbSrcFmt_t src_format, FxU32 src_width, FxU32 src_height, FxI32 src_stride, void *src_data);
GLIDE_EXTERN void (FX_CALL *grRenderBuffer)(GrBuffer_t buffer);
GLIDE_EXTERN void (FX_CALL *grResetTriStats)();
GLIDE_EXTERN void (FX_CALL *grSplash)(float x, float y, float width, float height, FxU32 frame);
GLIDE_EXTERN void (FX_CALL *grSstIdle)(void);
GLIDE_EXTERN FxBool (FX_CALL *grSstIsBusy)(void);
GLIDE_EXTERN void (FX_CALL *grSstOrigin)(GrOriginLocation_t origin);
GLIDE_EXTERN void (FX_CALL *grSstPerfStats)(GrSstPerfStats_t *pStats);
GLIDE_EXTERN FxBool (FX_CALL *grSstQueryBoards)(GrHwConfiguration *hwconfig);
GLIDE_EXTERN void (FX_CALL *grSstResetPerfStats)(void);
GLIDE_EXTERN FxU32 (FX_CALL *grSstScreenHeight)(void);
GLIDE_EXTERN FxU32 (FX_CALL *grSstScreenWidth)(void);
GLIDE_EXTERN void (FX_CALL *grSstSelect)(int which_sst);
GLIDE_EXTERN FxU32 (FX_CALL *grSstStatus)(void);
GLIDE_EXTERN FxBool (FX_CALL *grSstVRetraceOn)(void);
GLIDE_EXTERN FxU32 (FX_CALL *grSstVideoLine)(void);
GLIDE_EXTERN FxU32 (FX_CALL *grTexCalcMemRequired)(GrLOD_t lodmin, GrLOD_t lodmax, GrAspectRatio_t aspect, GrTextureFormat_t fmt);
GLIDE_EXTERN void (FX_CALL *grTexClampMode)(GrChipID_t tmu, GrTextureClampMode_t s_clampmode, GrTextureClampMode_t t_clampmode);
GLIDE_EXTERN void (FX_CALL *grTexCombine)(GrChipID_t tmu, GrCombineFunction_t rgb_function, GrCombineFactor_t rgb_factor, GrCombineFunction_t alpha_function, GrCombineFactor_t alpha_factor, FxBool rgb_invert, FxBool alpha_invert);
GLIDE_EXTERN void (FX_CALL *grTexCombineFunction)(GrChipID_t tmu, GrTextureCombineFnc_t fnc);
GLIDE_EXTERN void (FX_CALL *grTexDetailControl)(GrChipID_t tmu, int lod_bias, FxU8 detail_scale, float detail_max);
GLIDE_EXTERN void (FX_CALL *grTexDownloadMipMap)(GrChipID_t tmu, FxU32 startAddress, FxU32 evenOdd, GrTexInfo *info);
GLIDE_EXTERN void (FX_CALL *grTexDownloadMipMapLevel)(GrChipID_t tmu, FxU32 startAddress, GrLOD_t thisLod, GrLOD_t largeLod, GrAspectRatio_t aspectRatio, GrTextureFormat_t format, FxU32 evenOdd, void *data);
GLIDE_EXTERN void (FX_CALL *grTexDownloadMipMapLevelPartial)(GrChipID_t tmu, FxU32 startAddress, GrLOD_t thisLod, GrLOD_t largeLod, GrAspectRatio_t aspectRatio, GrTextureFormat_t format, FxU32 evenOdd, void *data, int start, int end);
GLIDE_EXTERN void (FX_CALL *grTexDownloadTable)(GrChipID_t tmu, GrTexTable_t type, void *data);
GLIDE_EXTERN void (FX_CALL *grTexDownloadTablePartial)(GrChipID_t tmu, GrTexTable_t type, void *data, int start, int end);
GLIDE_EXTERN void (FX_CALL *grTexFilterMode)(GrChipID_t tmu, GrTextureFilterMode_t minfilter_mode, GrTextureFilterMode_t magfilter_mode);
GLIDE_EXTERN void (FX_CALL *grTexLodBiasValue)(GrChipID_t tmu, float bias);
GLIDE_EXTERN FxU32 (FX_CALL *grTexMaxAddress)(GrChipID_t tmu);
GLIDE_EXTERN FxU32 (FX_CALL *grTexMinAddress)(GrChipID_t tmu);
GLIDE_EXTERN void (FX_CALL *grTexMipMapMode)(GrChipID_t tmu, GrMipMapMode_t mode, FxBool lodBlend);
GLIDE_EXTERN void (FX_CALL *grTexMultibase)(GrChipID_t tmu, FxBool enable);
GLIDE_EXTERN void (FX_CALL *grTexMultibaseAddress)(GrChipID_t tmu, GrTexBaseRange_t range, FxU32 startAddress, FxU32 evenOdd, GrTexInfo *info);
GLIDE_EXTERN void (FX_CALL *grTexNCCTable)(GrChipID_t tmu, GrNCCTable_t table);
GLIDE_EXTERN void (FX_CALL *grTexSource)(GrChipID_t tmu, FxU32 startAddress, FxU32 evenOdd, GrTexInfo *info);
GLIDE_EXTERN FxU32 (FX_CALL *grTexTextureMemRequired)(FxU32 evenOdd, GrTexInfo *info);
GLIDE_EXTERN void (FX_CALL *grTriStats)(FxU32 *trisProcessed, FxU32 *trisDrawn);
GLIDE_EXTERN FxBool (FX_CALL *gu3dfGetInfo)(const char *filename, Gu3dfInfo *info);
GLIDE_EXTERN FxBool (FX_CALL *gu3dfLoad)(const char *filename, Gu3dfInfo *data);
GLIDE_EXTERN void (FX_CALL *guAADrawTriangleWithClip)(const GrVertex *a, const GrVertex *b, const GrVertex *c);
GLIDE_EXTERN void (FX_CALL *guAlphaSource)(GrAlphaSource_t mode);
GLIDE_EXTERN void (FX_CALL *guColorCombineFunction)(GrColorCombineFnc_t fnc);
GLIDE_EXTERN void (FX_CALL *guDrawPolygonVertexListWithClip)(int nverts, const GrVertex vlist[]);
GLIDE_EXTERN void (FX_CALL *guDrawTriangleWithClip)(const GrVertex *a, const GrVertex *b, const GrVertex *c);
GLIDE_EXTERN int (FX_CALL *guEncodeRLE16)(void *dst, void *src, FxU32 width, FxU32 height);
GLIDE_EXTERN FxU16 (FX_CALL *guEndianSwapBytes)(FxU16 value);
GLIDE_EXTERN FxU32 (FX_CALL *guEndianSwapWords)(FxU32 value);
GLIDE_EXTERN void (FX_CALL *guFogGenerateExp2)(GrFog_t fogtable[64], float density);
GLIDE_EXTERN void (FX_CALL *guFogGenerateExp)(GrFog_t fogtable[64], float density);
GLIDE_EXTERN void (FX_CALL *guFogGenerateLinear)(GrFog_t fogtable[64], float nearZ, float farZ);
GLIDE_EXTERN float (FX_CALL *guFogTableIndexToW)(int i);
GLIDE_EXTERN GrMipMapId_t (FX_CALL *guTexAllocateMemory)(GrChipID_t tmu, FxU8 odd_even_mask, int width, int height, GrTextureFormat_t fmt, GrMipMapMode_t mm_mode, GrLOD_t smallest_lod, GrLOD_t largest_lod, GrAspectRatio_t aspect, GrTextureClampMode_t s_clamp_mode, GrTextureClampMode_t t_clamp_mode, GrTextureFilterMode_t minfilter_mode, GrTextureFilterMode_t magfilter_mode, float lod_bias, FxBool trilinear);
GLIDE_EXTERN FxBool (FX_CALL *guTexChangeAttributes)(GrMipMapId_t mmid, int width, int height, GrTextureFormat_t fmt, GrMipMapMode_t mm_mode, GrLOD_t smallest_lod, GrLOD_t largest_lod, GrAspectRatio_t aspect, GrTextureClampMode_t s_clamp_mode, GrTextureClampMode_t t_clamp_mode, GrTextureFilterMode_t minFilterMode, GrTextureFilterMode_t magFilterMode);
GLIDE_EXTERN void (FX_CALL *guTexCombineFunction)(GrChipID_t tmu, GrTextureCombineFnc_t fnc);
GLIDE_EXTERN FxU16 * (FX_CALL *guTexCreateColorMipMap)(void);
GLIDE_EXTERN void (FX_CALL *gu_TexDownloadMipMap)(GrMipMapId_t mmid, const void *src, const GuNccTable *table);
GLIDE_EXTERN void (FX_CALL *guTexDownloadMipMapLevel)(GrMipMapId_t mmid, GrLOD_t lod, const void **src);
GLIDE_EXTERN GrMipMapId_t (FX_CALL *guTexGetCurrentMipMap)(GrChipID_t tmu);
GLIDE_EXTERN GrMipMapInfo * (FX_CALL *guTexGetMipMapInfo)(GrMipMapId_t mmid);
GLIDE_EXTERN FxU32 (FX_CALL *guTexMemQueryAvail)(GrChipID_t tmu);
GLIDE_EXTERN void (FX_CALL *guTexMemReset)(void);
GLIDE_EXTERN void (FX_CALL *guTexSource)(GrMipMapId_t id);
GLIDE_EXTERN void (FX_CALL *gr_GlideInit)(void);
GLIDE_EXTERN void (FX_CALL *gr_GlideShutdown)(void);
GLIDE_EXTERN FxBool (FX_CALL *gr_SstQueryHardware)(GrHwConfiguration *hwconfig);
GLIDE_EXTERN FxBool (FX_CALL *gr_SstWinOpen)(FxU32 hWnd, GrScreenResolution_t screen_resolution, GrScreenRefresh_t refresh_rate, GrColorFormat_t color_format, GrOriginLocation_t origin_location, int nColBuffers, int nAuxBuffers);
GLIDE_EXTERN void (FX_CALL *gr_SstWinClose)(void);
GLIDE_EXTERN FxBool (FX_CALL *gr_SstControl)(FxU32 code);
GLIDE_EXTERN void (FX_CALL *gDrawLine)(const GrVertex *v1, const GrVertex *v2);
GLIDE_EXTERN void (FX_CALL *gDrawPolygonVertexList)(int nverts, const GrVertex vlist[]);
GLIDE_EXTERN void (FX_CALL *gDrawTriangle)(const GrVertex *a, const GrVertex *b, const GrVertex *c);
GLIDE_EXTERN void (FX_CALL *gDrawTriangleWithClip)(const GrVertex *a, const GrVertex *b, const GrVertex *c);

#endif
