/*
 * flat/fudge.h - globals and functions of flat/fudge.c used by other files
 */

#ifndef FLAT_FUDGE_H
#define FLAT_FUDGE_H

#include "dr_types.h"

// functions
extern void MotelDisableZ(void);
extern void MotelEnableZ(void);
extern void MyDrawIcon(float x, float y, TEX_INFO *tinfo, u_char r, u_char g, u_char b, char mirror, char blend);
extern void MyDrawSlider(float x, float y, float value, float max, TEX_INFO *tinfo, u_char r, u_char g, u_char b, char blend);

#endif // FLAT_FUDGE_H
