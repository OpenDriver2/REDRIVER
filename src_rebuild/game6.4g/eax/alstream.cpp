/*
 * eax/alstream.cpp - [A] streamed music through OpenAL Soft, WAV and Ogg Vorbis, the replacement of
 * the CD drive behind eax/cdaudio.c
 *
 * Not in the original. A track is read a little at a time into a queue of OpenAL buffers on a thread
 * of its own, which matters for more than the frame rate:
 * - the game runs the FPU in single precision with chop rounding (FPUInit) and the decoder must not:
 *   a new thread starts in the default mode, so everything of stb_vorbis stays on that thread,
 *   opening the file too (it builds its tables there);
 * - the music plays in the OpenAL context of the sound system (eax/alsound.c), current on that
 *   thread too, so it goes to the card the sound options name. The device stays open after the
 *   sound system shuts down, for a video too, and closes for another card or for no sound:
 *   MusicStream_SetContext takes the source and buffers out of the old context before it goes and
 *   makes them again in the new one. The decoder
 *   stays open meanwhile, the track goes on where it was but for what was queued. With no sound
 *   device there is no context and no music. The music has no send to the reverb, and the master
 *   gain of the listener is never set (OpenAL_SetMasterVolume has no callers), as the CD was outside
 *   of it.
 *
 * Instead of a file it can run a function on the thread (MusicStream_PlayTicker), the XM player of
 * eax/psxmusic.c, which plays through OpenAL sources of its own in the same context. The ticker is
 * told MUSIC_TICK_STOP before its context goes, and starts its song again in the next one.
 *
 * Requests are levels, not a queue: the game thread writes what it wants and the stream thread
 * compares that to what it is doing, so nothing here can block the game on the disk or the decoder.
 */

#include <stdio.h>
#include <string.h>

#define AL_ALEXT_PROTOTYPES
#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>

#include <SDL.h>

#define STB_VORBIS_HEADER_ONLY
#include "stb/stb_vorbis.c"

#include "eax/alstream.h"

#define UPDATE_PERIOD		15		// ms
#define TICKER_PERIOD		2		// ms, for the timing of the notes of a ticker

// about 1.5 s of CD audio queued, far more than the period above
#define NUM_BUFFERS			8
#define BUFFER_SIZE			32768

#define MAX_FILENAME		260

//------------------------------------------------------------------------------------------
// Sources of PCM data

class IAudioStream
{
public:
	virtual ~IAudioStream() {}

	// Returns the frames read, 0 at the end of the data
	virtual int Read(void *out, int frames) = 0;

	int GetFrameSize() const { return channels * (bits >> 3); }

	int channels;
	int bits;
	int rate;
};

class CWaveStream : public IAudioStream
{
public:
	CWaveStream() : m_file(NULL), m_remaining(0) {}
	~CWaveStream();

	bool Open(const char *filename);
	int Read(void *out, int frames);

private:
	bool ReadChunks();

	FILE *m_file;
	int m_remaining;	// bytes of the data chunk left
};

CWaveStream::~CWaveStream()
{
	if (m_file)
		fclose(m_file);
}

bool CWaveStream::Open(const char *filename)
{
	m_file = fopen(filename, "rb");

	if (m_file == NULL)
		return false;

	return ReadChunks();
}

