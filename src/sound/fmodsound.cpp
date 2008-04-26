/*
** i_sound.cpp
** System interface for sound; uses fmod.dll
**
**---------------------------------------------------------------------------
** Copyright 1998-2008 Randy Heit
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

// HEADER FILES ------------------------------------------------------------

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include "resource.h"
extern HWND Window;
#define USE_WINDOWS_DWORD
#else
#define FALSE 0
#define TRUE 1
#endif

#include "templates.h"
#ifndef NO_SOUND
#include "fmodsound.h"
#endif
#include "c_cvars.h"
#include "i_system.h"
#include "gi.h"
#include "actor.h"
#include "r_state.h"
#include "w_wad.h"
#include "i_music.h"
#include "i_musicinterns.h"
#include "v_text.h"

// MACROS ------------------------------------------------------------------

// killough 2/21/98: optionally use varying pitched sounds
#define PITCH(freq,pitch) (snd_pitched ? ((freq)*(pitch))/128.f : float(freq))

// Just some extra for music and whatever
#define NUM_EXTRA_SOFTWARE_CHANNELS		8

#define ERRCHECK(x)


// TYPES -------------------------------------------------------------------

struct FEnumList
{
	const char *Name;
	int Value;
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

FMOD_RESULT SPC_CreateCodec(FMOD::System *sys);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static int Enum_NumForName(const FEnumList *list, const char *name);
static const char *Enum_NameForNum(const FEnumList *list, int num);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int MAX_SND_DIST;

EXTERN_CVAR (String, snd_output)
EXTERN_CVAR (Float, snd_musicvolume)
EXTERN_CVAR (Int, snd_buffersize)
EXTERN_CVAR (Int, snd_samplerate)
EXTERN_CVAR (Bool, snd_pitched)
EXTERN_CVAR (Int, snd_channels)

// PUBLIC DATA DEFINITIONS -------------------------------------------------

#ifndef NO_SOUND
ReverbContainer *ForcedEnvironment;

CVAR (Int, snd_driver, 0, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Bool, snd_3d, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Bool, snd_hw3d, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Bool, snd_waterreverb, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (String, snd_resampler, "Linear", CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (String, snd_speakermode, "Auto", CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (String, snd_output_format, "PCM-16", CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Bool, snd_dspnet, false, 0)

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static const int S_CLIPPING_DIST = 1200;
static const int S_CLOSE_DIST = 160;
static const ReverbContainer *PrevEnvironment;

// In the below lists, duplicate entries are for user selection. When
// queried, only the first one for the particular value is shown.
static const FEnumList OutputNames[] =
{
	{ "Auto",					FMOD_OUTPUTTYPE_AUTODETECT },
	{ "Default",				FMOD_OUTPUTTYPE_AUTODETECT },
	{ "No sound",				FMOD_OUTPUTTYPE_NOSOUND },

	// Windows
	{ "DirectSound",			FMOD_OUTPUTTYPE_DSOUND },
	{ "DSound",					FMOD_OUTPUTTYPE_DSOUND },
	{ "Windows Multimedia",		FMOD_OUTPUTTYPE_WINMM },
	{ "WinMM",					FMOD_OUTPUTTYPE_WINMM },
	{ "WaveOut",				FMOD_OUTPUTTYPE_WINMM },
	{ "OpenAL",					FMOD_OUTPUTTYPE_OPENAL },
	{ "WASAPI",					FMOD_OUTPUTTYPE_WASAPI },
	{ "ASIO",					FMOD_OUTPUTTYPE_ASIO },

	// Linux
	{ "OSS",					FMOD_OUTPUTTYPE_OSS },
	{ "ALSA",					FMOD_OUTPUTTYPE_ALSA },
	{ "ESD",					FMOD_OUTPUTTYPE_ESD },

	// Mac
	{ "Sound Manager",			FMOD_OUTPUTTYPE_SOUNDMANAGER },
	{ "Core Audio",				FMOD_OUTPUTTYPE_COREAUDIO },

	{ NULL, 0 }
};

static const FEnumList SpeakerModeNames[] =
{
	{ "Mono",					FMOD_SPEAKERMODE_MONO },
	{ "Stereo",					FMOD_SPEAKERMODE_STEREO },
	{ "Quad",					FMOD_SPEAKERMODE_QUAD },
	{ "Surround",				FMOD_SPEAKERMODE_SURROUND },
	{ "5.1",					FMOD_SPEAKERMODE_5POINT1 },
	{ "7.1",					FMOD_SPEAKERMODE_7POINT1 },
	{ "Prologic",				FMOD_SPEAKERMODE_PROLOGIC },
	{ "1",						FMOD_SPEAKERMODE_MONO },
	{ "2",						FMOD_SPEAKERMODE_STEREO },
	{ "4",						FMOD_SPEAKERMODE_QUAD },
	{ NULL, 0 }
};

static const FEnumList ResamplerNames[] =
{
	{ "No Interpolation",		FMOD_DSP_RESAMPLER_NOINTERP },
	{ "NoInterp",				FMOD_DSP_RESAMPLER_NOINTERP },
	{ "Linear",					FMOD_DSP_RESAMPLER_LINEAR },
	{ "Cubic",					FMOD_DSP_RESAMPLER_CUBIC },
	{ "Spline",					FMOD_DSP_RESAMPLER_SPLINE },
	{ NULL, 0 }
};

static const FEnumList SoundFormatNames[] =
{
	{ "None",					FMOD_SOUND_FORMAT_NONE },
	{ "PCM-8",					FMOD_SOUND_FORMAT_PCM8 },
	{ "PCM-16",					FMOD_SOUND_FORMAT_PCM16 },
	{ "PCM-24",					FMOD_SOUND_FORMAT_PCM24 },
	{ "PCM-32",					FMOD_SOUND_FORMAT_PCM32 },
	{ "PCM-Float",				FMOD_SOUND_FORMAT_PCMFLOAT },
	{ "GCADPCM",				FMOD_SOUND_FORMAT_GCADPCM },
	{ "IMAADPCM",				FMOD_SOUND_FORMAT_IMAADPCM },
	{ "VAG",					FMOD_SOUND_FORMAT_VAG },
	{ "XMA",					FMOD_SOUND_FORMAT_XMA },
	{ "MPEG",					FMOD_SOUND_FORMAT_MPEG },
	{ NULL, 0 }
};

// CODE --------------------------------------------------------------------

//==========================================================================
//
// Enum_NumForName
//
// Returns the value of an enum name, or -1 if not found.
//
//==========================================================================

static int Enum_NumForName(const FEnumList *list, const char *name)
{
	while (list->Name != NULL)
	{
		if (stricmp(list->Name, name) == 0)
		{
			return list->Value;
		}
		list++;
	}
	return -1;
}

//==========================================================================
//
// Enum_NameForNum
//
// Returns the name of an enum value. If there is more than one name for a
// value, on the first one in the list is returned. Returns NULL if there
// was no match.
//
//==========================================================================

static const char *Enum_NameForNum(const FEnumList *list, int num)
{
	while (list->Name != NULL)
	{
		if (list->Value == num)
		{
			return list->Name;
		}
		list++;
	}
	return NULL;
}

//==========================================================================
//
// The container for a FSOUND_STREAM.
//
//==========================================================================

class FMODStreamCapsule : public SoundStream
{
public:
	FMODStreamCapsule(FMOD::Sound *stream, FMODSoundRenderer *owner)
		: Owner(owner), Stream(stream), Channel(NULL), DSP(NULL),
		  UserData(NULL), Callback(NULL)
	{}

	FMODStreamCapsule(void *udata, SoundStreamCallback callback, FMODSoundRenderer *owner)
		: Owner(owner), Stream(NULL), Channel(NULL), DSP(NULL),
		  UserData(udata), Callback(callback)
	{}

	~FMODStreamCapsule()
	{
		if (Stream != NULL)
		{
			Stream->release();
		}
	}

	void SetStream(FMOD::Sound *stream)
	{
		Stream = stream;
	}

	bool Play(bool looping, float volume, bool normalize)
	{
		FMOD_RESULT result;

		Stream->setMode(looping ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF);
		result = Owner->Sys->playSound(FMOD_CHANNEL_FREE, Stream, true, &Channel);
		if (result != FMOD_OK)
		{
			return false;
		}
		Channel->setChannelGroup(Owner->MusicGroup);
		Channel->setVolume(volume);
		if (Owner->Sound3D)
		{ // Ensure reverb is disabled when using 3D sound.
			FMOD_REVERB_CHANNELPROPERTIES reverb;
			if (FMOD_OK == Channel->getReverbProperties(&reverb))
			{
				reverb.Room = -10000;
				Channel->setReverbProperties(&reverb);
			}
		}
		if (normalize)
		{ // Attach a normalizer DSP unit to the channel.
			result = Owner->Sys->createDSPByType(FMOD_DSP_TYPE_NORMALIZE, &DSP);
			if (result == FMOD_OK)
			{
				Channel->addDSP(DSP);
			}
		}
		Channel->setPaused(false);
		return true;
	}

	void Stop()
	{
		if (Channel != NULL)
		{
			Channel->stop();
			Channel = NULL;
		}
		if (DSP != NULL)
		{
			DSP->release();
			DSP = NULL;
		}
	}

	bool SetPaused(bool paused)
	{
		if (Channel != NULL)
		{
			return FMOD_OK == Channel->setPaused(paused);
		}
		return false;
	}

	unsigned int GetPosition()
	{
		unsigned int pos;

		if (Channel != NULL && FMOD_OK == Channel->getPosition(&pos, FMOD_TIMEUNIT_MS))
		{
			return pos;
		}
		return 0;
	}

	void SetVolume(float volume)
	{
		if (Channel != NULL)
		{
			Channel->setVolume(volume);
		}
	}

	// Sets the current order number for a MOD-type song, or the position in ms
	// for anything else.
	bool SetPosition(int pos)
	{
		FMOD_SOUND_TYPE type;

		if (FMOD_OK == Stream->getFormat(&type, NULL, NULL, NULL) &&
			(type == FMOD_SOUND_TYPE_IT ||
			 type == FMOD_SOUND_TYPE_MOD ||
			 type == FMOD_SOUND_TYPE_S3M ||
			 type == FMOD_SOUND_TYPE_XM))
		{
			return FMOD_OK == Channel->setPosition(pos, FMOD_TIMEUNIT_MODORDER);
		}
		return FMOD_OK == Channel->setPosition(pos, FMOD_TIMEUNIT_MS);
	}

	static FMOD_RESULT F_CALLBACK PCMReadCallback(FMOD_SOUND *sound, void *data, unsigned int datalen)
	{
		FMOD_RESULT result;
		FMODStreamCapsule *self;
		
		result = ((FMOD::Sound *)sound)->getUserData((void **)&self);
		if (result != FMOD_OK || self == NULL || self->Callback == NULL)
		{
			return FMOD_ERR_INVALID_PARAM;
		}
		if (self->Callback(self, data, datalen, self->UserData))
		{
			return FMOD_OK;
		}
		else
		{
			return FMOD_ERR_FILE_EOF;
		}
	}

	static FMOD_RESULT F_CALLBACK PCMSetPosCallback(FMOD_SOUND *sound, int subsound, unsigned int position, FMOD_TIMEUNIT postype)
	{
		// This is useful if the user calls Channel::setPosition and you want
		// to seek your data accordingly.
		return FMOD_OK;
	}

private:
	FMODSoundRenderer *Owner;
	FMOD::Sound *Stream;
	FMOD::Channel *Channel;
	FMOD::DSP *DSP;
	void *UserData;
	SoundStreamCallback Callback;
};

//==========================================================================
//
// The interface the game uses to talk to FMOD.
//
//==========================================================================

FMODSoundRenderer::FMODSoundRenderer()
{
	Init();
}

FMODSoundRenderer::~FMODSoundRenderer()
{
	Shutdown();
}

bool FMODSoundRenderer::IsValid()
{
	return Sys != NULL;
}

bool FMODSoundRenderer::Init()
{
	FMOD_RESULT result;
	unsigned int version;
	FMOD_SPEAKERMODE speakermode;
	FMOD_SOUND_FORMAT format;
	FMOD_DSP_RESAMPLER resampler;
	FMOD_INITFLAGS initflags;

	int eval;

	ChannelMap = NULL;
	NumChannels = 0;
	SFXPaused = false;
	MusicGroup = NULL;
	SfxGroup = NULL;
	PausableSfx = NULL;
	PrevEnvironment = DefaultEnvironments[0];

	Printf ("I_InitSound: Initializing FMOD\n");

	// Create a System object and initialize.
	result = FMOD::System_Create(&Sys);
	ERRCHECK(result);

	result = Sys->getVersion(&version);
	ERRCHECK(result);

	if (version < FMOD_VERSION)
	{
		Printf ("Error! You are using an old version of FMOD %08x.\n"
				"This program requires %08x\n", version, FMOD_VERSION);
		return false;
	}

	result = Sys->getDriverCaps(0, &Driver_Caps, &Driver_MinFrequency, &Driver_MaxFrequency, &speakermode);
	ERRCHECK(result);

	// Set the user selected speaker mode.
	eval = Enum_NumForName(SpeakerModeNames, snd_speakermode);
	if (eval >= 0)
	{
		speakermode = FMOD_SPEAKERMODE(eval);
	}
	result = Sys->setSpeakerMode(speakermode);
	ERRCHECK(result);

	// Set software format
	eval = Enum_NumForName(SoundFormatNames, snd_output_format);
	format = eval >= 0 ? FMOD_SOUND_FORMAT(eval) : FMOD_SOUND_FORMAT_PCM16;
	eval = Enum_NumForName(ResamplerNames, snd_resampler);
	resampler = eval >= 0 ? FMOD_DSP_RESAMPLER(eval) : FMOD_DSP_RESAMPLER_LINEAR;
	result = Sys->setSoftwareFormat(snd_samplerate, format, 0, 0, resampler);
	ERRCHECK(result);

	// Set software channels according to snd_channels
	result = Sys->setSoftwareChannels(snd_channels + NUM_EXTRA_SOFTWARE_CHANNELS);
	ERRCHECK(result);

#ifdef _WIN32
	if (OSPlatform == os_WinNT4)
	{
		// The following was true as of FMOD 3. I don't know if it still
		// applies to FMOD Ex, nor do I have an NT 4 install anymore, but
		// there's no reason to get rid of it yet.
		//
		// If running Windows NT 4, we need to initialize DirectSound before
		// using WinMM. If we don't, then FSOUND_Close will corrupt a
		// heap. This might just be the Audigy's drivers--I don't know why
		// it happens. At least the fix is simple enough. I only need to
		// initialize DirectSound once, and then I can initialize/close
		// WinMM as many times as I want.
		//
		// Yes, using WinMM under NT 4 is a good idea. I can get latencies as
		// low as 20 ms with WinMM, but with DirectSound I need to have the
		// latency as high as 120 ms to avoid crackling--quite the opposite
		// from the other Windows versions with real DirectSound support.

		static bool inited_dsound = false;

		if (!inited_dsound)
		{
			if (Sys->setOutput(FMOD_OUTPUTTYPE_DSOUND) == FMOD_OK)
			{
				if (Sys->init(1, FMOD_INIT_NORMAL, 0) == FMOD_OK)
				{
					inited_dsound = true;
					Sleep(50);
					Sys->close();
				}
				Sys->setOutput(FMOD_OUTPUTTYPE_WINMM);
			}
		}
	}
#endif

	// Set the user specified output mode.
	eval = Enum_NumForName(OutputNames, snd_output);
	if (eval >= 0)
	{
		result = Sys->setOutput(FMOD_OUTPUTTYPE(eval));
		ERRCHECK(result);
	}

	if (Driver_Caps & FMOD_CAPS_HARDWARE_EMULATED)
	{ // The user has the 'Acceleration' slider set to off!
	  // This is really bad for latency!
		Printf ("Warning: The sound acceleration slider has been set to off.\n");
		Printf ("Please turn it back on if you want decent sound.\n");
		result = Sys->setDSPBufferSize(1024, 10);	// At 48khz, the latency between issuing an fmod command and hearing it will now be about 213ms.
		ERRCHECK(result);
	}
	/*
	// [BC] Initalize FMOD with global focus, so that sound continues to play even if
	// Skulltag is minimized. That way, people can join a server, alt+tab out while
	// waiting for someone to show up, and then hear if someone joins.
	if (!FModLog (FSOUND_Init (snd_samplerate, 64, FSOUND_INIT_DSOUND_DEFERRED|FSOUND_INIT_GLOBALFOCUS)))
	*/
	// Try to init
	initflags = FMOD_INIT_NORMAL | FMOD_INIT_SOFTWARE_HRTF;
	if (snd_dspnet)
	{
		initflags |= FMOD_INIT_ENABLE_DSPNET;
	}
	result = Sys->init(100, initflags, 0);
	if (result == FMOD_ERR_OUTPUT_CREATEBUFFER)
	{ // The speaker mode selected isn't supported by this soundcard. Switch it back to stereo.
		result = Sys->setSpeakerMode(FMOD_SPEAKERMODE_STEREO);
		ERRCHECK(result);

		result = Sys->init(100, FMOD_INIT_NORMAL, 0);
		ERRCHECK(result);
	}
	if (result != FMOD_OK)
	{ // Initializing FMOD failed. Cry cry.
		return false;
	}

	// Create channel groups
	result = Sys->createChannelGroup("Music", &MusicGroup);
	ERRCHECK(result);

	result = Sys->createChannelGroup("SFX", &SfxGroup);
	ERRCHECK(result);

	result = Sys->createChannelGroup("Pausable SFX", &PausableSfx);
	ERRCHECK(result);

	result = SfxGroup->addGroup(PausableSfx);
	ERRCHECK(result);

	result = SPC_CreateCodec(Sys);

	if (snd_3d)
	{
		float rolloff_factor;

		Sound3D = true;
		if (gameinfo.gametype == GAME_Doom || gameinfo.gametype == GAME_Strife)
		{ 
			rolloff_factor = 1.7f;
		}
		else if (gameinfo.gametype == GAME_Heretic)
		{
			rolloff_factor = 1.24f;
		}
		else
		{
			rolloff_factor = 0.96f;
		}
		Sys->set3DSettings(1.f, 100.f, rolloff_factor);
		Hardware3D = snd_hw3d;
	}
	else
	{
		Sound3D = false;
		Hardware3D = false;
	}
	snd_sfxvolume.Callback ();
	return true;
}

