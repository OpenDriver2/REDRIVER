/*
 * chairs.c - chairs (seated pedestrians)
 *
 * Rebuilt from Driver_sym2_94.i64 (IDA folder "chairs.c"); partially translated.
 */

#include <stdio.h>
#include <string.h>

#include "dr_types.h"
#include "chairs.h"
#include "asmmath.h"
#include "debug.h"
#include "general.h"
#include "map.h"

typedef struct CHAIR_LIST
{
	int cell_num;
	int num_chairs;
	CHAIR *chairs;
} CHAIR_LIST;

typedef struct SMASHED_CHAIR
{
	int id;
	int cell_num;
	int chair_num;
	int x;
	int z;
	int flags;
} SMASHED_CHAIR;

#define MAX_SMASHED_CHAIRS	100

#define CHAIR_LUMP_VERSION	2

typedef struct CHAIR_HEADER
{
	char ident[4];
	int version;
	int cells_x;
	int cells_z;
	int num_cells;
} CHAIR_HEADER;

void DestroyChairs(void);
int AddChairList(CHAIR_LIST *list);

// NOTE: bss globals in the linker's common area; defined here as chairs.c is their main user
CHAIR_LIST **chair_data;
int SmashedChairIndex;
int ChairMapHeight;
int ChairMapWidth;
SMASHED_CHAIR SmashedChairs[MAX_SMASHED_CHAIRS];
int chair_count;
int SmashedAlloc[MAX_SMASHED_CHAIRS];

// [0x4a4330] [D]
void InitialiseChairs(void)
{
	int i;

	ChairMapWidth = 0;
	ChairMapHeight = 0;
	chair_data = NULL;
	chair_count = 0;

	memset(SmashedChairs, 0, sizeof(SmashedChairs));

	for (i = 0; i < MAX_SMASHED_CHAIRS; i++)
	{
		SmashedAlloc[i] = i;
		SmashedChairs[i].id = i;
	}

	SmashedChairIndex = 0;
}

// [0x4a43b4] [D]
void ShutdownChairs(void)
{
	DestroyChairs();
	chair_count = 0;
}

// [0x4a43d2] [D]
// Takes a free smashed chair slot; -1 if all are used.
int AddSmashedChair(void)
{
	int idx;

	if (SmashedChairIndex >= MAX_SMASHED_CHAIRS)
		return -1;

	idx = SmashedAlloc[SmashedChairIndex++];
	SmashedChairs[idx].flags = 1;

	return idx;
}

// [0x4a45d4] [D]
// Marks the chair at a knocked over object's position smashed; returns 1 if there was one.
int SmashChair(float x, float z)
{
	int smashed;
	int ix;
	int iz;
	int cell;
	CHAIR_LIST *list;
	SMASHED_CHAIR *sc;
	CHAIR *chairs;
	int idx;
	int i;

	smashed = 0;

	if (!chair_data)
		return 0;

	ix = float_to_int(x);
	iz = float_to_int(z);

	cell = cells_across / 2 + ix / cell_size + cells_across * (cells_down / 2 + iz / cell_size);
	list = chair_data[cell];

	if (!list)
		return 0;

	chairs = list->chairs;

	for (i = 0; i < list->num_chairs; i++, chairs++)
	{
		if (chairs->x == ix && chairs->z == iz)
		{
			idx = AddSmashedChair();

			if (idx != -1)
			{
				sc = &SmashedChairs[idx];

				sc->cell_num = cell;
				sc->chair_num = i;
				sc->x = chairs->x;
				sc->z = chairs->z;

				chairs->flags |= 1;

				smashed = 1;
			}
		}
	}

	return smashed;
}

