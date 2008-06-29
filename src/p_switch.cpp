/*
** p_switch.cpp
** Switch and button maintenance and animation
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
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

#include "templates.h"
#include "i_system.h"
#include "doomdef.h"
#include "p_local.h"
#include "p_lnspec.h"
#include "m_random.h"
#include "g_game.h"
#include "s_sound.h"
#include "doomstat.h"
#include "r_state.h"
#include "w_wad.h"
#include "tarray.h"
#include "cmdlib.h"
#include "sc_man.h"
// [BC] New #includes.
#include "cl_demo.h"
#include "deathmatch.h"
#include "network.h"
#include "sv_commands.h"

#include "gi.h"

#define MAX_FRAMES 128

static FRandom pr_switchanim ("AnimSwitch");

class DActiveButton : public DThinker
{
	DECLARE_CLASS (DActiveButton, DThinker)
public:
	enum EWhere
	{
		BUTTON_Top,
		BUTTON_Middle,
		BUTTON_Bottom,
		BUTTON_Nowhere
	};

	DActiveButton ();
	DActiveButton (side_t *, EWhere, WORD switchnum, fixed_t x, fixed_t y, bool flippable);

	void Serialize (FArchive &arc);
	void Tick ();

	side_t	*m_Side;
	EWhere	m_Where;
	WORD	m_SwitchDef;
	WORD	m_Frame;
	WORD	m_Timer;
	bool	bFlippable;
	fixed_t	m_X, m_Y;	// Location of timer sound

protected:
	bool AdvanceFrame ();
	void StoreTexture (short tex) const;

	friend FArchive &operator<< (FArchive &arc, EWhere &where)
	{
		BYTE val = (BYTE)where;
		arc << val;
		where = (EWhere)val;
		return arc;
	}
};

struct FSwitchDef
{
	SWORD PreTexture;	// texture to switch from
	WORD PairIndex;		// switch def to use to return to PreTexture
	SWORD Sound;		// sound to play at start of animation
	WORD NumFrames;		// # of animation frames
	bool QuestPanel;	// Special texture for Strife mission
	union				// Array of times followed by array of textures
	{					//   actual length of each array is <NumFrames>
		DWORD Times[1];
		SWORD Textures[3];
	} u;
};

static int STACK_ARGS SortSwitchDefs (const void *a, const void *b);
static FSwitchDef *ParseSwitchDef (bool ignoreBad);
static WORD AddSwitchDef (FSwitchDef *def);

//
// CHANGE THE TEXTURE OF A WALL SWITCH TO ITS OPPOSITE
//

class DeletingSwitchArray : public TArray<FSwitchDef *>
{
public:
	~DeletingSwitchArray()
	{
		for(unsigned i=0;i<Size();i++)
		{
			if ((*this)[i] != NULL) free((*this)[i]);
			(*this)[i]=NULL;
		}
	}
};
static DeletingSwitchArray SwitchList;

//
// P_InitSwitchList
// Only called at game initialization.
//
// [RH] Rewritten to use a BOOM-style SWITCHES lump and remove the
//		MAXSWITCHES limit.
void P_InitSwitchList ()
{
	const BITFIELD texflags = FTextureManager::TEXMAN_Overridable | FTextureManager::TEXMAN_TryAny;
	int lump = Wads.CheckNumForName ("SWITCHES");
	FSwitchDef **origMap;
	int i, j;

	if (lump != -1)
	{
		FMemLump lumpdata = Wads.ReadLump (lump);
		const char *alphSwitchList = (const char *)lumpdata.GetMem();
		const char *list_p;
		FSwitchDef *def1, *def2;

		for (list_p = alphSwitchList; list_p[18] || list_p[19]; list_p += 20)
		{
			// [RH] Check for switches that aren't really switches
			if (stricmp (list_p, list_p+9) == 0)
			{
				Printf ("Switch %s in SWITCHES has the same 'on' state\n", list_p);
				continue;
			}
			// [RH] Skip this switch if its texture can't be found.
			if (((gameinfo.maxSwitch & 15) >= (list_p[18] & 15)) &&
				((gameinfo.maxSwitch & ~15) == (list_p[18] & ~15)) &&
				TexMan.CheckForTexture (list_p /* .name1 */, FTexture::TEX_Wall, texflags) >= 0)
			{
				def1 = (FSwitchDef *)M_Malloc (sizeof(FSwitchDef));
				def2 = (FSwitchDef *)M_Malloc (sizeof(FSwitchDef));
				def1->PreTexture = def2->u.Textures[2] = TexMan.CheckForTexture (list_p /* .name1 */, FTexture::TEX_Wall, texflags);
				def2->PreTexture = def1->u.Textures[2] = TexMan.CheckForTexture (list_p + 9, FTexture::TEX_Wall, texflags);
				def1->Sound = def2->Sound = 0;
				def1->NumFrames = def2->NumFrames = 1;
				def1->u.Times[0] = def2->u.Times[0] = 0;
				def2->PairIndex = AddSwitchDef (def1);
				def1->PairIndex = AddSwitchDef (def2);
			}
		}
	}

	SwitchList.ShrinkToFit ();

	// Sort SwitchList for quick searching
	origMap = new FSwitchDef *[SwitchList.Size ()];
	for (i = 0; i < (int)SwitchList.Size (); i++)
	{
		origMap[i] = SwitchList[i];
	}

	qsort (&SwitchList[0], i, sizeof(FSwitchDef *), SortSwitchDefs);

	// Correct the PairIndex of each switch def, since the sorting broke them
	for (i = (int)(SwitchList.Size () - 1); i >= 0; i--)
	{
		FSwitchDef *def = SwitchList[i];
		if (def->PairIndex != 65535)
		{
			for (j = (int)(SwitchList.Size () - 1); j >= 0; j--)
			{
				if (SwitchList[j] == origMap[def->PairIndex])
				{
					def->PairIndex = (WORD)j;
					break;
				}
			}
		}
	}

	delete[] origMap;
}

