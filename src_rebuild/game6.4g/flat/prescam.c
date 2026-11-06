/*
 * flat/prescam.c - motel room camera: moves between the fixed views along the fly-by spline
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "motel\path.c=flat\prescam.c", 0x41a600 - 0x41ae1d)
 */

#include <stdio.h>

#include "dr_types.h"
#include "dr_globals.h"
#include "general.h"
#include "shell.h"
#include "flat/frontend.h"
#include "flat/prescam.h"
#include "camera.h"
#include "testmath.h"
#include "Engine/3dfx/gfxinit.h"
#include "Engine/3dfx/polydraw.h"

void MoveCameraTo(int camera);
void StepCameraPath(void);

//------------------------------------------------------------------------------------------
// initialised data (0x5508b4)

// pairs of views joined directly by the spline (in either direction)
char camlookup[17][2] = {
	{ 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 4 }, { 4, 0 }, { 0, 5 }, { 1, 6 }, { 2, 7 }, { 3, 8 },
	{ 4, 9 }, { 10, 11 }, { 11, 10 }, { 12, 13 }, { 13, 12 }, { 14, 15 }, { 15, 16 }, { 16, 14 },
};

// each view: 1 = reached by a direct jump, the control point of the view, the spline node
// to start from
char cplookup[17][3] = {
	{ 0, 5, 6 }, { 0, 10, 11 }, { 0, 15, 16 }, { 0, 20, 21 }, { 0, 25, 1 }, { 1, 27, 0 },
	{ 1, 28, 1 }, { 1, 29, 2 }, { 1, 30, 3 }, { 1, 26, 4 }, { 0, 5, 6 }, { 0, 10, 1 },
	{ 0, 5, 6 }, { 0, 10, 1 }, { 0, 5, 6 }, { 0, 10, 11 }, { 0, 15, 1 },
};

//------------------------------------------------------------------------------------------
// uninitialised data (0x600d08)
// NOTE: the unnamed, unreferenced slots between these (0x600d0c - 0x600d4c, 0x600d5c - 0x600d6c,
// 0x600d74 - 0x600db4, 0x600dbc - 0x600dcc, 0x600dd4 - 0x600dfc) are left out

int current_frame;
static int to_camera;
static int current_node;
static int wrap_node;			// the last spline node of the loop the views are on
int from_camera;
int num_nodes;
int local_node;
CAMERA_POINT sequence[10];		// the path being followed
int motel_cam_buffer;			// NOTE: a pointer kept as int
int gCameraOption;
int gCameraMoving;

// [0x41a600] [D]
void InitMotelCameras(int level)
{
	char filename[32];

	sprintf(filename, "SCRIPTS\\FLYBY\\FLAT%d.FBD", level);

	if (LoadFlyByData(filename) == 0)
		DisplayErrorAndExit("Files missing, please reinstall");
}

// [0x41a63c] [D]
void FreeMotelCameras(void)
{
	if (motel_cam_buffer)
	{
		DebugFree((void *)motel_cam_buffer);
		motel_cam_buffer = 0;
	}
}

// [0x41a662] [D]
void ResetCamera(void)
{
	gCameraMoving = 0;
	current_frame = 0;
	num_nodes = 0;
}

// [0x41a685] [D]
void SetMotelCamera(LONGVECTOR4 *position)
{
	(*position)[0] = (LONG)control_points[1].position.vx;
	(*position)[1] = (LONG)control_points[1].position.vy;
	(*position)[2] = (LONG)control_points[1].position.vz;
}

// [0x41a6bc] [D]
void PlaceCameraAt(int camera)
{
	from_camera = camera;
}

