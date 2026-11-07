/*
 * interview.c - the interview: the car park test before the game, and the training ghost trail
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "interview.c", 0x4e6ae0 - 0x4e99a5).
 *
 * In the interview (GameLevel 4) the player has a stopwatch lap (HandleStopwatch) to do the nine
 * tasks on the paper (TaskStatus): a burnout, a handbrake turn, the slaloms, a 180, a 360, a
 * reverse 180, a quick stop, a brake turn and a lap of the car park. HandleInterviewTasks watches
 * the car for them (the slaloms and the lap by the car park zones it drives through,
 * VisitedZones); four crashes end it. In training (gDoTraining) and on mission chases
 * (GameMode 201) AddToGhostTrail lays a fading trail behind the leader car (car_data[65]) that
 * DrawGhostTrail draws and CloseToTrail scores the player against.
 */

#include <stdio.h>
#include <string.h>

#include "dr_types.h"
#include "interview.h"
#include "dr_globals.h"
#include "ai.h"
#include "asmmath.h"
#include "camera.h"
#include "general.h"
#include "gsound.h"
#include "handling.h"
#include "map.h"
#include "MScript.h"
#include "pause.h"
#include "pres.h"
#include "shell.h"
#include "sound.h"
#include "testmath.h"
#include "D3DGlide/glide_ptrs.h"
#include "Engine/3dfx/gfxinit.h"
#include "Engine/3dfx/tset.h"
#include "frontend/font.h"
#include "Engine/3dfx/object.h"
#include "Engine/3dfx/polydraw.h"
#include "dr_cars.h"
#include "frontend/gvars.h"
#include "glaunch.h"

#define NUM_TASKS			9
#define NUM_GHOST_TRAIL		40

// game_frame % 3 + one of these, from BANK0.DSS (loaded at 0xF000)
#define INTERVIEW_SPEECH_BASE	0xF000

// tasks (TaskStatus, TaskSpeech, CrossoutInfo indices)
#define TASK_BURNOUT		0
#define TASK_HANDBRAKE		1
#define TASK_SLALOM			2
#define TASK_180			3
#define TASK_360			4
#define TASK_REVERSE_180	5
#define TASK_QUICK_STOP		6
#define TASK_BRAKE_TURN		7
#define TASK_LAP			8

// giInterviewState
#define INTERVIEW_START		0
#define INTERVIEW_RUNNING	1
#define INTERVIEW_PASSED	2	// all tasks done (or the training replay ended)
#define INTERVIEW_OVER		3	// time up or four crashes

typedef struct INTERVIEW_TASK
{
	int count;
	const int *zones;		// the car park zones (sub_4E79DA) driven through, in order
} INTERVIEW_TASK;

// one piece of the ghost trail: a quad edge across the leader car's path
typedef struct GHOST_TRAIL
{
	VECTOR4 left;			// world positions (200 either side behind the car)
	VECTOR4 right;
	VECTOR4 view_left;		// in view space (DrawGhostTrail)
	VECTOR4 view_right;
	int unknown;			// NOTE: never read or written
	u_char r;
	u_char g;
	u_char b;
	u_char used;
} GHOST_TRAIL;

int sub_4E79DA(int x, int z);
void sub_4E7A58(int zone);
int sub_4E7E84(INTERVIEW_TASK *task);
void InitInterviewMode(void);
void InitInterviewCars(void);
void TrainingHandler(void);
void HandleStopwatch(void);
void HandleInterviewTasks(char pad, CAR_DATA *cp);
void SetTrainingSpeechData(int text, int time, int if_quiet);
void DrawStopwatch(void);
void DrawTasks(void);
int CompleteInterviewTask(int task);
void DrawCrossout(int task);
void InitInterviewPaper(void);
void InitInterviewGhost(void);
void DrawGhostTrail(void);
void AddToGhostTrail(void);
void CloseToTrail(void);
void Draw2DTexturedQuadTC(float x, float y, TEX_INFO *tex, u_char r, u_char g, u_char b);
void sub_4E9570(TEX_INFO *tex, char *name);
void DrawTexturedQuadEx(TEX_INFO *tex, float x, float y, int width, int max_width);
void DrawScaledTexturedPoly(TEX_INFO *tex, float x, float y, float xscale, float yscale);

// read-only data (0x5489b8): the zones of the car park tasks
static const int slalom_left_zones[16] = { 0, 1, 4, 3, 6, 7, 10, 9, 12, 13, 10, 9, 6, 7, 4, 3 };
static const int slalom_right_zones[16] = { 1, 2, 5, 4, 7, 8, 11, 10, 13, 14, 11, 10, 7, 8, 5, 4 };
static const int lap_zones[12] = { 0, 1, 2, 5, 8, 11, 14, 13, 12, 9, 6, 3 };

// initialised data (0x564d50)

// per task: the paper text (gLanguageStrings) and the speech sample (-1 none)
int TaskSpeech[NUM_TASKS][2] = {
	{ 273, -1 },
	{ 274, -1 },
	{ 275, -1 },
	{ 276, -1 },
	{ 277, 4 },
	{ 278, -1 },
	{ 279, -1 },
	{ 280, 3 },
	{ 281, -1 },
};

// NOTE: no references; 9 x, y pairs (perhaps positions of the task texts on the paper)
static int unknown_564d98[NUM_TASKS][2] = {
	{ 400, 30 },
	{ 397, 40 },
	{ 399, 52 },
	{ 403, 63 },
	{ 403, 74 },
	{ 403, 83 },
	{ 405, 93 },
	{ 406, 103 },
	{ 414, 116 },
};

// the TaskStatus counts sub_4E81E1 counts up to
static u_char task_max[16] = { 60, 80, 50, 30, 30, 80, 50, 80, 30 };

// per task the crossout on the paper: x from, x to, y from, y to (x from the paper's left edge)
float CrossoutInfo[NUM_TASKS][4] = {
	{ 21.0f, 79.0f, 20.0f, 28.0f },
	{ 17.0f, 91.0f, 31.0f, 39.0f },
	{ 18.0f, 67.0f, 41.0f, 49.0f },
	{ 21.0f, 59.0f, 52.0f, 59.0f },
	{ 23.0f, 58.0f, 62.0f, 69.0f },
	{ 20.0f, 109.0f, 72.0f, 80.0f },
	{ 25.0f, 71.0f, 83.0f, 90.0f },
	{ 25.0f, 101.0f, 93.0f, 101.0f },
	{ 31.0f, 62.0f, 104.0f, 111.0f },
};