// Leaves the file at the start of the samples
bool CWaveStream::ReadChunks()
{
	unsigned char header[12];
	unsigned char fmt[40];
	unsigned int length;
	long pos;
	long end;
	int fmtLength;
	int tag;
	bool haveFormat;

	if (fread(header, 12, 1, m_file) != 1 || memcmp(header, "RIFF", 4) != 0 || memcmp(header + 8, "WAVE", 4) != 0)
		return false;

	fseek(m_file, 0, SEEK_END);
	end = ftell(m_file);
	fseek(m_file, 12, SEEK_SET);

	haveFormat = false;

	while (fread(header, 8, 1, m_file) == 1)
	{
		length = header[4] | (header[5] << 8) | (header[6] << 16) | ((unsigned int)header[7] << 24);
		pos = ftell(m_file);

		if (memcmp(header, "fmt ", 4) == 0 && length >= 16)
		{
			fmtLength = length < sizeof(fmt) ? (int)length : (int)sizeof(fmt);

			if (fread(fmt, fmtLength, 1, m_file) != 1)
				return false;

			tag = fmt[0] | (fmt[1] << 8);

			// WAVE_FORMAT_EXTENSIBLE keeps the real tag at the start of its GUID
			if (tag == 0xFFFE && fmtLength >= 26)
				tag = fmt[24] | (fmt[25] << 8);

			channels = fmt[2] | (fmt[3] << 8);
			rate = fmt[4] | (fmt[5] << 8) | (fmt[6] << 16) | (fmt[7] << 24);
			bits = fmt[14] | (fmt[15] << 8);

			if (tag != 1 || channels < 1 || channels > 2 || (bits != 8 && bits != 16) || rate <= 0)
				return false;

			haveFormat = true;
		}
		else if (memcmp(header, "data", 4) == 0)
		{
			if (!haveFormat)
				return false;

			// the size in the header can't be trusted past the end of the file
			if (length > (unsigned int)(end - pos))
				length = (unsigned int)(end - pos);

			m_remaining = (int)length;
			return true;
		}

		// chunks are 16 bit aligned
		if (length > (unsigned int)(end - pos) || fseek(m_file, pos + (long)((length + 1) & ~1u), SEEK_SET) != 0)
			return false;
	}

	return false;
}

int CWaveStream::Read(void *out, int frames)
{
	int frameSize;
	int count;

	frameSize = GetFrameSize();

	if (frames > m_remaining / frameSize)
		frames = m_remaining / frameSize;

	count = (int)fread(out, frameSize, frames, m_file);
	m_remaining -= count * frameSize;

	return count;
}

class COggStream : public IAudioStream
{
public:
	COggStream() : m_vorbis(NULL) {}
	~COggStream();

	bool Open(const char *filename);
	int Read(void *out, int frames);

private:
	stb_vorbis *m_vorbis;
};

COggStream::~COggStream()
{
	if (m_vorbis)
		stb_vorbis_close(m_vorbis);
}

bool COggStream::Open(const char *filename)
{
	stb_vorbis_info info;
	int error;

	m_vorbis = stb_vorbis_open_filename(filename, &error, NULL);

	if (m_vorbis == NULL)
		return false;

	info = stb_vorbis_get_info(m_vorbis);

	channels = info.channels;
	bits = 16;
	rate = (int)info.sample_rate;

	return channels >= 1 && channels <= 2;
}

int COggStream::Read(void *out, int frames)
{
	return stb_vorbis_get_samples_short_interleaved(m_vorbis, channels, (short *)out, frames * channels);
}

// The format goes by the extension. Returns NULL for anything that can't be played.
static IAudioStream *OpenAudioStream(const char *filename)
{
	const char *ext;

	ext = strrchr(filename, '.');

	if (ext && SDL_strcasecmp(ext, ".ogg") == 0)
	{
		COggStream *ogg = new COggStream();

		if (ogg->Open(filename))
			return ogg;

		delete ogg;
	}
	else
	{
		CWaveStream *wave = new CWaveStream();

		if (wave->Open(filename))
			return wave;

		delete wave;
	}

	return NULL;
}

//------------------------------------------------------------------------------------------
// Player

class CStreamPlayer
{
public:
	CStreamPlayer();

	bool Init();
	void Shutdown();

	void Play(const char *filename, MusicStreamTicker ticker);
	void Stop();
	void Pause();
	bool Resume();
	void SetVolume(float volume);
	void SetContext(ALCcontext *context);

	bool IsActive();
	bool TakeFailure();

private:
	enum EState
	{
		STATE_STOPPED,
		STATE_PLAYING,
		STATE_PAUSED,
	};

	static int SDLCALL ThreadFunc(void *data);

	void Run();
	void UpdateContext();
	void AttachContext(ALCcontext *context);
	void DetachContext();
	void Update();
	void OpenStream(const char *filename);
	void CloseStream();
	void StopTicker();
	void FillBuffers();

	void SetEnded(bool failed);

	SDL_Thread *m_thread;
	SDL_mutex *m_mutex;
	SDL_cond *m_contextDone;	// signalled when the stream thread has taken m_wantContext

	// written by the game thread, under m_mutex
	char m_filename[MAX_FILENAME];
	MusicStreamTicker m_ticker;		// instead of the file when not NULL
	int m_serial;			// one up for every Play, so the same track can start again
	EState m_state;
	float m_volume;
	bool m_exit;
	ALCcontext *m_wantContext;
	int m_contextSerial;	// one up for every SetContext