void FMODSoundRenderer::Shutdown()
{
	if (Sys != NULL)
	{
		unsigned int i;

		if (MusicGroup != NULL)
		{
			MusicGroup->release();
			MusicGroup = NULL;
		}
		if (PausableSfx != NULL)
		{
			PausableSfx->release();
			PausableSfx = NULL;
		}
		if (SfxGroup != NULL)
		{
			SfxGroup->release();
			SfxGroup = NULL;
		}
		if (ChannelMap)
		{
			delete[] ChannelMap;
			ChannelMap = NULL;
		}
		NumChannels = 0;

		// Free all loaded samples
		for (i = 0; i < S_sfx.Size (); i++)
		{
			if (S_sfx[i].data != NULL)
			{
				((FMOD::Sound *)S_sfx[i].data)->release();
				S_sfx[i].data = NULL;
			}
			if (S_sfx[i].altdata != NULL)
			{
				((FMOD::Sound *)S_sfx[i].altdata)->release();
				S_sfx[i].altdata = NULL;
			}
			S_sfx[i].bHaveLoop = false;
		}

		Sys->close();
		Sys = NULL;
	}
}

void FMODSoundRenderer::PrintStatus()
{
	FMOD_OUTPUTTYPE output;
	FMOD_SPEAKERMODE speakermode;
	FMOD_SOUND_FORMAT format;
	FMOD_DSP_RESAMPLER resampler;
	int driver;
	int samplerate;
	int numoutputchannels;
	int num2d, num3d, total;

	if (FMOD_OK == Sys->getOutput(&output))
	{
		Printf ("Output type: "TEXTCOLOR_GREEN"%s\n", Enum_NameForNum(OutputNames, output));
	}
	if (FMOD_OK == Sys->getSpeakerMode(&speakermode))
	{
		Printf ("Speaker mode: "TEXTCOLOR_GREEN"%s\n", Enum_NameForNum(SpeakerModeNames, speakermode));
	}
	if (FMOD_OK == Sys->getDriver(&driver))
	{
		char name[256];
		if (FMOD_OK != Sys->getDriverInfo(driver, name, sizeof(name), NULL))
		{
			strcpy(name, "Unknown");
		}
		Printf ("Driver: "TEXTCOLOR_GREEN"%d"TEXTCOLOR_NORMAL" ("TEXTCOLOR_ORANGE"%s"TEXTCOLOR_NORMAL")\n", driver, name);
		DumpDriverCaps(Driver_Caps, Driver_MinFrequency, Driver_MaxFrequency);
	}
	if (FMOD_OK == Sys->getHardwareChannels(&num2d, &num3d, &total))
	{
		Printf (TEXTCOLOR_YELLOW "Hardware 2D channels: "TEXTCOLOR_GREEN"%d\n", num2d);
		Printf (TEXTCOLOR_YELLOW "Hardware 3D channels: "TEXTCOLOR_GREEN"%d\n", num3d);
		Printf (TEXTCOLOR_YELLOW "Total hardware channels: "TEXTCOLOR_GREEN"%d\n", total);
	}
	if (FMOD_OK == Sys->getSoftwareFormat(&samplerate, &format, &numoutputchannels, NULL, &resampler, NULL))
	{
		Printf (TEXTCOLOR_LIGHTBLUE "Software mixer sample rate: "TEXTCOLOR_GREEN"%d\n", samplerate);
		Printf (TEXTCOLOR_LIGHTBLUE "Software mixer format: "TEXTCOLOR_GREEN"%s\n", Enum_NameForNum(SoundFormatNames, format));
		Printf (TEXTCOLOR_LIGHTBLUE "Software mixer channels: "TEXTCOLOR_GREEN"%d\n", numoutputchannels);
		Printf (TEXTCOLOR_LIGHTBLUE "Software mixer resampler: "TEXTCOLOR_GREEN"%s\n", Enum_NameForNum(ResamplerNames, resampler));
	}
	Printf("Using 3D sound: "TEXTCOLOR_GREEN"%s\n", Sound3D ? "yes" : "no");
	if (Sound3D)
	{
		Printf("Using hardware 3D sound: "TEXTCOLOR_GREEN"%s\n", Hardware3D ? "yes" : "no");
	}
}

