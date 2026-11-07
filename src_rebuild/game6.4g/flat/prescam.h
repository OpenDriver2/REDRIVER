/*
 * flat/prescam.h - globals and functions of flat/prescam.c used by other files
 */

#ifndef FLAT_PRESCAM_H
#define FLAT_PRESCAM_H

#include "dr_types.h"

// globals
extern int gCameraOption;
extern int gCameraMoving;

// functions
extern void FollowCameraPath(void);
extern void FreeMotelCameras(void);
extern void InitMotelCameras(int level);
extern void MoveCameraTo(int camera);
extern void PlaceCameraAt(int camera);
extern void ResetCamera(void);
extern void SetMotelCamera(LONGVECTOR4 *position);

#endif // FLAT_PRESCAM_H
