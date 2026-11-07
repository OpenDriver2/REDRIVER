/*
 * Engine/3dfx/polydraw.c - polygon drawing and the conversion of level models
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "Engine\3dfx\polydraw.c").
 */

#include <math.h>

#include "dr_types.h"
#include "Engine/3dfx/polydraw.h"
#include "D3DGlide/glide_ptrs.h"
#include "Engine/3dfx/object.h"
#include "Engine/3dfx/tset.h"
#include "asmmath.h"
#include "dr_globals.h"
#include "testmath.h"
#include "Engine/3dfx/gfxinit.h"

// polygons in the level file

typedef struct UV_INFO
{
	unsigned char u;
	unsigned char v;
} UV_INFO;

typedef struct OUT_POLYF3
{
	char id;
	char pad;
	short v0;
	short v1;
	short v2;
	RGB color;
} OUT_POLYF3;

typedef struct OUT_POLYF3LIT
{
	char id;
	char pad1;
	short v0;
	short v1;
	short v2;
	short pad2;
	short normal;
	RGB color;
} OUT_POLYF3LIT;

typedef struct OUT_POLYF4
{
	char id;
	char pad1;
	short v0;
	short v1;
	short v2;
	short v3;
	short pad2;
	RGB color;
} OUT_POLYF4;

typedef struct OUT_POLYF4LIT
{
	char id;
	char pad;
	short v0;
	short v1;
	short v2;
	short v3;
	short normal;
	RGB color;
} OUT_POLYF4LIT;

typedef struct OUT_POLYG3
{
	char id;
	char pad;
	short v0;
	short v1;
	short v2;
	RGB c0;
	RGB c1;
	RGB c2;
} OUT_POLYG3;

typedef struct OUT_POLYG3LIT
{
	char id;
	char pad1;
	short v0;
	short v1;
	short v2;
	short pad2;
	short pn0;
	short pn1;
	short pn2;
	RGB color;
} OUT_POLYG3LIT;

typedef struct OUT_POLYG4
{
	char id;
	char pad1;
	short v0;
	short v1;
	short v2;
	short v3;
	short pad2;
	RGB c0;
	RGB c1;
	RGB c2;
	RGB c3;
} OUT_POLYG4;

typedef struct OUT_POLYG4LIT
{
	char id;
	char pad1;
	short v0;
	short v1;
	short v2;
	short v3;
	short pad2;
	short pn0;
	short pn1;
	short pn2;
	short pn3;
	RGB color;
} OUT_POLYG4LIT;

typedef struct OUT_POLYFT3
{
	char id;
	char texture_set;
	short v0;
	short v1;
	short v2;
	short pad;
	UV_INFO uv0;
	UV_INFO uv1;
	UV_INFO uv2;
	RGB color;
} OUT_POLYFT3;

typedef struct OUT_POLYFT3LIT
{
	char id;
	char texture_set;
	short v0;
	short v1;
	short v2;
	short normal;
	UV_INFO uv0;
	UV_INFO uv1;
	UV_INFO uv2;
	RGB color;
} OUT_POLYFT3LIT;

typedef struct OUT_POLYFT4
{
	char id;
	char texture_set;
	short v0;
	short v1;
	short v2;
	short v3;
	short pad;
	UV_INFO uv0;
	UV_INFO uv1;
	UV_INFO uv2;
	UV_INFO uv3;
	RGB color;
} OUT_POLYFT4;

typedef struct OUT_POLYFT4LIT
{
	char id;
	char texture_set;
	short v0;
	short v1;
	short v2;
	short v3;
	short normal;
	UV_INFO uv0;
	UV_INFO uv1;
	UV_INFO uv2;
	UV_INFO uv3;
	RGB color;
} OUT_POLYFT4LIT;

typedef struct OUT_POLYGT3
{
	unsigned char id;
	unsigned char texture_set;
	unsigned short v0;
	unsigned short v1;
	unsigned short v2;
	unsigned short pad;
	UV_INFO uv0;
	UV_INFO uv1;
	UV_INFO uv2;
	RGB c0;
	RGB c1;
	RGB c2;
} OUT_POLYGT3;

typedef struct OUT_POLYGT3LIT
{
	char id;
	char texture_set;
	short v0;
	short v1;
	short v2;
	short pn0;
	short pn1;
	short pn2;
	UV_INFO uv0;
	UV_INFO uv1;
	UV_INFO uv2;
	RGB color;
} OUT_POLYGT3LIT;

typedef struct OUT_POLYGT4
{
	unsigned char id;
	unsigned char texture_set;
	unsigned short v0;
	unsigned short v1;
	unsigned short v2;
	unsigned short v3;
	unsigned short pad;
	UV_INFO uv0;
	UV_INFO uv1;
	UV_INFO uv2;
	UV_INFO uv3;
	RGB c0;
	RGB c1;
	RGB c2;
	RGB c3;
} OUT_POLYGT4;

typedef struct OUT_POLYGT4LIT
{
	char id;
	char texture_set;
	short v0;
	short v1;
	short v2;
	short v3;
	short pad;
	short pn0;
	short pn1;
	short pn2;
	short pn3;
	UV_INFO uv0;
	UV_INFO uv1;
	UV_INFO uv2;
	UV_INFO uv3;
	RGB color;
} OUT_POLYGT4LIT;

// a polygon of either kind: the type byte comes first in both
typedef union POLY_PTR
{
	char *p;
	OUT_POLYF3 *f3;
	OUT_POLYF4 *f4;
	OUT_POLYG3 *g3;
	OUT_POLYG4 *g4;
	OUT_POLYFT3 *ft3;
	OUT_POLYFT4 *ft4;
	OUT_POLYGT3 *gt3;
	OUT_POLYGT4 *gt4;
	OUT_POLYF3LIT *f3l;
	OUT_POLYF4LIT *f4l;
	OUT_POLYG3LIT *g3l;
	OUT_POLYG4LIT *g4l;
	OUT_POLYFT3LIT *ft3l;
	OUT_POLYFT4LIT *ft4l;
	OUT_POLYGT3LIT *gt3l;
	OUT_POLYGT4LIT *gt4l;
} POLY_PTR;

// a point of the clipped polygon (ClipTestScale*)
// NOTE: IDA types clip_poly_info as TRI_POINT[5], of the same size
typedef struct VINF
{
	VECTOR4 point;
	float u;
	float v;
	float intensity;
	float r;
	float g;
	float b;
	float field_28;		// the car shine texture u, v (d3dDrawClippedPolyGT3_LIT ...)
	float field_2c;
} VINF;

// a queued car shine triangle (AddCarShine)
typedef struct CAR_SHINE
{
	int clipped;
	GrVertex v0;
	GrVertex v1;
	GrVertex v2;
} CAR_SHINE;

#define MAX_CAR_SHINES		2048

#define NEAR_CLIP_Z			50.0f

// the float's bits as an int
#define FLOAT_BITS(f)		(*(int *)&(f))

// the car shine texture u, v of a point normal (DrawCarModel)
#define SHINE_U(pn)			((float)stru_11D9060[pn].field_0)
#define SHINE_V(pn)			((float)stru_11D9060[pn].field_1)

// the start of every converted polygon: type (POLY_TYPES)
#define POLY_TYPE(p)		(*(unsigned char *)(p))

#define AXIS_EPSILON_LOW	0.999f
#define AXIS_EPSILON_HIGH	1.001f

//------------------------------------------------------------------------------------------
// initialised data (0x55299c)

// NOTE: no names in the database: the screen size of the off screen test (PolyOffScreen)
static float offscreen_x = 640.0f;
static float offscreen_y = 480.0f;

char gShinyType = 1;

// the draw function of each polygon type (POLY_TYPES), D3D and Glide; InitDrawingSystem selects
// the set
void SkipDrawPOLY(int trans);
void DrawClippedPOLYFT4(int trans);
void DrawPOLYFT4(int trans);
void DrawClippedPOLYF3(int trans);
void DrawClippedPOLYF3_LIT(int trans);
void DrawClippedPOLYF4(int trans);
void DrawClippedPOLYF4_LIT(int trans);
void DrawClippedPOLYFT3(int trans);
void DrawClippedPOLYFT3_LIT(int trans);
void DrawClippedPOLYFT4_LIT(int trans);
void DrawClippedPOLYG3(int trans);
void DrawClippedPOLYG3_LIT(int trans);
void DrawClippedPOLYG4(int trans);
void DrawClippedPOLYG4_LIT(int trans);
void DrawClippedPOLYGT3(int trans);
void DrawClippedPOLYGT3_LIT(int trans);
void DrawClippedPOLYGT4(int trans);
void DrawClippedPOLYGT4_LIT(int trans);
void DrawFarClippedPOLYF3(int trans);
void DrawFarClippedPOLYF3_LIT(int trans);
void DrawFarClippedPOLYF4(int trans);
void DrawFarClippedPOLYF4_LIT(int trans);
void DrawFarClippedPOLYFT3(int trans);
void DrawFarClippedPOLYFT3_LIT(int trans);
void DrawFarClippedPOLYFT4(int trans);
void DrawFarClippedPOLYFT4_LIT(int trans);
void DrawFarClippedPOLYGT3(int trans);
void DrawFarClippedPOLYGT3_LIT(int trans);
void DrawFarClippedPOLYGT4(int trans);
void DrawFarClippedPOLYGT4_LIT(int trans);
void DrawPOLYF3(int trans);
void DrawPOLYF3_LIT(int trans);
void DrawPOLYF4(int trans);
void DrawPOLYF4_LIT(int trans);
void DrawPOLYFT3(int trans);
void DrawPOLYFT3_LIT(int trans);
void DrawPOLYFT4_LIT(int trans);
void DrawPOLYG3(int trans);
void DrawPOLYG3_LIT(int trans);
void DrawPOLYG4(int trans);
void DrawPOLYG4_LIT(int trans);
void DrawPOLYGT3(int trans);
void DrawPOLYGT3_LIT(int trans);
void DrawPOLYGT4(int trans);
void DrawPOLYGT4_LIT(int trans);
void d3dDrawClippedPolyF3(int trans);
void d3dDrawClippedPolyF3_LIT(int trans);
void d3dDrawClippedPolyF4(int trans);
void d3dDrawClippedPolyF4_LIT(int trans);
void d3dDrawClippedPolyFT3(int trans);
void d3dDrawClippedPolyFT3_LIT(int trans);
void d3dDrawClippedPolyFT4(int trans);
void d3dDrawClippedPolyFT4_LIT(int trans);
void d3dDrawClippedPolyG3(int trans);
void d3dDrawClippedPolyG3_LIT(int trans);
void d3dDrawClippedPolyG4(int trans);
void d3dDrawClippedPolyG4_LIT(int trans);
void d3dDrawClippedPolyGT3(int trans);
void d3dDrawClippedPolyGT3_LIT(int trans);
void d3dDrawClippedPolyGT4(int trans);
void d3dDrawClippedPolyGT4_LIT(int trans);
void d3dDrawFarClippedPOLYF3(int trans);
void d3dDrawFarClippedPOLYF3_LIT(int trans);
void d3dDrawFarClippedPOLYF4(int trans);
void d3dDrawFarClippedPOLYF4_LIT(int trans);
void d3dDrawFarClippedPOLYFT3(int trans);
void d3dDrawFarClippedPOLYFT3_LIT(int trans);
void d3dDrawFarClippedPOLYFT4(int trans);
void d3dDrawFarClippedPOLYFT4_LIT(int trans);
void d3dDrawFarClippedPOLYGT3(int trans);
void d3dDrawFarClippedPOLYGT3_LIT(int trans);
void d3dDrawFarClippedPOLYGT4(int trans);
void d3dDrawFarClippedPOLYGT4_LIT(int trans);
void d3dDrawPolyF3(int trans);
void d3dDrawPolyF3_LIT(int trans);
void d3dDrawPolyF4(int trans);
void d3dDrawPolyF4_LIT(int trans);
void d3dDrawPolyFT3(int trans);
void d3dDrawPolyFT3_LIT(int trans);
void d3dDrawPolyFT4(int trans);
void d3dDrawPolyFT4_LIT(int trans);
void d3dDrawPolyG3(int trans);
void d3dDrawPolyG3_LIT(int trans);
void d3dDrawPolyG4(int trans);
void d3dDrawPolyG4_LIT(int trans);
void d3dDrawPolyGT3(int trans);
void d3dDrawPolyGT3_LIT(int trans);
void d3dDrawPolyGT4(int trans);
void d3dDrawPolyGT4_LIT(int trans);

void (*PolygonDrawFunction_D3D[56])(int) =
{
	d3dDrawPolyF3, d3dDrawPolyF4, d3dDrawPolyG3, d3dDrawPolyG4, d3dDrawPolyFT3, d3dDrawPolyFT4, d3dDrawPolyGT3, d3dDrawPolyGT4,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	d3dDrawPolyF3_LIT, d3dDrawPolyF4_LIT, d3dDrawPolyG3_LIT, d3dDrawPolyG4_LIT, d3dDrawPolyFT3_LIT, d3dDrawPolyFT4_LIT, d3dDrawPolyGT3_LIT, d3dDrawPolyGT4_LIT,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	d3dDrawPolyF3, d3dDrawPolyF4, d3dDrawPolyG3, d3dDrawPolyG4, d3dDrawPolyFT3, d3dDrawPolyFT4, d3dDrawPolyGT3, d3dDrawPolyGT4,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	d3dDrawPolyF3_LIT, d3dDrawPolyF4_LIT, d3dDrawPolyG3_LIT, d3dDrawPolyG4_LIT, d3dDrawPolyFT3_LIT, d3dDrawPolyFT4_LIT, d3dDrawPolyGT3_LIT, d3dDrawPolyGT4_LIT,
};

void (*ClipNearPolygonDrawFunction_D3D[56])(int) =
{
	d3dDrawClippedPolyF3, d3dDrawClippedPolyF4, d3dDrawClippedPolyG3, d3dDrawClippedPolyG4, d3dDrawClippedPolyFT3, d3dDrawClippedPolyFT4, d3dDrawClippedPolyGT3, d3dDrawClippedPolyGT4,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	d3dDrawClippedPolyF3_LIT, d3dDrawClippedPolyF4_LIT, d3dDrawClippedPolyG3_LIT, d3dDrawClippedPolyG4_LIT, d3dDrawClippedPolyFT3_LIT, d3dDrawClippedPolyFT4_LIT, d3dDrawClippedPolyGT3_LIT, d3dDrawClippedPolyGT4_LIT,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	d3dDrawClippedPolyF3, d3dDrawClippedPolyF4, d3dDrawClippedPolyG3, d3dDrawClippedPolyG4, d3dDrawClippedPolyFT3, d3dDrawClippedPolyFT4, d3dDrawClippedPolyGT3, d3dDrawClippedPolyGT4,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	d3dDrawClippedPolyF3_LIT, d3dDrawClippedPolyF4_LIT, d3dDrawClippedPolyG3_LIT, d3dDrawClippedPolyG4_LIT, d3dDrawClippedPolyFT3_LIT, d3dDrawClippedPolyFT4_LIT, d3dDrawClippedPolyGT3_LIT, d3dDrawClippedPolyGT4_LIT,
};

void (*ClipFarPolygonDrawFunction_D3D[56])(int) =
{
	d3dDrawFarClippedPOLYF3, d3dDrawFarClippedPOLYF4, SkipDrawPOLY, SkipDrawPOLY, d3dDrawFarClippedPOLYFT3, d3dDrawFarClippedPOLYFT4, d3dDrawFarClippedPOLYGT3, d3dDrawFarClippedPOLYGT4,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	d3dDrawFarClippedPOLYF3_LIT, d3dDrawFarClippedPOLYF4_LIT, SkipDrawPOLY, SkipDrawPOLY, d3dDrawFarClippedPOLYFT3_LIT, d3dDrawFarClippedPOLYFT4_LIT, d3dDrawFarClippedPOLYGT3_LIT, d3dDrawFarClippedPOLYGT4_LIT,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	d3dDrawFarClippedPOLYF3, d3dDrawFarClippedPOLYF4, SkipDrawPOLY, SkipDrawPOLY, d3dDrawFarClippedPOLYFT3, d3dDrawFarClippedPOLYFT4, d3dDrawFarClippedPOLYGT3, d3dDrawFarClippedPOLYGT4,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	d3dDrawFarClippedPOLYF3_LIT, d3dDrawFarClippedPOLYF4_LIT, SkipDrawPOLY, SkipDrawPOLY, d3dDrawFarClippedPOLYFT3_LIT, d3dDrawFarClippedPOLYFT4_LIT, d3dDrawFarClippedPOLYGT3_LIT, d3dDrawFarClippedPOLYGT4_LIT,
};

void (*PolygonDrawFunction_Glide[56])(int) =
{
	DrawPOLYF3, DrawPOLYF4, DrawPOLYG3, DrawPOLYG4, DrawPOLYFT3, DrawPOLYFT4, DrawPOLYGT3, DrawPOLYGT4,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	DrawPOLYF3_LIT, DrawPOLYF4_LIT, DrawPOLYG3_LIT, DrawPOLYG4_LIT, DrawPOLYFT3_LIT, DrawPOLYFT4_LIT, DrawPOLYGT3_LIT, DrawPOLYGT4_LIT,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	DrawPOLYF3, DrawPOLYF4, DrawPOLYG3, DrawPOLYG4, DrawPOLYFT3, DrawPOLYFT4, DrawPOLYGT3, DrawPOLYGT4,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	DrawPOLYF3_LIT, DrawPOLYF4_LIT, DrawPOLYG3_LIT, DrawPOLYG4_LIT, DrawPOLYFT3_LIT, DrawPOLYFT4_LIT, DrawPOLYGT3_LIT, DrawPOLYGT4_LIT,
};

void (*ClipNearPolygonDrawFunction_Glide[56])(int) =
{
	DrawClippedPOLYF3, DrawClippedPOLYF4, DrawClippedPOLYG3, DrawClippedPOLYG4, DrawClippedPOLYFT3, DrawClippedPOLYFT4, DrawClippedPOLYGT3, DrawClippedPOLYGT4,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	DrawClippedPOLYF3_LIT, DrawClippedPOLYF4_LIT, DrawClippedPOLYG3_LIT, DrawClippedPOLYG4_LIT, DrawClippedPOLYFT3_LIT, DrawClippedPOLYFT4_LIT, DrawClippedPOLYGT3_LIT, DrawClippedPOLYGT4_LIT,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	DrawClippedPOLYF3, DrawClippedPOLYF4, DrawClippedPOLYG3, DrawClippedPOLYG4, DrawClippedPOLYFT3, DrawClippedPOLYFT4, DrawClippedPOLYGT3, DrawClippedPOLYGT4,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	DrawClippedPOLYF3_LIT, DrawClippedPOLYF4_LIT, DrawClippedPOLYG3_LIT, DrawClippedPOLYG4_LIT, DrawClippedPOLYFT3_LIT, DrawClippedPOLYFT4_LIT, DrawClippedPOLYGT3_LIT, DrawClippedPOLYGT4_LIT,
};

void (*ClipFarPolygonDrawFunction_Glide[56])(int) =
{
	DrawFarClippedPOLYF3, DrawFarClippedPOLYF4, SkipDrawPOLY, SkipDrawPOLY, DrawFarClippedPOLYFT3, DrawFarClippedPOLYFT4, DrawFarClippedPOLYGT3, DrawFarClippedPOLYGT4,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	DrawFarClippedPOLYF3_LIT, DrawFarClippedPOLYF4_LIT, SkipDrawPOLY, SkipDrawPOLY, DrawFarClippedPOLYFT3_LIT, DrawFarClippedPOLYFT4_LIT, DrawFarClippedPOLYGT3_LIT, DrawFarClippedPOLYGT4_LIT,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	DrawFarClippedPOLYF3, DrawFarClippedPOLYF4, SkipDrawPOLY, SkipDrawPOLY, DrawFarClippedPOLYFT3, DrawFarClippedPOLYFT4, DrawFarClippedPOLYGT3, DrawFarClippedPOLYGT4,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	DrawFarClippedPOLYF3_LIT, DrawFarClippedPOLYF4_LIT, SkipDrawPOLY, SkipDrawPOLY, DrawFarClippedPOLYFT3_LIT, DrawFarClippedPOLYFT4_LIT, DrawFarClippedPOLYGT3_LIT, DrawFarClippedPOLYGT4_LIT,
};

// size of each polygon type in memory (POLY_TYPES)
int PolySizes[56] =
{
	32, 36, 56, 72, 56, 68, 80, 104, 0, 0, 0, 0, 0, 0, 0, 0,
	32, 36, 44, 52, 56, 68, 68, 84, 0, 0, 0, 0, 0, 0, 0, 0,
	32, 36, 56, 72, 56, 68, 80, 104, 0, 0, 0, 0, 0, 0, 0, 0,
	32, 36, 44, 52, 56, 68, 68, 84,
};

// size of each polygon type in the level file
int OUT_PolySizes[56] =
{
	12, 16, 20, 28, 20, 24, 28, 36, 0, 0, 0, 0, 0, 0, 0, 0,
	16, 16, 20, 24, 20, 24, 24, 32, 0, 0, 0, 0, 0, 0, 0, 0,
	12, 16, 20, 28, 20, 24, 28, 36, 0, 0, 0, 0, 0, 0, 0, 0,
	16, 16, 20, 24, 20, 24, 24, 32,
};

// the lit polygon types (their light intensity is computed when drawn)
char PolyLit[56] =
{
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	1, 1, 1, 1, 1, 1, 1, 1,
};

// the transparency of each polygon type (the draw functions' argument)
char PolyTrans[56] =
{
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0,
	1, 1, 1, 1, 1, 1, 1, 1,
};

//------------------------------------------------------------------------------------------
// uninitialised data
// NOTE: bss globals in the linker's common area; defined here as polydraw.c is their main user

GrVertex VertexA;
GrVertex VertexB;
GrVertex VertexC;

int total_axis_alligned;
int total_none_axis_alligned;
int total_axis_alligned_models;
int total_partial_axis_alligned_models;
int total_none_axis_alligned_models;
int num_axis_alligned;
int num_none_axis_alligned;

int JClipCheck;
float lit_vertex_color;		// the car shine colour (DrawCarModel)
struc_11D9060 stru_11D9060[256];	// the car shine texture u, v per point normal

VINF clip_poly_info[5];
int num_clip_poly_points;
VINF *clip_ptr;

// NOTE: no names in the database (stru_11D93A0, stru_11D93E0, stru_11D9360)
static GrVertex shine_vertex_c;
static GrVertex shine_vertex_a;
static GrVertex shine_vertex_b;

CAR_SHINE *car_shine_table;
int NumShinyPolys;
CAR_SHINE car_shines[MAX_CAR_SHINES];

VERTEX conv_points[4096];		// the vertices of the model being drawn
int polyblock;					// the polygon being drawn

void AddCarShine(GrVertex *v1, GrVertex *v2, GrVertex *v3, int clipped);
void ClipTestScale(VERTEX *from, VERTEX *to);
void ClipTestScaleIntensity(VERTEX *from, VERTEX *to, float i0, float i1);
void ClipTestScaleUV(VERTEX *from, VERTEX *to, float u0, float v0, float u1, float v1);
void ClipTestScaleRGB(VERTEX *from, VERTEX *to, float r0, float g0, float b0, float r1, float g1, float b1);
void ClipTestScaleRGBUV(VERTEX *from, VERTEX *to, float r0, float g0, float b0, float u0, float v0, float r1, float g1, float b1, float u1, float v1);
void ClipTestScaleRGBUVWHIntensity(VERTEX *from, VERTEX *to, float r0, float g0, float b0, float u0, float v0, float w0, float h0, float r1, float g1, float b1, float u1, float v1, float w1, float h1);
void FarClipTestScale(VERTEX *from, VERTEX *to);
void FarClipTestScaleUV(VERTEX *from, VERTEX *to, float u0, float v0, float u1, float v1);
void FarClipTestScaleRGBUV(VERTEX *from, VERTEX *to, float r0, float g0, float b0, float u0, float v0, float r1, float g1, float b1, float u1, float v1);

//------------------------------------------------------------------------------------------
// [A] helpers for the polygon draw functions
// NOTE: not functions in the original: these blocks are written out (macros) in every draw
// function

// [A]
// The vertex of the model being drawn, rotated (and projected) once per object.
static VERTEX *GetVertex(int index, void (*rotate)(VERTEX *))
{
	VERTEX *cv;
	VECTOR4 *iv;

	cv = &conv_points[index];

	if (cv->computed != current_vertex_computed_value)
	{
		iv = &input_vertices_ptr[index];

		cv->vx = iv->vx;
		cv->vy = iv->vy;
		cv->vz = iv->vz;

		rotate(cv);
	}

	return cv;
}

// [A]
// Whether a polygon is entirely off screen: all its x (or y) negative, by the float sign bits.
// NOTE: the right and bottom tests were meant to be "all x - 640 (y - 480) positive", but they
// cull only when the differences OR to zero bits (all exactly 640 / 480), so they never cull in
// practice. The differences go to unnamed globals (0x11d903c ...) that nothing else reads.
static int PolyOffScreen(VERTEX **v, int count)
{
	float d;
	int bits;
	int i;

	bits = 0x80000000;

	for (i = 0; i < count; i++)
		bits &= FLOAT_BITS(v[i]->vx);

	JClipCheck = bits;

	if (JClipCheck)
		return 1;

	bits = 0x80000000;

	for (i = 0; i < count; i++)
		bits &= FLOAT_BITS(v[i]->vy);

	JClipCheck = bits;

	if (JClipCheck)
		return 1;

	bits = 0;

	for (i = 0; i < count; i++)
	{
		d = v[i]->vx - offscreen_x;
		bits |= FLOAT_BITS(d);
	}

	JClipCheck = bits;

	if (JClipCheck == 0)
		return 1;

	bits = 0;

	for (i = 0; i < count; i++)
	{
		d = v[i]->vy - offscreen_y;
		bits |= FLOAT_BITS(d);
	}

	JClipCheck = bits;

	if (JClipCheck == 0)
		return 1;

	return 0;
}

// [A]
static int PolyOffScreen3(VERTEX *v0, VERTEX *v1, VERTEX *v2)
{
	VERTEX *v[3];

	v[0] = v0;
	v[1] = v1;
	v[2] = v2;

	return PolyOffScreen(v, 3);
}

// [A]
static int PolyOffScreen4(VERTEX *v0, VERTEX *v1, VERTEX *v2, VERTEX *v3)
{
	VERTEX *v[4];

	v[0] = v0;
	v[1] = v1;
	v[2] = v2;
	v[3] = v3;

	return PolyOffScreen(v, 4);
}

// [A]
static int PolyOffScreen5(VERTEX *v0, VERTEX *v1, VERTEX *v2, VERTEX *v3, VERTEX *v4)
{
	VERTEX *v[5];

	v[0] = v0;
	v[1] = v1;
	v[2] = v2;
	v[3] = v3;
	v[4] = v4;

	return PolyOffScreen(v, 5);
}

// [A]
// Projects point i of the clipped polygon (clip_poly_info) to the screen: vx, vy, vz, oow.
// glide: the Glide draw functions' screen (screen_xmid / ymid less the snapper).
static void ProjectClipPoint(VERTEX *out, int i, int glide)
{
	VINF *p;
	float scale;

	p = &clip_poly_info[i];

	out->vz = p->point.vz;
	out->oow = ooz_table[float_to_int(p->point.vz)];

	scale = out->oow * DIST;

	if (glide)
	{
		out->vx = p->point.vx * scale + screen_xmid - VERTEX_SNAPPER;
		out->vy = p->point.vy * scale + screen_ymid - VERTEX_SNAPPER;
	}
	else
	{
		out->vx = p->point.vx * scale + d3d_screen_xmid;
		out->vy = p->point.vy * scale + d3d_screen_ymid;
	}
}

// [A]
// Colour combine for the untextured polygons: the iterated colour.
static void SetFlatColorCombine(void)
{
	if (last_state != FLAT3)
	{
		grColorCombine(GR_COMBINE_FUNCTION_LOCAL, GR_COMBINE_FACTOR_ZERO, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_CONSTANT, FXFALSE);
		last_state = FLAT3;
	}
}

// [A]
// Colour combine for the textured polygons: texture * iterated colour.
static void SetTexturedColorCombine(void)
{
	if (last_state != TEXT3)
	{
		grColorCombine(GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_LOCAL, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_TEXTURE, FXFALSE);
		last_state = TEXT3;
	}
}

// [A]
static void SetTextureSet(int tset)
{
	if (tset != last_tset)
	{
		last_tset = tset;
		grTexSource(GR_TMU0, TextureSlots[last_tset].texture_ram_address, GR_MIPMAPLEVELMASK_BOTH, TextureSlots[last_tset].TInfo);
	}
}

// [A]
// trans: 0 opaque, 1 alpha, 2 subtractive (1 - src colour), 3 additive; force_transparency
// overrides a transparent polygon's.
static void SetTransparency(int trans)
{
	if (trans && force_transparency)
		trans = force_transparency;

	if (last_trans_on != trans)
	{
		switch (trans)
		{
			case 0:
				grAlphaBlendFunction(GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_ONE, GR_BLEND_ZERO);
				break;
			case 1:
				grAlphaBlendFunction(GR_BLEND_ZERO, GR_BLEND_ONE_MINUS_SRC_ALPHA, GR_BLEND_ONE, GR_BLEND_ZERO);
				break;
			case 2:
				grAlphaBlendFunction(GR_BLEND_ZERO, GR_BLEND_ONE_MINUS_SRC_COLOR, GR_BLEND_ONE, GR_BLEND_ZERO);
				break;
			case 3:
				grAlphaBlendFunction(GR_BLEND_ONE, GR_BLEND_ONE, GR_BLEND_ONE, GR_BLEND_ZERO);
				break;
		}

		last_trans_on = trans;
	}
}

// [A]
// NOTE: with OPENDRIVER_PIXEL_LIGHTS it also clears the lamp mark of AddLampLight
static void SetColor(GrVertex *out, float r, float g, float b)
{
	out->r = r;
	out->g = g;
	out->b = b;

#ifdef OPENDRIVER_PIXEL_LIGHTS
	out->tmuvtx[1].oow = 0.0f;
#endif
}

// [A]
// The screen position of a projected vertex.
static void SetPosition(GrVertex *out, VERTEX *cv)
{
	out->x = cv->vx;
	out->y = cv->vy;
	out->z = cv->vz;
	out->oow = cv->oow;
}

// [A]
// The screen position of a vertex projected for Glide (no z).
static void SetGlidePosition(GrVertex *out, VERTEX *cv)
{
	out->x = cv->vx;
	out->y = cv->vy;
	out->oow = cv->oow;
}

// [A]
static void SetTexCoord(GrVertex *out, float s, float t)
{
	out->tmuvtx[0].sow = s * out->oow;
	out->tmuvtx[0].tow = t * out->oow;
}

// [A]
static void DrawTriangleABC(void)
{
	if (do_clipping)
		gDrawTriangleWithClip(&VertexA, &VertexB, &VertexC);
	else
		gDrawTriangle(&VertexA, &VertexB, &VertexC);
}

