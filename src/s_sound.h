// Emacs style mode select	 -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// DESCRIPTION:
//		The not so system specific sound interface.
//
//-----------------------------------------------------------------------------


#ifndef __S_SOUND__
#define __S_SOUND__

#include "m_fixed.h"
#include "tarray.h"

class AActor;
class FScanner;

//
// SoundFX struct.
//
struct sfxinfo_t
{
	// Next field is for use by the system sound interface.
	// A non-null data means the sound has been loaded.
	void	   *data;

	FString		name;					// [RH] Sound name defined in SNDINFO
	int 		lumpnum;				// lump number of sfx

	unsigned int next, index;			// [RH] For hashing
//	unsigned int frequency;				// [RH] Preferred playback rate
	float		Volume;

	BYTE		PitchMask;
	SWORD		NearLimit;				// 0 means unlimited

	WORD		bRandomHeader:1;
	WORD		bPlayerReserve:1;
	WORD		bForce11025:1;
	WORD		bForce22050:1;
	WORD		bLoadRAW:1;
	WORD		bPlayerCompat:1;
	WORD		b16bit:1;
	WORD		bUsed:1;
	WORD		bSingular:1;
	WORD		bTentative:1;
	WORD		RolloffType:2;

	unsigned int link;
	enum { NO_LINK = 0xffffffff };

	float		MinDistance;
	union		{ float MaxDistance, RolloffFactor; };
};

// Rolloff types
enum
{
	ROLLOFF_Doom,		// Linear rolloff with a logarithmic volume scale
	ROLLOFF_Linear,		// Linear rolloff with a linear volume scale
	ROLLOFF_Log,		// Logarithmic rolloff (standard hardware type)
	ROLLOFF_Custom		// Lookup volume from SNDCURVE
};

int S_FindSound (const char *logicalname);

// the complete set of sound effects
extern TArray<sfxinfo_t> S_sfx;

// An index into the S_sfx[] array.
class FSoundID
{
public:
	FSoundID()
	{
		ID = 0;
	}
	FSoundID(int id)
	{
		ID = id;
	}
	FSoundID(const char *name)
	{
		ID = S_FindSound(name);
	}
	FSoundID(const FString &name)
	{
		ID = S_FindSound(name);
	}
	FSoundID(const FSoundID &other)
	{
		ID = other.ID;
	}
	FSoundID &operator=(const FSoundID &other)
	{
		ID = other.ID;
		return *this;
	}
	FSoundID &operator=(const char *name)
	{
		ID = S_FindSound(name);
		return *this;
	}
	FSoundID &operator=(const FString &name)
	{
		ID = S_FindSound(name);
		return *this;
	}
	operator int() const
	{
		return ID;
	}
	operator FString() const
	{
		return ID ? S_sfx[ID].name : "";
	}
	operator const char *() const
	{
		return ID ? S_sfx[ID].name.GetChars() : NULL;
	}
private:
	int ID;
protected:
	enum EDummy { NoInit };
	FSoundID(EDummy) {}
};

class FSoundIDNoInit : public FSoundID
{
public:
	FSoundIDNoInit() : FSoundID(NoInit) {}

	FSoundID &operator=(const FSoundID &other)
	{
		return FSoundID::operator=(other);
	}
	FSoundID &operator=(const char *name)
	{
		return FSoundID::operator=(name);
	}
	FSoundID &operator=(const FString &name)
	{
		return FSoundID::operator=(name);
	}
};

FArchive &operator<<(FArchive &arc, FSoundID &sid);

// Default rolloff information.
extern int S_RolloffType;
extern float S_MinDistance;
extern float S_MaxDistanceOrRolloffFactor;
extern BYTE *S_SoundCurve;
extern int S_SoundCurveSize;

// Information about one playing sound.
struct sector_t;
struct FSoundChan
{
	void		*SysChannel;// Channel information from the system interface.
	FSoundChan	*NextChan;	// Next channel in this list.
	FSoundChan **PrevChan;	// Previous channel in this list.
	AActor		*Mover;		// Used for velocity.
	sector_t	*Sector;	// Sector for area sounds.
	fixed_t		*Pt;		// Origin of sound.
	sfxinfo_t	*SfxInfo;	// Sound information.
	fixed_t		X,Y,Z;		// Origin if Mover is NULL.
	int			SoundID;	// Sound ID of playing sound
	int			OrgID;		// Sound ID of sound used to start this channel
	float		Volume;
	float		DistanceScale;
	BYTE		EntChannel;	// Actor's sound channel.
	int			ChanFlags;
};
extern FSoundChan *Channels;