INTERVIEW_TASK SlalomLeftTask = { 16, slalom_left_zones };
INTERVIEW_TASK SlalomRightTask = { 16, slalom_right_zones };
INTERVIEW_TASK LapTask = { 12, lap_zones };

// uninitialised data
// NOTE: in the linker's common area (0xd82720 - 0xd83c10); only interview.c uses them (except
// giInterviewState, read by mirror.c), so they are defined here. byte_CD4060 (0xcd4060) is set
// with cameraview by interview.c and replays.c; defined here too.

int ReverseCount;
char gLastScoringPiece;
int LastCarDam;
TEX_INFO *gCrossTexture;
TEX_INFO *gTasksTexture;
int TimeTillFastSpin;
int gGhostWrap;				// the trail has wrapped round
int TurnCount180;
GHOST_TRAIL gGhostTrail[NUM_GHOST_TRAIL];
int MaxAngVel;
int TurnCount360;
int TimeTillCarCollision;
int giJobCountdown;
int gGivenPraiseTimer;
int ReachedSpeed;
float gStopwatchAngle;		// the stopwatch hand, 4096 a lap
int giTrainingSpeechTimer;
int MaxWheelSpeed;
TEX_INFO *gGhostTexture;
TEX_INFO *gLineTexture;
int CrossoutTimers[NUM_TASKS];	// how much of each crossout line is drawn
int giChaseTimer;
int gInterviewFrames;
int TimeTillSpeed;
int giTrainingSpeechCounter;	// frames gszTrainingSpeechBuffer stays on screen
int giStopwatchFlash;
int gbTrainingOver;
// the last car park zones driven through, newest first
// NOTE: 22 ints up to gCurrentGhostTrail, 21 are used; the matching in sub_4E7E84 can read one
// further
int VisitedZones[22];
int gCurrentGhostTrail;		// the next gGhostTrail piece
int old_smashable_count;
IVECTOR dummy;
u_char TasksComplete;
int giInterviewState;
char gClosestPiece;
int CameraAngleCopy;
int giInterviewDamage;		// crashes, 4 ends the interview
int BrakeCount;
char gszTrainingSpeechBuffer[256];
int ReverseTest;
float gStopwatchStep;
TEX_INFO *gStopwatchTexture;
int gTrainingOffPath;
int TimeTillAverageSpin;
CAR_DATA GhostStart;
int ReverseFlag;
int gbTrainingDemonstration;
int giStopwatchFlashDir;
int BrakeTest;
CAR_DATA ThisCircuitRaceStart;
int Doing180;
int gOffPathTimer;
u_char TaskStatus[12];		// per task: done (sub_4E81E1: a count up to task_max)
int Doing360;
int giTrainingScore;
char byte_CD4060;			// NOTE: untyped in IDA, 1 byte; set to 1 with cameraview 0

// [0x4e6ae0] [D]
void InitInterviewMode(void)
{
	gClosestPiece = 0;
	gLastScoringPiece = 0;
	gCurrentGhostTrail = 0;
	giTrainingScore = 0;
	gTrainingOffPath = 0;
	gOffPathTimer = 0;
	giTrainingSpeechTimer = 0;
	gGivenPraiseTimer = 0;
	giTrainingSpeechCounter = 0;
	CameraAngleCopy = 0;
	giChaseTimer = 0;
	giInterviewState = INTERVIEW_START;
	gInterviewFrames = 0;
	giJobCountdown = 0;
	giInterviewDamage = 0;
	ReverseFlag = 0;
	ReverseTest = 0;
	ReverseCount = 0;
	BrakeCount = 0;
	ReachedSpeed = 0;
	Doing360 = 0;
	Doing180 = 0;
	TurnCount180 = 0;
	TurnCount360 = 0;
	old_smashable_count = 0;
	giStopwatchFlash = 0;
	giStopwatchFlashDir = 0;
	gStopwatchAngle = 0.0f;
	gStopwatchStep = 0.0f;
	gbTrainingOver = 0;
	gGhostWrap = 0;
	gCarCarCollision = 0;
	gbTrainingDemonstration = 0;
	MaxAngVel = 0;
	MaxWheelSpeed = 0;
	LastCarDam = 0;
	TimeTillSpeed = 0;
	TimeTillAverageSpin = 0;
	TimeTillFastSpin = 0;
	TimeTillCarCollision = 0;
	BrakeTest = 0;

	dummy.vz = 0;
	dummy.vy = 0;
	dummy.vx = 0;

	ClearMem(TaskStatus, NUM_TASKS);
	ClearMem(&GhostStart, sizeof(CAR_DATA));
	ClearMem(gGhostTrail, sizeof(gGhostTrail));
	ClearMem(VisitedZones, 20 * sizeof(int));
	ClearMem(CrossoutTimers, sizeof(CrossoutTimers));
	ClearMem(&ThisCircuitRaceStart, sizeof(CAR_DATA));
}

// [0x4e6cfc] [D]
void InitInterview(void)
{
	InitInterviewMode();

	LastCarDam = 0;
	MaxAngVel = 0;
	MaxWheelSpeed = 0;
	TimeTillSpeed = 0;
	TimeTillAverageSpin = 0;
	TimeTillFastSpin = 0;
	TimeTillCarCollision = 0;
	gInterviewFrames = 0;
	ReachedSpeed = 0;
	gbTrainingOver = 0;
	Doing180 = 0;
	Doing360 = 0;
	TurnCount180 = 40;
	TurnCount360 = 70;
	giStopwatchFlash = 0;
	giStopwatchFlashDir = 0;

	gStopwatchTexture = GetTextureDetails("STOPWATCH");

	giTrainingScore = 0;
	gTrainingOffPath = 0;
	gStopwatchAngle = 0.0f;
	giInterviewState = INTERVIEW_START;
	gGivenPraiseTimer = 240;
	old_smashable_count = 0;
	gOffPathTimer = 30;
	gStopwatchStep = 1.365f;
	gCameraAngle = 2048;

	InitInterviewCars();

	gStopPadReads = 1;
	cameraview = 0;
	byte_CD4060 = 1;
	giInterviewDamage = 0;

	InitInterviewPaper();
	InitInterviewGhost();

	LoadSounds("BANK0.DSS", INTERVIEW_SPEECH_BASE);
}

