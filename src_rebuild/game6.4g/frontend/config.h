/*
 * frontend/config.h - globals and functions of frontend/config.c used by other files
 */

#ifndef FRONTEND_CONFIG_H
#define FRONTEND_CONFIG_H

#include "dr_types.h"

// globals
extern int wanted_display_width;
extern int wanted_display_height;
extern int forced_display_width;
extern int forced_display_height;
extern int wanted_display_depth;
extern int wanted_display_device;
extern int gDisableConfig;
extern GAME_DATA game_data;
extern CONFIG_DATA config_data;

// functions
extern int CheckReplay(int slot, char *name);
extern int CheckSaveGame(int slot, char *name);
extern int LoadSavedGame(int slot);
extern int LoadSavedOptions(void);
extern int LoadSavedReplay(int slot);
extern void LoadSavedScores(void);
extern void ResetGameVariables(void);
extern void SetGraphicsLevel(int level, int keepDensity);
extern void UpdateSavedGame(int slot, char *name);
extern void UpdateSavedOptions(void);
extern int UpdateSavedReplay(int slot, char *name);
extern void UpdateSavedScores(void);

#endif // FRONTEND_CONFIG_H
