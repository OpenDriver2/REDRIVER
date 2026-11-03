/*
 * bcollide.c - car to building collisions and car damage
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "bcollide.c", 0x49acd0 - 0x49cd4a).
 *
 * A building box (BUILDING_BOX, from objcoll.c) and the car are tested as two rectangles on the
 * ground (CDATA2D, separating axes). On a hit the car is moved back to the time of the hit
 * (bFindCollisionTime, binary search over the frame's movement), pushed out along the normal of
 * the face it hit and given an impulse; smashable objects are knocked over instead.
 * Damage goes to one of six zones of the car (0 / 1 front, 2 / 3 right side ..., by the hit
 * position against the axes of the car).
 */

#include <stdlib.h>
#include <string.h>

#include "dr_types.h"
#include "bcollide.h"
#include "dr_globals.h"
#include "debris.h"
#include "felony.h"
#include "handling.h"
#include "input.h"
#include "map.h"
#include "MScript.h"
#include "objanim.h"
#include "shell.h"
#include "sound.h"
#include "testmath.h"
#include "carnage.h"
#include "smashable.h"

// a body for the 2d collision test: centre, axes, velocity in the frame, angle, half sizes;
// dist and limit are the distances of the other body along the axes and the limits for a hit
typedef struct CDATA2D
{
	IVECTOR x;
	IVECTOR axis[2];
	IVECTOR vel;
	int theta;
	int length[2];
	int dist[2];
	int limit[2];
	int avel;
} CDATA2D;

typedef struct CRET2D
{
	IVECTOR hit;
	IVECTOR surfNormal;
	int penetration;
	int neverfree;
} CRET2D;

void DamageCar(CAR_DATA *cp, CDATA2D *cd, CRET2D *collisionResult, int strikeVel);

//------------------------------------------------------------------------------------------
// initialised data (0x5536c0)

// the height of the sparks of a lamp hit at night, per level
static int viclamp_spark_pos[9] = { 735, 580, 0, 1190, 0, 0, 0, 0, 0 };

//------------------------------------------------------------------------------------------

// [0x49acd0] [D]
int bcollided2d(CDATA2D *body)
{
	int ac, as;
	int dtheta;
	IVECTOR delta;
	int i, j, k;

	dtheta = body[1].theta - body[0].theta;

	ac = isin((dtheta + 1024) & 0x7FF);
	as = isin(dtheta & 0x7FF);

	i = 2;
	while (i--)
	{
		body[i].axis[0].vx = isin(body[i].theta);
		body[i].axis[0].vz = icos(body[i].theta);
		body[i].axis[1].vx = body[i].axis[0].vz;
		body[i].axis[1].vz = -body[i].axis[0].vx;
	}

	delta.vx = body[0].x.vx - body[1].x.vx;
	delta.vz = body[0].x.vz - body[1].x.vz;

	i = 2;
	for (;;)
	{
		j = i & 1;

		if (!i--)
			break;

		k = 2;
		while (k--)
		{
			body[i].dist[k] = (delta.vx * body[i].axis[k].vx + delta.vz * body[i].axis[k].vz + 2048) >> 12;
			body[i].limit[k] = ((ac * body[j].length[k] + as * body[j].length[1 - k] + 2048) >> 12) + body[i].length[k];

			if (body[i].dist[k] > body[i].limit[k] || body[i].dist[k] < -body[i].limit[k])
				return 0;
		}
	}

	return 1;
}

