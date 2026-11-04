/*
 * pedest.c - pedestrians: pinging them in on the pavement around the player, walking from
 * pavement cell to pavement cell, sitters on chairs, diving out of the way of cars
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "pedest.c=pedestrian.c", 0x514040 - 0x517f9b).
 *
 * The road map is a grid of 1500 unit cells; a cell where ROADS_GetRoadOrJunction gives -32768
 * (no road) is pavement. A walking pedestrian heads for the next pavement cell (last_dir: 1 west,
 * 2 east, 4 north, 8 south) and turns at the cell centres (NewTarget). Up to max_pedestrians
 * are pinged in at the traffic ping points (ai.c), a quarter of them sitting on chairs (chairs.c);
 * pedestrians 40500 units from the player are removed. The animation is motion capture data
 * (motion_c.c, MC_Data per motion type 1 walk, 2 run, 3 dive, 4 sit), its root motion moving the
 * pedestrian.
 *
 * flags_1: 0x8 stepped onto the road (heading back to field_a4), 0x10 active, 0x80 blending into
 * motion_type_1, 0x100 on the road side of the pavement, 0x400 / 0x800 jumping (field_74 is the
 * velocity, flags_2 the jump phase), 0x1000 / 0x2000 turning left / right (field_14 the target
 * angle, field_18 the angle left).
 *
 * The pedestrian bss sits in the linker's common area (between replays.c, pres.c and pathfind.c
 * data), so its owner is the main user; drawing_cut_scene and pedestrian_data_ok are left to
 * their other users. The three zero dwords after PedestrianActionHandlers (0x577d10) are left out:
 * the chunk boundary is unclear.
 */

#include <stdlib.h>
#include <string.h>

#include "dr_types.h"
#include "pedest.h"
#include "dr_globals.h"
#include "ai.h"
#include "asmmath.h"
#include "chairs.h"
#include "debris.h"
#include "felony.h"
#include "general.h"
#include "handling.h"
#include "map.h"
#include "pause.h"
#include "roadbits.h"
#include "shell.h"
#include "testmath.h"
#include "Engine/3dfx/gfxinit.h"
#include "Engine/3dfx/object.h"
#include "motion_c.h"

// the road map cells around a pedestrian (ROADS_GetRoadOrJunction, -32768: pavement)
typedef struct PEDESTRIAN_ROADS
{
	short pos;
	short north;
	short south;
	short east;
	short west;
} PEDESTRIAN_ROADS;

typedef void (*PEDESTRIAN_FUNC)(PEDESTRIAN *ped);

// bytes of one frame of motion capture data (MOTION_PACKET)
#define MOTION_FRAME_SIZE	(sizeof(QUATERNION) * num_objects + sizeof(VECTOR4))

void DestroyPedestrian(PEDESTRIAN *ped);
void HandleSitters(void);
int CreatePedestrian(void);
void DrawPedestrian(PEDESTRIAN *ped);
void StepPedestrianAnimation(PEDESTRIAN *ped);
void PingInPedestrians(void);
int PingInSittingPedestrian(PEDESTRIAN *ped, int num_peds);
int PingInRandomPedestrian(PEDESTRIAN *ped, int num_peds);
int UpdatePedestrianPings(int x, int z);
int sub_515AE5(int x, int z);
int CalcPedestrianDirection(int last_dir, int wx, int wz, VECTOR4 *target);
void sub_515EA7(PEDESTRIAN *ped);
void sub_515EC0(PEDESTRIAN *ped);
void sub_515ED9(PEDESTRIAN *ped);
void sub_515EEA(PEDESTRIAN *ped);
void PedestrianAction_WalkToTarget(PEDESTRIAN *ped);
void PedestrianAction_RunToTarget(PEDESTRIAN *ped);
void PedestrianAction_Sitting(PEDESTRIAN *ped);
void PedestrianAction_Dive(PEDESTRIAN *ped);
int GetPedDirection(short angle);
void SetPedestrianTurn(PEDESTRIAN *ped);
void PedestrianActionInit_WalkToTarget(PEDESTRIAN *ped);
void PedestrianActionInit_Sitting(PEDESTRIAN *ped);
void PedestrianActionInit_RunToTarget(PEDESTRIAN *ped);
void CorrectPathPosition(PEDESTRIAN *ped, VECTOR4 *pos);
void CorrectTargetPosition(PEDESTRIAN *ped, VECTOR4 *target);
void CalculatePedestrianInterest(PEDESTRIAN *ped);
void sub_516DA7(PEDESTRIAN *ped, int motion, int a3);
void sub_516E52(PEDESTRIAN *ped, short motion, int a3, int flags, int interp_frame);
int sub_516E7F(PEDESTRIAN *ped, int frames);
void sub_517079(PEDESTRIAN *ped);
void CorrectPedDirection(PEDESTRIAN *ped);
void NewTarget(PEDESTRIAN *ped);
void CheckForPlayerCar(PEDESTRIAN *ped);
void SetupCivJump(PEDESTRIAN *ped, CAR_DATA *cp);
void BuildCollisionFrustrum(CAR_DATA *cp, MATRIX *frustrum, int length);
int CheckForCar(PEDESTRIAN *ped, MATRIX *frustrum);
int sub_517EAA(PEDESTRIAN *ped, CAR_DATA *cp);

int max_pedestrians = 32;

// palette per pedestrian pallet (DisplayPedestrians)
int pedestrian_palette_table[5] = { 14, 26, 28, 29, 30 };

PEDESTRIAN_FUNC PedestrianInitHandlers[3] = {
	PedestrianActionInit_WalkToTarget,
	PedestrianActionInit_RunToTarget,
	PedestrianActionInit_Sitting
};

// per motion_type_2 - 1
PEDESTRIAN_FUNC PedestrianActionHandlers[4] = {
	PedestrianAction_WalkToTarget,
	PedestrianAction_RunToTarget,
	PedestrianAction_Dive,
	PedestrianAction_Sitting
};

int dword_BD9AEC;						// NOTE: never set: SetupCivJump never takes the jump branch
PEDESTRIAN_ROADS pedestrian_roads;
int frus_len;
int pedestrian_ping_extra;
int pedestrian_frame_counter;
int sitter_count;
float DIVE_SCALE;
MATRIX gPlayerFrustrum;
int max_sitters;
int pedestrian_count;
PEDESTRIAN pedestrians[64];
int pedestrian_ping_radius;
int pedestrian_list[64];				// free pedestrians slots from pedestrian_count on

