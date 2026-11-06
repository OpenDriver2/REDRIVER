/*
 * Director.c - the replay director: cameras, waypoints, the replay menu
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "Director.c").
 *
 * After a game the player can watch the replay and edit its cameras ("film director"). Director.Mode
 * (DM_*) says what the director does each frame (HandleDirector): the paused menu, playing the replay
 * with the stored cameras, the automatic director (CreateNewAutoCamera at the recorded waypoints), or
 * moving, rotating and retiming the work camera. The cameras are a sorted list of DIRECTOR_CAMERA
 * keyed by the frame they start at (Director.CameraList); DirectorStorage.c saves and loads them.
 */

#include "dr_keys.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "dr_types.h"
#include "Director.h"
#include "DirectorStorage.h"
#include "dr_globals.h"
#include "ai.h"
#include "asmmath.h"
#include "camera.h"
#include "debug.h"
#include "denting.h"
#include "general.h"
#include "handling.h"
#include "input.h"
#include "interview.h"
#include "map.h"
#include "minicutscene.h"
#include "pause.h"
#include "pres.h"
#include "replays.h"
#include "roadbits.h"
#include "shell.h"
#include "dr_time.h"
#include "sound.h"
#include "testmath.h"
#include "frontend/font.h"
#include "frontend/frontend.h"
#include "Engine/3dfx/gfxinit.h"
#include "Engine/3dfx/object.h"
#include "Engine/3dfx/polydraw.h"
#include "glaunch.h"

// ReplayDebounce pad bits
#define DPAD_LEFT			0x1
#define DPAD_RIGHT			0x2
#define DPAD_UP				0x4
#define DPAD_DOWN			0x8
#define DPAD_SELECT			0x10
#define DPAD_LOWER			0x20	// numpad minus: camera down
#define DPAD_RAISE			0x40	// numpad plus: camera up
#define DPAD_FAST			0x80	// shift
#define DPAD_LEVEL			0x100	// end: level the camera

#define TEMP_CAMERA_FILE	"REPLAYDATA\\USER\\$TEMP$.CAMS"

// the menu grid: an icon is 45 pixels, 53 apart, from 16 16
#define ICON_POS(n)			(53 * (n) + 16)
#define ICON_SIZE			45

#define WAYPOINT_FRAMES		150		// frames between the recorded waypoints

void PositionDirectorCamera(int padd);
void RotateDirectorCamera(int padd);
void MoveDirectorCamera(int padd);
void HandleCameraPad(int padd);
int UpdateCameraPosition(DIRECTOR_CAMERA *camera);
void CheckCameraDistance(DIRECTOR_CAMERA *camera);
void UpdateCameraHeight(DIRECTOR_CAMERA *camera);
void InitialiseDirectorMenu(DIRECTOR_TEXT *menu);
WAYPOINT *FindWaypoint(int frame);
WAYPOINT *FindNextWaypoint(int frame);
void DumpWaypoints(void);
void ReplayMenuHandler(int padd);
void DrawReplayMenu(void);
void DrawReplaySubMenu(DIRECTOR_TEXT *menu);
void DrawReplaySubMenuText(DIRECTOR_TEXT *menu);
void DrawDirectorIcon(DIRECTOR_TEXT *item);
void DrawDirectorText(DIRECTOR_TEXT *item);
BOOL EnableDirectorItem(int index, int enabled);
int ReplayDebounce(void);
DIRECTOR_TEXT *GetDirectorItem(int index);
DIRECTOR_TEXT *sub_4C5965(int index);
DIRECTOR_TEXT *GetReplaySubMenu(int index);
int sub_4C59ED(int x, int y);
void DisplayDemonstration(void);
void HandleMenuCommand(int selection);
void AddCamera(int selection);
void ChangeCamera(int selection);
void AddTripodCamera(int selection);
void ChangeTripodCamera(int selection);
void InitialiseEnabledItems(void);
void AutoDirector(void);
void PlayDirectorReplay(void);
void ReplayEditor(void);
void SetCurrentCamera(DIRECTOR_CAMERA *camera);
void LockCameraToCar(DIRECTOR_CAMERA *camera);
void GetCurrentCamera(DIRECTOR_CAMERA *camera);
void CameraDump(DIRECTOR_CAMERA *camera);
const char *GetCameraTypeName(int type);
void CreateNewAutoCamera(DIRECTOR_CAMERA *camera);
int NextPursuerInView(int id);
int PursuersInView(void);
int sub_4C7758(DIRECTOR_CAMERA *camera);
int valid_region(int x, int z);
void SetCamera(DIRECTOR_CAMERA *camera, int index);
DIRECTOR_CAMERA *GetCameraByIndex(int index);
DIRECTOR_CAMERA *GetCameraByFrameIndex(int frame_index);
DIRECTOR_CAMERA *GetLastCameraByFrameIndex(int frame_index);
void ReInitDirectorCameras(void);
DIRECTOR_CAMERA *InsertCamera(DIRECTOR_CAMERA *camera, int index);
void DeleteCamera(int index);
DIRECTOR_CAMERA *FindNextCamera(int frame_index);
int sub_4C8065(DIRECTOR_CAMERA *camera, int frame_index);
int CheckForNewCamera(DIRECTOR_CAMERA **ppCamera);
void CheckValidTargetCar(DIRECTOR_CAMERA *camera);
void DrawCameraBar(int FrameIndex);
void MakeChaseCamera(DIRECTOR_CAMERA *camera);
int GetDirectorMode(void);
void StartDirectorModify(void);
void FinishDirectorModify(void);
void CancelDirectorModify(void);

//------------------------------------------------------------------------------------------
// initialised data

// [0x561318]
DIRECTOR_TEXT DirectorPauseMenu[] =
{
	{ 249, "PAUSE", 0, 0, 0, 0, 0, 0, 0, NULL, DT_UNKNOWN },
	// NOTE: the terminators' icons point at an empty bss byte in the original
	{ -1, "", 0, 0, 0, 0, 0, 0, 0, NULL, DT_END },
};

// [0x561370]
DIRECTOR_TEXT DirectorMainMenu[] =
{
	//
	// Main menu
	//
	{ 250, "PLAYPAU",   0,  0, 0, DTF_SIDE_SUBMENU,                0,  2,  0, NULL, DT_PLAYPAUSE },
	{ 251, "AUTO",      0,  1, 0, 0,                               0,  3,  0, NULL, DT_AUTOCAM },
	{ 252, "PLAYCAM",   0,  2, 0, 0,                               0,  4,  0, NULL, DT_PLAYCAM },
	{ 253, "FRAMEADV",  0,  3, 0, 0,                               0,  5,  0, NULL, DT_FRAME_ADVANCE },
	{ 254, "RESTREP",   0,  4, 0, 0,                               0,  6,  0, NULL, DT_RESTART },
	{ 255, "ADDCAM",    0,  5, 0, DTF_DOWN_SUBMENU,                0,  7,  9, NULL, DT_ADDCAM },
	{ 256, "EDITCAM",   0,  6, 0, DTF_DOWN_SUBMENU,                0,  8, 13, NULL, DT_EDITCAM },
	{ 257, "OK",        0,  7, 0, 0,                               0,  0,  0, NULL, DT_EXIT },
	//
	// Submenus
	//
	// Add camera
	{ 258, "INCAR",     0,  5, 1, DTF_TEXT_RIGHT | DTF_SIDE_SUBMENU, 6, 28, 10, NULL, DT_ADD_INCAR },
	{ 259, "CHASEC",    0,  5, 2, DTF_TEXT_RIGHT | DTF_SIDE_SUBMENU, 6, 30, 11, NULL, DT_ADD_CHASE },
	{ 260, "FIXEDCA",   0,  5, 3, DTF_TEXT_RIGHT | DTF_SIDE_SUBMENU, 6, 19, 12, NULL, DT_ADD_FIXED },
	{ 261, "OK",        0,  5, 4, DTF_TEXT_RIGHT,                  6,  0,  0, NULL, DT_ADD_OK },
	// Edit camera
	{ 258, "INCAR",     0,  6, 1, DTF_TEXT_RIGHT | DTF_SIDE_SUBMENU, 7, 29, 14, NULL, DT_EDIT_INCAR },
	{ 259, "CHASEC",    0,  6, 2, DTF_TEXT_RIGHT | DTF_SIDE_SUBMENU, 7, 34, 15, NULL, DT_EDIT_CHASE },
	{ 260, "FIXEDCA",   0,  6, 3, DTF_TEXT_RIGHT | DTF_SIDE_SUBMENU, 7, 23, 16, NULL, DT_EDIT_FIXED },
	{ 262, "CLOCK",     0,  6, 4, DTF_TEXT_RIGHT | DTF_DISABLED,   7,  0, 17, NULL, DT_EDIT_CLOCK },
	{ 263, "DELCAM",    0,  6, 5, DTF_TEXT_RIGHT | DTF_DISABLED,   7,  0, 18, NULL, DT_EDIT_DELETE },
	{ 264, "OK",        0,  6, 6, DTF_TEXT_RIGHT,                  7,  0,  0, NULL, DT_EDIT_OK },
	// Add fixed camera
	{ 265, "MOVECAM",   0,  6, 3, 0,                              11, 20,  0, NULL, DT_ADD_FIXED_MOVE },
	{ 266, "LOOKCAR",   0,  7, 3, 0,                              11, 21,  0, NULL, DT_ADD_FIXED_LOOK },
	{ 267, "MOVECMP",   0,  8, 3, 0,                              11, 22,  0, NULL, DT_ADD_FIXED_MOVECMP },
	{ 268, "LENSCHA",   0,  9, 3, DTF_DISABLED,                   11,  0,  0, NULL, DT_ADD_FIXED_LENSCHA },
	// Edit fixed camera
	{ 265, "MOVECAM",   0,  7, 3, 0,                              15, 24,  0, NULL, DT_EDIT_FIXED_MOVE },
	{ 266, "LOOKCAR",   0,  8, 3, 0,                              15, 25,  0, NULL, DT_EDIT_FIXED_LOOK },
	{ 267, "MOVECMP",   0,  9, 3, 0,                              15, 26,  0, NULL, DT_EDIT_FIXED_MOVECMP },
	{ 268, "LENSCHA",   0, 10, 3, DTF_DISABLED,                   15,  0,  0, NULL, DT_EDIT_FIXED_LENSCHA },
	// In car: choose the car
	{ 269, "CHOOSECA",  0,  6, 1, 0,                               9,  0,  0, NULL, DT_ADD_INCAR_CHOOSECAR },
	{ 269, "CHOOSECA",  0,  7, 1, 0,                              13,  0,  0, NULL, DT_EDIT_INCAR_CHOOSECAR },
	// Chase camera: choose the car, move the camera
	{ 269, "CHOOSECAR", 0,  6, 2, 0,                              10, 31,  0, NULL, DT_ADD_CHASE_CHOOSECAR },
	{ 265, "MOVECAM",   0,  7, 2, 0,                              10, 32,  0, NULL, DT_ADD_CHASE_MOVECAM },
	{ 269, "CHOOSECAR", 0,  7, 2, 0,                              14, 35,  0, NULL, DT_EDIT_CHASE_CHOOSECAR },
	{ 265, "MOVECAM",   0,  8, 2, 0,                              14,  0,  0, NULL, DT_EDIT_CHASE_MOVECAM },

	{ -1, "", 0, 0, 0, 0, 0, 0, 0, NULL, DT_END },
};

//------------------------------------------------------------------------------------------
// uninitialised data

int bWaypointsStarted;
int iNumWaypoints;
DIRECTOR Director;

// NOTE: unmarked bss right after Director, used only here
static int bOverlaysDisabled;

char gAICarDamageTexture[1800];

// [0x4c3802] [D]
// Clears the car model and denting state (denting.c) before InitialiseDenting.
void ResetRoysVariables(void)
{
	ClearMem(gCarUnitDamage, sizeof(gCarUnitDamage));
	ClearMem(gTempCarVertDump, sizeof(gTempCarVertDump));
	ClearMem(gTempCarNormalDump, sizeof(gTempCarNormalDump));
	ClearMem(gTempCarPolyDump, sizeof(gTempCarPolyDump));
	ClearMem(gCarCleanModelPtr, sizeof(gCarCleanModelPtr));
	ClearMem(gCarDamModelPtr, sizeof(gCarDamModelPtr));
	ClearMem(gCarLowModelPtr, sizeof(gCarLowModelPtr));
	ClearMem(gTempCarPolyDumpSize, sizeof(gTempCarPolyDumpSize));
	ClearMem(gZoneVertDamageDump, sizeof(gZoneVertDamageDump));
	ClearMem(gCarMaxDamage, 68 * sizeof(int));
	ClearMem(&gHubcap, sizeof(gHubcap));
	ClearMem(CalcHubcapTime, sizeof(CalcHubcapTime));
	ClearMem(DentQueue, sizeof(DentQueue));
	ClearMem(gAICarDamageTexture, sizeof(gAICarDamageTexture));
	ClearMem(gOldCarType, sizeof(gOldCarType));

	gHubcapTime = 0;
	CalcHubcapNum = 0;
	NumCarsToDent = 0;
}