static int STACK_ARGS SortSwitchDefs (const void *a, const void *b)
{
	return (*(FSwitchDef **)a)->PreTexture - (*(FSwitchDef **)b)->PreTexture;
}

// Parse a switch block in ANIMDEFS and add the definitions to SwitchList
void P_ProcessSwitchDef ()
{
	const BITFIELD texflags = FTextureManager::TEXMAN_Overridable | FTextureManager::TEXMAN_TryAny;
	char *picname;
	FSwitchDef *def1, *def2;
	SWORD picnum;
	BYTE max;
	bool quest = false;

	def1 = def2 = NULL;
	SC_MustGetString ();
	if (SC_Compare ("doom"))
	{
		max = 0;
	}
	else if (SC_Compare ("heretic"))
	{
		max = 17;
	}
	else if (SC_Compare ("hexen"))
	{
		max = 33;
	}
	else if (SC_Compare ("strife"))
	{
		max = 49;
	}
	else if (SC_Compare ("any"))
	{
		max = 240;
	}
	else
	{
		//SC_ScriptError ("Unknown game");
		// There is no game specified; just treat as any
		max = 240;
		SC_UnGet ();
	}
	if (max == 0)
	{
		SC_MustGetNumber ();
		max |= sc_Number & 15;
	}
	SC_MustGetString ();
	picnum = TexMan.CheckForTexture (sc_String, FTexture::TEX_Wall, texflags);
	picname = copystring (sc_String);
	while (SC_GetString ())
	{
		if (SC_Compare ("quest"))
		{
			quest = true;
		}
		else if (SC_Compare ("on"))
		{
			if (def1 != NULL)
			{
				SC_ScriptError ("Switch already has an on state");
			}
			def1 = ParseSwitchDef (picnum == -1);
		}
		else if (SC_Compare ("off"))
		{
			if (def2 != NULL)
			{
				SC_ScriptError ("Switch already has an off state");
			}
			def2 = ParseSwitchDef (picnum == -1);
		}
		else
		{
			SC_UnGet ();
			break;
		}
	}
/*
	if (def1 == NULL)
	{
		SC_ScriptError ("Switch must have an on state");
	}
*/
	if (def1 == NULL || picnum == -1 ||
		((max & 240) != 240 &&
		 ((gameinfo.maxSwitch & 240) != (max & 240) ||
		  (gameinfo.maxSwitch & 15) < (max & 15))))
	{
		if (def2 != NULL)
		{
			free (def2);
		}
		if (def1 != NULL)
		{
			free (def1);
		}
		delete [] picname;
		return;
	}

	// If the switch did not have an off state, create one that just returns
	// it to the original texture without doing anything interesting
	if (def2 == NULL)
	{
		def2 = (FSwitchDef *)M_Malloc (sizeof(FSwitchDef));
		def2->Sound = def1->Sound;
		def2->NumFrames = 1;
		def2->u.Times[0] = 0;
		def2->u.Textures[2] = picnum;
	}

	def1->PreTexture = picnum;
	def2->PreTexture = def1->u.Textures[def1->NumFrames*2+def1->NumFrames-1];
	if (def1->PreTexture == def2->PreTexture)
	{
		SC_ScriptError ("The on state for switch %s must end with a texture other than %s", picname, picname);
	}
	def2->PairIndex = AddSwitchDef (def1);
	def1->PairIndex = AddSwitchDef (def2);
	def1->QuestPanel = def2->QuestPanel = quest;
	delete [] picname;
}

