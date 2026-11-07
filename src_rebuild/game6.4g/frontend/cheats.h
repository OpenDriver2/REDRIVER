/*
 * frontend/cheats.h - globals and functions of frontend/cheats.c used by other files
 */

#ifndef FRONTEND_CHEATS_H
#define FRONTEND_CHEATS_H

#include "dr_types.h"

// globals
extern short CurrentJunction;
extern int gPlayerCarDamage;
extern short CurrentRoad;
extern int gCheatsAllowed[11];
extern CHEATS gCheatFlags;

// functions
extern int SelectCheats(void);

#endif // FRONTEND_CHEATS_H
