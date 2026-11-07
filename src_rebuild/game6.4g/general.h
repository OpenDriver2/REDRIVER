/*
 * general.h - globals and functions of general.c used by other files
 */

#ifndef GENERAL_H
#define GENERAL_H

#include "dr_types.h"

// globals
extern int ifps;
extern int NextSyncFrame;
extern MODEL *gDamWheelModelPtr;
extern MODEL *gFastWheelModelPtr;
extern MODEL *gCleanWheelModelPtr;
extern int gTicksTillKeysActive;
extern int accumg;
extern int accumf;
extern MODEL *gHubcapModelPtr;

// functions
extern void AddTextToList(char *text);
extern int AnyKeyPressed(void);
extern void CheckMiscKeys(void);
extern void CloseMemDebug(void);
extern void DebugFree(void *mem);
extern void *DebugMalloc(size_t size, const char *filename, int line);
extern void DisplayCurrentTime(void);
extern void DrawGradientBar(float x, float y, float w, float h, u_char r1, u_char g1, u_char b1, u_char r2, u_char g2, u_char b2, u_char r3, u_char g3, u_char b3, u_char r4, u_char g4, u_char b4);
extern void DrawQuad(float x, float y, float w, float h, int color);
extern void DrawQuadTC(float x, float y, float w, float h, int color);
extern void DrawScaledGouraudBar(float x, float y, float w, float h, int color1, int color2, int color3, int color4);
extern void DrawTexturedQuad(float x, float y, float w, float h, TEX_INFO *texture, u_char r, u_char g, u_char b, char type);
extern char DriverFRead(void *dst, int size, int count, FILE *file);
extern int GetCarHeading(short dir);
extern void InitMemDebug(void);
extern void InitModelNames(void);
extern int Random(void);
extern void RandomInit(int i1, int i2);
extern void SaveScreenShot(void);
extern void StartLoadingBarNormal(int bytesToRead, int xpos, int ypos, int width, int height);
extern void StopLoadingBar(void);
extern void TextColour(short r, short g, short b);
extern void TextOffset(int x, int y);
extern void TextReset(void);
extern int WaitForSync(int delta);
extern void WriteTextFromList(void);

#endif // GENERAL_H
