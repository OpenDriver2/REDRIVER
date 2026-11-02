/*
 * MScript.c - the mission scripts (Scripts\Missions\MissionN.dms): compiler and interpreter
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "MScript.c"); partially translated (the compiler).
 *
 * A script is a text file of commands (DmsCommands) and variable assignments (DmsVars), one per
 * line, parameters separated by spaces or commas; "//" and "#" start a comment. Commands that
 * set up the level (LOADMAP, NIGHT, PLAYERCARTYPE ...) act while compiling; the others are
 * compiled into MissionList, a list of lines (command type, then int parameters or a string)
 * that ExecuteMissionScript runs from MissionHandler every frame.
 */

#include <stdio.h>
#include <string.h>
#include <windows.h>

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
#include "cops.h"
#include "felony.h"
#include "FMV/render.h"
#include "frontend/frontend.h"
#include "gsound.h"
#include "overlay.h"

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

// NOTE: no names in the database
static int dword_D000E0;
static int giCurrentMissionNumber;

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
		PlayRender(gScreen.handle, MissionQuickRender[0], MissionQuickRender[1]);

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

	time = (char)timeGetTime();

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
