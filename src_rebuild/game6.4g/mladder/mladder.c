/*
 * mladder/mladder.c - the mission ladder: the story mode script DATA\MLADDER.DML
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "mladder\mladder.c", 0x41b690 - 0x41cd97).
 *
 * InitMissionLadder compiles the script line by line (CompileLadderCommand) into a list of
 * MLADDER_COMMANDs in command_buffer, linked by next_cmd / else_cmd, and a MLADDER_ROOM per ROOM
 * block in room_buffer: the answerphone messages of that motel room (a MISSION, JOKE or CARSELECT
 * line inside the block becomes one message, at most 4). RunMissionLadder walks the list: SETCITY
 * opens a city, ROOM runs the motel room (RunLadderRoom -> DoFlat) and continues at the command of
 * the accepted message, MISSION / SUBMISSION / INTERVIEW play the game, RENDER plays an FMV,
 * TRIGGER / IF test the ladder flags, IFOK / QUITONFAIL test the last mission's outcome, END plays
 * the credits. The ladder position (command index, city, flags) is stored in game_data before each
 * room and mission, so a saved game resumes there.
 *
 * Script syntax: one command per line, the parameter after a space; IF / IFOK ... [ELSE ...] ENDIF
 * and ROOM ... ENDROOM nest up to 4 deep.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dr_types.h"
#include "dr_globals.h"
#include "mladder/mladder.h"
#include "debris.h"
#include "debug.h"
#include "flat/flat.h"
#include "FMV/render.h"
#include "frontend/config.h"
#include "frontend/frontend.h"
#include "frontend/name.h"
#include "MScript.h"
#include "replays.h"
#include "shell.h"
#include "dr_scores.h"

// MLADDER_COMMAND.type (NOTE: names from the script commands that compile to them)
#define MLC_SETCITY		0
#define MLC_ROOM		1
#define MLC_MISSION		2		// MISSION and SUBMISSION
#define MLC_RENDER		3
#define MLC_TRIGGER		4
#define MLC_IF			5
#define MLC_IFOK		6
#define MLC_JUMP		7		// ELSE and ENDIF: continue at next_cmd
#define MLC_STOP		8		// appended after the last line
#define MLC_QUITONFAIL	9
#define MLC_INTERVIEW	11
#define MLC_END			12

#define MAX_LADDER_COMMANDS	256
#define MAX_LADDER_ROOMS	32
#define MAX_LADDER_LEVEL	4		// nesting of IF / ROOM blocks
#define MAX_ROOM_MESSAGES	4

void TriggerFlag(int flag);
int FlagTriggered(int flag);
MLADDER_COMMAND *RunLadderRoom(MLADDER_ROOM *room);
int CompileLadderCommand(char *line);
int ReadLadderLine(FILE *file, char *line);
int GetLadderCommand(char *line, char *cmd);

// defined below in the bss block
extern MLADDER_COMMAND command_buffer[];

//------------------------------------------------------------------------------------------
// initialised data (0x550b98)

MLADDER_COMMAND *cur_command = command_buffer;

//------------------------------------------------------------------------------------------
// uninitialised data (0x601198 - 0x602614)

int missions[MAX_LADDER_LEVEL];					// messages so far in the open ROOM of each level
MLADDER_ROOM room_buffer[MAX_LADDER_ROOMS];
int rooms_num[MAX_LADDER_LEVEL];
MLADDER_COMMAND *if_prev_command[MAX_LADDER_LEVEL];	// the ELSE jump, pointed past ENDIF
MLADDER_COMMAND *if_command[MAX_LADDER_LEVEL];		// the open IF / IFOK, else_cmd set by ELSE / ENDIF
MLADDER_COMMAND command_buffer[MAX_LADDER_COMMANDS];
int data;
MLADDER_COMMAND *mission_end[MAX_ROOM_MESSAGES];	// last command of each room message's commands
MLADDER_ROOM *rooms[MAX_LADDER_LEVEL];
int ct;
MLADDER_ROOM *cur_room;
int num_commands;
int num_rooms;
int level;
int in_room;

// NOTE: moved here from the generated stubs; no IDA mark, the owner is a guess from the users
// and the neighbouring data
MISSION_LADDER mladder;

// [0x41b690] [D]
int InitMissionLadder(void)
{
	FILE *file;
	char line[128];

	memset(command_buffer, 0, sizeof(command_buffer));
	memset(room_buffer, 0, sizeof(room_buffer));
	memset(if_command, 0, sizeof(if_command));
	memset(if_prev_command, 0, sizeof(if_prev_command));
	memset(rooms, 0, sizeof(rooms));
	memset(rooms_num, 0, sizeof(rooms_num));
	memset(missions, 0, sizeof(missions));
	memset(mission_end, 0, sizeof(mission_end));

	cur_command = command_buffer;
	cur_room = NULL;
	num_commands = 0;
	num_rooms = 0;

	file = fopen("DATA\\MLADDER.DML", "r");

	if (file == NULL)
	{
		LogQuick("MLADDER: DATA\\MLADDER.DML missing");
		return 0;
	}

	LogQuick("\nMLADDER: Compiling...");

	while (ReadLadderLine(file, line) != -1)
	{
		if (!CompileLadderCommand(line))
		{
			fclose(file);

			LogQuick("MLADDER: Compilation failed\n");
			return 0;
		}
	}

	fclose(file);

	cur_command->type = MLC_STOP;
	cur_command->data = 0;
	cur_command->next_cmd = NULL;
	cur_command->else_cmd = NULL;
	num_commands++;

	LogQuick("MLADDER: Compilation successful\n");

	mladder.command_list = command_buffer;
	mladder.command = command_buffer;
	mladder.room_list = room_buffer;
	mladder.flags = 0;
	mladder.level = 0;

	return 1;
}

// [0x41b876] [D]
void FreeMissionLadder(void)
{
}

// [0x41b87b] [D]
void RunMissionLadder(void)
{
	int running;

	running = 1;

	while (running)
	{
		switch (mladder.command->type)
		{
			case MLC_SETCITY:
				mladder.level = mladder.command->data;

				if (gNumCitiesOpen < mladder.level + 1)
				{
					gNumCitiesOpen = mladder.level + 1;
					UpdateSavedOptions();
				}

				mladder.command = mladder.command->next_cmd;
				break;
			case MLC_ROOM:
				game_data.gNumCitiesOpen = gNumCitiesOpen;
				game_data.mladder_index = mladder.command - mladder.command_list;
				game_data.mladder_level = mladder.level;
				game_data.mladder_flags = mladder.flags;
				game_data.gStoredPlayerCarType = gStoredPlayerCarType;

				GameLevel = mladder.level;

				mladder.command = RunLadderRoom(&mladder.room_list[mladder.command->data]);

				if (mladder.command == NULL)
					running = 0;
				break;
			case MLC_MISSION:
				game_data.gNumCitiesOpen = gNumCitiesOpen;
				game_data.mladder_index = mladder.command - mladder.command_list;
				game_data.mladder_level = mladder.level;
				game_data.mladder_flags = mladder.flags;
				game_data.gStoredPlayerCarType = gStoredPlayerCarType;

				GameLevel = mladder.level;
				GameMode = mladder.command->data;

				global_score.type = GAME_MISSION;
				StartGame(0);

				if (exit_flag == 1)
					running = 0;
				else
					mladder.command = mladder.command->next_cmd;
				break;
			case MLC_RENDER:
				PlayRender(mladder.command->data, -1);
				mladder.command = mladder.command->next_cmd;
				break;
			case MLC_TRIGGER:
				TriggerFlag(mladder.command->data);
				mladder.command = mladder.command->next_cmd;
				break;
			case MLC_IF:
				if (FlagTriggered(mladder.command->data))
					mladder.command = mladder.command->next_cmd;
				else
					mladder.command = mladder.command->else_cmd;
				break;
			case MLC_IFOK:
				if (mission.objective == 0xFF0)
					mladder.command = mladder.command->else_cmd;
				else
					mladder.command = mladder.command->next_cmd;
				break;
			case MLC_JUMP:
				mladder.command = mladder.command->next_cmd;
				break;
			case MLC_QUITONFAIL:
				if (mission.objective == 0xFF0)
					running = 0;
				else
					mladder.command = mladder.command->next_cmd;
				break;
			case MLC_INTERVIEW:
				if (gSkipInterview)
				{
					mission.objective = 0x8000;
					mladder.command = mladder.command->next_cmd;
					break;
				}

				GameLevel = 4;
				GameMode = 0;
				gDoTraining = 0;
				gDoInterview = 1;
				gDoLightFlare = 1;
				gNight = 1;

				StartGame(0);

				GameLevel = 0;
				GameMode = 0;
				gDoInterview = 0;
				gDoLightFlare = 0;
				gNight = 0;

				LoadSavedOptions();

				if (exit_flag == 1)
					running = 0;
				else
					mladder.command = mladder.command->next_cmd;
				break;
			case MLC_END:
				gDoCredits = 1;
				gbPlayingDemo = 1;

				InitReplay("CREDITS");
				StartGame(1);

				gbPlayingDemo = 0;
				gDoCredits = 0;

				UnlockAllCheats();
				UpdateSavedOptions();

				mladder.command = mladder.command->next_cmd;
				break;
			default:
				running = 0;
				break;
		}
	}
}

// [0x41bc60] [D]
void TriggerFlag(int flag)
{
	mladder.flags |= 1 << flag;
}

// [0x41bc7d] [D]
int FlagTriggered(int flag)
{
	return ((1 << flag) & mladder.flags) != 0;
}

// [0x41bca1] [D]
MLADDER_COMMAND *RunLadderRoom(MLADDER_ROOM *room)
{
	int result;

	result = 0;

	GameLevel = mladder.level;

	while (result != -1)
	{
		result = DoFlat(room);

		if (result >= 0)
			return room->messages[result].command;

		if (result == -2)
		{
			// Take a Drive
			if (GameLevel == 1)
				GameMode = 247;
			else if (GameLevel == 2)
				GameMode = 248;
			else if (GameLevel == 3)
				GameMode = 249;
			else
				GameMode = 246;

			global_score.type = GAME_TAKEADRIVE;

			StartGame(0);
		}
	}

	exit_flag = 1;

	return NULL;
}

// [0x41bd7c] [D]
int CompileLadderCommand(char *line)
{
	int city;
	char cmd[128];

	if (num_commands >= MAX_LADDER_COMMANDS || num_rooms >= MAX_LADDER_ROOMS || level >= MAX_LADDER_LEVEL)
	{
		LogQuick("MLADDER: Ladder too big");
		return 0;
	}

	if (GetLadderCommand(line, cmd))
		return 1;

	if (_strcmpi(cmd, "SETCITY") == 0)
	{
		if (GetLadderCommand(line, cmd))
		{
			LogQuick("MLADDER: Missing parameter");
			return 0;
		}

		if (_strcmpi(cmd, "[MIAMI]") == 0)
		{
			city = 0;
		}
		else if (_strcmpi(cmd, "[SF]") == 0)
		{
			city = 1;
		}
		else if (_strcmpi(cmd, "[LA]") == 0)
		{
			city = 2;
		}
		else if (_strcmpi(cmd, "[NY]") == 0)
		{
			city = 3;
		}
		else
		{
			LogQuick("MLADDER: Unknown CITY");
			return 0;
		}

		cur_command->type = MLC_SETCITY;
		cur_command->data = city;
		cur_command->next_cmd = cur_command + 1;
		cur_command->else_cmd = cur_command + 1;
		cur_command++;
		num_commands++;
	}
	else if (_strcmpi(cmd, "ROOM") == 0)
	{
		if (num_rooms >= MAX_LADDER_ROOMS)
		{
			LogQuick("MLADDER: Too many rooms");
			return 0;
		}

		if (cur_room)
		{
			cur_room++;
		}
		else
		{
			cur_room = room_buffer;
			num_rooms = 0;
		}

		for (ct = 0; ct < MAX_ROOM_MESSAGES; ct++)
		{
			cur_room->messages[ct].type = MSG_END;
			cur_room->messages[ct].data = 0;
			cur_room->messages[ct].command = NULL;
		}

		rooms[level] = cur_room;
		rooms_num[level] = num_rooms;
		missions[level] = 0;

		in_room++;

		// the room command points at itself: RunMissionLadder continues at the accepted message
		cur_command->type = MLC_ROOM;
		cur_command->data = num_rooms;
		cur_command->next_cmd = cur_command;
		cur_command->else_cmd = cur_command;
		cur_command++;
		num_commands++;

		num_rooms++;
		level++;
	}
	else if (_strcmpi(cmd, "ENDROOM") == 0)
	{
		if (--level < 0)
		{
			LogQuick("MLADDER: Unmatched ENDROOM");
			return 0;
		}

		if (rooms[level] == NULL)
		{
			LogQuick("MLADDER: Unmatched ENDROOM");
			return 0;
		}

		in_room--;
		rooms[level] = NULL;

		for (ct = 0; ct < MAX_ROOM_MESSAGES; ct++)
		{
			if (mission_end[ct])
			{
				mission_end[ct]->next_cmd = cur_command;
				mission_end[ct]->else_cmd = cur_command;
				mission_end[ct] = NULL;
			}
		}
	}
	else if (_strcmpi(cmd, "INTERVIEW") == 0)
	{
		cur_command->type = MLC_INTERVIEW;
		cur_command->data = 0;
		cur_command->next_cmd = cur_command + 1;
		cur_command->else_cmd = cur_command + 1;
		cur_command++;
		num_commands++;
	}
	else if (_strcmpi(cmd, "MISSION") == 0)
	{
		if (GetLadderCommand(line, cmd))
		{
			LogQuick("MLADDER: Missing MISSION parameter");
			return 0;
		}

		data = atoi(cmd);

		if (in_room)
		{
			if (missions[level - 1] >= MAX_ROOM_MESSAGES)
			{
				LogQuick("MLADDER: Too many options in ROOM");
				return 0;
			}

			// NOTE: the previous message's last command is stored at the new message's index
			// (mission_end[1..3]); ENDROOM points every stored one past the room
			if (missions[level - 1] > 0)
				mission_end[missions[level - 1]] = cur_command - 1;

			rooms[level - 1]->messages[missions[level - 1]].type = MSG_MISSION;
			rooms[level - 1]->messages[missions[level - 1]].data = data;
			rooms[level - 1]->messages[missions[level - 1]++].command = cur_command;
		}

		cur_command->type = MLC_MISSION;
		cur_command->data = data;
		cur_command->next_cmd = cur_command + 1;
		cur_command->else_cmd = cur_command + 1;
		cur_command++;
		num_commands++;
	}
	else if (_strcmpi(cmd, "SUBMISSION") == 0)
	{
		if (GetLadderCommand(line, cmd))
		{
			LogQuick("MLADDER: Missing SUBMISSION parameter");
			return 0;
		}

		data = atoi(cmd);

		cur_command->type = MLC_MISSION;
		cur_command->data = data;
		cur_command->next_cmd = cur_command + 1;
		cur_command->else_cmd = cur_command + 1;
		cur_command++;
		num_commands++;
	}
	else if (_strcmpi(cmd, "JOKE") == 0)
	{
		if (!in_room)
		{
			LogQuick("MLADDER: JOKE used outside ROOM");
			return 0;
		}

		if (missions[level - 1] >= MAX_ROOM_MESSAGES)
		{
			LogQuick("MLADDER: Too many options in ROOM");
			return 0;
		}

		if (GetLadderCommand(line, cmd))
		{
			LogQuick("MLADDER: Missing JOKE parameter");
			return 0;
		}

		data = atoi(cmd);

		rooms[level - 1]->messages[missions[level - 1]].type = MSG_JOKE;
		rooms[level - 1]->messages[missions[level - 1]].data = data;
		rooms[level - 1]->messages[missions[level - 1]++].command = NULL;
	}
	else if (_strcmpi(cmd, "CARSELECT") == 0)
	{
		if (!in_room)
		{
			LogQuick("MLADDER: CARSELECT used outside ROOM");
			return 0;
		}

		if (missions[level - 1] >= MAX_ROOM_MESSAGES)
		{
			LogQuick("MLADDER: Too many options in ROOM");
			return 0;
		}

		if (GetLadderCommand(line, cmd))
		{
			LogQuick("MLADDER: Missing CARSELECT parameter");
			return 0;
		}

		data = atoi(cmd);

		rooms[level - 1]->messages[missions[level - 1]].type = MSG_CAR;
		rooms[level - 1]->messages[missions[level - 1]].data = data;
		rooms[level - 1]->messages[missions[level - 1]++].command = NULL;
	}
	else if (_strcmpi(cmd, "RENDER") == 0)
	{
		if (GetLadderCommand(line, cmd))
		{
			LogQuick("MLADDER: Missing RENDER parameter");
			return 0;
		}

		cur_command->type = MLC_RENDER;
		cur_command->data = atoi(cmd);
		cur_command->next_cmd = cur_command + 1;
		cur_command->else_cmd = cur_command + 1;
		cur_command++;
		num_commands++;
	}
	else if (_strcmpi(cmd, "TRIGGER") == 0)
	{
		if (GetLadderCommand(line, cmd))
		{
			LogQuick("MLADDER: Missing TRIGGER parameter");
			return 0;
		}

		cur_command->type = MLC_TRIGGER;
		cur_command->data = atoi(cmd);
		cur_command->next_cmd = cur_command + 1;
		cur_command->else_cmd = cur_command + 1;
		cur_command++;
		num_commands++;
	}
	else if (_strcmpi(cmd, "IF") == 0)
	{
		if (GetLadderCommand(line, cmd))
		{
			LogQuick("MLADDER: Missing IF parameter");
			return 0;
		}

		cur_command->type = MLC_IF;
		cur_command->data = atoi(cmd);
		cur_command->next_cmd = cur_command + 1;
		cur_command->else_cmd = cur_command + 1;

		if_command[level] = cur_command++;

		num_commands++;
		level++;
	}
	else if (_strcmpi(cmd, "IFOK") == 0)
	{
		// IFOK has no parameter: data is atoi("IFOK") = 0
		cur_command->type = MLC_IFOK;
		cur_command->data = atoi(cmd);
		cur_command->next_cmd = cur_command + 1;
		cur_command->else_cmd = cur_command + 1;

		if_command[level] = cur_command++;

		num_commands++;
		level++;
	}
	else if (_strcmpi(cmd, "ELSE") == 0)
	{
		if (--level < 0)
		{
			LogQuick("MLADDER: Unmatched ELSE");
			return 0;
		}

		cur_command->type = MLC_JUMP;
		cur_command->data = 0;
		cur_command->next_cmd = cur_command + 1;
		cur_command->else_cmd = cur_command + 1;
		cur_command++;
		num_commands++;

		if (if_command[level])
		{
			if_command[level]->else_cmd = cur_command;
			if_command[level] = NULL;
		}

		if_prev_command[level++] = cur_command - 1;
	}
	else if (_strcmpi(cmd, "ENDIF") == 0)
	{
		if (--level < 0)
		{
			LogQuick("MLADDER: Unmatched ENDIF");
			return 0;
		}

		cur_command->type = MLC_JUMP;
		cur_command->data = 0;
		cur_command->next_cmd = cur_command + 1;
		cur_command->else_cmd = cur_command + 1;
		cur_command++;
		num_commands++;

		if (if_command[level])
		{
			if_command[level]->else_cmd = cur_command;
			if_command[level] = NULL;
		}
		else
		{
			if (if_prev_command[level] == NULL)
			{
				LogQuick("MLADDER: Unmatched ENDIF");
				return 0;
			}

			if_prev_command[level]->next_cmd = cur_command;
			if_prev_command[level]->else_cmd = cur_command;
			if_prev_command[level] = NULL;
		}
	}
	else if (_strcmpi(cmd, "QUITONFAIL") == 0)
	{
		cur_command->type = MLC_QUITONFAIL;
		cur_command->data = 0;
		cur_command->next_cmd = cur_command + 1;
		cur_command->else_cmd = cur_command + 1;
		cur_command++;
		num_commands++;
	}
	else if (_strcmpi(cmd, "END") == 0)
	{
		cur_command->type = MLC_END;
		cur_command->data = 0;
		cur_command->next_cmd = cur_command + 1;
		cur_command->else_cmd = cur_command + 1;
		cur_command++;
		num_commands++;
	}
	else
	{
		// NOTE: the original tests (!cmd[0] == '#' && cmd[0]), which is never true (probably meant
		// cmd[0] != '#'), so unknown commands and '#' comments are both skipped silently
		if ((cmd[0] == 0) == '#' && cmd[0] != 0)
		{
			LogQuick("MLADDER: Unknown command");
			return 0;
		}
	}

	return 1;
}

// [0x41cc7d] [D]
int ReadLadderLine(FILE *file, char *line)
{
	if (fgets(line, 128, file))
		return 0;

	return -1;
}

// [0x41ccaa] [D]
int GetLadderCommand(char *line, char *cmd)
{
	int idx;
	int num;
	size_t count;
	char c;
	int end;

	num = 0;
	idx = 0;

	// skip leading spaces, take the word up to a space or newline
	count = strlen(line);

	while (count--)
	{
		c = line[idx++];

		if (c == ' ' && num == 0)
			continue;

		if (c == ' ' || c == '\n')
			break;

		cmd[num++] = c;
	}

	cmd[num] = 0;

	// remove the word from the line
	end = 0;

	while (line[idx])
		line[end++] = line[idx++];

	line[end] = 0;

	return num == 0;
}
