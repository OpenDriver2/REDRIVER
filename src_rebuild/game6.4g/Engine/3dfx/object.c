/*
 * Engine/3dfx/object.c - model setup (relocation, bounds, rotation matrices) and object drawing
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "Engine\3dfx\object.c", 0x426e50-0x42c2f7).
 */

#include <math.h>

#include "dr_types.h"
#include "Engine/3dfx/object.h"
#include "D3DGlide/glide_ptrs.h"
#include "Engine/3dfx/polydraw.h"
#include "asmmath.h"
#include "camera.h"
#include "denting.h"
#include "dr_globals.h"
#include "general.h"
#include "handling.h"
#include "map.h"
#include "shell.h"
#include "testmath.h"
#include "effects.h"
#include "Engine/3dfx/gfxinit.h"
#include "dr_cars.h"
#include "glaunch.h"
#include "mirror.h"

extern struc_11D9060 stru_11D9060[256];

#define NUM_ANGLES			4096	// a full turn

#define MAX_LAMPS			20

#define TRAINING_CAR_ID		65		// the car of the training mode (gDoTraining)

// the start of every converted polygon (ConvertModelToInternalFormat): type (POLY_TYPES),
// flags, normal index
#define POLY_TYPE(p)		(*(unsigned char *)(p))
#define POLY_FLAGS(p)		(*(unsigned char *)((p) + 1))
#define POLY_NORMAL(p)		(*(int *)((p) + 4))

// polygon flags: the axis of the normal (SetAxisBits) and the first polygon of its normal
// (CalcDuplicateNormals), which does the back face test for the others
// NOTE: IDA shows the flag as SHAPE_FLAG_TRANS
#define POLY_AXIS_MASK			0x7
#define POLY_FLAG_NORMAL_FIRST	0x8

// BSP_data: the offsets at +12 (polygon pointer list), +16, +20, +24 are relocated too
#define BSP_POLY_PTRS		3

//------------------------------------------------------------------------------------------
// initialised data (0x552948)

VECTOR4 light_source = { 0.0f, 0.0f, -1.0f, 0.0f };

// NOTE: no name in the database (flt_552958): car position to shine texture offset
static float shine_scale = 1.0f / 256.0f;

//------------------------------------------------------------------------------------------
// uninitialised data
// NOTE: bss globals in the linker's common area; defined here as object.c is their main user

MATRIX (*matrix_mem)[NUM_ANGLES];
TEX_INFO *gWaterTexture;

int num_lamps;
GAMELAMP lamps[MAX_LAMPS];
int num_potential_lamps;
int potential_lamps[MAX_LAMPS];		// indices into lamps reaching the model being drawn
int num_pending_lamps;
PENDING_LAMP plamps[MAX_LAMPS];

PLANE frustrum_left;
PLANE frustrum_right;
PLANE frustrum_top;
PLANE frustrum_bottom;
PLANE frustrum_near;
PLANE frustrum_far;
float FRUS_XY;
float FRUS_Z;
char do_clipping;		// the object crosses a side of the view (FrustrumCheck)

// camera matrix * rotation about y for each angle, valid when current_matrix_computed_val
// matches current_object_computed_value
// NOTE: IDA types current_matrix_computed_val as int[4][1024]; it is indexed by angle
MATRIX calculated_matrix[NUM_ANGLES];
int current_matrix_computed_val[NUM_ANGLES];
VECTOR4 prerot_light_source[4];

// light intensity of the world axes (ChangeLightLevel)
float axis_light_intensity_front;
float axis_light_intensity_back;
float axis_light_intensity_left;
float axis_light_intensity_right;
float axis_light_intensity_up;
float axis_light_intensity_down;

// the model being drawn
VECTOR4 *input_vertices_ptr;
PLANE *input_plane_ptr;
float light_intensity[4096];		// per normal
char face_vis[4096];				// per normal: faces the camera
float conv_point_normals[2048];		// light intensity per point normal

void RelocateModel(MODEL *model);
int FrustrumCheck(VECTOR4 *pos, float bounding_sphere);

// NOTE: moved here from the generated stubs; no IDA mark, the owner is a guess from the users
// and the neighbouring data
float x_trans;
float y_trans;
float z_trans;
char FlipX;
MATRIX *CurrentRotationMatrix;
TEX_INFO *gShinyTexture;

// [0x426e50] [D]
void FlushLamps(void)
{
	num_lamps = 0;
}

// [0x426e5f] [D]
// Adds a lamp lighting the models this frame when it is in the PVS and the view (or the rear
// view mirror's); stored in camera space.
void AddLamp(VECTOR4 *pos, int range, int red, int green, int blue)
{
	GAMELAMP *lamp;
	VECTOR4 v;
	int visible;

	if (num_lamps == MAX_LAMPS)
		return;

	if (!PositionVisible(pos, CurrentPVS, camera.current_cell_x, camera.current_cell_z))
		return;

	visible = FrustrumCheck(pos, (float)range);

	if (visible == 0 && gDoRearViewMirror == 1)
	{
		// the position mirrored through the camera
		v.vx = camera_position.vx - pos->vx;
		v.vy = camera_position.vy - pos->vy;
		v.vz = camera_position.vz - pos->vz;

		v.vx += camera_position.vx;
		v.vy += camera_position.vy;
		v.vz += camera_position.vz;

		visible = FrustrumCheck(&v, (float)range);
	}

	if (visible == 0)
		return;

	v.vx = pos->vx - camera_position.vx;
	v.vy = pos->vy - camera_position.vy;
	v.vz = pos->vz - camera_position.vz;

	CurrentRotationMatrix = &inv_camera_matrix;
	MatrixRotate(&v);

	lamp = &lamps[num_lamps++];

	lamp->position.vx = v.vx;
	lamp->position.vy = v.vy;
	lamp->position.vz = v.vz;

	lamp->radius = range;
	lamp->sqradius = (float)(range * range);
	lamp->scale = 1.0f / lamp->sqradius;

	lamp->color.vx = (float)((gLampDawnScale * red) >> 8);
	lamp->color.vy = (float)((gLampDawnScale * green) >> 8);
	lamp->color.vz = (float)((gLampDawnScale * blue) >> 8);
}

// [0x42702c] [D]
void AddPendingLamp(VECTOR4 *position, int range, int red, int green, int blue)
{
	PENDING_LAMP *plamp;

	if (num_pending_lamps == MAX_LAMPS)
		return;

	plamp = &plamps[num_pending_lamps++];

	plamp->position.vx = position->vx;
	plamp->position.vy = position->vy;
	plamp->position.vz = position->vz;

	plamp->range = range;

	plamp->colour.red = (float)red;
	plamp->colour.green = (float)green;
	plamp->colour.blue = (float)blue;
}

