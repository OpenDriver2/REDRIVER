/*
 * eax/directsound.c - DirectSound 5 back end of the sound system (SOUND_CALLBACKS), 3D buffers,
 * EAX reverb through IKsPropertySet
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "eax\directsound.c", 0x48c560 - 0x48db6d).
 * The COM calls follow the disassembly (Hex-Rays mixes up the vtable slots).
 */

#include <stdio.h>
#include <string.h>

#define DIRECTSOUND_VERSION 0x0500
#include "dr_types.h"
#include <mmsystem.h>
#include <dsound.h>

#include "dr_globals.h"
#include "general.h"
#include "sound.h"
#include "eax/directsound.h"

#define MAX_DS_CHANNELS		20
#define MAX_DS_BUFFERS		80

// NOTE: the EAX 1.0 property sets and structure (eax.h is not part of the SDK)
typedef struct EAX_REVERBPROPERTIES
{
	unsigned long environment;
	float fVolume;
	float fDecayTime_sec;
	float fDamping;
} EAX_REVERBPROPERTIES;

#define DSPROPERTY_EAX_ALL				0
#define DSPROPERTY_EAXBUFFER_REVERBMIX	1

static const GUID DSPROPSETID_EAX_ReverbProperties =
	{ 0x4a4e6fc1, 0xc341, 0x11d1, { 0xb7, 0x3a, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00 } };
static const GUID DSPROPSETID_EAXBUFFER_ReverbProperties =
	{ 0x4a4e6fc0, 0xc341, 0x11d1, { 0xb7, 0x3a, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00 } };
// NOTE: the Aureal / Creative voice manager property set (vmanpset.h), name guessed from the GUID
static const GUID DSPROPSETID_VoiceManager =
	{ 0x62a69bae, 0xdf9d, 0x11d1, { 0x99, 0xa6, 0x00, 0xc0, 0x4f, 0xc9, 0x9d, 0x46 } };

typedef struct SOUND_BUFFER
{
	LPDIRECTSOUNDBUFFER lpDSBuffer;
	LPKSPROPERTYSET lpEAX;
	char filename[128];
} SOUND_BUFFER;

// a playing duplicate of a sample buffer
typedef struct SOUND_BUFFER_CHANNEL
{
	LPDIRECTSOUNDBUFFER lpDSBuffer;
	LPDIRECTSOUND3DBUFFER lpDS3DBuffer;
} SOUND_BUFFER_CHANNEL;

static LPDIRECTSOUNDBUFFER DSCreateWaveSoundBuffer(LPDIRECTSOUND lpDS, void *wave, int software);
static BOOL DSLoadWaveData(void *wave, LPWAVEFORMATEX *ppwfxInfo, void **ppDataOffset, unsigned int *ppDataSize);
static BOOL DSCopySoundBuffer(LPDIRECTSOUNDBUFFER lpDSBuffer, void *pWaveData, DWORD dataSize);
static BOOL DSReadWaveData(void *riff, LPWAVEFORMATEX *ppwfxInfo, void **ppDataOffset, unsigned int *ppDataSize);
void InitDirectSound(void);
void DirectSound_CloseAudio(void);
void DirectSound_StartFrame(void);
void DirectSound_EndFrame(void);
void DirectSound_SetMasterVolume(int volume);
void DirectSound_SetSoundFormat(int bits, int frequency);
void DirectSound_SetSpeakerMode(int mode);
void DirectSound_SetWorldStats(float distanceFactor, float minDistance, float maxDistance, float rolloffFactor, float dopplerFactor);
void DirectSound_SetEnvironment(int environment);
int DirectSound_LoadSoundBuffer(int sample, char *filename);
int DirectSound_DuplicateSample(int sample, int channel);
void DirectSound_ReleaseSample(int sample);
void DirectSound_SetListenerPosition(float x, float y, float z);
void DirectSound_SetListenerOrientation(float frontX, float frontY, float frontZ, float topX, float topY, float topZ);
void DirectSound_SetListenerVelocity(float x, float y, float z);
int DirectSound_PlayChannel(int channel, int looping);
void DirectSound_StopChannel(int channel);
void DirectSound_RestartChannel(int channel);
void DirectSound_SetChannelVolume(int channel, int volume);
void DirectSound_SetChannelFrequency(int channel, int frequency);
void DirectSound_SetChannelPosition(int channel, float x, float y, float z);
void DirectSound_SetChannelVelocity(int channel, float x, float y, float z);
void DirectSound_SetChannelMode(int channel, int mode);
void DirectSound_GetChannelStatus(int channel, void *status);
static LPKSPROPERTYSET EAXCreateInterface(LPDIRECTSOUNDBUFFER lpDSBuffer);
static void EAXSetReverbProperties(LPKSPROPERTYSET lpPropertySet);
static int EnableVoiceManager(void);
static void DisableVoiceManager(void);
static int EAXCreate(void);
static void EAXRelease(void);
static int EAXCreateBuffer(void);
static void EAXFreeBuffer(void);
BOOL CALLBACK DirectSound_EnumerateDevice(LPGUID lpGuid, LPCSTR lpszDesc, LPCSTR lpszDrvName, LPVOID lpContext);

