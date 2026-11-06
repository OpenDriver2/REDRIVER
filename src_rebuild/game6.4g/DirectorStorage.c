/*
 * DirectorStorage.c - replay director cameras and waypoints storage
 *
 * Rebuilt from Driver_sym2_94.i64 (IDA folder "DirectorStorage.c").
 * The original path was D:\Driver\game6.4g\DirectorStorage.c (from a DebugMalloc call).
 *
 * A director file (ReplayData\User\<name>.CAMS) is a "DRLP" chunk holding the chunks "RPDR"
 * (the replay's frame count), "WYPT" (the waypoints) and "RCAM" (the cameras). Each chunk starts
 * with a DIRECTOR_CHUNK header whose size counts the header too. While a game is played,
 * SaveWaypoints appends the player's position every 150 frames to Scripts\Waypoint.tmp;
 * LoadWaypoints moves them into the director.
 */

#include <stdio.h>

#include "dr_types.h"
#include "Director.h"
#include "DirectorStorage.h"
#include "dr_globals.h"
#include "ai.h"
#include "debug.h"
#include "general.h"
#include "handling.h"
#include "glaunch.h"

// NOTE: the name is mine; the layout is the 12 bytes every chunk read and write uses
typedef struct DIRECTOR_CHUNK
{
	int id;
	int size;		// bytes, the header included
	int version;
} DIRECTOR_CHUNK;

// a camera as version 1 of the "RCAM" chunk stores it (60 bytes)
// NOTE: only the fields ReadReplayCameraDataV1 copies are known; the names are the
// DIRECTOR_CAMERA fields they go to
typedef struct DIRECTOR_CAMERA_V1
{
	int type;
	int unk_04;
	int distance;
	int max_distance;
	int target;
	int frame_index;
	int turnAngle;
	int unk_1c[4];
	LONGVECTOR4 position;
} DIRECTOR_CAMERA_V1;

// chunk ids (the four characters as they appear in the file)
#define CHUNK_DIRECTOR			0x504C5244	// "DRLP"
#define CHUNK_CAMERAS			0x4D414352	// "RCAM"
#define CHUNK_REPLAY			0x52445052	// "RPDR"
#define CHUNK_WAYPOINTS			0x54505957	// "WYPT"

#define WAYPOINT_FILE			"Scripts\\Waypoint.tmp"

int SaveDirector(char *filename);
int ReadDirectorData(FILE *fp);
int WriteDirectorData(FILE *fp);
int WriteReplayWaypointData(FILE *fp);
int WriteReplayCameraData(FILE *fp);
size_t WriteReplayDirectorData(FILE *fp);
void ReadReplayCameraData(FILE *fp);
void ReadReplayCameraDataV1(FILE *fp);
void SetDefaultCamera(void);
int LoadWaypoints(void);

// [0x4caea0] [D]
int SaveDirector(char *filename)
{
	FILE *fp;

	fp = fopen(filename, "wb");

	if (!fp)
	{
		LogQuick("SaveDirector: Unable to write director data file.  Please make sure target is not write protected.");
		return 0;
	}

	WriteDirectorData(fp);
	fclose(fp);

	return 1;
}

// [0x4caef0] [D]
int LoadDirector(char *filename)
{
	FILE *fp;

	fp = fopen(filename, "rb");

	if (!fp)
	{
		LogQuick("LoadDirector: Unable to open director file, please make sure it exists.");
		return 0;
	}

	ReadDirectorData(fp);
	fclose(fp);

	return 1;
}

// [0x4caf40] [D]
// Adds a chase camera behind the player from frame 2.
void SetDefaultCamera(void)
{
	DIRECTOR_CAMERA camera;

	ClearMem(&camera, sizeof(camera));

	camera.camerapos[1] = -256;
	camera.turnAngle = 2048;
	camera.distance = 1280;
	camera.max_distance = 1280;

	AddDirectorCamera(&camera, 2);
}

