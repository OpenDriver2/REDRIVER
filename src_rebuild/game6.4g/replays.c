/*
 * replays.c - pad recording and playback (replays, ghost laps, mission chases)
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "replays.c", 0x519bb0 - 0x51c163).
 *
 * A recording is a list of 5 byte records (PadStateRecord, PadStateRead[car]): the number of
 * frames the record repeats, the low 16 bits of controller_bits and the analogue x, y (signed
 * bytes). A .PADS file starts with NumReplayPads and the record count (bit 31: recorded with an
 * analogue joystick, ReplayAnalogue). Car 0 is the player, car 1 the chase leader / ghost car
 * (car_data[65]). User replays are ReplayData\User\<name>.CFG (REPLAY_CONFIG), .PADS and .CAMS;
 * mission chases keep the leader's start state (.LEADER), the player's (.PLAYER), the pads and
 * the civilian pings (.PINGS, REPLAY_PING) in ReplayData\Chase.
 *
 * The strings "ReplayData\User\$TEMP$.CAMS", "...$TEMP$.KEYS", "...$TEMP$.CFG" (0x577dc8 -
 * 0x577e1a) and "R" (0x5780d4) have no references: left over from functions the linker dropped.
 * The bss sits in the linker's common area; globals other files use are defined here as their
 * main user (see the NOTE), byte_CD4060 is left to interview.c.
 */

#include <stdio.h>
#include <string.h>

#include "dr_types.h"
#include "replays.h"
#include "dr_globals.h"
#include "ai.h"
#include "asmmath.h"
#include "camera.h"
#include "debris.h"
#include "debug.h"
#include "denting.h"
#include "DirectorStorage.h"
#include "effects.h"
#include "file.h"
#include "general.h"
#include "handling.h"
#include "input.h"
#include "interview.h"
#include "MScript.h"
#include "pedest.h"
#include "pres.h"
#include "shell.h"
#include "sound.h"
#include "Engine/3dfx/gfxinit.h"
#include "Engine/3dfx/polydraw.h"
#include "dr_scores.h"
#include "frontend/cheats.h"
#include "glaunch.h"

// the replay's settings (ReplayData\User\<name>.CFG)
typedef struct REPLAY_CONFIG
{
	int type;					// 0 free drive (gameMode is the level), 1 game mode, 2 mission chase
	int gameMode;
	int chaseNum;
	int carModel;
	char doNight;
	char doRain;
	char doSnow;
	char doLightning;
	char doFogging;
	char pad1[3];				// pad1 - pad4: ReplayCarEndPosition (see SetEndPosition)
	char field_18[12];
	int numReplayFrames;
	char doInterview;
	char scoreType;
	char chaseHQReplay;
	char reusePlayerData;
	LONGVECTOR3 position;
	int felony;
	int direction;
	int maxPedestrians;
	int damage[6];
	int totalDamage;
	char trafficDensity;
	char pad2[3];
	char difficulty;
	char pad3[3];
	char autoBrake;
	char pad4[3];
	CHEATS cheats;
} REPLAY_CONFIG;

// a civilian pinged in during a mission chase recording (ReplayData\Chase\<name>.PINGS)
typedef struct REPLAY_PING
{
	int frame;
	int id;
	CIV_STATE ai_data;
	CAR_DATA car_data;
} REPLAY_PING;

typedef struct REPLAY_CAMERA
{
	int frame;
	char cameraview;			// NOTE: IDA has int distance here; only its low byte is written
	char pad[3];
	VECTOR4 position;
	SVECTOR angle;
	int field_20;				// camera distance, in-car view or perspective mode (by cameraview)
	int turnAngle;
} REPLAY_CAMERA;

#define PAD_RECORD_SIZE		5
#define MAX_PAD_RECORD		0x40000
#define MAX_PINGS			2048
#define MAX_REPLAY_CAMERAS	512

// a pad no player can press: ends the last record when a recording stops
#define PAD_END_OF_RECORDING	0x10100000

/* interview.c */
extern char byte_CD4060;		// NOTE: untyped in IDA, 1 byte

/* handling.c */
extern RigidBodyState _tp[66];
extern RigidBodyState _d0[66];
extern RigidBodyState _d1[66];

void InitRecordingPads(void);
void StartOfGhostLap(void);
void GetRecordingPad(int car);
int GetRecordedPad(int car);
void RecordCamera(char replace);
void SaveReplayConfig(void);
int SetKeyFrame(int frame);
void EnableReplayEditing(void);

//------------------------------------------------------------------------------------------
// initialised data

int KeyFrame = -1;

//------------------------------------------------------------------------------------------
// uninitialised data
// NOTE: the linker's common area; globals other files use too (ReplayMode, NumReplayPads,
// ReplayAnalogue, RecordingCar, CamerasChanged, MissionName, rposname,
// dword_CCC8A0, dword_CCCC34, gStartGhostLap, gEndGhostLap) are defined here as their owner
// is guessed (most references; all their neighbours are replays.c globals)

int dword_BD9B04;
int dword_BD9B08;
char NoPlayerControl;
void *PadStateRecord;
int RecordPadRepeat[5];
int LastJoyAnalogueX[5];
int LastJoyAnalogueY[5];
char RecordingPads;
char ReplayingPads;
int RecordingCar;
int dword_BD9B58;
int RepCamRecNum;
int FinalRepCamRec;
char CamerasChanged[4];
int KeyFrameDelay;
int NumKeyFrames;
int dword_BD9B70;
int dword_BD9B74;
REPLAY_PING *PingBuffer;
size_t PingBufferPos;
size_t PingBufferEnd;
int dword_BD9B84;
// gChaseOverride (0xbd9b88) is defined in MScript.c
char EditingReplay;