void FMODSoundRenderer::DumpDriverCaps(FMOD_CAPS caps, int minfrequency, int maxfrequency)
{
	Printf (TEXTCOLOR_OLIVE "   Min. frequency: "TEXTCOLOR_GREEN"%d\n", minfrequency);
	Printf (TEXTCOLOR_OLIVE "   Max. frequency: "TEXTCOLOR_GREEN"%d\n", maxfrequency);
	Printf ("  Features:\n");
	if (caps == 0)									Printf(TEXTCOLOR_OLIVE "   None\n");
	if (caps & FMOD_CAPS_HARDWARE)					Printf(TEXTCOLOR_OLIVE "   Hardware mixing\n");
	if (caps & FMOD_CAPS_HARDWARE_EMULATED)			Printf(TEXTCOLOR_OLIVE "   Hardware acceleration is turned off!\n");
	if (caps & FMOD_CAPS_OUTPUT_MULTICHANNEL)		Printf(TEXTCOLOR_OLIVE "   Multichannel\n");
	if (caps & FMOD_CAPS_OUTPUT_FORMAT_PCM8)		Printf(TEXTCOLOR_OLIVE "   PCM-8");
	if (caps & FMOD_CAPS_OUTPUT_FORMAT_PCM16)		Printf(TEXTCOLOR_OLIVE "   PCM-16");
	if (caps & FMOD_CAPS_OUTPUT_FORMAT_PCM24)		Printf(TEXTCOLOR_OLIVE "   PCM-24");
	if (caps & FMOD_CAPS_OUTPUT_FORMAT_PCM32)		Printf(TEXTCOLOR_OLIVE "   PCM-32");
	if (caps & FMOD_CAPS_OUTPUT_FORMAT_PCMFLOAT)	Printf(TEXTCOLOR_OLIVE "   PCM-Float");
	if (caps & (FMOD_CAPS_OUTPUT_FORMAT_PCM8 | FMOD_CAPS_OUTPUT_FORMAT_PCM16 | FMOD_CAPS_OUTPUT_FORMAT_PCM24 | FMOD_CAPS_OUTPUT_FORMAT_PCM32 | FMOD_CAPS_OUTPUT_FORMAT_PCMFLOAT))
	{
		Printf("\n");
	}
	if (caps & FMOD_CAPS_REVERB_EAX2)				Printf(TEXTCOLOR_OLIVE "   EAX2");
	if (caps & FMOD_CAPS_REVERB_EAX3)				Printf(TEXTCOLOR_OLIVE "   EAX3");
	if (caps & FMOD_CAPS_REVERB_EAX4)				Printf(TEXTCOLOR_OLIVE "   EAX4");
	if (caps & FMOD_CAPS_REVERB_EAX5)				Printf(TEXTCOLOR_OLIVE "   EAX5");
	if (caps & FMOD_CAPS_REVERB_I3DL2)				Printf(TEXTCOLOR_OLIVE "   I3DL2");
	if (caps & (FMOD_CAPS_REVERB_EAX2 | FMOD_CAPS_REVERB_EAX3 | FMOD_CAPS_REVERB_EAX4 | FMOD_CAPS_REVERB_EAX5 | FMOD_CAPS_REVERB_I3DL2))
	{
		Printf("\n");
	}
	if (caps & FMOD_CAPS_REVERB_LIMITED)			Printf("TEXTCOLOR_OLIVE    Limited reverb\n");
}