// [0x4caf91] [D]
// Moves the recorded waypoints into the director and saves it.
void SaveDirectorCameras(char *filename)
{
	if (!bWaypointsStarted && !iNumWaypoints)
		return;

	LoadWaypoints();

	Director.NumFrames = game_frame - 4;

	SaveDirector(filename);
}

// [0x4cafc6] [D]
// Reads a "DRLP" chunk; leaves the file after it even when it is not one.
int ReadDirectorData(FILE *fp)
{
	DIRECTOR_CHUNK header;
	DIRECTOR_CHUNK chunk;
	int start;
	int chunk_start;
	int pos;
	int num_frames;
	int num_waypoints;

	start = ftell(fp);

	fread(&header, sizeof(header), 1, fp);

	if (header.id == CHUNK_DIRECTOR && header.version == 1)
	{
		pos = sizeof(header);

		DestroyCameras();
		DestroyWaypoints();

		while (pos < header.size)
		{
			chunk_start = ftell(fp);

			fread(&chunk, sizeof(chunk), 1, fp);

			switch (chunk.id)
			{
				case CHUNK_CAMERAS:
					if (chunk.version == 2)
						ReadReplayCameraData(fp);
					else if (chunk.version == 1)
						ReadReplayCameraDataV1(fp);
					else
						LogQuick("ReadDirectorData: Incompatible replay camera data chunk found.");
					break;

				case CHUNK_REPLAY:
					if (chunk.version == 1)
					{
						fread(&num_frames, 4, 1, fp);
						Director.NumFrames = num_frames;
					}
					else
					{
						LogQuick("ReadDirectorData: Incompatible director data chunk found.");
					}
					break;

				case CHUNK_WAYPOINTS:
					if (chunk.version == 1)
					{
						fread(&num_waypoints, 4, 1, fp);

						if (num_waypoints > 0)
						{
							DestroyWaypoints();

							Director.NumWaypoints = num_waypoints;
							// NOTE: the binary passes "D:\\Driver\\game6.4g\\DirectorStorage.c", 216 here
							Director.Waypoints = (WAYPOINT *)DebugMalloc(num_waypoints * sizeof(WAYPOINT), __FILE__, __LINE__);

							if (Director.Waypoints)
								fread(Director.Waypoints, sizeof(WAYPOINT), Director.NumWaypoints, fp);
							else
								Director.NumWaypoints = 0;
						}
					}
					else
					{
						LogQuick("ReadDirectorData: Incompatible waypoint data chunk found.");
					}
					break;

				default:
					LogQuick("ReadDirectorData: Error, unknown chunk ID.  Skipping");
					break;
			}

			fseek(fp, chunk_start, SEEK_SET);
			fseek(fp, chunk.size, SEEK_CUR);

			pos += chunk.size;
		}
	}

	fseek(fp, start, SEEK_SET);

	return fseek(fp, header.size, SEEK_CUR);
}

// [0x4cb201] [D]
int WriteDirectorData(FILE *fp)
{
	DIRECTOR_CHUNK header;
	int start;
	int end;

	start = ftell(fp);

	header.id = CHUNK_DIRECTOR;
	header.version = 1;

	// NOTE: the size is written uninitialised here and patched below, as in the original
	fwrite(&header, sizeof(header), 1, fp);

	WriteReplayDirectorData(fp);
	WriteReplayWaypointData(fp);
	WriteReplayCameraData(fp);

	end = ftell(fp);

	header.size = end - start;

	fseek(fp, start, SEEK_SET);
	fwrite(&header, sizeof(header), 1, fp);

	return fseek(fp, end, SEEK_SET);
}

// [0x4cb2b6] [D]
int WriteReplayWaypointData(FILE *fp)
{
	DIRECTOR_CHUNK chunk;
	int num_waypoints;
	int start;
	int end;

	start = ftell(fp);

	chunk.id = CHUNK_WAYPOINTS;
	chunk.version = 1;

	fwrite(&chunk, sizeof(chunk), 1, fp);

	num_waypoints = Director.NumWaypoints;

	fwrite(&num_waypoints, 4, 1, fp);

	if (num_waypoints)
		fwrite(Director.Waypoints, sizeof(WAYPOINT), num_waypoints, fp);

	end = ftell(fp);

	fseek(fp, start, SEEK_SET);

	chunk.size = end - start;

	fwrite(&chunk, sizeof(chunk), 1, fp);

	return fseek(fp, end, SEEK_SET);
}