// [0x514040] [D]
void InitPedestrians(void)
{
	int i;

	drawing_cut_scene = 0;
	pedestrian_count = 0;
	sitter_count = 0;

	ClearMem(pedestrian_list, sizeof(pedestrian_list));
	memset(pedestrians, 0, sizeof(pedestrians));

	for (i = 0; i < 64; i++)
	{
		if (i & 1)
			pedestrians[i].model_base = ped1base;
		else
			pedestrians[i].model_base = ped2base;

		pedestrian_list[i] = i;
		pedestrians[i].index = i;
	}

	pedestrian_ping_radius = 4500000;

	if (max_pedestrians != 0)
		pedestrian_ping_radius = (pedestrian_ping_radius << 6) / max_pedestrians;

	pedestrian_ping_extra = 0;
	pedestrian_frame_counter = 0;

	if (max_pedestrians <= 0)
		max_sitters = 0;
	else
		max_sitters = max_pedestrians / 4 + 1;
}

// [0x514198] [D]
// NOTE: no callers. Tests pedestrians[i] but destroys pedestrians[pedestrian_list[i]] (as in the
// original).
void DestroyPedestrians(void)
{
	int i;

	for (i = 0; i < 64; i++)
	{
		if (pedestrians[i].flags_1 & 0x10)
			DestroyPedestrian(&pedestrians[pedestrian_list[i]]);
	}
}

// [0x5141ef] [D]
void DestroyPedestrian(PEDESTRIAN *ped)
{
	if (pedestrian_count > 0 && (ped->flags_1 & 0x10))
	{
		ped->flags_1 = 0;
		ped->interp_frame = 0;
		ped->motion_type_2 = 0;
		ped->motion_type_1 = 0;
		ped->field_24 = 0;
		ped->field_20 = 0;
		ped->direction.vz = 0;
		ped->direction.vy = 0;
		ped->direction.vx = 0;

		pedestrian_list[--pedestrian_count] = ped->index;
	}
}

// [0x514287] [D]
void HandleSitters(void)
{
	PEDESTRIAN *ped;
	int i;

	sitter_count = 0;

	for (ped = pedestrians, i = 0; i < 64; i++, ped++)
	{
		if ((ped->flags_1 & 0x10) && ped->type == 3)
			sitter_count++;
	}
}

// [0x5142ec] [D]
int CreatePedestrian(void)
{
	int slot;
	PEDESTRIAN *ped;

	if (pedestrian_count >= 64)
		return -1;

	slot = pedestrian_list[pedestrian_count++];
	ped = &pedestrians[slot];

	ped->flags_1 = 0x10;
	ped->num_frames = 0;
	ped->field_54 = 0;

	return slot;
}

// [0x514354] [D]
void HandlePedestrians(void)
{
	PEDESTRIAN *ped;
	int i;
	int dir;
	short vy;
	float dx;
	float dz;

	if (pedestrian_data_ok == 0 || max_pedestrians == 0)
		return;

	pedestrian_frame_counter++;

	frus_len = 15 * car_data[0].speed;

	if (15 * car_data[0].speed < 1500)
		frus_len = 1500;

	BuildCollisionFrustrum(car_data, &gPlayerFrustrum, frus_len);

	if (gNoPedestrians)
		return;

	HandleSitters();
	PingInPedestrians();

	for (ped = pedestrians, i = 0; i < 64; i++, ped++)
	{
		if (ped->flags_1 & 0x10)
		{
			pedestrian_roads.pos = ROADS_GetRoadOrJunction(float_to_int(ped->position.vx), float_to_int(ped->position.vz));
			pedestrian_roads.north = ROADS_GetRoadOrJunction(float_to_int(ped->position.vx), float_to_int(ped->position.vz) - 1500);
			pedestrian_roads.south = ROADS_GetRoadOrJunction(float_to_int(ped->position.vx), float_to_int(ped->position.vz) + 1500);
			pedestrian_roads.east = ROADS_GetRoadOrJunction(float_to_int(ped->position.vx) + 1500, float_to_int(ped->position.vz));
			pedestrian_roads.west = ROADS_GetRoadOrJunction(float_to_int(ped->position.vx) - 1500, float_to_int(ped->position.vz));

			if (ped->flags_1 & 0x800)
			{
				sub_515EEA(ped);
			}
			else if (pedestrian_roads.pos == -32768)
			{
				// on the pavement: remember where, pick a new target when back from the road
				ped->field_a4 = ped->position;

				if (ped->flags_1 & 8)
				{
					ped->flags_1 &= ~8;

					vy = ped->direction.vy;
					NewTarget(ped);

					if (ped->flags_1 & 0x10)
					{
						ped->direction.vy = vy;
						SetPedestrianTurn(ped);
					}
				}
			}
			else if ((ped->flags_1 & 8) == 0 && ped->motion_type_2 != 3)
			{
				// stepped onto the road: head back to the pavement
				ped->flags_1 |= 8;
				ped->target = ped->field_a4;

				dx = ped->target.vx - ped->position.vx;
				dz = ped->target.vz - ped->position.vz;

				dir = GetPedDirection((-Arc_Tan(float_to_int(dz), float_to_int(dx)) - 1024) & 0xFFF);
				SetPedestrianTurn(ped);

				ped->last_dir = dir;
			}

			ped->interest = 0;

			if ((ped->flags_1 & 0x800) == 0 || ped->motion_type_2 != 3)
			{
				switch (ped->type)
				{
					case 1:
						sub_515EA7(ped);
						break;
					case 2:
						sub_515EC0(ped);
						break;
					case 3:
						sub_515ED9(ped);
						break;
				}
			}
		}

		if (ped->flags_1 & 0x10)
		{
			if (ped->flags_1 & 0x1000)
			{
				ped->direction.vy -= 128;

				if (ped->type == 2)
					ped->direction.vy -= 128;

				ped->direction.vy &= 0xFFF;

				ped->field_18 -= 128;

				if (ped->type == 2)
					ped->field_18 -= 128;

				if (ped->field_18 <= 0)
				{
					ped->flags_1 &= ~0x3000;
					ped->direction.vy = ped->field_14;
					SetPedestrianTurn(ped);
				}
			}
			else if (ped->flags_1 & 0x2000)
			{
				ped->direction.vy += 128;
				ped->direction.vy &= 0xFFF;

				ped->field_18 -= 128;

				if (ped->field_18 <= 0)
				{
					ped->flags_1 &= ~0x3000;
					ped->direction.vy = ped->field_14;
					SetPedestrianTurn(ped);
				}
			}

			StepPedestrianAnimation(ped);

			dx = ped->position.vx - (float)car_data[0].location.vx;
			dz = ped->position.vz - (float)car_data[0].location.vz;

			if (dx * dx + dz * dz <= 1640250000.0f)
			{
				if (car_data[0].speed != 0)
					CheckForPlayerCar(ped);
			}
			else
			{
				DestroyPedestrian(ped);
			}
		}
	}
}