// [A]
// DrawTriangleABC of the Glide draw functions.
static void DrawGlideTriangleABC(void)
{
	if (do_clipping)
		guDrawTriangleWithClip(&VertexA, &VertexB, &VertexC);
	else
		grDrawTriangle(&VertexA, &VertexB, &VertexC);
}

// [A]
// The colour r, g, b at a vertex plus the light of the lamps reaching the model
// (CalcVisibleLamps), each component up to 255. tx, ty, vz: the vertex in camera space.
// NOTE: with OPENDRIVER_SMOOTH_LAMPS a lamp falls off as the inverse square of the distance,
// (1 - x^2)^2 / (1 + LAMP_INV_SQUARE * x) with x = d^2 / r^2 and r the range made larger in
// AddLamp, instead of the original 1 - d^2 / r^2: bright near the lamp, a long soft tail, and 0
// at the range with no slope, so the edge of the light shows much less.
// NOTE: with OPENDRIVER_PIXEL_LIGHTS only the colour is set and the vertex is marked
// (PIXEL_LAMP_MARK); the OpenGL device adds the lamps per pixel (glide_gl.c).
static void AddLampLight(GrVertex *out, float tx, float ty, float tz, float r, float g, float b)
{
#ifdef OPENDRIVER_PIXEL_LIGHTS
	out->tmuvtx[1].oow = PIXEL_LAMP_MARK;
#else
	GAMELAMP *lamp;
	float dx;
	float dy;
	float dz;
	float dist;
	float scale;
	int i;

	for (i = 0; i < num_potential_lamps; i++)
	{
		lamp = &lamps[potential_lamps[i]];

		dx = lamp->position.vx - tx;
		dy = lamp->position.vy - ty;
		dz = lamp->position.vz - tz;

		dist = dx * dx + dy * dy + dz * dz;

		if (dist < lamp->sqradius)
		{
#ifdef OPENDRIVER_SMOOTH_LAMPS
			dist *= lamp->scale;
			scale = 1.0f - dist * dist;
			scale = scale * scale / (1.0f + (float)LAMP_INV_SQUARE * dist);
#else
			scale = (lamp->sqradius - dist) * lamp->scale;
#endif

			r += scale * lamp->color.vx;

			if (r > 255.0f)
				r = 255.0f;

			g += scale * lamp->color.vy;

			if (g > 255.0f)
				g = 255.0f;

			b += scale * lamp->color.vz;

			if (b > 255.0f)
				b = 255.0f;
		}
	}
#endif

	out->r = r;
	out->g = g;
	out->b = b;
}

// [A]
// The car shine texture at a vertex (DrawCarModel).
// NOTE: with OPENDRIVER_PIXEL_LIGHTS it also clears the lamp mark: the shine gets no lamp light
static void SetShine(GrVertex *out, int point_normal)
{
	out->r = lit_vertex_color;
	out->g = lit_vertex_color;
	out->b = lit_vertex_color;

#ifdef OPENDRIVER_PIXEL_LIGHTS
	out->tmuvtx[1].oow = 0.0f;
#endif

	SetTexCoord(out, (float)stru_11D9060[point_normal].field_0, (float)stru_11D9060[point_normal].field_1);
}

// flags of the clipped polygon drawing
#define CLIP_RGB		0x1		// the colour of each point (clip_poly_info)
#define CLIP_UV			0x2		// the texture coordinates of each point
#define CLIP_RGB_BUG	0x4		// see DrawClipFan

#define CLIP_QUAD		0x1		// up to 5 points
#define CLIP_TEST5		0x2		// 5 points are tested off screen only when do_clipping is set

#define CLIP_GLIDE		0x8		// (both) a Glide draw function: its projection and triangles

// [A]
// Projects the clipped polygon (clip_poly_info) to the screen and tests it off screen.
// Returns the number of points to draw: 3 to 5, 0 for none.
static int ProjectClipPoly(VERTEX *scr, int flags)
{
	if (num_clip_poly_points == 0)
		return 0;

	ProjectClipPoint(&scr[0], 0, flags & CLIP_GLIDE);
	ProjectClipPoint(&scr[1], 1, flags & CLIP_GLIDE);
	ProjectClipPoint(&scr[2], 2, flags & CLIP_GLIDE);

	switch (num_clip_poly_points)
	{
		case 3:
			if (PolyOffScreen3(&scr[0], &scr[1], &scr[2]))
				return 0;

			return 3;
		case 4:
			ProjectClipPoint(&scr[3], 3, flags & CLIP_GLIDE);

			if (PolyOffScreen4(&scr[0], &scr[1], &scr[2], &scr[3]))
				return 0;

			return 4;
		case 5:
			if ((flags & CLIP_QUAD) == 0)
				return 0;

			ProjectClipPoint(&scr[3], 3, flags & CLIP_GLIDE);
			ProjectClipPoint(&scr[4], 4, flags & CLIP_GLIDE);

			if (((flags & CLIP_TEST5) == 0 || do_clipping) && PolyOffScreen5(&scr[0], &scr[1], &scr[2], &scr[3], &scr[4]))
				return 0;

			return 5;
	}

	return 0;
}

// [A]
static void SetClipColor(GrVertex *out, int i)
{
	out->r = clip_poly_info[i].r;
	out->g = clip_poly_info[i].g;
	out->b = clip_poly_info[i].b;
}

// [A]
// A draw vertex from point i of the clipped polygon: position and, per flags, colour and
// texture coordinates.
static void SetClipVertex(GrVertex *out, VERTEX *scr, int i, int flags)
{
	out->x = scr[i].vx;
	out->y = scr[i].vy;

	if ((flags & CLIP_GLIDE) == 0)
		out->z = scr[i].vz;

	out->oow = scr[i].oow;

	if (flags & CLIP_RGB)
		SetClipColor(out, i);

	if (flags & CLIP_UV)
	{
		out->tmuvtx[0].sow = clip_poly_info[i].u * scr[i].oow;
		out->tmuvtx[0].tow = clip_poly_info[i].v * scr[i].oow;
	}
}

// [A]
static void DrawClipTriangle(int flags)
{
	if (flags & CLIP_GLIDE)
		guDrawTriangleWithClip(&VertexA, &VertexB, &VertexC);
	else
		gDrawTriangleWithClip(&VertexA, &VertexB, &VertexC);
}

// [A]
// Draws the clipped polygon as triangles of its points: 0 1 2 (3 points); 0 2 1, 0 2 3 (4);
// 0 2 1, 0 2 3, 0 4 3 (5).
// NOTE: with CLIP_RGB_BUG (the triangles G3, GT3) 4 points take the colours of the original:
// VertexA gets point 2's, VertexB keeps the previous polygon's
static void DrawClipFan(VERTEX *scr, int count, int flags)
{
	SetClipVertex(&VertexA, scr, 0, flags);

	if (count == 3)
	{
		SetClipVertex(&VertexB, scr, 1, flags);
		SetClipVertex(&VertexC, scr, 2, flags);
		DrawClipTriangle(flags);
		return;
	}

	if (flags & CLIP_RGB_BUG)
	{
		SetClipColor(&VertexA, 2);
		SetClipVertex(&VertexB, scr, 2, flags & ~CLIP_RGB);
	}
	else
	{
		SetClipVertex(&VertexB, scr, 2, flags);
	}

	SetClipVertex(&VertexC, scr, 1, flags);
	DrawClipTriangle(flags);

	SetClipVertex(&VertexC, scr, 3, flags);
	DrawClipTriangle(flags);

	if (count == 5)
	{
		SetClipVertex(&VertexB, scr, 4, flags);
		DrawClipTriangle(flags);
	}
}

// [A]
// The car shine copy of a clipped draw vertex (GT3_LIT, GT4_LIT): the shine texture u, v went
// through the clipping as field_28, field_2c.
// NOTE: with OPENDRIVER_PIXEL_LIGHTS it also clears the lamp mark copied from the draw vertex
static void SetClipShine(GrVertex *out, GrVertex *from, VERTEX *scr, int i)
{
	*out = *from;

	out->r = lit_vertex_color;
	out->g = lit_vertex_color;
	out->b = lit_vertex_color;

#ifdef OPENDRIVER_PIXEL_LIGHTS
	out->tmuvtx[1].oow = 0.0f;
#endif

	out->tmuvtx[0].sow = clip_poly_info[i].field_28 * scr[i].oow;
	out->tmuvtx[0].tow = clip_poly_info[i].field_2c * scr[i].oow;
}

// NOTE: data only, moved here from the generated stubs: camera_angle starts the file's block
// in IDA, the rest follows the main user
const float VERTEX_SNAPPER = 1048576.0f;
SVECTOR camera_angle;
VECTOR4 camera_position;
char force_transparency;

// [0x42d820] [D]
void SetupCarShine(void)
{
	NumShinyPolys = 0;
	car_shine_table = car_shines;
}

// [0x42d839] [D]
// Queues a car shine triangle; DrawCarShine draws them after the cars.
void AddCarShine(GrVertex *v1, GrVertex *v2, GrVertex *v3, int clipped)
{
	car_shine_table->v0 = *v1;
	car_shine_table->v1 = *v2;
	car_shine_table->v2 = *v3;

	car_shine_table->clipped = clipped;

	NumShinyPolys++;
	car_shine_table++;
}

// [0x42d8f4] [D]
void DrawCarShine(void)
{
	int i;

	if (NumShinyPolys == 0)
		return;

	if (last_state != TEXT3)
	{
		grColorCombine(GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_LOCAL, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_TEXTURE, FXFALSE);
		last_state = TEXT3;
	}

	if (gShinyTexture->tset != last_tset)
	{
		last_tset = gShinyTexture->tset;
		grTexSource(GR_TMU0, TextureSlots[gShinyTexture->tset].texture_ram_address, GR_MIPMAPLEVELMASK_BOTH, TextureSlots[gShinyTexture->tset].TInfo);
	}

	if (last_trans_on != 3)
	{
		grAlphaBlendFunction(GR_BLEND_ONE, GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_ZERO);
		last_trans_on = 3;
	}

	car_shine_table = car_shines;

	if (using_glide)
	{
		for (i = 0; i < NumShinyPolys; i++)
		{
			if (car_shine_table->clipped)
				guDrawTriangleWithClip(&car_shine_table->v0, &car_shine_table->v1, &car_shine_table->v2);
			else
				grDrawTriangle(&car_shine_table->v0, &car_shine_table->v1, &car_shine_table->v2);

			car_shine_table++;
		}
	}
	else
	{
		for (i = 0; i < NumShinyPolys; i++)
		{
			if (car_shine_table->clipped)
				gDrawTriangleWithClip(&car_shine_table->v0, &car_shine_table->v1, &car_shine_table->v2);
			else
				gDrawTriangle(&car_shine_table->v0, &car_shine_table->v1, &car_shine_table->v2);

			car_shine_table++;
		}
	}
}

// [0x42dac4] [D]
// Clips the polygon edge from -> to against the near plane (z = NEAR_CLIP_Z): adds the edge's
// intersection and / or its end to clip_poly_info.
void ClipTestScale(VERTEX *from, VERTEX *to)
{
	float dx;
	float dy;
	float dz;
	float t;

	if (from->vz < NEAR_CLIP_Z && to->vz < NEAR_CLIP_Z)
		return;

	if (from->vz >= NEAR_CLIP_Z && to->vz >= NEAR_CLIP_Z)
	{
		// both in front
		clip_ptr->point.vx = to->vx;
		clip_ptr->point.vy = to->vy;
		clip_ptr->point.vz = to->vz;

		num_clip_poly_points++;
		clip_ptr++;
	}
	else if (from->vz >= NEAR_CLIP_Z)
	{
		// leaving: the intersection
		dx = from->vx - to->vx;
		dy = from->vy - to->vy;
		dz = from->vz - to->vz;

		t = (from->vz - NEAR_CLIP_Z) / dz;

		clip_ptr->point.vx = from->vx - dx * t;
		clip_ptr->point.vy = from->vy - dy * t;
		clip_ptr->point.vz = NEAR_CLIP_Z;

		num_clip_poly_points++;
		clip_ptr++;
	}
	else
	{
		// entering: the intersection and the end
		dx = to->vx - from->vx;
		dy = to->vy - from->vy;
		dz = (float)fabs(to->vz - from->vz);

		t = (to->vz - NEAR_CLIP_Z) / dz;

		clip_ptr->point.vx = to->vx - dx * t;
		clip_ptr->point.vy = to->vy - dy * t;
		clip_ptr->point.vz = NEAR_CLIP_Z;
		clip_ptr++;

		clip_ptr->point.vx = to->vx;
		clip_ptr->point.vy = to->vy;
		clip_ptr->point.vz = to->vz;
		clip_ptr++;

		num_clip_poly_points += 2;
	}
}

// [0x42dd07] [D]
// ClipTestScale with an intensity per end.
void ClipTestScaleIntensity(VERTEX *from, VERTEX *to, float i0, float i1)
{
	float dx;
	float dy;
	float dz;
	float t;

	if (from->vz < NEAR_CLIP_Z && to->vz < NEAR_CLIP_Z)
		return;

	if (from->vz >= NEAR_CLIP_Z && to->vz >= NEAR_CLIP_Z)
	{
		clip_ptr->intensity = i1;
		clip_ptr->point.vx = to->vx;
		clip_ptr->point.vy = to->vy;
		clip_ptr->point.vz = to->vz;

		num_clip_poly_points++;
		clip_ptr++;
	}
	else if (from->vz >= NEAR_CLIP_Z)
	{
		dx = from->vx - to->vx;
		dy = from->vy - to->vy;
		dz = from->vz - to->vz;

		t = (from->vz - NEAR_CLIP_Z) / dz;

		clip_ptr->point.vx = from->vx - dx * t;
		clip_ptr->point.vy = from->vy - dy * t;
		clip_ptr->point.vz = NEAR_CLIP_Z;
		clip_ptr->intensity = i0 - (i0 - i1) * t;

		num_clip_poly_points++;
		clip_ptr++;
	}
	else
	{
		dx = to->vx - from->vx;
		dy = to->vy - from->vy;
		dz = to->vz - from->vz;

		t = (to->vz - NEAR_CLIP_Z) / dz;

		clip_ptr->intensity = i1 - (i1 - i0) * t;
		clip_ptr->point.vx = to->vx - dx * t;
		clip_ptr->point.vy = to->vy - dy * t;
		clip_ptr->point.vz = NEAR_CLIP_Z;
		clip_ptr++;

		clip_ptr->intensity = i1;
		clip_ptr->point.vx = to->vx;
		clip_ptr->point.vy = to->vy;
		clip_ptr->point.vz = to->vz;

		num_clip_poly_points += 2;
		clip_ptr++;
	}
}

// [0x42df86] [D]
// ClipTestScale with texture coordinates per end.
void ClipTestScaleUV(VERTEX *from, VERTEX *to, float u0, float v0, float u1, float v1)
{
	float dx;
	float dy;
	float dz;
	float t;

	if (from->vz < NEAR_CLIP_Z && to->vz < NEAR_CLIP_Z)
		return;

	if (from->vz >= NEAR_CLIP_Z && to->vz >= NEAR_CLIP_Z)
	{
		clip_ptr->u = u1;
		clip_ptr->v = v1;
		clip_ptr->point.vx = to->vx;
		clip_ptr->point.vy = to->vy;
		clip_ptr->point.vz = to->vz;

		num_clip_poly_points++;
		clip_ptr++;
	}
	else if (from->vz >= NEAR_CLIP_Z)
	{
		dx = from->vx - to->vx;
		dy = from->vy - to->vy;
		dz = from->vz - to->vz;

		t = (from->vz - NEAR_CLIP_Z) / dz;

		clip_ptr->u = u0 - (u0 - u1) * t;
		clip_ptr->v = v0 - (v0 - v1) * t;
		clip_ptr->point.vx = from->vx - dx * t;
		clip_ptr->point.vy = from->vy - dy * t;
		clip_ptr->point.vz = NEAR_CLIP_Z;

		num_clip_poly_points++;
		clip_ptr++;
	}
	else
	{
		dx = to->vx - from->vx;
		dy = to->vy - from->vy;
		dz = to->vz - from->vz;

		t = (to->vz - NEAR_CLIP_Z) / dz;

		clip_ptr->u = u1 - (u1 - u0) * t;
		clip_ptr->v = v1 - (v1 - v0) * t;
		clip_ptr->point.vx = to->vx - dx * t;
		clip_ptr->point.vy = to->vy - dy * t;
		clip_ptr->point.vz = NEAR_CLIP_Z;
		clip_ptr++;

		clip_ptr->u = u1;
		clip_ptr->v = v1;
		clip_ptr->point.vx = to->vx;
		clip_ptr->point.vy = to->vy;
		clip_ptr->point.vz = to->vz;

		num_clip_poly_points += 2;
		clip_ptr++;
	}
}

// [0x42e258] [D]
// ClipTestScale with texture coordinates and an intensity per end.
void ClipTestScaleUVIntensity(VERTEX *from, VERTEX *to, float u0, float v0, float i0, float u1, float v1, float i1)
{
	float dx;
	float dy;
	float dz;
	float t;

	if (from->vz < NEAR_CLIP_Z && to->vz < NEAR_CLIP_Z)
		return;

	if (from->vz >= NEAR_CLIP_Z && to->vz >= NEAR_CLIP_Z)
	{
		clip_ptr->u = u1;
		clip_ptr->v = v1;
		clip_ptr->intensity = i1;
		clip_ptr->point.vx = to->vx;
		clip_ptr->point.vy = to->vy;
		clip_ptr->point.vz = to->vz;

		num_clip_poly_points++;
		clip_ptr++;
	}
	else if (from->vz >= NEAR_CLIP_Z)
	{
		dx = from->vx - to->vx;
		dy = from->vy - to->vy;
		dz = from->vz - to->vz;

		t = (from->vz - NEAR_CLIP_Z) / dz;

		clip_ptr->u = u0 - (u0 - u1) * t;
		clip_ptr->v = v0 - (v0 - v1) * t;
		clip_ptr->intensity = i0 - (i0 - i1) * t;
		clip_ptr->point.vx = from->vx - dx * t;
		clip_ptr->point.vy = from->vy - dy * t;
		clip_ptr->point.vz = NEAR_CLIP_Z;

		num_clip_poly_points++;
		clip_ptr++;
	}
	else
	{
		dx = to->vx - from->vx;
		dy = to->vy - from->vy;
		dz = to->vz - from->vz;

		t = (to->vz - NEAR_CLIP_Z) / dz;

		clip_ptr->u = u1 - (u1 - u0) * t;
		clip_ptr->v = v1 - (v1 - v0) * t;
		clip_ptr->intensity = i1 - (i1 - i0) * t;
		clip_ptr->point.vx = to->vx - dx * t;
		clip_ptr->point.vy = to->vy - dy * t;
		clip_ptr->point.vz = NEAR_CLIP_Z;
		clip_ptr++;

		clip_ptr->u = u1;
		clip_ptr->v = v1;
		clip_ptr->intensity = i1;
		clip_ptr->point.vx = to->vx;
		clip_ptr->point.vy = to->vy;
		clip_ptr->point.vz = to->vz;

		num_clip_poly_points += 2;
		clip_ptr++;
	}
}

// [0x42e577] [D]
// ClipTestScale with texture coordinates, two more values (field_28, field_2c) and an
// intensity per end.
void ClipTestScaleUVWHIntensity(VERTEX *from, VERTEX *to, float u0, float v0, float w0, float h0, float i0, float u1, float v1, float w1, float h1, float i1)
{
	float dx;
	float dy;
	float dz;
	float t;

	if (from->vz < NEAR_CLIP_Z && to->vz < NEAR_CLIP_Z)
		return;

	if (from->vz >= NEAR_CLIP_Z && to->vz >= NEAR_CLIP_Z)
	{
		clip_ptr->u = u1;
		clip_ptr->v = v1;
		clip_ptr->field_28 = w1;
		clip_ptr->field_2c = h1;
		clip_ptr->intensity = i1;
		clip_ptr->point.vx = to->vx;
		clip_ptr->point.vy = to->vy;
		clip_ptr->point.vz = to->vz;

		num_clip_poly_points++;
		clip_ptr++;
	}
	else if (from->vz >= NEAR_CLIP_Z)
	{
		dx = from->vx - to->vx;
		dy = from->vy - to->vy;
		dz = from->vz - to->vz;

		t = (from->vz - NEAR_CLIP_Z) / dz;

		clip_ptr->u = u0 - (u0 - u1) * t;
		clip_ptr->v = v0 - (v0 - v1) * t;
		clip_ptr->field_28 = w0 - (w0 - w1) * t;
		clip_ptr->field_2c = h0 - (h0 - h1) * t;
		clip_ptr->intensity = i0 - (i0 - i1) * t;
		clip_ptr->point.vx = from->vx - dx * t;
		clip_ptr->point.vy = from->vy - dy * t;
		clip_ptr->point.vz = NEAR_CLIP_Z;

		num_clip_poly_points++;
		clip_ptr++;
	}
	else
	{
		dx = to->vx - from->vx;
		dy = to->vy - from->vy;
		dz = to->vz - from->vz;

		t = (to->vz - NEAR_CLIP_Z) / dz;

		clip_ptr->u = u1 - (u1 - u0) * t;
		clip_ptr->v = v1 - (v1 - v0) * t;
		clip_ptr->field_28 = w1 - (w1 - w0) * t;
		clip_ptr->field_2c = h1 - (h1 - h0) * t;
		clip_ptr->intensity = i1 - (i1 - i0) * t;
		clip_ptr->point.vx = to->vx - dx * t;
		clip_ptr->point.vy = to->vy - dy * t;
		clip_ptr->point.vz = NEAR_CLIP_Z;
		clip_ptr++;

		clip_ptr->u = u1;
		clip_ptr->v = v1;
		clip_ptr->field_28 = w1;
		clip_ptr->field_2c = h1;
		clip_ptr->intensity = i1;
		clip_ptr->point.vx = to->vx;
		clip_ptr->point.vy = to->vy;
		clip_ptr->point.vz = to->vz;

		num_clip_poly_points += 2;
		clip_ptr++;
	}
}

// [0x42e937] [D]
// ClipTestScale with a colour per end.
void ClipTestScaleRGB(VERTEX *from, VERTEX *to, float r0, float g0, float b0, float r1, float g1, float b1)
{
	float dx;
	float dy;
	float dz;
	float t;

	if (from->vz < NEAR_CLIP_Z && to->vz < NEAR_CLIP_Z)
		return;

	if (from->vz >= NEAR_CLIP_Z && to->vz >= NEAR_CLIP_Z)
	{
		clip_ptr->r = r1;
		clip_ptr->g = g1;
		clip_ptr->b = b1;
		clip_ptr->point.vx = to->vx;
		clip_ptr->point.vy = to->vy;
		clip_ptr->point.vz = to->vz;

		num_clip_poly_points++;
		clip_ptr++;
	}
	else if (from->vz >= NEAR_CLIP_Z)
	{
		dx = from->vx - to->vx;
		dy = from->vy - to->vy;
		dz = from->vz - to->vz;

		t = (from->vz - NEAR_CLIP_Z) / dz;

		clip_ptr->r = r0 - (r0 - r1) * t;
		clip_ptr->g = g0 - (g0 - g1) * t;
		clip_ptr->b = b0 - (b0 - b1) * t;
		clip_ptr->point.vx = from->vx - dx * t;
		clip_ptr->point.vy = from->vy - dy * t;
		clip_ptr->point.vz = NEAR_CLIP_Z;

		num_clip_poly_points++;
		clip_ptr++;
	}
	else
	{
		dx = to->vx - from->vx;
		dy = to->vy - from->vy;
		dz = to->vz - from->vz;

		t = (to->vz - NEAR_CLIP_Z) / dz;

		clip_ptr->r = r1 - (r1 - r0) * t;
		clip_ptr->g = g1 - (g1 - g0) * t;
		clip_ptr->b = b1 - (b1 - b0) * t;
		clip_ptr->point.vx = to->vx - dx * t;
		clip_ptr->point.vy = to->vy - dy * t;
		clip_ptr->point.vz = NEAR_CLIP_Z;
		clip_ptr++;

		clip_ptr->r = r1;
		clip_ptr->g = g1;
		clip_ptr->b = b1;
		clip_ptr->point.vx = to->vx;
		clip_ptr->point.vy = to->vy;
		clip_ptr->point.vz = to->vz;

		num_clip_poly_points += 2;
		clip_ptr++;
	}
}

// [0x42ec56] [D]
// ClipTestScale with a colour and texture coordinates per end.
void ClipTestScaleRGBUV(VERTEX *from, VERTEX *to, float r0, float g0, float b0, float u0, float v0, float r1, float g1, float b1, float u1, float v1)
{
	float dx;
	float dy;
	float dz;
	float t;

	if (from->vz < NEAR_CLIP_Z && to->vz < NEAR_CLIP_Z)
		return;

	if (from->vz >= NEAR_CLIP_Z && to->vz >= NEAR_CLIP_Z)
	{
		clip_ptr->r = r1;
		clip_ptr->g = g1;
		clip_ptr->b = b1;
		clip_ptr->u = u1;
		clip_ptr->v = v1;
		clip_ptr->point.vx = to->vx;
		clip_ptr->point.vy = to->vy;
		clip_ptr->point.vz = to->vz;

		num_clip_poly_points++;
		clip_ptr++;
	}
	else if (from->vz >= NEAR_CLIP_Z)
	{
		dx = from->vx - to->vx;
		dy = from->vy - to->vy;
		dz = from->vz - to->vz;

		t = (from->vz - NEAR_CLIP_Z) / dz;

		clip_ptr->r = r0 - (r0 - r1) * t;
		clip_ptr->g = g0 - (g0 - g1) * t;
		clip_ptr->b = b0 - (b0 - b1) * t;
		clip_ptr->u = u0 - (u0 - u1) * t;
		clip_ptr->v = v0 - (v0 - v1) * t;
		clip_ptr->point.vx = from->vx - dx * t;
		clip_ptr->point.vy = from->vy - dy * t;
		clip_ptr->point.vz = NEAR_CLIP_Z;

		num_clip_poly_points++;
		clip_ptr++;
	}
	else
	{
		dx = to->vx - from->vx;
		dy = to->vy - from->vy;
		dz = to->vz - from->vz;

		t = (to->vz - NEAR_CLIP_Z) / dz;

		clip_ptr->r = r1 - (r1 - r0) * t;
		clip_ptr->g = g1 - (g1 - g0) * t;
		clip_ptr->b = b1 - (b1 - b0) * t;
		clip_ptr->u = u1 - (u1 - u0) * t;
		clip_ptr->v = v1 - (v1 - v0) * t;
		clip_ptr->point.vx = to->vx - dx * t;
		clip_ptr->point.vy = to->vy - dy * t;
		clip_ptr->point.vz = NEAR_CLIP_Z;
		clip_ptr++;

		clip_ptr->r = r1;
		clip_ptr->g = g1;
		clip_ptr->b = b1;
		clip_ptr->u = u1;
		clip_ptr->v = v1;
		clip_ptr->point.vx = to->vx;
		clip_ptr->point.vy = to->vy;
		clip_ptr->point.vz = to->vz;

		num_clip_poly_points += 2;
		clip_ptr++;
	}
}

// [0x42f016] [D]
// ClipTestScale with a colour, texture coordinates and two more values (field_28, field_2c)
// per end.
void ClipTestScaleRGBUVWHIntensity(VERTEX *from, VERTEX *to, float r0, float g0, float b0, float u0, float v0, float w0, float h0, float r1, float g1, float b1, float u1, float v1, float w1, float h1)
{
	float dx;
	float dy;
	float dz;
	float t;

	if (from->vz < NEAR_CLIP_Z && to->vz < NEAR_CLIP_Z)
		return;

	if (from->vz >= NEAR_CLIP_Z && to->vz >= NEAR_CLIP_Z)
	{
		clip_ptr->r = r1;
		clip_ptr->g = g1;
		clip_ptr->b = b1;
		clip_ptr->u = u1;
		clip_ptr->v = v1;
		clip_ptr->field_28 = w1;
		clip_ptr->field_2c = h1;
		clip_ptr->point.vx = to->vx;
		clip_ptr->point.vy = to->vy;
		clip_ptr->point.vz = to->vz;

		num_clip_poly_points++;
		clip_ptr++;
	}
	else if (from->vz >= NEAR_CLIP_Z)
	{
		dx = from->vx - to->vx;
		dy = from->vy - to->vy;
		dz = from->vz - to->vz;

		t = (from->vz - NEAR_CLIP_Z) / dz;

		clip_ptr->r = r0 - (r0 - r1) * t;
		clip_ptr->g = g0 - (g0 - g1) * t;
		clip_ptr->b = b0 - (b0 - b1) * t;
		clip_ptr->u = u0 - (u0 - u1) * t;
		clip_ptr->v = v0 - (v0 - v1) * t;
		clip_ptr->field_28 = w0 - (w0 - w1) * t;
		clip_ptr->field_2c = h0 - (h0 - h1) * t;
		clip_ptr->point.vx = from->vx - dx * t;
		clip_ptr->point.vy = from->vy - dy * t;
		clip_ptr->point.vz = NEAR_CLIP_Z;

		num_clip_poly_points++;
		clip_ptr++;
	}
	else
	{
		dx = to->vx - from->vx;
		dy = to->vy - from->vy;
		dz = to->vz - from->vz;

		t = (to->vz - NEAR_CLIP_Z) / dz;

		clip_ptr->r = r1 - (r1 - r0) * t;
		clip_ptr->g = g1 - (g1 - g0) * t;
		clip_ptr->b = b1 - (b1 - b0) * t;
		clip_ptr->u = u1 - (u1 - u0) * t;
		clip_ptr->v = v1 - (v1 - v0) * t;
		clip_ptr->field_28 = w1 - (w1 - w0) * t;
		clip_ptr->field_2c = h1 - (h1 - h0) * t;
		clip_ptr->point.vx = to->vx - dx * t;
		clip_ptr->point.vy = to->vy - dy * t;
		clip_ptr->point.vz = NEAR_CLIP_Z;
		clip_ptr++;

		// NOTE: field_28 and field_2c of the end point are not set (the original's)
		clip_ptr->r = r1;
		clip_ptr->g = g1;
		clip_ptr->b = b1;
		clip_ptr->u = u1;
		clip_ptr->v = v1;
		clip_ptr->point.vx = to->vx;
		clip_ptr->point.vy = to->vy;
		clip_ptr->point.vz = to->vz;

		num_clip_poly_points += 2;
		clip_ptr++;
	}
}

