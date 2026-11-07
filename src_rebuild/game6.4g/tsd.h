/*
 * tsd.h - globals and functions of tsd.c used by other files
 */

#ifndef TSD_H
#define TSD_H

#include "dr_types.h"

// functions
extern void COPY8TO16(u_char *src, void *dst, int width, int height);
extern void COPY8TO32(u_char *src, void *dst, int width, int height);
extern void DRAW8CHROMA(u_char *tpage, u_char *screen, int w, int h, u_char u, u_char v);
extern void DRAW8TRANS(u_char *tpage, u_char *screen, int w, int h, u_char u, u_char v);
extern void FILL8(u_char *screen, int w, int h, u_char colour);

#endif // TSD_H
