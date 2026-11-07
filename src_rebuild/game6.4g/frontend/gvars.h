/*
 * frontend/gvars.h - globals and functions of frontend/gvars.c used by other files
 */

#ifndef FRONTEND_GVARS_H
#define FRONTEND_GVARS_H

#include "dr_types.h"

// globals
// NOTE: IDA mark @frontend\gvars.c;extern
extern DISPLAY_MODE display_modes[8];
// NOTE: IDA mark @frontend\gvars.c;extern
extern CONTROL_KEY_SET DefaultKeySet;
// NOTE: IDA mark @frontend\gvars.c;extern
extern CONTROL_KEY_SET UserKeys;
// NOTE: IDA mark @frontend\gvars.c;extern
extern CONTROL_JOY_SET DefaultJoySet;
extern int gUserLanguage;

// functions
extern int GetASCIIInput(void);
extern int GetJoyKeyInput(void);
extern char *GetJoyKeyName(u_char button);
extern int GetKeyInput(void);
extern char *GetKeyName(u_char key);
extern void ReadKeyTextFile(void);

#endif // FRONTEND_GVARS_H
