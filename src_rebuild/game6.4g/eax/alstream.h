/*
 * eax/alstream.h - [A] the music stream of eax/alstream.cpp, used by eax/cdaudio.c
 */

#ifndef EAX_ALSTREAM_H
#define EAX_ALSTREAM_H

#include <AL/alc.h>

#ifdef __cplusplus
extern "C" {
#endif

// Runs on the stream thread with its OpenAL context current, every TICKER_PERIOD while it plays,
// and once with MUSIC_TICK_STOP when it is replaced or stopped. volume: the music volume, 0 - 1.
typedef void (*MusicStreamTicker)(int tick, float volume);

enum EMusicTick
{
	MUSIC_TICK_PLAY,
	MUSIC_TICK_PAUSE,
	MUSIC_TICK_STOP,
};

int InitMusicStream(void);
void ShutdownMusicStream(void);

// All of these only leave a request for the stream thread, which acts on it within UPDATE_PERIOD
void MusicStream_Play(const char *filename);
void MusicStream_PlayTicker(MusicStreamTicker ticker);
void MusicStream_Stop(void);
void MusicStream_Pause(void);
int MusicStream_Resume(void);				// 0: nothing paused, or the track has ended
void MusicStream_SetVolume(float volume);	// 0 - 1, linear

// The context of the OpenAL device (eax/alsound.c), NULL before it is destroyed. Waits until the stream
// thread has moved its source and buffers, unlike the requests above.
void MusicStream_SetContext(ALCcontext *context);

int MusicStream_IsActive(void);				// 0: stopped, or the track has played to its end
int MusicStream_TakeFailure(void);			// 1 once when the last file could not be read

#ifdef __cplusplus
}
#endif

#endif // EAX_ALSTREAM_H