// [0x4cb382] [D]
int WriteReplayCameraData(FILE *fp)
{
	DIRECTOR_CHUNK chunk;
	int num_cameras;
	int start;
	int end;
	int i;

	start = ftell(fp);

	chunk.id = CHUNK_CAMERAS;
	chunk.version = 2;

	fwrite(&chunk, sizeof(chunk), 1, fp);

	num_cameras = Director.NumCameras;

	fwrite(&num_cameras, 4, 1, fp);

	for (i = 0; i < Director.NumCameras; i++)
		fwrite(Director.CameraList[i], sizeof(DIRECTOR_CAMERA), 1, fp);

	end = ftell(fp);

	fseek(fp, start, SEEK_SET);

	chunk.size = end - start;

	fwrite(&chunk, sizeof(chunk), 1, fp);

	return fseek(fp, end, SEEK_SET);
}

// [0x4cb46e] [D]
size_t WriteReplayDirectorData(FILE *fp)
{
	DIRECTOR_CHUNK chunk;
	int num_frames;

	chunk.id = CHUNK_REPLAY;
	chunk.version = 1;
	chunk.size = sizeof(chunk) + 4;

	fwrite(&chunk, sizeof(chunk), 1, fp);

	num_frames = Director.NumFrames;

	return fwrite(&num_frames, 4, 1, fp);
}

// [0x4cb4be] [D]
// Reads version 2 of the "RCAM" chunk: the camera count, then the cameras as they are in memory.
void ReadReplayCameraData(FILE *fp)
{
	DIRECTOR_CAMERA **list;
	int num_cameras;
	int i;

	fread(&num_cameras, 4, 1, fp);

	if (!num_cameras)
		return;

	// NOTE: the binary passes "D:\\Driver\\game6.4g\\DirectorStorage.c", 396 here
	list = (DIRECTOR_CAMERA **)DebugMalloc(num_cameras * sizeof(DIRECTOR_CAMERA *), __FILE__, __LINE__);

	if (!list)
		return;

	ClearMem(list, num_cameras * sizeof(DIRECTOR_CAMERA *));

	for (i = 0; i < num_cameras; i++)
	{
		// NOTE: the binary passes "D:\\Driver\\game6.4g\\DirectorStorage.c", 405 here
		list[i] = (DIRECTOR_CAMERA *)DebugMalloc(sizeof(DIRECTOR_CAMERA), __FILE__, __LINE__);

		if (!list[i])
		{
			for (i = 0; i < num_cameras; i++)
			{
				if (list[i])
					DebugFree(list[i]);
			}

			DebugFree(list);
			return;
		}

		fread(list[i], sizeof(DIRECTOR_CAMERA), 1, fp);
	}

	DestroyCameras();

	Director.NumCameras = num_cameras;
	Director.CameraList = list;
}