// [0x4270a4] [D]
// The lamps whose range reaches the model's bounding sphere, into potential_lamps.
void CalcVisibleLamps(VECTOR4 *pos, MODEL *model)
{
	GAMELAMP *lamp;
	float dist;
	float dx;
	float dy;
	float dz;
	int i;

	lamp = lamps;
	num_potential_lamps = 0;

	for (i = 0; i < num_lamps; i++)
	{
		dist = (float)lamp->radius + model->bounding_sphere;
		dist = dist * dist;

		dx = pos->vx - lamp->position.vx;
		dy = pos->vy - lamp->position.vy;
		dz = pos->vz - lamp->position.vz;

		if (dx * dx + dy * dy + dz * dz < dist)
			potential_lamps[num_potential_lamps++] = i;

		lamp++;
	}
}

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

// [0x42802e] [D]
// The frustum planes of the x / z plane only (FrustrumCheck2d).
void SetupFrustrum2d(void)
{
	VECTOR4 v;

	FRUS_XY = fsintab[1499];
	FRUS_Z = fsintab[475];

	CurrentRotationMatrix = &camera_matrix;

	// NOTE: v.vy is not set before the first rotation in the original (stack garbage); it only
	// reaches frustrum_left.a / c through the matrix
	v.vx = -fsintab[1499];
	v.vz = -fsintab[475];
	MatrixRotate(&v);

	frustrum_left.a = v.vx;
	frustrum_left.b = 0.0f;
	frustrum_left.c = v.vz;

	v.vx = FRUS_XY;
	v.vy = 0.0f;
	v.vz = -FRUS_Z;
	MatrixRotate(&v);

	frustrum_right.a = v.vx;
	frustrum_right.b = 0.0f;
	frustrum_right.c = v.vz;

	v.vx = 0.0f;
	v.vy = 0.0f;
	v.vz = -1.0f;
	MatrixRotate(&v);

	frustrum_near.a = v.vx;
	frustrum_near.b = 0.0f;
	frustrum_near.c = v.vz;

	frustrum_far.a = -v.vx;
	frustrum_far.b = 0.0f;
	frustrum_far.c = -v.vz;

	v.vy = 0.0f;
	v.vx = 0.0f;
	v.vz = 50.0f;
	MatrixRotate(&v);

	v.vx += camera_position.vx;
	v.vz += camera_position.vz;
	frustrum_near.d = v.vx * frustrum_near.a + v.vz * frustrum_near.c;

	v.vy = 0.0f;
	v.vx = 0.0f;
	v.vz = far_clip;
	MatrixRotate(&v);

	v.vx += camera_position.vx;
	v.vz += camera_position.vz;
	frustrum_far.d = v.vx * frustrum_far.a + v.vz * frustrum_far.c;

	v.vx = -320.0f;
	v.vy = 0.0f;
	v.vz = DIST;
	MatrixRotate(&v);

	v.vx += camera_position.vx;
	v.vy += camera_position.vy;
	v.vz += camera_position.vz;
	frustrum_left.d = v.vx * frustrum_left.a + v.vy * frustrum_left.b + v.vz * frustrum_left.c;

	v.vx = 320.0f;
	v.vy = 0.0f;
	v.vz = DIST;
	MatrixRotate(&v);

	v.vx += camera_position.vx;
	v.vy += camera_position.vy;
	v.vz += camera_position.vz;
	frustrum_right.d = v.vx * frustrum_right.a + v.vy * frustrum_right.b + v.vz * frustrum_right.c;
}

// [0x4282ca] [D]
// 0: outside; 1: inside; 2: crosses the near plane. Sets do_clipping when it crosses a side.
int FrustrumCheck2d(VECTOR4 *pos, float radius)
{
	float dist;
	float neg_radius;
	float x;
	float z;
	int zclip;

	zclip = 0;
	neg_radius = -radius;

	CurrentRotationMatrix = &camera_matrix;

	x = pos->vx;
	z = pos->vz;

	do_clipping = 0;

	dist = frustrum_far.a * x + frustrum_far.c * z - frustrum_far.d;

	if (dist >= radius)
		return 0;

	dist = frustrum_left.a * x + frustrum_left.c * z - frustrum_left.d;

	if (dist >= radius)
		return 0;

	if (dist < 0.0f && dist > neg_radius || dist > 0.0f)
		do_clipping = 1;

	dist = frustrum_right.a * x + frustrum_right.c * z - frustrum_right.d;

	if (dist >= radius)
		return 0;

	if (dist < 0.0f && dist > neg_radius || dist > 0.0f)
		do_clipping = 1;

	dist = frustrum_near.a * x + frustrum_near.c * z - frustrum_near.d;

	if (dist < 0.0f)
	{
		if (radius + dist >= 0.0f)
			zclip = 1;
	}
	else
	{
		zclip = 1;
	}

	return zclip + 1;
}

// [0x42845a] [D]
// The six frustum planes in world space from the camera matrix, DIST, far_clip.
void SetupFrustrum(void)
{
	VECTOR4 in;

	FRUS_XY = fsintab[1499];
	FRUS_Z = fsintab[475];

	CurrentRotationMatrix = &camera_matrix;

	in.vx = 0.0f;
	in.vy = -fsintab[1499];
	in.vz = -fsintab[475];
	MatrixRotate(&in);

	frustrum_top.a = in.vx;
	frustrum_top.b = in.vy;
	frustrum_top.c = in.vz;

	in.vx = 0.0f;
	in.vy = FRUS_XY;
	in.vz = -FRUS_Z;
	MatrixRotate(&in);

	frustrum_bottom.a = in.vx;
	frustrum_bottom.b = in.vy;
	frustrum_bottom.c = in.vz;

	in.vx = -FRUS_XY;
	in.vy = 0.0f;
	in.vz = -FRUS_Z;
	MatrixRotate(&in);

	frustrum_left.a = in.vx;
	frustrum_left.b = in.vy;
	frustrum_left.c = in.vz;

	in.vx = FRUS_XY;
	in.vy = 0.0f;
	in.vz = -FRUS_Z;
	MatrixRotate(&in);

	frustrum_right.a = in.vx;
	frustrum_right.b = in.vy;
	frustrum_right.c = in.vz;

	in.vx = 0.0f;
	in.vy = 0.0f;
	in.vz = -1.0f;
	MatrixRotate(&in);

	frustrum_near.a = in.vx;
	frustrum_near.b = in.vy;
	frustrum_near.c = in.vz;

	frustrum_far.a = -in.vx;
	frustrum_far.b = -in.vy;
	frustrum_far.c = -in.vz;

	in.vy = 0.0f;
	in.vx = 0.0f;
	in.vz = 50.0f;
	MatrixRotate(&in);

	in.vx += camera_position.vx;
	in.vy += camera_position.vy;
	in.vz += camera_position.vz;
	frustrum_near.d = in.vx * frustrum_near.a + in.vy * frustrum_near.b + in.vz * frustrum_near.c;

	in.vy = 0.0f;
	in.vx = 0.0f;
	in.vz = far_clip;
	MatrixRotate(&in);

	in.vx += camera_position.vx;
	in.vy += camera_position.vy;
	in.vz += camera_position.vz;
	frustrum_far.d = in.vx * frustrum_far.a + in.vy * frustrum_far.b + in.vz * frustrum_far.c;

	in.vx = -320.0f;
	in.vy = 0.0f;
	in.vz = DIST;
	MatrixRotate(&in);

	in.vx += camera_position.vx;
	in.vy += camera_position.vy;
	in.vz += camera_position.vz;
	frustrum_left.d = in.vx * frustrum_left.a + in.vy * frustrum_left.b + in.vz * frustrum_left.c;

	in.vx = 320.0f;
	in.vy = 0.0f;
	in.vz = DIST;
	MatrixRotate(&in);

	in.vx += camera_position.vx;
	in.vy += camera_position.vy;
	in.vz += camera_position.vz;
	frustrum_right.d = in.vx * frustrum_right.a + in.vy * frustrum_right.b + in.vz * frustrum_right.c;

	in.vx = 0.0f;
	in.vy = -240.0f;
	in.vz = DIST;
	MatrixRotate(&in);

	in.vx += camera_position.vx;
	in.vy += camera_position.vy;
	in.vz += camera_position.vz;
	frustrum_top.d = in.vx * frustrum_top.a + in.vy * frustrum_top.b + in.vz * frustrum_top.c;

	in.vx = 0.0f;
	in.vy = 240.0f;
	in.vz = DIST;
	MatrixRotate(&in);

	in.vx += camera_position.vx;
	in.vy += camera_position.vy;
	in.vz += camera_position.vz;
	frustrum_bottom.d = in.vx * frustrum_bottom.a + in.vy * frustrum_bottom.b + in.vz * frustrum_bottom.c;
}

