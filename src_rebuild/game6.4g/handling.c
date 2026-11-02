/*
 * handling.c - car physics: rigid body time step, wheels, surfaces, the player's pad
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "handling.c", 0x4dc580 - 0x4e32e1)
 *
 * Fixed point: positions in world units (st.fposition in 1/16), matrices and quaternions in 1/4096,
 * velocities in 1/4096 world units per frame. GlobalTimeStep moves every active car (StepCarPhysics
 * adds it) one frame: StepOneCar works out the forces (gravity, body points under the ground,
 * wheels with suspension, friction, thrust and steering), then cars that may collide take four
 * sub frames of two Runge-Kutta steps each with the car to car impulses.
 */

#include <stdlib.h>
#include <string.h>

#include "dr_types.h"
#include "handling.h"
#include "asmmath.h"
#include "dr_globals.h"
#include "MScript.h"
#include "testmath.h"
#include "ai.h"
#include "bcoll3d.h"
#include "bcollide.h"
#include "debris.h"
#include "denting.h"
#include "felony.h"
#include "gsound.h"
#include "input.h"
#include "interview.h"
#include "map.h"
#include "pedest.h"
#include "roadbits.h"
#include "shadow.h"
#include "shell.h"
#include "sound.h"
#include "camera.h"
#include "frontend/cheats.h"

// the body state integrators of GlobalTimeStep (size of RigidBodyState in ints)
#define BODY_STATE_INTS		13

typedef struct CAR_LOCALS
{
	int unk_00[4];
	int unk_04;
	LONGVECTOR3 where;
	LONGVECTOR4 vel;
	LONGVECTOR4 acc;
	LONGVECTOR4 avel;
	LONGVECTOR4 aacc;
	int extraangulardamping;
} CAR_LOCALS;

typedef struct BOUND_BOX
{
	int x0;
	int y0;
	int z0;
	int x1;
	int y1;
	int z1;
} BOUND_BOX;

void nose_down(CAR_DATA *cp);
void jump_debris(CAR_DATA *cp);
void ConvertTorqueToAngularAcceleration(CAR_DATA *cp, CAR_LOCALS *cl);
void StepOneCar(CAR_DATA *cp, int phys);
void StepCars(void);
void CheckCarToCarCollisions(void);

//------------------------------------------------------------------------------------------
// initialised data

int horn_volume = -10000;			// NOTE: owner guessed (most references)
int gTimeInWater = 15;
int AllowSkidSound = 1;

static int frictionLimit[6] =
{
	5898240, 35389440, 5898240, 29491200, 176947200, 29491200,
};

//------------------------------------------------------------------------------------------
// uninitialised data

static int wheelheight[4];			// the player's wheel ground heights last frame (landing sound)
static int localOnGrass;			// FindSurface found a grass surface
static short normsintable[160];
static short qsintable[160];
static BOUND_BOX bbox[66];
static SIPOLY *savepp;				// MapHeightShadow: the last polygon hit
static int lsteeringTorque;
static SURFACEINFO *sip;			// MapHeightShadow: the surface of the last cell
static CAR_DATA *active_car_list[66];
static short sqsq[4100];

char other_continous_track;
static VECTOR4 other_wp[4];
static unsigned char DebrisTimer;
int Reuse_player_data;
RigidBodyState _tp[66];
int wetness;
static int PlayerWipedOutTimer;
int steeringTorque;
static int other_last_track_state[4];
CAR_DATA car_data[66];
CAR_COSMETICS car_cosmetics[12];
char gPlayerSirenOn;
static int InWater;
static char lay_down_tracks;
static int hit_delay;
char continuous_track;
static int WasOnGround;
int num_active_cars;
static int last_track_state[4];
int num_cars;
short gPlayerSirenClock;
char gPreviousHornState;
static int save_track_count;
int gCruiseControlSpeed;
static char rear_only;
RigidBodyState _d0[66];
RigidBodyState _d1[66];

int last_shadowz;					// NOTE: owner guessed (most references)
int last_shadowx;

/* replays.c */
extern int dword_CCC8A0;
extern int dword_CCCC34;

// [0x4dc580] [D]
// Fills tables no code reads (sqsq, qsintable, normsintable); called from InitialiseCarHandling.
void sub_4DC580(void)
{
	int i;
	int v;
	int s;
	int q;

	for (i = 0; i < 4100; i++)
	{
		v = (((i * i + 2048) >> 12) * i + 2048) >> 12;
		sqsq[i] = (v * v + 2048) >> 12;
	}

	for (i = 159; i >= 0; i--)
	{
		s = int_sintab[(i << 6) & 0xFFF];

		qsintable[i] = (int_sintab[(i * 32) & 0xFFF]
			* (((((81 * int_sintab[((i << 8) + 1024) & 0xFFF] + 2048) >> 12) + 4014)
				* (((1064 * ((s * s + 2048) >> 12) + 2048) >> 12) + 4096)
				+ 2048) >> 12)
			+ 2048) >> 12;

		q = qsintable[i];

		normsintable[i] = (((((q * q + 2048) >> 12) * ((q * q + 2048) >> 12) + 2048) >> 12) * q + 2048) >> 12;
	}
}

// [0x4dc6fb] [D]
void CalcPlayerCarDamage(void)
{
	gPlayerCarDamage = car_data[0].totalDamage;
}

// [0x4dc70a] [D]
void LongQuaternion2Matrix(LONGQUATERNION *qua, IMATRIX *m)
{
	int xx, xy, xz, xw;
	int yy, yz, yw;
	int zz, zw;

	xx = ((*qua)[0] * (*qua)[0] + 1024) >> 11;
	xy = ((*qua)[1] * (*qua)[0] + 1024) >> 11;
	xz = ((*qua)[2] * (*qua)[0] + 1024) >> 11;
	xw = ((*qua)[3] * (*qua)[0] + 1024) >> 11;
	yy = ((*qua)[1] * (*qua)[1] + 1024) >> 11;
	yz = ((*qua)[2] * (*qua)[1] + 1024) >> 11;
	yw = ((*qua)[3] * (*qua)[1] + 1024) >> 11;
	zz = ((*qua)[2] * (*qua)[2] + 1024) >> 11;
	zw = ((*qua)[3] * (*qua)[2] + 1024) >> 11;

	m->m[0][0] = 4096 - (zz + yy);
	m->m[0][1] = xy - zw;
	m->m[0][2] = yw + xz;
	m->m[1][0] = zw + xy;
	m->m[1][1] = 4096 - (zz + xx);
	m->m[1][2] = yz - xw;
	m->m[2][0] = xz - yw;
	m->m[2][1] = xw + yz;
	m->m[2][2] = 4096 - (yy + xx);
}

// [0x4dc879] [D]
// The car's matrix and location from its body state; renormalises the orientation quaternion.
void RebuildCarMatrix(RigidBodyState *st, CAR_DATA *cp)
{
	int sm;
	int osm;

	cp->where.t[0] = st->n.fposition[0] >> 4;
	cp->location.vx = cp->where.t[0];

	cp->where.t[1] = st->n.fposition[1] >> 4;
	cp->location.vy = cp->where.t[1];

	cp->where.t[2] = st->n.fposition[2] >> 4;
	cp->location.vz = cp->where.t[2];

	osm = st->n.orientation[3] * st->n.orientation[3]
		+ st->n.orientation[2] * st->n.orientation[2]
		+ st->n.orientation[1] * st->n.orientation[1]
		+ st->n.orientation[0] * st->n.orientation[0];

	if (osm >= 1024)
	{
		sm = 0x1800 - (osm >> 13);

		st->n.orientation[0] = (st->n.orientation[0] * sm + 2048) >> 12;
		st->n.orientation[1] = (st->n.orientation[1] * sm + 2048) >> 12;
		st->n.orientation[2] = (st->n.orientation[2] * sm + 2048) >> 12;
		st->n.orientation[3] = (st->n.orientation[3] * sm + 2048) >> 12;
	}
	else
	{
		st->n.orientation[2] = 0;
		st->n.orientation[1] = 0;
		st->n.orientation[0] = 0;
		st->n.orientation[3] = 4096;
	}

	LongQuaternion2Matrix(&st->n.orientation, &cp->where);
}

// [0x4dc9d6] [D]
void InitCarPhysics(CAR_DATA *cp, LONGVECTOR4 *startpos)
{
	ClearMem(cp, sizeof(CAR_DATA));

	localOnGrass = 0;

	cp->st.n.orientation[3] = 4096;
	cp->st.n.fposition[0] = 16 * (*startpos)[0];
	cp->st.n.fposition[1] = 16 * (*startpos)[1];
	cp->st.n.fposition[2] = 16 * (*startpos)[2];

	RebuildCarMatrix(&cp->st, cp);

	cp->acc[0] = 0;
	cp->acc[1] = GRAVITY;
	cp->acc[2] = 0;

	gTimeInWater = 15;
}

// [0x4dca67] [D]
// Sets the body state from the car's direction (a rotation about y), and position when init is 1.
void TempBuildHandlingMatrix(CAR_DATA *cp, int init)
{
	if (init == 1)
	{
		cp->st.n.fposition[0] = 16 * cp->location.vx;
		cp->st.n.fposition[1] = 16 * cp->location.vy;
		cp->st.n.fposition[2] = 16 * cp->location.vz;
	}

	cp->st.n.orientation[0] = 0;
	cp->st.n.orientation[1] = isin(cp->direction / 2);
	cp->st.n.orientation[2] = 0;
	cp->st.n.orientation[3] = icos(cp->direction / 2);

	RebuildCarMatrix(&cp->st, cp);
}

// [0x4dcb20] [D]
// The derivative of a body state: position from the velocity, orientation from the angular
// velocity.
void GetCollidedBodyState(RigidBodyState *state, RigidBodyState *delta, CAR_DATA *cp)
{
	LONGVECTOR4 AV;

	delta->n.fposition[0] = state->n.linearVelocity[0] >> 8;
	delta->n.fposition[1] = state->n.linearVelocity[1] >> 8;
	delta->n.fposition[2] = state->n.linearVelocity[2] >> 8;

	AV[0] = (state->n.angularVelocity[0] + 4096) >> 13;
	AV[1] = (state->n.angularVelocity[1] + 4096) >> 13;
	AV[2] = (state->n.angularVelocity[2] + 4096) >> 13;

	delta->n.orientation[0] = (AV[1] * state->n.orientation[2]
		+ AV[2] * -state->n.orientation[1]
		+ AV[0] * state->n.orientation[3]
		+ 2048) >> 12;

	delta->n.orientation[1] = (AV[2] * state->n.orientation[0]
		- AV[0] * state->n.orientation[2]
		+ AV[1] * state->n.orientation[3]
		+ 2048) >> 12;

	delta->n.orientation[2] = (AV[0] * state->n.orientation[1]
		+ AV[1] * -state->n.orientation[0]
		+ AV[2] * state->n.orientation[3]
		+ 2048) >> 12;

	delta->n.orientation[3] = (AV[0] * -state->n.orientation[0]
		- AV[1] * state->n.orientation[1]
		- AV[2] * state->n.orientation[2]
		+ 2048) >> 12;

	delta->n.linearVelocity[0] = 0;
	delta->n.linearVelocity[1] = 0;
	delta->n.linearVelocity[2] = 0;

	delta->n.angularVelocity[0] = 0;
	delta->n.angularVelocity[1] = 0;
	delta->n.angularVelocity[2] = 0;
}

