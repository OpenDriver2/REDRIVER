/*
 * Engine/3dfx/gfxinit.h - globals and functions of Engine/3dfx/gfxinit.c used by other files
 */

#ifndef ENGINE_3DFX_GFXINIT_H
#define ENGINE_3DFX_GFXINIT_H

#include "dr_types.h"

// globals
extern int using_glide;
extern char glide_active;
extern int got_3d_hardware;
extern int got_3d_device;
extern int device_type;
extern int glide_origin;
extern void (**PolygonDrawFunction)(int);
extern void (**ClipFarPolygonDrawFunction)(int);
extern void (**ClipNearPolygonDrawFunction)(int);
extern float d3d_screen_xmid;
extern float DIST;
extern float ambient_light_level;
extern int first_error;
extern float clipxmin;
extern float clipxmax;
extern float clipymin;
extern float clipymax;
extern float true_screen_xmid;
extern int gPalettePresent[50];
extern int gCurrentPalette;
extern float screen_ymid;
extern float d3d_screen_ymid;
extern int LastPaletteNo;
extern int gPreviousAlpha;
extern float screen_xmid;
extern char shitfog[64];
extern int last_filter_state;
extern char gPalette[50][1024];
extern float screen_scale;
extern char last_trans_on;
extern float cell_recip;
extern int last_tset;
extern int display_height;
extern float true_screen_ymid;
extern float mirror_far_clip;
extern char highlight;
extern float true_screen_scale;
extern int current_vertex_computed_value;
extern float far_clip;
extern int display_width;
extern int current_object_computed_value;
extern int last_state;
extern int viewport_reset;
extern int ooz_far_clip;
extern int gUseAlphaTests;
extern float wide_aspect;
extern int wide_hud_edges;

// the width of the 4:3 picture in the middle of the screen, what the HUD is scaled to
#define HUD_WIDTH ((float)display_width / wide_aspect)

// [A] screen pixels in a pixel of the 640 x 480 HUD
#define HUD_SCALE (HUD_WIDTH / 640.0f)

// functions
extern int Detect3dfx(void);
extern void DrawScaledLine(float x1, float y1, float x2, float y2, int colour);
extern int EnableDepthMask(int enabled);
extern void EndOfFrame(void);
extern void ExpandViewport(void);
extern int InitDrawingSystem(int fullscreen, int keepViewport);
extern int ResizeDisplay(int width, int height);
extern void InitEngineGlobals(void);
extern void ReduceViewport(void);
extern void ResetViewport(int x, int y, char clear);
extern void SetFogType(void);
extern void ShutdownDrawingSystem(void);
extern void StartOfFrame(void);
extern void SwapPalette(int PaletteNo);
extern int UseGlideDevice(void);

#endif // ENGINE_3DFX_GFXINIT_H
