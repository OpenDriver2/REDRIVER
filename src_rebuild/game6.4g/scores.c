/*
 * scores.c - the high score tables
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "scores.c", 0x51ec80 - 0x51f57d).
 *
 * Each game type has 5 entries per city or course, best first; time trial shares the dirt track
 * tables and best lap has a single entry per city. HighScores points at the start of each table.
 * A used entry has lap_info.flags == 8. The tables are stored in SAVES\SCORES.HI after a 16 byte
 * header.
 */

#include <stdio.h>
#include <string.h>

#include "dr_types.h"
#include "debug.h"
#include "dr_globals.h"
#include "dr_scores.h"
#include "frontend/selrace.h"
#include "overlay.h"

#define HIGHSCORE_IDENT		'SCRE'
#define HIGHSCORE_VERSION	3

typedef struct HIGH_SCORES
{
	HIGHSCORE_DATA *CheckpointScores[8];
	HIGHSCORE_DATA *TrailblazerScores[8];
	HIGHSCORE_DATA *SurvivalScores[4];
	HIGHSCORE_DATA *DirtTrackScores[4];
	HIGHSCORE_DATA *CarnageScores[4];
	HIGHSCORE_DATA *BestLapScores[4];
} HIGH_SCORES;

// NOTE: these sit in the linker's common area; owner by their only users, the functions here
HIGHSCORE_DATA CarnageScores[4][5];
HIGHSCORE_DATA DirtTrackScores[4][5];
HIGHSCORE_DATA SurvivalScores[4][5];
HIGHSCORE_DATA CheckpointScores[8][5];
HIGHSCORE_DATA TrailblazerScores[8][5];
HIGHSCORE_DATA BestLapScores[4];
HIGH_SCORES HighScores;

static int GetCheckpointScorePosition(SCORE_DATA *score);
static int GetTrailblazerScorePosition(SCORE_DATA *score);
static int GetSurvivalScorePosition(SCORE_DATA *score);
static int GetTimeTrialScorePosition(SCORE_DATA *score);
static int GetCarnageScorePosition(SCORE_DATA *score);
static int GetBestLapScorePosition(SCORE_DATA *score);

// [0x51ec80] [D]
void InitialiseHighScores(void)
{
	int i;

	memset(&HighScores, 0, sizeof(HighScores));
	memset(CheckpointScores, 0, sizeof(CheckpointScores));
	memset(TrailblazerScores, 0, sizeof(TrailblazerScores));
	memset(SurvivalScores, 0, sizeof(SurvivalScores));
	memset(DirtTrackScores, 0, sizeof(DirtTrackScores));
	memset(CarnageScores, 0, sizeof(CarnageScores));
	memset(BestLapScores, 0, sizeof(BestLapScores));

	for (i = 0; i < 8; i++)
		HighScores.CheckpointScores[i] = CheckpointScores[i];

	for (i = 0; i < 8; i++)
		HighScores.TrailblazerScores[i] = TrailblazerScores[i];

	for (i = 0; i < 4; i++)
		HighScores.SurvivalScores[i] = SurvivalScores[i];

	for (i = 0; i < 4; i++)
	{
		HighScores.DirtTrackScores[i] = DirtTrackScores[i];
		HighScores.BestLapScores[i] = &BestLapScores[i];
	}

	for (i = 0; i < 4; i++)
		HighScores.CarnageScores[i] = CarnageScores[i];
}

// [0x51ee27] [D]
int LoadHighScores(FILE *f)
{
	int start;
	int header[4];

	start = ftell(f);

	fread(header, sizeof(header), 1, f);

	if (header[0] == HIGHSCORE_IDENT && header[3] == HIGHSCORE_VERSION)
	{
		fread(CheckpointScores, sizeof(HIGHSCORE_DATA), 40, f);
		fread(TrailblazerScores, sizeof(HIGHSCORE_DATA), 40, f);
		fread(SurvivalScores, sizeof(HIGHSCORE_DATA), 20, f);
		fread(DirtTrackScores, sizeof(HIGHSCORE_DATA), 20, f);
		fread(CarnageScores, sizeof(HIGHSCORE_DATA), 20, f);
		fread(BestLapScores, sizeof(HIGHSCORE_DATA), 4, f);

		return 1;
	}

	fseek(f, start, SEEK_SET);
	return 0;
}

// [0x51ef18] [D]
int SaveHighScores(FILE *f)
{
	int header[4];

	header[0] = HIGHSCORE_IDENT;
	header[1] = 0;
	header[2] = 0;
	header[3] = HIGHSCORE_VERSION;

	fwrite(header, sizeof(header), 1, f);

	fwrite(CheckpointScores, sizeof(HIGHSCORE_DATA), 40, f);
	fwrite(TrailblazerScores, sizeof(HIGHSCORE_DATA), 40, f);
	fwrite(SurvivalScores, sizeof(HIGHSCORE_DATA), 20, f);
	fwrite(DirtTrackScores, sizeof(HIGHSCORE_DATA), 20, f);
	fwrite(CarnageScores, sizeof(HIGHSCORE_DATA), 20, f);
	fwrite(BestLapScores, sizeof(HIGHSCORE_DATA), 4, f);

	return 1;
}

