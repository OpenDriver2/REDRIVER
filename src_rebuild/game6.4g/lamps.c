/*
 * lamps.c - street lamps of the level (the LEV lamps lump)
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "lamps.c").
 */

#include <stdio.h>
#include <string.h>

#include "dr_types.h"
#include "general.h"
#include "lamps.h"
#include "asmmath.h"
#include "handling.h"
#include "map.h"
#include "MScript.h"
#include "pause.h"
#include "shell.h"
#include "Engine/3dfx/object.h"

// per map sector: the number of lamps and a pointer to them
typedef int LAMP_SECTOR[2];

typedef struct GAMELAMP_HEADER
{
	char ident[4];		// "GLMP"
	float version;
	int sectors_wide;
	int sectors_high;
	int num_cells;
} GAMELAMP_HEADER;

void DestroyLamps(void);
int ReadLampFileChunk(FILE *file_chunk);

// NOTE: bss globals in the linker's common area; defined here as lamps.c is their main user
int num_total_lamps;
int lamp_frame;
LAMP_SECTOR *lamp_data;
GAMELAMP_HEADER lamp_header;

// [0x4ecd90] [D]
// At night adds the lamps of the map sectors around the player (13x13, 21x21 with gUltraLamps)
// and the mission lamps within 30000 of the player. Type 1 lamps flicker.
// NOTE: type 2 lamps of the sectors are never drawn: lamp_frame (0 - 15) is compared with 512
// (0x200 in the binary); the mission lamps of type 2 blink on lamp_frame 8 - 15
void ProcessLamps(void)
{
	LAMP_OUTPUT *lamp;
	int sector_x;
	int sector_z;
	int sector;
	int rows;
	int count;
	int num_sector_lamps;
	int num;
	int dx;
	int dz;

	if (!lamp_data || !gNight)
		return;

	if (!GamePaused)
		lamp_frame++;

	lamp_frame &= 15;

	sector_x = cells_across_halved + car_data[0].location.vx / cell_size;
	sector_z = cells_down_halved + car_data[0].location.vz / cell_size;

	if (gUltraLamps)
	{
		sector_x -= 10;
		sector_z -= 10;
		rows = 21;
	}
	else
	{
		sector_x -= 6;
		sector_z -= 6;
		rows = 13;
	}

	while (rows--)
	{
		sector = sector_x + lamp_header.sectors_wide * sector_z;

		if (gUltraLamps)
			count = 21;
		else
			count = 13;

		while (count--)
		{
			if (lamp_data[sector][1])
			{
				lamp = (LAMP_OUTPUT *)lamp_data[sector][1];
				num_sector_lamps = lamp_data[sector][0];

				while (num_sector_lamps--)
				{
					if (lamp->type == 1)
					{
						if (GamePaused || (rand() & 3))
							AddLamp(&lamp->position, (int)lamp->range, lamp->color.red, lamp->color.green, lamp->color.blue);
					}
					else if (lamp->type != 2 || lamp_frame >= 512)
					{
						AddLamp(&lamp->position, (int)lamp->range, lamp->color.red, lamp->color.green, lamp->color.blue);
					}

					lamp++;
				}
			}

			sector++;
		}

		sector_z++;
	}

	num = 5;
	lamp = &MissionLamps[4];

	while (num--)
	{
		if (MissionLampsStatus[num])
		{
			dx = float_to_int(lamp->position.vx) - car_data[0].location.vx;
			dz = float_to_int(lamp->position.vz) - car_data[0].location.vz;

			if (dz * dz + dx * dx < 900000000)
			{
				if (lamp->type == 1)
				{
					if (GamePaused || (rand() & 3))
						AddLamp(&lamp->position, (int)lamp->range, lamp->color.red, lamp->color.green, lamp->color.blue);
				}
				else if (lamp->type != 2 || lamp_frame >= 8)
				{
					AddLamp(&lamp->position, (int)lamp->range, lamp->color.red, lamp->color.green, lamp->color.blue);
				}
			}
		}

		lamp--;
	}
}

// [0x4ed0ad] [D]
// Mirrors the added lamps in z (for the rear view mirror).
void InvertLamps(void)
{
	GAMELAMP *lamp;
	int count;

	count = num_lamps;

	for (lamp = lamps; count--; lamp++)
		lamp->position.vz = -lamp->position.vz;
}

// [0x4ed0ef] [D]
int ProcessLampsLump(FILE *lump_file)
{
	return ReadLampFileChunk(lump_file);
}

// [0x4ed100] [D]
// NOTE: without the "GLMP" ident the file is put back at the lump start and the lump is not
// skipped, as in the original
int ReadLampFileChunk(FILE *file_chunk)
{
	int sector[2];
	int index;
	long start;
	int num_cells;

	start = ftell(file_chunk);
	fread(&lamp_header, sizeof(GAMELAMP_HEADER), 1, file_chunk);

	if (lamp_header.ident[0] != 'G' || lamp_header.ident[1] != 'L' || lamp_header.ident[2] != 'M' || lamp_header.ident[3] != 'P')
	{
		fseek(file_chunk, start, SEEK_SET);
		return 0;
	}

	if (lamp_data)
		DestroyLamps();

	num_total_lamps = lamp_header.sectors_high * lamp_header.sectors_wide;

	lamp_data = (LAMP_SECTOR *)DebugMalloc(lamp_header.sectors_high * lamp_header.sectors_wide * sizeof(LAMP_SECTOR), "D:\\Driver\\game6.4g\\lamps.c", 258);

	if (!lamp_data)
		return 0;

	memset(lamp_data, 0, num_total_lamps * sizeof(LAMP_SECTOR));

	num_cells = lamp_header.num_cells;

	while (num_cells--)
	{
		// sector index, number of lamps
		fread(sector, sizeof(sector), 1, file_chunk);

		index = sector[0];
		lamp_data[index][0] = sector[1];

		if (sector[1])
		{
			lamp_data[index][1] = (int)DebugMalloc(lamp_data[index][0] * sizeof(LAMP_OUTPUT), "D:\\Driver\\game6.4g\\lamps.c", 276);

			if (!lamp_data[index][1])
			{
				DestroyLamps();
				return 0;
			}

			fread((void *)lamp_data[index][1], sizeof(LAMP_OUTPUT), lamp_data[index][0], file_chunk);
		}
		else
		{
			lamp_data[index][1] = 0;
		}
	}

	return 1;
}

// [0x4ed2bb] [D]
void DestroyLamps(void)
{
	if (lamp_data)
	{
		while (num_total_lamps--)
		{
			if (lamp_data[num_total_lamps][1])
			{
				DebugFree((void *)lamp_data[num_total_lamps][1]);
				lamp_data[num_total_lamps][1] = 0;
			}

			lamp_data[num_total_lamps][1] = 0;
		}

		DebugFree(lamp_data);
		lamp_data = NULL;
	}

	num_total_lamps = 0;
}
