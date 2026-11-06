/*
 * compiler.c - event script compiler
 *
 * Rebuilt from Driver_sym2_94.i64 (IDA folder "compiler.c", 0x4a5500 - 0x4a72bb).
 *
 * CompileEventScript reads the level's event script ("Scripts\Events\<EVENTSCRIPT>", a text file
 * starting "DRIVER EVENT SCRIPT 2.0") into the EVENT list event.c runs. MODEL_NAMES gives names
 * to event model numbers; EVENT_LIST holds EVENT_START ... EVENT_END blocks, and
 * MULTIEVENT_START n blocks (MODELS / PALETTES / VELOCITIES lists of n) compiled once and cloned
 * n - 1 times. Each command is stored in its model's command list as the command number and
 * its parameters (comm_len bytes), ended by a 0 command; the event.c handlers read them back.
 */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <float.h>

#include "dr_types.h"
#include "compiler.h"
#include "asmmath.h"
#include "debug.h"
#include "dr_globals.h"
#include "event.h"
#include "general.h"
#include "map.h"
#include "MScript.h"
#include "file.h"
#include "shell.h"

char *CompileModelNames(char *line);
void CompileEvents(char *line);
void CloneEvent(int id);
char *CompileEvent(char *line);
char *CompileCommand(char *line, EVENT *ev);
short GetCommand(char *line);
int ScanShortParam(char **line, int param, short command);
int ScanFloatParam(char *line, int param);
void SkipNonNumbers(char **line);
int SkipAnyNumbers(char **line);
int CheckWord(char *line, char *word);
char *ReadModelName(char *line, char *name);
char *StoreCommand(unsigned short command, EVENT_MODEL *em);
char *GetCommandParams(char *dest, const char *params);
void AddModelName(char *line, short index);
int FindModelName(char *name, short *index);
char *SkipNonLetters(char *line);
char *NextLine(char *line);
int MoveToWord(char *line, char *word);
char *MoveToLine(char *line, char *word);
int ScriptError(char *text);
char *ReadShortParams(char *line, short *params, int count);

// [A]
// NOTE: the game runs the FPU with chop rounding (FPUInit); the current CRT parses floats only in
// round-to-nearest mode (debug builds assert "unexpected rounding mode"), so sscanf switches to
// it for the call (as fscanf_nearest in camera.c)
static int sscanf_nearest(const char *buffer, const char *format, ...)
{
	unsigned int old_control;
	unsigned int control;
	va_list args;
	int result;

	_controlfp_s(&old_control, 0, 0);
	_controlfp_s(&control, _RC_NEAR, _MCW_RC);

	va_start(args, format);
	result = vsscanf(buffer, format, args);
	va_end(args);

	_controlfp_s(&control, old_control, _MCW_RC);

	return result;
}

// script command numbers (GetCommand: event_commands index + 1)
#define CMD_TRIGGER_RADIUS		1
#define CMD_TRIGGER_CELL		2
#define CMD_TRIGGER_VALUE		3
#define CMD_SET_MODEL			4
#define CMD_MODEL_MOVE			9
#define CMD_MODEL_WAIT			14
#define CMD_DO					19
#define CMD_LOOP				20
#define CMD_LOOP_COUNT			21
#define CMD_COMMENT				50

// [0x553dca] bytes a stored command takes, command number included
short comm_len[32] = {
	0, 14, 6, 4, 16, 8, 14, 2, 14, 18, 2, 6, 10, 10, 6, 2,
	2, 6, 16, 0, 4, 8, 4, 4, 6, 2, 2, 2, 4, 8, 20, 8
};

// [0x553e0a] whether a command takes parameters (by command number)
// NOTE: IDA marks the table "!!! NOT ACCURATE !!!"; GetCommand only tests it for 0
short num_param[32] = {
	2, 3, 1, 1, 4, 3, 3, 0, 3, 4, 0, 1, 2, 2, 1, 0,
	0, 2, 4, 0, 0, 1, 1, 1, 2, 0, 0, 0, 1, 2, 5, 3
};

