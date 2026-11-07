/*
 * frontend/selrace.h - globals and functions of frontend/selrace.c used by other files
 */

#ifndef FRONTEND_SELRACE_H
#define FRONTEND_SELRACE_H

#include "dr_types.h"

// globals
extern int want_score;

// functions
extern int CheckIfCheating(void);
extern int SelectRace(void);

#endif // FRONTEND_SELRACE_H