// [0x4c4270] [D]
// DM_EDITCAMERA: left and right move the start frame of the current camera (never past the
// current frame); select returns to the menu.
void PositionDirectorCamera(int padd)
{
	DIRECTOR_CAMERA *camera;
	DIRECTOR_CAMERA *prev;

	if (padd & DPAD_SELECT)
	{
		if (Director.Moving == 0)
		{
			SetDirectorMode(DM_DEBOUNCE, Director.Selection, 0);
			return;
		}
	}
	else
	{
		Director.Moving = 0;
	}

	camera = GetLastCameraByFrameIndex(Director.FrameIndex);

	if (camera)
	{
		if (camera->frame_index > 0 && (padd & DPAD_LEFT))
		{
			prev = GetCameraByFrameIndex(camera->frame_index - 1);

			if (prev)
				DeleteCamera(prev->index);

			camera->frame_index--;
		}

		if ((padd & DPAD_RIGHT) && camera->frame_index < Director.FrameIndex)
			camera->frame_index++;
	}
	else
	{
		Director.Moving = 0;

		SetDirectorMode(DM_DEBOUNCE, Director.Selection, 0);
	}
}

// [0x4c435a] [D]
// DM_ROTATECAMERA: turns the static work camera (pitch limited to straight up or down), plus and
// minus raise and lower it, end levels it.
void RotateDirectorCamera(int padd)
{
	SVECTOR old_angle;

	if (padd & DPAD_SELECT)
	{
		if (Director.Moving == 0)
		{
			SetDirectorMode(DM_DEBOUNCE, Director.Selection, 0);
			return;
		}
	}
	else
	{
		Director.Moving = 0;
	}

	old_angle = Director.WorkCamera.angle;		// [unused]

	if (padd & DPAD_LEFT)
		Director.WorkCamera.angle.vy += 20;
	if (padd & DPAD_RIGHT)
		Director.WorkCamera.angle.vy -= 20;
	if (padd & DPAD_UP)
		Director.WorkCamera.angle.vx += 20;
	if (padd & DPAD_DOWN)
		Director.WorkCamera.angle.vx -= 20;
	if (padd & DPAD_LEVEL)
		Director.WorkCamera.angle.vx = 0;
	if (padd & DPAD_LOWER)
		Director.WorkCamera.holding_position[1] -= 40;
	if (padd & DPAD_RAISE)
		Director.WorkCamera.holding_position[1] += 40;

	Director.WorkCamera.angle.vx &= 0xfff;
	Director.WorkCamera.angle.vy &= 0xfff;

	if (Director.WorkCamera.angle.vx >= 1024 && Director.WorkCamera.angle.vx < 2048)
		Director.WorkCamera.angle.vx = 1024;
	if (Director.WorkCamera.angle.vx >= 2048 && Director.WorkCamera.angle.vx < 3072)
		Director.WorkCamera.angle.vx = 3072;

	Director.WorkCamera.yang = Director.WorkCamera.angle.vy;
	Director.WorkCamera.turnAngle = Director.WorkCamera.yang;
	Director.WorkCamera.xang = Director.WorkCamera.angle.vx;

	UpdateCameraHeight(&Director.WorkCamera);
}

// [0x4c4533] [D]
// DM_MOVECAMERA: moves the work camera, a chase camera around its car (HandleCameraPad), any other
// along the view (shift moves faster); a move into scenery is undone.
void MoveDirectorCamera(int padd)
{
	float x;
	float y;
	float z;
	float step;

	if (padd & DPAD_SELECT)
	{
		if (Director.Moving == 0)
		{
			SetDirectorMode(DM_DEBOUNCE, Director.Selection, 0);
			return;
		}
	}
	else
	{
		Director.Moving = 0;
	}

	if (Director.WorkCamera.type != CAMERA_FOLLOW)
	{
		x = (float)Director.WorkCamera.position[0];
		z = (float)Director.WorkCamera.position[2];

		if (padd & DPAD_FAST)
			step = 80.0f;
		else
			step = 20.0f;

		// NOTE: the original also reads position[1] here into an unused variable
		if (padd & DPAD_UP)
		{
			x = step * inv_camera_matrix.m[0][2] + x;
			z = step * inv_camera_matrix.m[2][2] + z;
		}

		if (padd & DPAD_DOWN)
		{
			x = x - step * inv_camera_matrix.m[0][2];
			z = z - step * inv_camera_matrix.m[2][2];
		}

		if (padd & DPAD_LEFT)
		{
			x = x - step * inv_camera_matrix.m[0][0];
			z = z - step * inv_camera_matrix.m[2][0];
		}

		if (padd & DPAD_RIGHT)
		{
			x = step * inv_camera_matrix.m[0][0] + x;
			z = step * inv_camera_matrix.m[2][0] + z;
		}

		y = (float)Director.WorkCamera.position[1];

		if (padd & DPAD_LOWER)
			y = y - step;

		if (padd & DPAD_RAISE)
			y = y + step;

		Director.WorkCamera.position[0] = float_to_int(x);
		Director.WorkCamera.position[1] = float_to_int(y);
		Director.WorkCamera.position[2] = float_to_int(z);
		Director.WorkCamera.holding_position[0] = float_to_int(x);
		Director.WorkCamera.holding_position[1] = float_to_int(y);
		Director.WorkCamera.holding_position[2] = float_to_int(z);

		if (UpdateCameraPosition(&Director.WorkCamera)
			&& (UpdateCameraHeight(&Director.WorkCamera), CameraMoveAllowed(x, y, z)))
		{
			Director.Camera = &Director.WorkCamera;
		}
		else
		{
			GetCurrentCamera(&Director.WorkCamera);
		}
	}
	else
	{
		HandleCameraPad(padd);
		UpdateCameraHeight(&Director.WorkCamera);

		x = (float)Director.WorkCamera.position[0];
		y = (float)Director.WorkCamera.position[1];
		z = (float)Director.WorkCamera.position[2];

		if (!CameraMoveAllowed(x, y, z))
			GetCurrentCamera(&Director.WorkCamera);

		SetCurrentCamera(&Director.WorkCamera);
	}
}

// [0x4c489d] [D]
// Moves the chase work camera around its target car: left and right turn around it, up and down
// change the distance (20 to 15000), plus and minus the height.
void HandleCameraPad(int padd)
{
	DIRECTOR_CAMERA *camera;
	CAR_DATA *cp;
	MATRIX mat;
	VECTOR4 offset;
	int step;
	int angle;

	camera = &Director.WorkCamera;

	if (Director.WorkCamera.target == -1)
		camera->target = 0;

	if (padd & DPAD_FAST)
		step = 80;
	else
		step = 40;

	cp = &car_data[camera->target];
	angle = (camera->turnAngle + cp->direction) & 0xfff;

	if (padd & DPAD_LEFT)
		angle += step / 8;

	if (padd & DPAD_RIGHT)
		angle -= step / 8;

	if (padd & DPAD_LOWER)
		camera->camerapos[1] -= step;

	if (padd & DPAD_RAISE)
		camera->camerapos[1] += step;

	if (padd & DPAD_UP)
		camera->distance -= step;

	if (padd & DPAD_DOWN)
		camera->distance += step;

	if (camera->distance < 20)
		camera->distance = 20;

	if (camera->distance >= 15000)
		camera->distance = 15000;

	angle &= 0xfff;

	offset.vx = 0.0f;
	offset.vy = (float)camera->camerapos[1];
	offset.vz = (float)camera->distance;

	mat.m[0][0] = 1.0f;
	mat.m[1][0] = 0.0f;
	mat.m[2][0] = 0.0f;
	mat.m[0][1] = 0.0f;
	mat.m[0][2] = 0.0f;
	mat.m[1][1] = 1.0f;
	mat.m[2][1] = 0.0f;
	mat.m[1][2] = 0.0f;
	mat.m[2][2] = 1.0f;

	RotMatrixY(&mat, -angle & 0xfff);

	CurrentRotationMatrix = &mat;
	MatrixRotate(&offset);

	camera->position[0] = float_to_int(offset.vx) + cp->location.vx;
	camera->position[1] = float_to_int(offset.vy) - cp->location.vy;
	camera->position[2] = float_to_int(offset.vz) + cp->location.vz;
	camera->max_distance = camera->distance;
	camera->turnAngle = (angle - cp->direction) & 0xfff;
	camera->yang = angle & 0xfff;
}

// [0x4c4b1e] [D]
// Keeps a camera within 15000 of the player: a chase camera gets the longest distance, a fixed one
// is pulled in along its direction to the player. Always returns 1.
int UpdateCameraPosition(DIRECTOR_CAMERA *camera)
{
	int dx;
	int dz;
	int angle;

	dx = abs(car_data[0].location.vx - camera->position[0]);
	dz = abs(car_data[0].location.vz - camera->position[2]);

	if (dz * dz + dx * dx > 15000 * 15000)
	{
		if (camera->type == CAMERA_FOLLOW)
		{
			camera->distance = 15000;
			camera->max_distance = 15000;

			return 1;
		}

		if (camera->type == CAMERA_REPLAY_TARGET || camera->type == CAMERA_REPLAY_FIXED)
		{
			angle = Arc_Tan(camera->holding_position[2] - car_data[0].location.vz,
							camera->holding_position[0] - car_data[0].location.vx) & 0xfff;

			camera->holding_position[0] = (int)(fcos(angle) * 15000.0);
			camera->holding_position[2] = (int)(fsin(angle) * 15000.0);

			camera->holding_position[0] += car_data[0].location.vx;
			camera->holding_position[2] += car_data[0].location.vz;

			memcpy(camera->position, camera->holding_position, sizeof(LONGVECTOR4));
		}
	}

	return 1;
}

// [0x4c4c84] [D]
// While the director runs: a camera more than 30000 from its car becomes a chase camera behind the
// player; a chase camera closer than 100 becomes an in car camera. The change is stored as a camera.
void CheckCameraDistance(DIRECTOR_CAMERA *camera)
{
	DIRECTOR_CAMERA *last;
	CAR_DATA *cp;
	int dx;
	int dz;
	int dist;

	if (Director.Mode == DM_EXIT)
		return;

	if (camera->target == -1)
		camera->target = 0;

	cp = &car_data[camera->target];

	if (camera->type != CAMERA_FOLLOW)
	{
		if (camera->type == CAMERA_INCAR)
			return;

		dx = camera->position[0] - cp->location.vx;
		dz = camera->position[2] - cp->location.vz;
		dist = dz * dz + dx * dx;
	}
	else
	{
		dist = camera->max_distance * camera->max_distance;
	}

	if (dist <= 30000 * 30000)
	{
		if (dist < 100 * 100 && camera->type == CAMERA_FOLLOW)
		{
			camera->type = CAMERA_INCAR;

			MakeChaseCamera(camera);

			last = GetLastCameraByFrameIndex(Director.FrameIndex);

			if (last)
				AddDirectorCamera(camera, last->frame_index);
			else
				AddDirectorCamera(camera, Director.FrameIndex);
		}
	}
	else
	{
		camera->target = 0;
		camera->type = CAMERA_FOLLOW;
		camera->turnAngle = 1280;
		camera->camerapos[1] = -256;

		AddDirectorCamera(camera, Director.FrameIndex);
	}
}

// [0x4c4dca] [D]
// Keeps a camera at least 100 above the ground and at most 1600 (620 in level 4) above it.
void UpdateCameraHeight(DIRECTOR_CAMERA *camera)
{
	CAR_DATA *cp;
	int ground;
	int max_height;

	if (camera->target == -1)
		camera->target = 0;

	cp = &car_data[camera->target];

	if (GameLevel == 4)
		max_height = -620;
	else
		max_height = -1600;

	ground = -MapHeightInt((IVECTOR *)camera->position) - 100;

	if (camera->position[1] > ground)
	{
		camera->camerapos[1] = cp->location.vy + ground;
		camera->position[1] = ground;
		camera->holding_position[1] = ground;
	}

	if (camera->position[1] - ground < max_height)
	{
		camera->position[1] = max_height + ground;
		camera->holding_position[1] = max_height + ground;
		camera->camerapos[1] = cp->location.vy + camera->position[1];
	}
}

// [0x4c4ea1] [D]
// Looks up the icon texture of each item of a menu.
void InitialiseDirectorMenu(DIRECTOR_TEXT *menu)
{
	while (menu->index != DT_END)
	{
		if (strlen(menu->icon) != 0)
			menu->texture = GetTextureDetails(menu->icon);
		else
			menu->texture = NULL;

		menu++;
	}
}

// [0x4c4ef1] [D]
void DestroyWaypoints(void)
{
	if (Director.Waypoints)
		DebugFree(Director.Waypoints);

	Director.Waypoints = NULL;
	Director.NumWaypoints = 0;
}

// [0x4c4f21] [D]
// The waypoint after the one recorded at or before frame (waypoints are WAYPOINT_FRAMES apart).
WAYPOINT *FindWaypoint(int frame)
{
	if (Director.Waypoints == NULL)
		return NULL;

	if (frame / WAYPOINT_FRAMES + 1 < Director.NumWaypoints)
		return &Director.Waypoints[frame / WAYPOINT_FRAMES + 1];

	return NULL;
}