	// written by the stream thread, under m_mutex
	bool m_ended;
	bool m_failed;
	int m_contextDoneSerial;

	// stream thread only
	ALCcontext *m_context;	// current on this thread, NULL: no source and buffers
	IAudioStream *m_stream;
	MusicStreamTicker m_running;	// the ticker that plays
	ALuint m_source;
	ALuint m_buffers[NUM_BUFFERS];
	ALuint m_free[NUM_BUFFERS];
	int m_numFree;
	int m_openSerial;
	float m_gain;
	bool m_paused;
	bool m_eof;
	char m_pcm[BUFFER_SIZE];
};

static CStreamPlayer *player;
static ALCcontext *sound_context;		// the last one given to MusicStream_SetContext

CStreamPlayer::CStreamPlayer()
	: m_thread(NULL), m_mutex(NULL), m_contextDone(NULL), m_ticker(NULL), m_serial(0),
	m_state(STATE_STOPPED), m_volume(1.0f), m_exit(false), m_wantContext(NULL), m_contextSerial(0),
	m_ended(false), m_failed(false), m_contextDoneSerial(0), m_context(NULL), m_stream(NULL),
	m_running(NULL), m_source(0), m_numFree(0), m_openSerial(0), m_gain(-1.0f), m_paused(false),
	m_eof(false)
{
	m_filename[0] = 0;
}

bool CStreamPlayer::Init()
{
	m_wantContext = sound_context;

	m_mutex = SDL_CreateMutex();
	m_contextDone = SDL_CreateCond();

	if (m_mutex && m_contextDone)
		m_thread = SDL_CreateThread(ThreadFunc, "Music", this);

	if (m_thread == NULL)
	{
		Shutdown();
		return false;
	}

	return true;
}

void CStreamPlayer::Shutdown()
{
	if (m_thread)
	{
		SDL_LockMutex(m_mutex);
		m_exit = true;
		SDL_UnlockMutex(m_mutex);

		SDL_WaitThread(m_thread, NULL);
		m_thread = NULL;
	}

	if (m_contextDone)
		SDL_DestroyCond(m_contextDone);

	if (m_mutex)
		SDL_DestroyMutex(m_mutex);

	m_contextDone = NULL;
	m_mutex = NULL;
}

void CStreamPlayer::Play(const char *filename, MusicStreamTicker ticker)
{
	SDL_LockMutex(m_mutex);

	SDL_strlcpy(m_filename, filename, sizeof(m_filename));
	m_ticker = ticker;
	m_serial++;
	m_state = STATE_PLAYING;
	m_ended = false;
	m_failed = false;

	SDL_UnlockMutex(m_mutex);
}

void CStreamPlayer::Stop()
{
	SDL_LockMutex(m_mutex);
	m_state = STATE_STOPPED;
	SDL_UnlockMutex(m_mutex);
}

void CStreamPlayer::Pause()
{
	SDL_LockMutex(m_mutex);

	if (m_state == STATE_PLAYING)
		m_state = STATE_PAUSED;

	SDL_UnlockMutex(m_mutex);
}

bool CStreamPlayer::Resume()
{
	bool resumed;

	SDL_LockMutex(m_mutex);

	resumed = m_state == STATE_PAUSED && !m_ended;

	if (resumed)
		m_state = STATE_PLAYING;

	SDL_UnlockMutex(m_mutex);

	return resumed;
}

void CStreamPlayer::SetVolume(float volume)
{
	SDL_LockMutex(m_mutex);
	m_volume = volume;
	SDL_UnlockMutex(m_mutex);
}

// Waits for the stream thread: the old context may be destroyed as soon as this returns
void CStreamPlayer::SetContext(ALCcontext *context)
{
	int serial;

	SDL_LockMutex(m_mutex);

	m_wantContext = context;
	serial = ++m_contextSerial;

	while (m_contextDoneSerial != serial)
		SDL_CondWait(m_contextDone, m_mutex);

	SDL_UnlockMutex(m_mutex);
}

bool CStreamPlayer::IsActive()
{
	bool active;

	SDL_LockMutex(m_mutex);
	active = m_state != STATE_STOPPED && !m_ended;
	SDL_UnlockMutex(m_mutex);

	return active;
}