// [0x428868] [D]
void SetFrustrumClip(int value)
{
	VECTOR4 frus;

	far_clip = (float)value;
	ooz_far_clip = value / 4 + value;

	CurrentRotationMatrix = &camera_matrix;

	frus.vx = 0.0f;
	frus.vy = 0.0f;
	frus.vz = -1.0f;
	MatrixRotate(&frus);

	frustrum_far.a = -frus.vx;
	frustrum_far.b = -frus.vy;
	frustrum_far.c = -frus.vz;

	frus.vy = 0.0f;
	frus.vx = 0.0f;
	frus.vz = far_clip;
	MatrixRotate(&frus);

	frus.vx += camera_position.vx;
	frus.vy += camera_position.vy;
	frus.vz += camera_position.vz;

	frustrum_far.d = frus.vx * frustrum_far.a + frus.vy * frustrum_far.b + frus.vz * frustrum_far.c;
}

// [0x428949] [D]
// 0: outside; 1: inside; 2: crosses the near plane. Sets do_clipping when the sphere crosses a
// side plane (always in a perspective camera mode).
int FrustrumCheck(VECTOR4 *pos, float bounding_sphere)
{
	float dist;
	float neg_bounding_sphere;
	float x;
	float y;
	float z;
	int zclip;

	zclip = 0;

	x = pos->vx;
	y = pos->vy;
	z = pos->vz;

	do_clipping = 0;

	dist = frustrum_far.a * x + frustrum_far.b * y + frustrum_far.c * z - frustrum_far.d;

	if (dist >= bounding_sphere)
		return 0;

	dist = frustrum_left.a * x + frustrum_left.b * y + frustrum_left.c * z - frustrum_left.d;

	if (dist >= bounding_sphere)
		return 0;

	neg_bounding_sphere = -bounding_sphere;

	if (dist < 0.0f && dist > neg_bounding_sphere || dist >= 0.0f)
		do_clipping = 1;

	dist = frustrum_right.a * x + frustrum_right.b * y + frustrum_right.c * z - frustrum_right.d;

	if (dist >= bounding_sphere)
		return 0;

	if (dist < 0.0f && dist > neg_bounding_sphere || dist >= 0.0f)
		do_clipping = 1;

	dist = frustrum_near.a * x + frustrum_near.b * y + frustrum_near.c * z - frustrum_near.d;

	if (dist < 0.0f)
	{
		if (bounding_sphere + dist >= 0.0f)
			zclip = 1;
	}
	else
	{
		dist -= bounding_sphere;
		zclip = 1;
	}

	if (dist >= 0.0f)
		return 0;

	dist = frustrum_top.a * x + frustrum_top.b * y + frustrum_top.c * z - frustrum_top.d;

	if (dist >= bounding_sphere)
		return 0;

	if (dist < 0.0f && dist > neg_bounding_sphere || dist >= 0.0f)
		do_clipping = 1;

	dist = frustrum_bottom.a * x + frustrum_bottom.b * y + frustrum_bottom.c * z - frustrum_bottom.d;

	if (dist >= bounding_sphere)
		return 0;

	if (dist < 0.0f && dist > neg_bounding_sphere || dist >= 0.0f)
		do_clipping = 1;

	if (CamPerspectiveMode)
		do_clipping = 1;

	return zclip + 1;
}

// [0x428be2] [D]
// As FrustrumCheck, and 3 when the sphere crosses the far plane (the far plane is tested only
// when it does not cross the near one).
int FrustrumCheckFar(VECTOR4 *pos, float radius)
{
	float dist;
	float neg_radius;
	float x;
	float y;
	float z;
	int zclip;

	zclip = 0;

	x = pos->vx;
	y = pos->vy;
	z = pos->vz;

	do_clipping = 0;

	dist = frustrum_left.a * x + frustrum_left.b * y + frustrum_left.c * z - frustrum_left.d;

	if (dist >= radius)
		return 0;

	neg_radius = -radius;

	if (dist < 0.0f && dist > neg_radius || dist >= 0.0f)
		do_clipping = 1;

	dist = frustrum_right.a * x + frustrum_right.b * y + frustrum_right.c * z - frustrum_right.d;

	if (dist >= radius)
		return 0;

	if (dist < 0.0f && dist > neg_radius || dist >= 0.0f)
		do_clipping = 1;

	dist = frustrum_near.a * x + frustrum_near.b * y + frustrum_near.c * z - frustrum_near.d;

	if (dist < 0.0f)
	{
		if (radius + dist >= 0.0f)
			zclip = 1;
	}
	else
	{
		dist -= radius;
		zclip = 1;
	}

	if (dist >= 0.0f)
		return 0;

	if (zclip == 0)
	{
		dist = frustrum_far.a * x + frustrum_far.b * y + frustrum_far.c * z - frustrum_far.d;

		if (dist > radius)
			return 0;

		if (dist > neg_radius)
			zclip = 2;
	}

	dist = frustrum_top.a * x + frustrum_top.b * y + frustrum_top.c * z - frustrum_top.d;

	if (dist >= radius)
		return 0;

	if (dist < 0.0f && dist > neg_radius || dist >= 0.0f)
		do_clipping = 1;

	dist = frustrum_bottom.a * x + frustrum_bottom.b * y + frustrum_bottom.c * z - frustrum_bottom.d;

	if (dist >= radius)
		return 0;

	if (dist < 0.0f && dist > neg_radius || dist >= 0.0f)
		do_clipping = 1;

	if (CamPerspectiveMode)
		do_clipping = 1;

	return zclip + 1;
}