size_t PadStateRecordSize;

char MissionPingName[64];
int num_replay_frames;
char byte_CCC804;
int NumReplayPads;
char PlayerPadName[64];
char ReplayName[64];
int dword_CCC8A0;				// the ghost lap is being played (cleared when it ends)
int BestPadCount;
int NumGhostPads;
char ReplayMode;				// 1 recording, 2 replaying, 3 mission chase
char *PadStateRead[5];
char rkeyname[64];
int ReplayPadRepeat[5];
int PadSameCount[5];
char PlayerCamName[64];
RigidBodyState best_lap_ghost_d0;
RigidBodyState best_lap_ghost_d1;
int dword_CCCA14;
int FinalReplayRec[5];
RigidBodyState best_lap_ghost_tp;
char MissionPlayerName[64];
int PadRecNumKeys[5];
int PadPlayNumKeys[5];
int dword_CCCB40;
char ReplayAnalogue;
RigidBodyState ghost_lap_d1;
RigidBodyState ghost_lap_d0;
int LastKnownPad[5];
int AllowReplayEditing;
int start_frame;
RigidBodyState ghost_lap_tp;
int dword_CCCC34;				// a best lap ghost has been recorded
REPLAY_CAMERA ReplayCameras[MAX_REPLAY_CAMERAS];
int dword_CD3C40;
char MissionName[64];
int ChaseHQReplay;
char rposname[64];
CAR_DATA best_lap_ghost_start;
int dword_CD3EA0;
int gStartGhostLap;
int PadRecNum;
CAR_DATA ghost_lap_start;
char rcamname[64];
int dword_CD4064;
REPLAY_CONFIG ReplayInfo;
char MissionPadName[64];
int dword_CD4160;
int gEndGhostLap;
char MissionLeaderName[64];
int PadReplayNum[5];

// The end position is kept in the padding bytes pad1, pad2, pad3, pad4 of REPLAY_CONFIG: 12
// bytes holding x, y, z as ints, byte by byte.
static char *EndPositionByte(int i)
{
	if (i < 3)
		return &ReplayInfo.pad1[i];

	if (i < 6)
		return &ReplayInfo.pad2[i - 3];

	if (i < 9)
		return &ReplayInfo.pad3[i - 6];

	return &ReplayInfo.pad4[i - 9];
}

// [A] the block InitReplay repeats for x, y, z
static int GetEndPosition(int axis)
{
	int value;
	char *bytes;
	int i;

	bytes = (char *)&value;

	for (i = 0; i < 4; i++)
		bytes[i] = *EndPositionByte(axis * 4 + i);

	return value;
}

// [A] the block SaveReplayConfig repeats for x, y, z
static void SetEndPosition(int axis, int value)
{
	char *bytes;
	int i;

	bytes = (char *)&value;

	for (i = 0; i < 4; i++)
		*EndPositionByte(axis * 4 + i) = bytes[i];
}

// [A] the block InitMissionReplay and InitReplayCars repeat: loads a .PADS file into
// PadStateRead[car]; size is the buffer size (0: the file's).
static int LoadPadFile(char *filename, int car, int *num_pads, int size, int line)
{
	FILE *fp;
	int length;

	fp = fopen(filename, "rb");

	if (!fp)
		return 0;

	length = FileSize(fp) - 4;

	// NOTE: the binary passes "D:\\Driver\\game6.4g\\replays.c" and the original line here
	PadStateRead[car] = (char *)DebugMalloc(size ? size : length, __FILE__, line);

	fread(num_pads, 1, 4, fp);
	fread(&FinalReplayRec[car], 1, 4, fp);
	fread(PadStateRead[car], 1, length, fp);

	fclose(fp);

	if (FinalReplayRec[car] < 0)
	{
		FinalReplayRec[car] &= ~0x80000000;
		ReplayAnalogue = 1;
	}

	return 1;
}

// [0x519bb0] [D]
void InitReplayGlobals(void)
{
	int i;

	dword_CCC8A0 = 1;
	dword_CCCC34 = 0;
	byte_CD4060 = 0;
	dword_CCCB40 = 0;
	AllowReplayEditing = 0;
	byte_CCC804 = 0;
	NoPlayerControl = 0;
	PadStateRecord = NULL;
	PadRecNum = 0;

	PadReplayNum[0] = PadReplayNum[1] = PadReplayNum[2] = PadReplayNum[3] = PadReplayNum[4] = 0;

	NumGhostPads = BestPadCount = 0;

	PadStateRecordSize = 0;
	dword_BD9B58 = 0;
	PadRecNumKeys[0] = 0;

	gStartGhostLap = 0;
	gEndGhostLap = 0;

	ClearMem(ReplayCameras, sizeof(ReplayCameras));

	RepCamRecNum = 0;
	FinalRepCamRec = 0;

	CamerasChanged[0] = 0;
	CamerasChanged[1] = 0;
	CamerasChanged[2] = 0;

	dword_CD3C40 = PadRecNumKeys[1] = dword_CD4064 = dword_CD3EA0 = 0;

	dword_CD4160 = 0;
	CamerasChanged[3] = 0;
	dword_CCCA14 = 0;
	KeyFrameDelay = 0;
	NumKeyFrames = 0;
	KeyFrame = -1;
	dword_BD9B70 = 0;
	dword_BD9B74 = 0;
	dword_BD9B08 = 0;
	dword_BD9B04 = 0;
	PingBuffer = NULL;
	PingBufferPos = 0;
	PingBufferEnd = 0;
	dword_BD9B84 = 0;

	ClearMem(&ReplayInfo, sizeof(REPLAY_CONFIG));
	ClearMem(&best_lap_ghost_start, sizeof(CAR_DATA));

	EditingReplay = 0;

	for (i = 0; i < 5; i++)
	{
		PadStateRead[i] = NULL;
		PadPlayNumKeys[i] = 0;
		RecordPadRepeat[i] = 0;
		LastKnownPad[i] = 0;
		PadSameCount[i] = 0;
		ReplayPadRepeat[i] = 0;
		LastJoyAnalogueX[i] = 0;
		LastJoyAnalogueY[i] = 0;
		PadSameCount[i] = 0;
		FinalReplayRec[i] = 0;
	}

	controller_bits = CAR_PAD_HANDBRAKE;
	NoPlayerControl = 0;

	RecordingPads = 0;
	ReplayingPads = 0;
	RecordingCar = 0;
	dword_BD9B58 = 0;
	RepCamRecNum = 0;
	FinalRepCamRec = 0;

	CamerasChanged[0] = 0;
	CamerasChanged[1] = 0;
	CamerasChanged[2] = 0;
	CamerasChanged[3] = 0;

	PingBufferPos = 0;
	PingBufferEnd = 0;
	dword_BD9B84 = 0;
	EditingReplay = 0;
	KeyFrameDelay = 0;
	NumKeyFrames = 0;
	ReplayAnalogue = 0;
	dword_BD9B08 = 0;
	KeyFrame = -1;
	dword_BD9B04 = 0;
	dword_BD9B70 = 0;
	dword_BD9B74 = 0;
	PingBuffer = NULL;
	PadStateRecord = NULL;
}

