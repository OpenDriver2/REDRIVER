/*
 * MScript.c - the mission scripts (Scripts\Missions\MissionN.dms): compiler and interpreter
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "MScript.c", 0x4f62a0 - 0x4fe701).
 *
 * A script is a text file of commands (DmsCommands) and variable assignments (DmsVars), one per
 * line, parameters separated by spaces or commas; "//" and "#" start a comment. Commands that
 * set up the level (LOADMAP, NIGHT, PLAYERCARTYPE ...) act while compiling; the others are
 * compiled into MissionList, a list of lines (command type, then int parameters or a string)
 * that ExecuteMissionScript runs from MissionHandler every frame.
 *
 * mission.objective: 0 none, 2 / 7 reach the target (7: trail blazer), 3 race, 4 scare the
 * opponent, 5 chase the lead car, 6 wreck the target cars, 8 wreck the gang car; 0x7FFF the
 * objective is complete (WAITFOROBJECTIVECOMPLETE goes on), 0xFF0 failed (failed_reason
 * 1 time out, 2 smashed, 3 out of range, 4 the car got away), 0x8000 set: the mission is
 * complete, 0xF1 the race is.
 * MISSION_TARGET.flags: 1 active, 2 out of the mission window, 4 a tail to lose, 8 follows an
 * event, 0x10 no pulse, 0x20 no arrow, 0x40 no tracer, 0x80 not detected, 0x100 a car to wreck,
 * 0x200 too fast, 0x400 no lose tail message.
 * The trail blazer courses (pTrailData) and the recording of one (TRAIL.DAT, F5 / F6) are here too.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dr_keys.h"

#include "dr_types.h"
#include "MScript.h"
#include "dr_globals.h"
#include "asmmath.h"
#include "debug.h"
#include "general.h"
#include "map.h"
#include "ai.h"
#include "file.h"
#include "handling.h"
#include "effects.h"
#include "shell.h"
#include "dr_time.h"
#include "cops.h"
#include "debris.h"
#include "felony.h"
#include "FMV/render.h"
#include "frontend/frontend.h"
#include "gsound.h"
#include "overlay.h"
#include "checkpoint.h"
#include "dr_cars.h"
#include "event.h"
#include "frontend/font.h"
#include "smashable.h"
#include "pause.h"
#include "pedest.h"
#include "pres.h"
#include "sound.h"
#include "camera.h"
#include "replays.h"
#include "Director.h"
#include "input.h"
#include "objanim.h"
#include "Engine/3dfx/gfxinit.h"
#include "Engine/3dfx/object.h"
#include "frontend/cheats.h"
#include "carnage.h"
#include "minicutscene.h"
#include "job_fx.h"
#include "Engine/3dfx/polydraw.h"
#include "dr_scores.h"
#include "glaunch.h"
#include "mirror.h"

// the mission script commands (DmsCommands[].type)
typedef enum DMSCommandType
{
	DMS_COM_CHECKPOINT = 0,
	DMS_COM_MISSIONEND = 1,
	DMS_COM_SET = 2,
	DMS_COM_TRIGGEREVENT = 3,
	DMS_COM_PLAYSOUND = 4,
	DMS_COM_RESETCHECKPOINTS = 5,
	DMS_COM_INITIALISERACE = 6,
	DMS_COM_WAITFOROBJECTIVECOMPLETE = 7,
	DMS_COM_TIMEPLAYER = 8,
	DMS_COM_COUNTDOWN = 9,
	DMS_COM_STOPPLAYER = 10,
	DMS_COM_RELEASEPLAYER = 11,
	DMS_COM_INCREASE = 12,
	DMS_COM_INC = 13,
	DMS_COM_DECREASE = 14,
	DMS_COM_DEC = 15,
	DMS_COM_ADD = 16,
	DMS_COM_SUB = 17,
	DMS_COM_CHECKPOINTLOGO = 18,
	DMS_COM_SETPLAYERPOSITION = 19,
	DMS_COM_SPAWNCOPS = 20,
	DMS_COM_PLAYCUTSCENE = 21,
	DMS_COM_INFORMPLAYER = 22,
	DMS_COM_RACECOMPLETE = 23,
	DMS_COM_WAIT = 24,
	DMS_COM_SETTARGET = 25,
	DMS_COM_ENABLETARGET = 26,
	DMS_COM_DISABLETARGET = 27,
	DMS_COM_STOPTIMER = 28,
	DMS_COM_CONTINUETIMER = 29,
	DMS_COM_RAINING = 30,
	DMS_COM_NIGHT = 31,
	DMS_COM_LIGHTNING = 32,
	DMS_COM_PLAY3DSOUND = 33,
	DMS_COM_LOADMAP = 34,
	DMS_COM_MISSIONWINDOW = 35,
	DMS_COM_ENABLEWINDOW = 36,
	DMS_COM_DISABLEWINDOW = 37,
	DMS_COM_SETMAXIMUMCOPS = 38,
	DMS_COM_DUSK = 39,
	DMS_COM_DAWN = 40,
	DMS_COM_TARGETEVENT = 41,
	DMS_COM_LAMPPOSITION = 42,
	DMS_COM_LAMPCOLOR = 43,
	DMS_COM_LAMPTYPE = 44,
	DMS_COM_LAMPRANGE = 45,
	DMS_COM_ENABLELAMP = 46,
	DMS_COM_DISABLELAMP = 47,
	DMS_COM_NOLENSFLARE = 48,
	DMS_COM_COPS_OFF = 49,
	DMS_COM_COPS_ON = 50,
	DMS_COM_SETFAILEDSOUND = 51,
	DMS_COM_SETPLAYERYROTATION = 52,
	DMS_COM_FORCEFELONYRATING = 53,
	DMS_COM_ADDCIVCAR = 54,
	DMS_COM_ENABLEPLAYERDAMAGE = 55,
	DMS_COM_DISABLEPLAYERDAMAGE = 56,
	DMS_COM_TRIGGEREVENTAT = 57,
	DMS_COM_KILLTIMER = 58,
	DMS_COM_ADDTOTIMER = 59,
	DMS_COM_SUBFROMTIMER = 60,
	DMS_COM_RANDOMCHASE = 61,
	DMS_COM_OVERHEADMAP = 62,
	DMS_COM_CRATES = 63,
	DMS_COM_PLAYERCARTYPE = 64,
	DMS_COM_PLAYERCARDAMAGE = 65,
	DMS_COM_SNOWING = 66,
	DMS_COM_SETLEADCARMODEL = 67,
	DMS_COM_SETLEADCARPALETTE = 68,
	DMS_COM_MCS_SETSTARTCOORD = 69,
	DMS_COM_MCS_SETDOORAXIS = 70,
	DMS_COM_MCS_SETNUMBADGUYS = 71,
	DMS_COM_MCS_SETRUN = 72,
	DMS_COM_MCS_SETWALK = 73,
	DMS_COM_MCS_ACTIVATE = 74,
	DMS_COM_MCS_WAITFORSCENE = 75,
	DMS_COM_MCS_SETSTARTHEIGHT = 76,
	DMS_COM_QUICKRENDER = 77,
	DMS_COM_NOTSOQUICKRENDER = 78,
	DMS_COM_MCS_DROPOFF = 79,
	DMS_COM_ADDCIVCARATREPLAY = 80,
	DMS_COM_MCS_USEREPLAYCARPOSITION = 81,
	DMS_COM_MCS_REPLAYCAROFFSET = 82,
	DMS_COM_USELASTPLAYERPOSITION = 83,
	DMS_COM_FOGGY = 84,
	DMS_COM_ENABLETARGETPULSE = 85,
	DMS_COM_DISABLETARGETPULSE = 86,
	DMS_COM_ENABLETARGETARROW = 87,
	DMS_COM_DISABLETARGETARROW = 88,
	DMS_COM_ENABLETARGETTRACER = 89,
	DMS_COM_DISABLETARGETTRACER = 90,
	DMS_COM_ENABLEDETECTION = 91,
	DMS_COM_DISABLEDETECTION = 92,
	DMS_COM_TARGETARROW = 93,
	DMS_COM_LOADTRAIL = 94,
	DMS_COM_DARKNIGHT = 95,
	DMS_COM_KILLEVENT = 96,
	DMS_COM_CHOPPER = 97,
	DMS_COM_CHOPPER_FORCE = 98,
	DMS_COM_CHOPPER_START = 99,
	DMS_COM_DISABLETAILMESSAGE = 100,
	DMS_COM_ENABLETAILMESSAGE = 101,
	DMS_COM_ADDDASHMESSAGE = 102,
	DMS_COM_SHOWDASHMESSAGE = 103,
	DMS_COM_CLEARDASHDATA = 104,
	DMS_COM_LABEL = 105,
	DMS_COM_GOTO = 106,
	DMS_COM_IFEQUAL = 107,
	DMS_COM_IFNOTEQUAL = 108,
	DMS_COM_IFLESSTHAN = 109,
	DMS_COM_IFGREATERTHAN = 110,
	DMS_COM_IFLESSOREQUAL = 111,
	DMS_COM_IFGREATEROREQUAL = 112,
	DMS_COM_GOSUB = 113,
	DMS_COM_RETURN = 114,
	DMS_COM_LOG = 115,
	DMS_COM_UPDATEBESTLAP = 116,
	DMS_COM_LOADSOUNDBANK = 117,
	DMS_COM_PLAYBANKEDSOUND = 118,
	DMS_COM_WAITUNTIL = 119,
	DMS_COM_MCS_SETENDCOORD = 120,
	DMS_COM_MCS_SETDOORCOORD = 121,
	DMS_COM_CREATEGANGCAR = 122,
	DMS_COM_INFORMPLAYERBYID = 123,
	DMS_COM_ADDDASHMESSAGEBYID = 124,
	DMS_COM_FAILEDMESSAGEID = 125,
	DMS_COM_WINDOWMESSAGEID = 126,
	DMS_COM_LOSETAILMESSAGEID = 127,
	DMS_COM_SMASHEDMESSAGEID = 128,
	DMS_COM_OUTOFRANGEMESSAGEID = 129,
	DMS_COM_CARGOTAWAYMESSAGEID = 130,
	DMS_COM_OPPONENTBARID = 131,
	DMS_COM_DAMAGEBARID = 132,
	DMS_COM_FELONYBARID = 133,
	DMS_COM_RANDOMWEATHER = 134,
	DMS_COM_LOSETAILSOUNDS = 135,
	DMS_COM_USESTOREDCARTYPE = 136,
	DMS_COM_ENABLEMODEL = 137,
	DMS_COM_DISABLEMODEL = 138,
	DMS_COM_STARTGHOSTLAP = 139,
	DMS_COM_ENDGHOSTLAP = 140,
	DMS_COM_MODELBONUS = 141,
	DMS_COM_RESETMAP = 142,
	DMS_COM_MAXSPEEDMESSAGEID = 143,
	DMS_COM_MODELTARGETRANGE = 144,
	DMS_COM_MCS_SETCAMERASTART = 145,
	DMS_COM_MCS_SETCAMERAEND = 146,
	DMS_COM_CUTTOCAR = 147,
	DMS_COM_DRAW321GO = 148,
	DMS_COM_DRAWFINISHED = 149,
	DMS_COM_TRAININGMODE = 150,
	DMS_COM_TRAINING = 151,
} DMSCommandType;

// DMSVariable.type
typedef enum DMSVariableType
{
	DMS_VAR_NONE = 0,
	DMS_VAR_STRING = 1,
	DMS_VAR_FLOAT = 2,
	DMS_VAR_INT = 3,
	DMS_VAR_BOOL = 4,
	DMS_VAR_END = 127,		// ends DmsVars
} DMSVariableType;

// the results of the compiler's functions
typedef enum DMSErrorType
{
	DMS_SUCCESS = 0,
	DMS_OK = 1,
	DMS_WORD_VALUE = 2,
	DMS_OUT_OF_MEMORY = 3,
	DMS_END_OF_FILE = 4,
	DMS_FILE_ERROR = 5,
	DMS_BAD_COMMA = 6,
	DMS_BAD_FILE = 7,
	DMS_BAD_LINE = 8,
	DMS_BAD_COMMAND = 9,
	DMS_END_OF_LINE = 10,
	DMS_COMPILED = 11,
	DMS_ILLEGAL_PARAM = 12,
	DMS_ERROR = 13,
} DMSErrorType;

// a script command: count int parameters, compiled into a line of size bytes
typedef struct DMSCommand
{
	char name[256];
	int count;
	int type;			// DMSCommandType
	int size;
	int unk_3;
} DMSCommand;

typedef struct DMS_LABEL
{
	char *name;
	int line;			// the MissionList line it stands before
} DMS_LABEL;

typedef struct LABEL_STORE
{
	int count;
	DMS_LABEL **labels;
} LABEL_STORE;

typedef struct MISSION_SOUNDS
{
	int FailedSound;
	int SmashedSound;
	int OutOfRangeSound;
	int GotAwaySound;
	int field_10;
	int WindowSound;
	int LoseTailSound;
} MISSION_SOUNDS;

typedef struct RANDOM_WEATHER
{
	int rain;
	int lightning;
	int snow;
	char fog;
} RANDOM_WEATHER;

int CompileMissionScript(char *filename);
int CompileMissionLine(void);
int GetMissionCommand(char *line, DMSCommand **cmd);
int GetVariableIndex(char *name);
int ReadParam(char *line, char *data);
int ReadString(char *line, char *data);
int ReadToken(char *line, char *data);
int CompileMissionCommand(DMSCommand *cmd);
int CompileSETCommand(DMSCommand *cmd, DMSVariable *var);
int AddScriptLine(size_t size);
const char *GetScriptErrorString(int code);
int ReadIntParams(DMSCommand *cmd);
void CopyStringVar(DMSVariable *var, char *value);
void InitMissionVariables(void);
void SetIntVar(int index, int value);
void SetBoolVar(int index, int value);
void SetStringVar(int index, char *value);
int AddLabel(char *name, int line, LABEL_STORE *store);
int GetLabel(char *name, LABEL_STORE *store);
void ClearLabels(LABEL_STORE *store);
void ClearDashData(void);
int DoRandomWeather(unsigned int level);
int DisplayMissionTargetData(void);
void UpdateEventTargets(void);
void HandleEventTargets(void);
void HandleMissionObjective(MISSION *mission);
void HandleMissionTarget(MISSION *mission);
void sub_4F77AA(int radius, MISSION *mission, MISSION_TARGET *target);
int sub_4F78F7(int radius, MISSION *mission, MISSION_TARGET *target);
int sub_4F79E6(int radius, MISSION *mission, MISSION_TARGET *target);
void MissionFailed(void);
void MissionCompleted(void);
void DMSHandleTimer(TIMER *timer);
void DMSInitTimerSeconds(TIMER *timer, int seconds);
void HandleTimeBonus(void);
void DisplayTimeBonus(void);
int AddDashMessage(char *text, int seconds);
char *GetDashMessage(int count);
void DisplayDebugInfo(void);
void PlayMissionSound(int sample);
int ExecuteMissionScript(int *list);
static int SetMessageVar(char *name, int id);
static const char *IfName(int type);
static int IfTrue(int type, int value, int param);
void DisplayTimer(TIMER *timer);
int LoadTrailData(char *filename);

//------------------------------------------------------------------------------------------
// initialised data

// the event script (.DES) of each level without a mission
static char *script_files[7] =
{
	"MIAMI_01.DES",
	"FRISCO_01.DES",
	"LA_01.DES",
	"NY_01.DES",
	"LEVEL4.DES",
	"LEVEL5.DES",
	"NEWCASTLE.DES",
};

//------------------------------------------------------------------------------------------
// uninitialised data

MISSION mission;
MISSION_DATA MLast;					// NOTE: owner guessed (most references)
MISSION_TARGET MissionTargets[10];
int MaxPlayerDamage;
char overhead_map[256];
char gChaseOverride;				// NOTE: owner guessed (most references)
int MissionLampsStatus[5];
int ActivateChopper;				// NOTE: owner guessed (most references)
int gLoseTailSounds[4];
CHECKPOINT_DATA Checkpoint;			// NOTE: untyped in IDA (72 bytes): a CHECKPOINT_DATA by its use (mission.checkpoint)
TRAIL_DATA *pTrailData;				// NOTE: owner by its place among MScript.c's statics
VECTOR4 ReplayCarEndPosition;		// where the lead car ended; NOTE: owner guessed (most references)
LAMP_OUTPUT MissionLamps[5];		// NOTE: owner by its place among MScript.c's statics

// NOTE: the MCS_* globals (dr_globals.h) are left to minicutscene.c, their probable owner;
// MCS_CameraStart and MCS_CameraEnd are untyped in IDA (16 bytes): VECTOR4 by their use

static int dms_sound_chan;
static int cones_recorded;
static int PlayerCarIsDodgy;
static int int_params[20];			// the int parameters of the command being compiled
static int curr_line;				// the script line being compiled (error messages)
static int bonus_display;
static int bonus_time;
static VECTOR4 ReplayCarOffset;
static int next_cone;
static int execution_line;			// the MissionList line ExecuteMissionScript runs next
static char replay_name[64];
static MISSION_SOUNDS MissionSounds;
static int StackPosition;
static int scare_target;
static int PlayerMustBeHidden;
static int *pnDashTimes;
static VECTOR4 LastPotentialPosition;
static int gNeedToLoseTail;
static int bonus_count;
static int nNumDashMessages;
static char MissionCommand[256];	// the token / string being compiled
static char *MissionBuffer;			// the script line being compiled
static int NumScriptLines;
static int MissionDisplayFlags;
static LABEL_STORE ScriptLabels;
static char **ppszDashMessages;
static char **MissionList;			// the compiled script
static int get_new_potential;
static char map_name[256];
static int MissionHasBeenCompleted;
static TIMER dms_countdown;			// NOTE: untyped in IDA (24 bytes): a TIMER by its use (mission.timer)
static u_short cone_spacing;		// frames to the next cone recorded; NOTE: untyped in IDA (2 bytes)
static int next_flying_cone;		// the cone_queue entry the next cone hit takes
static char bonus_text[64];			// "+n seconds!"
static FILE *TrailFile;				// TRAIL.DAT being recorded
static int CallStack[100];			// GOSUB return lines
static CELL_OBJECT cone_queue[4];	// the cones hit, flying off

// NOTE: no names in the database
static int dword_D000E0;
int giCurrentMissionNumber;

// (initialised data, continued)

// the weather RANDOMWEATHER picks from, per level (0 - 3)
static RANDOM_WEATHER weather[4] =
{
	{ 1, 1, 0, 0 },
	{ 1, 1, 0, 1 },
	{ 1, 1, 0, 0 },
	{ 1, 1, 1, 1 },
};

// the commands; an empty name ends the table
DMSCommand DmsCommands[] =
{
	{ "CHECKPOINT", 3, DMS_COM_CHECKPOINT, 16, 0 },
	{ "MISSIONEND", 0, DMS_COM_MISSIONEND, 4, 0 },
	{ "SET", 1, DMS_COM_SET, 4, 0 },
	{ "TRIGGEREVENT", 1, DMS_COM_TRIGGEREVENT, 8, 0 },
	{ "PLAYSOUND", 1, DMS_COM_PLAYSOUND, 8, 0 },
	{ "RESETCHECKPOINTS", 0, DMS_COM_RESETCHECKPOINTS, 4, 0 },
	{ "INITIALISERACE", 0, DMS_COM_INITIALISERACE, 4, 0 },
	{ "WAITFOROBJECTIVECOMPLETE", 0, DMS_COM_WAITFOROBJECTIVECOMPLETE, 4, 0 },
	{ "TIMEPLAYER", 0, DMS_COM_TIMEPLAYER, 4, 0 },
	{ "COUNTDOWN", 1, DMS_COM_COUNTDOWN, 8, 0 },
	{ "STOPPLAYER", 0, DMS_COM_STOPPLAYER, 4, 0 },
	{ "RELEASEPLAYER", 0, DMS_COM_RELEASEPLAYER, 4, 0 },
	{ "INCREASE", 0, DMS_COM_INCREASE, 4, 0 },
	{ "INC", 0, DMS_COM_INC, 4, 0 },
	{ "DECREASE", 0, DMS_COM_DECREASE, 4, 0 },
	{ "DEC", 0, DMS_COM_DEC, 4, 0 },
	{ "ADD", 0, DMS_COM_ADD, 4, 0 },
	{ "SUB", 0, DMS_COM_SUB, 4, 0 },
	{ "CHECKPOINTLOGO", 0, DMS_COM_CHECKPOINTLOGO, 4, 0 },
	{ "SETPLAYERPOSITION", 2, DMS_COM_SETPLAYERPOSITION, 12, 0 },
	{ "SPAWNCOPS", 0, DMS_COM_SPAWNCOPS, 4, 0 },
	{ "PLAYCUTSCENE", 0, DMS_COM_PLAYCUTSCENE, 4, 0 },
	{ "INFORMPLAYER", 0, DMS_COM_INFORMPLAYER, 4, 0 },
	{ "RACECOMPLETE", 0, DMS_COM_RACECOMPLETE, 4, 0 },
	{ "WAIT", 1, DMS_COM_WAIT, 8, 0 },
	{ "SETTARGET", 4, DMS_COM_SETTARGET, 20, 0 },
	{ "ENABLETARGET", 1, DMS_COM_ENABLETARGET, 8, 0 },
	{ "DISABLETARGET", 1, DMS_COM_DISABLETARGET, 8, 0 },
	{ "STOPTIMER", 0, DMS_COM_STOPTIMER, 4, 0 },
	{ "CONTINUETIMER", 0, DMS_COM_CONTINUETIMER, 4, 0 },
	{ "RAINING", 0, DMS_COM_RAINING, 4, 0 },
	{ "NIGHT", 0, DMS_COM_NIGHT, 4, 0 },
	{ "LIGHTNING", 0, DMS_COM_LIGHTNING, 4, 0 },
	{ "PLAY3DSOUND", 4, DMS_COM_PLAY3DSOUND, 20, 0 },
	{ "LOADMAP", 0, DMS_COM_LOADMAP, 4, 0 },
	{ "MISSIONWINDOW", 1, DMS_COM_MISSIONWINDOW, 8, 0 },
	{ "ENABLEWINDOW", 0, DMS_COM_ENABLEWINDOW, 4, 0 },
	{ "DISABLEWINDOW", 0, DMS_COM_DISABLEWINDOW, 4, 0 },
	{ "SETMAXIMUMCOPS", 1, DMS_COM_SETMAXIMUMCOPS, 8, 0 },
	{ "DUSK", 0, DMS_COM_DUSK, 4, 0 },
	{ "DAWN", 0, DMS_COM_DAWN, 4, 0 },
	{ "TARGETEVENT", 2, DMS_COM_TARGETEVENT, 12, 0 },
	{ "LAMPPOSITION", 4, DMS_COM_LAMPPOSITION, 20, 0 },
	{ "LAMPCOLOR", 4, DMS_COM_LAMPCOLOR, 20, 0 },
	{ "LAMPTYPE", 2, DMS_COM_LAMPTYPE, 12, 0 },
	{ "LAMPRANGE", 2, DMS_COM_LAMPRANGE, 12, 0 },
	{ "ENABLELAMP", 1, DMS_COM_ENABLELAMP, 8, 0 },
	{ "DISABLELAMP", 1, DMS_COM_DISABLELAMP, 8, 0 },
	{ "NOLENSFLARE", 0, DMS_COM_NOLENSFLARE, 4, 0 },
	{ "COPS_OFF", 0, DMS_COM_COPS_OFF, 4, 0 },
	{ "COPS_ON", 0, DMS_COM_COPS_ON, 4, 0 },
	{ "SETFAILEDSOUND", 1, DMS_COM_SETFAILEDSOUND, 8, 0 },
	{ "SETPLAYERYROTATION", 1, DMS_COM_SETPLAYERYROTATION, 8, 0 },
	{ "FORCEFELONYRATING", 1, DMS_COM_FORCEFELONYRATING, 8, 0 },
	{ "ADDCIVCAR", 7, DMS_COM_ADDCIVCAR, 32, 0 },
	{ "ENABLEPLAYERDAMAGE", 0, DMS_COM_ENABLEPLAYERDAMAGE, 4, 0 },
	{ "DISABLEPLAYERDAMAGE", 0, DMS_COM_DISABLEPLAYERDAMAGE, 4, 0 },
	{ "TRIGGEREVENTAT", 1, DMS_COM_TRIGGEREVENTAT, 8, 0 },
	{ "KILLTIMER", 0, DMS_COM_KILLTIMER, 4, 0 },
	{ "ADDTOTIMER", 1, DMS_COM_ADDTOTIMER, 8, 0 },
	{ "SUBFROMTIMER", 1, DMS_COM_SUBFROMTIMER, 8, 0 },
	{ "RANDOMCHASE", 0, DMS_COM_RANDOMCHASE, 4, 0 },
	{ "OVERHEADMAP", 0, DMS_COM_OVERHEADMAP, 4, 0 },
	{ "CRATES", 0, DMS_COM_CRATES, 4, 0 },
	{ "PLAYERCARTYPE", 1, DMS_COM_PLAYERCARTYPE, 8, 0 },
	{ "PLAYERCARDAMAGE", 1, DMS_COM_PLAYERCARDAMAGE, 8, 0 },
	{ "SNOWING", 0, DMS_COM_SNOWING, 4, 0 },
	{ "SETLEADCARMODEL", 1, DMS_COM_SETLEADCARMODEL, 8, 0 },
	{ "SETLEADCARPALETTE", 1, DMS_COM_SETLEADCARPALETTE, 8, 0 },
	{ "MCS_SETSTARTCOORD", 3, DMS_COM_MCS_SETSTARTCOORD, 16, 0 },
	{ "MCS_SETDOORAXIS", 1, DMS_COM_MCS_SETDOORAXIS, 8, 0 },
	{ "MCS_SETNUMBADGUYS", 1, DMS_COM_MCS_SETNUMBADGUYS, 8, 0 },
	{ "MCS_SETRUN", 0, DMS_COM_MCS_SETRUN, 4, 0 },
	{ "MCS_SETWALK", 0, DMS_COM_MCS_SETWALK, 4, 0 },
	{ "MCS_ACTIVATE", 0, DMS_COM_MCS_ACTIVATE, 4, 0 },
	{ "MCS_WAITFORSCENE", 0, DMS_COM_MCS_WAITFORSCENE, 4, 0 },
	{ "MCS_SETSTARTHEIGHT", 1, DMS_COM_MCS_SETSTARTHEIGHT, 8, 0 },
	{ "QUICKRENDER", 0, DMS_COM_QUICKRENDER, 4, 0 },
	{ "NOTSOQUICKRENDER", 0, DMS_COM_NOTSOQUICKRENDER, 4, 0 },
	{ "MCS_DROPOFF", 1, DMS_COM_MCS_DROPOFF, 8, 0 },
	{ "ADDCIVCARATREPLAY", 5, DMS_COM_ADDCIVCARATREPLAY, 24, 0 },
	// NOTE: the type of MCS_USEREPLAYCARPOSITION, as in the binary
	{ "MCS_REPLAYCAROFFSET", 2, DMS_COM_MCS_USEREPLAYCARPOSITION, 12, 0 },
	{ "MCS_USEREPLAYCARPOSITION", 2, DMS_COM_MCS_USEREPLAYCARPOSITION, 12, 0 },
	{ "USELASTPLAYERPOSITION", 0, DMS_COM_USELASTPLAYERPOSITION, 4, 0 },
	{ "FOGGY", 0, DMS_COM_FOGGY, 4, 0 },
	{ "ENABLETARGETPULSE", 1, DMS_COM_ENABLETARGETPULSE, 8, 0 },
	{ "DISABLETARGETPULSE", 1, DMS_COM_DISABLETARGETPULSE, 8, 0 },
	{ "ENABLETARGETARROW", 1, DMS_COM_ENABLETARGETARROW, 8, 0 },
	{ "DISABLETARGETARROW", 1, DMS_COM_DISABLETARGETARROW, 8, 0 },
	{ "ENABLETARGETTRACER", 1, DMS_COM_ENABLETARGETTRACER, 8, 0 },
	{ "DISABLETARGETTRACER", 1, DMS_COM_DISABLETARGETTRACER, 8, 0 },
	{ "ENABLEDETECTION", 1, DMS_COM_ENABLEDETECTION, 8, 0 },
	{ "DISABLEDETECTION", 1, DMS_COM_DISABLEDETECTION, 8, 0 },
	{ "TARGETARROW", 4, DMS_COM_TARGETARROW, 20, 0 },
	{ "LOADTRAIL", 0, DMS_COM_LOADTRAIL, 4, 0 },
	{ "DARKNIGHT", 0, DMS_COM_DARKNIGHT, 4, 0 },
	{ "KILLEVENT", 1, DMS_COM_KILLEVENT, 8, 0 },
	{ "CHOPPER", 4, DMS_COM_CHOPPER, 20, 0 },
	{ "CHOPPER_FORCE", 0, DMS_COM_CHOPPER_FORCE, 4, 0 },
	{ "CHOPPER_START", 1, DMS_COM_CHOPPER_START, 8, 0 },
	{ "DISABLETAILMESSAGE", 1, DMS_COM_DISABLETAILMESSAGE, 8, 0 },
	{ "ENABLETAILMESSAGE", 1, DMS_COM_ENABLETAILMESSAGE, 8, 0 },
	{ "ADDDASHMESSAGE", 0, DMS_COM_ADDDASHMESSAGE, 4, 0 },
	{ "SHOWDASHMESSAGE", 1, DMS_COM_SHOWDASHMESSAGE, 8, 0 },
	{ "CLEARDASHDATA", 0, DMS_COM_CLEARDASHDATA, 4, 0 },
	{ "LABEL", 0, DMS_COM_LABEL, 4, 0 },
	{ "GOTO", 0, DMS_COM_GOTO, 4, 0 },
	{ "IFEQUAL", 0, DMS_COM_IFEQUAL, 4, 0 },
	{ "IFNOTEQUAL", 0, DMS_COM_IFNOTEQUAL, 4, 0 },
	{ "IFLESSTHAN", 0, DMS_COM_IFLESSTHAN, 4, 0 },
	{ "IFGREATERTHAN", 0, DMS_COM_IFGREATERTHAN, 4, 0 },
	{ "IFLESSOREQUAL", 0, DMS_COM_IFLESSOREQUAL, 4, 0 },
	{ "IFGREATEROREQUAL", 0, DMS_COM_IFGREATEROREQUAL, 4, 0 },
	{ "GOSUB", 0, DMS_COM_GOSUB, 4, 0 },
	{ "RETURN", 0, DMS_COM_RETURN, 4, 0 },
	{ "LOG", 0, DMS_COM_LOG, 4, 0 },
	{ "UPDATEBESTLAP", 0, DMS_COM_UPDATEBESTLAP, 4, 0 },
	{ "LOADSOUNDBANK", 1, DMS_COM_LOADSOUNDBANK, 8, 0 },
	{ "PLAYBANKEDSOUND", 1, DMS_COM_PLAYBANKEDSOUND, 8, 0 },
	{ "WAITUNTIL", 1, DMS_COM_WAITUNTIL, 8, 0 },
	{ "CREATEGANGCAR", 1, DMS_COM_CREATEGANGCAR, 8, 0 },
	{ "MCS_SETENDCOORD", 3, DMS_COM_MCS_SETENDCOORD, 16, 0 },
	{ "MCS_SETDOORCOORD", 3, DMS_COM_MCS_SETDOORCOORD, 16, 0 },
	{ "MCS_SETCAMERASTART", 5, DMS_COM_MCS_SETCAMERASTART, 24, 0 },
	{ "MCS_SETCAMERAEND", 5, DMS_COM_MCS_SETCAMERAEND, 24, 0 },
	{ "INFORMPLAYERBYID", 2, DMS_COM_INFORMPLAYERBYID, 12, 0 },
	{ "ADDDASHMESSAGEBYID", 2, DMS_COM_ADDDASHMESSAGEBYID, 12, 0 },
	{ "FAILEDMESSAGEID", 1, DMS_COM_FAILEDMESSAGEID, 8, 0 },
	{ "WINDOWMESSAGEID", 1, DMS_COM_WINDOWMESSAGEID, 8, 0 },
	{ "LOSETAILMESSAGEID", 1, DMS_COM_LOSETAILMESSAGEID, 8, 0 },
	{ "SMASHEDMESSAGEID", 1, DMS_COM_SMASHEDMESSAGEID, 8, 0 },
	{ "OUTOFRANGEMESSAGEID", 1, DMS_COM_OUTOFRANGEMESSAGEID, 8, 0 },
	{ "CARGOTAWAYMESSAGEID", 1, DMS_COM_CARGOTAWAYMESSAGEID, 8, 0 },
	{ "OPPONENTBARID", 1, DMS_COM_OPPONENTBARID, 8, 0 },
	{ "DAMAGEBARID", 1, DMS_COM_DAMAGEBARID, 8, 0 },
	{ "FELONYBARID", 1, DMS_COM_FELONYBARID, 8, 0 },
	{ "RANDOMWEATHER", 0, DMS_COM_RANDOMWEATHER, 4, 0 },
	{ "LOSETAILSOUNDS", 0, DMS_COM_LOSETAILSOUNDS, 4, 0 },
	{ "USESTOREDCARTYPE", 0, DMS_COM_USESTOREDCARTYPE, 4, 0 },
	{ "ENABLEMODEL", 1, DMS_COM_ENABLEMODEL, 8, 0 },
	{ "DISABLEMODEL", 1, DMS_COM_DISABLEMODEL, 8, 0 },
	{ "STARTGHOSTLAP", 0, DMS_COM_STARTGHOSTLAP, 4, 0 },
	{ "ENDGHOSTLAP", 0, DMS_COM_ENDGHOSTLAP, 4, 0 },
	{ "MODELBONUS", 0, DMS_COM_MODELBONUS, 4, 0 },
	{ "RESETMAP", 0, DMS_COM_RESETMAP, 4, 0 },
	{ "MAXSPEEDMESSAGEID", 1, DMS_COM_MAXSPEEDMESSAGEID, 8, 0 },
	{ "MODELTARGETRANGE", 2, DMS_COM_MODELTARGETRANGE, 12, 0 },
	{ "CUTTOCAR", 0, DMS_COM_CUTTOCAR, 4, 0 },
	{ "DRAW321GO", 0, DMS_COM_DRAW321GO, 4, 0 },
	{ "DRAWFINISHED", 0, DMS_COM_DRAWFINISHED, 4, 0 },
	{ "TRAININGMODE", 1, DMS_COM_TRAININGMODE, 8, 0 },
	{ "TRAINING", 0, DMS_COM_TRAINING, 4, 0 },
	{ "", 0, 0, 0, 0 },
};

// the variables, data pointing at the global each sets; type 127 ends the table
DMSVariable DmsVars[] =
{
	{ "EVENTSCRIPT", DMS_VAR_STRING, NULL, "default.des", 0 },
	{ "COP_AGGRESSION", DMS_VAR_INT, &FelonyRating, NULL, 5 },
	{ "NUM_CHECKPOINTS", DMS_VAR_INT, &Checkpoint.count, NULL, 0 },
	{ "OBJECTIVE", DMS_VAR_INT, &mission.objective, NULL, 0 },
	{ "CHECKPOINT_INDEX", DMS_VAR_INT, &Checkpoint.index, NULL, 0 },
	{ "FAILEDMESSAGE", DMS_VAR_STRING, NULL, "The message for a failed mission has not been set!", 0 },
	{ "MAX_COPS", DMS_VAR_INT, &max_active_cops, NULL, 0 },
	{ "WINDOWMESSAGE", DMS_VAR_STRING, NULL, "The message for missing the mission window has not been set!", 0 },
	{ "USEBADGUYMODEL", DMS_VAR_BOOL, &gUseBadGuyModel, NULL, 0 },
	{ "USECOPMODEL", DMS_VAR_BOOL, &gUseCopModel, NULL, 1 },
	{ "MAXPLAYERDAMAGE", DMS_VAR_INT, &MaxPlayerDamage, NULL, 19000 },
	{ "MISSIONID", DMS_VAR_INT, &mission.id, NULL, 0 },
	{ "DAMAGESCALE", DMS_VAR_INT, &player_damage_factor, NULL, 100 },
	{ "NEEDTOLOSETAIL", DMS_VAR_BOOL, &gNeedToLoseTail, NULL, 0 },
	{ "DEBUGINFO", DMS_VAR_BOOL, &mission.debugInfo, NULL, 0 },
	{ "COP_RESPAWN", DMS_VAR_INT, &gCopRespawnTime, NULL, 0 },
	{ "MAXSCARERESULT", DMS_VAR_INT, &scare_target, NULL, 0 },
	{ "CHASEFILENAME", DMS_VAR_STRING, NULL, "no_chase", 0 },
	{ "DODGYCAR", DMS_VAR_BOOL, &PlayerCarIsDodgy, NULL, 0 },
	{ "MUSTBEHIDDEN", DMS_VAR_BOOL, &PlayerMustBeHidden, NULL, 0 },
	{ "LOSETAILMESSAGE", DMS_VAR_STRING, NULL, "LOSETAILMESSAGE command missing", 0 },
	{ "SMASHEDMESSAGE", DMS_VAR_STRING, NULL, "SMASHEDMESSAGE command missing", 0 },
	{ "OUTOFRANGEMESSAGE", DMS_VAR_STRING, NULL, "OUTOFRANGEMESSAGE command missing", 0 },
	{ "CARGOTAWAYMESSAGE", DMS_VAR_STRING, NULL, "CARGOTAWAYMESSAGE command missing", 0 },
	{ "SMASHEDSOUND", DMS_VAR_INT, &MissionSounds.SmashedSound, NULL, -1 },
	{ "OUTOFRANGESOUND", DMS_VAR_INT, &MissionSounds.OutOfRangeSound, NULL, -1 },
	{ "GOTAWAYSOUND", DMS_VAR_INT, &MissionSounds.GotAwaySound, NULL, -1 },
	{ "WINDOWSOUND", DMS_VAR_INT, &MissionSounds.WindowSound, NULL, -1 },
	{ "LOSETAILSOUND", DMS_VAR_INT, &MissionSounds.LoseTailSound, NULL, -1 },
	{ "TARGETDAMAGE", DMS_VAR_INT, &mission.target_damage, NULL, -1 },
	{ "CHASEHQMODE", DMS_VAR_BOOL, &gChaseHQMode, NULL, 0 },
	{ "CHASEMODEHITS", DMS_VAR_INT, &gChaseModeHitsRequired, NULL, 100 },
	{ "OUTERRADIUS", DMS_VAR_INT, &mission.outer_radius, NULL, 3000 },
	{ "WINDOWRADIUS", DMS_VAR_INT, &mission.window_radius, NULL, 0 },
	{ "MAXSPEED", DMS_VAR_INT, &mission.max_speed, NULL, 0 },
	{ "MAXSPEEDENABLE", DMS_VAR_BOOL, &mission.maxSpeedEnable, NULL, 0 },
	{ "COPS_SPEEDSCALE", DMS_VAR_INT, &gCopsSpeedScale, NULL, 4096 },
	{ "COPS_POWERSCALE", DMS_VAR_INT, &gCopsPowerScale, NULL, 4096 },
	{ "CANBEFAILED", DMS_VAR_BOOL, &mission.canBeFailed, NULL, 0 },
	{ "TRAILMODEL", DMS_VAR_INT, &mission.trail_model, NULL, 426 },
	{ "COPS_AUTOPOWERSCALE", DMS_VAR_INT, &CopsAutoPowerScale, NULL, 819 },
	{ "COPS_AUTOSPEEDSCALE", DMS_VAR_INT, &gCopsAutoSpeedScale, NULL, 819 },
	{ "MAXSPEEDMESSAGE", DMS_VAR_STRING, NULL, "MAXSPEEDMESSAGE command missing", 0 },
	{ "PAYBACK", DMS_VAR_BOOL, &mission.payback, NULL, 0 },
	{ "PAYBACKRADIUS", DMS_VAR_INT, &mission.payback_radius, NULL, 0 },
	{ "OPPONENTBAR", DMS_VAR_STRING, NULL, "Not set", 0 },
	{ "USERVAR1", DMS_VAR_INT, &mission.user_var_1, NULL, 0 },
	{ "USERVAR2", DMS_VAR_INT, &mission.user_var_2, NULL, 0 },
	{ "USERVAR3", DMS_VAR_INT, &mission.user_var_3, NULL, 0 },
	{ "USERVAR4", DMS_VAR_INT, &mission.user_var_4, NULL, 0 },
	{ "USERVAR5", DMS_VAR_INT, &mission.user_var_5, NULL, 0 },
	{ "LAPCOUNT", DMS_VAR_INT, &gLapCount, NULL, 0 },
	{ "NUMLAPS", DMS_VAR_INT, &gNumLaps, NULL, 0 },
	{ "CARNAGESCORE", DMS_VAR_INT, &carnage_score, NULL, 0 },
	{ "BITCHINGMODE", DMS_VAR_INT, &gBitchingMode, NULL, 0 },
	{ "DAMAGEBAR", DMS_VAR_STRING, NULL, "Not set", 0 },
	{ "FELONYBAR", DMS_VAR_STRING, NULL, "Not set", 0 },
	{ "ROADBLOCKRESPAWN", DMS_VAR_INT, &RoadblockTimer, NULL, 450 },
	{ "ROADBLOCKTRIGGER", DMS_VAR_INT, &gCopData.roadblockTrigger, NULL, 0 },
	{ "COPS_BATTERPLAYER", DMS_VAR_BOOL, &gBatterPlayer, NULL, 0 },
	{ "COPS_IMMORTAL", DMS_VAR_BOOL, &gCopsImmortal, NULL, 0 },
	{ "ENABLELOSETAILSOUNDS", DMS_VAR_BOOL, &gEnableLoseTailSounds, NULL, 0 },
	{ "DO_PUPPY_DOG_COP", DMS_VAR_BOOL, &gPuppyDogCop, NULL, 0 },
	{ "ALLOWCOPSPEECH", DMS_VAR_BOOL, &gAllowCopSpeech, NULL, 1 },
	{ "DODGY_BRAKE_FACTOR", DMS_VAR_INT, &gDodgyBrakeFactor, NULL, 4096 },
	{ "DODGY_SPEED_FACTOR", DMS_VAR_INT, &gDodgySpeedFactor, NULL, 4096 },
	{ "GHOSTCARTYPE", DMS_VAR_INT, &mission.ghost_car_type, NULL, 0 },
	{ "EVENTTARGETRANGE0", DMS_VAR_INT, &gEventTargetRange[0], NULL, 0 },
	{ "EVENTTARGETRANGE1", DMS_VAR_INT, &gEventTargetRange[1], NULL, 0 },
	{ "EVENTTARGETRANGE2", DMS_VAR_INT, &gEventTargetRange[2], NULL, 0 },
	{ "EVENTTARGETRANGE3", DMS_VAR_INT, &gEventTargetRange[3], NULL, 0 },
	{ "EVENTTARGETRANGE4", DMS_VAR_INT, &gEventTargetRange[4], NULL, 0 },
	{ "COPWIPEOUTDAMAGE", DMS_VAR_INT, &gCopCarWipedOutDamage, NULL, 7000 },
	{ "DEMOSCRIPT", DMS_VAR_INT, &giDemoScript, NULL, -1 },
	{ "NEXTSCRIPT", DMS_VAR_INT, &gNextScript, NULL, -1 },
	{ "TRYITSCRIPT", DMS_VAR_INT, &gTryItScript, NULL, -1 },
	{ "", 127, NULL, NULL, 0 },
};

// NOTE: moved here from the generated stubs; no IDA mark, the owner is a guess from the users
// and the neighbouring data
int ChaseCarPalette;
int ChaseCarType;
int gChaseModeHits;
int gDoChopper;
int gChaseModeHitsRequired;
int gPlayerForceDamage;
int giDemoScript;
int MissionQuickRender[2];
int MissionNotSoQuickRender[2];
MISSION_DATA MStart;
MISSION_DATA MNext;
int gDoCrates;
int gTryItScript;
int gNextScript;

// [0x4f62a0] [D]
// Resets the mission state and compiles Scripts\Missions\Mission<n>.dms; mission 0 is a level
// without a mission (its standard event script). Returns 0 when the script fails to compile.
int LoadMission(int mission_number)
{
	int code;
	int index;

	if (gMissionRecording)
		gTrainingMode = 1;
	else
		gTrainingMode = 2;

	next_cone = 0;
	cones_recorded = 0;

	ClearMem(int_params, sizeof(int_params));

	execution_line = 0;
	curr_line = 0;
	NumScriptLines = 0;
	MissionDisplayFlags = 0;
	StackPosition = 0;
	bonus_display = 0;
	scare_target = 0;
	dword_D000E0 = 0;
	dms_sound_chan = 0;
	MaxPlayerDamage = 0;
	ppszDashMessages = NULL;
	pnDashTimes = NULL;
	nNumDashMessages = 0;
	MissionHasBeenCompleted = 0;
	PlayerCarIsDodgy = 0;
	PlayerMustBeHidden = 0;
	get_new_potential = 0;
	text3d[1] = 0;

	LogQuick("* LoadMission *");

	memset(&mission, 0, sizeof(mission));
	memset(MissionTargets, 0, sizeof(MissionTargets));
	memset(&ScriptLabels, 0, sizeof(ScriptLabels));

	StackPosition = 0;

	ClearLabels(&ScriptLabels);

	bonus_time = 0;
	bonus_count = 0;
	bonus_display = 0;

	if (gDoInterview)
		gPlayerCarModelType = 0;

	gPlayerForceDamage = 0;
	pTrailData = NULL;

	InitMissionVariables();

	MissionQuickRender[0] = -1;
	MissionQuickRender[1] = -1;
	MissionNotSoQuickRender[0] = -1;
	MissionNotSoQuickRender[1] = -1;

	MissionHasBeenCompleted = 0;
	gDoChopper = 0;
	ActivateChopper = 0;
	gChaseModeHits = 0;
	gNeedToLoseTail = 0;
	gDoCrates = 0;
	MCS_DropOff = 0;

	ReplayCarOffset.vx = 0.0f;
	ReplayCarOffset.vy = 0.0f;
	ReplayCarOffset.vz = 0.0f;

	get_new_potential = 1;
	overhead_map[0] = 0;

	memset(&MissionSounds, 0xFF, sizeof(MissionSounds));

	NumScriptLines = 0;
	MissionList = NULL;
	dms_sound_chan = 0;
	giCurrentMissionNumber = mission_number;

	if (mission_number == 0)
	{
		// no mission: the level's standard event script
		sprintf(map_name, "%s", level_files[GameLevel]);

		mission.map_name = map_name;
		mission.objective = 0;
		mission.failed_reason = 0;

		index = GetVariableIndex("EVENTSCRIPT");

		if (index == -1)
		{
			LogQuick("FAILED: Could not find variable for event script filename.");
			return 0;
		}

		CopyStringVar(&DmsVars[index], script_files[GameLevel]);

		LogQuick(">> LoadMission Complete - No Mission Required, Standard Level Initialised - Routine Terminated <<");
		return 1;
	}

	sprintf(MissionCommand, "Attempting to load: Scripts\\Missions\\Mission%d.dms", mission_number);
	LogQuick(MissionCommand);

	sprintf(MissionCommand, "Scripts\\Missions\\Mission%d.dms", mission_number);

	code = CompileMissionScript(MissionCommand);

	if (code == DMS_SUCCESS || code == DMS_END_OF_LINE || code == DMS_END_OF_FILE)
	{
		PlayRender(MissionQuickRender[0], MissionQuickRender[1]);

		mission.loaded = 1;

		LogQuick(">> LoadMission Complete - Routine Terminated <<");
		return 1;
	}

	if (code == DMS_OUT_OF_MEMORY)
	{
		LogQuick("FAILED: CompileMissionScript - OutOfMemory");
	}
	else
	{
		sprintf(MissionCommand, "FAILED: CompileMissionScript - %s, Line: %d", GetScriptErrorString(code), curr_line);
		LogQuick(MissionCommand);
	}

	return 0;
}

// [0x4f66b6] [D]
// Compiles the script file line by line; returns DMS_END_OF_FILE at the end, else the first
// error.
int CompileMissionScript(char *filename)
{
	FILE *fp;
	int code;

	memset(&mission, 0, sizeof(mission));
	memset(&dms_countdown, 0, sizeof(dms_countdown));
	memset(&Checkpoint, 0, sizeof(Checkpoint));
	memset(MissionLamps, 0, sizeof(MissionLamps));
	memset(MissionLampsStatus, 0, sizeof(MissionLampsStatus));

	mission.checkpoint = &Checkpoint;
	mission.timer = &dms_countdown;

	DestroyMissionScript();
	InitMissionVariables();

	MissionDisplayFlags = 0;
	MCS_CameraPositionsSet = 0;

	MissionBuffer = (char *)DebugMalloc(1024, "D:\\Driver\\game6.4g\\MScript.c", 329);

	if (!MissionBuffer)
		return DMS_OUT_OF_MEMORY;

	fp = fopen(filename, "rt");

	if (!fp)
		return DMS_FILE_ERROR;

	// (the original calls FileSize here and drops the result)
	FileSize(fp);

	execution_line = 0;
	code = DMS_SUCCESS;

	for (curr_line = 0; ; curr_line++)
	{
		if (!fgets(MissionBuffer, 1024, fp))
		{
			fclose(fp);
			return DMS_END_OF_FILE;
		}

		code = CompileMissionLine();

		if (code != DMS_SUCCESS && code != DMS_END_OF_LINE)
			break;
	}

	fclose(fp);

	return code;
}

// [0x4f684b] [D]
// Every frame: runs the script up to a line that waits, then the checkpoint, the timer and the
// objective; a smashed player's car fails the mission.
void MissionHandler(void)
{
	int dx;
	int dz;
	int code;

	if (!mission.loaded)
		return;

	memcpy(MLast.damage, car_data[0].damage, sizeof(MLast.damage));
	MLast.totalDamage = car_data[0].totalDamage;
	MLast.direction = car_data[0].direction;
	MLast.felony = FelonyRating;

	UpdateEventTargets();

	if (text3d[1])
		Handle3dText();

	if (mission.objective != 0xFF0)
		HandleEventTargets();

	if (gMissionReplaying)
	{
		if (!LeadCarHit)
		{
			MStart = MLast;

			if (get_new_potential)
			{
				get_new_potential = 0;
				LastPotentialPosition.vx = (float)car_data[0].location.vx;
				LastPotentialPosition.vz = (float)car_data[0].location.vz;
			}

			dx = float_to_int(LastPotentialPosition.vx) - car_data[65].location.vx;
			dz = float_to_int(LastPotentialPosition.vz) - car_data[65].location.vz;

			// the lead car is 2000 away from the position taken: the player's last position
			if (dx * dx + dz * dz > 4000000)
			{
				MLast.position.vx = LastPotentialPosition.vx;
				MLast.position.vz = LastPotentialPosition.vz;

				get_new_potential = 1;
			}
		}
	}
	else
	{
		MLast.position.vx = (float)car_data[0].location.vx;
		MLast.position.vz = (float)car_data[0].location.vz;
	}

	if (MissionList && NumScriptLines && !GamePaused)
	{
		// the lines up to one that waits (DMS_ERROR)
		do
		{
			if (execution_line >= NumScriptLines || mission.objective == 0xFF0)
				break;

			code = ExecuteMissionScript((int *)MissionList[execution_line]);
		}
		while (code != DMS_ERROR);

		if (mission.objective != 0xFF0)
		{
			HandleCheckpoint(mission.checkpoint);
			DMSHandleTimer(mission.timer);
			HandleMissionObjective(&mission);

			if (mission.objective & 0x8000)
				MissionCompleted();

			if (mission.inform_count)
				mission.inform_count--;

			if ((mission.flags & 4) && gPlayerCarDamage > MaxPlayerDamage)
			{
				mission.objective = 0xFF0;
				mission.failed_reason = 2;
				mission.pause_timer = 90;
			}

			if (mission.objective == 0xFF0)
				mission.field_C = 0;
		}

		if (mission.objective == 0xFF0)
			MissionFailed();

		HandleTimeBonus();
	}
	else if (mission.objective == 0xFF0)
	{
		MissionFailed();
	}
	else if (gPlayerCarDamage > MaxPlayerDamage)
	{
		mission.objective = 0xFF0;
		mission.failed_reason = 2;
		mission.pause_timer = 90;
		mission.field_C = 0;
	}
}

// [0x4f6b53] [D]
// Draws the timer, then the failure message (playing its sound once) or a target's message, else
// the InformPlayer text; and the time bonus.
void DisplayMissionData(void)
{
	TIMER *timer;
	int index;
	int text_drawn;

	text_drawn = 0;

	if (!mission.loaded)
		return;

	timer = mission.timer;

	DisplayDebugInfo();

	SwapPalette(24);
	SetTextColour(255, 255, 255);

	if (timer && (timer->flags & 4))
		DisplayTimer(timer);

	if (mission.objective == 0xFF0)
	{
		switch (mission.failed_reason)
		{
			case 1:
				index = GetVariableIndex("FAILEDMESSAGE");

				if (MissionSounds.FailedSound != -1)
					PlayMissionSound(MissionSounds.FailedSound + 0xF000);

				MissionSounds.FailedSound = -1;
				break;
			case 2:
				index = GetVariableIndex("SMASHEDMESSAGE");

				if (MissionSounds.SmashedSound != -1)
					PlayMissionSound(MissionSounds.SmashedSound + 0xF000);

				MissionSounds.SmashedSound = -1;
				break;
			case 3:
				index = GetVariableIndex("OUTOFRANGEMESSAGE");

				if (MissionSounds.OutOfRangeSound != -1)
					PlayMissionSound(MissionSounds.OutOfRangeSound + 0xF000);

				MissionSounds.OutOfRangeSound = -1;
				break;
			case 4:
				index = GetVariableIndex("CARGOTAWAYMESSAGE");

				if (MissionSounds.GotAwaySound != -1)
					PlayMissionSound(MissionSounds.GotAwaySound + 0xF000);

				MissionSounds.GotAwaySound = -1;
				break;
			default:
				index = -1;
				break;
		}

		if (index >= 0 && DmsVars[index].data)
		{
			DrawMissionText((char *)DmsVars[index].data);
			text_drawn = 1;
		}
	}
	else
	{
		text_drawn = DisplayMissionTargetData();
	}

	if (!text_drawn && mission.inform_count)
		DrawMissionText(mission.text);

	DisplayTimeBonus();
}

// [0x4f6d5d] [D]
// The distance (max + min / 2 of the axes) to the nearest active target.
int NearestTargetDistance(void)
{
	MISSION_TARGET *target;
	int nearest;
	int dist;
	int dx;
	int dz;
	int i;

	target = MissionTargets;
	nearest = 0x7FFFFFFF;

	for (i = 0; i < 10; i++, target++)
	{
		if (!(target->flags & 1))
			continue;

		dx = abs(target->position.vx - car_data[0].location.vx);
		dz = abs(target->position.vz - car_data[0].location.vz);

		if (dx >= dz)
			dist = dx + dz / 2;
		else
			dist = dz + dx / 2;

		if (dist < nearest)
			nearest = dist;
	}

	return nearest;
}

// [0x4f6e5c] [D]
// The message of the first active target out of the mission window, with a tail to lose or too
// fast; returns 1 when one is drawn.
int DisplayMissionTargetData(void)
{
	MISSION_TARGET *target;
	int index;
	int i;

	if (gDoTraining || gDoInterview)
		return 0;

	target = MissionTargets;

	for (i = 0; i < 10; i++, target++)
	{
		if (!(target->flags & 1))
			continue;

		if (target->flags & 2)
		{
			index = GetVariableIndex("WINDOWMESSAGE");

			if (MissionSounds.WindowSound != -1)
				PlayMissionSound(MissionSounds.WindowSound + 0xF000);

			MissionSounds.WindowSound = -1;

			if (index >= 0 && DmsVars[index].data)
			{
				DrawMissionText((char *)DmsVars[index].data);
				return 1;
			}
		}

		if ((target->flags & 4) && !(target->flags & 0x400))
		{
			index = GetVariableIndex("LOSETAILMESSAGE");

			target->flags &= ~4;

			if (MissionSounds.LoseTailSound != -1)
				PlayMissionSound(MissionSounds.LoseTailSound + 0xF000);

			MissionSounds.LoseTailSound = -1;

			if (index >= 0 && DmsVars[index].data)
			{
				DrawMissionText((char *)DmsVars[index].data);
				return 1;
			}
		}

		if (target->flags & 0x200)
		{
			index = GetVariableIndex("MAXSPEEDMESSAGE");

			if (index >= 0 && DmsVars[index].data)
			{
				DrawMissionText((char *)DmsVars[index].data);
				return 1;
			}
		}
	}

	return 0;
}

// [0x4f7030] [D]
// Moves the event targets to their event's model.
void UpdateEventTargets(void)
{
	MISSION_TARGET *target;
	EVENT *ev;
	int i;

	if (!mission.loaded)
		return;

	target = MissionTargets;

	for (i = 0; i < 10; i++, target++)
	{
		if (!(target->flags & 1) || !(target->flags & 8))
			continue;

		for (ev = FirstEvent; ev; ev = ev->next)
		{
			if ((ev->flags & 8) && (u_short)ev->id == target->event)
			{
				if (ev->model)
				{
					target->position.vx = float_to_int(ev->model->position.vx);
					target->position.vz = float_to_int(ev->model->position.vz);
				}

				break;
			}
		}
	}
}

// [0x4f7132] [D] [A]
// Fails the mission (out of range) when the player is further than gEventTargetRange[i] from
// event target i.
// NOTE: [A] targets 5 - 9 have no range: the original reads them past the end of
// gEventTargetRange[5] (the globals after it, gNight for target 9)
void HandleEventTargets(void)
{
	MISSION_TARGET *target;
	EVENT *ev;
	int range;
	int dist;
	int dx;
	int dz;
	int i;

	if (!mission.loaded)
		return;

	target = MissionTargets;

	for (i = 0; i < 10; i++, target++)
	{
		range = i < 5 ? gEventTargetRange[i] : 0;

		if (!range || !(target->flags & 1) || !(target->flags & 8))
			continue;

		for (ev = FirstEvent; ev; ev = ev->next)
		{
			if ((ev->flags & 8) && (u_short)ev->id == target->event)
			{
				if (ev->model)
				{
					dx = abs(car_data[0].location.vx - float_to_int(ev->model->position.vx));
					dz = abs(car_data[0].location.vz - float_to_int(ev->model->position.vz));

					if (dx >= dz)
						dist = dx + dz / 2;
					else
						dist = dz + dx / 2;

					if (dist > range)
					{
						mission.objective = 0xFF0;
						mission.failed_reason = 3;
						mission.pause_timer = 90;
					}
				}

				break;
			}
		}
	}
}

// [0x4f72d4] [D]
// The targets, then the objective: 4 scare the opponent, 5 chase the lead car, 6 wreck the target
// cars, 8 wreck the gang car; a target car out of range fails the mission.
void HandleMissionObjective(MISSION *mission)
{
	int i;

	HandleMissionTarget(mission);

	switch (mission->objective)
	{
		case 4:
			if (OpponentBar.active)
			{
				if (OpponentBar.position >= OpponentBar.max)
					mission->objective = 0x7FFF;
			}
			else
			{
				InitPercentageBar(&OpponentBar, (float)scare_target, 0xFF00, "Scare?", 1, 0);
			}

			break;
		case 5:
			if (!OpponentBar.active && gChaseHQMode)
				InitPercentageBar(&OpponentBar, (float)gChaseModeHitsRequired, 0xFF00, "Opponent?", 1, 0);

			if (gChaseHQMode)
				OpponentBar.position = (float)gChaseModeHits;

			if (gChaseHQMode && gChaseModeHits >= gChaseModeHitsRequired)
			{
				mission->objective = 0x7FFF;
				gChaseHQMode = 0;
				LeadCarHit = 1;

				ReplayCarEndPosition.vx = (float)car_data[65].location.vx;
				ReplayCarEndPosition.vy = (float)car_data[65].location.vy;
				ReplayCarEndPosition.vz = (float)car_data[65].location.vz;
			}
			else if (LeadCarLost)
			{
				mission->objective = 0xFF0;
				mission->failed_reason = 3;
				mission->pause_timer = 90;
			}
			else if (LeadCarHit)
			{
				mission->objective = 0x7FFF;
			}
			else if (LeadCarFinished)
			{
				mission->objective = 0x7FFF;

				ReplayCarEndPosition.vx = (float)car_data[65].location.vx;
				ReplayCarEndPosition.vy = (float)car_data[65].location.vy;
				ReplayCarEndPosition.vz = (float)car_data[65].location.vz;
			}

			break;
		case 6:
			for (i = 0; i < 10; i++)
			{
				if ((MissionTargets[i].flags & 1) && (MissionTargets[i].flags & 0x100) &&
					IsMissionCarWrecked(MissionTargets[i].ping_slot, MissionTargets[i].target_damage, &MissionTargets[i].position))
				{
					mission->objective = 0x7FFF;
					MissionTargets[i].flags &= ~0x101;
				}
			}

			break;
		case 8:
			if (mission->gangCarId == -1)
			{
				mission->objective = 0x7FFF;
			}
			else if (civilian_ai_data[mission->gangCarId].isGangCar)
			{
				if (GetCarDamage(mission->gangCarId) < mission->gangCarMaxDamage)
				{
					MissionTargets[0].position.vx = car_data[mission->gangCarId + 1].location.vx;
					MissionTargets[0].position.vz = car_data[mission->gangCarId + 1].location.vz;
				}
				else
				{
					civilian_ai_data[mission->gangCarId].isGangCar = 0;

					mission->gangCarId = -1;
					MissionTargets[0].flags &= ~1;
				}
			}
			else
			{
				MissionTargets[0].flags &= ~1;

				mission->objective = 0xFF0;
				mission->failed_reason = 4;
				mission->pause_timer = 90;
			}

			break;
	}

	if (mission->objective != 0xFF0 && mission->objective != 0x7FFF && mission->objective != 0x8000 &&
		mission->objective != 0xF1 && IsMissionCarLost(mission->targetCarId, mission->targetCarRange))
	{
		mission->targetCarRange = 0;
		mission->objective = 0xFF0;
		mission->failed_reason = 4;
		mission->pause_timer = 90;
	}
}

// [0x4f768c] [D]
// Each active, detected target: the mission window, the tail and speed checks, then whether the
// player reached it.
void HandleMissionTarget(MISSION *mission)
{
	MISSION_TARGET *target;
	int radius;
	int dx;
	int dz;
	int i;

	target = MissionTargets;

	for (i = 0; i < 10; i++, target++)
	{
		if (!(target->flags & 1) || (target->flags & 0x80))
			continue;

		dx = abs(target->position.vx - car_data[0].location.vx);
		dz = abs(target->position.vz - car_data[0].location.vz);

		if (dx >= dz)
			radius = dx + dz / 2;
		else
			radius = dz + dx / 2;

		target->flags &= ~2;

		if (!sub_4F79E6(radius, mission, target) && !sub_4F78F7(radius, mission, target))
			sub_4F77AA(radius, mission, target);
	}
}

// [0x4f77aa] [D]
// NOTE: no name in the database; the target is reached (objective 2 or 7) unless the player still
// has a tail to lose or is too fast: the objective is complete. Past the mission window the player
// only gets felony.
void sub_4F77AA(int radius, MISSION *mission, MISSION_TARGET *target)
{
	int hasATail;
	int window;

	hasATail = 0;

	if (FelonyRating >= 682 && (CopsCanSeePlayer || player_position_known > 0))
		hasATail = 1;

	if (radius >= target->radius || (mission->objective != 2 && mission->objective != 7))
		return;

	if ((gNeedToLoseTail | PlayerMustBeHidden) && hasATail)
		return;

	if (mission->flags & 1)
	{
		window = 50 * mission->window;

		if (mission->timer->count >= window)
		{
			IncreaseFelonyRating(5);
		}
		else if (!mission->maxSpeedEnable || car_data[0].wheel_speed <= mission->max_speed)
		{
			mission->objective = 0x7FFF;
			target->flags &= ~0x207;
		}
	}
	else if (!mission->maxSpeedEnable || car_data[0].wheel_speed <= mission->max_speed)
	{
		mission->objective = 0x7FFF;
		target->flags &= ~1;
	}
}

// [0x4f78f7] [D]
// NOTE: no name in the database; near the target (objective 2) with a tail to lose: the lose
// tail message (flag 4); in the mission window within 6000 of it too fast: the max speed
// message (flag 0x200). Returns 1 when the target is not reached for that.
int sub_4F78F7(int radius, MISSION *mission, MISSION_TARGET *target)
{
	int hasATail;

	if (mission->objective != 2)
		return 0;

	hasATail = 0;

	if (FelonyRating >= 682 && (CopsCanSeePlayer || player_position_known > 0))
		hasATail = 1;

	if (radius < mission->outer_radius && (gNeedToLoseTail | PlayerMustBeHidden) && hasATail)
	{
		target->flags |= 4;
		return 1;
	}

	if (!mission->maxSpeedEnable || !(mission->flags & 1))
		return 0;

	target->flags &= ~0x200;

	if (radius >= 6000)
		return 0;

	if (mission->timer->count < 50 * mission->window && car_data[0].wheel_speed > mission->max_speed)
	{
		target->flags |= 0x200;
		return 1;
	}

	return 0;
}

// [0x4f79e6] [D]
// NOTE: no name in the database; within window_radius of the target once the mission window has
// passed: the window message (flag 2), felony every 4th frame and the cops come. Returns 1 then.
int sub_4F79E6(int radius, MISSION *mission, MISSION_TARGET *target)
{
	int window;

	window = 50 * mission->window;

	if (radius >= mission->window_radius || !(mission->flags & 1) || mission->timer->count < window)
		return 0;

	target->flags |= 2;

	if ((game_frame & 3) == 3)
	{
		AddFelonyPoints(1);

		if (FelonyRating >= 682)
		{
			CopsAllowed = 1;
			max_active_cops = 1;
		}
	}

	return 1;
}

// [0x4f7a71] [D]
// Plays the failure sound once; the cops leave and the player loses control.
void MissionFailed(void)
{
	CopsAllowed = 0;

	switch (mission.failed_reason)
	{
		case 1:
			if (MissionSounds.FailedSound != -1)
			{
				PlayMissionSound(MissionSounds.FailedSound + 0xF000);
				MissionSounds.FailedSound = -1;
			}

			break;
		case 2:
			if (MissionSounds.SmashedSound != -1)
			{
				PlayMissionSound(MissionSounds.SmashedSound + 0xF000);
				MissionSounds.SmashedSound = -1;
			}

			break;
		case 3:
			if (MissionSounds.OutOfRangeSound != -1)
			{
				PlayMissionSound(MissionSounds.OutOfRangeSound + 0xF000);
				MissionSounds.OutOfRangeSound = -1;
			}

			break;
		case 4:
			if (MissionSounds.GotAwaySound != -1)
			{
				PlayMissionSound(MissionSounds.GotAwaySound + 0xF000);
				MissionSounds.GotAwaySound = -1;
			}

			break;
	}

	if (mission.field_C > 50)
		mission.field_C = 50;

	gStopPadReads = 1;
}

// [0x4f7b77] [D]
void MissionCompleted(void)
{
	CopsAllowed = 0;

	mission.field_8++;

	if (mission.objective & 0x8000)
	{
		if (!MissionHasBeenCompleted)
		{
			MissionHasBeenCompleted = 1;

			mission.failed_reason = 0;
			mission.pause_timer = 150;

			gStopPadReads = 1;
		}
	}
	else if (mission.objective == 0xF1)
	{
		// race completed
		if (mission.field_8 <= 50)
			DrawWord(5, mission.field_8);
		else
			mission.field_8 = 51;
	}
	else
	{
		mission.objective = 0x7FFF;
	}
}

// [0x4f7c1e] [D]
// A running timer (flag 4) counts up (flag 0x80) or down while not stopped (flag 8); a countdown
// at 0 fails the mission. TriggerEventAt fires at its count.
void DMSHandleTimer(TIMER *timer)
{
	if (!(timer->flags & 4))
		return;

	if (timer->flags & 0x80)
	{
		if (timer->flags & 8)
		{
			timer->count++;
			timer->ticks++;
		}
	}
	else if (timer->count)
	{
		if (timer->flags & 8)
		{
			timer->count--;
			timer->ticks++;
		}
	}
	else
	{
		if (!gMissionReplaying)
		{
			mission.objective = 0xFF0;
			mission.failed_reason = 1;
			mission.pause_timer = 90;
		}

		timer->flags &= ~8;
	}

	if ((mission.flags & 8) && timer->count == mission.event_timer)
		TriggerEvent(mission.event_id);

	timer->sec = timer->count / 50;
	timer->min = timer->sec / 60;
	timer->frac = timer->count % 50;
	timer->sec %= 60;
	timer->frac *= 2;

	if (timer->min > 59)
	{
		timer->min = 59;
		timer->sec = 59;
	}
}

// [0x4f7d78] [D]
void DMSInitTimerSeconds(TIMER *timer, int seconds)
{
	timer->count = 50 * seconds;
	timer->flags = 12;
}

// [0x4f7d92] [D]
void DMSInitTimerFrames(TIMER *timer, int frames)
{
	timer->count = frames;
	timer->flags = 12;
}

// [0x4f7da9] [D]
// Adds seconds to a running timer and shows "+n seconds!" for a second.
void AddTimeBonus(MISSION *mission, int seconds)
{
	int text;

	if (!seconds)
		return;

	if (seconds == 1)
		text = 298;
	else
		text = 248;

	if (!(mission->timer->flags & 4))
		return;

	bonus_time += seconds;

	if (seconds >= 0)
		sprintf(bonus_text, "+%d %s!", bonus_time, gLanguageStrings[text]);
	else
		sprintf(bonus_text, "%d %s!", bonus_time, gLanguageStrings[text]);

	mission->timer->count += 50 * seconds;

	bonus_count = 50;

	mission->field_64++;
}

// [0x4f7e87] [D]
void HandleTimeBonus(void)
{
	if (bonus_count > 0)
	{
		bonus_count--;
	}
	else
	{
		bonus_count = 0;
		bonus_time = 0;
	}
}

// [0x4f7eb8] [D]
void DisplayTimeBonus(void)
{
	if (bonus_count > 0)
		WriteDropShadowedString(320, 80, bonus_text, 0, 1, 255, 255, 255);
}

// [0x4f7eed] [D]
void ClearDashData(void)
{
	int i;

	for (i = 0; i < nNumDashMessages; i++)
		DebugFree(ppszDashMessages[i]);

	if (ppszDashMessages)
		DebugFree(ppszDashMessages);

	if (pnDashTimes)
		DebugFree(pnDashTimes);

	nNumDashMessages = 0;
	ppszDashMessages = NULL;
	pnDashTimes = NULL;
}

// [0x4f7f75] [D]
// Adds a dash message shown until the timer reaches seconds (ShowDashMessage); returns 0 when out
// of memory.
int AddDashMessage(char *text, int seconds)
{
	char **messages;
	int *times;
	int i;

	messages = (char **)DebugMalloc(4 * nNumDashMessages + 4, "D:\\Driver\\game6.4g\\MScript.c", 1563);

	if (!messages)
	{
		LogQuick("AddDashMessage: Unable to allocate new list for messages.");
		return 0;
	}

	times = (int *)DebugMalloc(4 * nNumDashMessages + 4, "D:\\Driver\\game6.4g\\MScript.c", 1570);

	if (!times)
	{
		LogQuick("AddDashMessage: Unable to allocate new list for dash timers.");
		DebugFree(messages);
		return 0;
	}

	for (i = 0; i < nNumDashMessages; i++)
	{
		times[i] = pnDashTimes[i];
		messages[i] = ppszDashMessages[i];
	}

	times[nNumDashMessages] = seconds;
	messages[nNumDashMessages] = (char *)DebugMalloc(strlen(text) + 1, "D:\\Driver\\game6.4g\\MScript.c", 1587);

	if (!messages[nNumDashMessages])
	{
		LogQuick("AddDashMessage: Unable to allocate memory for new message.");
		DebugFree(messages);
		DebugFree(times);
		return 0;
	}

	strcpy(messages[nNumDashMessages], text);

	if (ppszDashMessages)
		DebugFree(ppszDashMessages);

	if (pnDashTimes)
		DebugFree(pnDashTimes);

	nNumDashMessages++;

	ppszDashMessages = messages;
	pnDashTimes = times;

	return 1;
}

// [0x4f8131] [D]
// The dash message for a timer count (frames); past the last one the mission fails.
char *GetDashMessage(int count)
{
	int seconds;
	int start;
	int i;

	if (!count)
		return NULL;

	start = 0;
	seconds = count / 50;

	for (i = 0; i < nNumDashMessages; i++)
	{
		if (seconds >= start && seconds < pnDashTimes[i])
			return ppszDashMessages[i];

		start = pnDashTimes[i];
	}

	mission.objective = 0xFF0;
	mission.failed_reason = 1;
	mission.pause_timer = 90;

	return NULL;
}

// [0x4f81d7] [D]
// The DEBUGINFO variable: the player's car and the pedestrians.
void DisplayDebugInfo(void)
{
	char text[64];

	if (gDisableDebug || !mission.debugInfo)
		return;

	TextOffset(0, display_height / 8 - 9);

	AddTextToList("-== Debug Information ==-\n");

	sprintf(text, "Player car X position = %d\n", car_data[0].location.vx);
	AddTextToList(text);

	sprintf(text, "Player car Y position = %d\n", car_data[0].location.vy);
	AddTextToList(text);

	sprintf(text, "Player car Z position = %d\n", car_data[0].location.vz);
	AddTextToList(text);

	sprintf(text, "Player car Y rotation = %d, Current damage = %d\n", car_data[0].rotation.vy, gPlayerCarDamage);
	AddTextToList(text);

	sprintf(text, "Player car speed = %d\n", car_data[0].wheel_speed);
	AddTextToList(text);

	sprintf(text, "Number of pedestrians active: %d ( out of %d )\n", pedestrian_count, max_pedestrians);
	AddTextToList(text);

	if (cones_recorded > 0)
	{
		sprintf(text, "Number of cones recorded: %d\n", cones_recorded);
		AddTextToList(text);
	}
}

// [0x4f833d] [D]
// Compiles the commands and assignments of the line in MissionBuffer, up to a comment or the end
// of the line.
int CompileMissionLine(void)
{
	int code;
	size_t len;
	DMSCommand *cmd;

	do
	{
		for (;;)
		{
			do
			{
				code = ReadToken(MissionBuffer, MissionCommand);

				if (code != DMS_SUCCESS)
					return code;

				_strupr(MissionCommand);

				len = strlen(MissionCommand);

				if (MissionCommand[0] == '/' && MissionCommand[1] == '/')
					return DMS_SUCCESS;

				if (MissionCommand[0] == '#')
					return DMS_SUCCESS;

				if (len == 0)
					return DMS_SUCCESS;

				if (MissionCommand[0] == ',')
					return DMS_BAD_COMMA;
			}
			while (MissionCommand[0] == ';');

			GetMissionCommand(MissionCommand, &cmd);

			if (cmd)
				break;

			// not a command: a variable assignment
			code = CompileSETCommand(NULL, NULL);

			if (code != DMS_SUCCESS)
				return code;
		}

		code = CompileMissionCommand(cmd);
	}
	while (code == DMS_COMPILED);

	return code;
}

// [0x4f8453] [D]
int GetMissionCommand(char *line, DMSCommand **cmd)
{
	DMSCommand *c;

	*cmd = NULL;

	for (c = DmsCommands; strlen(c->name) != 0; c++)
	{
		if (strcmp(line, c->name) == 0)
		{
			*cmd = c;
			return DMS_SUCCESS;
		}
	}

	return DMS_BAD_COMMAND;
}

// [0x4f84ad] [D]
// Frees the compiled script, the dash messages, the labels, the string variables and the trail.
void DestroyMissionScript(void)
{
	DMSVariable *var;
	int i;

	if (MissionList)
	{
		for (i = 0; i < NumScriptLines; i++)
		{
			if (MissionList[i])
			{
				DebugFree(MissionList[i]);
				MissionList[i] = NULL;
			}

			MissionList[i] = NULL;
		}

		DebugFree(MissionList);
		MissionList = NULL;
	}

	ClearDashData();
	ClearLabels(&ScriptLabels);

	if (MissionBuffer)
	{
		DebugFree(MissionBuffer);
		MissionBuffer = NULL;
	}

	for (var = DmsVars; var->type != DMS_VAR_END; var++)
	{
		if (var->type == DMS_VAR_STRING)
		{
			if (var->data)
			{
				DebugFree(var->data);
				var->data = NULL;
			}

			var->data = NULL;
		}
	}

	if (pTrailData)
		DebugFree(pTrailData);

	pTrailData = NULL;
	MissionList = NULL;
	MissionBuffer = NULL;
	NumScriptLines = 0;
}

// [0x4f85fd] [D]
int GetVariableIndex(char *name)
{
	DMSVariable *var;
	int index;

	index = 0;

	for (var = DmsVars; var->type != DMS_VAR_END; var++)
	{
		if (strcmp(var->name, name) == 0)
			return index;

		index++;
	}

	return -1;
}

// [0x4f864e] [D]
// Takes the next parameter (up to a comma or white space) off the line.
int ReadParam(char *line, char *data)
{
	int len;
	int n;
	int i;
	int j;

	len = strlen(line);
	n = 0;

	for (i = 0; i < len; i++)
	{
		if ((unsigned char)line[i] >= 0x21 && (unsigned char)line[i] <= 0x7F)
		{
			if (line[i] == ',')
				break;

			data[n++] = line[i];
		}
		else if (n)
		{
			break;
		}
	}

	if (n == 0)
		return DMS_ILLEGAL_PARAM;

	i++;

	j = 0;

	while (i < len)
		line[j++] = line[i++];

	data[n] = 0;

	return DMS_SUCCESS;
}

// [0x4f8732] [D]
// Takes the next "quoted string" off the line.
int ReadString(char *line, char *data)
{
	int start;
	int len;
	int i;
	int n;

	len = strlen(line);
	n = 0;

	for (i = 0; i < len; i++)
	{
		if (line[i] != '"' && line[i] != ' ' && line[i] != '\t')
			return DMS_ILLEGAL_PARAM;

		if (line[i] == '"')
		{
			start = ++i;

			while (i < len)
			{
				if (line[i] == '"')
				{
					for (i = 0; i < n; i++)
						data[i] = line[i + start];

					data[i] = 0;

					// NOTE: strcpy between overlapping parts of the line, as in the binary
					strcpy(line, &line[i + 1 + start]);

					return DMS_SUCCESS;
				}

				if ((unsigned char)line[i] <= 0x1F || (unsigned char)line[i] >= 0x80)
					return DMS_ILLEGAL_PARAM;

				n++;
				i++;
			}
		}
	}

	return DMS_ILLEGAL_PARAM;
}

// [0x4f8894] [D]
// Takes the next token (up to white space, a newline, comma or semicolon) off the line.
int ReadToken(char *line, char *data)
{
	int i;
	int n;
	size_t len;
	char c;
	int j;

	n = 0;
	i = 0;

	len = strlen(line);

	while (len--)
	{
		c = line[i++];

		if ((c == ' ' || c == '\t') && n == 0)
			continue;

		if (c == ' ' || c == '\t' || c == '\n' || c == ',' || c == ';')
			break;

		data[n++] = c;
	}

	data[n] = 0;

	// remove the token from the buffer
	j = 0;

	while (line[i])
		line[j++] = line[i++];

	line[j] = 0;

	if (n)
		return DMS_SUCCESS;

	return DMS_END_OF_LINE;
}

// [0x4f89c1] [D]
// Compiles one command: the level set up commands act now, the others add a MissionList line.
// Returns DMS_COMPILED to go on with the line.
int CompileMissionCommand(DMSCommand *cmd)
{
	int if_var;
	FILE *fp;
	char filename[64];
	int j;
	int code;
	int num;
	int var_idx;
	int size;
	const char *if_type;
	char text[128];
	int *list;
	int i;

	if_type = NULL;

	if (!cmd)
		return DMS_BAD_COMMAND;

	switch (cmd->type)
	{
		case DMS_COM_SET:
			code = ReadToken(MissionBuffer, MissionCommand);

			if (code != DMS_SUCCESS)
				return code;

			_strupr(MissionCommand);

			return CompileSETCommand(cmd, NULL);

		case DMS_COM_INCREASE:
		case DMS_COM_INC:
			code = ReadToken(MissionBuffer, MissionCommand);

			if (code != DMS_SUCCESS)
				return code;

			_strupr(MissionCommand);

			var_idx = GetVariableIndex(MissionCommand);

			if (var_idx == -1 || DmsVars[var_idx].type != DMS_VAR_INT)
				return DMS_ILLEGAL_PARAM;

			AddScriptLine(8);

			list = (int *)MissionList[NumScriptLines - 1];
			*list++ = DMS_COM_INCREASE;
			*list++ = var_idx;

			// NOTE: DMS_SUCCESS (not DMS_COMPILED) ends the line, as in the binary
			return DMS_SUCCESS;

		case DMS_COM_DECREASE:
		case DMS_COM_DEC:
			code = ReadToken(MissionBuffer, MissionCommand);

			if (code != DMS_SUCCESS)
				return code;

			_strupr(MissionCommand);

			var_idx = GetVariableIndex(MissionCommand);

			if (var_idx == -1 || DmsVars[var_idx].type != DMS_VAR_INT)
				return DMS_ILLEGAL_PARAM;

			AddScriptLine(8);

			list = (int *)MissionList[NumScriptLines - 1];
			*list++ = DMS_COM_DECREASE;
			*list++ = var_idx;

			return DMS_SUCCESS;

		case DMS_COM_ADD:
		case DMS_COM_SUB:
			code = ReadToken(MissionBuffer, MissionCommand);

			if (code != DMS_SUCCESS)
				return code;

			_strupr(MissionCommand);

			var_idx = GetVariableIndex(MissionCommand);

			if (var_idx == -1 || DmsVars[var_idx].type != DMS_VAR_INT)
				return DMS_ILLEGAL_PARAM;

			code = ReadToken(MissionBuffer, MissionCommand);

			if (code != DMS_SUCCESS)
				return code;

			num = sscanf(MissionCommand, "%d", &int_params[0]);

			if (num == 0 || num == -1)
				return DMS_ILLEGAL_PARAM;

			AddScriptLine(12);

			list = (int *)MissionList[NumScriptLines - 1];
			*list++ = cmd->type;
			*list++ = var_idx;
			*list++ = int_params[0];

			return DMS_COMPILED;

		case DMS_COM_SETPLAYERPOSITION:
			code = ReadIntParams(cmd);

			if (code != DMS_SUCCESS)
				return code;

			mission.useLastPlayerPosition = 1;
			mission.player_position[0] = int_params[0];
			mission.player_position[1] = -500;
			mission.player_position[2] = int_params[1];

			return DMS_COMPILED;

		case DMS_COM_SPAWNCOPS:
			// SPAWNCOPS n, then x z direction for each of the n (at most 3) cops
			code = ReadToken(MissionBuffer, MissionCommand);

			if (code != DMS_SUCCESS)
				return code;

			_strupr(MissionCommand);

			num = sscanf(MissionCommand, "%d", &int_params[0]);

			if (num == 0 || num == -1)
				return DMS_ILLEGAL_PARAM;

			if (int_params[0] > 3)
				int_params[0] = 3;

			for (i = 0; i < 3 * int_params[0]; i++)
			{
				code = ReadToken(MissionBuffer, MissionCommand);

				if (code != DMS_SUCCESS)
					return code;

				_strupr(MissionCommand);

				num = sscanf(MissionCommand, "%d", &int_params[i + 1]);

				if (num == 0 || num == -1)
					return DMS_ILLEGAL_PARAM;
			}

			size = 12 * int_params[0] + 8;
			AddScriptLine(size);

			list = (int *)MissionList[NumScriptLines - 1];
			*list++ = DMS_COM_SPAWNCOPS;
			*list++ = int_params[0];

			for (i = 0; i < 3 * int_params[0]; i++)
				*list++ = int_params[i + 1];

			return DMS_COMPILED;

		case DMS_COM_INFORMPLAYER:
		case DMS_COM_ADDDASHMESSAGE:
			// seconds "text"
			code = ReadToken(MissionBuffer, MissionCommand);

			if (code != DMS_SUCCESS)
				return code;

			_strupr(MissionCommand);

			num = sscanf(MissionCommand, "%d", &int_params[0]);

			if (num == 0 || num == -1)
				return DMS_ILLEGAL_PARAM;

			code = ReadString(MissionBuffer, MissionCommand);

			if (code != DMS_SUCCESS)
				return code;

			size = strlen(MissionCommand) + 1;
			AddScriptLine(size + 12);

			list = (int *)MissionList[NumScriptLines - 1];
			*list++ = cmd->type;
			*list++ = 50 * int_params[0];
			*list = size;
			CopyMem(list + 1, MissionCommand, size);

			return DMS_COMPILED;

		case DMS_COM_RAINING:
			gDoRain = 1;
			return DMS_COMPILED;

		case DMS_COM_NIGHT:
			gDoLightFlare = 1;
			gNight = 1;
			gDarkNight = 0;
			return DMS_COMPILED;

		case DMS_COM_LIGHTNING:
			gDoLightFlare = 1;
			gNight = 1;
			gDoLightning = 1;
			return DMS_COMPILED;

		case DMS_COM_LOADMAP:
			code = ReadString(MissionBuffer, map_name);

			if (code != DMS_SUCCESS)
			{
				map_name[0] = 0;
				mission.map_name = NULL;
				return code;
			}

			mission.map_name = map_name;
			_strupr(map_name);

			for (i = 0; i < 7; i++)
			{
				if (strcmp(map_name, level_files[i]) == 0)
				{
					GameLevel = i;
					break;
				}
			}

			return DMS_COMPILED;

		case DMS_COM_DUSK:
			gDoDusk = 1;
			gDawnTimer = 0;
			return DMS_COMPILED;

		case DMS_COM_DAWN:
			gDoDawn = 1;
			gNight = 1;
			gDoLightFlare = 1;
			gDawnTimer = 0;
			return DMS_COMPILED;

		case DMS_COM_SETPLAYERYROTATION:
			code = ReadIntParams(cmd);

			if (code != DMS_SUCCESS)
				return code;

			mission.player_rotation = int_params[0];
			return DMS_COMPILED;

		case DMS_COM_RANDOMCHASE:
			// picks one of the chases <name>0, <name>1 ... in turn
			code = ReadString(MissionBuffer, replay_name);

			if (code != DMS_SUCCESS)
			{
				// NOTE: clears map_name, not replay_name, as in the binary
				map_name[0] = 0;
				return code;
			}

			for (j = 0; ; j++)
			{
				sprintf(filename, "replaydata\\Chase\\%s%d.leader", replay_name, j);
				fp = fopen(filename, "rb");

				if (!fp)
					break;

				fclose(fp);
			}

			if (j)
			{
				if (!exit_flag)
				{
					gChaseOverride++;
					gChaseOverride = (unsigned char)gChaseOverride % j;
				}

				sprintf(MissionName, "%s%d", replay_name, (unsigned char)gChaseOverride);

				gMissionReplaying = 1;
				ReplayMode = 3;
			}

			return DMS_COMPILED;

		case DMS_COM_OVERHEADMAP:
			code = ReadString(MissionBuffer, overhead_map);

			if (code != DMS_SUCCESS)
			{
				overhead_map[0] = 0;
				return code;
			}

			return DMS_COMPILED;

		case DMS_COM_CRATES:
			gDoCrates = 1;
			return DMS_COMPILED;

		case DMS_COM_PLAYERCARTYPE:
			code = ReadToken(MissionBuffer, MissionCommand);

			if (code != DMS_SUCCESS)
				return code;

			num = sscanf(MissionCommand, "%d", &int_params[0]);

			if (num == 0 || num == -1)
				return DMS_ILLEGAL_PARAM;

			gPlayerCarModelType = int_params[0];

			// the cheat car replaces the car of a take a drive game
			if (gCheatFlags.PlayerCar && (unsigned int)GameLevel < 4 && global_score.type == GAME_TAKEADRIVE)
				gPlayerCarModelType = gCheatFlags.PlayerCar - 1;

			return DMS_COMPILED;

		case DMS_COM_PLAYERCARDAMAGE:
			code = ReadToken(MissionBuffer, MissionCommand);

			if (code != DMS_SUCCESS)
				return code;

			num = sscanf(MissionCommand, "%d", &int_params[0]);

			if (num == 0 || num == -1)
				return DMS_ILLEGAL_PARAM;

			gPlayerForceDamage = int_params[0];
			return DMS_COMPILED;

		case DMS_COM_SNOWING:
			gDoSnow = 1;
			return DMS_COMPILED;

		case DMS_COM_SETLEADCARMODEL:
			code = ReadToken(MissionBuffer, MissionCommand);

			if (code != DMS_SUCCESS)
				return code;

			num = sscanf(MissionCommand, "%d", &int_params[0]);

			if (num == 0 || num == -1)
				return DMS_ILLEGAL_PARAM;

			ChaseCarType = int_params[0];
			return DMS_COMPILED;

		case DMS_COM_SETLEADCARPALETTE:
			code = ReadToken(MissionBuffer, MissionCommand);

			if (code != DMS_SUCCESS)
				return code;

			num = sscanf(MissionCommand, "%d", &int_params[0]);

			if (num == 0 || num == -1)
				return DMS_ILLEGAL_PARAM;

			ChaseCarPalette = int_params[0] + 4;
			return DMS_COMPILED;

		case DMS_COM_QUICKRENDER:
		case DMS_COM_NOTSOQUICKRENDER:
			// one or two FMV numbers
			code = ReadToken(MissionBuffer, MissionCommand);

			if (code != DMS_SUCCESS)
				return code;

			num = sscanf(MissionCommand, "%d", &int_params[0]);

			if (num == 0 || num == -1)
				return DMS_ILLEGAL_PARAM;

			if (ReadToken(MissionBuffer, MissionCommand) != DMS_SUCCESS)
			{
				int_params[1] = -1;
			}
			else
			{
				num = sscanf(MissionCommand, "%d", &int_params[1]);

				if (num == 0 || num == -1)
					return DMS_ILLEGAL_PARAM;
			}

			if (cmd->type == DMS_COM_QUICKRENDER)
			{
				MissionQuickRender[0] = int_params[0];
				MissionQuickRender[1] = int_params[1];
			}
			else
			{
				MissionNotSoQuickRender[0] = int_params[0];
				MissionNotSoQuickRender[1] = int_params[1];
			}

			return DMS_COMPILED;

		case DMS_COM_USELASTPLAYERPOSITION:
			if (!exit_flag)
			{
				LogQuick("MSCRIPT: Reusing player position (MStart = MNext)");
				MStart = MNext;
			}

			mission.useLastPlayerPosition = 1;
			mission.player_position[0] = float_to_int(MStart.position.vx);
			mission.player_position[1] = -500;
			mission.player_position[2] = float_to_int(MStart.position.vz);

			Reuse_player_data = 1;
			return DMS_COMPILED;

		case DMS_COM_FOGGY:
			do_fogging = 1;
			return DMS_COMPILED;

		case DMS_COM_LOADTRAIL:
		case DMS_COM_LOG:
			// "text"
			code = ReadString(MissionBuffer, MissionCommand);

			if (code != DMS_SUCCESS)
				return code;

			size = strlen(MissionCommand) + 1;
			AddScriptLine(size + 8);

			list = (int *)MissionList[NumScriptLines - 1];
			*list++ = cmd->type;
			*list = size;
			CopyMem(list + 1, MissionCommand, size);

			return DMS_COMPILED;

		case DMS_COM_DARKNIGHT:
			gDoLightFlare = 1;
			gNight = 1;
			gDarkNight = 1;
			return DMS_COMPILED;

		case DMS_COM_LABEL:
			code = ReadToken(MissionBuffer, MissionCommand);

			if (code != DMS_SUCCESS)
				return code;

			// NOTE: an out of memory error is not reported, as in the binary
			AddLabel(MissionCommand, NumScriptLines, &ScriptLabels);
			return DMS_COMPILED;

		case DMS_COM_GOTO:
		case DMS_COM_GOSUB:
			// label
			code = ReadToken(MissionBuffer, MissionCommand);

			if (code != DMS_SUCCESS)
				return code;

			size = strlen(MissionCommand) + 1;
			AddScriptLine(size + 8);

			list = (int *)MissionList[NumScriptLines - 1];
			*list++ = cmd->type;
			*list = size;
			strcpy((char *)(list + 1), MissionCommand);

			return DMS_COMPILED;

		case DMS_COM_IFEQUAL:
			if_type = "IfEqual: ";
			// fall through
		case DMS_COM_IFNOTEQUAL:
			if (!if_type)
				if_type = "IfNotEqual: ";
			// fall through
		case DMS_COM_IFLESSTHAN:
			if (!if_type)
				if_type = "IfLessThan: ";
			// fall through
		case DMS_COM_IFGREATERTHAN:
			if (!if_type)
				if_type = "IfGreaterThan: ";
			// fall through
		case DMS_COM_IFLESSOREQUAL:
			if (!if_type)
				if_type = "IfLessOrEqual: ";
			// fall through
		case DMS_COM_IFGREATEROREQUAL:
			if (!if_type)
				if_type = "IfGreaterOrEqual: ";

			// variable value
			code = ReadToken(MissionBuffer, MissionCommand);

			if (code != DMS_SUCCESS)
				return code;

			_strupr(MissionCommand);

			if_var = GetVariableIndex(MissionCommand);

			if (if_var == -1)
			{
				sprintf(text, "%sVariable not found.", if_type);
				LogQuick(text);
				return DMS_ILLEGAL_PARAM;
			}

			code = ReadToken(MissionBuffer, MissionCommand);

			if (code != DMS_SUCCESS)
				return code;

			_strupr(MissionCommand);

			num = sscanf(MissionCommand, "%d", &int_params[0]);

			if (num == 0 || num == -1)
				return DMS_ILLEGAL_PARAM;

			AddScriptLine(12);

			list = (int *)MissionList[NumScriptLines - 1];
			*list++ = cmd->type;
			*list++ = if_var;
			*list++ = int_params[0];

			return DMS_COMPILED;

		case DMS_COM_RANDOMWEATHER:
			if (!exit_flag)
				DoRandomWeather(GameLevel & 3);

			return DMS_COMPILED;

		case DMS_COM_LOSETAILSOUNDS:
			// n, then n (at most 4) sound ids
			code = ReadToken(MissionBuffer, MissionCommand);

			if (code != DMS_SUCCESS)
				return code;

			_strupr(MissionCommand);

			num = sscanf(MissionCommand, "%d", &int_params[0]);

			if (num == 0 || num == -1)
				return DMS_ILLEGAL_PARAM;

			if (int_params[0] > 4)
				int_params[0] = 4;

			for (i = 0; i < int_params[0]; i++)
			{
				code = ReadToken(MissionBuffer, MissionCommand);

				if (code != DMS_SUCCESS)
					return code;

				_strupr(MissionCommand);

				num = sscanf(MissionCommand, "%d", &int_params[i + 1]);

				if (num == 0 || num == -1)
					return DMS_ILLEGAL_PARAM;
			}

			gNumLoseTailSounds = int_params[0];

			for (i = 0; i < int_params[0]; i++)
				gLoseTailSounds[i] = int_params[i + 1];

			return DMS_COMPILED;

		case DMS_COM_USESTOREDCARTYPE:
			gPlayerCarModelType = gStoredPlayerCarType;
			return DMS_COMPILED;

		case DMS_COM_ENABLEMODEL:
		case DMS_COM_DISABLEMODEL:
			// "model name"
			code = ReadString(MissionBuffer, MissionCommand);

			if (code != DMS_SUCCESS)
				return code;

			size = strlen(MissionCommand) + 1;
			AddScriptLine(size + 8);

			list = (int *)MissionList[NumScriptLines - 1];
			*list++ = cmd->type;
			*list = size;
			CopyMem(list + 1, MissionCommand, size);

			return DMS_COMPILED;

		case DMS_COM_MODELBONUS:
			// bonus "model name"
			code = ReadToken(MissionBuffer, MissionCommand);

			if (code != DMS_SUCCESS)
				return code;

			_strupr(MissionCommand);

			num = sscanf(MissionCommand, "%d", &int_params[0]);

			if (num == 0 || num == -1)
				return DMS_ILLEGAL_PARAM;

			code = ReadString(MissionBuffer, MissionCommand);

			if (code != DMS_SUCCESS)
				return code;

			size = strlen(MissionCommand) + 1;
			AddScriptLine(size + 12);

			list = (int *)MissionList[NumScriptLines - 1];
			*list++ = cmd->type;
			*list++ = int_params[0];
			*list = size;
			CopyMem(list + 1, MissionCommand, size);

			return DMS_COMPILED;

		case DMS_COM_TRAININGMODE:
			code = ReadToken(MissionBuffer, MissionCommand);

			if (code != DMS_SUCCESS)
				return code;

			num = sscanf(MissionCommand, "%d", &int_params[0]);

			if (num == 0 || num == -1)
				return DMS_ILLEGAL_PARAM;

			gTrainingMode = int_params[0];
			return DMS_COMPILED;

		case DMS_COM_TRAINING:
			gDoTraining = 1;
			return DMS_COMPILED;

		default:
			// the command's count int parameters
			code = ReadIntParams(cmd);

			if (code != DMS_SUCCESS)
				return code;

			code = AddScriptLine(cmd->size);

			if (code != DMS_SUCCESS)
				return code;

			list = (int *)MissionList[NumScriptLines - 1];
			*list++ = cmd->type;

			for (i = 0; i < cmd->count; i++)
				*list++ = int_params[i];

			return DMS_COMPILED;
	}
}

// [0x4fa5a5] [D]
// An assignment "VARIABLE value" (cmd and var NULL: MissionCommand names the variable) or
// "SET VARIABLE value". EVENTSCRIPT is set now, the others by a MissionList line.
int CompileSETCommand(DMSCommand *cmd, DMSVariable *var)
{
	int code;
	int index;
	int *list;

	index = 0;

	if (!var)
	{
		for (var = DmsVars; var->type != DMS_VAR_END && strcmp(MissionCommand, var->name) != 0; var++)
			index++;

		if (strlen(var->name) == 0)
			return DMS_BAD_COMMAND;

		cmd = &DmsCommands[DMS_COM_SET];
	}

	if (cmd->type != DMS_COM_SET)
		return DMS_BAD_COMMAND;

	if (var->type == DMS_VAR_STRING)
	{
		code = ReadString(MissionBuffer, MissionCommand);

		if (code != DMS_SUCCESS)
			return code;

		if (index == 0)
		{
			SetStringVar(0, MissionCommand);
			return DMS_SUCCESS;
		}

		AddScriptLine(strlen(MissionCommand) + 9);

		list = (int *)MissionList[NumScriptLines - 1];
		*list++ = DMS_COM_SET;
		*list = index;
		strcpy((char *)(list + 1), MissionCommand);
	}
	else
	{
		code = ReadIntParams(cmd);

		if (code != DMS_SUCCESS)
			return code;

		AddScriptLine(12);

		list = (int *)MissionList[NumScriptLines - 1];
		*list++ = DMS_COM_SET;
		*list = index;
		list[1] = int_params[0];
	}

	return DMS_SUCCESS;
}

// [0x4fa763] [D]
// Adds a line of size bytes to MissionList.
int AddScriptLine(size_t size)
{
	char *line;
	int i;
	char **list;

	list = (char **)DebugMalloc(4 * NumScriptLines + 4, "D:\\Driver\\game6.4g\\MScript.c", 2879);

	if (!list)
		return DMS_OUT_OF_MEMORY;

	line = (char *)DebugMalloc(size, "D:\\Driver\\game6.4g\\MScript.c", 2883);

	if (!line)
	{
		DebugFree(list);
		return DMS_OUT_OF_MEMORY;
	}

	for (i = 0; i < NumScriptLines; i++)
		list[i] = MissionList[i];

	list[i] = line;

	if (MissionList)
	{
		DebugFree(MissionList);
		MissionList = NULL;
	}

	MissionList = list;
	NumScriptLines++;

	return DMS_SUCCESS;
}

// [0x4fa855] [D]
const char *GetScriptErrorString(int code)
{
	switch (code)
	{
		case DMS_SUCCESS:
			return "No errors found";
		case DMS_WORD_VALUE:
			return "Word value has been retrieved";
		case DMS_OUT_OF_MEMORY:
			return "Out of memory";
		case DMS_END_OF_FILE:
			return "End of file reached";
		case DMS_FILE_ERROR:
			return "Specified file was not found";
		case DMS_BAD_COMMA:
			return "Unexpected comma";
		case DMS_BAD_FILE:
			return "Unexpected end of file";
		case DMS_BAD_LINE:
			return "Unexpected end of line";
		case DMS_BAD_COMMAND:
			return "Unrecognised command";
		case DMS_END_OF_LINE:
			return "End of line reached";
		case DMS_COMPILED:
			return "Compilation completed successfully";
		case DMS_ILLEGAL_PARAM:
			return "Illegal parameter";
	}

	return "Unknown error code";
}

// [0x4fa900] [D]
// Reads the command's count int parameters into int_params.
int ReadIntParams(DMSCommand *cmd)
{
	int code;
	int num;
	int i;

	for (i = 0; i < cmd->count; i++)
	{
		code = ReadToken(MissionBuffer, MissionCommand);

		if (code != DMS_SUCCESS)
			return code;

		num = sscanf(MissionCommand, "%d", &int_params[i]);

		if (num == 0 || num == -1)
			return DMS_ILLEGAL_PARAM;
	}

	return DMS_SUCCESS;
}

// [0x4fa982] [D]
// Sets a string variable to a copy of value.
void CopyStringVar(DMSVariable *var, char *value)
{
	char *buffer;

	if (var->type != DMS_VAR_STRING)
		return;

	buffer = (char *)DebugMalloc(strlen(value) + 1, "D:\\Driver\\game6.4g\\MScript.c", 3009);

	if (!buffer)
		return;

	strcpy(buffer, value);

	if (var->data)
	{
		DebugFree(var->data);
		var->data = NULL;
	}

	var->data = buffer;
}

// [0x4faa4a] [D]
// Sets every variable to its default.
void InitMissionVariables(void)
{
	int index;
	DMSVariable *var;

	index = 0;

	for (var = DmsVars; var->type != DMS_VAR_END; var++)
	{
		switch (var->type)
		{
			case DMS_VAR_STRING:
				SetStringVar(index, var->default_str);
				break;
			case DMS_VAR_INT:
				SetIntVar(index, var->default_val);
				break;
			case DMS_VAR_BOOL:
				SetBoolVar(index, var->default_val);
				break;
		}

		index++;
	}
}

// [0x4faad7] [D]
void SetIntVar(int index, int value)
{
	if (DmsVars[index].type != DMS_VAR_STRING)
		*(int *)DmsVars[index].data = value;
}

// [0x4fab0b] [D]
void SetBoolVar(int index, int value)
{
	if (DmsVars[index].type == DMS_VAR_BOOL && (unsigned int)value <= 1)
		*(int *)DmsVars[index].data = value;
}

// [0x4fab4d] [D]
void SetStringVar(int index, char *value)
{
	size_t size;
	void *buffer;
	DMSVariable *var;

	var = &DmsVars[index];

	if (var->type != DMS_VAR_STRING)
		return;

	size = strlen(value) + 1;
	buffer = DebugMalloc(size, "D:\\Driver\\game6.4g\\MScript.c", 3128);

	if (!buffer)
		return;

	if (var->data)
	{
		DebugFree(var->data);
		var->data = NULL;
	}

	CopyMem(buffer, value, size);
	var->data = buffer;
}

// [0x4fabec] [D]
// Plays a sample on channel 13 and 14 in turn.
void PlayMissionSound(int sample)
{
	if (dms_sound_chan)
		SimplePlaySound(sample, 14, -1);
	else
		SimplePlaySound(sample, 13, -1);

	dms_sound_chan = ~dms_sound_chan;
}

// [0x4fac2a] [D]
// Adds a label (upper case) for the MissionList line; returns 0 when out of memory.
int AddLabel(char *name, int line, LABEL_STORE *store)
{
	LABEL_STORE *newStore;
	DMS_LABEL *newLabel;
	int i;

	newStore = (LABEL_STORE *)DebugMalloc(sizeof(LABEL_STORE), "D:\\Driver\\game6.4g\\MScript.c", 3183);

	if (!newStore)
	{
		LogQuick("Error: Unable to add label, out of memory.");
		return 0;
	}

	newStore->count = store->count + 1;
	newStore->labels = (DMS_LABEL **)DebugMalloc(4 * newStore->count, "D:\\Driver\\game6.4g\\MScript.c", 3193);

	if (!newStore->labels)
	{
		LogQuick("Error: Unable to add label, out of memory.");
		DebugFree(newStore);
		return 0;
	}

	newLabel = (DMS_LABEL *)DebugMalloc(sizeof(DMS_LABEL), "D:\\Driver\\game6.4g\\MScript.c", 3201);

	if (!newLabel)
	{
		LogQuick("Error: Unable to add label, out of memory.");
		DebugFree(newStore->labels);
		DebugFree(newStore);
		return 0;
	}

	newLabel->name = (char *)DebugMalloc(strlen(name) + 1, "D:\\Driver\\game6.4g\\MScript.c", 3210);

	if (!newLabel->name)
	{
		LogQuick("Error: Unable to add label, out of memory.");
		DebugFree(newLabel);
		DebugFree(newStore->labels);
		DebugFree(newStore);
		return 0;
	}

	strcpy(newLabel->name, name);
	_strupr(newLabel->name);

	for (i = 0; i < store->count; i++)
		newStore->labels[i] = store->labels[i];

	if (store->labels)
		DebugFree(store->labels);

	newLabel->line = line;
	newStore->labels[store->count] = newLabel;

	store->count = newStore->count;
	store->labels = newStore->labels;

	DebugFree(newStore);

	return 1;
}

// [0x4fae1d] [D]
// The MissionList line of a label, -1 if there is none.
int GetLabel(char *name, LABEL_STORE *store)
{
	int i;

	if (!name)
		return -1;

	_strupr(name);

	for (i = 0; i < store->count; i++)
	{
		if (strcmp(name, store->labels[i]->name) == 0)
			return store->labels[i]->line;
	}

	return -1;
}

// [0x4fae8d] [D]
void ClearLabels(LABEL_STORE *store)
{
	DMS_LABEL **labels;
	int i;

	if (!store->count)
		return;

	labels = store->labels;

	for (i = 0; i < store->count; i++)
	{
		if (labels[i]->name)
			DebugFree(labels[i]->name);

		DebugFree(labels[i]);
	}

	if (labels)
		DebugFree(labels);

	store->count = 0;
	store->labels = NULL;
}

// [0x4faf2c] [D]
// Runs one MissionList line (the command type, then its parameters) and moves execution_line on.
// A line that waits returns DMS_ERROR and stays, which ends the frame's script; so do the
// commands that never set the result (STOPPLAYER, RELEASEPLAYER, TRIGGEREVENTAT, RESETMAP and
// the compile time commands), as in the binary, but those move on.
int ExecuteMissionScript(int *list)
{
	char text[128];
	char filename[32];
	DMSVariable *var;
	int *data;
	char *message;
	int code;
	int num_lines;
	int type;
	int index;
	int num_cops;
	int target_id;
	int lamp_id;
	int civcar_index;
	int civcar_id;
	int line;
	int felony;
	int model;
	int namelen;
	int bonus;
	int car_offset;
	int zoom;

	code = DMS_ERROR;
	num_lines = 1;

	type = *list++;

	switch (type)
	{
		case DMS_COM_CHECKPOINT:
			// Checkpoint <id> <x> <z>
			code = DMS_SUCCESS;
			index = *list++;

			AddCheckPoint(list[0], list[1]);

			mission.checkpoint->flags &= ~0x80;
			mission.checkpoint->model = index;
			mission.checkpoint->radius = 250000;
			break;

		case DMS_COM_MISSIONEND:
			num_lines = 0;
			mission.objective |= 0x8000;
			global_score.complete = 1;
			break;

		case DMS_COM_SET:
			// Set <var_name> <value>
			code = DMS_SUCCESS;
			index = *list++;

			if (DmsVars[index].type == DMS_VAR_STRING)
				SetStringVar(index, (char *)list);
			else
				SetIntVar(index, *list);

			break;

		case DMS_COM_TRIGGEREVENT:
			// TriggerEvent <id>
			code = DMS_SUCCESS;
			TriggerEvent((short)*list);
			break;

		case DMS_COM_PLAYSOUND:
			// PlaySound <sample_id>
			code = DMS_SUCCESS;
			PlayMissionSound(*list);
			break;

		case DMS_COM_RESETCHECKPOINTS:
			code = DMS_SUCCESS;
			memset(mission.checkpoint, 0, sizeof(CHECKPOINT_DATA));
			break;

		case DMS_COM_INITIALISERACE:
			code = DMS_SUCCESS;
			mission.objective = 3;
			mission.checkpoint->flags = 0x80;
			mission.checkpoint->type = 1;
			mission.checkpoint->display_flags = 0x400;
			mission.checkpoint->index = 1;
			break;

		case DMS_COM_WAITFOROBJECTIVECOMPLETE:
			if (mission.objective != 0 && (mission.objective != 1 || (mission.checkpoint->flags & 0x80) == 0))
			{
				if (mission.objective == 0x7FFF)
				{
					mission.objective = 0;
					code = DMS_SUCCESS;
				}
				else
				{
					num_lines = 0;
				}
			}

			break;

		case DMS_COM_TIMEPLAYER:
			code = DMS_SUCCESS;
			mission.timer->count = 0;
			mission.timer->flags = 140;
			break;

		case DMS_COM_COUNTDOWN:
			// Countdown <seconds>
			code = DMS_SUCCESS;
			DMSInitTimerSeconds(mission.timer, *list);
			break;

		case DMS_COM_STOPPLAYER:
			gStopPadReads = 1;
			break;

		case DMS_COM_RELEASEPLAYER:
			gStopPadReads = 0;
			break;

		case DMS_COM_INCREASE:
			// [Inc]rease <var_name>
			code = DMS_SUCCESS;
			(*(int *)DmsVars[*list].data)++;
			break;

		case DMS_COM_DECREASE:
			// [Dec]rease <var_name>
			code = DMS_SUCCESS;
			(*(int *)DmsVars[*list].data)--;
			break;

		case DMS_COM_ADD:
			// Add <var_name> <value>
			code = DMS_SUCCESS;
			*(int *)DmsVars[list[0]].data += list[1];
			break;

		case DMS_COM_SUB:
			// Sub <var_name> <value>
			code = DMS_SUCCESS;
			*(int *)DmsVars[list[0]].data -= list[1];
			break;

		case DMS_COM_CHECKPOINTLOGO:
			code = DMS_SUCCESS;
			mission.checkpoint->display_flags |= 0x100;
			mission.checkpoint->step = 0;
			break;

		case DMS_COM_SPAWNCOPS:
			// SpawnCops <num> [<dir> <x> <z>]...
			code = DMS_SUCCESS;
			num_cops = *list++;

			if (num_cops && !gCheatFlags.Immunity)
			{
				felony = 853 * (num_cops - 1) + 682;

				if (FelonyRating < felony)
					FelonyRating = felony;

				cop_respawn_timer = gCopRespawnTime + 1;
			}

			while (num_cops--)
			{
				InstantCopCar(list[0], list[1], list[2]);
				list += 3;
			}

			break;

		case DMS_COM_PLAYCUTSCENE:
			code = DMS_SUCCESS;
			break;

		case DMS_COM_INFORMPLAYER:
			// InformPlayer <seconds> <message>: frames, length, text
			code = DMS_SUCCESS;
			mission.inform_count = list[0];
			CopyMem(mission.text, list + 2, list[1]);
			break;

		case DMS_COM_RACECOMPLETE:
			code = DMS_SUCCESS;
			mission.field_8 = 0;
			mission.objective = 0x80F1;
			mission.checkpoint->display_flags &= ~4;
			global_score.complete = 1;
			global_score.lap_info = mission.checkpoint->timer;
			break;

		case DMS_COM_WAIT:
			// Wait <seconds>
			if (mission.wait_count)
			{
				if (--mission.wait_count)
					num_lines = 0;
				else
					code = DMS_SUCCESS;
			}
			else
			{
				mission.wait_count = *list;
				mission.wait_count *= 50;
				num_lines = 0;
			}

			break;

		case DMS_COM_SETTARGET:
			// SetTarget <id> <radius> <x> <z>
			code = DMS_SUCCESS;
			target_id = list[0];

			if (target_id < 10)
			{
				MissionTargets[target_id].radius = list[1];
				MissionTargets[target_id].position.vx = list[2];
				MissionTargets[target_id].position.vz = list[3];
				MissionTargets[target_id].offset.vx = 0;
				MissionTargets[target_id].offset.vy = 0;
				MissionTargets[target_id].offset.vz = 0;
			}
			else
			{
				LogQuick("SetTarget: Illegal target index in mission script.");
			}

			break;

		case DMS_COM_ENABLETARGET:
			// EnableTarget <id>
			code = DMS_SUCCESS;
			target_id = *list;

			if (target_id < 10)
				MissionTargets[target_id].flags |= 1;
			else
				LogQuick("EnableTarget: Illegal target index in mission script.");

			break;

		case DMS_COM_DISABLETARGET:
			// DisableTarget <id>
			code = DMS_SUCCESS;
			target_id = *list;

			if (target_id < 10)
				MissionTargets[target_id].flags &= ~1;
			else
				LogQuick("DisableTarget: Illegal target index in mission script.");

			break;

		case DMS_COM_STOPTIMER:
			code = DMS_SUCCESS;

			if (global_score.type == GAME_CHECKPOINT)
				mission.checkpoint->timer.flags &= ~8;
			else
				mission.timer->flags &= ~8;

			break;

		case DMS_COM_CONTINUETIMER:
			code = DMS_SUCCESS;

			if (mission.timer->flags & 4)
				mission.timer->flags |= 8;

			break;

		case DMS_COM_PLAY3DSOUND:
			// Play3DSound <sample_id> <x> <y> <z>
			code = DMS_SUCCESS;
			Play3DSound(list[0], 12, (float)list[1], (float)list[2], (float)list[3]);
			break;

		case DMS_COM_MISSIONWINDOW:
			// MissionWindow <seconds>
			code = DMS_SUCCESS;
			mission.window = *list;
			break;

		case DMS_COM_ENABLEWINDOW:
			code = DMS_SUCCESS;
			mission.flags |= 1;
			break;

		case DMS_COM_DISABLEWINDOW:
			code = DMS_SUCCESS;
			mission.flags &= ~1;
			break;

		case DMS_COM_SETMAXIMUMCOPS:
			// SetMaximumCops <count>
			code = DMS_SUCCESS;
			max_active_cops = *list;
			break;

		case DMS_COM_TARGETEVENT:
			// TargetEvent <id> <event_id>
			code = DMS_SUCCESS;
			target_id = list[0];

			if (target_id < 10)
			{
				if (list[1] == -1)
				{
					MissionTargets[target_id].flags &= ~8;
				}
				else
				{
					MissionTargets[target_id].flags |= 8;
					MissionTargets[target_id].event = list[1];
					MissionTargets[target_id].offset.vx = 0;
					MissionTargets[target_id].offset.vy = 0;
					MissionTargets[target_id].offset.vz = 0;
				}
			}
			else
			{
				LogQuick("TargetEvent: Illegal target index in mission script.");
			}

			break;

		case DMS_COM_LAMPPOSITION:
			// LampPosition <id> <x> <y> <z>
			code = DMS_SUCCESS;
			lamp_id = *list++;

			if (lamp_id < 5)
			{
				MissionLamps[lamp_id].position.vx = (float)list[0];
				MissionLamps[lamp_id].position.vy = (float)list[1];
				MissionLamps[lamp_id].position.vz = (float)list[2];
			}

			break;

		case DMS_COM_LAMPCOLOR:
			// LampColor <id> <r> <g> <b>
			code = DMS_SUCCESS;
			lamp_id = *list++;

			if (lamp_id < 5)
			{
				MissionLamps[lamp_id].color.red = list[0];
				MissionLamps[lamp_id].color.green = list[1];
				MissionLamps[lamp_id].color.blue = list[2];
			}

			break;

		case DMS_COM_LAMPTYPE:
			// LampType <id> <type>
			code = DMS_SUCCESS;
			lamp_id = *list++;

			if (lamp_id < 5)
				MissionLamps[lamp_id].type = *list;

			break;

		case DMS_COM_LAMPRANGE:
			// LampRange <id> <range>
			code = DMS_SUCCESS;
			lamp_id = *list++;

			if (lamp_id < 5)
				MissionLamps[lamp_id].range = (float)*list;

			break;

		case DMS_COM_ENABLELAMP:
			// EnableLamp <id>
			code = DMS_SUCCESS;
			lamp_id = *list;

			if (lamp_id < 5)
				MissionLampsStatus[lamp_id] = 1;

			break;

		case DMS_COM_DISABLELAMP:
			// DisableLamp <id>
			code = DMS_SUCCESS;
			lamp_id = *list;

			if (lamp_id < 5)
				MissionLampsStatus[lamp_id] = 0;

			break;

		case DMS_COM_NOLENSFLARE:
			code = DMS_SUCCESS;
			gDoLensFlare = 0;
			break;

		case DMS_COM_COPS_OFF:
			code = DMS_SUCCESS;
			CopsAllowed = 0;
			break;

		case DMS_COM_COPS_ON:
			code = DMS_SUCCESS;
			CopsAllowed = 1;
			break;

		case DMS_COM_SETFAILEDSOUND:
			// SetFailedSound <sample_id>
			code = DMS_SUCCESS;
			MissionSounds.FailedSound = *list;
			break;

		case DMS_COM_SETPLAYERYROTATION:
			// SetPlayerYRotation <value>
			code = DMS_SUCCESS;
			car_data[0].rotation.vy = *list & 0xFFF;
			break;

		case DMS_COM_FORCEFELONYRATING:
			// ForceFelonyRating <value>: the felony of that many cops
			if (*list && !gCheatFlags.Immunity)
			{
				felony = 853 * (*list - 1) + 682;

				if (FelonyRating < felony)
					FelonyRating = felony;

				cop_respawn_timer = gCopRespawnTime + 1;
			}

			code = DMS_SUCCESS;
			break;

		case DMS_COM_ADDCIVCAR:
			// AddCivCar <target_id> <civ_id> <model> <palette> <dir> <x> <z>
			civcar_index = list[0];
			civcar_id = list[1];
			code = DMS_SUCCESS;

			if (civcar_index < 10)
			{
				InstantCivilianCar(civcar_id, list[2], list[3], list[4], list[5], list[6]);

				MissionTargets[civcar_index].target_damage = mission.target_damage;
				MissionTargets[civcar_index].ping_slot = civcar_id;
				MissionTargets[civcar_index].flags = 0x100;
			}
			else
			{
				LogQuick("Script Error: AddCivCar - Illegal index value supplied.\n");
			}

			break;

		case DMS_COM_ENABLEPLAYERDAMAGE:
			code = DMS_SUCCESS;
			mission.flags |= 4;
			break;

		case DMS_COM_DISABLEPLAYERDAMAGE:
			code = DMS_SUCCESS;
			mission.flags &= ~4;
			break;

		case DMS_COM_TRIGGEREVENTAT:
			// TriggerEventAt <seconds> <event_id>
			mission.flags |= 8;
			mission.event_timer = 50 * list[0];
			mission.event_id = list[1];
			break;

		case DMS_COM_KILLTIMER:
			code = DMS_SUCCESS;
			mission.timer->flags = 0;
			break;

		case DMS_COM_ADDTOTIMER:
			// AddToTimer <seconds>
			code = DMS_SUCCESS;
			mission.timer->count += 50 * *list;
			break;

		case DMS_COM_SUBFROMTIMER:
			// SubFromTimer <seconds>
			code = DMS_SUCCESS;
			mission.timer->count -= 50 * *list;
			break;

		case DMS_COM_MCS_SETSTARTCOORD:
			// MCS_SetStartCoord <x> <y> <z>
			code = DMS_SUCCESS;
			MCS_StartPosition.vx = (float)list[0];
			MCS_StartPosition.vy = (float)list[1];
			MCS_StartPosition.vz = (float)list[2];
			break;

		case DMS_COM_MCS_SETDOORAXIS:
			// MCS_SetDoorAxis <x=0,z=1>
			code = DMS_SUCCESS;
			MCS_Axis = *list;
			break;

		case DMS_COM_MCS_SETNUMBADGUYS:
			// MCS_SetNumBadGuys <count>
			code = DMS_SUCCESS;
			MCS_NumBadGuys = *list;
			break;

		case DMS_COM_MCS_SETRUN:
			code = DMS_SUCCESS;
			MCS_Run = 1;
			break;

		case DMS_COM_MCS_SETWALK:
			code = DMS_SUCCESS;
			MCS_Run = 0;
			break;

		case DMS_COM_MCS_ACTIVATE:
			code = DMS_SUCCESS;
			StartCutScene = 1;
			break;

		case DMS_COM_MCS_WAITFORSCENE:
			if (CutSceneActive || StartCutScene)
				num_lines = 0;
			else
				code = DMS_SUCCESS;

			break;

		case DMS_COM_MCS_SETSTARTHEIGHT:
			// MCS_SetStartHeight <value>
			code = DMS_SUCCESS;
			CAMERA_START_ELEVATION = *list;
			CAMERA_DELTA = -200 - CAMERA_START_ELEVATION;
			break;

		case DMS_COM_MCS_DROPOFF:
			// MCS_DropOff <to_car=0,from_car=1,to_end=2>
			code = DMS_SUCCESS;
			MCS_DropOff = *list;
			break;

		case DMS_COM_ADDCIVCARATREPLAY:
			// AddCivCarAtReplay <target_id> <civ_id> <model> <palette> <dir>: where the lead car
			// ended
			civcar_index = list[0];
			civcar_id = list[1];
			code = DMS_SUCCESS;

			InstantCivilianCar(civcar_id, list[2], list[3], list[4],
				float_to_int(ReplayCarEndPosition.vx), float_to_int(ReplayCarEndPosition.vz));

			// NOTE: no check of the target index, as in the binary
			MissionTargets[civcar_index].target_damage = mission.target_damage;
			MissionTargets[civcar_index].ping_slot = civcar_id;
			MissionTargets[civcar_index].flags = 0x100;
			break;

		case DMS_COM_MCS_USEREPLAYCARPOSITION:
			code = DMS_SUCCESS;
			MCS_EndPosition.vx = ReplayCarEndPosition.vx + ReplayCarOffset.vx;
			MCS_StartPosition.vx = MCS_EndPosition.vx;
			MCS_EndPosition.vy = ReplayCarEndPosition.vy;
			MCS_StartPosition.vy = ReplayCarEndPosition.vy;
			MCS_EndPosition.vz = ReplayCarEndPosition.vz + ReplayCarOffset.vz;
			MCS_StartPosition.vz = MCS_EndPosition.vz;
			break;

		case DMS_COM_MCS_REPLAYCAROFFSET:
			// MCS_ReplayCarOffset <x> <z>
			code = DMS_SUCCESS;
			ReplayCarOffset.vx = (float)list[0];
			ReplayCarOffset.vz = (float)list[1];
			break;

		case DMS_COM_ENABLETARGETPULSE:
			// EnableTargetPulse <target_id>
			code = DMS_SUCCESS;
			target_id = *list;

			if (target_id < 10)
				MissionTargets[target_id].flags &= ~0x10;
			else
				LogQuick("EnablePulse: Illegal target index in mission script.");

			break;

		case DMS_COM_DISABLETARGETPULSE:
			// DisableTargetPulse <target_id>
			code = DMS_SUCCESS;
			target_id = *list;

			if (target_id < 10)
				MissionTargets[target_id].flags |= 0x10;
			else
				LogQuick("DisablePulse: Illegal target index in mission script.");

			break;

		case DMS_COM_ENABLETARGETARROW:
			// EnableTargetArrow <target_id>
			code = DMS_SUCCESS;
			target_id = *list;

			if (target_id < 10)
				MissionTargets[target_id].flags &= ~0x20;
			else
				LogQuick("EnableArrow: Illegal target index in mission script.");

			break;

		case DMS_COM_DISABLETARGETARROW:
			// DisableTargetArrow <target_id>
			code = DMS_SUCCESS;
			target_id = *list;

			if (target_id < 10)
				MissionTargets[target_id].flags |= 0x20;
			else
				LogQuick("DisableArrow: Illegal target index in mission script.");

			break;

		case DMS_COM_ENABLETARGETTRACER:
			// EnableTargetTracer <target_id>
			code = DMS_SUCCESS;
			target_id = *list;

			if (target_id < 10)
				MissionTargets[target_id].flags &= ~0x40;
			else
				LogQuick("EnableTrace: Illegal target index in mission script.");

			break;

		case DMS_COM_DISABLETARGETTRACER:
			// DisableTargetTracer <target_id>
			code = DMS_SUCCESS;
			target_id = *list;

			if (target_id < 10)
				MissionTargets[target_id].flags |= 0x40;
			else
				LogQuick("DisableTrace: Illegal target index in mission script.");

			break;

		case DMS_COM_ENABLEDETECTION:
			// EnableDetection <target_id>
			code = DMS_SUCCESS;
			target_id = *list;

			if (target_id < 10)
				MissionTargets[target_id].flags &= ~0x80;
			else
				LogQuick("EnableDetection: Illegal target index in mission script.");

			break;

		case DMS_COM_DISABLEDETECTION:
			// DisableDetection <target_id>
			code = DMS_SUCCESS;
			target_id = *list;

			if (target_id < 10)
				MissionTargets[target_id].flags |= 0x80;
			else
				LogQuick("DisableDetection: Illegal target index in mission script.");

			break;

		case DMS_COM_TARGETARROW:
			// TargetArrow <target_id> <offset_x> <offset_y> <offset_z>
			code = DMS_SUCCESS;
			target_id = *list++;

			if (target_id < 10)
			{
				MissionTargets[target_id].offset.vx = list[0];
				MissionTargets[target_id].offset.vy = list[1];
				MissionTargets[target_id].offset.vz = list[2];
			}
			else
			{
				LogQuick("TargetArrow: Illegal target index in mission script.");
			}

			break;

		case DMS_COM_LOADTRAIL:
			// LoadTrail <filename>: length, text
			code = DMS_SUCCESS;
			LoadTrailData((char *)(list + 1));
			break;

		case DMS_COM_KILLEVENT:
			// KillEvent <id>
			code = DMS_SUCCESS;
			KillEvent(*list);
			break;

		case DMS_COM_CHOPPER:
			// Chopper <x> <y> <z> <dir>
			chopper.position.vx = (float)list[0];
			chopper.start_ypos = list[1];
			chopper.position.vy = (float)chopper.start_ypos;
			chopper.position.vz = (float)list[2];
			chopper.start_yang = list[3];
			chopper.angle.vy = chopper.start_yang;
			chopper.start = 0x7FFFFFFF;

			gDoChopper = 1;
			code = DMS_SUCCESS;
			break;

		case DMS_COM_CHOPPER_FORCE:
			ActivateChopper = 1;
			code = DMS_SUCCESS;
			break;

		case DMS_COM_CHOPPER_START:
			// Chopper_Start <seconds>
			chopper.start = *list;
			code = DMS_SUCCESS;
			break;

		case DMS_COM_DISABLETAILMESSAGE:
			// DisableTailMessage <target_id>
			code = DMS_SUCCESS;
			target_id = *list;

			if (target_id < 10)
				MissionTargets[target_id].flags |= 0x400;
			else
				LogQuick("ToggleTailMessage: Illegal target index in mission script.");

			break;

		case DMS_COM_ENABLETAILMESSAGE:
			// EnableTailMessage <target_id>
			code = DMS_SUCCESS;
			target_id = *list;

			if (target_id < 10)
				MissionTargets[target_id].flags &= ~0x400;
			else
				LogQuick("ToggleTailMessage: Illegal target index in mission script.");

			break;

		case DMS_COM_ADDDASHMESSAGE:
			// AddDashMessage <seconds> <message>: frames, length, text
			code = DMS_SUCCESS;
			AddDashMessage((char *)(list + 2), list[0]);
			break;

		case DMS_COM_SHOWDASHMESSAGE:
			// ShowDashMessage <duration>
			code = DMS_SUCCESS;

			if (mission.timer->flags & 4)
			{
				message = GetDashMessage(mission.timer->count);

				if (message)
				{
					// NOTE: the duration is the timer's count, not the parameter, and the text is
					// copied without its terminator, as in the binary
					mission.inform_count = 50 * mission.timer->count;
					CopyMem(mission.text, message, strlen(message));
				}
			}

			break;

		case DMS_COM_CLEARDASHDATA:
			code = DMS_SUCCESS;
			ClearDashData();
			break;

		case DMS_COM_GOTO:
			// GoTo <label_name>: length, text
			line = GetLabel((char *)(list + 1), &ScriptLabels);

			if (line == -1)
			{
				LogQuick("Script error in GOTO: Unknown label.");
				break;
			}

			execution_line = line;
			return DMS_SUCCESS;

		case DMS_COM_IFEQUAL:
		case DMS_COM_IFNOTEQUAL:
		case DMS_COM_IFLESSTHAN:
		case DMS_COM_IFGREATERTHAN:
		case DMS_COM_IFLESSOREQUAL:
		case DMS_COM_IFGREATEROREQUAL:
			// If... <var_name> <value>: skips the next line when false
			code = DMS_SUCCESS;
			var = &DmsVars[*list++];
			data = (int *)var->data;

			switch (var->type)
			{
				case DMS_VAR_STRING:
					sprintf(text, "%sType mismatch, cannot convert string variable for integer comparison.", IfName(type));
					LogQuick(text);
					break;
				case DMS_VAR_FLOAT:
					sprintf(text, "%sType mismatch, cannot convert floating point variable for integer comparison.", IfName(type));
					LogQuick(text);
					break;
				case DMS_VAR_BOOL:
					if ((unsigned int)*list >= 2)
					{
						sprintf(text, "%sType mismatch, cannot convert boolean variable for integer comparison.", IfName(type));
						LogQuick(text);
						break;
					}

					// fall through
				case DMS_VAR_INT:
					if (!IfTrue(type, *data, *list))
						num_lines = 2;

					break;
			}

			break;

		case DMS_COM_GOSUB:
			// GoSub <label_name>: length, text
			code = DMS_SUCCESS;

			if (StackPosition >= 100)
			{
				LogQuick("Script error, GoSub: Out of Stack space.");
				break;
			}

			line = GetLabel((char *)(list + 1), &ScriptLabels);

			if (line == -1)
			{
				LogQuick("Script error, GoSub: Unknown label.");
				break;
			}

			CallStack[StackPosition++] = execution_line + 1;
			execution_line = line;
			return DMS_SUCCESS;

		case DMS_COM_RETURN:
			code = DMS_SUCCESS;

			if (StackPosition <= 0)
			{
				LogQuick("Script error, Return without GoSub!");
				break;
			}

			execution_line = CallStack[--StackPosition];
			return code;

		case DMS_COM_LOG:
			// Log <message>: length, text
			code = DMS_SUCCESS;
			LogQuick((char *)(list + 1));
			break;

		case DMS_COM_UPDATEBESTLAP:
			// NOTE: DMS_COMPILED (not DMS_SUCCESS), as in the binary; it goes on all the same
			code = DMS_COMPILED;
			mission.best_lap = UpdateBestLap();
			break;

		case DMS_COM_LOADSOUNDBANK:
			// LoadSoundBank <bank_id>
			code = DMS_COMPILED;
			sprintf(filename, "BANK%d.DSS", *list);
			LoadSounds(filename, 0xF000);
			break;

		case DMS_COM_PLAYBANKEDSOUND:
			// PlayBankedSound <sample_id>
			code = DMS_COMPILED;
			PlaySpeech(*list + 0xF000);
			break;

		case DMS_COM_WAITUNTIL:
			// WaitUntil <seconds>: the timer counting up past it or down to it
			if (mission.wait_count == 0 || mission.wait_count == mission.timer->count)
			{
				mission.wait_count = 50 * *list;

				if (((mission.timer->flags & 0x80) && mission.timer->count >= mission.wait_count) ||
					((mission.timer->flags & 8) && mission.timer->count <= mission.wait_count))
				{
					mission.wait_count = 0;
					code = DMS_SUCCESS;
				}
				else
				{
					num_lines = 0;
				}
			}
			else
			{
				num_lines = 0;
			}

			break;

		case DMS_COM_MCS_SETENDCOORD:
			// MCS_SetEndCoord <x> <y> <z>
			code = DMS_SUCCESS;
			MCS_EndPosition.vx = (float)list[0];
			MCS_EndPosition.vy = (float)list[1];
			MCS_EndPosition.vz = (float)list[2];
			break;

		case DMS_COM_MCS_SETDOORCOORD:
			// MCS_SetDoorCoord <x> <y> <z>
			code = DMS_SUCCESS;
			MCS_EndPosition.vx = (float)list[0];
			MCS_StartPosition.vx = MCS_EndPosition.vx;
			MCS_EndPosition.vy = (float)list[1];
			MCS_StartPosition.vy = MCS_EndPosition.vy;
			MCS_EndPosition.vz = (float)list[2];
			MCS_StartPosition.vz = MCS_EndPosition.vz;
			break;

		case DMS_COM_CREATEGANGCAR:
			// CreateGangCar <max_damage>: waits for a car to become the gang car
			index = SelectGangCar();

			if (index == -1)
			{
				num_lines = 0;
			}
			else
			{
				code = DMS_SUCCESS;
				mission.gangCarId = index;
				mission.gangCarMaxDamage = *list;
			}

			break;

		case DMS_COM_INFORMPLAYERBYID:
			// InformPlayerById <seconds> <message_id>
			if (list[1] != -1)
			{
				code = DMS_SUCCESS;
				mission.inform_count = 50 * list[0];
				strcpy(mission.text, gMissionStrings[list[1]]);
			}

			break;

		case DMS_COM_ADDDASHMESSAGEBYID:
			// AddDashMessageById <seconds> <message_id>
			code = DMS_SUCCESS;

			if (list[1] != -1)
				AddDashMessage(gMissionStrings[list[1]], list[0]);

			break;

		case DMS_COM_FAILEDMESSAGEID:
			// FailedMessageId <message_id>
			code = SetMessageVar("FAILEDMESSAGE", *list);
			break;

		case DMS_COM_WINDOWMESSAGEID:
			// WindowMessageId <message_id>
			code = SetMessageVar("WINDOWMESSAGE", *list);
			break;

		case DMS_COM_LOSETAILMESSAGEID:
			// LoseTailMessageId <message_id>
			code = SetMessageVar("LOSETAILMESSAGE", *list);
			break;

		case DMS_COM_SMASHEDMESSAGEID:
			// SmashedMessageId <message_id>
			code = SetMessageVar("SMASHEDMESSAGE", *list);
			break;

		case DMS_COM_OUTOFRANGEMESSAGEID:
			// OutOfRangeMessageId <message_id>
			code = SetMessageVar("OUTOFRANGEMESSAGE", *list);
			break;

		case DMS_COM_CARGOTAWAYMESSAGEID:
			// CarGotAwayMessageId <message_id>
			code = SetMessageVar("CARGOTAWAYMESSAGE", *list);
			break;

		case DMS_COM_OPPONENTBARID:
			// OpponentBarId <message_id>
			code = SetMessageVar("OPPONENTBAR", *list);
			break;

		case DMS_COM_DAMAGEBARID:
			// DamageBarId <message_id>
			code = SetMessageVar("DAMAGEBAR", *list);
			break;

		case DMS_COM_FELONYBARID:
			// FelonyBarId <message_id>
			code = SetMessageVar("FELONYBAR", *list);
			break;

		case DMS_COM_ENABLEMODEL:
			// EnableModel <name>: length, text
			code = DMS_SUCCESS;
			namelen = *list++;
			model = FindModelWithName((char *)list);

			if (model == -1)
			{
				if (namelen >= 90)
					sprintf(text, "EnableModel: Unknown model name provided, function failed.");
				else
					sprintf(text, "EnableModel: Unknown model name provided (%s), function failed.", (char *)list);

				LogQuick(text);
			}
			else
			{
				EnableModel(model, 1);
			}

			break;

		case DMS_COM_DISABLEMODEL:
			// DisableModel <name>: length, text
			code = DMS_SUCCESS;
			namelen = *list++;
			model = FindModelWithName((char *)list);

			if (model == -1)
			{
				if (namelen >= 90)
					sprintf(text, "DisableModel: Unknown model name provided, function failed.");
				else
					sprintf(text, "DisableModel: Unknown model name provided (%s), function failed.", (char *)list);

				LogQuick(text);
			}
			else
			{
				EnableModel(model, 0);
			}

			break;

		case DMS_COM_STARTGHOSTLAP:
			code = DMS_SUCCESS;
			gStartGhostLap = 1;
			break;

		case DMS_COM_ENDGHOSTLAP:
			code = DMS_SUCCESS;
			gEndGhostLap = 1;
			break;

		case DMS_COM_MODELBONUS:
			// ModelBonus <bonus_seconds> <model_name>: seconds, length, text
			code = DMS_SUCCESS;
			bonus = *list++;
			model = FindModelWithName((char *)(list + 1));

			if (model == -1)
				LogQuick("ModelBonus: Error: Could not find specified model!");
			else
				ModelTimeBonus[model] = bonus;

			break;

		case DMS_COM_RESETMAP:
			ResetMap();
			break;

		case DMS_COM_MAXSPEEDMESSAGEID:
			// MaxSpeedMessageId <message_id>
			code = SetMessageVar("MAXSPEEDMESSAGE", *list);
			break;

		case DMS_COM_MODELTARGETRANGE:
			// ModelTargetRange <civ_id> <max_radius>
			code = DMS_SUCCESS;
			mission.targetCarId = list[0];
			mission.targetCarRange = list[1];
			break;

		case DMS_COM_MCS_SETCAMERASTART:
			// MCS_SetCameraStart <world=0,car_offset=1> <x> <y> <z> <zoom>
			car_offset = list[0];
			zoom = list[4];
			code = DMS_SUCCESS;

			MCS_CameraStart.vx = (float)list[1];
			MCS_CameraStart.vy = -(float)list[2];
			MCS_CameraStart.vz = (float)list[3];

			if (car_offset == 1)
			{
				MCS_CameraStart.vx += (float)car_data[0].location.vx;
				MCS_CameraStart.vy += (float)car_data[0].location.vy;
				MCS_CameraStart.vz += (float)car_data[0].location.vz;
			}

			if (zoom < 50 || zoom > 200)
				MCS_ZoomStart = 386.0f;
			else
				MCS_ZoomStart = 3.86f * (float)zoom;

			MCS_CameraPositionsSet++;
			break;

		case DMS_COM_MCS_SETCAMERAEND:
			// MCS_SetCameraEnd <world=0,car_offset=1> <x> <y> <z> <zoom>
			car_offset = list[0];
			zoom = list[4];
			code = DMS_SUCCESS;

			MCS_CameraEnd.vx = (float)list[1];
			MCS_CameraEnd.vy = -(float)list[2];
			MCS_CameraEnd.vz = (float)list[3];

			if (car_offset == 1)
			{
				MCS_CameraEnd.vx += (float)car_data[0].location.vx;
				MCS_CameraEnd.vy += (float)car_data[0].location.vy;
				MCS_CameraEnd.vz += (float)car_data[0].location.vz;
			}

			if (zoom < 50 || zoom > 200)
				MCS_ZoomEnd = 386.0f;
			else
				MCS_ZoomEnd = 3.86f * (float)zoom;

			MCS_CameraPositionsSet++;
			break;

		case DMS_COM_CUTTOCAR:
			code = DMS_SUCCESS;
			CutToCar = 1;
			break;

		case DMS_COM_DRAW321GO:
			code = DMS_SUCCESS;
			text3d[1] = 1;
			text3d[0] = 0;
			mission.objective = 3;
			break;

		case DMS_COM_DRAWFINISHED:
			code = DMS_SUCCESS;
			text3d[1] = 1;
			text3d[0] = 5;
			break;
	}

	execution_line += num_lines;

	return code;
}

// [A]
// NOTE: not in the original: the ...MessageId and ...BarId commands. Sets the string variable to
// mission string id and returns DMS_SUCCESS; id -1 leaves it and returns DMS_ERROR.
static int SetMessageVar(char *name, int id)
{
	int index;

	if (id == -1)
		return DMS_ERROR;

	index = GetVariableIndex(name);

	if (index != -1)
		CopyStringVar(&DmsVars[index], gMissionStrings[id]);

	return DMS_SUCCESS;
}

// [A]
// NOTE: not in the original: the start of the If... commands' messages.
static const char *IfName(int type)
{
	switch (type)
	{
		case DMS_COM_IFEQUAL:
			return "IfEqual: ";
		case DMS_COM_IFNOTEQUAL:
			return "IfNotEqual: ";
		case DMS_COM_IFLESSTHAN:
			return "IfLessThan: ";
		case DMS_COM_IFGREATERTHAN:
			return "IfGreaterThan: ";
		case DMS_COM_IFLESSOREQUAL:
			return "IfLessOrEqual: ";
		default:
			return "IfGreaterOrEqual: ";
	}
}

// [A]
// NOTE: not in the original: the comparison of an If... command.
static int IfTrue(int type, int value, int param)
{
	switch (type)
	{
		case DMS_COM_IFEQUAL:
			return value == param;
		case DMS_COM_IFNOTEQUAL:
			return value != param;
		case DMS_COM_IFLESSTHAN:
			return value < param;
		case DMS_COM_IFGREATERTHAN:
			return value > param;
		case DMS_COM_IFLESSOREQUAL:
			return value <= param;
		default:
			return value >= param;
	}
}

// [0x4fdb43] [D]
// mm:ss and the hundredths smaller after them: bottom left with the rear view mirror, else top
// middle.
void DisplayTimer(TIMER *timer)
{
	char string[20];
	int x;

	if (Director.Mode)
		return;

	if (gDoRearViewMirror)
	{
		sprintf(string, "%02d:%02d", timer->min, timer->sec);
		WriteDropShadowedString(10, 424, string, 1, 0, 255, 255, 255);

		x = float_to_int(PrintDigit(string, 1)) + 10;

		sprintf(string, ".%02d", timer->frac);
		WriteDropShadowedString(x, 426, string, 0, 0, 255, 255, 255);
	}
	else
	{
		sprintf(string, "%02d:%02d", timer->min, timer->sec);
		WriteDropShadowedString(320, 2, string, 1, 1, 255, 255, 255);

		x = (float_to_int(PrintDigit(string, 1)) >> 1) + 320;

		sprintf(string, ".%02d", timer->frac);
		WriteDropShadowedString(x, 4, string, 0, 0, 255, 255, 255);
	}
}

// [0x4fdce5] [D]
void DrawMissionText(char *text)
{
	if (!GamePaused)
		WriteDropShadowedString(320, 240, text, 0, 1, 255, 255, 255);
}

// [0x4fdd1c] [D]
// Reads a trail blazer course: x y z of each cone, then an end marker (4 bytes).
int LoadTrailData(char *filename)
{
	TRAIL_DATA *trail;
	FILE *fp;
	char text[128];
	int count;
	int i;

	sprintf(text, "Attempting to read Trailblazer data file: \"%s\"", filename);
	LogQuick(text);

	if (pTrailData)
	{
		LogQuick("Trail data already exists in memory, removing.");

		DebugFree(pTrailData);
		pTrailData = NULL;

		mission.iTrailCount = 0;
		mission.iTrailIndex = 0;
	}

	fp = fopen(filename, "rb");

	if (!fp)
	{
		LogQuick("Failed to acces Trailblazer data file.\n");
		return 0;
	}

	count = (unsigned int)(FileSize(fp) - 4) / 12;
	mission.iTrailCount = count;

	trail = (TRAIL_DATA *)DebugMalloc(sizeof(TRAIL_DATA) * count, "D:\\Driver\\game6.4g\\MScript.c", 5169);

	if (!trail)
	{
		LogQuick("Unable to allocate memory for trail data.  Function failed.");
		fclose(fp);
		return 0;
	}

	memset(trail, 0, sizeof(TRAIL_DATA) * count);
	pTrailData = trail;

	for (i = 0; i < count; i++, trail++)
	{
		fread(&trail->x, 1, 4, fp);
		fread(&trail->y, 1, 4, fp);
		fread(&trail->z, 1, 4, fp);
		trail->cone = i;
	}

	next_cone = 0;

	LogQuick("Trail data has loaded successfully.\n");

	fclose(fp);

	global_score.lap_info.count = 0;
	global_score.lap_info.frac = 0;
	global_score.lap_info.sec = 0;
	global_score.lap_info.min = 0;

	for (next_flying_cone = 3; next_flying_cone >= 0; next_flying_cone--)
		cone_queue[next_flying_cone].yang = 0;

	next_flying_cone = 0;

	return 1;
}

// [0x4fdfb6] [D]
void InitSaveTrail(void)
{
	TrailFile = fopen("TRAIL.DAT", "wb");
}

// [0x4fdfd2] [D]
// Records a trail: F5 starts, F6 stops; a cone every TrailSpacing frames, at most 100.
void ControlSaveTrail(void)
{
	IVECTOR pos;
	int y;

	if (KeyMap[DIK_F5])
		TrailStarted = 1;

	if (KeyMap[DIK_F6])
		TrailStarted = 0;

	if (!TrailStarted || cones_recorded == 100)
		return;

	if (cone_spacing++ == 0)
	{
		pos.vx = car_data[0].location.vx;
		pos.vz = car_data[0].location.vz;

		y = -MapHeightInt(&pos);

		fwrite(&pos.vx, 1, 4, TrailFile);
		fwrite(&y, 1, 4, TrailFile);
		fwrite(&pos.vz, 1, 4, TrailFile);

		cones_recorded++;
	}
	else if (cone_spacing > TrailSpacing)
	{
		cone_spacing = 0;
	}
}

// [0x4fe0cd] [D]
// Ends TRAIL.DAT and logs its last cone and the car's position.
void ExitSaveTrail(void)
{
	int marker;
	int last_x;
	int cone[3];
	size_t read;

	marker = 0x7FFFFFFF;
	fwrite(&marker, 1, 4, TrailFile);
	fclose(TrailFile);

	TrailFile = fopen("TRAIL.DAT", "rb");

	// read one int ahead: cone[] holds y and z of a cone and x of the next (the end marker after
	// the last)
	fread(&cone[2], 4, 1, TrailFile);

	do
	{
		last_x = cone[2];
		read = fread(cone, 4, 3, TrailFile);

		if (read != 3 || cone[1] == -1)
		{
			sprintf(MissionCommand, "Trail Not Recorded");
			LogQuick(MissionCommand);

			// NOTE: TrailFile is left open, as in the binary
			return;
		}
	}
	while (cone[2] != 0x7FFFFFFF);

	fclose(TrailFile);

	sprintf(MissionCommand, "Last Cone: x = %d, z = %d\n", last_x, cone[1]);
	LogQuick(MissionCommand);

	sprintf(MissionCommand, "End Car Position: x = %d, z = %d\n", car_data[0].location.vx, car_data[0].location.vz);
	LogQuick(MissionCommand);
}

// [0x4fe1e7] [D]
int InitTestTrail(void)
{
	return LoadTrailData("TRAIL.DAT");
}

// [0x4fe1f9] [D]
// Trail blazer (objective 7): a cone of the next five within 500 is hit (a second on the timer,
// the cone flies off); the 100th ends the course.
void HandleTrailBlazer(TRAIL_DATA *trail)
{
	SMASHABLE_OBJECT *sip;
	IVECTOR velocity;
	TRAIL_DATA *cone;
	int dist;
	int dx;
	int dz;
	int chan;
	int i;

	if ((mission.objective != 7 && !TestTrail) || !trail)
		return;

	cone = &trail[next_cone];

	for (i = 0; i < 5 && next_cone + i <= mission.iTrailCount; i++, cone++)
	{
		dx = abs(car_data[0].location.vx - cone->x);
		dz = abs(car_data[0].location.vz - cone->z);

		if (dx >= dz)
			dist = dx + dz / 2;
		else
			dist = dz + dx / 2;

		if (TestTrail || dist >= 500)
			continue;

		if (mission.timer->flags & 4)
			mission.timer->count += 50;

		cone_queue[next_flying_cone].pos.vx = (float)cone->x;
		cone_queue[next_flying_cone].pos.vy = (float)cone->y;
		cone_queue[next_flying_cone].pos.vz = (float)cone->z;
		cone_queue[next_flying_cone].type = mission.trail_model;

		velocity.vx = car_data[0].st.n.linearVelocity[0] / 4096;
		velocity.vy = -17;
		velocity.vz = car_data[0].st.n.linearVelocity[2] / 4096;

		damage_object(&cone_queue[next_flying_cone], &velocity);

		next_flying_cone = (next_flying_cone + 1) & 3;

		sip = &smashable[0];

		chan = Play3DSound(smashable[0].sound, -1, (float)car_data[0].location.vx, (float)-car_data[0].location.vy, (float)car_data[0].location.vz);

		if (chan != -1)
		{
			SetSoundFreq(chan, sip->pitch);
			SetSoundVol(chan, sip->volume);
		}

		global_score.items = ++mission.iTrailIndex;

		next_cone += i + 1;

		// NOTE: i restarts from 0 (then 1 after the loop's i++) while cone goes on, as in the binary
		i = 0;

		if (next_cone == 100)
		{
			mission.objective = 0x7FFF;

			DMSHandleTimer(mission.timer);

			global_score.lap_info = *mission.timer;
			return;
		}
	}
}

// [0x4fe514] [D]
// The next five cones of the course.
void DrawTrailblazer(TRAIL_DATA *trail)
{
	MODEL *model;
	VECTOR4 pos;
	VECTOR4 rel;
	int clip;
	int i;

	if ((mission.objective != 7 && !TestTrail) || !trail)
		return;

	if (TestTrail)
		model = model_pointers[426];
	else
		model = model_pointers[mission.trail_model];

	trail = &trail[next_cone];

	for (i = 0; i < 5 && next_cone + i <= mission.iTrailCount; i++, trail++)
	{
		pos.vx = (float)trail->x;
		pos.vy = (float)trail->y;
		pos.vz = (float)trail->z;

		clip = FrustrumCheckFar(&pos, model->bounding_sphere);

		if (!clip || !PositionVisible(&pos, CurrentPVS, camera.current_cell_x, camera.current_cell_z))
			continue;

		rel.vx = pos.vx - camera_position.vx;
		rel.vy = pos.vy - camera_position.vy;
		rel.vz = pos.vz - camera_position.vz;

		MatrixRotate(&rel);

		if (clip == 1)
			DrawObjectOSReuseMatrices(model, 0, &rel, 0);
		else if (clip == 2)
			DrawObjectOSReuseMatrices(model, 0, &rel, 1);
		else
			DrawObjectOSReuseMatrices(model, 0, &rel, 2);

		CurrentRotationMatrix = &inv_camera_matrix;
	}
}

// [0x4fe69b] [D]
void ExitTestTrail(void)
{
	if (pTrailData)
		DebugFree(pTrailData);

	pTrailData = NULL;
	mission.iTrailCount = 0;
}

// [0x4fe6cb] [D]
// NOTE: never steps through the table, as in the binary: loops for ever unless the first
// command (CHECKPOINT) has the type; no callers in the binary
DMSCommand *FindCommandType(int type)
{
	while (strlen(DmsCommands[0].name) != 0)
	{
		if (DmsCommands[0].type == type)
			return DmsCommands;
	}

	return NULL;
}

// [0x4fe701] [D]
// RANDOMWEATHER: rain (perhaps with lightning), snow or fog by the clock's low bits.
int DoRandomWeather(unsigned int level)
{
	char time;

	if (level >= 4)
		level = 0;

	time = (char)GetTicksMs();

	gDoRain = 0;
	gDoLightning = 0;
	do_fogging = 0;
	gDoSnow = 0;

	if (time & 1)
	{
		gDoRain = weather[level].rain;

		if (gDoRain && (time & 2))
			gDoLightning = weather[level].lightning;
	}

	if ((time & 4) && !gDoRain && !gNight)
		gDoSnow = weather[level].snow;

	if ((time & 8) && !gDoRain && !gDoSnow && gNight)
		do_fogging = weather[level].fog;

	return time & 8;
}
