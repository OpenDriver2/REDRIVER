/*
 * testmath.c - maths helpers and lookup tables
 *
 * Rebuilt from Driver_sym2_94.i64 (IDA folder "testmath.c"); partially translated.
 */

#include "dr_globals.h"
#include "testmath.h"

#define OOZ_TABLE_SIZE		56250

// NOTE: bss global in the linker's common area; defined here as testmath.c fills it
float ooz_table[OOZ_TABLE_SIZE];

// [0x528277] [D]
// ooz_table[z] = screen_scale / z (perspective divide lookup).
void PreCalculateOOZ(void)
{
	int i;

	for (i = 0; i < OOZ_TABLE_SIZE; i++)
	{
		if (i)
			ooz_table[i] = screen_scale / (float)i;
		else
			ooz_table[0] = 0.0f;
	}
}
