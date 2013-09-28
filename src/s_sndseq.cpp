//**************************************************************************
//**
//** sn_sonix.c : Heretic 2 : Raven Software, Corp.
//**
//** $RCSfile: sn_sonix.c,v $
//** $Revision: 1.17 $
//** $Date: 95/10/05 18:25:44 $
//** $Author: paul $
//**
//**************************************************************************

#include <string.h>
#include <stdio.h>

#include "doomtype.h"
#include "doomstat.h"
#include "sc_man.h"
#include "m_random.h"
#include "s_sound.h"
#include "s_sndseq.h"
#include "w_wad.h"
#include "i_system.h"
#include "cmdlib.h"
#include "p_local.h"
#include "gi.h"
#include "templates.h"
#include "c_dispatch.h"
#include "g_level.h"

// MACROS ------------------------------------------------------------------

#define GetCommand(a)		((a) & 255)
#define GetData(a)			(SDWORD(a) >> 8 )
#define MakeCommand(a,b)	((a) | ((b) << 8))
#define HexenPlatSeq(a)		(a)
#define HexenDoorSeq(a)		((a) | 0x40)
#define HexenEnvSeq(a)		((a) | 0x80)
#define HexenLastSeq		(0xff)

#define TIME_REFERENCE		level.time

// TYPES -------------------------------------------------------------------

typedef enum
{
	SS_STRING_PLAY,
	SS_STRING_PLAYUNTILDONE,
	SS_STRING_PLAYTIME,
	SS_STRING_PLAYREPEAT,
	SS_STRING_PLAYLOOP,
	SS_STRING_DELAY,
	SS_STRING_DELAYONCE,
	SS_STRING_DELAYRAND,
	SS_STRING_VOLUME,
	SS_STRING_VOLUMEREL,
	SS_STRING_VOLUMERAND,
	SS_STRING_END,
	SS_STRING_STOPSOUND,
	SS_STRING_ATTENUATION,
	SS_STRING_NOSTOPCUTOFF,
	SS_STRING_SLOT,
	SS_STRING_RANDOMSEQUENCE,
	SS_STRING_RESTART,
	// These must be last and in the same order as they appear in seqtype_t
	SS_STRING_PLATFORM,
	SS_STRING_DOOR,
	SS_STRING_ENVIRONMENT
} ssstrings_t;

typedef enum
{
	SS_CMD_NONE,
	SS_CMD_PLAY,
	SS_CMD_WAITUNTILDONE, // used by PLAYUNTILDONE
	SS_CMD_PLAYTIME,
	SS_CMD_PLAYREPEAT,
	SS_CMD_PLAYLOOP,
	SS_CMD_DELAY,
	SS_CMD_DELAYRAND,
	SS_CMD_VOLUME,
	SS_CMD_VOLUMEREL,
	SS_CMD_VOLUMERAND,
	SS_CMD_STOPSOUND,
	SS_CMD_ATTENUATION,
	SS_CMD_RANDOMSEQUENCE,
	SS_CMD_BRANCH,
	SS_CMD_LAST2NOP,
	SS_CMD_SELECT,
	SS_CMD_END
} sscmds_t;

struct hexenseq_t
{
	ENamedName	Name;
	BYTE		Seqs[4];
};

class DSeqActorNode : public DSeqNode
{
	DECLARE_CLASS(DSeqActorNode, DSeqNode)
	HAS_OBJECT_POINTERS
public:
	DSeqActorNode(AActor *actor, int sequence, int modenum);
	void Destroy();
	void Serialize(FArchive &arc);
	void MakeSound(int loop, FSoundID id)
	{
		S_Sound(m_Actor, CHAN_BODY|loop, id, clamp(m_Volume, 0.f, 1.f), m_Atten);
	}
	bool IsPlaying()
	{
		return S_IsActorPlayingSomething (m_Actor, CHAN_BODY, m_CurrentSoundID);
	}
	void *Source()
	{
		return m_Actor;
	}
	DSeqNode *SpawnChild(int seqnum)
	{
		return SN_StartSequence (m_Actor, seqnum, SEQ_NOTRANS, m_ModeNum, true);
	}
private:
	DSeqActorNode() {}
	TObjPtr<AActor> m_Actor;
};

class DSeqPolyNode : public DSeqNode
{
	DECLARE_CLASS(DSeqPolyNode, DSeqNode)
public:
	DSeqPolyNode(FPolyObj *poly, int sequence, int modenum);
	void Destroy();
	void Serialize(FArchive &arc);
	void MakeSound(int loop, FSoundID id)
	{
		S_Sound (m_Poly, CHAN_BODY|loop, id, clamp(m_Volume, 0.f, 1.f), m_Atten);
	}
	bool IsPlaying()
	{
		return S_GetSoundPlayingInfo (m_Poly, m_CurrentSoundID);
	}
	void *Source()
	{
		return m_Poly;
	}
	DSeqNode *SpawnChild (int seqnum)
	{
		return SN_StartSequence (m_Poly, seqnum, SEQ_NOTRANS, m_ModeNum, true);
	}
private:
	DSeqPolyNode () {}
	FPolyObj *m_Poly;
};