// [0x4dcc8e] [D]
// Moves every active car one frame (see the top of the file).
void GlobalTimeStep(void)
{
	IVECTOR surfaceNormal;
	IVECTOR outpoint;
	LONGVECTOR4 pointPos;
	int n;
	int m;
	RigidBodyState *st;
	RigidBodyState *tp;
	RigidBodyState *d1;
	RigidBodyState *d0;
	LONGVECTOR4 velocity1;
	LONGVECTOR4 velocity1_4wd;
	LONGVECTOR4 velocity0;
	LONGVECTOR4 velocity0_4wd;
	LONGVECTOR4 velocity;
	LONGVECTOR4 pointVel0;
	LONGVECTOR4 pointVel1;
	LONGVECTOR4 torque;
	LONGVECTOR4 lever1;
	LONGVECTOR4 lever0;
	int howHard;
	int depth;
	int strikeVel;
	LONGVECTOR4 cp0;
	LONGVECTOR4 cp1;
	LONGVECTOR4 collisionpoint;
	LONGVECTOR4 diff;
	LONGVECTOR4 normal;
	LONGVECTOR4 offset;
	RigidBodyState *thisState_j;
	CAR_DATA *c1;
	int k;
	RigidBodyState *thisState_i;
	int c2NotInfiniteMass;
	int c1NotInfiniteMass;
	int twistY;
	CAR_DATA *cp;
	RigidBodyState *thisDelta;
	LONG AV[4];
	LONGVECTOR4 delta_orientation;
	int vel;
	RigidBodyState *p_st;
	int RKstep;
	CAR_COSMETICS *car_cos;
	int i;
	int subframe;

	if (hit_delay)
		hit_delay--;

	StepCars();
	CheckCarToCarCollisions();

	for (i = 0; i < num_active_cars; i++)
	{
		cp = active_car_list[i];
		p_st = &cp->st;

		// (removed in the original: a check that a car is only once in active_car_list)

		cp->acc[0] >>= 1;
		cp->acc[1] >>= 1;
		cp->acc[2] >>= 1;

		cp->st.n.linearVelocity[0] += cp->acc[0];
		cp->st.n.linearVelocity[1] += cp->acc[1];
		cp->st.n.linearVelocity[2] += cp->acc[2];

		cp->aacc[0] >>= 1;
		cp->aacc[1] >>= 1;
		cp->aacc[2] >>= 1;

		cp->st.n.angularVelocity[0] += cp->aacc[0];
		cp->st.n.angularVelocity[1] += cp->aacc[1];
		cp->st.n.angularVelocity[2] += cp->aacc[2];

		cp->aacc[0] = 0;
		cp->aacc[1] = 0;
		cp->aacc[2] = 0;

		if (cp->speed == 0)
		{
			vel = abs(cp->st.n.linearVelocity[2]) + abs(cp->st.n.linearVelocity[1]) + abs(cp->st.n.linearVelocity[0]);

			if (vel >= 1000)
			{
				cp->speed = 1;
			}
			else
			{
				cp->st.n.linearVelocity[0] = 0;
				cp->st.n.linearVelocity[1] = 0;
				cp->st.n.linearVelocity[2] = 0;
			}
		}

		// limit angular velocity
		for (m = 0; m < 3; m++)
		{
			if (p_st->n.angularVelocity[m] > 0x1000000)
				p_st->n.angularVelocity[m] = 0x1000000;
			else if (p_st->n.angularVelocity[m] < -0x1000000)
				p_st->n.angularVelocity[m] = -0x1000000;
		}

		if (gMissionRecording || gMissionReplaying)
			cp->mayBeColliding = 1;

		if (gDoTraining)
			cp->mayBeColliding = 0;

		if (!cp->mayBeColliding)
		{
			p_st->n.fposition[0] += p_st->n.linearVelocity[0] >> 9;
			p_st->n.fposition[1] += p_st->n.linearVelocity[1] >> 9;
			p_st->n.fposition[2] += p_st->n.linearVelocity[2] >> 9;

			AV[0] = (p_st->n.angularVelocity[0] + 4096) >> 13;
			AV[1] = (p_st->n.angularVelocity[1] + 4096) >> 13;
			AV[2] = (p_st->n.angularVelocity[2] + 4096) >> 13;

			delta_orientation[0] = (AV[1] * p_st->n.orientation[2]
				+ AV[2] * -p_st->n.orientation[1]
				+ AV[0] * p_st->n.orientation[3]
				+ 2048) >> 12;
			delta_orientation[1] = (AV[2] * p_st->n.orientation[0]
				- AV[0] * p_st->n.orientation[2]
				+ AV[1] * p_st->n.orientation[3]
				+ 2048) >> 12;
			delta_orientation[2] = (AV[0] * p_st->n.orientation[1]
				+ AV[1] * -p_st->n.orientation[0]
				+ AV[2] * p_st->n.orientation[3]
				+ 2048) >> 12;
			delta_orientation[3] = (AV[0] * -p_st->n.orientation[0]
				- AV[1] * p_st->n.orientation[1]
				- AV[2] * p_st->n.orientation[2]
				+ 2048) >> 12;

			p_st->n.orientation[0] += delta_orientation[0] >> 1;
			p_st->n.orientation[1] += delta_orientation[1] >> 1;
			p_st->n.orientation[2] += delta_orientation[2] >> 1;
			p_st->n.orientation[3] += delta_orientation[3] >> 1;

			RebuildCarMatrix(p_st, cp);
		}
	}

	// do collision interactions
	for (subframe = 0; subframe < 4; subframe++)
	{
		for (RKstep = 0; RKstep < 2; RKstep++)
		{
			for (i = 0; i < num_active_cars; i++)
			{
				cp = active_car_list[i];

				if (!cp->mayBeColliding)
					continue;

				if (RKstep)
				{
					thisState_i = &_tp[i];
					thisDelta = _d1;
				}
				else
				{
					thisState_i = &cp->st;
					thisDelta = _d0;
				}

				GetCollidedBodyState(thisState_i, &thisDelta[i], cp);

				if (ghost_mode || gDoTraining)
					continue;

				for (k = 0; k < i; k++)
				{
					c1 = active_car_list[k];

					thisState_j = &c1->st;

					if (RKstep > 0)
						thisState_j = &_tp[k];

					if (!c1->mayBeColliding || (!c1->speed && !cp->speed))
						continue;

					memcpy(cp0, cp->where.t, 12);
					memcpy(cp1, c1->where.t, 12);

					diff[0] = cp1[0] - cp0[0];
					diff[1] = cp1[1] - cp0[1];
					diff[2] = cp1[2] - cp0[2];

					if (diff[0] > 1200 || diff[0] < -1200
						|| diff[1] > 1200 || diff[1] < -1200
						|| diff[2] > 1200 || diff[2] < -1200
						|| !CarCarCollision3(cp, c1, &depth, (IVECTOR *)collisionpoint, (IVECTOR *)normal, (IVECTOR *)offset))
					{
						continue;
					}

					strikeVel = depth << 15;

					collisionpoint[1] += 70;

					lever0[0] = collisionpoint[0] - cp0[0];
					lever0[1] = collisionpoint[1] - cp0[1];
					lever0[2] = collisionpoint[2] - cp0[2];
					lever1[0] = collisionpoint[0] - cp1[0];
					lever1[1] = collisionpoint[1] - cp1[1];
					lever1[2] = collisionpoint[2] - cp1[2];

					if (k == 0 || k >= 65)
						lever1[1] -= 50;

					pointVel0[0] = (lever0[2] * thisState_i->n.angularVelocity[1] - lever0[1] * thisState_i->n.angularVelocity[2] + 2048) >> 12;
					pointVel0[1] = (lever0[0] * thisState_i->n.angularVelocity[2] - lever0[2] * thisState_i->n.angularVelocity[0] + 2048) >> 12;
					pointVel0[2] = (lever0[1] * thisState_i->n.angularVelocity[0] - lever0[0] * thisState_i->n.angularVelocity[1] + 2048) >> 12;

					pointVel0[0] += thisState_i->n.linearVelocity[0];
					pointVel0[1] += thisState_i->n.linearVelocity[1];
					pointVel0[2] += thisState_i->n.linearVelocity[2];

					pointVel1[0] = (lever1[2] * thisState_j->n.angularVelocity[1] - lever1[1] * thisState_j->n.angularVelocity[2] + 2048) >> 12;
					pointVel1[1] = (lever1[0] * thisState_j->n.angularVelocity[2] - lever1[2] * thisState_j->n.angularVelocity[0] + 2048) >> 12;
					pointVel1[2] = (lever1[1] * thisState_j->n.angularVelocity[0] - lever1[0] * thisState_j->n.angularVelocity[1] + 2048) >> 12;

					pointVel1[0] += thisState_j->n.linearVelocity[0];
					pointVel1[1] += thisState_j->n.linearVelocity[1];
					pointVel1[2] += thisState_j->n.linearVelocity[2];

					pointVel0[0] -= pointVel1[0];
					pointVel0[1] -= pointVel1[1];
					pointVel0[2] -= pointVel1[2];

					howHard = normal[2] / 32 * (pointVel0[2] / 256)
						+ normal[1] / 32 * (pointVel0[1] / 256)
						+ normal[0] / 32 * (pointVel0[0] / 256);

					if (howHard > 0 && RKstep == 0)
					{
						if (cp->fourWheelDrive != 1 || c1->fourWheelDrive != 1)
						{
							if ((!gCheatFlags.FreightTrain || cp->id != 0)
								&& (!gCheatFlags.RailroadCops || cp->model != 1)
								&& DamageCar3D(cp, &lever0, howHard))
							{
								cp->needsDenting = 1;
							}

							// NOTE: tests cp->model, not c1->model, as in the binary
							if ((!gCheatFlags.FreightTrain || c1->id != 0)
								&& (!gCheatFlags.RailroadCops || cp->model != 1)
								&& DamageCar3D(c1, &lever1, howHard))
							{
								c1->needsDenting = 1;
							}
						}

						if (howHard > 0x5000)
						{
							if (cp->id != 0 && (unsigned char)cp->id < 65)
							{
								velocity[0] = c1->st.n.linearVelocity[0] / 4096;
								velocity[1] = -17;
								velocity[2] = c1->st.n.linearVelocity[2] / 4096;
							}
							else
							{
								velocity[0] = cp->st.n.linearVelocity[0] / 4096;
								velocity[1] = -17;
								velocity[2] = cp->st.n.linearVelocity[2] / 4096;
							}

							collisionpoint[1] -= 70;

							collisionpoint[0] -= offset[0];
							collisionpoint[1] -= offset[1];
							collisionpoint[2] -= offset[2];

							collisionpoint[1] = -collisionpoint[1];

							Setup_Sparks((IVECTOR *)collisionpoint, (IVECTOR *)velocity, 6, 1);

							collisionpoint[1] = -collisionpoint[1];

							if (cp->id == 0 || c1->id == 0)
							{
								if (gDoInterview)
									gCarCarCollision = 1;

								SetForceDirection(-10000, -10000);
							}

							if (howHard > 0x5000)
							{
								Setup_Debris((IVECTOR *)collisionpoint, (IVECTOR *)velocity, 2, 1, (unsigned char)cp->model);
								Setup_Debris((IVECTOR *)collisionpoint, (IVECTOR *)velocity, 4, 0, (unsigned char)cp->model);
								Setup_Debris((IVECTOR *)collisionpoint, (IVECTOR *)velocity, 2, 1, (unsigned char)c1->model);
								Setup_Debris((IVECTOR *)collisionpoint, (IVECTOR *)velocity, 4, 0, (unsigned char)c1->model);

								Play3DSound(3, 3, (float)collisionpoint[0], (float)collisionpoint[1], (float)collisionpoint[2]);
							}
						}
					}

					strikeVel += 9 * howHard / 4;

					if (strikeVel > 0x190000)
						strikeVel = 0x190000;

					normal[0] = ((strikeVel >> 3) * (normal[0] >> 3)) >> 6;
					normal[1] = ((strikeVel >> 3) * (normal[1] >> 3)) >> 6;
					normal[2] = ((strikeVel >> 3) * (normal[2] >> 3)) >> 6;

					c2NotInfiniteMass = 1;
					c1NotInfiniteMass = 1;
					twistY = 55;

					if (gMissionReplaying || gMissionRecording || gCheatFlags.FreightTrain || gCheatFlags.RailroadCops)
					{
						if ((unsigned char)c1->id >= 65
							|| (gCheatFlags.FreightTrain && c1->id == 0)
							|| (gCheatFlags.RailroadCops && c1->model == 1))
						{
							twistY >>= 1;

							if (cp->id == 0 && !hit_delay && howHard > 8000)
							{
								hit_delay = 50;
								gChaseModeHits++;
							}

							c2NotInfiniteMass = 0;
						}

						if ((unsigned char)cp->id >= 65
							|| (gCheatFlags.FreightTrain && cp->id == 0)
							|| (gCheatFlags.RailroadCops && cp->model == 1))
						{
							twistY >>= 1;

							if (c1->id == 0 && !hit_delay && howHard > 8000)
							{
								hit_delay = 50;
								gChaseModeHits++;
							}

							c1NotInfiniteMass = 0;
						}
					}

					// apply force to car 0
					if (c1NotInfiniteMass)
					{
						if (cp->fourWheelDrive == 1 && c1->fourWheelDrive != 1 && c1->id != 0)
						{
							velocity0_4wd[0] = (7 - gDifficulty) * normal[0] / 8;
							velocity0_4wd[1] = normal[1] * (7 - gDifficulty) / 8;
							velocity0_4wd[2] = (7 - gDifficulty) * normal[2] / 8;

							thisDelta[i].n.linearVelocity[0] -= velocity0_4wd[0];
							thisDelta[i].n.linearVelocity[1] -= velocity0_4wd[1];
							thisDelta[i].n.linearVelocity[2] -= velocity0_4wd[2];

							torque[0] = (lever0[2] * velocity0_4wd[1] - lever0[1] * velocity0_4wd[2] + 2048) >> 12;
							torque[1] = (lever0[0] * velocity0_4wd[2] - lever0[2] * velocity0_4wd[0] + 2048) >> 12;
							torque[2] = (lever0[1] * velocity0_4wd[0] - lever0[0] * velocity0_4wd[1] + 2048) >> 12;
						}
						else if (cp->model != 6 || c1->model == 6)
						{
							thisDelta[i].n.linearVelocity[0] -= normal[0];
							thisDelta[i].n.linearVelocity[1] -= normal[1];
							thisDelta[i].n.linearVelocity[2] -= normal[2];

							torque[0] = (lever0[2] * normal[1] - lever0[1] * normal[2] + 2048) >> 12;
							torque[1] = (lever0[0] * normal[2] - lever0[2] * normal[0] + 2048) >> 12;
							torque[2] = (lever0[1] * normal[0] - lever0[0] * normal[1] + 2048) >> 12;
						}
						else
						{
							velocity0[0] = 2 * normal[0] / 3;
							velocity0[1] = 2 * normal[1] / 3;
							velocity0[2] = 2 * normal[2] / 3;

							thisDelta[i].n.linearVelocity[0] -= velocity0[0];
							thisDelta[i].n.linearVelocity[1] -= velocity0[1];
							thisDelta[i].n.linearVelocity[2] -= velocity0[2];

							torque[0] = (lever0[2] * velocity0[1] - lever0[1] * velocity0[2] + 2048) >> 12;
							torque[1] = (lever0[0] * velocity0[2] - lever0[2] * velocity0[0] + 2048) >> 12;
							torque[2] = (lever0[1] * velocity0[0] - lever0[0] * velocity0[1] + 2048) >> 12;
						}

						thisDelta[i].n.angularVelocity[0] += torque[0] * twistY;
						thisDelta[i].n.angularVelocity[1] += torque[1] * twistY;
						thisDelta[i].n.angularVelocity[2] += torque[2] * twistY;
					}

					// apply force to car 1
					if (c2NotInfiniteMass)
					{
						if (cp->fourWheelDrive != 1 && c1->fourWheelDrive == 1 && cp->id != 0)
						{
							velocity1_4wd[0] = (7 - gDifficulty) * normal[0] / 8;
							velocity1_4wd[1] = normal[1] * (7 - gDifficulty) / 8;
							velocity1_4wd[2] = (7 - gDifficulty) * normal[2] / 8;

							thisDelta[k].n.linearVelocity[0] += velocity1_4wd[0];
							thisDelta[k].n.linearVelocity[1] += velocity1_4wd[1];
							thisDelta[k].n.linearVelocity[2] += velocity1_4wd[2];

							torque[0] = (velocity1_4wd[2] * lever1[1] - velocity1_4wd[1] * lever1[2] + 2048) >> 12;
							torque[1] = (velocity1_4wd[0] * lever1[2] - velocity1_4wd[2] * lever1[0] + 2048) >> 12;
							torque[2] = (velocity1_4wd[1] * lever1[0] - velocity1_4wd[0] * lever1[1] + 2048) >> 12;
						}
						else if (cp->model == 6 || c1->model != 6)
						{
							thisDelta[k].n.linearVelocity[0] += normal[0];
							thisDelta[k].n.linearVelocity[1] += normal[1];
							thisDelta[k].n.linearVelocity[2] += normal[2];

							torque[0] = (normal[2] * lever1[1] - normal[1] * lever1[2] + 2048) >> 12;
							torque[1] = (normal[0] * lever1[2] - normal[2] * lever1[0] + 2048) >> 12;
							torque[2] = (normal[1] * lever1[0] - normal[0] * lever1[1] + 2048) >> 12;
						}
						else
						{
							velocity1[0] = 2 * normal[0] / 3;
							velocity1[1] = 2 * normal[1] / 3;
							velocity1[2] = 2 * normal[2] / 3;

							thisDelta[k].n.linearVelocity[0] += velocity1[0];
							thisDelta[k].n.linearVelocity[1] += velocity1[1];
							thisDelta[k].n.linearVelocity[2] += velocity1[2];

							torque[0] = (velocity1[2] * lever1[1] - velocity1[1] * lever1[2] + 2048) >> 12;
							torque[1] = (velocity1[0] * lever1[2] - velocity1[2] * lever1[0] + 2048) >> 12;
							torque[2] = (velocity1[1] * lever1[0] - velocity1[0] * lever1[1] + 2048) >> 12;
						}

						thisDelta[k].n.angularVelocity[0] += torque[0] * twistY;
						thisDelta[k].n.angularVelocity[1] += torque[1] * twistY;
						thisDelta[k].n.angularVelocity[2] += torque[2] * twistY;
					}

					NotifyAIOfCollision((unsigned char)cp->id, (unsigned char)c1->id, howHard);
				}
			}

			// update forces and rebuild matrices of the cars
			for (i = 0; i < num_active_cars; i++)
			{
				cp = active_car_list[i];

				if (!cp->mayBeColliding)
					continue;

				st = &cp->st;
				tp = &_tp[i];
				d0 = &_d0[i];
				d1 = &_d1[i];

				if (RKstep == 0)
				{
					for (m = BODY_STATE_INTS - 1; m >= 0; m--)
						tp->v[m] = (d0->v[m] >> 3) + st->v[m];

					RebuildCarMatrix(tp, cp);
				}
				else if (RKstep == 1)
				{
					for (m = BODY_STATE_INTS - 1; m >= 0; m--)
						st->v[m] += (d1->v[m] + d0->v[m]) >> 4;

					RebuildCarMatrix(st, cp);
				}
			}
		}
	}

	// second sub frame passed, update matrices and physics direction
	// dent cars
	for (i = 0; i < num_active_cars; i++)
	{
		cp = active_car_list[i];

		car_cos = &car_cosmetics[(unsigned char)cp->model];

		if (cp->needsDenting && (gDoTraining || ((i + game_frame) & 3) == 0))
		{
			QueueDentCar((unsigned char)cp->id - 1);
			cp->needsDenting = 0;
		}

		cp->direction = Arc_Tan(cp->where.m[0][2], cp->where.m[2][2]) & 0xFFF;

		// NOTE: reads civilian_ai_data[-1] for the player's car (id 0), as in the binary
		if (civilian_ai_data[(unsigned char)cp->id - 1].IsCopCar
			|| cp == &car_data[PlayerCar]
			|| cp >= &car_data[65])
		{
			cp->rotation.vx = Arc_Tan(-cp->where.m[1][2], SquareRoot0(cp->where.m[1][1] * cp->where.m[1][1] + cp->where.m[1][0] * cp->where.m[1][0])) & 0xFFF;
			cp->rotation.vy = cp->direction;
			cp->rotation.vz = Arc_Tan(cp->where.m[1][0], cp->where.m[1][1]) & 0xFFF;
		}

		cp->location.vx = cp->where.t[0];
		cp->location.vy = cp->where.t[1];
		cp->location.vz = cp->where.t[2];

		cp->velocity.vx = (cp->st.n.linearVelocity[0] + 2048) >> 12;
		cp->velocity.vy = (cp->st.n.linearVelocity[1] + 2048) >> 12;
		cp->velocity.vz = (cp->st.n.linearVelocity[2] + 2048) >> 12;

		// set shadow points
		gte_SetRotMatrix(&cp->where);
		gte_SetTransVector((IVECTOR *)cp->where.t);

		InWater = 0;

		for (n = 0; n < 4; n++)
		{
			gte_ldv0(&car_cos->cPoints[n]);
			gte_stlvnl((IVECTOR *)pointPos);

			FindSurface((IVECTOR *)pointPos, &surfaceNormal, &outpoint);

			cp->shadowPoints[n].vx = outpoint.vx;
			cp->shadowPoints[n].vy = outpoint.vy;
			cp->shadowPoints[n].vz = outpoint.vz;
		}

		cp->inWater = InWater;
	}
}