// [0x428e98] [D]
// The camera matrices of the four axis angles and the light source rotated into each, for the
// axis aligned models (DrawObjectOSUseAxisChecks).
void GenerateAxisPrecalcs(void)
{
	MATRIX *matrix;
	int i;

	prerot_light_source[3] = light_source;
	prerot_light_source[2] = light_source;
	prerot_light_source[1] = light_source;
	prerot_light_source[0] = light_source;

	for (i = 0; i < 4; i++)
	{
		matrix = &(*matrix_mem)[i * 1024];

		MulMatrixB(&camera_matrix, matrix, &calculated_matrix[i * 1024]);
		current_matrix_computed_val[i * 1024] = current_object_computed_value;

		CurrentRotationMatrix = matrix;
		MatrixRotate(&prerot_light_source[i]);
	}
}

// [0x4290d9] [D]
// Draws a model with an arbitrary rotation. clip_mode: 0 none, 1 near plane, else far plane.
void DrawObject(MODEL *model, MATRIX *matrix, VECTOR4 *position, char clip_mode)
{
	void (**DrawFunction)(int);
	MATRIX drawmat;
	VECTOR4 light;
	VECTOR4 pos;
	VECTOR4 *in;
	PLANE *pp;
	float *li;
	float *out;
	char *vis;
	float lx;
	float ly;
	float lz;
	float px;
	float py;
	float pz;
	int ptype;
	int flags;
	int normal;
	int i;

	if (num_lamps)
		CalcVisibleLamps(position, model);
	else
		num_potential_lamps = 0;

	if (clip_mode == 1)
		DrawFunction = ClipNearPolygonDrawFunction;
	else if (clip_mode)
		DrawFunction = ClipFarPolygonDrawFunction;
	else
		DrawFunction = PolygonDrawFunction;

	if (FlipX)
		x_trans = -position->vx;
	else
		x_trans = position->vx;

	y_trans = position->vy;
	z_trans = position->vz;

	input_vertices_ptr = (VECTOR4 *)model->vertices;
	input_plane_ptr = (PLANE *)model->normals;

	if (model->shape_flags & SHAPE_FLAG_SPRITE)
		matrix = &(*matrix_mem)[-camera_angle.vy & (NUM_ANGLES - 1)];

	if (model->shape_flags & SHAPE_FLAG_LIT)
	{
		// the light in model space
		CurrentRotationMatrix = matrix;

		light = light_source;
		MatrixRotate(&light);

		lx = light.vx;
		ly = light.vy;
		lz = light.vz;
	}

	MulMatrixB(&camera_matrix, matrix, &drawmat);

	// the camera in model space, for the back face test
	CurrentRotationMatrix = &drawmat;

	pos.vx = position->vx;
	pos.vy = position->vy;
	pos.vz = position->vz;
	MatrixRotateB(&pos);

	px = -pos.vx;
	py = -pos.vy;
	pz = -pos.vz;

	polyblock = model->poly_block;

	if (FlipX)
	{
		drawmat.m[0][0] = -drawmat.m[0][0];
		drawmat.m[1][0] = -drawmat.m[1][0];
		drawmat.m[2][0] = -drawmat.m[2][0];
	}

	if (model->shape_flags & SHAPE_FLAG_LIT)
	{
		in = (VECTOR4 *)model->point_normals;
		out = conv_point_normals;

		for (i = 0; i < (unsigned short)model->num_point_normals; i++)
		{
			*out = lx * in->vx + ly * in->vy + lz * in->vz;
			FAST_AMBIENT_LIGHT_ADDITION(out);

			in++;
			out++;
		}

		for (i = 0; i < (unsigned short)model->num_polys; i++)
		{
			ptype = POLY_TYPE(polyblock);
			flags = POLY_FLAGS(polyblock);
			normal = POLY_NORMAL(polyblock);

			li = &light_intensity[normal];
			pp = &input_plane_ptr[normal];
			vis = &face_vis[normal];

			if (flags & POLY_FLAG_NORMAL_FIRST)
			{
				if (px * pp->a + py * pp->b + pz * pp->c - pp->d >= 0.0f)
				{
					*vis = 0;
					polyblock += PolySizes[ptype];
				}
				else
				{
					*vis = 1;

					if (PolyLit[ptype])
					{
						*li = lx * pp->a + ly * pp->b + lz * pp->c;
						FAST_AMBIENT_LIGHT_ADDITION(li);
					}

					DrawFunction[ptype](PolyTrans[ptype]);
				}
			}
			else if (*vis)
			{
				DrawFunction[ptype](PolyTrans[ptype]);
			}
			else
			{
				polyblock += PolySizes[ptype];
			}
		}
	}
	else
	{
		for (i = 0; i < (unsigned short)model->num_polys; i++)
		{
			ptype = POLY_TYPE(polyblock);
			flags = POLY_FLAGS(polyblock);
			normal = POLY_NORMAL(polyblock);

			pp = &input_plane_ptr[normal];
			vis = &face_vis[normal];

			if (flags & POLY_FLAG_NORMAL_FIRST)
			{
				if (px * pp->a + py * pp->b + pz * pp->c - pp->d >= 0.0f)
				{
					*vis = 0;
					polyblock += PolySizes[ptype];
				}
				else
				{
					*vis = 1;
					DrawFunction[ptype](PolyTrans[ptype]);
				}
			}
			else if (*vis)
			{
				DrawFunction[ptype](PolyTrans[ptype]);
			}
			else
			{
				polyblock += PolySizes[ptype];
			}
		}
	}

	current_vertex_computed_value++;
}