class DSeqSectorNode : public DSeqNode
{
	DECLARE_CLASS(DSeqSectorNode, DSeqNode)
public:
	DSeqSectorNode(sector_t *sec, int chan, int sequence, int modenum);
	void Destroy();
	void Serialize(FArchive &arc);
	void MakeSound(int loop, FSoundID id)
	{
		Channel = (Channel & 7) | CHAN_AREA | loop;
		S_Sound(m_Sector, Channel, id, clamp(m_Volume, 0.f, 1.f), m_Atten);
	}
	bool IsPlaying()
	{
		return S_GetSoundPlayingInfo (m_Sector, m_CurrentSoundID);
	}
	void *Source()
	{
		return m_Sector;
	}
	DSeqNode *SpawnChild(int seqnum)
	{
		return SN_StartSequence (m_Sector, Channel, seqnum, SEQ_NOTRANS, m_ModeNum, true);
	}
	int Channel;
private:
	DSeqSectorNode() {}
	sector_t *m_Sector;
};

// When destroyed, destroy the sound sequences too.
struct FSoundSequencePtrArray : public TArray<FSoundSequence *>
{
	~FSoundSequencePtrArray()
	{
		for (unsigned int i = 0; i < Size(); ++i)
		{
			M_Free((*this)[i]);
		}
	}
};

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void AssignTranslations (FScanner &sc, int seq, seqtype_t type);
static void AssignHexenTranslations (void);
static void AddSequence (int curseq, FName seqname, FName slot, int stopsound, const TArray<DWORD> &ScriptTemp);
static int FindSequence (const char *searchname);
static int FindSequence (FName seqname);
static bool TwiddleSeqNum (int &sequence, seqtype_t type);

// PUBLIC DATA DEFINITIONS -------------------------------------------------

FSoundSequencePtrArray Sequences;
int ActiveSequences;
DSeqNode *DSeqNode::SequenceListHead;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static const char *SSStrings[] = {
	"play",
	"playuntildone",
	"playtime",
	"playrepeat",
	"playloop",
	"delay",
	"delayonce",
	"delayrand",
	"volume",
	"volumerel",
	"volumerand",
	"end",
	"stopsound",
	"attenuation",
	"nostopcutoff",
	"slot",
	"randomsequence",
	"restart",
	// These must be last and in the same order as they appear in seqtype_t
	"platform",
	"door",
	"environment",
	NULL
};

struct SSAttenuation
{
	const char *name;
	float value;
};

static const SSAttenuation Attenuations[] = {
	{ "none", ATTN_NONE },
	{ "normal", ATTN_NORM }, 
	{ "idle", ATTN_IDLE },
	{ "static", ATTN_STATIC },
	{ "surround", ATTN_NONE },
	{ NULL, 0}
};

static const hexenseq_t HexenSequences[] = {
	{ NAME_Platform,		{ HexenPlatSeq(0), HexenPlatSeq(1), HexenPlatSeq(3), HexenLastSeq } },
	{ NAME_PlatformMetal,	{ HexenPlatSeq(2), HexenLastSeq } },
	{ NAME_Silence,			{ HexenPlatSeq(4), HexenDoorSeq(4), HexenLastSeq } },
	{ NAME_Lava,			{ HexenPlatSeq(5), HexenDoorSeq(5), HexenLastSeq } },
	{ NAME_Water,			{ HexenPlatSeq(6), HexenDoorSeq(6), HexenLastSeq } },
	{ NAME_Ice,				{ HexenPlatSeq(7), HexenDoorSeq(7), HexenLastSeq } },
	{ NAME_Earth,			{ HexenPlatSeq(8), HexenDoorSeq(8), HexenLastSeq } },
	{ NAME_PlatformMetal2,	{ HexenPlatSeq(9), HexenLastSeq } },
	{ NAME_DoorNormal,		{ HexenDoorSeq(0), HexenLastSeq } },
	{ NAME_DoorHeavy,		{ HexenDoorSeq(1), HexenLastSeq } },
	{ NAME_DoorMetal,		{ HexenDoorSeq(2), HexenLastSeq } },
	{ NAME_DoorCreak,		{ HexenDoorSeq(3), HexenLastSeq } },
	{ NAME_DoorMetal2,		{ HexenDoorSeq(9), HexenLastSeq } },
	{ NAME_Wind,			{ HexenEnvSeq(10), HexenLastSeq } },
	{ NAME_None }
};

static int SeqTrans[64*3];

static FRandom pr_sndseq ("SndSeq");

// CODE --------------------------------------------------------------------

void DSeqNode::SerializeSequences (FArchive &arc)
{
	arc << SequenceListHead;
}

IMPLEMENT_POINTY_CLASS (DSeqNode)
 DECLARE_POINTER(m_ChildSeqNode)
 DECLARE_POINTER(m_ParentSeqNode)
 DECLARE_POINTER(m_Next)
 DECLARE_POINTER(m_Prev)
END_POINTERS

