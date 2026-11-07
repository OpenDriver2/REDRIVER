/*
 * frontend/frontend.h - globals and functions of frontend/frontend.c used by other files
 */

#ifndef FRONTEND_FRONTEND_H
#define FRONTEND_FRONTEND_H

#include "dr_types.h"

// globals
extern int WantedGameMode;
extern int on_slider;
extern int exit_flag;
extern int wanted_exit_flag;
extern int gInFrondend;
extern int gbPlayingDemo;
extern FE_POLY *fe_cur_polys;
extern int fe_palette2[256];

// functions
extern void ClearFullDisplay(int count);
extern void CloseFrontendScreen(void);
extern void DrawFrontend(void);
extern void FE_PlayClickSound(void);
extern void FE_PlayErrorSound(void);
extern void FadeInHiresScreen(char *filename, int wait);
extern void FadeInScreen(const char *filename, int seconds);
extern int HandleMenu(FE_BUTTON *buttons, int *selection);
extern int InitFrontend(void);
extern void OpenFrontendScreen(FE_POLY *polys, FE_TEXT *texts);
extern void QuitFrontend(void);
extern void ResetMenu(FE_BUTTON *buttons, int selection);
extern void StartGame(int do_credits);
extern void StartGameFromFrontend(int do_gamemode, int do_credits);
extern void StartMissionFromFrontend(int load_game);

// NOTE: frontend._backcol in IDA (a static), but main.c's SetupQuit greys it out
extern RGBA backcol;

#endif // FRONTEND_FRONTEND_H