FSwitchDef *ParseSwitchDef (bool ignoreBad)
{
	const BITFIELD texflags = FTextureManager::TEXMAN_Overridable | FTextureManager::TEXMAN_TryAny;
	FSwitchDef *def;
	SWORD pics[MAX_FRAMES];
	DWORD times[MAX_FRAMES];
	int numframes;
	int picnum;
	bool bad;
	SWORD sound;

	numframes = 0;
	sound = 0;
	bad = false;

	while (SC_GetString ())
	{
		if (SC_Compare ("sound"))
		{
			if (sound != 0)
			{
				SC_ScriptError ("Switch state already has a sound");
			}
			SC_MustGetString ();
			sound = S_FindSound (sc_String);
		}
		else if (SC_Compare ("pic"))
		{
			if (numframes == MAX_FRAMES)
			{
				SC_ScriptError ("Switch has too many frames");
			}
			SC_MustGetString ();
			picnum = TexMan.CheckForTexture (sc_String, FTexture::TEX_Wall, texflags);
			if (picnum < 0 && !ignoreBad)
			{
				//Printf ("Unknown switch texture %s\n", sc_String);
				bad = true;
			}
			pics[numframes] = picnum;
			SC_MustGetString ();
			if (SC_Compare ("tics"))
			{
				SC_MustGetNumber ();
				times[numframes] = sc_Number & 65535;
			}
			else if (SC_Compare ("rand"))
			{
				int min, max;

				SC_MustGetNumber ();
				min = sc_Number & 65535;
				SC_MustGetNumber ();
				max = sc_Number & 65535;
				if (min > max)
				{
					swap (min, max);
				}
				times[numframes] = ((max - min + 1) << 16) | min;
			}
			else
			{
				SC_ScriptError ("Must specify a duration for switch frame");
			}
			numframes++;
		}
		else
		{
			SC_UnGet ();
			break;
		}
	}
	if (numframes == 0)
	{
		SC_ScriptError ("Switch state needs at least one frame");
	}
	if (bad)
	{
		return NULL;
	}
	def = (FSwitchDef *)M_Malloc (myoffsetof (FSwitchDef, u.Times[0]) + numframes * 6);
	def->Sound = sound;
	def->NumFrames = numframes;
	memcpy (&def->u.Times[0], times, numframes * 4);
	memcpy (&def->u.Textures[numframes*2], pics, numframes * 2);
	def->PairIndex = 65535;
	return def;
}

static WORD AddSwitchDef (FSwitchDef *def)
{
	unsigned int i;

	for (i = SwitchList.Size (); i-- > 0; )
	{
		if (SwitchList[i]->PreTexture == def->PreTexture)
		{
			free (SwitchList[i]);
			SwitchList[i] = def;
			return (WORD)i;
		}
	}
	return (WORD)SwitchList.Push (def);
}