DSeqNode::DSeqNode ()
: m_SequenceChoices(0)
{
	m_Next = m_Prev = m_ChildSeqNode = m_ParentSeqNode = NULL;
}

void DSeqNode::Serialize (FArchive &arc)
{
	int seqOffset;
	unsigned int i;

	Super::Serialize (arc);
	if (arc.IsStoring ())
	{
		seqOffset = (int)SN_GetSequenceOffset (m_Sequence, m_SequencePtr);
		arc << seqOffset
			<< m_DelayUntilTic
			<< m_Volume
			<< m_Atten
			<< m_ModeNum
			<< m_Next
			<< m_Prev
			<< m_ChildSeqNode
			<< m_ParentSeqNode
			<< m_CurrentSoundID
			<< Sequences[m_Sequence]->SeqName;

		arc.WriteCount (m_SequenceChoices.Size());
		for (i = 0; i < m_SequenceChoices.Size(); ++i)
		{
			arc << Sequences[m_SequenceChoices[i]]->SeqName;
		}
	}
	else
	{
		FName seqName;
		int delayTics = 0;
		FSoundID id;
		float volume;
		float atten = ATTN_NORM;
		int seqnum;
		unsigned int numchoices;

		arc << seqOffset
			<< delayTics
			<< volume
			<< atten
			<< m_ModeNum
			<< m_Next
			<< m_Prev
			<< m_ChildSeqNode
			<< m_ParentSeqNode
			<< id
			<< seqName;

		seqnum = FindSequence (seqName);
		if (seqnum >= 0)
		{
			ActivateSequence (seqnum);
		}
		else
		{
			I_Error ("Unknown sound sequence '%s'\n", seqName.GetChars());
			// Can I just Destroy() here instead of erroring out?
		}

		ChangeData (seqOffset, delayTics - TIME_REFERENCE, volume, id);

		numchoices = arc.ReadCount();
		m_SequenceChoices.Resize(numchoices);
		for (i = 0; i < numchoices; ++i)
		{
			arc << seqName;
			m_SequenceChoices[i] = FindSequence (seqName);
		}
	}
}

void DSeqNode::Destroy()
{
	// If this sequence was launched by a parent sequence, advance that
	// sequence now.
	if (m_ParentSeqNode != NULL && m_ParentSeqNode->m_ChildSeqNode == this)
	{
		m_ParentSeqNode->m_SequencePtr++;
		m_ParentSeqNode->m_ChildSeqNode = NULL;
		m_ParentSeqNode = NULL;
	}
	if (SequenceListHead == this)
	{
		SequenceListHead = m_Next;
		GC::WriteBarrier(m_Next);
	}
	if (m_Prev)
	{
		m_Prev->m_Next = m_Next;
		GC::WriteBarrier(m_Prev, m_Next);
	}
	if (m_Next)
	{
		m_Next->m_Prev = m_Prev;
		GC::WriteBarrier(m_Next, m_Prev);
	}
	ActiveSequences--;
	Super::Destroy();
}

void DSeqNode::StopAndDestroy ()
{
	if (m_ChildSeqNode != NULL)
	{
		m_ChildSeqNode->StopAndDestroy();
	}
	Destroy();
}

void DSeqNode::AddChoice (int seqnum, seqtype_t type)
{
	if (TwiddleSeqNum (seqnum, type))
	{
		m_SequenceChoices.Push (seqnum);
	}
}

FName DSeqNode::GetSequenceName () const
{
	return Sequences[m_Sequence]->SeqName;
}

IMPLEMENT_POINTY_CLASS (DSeqActorNode)
 DECLARE_POINTER (m_Actor)
END_POINTERS

void DSeqActorNode::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << m_Actor;
}

IMPLEMENT_CLASS (DSeqPolyNode)

void DSeqPolyNode::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << m_Poly;
}

IMPLEMENT_CLASS (DSeqSectorNode)

void DSeqSectorNode::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << m_Sector << Channel;
}

//==========================================================================
//
// AssignTranslations
//
//==========================================================================

static void AssignTranslations (FScanner &sc, int seq, seqtype_t type)
{
	sc.Crossed = false;

	while (sc.GetString() && !sc.Crossed)
	{
		if (IsNum(sc.String))
		{
			SeqTrans[(atoi(sc.String) & 63) + type * 64] = seq;
		}
	}
	sc.UnGet();
}

//==========================================================================
//
// AssignHexenTranslations
//
//==========================================================================

static void AssignHexenTranslations (void)
{
	unsigned int i, j, seq;

	for (i = 0; HexenSequences[i].Name != NAME_None; i++)
	{
		for (seq = 0; seq < Sequences.Size(); seq++)
		{
			if (Sequences[seq] != NULL && HexenSequences[i].Name == Sequences[seq]->SeqName)
				break;
		}
		if (seq == Sequences.Size())
			continue;

		for (j = 0; j < 4 && HexenSequences[i].Seqs[j] != HexenLastSeq; j++)
		{
			int trans;

			if (HexenSequences[i].Seqs[j] & 0x40)
			{
				trans = 64 * SEQ_DOOR;
			}
			else if (HexenSequences[i].Seqs[j] & 0x80)
				trans = 64 * SEQ_ENVIRONMENT;
			else
				trans = 64 * SEQ_PLATFORM;

			SeqTrans[trans + (HexenSequences[i].Seqs[j] & 0x3f)] = seq;
		}
	}
}