// [0x553e4c] parameter types of each command (by command number - 1): f float, i short
// NOTE: the empty entries point at zeroed bss dwords (0xb9f224 ...) in the original
char *command_params[32] = {
	"fff",
	"ii",
	"i",
	"ifff",
	"iii",
	"fff",
	"",
	"fff",
	"ffff",
	"",
	"f",
	"ff",
	"ff",
	"i",
	"",
	"",
	"ii",
	"ifff",
	"",
	"",
	"i",
	"i",
	"i",
	"ii",
	"",
	"",
	"",
	"i",
	"fi",
	"iffff",
	"iii",
	""
};

// [0x553ecc]
char *event_commands[32] = {
	"TRIGGER_RADIUS",
	"TRIGGER_CELL",
	"TRIGGER_VALUE",
	"SET_MODEL",
	"SET_ROTATION",
	"MODEL_TARGET",
	"TARGET_PLAYER",
	"MODEL_MOVETO",
	"MODEL_MOVE",
	"MODEL_STOP",
	"MODEL_VELOCITY",
	"MODEL_SLOW",
	"MODEL_SPEED",
	"MODEL_WAIT",
	"MODEL_DESTROY",
	"TERMINATE_EVENT",
	"SEND_MESSAGE",
	"ADD_MODEL",
	"DO",
	"LOOP",
	"LOOP",
	"TRIGGER",
	"PLAY_SOUND",
	"STOPBOX",
	"MOVEFORWARD",
	"TURNTOTARGET",
	"WAITFORTARGET",
	"TRIGNEXT",
	"MODEL_TURN",
	"MODEL_SPEEDCURVE",
	"MODEL_ROTATE",
	"COLLISIONSOFF"
};

EVENT *event;
void *event_model_name;			// name\0, short model number; ... ended by two 0 bytes
int event_name_length;
EVENT_MODEL *model;

// NOTE: bss globals in the linker's common area; defined here as compiler.c is their only user
short comp_iparam1;
short comp_iparam2;
short comp_iparam3;
short comp_iparam4;
int do_pos;
short *velocities;
int script_line;
int num_event_names;
EVENT_MODEL *parentmodel;
char events_filename[256];
int set_do;
short *models;
int trigger;
int spare_model_count;
float comp_fparam4;
float comp_fparam3;
float comp_fparam2;
float comp_fparam1;
short *palettes;

// [0x4a5500] [D] [A]
// NOTE: [A] sscanf_nearest instead of sscanf
void CompileEventScript(void)
{
	int idx;
	int res;
	float version;
	DMSVariable *var;
	char *script;
	char *line;

	idx = GetVariableIndex("EVENTSCRIPT");

	if (idx == -1)
		return;

	var = &DmsVars[idx];

	// NB: we can DEFINITELY corrupt the program this way :D
	sprintf(events_filename, "\nAttempting to compile event script \"Scripts\\Events\\%s\".", (char *)var->data);
	LogQuick(events_filename);

	sprintf(events_filename, "Scripts\\Events\\%s", (char *)var->data);

	num_event_names = 0;

	if (num_event_models == 0)
		return;

	script = (char *)FileRead(events_filename);

	if (!script)
	{
		LogQuick("Unable to open script file!  Operation failed.");
		return;
	}

	res = sscanf_nearest(script, "DRIVER EVENT SCRIPT %f", &version);

	if (res == 0 || res == -1)
	{
		LogQuick("This is not a legal Driver Event Script.  Operation failed.");
		DebugFree(script);
		return;
	}

	if (version != 2.0f)
	{
		LogQuick("Script file is not version 2.0!  Operation failed.");
		DebugFree(script);
		return;
	}

	line = CompileModelNames(script);
	line = MoveToLine(line, "EVENT_LIST");

	CompileEvents(line);

	// NOTE: the original reads FirstEvent->model here into an unused local (crashing on a script
	// with no events); dropped

	if (event_model_name)
		DebugFree(event_model_name);

	event_model_name = NULL;
	event_name_length = 0;

	DebugFree(script);

	LogQuick("Script file compiled successfully.  Operation complete.\n");
}

