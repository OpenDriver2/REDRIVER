/*
 * carnage.c - the Carnage driving game: score for damaging cars and smashing street objects
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "carnage.c", 0x4a1d80 - 0x4a25b5).
 *
 * BuildCarnageTable finds the smashable models of the level by name and groups them in table
 * by kind (ptable[kind] is the first of a kind); GetCarnageType turns a model back into its kind
 * and values[kind] is what smashing it is worth.
 */

#include <stdio.h>

#include "dr_types.h"
#include "carnage.h"
#include "dr_globals.h"
#include "map.h"

#define NUM_CARNAGE_TYPES	18		// kinds of smashable objects (values)
#define CARNAGE_NONE		18		// GetCarnageType: not a smashable object
#define CARNAGE_CAR_DAMAGE	0xFF00	// ReportCarnage: score new_damage

#define CARNAGE_MAX_SCORE	999999

int BuildCarnageTable(void);
u_char GetCarnageType(int type);

// initialised data (0x5537b0)

// score for smashing each kind of object (11 and 15 have no models)
int values[NUM_CARNAGE_TYPES] = {
	100, 150, 200, 50, 250, 300, 200, 250, 150, 500, 400, 0, 300, 700, 1000, 200, 95, 175
};

// uninitialised data
// NOTE: bss globals in the linker's common area (0x11b5ba4 - 0x11b5da8); defined here as this
// file is named after them (new_damage is mostly changed by bcollide.c DamageCar3D / DamageCar)

int carnage_score;
int carnage_flags;			// 1: the carnage game is on
// the model numbers by kind; 79 are used
// NOTE: 312 bytes in IDA, plus 8 unnamed bytes up to ptable (table[78] is named unk_11B5D58)
int table[80];
// the first model of each kind in table
// NOTE: IDA types it int *[19] but gives it 72 bytes: in the original ptable[18], read as the
// end of kind 17, is new_damage, which is 0 whenever BuildCarnageTable and GetCarnageType run
// (DamageCar adds and takes away the same damage, ReportCarnage zeroes it). Here ptable[18] is
// never set and stays NULL, which reads the same.
int *ptable[NUM_CARNAGE_TYPES + 1];
int new_damage;				// car damage not yet scored

// [0x4a1d80] [D]
// Starts carnage scoring in the Carnage game; -1 in other games.
int InitCarnage(void)
{
	carnage_flags = 0;

	if (global_score.type != GAME_CARNAGE)
		return -1;

	carnage_flags |= 1;
	carnage_score = 0;

	BuildCarnageTable();

	return 0;
}

