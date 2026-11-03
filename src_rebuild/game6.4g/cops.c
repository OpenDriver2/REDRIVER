/*
 * cops.c - the police: cop cars chasing the player, roadblock requests, felony
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "cops.c"); partially translated.
 */

#include "dr_types.h"
#include "cops.h"
#include "dr_globals.h"
#include "ai.h"

// [0x4a7f09] [D]
// A free cop slot (1 - 8, cleared), -1 if all 8 are active.
int GetCopCarSlot(void)
{
	COP *cop;
	int i;

	cop = cop_data;

	for (i = 1; i <= 8; i++)
	{
		if (!cop->active)
		{
			ClearMem(cop, sizeof(COP));
			cop->id = i - 1;

			return i;
		}

		cop++;
	}

	return -1;
}
