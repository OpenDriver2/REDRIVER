/*
 * chairs.h - globals and functions of chairs.c used by other files
 */

#ifndef CHAIRS_H
#define CHAIRS_H

#include "dr_types.h"

// globals
extern int chair_count;

// functions
extern int CheckForSmashedChair(int x, int z);
extern CHAIR *GetRandomChair(int cell_num);
extern void HandleChairs(void);
extern void InitialiseChairs(void);
extern void ProcessChairLump(FILE *lump_file, int lump_size);
extern void ShutdownChairs(void);
extern int SmashChair(float x, float z);

#endif // CHAIRS_H