void FMODSoundRenderer::PrintDriversList()
{
	int numdrivers;
	int i;
	char name[256];
	
	if (FMOD_OK == Sys->getNumDrivers(&numdrivers))
	{
		for (i = 0; i < numdrivers; ++i)
		{
			if (FMOD_OK == Sys->getDriverInfo(i, name, sizeof(name), NULL))
			{
				Printf("%d. %s\n", i, name);
			}
		}
	}
}

FString FMODSoundRenderer::GatherStats()
{
	int channels;
	float dsp, stream, update, total;
	FString out;

	channels = 0;
	total = update = stream = dsp = 0;
	Sys->getChannelsPlaying(&channels);
	Sys->getCPUUsage(&dsp, &stream, &update, &total);

	out.Format ("%d channels,%5.2f%% CPU (%.2f%% DSP  %.2f%% Stream  %.2f%% Update)",
		channels, total, dsp, stream, update);
	return out;
}

void FMODSoundRenderer::MovieDisableSound()
{
	I_ShutdownMusic();
	Shutdown();
}

void FMODSoundRenderer::MovieResumeSound()
{
	Init();
	S_Init();
	S_RestartMusic();
}

void FMODSoundRenderer::SetSfxVolume(float volume)
{
	SfxGroup->setVolume(volume);
}

