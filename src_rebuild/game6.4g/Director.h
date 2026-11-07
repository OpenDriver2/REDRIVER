/*
 * Director.h - globals and functions of Director.c used by other files
 */

#ifndef DIRECTOR_H
#define DIRECTOR_H

#include "dr_types.h"

// Director.Mode (DIRECTOR_MODE in IDA)
#define DM_EXIT				0
#define DM_PLAY				1		// the replay menu, paused
#define DM_REPLAYEDITOR		2
#define DM_AUTOCAMERA		3
#define DM_PLAYPAUSE		4
#define DM_FRAMEADVANCE		5
#define DM_PLAYCAMERA		6
#define DM_DELETE			7
#define DM_MOVECAMERA		8
#define DM_EDITCAMERA		9
#define DM_ROTATECAMERA		10
#define DM_AUTODIRECTOR		11
#define DM_DEBOUNCE			12
#define DM_PLAYREPLAY		13
#define DM_UNKNOWN			14
#define DM_PLAYREPLAY_2		15

// DIRECTOR_TEXT.index, the replay menu buttons (DIRECTOR_BUTTON in IDA)
#define DT_PLAYPAUSE			1
#define DT_AUTOCAM				2
#define DT_PLAYCAM				3
#define DT_FRAME_ADVANCE		4
#define DT_RESTART				5
#define DT_ADDCAM				6
#define DT_EDITCAM				7
#define DT_EXIT					8
#define DT_ADD_INCAR			9
#define DT_ADD_CHASE			10
#define DT_ADD_FIXED			11
#define DT_ADD_OK				12
#define DT_EDIT_INCAR			13
#define DT_EDIT_CHASE			14
#define DT_EDIT_FIXED			15
#define DT_EDIT_CLOCK			16
#define DT_EDIT_DELETE			17
#define DT_EDIT_OK				18
#define DT_ADD_FIXED_MOVE		19
#define DT_ADD_FIXED_LOOK		20
#define DT_ADD_FIXED_MOVECMP	21
#define DT_ADD_FIXED_LENSCHA	22
#define DT_EDIT_FIXED_MOVE		23
#define DT_EDIT_FIXED_LOOK		24
#define DT_EDIT_FIXED_MOVECMP	25
#define DT_EDIT_FIXED_LENSCHA	26
#define DT_UNKNOWN				27		// the PAUSE button of DirectorPauseMenu
#define DT_ADD_INCAR_CHOOSECAR	28
#define DT_EDIT_INCAR_CHOOSECAR	29
#define DT_ADD_CHASE_CHOOSECAR	30
#define DT_ADD_CHASE_MOVECAM	31
#define DT_ADD_UNKNOWN			32
#define DT_EDIT_UNKNOWN			33
#define DT_EDIT_CHASE_CHOOSECAR	34
#define DT_EDIT_CHASE_MOVECAM	35

#define DT_END					0xFFFF	// DIRECTOR_TEXT.index of the menu terminator

// DIRECTOR_TEXT.flags
// NOTE: names guessed from how ReplayMenuHandler and DrawDirectorText use them
#define DTF_TEXT_RIGHT			0x1		// text beside the icon, not under it
#define DTF_DISABLED			0x4
#define DTF_SIDE_SUBMENU		0x8		// next is a submenu entered with select, not with right
#define DTF_DOWN_SUBMENU		0x10	// extra is a vertical submenu, entered with select

// globals
extern int bWaypointsStarted;
extern int iNumWaypoints;
extern DIRECTOR Director;

// functions
extern DIRECTOR_CAMERA *AddDirectorCamera(DIRECTOR_CAMERA *camera, int frame_index);
extern int ApplyDirectorCollision(int x, int z);
extern int CheckDirectorCameraCollide(CELL_OBJECT *cop);
extern void DestroyCameras(void);
extern void DestroyWaypoints(void);
extern void DrawReplayInterface(void);
extern void HandleDirector(void);
extern void InitialiseDirector(void);
extern void InitialiseDirectorMenus(void);
extern void ResetRoysVariables(void);
extern int SetDirectorMode(int mode, int selection, int debug);
extern void ShutdownDirector(void);

#endif // DIRECTOR_H
