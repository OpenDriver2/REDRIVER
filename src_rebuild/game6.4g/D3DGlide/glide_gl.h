/*
 * D3DGlide/glide_gl.h - [A] the OpenGL Glide device (glide_gl.c)
 */

#ifndef GLIDE_GL_H
#define GLIDE_GL_H

#include "d3dglide.h"

#define DEVICE_TYPE_GL		2

int UseGLDevice(void);
void gl_GetHWSettings(HARDWARE_SETTINGS *settings);
FxBool gl_LfbLockInfo(GrBuffer_t buffer, LFB_INFO *info);
void FX_CALL gl_TexDownloadMipMap(GrChipID_t tmu, FxU32 startAddress, FxU32 evenOdd, GrTexInfo *info);

#endif // GLIDE_GL_H