// [0x41a6c9] [D]
// Builds the path from the current view to the given one: a jump for the direct views, else
// along the spline loop forwards or backwards; while moving, it turns round from where it is.
void MoveCameraTo(int camera)
{
	int direction;
	int node;
	int cp;
	int i;

	to_camera = camera;

	if (camera == from_camera)
	{
		QuaternionToWorldMatrix(&control_points[(u_char)cplookup[from_camera][1]].rotation);
		DIST = control_points[(u_char)cplookup[from_camera][1]].zoom;
		camera_position = control_points[(u_char)cplookup[from_camera][1]].position;
		return;
	}

	if (to_camera >= 10)
	{
		if (to_camera >= 14)
			wrap_node = 15;
		else
			wrap_node = 10;
	}
	else
	{
		wrap_node = 25;
	}

	// NOTE: direction stays unset when the pair is not in camlookup (as in the original)
	for (i = 0; i < 17; i++)
	{
		if (from_camera == (u_char)camlookup[i][0] && to_camera == (u_char)camlookup[i][1])
		{
			direction = 1;
			break;
		}

		if (from_camera == (u_char)camlookup[i][1] && to_camera == (u_char)camlookup[i][0])
		{
			direction = -1;
			break;
		}
	}

	if (gCameraMoving)
	{
		sequence[0] = sequence[local_node - 1];
		sequence[1] = sequence[local_node];
		sequence[2] = sequence[local_node + 1];
		sequence[3] = control_points[(u_char)cplookup[to_camera][1]];
		sequence[3].frame = sequence[2].frame + 30;

		if (DotQuaternions(&sequence[2].rotation, &sequence[3].rotation) < 0.0f)
			NegQuaternion(&sequence[3].rotation);

		sequence[4] = sequence[3];
		sequence[4].frame = sequence[3].frame + 30;

		local_node = 1;
		num_nodes = 4;
	}
	else
	{
		if (cplookup[to_camera][0] == 1 || cplookup[from_camera][0] == 1)
		{
			num_nodes = 3;

			if (DotQuaternions(&control_points[(u_char)cplookup[from_camera][1]].rotation,
							   &control_points[(u_char)cplookup[to_camera][1]].rotation) < 0.0f)
				NegQuaternion(&control_points[(u_char)cplookup[to_camera][1]].rotation);

			sequence[1] = control_points[(u_char)cplookup[from_camera][1]];
			sequence[0] = sequence[1];
			sequence[1].frame = 0;
			sequence[0].frame = 0;

			sequence[3] = control_points[(u_char)cplookup[to_camera][1]];
			sequence[2] = sequence[3];
			sequence[3].frame = 50;
			sequence[2].frame = 50;

			current_node = (u_char)cplookup[to_camera][1];
		}
		else if (direction == 1)
		{
			sequence[0] = control_points[(u_char)cplookup[from_camera][1]];

			node = 1;
			cp = (u_char)cplookup[from_camera][2];

			for (;;)
			{
				sequence[node] = control_points[cp];

				if (cp == 1)
					sequence[node].frame = sequence[node - 1].frame + 30;
				else
					sequence[node].frame = sequence[node].frame + sequence[node - 1].frame - control_points[cp - 1].frame;

				if (cp == (u_char)cplookup[to_camera][2])
					break;

				if (++cp > wrap_node)
					cp = 1;

				if (DotQuaternions(&sequence[node].rotation, &sequence[node - 1].rotation) < 0.0f)
					NegQuaternion(&sequence[node].rotation);

				node++;
			}

			num_nodes = node;
		}
		else
		{
			sequence[0] = control_points[(u_char)cplookup[from_camera][2]];

			node = 1;
			cp = (u_char)cplookup[from_camera][1];

			for (;;)
			{
				sequence[node] = control_points[cp];

				if (cp == wrap_node)
					sequence[node].frame = sequence[node - 1].frame + 30;
				else
					sequence[node].frame = control_points[cp + 1].frame + sequence[node - 1].frame - sequence[node].frame;

				if (cp == (u_char)cplookup[to_camera][1])
					break;

				if (--cp < 1)
					cp = wrap_node;

				if (DotQuaternions(&sequence[node].rotation, &sequence[node - 1].rotation) < 0.0f)
					NegQuaternion(&sequence[node].rotation);

				node++;
			}

			num_nodes = node;
		}

		gCameraMoving = 1;
		current_frame = sequence[1].frame;
		local_node = 1;
	}

	from_camera = camera;
}

// [0x41ad7b] [D]
void FollowCameraPath(void)
{
	if (gCameraMoving)
	{
		StepCameraPath();
		current_frame++;
	}
}

// [0x41ad9d] [D]
// At the end of the path the camera is placed exactly on the view.
void StepCameraPath(void)
{
	if (current_frame == sequence[local_node + 1].frame && ++local_node == num_nodes - 1)
	{
		MoveCameraTo(from_camera);
		motel_font_override = 0;
		gCameraMoving = 0;
	}
	else
	{
		SetCameraOnSpline(current_frame, &sequence[local_node]);
	}
}