// [0x4c4f64] [D]
// The waypoint recorded exactly at frame - 2, if any.
WAYPOINT *FindNextWaypoint(int frame)
{
	int index;

	if (Director.Waypoints == NULL)
		return NULL;

	index = (frame - 2) / WAYPOINT_FRAMES;

	if (WAYPOINT_FRAMES * index != frame - 2)
		return NULL;

	if (index < Director.NumWaypoints)
		return &Director.Waypoints[index];

	return NULL;
}

// [0x4c4fbd] [D]
// NOTE: unreferenced
void DumpWaypoints(void)
{
	char buffer[64];
	int i;

	if (Director.NumWaypoints == 0)
		return;

	sprintf(buffer, "The game contains %d replay waypoints.", Director.NumWaypoints);
	LogQuick(buffer);

	for (i = 0; i < Director.NumWaypoints; i++)
	{
		sprintf(buffer, "Waypoint number %d, dump information:", i);
		LogQuick(buffer);

		sprintf(buffer, "Frame index: %d", Director.Waypoints[i].frame);
		LogQuick(buffer);

		sprintf(buffer, "X position: %d\nZ position: %d", Director.Waypoints[i].x, Director.Waypoints[i].z);
		LogQuick(buffer);
	}
}

// [0x4c50a9] [D]
// Moves the menu selection with the pad (disabled items are skipped) or runs the selected command.
// Moving onto or off the in car item of the add / edit submenu starts or cancels the camera edit.
// Rebuilt from the disassembly (Hex-Rays mangles the switch).
void ReplayMenuHandler(int padd)
{
	DIRECTOR_TEXT *item;
	int index;
	int skip;

	skip = 1;

	Director.LastSelection = Director.Selection;
	item = GetDirectorItem(Director.Selection);

	if (item == NULL)
		return;

	switch (padd)
	{
		case 0:
			break;
		case DPAD_LEFT:
			// move left
			if (Director.Selection == DT_PLAYPAUSE)
			{
				Director.Selection = DT_EXIT;
				break;
			}

			while (skip)
			{
				skip = 0;

				index = item->index;
				item = sub_4C5965(index);

				if (item == NULL || item->next != index)
					return;

				if (item->flags & DTF_DISABLED)
				{
					if (item->index == DT_PLAYPAUSE)
						item = GetDirectorItem(DT_EXIT);
					else
						skip = 1;
				}
			}

			Director.Selection = item->index;
			break;
		case DPAD_RIGHT:
			// move right
			if (Director.Selection == DT_EXIT)
			{
				Director.Selection = DT_PLAYPAUSE;
				break;
			}

			if (!(item->flags & DTF_SIDE_SUBMENU) || item->index == DT_PLAYPAUSE)
			{
				item = GetDirectorItem(item->next);

				do
				{
					if (item == NULL)
						return;

					if (item->flags & DTF_DISABLED)
						item = GetDirectorItem(item->next);
					else
						skip = 0;
				} while (skip);
			}

			Director.Selection = item->index;
			break;
		case DPAD_UP:
			// move up
			while (skip)
			{
				skip = 0;

				index = item->index;
				item = sub_4C5965(index);

				if (item == NULL || item->extra != index)
					return;

				if (item->flags & DTF_DISABLED)
					skip = 1;
			}

			Director.Selection = item->index;
			break;
		case DPAD_DOWN:
			// move down
			if (!(item->flags & DTF_DOWN_SUBMENU))
			{
				item = GetDirectorItem(item->extra);

				do
				{
					if (item == NULL)
						return;

					if (item->flags & DTF_DISABLED)
						item = GetDirectorItem(item->extra);
					else
						skip = 0;
				} while (skip);
			}

			Director.Selection = item->index;
			break;
		case DPAD_SELECT:
			// do the thing
			HandleMenuCommand(Director.Selection);
			break;
		default:
			LogQuick("ReplayMenuHandler: Error! - Unknown movement/selection.");
			break;
	}

	if (Director.Selection == DT_ADD_INCAR && Director.LastSelection == DT_ADDCAM)
		StartDirectorModify();

	if (Director.Selection == DT_EDIT_INCAR && Director.LastSelection == DT_EDITCAM)
		StartDirectorModify();

	if (Director.Selection == DT_EDITCAM && Director.LastSelection == DT_EDIT_INCAR)
		CancelDirectorModify();

	if (Director.Selection == DT_ADDCAM && Director.LastSelection == DT_ADD_INCAR)
		CancelDirectorModify();
}

// [0x4c538e] [D]
// Draws the icons of the selected item's submenus and of the play / pause buttons, then their texts.
void DrawReplayMenu(void)
{
	DIRECTOR_TEXT *item;

	SwapPalette(27);

	for (item = GetReplaySubMenu(Director.Selection); item; item = GetReplaySubMenu(item->index))
		DrawReplaySubMenu(item);

	DrawReplaySubMenu(GetDirectorItem(DT_PLAYPAUSE));
	DrawReplaySubMenu(GetDirectorItem(DT_UNKNOWN));

	for (item = GetReplaySubMenu(Director.Selection); item; item = GetReplaySubMenu(item->index))
		DrawReplaySubMenuText(item);

	DrawReplaySubMenuText(GetDirectorItem(DT_PLAYPAUSE));
	DrawReplaySubMenuText(GetDirectorItem(DT_UNKNOWN));
}

// [0x4c545b] [D]
// Draws the icons of a menu row (or column, DTF_DOWN_SUBMENU) starting at menu.
void DrawReplaySubMenu(DIRECTOR_TEXT *menu)
{
	DIRECTOR_TEXT *item;

	if (menu == NULL)
		return;

	item = menu;

	while (item)
	{
		DrawDirectorIcon(item);

		if (menu->flags & DTF_DOWN_SUBMENU)
			item = GetDirectorItem(item->extra);
		else
			item = GetDirectorItem(item->next);
	}
}

// [0x4c54b8] [D]
// Draws the text of the selected item of a menu row (or column) starting at menu.
void DrawReplaySubMenuText(DIRECTOR_TEXT *menu)
{
	DIRECTOR_TEXT *item;

	if (menu == NULL)
		return;

	item = menu;

	while (item)
	{
		if (item->index == Director.Selection)
			DrawDirectorText(item);

		if (menu->flags & DTF_DOWN_SUBMENU)
			item = GetDirectorItem(item->extra);
		else
			item = GetDirectorItem(item->next);
	}
}

// [0x4c5523] [D]
// Draws a menu icon: flashing when selected, dark when disabled.
void DrawDirectorIcon(DIRECTOR_TEXT *item)
{
	float x;
	float y;
	int colour;

	if (item->index == Director.Selection)
		colour = Director.IconColor;
	else if (item->flags & DTF_DISABLED)
		colour = 0x804500;
	else
		colour = 0xff8b00;

	x = 53.0f * (float)item->offsetX + 16.0f;
	y = 53.0f * (float)item->offsetY + 16.0f;

	if (item->texture)
		DrawScaledTexturedQuadSize(x, y, 45.0f / (float)item->texture->w, 45.0f / (float)item->texture->h, item->texture, colour);
	else
		DrawQuad(x, y, 45.0f, 45.0f, colour);
}

// [0x4c5631] [D]
// Writes the name of a menu item under its icon (or beside it, DTF_TEXT_RIGHT); grey when disabled.
void DrawDirectorText(DIRECTOR_TEXT *item)
{
	int x;
	int y;
	u_char r;
	u_char g;
	u_char b;

	x = ICON_POS(item->offsetX);

	r = 255;
	g = 255;
	b = 255;

	if (item->flags & DTF_DISABLED)
	{
		r = 64;
		g = 64;
		b = 64;
	}

	if (item->flags & DTF_TEXT_RIGHT)
	{
		x = ICON_POS(item->offsetX) + 49;
		y = ICON_POS(item->offsetY);
		y += ICON_SIZE - GetFontHeight(0);
	}
	else
	{
		// this belongs at the top, not in an else statement
		y = ICON_POS(item->offsetY) + 49;
	}

	if (item->text != -1)
		WriteDropShadowedString(x, y, gLanguageStrings[item->text], 0, 0, r, g, b);
}

// [0x4c56ed] [D]
// Enables or disables a menu item; returns whether it was enabled.
BOOL EnableDirectorItem(int index, int enabled)
{
	DIRECTOR_TEXT *item;
	BOOL was_enabled;

	item = GetDirectorItem(index);

	if (item == NULL)
		return 0;

	was_enabled = (item->flags & DTF_DISABLED) == 0;

	if (enabled)
		item->flags &= ~DTF_DISABLED;
	else
		item->flags |= DTF_DISABLED;

	return was_enabled;
}

// [0x4c5756] [D]
// Reads the replay keys as DPAD_* bits: debounced (one key, CheckKey) in the menu, held (KeyMap)
// while moving a camera.
int ReplayDebounce(void)
{
	int padd;

	padd = 0;

	if (Director.Debounce)
	{
		if (CheckKey(DIK_LSHIFT) || CheckKey(DIK_RSHIFT))
			padd = DPAD_FAST;
		if (CheckKey(DIK_SUBTRACT))
			padd = DPAD_LOWER;
		if (CheckKey(DIK_ADD))
			padd = DPAD_RAISE;
		if (CheckKey(DIK_LEFT))
			padd = DPAD_LEFT;
		if (CheckKey(DIK_RIGHT))
			padd = DPAD_RIGHT;
		if (CheckKey(DIK_UP))
			padd = DPAD_UP;
		if (CheckKey(DIK_DOWN))
			padd = DPAD_DOWN;
		if (CheckKey(DIK_RETURN) || CheckKey(DIK_SPACE))
			return DPAD_SELECT;
	}
	else
	{
		if (KeyMap[DIK_LSHIFT] || KeyMap[DIK_RSHIFT])
			padd |= DPAD_FAST;
		if (KeyMap[DIK_SUBTRACT])
			padd |= DPAD_LOWER;
		if (KeyMap[DIK_ADD])
			padd |= DPAD_RAISE;
		if (KeyMap[DIK_LEFT])
			padd |= DPAD_LEFT;
		if (KeyMap[DIK_RIGHT])
			padd |= DPAD_RIGHT;
		if (KeyMap[DIK_UP])
			padd |= DPAD_UP;
		if (KeyMap[DIK_DOWN])
			padd |= DPAD_DOWN;
		if (KeyMap[DIK_RETURN] || KeyMap[DIK_SPACE])
			padd |= DPAD_SELECT;
		if (KeyMap[DIK_END])
			return padd | DPAD_LEVEL;
	}

	return padd;
}

// [0x4c5918] [D]
// The item of the current menu with this index.
DIRECTOR_TEXT *GetDirectorItem(int index)
{
	DIRECTOR_TEXT *item;

	if (index == 0)
		return NULL;

	item = Director.Menu;

	if (item == NULL)
		return NULL;

	while (item->index != DT_END)
	{
		if (item->index == index)
			return item;

		item++;
	}

	return NULL;
}

// [0x4c5965] [D]
// The item of the current menu whose next or extra is index (the one left of it or above it).
DIRECTOR_TEXT *sub_4C5965(int index)
{
	DIRECTOR_TEXT *item;

	if (index == 0)
		return NULL;

	for (item = Director.Menu; item->index != DT_END; item++)
	{
		if (item->next == index || item->extra == index)
			return item;
	}

	return NULL;
}

// [0x4c59b3] [D]
// The parent item of a menu item.
DIRECTOR_TEXT *GetReplaySubMenu(int index)
{
	DIRECTOR_TEXT *item;

	if (index == 0)
		return NULL;

	item = GetDirectorItem(index);

	if (item)
		return GetDirectorItem(item->prev);

	return NULL;
}

// [0x4c59ed] [D]
// The menu item whose icon is under x y.
// NOTE: unreferenced
int sub_4C59ED(int x, int y)
{
	DIRECTOR_TEXT *item;

	// OOOOOOOOOOOOO, mouse bounds check!
	for (item = Director.Menu; item->index != DT_END; item++)
	{
		if (x >= ICON_POS(item->offsetX) && x < ICON_POS(item->offsetX) + ICON_SIZE
			&& y >= ICON_POS(item->offsetY) && y < ICON_POS(item->offsetY) + ICON_SIZE)
		{
			return item->index;
		}
	}

	return 0;
}

// [0x4c5a70] [D]
// Flashes the "demonstration" text in a corner (or the middle) of the screen.
void DisplayDemonstration(void)
{
	if (Director.Attract == 0 || Director.DemoTimer >= 32)
		return;

	switch (Director.DemoType)
	{
		case 0:
			WriteDropShadowedString(16, 16, gLanguageStrings[270], 0, 0, 255, 0, 0);
			break;
		case 1:
			WriteDropShadowedString(624, 16, gLanguageStrings[270], 0, 2, 255, 0, 0);
			break;
		case 2:
			WriteDropShadowedString(16, 464, gLanguageStrings[270], 0, 0x20, 255, 0, 0);
			break;
		case 3:
			WriteDropShadowedString(624, 464, gLanguageStrings[270], 0, 0x22, 255, 0, 0);
			break;
		case 4:
			WriteDropShadowedString(320, 240, gLanguageStrings[270], 0, 1, 255, 0, 0);
			break;
	}
}