// [0x514a97] [D]
// Draws the pedestrians one palette at a time, standing on the ground.
void DisplayPedestrians(void)
{
	PEDESTRIAN *ped;
	int i;
	int j;

	if (max_pedestrians == 0)
		return;

	gPedShadowsToDraw = 0;

	for (i = 0; i < 5; i++)
	{
		SwapPalette(pedestrian_palette_table[i]);

		for (ped = pedestrians, j = 0; j < 64; j++, ped++)
		{
			if ((ped->flags_1 & 0x10) && ped->pallet == i)
			{
				// NOTE: the original has the same code for motion_type_2 4 (sitting) and the rest
				ped->position.vy = (float)-MapHeight(&ped->position) - 224.0f;

				DrawPedestrian(ped);
			}
		}
	}
}

// [0x514bc1] [D] [A]
// NOTE: the root offset passed to WalkMotionObject keeps the frame's root vw while blending as
// well (unset then in the original).
void DrawPedestrian(PEDESTRIAN *ped)
{
	MOTION_PACKET *packet;
	MOTION_PACKET *next;
	SVECTOR rot;
	QUATERNION quat;
	VECTOR4 offset;
	float t;
	float root_y;
	int dx;
	int dz;
	int dist;

	packet = (MOTION_PACKET *)(MC_Data[ped->motion_type_2 - 1] + ped->frame * MOTION_FRAME_SIZE);

	if (ped->flags_1 & 0x80)
	{
		// blending into the first frame of motion_type_1
		next = (MOTION_PACKET *)MC_Data[ped->motion_type_1 - 1];

		if (ped->interp_frame >= 5)
			ped->interp_frame = 5;

		if (ped->interp_frame < 0)
			ped->interp_frame = 0;

		t = (float)(5 - ped->interp_frame) / 5.0f;
		root_y = (next->root.vy - packet->root.vy) * (1.0f - t) + packet->root.vy;
	}
	else
	{
		root_y = packet->root.vy;
	}

	ped_trans.vx = ped->position.vx;
	ped_trans.vy = root_y + ped->position.vy;
	ped_trans.vz = ped->position.vz;

	dx = abs(float_to_int(ped_trans.vx - camera_position.vx));
	dz = abs(float_to_int(ped_trans.vz - camera_position.vz));

	if (dx >= dz)
		dist = dz / 2 + dx;
	else
		dist = dx / 2 + dz;

	if (dist > 15000)
		return;

	do_z_clipping = FrustrumCheck(&ped_trans, 800.0f);

	if (do_z_clipping == 0 ||
		PositionVisible(&ped_trans, CurrentPVS, camera.current_cell_x, camera.current_cell_z) == 0)
	{
		return;
	}

	rot.vx = ped->direction.vx;
	rot.vy = ped->direction.vy;
	rot.vz = ped->direction.vz;

	// x = y = z = 0.0 ( why tho? )
	offset.vx = 0.0f;
	offset.vy = 0.0f;
	offset.vz = 0.0f;
	offset.vw = packet->root.vw;

	EulerToQuaternion(&rot, &quat);

	model_base = ped->model_base;

	if (ped->flags_1 & 0x80)
	{
		LerpObjects(packet, next, t);
		WalkMotionObject(ped, lerp_quaternions, hierarchy_tree, &quat, &offset);
	}
	else
	{
		WalkMotionObject(ped, packet->objects, hierarchy_tree, &quat, &offset);
	}

	gPedNumToShadow[gPedShadowsToDraw++] = ped->index;
}

// [0x514fc7] [D]
// Advances the animation by speed frames, moving the pedestrian by the root motion of each
// frame (scaled 0.4 walking, 0.7 running, 1 - 4 diving: more the closer the player).
void StepPedestrianAnimation(PEDESTRIAN *ped)
{
	MOTION_PACKET *packet;
	char *data;
	float sn;
	float cs;
	float scale;
	float dx;
	float dz;
	float dist;
	int num_frames;
	int frame;
	int last;
	int i;

	cs = fcos(ped->direction.vy);
	sn = fsin(ped->direction.vy);

	frame = ped->frame;
	num_frames = mcd_frames[ped->motion_type_2 - 1];
	data = MC_Data[ped->motion_type_2 - 1];

	if (ped->flags_1 & 0x80)
	{
		if (ped->interp_frame >= 5)
			ped->interp_frame = 5;

		if (ped->interp_frame < 0)
			ped->interp_frame = 0;
	}

	if (GamePaused == 0)
		ped->frame += ped->speed;

	last = ped->frame;

	if ((ped->flags_1 & 0x80) == 0)
	{
		ped_trans.vx = ped->position.vx;
		ped_trans.vy = ped->position.vy;
		ped_trans.vz = ped->position.vz;

		for (i = frame + 1; i <= last; i++)
		{
			if (i >= num_frames)
				packet = (MOTION_PACKET *)(data + i % num_frames * MOTION_FRAME_SIZE);
			else
				packet = (MOTION_PACKET *)(data + i * MOTION_FRAME_SIZE);

			if (ped->motion_type_2 == 3)
			{
				dx = ped->position.vx - (float)car_data[0].location.vx;
				dz = ped->position.vz - (float)car_data[0].location.vz;

				if (dx < 0.0f)
					dx = -dx;

				if (dz < 0.0f)
					dz = -dz;

				if (dx >= dz)
					dist = dz * 0.5f + dx;
				else
					dist = dx * 0.5f + dz;

				scale = dist / 275.0f;

				if (scale > 3.0f)
					scale = 3.0f;

				DIVE_SCALE = 4.0f - scale;

				ped_trans.vx = (cs * packet->root.vx + sn * packet->root.vz) * DIVE_SCALE + ped_trans.vx;
				ped_trans.vz = (cs * packet->root.vz - sn * packet->root.vx) * DIVE_SCALE + ped_trans.vz;
			}
			else if (ped->motion_type_2 == 2)
			{
				ped_trans.vx = (cs * packet->root.vx + sn * packet->root.vz) * 0.7f + ped_trans.vx;
				ped_trans.vz = (cs * packet->root.vz - sn * packet->root.vx) * 0.7f + ped_trans.vz;
			}
			else
			{
				ped_trans.vx = (cs * packet->root.vx + sn * packet->root.vz) * 0.4f + ped_trans.vx;
				ped_trans.vz = (cs * packet->root.vz - sn * packet->root.vx) * 0.4f + ped_trans.vz;
			}
		}

		ped->position.vx = ped_trans.vx;
		ped->position.vy = ped_trans.vy;
		ped->position.vz = ped_trans.vz;
	}

	if (ped->frame >= num_frames)
		ped->frame %= num_frames;

	if (ped->flags_1 & 0x80)
	{
		if (ped->interp_frame-- == 0)
			sub_517079(ped);
	}
}

