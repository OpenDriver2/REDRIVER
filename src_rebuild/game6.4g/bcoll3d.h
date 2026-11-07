/*
 * bcoll3d.h - globals and functions of bcoll3d.c used by other files
 */

#ifndef BCOLL3D_H
#define BCOLL3D_H

#include "dr_types.h"

// functions
extern int CarCarCollision3(CAR_DATA *c0, CAR_DATA *c1, int *depth, IVECTOR *where, IVECTOR *normal, IVECTOR *offset);

#endif // BCOLL3D_H