// [0x4de850] [D]
// Adds the car to this frame's active cars; phys: full physics (the player and chased cars).
void StepCarPhysics(CAR_DATA *cp, int phys)
{
	cp->phys = phys != 0;
	active_car_list[num_active_cars++] = cp;
}

// [0x4de88a] [D]
void ResetPlayerCar(void)
{
	InitCarPhysics(&car_data[PlayerCar], (LONGVECTOR4 *)car_data[PlayerCar].where.t);
}

// [0x4de8bb] [D]
// The ground height under a point: the road map cell's surface model, else the cell height.
int MapHeight(VECTOR4 *pos)
{
	SURFACEINFO *si;
	SIPOLY *polys;
	unsigned int angle;
	int x;
	int z;
	int lx;
	int lz;
	int i;
	int in;

	x = float_to_int(pos->vx);
	z = float_to_int(pos->vz);

	if (!ROADS_GetRoadInfo(x, z))
		return 0;

	si = SurfacePtrs[RouteData.type];

	if (!si)
		return -RouteData.height;

	polys = si->polys;

	// the point in the cell, about its centre
	lx = 750 - abs((x + RoadMapUnitXMid + 750) % 1500);
	lz = 750 - abs((RoadMapUnitZMid + 750 - z) % 1500);

	angle = -RouteData.objectAngle & 0xFFF;

	if (angle == 0)
	{
		x = -lx;
		z = lz;
	}
	else if (angle == 1024)
	{
		x = lz;
		z = lx;
	}
	else if (angle == 2048)
	{
		x = lx;
		z = -lz;
	}
	else if (angle == 3072)
	{
		x = -lz;
		z = -lx;
	}

	for (i = 0; i < si->numpolys; i++)
	{
		if (polys->num_vertices == 4)
			in = PointInQuad2d(x, z, polys->xz);
		else
			in = PointInTri2d(x, z, polys->xz);

		if (in)
			return float_to_int((float)x * polys->normals.a + (float)z * polys->normals.c - polys->normals.d - (float)RouteData.height);

		polys++;
	}

	return -RouteData.height;
}