bool CStreamPlayer::TakeFailure()
{
	bool failed;

	SDL_LockMutex(m_mutex);
	failed = m_failed;
	m_failed = false;
	SDL_UnlockMutex(m_mutex);

	return failed;
}

int SDLCALL CStreamPlayer::ThreadFunc(void *data)
{
	((CStreamPlayer *)data)->Run();
	return 0;
}

void CStreamPlayer::Run()
{
	bool exiting;

	do
	{
		UpdateContext();
		Update();
		SDL_Delay(m_running ? TICKER_PERIOD : UPDATE_PERIOD);

		SDL_LockMutex(m_mutex);
		exiting = m_exit;
		SDL_UnlockMutex(m_mutex);
	} while (!exiting);

	CloseStream();
	StopTicker();
	DetachContext();
}

void CStreamPlayer::UpdateContext()
{
	ALCcontext *context;
	int serial;

	SDL_LockMutex(m_mutex);
	context = m_wantContext;
	serial = m_contextSerial;
	SDL_UnlockMutex(m_mutex);

	if (context != m_context)
	{
		DetachContext();

		if (context)
			AttachContext(context);
	}

	SDL_LockMutex(m_mutex);

	if (m_contextDoneSerial != serial)
	{
		m_contextDoneSerial = serial;
		SDL_CondBroadcast(m_contextDone);
	}

	SDL_UnlockMutex(m_mutex);
}

void CStreamPlayer::AttachContext(ALCcontext *context)
{
	int i;

	m_context = context;
	alcSetThreadContext(m_context);

	alGenSources(1, &m_source);
	alGenBuffers(NUM_BUFFERS, m_buffers);

	// plays as it is, the speakers get the left and right of the track as they did from the CD
	alSourcei(m_source, AL_SOURCE_RELATIVE, AL_TRUE);
	alSource3f(m_source, AL_POSITION, 0.0f, 0.0f, 0.0f);
	alSourcef(m_source, AL_ROLLOFF_FACTOR, 0.0f);
	alSourcei(m_source, AL_DIRECT_CHANNELS_SOFT, AL_TRUE);

	for (i = 0; i < NUM_BUFFERS; i++)
		m_free[i] = m_buffers[i];

	m_numFree = NUM_BUFFERS;
	m_gain = -1.0f;			// a new source has to be told the volume
	m_paused = false;
}

// The stream stays open, the queued part of it is lost
void CStreamPlayer::DetachContext()
{
	if (m_context == NULL)
		return;

	// its voices go with the context, it stays the one that plays
	if (m_running)
		m_running(MUSIC_TICK_STOP, 0.0f);

	alSourceStop(m_source);
	alSourcei(m_source, AL_BUFFER, 0);

	alDeleteSources(1, &m_source);
	alDeleteBuffers(NUM_BUFFERS, m_buffers);

	alcSetThreadContext(NULL);

	m_source = 0;
	m_numFree = 0;
	m_context = NULL;
}

void CStreamPlayer::Update()
{
	char filename[MAX_FILENAME];
	MusicStreamTicker ticker;
	EState state;
	float volume;
	int serial;

	SDL_LockMutex(m_mutex);

	state = m_state;
	volume = m_volume;
	serial = m_serial;
	ticker = m_ticker;

	if (serial != m_openSerial)
		memcpy(filename, m_filename, sizeof(filename));

	SDL_UnlockMutex(m_mutex);

	if (m_context && volume != m_gain)
	{
		m_gain = volume;
		alSourcef(m_source, AL_GAIN, volume);
	}

	if (serial != m_openSerial)
	{
		m_openSerial = serial;
		CloseStream();
		StopTicker();

		if (state != STATE_STOPPED)
		{
			if (ticker)
				m_running = ticker;
			else
				OpenStream(filename);
		}
	}

	if (state == STATE_STOPPED)
	{
		CloseStream();
		StopTicker();
		return;
	}

	// without a context the ticker waits too, and starts its song again in the next one
	if (m_running)
	{
		if (m_context)
			m_running(state == STATE_PAUSED ? MUSIC_TICK_PAUSE : MUSIC_TICK_PLAY, volume);

		return;
	}

	// without a context the track waits where it is
	if (m_stream == NULL || m_context == NULL)
		return;

	if (state == STATE_PAUSED)
	{
		if (!m_paused)
			alSourcePause(m_source);

		m_paused = true;
		return;
	}

	m_paused = false;

	FillBuffers();
}

