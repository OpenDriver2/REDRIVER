/*
 * eax/alsound.h - globals and functions of eax/alsound.c used by other files
 */

#ifndef EAX_ALSOUND_H
#define EAX_ALSOUND_H

#include <AL/alc.h>

#include "dr_types.h"

// functions
extern int InitOpenALSystem(int device);
extern void OpenAL_CloseDevice(void);
extern void OpenAL_EnumerateSoundCards(void);
extern ALCcontext *OpenAL_GetContext(void);

#endif // EAX_ALSOUND_H