// [0x4deb02] [D]
// MapHeight for the shadows: reads the road map itself and remembers the last cell and polygon.
float MapHeightShadow(VECTOR4 *pos)
{
	static short hdg[4] = { 0, 1024, 2048, 3072 };

	SIPOLY *polys;
	unsigned int angle;
	unsigned int cell;
	int same;
	int cx;
	int cz;
	int rz;
	int x;
	int z;
	int lx;
	int lz;
	int i;
	int in;

	x = float_to_int(pos->vx);
	z = float_to_int(pos->vz);

	cx = (x + RoadMapUnitXMid + 750) / 1500;
	cz = (RoadMapUnitZMid + 750 - z) / 1500;
	rz = (RoadMapUnitZMid + 750 - z) % 1500;

	same = 1;

	if (cx != last_shadowx || cz != last_shadowz)
		same = 0;

	last_shadowx = cx;
	last_shadowz = cz;

	if (!same)
	{
		if (cx < 0 || cx >= RoadMapWidth || cz < 0 || cz >= RoadMapHeight)
		{
			last_shadowx = 0x7FFFFF;
			last_shadowz = 0x7FFFFF;
			savepp = NULL;
			return 0.0f;
		}

		cell = RoadMapInfoRegions[2 * ((regions_down - 1 - cz / road_region_size) & 1) + ((cx / road_region_size) & 1)]
			[road_region_size * (cz % road_region_size) + cx % road_region_size];

		if (!cell)
		{
			last_shadowx = 0x7FFFFF;
			last_shadowz = 0x7FFFFF;
			savepp = NULL;
			return 0.0f;
		}

		RouteData.type = (cell >> 16) & 0x3FF;
		RouteData.height = (short)cell;
		RouteData.objectAngle = hdg[cell >> 30];

		sip = SurfacePtrs[(cell >> 16) & 0x3FF];
	}

	if (!sip)
	{
		last_shadowx = 0x7FFFFF;
		last_shadowz = 0x7FFFFF;
		savepp = NULL;
		return (float)-RouteData.height;
	}

	lx = 750 - abs((x + RoadMapUnitXMid + 750) % 1500);
	lz = 750 - abs(rz);

	angle = -RouteData.objectAngle & 0xFFF;

	if (angle == 0)
	{
		x = -lx;
		z = lz;
	}
	else if (angle == 1024)
	{
		x = lz;
		z = lx;
	}
	else if (angle == 2048)
	{
		x = lx;
		z = -lz;
	}
	else if (angle == 3072)
	{
		x = -lz;
		z = -lx;
	}

	if (same == 1 && savepp)
	{
		if (savepp->num_vertices == 4)
			in = PointInQuad2d(x, z, savepp->xz);
		else
			in = PointInTri2d(x, z, savepp->xz);

		if (in)
			return (float)x * savepp->normals.a + (float)z * savepp->normals.c - savepp->normals.d - (float)RouteData.height;
	}

	polys = sip->polys;

	for (i = 0; i < sip->numpolys; i++)
	{
		if (polys->num_vertices == 4)
			in = PointInQuad2d(x, z, polys->xz);
		else
			in = PointInTri2d(x, z, polys->xz);

		if (in)
		{
			savepp = polys;
			return (float)x * polys->normals.a + (float)z * polys->normals.c - polys->normals.d - (float)RouteData.height;
		}

		polys++;
	}

	savepp = NULL;
	return (float)-RouteData.height;
}

// [0x4defd9] [D]
// MapHeight for an int position.
int MapHeightInt(IVECTOR *pos)
{
	SURFACEINFO *si;
	SIPOLY *polys;
	unsigned int angle;
	int x;
	int z;
	int lx;
	int lz;
	int i;
	int in;

	x = pos->vx;
	z = pos->vz;

	if (!ROADS_GetRoadInfo(pos->vx, z))
		return 0;

	si = SurfacePtrs[RouteData.type];

	if (!si)
		return -RouteData.height;

	polys = si->polys;

	lx = 750 - abs((x + RoadMapUnitXMid + 750) % 1500);
	lz = 750 - abs((RoadMapUnitZMid + 750 - z) % 1500);

	angle = -RouteData.objectAngle & 0xFFF;

	if (angle == 0)
	{
		x = -lx;
		z = lz;
	}
	else if (angle == 1024)
	{
		x = lz;
		z = lx;
	}
	else if (angle == 2048)
	{
		x = lx;
		z = -lz;
	}
	else if (angle == 3072)
	{
		x = -lz;
		z = -lx;
	}

	for (i = 0; i < si->numpolys; i++)
	{
		if (polys->num_vertices == 4)
			in = PointInQuad2d(x, z, polys->xz);
		else
			in = PointInTri2d(x, z, polys->xz);

		if (in)
			return float_to_int((float)x * polys->normals.a + (float)z * polys->normals.c - polys->normals.d - (float)RouteData.height);

		polys++;
	}

	return -RouteData.height;
}

// [0x4df202] [D]
// The ground under pos: out = pos on the ground, normal = the surface normal (1/4096). Returns
// the friction (4096, 2048 on grass and water).
int FindSurface(IVECTOR *pos, IVECTOR *normal, IVECTOR *out)
{
	SURFACEINFO *si;
	SIPOLY *polys;
	float a;
	float b;
	float c;
	float t;
	float height;
	int angle;
	int x;
	int z;
	int lx;
	int lz;
	int i;
	int in;

	localOnGrass = 0;

	out->vx = pos->vx;
	x = out->vx;
	out->vz = pos->vz;
	z = out->vz;

	if (!ROADS_GetRoadInfo(x, z))
	{
		normal->vx = 0;
		normal->vy = 4096;
		normal->vz = 0;
		out->vy = 0;
		return 4096;
	}

	si = SurfacePtrs[RouteData.type];

	if (!si)
	{
		out->vy = -RouteData.height;

		normal->vx = 0;
		normal->vy = 4096;
		normal->vz = 0;

		if (model_pointers[RouteData.type] && (model_pointers[RouteData.type]->shape_flags & SHAPE_FLAG_WATER) && pos->vy - out->vy < 20)
		{
			out->vy -= float_to_int(fsintab[gBobIndex] * 30.0f) + 200;
			InWater++;
			return 2048;
		}

		if (model_pointers[RouteData.type] && (model_pointers[RouteData.type]->flags2 & 0x4000))
		{
			out->vy += isin(2 * (z + x)) >> 9;
			return 2048;
		}

		return 4096;
	}

	polys = si->polys;

	lx = 750 - abs((x + RoadMapUnitXMid + 750) % 1500);
	lz = 750 - abs((RoadMapUnitZMid + 750 - z) % 1500);

	angle = -RouteData.objectAngle & 0xFFF;

	if (angle == 0)
	{
		x = -lx;
		z = lz;
	}
	else if (angle == 1024)
	{
		x = lz;
		z = lx;
	}
	else if (angle == 2048)
	{
		x = lx;
		z = -lz;
	}
	else if (angle == 3072)
	{
		x = -lz;
		z = -lx;
	}

	for (i = 0; ; i++)
	{
		if (i >= si->numpolys)
		{
			normal->vx = 0;
			normal->vy = 4096;
			normal->vz = 0;
			out->vy = -RouteData.height;
			return 4096;
		}

		if (polys->num_vertices == 4)
			in = PointInQuad2d(x, z, polys->xz);
		else
			in = PointInTri2d(x, z, polys->xz);

		if (in)
			break;

		polys++;
	}

	a = polys->normals.a;
	b = polys->normals.b;
	c = polys->normals.c;

	height = (float)x * a + (float)z * c - polys->normals.d;
	height -= (float)RouteData.height;

	// the normal back to world space
	if (angle == 0)
	{
		a = -a;
		c = -c;
	}
	else if (angle == 1024)
	{
		t = -a;
		a = c;
		c = t;
	}
	else if (angle == 3072)
	{
		t = a;
		a = -c;
		c = t;
	}

	normal->vx = float_to_int(4096.0f * a);
	normal->vy = float_to_int(4096.0f * b);
	normal->vz = float_to_int(4096.0f * c);
	out->vy = float_to_int(height);

	if (model_pointers[RouteData.type] && (model_pointers[RouteData.type]->flags2 & 0x4000))
	{
		localOnGrass = 1;
		out->vy += isin(2 * (z + x)) >> 9;
		return 2048;
	}

	return 4096;
}