// [0x42f456] [D]
// Clips the polygon edge from -> to against the far plane (z = far_clip), as ClipTestScale.
void FarClipTestScale(VERTEX *from, VERTEX *to)
{
	float dx;
	float dy;
	float dz;
	float t;

	if (from->vz > far_clip && to->vz > far_clip)
		return;

	if (from->vz <= far_clip && to->vz <= far_clip)
	{
		clip_ptr->point.vx = to->vx;
		clip_ptr->point.vy = to->vy;
		clip_ptr->point.vz = to->vz;

		num_clip_poly_points++;
		clip_ptr++;
	}
	else if (from->vz <= far_clip)
	{
		dx = from->vx - to->vx;
		dy = from->vy - to->vy;
		dz = from->vz - to->vz;

		t = (from->vz - far_clip) / dz;

		clip_ptr->point.vx = from->vx - dx * t;
		clip_ptr->point.vy = from->vy - dy * t;
		clip_ptr->point.vz = far_clip;

		num_clip_poly_points++;
		clip_ptr++;
	}
	else
	{
		dx = to->vx - from->vx;
		dy = to->vy - from->vy;
		dz = to->vz - from->vz;

		t = (to->vz - far_clip) / dz;

		clip_ptr->point.vx = to->vx - dx * t;
		clip_ptr->point.vy = to->vy - dy * t;
		clip_ptr->point.vz = far_clip;
		clip_ptr++;

		clip_ptr->point.vx = to->vx;
		clip_ptr->point.vy = to->vy;
		clip_ptr->point.vz = to->vz;

		num_clip_poly_points += 2;
		clip_ptr++;
	}
}

// [0x42f694] [D]
void FarClipTestScaleUV(VERTEX *from, VERTEX *to, float u0, float v0, float u1, float v1)
{
	float dx;
	float dy;
	float dz;
	float t;

	if (from->vz > far_clip && to->vz > far_clip)
		return;

	if (from->vz <= far_clip && to->vz <= far_clip)
	{
		clip_ptr->u = u1;
		clip_ptr->v = v1;
		clip_ptr->point.vx = to->vx;
		clip_ptr->point.vy = to->vy;
		clip_ptr->point.vz = to->vz;

		num_clip_poly_points++;
		clip_ptr++;
	}
	else if (from->vz <= far_clip)
	{
		dx = from->vx - to->vx;
		dy = from->vy - to->vy;
		dz = from->vz - to->vz;

		t = (from->vz - far_clip) / dz;

		clip_ptr->u = u0 - (u0 - u1) * t;
		clip_ptr->v = v0 - (v0 - v1) * t;
		clip_ptr->point.vx = from->vx - dx * t;
		clip_ptr->point.vy = from->vy - dy * t;
		clip_ptr->point.vz = far_clip;

		num_clip_poly_points++;
		clip_ptr++;
	}
	else
	{
		dx = to->vx - from->vx;
		dy = to->vy - from->vy;
		dz = to->vz - from->vz;

		t = (to->vz - far_clip) / dz;

		clip_ptr->u = u1 - (u1 - u0) * t;
		clip_ptr->v = v1 - (v1 - v0) * t;
		clip_ptr->point.vx = to->vx - dx * t;
		clip_ptr->point.vy = to->vy - dy * t;
		clip_ptr->point.vz = far_clip;
		clip_ptr++;

		clip_ptr->u = u1;
		clip_ptr->v = v1;
		clip_ptr->point.vx = to->vx;
		clip_ptr->point.vy = to->vy;
		clip_ptr->point.vz = to->vz;

		num_clip_poly_points += 2;
		clip_ptr++;
	}
}

// [0x42f967] [D]
void FarClipTestScaleRGBUV(VERTEX *from, VERTEX *to, float r0, float g0, float b0, float u0, float v0, float r1, float g1, float b1, float u1, float v1)
{
	float dx;
	float dy;
	float dz;
	float t;

	if (from->vz > far_clip && to->vz > far_clip)
		return;

	if (from->vz <= far_clip && to->vz <= far_clip)
	{
		clip_ptr->r = r1;
		clip_ptr->g = g1;
		clip_ptr->b = b1;
		clip_ptr->u = u1;
		clip_ptr->v = v1;
		clip_ptr->point.vx = to->vx;
		clip_ptr->point.vy = to->vy;
		clip_ptr->point.vz = to->vz;

		num_clip_poly_points++;
		clip_ptr++;
	}
	else if (from->vz <= far_clip)
	{
		dx = from->vx - to->vx;
		dy = from->vy - to->vy;
		dz = from->vz - to->vz;

		t = (from->vz - far_clip) / dz;

		clip_ptr->r = r0 - (r0 - r1) * t;
		clip_ptr->g = g0 - (g0 - g1) * t;
		clip_ptr->b = b0 - (b0 - b1) * t;
		clip_ptr->u = u0 - (u0 - u1) * t;
		clip_ptr->v = v0 - (v0 - v1) * t;
		clip_ptr->point.vx = from->vx - dx * t;
		clip_ptr->point.vy = from->vy - dy * t;
		clip_ptr->point.vz = far_clip;

		num_clip_poly_points++;
		clip_ptr++;
	}
	else
	{
		dx = to->vx - from->vx;
		dy = to->vy - from->vy;
		dz = to->vz - from->vz;

		t = (to->vz - far_clip) / dz;

		clip_ptr->r = r1 - (r1 - r0) * t;
		clip_ptr->g = g1 - (g1 - g0) * t;
		clip_ptr->b = b1 - (b1 - b0) * t;
		clip_ptr->u = u1 - (u1 - u0) * t;
		clip_ptr->v = v1 - (v1 - v0) * t;
		clip_ptr->point.vx = to->vx - dx * t;
		clip_ptr->point.vy = to->vy - dy * t;
		clip_ptr->point.vz = far_clip;
		clip_ptr++;

		clip_ptr->r = r1;
		clip_ptr->g = g1;
		clip_ptr->b = b1;
		clip_ptr->u = u1;
		clip_ptr->v = v1;
		clip_ptr->point.vx = to->vx;
		clip_ptr->point.vy = to->vy;
		clip_ptr->point.vz = to->vz;

		num_clip_poly_points += 2;
		clip_ptr++;
	}
}

// [0x42fd28] [D]
// The draw function of the polygon types not drawn: skips the polygon.
// NOTE: IDA shows an int return (polyblock's old value left in eax); nothing uses it
void SkipDrawPOLY(int trans)
{
	polyblock += PolySizes[POLY_TYPE(polyblock)];
}

// [0x42fd49] [D]
void DrawPOLYF3(int trans)
{
	POLYF3 *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;

	poly = (POLYF3 *)polyblock;
	polyblock += sizeof(POLYF3);

	v0 = GetVertex(poly->v0, VertexRotatePers);
	v1 = GetVertex(poly->v1, VertexRotatePers);
	v2 = GetVertex(poly->v2, VertexRotatePers);

	if (do_clipping && PolyOffScreen3(v0, v1, v2))
		return;

	SetColor(&VertexA, poly->r, poly->g, poly->b);
	SetColor(&VertexB, poly->r, poly->g, poly->b);
	SetColor(&VertexC, poly->r, poly->g, poly->b);

	SetFlatColorCombine();
	SetTransparency(trans);

	SetGlidePosition(&VertexA, v0);
	SetGlidePosition(&VertexB, v1);
	SetGlidePosition(&VertexC, v2);

	DrawGlideTriangleABC();
}

// [0x430199] [D]
// Quads are drawn as the triangles v0 v2 v1 and v0 v2 v3.
void DrawPOLYF4(int trans)
{
	POLYF4 *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	VERTEX *v3;

	poly = (POLYF4 *)polyblock;
	polyblock += sizeof(POLYF4);

	v0 = GetVertex(poly->v0, VertexRotatePers);
	v1 = GetVertex(poly->v1, VertexRotatePers);
	v2 = GetVertex(poly->v2, VertexRotatePers);
	v3 = GetVertex(poly->v3, VertexRotatePers);

	if (do_clipping && PolyOffScreen4(v0, v1, v2, v3))
		return;

	SetColor(&VertexA, poly->r, poly->g, poly->b);
	SetColor(&VertexB, poly->r, poly->g, poly->b);
	SetColor(&VertexC, poly->r, poly->g, poly->b);

	SetFlatColorCombine();
	SetTransparency(trans);

	SetGlidePosition(&VertexA, v0);
	SetGlidePosition(&VertexB, v2);
	SetGlidePosition(&VertexC, v1);
	DrawGlideTriangleABC();

	SetGlidePosition(&VertexC, v3);
	DrawGlideTriangleABC();
}

// [0x4306e4] [D]
void DrawPOLYG3(int trans)
{
	POLYG3 *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;

	poly = (POLYG3 *)polyblock;
	polyblock += sizeof(POLYG3);

	v0 = GetVertex(poly->v0, VertexRotatePers);
	v1 = GetVertex(poly->v1, VertexRotatePers);
	v2 = GetVertex(poly->v2, VertexRotatePers);

	if (do_clipping && PolyOffScreen3(v0, v1, v2))
		return;

	SetColor(&VertexA, poly->r0, poly->g0, poly->b0);
	SetColor(&VertexB, poly->r1, poly->g1, poly->b1);
	SetColor(&VertexC, poly->r2, poly->g2, poly->b2);

	SetFlatColorCombine();
	SetTransparency(trans);

	SetGlidePosition(&VertexA, v0);
	SetGlidePosition(&VertexB, v1);
	SetGlidePosition(&VertexC, v2);

	DrawGlideTriangleABC();
}

// [0x430b36] [D]
void DrawPOLYG4(int trans)
{
	POLYG4 *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	VERTEX *v3;

	poly = (POLYG4 *)polyblock;
	polyblock += sizeof(POLYG4);

	v0 = GetVertex(poly->v0, VertexRotatePers);
	v1 = GetVertex(poly->v1, VertexRotatePers);
	v2 = GetVertex(poly->v2, VertexRotatePers);
	v3 = GetVertex(poly->v3, VertexRotatePers);

	if (do_clipping && PolyOffScreen4(v0, v1, v2, v3))
		return;

	// NOTE: the colours of v1 and v2 are swapped and v3's is not used (the original's)
	SetColor(&VertexA, poly->r0, poly->g0, poly->b0);
	SetColor(&VertexB, poly->r1, poly->g1, poly->b1);
	SetColor(&VertexC, poly->r2, poly->g2, poly->b2);

	SetFlatColorCombine();
	SetTransparency(trans);

	SetGlidePosition(&VertexA, v0);
	SetGlidePosition(&VertexB, v2);
	SetGlidePosition(&VertexC, v1);
	DrawGlideTriangleABC();

	SetGlidePosition(&VertexC, v3);
	DrawGlideTriangleABC();
}

// [0x431083] [D]
void DrawPOLYFT3(int trans)
{
	POLYFT3 *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;

	poly = (POLYFT3 *)polyblock;
	polyblock += sizeof(POLYFT3);

	v0 = GetVertex(poly->v0, VertexRotatePers);
	v1 = GetVertex(poly->v1, VertexRotatePers);
	v2 = GetVertex(poly->v2, VertexRotatePers);

	if (do_clipping && PolyOffScreen3(v0, v1, v2))
		return;

	SetColor(&VertexA, poly->r, poly->g, poly->b);
	SetColor(&VertexB, poly->r, poly->g, poly->b);
	SetColor(&VertexC, poly->r, poly->g, poly->b);

	SetTexturedColorCombine();
	SetTextureSet(poly->texture_set);
	SetTransparency(trans);

	SetGlidePosition(&VertexA, v0);
	SetGlidePosition(&VertexB, v1);
	SetGlidePosition(&VertexC, v2);

	SetTexCoord(&VertexA, poly->s0, poly->t0);
	SetTexCoord(&VertexB, poly->s1, poly->t1);
	SetTexCoord(&VertexC, poly->s2, poly->t2);

	DrawGlideTriangleABC();
}

// [0x431586] [D]
void DrawPOLYFT4(int trans)
{
	POLYFT4 *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	VERTEX *v3;

	poly = (POLYFT4 *)polyblock;
	polyblock += sizeof(POLYFT4);

	v0 = GetVertex(poly->v0, VertexRotatePers);
	v1 = GetVertex(poly->v1, VertexRotatePers);
	v2 = GetVertex(poly->v2, VertexRotatePers);
	v3 = GetVertex(poly->v3, VertexRotatePers);

	if (do_clipping && PolyOffScreen4(v0, v1, v2, v3))
		return;

	SetColor(&VertexA, poly->r, poly->g, poly->b);
	SetColor(&VertexB, poly->r, poly->g, poly->b);
	SetColor(&VertexC, poly->r, poly->g, poly->b);

	SetTexturedColorCombine();
	SetTextureSet(poly->texture_set);
	SetTransparency(trans);

	SetGlidePosition(&VertexA, v0);
	SetGlidePosition(&VertexB, v2);
	SetGlidePosition(&VertexC, v1);

	SetTexCoord(&VertexA, poly->s0, poly->t0);
	SetTexCoord(&VertexB, poly->s2, poly->t2);
	SetTexCoord(&VertexC, poly->s1, poly->t1);
	DrawGlideTriangleABC();

	SetGlidePosition(&VertexC, v3);
	SetTexCoord(&VertexC, poly->s3, poly->t3);
	DrawGlideTriangleABC();
}

// [0x431ba9] [D]
void DrawPOLYGT3(int trans)
{
	POLYGT3 *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;

	poly = (POLYGT3 *)polyblock;
	polyblock += sizeof(POLYGT3);

	v0 = GetVertex(poly->v0, VertexRotatePers);
	v1 = GetVertex(poly->v1, VertexRotatePers);
	v2 = GetVertex(poly->v2, VertexRotatePers);

	if (do_clipping && PolyOffScreen3(v0, v1, v2))
		return;

	SetColor(&VertexA, poly->r0, poly->g0, poly->b0);
	SetColor(&VertexB, poly->r1, poly->g1, poly->b1);
	SetColor(&VertexC, poly->r2, poly->g2, poly->b2);

	SetTexturedColorCombine();
	SetTextureSet(poly->texture_set);
	SetTransparency(trans);

	SetGlidePosition(&VertexA, v0);
	SetGlidePosition(&VertexB, v1);
	SetGlidePosition(&VertexC, v2);

	SetTexCoord(&VertexA, poly->s0, poly->t0);
	SetTexCoord(&VertexB, poly->s1, poly->t1);
	SetTexCoord(&VertexC, poly->s2, poly->t2);

	DrawGlideTriangleABC();
}

// [0x4320ae] [D]
void DrawPOLYGT4(int trans)
{
	POLYGT4 *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	VERTEX *v3;

	poly = (POLYGT4 *)polyblock;
	polyblock += sizeof(POLYGT4);

	v0 = GetVertex(poly->v0, VertexRotatePers);
	v1 = GetVertex(poly->v1, VertexRotatePers);
	v2 = GetVertex(poly->v2, VertexRotatePers);
	v3 = GetVertex(poly->v3, VertexRotatePers);

	if (do_clipping && PolyOffScreen4(v0, v1, v2, v3))
		return;

	SetColor(&VertexA, poly->r0, poly->g0, poly->b0);
	SetColor(&VertexB, poly->r2, poly->g2, poly->b2);
	SetColor(&VertexC, poly->r1, poly->g1, poly->b1);

	SetTexturedColorCombine();
	SetTextureSet(poly->texture_set);
	SetTransparency(trans);

	SetGlidePosition(&VertexA, v0);
	SetGlidePosition(&VertexB, v2);
	SetGlidePosition(&VertexC, v1);

	SetTexCoord(&VertexA, poly->s0, poly->t0);
	SetTexCoord(&VertexB, poly->s2, poly->t2);
	SetTexCoord(&VertexC, poly->s1, poly->t1);
	DrawGlideTriangleABC();

	SetGlidePosition(&VertexC, v3);
	SetTexCoord(&VertexC, poly->s3, poly->t3);
	SetColor(&VertexC, poly->r3, poly->g3, poly->b3);
	DrawGlideTriangleABC();
}

// [0x4326f6] [D]
// The _LIT polygons scale their colour by the light intensity of their normal (DrawObject)
// or of each point normal (G, GT), plus the lamps (FT, GT).
void DrawPOLYF3_LIT(int trans)
{
	POLYF3LIT *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	float li;

	poly = (POLYF3LIT *)polyblock;
	polyblock += sizeof(POLYF3LIT);

	v0 = GetVertex(poly->v0, VertexRotatePers);
	v1 = GetVertex(poly->v1, VertexRotatePers);
	v2 = GetVertex(poly->v2, VertexRotatePers);

	if (do_clipping && PolyOffScreen3(v0, v1, v2))
		return;

	li = light_intensity[poly->normal];

	SetColor(&VertexA, li * poly->r, li * poly->g, li * poly->b);
	SetColor(&VertexB, li * poly->r, li * poly->g, li * poly->b);
	SetColor(&VertexC, li * poly->r, li * poly->g, li * poly->b);

	SetFlatColorCombine();
	SetTransparency(trans);

	SetGlidePosition(&VertexA, v0);
	SetGlidePosition(&VertexB, v1);
	SetGlidePosition(&VertexC, v2);

	DrawGlideTriangleABC();
}

// [0x432b71] [D]
void DrawPOLYF4_LIT(int trans)
{
	POLYF4LIT *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	VERTEX *v3;
	float li;

	poly = (POLYF4LIT *)polyblock;
	polyblock += sizeof(POLYF4LIT);

	v0 = GetVertex(poly->v0, VertexRotatePers);
	v1 = GetVertex(poly->v1, VertexRotatePers);
	v2 = GetVertex(poly->v2, VertexRotatePers);
	v3 = GetVertex(poly->v3, VertexRotatePers);

	if (do_clipping && PolyOffScreen4(v0, v1, v2, v3))
		return;

	li = light_intensity[poly->normal];

	SetColor(&VertexA, li * poly->r, li * poly->g, li * poly->b);
	SetColor(&VertexB, li * poly->r, li * poly->g, li * poly->b);
	SetColor(&VertexC, li * poly->r, li * poly->g, li * poly->b);

	SetFlatColorCombine();
	SetTransparency(trans);

	SetGlidePosition(&VertexA, v0);
	SetGlidePosition(&VertexB, v2);
	SetGlidePosition(&VertexC, v1);
	DrawGlideTriangleABC();

	SetGlidePosition(&VertexC, v3);
	DrawGlideTriangleABC();
}

// [0x4330e7] [D]
void DrawPOLYG3_LIT(int trans)
{
	POLYG3LIT *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	float li;

	poly = (POLYG3LIT *)polyblock;
	polyblock += sizeof(POLYG3LIT);

	v0 = GetVertex(poly->v0, VertexRotatePers);
	v1 = GetVertex(poly->v1, VertexRotatePers);
	v2 = GetVertex(poly->v2, VertexRotatePers);

	if (do_clipping && PolyOffScreen3(v0, v1, v2))
		return;

	li = conv_point_normals[poly->pn0];
	SetColor(&VertexA, poly->r * li, poly->g * li, poly->b * li);

	li = conv_point_normals[poly->pn1];
	SetColor(&VertexB, poly->r * li, poly->g * li, poly->b * li);

	li = conv_point_normals[poly->pn2];
	SetColor(&VertexC, poly->r * li, poly->g * li, poly->b * li);

	SetFlatColorCombine();
	SetTransparency(trans);

	SetGlidePosition(&VertexA, v0);
	SetGlidePosition(&VertexB, v1);
	SetGlidePosition(&VertexC, v2);

	DrawGlideTriangleABC();
}

// [0x433587] [D]
void DrawPOLYG4_LIT(int trans)
{
	POLYG4LIT *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	VERTEX *v3;
	float li;

	poly = (POLYG4LIT *)polyblock;
	polyblock += sizeof(POLYG4LIT);

	v0 = GetVertex(poly->v0, VertexRotatePers);
	v1 = GetVertex(poly->v1, VertexRotatePers);
	v2 = GetVertex(poly->v2, VertexRotatePers);
	v3 = GetVertex(poly->v3, VertexRotatePers);

	if (do_clipping && PolyOffScreen4(v0, v1, v2, v3))
		return;

	li = conv_point_normals[poly->pn0];
	SetColor(&VertexA, poly->r * li, poly->g * li, poly->b * li);

	li = conv_point_normals[poly->pn2];
	SetColor(&VertexB, poly->r * li, poly->g * li, poly->b * li);

	li = conv_point_normals[poly->pn1];
	SetColor(&VertexC, poly->r * li, poly->g * li, poly->b * li);

	SetFlatColorCombine();
	SetTransparency(trans);

	SetGlidePosition(&VertexA, v0);
	SetGlidePosition(&VertexB, v2);
	SetGlidePosition(&VertexC, v1);
	DrawGlideTriangleABC();

	SetGlidePosition(&VertexC, v3);

	li = conv_point_normals[poly->pn3];
	SetColor(&VertexC, poly->r * li, poly->g * li, poly->b * li);

	DrawGlideTriangleABC();
}

// [0x433b56] [D]
void DrawPOLYFT3_LIT(int trans)
{
	POLYFT3LIT *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	float li;
	float r;
	float g;
	float b;

	poly = (POLYFT3LIT *)polyblock;
	polyblock += sizeof(POLYFT3LIT);

	v0 = GetVertex(poly->v0, VertexRotatePers);
	v1 = GetVertex(poly->v1, VertexRotatePers);
	v2 = GetVertex(poly->v2, VertexRotatePers);

	if (do_clipping && PolyOffScreen3(v0, v1, v2))
		return;

	li = light_intensity[poly->normal];

	r = li * poly->r;
	g = li * poly->g;
	b = li * poly->b;

	if (num_potential_lamps)
	{
		AddLampLight(&VertexA, v0->tx, v0->ty, v0->vz, r, g, b);
		AddLampLight(&VertexB, v1->tx, v1->ty, v1->vz, r, g, b);
		AddLampLight(&VertexC, v2->tx, v2->ty, v2->vz, r, g, b);
	}
	else
	{
		SetColor(&VertexA, r, g, b);
		SetColor(&VertexB, r, g, b);
		SetColor(&VertexC, r, g, b);
	}

	SetTexturedColorCombine();
	SetTextureSet(poly->texture_set);
	SetTransparency(trans);

	SetGlidePosition(&VertexA, v0);
	SetGlidePosition(&VertexB, v1);
	SetGlidePosition(&VertexC, v2);

	SetTexCoord(&VertexA, poly->s0, poly->t0);
	SetTexCoord(&VertexB, poly->s1, poly->t1);
	SetTexCoord(&VertexC, poly->s2, poly->t2);

	DrawGlideTriangleABC();
}

// [0x4344bc] [D]
void DrawPOLYFT4_LIT(int trans)
{
	POLYFT4LIT *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	VERTEX *v3;
	float li;
	float r;
	float g;
	float b;

	poly = (POLYFT4LIT *)polyblock;
	polyblock += sizeof(POLYFT4LIT);

	v0 = GetVertex(poly->v0, VertexRotatePers);
	v1 = GetVertex(poly->v1, VertexRotatePers);
	v2 = GetVertex(poly->v2, VertexRotatePers);
	v3 = GetVertex(poly->v3, VertexRotatePers);

	if (do_clipping && PolyOffScreen4(v0, v1, v2, v3))
		return;

	li = light_intensity[poly->normal];

	r = li * poly->r;
	g = li * poly->g;
	b = li * poly->b;

	if (num_potential_lamps)
	{
		AddLampLight(&VertexA, v0->tx, v0->ty, v0->vz, r, g, b);
		AddLampLight(&VertexB, v2->tx, v2->ty, v2->vz, r, g, b);
		AddLampLight(&VertexC, v1->tx, v1->ty, v1->vz, r, g, b);
	}
	else
	{
		SetColor(&VertexA, r, g, b);
		SetColor(&VertexB, r, g, b);
		SetColor(&VertexC, r, g, b);
	}

	SetTexturedColorCombine();
	SetTextureSet(poly->texture_set);
	SetTransparency(trans);

	SetGlidePosition(&VertexA, v0);
	SetGlidePosition(&VertexB, v2);
	SetGlidePosition(&VertexC, v1);

	SetTexCoord(&VertexA, poly->s0, poly->t0);
	SetTexCoord(&VertexB, poly->s2, poly->t2);
	SetTexCoord(&VertexC, poly->s1, poly->t1);
	DrawGlideTriangleABC();

	if (num_potential_lamps)
		AddLampLight(&VertexC, v3->tx, v3->ty, v3->vz, r, g, b);

	SetGlidePosition(&VertexC, v3);
	SetTexCoord(&VertexC, poly->s3, poly->t3);
	DrawGlideTriangleABC();
}

// [0x4351c8] [D]
// With gShinyType (DrawCarModel) also queues the car shine of the polygon; type 2 draws only
// the shine.
void DrawPOLYGT3_LIT(int trans)
{
	POLYGT3LIT *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	float li;

	poly = (POLYGT3LIT *)polyblock;
	polyblock += sizeof(POLYGT3LIT);

	v0 = GetVertex(poly->v0, VertexRotatePers);
	v1 = GetVertex(poly->v1, VertexRotatePers);
	v2 = GetVertex(poly->v2, VertexRotatePers);

	if (do_clipping && PolyOffScreen3(v0, v1, v2))
		return;

	if (num_potential_lamps)
	{
		li = conv_point_normals[poly->pn0];
		AddLampLight(&VertexA, v0->tx, v0->ty, v0->vz, li * poly->r, li * poly->g, li * poly->b);

		li = conv_point_normals[poly->pn1];
		AddLampLight(&VertexB, v1->tx, v1->ty, v1->vz, li * poly->r, li * poly->g, li * poly->b);

		li = conv_point_normals[poly->pn2];
		AddLampLight(&VertexC, v2->tx, v2->ty, v2->vz, li * poly->r, li * poly->g, li * poly->b);
	}
	else
	{
		li = conv_point_normals[poly->pn0];
		SetColor(&VertexA, poly->r * li, poly->g * li, poly->b * li);

		li = conv_point_normals[poly->pn1];
		SetColor(&VertexB, poly->r * li, poly->g * li, poly->b * li);

		li = conv_point_normals[poly->pn2];
		SetColor(&VertexC, poly->r * li, poly->g * li, poly->b * li);
	}

	SetTexturedColorCombine();
	SetTextureSet(poly->texture_set);
	SetTransparency(trans);

	SetGlidePosition(&VertexA, v0);
	SetGlidePosition(&VertexB, v1);
	SetGlidePosition(&VertexC, v2);

	SetTexCoord(&VertexA, poly->s0, poly->t0);
	SetTexCoord(&VertexB, poly->s1, poly->t1);
	SetTexCoord(&VertexC, poly->s2, poly->t2);

	if (gShinyType != 2)
		DrawGlideTriangleABC();

	if (gShinyType)
	{
		SetShine(&VertexA, poly->pn0);
		SetShine(&VertexB, poly->pn1);
		SetShine(&VertexC, poly->pn2);

		AddCarShine(&VertexA, &VertexB, &VertexC, do_clipping ? 1 : 0);
	}
}

// [0x435eae] [D]
void DrawPOLYGT4_LIT(int trans)
{
	POLYGT4LIT *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	VERTEX *v3;
	float li;

	poly = (POLYGT4LIT *)polyblock;
	polyblock += sizeof(POLYGT4LIT);

	v0 = GetVertex(poly->v0, VertexRotatePers);
	v1 = GetVertex(poly->v1, VertexRotatePers);
	v2 = GetVertex(poly->v2, VertexRotatePers);
	v3 = GetVertex(poly->v3, VertexRotatePers);

	if (do_clipping && PolyOffScreen4(v0, v1, v2, v3))
		return;

	if (num_potential_lamps)
	{
		li = conv_point_normals[poly->pn0];
		AddLampLight(&VertexA, v0->tx, v0->ty, v0->vz, li * poly->r, li * poly->g, li * poly->b);

		li = conv_point_normals[poly->pn2];
		AddLampLight(&VertexB, v2->tx, v2->ty, v2->vz, li * poly->r, li * poly->g, li * poly->b);

		li = conv_point_normals[poly->pn1];
		AddLampLight(&VertexC, v1->tx, v1->ty, v1->vz, li * poly->r, li * poly->g, li * poly->b);
	}
	else
	{
		li = conv_point_normals[poly->pn0];
		SetColor(&VertexA, poly->r * li, poly->g * li, poly->b * li);

		li = conv_point_normals[poly->pn2];
		SetColor(&VertexB, poly->r * li, poly->g * li, poly->b * li);

		li = conv_point_normals[poly->pn1];
		SetColor(&VertexC, poly->r * li, poly->g * li, poly->b * li);
	}

	SetTexturedColorCombine();
	SetTextureSet(poly->texture_set);
	SetTransparency(trans);

	SetGlidePosition(&VertexA, v0);
	SetGlidePosition(&VertexB, v2);
	SetGlidePosition(&VertexC, v1);

	SetTexCoord(&VertexA, poly->s0, poly->t0);
	SetTexCoord(&VertexB, poly->s2, poly->t2);
	SetTexCoord(&VertexC, poly->s1, poly->t1);

	if (gShinyType != 2)
		DrawGlideTriangleABC();

	SetGlidePosition(&VertexC, v3);
	SetTexCoord(&VertexC, poly->s3, poly->t3);

	li = conv_point_normals[poly->pn3];

	if (num_potential_lamps)
		AddLampLight(&VertexC, v3->tx, v3->ty, v3->vz, li * poly->r, li * poly->g, li * poly->b);
	else
		SetColor(&VertexC, poly->r * li, poly->g * li, poly->b * li);

	if (gShinyType != 2)
		DrawGlideTriangleABC();

	if (gShinyType)
	{
		// NOTE: VertexC keeps v3's z for the first shine triangle (the original's)
		VertexC.x = v1->vx;
		VertexC.y = v1->vy;
		VertexC.oow = v1->oow;

		SetShine(&VertexA, poly->pn0);
		SetShine(&VertexB, poly->pn2);
		SetShine(&VertexC, poly->pn1);

		AddCarShine(&VertexA, &VertexB, &VertexC, do_clipping ? 1 : 0);

		VertexC.x = v3->vx;
		VertexC.y = v3->vy;
		VertexC.oow = v3->oow;

		SetTexCoord(&VertexC, (float)stru_11D9060[poly->pn3].field_0, (float)stru_11D9060[poly->pn3].field_1);

		AddCarShine(&VertexA, &VertexB, &VertexC, do_clipping ? 1 : 0);
	}
}

// [0x437020] [D]
// The near clipped polygons (the object crosses z = NEAR_CLIP_Z): the vertices are rotated
// only, the polygon clipped (ClipTestScale*) and the points projected here.
void DrawClippedPOLYF3(int trans)
{
	POLYF3 *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	VERTEX scr[5];
	int count;

	poly = (POLYF3 *)polyblock;
	polyblock += sizeof(POLYF3);

	v0 = GetVertex(poly->v0, VertexRotate);
	v1 = GetVertex(poly->v1, VertexRotate);
	v2 = GetVertex(poly->v2, VertexRotate);

	num_clip_poly_points = 0;
	clip_ptr = clip_poly_info;

	ClipTestScale(v0, v1);
	ClipTestScale(v1, v2);
	ClipTestScale(v2, v0);

	count = ProjectClipPoly(scr, CLIP_GLIDE);

	if (count == 0)
		return;

	SetFlatColorCombine();
	SetTransparency(trans);

	SetColor(&VertexA, poly->r, poly->g, poly->b);
	SetColor(&VertexB, poly->r, poly->g, poly->b);
	SetColor(&VertexC, poly->r, poly->g, poly->b);

	DrawClipFan(scr, count, CLIP_GLIDE);
}