// [0x51547b] [D]
// A pedestrian at each ping point (ai.c) while fewer than max_pedestrians: sitting on a chair,
// else walking when no other pedestrian is near the point.
void PingInPedestrians(void)
{
	PING_POINT *ping;
	PEDESTRIAN *ped;
	int ping_loop;
	int num_peds;
	int slot;
	int done;

	ping = path_ping_points;

	for (ping_loop = num_path_ping_points; ping_loop != 0; ping_loop--, ping++)
	{
		if (pedestrian_count >= max_pedestrians)
			break;

		num_peds = UpdatePedestrianPings(ping->x, ping->z);

		slot = CreatePedestrian();

		if (slot == -1)
			break;

		ped = &pedestrians[slot];
		ped->position.vx = (float)ping->x;
		ped->position.vz = (float)ping->z;
		ped->position.vy = (float)-MapHeight(&ped->position);

		pedestrian_roads.pos = ROADS_GetRoadOrJunction(float_to_int(ped->position.vx), float_to_int(ped->position.vz));
		pedestrian_roads.north = ROADS_GetRoadOrJunction(float_to_int(ped->position.vx), float_to_int(ped->position.vz) - 1500);
		pedestrian_roads.south = ROADS_GetRoadOrJunction(float_to_int(ped->position.vx), float_to_int(ped->position.vz) + 1500);
		pedestrian_roads.east = ROADS_GetRoadOrJunction(float_to_int(ped->position.vx) + 1500, float_to_int(ped->position.vz));
		pedestrian_roads.west = ROADS_GetRoadOrJunction(float_to_int(ped->position.vx) - 1500, float_to_int(ped->position.vz));

		done = PingInSittingPedestrian(ped, num_peds);

		if (done == 0)
		{
			done = PingInRandomPedestrian(ped, num_peds);

			if (done == 0)
				DestroyPedestrian(ped);
		}

		if (done)
			ped->pallet = abs(Random() % 5);
	}
}

// [0x51576d] [D]
// Seats the pedestrian on a random chair of its cell, facing away from it, unless another
// pedestrian is already there.
int PingInSittingPedestrian(PEDESTRIAN *ped, int num_peds)
{
	CHAIR *chair;
	int angle;

	if (sitter_count >= max_sitters || num_peds >= max_sitters / 4)
		return 0;

	chair = GetRandomChair((int)(ped->position.vx / (float)cell_size + (float)(cells_across / 2)) +
		cells_across * (int)(ped->position.vz / (float)cell_size + (float)(cells_down / 2)));

	if (chair == NULL)
		return 0;

	ped->type = 3;
	ped->motion_type_2 = 4;
	PedestrianActionInit_Sitting(ped);

	ped->position.vx = (float)chair->x;
	ped->position.vz = (float)chair->z;

	// copy angle from chair (compiler optimized)
	ped->direction = chair->ang;

	ped->direction.vx = (ped->direction.vx - 2048) & 0xFFF;

	angle = (ped->direction.vy - 2048) & 0xFFF;

	// 1000 aside so that sub_515AE5 does not find this pedestrian itself
	ped->position.vx = fsin(angle) * 60.0f - 1000.0f + ped->position.vx;
	ped->position.vz = fcos(angle) * 60.0f - 1000.0f + ped->position.vz;

	if (sub_515AE5(chair->x, chair->z) != -1)
		return 0;

	ped->position.vx = ped->position.vx + 1000.0f;
	ped->position.vz = ped->position.vz + 1000.0f;

	sitter_count++;

	// set the return variable, not return...
	return 1;
}

// [0x51596b] [D]
int PingInRandomPedestrian(PEDESTRIAN *ped, int num_peds)
{
	if (pedestrian_count >= max_pedestrians || num_peds != 0)
		return 0;

	Random();

	ped->type = 1;
	ped->motion_type_2 = 1;
	PedestrianActionInit_WalkToTarget(ped);

	return 1;
}

// [0x5159eb] [D]
// The number of pedestrians within the ping radius of x, z; with none, a new random extra
// radius.
int UpdatePedestrianPings(int x, int z)
{
	PEDESTRIAN *ped;
	int ped_loop;
	int num_peds;
	int dx;
	int dz;

	num_peds = 0;

	for (ped = pedestrians, ped_loop = 0; ped_loop < 64; ped_loop++, ped++)
	{
		if (ped->flags_1 & 0x10)
		{
			dx = float_to_int(ped->position.vx) - x;
			dz = float_to_int(ped->position.vz) - z;

			if (dz * dz + dx * dx < pedestrian_ping_extra + pedestrian_ping_radius)
				num_peds++;
		}
	}

	if (num_peds == 0)
		pedestrian_ping_extra = 1500 * (Random() & 3);

	return num_peds;
}

// [0x515ae5] [D]
// NOTE: no name in the database (called by PingInSittingPedestrian). The pedestrian within 100
// units (x + z distance) of x, z, -1 if none.
int sub_515AE5(int x, int z)
{
	PEDESTRIAN *ped;
	int i;

	ped = pedestrians;

	for (i = 0; i < 64; i++, ped++)
	{
		if (ped->flags_1 & 0x10)
		{
			if (abs(z - float_to_int(ped->position.vz)) + abs(x - float_to_int(ped->position.vx)) < 100)
				return i;
		}
	}

	return -1;
}

// [0x515bb7] [D]
// Picks a pavement direction from pedestrian_roads (last_dir if free, else a random free one)
// and sets target to the centre of the cell that way. Returns the direction, 0 if none.
int CalcPedestrianDirection(int last_dir, int wx, int wz, VECTOR4 *target)
{
	int dirs;
	int count;
	int bit;
	int pick;
	int cx;
	int cz;

	count = 0;
	dirs = 0;

	if (pedestrian_roads.west == -32768)
	{
		dirs = 1;
		count = 1;
	}

	if (pedestrian_roads.east == -32768)
	{
		dirs |= 2;
		count++;
	}

	if (pedestrian_roads.north == -32768)
	{
		dirs |= 4;
		count++;
	}

	if (pedestrian_roads.south == -32768)
	{
		dirs |= 8;
		count++;
	}

	if (last_dir & dirs)
	{
		dirs &= last_dir;
	}
	else if (count > 1)
	{
		bit = 16;
		pick = Random() % count + 1;

		while (pick != 0)
		{
			bit >>= 1;

			if (bit & dirs)
				pick--;
		}

		dirs &= bit;
	}

	cx = (wx + RoadMapUnitXMid + 750) / 1500;
	cz = (RoadMapUnitZMid + 750 - wz) / 1500;

	switch (dirs)
	{
		case 1:
			target->vx = (float)(1500 * (cx - 1) - RoadMapUnitXMid);
			target->vz = (float)(RoadMapUnitZMid - 1500 * cz);
			break;
		case 2:
			target->vx = (float)(1500 * (cx + 1) - RoadMapUnitXMid);
			target->vz = (float)(RoadMapUnitZMid - 1500 * cz);
			break;
		case 4:
			target->vx = (float)(1500 * cx - RoadMapUnitXMid);
			target->vz = (float)(RoadMapUnitZMid - 1500 * (cz + 1));
			break;
		case 8:
			target->vx = (float)(1500 * cx - RoadMapUnitXMid);
			target->vz = (float)(RoadMapUnitZMid - 1500 * (cz - 1));
			break;
	}

	target->vy = (float)-MapHeight(target);

	return dirs;
}

