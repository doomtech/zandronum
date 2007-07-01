/*
** dthinker.cpp
** Implements the base class for almost anything in a level that might think
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

// [BB] network.h has to be included before stats.h under Linux.
// The reason should be investigated.
#include "network.h"

#include "dthinker.h"
#include "stats.h"
#include "p_local.h"
#include "statnums.h"
#include "i_system.h"
#include "doomerrors.h"


static cycle_t ThinkCycles;
// [BC] Show the number of ticked thinkers this tick.
static	LONG	g_lTickedThinkers;

IMPLEMENT_CLASS (DThinker)

static Node *NextToThink;

List DThinker::Thinkers[MAX_STATNUM+1];
List DThinker::FreshThinkers[MAX_STATNUM+1];
bool DThinker::bSerialOverride = false;

void DThinker::SerializeAll (FArchive &arc, bool hubLoad)
{
	Node *node;
	DThinker *thinker;
	BYTE stat;
	int statcount;
	int i;

	// Save lists of thinkers, but not by storing the first one and letting
	// the archiver catch the rest. (Which leads to buttloads of recursion
	// and makes the file larger.) Instead, we explicitly save each thinker
	// in sequence. When restoring an archive, we also have to maintain
	// the thinker lists here instead of relying on the archiver to do it
	// for us.

	if (arc.IsStoring ())
	{
		for (statcount = i = 0; i <= MAX_STATNUM; i++)
		{
			if (!Thinkers[i].IsEmpty ())
			{
				statcount++;
			}
		}
		arc << statcount;
		for (i = 0; i <= MAX_STATNUM; i++)
		{
			node = Thinkers[i].Head;
			if (node->Succ != NULL)
			{
				stat = i;
				arc << stat;
				do
				{
					thinker = static_cast<DThinker *> (node);
					arc << thinker;
					node = node->Succ;
				} while (node->Succ != NULL);
				thinker = NULL;
				arc << thinker;		// Save a final NULL for this list
			}
		}
	}
	else
	{
		if (hubLoad)
			DestroyMostThinkers ();
		else
			DestroyAllThinkers ();

		// Prevent the constructor from inserting thinkers into a list.
		bSerialOverride = true;

		try
		{
			arc << statcount;
			while (statcount > 0)
			{
				arc << stat << thinker;
				while (thinker != NULL)
				{
					Thinkers[stat].AddTail (thinker);
					arc << thinker;
				}
				statcount--;
			}
		}
		catch (class CDoomError &)
		{
			bSerialOverride = false;
			DestroyAllThinkers ();
			throw;
		}
		bSerialOverride = false;
	}
}

DThinker::DThinker (int statnum) throw()
{
	if (bSerialOverride)
	{ // The serializer will insert us into the right list
		Succ = NULL;
		return;
	}

	ObjectFlags |= OF_JustSpawned;
	if ((unsigned)statnum > MAX_STATNUM)
	{
		statnum = MAX_STATNUM;
	}
	FreshThinkers[statnum].AddTail (this);
}

DThinker::~DThinker ()
{
	if (Succ != NULL)
	{
		Remove ();
	}
}

void DThinker::Destroy ()
{
	if (this == NextToThink)
		NextToThink = Succ;

	if (Succ != NULL)
	{
		Remove ();
		Succ = NULL;
	}
	Super::Destroy ();
}

void DThinker::PostBeginPlay ()
{
}

DThinker *DThinker::FirstThinker (int statnum)
{
	Node *node;

	if ((unsigned)statnum > MAX_STATNUM)
	{
		statnum = MAX_STATNUM;
	}
	node = Thinkers[statnum].Head;
	if (node->Succ == NULL)
	{
		node = FreshThinkers[statnum].Head;
		if (node->Succ == NULL)
		{
			return NULL;
		}
	}
	return static_cast<DThinker *>(node);
}

void DThinker::ChangeStatNum (int statnum)
{
	if ((unsigned)statnum > MAX_STATNUM)
	{
		statnum = MAX_STATNUM;
	}
	Remove ();
	if ((ObjectFlags & OF_JustSpawned) && statnum >= STAT_FIRST_THINKING)
	{
		FreshThinkers[statnum].AddTail (this);
	}
	else
	{
		Thinkers[statnum].AddTail (this);
	}
}

// Destroy every thinker
void DThinker::DestroyAllThinkers ()
{
	int i;

	DObject::BeginFrame ();
	for (i = 0; i <= MAX_STATNUM; i++)
	{
		if (i != STAT_TRAVELLING)
		{
			DestroyThinkersInList (Thinkers[i].Head);
			DestroyThinkersInList (FreshThinkers[i].Head);
		}
	}
	DObject::EndFrame ();
}

// Destroy all thinkers except for player-controlled actors
// Players are simply removed from the list of thinkers and
// will be added back after serialization is complete.
void DThinker::DestroyMostThinkers ()
{
	int i;

	DObject::BeginFrame ();
	for (i = 0; i <= MAX_STATNUM; i++)
	{
		if (i != STAT_TRAVELLING)
		{
			DestroyMostThinkersInList (Thinkers[i], i);
			DestroyMostThinkersInList (FreshThinkers[i], i);
		}
	}
	DObject::EndFrame ();
}

void DThinker::DestroyThinkersInList (Node *node)
{
	while (node->Succ != NULL)
	{
		Node *next = node->Succ;
		static_cast<DThinker *> (node)->Destroy ();
		node = next;
	}
}

void DThinker::DestroyMostThinkersInList (List &list, int stat)
{
	if (stat != STAT_PLAYER)
	{
		DestroyThinkersInList (list.Head);
	}
	else
	{
		Node *node = list.Head;
		while (node->Succ != NULL)
		{
			Node *next = node->Succ;
			node->Remove ();
			node = next;
		}
	}
}

void DThinker::RunThinkers ()
{
	int i, count;

	// [BC] Reset the number of thinkers ticked.
	ThinkCycles = 0;
	g_lTickedThinkers = 0;

	clock (ThinkCycles);

	// Tick every thinker left from last time
	for (i = STAT_FIRST_THINKING; i <= MAX_STATNUM; ++i)
	{
		TickThinkers (&Thinkers[i], NULL);
	}

	// Keep ticking the fresh thinkers until there are no new ones.
	do
	{
		count = 0;
		for (i = STAT_FIRST_THINKING; i <= MAX_STATNUM; ++i)
		{
			count += TickThinkers (&FreshThinkers[i], &Thinkers[i]);
		}
	} while (count != 0);

	unclock (ThinkCycles);
}

int DThinker::TickThinkers (List *list, List *dest)
{
	int count = 0;
	Node *node = list->Head;

	while (node->Succ != NULL)
	{
		++count;
		NextToThink = node->Succ;
		DThinker *thinker = static_cast<DThinker *> (node);
		if (thinker->ObjectFlags & OF_JustSpawned)
		{
			thinker->ObjectFlags &= ~OF_JustSpawned;
			if (dest != NULL)
			{ // Move thinker from this list to the destination list
				node->Remove ();
				dest->AddTail (node);
			}
			thinker->PostBeginPlay ();
		}
		else if (dest != NULL)
		{ // Move thinker from this list to the destination list
			I_Error ("There is a thinker in the fresh list that has already ticked.\n");
		}

		if (!(thinker->ObjectFlags & OF_MassDestruction))
		{ // Only tick thinkers not scheduled for destruction
#ifndef	MULTITICK_HACK_FIX
			bool	bTickThinker = true;

			switch ( NETWORK_GetState( ))
			{
			case NETSTATE_CLIENT:

				// Don't tick the consoleplayer's actor in client
				// mode, because that's done in the main prediction function
				if (( thinker->IsKindOf( RUNTIME_CLASS( AActor ))) && ( static_cast<AActor *>(thinker) == players[consoleplayer].mo ))
					bTickThinker = false;
				break;
			case NETSTATE_SERVER:

				// Player's actors are ticked whenever we receive a movement command from them, so don't do it here.
				if (( thinker->IsKindOf( RUNTIME_CLASS( AActor ))) && ( static_cast<AActor *>(thinker)->player ) && ( static_cast<AActor *>(thinker)->player->bIsBot == false ))
					bTickThinker = false;
				break;
			}

			if ( bTickThinker )
#endif
			{
				// [BC] Tick the thinker, and make a note of it.
				thinker->Tick ();
				g_lTickedThinkers++;
			}
		}
		node = NextToThink;
	}
	return count;
}

void DThinker::Tick ()
{
}

FThinkerIterator::FThinkerIterator (const PClass *type, int statnum)
{
	if ((unsigned)statnum > MAX_STATNUM)
	{
		m_Stat = STAT_FIRST_THINKING;
		m_SearchStats = true;
	}
	else
	{
		m_Stat = statnum;
		m_SearchStats = false;
	}
	m_ParentType = type;
	m_CurrThinker = DThinker::Thinkers[m_Stat].Head;
	m_SearchingFresh = false;
}

FThinkerIterator::FThinkerIterator (const PClass *type, int statnum, DThinker *prev)
{
	if ((unsigned)statnum > MAX_STATNUM)
	{
		m_Stat = STAT_FIRST_THINKING;
		m_SearchStats = true;
	}
	else
	{
		m_Stat = statnum;
		m_SearchStats = false;
	}
	m_ParentType = type;
	if (prev == NULL || prev->Succ == NULL)
	{
		Reinit ();
	}
	else
	{
		m_CurrThinker = prev->Succ;
		m_SearchingFresh = false;
	}
}

void FThinkerIterator::Reinit ()
{
	m_CurrThinker = DThinker::Thinkers[m_Stat].Head;
	m_SearchingFresh = false;
}

DThinker *FThinkerIterator::Next ()
{
	if (m_ParentType == NULL) return NULL;
	do
	{
		do
		{
			while (m_CurrThinker->Succ)
			{
				DThinker *thinker = static_cast<DThinker *> (m_CurrThinker);
				if (thinker->IsKindOf (m_ParentType))
				{
					m_CurrThinker = m_CurrThinker->Succ;
					return thinker;
				}
				m_CurrThinker = m_CurrThinker->Succ;
			}
			if ((m_SearchingFresh = !m_SearchingFresh))
			{
				m_CurrThinker = DThinker::FreshThinkers[m_Stat].Head;
			}
		} while (m_SearchingFresh);
		if (m_SearchStats)
		{
			m_Stat++;
			if (m_Stat > MAX_STATNUM)
			{
				m_Stat = STAT_FIRST_THINKING;
			}
		}
		m_CurrThinker = DThinker::Thinkers[m_Stat].Head;
		m_SearchingFresh = false;
	} while (m_SearchStats && m_Stat != STAT_FIRST_THINKING);
	return NULL;
}

ADD_STAT (think)
{
	FString out;
	// [BC] Show the number of ticked thinkers this tick.
	out.Format ("Think time = %04.1f ms, %3d",
		SecondsPerCycle * (double)ThinkCycles * 1000,
		g_lTickedThinkers );
	return out;
}
