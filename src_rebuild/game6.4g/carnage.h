/*
 * carnage.h - globals and functions of carnage.c used by other files
 */

#ifndef CARNAGE_H
#define CARNAGE_H

#include "dr_types.h"

// globals
// NOTE: no IDA type
// most references: carnage.c 8, overlay.c 1
extern int carnage_score;
// most references: bcollide.c 4, carnage.c 3, smashable.c 1
extern int carnage_flags;
// most references: bcollide.c 8, carnage.c 2
extern int new_damage;

// functions
extern int InitCarnage(void);
extern int ReportCarnage(int type);

#endif // CARNAGE_H