//------------------------------------------------------------------------------------------
// initialised data (0x553460)

static int voice_manager_mode = 2;			// NOTE: dword_553460; DSPROPERTY_VMANAGER_MODE value
static float eax_reverb_mix = -1.0f;		// NOTE: flt_553464; EAX_REVERBMIX_USEDISTANCE
int use_voice_manager = 1;					// NOTE: dword_553468
float ds_min_distance = 100.0f;				// NOTE: flt_55346C; of every 3D buffer (SetWorldStats)
float ds_max_distance = 10000.0f;			// NOTE: flt_553470
static int unknown_553474;					// NOTE: no references

// SetEnvironment presets: EAX environment, volume, decay time, damping
static EAX_REVERBPROPERTIES environments[7] = {
	{ 6, 0.403f, 4.279f, 0.5f },
	{ 16, 0.111f, 2.767f, 0.224f },
	{ 14, 0.25f, 1.752f, 0.776f },
	{ 20, 0.208f, 1.652f, 1.5f },
	{ 22, 1.0f, 1.499f, 0.0f },
	{ 2, 0.417f, 0.4f, 0.666f },
	{ 21, 0.652f, 2.886f, 0.25f },
};

//------------------------------------------------------------------------------------------
// uninitialised data (0xb9bbb0)
// NOTE: unmarked block in the common area; only this file uses it

static SOUND_BUFFER_CHANNEL SoundChannels[MAX_DS_CHANNELS];
static SOUND_BUFFER SoundBuffers[MAX_DS_BUFFERS];
LPDIRECTSOUND lpDirectSound;
LPDIRECTSOUNDBUFFER lpPrimarySoundBuffer;
LPDIRECTSOUND3DLISTENER lpSound3dListener;
LPDIRECTSOUNDBUFFER lpEAXBuffer;			// a dummy buffer to reach the EAX listener properties
LPKSPROPERTYSET lpEAX;
int ds_software_buffers;					// NOTE: dsound.dword_B9E6E4; skip the hardware attempt

// [0x48c560] [D]
// A static 3D buffer with the wave file's data: in hardware if possible (unless software is
// asked for), else in software.
static LPDIRECTSOUNDBUFFER DSCreateWaveSoundBuffer(LPDIRECTSOUND lpDS, void *wave, int software)
{
	void *pWaveData;
	LPDIRECTSOUNDBUFFER lpDSBuffer;
	DSBUFFERDESC dsbd;

	memset(&dsbd, 0, sizeof(dsbd));

	if (DSLoadWaveData(wave, &dsbd.lpwfxFormat, &pWaveData, (unsigned int *)&dsbd.dwBufferBytes) == 0)
		return NULL;

	if (software == 0)
	{
		dsbd.dwSize = sizeof(DSBUFFERDESC);
		dsbd.dwFlags = DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLFREQUENCY | DSBCAPS_CTRL3D | DSBCAPS_STATIC;

		if (IDirectSound_CreateSoundBuffer(lpDS, &dsbd, &lpDSBuffer, NULL) == DS_OK)
		{
			if (DSCopySoundBuffer(lpDSBuffer, pWaveData, dsbd.dwBufferBytes))
				return lpDSBuffer;

			IDirectSoundBuffer_Release(lpDSBuffer);
			lpDSBuffer = NULL;
		}
	}

	dsbd.dwSize = sizeof(DSBUFFERDESC);
	dsbd.dwFlags = DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLFREQUENCY | DSBCAPS_CTRL3D | DSBCAPS_LOCSOFTWARE | DSBCAPS_STATIC;

	if (IDirectSound_CreateSoundBuffer(lpDS, &dsbd, &lpDSBuffer, NULL) == DS_OK)
	{
		if (DSCopySoundBuffer(lpDSBuffer, pWaveData, dsbd.dwBufferBytes))
			return lpDSBuffer;

		IDirectSoundBuffer_Release(lpDSBuffer);
		lpDSBuffer = NULL;
	}

	return NULL;
}

// [0x48c671] [D]
// NOTE: sub_48C671, the name is a guess; not referenced
// Restores a lost buffer and copies the wave data in again.
static BOOL DSReloadSoundBuffer(LPDIRECTSOUNDBUFFER lpDSBuffer, void *wave)
{
	void *pWaveData;
	unsigned int dataSize;

	return DSLoadWaveData(wave, NULL, &pWaveData, &dataSize)
		&& IDirectSoundBuffer_Restore(lpDSBuffer) >= 0
		&& DSCopySoundBuffer(lpDSBuffer, pWaveData, dataSize);
}

