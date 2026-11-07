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
void gl_RepaintWindow(void);
int gl_StartVideo(void);
void gl_StopVideo(void);
int gl_SetVideoSize(int width, int height);
void gl_ShowVideoFrame(const void *data);
void gl_SetScreenScale(float scale);
void gl_SetFullFog(int full);
void gl_SetFarFade(float far_dist);
void gl_SetVertexAlpha(int on);
int gl_CreateEmulatedTexture(int tset, unsigned char *data, int palette, char *palette_data);
void gl_ReleaseEmulatedTextures(void);
void gl_TexDownloadMipMapSet(GrChipID_t tmu, FxU32 startAddress, FxU32 evenOdd, GrTexInfo *info, int tset);
void gl_TexDownload8(GrChipID_t tmu, FxU32 startAddress, FxU32 evenOdd, GrTexInfo *info, int tset);
void gl_TexDownload16(int tset, void *data);
void gl_TexDownloadPalette(int palette, void *data);

#endif // GLIDE_GL_H