//==========================================================================
//
// S_ParseSndSeq
//
//==========================================================================

void S_ParseSndSeq (int levellump)
{
	TArray<DWORD> ScriptTemp;
	int lastlump, lump;
	char seqtype = ':';
	FName seqname;
	FName slot;
	int stopsound;
	int delaybase;
	float volumebase;
	int curseq = -1;
	fixed_t val;

	// First free the old SNDSEQ data. This allows us to reload this for each level
	// and specify a level specific SNDSEQ lump!
	for (unsigned int i = 0; i < Sequences.Size(); i++)
	{
		if (Sequences[i])
		{
			M_Free(Sequences[i]);
		}
	}
	Sequences.Clear();

	// be gone, compiler warnings
	stopsound = 0;

	memset (SeqTrans, -1, sizeof(SeqTrans));
	lastlump = 0;

	while (((lump = Wads.FindLump ("SNDSEQ", &lastlump)) != -1 || levellump != -1) && levellump != -2)
	{
		if (lump == -1)
		{
			lump = levellump;
			levellump = -2;
		}
		FScanner sc(lump);
		while (sc.GetString ())
		{
			if (*sc.String == ':' || *sc.String == '[')
			{
				if (curseq != -1)
				{
					sc.ScriptError ("S_ParseSndSeq: Nested Script Error");
				}
				seqname = sc.String + 1;
				seqtype = sc.String[0];
				for (curseq = 0; curseq < (int)Sequences.Size(); curseq++)
				{
					if (Sequences[curseq] != NULL && Sequences[curseq]->SeqName == seqname)
					{
						M_Free (Sequences[curseq]);
						Sequences[curseq] = NULL;
						break;
					}
				}
				if (curseq == (int)Sequences.Size())
				{
					Sequences.Push (NULL);
				}
				ScriptTemp.Clear();
				stopsound = 0;
				slot = NAME_None;
				if (seqtype == '[')
				{
					sc.SetCMode (true);
					ScriptTemp.Push (0);	// to be filled when definition is complete
				}
				continue;
			}
			if (curseq == -1)
			{
				continue;
			}
			if (seqtype == '[')
			{
				if (sc.String[0] == ']')
				{ // End of this definition
					ScriptTemp[0] = MakeCommand(SS_CMD_SELECT, (ScriptTemp.Size()-1)/2);
					AddSequence (curseq, seqname, slot, stopsound, ScriptTemp);
					curseq = -1;
					sc.SetCMode (false);
				}
				else
				{ // Add a selection
					sc.UnGet();
					if (sc.CheckNumber())
					{
						ScriptTemp.Push (sc.Number);
						sc.MustGetString();
						ScriptTemp.Push (FName(sc.String));
					}
					else
					{
						seqtype_t seqtype = seqtype_t(sc.MustMatchString (SSStrings + SS_STRING_PLATFORM));
						AssignTranslations (sc, curseq, seqtype);
					}
				}
				continue;
			}
			switch (sc.MustMatchString (SSStrings))
			{
				case SS_STRING_PLAYUNTILDONE:
					sc.MustGetString ();
					ScriptTemp.Push(MakeCommand(SS_CMD_PLAY, S_FindSound (sc.String)));
					ScriptTemp.Push(MakeCommand(SS_CMD_WAITUNTILDONE, 0));
					break;

				case SS_STRING_PLAY:
					sc.MustGetString ();
					ScriptTemp.Push(MakeCommand(SS_CMD_PLAY, S_FindSound (sc.String)));
					break;

				case SS_STRING_PLAYTIME:
					sc.MustGetString ();
					ScriptTemp.Push(MakeCommand(SS_CMD_PLAY, S_FindSound (sc.String)));
					sc.MustGetNumber ();
					ScriptTemp.Push(MakeCommand(SS_CMD_DELAY, sc.Number));
					break;

				case SS_STRING_PLAYREPEAT:
					sc.MustGetString ();
					ScriptTemp.Push(MakeCommand (SS_CMD_PLAYREPEAT, S_FindSound (sc.String)));
					break;

				case SS_STRING_PLAYLOOP:
					sc.MustGetString ();
					ScriptTemp.Push(MakeCommand (SS_CMD_PLAYLOOP, S_FindSound (sc.String)));
					sc.MustGetNumber ();
					ScriptTemp.Push(sc.Number);
					break;

				case SS_STRING_DELAY:
					sc.MustGetNumber ();
					ScriptTemp.Push(MakeCommand(SS_CMD_DELAY, sc.Number));
					break;

				case SS_STRING_DELAYONCE:
					sc.MustGetNumber ();
					ScriptTemp.Push(MakeCommand(SS_CMD_DELAY, sc.Number));
					ScriptTemp.Push(MakeCommand(SS_CMD_LAST2NOP, 0));
					break;

				case SS_STRING_DELAYRAND:
					sc.MustGetNumber ();
					delaybase = sc.Number;
					ScriptTemp.Push(MakeCommand(SS_CMD_DELAYRAND, sc.Number));
					sc.MustGetNumber ();
					ScriptTemp.Push(MAX(1, sc.Number - delaybase + 1));
					break;

				case SS_STRING_VOLUME:		// volume is in range 0..100
					sc.MustGetFloat ();
					ScriptTemp.Push(MakeCommand(SS_CMD_VOLUME, int(sc.Float * (FRACUNIT/100.f))));
					break;

				case SS_STRING_VOLUMEREL:
					sc.MustGetFloat ();
					ScriptTemp.Push(MakeCommand(SS_CMD_VOLUMEREL, int(sc.Float * (FRACUNIT/100.f))));
					break;

				case SS_STRING_VOLUMERAND:
					sc.MustGetFloat ();
					volumebase = float(sc.Float);
					ScriptTemp.Push(MakeCommand(SS_CMD_VOLUMERAND, int(sc.Float * (FRACUNIT/100.f))));
					sc.MustGetFloat ();
					ScriptTemp.Push(int((sc.Float - volumebase) * (256/100.f)));
					break;

				case SS_STRING_STOPSOUND:
					sc.MustGetString ();
					stopsound = S_FindSound (sc.String);
					ScriptTemp.Push(MakeCommand(SS_CMD_STOPSOUND, 0));
					break;

				case SS_STRING_NOSTOPCUTOFF:
					stopsound = -1;
					ScriptTemp.Push(MakeCommand(SS_CMD_STOPSOUND, 0));
					break;

				case SS_STRING_ATTENUATION:
					if (sc.CheckFloat())
					{
						val = FLOAT2FIXED(sc.Float);
					}
					else
					{
						sc.MustGetString ();
						val = sc.MustMatchString(&Attenuations[0].name, sizeof(Attenuations[0])) << FRACBITS;
					}
					ScriptTemp.Push(MakeCommand(SS_CMD_ATTENUATION, val));
					break;

				case SS_STRING_RANDOMSEQUENCE:
					ScriptTemp.Push(MakeCommand(SS_CMD_RANDOMSEQUENCE, 0));
					break;

				case SS_STRING_RESTART:
					ScriptTemp.Push(MakeCommand(SS_CMD_BRANCH, ScriptTemp.Size()));
					break;

				case SS_STRING_END:
					AddSequence (curseq, seqname, slot, stopsound, ScriptTemp);
					curseq = -1;
					break;

				case SS_STRING_PLATFORM:
					AssignTranslations (sc, curseq, SEQ_PLATFORM);
					break;

				case SS_STRING_DOOR:
					AssignTranslations (sc, curseq, SEQ_DOOR);
					break;

				case SS_STRING_ENVIRONMENT:
					AssignTranslations (sc, curseq, SEQ_ENVIRONMENT);
					break;

				case SS_STRING_SLOT:
					sc.MustGetString();
					slot = sc.String;
					break;
			}
		}
		if (curseq > 0)
		{
			sc.ScriptError("End of file encountered before the final sequence ended.");
		}
	}

	if (gameinfo.gametype == GAME_Hexen)
		AssignHexenTranslations ();
}

