/*
 * dr_cars.h - globals and functions of cars.c used by other files
 */

#ifndef DR_CARS_H
#define DR_CARS_H

#include "dr_types.h"

// globals
extern CAR_COSMETICS player_car_cosmetics[18];
extern MATRIX float_body_mat[66];
extern char gang_car_vis[964];
extern int player_damage_factor;

// functions
extern void DrawCar(int id, int slot);
extern int GetCarDamage(int car);
extern void InitCarCosmetics(void);
extern int SelectGangCar(void);

#endif // DR_CARS_H