// [0x515e32] [D]
// NOTE: no name in the database and no callers. Starts a random action; the index (0 - 7) runs
// past the 3 PedestrianInitHandlers (as in the original).
void sub_515E32(PEDESTRIAN *ped)
{
	int action;

	action = Random() % 8;

	if (ped->type == 1 || ped->type == 2 || ped->type == 3)
		PedestrianInitHandlers[action](ped);
}

// [0x515ea7] [D]
// NOTE: no name in the database (HandlePedestrians, type 1: walking)
void sub_515EA7(PEDESTRIAN *ped)
{
	PedestrianActionHandlers[ped->motion_type_2 - 1](ped);
}

// [0x515ec0] [D]
// NOTE: no name in the database (HandlePedestrians, type 2: running)
void sub_515EC0(PEDESTRIAN *ped)
{
	PedestrianActionHandlers[ped->motion_type_2 - 1](ped);
}

// [0x515ed9] [D]
// NOTE: no name in the database (HandlePedestrians, type 3: sitting)
void sub_515ED9(PEDESTRIAN *ped)
{
	CalculatePedestrianInterest(ped);
}

// [0x515eea] [D]
// NOTE: no name in the database (HandlePedestrians, flags_1 0x800). A jump: up with velocity
// field_74 under gravity 9, 4 frames at the top (flags_2 0x10, frame count in bits 0 - 1), then
// falling (0x20) until the ground.
void sub_515EEA(PEDESTRIAN *ped)
{
	int ground;
	int count;

	ped->position.vx = ped->position.vx + ped->field_74.vx;
	ped->position.vy = ped->position.vy + ped->field_74.vy;
	ped->position.vz = ped->position.vz + ped->field_74.vz;

	ground = -MapHeight(&ped->position);

	if (ped->flags_2 & 0x10)
	{
		count = ((ped->flags_2 & 3) + 1) & 3;

		ped->flags_2 &= ~3;
		ped->flags_2 |= count;

		if (count == 0)
		{
			ped->flags_2 &= ~0x10;
			ped->flags_2 |= 0x20;
		}
	}
	else if (ped->flags_2 & 0x20)
	{
		if (ped->position.vy <= (float)ground)
		{
			ped->field_74.vy = ped->field_74.vy + 9.0f;
		}
		else
		{
			ped->position.vy = (float)ground;
			ped->flags_1 &= ~0x800;
			ped->flags_2 = 0;
		}
	}
	else if (ped->field_74.vy >= 0.0f)
	{
		ped->field_74.vy = ped->field_74.vy + 9.0f;
	}
	else if (ped->field_74.vy + 9.0f < 0.0f)
	{
		ped->field_74.vy = ped->field_74.vy + 9.0f;
	}
	else
	{
		ped->field_74.vy = 0.0f;
		ped->flags_2 |= 0x10;
	}

	if (ped->field_74.vy > 55.0f)
		ped->field_74.vy = 55.0f;
}

// [0x5160dd] [D]
void PedestrianAction_WalkToTarget(PEDESTRIAN *ped)
{
	int last_dir;
	SVECTOR direction;

	last_dir = ped->last_dir;

	CalculatePedestrianInterest(ped);

	direction = ped->direction;

	if (sub_516E7F(ped, 5) == 1)
	{
		NewTarget(ped);

		if ((ped->flags_1 & 0x10) && ped->last_dir != last_dir)
		{
			ped->direction.vy = direction.vy;
			SetPedestrianTurn(ped);
		}
	}
	else if ((ped->flags_1 & 0x80) == 0 && (ped->flags_1 & 0x400) == 0 && (ped->flags_1 & 8) == 0)
	{
		SetPedestrianTurn(ped);
	}
}

// [0x51619c] [D]
void PedestrianAction_RunToTarget(PEDESTRIAN *ped)
{
	int last_dir;
	SVECTOR direction;

	last_dir = ped->last_dir;
	direction = ped->direction;

	if (sub_516E7F(ped, 5) == 1)
	{
		NewTarget(ped);

		if ((ped->flags_1 & 0x10) && ped->last_dir != last_dir)
		{
			ped->direction.vy = direction.vy;
			SetPedestrianTurn(ped);
		}
	}
	else if ((ped->flags_1 & 0x80) == 0 && (ped->flags_1 & 0x400) == 0 && (ped->flags_1 & 8) == 0)
	{
		SetPedestrianTurn(ped);
	}
}

// [0x51624e] [D]
void PedestrianAction_Sitting(PEDESTRIAN *ped)
{
}

// [0x516253] [D]
// At the end of the dive, back to walking (running for type 2).
void PedestrianAction_Dive(PEDESTRIAN *ped)
{
	ped->flags_1 &= ~0x3000;

	if (ped->speed + ped->frame >= ped->num_frames)
	{
		if (ped->type == 2)
			ped->motion_type_2 = 2;
		else
			ped->motion_type_2 = 1;

		ped->field_20 = 0;
		ped->num_frames = mcd_frames[ped->motion_type_2 - 1];
		ped->speed = 1;
		ped->head_rotation = 0;
		ped->frame = 0;
	}
}

// [0x5162e3] [D]
// The direction (4 north, 2 east, 8 south, 1 west) of an angle.
int GetPedDirection(short angle)
{
	int a;

	a = angle & 0xFFF;

	if (a < 0x200)
		return 4;

	if (a < 0x600)
		return 2;

	if (a < 0xA00)
		return 8;

	if (a < 0xE00)
		return 1;

	if (a >= 0x1000)
		return 0;

	return 4;
}

// [0x516345] [D]
// Starts turning towards the velocity: left (0x1000) or right (0x2000) by field_18 to the
// angle field_14.
void SetPedestrianTurn(PEDESTRIAN *ped)
{
	int angle;
	int diff;

	angle = (-Arc_Tan(float_to_int(ped->velocity.vz), float_to_int(ped->velocity.vx)) - 1024) & 0xFFF;
	diff = DIFF_ANGLES(ped->direction.vy, angle);

	if (diff >= 0)
	{
		ped->flags_1 |= 0x2000;
	}
	else
	{
		ped->flags_1 |= 0x1000;
		diff = -diff;
	}

	ped->field_14 = angle;
	ped->field_18 = diff;
}

// [0x516441] [D]
void PedestrianActionInit_WalkToTarget(PEDESTRIAN *ped)
{
	int dir;

	dir = CalcPedestrianDirection(0, float_to_int(ped->position.vx), float_to_int(ped->position.vz), &ped->target);

	if (dir != 0)
	{
		ped->last_dir = dir;
		CorrectPathPosition(ped, &ped->position);
		CorrectTargetPosition(ped, &ped->target);
		CorrectPedDirection(ped);
		sub_516DA7(ped, 1, 0);
	}
	else
	{
		DestroyPedestrian(ped);
	}
}

