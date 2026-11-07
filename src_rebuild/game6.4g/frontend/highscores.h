/*
 * frontend/highscores.h - globals and functions of frontend/highscores.c used by other files
 */

#ifndef FRONTEND_HIGHSCORES_H
#define FRONTEND_HIGHSCORES_H

#include "dr_types.h"

// globals
extern GAMETYPE highscore_type;
extern int highscore_index;

// functions
extern int SelectShowHighscore(void);

#endif // FRONTEND_HIGHSCORES_H