// [0x4df78c] [D]
void InitialiseCarHandling(void)
{
	CAR_COSMETICS *car_cos;
	CAR_DATA *cp;
	int id;
	LONGVECTOR4 startpos;

	cp = car_data;

	memset(startpos, 0, sizeof(startpos));

	PlayerWipedOutTimer = 0;

	wheelheight[3] = 0;
	wheelheight[2] = 0;
	wheelheight[1] = 0;
	wheelheight[0] = 0;
	steeringTorque = 0;

	gPlayerCarDamage = 0;
	DebrisTimer = 0;
	WasOnGround = 0;
	gCruiseControlSpeed = 0;
	hit_delay = 0;

	last_track_state[0] = -1;
	last_track_state[1] = -1;
	last_track_state[2] = -1;
	last_track_state[3] = -1;

	other_last_track_state[0] = -1;
	other_last_track_state[1] = -1;
	other_last_track_state[2] = -1;
	other_last_track_state[3] = -1;

	InWater = 0;
	save_track_count = 0;

	rear_only = 0;
	lay_down_tracks = 0;
	continuous_track = 0;
	other_continous_track = 0;
	AllowSkidSound = 1;
	localOnGrass = 0;

	gTimeInWater = 15;

	ClearMem(_tp, sizeof(_tp));
	ClearMem(_d0, sizeof(_d0));
	ClearMem(_d1, sizeof(_d1));

	sub_4DC580();

	gCruiseControlSpeed = 0;
	num_active_cars = 0;
	hit_delay = 0;

	for (id = 0; id < 66; id++)
	{
		cp->id = id;

		if (gMissionSetting)
			cp->model = ChaseCarType;
		else if (id < 65)
			cp->model = 0;

		if (id != PlayerCar && id < 65)
			InitCarPhysics(cp, &startpos);

		cp++;
	}

	DebrisTimer = 0;
	WasOnGround = 1;

	if (Reuse_player_data)
	{
		FelonyRating = MStart.felony;
		car_data[0].direction = MStart.direction;
		TempBuildHandlingMatrix(car_data, 0);
	}

	if (!gMissionRecording && !gMissionReplaying)
	{
		car_cos = &car_cosmetics[(unsigned char)car_data[0].model];
		car_data[0].location.vy = MapHeightInt(&car_data[0].location) - car_cos->wheelDisp[0].vy;
		TempBuildHandlingMatrix(car_data, 1);

		for (id = 3; id >= 0; id--)
			car_data[0].wheel[id].susCompression = 16;
	}

	if (gMissionRecording)
		gAutoBrake = 1;
}

// [0x4dfa6a] [D]
// Turns the pad bits into the car's steering, thrust, handbrake and wheelspin (horn and siren
// for the player).
void ProcessCarPad(int id, int pad)
{
	int dx;
	int dz;
	int dist;
	int vel;
	CAR_DATA *cp;
	int use_analogue;

	cp = &car_data[id];

	if (id == 0)
	{
		if (pad & CAR_PAD_HORN)
		{
			CheckHornReaction(car_data);

			if (gPlayerCarModelType == 10)
			{
				// the siren toggles on each press
				if (!gPreviousHornState)
				{
					if (gPlayerSirenOn)
					{
						gPlayerSirenOn = 0;
						horn_volume = -10000;
					}
					else
					{
						gPlayerSirenOn = 1;
						horn_volume = 0;
					}
				}

				gPreviousHornState = 1;
			}
			else if (horn_volume != 0)
			{
				horn_volume += 4000;

				if (horn_volume > 0)
					horn_volume = 0;
			}
		}
		else if (gPlayerCarModelType == 10)
		{
			gPreviousHornState = 0;
		}
		else
		{
			if (horn_volume != -10000)
			{
				horn_volume -= 4000;

				if (horn_volume < -10000)
					horn_volume = -10000;
			}

			horn_volume = -10000;
		}

		SetSoundVol(2, horn_volume);
		ControlBitchingSounds(pad);
		DoTrainingMode(pad);
	}

	if (gMissionRecording && id == 65)
	{
		ControlBitchingSounds(pad);
		DoTrainingMode(pad);
	}

	if (cp->autoBrake > 90)
		cp->autoBrake = 90;

	if (pad & CAR_PAD_HANDBRAKE)
	{
		gCruiseControlSpeed = 0;
		cp->handbrake = 1;
	}
	else
	{
		cp->handbrake = 0;

		if (!(pad & CAR_PAD_WHEELSPIN) || CutSceneActive)
		{
			cp->wheelspin = 0;
		}
		else
		{
			gCruiseControlSpeed = 0;
			cp->wheelspin = 1;
		}

		if (cp->wheelspin && cp->wheel_speed > 0xA2000)
		{
			cp->wheelspin = 0;
			pad |= CAR_PAD_ACCEL;
		}
	}

	use_analogue = 0;

	if (id >= 65 || id == PlayerCar)
	{
		if ((pad & CAR_PAD_CRUISECONTROL) && !gCruiseControlSpeed)
			gCruiseControlSpeed = (cp->wheel_speed + 2048) >> 12;

		if (ReplayMode == 2)
		{
			if (ReplayAnalogue)
				use_analogue = 1;
		}
		else if ((OldJoyIsAnalogue && id == 0)
			|| (OldJoyIsAnalogue && gDoTraining && id != 0 && gTrainingMode == 2))
		{
			use_analogue = 1;
		}

		if (use_analogue)
		{
			cp->wheel_angle = 4 * JoyAnalogueX;
		}
		else
		{
			if (pad & CAR_PAD_FASTSTEER)
			{
				if (pad & CAR_PAD_RIGHT)
				{
					cp->wheel_angle += 32;

					if (cp->wheel_angle > 511)
						cp->wheel_angle = 511;
				}

				if (pad & CAR_PAD_LEFT)
				{
					cp->wheel_angle -= 32;

					if (cp->wheel_angle < -511)
						cp->wheel_angle = -511;
				}
			}
			else
			{
				if (pad & CAR_PAD_RIGHT)
				{
					cp->wheel_angle += 16;

					if (cp->wheel_angle > 352)
						cp->wheel_angle = 352;
				}

				if (pad & CAR_PAD_LEFT)
				{
					cp->wheel_angle -= 16;

					if (cp->wheel_angle < -352)
						cp->wheel_angle = -352;
				}
			}

			// straighten up
			if (!(pad & (CAR_PAD_RIGHT | CAR_PAD_LEFT)))
			{
				if (cp->wheel_angle < -64)
					cp->wheel_angle += 32;
				else if (cp->wheel_angle > 64)
					cp->wheel_angle -= 32;
				else
					cp->wheel_angle = 0;
			}
		}
	}

	if ((pad & (CAR_PAD_RIGHT | CAR_PAD_LEFT)) || (use_analogue && (JoyAnalogueX < -63 || JoyAnalogueX > 63)))
		cp->autoBrake++;
	else
		cp->autoBrake = 0;

	if (!(pad & CAR_PAD_BRAKE) || (pad & CAR_PAD_HANDBRAKE))
	{
		if (!(pad & CAR_PAD_ACCEL))
		{
			cp->thrust = 0;
		}
		else
		{
			cp->thrust = car_cosmetics[(unsigned char)cp->model].powerRatio;

			gCruiseControlSpeed = 0;

			// rubberband: more thrust the further the chased car is
			if (gMissionReplaying && !gDoTraining && cp->id == 0)
			{
				dx = abs(car_data[65].location.vx - cp->location.vx);
				dz = abs(car_data[65].location.vz - cp->location.vz);

				if (dx >= dz)
					dist = dz / 2 + dx;
				else
					dist = dx / 2 + dz;

				dist = (dist >> 2) + dist;

				if (dist > 18000)
					cp->thrust += 3000;
				else if (dist > 13500)
					cp->thrust += 2500;
				else if (dist > 9000)
					cp->thrust += 2000;
				else if (dist > 6000)
					cp->thrust += 1500;
				else
					cp->thrust += 500;
			}

			if (id == 0 || id == 65)
			{
				if (id == 0)
					cp->thrust = gDodgySpeedFactor * cp->thrust / 4096;

				if (cp->changingGear)
					cp->thrust = 1;
			}
		}
	}
	else
	{
		vel = -((cp->wheel_speed + 2048) >> 12);

		gCruiseControlSpeed = 0;

		if (vel <= 22)
			cp->thrust = -car_cosmetics[(unsigned char)cp->model].powerRatio;
		else
			cp->thrust = ((256 - (vel - 22)) * -car_cosmetics[(unsigned char)cp->model].powerRatio) >> 8;

		if (cp->wheel_speed < 0 && id == 0)
			cp->thrust = gDodgySpeedFactor * cp->thrust / 4096;
	}

	if (id == 0)
	{
		if (!gDoTraining)
		{
			if (abs(gCruiseControlSpeed - ((cp->wheel_speed + 2048) >> 12)) > 36)
				gCruiseControlSpeed = 0;

			if (gCruiseControlSpeed)
			{
				cp->thrust = 22 * (2 * gCruiseControlSpeed - ((cp->wheel_speed + 2048) >> 12));

				if (cp->thrust > 8000)
					cp->thrust = 8000;
				else if (cp->thrust < -8000)
					cp->thrust = -8000;

				if (cp->thrust < 0)
					cp->thrust = 0;
			}
		}

		if (gPlayerCarModelType > 15)
			cp->thrust *= 2;

		if (gCheatFlags.SuperFastCars)
			cp->thrust *= 2;
	}

	if (pad & CAR_PAD_LEFT)
		cp->analog_steer = 1;
	else if (pad & CAR_PAD_RIGHT)
		cp->analog_steer = -1;
	else
		cp->analog_steer = 0;
}

// [0x4e02e9] [D]
// The player's skid sound and tyre tracks.
void CheckCarEffects(int id)
{
	int rearsound;
	CAR_DATA *cp;
	int skidsound;
	int front_vel;

	cp = &car_data[id];

	if (GamePaused)
		return;

	lay_down_tracks = 0;

	front_vel = cp->front_vel;

	if (id != 0)
		return;

	if (cp->wheelsOnGround && (cp->wheelspin || abs(cp->rear_vel) > 15000))
	{
		rear_only = 1;
		lay_down_tracks = 1;

		if (cp->wheelspin)
			rearsound = 10000;
		else
			rearsound = (abs(cp->rear_vel) - 15000) / 2 + 1;

		skidsound = rearsound;
	}
	else
	{
		skidsound = 0;
	}

	if (!gDoTraining)
	{
		jump_debris(cp);
		CheckAddGrass();
	}

	if (abs(front_vel) > 15000)
	{
		rear_only = 0;
		lay_down_tracks = 1;
	}

	if (!skidsound || (cp->wheel[1].onGrass && cp->wheel[3].onGrass) || gDoRain || gDoSnow)
	{
		SetSoundVol(4, -10000);
	}
	else
	{
		if (skidsound > 10000)
			skidsound = 10000;

		SetSoundPos(4, (float)car_data[0].location.vx, (float)-car_data[0].location.vy, (float)car_data[0].location.vz);
		SetSoundFreq(4, -1);
		SetSoundVol(4, skidsound - 10000);
	}

	GetTyreTrackPositions();

	if (!lay_down_tracks)
	{
		SetTyreTrackOldPositions();

		last_track_state[0] = -1;
		last_track_state[1] = -1;
		last_track_state[2] = -1;
		last_track_state[3] = -1;
		return;
	}

	if (!ROADS_GetRoadInfo(cp->location.vx, cp->location.vz)
		|| !model_pointers[RouteData.type]
		|| (model_pointers[RouteData.type]->shape_flags & SHAPE_FLAG_WATER))
	{
		last_track_state[0] = -1;
		last_track_state[1] = -1;
		last_track_state[2] = -1;
		last_track_state[3] = -1;
		return;
	}

	if (cp->wheel[0].susCompression)
	{
		continuous_track = last_track_state[0] != -1;
		AddTyreTrack(0);
		last_track_state[0] = 1;
	}
	else
	{
		last_track_state[0] = -1;
	}

	if (cp->wheel[2].susCompression)
	{
		continuous_track = last_track_state[2] != -1;
		AddTyreTrack(2);
		last_track_state[2] = 1;
	}
	else
	{
		last_track_state[2] = -1;
	}

	if (rear_only)
	{
		last_track_state[1] = -1;
		last_track_state[3] = -1;
	}
	else
	{
		if (cp->wheel[1].susCompression)
		{
			continuous_track = last_track_state[1] != -1;
			AddTyreTrack(1);
			last_track_state[1] = 1;
		}
		else
		{
			last_track_state[1] = -1;
		}

		if (cp->wheel[3].susCompression)
		{
			continuous_track = last_track_state[3] != -1;
			AddTyreTrack(3);
			last_track_state[3] = 1;
		}
		else
		{
			last_track_state[3] = -1;
		}
	}

	SetTyreTrackOldPositions();
}