// [0x4c5b7d] [D]
// Runs the command of the selected menu item (found through its top level item).
void HandleMenuCommand(int selection)
{
	DIRECTOR_TEXT *item;

	for (item = GetDirectorItem(selection); item->prev != 0; item = GetDirectorItem(item->prev))
		;

	switch (item->index)
	{
		case DT_PLAYPAUSE:
			SetDirectorMode(DM_PLAYPAUSE, 0, 0);
			break;
		case DT_AUTOCAM:
			SetDirectorMode(DM_AUTOCAMERA, 0, 0);
			break;
		case DT_PLAYCAM:
			SetDirectorMode(DM_PLAYCAMERA, 0, 0);
			break;
		case DT_FRAME_ADVANCE:
			SetDirectorMode(DM_FRAMEADVANCE, 0, 0);
			break;
		case DT_RESTART:
			if (Director.FrameIndex > 2)
			{
				SaveDirector(TEMP_CAMERA_FILE);

				wanted_exit_flag = 5;
				quit = 0;
			}
			break;
		case DT_ADDCAM:
			if (item->index == selection)
				Director.Selection = item->extra;
			else
				AddCamera(selection);
			break;
		case DT_EDITCAM:
			if (item->index == selection)
				Director.Selection = item->extra;
			else
				ChangeCamera(selection);
			break;
		case DT_EXIT:
			SaveDirector(TEMP_CAMERA_FILE);
			SetDirectorMode(DM_EXIT, DT_PLAYPAUSE, 0);
			PauseGame(1);
			break;
		case DT_UNKNOWN:
			if (Director.Mode == DM_AUTOCAMERA)
				SetDirectorMode(DM_PLAY, DT_AUTOCAM, 0);
			else if (Director.Mode == DM_PLAYCAMERA)
				SetDirectorMode(DM_PLAY, DT_PLAYCAM, 0);
			else
				SetDirectorMode(DM_PLAY, DT_PLAYPAUSE, 0);
			break;
	}

	if (Director.WorkCamera.type != CAMERA_REPLAY_TARGET)
		Director.WorkCamera.perspective = 0;
}

// [0x4c5d66] [D]
// The add camera submenu: sets up the work camera as an in car, chase or fixed camera; OK stores it
// at the current frame.
void AddCamera(int selection)
{
	DIRECTOR_CAMERA *camera;
	DIRECTOR_TEXT *item;

	for (item = GetDirectorItem(selection); item->prev != DT_ADDCAM && item->prev != 0; item = GetDirectorItem(item->prev))
		;

	switch (item->index)
	{
		case DT_ADD_INCAR:
			if (item->index == selection)
			{
				CamPerspectiveMode = 0;

				if (PursuersInView())
				{
					Director.WorkCamera.type = CAMERA_INCAR;
					Director.Selection = item->next;
				}
				else
				{
					cameraview = CAMERA_INCAR;

					PlaceCameraInCar(0);
					GetCurrentCamera(&Director.WorkCamera);
				}
			}
			else
			{
				Director.WorkCamera.target = NextPursuerInView(Director.WorkCamera.target);
			}
			break;
		case DT_ADD_CHASE:
			if (item->index == selection)
			{
				CamPerspectiveMode = 0;

				if (PursuersInView())
				{
					MakeChaseCamera(&Director.WorkCamera);
					SetCurrentCamera(&Director.WorkCamera);
				}
				else
				{
					Director.WorkCamera.target = 0;
					MakeChaseCamera(&Director.WorkCamera);
				}

				Director.Selection = item->next;
				item = GetDirectorItem(item->next);

				if (item->flags & DTF_DISABLED)
					Director.Selection = item->next;
			}
			else if (selection == DT_ADD_CHASE_MOVECAM)
			{
				SetDirectorMode(DM_MOVECAMERA, 0, 0);
			}
			else if (selection == DT_ADD_CHASE_CHOOSECAR)
			{
				Director.WorkCamera.target = NextPursuerInView(Director.WorkCamera.target);
				MakeChaseCamera(&Director.WorkCamera);
			}
			break;
		case DT_ADD_FIXED:
			if (item->index == selection)
			{
				if (Director.WorkCamera.type != CAMERA_REPLAY_TARGET && Director.WorkCamera.type != CAMERA_REPLAY_FIXED)
				{
					Director.WorkCamera.target = 0;

					if (Director.WorkCamera.type == CAMERA_INCAR)
					{
						MakeChaseCamera(&Director.WorkCamera);
						LockCameraToCar(&Director.WorkCamera);
					}

					Director.WorkCamera.type = CAMERA_REPLAY_TARGET;
					memcpy(Director.WorkCamera.holding_position, Director.WorkCamera.position, sizeof(LONGVECTOR4));
				}

				Director.Selection = item->next;
			}
			else
			{
				AddTripodCamera(selection);
			}
			break;
		case DT_ADD_OK:
			FinishDirectorModify();

			camera = GetCameraByFrameIndex(Director.FrameIndex);

			if (camera)
				DeleteCamera(camera->index);

			AddDirectorCamera(&Director.WorkCamera, Director.FrameIndex);

			memcpy(&Director.WorkCamera, GetLastCameraByFrameIndex(Director.FrameIndex), sizeof(Director.WorkCamera));

			Director.Selection = DT_ADDCAM;
			break;
		default:
			LogQuick("AddCamera: Unknown command id.\n");
			break;
	}
}

// [0x4c602c] [D]
// The edit camera submenu: changes the current camera's type, start frame (clock), deletes it; OK
// stores the work camera over it.
void ChangeCamera(int selection)
{
	DIRECTOR_CAMERA *camera;
	DIRECTOR_TEXT *item;

	for (item = GetDirectorItem(selection); item->prev != DT_EDITCAM && item->prev != 0; item = GetDirectorItem(item->prev))
		;

	switch (item->index)
	{
		case DT_EDIT_INCAR:
			if (item->index == selection)
			{
				CamPerspectiveMode = 0;

				if (PursuersInView())
				{
					Director.WorkCamera.type = CAMERA_INCAR;
					Director.Selection = item->next;
				}
				else
				{
					cameraview = CAMERA_INCAR;

					PlaceCameraInCar(0);
					GetCurrentCamera(&Director.WorkCamera);
				}
			}
			else
			{
				Director.WorkCamera.target = NextPursuerInView(Director.WorkCamera.target);
			}
			break;
		case DT_EDIT_CHASE:
			if (item->index == selection)
			{
				CamPerspectiveMode = 0;

				if (PursuersInView())
				{
					if (cameraview != CAMERA_FOLLOW)
					{
						MakeChaseCamera(&Director.WorkCamera);
						SetCurrentCamera(&Director.WorkCamera);
					}
				}
				else
				{
					Director.WorkCamera.target = 0;
					MakeChaseCamera(&Director.WorkCamera);
					SetCurrentCamera(&Director.WorkCamera);
				}

				Director.Selection = item->next;
				item = GetDirectorItem(item->next);

				if (item->flags & DTF_DISABLED)
					Director.Selection = item->next;
			}
			else if (selection == DT_EDIT_CHASE_MOVECAM)
			{
				SetDirectorMode(DM_MOVECAMERA, 0, 0);
			}
			else if (selection == DT_EDIT_CHASE_CHOOSECAR)
			{
				Director.WorkCamera.target = NextPursuerInView(Director.WorkCamera.target);
				MakeChaseCamera(&Director.WorkCamera);
			}
			break;
		case DT_EDIT_FIXED:
			if (item->index == selection)
			{
				if (Director.WorkCamera.type != CAMERA_REPLAY_TARGET && Director.WorkCamera.type != CAMERA_REPLAY_FIXED)
				{
					Director.WorkCamera.target = 0;

					if (Director.WorkCamera.type == CAMERA_INCAR)
					{
						MakeChaseCamera(&Director.WorkCamera);
						LockCameraToCar(&Director.WorkCamera);
					}

					Director.WorkCamera.type = CAMERA_REPLAY_TARGET;
					memcpy(Director.WorkCamera.holding_position, Director.WorkCamera.position, sizeof(LONGVECTOR4));
				}

				Director.Selection = item->next;
			}
			else
			{
				ChangeTripodCamera(selection);
			}
			break;
		case DT_EDIT_CLOCK:
			SetDirectorMode(DM_EDITCAMERA, 0, 0);
			break;
		case DT_EDIT_DELETE:
			camera = GetLastCameraByFrameIndex(Director.FrameIndex);

			if (camera)
				DeleteCamera(camera->index);

			if (GetLastCameraByFrameIndex(Director.FrameIndex))
				Director.Selection = DT_EDITCAM;
			else
				Director.Selection = DT_ADDCAM;
			break;
		case DT_EDIT_OK:
			FinishDirectorModify();

			camera = GetLastCameraByFrameIndex(Director.FrameIndex);

			GetCurrentCamera(camera);
			ReInitDirectorCameras();

			memcpy(&Director.WorkCamera, camera, sizeof(Director.WorkCamera));

			Director.Selection = DT_EDITCAM;
			break;
		default:
			LogQuick("ChangeCamera: Unknown command id.");
			break;
	}
}

// [0x4c6359] [D]
// The add fixed camera submenu: move it, make it watch the player, make it static (facing the
// player, then rotated with DM_ROTATECAMERA), toggle the lens.
void AddTripodCamera(int selection)
{
	CAR_DATA *cp;

	Director.WorkCamera.target = 0;
	cp = &car_data[Director.WorkCamera.target];

	switch (selection)
	{
		case DT_ADD_FIXED_MOVE:
			if (Director.WorkCamera.type == CAMERA_REPLAY_TARGET || Director.WorkCamera.type == CAMERA_REPLAY_FIXED)
				SetDirectorMode(DM_MOVECAMERA, 0, 0);
			break;
		case DT_ADD_FIXED_LOOK:
			if (Director.WorkCamera.type != CAMERA_INCAR)
			{
				Director.WorkCamera.type = CAMERA_REPLAY_TARGET;
				Director.WorkCamera.holding_position[0] = Director.WorkCamera.position[0];
				Director.WorkCamera.holding_position[1] = Director.WorkCamera.position[1];
				Director.WorkCamera.holding_position[2] = Director.WorkCamera.position[2];
			}
			break;
		case DT_ADD_FIXED_MOVECMP:
			if (Director.WorkCamera.type != CAMERA_INCAR)
			{
				if (Director.WorkCamera.type == CAMERA_REPLAY_FIXED)
				{
					SetDirectorMode(DM_ROTATECAMERA, 0, 0);
				}
				else
				{
					Director.WorkCamera.holding_position[0] = Director.WorkCamera.position[0];
					Director.WorkCamera.holding_position[1] = Director.WorkCamera.position[1];
					Director.WorkCamera.holding_position[2] = Director.WorkCamera.position[2];
					Director.WorkCamera.turnAngle = -Arc_Tan(cp->location.vx - Director.WorkCamera.position[0],
															 cp->location.vz - Director.WorkCamera.position[2]) & 0xfff;
					Director.WorkCamera.yang = Director.WorkCamera.turnAngle;
					Director.WorkCamera.angle.vy = Director.WorkCamera.yang;
					Director.WorkCamera.xang = 0;
					Director.WorkCamera.type = CAMERA_REPLAY_FIXED;
				}
			}
			break;
		case DT_ADD_FIXED_LENSCHA:
			if (Director.WorkCamera.type == CAMERA_REPLAY_TARGET)
				Director.WorkCamera.perspective = Director.WorkCamera.perspective == 0;
			break;
		default:
			LogQuick("AddTripodCamera: Unknown command id.");
			break;
	}
}

// [0x4c650d] [D]
// The edit fixed camera submenu (AddTripodCamera, but a new static camera also gets the pitch to
// the target car).
void ChangeTripodCamera(int selection)
{
	CAR_DATA *cp;
	int dx;
	int dy;
	int dz;
	int dist;

	if (Director.WorkCamera.target == -1)
		cp = &car_data[0];
	else
		cp = &car_data[Director.WorkCamera.target];

	switch (selection)
	{
		case DT_EDIT_FIXED_MOVE:
			if (Director.WorkCamera.type == CAMERA_REPLAY_TARGET || Director.WorkCamera.type == CAMERA_REPLAY_FIXED)
				SetDirectorMode(DM_MOVECAMERA, 0, 0);
			break;
		case DT_EDIT_FIXED_LOOK:
			if (Director.WorkCamera.type != CAMERA_INCAR)
			{
				Director.WorkCamera.type = CAMERA_REPLAY_TARGET;
				Director.WorkCamera.holding_position[0] = Director.WorkCamera.position[0];
				Director.WorkCamera.holding_position[1] = Director.WorkCamera.position[1];
				Director.WorkCamera.holding_position[2] = Director.WorkCamera.position[2];
			}
			break;
		case DT_EDIT_FIXED_MOVECMP:
			if (Director.WorkCamera.type != CAMERA_INCAR)
			{
				if (Director.WorkCamera.type == CAMERA_REPLAY_FIXED)
				{
					SetDirectorMode(DM_ROTATECAMERA, 0, 0);
				}
				else
				{
					Director.WorkCamera.holding_position[0] = Director.WorkCamera.position[0];
					Director.WorkCamera.holding_position[1] = Director.WorkCamera.position[1];
					Director.WorkCamera.holding_position[2] = Director.WorkCamera.position[2];

					dx = cp->location.vx - Director.WorkCamera.position[0];
					dy = cp->location.vy - Director.WorkCamera.position[1];
					dz = cp->location.vz - Director.WorkCamera.position[2];

					Director.WorkCamera.turnAngle = -Arc_Tan(dx, dz) & 0xfff;
					Director.WorkCamera.yang = Director.WorkCamera.turnAngle;
					Director.WorkCamera.angle.vy = Director.WorkCamera.yang;

					dist = (int)sqrt((double)(dz * dz + dx * dx));

					Director.WorkCamera.xang = Arc_Tan(dy, dist);
					Director.WorkCamera.angle.vx = Director.WorkCamera.xang;
					Director.WorkCamera.type = CAMERA_REPLAY_FIXED;
				}
			}
			break;
		case DT_EDIT_FIXED_LENSCHA:
			if (Director.WorkCamera.type == CAMERA_REPLAY_TARGET)
				Director.WorkCamera.perspective = Director.WorkCamera.perspective == 0;
			break;
		default:
			LogQuick("ChangeTripodCamera: Unknown command id.");
			break;
	}
}

