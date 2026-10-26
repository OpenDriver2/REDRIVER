/*
 * chairs.c - chairs (seated pedestrians)
 *
 * Rebuilt from Driver_sym2_94.i64 (IDA folder "chairs.c"); partially translated.
 */

#include <string.h>

typedef struct SVECTOR
{
	short vx;
	short vy;
	short vz;
	short pad;
} SVECTOR;

typedef struct CHAIR
{
	int x;
	int z;
	int flags;
	SVECTOR ang;
} CHAIR;

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

void DestroyChairs(void);

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
