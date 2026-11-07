/*
 * flat/frontend.h - globals and functions of flat/frontend.c used by other files
 */

#ifndef FLAT_FRONTEND_H
#define FLAT_FRONTEND_H

#include "dr_types.h"

// globals
extern int flat_retcode;
extern float motel_xscale;
extern float motel_yscale;
extern float motel_xoffset;
extern int motel_font_override;
extern float gMotelFadeIntensity;
extern int gDoingCarSelect;

// functions
extern void DrawMotel(FE_TEXT *texts, void (*drawicons)(void));
extern void FadeInMotel(FE_TEXT *texts, void (*drawicons)(void));
extern void FadeOutMotel(FE_TEXT *texts, void (*drawicons)(void));
extern void InitFlat(void);
extern int MotelMenuKeyControl(FE_BUTTON *buttons, int selection);
extern void PlayMotelClickSound(void);
extern void QuitFlat(void);
extern void ResetMotelMenu(FE_BUTTON *buttons, int selection, FE_TEXT *texts);
extern void SetMotelIntensity(float intensity);
extern void UpdateMotelRegion(void);

#endif // FLAT_FRONTEND_H