void FMODSoundRenderer::SetMusicVolume(float volume)
{
	MusicGroup->setVolume(volume);
}

int FMODSoundRenderer::GetNumChannels()
{
	int chancount;

	if (!Hardware3D)
	{
		if (FMOD_OK == Sys->getSoftwareChannels(&chancount))
		{
			chancount = MIN<int>(snd_channels, chancount - NUM_EXTRA_SOFTWARE_CHANNELS);
		}
	}
	// If hardware, let FMOD deal with the maximum actually supported by the hardware.
	chancount = snd_channels;

	ChannelMap = new ChanMap[chancount];
	for (int i = 0; i < chancount; i++)
	{
		ChannelMap[i].soundID = -1;
	}

	NumChannels = chancount;
	return chancount;
}

SoundStream *FMODSoundRenderer::CreateStream (SoundStreamCallback callback, int buffbytes, int flags, int samplerate, void *userdata)
{
	FMODStreamCapsule *capsule;
	FMOD::Sound *sound;
	FMOD_RESULT result;
	FMOD_CREATESOUNDEXINFO exinfo = { sizeof(exinfo), };
	FMOD_MODE mode;
	int sample_shift;
	int channel_shift;
	
	capsule = new FMODStreamCapsule (userdata, callback, this);

	mode = FMOD_2D | FMOD_OPENUSER | FMOD_LOOP_NORMAL | FMOD_SOFTWARE | FMOD_CREATESTREAM;
	sample_shift = (flags & SoundStream::Bits8) ? 0 : 1;
	channel_shift = (flags & SoundStream::Mono) ? 0 : 1;

	// Chunk size of stream update in samples. This will be the amount of data
	// passed to the user callback.
	exinfo.decodebuffersize	 = buffbytes >> (sample_shift + channel_shift);

	// Number of channels in the sound.
	exinfo.numchannels		 = 1 << channel_shift;

	// Length of PCM data in bytes of whole song (for Sound::getLength).
	// This pretends it's 5 seconds long.
	exinfo.length			 = (samplerate * 5) << (sample_shift + channel_shift);

	// Default playback rate of sound. */
	exinfo.defaultfrequency	 = samplerate;

	// Data format of sound.
	exinfo.format			 = (flags & SoundStream::Bits8) ? FMOD_SOUND_FORMAT_PCM8 : FMOD_SOUND_FORMAT_PCM16;

	// User callback for reading.
	exinfo.pcmreadcallback	 = FMODStreamCapsule::PCMReadCallback;

	// User callback for seeking.
	exinfo.pcmsetposcallback = FMODStreamCapsule::PCMSetPosCallback;

	// User data to be attached to the sound during creation.  Access via Sound::getUserData.
	exinfo.userdata = capsule;

	result = Sys->createSound(NULL, mode, &exinfo, &sound);
	if (result != FMOD_OK)
	{
		delete capsule;
		return NULL;
	}
	capsule->SetStream(sound);
	return capsule;
}