// [0x4296b3] [D]
// Draws a model rotated about y by yang, reusing the camera matrices computed this frame
// (calculated_matrix).
void DrawObjectOSReuseMatrices(MODEL *model, int yang, VECTOR4 *pos, char clip_mode)
{
	void (**DrawFunction)(int);
	MATRIX flipmat;
	MATRIX *drawmat;
	MATRIX *matrix;
	VECTOR4 light;
	VECTOR4 cpos;
	PLANE *pp;
	float *in;
	float *li;
	float *out;
	char *vis;
	float lx;
	float ly;
	float lz;
	float px;
	float py;
	float pz;
	int ptype;
	int flags;
	int normal;
	int i;

	if (num_lamps)
		CalcVisibleLamps(pos, model);
	else
		num_potential_lamps = 0;

	if (clip_mode == 1)
		DrawFunction = ClipNearPolygonDrawFunction;
	else if (clip_mode)
		DrawFunction = ClipFarPolygonDrawFunction;
	else
		DrawFunction = PolygonDrawFunction;

	if (FlipX)
		x_trans = -pos->vx;
	else
		x_trans = pos->vx;

	y_trans = pos->vy;
	z_trans = pos->vz;

	input_vertices_ptr = (VECTOR4 *)model->vertices;
	input_plane_ptr = (PLANE *)model->normals;

	if (model->shape_flags & SHAPE_FLAG_SPRITE)
		yang = -camera_angle.vy & (NUM_ANGLES - 1);

	matrix = &(*matrix_mem)[yang];
	drawmat = &calculated_matrix[yang];

	if (current_matrix_computed_val[yang] != current_object_computed_value)
	{
		MulMatrixB(&camera_matrix, matrix, drawmat);
		current_matrix_computed_val[yang] = current_object_computed_value;
	}

	if (model->shape_flags & SHAPE_FLAG_LIT)
	{
		CurrentRotationMatrix = matrix;

		light = light_source;
		MatrixRotate(&light);

		lx = light.vx;
		ly = light.vy;
		lz = light.vz;
	}

	CurrentRotationMatrix = drawmat;

	cpos.vx = pos->vx;
	cpos.vy = pos->vy;
	cpos.vz = pos->vz;
	MatrixRotateB(&cpos);

	px = -cpos.vx;
	py = -cpos.vy;
	pz = -cpos.vz;

	polyblock = model->poly_block;

	if (FlipX)
	{
		flipmat = *drawmat;
		flipmat.m[0][0] = -flipmat.m[0][0];
		flipmat.m[1][0] = -flipmat.m[1][0];
		flipmat.m[2][0] = -flipmat.m[2][0];

		CurrentRotationMatrix = &flipmat;
	}

	if (model->shape_flags & SHAPE_FLAG_LIT)
	{
		in = (float *)model->point_normals;
		out = conv_point_normals;

		for (i = 0; i < (unsigned short)model->num_point_normals; i++)
		{
			*out = lx * in[0] + ly * in[1] + lz * in[2];
			FAST_AMBIENT_LIGHT_ADDITION(out);

			in += 4;
			out++;
		}

		for (i = 0; i < (unsigned short)model->num_polys; i++)
		{
			ptype = POLY_TYPE(polyblock);
			flags = POLY_FLAGS(polyblock);
			normal = POLY_NORMAL(polyblock);

			li = &light_intensity[normal];
			pp = &input_plane_ptr[normal];
			vis = &face_vis[normal];

			if (flags & POLY_FLAG_NORMAL_FIRST)
			{
				if (px * pp->a + py * pp->b + pz * pp->c - pp->d >= 0.0f)
				{
					polyblock += PolySizes[ptype];
					*vis = 0;
					continue;
				}

				*vis = 1;

				if (PolyLit[ptype])
				{
					*li = lx * pp->a + ly * pp->b + lz * pp->c;
					FAST_AMBIENT_LIGHT_ADDITION(li);
				}
			}
			else if (*vis == 0)
			{
				polyblock += PolySizes[ptype];
				continue;
			}

			DrawFunction[ptype](PolyTrans[ptype]);
		}
	}
	else
	{
		for (i = 0; i < (unsigned short)model->num_polys; i++)
		{
			ptype = POLY_TYPE(polyblock);
			flags = POLY_FLAGS(polyblock);
			normal = POLY_NORMAL(polyblock);

			pp = &input_plane_ptr[normal];
			vis = &face_vis[normal];

			if (flags & POLY_FLAG_NORMAL_FIRST)
			{
				if (px * pp->a + py * pp->b + pz * pp->c - pp->d >= 0.0f)
				{
					*vis = 0;
					polyblock += PolySizes[ptype];
				}
				else
				{
					*vis = 1;
					DrawFunction[ptype](PolyTrans[ptype]);
				}
			}
			else if (*vis)
			{
				DrawFunction[ptype](PolyTrans[ptype]);
			}
			else
			{
				polyblock += PolySizes[ptype];
			}
		}
	}

	current_vertex_computed_value++;
}

