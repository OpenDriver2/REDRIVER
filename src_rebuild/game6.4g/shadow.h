/*
 * shadow.h - globals and functions of shadow.c used by other files
 */

#ifndef SHADOW_H
#define SHADOW_H

#include "dr_types.h"

// globals
// most references: frontend/config.c 6, frontend/details.c 5, pause.c 5
extern int gDoTexturedTyreTracks;
// most references: frontend/config.c 6, frontend/details.c 4, pause.c 4
extern int gDoTyreTracks;

// functions
extern void AddOtherTyreTrack(int wheel, VECTOR4 *pos);
extern void AddTyreTrack(int wheel);
extern void CheckAddGrass(void);
extern void DrawOtherTyreTracks(void);
extern void DrawShadow(int id);
extern void DrawTyreTracks(void);
extern void GetCarTyreTrackPositions(CAR_DATA *cp, VECTOR4 *pos);
extern void GetTyreTrackPositions(void);
extern void InitShadow(void);
extern void InitTyreTracks(void);
extern void SetOtherTyreTrackOldPositions(VECTOR4 *pos);
extern void SetTyreTrackOldPositions(void);

#endif // SHADOW_H