// [0x48c6c6] [D]
// NOTE: wave is a file loaded into memory; LockResource returns it unchanged (as in the original,
// taken over from a resource loader)
static BOOL DSLoadWaveData(void *wave, LPWAVEFORMATEX *ppwfxInfo, void **ppDataOffset, unsigned int *ppDataSize)
{
	return DSReadWaveData(LockResource(wave), ppwfxInfo, ppDataOffset, ppDataSize) != 0;
}

// [0x48c708] [D]
static BOOL DSCopySoundBuffer(LPDIRECTSOUNDBUFFER lpDSBuffer, void *pWaveData, DWORD dataSize)
{
	void *pBuffer1;
	DWORD size1;
	void *pBuffer2;
	DWORD size2;

	if (lpDSBuffer == NULL || pWaveData == NULL || dataSize == 0)
		return FALSE;

	if (IDirectSoundBuffer_Lock(lpDSBuffer, 0, dataSize, &pBuffer1, &size1, &pBuffer2, &size2, 0) < 0)
		return FALSE;

	memcpy(pBuffer1, pWaveData, size1);

	if (size2)
		memcpy(pBuffer2, (char *)pWaveData + size1, size2);

	IDirectSoundBuffer_Unlock(lpDSBuffer, pBuffer1, size1, pBuffer2, size2);

	return TRUE;
}

// [0x48c7aa] [D]
// Finds the "fmt " and "data" chunks of a RIFF WAVE file in memory.
static BOOL DSReadWaveData(void *riff, LPWAVEFORMATEX *ppwfxInfo, void **ppDataOffset, unsigned int *ppDataSize)
{
	MMCKINFO *pckInRIFF;
	DWORD *chunk;
	DWORD *end;
	DWORD ident;
	DWORD size;

	pckInRIFF = (MMCKINFO *)riff;

	if (ppwfxInfo)
		*ppwfxInfo = NULL;

	if (ppDataOffset)
		*ppDataOffset = NULL;

	if (ppDataSize)
		*ppDataSize = 0;

	chunk = &pckInRIFF->dwDataOffset;

	if (pckInRIFF->ckid != mmioFOURCC('R', 'I', 'F', 'F'))
		return FALSE;

	if (pckInRIFF->fccType != mmioFOURCC('W', 'A', 'V', 'E'))
		return FALSE;

	end = (DWORD *)((char *)chunk + pckInRIFF->cksize - 4);

	while (chunk < end)
	{
		ident = *chunk++;
		size = *chunk++;

		if (ident == mmioFOURCC('f', 'm', 't', ' '))
		{
			if (ppwfxInfo && *ppwfxInfo == NULL)
			{
				if (size < 14)
					return FALSE;

				*ppwfxInfo = (LPWAVEFORMATEX)chunk;

				if ((ppDataOffset == NULL || *ppDataOffset) && (ppDataSize == NULL || *ppDataSize))
					return TRUE;
			}
		}
		else if (ident == mmioFOURCC('d', 'a', 't', 'a'))
		{
			if ((ppDataOffset && *ppDataOffset == NULL) || (ppDataSize && *ppDataSize == 0))
			{
				if (ppDataOffset)
					*ppDataOffset = chunk;

				if (ppDataSize)
					*ppDataSize = size;

				if (ppwfxInfo == NULL || *ppwfxInfo)
					return TRUE;
			}
		}

		// chunks are 16 bit aligned
		chunk = (DWORD *)((char *)chunk + ((size + 1) & ~1));
	}

	return FALSE;
}

// [0x48c93e] [D]
HRESULT EnumerateSoundCards(void)
{
	return DirectSoundEnumerateA(DirectSound_EnumerateDevice, NULL);
}