// [0x519f38] [D]
// Loads ReplayData\User\<rname>.CFG (or ReplayData\Demo\, which allows editing) and sets the
// game up as it was recorded; returns 0 on failure.
int InitReplay(char *rname)
{
	char fname[64];
	FILE *fp;

	dword_CCCC34 = 0;
	ChaseHQReplay = 0;
	AllowReplayEditing = 0;

	strcpy(ReplayName, rname);

	sprintf(fname, "ReplayData\\User\\%s.CFG", rname);

	fp = fopen(fname, "rb");

	if (fp)
	{
		fread(&ReplayInfo, 1, sizeof(REPLAY_CONFIG), fp);

		ChaseHQReplay = ReplayInfo.chaseHQReplay;

		fclose(fp);
	}
	else
	{
		sprintf(fname, "ReplayData\\Demo\\%s.CFG", rname);

		fp = fopen(fname, "rb");

		if (!fp)
			return 0;

		fread(&ReplayInfo, 1, sizeof(REPLAY_CONFIG), fp);

		ChaseHQReplay = ReplayInfo.chaseHQReplay;

		fclose(fp);

		AllowReplayEditing = 1;
	}

	ReplayCarEndPosition.vx = GetEndPosition(0);
	ReplayCarEndPosition.vy = GetEndPosition(1);
	ReplayCarEndPosition.vz = GetEndPosition(2);

	if (global_score.type != GAME_CARPARK)
		global_score.type = ReplayInfo.scoreType;

	gCheatFlags = ReplayInfo.cheats;
	gNight = ReplayInfo.doNight;
	gDoRain = ReplayInfo.doRain;
	gDoSnow = ReplayInfo.doSnow;
	gDoLightning = ReplayInfo.doLightning;
	do_fogging = ReplayInfo.doFogging;
	gTrafficDensity = ReplayInfo.trafficDensity;
	num_replay_frames = ReplayInfo.numReplayFrames;
	Reuse_player_data = ReplayInfo.reusePlayerData;

	memcpy(MStart.damage, ReplayInfo.damage, sizeof(MStart.damage));
	MStart.totalDamage = ReplayInfo.totalDamage;
	MStart.position.vx = ReplayInfo.position[0];
	MStart.position.vy = ReplayInfo.position[1];
	MStart.position.vz = ReplayInfo.position[2];
	MStart.felony = ReplayInfo.felony;
	MStart.direction = ReplayInfo.direction;

	max_pedestrians = ReplayInfo.maxPedestrians;
	gDifficulty = ReplayInfo.difficulty;
	gAutoBrake = ReplayInfo.autoBrake;

	if (ReplayInfo.doNight)
	{
		gDoLightFlare = 1;
		gDoPlayerHeadlight = 1;
	}
	else
	{
		gDoLightFlare = 0;
		gDoPlayerHeadlight = 0;
	}

	sprintf(PlayerPadName, "ReplayData\\User\\%s.PADS", rname);
	sprintf(PlayerCamName, "ReplayData\\User\\%s.CAMS", rname);

	if (!LoadWaypoints() && !LoadDirector(PlayerCamName))
		return 0;

	gDoInterview = ReplayInfo.doInterview;

	if (ReplayInfo.type == 0)
	{
		if (gbGameDemo)
		{
			ReplayMode = 2;

			if (gDoInterview)
			{
				GameMode = 0;
				GameLevel = 4;
				gNextScript = -1;
				giDemoScript = 0;
				gTryItScript = 201;
			}
			else
			{
				GameLevel = ReplayInfo.gameMode;
			}
		}
		else
		{
			GameMode = 0;
			ReplayMode = 2;

			if (gDoInterview)
				GameLevel = 4;
			else
				GameLevel = ReplayInfo.gameMode;
		}

		return 1;
	}

	if (ReplayInfo.type == 1)
	{
		GameMode = ReplayInfo.gameMode;
		ReplayMode = 2;

		return 1;
	}

	if (ReplayInfo.type == 2)
	{
		GameMode = ReplayInfo.gameMode;

		if (ChaseHQReplay)
			gChaseOverride = ReplayInfo.chaseNum;

		ReplayMode = 3;
		gMissionReplaying = 1;

		return 1;
	}

	return 0;
}

