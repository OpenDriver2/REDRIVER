/*
 * job_fx.c - mission effects: the bomb crate and its explosion, the helicopter
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "job_fx.c", 0x4ea560 - 0x4ec581).
 *
 * The bomb mission: a crate (CRATE) slides about the player car's boot with the car's
 * acceleration and takes damage from the knocks; at 80 (or with the car on its side or roof) it
 * goes off. The explosion (ex_time, ex_centre) is drawn as growing spheres (DrawSphere) with a
 * full screen flash, lights the scene (DoExplosionLamps) and smashes the smashable objects
 * round it (SmashSurroundingObjects). The helicopter (chopper, Chicago) lifts off when the
 * mission timer reaches chopper.start.
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "dr_types.h"
#include "job_fx.h"
#include "dr_globals.h"
#include "asmmath.h"
#include "camera.h"
#include "debris.h"
#include "Director.h"
#include "handling.h"
#include "map.h"
#include "MScript.h"
#include "pause.h"
#include "shell.h"
#include "smashable.h"
#include "sound.h"
#include "testmath.h"
#include "D3DGlide/glide_ptrs.h"
#include "Engine/3dfx/gfxinit.h"
#include "Engine/3dfx/object.h"
#include "Engine/3dfx/tset.h"
#include "Engine/3dfx/polydraw.h"
#include "dr_cars.h"
#include "glaunch.h"

#define EXPLOSION_TIME		192		// ex_time when the explosion is over
#define CRATE_EXPLODES		80		// ExBoxDamage

typedef struct CRATE
{
	VECTOR4 location;		// in the car's space
	VECTOR4 velocity;
	VECTOR4 field_20;		// the force on it this frame
	int field_30;			// damage, ExBoxDamage = field_30 >> 6
} CRATE;

void InitSphere(void);
void DrawSphere(float scale, VECTOR4 *position, int direction, u_char red, u_char green, u_char blue, int flat, int smoke);
void DamageObjects(VECTOR4 *pos);
void SmashSurroundingObjects(void);
void ChopperLift(int height);
void ChopperTilt(int x, int y, int z, float t);

// uninitialised data
// NOTE: in the linker's common area (0xd814c0 - 0xd82720); defined here as job_fx.c is their
// main user (ActivateChopper, among them, is left in MScript.c)

char ex_green;
int ExBoxDamage;
int TriggerBomb;			// the bomb has gone off
MODEL *ChopperBody;
int box_model;
MODEL *ChopperRotor;
int ChopperBodyNum;
int ExplosionJustHappened;
VECTOR4 ex_centre;
int rotate2;
int rotate1;
char ex_red;
char ex_blue;
VECTOR4 SmokePos;			// above the crate
int ex_time;
Helicopter chopper;
CRATE crate;
VECTOR4 last_vel;			// the player car's linear velocity last frame
VECTOR4 sphere[7][32];		// unit sphere: 7 rings of 32 points, -0.75 to 0.75 high

// [0x4ea560] [D]
void InitSphere(void)
{
	int i;
	int j;
	float y;
	float r;

	for (i = 0; i < 4; i++)
	{
		y = (float)i * 0.25f;
		r = (float)sqrt(1.0 - y * y);

		for (j = 0; j < 32; j++)
		{
			sphere[3 - i][j].vx = fsintab[128 * j] * r;
			sphere[i + 3][j].vx = sphere[3 - i][j].vx;

			sphere[i + 3][j].vy = y;
			sphere[3 - i][j].vy = -y;

			sphere[3 - i][j].vz = fsintab[128 * j + 1024] * r;
			sphere[i + 3][j].vz = sphere[3 - i][j].vz;
		}
	}
}

// [0x4ea6b5] [D]
// Draws the sphere at position (camera relative) scaled and turned by direction, as triangle
// strips between the rings, darker away from the middle; flat: 20 high; smoke: with the smoke
// texture.
// NOTE: direction 4096 (DrawExplosion with rotate2 0) reads fsintab[5120], one past the end, as
// in the original
void DrawSphere(float scale, VECTOR4 *position, int direction, u_char red, u_char green, u_char blue, int flat, int smoke)
{
	int skip;
	int i;
	int j;
	int n;
	int idx;
	int ring;
	float ooz;
	float dist;
	VECTOR4 pos;
	GrVertex vert[3];

	skip = 2;

	if (last_trans_on != 3)
	{
		grAlphaBlendFunction(GR_BLEND_ONE, GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_ZERO);
		last_trans_on = 3;
	}

	if (smoke)
	{
		if (smoke_texture->tset != last_tset)
		{
			last_tset = smoke_texture->tset;
			grTexSource(GR_TMU0, TextureSlots[smoke_texture->tset].texture_ram_address, GR_MIPMAPLEVELMASK_BOTH, TextureSlots[smoke_texture->tset].TInfo);
		}

		if (last_state != TEXT3)
		{
			grColorCombine(GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_LOCAL, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_TEXTURE, FXFALSE);
			last_state = TEXT3;
		}

		SwapPalette(0);
	}
	else if (last_state != FLAT3)
	{
		grColorCombine(GR_COMBINE_FUNCTION_LOCAL, GR_COMBINE_FACTOR_ZERO, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_CONSTANT, FXFALSE);
		last_state = FLAT3;
	}

	CurrentRotationMatrix = &inv_camera_matrix;

	for (i = 0; i < 6; i++)
	{
		n = 0;

		// a strip zigzagging between ring i and i + 1, round and back to the start
		for (j = 0; j < 66; j++)
		{
			ring = (j & 1) + i;
			idx = 32 * ring + ((j & 0x3F) >> 1);

			pos.vx = fsintab[direction + 1024] * sphere[0][idx].vx - fsintab[direction] * sphere[0][idx].vz;
			pos.vx = pos.vx * scale + position->vx;

			if (flat)
				pos.vy = -sphere[0][idx].vy * 20.0f + position->vy;
			else
				pos.vy = -sphere[0][idx].vy * scale + position->vy;

			pos.vz = fsintab[direction] * sphere[0][idx].vx + fsintab[direction + 1024] * sphere[0][idx].vz;
			pos.vz = pos.vz * scale + position->vz;

			MatrixRotate(&pos);

			if (pos.vz >= 50.0f)
			{
				ooz = ooz_table[(int)pos.vz];
				dist = DIST * ooz;

				vert[n].oow = ooz;
				vert[n].x = pos.vx * dist + screen_xmid - VERTEX_SNAPPER;
				vert[n].y = pos.vy * dist + screen_ymid - VERTEX_SNAPPER;

				ring = abs(3 - ring);

				vert[n].r = (float)(red - 20 * ring);

				if (vert[n].r < 0.0f)
					vert[n].r = 0.0f;

				vert[n].g = (float)(green - 30 * ring);

				if (vert[n].g < 0.0f)
					vert[n].g = 0.0f;

				vert[n].b = (float)(blue - 50 * ring);

				if (vert[n].b < 0.0f)
					vert[n].b = 0.0f;

				if (n % 3 != 0)
					vert[n].tmuvtx[0].sow = (float)smoke_texture->u * vert[n].oow;
				else
					vert[n].tmuvtx[0].sow = (float)smoke_texture->w * vert[n].oow;

				if ((n + 1) % 3 != 0)
					vert[n].tmuvtx[0].tow = (float)smoke_texture->v * vert[n].oow;
				else
					vert[n].tmuvtx[0].tow = (float)smoke_texture->h * vert[n].oow;

				if (skip)
					skip--;
				else
					guDrawTriangleWithClip(&vert[0], &vert[1], &vert[2]);
			}
			else
			{
				skip = 2;
			}

			if (++n == 3)
				n = 0;
		}
	}
}

// [0x4eacf0] [D]
// The explosion: growing fire and smoke spheres and two flat shock rings, then a full screen
// flash in the explosion colour.
void DrawExplosion(void)
{
	int oldDepthMask;
	float size;
	VECTOR4 pos;
	GrVertex v0;
	GrVertex v1;
	GrVertex v2;

	if (ex_time >= EXPLOSION_TIME)
		return;

	size = (float)ex_time * 8.0f + 500.0f;

	pos.vx = ex_centre.vx - camera_position.vx;
	pos.vy = ex_centre.vy - camera_position.vy;
	pos.vz = ex_centre.vz - camera_position.vz;

	oldDepthMask = EnableDepthMask(0);

	DrawSphere(size - 20.0f, &pos, 0, ex_red, ex_green, ex_blue, 0, 0);
	DrawSphere(size, &pos, 4096 - rotate2, ex_red, ex_green, ex_blue, 0, 1);
	DrawSphere(size, &pos, rotate2, ex_red, ex_green, ex_blue, 0, 1);
	DrawSphere((size - 200.0f) * 4.0f, &pos, rotate2, ex_red, ex_red, ex_red, 1, 1);
	DrawSphere((size - 200.0f) * 12.0f, &pos, rotate2, ex_red, ex_red, ex_red, 1, 1);

	if (last_state != FLAT3)
	{
		grColorCombine(GR_COMBINE_FUNCTION_LOCAL, GR_COMBINE_FACTOR_ZERO, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_CONSTANT, FXFALSE);
		last_state = FLAT3;
	}

	if (last_trans_on != 3)
	{
		grAlphaBlendFunction(GR_BLEND_ONE, GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_ZERO);
		last_trans_on = 3;
	}

	v0.r = (float)(u_char)ex_red;
	v1.r = v0.r;
	v2.r = v0.r;
	v0.g = (float)(u_char)ex_green;
	v1.g = v0.g;
	v2.g = v0.g;
	v0.b = (float)(u_char)ex_blue;
	v1.b = v0.b;
	v2.b = v0.b;

	v2.x = clipxmax;
	v2.y = clipymin;
	v1.x = clipxmin;
	v1.y = clipymax;
	v0.x = clipxmin;
	v0.y = clipymin;

	if (using_glide)
	{
		v0.oow = 1.0f;
		v1.oow = 1.0f;
		v2.oow = 1.0f;
	}
	else
	{
		v0.oow = 0.015625f;
		v1.oow = 0.015625f;
		v2.oow = 0.015625f;
	}

	guDrawTriangleWithClip(&v2, &v1, &v0);

	v0.x = clipxmax;
	v0.y = clipymax;

	guDrawTriangleWithClip(&v2, &v1, &v0);

	EnableDepthMask(oldDepthMask);
}

// [0x4eb03e] [D]
// The explosion's light: white, then orange, then fading red.
void DoExplosionLamps(void)
{
	if (ex_time < 64)
	{
		ex_red = 255 - ex_time;
		ex_green = 255 - ex_time;
		ex_blue = 255 - 4 * ex_time;
	}
	else if (ex_time < 128)
	{
		ex_red = 255 - ex_time;
		ex_green = 63 - 2 * ex_time;
		ex_blue = 0;
	}
	else if (ex_time < EXPLOSION_TIME)
	{
		ex_red = 127 - 2 * ex_time;
		ex_green = 192 - ex_time;
		ex_blue = 0;
	}
	else
	{
		return;
	}

	AddLamp(&ex_centre, 6000, (u_char)ex_red, (u_char)ex_green, (u_char)ex_blue);
}

// [0x4eb126] [D]
// Smashes the smashable objects in the cell at pos.
void DamageObjects(VECTOR4 *pos)
{
	int cellx;
	int cellz;
	int cell;
	IVECTOR velocity;
	CELL_DATA *cdp;
	CELL_OBJECT *cop;
	MODEL *model;

	cellx = float_to_int(((float)units_across_halved + pos->vx) * cell_recip);
	cellz = float_to_int(((float)units_down_halved + pos->vz) * cell_recip);

	for (cell = cell_ptrs[region_size * (cellz % region_size) + cellx % region_size +
			region_size * region_size * (((cellx / region_size) & 1) + 2 * ((cellz / region_size) & 1))];
		cell != -1; cell = cdp->next)
	{
		cdp = &cells[cell];
		cop = &cell_objects[cdp->num];
		model = model_pointers[cop->type];

		if (model->flags2 & MODEL_FLAG_SMASHABLE)
		{
			memset(&velocity, 0, sizeof(velocity));
			velocity.vx = -(rand() % 50) - 50;

			damage_object(cop, &velocity);
		}
	}
}

// [0x4eb2bd] [D]
// The blast reaches the cells round the explosion: the centre at 10, the ring 1500 out at 30,
// the ring 3000 out at 60.
void SmashSurroundingObjects(void)
{
	int i;

	switch (ex_time)
	{
		case 10:
			DamageObjects(&ex_centre);
			break;
		case 30:
			ex_centre.vx = ex_centre.vx + 1500.0f;
			DamageObjects(&ex_centre);

			ex_centre.vz = ex_centre.vz + 1500.0f;
			DamageObjects(&ex_centre);

			ex_centre.vx = ex_centre.vx - 1500.0f;
			DamageObjects(&ex_centre);

			ex_centre.vx = ex_centre.vx - 1500.0f;
			DamageObjects(&ex_centre);

			ex_centre.vz = ex_centre.vz - 1500.0f;
			DamageObjects(&ex_centre);

			ex_centre.vz = ex_centre.vz - 1500.0f;
			DamageObjects(&ex_centre);

			ex_centre.vx = ex_centre.vx + 1500.0f;
			DamageObjects(&ex_centre);

			ex_centre.vx = ex_centre.vx + 1500.0f;
			DamageObjects(&ex_centre);

			ex_centre.vz = ex_centre.vz + 1500.0f;
			ex_centre.vx = ex_centre.vx - 1500.0f;
			break;
		case 60:
			ex_centre.vx = ex_centre.vx + 3000.0f;
			ex_centre.vz = ex_centre.vz + 3000.0f;

			for (i = 0; i < 4; i++)
			{
				ex_centre.vx = ex_centre.vx - 1500.0f;
				DamageObjects(&ex_centre);
			}

			for (i = 0; i < 4; i++)
			{
				ex_centre.vz = ex_centre.vz - 1500.0f;
				DamageObjects(&ex_centre);
			}

			for (i = 0; i < 4; i++)
			{
				ex_centre.vx = ex_centre.vx + 1500.0f;
				DamageObjects(&ex_centre);
			}

			for (i = 0; i < 4; i++)
			{
				ex_centre.vz = ex_centre.vz + 1500.0f;
				DamageObjects(&ex_centre);
			}

			ex_centre.vx = ex_centre.vx - 3000.0f;
			ex_centre.vz = ex_centre.vz - 3000.0f;
			break;
	}
}

// [0x4eb547] [D]
void InitJobFXGlobals(void)
{
	TriggerBomb = 0;

	InitSphere();

	ex_time = 0;
	rotate1 = 0;
	rotate2 = 900;

	crate.location.vx = 20.0f;
	crate.location.vy = 0.0f;
	crate.location.vz = 300.0f;

	last_vel.vx = 0.0f;
	crate.velocity.vx = 0.0f;

	last_vel.vy = 0.0f;
	crate.velocity.vy = 0.0f;

	last_vel.vz = 0.0f;
	crate.velocity.vz = 0.0f;

	crate.field_30 = 0;

	ExBoxDamage = 0;
	box_model = FindModelWithName("BOX");

	if (box_model == -1)
		box_model = 5;

	car_data[0].rotation.vz = 0;
	car_data[0].rotation.vx = 0;

	if (GameLevel == 1)
	{
		ChopperBody = FindModelPtrWithName("CHOP");
		ChopperRotor = FindModelPtrWithName("ROT");
		ChopperBodyNum = FindModelWithName("CHOP");

		chopper.field_10.vz = 0.0f;
		chopper.field_10.vy = 0.0f;
		chopper.field_10.vx = 0.0f;

		chopper.angle.vx = 0;
		chopper.angle.vz = 0;

		chopper.field_68 = 0.0f;
		chopper.frame = 0;
		chopper.rotorrot = 0;
	}
}

// [0x4eb6ba] [D]
// Full lift while the helicopter is more than 100 below height (or not climbing).
// NOTE: the original also works out fabs(height difference) and fabs(difference / vertical
// speed) and drops both (IDA: "missing variable assignment"); left out
void ChopperLift(int height)
{
	float dy;

	dy = (float)height - chopper.position.vy;

	if (dy < 0.0f)
	{
		if ((float)fabs(dy) > 100.0f || chopper.field_10.vy <= 0.0f)
			chopper.field_68 = 2.0f;
	}
}

// [0x4eb76a] [D]
// Tilts the helicopter to t of the angles x y (y from its start heading).
void ChopperTilt(int x, int y, int z, float t)
{
	chopper.angle.vx = (int)((float)x * t) & 0xFFF;
	chopper.angle.vy = ((int)((float)y * t) + (unsigned short)chopper.start_yang) & 0xFFF;
	chopper.angle.vz = 0;
}

// [0x4eb7bf] [D]
// draw: draws the helicopter and its rotor; else moves it: once the mission timer reaches
// chopper.start it lifts off, tilting over the first 200 frames, and climbs to 13000.
void ControlChopper(int draw)
{
	int clip;
	VECTOR4 out1;
	VECTOR4 out2;
	VECTOR4 lift;
	VECTOR4 pos;
	VECTOR4 forward;
	VECTOR4 up;
	VECTOR4 right;
	MATRIX draw_mat;

	if (draw)
	{
		clip = FrustrumCheck(&chopper.position, ChopperBody->bounding_sphere);

		if (clip && PositionVisible(&chopper.position, CurrentPVS, camera.current_cell_x, camera.current_cell_z))
		{
			pos.vx = chopper.position.vx - camera_position.vx;
			pos.vy = chopper.position.vy - camera_position.vy;
			pos.vz = chopper.position.vz - camera_position.vz;

			CurrentRotationMatrix = &inv_camera_matrix;

			MatrixRotate(&pos);

			draw_mat = chopper.draw_mat;

			draw_mat.m[0][0] = -chopper.draw_mat.m[0][0];
			draw_mat.m[0][1] = -chopper.draw_mat.m[0][1];
			draw_mat.m[0][2] = -chopper.draw_mat.m[0][2];
			draw_mat.m[2][0] = -chopper.draw_mat.m[2][0];
			draw_mat.m[2][1] = -chopper.draw_mat.m[2][1];
			draw_mat.m[2][2] = -chopper.draw_mat.m[2][2];

			DrawObject(ChopperBody, &draw_mat, &pos, 1);

			// the rotor, above and behind the body's origin
			pos.vx = chopper.position.vx - camera_position.vx;
			pos.vy = chopper.position.vy - camera_position.vy;
			pos.vz = chopper.position.vz - camera_position.vz;

			pos.vx = -290.0f * draw_mat.m[0][1] + -340.0f * draw_mat.m[0][2] + pos.vx;
			pos.vy = -290.0f * draw_mat.m[1][1] + -340.0f * draw_mat.m[1][2] + pos.vy;
			pos.vz = -290.0f * draw_mat.m[2][1] + -340.0f * draw_mat.m[2][2] + pos.vz;

			CurrentRotationMatrix = &inv_camera_matrix;

			MatrixRotate(&pos);
			RotMatrixY(&draw_mat, chopper.rotorrot);

			force_transparency = 2;

			SwapPalette(0);
			DrawObject(ChopperRotor, &draw_mat, &pos, 1);

			force_transparency = 0;
		}
	}
	else if (!GamePaused)
	{
		if (mission.timer->sec == chopper.start)
			ActivateChopper = 1;

		right.vx = 1.0f;
		right.vy = 0.0f;
		right.vz = 0.0f;
		up.vx = 0.0f;
		up.vy = 1.0f;
		up.vz = 0.0f;
		forward.vx = 0.0f;
		forward.vy = 0.0f;
		forward.vz = 1.0f;

		draw_mat.m[0][0] = 1.0f;
		draw_mat.m[1][0] = 0.0f;
		draw_mat.m[2][0] = 0.0f;
		draw_mat.m[0][1] = 0.0f;
		draw_mat.m[0][2] = 0.0f;
		draw_mat.m[1][1] = 1.0f;
		draw_mat.m[2][1] = 0.0f;
		draw_mat.m[1][2] = 0.0f;
		draw_mat.m[2][2] = 1.0f;

		RotMatrixX(&draw_mat, chopper.angle.vx);
		RotMatrixY(&draw_mat, chopper.angle.vy);

		CurrentRotationMatrix = &draw_mat;

		MatrixRotate(&forward);
		Normalise(&forward);

		right = *CrossProduct(&out1, &forward, &up);
		up = *CrossProduct(&out2, &right, &forward);

		chopper.draw_mat.m[1][0] = right.vy;
		chopper.draw_mat.m[0][0] = right.vx;
		chopper.draw_mat.m[2][0] = right.vz;
		chopper.draw_mat.m[1][1] = up.vy;
		chopper.draw_mat.m[0][1] = up.vx;
		chopper.draw_mat.m[2][1] = up.vz;
		chopper.draw_mat.m[0][2] = forward.vx;
		chopper.draw_mat.m[1][2] = forward.vy;
		chopper.draw_mat.m[2][2] = forward.vz;

		if (ActivateChopper)
		{
			// lift along the rotor axis, against gravity
			lift.vx = 0.0f;
			lift.vy = chopper.field_68;
			lift.vz = 0.0f;
			MatrixRotate(&lift);

			lift.vy = lift.vy - 1.6006666f;
			chopper.field_10.vx = chopper.field_10.vx + lift.vx;
			chopper.field_10.vy = chopper.field_10.vy + lift.vy;
			chopper.field_10.vz = chopper.field_10.vz + lift.vz;

			chopper.position.vx = chopper.position.vx + chopper.field_10.vx;
			chopper.position.vy = chopper.position.vy - chopper.field_10.vy;
			chopper.position.vz = chopper.position.vz + chopper.field_10.vz;

			// on the ground
			if (chopper.position.vy > (float)chopper.start_ypos)
			{
				chopper.position.vy = (float)chopper.start_ypos;
				chopper.field_10.vx = 0.0f;
				chopper.field_10.vy = 0.0f;
				chopper.field_10.vz = 0.0f;
			}

			chopper.field_68 = 0.0f;

			if (chopper.frame >= 200)
			{
				ChopperLift(chopper.start_ypos - 13000);
			}
			else
			{
				ChopperLift(chopper.start_ypos - 1000);
				ChopperTilt(-180, 1200, 0, (float)chopper.frame / 200.0f);
			}

			chopper.frame++;
		}

		chopper.rotorrot = (chopper.rotorrot + 90) & 0xFFF;
	}
}

// [0x4ebcdf] [D]
// The crate in the boot: pushed by the car's acceleration, held by friction, bounded by the
// boot (the front edge moves in as the rear is damaged); hard knocks damage it and it explodes.
// While the bomb goes off, steps the explosion. It smokes more as it is damaged.
void ControlCrate(void)
{
	static VECTOR4 Drift;	// NOTE: Drift.10529, never set
	int i;
	char moving;
	float ax;
	float ay;
	float az;
	float side;
	float fwd;
	float accel;
	float speed;
	float k;
	float vx;
	float vy;
	float vz;
	float limit;
	float boot;
	int dir;

	if (GamePaused)
		return;

	if (TriggerBomb)
	{
		ExplosionJustHappened = 0;

		ex_time += 2;

		rotate1 = (rotate1 + 2) & 0xFFF;
		rotate2 = (rotate2 + 3) & 0xFFF;

		SmashSurroundingObjects();
	}
	else
	{
		vx = (float)car_data[0].st.n.linearVelocity[0];
		ax = vx - last_vel.vx;

		vy = (float)car_data[0].st.n.linearVelocity[1];
		ay = vy - last_vel.vy;

		vz = (float)car_data[0].st.n.linearVelocity[2];
		az = vz - last_vel.vz;

		last_vel.vx = vx;
		last_vel.vy = vy;
		last_vel.vz = vz;

		// the acceleration in the car's space
		dir = (car_data[0].direction - 1024) & 0xFFF;

		side = fsintab[dir] * ax + fsintab[dir + 1024] * az;
		fwd = -ax * fsintab[dir + 1024] + fsintab[dir] * az;

		accel = (float)sqrt(side * side + fwd * fwd);
		speed = (float)sqrt(crate.velocity.vx * crate.velocity.vx + crate.velocity.vz * crate.velocity.vz);

		if (speed == 0.0f)
		{
			// static friction
			if (accel <= 7500.0f)
			{
				crate.field_20.vz = 0.0f;
				crate.field_20.vx = 0.0f;
			}
			else
			{
				k = 7500.0f - accel;

				crate.field_20.vx = side * k / accel;
				crate.field_20.vz = fwd * k / accel;
			}
		}
		else
		{
			crate.field_20.vx = -crate.velocity.vx * 7500.0f / speed - side;
			crate.field_20.vz = -crate.velocity.vz * 7500.0f / speed - fwd;
		}

		if (crate.location.vy < 0.0f)
		{
			crate.field_20.vy = 3000.0f;
		}
		else
		{
			crate.location.vy = 0.0f;

			if (-ay <= 3000.0f)
				crate.field_20.vy = 0.0f;
			else
				crate.field_20.vy = ay + 3000.0f;
		}

		if (crate.field_20.vx + crate.field_20.vz > 3000.0f)
			crate.field_30++;

		// friction stops the crate rather than turning it round
		moving = 3;

		if (crate.velocity.vx < 0.0f)
			moving = 2;

		if (crate.velocity.vx == 0.0f)
			moving |= 4;

		if (crate.velocity.vz < 0.0f)
			moving &= 5;

		if (crate.velocity.vz == 0.0f)
			moving |= 8;

		crate.velocity.vx = crate.field_20.vx / 4500.0f + crate.velocity.vx;
		crate.velocity.vy = crate.field_20.vy / 800.0f + crate.velocity.vy;
		crate.velocity.vz = crate.field_20.vz / 4500.0f + crate.velocity.vz;

		if (crate.velocity.vx < 0.0f)
			moving ^= 1;

		if (crate.velocity.vz < 0.0f)
			moving ^= 2;

		if (!(moving & 1) && !(moving & 4))
			crate.velocity.vx = 0.0f;

		if (!(moving & 2) && !(moving & 8))
			crate.velocity.vz = 0.0f;

		crate.location.vx = crate.location.vx + crate.velocity.vx;
		crate.location.vy = crate.location.vy + crate.velocity.vy;
		crate.location.vz = crate.location.vz + crate.velocity.vz;

		// the boot walls
		boot = (float)((car_data[0].damage[4] + car_data[0].damage[3]) >> 6);

		if (crate.location.vx > 45.0f)
		{
			crate.location.vx = 45.0f;
			crate.field_30 += 5 * float_to_int(crate.velocity.vx);
			crate.velocity.vx = 0.0f;
		}

		if (crate.location.vx < -50.0f)
		{
			crate.location.vx = -50.0f;
			crate.field_30 -= 5 * float_to_int(crate.velocity.vx);
			crate.velocity.vx = 0.0f;
		}

		limit = 390.0f - boot;

		if (crate.location.vz > limit)
		{
			crate.location.vz = 390.0f - boot;
			crate.field_30 += 5 * float_to_int(crate.velocity.vz);
			crate.velocity.vz = 0.0f;
		}

		if (crate.location.vz < 230.0f)
		{
			crate.location.vz = 230.0f;
			crate.field_30 -= 5 * float_to_int(crate.velocity.vz);
			crate.velocity.vz = 0.0f;
		}

		ExBoxDamage = crate.field_30 >> 6;

		if ((crate.field_30 >> 6) >= CRATE_EXPLODES ||
			(car_data[0].rotation.vx > 512 && car_data[0].rotation.vx < 3584) ||
			(car_data[0].rotation.vz > 512 && car_data[0].rotation.vz < 3584))
		{
			Play3DSound(3, 3, SmokePos.vx, SmokePos.vy, SmokePos.vz);

			ExBoxDamage = CRATE_EXPLODES;
			ExplosionJustHappened = 1;
			TriggerBomb = 1;
			cameraview = 9;
			no_shake_camera = camera_angle;

			for (i = 0; i < 6; i++)
				car_data[0].damage[i] = 4096;

			ex_centre.vx = crate.location.vx;
			ex_centre.vy = crate.location.vy - 100.0f;
			ex_centre.vz = crate.location.vz;

			CurrentRotationMatrix = &float_body_mat[0];

			MatrixRotateB(&ex_centre);

			ex_centre.vx = ex_centre.vx + (float)car_data[0].location.vx;
			ex_centre.vy = ex_centre.vy + (float)-car_data[0].location.vy;
			ex_centre.vz = ex_centre.vz + (float)car_data[0].location.vz;

			mission.objective = 4080;
			mission.failed_reason = 1;
			mission.pause_timer = 180;
		}
	}

	if (ExBoxDamage > 40)
	{
		if (ExBoxDamage >= 70)
			Setup_Smoke(&SmokePos, 100, 500, 1, car_data[0].speed, &Drift, 0);
		else if (ExBoxDamage >= 55)
		{
			if (game_frame & 2)
				Setup_Smoke(&SmokePos, 100, 500, 1, car_data[0].speed, &Drift, 0);
		}
		else if ((game_frame & 6) == 6)
		{
			Setup_Smoke(&SmokePos, 100, 500, 1, car_data[0].speed, &Drift, 0);
		}
	}
}

// [0x4ec466] [D]
// Draws the crate in the boot (not after the explosion) and keeps SmokePos above it.
void DrawCrate(void)
{
	VECTOR4 pos;

	if (TriggerBomb)
		return;

	pos.vx = crate.location.vx;
	pos.vy = crate.location.vy - 100.0f;
	pos.vz = crate.location.vz;

	CurrentRotationMatrix = &float_body_mat[0];
	MatrixRotateB(&pos);

	pos.vx = pos.vx + (float)car_data[0].location.vx;
	pos.vy = pos.vy + (float)-car_data[0].location.vy;
	pos.vz = pos.vz + (float)car_data[0].location.vz;

	SmokePos.vx = pos.vx;
	SmokePos.vz = pos.vz;
	SmokePos.vw = pos.vw;
	SmokePos.vy = pos.vy + 50.0f;

	pos.vx = pos.vx - camera_position.vx;
	pos.vy = pos.vy - camera_position.vy;
	pos.vz = pos.vz - camera_position.vz;

	CurrentRotationMatrix = &inv_camera_matrix;
	MatrixRotate(&pos);

	DrawObject(model_pointers[box_model], &float_body_mat[0], &pos, 1);
}
