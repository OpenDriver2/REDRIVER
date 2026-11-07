/*
 * dr_scores.h - globals and functions of scores.c used by other files
 */

#ifndef DR_SCORES_H
#define DR_SCORES_H

#include "dr_types.h"

// globals
extern SCORE_DATA global_score;

// functions
extern HIGHSCORE_DATA *CheckForRecord(int *scorepos, int best_lap);
extern HIGHSCORE_DATA *GetHighScoreTable(int type, int index);
extern void InitialiseHighScores(void);
extern int LoadHighScores(FILE *f);
extern int SaveHighScores(FILE *f);

#endif // DR_SCORES_H
