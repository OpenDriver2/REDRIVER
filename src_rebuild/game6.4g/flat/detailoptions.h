/*
 * flat/detailoptions.h - globals and functions of flat/detailoptions.c used by other files
 */

#ifndef FLAT_DETAILOPTIONS_H
#define FLAT_DETAILOPTIONS_H

#include "dr_types.h"

// functions
extern int DoMotelDetailOptions(void);
extern void DrawMotelDetailOptionsIcons(float peds, float view, float mirror, float traffic, float night, int selected);

#endif // FLAT_DETAILOPTIONS_H