FSoundChan *S_GetChannel(void *syschan);
void S_ReturnChannel(FSoundChan *chan);

void S_LinkChannel(FSoundChan *chan, FSoundChan **head);
void S_UnlinkChannel(FSoundChan *chan);

// Initializes sound stuff, including volume
// Sets channels, SFX and music volume,
//	allocates channel buffer, sets S_sfx lookup.
//
void S_Init ();
void S_InitData ();
void S_Shutdown ();

// Per level startup code.
// Kills playing sounds at start of level and starts new music.
//
void S_Start ();

// Called after a level is loaded. Ensures that most sounds are loaded.
void S_PrecacheLevel ();

// Loads a sound, including any random sounds it might reference.
void S_CacheSound (sfxinfo_t *sfx);

// Start sound for thing at <ent>
void S_Sound (int channel, FSoundID sfxid, float volume, int attenuation);
void S_Sound (AActor *ent, int channel, FSoundID sfxid, float volume, int attenuation);
void S_Sound (fixed_t *pt, int channel, FSoundID sfxid, float volume, int attenuation);
void S_Sound (fixed_t x, fixed_t y, fixed_t z, int channel, FSoundID sfxid, float volume, int attenuation);
void S_Sound (sector_t *sec, int channel, FSoundID sfxid, float volume, int attenuation);

// sound channels
// channel 0 never willingly overrides
// other channels (1-7) always override a playing sound on that channel
//
// CHAN_AUTO searches down from channel 7 until it finds a channel not in use
// CHAN_WEAPON is for weapons
// CHAN_VOICE is for oof, sight, or other voice sounds
// CHAN_ITEM is for small things and item pickup
// CHAN_BODY is for generic body sounds
// CHAN_PICKUP can optionally be set as a local sound only for "compatibility"

#define CHAN_AUTO				0
#define CHAN_WEAPON				1
#define CHAN_VOICE				2
#define CHAN_ITEM				3
#define CHAN_BODY				4
// modifier flags
#define CHAN_LISTENERZ			8
#define CHAN_IMMOBILE			16
#define CHAN_MAYBE_LOCAL		32
#define CHAN_NOPAUSE			64	// do not pause this sound in menus
#define CHAN_AREA				128	// Sound plays from all around. Only valid with sector sounds.
#define CHAN_LOOP				256

// Channel alias for sector sounds. These define how listener height is
// used when calculating 3D sound volume.
#define CHAN_FLOOR				1	// Sound comes from the floor.
#define CHAN_CEILING			2	// Sound comes from the ceiling.
#define CHAN_FULLHEIGHT			3	// Sound comes entire height of the sector.

#define CHAN_IS3D				(1<<24)	// internal flag

#define CHAN_PICKUP				(CHAN_ITEM|CHAN_MAYBE_LOCAL)

// sound attenuation values
#define ATTN_NONE				0	// full volume the entire level
#define ATTN_NORM				1
#define ATTN_IDLE				2
#define ATTN_STATIC				3	// diminish very rapidly with distance

int S_PickReplacement (int refid);
void S_CacheRandomSound (sfxinfo_t *sfx);

// Checks if a copy of this sound is already playing.
bool S_CheckSingular (int sound_id);

// [BB]
void S_StopSoundID (int sound_id, int channel);

// Stops a sound emanating from one of an entity's channels
void S_StopSound (AActor *ent, int channel);
void S_StopSound (fixed_t *pt, int channel);

// Stop sound for all channels
void S_StopAllChannels (void);

// [BB] Stop sound of an actor for all channels
void S_StopAllSoundsFromActor (AActor *ent);

// Is the sound playing on one of the entity's channels?
bool S_GetSoundPlayingInfo (AActor *ent, int sound_id);
bool S_GetSoundPlayingInfo (fixed_t *pt, int sound_id);
bool S_IsActorPlayingSomething (AActor *actor, int channel, int sound_id);

// Moves all sounds from one mobj to another
void S_RelinkSound (AActor *from, AActor *to);

// Start music using <music_name>
bool S_StartMusic (const char *music_name);

// Start music using <music_name>, and set whether looping
bool S_ChangeMusic (const char *music_name, int order=0, bool looping=true, bool force=false);

// Start playing a cd track as music
bool S_ChangeCDMusic (int track, unsigned int id=0, bool looping=true);

void S_RestartMusic ();

int S_GetMusic (char **name);

// Stops the music for sure.
void S_StopMusic (bool force);