// [0x51a3b2] [D]
// NOTE: the second argument is unused (glaunch.c passes GameLevel for both)
void InitReplayFiles(int num, int a2)
{
	sprintf(MissionLeaderName, "ReplayData\\Chase\\%s.LEADER", MissionName);
	sprintf(MissionPlayerName, "ReplayData\\Chase\\%s.PLAYER", MissionName);
	sprintf(MissionPadName, "ReplayData\\Chase\\%s.PADS", MissionName);
	sprintf(MissionPingName, "ReplayData\\Chase\\%s.PINGS", MissionName);
	sprintf(rcamname, "ReplayData\\RepCams.%d", num);
	sprintf(rkeyname, "ReplayData\\KeyFrame.%d", num);
	sprintf(rposname, "ReplayData\\StartPos.%d", num);
}

// [0x51a455] [D]
// Mission chase: loads the leader's pads, start state and pings (car_data[65]), and either the
// player's (ChaseHQReplay) or starts recording the player.
void InitMissionReplay(void)
{
	char filename[128];
	FILE *fp;
	int num_pads;

	gNoCops = 1;

	// NOTE: in training the buffer is MAX_PAD_RECORD bytes (the ghost lap is recorded into it)
	if (LoadPadFile(MissionPadName, 1, &NumReplayPads, gDoTraining ? MAX_PAD_RECORD : 0,
		gDoTraining ? 621 : 623))
	{
		if (!gDoTraining || gTrainingMode != 2)
		{
			fp = fopen(MissionLeaderName, "rb");
			fread(&car_data[65], 1, sizeof(CAR_DATA), fp);
			fclose(fp);
		}

		car_data[65].savedDamage = 0;
		car_data[65].damage[5] = 0;
		car_data[65].damage[4] = 0;
		car_data[65].damage[3] = 0;
		car_data[65].damage[2] = 0;
		car_data[65].damage[1] = 0;
		car_data[65].damage[0] = 0;
		car_data[65].model = ChaseCarType;
		car_data[65].id = 65;

		civilian_ai_data[64].ctrlState = CIVSTATE_LEADING;
		civilian_ai_data[64].wrecked = 0;
		civilian_ai_data[64].model = ChaseCarType;
		civilian_ai_data[64].palette = ChaseCarPalette;

		ReplayPadRepeat[1] = 0;
		PadPlayNumKeys[1] = 0;
		PadReplayNum[1] = 0;
		PingBufferPos = 0;
		JoyAnalogueX = 0;
		LastJoyAnalogueX[1] = 0;
		JoyAnalogueY = 0;
		LastJoyAnalogueY[1] = 0;

		CreateDentableCar(64);

		// NOTE: the binary passes "D:\\Driver\\game6.4g\\replays.c", 669 here
		PingBuffer = (REPLAY_PING *)DebugMalloc(MAX_PINGS * sizeof(REPLAY_PING), __FILE__, __LINE__);

		if (PingBuffer)
		{
			sprintf(filename, "ReplayData\\Chase\\%s.PINGS", MissionName);

			fp = fopen(filename, "rb");

			fread(&PingBufferEnd, 1, 4, fp);
			fread(PingBuffer, PingBufferEnd, sizeof(REPLAY_PING), fp);

			fclose(fp);
		}
	}

	if (ChaseHQReplay)
	{
		if (LoadPadFile(PlayerPadName, 0, &num_pads, 0, 691))
		{
			if (!gDoTraining || gTrainingMode != 2)
			{
				fp = fopen(MissionPlayerName, "rb");
				fread(car_data, 1, sizeof(CAR_DATA), fp);
				fclose(fp);
			}

			car_data[0].savedDamage = 0;
			car_data[0].damage[5] = 0;
			car_data[0].damage[4] = 0;
			car_data[0].damage[3] = 0;
			car_data[0].damage[2] = 0;
			car_data[0].damage[1] = 0;
			car_data[0].damage[0] = 0;
			car_data[0].id = 0;

			ReplayPadRepeat[0] = 0;
			PadPlayNumKeys[0] = 0;
			PadReplayNum[0] = 0;
			LastJoyAnalogueX[0] = 0;
			LastJoyAnalogueY[0] = 0;

			CreateDentableCar(-1);

			NoPlayerControl++;
		}
	}
	else
	{
		RecordingCar = 0;
		TargetCar = 0;

		gBuildKeyFrames = 1;

		InitPadRecording();
	}
}

// [0x51a949] [D]
void InitRecordingPads(void)
{
	int i;

	start_frame = game_frame + 1;

	for (i = 0; i < 5; i++)
	{
		RecordPadRepeat[i] = 0;
		LastJoyAnalogueX[i] = 0;
		LastJoyAnalogueY[i] = 0;
		PadSameCount[i] = 0;
	}

	dword_BD9B58 = 0;
	RepCamRecNum = 0;
	FinalRepCamRec = 0;

	if (gMissionRecording)
		NumReplayPads = 0;

	JoyAnalogueX = 0;
	JoyAnalogueY = 0;

	PadStateRecordSize = 0;
	PadRecNum = 0;
	PingBufferPos = 0;
	RecordingPads = 1;
}