// [0x4e07d4] [D]
// Tyre tracks of another car (the chased car of a mission replay).
void CheckOtherCarEffects(int id)
{
	int rear;
	int front;
	int i;
	CAR_DATA *cp;

	cp = &car_data[id];

	if (GamePaused || gDoRain || (id == 65 && dword_CCCC34 && !dword_CCC8A0))
		return;

	front = 0;
	rear = 0;

	if (cp->wheelsOnGround && (cp->wheelspin || abs(cp->rear_vel) > 15000))
		rear = 1;

	if (abs(cp->front_vel) > 15000)
		front = 1;

	GetCarTyreTrackPositions(cp, other_wp);

	if (front || rear)
	{
		for (i = 0; i < 4; i++)
		{
			if (cp->wheel[i].susCompression && ((i & 1) == 0 || rear) && ((i & 1) != 0 || front))
			{
				other_continous_track = other_last_track_state[i] != -1;
				AddOtherTyreTrack(i, &other_wp[i]);
				other_last_track_state[i] = 1;
			}
			else
			{
				other_last_track_state[i] = -1;
			}
		}
	}
	else
	{
		other_last_track_state[0] = -1;
		other_last_track_state[1] = -1;
		other_last_track_state[2] = -1;
		other_last_track_state[3] = -1;
	}

	SetOtherTyreTrackOldPositions(other_wp);
}

// [0x4e099d] [D]
// Puts a car that has just been placed (pinged in) on the ground.
void InitPingedCar(CAR_DATA *cp, CIV_STATE *ai)
{
	CAR_COSMETICS *car_cos;
	int dist;
	int dx;
	int dz;
	int i;

	if (gMissionRecording || gMissionReplaying)
	{
		dx = abs(car_data[65].location.vx - cp->location.vx);
		dz = abs(car_data[65].location.vz - cp->location.vz);
	}
	else
	{
		dx = abs(car_data[0].location.vx - cp->location.vx);
		dz = abs(car_data[0].location.vz - cp->location.vz);
	}

	if (dx >= dz)
		dist = dz / 2 + dx;
	else
		dist = dx / 2 + dz;

	// NOTE: as Hex-Rays shows it, (dist < 40500) != 6000: always true
	if ((dist < 40500) != 6000 || gMissionRecording || gMissionReplaying)
	{
		car_cos = &car_cosmetics[(unsigned char)cp->model];

		cp->location.vy = MapHeightInt(&cp->location) - car_cos->wheelDisp[0].vy;

		for (i = 3; i >= 0; i--)
			cp->wheel[i].susCompression = 16;

		ai->pinged = 1;
	}
	else
	{
		ai->pinged = 0;
	}
}

// [0x4e0b01] [D]
// Marks the cars whose bounding boxes (grown by their velocity) overlap as maybe colliding.
void CheckCarToCarCollisions(void)
{
	int fz;
	int sz;
	int zz;
	int xx;
	short cbY;
	BOUND_BOX *bb;
	short cbZ;
	short cbX;
	int i;
	CAR_DATA *cp;
	int car;
	BOUND_BOX *bb2;
	int cars_to_update;
	BOUND_BOX *bb1;

	cp = car_data;

	if (ghost_mode || gDoTraining)
		return;

	bb = bbox;

	if (gMissionReplaying || gMissionRecording)
		cars_to_update = 66;
	else
		cars_to_update = 65;

	for (car = 0; car < cars_to_update; car++)
	{
		cbX = car_cosmetics[(unsigned char)cp->model].colBox.vx;
		cbZ = car_cosmetics[(unsigned char)cp->model].colBox.vz;
		cbY = car_cosmetics[(unsigned char)cp->model].colBox.vy;

		sz = abs(9 * cbZ / 8 * cp->where.m[0][2]);
		xx = cbY + ((sz + abs(9 * cbX / 8 * cp->where.m[0][0]) + 2048) >> 12);
		fz = abs(9 * cbZ / 8 * cp->where.m[2][2]);
		zz = cbY + ((fz + abs(9 * cbX / 8 * cp->where.m[2][0]) + 2048) >> 12);

		bb->x0 = (cp->where.t[0] - xx) / 16;
		bb->z0 = (cp->where.t[2] - zz) / 16;
		bb->x1 = (xx + cp->where.t[0]) / 16;
		bb->z1 = (zz + cp->where.t[2]) / 16;

		if (cp->st.n.linearVelocity[0] >= 0)
			bb->x1 += ((cp->st.n.linearVelocity[0] + 2048) >> 12) / 8;
		else
			bb->x0 += ((cp->st.n.linearVelocity[0] + 2048) >> 12) / 8;

		if (cp->st.n.linearVelocity[2] >= 0)
			bb->z1 += ((cp->st.n.linearVelocity[2] + 2048) >> 12) / 8;
		else
			bb->z0 += ((cp->st.n.linearVelocity[2] + 2048) >> 12) / 8;

		bb->y0 = (cp->location.vy - 2400) / 16;
		bb->y1 = (cp->location.vy + 2400) / 16;

		bb++;
		cp++;
	}

	bb1 = bbox;

	for (car = 0; car < cars_to_update; car++)
	{
		bb2 = bb1 + 1;

		for (i = car + 1; i < cars_to_update; i++)
		{
			if (bb1->x1 > bb2->x0
				&& bb1->z1 > bb2->z0
				&& bb1->x0 < bb2->x1
				&& bb1->z0 < bb2->z1
				&& bb1->y1 > bb2->y0
				&& bb1->y0 < bb2->y1
				&& (car == 0 || civilian_ai_data[car - 1].ctrlState != 0)
				&& civilian_ai_data[i - 1].ctrlState != 0)
			{
				car_data[car].mayBeColliding = 1;
				car_data[i].mayBeColliding = 1;

				NotifyAIRunAFewFrames(car);
				NotifyAIRunAFewFrames(i);
			}

			bb2++;
		}

		bb1++;
	}
}

// [0x4e0f39] [D]
void StepCars(void)
{
	int i;

	for (i = 0; i < num_active_cars; i++)
		StepOneCar(active_car_list[i], active_car_list[i]->phys == 1);
}