// Stop and resume music, during game PAUSE.
void S_PauseSound (bool notmusic);
void S_ResumeSound ();

//
// Updates music & sounds
//
void S_UpdateSounds (void *listener);

// [RH] S_sfx "maintenance" routines
void S_ParseSndInfo ();
void S_ParseReverbDef ();
void S_UnloadReverbDef ();

void S_HashSounds ();
int S_FindSoundNoHash (const char *logicalname);
bool S_AreSoundsEquivalent (AActor *actor, int id1, int id2);
bool S_AreSoundsEquivalent (AActor *actor, const char *name1, const char *name2);
int S_LookupPlayerSound (const char *playerclass, int gender, const char *logicalname);
int S_LookupPlayerSound (const char *playerclass, int gender, FSoundID refid);
int S_FindSkinnedSound (AActor *actor, FSoundID refid);
int S_FindSkinnedSoundEx (AActor *actor, const char *logicalname, const char *extendedname);
int S_FindSoundByLump (int lump);
int S_AddSound (const char *logicalname, const char *lumpname, FScanner *sc=NULL);	// Add sound by lumpname
int S_AddSoundLump (const char *logicalname, int lump);	// Add sound by lump index
int S_AddPlayerSound (const char *playerclass, const int gender, int refid, const char *lumpname);
int S_AddPlayerSound (const char *playerclass, const int gender, int refid, int lumpnum, bool fromskin=false);
int S_AddPlayerSoundExisting (const char *playerclass, const int gender, int refid, int aliasto, bool fromskin=false);
void S_ShrinkPlayerSoundLists ();

// [BC]
const char	*S_GetName( LONG lSoundID );

// [RH] Prints sound debug info to the screen.
//		Modelled after Hexen's noise cheat.
void S_NoiseDebug ();

// For convenience, this structure matches FMOD_REVERB_PROPERTIES.
// Since I can't very well #include system-specific stuff in the
// main game files, I duplicate it here.
struct REVERB_PROPERTIES
{                
	int			 Instance;
    int			 Environment;
    float        EnvSize;
    float        EnvDiffusion;
    int          Room;
    int          RoomHF;
    int          RoomLF;
    float        DecayTime;
    float        DecayHFRatio;
    float        DecayLFRatio;
    int          Reflections;
    float        ReflectionsDelay;
    float        ReflectionsPan0;
	float        ReflectionsPan1;
	float        ReflectionsPan2;
    int          Reverb;
    float        ReverbDelay;
    float        ReverbPan0;
	float        ReverbPan1;
	float        ReverbPan2;
    float        EchoTime;
    float        EchoDepth;
    float        ModulationTime;
    float        ModulationDepth;
    float        AirAbsorptionHF;
    float        HFReference;
    float        LFReference;
    float        RoomRolloffFactor;
    float        Diffusion;
    float        Density;
    unsigned int Flags;
};

#define REVERB_FLAGS_DECAYTIMESCALE        0x00000001
#define REVERB_FLAGS_REFLECTIONSSCALE      0x00000002
#define REVERB_FLAGS_REFLECTIONSDELAYSCALE 0x00000004
#define REVERB_FLAGS_REVERBSCALE           0x00000008
#define REVERB_FLAGS_REVERBDELAYSCALE      0x00000010
#define REVERB_FLAGS_DECAYHFLIMIT          0x00000020
#define REVERB_FLAGS_ECHOTIMESCALE         0x00000040
#define REVERB_FLAGS_MODULATIONTIMESCALE   0x00000080

struct ReverbContainer
{
	ReverbContainer *Next;
	const char *Name;
	WORD ID;
	bool Builtin;
	bool Modified;
	REVERB_PROPERTIES Properties;
	bool SoftwareWater;
};

extern ReverbContainer *Environments;
extern ReverbContainer *DefaultEnvironments[26];

class FArchive;
FArchive &operator<< (FArchive &arc, ReverbContainer *&env);

void S_SetEnvironment (const ReverbContainer *settings);
ReverbContainer *S_FindEnvironment (const char *name);
ReverbContainer *S_FindEnvironment (int id);
void S_AddEnvironment (ReverbContainer *settings);

enum EMidiDevice
{
	MDEV_DEFAULT = -1,
	MDEV_MMAPI = 0,
	MDEV_TIMIDITY = 1,
	MDEV_OPL = 2,
	MDEV_FMOD = 3,
};

typedef TMap<FName, int> MidiDeviceMap;

extern MidiDeviceMap MidiDevices;

#endif