// [0x48c94f] [D]
// mode (gSoundCompatibility): 0 hardware buffers and voice manager, 1 software buffers and voice
// manager, 2 software buffers only. Opens the device with a 3D listener on the primary buffer.
int InitDirectSoundSystem(int device, int mode, HWND hWnd)
{
	LPGUID guid;
	DSBUFFERDESC dsbd;

	if (device < 0 || device >= num_soundcards)
		return 0;

	guid = SoundDevices[device].field_04;

	switch (mode)
	{
		case 1:
			ds_software_buffers = 1;
			use_voice_manager = 1;
			break;
		case 2:
			ds_software_buffers = 1;
			use_voice_manager = 0;
			break;
		default:
			ds_software_buffers = 0;
			use_voice_manager = 1;
			break;
	}

	if (lpDirectSound == NULL && DirectSoundCreate(guid, &lpDirectSound, NULL) != DS_OK)
	{
		DirectSound_CloseAudio();
		return 0;
	}

	if (IDirectSound_SetCooperativeLevel(lpDirectSound, hWnd, DSSCL_EXCLUSIVE) != DS_OK)
	{
		DirectSound_CloseAudio();
		return 0;
	}

	memset(&dsbd, 0, sizeof(dsbd));
	dsbd.dwSize = sizeof(DSBUFFERDESC);
	dsbd.dwFlags = DSBCAPS_PRIMARYBUFFER | DSBCAPS_CTRL3D;
	dsbd.dwBufferBytes = 0;

	if (IDirectSound_CreateSoundBuffer(lpDirectSound, &dsbd, &lpPrimarySoundBuffer, NULL) != DS_OK)
	{
		DirectSound_CloseAudio();
		return 0;
	}

	if (IDirectSoundBuffer_QueryInterface(lpPrimarySoundBuffer, &IID_IDirectSound3DListener, (LPVOID *)&lpSound3dListener) != DS_OK)
	{
		DirectSound_CloseAudio();
		return 0;
	}

	if (IDirectSoundBuffer_Play(lpPrimarySoundBuffer, 0, 0, DSBPLAY_LOOPING) != DS_OK)
	{
		DirectSound_CloseAudio();
		return 0;
	}

	if (EAXCreate() == 0)
		EAXRelease();

	if (use_voice_manager)
		EnableVoiceManager();

	InitDirectSound();

	return 1;
}

// [0x48cafc] [D]
void InitDirectSound(void)
{
	SoundCalls.CloseAudio = DirectSound_CloseAudio;
	SoundCalls.StartFrame = DirectSound_StartFrame;
	SoundCalls.EndFrame = DirectSound_EndFrame;
	SoundCalls.SetMasterVolume = DirectSound_SetMasterVolume;
	SoundCalls.SetSoundFormat = DirectSound_SetSoundFormat;
	SoundCalls.SetSpeakerMode = DirectSound_SetSpeakerMode;
	SoundCalls.SetWorldStats = DirectSound_SetWorldStats;
	SoundCalls.SetEnvironment = DirectSound_SetEnvironment;
	SoundCalls.LoadSoundBuffer = DirectSound_LoadSoundBuffer;
	SoundCalls.DuplicateSample = DirectSound_DuplicateSample;
	SoundCalls.ReleaseSample = DirectSound_ReleaseSample;
	SoundCalls.SetListenerPosition = DirectSound_SetListenerPosition;
	SoundCalls.SetListenerOrientation = DirectSound_SetListenerOrientation;
	SoundCalls.SetListenerVelocity = DirectSound_SetListenerVelocity;
	SoundCalls.PlayChannel = DirectSound_PlayChannel;
	SoundCalls.StopChannel = DirectSound_StopChannel;
	SoundCalls.RestartChannel = DirectSound_RestartChannel;
	SoundCalls.SetChannelVolume = DirectSound_SetChannelVolume;
	SoundCalls.SetChannelFrequency = DirectSound_SetChannelFrequency;
	SoundCalls.SetChannelPosition = DirectSound_SetChannelPosition;
	SoundCalls.SetChannelVelocity = DirectSound_SetChannelVelocity;
	SoundCalls.SetChannelMode = DirectSound_SetChannelMode;
	SoundCalls.GetChannelStatus = DirectSound_GetChannelStatus;
}

// [0x48cbe7] [D]
void DirectSound_CloseAudio(void)
{
	DisableVoiceManager();
	EAXRelease();

	if (lpSound3dListener)
	{
		IDirectSound3DListener_Release(lpSound3dListener);
		lpSound3dListener = NULL;
	}

	if (lpPrimarySoundBuffer)
	{
		IDirectSoundBuffer_Release(lpPrimarySoundBuffer);
		lpPrimarySoundBuffer = NULL;
	}

	if (lpDirectSound)
	{
		IDirectSound_Release(lpDirectSound);
		lpDirectSound = NULL;
	}
}

// [0x48cc62] [D]
void DirectSound_StartFrame(void)
{
}

// [0x48cc67] [D]
// The listener and channel changes of the frame are deferred and applied here.
void DirectSound_EndFrame(void)
{
	if (lpSound3dListener)
		IDirectSound3DListener_CommitDeferredSettings(lpSound3dListener);
}

// [0x48cc8c] [D]
// The wave out volume, 0 - 0xffff on both sides.
void DirectSound_SetMasterVolume(int volume)
{
	int clamped;

	clamped = volume <= 0xffff ? volume : 0xffff;

	if (clamped < 0)
		clamped = 0;

	waveOutSetVolume(0, clamped | (clamped << 16));
}