// [0x51efd5] [D]
int FindPlayerScorePosition(SCORE_DATA *score, int type, int index)
{
	HIGHSCORE_DATA *table;

	table = GetHighScoreTable(type, index);

	if (table == NULL)
	{
		score->table = NULL;
		return -1;
	}

	score->table = table;

	switch (type)
	{
		case GAME_TRAILBLAZER:
			return GetTrailblazerScorePosition(score);
		case GAME_CHECKPOINT:
			return GetCheckpointScorePosition(score);
		case GAME_SURVIVAL:
			return GetSurvivalScorePosition(score);
		case GAME_CARNAGE:
			return GetCarnageScorePosition(score);
		case GAME_TIMETRIAL:
			return GetTimeTrialScorePosition(score);
		case GAME_BESTLAP:
			return GetBestLapScorePosition(score);
	}

	score->table = NULL;
	return -1;
}

// [0x51f0c3] [D]
HIGHSCORE_DATA *GetHighScoreTable(int type, int index)
{
	switch (type)
	{
		case GAME_TRAILBLAZER:
			return HighScores.TrailblazerScores[index];
		case GAME_CHECKPOINT:
			return HighScores.CheckpointScores[index];
		case GAME_SURVIVAL:
			return HighScores.SurvivalScores[index];
		case GAME_DIRTTRACK:
		case GAME_TIMETRIAL:
			return HighScores.DirtTrackScores[index];
		case GAME_CARNAGE:
			return HighScores.CarnageScores[index];
		case GAME_BESTLAP:
			return HighScores.BestLapScores[index];
	}

	LogQuick("GetHighScoreTable: Invalid table requested");
	return NULL;
}

// [0x51f16c] [D]
HIGHSCORE_DATA *CheckForRecord(int *scorepos, int best_lap)
{
	HIGHSCORE_DATA *result;

	result = NULL;

	if (CheckIfCheating())
		return NULL;

	if (best_lap == 0)
	{
		*scorepos = FindPlayerScorePosition(&global_score, global_score.type, global_score.index);

		if (*scorepos != -1)
			result = GetHighScoreTable(global_score.type, global_score.index);
	}
	else if (global_score.type == GAME_TIMETRIAL)
	{
		*scorepos = FindPlayerScorePosition(&global_score, GAME_BESTLAP, global_score.index);

		if (*scorepos != -1)
			result = GetHighScoreTable(GAME_BESTLAP, global_score.index);
	}

	return result;
}

// [0x51f214] [D]
static int GetCheckpointScorePosition(SCORE_DATA *score)
{
	int time;
	int i;

	if (score->complete)
	{
		time = TimeToInt(&score->lap_info);

		for (i = 0; i < 5; i++)
		{
			if (score->table[i].lap_info.flags != 8)
				return i;

			if (time < TimeToInt(&score->table[i].lap_info))
				return i;
		}
	}

	return -1;
}

// [0x51f297] [D]
static int GetTrailblazerScorePosition(SCORE_DATA *score)
{
	int i;

	if (score->complete)
	{
		for (i = 0; i < 5; i++)
		{
			if (score->table[i].lap_info.flags != 8)
				return i;

			if (score->items > score->table[i].items)
				return i;

			if (score->items == score->table[i].items &&
				TimeToInt(&score->lap_info) > TimeToInt(&score->table[i].lap_info))
				return i;
		}
	}

	return -1;
}

// [0x51f35a] [D]
static int GetSurvivalScorePosition(SCORE_DATA *score)
{
	int time;
	int i;

	if (score->complete)
	{
		time = TimeToInt(&score->lap_info);

		for (i = 0; i < 5; i++)
		{
			if (score->table[i].lap_info.flags != 8)
				return i;

			if (time > TimeToInt(&score->table[i].lap_info))
				return i;
		}
	}

	return -1;
}

// [0x51f3dd] [D]
static int GetTimeTrialScorePosition(SCORE_DATA *score)
{
	int time;
	int i;

	if (score->complete)
	{
		time = TimeToInt(&score->lap_info);

		for (i = 0; i < 5; i++)
		{
			if (score->table[i].lap_info.flags != 8)
				return i;

			if (time < TimeToInt(&score->table[i].lap_info))
				return i;
		}
	}

	return -1;
}

// [0x51f460] [D]
static int GetCarnageScorePosition(SCORE_DATA *score)
{
	int i;

	if (score->complete)
	{
		for (i = 0; i < 5; i++)
		{
			if (score->table[i].lap_info.flags != 8)
				return i;

			if (score->items > score->table[i].items)
				return i;

			if (score->items == score->table[i].items &&
				TimeToInt(&score->lap_info) > TimeToInt(&score->table[i].lap_info))
				return i;
		}
	}

	return -1;
}

// [0x51f523] [D]
static int GetBestLapScorePosition(SCORE_DATA *score)
{
	int time;

	if (score->best_lap.flags != 8)
		return -1;

	if (score->table->lap_info.flags == 8)
	{
		time = TimeToInt(&score->best_lap);

		if (time >= TimeToInt(&score->table->lap_info))
			return -1;
	}

	return 0;
}