// [0x4c6718] [D]
// Enables all menu items, then disables what can't be used now (at the end of the replay, without
// a camera at this frame, without cops in view, by the camera type).
void InitialiseEnabledItems(void)
{
	EnableDirectorItem(DT_PLAYPAUSE, 1);
	EnableDirectorItem(DT_AUTOCAM, 1);
	EnableDirectorItem(DT_PLAYCAM, 1);
	EnableDirectorItem(DT_FRAME_ADVANCE, 1);
	EnableDirectorItem(DT_RESTART, 1);
	EnableDirectorItem(DT_ADDCAM, 1);
	EnableDirectorItem(DT_EDITCAM, 1);
	EnableDirectorItem(DT_EXIT, 1);
	EnableDirectorItem(DT_ADD_INCAR, 1);
	EnableDirectorItem(DT_ADD_CHASE, 1);
	EnableDirectorItem(DT_ADD_FIXED, 1);
	EnableDirectorItem(DT_ADD_OK, 1);
	EnableDirectorItem(DT_EDIT_INCAR, 1);
	EnableDirectorItem(DT_EDIT_CHASE, 1);
	EnableDirectorItem(DT_EDIT_FIXED, 1);
	EnableDirectorItem(DT_EDIT_CLOCK, 1);
	EnableDirectorItem(DT_EDIT_DELETE, 1);
	EnableDirectorItem(DT_EDIT_OK, 1);
	EnableDirectorItem(DT_ADD_FIXED_MOVE, 1);
	EnableDirectorItem(DT_ADD_FIXED_LOOK, 1);
	EnableDirectorItem(DT_ADD_FIXED_MOVECMP, 1);
	EnableDirectorItem(DT_ADD_FIXED_LENSCHA, 1);
	EnableDirectorItem(DT_EDIT_FIXED_MOVE, 1);
	EnableDirectorItem(DT_EDIT_FIXED_LOOK, 1);
	EnableDirectorItem(DT_EDIT_FIXED_MOVECMP, 1);
	EnableDirectorItem(DT_EDIT_FIXED_LENSCHA, 1);
	EnableDirectorItem(DT_UNKNOWN, 1);
	EnableDirectorItem(DT_ADD_INCAR_CHOOSECAR, 1);
	EnableDirectorItem(DT_EDIT_INCAR_CHOOSECAR, 1);
	EnableDirectorItem(DT_ADD_CHASE_CHOOSECAR, 1);
	EnableDirectorItem(DT_ADD_CHASE_MOVECAM, 1);
	EnableDirectorItem(DT_ADD_UNKNOWN, 1);
	EnableDirectorItem(DT_EDIT_UNKNOWN, 1);
	EnableDirectorItem(DT_EDIT_CHASE_CHOOSECAR, 1);
	EnableDirectorItem(DT_EDIT_CHASE_MOVECAM, 1);

	if (Director.FrameIndex < 3)
		EnableDirectorItem(DT_RESTART, 0);

	if (Director.FrameIndex >= Director.NumFrames)
	{
		EnableDirectorItem(DT_PLAYPAUSE, 0);
		EnableDirectorItem(DT_AUTOCAM, 0);
		EnableDirectorItem(DT_PLAYCAM, 0);
		EnableDirectorItem(DT_FRAME_ADVANCE, 0);
		EnableDirectorItem(DT_ADDCAM, 0);
		EnableDirectorItem(DT_EDITCAM, 0);

		if (Director.FrameIndex >= 3)
		{
			if (Director.Selection != DT_RESTART && Director.Selection != DT_EXIT)
				Director.Selection = DT_RESTART;
		}
		else
		{
			Director.Selection = DT_EXIT;
		}
	}

	if (cameraview == CAMERA_INCAR)
	{
		EnableDirectorItem(DT_ADD_FIXED_LOOK, 0);
		EnableDirectorItem(DT_ADD_FIXED_LENSCHA, 0);
		EnableDirectorItem(DT_EDIT_FIXED_LOOK, 0);
		EnableDirectorItem(DT_EDIT_FIXED_LENSCHA, 0);
	}

	if (PursuersInView() == 0)
	{
		EnableDirectorItem(DT_ADD_CHASE_CHOOSECAR, 0);
		EnableDirectorItem(DT_EDIT_CHASE_CHOOSECAR, 0);
		EnableDirectorItem(DT_ADD_INCAR_CHOOSECAR, 0);
		EnableDirectorItem(DT_EDIT_INCAR_CHOOSECAR, 0);
	}

	if (cameraview == CAMERA_FOLLOW)
	{
		EnableDirectorItem(DT_ADD_FIXED_LENSCHA, 0);
		EnableDirectorItem(DT_EDIT_FIXED_LENSCHA, 0);
	}

	if (cameraview != CAMERA_REPLAY_TARGET)
	{
		EnableDirectorItem(DT_ADD_FIXED_LENSCHA, 0);
		EnableDirectorItem(DT_EDIT_FIXED_LENSCHA, 0);
	}

	if (GetLastCameraByFrameIndex(Director.FrameIndex) == NULL)
		EnableDirectorItem(DT_EDITCAM, 0);
}

// [0x4c6a41] [D]
// The automatic director: switches to a new (or stored) camera when one is due and moves the
// sound listener to it.
void AutoDirector(void)
{
	if (CheckForNewCamera(&Director.Camera) == 0)
		return;

	if (Director.FrameIndex <= 3)
		Director.Camera->frame_index = 2;
	else
		Director.Camera->frame_index = Director.FrameIndex;

	Director.CameraIndex = Director.Camera->index;
	memcpy(&Director.WorkCamera, Director.Camera, sizeof(Director.WorkCamera));

	SetListenerPosition((float)Director.Camera->position[0], (float)Director.Camera->position[1], (float)Director.Camera->position[2]);
}

// [0x4c6ae6] [D]
// Plays the stored cameras: takes the camera of the current frame, moving the sound listener when
// it changes.
void PlayDirectorReplay(void)
{
	DIRECTOR_CAMERA *camera;
	int old_index;

	old_index = Director.CameraIndex;
	camera = GetLastCameraByFrameIndex(Director.FrameIndex);

	if (camera == NULL)
		return;

	Director.Camera = camera;
	memcpy(&Director.WorkCamera, camera, sizeof(Director.WorkCamera));
	Director.CameraIndex = camera->index;

	if (Director.CameraIndex != old_index)
		SetListenerPosition((float)camera->position[0], (float)camera->position[1], (float)camera->position[2]);
}

// [0x4c6b74] [D]
// DM_REPLAYEDITOR: empty.
void ReplayEditor(void)
{
}

// [0x4c6b79] [D]
// Sets the game camera (camera.c) from a director camera.
void SetCurrentCamera(DIRECTOR_CAMERA *camera)
{
	camera_position.vx = (float)camera->position[0];
	camera_position.vy = (float)camera->position[1];
	camera_position.vz = (float)camera->position[2];
	CameraPos.vx = (float)camera->camerapos[0];
	CameraPos.vy = (float)camera->camerapos[1];
	CameraPos.vz = (float)camera->camerapos[2];
	holding_camera_position.vx = (float)camera->holding_position[0];
	holding_camera_position.vy = (float)camera->holding_position[1];
	holding_camera_position.vz = (float)camera->holding_position[2];
	holding_camera_angle.vx = camera->angle.vx;
	holding_camera_angle.vy = camera->angle.vy;
	holding_camera_angle.vz = camera->angle.vz;
	gCameraDistance = (float)camera->distance;
	gCameraMaxDistance = (float)camera->max_distance;
	gCameraAngle = camera->turnAngle;
	current_camera_yangle = camera->yang;
	current_camera_xangle = camera->xang;
	TargetCar = camera->target;
	cameraview = camera->type;

	if (camera->type != CAMERA_REPLAY_TARGET)
		camera->perspective = 0;

	CamPerspectiveMode = camera->perspective;

	if (camera->type == CAMERA_INCAR)
	{
		CameraInCar = camera->target;
		PlaceCameraInCar(camera->target);
	}
}

// [0x4c6ca6] [D]
// Places a chase camera at its distance and angle behind its target car.
void LockCameraToCar(DIRECTOR_CAMERA *camera)
{
	CAR_DATA *cp;
	MATRIX mat;
	VECTOR4 offset;
	int angle;

	if (camera->type != CAMERA_FOLLOW)
		return;

	if (camera->target == -1)
		camera->target = 0;

	cp = &car_data[camera->target];
	angle = (camera->turnAngle + cp->direction) & 0xfff;

	offset.vx = 0.0f;
	offset.vy = (float)camera->camerapos[1];
	offset.vz = (float)camera->distance;

	mat.m[0][0] = 1.0f;
	mat.m[1][0] = 0.0f;
	mat.m[2][0] = 0.0f;
	mat.m[0][1] = 0.0f;
	mat.m[0][2] = 0.0f;
	mat.m[1][1] = 1.0f;
	mat.m[2][1] = 0.0f;
	mat.m[1][2] = 0.0f;
	mat.m[2][2] = 1.0f;

	RotMatrixY(&mat, -angle & 0xfff);

	CurrentRotationMatrix = &mat;
	MatrixRotate(&offset);

	camera->position[0] = (int)offset.vx + cp->location.vx;
	camera->position[1] = (int)offset.vy - cp->location.vy;
	camera->position[2] = (int)offset.vz + cp->location.vz;
}

// [0x4c6dcf] [D]
// Fills a director camera from the game camera (camera.c).
void GetCurrentCamera(DIRECTOR_CAMERA *camera)
{
	camera->holding_position[0] = float_to_int(holding_camera_position.vx);
	camera->holding_position[1] = float_to_int(holding_camera_position.vy);
	camera->holding_position[2] = float_to_int(holding_camera_position.vz);

	camera->camerapos[0] = float_to_int(CameraPos.vx);
	camera->camerapos[1] = float_to_int(CameraPos.vy);
	camera->camerapos[2] = float_to_int(CameraPos.vz);
	camera->angle.vx = holding_camera_angle.vx;
	camera->angle.vy = holding_camera_angle.vy;
	camera->angle.vz = holding_camera_angle.vz;
	camera->position[0] = float_to_int(camera_position.vx);
	camera->position[1] = float_to_int(camera_position.vy);
	camera->position[2] = float_to_int(camera_position.vz);
	camera->distance = float_to_int(gCameraDistance);
	camera->max_distance = float_to_int(gCameraMaxDistance);
	camera->turnAngle = gCameraAngle;
	camera->yang = current_camera_yangle;
	camera->xang = current_camera_xangle;
	camera->target = TargetCar;
	camera->type = (u_char)cameraview;
	camera->perspective = (u_char)CamPerspectiveMode;

	if (camera->type != CAMERA_REPLAY_TARGET)
		camera->perspective = 0;
}

// [0x4c6fb0] [D]
// NOTE: unreferenced
void CameraDump(DIRECTOR_CAMERA *camera)
{
	char buffer[128];

	LogQuick("CameraDump....");

	sprintf(buffer, "Camera type is %d(%s)", camera->type, GetCameraTypeName(camera->type));
	LogQuick(buffer);

	sprintf(buffer, "Camera position is x = %d, y = %d, z = %d", camera->position[0], camera->position[1], camera->position[2]);
	LogQuick(buffer);

	sprintf(buffer, "Holding camera position is x = %d, y = %d, z = %d",
			camera->holding_position[0], camera->holding_position[1], camera->holding_position[2]);
	LogQuick(buffer);

	sprintf(buffer, "Camera angle is %d, %d, %d", camera->angle.vx, camera->angle.vy, camera->angle.vz);
	LogQuick(buffer);

	sprintf(buffer, "Camera distance is %d", camera->distance);
	LogQuick(buffer);

	sprintf(buffer, "Cameras maximum distance is %d", camera->max_distance);
	LogQuick(buffer);

	LogQuick("End of Camera dump\n");
}