// [0x48ccf3] [D]
// The primary buffer's format: stereo PCM with the given bits and frequency.
void DirectSound_SetSoundFormat(int bits, int frequency)
{
	WAVEFORMATEX wfx;

	if (lpPrimarySoundBuffer == NULL)
		return;

	memset(&wfx, 0, sizeof(wfx));
	wfx.wFormatTag = WAVE_FORMAT_PCM;
	wfx.nChannels = 2;
	wfx.nSamplesPerSec = frequency;
	wfx.nAvgBytesPerSec = frequency * wfx.nChannels * (bits / 8);
	wfx.nBlockAlign = (bits * wfx.nChannels) / 8;
	wfx.wBitsPerSample = bits;
	wfx.cbSize = 0;

	IDirectSoundBuffer_SetFormat(lpPrimarySoundBuffer, &wfx);
}

// [0x48cd8f] [D]
// mode (gAudioSpeakerMode): 0 stereo, 1 quad, 2 mono, 3 headphones, 4 surround.
void DirectSound_SetSpeakerMode(int mode)
{
	DWORD config;

	if (lpDirectSound == NULL)
		return;

	switch (mode)
	{
		case 1:
			config = DSSPEAKER_QUAD;
			break;
		case 2:
			config = DSSPEAKER_MONO;
			break;
		case 3:
			config = DSSPEAKER_HEADPHONE;
			break;
		case 4:
			config = DSSPEAKER_SURROUND;
			break;
		default:
			config = DSSPEAKER_STEREO;
			break;
	}

	IDirectSound_SetSpeakerConfig(lpDirectSound, config);
}

// [0x48ce15] [D]
void DirectSound_SetWorldStats(float distanceFactor, float minDistance, float maxDistance, float rolloffFactor, float dopplerFactor)
{
	if (lpSound3dListener == NULL)
		return;

	ds_min_distance = minDistance;
	ds_max_distance = maxDistance;

	IDirectSound3DListener_SetDistanceFactor(lpSound3dListener, distanceFactor, DS3D_IMMEDIATE);
	IDirectSound3DListener_SetDopplerFactor(lpSound3dListener, dopplerFactor, DS3D_IMMEDIATE);
	IDirectSound3DListener_SetRolloffFactor(lpSound3dListener, rolloffFactor, DS3D_IMMEDIATE);
}

// [0x48ce86] [D]
void DirectSound_SetEnvironment(int environment)
{
	if (EnableEAX && lpEAX)
	{
		IKsPropertySet_Set(lpEAX, &DSPROPSETID_EAX_ReverbProperties, DSPROPERTY_EAX_ALL, NULL, 0,
						   &environments[environment], sizeof(EAX_REVERBPROPERTIES));
	}
}

// [0x48cece] [D]
// Loads a wave file into the sample's buffer (with an EAX interface when EAX is on).
// Returns the frequency, -1 on failure.
// NOTE: the format is read from file offset 24, 4 bytes after where the canonical "fmt " data
// starts, so the value returned is nAvgBytesPerSec (= the frequency for 8 bit mono), as in the
// original
int DirectSound_LoadSoundBuffer(int sample, char *filename)
{
	WAVEFORMATEX wfxInfo;
	FILE *f;
	size_t size;
	void *buffer;

	if (lpDirectSound == NULL)
		return -1;

	f = fopen(filename, "rb");

	if (f == NULL)
		return -1;

	fseek(f, 0, SEEK_END);
	size = ftell(f);

	if (size == (size_t)-1)
	{
		fclose(f);
		return -1;
	}

	fseek(f, 24, SEEK_SET);
	fread(&wfxInfo, 1, sizeof(WAVEFORMATEX), f);
	fseek(f, 0, SEEK_SET);

	buffer = DebugMalloc(size, "D:\\Driver\\game\\eax\\directsound.c", 499);

	if (buffer == NULL)
	{
		fclose(f);
		return -1;
	}

	fread(buffer, 1, size, f);
	fclose(f);

	SoundBuffers[sample].lpDSBuffer = DSCreateWaveSoundBuffer(lpDirectSound, buffer, ds_software_buffers);

	if (SoundBuffers[sample].lpDSBuffer == NULL)
	{
		DebugFree(buffer);
		return -1;
	}

	DebugFree(buffer);

	if (strlen(filename) < 128)
		strcpy(SoundBuffers[sample].filename, filename);
	else
		SoundBuffers[sample].filename[0] = 0;

	if (EnableEAX)
	{
		SoundBuffers[sample].lpEAX = EAXCreateInterface(SoundBuffers[sample].lpDSBuffer);

		if (SoundBuffers[sample].lpEAX)
			EAXSetReverbProperties(SoundBuffers[sample].lpEAX);
	}

	return wfxInfo.nSamplesPerSec;
}