static void AddSequence (int curseq, FName seqname, FName slot, int stopsound, const TArray<DWORD> &ScriptTemp)
{
	Sequences[curseq] = (FSoundSequence *)M_Malloc (sizeof(FSoundSequence) + sizeof(DWORD)*ScriptTemp.Size());
	Sequences[curseq]->SeqName = seqname;
	Sequences[curseq]->Slot = slot;
	Sequences[curseq]->StopSound = stopsound;
	memcpy (Sequences[curseq]->Script, &ScriptTemp[0], sizeof(DWORD)*ScriptTemp.Size());
	Sequences[curseq]->Script[ScriptTemp.Size()] = MakeCommand(SS_CMD_END, 0);
}

DSeqNode::DSeqNode (int sequence, int modenum)
: m_ModeNum(modenum), m_SequenceChoices(0)
{
	ActivateSequence (sequence);
	if (!SequenceListHead)
	{
		SequenceListHead = this;
		m_Next = m_Prev = NULL;
	}
	else
	{
		SequenceListHead->m_Prev = this;		GC::WriteBarrier(SequenceListHead->m_Prev, this);
		m_Next = SequenceListHead;				GC::WriteBarrier(this, SequenceListHead);
		SequenceListHead = this;
		m_Prev = NULL;
	}
	GC::WriteBarrier(this);
	m_ParentSeqNode = m_ChildSeqNode = NULL;
}

void DSeqNode::ActivateSequence (int sequence)
{
	m_SequencePtr = Sequences[sequence]->Script;
	m_Sequence = sequence;
	m_DelayUntilTic = 0;
	m_StopSound = Sequences[sequence]->StopSound;
	m_CurrentSoundID = 0;
	m_Volume = 1;			// Start at max volume...
	m_Atten = ATTN_IDLE;	// ...and idle attenuation

	ActiveSequences++;
}

