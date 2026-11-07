/*
 * camera.h - globals and functions of camera.c used by other files
 */

#ifndef CAMERA_H
#define CAMERA_H

#include "dr_types.h"

// cameraview
#define CAMERA_FOLLOW			0
#define CAMERA_TRIPOD			1
#define CAMERA_INCAR			2
#define CAMERA_TOPDOWN			6		// GTA style, also the overhead view cheat
#define CAMERA_CUTSCENE			7
#define CAMERA_BUMPER			8
#define CAMERA_EXPLOSION		9
#define CAMERA_REPLAY_FIXED		16
#define CAMERA_REPLAY_TARGET	17

// globals
extern VECTOR4 player_car_incar_camera_offsets[18];
extern int CAMERA_START_ELEVATION;
extern int CAMERA_DELTA;
extern char cameraback;
extern int TargetCar;
extern int CameraInCar;
extern char camerarotateleft;
extern int gCameraAngle;
extern char camera_has_changed;
extern VECTOR4 holding_camera_position;
// most references: flat/prescam.c 8, camera.c 5
extern CAMERA_POINT control_points[32];
extern int current_camera_yangle;
extern int current_camera_xangle;
extern float gCameraMaxDistance;
extern int gHeadTurn;
// most references: general.c 9, Director.c 8, camera.c 5
extern char cameraview;
extern char lastcameraview;
extern char camerarotateright;
extern int gHeadTurnTarget;
extern float gCameraDistance;
extern char cameraforward;
extern SVECTOR no_shake_camera;
extern VECTOR4 CameraPos;
extern SVECTOR holding_camera_angle;
extern char CamPerspectiveMode;
extern MATRIX inv_camera_matrix;
extern MATRIX camera_matrix;

// functions
extern BOOL CameraCollisionCheck(int ground);
extern int CameraMoveAllowed(float x, float y, float z);
extern void ControlCamera(int drawing);
extern void FBEndOfFrame(void);
extern void InitCameraGlobals(void);
extern void InitialiseFlyBy(void);
extern int LoadFlyByData(char *text);
extern CAR_DATA *PlaceCameraInCar(int id);
extern void QuaternionToWorldMatrix(QUATERNION *q);
extern void RunFlyBy(void);
extern void SetCameraOnSpline(int frame, CAMERA_POINT *points);

#endif // CAMERA_H
