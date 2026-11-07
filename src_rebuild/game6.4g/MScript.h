/*
 * MScript.h - globals and functions of MScript.c used by other files
 */

#ifndef MSCRIPT_H
#define MSCRIPT_H

#include "dr_types.h"

// globals
extern DMSVariable DmsVars[];
extern int ChaseCarPalette;
extern int ChaseCarType;
extern int gChaseModeHits;
extern int gDoChopper;
extern int gChaseModeHitsRequired;
extern int gPlayerForceDamage;
extern char gChaseOverride;
// most references: MScript.c 11, glaunch.c 2
extern TRAIL_DATA *pTrailData;
extern MISSION_TARGET MissionTargets[10];
extern int giDemoScript;
extern VECTOR4 ReplayCarEndPosition;
extern int MissionLampsStatus[5];
extern char overhead_map[256];
extern int MaxPlayerDamage;
// most references: MScript.c 3, pause.c 2, smashable.c 2
extern MISSION mission;
extern MISSION_DATA MLast;
extern int MissionQuickRender[2];
extern int giCurrentMissionNumber;
extern int MissionNotSoQuickRender[2];
extern MISSION_DATA MStart;
extern MISSION_DATA MNext;
extern int gDoCrates;
extern LAMP_OUTPUT MissionLamps[5];
extern int gTryItScript;
extern int gNextScript;
extern int ActivateChopper;
extern int gLoseTailSounds[4];
extern CHECKPOINT_DATA Checkpoint;

// functions
extern void AddTimeBonus(MISSION *mission, int seconds);
extern void ControlSaveTrail(void);
extern void DMSHandleTimer(TIMER *timer);
extern void DMSInitTimerFrames(TIMER *timer, int frames);
extern void DestroyMissionScript(void);
extern void DisplayMissionData(void);
extern void DisplayTimer(TIMER *timer);
extern void DrawMissionText(char *text);
extern void DrawTrailblazer(TRAIL_DATA *trail);
extern void ExitSaveTrail(void);
extern void ExitTestTrail(void);
extern int GetVariableIndex(char *name);
extern void HandleTrailBlazer(TRAIL_DATA *trail);
extern void InitSaveTrail(void);
extern int InitTestTrail(void);
extern int LoadMission(int mission_number);
extern void MissionHandler(void);
extern int NearestTargetDistance(void);

#endif // MSCRIPT_H
