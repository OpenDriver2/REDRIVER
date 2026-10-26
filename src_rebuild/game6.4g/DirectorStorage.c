/*
 * DirectorStorage.c - replay director cameras and waypoints storage
 *
 * Rebuilt from Driver_sym2_94.i64 (IDA folder "DirectorStorage.c"); partially translated.
 */

#include <stdio.h>

extern int bWaypointsStarted;		// @Director.c
extern int iNumWaypoints;			// @Director.c

// [0x4cb9ac] [D]
void DeleteWaypoints(void)
{
	remove("Scripts\\Waypoint.tmp");

	bWaypointsStarted = 0;
	iNumWaypoints = 0;
}
