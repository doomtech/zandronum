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
#include "p_3dmidtex.h"
#include "m_random.h"
#include "g_game.h"
#include "s_sound.h"
#include "doomstat.h"
#include "r_state.h"
#include "w_wad.h"
#include "tarray.h"
#include "cmdlib.h"
// [BC] New #includes.
#include "cl_demo.h"
#include "deathmatch.h"
#include "network.h"
#include "sv_commands.h"

#include "gi.h"

static FRandom pr_switchanim ("AnimSwitch");

class DActiveButton : public DThinker
{
	DECLARE_CLASS (DActiveButton, DThinker)
public:
	DActiveButton ();
	DActiveButton (side_t *, int, WORD switchnum, fixed_t x, fixed_t y, bool flippable);

	void Serialize (FArchive &arc);
	void Tick ();

	side_t	*m_Side;
	SBYTE	m_Part;
	WORD	m_SwitchDef;
	WORD	m_Frame;
	WORD	m_Timer;
	bool	bFlippable;
	fixed_t	m_X, m_Y;	// Location of timer sound

protected:
	bool AdvanceFrame ();
};


//==========================================================================
//
// Start a button counting down till it turns off.
// [RH] Rewritten to remove MAXBUTTONS limit.
//
//==========================================================================

static bool P_StartButton (side_t *side, int Where, int switchnum,
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

	new DActiveButton (side, Where, switchnum, x, y, useagain);
	return true;
}

//==========================================================================
//
// Checks whether a switch is reachable
// This is optional because old maps can rely on being able to 
// use non-reachable switches.
//
//==========================================================================

bool P_CheckSwitchRange(AActor *user, line_t *line, int sideno)
{
	// Activated from an empty side -> always succeed
	side_t *side = line->sidedef[sideno];
	if (side == NULL)
		return true;

	fixed_t checktop;
	fixed_t checkbot;
	sector_t *front = side->sector;
	FLineOpening open;

	// 3DMIDTEX forces CHECKSWITCHRANGE because otherwise it might cause problems.
	if (!(line->flags & (ML_3DMIDTEX|ML_CHECKSWITCHRANGE)))
		return true;

	// calculate the point where the user would touch the wall.
	divline_t dll, dlu;
	fixed_t inter, checkx, checky;

	P_MakeDivline (line, &dll);

	dlu.x = user->x;
	dlu.y = user->y;
	dlu.dx = finecosine[user->angle >> ANGLETOFINESHIFT];
	dlu.dy = finesine[user->angle >> ANGLETOFINESHIFT];
	inter = P_InterceptVector(&dll, &dlu);


	// Polyobjects must test the containing sector, not the one they originate from.
	if (line->sidedef[0]->Flags & WALLF_POLYOBJ)
	{
		// Get a check point slightly inside the polyobject so that this still works
		// if the polyobject lies directly on a sector boundary
		checkx = dll.x + FixedMul(dll.dx, inter + (FRACUNIT/100));
		checky = dll.y + FixedMul(dll.dy, inter + (FRACUNIT/100));
		front = P_PointInSector(checkx, checky);
	}
	else
	{
		checkx = dll.x + FixedMul(dll.dx, inter);
		checky = dll.y + FixedMul(dll.dy, inter);
	}


	// one sided line or polyobject
	if (line->sidedef[1] == NULL || (line->sidedef[0]->Flags & WALLF_POLYOBJ))
	{
	onesided:
		fixed_t sectorc = front->ceilingplane.ZatPoint(checkx, checky);
		fixed_t sectorf = front->floorplane.ZatPoint(checkx, checky);
		return (user->z + user->height >= sectorf && user->z <= sectorc);
	}

	// Now get the information from the line.
	P_LineOpening(open, NULL, line, checkx, checky, user->x, user->y);
	if (open.range <= 0)
		goto onesided;

	if ((TexMan.FindSwitch (side->GetTexture(side_t::top))) != -1)
	{
		return (user->z + user->height >= open.top);
	}
	else if ((TexMan.FindSwitch (side->GetTexture(side_t::bottom))) != -1)
	{
		return (user->z <= open.bottom);
	}
	else if ((line->flags & (ML_3DMIDTEX)) || (TexMan.FindSwitch (side->GetTexture(side_t::mid))) != -1)
	{
		// 3DMIDTEX lines will force a mid texture check if no switch is found on this line
		// to keep compatibility with Eternity's implementation.
		if (!P_GetMidTexturePosition(line, sideno, &checktop, &checkbot))
			return false;
		return user->z < checktop && user->z + user->height > checkbot;
	}
	else
	{
		// no switch found. Check whether the player can touch either top or bottom texture
		return (user->z + user->height >= open.top) || (user->z <= open.bottom);
	}
}

//==========================================================================
//
// Function that changes wall texture.
// Tell it if switch is ok to use again (1=yes, it's a button).
//
//==========================================================================

