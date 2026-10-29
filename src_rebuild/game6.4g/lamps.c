/*
 * lamps.c - street lamps of the level (the LEV lamps lump)
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "lamps.c"); partially translated.
 */

#include <stdio.h>
#include <string.h>

#include "dr_types.h"
#include "general.h"
#include "lamps.h"

typedef struct LAMP_RGB
{
	int red;
	int green;
	int blue;
	int pad;
} LAMP_RGB;

typedef struct LAMP_OUTPUT
{
	int type;
	float range;
	VECTOR4 position;
	LAMP_RGB color;
} LAMP_OUTPUT;

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
