#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define USE_WINDOWS_DWORD
#if defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0400
#undef _WIN32_WINNT
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0400
#endif
#ifndef USE_WINDOWS_DWORD
#define USE_WINDOWS_DWORD
#endif
#include <windows.h>
#include <mmsystem.h>
#else
#include <SDL.h>
#define FALSE 0
#define TRUE 1
#endif
#include "tempfiles.h"
#include "oplsynth/opl_mus_player.h"
#include "c_cvars.h"
#include "mus2midi.h"
#include "i_sound.h"
#include "i_music.h"

void I_InitMusicWin32 ();
void I_ShutdownMusicWin32 ();

extern float relative_volume;

EXTERN_CVAR (Float, timidity_mastervolume)


// A device that provides a WinMM-like MIDI streaming interface -------------

#ifndef _WIN32
struct MIDIHDR
{
	BYTE *lpData;
	DWORD dwBufferLength;
	DWORD dwBytesRecorded;
	MIDIHDR *lpNext;
};

enum
{
	MOD_MIDIPORT = 1,
	MOD_SYNTH,
	MOD_SQSYNTH,
	MOD_FMSYNTH,
	MOD_MAPPER,
	MOD_WAVETABLE,
	MOD_SWSYNTH
};

typedef BYTE *LPSTR;

#define MEVT_TEMPO			((BYTE)1)
#define MEVT_NOP			((BYTE)2)
#define MEVT_LONGMSG		((BYTE)128)

#define MEVT_EVENTTYPE(x)	((BYTE)((x) >> 24))
#define MEVT_EVENTPARM(x)   ((x) & 0xffffff)

#define MOM_DONE			969
#else
// w32api does not define these
#ifndef MOD_WAVETABLE
#define MOD_WAVETABLE   6
#define MOD_SWSYNTH     7
#endif
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
	virtual int StreamOutSync(MIDIHDR *data) = 0;
	virtual int Resume() = 0;
	virtual void Stop() = 0;
	virtual int PrepareHeader(MIDIHDR *data) = 0;
	virtual int UnprepareHeader(MIDIHDR *data) = 0;
	virtual bool FakeVolume() = 0;
	virtual bool Pause(bool paused) = 0;
	virtual bool NeedThreadedCallback() = 0;
	virtual void PrecacheInstruments(const WORD *instruments, int count);
	virtual void TimidityVolumeChanged();
	virtual void FluidSettingInt(const char *setting, int value);
	virtual void FluidSettingNum(const char *setting, double value);
	virtual void FluidSettingStr(const char *setting, const char *value);
	virtual FString GetStats();
};

// WinMM implementation of a MIDI output device -----------------------------

#ifdef _WIN32
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
	int StreamOutSync(MIDIHDR *data);
	int Resume();
	void Stop();
	int PrepareHeader(MIDIHDR *data);
	int UnprepareHeader(MIDIHDR *data);
	bool FakeVolume();
	bool NeedThreadedCallback();
	bool Pause(bool paused);
	void PrecacheInstruments(const WORD *instruments, int count);

protected:
	static void CALLBACK CallbackFunc(HMIDIOUT, UINT, DWORD_PTR, DWORD, DWORD);

	HMIDISTRM MidiOut;
	UINT DeviceID;
	DWORD SavedVolume;
	bool VolumeWorks;

	void (*Callback)(unsigned int, void *, DWORD, DWORD);
	void *CallbackData;
};
#endif

// OPL implementation of a MIDI output device -------------------------------

class OPLMIDIDevice : public MIDIDevice, protected OPLmusicBlock
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
	int StreamOutSync(MIDIHDR *data);
	int Resume();
	void Stop();
	int PrepareHeader(MIDIHDR *data);
	int UnprepareHeader(MIDIHDR *data);
	bool FakeVolume();
	bool NeedThreadedCallback();
	bool Pause(bool paused);
	FString GetStats();

protected:
	static bool FillStream(SoundStream *stream, void *buff, int len, void *userdata);

	void (*Callback)(unsigned int, void *, DWORD, DWORD);
	void *CallbackData;

	void CalcTickRate();
	void HandleEvent(int status, int parm1, int parm2);
	int PlayTick();

	SoundStream *Stream;
	double Tempo;
	double Division;
	MIDIHDR *Events;
	bool Started;
	DWORD Position;
};

// OPL dumper implementation of a MIDI output device ------------------------

class OPLDumperMIDIDevice : public OPLMIDIDevice
{
public:
	OPLDumperMIDIDevice(const char *filename);
	~OPLDumperMIDIDevice();
	int Resume();
	void Stop();
};

// Internal TiMidity MIDI device --------------------------------------------

namespace Timidity { struct Renderer; }