// [0x49af3d] [D]
void bFindCollisionPoint(CDATA2D *body, CRET2D *collisionResult)
{
	int smallest;
	int besti, bestk;
	int sign, sign0, sign1;
	int i, j, k;

	// NOTE: besti, bestk and sign are not initialised in the binary
	besti = 0;
	bestk = 0;
	sign = 0;

	smallest = body[0].limit[0] + 1;

	i = 2;
	while (i--)
	{
		k = 2;
		while (k--)
		{
			if (body[i].limit[k] - body[i].dist[k] < smallest)
			{
				smallest = body[i].limit[k] - body[i].dist[k];
				besti = i;
				bestk = k;
				sign = (i == 1) ? 1 : -1;
			}

			if (body[i].limit[k] + body[i].dist[k] < smallest)
			{
				smallest = body[i].limit[k] + body[i].dist[k];
				besti = i;
				bestk = k;
				sign = (i != 1) ? 1 : -1;
			}
		}
	}

	j = besti ^ 1;

	if ((body[besti].axis[bestk].vx * body[j].axis[0].vx + body[besti].axis[bestk].vz * body[j].axis[0].vz + 2048) >> 12 >= 0)
		sign0 = -sign;
	else
		sign0 = sign;

	if ((body[besti].axis[bestk].vx * body[j].axis[1].vx + body[besti].axis[bestk].vz * body[j].axis[1].vz + 2048) >> 12 >= 0)
		sign1 = -sign;
	else
		sign1 = sign;

	collisionResult->hit.vx = ((sign0 * body[j].length[0] * body[j].axis[0].vx
		+ sign1 * body[j].length[1] * body[j].axis[1].vx
		+ 2048) >> 12)
		+ body[j].x.vx;

	collisionResult->hit.vz = ((sign0 * body[j].length[0] * body[j].axis[0].vz
		+ sign1 * body[j].length[1] * body[j].axis[1].vz
		+ 2048) >> 12)
		+ body[j].x.vz;

	collisionResult->penetration = smallest;

	if (besti != 0)
		sign = -sign;

	collisionResult->surfNormal.vx = sign * body[besti].axis[bestk].vx;
	collisionResult->surfNormal.vy = 0;
	collisionResult->surfNormal.vz = sign * body[besti].axis[bestk].vz;
}

// [0x49b240] [D]
int bFindCollisionTime(CDATA2D *cd, CRET2D *collisionResult)
{
	CDATA2D original[2];
	int hit;
	int neverfree;
	int time, step;
	int i, q;

	hit = 1;
	neverfree = 1;
	time = 4096;
	step = 2048;

	i = 2;
	while (i--)
	{
		original[i].x.vx = cd[i].x.vx;
		original[i].x.vz = cd[i].x.vz;
		original[i].theta = cd[i].theta;

		q = 2;
		while (q--)
		{
			original[i].limit[q] = cd[i].limit[q];
			original[i].dist[q] = cd[i].dist[q];
		}
	}

	// binary search for the time of the hit: halve the frame's movement each step, back while
	// they overlap, forward while they don't
	q = 8;
	while (q--)
	{
		i = 2;
		while (i--)
		{
			cd[i].vel.vx >>= 1;
			cd[i].vel.vz >>= 1;
			cd[i].avel >>= 1;

			if (hit)
			{
				cd[i].x.vx -= cd[i].vel.vx;
				cd[i].x.vz -= cd[i].vel.vz;
				cd[i].theta -= cd[i].avel;
			}
			else
			{
				cd[i].x.vx += cd[i].vel.vx;
				cd[i].x.vz += cd[i].vel.vz;
				cd[i].theta += cd[i].avel;
			}
		}

		if (hit)
		{
			time -= step;
		}
		else
		{
			time += step;
			neverfree = 0;
		}

		hit = bcollided2d(cd);

		if (q)
			step >>= 1;
	}

	if (hit)
	{
		if (neverfree)
		{
			i = 2;
			while (i--)
			{
				cd[i].x.vx = original[i].x.vx;
				cd[i].x.vz = original[i].x.vz;
				cd[i].theta = original[i].theta;

				hit = bcollided2d(cd);
			}

			time = 4096;
		}
	}
	else
	{
		i = 2;
		while (i--)
		{
			cd[i].x.vx += cd[i].vel.vx;
			cd[i].x.vz += cd[i].vel.vz;
			cd[i].theta += cd[i].avel;
		}

		time += step;
		hit = bcollided2d(cd);
	}

	collisionResult->neverfree = neverfree;

	return time;
}

