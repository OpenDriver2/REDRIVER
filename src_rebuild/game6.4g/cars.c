/*
 * cars.c - drawing the cars: body, wheels, lights and smoke
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "cars.c", 0x4a27d0 - 0x4a3daf)
 */

#include <stdlib.h>
#include <string.h>

#include "dr_types.h"
#include "dr_globals.h"
#include "handling.h"
#include "asmmath.h"
#include "testmath.h"
#include "Engine/3dfx/object.h"
#include "D3DGlide/glide_ptrs.h"
#include "camera.h"
#include "debris.h"
#include "denting.h"
#include "dr_cars.h"
#include "general.h"
#include "map.h"
#include "roadbits.h"
#include "shadow.h"
#include "shell.h"
#include "pause.h"
#include "cops.h"

// the radius of a wheel model's rim vertices (DrawCarWheels)
#define WHEEL_RADIUS	72.0f

//------------------------------------------------------------------------------------------
// uninitialised data

static int front_wheel_rotation[66];		// NOTE: no names in the database (dword_B9EF98, dword_B9F0A0)
static int rear_wheel_rotation[66];

void DrawCarWheels(int id, MATRIX *matrix, char visibility);
int GetCarDamage(int car);

// [0x4a27d0] [D]
// The player's car cosmetics (wheel positions, body points, power ...) from the chosen car.
void InitCarCosmetics(void)
{
	memcpy(car_cosmetics, &player_car_cosmetics[gPlayerCarModelType], sizeof(CAR_COSMETICS));
}