// [0x4a4737] [D]
void ProcessChairLump(FILE *lump_file, int lump_size)
{
	int num_cells;
	long start;
	int i;
	char text[128];
	CHAIR *chairs;
	CHAIR_LIST list;
	CHAIR_HEADER header;

	LogQuick("\nAttempting to process Chair placement lump.");

	DestroyChairs();

	start = ftell(lump_file);
	fread(&header, 1, sizeof(CHAIR_HEADER), lump_file);

	if (header.version != CHAIR_LUMP_VERSION)
	{
		fseek(lump_file, start + lump_size, SEEK_SET);

		LogQuick("Chair chunk is an incompatible version, ignoring.  Sitting pedestrians will not be available during game.");
		return;
	}

	sprintf(text, "Map is %d cells wide and %d cells high, pointer is %d bytes long required memory is %d (%dk).",
		header.cells_x, header.cells_z, sizeof(CHAIR_LIST *),
		header.cells_z * header.cells_x * sizeof(CHAIR_LIST *),
		header.cells_z * header.cells_x * sizeof(CHAIR_LIST *) / 1024);

	LogQuick(text);

	chair_data = (CHAIR_LIST **)DebugMalloc(header.cells_z * header.cells_x * sizeof(CHAIR_LIST *), "D:\\Driver\\game6.4g\\chairs.c", 380);

	if (!chair_data)
	{
		sprintf(text, "Error allocating memory for ChairCells (%d requested), terminating routine.",
			header.cells_z * header.cells_x * sizeof(CHAIR_LIST *));
		LogQuick(text);
		return;
	}

	memset(chair_data, 0, header.cells_z * header.cells_x * sizeof(CHAIR_LIST *));

	ChairMapWidth = header.cells_x;
	ChairMapHeight = header.cells_z;

	num_cells = header.num_cells;

	while (num_cells--)
	{
		fread(&list, 1, sizeof(CHAIR_LIST), lump_file);

		chairs = (CHAIR *)DebugMalloc(list.num_chairs * sizeof(CHAIR), "D:\\Driver\\game6.4g\\chairs.c", 403);

		if (!chairs)
		{
			LogQuick("Unable to allocate memory for chair list.");
			DestroyChairs();
			return;
		}

		chair_count += list.num_chairs;

		list.chairs = chairs;

		for (i = 0; i < list.num_chairs; i++)
			fread(&chairs[i], 1, sizeof(CHAIR), lump_file);

		if (!AddChairList(&list))
		{
			LogQuick("Failed to Add chair list to game, routine terminated.");

			DebugFree(chairs);
			DestroyChairs();

			return;
		}
	}

	sprintf(text, "ProcessChairLump completed successfully (%d chairs retrieved), routine terminated.", chair_count);
	LogQuick(text);
}

// [0x4a49bc] [D]
int AddChairList(CHAIR_LIST *list)
{
	if (!chair_data)
		return 0;

	if (!ChairMapHeight || !ChairMapWidth)
		return 0;

	if (chair_data[list->cell_num])
	{
		if (chair_data[list->cell_num]->chairs)
			DebugFree(chair_data[list->cell_num]->chairs);
	}
	else
	{
		chair_data[list->cell_num] = (CHAIR_LIST *)DebugMalloc(sizeof(CHAIR_LIST), "D:\\Driver\\game6.4g\\chairs.c", 462);

		if (!chair_data[list->cell_num])
			return 0;
	}

	chair_data[list->cell_num]->chairs = list->chairs;
	chair_data[list->cell_num]->cell_num = list->cell_num;
	chair_data[list->cell_num]->num_chairs = list->num_chairs;

	return 1;
}

// [0x4a4b14] [D]
void DestroyChairs(void)
{
	int i;

	if (chair_data)
	{
		for (i = 0; i < ChairMapWidth * ChairMapHeight; i++)
		{
			if (chair_data[i])
			{
				DebugFree(chair_data[i]->chairs);
				DebugFree(chair_data[i]);

				chair_data[i] = NULL;
			}
		}

		DebugFree(chair_data);
	}

	ChairMapWidth = 0;
	ChairMapHeight = 0;

	chair_data = NULL;
}
