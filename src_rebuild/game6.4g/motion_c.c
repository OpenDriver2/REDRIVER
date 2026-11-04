/*
 * motion_c.c - motion capture data for the pedestrians: loading the .MCD files and drawing a
 * skeleton of models from a frame
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "motion_c.c", 0x4f55b0 - 0x4f6003).
 *
 * MC_Data[0 - 3] hold the walk, run, dive and sitting animations (mcd_frames frames each). A
 * frame (MOTION_PACKET) is the root motion since the previous frame, then a rotation per object
 * of the skeleton. hierarchy_tree and bone_lengths come from the first file with them; a node
 * with num_children 255 is a model drawn at the end of its bone chain (WalkMotionObject).
 * ped1base - ped4base are the first models of the pedestrian skeletons, ped1head - ped4head the
 * head relative to them (turned by PEDESTRIAN.head_rotation).
 *
 * The bss sits in the linker's common area (among mirror.c, frontend and map data), so its owner
 * is the main user; the unnamed 16 bytes after ped_trans and MC_Data are left out.
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "dr_types.h"
#include "motion_c.h"
#include "dr_globals.h"
#include "asmmath.h"
#include "debug.h"
#include "general.h"
#include "map.h"
#include "shell.h"
#include "testmath.h"
#include "Engine/3dfx/object.h"

int LoadMCDFile(char *filename, char *data, int startCut, int endCut);

// NOTE: SITTING1.MCD is never loaded (only 4 motion types)
char *mcd_fnames[5] = { "WALK.MCD", "RUN.MCD", "DIVE.MCD", "SITTING.MCD", "SITTING1.MCD" };

// frames cut from the start and the end of each file (all 0)
// NOTE: int[4] in IDA; 5 by the layout (4 bytes free after each), as mcd_fnames
int mcd_start_cut[5];
int mcd_end_cut[5];

int pedestrian_data_ok;					// NOTE: owner guessed (most references)
int mcd_frames[4];
VECTOR4 ped_trans;						// NOTE: owner guessed (pedest.c and minicutscene.c use it most)
char *MC_Data[4];
QUATERNION lerp_quaternions[32];
VECTOR4 bone_lengths[32];
HIERARCHY_NODE hierarchy_tree[48];
int num_objects;
int model_base;
int do_z_clipping;
float mcd_speed[4];
int ped3base;
int ped2head;
int ped4base;
int ped3head;
int ped1base;
int ped4head;
int ped2base;
int ped1head;

// [0x4f55b0] [D]
// Reads a .MCD file into data (MOTION_PACKET frames), without startCut frames at the start and
// endCut at the end. Root motion: no x, the height, and the z moved since the previous frame.
// Returns the number of frames loaded, 0 on failure.
int LoadMCDFile(char *filename, char *data, int startCut, int endCut)
{
	char text[256];
	FILE *fp;
	VECTOR4 *bone;
	QUATERNION *quat;
	VECTOR4 *root;
	VECTOR pos;
	VECTOR last;
	VECTOR len;
	float rot[4];
	int save_mode;
	int num_frames;
	int frames;
	int i;
	int j;

	frames = 0;

	sprintf(text, "Attempting to load file: %s", filename);
	LogQuick(text);

	fp = fopen(filename, "rb");

	if (fp == NULL)
		return frames;

	DriverFRead(&save_mode, 4, 1, fp);
	DriverFRead(&num_objects, 4, 1, fp);
	DriverFRead(&num_frames, 4, 1, fp);

	if (save_mode)
	{
		// the skeleton
		bone = bone_lengths;

		for (i = 0; i < num_objects; i++, bone++)
		{
			DriverFRead(&len, 12, 1, fp);

			bone->vx = len.vx;
			bone->vy = len.vy;
			bone->vz = len.vz;
			bone->vw = 1.0f;
		}

		bone_lengths[0].vz = 0.0f;
		bone_lengths[0].vy = 0.0f;
		bone_lengths[0].vx = 0.0f;

		DriverFRead(hierarchy_tree, 8, 48, fp);
	}

	memset(&last, 0, sizeof(last));
	quat = (QUATERNION *)data;

	for (i = 0; i < num_frames; i++)
	{
		DriverFRead(&pos, 12, 1, fp);

		if ((startCut != 0 && i < startCut) || (endCut != 0 && i >= num_frames - (endCut + startCut)))
		{
			for (j = 0; j < num_objects; j++)
				DriverFRead(rot, 16, 1, fp);
		}
		else
		{
			root = &quat->v;

			if (frames == 0)
				last = pos;

			root->vx = 0.0f;
			root->vy = pos.vy;
			root->vz = pos.vz - last.vz;
			root->vw = 1.0f;

			last = pos;
			frames++;

			quat = (QUATERNION *)(root + 1);

			for (j = 0; j < num_objects; j++, quat++)
			{
				DriverFRead(rot, 16, 1, fp);

				quat->v.vx = rot[0];
				quat->v.vy = rot[1];
				quat->v.vz = rot[2];
				quat->v.vw = 1.0f;
				quat->s = rot[3];
			}
		}
	}

	fclose(fp);

	return frames;
}

// [0x4f59ae] [D]
// Finds the pedestrian models and loads the four animations (RUSSJUMP.MCD for walking with the
// RailroadCops cheat); mcd_speed is the length of each animation's total root motion.
int InitMotionCaptureData(void)
{
	MOTION_PACKET *packet;
	float x;
	float y;
	float z;
	int i;
	int k;

	LogQuick("\nAttempting to initialise motion capture data...");

	pedestrian_data_ok = 1;

	ped1base = FindModelWithName("PED1BASE");
	ped2base = FindModelWithName("PED2BASE");
	ped3base = FindModelWithName("PED3BASE");

	if (GameLevel <= 1)
		ped4base = FindModelWithName("PED1BASE");
	else
		ped4base = FindModelWithName("PED4BASE");

	ped1head = FindModelWithName("PED1HEAD");
	ped2head = FindModelWithName("PED2HEAD");
	ped3head = FindModelWithName("PED3HEAD");

	if (GameLevel <= 1)
		ped4head = FindModelWithName("PED1HEAD");
	else
		ped4head = FindModelWithName("PED4HEAD");

	if (ped1base == -1 || ped2base == -1 || ped1head == -1 || ped2head == -1)
	{
		pedestrian_data_ok = 0;

		LogQuick("Could not find required models.  Pedestrians have been disabled.");
		return 0;
	}

	ped1head -= ped1base;
	ped2head -= ped2base;
	ped3head -= ped3base;
	ped4head -= ped4base;

	for (i = 0; i < 4; i++)
		MC_Data[i] = NULL;

	for (i = 0; i < 4; i++)
	{
		MC_Data[i] = (char *)DebugMalloc(0x80002, "D:\\Driver\\game6.4g\\motion_c.c", 257);

		if (MC_Data[i] == NULL)
		{
			pedestrian_data_ok = 0;
			return 0;
		}

		if (i != 0 || gCheatFlags.RailroadCops == 0)
			mcd_frames[i] = LoadMCDFile(mcd_fnames[i], MC_Data[i], mcd_start_cut[i], mcd_end_cut[i]);
		else
			mcd_frames[0] = LoadMCDFile("RUSSJUMP.MCD", MC_Data[0], mcd_start_cut[0], mcd_end_cut[0]);

		if (mcd_frames[i] == 0)
		{
			pedestrian_data_ok = 0;

			LogQuick("Unable to Load file!  Pedestrians have been disabled.");
			return 0;
		}

		LogQuick("File loaded successfully.");

		z = 0.0f;
		y = 0.0f;
		x = 0.0f;

		for (k = 0; k < mcd_frames[i]; k++)
		{
			packet = (MOTION_PACKET *)(MC_Data[i] + k * (sizeof(QUATERNION) * num_objects + sizeof(VECTOR4)));

			x = x + packet->root.vx;
			y = y + packet->root.vy;
			z = z + packet->root.vz;
		}

		mcd_speed[i] = (float)sqrt(x * x + y * y + z * z);
	}

	LogQuick("Motion capture has been initialised.");

	return 1;
}

// [0x4f5d36] [D]
void FreeMotionCaptureData(void)
{
	int i;

	for (i = 0; i < 4; i++)
	{
		if (MC_Data[i] != NULL)
		{
			DebugFree(MC_Data[i]);
			MC_Data[i] = NULL;
		}

		MC_Data[i] = NULL;
	}
}

// [0x4f5d94] [D]
// Walks the skeleton from hnp: each bone turns by its rotation in frame and moves pos along its
// length; a leaf (num_children 255) draws its model (model_base + object_num) at pos + ped_trans,
// the head turned by the pedestrian's head_rotation.
void WalkMotionObject(PEDESTRIAN *ped, QUATERNION *frame, HIERARCHY_NODE *hnp, QUATERNION *rot, VECTOR4 *pos)
{
	VECTOR4 v;
	MATRIX m;
	MATRIX inv;
	QUATERNION q;
	int i;

	if (hnp->num_children == 255)
	{
		v.vx = pos->vx - camera_position.vx + ped_trans.vx;
		v.vy = pos->vy - camera_position.vy + ped_trans.vy;
		v.vz = pos->vz - camera_position.vz + ped_trans.vz;

		CurrentRotationMatrix = &inv_camera_matrix;

		MatrixRotate(&v);
		Quaternion2Matrix(rot, &m);

		if (hnp->object_num == ped1head || hnp->object_num == ped2head)
			RotMatrixY(&m, ped->head_rotation);

		InvertMatrix(&m, &inv);

		DrawObject(model_pointers[hnp->object_num + model_base], &inv, &v, do_z_clipping - 1);
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
			WalkMotionObject(ped, frame, &hierarchy_tree[hnp->children[i]], &q, &v);
	}
}

// [0x4f5f98] [D]
// lerp_quaternions = the rotations of a blended with b by t.
void LerpObjects(MOTION_PACKET *a, MOTION_PACKET *b, float t)
{
	QUATERNION *qa;
	QUATERNION *qb;
	int i;

	qa = a->objects;
	qb = b->objects;

	for (i = 0; i < num_objects; i++)
		LerpQuaternions(qa++, qb++, t, &lerp_quaternions[i]);
}