// [0x4e6e55] [D]
// Parks cars in the car park for the interview.
void InitInterviewCars(void)
{
	if (GameLevel == 4 && !gbGameDemo && GameMode != 201 && !gRecordTraining && !gMissionRecording)
	{
		AllocateAndPlaceCarAtPosition(-16000 - (Random() & 0xFE), 200, -10000, 1024, 0);
		AllocateAndPlaceCarAtPosition(-16000 - (Random() & 0xFE), 200, -11000, 1024, 0);
		AllocateAndPlaceCarAtPosition(-16000 - (Random() & 0xFE), 200, -14000, 1024, 0);
		AllocateAndPlaceCarAtPosition(-16000 - (Random() & 0xFE), 200, -17000, 1024, 0);
		AllocateAndPlaceCarAtPosition(-16000 - (Random() & 0xFE), 200, -18500, 1024, 0);
		AllocateAndPlaceCarAtPosition(-12800, 200, -10200, 1024, 0);

		AllocateAndPlaceCarAtPosition((Random() & 0xFE) - 4700, 200, -9000, 3072, 0);
		AllocateAndPlaceCarAtPosition((Random() & 0xFE) - 4700, 200, -11000, 3072, 0);
		AllocateAndPlaceCarAtPosition((Random() & 0xFE) - 4700, 200, -12000, 3072, 0);
		AllocateAndPlaceCarAtPosition((Random() & 0xFE) - 4700, 200, -14000, 3072, 0);
		AllocateAndPlaceCarAtPosition(-8200, 200, -18000, 3072, 0);
		AllocateAndPlaceCarAtPosition(-8200, 200, -20000, 3072, 0);
		AllocateAndPlaceCarAtPosition(-8200, 200, -21000, 3072, 0);
	}
}

// [0x4e7082] [D]
void DoTrainingMode(char pad)
{
	CAR_DATA *cp;

	if (GameLevel == 4 || gDoTraining)
	{
		if (gRecordTraining)
			cp = &car_data[65];
		else
			cp = &car_data[PlayerCar];

		if (!GamePaused)
			gInterviewFrames++;

		if (giTrainingSpeechCounter)
			giTrainingSpeechCounter--;

		if (gGivenPraiseTimer > 0)
			gGivenPraiseTimer--;

		HandleStopwatch();
		TrainingHandler();
		HandleInterviewTasks(pad, cp);
	}
}

// [0x4e7131] [D]
void TrainingHandler(void)
{
	if (giInterviewState == INTERVIEW_START)
	{
		TasksComplete = 0;

		gCameraAngle = 2048;
		gInterviewFrames = 0;
		gStopPadReads = 0;
		giTrainingSpeechCounter = 0;

		mission.objective = 0;

		giInterviewState = INTERVIEW_RUNNING;

		if (gDoTraining)
		{
			if (gbGameDemo)
				SetTrainingSpeechData(289, 250, 0);
			else if (GameLevel == 4)
				SetTrainingSpeechData(282, 250, 0);
		}
		else
		{
			SetTrainingSpeechData(282, 250, 0);
		}
	}
	else if (giInterviewState == INTERVIEW_PASSED)
	{
		if (mission.pause_timer)
		{
			gCameraAngle = (gCameraAngle - 11) & 0xFFF;
		}
		else if (!gbTrainingOver)
		{
			if (gTrainingMode != 1)
			{
				SetTrainingSpeechData(283, 150, 0);
				AddToSpeechQueue(game_frame % 3 + INTERVIEW_SPEECH_BASE + 10);
			}

			mission.pause_timer = 150;
			mission.objective = 0x8000;

			gbTrainingOver = 1;
		}
	}
	else if (giInterviewState == INTERVIEW_OVER)
	{
		if (mission.pause_timer)
		{
			gCameraAngle = (gCameraAngle - 11) & 0xFFF;
		}
		else if (!gbTrainingOver)
		{
			if (gTrainingMode != 1)
			{
				if (giInterviewDamage == 4)
				{
					SetTrainingSpeechData(284, 300, 0);
					AddToSpeechQueue(INTERVIEW_SPEECH_BASE + 5);
				}
				else
				{
					SetTrainingSpeechData(285, 150, 0);

					if (TasksComplete < 2)
						AddToSpeechQueue(INTERVIEW_SPEECH_BASE + 7);
					else if (TasksComplete < 5)
						AddToSpeechQueue(INTERVIEW_SPEECH_BASE + 6);
					else if (TasksComplete < 7)
						AddToSpeechQueue(INTERVIEW_SPEECH_BASE + 8);
					else
						AddToSpeechQueue(INTERVIEW_SPEECH_BASE + 9);
				}
			}

			cameraview = 0;
			byte_CD4060 = 1;

			mission.pause_timer = 300;
			mission.objective = 4080;

			gbTrainingOver = 1;
		}
	}
	else if (gDoTraining)
	{
		AddToGhostTrail();
		CloseToTrail();
	}
}

// [0x4e73a4] [D]
// Turns the stopwatch hand; it flashes in the last quarter and the interview is over at a lap.
void HandleStopwatch(void)
{
	if (giInterviewState == INTERVIEW_RUNNING && !GamePaused && GameLevel == 4)
	{
		if (giStopwatchFlashDir)
		{
			giStopwatchFlash += giStopwatchFlashDir;

			if (giStopwatchFlash < 0)
			{
				giStopwatchFlash = 0;
				giStopwatchFlashDir = -giStopwatchFlashDir;

				if (gStopwatchAngle > 4095.0f)
					giStopwatchFlashDir = 0;
			}

			if (giStopwatchFlash > 255)
			{
				giStopwatchFlash = 255;
				giStopwatchFlashDir = -giStopwatchFlashDir;
			}
		}
		else if (gStopwatchAngle > 3072.0f && gStopwatchAngle < 4096.0f)
		{
			giStopwatchFlashDir = 12;
			giStopwatchFlash = 0;
		}

		if (gStopwatchAngle >= 4096.0f)
		{
			gStopPadReads = 1;

			giInterviewState = INTERVIEW_OVER;
			gInterviewFrames = 0;

			cameraview = 0;
		}
		else if (!GamePaused)
		{
			gStopwatchAngle = gStopwatchAngle + gStopwatchStep;
		}
	}
}

