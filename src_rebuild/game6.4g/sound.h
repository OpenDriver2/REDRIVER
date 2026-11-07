/*
 * sound.h - globals and functions of sound.c used by other files
 */

#ifndef SOUND_H
#define SOUND_H

#include "dr_types.h"

// globals
extern SOUND_CALLBACKS SoundCalls;
extern DSOUND_CARD_INFO SoundDevices[];
extern int num_soundcards;
extern int EnableEAX;

// functions
extern int ChangeSample(char *filename, int sample);
extern int ChannelInUse(int channel);
extern void InitSoundDevices(void);
extern int InitSoundSystem(int device, int mode, int useEAX);
extern int LoadSounds(char *filename, unsigned short bank);
extern void LockSoundChannel(int channel);
extern void PauseSounds(void);
extern int Play3DSound(int sample, int channel, float x, float y, float z);
extern void ReallyUpdateSound(void);
extern void ReleaseSample(int sample);
extern void ReleaseSounds(void);
extern void SetEffectsVolume(float volume);
extern void SetListenerPosition(float x, float y, float z);
extern void SetSoundFreq(int channel, int frequency);
extern void SetSoundPos(int channel, float x, float y, float z);
extern void SetSoundSystemDoppler(int enabled);
extern void SetSoundSystemEnvironment(int environment);
extern void SetSoundSystemFormat(int bits, int freq);
extern void SetSoundSystemMode(int mode);
extern void SetSoundVol(int channel, int volume);
extern void SetTrackingSound(int channel, IVECTOR *position);
extern void ShutdownSoundSystem(void);
extern void SimplePlaySound(int sample, int channel, int frequency);
extern void StopSound(int channel);
extern void StopSounds(void);
extern void UnPauseSounds(void);
extern void UnlockAllSoundChannels(void);
extern void UpdateSound(void);

#endif // SOUND_H