DSeqActorNode::DSeqActorNode (AActor *actor, int sequence, int modenum)
	: DSeqNode (sequence, modenum),
	  m_Actor (actor)
{
}

DSeqPolyNode::DSeqPolyNode (FPolyObj *poly, int sequence, int modenum)
	: DSeqNode (sequence, modenum),
	  m_Poly (poly)
{
}

DSeqSectorNode::DSeqSectorNode (sector_t *sec, int chan, int sequence, int modenum)
	: DSeqNode (sequence, modenum),
	  Channel (chan),
	  m_Sector (sec)
{
}

//==========================================================================
//
//  SN_StartSequence
//
//==========================================================================

static bool TwiddleSeqNum (int &sequence, seqtype_t type)
{
	if (type < SEQ_NUMSEQTYPES)
	{
		// [GrafZahl] Needs some range checking:
		// Sector_ChangeSound doesn't do it so this makes invalid sequences play nothing.
		if (sequence >= 0 && sequence < 64)
		{
			sequence = SeqTrans[sequence + type * 64];
		}
		else
		{
			return false;
		}
	}

	// [BC] Added check since servers don't load sequences.
	if (sequence == -1 || ( static_cast<unsigned> (sequence) >= Sequences.Size( )) || Sequences[sequence] == NULL)
		return false;
	else
		return true;
}

DSeqNode *SN_StartSequence (AActor *actor, int sequence, seqtype_t type, int modenum, bool nostop)
{
	if (!nostop)
	{
		SN_StopSequence (actor); // Stop any previous sequence
	}
	if (TwiddleSeqNum (sequence, type))
	{
		return new DSeqActorNode (actor, sequence, modenum);
	}
	return NULL;
}

DSeqNode *SN_StartSequence (sector_t *sector, int chan, int sequence, seqtype_t type, int modenum, bool nostop)
{
	if (!nostop)
	{
		SN_StopSequence (sector, chan);
	}
	if (TwiddleSeqNum (sequence, type))
	{
		return new DSeqSectorNode (sector, chan, sequence, modenum);
	}
	return NULL;
}

DSeqNode *SN_StartSequence (FPolyObj *poly, int sequence, seqtype_t type, int modenum, bool nostop)
{
	if (!nostop)
	{
		SN_StopSequence (poly);
	}
	if (TwiddleSeqNum (sequence, type))
	{
		return new DSeqPolyNode (poly, sequence, modenum);
	}
	return NULL;
}

//==========================================================================
//
//  SN_StartSequence (named)
//
//==========================================================================

DSeqNode *SN_StartSequence (AActor *actor, const char *seqname, int modenum)
{
	int seqnum = FindSequence (seqname);
	if (seqnum >= 0)
	{
		return SN_StartSequence (actor, seqnum, SEQ_NOTRANS, modenum);
	}
	return NULL;
}

DSeqNode *SN_StartSequence (AActor *actor, FName seqname, int modenum)
{
	int seqnum = FindSequence (seqname);
	if (seqnum >= 0)
	{
		return SN_StartSequence (actor, seqnum, SEQ_NOTRANS, modenum);
	}
	return NULL;
}

DSeqNode *SN_StartSequence (sector_t *sec, int chan, const char *seqname, int modenum)
{
	int seqnum = FindSequence (seqname);
	if (seqnum >= 0)
	{
		return SN_StartSequence (sec, chan, seqnum, SEQ_NOTRANS, modenum);
	}
	return NULL;
}

DSeqNode *SN_StartSequence (sector_t *sec, int chan, FName seqname, int modenum)
{
	int seqnum = FindSequence (seqname);
	if (seqnum >= 0)
	{
		return SN_StartSequence (sec, chan, seqnum, SEQ_NOTRANS, modenum);
	}
	return NULL;
}

DSeqNode *SN_StartSequence (FPolyObj *poly, const char *seqname, int modenum)
{
	int seqnum = FindSequence (seqname);
	if (seqnum >= 0)
	{
		return SN_StartSequence (poly, seqnum, SEQ_NOTRANS, modenum);
	}
	return NULL;
}

static int FindSequence (const char *searchname)
{
	FName seqname (searchname, true);

	if (seqname != NAME_None)
	{
		return FindSequence (seqname);
	}
	return -1;
}

static int FindSequence (FName seqname)
{
	int i;

	for (i = Sequences.Size(); i-- > 0; )
	{
		if (Sequences[i] != NULL && seqname == Sequences[i]->SeqName)
		{
			return i;
		}
	}
	return -1;
}

//==========================================================================
//
//  SN_StopSequence
//
//==========================================================================

void SN_StopSequence (AActor *actor)
{
	SN_DoStop (actor);
}

void SN_StopSequence (sector_t *sector, int chan)
{
	DSeqNode *node;

	for (node = DSeqNode::FirstSequence(); node; )
	{
		DSeqNode *next = node->NextSequence();
		if (node->Source() == sector)
		{
			assert(node->IsKindOf(RUNTIME_CLASS(DSeqSectorNode)));
			if ((static_cast<DSeqSectorNode *>(node)->Channel & 7) == chan)
			{
				node->StopAndDestroy ();
			}
		}
		node = next;
	}
}