// [0x4e74d5] [D]
// Watches the car for the interview tasks and crashes.
void HandleInterviewTasks(char pad, CAR_DATA *cp)
{
	int reversing;
	int speed;
	int damage;
	int angVel;
	int crashed;

	if (gMissionRecording)
	{
		if (GameLevel != 4)
			return;
	}
	else if (giInterviewState != INTERVIEW_RUNNING || GameLevel != 4)
	{
		return;
	}

	speed = cp->wheel_speed >> 12;

	if (speed < 0)
	{
		reversing = 1;
		speed = -speed;
	}
	else
	{
		reversing = 0;
	}

	if (MaxWheelSpeed < speed)
		MaxWheelSpeed = speed;

	angVel = cp->st.n.angularVelocity[1] >> 12;

	if (angVel < 0)
		angVel = -angVel;

	if (angVel > MaxAngVel)
		MaxAngVel = angVel;

	damage = GetTotalDamage(cp) >> 3;

	if (giInterviewState == INTERVIEW_RUNNING && giInterviewDamage == 4)
	{
		giInterviewState = INTERVIEW_OVER;
		gStopPadReads = 1;
		gInterviewFrames = 0;
		return;
	}

	sub_4E7A58(sub_4E79DA(cp->location.vx, cp->location.vz));

	if (sub_4E7E84(&SlalomLeftTask))
		CompleteInterviewTask(TASK_SLALOM);

	if (sub_4E7E84(&SlalomRightTask))
		CompleteInterviewTask(TASK_SLALOM);

	if (sub_4E7E84(&LapTask))
		CompleteInterviewTask(TASK_LAP);

	// spinning for 70 frames
	if (angVel <= 300)
	{
		Doing360 = 0;
	}
	else if (Doing360)
	{
		if (TurnCount360)
			TurnCount360--;
		else
			CompleteInterviewTask(TASK_360);
	}
	else
	{
		Doing360 = 1;
		TurnCount360 = 70;
	}

	// a handbrake turn for 40 frames
	if (angVel <= 250)
	{
		Doing180 = 0;
	}
	else if (Doing180)
	{
		if (TurnCount180)
			TurnCount180--;
		else
			CompleteInterviewTask(TASK_180);
	}
	else if (pad & CAR_PAD_HANDBRAKE)
	{
		Doing180 = 1;
		TurnCount180 = 40;
	}

	if (TimeTillSpeed > 0)
		TimeTillSpeed--;

	if (TimeTillAverageSpin > 0)
		TimeTillAverageSpin--;

	if (TimeTillFastSpin > 0)
		TimeTillFastSpin--;

	if (TimeTillCarCollision > 0)
		TimeTillCarCollision--;

	crashed = 0;

	if (gCarCarCollision && TimeTillCarCollision == 0)
	{
		crashed = 1;
		TimeTillCarCollision = 40;
		LastCarDam = damage;

		SetTrainingSpeechData(286, 150, 1);

		if (!IsSpeechPlaying())
			AddToSpeechQueue(game_frame % 3 + INTERVIEW_SPEECH_BASE);
	}

	if (TimeTillCarCollision > 0)
		gCarCarCollision = 0;

	if (damage - LastCarDam > 30 && TimeTillCarCollision == 0)
	{
		crashed = 1;
		TimeTillCarCollision = 40;

		SetTrainingSpeechData(287, 150, 1);

		if (!IsSpeechPlaying())
			AddToSpeechQueue(game_frame % 3 + INTERVIEW_SPEECH_BASE);
	}

	if (TimeTillCarCollision > 0)
		LastCarDam = damage;

	if (crashed && giInterviewDamage < 4)
		giInterviewDamage++;

	// spinning for 40 frames after reversing fast
	if (reversing && speed > 100)
		ReverseTest = 1;

	if (angVel > 200 && ReverseTest)
	{
		if (ReverseCount)
			ReverseCount--;
		else
			CompleteInterviewTask(TASK_REVERSE_180);
	}
	else
	{
		ReverseTest = 0;
		ReverseCount = 40;
	}

	// braking from speed, 40 frames on
	if (cp->wheel_speed > 696320)
	{
		BrakeTest = 1;
		BrakeCount = 40;
	}
	else if (!(pad & CAR_PAD_BRAKE) || crashed)
	{
		BrakeTest = 0;
	}

	if (BrakeTest)
	{
		if (BrakeCount)
			BrakeCount--;
		else
			CompleteInterviewTask(TASK_BRAKE_TURN);
	}

	// fast for more than 5 frames, then nearly stopped
	if (cp->wheel_speed > 737280)
		ReachedSpeed++;

	if (crashed || cp->wheel_speed < 0)
		ReachedSpeed = 0;

	if (ReachedSpeed > 5 && cp->wheel_speed < 81920)
	{
		CompleteInterviewTask(TASK_QUICK_STOP);
		ReachedSpeed = 0;
	}

	if ((pad & CAR_PAD_WHEELSPIN) && speed > 50)
		CompleteInterviewTask(TASK_BURNOUT);

	if ((pad & CAR_PAD_HANDBRAKE) && speed > 50)
		CompleteInterviewTask(TASK_HANDBRAKE);
}

// [0x4e79da] [D]
// The car park zone at x z: 3 columns by 5 rows, 0 - 14.
int sub_4E79DA(int x, int z)
{
	int zone;

	zone = 0;

	if (z <= -10460)
	{
		if (z > -13460)
			zone = 3;
		else if (z > -16460)
			zone = 6;
		else if (z > -19460)
			zone = 9;
		else
			zone = 12;
	}

	if (x <= -7453)
	{
		if (x > -13526)
			zone++;
		else
			zone += 2;
	}

	return zone;
}

// [0x4e7a58] [D]
// Adds a zone to VisitedZones when the car has moved into another.
void sub_4E7A58(int zone)
{
	int i;

	if (zone != VisitedZones[0])
	{
		for (i = 20; i > 0; i--)
			VisitedZones[i] = VisitedZones[i - 1];

		VisitedZones[0] = zone;
	}
}

// [0x4e7aa2] [D]
// Shows a gLanguageStrings text for time frames; with if_quiet only when no other is showing.
void SetTrainingSpeechData(int text, int time, int if_quiet)
{
	if (!if_quiet || giTrainingSpeechCounter == 0)
	{
		sprintf(gszTrainingSpeechBuffer, gLanguageStrings[text]);
		giTrainingSpeechCounter = time;
	}
}

// NOTE: ours. How far, in x of 640, the stopwatch and the task paper move to stay at their side
// of a screen wider than 4:3, as the other side items of the HUD (wide_hud_edges)
static float InterviewEdgeShift(void)
{
	if (!wide_hud_edges)
		return 0.0f;

	return ((float)display_width - HUD_WIDTH) / 2.0f * 640.0f / HUD_WIDTH;
}

// [0x4e7ad9] [D]
void DisplayTrainingData(void)
{
	if (GameLevel == 4)
	{
		if (GameMode == 201)
			DrawGhostTrail();

		if (GameLevel == 4)
		{
			DrawStopwatch();
			DrawTasks();
		}

		if (giTrainingSpeechCounter)
			DrawMissionText(gszTrainingSpeechBuffer);
	}
	else if (GameLevel == 5 && gDoTraining)
	{
		DrawGhostTrail();
	}
}