// [0x51aa1a] [D]
// Starts recording the player's pads; recording a mission chase also loads the leader
// (car_data[65]) and records the civilian pings.
void InitPadRecording(void)
{
	char filename[64];
	FILE *fp;

	if (NoPlayerControl)
		return;

	// NOTE: the binary passes "D:\\Driver\\game6.4g\\replays.c", 823 here
	PadStateRecord = DebugMalloc(MAX_PAD_RECORD, __FILE__, __LINE__);

	if (!PadStateRecord)
		return;

	if (gMissionRecording)
	{
		// NOTE: the binary passes "D:\\Driver\\game6.4g\\replays.c", 844 here
		PingBuffer = (REPLAY_PING *)DebugMalloc(MAX_PINGS * sizeof(REPLAY_PING), __FILE__, __LINE__);

		if (!PingBuffer)
			return;

		memset(PingBuffer, 0, MAX_PINGS * sizeof(REPLAY_PING));

		if (gMissionRecording)
		{
			sprintf(filename, "ReplayData\\Chase\\%s.LEADER", MissionName);

			fp = fopen(filename, "rb");

			fread(&car_data[65], 1, sizeof(CAR_DATA), fp);

			fclose(fp);

			car_data[65].id = 65;
			car_data[65].model = ChaseCarType;

			civilian_ai_data[64].ctrlState = CIVSTATE_LEADREC;
			civilian_ai_data[64].wrecked = 0;
			civilian_ai_data[64].model = ChaseCarType;
			civilian_ai_data[64].palette = ChaseCarPalette;

			CreateDentableCar(64);
		}
	}

	InitRecordingPads();

	controller_bits = CAR_PAD_HANDBRAKE;

	RecordCamera(0);
}

// [0x51ab67] [D]
// Saves the player's state at the start of the lap and records the lap's pads.
void StartOfGhostLap(void)
{
	CopyMem(&ghost_lap_start, car_data, sizeof(CAR_DATA));
	CopyMem(&ghost_lap_tp, _tp, sizeof(RigidBodyState));
	CopyMem(&ghost_lap_d0, _d0, sizeof(RigidBodyState));
	CopyMem(&ghost_lap_d1, _d1, sizeof(RigidBodyState));

	RecordingCar = 0;

	InitRecordingPads();
}

// [0x51abce] [D]
// A lap ended: a best lap becomes the ghost (its start state and pads); the ghost car
// (car_data[65]) restarts from the best lap's start.
void EndOfGhostLap(int best_lap)
{
	controller_bits = PAD_END_OF_RECORDING;

	GetRecordingPad(RecordingCar);

	if (best_lap)
	{
		CopyMem(&best_lap_ghost_start, &ghost_lap_start, sizeof(CAR_DATA));
		CopyMem(&best_lap_ghost_tp, &ghost_lap_tp, sizeof(RigidBodyState));
		CopyMem(&best_lap_ghost_d0, &ghost_lap_d0, sizeof(RigidBodyState));
		CopyMem(&best_lap_ghost_d1, &ghost_lap_d1, sizeof(RigidBodyState));

		CopyMem(PadStateRead[1], PadStateRecord, PadStateRecordSize);

		NumGhostPads = PadRecNum;
		dword_CCCC34 = 1;
	}

	StartOfGhostLap();

	ReplayPadRepeat[1] = ReplayPadRepeat[0];
	PadPlayNumKeys[1] = PadPlayNumKeys[0];
	PadReplayNum[1] = PadReplayNum[0];

	LastJoyAnalogueX[1] = LastJoyAnalogueX[0];
	LastJoyAnalogueY[1] = LastJoyAnalogueY[0];

	CopyMem(&car_data[65], &best_lap_ghost_start, sizeof(CAR_DATA));
	CopyMem(&_tp[65], &best_lap_ghost_tp, sizeof(RigidBodyState));
	CopyMem(&_d0[65], &best_lap_ghost_d0, sizeof(RigidBodyState));
	CopyMem(&_d1[65], &best_lap_ghost_d1, sizeof(RigidBodyState));

	car_data[65].id = 65;

	CreateDentableCar(65);

	BestPadCount = 0;
	dword_CCC8A0 = 1;
}

// [0x51ad2f] [D]
// Drives the ghost car (car_data[65]) with the best lap's pads; at their end it stops until
// the next lap.
void PlayGhostLap(void)
{
	int pad;

	if ((!gDoTraining || gTrainingMode != 2 || dword_CCC8A0) && dword_CCCC34)
	{
		pad = GetRecordedPad(1);

		if (PadReplayNum[1] == NumGhostPads)
		{
			CopyMem(&car_data[65], &best_lap_ghost_start, sizeof(CAR_DATA));
			CopyMem(&_tp[65], &best_lap_ghost_tp, sizeof(RigidBodyState));
			CopyMem(&_d0[65], &best_lap_ghost_d0, sizeof(RigidBodyState));
			CopyMem(&_d1[65], &best_lap_ghost_d1, sizeof(RigidBodyState));

			car_data[65].id = 65;

			CreateDentableCar(65);

			ReplayPadRepeat[1] = 0;
			LastKnownPad[1] = 0;
			PadReplayNum[1] = 0;
			PadPlayNumKeys[1] = 0;

			dword_CCC8A0 = 0;
		}
		else
		{
			ProcessCarPad(65, pad);
			StepCarPhysics(&car_data[65], 1);
		}
	}
}

// [0x51ae37] [D]
// Loads the player's pads of a user replay (PlayerPadName).
void InitReplayCars(void)
{
	if (gMissionReplaying)
		return;

	if (LoadPadFile(PlayerPadName, 0, &NumReplayPads, 0, 1033))
	{
		car_data[0].totalDamage = 0;
		car_data[0].damage[5] = 0;
		car_data[0].damage[4] = 0;
		car_data[0].damage[3] = 0;
		car_data[0].damage[2] = 0;
		car_data[0].damage[1] = 0;
		car_data[0].damage[0] = 0;

		ReplayPadRepeat[0] = 0;
		PadPlayNumKeys[0] = 0;
		PadReplayNum[0] = 0;

		PingBufferPos = 0;

		JoyAnalogueX = 0;
		LastJoyAnalogueX[0] = 0;

		JoyAnalogueY = 0;
		LastJoyAnalogueY[0] = 0;
	}
}