// [0x516512] [D]
void PedestrianActionInit_Sitting(PEDESTRIAN *ped)
{
	int dir;

	dir = CalcPedestrianDirection(0, float_to_int(ped->position.vx), float_to_int(ped->position.vz), &ped->target);

	if (dir != 0)
	{
		ped->last_dir = dir;
		CorrectPathPosition(ped, &ped->position);
		CorrectTargetPosition(ped, &ped->target);
		CorrectPedDirection(ped);
		sub_516DA7(ped, 4, 0);
	}
	else
	{
		DestroyPedestrian(ped);
	}
}

// [0x5165e3] [D]
void PedestrianActionInit_RunToTarget(PEDESTRIAN *ped)
{
	int dir;

	dir = CalcPedestrianDirection(0, float_to_int(ped->position.vx), float_to_int(ped->position.vz), &ped->target);

	if (dir != 0)
	{
		ped->last_dir = dir;
		CorrectPathPosition(ped, &ped->position);
		CorrectTargetPosition(ped, &ped->target);
		CorrectPedDirection(ped);
		sub_516DA7(ped, 2, 0);
	}
	else
	{
		DestroyPedestrian(ped);
	}
}

// [0x5166b4] [D]
// Moves pos across the direction of travel to the side of the pavement cell: 550 from the centre
// towards the road (flags_1 0x100) when the cell on the left is road, else 350 away.
void CorrectPathPosition(PEDESTRIAN *ped, VECTOR4 *pos)
{
	short north;
	short south;
	short west;
	short east;
	int x;
	int z;

	x = 1500 * (int)((pos->vx + 750.0f + (float)RoadMapUnitXMid) / 1500.0f) - RoadMapUnitXMid;
	z = RoadMapUnitZMid - 1500 * (int)(((float)RoadMapUnitZMid - (pos->vz - 750.0f)) / 1500.0f);

	north = ROADS_GetRoadOrJunction(x, z - 1500);
	south = ROADS_GetRoadOrJunction(x, z + 1500);
	west = ROADS_GetRoadOrJunction(x - 1500, z);
	east = ROADS_GetRoadOrJunction(x + 1500, z);

	switch (ped->last_dir)
	{
		case 1:
			if (north < 0)
			{
				ped->flags_1 &= ~0x100;
				pos->vz = (float)(z + 350);
			}
			else
			{
				ped->flags_1 |= 0x100;
				pos->vz = (float)(z - 550);
			}
			break;
		case 2:
			if (south < 0)
			{
				ped->flags_1 &= ~0x100;
				pos->vz = (float)(z - 350);
			}
			else
			{
				ped->flags_1 |= 0x100;
				pos->vz = (float)(z + 550);
			}
			break;
		case 4:
			if (east < 0)
			{
				ped->flags_1 &= ~0x100;
				pos->vx = (float)(x - 350);
			}
			else
			{
				ped->flags_1 |= 0x100;
				pos->vx = (float)(x + 550);
			}
			break;
		case 8:
			if (west < 0)
			{
				ped->flags_1 &= ~0x100;
				pos->vx = (float)(x + 350);
			}
			else
			{
				ped->flags_1 |= 0x100;
				pos->vx = (float)(x - 550);
			}
			break;
	}
}

// [0x516959] [D]
// Moves the target to the corner of its cell on the pedestrian's side of the pavement.
void CorrectTargetPosition(PEDESTRIAN *ped, VECTOR4 *target)
{
	short north;
	short south;
	short west;
	short east;
	int x;
	int z;

	x = 1500 * (int)((target->vx + 750.0f + (float)RoadMapUnitXMid) / 1500.0f) - RoadMapUnitXMid;
	z = RoadMapUnitZMid - 1500 * (int)(((float)RoadMapUnitZMid - (target->vz - 750.0f)) / 1500.0f);

	north = ROADS_GetRoadOrJunction(x, z - 1500);
	south = ROADS_GetRoadOrJunction(x, z + 1500);
	west = ROADS_GetRoadOrJunction(x - 1500, z);
	east = ROADS_GetRoadOrJunction(x + 1500, z);

	switch (ped->last_dir)
	{
		case 1:
			if (north < 0)
			{
				target->vx = (float)(x - 350);
				target->vz = (float)(z + 350);
			}
			else
			{
				target->vx = (float)(x - 550);
				target->vz = (float)(z - 550);
			}
			break;
		case 2:
			if (south < 0)
			{
				target->vx = (float)(x + 350);
				target->vz = (float)(z - 350);
			}
			else
			{
				target->vx = (float)(x + 550);
				target->vz = (float)(z + 550);
			}
			break;
		case 4:
			if (west < 0)
			{
				target->vx = (float)(x + 550);
				target->vz = (float)(z - 550);
			}
			else
			{
				target->vx = (float)(x - 350);
				target->vz = (float)(z - 350);
			}
			break;
		case 8:
			if (east < 0)
			{
				target->vx = (float)(x - 550);
				target->vz = (float)(z + 550);
			}
			else
			{
				target->vx = (float)(x + 350);
				target->vz = (float)(z + 350);
			}
			break;
	}
}

// [0x516c27] [D]
// Interest in the player (wheelspin close by) and the head turned towards the player, for
// walkers and sitters within 840 of straight ahead.
void CalculatePedestrianInterest(PEDESTRIAN *ped)
{
	int interest;
	int dist;
	int angle;
	float dx;
	float dz;

	interest = 0;
	ped->head_rotation = 0;

	if (car_data[0].wheelspin)
		interest = 10;

	dx = ped->position.vx - (float)car_data[0].location.vx;
	dz = ped->position.vz - (float)car_data[0].location.vz;

	// NOTE: the squared distance over 1500 (as in the original)
	dist = float_to_int(dx * dx + dz * dz) / 1500;

	if (dist > 5)
		interest = 0;
	else if (dist > 0)
		interest /= dist;

	ped->interest = interest;

	angle = ((ped->direction.vy - (Arc_Tan(float_to_int(dx), float_to_int(dz)) & 0xFFF) + 2048) & 0xFFF) - 2048;
	ped->head_rotation = angle & 0xFFF;

	if (ped->motion_type_2 != 1 && ped->type != 3)
		ped->head_rotation = 0;
	else if (angle > 840 || angle < -840)
		ped->head_rotation = 0;
}

// [0x516da7] [D]
// NOTE: no name in the database (the PedestrianActionInit functions). Starts the motion (1 walk,
// 2 run, 3 dive, 4 sit) from its first frame.
void sub_516DA7(PEDESTRIAN *ped, int motion, int a3)
{
	ped->motion_type_1 = 0;
	ped->field_24 = 0;
	ped->interp_frame = 0;
	ped->flags_1 &= ~0x480;
	ped->motion_type_2 = motion;
	ped->field_20 = a3;
	ped->num_frames = mcd_frames[motion - 1];
	ped->frame = 0;

	if (motion == 2)
		ped->speed = 2;
	else
		ped->speed = 1;
}

