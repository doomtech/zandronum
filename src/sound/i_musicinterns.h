#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define USE_WINDOWS_DWORD
#if defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0400
#undef _WIN32_WINNT
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0400
#endif
#include <windows.h>
#include <mmsystem.h>
#else
#include <SDL.h>
#define FALSE 0
#define TRUE 1
//[BB] fmod.h is not needed here under Windows.
#ifndef NO_SOUND
#include <fmod.h>
#endif
#endif
#include "tempfiles.h"
#include "oplsynth/opl_mus_player.h"
#include "c_cvars.h"
#include "mus2midi.h"
#include "i_sound.h"

void I_InitMusicWin32 ();
void I_ShutdownMusicWin32 ();

extern float relative_volume;

// The base music class. Everything is derived from this --------------------

class MusInfo
{
public:
	MusInfo ();
	virtual ~MusInfo ();
	virtual void MusicVolumeChanged();		// snd_musicvolume changed
	virtual void TimidityVolumeChanged();	// timidity_mastervolume changed
	virtual void Play (bool looping) = 0;
	virtual void Pause () = 0;
	virtual void Resume () = 0;
	virtual void Stop () = 0;
	virtual bool IsPlaying () = 0;
	virtual bool IsMIDI () const = 0;
	virtual bool IsValid () const = 0;
	virtual bool SetPosition (int order);
	virtual void Update();
	virtual FString GetStats();

	enum EState
	{
		STATE_Stopped,
		STATE_Playing,
		STATE_Paused
	} m_Status;
	bool m_Looping;
	bool m_NotStartedYet;	// Song has been created but not yet played
};

#ifdef _WIN32

// A device that provides a WinMM-like MIDI streaming interface -------------

#ifndef _WIN32
struct MIDIHDR
{
	BYTE *lpData;
	DWORD dwBufferLength;
	DWORD dwBytesRecorded;
	MIDIHDR *Next;
};
#endif

class MIDIDevice
{
public:
	MIDIDevice();
	virtual ~MIDIDevice();

	virtual int Open(void (*callback)(unsigned int, void *, DWORD, DWORD), void *userdata) = 0;
	virtual void Close() = 0;
	virtual bool IsOpen() const = 0;
	virtual int GetTechnology() const = 0;
	virtual int SetTempo(int tempo) = 0;
	virtual int SetTimeDiv(int timediv) = 0;
	virtual int StreamOut(MIDIHDR *data) = 0;
	virtual int Resume() = 0;
	virtual void Stop() = 0;
	virtual int PrepareHeader(MIDIHDR *data) = 0;
	virtual int UnprepareHeader(MIDIHDR *data) = 0;
};

// WinMM implementation of a MIDI output device -----------------------------

class WinMIDIDevice : public MIDIDevice
{
public:
	WinMIDIDevice(int dev_id);
	~WinMIDIDevice();
	int Open(void (*callback)(unsigned int, void *, DWORD, DWORD), void *userdata);
	void Close();
	bool IsOpen() const;
	int GetTechnology() const;
	int SetTempo(int tempo);
	int SetTimeDiv(int timediv);
	int StreamOut(MIDIHDR *data);
	int Resume();
	void Stop();
	int PrepareHeader(MIDIHDR *data);
	int UnprepareHeader(MIDIHDR *data);

protected:
	static void CALLBACK CallbackFunc(HMIDIOUT, UINT, DWORD_PTR, DWORD, DWORD);

	HMIDISTRM MidiOut;
	UINT DeviceID;
	DWORD SavedVolume;
	bool VolumeWorks;

	void (*Callback)(unsigned int, void *, DWORD, DWORD);
	void *CallbackData;
};

// OPL implementation of a MIDI output device -------------------------------

class OPLMIDIDevice : public MIDIDevice, OPLmusicBlock
{
public:
	OPLMIDIDevice();
	~OPLMIDIDevice();
	int Open(void (*callback)(unsigned int, void *, DWORD, DWORD), void *userdata);
	void Close();
	bool IsOpen() const;
	int GetTechnology() const;
	int SetTempo(int tempo);
	int SetTimeDiv(int timediv);
	int StreamOut(MIDIHDR *data);
	int Resume();
	void Stop();
	int PrepareHeader(MIDIHDR *data);
	int UnprepareHeader(MIDIHDR *data);

protected:
	void (*Callback)(unsigned int, void *, DWORD, DWORD);
	void *CallbackData;
};

// Base class for streaming MUS and MIDI files ------------------------------

class MIDIStreamer : public MusInfo
{
public:
	MIDIStreamer();
	~MIDIStreamer();

	void MusicVolumeChanged();
	void Play(bool looping);
	void Pause();
	void Resume();
	void Stop();
	bool IsPlaying();
	bool IsMIDI() const;
	bool IsValid() const;
	void Update();

protected:
	static DWORD WINAPI PlayerProc (LPVOID lpParameter);
	static void Callback(UINT uMsg, void *userdata, DWORD dwParam1, DWORD dwParam2);
	DWORD PlayerLoop();
	void OutputVolume (DWORD volume);
	int FillBuffer(int buffer_num, int max_events, DWORD max_time);
	bool ServiceEvent();
	int VolumeControllerChange(int channel, int volume);