// [0x48d0c6] [D]
// The channel plays a duplicate of the sample's buffer (a lost buffer is loaded again), with its
// own 3D buffer.
int DirectSound_DuplicateSample(int sample, int channel)
{
	HRESULT hr;

	if (SoundBuffers[sample].lpDSBuffer == NULL)
		return 0;

	hr = IDirectSound_DuplicateSoundBuffer(lpDirectSound, SoundBuffers[sample].lpDSBuffer, &SoundChannels[channel].lpDSBuffer);

	if (hr == DSERR_BUFFERLOST)
	{
		DirectSound_ReleaseSample(sample);

		// NOTE: LoadSoundBuffer returns -1 on failure, so this test never fails (as in the original)
		if (DirectSound_LoadSoundBuffer(sample, SoundBuffers[sample].filename) == 0)
		{
			DirectSound_StopChannel(channel);
			return 0;
		}

		hr = IDirectSound_DuplicateSoundBuffer(lpDirectSound, SoundBuffers[sample].lpDSBuffer, &SoundChannels[channel].lpDSBuffer);
	}

	if (hr != DS_OK)
	{
		DirectSound_StopChannel(channel);
		return 0;
	}

	IDirectSoundBuffer_SetCurrentPosition(SoundChannels[channel].lpDSBuffer, 0);

	if (IDirectSoundBuffer_QueryInterface(SoundChannels[channel].lpDSBuffer, &IID_IDirectSound3DBuffer, (LPVOID *)&SoundChannels[channel].lpDS3DBuffer) != DS_OK)
	{
		DirectSound_StopChannel(channel);
		return 0;
	}

	IDirectSound3DBuffer_SetMinDistance(SoundChannels[channel].lpDS3DBuffer, ds_min_distance, DS3D_IMMEDIATE);
	IDirectSound3DBuffer_SetMaxDistance(SoundChannels[channel].lpDS3DBuffer, ds_max_distance, DS3D_IMMEDIATE);

	return 1;
}

// [0x48d257] [D]
void DirectSound_ReleaseSample(int sample)
{
	if (SoundBuffers[sample].lpEAX)
	{
		IKsPropertySet_Release(SoundBuffers[sample].lpEAX);
		SoundBuffers[sample].lpEAX = NULL;
	}

	if (SoundBuffers[sample].lpDSBuffer)
	{
		IDirectSoundBuffer_Release(SoundBuffers[sample].lpDSBuffer);
		SoundBuffers[sample].lpDSBuffer = NULL;
	}
}

// [0x48d2ee] [D]
void DirectSound_SetListenerPosition(float x, float y, float z)
{
	if (lpSound3dListener)
		IDirectSound3DListener_SetPosition(lpSound3dListener, x, y, z, DS3D_DEFERRED);
}

// [0x48d321] [D]
void DirectSound_SetListenerOrientation(float frontX, float frontY, float frontZ, float topX, float topY, float topZ)
{
	if (lpSound3dListener)
		IDirectSound3DListener_SetOrientation(lpSound3dListener, frontX, frontY, frontZ, topX, topY, topZ, DS3D_DEFERRED);
}

// [0x48d360] [D]
// The velocity is given per frame: * 25 makes it per second.
void DirectSound_SetListenerVelocity(float x, float y, float z)
{
	if (lpSound3dListener == NULL)
		return;

	x *= 25.0f;
	y *= 25.0f;
	z *= 25.0f;

	IDirectSound3DListener_SetVelocity(lpSound3dListener, x, y, z, DS3D_DEFERRED);
}

// [0x48d3b7] [D]
int DirectSound_PlayChannel(int channel, int looping)
{
	return SoundChannels[channel].lpDSBuffer
		&& IDirectSoundBuffer_Play(SoundChannels[channel].lpDSBuffer, 0, 0, looping) == DS_OK;
}

// [0x48d404] [D]
void DirectSound_StopChannel(int channel)
{
	if (SoundChannels[channel].lpDSBuffer == NULL)
		return;

	IDirectSoundBuffer_Stop(SoundChannels[channel].lpDSBuffer);

	if (SoundChannels[channel].lpDS3DBuffer)
	{
		IDirectSound3DBuffer_Release(SoundChannels[channel].lpDS3DBuffer);
		SoundChannels[channel].lpDS3DBuffer = NULL;
	}

	IDirectSoundBuffer_Release(SoundChannels[channel].lpDSBuffer);
	SoundChannels[channel].lpDSBuffer = NULL;
}

