/*
 * gsound.h - globals and functions of gsound.c used by other files
 */

#ifndef GSOUND_H
#define GSOUND_H

#include "dr_types.h"

// globals
extern int TimeSinceLastSpeech;
// NOTE: no IDA type
extern int gEnableLoseTailSounds;
// most references: gsound.c 4, MScript.c 1
extern int gNumLoseTailSounds;

// functions
extern void AddToSpeechQueue(int id);
extern void ControlBitchingSounds(int pad);
extern int IsSpeechPlaying(void);
extern void PlaySpeech(int id);
extern void PlaySpeechNow(int id1, int id2, int id3, int id4);
extern void ReleaseGameSounds(void);
extern void SoundTasks(void);
extern void StartGameSounds(void);
extern void StopGameSounds(void);
extern void TriggerAlarm(int id);
extern int TriggerThunder(int delay);

#endif // GSOUND_H
