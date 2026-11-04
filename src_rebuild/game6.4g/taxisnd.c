/*
 * taxisnd.c - the taxi passenger's complaints about the player's driving
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "taxisnd.c", 0x527750 - 0x527b8c).
 *
 * HandleTaxiSounds watches the player car (spins, reversing, hard braking, crashes, speed) and
 * ScareTaxi plays a passenger speech sample for it, at most once per 500 frames each and never
 * twice in a row, and fills OpponentBar by its woahh amount.
 */

#include "dr_types.h"
#include "taxisnd.h"
#include "dr_globals.h"
#include "gsound.h"
#include "handling.h"
#include "interview.h"
#include "overlay.h"

// complaints (taxi_sounds indices)
#define TAXI_BRAKE			0	// braked hard from speed
#define TAXI_HANDBRAKE		1
#define TAXI_SPIN180		2	// handbrake turn
#define TAXI_REVERSE		3
#define TAXI_SPIN360		4
#define TAXI_CAR_CRASH		5
#define TAXI_WHEELSPIN		6
#define TAXI_DAMAGE			7
#define TAXI_SPEED			8

#define NUM_TAXI_SOUNDS		9

// first taxi passenger sample of the speech queue
#define TAXI_SPEECH_BASE	0xF000

typedef struct TAXI_SOUND
{
	int sample;
	int frame;		// game_frame it last played
	int woahh;		// added to OpponentBar
} TAXI_SOUND;

void ScareTaxi(int sound);

// initialised data (0x5788d8)
// NOTE: the 24 zero bytes after taxi_sounds (0x578944, no references) are left out: the chunk
// boundary before testmath.c's Arctan_Table is not clear

TAXI_SOUND taxi_sounds[NUM_TAXI_SOUNDS] = {
	{ 0, 0, 10 },
	{ 0, 0, 10 },
	{ 1, 0, 200 },
	{ 1, 0, 200 },
	{ 2, 0, 500 },
	{ 3, 0, 50 },
	{ 3, 0, 10 },
	{ 4, 0, 10 },
	{ 5, 0, 500 },
};

// uninitialised data
// NOTE: in the linker's common area (0xc12e58 - 0xc12e7c, 0xc12f80 - 0xc12f8c); IDA names them
// with a .1 suffix (statics), only these functions use them. last_id is named without it but
// sits among them, so it is taken as a static too.

static int ReverseCount;
static int LastCarDam;
static int TurnCount180;
static int MaxAngVel;
static int TurnCount360;
static int last_id;			// the last complaint played
static int ReachedSpeed;
static int MaxWheelSpeed;
static int BrakeCount;
static int ReverseTest;
static int ReverseFlag;
static int BrakeTest;
static int Doing180;
static int Doing360;

// [0x527750] [D]
void InitTaxiSounds(void)
{
	int i;

	BrakeCount = 0;
	Doing180 = 0;
	Doing360 = 0;
	LastCarDam = 0;
	MaxAngVel = 0;
	MaxWheelSpeed = 0;
	ReachedSpeed = 0;
	ReverseCount = 0;
	ReverseFlag = 0;
	ReverseTest = 0;
	TurnCount180 = 0;
	TurnCount360 = 0;
	gCarCarCollision = 0;
	BrakeTest = 0;
	last_id = -1;

	for (i = 0; i < NUM_TAXI_SOUNDS; i++)
		taxi_sounds[i].frame = 0;
}

// [0x527818] [D]
// NOTE: ReverseTest is never set, so the reversing complaint never plays (original behaviour)
void HandleTaxiSounds(int pad)
{
	int speed;
	LONG angVel;
	int damage;
	int scared;

	speed = car_data[0].wheel_speed >> 12;

	if (speed < 0)
	{
		ReverseFlag = 1;
		speed = -speed;
	}
	else
	{
		ReverseFlag = 0;
	}

	if (MaxWheelSpeed < speed)
		MaxWheelSpeed = speed;

	angVel = car_data[0].st.n.angularVelocity[1] >> 12;

	if (angVel < 0)
		angVel = -angVel;

	if (angVel > MaxAngVel)
		MaxAngVel = angVel;

	damage = GetTotalDamage(&car_data[0]) >> 3;

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
			ScareTaxi(TAXI_SPIN360);
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
			ScareTaxi(TAXI_SPIN180);
	}
	else if (pad & CAR_PAD_HANDBRAKE)
	{
		Doing180 = 1;
		TurnCount180 = 40;
	}

	scared = 0;

	if (gCarCarCollision)
	{
		gCarCarCollision = 0;
		LastCarDam = damage;
		ScareTaxi(TAXI_CAR_CRASH);
	}

	if (damage - LastCarDam > 30)
	{
		scared = 1;
		LastCarDam = damage;
	}

	if (scared)
		ScareTaxi(TAXI_DAMAGE);

	if (angVel > 200 && ReverseTest)
	{
		if (ReverseCount)
			ReverseCount--;
		else
			ScareTaxi(TAXI_REVERSE);
	}
	else
	{
		ReverseTest = 0;
		ReverseCount = 40;
	}

	// braking from speed, 40 frames on
	if (car_data[0].wheel_speed > 696320)
	{
		BrakeTest = 1;
		BrakeCount = 40;
	}
	else if (!(pad & CAR_PAD_BRAKE) || scared)
	{
		BrakeTest = 0;
	}

	if (BrakeTest)
	{
		if (BrakeCount)
			BrakeCount--;
		else
			ScareTaxi(TAXI_BRAKE);
	}

	// fast for more than 5 frames
	if (car_data[0].speed > 270)
		ReachedSpeed++;

	if (scared || car_data[0].speed < 270)
		ReachedSpeed = 0;

	if (ReachedSpeed > 5)
	{
		ScareTaxi(TAXI_SPEED);
		ReachedSpeed = 0;
	}

	if ((pad & CAR_PAD_WHEELSPIN) && speed > 50)
		ScareTaxi(TAXI_WHEELSPIN);

	if ((pad & CAR_PAD_HANDBRAKE) && speed > 50)
		ScareTaxi(TAXI_HANDBRAKE);
}

// [0x527b07] [D]
void ScareTaxi(int sound)
{
	if (last_id != sound && game_frame - taxi_sounds[sound].frame > 500)
	{
		AddToSpeechQueue(taxi_sounds[sound].sample + TAXI_SPEECH_BASE);

		taxi_sounds[sound].frame = game_frame;
		OpponentBar.position += (float)taxi_sounds[sound].woahh;

		last_id = sound;
	}
}