// [0x4c70ed] [D]
const char *GetCameraTypeName(int type)
{
	switch (type)
	{
		case CAMERA_FOLLOW:
			return "Chase camera";
		case CAMERA_TRIPOD:
			return "Flying camera";
		case CAMERA_INCAR:
			return "In car camera";
		case CAMERA_REPLAY_FIXED:
			return "Static camera";
		case CAMERA_REPLAY_TARGET:
			return "Watch car camera";
	}

	return "WARNING! Unknown camera type.";
}

// [0x4c7165] [D]
// A random camera for the automatic director up to the next waypoint: a chase camera at a random
// angle and distance, a camera watching the player from between the waypoints, or in a pursuing
// cop car. Placed a random height above the ground; an in car camera off the unpacked map.
void CreateNewAutoCamera(DIRECTOR_CAMERA *camera)
{
	WAYPOINT *waypoint;
	float distance;
	float x;
	float z;
	int frames;
	int ground;
	int height;

	ClearMem(camera, sizeof(DIRECTOR_CAMERA));

	waypoint = FindWaypoint(Director.FrameIndex);

	if (waypoint == NULL)
	{
		LogQuick("CreateNewAutoCamera: No waypoint found, function failed.");
		return;
	}

	if (Director.FrameIndex >= waypoint->frame)
	{
		camera->type = CAMERA_INCAR;

		MakeChaseCamera(camera);
		return;
	}

	frames = waypoint->frame - Director.FrameIndex;

	camera->field_44 = 1;
	camera->target = 0;

	switch (rand() & 7)
	{
		case 0:
			camera->type = CAMERA_FOLLOW;
			camera->turnAngle = -rand() & 0xfff;
			camera->distance = (rand() & 0x3ff) + 2000;
			camera->distance += (rand() & 0x3ff) >> 1;
			camera->max_distance = camera->distance;
			camera->camerapos[1] = -((u_char)rand() + 100);

			distance = (float)camera->distance;
			x = -fsintab[-camera->turnAngle & 0xfff] * distance;
			z = distance * fsintab[(-camera->turnAngle & 0xfff) + 1024];

			camera->position[0] = float_to_int(x) + car_data[0].location.vx;
			camera->position[2] = float_to_int(z) + car_data[0].location.vz;
			break;
		case 1:
		case 2:
			camera->type = CAMERA_REPLAY_TARGET;
			break;
		case 3:
			camera->perspective = 1;
			// fall through
		case 4:
			camera->target = 0;
			camera->type = CAMERA_REPLAY_TARGET;
			break;
		case 5:
			NextPursuerInView(0);
			camera->type = CAMERA_INCAR;
			camera->target = NextPursuerInView(camera->target);
			break;
		case 6:
			camera->perspective = 1;
			// fall through
		case 7:
			camera->type = CAMERA_REPLAY_TARGET;
			break;
	}

	if (frames + Director.FrameIndex < Director.NumFrames)
	{
		if (camera->type == CAMERA_REPLAY_TARGET)
		{
			camera->position[0] = waypoint[-1].x + (waypoint->x - waypoint[-1].x) / 2;
			camera->position[2] = waypoint[-1].z + (waypoint->z - waypoint[-1].z) / 2;
		}
		else if (camera->type != CAMERA_FOLLOW)
		{
			camera->position[0] = waypoint->x;
			camera->position[2] = waypoint->z;
		}
	}
	else
	{
		camera->type = CAMERA_FOLLOW;
		camera->target = 0;
		LogQuick("Camera changed to type 0 because went beyond replay length.");
	}

	ground = -MapHeightInt((IVECTOR *)camera->position);

	// NOTE: the original also reads the ground height under the target car here and drops the result
	MapHeightInt(&car_data[camera->target].location);

	if (GameLevel == 4)
		camera->position[1] = ground - 200;
	else
		camera->position[1] = ground - 70 - (rand() & 0x3ff);

	if (camera->position[1] > ground)
		camera->position[1] = ground - 100;

	height = abs(camera->position[1] - ground);

	if (height > 1600)
		camera->position[1] = ground - 1600;

	if (valid_region(camera->position[0], camera->position[2]) == 0
		&& camera->type != CAMERA_FOLLOW && camera->type != CAMERA_INCAR)
	{
		camera->type = CAMERA_INCAR;
		camera->target = 0;
	}

	memcpy(camera->holding_position, camera->position, sizeof(LONGVECTOR4));
}

// [0x4c75ad] [D]
// The next pursuing cop car after id within 30000 of the player, 0 (the player) when none.
int NextPursuerInView(int id)
{
	CIV_STATE *civ;
	int dx;
	int dz;
	int i;

	id++;
	civ = &civilian_ai_data[id - 1];

	for (i = 0; i < 64; i++)
	{
		if (id >= 65)
			return 0;

		if (civ->IsCopCar && civ->model == 1 && civ->ctrlState == 4)
		{
			dx = car_data[0].location.vx - car_data[id].location.vx;
			dz = car_data[0].location.vz - car_data[id].location.vz;

			if (dz * dz + dx * dx < 30000 * 30000)
				return id;
		}

		civ++;
		id++;
	}

	return 0;
}

// [0x4c7690] [D]
// The number of pursuing cop cars within 30000 of the player.
int PursuersInView(void)
{
	CIV_STATE *civ;
	int dx;
	int dz;
	int i;
	int count;

	count = 0;
	civ = civilian_ai_data;

	for (i = 0; i < 64; i++)
	{
		if (civ->IsCopCar && civ->model == 1 && civ->ctrlState == 4)
		{
			dx = car_data[0].location.vx - car_data[i + 1].location.vx;
			dz = car_data[0].location.vz - car_data[i + 1].location.vz;

			if (dz * dz + dx * dx < 30000 * 30000)
				count++;
		}

		civ++;
	}

	return count;
}

// [0x4c7758] [D]
// Fixes a camera that lost its car or is too far from the player; returns 1 when it changed it.
// NOTE: unreferenced
int sub_4C7758(DIRECTOR_CAMERA *camera)
{
	CAR_DATA *cp;
	int dx;
	int dz;
	int dist;

	if (camera->type == CAMERA_FOLLOW)
		return 0;

	if (camera->type == CAMERA_INCAR)
	{
		if (camera->target == 0)
			return 0;

		if (civilian_ai_data[camera->target - 1].ctrlState == 0)
		{
			camera->target = 0;
			camera->position[1] = -300;
			return 1;
		}
	}

	if (ReplayMode == 3)
		cp = &car_data[65];
	else
		cp = &car_data[0];

	dx = abs(cp->location.vx - camera->position[0]);
	dz = abs(cp->location.vz - camera->position[2]);

	if (dx >= dz)
		dist = dz / 2 + dx;
	else
		dist = dx / 2 + dz;

	if (dist > 35000)
	{
		camera->target = 0;
		camera->type = CAMERA_FOLLOW;
		camera->position[1] = -300;
		return 1;
	}

	if (valid_region(camera->position[0], camera->position[2]))
		return 0;

	camera->target = 0;
	camera->type = CAMERA_INCAR;
	return 1;
}

// [0x4c78b9] [D]
// Whether x z is on a road in one of the unpacked map regions: the region number + 1, else 0.
int valid_region(int x, int z)
{
	int cell_x;
	int cell_z;
	int region;

	if (ROADS_GetRoadInfo(x, z) == 0)
		return 0;

	if (RouteData.type == 0)
		return 0;

	cell_x = (x + RoadMapUnitXMid + 750) / 1500;
	cell_z = (RoadMapUnitZMid + 750 - z) / 1500;

	if (cell_x < 0 || cell_x >= RoadMapWidth || cell_z < 0 || cell_z >= RoadMapHeight)
		return 0;

	region = cell_x / road_region_size + regions_across * (regions_down - 1 - cell_z / road_region_size);

	if (region == regions_unpacked[0] || region == regions_unpacked[1] || region == regions_unpacked[2] || region == regions_unpacked[3])
		return region + 1;

	return 0;
}

// [0x4c79e1] [D]
// Called by CameraCollisionCheck; always returns 1.
int ApplyDirectorCollision(int x, int z)
{
	ROADS_GetRoadOrJunction(x, z);
	return 1;
}

// [0x4c7a02] [D]
// Whether the camera collides with a cell object: not smashable, at least 200 across.
int CheckDirectorCameraCollide(CELL_OBJECT *cop)
{
	MODEL *model;

	model = model_pointers[cop->type];

	if (model->flags2 & MODEL_FLAG_SMASHABLE)
		return 0;

	return model->bounding_sphere2d >= 200.0f;
}

// [0x4c7a4a] [D]
// NOTE: unreferenced
void SetCamera(DIRECTOR_CAMERA *camera, int index)
{
	if (index >= 0 && index < Director.NumCameras)
		memcpy(Director.CameraList[index], camera, sizeof(DIRECTOR_CAMERA));
	else
		LogQuick("SetCamera: Cannot set camera contents, illegsl index supplied.");
}

// [0x4c7a92] [D]
DIRECTOR_CAMERA *GetCameraByIndex(int index)
{
	if (index < Director.NumCameras && index >= 0)
		return Director.CameraList[index];

	return NULL;
}

// [0x4c7ab8] [D]
// The camera starting exactly at frame_index.
DIRECTOR_CAMERA *GetCameraByFrameIndex(int frame_index)
{
	int i;

	for (i = 0; i < Director.NumCameras; i++)
	{
		if (Director.CameraList[i]->frame_index == frame_index)
			return Director.CameraList[i];
	}

	return NULL;
}

// [0x4c7b02] [D]
// The camera in use at frame_index: the last one starting at or before it.
DIRECTOR_CAMERA *GetLastCameraByFrameIndex(int frame_index)
{
	int i;

	for (i = 0; i < Director.NumCameras; i++)
	{
		if (Director.CameraList[i]->frame_index <= frame_index)
		{
			if (i == Director.NumCameras - 1)
				return Director.CameraList[i];

			if (Director.CameraList[i + 1]->frame_index > frame_index)
				return Director.CameraList[i];
		}
	}

	return NULL;
}

// [0x4c7b7c] [D]
// Renumbers the cameras by their place in the list.
void ReInitDirectorCameras(void)
{
	int i;

	for (i = 0; i < Director.NumCameras; i++)
		Director.CameraList[i]->index = i;
}

// [0x4c7bb4] [D]
// Stores a copy of camera starting at frame_index (replacing one starting there), sorted by frame.
// NOTE: void in IDA, but it ends with InsertCamera's result in eax and CheckForNewCamera uses it as
// the new camera, so it returns it here
DIRECTOR_CAMERA *AddDirectorCamera(DIRECTOR_CAMERA *camera, int frame_index)
{
	DIRECTOR_CAMERA *old;
	int i;

	old = GetCameraByFrameIndex(frame_index);

	if (old)
		DeleteCamera(old->index);

	camera->frame_index = frame_index;

	for (i = 0; i < Director.NumCameras; i++)
	{
		if (frame_index == Director.CameraList[i]->frame_index)
		{
			LogQuick("Director.c::AddDirectorCamera: Camera already exists.");
			return NULL;
		}

		if (frame_index < Director.CameraList[i]->frame_index)
			return InsertCamera(camera, i);
	}

	return InsertCamera(camera, -1);
}

// [0x4c7c62] [D]
// Inserts a copy of camera at index of the list (-1 or past the end: appends); returns the copy.
DIRECTOR_CAMERA *InsertCamera(DIRECTOR_CAMERA *camera, int index)
{
	DIRECTOR_CAMERA **list;
	DIRECTOR_CAMERA *copy;
	int new_index;
	int old;
	int i;

	if (index >= Director.NumCameras)
		index = -1;

	list = (DIRECTOR_CAMERA **)DebugMalloc(sizeof(DIRECTOR_CAMERA *) * (Director.NumCameras + 1), __FILE__, __LINE__);

	if (list == NULL)
	{
		LogQuick("InsertCamera: Error, unable to allocate memory for new camera list.");
		return NULL;
	}

	copy = (DIRECTOR_CAMERA *)DebugMalloc(sizeof(DIRECTOR_CAMERA), __FILE__, __LINE__);

	if (copy == NULL)
	{
		LogQuick("InsertCamera: Error, unable to allocate memory for new camera.");
		DebugFree(list);

		return NULL;
	}

	if (index == -1)
	{
		for (i = 0; i < Director.NumCameras; i++)
			list[i] = Director.CameraList[i];

		list[Director.NumCameras] = copy;
		new_index = Director.NumCameras++;
	}
	else
	{
		Director.NumCameras++;

		old = 0;

		for (i = 0; i < Director.NumCameras; i++)
		{
			if (i == index)
			{
				list[i] = copy;
				new_index = i;
			}
			else
			{
				list[i] = Director.CameraList[old++];
			}
		}
	}

	memcpy(copy, camera, sizeof(DIRECTOR_CAMERA));

	if (Director.CameraList)
		DebugFree(Director.CameraList);

	Director.CameraList = list;

	ReInitDirectorCameras();

	return Director.CameraList[new_index];
}

// [0x4c7e0d] [D]
void DestroyCameras(void)
{
	int i;

	for (i = 0; i < Director.NumCameras; i++)
		DebugFree(Director.CameraList[i]);

	if (Director.CameraList)
		DebugFree(Director.CameraList);

	Director.NumCameras = 0;
	Director.CameraList = NULL;
}