// [0x4e7b3a] [D] [A]
// NOTE: [A] wider than 4:3 the stopwatch and its hand stay at the left edge (InterviewEdgeShift)
void DrawStopwatch(void)
{
	int angle;
	float x;
	float y;
	float left;

	left = -InterviewEdgeShift();

	if (gStopwatchTexture)
	{
		if (gPreviousAlpha != 1)
		{
			grAlphaCombine(GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_ONE, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_TEXTURE, FXTRUE);
			grAlphaTestFunction(GR_CMP_GREATER);
			gPreviousAlpha = 1;
		}

		if (giStopwatchFlashDir)
			Draw2DTexturedQuadTC(left, 0.0f, gStopwatchTexture, 255, 255 - giStopwatchFlash, 255 - giStopwatchFlash);
		else
			Draw2DTexturedQuadTC(left, 0.0f, gStopwatchTexture, 255, 255, 255);

		if (gPreviousAlpha != 0)
		{
			grAlphaCombine(GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_ONE, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_CONSTANT, FXTRUE);
			grAlphaTestFunction(GR_CMP_ALWAYS);
			gPreviousAlpha = 0;
		}
	}

	// the hand
	angle = float_to_int(gStopwatchAngle) & 0xFFF;

	x = fsintab[angle] * 32.0f + 47.0f + left;
	y = fsintab[angle + 1024] * -32.0f + 87.0f;

	DrawScaledLine(47.0f + left, 87.0f, x, y, 0);
}

// [0x4e7c98] [D] [A]
// The task paper at the top right with the done tasks crossed out, and a cross per crash.
// NOTE: [A] wider than 4:3 the paper stays at the right edge (InterviewEdgeShift)
void DrawTasks(void)
{
	int i;
	float x;
	float y;
	float left;

	if (!gTasksTexture)
		return;

	left = 640.0f - (float)gTasksTexture->w + InterviewEdgeShift();

	if (gPreviousAlpha != 1)
	{
		grAlphaCombine(GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_ONE, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_TEXTURE, FXTRUE);
		grAlphaTestFunction(GR_CMP_GREATER);
		gPreviousAlpha = 1;
	}

	Draw2DTexturedQuadTC(left, 0.0f, gTasksTexture, 255, 255, 255);

	if (gPreviousAlpha != 0)
	{
		grAlphaCombine(GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_ONE, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_CONSTANT, FXTRUE);
		grAlphaTestFunction(GR_CMP_ALWAYS);
		gPreviousAlpha = 0;
	}

	for (i = 0; i < NUM_TASKS; i++)
	{
		if (TaskStatus[i])
			DrawCrossout(i);
	}

	x = left + 11.0f;
	y = 0.0f + 10.0f;

	i = giInterviewDamage;

	while (i--)
	{
		if (gCrossTexture)
		{
			DrawScaledTexturedPoly(gCrossTexture, x, y, 0.5f, 0.5f);
			x = x + (float)gCrossTexture->w;
		}
		else
		{
			DrawScaledLine(x, y, x + 10.0f, y + 8.0f, 0xFF0000);
			DrawScaledLine(x, y + 8.0f, x + 10.0f, y, 0xFF0000);
			x = x + 20.0f;
		}
	}
}

// [0x4e7e84] [D]
// 1 if the last zones driven through follow the task's zone loop (either way round) for more
// than a whole loop.
int sub_4E7E84(INTERVIEW_TASK *task)
{
	int i;
	int n;
	int j;

	if (!task->zones)
		return 0;

	for (i = 0; i < task->count; i++)
	{
		if (VisitedZones[0] == task->zones[i])
		{
			n = 0;
			j = i;

			while (VisitedZones[n] == task->zones[j])
			{
				n++;

				if (++j >= task->count)
					j = 0;
			}

			if (n > task->count)
				return 1;
		}
	}

	for (i = 0; i < task->count; i++)
	{
		if (VisitedZones[0] == task->zones[i])
		{
			n = 0;
			j = i;

			while (VisitedZones[n] == task->zones[j])
			{
				n++;

				if (--j < 0)
					j = task->count - 1;
			}

			if (n > task->count)
				return 1;
		}
	}

	return 0;
}

// [0x4e7fc2] [D]
// Ticks off a task; 0 if it was done already.
int CompleteInterviewTask(int task)
{
	if (TaskStatus[task])
		return 0;

	TaskStatus[task] = 1;
	TasksComplete++;

	SetTrainingSpeechData(TaskSpeech[task][0], 150, 0);

	if (TaskSpeech[task][1] != -1)
		AddToSpeechQueue(TaskSpeech[task][1] + INTERVIEW_SPEECH_BASE);

	if (TasksComplete == NUM_TASKS)
		giInterviewState = INTERVIEW_PASSED;

	TimeTillSpeed = 0;
	TimeTillAverageSpin = 0;
	TimeTillFastSpin = 0;

	return 1;
}

// [0x4e806d] [D]
int GetTotalDamage(CAR_DATA *cp)
{
	return cp->totalDamage;
}

// [0x4e807b] [D]
// Draws the crossout lines of the done tasks out, 3 pixels a frame.
void HandleCrossouts(void)
{
	int w;
	int i;

	if (GameLevel == 4)
	{
		for (i = 0; i < NUM_TASKS; i++)
		{
			if (TaskStatus[i])
			{
				w = gLineTexture->w;

				CrossoutTimers[i] += 3;

				if (CrossoutTimers[i] > w)
					CrossoutTimers[i] = w;
			}
		}
	}
}

// [0x4e80f7] [D] [A]
// NOTE: reads gTasksTexture->w before checking gTasksTexture, as in the original (DrawTasks
// only calls it with a texture)
// NOTE: [A] wider than 4:3 the crossouts move with the paper (InterviewEdgeShift)
void DrawCrossout(int task)
{
	float *info;
	float left;

	info = CrossoutInfo[task];
	left = 640.0f - (float)gTasksTexture->w + InterviewEdgeShift();

	if (gTasksTexture)
	{
		if (gLineTexture)
		{
			DrawTexturedQuadEx(gLineTexture, left + info[0], info[2], CrossoutTimers[task], gLineTexture->w);
		}
		else
		{
			DrawScaledLine(left + info[0], info[2], left + info[1], info[3], 0);
			DrawScaledLine(left + info[1], info[2], left + info[0], info[3], 0);
		}
	}
}