// [0x516e23] [D]
// NOTE: no name in the database and no callers. Queues the motion with flags | 0x400.
void sub_516E23(PEDESTRIAN *ped, short motion, int a3, int flags)
{
	ped->motion_type_1 = motion;
	ped->field_24 = a3;
	ped->interp_frame = 0;
	ped->flags_1 = flags | 0x400;
}

// [0x516e52] [D]
// NOTE: no name in the database (SetupCivJump). Blends into the motion over interp_frame frames
// (flags | 0x80).
void sub_516E52(PEDESTRIAN *ped, short motion, int a3, int flags, int interp_frame)
{
	ped->motion_type_1 = motion;
	ped->field_24 = a3;
	ped->interp_frame = interp_frame;
	ped->flags_1 = flags | 0x80;
}

// [0x516e7f] [D]
// NOTE: no name in the database (the PedestrianAction functions). 1 if the pedestrian will be
// past its target (moving away from it) after frames * speed more frames, 0 while blending. The
// sine and cosine are truncated to int (0 but at the right angles) as in the original.
int sub_516E7F(PEDESTRIAN *ped, int frames)
{
	MOTION_PACKET *packet;
	char *data;
	int count;
	int frame;
	int sn;
	int cs;
	float x;
	float z;
	float dx;
	float dz;

	if (ped->flags_1 & 0x80)
		return 0;

	count = ped->speed * frames;
	frame = ped->frame;
	data = MC_Data[ped->motion_type_2 - 1];

	sn = float_to_int(fsin(ped->direction.vy));
	cs = float_to_int(fcos(ped->direction.vy));

	x = ped->position.vx;
	z = ped->position.vz;

	while (count-- != 0)
	{
		if (frame > ped->num_frames)
			frame %= ped->num_frames;

		packet = (MOTION_PACKET *)(data + frame * MOTION_FRAME_SIZE);

		x = (float)cs * packet->root.vx + (float)sn * packet->root.vz + x;
		z = (float)cs * packet->root.vz - (float)sn * packet->root.vx + z;

		frame++;
	}

	dx = ped->target.vx - x;
	dz = ped->target.vz - z;

	return dx * ped->velocity.vx + dz * ped->velocity.vz < 0.0f;
}

// [0x51702c] [D]
// NOTE: no name in the database and no callers. Blending into another motion?
int sub_51702C(PEDESTRIAN *ped)
{
	return (ped->flags_1 & 0x80) != 0;
}

// [0x51704a] [D]
// NOTE: no name in the database and no callers. The motion ends within frames * speed frames?
int sub_51704A(PEDESTRIAN *ped, int frames)
{
	return ped->frame + ped->speed * frames >= ped->num_frames;
}

// [0x517079] [D]
// NOTE: no name in the database (StepPedestrianAnimation at the end of a blend). Switches to the
// queued motion_type_1.
void sub_517079(PEDESTRIAN *ped)
{
	ped->motion_type_2 = ped->motion_type_1;
	ped->field_20 = ped->field_24;
	ped->motion_type_1 = 0;
	ped->field_24 = 0;
	ped->num_frames = mcd_frames[ped->motion_type_2 - 1];
	ped->speed = 1;
	ped->frame = 0;
	ped->head_rotation = 0;
	ped->flags_1 &= ~0x80;
}

// [0x5170ea] [D]
// NOTE: no name in the database and no callers. 1 for the pedestrian types 1 - 3, else -1.
int sub_5170EA(PEDESTRIAN *ped)
{
	switch (ped->type)
	{
		case 1:
			return 1;
		case 2:
			return 1;
		case 3:
			return 1;
	}

	return -1;
}

// [0x517127] [D]
// Faces the pedestrian (and its velocity) at the target.
void CorrectPedDirection(PEDESTRIAN *ped)
{
	ped->direction.vx = 2048;
	ped->direction.vz = 0;

	ped->velocity.vx = ped->target.vx - ped->position.vx;
	ped->velocity.vy = ped->target.vy - ped->position.vy;
	ped->velocity.vz = ped->target.vz - ped->position.vz;

	ped->direction.vy = (-Arc_Tan(float_to_int(ped->velocity.vz), float_to_int(ped->velocity.vx)) - 1024) & 0xFFF;
}

// [0x5171ec] [D]
// At a cell centre: on along last_dir if the pavement goes on, else a random other way but back
// (back only when there is no other). No way at all removes the pedestrian.
void NewTarget(PEDESTRIAN *ped)
{
	int last_dir;
	int back;
	int dirs;
	int count;
	int bit;
	int pick;
	int x;
	int z;

	last_dir = ped->last_dir;
	x = float_to_int(ped->position.vx);
	z = float_to_int(ped->position.vz);

	dirs = 0;
	count = 0;

	if (pedestrian_roads.west == -32768)
	{
		dirs = 1;
		count = 1;
	}

	if (pedestrian_roads.east == -32768)
	{
		dirs |= 2;
		count++;
	}

	if (pedestrian_roads.north == -32768)
	{
		dirs |= 4;
		count++;
	}

	if (pedestrian_roads.south == -32768)
	{
		dirs |= 8;
		count++;
	}

	if (count == 0)
	{
		DestroyPedestrian(ped);
		return;
	}

	// NOTE: the four cases (1, 2, 4, 8), the same code with other bits in the original, share
	// one block here
	switch (last_dir)
	{
		case 1:
			back = 2;
			break;
		case 2:
			back = 1;
			break;
		case 4:
			back = 8;
			break;
		case 8:
			back = 4;
			break;
		default:
			back = 0;
			break;
	}

	if (back != 0)
	{
		// the way back is kept aside in bits 8 - 11
		if (dirs & back)
		{
			dirs = (dirs & ~back) | (back << 8);
			count--;
		}

		if (dirs & last_dir)
		{
			dirs &= last_dir;
		}
		else if (count != 0)
		{
			bit = 16;
			pick = Random() % count + 1;

			while (pick != 0)
			{
				bit >>= 1;

				if (bit & dirs)
					pick--;
			}

			dirs &= bit;
		}
		else
		{
			dirs = (dirs >> 8) & 0xF;
		}
	}

	if (dirs == 0)
	{
		DestroyPedestrian(ped);
		return;
	}

	ped->last_dir = dirs;

	x = 1500 * ((x + RoadMapUnitXMid + 750) / 1500) - RoadMapUnitXMid;
	z = RoadMapUnitZMid - 1500 * ((RoadMapUnitZMid + 750 - z) / 1500);

	switch (dirs)
	{
		case 1:
			ped->target.vx = (float)(x - 1500);
			ped->target.vz = (float)z;
			break;
		case 2:
			ped->target.vx = (float)(x + 1500);
			ped->target.vz = (float)z;
			break;
		case 4:
			ped->target.vx = (float)x;
			ped->target.vz = (float)(z - 1500);
			break;
		case 8:
			ped->target.vx = (float)x;
			ped->target.vz = (float)(z + 1500);
			break;
	}

	ped->target.vy = (float)-MapHeight(&ped->target);

	CorrectTargetPosition(ped, &ped->target);
	CorrectPedDirection(ped);
}