// [0x429d9e] [D]
// Draws an axis aligned model at one of the four axis angles (yang 0, 1024, 2048, 3072): the
// back face test of an axis aligned normal is one coordinate of the camera position, and its
// light the precalculated axis intensity (GenerateAxisPrecalcs, ChangeLightLevel).
void DrawObjectOSUseAxisChecks(MODEL *model, int yang, VECTOR4 *pos, char clip_mode)
{
	void (**DrawFunction)(int);
	MATRIX flipmat;
	VECTOR4 cpos;
	VECTOR4 *light;
	VECTOR4 *in;
	PLANE *plane;
	float *intensity;
	float *out;
	char *vis;
	float px;
	float py;
	float pz;
	float npx;
	float npy;
	float npz;
	float lx;
	float ly;
	float lz;
	float light_neg_x;
	float light_pos_x;
	float light_neg_z;
	float light_pos_z;
	int ptype;
	int flags;
	int normal;
	int draw;
	int i;

	if (num_lamps)
		CalcVisibleLamps(pos, model);
	else
		num_potential_lamps = 0;

	if (clip_mode == 1)
		DrawFunction = ClipNearPolygonDrawFunction;
	else if (clip_mode)
		DrawFunction = ClipFarPolygonDrawFunction;
	else
		DrawFunction = PolygonDrawFunction;

	if (FlipX)
		x_trans = -pos->vx;
	else
		x_trans = pos->vx;

	y_trans = pos->vy;
	z_trans = pos->vz;

	input_vertices_ptr = (VECTOR4 *)model->vertices;
	input_plane_ptr = (PLANE *)model->normals;

	CurrentRotationMatrix = &calculated_matrix[yang];

	cpos.vx = pos->vx;
	cpos.vy = pos->vy;
	cpos.vz = pos->vz;
	MatrixRotateB(&cpos);

	px = cpos.vx;
	py = cpos.vy;
	pz = cpos.vz;

	npx = -cpos.vx;
	npy = -cpos.vy;
	npz = -cpos.vz;

	light = &prerot_light_source[yang / 1024];

	lx = light->vx;
	ly = light->vy;
	lz = light->vz;

	if (FlipX)
	{
		flipmat = calculated_matrix[yang];
		flipmat.m[0][0] = -flipmat.m[0][0];
		flipmat.m[1][0] = -flipmat.m[1][0];
		flipmat.m[2][0] = -flipmat.m[2][0];

		CurrentRotationMatrix = &flipmat;
	}

	polyblock = model->poly_block;

	if (model->shape_flags & SHAPE_FLAG_LIT)
	{
		// the world axis light intensities seen from the model's axes
		// NOTE: left unset (stack garbage in the original) for any other yang
		if (yang > 2048)
		{
			if (yang == 3072)
			{
				light_pos_z = axis_light_intensity_left;
				light_neg_x = axis_light_intensity_front;
				light_neg_z = axis_light_intensity_right;
				light_pos_x = axis_light_intensity_back;
			}
		}
		else if (yang == 2048)
		{
			light_pos_z = axis_light_intensity_back;
			light_neg_x = axis_light_intensity_left;
			light_neg_z = axis_light_intensity_front;
			light_pos_x = axis_light_intensity_right;
		}
		else if (yang)
		{
			if (yang == 1024)
			{
				light_pos_z = axis_light_intensity_right;
				light_neg_x = axis_light_intensity_back;
				light_neg_z = axis_light_intensity_left;
				light_pos_x = axis_light_intensity_front;
			}
		}
		else
		{
			light_pos_z = axis_light_intensity_front;
			light_neg_x = axis_light_intensity_right;
			light_neg_z = axis_light_intensity_back;
			light_pos_x = axis_light_intensity_left;
		}

		in = (VECTOR4 *)model->point_normals;
		out = conv_point_normals;

		for (i = 0; i < (unsigned short)model->num_point_normals; i++)
		{
			*out = lx * in->vx + ly * in->vy + lz * in->vz;
			FAST_AMBIENT_LIGHT_ADDITION(out);

			in++;
			out++;
		}

		for (i = 0; i < (unsigned short)model->num_polys; i++)
		{
			ptype = POLY_TYPE(polyblock);
			flags = POLY_FLAGS(polyblock);
			normal = POLY_NORMAL(polyblock);

			intensity = &light_intensity[normal];
			plane = &input_plane_ptr[normal];
			vis = &face_vis[normal];

			if (flags & POLY_FLAG_NORMAL_FIRST)
			{
				switch (flags & POLY_AXIS_MASK)
				{
					case 0:
						if (npx * plane->a + npy * plane->b + npz * plane->c - plane->d < 0.0f)
						{
							*vis = 1;

							if (PolyLit[ptype])
							{
								*intensity = lx * plane->a + ly * plane->b + lz * plane->c;
								FAST_AMBIENT_LIGHT_ADDITION(intensity);
							}

							DrawFunction[ptype](PolyTrans[ptype]);
							continue;
						}
						break;
					case 1:
						if (px - plane->d < 0.0f)
						{
							*vis = 1;
							*intensity = light_pos_x;
							DrawFunction[ptype](PolyTrans[ptype]);
							continue;
						}
						break;
					case 2:
						if (npx - plane->d < 0.0f)
						{
							*vis = 1;
							*intensity = light_neg_x;
							DrawFunction[ptype](PolyTrans[ptype]);
							continue;
						}
						break;
					case 3:
						if (npz - plane->d < 0.0f)
						{
							*vis = 1;
							*intensity = light_pos_z;
							DrawFunction[ptype](PolyTrans[ptype]);
							continue;
						}
						break;
					case 4:
						if (pz - plane->d < 0.0f)
						{
							*vis = 1;
							*intensity = light_neg_z;
							DrawFunction[ptype](PolyTrans[ptype]);
							continue;
						}
						break;
					case 5:
						if (py - plane->d < 0.0f)
						{
							*vis = 1;
							*intensity = axis_light_intensity_up;
							DrawFunction[ptype](PolyTrans[ptype]);
							continue;
						}
						break;
					case 6:
						if (npy - plane->d < 0.0f)
						{
							*vis = 1;
							*intensity = axis_light_intensity_down;
							DrawFunction[ptype](PolyTrans[ptype]);
							continue;
						}
						break;
					default:
						// NOTE: axis 7 never occurs; the polygon is neither drawn nor skipped
						continue;
				}

				polyblock += PolySizes[ptype];
				*vis = 0;
			}
			else if (*vis)
			{
				DrawFunction[ptype](PolyTrans[ptype]);
			}
			else
			{
				polyblock += PolySizes[ptype];
			}
		}
	}
	else
	{
		for (i = 0; i < (unsigned short)model->num_polys; i++)
		{
			ptype = POLY_TYPE(polyblock);
			flags = POLY_FLAGS(polyblock);
			normal = POLY_NORMAL(polyblock);

			plane = &input_plane_ptr[normal];
			vis = &face_vis[normal];

			if (flags & POLY_FLAG_NORMAL_FIRST)
			{
				switch (flags & POLY_AXIS_MASK)
				{
					case 0:
						draw = npx * plane->a + npy * plane->b + npz * plane->c - plane->d < 0.0f;
						break;
					case 1:
						draw = px - plane->d < 0.0f;
						break;
					case 2:
						draw = npx - plane->d < 0.0f;
						break;
					case 3:
						draw = npz - plane->d < 0.0f;
						break;
					case 4:
						draw = pz - plane->d < 0.0f;
						break;
					case 5:
						draw = py - plane->d < 0.0f;
						break;
					case 6:
						draw = npy - plane->d < 0.0f;
						break;
					default:
						continue;
				}

				if (draw)
				{
					*vis = 1;
					DrawFunction[ptype](PolyTrans[ptype]);
				}
				else
				{
					*vis = 0;
					polyblock += PolySizes[ptype];
				}
			}
			else if (*vis)
			{
				DrawFunction[ptype](PolyTrans[ptype]);
			}
			else
			{
				polyblock += PolySizes[ptype];
			}
		}
	}

	current_vertex_computed_value++;
}

// [0x42a993] [D]
// Draws a single polygon model (a sprite) rotated about y by yang, without back face test.
void DrawObjectOS(MODEL *model, int yang, VECTOR4 *pos, char clip_mode)
{
	MATRIX flipmat;
	MATRIX *drawmat;

	if (num_lamps)
		CalcVisibleLamps(pos, model);
	else
		num_potential_lamps = 0;

	light_intensity[0] = axis_light_intensity_down;

	if (FlipX)
		x_trans = -pos->vx;
	else
		x_trans = pos->vx;

	y_trans = pos->vy;
	z_trans = pos->vz;

	input_vertices_ptr = (VECTOR4 *)model->vertices;
	input_plane_ptr = (PLANE *)model->normals;

	drawmat = &calculated_matrix[yang];

	if (current_matrix_computed_val[yang] != current_object_computed_value)
	{
		MulMatrixB(&camera_matrix, &(*matrix_mem)[yang], drawmat);
		current_matrix_computed_val[yang] = current_object_computed_value;
	}

	CurrentRotationMatrix = drawmat;

	if (FlipX)
	{
		flipmat = *drawmat;
		flipmat.m[0][0] = -flipmat.m[0][0];
		flipmat.m[1][0] = -flipmat.m[1][0];
		flipmat.m[2][0] = -flipmat.m[2][0];

		CurrentRotationMatrix = &flipmat;
	}

	polyblock = model->poly_block;

	if (clip_mode == 0)
		PolygonDrawFunction[POLY_TYPE(polyblock)](0);
	else if (clip_mode == 1)
		ClipNearPolygonDrawFunction[POLY_TYPE(polyblock)](0);
	else
		ClipFarPolygonDrawFunction[POLY_TYPE(polyblock)](0);

	current_vertex_computed_value++;
}

