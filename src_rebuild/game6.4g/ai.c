/*
 * ai.c - car AI
 *
 * Rebuilt from Driver_sym2_94.i64 (IDA folder "ai.c"); partially translated.
 */

#include <stdio.h>
#include <string.h>

#include "dr_types.h"
#include "ai.h"
#include "dr_globals.h"
#include "MScript.h"
#include "general.h"
#include "handling.h"
#include "shell.h"
#include "flat/prescam.h"

// [0x48e865] [D]
// Clears memory a dword at a time, then the remaining bytes.
void ClearMem(void *mem, size_t size)
{
	char *ptr;

	ptr = (char *)mem;

	if (size >> 2)
	{
		memset(mem, 0, (size >> 2) * 4);
		ptr = (char *)mem + (size >> 2) * 4;
	}

	if (size & 3)
		memset(ptr, 0, size & 3);
}

// [0x4913f9] [D]
// Places the player's car: the motel camera, the interview start, a mission's start, the saved
// start position (rposname) or the replayed chase's cars.
void InitPlayerCarPosition(void)
{
	FILE *fp;
	LONGVECTOR4 startpos;
	char filename[64];

	if (gDoMotelRoom)
	{
		SetMotelCamera(&startpos);

		car_data[PlayerCar].location.vx = startpos[0];
		car_data[PlayerCar].location.vy = startpos[1];
		car_data[PlayerCar].location.vz = startpos[2];

		InitCarPhysics(&car_data[PlayerCar], &startpos);

		car_data[0].direction = 1024;
	}
	else if (gDoInterview)
	{
		if (GameMode == 201)
		{
			sprintf(filename, "ReplayData\Chase\%s.LEADER", MissionName);
			fp = fopen(filename, "rb");
			DriverFRead(&car_data[65], 1, sizeof(CAR_DATA), fp);
			fclose(fp);

			car_data[65].model = 0;

			sprintf(filename, "ReplayData\Chase\%s.PLAYER", MissionName);
			fp = fopen(filename, "rb");
			DriverFRead(car_data, 1, sizeof(CAR_DATA), fp);
			fclose(fp);

			car_data[0].model = ChaseCarType;
		}
		else
		{
			// setup interview spawn
			car_data[PlayerCar].location.vx = -10240;
			car_data[PlayerCar].location.vy = -500;
			car_data[PlayerCar].location.vz = -21504;

			startpos[0] = car_data[PlayerCar].location.vx;
			startpos[1] = car_data[PlayerCar].location.vy;
			startpos[2] = car_data[PlayerCar].location.vz;

			car_data[0].model = 0;

			InitCarPhysics(&car_data[PlayerCar], &startpos);

			car_data[0].direction = 1024;
		}
	}
	else if ((!gMissionRecording && !gMissionReplaying) || (gDoTraining && gTrainingMode == 2))
	{
		if (mission.useLastPlayerPosition)
		{
			startpos[0] = mission.player_position[0];
			startpos[1] = mission.player_position[1];
			startpos[2] = mission.player_position[2];

			InitCarPhysics(car_data, &startpos);

			car_data[0].direction = mission.player_rotation;
			car_data[0].model = ChaseCarType;

			TempBuildHandlingMatrix(car_data, 0);
		}
		else
		{
			fp = fopen(rposname, "rb");

			if (fp)
			{
				ClearMem(car_data, sizeof(CAR_DATA));

				DriverFRead(car_data, 1, sizeof(CAR_DATA), fp);
				fclose(fp);

				car_data[0].model = 0;
			}
			else
			{
				memset(startpos, 0, 12);

				car_data[0].model = 0;

				InitCarPhysics(car_data, &startpos);

				car_data[0].direction = 1024;

				TempBuildHandlingMatrix(car_data, 0);
			}
		}
	}
	else
	{
		sprintf(filename, "ReplayData\Chase\%s.LEADER", MissionName);
		fp = fopen(filename, "rb");
		DriverFRead(&car_data[65], 1, sizeof(CAR_DATA), fp);
		fclose(fp);

		car_data[65].model = 0;

		sprintf(filename, "ReplayData\Chase\%s.PLAYER", MissionName);
		fp = fopen(filename, "rb");
		DriverFRead(car_data, 1, sizeof(CAR_DATA), fp);
		fclose(fp);

		car_data[0].model = ChaseCarType;
	}

	if (gDoTraining && gTrainingMode == 2)
		car_data[65] = car_data[0];

	if (gMissionRecording || gMissionReplaying)
	{
		camera_position.vx = (float)car_data[65].location.vx;
		camera_position.vy = -400.0f;
		camera_position.vz = (float)car_data[65].location.vz;
	}
	else
	{
		camera_position.vx = (float)car_data[PlayerCar].location.vx;
		camera_position.vy = -400.0f;
		camera_position.vz = (float)car_data[PlayerCar].location.vz;
	}

	car_data[0].totalDamage = 0;
	car_data[0].damage[5] = 0;
	car_data[0].damage[4] = 0;
	car_data[0].damage[3] = 0;
	car_data[0].damage[2] = 0;
	car_data[0].damage[1] = 0;
	car_data[0].damage[0] = 0;
}
