/*
 * bcollide.h - globals and functions of bcollide.c used by other files
 */

#ifndef BCOLLIDE_H
#define BCOLLIDE_H

#include "dr_types.h"

// functions
extern int CarBuildingCollision(CAR_DATA *cp, BUILDING_BOX *building, CELL_OBJECT *cop);
extern int DamageCar3D(CAR_DATA *cp, LONGVECTOR4 *delta, int strikeVel);

#endif // BCOLLIDE_H
