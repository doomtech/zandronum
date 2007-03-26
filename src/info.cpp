/*
** info.cpp
** Keeps track of available actors and their states
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
** This is completely different from Doom's info.c.
**
** The primary advancement over Doom's system is that actors can be defined
** across multiple files without having to recompile most of the source
** whenever one changes.
*/


#include "info.h"
#include "m_fixed.h"
#include "c_dispatch.h"
#include "autosegs.h"

#include "gi.h"

#include "actor.h"
#include "r_state.h"
#include "i_system.h"
#include "p_local.h"
#include "cl_commands.h"
#include "cl_main.h"
#include "network.h"

extern void LoadDecorations (void (*process)(FState *, int));

// Each state is owned by an actor. Actors can own any number of
// states, but a single state cannot be owned by more than one
// actor. States are archived by recording the actor they belong
// to and the index into that actor's list of states.

// For NULL states, which aren't owned by any actor, the owner
// is recorded as AActor with the following state. AActor should
// never actually have this many states of its own, so this
// is (relatively) safe.

#define NULL_STATE_INDEX	127

//==========================================================================
//
//
//==========================================================================

FArchive &operator<< (FArchive &arc, FState *&state)
{
	const PClass *info;

	if (arc.IsStoring ())
	{
		if (state == NULL)
		{
			arc.UserWriteClass (RUNTIME_CLASS(AActor));
			arc.WriteCount (NULL_STATE_INDEX);
			return arc;
		}

		info = FState::StaticFindStateOwner (state);

		if (info != NULL)
		{
			arc.UserWriteClass (info);
			arc.WriteCount ((DWORD)(state - info->ActorInfo->OwnedStates));
		}
		else
		{
			I_Error ("Cannot find owner for state %p:\n"
					 "%s %c%c %3d [%p] -> %p", state,
					 sprites[state->sprite.index].name,
					 state->GetFrame() + 'A',
					 state->GetFullbright() ? '*' : ' ',
					 state->GetTics(),
					 state->GetAction(),
					 state->GetNextState());
		}
	}
	else
	{
		const PClass *info;
		DWORD ofs;

		arc.UserReadClass (info);
		ofs = arc.ReadCount ();
		if (ofs == NULL_STATE_INDEX && info == RUNTIME_CLASS(AActor))
		{
			state = NULL;
		}
		else if (info->ActorInfo != NULL)
		{
			state = info->ActorInfo->OwnedStates + ofs;
		}
		else
		{
			state = NULL;
		}
	}
	return arc;
}

//==========================================================================
//
// Find the actor that a state belongs to.
//
//==========================================================================

const PClass *FState::StaticFindStateOwner (const FState *state)
{
	const FActorInfo *info = RUNTIME_CLASS(AActor)->ActorInfo;

	if (state >= info->OwnedStates &&
		state < info->OwnedStates + info->NumOwnedStates)
	{
		return RUNTIME_CLASS(AActor);
	}

	TAutoSegIterator<FActorInfo *, &ARegHead, &ARegTail> reg;
	while (++reg != NULL)
	{
		if (state >= reg->OwnedStates &&
			state <  reg->OwnedStates + reg->NumOwnedStates)
		{
			return reg->Class;
		}
	}

	for (unsigned int i = 0; i < PClass::m_RuntimeActors.Size(); ++i)
	{
		info = PClass::m_RuntimeActors[i]->ActorInfo;
		if (state >= info->OwnedStates &&
			state <  info->OwnedStates + info->NumOwnedStates)
		{
			return info->Class;
		}
	}

	return NULL;
}

//==========================================================================
//
// Find the actor that a state belongs to, but restrict the search to
// the specified type and its ancestors.
//
//==========================================================================

const PClass *FState::StaticFindStateOwner (const FState *state, const FActorInfo *info)
{
	while (info != NULL)
	{
		if (state >= info->OwnedStates &&
			state <  info->OwnedStates + info->NumOwnedStates)
		{
			return info->Class;
		}
		info = info->Class->ParentClass->ActorInfo;
	}
	return NULL;
}

//==========================================================================
//
//
//==========================================================================

int GetSpriteIndex(const char * spritename)
{
	for (unsigned i = 0; i < sprites.Size (); ++i)
	{
		if (strncmp (sprites[i].name, spritename, 4) == 0)
		{
			return (int)i;
		}
	}
	spritedef_t temp;
	strncpy (temp.name, spritename, 4);
	temp.name[4] = 0;
	temp.numframes = 0;
	temp.spriteframes = 0;
	return (int)sprites.Push (temp);
}


