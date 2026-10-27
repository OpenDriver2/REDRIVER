/*
 * DirectorStorage.c - replay director cameras and waypoints storage
 *
 * Rebuilt from Driver_sym2_94.i64 (IDA folder "DirectorStorage.c"); partially translated.
 */

#include <stdio.h>

#include "Director.h"
#include "DirectorStorage.h"

// [0x4cb9ac] [D]
void DeleteWaypoints(void)
{
	remove("Scripts\\Waypoint.tmp");

	bWaypointsStarted = 0;
	iNumWaypoints = 0;
}
