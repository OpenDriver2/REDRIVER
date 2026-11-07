/*
 * overlay.h - globals and functions of overlay.c used by other files
 */

#ifndef OVERLAY_H
#define OVERLAY_H

#include "dr_types.h"

// globals
// most references: overlay.c 15, glaunch.c 6
extern int map_data;
extern int map_bmp_height;
extern PERCENTAGE_BAR OpponentBar;
// NOTE: no IDA type
extern int gNumLaps;
// NOTE: no IDA type
extern int gLapCount;

// functions
extern void DisplayOverlays(void);
extern void DrawFullscreenMap(void);
extern void DrawGameSpecificOverlays(void);
extern void DrawTargetArrows(void);
extern void DrawTargetShadows(void);
extern void HandleOverlays(void);
extern void InitLapTimes(void);
extern void InitOverlays(void);
extern void InitPercentageBar(PERCENTAGE_BAR *bar, float size, int color, char *text, int rightSide, int secondLine);
extern void LoadLevelMap(void);
extern int TimeToInt(TIMER *time);
extern int UpdateBestLap(void);

#endif // OVERLAY_H
