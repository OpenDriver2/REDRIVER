/*
 * mirror.h - globals and functions of mirror.c used by other files
 */

#ifndef MIRROR_H
#define MIRROR_H

#include "dr_types.h"

// globals
extern int gDoRearViewMirror;

// functions
extern void CalculateMirrorViewArc(float radius);
extern void CalculateNormalViewArc(void);
extern void DrawRearViewMirror(void);
extern void KillMirrorViewArc(void);
extern void PackViewArc(short *arc, char *grid);

extern short (*normal_view_arc)[64];

#endif // MIRROR_H