// [0x4e81e1] [D]
// Counts a task up by 19 to its task_max; 0 if it is there already.
// NOTE: unreferenced (only its thunk is left)
int sub_4E81E1(int task)
{
	if (TaskStatus[task] >= task_max[task])
		return 0;

	TaskStatus[task] += 19;

	if (TaskStatus[task] > task_max[task])
		TaskStatus[task] = task_max[task];

	return 1;
}

// [0x4e824a] [D]
void EndOfTrainingReplay(void)
{
	if (giInterviewState != INTERVIEW_PASSED)
	{
		giInterviewState = INTERVIEW_PASSED;
		cameraview = 0;
		byte_CD4060 = 1;
		gInterviewFrames = 0;
		gStopPadReads = 1;
	}
}

// [0x4e8286] [D]
void InitRecTrainingModeCounter(void)
{
	gInterviewFrames = 180;
}

// [0x4e8295] [D]
// Finds the paper textures; other languages copy their paper over ELPAPEL.
void InitInterviewPaper(void)
{
	int i;

	gTasksTexture = GetTextureDetails("ELPAPEL");
	gLineTexture = GetTextureDetails("LINE");
	gCrossTexture = GetTextureDetails("CROSS");

	switch (gUserLanguage)
	{
		case 1:
			sub_4E9570(gTasksTexture, "PAPER_FR");
			break;
		case 2:
			sub_4E9570(gTasksTexture, "PAPER_GE");
			break;
		case 3:
			sub_4E9570(gTasksTexture, "PAPER_SP");
			break;
		case 4:
			sub_4E9570(gTasksTexture, "PAPER_IT");
			break;
	}

	for (i = 0; i < NUM_TASKS; i++)
		TaskStatus[i] = 0;
}

// [0x4e837e] [D]
// Clears the ghost trail; the GHOST texture is used one texel in from each edge (u v w h become
// the corners).
void InitInterviewGhost(void)
{
	ClearMem(gGhostTrail, sizeof(gGhostTrail));

	gCurrentGhostTrail = 0;
	gGhostWrap = 0;

	gGhostTexture = GetTextureDetails("GHOST");

	if (gGhostTexture)
	{
		gGhostTexture->u++;
		gGhostTexture->v++;
		gGhostTexture->w += gGhostTexture->u - 2;
		gGhostTexture->h += gGhostTexture->v - 2;
	}
}

// [0x4e8430] [D]
// Draws the ghost trail additively, newest piece first, as quads between neighbouring pieces.
void DrawGhostTrail(void)
{
	MATRIX mat;
	int i;
	int prev;
	float ooz;
	float scale;
	GHOST_TRAIL *gt;
	VECTOR4 *p0;
	VECTOR4 *p1;
	VECTOR4 *p2;
	VECTOR4 *p3;
	VECTOR4 left;
	VECTOR4 right;
	GrVertex v0;
	GrVertex v1;
	GrVertex v2;
	GrVertex v3;

	gte_SetTransVector(&dummy);

	if (last_state != TEXT3)
	{
		grColorCombine(GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_LOCAL, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_TEXTURE, FXFALSE);
		last_state = TEXT3;
	}

	if (gGhostTexture)
	{
		if (gGhostTexture->tset != last_tset)
		{
			last_tset = gGhostTexture->tset;
			grTexSource(GR_TMU0, TextureSlots[gGhostTexture->tset].texture_ram_address, GR_MIPMAPLEVELMASK_BOTH, TextureSlots[gGhostTexture->tset].TInfo);
		}
	}
	else if (last_tset != 0)
	{
		last_tset = 0;
		grTexSource(GR_TMU0, TextureSlots[0].texture_ram_address, GR_MIPMAPLEVELMASK_BOTH, TextureSlots[0].TInfo);
	}

	if (last_trans_on != 3)
	{
		grAlphaBlendFunction(GR_BLEND_ONE, GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_ZERO);
		last_trans_on = 3;
	}

	if (FlipX)
	{
		mat = inv_camera_matrix;
		mat.m[0][0] = -inv_camera_matrix.m[0][0];
		mat.m[1][0] = -inv_camera_matrix.m[1][0];
		mat.m[2][0] = -inv_camera_matrix.m[2][0];
		CurrentRotationMatrix = &mat;
	}
	else
	{
		CurrentRotationMatrix = &inv_camera_matrix;
	}

	for (i = 0; i < NUM_GHOST_TRAIL; i++)
	{
		left = gGhostTrail[i].left;
		right = gGhostTrail[i].right;

		left.vx = left.vx - camera_position.vx;
		left.vy = left.vy - camera_position.vy;
		left.vz = left.vz - camera_position.vz;

		right.vx = right.vx - camera_position.vx;
		right.vy = right.vy - camera_position.vy;
		right.vz = right.vz - camera_position.vz;

		MatrixRotate(&left);
		MatrixRotate(&right);

		gGhostTrail[i].view_left = left;
		gGhostTrail[i].view_right = right;
	}

	i = gCurrentGhostTrail - 1;
	i += i < 0 ? NUM_GHOST_TRAIL : 0;

	do
	{
		prev = i - 1;
		prev += prev < 0 ? NUM_GHOST_TRAIL : 0;

		if (!gGhostTrail[i].used || !gGhostTrail[prev].used)
			break;

		p0 = &gGhostTrail[i].view_left;
		p1 = &gGhostTrail[prev].view_left;
		p2 = &gGhostTrail[prev].view_right;
		p3 = &gGhostTrail[i].view_right;

		if (p0->vz > 50.0f && p1->vz > 50.0f && p2->vz > 50.0f && p3->vz > 50.0f)
		{
			ooz = ooz_table[(int)p0->vz];
			scale = ooz * DIST;
			v0.x = scale * p0->vx + screen_xmid - VERTEX_SNAPPER;
			v0.y = scale * p0->vy + screen_ymid - VERTEX_SNAPPER;
			v0.oow = ooz;

			ooz = ooz_table[(int)p1->vz];
			scale = ooz * DIST;
			v1.x = scale * p1->vx + screen_xmid - VERTEX_SNAPPER;
			v1.y = scale * p1->vy + screen_ymid - VERTEX_SNAPPER;
			v1.oow = ooz;

			ooz = ooz_table[(int)p2->vz];
			scale = ooz * DIST;
			v2.x = scale * p2->vx + screen_xmid - VERTEX_SNAPPER;
			v2.y = scale * p2->vy + screen_ymid - VERTEX_SNAPPER;
			v2.oow = ooz;

			ooz = ooz_table[(int)p3->vz];
			scale = ooz * DIST;
			v3.x = scale * p3->vx + screen_xmid - VERTEX_SNAPPER;
			v3.y = scale * p3->vy + screen_ymid - VERTEX_SNAPPER;
			v3.oow = ooz;

			gt = &gGhostTrail[i];
			v0.r = (float)gt->r;
			v0.g = (float)gt->g;
			v0.b = (float)gt->b;
			v3.r = (float)gt->r;
			v3.g = (float)gt->g;
			v3.b = (float)gt->b;

			gt = &gGhostTrail[prev];
			v1.r = (float)gt->r;
			v1.g = (float)gt->g;
			v1.b = (float)gt->b;
			v2.r = (float)gt->r;
			v2.g = (float)gt->g;
			v2.b = (float)gt->b;

			if (gGhostTexture)
			{
				v0.tmuvtx[0].sow = (float)gGhostTexture->u * v0.oow;
				v0.tmuvtx[0].tow = (float)gGhostTexture->v * v0.oow;
				v1.tmuvtx[0].sow = (float)gGhostTexture->w * v1.oow;
				v1.tmuvtx[0].tow = (float)gGhostTexture->v * v1.oow;
				v2.tmuvtx[0].sow = (float)gGhostTexture->w * v2.oow;
				v2.tmuvtx[0].tow = (float)gGhostTexture->h * v2.oow;
				v3.tmuvtx[0].sow = (float)gGhostTexture->u * v3.oow;
				v3.tmuvtx[0].tow = (float)gGhostTexture->h * v3.oow;
			}
			else
			{
				v0.tmuvtx[0].sow = 0.0f;
				v0.tmuvtx[0].tow = 0.0f;
				v1.tmuvtx[0].sow = 0.0f;
				v1.tmuvtx[0].tow = 0.0f;
				v2.tmuvtx[0].sow = 0.0f;
				v2.tmuvtx[0].tow = 0.0f;
				v3.tmuvtx[0].sow = 0.0f;
				v3.tmuvtx[0].tow = 0.0f;
			}

			guDrawTriangleWithClip(&v0, &v1, &v2);
			guDrawTriangleWithClip(&v0, &v2, &v3);
		}

		i--;
		i += i < 0 ? NUM_GHOST_TRAIL : 0;

		if (i == (gCurrentGhostTrail + 1) % NUM_GHOST_TRAIL)
			break;
	} while (i != gCurrentGhostTrail);
}