// [0x4cb5f6] [D]
// Reads version 1 of the "RCAM" chunk (60-byte cameras) into DIRECTOR_CAMERAs.
void ReadReplayCameraDataV1(FILE *fp)
{
	DIRECTOR_CAMERA **list;
	DIRECTOR_CAMERA_V1 old;
	DIRECTOR_CAMERA *camera;
	int num_cameras;
	int i;

	fread(&num_cameras, 4, 1, fp);

	if (!num_cameras)
		return;

	// NOTE: the binary passes "D:\\Driver\\game6.4g\\DirectorStorage.c", 453 here
	list = (DIRECTOR_CAMERA **)DebugMalloc(num_cameras * sizeof(DIRECTOR_CAMERA *), __FILE__, __LINE__);

	if (!list)
		return;

	ClearMem(list, num_cameras * sizeof(DIRECTOR_CAMERA *));

	for (i = 0; i < num_cameras; i++)
	{
		// NOTE: the binary passes "D:\\Driver\\game6.4g\\DirectorStorage.c", 462 here
		camera = (DIRECTOR_CAMERA *)DebugMalloc(sizeof(DIRECTOR_CAMERA), __FILE__, __LINE__);
		list[i] = camera;

		if (!camera)
		{
			for (i = 0; i < num_cameras; i++)
			{
				if (list[i])
					DebugFree(list[i]);
			}

			DebugFree(list);
			return;
		}

		fread(&old, sizeof(old), 1, fp);

		ClearMem(camera, sizeof(DIRECTOR_CAMERA));

		camera->holding_position[0] = old.position[0];
		camera->holding_position[1] = old.position[1];
		camera->holding_position[2] = old.position[2];
		camera->holding_position[3] = old.position[3];

		camera->position[0] = old.position[0];
		camera->position[1] = old.position[1];
		camera->position[2] = old.position[2];
		camera->position[3] = old.position[3];

		camera->distance = old.distance;
		camera->max_distance = old.max_distance;
		camera->turnAngle = old.turnAngle;
		camera->target = old.target;
		camera->frame_index = old.frame_index;
		camera->type = old.type;
	}

	DestroyCameras();

	Director.NumCameras = num_cameras;
	Director.CameraList = list;
}

// [0x4cb7d4] [D]
// Every 150 frames appends the player's position (rounded down to 1500) to Scripts\Waypoint.tmp.
void SaveWaypoints(int frame)
{
	FILE *fp;
	WAYPOINT waypoint;

	bWaypointsStarted = 1;

	if (frame % 150 != 0)
		return;

	fp = fopen(WAYPOINT_FILE, "a+b");

	if (!fp)
		return;

	waypoint.frame = frame;
	waypoint.x = (car_data[0].location.vx / 1500) * 1500;
	waypoint.z = (car_data[0].location.vz / 1500) * 1500;

	fwrite(&waypoint, sizeof(waypoint), 1, fp);

	iNumWaypoints++;

	fclose(fp);
}

// [0x4cb880] [D]
// Moves the waypoints from Scripts\Waypoint.tmp into the director (with the default camera
// when it has none) and deletes the file; 0 when there is no file.
int LoadWaypoints(void)
{
	FILE *fp;
	WAYPOINT *waypoints;
	int start;
	int end;

	fp = fopen(WAYPOINT_FILE, "rb");

	if (!fp)
	{
		LogQuick("LoadWaypoints: Unable to open waypoint file.");
		return 0;
	}

	DestroyCameras();
	DestroyWaypoints();

	start = ftell(fp);
	fseek(fp, 0, SEEK_END);
	end = ftell(fp);
	fseek(fp, start, SEEK_SET);

	if ((unsigned int)(end - start) / sizeof(WAYPOINT))
	{
		// NOTE: the binary passes "D:\\Driver\\game6.4g\\DirectorStorage.c", 568 here
		waypoints = (WAYPOINT *)DebugMalloc((unsigned int)(end - start) / sizeof(WAYPOINT) * sizeof(WAYPOINT), __FILE__, __LINE__);

		if (!waypoints)
		{
			LogQuick("LoadWaypoints: Unable to allocate space for waypoint data.");
			fclose(fp);
			return 0;
		}

		Director.NumWaypoints = (unsigned int)(end - start) / sizeof(WAYPOINT);
		Director.Waypoints = waypoints;

		fread(waypoints, end - start, 1, fp);
	}

	fclose(fp);

	if (!Director.NumCameras)
		SetDefaultCamera();

	DeleteWaypoints();

	return 1;
}

// [0x4cb9ac] [D]
void DeleteWaypoints(void)
{
	remove(WAYPOINT_FILE);

	bWaypointsStarted = 0;
	iNumWaypoints = 0;
}