// [0x48d49d] [D]
void DirectSound_RestartChannel(int channel)
{
	if (SoundChannels[channel].lpDSBuffer == NULL)
		return;

	IDirectSoundBuffer_Stop(SoundChannels[channel].lpDSBuffer);
	IDirectSoundBuffer_SetCurrentPosition(SoundChannels[channel].lpDSBuffer, 0);
	IDirectSoundBuffer_SetVolume(SoundChannels[channel].lpDSBuffer, 0);

	if (SoundChannels[channel].lpDS3DBuffer)
		IDirectSound3DBuffer_SetVelocity(SoundChannels[channel].lpDS3DBuffer, 0.0f, 0.0f, 0.0f, DS3D_IMMEDIATE);
}

// [0x48d543] [D]
void DirectSound_SetChannelVolume(int channel, int volume)
{
	if (SoundChannels[channel].lpDSBuffer)
		IDirectSoundBuffer_SetVolume(SoundChannels[channel].lpDSBuffer, volume);
}

// [0x48d579] [D]
void DirectSound_SetChannelFrequency(int channel, int frequency)
{
	if (SoundChannels[channel].lpDSBuffer)
		IDirectSoundBuffer_SetFrequency(SoundChannels[channel].lpDSBuffer, frequency);
}

// [0x48d5af] [D]
void DirectSound_SetChannelPosition(int channel, float x, float y, float z)
{
	if (SoundChannels[channel].lpDS3DBuffer)
		IDirectSound3DBuffer_SetPosition(SoundChannels[channel].lpDS3DBuffer, x, y, z, DS3D_DEFERRED);
}

// [0x48d5ef] [D]
// The velocity is given per frame: * 25 makes it per second.
void DirectSound_SetChannelVelocity(int channel, float x, float y, float z)
{
	if (SoundChannels[channel].lpDS3DBuffer == NULL)
		return;

	x *= 25.0f;
	y *= 25.0f;
	z *= 25.0f;

	IDirectSound3DBuffer_SetVelocity(SoundChannels[channel].lpDS3DBuffer, x, y, z, DS3D_DEFERRED);
}

// [0x48d653] [D]
// mode: DS3DMODE_NORMAL, DS3DMODE_HEADRELATIVE or DS3DMODE_DISABLE
void DirectSound_SetChannelMode(int channel, int mode)
{
	if (SoundChannels[channel].lpDS3DBuffer)
		IDirectSound3DBuffer_SetMode(SoundChannels[channel].lpDS3DBuffer, mode, DS3D_IMMEDIATE);
}

// [0x48d68b] [D]
void DirectSound_GetChannelStatus(int channel, void *status)
{
	if (SoundChannels[channel].lpDSBuffer)
		IDirectSoundBuffer_GetStatus(SoundChannels[channel].lpDSBuffer, (LPDWORD)status);
}

// [0x48d6c1] [D]
// The buffer's property set if it supports both the EAX listener and buffer reverb properties.
static LPKSPROPERTYSET EAXCreateInterface(LPDIRECTSOUNDBUFFER lpDSBuffer)
{
	LPKSPROPERTYSET lpPropertySet;
	HRESULT hr;
	ULONG support;

	support = 0;

	hr = IDirectSoundBuffer_QueryInterface(lpDSBuffer, &IID_IKsPropertySet, (LPVOID *)&lpPropertySet);

	if (hr != DS_OK)
		return NULL;

	hr = IKsPropertySet_QuerySupport(lpPropertySet, &DSPROPSETID_EAX_ReverbProperties, DSPROPERTY_EAX_ALL, &support);

	if (hr != DS_OK || (support & (KSPROPERTY_SUPPORT_GET | KSPROPERTY_SUPPORT_SET)) != (KSPROPERTY_SUPPORT_GET | KSPROPERTY_SUPPORT_SET))
	{
		IKsPropertySet_Release(lpPropertySet);
		return NULL;
	}

	hr = IKsPropertySet_QuerySupport(lpPropertySet, &DSPROPSETID_EAXBUFFER_ReverbProperties, DSPROPERTY_EAX_ALL, &support);

	if (hr != DS_OK || (support & (KSPROPERTY_SUPPORT_GET | KSPROPERTY_SUPPORT_SET)) != (KSPROPERTY_SUPPORT_GET | KSPROPERTY_SUPPORT_SET))
	{
		IKsPropertySet_Release(lpPropertySet);
		return NULL;
	}

	return lpPropertySet;
}

// [0x48d78f] [D]
static void EAXSetReverbProperties(LPKSPROPERTYSET lpPropertySet)
{
	if (lpPropertySet)
	{
		IKsPropertySet_Set(lpPropertySet, &DSPROPSETID_EAXBUFFER_ReverbProperties, DSPROPERTY_EAXBUFFER_REVERBMIX, NULL, 0,
						   &eax_reverb_mix, sizeof(float));
	}
}