// [0x4e8db2] [D]
// Every 4 frames adds a piece across the path of the leader car (car_data[65]); the pieces fade.
void AddToGhostTrail(void)
{
	int i;
	VECTOR4 pos;

	if (GamePaused || (!gDoTraining && GameMode != 201) || (gDoTraining && gTrainingMode != 1))
		return;

	for (i = 0; i < NUM_GHOST_TRAIL; i++)
	{
		if (gGhostTrail[i].r >= 3)
			gGhostTrail[i].r -= 3;

		if (gGhostTrail[i].g >= 4)
			gGhostTrail[i].g -= 4;

		if (gGhostTrail[i].b >= 4)
			gGhostTrail[i].b -= 4;
	}

	if ((game_frame & 3) == 0)
	{
		CurrentRotationMatrix = &float_body_mat[65];

		gGhostTrail[gCurrentGhostTrail].r = 0;
		gGhostTrail[gCurrentGhostTrail].g = 0;
		gGhostTrail[gCurrentGhostTrail].b = 0;
		gGhostTrail[gCurrentGhostTrail].used = 1;

		if (gCurrentGhostTrail)
		{
			gGhostTrail[gCurrentGhostTrail - 1].r = 255;
			gGhostTrail[gCurrentGhostTrail - 1].g = 255;
			gGhostTrail[gCurrentGhostTrail - 1].b = 255;
		}
		else if (gGhostWrap)
		{
			gGhostTrail[NUM_GHOST_TRAIL - 1].r = 255;
			gGhostTrail[NUM_GHOST_TRAIL - 1].g = 255;
			gGhostTrail[NUM_GHOST_TRAIL - 1].b = 255;
		}

		pos.vx = 200.0f;
		pos.vy = 0.0f;
		pos.vz = -200.0f;
		MatrixRotateB(&pos);

		pos.vx = pos.vx + (float)car_data[65].location.vx;
		pos.vz = pos.vz + (float)car_data[65].location.vz;
		pos.vy = (float)-MapHeight(&pos) - 10.0f;

		gGhostTrail[gCurrentGhostTrail].left = pos;

		pos.vx = -200.0f;
		pos.vy = 0.0f;
		pos.vz = -200.0f;
		MatrixRotateB(&pos);

		pos.vx = pos.vx + (float)car_data[65].location.vx;
		pos.vz = pos.vz + (float)car_data[65].location.vz;
		pos.vy = (float)-MapHeight(&pos) - 10.0f;

		gGhostTrail[gCurrentGhostTrail++].right = pos;

		if (gCurrentGhostTrail >= NUM_GHOST_TRAIL)
		{
			gCurrentGhostTrail = 0;
			gGhostWrap = 1;
		}
	}
}

// [0x4e90b0] [D]
// Finds the trail piece closest to the player; too far off the trail counts gTrainingOffPath.
void CloseToTrail(void)
{
	int closest;
	int n;
	int x;
	int z;
	int dx;
	int dz;
	int dist;

	closest = 99999999;

	if (GamePaused || (!gDoTraining && GameMode != 201) || (gDoTraining && gTrainingMode != 1))
		return;

	if (gOffPathTimer > 0)
		gOffPathTimer--;

	if (gGhostWrap)
	{
		n = NUM_GHOST_TRAIL;
	}
	else
	{
		if (gCurrentGhostTrail == 0)
			return;

		n = gCurrentGhostTrail - 1;
	}

	x = car_data[0].location.vx;
	z = car_data[0].location.vz;

	while (n--)
	{
		dx = x - float_to_int(gGhostTrail[n].left.vx);

		if (dx < 0)
			dx = -dx;

		dz = z - float_to_int(gGhostTrail[n].left.vz);

		if (dz < 0)
			dz = -dz;

		dist = dz + dx;

		dx = x - float_to_int(gGhostTrail[n].right.vx);

		if (dx < 0)
			dx = -dx;

		dz = z - float_to_int(gGhostTrail[n].right.vz);

		if (dz < 0)
			dz = -dz;

		dist = dz + dx + dist;

		if (dist < closest)
		{
			closest = dist;
			gClosestPiece = n;
		}
	}

	if (closest < 800 && gClosestPiece != gLastScoringPiece)
		gLastScoringPiece = gClosestPiece;

	if (closest > 3500)
	{
		SetTrainingSpeechData(292, 150, 0);

		if (gOffPathTimer == 0)
		{
			gTrainingOffPath++;
			gOffPathTimer = 30;
		}
	}
}