// [0x4378e9] [D]
void DrawClippedPOLYF4(int trans)
{
	POLYF4 *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	VERTEX *v3;
	VERTEX scr[5];
	int count;

	poly = (POLYF4 *)polyblock;
	polyblock += sizeof(POLYF4);

	v0 = GetVertex(poly->v0, VertexRotate);
	v1 = GetVertex(poly->v1, VertexRotate);
	v2 = GetVertex(poly->v2, VertexRotate);
	v3 = GetVertex(poly->v3, VertexRotate);

	num_clip_poly_points = 0;
	clip_ptr = clip_poly_info;

	ClipTestScale(v0, v1);
	ClipTestScale(v1, v2);
	ClipTestScale(v2, v3);
	ClipTestScale(v3, v0);

	count = ProjectClipPoly(scr, CLIP_QUAD | CLIP_TEST5 | CLIP_GLIDE);

	if (count == 0)
		return;

	SetFlatColorCombine();
	SetTransparency(trans);

	SetColor(&VertexA, poly->r, poly->g, poly->b);
	SetColor(&VertexB, poly->r, poly->g, poly->b);
	SetColor(&VertexC, poly->r, poly->g, poly->b);

	DrawClipFan(scr, count, CLIP_GLIDE);
}

// [0x438694] [D]
void DrawClippedPOLYG3(int trans)
{
	POLYG3 *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	VERTEX scr[5];
	int count;

	poly = (POLYG3 *)polyblock;
	polyblock += sizeof(POLYG3);

	v0 = GetVertex(poly->v0, VertexRotate);
	v1 = GetVertex(poly->v1, VertexRotate);
	v2 = GetVertex(poly->v2, VertexRotate);

	num_clip_poly_points = 0;
	clip_ptr = clip_poly_info;

	ClipTestScaleRGB(v1, v2, poly->r1, poly->g1, poly->b1, poly->r2, poly->g2, poly->b2);
	ClipTestScaleRGB(v2, v0, poly->r2, poly->g2, poly->b2, poly->r0, poly->g0, poly->b0);
	ClipTestScaleRGB(v0, v1, poly->r0, poly->g0, poly->b0, poly->r1, poly->g1, poly->b1);

	count = ProjectClipPoly(scr, CLIP_GLIDE);

	if (count == 0)
		return;

	SetFlatColorCombine();
	SetTransparency(trans);

	DrawClipFan(scr, count, CLIP_RGB | CLIP_RGB_BUG | CLIP_GLIDE);
}

// [0x439097] [D]
void DrawClippedPOLYG4(int trans)
{
	POLYG4 *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	VERTEX *v3;
	VERTEX scr[5];
	int count;

	poly = (POLYG4 *)polyblock;
	polyblock += sizeof(POLYG4);

	v0 = GetVertex(poly->v0, VertexRotate);
	v1 = GetVertex(poly->v1, VertexRotate);
	v2 = GetVertex(poly->v2, VertexRotate);
	v3 = GetVertex(poly->v3, VertexRotate);

	num_clip_poly_points = 0;
	clip_ptr = clip_poly_info;

	ClipTestScaleRGB(v3, v0, poly->r3, poly->g3, poly->b3, poly->r0, poly->g0, poly->b0);
	ClipTestScaleRGB(v0, v1, poly->r0, poly->g0, poly->b0, poly->r1, poly->g1, poly->b1);
	ClipTestScaleRGB(v1, v2, poly->r1, poly->g1, poly->b1, poly->r2, poly->g2, poly->b2);
	ClipTestScaleRGB(v2, v3, poly->r2, poly->g2, poly->b2, poly->r3, poly->g3, poly->b3);

	count = ProjectClipPoly(scr, CLIP_QUAD | CLIP_TEST5 | CLIP_GLIDE);

	if (count == 0)
		return;

	SetFlatColorCombine();
	SetTransparency(trans);

	DrawClipFan(scr, count, CLIP_RGB | CLIP_GLIDE);
}

// [0x43a0b2] [D]
void DrawClippedPOLYFT3(int trans)
{
	POLYFT3 *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	VERTEX scr[5];
	int count;

	poly = (POLYFT3 *)polyblock;
	polyblock += sizeof(POLYFT3);

	v0 = GetVertex(poly->v0, VertexRotate);
	v1 = GetVertex(poly->v1, VertexRotate);
	v2 = GetVertex(poly->v2, VertexRotate);

	num_clip_poly_points = 0;
	clip_ptr = clip_poly_info;

	ClipTestScaleUV(v0, v1, poly->s0, poly->t0, poly->s1, poly->t1);
	ClipTestScaleUV(v1, v2, poly->s1, poly->t1, poly->s2, poly->t2);
	ClipTestScaleUV(v2, v0, poly->s2, poly->t2, poly->s0, poly->t0);

	count = ProjectClipPoly(scr, CLIP_GLIDE);

	if (count == 0)
		return;

	SetTexturedColorCombine();
	SetTextureSet(poly->texture_set);
	SetTransparency(trans);

	SetColor(&VertexA, poly->r, poly->g, poly->b);
	SetColor(&VertexB, poly->r, poly->g, poly->b);
	SetColor(&VertexC, poly->r, poly->g, poly->b);

	DrawClipFan(scr, count, CLIP_UV | CLIP_GLIDE);
}

// [0x43ab2e] [D]
void DrawClippedPOLYFT4(int trans)
{
	POLYFT4 *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	VERTEX *v3;
	VERTEX scr[5];
	int count;

	poly = (POLYFT4 *)polyblock;
	polyblock += sizeof(POLYFT4);

	v0 = GetVertex(poly->v0, VertexRotate);
	v1 = GetVertex(poly->v1, VertexRotate);
	v2 = GetVertex(poly->v2, VertexRotate);
	v3 = GetVertex(poly->v3, VertexRotate);

	num_clip_poly_points = 0;
	clip_ptr = clip_poly_info;

	ClipTestScaleUV(v0, v1, poly->s0, poly->t0, poly->s1, poly->t1);
	ClipTestScaleUV(v1, v2, poly->s1, poly->t1, poly->s2, poly->t2);
	ClipTestScaleUV(v2, v3, poly->s2, poly->t2, poly->s3, poly->t3);
	ClipTestScaleUV(v3, v0, poly->s3, poly->t3, poly->s0, poly->t0);

	count = ProjectClipPoly(scr, CLIP_QUAD | CLIP_TEST5 | CLIP_GLIDE);

	if (count == 0)
		return;

	SetTexturedColorCombine();
	SetTextureSet(poly->texture_set);
	SetTransparency(trans);

	SetColor(&VertexA, poly->r, poly->g, poly->b);
	SetColor(&VertexB, poly->r, poly->g, poly->b);
	SetColor(&VertexC, poly->r, poly->g, poly->b);

	DrawClipFan(scr, count, CLIP_UV | CLIP_GLIDE);
}

// [0x43bb8b] [D]
void DrawClippedPOLYGT3(int trans)
{
	POLYGT3 *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	VERTEX scr[5];
	int count;

	poly = (POLYGT3 *)polyblock;
	polyblock += sizeof(POLYGT3);

	v0 = GetVertex(poly->v0, VertexRotate);
	v1 = GetVertex(poly->v1, VertexRotate);
	v2 = GetVertex(poly->v2, VertexRotate);

	num_clip_poly_points = 0;
	clip_ptr = clip_poly_info;

	ClipTestScaleRGBUV(v1, v2, poly->r1, poly->g1, poly->b1, poly->s1, poly->t1, poly->r2, poly->g2, poly->b2, poly->s2, poly->t2);
	ClipTestScaleRGBUV(v2, v0, poly->r2, poly->g2, poly->b2, poly->s2, poly->t2, poly->r0, poly->g0, poly->b0, poly->s0, poly->t0);
	ClipTestScaleRGBUV(v0, v1, poly->r0, poly->g0, poly->b0, poly->s0, poly->t0, poly->r1, poly->g1, poly->b1, poly->s1, poly->t1);

	count = ProjectClipPoly(scr, CLIP_GLIDE);

	if (count == 0)
		return;

	SetTexturedColorCombine();
	SetTextureSet(poly->texture_set);
	SetTransparency(trans);

	DrawClipFan(scr, count, CLIP_RGB | CLIP_UV | CLIP_RGB_BUG | CLIP_GLIDE);
}

// [0x43c741] [D]
void DrawClippedPOLYGT4(int trans)
{
	POLYGT4 *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	VERTEX *v3;
	VERTEX scr[5];
	int count;

	poly = (POLYGT4 *)polyblock;
	polyblock += sizeof(POLYGT4);

	v0 = GetVertex(poly->v0, VertexRotate);
	v1 = GetVertex(poly->v1, VertexRotate);
	v2 = GetVertex(poly->v2, VertexRotate);
	v3 = GetVertex(poly->v3, VertexRotate);

	num_clip_poly_points = 0;
	clip_ptr = clip_poly_info;

	ClipTestScaleRGBUV(v3, v0, poly->r3, poly->g3, poly->b3, poly->s3, poly->t3, poly->r0, poly->g0, poly->b0, poly->s0, poly->t0);
	ClipTestScaleRGBUV(v0, v1, poly->r0, poly->g0, poly->b0, poly->s0, poly->t0, poly->r1, poly->g1, poly->b1, poly->s1, poly->t1);
	ClipTestScaleRGBUV(v1, v2, poly->r1, poly->g1, poly->b1, poly->s1, poly->t1, poly->r2, poly->g2, poly->b2, poly->s2, poly->t2);
	ClipTestScaleRGBUV(v2, v3, poly->r2, poly->g2, poly->b2, poly->s2, poly->t2, poly->r3, poly->g3, poly->b3, poly->s3, poly->t3);

	count = ProjectClipPoly(scr, CLIP_QUAD | CLIP_TEST5 | CLIP_GLIDE);

	if (count == 0)
		return;

	SetTexturedColorCombine();
	SetTextureSet(poly->texture_set);
	SetTransparency(trans);

	DrawClipFan(scr, count, CLIP_RGB | CLIP_UV | CLIP_GLIDE);
}

// [0x43da20] [D]
void DrawClippedPOLYF3_LIT(int trans)
{
	POLYF3LIT *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	VERTEX scr[5];
	float li;
	int count;

	poly = (POLYF3LIT *)polyblock;
	polyblock += sizeof(POLYF3LIT);

	v0 = GetVertex(poly->v0, VertexRotate);
	v1 = GetVertex(poly->v1, VertexRotate);
	v2 = GetVertex(poly->v2, VertexRotate);

	num_clip_poly_points = 0;
	clip_ptr = clip_poly_info;

	ClipTestScale(v0, v1);
	ClipTestScale(v1, v2);
	ClipTestScale(v2, v0);

	count = ProjectClipPoly(scr, CLIP_GLIDE);

	if (count == 0)
		return;

	SetFlatColorCombine();
	SetTransparency(trans);

	li = light_intensity[poly->normal];

	SetColor(&VertexA, li * poly->r, li * poly->g, li * poly->b);
	SetColor(&VertexB, li * poly->r, li * poly->g, li * poly->b);
	SetColor(&VertexC, li * poly->r, li * poly->g, li * poly->b);

	DrawClipFan(scr, count, CLIP_GLIDE);
}

// [0x43e34b] [D]
void DrawClippedPOLYF4_LIT(int trans)
{
	POLYF4LIT *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	VERTEX *v3;
	VERTEX scr[5];
	float li;
	int count;

	poly = (POLYF4LIT *)polyblock;
	polyblock += sizeof(POLYF4LIT);

	v0 = GetVertex(poly->v0, VertexRotate);
	v1 = GetVertex(poly->v1, VertexRotate);
	v2 = GetVertex(poly->v2, VertexRotate);
	v3 = GetVertex(poly->v3, VertexRotate);

	num_clip_poly_points = 0;
	clip_ptr = clip_poly_info;

	ClipTestScale(v0, v1);
	ClipTestScale(v1, v2);
	ClipTestScale(v2, v3);
	ClipTestScale(v3, v0);

	count = ProjectClipPoly(scr, CLIP_QUAD | CLIP_GLIDE);

	if (count == 0)
		return;

	SetFlatColorCombine();
	SetTransparency(trans);

	li = light_intensity[poly->normal];

	SetColor(&VertexA, li * poly->r, li * poly->g, li * poly->b);
	SetColor(&VertexB, li * poly->r, li * poly->g, li * poly->b);
	SetColor(&VertexC, li * poly->r, li * poly->g, li * poly->b);

	DrawClipFan(scr, count, CLIP_GLIDE);
}

// [0x43f1a5] [D]
void DrawClippedPOLYG3_LIT(int trans)
{
	POLYG3LIT *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	VERTEX scr[5];
	int count;
	int i;

	poly = (POLYG3LIT *)polyblock;
	polyblock += sizeof(POLYG3LIT);

	v0 = GetVertex(poly->v0, VertexRotate);
	v1 = GetVertex(poly->v1, VertexRotate);
	v2 = GetVertex(poly->v2, VertexRotate);

	num_clip_poly_points = 0;
	clip_ptr = clip_poly_info;

	ClipTestScaleIntensity(v1, v2, conv_point_normals[poly->pn1], conv_point_normals[poly->pn2]);
	ClipTestScaleIntensity(v2, v0, conv_point_normals[poly->pn2], conv_point_normals[poly->pn0]);
	ClipTestScaleIntensity(v0, v1, conv_point_normals[poly->pn0], conv_point_normals[poly->pn1]);

	count = ProjectClipPoly(scr, CLIP_GLIDE);

	if (count == 0)
		return;

	SetFlatColorCombine();
	SetTransparency(trans);

	// NOTE: the original computes each draw vertex's colour from the point's intensity
	// directly; the colours go through clip_poly_info here
	for (i = 0; i < count; i++)
	{
		clip_poly_info[i].r = poly->r * clip_poly_info[i].intensity;
		clip_poly_info[i].g = poly->g * clip_poly_info[i].intensity;
		clip_poly_info[i].b = poly->b * clip_poly_info[i].intensity;
	}

	DrawClipFan(scr, count, CLIP_RGB | CLIP_GLIDE);
}

// [0x43fb77] [D]
void DrawClippedPOLYG4_LIT(int trans)
{
	POLYG4LIT *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	VERTEX *v3;
	VERTEX scr[5];
	int count;
	int i;

	poly = (POLYG4LIT *)polyblock;
	polyblock += sizeof(POLYG4LIT);

	v0 = GetVertex(poly->v0, VertexRotate);
	v1 = GetVertex(poly->v1, VertexRotate);
	v2 = GetVertex(poly->v2, VertexRotate);
	v3 = GetVertex(poly->v3, VertexRotate);

	num_clip_poly_points = 0;
	clip_ptr = clip_poly_info;

	ClipTestScaleIntensity(v3, v0, conv_point_normals[poly->pn3], conv_point_normals[poly->pn0]);
	ClipTestScaleIntensity(v0, v1, conv_point_normals[poly->pn0], conv_point_normals[poly->pn1]);
	ClipTestScaleIntensity(v1, v2, conv_point_normals[poly->pn1], conv_point_normals[poly->pn2]);
	ClipTestScaleIntensity(v2, v3, conv_point_normals[poly->pn2], conv_point_normals[poly->pn3]);

	count = ProjectClipPoly(scr, CLIP_QUAD | CLIP_GLIDE);

	if (count == 0)
		return;

	SetFlatColorCombine();
	SetTransparency(trans);

	// NOTE: as in DrawClippedPOLYG3_LIT
	for (i = 0; i < count; i++)
	{
		clip_poly_info[i].r = poly->r * clip_poly_info[i].intensity;
		clip_poly_info[i].g = poly->g * clip_poly_info[i].intensity;
		clip_poly_info[i].b = poly->b * clip_poly_info[i].intensity;
	}

	DrawClipFan(scr, count, CLIP_RGB | CLIP_GLIDE);
}

// [0x440af5] [D]
// With lamps the colour of each vertex goes through the clipping, else all are the same.
void DrawClippedPOLYFT3_LIT(int trans)
{
	POLYFT3LIT *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	VERTEX scr[5];
	float li;
	float r;
	float g;
	float b;
	int count;

	poly = (POLYFT3LIT *)polyblock;
	polyblock += sizeof(POLYFT3LIT);

	v0 = GetVertex(poly->v0, VertexRotate);
	v1 = GetVertex(poly->v1, VertexRotate);
	v2 = GetVertex(poly->v2, VertexRotate);

	num_clip_poly_points = 0;
	clip_ptr = clip_poly_info;

	li = light_intensity[poly->normal];

	r = li * poly->r;
	g = li * poly->g;
	b = li * poly->b;

	if (num_potential_lamps)
	{
		AddLampLight(&VertexA, v0->vx, v0->vy, v0->vz, r, g, b);
		AddLampLight(&VertexB, v1->vx, v1->vy, v1->vz, r, g, b);
		AddLampLight(&VertexC, v2->vx, v2->vy, v2->vz, r, g, b);

		ClipTestScaleRGBUV(v0, v1, VertexA.r, VertexA.g, VertexA.b, poly->s0, poly->t0, VertexB.r, VertexB.g, VertexB.b, poly->s1, poly->t1);
		ClipTestScaleRGBUV(v1, v2, VertexB.r, VertexB.g, VertexB.b, poly->s1, poly->t1, VertexC.r, VertexC.g, VertexC.b, poly->s2, poly->t2);
		ClipTestScaleRGBUV(v2, v0, VertexC.r, VertexC.g, VertexC.b, poly->s2, poly->t2, VertexA.r, VertexA.g, VertexA.b, poly->s0, poly->t0);
	}
	else
	{
		SetColor(&VertexA, r, g, b);
		SetColor(&VertexB, r, g, b);
		SetColor(&VertexC, r, g, b);

		ClipTestScaleUV(v0, v1, poly->s0, poly->t0, poly->s1, poly->t1);
		ClipTestScaleUV(v1, v2, poly->s1, poly->t1, poly->s2, poly->t2);
		ClipTestScaleUV(v2, v0, poly->s2, poly->t2, poly->s0, poly->t0);
	}

	count = ProjectClipPoly(scr, CLIP_GLIDE);

	if (count == 0)
		return;

	SetTexturedColorCombine();
	SetTextureSet(poly->texture_set);
	SetTransparency(trans);

	DrawClipFan(scr, count, (num_potential_lamps ? CLIP_RGB | CLIP_UV : CLIP_UV) | CLIP_GLIDE);
}

// [0x441d25] [D]
void DrawClippedPOLYFT4_LIT(int trans)
{
	POLYFT4LIT *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	VERTEX *v3;
	VERTEX scr[5];
	GrVertex light3;
	float li;
	float r;
	float g;
	float b;
	int count;

	poly = (POLYFT4LIT *)polyblock;
	polyblock += sizeof(POLYFT4LIT);

	v0 = GetVertex(poly->v0, VertexRotate);
	v1 = GetVertex(poly->v1, VertexRotate);
	v2 = GetVertex(poly->v2, VertexRotate);
	v3 = GetVertex(poly->v3, VertexRotate);

	num_clip_poly_points = 0;
	clip_ptr = clip_poly_info;

	li = light_intensity[poly->normal];

	r = li * poly->r;
	g = li * poly->g;
	b = li * poly->b;

	if (num_potential_lamps)
	{
		// NOTE: the original keeps v3's colour in unnamed globals (0xb9b59c ...)
		AddLampLight(&VertexA, v0->vx, v0->vy, v0->vz, r, g, b);
		AddLampLight(&VertexB, v1->vx, v1->vy, v1->vz, r, g, b);
		AddLampLight(&VertexC, v2->vx, v2->vy, v2->vz, r, g, b);
		AddLampLight(&light3, v3->vx, v3->vy, v3->vz, r, g, b);

		ClipTestScaleRGBUV(v0, v1, VertexA.r, VertexA.g, VertexA.b, poly->s0, poly->t0, VertexB.r, VertexB.g, VertexB.b, poly->s1, poly->t1);
		ClipTestScaleRGBUV(v1, v2, VertexB.r, VertexB.g, VertexB.b, poly->s1, poly->t1, VertexC.r, VertexC.g, VertexC.b, poly->s2, poly->t2);
		ClipTestScaleRGBUV(v2, v3, VertexC.r, VertexC.g, VertexC.b, poly->s2, poly->t2, light3.r, light3.g, light3.b, poly->s3, poly->t3);
		ClipTestScaleRGBUV(v3, v0, light3.r, light3.g, light3.b, poly->s3, poly->t3, VertexA.r, VertexA.g, VertexA.b, poly->s0, poly->t0);
	}
	else
	{
		SetColor(&VertexA, r, g, b);
		SetColor(&VertexB, r, g, b);
		SetColor(&VertexC, r, g, b);

		ClipTestScaleUV(v0, v1, poly->s0, poly->t0, poly->s1, poly->t1);
		ClipTestScaleUV(v1, v2, poly->s1, poly->t1, poly->s2, poly->t2);
		ClipTestScaleUV(v2, v3, poly->s2, poly->t2, poly->s3, poly->t3);
		ClipTestScaleUV(v3, v0, poly->s3, poly->t3, poly->s0, poly->t0);
	}

	count = ProjectClipPoly(scr, CLIP_QUAD | CLIP_GLIDE);

	if (count == 0)
		return;

	SetTexturedColorCombine();
	SetTextureSet(poly->texture_set);
	SetTransparency(trans);

	DrawClipFan(scr, count, (num_potential_lamps ? CLIP_RGB | CLIP_UV : CLIP_UV) | CLIP_GLIDE);
}

// [0x44382b] [D]
// The colour of each vertex goes through the clipping, and with gShinyType the shine texture
// u, v too (as field_28, field_2c).
void DrawClippedPOLYGT3_LIT(int trans)
{
	POLYGT3LIT *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	VERTEX scr[5];
	float li;
	int count;

	poly = (POLYGT3LIT *)polyblock;
	polyblock += sizeof(POLYGT3LIT);

	v0 = GetVertex(poly->v0, VertexRotate);
	v1 = GetVertex(poly->v1, VertexRotate);
	v2 = GetVertex(poly->v2, VertexRotate);

	num_clip_poly_points = 0;
	clip_ptr = clip_poly_info;

	if (num_potential_lamps)
	{
		li = conv_point_normals[poly->pn0];
		AddLampLight(&VertexA, v0->vx, v0->vy, v0->vz, li * poly->r, li * poly->g, li * poly->b);

		li = conv_point_normals[poly->pn1];
		AddLampLight(&VertexB, v1->vx, v1->vy, v1->vz, li * poly->r, li * poly->g, li * poly->b);

		li = conv_point_normals[poly->pn2];
		AddLampLight(&VertexC, v2->vx, v2->vy, v2->vz, li * poly->r, li * poly->g, li * poly->b);
	}
	else
	{
		li = conv_point_normals[poly->pn0];
		SetColor(&VertexA, poly->r * li, poly->g * li, poly->b * li);

		li = conv_point_normals[poly->pn1];
		SetColor(&VertexB, poly->r * li, poly->g * li, poly->b * li);

		li = conv_point_normals[poly->pn2];
		SetColor(&VertexC, poly->r * li, poly->g * li, poly->b * li);
	}

	if (gShinyType)
	{
		ClipTestScaleRGBUVWHIntensity(v1, v2, VertexB.r, VertexB.g, VertexB.b, poly->s1, poly->t1, SHINE_U(poly->pn1), SHINE_V(poly->pn1),
			VertexC.r, VertexC.g, VertexC.b, poly->s2, poly->t2, SHINE_U(poly->pn2), SHINE_V(poly->pn2));
		ClipTestScaleRGBUVWHIntensity(v2, v0, VertexC.r, VertexC.g, VertexC.b, poly->s2, poly->t2, SHINE_U(poly->pn2), SHINE_V(poly->pn2),
			VertexA.r, VertexA.g, VertexA.b, poly->s0, poly->t0, SHINE_U(poly->pn0), SHINE_V(poly->pn0));
		ClipTestScaleRGBUVWHIntensity(v0, v1, VertexA.r, VertexA.g, VertexA.b, poly->s0, poly->t0, SHINE_U(poly->pn0), SHINE_V(poly->pn0),
			VertexB.r, VertexB.g, VertexB.b, poly->s1, poly->t1, SHINE_U(poly->pn1), SHINE_V(poly->pn1));
	}
	else
	{
		ClipTestScaleRGBUV(v1, v2, VertexB.r, VertexB.g, VertexB.b, poly->s1, poly->t1, VertexC.r, VertexC.g, VertexC.b, poly->s2, poly->t2);
		ClipTestScaleRGBUV(v2, v0, VertexC.r, VertexC.g, VertexC.b, poly->s2, poly->t2, VertexA.r, VertexA.g, VertexA.b, poly->s0, poly->t0);
		ClipTestScaleRGBUV(v0, v1, VertexA.r, VertexA.g, VertexA.b, poly->s0, poly->t0, VertexB.r, VertexB.g, VertexB.b, poly->s1, poly->t1);
	}

	count = ProjectClipPoly(scr, CLIP_GLIDE);

	if (count == 0)
		return;

	SetTexturedColorCombine();
	SetTextureSet(poly->texture_set);
	SetTransparency(trans);

	SetClipVertex(&VertexA, scr, 0, CLIP_RGB | CLIP_UV | CLIP_GLIDE);

	if (count == 3)
	{
		SetClipVertex(&VertexB, scr, 1, CLIP_RGB | CLIP_UV | CLIP_GLIDE);
		SetClipVertex(&VertexC, scr, 2, CLIP_RGB | CLIP_UV | CLIP_GLIDE);

		if (gShinyType != 2)
			guDrawTriangleWithClip(&VertexA, &VertexB, &VertexC);

		if (gShinyType)
		{
			SetClipShine(&shine_vertex_a, &VertexA, scr, 0);
			SetClipShine(&shine_vertex_b, &VertexB, scr, 1);
			SetClipShine(&shine_vertex_c, &VertexC, scr, 2);

			AddCarShine(&shine_vertex_a, &shine_vertex_b, &shine_vertex_c, 1);
		}

		return;
	}

	SetClipVertex(&VertexB, scr, 2, CLIP_RGB | CLIP_UV | CLIP_GLIDE);
	SetClipVertex(&VertexC, scr, 1, CLIP_RGB | CLIP_UV | CLIP_GLIDE);

	if (gShinyType != 2)
		guDrawTriangleWithClip(&VertexA, &VertexB, &VertexC);

	if (gShinyType)
	{
		SetClipShine(&shine_vertex_a, &VertexA, scr, 0);
		SetClipShine(&shine_vertex_b, &VertexB, scr, 2);
		SetClipShine(&shine_vertex_c, &VertexC, scr, 1);

		AddCarShine(&shine_vertex_a, &shine_vertex_b, &shine_vertex_c, 1);
	}

	SetClipVertex(&VertexC, scr, 3, CLIP_RGB | CLIP_UV | CLIP_GLIDE);

	if (gShinyType != 2)
		guDrawTriangleWithClip(&VertexA, &VertexB, &VertexC);

	if (gShinyType)
	{
		SetClipShine(&shine_vertex_c, &VertexC, scr, 3);
		AddCarShine(&shine_vertex_a, &shine_vertex_b, &shine_vertex_c, 1);
	}
}

// [0x445030] [D]
void DrawClippedPOLYGT4_LIT(int trans)
{
	POLYGT4LIT *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	VERTEX *v3;
	VERTEX scr[5];
	GrVertex light3;
	float li;
	int count;

	poly = (POLYGT4LIT *)polyblock;
	polyblock += sizeof(POLYGT4LIT);

	v0 = GetVertex(poly->v0, VertexRotate);
	v1 = GetVertex(poly->v1, VertexRotate);
	v2 = GetVertex(poly->v2, VertexRotate);
	v3 = GetVertex(poly->v3, VertexRotate);

	num_clip_poly_points = 0;
	clip_ptr = clip_poly_info;

	// NOTE: the original keeps v3's colour in unnamed globals (0xb9b59c ...)
	if (num_potential_lamps)
	{
		li = conv_point_normals[poly->pn0];
		AddLampLight(&VertexA, v0->vx, v0->vy, v0->vz, li * poly->r, li * poly->g, li * poly->b);

		li = conv_point_normals[poly->pn1];
		AddLampLight(&VertexB, v1->vx, v1->vy, v1->vz, li * poly->r, li * poly->g, li * poly->b);

		li = conv_point_normals[poly->pn2];
		AddLampLight(&VertexC, v2->vx, v2->vy, v2->vz, li * poly->r, li * poly->g, li * poly->b);

		li = conv_point_normals[poly->pn3];
		AddLampLight(&light3, v3->vx, v3->vy, v3->vz, li * poly->r, li * poly->g, li * poly->b);
	}
	else
	{
		li = conv_point_normals[poly->pn0];
		SetColor(&VertexA, poly->r * li, poly->g * li, poly->b * li);

		li = conv_point_normals[poly->pn1];
		SetColor(&VertexB, poly->r * li, poly->g * li, poly->b * li);

		li = conv_point_normals[poly->pn2];
		SetColor(&VertexC, poly->r * li, poly->g * li, poly->b * li);

		li = conv_point_normals[poly->pn3];
		SetColor(&light3, poly->r * li, poly->g * li, poly->b * li);
	}

	if (gShinyType)
	{
		ClipTestScaleRGBUVWHIntensity(v3, v0, light3.r, light3.g, light3.b, poly->s3, poly->t3, SHINE_U(poly->pn3), SHINE_V(poly->pn3),
			VertexA.r, VertexA.g, VertexA.b, poly->s0, poly->t0, SHINE_U(poly->pn0), SHINE_V(poly->pn0));
		ClipTestScaleRGBUVWHIntensity(v0, v1, VertexA.r, VertexA.g, VertexA.b, poly->s0, poly->t0, SHINE_U(poly->pn0), SHINE_V(poly->pn0),
			VertexB.r, VertexB.g, VertexB.b, poly->s1, poly->t1, SHINE_U(poly->pn1), SHINE_V(poly->pn1));
		ClipTestScaleRGBUVWHIntensity(v1, v2, VertexB.r, VertexB.g, VertexB.b, poly->s1, poly->t1, SHINE_U(poly->pn1), SHINE_V(poly->pn1),
			VertexC.r, VertexC.g, VertexC.b, poly->s2, poly->t2, SHINE_U(poly->pn2), SHINE_V(poly->pn2));
		ClipTestScaleRGBUVWHIntensity(v2, v3, VertexC.r, VertexC.g, VertexC.b, poly->s2, poly->t2, SHINE_U(poly->pn2), SHINE_V(poly->pn2),
			light3.r, light3.g, light3.b, poly->s3, poly->t3, SHINE_U(poly->pn3), SHINE_V(poly->pn3));
	}
	else
	{
		ClipTestScaleRGBUV(v3, v0, light3.r, light3.g, light3.b, poly->s3, poly->t3, VertexA.r, VertexA.g, VertexA.b, poly->s0, poly->t0);
		ClipTestScaleRGBUV(v0, v1, VertexA.r, VertexA.g, VertexA.b, poly->s0, poly->t0, VertexB.r, VertexB.g, VertexB.b, poly->s1, poly->t1);
		ClipTestScaleRGBUV(v1, v2, VertexB.r, VertexB.g, VertexB.b, poly->s1, poly->t1, VertexC.r, VertexC.g, VertexC.b, poly->s2, poly->t2);
		ClipTestScaleRGBUV(v2, v3, VertexC.r, VertexC.g, VertexC.b, poly->s2, poly->t2, light3.r, light3.g, light3.b, poly->s3, poly->t3);
	}

	count = ProjectClipPoly(scr, CLIP_QUAD | CLIP_GLIDE);

	if (count == 0)
		return;

	SetTexturedColorCombine();
	SetTextureSet(poly->texture_set);
	SetTransparency(trans);

	SetClipVertex(&VertexA, scr, 0, CLIP_RGB | CLIP_UV | CLIP_GLIDE);

	if (count == 3)
	{
		SetClipVertex(&VertexB, scr, 1, CLIP_RGB | CLIP_UV | CLIP_GLIDE);
		SetClipVertex(&VertexC, scr, 2, CLIP_RGB | CLIP_UV | CLIP_GLIDE);

		if (gShinyType != 2)
			guDrawTriangleWithClip(&VertexA, &VertexB, &VertexC);

		if (gShinyType)
		{
			SetClipShine(&shine_vertex_a, &VertexA, scr, 0);
			SetClipShine(&shine_vertex_b, &VertexB, scr, 1);
			SetClipShine(&shine_vertex_c, &VertexC, scr, 2);

			AddCarShine(&shine_vertex_a, &shine_vertex_b, &shine_vertex_c, 1);
		}

		return;
	}

	SetClipVertex(&VertexB, scr, 2, CLIP_RGB | CLIP_UV | CLIP_GLIDE);
	SetClipVertex(&VertexC, scr, 1, CLIP_RGB | CLIP_UV | CLIP_GLIDE);

	if (gShinyType != 2)
		guDrawTriangleWithClip(&VertexA, &VertexB, &VertexC);

	if (gShinyType)
	{
		SetClipShine(&shine_vertex_a, &VertexA, scr, 0);
		SetClipShine(&shine_vertex_b, &VertexB, scr, 2);
		SetClipShine(&shine_vertex_c, &VertexC, scr, 1);

		AddCarShine(&shine_vertex_a, &shine_vertex_b, &shine_vertex_c, 1);
	}

	SetClipVertex(&VertexC, scr, 3, CLIP_RGB | CLIP_UV | CLIP_GLIDE);

	if (gShinyType != 2)
		guDrawTriangleWithClip(&VertexA, &VertexB, &VertexC);

	if (gShinyType)
	{
		SetClipShine(&shine_vertex_c, &VertexC, scr, 3);
		AddCarShine(&shine_vertex_a, &shine_vertex_b, &shine_vertex_c, 1);
	}

	if (count == 5)
	{
		SetClipVertex(&VertexB, scr, 4, CLIP_RGB | CLIP_UV | CLIP_GLIDE);

		if (gShinyType != 2)
			guDrawTriangleWithClip(&VertexA, &VertexB, &VertexC);

		if (gShinyType)
		{
			SetClipShine(&shine_vertex_b, &VertexB, scr, 4);
			AddCarShine(&shine_vertex_a, &shine_vertex_b, &shine_vertex_c, 1);
		}
	}
}