class TimidityMIDIDevice : public MIDIDevice
{
public:
	TimidityMIDIDevice();
	TimidityMIDIDevice(int rate);
	~TimidityMIDIDevice();

	int Open(void (*callback)(unsigned int, void *, DWORD, DWORD), void *userdata);
	void Close();
	bool IsOpen() const;
	int GetTechnology() const;
	int SetTempo(int tempo);
	int SetTimeDiv(int timediv);
	int StreamOut(MIDIHDR *data);
	int StreamOutSync(MIDIHDR *data);
	int Resume();
	void Stop();
	int PrepareHeader(MIDIHDR *data);
	int UnprepareHeader(MIDIHDR *data);
	bool FakeVolume();
	bool Pause(bool paused);
	bool NeedThreadedCallback();
	void PrecacheInstruments(const WORD *instruments, int count);
	void TimidityVolumeChanged();
	FString GetStats();

protected:
	static bool FillStream(SoundStream *stream, void *buff, int len, void *userdata);
	bool ServiceStream (void *buff, int numbytes);

	void (*Callback)(unsigned int, void *, DWORD, DWORD);
	void *CallbackData;

	void CalcTickRate();
	int PlayTick();

	FCriticalSection CritSec;
	SoundStream *Stream;
	Timidity::Renderer *Renderer;
	double Tempo;
	double Division;
	double SamplesPerTick;
	double NextTickIn;
	MIDIHDR *Events;
	bool Started;
	DWORD Position;
};

// Internal TiMidity disk writing version of a MIDI device ------------------

class TimidityWaveWriterMIDIDevice : public TimidityMIDIDevice
{
public:
	TimidityWaveWriterMIDIDevice(const char *filename, int rate);
	~TimidityWaveWriterMIDIDevice();
	int Resume();
	void Stop();

protected:
	FILE *File;
};

// FluidSynth implementation of a MIDI device -------------------------------

#ifdef HAVE_FLUIDSYNTH
#include <fluidsynth.h>

class FluidSynthMIDIDevice : public MIDIDevice
{
public:
	FluidSynthMIDIDevice();
	~FluidSynthMIDIDevice();

	int Open(void (*callback)(unsigned int, void *, DWORD, DWORD), void *userdata);
	void Close();
	bool IsOpen() const;
	int GetTechnology() const;
	int SetTempo(int tempo);
	int SetTimeDiv(int timediv);
	int StreamOut(MIDIHDR *data);
	int StreamOutSync(MIDIHDR *data);
	int Resume();
	void Stop();
	int PrepareHeader(MIDIHDR *data);
	int UnprepareHeader(MIDIHDR *data);
	bool FakeVolume();
	bool Pause(bool paused);
	bool NeedThreadedCallback();
	void PrecacheInstruments(const WORD *instruments, int count);
	FString GetStats();
	void FluidSettingInt(const char *setting, int value);
	void FluidSettingNum(const char *setting, double value);
	void FluidSettingStr(const char *setting, const char *value);

protected:
	static bool FillStream(SoundStream *stream, void *buff, int len, void *userdata);
	bool ServiceStream(void *buff, int numbytes);
	void HandleEvent(int status, int parm1, int parm2);
	int LoadPatchSets(const char *patches);

	void (*Callback)(unsigned int, void *, DWORD, DWORD);
	void *CallbackData;

	void CalcTickRate();
	int PlayTick();

	FCriticalSection CritSec;
	SoundStream *Stream;
	fluid_settings_t *FluidSettings;
	fluid_synth_t *FluidSynth;
	double Tempo;
	double Division;
	double SamplesPerTick;
	double NextTickIn;
	MIDIHDR *Events;
	bool Started;
	DWORD Position;
};
#endif

// Base class for streaming MUS and MIDI files ------------------------------

// MIDI device selection.
enum EMIDIDevice
{
	MIDI_Win,
	MIDI_OPL,
	MIDI_Timidity,
	MIDI_Fluid
};

class MIDIStreamer : public MusInfo
{
public:
	MIDIStreamer(EMIDIDevice type);
	~MIDIStreamer();

	void MusicVolumeChanged();
	void TimidityVolumeChanged();
	void Play(bool looping, int subsong);
	void Pause();
	void Resume();
	void Stop();
	bool IsPlaying();
	bool IsMIDI() const;
	bool IsValid() const;
	void Update();
	FString GetStats();
	void FluidSettingInt(const char *setting, int value);
	void FluidSettingNum(const char *setting, double value);
	void FluidSettingStr(const char *setting, const char *value);

protected:
	MIDIStreamer(const char *dumpname, EMIDIDevice type);

	void OutputVolume (DWORD volume);
	int FillBuffer(int buffer_num, int max_events, DWORD max_time);
	int ServiceEvent();
	int VolumeControllerChange(int channel, int volume);
	
