/*
 * glaunch.c - game launch: system init/exit, game init, main game loop
 *
 * Rebuilt from Driver_sym2_94.i64 (IDA folder "glaunch.c"); partially translated.
 */

#include <windows.h>
#include <objbase.h>

/* input.c */
extern int InitDirectInput(void);
extern void ShutdownDevices(void);

/* pres.c */
extern void InitStringMng(void);
extern void DeinitStringMng(void);

/* sound.c */
extern void InitSoundDevices(void);
extern int InitSoundSystem(int hardware, int compatibility, int useEAX);
extern void ShutdownSoundSystem(void);

/* eax/cdaudio.c */
extern int InitCDAudio(void);
extern void ShutdownCDAudio(void);

/* chairs.c */
extern void ShutdownChairs(void);

extern int gSoundHardware;
extern int gSoundCompatibility;
extern int gUseEAX;

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
