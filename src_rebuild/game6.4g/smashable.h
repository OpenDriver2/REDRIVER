/*
 * smashable.h - globals and functions of smashable.c used by other files
 */

#ifndef SMASHABLE_H
#define SMASHABLE_H

#include "dr_types.h"

// globals
extern DAMAGE_LAMP damaged_lamp[4];
extern int ModelTimeBonus[1152];

// functions
extern void DrawSmashable_sprites(char transparent);
extern int EnableModel(int model, int active);
extern void InitialiseSmashableObjects(void);
extern void MoveSmashable_object(void);
extern int damage_lamp(CELL_OBJECT *cop);
extern int damage_object(CELL_OBJECT *cop, IVECTOR *velocity);

#endif // SMASHABLE_H
