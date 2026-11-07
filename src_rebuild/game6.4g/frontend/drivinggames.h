/*
 * frontend/drivinggames.h - globals and functions of frontend/drivinggames.c used by other files
 */

#ifndef FRONTEND_DRIVINGGAMES_H
#define FRONTEND_DRIVINGGAMES_H

#include "dr_types.h"

// functions
extern int SelectDrivingGames(void);
extern void carnage_selfunc(int flag);
extern void carnage_unselfunc(int flag);
extern void checkpoint_selfunc(int flag);
extern void checkpoint_unselfunc(int flag);
extern void dirttrack_selfunc(int flag);
extern void dirttrack_unselfunc(int flag);
extern void survival_selfunc(int flag);
extern void survival_unselfunc(int flag);
extern void trail_selfunc(int flag);
extern void trail_unselfunc(int flag);

#endif // FRONTEND_DRIVINGGAMES_H