// [0x5176fe] [D]
// The pedestrians within 750 of a car sounding its horn jump out of the way.
void CheckHornReaction(CAR_DATA *cp)
{
	PEDESTRIAN *ped;
	int i;
	int dx;
	int dz;
	int dist;

	for (ped = pedestrians, i = 0; i < 64; i++, ped++)
	{
		if ((ped->flags_1 & 0x10) && ped->motion_type_2 != 3 && (ped->flags_1 & 0x880) == 0)
		{
			dx = abs(float_to_int(ped->position.vx) - cp->location.vx);
			dz = abs(float_to_int(ped->position.vz) - cp->location.vz);

			if (dx >= dz)
				dist = dz / 2 + dx;
			else
				dist = dx / 2 + dz;

			if (dist <= 750)
				SetupCivJump(ped, cp);
		}
	}
}

// [0x51782f] [D]
// A pedestrian in the player's path (gPlayerFrustrum), or a sitter whose chair was smashed,
// dives away; a felony.
void CheckForPlayerCar(PEDESTRIAN *ped)
{
	int chair_ok;
	int dx;
	int dz;
	int dist;

	chair_ok = 1;

	if ((ped->flags_1 & 0x80) || ped->motion_type_2 == 3 || (ped->flags_1 & 0x800))
		return;

	if (ped->type == 3)
		chair_ok = CheckForSmashedChair(float_to_int(ped->position.vx), float_to_int(ped->position.vz)) == 0;

	if (chair_ok)
	{
		dx = abs(float_to_int(ped->position.vx) - car_data[0].location.vx);
		dz = abs(float_to_int(ped->position.vz) - car_data[0].location.vz);

		if (dx >= dz)
			dist = dz / 2 + dx;
		else
			dist = dx / 2 + dz;

		if (dist <= frus_len && CheckForCar(ped, &gPlayerFrustrum))
		{
			SetupCivJump(ped, car_data);
			NoteFelony(&gFelonyData, 1, 4096);
		}
	}
	else
	{
		SetupCivJump(ped, car_data);
		NoteFelony(&gFelonyData, 1, 4096);
	}
}

// [0x5179f3] [D]
// Turns the pedestrian away from the car's path and starts the dive.
void SetupCivJump(PEDESTRIAN *ped, CAR_DATA *cp)
{
	int angle;

	angle = sub_517EAA(ped, cp);
	ped->type = 1;

	if (dword_BD9AEC)
	{
		ped->flags_1 |= 0x800;
		ped->field_74.vx = (float)(isin(angle) >> 7);
		ped->field_74.vz = (float)(icos(angle) >> 7);
		ped->field_74.vy = -55.0f;
	}
	else
	{
		ped->field_14 = angle;
		ped->direction.vy = angle;
		sub_516E52(ped, 3, 0, ped->flags_1 & ~0x400, 1);
	}
}

// [0x517aaf] [D]
// The area in front of the car (as wide as its rotated box, length long) as four planes
// (nx, -, nz, d) in the 16 floats of frustrum: a point p is inside when p.n <= d for all.
void BuildCollisionFrustrum(CAR_DATA *cp, MATRIX *frustrum, int length)
{
	VECTOR4 *plane;
	int *box;
	float x;
	float z;
	float px;
	float pz;
	float minx;
	float maxx;
	float width;
	float cs;
	float sn;
	int angle;
	int half;
	int i;

	plane = (VECTOR4 *)frustrum;

	x = (float)cp->location.vx;
	z = (float)cp->location.vz;

	angle = -Arc_Tan(cp->st.n.linearVelocity[0] >> 8, cp->st.n.linearVelocity[2] >> 8) & 0xFFF;

	box = pre_rotated_boxes[(DIFF_ANGLES(angle, cp->direction) & 0xFFF) / 4][0];

	minx = (float)box[0];
	maxx = (float)box[0];

	for (i = 2; i < 8; i += 2)
	{
		if ((float)box[i] < minx)
			minx = (float)box[i];
	}

	for (i = 2; i < 8; i += 2)
	{
		if ((float)box[i] > maxx)
			maxx = (float)box[i];
	}

	width = maxx - minx;

	if (width < 0.0f)
		width = -width;

	half = float_to_int(width) >> 1;

	cs = fcos(angle);
	sn = fsin(angle);

	// side
	px = (float)-half * cs + x;
	pz = (float)-half * sn + z;
	plane[0].vx = -cs;
	plane[0].vz = -sn;
	plane[0].vw = px * plane[0].vx + pz * plane[0].vz;

	// other side
	px = (float)half * cs + x;
	pz = (float)half * sn + z;
	plane[1].vx = cs;
	plane[1].vz = sn;
	plane[1].vw = px * plane[1].vx + pz * plane[1].vz;

	// far end
	px = (float)length * -sn + x;
	pz = (float)length * cs + z;
	plane[2].vx = -sn;
	plane[2].vz = cs;
	plane[2].vw = px * plane[2].vx + pz * plane[2].vz;

	// the car
	plane[3].vx = sn;
	plane[3].vz = -cs;
	plane[3].vw = x * plane[3].vx + z * plane[3].vz;
}

// [0x517dd1] [D]
// Is the pedestrian inside the frustrum of BuildCollisionFrustrum?
int CheckForCar(PEDESTRIAN *ped, MATRIX *frustrum)
{
	VECTOR4 *plane;
	float x;
	float z;
	int i;

	plane = (VECTOR4 *)frustrum;

	x = ped->position.vx;
	z = ped->position.vz;

	for (i = 0; i < 3; i++)
	{
		if (x * plane[i].vx + z * plane[i].vz > plane[i].vw)
			return 0;
	}

	return x * plane[3].vx + z * plane[3].vz <= plane[3].vw;
}

// [0x517eaa] [D]
// NOTE: no name in the database (SetupCivJump). The angle to dive at: square to the car's
// direction of travel, to the side of it the pedestrian is on.
int sub_517EAA(PEDESTRIAN *ped, CAR_DATA *cp)
{
	int angle;
	float dx;
	float dz;

	angle = Arc_Tan(cp->st.n.linearVelocity[0] >> 8, cp->st.n.linearVelocity[2] >> 8) & 0xFFF;

	dx = ped->position.vx - (float)cp->location.vx;
	dz = ped->position.vz - (float)cp->location.vz;

	if (DIFF_ANGLES(angle, Arc_Tan(float_to_int(dx), float_to_int(dz))) <= 0)
		return (angle + 1024) & 0xFFF;

	return (angle - 1024) & 0xFFF;
}
