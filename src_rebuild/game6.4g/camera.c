/*
 * camera.c - game cameras and fly-by camera paths
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "camera.c", 0x49d570 - 0x4a0f10).
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include "dr_keys.h"

#include "dr_types.h"
#include "camera.h"
#include "D3DGlide/glide_ptrs.h"
#include "asmmath.h"
#include "Director.h"
#include "dr_globals.h"
#include "Engine/3dfx/gfxinit.h"
#include "Engine/3dfx/object.h"
#include "handling.h"
#include "job_fx.h"
#include "input.h"
#include "map.h"
#include "minicutscene.h"
#include "MScript.h"
#include "shell.h"
#include "testmath.h"
#include "pause.h"
#include "Engine/3dfx/polydraw.h"
#include "ai.h"
#include "frontend/cheats.h"
#include "glaunch.h"

#define MAX_CONTROL_POINTS		32

#define CONTROL_POINT_SET		0x1		// CAMERA_POINT.flags: loaded from the file

#define FBFLAG_RUNNING			0x1

#define DIST_NORMAL				384.0f
#define DIST_PERSPECTIVE		1152.0f
#define DIST_ZOOM_MAX			2112.0f

#define NUM_CIV_CARS			65

#define ANGLE_MASK				0xFFF

//------------------------------------------------------------------------------------------
// uninitialised data
// NOTE: bss globals in the linker's common area (0x11b5dac - 0x11b66a8 and two at 0xd015d4,
// 0xd016a0); defined here as camera.c is their main user

int CAMERA_START_ELEVATION;
int CAMERA_DELTA;

char cameraback;
int TargetCar;
int CameraInCar;
char camerarotateleft;
float gCameraExpandSpeed;
int gCameraAngle;
char camera_has_changed;	// BUGGED: can be set, but never gets unset
int FBflags;
VECTOR4 holding_camera_position;
CAMERA_POINT control_points[MAX_CONTROL_POINTS];
float zoom;
int camera_drawing;
int current_camera_yangle;
int FBid;
int current_camera_xangle;
float gCameraMaxDistance;
int gHeadTurn;
char cameraview;
QUATERNION FBcamera_rotation;
char lastcameraview;
int step_increase;
char camerarotateright;
int FBframe;
int gHeadTurnTarget;
float gCameraDistance;
int main_display_height;
char cameraforward;
int last_control_point;
int screen_fade;
SVECTOR no_shake_camera;
int g_xo;
int g_yo;
int g_zo;
VECTOR4 CameraPos;
int title_fade;
SVECTOR holding_camera_angle;
char CamPerspectiveMode;
int wide_screen_height;

BOOL CameraCollisionCheck(int ground);
void ScalarInterp(int frame, float *t, int f0, int f1, int f2, int f3);
void sub_49FB7D(float t, float *out, float p0, float p1, float p2, float p3);
void sub_49FC32(float *t, VECTOR4 *out, VECTOR4 *p0, VECTOR4 *p1, VECTOR4 *p2, VECTOR4 *p3);
void sub_49FD51(float *t, VECTOR4 *out, VECTOR4 *p0, VECTOR4 *p1, VECTOR4 *p2, VECTOR4 *p3);

// NOTE: moved here from the generated stubs; no IDA mark, the owner is a guess from the users
// and the neighbouring data
VECTOR4 player_car_incar_camera_offsets[18] = {
	{ 0.0f, -100.0f, 95.0f, 0.0f },
	{ 0.0f, -105.0f, 135.0f, 0.0f },
	{ 0.0f, -80.0f, 120.0f, 0.0f },
	{ 0.0f, -90.0f, 120.0f, 0.0f },
	{ 0.0f, -90.0f, 80.0f, 0.0f },
	{ 0.0f, -85.0f, 115.0f, 0.0f },
	{ 0.0f, -95.0f, 115.0f, 0.0f },
	{ 0.0f, -80.0f, 125.0f, 0.0f },
	{ 0.0f, -90.0f, 110.0f, 0.0f },
	{ 0.0f, -85.0f, 120.0f, 0.0f },
	{ 0.0f, -75.0f, 130.0f, 0.0f },
	{ 0.0f, -90.0f, 115.0f, 0.0f },
	{ 0.0f, -95.0f, 90.0f, 0.0f },
	{ 0.0f, -90.0f, 110.0f, 0.0f },
	{ 0.0f, -90.0f, 100.0f, 0.0f },
	{ 0.0f, -90.0f, 125.0f, 0.0f },
	{ 0.0f, -95.0f, 47.0f, 0.0f },
	{ 0.0f, -90.0f, 120.0f, 0.0f },
};
MATRIX inv_camera_matrix;
MATRIX camera_matrix;

// [0x49d570] [D]
void InitCameraGlobals(void)
{
	current_camera_xangle = 0;
	current_camera_yangle = 2048;

	CAMERA_START_ELEVATION = -1024;
	CAMERA_DELTA = 824;

	CameraPos.vx = -45.0f;
	CameraPos.vy = -256.0f;
	CameraPos.vz = -160.0f;

	gCameraDistance = 1280.0f;
	gCameraMaxDistance = 1280.0f;
	gCameraExpandSpeed = 10.0f;

	CameraInCar = 0;
	gCameraAngle = 2048;
	TargetCar = 0;

	cameraview = CAMERA_FOLLOW;
	lastcameraview = CAMERA_FOLLOW;
	CamPerspectiveMode = 0;

	g_xo = 0;
	g_yo = 42000;
	g_zo = 0;
}

// [0x49d630] [D]
// Straight down over the player car (overhead view cheat).
void PlaceCameraGTA(void)
{
	float height;

	height = 28672.0f;

	if (far_clip - 2000.0f < 28672.0f)
		height = far_clip - 2000.0f;

	camera_position.vx = (float)car_data[0].location.vx;
	camera_position.vy = -(float)car_data[0].location.vy - height;
	camera_position.vz = (float)car_data[0].location.vz;

	camera_angle.vx = 1024;
	camera_angle.vy = 0;
	camera_angle.vz = 0;

	BuildWorldMatrix();

	cameraview = CAMERA_TOPDOWN;
}

// [0x49d6bc] [D]
// The top-down camera, moved with the arrow keys and up / down.
void ControlGTACamera(void)
{
	if (KeyMap[DIK_LEFTARROW])
		g_xo -= 512;
	else if (KeyMap[DIK_RIGHTARROW])
		g_xo += 512;

	if (movedown)
		g_yo -= 512;
	else if (moveup)
		g_yo += 512;

	if (KeyMap[DIK_UPARROW])
		g_zo += 512;
	else if (KeyMap[DIK_DOWNARROW])
		g_zo -= 512;

	camera_position.vx = (float)car_data[0].location.vx + (float)g_xo;
	camera_position.vy = -(float)car_data[0].location.vy - (float)g_yo;
	camera_position.vz = (float)car_data[0].location.vz + (float)g_zo;

	camera_angle.vx = 1024;
	camera_angle.vy = 0;
	camera_angle.vz = 0;

	BuildWorldMatrix();
}

// [0x49d7f4] [D]
// Pushes the camera out of the collision boxes of the objects in its cell (shortening
// gCameraDistance), else lets the distance grow back; ground: keep it 250 above the surface.
// Returns whether the camera was moved.
BOOL CameraCollisionCheck(int ground)
{
	int moved;
	int director_collision;
	float cellx_f;
	float cellz_f;
	int cellx;
	int cellz;
	int barrel;
	int cell;
	CELL_DATA *cdp;
	CELL_OBJECT *cop;
	MODEL *model;
	COLLISION_PACKET *packet;
	int num_packets;
	int i;
	int radius;
	int dx;
	int dz;
	MATRIX *m;
	float px;
	float py;
	float pz;
	float t;
	float xsize;
	float ysize;
	float zsize;
	float dist_xmin;
	float dist_xmax;
	float dist_zmin;
	float dist_zmax;
	float nearest;
	char side;
	float pushx;
	float pushz;
	float carx;
	float carz;
	float dist;
	IVECTOR pos;
	IVECTOR normal;
	IVECTOR surface;

	if (GamePaused && !Director.Mode)
		return FALSE;

	director_collision = ApplyDirectorCollision((int)camera_position.vx, (int)camera_position.vz);

	moved = 0;

	cellx_f = (camera_position.vx + (float)units_across_halved) * cell_recip;
	cellz_f = (camera_position.vz + (float)units_down_halved) * cell_recip;

	cellx = float_to_int(cellx_f);
	cellz = float_to_int(cellz_f);

	barrel = ((cellx / region_size) & 1) + 2 * ((cellz / region_size) & 1);
	cell = cell_ptrs[region_size * (cellz % region_size) + cellx % region_size + region_size * region_size * barrel];

	if (gCameraDistance > 0.0f)
	{
		while (cell != -1)
		{
			cdp = &cells[cell];
			cop = &cell_objects[cdp->num];
			model = model_pointers[cop->type];

			if (model->collision_block && (!director_collision || CheckDirectorCameraCollide(cop)))
			{
				radius = float_to_int(model->bounding_sphere2d);

				dx = float_to_int((float)fabs(cop->pos.vx - camera_position.vx));
				dz = float_to_int((float)fabs(cop->pos.vz - camera_position.vz));

				if (radius * radius > dz * dz + dx * dx)
				{
					packet = (COLLISION_PACKET *)(model->collision_block + 4);
					num_packets = *(int *)model->collision_block;

					for (i = 0; i < num_packets; i++, packet++)
					{
						// NOTE: the binary computes the position and radius of cylinders and
						// does nothing with them
						if (packet->type != COLLISION_BOX)
							continue;

						CurrentRotationMatrix = &(*matrix_mem)[cop->yang];

						// the camera in the box space
						m = &(*matrix_mem)[cop->yang & ANGLE_MASK];

						t = camera_position.vx - cop->pos.vx;
						pz = camera_position.vz - cop->pos.vz;

						px = t * m->m[0][0] + pz * m->m[2][0] - (float)packet->xpos;
						py = camera_position.vy - cop->pos.vy - (float)packet->ypos;
						pz = t * m->m[0][2] + pz * m->m[2][2] - (float)packet->zpos;

						if (packet->yang)
						{
							m = &(*matrix_mem)[(unsigned short)packet->yang & ANGLE_MASK];

							t = px;
							px = px * m->m[0][0] + pz * m->m[2][0];
							pz = t * m->m[0][2] + pz * m->m[2][2];
						}

						xsize = (float)(packet->xsize >> 1) + 50.0f;
						ysize = (float)(packet->ysize >> 1) + 50.0f;
						zsize = (float)(packet->zsize >> 1) + 50.0f;

						// only big boxes
						if ((xsize >= 300.0f || zsize >= 300.0f) &&
							px > -xsize && px < xsize &&
							py > -ysize && py < ysize &&
							pz > -zsize && pz < zsize)
						{
							// out through the nearest side
							dist_xmin = (float)fabs(px + xsize);
							dist_xmax = (float)fabs(px - xsize);
							dist_zmin = (float)fabs(pz + zsize);
							dist_zmax = (float)fabs(pz - zsize);

							if (dist_xmin >= dist_xmax)
							{
								side = 1;
								nearest = dist_xmax;
							}
							else
							{
								side = 0;
								nearest = dist_xmin;
							}

							if (dist_zmin >= dist_zmax)
							{
								if (dist_zmax < nearest)
									side = 3;
							}
							else if (dist_zmin < nearest)
							{
								side = 2;
							}

							switch (side)
							{
								case 0:
									pushx = -dist_xmin;
									pushz = 0.0f;
									break;
								case 1:
									pushx = dist_xmax;
									pushz = 0.0f;
									break;
								case 2:
									pushx = 0.0f;
									pushz = -dist_zmin;
									break;
								case 3:
									pushx = 0.0f;
									pushz = dist_zmax;
									break;
							}

							// back to world space
							m = &(*matrix_mem)[-(packet->yang + cop->yang) & ANGLE_MASK];

							camera_position.vx += pushx * m->m[0][0] + pushz * m->m[2][0];
							camera_position.vz += pushx * m->m[0][2] + pushz * m->m[2][2];

							carx = camera_position.vx - (float)car_data[0].location.vx;
							carz = camera_position.vz - (float)car_data[0].location.vz;

							dist = (float)sqrt(carx * carx + carz * carz);

							if (dist < gCameraDistance)
								gCameraDistance = dist;

							moved = 1;
						}
					}
				}
			}

			cell = cdp->next;
		}
	}

	if (!moved)
	{
		gCameraDistance += gCameraExpandSpeed;

		if (gCameraDistance > gCameraMaxDistance)
			gCameraDistance = gCameraMaxDistance;
	}

	if (ground)
	{
		pos.vx = float_to_int(camera_position.vx);
		pos.vy = float_to_int(camera_position.vy);
		pos.vz = float_to_int(camera_position.vz);

		FindSurface(&pos, &normal, &surface);

		if (-camera_position.vy < (float)(surface.vy + 250))
			camera_position.vy = -((float)surface.vy + 250.0f);
	}

	return moved;
}

// [0x49e3c4] [D]
// Lifts the camera over the cars (the player and the civilians) close to it.
void PlaceCameraFollowCivilians(void)
{
	float scale;
	CIV_STATE *ai;
	CAR_DATA *cp;
	int slot;
	float dist;
	float dx;
	float dz;
	float above;

	cp = car_data;
	ai = &civilian_ai_data[-1];

	for (slot = 0; slot < NUM_CIV_CARS; slot++)
	{
		// if (slot == 0 || ai->ctrlState)
		if (slot == 0 || ai->ctrlState)
		{
			dx = (float)cp->location.vx - camera_position.vx;
			dz = (float)cp->location.vz - camera_position.vz;

			dist = dx * dx + dz * dz;

			// within 1020
			if (dist <= 1040400.0f)
			{
				above = (float)(cp->location.vy + 300) + camera_position.vy;

				// NOTE: the original computes the scale once more before this test and drops it
				if (above > 0.0f)
				{
					if (1040400.0f - dist >= 250000.0f)
						scale = 250000.0f;
					else
						scale = 1040400.0f - dist;

					camera_position.vy -= (float)sqrt(scale / 250000.0f) * above;
				}
			}
		}

		cp++;
		ai++;
	}
}

// [0x49e56a] [D] [A]
// The chase camera behind car id.
// NOTE: the head turn goes first, so the offset is turned before the tilt and the camera stays on
// the slope. The original turned it after the tilt: looking back, the camera in front of the car
// went down on a climb and up on a descent.
void PlaceCameraFollowCar(int id)
{
	float dx;
	MATRIX m;
	CAR_DATA *cp;
	VECTOR4 locCameraPos;
	LONG len;
	LONG ix;
	LONG iy;
	LONG iz;
	int new_angle;
	float dz;
	float dy;

	cp = &car_data[id];

	if (!GamePaused && !camera_drawing)
	{
		if (cameraforward)
		{
			gCameraDistance -= 40.0f;

			if (gCameraDistance < 1.0f)
				gCameraDistance = 1.0f;
		}

		if (cameraback)
			gCameraDistance += 40.0f;

		if (camerarotateleft)
			gCameraAngle = (gCameraAngle - 22) & ANGLE_MASK;

		if (camerarotateright)
			gCameraAngle = (gCameraAngle + 22) & ANGLE_MASK;
	}

	new_angle = DIFF_ANGLES(current_camera_yangle, cp->direction + gCameraAngle);

	if ((!GamePaused || Director.Mode) && !camera_drawing)
	{
		if (Director.FollowCar)
			current_camera_yangle = (current_camera_yangle + new_angle / 8) & ANGLE_MASK;
		else
			current_camera_yangle = gCameraAngle + cp->direction;
	}

	new_angle = DIFF_ANGLES(current_camera_xangle, -cp->rotation.vx & ANGLE_MASK);

	if ((!GamePaused || Director.Mode) && !camera_drawing)
		current_camera_xangle = (current_camera_xangle + new_angle / 8) & ANGLE_MASK;

	locCameraPos.vx = 0.0f;
	locCameraPos.vy = CameraPos.vy;
	locCameraPos.vz = gCameraDistance;

	m.m[0][0] = 1.0f;
	m.m[0][1] = 0.0f;
	m.m[0][2] = 0.0f;
	m.m[1][0] = 0.0f;
	m.m[1][1] = 1.0f;
	m.m[1][2] = 0.0f;
	m.m[2][0] = 0.0f;
	m.m[2][1] = 0.0f;
	m.m[2][2] = 1.0f;

	RotMatrixY(&m, (gHeadTurn >> 16) & ANGLE_MASK);

	if (!TriggerBomb)
		RotMatrixX(&m, current_camera_xangle & ANGLE_MASK);

	RotMatrixY(&m, -current_camera_yangle & ANGLE_MASK);

	CurrentRotationMatrix = &m;
	MatrixRotate(&locCameraPos);

	camera_position.vx = (float)cp->location.vx + locCameraPos.vx;
	camera_position.vy = (float)-cp->location.vy + locCameraPos.vy;
	camera_position.vz = (float)cp->location.vz + locCameraPos.vz;

	CameraCollisionCheck(1);

	// look at the car
	dx = (float)cp->location.vx - camera_position.vx;
	dy = (float)-cp->location.vy - camera_position.vy;
	dz = (float)cp->location.vz - camera_position.vz;

	len = float_to_int((float)sqrt(dx * dx + dz * dz));
	ix = float_to_int(dx);
	iy = float_to_int(dy);
	iz = float_to_int(dz);

	camera_angle.vx = (1024 - Arc_Tan(len, iy)) & ANGLE_MASK;
	camera_angle.vy = -Arc_Tan(ix, iz) & ANGLE_MASK;
	camera_angle.vz = 0;

	BuildWorldMatrix();
}

// [0x49e993] [D]
// The tripod camera: moves a tenth of the way to CameraPos each frame, looking at car id.
void PlaceCameraAtLocation(int id)
{
	int dx;
	float fx;
	CAR_DATA *cp;
	LONG iz;
	LONG len;
	LONG iy;
	LONG ix;
	int dz;
	float fz;
	int dy;
	float fy;

	cp = &car_data[id];

	if (cameraforward)
		CameraPos.vz += 32.0f;

	if (cameraback)
		CameraPos.vz -= 32.0f;

	if (camerarotateleft)
		CameraPos.vx -= 32.0f;

	if (camerarotateright)
		CameraPos.vx += 32.0f;

	dx = float_to_int(CameraPos.vx - camera_position.vx);
	dy = float_to_int(CameraPos.vy - camera_position.vy);
	dz = float_to_int(CameraPos.vz - camera_position.vz);

	if ((!GamePaused || Director.Mode) && !camera_drawing)
	{
		camera_position.vx += (float)(dx / 10);
		camera_position.vy += (float)(dy / 10);
		camera_position.vz += (float)(dz / 10);
	}

	fx = (float)cp->location.vx - camera_position.vx;
	fy = camera_position.vy + (float)cp->location.vy;
	fz = (float)cp->location.vz - camera_position.vz;

	len = float_to_int((float)sqrt(fx * fx + fz * fz)) << 8;
	ix = float_to_int(fx) << 8;
	iy = float_to_int(fy) << 8;
	iz = float_to_int(fz) << 8;

	while (len > 64000)
	{
		len /= 4;
		ix /= 4;
		iy /= 4;
		iz /= 4;
	}

	camera_angle.vx = (Arc_Tan(len, iy) - 1024) & ANGLE_MASK;
	camera_angle.vy = -Arc_Tan(ix, iz) & ANGLE_MASK;
	camera_angle.vz = 0;

	BuildWorldMatrix();
}

// [0x49ecb1] [D]
// Straight down from 400 above car id.
void PlaceCameraAtCar(int id)
{
	float fx;
	LONG iz;
	LONG len;
	LONG iy;
	LONG ix;
	float fz;
	float fy;

	camera_position.vx = (float)car_data[id].location.vx;
	camera_position.vy = (float)car_data[id].location.vy - 400.0f;
	camera_position.vz = (float)car_data[id].location.vz;

	fx = (float)car_data[id].location.vx - camera_position.vx;
	fy = camera_position.vy + (float)car_data[id].location.vy;
	fz = (float)car_data[id].location.vz - camera_position.vz;

	len = float_to_int((float)sqrt(fx * fx + fz * fz)) << 8;
	ix = float_to_int(fx) << 8;
	iy = float_to_int(fy) << 8;
	iz = float_to_int(fz) << 8;

	while (len > 64000)
	{
		len /= 4;
		ix /= 4;
		iy /= 4;
		iz /= 4;
	}

	camera_angle.vx = (Arc_Tan(len, iy) - 1024) & ANGLE_MASK;
	camera_angle.vy = -Arc_Tan(ix, iz) & ANGLE_MASK;
	camera_angle.vz = 0;

	BuildWorldMatrix();
}

// [0x49ee8c] [D]
// A replay camera at the stored position: with its stored angle (id -1), or looking at car id
// with the zoom following the distance in perspective mode.
// NOTE: written from the disassembly (Hex-Rays loses the distance); the binary first computes
// an approximate distance to the player car and overwrites it unused
void ControlReplayCamera(int id)
{
	float fx;
	float fy;
	float fz;
	float dist;
	LONG len;
	LONG ix;
	LONG iy;
	LONG iz;

	camera_position.vx = holding_camera_position.vx;
	camera_position.vy = holding_camera_position.vy;
	camera_position.vz = holding_camera_position.vz;

	CameraCollisionCheck(0);

	if (id == -1)
	{
		camera_angle.vx = holding_camera_angle.vx;
		camera_angle.vy = holding_camera_angle.vy;
		camera_angle.vz = holding_camera_angle.vz;

		if (CamPerspectiveMode)
			DIST = DIST_PERSPECTIVE;
		else
			DIST = DIST_NORMAL;
	}
	else
	{
		fx = (float)car_data[id].location.vx - camera_position.vx;
		fy = camera_position.vy + (float)car_data[id].location.vy;
		fz = (float)car_data[id].location.vz - camera_position.vz;

		dist = (float)sqrt(fx * fx + fy * fy + fz * fz);

		len = float_to_int(dist) << 8;
		ix = float_to_int(fx) << 8;
		iy = float_to_int(fy) << 8;
		iz = float_to_int(fz) << 8;

		while (len > 64000)
		{
			len >>= 2;
			ix >>= 2;
			iy >>= 2;
			iz >>= 2;
		}

		camera_angle.vx = (Arc_Tan(len, iy) - 1024) & ANGLE_MASK;
		camera_angle.vy = -Arc_Tan(ix, iz) & ANGLE_MASK;
		camera_angle.vz = 0;

		if (CamPerspectiveMode)
		{
			DIST = (float)fabs(dist * 0.2f);

			if (DIST < DIST_NORMAL)
				DIST = DIST_NORMAL;
			else if (DIST > DIST_ZOOM_MAX)
				DIST = DIST_ZOOM_MAX;
		}
		else
		{
			DIST = DIST_NORMAL;
		}
	}

	BuildWorldMatrix();
}

// [0x49f19b] [D] [A]
// On the front bumper of the player car.
// NOTE: the head turn goes first, as in PlaceCameraInCar.
void PlaceCameraBumper(void)
{
	VECTOR4 offset;

	camera_angle.vx = car_data[0].rotation.vx & ANGLE_MASK;
	camera_angle.vy = ((gHeadTurn >> 16) - car_data[0].rotation.vy) & ANGLE_MASK;
	camera_angle.vz = car_data[0].rotation.vz & ANGLE_MASK;

	camera_matrix.m[0][0] = 1.0f;
	camera_matrix.m[1][0] = 0.0f;
	camera_matrix.m[1][1] = 1.0f;
	camera_matrix.m[2][0] = 0.0f;
	camera_matrix.m[0][1] = 0.0f;
	camera_matrix.m[2][1] = 0.0f;
	camera_matrix.m[2][2] = 1.0f;
	camera_matrix.m[0][2] = 0.0f;
	camera_matrix.m[1][2] = 0.0f;

	RotMatrixY(&camera_matrix, (gHeadTurn >> 16) & ANGLE_MASK);
	RotMatrixZ(&camera_matrix, car_data[0].rotation.vz & ANGLE_MASK);
	RotMatrixX(&camera_matrix, camera_angle.vx);
	RotMatrixY(&camera_matrix, -car_data[0].rotation.vy & ANGLE_MASK);

	InvertMatrix(&camera_matrix, &inv_camera_matrix);

	CurrentRotationMatrix = &camera_matrix;

	offset.vx = 0.0f;
	offset.vy = 60.0f;
	offset.vz = 350.0f;

	MatrixRotate(&offset);

	camera_position.vx = (float)car_data[0].location.vx + offset.vx;
	camera_position.vy = (float)-car_data[0].location.vy + offset.vy;
	camera_position.vz = (float)car_data[0].location.vz + offset.vz;
}

// [0x49f307] [D] [A]
// Inside car id (the player car at the driver's seat of its model).
// NOTE: the head turns inside the car, as in Driver 2, so its turn goes first. The original turned
// it after the car's pitch and roll: looking back they were inverted, to the side they were swapped.
// camera_angle keeps the car's angles, it no longer describes camera_matrix exactly.
CAR_DATA *PlaceCameraInCar(int id)
{
	VECTOR4 position;
	CAR_DATA *cp;

	cp = &car_data[id];

	camera_angle.vx = cp->rotation.vx & ANGLE_MASK;
	camera_angle.vy = ((gHeadTurn >> 16) - cp->rotation.vy) & ANGLE_MASK;
	camera_angle.vz = cp->rotation.vz & ANGLE_MASK;

	camera_matrix.m[0][0] = 1.0f;
	camera_matrix.m[1][0] = 0.0f;
	camera_matrix.m[1][1] = 1.0f;
	camera_matrix.m[2][0] = 0.0f;
	camera_matrix.m[0][1] = 0.0f;
	camera_matrix.m[2][1] = 0.0f;
	camera_matrix.m[2][2] = 1.0f;
	camera_matrix.m[0][2] = 0.0f;
	camera_matrix.m[1][2] = 0.0f;

	RotMatrixY(&camera_matrix, (gHeadTurn >> 16) & ANGLE_MASK);
	RotMatrixZ(&camera_matrix, camera_angle.vz);
	RotMatrixX(&camera_matrix, camera_angle.vx);
	RotMatrixY(&camera_matrix, -cp->rotation.vy & ANGLE_MASK);

	InvertMatrix(&camera_matrix, &inv_camera_matrix);

	CurrentRotationMatrix = &camera_matrix;

	if (id)
	{
		position.vx = 5.0f;
		position.vy = -115.0f;
		position.vz = 150.0f;
	}
	else
	{
		position = player_car_incar_camera_offsets[gPlayerCarModelType];
	}

	MatrixRotate(&position);

	camera_position.vx = (float)cp->location.vx + position.vx;
	camera_position.vy = (float)-cp->location.vy + position.vy;
	camera_position.vz = (float)cp->location.vz + position.vz;

	return cp;
}

// [0x49f4c8] [D]
// Blown away from the explosion centre and shaken while it lasts.
void ControlCameraDuringExplosion(void)
{
	int shake;
	short r;
	float dz;
	float dx;
	float push;
	float dist;

	camera_angle = no_shake_camera;

	if (!GamePaused || Director.Mode)
	{
		dx = camera_position.vx - ex_centre.vx;
		dz = camera_position.vz - ex_centre.vz;

		dist = (float)sqrt(dx * dx + dz * dz);
		push = (3000.0f - dist) / (20.0f * dist);

		camera_position.vx += dx * push;
		camera_position.vz += dz * push;

		CameraCollisionCheck(1);
	}

	if (ex_time < 48)
	{
		r = rand();
		shake = (48 - ex_time) >> 1;

		if ((r & 0x3F) > ex_time)
		{
			if (r & 0x2000)
				camera_angle.vx += shake;

			if (r & 0x1000)
				camera_angle.vx -= shake;

			if (r & 0x800)
				camera_angle.vy += shake;

			if (r & 0x400)
				camera_angle.vy -= shake;
		}
	}

	BuildWorldMatrix();
}

// [0x49f631] [D]
// The fly-by of the mission (SCRIPTS\FLYBY\MISSION<n>.FBD) before it starts.
void InitialiseFlyBy(void)
{
	char filename[32];
	int i;

	FlyBy = 1;
	EndofFlyBy = 0;

	for (i = 0; i < MAX_CONTROL_POINTS; i++)
		control_points[i].flags = 0;

	FBflags = 0;

	sprintf(filename, "SCRIPTS\\FLYBY\\MISSION%d.FBD", GameMode);

	if (LoadFlyByData(filename))
	{
		FBflags |= FBFLAG_RUNNING;

		FBid = 1;
		FBframe = control_points[1].frame;
		camera_position = control_points[1].position;

		// the letterbox opens towards the end (heights in 1/4096)
		wide_screen_height = 3072 * display_height;
		step_increase = 32 * display_height - ((3072 * display_height) >> 7);
		title_fade = 204;
		main_display_height = display_height;
	}
	else
	{
		FlyBy = 0;
	}
}

// [0x49f757] [D]
void RunFlyBy(void)
{
	if (mission.inform_count)
	{
		mission.inform_count--;

		if (title_fade)
			title_fade -= 4;
	}
	else if (!KeyMap[DIK_ESCAPE] && (control_points[FBid + 2].flags & CONTROL_POINT_SET))
	{
		while (FBframe == control_points[FBid + 1].frame)
		{
			if (!(control_points[++FBid + 2].flags & CONTROL_POINT_SET))
			{
				FBflags = 0;
				EndofFlyBy = 1;
				current_camera_yangle = (gCameraAngle + car_data[0].direction) & ANGLE_MASK;
				wide_screen_height = main_display_height << 12;
				return;
			}
		}

		SetCameraOnSpline(FBframe, &control_points[FBid]);

		if (control_points[last_control_point].frame < FBframe + 128)
		{
			wide_screen_height += step_increase;

			if (wide_screen_height > main_display_height << 12)
				wide_screen_height = main_display_height << 12;
		}

		screen_fade -= 2;

		if (screen_fade < 0)
			screen_fade = 0;

		FBframe++;
	}
	else
	{
		FBflags = 0;
		EndofFlyBy = 1;
		current_camera_yangle = (gCameraAngle + car_data[0].direction) & ANGLE_MASK;
		wide_screen_height = main_display_height << 12;
		camera_has_changed = 1;		// BUG: once it's set, it never gets unset!
	}
}

// [0x49f921] [D]
// The camera at frame between points[0] and points[1] (Catmull-Rom over points[-1] .. [2]).
void SetCameraOnSpline(int frame, CAMERA_POINT *points)
{
	float t;
	VECTOR4 position;
	QUATERNION rotation;
	float zoom_value;

	ScalarInterp(frame, &t, points[-1].frame, points[0].frame, points[1].frame, points[2].frame);

	sub_49FC32(&t, &position, &points[-1].position, &points[0].position, &points[1].position, &points[2].position);
	sub_49FD51(&t, &rotation.v, &points[-1].rotation.v, &points[0].rotation.v, &points[1].rotation.v, &points[2].rotation.v);
	sub_49FB7D(t, &zoom_value, points[-1].zoom, points[0].zoom, points[1].zoom, points[2].zoom);

	NormQuaternion(&rotation);
	QuaternionToWorldMatrix(&rotation);

	DIST = zoom_value;
	zoom = zoom_value;
	camera_position = position;
	FBcamera_rotation = rotation;
}

// [0x49fa40] [D]
// The spline parameter (0 at f1, 1 at f2) of frame, from a cubic through the four key frames.
// NOTE: IDA types it int; the value is a leftover (f2 - f1 cubed) no caller uses
void ScalarInterp(int frame, float *t, int f0, int f1, int f2, int f3)
{
	int d0;
	int d2;
	int d3;
	float k;
	float a;
	float b;
	float c;
	float x;

	d0 = f0 - f1;
	d2 = f2 - f1;
	d3 = f3 - f1;

	if (d2 == d3)
		d3 += d2;

	if (d0 == 0)
		d0 = -d2;

	k = 2.0f * (float)(d3 + d2 - d0) / (float)((d2 - d0) * d3);

	a = 2.0f / (float)(d2 - d0);
	b = (3.0f - (float)d2 * (a + k)) / (float)(d2 * d2);
	c = (k * (float)d2 - 2.0f) / (float)(d2 * d2 * d2);

	x = (float)(frame - f1);

	*t = a * x + b * x * x + c * x * x * x;
}

// [0x49fb7d] [D]
// NOTE: no name in the database; Catmull-Rom interpolation of a float
void sub_49FB7D(float t, float *out, float p0, float p1, float p2, float p3)
{
	float t3;
	float t2;
	float w3;
	float w2;
	float w1;
	float w0;

	t2 = t * t;
	t3 = t2 * t;

	w0 = t2 - 0.5f * t3 - 0.5f * t;
	w1 = 1.5f * t3 - 2.5f * t2 + 1.0f;
	w2 = 2.0f * t2 - 1.5f * t3 + 0.5f * t;
	w3 = 0.5f * t3 - 0.5f * t2;

	*out = w0 * p0 + w1 * p1 + w2 * p2 + w3 * p3;
}

// [0x49fc32] [D]
// NOTE: no name in the database; Catmull-Rom interpolation of a position (xyz)
void sub_49FC32(float *t, VECTOR4 *out, VECTOR4 *p0, VECTOR4 *p1, VECTOR4 *p2, VECTOR4 *p3)
{
	float t3;
	float t2;
	float w3;
	float w2;
	float w1;
	float w0;

	t2 = *t * *t;
	t3 = t2 * *t;

	w0 = t2 - 0.5f * t3 - 0.5f * *t;
	w1 = 1.5f * t3 - 2.5f * t2 + 1.0f;
	w2 = 2.0f * t2 - 1.5f * t3 + 0.5f * *t;
	w3 = 0.5f * t3 - 0.5f * t2;

	out->vx = w0 * p0->vx + w1 * p1->vx + w2 * p2->vx + w3 * p3->vx;
	out->vy = w0 * p0->vy + w1 * p1->vy + w2 * p2->vy + w3 * p3->vy;
	out->vz = w0 * p0->vz + w1 * p1->vz + w2 * p2->vz + w3 * p3->vz;
}

// [0x49fd51] [D]
// NOTE: no name in the database; Catmull-Rom interpolation of a quaternion: the xyz of the
// VECTOR4 and the float after it (QUATERNION.s)
void sub_49FD51(float *t, VECTOR4 *out, VECTOR4 *p0, VECTOR4 *p1, VECTOR4 *p2, VECTOR4 *p3)
{
	float t3;
	float t2;
	float w3;
	float w2;
	float w1;
	float w0;

	t2 = *t * *t;
	t3 = t2 * *t;

	w0 = t2 - 0.5f * t3 - 0.5f * *t;
	w1 = 1.5f * t3 - 2.5f * t2 + 1.0f;
	w2 = 2.0f * t2 - 1.5f * t3 + 0.5f * *t;
	w3 = 0.5f * t3 - 0.5f * t2;

	out[1].vx = w0 * p0[1].vx + w1 * p1[1].vx + w2 * p2[1].vx + w3 * p3[1].vx;
	out->vx = w0 * p0->vx + w1 * p1->vx + w2 * p2->vx + w3 * p3->vx;
	out->vy = w0 * p0->vy + w1 * p1->vy + w2 * p2->vy + w3 * p3->vy;
	out->vz = w0 * p0->vz + w1 * p1->vz + w2 * p2->vz + w3 * p3->vz;
}

// [0x49fea0] [D]
// NOTE: no name in the database and no callers; rotates v by q (q * v * q^-1). IDA types it
// int (the last word of v) and v as int *
void sub_49FEA0(VECTOR4 *v, QUATERNION *q)
{
	QUATERNION inv;
	QUATERNION p;

	p.s = 0.0f;
	p.v = *v;

	inv = *q;
	QuaternionInverse(&inv);

	MulQuaternions(&p, &inv, &p);
	MulQuaternions(q, &p, &p);

	*v = p.v;
}

// [A]
// NOTE: the game runs the FPU with chop rounding (FPUInit); the current CRT parses floats only in
// round-to-nearest mode (debug builds assert "unexpected rounding mode"), so scanf switches to
// it for the call (not needed while OPENDRIVER_NO_FPU_CHOP is set, see asmmath.c)
static int fscanf_nearest(FILE *f, const char *format, ...)
{
	unsigned int old_control;
	unsigned int control;
	va_list args;
	int result;

	_controlfp_s(&old_control, 0, 0);
	_controlfp_s(&control, _RC_NEAR, _MCW_RC);

	va_start(args, format);
	result = vfscanf(f, format, args);
	va_end(args);

	_controlfp_s(&control, old_control, _MCW_RC);

	return result;
}

// [0x49ff2a] [D] [A]
// Reads a fly-by camera path (text file). The file name buffer is reused for reading lines.
// NOTE: [A] fscanf_nearest instead of fscanf
int LoadFlyByData(char *text)
{
	FILE *f;
	int id;
	int tokens;

	f = fopen(text, "rt");

	if (!f)
		return 0;

	// NB: hacks like these are bad, kids!
	// check header
	// NOTE: the file is left open on a bad header or point, as in the original
	fgets(text, 30, f);

	if (strcmp(text, "DRIVER Fly-By Text-file\n"))
		return 0;

	// skip empty lines
	fscanf(f, "\n");

	// fill buffer to check for end marker
	fgets(text, 14, f);

	// parse until end marker is found
	while (strcmp(text, "End of Fly-By"))
	{
		tokens = 0;

		// control point definition (ignores any prefix text)
		// BUG: no bounds check!
		tokens += fscanf_nearest(f, "%d\n", &id);

		tokens += fscanf_nearest(f, "  Position: %f, %f, %f\n",
			&control_points[id].position.vx, &control_points[id].position.vy, &control_points[id].position.vz);

		tokens += fscanf_nearest(f, "  Rotation: %f, %f, %f, %f\n",
			&control_points[id].rotation.s, &control_points[id].rotation.v.vx,
			&control_points[id].rotation.v.vy, &control_points[id].rotation.v.vz);

		tokens += fscanf_nearest(f, "  Zoom:     %f\n", &control_points[id].zoom);
		tokens += fscanf_nearest(f, "  Frame:    %d\n\n", &control_points[id].frame);

		// validate number of tokens
		if (tokens != 10)
			return 0;

		control_points[id].flags |= CONTROL_POINT_SET;

		// fill buffer to check for end marker
		fgets(text, 14, f);
	}

	// the last point is doubled for the spline
	control_points[id + 1] = control_points[id];

	fclose(f);

	last_control_point = id + 1;

	return 1;
}

// [0x4a0138] [D]
void QuaternionToWorldMatrix(QUATERNION *q)
{
	Quaternion2Matrix(q, &camera_matrix);

	camera_angle.vy = Arc_Tan((LONG)(-camera_matrix.m[2][0] * 100000.0f), (LONG)(camera_matrix.m[2][2] * 100000.0f));

	InvertMatrix(&camera_matrix, &inv_camera_matrix);
}

// [0x4a0194] [D]
// The end of a fly-by frame: the mission title on black, the fade, the letterbox.
void FBEndOfFrame(void)
{
	GrVertex vC;
	GrVertex vB;
	GrVertex vA;
	int title;

	title = 0;

	if (mission.inform_count)
	{
		grBufferClear(0, 0, 0xFFFF);

		DrawMissionText(mission.text);

		title = 1;
		screen_fade = title_fade;
	}

	if (screen_fade)
	{
		// UNTEXTURED FLATSTATE
		if (last_state)
		{
			grColorCombine(GR_COMBINE_FUNCTION_LOCAL, GR_COMBINE_FACTOR_ZERO, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_CONSTANT, FXFALSE);
			last_state = FLAT3;
		}

		// TRANSPARENCY 2
		if (last_trans_on != 2)
		{
			grAlphaBlendFunction(GR_BLEND_ZERO, GR_BLEND_ONE_MINUS_SRC_COLOR, GR_BLEND_ONE, GR_BLEND_ZERO);
			last_trans_on = 2;
		}

		vC.b = (float)screen_fade;
		vC.g = vC.b;
		vC.r = vC.b;
		vB.b = vC.b;
		vB.g = vC.b;
		vB.r = vC.b;
		vA.b = vC.b;
		vA.g = vC.b;
		vA.r = vC.b;

		vA.x = 0.0f;
		vA.y = 0.0f;
		vB.x = (float)display_width;
		vB.y = (float)main_display_height;
		vC.y = vB.y;
		vC.x = 0.0f;

		if (using_glide)
		{
			vA.oow = 1.0f;
			vB.oow = 1.0f;
			vC.oow = 1.0f;
		}
		else
		{
			vA.oow = 1.0f / 64.0f;
			vB.oow = 1.0f / 64.0f;
			vC.oow = 1.0f / 64.0f;
		}

		grDepthBufferMode(GR_DEPTHBUFFER_DISABLE);
		guDrawTriangleWithClip(&vA, &vB, &vC);

		vC.x = (float)display_width;
		vC.y = 0.0f;
		guDrawTriangleWithClip(&vA, &vB, &vC);
		grDepthBufferMode(GR_DEPTHBUFFER_WBUFFER);
	}

	if (title)
	{
		grBufferSwap(0);
		screen_fade = 255;
		return;
	}

	grBufferSwap(0);

	if (display_height != wide_screen_height >> 12)
	{
		display_height = wide_screen_height >> 12;
		ResetViewport(0, (main_display_height - (wide_screen_height >> 12)) / 2, 0);
	}

	if (viewport_reset)
	{
		ResetViewport(0, (main_display_height - display_height) / 2, 1);
		viewport_reset--;
	}

	current_object_computed_value++;
}

// [0x4a03e7] [D]
// Places the camera of the current cameraview; drawing: a second view of the same frame (no
// input, no smoothing).
void ControlCamera(int drawing)
{
	float dx;
	float dz;

	if (gCheatFlags.OverheadView && !Director.Mode)
	{
		PlaceCameraGTA();
		return;
	}

	camera_drawing = drawing;

	if (CutSceneActive)
		cameraview = CAMERA_CUTSCENE;

	if ((!GamePaused || Director.Mode) && !camera_drawing)
	{
		gHeadTurn = gHeadTurnTarget;

		if (moveup)
		{
			camera_position.vy -= 10.0f;
			CameraPos.vy -= 10.0f;
		}

		if (movedown)
		{
			camera_position.vy += 10.0f;
			CameraPos.vy += 10.0f;
		}
	}

	if (gDoTraining && !gTrainingMode)
	{
		CamPerspectiveMode = 0;
		DIST = DIST_NORMAL;
		PlaceCameraFollowCar(65);
		return;
	}

	switch (cameraview)
	{
		case CAMERA_FOLLOW:
			CamPerspectiveMode = 0;
			DIST = DIST_NORMAL;
			PlaceCameraFollowCar(TargetCar);
			break;
		case CAMERA_TRIPOD:
			CamPerspectiveMode = 0;
			DIST = DIST_NORMAL;
			PlaceCameraAtLocation(TargetCar);
			CameraPos.vy = (float)-MapHeight(&camera_position) - 512.0f;
			gCameraDistance = 1280.0f;
			break;
		case CAMERA_INCAR:
			CamPerspectiveMode = 0;
			DIST = DIST_NORMAL;
			PlaceCameraInCar(CameraInCar);
			break;
		case CAMERA_TOPDOWN:
			ControlGTACamera();
			break;
		case CAMERA_CUTSCENE:
			PlaceCameraCutScene();
			break;
		case CAMERA_BUMPER:
			PlaceCameraBumper();
			break;
		case CAMERA_EXPLOSION:
			ControlCameraDuringExplosion();
			break;
		case CAMERA_REPLAY_FIXED:
			ControlReplayCamera(-1);
			break;
		case CAMERA_REPLAY_TARGET:
			ControlReplayCamera(TargetCar);
			break;
	}

	// back to the chase camera when more than 45000 from the player car
	dx = (float)fabs((float)car_data[0].location.vx - camera_position.vx);
	dz = (float)fabs((float)car_data[0].location.vz - camera_position.vz);

	if (dx * dx + dz * dz > 2025000000.0f)
	{
		cameraview = CAMERA_FOLLOW;
		CameraPos.vy = -256.0f;
		gCameraDistance = 1280.0f;
		gCameraAngle = 2048;
	}
}

// [0x4a0694] [D]
// Whether the camera may move to (x, y, z): not into, nor through, a big collision box of the
// objects in the target and the current cell.
int CameraMoveAllowed(float x, float y, float z)
{
	float cellx_f;
	float cellz_f;
	int cellx;
	int cellz;
	int barrel;
	int target_cell;
	int camera_cell;
	int next;
	int pass;
	int i;
	CELL_DATA *cdp;
	CELL_OBJECT *cop;
	MODEL *model;
	COLLISION_PACKET *packet;
	int num_packets;
	MATRIX *m;
	float cx;
	float cz;
	float tx;
	float ty;
	float tz;
	float t;
	float xsize;
	float ysize;
	float zsize;
	IVECTOR box;
	XYPAIR from;
	XYPAIR to;
	XYPAIR hit1;
	XYPAIR hit2;

	cellx_f = (x + (float)units_across_halved) * cell_recip;
	cellz_f = (z + (float)units_down_halved) * cell_recip;

	cellx = float_to_int(cellx_f);
	cellz = float_to_int(cellz_f);

	barrel = ((cellx / region_size) & 1) + 2 * ((cellz / region_size) & 1);
	target_cell = cell_ptrs[region_size * (cellz % region_size) + cellx % region_size + region_size * region_size * barrel];

	cellx_f = (camera_position.vx + (float)units_across_halved) * cell_recip;
	cellz_f = (camera_position.vz + (float)units_down_halved) * cell_recip;

	cellx = float_to_int(cellx_f);
	cellz = float_to_int(cellz_f);

	barrel = ((cellx / region_size) & 1) + 2 * ((cellz / region_size) & 1);
	camera_cell = cell_ptrs[region_size * (cellz % region_size) + cellx % region_size + region_size * region_size * barrel];

	next = target_cell;

	for (pass = 0; pass < 2; pass++)
	{
		while (next != -1)
		{
			cdp = &cells[next];
			cop = &cell_objects[cdp->num];
			model = model_pointers[cop->type];

			if (model->collision_block)
			{
				packet = (COLLISION_PACKET *)(model->collision_block + 4);
				num_packets = *(int *)model->collision_block;

				for (i = 0; i < num_packets; i++, packet++)
				{
					if (packet->type != COLLISION_BOX)
						continue;

					// the camera and the target in the box space (the camera height is computed and not
					// used in the original)
					m = &(*matrix_mem)[cop->yang & ANGLE_MASK];

					t = camera_position.vx - cop->pos.vx;
					cz = camera_position.vz - cop->pos.vz;

					cx = t * m->m[0][0] + cz * m->m[2][0] - (float)packet->xpos;
					cz = t * m->m[0][2] + cz * m->m[2][2] - (float)packet->zpos;

					t = x - cop->pos.vx;
					tz = z - cop->pos.vz;

					tx = t * m->m[0][0] + tz * m->m[2][0] - (float)packet->xpos;
					ty = y - cop->pos.vy - (float)packet->ypos;
					tz = t * m->m[0][2] + tz * m->m[2][2] - (float)packet->zpos;

					if (packet->yang)
					{
						m = &(*matrix_mem)[(unsigned short)packet->yang & ANGLE_MASK];

						t = cx;
						cx = cx * m->m[0][0] + cz * m->m[2][0];
						cz = t * m->m[0][2] + cz * m->m[2][2];

						t = tx;
						tx = tx * m->m[0][0] + tz * m->m[2][0];
						tz = t * m->m[0][2] + tz * m->m[2][2];
					}

					xsize = (float)((packet->xsize >> 1) + 2);
					ysize = (float)((packet->ysize >> 1) + 2);
					zsize = (float)((packet->zsize >> 1) + 2);

					if (xsize >= 300.0f || zsize >= 300.0f)
					{
						if (tx > -xsize && tx < xsize && tz > -zsize && tz < zsize && ty > -ysize && ty < ysize)
							return 0;

						if (ty < ysize && ty > -ysize)
						{
							box.vx = float_to_int(-xsize);
							box.vz = float_to_int(xsize);
							box.vy = float_to_int(-zsize);
							box.vw = float_to_int(zsize);

							from.x = float_to_int(cx);
							from.y = float_to_int(cz);
							to.x = float_to_int(tx);
							to.y = float_to_int(tz);

							if (LineBoxIntersect(&box, &from, &to, &hit1, &hit2))
								return 0;
						}
					}
				}
			}

			next = cdp->next;
		}

		next = camera_cell;
	}

	return 1;
}