bool P_ChangeSwitchTexture (side_t *side, int useAgain, BYTE special, bool *quest)
{
	int texture;
	int sound;
	int i;
	FSwitchDef *Switch;
	// [BC]
	ULONG	ulShift;
	// [BB] Check the ulShift handling.

	if ((i = TexMan.FindSwitch (side->GetTexture(side_t::top))) != -1)
	{
		texture = side_t::top;
		ulShift = 0;
	}
	else if ((i = TexMan.FindSwitch (side->GetTexture(side_t::bottom))) != -1)
	{
		texture = side_t::bottom;
		ulShift = 2;
	}
	else if ((i = TexMan.FindSwitch (side->GetTexture(side_t::mid))) != -1)
	{
		texture = side_t::mid;
		ulShift = 1;
	}
	else
	{
		if (quest != NULL)
		{
			*quest = false;
		}
		return false;
	}
	Switch = TexMan.GetSwitch(i);

	// [BC] Set the texture change flags.
	if (( NETWORK_GetState( ) != NETSTATE_CLIENT ) &&
		( CLIENTDEMO_IsPlaying( ) == false ))
	{
		if ( side->linedef != NULL )
		{
			side->linedef->ulTexChangeFlags |= 1 << ulShift;
			ulShift += 3;
			side->linedef->ulTexChangeFlags |= 1 << ulShift;
		}
	}

	// EXIT SWITCH?
	if (Switch->Sound != 0)
	{
		sound = Switch->Sound;
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
	fixed_t pt[2];
	line_t *line = side->linedef;
	bool playsound;

	pt[0] = line->v1->x + (line->dx >> 1);
	pt[1] = line->v1->y + (line->dy >> 1);
	side->SetTexture(texture, Switch->u[0].Texture);

	// [BC] If we're the server, tell clients to set the line texture.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SetLineTexture( static_cast<ULONG>( side->linedef - lines ) );

	if (useAgain || Switch->NumFrames > 1)
	{
		playsound = P_StartButton (side, texture, i, pt[0], pt[1], !!useAgain);
	}
	else 
	{
		playsound = true;
	}
	if (playsound)
	{
		S_Sound (pt[0], pt[1], 0, CHAN_VOICE|CHAN_LISTENERZ, sound, 1, ATTN_STATIC);

		// [BC] If we're the server, tell clients to play the sound.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SoundPoint( pt[0], pt[1], 0, CHAN_VOICE|CHAN_LISTENERZ, S_GetName( sound ), 1, ATTN_STATIC );
	}
	if (quest != NULL)
	{
		*quest = Switch->QuestPanel;
	}
	return true;
}

//==========================================================================
//
// Button thinker
//
//==========================================================================

IMPLEMENT_CLASS (DActiveButton)

DActiveButton::DActiveButton ()
{
	m_Side = NULL;
	m_Part = -1;
	m_SwitchDef = 0;
	m_Timer = 0;
	m_X = 0;
	m_Y = 0;
	bFlippable = false;
}

DActiveButton::DActiveButton (side_t *side, int Where, WORD switchnum,
							  fixed_t x, fixed_t y, bool useagain)
{
	m_Side = side;
	m_Part = SBYTE(Where);
	m_X = x;
	m_Y = y;
	bFlippable = useagain;

	m_SwitchDef = switchnum;
	m_Frame = 65535;
	AdvanceFrame ();
}

//==========================================================================
//
//
//
//==========================================================================

void DActiveButton::Serialize (FArchive &arc)
{
	SDWORD sidenum;

	Super::Serialize (arc);
	if (arc.IsStoring ())
	{
		sidenum = m_Side ? SDWORD(m_Side - sides) : -1;
	}
	arc << sidenum << m_Part << m_SwitchDef << m_Frame << m_Timer << bFlippable << m_X << m_Y;
	if (arc.IsLoading ())
	{
		m_Side = sidenum >= 0 ? sides + sidenum : NULL;
	}
}

//==========================================================================
//
//
//
//==========================================================================

void DActiveButton::Tick ()
{
	if (--m_Timer == 0)
	{
		FSwitchDef *def = TexMan.GetSwitch(m_SwitchDef);
		if (m_Frame == def->NumFrames - 1)
		{
			m_SwitchDef = def->PairIndex;
			if (m_SwitchDef != 65535)
			{
				def = TexMan.GetSwitch(def->PairIndex);
				m_Frame = 65535;
				S_Sound (m_X, m_Y, 0, CHAN_VOICE|CHAN_LISTENERZ,
					def->Sound != 0 ? FSoundID(def->Sound) : FSoundID("switches/normbutn"),
					1, ATTN_STATIC);

				// [BC] If we're the server, tell clients to play the sound.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_SoundPoint( m_X, m_Y, 0, CHAN_VOICE|CHAN_LISTENERZ, ( def->Sound != 0 ) ? S_GetName( def->Sound ) : "switches/normbutn", 1, ATTN_STATIC );

				bFlippable = false;
			}
			else
			{
				Destroy ();
				return;
			}
		}
		bool killme = AdvanceFrame ();

		m_Side->SetTexture(m_Part, def->u[m_Frame].Texture);

		// [BC] If we're the server, tell clients to update the switch texture.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetLineTexture( static_cast<ULONG>( m_Side->linedef - lines )  );

		if (killme)
		{
			Destroy ();
		}
	}
}

//==========================================================================
//
//
//
//==========================================================================

bool DActiveButton::AdvanceFrame ()
{
	bool ret = false;
	FSwitchDef *def = TexMan.GetSwitch(m_SwitchDef);

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
		if (def->u[m_Frame].Time & 0xffff0000)
		{
			int t = pr_switchanim();

			m_Timer = (WORD)((((t | (pr_switchanim() << 8))
				% def->u[m_Frame].Time) >> 16)
				+ (def->u[m_Frame].Time & 0xffff));
		}
		else
		{
			m_Timer = (WORD)def->u[m_Frame].Time;
		}
	}
	return ret;
}

