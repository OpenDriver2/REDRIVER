/*
 * eax/cdaudio.h - globals and functions of eax/cdaudio.c used by other files
 */

#ifndef EAX_CDAUDIO_H
#define EAX_CDAUDIO_H

#include "dr_types.h"

// globals
extern int gMusicPlaying;

// functions
extern int InitCDAudio(void);
extern void PauseCDTrack(void);
extern void PlayGameMusic(int track, int mission);
extern void PlayCDTrack(int track);
extern void SetCDVolume(int volume);
extern void ShutdownCDAudio(void);
extern void StopCDTrack(void);
extern void UnPauseCDTrack(void);

#endif // EAX_CDAUDIO_H