// [0x4a27f7] [D]
// Draws a car (slot -1: the player's, else a civilian slot) with its shadow, wheels, lights,
// smoke and water splash; the player's car sinks in deep water.
void DrawCar(int id, int slot)
{
	char old_clipping;
	MODEL *car_model;
	VECTOR4 position;
	int visibility;
	int cop_slot;
	int poly_block;
	float shine;
	CAR_DATA *cp;
	int normals;
	float fade_dist;
	CIV_STATE *ai;
	int civ;
	int model;
	int vertices;
	int WheelSpeed;

	// not from inside the player's car, nor when the head is turned round
	if (slot == -1 && cameraview == 8)
		return;

	if (FlipX && slot == -1)
		return;

	if (slot == -1 && cameraview == 2 && abs(gHeadTurn >> 16) > 1024)
		return;

	cp = &car_data[id];

	position.vx = (float)cp->location.vx;
	position.vy = -(float)cp->location.vy;
	position.vz = (float)cp->location.vz;

	if (cameraview != 6 && !PositionVisible(&position, CurrentPVS, camera.current_cell_x, camera.current_cell_z))
		return;

	cop_slot = 0;

	if (slot == -1)
	{
		civ = 0;
		model = 0;
	}
	else
	{
		civ = slot + 1;
		cop_slot = (unsigned char)civilian_ai_data[slot].IsCopCar;
		model = (unsigned char)civilian_ai_data[slot].model;
	}

	car_model = gCarCleanModelPtr[model];

	// the headlight pools on the road at night
	if (gNight && id != 0)
	{
		ai = &civilian_ai_data[id - 1];

		if (ai->ctrlState != CIVSTATE_PARKED
			&& car_data[id].damage[0] < 1000
			&& car_data[id].damage[1] < 1000
			&& FrustrumCheck(&position, car_model->bounding_sphere * 2.5f))
		{
			gCarPools[gCarPoolsToDraw++] = id;
		}
	}

	visibility = FrustrumCheck(&position, car_model->bounding_sphere);

	if (!visibility)
		return;

	// the body matrix in float, x and z flipped
	float_body_mat[id].m[0][0] = (float)car_data[id].where.m[0][0] * (1.0f / 4096.0f);
	float_body_mat[id].m[1][0] = (float)car_data[id].where.m[1][0] * (1.0f / 4096.0f);
	float_body_mat[id].m[2][0] = (float)car_data[id].where.m[2][0] * (1.0f / 4096.0f);
	float_body_mat[id].m[0][1] = (float)car_data[id].where.m[0][1] * (1.0f / 4096.0f);
	float_body_mat[id].m[1][1] = (float)car_data[id].where.m[1][1] * (1.0f / 4096.0f);
	float_body_mat[id].m[2][1] = (float)car_data[id].where.m[2][1] * (1.0f / 4096.0f);
	float_body_mat[id].m[0][2] = (float)car_data[id].where.m[0][2] * (1.0f / 4096.0f);
	float_body_mat[id].m[1][2] = (float)car_data[id].where.m[1][2] * (1.0f / 4096.0f);
	float_body_mat[id].m[2][2] = (float)car_data[id].where.m[2][2] * (1.0f / 4096.0f);

	float_body_mat[id].m[0][0] = -float_body_mat[id].m[0][0];
	float_body_mat[id].m[0][1] = -float_body_mat[id].m[0][1];
	float_body_mat[id].m[0][2] = -float_body_mat[id].m[0][2];
	float_body_mat[id].m[2][0] = -float_body_mat[id].m[2][0];
	float_body_mat[id].m[2][1] = -float_body_mat[id].m[2][1];
	float_body_mat[id].m[2][2] = -float_body_mat[id].m[2][2];

	position.vx = position.vx - camera_position.vx;
	position.vy = position.vy - camera_position.vy;
	position.vz = position.vz - camera_position.vz;

	CurrentRotationMatrix = &inv_camera_matrix;
	MatrixRotate(&position);

	// the car shine fades out with distance
	if (do_fogging)
		fade_dist = 1500.0f;
	else
		fade_dist = 5500.0f;

	if (position.vz > fade_dist)
		shine = 0.0f;
	else if (position.vz >= 1000.0f)
		shine = 1.0f - (position.vz - 1000.0f) / (fade_dist - 1000.0f);
	else
		shine = 1.0f;

	if (far_clip - 1000.0f < position.vz)
		return;

	// far away: the low detail model only
	if (position.vz > 15000.0f && !CamPerspectiveMode)
	{
		DrawCarModel(gCarLowModelPtr[model], &float_body_mat[id], &position, 0, id, model, gCarMaxDamage[civ], shine);
		return;
	}

	if (!gSinking || slot != -1)
	{
		old_clipping = do_clipping;
		DrawShadow(id);
		do_clipping = old_clipping;

		AddHeadLights(id);
	}

	DrawCarWheels(id, &float_body_mat[id], visibility);

	if (id == PlayerCar && !gSinking)
	{
		if (cp->wheel_speed > 0 && cp->thrust < 0)
			AddSideLights(id);
		else
			AddBrakeLight(id);

		if (cp->wheel_speed < 0 && cp->thrust < 0)
			AddReverseLight(id);

		if (cp->wheel_speed < 307200 && cp->wheel_speed > 0 && cp->wheel_angle > 200)
			AddIndicatorLight(id, 0);
		else if (cp->wheel_speed < 307200 && cp->wheel_speed > 0 && cp->wheel_angle < -200)
			AddIndicatorLight(id, 1);
	}

	// NOTE: WheelSpeed is not set while the player's car sinks (gSinking), as in the binary
	WheelSpeed = 0;

	if (!gSinking)
	{
		WheelSpeed = cp->wheel_speed;

		if (cp->damage[0] > 4000
			|| cp->damage[1] > 4000
			|| (cop_slot && cop_data[cop_slot - 1].totalDamage > gCopCarWipedOutDamage))
		{
			if (WheelSpeed < 400000 && WheelSpeed > -200000)
			{
				AddSmokingEngine(id, 1, WheelSpeed);

				if (WheelSpeed < 30000 && WheelSpeed > -30000)
					AddFlamingEngine(id);
			}
		}
		else if ((cp->damage[0] > 3000 || cp->damage[1] > 3000) && WheelSpeed < 400000 && WheelSpeed > -200000)
		{
			AddSmokingEngine(id, 0, WheelSpeed);
		}
	}

	if (WheelSpeed > 80000 || WheelSpeed < -40000)
		SwirlLeaves(&cp->location);

	if (((!gSinking && WheelSpeed < 60000 && WheelSpeed > -60000) || (gCheatFlags.SuperFastCars && id == 0))
		&& (id == 0 || civilian_ai_data[id - 1].ctrlState != CIVSTATE_PARKED))
	{
		AddExhaustSmoke(id, WheelSpeed);
	}

	// the player's car in deep water: splash, slow down, then sink
	if (gDoSmoke
		&& id == 0
		&& ROADS_GetRoadInfo(cp->location.vx, cp->location.vz)
		&& model_pointers[RouteData.type]
		&& (model_pointers[RouteData.type]->shape_flags & SHAPE_FLAG_WATER)
		&& RouteData.height + cp->location.vy < 0)
	{
		if (gTimeInWater)
		{
			AddWater(0, RouteData.height);

			car_data[0].thrust = -7000;
			car_data[0].st.n.linearVelocity[0] >>= 1;
			car_data[0].st.n.linearVelocity[1] >>= 1;
			car_data[0].st.n.linearVelocity[2] >>= 1;

			gTimeInWater--;
		}
		else
		{
			car_data[0].thrust = -7000;
			car_data[0].st.n.linearVelocity[0] >>= 1;
			car_data[0].st.n.linearVelocity[1] >>= 1;
			car_data[0].st.n.linearVelocity[2] >>= 1;

			gSinking = 1;
		}
	}

	if (id == 0 && gPlayerCarModelType == 10 && gPlayerSirenOn)
		AddCopCarLight(0, &gPlayerSirenClock, 1.0f);

	// draw with the slot's own (dented) copy of the vertices, normals and, for civilians, polygons
	vertices = car_model->vertices;
	normals = car_model->normals;

	if (civ)
		poly_block = car_model->poly_block;

	car_model->vertices = (int)gTempCarVertDump[civ];
	car_model->normals = (int)gTempCarNormalDump[civ];

	if (civ)
		car_model->poly_block = (int)gTempCarPolyDump[civ];

	if (visibility == 1)
		DrawCarModel(car_model, &float_body_mat[id], &position, 0, id, model, gCarMaxDamage[civ], shine);
	else if (visibility == 2)
		DrawCarModel(car_model, &float_body_mat[id], &position, 1, id, model, gCarMaxDamage[civ], shine);

	car_model->vertices = vertices;
	car_model->normals = normals;

	if (civ)
		car_model->poly_block = poly_block;
}