SoundStream *FMODSoundRenderer::OpenStream(const char *filename_or_data, int flags, int offset, int length)
{
	FMOD_MODE mode;
	FMOD_CREATESOUNDEXINFO exinfo = { sizeof(exinfo), };
	FMOD::Sound *stream;

	mode = FMOD_SOFTWARE | FMOD_2D | FMOD_CREATESTREAM;
	if (flags & SoundStream::Loop)
	{
		mode |= FMOD_LOOP_NORMAL;
	}
	if (offset == -1)
	{
		mode |= FMOD_OPENMEMORY;
		offset = 0;
	}
	exinfo.length = length;
	exinfo.fileoffset = offset;

	if (FMOD_OK == Sys->createSound(filename_or_data, mode, &exinfo, &stream))
	{
		return new FMODStreamCapsule(stream, this);
	}
	return NULL;
}

//
// vol range is 0-255
// sep range is 0-255, -1 for surround, -2 for full vol middle
//
long FMODSoundRenderer::StartSound(sfxinfo_t *sfx, float vol, float sep, int pitch, int channel, bool looping, bool pausable)
{
	if (!ChannelMap)
		return 0;

	int id = int(sfx - &S_sfx[0]);
	FMOD_RESULT result;
	FMOD::Channel *chan;
	float freq;

	freq = PITCH(sfx->frequency, pitch);

	result = Sys->playSound(FMOD_CHANNEL_FREE, CheckLooping(sfx, looping), true, &chan);
	if (FMOD_OK == result)
	{
		chan->setChannelGroup((pausable && !SFXPaused) ? PausableSfx : SfxGroup);
		chan->setFrequency(freq);
		chan->setVolume(vol);
		chan->setPan(sep);
		if (Sound3D)
		{ // Make 2D sounds head relative.
			FMOD_MODE mode;

			if (FMOD_OK == chan->getMode(&mode))
			{
				mode = (mode & ~FMOD_3D_WORLDRELATIVE) | (FMOD_3D_HEADRELATIVE);
				chan->setMode(mode);
			}
			FMOD_VECTOR zero = { 0, 0, 0 };
			chan->set3DAttributes(&zero, &zero);
		}
		chan->setPaused(false);
		ChannelMap[channel].channelID = chan;
		ChannelMap[channel].soundID = id;
		ChannelMap[channel].bIsLooping = looping;
		return channel + 1;
	}

	DPrintf ("Sound %s failed to play: %d\n", sfx->name.GetChars(), result);
	return 0;
}

long FMODSoundRenderer::StartSound3D(sfxinfo_t *sfx, float vol, int pitch, int channel,
	bool looping, float pos[3], float vel[3], bool pausable)
{
	if (!Sound3D || !ChannelMap)
		return 0;

	int id = int(sfx - &S_sfx[0]);
	FMOD_RESULT result;
	FMOD::Channel *chan;
	float freq;

	freq = PITCH(sfx->frequency, pitch);

	result = Sys->playSound(FMOD_CHANNEL_FREE, CheckLooping(sfx, looping), true, &chan);
	if (FMOD_OK == result)
	{
		chan->setChannelGroup((pausable && !SFXPaused) ? PausableSfx : SfxGroup);
		chan->setFrequency(freq);
		chan->setVolume(vol);
		chan->set3DAttributes((FMOD_VECTOR *)pos, (FMOD_VECTOR *)vel);
		chan->setPaused(false);
		ChannelMap[channel].channelID = chan;
		ChannelMap[channel].soundID = id;
		ChannelMap[channel].bIsLooping = looping;
		return channel + 1;
	}

	DPrintf ("Sound %s failed to play: %d\n", sfx->name.GetChars(), result);
	return 0;
}

void FMODSoundRenderer::StopSound(long handle)
{
	if (!handle || !ChannelMap)
		return;

	handle--;
	if (ChannelMap[handle].soundID != -1)
	{
		ChannelMap[handle].channelID->stop();
		UncheckSound(&S_sfx[ChannelMap[handle].soundID], ChannelMap[handle].bIsLooping);
		ChannelMap[handle].soundID = -1;
	}
}

void FMODSoundRenderer::StopAllChannels()
{
	for (int i = 1; i <= NumChannels; ++i)
	{
		StopSound(i);
	}
}


void FMODSoundRenderer::SetSfxPaused(bool paused)
{
	if (SFXPaused != paused)
	{
		PausableSfx->setPaused(paused);
		SFXPaused = paused;
	}
}

bool FMODSoundRenderer::IsPlayingSound(long handle)
{
	if (!handle || !ChannelMap)
		return false;

	handle--;
	if (ChannelMap[handle].channelID != NULL)
	{
		bool is;

		if (FMOD_OK == ChannelMap[handle].channelID->isPlaying(&is))
		{
			return is;
		}
	}
	return false;
}

void FMODSoundRenderer::UpdateSoundParams(long handle, float vol, float sep, int pitch)
{
	if (!handle || !ChannelMap)
		return;

	handle--;
	if (ChannelMap[handle].soundID == -1 || ChannelMap[handle].channelID == NULL)
		return;

	float freq = PITCH(S_sfx[ChannelMap[handle].soundID].frequency, pitch);
	FMOD::Channel *chan = ChannelMap[handle].channelID;

	chan->setPan(sep);
	chan->setVolume(vol);
	chan->setFrequency(freq);
}

void FMODSoundRenderer::UpdateSoundParams3D(long handle, float pos[3], float vel[3])
{
	if (!handle || !ChannelMap)
		return;

	handle--;
	if (ChannelMap[handle].soundID == -1 || ChannelMap[handle].channelID == NULL)
		return;

	ChannelMap[handle].channelID->set3DAttributes((FMOD_VECTOR *)pos, (FMOD_VECTOR *)vel);
}

void FMODSoundRenderer::ResetEnvironment()
{
	PrevEnvironment = NULL;
}