// [0x4a56c6] [D]
char *CompileModelNames(char *line)
{
	int index;

	script_line = 0;
	index = 0;

	line = MoveToLine(line, "MODEL_NAMES");
	line = NextLine(line);

	while (MoveToWord(line, "CHUNK_END") != 1)
	{
		line = SkipNonLetters(line);
		AddModelName(line, index);
		line = NextLine(line);

		index++;
	}

	return line;
}

// [0x4a5766] [D]
void CompileEvents(char *line)
{
	int multi;
	int count;
	int i;

	for (;;)
	{
		multi = 0;

		while (MoveToWord(line, "EVENT_START") == 0)
		{
			multi = MoveToWord(line, "MULTIEVENT_START");

			if (multi)
				break;

			if (MoveToWord(line, "CHUNK_END") == 1)
				return;

			line = NextLine(line);
			line = SkipNonLetters(line);
		}

		spare_model_count = 0;

		if (!multi)
		{
			line = NextLine(line);
			line = SkipNonLetters(line);
			line = CompileEvent(line);
		}
		else
		{
			// move past "MULTIEVENT_START" very poorly
			for (line += 16; *line == ' ' || *line == '\t'; line++)
				;

			ScanShortParam(&line, 0, 0);

			count = (unsigned short)comp_iparam1;

			line = NextLine(line);
			line = SkipNonLetters(line);

			if (MoveToWord(line, "MODELS") == 1)
			{
				models = (short *)DebugMalloc(count * sizeof(short), "D:\\Driver\\game6.4g\\compiler.c", 374);

				if (!models)
				{
					ScriptError("8 Out of Memory");
					return;
				}

				// move past "MODELS" very poorly
				line += 6;
				line = ReadShortParams(line, models, count);
			}

			line = SkipNonLetters(line);

			if (MoveToWord(line, "PALETTES") == 1)
			{
				palettes = (short *)DebugMalloc(count * sizeof(short), "D:\\Driver\\game6.4g\\compiler.c", 392);

				if (!palettes)
				{
					ScriptError("8 Out of Memory");
					return;
				}

				// move past "PALETTES" very poorly
				line += 8;
				line = ReadShortParams(line, palettes, count);
			}

			line = SkipNonLetters(line);

			if (MoveToWord(line, "VELOCITIES") == 1)
			{
				velocities = (short *)DebugMalloc(count * sizeof(short), "D:\\Driver\\game6.4g\\compiler.c", 410);

				if (!velocities)
				{
					ScriptError("8 Out of Memory");
					return;
				}

				// move past "VELOCITIES" very poorly
				line += 10;
				line = ReadShortParams(line, velocities, count);
			}

			line = SkipNonLetters(line);
			line = CompileEvent(line);

			parentmodel->model_id = *models;

			for (i = 1; i < count; i++)
			{
				trigger++;
				CloneEvent(i);
			}

			if (models)
			{
				DebugFree(models);
				models = NULL;
			}

			if (palettes)
			{
				DebugFree(palettes);
				palettes = NULL;
			}

			if (velocities)
			{
				DebugFree(velocities);
				velocities = NULL;
			}
		}

		line = NextLine(line);
		line = SkipNonLetters(line);
	}
}