// [0x4e0f8c] [D]
// The forces on one car this frame: gravity (and a bomb's blast), body points under the ground,
// then each wheel's suspension, side friction, thrust and brakes.
void StepOneCar(CAR_DATA *cp, int phys)
{
	int airSpeed;
	int angle_scale;
	int doAutoBrake;
	int sidevel;
	int lfz;
	LONG pointVel[3];
	int lfx;
	int forceZ;
	LONG forceY;
	int slidevel;
	int forceX;
	int wheelHeight;
	LONGVECTOR4 wheelSurfacePoint;
	LONGVECTOR4 wheelSurfaceNormal;
	LONGVECTOR4 wheelPos;
	WHEEL *wheel;
	LONG newCompression;
	int oldCompression;
	int susForce;
	int autoBrakeGrip;
	int q;
	int autoBrakeOn;
	int cdx;
	int rearFS;
	int frontFS;
	int sdz;
	int sdx;
	LONG dampZ;
	int frictionScale;
	int cdz;
	LONG dampY;
	LONG dampX;
	LONGVECTOR4 direction1;
	LONGVECTOR4 direction2;
	LONG loss;
	LONG lever_dot_n;
	LONG displacementsquared;
	int denom;
	int componant;
	LONG impulse;
	LONGVECTOR4 surfacePoint;
	LONGVECTOR4 deepestNormal;
	LONGVECTOR4 lever;
	LONGVECTOR4 pointPos;
	LONGVECTOR4 deepestLever;
	LONG newLift;
	LONGVECTOR4 reaction;
	int count;
	int lift;
	LONG b;
	LONG a;
	int bz;
	int bx;
	int bdist;
	unsigned char cop_slot;
	CAR_LOCALS *cl;
	int blastZ;
	CAR_LOCALS _cl;
	CAR_COSMETICS *car_cos;
	int i;
	int blastY;
	int blastX;
	int dir;
	int frontvel;
	int friction_coef;
	int rearvel;
	int speed;
	int steer;

	cl = &_cl;

	blastX = 0;
	blastY = 0;
	blastZ = 0;

	_cl.extraangulardamping = 0;

	car_cos = &car_cosmetics[(unsigned char)cp->model];

	for (i = 2; i >= 0; i--)
	{
		cp->st.n.fposition[i] &= 0xF;
		cp->st.n.fposition[i] += 16 * cp->where.t[i];

		cl->vel[i] = cp->st.n.linearVelocity[i];
		cl->where[i] = cp->where.t[i];
		cl->avel[i] = cp->st.n.angularVelocity[i];
		cl->aacc[i] = cp->aacc[i];
	}

	if (TriggerBomb && cp->id != 0)
	{
		bx = cp->location.vx - (int)ex_centre.vx;
		bz = cp->location.vz - (int)ex_centre.vz;

		if (bx < 10000 && bx > -10000 && bz < 10000 && bz > -10000)
		{
			bdist = bz * bz + bx * bx;
			blastX = bx * (122880000 / bdist);
			blastY = -218440000 / bdist;
			blastZ = bz * (122880000 / bdist);
		}

		if (ExplosionJustHappened)
		{
			cp->damage[5] = 4095;
			cp->damage[4] = 4095;
			cp->damage[3] = 4095;
			cp->damage[2] = 4095;
			cp->damage[1] = 4095;
			cp->damage[0] = 4095;

			cp->totalDamage = 24570;

			QueueDentCar((unsigned char)cp->id - 1);

			cop_slot = civilian_ai_data[(unsigned char)cp->id - 1].IsCopCar;

			if (cop_slot)
				cop_data[cop_slot - 1].dying = 1;
		}
	}

	cl->acc[0] = blastX;
	cl->acc[1] = blastY + GRAVITY;		// apply gravity
	cl->acc[2] = blastZ;

	a = (cl->vel[0] + 2048) >> 12;
	b = (cl->vel[2] + 2048) >> 12;

	if (a < 0)
		a = -a;

	if (b < 0)
		b = -b;

	if (a >= b)
		speed = b / 2 + a;
	else
		speed = a / 2 + b;

	cp->speed = speed;

	gte_SetRotMatrix(&cp->where);
	gte_SetTransVector((IVECTOR *)cl->where);

	lift = 0;

	// all 12 body points when the car is on its side or roof, else the 4 lowest
	count = 12;

	if (cp->where.m[1][1] > 2048)
		count = 4;

	i = count;

	while (i--)
	{
		gte_ldv0(&car_cos->cPoints[i]);
		gte_stlvnl((IVECTOR *)pointPos);

		deepestLever[0] = pointPos[0] - cl->where[0];
		deepestLever[1] = pointPos[1] - cl->where[1];
		deepestLever[2] = pointPos[2] - cl->where[2];

		friction_coef = FindSurface((IVECTOR *)pointPos, (IVECTOR *)deepestNormal, (IVECTOR *)surfacePoint);

		if (cp->model == 1)
			friction_coef = 4096;
		else if (cp->id == 0 || (unsigned char)cp->id >= 65)
			friction_coef = ((32400 - wetness) * friction_coef) >> 15;

		if (i < 4)
		{
			cp->shadowPoints[i].vx = surfacePoint[0];
			cp->shadowPoints[i].vy = surfacePoint[1];
			cp->shadowPoints[i].vz = surfacePoint[2];
		}

		newLift = surfacePoint[1] - pointPos[1];

		if (newLift <= 0)
			continue;

		// (the original also works out an unused friction index here)

		if (newLift > lift)
			lift = newLift;

		lever[0] = (deepestLever[2] * cl->avel[1] - deepestLever[1] * cl->avel[2] + 2048) >> 12;
		lever[1] = (deepestLever[0] * cl->avel[2] - deepestLever[2] * cl->avel[0] + 2048) >> 12;
		lever[2] = (deepestLever[1] * cl->avel[0] - deepestLever[0] * cl->avel[1] + 2048) >> 12;

		lever[0] += cl->vel[0];
		lever[1] += cl->vel[1];
		lever[2] += cl->vel[2];

		lever_dot_n = (deepestNormal[1] * deepestLever[1]
			+ deepestNormal[0] * deepestLever[0]
			+ deepestNormal[2] * deepestLever[2]
			+ 2048) >> 12;

		displacementsquared = deepestLever[2] * deepestLever[2]
			+ deepestLever[1] * deepestLever[1]
			+ deepestLever[0] * deepestLever[0]
			- lever_dot_n * lever_dot_n;

		denom = ((110 * displacementsquared + 2048) >> 12) + 4096;

		impulse = -2048 * (((deepestNormal[1] * lever[1] + deepestNormal[0] * lever[0] + deepestNormal[2] * lever[2] + 2048) >> 12) / denom);

		for (componant = 2; componant >= 0; componant--)
		{
			loss = 96 * lever[componant];

			if (loss > frictionLimit[componant])
				loss = frictionLimit[componant];
			else if (loss < -frictionLimit[componant])
				loss = -frictionLimit[componant];

			reaction[componant] = (deepestNormal[componant] * impulse - loss + 2048) >> 12;
		}

		if (impulse > 20000)
		{
			if (gNight)
			{
				direction2[0] = 0;
				direction2[1] = 50;
				direction2[2] = 0;
				direction2[3] = 0;

				Setup_Sparks((IVECTOR *)surfacePoint, (IVECTOR *)direction2, 15, 1);
			}
			else
			{
				direction1[0] = 0;
				direction1[1] = 40;
				direction1[2] = 0;
				direction1[3] = 0;

				Setup_Debris((IVECTOR *)surfacePoint, (IVECTOR *)direction1, 10, 0, (unsigned char)cp->model);
			}

			// NOTE: car_data[0] rather than the player car, as in the binary
			if ((cp == car_data || cp >= &car_data[65]) && impulse > 30000)
				Play3DSound(3, 3, (float)surfacePoint[0], (float)surfacePoint[1], (float)surfacePoint[2]);
		}

		cl->aacc[2] -= (reaction[0] * deepestLever[1] + 2048) >> 12;
		cl->aacc[1] += (reaction[0] * deepestLever[2] + 2048) >> 12;
		cl->acc[0] += reaction[0];

		cl->aacc[0] -= (reaction[1] * deepestLever[2] + 2048) >> 12;
		cl->aacc[2] += (reaction[1] * deepestLever[0] + 2048) >> 12;
		cl->acc[1] += reaction[1];

		cl->aacc[1] -= (reaction[2] * deepestLever[0] + 2048) >> 12;
		cl->aacc[0] += (reaction[2] * deepestLever[1] + 2048) >> 12;
		cl->acc[2] += reaction[2];
	}

	if (lift)
	{
		cl->where[1] += lift;
		cp->st.n.fposition[1] += 16 * lift;
		cp->location.vy += lift;

		gte_SetTransVector((IVECTOR *)cl->where);

		cl->extraangulardamping = 1;

		if (lift > 120)
			cp->st.n.linearVelocity[1] = 0;
	}

	// **** AddWheelForces ****
	if (speed >= 32)
		frictionScale = 1824 - speed - 400;
	else
		frictionScale = 4096 - 72 * speed - 400;

	cdx = isin(cp->direction);
	cdz = icos(cp->direction);

	dampX = -12 * cl->vel[0];
	dampY = -12 * cl->vel[1];
	dampZ = -12 * cl->vel[2];

	dir = cp->wheel_angle;

	sdx = isin(dir + cp->direction);
	sdz = icos(dir + cp->direction);

	if (frictionScale > 3300)
		frictionScale = 3300;

	// **** GetFrictionScales ****
	if (cp->thrust < 0)
		frontFS = 1453;
	else if (cp->thrust > 0)
		frontFS = 820;
	else
		frontFS = 937;

	autoBrakeOn = 0;

	if (gAutoBrake)
		autoBrakeOn = cp->id == 0 || cp->id == 65;
	else if (cp->id == 65 && (!gDoTraining || gTrainingMode != 2))
		autoBrakeOn = 1;

	if (autoBrakeOn && !cp->wheelspin && cp->wheel_speed > 0)
	{
		q = cp->autoBrake;

		if (q > 0)
		{
			if (q > 26)
				q = 26;

			frontFS += 22 * q;
		}
	}

	if (cp->thrust < 0 && cp->wheel_speed > 0xF000 && phys)
	{
		cp->wheel[3].locked = 1;
		cp->wheel[2].locked = 1;
		cp->wheel[1].locked = 1;
		cp->wheel[0].locked = 1;
	}
	else
	{
		cp->wheel[3].locked = 0;
		cp->wheel[2].locked = 0;
		cp->wheel[1].locked = 0;
		cp->wheel[0].locked = 0;
	}

	if (cp->handbrake)
	{
		if (cp->thrust >= 0)
			cp->thrust = 0;

		if (cp->wheel_speed <= 0)
			frontFS -= 375;
		else
			frontFS += 656;

		cp->wheel[1].locked = 1;
		cp->wheel[3].locked = 1;

		cp->wheelspin = 0;
	}
	else if (cp->wheelspin)
	{
		frontFS += 600;
	}

	if (cp->wheel_speed < 0 && cp->thrust >= 0 && !cp->handbrake)
		frontFS -= 400;

	rearFS = 1920 - frontFS;

	if (cp->wheelspin)
	{
		if (gCheatFlags.SuperFastCars)
			cp->thrust = 14000;
		else
			cp->thrust = 7000;

		if ((unsigned char)cp->model > 15)
			cp->thrust *= 2;

		if (cp->id == 0)
			cp->thrust = gDodgySpeedFactor * cp->thrust / 4096;
	}

	if (cp->thrust < 0 && cp->wheel_speed > 0xF000 && phys)
	{
		frontFS = 10 * frontFS / 8;
		rearFS = 10 * rearFS / 8;

		if (cp->id == 0)
		{
			frontFS = gDodgyBrakeFactor * frontFS / 4096;
			rearFS = gDodgyBrakeFactor * rearFS / 4096;
		}
	}
	else
	{
		autoBrakeGrip = 0;

		if (gAutoBrake)
			autoBrakeGrip = cp->id == 0 || cp->id == 65;
		else if (cp->id == 65 && (!gDoTraining || gTrainingMode != 2))
			autoBrakeGrip = 1;

		if (autoBrakeGrip)
		{
			if (cp->wheel[0].onGrass)
				frontFS = 39 * frontFS / 32;
			else
				frontFS = 35 * frontFS / 32;
		}
		else if (phys && cp->wheel[0].onGrass)
		{
			frontFS = 36 * frontFS / 32;
		}
	}

	if (cp->fourWheelDrive == 1)
	{
		if (gDifficulty == 0)
		{
			frontFS = 45 * frontFS / 32;
			rearFS = 45 * rearFS / 32;
		}
		else if (gDifficulty == 1)
		{
			frontFS = 55 * frontFS / 32;
			rearFS = 55 * rearFS / 32;
		}
		else
		{
			frontFS = 68 * frontFS / 32;
			rearFS = 68 * rearFS / 32;
		}
	}
	else if (cp != car_data && cp->id != 65)	// NOTE: car_data[0] rather than the player car, as in the binary
	{
		frontFS = 29 * frontFS / 32;
		rearFS = 29 * rearFS / 32;
	}
	// **** / GetFrictionScales ****

	frontvel = 0;
	rearvel = 0;

	cp->wheelsOnGround = 0;

	for (i = 3; i >= 0; i--)
	{
		wheel = &cp->wheel[i];

		gte_ldv0(&car_cos->wheelDisp[i]);
		gte_stlvnl((IVECTOR *)wheelPos);

		friction_coef = FindSurface((IVECTOR *)wheelPos, (IVECTOR *)wheelSurfaceNormal, (IVECTOR *)wheelSurfacePoint);

		// the player's landing thump
		// NOTE: car_data[0] rather than the player car, as in the binary
		if (cp == car_data)
		{
			wheelHeight = wheelSurfacePoint[1];

			if (wheelSurfacePoint[1] - wheelheight[i] > 16)
			{
				if (Play3DSound(29, 16, (float)wheelSurfacePoint[0], (float)-wheelSurfacePoint[1], (float)wheelSurfacePoint[2]) != -1)
				{
					SetSoundFreq(16, 5000);
					SetSoundVol(16, cameraview == 2 ? 0 : -7000);
				}
			}

			wheelheight[i] = wheelHeight;
		}

		if (cp->model == 1)
			friction_coef = 4096;
		else if (cp->id == 0 || (unsigned char)cp->id >= 65)
			friction_coef = ((32400 - wetness) * friction_coef) >> 15;

		// civilian cars drive on a flat, grippy road
		if (cp != &car_data[PlayerCar]
			&& cp < &car_data[65]
			&& civilian_ai_data[(unsigned char)cp->id - 1].wrecked != 1
			&& cp->model != 1)
		{
			wheelSurfaceNormal[0] = 0;
			wheelSurfaceNormal[1] = 4096;
			wheelSurfaceNormal[2] = 0;

			friction_coef = 10240;
		}

		wheel->onGrass = localOnGrass;

		oldCompression = (unsigned char)wheel->susCompression;
		newCompression = wheelSurfacePoint[1] + 20 - wheelPos[1];

		if (cp->id == 0)
			lsteeringTorque = 0;

		if (newCompression < 0)
			newCompression = 0;

		if (cp->id == 0)
		{
			if (newCompression > 120)
				SetForceDirection(-5000, -2500);
			else if (newCompression > 48)
				SetForceDirection(-2500, -1000);
		}

		if (newCompression > 60)
			newCompression = 60;

		if (!newCompression && !oldCompression)
		{
			wheel->susCompression = 0;
			continue;
		}

		susForce = 330 * newCompression - 200 * oldCompression;

		wheelPos[0] -= cl->where[0];
		wheelPos[1] -= cl->where[1];
		wheelPos[2] -= cl->where[2];

		pointVel[0] = cl->vel[0] + ((wheelPos[2] * cl->avel[1] - wheelPos[1] * cl->avel[2] + 2048) >> 12);
		pointVel[1] = cl->vel[1] + ((wheelPos[0] * cl->avel[2] - wheelPos[2] * cl->avel[0] + 2048) >> 12);
		pointVel[2] = cl->vel[2] + ((wheelPos[1] * cl->avel[0] - wheelPos[0] * cl->avel[1] + 2048) >> 12);

		forceZ = 0;
		forceX = 0;

		// the direction the wheel rolls in
		if (wheel->locked)
		{
			dir = Arc_Tan(pointVel[0] / 64, pointVel[2] / 64);
			lfz = icos(dir);
			lfx = isin(dir);
		}
		else if (i & 1)
		{
			lfx = cdz;
			lfz = -cdx;
		}
		else
		{
			lfx = sdz;
			lfz = -sdx;
		}

		sidevel = lfz / 64 * (pointVel[2] / 64) + lfx / 64 * (pointVel[0] / 64);
		slidevel = abs(sidevel / 64 * (frictionScale / 64));

		if (sidevel > 72000)
		{
			sidevel = 18000;
		}
		else if (sidevel < -72000)
		{
			sidevel = -18000;
		}
		else
		{
			sidevel = (sidevel * frictionScale + 2048) >> 12;

			if (sidevel > 18000)
				sidevel = 18000;
			else if (sidevel < -18000)
				sidevel = -18000;
		}

		if (i & 1)
		{
			// rear wheels
			if (wheel->locked)
			{
				sidevel = (sidevel * frontFS + 8192) >> 14;
			}
			else
			{
				sidevel = (sidevel * rearFS + 2048) >> 12;

				doAutoBrake = cp->id == 0 || (cp->id == 65 && gDoTraining && gTrainingMode == 2);

				if ((cp->id == 0 || cp->id == 65)
					&& ((doAutoBrake && cp->analog_steer * sidevel < 0) || cp->wheel_angle * sidevel > 0))
				{
					cp->autoBrake = -1;
				}

				forceX = (cp->thrust * -lfz + 2048) >> 12;
				forceZ = (cp->thrust * lfx + 2048) >> 12;
			}

			if (slidevel > rearvel)
				rearvel = slidevel;
		}
		else
		{
			// front wheels
			sidevel = (sidevel * frontFS + 2048) >> 12;

			if (wheel->locked)
			{
				sidevel = (sidevel + (sidevel >> 1)) >> 1;

				steer = (sdz * ((lfx * -sidevel + 2048) >> 12) - sdx * ((lfz * -sidevel + 2048) >> 12) + 1024) >> 11;

				forceX = (sdz * steer + 2048) >> 12;
				forceZ = (sdx * -steer + 2048) >> 12;

				if (cp->id == 0)
					lsteeringTorque += steer;
			}
			else if (cp->fourWheelDrive == 1)
			{
				forceX = (cp->thrust * sdx + 2048) >> 12;
				forceZ = (cp->thrust * sdz + 2048) >> 12;

				if (cp->id == 0)
					lsteeringTorque += sidevel;
			}
			else if (cp->id == 0)
			{
				lsteeringTorque -= 2 * sidevel;
			}

			if (slidevel > frontvel)
				frontvel = slidevel;
		}

		forceY = (wheelSurfaceNormal[1] * susForce + dampY + 2048) >> 12;
		forceX = ((friction_coef + 500) * (((wheelSurfaceNormal[0] * susForce - lfx * sidevel + dampX + 2048) >> 12) + forceX)) >> 12;
		forceZ = ((friction_coef + 500) * (((wheelSurfaceNormal[2] * susForce - lfz * sidevel + dampZ + 2048) >> 12) + forceZ)) >> 12;

		// less grip the further the car leans over
		if (cp->where.m[1][1] < 2048)
		{
			angle_scale = 4096 - cp->where.m[1][1];

			if (angle_scale <= 4096)
				angle_scale = 4096 - ((angle_scale * angle_scale + 2048) >> 12);
			else
				angle_scale = 0;

			forceX = (angle_scale * forceX + 2048) >> 12;
			forceZ = (angle_scale * forceZ + 2048) >> 12;
		}

		if (cp->fourWheelDrive == 1)
		{
			if (gDifficulty == 2)
				wheelPos[1] = 7 * wheelPos[1] / 32;
			else
				wheelPos[1] = 8 * wheelPos[1] / 32;
		}
		else
		{
			wheelPos[1] = 5 * wheelPos[1] / 16;
		}

		if (!TriggerBomb || cp->id != 0)
		{
			cl->aacc[2] -= (forceX * wheelPos[1] + 2048) >> 12;
			cl->aacc[1] += (forceX * wheelPos[2] + 2048) >> 12;
			cl->acc[0] += forceX;

			cl->aacc[0] -= (forceY * wheelPos[2] + 2048) >> 12;
			cl->aacc[2] += (forceY * wheelPos[0] + 2048) >> 12;
			cl->acc[1] += forceY;

			cl->aacc[1] -= (forceZ * wheelPos[0] + 2048) >> 12;
			cl->aacc[0] += (forceZ * wheelPos[1] + 2048) >> 12;
			cl->acc[2] += forceZ;
		}
		else if (i & 1)
		{
			// the bomb has gone off under the player: the right side goes up
			cl->aacc[0] -= (0xB000 * wheelPos[2] + 2048) >> 12;
			cl->aacc[2] += (0xB000 * wheelPos[0] + 2048) >> 12;
			cl->acc[1] += 0xB000;
		}
		else
		{
			// ... and the left side more
			cl->aacc[0] -= (0x23000 * wheelPos[2] + 2048) >> 12;
			cl->aacc[2] += (0x23000 * wheelPos[0] + 2048) >> 12;
			cl->acc[1] += 0x23000;
		}

		wheel->susCompression = newCompression;

		cp->wheelsOnGround++;
	}

	if (cp->wheel[1].susCompression || cp->wheel[3].susCompression)
	{
		cp->wheel_speed = cdz / 64 * (cl->vel[2] / 64) + cdx / 64 * (cl->vel[0] / 64);
	}
	else
	{
		// rear wheels in the air: they spin freely
		if (cp->thrust > 0)
			airSpeed = 0x1A4000;
		else if (cp->thrust < 0)
			airSpeed = -0x12C000;
		else
			airSpeed = 0;

		cp->wheel_speed = airSpeed;
	}

	cp->front_vel = frontvel;
	cp->rear_vel = rearvel;
	// **** / AddWheelForces ****

	ConvertTorqueToAngularAcceleration(cp, cl);

	cp->mayBeColliding = 0;

	if (cp->id == 0)
		steeringTorque = (lsteeringTorque + 3 * steeringTorque) / 4;
}