// [0x51afad] [D]
// Records this frame's controller_bits and analogue x, y: a pad equal to the last one only
// counts up the last record (up to 255 repeats), a new pad adds a record.
void GetRecordingPad(int car)
{
	unsigned char *record;

	if (!RecordingPads)
		return;

	if (gMissionRecording)
		NumReplayPads++;

	if (PadStateRecordSize > MAX_PAD_RECORD - PAD_RECORD_SIZE)
	{
		RecordingPads = 0;
		return;
	}

	if (controller_bits == PadSameCount[car] && RecordPadRepeat[car] < 255 && game_frame >= start_frame &&
		JoyAnalogueX == LastJoyAnalogueX[car] && JoyAnalogueY == LastJoyAnalogueY[car])
	{
		RecordPadRepeat[car]++;
		return;
	}

	if (RecordPadRepeat[car])
	{
		// rewrite the last record with its repeat count
		if (PadStateRecordSize)
			PadStateRecordSize -= PAD_RECORD_SIZE;

		record = (unsigned char *)PadStateRecord + PadStateRecordSize;

		record[0] = RecordPadRepeat[car];
		*(short *)&record[1] = PadSameCount[car];
		record[3] = LastJoyAnalogueX[car];
		record[4] = LastJoyAnalogueY[car];

		PadStateRecordSize += PAD_RECORD_SIZE;
	}

	RecordPadRepeat[car] = 0;

	record = (unsigned char *)PadStateRecord + PadStateRecordSize;

	record[0] = RecordPadRepeat[car];
	*(short *)&record[1] = controller_bits;
	record[3] = JoyAnalogueX;
	record[4] = JoyAnalogueY;

	PadStateRecordSize += PAD_RECORD_SIZE;

	PadSameCount[car] = controller_bits;
	LastJoyAnalogueX[car] = JoyAnalogueX;
	LastJoyAnalogueY[car] = JoyAnalogueY;

	PadRecNum++;
}

// [0x51b1bf] [D]
// The recorded pad of this frame (the analogue x, y go to JoyAnalogueX, JoyAnalogueY for the
// player and the training ghost). At the end of the recording: training ends the replay, a
// mission chase finishes the leader and, at the end of the player's pads, ends the game or
// starts the replay editor.
int GetRecordedPad(int car)
{
	char *record;

	if (!PadStateRead[car])
		return CAR_PAD_HANDBRAKE;

	if (gMissionReplaying && (LeadCarHit || LeadCarLost || LeadCarFinished))
		return CAR_PAD_HANDBRAKE;

	if (ReplayPadRepeat[car])
	{
		ReplayPadRepeat[car]--;

		if (car == 0 || (gTrainingMode == 2 && gDoTraining))
		{
			JoyAnalogueX = LastJoyAnalogueX[car];
			JoyAnalogueY = LastJoyAnalogueY[car];
		}

		return LastKnownPad[car];
	}

	if (PadReplayNum[car] != FinalReplayRec[car] || (gDoTraining && gTrainingMode == 2 && car != 0))
	{
		record = &PadStateRead[car][PadPlayNumKeys[car]];

		ReplayPadRepeat[car] = (unsigned char)record[0];
		LastKnownPad[car] = *(short *)&record[1];
		LastJoyAnalogueX[car] = (signed char)record[3];
		LastJoyAnalogueY[car] = (signed char)record[4];

		if (car == 0 || (gTrainingMode == 2 && gDoTraining))
		{
			JoyAnalogueX = LastJoyAnalogueX[car];
			JoyAnalogueY = LastJoyAnalogueY[car];
		}

		PadPlayNumKeys[car] += PAD_RECORD_SIZE;
		PadReplayNum[car]++;

		return LastKnownPad[car];
	}

	if (gDoTraining)
	{
		EndOfTrainingReplay();
		return CAR_PAD_HANDBRAKE;
	}

	if (gMissionReplaying)
	{
		if (!LeadCarLost && !LeadCarHit && !LeadCarFinished)
			LeadCarFinished = 1;

		if (!PadStateRead[0])
			return CAR_PAD_HANDBRAKE;

		if (PadReplayNum[0] == FinalReplayRec[0])
		{
			if (!AllowReplayEditing)
			{
				exit_code = 100;
				quit = 0;
				return 0;
			}

			CamerasChanged[3] = 1;

			EnableReplayEditing();
			PauseSounds();
		}
	}

	return CAR_PAD_HANDBRAKE;
}

// [0x51b440] [D]
// Feeds this frame's pad to the player's car (and the chased car 65), recorded or played back
// as ReplayMode says, and adds the cars to the physics.
void ControlCars(void)
{
	int joyX;
	int joyY;
	int pad;

	if (gStartGhostLap)
		StartOfGhostLap();

	if (gEndGhostLap)
		EndOfGhostLap(mission.best_lap);

	gStartGhostLap = 0;
	gEndGhostLap = 0;

	if (ReplayMode != 3 || (gDoTraining && gTrainingMode == 3))
	{
		if (ReplayMode == 1)
		{
			if (gMissionRecording)
			{
				GetRecordingPad(1);
				ProcessCarPad(65, controller_bits);
				StepCarPhysics(&car_data[65], 1);
			}
			else
			{
				GetRecordingPad(0);
				ProcessCarPad(0, controller_bits);
				StepCarPhysics(car_data, 1);
			}
		}
		else if (ReplayMode == 2)
		{
			pad = GetRecordedPad(0);

			ProcessCarPad(0, pad);
			StepCarPhysics(car_data, 1);
		}
	}
	else if (gDoTraining && gTrainingMode == 2)
	{
		joyX = JoyAnalogueX;
		joyY = JoyAnalogueY;

		PlayGhostLap();

		JoyAnalogueX = joyX;
		JoyAnalogueY = joyY;

		GetRecordingPad(0);
		ProcessCarPad(0, controller_bits);
		StepCarPhysics(car_data, 1);
	}
	else
	{
		if (mission.objective == 0x7FFF)
			ProcessCarPad(65, CAR_PAD_HANDBRAKE);
		else
			ProcessCarPad(65, GetRecordedPad(1));

		StepCarPhysics(&car_data[65], 1);

		if (!gDoTraining || gTrainingMode != 0)
		{
			if (ChaseHQReplay)
				controller_bits = GetRecordedPad(0);
			else
				GetRecordingPad(0);

			ProcessCarPad(0, controller_bits);
			StepCarPhysics(car_data, 1);
		}
	}
}