// [0x4a5b00] [D]
// Appends a copy of the last event that shares its parent model's commands; id is the index in
// the MULTIEVENT lists.
void CloneEvent(int id)
{
	EVENT *ev;
	EVENT *last;
	EVENT_MODEL *em;

	ev = (EVENT *)DebugMalloc(sizeof(EVENT), "D:\\Driver\\game6.4g\\compiler.c", 461);

	if (!ev)
	{
		ScriptError("Not enough memory for events!");
		return;
	}

	memset(ev, 0, sizeof(EVENT));

	for (last = FirstEvent; last->next; last = last->next)
		;

	last->next = ev;

	if ((last->flags & 0x20) == 0)
	{
		last->flags |= 0x10;
		EVENT_PARENT_ID(last) = last->id;
	}

	ev->rotation = 1;
	ev->flags |= 8;
	ev->flags |= 0x20;
	ev->id = trigger;

	EVENT_PARENT_ID(ev) = EVENT_PARENT_ID(last);

	em = (EVENT_MODEL *)DebugMalloc(sizeof(EVENT_MODEL), "D:\\Driver\\game6.4g\\compiler.c", 494);

	if (em)
	{
		memset(em, 0, sizeof(EVENT_MODEL));
		ev->model = em;
	}
	else
	{
		// BUG: CRASH!!!
		// NOTE: the original goes on with an uninitialised pointer; here it is NULL
		ScriptError("Not enough memory for\nnew model!");
	}

	em->commands_begin = parentmodel->commands_begin;

	if (models)
		em->model_id = models[id];

	if (palettes)
		em->palette = palettes[id];

	if (velocities)
		em->velocity = (float)(unsigned short)velocities[id];
}

// [0x4a5c94] [D]
// Compiles the commands up to EVENT_END into a new event at the end of the list.
char *CompileEvent(char *line)
{
	EVENT *last;

	event = (EVENT *)DebugMalloc(sizeof(EVENT), "D:\\Driver\\game6.4g\\compiler.c", 534);

	if (!event)
	{
		ScriptError("Not enough memory for events!");
		return NULL;
	}

	memset(event, 0, sizeof(EVENT));

	if (FirstEvent)
	{
		for (last = FirstEvent; last->next; last = last->next)
			;

		last->next = event;
	}
	else
	{
		FirstEvent = event;
	}

	do_pos = 0;
	set_do = 0;

	while (MoveToWord(line, "EVENT_END") != 1)
	{
		line = SkipNonLetters(line);
		line = CompileCommand(line, event);
		line = NextLine(line);
	}

	event->rotation = spare_model_count;

	return line;
}

