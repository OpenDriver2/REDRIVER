/*
 * lamps.h - globals and functions of lamps.c used by other files
 */

#ifndef LAMPS_H
#define LAMPS_H

#include "dr_types.h"

// functions
extern void DestroyLamps(void);
extern void InvertLamps(void);
extern void ProcessLamps(void);
extern int ProcessLampsLump(FILE *lump_file);

#endif // LAMPS_H