	// Virtuals for subclasses to override
	virtual void CheckCaps();
	virtual void DoInitialSetup() = 0;
	virtual void DoRestart() = 0;
	virtual bool CheckDone() = 0;
	virtual DWORD *MakeEvents(DWORD *events, DWORD *max_event_p, DWORD max_time) = 0;

	enum
	{
		MAX_EVENTS = 128
	};

	enum
	{
		SONG_MORE,
		SONG_DONE,
		SONG_ERROR
	};

	MIDIDevice *MIDI;
	HANDLE PlayerThread;
	HANDLE ExitEvent;
	HANDLE BufferDoneEvent;

	DWORD Events[2][MAX_EVENTS*3];
	MIDIHDR Buffer[2];
	int BufferNum;
	int EndQueued;
	bool VolumeChanged;
	bool Restarting;
	bool InitialPlayback;
	DWORD NewVolume;
	int Division;
	int Tempo;
	int InitialTempo;
	BYTE ChannelVolumes[16];
	DWORD Volume;
};

// MUS file played with a MIDI stream ---------------------------------------

class MUSSong2 : public MIDIStreamer
{
public:
	MUSSong2 (FILE *file, char *musiccache, int length);
	~MUSSong2 ();

protected:
	void DoInitialSetup();
	void DoRestart();
	bool CheckDone();
	DWORD *MakeEvents(DWORD *events, DWORD *max_events_p, DWORD max_time);

	MUSHeader *MusHeader;
	BYTE *MusBuffer;
	BYTE LastVelocity[16];
	size_t MusP, MaxMusP;
};

// MIDI file played with a MIDI stream --------------------------------------

class MIDISong2 : public MIDIStreamer
{
public:
	MIDISong2 (FILE *file, char *musiccache, int length);
	~MIDISong2 ();

protected:
	void CheckCaps();
	void DoInitialSetup();
	void DoRestart();
	bool CheckDone();
	DWORD *MakeEvents(DWORD *events, DWORD *max_events_p, DWORD max_time);

	struct TrackInfo;

	void ProcessInitialMetaEvents ();
	DWORD *SendCommand (DWORD *event, TrackInfo *track, DWORD delay);
	TrackInfo *FindNextDue ();
	void SetTempo(int new_tempo);

	BYTE *MusHeader;
	TrackInfo *Tracks;
	TrackInfo *TrackDue;
	int NumTracks;
	int Format;
	WORD DesignationMask;
};

#endif	/* _WIN32 */

// Anything supported by FMOD out of the box --------------------------------

class StreamSong : public MusInfo
{
public:
	StreamSong (const char *file, int offset, int length);
	~StreamSong ();
	void Play (bool looping);
	void Pause ();
	void Resume ();
	void Stop ();
	bool IsPlaying ();
	bool IsMIDI () const { return false; }
	bool IsValid () const { return m_Stream != NULL; }
	bool SetPosition (int order);
	FString GetStats();

protected:
	StreamSong () : m_Stream(NULL), m_LastPos(0) {}

	SoundStream *m_Stream;
	int m_LastPos;
};

#if defined(_WIN32) || !defined(NO_SOUND)
#endif
// MIDI file played with Timidity and possibly streamed through FMOD --------

class TimiditySong : public StreamSong
{
public:
	TimiditySong (FILE *file, char * musiccache, int length);
	~TimiditySong ();
	void Play (bool looping);
	void Stop ();
	bool IsPlaying ();
	bool IsValid () const { return CommandLine.Len() > 0; }
	void TimidityVolumeChanged();

protected:
	void PrepTimidity ();
	bool LaunchTimidity ();

	FTempFileName DiskName;
#ifdef _WIN32
	HANDLE ReadWavePipe;
	HANDLE WriteWavePipe;
	HANDLE KillerEvent;
	HANDLE ChildProcess;
	bool Validated;
	bool ValidateTimidity ();
#else // _WIN32
	int WavePipe[2];
	pid_t ChildProcess;
#endif
	FString CommandLine;
	size_t LoopPos;

	static bool FillStream (SoundStream *stream, void *buff, int len, void *userdata);
#ifdef _WIN32
	static const char EventName[];
#endif
};

// MUS file played by a software OPL2 synth and streamed through FMOD -------

class OPLMUSSong : public StreamSong
{
public:
	OPLMUSSong (FILE *file, char * musiccache, int length);
	~OPLMUSSong ();
	void Play (bool looping);
	bool IsPlaying ();
	bool IsValid () const;
	void ResetChips ();

protected:
	static bool FillStream (SoundStream *stream, void *buff, int len, void *userdata);

	OPLmusicFile *Music;
};

// CD track/disk played through the multimedia system -----------------------

class CDSong : public MusInfo
{
public:
	CDSong (int track, int id);
	~CDSong ();
	void Play (bool looping);
	void Pause ();
	void Resume ();
	void Stop ();
	bool IsPlaying ();
	bool IsMIDI () const { return false; }
	bool IsValid () const { return m_Inited; }

protected:
	CDSong () : m_Inited(false) {}

	int m_Track;
	bool m_Inited;
};

// CD track on a specific disk played through the multimedia system ---------

class CDDAFile : public CDSong
{
public:
	CDDAFile (FILE *file, int length);
};

// --------------------------------------------------------------------------

extern MusInfo *currSong;
extern int		nomusic;

EXTERN_CVAR (Float, snd_musicvolume)
EXTERN_CVAR (Bool, opl_enable)