// [0x51b662] [D]
// A key frame every 450 frames.
void ControlKeyFrame(void)
{
	if (KeyFrameDelay == 0)
	{
		KeyFrameDelay = 450;
		KeyFrame++;
	}

	KeyFrameDelay--;
}

// [0x51b696] [D]
// Ends recording (saves the replay config, the pads to ReplayData\User\$TEMP$.PADS or the
// mission chase's .PADS and .PINGS) and frees the pad buffers.
void DeInitPadPlayRec(void)
{
	char filename[64];
	FILE *fp;
	int i;

	num_replay_frames = game_frame;

	if (RecordingPads)
	{
		SaveReplayConfig();

		controller_bits = PAD_END_OF_RECORDING;
		GetRecordingPad(RecordingCar);

		RecordingPads = 0;

		if (!NoPlayerControl)
		{
			if (gMissionRecording)
			{
				sprintf(filename, "ReplayData\\Chase\\%s.PADS", MissionName);
				fp = fopen(filename, "wb");
			}
			else
			{
				fp = fopen("ReplayData\\User\\$TEMP$.PADS", "wb");
			}

			if (fp)
			{
				if (OldJoyIsAnalogue)
					PadRecNum |= 0x80000000;

				fwrite(&NumReplayPads, 1, 4, fp);
				fwrite(&PadRecNum, 1, 4, fp);
				fwrite(PadStateRecord, 1, PadStateRecordSize, fp);

				fclose(fp);

				DebugFree(PadStateRecord);
				PadStateRecord = NULL;
			}

			if (gMissionRecording)
			{
				sprintf(filename, "ReplayData\\Chase\\%s.PINGS", MissionName);

				fp = fopen(filename, "wb");

				if (fp)
				{
					fwrite(&PingBufferPos, 1, 4, fp);
					fwrite(PingBuffer, PingBufferPos, sizeof(REPLAY_PING), fp);

					fclose(fp);
				}
			}
		}
	}

	if (PingBuffer)
	{
		DebugFree(PingBuffer);
		PingBuffer = NULL;
	}

	for (i = 0; i < 5; i++)
	{
		if (PadStateRead[i])
		{
			DebugFree(PadStateRead[i]);
			PadStateRead[i] = NULL;
		}
	}

	ReplayMode = 0;
	gMissionRecording = 0;
	gMissionReplaying = 0;
	ChaseHQReplay = 0;
}

// [0x51b8d5] [D]
// Writes the game's settings to ReplayData\User\$TEMP$.CFG.
void SaveReplayConfig(void)
{
	FILE *fp;

	memset(&ReplayInfo, 0, sizeof(ReplayInfo));

	SetEndPosition(0, (int)ReplayCarEndPosition.vx);
	SetEndPosition(1, (int)ReplayCarEndPosition.vy);
	SetEndPosition(2, (int)ReplayCarEndPosition.vz);

	ReplayInfo.scoreType = global_score.type;
	ReplayInfo.cheats = gCheatFlags;
	ReplayInfo.doInterview = gDoInterview;

	if (Reuse_player_data)
	{
		ReplayInfo.reusePlayerData = 1;

		memcpy(ReplayInfo.damage, MStart.damage, sizeof(ReplayInfo.damage));
		ReplayInfo.totalDamage = MStart.totalDamage;
		ReplayInfo.position[0] = float_to_int(MStart.position.vx);
		ReplayInfo.position[1] = float_to_int(MStart.position.vy);
		ReplayInfo.position[2] = float_to_int(MStart.position.vz);
		ReplayInfo.felony = MStart.felony;
		ReplayInfo.direction = MStart.direction;
	}
	else
	{
		ReplayInfo.reusePlayerData = 0;
	}

	ReplayInfo.trafficDensity = gTrafficDensity;
	ReplayInfo.maxPedestrians = max_pedestrians;
	ReplayInfo.difficulty = gDifficulty;
	ReplayInfo.autoBrake = gAutoBrake;
	ReplayInfo.carModel = car_data[0].model;
	ReplayInfo.doNight = gNight;
	ReplayInfo.doRain = gDoRain;
	ReplayInfo.doSnow = gDoSnow;
	ReplayInfo.doLightning = gDoLightning;
	ReplayInfo.doFogging = do_fogging;
	ReplayInfo.numReplayFrames = num_replay_frames;

	if (gMissionReplaying)
		ReplayInfo.chaseHQReplay = 1;

	if (GameMode)
	{
		ReplayInfo.gameMode = GameMode;

		if (gMissionReplaying)
		{
			ReplayInfo.type = 2;
			ReplayInfo.chaseNum = gChaseOverride;
		}
		else
		{
			ReplayInfo.type = 1;
		}
	}
	else
	{
		ReplayInfo.type = 0;
		ReplayInfo.gameMode = GameLevel;
	}

	fp = fopen("ReplayData\\User\\$TEMP$.CFG", "wb");

	if (fp)
	{
		fwrite(&ReplayInfo, 1, sizeof(REPLAY_CONFIG), fp);
		fclose(fp);
	}
}

