/*
 * eax/cdaudio.c - CD audio (MCI) music
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "eax\cdaudio.c", 0x48bf90 - 0x48c42f; the bss block
 * is marked "Sound\cdaudio.c")
 */

#include <windows.h>
#include <mmsystem.h>
#include <string.h>

#include "dr_types.h"
#include "debug.h"
#include "shell.h"
#include "dr_time.h"
#include "eax/cdaudio.h"

#define CD_TIMER_PERIOD		10000	// ms

int GetCDTOC(void);
void StopCDTrack(void);
void CDAudioTimer(void);

//------------------------------------------------------------------------------------------
// initialised data (0x55335c)

UINT cd_volume_device = -1;
DWORD cd_volume_value = -1;
int cdaudio_timer = -1;

//------------------------------------------------------------------------------------------
// uninitialised data (0xb9bb40, start of the file's block per IDA mark)

MCI_SET_PARMS gCDSetParams;
static int unknown_B9BB4C;			// NOTE: no references
MCI_OPEN_PARMS gCDOpenParams;
static int unknown_B9BB64;			// NOTE: no references
MCI_PLAY_PARMS gCDPlayParams;
int gCDAudioInited;
static int unknown_B9BB78[2];		// NOTE: no references
MCIDEVICEID gCDDevice;
int gCDPlayFrom;
int gCDPlayTo;
int gCDNumTracks;
int gCDTrack;
int gMusicPlaying;

// [0x48bf90] [D]
int InitCDAudio(void)
{
	int numDevs;
	UINT fallbackDevice;
	int dev;
	AUXCAPS caps;

	fallbackDevice = -1;

	if (!gCDAudioInited)
	{
		cd_volume_device = -1;

		gCDOpenParams.lpstrDeviceType = "cdaudio";

		if (mciSendCommand(0, MCI_OPEN, MCI_OPEN_TYPE, (DWORD)&gCDOpenParams))
		{
			LogQuick("InitCDAudio: Open CD device failed");
			return 0;
		}

		gCDDevice = gCDOpenParams.wDeviceID;

		gCDSetParams.dwTimeFormat = MCI_FORMAT_TMSF;

		if (mciSendCommand(gCDOpenParams.wDeviceID, MCI_SET, MCI_SET_TIME_FORMAT, (DWORD)&gCDSetParams))
		{
			LogQuick("InitCDAudio: Set time format failed");
			mciSendCommand(gCDDevice, MCI_CLOSE, 0, 0);
			return 0;
		}

		if (!GetCDTOC())
		{
			mciSendCommand(gCDDevice, MCI_CLOSE, 0, 0);
			return 0;
		}

		gCDAudioInited = 1;

		// find the mixer line that controls CD volume
		numDevs = auxGetNumDevs();

		for (dev = 0; dev < numDevs; dev++)
		{
			memset(&caps, 0, sizeof(caps));
			auxGetDevCaps(dev, &caps, sizeof(caps));

			if (caps.wTechnology == AUXCAPS_CDAUDIO && (caps.dwSupport & AUXCAPS_VOLUME))
			{
				cd_volume_device = dev;
				break;
			}

			if ((caps.dwSupport & AUXCAPS_LRVOLUME) && fallbackDevice == -1)
				fallbackDevice = dev;
		}

		if (cd_volume_device == -1)
		{
			LogQuick("InitCDAudio: No specific CD volume control found.");
			cd_volume_device = fallbackDevice;
		}

		if (cd_volume_device != -1)
			auxGetVolume(cd_volume_device, &cd_volume_value);

		cdaudio_timer = CreateTimer(CD_TIMER_PERIOD, TIME_PERIODIC, CDAudioTimer);
	}

	return 1;
}

// [0x48c152] [D]
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

		mciSendCommand(gCDDevice, MCI_CLOSE, 0, 0);
		gCDAudioInited = 0;

		// restore the user's CD volume
		if (cd_volume_device != -1)
			auxSetVolume(cd_volume_device, cd_volume_value);

		cd_volume_device = -1;
	}
}

// [0x48c1d5] [D]
// Plays one track (tracks start at 2: track 1 is data); a bad track switches CD audio off.
void PlayCDTrack(int track)
{
	if (gCDAudioInited == 0)
		return;

	if (track >= 2 && track <= gCDNumTracks)
	{
		// TMSF time format: the track is the low byte
		gCDPlayParams.dwFrom = (u_char)track;
		gCDPlayParams.dwTo = (u_char)(track + 1);

		mciSendCommand(gCDDevice, MCI_PLAY, MCI_FROM | MCI_TO, (DWORD_PTR)&gCDPlayParams);

		gCDPlayFrom = gCDPlayParams.dwFrom;
		gCDPlayTo = gCDPlayParams.dwTo;
		gCDTrack = track;
		gMusicPlaying = 1;
	}
	else
	{
		LogQuick("PlayCDTrack: Invalid track number");
		gUseCDAudio = 0;
	}
}

// [0x48c274] [D]
void StopCDTrack(void)
{
	if (gCDAudioInited)
	{
		mciSendCommand(gCDDevice, MCI_STOP, 0, 0);
		gMusicPlaying = 0;
	}
}

// [0x48c2a1] [D]
void PauseCDTrack(void)
{
	if (gCDAudioInited)
	{
		mciSendCommand(gCDDevice, MCI_STOP, 0, 0);
		gMusicPlaying = 0;
	}
}

// [0x48c2ce] [D]
// Plays on from where it stopped to the end of the track.
void UnPauseCDTrack(void)
{
	MCI_PLAY_PARMS params;

	if (gCDAudioInited && gMusicPlaying == 0)
	{
		params.dwTo = gCDPlayTo;
		gMusicPlaying = mciSendCommand(gCDDevice, MCI_PLAY, MCI_TO, (DWORD_PTR)&params) == 0;
	}
}

// [0x48c318] [D]
void SetCDVolume(int volume)
{
	int vol;

	if (cd_volume_device != -1)
	{
		vol = max(min(volume, 0xFFFF), 0);
		auxSetVolume(cd_volume_device, vol | (vol << 16));
	}
}

// [0x48c38f] [D]
int GetCDTOC(void)
{
	MCI_STATUS_PARMS status;

	status.dwItem = MCI_STATUS_NUMBER_OF_TRACKS;

	if (mciSendCommand(gCDDevice, MCI_STATUS, MCI_STATUS_ITEM, (DWORD_PTR)&status))
	{
		LogQuick("GetCDTOC: Query # CD tracks failed");
		return 0;
	}

	gCDNumTracks = status.dwReturn;
	return 1;
}

// [0x48c3de] [D]
// Every 10 s: notices the end of the track.
void CDAudioTimer(void)
{
	MCI_STATUS_PARMS status;

	if (gCDAudioInited && gMusicPlaying)
	{
		status.dwItem = MCI_STATUS_MODE;

		mciSendCommand(gCDDevice, MCI_STATUS, MCI_STATUS_ITEM, (DWORD_PTR)&status);

		if (status.dwReturn != MCI_MODE_PLAY)
			gMusicPlaying = 0;
	}
}
