/*
 * eax/cdaudio.c - CD audio (MCI) music
 *
 * Rebuilt from Driver_sym2_94.i64 (IDA folder "eax\cdaudio.c"; the bss block is marked
 * "Sound\cdaudio.c"); partially translated.
 */

#include <windows.h>
#include <mmsystem.h>
#include <string.h>

#include "debug.h"
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
