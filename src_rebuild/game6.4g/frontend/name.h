/*
 * frontend/name.h - globals and functions of frontend/name.c used by other files
 */

#ifndef FRONTEND_NAME_H
#define FRONTEND_NAME_H

#include "dr_types.h"

// functions
extern int SetupEnterPlayerName(char *buffer, int max_length);
extern void UnlockAllCheats(void);

#endif // FRONTEND_NAME_H