//
// Start a button counting down till it turns off.
// [RH] Rewritten to remove MAXBUTTONS limit.
//
static bool P_StartButton (side_t *side, DActiveButton::EWhere w, int switchnum,
						   fixed_t x, fixed_t y, bool useagain)
{
	DActiveButton *button;
	TThinkerIterator<DActiveButton> iterator;
	
	// See if button is already pressed
	while ( (button = iterator.Next ()) )
	{
		if (button->m_Side == side)
		{
			button->m_Timer=1;	// force advancing to the next frame
			return false;
		}
	}

	new DActiveButton (side, w, switchnum, x, y, useagain);
	return true;
}

static int TryFindSwitch (SWORD texture)
{
	int mid, low, high;

	high = (int)(SwitchList.Size () - 1);
	if (high >= 0)
	{
		low = 0;
		do
		{
			mid = (high + low) / 2;
			if (SwitchList[mid]->PreTexture == texture)
			{
				return mid;
			}
			else if (texture < SwitchList[mid]->PreTexture)
			{
				high = mid - 1;
			}
			else
			{
				low = mid + 1;
			}
		} while (low <= high);
	}
	return -1;
}

//
// Function that changes wall texture.
// Tell it if switch is ok to use again (1=yes, it's a button).
//
bool P_ChangeSwitchTexture (side_t *side, int useAgain, BYTE special, bool *quest)
{
	DActiveButton::EWhere where;
	int *texture;
	int i, sound;
	// [BC]
	ULONG	ulShift;

	if ((i = TryFindSwitch (side->toptexture)) != -1)
	{
		texture = &side->toptexture;
		where = DActiveButton::BUTTON_Top;
	}
	else if ((i = TryFindSwitch (side->bottomtexture)) != -1)
	{
		texture = &side->bottomtexture;
		where = DActiveButton::BUTTON_Bottom;
	}
	else if ((i = TryFindSwitch (side->midtexture)) != -1)
	{
		texture = &side->midtexture;
		where = DActiveButton::BUTTON_Middle;
	}
	else
	{
		if (quest != NULL)
		{
			*quest = false;
		}
		return false;
	}

	// [BC] Set the texture change flags.
	if (( NETWORK_GetState( ) != NETSTATE_CLIENT ) &&
		( CLIENTDEMO_IsPlaying( ) == false ))
	{
		ulShift = 0;
		ulShift += where;

		if ( side->linenum < static_cast<unsigned> (numlines) )
		{
			lines[side->linenum].ulTexChangeFlags |= 1 << ulShift;
			ulShift += 3;
			lines[side->linenum].ulTexChangeFlags |= 1 << ulShift;
		}
	}

	// EXIT SWITCH?
	if (SwitchList[i]->Sound != 0)
	{
		sound = SwitchList[i]->Sound;
	}
	else
	{
		sound = S_FindSound (
			special == Exit_Normal ||
			special == Exit_Secret ||
			special == Teleport_NewMap ||
			special == Teleport_EndGame
		   ? "switches/exitbutn" : "switches/normbutn");
	}

	// [RH] The original code played the sound at buttonlist->soundorg,
	//		which wasn't necessarily anywhere near the switch if it was
	//		facing a big sector (and which wasn't necessarily for the
	//		button just activated, either).
	fixed_t pt[3];
	line_t *line = &lines[side->linenum];
	bool playsound;

	pt[0] = line->v1->x + (line->dx >> 1);
	pt[1] = line->v1->y + (line->dy >> 1);
	*texture = SwitchList[i]->u.Textures[SwitchList[i]->NumFrames*2];

	// [BC] If we're the server, tell clients to set the line texture.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SetLineTexture( side->linenum );

	if (useAgain || SwitchList[i]->NumFrames > 1)
		playsound = P_StartButton (side, where, i, pt[0], pt[1], !!useAgain);
	else 
		playsound = true;
	if (playsound)
	{
		S_SoundID (pt, CHAN_VOICE|CHAN_LISTENERZ|CHAN_IMMOBILE, sound, 1, ATTN_STATIC);

		// [BC] If we're the server, tell clients to play the sound.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SoundPoint( pt[0], pt[1], CHAN_VOICE|CHAN_LISTENERZ|CHAN_IMMOBILE, S_GetName( sound ), 1, ATTN_STATIC );
	}
	if (quest != NULL)
	{
		*quest = SwitchList[i]->QuestPanel;
	}
	return true;
}