void FMODSoundRenderer::UpdateListener(AActor *listener)
{
	float angle;
	FMOD_VECTOR pos, vel;
	FMOD_VECTOR forward;
	FMOD_VECTOR up;

	if(Sound3D && ChannelMap)
	{
		vel.x = listener->momx * (TICRATE/65536.f);
		vel.y = listener->momz * (TICRATE/65536.f);
		vel.z = listener->momy * (TICRATE/65536.f);
		pos.x = listener->x / 65536.f;
		pos.y = listener->z / 65536.f;
		pos.z = listener->y / 65536.f;

		angle = (float)(listener->angle) * ((float)PI / 2147483648.f);

		forward.x = cosf(angle);
		forward.y = 0;
		forward.z = sinf(angle);

		up.x = 0;
		up.y = 1;
		up.z = 0;

		Sys->set3DListenerAttributes(0, &pos, &vel, &forward, &up);

		bool underwater;
		const ReverbContainer *env;

		if (ForcedEnvironment)
		{
			env = ForcedEnvironment;
		}
		else
		{
			underwater = (listener->waterlevel == 3 && snd_waterreverb);
			assert (zones != NULL);
			env = zones[listener->Sector->ZoneNumber].Environment;
			if (env == NULL)
			{
				env = DefaultEnvironments[0];
			}
			if (env == DefaultEnvironments[0] && underwater)
			{
				env = DefaultEnvironments[22];
			}
		}
		if (env != PrevEnvironment || env->Modified)
		{
			DPrintf ("Reverb Environment %s\n", env->Name);
			const_cast<ReverbContainer*>(env)->Modified = false;
			Sys->setReverbProperties((FMOD_REVERB_PROPERTIES *)(&env->Properties));
			PrevEnvironment = env;
		}
	}
	Sys->update();
}

void FMODSoundRenderer::LoadSound(sfxinfo_t *sfx)
{
	if (!sfx->data)
	{
		DPrintf("loading sound \"%s\" (%d) ", sfx->name.GetChars(), sfx - &S_sfx[0]);
		getsfx(sfx);
	}
}

void FMODSoundRenderer::UnloadSound(sfxinfo_t *sfx)
{
	if (sfx->data == NULL)
		return;

	sfx->bHaveLoop = false;
	sfx->normal = 0;
	sfx->looping = 0;
	if (sfx->altdata != NULL)
	{
		((FMOD::Sound *)sfx->altdata)->release();
		sfx->altdata = NULL;
	}
	if (sfx->data != NULL)
	{
		((FMOD::Sound *)sfx->data)->release();
		sfx->data = NULL;
	}

	DPrintf("Unloaded sound \"%s\" (%d)\n", sfx->name.GetChars(), sfx - &S_sfx[0]);
}

// FSOUND_Sample_Upload seems to mess up the signedness of sound data when
// uploading to hardware buffers. The pattern is not particularly predictable,
// so this is a replacement for it that loads the data manually. Source data
// is mono, unsigned, 8-bit. Output is mono, signed, 8- or 16-bit.
#if 0
int FMODSoundRenderer::PutSampleData (FSOUND_SAMPLE *sample, const BYTE *data, int len, unsigned int mode)
{
	/*if (mode & FSOUND_2D)
	{
		return FSOUND_Sample_Upload (sample, const_cast<BYTE *>(data),
			FSOUND_8BITS|FSOUND_MONO|FSOUND_UNSIGNED);
	}
	else*/ if (FSOUND_Sample_GetMode (sample) & FSOUND_8BITS)
	{
		void *ptr1, *ptr2;
		unsigned int len1, len2;

		if (FSOUND_Sample_Lock (sample, 0, len, &ptr1, &ptr2, &len1, &len2))
		{
			int i;
			BYTE *ptr;
			int len;

			for (i = 0, ptr = (BYTE *)ptr1, len = len1;
				 i < 2 && ptr && len;
				i++, ptr = (BYTE *)ptr2, len = len2)
			{
				int j;
				for (j = 0; j < len; j++)
				{
					ptr[j] = *data++ - 128;
				}
			}
			FSOUND_Sample_Unlock (sample, ptr1, ptr2, len1, len2);
			return TRUE;
		}
		else
		{
			return FALSE;
		}
	}
	else
	{
		void *ptr1, *ptr2;
		unsigned int len1, len2;

		if (FSOUND_Sample_Lock (sample, 0, len*2, &ptr1, &ptr2, &len1, &len2))
		{
			int i;
			SWORD *ptr;
			int len;

			for (i = 0, ptr = (SWORD *)ptr1, len = len1/2;
				 i < 2 && ptr && len;
				i++, ptr = (SWORD *)ptr2, len = len2/2)
			{
				int j;
				for (j = 0; j < len; j++)
				{
					ptr[j] = ((*data<<8)|(*data)) - 32768;
					data++;
				}
			}
			FSOUND_Sample_Unlock (sample, ptr1, ptr2, len1, len2);
			return TRUE;
		}
		else
		{
			return FALSE;
		}
	}
}
#endif