// [0x4e92fc] [D] [A]
// Draws a texture unscaled at x y of the 640x480 screen in colour r g b.
// NOTE: [A] wider than 4:3 the HUD keeps its proportions, see wide_aspect
void Draw2DTexturedQuadTC(float x, float y, TEX_INFO *tex, u_char r, u_char g, u_char b)
{
	u_char colour[4];
	float width;
	float height;
	float scale;

	colour[0] = r;
	colour[1] = g;
	colour[2] = b;

	x = x / 640.0f;
	y = y / 480.0f;
	x = (x - 0.5f) * screen_scale / wide_aspect + 0.5f;
	y = (y - 0.5f) * screen_scale + 0.5f;

	width = (float)display_width;
	x = x * width;
	height = (float)display_height;
	y = y * height;

	scale = HUD_WIDTH / 640.0f * screen_scale;

	DrawTexturedPoly(x, y, scale, scale, (float)tex->u, (float)tex->v, (float)tex->w, (float)tex->h, tex->tset, colour);
}

// [0x4e9418] [D] [A]
// Draws a texture at x y of the 640x480 screen, scaled by xscale yscale, in colour 0xRRGGBB.
// Used by the replay director icons (DrawDirectorIcon).
// NOTE: [A] wider than 4:3 the HUD keeps its proportions, see wide_aspect
void DrawScaledTexturedQuadSize(float x, float y, float xscale, float yscale, TEX_INFO *tex, int rgb)
{
	u_char colour[4];
	float width;
	float height;

	colour[0] = rgb >> 16;
	colour[1] = rgb >> 8;
	colour[2] = rgb;

	x = x / 640.0f;
	y = y / 480.0f;
	x = (x - 0.5f) * screen_scale / wide_aspect + 0.5f;
	y = (y - 0.5f) * screen_scale + 0.5f;

	width = (float)display_width;
	x = x * width;
	height = (float)display_height;
	y = y * height;

	xscale = xscale * screen_scale * (HUD_WIDTH / 640.0f);
	yscale = yscale * screen_scale * (HUD_WIDTH / 640.0f);

	DrawTexturedPoly(x, y, xscale, yscale, (float)tex->u, (float)tex->v, (float)tex->w, (float)tex->h, tex->tset, colour);
}

// [0x4e9570] [D]
// Copies the image of texture name over tex (same size) and reloads its texture set.
void sub_4E9570(TEX_INFO *tex, char *name)
{
	int dst_h;
	int dst_w;
	int h;
	int w;
	char *dst_data;
	char *dst;
	char *src_data;
	char *src;
	int i;
	TEX_INFO *src_tex;

	src_tex = GetTextureDetails(name);

	if (!src_tex)
		return;

	w = src_tex->w ? src_tex->w : 256;
	h = src_tex->h ? src_tex->h : 256;
	dst_w = tex->w ? tex->w : 256;
	dst_h = tex->h ? tex->h : 256;

	if (w == dst_w && h == dst_h)
	{
		src_data = (char *)TextureSlots[src_tex->tset].data;
		dst_data = (char *)TextureSlots[tex->tset].data;

		if (src_data && dst_data)
		{
			// 256 x 256 16 bit texture sets
			src = &src_data[512 * src_tex->v + 2 * src_tex->u];
			dst = &dst_data[512 * tex->v + 2 * tex->u];

			for (i = 0; i < h; i++)
			{
				memcpy(dst, src, 2 * w);
				src += 512;
				dst += 512;
			}

			ReplaceTextureSet(tex->tset, dst_data);
		}
	}
}

// [0x4e9733] [D] [A]
// Draws width / max_width of a texture from the left at x y of the 640x480 screen.
// NOTE: [A] wider than 4:3 the HUD keeps its proportions, see wide_aspect; x is centred as in
// Draw2DTexturedQuadTC, not moved to an edge by GetScaledXCoord, the callers move whole items
void DrawTexturedQuadEx(TEX_INFO *tex, float x, float y, int width, int max_width)
{
	u_char colour[4];
	float frac;
	float scale;

	colour[0] = 255;
	colour[1] = 255;
	colour[2] = 255;

	GetScaledXYCoords(&x, &y);

	frac = (float)width / (float)max_width;
	scale = screen_scale * HUD_WIDTH / 640.0f;

	if (gPreviousAlpha != 1)
	{
		grAlphaCombine(GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_ONE, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_TEXTURE, FXTRUE);
		grAlphaTestFunction(GR_CMP_GREATER);
		gPreviousAlpha = 1;
	}

	DrawTexturedPoly(x, y, scale, scale * 1.1f, (float)tex->u, (float)tex->v, (float)tex->w * frac, (float)tex->h, tex->tset, colour);

	if (gPreviousAlpha != 0)
	{
		grAlphaCombine(GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_ONE, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_CONSTANT, FXTRUE);
		grAlphaTestFunction(GR_CMP_ALWAYS);
		gPreviousAlpha = 0;
	}
}

// [0x4e9875] [D] [A]
// Draws a texture at x y of the 640x480 screen, scaled by xscale yscale.
// NOTE: IDA types x and y int; they are passed and read as floats
// NOTE: [A] wider than 4:3 the HUD keeps its proportions, see wide_aspect; x is centred as in
// Draw2DTexturedQuadTC, not moved to an edge by GetScaledXCoord, the callers move whole items
void DrawScaledTexturedPoly(TEX_INFO *tex, float x, float y, float xscale, float yscale)
{
	u_char colour[4];
	float scale;

	colour[0] = 255;
	colour[1] = 255;
	colour[2] = 255;

	GetScaledXYCoords(&x, &y);

	scale = screen_scale * HUD_WIDTH / 640.0f;
	xscale = xscale * scale;
	yscale = yscale * scale;

	if (gPreviousAlpha != 1)
	{
		grAlphaCombine(GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_ONE, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_TEXTURE, FXTRUE);
		grAlphaTestFunction(GR_CMP_GREATER);
		gPreviousAlpha = 1;
	}

	DrawTexturedPoly(x, y, xscale, yscale, (float)tex->u, (float)tex->v, (float)tex->w, (float)tex->h, tex->tset, colour);

	if (gPreviousAlpha != 0)
	{
		grAlphaCombine(GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_ONE, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_CONSTANT, FXTRUE);
		grAlphaTestFunction(GR_CMP_ALWAYS);
		gPreviousAlpha = 0;
	}
}
