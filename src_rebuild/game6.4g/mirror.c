/*
 * mirror.c - the rear view mirror
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "mirror.c", 0x4f4480 - 0x4f5240).
 *
 * The mirror is drawn after the main view, into a small viewport at the top of the screen: the
 * lamps are mirrored in z, FlipX is set, and the camera is placed in the player's car with the car
 * turned round. The horizontal mirroring is done by the drawing code on FlipX.
 *
 * A view arc says which of the 31x31 map cells around the camera can be seen, for each of 256
 * angles. An entry of 64 shorts holds the first row (-1: none) and the number of rows, then the
 * start column (-1: none) and length of the span on each row. mirror_view_arc and normal_view_arc
 * are view_arc cut down to a radius.
 */

#include "dr_types.h"
#include "D3DGlide/glide_ptrs.h"
#include "Director.h"
#include "Engine/3dfx/gfxinit.h"
#include "Engine/3dfx/object.h"
#include "Engine/3dfx/polydraw.h"
#include "ai.h"
#include "asmmath.h"
#include "camera.h"
#include "debris.h"
#include "dr_globals.h"
#include "effects.h"
#include "event.h"
#include "general.h"
#include "handling.h"
#include "interview.h"
#include "job_fx.h"
#include "lamps.h"
#include "map.h"
#include "minicutscene.h"
#include "mirror.h"
#include "overlay.h"
#include "pedest.h"
#include "shadow.h"
#include "shell.h"
#include "smashable.h"

#define ARC_SIZE	31

// the size of the mirror on the screen
static float mirror_width_scale = 0.36363637f;		// 0x5659b0
static float mirror_height_scale = 0.13333334f;		// 0x5659b4

// NOTE: these sit in the linker's common area; owner by their users, the functions here
short (*mirror_view_arc)[64];
short (*normal_view_arc)[64];
int gRearViewMirrorAvailable;

// NOTE: both names are ours; they have j__ thunks, so they are not static in the original
void PackViewArc(short *arc, char *grid);
void DrawViewArc(short *arc, int x, int y);

// [0x4f4480] [D]
void DrawRearViewMirror(void)
{
	float old_clip;
	int width;
	int height;
	VECTOR4 cam_pos;
	float mirror_w;
	float mirror_h;
	float mirror_x;
	float mirror_y;
	float half_height;
	int oldSubmergedObjects;
	int clip_x;
	int clip_y;
	int clip_w;
	int clip_h;
	int border;

	old_clip = far_clip;
	width = display_width;
	height = display_height;
	cam_pos = camera_position;

	if ((gDoInterview == 0 || giInterviewState != 3 && giInterviewState != 2) &&
		CutSceneActive == 0 && Director.Mode == 0 && FlyBy == 0 && TriggerBomb == 0)
	{
		StartOfEffectsFrame();

		gRainList = &gMirroredRainList;

		arc_view = (short *)mirror_view_arc;

		SetFrustrumClip(float_to_int(mirror_far_clip));
		InvertLamps();

		FlipX = 1;

		mirror_w = (float)display_width * mirror_width_scale;
		mirror_h = (float)display_height * mirror_height_scale;
		mirror_x = (float)(display_width >> 1) - mirror_w * 0.5f;
		half_height = (float)(display_height >> 1);
		mirror_y = half_height - mirror_h * 0.5f + (mirror_h * 0.5f + 9.0f - half_height) * screen_scale;

		display_width = float_to_int(mirror_w);
		display_height = float_to_int(mirror_h);

		ResetViewport(float_to_int(mirror_x), float_to_int(mirror_y), 2);

		current_vertex_computed_value++;
		current_object_computed_value++;

		car_data[0].rotation.vx = -car_data[0].rotation.vx & 0xFFF;
		car_data[0].rotation.vy = (car_data[0].rotation.vy + 2048) & 0xFFF;

		PlaceCameraInCar(0);

		car_data[0].rotation.vx = -car_data[0].rotation.vx & 0xFFF;
		car_data[0].rotation.vy = (car_data[0].rotation.vy + 2048) & 0xFFF;

		SetupFrustrum();

		DrawMap(1);
		DrawMoveableObjects();
		DrawSmashable_sprites(0);

		if (do_fogging)
			grFogMode(GR_FOG_DISABLE);

		DrawTyreTracks();

		if (gMissionRecording || gMissionReplaying)
			DrawOtherTyreTracks();

		if (do_fogging)
			grFogMode(GR_FOG_WITH_TABLE);

		SetupCarShine();
		DrawCivilianCars();
		DrawCarShine();

		DisplayEvents();

		DisplayPedestrians();
		DrawAllPedShadows();

		DrawTargetShadows();

		if (do_fogging)
		{
			oldSubmergedObjects = DrawSubmergedObjects;
			DrawSubmergedObjects = 0;
		}

		DrawWater();

		if (do_fogging)
			DrawSubmergedObjects = oldSubmergedObjects;		// oh shut up

		DrawHorizon();

		if (do_fogging)
			grFogMode(GR_FOG_DISABLE);

		if (gDoSnow)
			DrawSnow();

		if (do_fogging)
			grFogMode(GR_FOG_WITH_TABLE);

		if (gDoChopper)
			ControlChopper(1);

		DisplayEffects();
		DrawCarPools();
		DrawTransparentObjects();
		DrawSmashable_sprites(1);
		ControlCamera(1);

		clip_x = float_to_int(clipxmin);
		clip_y = float_to_int(clipymin);
		clip_w = float_to_int(clipxmax - clipxmin);
		clip_h = float_to_int(clipymax - clipymin);

		display_width = width;
		display_height = height;

		ResetViewport(0, 0, 0);

		// the frame
		if (screen_scale >= 0.5f)
			border = 2;
		else
			border = 1;

		DrawQuad((float)(clip_x - border), (float)(clip_y - border), (float)(clip_w + 2 * border), (float)border, 0x111111);
		DrawQuad((float)(clip_x - border), (float)(clip_h + clip_y), (float)(clip_w + 2 * border), (float)border, 0x111111);
		DrawQuad((float)(clip_x - border), (float)clip_y, (float)border, (float)clip_h, 0x111111);
		DrawQuad((float)(clip_w + clip_x), (float)clip_y, (float)border, (float)clip_h, 0x111111);

		FlipX = 0;

		SetFrustrumClip(float_to_int(old_clip));
		arc_view = (short *)normal_view_arc;

		gRainList = &gNormalRainList;

		camera_position = cam_pos;
	}
}