void SN_StopSequence (FPolyObj *poly)
{
	SN_DoStop (poly);
}

void SN_DoStop (void *source)
{
	DSeqNode *node;

	for (node = DSeqNode::FirstSequence(); node; )
	{
		DSeqNode *next = node->NextSequence();
		if (node->Source() == source)
		{
			node->StopAndDestroy ();
		}
		node = next;
	}
}

void DSeqActorNode::Destroy ()
{
	if (m_StopSound >= 0)
		S_StopSound (m_Actor, CHAN_BODY);
	if (m_StopSound >= 1)
		MakeSound (0, m_StopSound);
	Super::Destroy();
}

void DSeqSectorNode::Destroy ()
{
	if (m_StopSound >= 0)
		S_StopSound (m_Sector, Channel & 7);
	if (m_StopSound >= 1)
		MakeSound (0, m_StopSound);
	Super::Destroy();
}

void DSeqPolyNode::Destroy ()
{
	if (m_StopSound >= 0)
		S_StopSound (m_Poly, CHAN_BODY);
	if (m_StopSound >= 1)
		MakeSound (0, m_StopSound);
	Super::Destroy();
}

//==========================================================================
//
//  SN_IsMakingLoopingSound
//
//==========================================================================

bool SN_IsMakingLoopingSound (sector_t *sector)
{
	DSeqNode *node;

	for (node = DSeqNode::FirstSequence (); node; )
	{
		DSeqNode *next = node->NextSequence();
		if (node->Source() == (void *)sector)
		{
			return !!(static_cast<DSeqSectorNode *>(node)->Channel & CHAN_LOOP);
		}
		node = next;
	}
	return false;
}

//==========================================================================
//
//  SN_UpdateActiveSequences
//
//==========================================================================

void DSeqNode::Tick ()
{
	if (TIME_REFERENCE < m_DelayUntilTic)
	{
		return;
	}
	for (;;)
	{
		switch (GetCommand(*m_SequencePtr))
		{
		case SS_CMD_NONE:
			m_SequencePtr++;
			break;

		case SS_CMD_PLAY:
			if (!IsPlaying())
			{
				m_CurrentSoundID = FSoundID(GetData(*m_SequencePtr));
				MakeSound (0, m_CurrentSoundID);
			}
			m_SequencePtr++;
			break;

		case SS_CMD_WAITUNTILDONE:
			if (!IsPlaying())
			{
				m_SequencePtr++;
				m_CurrentSoundID = 0;
			}
			else
			{
				return;
			}
			break;

		case SS_CMD_PLAYREPEAT:
			if (!IsPlaying())
			{
				// Does not advance sequencePtr, so it will repeat as necessary.
				m_CurrentSoundID = FSoundID(GetData(*m_SequencePtr));
				MakeSound (CHAN_LOOP, m_CurrentSoundID);
			}
			return;

		case SS_CMD_PLAYLOOP:
			// Like SS_CMD_PLAYREPEAT, sequencePtr is not advanced, so this
			// command will repeat until the sequence is stopped.
			m_CurrentSoundID = FSoundID(GetData(m_SequencePtr[0]));
			MakeSound (0, m_CurrentSoundID);
			m_DelayUntilTic = TIME_REFERENCE + m_SequencePtr[1];
			return;

		case SS_CMD_RANDOMSEQUENCE:
			// If there's nothing to choose from, then there's nothing to do here.
			if (m_SequenceChoices.Size() == 0)
			{
				m_SequencePtr++;
			}
			else if (m_ChildSeqNode == NULL)
			{
				int choice = pr_sndseq() % m_SequenceChoices.Size();
				m_ChildSeqNode = SpawnChild (m_SequenceChoices[choice]);
				GC::WriteBarrier(this, m_ChildSeqNode);
				if (m_ChildSeqNode == NULL)
				{ // Failed, so skip to next instruction.
					m_SequencePtr++;
				}
				else
				{ // Copy parameters to the child and link it to this one.
					m_ChildSeqNode->m_Volume = m_Volume;
					m_ChildSeqNode->m_Atten = m_Atten;
					m_ChildSeqNode->m_ParentSeqNode = this;
					return;
				}
			}
			else
			{
				// If we get here, then the child sequence is playing, and it
				// will advance our sequence pointer for us when it finishes.
				return;
			}
			break;

		case SS_CMD_DELAY:
			m_DelayUntilTic = TIME_REFERENCE + GetData(*m_SequencePtr);
			m_SequencePtr++;
			m_CurrentSoundID = 0;
			return;

		case SS_CMD_DELAYRAND:
			m_DelayUntilTic = TIME_REFERENCE + GetData(m_SequencePtr[0]) + pr_sndseq(m_SequencePtr[1]);
			m_SequencePtr += 2;
			m_CurrentSoundID = 0;
			return;

		case SS_CMD_VOLUME:
			m_Volume = GetData(*m_SequencePtr) / float(FRACUNIT);
			m_SequencePtr++;
			break;

		case SS_CMD_VOLUMEREL:
			// like SS_CMD_VOLUME, but the new volume is added to the old volume
			m_Volume += GetData(*m_SequencePtr) / float(FRACUNIT);
			m_SequencePtr++;
			break;

		case SS_CMD_VOLUMERAND:
			// like SS_CMD_VOLUME, but the new volume is chosen randomly from a range
			m_Volume = GetData(m_SequencePtr[0]) / float(FRACUNIT) + (pr_sndseq() % m_SequencePtr[1]) / 255.f;
			m_SequencePtr += 2;
			break;

		case SS_CMD_STOPSOUND:
			// Wait until something else stops the sequence
			return;

		case SS_CMD_ATTENUATION:
			m_Atten = FIXED2FLOAT(GetData(*m_SequencePtr));
			m_SequencePtr++;
			break;

		case SS_CMD_BRANCH:
			m_SequencePtr -= GetData(*m_SequencePtr);
			break;

		case SS_CMD_SELECT:
			{ // Completely transfer control to the choice matching m_ModeNum.
			  // If no match is found, then just advance to the next command
			  // in this sequence, which should be SS_CMD_END.
				int numchoices = GetData(*m_SequencePtr++);
				int i;

				for (i = 0; i < numchoices; ++i)
				{
					if (m_SequencePtr[i*2] == m_ModeNum)
					{
						int seqnum = FindSequence (ENamedName(m_SequencePtr[i*2+1]));
						if (seqnum >= 0)
						{ // Found a match, and it's a good one too.
							ActiveSequences--;
							ActivateSequence (seqnum);
							break;
						}
					}
				}
				if (i == numchoices)
				{ // No match (or no good match) was found.
					m_SequencePtr += numchoices * 2;
				}
			}
			break;

		case SS_CMD_LAST2NOP:
			*(m_SequencePtr - 1) = MakeCommand(SS_CMD_NONE, 0);
			*m_SequencePtr = MakeCommand(SS_CMD_NONE, 0);
			m_SequencePtr++;
			break;

		case SS_CMD_END:
			Destroy ();
			return;

		default:
			Printf ("Corrupted sound sequence: %s\n", Sequences[m_Sequence]->SeqName.GetChars());
			Destroy ();
			return;
		}
	}
}