// [0x4a32ed] [D]
// Picks a moving civilian car out of the player's sight and heading away as the gang car.
int SelectGangCar(void)
{
	int gang;
	VECTOR4 pos;
	int ccz;
	int ccx;
	int angle;
	CIV_STATE *ai;
	VECTOR4 player_pos;
	int i;
	CAR_DATA *cp;

	player_pos.vx = (float)car_data[0].location.vx;
	player_pos.vz = (float)car_data[0].location.vz;

	GetVisSetAtPosition(&player_pos, gang_car_vis, &ccx, &ccz);

	ai = civilian_ai_data;
	gang = -1;

	for (i = 0; i < 64; i++, ai++)
	{
		if (ai->isGangCar)
			return i;

		if (ai->IsCopCar || ai->model == 1 || ai->ctrlState != CIVSTATE_MOVING || gang != -1 || GetCarDamage(i))
			continue;

		cp = &car_data[i + 1];

		pos.vx = (float)cp->location.vx;
		pos.vz = (float)cp->location.vz;

		if (PositionVisible(&pos, gang_car_vis, ccx, ccz))
			continue;

		if (abs(((car_data[0].direction - cp->direction + 2048) & 0xFFF) - 2048) < 1024)
			continue;

		angle = Arc_Tan(car_data[0].location.vx - float_to_int(pos.vx), car_data[0].location.vz - float_to_int(pos.vz));
		angle = abs(((cp->direction - angle + 2048) & 0xFFF) - 2048);

		if (angle <= 712)
			gang = i;
	}

	if (gang != -1)
		civilian_ai_data[gang].isGangCar = 1;

	return gang;
}

// [0x4a34f1] [D]
int GetCarDamage(int car)
{
	return civ_ai_data_pt[car].totalDamage;
}