// [0x4a5dab] [D]
char *CompileCommand(char *line, EVENT *ev)
{
	unsigned short command;
	short model_id;
	EVENT_MODEL *em;
	float len;

	line = SkipNonLetters(line);		// NB: redundant, already handled prior to this

	command = GetCommand(line);

	if (command == CMD_COMMENT)
		return line;

	if (command == CMD_TRIGGER_RADIUS)
	{
		EVENT_RADIUS_SQR(ev) = comp_fparam1 * comp_fparam1;
		EVENT_TRIGGER_X(ev) = comp_fparam2;
		EVENT_TRIGGER_Z(ev) = comp_fparam3;
	}
	else if (command == CMD_TRIGGER_CELL)
	{
		ev->flags |= 4;
		EVENT_TRIGGER_CELL(ev) = (unsigned short)comp_iparam1 + cells_across * (unsigned short)comp_iparam2;
	}
	else if (command == CMD_TRIGGER_VALUE)
	{
		ev->flags |= 8;
		ev->id = comp_iparam1;
		trigger = (unsigned short)comp_iparam1;
	}
	else if (command < 4)
	{
		ScriptError("Unknown Command in script!");
	}
	else if (command == CMD_SET_MODEL)
	{
		if (set_do)
			ScriptError("DO without LOOP!");

		model_id = comp_iparam1;
		em = (EVENT_MODEL *)DebugMalloc(sizeof(EVENT_MODEL), "D:\\Driver\\game6.4g\\compiler.c", 641);

		if (ev->model)
		{
			if (em)
			{
				model->next = em;
				model = em;
			}
			else
			{
				ScriptError("Not enough memory for\nnew model!");
			}
		}
		else if (em)
		{
			ev->model = em;
			model = em;
		}
		else
		{
			ScriptError("Not enough memory for\nnew model!");
		}

		memset(model, 0, sizeof(EVENT_MODEL));

		model->model_id = model_id;
		model->palette = -1;

		parentmodel = model;
		spare_model_count++;

		StoreCommand(command, model);
	}
	else
	{
		if (set_do)
			do_pos += (unsigned short)comm_len[command];

		switch (command)
		{
			case 5:		// SET_ROTATION
			case 6:		// MODEL_TARGET
			case 7:		// TARGET_PLAYER
			case 8:		// MODEL_MOVETO
			case 10:	// MODEL_STOP
			case 11:	// MODEL_VELOCITY
			case 12:	// MODEL_SLOW
			case 13:	// MODEL_SPEED
			case 15:	// MODEL_DESTROY
			case 16:	// TERMINATE_EVENT
			case 17:	// SEND_MESSAGE
			case 18:	// ADD_MODEL
			case 22:	// TRIGGER
			case 23:	// PLAY_SOUND
			case 26:	// TURNTOTARGET
			case 27:	// WAITFORTARGET
			case 28:	// TRIGNEXT
			case 29:	// MODEL_TURN
			case 30:	// MODEL_SPEEDCURVE
			case 31:	// MODEL_ROTATE
			case 32:	// COLLISIONSOFF
				StoreCommand(command, model);
				break;
			case CMD_MODEL_MOVE:
				len = sqrt(comp_fparam2 * comp_fparam2 + comp_fparam3 * comp_fparam3 + comp_fparam4 * comp_fparam4);

				comp_fparam2 = comp_fparam2 / len;
				comp_fparam3 = comp_fparam3 / len;
				comp_fparam4 = comp_fparam4 / len;

				StoreCommand(command, model);
				break;
			case CMD_MODEL_WAIT:
				comp_iparam2 = comp_iparam1;		// the counter starts at the wait
				StoreCommand(command, model);
				break;
			case CMD_DO:
				if (set_do)
					ScriptError("Cannot have embedded DO!");

				set_do = 1;
				do_pos = 0;
				break;
			case CMD_LOOP:
				if (!set_do)
					ScriptError("LOOP without DO!");

				// bytes back from the loop's parameter to the first command after DO
				comp_iparam1 = do_pos - comm_len[command] + 2;

				StoreCommand(command, model);
				set_do = 0;
				break;
			case CMD_LOOP_COUNT:
				// NOTE: unreachable, GetCommand matches the first "LOOP"
				if (!set_do)
					ScriptError("LOOP without DO!");

				comp_iparam2 = comp_iparam1;
				comp_iparam3 = comp_iparam1;
				comp_iparam1 = do_pos + 6 - comm_len[command];

				StoreCommand(command, model);
				set_do = 0;
				break;
			default:
				// STOPBOX, MOVEFORWARD are not stored
				break;
		}
	}

	return line;
}

// [0x4a621f] [D]
// The command at the line start with its parameters scanned into comp_iparam / comp_fparam;
// 0 if unknown or bad, CMD_COMMENT for a comment.
short GetCommand(char *line)
{
	unsigned short command;
	char *params;
	int spaces;
	int fparam;
	int iparam;
	int i;

	if (*line == '#' || (*line == '/' && line[1] == '/'))
		return CMD_COMMENT;

	command = 0;

	for (i = 0; i < 32; i++)
	{
		if (CheckWord(line, event_commands[i]))
		{
			command = i + 1;
			break;
		}
	}

	if (!command)
		return 0;

	line += strlen(event_commands[command - 1]);

	// num_param is read by command number, so COLLISIONSOFF (32) reads past it, into the
	// alignment word before command_params, which is 0 in the binary
	if (command >= 32 || num_param[command] == 0)
		return command;

	spaces = 0;

	while (*line == ' ')
	{
		spaces++;
		line++;
	}

	if (spaces == 0 && *line != 0)
		return 0;

	fparam = 0;
	iparam = 0;
	params = command_params[command - 1];

	while (*params)
	{
		i = *params++;

		if (i == 'f')
		{
			ScanFloatParam(line, fparam);
			SkipAnyNumbers(&line);
			fparam++;
		}
		else if (i == 'i')
		{
			ScanShortParam(&line, iparam, command);
			SkipNonNumbers(&line);
			iparam++;
		}

		if (*params == 0)
		{
			while (*line == ' ')
				line++;

			for (;;)
			{
				if (*line == '\n')
					return command;

				if (*line == '#' || (*line == '/' && line[1] == '/'))
					return command;

				if (*line != '\t' && *line != ' ' && *line != '\n' && *line != '\r')
					break;

				line++;
			}

			ScriptError("Junk at end of line!");
			return 0;
		}

		while (*line == ' ')
			line++;

		if (*line++ != ',')
		{
			ScriptError("Expecting comma (,).");
			return 0;
		}

		while (*line == ' ')
			line++;
	}

	return command;
}

