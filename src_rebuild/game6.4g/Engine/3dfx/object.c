/*
 * Engine/3dfx/object.c - model setup (relocation, bounds, rotation matrices) and object drawing
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "Engine\3dfx\object.c"); partially translated.
 */

#include <math.h>

#include "dr_types.h"
#include "Engine/3dfx/object.h"
#include "Engine/3dfx/polydraw.h"
#include "asmmath.h"
#include "general.h"
#include "map.h"
#include "testmath.h"

#define NUM_ANGLES			4096	// a full turn

// BSP_data: the offsets at +12 (polygon pointer list), +16, +20, +24 are relocated too
#define BSP_POLY_PTRS		3

//------------------------------------------------------------------------------------------
// initialised data (0x552948)
// NOTE: only the data the translated functions use; flt_552958 after light_source belongs to
// DrawCarModel

VECTOR4 light_source = { 0.0f, 0.0f, -1.0f, 0.0f };

//------------------------------------------------------------------------------------------
// uninitialised data
// NOTE: bss globals in the linker's common area; defined here as object.c is their main user

MATRIX (*matrix_mem)[NUM_ANGLES];
TEX_INFO *gWaterTexture;

void RelocateModel(MODEL *model);

// [0x427176] [D]
// The rotation matrices about y for every angular_step of the 4096 angles.
LONG CalcObjectRotationMatrices(int angular_step)
{
	MATRIX *matrix;
	SVECTOR ang;
	int i;
	int count;

	ang.vx = 0;
	ang.vy = 0;
	ang.vz = 0;
	ang.pad = 0;

	count = NUM_ANGLES / angular_step;

	matrix_mem = (MATRIX (*)[NUM_ANGLES])DebugMalloc(count * sizeof(MATRIX), "D:\\Driver\\game6.4g\\Engine\\3dfx\\Object.c", 245);

	if (matrix_mem)
	{
		matrix = (MATRIX *)matrix_mem;

		for (i = 0; i < count; i++)
		{
			CalcMatrixXYZ(matrix, &ang);
			ang.vy = (ang.vy + angular_step) & (NUM_ANGLES - 1);

			matrix++;
		}
	}

	return (LONG)matrix_mem;
}

// [0x42721f] [D]
void InitWater(void)
{
	gWaterTexture = GetTextureDetails("SEA");
}

// [0x427236] [D]
void DestroyObjectRotationMatrices(void)
{
	DebugFree(matrix_mem);
	matrix_mem = NULL;
}

// [0x427253] [D]
// The extents of a model of the level's model list.
// NOTE: written as a loop over the 3 or 4 vertices of each polygon; the binary repeats the
// comparisons for each vertex
BOX *CalcBoundingBox(BOX *box, int model)
{
	float upper_z;
	float upper_y;
	float upper_x;
	BOX result;
	char *poly_block;
	int i;
	int j;
	int num_verts;
	VECTOR4 *vertices;
	VECTOR4 *v;
	float lower_z;
	float lower_y;
	float lower_x;

	vertices = (VECTOR4 *)model_pointers[model]->vertices;

	lower_x = 1000000.0f;
	upper_x = -1000000.0f;
	lower_y = 1000000.0f;
	upper_y = -1000000.0f;
	lower_z = 1000000.0f;
	upper_z = -1000000.0f;

	poly_block = (char *)model_pointers[model]->poly_block;

	for (i = 0; i < (unsigned short)model_pointers[model]->num_polys; i++)
	{
		// quads have odd types; the vertex indices are at +8
		num_verts = (*poly_block & 1) ? 4 : 3;

		for (j = 0; j < num_verts; j++)
		{
			v = &vertices[*(int *)(poly_block + 8 + j * 4)];

			if (v->vx < lower_x)
				lower_x = v->vx;

			if (v->vx > upper_x)
				upper_x = v->vx;

			if (v->vy < lower_y)
				lower_y = v->vy;

			if (v->vy > upper_y)
				upper_y = v->vy;

			if (v->vz < lower_z)
				lower_z = v->vz;

			if (v->vz > upper_z)
				upper_z = v->vz;
		}

		poly_block += PolySizes[*(unsigned char *)poly_block];
	}

	result.xmin = lower_x;
	result.ymin = lower_y;
	result.zmin = lower_z;
	result.xmax = upper_x;
	result.ymax = upper_y;
	result.zmax = upper_z;
	result.theta = upper_y - lower_y;
	result.iTheta = float_to_int(result.theta);

	*box = result;

	return box;
}

// [0x427aba] [D]
void GetPolyPtrs(MODEL *model, int *ptrs)
{
	int i;
	unsigned char *poly_block;

	poly_block = (unsigned char *)model->poly_block;

	for (i = 0; i < (unsigned short)model->num_polys; i++)
	{
		*ptrs = (int)poly_block;
		poly_block += PolySizes[*poly_block];
		ptrs++;
	}
}

