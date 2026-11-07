/*
 * Engine/3dfx/polydraw.h - globals and functions of Engine/3dfx/polydraw.c used by other files
 */

#ifndef ENGINE_3DFX_POLYDRAW_H
#define ENGINE_3DFX_POLYDRAW_H

#include "dr_types.h"

// [A] OPENDRIVER_PIXEL_LIGHTS: tmuvtx[1].oow of a draw vertex the lamps light (AddLampLight);
// the second TMU is not used, a value no vertex has by chance
#define PIXEL_LAMP_MARK		-4096.0f

// globals
extern const float VERTEX_SNAPPER;
extern char gShinyType;
extern void (*PolygonDrawFunction_D3D[])(int);
extern void (*ClipNearPolygonDrawFunction_D3D[])(int);
extern void (*ClipFarPolygonDrawFunction_D3D[])(int);
extern int PolySizes[56];
extern char PolyLit[56];
extern char PolyTrans[56];
extern void (*PolygonDrawFunction_Glide[])(int);
extern void (*ClipNearPolygonDrawFunction_Glide[])(int);
extern void (*ClipFarPolygonDrawFunction_Glide[])(int);
extern SVECTOR camera_angle;
extern VECTOR4 camera_position;
extern char force_transparency;
// most references: Engine/3dfx/polydraw.c 118, Engine/3dfx/object.c 2
extern float lit_vertex_color;
extern VERTEX conv_points[4096];
// most references: Engine/3dfx/polydraw.c 267, Engine/3dfx/object.c 108, debris.c 2
extern int polyblock;

// functions
extern int CalcDuplicateNormals(MODEL *model);
extern int ConvertModelToInternalFormat(MODEL *in, MODEL *out);
extern void DrawCarShine(void);
extern void DrawClippedPOLYFT4(int trans);
extern void DrawPOLYFT4(int trans);
extern void SetAxisBits(MODEL *model);
extern void SetupCarShine(void);

#endif // ENGINE_3DFX_POLYDRAW_H
