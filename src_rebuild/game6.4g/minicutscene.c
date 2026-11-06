/*
 * minicutscene.c - mission mini cut scenes: bad guys running to (or from) the player's car
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "minicutscene.c", 0x4f30b0 - 0x4f407f).
 *
 * The mission script sets the scene up (MCS_StartPosition, MCS_EndPosition, MCS_NumBadGuys,
 * MCS_DropOff, camera start / end ...) and StartCutScene. ControlMiniCutScene then walks up to
 * four bad guys (the walk or run motion capture, drawn with the third or fourth pedestrian
 * skeleton) towards CarDoorPosition until they are all there, while PlaceCameraCutScene points
 * the camera (cameraview 7) at the first of them.
 *
 * The bss sits in the linker's common area among motion_c.c and camera.c data
 * (CAMERA_START_ELEVATION, CAMERA_DELTA are camera.c's); the MCS_ globals, CutSceneActive,
 * StartCutScene and CutToCar are defined here as the cut scene's (MScript.c sets them).
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "dr_types.h"
#include "minicutscene.h"
#include "dr_globals.h"
#include "asmmath.h"
#include "camera.h"
#include "general.h"
#include "handling.h"
#include "map.h"
#include "motion_c.h"
#include "pause.h"
#include "testmath.h"
#include "Engine/3dfx/gfxinit.h"
#include "Engine/3dfx/object.h"
#include "Engine/3dfx/polydraw.h"

#define MAX_BAD_GUYS		4

// bytes of one frame of motion capture data (MOTION_PACKET)
#define MOTION_FRAME_SIZE	(sizeof(QUATERNION) * num_objects + sizeof(VECTOR4))

typedef struct MCS_BAD_GUY
{
	char enabled;
	char type;				// motion: 1 walk, 2 run
	short delay;			// frames before it starts
	int num_frames;			// current motion frame
	VECTOR4 position;
} MCS_BAD_GUY;

void GetDoorPosition(void);
void StartMiniCutScene(void);
void TrimmedWalkMotionObject(QUATERNION *frame, HIERARCHY_NODE *hnp, QUATERNION *rot, VECTOR4 *pos);

// uninitialised data

char side;					// the door: 0 left, 1 right of the player car

int start_dist;				// bad guy to door distance at the start
VECTOR4 MCS_CameraEnd;
int current_distance;		// first bad guy to door distance now
MCS_BAD_GUY bad_guys[MAX_BAD_GUYS];
int CutSceneActive;
VECTOR4 MCS_EndPosition;
VECTOR4 CarDoorPosition;	// where the bad guys go
int MCS_DropOff;			// 0 pick up (from MCS_StartPosition to the door), 1 drop off (from the door to MCS_EndPosition), 2 from MCS_StartPosition to MCS_EndPosition
int MCS_Run;
int MCS_Axis;				// the bad guys are spread in z (else x)
float MCS_ZoomStart;
int bad_guy_num;			// the bad guy being drawn
VECTOR4 MCS_CameraStart;
int MCS_CameraPositionsSet;	// 2: the camera moves from MCS_CameraStart to MCS_CameraEnd
int CutToCar;
VECTOR4 MCS_StartPosition;
float MCS_ZoomEnd;
int StartCutScene;
int MCS_NumBadGuys;
int CutSceneDelay;			// frames the scene goes on after the bad guys got there

// [0x4f30b0] [D]
// Camera of the cut scene (cameraview 7): looks at the first bad guy, from a point moving from
// MCS_CameraEnd to MCS_CameraStart with the distance left, else from beside the car rising (drop
// off) or lowering as the bad guy comes closer.
void PlaceCameraCutScene(void)
{
	int t;
	float f;
	float dx;
	float dy;
	float dz;
	LONG dist;
	LONG x;
	LONG y;
	LONG z;

	if (!CutSceneActive)
		return;

	if (!MCS_DropOff)
		GetDoorPosition();

	CameraCollisionCheck(1);

	t = (current_distance << 12) / start_dist;

	if (t > 4096)
		t = 4096;

	if (MCS_CameraPositionsSet == 2)
	{
		f = (float)t / 4096.0f;

		CameraPos.vx = MCS_CameraStart.vx * f + (1.0f - f) * MCS_CameraEnd.vx;
		camera_position.vx = CameraPos.vx;
		CameraPos.vy = MCS_CameraStart.vy * f + (1.0f - f) * MCS_CameraEnd.vy;
		camera_position.vy = CameraPos.vy;
		CameraPos.vz = MCS_CameraStart.vz * f + (1.0f - f) * MCS_CameraEnd.vz;
		camera_position.vz = CameraPos.vz;

		DIST = MCS_ZoomStart * f + (1.0f - f) * MCS_ZoomEnd;
	}
	else
	{
		CameraPos.vy = (float)-MapHeight(&CameraPos);

		if (MCS_DropOff)
			CameraPos.vy += (float)(current_distance * CAMERA_DELTA / start_dist + CAMERA_START_ELEVATION);
		else
			CameraPos.vy += (float)(-200 - current_distance * CAMERA_DELTA / start_dist);

		if (!GamePaused)
		{
			camera_position.vx += (float)((int)(CameraPos.vx - camera_position.vx) / 8);
			camera_position.vy += (float)((int)(CameraPos.vy - camera_position.vy) / 8);
			camera_position.vz += (float)((int)(CameraPos.vz - camera_position.vz) / 8);
		}

		if (MCS_DropOff == 2)
			DIST = 784.0f - 0.097655997f * (float)t;
	}

	dx = bad_guys[0].position.vx - camera_position.vx;
	dy = camera_position.vy + bad_guys[0].position.vy;
	dz = bad_guys[0].position.vz - camera_position.vz;

	dist = float_to_int((float)sqrt(dx * dx + dz * dz));
	x = float_to_int(dx);
	y = float_to_int(dy);
	z = float_to_int(dz);

	camera_angle.vx = (Arc_Tan(dist, y) - 1024) & 0xFFF;
	current_camera_xangle = camera_angle.vx;
	camera_angle.vy = -(short)Arc_Tan(x, z) & 0xFFF;
	current_camera_yangle = camera_angle.vy;
	camera_angle.vz = 0;

	BuildWorldMatrix();
}

// [0x4f3445] [D]
// WalkMotionObject for the bad guys: the third pedestrian skeleton for even bad_guy_num, the
// fourth for odd, no head turning.
void TrimmedWalkMotionObject(QUATERNION *frame, HIERARCHY_NODE *hnp, QUATERNION *rot, VECTOR4 *pos)
{
	VECTOR4 v;
	MATRIX inv;
	QUATERNION q;
	MATRIX m;
	int i;

	if (hnp->num_children == 255)
	{
		v.vx = pos->vx - camera_position.vx + ped_trans.vx;
		v.vy = pos->vy - camera_position.vy + ped_trans.vy;
		v.vz = pos->vz - camera_position.vz + ped_trans.vz;

		CurrentRotationMatrix = &inv_camera_matrix;

		MatrixRotate(&v);
		Quaternion2Matrix(rot, &m);
		InvertMatrix(&m, &inv);

		if (bad_guy_num & 1)
			DrawObject(model_pointers[hnp->object_num + ped4base], &inv, &v, do_z_clipping - 1);
		else
			DrawObject(model_pointers[hnp->object_num + ped3base], &inv, &v, do_z_clipping - 1);
	}
	else
	{
		MulQuaternions(&frame[hnp->object_num], rot, &q);

		v = bone_lengths[hnp->object_num];

		Quaternion2Matrix(rot, &m);
		CurrentRotationMatrix = &m;
		MatrixRotate(&v);

		v.vx = v.vx + pos->vx;
		v.vy = v.vy + pos->vy;
		v.vz = v.vz + pos->vz;

		for (i = 0; i < hnp->num_children; i++)
			TrimmedWalkMotionObject(frame, &hierarchy_tree[hnp->children[i]], &q, &v);
	}
}

// [0x4f366e] [D]
// CarDoorPosition: the side of the player car (200 out) nearer MCS_StartPosition, and its
// distance as start_dist (from MCS_StartPosition to MCS_EndPosition when MCS_DropOff is 2).
void GetDoorPosition(void)
{
	float cs;
	float sn;
	float carx;
	float carz;
	float leftx;
	float leftz;
	float rightx;
	float rightz;
	float dx;
	float dz;
	float left_dist;
	float right_dist;

	cs = fcos(-car_data[0].rotation.vy & 0xFFF);
	sn = fsin(-car_data[0].rotation.vy & 0xFFF);

	carx = (float)car_data[0].location.vx;
	leftx = -200.0f * cs + carx;
	rightx = 200.0f * cs + carx;

	carz = (float)car_data[0].location.vz;
	leftz = -200.0f * sn + carz;
	rightz = 200.0f * sn + carz;

	dx = leftx - MCS_StartPosition.vx;
	dz = leftz - MCS_StartPosition.vz;
	left_dist = (float)sqrt(dx * dx + dz * dz);

	dx = rightx - MCS_StartPosition.vx;
	dz = rightz - MCS_StartPosition.vz;
	right_dist = (float)sqrt(dx * dx + dz * dz);

	if (left_dist >= right_dist)
	{
		CarDoorPosition.vx = rightx;
		CarDoorPosition.vz = rightz;
		side = 1;
		start_dist = float_to_int(right_dist);
	}
	else
	{
		CarDoorPosition.vx = leftx;
		CarDoorPosition.vz = leftz;
		side = 0;
		start_dist = float_to_int(left_dist);
	}

	if (MCS_DropOff == 2)
	{
		dx = MCS_EndPosition.vx - MCS_StartPosition.vx;
		dz = MCS_EndPosition.vz - MCS_StartPosition.vz;
		start_dist = float_to_int((float)sqrt(dx * dx + dz * dz));
	}
}

// [0x4f3865] [D]
// Places the bad guys (at the door for a drop off, else spread around MCS_StartPosition), each
// starting 40 - 55 frames (running 20 - 35) after the one before, and the camera 800 beside the
// car on the door's side.
void StartMiniCutScene(void)
{
	MCS_BAD_GUY *bad_guy;
	float cs;
	float sn;
	int delay;
	int i;

	bad_guy = bad_guys;
	delay = 0;

	GetDoorPosition();

	cs = fcos(-car_data[0].rotation.vy & 0xFFF);
	sn = fsin(-car_data[0].rotation.vy & 0xFFF);

	if (MCS_NumBadGuys > MAX_BAD_GUYS)
		MCS_NumBadGuys = MAX_BAD_GUYS;

	for (i = 0; i < MCS_NumBadGuys; i++)
	{
		if (MCS_DropOff == 1)
		{
			bad_guy->position = CarDoorPosition;
		}
		else
		{
			bad_guy->position = MCS_StartPosition;

			if (MCS_Axis)
				bad_guy->position.vz += (float)((Random() & 0x1FF) - 256);
			else
				bad_guy->position.vx += (float)((Random() & 0x1FF) - 256);
		}

		bad_guy->delay = delay;

		if (MCS_Run)
			bad_guy->type = 2;
		else
			bad_guy->type = 1;

		bad_guy->enabled = 1;
		bad_guy->num_frames = Random() % mcd_frames[bad_guy->type - 1];

		if (MCS_Run)
			delay += (Random() & 0xF) + 20;
		else
			delay += (Random() & 0xF) + 40;

		bad_guy++;
	}

	if (side)
	{
		CameraPos.vx = -800.0f * cs + (float)car_data[0].location.vx;
		CameraPos.vz = -800.0f * sn + (float)car_data[0].location.vz;
	}
	else
	{
		CameraPos.vx = 800.0f * cs + (float)car_data[0].location.vx;
		CameraPos.vz = 800.0f * sn + (float)car_data[0].location.vz;
	}

	CameraPos.vy = (float)(-MapHeight(&CameraPos) - CAMERA_START_ELEVATION);
	cameraview = 7;

	if (MCS_DropOff)
		CarDoorPosition = MCS_EndPosition;

	CutSceneDelay = 50;
	CutSceneActive = 1;
}

// [0x4f3b39] [D]
// Runs the cut scene (drawing 0, in the game step) or draws the bad guys (drawing 1). Each bad
// guy walks towards CarDoorPosition along its motion until within 50 of it; CutSceneDelay frames
// after the last one got there the scene ends and the next call cuts back behind the car.
void ControlMiniCutScene(int drawing)
{
	MCS_BAD_GUY *bad_guy;
	QUATERNION rot_quat;
	SVECTOR rot;
	VECTOR4 root;
	char *data;
	char *packet;
	float *frame_root;
	float cs;
	float sn;
	float dx;
	float dz;
	LONG x;
	LONG z;
	int dir;
	int dist;
	int num_frames;
	int end_frame;
	int busy;
	int i;
	int j;

	bad_guy = bad_guys;
	busy = 0;

	if (CutToCar)
	{
		DIST = 384.0f;

		current_camera_xangle = 0;
		current_camera_yangle = (car_data[0].rotation.vy + 2048) & 0xFFF;

		CameraPos.vy = -256.0f;

		cameraview = 0;
		CutToCar = 0;
		return;
	}

	if (!CutSceneActive)
	{
		if (!StartCutScene || drawing)
			return;

		StartMiniCutScene();
		StartCutScene = 0;
	}

	if (drawing)
		SwapPalette(14);

	for (i = 0; i < MCS_NumBadGuys; i++)
	{
		bad_guy_num = i;

		if (!bad_guy->enabled || bad_guy->delay)
		{
			if (bad_guy->enabled && !drawing)
			{
				bad_guy->delay--;
				busy = 1;
			}
		}
		else
		{
			ped_trans.vx = bad_guy->position.vx;
			ped_trans.vz = bad_guy->position.vz;
			ped_trans.vy = (float)(-MapHeight(&ped_trans) - 224);

			x = (int)(ped_trans.vx - CarDoorPosition.vx);
			z = (int)(ped_trans.vz - CarDoorPosition.vz);

			dir = Arc_Tan(x, z) & 0xFFF;

			x = abs(x);
			z = abs(z);

			if (x >= z)
				dist = z / 2 + x;
			else
				dist = x / 2 + z;

			if (i == 0)
				current_distance = dist;

			if (dist >= 50)
			{
				busy = 1;

				cs = fcos(dir);
				sn = fsin(dir);

				num_frames = mcd_frames[bad_guy->type - 1];
				data = MC_Data[bad_guy->type - 1];
				packet = &data[bad_guy->num_frames * MOTION_FRAME_SIZE];

				// the motion's root offset of this frame, turned to the walking direction
				root = *(VECTOR4 *)packet;

				ped_trans.vx = root.vx * cs + root.vz * sn + ped_trans.vx;
				ped_trans.vy = ped_trans.vy + root.vy;
				ped_trans.vz = root.vz * cs - root.vx * sn + ped_trans.vz;

				dx = ped_trans.vx - camera_position.vx;
				dz = ped_trans.vz - camera_position.vz;

				x = abs(float_to_int(dx));
				z = abs(float_to_int(dz));

				if (x >= z)
					dist = z / 2 + x;
				else
					dist = x / 2 + z;

				if (dist <= 15000 || !drawing)
				{
					do_z_clipping = FrustrumCheck(&ped_trans, 800.0f);

					if (do_z_clipping)
					{
						rot.vx = 2048;
						rot.vy = dir;
						rot.vz = 0;

						memset(&root, 0, sizeof(VECTOR));

						EulerToQuaternion(&rot, &rot_quat);

						packet += sizeof(VECTOR4);
						model_base = ped3base;

						if (drawing)
							TrimmedWalkMotionObject((QUATERNION *)packet, hierarchy_tree, &rot_quat, &root);
					}

					// two frames of motion a step
					end_frame = bad_guy->num_frames + 2;

					for (j = bad_guy->num_frames; j < end_frame; j++)
					{
						if (j >= num_frames)
							frame_root = (float *)&data[j % num_frames * MOTION_FRAME_SIZE];
						else
							frame_root = (float *)&data[j * MOTION_FRAME_SIZE];

						if (!GamePaused && !drawing)
						{
							bad_guy->position.vx = cs * frame_root[0] + sn * frame_root[2] + bad_guy->position.vx;
							bad_guy->position.vz = cs * frame_root[2] - sn * frame_root[0] + bad_guy->position.vz;
						}
					}

					if (!GamePaused && !drawing)
						bad_guy->num_frames = (bad_guy->num_frames + 2) % num_frames;
				}
			}
			else
			{
				bad_guy->enabled = 0;
			}
		}

		bad_guy++;
	}

	if (!busy && !drawing && --CutSceneDelay == 0)
	{
		if (MCS_CameraPositionsSet < 2)
			CutToCar = 1;

		CutSceneActive = 0;
		MCS_CameraPositionsSet = 0;
	}
}