// [0x4a350c] [D]
// Draws the four wheels, turned by the steering and spun by the wheel speed (fast spinning
// wheels use the blurred model, a lost hubcap the damaged one).
void DrawCarWheels(int id, MATRIX *matrix, char visibility)
{
	MODEL *front_model;
	MODEL *rear_model;
	MODEL *wheel_model;
	CAR_COSMETICS *car_cos;
	float *verts;
	MATRIX steer_mat;
	MATRIX turned;
	VECTOR4 pos;
	CAR_DATA *cp;
	float c;
	float s;
	float f0, f1, f2, f3, f4, f5;		// the front rim corners
	float r0, r1, r2, r3, r4, r5;		// the rear rim corners
	float dx;
	float dy;
	float dz;
	int front_speed;
	int rear_speed;
	int i;

	cp = &car_data[id];

	front_speed = cp->wheel_speed / 256;
	rear_speed = cp->wheel_speed / 256;

	if (cp->wheelspin)
		rear_speed = -700;

	if (cp->wheel[0].locked)
		front_speed = 0;

	if (cp->wheel[3].locked)
		rear_speed = 0;

	if (!GamePaused)
	{
		front_wheel_rotation[id] = (front_speed + front_wheel_rotation[id]) & 0xFFF;
		rear_wheel_rotation[id] = (rear_speed + rear_wheel_rotation[id]) & 0xFFF;
	}

	if (front_speed <= 500 && front_speed >= -500)
		front_model = gCleanWheelModelPtr;
	else
		front_model = gFastWheelModelPtr;

	if (rear_speed <= 500 && rear_speed >= -500)
		rear_model = gCleanWheelModelPtr;
	else
		rear_model = gFastWheelModelPtr;

	steer_mat.m[0][0] = 1.0f;
	steer_mat.m[0][1] = 0.0f;
	steer_mat.m[0][2] = 0.0f;
	steer_mat.m[1][0] = 0.0f;
	steer_mat.m[1][1] = 1.0f;
	steer_mat.m[1][2] = 0.0f;
	steer_mat.m[2][0] = 0.0f;
	steer_mat.m[2][1] = 0.0f;
	steer_mat.m[2][2] = 1.0f;

	RotMatrixY(&steer_mat, cp->wheel_angle & 0xFFF);
	MulMatrix(&steer_mat, matrix, &turned);

	c = fcos(front_wheel_rotation[id]) * WHEEL_RADIUS;
	s = fsin(front_wheel_rotation[id]) * WHEEL_RADIUS;

	f0 = c - s;
	f1 = s + c;
	f2 = c + s;
	f3 = s - c;
	f4 = -s - c;
	f5 = -c - s;

	c = fcos(rear_wheel_rotation[id]) * WHEEL_RADIUS;
	s = fsin(rear_wheel_rotation[id]) * WHEEL_RADIUS;

	r0 = c - s;
	r1 = s + c;
	r2 = c + s;
	r3 = s - c;
	r4 = -s - c;
	r5 = -c - s;

	if (gUseAlphaTests && gPreviousAlpha != 1)
	{
		grAlphaCombine(GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_ONE, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_TEXTURE, FXTRUE);
		grAlphaTestFunction(GR_CMP_GREATER);
		gPreviousAlpha = 1;
	}

	dx = (float)cp->location.vx - camera_position.vx;
	dy = (float)-cp->location.vy - camera_position.vy;
	dz = (float)cp->location.vz - camera_position.vz;

	car_cos = &car_cosmetics[(unsigned char)cp->model];

	wheel_model = NULL;

	for (i = 0; i < 4; i++)
	{
		if (id == 0 && !gHubcap.Present[3 - i])
			wheel_model = gDamWheelModelPtr;
		else if (i & 1)
			wheel_model = rear_model;
		else
			wheel_model = front_model;

		if (i & 2)
			pos.vx = (float)(-car_cos->wheelDisp[i].vx - 25);
		else
			pos.vx = (float)(25 - car_cos->wheelDisp[i].vx);

		pos.vy = (float)(-car_cos->wheelDisp[i].vy - 70);
		pos.vy = pos.vy + (float)(20 - (unsigned char)cp->wheel[i].susCompression);
		pos.vz = (float)-car_cos->wheelDisp[i].vz;

		CurrentRotationMatrix = matrix;
		MatrixRotateB(&pos);

		pos.vx = pos.vx + dx;
		pos.vy = pos.vy + dy;
		pos.vz = pos.vz + dz;

		CurrentRotationMatrix = &inv_camera_matrix;
		MatrixRotate(&pos);

		// spin the rim: the y / z of vertices 8 - 15 (VECTOR4 vertices)
		verts = (float *)wheel_model->vertices;

		if (i & 1)
		{
			verts[33] = r0;
			verts[49] = r0;
			verts[34] = r1;
			verts[50] = r1;
			verts[37] = r5;
			verts[53] = r5;
			verts[38] = r0;
			verts[54] = r0;
			verts[41] = r3;
			verts[57] = r3;
			verts[42] = r4;
			verts[58] = r4;
			verts[45] = r2;
			verts[61] = r2;
			verts[46] = r3;
			verts[62] = r3;

			if (gDoTraining && id == 65)
				sub_42BD3B(wheel_model, matrix, (VECTOR *)&pos, visibility - 1);
			else
				DrawObject(wheel_model, matrix, &pos, visibility - 1);
		}
		else
		{
			verts[33] = f0;
			verts[49] = f0;
			verts[34] = f1;
			verts[50] = f1;
			verts[37] = f5;
			verts[53] = f5;
			verts[38] = f0;
			verts[54] = f0;
			verts[41] = f3;
			verts[57] = f3;
			verts[42] = f4;
			verts[58] = f4;
			verts[45] = f2;
			verts[61] = f2;
			verts[46] = f3;
			verts[62] = f3;

			if (gDoTraining && id == 65)
				sub_42BD3B(wheel_model, &turned, (VECTOR *)&pos, visibility - 1);
			else
				DrawObject(wheel_model, &turned, &pos, visibility - 1);
		}
	}

	if (gUseAlphaTests && gPreviousAlpha != 0)
	{
		grAlphaCombine(GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_ONE, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_CONSTANT, FXTRUE);
		grAlphaTestFunction(GR_CMP_ALWAYS);
		gPreviousAlpha = 0;
	}
}