void CStreamPlayer::OpenStream(const char *filename)
{
	int i;

	m_stream = OpenAudioStream(filename);

	if (m_stream == NULL)
	{
		SetEnded(true);
		return;
	}

	if (m_context)
	{
		for (i = 0; i < NUM_BUFFERS; i++)
			m_free[i] = m_buffers[i];

		m_numFree = NUM_BUFFERS;
	}

	m_eof = false;
}

void CStreamPlayer::CloseStream()
{
	if (m_stream == NULL)
		return;

	if (m_context)
	{
		alSourceStop(m_source);
		alSourcei(m_source, AL_BUFFER, 0);
	}

	delete m_stream;

	m_stream = NULL;
	m_paused = false;
}

// Without a context its voices are already gone (DetachContext)
void CStreamPlayer::StopTicker()
{
	if (m_running && m_context)
		m_running(MUSIC_TICK_STOP, 0.0f);

	m_running = NULL;
}

// Puts the played buffers back into the queue with new data, and keeps the source going: it stops
// by itself at the end of the queue, and that is the end of the track only when no data is left.
void CStreamPlayer::FillBuffers()
{
	ALuint buffer;
	ALenum format;
	ALint processed;
	ALint queued;
	ALint state;
	int frameSize;
	int frames;

	// taken before the played buffers go: if it isn't playing now, whatever is queued from here on is
	// new. A stopped source can't say so afterwards, it counts all of its queue as processed.
	alGetSourcei(m_source, AL_SOURCE_STATE, &state);
	alGetSourcei(m_source, AL_BUFFERS_PROCESSED, &processed);

	while (processed-- > 0)
	{
		alSourceUnqueueBuffers(m_source, 1, &buffer);
		m_free[m_numFree++] = buffer;
	}

	frameSize = m_stream->GetFrameSize();

	if (m_stream->bits == 16)
		format = m_stream->channels == 2 ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16;
	else
		format = m_stream->channels == 2 ? AL_FORMAT_STEREO8 : AL_FORMAT_MONO8;

	while (m_numFree > 0 && !m_eof)
	{
		frames = m_stream->Read(m_pcm, BUFFER_SIZE / frameSize);

		if (frames <= 0)
		{
			m_eof = true;
			break;
		}

		buffer = m_free[--m_numFree];

		alBufferData(buffer, format, m_pcm, frames * frameSize, m_stream->rate);
		alSourceQueueBuffers(m_source, 1, &buffer);
	}

	if (state != AL_PLAYING)
	{
		alGetSourcei(m_source, AL_BUFFERS_QUEUED, &queued);

		if (queued > 0)
			alSourcePlay(m_source);
		else if (queued == 0 && m_eof)
		{
			CloseStream();
			SetEnded(false);
		}
	}
}

void CStreamPlayer::SetEnded(bool failed)
{
	SDL_LockMutex(m_mutex);
	m_ended = true;
	m_failed = failed;
	SDL_UnlockMutex(m_mutex);
}

//------------------------------------------------------------------------------------------
// The interface for the C code

extern "C" int InitMusicStream(void)
{
	player = new CStreamPlayer();

	if (player->Init())
		return 1;

	delete player;
	player = NULL;

	return 0;
}

extern "C" void ShutdownMusicStream(void)
{
	if (player)
	{
		player->Shutdown();

		delete player;
		player = NULL;
	}
}

extern "C" void MusicStream_Play(const char *filename)
{
	if (player)
		player->Play(filename, NULL);
}

extern "C" void MusicStream_PlayTicker(MusicStreamTicker ticker)
{
	if (player)
		player->Play("", ticker);
}

extern "C" void MusicStream_Stop(void)
{
	if (player)
		player->Stop();
}

extern "C" void MusicStream_Pause(void)
{
	if (player)
		player->Pause();
}

extern "C" int MusicStream_Resume(void)
{
	return player && player->Resume();
}

extern "C" void MusicStream_SetVolume(float volume)
{
	if (player)
		player->SetVolume(volume);
}

extern "C" void MusicStream_SetContext(ALCcontext *context)
{
	sound_context = context;

	if (player)
		player->SetContext(context);
}

extern "C" int MusicStream_IsActive(void)
{
	return player && player->IsActive();
}

extern "C" int MusicStream_TakeFailure(void)
{
	return player && player->TakeFailure();
}