// [0x42ab4e] [D]
// Draws the water surface model (one lit textured quad) with its texture coordinates swaying
// with game_frame.
void DrawBoatModel(MODEL *model, int yang, VECTOR4 *pos, char clip_mode)
{
	MATRIX flipmat;
	MATRIX *drawmat;
	POLYFT4LIT *poly;
	float sway_s;
	float sway_t;
	float border;
	float u0;
	float v0;
	float u1;
	float v1;
	int iborder;

	if (num_lamps)
		CalcVisibleLamps(pos, model);
	else
		num_potential_lamps = 0;

	light_intensity[0] = axis_light_intensity_down;

	if (FlipX)
		x_trans = -pos->vx;
	else
		x_trans = pos->vx;

	y_trans = pos->vy;
	z_trans = pos->vz;

	input_vertices_ptr = (VECTOR4 *)model->vertices;
	input_plane_ptr = (PLANE *)model->normals;

	drawmat = &calculated_matrix[yang];

	if (current_matrix_computed_val[yang] != current_object_computed_value)
	{
		MulMatrixB(&camera_matrix, &(*matrix_mem)[yang], drawmat);
		current_matrix_computed_val[yang] = current_object_computed_value;
	}

	CurrentRotationMatrix = drawmat;

	if (FlipX)
	{
		// NOTE: the original copies the matrix as 16 floats and negates [0], [3], [6], which
		// are m[0][0], m[1][0], m[2][0]
		flipmat = *drawmat;
		flipmat.m[0][0] = -flipmat.m[0][0];
		flipmat.m[1][0] = -flipmat.m[1][0];
		flipmat.m[2][0] = -flipmat.m[2][0];

		CurrentRotationMatrix = &flipmat;
	}

	polyblock = model->poly_block;

	poly = (POLYFT4LIT *)polyblock;
	poly->r = 159.0f;
	poly->g = 168.0f;
	poly->b = 192.0f;

	// NOTE: the original also stores the fsintab indices in an unnamed global (0xb9b4c4) that
	// nothing reads
	sway_s = fsintab[16 * (game_frame & 0xFF)];
	sway_t = fsintab[(16 * (game_frame & 0xFF) + 1737) & (NUM_ANGLES - 1)];

	iborder = gWaterTexture->w / 32;
	border = (float)iborder;

	sway_s *= border;
	sway_t *= border;

	u0 = border + (float)gWaterTexture->u;
	v0 = border + (float)gWaterTexture->v;
	u1 = (float)(gWaterTexture->w - iborder) + (float)gWaterTexture->u;
	v1 = (float)(gWaterTexture->h - iborder) + (float)gWaterTexture->v;

	poly->s0 = u0 + sway_s;
	poly->t0 = v0 + sway_t;
	poly->s1 = u1 + sway_t;
	poly->t1 = v0 - sway_s;
	poly->s2 = u1 - sway_s;
	poly->t2 = v1 - sway_t;
	poly->s3 = u0 - sway_t;
	poly->t3 = v1 + sway_t;

	// transparency 3 (additive) when the objects under the water show through
	if (DrawSubmergedObjects)
	{
		if (clip_mode == 0)
			PolygonDrawFunction[POLY_TYPE(polyblock)](3);
		else if (clip_mode == 1)
			ClipNearPolygonDrawFunction[POLY_TYPE(polyblock)](3);
		else
			ClipFarPolygonDrawFunction[POLY_TYPE(polyblock)](3);
	}
	else
	{
		if (clip_mode == 0)
			PolygonDrawFunction[POLY_TYPE(polyblock)](0);
		else if (clip_mode == 1)
			ClipNearPolygonDrawFunction[POLY_TYPE(polyblock)](0);
		else
			ClipFarPolygonDrawFunction[POLY_TYPE(polyblock)](0);
	}

	current_vertex_computed_value++;
}

// [0x42af61] [D]
// Draws a car body: back face test on every polygon (no shared normals), and in shiny mode
// (or the training car) the shine texture u, v per point normal for the polygons marked in
// gCarShinyPolyLookup.
void DrawCarModel(MODEL *model, MATRIX *matrix, VECTOR4 *pos, char clip_mode, int id, int carmodel, int maxdamage, float shine)
{
	void (**DrawFunction)(int);
	CAR_DATA *cp;
	MATRIX drawmat;
	MATRIX *body_mat;
	VECTOR4 light;
	VECTOR4 cpos;
	VECTOR4 *point_normals;
	PLANE *plane;
	struc_11D9060 *shine_uv;
	float *intensity;
	float *cpnormals;
	float lx;
	float ly;
	float lz;
	float px;
	float py;
	float pz;
	float nx;
	float ny;
	float nz;
	float sx;
	float sz;
	float shiny_color;
	float fhalfh;
	float fhalfw;
	float fcx;
	float fcz;
	float fcx_u;
	float fcz_v;
	int ihalfw;
	int ihalfh;
	int ptype;
	int polyidx;
	int training;
	int i;
	char shiny;

	cp = &car_data[id];

	if (num_lamps)
		CalcVisibleLamps(pos, model);
	else
		num_potential_lamps = 0;

	if (clip_mode == 1)
		DrawFunction = ClipNearPolygonDrawFunction;
	else if (clip_mode)
		DrawFunction = ClipFarPolygonDrawFunction;
	else
		DrawFunction = PolygonDrawFunction;

	if (FlipX)
		x_trans = -pos->vx;
	else
		x_trans = pos->vx;

	y_trans = pos->vy;
	z_trans = pos->vz;

	input_vertices_ptr = (VECTOR4 *)model->vertices;
	input_plane_ptr = (PLANE *)model->normals;

	plane = input_plane_ptr;

	if (last_filter_state != 1)
	{
		grTexFilterMode(GR_TMU0, GR_TEXTUREFILTER_BILINEAR, GR_TEXTUREFILTER_BILINEAR);
		last_filter_state = 1;
	}

	MulMatrixB(&camera_matrix, matrix, &drawmat);

	CurrentRotationMatrix = matrix;

	intensity = light_intensity;

	light = light_source;
	MatrixRotate(&light);

	lx = light.vx;
	ly = light.vy;
	lz = light.vz;

	CurrentRotationMatrix = &drawmat;

	cpos.vx = pos->vx;
	cpos.vy = pos->vy;
	cpos.vz = pos->vz;
	MatrixRotateB(&cpos);

	px = -cpos.vx;
	py = -cpos.vy;
	pz = -cpos.vz;

	point_normals = (VECTOR4 *)model->point_normals;
	cpnormals = conv_point_normals;

	for (i = 0; i < (unsigned short)model->num_point_normals; i++)
	{
		*cpnormals = lx * point_normals->vx + ly * point_normals->vy + lz * point_normals->vz;
		FAST_AMBIENT_LIGHT_ADDITION(cpnormals);

		point_normals++;
		cpnormals++;
	}

	if (FlipX)
	{
		drawmat.m[0][0] = -drawmat.m[0][0];
		drawmat.m[1][0] = -drawmat.m[1][0];
		drawmat.m[2][0] = -drawmat.m[2][0];
	}

	training = gDoTraining && id == TRAINING_CAR_ID;

	if (training)
	{
		shiny_color = 100.0f;
	}
	else if (gDoShinyCars == 0 || shine == 0.0f)
	{
		gShinyType = 0;

		polyblock = model->poly_block;

		for (polyidx = 0; polyidx < (unsigned short)model->num_polys; polyidx++)
		{
			ptype = POLY_TYPE(polyblock);

			if (px * plane->a + py * plane->b + pz * plane->c - plane->d >= 0.0f)
			{
				polyblock += PolySizes[ptype];
			}
			else
			{
				if (PolyLit[ptype])
				{
					*intensity = lx * plane->a + ly * plane->b + lz * plane->c;
					FAST_AMBIENT_LIGHT_ADDITION(intensity);
				}

				DrawFunction[ptype](PolyTrans[ptype]);
			}

			plane++;
			intensity++;
		}

		current_vertex_computed_value++;
		return;
	}
	else
	{
		shiny_color = (float)maxdamage * shine;
	}

	// the shine texture follows the car position; u, v from the body matrix x and z of each
	// point normal
	ihalfw = gShinyTexture->w >> 1;
	ihalfh = gShinyTexture->h >> 1;

	fhalfw = (float)(ihalfw >> 1);
	fhalfh = (float)(ihalfh >> 1);

	fcx = (float)(-float_to_int((float)cp->location.vx * shine_scale) % ihalfw);
	fcz = (float)(float_to_int((float)cp->location.vz * shine_scale) % ihalfh);

	// NOTE: both wrap by the half height (the original's)
	if (fcx < 0.0f)
		fcx += (float)ihalfh;

	if (fcz < 0.0f)
		fcz += (float)ihalfh;

	point_normals = (VECTOR4 *)model->point_normals;
	shine_uv = stru_11D9060;

	body_mat = &float_body_mat[id];

	fcx_u = fcx + (float)gShinyTexture->u;
	fcz_v = fcz + (float)gShinyTexture->v;

	for (i = 0; i < (unsigned short)model->num_point_normals; i++)
	{
		nx = point_normals->vx;
		ny = point_normals->vy;
		nz = point_normals->vz;

		sx = nx * body_mat->m[0][0] + ny * body_mat->m[1][0] + nz * body_mat->m[2][0];
		sz = nx * body_mat->m[0][2] + ny * body_mat->m[1][2] + nz * body_mat->m[2][2];

		shine_uv->field_0 = float_to_int((1.0f - sx) * fhalfw + fcx_u);
		shine_uv->field_1 = float_to_int((1.0f - sz) * fhalfh + fcz_v);

		point_normals++;
		shine_uv++;
	}

	polyblock = model->poly_block;

	for (polyidx = 0; polyidx < (unsigned short)model->num_polys; polyidx++)
	{
		ptype = POLY_TYPE(polyblock);

		if (px * plane->a + py * plane->b + pz * plane->c - plane->d >= 0.0f)
		{
			polyblock += PolySizes[ptype];
		}
		else
		{
			if (PolyLit[ptype])
			{
				*intensity = lx * plane->a + ly * plane->b + lz * plane->c;
				FAST_AMBIENT_LIGHT_ADDITION(intensity);
			}

			shiny = gCarShinyPolyLookup[carmodel][polyidx];

			// NOTE: gShinyType stays as it was for a lookup value above 2
			if (shiny == 0)
			{
				gShinyType = training ? 2 : 0;
			}
			else if (shiny <= 2)
			{
				gShinyType = training ? 2 : 1;
				lit_vertex_color = shiny_color;
			}

			DrawFunction[ptype](PolyTrans[ptype]);
		}

		plane++;
		intensity++;
	}

	gShinyType = 0;

	current_vertex_computed_value++;
}