// [0x4473fa] [D]
// The far clipped polygons (the object crosses far_clip), as the near clipped ones.
void DrawFarClippedPOLYF3(int trans)
{
	POLYF3 *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	VERTEX scr[5];
	int count;

	poly = (POLYF3 *)polyblock;
	polyblock += sizeof(POLYF3);

	v0 = GetVertex(poly->v0, VertexRotate);
	v1 = GetVertex(poly->v1, VertexRotate);
	v2 = GetVertex(poly->v2, VertexRotate);

	num_clip_poly_points = 0;
	clip_ptr = clip_poly_info;

	FarClipTestScale(v0, v1);
	FarClipTestScale(v1, v2);
	FarClipTestScale(v2, v0);

	count = ProjectClipPoly(scr, CLIP_GLIDE);

	if (count == 0)
		return;

	SetFlatColorCombine();
	SetTransparency(trans);

	SetColor(&VertexA, poly->r, poly->g, poly->b);
	SetColor(&VertexB, poly->r, poly->g, poly->b);
	SetColor(&VertexC, poly->r, poly->g, poly->b);

	DrawClipFan(scr, count, CLIP_GLIDE);
}

// [0x447cc3] [D]
void DrawFarClippedPOLYF4(int trans)
{
	POLYF4 *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	VERTEX *v3;
	VERTEX scr[5];
	int count;

	poly = (POLYF4 *)polyblock;
	polyblock += sizeof(POLYF4);

	v0 = GetVertex(poly->v0, VertexRotate);
	v1 = GetVertex(poly->v1, VertexRotate);
	v2 = GetVertex(poly->v2, VertexRotate);
	v3 = GetVertex(poly->v3, VertexRotate);

	num_clip_poly_points = 0;
	clip_ptr = clip_poly_info;

	FarClipTestScale(v0, v1);
	FarClipTestScale(v1, v2);
	FarClipTestScale(v2, v3);
	FarClipTestScale(v3, v0);

	count = ProjectClipPoly(scr, CLIP_QUAD | CLIP_TEST5 | CLIP_GLIDE);

	if (count == 0)
		return;

	SetFlatColorCombine();
	SetTransparency(trans);

	SetColor(&VertexA, poly->r, poly->g, poly->b);
	SetColor(&VertexB, poly->r, poly->g, poly->b);
	SetColor(&VertexC, poly->r, poly->g, poly->b);

	DrawClipFan(scr, count, CLIP_GLIDE);
}

// [0x448a6e] [D]
void DrawFarClippedPOLYF3_LIT(int trans)
{
	POLYF3LIT *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	VERTEX scr[5];
	float li;
	int count;

	poly = (POLYF3LIT *)polyblock;
	polyblock += sizeof(POLYF3LIT);

	v0 = GetVertex(poly->v0, VertexRotate);
	v1 = GetVertex(poly->v1, VertexRotate);
	v2 = GetVertex(poly->v2, VertexRotate);

	num_clip_poly_points = 0;
	clip_ptr = clip_poly_info;

	FarClipTestScale(v0, v1);
	FarClipTestScale(v1, v2);
	FarClipTestScale(v2, v0);

	count = ProjectClipPoly(scr, CLIP_GLIDE);

	if (count == 0)
		return;

	SetFlatColorCombine();
	SetTransparency(trans);

	li = light_intensity[poly->normal];

	SetColor(&VertexA, li * poly->r, li * poly->g, li * poly->b);
	SetColor(&VertexB, li * poly->r, li * poly->g, li * poly->b);
	SetColor(&VertexC, li * poly->r, li * poly->g, li * poly->b);

	DrawClipFan(scr, count, CLIP_GLIDE);
}

// [0x449399] [D]
void DrawFarClippedPOLYF4_LIT(int trans)
{
	POLYF4LIT *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	VERTEX *v3;
	VERTEX scr[5];
	float li;
	int count;

	poly = (POLYF4LIT *)polyblock;
	polyblock += sizeof(POLYF4LIT);

	v0 = GetVertex(poly->v0, VertexRotate);
	v1 = GetVertex(poly->v1, VertexRotate);
	v2 = GetVertex(poly->v2, VertexRotate);
	v3 = GetVertex(poly->v3, VertexRotate);

	num_clip_poly_points = 0;
	clip_ptr = clip_poly_info;

	FarClipTestScale(v0, v1);
	FarClipTestScale(v1, v2);
	FarClipTestScale(v2, v3);
	FarClipTestScale(v3, v0);

	count = ProjectClipPoly(scr, CLIP_QUAD | CLIP_GLIDE);

	if (count == 0)
		return;

	SetFlatColorCombine();
	SetTransparency(trans);

	li = light_intensity[poly->normal];

	SetColor(&VertexA, li * poly->r, li * poly->g, li * poly->b);
	SetColor(&VertexB, li * poly->r, li * poly->g, li * poly->b);
	SetColor(&VertexC, li * poly->r, li * poly->g, li * poly->b);

	DrawClipFan(scr, count, CLIP_GLIDE);
}

// [0x44a1f3] [D]
void DrawFarClippedPOLYFT3(int trans)
{
	POLYFT3 *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	VERTEX scr[5];
	int count;

	poly = (POLYFT3 *)polyblock;
	polyblock += sizeof(POLYFT3);

	v0 = GetVertex(poly->v0, VertexRotate);
	v1 = GetVertex(poly->v1, VertexRotate);
	v2 = GetVertex(poly->v2, VertexRotate);

	num_clip_poly_points = 0;
	clip_ptr = clip_poly_info;

	FarClipTestScaleUV(v0, v1, poly->s0, poly->t0, poly->s1, poly->t1);
	FarClipTestScaleUV(v1, v2, poly->s1, poly->t1, poly->s2, poly->t2);
	FarClipTestScaleUV(v2, v0, poly->s2, poly->t2, poly->s0, poly->t0);

	count = ProjectClipPoly(scr, CLIP_GLIDE);

	if (count == 0)
		return;

	SetTexturedColorCombine();
	SetTextureSet(poly->texture_set);
	SetTransparency(trans);

	SetColor(&VertexA, poly->r, poly->g, poly->b);
	SetColor(&VertexB, poly->r, poly->g, poly->b);
	SetColor(&VertexC, poly->r, poly->g, poly->b);

	DrawClipFan(scr, count, CLIP_UV | CLIP_GLIDE);
}

// [0x44ac6f] [D]
void DrawFarClippedPOLYFT4(int trans)
{
	POLYFT4 *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	VERTEX *v3;
	VERTEX scr[5];
	int count;

	poly = (POLYFT4 *)polyblock;
	polyblock += sizeof(POLYFT4);

	v0 = GetVertex(poly->v0, VertexRotate);
	v1 = GetVertex(poly->v1, VertexRotate);
	v2 = GetVertex(poly->v2, VertexRotate);
	v3 = GetVertex(poly->v3, VertexRotate);

	num_clip_poly_points = 0;
	clip_ptr = clip_poly_info;

	FarClipTestScaleUV(v0, v1, poly->s0, poly->t0, poly->s1, poly->t1);
	FarClipTestScaleUV(v1, v2, poly->s1, poly->t1, poly->s2, poly->t2);
	FarClipTestScaleUV(v2, v3, poly->s2, poly->t2, poly->s3, poly->t3);
	FarClipTestScaleUV(v3, v0, poly->s3, poly->t3, poly->s0, poly->t0);

	count = ProjectClipPoly(scr, CLIP_QUAD | CLIP_GLIDE);

	if (count == 0)
		return;

	SetTexturedColorCombine();
	SetTextureSet(poly->texture_set);
	SetTransparency(trans);

	SetColor(&VertexA, poly->r, poly->g, poly->b);
	SetColor(&VertexB, poly->r, poly->g, poly->b);
	SetColor(&VertexC, poly->r, poly->g, poly->b);

	DrawClipFan(scr, count, CLIP_UV | CLIP_GLIDE);
}

// [0x44bcbd] [D]
void DrawFarClippedPOLYGT3(int trans)
{
	POLYGT3 *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	VERTEX scr[5];
	int count;

	poly = (POLYGT3 *)polyblock;
	polyblock += sizeof(POLYGT3);

	v0 = GetVertex(poly->v0, VertexRotate);
	v1 = GetVertex(poly->v1, VertexRotate);
	v2 = GetVertex(poly->v2, VertexRotate);

	num_clip_poly_points = 0;
	clip_ptr = clip_poly_info;

	FarClipTestScaleRGBUV(v1, v2, poly->r1, poly->g1, poly->b1, poly->s1, poly->t1, poly->r2, poly->g2, poly->b2, poly->s2, poly->t2);
	FarClipTestScaleRGBUV(v2, v0, poly->r2, poly->g2, poly->b2, poly->s2, poly->t2, poly->r0, poly->g0, poly->b0, poly->s0, poly->t0);
	FarClipTestScaleRGBUV(v0, v1, poly->r0, poly->g0, poly->b0, poly->s0, poly->t0, poly->r1, poly->g1, poly->b1, poly->s1, poly->t1);

	count = ProjectClipPoly(scr, CLIP_GLIDE);

	if (count == 0)
		return;

	SetTexturedColorCombine();
	SetTextureSet(poly->texture_set);
	SetTransparency(trans);

	DrawClipFan(scr, count, CLIP_RGB | CLIP_UV | CLIP_RGB_BUG | CLIP_GLIDE);
}

// [0x44c873] [D]
void DrawFarClippedPOLYGT4(int trans)
{
	POLYGT4 *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	VERTEX *v3;
	VERTEX scr[5];
	int count;

	poly = (POLYGT4 *)polyblock;
	polyblock += sizeof(POLYGT4);

	v0 = GetVertex(poly->v0, VertexRotate);
	v1 = GetVertex(poly->v1, VertexRotate);
	v2 = GetVertex(poly->v2, VertexRotate);
	v3 = GetVertex(poly->v3, VertexRotate);

	num_clip_poly_points = 0;
	clip_ptr = clip_poly_info;

	FarClipTestScaleRGBUV(v3, v0, poly->r3, poly->g3, poly->b3, poly->s3, poly->t3, poly->r0, poly->g0, poly->b0, poly->s0, poly->t0);
	FarClipTestScaleRGBUV(v0, v1, poly->r0, poly->g0, poly->b0, poly->s0, poly->t0, poly->r1, poly->g1, poly->b1, poly->s1, poly->t1);
	FarClipTestScaleRGBUV(v1, v2, poly->r1, poly->g1, poly->b1, poly->s1, poly->t1, poly->r2, poly->g2, poly->b2, poly->s2, poly->t2);
	FarClipTestScaleRGBUV(v2, v3, poly->r2, poly->g2, poly->b2, poly->s2, poly->t2, poly->r3, poly->g3, poly->b3, poly->s3, poly->t3);

	count = ProjectClipPoly(scr, CLIP_QUAD | CLIP_TEST5 | CLIP_GLIDE);

	if (count == 0)
		return;

	SetTexturedColorCombine();
	SetTextureSet(poly->texture_set);
	SetTransparency(trans);

	DrawClipFan(scr, count, CLIP_RGB | CLIP_UV | CLIP_GLIDE);
}

// [0x44db52] [D]
void DrawFarClippedPOLYFT3_LIT(int trans)
{
	POLYFT3LIT *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	VERTEX scr[5];
	float li;
	int count;

	poly = (POLYFT3LIT *)polyblock;
	polyblock += sizeof(POLYFT3LIT);

	v0 = GetVertex(poly->v0, VertexRotate);
	v1 = GetVertex(poly->v1, VertexRotate);
	v2 = GetVertex(poly->v2, VertexRotate);

	num_clip_poly_points = 0;
	clip_ptr = clip_poly_info;

	FarClipTestScaleUV(v0, v1, poly->s0, poly->t0, poly->s1, poly->t1);
	FarClipTestScaleUV(v1, v2, poly->s1, poly->t1, poly->s2, poly->t2);
	FarClipTestScaleUV(v2, v0, poly->s2, poly->t2, poly->s0, poly->t0);

	count = ProjectClipPoly(scr, CLIP_GLIDE);

	if (count == 0)
		return;

	SetTexturedColorCombine();
	SetTextureSet(poly->texture_set);
	SetTransparency(trans);

	li = light_intensity[poly->normal];

	SetColor(&VertexA, li * poly->r, li * poly->g, li * poly->b);
	SetColor(&VertexB, li * poly->r, li * poly->g, li * poly->b);
	SetColor(&VertexC, li * poly->r, li * poly->g, li * poly->b);

	DrawClipFan(scr, count, CLIP_UV | CLIP_GLIDE);
}

// [0x44e630] [D]
void DrawFarClippedPOLYFT4_LIT(int trans)
{
	POLYFT4LIT *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	VERTEX *v3;
	VERTEX scr[5];
	float li;
	int count;

	poly = (POLYFT4LIT *)polyblock;
	polyblock += sizeof(POLYFT4LIT);

	v0 = GetVertex(poly->v0, VertexRotate);
	v1 = GetVertex(poly->v1, VertexRotate);
	v2 = GetVertex(poly->v2, VertexRotate);
	v3 = GetVertex(poly->v3, VertexRotate);

	num_clip_poly_points = 0;
	clip_ptr = clip_poly_info;

	FarClipTestScaleUV(v0, v1, poly->s0, poly->t0, poly->s1, poly->t1);
	FarClipTestScaleUV(v1, v2, poly->s1, poly->t1, poly->s2, poly->t2);
	FarClipTestScaleUV(v2, v3, poly->s2, poly->t2, poly->s3, poly->t3);
	FarClipTestScaleUV(v3, v0, poly->s3, poly->t3, poly->s0, poly->t0);

	count = ProjectClipPoly(scr, CLIP_QUAD | CLIP_GLIDE);

	if (count == 0)
		return;

	SetTexturedColorCombine();
	SetTextureSet(poly->texture_set);
	SetTransparency(trans);

	li = light_intensity[poly->normal];

	SetColor(&VertexA, li * poly->r, li * poly->g, li * poly->b);
	SetColor(&VertexB, li * poly->r, li * poly->g, li * poly->b);
	SetColor(&VertexC, li * poly->r, li * poly->g, li * poly->b);

	DrawClipFan(scr, count, CLIP_UV | CLIP_GLIDE);
}

// [0x44f741] [D]
void DrawFarClippedPOLYGT3_LIT(int trans)
{
	POLYGT3LIT *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	VERTEX scr[5];
	float li;
	int count;

	poly = (POLYGT3LIT *)polyblock;
	polyblock += sizeof(POLYGT3LIT);

	v0 = GetVertex(poly->v0, VertexRotate);
	v1 = GetVertex(poly->v1, VertexRotate);
	v2 = GetVertex(poly->v2, VertexRotate);

	num_clip_poly_points = 0;
	clip_ptr = clip_poly_info;

	if (num_potential_lamps)
	{
		li = conv_point_normals[poly->pn0];
		AddLampLight(&VertexA, v0->vx, v0->vy, v0->vz, li * poly->r, li * poly->g, li * poly->b);

		li = conv_point_normals[poly->pn1];
		AddLampLight(&VertexB, v1->vx, v1->vy, v1->vz, li * poly->r, li * poly->g, li * poly->b);

		li = conv_point_normals[poly->pn2];
		AddLampLight(&VertexC, v2->vx, v2->vy, v2->vz, li * poly->r, li * poly->g, li * poly->b);
	}
	else
	{
		li = conv_point_normals[poly->pn0];
		SetColor(&VertexA, poly->r * li, poly->g * li, poly->b * li);

		li = conv_point_normals[poly->pn1];
		SetColor(&VertexB, poly->r * li, poly->g * li, poly->b * li);

		li = conv_point_normals[poly->pn2];
		SetColor(&VertexC, poly->r * li, poly->g * li, poly->b * li);
	}

	FarClipTestScaleRGBUV(v1, v2, VertexB.r, VertexB.g, VertexB.b, poly->s1, poly->t1, VertexC.r, VertexC.g, VertexC.b, poly->s2, poly->t2);
	FarClipTestScaleRGBUV(v2, v0, VertexC.r, VertexC.g, VertexC.b, poly->s2, poly->t2, VertexA.r, VertexA.g, VertexA.b, poly->s0, poly->t0);
	FarClipTestScaleRGBUV(v0, v1, VertexA.r, VertexA.g, VertexA.b, poly->s0, poly->t0, VertexB.r, VertexB.g, VertexB.b, poly->s1, poly->t1);

	count = ProjectClipPoly(scr, CLIP_GLIDE);

	if (count == 0)
		return;

	SetTexturedColorCombine();
	SetTextureSet(poly->texture_set);
	SetTransparency(trans);

	DrawClipFan(scr, count, CLIP_RGB | CLIP_UV | CLIP_GLIDE);
}

// [0x450a78] [D]
void DrawFarClippedPOLYGT4_LIT(int trans)
{
	POLYGT4LIT *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	VERTEX *v3;
	VERTEX scr[5];
	GrVertex light3;
	float li;
	int count;

	poly = (POLYGT4LIT *)polyblock;
	polyblock += sizeof(POLYGT4LIT);

	v0 = GetVertex(poly->v0, VertexRotate);
	v1 = GetVertex(poly->v1, VertexRotate);
	v2 = GetVertex(poly->v2, VertexRotate);
	v3 = GetVertex(poly->v3, VertexRotate);

	num_clip_poly_points = 0;
	clip_ptr = clip_poly_info;

	// NOTE: the original keeps v3's colour in unnamed globals (0xb9b59c ...)
	if (num_potential_lamps)
	{
		li = conv_point_normals[poly->pn0];
		AddLampLight(&VertexA, v0->vx, v0->vy, v0->vz, li * poly->r, li * poly->g, li * poly->b);

		li = conv_point_normals[poly->pn1];
		AddLampLight(&VertexB, v1->vx, v1->vy, v1->vz, li * poly->r, li * poly->g, li * poly->b);

		li = conv_point_normals[poly->pn2];
		AddLampLight(&VertexC, v2->vx, v2->vy, v2->vz, li * poly->r, li * poly->g, li * poly->b);

		li = conv_point_normals[poly->pn3];
		AddLampLight(&light3, v3->vx, v3->vy, v3->vz, li * poly->r, li * poly->g, li * poly->b);
	}
	else
	{
		li = conv_point_normals[poly->pn0];
		SetColor(&VertexA, poly->r * li, poly->g * li, poly->b * li);

		li = conv_point_normals[poly->pn1];
		SetColor(&VertexB, poly->r * li, poly->g * li, poly->b * li);

		li = conv_point_normals[poly->pn2];
		SetColor(&VertexC, poly->r * li, poly->g * li, poly->b * li);

		li = conv_point_normals[poly->pn3];
		SetColor(&light3, poly->r * li, poly->g * li, poly->b * li);
	}

	// NOTE: clipped against the near plane, not the far one (the original's)
	ClipTestScaleRGBUV(v3, v0, light3.r, light3.g, light3.b, poly->s3, poly->t3, VertexA.r, VertexA.g, VertexA.b, poly->s0, poly->t0);
	ClipTestScaleRGBUV(v0, v1, VertexA.r, VertexA.g, VertexA.b, poly->s0, poly->t0, VertexB.r, VertexB.g, VertexB.b, poly->s1, poly->t1);
	ClipTestScaleRGBUV(v1, v2, VertexB.r, VertexB.g, VertexB.b, poly->s1, poly->t1, VertexC.r, VertexC.g, VertexC.b, poly->s2, poly->t2);
	ClipTestScaleRGBUV(v2, v3, VertexC.r, VertexC.g, VertexC.b, poly->s2, poly->t2, light3.r, light3.g, light3.b, poly->s3, poly->t3);

	count = ProjectClipPoly(scr, CLIP_QUAD | CLIP_GLIDE);

	if (count == 0)
		return;

	SetTexturedColorCombine();
	SetTextureSet(poly->texture_set);
	SetTransparency(trans);

	DrawClipFan(scr, count, CLIP_RGB | CLIP_UV | CLIP_GLIDE);
}

// [0x452897] [D]
void d3dDrawPolyF3(int trans)
{
	POLYF3 *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;

	poly = (POLYF3 *)polyblock;
	polyblock += sizeof(POLYF3);

	v0 = GetVertex(poly->v0, VertexRotatePers_D3D);
	v1 = GetVertex(poly->v1, VertexRotatePers_D3D);
	v2 = GetVertex(poly->v2, VertexRotatePers_D3D);

	if (do_clipping && PolyOffScreen3(v0, v1, v2))
		return;

	SetColor(&VertexA, poly->r, poly->g, poly->b);
	SetColor(&VertexB, poly->r, poly->g, poly->b);
	SetColor(&VertexC, poly->r, poly->g, poly->b);

	SetFlatColorCombine();
	SetTransparency(trans);

	SetPosition(&VertexA, v0);
	SetPosition(&VertexB, v1);
	SetPosition(&VertexC, v2);

	DrawTriangleABC();
}

// [0x452d1c] [D]
// Quads are drawn as the triangles v0 v2 v1 and v0 v2 v3.
void d3dDrawPolyF4(int trans)
{
	POLYF4 *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	VERTEX *v3;

	poly = (POLYF4 *)polyblock;
	polyblock += sizeof(POLYF4);

	v0 = GetVertex(poly->v0, VertexRotatePers_D3D);
	v1 = GetVertex(poly->v1, VertexRotatePers_D3D);
	v2 = GetVertex(poly->v2, VertexRotatePers_D3D);
	v3 = GetVertex(poly->v3, VertexRotatePers_D3D);

	if (do_clipping && PolyOffScreen4(v0, v1, v2, v3))
		return;

	SetColor(&VertexA, poly->r, poly->g, poly->b);
	SetColor(&VertexB, poly->r, poly->g, poly->b);
	SetColor(&VertexC, poly->r, poly->g, poly->b);

	SetFlatColorCombine();
	SetTransparency(trans);

	SetPosition(&VertexA, v0);
	SetPosition(&VertexB, v2);
	SetPosition(&VertexC, v1);
	DrawTriangleABC();

	SetPosition(&VertexC, v3);
	DrawTriangleABC();
}

// [0x4532ae] [D]
void d3dDrawPolyG3(int trans)
{
	POLYG3 *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;

	poly = (POLYG3 *)polyblock;
	polyblock += sizeof(POLYG3);

	v0 = GetVertex(poly->v0, VertexRotatePers_D3D);
	v1 = GetVertex(poly->v1, VertexRotatePers_D3D);
	v2 = GetVertex(poly->v2, VertexRotatePers_D3D);

	if (do_clipping && PolyOffScreen3(v0, v1, v2))
		return;

	SetColor(&VertexA, poly->r0, poly->g0, poly->b0);
	SetColor(&VertexB, poly->r1, poly->g1, poly->b1);
	SetColor(&VertexC, poly->r2, poly->g2, poly->b2);

	SetFlatColorCombine();
	SetTransparency(trans);

	SetPosition(&VertexA, v0);
	SetPosition(&VertexB, v1);
	SetPosition(&VertexC, v2);

	DrawTriangleABC();
}

// [0x453735] [D]
void d3dDrawPolyG4(int trans)
{
	POLYG4 *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	VERTEX *v3;

	poly = (POLYG4 *)polyblock;
	polyblock += sizeof(POLYG4);

	v0 = GetVertex(poly->v0, VertexRotatePers_D3D);
	v1 = GetVertex(poly->v1, VertexRotatePers_D3D);
	v2 = GetVertex(poly->v2, VertexRotatePers_D3D);
	v3 = GetVertex(poly->v3, VertexRotatePers_D3D);

	if (do_clipping && PolyOffScreen4(v0, v1, v2, v3))
		return;

	// NOTE: the colours of v1 and v2 are swapped and v3's is not used (the original's)
	SetColor(&VertexA, poly->r0, poly->g0, poly->b0);
	SetColor(&VertexB, poly->r1, poly->g1, poly->b1);
	SetColor(&VertexC, poly->r2, poly->g2, poly->b2);

	SetFlatColorCombine();
	SetTransparency(trans);

	SetPosition(&VertexA, v0);
	SetPosition(&VertexB, v2);
	SetPosition(&VertexC, v1);
	DrawTriangleABC();

	SetPosition(&VertexC, v3);
	DrawTriangleABC();
}

// [0x453cc9] [D]
void d3dDrawPolyFT3(int trans)
{
	POLYFT3 *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;

	poly = (POLYFT3 *)polyblock;
	polyblock += sizeof(POLYFT3);

	v0 = GetVertex(poly->v0, VertexRotatePers_D3D);
	v1 = GetVertex(poly->v1, VertexRotatePers_D3D);
	v2 = GetVertex(poly->v2, VertexRotatePers_D3D);

	if (do_clipping && PolyOffScreen3(v0, v1, v2))
		return;

	SetColor(&VertexA, poly->r, poly->g, poly->b);
	SetColor(&VertexB, poly->r, poly->g, poly->b);
	SetColor(&VertexC, poly->r, poly->g, poly->b);

	SetTexturedColorCombine();
	SetTextureSet(poly->texture_set);
	SetTransparency(trans);

	SetPosition(&VertexA, v0);
	SetPosition(&VertexB, v1);
	SetPosition(&VertexC, v2);

	SetTexCoord(&VertexA, poly->s0, poly->t0);
	SetTexCoord(&VertexB, poly->s1, poly->t1);
	SetTexCoord(&VertexC, poly->s2, poly->t2);

	DrawTriangleABC();
}

// [0x454201] [D]
void d3dDrawPolyFT4(int trans)
{
	POLYFT4 *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	VERTEX *v3;

	poly = (POLYFT4 *)polyblock;
	polyblock += sizeof(POLYFT4);

	v0 = GetVertex(poly->v0, VertexRotatePers_D3D);
	v1 = GetVertex(poly->v1, VertexRotatePers_D3D);
	v2 = GetVertex(poly->v2, VertexRotatePers_D3D);
	v3 = GetVertex(poly->v3, VertexRotatePers_D3D);

	if (do_clipping && PolyOffScreen4(v0, v1, v2, v3))
		return;

	SetColor(&VertexA, poly->r, poly->g, poly->b);
	SetColor(&VertexB, poly->r, poly->g, poly->b);
	SetColor(&VertexC, poly->r, poly->g, poly->b);

	SetTexturedColorCombine();
	SetTextureSet(poly->texture_set);
	SetTransparency(trans);

	SetPosition(&VertexA, v0);
	SetPosition(&VertexB, v2);
	SetPosition(&VertexC, v1);

	SetTexCoord(&VertexA, poly->s0, poly->t0);
	SetTexCoord(&VertexB, poly->s2, poly->t2);
	SetTexCoord(&VertexC, poly->s1, poly->t1);
	DrawTriangleABC();

	SetPosition(&VertexC, v3);
	SetTexCoord(&VertexC, poly->s3, poly->t3);
	DrawTriangleABC();
}

// [0x45486c] [D]
void d3dDrawPolyGT3(int trans)
{
	POLYGT3 *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;

	poly = (POLYGT3 *)polyblock;
	polyblock += sizeof(POLYGT3);

	v0 = GetVertex(poly->v0, VertexRotatePers_D3D);
	v1 = GetVertex(poly->v1, VertexRotatePers_D3D);
	v2 = GetVertex(poly->v2, VertexRotatePers_D3D);

	if (do_clipping && PolyOffScreen3(v0, v1, v2))
		return;

	SetColor(&VertexA, poly->r0, poly->g0, poly->b0);
	SetColor(&VertexB, poly->r1, poly->g1, poly->b1);
	SetColor(&VertexC, poly->r2, poly->g2, poly->b2);

	SetTexturedColorCombine();
	SetTextureSet(poly->texture_set);
	SetTransparency(trans);

	SetPosition(&VertexA, v0);
	SetPosition(&VertexB, v1);
	SetPosition(&VertexC, v2);

	SetTexCoord(&VertexA, poly->s0, poly->t0);
	SetTexCoord(&VertexB, poly->s1, poly->t1);
	SetTexCoord(&VertexC, poly->s2, poly->t2);

	DrawTriangleABC();
}

