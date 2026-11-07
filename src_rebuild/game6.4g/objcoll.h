/*
 * objcoll.h - globals and functions of objcoll.c used by other files
 */

#ifndef OBJCOLL_H
#define OBJCOLL_H

#include "dr_types.h"

// globals
extern short gLastModelCollisionCheck;

// functions
extern void CheckScenaryCollisions(int id);
extern void DoScenaryCollisions(void);

#endif // OBJCOLL_H