// [0x4e2f3c] [D]
void ConvertTorqueToAngularAcceleration(CAR_DATA *cp, CAR_LOCALS *cl)
{
	int i;
	LONG zd;

	zd = 440 * ((cl->aacc[2] * cp->where.m[2][2] + cl->aacc[1] * cp->where.m[1][2] + cl->aacc[0] * cp->where.m[0][2] + 2048) >> 12);

	for (i = 2; i >= 0; i--)
	{
		cl->aacc[i] = ((zd * cp->where.m[i][2] - (cl->avel[i] << 7) + 2048) >> 12) + 110 * cl->aacc[i];

		if (cl->extraangulardamping == 1)
			cl->aacc[i] -= cl->avel[i] / 8;

		cp->aacc[i] = cl->aacc[i];
		cp->acc[i] = cl->acc[i];
	}
}

// [0x4e304b] [D]
// Debris thrown up for a while after the player's car leaves the ground.
void jump_debris(CAR_DATA *cp)
{
	IVECTOR velocity;
	IVECTOR position;
	int i;

	for (i = 0; i < 4; i++)
	{
		if (cp->wheel[i].susCompression)
		{
			DebrisTimer = 0;
			WasOnGround = 1;
			return;
		}
	}

	if (WasOnGround)
	{
		WasOnGround = 0;
		DebrisTimer = 50;
		nose_down(cp);
	}

	if (DebrisTimer && --DebrisTimer <= 44)
	{
		if (DebrisTimer > (rand() & 0x3F))
		{
			memset(&velocity, 0, sizeof(velocity));

			position.vx = cp->location.vx + (rand() & 0x1FF) - 256;
			position.vy = cp->location.vy - 200;
			position.vz = cp->location.vz + (rand() & 0x1FF) - 256;

			Setup_Debris(&position, &velocity, 3, 5, (unsigned char)cp->model);
		}
	}
}

// [0x4e3194] [D]
// Tips the car's nose down as it leaves the ground.
void nose_down(CAR_DATA *cp)
{
	// (the original also works out the angular velocity about the car's x axis, unused)
	cp->st.n.angularVelocity[0] += 50 * cp->where.m[0][0];
	cp->st.n.angularVelocity[1] += 50 * cp->where.m[1][0];
	cp->st.n.angularVelocity[2] += 50 * cp->where.m[2][0];
}

// [0x4e324c] [D]
// Fails the mission when the player's car has been off its wheels too long.
void FailIfPlayerUpsideDown(void)
{
	int limit;

	if (mission.objective == 4080)
		return;

	if (car_data[0].wheelsOnGround >= 3)
		PlayerWipedOutTimer = 0;
	else if (!GamePaused && !FlyBy)
		PlayerWipedOutTimer++;

	if (gCheatFlags.RailroadCops || gCheatFlags.SuperFastCars)
		limit = 600;
	else
		limit = 300;

	if (PlayerWipedOutTimer > limit)
	{
		mission.objective = 4080;
		mission.failed_reason = 2;
		mission.pause_timer = 90;
	}
}