void FMODSoundRenderer::DoLoad(void **slot, sfxinfo_t *sfx)
{
	BYTE *sfxdata;
	BYTE *sfxstart;
	int size;
	int errcount;
	FMOD_RESULT result;
	FMOD_MODE samplemode;
	FMOD_CREATESOUNDEXINFO exinfo = { sizeof(exinfo), };
	FMOD::Sound *sample;

	samplemode = (Sound3D ? FMOD_3D : FMOD_2D) | FMOD_OPENMEMORY;
	samplemode |= Hardware3D ? FMOD_HARDWARE : FMOD_SOFTWARE;
	sfxdata = NULL;

	errcount = 0;
	while (errcount < 2)
	{
		if (sfxdata != NULL)
		{
			delete[] sfxdata;
			sfxdata = NULL;
		}

		if (errcount)
			sfx->lumpnum = Wads.GetNumForName("dsempty", ns_sounds);

		size = Wads.LumpLength(sfx->lumpnum);
		if (size == 0)
		{
			errcount++;
			continue;
		}

		FWadLump wlump = Wads.OpenLumpNum(sfx->lumpnum);
		sfxstart = sfxdata = new BYTE[size];
		wlump.Read(sfxdata, size);
		SDWORD len = ((SDWORD *)sfxdata)[1];

		// If the sound is raw, just load it as such.
		// Otherwise, try the sound as DMX format.
		// If that fails, let FMOD try and figure it out.
		if (sfx->bLoadRAW ||
			(((BYTE *)sfxdata)[0] == 3 && ((BYTE *)sfxdata)[1] == 0 && len <= size - 8))
		{
			if (sfx->bLoadRAW)
			{
				len = Wads.LumpLength (sfx->lumpnum);
				sfx->frequency = (sfx->bForce22050 ? 22050 : 11025);
			}
			else
			{
				sfx->frequency = ((WORD *)sfxdata)[1];
				if (sfx->frequency == 0)
				{
					sfx->frequency = 11025;
				}
				sfxstart = sfxdata + 8;
			}
			sfx->length = len;

			exinfo.length = len;
			exinfo.numchannels = 1;
			exinfo.defaultfrequency = sfx->frequency;
			exinfo.format = FMOD_SOUND_FORMAT_PCM8;

			samplemode |= FMOD_OPENRAW;

			// Need to convert sample data from unsigned to signed.
			for (int i = 0; i < len; ++i)
			{
				sfxstart[i] = sfxstart[i] - 128;
			}
		}
		else
		{
			exinfo.length = size;
		}
		result = Sys->createSound((char *)sfxstart, samplemode, &exinfo, &sample);
		if (result == FMOD_ERR_OUTPUT_CREATEBUFFER && !(samplemode & FMOD_SOFTWARE))
		{
			DPrintf("Trying to fall back to software sample\n");
			samplemode = (samplemode & ~FMOD_HARDWARE) | FMOD_SOFTWARE;
			result = Sys->createSound((char *)sfxstart, samplemode, &exinfo, &sample);
		}
		if (result != FMOD_OK)
		{
			DPrintf("Failed to allocate sample: %d\n", result);
			errcount++;
			continue;
		}
		*slot = sample;
		// Get frequency and length for sounds FMOD handled for us.
		if (!(samplemode & FMOD_OPENRAW))
		{
			float freq;

			result = sample->getDefaults(&freq, NULL, NULL, NULL);
			if (result != FMOD_OK)
			{
				DPrintf("Failed getting default sound frequency, assuming 11025Hz\n");
				freq = 11025;
			}
			sfx->frequency = (unsigned int)freq;

			result = sample->getLength(&sfx->length, FMOD_TIMEUNIT_PCM);
			if (result != FMOD_OK)
			{
				DPrintf("Failed getting sample length\n");
			}
		}

		// Get sample length in milliseconds. I think this is only used for ambient sounds?
		if (FMOD_OK != sample->getLength(&sfx->ms, FMOD_TIMEUNIT_MS))
		{
			sfx->ms = (sfx->length * 1000) / sfx->frequency;
		}
		break;
	}

	if (sfx->data)
	{
		DPrintf ("[%d Hz %d samples]\n", sfx->frequency, sfx->length);

		if (Sound3D)
		{
			// Match s_sound.cpp min distance.
			// Max distance is irrelevant.
			sample->set3DMinMaxDistance(float(S_CLOSE_DIST), float(MAX_SND_DIST)*2);
		}
	}

	if (sfxdata != NULL)
	{
		delete[] sfxdata;
	}
}

void FMODSoundRenderer::getsfx(sfxinfo_t *sfx)
{
	unsigned int i;

	// Get the sound data from the WAD and register it with sound library

	// If the sound doesn't exist, replace it with the empty sound.
	if (sfx->lumpnum == -1)
	{
		sfx->lumpnum = Wads.GetNumForName ("dsempty", ns_sounds);
	}
	
	// See if there is another sound already initialized with this lump. If so,
	// then set this one up as a link, and don't load the sound again.
	for (i = 0; i < S_sfx.Size (); i++)
	{
		if (S_sfx[i].data && S_sfx[i].link == sfxinfo_t::NO_LINK && S_sfx[i].lumpnum == sfx->lumpnum)
		{
			DPrintf ("Linked to %s (%d)\n", S_sfx[i].name.GetChars(), i);
			sfx->link = i;
			sfx->ms = S_sfx[i].ms;
			return;
		}
	}

	sfx->bHaveLoop = false;
	sfx->normal = 0;
	sfx->looping = 0;
	sfx->altdata = NULL;
	DoLoad (&sfx->data, sfx);
}


//===========================================================================
//
// FMODSoundRenderer :: CheckLooping
//
// Hardware sounds do not support arbitrarily changing the loop mode for
// different playing copies of the same sound, so if we need to play both
// a looping and an unlooping version of the same sound, then we need two
// copies of the sound.
//
// Fortunately, most sounds will be played as one or the other and not both.
// This function juggles the sample between looping and non-looping, creating
// a copy if necessary, and increasing the appropriate use counter.
//
// Note that software sounds do not have this restriction, but since there's
// no way for the engine to designate that a sound should always be software,
// this function is used for them too, just to keep things simpler for me.
//
//===========================================================================

FMOD::Sound *FMODSoundRenderer::CheckLooping(sfxinfo_t *sfx, bool looped)
{
	if (looped)
	{
		sfx->looping++;
		if (sfx->bHaveLoop)
		{
			return (FMOD::Sound *)(sfx->altdata ? sfx->altdata : sfx->data);
		}
		else if (sfx->normal == 0)
		{
			sfx->bHaveLoop = true;
			((FMOD::Sound *)sfx->data)->setLoopCount(-1);
			return (FMOD::Sound *)sfx->data;
		}
	}
	else
	{
		sfx->normal++;
		if (sfx->altdata || !sfx->bHaveLoop)
		{
			return (FMOD::Sound *)sfx->data;
		}
		else if (sfx->looping == 0)
		{
			sfx->bHaveLoop = false;
			((FMOD::Sound *)sfx->data)->setLoopCount(0);
			return (FMOD::Sound *)sfx->data;
		}
	}

	// If we get here, we need to create an alternate version of the sample.
	((FMOD::Sound *)sfx->data)->setLoopCount(0);
	DoLoad(&sfx->altdata, sfx);
	((FMOD::Sound *)sfx->altdata)->setLoopCount(-1);
	sfx->bHaveLoop = true;
	return (FMOD::Sound *)(looped ? sfx->altdata : sfx->data);
}

void FMODSoundRenderer::UncheckSound(sfxinfo_t *sfx, bool looped)
{
	if (looped)
	{
		if (sfx->looping > 0)
			sfx->looping--;
	}
	else
	{
		if (sfx->normal > 0)
			sfx->normal--;
	}
}
#endif //NO_SOUND