IMPLEMENT_CLASS (DActiveButton)

DActiveButton::DActiveButton ()
{
	m_Side = NULL;
	m_Where = BUTTON_Nowhere;
	m_SwitchDef = 0;
	m_Timer = 0;
	m_X = 0;
	m_Y = 0;
	bFlippable = false;
}

DActiveButton::DActiveButton (side_t *side, EWhere where, WORD switchnum,
							  fixed_t x, fixed_t y, bool useagain)
{
	m_Side = side;
	m_Where = where;
	m_X = x;
	m_Y = y;
	bFlippable = useagain;

	m_SwitchDef = switchnum;
	m_Frame = 65535;
	AdvanceFrame ();
}

void DActiveButton::Serialize (FArchive &arc)
{
	SDWORD sidenum;

	Super::Serialize (arc);
	if (arc.IsStoring ())
	{
		sidenum = m_Side ? m_Side - sides : -1;
	}
	arc << sidenum << m_Where << m_SwitchDef << m_Frame << m_Timer << bFlippable << m_X << m_Y;
	if (arc.IsLoading ())
	{
		m_Side = sidenum >= 0 ? sides + sidenum : NULL;
	}
}

void DActiveButton::Tick ()
{
	if (--m_Timer == 0)
	{
		FSwitchDef *def = SwitchList[m_SwitchDef];
		if (m_Frame == def->NumFrames - 1)
		{
			fixed_t pt[3];

			m_SwitchDef = def->PairIndex;
			if (m_SwitchDef != 65535)
			{
				def = SwitchList[def->PairIndex];
				m_Frame = 65535;
				pt[0] = m_X;
				pt[1] = m_Y;
				S_SoundID (pt, CHAN_VOICE|CHAN_LISTENERZ|CHAN_IMMOBILE,
					def->Sound != 0 ? def->Sound
					: S_FindSound ("switches/normbutn"), 1, ATTN_STATIC);

				// [BC] If we're the server, tell clients to play the sound.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_SoundPoint( pt[0], pt[1], CHAN_VOICE|CHAN_LISTENERZ|CHAN_IMMOBILE, ( def->Sound != 0 ) ? S_GetName( def->Sound ) : "switches/normbutn", 1, ATTN_STATIC );

				bFlippable = false;
			}
			else
			{
				Destroy ();
				return;
			}
		}
		bool killme = AdvanceFrame ();

		StoreTexture (def->u.Textures[def->NumFrames*2+m_Frame]);

		// [BC] If we're the server, tell clients to update the switch texture.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetLineTexture( m_Side->linenum );

		if (killme)
		{
			Destroy ();
		}
	}
}

bool DActiveButton::AdvanceFrame ()
{
	bool ret = false;
	FSwitchDef *def = SwitchList[m_SwitchDef];

	if (++m_Frame == def->NumFrames - 1)
	{
		if (bFlippable == true)
		{
			m_Timer = BUTTONTIME;
		}
		else
		{
			ret = true;
		}
	}
	else
	{
		if (def->u.Times[m_Frame] & 0xffff0000)
		{
			int t = pr_switchanim();

			m_Timer = (WORD)((((t | (pr_switchanim() << 8))
				% def->u.Times[m_Frame]) >> 16)
				+ (def->u.Times[m_Frame] & 0xffff));
		}
		else
		{
			m_Timer = (WORD)def->u.Times[m_Frame];
		}
	}
	return ret;
}

void DActiveButton::StoreTexture (short tex) const
{
	switch (m_Where)
	{
	case BUTTON_Middle:
		m_Side->midtexture = tex;
		break;

	case BUTTON_Bottom:
		m_Side->bottomtexture = tex;
		break;

	case BUTTON_Top:
		m_Side->toptexture = tex;
		break;

	default:
		return;
	}
}