// [0x4a1dba] [D]
// Fills table with the smashable models of the level, kind by kind, each kind sorted.
int BuildCarnageTable(void)
{
	char name[8];
	u_char i;
	u_char k;
	u_char num;
	int *tab;
	int *p;
	int j;
	int temp;

	tab = table;

	for (i = 0; i < NUM_CARNAGE_TYPES; i++)
	{
		ptable[i] = tab;

		switch (i)
		{
			case 0:
				*tab++ = FindModelWithName("CONE");
				*tab++ = FindModelWithName("CONE1");
				*tab++ = FindModelWithName("CONE2");
				*tab++ = FindModelWithName("CONE3");
				*tab++ = FindModelWithName("TRAININGCONES1");
				*tab++ = FindModelWithName("TRAININGCONES2");
				*tab++ = FindModelWithName("TRAININGCONES3");
				*tab++ = FindModelWithName("TRAININGCONES4");
				*tab++ = FindModelWithName("BIGCONE");
				break;
			case 1:
				*tab++ = FindModelWithName("BALLARD");
				*tab++ = FindModelWithName("BALLARD2");
				break;
			case 2:
				*tab++ = FindModelWithName("BIN1");
				*tab++ = FindModelWithName("BIN2");
				break;
			case 3:
				*tab++ = FindModelWithName("BOX");
				*tab++ = FindModelWithName("BOX1");
				*tab++ = FindModelWithName("BOX2");
				*tab++ = FindModelWithName("BOX3");
				*tab++ = FindModelWithName("PAPERBOX");
				*tab++ = FindModelWithName("PAPERBOX2");
				break;
			case 4:
				*tab++ = FindModelWithName("BARR1");
				*tab++ = FindModelWithName("CP-BARRIER");
				*tab++ = FindModelWithName("FENCE");
				break;
			case 5:
				*tab++ = FindModelWithName("PHONE");
				*tab++ = FindModelWithName("PHONE1");
				*tab++ = FindModelWithName("PHONE2");
				break;
			case 6:
				*tab++ = FindModelWithName("SIGN1");
				*tab++ = FindModelWithName("SIGN2");
				*tab++ = FindModelWithName("SIGN3");
				*tab++ = FindModelWithName("SIGN4");
				*tab++ = FindModelWithName("SIGN5");
				*tab++ = FindModelWithName("SIGN6");
				break;
			case 7:
				*tab++ = FindModelWithName("TABLE");
				*tab++ = FindModelWithName("TABLE2");
				break;
			case 8:
				*tab++ = FindModelWithName("CHAIR");
				*tab++ = FindModelWithName("CHAIR2");
				break;
			case 9:
				*tab++ = FindModelWithName("PARK_M");
				break;
			case 10:
				*tab++ = FindModelWithName("BOARD1");
				*tab++ = FindModelWithName("BOARD2");
				*tab++ = FindModelWithName("BOARD3");
				break;
			case 12:
				*tab++ = FindModelWithName("TAR_DRUM");
				*tab++ = FindModelWithName("DRUM");
				break;
			case 13:
				*tab++ = FindModelWithName("BENCH");
				break;
			case 14:
				*tab++ = FindModelWithName("HOTDOGS");
				break;
			case 16:
				for (j = 0; j < 34; j++)
				{
					sprintf(name, "GLASS%02d", j + 1);
					*tab++ = FindModelWithName(name);
				}
				break;
			case 17:
				*tab++ = FindModelWithName("UMBRELLA");
				*tab++ = FindModelWithName("PARASOL");
				break;
			default:
				// 11, 15: none
				break;
		}
	}

	// insertion sort of each kind (unsigned, so not found -1 goes last)
	for (i = 0; i < NUM_CARNAGE_TYPES; i++)
	{
		for (k = 1; &ptable[i][k] < ptable[i + 1]; k++)
		{
			num = k;

			while (num--)
			{
				p = &ptable[i][num];

				if ((unsigned int)p[0] > (unsigned int)p[1])
				{
					temp = p[0];
					p[0] = p[1];
					p[1] = temp;
				}
			}
		}
	}

	return 0;
}

// [0x4a248a] [D]
// The kind of smashable model `type`, CARNAGE_NONE if it is none.
u_char GetCarnageType(int type)
{
	u_char i;
	int *tab;

	i = 0;
	tab = table;

	while (i < NUM_CARNAGE_TYPES)
	{
		// skip the kinds without models
		while (tab == ptable[i + 1])
			i++;

		if ((unsigned int)*tab > (unsigned int)type)
		{
			// sorted: not of this kind
			tab = ptable[++i];
		}
		else
		{
			if ((unsigned int)*tab >= (unsigned int)type)
				return i;

			if (tab == &table[78])
				return CARNAGE_NONE;

			tab++;
		}
	}

	return i;
}

// [0x4a251f] [D]
// Scores smashing a model, or with CARNAGE_CAR_DAMAGE the car damage done since the last call.
int ReportCarnage(int type)
{
	u_char kind;

	if (type == CARNAGE_CAR_DAMAGE)
	{
		carnage_score += 100 * (new_damage / 120);
		new_damage = 0;
	}
	else
	{
		kind = GetCarnageType(type);

		if (kind != CARNAGE_NONE)
			carnage_score += values[kind];
	}

	if (carnage_score > CARNAGE_MAX_SCORE)
		carnage_score = CARNAGE_MAX_SCORE;

	global_score.items = carnage_score;

	return 0;
}