// [0x42bd3b] [D]
// NOTE: no name in the database and no callers. DrawObject drawing every polygon with
// transparency 2 (3 when lit), and the lit polygons with a fixed intensity.
void sub_42BD3B(MODEL *model, MATRIX *matrix, VECTOR *position, char clip_mode)
{
	void (**DrawFunction)(int);
	MATRIX drawmat;
	VECTOR4 light;
	VECTOR4 pos;
	PLANE *pp;
	float *in;
	float *li;
	float *out;
	char *vis;
	float lx;
	float ly;
	float lz;
	float px;
	float py;
	float pz;
	int ptype;
	int flags;
	int normal;
	int i;

	if (num_lamps)
		CalcVisibleLamps((VECTOR4 *)position, model);
	else
		num_potential_lamps = 0;

	if (clip_mode == 1)
		DrawFunction = ClipNearPolygonDrawFunction;
	else if (clip_mode)
		DrawFunction = ClipFarPolygonDrawFunction;
	else
		DrawFunction = PolygonDrawFunction;

	if (FlipX)
		x_trans = -position->vx;
	else
		x_trans = position->vx;

	y_trans = position->vy;
	z_trans = position->vz;

	input_vertices_ptr = (VECTOR4 *)model->vertices;
	input_plane_ptr = (PLANE *)model->normals;

	if (model->shape_flags & SHAPE_FLAG_SPRITE)
		matrix = &(*matrix_mem)[-camera_angle.vy & (NUM_ANGLES - 1)];

	if (model->shape_flags & SHAPE_FLAG_LIT)
	{
		CurrentRotationMatrix = matrix;

		light = light_source;
		MatrixRotate(&light);

		lx = light.vx;
		ly = light.vy;
		lz = light.vz;
	}

	MulMatrixB(&camera_matrix, matrix, &drawmat);

	CurrentRotationMatrix = &drawmat;

	pos.vx = position->vx;
	pos.vy = position->vy;
	pos.vz = position->vz;
	MatrixRotateB(&pos);

	px = -pos.vx;
	py = -pos.vy;
	pz = -pos.vz;

	polyblock = model->poly_block;

	if (FlipX)
	{
		drawmat.m[0][0] = -drawmat.m[0][0];
		drawmat.m[1][0] = -drawmat.m[1][0];
		drawmat.m[2][0] = -drawmat.m[2][0];
	}

	if (model->shape_flags & SHAPE_FLAG_LIT)
	{
		in = (float *)model->point_normals;
		out = conv_point_normals;

		for (i = 0; i < (unsigned short)model->num_point_normals; i++)
		{
			*out = lx * in[0] + ly * in[1] + lz * in[2];
			FAST_AMBIENT_LIGHT_ADDITION(out);

			in += 4;
			out++;
		}

		for (i = 0; i < (unsigned short)model->num_polys; i++)
		{
			ptype = POLY_TYPE(polyblock);
			flags = POLY_FLAGS(polyblock);
			normal = POLY_NORMAL(polyblock);

			li = &light_intensity[normal];
			pp = &input_plane_ptr[normal];
			vis = &face_vis[normal];

			if (flags & POLY_FLAG_NORMAL_FIRST)
			{
				if (px * pp->a + py * pp->b + pz * pp->c - pp->d >= 0.0f)
				{
					*vis = 0;
					polyblock += PolySizes[ptype];
					continue;
				}

				*vis = 1;

				if (PolyLit[ptype])
				{
					// NOTE: the computed intensity is overwritten at once
					*li = lx * pp->a + ly * pp->b + lz * pp->c;
					*li = 0.01f;
					FAST_AMBIENT_LIGHT_ADDITION(li);
				}
			}
			else if (*vis == 0)
			{
				polyblock += PolySizes[ptype];
				continue;
			}

			DrawFunction[ptype](3);
		}
	}
	else
	{
		for (i = 0; i < (unsigned short)model->num_polys; i++)
		{
			ptype = POLY_TYPE(polyblock);
			flags = POLY_FLAGS(polyblock);
			normal = POLY_NORMAL(polyblock);

			pp = &input_plane_ptr[normal];
			vis = &face_vis[normal];

			if (flags & POLY_FLAG_NORMAL_FIRST)
			{
				if (px * pp->a + py * pp->b + pz * pp->c - pp->d >= 0.0f)
				{
					*vis = 0;
					polyblock += PolySizes[ptype];
				}
				else
				{
					*vis = 1;
					DrawFunction[ptype](2);
				}
			}
			else if (*vis)
			{
				DrawFunction[ptype](2);
			}
			else
			{
				polyblock += PolySizes[ptype];
			}
		}
	}

	current_vertex_computed_value++;
}