//==========================================================================
//
// Change sprite names to indices
//
//==========================================================================

static void ProcessStates (FState *states, int numstates)
{
	int sprite = -1;

	if (states == NULL)
		return;
	while (--numstates >= 0)
	{
		if (sprite == -1 || strncmp (sprites[sprite].name, states->sprite.name, 4) != 0)
		{
			sprite = GetSpriteIndex(states->sprite.name);
		}
		states->sprite.index = sprite;
		states++;
	}
}


//==========================================================================
//
//
//==========================================================================

void FActorInfo::StaticInit ()
{
	TAutoSegIterator<FActorInfo *, &ARegHead, &ARegTail> reg;

	if (sprites.Size() == 0)
	{
		spritedef_t temp;

		// Sprite 0 is always TNT1
		memcpy (temp.name, "TNT1", 5);
		temp.numframes = 0;
		temp.spriteframes = 0;
		sprites.Push (temp);

		// Sprite 1 is always ----
		memcpy (temp.name, "----", 5);
		sprites.Push (temp);
	}

	// Attach FActorInfo structures to every actor's PClass
	while (++reg != NULL)
	{
		reg->Class->ActorInfo = reg;
		if (reg->OwnedStates &&
			(unsigned)reg->OwnedStates->sprite.index < sprites.Size ())
		{
			Printf ("\x1c+%s is stateless. Fix its default list.\n",
				reg->Class->TypeName.GetChars());
		}
		ProcessStates (reg->OwnedStates, reg->NumOwnedStates);
	}

	// Now build default instances of every actor
	reg.Reset ();
	while (++reg != NULL)
	{
		reg->BuildDefaults ();
	}

	Printf ("LoadDecorations: Load external actors.\n");
	LoadDecorations (ProcessStates);
}

//==========================================================================
//
// Called after the IWAD has been identified
//
//==========================================================================

void FActorInfo::StaticGameSet ()
{
	// Run every AT_GAME_SET function
	TAutoSegIteratorNoArrow<void (*)(), &GRegHead, &GRegTail> setters;
	while (++setters != NULL)
	{
		((void (*)())setters) ();
	}
}

//==========================================================================
//
// Called after Dehacked patches are applied
//
//==========================================================================

void FActorInfo::StaticSetActorNums ()
{
	memset (SpawnableThings, 0, sizeof(SpawnableThings));
	DoomEdMap.Empty ();

	// For every actor valid for this game, add it to the
	// SpawnableThings array and DoomEdMap
	TAutoSegIterator<FActorInfo *, &ARegHead, &ARegTail> reg;
	while (++reg != NULL)
	{
		reg->RegisterIDs ();
	}

	for (unsigned int i = 0; i < PClass::m_RuntimeActors.Size(); ++i)
	{
		PClass::m_RuntimeActors[i]->ActorInfo->RegisterIDs ();
	}
}

//==========================================================================
//
//
//==========================================================================

void FActorInfo::RegisterIDs ()
{
	if (GameFilter == GAME_Any || (GameFilter & gameinfo.gametype))
	{
		if (SpawnID != 0)
		{
			SpawnableThings[SpawnID] = Class;
		}
		if (DoomEdNum != -1)
		{
			DoomEdMap.AddType (DoomEdNum, Class);
		}
	}
}

//==========================================================================
//
// Called when a new game is started, but only if the game
// speed has changed.
//
//==========================================================================

void FActorInfo::StaticSpeedSet ()
{
	TAutoSegIteratorNoArrow<void (*)(int), &SRegHead, &SRegTail> setters;
	while (++setters != NULL)
	{
		((void (*)(int))setters) (GameSpeed);
	}
}

//==========================================================================
//
//
//==========================================================================

FActorInfo *FActorInfo::GetReplacement ()
{
	if (Replacement == NULL)
	{
		return this;
	}
	// The Replacement field is temporarily NULLed to prevent
	// potential infinite recursion.
	FActorInfo *savedrep = Replacement;
	Replacement = NULL;
	FActorInfo *rep = savedrep->GetReplacement ();
	Replacement = savedrep;
	return rep;
}

//==========================================================================
//
//
//==========================================================================

FActorInfo *FActorInfo::GetReplacee ()
{
	if (Replacee == NULL)
	{
		return this;
	}
	// The Replacee field is temporarily NULLed to prevent
	// potential infinite recursion.
	FActorInfo *savedrep = Replacee;
	Replacee = NULL;
	FActorInfo *rep = savedrep->GetReplacee ();
	Replacee = savedrep;
	return rep;
}