// [0x51bb9f] [D]
// Records the ping of civilian civ_id (mission chase recording).
void SaveControlledPing(int civ_id)
{
	REPLAY_PING *ping;

	// BUG: read from unverified memory
	ping = &PingBuffer[PingBufferPos];

	if (PingBuffer && PingBufferPos < MAX_PINGS)
	{
		ping->frame = game_frame;
		ping->id = civ_id;

		CopyMem(&ping->ai_data, &civilian_ai_data[civ_id], sizeof(CIV_STATE));
		CopyMem(&ping->car_data, &civ_ai_data_pt[civ_id], sizeof(CAR_DATA));

		PingBufferPos++;
	}
}

// [0x51bc36] [D]
// Pings in the civilian recorded for this frame; returns its id or -1.
int DoControlledPing(void)
{
	REPLAY_PING *ping;
	int id;

	// BUG: read from unverified memory
	ping = &PingBuffer[PingBufferPos];
	id = -1;

	if (PingBuffer && PingBufferPos < MAX_PINGS && ping->frame == game_frame)
	{
		id = ping->id;

		CopyMem(&civilian_ai_data[id], &ping->ai_data, sizeof(CIV_STATE));
		CopyMem(&civ_ai_data_pt[id], &ping->car_data, sizeof(CAR_DATA));

		CreateDentableCar(id);

		PingBufferPos++;
	}

	return id;
}

// [0x51bce8] [D]
// A debug "R" in the corner: green while replaying, red while recording.
void ShowReplayStatus(void)
{
	char text[3] = "R";

	if (NoPlayerControl)
		DBGPrintCol(0, 240, 0);
	else if (RecordingPads)
		DBGPrintCol(240, 0, 0);
	else
		return;

	DBGPrintPos(0.0f, 0.0f);
	DBGPrintString(text);
	DBGPrintCol(240, 240, 160);
}

// [0x51bd71] [D]
// Stores the current camera at RepCamRecNum: inserted with this frame's number, or replacing
// the camera there (replace).
void RecordCamera(char replace)
{
	REPLAY_CAMERA *cam;
	int i;

	if (RepCamRecNum == MAX_REPLAY_CAMERAS)
		return;

	if (!replace && RepCamRecNum < FinalRepCamRec)
	{
		for (i = FinalRepCamRec + 1; i > RepCamRecNum; i--)
			ReplayCameras[i] = ReplayCameras[i - 1];
	}

	cam = &ReplayCameras[RepCamRecNum];

	if (!replace)
		cam->frame = game_frame;

	cam->cameraview = cameraview;

	switch (cameraview)
	{
		case 0:
			cam->position = camera_position;
			cam->angle = camera_angle;
			cam->field_20 = (int)gCameraDistance;
			cam->turnAngle = gCameraAngle;
			break;
		case 1:
			cam->position = camera_position;
			cam->angle = camera_angle;
			break;
		case 2:
			cam->field_20 = CameraInCar;
			break;
		case 16:
			cam->position = holding_camera_position;
			cam->angle = holding_camera_angle;
			cam->field_20 = CamPerspectiveMode & 1;
			break;
		case 17:
			cam->position = holding_camera_position;
			cam->angle = camera_angle;
			cam->field_20 = CamPerspectiveMode & 1;
			break;
	}

	if (replace)
		RepCamRecNum++;
	else
		FinalRepCamRec++;

	CamerasChanged[0] = 1;
}

// [0x51bff1] [D]
// Moves the held camera along its view direction.
void AdjustReplayCamera(float dist)
{
	holding_camera_position.vx = dist * inv_camera_matrix.m[0][2] + holding_camera_position.vx;
	holding_camera_position.vy = dist * inv_camera_matrix.m[1][2] + holding_camera_position.vy;
	holding_camera_position.vz = dist * inv_camera_matrix.m[2][2] + holding_camera_position.vz;
}

// [0x51c035] [D]
void MoveReplayCamera(float dy)
{
	holding_camera_position.vy = holding_camera_position.vy + dy;
}

// [0x51c049] [D]
void InitKeyFrame(int frame)
{
}

// [0x51c04e] [D]
int SetKeyFrame(int frame)
{
	return 1;
}

// [0x51c058] [D]
int NextKeyFrame(void)
{
	if (KeyFrame >= NumKeyFrames - 1)
		return 0;

	SetKeyFrame(KeyFrame + 1);

	gTimeInWater = 15;
	gSinkingTimer = 180;

	return 1;
}

// [0x51c09c] [D]
// Back to the start of this key frame, or to the previous one in its first 15 frames.
int PrevKeyFrame(void)
{
	if (KeyFrame == 0)
	{
		RepCamRecNum = 0;
		SetKeyFrame(0);
	}
	else if (KeyFrameDelay > 435)
	{
		SetKeyFrame(KeyFrame - 1);
	}
	else
	{
		SetKeyFrame(KeyFrame);
	}

	gTimeInWater = 15;
	gSinkingTimer = 180;

	return 1;
}

// [0x51c107] [D]
void EnableReplayEditing(void)
{
	EditingReplay = 1;
}

// [0x51c113] [D]
void DisableReplayEditing(void)
{
	EditingReplay = 0;
}

// [0x51c11f] [D]
void ControlReplayEditor(void)
{
	SwapPalette(24);
	SetTextColour(255, 255, 255);
	last_state = -1;
}

// [0x51c14f] [D]
void sub_51C14F(void)
{
}

// [0x51c154] [D]
void sub_51C154(void)
{
}

// [0x51c159] [D]
void sub_51C159(void)
{
}

// [0x51c15e] [D]
void sub_51C15E(void)
{
}