// [0x4a6544] [D]
// Scans an unsigned short (or, for SET_MODEL, a model name) into comp_iparam1 - 4.
int ScanShortParam(char **line, int param, short command)
{
	unsigned int value;
	int res;
	int neg;
	char name[256];

	neg = 0;

	if (**line == '-')
	{
		(*line)++;
		neg = 1;
	}

	res = sscanf(*line, "%u", &value);

	SkipNonNumbers(line);

	if ((res == 0 || res == -1) && command == CMD_SET_MODEL)
	{
		*line = ReadModelName(*line, name);

		// NOTE: sets only the low word of value, as in the original
		if (!FindModelName(name, (short *)&value))
		{
			ScriptError("Model number must be an unsigned\ninteger.  Please correct!");
			return 0;
		}
	}
	else if (res == 0 || res == -1)
	{
		ScriptError("Expecting integer value!");
		return 0;
	}

	if (neg)
		value = -(unsigned short)value;

	switch (param)
	{
		case 0:
			comp_iparam1 = value;
			break;
		case 1:
			comp_iparam2 = value;
			break;
		case 2:
			comp_iparam3 = value;
			break;
		case 3:
			comp_iparam4 = value;
			break;
	}

	return 1;
}

// [0x4a673d] [D] [A]
// Scans a float into comp_fparam1 - 4.
// NOTE: [A] sscanf_nearest instead of sscanf
int ScanFloatParam(char *line, int param)
{
	float value;
	int res;
	int neg;

	neg = 0;

	if (*line == '-')
	{
		line++;
		neg = 1;
	}

	res = sscanf_nearest(line, "%f", &value);

	SkipAnyNumbers(&line);

	if (res == 0 || res == -1)
	{
		ScriptError("Expecting floating point value!");
		return 0;
	}

	if (neg)
		value = -value;

	switch (param)
	{
		case 0:
			comp_fparam1 = value;
			break;
		case 1:
			comp_fparam2 = value;
			break;
		case 2:
			comp_fparam3 = value;
			break;
		case 3:
			comp_fparam4 = value;
			break;
	}

	return 1;
}

// [0x4a6862] [D]
void SkipNonNumbers(char **line)
{
	if (**line == '-')
		(*line)++;

	while (**line >= '0' && **line <= '9')
		(*line)++;
}

// [0x4a68b7] [D]
// Skips a number with a fraction; 0 on a second '.'.
int SkipAnyNumbers(char **line)
{
	int point;

	point = 0;

	if (**line == '-')
		(*line)++;

	for (;;)
	{
		while (**line >= '0' && **line <= '9')
			(*line)++;

		if (**line == '.' && point)
			return 0;

		if (**line != '.')
			break;

		(*line)++;
		point = 1;
	}

	return 1;
}

// [0x4a695a] [D]
// The length of word if the line starts with it as a whole word, else 0.
int CheckWord(char *line, char *word)
{
	int len;
	int i;

	len = strlen(word);

	if (len == 0)
		return 0;

	for (i = len; i; i--)
	{
		if (*line++ != *word++)
			return 0;
	}

	if ((*line <= 'Z' && *line >= 'A') || *line == '_')
		return 0;

	return len;
}

