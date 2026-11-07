/*
 * debug.h - globals and functions of debug.c used by other files
 */

#ifndef DEBUG_H
#define DEBUG_H

#include "dr_types.h"

// globals
extern char DebugFont[2056];

// functions
extern void DBGCR(void);
extern void DBGPrintCol(u_char r, u_char g, u_char b);
extern void DBGPrintPos(float x, float y);
extern void DBGPrintSize(int size);
extern void DBGPrintString(char *text);
extern void DisplayDebugStats(char *screen, char fg, char bg);
extern void LogQuick(char *text);

#endif // DEBUG_H