// [0x4f4b17] [D]
void CalculateMirrorViewArc(float radius)
{
	short *arc;
	short *span;
	char grid[964];
	char *cell;
	float dist;
	float limit;
	int size;
	int row;
	int i;
	int j;
	int k;
	int m;

	size = ARC_SIZE;

	if (mirror_view_arc != NULL)
		KillMirrorViewArc();

	mirror_view_arc = (short (*)[64])DebugMalloc(0x8000, __FILE__, __LINE__);

	if (mirror_view_arc == NULL)
	{
		gRearViewMirrorAvailable = 0;
		gDoRearViewMirror = 0;
		return;
	}

	ClearMem(mirror_view_arc, 0x8000);

	for (i = 0; i < 256; i++)
	{
		ClearMem(grid, size * size);

		// unpack view_arc to a grid
		arc = view_arc[i];

		if (arc[0] == -1)
			row = 0;
		else
			row = arc[0];

		span = &arc[2 * row + 2];

		while (row < size)
		{
			if (span[0] != -1)
			{
				cell = &grid[size * row];
				cell += span[0];

				for (j = 0; j < span[1]; j++)
					*cell++ = 1;
			}

			row++;
			span += 2;
		}

		// remove the cells past the radius
		for (k = -15; k <= 15; k++)
		{
			row = size * (k + 15);

			for (m = -15; m <= 15; m++)
			{
				if (grid[row])
				{
					dist = (float)(k * k + m * m);
					limit = radius * radius;

					if (dist > limit)
						grid[row] = 0;
				}

				row++;
			}
		}

		PackViewArc(mirror_view_arc[i], grid);
	}

	gRearViewMirrorAvailable = 1;
}

// [0x4f4dba] [D]
void CalculateNormalViewArc(void)
{
	short *arc;
	short *span;
	char grid[964];
	char *cell;
	float dist;
	float limit;
	int size;
	int row;
	int i;
	int j;
	int k;
	int m;

	size = ARC_SIZE;
	limit = far_clip * far_clip;

	// BUG: unnecessary re-allocation
	if (normal_view_arc != NULL)
	{
		DebugFree(normal_view_arc);
		normal_view_arc = NULL;
	}

	normal_view_arc = (short (*)[64])DebugMalloc(0x8000, __FILE__, __LINE__);

	ClearMem(normal_view_arc, 0x8000);

	for (i = 0; i < 256; i++)
	{
		ClearMem(grid, size * size);

		// unpack view_arc to a grid
		arc = view_arc[i];

		if (arc[0] == -1)
			row = 0;
		else
			row = arc[0];

		span = &arc[2 * row + 2];

		while (row < size)
		{
			if (span[0] != -1)
			{
				cell = &grid[size * row];
				cell += span[0];

				for (j = 0; j < span[1]; j++)
					*cell++ = 1;
			}

			row++;
			span += 2;
		}

		// remove the cells past the radius
		for (k = -15; k <= 15; k++)
		{
			row = size * (k + 15);

			for (m = -15; m <= 15; m++)
			{
				if (grid[row])
				{
					dist = (float)(k * k + m * m);

					if (dist > limit)
						grid[row] = 0;
				}

				row++;
			}
		}

		PackViewArc(normal_view_arc[i], grid);
	}
}

// [0x4f504a] [D]
// Packs a 31x31 grid to a view arc entry. Each row has one span, from its first set cell.
void PackViewArc(short *arc, char *grid)
{
	short *span;
	char *cells;
	int i;
	int j;

	arc[1] = 0;
	arc[0] = -1;

	span = arc + 2;

	for (i = 0; i < ARC_SIZE; i++)
	{
		cells = &grid[ARC_SIZE * i];

		span[0] = -1;
		span[1] = 0;

		for (j = 0; j < ARC_SIZE; j++)
		{
			if (cells[j])
			{
				if (arc[0] == -1)
					arc[0] = i;

				arc[1]++;

				span[0] = j;

				while (j < ARC_SIZE)
				{
					if (cells[j++])
						span[1]++;
				}
			}
		}

		span += 2;
	}
}

// [0x4f5152] [D]
// Debug view of a view arc entry.
void DrawViewArc(short *arc, int x, int y)
{
	short *span;
	int i;

	if (arc != NULL)
	{
		span = arc + 2;

		for (i = 0; i < ARC_SIZE; i++)
		{
			if (span[1] > 0)
				DrawQuad((float)(10 * span[0] + x), (float)(10 * i + y), (float)(10 * span[1]), 10.0f, 0x8800);

			span += 2;
		}
	}
}

// [0x4f5206] [D]
void KillMirrorViewArc(void)
{
	if (mirror_view_arc != NULL)
	{
		DebugFree(mirror_view_arc);
		mirror_view_arc = NULL;
	}

	mirror_view_arc = NULL;		// redundant
	gRearViewMirrorAvailable = 0;
}