// [0x4a69ea] [D]
// Copies a model name (letters, digits, '_'; uppercased in the line) into name.
char *ReadModelName(char *line, char *name)
{
	if (*line >= '0' && *line <= '9')
	{
		*name = 0;
		return line;
	}

	while (*line)
	{
		if (*line >= 'a' && *line <= 'z')
			*line -= 0x20;

		if ((*line >= 'A' && *line <= 'Z') || (*line >= '0' && *line <= '9') || *line == '_')
			*name++ = *line++;
		else
			break;
	}

	*name = 0;

	return line;
}

// [0x4a6af8] [D]
// Appends the command and its scanned parameters to the model's command list.
char *StoreCommand(unsigned short command, EVENT_MODEL *em)
{
	int len;
	short *dest;
	char *end;

	len = (unsigned short)comm_len[command];

	if (em->commands_begin)
	{
		dest = (short *)DebugMalloc(len + em->commands_size, "D:\\Driver\\game6.4g\\compiler.c", 1231);

		if (!dest)
			ScriptError("Not enough memory for\nnew command!");

		CopyMem(dest, em->commands_begin, em->commands_size);
		DebugFree(em->commands_begin);

		em->commands_begin = dest;

		// over the old end marker
		dest = (short *)((char *)dest + em->commands_size - 2);

		em->commands_size += len;
	}
	else
	{
		em->commands_size = len + 2;

		dest = (short *)DebugMalloc(em->commands_size, "D:\\Driver\\game6.4g\\compiler.c", 1219);

		if (!dest)
			ScriptError("Not enough memory for\nnew command!");

		em->commands_begin = dest;
	}

	*dest = command;

	switch (command)
	{
		case CMD_MODEL_WAIT:
			GetCommandParams((char *)(dest + 1), "ii");
			break;
		case CMD_LOOP:
			GetCommandParams((char *)(dest + 1), "i");
			break;
		case CMD_LOOP_COUNT:
			GetCommandParams((char *)(dest + 1), "iii");
			break;
		default:
			GetCommandParams((char *)(dest + 1), command_params[command - 1]);
			break;
	}

	end = (char *)em->commands_begin + em->commands_size - 2;

	end[0] = 0;
	end[1] = 0;

	return end + 2;
}

// [0x4a6cc5] [D]
// Writes comp_fparam / comp_iparam in the order params gives.
char *GetCommandParams(char *dest, const char *params)
{
	int iparam;
	int fparam;

	iparam = 0;
	fparam = 0;

	for (;; params++)
	{
		if (*params == 'f')
		{
			switch (fparam)
			{
				case 0:
					*(float *)dest = comp_fparam1;
					break;
				case 1:
					*(float *)dest = comp_fparam2;
					break;
				case 2:
					*(float *)dest = comp_fparam3;
					break;
				case 3:
					*(float *)dest = comp_fparam4;
					break;
			}

			fparam++;
			dest += 4;
		}
		else if (*params == 'i')
		{
			switch (iparam)
			{
				case 0:
					*(short *)dest = comp_iparam1;
					break;
				case 1:
					*(short *)dest = comp_iparam2;
					break;
				case 2:
					*(short *)dest = comp_iparam3;
					break;
				case 3:
					*(short *)dest = comp_iparam4;
					break;
			}

			iparam++;
			dest += 2;
		}
		else
		{
			return dest;
		}
	}
}