// [0x4c7e74] [D]
// Removes the camera at index from the list.
void DeleteCamera(int index)
{
	DIRECTOR_CAMERA **list;
	int num;
	int i;

	if (index >= Director.NumCameras || index < 0)
	{
		LogQuick("DeleteCamera: Error! Specified camera does not exist.");
		return;
	}

	if (Director.NumCameras <= 1)
	{
		DebugFree(Director.CameraList[index]);
		DebugFree(Director.CameraList);

		Director.Camera = NULL;
		Director.CameraList = NULL;
	}
	else
	{
		list = (DIRECTOR_CAMERA **)DebugMalloc(sizeof(DIRECTOR_CAMERA *) * (Director.NumCameras - 1), __FILE__, __LINE__);

		if (list == NULL)
		{
			LogQuick("DeleteCamera: Error! Unable to allocate memory for new camera list.");
			return;
		}

		num = 0;

		for (i = 0; i < Director.NumCameras; i++)
		{
			if (i != index)
			{
				list[num] = Director.CameraList[i];
				list[num]->index = num;
				num++;
			}
		}

		if (Director.Camera && Director.Camera->index == index)
			Director.Camera = NULL;

		DebugFree(Director.CameraList[index]);
		DebugFree(Director.CameraList);

		Director.CameraList = list;
	}

	Director.NumCameras--;

	ReInitDirectorCameras();
}

// [0x4c7fe1] [D]
// The camera starting nearest to frame_index.
// NOTE: as in the original the best distance is stored signed, so once a camera before frame_index
// is taken no later one can win
DIRECTOR_CAMERA *FindNextCamera(int frame_index)
{
	DIRECTOR_CAMERA *camera;
	int frame_change;
	int frames;
	int i;

	camera = NULL;
	frame_change = Director.NumFrames + 500;

	for (i = 0; i < Director.NumCameras; i++)
	{
		frames = Director.CameraList[i]->frame_index - frame_index;

		if (abs(frames) < frame_change)
		{
			frame_change = frames;
			camera = Director.CameraList[i];
		}
	}

	return camera;
}

// [0x4c8065] [D]
// NOTE: unreferenced
int sub_4C8065(DIRECTOR_CAMERA *camera, int frame_index)
{
	return abs(camera->frame_index - frame_index);
}

// [0x4c8090] [D]
// The automatic director's camera for this frame: a stored one starting here, else a new automatic
// one at each waypoint (unless a stored camera starts within WAYPOINT_FRAMES of it). Returns 1 with
// *ppCamera set when the camera changes.
int CheckForNewCamera(DIRECTOR_CAMERA **ppCamera)
{
	DIRECTOR_CAMERA camera;
	DIRECTOR_CAMERA *found;
	WAYPOINT *waypoint;

	if (CutSceneActive)
		return 0;

	found = GetCameraByFrameIndex(Director.FrameIndex);

	if (found)
	{
		memcpy(&Director.WorkCamera, found, sizeof(Director.WorkCamera));

		*ppCamera = found;
		return 1;
	}

	if (Director.CameraList == NULL)
	{
		CreateNewAutoCamera(&camera);

		if (Director.FrameIndex <= 3)
			*ppCamera = AddDirectorCamera(&camera, 2);
		else
			*ppCamera = AddDirectorCamera(&camera, Director.FrameIndex);

		return 1;
	}

	found = FindNextCamera(Director.FrameIndex);
	waypoint = FindNextWaypoint(Director.FrameIndex);

	if (waypoint == NULL)
		return 0;

	if (found && abs(found->frame_index - waypoint->frame) < WAYPOINT_FRAMES)
		return 0;

	if (Director.FrameIndex + WAYPOINT_FRAMES >= Director.NumFrames)
		return 0;

	CreateNewAutoCamera(&camera);
	*ppCamera = AddDirectorCamera(&camera, Director.FrameIndex);

	return 1;
}

// [0x4c81f7] [D]
// A chase or in car camera on a car that is no longer a pursuing cop goes back to the player (and
// is stored at this frame).
void CheckValidTargetCar(DIRECTOR_CAMERA *camera)
{
	CIV_STATE *ai;
	DIRECTOR_CAMERA *old;

	if (camera->type != CAMERA_FOLLOW && camera->type != CAMERA_INCAR)
		return;

	if (camera->target == 0 || camera->target == -1)
		return;

	ai = &civilian_ai_data[camera->target - 1];

	if (ai->IsCopCar && ai->model == 1)
		return;

	camera->target = 0;

	if (camera->type == CAMERA_FOLLOW)
	{
		camera->type = CAMERA_INCAR;
		MakeChaseCamera(camera);
	}

	old = GetCameraByFrameIndex(Director.FrameIndex);

	if (old)
		DeleteCamera(old->index);

	AddDirectorCamera(camera, Director.FrameIndex);
}

// [0x4c82be] [D]
// The camera timeline at the bottom of the screen: 608 frames around FrameIndex, a bar per camera
// coloured by its type, a tick every 12 frames and a longer one every 50.
void DrawCameraBar(int FrameIndex)
{
	DIRECTOR_CAMERA *camera;
	DIRECTOR_CAMERA *next;
	float frame;
	float start;
	float end;
	float x;
	int colour1;
	int colour2;
	int colour3;
	int colour4;
	int old_mask;
	int i;
	int j;

	if (Director.Preview == 0)
		return;

	old_mask = EnableDepthMask(0);

	frame = (float)FrameIndex;

	for (i = 0; i < Director.NumCameras; i++)
	{
		camera = GetCameraByIndex(i);
		next = GetCameraByIndex(i + 1);

		start = (float)camera->frame_index - 1.0f;

		if (next)
			end = (float)next->frame_index;
		else
			end = (float)Director.NumFrames;

		if (frame - 304.0f <= end && frame + 304.0f >= start)
		{
			if (frame - 304.0f > start)
				start = frame - 304.0f;

			if (frame + 304.0f < end)
				end = frame + 304.0f;

			switch (camera->type)
			{
				case CAMERA_FOLLOW:
					colour1 = 0xff0000;
					colour2 = 0x7f0000;
					colour3 = 0x3f0000;
					colour4 = 0x7f0000;
					break;
				case CAMERA_INCAR:
					colour1 = 0xff00;
					colour2 = 0x7f00;
					colour3 = 0x3f00;
					colour4 = 0x7f00;
					break;
				case CAMERA_REPLAY_FIXED:
					colour1 = 0xffff00;
					colour2 = 0x7f7f00;
					colour3 = 0x3f3f00;
					colour4 = 0x7f7f00;
					break;
				case CAMERA_REPLAY_TARGET:
					colour1 = 0x46ffff;
					colour2 = 0x237f7f;
					colour3 = 0x113f3f;
					colour4 = 0x237f7f;
					break;
				default:
					colour1 = 0;
					colour2 = 0;
					colour3 = 0;
					colour4 = 0;
					break;
			}

			start = start - (frame - 304.0f) + 16.0f;
			end = end - (frame - 304.0f) + 16.0f;

			DrawScaledGouraudBar(start, 432.0f, end - start, 32.0f, colour1, colour2, colour3, colour4);
		}
	}

	DrawScaledLine(320.0f, 432.0f, 320.0f, 464.0f, 0);
	DrawScaledLine(16.0f, 432.0f, 624.0f, 432.0f, 0);
	DrawScaledLine(16.0f, 464.0f, 624.0f, 464.0f, 0);
	DrawScaledLine(16.0f, 432.0f, 16.0f, 464.0f, 0);
	DrawScaledLine(624.0f, 432.0f, 624.0f, 464.0f, 0);

	for (j = FrameIndex - 354; j < FrameIndex + 354; j += 50)
	{
		x = (float)(50 * (j / 50) + 2 - (FrameIndex - 304)) + 16.0f;

		if (x >= 16.0f && x <= 624.0f)
			DrawScaledLine(x, 464.0f, x, 456.0f, 0);

		x = (float)(50 * (j / 50) + 14 - (FrameIndex - 304)) + 16.0f;

		if (x >= 16.0f && x <= 624.0f)
			DrawScaledLine(x, 464.0f, x, 460.0f, 0);

		x = (float)(50 * (j / 50) + 26 - (FrameIndex - 304)) + 16.0f;

		if (x >= 16.0f && x <= 624.0f)
			DrawScaledLine(x, 464.0f, x, 460.0f, 0);

		x = (float)(50 * (j / 50) + 38 - (FrameIndex - 304)) + 16.0f;

		if (x >= 16.0f && x <= 624.0f)
			DrawScaledLine(x, 464.0f, x, 460.0f, 0);
	}

	EnableDepthMask(old_mask);
}

// [0x4c8779] [D]
// Turns a camera into a chase camera on its target car: an in car camera goes to the default chase
// distance and height, any other keeps its place (as distance, height and angle).
void MakeChaseCamera(DIRECTOR_CAMERA *camera)
{
	CAR_DATA *cp;
	float dx;
	float dy;
	float dz;
	float dist;
	int distance;

	if (camera->target == -1)
		camera->target = 0;

	cp = &car_data[camera->target];

	camera->perspective = 0;

	if (camera->type != CAMERA_FOLLOW)
	{
		if (camera->type == CAMERA_INCAR)
		{
			camera->turnAngle = 2048;
			camera->distance = 1280;
			camera->camerapos[1] = -256;
		}
		else
		{
			dx = (float)(camera->holding_position[0] - cp->location.vx);
			dy = (float)(cp->location.vy + camera->holding_position[1]);
			dz = (float)(camera->holding_position[2] - cp->location.vz);

			dist = (float)sqrt(dx * dx + dz * dz);
			distance = float_to_int(dist);

			if (distance > 15000)
				distance = 15000;

			camera->distance = distance;
			memcpy(camera->position, camera->holding_position, sizeof(LONGVECTOR4));
			camera->camerapos[1] = float_to_int(dy);
			camera->turnAngle = (Arc_Tan(float_to_int(dx), float_to_int(dz)) - cp->direction) & 0xfff;

			UpdateCameraHeight(camera);
		}

		camera->yang = (camera->turnAngle + cp->direction) & 0xfff;
	}

	camera->max_distance = camera->distance;
	camera->type = CAMERA_FOLLOW;
}

// [0x4c9b20] [D]
// Resets the director for a new replay; the stored waypoints and temporary cameras are kept only
// when restarting the replay (exit_flag 4 and 5).
void InitialiseDirector(void)
{
	iNumWaypoints = 0;
	bWaypointsStarted = 0;

	Director.FrameIndex = 0;
	Director.CameraIndex = 0;

	ClearMem(&Director.WorkCamera, sizeof(Director.WorkCamera));

	Director.WorkCamera.index = -1;
	Director.Camera = NULL;
	Director.IconColor = 0xffff00;
	Director.Selection = DT_PLAYPAUSE;
	Director.DemoType = 4;
	Director.field_28 = 32;

	if (exit_flag != 4 && exit_flag != 5)
	{
		DeleteWaypoints();
		remove(TEMP_CAMERA_FILE);
	}
}

// [0x4c9bbf] [D]
void InitialiseDirectorMenus(void)
{
	InitialiseDirectorMenu(DirectorMainMenu);
	InitialiseDirectorMenu(DirectorPauseMenu);
}

// [0x4c9bde] [D]
void ShutdownDirector(void)
{
	DestroyCameras();
	DestroyWaypoints();
}