// [0x49b734] [D]
int DamageCar3D(CAR_DATA *cp, LONGVECTOR4 *delta, int strikeVel)
{
	int nose, door;
	short lbody;
	int region;
	int half;

	if ((cp->id == 0 || gDoTraining) && gCheatFlags.Invincibility)
		return 0;

	if (gCheatFlags.RailroadCops && cp->model == 1)
		return 0;

	if (cp->id == 0 || cp->id >= 65)
		strikeVel = strikeVel * player_damage_factor / 100;

	if (strikeVel < 0x1000)
		return 0;

	nose = ((*delta)[0] * cp->where.m[0][2] + (*delta)[1] * cp->where.m[1][2] + (*delta)[2] * cp->where.m[2][2] + 2048) >> 12;
	door = ((*delta)[0] * cp->where.m[0][0] + (*delta)[1] * cp->where.m[1][0] + (*delta)[2] * cp->where.m[2][0] + 2048) >> 12;

	lbody = car_cosmetics[cp->model].colBox.vz >> 1;

	if (door <= 0)
	{
		if (nose > lbody)
			region = 0;
		else
			region = (nose > -lbody) + 4;
	}
	else
	{
		if (nose > lbody)
			region = 1;
		else
			region = (nose <= -lbody) + 2;
	}

	if (cp->id == 0 || cp->id >= 65)
	{
		strikeVel >>= 8;

		if (strikeVel > 0x400)
			strikeVel = 0x400;

		strikeVel = (strikeVel * 1024 + strikeVel * strikeVel) >> 10;
	}
	else
	{
		// NOTE: compares GameMode with mission numbers (CarBuildingCollision tests
		// giCurrentMissionNumber with these), as in the binary
		switch (GameMode)
		{
			case 346:
			case 347:
			case 446:
			case 447:
				strikeVel = (strikeVel / 400 + 1024) * 7 / 8;
				break;
			default:
				strikeVel >>= 8;

				if (strikeVel > 0x400)
					strikeVel = 0x400;

				strikeVel = (strikeVel * 1024 + strikeVel * strikeVel) >> 10;
				strikeVel += 16;
				break;
		}
	}

	if (cp->id != 0 && (carnage_flags & 1) && !civilian_ai_data[cp->id - 1].IsCopCar)
		new_damage -= cp->damage[region];

	// apply damage
	cp->damage[region] = 0xFFF - (((0xFFF - strikeVel) * (0xFFF - cp->damage[region]) + 2048) >> 12);
	cp->totalDamage += strikeVel;

	// adjust symmetric regions
	half = cp->damage[region] / 2;

	switch (region)
	{
		case 0:
			if (cp->damage[1] < half)
				cp->damage[1] = half;
			break;
		case 1:
			if (cp->damage[0] < half)
				cp->damage[0] = half;
			break;
		case 3:
			if (cp->damage[4] < half)
				cp->damage[4] = half;
			break;
		case 4:
			if (cp->damage[3] < half)
				cp->damage[3] = half;
			break;
	}

	if (cp->id != 0 && (carnage_flags & 1) && !civilian_ai_data[cp->id - 1].IsCopCar)
	{
		new_damage += cp->damage[region];
		ReportCarnage(0xFF00);
	}

	return 1;
}

// [0x49bbb1] [D]
int sub_49BBB1(void)
{
	return 0;
}

// [0x49bbb8] [D]
void DamageCar(CAR_DATA *cp, CDATA2D *cd, CRET2D *collisionResult, int strikeVel)
{
	int dx, dz;
	int nose, door;
	short lbody;
	int region;
	int half;

	if (strikeVel < 0xA000)
		return;

	dx = collisionResult->hit.vx - cd->x.vx;
	dz = collisionResult->hit.vz - cd->x.vz;

	nose = (cd->axis[0].vx * dx + cd->axis[0].vz * dz + 2048) >> 12;
	door = (cd->axis[1].vx * dx + cd->axis[1].vz * dz + 2048) >> 12;

	lbody = car_cosmetics[cp->model].colBox.vz >> 1;

	if (door <= 0)
	{
		if (nose > lbody)
			region = 0;
		else
			region = (nose > -lbody) + 4;
	}
	else
	{
		if (nose > lbody)
			region = 1;
		else
			region = (nose <= -lbody) + 2;
	}

	strikeVel /= 300;

	if (strikeVel > 0x800)
		strikeVel = 0x800;

	if (cp->id != 0 && (carnage_flags & 1) && !civilian_ai_data[cp->id - 1].IsCopCar)
		new_damage -= cp->damage[region];

	cp->damage[region] = 0xFFF - (((0xFFF - strikeVel) * (0xFFF - cp->damage[region]) + 2048) >> 12);
	cp->totalDamage += strikeVel / 2;

	half = cp->damage[region] / 2;

	switch (region)
	{
		case 0:
			if (cp->damage[1] < half)
				cp->damage[1] = half;
			break;
		case 1:
			if (cp->damage[0] < half)
				cp->damage[0] = half;
			break;
		case 3:
			if (cp->damage[4] < half)
				cp->damage[4] = half;
			break;
		case 4:
			if (cp->damage[3] < half)
				cp->damage[3] = half;
			break;
	}

	if (cp->id != 0 && (carnage_flags & 1) && !civilian_ai_data[cp->id - 1].IsCopCar)
	{
		new_damage += cp->damage[region];
		ReportCarnage(0xFF00);
	}
}