// [0x4a6df4] [D]
// Adds "NAME = number" to event_model_name; index is used if there is no number.
void AddModelName(char *line, short index)
{
	unsigned char entry[256];
	unsigned int len;
	unsigned int number;
	char *names;

	len = 0;

	// collect the model name
	while (*line != '=' && *line != ' ')
	{
		entry[len] = *line++;

		// convert to uppercase if needed
		// BUG: lowercase 'z' is not accounted for!
		if (entry[len] > 0x60 && entry[len] < 0x7a)
			entry[len] -= 0x20;

		len++;
	}

	entry[len] = 0;

	// consume everything until the '='
	while (*line++ != '=')
		;

	// consume spaces before the index
	// BUG: hope you don't like indents!
	while (*line == ' ')
		line++;

	// NOTE: the original scans %u into the short argument's stack slot
	number = (unsigned short)index;
	sscanf(line, "%u", &number);

	entry[len++] = 0;

	*(short *)&entry[len] = number;

	len += 2;

	num_event_names++;

	if (event_model_name)
	{
		names = (char *)DebugMalloc(len + event_name_length, "D:\\Driver\\game6.4g\\compiler.c", 1391);

		if (names)
		{
			CopyMem(names, event_model_name, event_name_length - 2);
			memset(&names[event_name_length - 2], 0, len + 2);
			CopyMem(&names[event_name_length - 2], entry, len);
			DebugFree(event_model_name);

			event_model_name = names;
			event_name_length += len;
		}
		else if (!gScreen.fullscreen)
		{
			ScriptError("Not enough memory for\nmodel names list!");
		}
	}
	else
	{
		event_model_name = DebugMalloc(len + 2, "D:\\Driver\\game6.4g\\compiler.c", 1413);

		if (event_model_name)
		{
			memset(event_model_name, 0, len + 2);
			CopyMem(event_model_name, entry, len);
			event_name_length = len + 2;
		}
		else if (!gScreen.fullscreen)
		{
			ScriptError("Not enough memory for\nmodel names list!");
		}
	}
}

// [0x4a706d] [D]
int FindModelName(char *name, short *index)
{
	char text[256];
	char *names;
	int count;

	names = (char *)event_model_name;

	for (count = num_event_names; count; count--)
	{
		if (strcmp(name, names) == 0)
		{
			names += strlen(names) + 1;
			*index = *(short *)names;
			return 1;
		}

		names += strlen(names) + 1;
		names += 2;
	}

	sprintf(text, "Unkown model, %s.", name);

	if (!gScreen.fullscreen)
		ScriptError(text);

	return 0;
}

// [0x4a712b] [D]
char *SkipNonLetters(char *line)
{
	while ((*line < 'A' || (*line > 'Z' && *line < 'a') || *line > 'z') && *line != '_' && *line != '#')
		line++;

	return line;
}

// [0x4a7186] [D]
char *NextLine(char *line)
{
	script_line++;

	while (*line++ != '\n')
		;

	return line;
}

// [0x4a71b2] [D]
// 1 if the line starts with word after white space.
int MoveToWord(char *line, char *word)
{
	int i;

	i = strlen(word);

	while (*line == ' ' || *line == '\r' || *line == '\n' || *line == '\t')
		line++;

	while (i--)
	{
		if (line[i] != word[i])
			return 0;
	}

	return 1;
}

// [0x4a7243] [D]
// The line after the one starting with word.
char *MoveToLine(char *line, char *word)
{
	int found;

	do
	{
		found = MoveToWord(line, word);
		line = NextLine(line);
	} while (found != 1);

	return line;
}

// [0x4a7283] [D]
int ScriptError(char *text)
{
	char caption[52];

	sprintf(caption, "Error in line %u", script_line);

	return MessageBoxA(gScreen.handle, text, caption, MB_ICONWARNING);
}

// [0x4a72bb] [D]
// Reads count comma separated shorts.
char *ReadShortParams(char *line, short *params, int count)
{
	while (count--)
	{
		while (*line == ' ' || *line == '\t')
			line++;

		ScanShortParam(&line, 0, 0);

		*params++ = comp_iparam1;

		if (count > 0)
		{
			while (*line == ' ' || *line == '\t')
				line++;

			if (*line++ != ',')
			{
				ScriptError("Comma Expected!");
				return NULL;
			}
		}
	}

	return NextLine(line);
}
