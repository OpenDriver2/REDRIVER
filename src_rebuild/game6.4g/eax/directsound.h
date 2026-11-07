/*
 * eax/directsound.h - globals and functions of eax/directsound.c used by other files
 */

#ifndef EAX_DIRECTSOUND_H
#define EAX_DIRECTSOUND_H

#include "dr_types.h"

// functions
extern HRESULT EnumerateSoundCards(void);
extern int InitDirectSoundSystem(int device, int mode, HWND hWnd);

#endif // EAX_DIRECTSOUND_H