// [0x49be6d] [D]
int CarBuildingCollision(CAR_DATA *cp, BUILDING_BOX *building, CELL_OBJECT *cop)
{
	CDATA2D cd[2];
	CRET2D collisionResult;
	int destruction;
	int oldx, oldz;
	short width, length;
	int lever[3], reaction[3], pointVel[3];
	int dot, denom;
	int strikeVel;
	int CollisionTime;
	IVECTOR velocity;
	IVECTOR lamp_position, lamp_velocity;
	VECTOR4 LeafPosition;
	SMASHABLE_OBJECT *sip, *match;
	int chan;

	destruction = 0;

	if (model_pointers[cop->type]->flags2 & MODEL_FLAG_SMASHABLE)
	{
		destruction = 1;

		if (mission.ghost_car_type == 1 && cp->id == 65)
			return 0;
	}

	if (!ModelActive[cop->type])
		return 0;

	if (abs(cp->where.t[1] - building->pos.vy) > building->height + 125)
		return 0;

	oldx = cp->where.t[0];
	oldz = cp->where.t[2];

	width = car_cosmetics[cp->model].colBox.vx;
	length = car_cosmetics[cp->model].colBox.vz;

	// the car where it will be at the end of the frame
	cd[0].vel.vx = (cp->st.n.linearVelocity[0] + 2048) >> 12;
	cd[0].vel.vz = (cp->st.n.linearVelocity[2] + 2048) >> 12;

	cp->where.t[0] += cd[0].vel.vx;
	cp->where.t[2] += cd[0].vel.vz;

	cd[0].x.vx = cp->where.t[0];
	cd[0].x.vz = cp->where.t[2];

	cd[0].theta = cp->direction;
	cd[0].length[0] = length;

	if (cp->fourWheelDrive == 1)
		cd[0].length[1] = 3 * width / 4;
	else
		cd[0].length[1] = 11 * width / 12;

	cd[0].avel = (40 * ((cp->st.n.angularVelocity[1] + 2048) >> 12)) >> 8;

	cd[1].x.vx = building->pos.vx;
	cd[1].x.vz = building->pos.vz;

	cd[1].theta = building->theta;
	cd[1].length[0] = building->xsize;
	cd[1].length[1] = building->zsize;

	cd[1].vel.vx = 0;
	cd[1].vel.vz = 0;

	cd[1].avel = 0;

	if (!bcollided2d(cd))
	{
		cp->where.t[0] -= (cp->st.n.linearVelocity[0] + 2048) >> 12;
		cp->where.t[2] -= (cp->st.n.linearVelocity[2] + 2048) >> 12;
		return 0;
	}

	CollisionTime = bFindCollisionTime(cd, &collisionResult);
	bFindCollisionPoint(cd, &collisionResult);

	collisionResult.surfNormal.vy = 0;

	collisionResult.hit.vy = cp->where.t[1] + 60;

	collisionResult.surfNormal.vx = -collisionResult.surfNormal.vx;
	collisionResult.surfNormal.vy = 0;
	collisionResult.surfNormal.vz = -collisionResult.surfNormal.vz;

	// out of the building
	cp->where.t[0] += (collisionResult.surfNormal.vx * collisionResult.penetration + 2048) >> 12;
	cp->where.t[2] += (collisionResult.surfNormal.vz * collisionResult.penetration + 2048) >> 12;

	lever[0] = collisionResult.hit.vx - cp->where.t[0];
	lever[1] = collisionResult.hit.vy - cp->where.t[1];
	lever[2] = collisionResult.hit.vz - cp->where.t[2];

	reaction[0] = (lever[2] * cp->st.n.angularVelocity[1] - lever[1] * cp->st.n.angularVelocity[2] + 2048) >> 12;
	reaction[1] = (lever[0] * cp->st.n.angularVelocity[2] - lever[2] * cp->st.n.angularVelocity[0] + 2048) >> 12;
	reaction[2] = (lever[1] * cp->st.n.angularVelocity[0] - lever[0] * cp->st.n.angularVelocity[1] + 2048) >> 12;

	reaction[0] += cp->st.n.linearVelocity[0];
	reaction[1] += cp->st.n.linearVelocity[1];
	reaction[2] += cp->st.n.linearVelocity[2];

	strikeVel = -(collisionResult.surfNormal.vz / 16 * (reaction[2] / 256)
		+ collisionResult.surfNormal.vy / 16 * (reaction[1] / 256)
		+ collisionResult.surfNormal.vx / 16 * (reaction[0] / 256));

	if (strikeVel > 0)
	{
		if (cp->id == 0)
		{
			if (giCurrentMissionNumber == 446
				|| giCurrentMissionNumber == 447
				|| giCurrentMissionNumber == 346
				|| giCurrentMissionNumber == 347
				|| !destruction)
			{
				// Collision with building
				NoteFelony(&gFelonyData, 6, 4096);
			}
			else
			{
				// Damaging property
				NoteFelony(&gFelonyData, 7, 4096);
			}
		}

		collisionResult.hit.vy = -collisionResult.hit.vy;

		velocity.vx = cp->st.n.linearVelocity[0] / 4096;
		velocity.vy = -17;
		velocity.vz = cp->st.n.linearVelocity[2] / 4096;

		if (model_pointers[cop->type]->flags2 & MODEL_FLAG_SMASHABLE)
		{
			if (mission.ghost_car_type && cp->id == 65)
				return 0;

			damage_object(cop, &velocity);

			if (model_pointers[cop->type]->shape_flags & SHAPE_FLAG_TRANS)
			{
				Play3DSound(3, 3, (float)collisionResult.hit.vx, (float)collisionResult.hit.vy, (float)collisionResult.hit.vz);
			}
			else
			{
				match = NULL;

				for (sip = smashable; sip->name; sip++)
				{
					if (sip->model == model_pointers[cop->type])
					{
						match = sip;
						break;
					}
				}

				if (!match)
					match = smashable;

				chan = Play3DSound(match->sound, -1, (float)collisionResult.hit.vx, (float)collisionResult.hit.vy, (float)collisionResult.hit.vz);

				if (chan != -1)
				{
					SetSoundFreq(chan, match->pitch);
					SetSoundVol(chan, match->volume);
				}
			}

			// the car goes on through it
			cp->where.t[0] = oldx;
			cp->where.t[2] = oldz;

			collisionResult.hit.vy += 30;
			Setup_Sparks(&collisionResult.hit, &velocity, 10, 0);

			collisionResult.hit.vy = -30;
			Setup_Debris(&collisionResult.hit, &velocity, 10, 0, -1);

			if (cp->id == 0)
				SetForceDirection(-10000, -5000);

			return 0;
		}

		if (strikeVel > 0xA000)
		{
			if (model_pointers[(unsigned short)gLastModelCollisionCheck]->flags2 & MODEL_FLAG_TREE)
			{
				LeafPosition.vx = (float)collisionResult.hit.vx;
				LeafPosition.vy = -(float)collisionResult.hit.vy - 250.0f - (float)(rand() & 0xFE) - 128.0f;
				LeafPosition.vz = (float)collisionResult.hit.vz;

				AddLeaf(&LeafPosition, 3, 1);

				if ((rand() & 0xF) == 1)
				{
					LeafPosition.vy = LeafPosition.vy + 100.0f;
					CreateBirds(&LeafPosition, rand() & 7);
				}

				if (cp->id == 0)
					SetForceDirection(-10000, -5000);

				Play3DSound(4, 3, (float)collisionResult.hit.vx, (float)collisionResult.hit.vy, (float)collisionResult.hit.vz);
			}
			else
			{
				if (gNight
					&& (model_pointers[(unsigned short)gLastModelCollisionCheck]->flags2 & MODEL_FLAG_LAMP)
					&& damage_lamp(cop))
				{
					memset(&lamp_velocity, 0, sizeof(lamp_velocity));

					lamp_position.vx = (int)cop->pos.vx;
					lamp_position.vy = (int)cop->pos.vy - viclamp_spark_pos[GameLevel];
					lamp_position.vz = (int)cop->pos.vz;

					Setup_Sparks(&lamp_position, &lamp_velocity, 20, 1);
				}

				velocity.vy -= 20;
				collisionResult.hit.vy += 30;

				Setup_Sparks(&collisionResult.hit, &velocity, 4, 0);

				collisionResult.hit.vy -= 30;
				velocity.vy += 20;
			}

			if (strikeVel > 0x28000)
			{
				Setup_Debris(&collisionResult.hit, &velocity, 6, 0, cp->model);

				if (cp->id == 0)
					SetForceDirection(-10000, -5000);

				if (cp->id == 0 || cp->id >= 65)
					Play3DSound(3, 3, (float)collisionResult.hit.vx, (float)collisionResult.hit.vy, (float)collisionResult.hit.vz);
			}
			else
			{
				SetForceDirection(-3000, -1000);
			}
		}

		if ((cp->id != 0 && !gDoTraining || !gCheatFlags.Invincibility)
			&& (!gCheatFlags.RailroadCops || cp->model != 1))
		{
			DamageCar(cp, cd, &collisionResult, strikeVel);
		}

		// the impulse along the normal
		dot = (collisionResult.surfNormal.vy * lever[1]
			+ collisionResult.surfNormal.vx * lever[0]
			+ collisionResult.surfNormal.vz * lever[2]
			+ 2048) >> 12;

		denom = ((110 * (lever[2] * lever[2] + lever[0] * lever[0] - dot * dot) + 2048) >> 12) + 4096;

		if (strikeVel > 0x7F000)
			strikeVel = (strikeVel / denom) << 12;
		else
			strikeVel = (strikeVel << 12) / denom;

		strikeVel = 9 * strikeVel / 16;

		// NOTE: the binary first sets pointVel to -0.1875 of the reaction clamped to +-7200,
		// then overwrites it; that dead computation is left out
		pointVel[0] = collisionResult.surfNormal.vx / 64 * (strikeVel / 64);
		pointVel[1] = collisionResult.surfNormal.vy / 64 * (strikeVel / 64);
		pointVel[2] = collisionResult.surfNormal.vz / 64 * (strikeVel / 64);

		cp->where.t[0] -= (((cp->st.n.linearVelocity[0] + 2048) >> 12) * (4096 - CollisionTime) + 2048) >> 12;
		cp->where.t[2] -= (((cp->st.n.linearVelocity[2] + 2048) >> 12) * (4096 - CollisionTime) + 2048) >> 12;

		cp->aacc[0] += (pointVel[2] * lever[1] + 2048) >> 12;
		cp->aacc[0] -= (pointVel[1] * lever[2] + 2048) >> 12;

		cp->aacc[1] += (pointVel[0] * lever[2] + 2048) >> 12;
		cp->aacc[1] -= (pointVel[2] * lever[0] + 2048) >> 12;

		cp->aacc[2] += (pointVel[1] * lever[0] + 2048) >> 12;
		cp->aacc[2] -= (pointVel[0] * lever[1] + 2048) >> 12;

		cp->st.n.linearVelocity[0] += pointVel[0];
		cp->st.n.linearVelocity[1] += pointVel[1];
		cp->st.n.linearVelocity[2] += pointVel[2];

		cp->where.t[0] += (((cp->st.n.linearVelocity[0] + 2048) >> 12) * (4096 - CollisionTime) + 2048) >> 12;
		cp->where.t[2] += (((cp->st.n.linearVelocity[2] + 2048) >> 12) * (4096 - CollisionTime) + 2048) >> 12;
	}

	// back to the start of the frame: the time step moves the car
	cp->where.t[0] -= (cp->st.n.linearVelocity[0] + 2048) >> 12;
	cp->where.t[2] -= (cp->st.n.linearVelocity[2] + 2048) >> 12;

	return 1;
}
