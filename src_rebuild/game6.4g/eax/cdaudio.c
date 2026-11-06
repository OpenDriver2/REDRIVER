/*
 * eax/cdaudio.c - music. The original plays the audio tracks of the CD through MCI; [A] here they are
 * the files Music\TrackNN.ogg or .wav, streamed by eax/alstream.cpp
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "eax\cdaudio.c", 0x48bf90 - 0x48c42f; the bss block
 * is marked "Sound\cdaudio.c")
 */

#include <stdio.h>
#include <string.h>

#include "dr_types.h"
#include "debug.h"
#include "shell.h"
#include "dr_time.h"
#include "eax/alstream.h"
#include "eax/cdaudio.h"

#define CD_TIMER_PERIOD		10000	// ms
#define MAX_CD_TRACKS		100		// the numbers in the file names have two digits

int GetCDTOC(void);
void StopCDTrack(void);
void CDAudioTimer(void);

//------------------------------------------------------------------------------------------
// initialised data (0x55335c)

float cd_volume_value = 1.0f;		// NOTE: [A] was the user's CD mixer level, a DWORD
int cdaudio_timer = -1;

//------------------------------------------------------------------------------------------
// uninitialised data (0xb9bb40, start of the file's block per IDA mark)

int gCDAudioInited;
int gCDNumTracks;
int gCDTrack;
int gMusicPlaying;

static const char *cd_track_ext[MAX_CD_TRACKS];		// [A] of the track's file, NULL: there is none

// [A]
static void GetTrackFilename(char *filename, int track, const char *ext)
{
	sprintf(filename, "Music\\Track%02d.%s", track, ext);
}

// [0x48bf90] [D] [A]
// NOTE: [A] no CD drive and no mixer line to find. No music files or no sound device leaves the
// music switched off with no error, as no CD did.
int InitCDAudio(void)
{
	if (!gCDAudioInited)
	{
		if (!GetCDTOC())
		{
			LogQuick("InitCDAudio: No music files found");
			return 0;
		}

		if (!InitMusicStream())
		{
			LogQuick("InitCDAudio: Open music device failed");
			return 0;
		}

		MusicStream_SetVolume(cd_volume_value);

		gCDAudioInited = 1;

		cdaudio_timer = CreateTimer(CD_TIMER_PERIOD, TIME_PERIODIC, CDAudioTimer);
	}

	return 1;
}

// [0x48c152] [D] [A]
// NOTE: [A] there is no CD volume of the user's to put back
void ShutdownCDAudio(void)
{
	if (gCDAudioInited)
	{
		if (cdaudio_timer != -1)
		{
			DestroyTimer(cdaudio_timer);
			cdaudio_timer = -1;
		}

		if (gMusicPlaying)
			StopCDTrack();

		ShutdownMusicStream();
		gCDAudioInited = 0;
	}
}

// [0x48c1d5] [D] [A]
// Plays one track (tracks start at 2: track 1 is data).
// NOTE: [A] a track with no file stays silent. The original switches CD audio off for a bad track and
// saves that in the config. The game asks again every frame while gMusicPlaying is 0, so this has to
// be quiet and cheap, which is why GetCDTOC looks for the files once.
void PlayCDTrack(int track)
{
	char filename[32];

	if (gCDAudioInited == 0)
		return;

	// the stream thread found the last file unreadable
	if (MusicStream_TakeFailure())
	{
		LogQuick("PlayCDTrack: Can't play the music file");
		cd_track_ext[gCDTrack] = NULL;
	}

	if (track < 2 || track > gCDNumTracks || cd_track_ext[track] == NULL)
		return;

	GetTrackFilename(filename, track, cd_track_ext[track]);
	MusicStream_Play(filename);

	gCDTrack = track;
	gMusicPlaying = 1;
}

// [0x48c274] [D] [A]
void StopCDTrack(void)
{
	if (gCDAudioInited)
	{
		MusicStream_Stop();
		gMusicPlaying = 0;
	}
}

// [0x48c2a1] [D] [A]
void PauseCDTrack(void)
{
	if (gCDAudioInited)
	{
		MusicStream_Pause();
		gMusicPlaying = 0;
	}
}

// [0x48c2ce] [D] [A]
// Plays on from where it stopped to the end of the track.
void UnPauseCDTrack(void)
{
	if (gCDAudioInited && gMusicPlaying == 0)
		gMusicPlaying = MusicStream_Resume();
}

// [0x48c318] [D] [A]
// NOTE: [A] kept for InitCDAudio, in case the config is read before it
void SetCDVolume(int volume)
{
	cd_volume_value = max(min(volume, 0xFFFF), 0) / 65535.0f;
	MusicStream_SetVolume(cd_volume_value);
}

// [0x48c38f] [D] [A]
// NOTE: [A] looks for the files of the tracks. Ogg goes first, the user may keep both.
int GetCDTOC(void)
{
	static const char *extensions[2] = { "ogg", "wav" };

	char filename[32];
	FILE *file;
	int track;
	int i;

	gCDNumTracks = 0;

	for (track = 2; track < MAX_CD_TRACKS; track++)
	{
		for (i = 0; i < 2; i++)
		{
			GetTrackFilename(filename, track, extensions[i]);

			file = fopen(filename, "rb");

			if (file)
			{
				fclose(file);

				cd_track_ext[track] = extensions[i];
				gCDNumTracks = track;
				break;
			}
		}
	}

	return gCDNumTracks != 0;
}

// [0x48c3de] [D] [A]
// Every 10 s: notices the end of the track.
void CDAudioTimer(void)
{
	if (gCDAudioInited && gMusicPlaying)
	{
		if (!MusicStream_IsActive())
			gMusicPlaying = 0;
	}
}