	static void Callback(unsigned int uMsg, void *userdata, DWORD dwParam1, DWORD dwParam2);

	// Virtuals for subclasses to override
	virtual void CheckCaps();
	virtual void DoInitialSetup() = 0;
	virtual void DoRestart() = 0;
	virtual bool CheckDone() = 0;
	virtual void Precache() = 0;
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

#ifdef _WIN32
	static DWORD WINAPI PlayerProc (LPVOID lpParameter);
	DWORD PlayerLoop();
	
	HANDLE PlayerThread;
	HANDLE ExitEvent;
	HANDLE BufferDoneEvent;
#endif

	MIDIDevice *MIDI;
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
	EMIDIDevice DeviceType;
	bool CallbackIsThreaded;
	FString DumpFilename;
};

// MUS file played with a MIDI stream ---------------------------------------

class MUSSong2 : public MIDIStreamer
{
public:
	MUSSong2(FILE *file, BYTE *musiccache, int length, EMIDIDevice type);
	~MUSSong2();

	MusInfo *GetOPLDumper(const char *filename);
	MusInfo *GetWaveDumper(const char *filename, int rate);

protected:
	MUSSong2(const MUSSong2 *original, const char *filename, EMIDIDevice type);	// file dump constructor

	void DoInitialSetup();
	void DoRestart();
	bool CheckDone();
	void Precache();
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
	MIDISong2(FILE *file, BYTE *musiccache, int length, EMIDIDevice type);
	~MIDISong2();

	MusInfo *GetOPLDumper(const char *filename);
	MusInfo *GetWaveDumper(const char *filename, int rate);

protected:
	MIDISong2(const MIDISong2 *original, const char *filename, EMIDIDevice type);	// file dump constructor

	void CheckCaps();
	void DoInitialSetup();
	void DoRestart();
	bool CheckDone();
	void Precache();
	DWORD *MakeEvents(DWORD *events, DWORD *max_events_p, DWORD max_time);
	void AdvanceTracks(DWORD time);

	struct TrackInfo;

	void ProcessInitialMetaEvents ();
	DWORD *SendCommand (DWORD *event, TrackInfo *track, DWORD delay);
	TrackInfo *FindNextDue ();
	void SetTempo(int new_tempo);

	BYTE *MusHeader;
	int SongLen;
	TrackInfo *Tracks;
	TrackInfo *TrackDue;
	int NumTracks;
	int Format;
	WORD DesignationMask;
};

// Anything supported by FMOD out of the box --------------------------------

class StreamSong : public MusInfo
{
public:
	StreamSong (const char *file, int offset, int length);
	~StreamSong ();
	void Play (bool looping, int subsong);
	void Pause ();
	void Resume ();
	void Stop ();
	bool IsPlaying ();
	bool IsMIDI () const { return false; }
	bool IsValid () const { return m_Stream != NULL; }
	bool SetPosition (unsigned int pos);
	bool SetSubsong (int subsong);
	FString GetStats();

protected:
	StreamSong () : m_Stream(NULL) {}

	SoundStream *m_Stream;
};

#if defined(_WIN32) || !defined(NO_SOUND)
#endif
// MIDI file played with Timidity and possibly streamed through FMOD --------

class TimiditySong : public StreamSong
{
public:
	TimiditySong (FILE *file, BYTE *musiccache, int length);
	~TimiditySong ();
	void Play (bool looping, int subsong);
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
	OPLMUSSong (FILE *file, BYTE *musiccache, int length);
	~OPLMUSSong ();
	void Play (bool looping, int subsong);
	bool IsPlaying ();
	bool IsValid () const;
	void ResetChips ();
	MusInfo *GetOPLDumper(const char *filename);

protected:
	OPLMUSSong(const OPLMUSSong *original, const char *filename);	// OPL dump constructor

	static bool FillStream (SoundStream *stream, void *buff, int len, void *userdata);

	OPLmusicFile *Music;
};

class OPLMUSDumper : public OPLMUSSong
{
public:
	OPLMUSDumper(const OPLMUSSong *original, const char *filename);
	void Play(bool looping);
};

// CD track/disk played through the multimedia system -----------------------

class CDSong : public MusInfo
{
public:
	CDSong (int track, int id);
	~CDSong ();
	void Play (bool looping, int subsong);
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

// Module played via foo_dumb -----------------------------------------------

MusInfo *MOD_OpenSong(FILE *file, BYTE *musiccache, int len);

// Music played via Game Music Emu ------------------------------------------

const char *GME_CheckFormat(uint32 header);
MusInfo *GME_OpenSong(FILE *file, BYTE *musiccache, int len, const char *fmt);

// --------------------------------------------------------------------------

extern MusInfo *currSong;
extern int		nomusic;

EXTERN_CVAR (Float, snd_musicvolume)