// [0x48d7be] [D]
// NOTE: sub_48D7BE, the name is a guess
static int EnableVoiceManager(void)
{
	ULONG support;

	support = 0;

	if (lpEAX == NULL)
		return 0;

	if (IKsPropertySet_QuerySupport(lpEAX, &DSPROPSETID_VoiceManager, 0, &support) != DS_OK)
		return 0;

	if ((support & (KSPROPERTY_SUPPORT_GET | KSPROPERTY_SUPPORT_SET)) != (KSPROPERTY_SUPPORT_GET | KSPROPERTY_SUPPORT_SET))
		return 0;

	return IKsPropertySet_Set(lpEAX, &DSPROPSETID_VoiceManager, 0, NULL, 0, &voice_manager_mode, sizeof(int)) == DS_OK;
}

// [0x48d84a] [D]
// NOTE: sub_48D84A, the name is a guess; empty
static void DisableVoiceManager(void)
{
}

// [0x48d84f] [D]
static int EAXCreate(void)
{
	if (EAXCreateBuffer() && IDirectSoundBuffer_QueryInterface(lpEAXBuffer, &IID_IKsPropertySet, (LPVOID *)&lpEAX) == DS_OK)
		return 1;

	EAXFreeBuffer();
	return 0;
}

// [0x48d89a] [D]
static void EAXRelease(void)
{
	if (lpEAX)
	{
		IKsPropertySet_Release(lpEAX);
		lpEAX = NULL;
	}

	EAXFreeBuffer();
}

// [0x48d8c8] [D]
// A small 3D buffer whose property set reaches the EAX listener settings.
static int EAXCreateBuffer(void)
{
	DSBUFFERDESC dsbd;
	WAVEFORMATEX wfx;

	memset(&dsbd, 0, sizeof(dsbd));

	wfx.wFormatTag = WAVE_FORMAT_PCM;
	wfx.nChannels = 1;
	wfx.nSamplesPerSec = 11050;
	wfx.nAvgBytesPerSec = 22100;
	wfx.nBlockAlign = 2;
	wfx.wBitsPerSample = 16;
	wfx.cbSize = 0;

	dsbd.dwSize = sizeof(DSBUFFERDESC);
	dsbd.dwBufferBytes = 32;
	dsbd.dwFlags = DSBCAPS_STATIC | DSBCAPS_CTRL3D;
	dsbd.lpwfxFormat = &wfx;

	if (IDirectSound_CreateSoundBuffer(lpDirectSound, &dsbd, &lpEAXBuffer, NULL) == DS_OK)
		return 1;

	EAXFreeBuffer();
	return 0;
}

// [0x48d96d] [D]
static void EAXFreeBuffer(void)
{
	if (lpEAXBuffer)
	{
		IDirectSoundBuffer_Release(lpEAXBuffer);
		lpEAXBuffer = NULL;
	}
}

// [0x48d996] [D]
// Adds each device that is not an emulated driver to SoundDevices (the primary driver has no
// GUID); names are cut to 31 characters with "...".
BOOL CALLBACK DirectSound_EnumerateDevice(LPGUID lpGuid, LPCSTR lpszDesc, LPCSTR lpszDrvName, LPVOID lpContext)
{
	LPDIRECTSOUND lpDS;
	HRESULT hr;
	DSCAPS caps;

	if (num_soundcards >= 8)
		return FALSE;

	hr = DirectSoundCreate(lpGuid, &lpDS, NULL);

	if (hr != DS_OK)
		return TRUE;

	memset(&caps, 0, sizeof(caps));
	caps.dwSize = sizeof(DSCAPS);

	hr = IDirectSound_GetCaps(lpDS, &caps);

	if (hr != DS_OK)
	{
		IDirectSound_Release(lpDS);
		return TRUE;
	}

	IDirectSound_Release(lpDS);

	if (lpGuid)
	{
		if (caps.dwFlags & DSCAPS_EMULDRIVER)
			return TRUE;

		SoundDevices[num_soundcards].field_08 = *lpGuid;
		SoundDevices[num_soundcards].field_04 = &SoundDevices[num_soundcards].field_08;
	}
	else
	{
		memset(&SoundDevices[num_soundcards].field_08, 0, sizeof(GUID));
		SoundDevices[num_soundcards].field_04 = NULL;
	}

	if ((int)strlen(lpszDesc) <= 31)
	{
		strcpy(SoundDevices[num_soundcards].name, lpszDesc);
	}
	else
	{
		strncat(SoundDevices[num_soundcards].name, lpszDesc, 28);

		SoundDevices[num_soundcards].name[28] = '.';
		SoundDevices[num_soundcards].name[29] = '.';
		SoundDevices[num_soundcards].name[30] = '.';
		SoundDevices[num_soundcards].name[31] = 0;
	}

	num_soundcards++;

	return TRUE;
}
