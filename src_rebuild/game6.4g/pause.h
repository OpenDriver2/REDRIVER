/*
 * pause.h - globals and functions of pause.c used by other files
 */

#ifndef PAUSE_H
#define PAUSE_H

#include "dr_types.h"

// globals
// most references: debris.c 12, effects.c 9, camera.c 7
extern int GamePaused;
// most references: pause.c 5, glaunch.c 2
extern int gbFullscreenMap;

// functions
extern void CheckPause(void);
extern void PauseGame(int pause);
extern void PauseKeyControl(void);
extern void RenderInput(void);

#endif // PAUSE_H