// [0x454da6] [D]
void d3dDrawPolyGT4(int trans)
{
	POLYGT4 *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	VERTEX *v3;

	poly = (POLYGT4 *)polyblock;
	polyblock += sizeof(POLYGT4);

	v0 = GetVertex(poly->v0, VertexRotatePers_D3D);
	v1 = GetVertex(poly->v1, VertexRotatePers_D3D);
	v2 = GetVertex(poly->v2, VertexRotatePers_D3D);
	v3 = GetVertex(poly->v3, VertexRotatePers_D3D);

	if (do_clipping && PolyOffScreen4(v0, v1, v2, v3))
		return;

	SetColor(&VertexA, poly->r0, poly->g0, poly->b0);
	SetColor(&VertexB, poly->r2, poly->g2, poly->b2);
	SetColor(&VertexC, poly->r1, poly->g1, poly->b1);

	SetTexturedColorCombine();
	SetTextureSet(poly->texture_set);
	SetTransparency(trans);

	SetPosition(&VertexA, v0);
	SetPosition(&VertexB, v2);
	SetPosition(&VertexC, v1);

	SetTexCoord(&VertexA, poly->s0, poly->t0);
	SetTexCoord(&VertexB, poly->s2, poly->t2);
	SetTexCoord(&VertexC, poly->s1, poly->t1);
	DrawTriangleABC();

	SetPosition(&VertexC, v3);
	SetTexCoord(&VertexC, poly->s3, poly->t3);
	SetColor(&VertexC, poly->r3, poly->g3, poly->b3);
	DrawTriangleABC();
}

// [0x455436] [D]
// The _LIT polygons scale their colour by the light intensity of their normal (DrawObject)
// or of each point normal (G, GT), plus the lamps (FT, GT).
void d3dDrawPolyF3_LIT(int trans)
{
	POLYF3LIT *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	float li;

	poly = (POLYF3LIT *)polyblock;
	polyblock += sizeof(POLYF3LIT);

	v0 = GetVertex(poly->v0, VertexRotatePers_D3D);
	v1 = GetVertex(poly->v1, VertexRotatePers_D3D);
	v2 = GetVertex(poly->v2, VertexRotatePers_D3D);

	if (do_clipping && PolyOffScreen3(v0, v1, v2))
		return;

	li = light_intensity[poly->normal];

	SetColor(&VertexA, li * poly->r, li * poly->g, li * poly->b);
	SetColor(&VertexB, li * poly->r, li * poly->g, li * poly->b);
	SetColor(&VertexC, li * poly->r, li * poly->g, li * poly->b);

	SetFlatColorCombine();
	SetTransparency(trans);

	SetPosition(&VertexA, v0);
	SetPosition(&VertexB, v1);
	SetPosition(&VertexC, v2);

	DrawTriangleABC();
}

// [0x4558e6] [D]
void d3dDrawPolyF4_LIT(int trans)
{
	POLYF4LIT *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	VERTEX *v3;
	float li;

	poly = (POLYF4LIT *)polyblock;
	polyblock += sizeof(POLYF4LIT);

	v0 = GetVertex(poly->v0, VertexRotatePers_D3D);
	v1 = GetVertex(poly->v1, VertexRotatePers_D3D);
	v2 = GetVertex(poly->v2, VertexRotatePers_D3D);
	v3 = GetVertex(poly->v3, VertexRotatePers_D3D);

	if (do_clipping && PolyOffScreen4(v0, v1, v2, v3))
		return;

	li = light_intensity[poly->normal];

	SetColor(&VertexA, li * poly->r, li * poly->g, li * poly->b);
	SetColor(&VertexB, li * poly->r, li * poly->g, li * poly->b);
	SetColor(&VertexC, li * poly->r, li * poly->g, li * poly->b);

	SetFlatColorCombine();
	SetTransparency(trans);

	SetPosition(&VertexA, v0);
	SetPosition(&VertexB, v2);
	SetPosition(&VertexC, v1);
	DrawTriangleABC();

	SetPosition(&VertexC, v3);
	DrawTriangleABC();
}

// [0x455ea3] [D]
void d3dDrawPolyG3_LIT(int trans)
{
	POLYG3LIT *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	float li;

	poly = (POLYG3LIT *)polyblock;
	polyblock += sizeof(POLYG3LIT);

	v0 = GetVertex(poly->v0, VertexRotatePers_D3D);
	v1 = GetVertex(poly->v1, VertexRotatePers_D3D);
	v2 = GetVertex(poly->v2, VertexRotatePers_D3D);

	if (do_clipping && PolyOffScreen3(v0, v1, v2))
		return;

	li = conv_point_normals[poly->pn0];
	SetColor(&VertexA, poly->r * li, poly->g * li, poly->b * li);

	li = conv_point_normals[poly->pn1];
	SetColor(&VertexB, poly->r * li, poly->g * li, poly->b * li);

	li = conv_point_normals[poly->pn2];
	SetColor(&VertexC, poly->r * li, poly->g * li, poly->b * li);

	SetFlatColorCombine();
	SetTransparency(trans);

	SetPosition(&VertexA, v0);
	SetPosition(&VertexB, v1);
	SetPosition(&VertexC, v2);

	DrawTriangleABC();
}

// [0x456378] [D]
void d3dDrawPolyG4_LIT(int trans)
{
	POLYG4LIT *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	VERTEX *v3;
	float li;

	poly = (POLYG4LIT *)polyblock;
	polyblock += sizeof(POLYG4LIT);

	v0 = GetVertex(poly->v0, VertexRotatePers_D3D);
	v1 = GetVertex(poly->v1, VertexRotatePers_D3D);
	v2 = GetVertex(poly->v2, VertexRotatePers_D3D);
	v3 = GetVertex(poly->v3, VertexRotatePers_D3D);

	if (do_clipping && PolyOffScreen4(v0, v1, v2, v3))
		return;

	li = conv_point_normals[poly->pn0];
	SetColor(&VertexA, poly->r * li, poly->g * li, poly->b * li);

	li = conv_point_normals[poly->pn2];
	SetColor(&VertexB, poly->r * li, poly->g * li, poly->b * li);

	li = conv_point_normals[poly->pn1];
	SetColor(&VertexC, poly->r * li, poly->g * li, poly->b * li);

	SetFlatColorCombine();
	SetTransparency(trans);

	SetPosition(&VertexA, v0);
	SetPosition(&VertexB, v2);
	SetPosition(&VertexC, v1);
	DrawTriangleABC();

	SetPosition(&VertexC, v3);

	li = conv_point_normals[poly->pn3];
	SetColor(&VertexC, poly->r * li, poly->g * li, poly->b * li);

	DrawTriangleABC();
}

// [0x45698e] [D]
void d3dDrawPolyFT3_LIT(int trans)
{
	POLYFT3LIT *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	float li;
	float r;
	float g;
	float b;

	poly = (POLYFT3LIT *)polyblock;
	polyblock += sizeof(POLYFT3LIT);

	v0 = GetVertex(poly->v0, VertexRotatePers_D3D);
	v1 = GetVertex(poly->v1, VertexRotatePers_D3D);
	v2 = GetVertex(poly->v2, VertexRotatePers_D3D);

	if (do_clipping && PolyOffScreen3(v0, v1, v2))
		return;

	li = light_intensity[poly->normal];

	r = li * poly->r;
	g = li * poly->g;
	b = li * poly->b;

	if (num_potential_lamps)
	{
		AddLampLight(&VertexA, v0->tx, v0->ty, v0->vz, r, g, b);
		AddLampLight(&VertexB, v1->tx, v1->ty, v1->vz, r, g, b);
		AddLampLight(&VertexC, v2->tx, v2->ty, v2->vz, r, g, b);
	}
	else
	{
		SetColor(&VertexA, r, g, b);
		SetColor(&VertexB, r, g, b);
		SetColor(&VertexC, r, g, b);
	}

	SetTexturedColorCombine();
	SetTextureSet(poly->texture_set);
	SetTransparency(trans);

	SetPosition(&VertexA, v0);
	SetPosition(&VertexB, v1);
	SetPosition(&VertexC, v2);

	SetTexCoord(&VertexA, poly->s0, poly->t0);
	SetTexCoord(&VertexB, poly->s1, poly->t1);
	SetTexCoord(&VertexC, poly->s2, poly->t2);

	DrawTriangleABC();
}

// [0x45737a] [D]
void d3dDrawPolyFT4_LIT(int trans)
{
	POLYFT4LIT *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	VERTEX *v3;
	float li;
	float r;
	float g;
	float b;

	poly = (POLYFT4LIT *)polyblock;
	polyblock += sizeof(POLYFT4LIT);

	v0 = GetVertex(poly->v0, VertexRotatePers_D3D);
	v1 = GetVertex(poly->v1, VertexRotatePers_D3D);
	v2 = GetVertex(poly->v2, VertexRotatePers_D3D);
	v3 = GetVertex(poly->v3, VertexRotatePers_D3D);

	if (do_clipping && PolyOffScreen4(v0, v1, v2, v3))
		return;

	li = light_intensity[poly->normal];

	r = li * poly->r;
	g = li * poly->g;
	b = li * poly->b;

	if (num_potential_lamps)
	{
		AddLampLight(&VertexA, v0->tx, v0->ty, v0->vz, r, g, b);
		AddLampLight(&VertexB, v2->tx, v2->ty, v2->vz, r, g, b);
		AddLampLight(&VertexC, v1->tx, v1->ty, v1->vz, r, g, b);
	}
	else
	{
		SetColor(&VertexA, r, g, b);
		SetColor(&VertexB, r, g, b);
		SetColor(&VertexC, r, g, b);
	}

	SetTexturedColorCombine();
	SetTextureSet(poly->texture_set);
	SetTransparency(trans);

	SetPosition(&VertexA, v0);
	SetPosition(&VertexB, v2);
	SetPosition(&VertexC, v1);

	SetTexCoord(&VertexA, poly->s0, poly->t0);
	SetTexCoord(&VertexB, poly->s2, poly->t2);
	SetTexCoord(&VertexC, poly->s1, poly->t1);
	DrawTriangleABC();

	if (num_potential_lamps)
		AddLampLight(&VertexC, v3->tx, v3->ty, v3->vz, r, g, b);

	SetPosition(&VertexC, v3);
	SetTexCoord(&VertexC, poly->s3, poly->t3);
	DrawTriangleABC();
}

// [0x458149] [D]
// With gShinyType (DrawCarModel) also queues the car shine of the polygon; type 2 draws only
// the shine.
void d3dDrawPolyGT3_LIT(int trans)
{
	POLYGT3LIT *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	float li;

	poly = (POLYGT3LIT *)polyblock;
	polyblock += sizeof(POLYGT3LIT);

	v0 = GetVertex(poly->v0, VertexRotatePers_D3D);
	v1 = GetVertex(poly->v1, VertexRotatePers_D3D);
	v2 = GetVertex(poly->v2, VertexRotatePers_D3D);

	if (do_clipping && PolyOffScreen3(v0, v1, v2))
		return;

	if (num_potential_lamps)
	{
		li = conv_point_normals[poly->pn0];
		AddLampLight(&VertexA, v0->tx, v0->ty, v0->vz, li * poly->r, li * poly->g, li * poly->b);

		li = conv_point_normals[poly->pn1];
		AddLampLight(&VertexB, v1->tx, v1->ty, v1->vz, li * poly->r, li * poly->g, li * poly->b);

		li = conv_point_normals[poly->pn2];
		AddLampLight(&VertexC, v2->tx, v2->ty, v2->vz, li * poly->r, li * poly->g, li * poly->b);
	}
	else
	{
		li = conv_point_normals[poly->pn0];
		SetColor(&VertexA, poly->r * li, poly->g * li, poly->b * li);

		li = conv_point_normals[poly->pn1];
		SetColor(&VertexB, poly->r * li, poly->g * li, poly->b * li);

		li = conv_point_normals[poly->pn2];
		SetColor(&VertexC, poly->r * li, poly->g * li, poly->b * li);
	}

	SetTexturedColorCombine();
	SetTextureSet(poly->texture_set);
	SetTransparency(trans);

	SetPosition(&VertexA, v0);
	SetPosition(&VertexB, v1);
	SetPosition(&VertexC, v2);

	SetTexCoord(&VertexA, poly->s0, poly->t0);
	SetTexCoord(&VertexB, poly->s1, poly->t1);
	SetTexCoord(&VertexC, poly->s2, poly->t2);

	if (gShinyType != 2)
		DrawTriangleABC();

	if (gShinyType)
	{
		SetShine(&VertexA, poly->pn0);
		SetShine(&VertexB, poly->pn1);
		SetShine(&VertexC, poly->pn2);

		AddCarShine(&VertexA, &VertexB, &VertexC, do_clipping ? 1 : 0);
	}
}

// [0x458e7f] [D]
void d3dDrawPolyGT4_LIT(int trans)
{
	POLYGT4LIT *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	VERTEX *v3;
	float li;

	poly = (POLYGT4LIT *)polyblock;
	polyblock += sizeof(POLYGT4LIT);

	v0 = GetVertex(poly->v0, VertexRotatePers_D3D);
	v1 = GetVertex(poly->v1, VertexRotatePers_D3D);
	v2 = GetVertex(poly->v2, VertexRotatePers_D3D);
	v3 = GetVertex(poly->v3, VertexRotatePers_D3D);

	if (do_clipping && PolyOffScreen4(v0, v1, v2, v3))
		return;

	if (num_potential_lamps)
	{
		li = conv_point_normals[poly->pn0];
		AddLampLight(&VertexA, v0->tx, v0->ty, v0->vz, li * poly->r, li * poly->g, li * poly->b);

		li = conv_point_normals[poly->pn2];
		AddLampLight(&VertexB, v2->tx, v2->ty, v2->vz, li * poly->r, li * poly->g, li * poly->b);

		li = conv_point_normals[poly->pn1];
		AddLampLight(&VertexC, v1->tx, v1->ty, v1->vz, li * poly->r, li * poly->g, li * poly->b);
	}
	else
	{
		li = conv_point_normals[poly->pn0];
		SetColor(&VertexA, poly->r * li, poly->g * li, poly->b * li);

		li = conv_point_normals[poly->pn2];
		SetColor(&VertexB, poly->r * li, poly->g * li, poly->b * li);

		li = conv_point_normals[poly->pn1];
		SetColor(&VertexC, poly->r * li, poly->g * li, poly->b * li);
	}

	SetTexturedColorCombine();
	SetTextureSet(poly->texture_set);
	SetTransparency(trans);

	SetPosition(&VertexA, v0);
	SetPosition(&VertexB, v2);
	SetPosition(&VertexC, v1);

	SetTexCoord(&VertexA, poly->s0, poly->t0);
	SetTexCoord(&VertexB, poly->s2, poly->t2);
	SetTexCoord(&VertexC, poly->s1, poly->t1);

	if (gShinyType != 2)
		DrawTriangleABC();

	SetPosition(&VertexC, v3);
	SetTexCoord(&VertexC, poly->s3, poly->t3);

	li = conv_point_normals[poly->pn3];

	if (num_potential_lamps)
		AddLampLight(&VertexC, v3->tx, v3->ty, v3->vz, li * poly->r, li * poly->g, li * poly->b);
	else
		SetColor(&VertexC, poly->r * li, poly->g * li, poly->b * li);

	if (gShinyType != 2)
		DrawTriangleABC();

	if (gShinyType)
	{
		// NOTE: VertexC keeps v3's z for the first shine triangle (the original's)
		VertexC.x = v1->vx;
		VertexC.y = v1->vy;
		VertexC.oow = v1->oow;

		SetShine(&VertexA, poly->pn0);
		SetShine(&VertexB, poly->pn2);
		SetShine(&VertexC, poly->pn1);

		AddCarShine(&VertexA, &VertexB, &VertexC, do_clipping ? 1 : 0);

		VertexC.x = v3->vx;
		VertexC.y = v3->vy;
		VertexC.z = v3->vz;
		VertexC.oow = v3->oow;

		SetTexCoord(&VertexC, (float)stru_11D9060[poly->pn3].field_0, (float)stru_11D9060[poly->pn3].field_1);

		AddCarShine(&VertexA, &VertexB, &VertexC, do_clipping ? 1 : 0);
	}
}

// [0x45a091] [D]
// The near clipped polygons (the object crosses z = NEAR_CLIP_Z): the vertices are rotated
// only, the polygon clipped (ClipTestScale*) and the points projected here.
void d3dDrawClippedPolyF3(int trans)
{
	POLYF3 *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	VERTEX scr[5];
	int count;

	poly = (POLYF3 *)polyblock;
	polyblock += sizeof(POLYF3);

	v0 = GetVertex(poly->v0, VertexRotate);
	v1 = GetVertex(poly->v1, VertexRotate);
	v2 = GetVertex(poly->v2, VertexRotate);

	num_clip_poly_points = 0;
	clip_ptr = clip_poly_info;

	ClipTestScale(v0, v1);
	ClipTestScale(v1, v2);
	ClipTestScale(v2, v0);

	count = ProjectClipPoly(scr, 0);

	if (count == 0)
		return;

	SetFlatColorCombine();
	SetTransparency(trans);

	SetColor(&VertexA, poly->r, poly->g, poly->b);
	SetColor(&VertexB, poly->r, poly->g, poly->b);
	SetColor(&VertexC, poly->r, poly->g, poly->b);

	DrawClipFan(scr, count, 0);
}

// [0x45a966] [D]
void d3dDrawClippedPolyF4(int trans)
{
	POLYF4 *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	VERTEX *v3;
	VERTEX scr[5];
	int count;

	poly = (POLYF4 *)polyblock;
	polyblock += sizeof(POLYF4);

	v0 = GetVertex(poly->v0, VertexRotate);
	v1 = GetVertex(poly->v1, VertexRotate);
	v2 = GetVertex(poly->v2, VertexRotate);
	v3 = GetVertex(poly->v3, VertexRotate);

	num_clip_poly_points = 0;
	clip_ptr = clip_poly_info;

	ClipTestScale(v0, v1);
	ClipTestScale(v1, v2);
	ClipTestScale(v2, v3);
	ClipTestScale(v3, v0);

	count = ProjectClipPoly(scr, CLIP_QUAD | CLIP_TEST5);

	if (count == 0)
		return;

	SetFlatColorCombine();
	SetTransparency(trans);

	SetColor(&VertexA, poly->r, poly->g, poly->b);
	SetColor(&VertexB, poly->r, poly->g, poly->b);
	SetColor(&VertexC, poly->r, poly->g, poly->b);

	DrawClipFan(scr, count, 0);
}

// [0x45b731] [D]
void d3dDrawClippedPolyG3(int trans)
{
	POLYG3 *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	VERTEX scr[5];
	int count;

	poly = (POLYG3 *)polyblock;
	polyblock += sizeof(POLYG3);

	v0 = GetVertex(poly->v0, VertexRotate);
	v1 = GetVertex(poly->v1, VertexRotate);
	v2 = GetVertex(poly->v2, VertexRotate);

	num_clip_poly_points = 0;
	clip_ptr = clip_poly_info;

	ClipTestScaleRGB(v1, v2, poly->r1, poly->g1, poly->b1, poly->r2, poly->g2, poly->b2);
	ClipTestScaleRGB(v2, v0, poly->r2, poly->g2, poly->b2, poly->r0, poly->g0, poly->b0);
	ClipTestScaleRGB(v0, v1, poly->r0, poly->g0, poly->b0, poly->r1, poly->g1, poly->b1);

	count = ProjectClipPoly(scr, 0);

	if (count == 0)
		return;

	SetFlatColorCombine();
	SetTransparency(trans);

	DrawClipFan(scr, count, CLIP_RGB | CLIP_RGB_BUG);
}

// [0x45c146] [D]
void d3dDrawClippedPolyG4(int trans)
{
	POLYG4 *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	VERTEX *v3;
	VERTEX scr[5];
	int count;

	poly = (POLYG4 *)polyblock;
	polyblock += sizeof(POLYG4);

	v0 = GetVertex(poly->v0, VertexRotate);
	v1 = GetVertex(poly->v1, VertexRotate);
	v2 = GetVertex(poly->v2, VertexRotate);
	v3 = GetVertex(poly->v3, VertexRotate);

	num_clip_poly_points = 0;
	clip_ptr = clip_poly_info;

	ClipTestScaleRGB(v3, v0, poly->r3, poly->g3, poly->b3, poly->r0, poly->g0, poly->b0);
	ClipTestScaleRGB(v0, v1, poly->r0, poly->g0, poly->b0, poly->r1, poly->g1, poly->b1);
	ClipTestScaleRGB(v1, v2, poly->r1, poly->g1, poly->b1, poly->r2, poly->g2, poly->b2);
	ClipTestScaleRGB(v2, v3, poly->r2, poly->g2, poly->b2, poly->r3, poly->g3, poly->b3);

	count = ProjectClipPoly(scr, CLIP_QUAD | CLIP_TEST5);

	if (count == 0)
		return;

	SetFlatColorCombine();
	SetTransparency(trans);

	DrawClipFan(scr, count, CLIP_RGB);
}

// [0x45d18a] [D]
void d3dDrawClippedPolyFT3(int trans)
{
	POLYFT3 *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	VERTEX scr[5];
	int count;

	poly = (POLYFT3 *)polyblock;
	polyblock += sizeof(POLYFT3);

	v0 = GetVertex(poly->v0, VertexRotate);
	v1 = GetVertex(poly->v1, VertexRotate);
	v2 = GetVertex(poly->v2, VertexRotate);

	num_clip_poly_points = 0;
	clip_ptr = clip_poly_info;

	ClipTestScaleUV(v0, v1, poly->s0, poly->t0, poly->s1, poly->t1);
	ClipTestScaleUV(v1, v2, poly->s1, poly->t1, poly->s2, poly->t2);
	ClipTestScaleUV(v2, v0, poly->s2, poly->t2, poly->s0, poly->t0);

	count = ProjectClipPoly(scr, 0);

	if (count == 0)
		return;

	SetTexturedColorCombine();
	SetTextureSet(poly->texture_set);
	SetTransparency(trans);

	SetColor(&VertexA, poly->r, poly->g, poly->b);
	SetColor(&VertexB, poly->r, poly->g, poly->b);
	SetColor(&VertexC, poly->r, poly->g, poly->b);

	DrawClipFan(scr, count, CLIP_UV);
}

// [0x45dc13] [D]
void d3dDrawClippedPolyFT4(int trans)
{
	POLYFT4 *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	VERTEX *v3;
	VERTEX scr[5];
	int count;

	poly = (POLYFT4 *)polyblock;
	polyblock += sizeof(POLYFT4);

	v0 = GetVertex(poly->v0, VertexRotate);
	v1 = GetVertex(poly->v1, VertexRotate);
	v2 = GetVertex(poly->v2, VertexRotate);
	v3 = GetVertex(poly->v3, VertexRotate);

	num_clip_poly_points = 0;
	clip_ptr = clip_poly_info;

	ClipTestScaleUV(v0, v1, poly->s0, poly->t0, poly->s1, poly->t1);
	ClipTestScaleUV(v1, v2, poly->s1, poly->t1, poly->s2, poly->t2);
	ClipTestScaleUV(v2, v3, poly->s2, poly->t2, poly->s3, poly->t3);
	ClipTestScaleUV(v3, v0, poly->s3, poly->t3, poly->s0, poly->t0);

	count = ProjectClipPoly(scr, CLIP_QUAD | CLIP_TEST5);

	if (count == 0)
		return;

	SetTexturedColorCombine();
	SetTextureSet(poly->texture_set);
	SetTransparency(trans);

	SetColor(&VertexA, poly->r, poly->g, poly->b);
	SetColor(&VertexB, poly->r, poly->g, poly->b);
	SetColor(&VertexC, poly->r, poly->g, poly->b);

	DrawClipFan(scr, count, CLIP_UV);
}

// [0x45ec8e] [D]
void d3dDrawClippedPolyGT3(int trans)
{
	POLYGT3 *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	VERTEX scr[5];
	int count;

	poly = (POLYGT3 *)polyblock;
	polyblock += sizeof(POLYGT3);

	v0 = GetVertex(poly->v0, VertexRotate);
	v1 = GetVertex(poly->v1, VertexRotate);
	v2 = GetVertex(poly->v2, VertexRotate);

	num_clip_poly_points = 0;
	clip_ptr = clip_poly_info;

	ClipTestScaleRGBUV(v1, v2, poly->r1, poly->g1, poly->b1, poly->s1, poly->t1, poly->r2, poly->g2, poly->b2, poly->s2, poly->t2);
	ClipTestScaleRGBUV(v2, v0, poly->r2, poly->g2, poly->b2, poly->s2, poly->t2, poly->r0, poly->g0, poly->b0, poly->s0, poly->t0);
	ClipTestScaleRGBUV(v0, v1, poly->r0, poly->g0, poly->b0, poly->s0, poly->t0, poly->r1, poly->g1, poly->b1, poly->s1, poly->t1);

	count = ProjectClipPoly(scr, 0);

	if (count == 0)
		return;

	SetTexturedColorCombine();
	SetTextureSet(poly->texture_set);
	SetTransparency(trans);

	DrawClipFan(scr, count, CLIP_RGB | CLIP_UV | CLIP_RGB_BUG);
}

// [0x45f857] [D]
void d3dDrawClippedPolyGT4(int trans)
{
	POLYGT4 *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	VERTEX *v3;
	VERTEX scr[5];
	int count;

	poly = (POLYGT4 *)polyblock;
	polyblock += sizeof(POLYGT4);

	v0 = GetVertex(poly->v0, VertexRotate);
	v1 = GetVertex(poly->v1, VertexRotate);
	v2 = GetVertex(poly->v2, VertexRotate);
	v3 = GetVertex(poly->v3, VertexRotate);

	num_clip_poly_points = 0;
	clip_ptr = clip_poly_info;

	ClipTestScaleRGBUV(v3, v0, poly->r3, poly->g3, poly->b3, poly->s3, poly->t3, poly->r0, poly->g0, poly->b0, poly->s0, poly->t0);
	ClipTestScaleRGBUV(v0, v1, poly->r0, poly->g0, poly->b0, poly->s0, poly->t0, poly->r1, poly->g1, poly->b1, poly->s1, poly->t1);
	ClipTestScaleRGBUV(v1, v2, poly->r1, poly->g1, poly->b1, poly->s1, poly->t1, poly->r2, poly->g2, poly->b2, poly->s2, poly->t2);
	ClipTestScaleRGBUV(v2, v3, poly->r2, poly->g2, poly->b2, poly->s2, poly->t2, poly->r3, poly->g3, poly->b3, poly->s3, poly->t3);

	count = ProjectClipPoly(scr, CLIP_QUAD | CLIP_TEST5);

	if (count == 0)
		return;

	SetTexturedColorCombine();
	SetTextureSet(poly->texture_set);
	SetTransparency(trans);

	DrawClipFan(scr, count, CLIP_RGB | CLIP_UV);
}

// [0x460b5d] [D]
void d3dDrawClippedPolyF3_LIT(int trans)
{
	POLYF3LIT *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	VERTEX scr[5];
	float li;
	int count;

	poly = (POLYF3LIT *)polyblock;
	polyblock += sizeof(POLYF3LIT);

	v0 = GetVertex(poly->v0, VertexRotate);
	v1 = GetVertex(poly->v1, VertexRotate);
	v2 = GetVertex(poly->v2, VertexRotate);

	num_clip_poly_points = 0;
	clip_ptr = clip_poly_info;

	ClipTestScale(v0, v1);
	ClipTestScale(v1, v2);
	ClipTestScale(v2, v0);

	count = ProjectClipPoly(scr, 0);

	if (count == 0)
		return;

	SetFlatColorCombine();
	SetTransparency(trans);

	li = light_intensity[poly->normal];

	SetColor(&VertexA, li * poly->r, li * poly->g, li * poly->b);
	SetColor(&VertexB, li * poly->r, li * poly->g, li * poly->b);
	SetColor(&VertexC, li * poly->r, li * poly->g, li * poly->b);

	DrawClipFan(scr, count, 0);
}

// [0x461494] [D]
void d3dDrawClippedPolyF4_LIT(int trans)
{
	POLYF4LIT *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	VERTEX *v3;
	VERTEX scr[5];
	float li;
	int count;

	poly = (POLYF4LIT *)polyblock;
	polyblock += sizeof(POLYF4LIT);

	v0 = GetVertex(poly->v0, VertexRotate);
	v1 = GetVertex(poly->v1, VertexRotate);
	v2 = GetVertex(poly->v2, VertexRotate);
	v3 = GetVertex(poly->v3, VertexRotate);

	num_clip_poly_points = 0;
	clip_ptr = clip_poly_info;

	ClipTestScale(v0, v1);
	ClipTestScale(v1, v2);
	ClipTestScale(v2, v3);
	ClipTestScale(v3, v0);

	count = ProjectClipPoly(scr, CLIP_QUAD);

	if (count == 0)
		return;

	SetFlatColorCombine();
	SetTransparency(trans);

	li = light_intensity[poly->normal];

	SetColor(&VertexA, li * poly->r, li * poly->g, li * poly->b);
	SetColor(&VertexB, li * poly->r, li * poly->g, li * poly->b);
	SetColor(&VertexC, li * poly->r, li * poly->g, li * poly->b);

	DrawClipFan(scr, count, 0);
}

// [0x46230e] [D]
void d3dDrawClippedPolyG3_LIT(int trans)
{
	POLYG3LIT *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	VERTEX scr[5];
	int count;
	int i;

	poly = (POLYG3LIT *)polyblock;
	polyblock += sizeof(POLYG3LIT);

	v0 = GetVertex(poly->v0, VertexRotate);
	v1 = GetVertex(poly->v1, VertexRotate);
	v2 = GetVertex(poly->v2, VertexRotate);

	num_clip_poly_points = 0;
	clip_ptr = clip_poly_info;

	ClipTestScaleIntensity(v1, v2, conv_point_normals[poly->pn1], conv_point_normals[poly->pn2]);
	ClipTestScaleIntensity(v2, v0, conv_point_normals[poly->pn2], conv_point_normals[poly->pn0]);
	ClipTestScaleIntensity(v0, v1, conv_point_normals[poly->pn0], conv_point_normals[poly->pn1]);

	count = ProjectClipPoly(scr, 0);

	if (count == 0)
		return;

	SetFlatColorCombine();
	SetTransparency(trans);

	// NOTE: the original computes each draw vertex's colour from the point's intensity
	// directly; the colours go through clip_poly_info here
	for (i = 0; i < count; i++)
	{
		clip_poly_info[i].r = poly->r * clip_poly_info[i].intensity;
		clip_poly_info[i].g = poly->g * clip_poly_info[i].intensity;
		clip_poly_info[i].b = poly->b * clip_poly_info[i].intensity;
	}

	DrawClipFan(scr, count, CLIP_RGB);
}

