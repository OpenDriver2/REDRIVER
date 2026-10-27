/*
 * glaunch.c - game launch: system init/exit, game init, main game loop
 *
 * Rebuilt from Driver_sym2_94.i64 (IDA folder "glaunch.c"); partially translated.
 */

#include <windows.h>
#include <objbase.h>

#include "shell.h"
#include "chairs.h"
#include "eax/cdaudio.h"
#include "glaunch.h"
#include "input.h"
#include "pres.h"
#include "sound.h"

// [0x4d4fa0] [D]
int MainInit(void)
{
	CoInitialize(NULL);

	InitDirectInput();
	InitStringMng();
	InitSoundDevices();
	InitSoundSystem(gSoundHardware, gSoundCompatibility, gUseEAX);
	InitCDAudio();

	return 1;
}

// [0x4d4fe2] [D]
void MainExit(void)
{
	ShutdownChairs();
	ShutdownDevices();
	ShutdownSoundSystem();
	ShutdownCDAudio();
	DeinitStringMng();

	CoUninitialize();
}