// [0x427b1e] [D]
// Turns the pointers of a model back into offsets.
void UndoRelocateModel(MODEL *model)
{
	int *bsp;

	if (!(model->shape_flags & SHAPE_FLAG_RELOCATED))
		return;

	if (model->instance_number == -1)
	{
		model->shape_flags ^= SHAPE_FLAG_RELOCATED;

		model->texture_set_info -= (int)model;
		model->vertices -= (int)model;
		model->normals -= (int)model;
		model->point_normals -= (int)model;
		model->poly_block -= (int)model;

		if (model->collision_block)
			model->collision_block -= (int)model;

		// NOTE: the polygon pointer list (+12) is not undone here, as in the original
		if (model->BSP_data)
		{
			bsp = (int *)model->BSP_data;
			bsp[4] -= (int)bsp;
			bsp[5] -= model->BSP_data;
			bsp[6] -= model->BSP_data;
			model->BSP_data -= (int)model;
		}
	}
	else
	{
		model->shape_flags ^= SHAPE_FLAG_RELOCATED;

		model->texture_set_info -= (int)model;
		model->poly_block -= (int)model;
		model->point_normals -= (int)model;

		if (model->BSP_data)
		{
			bsp = (int *)model->BSP_data;
			bsp[4] -= (int)bsp;
			bsp[5] -= model->BSP_data;
			bsp[6] -= model->BSP_data;
			bsp[BSP_POLY_PTRS] -= model->BSP_data;
			model->BSP_data -= (int)model;
		}
	}
}

// [0x427cc7] [D]
// Turns the offsets of a model into pointers; an instance shares its parent's vertices,
// normals and collision data.
void RelocateModel(MODEL *model)
{
	int *bsp;
	MODEL *parentmodel;

	if (model->shape_flags & SHAPE_FLAG_RELOCATED)
		return;

	if (model->instance_number == -1)
	{
		model->shape_flags ^= SHAPE_FLAG_RELOCATED;

		model->texture_set_info += (int)model;
		model->vertices += (int)model;
		model->normals += (int)model;
		model->point_normals += (int)model;
		model->poly_block += (int)model;

		if (model->collision_block)
			model->collision_block += (int)model;

		if (model->BSP_data)
		{
			model->BSP_data += (int)model;

			bsp = (int *)model->BSP_data;
			bsp[4] += (int)bsp;
			bsp[5] += model->BSP_data;
			bsp[6] += model->BSP_data;
			bsp[BSP_POLY_PTRS] += model->BSP_data;

			GetPolyPtrs(model, (int *)bsp[BSP_POLY_PTRS]);
		}
	}
	else
	{
		parentmodel = model_pointers[model->instance_number];

		model->shape_flags ^= SHAPE_FLAG_RELOCATED;

		model->texture_set_info += (int)model;
		model->poly_block += (int)model;
		model->point_normals += (int)model;

		if (parentmodel->shape_flags & SHAPE_FLAG_RELOCATED)
		{
			model->vertices = parentmodel->vertices;
			model->normals = parentmodel->normals;
			model->collision_block = parentmodel->collision_block;
		}
		else
		{
			RelocateModel(parentmodel);

			model->vertices = parentmodel->vertices;
			model->normals = parentmodel->normals;
			model->collision_block = parentmodel->collision_block;

			UndoRelocateModel(parentmodel);
		}
	}
}

// [0x427eaf] [D]
// non3D: the radius in the xz plane (bounding_sphere2d)
void CalcBoundingSphere(MODEL *model, int non3D)
{
	float y2;
	float z2;
	float x2;
	float dist;
	int i;
	float *vertices;
	float radius;

	radius = 0.0f;
	vertices = (float *)model->vertices;

	for (i = 0; i < (unsigned short)model->num_vertices; i++)
	{
		if (non3D)
			y2 = 0.0f;
		else
			y2 = vertices[1] * vertices[1];

		z2 = vertices[2] * vertices[2];
		x2 = vertices[0] * vertices[0];

		dist = (float)sqrt(x2 + y2 + z2);

		if (radius < dist)
			radius = dist;

		vertices += 4;
	}

	if (non3D)
		model->bounding_sphere2d = radius;
	else
		model->bounding_sphere = radius;
}

// [0x427f78] [D]
void ScaleVertices(MODEL *model, float x, float y, float z)
{
	VECTOR4 *vertices;
	int i;

	vertices = (VECTOR4 *)model->vertices;

	for (i = 0; i < (unsigned short)model->num_vertices; i++)
	{
		vertices->vx *= x;
		vertices->vy *= y;
		vertices->vz *= z;

		vertices++;
	}
}

// [0x427fe1] [D]
// Scales the plane distance (vw) of the polygon normals.
void ScaleNormals(MODEL *model, float scale)
{
	int i;
	VECTOR4 *normals;

	normals = (VECTOR4 *)model->normals;

	for (i = 0; i < (unsigned short)model->num_polys; i++)
	{
		normals->vw *= scale;
		normals++;
	}
}