// [0x462ced] [D]
void d3dDrawClippedPolyG4_LIT(int trans)
{
	POLYG4LIT *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	VERTEX *v3;
	VERTEX scr[5];
	int count;
	int i;

	poly = (POLYG4LIT *)polyblock;
	polyblock += sizeof(POLYG4LIT);

	v0 = GetVertex(poly->v0, VertexRotate);
	v1 = GetVertex(poly->v1, VertexRotate);
	v2 = GetVertex(poly->v2, VertexRotate);
	v3 = GetVertex(poly->v3, VertexRotate);

	num_clip_poly_points = 0;
	clip_ptr = clip_poly_info;

	ClipTestScaleIntensity(v3, v0, conv_point_normals[poly->pn3], conv_point_normals[poly->pn0]);
	ClipTestScaleIntensity(v0, v1, conv_point_normals[poly->pn0], conv_point_normals[poly->pn1]);
	ClipTestScaleIntensity(v1, v2, conv_point_normals[poly->pn1], conv_point_normals[poly->pn2]);
	ClipTestScaleIntensity(v2, v3, conv_point_normals[poly->pn2], conv_point_normals[poly->pn3]);

	count = ProjectClipPoly(scr, CLIP_QUAD);

	if (count == 0)
		return;

	SetFlatColorCombine();
	SetTransparency(trans);

	// NOTE: as in d3dDrawClippedPolyG3_LIT
	for (i = 0; i < count; i++)
	{
		clip_poly_info[i].r = poly->r * clip_poly_info[i].intensity;
		clip_poly_info[i].g = poly->g * clip_poly_info[i].intensity;
		clip_poly_info[i].b = poly->b * clip_poly_info[i].intensity;
	}

	DrawClipFan(scr, count, CLIP_RGB);
}

// [0x463c93] [D]
// With lamps the colour of each vertex goes through the clipping, else all are the same.
void d3dDrawClippedPolyFT3_LIT(int trans)
{
	POLYFT3LIT *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	VERTEX scr[5];
	float li;
	float r;
	float g;
	float b;
	int count;

	poly = (POLYFT3LIT *)polyblock;
	polyblock += sizeof(POLYFT3LIT);

	v0 = GetVertex(poly->v0, VertexRotate);
	v1 = GetVertex(poly->v1, VertexRotate);
	v2 = GetVertex(poly->v2, VertexRotate);

	num_clip_poly_points = 0;
	clip_ptr = clip_poly_info;

	li = light_intensity[poly->normal];

	r = li * poly->r;
	g = li * poly->g;
	b = li * poly->b;

	if (num_potential_lamps)
	{
		AddLampLight(&VertexA, v0->vx, v0->vy, v0->vz, r, g, b);
		AddLampLight(&VertexB, v1->vx, v1->vy, v1->vz, r, g, b);
		AddLampLight(&VertexC, v2->vx, v2->vy, v2->vz, r, g, b);

		ClipTestScaleRGBUV(v0, v1, VertexA.r, VertexA.g, VertexA.b, poly->s0, poly->t0, VertexB.r, VertexB.g, VertexB.b, poly->s1, poly->t1);
		ClipTestScaleRGBUV(v1, v2, VertexB.r, VertexB.g, VertexB.b, poly->s1, poly->t1, VertexC.r, VertexC.g, VertexC.b, poly->s2, poly->t2);
		ClipTestScaleRGBUV(v2, v0, VertexC.r, VertexC.g, VertexC.b, poly->s2, poly->t2, VertexA.r, VertexA.g, VertexA.b, poly->s0, poly->t0);
	}
	else
	{
		SetColor(&VertexA, r, g, b);
		SetColor(&VertexB, r, g, b);
		SetColor(&VertexC, r, g, b);

		ClipTestScaleUV(v0, v1, poly->s0, poly->t0, poly->s1, poly->t1);
		ClipTestScaleUV(v1, v2, poly->s1, poly->t1, poly->s2, poly->t2);
		ClipTestScaleUV(v2, v0, poly->s2, poly->t2, poly->s0, poly->t0);
	}

	count = ProjectClipPoly(scr, 0);

	if (count == 0)
		return;

	SetTexturedColorCombine();
	SetTextureSet(poly->texture_set);
	SetTransparency(trans);

	DrawClipFan(scr, count, num_potential_lamps ? CLIP_RGB | CLIP_UV : CLIP_UV);
}

// [0x464ed0] [D]
void d3dDrawClippedPolyFT4_LIT(int trans)
{
	POLYFT4LIT *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	VERTEX *v3;
	VERTEX scr[5];
	GrVertex light3;
	float li;
	float r;
	float g;
	float b;
	int count;

	poly = (POLYFT4LIT *)polyblock;
	polyblock += sizeof(POLYFT4LIT);

	v0 = GetVertex(poly->v0, VertexRotate);
	v1 = GetVertex(poly->v1, VertexRotate);
	v2 = GetVertex(poly->v2, VertexRotate);
	v3 = GetVertex(poly->v3, VertexRotate);

	num_clip_poly_points = 0;
	clip_ptr = clip_poly_info;

	li = light_intensity[poly->normal];

	r = li * poly->r;
	g = li * poly->g;
	b = li * poly->b;

	if (num_potential_lamps)
	{
		// NOTE: the original keeps v3's colour in unnamed globals (0xb9b59c ...)
		AddLampLight(&VertexA, v0->vx, v0->vy, v0->vz, r, g, b);
		AddLampLight(&VertexB, v1->vx, v1->vy, v1->vz, r, g, b);
		AddLampLight(&VertexC, v2->vx, v2->vy, v2->vz, r, g, b);
		AddLampLight(&light3, v3->vx, v3->vy, v3->vz, r, g, b);

		ClipTestScaleRGBUV(v0, v1, VertexA.r, VertexA.g, VertexA.b, poly->s0, poly->t0, VertexB.r, VertexB.g, VertexB.b, poly->s1, poly->t1);
		ClipTestScaleRGBUV(v1, v2, VertexB.r, VertexB.g, VertexB.b, poly->s1, poly->t1, VertexC.r, VertexC.g, VertexC.b, poly->s2, poly->t2);
		ClipTestScaleRGBUV(v2, v3, VertexC.r, VertexC.g, VertexC.b, poly->s2, poly->t2, light3.r, light3.g, light3.b, poly->s3, poly->t3);
		ClipTestScaleRGBUV(v3, v0, light3.r, light3.g, light3.b, poly->s3, poly->t3, VertexA.r, VertexA.g, VertexA.b, poly->s0, poly->t0);
	}
	else
	{
		SetColor(&VertexA, r, g, b);
		SetColor(&VertexB, r, g, b);
		SetColor(&VertexC, r, g, b);

		ClipTestScaleUV(v0, v1, poly->s0, poly->t0, poly->s1, poly->t1);
		ClipTestScaleUV(v1, v2, poly->s1, poly->t1, poly->s2, poly->t2);
		ClipTestScaleUV(v2, v3, poly->s2, poly->t2, poly->s3, poly->t3);
		ClipTestScaleUV(v3, v0, poly->s3, poly->t3, poly->s0, poly->t0);
	}

	count = ProjectClipPoly(scr, CLIP_QUAD);

	if (count == 0)
		return;

	SetTexturedColorCombine();
	SetTextureSet(poly->texture_set);
	SetTransparency(trans);

	DrawClipFan(scr, count, num_potential_lamps ? CLIP_RGB | CLIP_UV : CLIP_UV);
}

// [0x4669f8] [D]
// The colour of each vertex goes through the clipping, and with gShinyType the shine texture
// u, v too (as field_28, field_2c).
void d3dDrawClippedPolyGT3_LIT(int trans)
{
	POLYGT3LIT *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	VERTEX scr[5];
	float li;
	int count;

	poly = (POLYGT3LIT *)polyblock;
	polyblock += sizeof(POLYGT3LIT);

	v0 = GetVertex(poly->v0, VertexRotate);
	v1 = GetVertex(poly->v1, VertexRotate);
	v2 = GetVertex(poly->v2, VertexRotate);

	num_clip_poly_points = 0;
	clip_ptr = clip_poly_info;

	if (num_potential_lamps)
	{
		li = conv_point_normals[poly->pn0];
		AddLampLight(&VertexA, v0->vx, v0->vy, v0->vz, li * poly->r, li * poly->g, li * poly->b);

		li = conv_point_normals[poly->pn1];
		AddLampLight(&VertexB, v1->vx, v1->vy, v1->vz, li * poly->r, li * poly->g, li * poly->b);

		li = conv_point_normals[poly->pn2];
		AddLampLight(&VertexC, v2->vx, v2->vy, v2->vz, li * poly->r, li * poly->g, li * poly->b);
	}
	else
	{
		li = conv_point_normals[poly->pn0];
		SetColor(&VertexA, poly->r * li, poly->g * li, poly->b * li);

		li = conv_point_normals[poly->pn1];
		SetColor(&VertexB, poly->r * li, poly->g * li, poly->b * li);

		li = conv_point_normals[poly->pn2];
		SetColor(&VertexC, poly->r * li, poly->g * li, poly->b * li);
	}

	if (gShinyType)
	{
		ClipTestScaleRGBUVWHIntensity(v1, v2, VertexB.r, VertexB.g, VertexB.b, poly->s1, poly->t1, SHINE_U(poly->pn1), SHINE_V(poly->pn1),
			VertexC.r, VertexC.g, VertexC.b, poly->s2, poly->t2, SHINE_U(poly->pn2), SHINE_V(poly->pn2));
		ClipTestScaleRGBUVWHIntensity(v2, v0, VertexC.r, VertexC.g, VertexC.b, poly->s2, poly->t2, SHINE_U(poly->pn2), SHINE_V(poly->pn2),
			VertexA.r, VertexA.g, VertexA.b, poly->s0, poly->t0, SHINE_U(poly->pn0), SHINE_V(poly->pn0));
		ClipTestScaleRGBUVWHIntensity(v0, v1, VertexA.r, VertexA.g, VertexA.b, poly->s0, poly->t0, SHINE_U(poly->pn0), SHINE_V(poly->pn0),
			VertexB.r, VertexB.g, VertexB.b, poly->s1, poly->t1, SHINE_U(poly->pn1), SHINE_V(poly->pn1));
	}
	else
	{
		ClipTestScaleRGBUV(v1, v2, VertexB.r, VertexB.g, VertexB.b, poly->s1, poly->t1, VertexC.r, VertexC.g, VertexC.b, poly->s2, poly->t2);
		ClipTestScaleRGBUV(v2, v0, VertexC.r, VertexC.g, VertexC.b, poly->s2, poly->t2, VertexA.r, VertexA.g, VertexA.b, poly->s0, poly->t0);
		ClipTestScaleRGBUV(v0, v1, VertexA.r, VertexA.g, VertexA.b, poly->s0, poly->t0, VertexB.r, VertexB.g, VertexB.b, poly->s1, poly->t1);
	}

	count = ProjectClipPoly(scr, 0);

	if (count == 0)
		return;

	SetTexturedColorCombine();
	SetTextureSet(poly->texture_set);
	SetTransparency(trans);

	SetClipVertex(&VertexA, scr, 0, CLIP_RGB | CLIP_UV);

	if (count == 3)
	{
		SetClipVertex(&VertexB, scr, 1, CLIP_RGB | CLIP_UV);
		SetClipVertex(&VertexC, scr, 2, CLIP_RGB | CLIP_UV);

		if (gShinyType != 2)
			gDrawTriangleWithClip(&VertexA, &VertexB, &VertexC);

		if (gShinyType)
		{
			SetClipShine(&shine_vertex_a, &VertexA, scr, 0);
			SetClipShine(&shine_vertex_b, &VertexB, scr, 1);
			SetClipShine(&shine_vertex_c, &VertexC, scr, 2);

			AddCarShine(&shine_vertex_a, &shine_vertex_b, &shine_vertex_c, 1);
		}

		return;
	}

	SetClipVertex(&VertexB, scr, 2, CLIP_RGB | CLIP_UV);
	SetClipVertex(&VertexC, scr, 1, CLIP_RGB | CLIP_UV);

	if (gShinyType != 2)
		gDrawTriangleWithClip(&VertexA, &VertexB, &VertexC);

	if (gShinyType)
	{
		SetClipShine(&shine_vertex_a, &VertexA, scr, 0);
		SetClipShine(&shine_vertex_b, &VertexB, scr, 2);
		SetClipShine(&shine_vertex_c, &VertexC, scr, 1);

		AddCarShine(&shine_vertex_a, &shine_vertex_b, &shine_vertex_c, 1);
	}

	SetClipVertex(&VertexC, scr, 3, CLIP_RGB | CLIP_UV);

	if (gShinyType != 2)
		gDrawTriangleWithClip(&VertexA, &VertexB, &VertexC);

	if (gShinyType)
	{
		SetClipShine(&shine_vertex_c, &VertexC, scr, 3);
		AddCarShine(&shine_vertex_a, &shine_vertex_b, &shine_vertex_c, 1);
	}
}

// [0x4681e0] [D]
void d3dDrawClippedPolyGT4_LIT(int trans)
{
	POLYGT4LIT *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	VERTEX *v3;
	VERTEX scr[5];
	GrVertex light3;
	float li;
	int count;

	poly = (POLYGT4LIT *)polyblock;
	polyblock += sizeof(POLYGT4LIT);

	v0 = GetVertex(poly->v0, VertexRotate);
	v1 = GetVertex(poly->v1, VertexRotate);
	v2 = GetVertex(poly->v2, VertexRotate);
	v3 = GetVertex(poly->v3, VertexRotate);

	num_clip_poly_points = 0;
	clip_ptr = clip_poly_info;

	// NOTE: the original keeps v3's colour in unnamed globals (0xb9b59c ...)
	if (num_potential_lamps)
	{
		li = conv_point_normals[poly->pn0];
		AddLampLight(&VertexA, v0->vx, v0->vy, v0->vz, li * poly->r, li * poly->g, li * poly->b);

		li = conv_point_normals[poly->pn1];
		AddLampLight(&VertexB, v1->vx, v1->vy, v1->vz, li * poly->r, li * poly->g, li * poly->b);

		li = conv_point_normals[poly->pn2];
		AddLampLight(&VertexC, v2->vx, v2->vy, v2->vz, li * poly->r, li * poly->g, li * poly->b);

		li = conv_point_normals[poly->pn3];
		AddLampLight(&light3, v3->vx, v3->vy, v3->vz, li * poly->r, li * poly->g, li * poly->b);
	}
	else
	{
		li = conv_point_normals[poly->pn0];
		SetColor(&VertexA, poly->r * li, poly->g * li, poly->b * li);

		li = conv_point_normals[poly->pn1];
		SetColor(&VertexB, poly->r * li, poly->g * li, poly->b * li);

		li = conv_point_normals[poly->pn2];
		SetColor(&VertexC, poly->r * li, poly->g * li, poly->b * li);

		li = conv_point_normals[poly->pn3];
		SetColor(&light3, poly->r * li, poly->g * li, poly->b * li);
	}

	if (gShinyType)
	{
		ClipTestScaleRGBUVWHIntensity(v3, v0, light3.r, light3.g, light3.b, poly->s3, poly->t3, SHINE_U(poly->pn3), SHINE_V(poly->pn3),
			VertexA.r, VertexA.g, VertexA.b, poly->s0, poly->t0, SHINE_U(poly->pn0), SHINE_V(poly->pn0));
		ClipTestScaleRGBUVWHIntensity(v0, v1, VertexA.r, VertexA.g, VertexA.b, poly->s0, poly->t0, SHINE_U(poly->pn0), SHINE_V(poly->pn0),
			VertexB.r, VertexB.g, VertexB.b, poly->s1, poly->t1, SHINE_U(poly->pn1), SHINE_V(poly->pn1));
		ClipTestScaleRGBUVWHIntensity(v1, v2, VertexB.r, VertexB.g, VertexB.b, poly->s1, poly->t1, SHINE_U(poly->pn1), SHINE_V(poly->pn1),
			VertexC.r, VertexC.g, VertexC.b, poly->s2, poly->t2, SHINE_U(poly->pn2), SHINE_V(poly->pn2));
		ClipTestScaleRGBUVWHIntensity(v2, v3, VertexC.r, VertexC.g, VertexC.b, poly->s2, poly->t2, SHINE_U(poly->pn2), SHINE_V(poly->pn2),
			light3.r, light3.g, light3.b, poly->s3, poly->t3, SHINE_U(poly->pn3), SHINE_V(poly->pn3));
	}
	else
	{
		ClipTestScaleRGBUV(v3, v0, light3.r, light3.g, light3.b, poly->s3, poly->t3, VertexA.r, VertexA.g, VertexA.b, poly->s0, poly->t0);
		ClipTestScaleRGBUV(v0, v1, VertexA.r, VertexA.g, VertexA.b, poly->s0, poly->t0, VertexB.r, VertexB.g, VertexB.b, poly->s1, poly->t1);
		ClipTestScaleRGBUV(v1, v2, VertexB.r, VertexB.g, VertexB.b, poly->s1, poly->t1, VertexC.r, VertexC.g, VertexC.b, poly->s2, poly->t2);
		ClipTestScaleRGBUV(v2, v3, VertexC.r, VertexC.g, VertexC.b, poly->s2, poly->t2, light3.r, light3.g, light3.b, poly->s3, poly->t3);
	}

	count = ProjectClipPoly(scr, CLIP_QUAD);

	if (count == 0)
		return;

	SetTexturedColorCombine();
	SetTextureSet(poly->texture_set);
	SetTransparency(trans);

	SetClipVertex(&VertexA, scr, 0, CLIP_RGB | CLIP_UV);

	if (count == 3)
	{
		SetClipVertex(&VertexB, scr, 1, CLIP_RGB | CLIP_UV);
		SetClipVertex(&VertexC, scr, 2, CLIP_RGB | CLIP_UV);

		if (gShinyType != 2)
			gDrawTriangleWithClip(&VertexA, &VertexB, &VertexC);

		if (gShinyType)
		{
			SetClipShine(&shine_vertex_a, &VertexA, scr, 0);
			SetClipShine(&shine_vertex_b, &VertexB, scr, 1);
			SetClipShine(&shine_vertex_c, &VertexC, scr, 2);

			AddCarShine(&shine_vertex_a, &shine_vertex_b, &shine_vertex_c, 1);
		}

		return;
	}

	SetClipVertex(&VertexB, scr, 2, CLIP_RGB | CLIP_UV);
	SetClipVertex(&VertexC, scr, 1, CLIP_RGB | CLIP_UV);

	if (gShinyType != 2)
		gDrawTriangleWithClip(&VertexA, &VertexB, &VertexC);

	if (gShinyType)
	{
		SetClipShine(&shine_vertex_a, &VertexA, scr, 0);
		SetClipShine(&shine_vertex_b, &VertexB, scr, 2);
		SetClipShine(&shine_vertex_c, &VertexC, scr, 1);

		AddCarShine(&shine_vertex_a, &shine_vertex_b, &shine_vertex_c, 1);
	}

	SetClipVertex(&VertexC, scr, 3, CLIP_RGB | CLIP_UV);

	if (gShinyType != 2)
		gDrawTriangleWithClip(&VertexA, &VertexB, &VertexC);

	if (gShinyType)
	{
		SetClipShine(&shine_vertex_c, &VertexC, scr, 3);
		AddCarShine(&shine_vertex_a, &shine_vertex_b, &shine_vertex_c, 1);
	}

	if (count == 5)
	{
		SetClipVertex(&VertexB, scr, 4, CLIP_RGB | CLIP_UV);

		if (gShinyType != 2)
			gDrawTriangleWithClip(&VertexA, &VertexB, &VertexC);

		if (gShinyType)
		{
			SetClipShine(&shine_vertex_b, &VertexB, scr, 4);
			AddCarShine(&shine_vertex_a, &shine_vertex_b, &shine_vertex_c, 1);
		}
	}
}

// [0x46a5c0] [D]
// The far clipped polygons (the object crosses far_clip), as the near clipped ones.
void d3dDrawFarClippedPOLYF3(int trans)
{
	POLYF3 *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	VERTEX scr[5];
	int count;

	poly = (POLYF3 *)polyblock;
	polyblock += sizeof(POLYF3);

	v0 = GetVertex(poly->v0, VertexRotate);
	v1 = GetVertex(poly->v1, VertexRotate);
	v2 = GetVertex(poly->v2, VertexRotate);

	num_clip_poly_points = 0;
	clip_ptr = clip_poly_info;

	FarClipTestScale(v0, v1);
	FarClipTestScale(v1, v2);
	FarClipTestScale(v2, v0);

	count = ProjectClipPoly(scr, 0);

	if (count == 0)
		return;

	SetFlatColorCombine();
	SetTransparency(trans);

	SetColor(&VertexA, poly->r, poly->g, poly->b);
	SetColor(&VertexB, poly->r, poly->g, poly->b);
	SetColor(&VertexC, poly->r, poly->g, poly->b);

	DrawClipFan(scr, count, 0);
}

// [0x46ae95] [D]
void d3dDrawFarClippedPOLYF4(int trans)
{
	POLYF4 *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	VERTEX *v3;
	VERTEX scr[5];
	int count;

	poly = (POLYF4 *)polyblock;
	polyblock += sizeof(POLYF4);

	v0 = GetVertex(poly->v0, VertexRotate);
	v1 = GetVertex(poly->v1, VertexRotate);
	v2 = GetVertex(poly->v2, VertexRotate);
	v3 = GetVertex(poly->v3, VertexRotate);

	num_clip_poly_points = 0;
	clip_ptr = clip_poly_info;

	FarClipTestScale(v0, v1);
	FarClipTestScale(v1, v2);
	FarClipTestScale(v2, v3);
	FarClipTestScale(v3, v0);

	count = ProjectClipPoly(scr, CLIP_QUAD | CLIP_TEST5);

	if (count == 0)
		return;

	SetFlatColorCombine();
	SetTransparency(trans);

	SetColor(&VertexA, poly->r, poly->g, poly->b);
	SetColor(&VertexB, poly->r, poly->g, poly->b);
	SetColor(&VertexC, poly->r, poly->g, poly->b);

	DrawClipFan(scr, count, 0);
}

// [0x46bc60] [D]
void d3dDrawFarClippedPOLYF3_LIT(int trans)
{
	POLYF3LIT *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	VERTEX scr[5];
	float li;
	int count;

	poly = (POLYF3LIT *)polyblock;
	polyblock += sizeof(POLYF3LIT);

	v0 = GetVertex(poly->v0, VertexRotate);
	v1 = GetVertex(poly->v1, VertexRotate);
	v2 = GetVertex(poly->v2, VertexRotate);

	num_clip_poly_points = 0;
	clip_ptr = clip_poly_info;

	FarClipTestScale(v0, v1);
	FarClipTestScale(v1, v2);
	FarClipTestScale(v2, v0);

	count = ProjectClipPoly(scr, 0);

	if (count == 0)
		return;

	SetFlatColorCombine();
	SetTransparency(trans);

	li = light_intensity[poly->normal];

	SetColor(&VertexA, li * poly->r, li * poly->g, li * poly->b);
	SetColor(&VertexB, li * poly->r, li * poly->g, li * poly->b);
	SetColor(&VertexC, li * poly->r, li * poly->g, li * poly->b);

	DrawClipFan(scr, count, 0);
}

// [0x46c597] [D]
void d3dDrawFarClippedPOLYF4_LIT(int trans)
{
	POLYF4LIT *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	VERTEX *v3;
	VERTEX scr[5];
	float li;
	int count;

	poly = (POLYF4LIT *)polyblock;
	polyblock += sizeof(POLYF4LIT);

	v0 = GetVertex(poly->v0, VertexRotate);
	v1 = GetVertex(poly->v1, VertexRotate);
	v2 = GetVertex(poly->v2, VertexRotate);
	v3 = GetVertex(poly->v3, VertexRotate);

	num_clip_poly_points = 0;
	clip_ptr = clip_poly_info;

	FarClipTestScale(v0, v1);
	FarClipTestScale(v1, v2);
	FarClipTestScale(v2, v3);
	FarClipTestScale(v3, v0);

	count = ProjectClipPoly(scr, CLIP_QUAD);

	if (count == 0)
		return;

	SetFlatColorCombine();
	SetTransparency(trans);

	li = light_intensity[poly->normal];

	SetColor(&VertexA, li * poly->r, li * poly->g, li * poly->b);
	SetColor(&VertexB, li * poly->r, li * poly->g, li * poly->b);
	SetColor(&VertexC, li * poly->r, li * poly->g, li * poly->b);

	DrawClipFan(scr, count, 0);
}

// [0x46d411] [D]
void d3dDrawFarClippedPOLYFT3(int trans)
{
	POLYFT3 *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	VERTEX scr[5];
	int count;

	poly = (POLYFT3 *)polyblock;
	polyblock += sizeof(POLYFT3);

	v0 = GetVertex(poly->v0, VertexRotate);
	v1 = GetVertex(poly->v1, VertexRotate);
	v2 = GetVertex(poly->v2, VertexRotate);

	num_clip_poly_points = 0;
	clip_ptr = clip_poly_info;

	FarClipTestScaleUV(v0, v1, poly->s0, poly->t0, poly->s1, poly->t1);
	FarClipTestScaleUV(v1, v2, poly->s1, poly->t1, poly->s2, poly->t2);
	FarClipTestScaleUV(v2, v0, poly->s2, poly->t2, poly->s0, poly->t0);

	count = ProjectClipPoly(scr, 0);

	if (count == 0)
		return;

	SetTexturedColorCombine();
	SetTextureSet(poly->texture_set);
	SetTransparency(trans);

	SetColor(&VertexA, poly->r, poly->g, poly->b);
	SetColor(&VertexB, poly->r, poly->g, poly->b);
	SetColor(&VertexC, poly->r, poly->g, poly->b);

	DrawClipFan(scr, count, CLIP_UV);
}

// [0x46de9a] [D]
void d3dDrawFarClippedPOLYFT4(int trans)
{
	POLYFT4 *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	VERTEX *v3;
	VERTEX scr[5];
	int count;

	poly = (POLYFT4 *)polyblock;
	polyblock += sizeof(POLYFT4);

	v0 = GetVertex(poly->v0, VertexRotate);
	v1 = GetVertex(poly->v1, VertexRotate);
	v2 = GetVertex(poly->v2, VertexRotate);
	v3 = GetVertex(poly->v3, VertexRotate);

	num_clip_poly_points = 0;
	clip_ptr = clip_poly_info;

	FarClipTestScaleUV(v0, v1, poly->s0, poly->t0, poly->s1, poly->t1);
	FarClipTestScaleUV(v1, v2, poly->s1, poly->t1, poly->s2, poly->t2);
	FarClipTestScaleUV(v2, v3, poly->s2, poly->t2, poly->s3, poly->t3);
	FarClipTestScaleUV(v3, v0, poly->s3, poly->t3, poly->s0, poly->t0);

	count = ProjectClipPoly(scr, CLIP_QUAD);

	if (count == 0)
		return;

	SetTexturedColorCombine();
	SetTextureSet(poly->texture_set);
	SetTransparency(trans);

	SetColor(&VertexA, poly->r, poly->g, poly->b);
	SetColor(&VertexB, poly->r, poly->g, poly->b);
	SetColor(&VertexC, poly->r, poly->g, poly->b);

	DrawClipFan(scr, count, CLIP_UV);
}

// [0x46ef07] [D]
void d3dDrawFarClippedPOLYGT3(int trans)
{
	POLYGT3 *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	VERTEX scr[5];
	int count;

	poly = (POLYGT3 *)polyblock;
	polyblock += sizeof(POLYGT3);

	v0 = GetVertex(poly->v0, VertexRotate);
	v1 = GetVertex(poly->v1, VertexRotate);
	v2 = GetVertex(poly->v2, VertexRotate);

	num_clip_poly_points = 0;
	clip_ptr = clip_poly_info;

	FarClipTestScaleRGBUV(v1, v2, poly->r1, poly->g1, poly->b1, poly->s1, poly->t1, poly->r2, poly->g2, poly->b2, poly->s2, poly->t2);
	FarClipTestScaleRGBUV(v2, v0, poly->r2, poly->g2, poly->b2, poly->s2, poly->t2, poly->r0, poly->g0, poly->b0, poly->s0, poly->t0);
	FarClipTestScaleRGBUV(v0, v1, poly->r0, poly->g0, poly->b0, poly->s0, poly->t0, poly->r1, poly->g1, poly->b1, poly->s1, poly->t1);

	count = ProjectClipPoly(scr, 0);

	if (count == 0)
		return;

	SetTexturedColorCombine();
	SetTextureSet(poly->texture_set);
	SetTransparency(trans);

	DrawClipFan(scr, count, CLIP_RGB | CLIP_UV | CLIP_RGB_BUG);
}

// [0x46fad0] [D]
void d3dDrawFarClippedPOLYGT4(int trans)
{
	POLYGT4 *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	VERTEX *v3;
	VERTEX scr[5];
	int count;

	poly = (POLYGT4 *)polyblock;
	polyblock += sizeof(POLYGT4);

	v0 = GetVertex(poly->v0, VertexRotate);
	v1 = GetVertex(poly->v1, VertexRotate);
	v2 = GetVertex(poly->v2, VertexRotate);
	v3 = GetVertex(poly->v3, VertexRotate);

	num_clip_poly_points = 0;
	clip_ptr = clip_poly_info;

	FarClipTestScaleRGBUV(v3, v0, poly->r3, poly->g3, poly->b3, poly->s3, poly->t3, poly->r0, poly->g0, poly->b0, poly->s0, poly->t0);
	FarClipTestScaleRGBUV(v0, v1, poly->r0, poly->g0, poly->b0, poly->s0, poly->t0, poly->r1, poly->g1, poly->b1, poly->s1, poly->t1);
	FarClipTestScaleRGBUV(v1, v2, poly->r1, poly->g1, poly->b1, poly->s1, poly->t1, poly->r2, poly->g2, poly->b2, poly->s2, poly->t2);
	FarClipTestScaleRGBUV(v2, v3, poly->r2, poly->g2, poly->b2, poly->s2, poly->t2, poly->r3, poly->g3, poly->b3, poly->s3, poly->t3);

	count = ProjectClipPoly(scr, CLIP_QUAD | CLIP_TEST5);

	if (count == 0)
		return;

	SetTexturedColorCombine();
	SetTextureSet(poly->texture_set);
	SetTransparency(trans);

	DrawClipFan(scr, count, CLIP_RGB | CLIP_UV);
}

// [0x470dd6] [D]
void d3dDrawFarClippedPOLYFT3_LIT(int trans)
{
	POLYFT3LIT *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	VERTEX scr[5];
	float li;
	int count;

	poly = (POLYFT3LIT *)polyblock;
	polyblock += sizeof(POLYFT3LIT);

	v0 = GetVertex(poly->v0, VertexRotate);
	v1 = GetVertex(poly->v1, VertexRotate);
	v2 = GetVertex(poly->v2, VertexRotate);

	num_clip_poly_points = 0;
	clip_ptr = clip_poly_info;

	FarClipTestScaleUV(v0, v1, poly->s0, poly->t0, poly->s1, poly->t1);
	FarClipTestScaleUV(v1, v2, poly->s1, poly->t1, poly->s2, poly->t2);
	FarClipTestScaleUV(v2, v0, poly->s2, poly->t2, poly->s0, poly->t0);

	count = ProjectClipPoly(scr, 0);

	if (count == 0)
		return;

	SetTexturedColorCombine();
	SetTextureSet(poly->texture_set);
	SetTransparency(trans);

	li = light_intensity[poly->normal];

	SetColor(&VertexA, li * poly->r, li * poly->g, li * poly->b);
	SetColor(&VertexB, li * poly->r, li * poly->g, li * poly->b);
	SetColor(&VertexC, li * poly->r, li * poly->g, li * poly->b);

	DrawClipFan(scr, count, CLIP_UV);
}

