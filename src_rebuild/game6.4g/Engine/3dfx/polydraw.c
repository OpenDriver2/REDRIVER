/*
 * Engine/3dfx/polydraw.c - polygon drawing and the conversion of level models
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "Engine\3dfx\polydraw.c"); partially translated.
 */

#include <math.h>

#include "dr_types.h"
#include "Engine/3dfx/polydraw.h"
#include "asmmath.h"

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

#define AXIS_EPSILON_LOW	0.999f
#define AXIS_EPSILON_HIGH	1.001f

//------------------------------------------------------------------------------------------
// initialised data (0x55299c)
// NOTE: only the data the translated functions use; the chunk also holds two floats and the
// D3D polygon draw function tables (PolygonDrawFunction_D3D ...) between gShinyType and
// PolySizes, and PolyLit after OUT_PolySizes

char gShinyType = 1;

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

//------------------------------------------------------------------------------------------
// uninitialised data
// NOTE: bss globals in the linker's common area; defined here as polydraw.c is their only user

int total_axis_alligned;
int total_none_axis_alligned;
int total_axis_alligned_models;
int total_partial_axis_alligned_models;
int total_none_axis_alligned_models;
int num_axis_alligned;
int num_none_axis_alligned;

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