FDoomEdMap DoomEdMap;

FDoomEdMap::FDoomEdEntry *FDoomEdMap::DoomEdHash[DOOMED_HASHSIZE];

FDoomEdMap::~FDoomEdMap()
{
	Empty();
}

void FDoomEdMap::AddType (int doomednum, const PClass *type)
{
	unsigned int hash = (unsigned int)doomednum % DOOMED_HASHSIZE;
	FDoomEdEntry *entry = DoomEdHash[hash];
	while (entry && entry->DoomEdNum != doomednum)
	{
		entry = entry->HashNext;
	}
	if (entry == NULL)
	{
		entry = new FDoomEdEntry;
		entry->HashNext = DoomEdHash[hash];
		entry->DoomEdNum = doomednum;
		DoomEdHash[hash] = entry;
	}
	else
	{
		Printf (PRINT_BOLD, "Warning: %s and %s both have doomednum %d.\n",
			type->TypeName.GetChars(), entry->Type->TypeName.GetChars(), doomednum);
	}
	entry->Type = type;
}

void FDoomEdMap::DelType (int doomednum)
{
	unsigned int hash = (unsigned int)doomednum % DOOMED_HASHSIZE;
	FDoomEdEntry **prev = &DoomEdHash[hash];
	FDoomEdEntry *entry = *prev;
	while (entry && entry->DoomEdNum != doomednum)
	{
		prev = &entry->HashNext;
		entry = entry->HashNext;
	}
	if (entry != NULL)
	{
		*prev = entry->HashNext;
		delete entry;
	}
}

void FDoomEdMap::Empty ()
{
	int bucket;

	for (bucket = 0; bucket < DOOMED_HASHSIZE; ++bucket)
	{
		FDoomEdEntry *probe = DoomEdHash[bucket];

		while (probe != NULL)
		{
			FDoomEdEntry *next = probe->HashNext;
			delete probe;
			probe = next;
		}
		DoomEdHash[bucket] = NULL;
	}
}

const PClass *FDoomEdMap::FindType (int doomednum) const
{
	unsigned int hash = (unsigned int)doomednum % DOOMED_HASHSIZE;
	FDoomEdEntry *entry = DoomEdHash[hash];
	while (entry && entry->DoomEdNum != doomednum)
		entry = entry->HashNext;
	return entry ? entry->Type : NULL;
}

struct EdSorting
{
	const PClass *Type;
	int DoomEdNum;
};

static int STACK_ARGS sortnums (const void *a, const void *b)
{
	return ((const EdSorting *)a)->DoomEdNum -
		((const EdSorting *)b)->DoomEdNum;
}

void FDoomEdMap::DumpMapThings ()
{
	TArray<EdSorting> infos (PClass::m_Types.Size());
	int i;

	for (i = 0; i < DOOMED_HASHSIZE; ++i)
	{
		FDoomEdEntry *probe = DoomEdHash[i];

		while (probe != NULL)
		{
			EdSorting sorting = { probe->Type, probe->DoomEdNum };
			infos.Push (sorting);
			probe = probe->HashNext;
		}
	}

	if (infos.Size () == 0)
	{
		Printf ("No map things registered\n");
	}
	else
	{
		qsort (&infos[0], infos.Size (), sizeof(EdSorting), sortnums);

		for (i = 0; i < (int)infos.Size (); ++i)
		{
			Printf ("%6d %s\n",
				infos[i].DoomEdNum, infos[i].Type->TypeName.GetChars());
		}
	}
}

CCMD (dumpmapthings)
{
	FDoomEdMap::DumpMapThings ();
}

bool CheckCheatmode ();

CCMD (summon)
{
	if (CheckCheatmode ())
		return;

	if (argv.argc() > 1)
	{
		if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
		{
			CLIENTCOMMANDS_SummonCheat( argv[1] );
			return;
		}

		const PClass *type = PClass::FindClass (argv[1]);
		if (type == NULL)
		{
			Printf ("Unknown class '%s'\n", argv[1]);
			return;
		}
		Net_WriteByte (DEM_SUMMON);
		Net_WriteString (type->TypeName.GetChars());
	}
}

CCMD (summonfriend)
{
	if (CheckCheatmode ())
		return;

	if (argv.argc() > 1)
	{
		const PClass *type = PClass::FindClass (argv[1]);
		if (type == NULL)
		{
			Printf ("Unknown class '%s'\n", argv[1]);
			return;
		}
		Net_WriteByte (DEM_SUMMONFRIEND);
		Net_WriteString (type->TypeName.GetChars());
	}
}