void SN_UpdateActiveSequences (void)
{
	DSeqNode *node;

	if (!ActiveSequences || paused)
	{ // No sequences currently playing/game is paused
		return;
	}
	for (node = DSeqNode::FirstSequence(); node; node = node->NextSequence())
	{
		node->Tick ();
	}
}

//==========================================================================
//
//  SN_StopAllSequences
//
//==========================================================================

void SN_StopAllSequences (void)
{
	DSeqNode *node;

	for (node = DSeqNode::FirstSequence(); node; )
	{
		DSeqNode *next = node->NextSequence();
		node->m_StopSound = 0; // don't play any stop sounds
		node->Destroy ();
		node = next;
	}
}
	
//==========================================================================
//
//  SN_GetSequenceOffset
//
//==========================================================================

ptrdiff_t SN_GetSequenceOffset (int sequence, SDWORD *sequencePtr)
{
	return sequencePtr - Sequences[sequence]->Script;
}

//==========================================================================
//
//  SN_GetSequenceSlot
//
//==========================================================================

FName SN_GetSequenceSlot (int sequence, seqtype_t type)
{
	if (TwiddleSeqNum (sequence, type))
	{
		return Sequences[sequence]->Slot;
	}
	return NAME_None;
}

//==========================================================================
//
//  SN_ChangeNodeData
//
// 	nodeNum zero is the first node
//==========================================================================

void SN_ChangeNodeData (int nodeNum, int seqOffset, int delayTics, float volume,
	int currentSoundID)
{
	int i;
	DSeqNode *node;

	i = 0;
	node = DSeqNode::FirstSequence();
	while (node && i < nodeNum)
	{
		node = node->NextSequence();
		i++;
	}
	if (!node)
	{ // reached the end of the list before finding the nodeNum-th node
		return;
	}
	node->ChangeData (seqOffset, delayTics, volume, currentSoundID);
}

void DSeqNode::ChangeData (int seqOffset, int delayTics, float volume, FSoundID currentSoundID)
{
	m_DelayUntilTic = TIME_REFERENCE + delayTics;
	m_Volume = volume;
	m_SequencePtr += seqOffset;
	m_CurrentSoundID = currentSoundID;
}

//==========================================================================
//
// CCMD playsequence
//
// Causes the player to play a sound sequence.
//==========================================================================

CCMD(playsequence)
{
	if (argv.argc() < 2 || argv.argc() > 3)
	{
		Printf ("Usage: playsequence <sound sequence name> [choice number]\n");
	}
	else
	{
		SN_StartSequence (players[consoleplayer].mo, argv[1], argv.argc() > 2 ? atoi(argv[2]) : 0);
	}
}
