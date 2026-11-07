/*
 * motion_c.h - globals and functions of motion_c.c used by other files
 */

#ifndef MOTION_C_H
#define MOTION_C_H

#include "dr_types.h"

// globals
// most references: motion_c.c 4, pedest.c 1
extern int pedestrian_data_ok;
// most references: pedest.c 5, motion_c.c 4, minicutscene.c 2
extern int mcd_frames[4];
// most references: pedest.c 12, minicutscene.c 8, motion_c.c 1
extern VECTOR4 ped_trans;
// most references: motion_c.c 10, pedest.c 7, minicutscene.c 1
extern char *MC_Data[4];
// most references: motion_c.c 1, pedest.c 1
extern QUATERNION lerp_quaternions[32];
extern VECTOR4 bone_lengths[32];
// most references: minicutscene.c 2, motion_c.c 2, pedest.c 2
extern HIERARCHY_NODE hierarchy_tree[48];
// most references: pedest.c 8, motion_c.c 7, minicutscene.c 3
extern int num_objects;
// most references: minicutscene.c 1, motion_c.c 1, pedest.c 1
extern int model_base;
// most references: minicutscene.c 4, pedest.c 2, motion_c.c 1
extern int do_z_clipping;
extern int ped3base;
extern int ped4base;
// most references: motion_c.c 3, pedest.c 1
extern int ped1base;
extern int ped2base;

// functions
extern void FreeMotionCaptureData(void);
extern int InitMotionCaptureData(void);
extern void LerpObjects(MOTION_PACKET *a, MOTION_PACKET *b, float t);
extern void WalkMotionObject(PEDESTRIAN *ped, QUATERNION *frame, HIERARCHY_NODE *hnp, QUATERNION *rot, VECTOR4 *pos);

#endif // MOTION_C_H
