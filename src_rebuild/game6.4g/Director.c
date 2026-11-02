/*
 * Director.c - the replay director: cameras, waypoints, the replay menu
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "Director.c"); partially translated.
 */

#include "dr_types.h"
#include "Director.h"
#include "dr_globals.h"
#include "ai.h"
#include "denting.h"

//------------------------------------------------------------------------------------------
// uninitialised data

char gAICarDamageTexture[1800];

// [0x4c3802] [D]
// Clears the car model and denting state (denting.c) before InitialiseDenting.
void ResetRoysVariables(void)
{
	ClearMem(gCarUnitDamage, sizeof(gCarUnitDamage));
	ClearMem(gTempCarVertDump, sizeof(gTempCarVertDump));
	ClearMem(gTempCarNormalDump, sizeof(gTempCarNormalDump));
	ClearMem(gTempCarPolyDump, sizeof(gTempCarPolyDump));
	ClearMem(gCarCleanModelPtr, sizeof(gCarCleanModelPtr));
	ClearMem(gCarDamModelPtr, sizeof(gCarDamModelPtr));
	ClearMem(gCarLowModelPtr, sizeof(gCarLowModelPtr));
	ClearMem(gTempCarPolyDumpSize, sizeof(gTempCarPolyDumpSize));
	ClearMem(gZoneVertDamageDump, sizeof(gZoneVertDamageDump));
	ClearMem(gCarMaxDamage, 68 * sizeof(int));
	ClearMem(&gHubcap, sizeof(gHubcap));
	ClearMem(CalcHubcapTime, sizeof(CalcHubcapTime));
	ClearMem(DentQueue, sizeof(DentQueue));
	ClearMem(gAICarDamageTexture, sizeof(gAICarDamageTexture));
	ClearMem(gOldCarType, sizeof(gOldCarType));

	gHubcapTime = 0;
	CalcHubcapNum = 0;
	NumCarsToDent = 0;
}