// [0x4c9bed] [D]
// The director's frame: runs the current mode with the replay keys, flashes the selected icon and
// applies the camera to the game camera.
void HandleDirector(void)
{
	DIRECTOR_CAMERA *camera;
	int padd;
	int selection;

	Director.FrameIndex = game_frame;

	if (CheckKey(DIK_PERIOD))
		bOverlaysDisabled = bOverlaysDisabled == 0;

	if (Director.Editing)
		GetCurrentCamera(&Director.WorkCamera);

	padd = ReplayDebounce();

	selection = Director.Selection;

	switch (Director.Mode)
	{
		case DM_EXIT:
			SaveWaypoints(game_frame);
			break;
		case DM_PLAY:
			if (Director.FrameIndex < 2)
				PlayDirectorReplay();
			else
				SetDirectorMode(DM_DEBOUNCE, Director.Selection, 0);
			break;
		case DM_REPLAYEDITOR:
			ReplayEditor();
			break;
		case DM_AUTOCAMERA:
			if (Director.NumFrames > Director.FrameIndex && !CheckKey(DIK_ESCAPE))
			{
				AutoDirector();
				ReplayMenuHandler(padd);
				CheckValidTargetCar(&Director.WorkCamera);
			}
			else
			{
				SetDirectorMode(DM_PLAY, DT_AUTOCAM, 0);
			}
			break;
		case DM_PLAYPAUSE:
			if (Director.NumFrames > Director.FrameIndex && !CheckKey(DIK_ESCAPE))
			{
				PlayDirectorReplay();
				ReplayMenuHandler(padd);
				CheckValidTargetCar(&Director.WorkCamera);
			}
			else
			{
				SetDirectorMode(DM_PLAY, DT_PLAYPAUSE, 0);
			}
			break;
		case DM_FRAMEADVANCE:
			PlayDirectorReplay();
			SetDirectorMode(DM_PLAY, DT_FRAME_ADVANCE, 0);
			break;
		case DM_PLAYCAMERA:
			if (Director.NumFrames <= Director.FrameIndex || CheckKey(DIK_ESCAPE))
			{
				SetDirectorMode(DM_PLAY, DT_PLAYCAM, 0);
			}
			else
			{
				PlayDirectorReplay();

				if (Director.WorkCamera.frame_index == Director.FrameIndex)
					SetDirectorMode(DM_PLAY, DT_PLAYCAM, 0);
				else
					ReplayMenuHandler(padd);

				CheckValidTargetCar(&Director.WorkCamera);
			}
			break;
		case DM_DELETE:
		case DM_UNKNOWN:
			if (Director.NumFrames > Director.FrameIndex && !CheckKey(DIK_ESCAPE))
			{
				PlayDirectorReplay();
				CheckValidTargetCar(&Director.WorkCamera);
			}
			else
			{
				DestroyCameras();
				DestroyWaypoints();
				SetDirectorMode(DM_EXIT, 0, 0);
				wanted_exit_flag = 1;
				quit = 0;
				PauseGame(0);
			}
			break;
		case DM_MOVECAMERA:
			MoveDirectorCamera(padd);
			break;
		case DM_EDITCAMERA:
			PositionDirectorCamera(padd);
			break;
		case DM_ROTATECAMERA:
			RotateDirectorCamera(padd);
			break;
		case DM_AUTODIRECTOR:
			if (Director.NumFrames <= Director.FrameIndex || CheckKey(DIK_ESCAPE))
			{
				SaveDirector(TEMP_CAMERA_FILE);
				SetDirectorMode(DM_EXIT, 0, 0);
				PauseGame(1);
			}
			else
			{
				AutoDirector();
				ReplayMenuHandler(padd);
				CheckValidTargetCar(&Director.WorkCamera);
			}
			break;
		case DM_DEBOUNCE:
			ReplayMenuHandler(padd);
			break;
		case DM_PLAYREPLAY:
			if (Director.NumFrames <= Director.FrameIndex || CheckKey(DIK_ESCAPE))
			{
				SaveDirector(TEMP_CAMERA_FILE);
				SetDirectorMode(DM_EXIT, 0, 0);
				PauseGame(1);
			}
			else
			{
				PlayDirectorReplay();
				ReplayMenuHandler(padd);
				CheckValidTargetCar(&Director.WorkCamera);
			}
			break;
		case DM_PLAYREPLAY_2:
			if (Director.NumFrames <= Director.FrameIndex || CheckKey(DIK_ESCAPE))
			{
				SetDirectorMode(DM_EXIT, DT_PLAYPAUSE, 0);
				PauseGame(1);
			}
			else
			{
				PlayDirectorReplay();
				CheckValidTargetCar(&Director.WorkCamera);
			}
			break;
		default:
			LogQuick("HandleDirector: Error! - Unknown control type for replays!");
			break;
	}

	if (++Director.DemoTimer >= 64)
		Director.DemoTimer = 0;

	if (++Director.FlashTimer >= 32)
		Director.FlashTimer = 0;

	if (Director.Selected)
	{
		Director.FlashPhase = (u_char)(Director.FlashPhase + 5);

		if (Director.FlashPhase & 0x80)
			Director.IconColor = (((Director.FlashPhase & 0x7f) + 128) << 8) | (((Director.FlashPhase & 0x7f) + 128) << 16);
		else
			Director.IconColor = ((255 - Director.FlashPhase) << 8) | ((255 - Director.FlashPhase) << 16);
	}
	else if (Director.FlashTimer >= 16)
	{
		Director.IconColor = 0xff8b00;
	}
	else
	{
		Director.IconColor = 0xffff00;
	}

	if (selection != Director.Selection)
		Director.FlashTimer = 0;

	CheckCameraDistance(&Director.WorkCamera);

	if (Director.Editing)
	{
		SetCurrentCamera(&Director.WorkCamera);
	}
	else
	{
		camera = GetCameraByFrameIndex(Director.FrameIndex);

		if (camera)
		{
			SetCurrentCamera(camera);
			GetCurrentCamera(&Director.WorkCamera);
		}
	}

	InitialiseEnabledItems();
}

// [0x4ca13e] [D]
// Draws the replay menu and camera bar (unless switched off with the period key) and the
// "demonstration" text.
void DrawReplayInterface(void)
{
	if (bOverlaysDisabled)
		return;

	switch (Director.Mode)
	{
		case DM_PLAY:
		case DM_PLAYPAUSE:
		case DM_PLAYCAMERA:
		case DM_MOVECAMERA:
		case DM_EDITCAMERA:
		case DM_ROTATECAMERA:
		case DM_DEBOUNCE:
			DrawReplayMenu();
			DrawCameraBar(Director.FrameIndex);
			break;
		case DM_AUTOCAMERA:
			if (Director.Waypoints)
			{
				DrawReplayMenu();
				DrawCameraBar(Director.FrameIndex);
			}
			else
			{
				SetDirectorMode(DM_PLAY, DT_AUTOCAM, 0);
			}
			break;
		case DM_FRAMEADVANCE:
			DrawCameraBar(Director.FrameIndex);
			SetTextColour(255, 255, 255);
			DrawReplayMenu();
			break;
	}

	DisplayDemonstration();
}

// [0x4ca262] [D]
// NOTE: unreferenced
int GetDirectorMode(void)
{
	return Director.Mode;
}

// [0x4ca26c] [D]
// Switches the director mode: pauses or runs the game and the sounds, picks the menu, loads the
// waypoints for the automatic director. Returns the old mode.
int SetDirectorMode(int mode, int selection, int debug)
{
	int old_mode;

	bOverlaysDisabled = 0;

	Director.Selected = 0;
	Director.FlashPhase = 0;
	Director.Debug = debug;

	gInPresentation = 0;

	old_mode = Director.Mode;

	Director.Debounce = 1;
	Director.field_3C = 0;

	if (mode == DM_PLAY)
		PauseSounds();

	srand(GetTicksMs());

	switch (mode)
	{
		case DM_EXIT:
			if (selection != 1)
				ShutdownDirector();

			Director.Mode = mode;
			Director.FollowCar = 1;
			Director.Menu = NULL;
			Director.field_48 = 0;
			Director.Preview = 0;
			Director.Attract = 0;
			Director.EditCamera = 0;

			if (selection != 1)
			{
				PauseGame(0);
				gInPresentation = 0;
				UnPauseSounds();
			}
			break;
		case DM_PLAY:
		case DM_DEBOUNCE:
			PauseGame(1);

			Director.Mode = mode;
			Director.FollowCar = 0;
			Director.Menu = DirectorMainMenu;
			Director.field_48 = 1;
			Director.Preview = 1;
			Director.Attract = 0;
			Director.EditCamera = 0;

			if (selection == -1)
				Director.Selection = DT_PLAYPAUSE;
			else
				Director.Selection = selection;

			gInPresentation = 1;
			break;
		case DM_REPLAYEDITOR:
			PauseGame(1);

			Director.Mode = mode;
			Director.FollowCar = 0;
			Director.Menu = NULL;
			Director.field_48 = 0;
			Director.Preview = 1;
			Director.Attract = 0;
			Director.EditCamera = 1;

			gInPresentation = 1;
			break;
		case DM_AUTOCAMERA:
			if (Director.FrameIndex >= Director.NumFrames - 1)
			{
				SetDirectorMode(DM_PLAY, DT_AUTOCAM, 0);
				break;
			}

			PauseGame(0);

			Director.Mode = mode;
			Director.FollowCar = 1;
			Director.Menu = DirectorPauseMenu;
			Director.field_48 = 0;
			Director.Preview = 1;
			Director.Attract = 0;
			Director.Selection = DT_UNKNOWN;
			Director.EditCamera = 0;

			if (Director.Waypoints == NULL)
				LoadWaypoints();

			gInPresentation = 0;
			UnPauseSounds();
			break;
		case DM_PLAYPAUSE:
			if (Director.FrameIndex >= Director.NumFrames - 1)
			{
				SetDirectorMode(DM_PLAY, DT_PLAYPAUSE, 0);
				break;
			}

			PauseGame(0);

			Director.Mode = mode;
			Director.FollowCar = 1;
			Director.Menu = DirectorPauseMenu;
			Director.field_48 = 0;
			Director.Preview = 1;
			Director.Attract = 0;
			Director.Selection = DT_UNKNOWN;
			Director.EditCamera = 0;

			gInPresentation = 0;

			UnPauseSounds();
			break;
		case DM_FRAMEADVANCE:
			if (Director.FrameIndex >= Director.NumFrames - 1)
			{
				SetDirectorMode(DM_PLAY, DT_FRAME_ADVANCE, 0);
			}
			else
			{
				PauseGame(0);

				Director.Mode = mode;
				Director.FollowCar = 0;
				Director.field_48 = 1;
				Director.Preview = 1;
				Director.Attract = 0;
				Director.EditCamera = 0;
			}

			gInPresentation = 0;
			break;
		case DM_PLAYCAMERA:
			if (Director.FrameIndex < Director.NumFrames - 1)
			{
				PauseGame(0);

				Director.Mode = mode;
				Director.FollowCar = 1;
				Director.Menu = DirectorPauseMenu;
				Director.field_48 = 0;
				Director.Preview = 1;
				Director.Attract = 0;
				Director.Selection = DT_UNKNOWN;
				Director.EditCamera = 0;

				gInPresentation = 0;
			}
			break;
		case DM_DELETE:
			PauseGame(0);

			Director.Mode = mode;
			Director.FollowCar = 1;
			Director.Menu = NULL;
			Director.field_48 = 0;
			Director.Preview = 0;
			Director.Attract = 1;
			Director.DemoType = 0;
			Director.EditCamera = 0;

			gInPresentation = 0;

			UnPauseSounds();
			break;
		case DM_MOVECAMERA:
		case DM_ROTATECAMERA:
			PauseGame(1);

			Director.Mode = mode;
			Director.FollowCar = 0;
			Director.Menu = DirectorMainMenu;
			Director.field_48 = 1;
			Director.Preview = 1;
			Director.Attract = 0;
			Director.field_3C = 1;
			Director.Debounce = 0;
			Director.Selected = 1;
			Director.Moving = 1;

			gInPresentation = 1;

			Director.EditCamera = 1;
			break;
		case DM_EDITCAMERA:
			PauseGame(1);

			Director.Mode = mode;
			Director.FollowCar = 0;
			Director.Menu = DirectorMainMenu;
			Director.field_48 = 1;
			Director.Preview = 1;
			Director.Attract = 0;
			Director.field_3C = 1;
			Director.Debounce = 0;
			Director.Selected = 1;
			Director.Moving = 1;

			gInPresentation = 1;

			Director.EditCamera = 0;
			break;
		case DM_AUTODIRECTOR:
		case DM_PLAYREPLAY:
			if (Director.FrameIndex >= Director.NumFrames - 1)
			{
				SetDirectorMode(DM_EXIT, 0, 0);
				PauseGame(1);
				break;
			}

			PauseGame(0);

			Director.FollowCar = 1;
			Director.Mode = mode;
			Director.Menu = NULL;
			Director.field_48 = 0;
			Director.Preview = 0;
			Director.Attract = 0;
			Director.Selection = 0;
			Director.EditCamera = 0;

			if (Director.Waypoints == NULL)
				LoadWaypoints();

			gInPresentation = 0;
			UnPauseSounds();
			break;
		case DM_UNKNOWN:
			PauseGame(0);

			Director.Mode = mode;
			Director.FollowCar = 1;
			Director.Menu = NULL;
			Director.field_48 = 0;
			Director.Preview = 0;
			Director.Attract = 0;
			Director.DemoType = 4;
			Director.EditCamera = 0;

			gInPresentation = 0;

			UnPauseSounds();
			break;
		case DM_PLAYREPLAY_2:
			PauseGame(0);

			Director.Mode = mode;
			Director.FollowCar = 1;
			Director.Menu = NULL;
			Director.field_48 = 0;
			Director.Preview = 0;
			Director.Attract = 1;
			Director.DemoType = 2;
			Director.EditCamera = 0;

			gInPresentation = 0;

			UnPauseSounds();
			break;
		default:
			LogQuick("SetDirectorMode: Illegal mode identifier provided, function failed.\n");
			break;
	}

	return old_mode;
}

// [0x4caa3f] [D]
// Starts editing: keeps the game camera to restore on cancel and copies it into the work camera.
void StartDirectorModify(void)
{
	if (Director.Editing)
		return;

	Director.Editing = 1;
	GetCurrentCamera(&Director.SaveCamera);
	GetCurrentCamera(&Director.WorkCamera);
}

// [0x4caa73] [D]
void FinishDirectorModify(void)
{
	if (Director.Editing)
		Director.Editing = 0;
}

// [0x4caa8d] [D]
// Cancels editing: puts back the game camera kept by StartDirectorModify.
void CancelDirectorModify(void)
{
	if (Director.Editing)
	{
		SetCurrentCamera(&Director.SaveCamera);
		Director.Editing = 0;
	}
}
