/*
 * eax/psxmusic.h - [A] the music of the PSX version in the game, eax/psxmusic.c
 */

#ifndef EAX_PSXMUSIC_H
#define EAX_PSXMUSIC_H

#define PSXMUSIC_RATE		44100	// the rate of pitch 4096 on the SPU

int PSXMusic_Init(void);
void PSXMusic_Shutdown(void);
int PSXMusic_Load(int song);
void PSXMusic_Tick(int tick, float volume);

void FunkUpDaBGMTunez(int funk);

// the SPU of eax/xmplay.c
void XMSpu_InitVoice(int channel);
void XMSpu_SetVoice(int channel, int addr, int pitch, int lvol, int rvol);
void XMSpu_SetVoiceVolume(int channel, int lvol, int rvol);
void XMSpu_SetVoicePitch(int channel, int pitch);
void XMSpu_SetKey(int on, unsigned int voices);
void XMSpu_GetAllKeysStatus(char *status);

#endif // EAX_PSXMUSIC_H