// [0x4718c1] [D]
void d3dDrawFarClippedPOLYFT4_LIT(int trans)
{
	POLYFT4LIT *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	VERTEX *v3;
	VERTEX scr[5];
	float li;
	int count;

	poly = (POLYFT4LIT *)polyblock;
	polyblock += sizeof(POLYFT4LIT);

	v0 = GetVertex(poly->v0, VertexRotate);
	v1 = GetVertex(poly->v1, VertexRotate);
	v2 = GetVertex(poly->v2, VertexRotate);
	v3 = GetVertex(poly->v3, VertexRotate);

	num_clip_poly_points = 0;
	clip_ptr = clip_poly_info;

	FarClipTestScaleUV(v0, v1, poly->s0, poly->t0, poly->s1, poly->t1);
	FarClipTestScaleUV(v1, v2, poly->s1, poly->t1, poly->s2, poly->t2);
	FarClipTestScaleUV(v2, v3, poly->s2, poly->t2, poly->s3, poly->t3);
	FarClipTestScaleUV(v3, v0, poly->s3, poly->t3, poly->s0, poly->t0);

	count = ProjectClipPoly(scr, CLIP_QUAD);

	if (count == 0)
		return;

	SetTexturedColorCombine();
	SetTextureSet(poly->texture_set);
	SetTransparency(trans);

	li = light_intensity[poly->normal];

	SetColor(&VertexA, li * poly->r, li * poly->g, li * poly->b);
	SetColor(&VertexB, li * poly->r, li * poly->g, li * poly->b);
	SetColor(&VertexC, li * poly->r, li * poly->g, li * poly->b);

	DrawClipFan(scr, count, CLIP_UV);
}

// [0x4729f1] [D]
void d3dDrawFarClippedPOLYGT3_LIT(int trans)
{
	POLYGT3LIT *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	VERTEX scr[5];
	float li;
	int count;

	poly = (POLYGT3LIT *)polyblock;
	polyblock += sizeof(POLYGT3LIT);

	v0 = GetVertex(poly->v0, VertexRotate);
	v1 = GetVertex(poly->v1, VertexRotate);
	v2 = GetVertex(poly->v2, VertexRotate);

	num_clip_poly_points = 0;
	clip_ptr = clip_poly_info;

	if (num_potential_lamps)
	{
		li = conv_point_normals[poly->pn0];
		AddLampLight(&VertexA, v0->vx, v0->vy, v0->vz, li * poly->r, li * poly->g, li * poly->b);

		li = conv_point_normals[poly->pn1];
		AddLampLight(&VertexB, v1->vx, v1->vy, v1->vz, li * poly->r, li * poly->g, li * poly->b);

		li = conv_point_normals[poly->pn2];
		AddLampLight(&VertexC, v2->vx, v2->vy, v2->vz, li * poly->r, li * poly->g, li * poly->b);
	}
	else
	{
		li = conv_point_normals[poly->pn0];
		SetColor(&VertexA, poly->r * li, poly->g * li, poly->b * li);

		li = conv_point_normals[poly->pn1];
		SetColor(&VertexB, poly->r * li, poly->g * li, poly->b * li);

		li = conv_point_normals[poly->pn2];
		SetColor(&VertexC, poly->r * li, poly->g * li, poly->b * li);
	}

	FarClipTestScaleRGBUV(v1, v2, VertexB.r, VertexB.g, VertexB.b, poly->s1, poly->t1, VertexC.r, VertexC.g, VertexC.b, poly->s2, poly->t2);
	FarClipTestScaleRGBUV(v2, v0, VertexC.r, VertexC.g, VertexC.b, poly->s2, poly->t2, VertexA.r, VertexA.g, VertexA.b, poly->s0, poly->t0);
	FarClipTestScaleRGBUV(v0, v1, VertexA.r, VertexA.g, VertexA.b, poly->s0, poly->t0, VertexB.r, VertexB.g, VertexB.b, poly->s1, poly->t1);

	count = ProjectClipPoly(scr, 0);

	if (count == 0)
		return;

	SetTexturedColorCombine();
	SetTextureSet(poly->texture_set);
	SetTransparency(trans);

	DrawClipFan(scr, count, CLIP_RGB | CLIP_UV);
}

// [0x473d34] [D]
void d3dDrawFarClippedPOLYGT4_LIT(int trans)
{
	POLYGT4LIT *poly;
	VERTEX *v0;
	VERTEX *v1;
	VERTEX *v2;
	VERTEX *v3;
	VERTEX scr[5];
	GrVertex light3;
	float li;
	int count;

	poly = (POLYGT4LIT *)polyblock;
	polyblock += sizeof(POLYGT4LIT);

	v0 = GetVertex(poly->v0, VertexRotate);
	v1 = GetVertex(poly->v1, VertexRotate);
	v2 = GetVertex(poly->v2, VertexRotate);
	v3 = GetVertex(poly->v3, VertexRotate);

	num_clip_poly_points = 0;
	clip_ptr = clip_poly_info;

	// NOTE: the original keeps v3's colour in unnamed globals (0xb9b59c ...)
	if (num_potential_lamps)
	{
		li = conv_point_normals[poly->pn0];
		AddLampLight(&VertexA, v0->vx, v0->vy, v0->vz, li * poly->r, li * poly->g, li * poly->b);

		li = conv_point_normals[poly->pn1];
		AddLampLight(&VertexB, v1->vx, v1->vy, v1->vz, li * poly->r, li * poly->g, li * poly->b);

		li = conv_point_normals[poly->pn2];
		AddLampLight(&VertexC, v2->vx, v2->vy, v2->vz, li * poly->r, li * poly->g, li * poly->b);

		li = conv_point_normals[poly->pn3];
		AddLampLight(&light3, v3->vx, v3->vy, v3->vz, li * poly->r, li * poly->g, li * poly->b);
	}
	else
	{
		li = conv_point_normals[poly->pn0];
		SetColor(&VertexA, poly->r * li, poly->g * li, poly->b * li);

		li = conv_point_normals[poly->pn1];
		SetColor(&VertexB, poly->r * li, poly->g * li, poly->b * li);

		li = conv_point_normals[poly->pn2];
		SetColor(&VertexC, poly->r * li, poly->g * li, poly->b * li);

		li = conv_point_normals[poly->pn3];
		SetColor(&light3, poly->r * li, poly->g * li, poly->b * li);
	}

	// NOTE: clipped against the near plane, not the far one (the original's)
	ClipTestScaleRGBUV(v3, v0, light3.r, light3.g, light3.b, poly->s3, poly->t3, VertexA.r, VertexA.g, VertexA.b, poly->s0, poly->t0);
	ClipTestScaleRGBUV(v0, v1, VertexA.r, VertexA.g, VertexA.b, poly->s0, poly->t0, VertexB.r, VertexB.g, VertexB.b, poly->s1, poly->t1);
	ClipTestScaleRGBUV(v1, v2, VertexB.r, VertexB.g, VertexB.b, poly->s1, poly->t1, VertexC.r, VertexC.g, VertexC.b, poly->s2, poly->t2);
	ClipTestScaleRGBUV(v2, v3, VertexC.r, VertexC.g, VertexC.b, poly->s2, poly->t2, light3.r, light3.g, light3.b, poly->s3, poly->t3);

	count = ProjectClipPoly(scr, CLIP_QUAD);

	if (count == 0)
		return;

	SetTexturedColorCombine();
	SetTextureSet(poly->texture_set);
	SetTransparency(trans);

	DrawClipFan(scr, count, CLIP_RGB | CLIP_UV);
}

// [0x475b7b] [D]
// NOTE: no name in the database. The axis a polygon normal points along: 1 / 2 -x / +x,
// 5 / 6 -y / +y, 4 / 3 -z / +z; 0 when it is not axis aligned.
int sub_475B7B(VECTOR4 *normal)
{
	float v;

	num_axis_alligned++;

	if (normal->vx < 0.0f)
	{
		v = (float)fabs(normal->vx);

		if (v > AXIS_EPSILON_LOW && v < AXIS_EPSILON_HIGH)
			return 1;
	}
	else if (normal->vx > AXIS_EPSILON_LOW && normal->vx < AXIS_EPSILON_HIGH)
	{
		return 2;
	}

	if (normal->vy < 0.0f)
	{
		v = (float)fabs(normal->vy);

		if (v > AXIS_EPSILON_LOW && v < AXIS_EPSILON_HIGH)
			return 5;
	}
	else
	{
		v = normal->vy;

		if (v > AXIS_EPSILON_LOW && v < AXIS_EPSILON_HIGH)
			return 6;
	}

	if (normal->vz < 0.0f)
	{
		v = (float)fabs(normal->vz);

		if (v > AXIS_EPSILON_LOW && v < AXIS_EPSILON_HIGH)
			return 4;
	}
	else
	{
		v = normal->vz;

		if (v > AXIS_EPSILON_LOW && v < AXIS_EPSILON_HIGH)
			return 3;
	}

	num_axis_alligned--;
	num_none_axis_alligned++;

	return 0;
}

// [0x475d42] [D]
// Stores the axis of each polygon normal in the polygon's flags byte; a model whose polygons
// are all axis aligned gets both axis flags (DrawObjectOSUseAxisChecks), partly aligned AXIS2.
void SetAxisBits(MODEL *model)
{
	unsigned char *poly_block;
	VECTOR4 *normals;
	int i;
	unsigned char type;

	normals = (VECTOR4 *)model->normals;

	num_none_axis_alligned = 0;
	num_axis_alligned = 0;

	poly_block = (unsigned char *)model->poly_block;

	for (i = 0; i < (unsigned short)model->num_polys; i++)
	{
		type = *poly_block;

		poly_block[1] = sub_475B7B(normals);
		poly_block += PolySizes[type];

		normals++;
	}

	if ((unsigned short)model->num_polys == num_axis_alligned)
	{
		model->shape_flags |= SHAPE_FLAG_AXIS2 | SHAPE_FLAG_AXIS1;
		total_axis_alligned_models++;
	}
	else if ((unsigned short)model->num_polys == num_none_axis_alligned)
	{
		total_none_axis_alligned_models++;
	}
	else
	{
		model->shape_flags |= SHAPE_FLAG_AXIS2;
		total_partial_axis_alligned_models++;
	}

	total_axis_alligned += num_axis_alligned;
	total_none_axis_alligned += num_none_axis_alligned;
}

// [0x475e76] [D]
// Points each polygon at the first polygon with the same normal (the int at +4); the first one
// of each normal gets flag 8. Returns the number of duplicates.
int CalcDuplicateNormals(MODEL *model)
{
	VECTOR4 *normal;
	int duplicates;
	char *poly_block;
	int i;
	VECTOR4 *normals;
	int j;

	duplicates = 0;

	poly_block = (char *)model->poly_block;

	for (i = 0; i < (unsigned short)model->num_polys; i++)
	{
		normals = (VECTOR4 *)model->normals;
		normal = &normals[i];

		for (j = 0; j < (unsigned short)model->num_polys; j++)
		{
			if (normals->vx == normal->vx && normals->vy == normal->vy && normals->vz == normal->vz && normals->vw == normal->vw)
			{
				*(int *)(poly_block + 4) = j;

				if (j == i)
					poly_block[1] |= 8;
				else
					duplicates++;

				break;
			}

			normals++;
		}

		poly_block += PolySizes[*(unsigned char *)poly_block];
	}

	return duplicates;
}

// [0x475fad] [D]
// Converts a model of the level file (short indices, byte colours and uvs) into the drawing
// format (int indices, float colours and uvs) in out; returns its size. Offsets are relative
// to the model until RelocateModel. The pointers of in are relocated in place.
// NOTE: Hex-Rays output is broken here (marked [FUCKED] in the database); GT3 and GT4 are
// written from the byte offsets it shows, which match OUT_POLYGT3 / OUT_POLYGT4.
int ConvertModelToInternalFormat(MODEL *in, MODEL *out)
{
	POLYF3 *f3;
	POLYF4 *f4;
	POLYG3 *g3;
	POLYG4 *g4;
	POLYFT3 *ft3;
	POLYFT4 *ft4;
	POLYGT3 *gt3;
	POLYGT4 *gt4;
	POLYF3LIT *f3_l;
	POLYF4LIT *f4_l;
	POLYG3LIT *g3_l;
	POLYG4LIT *g4_l;
	POLYFT3LIT *ft3_l;
	POLYFT4LIT *ft4_l;
	POLYGT3LIT *gt3_l;
	POLYGT4LIT *gt4_l;
	int numcb;
	VECTOR *point_normals;
	VECTOR4 *point_normals_out;
	VECTOR *vertices;
	VECTOR4 *vertices_out;
	int size;
	POLY_PTR poly;
	char *dst;
	int i;
	unsigned char polytype;

	size = sizeof(MODEL);

	CopyMem(out, in, sizeof(MODEL));

	in->vertices += (int)in;
	in->normals += (int)in;
	in->point_normals += (int)in;
	in->poly_block += (int)in;
	in->texture_set_info += (int)in;

	if (in->collision_block)
		in->collision_block += (int)in;

	// 1. vertex data (an instance uses its parent's vertices and normals)
	if (in->instance_number == -1)
	{
		if (in->num_vertices)
		{
			vertices = (VECTOR *)in->vertices;
			vertices_out = (VECTOR4 *)(out + 1);

			for (i = 0; i < (unsigned short)in->num_vertices; i++)
			{
				vertices_out->vx = vertices->vx;
				vertices_out->vy = vertices->vy;
				vertices_out->vz = vertices->vz;
				vertices_out->vw = 1.0f;

				vertices++;
				vertices_out++;
			}
		}

		out->vertices = sizeof(MODEL);
		size = sizeof(MODEL) + (unsigned short)in->num_vertices * sizeof(VECTOR4);
	}

	// 2. normals
	if (in->instance_number == -1)
	{
		if (in->num_polys)
			CopyMem((char *)out + size, (void *)in->normals, (unsigned short)in->num_polys * sizeof(VECTOR4));

		out->normals = size;
		size += (unsigned short)in->num_polys * sizeof(VECTOR4);
	}

	// 3. vertex normals
	if (in->num_point_normals)
	{
		point_normals = (VECTOR *)in->point_normals;
		point_normals_out = (VECTOR4 *)((char *)out + size);

		for (i = 0; i < (unsigned short)in->num_point_normals; i++)
		{
			point_normals_out->vx = point_normals->vx;
			point_normals_out->vy = point_normals->vy;
			point_normals_out->vz = point_normals->vz;
			point_normals_out->vw = 1.0f;

			point_normals++;
			point_normals_out++;
		}
	}

	out->point_normals = size;
	size += (unsigned short)in->num_point_normals * sizeof(VECTOR4);

	// 4. textures
	if (in->num_texture_sets)
		CopyMem((char *)out + size, (void *)in->texture_set_info, (unsigned short)in->num_texture_sets);

	out->texture_set_info = size;
	size += ((unsigned short)in->num_texture_sets + 3) & ~3;

	// 5. collision box
	if (in->instance_number == -1)
	{
		if (in->collision_block)
		{
			numcb = *(int *)in->collision_block;
			CopyMem((char *)out + size, (void *)in->collision_block, numcb * sizeof(COLLISION_PACKET) + 4);

			out->collision_block = size;
			size += numcb * sizeof(COLLISION_PACKET) + 4;
		}
		else
		{
			// no collision data!
			out->collision_block = 0;
		}
	}

	// 6. polys
	poly.p = (char *)in->poly_block;
	out->poly_block = size;

	for (i = 0; i < (unsigned short)in->num_polys; i++)
	{
		polytype = *poly.p;
		dst = (char *)out + size;

		// only the type byte is written (by each case in the binary: the types with a size); the
		// flags byte is filled by SetAxisBits
		if (PolySizes[polytype])
			*dst = polytype;

		switch (polytype)
		{
			case FLAT3:
			case FLAT3_T:
				f3 = (POLYF3 *)dst;
				f3->v0 = (unsigned short)poly.f3->v0;
				f3->v1 = (unsigned short)poly.f3->v1;
				f3->v2 = (unsigned short)poly.f3->v2;
				f3->r = (float)poly.f3->color.r;
				f3->g = (float)poly.f3->color.g;
				f3->b = (float)poly.f3->color.b;
				break;
			case FLAT4:
			case FLAT4_T:
				f4 = (POLYF4 *)dst;
				f4->v0 = (unsigned short)poly.f4->v0;
				f4->v1 = (unsigned short)poly.f4->v1;
				f4->v2 = (unsigned short)poly.f4->v2;
				f4->v3 = (unsigned short)poly.f4->v3;
				f4->r = (float)poly.f4->color.r;
				f4->g = (float)poly.f4->color.g;
				f4->b = (float)poly.f4->color.b;
				break;
			case GOUR3:
			case GOUR3_T:
				g3 = (POLYG3 *)dst;
				g3->v0 = (unsigned short)poly.g3->v0;
				g3->v1 = (unsigned short)poly.g3->v1;
				g3->v2 = (unsigned short)poly.g3->v2;
				g3->r0 = (float)poly.g3->c0.r;
				g3->g0 = (float)poly.g3->c0.g;
				g3->b0 = (float)poly.g3->c0.b;
				g3->r1 = (float)poly.g3->c1.r;
				g3->g1 = (float)poly.g3->c1.g;
				g3->b1 = (float)poly.g3->c1.b;
				g3->r2 = (float)poly.g3->c2.r;
				g3->g2 = (float)poly.g3->c2.g;
				g3->b2 = (float)poly.g3->c2.b;
				break;
			case GOUR4:
			case GOUR4_T:
				g4 = (POLYG4 *)dst;
				g4->v0 = (unsigned short)poly.g4->v0;
				g4->v1 = (unsigned short)poly.g4->v1;
				g4->v2 = (unsigned short)poly.g4->v2;
				g4->v3 = (unsigned short)poly.g4->v3;
				g4->r0 = (float)poly.g4->c0.r;
				g4->g0 = (float)poly.g4->c0.g;
				g4->b0 = (float)poly.g4->c0.b;
				g4->r1 = (float)poly.g4->c1.r;
				g4->g1 = (float)poly.g4->c1.g;
				g4->b1 = (float)poly.g4->c1.b;
				g4->r2 = (float)poly.g4->c2.r;
				g4->g2 = (float)poly.g4->c2.g;
				g4->b2 = (float)poly.g4->c2.b;
				g4->r3 = (float)poly.g4->c3.r;
				g4->g3 = (float)poly.g4->c3.g;
				g4->b3 = (float)poly.g4->c3.b;
				break;
			case TEXT3:
			case TEXT3_T:
				ft3 = (POLYFT3 *)dst;
				ft3->texture_set = poly.ft3->texture_set;
				ft3->v0 = (unsigned short)poly.ft3->v0;
				ft3->v1 = (unsigned short)poly.ft3->v1;
				ft3->v2 = (unsigned short)poly.ft3->v2;
				ft3->s0 = (float)poly.ft3->uv0.u;
				ft3->t0 = (float)poly.ft3->uv0.v;
				ft3->s1 = (float)poly.ft3->uv1.u;
				ft3->t1 = (float)poly.ft3->uv1.v;
				ft3->s2 = (float)poly.ft3->uv2.u;
				ft3->t2 = (float)poly.ft3->uv2.v;
				ft3->r = (float)poly.ft3->color.r;
				ft3->g = (float)poly.ft3->color.g;
				ft3->b = (float)poly.ft3->color.b;
				break;
			case TEXT4:
			case TEXT4_T:
				ft4 = (POLYFT4 *)dst;
				ft4->texture_set = poly.ft4->texture_set;
				ft4->v0 = (unsigned short)poly.ft4->v0;
				ft4->v1 = (unsigned short)poly.ft4->v1;
				ft4->v2 = (unsigned short)poly.ft4->v2;
				ft4->v3 = (unsigned short)poly.ft4->v3;
				ft4->s0 = (float)poly.ft4->uv0.u;
				ft4->t0 = (float)poly.ft4->uv0.v;
				ft4->s1 = (float)poly.ft4->uv1.u;
				ft4->t1 = (float)poly.ft4->uv1.v;
				ft4->s2 = (float)poly.ft4->uv2.u;
				ft4->t2 = (float)poly.ft4->uv2.v;
				ft4->s3 = (float)poly.ft4->uv3.u;
				ft4->t3 = (float)poly.ft4->uv3.v;
				ft4->r = (float)poly.ft4->color.r;
				ft4->g = (float)poly.ft4->color.g;
				ft4->b = (float)poly.ft4->color.b;
				break;
			case GOURTEXT3:
			case GOURTEXT3_T:
				gt3 = (POLYGT3 *)dst;
				gt3->texture_set = poly.gt3->texture_set;
				gt3->v0 = poly.gt3->v0;
				gt3->v1 = poly.gt3->v1;
				gt3->v2 = poly.gt3->v2;
				gt3->s0 = (float)poly.gt3->uv0.u;
				gt3->t0 = (float)poly.gt3->uv0.v;
				gt3->s1 = (float)poly.gt3->uv1.u;
				gt3->t1 = (float)poly.gt3->uv1.v;
				gt3->s2 = (float)poly.gt3->uv2.u;
				gt3->t2 = (float)poly.gt3->uv2.v;
				gt3->r0 = (float)poly.gt3->c0.r;
				gt3->g0 = (float)poly.gt3->c0.g;
				gt3->b0 = (float)poly.gt3->c0.b;
				gt3->r1 = (float)poly.gt3->c1.r;
				gt3->g1 = (float)poly.gt3->c1.g;
				gt3->b1 = (float)poly.gt3->c1.b;
				gt3->r2 = (float)poly.gt3->c2.r;
				gt3->g2 = (float)poly.gt3->c2.g;
				gt3->b2 = (float)poly.gt3->c2.b;
				break;
			case GOURTEXT4:
			case GOURTEXT4_T:
				gt4 = (POLYGT4 *)dst;
				gt4->texture_set = poly.gt4->texture_set;
				gt4->v0 = poly.gt4->v0;
				gt4->v1 = poly.gt4->v1;
				gt4->v2 = poly.gt4->v2;
				gt4->v3 = poly.gt4->v3;
				gt4->s0 = (float)poly.gt4->uv0.u;
				gt4->t0 = (float)poly.gt4->uv0.v;
				gt4->s1 = (float)poly.gt4->uv1.u;
				gt4->t1 = (float)poly.gt4->uv1.v;
				gt4->s2 = (float)poly.gt4->uv2.u;
				gt4->t2 = (float)poly.gt4->uv2.v;
				gt4->s3 = (float)poly.gt4->uv3.u;
				gt4->t3 = (float)poly.gt4->uv3.v;
				gt4->r0 = (float)poly.gt4->c0.r;
				gt4->g0 = (float)poly.gt4->c0.g;
				gt4->b0 = (float)poly.gt4->c0.b;
				gt4->r1 = (float)poly.gt4->c1.r;
				gt4->g1 = (float)poly.gt4->c1.g;
				gt4->b1 = (float)poly.gt4->c1.b;
				gt4->r2 = (float)poly.gt4->c2.r;
				gt4->g2 = (float)poly.gt4->c2.g;
				gt4->b2 = (float)poly.gt4->c2.b;
				gt4->r3 = (float)poly.gt4->c3.r;
				gt4->g3 = (float)poly.gt4->c3.g;
				gt4->b3 = (float)poly.gt4->c3.b;
				break;
			case FLAT3LIT:
			case FLAT3LIT_T:
				f3_l = (POLYF3LIT *)dst;
				f3_l->v0 = (unsigned short)poly.f3l->v0;
				f3_l->v1 = (unsigned short)poly.f3l->v1;
				f3_l->v2 = (unsigned short)poly.f3l->v2;
				f3_l->normal = (unsigned short)poly.f3l->normal;
				f3_l->r = (float)poly.f3l->color.r;
				f3_l->g = (float)poly.f3l->color.g;
				f3_l->b = (float)poly.f3l->color.b;
				break;
			case FLAT4LIT:
			case FLAT4LIT_T:
				f4_l = (POLYF4LIT *)dst;
				f4_l->v0 = (unsigned short)poly.f4l->v0;
				f4_l->v1 = (unsigned short)poly.f4l->v1;
				f4_l->v2 = (unsigned short)poly.f4l->v2;
				f4_l->v3 = (unsigned short)poly.f4l->v3;
				f4_l->normal = (unsigned short)poly.f4l->normal;
				f4_l->r = (float)poly.f4l->color.r;
				f4_l->g = (float)poly.f4l->color.g;
				f4_l->b = (float)poly.f4l->color.b;
				break;
			case GOUR3LIT:
			case GOUR3LIT_T:
				g3_l = (POLYG3LIT *)dst;
				g3_l->v0 = (unsigned short)poly.g3l->v0;
				g3_l->v1 = (unsigned short)poly.g3l->v1;
				g3_l->v2 = (unsigned short)poly.g3l->v2;
				g3_l->pn0 = (unsigned short)poly.g3l->pn0;
				g3_l->pn1 = (unsigned short)poly.g3l->pn1;
				g3_l->pn2 = (unsigned short)poly.g3l->pn2;
				g3_l->r = (float)poly.g3l->color.r;
				g3_l->g = (float)poly.g3l->color.g;
				g3_l->b = (float)poly.g3l->color.b;
				break;
			case GOUR4LIT:
			case GOUR4LIT_T:
				g4_l = (POLYG4LIT *)dst;
				g4_l->v0 = (unsigned short)poly.g4l->v0;
				g4_l->v1 = (unsigned short)poly.g4l->v1;
				g4_l->v2 = (unsigned short)poly.g4l->v2;
				g4_l->v3 = (unsigned short)poly.g4l->v3;
				g4_l->pn0 = (unsigned short)poly.g4l->pn0;
				g4_l->pn1 = (unsigned short)poly.g4l->pn1;
				g4_l->pn2 = (unsigned short)poly.g4l->pn2;
				g4_l->pn3 = (unsigned short)poly.g4l->pn3;
				g4_l->r = (float)poly.g4l->color.r;
				g4_l->g = (float)poly.g4l->color.g;
				g4_l->b = (float)poly.g4l->color.b;
				break;
			case TEXT3LIT:
			case TEXT3LIT_T:
				ft3_l = (POLYFT3LIT *)dst;
				ft3_l->texture_set = poly.ft3l->texture_set;
				ft3_l->v0 = (unsigned short)poly.ft3l->v0;
				ft3_l->v1 = (unsigned short)poly.ft3l->v1;
				ft3_l->v2 = (unsigned short)poly.ft3l->v2;
				ft3_l->normal = (unsigned short)poly.ft3l->normal;
				ft3_l->s0 = (float)poly.ft3l->uv0.u;
				ft3_l->t0 = (float)poly.ft3l->uv0.v;
				ft3_l->s1 = (float)poly.ft3l->uv1.u;
				ft3_l->t1 = (float)poly.ft3l->uv1.v;
				ft3_l->s2 = (float)poly.ft3l->uv2.u;
				ft3_l->t2 = (float)poly.ft3l->uv2.v;
				ft3_l->r = (float)poly.ft3l->color.r;
				ft3_l->g = (float)poly.ft3l->color.g;
				ft3_l->b = (float)poly.ft3l->color.b;
				break;
			case TEXT4LIT:
			case TEXT4LIT_T:
				ft4_l = (POLYFT4LIT *)dst;
				ft4_l->texture_set = poly.ft4l->texture_set;
				ft4_l->v0 = (unsigned short)poly.ft4l->v0;
				ft4_l->v1 = (unsigned short)poly.ft4l->v1;
				ft4_l->v2 = (unsigned short)poly.ft4l->v2;
				ft4_l->v3 = (unsigned short)poly.ft4l->v3;
				ft4_l->normal = (unsigned short)poly.ft4l->normal;
				ft4_l->s0 = (float)poly.ft4l->uv0.u;
				ft4_l->t0 = (float)poly.ft4l->uv0.v;
				ft4_l->s1 = (float)poly.ft4l->uv1.u;
				ft4_l->t1 = (float)poly.ft4l->uv1.v;
				ft4_l->s2 = (float)poly.ft4l->uv2.u;
				ft4_l->t2 = (float)poly.ft4l->uv2.v;
				ft4_l->s3 = (float)poly.ft4l->uv3.u;
				ft4_l->t3 = (float)poly.ft4l->uv3.v;
				ft4_l->r = (float)poly.ft4l->color.r;
				ft4_l->g = (float)poly.ft4l->color.g;
				ft4_l->b = (float)poly.ft4l->color.b;
				break;
			case GOURTEXT3LIT:
			case GOURTEXT3LIT_T:
				gt3_l = (POLYGT3LIT *)dst;
				gt3_l->texture_set = poly.gt3l->texture_set;
				gt3_l->v0 = (unsigned short)poly.gt3l->v0;
				gt3_l->v1 = (unsigned short)poly.gt3l->v1;
				gt3_l->v2 = (unsigned short)poly.gt3l->v2;
				gt3_l->pn0 = (unsigned short)poly.gt3l->pn0;
				gt3_l->pn1 = (unsigned short)poly.gt3l->pn1;
				gt3_l->pn2 = (unsigned short)poly.gt3l->pn2;
				gt3_l->s0 = (float)poly.gt3l->uv0.u;
				gt3_l->t0 = (float)poly.gt3l->uv0.v;
				gt3_l->s1 = (float)poly.gt3l->uv1.u;
				gt3_l->t1 = (float)poly.gt3l->uv1.v;
				gt3_l->s2 = (float)poly.gt3l->uv2.u;
				gt3_l->t2 = (float)poly.gt3l->uv2.v;
				gt3_l->r = (float)poly.gt3l->color.r;
				gt3_l->g = (float)poly.gt3l->color.g;
				gt3_l->b = (float)poly.gt3l->color.b;
				break;
			case GOURTEXT4LIT:
			case GOURTEXT4LIT_T:
				gt4_l = (POLYGT4LIT *)dst;
				gt4_l->texture_set = poly.gt4l->texture_set;
				gt4_l->v0 = (unsigned short)poly.gt4l->v0;
				gt4_l->v1 = (unsigned short)poly.gt4l->v1;
				gt4_l->v2 = (unsigned short)poly.gt4l->v2;
				gt4_l->v3 = (unsigned short)poly.gt4l->v3;
				gt4_l->pn0 = (unsigned short)poly.gt4l->pn0;
				gt4_l->pn1 = (unsigned short)poly.gt4l->pn1;
				gt4_l->pn2 = (unsigned short)poly.gt4l->pn2;
				gt4_l->pn3 = (unsigned short)poly.gt4l->pn3;
				gt4_l->s0 = (float)poly.gt4l->uv0.u;
				gt4_l->t0 = (float)poly.gt4l->uv0.v;
				gt4_l->s1 = (float)poly.gt4l->uv1.u;
				gt4_l->t1 = (float)poly.gt4l->uv1.v;
				gt4_l->s2 = (float)poly.gt4l->uv2.u;
				gt4_l->t2 = (float)poly.gt4l->uv2.v;
				gt4_l->s3 = (float)poly.gt4l->uv3.u;
				gt4_l->t3 = (float)poly.gt4l->uv3.v;
				gt4_l->r = (float)poly.gt4l->color.r;
				gt4_l->g = (float)poly.gt4l->color.g;
				gt4_l->b = (float)poly.gt4l->color.b;
				break;
		}

		size += PolySizes[polytype];
		poly.p += OUT_PolySizes[polytype];
	}

	return size;
}
