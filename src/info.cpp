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
*/


#include "info.h"
#include "m_fixed.h"
#include "c_dispatch.h"
#include "d_net.h"

#include "gi.h"

#include "actor.h"
#include "r_state.h"
#include "i_system.h"
#include "p_local.h"
#include "templates.h"
#include "cmdlib.h"
#include "g_level.h"
#include "cl_commands.h"
#include "cl_main.h"
#include "network.h"

extern void LoadActors ();


//==========================================================================
//
//
//==========================================================================

int GetSpriteIndex(const char * spritename)
{
	static char lastsprite[5];
	static int lastindex;

	// Make sure that the string is upper case and 4 characters long
	char upper[5]={0,0,0,0,0};
	for (int i = 0; spritename[i] != 0 && i < 4; i++)
	{
		upper[i] = toupper (spritename[i]);
	}

	// cache the name so if the next one is the same the function doesn't have to perform a search.
	if (!strcmp(upper, lastsprite))
	{
		return lastindex;
	}
	strcpy(lastsprite, upper);

	for (unsigned i = 0; i < sprites.Size (); ++i)
	{
		if (strcmp (sprites[i].name, upper) == 0)
		{
			return (lastindex = (int)i);
		}
	}
	spritedef_t temp;
	strcpy (temp.name, upper);
	temp.numframes = 0;
	temp.spriteframes = 0;
	return (lastindex = (int)sprites.Push (temp));
}


//==========================================================================
//
//
//==========================================================================

void FActorInfo::StaticInit ()
{
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

	Printf ("LoadActors: Load actor definitions.\n");
	LoadActors ();
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
	// Fill out the list for Chex Quest with Doom's actors
	if (gameinfo.gametype == GAME_Chex && DoomEdMap.FindType(DoomEdNum) == NULL &&
		(GameFilter & GAME_Doom))
	{
		DoomEdMap.AddType (DoomEdNum, Class, true);
	}
}

//==========================================================================
//
//
//==========================================================================

FActorInfo *FActorInfo::GetReplacement (bool lookskill)
{
	FName skillrepname = AllSkills[gameskill].GetReplacement(this->Class->TypeName);
	if (skillrepname != NAME_None && PClass::FindClass(skillrepname) == NULL)
	{
		Printf("Warning: incorrect actor name in definition of skill %s: \n\
			   class %s is replaced by inexistent class %s\n\
			   Skill replacement will be ignored for this actor.\n", 
			   AllSkills[gameskill].Name.GetChars(), 
			   this->Class->TypeName.GetChars(), skillrepname.GetChars());
		AllSkills[gameskill].SetReplacement(this->Class->TypeName, NAME_None);
		AllSkills[gameskill].SetReplacedBy(skillrepname, NAME_None);
		lookskill = false; skillrepname = NAME_None;
	}
	if (Replacement == NULL && (!lookskill || skillrepname == NAME_None))
	{
		return this;
	}
	// The Replacement field is temporarily NULLed to prevent
	// potential infinite recursion.
	FActorInfo *savedrep = Replacement;
	Replacement = NULL;
	FActorInfo *rep = savedrep;
	// Handle skill-based replacement here. It has precedence on DECORATE replacement
	// in that the skill replacement is applied first, followed by DECORATE replacement
	// on the actor indicated by the skill replacement.
	if (lookskill && (skillrepname != NAME_None))
	{
		rep = PClass::FindClass(skillrepname)->ActorInfo;
	}
	// Now handle DECORATE replacement chain
	// Skill replacements are not recursive, contrarily to DECORATE replacements
	rep = rep->GetReplacement(false);
	// Reset the temporarily NULLed field
	Replacement = savedrep;
	return rep;
}

//==========================================================================
//
//
//==========================================================================

FActorInfo *FActorInfo::GetReplacee (bool lookskill)
{
	FName skillrepname = AllSkills[gameskill].GetReplacedBy(this->Class->TypeName);
	if (skillrepname != NAME_None && PClass::FindClass(skillrepname) == NULL)
	{
		Printf("Warning: incorrect actor name in definition of skill %s: \
			   inexistent class %s is replaced by class %s\n\
			   Skill replacement will be ignored for this actor.\n", 
			   AllSkills[gameskill].Name.GetChars(), 
			   skillrepname.GetChars(), this->Class->TypeName.GetChars());
		AllSkills[gameskill].SetReplacedBy(this->Class->TypeName, NAME_None);
		AllSkills[gameskill].SetReplacement(skillrepname, NAME_None);
		lookskill = false; 
	}
	if (Replacee == NULL && (!lookskill || skillrepname == NAME_None))
	{
		return this;
	}
	// The Replacee field is temporarily NULLed to prevent
	// potential infinite recursion.
	FActorInfo *savedrep = Replacee;
	Replacee = NULL;
	FActorInfo *rep = savedrep;
	if (lookskill && (skillrepname != NAME_None) && (PClass::FindClass(skillrepname) != NULL))
	{
		rep = PClass::FindClass(skillrepname)->ActorInfo;
	}
	rep = rep->GetReplacee (false);	Replacee = savedrep;
	return rep;
}

//==========================================================================
//
//
//==========================================================================

void FActorInfo::SetDamageFactor(FName type, fixed_t factor)
{
	if (factor != FRACUNIT) 
	{
		if (DamageFactors == NULL) DamageFactors=new DmgFactors;
		DamageFactors->Insert(type, factor);
	}
	else 
	{
		if (DamageFactors != NULL) 
			DamageFactors->Remove(type);
	}
}

//==========================================================================
//
//
//==========================================================================

void FActorInfo::SetPainChance(FName type, int chance)
{
	if (chance >= 0) 
	{
		if (PainChances == NULL) PainChances=new PainChanceList;
		PainChances->Insert(type, MIN(chance, 255));
	}
	else 
	{
		if (PainChances != NULL) 
			PainChances->Remove(type);
	}
}

//==========================================================================
//
//
//==========================================================================

FDoomEdMap DoomEdMap;

FDoomEdMap::FDoomEdEntry *FDoomEdMap::DoomEdHash[DOOMED_HASHSIZE];

FDoomEdMap::~FDoomEdMap()
{
	Empty();
}

void FDoomEdMap::AddType (int doomednum, const PClass *type, bool temporary)
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
	else if (!entry->temp)
	{
		Printf (PRINT_BOLD, "Warning: %s and %s both have doomednum %d.\n",
			type->TypeName.GetChars(), entry->Type->TypeName.GetChars(), doomednum);
	}
	entry->temp = temporary;
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

static void SummonActor (int command, int command2, FCommandLine argv)
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

		if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
		{
			const bool bSetAngle = ( argv.argc() > 2 );
			const SHORT sAngle = bSetAngle ? atoi (argv[2]) : 0;
			switch(command)
			{
			case DEM_SUMMON:
				CLIENTCOMMANDS_SummonCheat( type->TypeName.GetChars( ), CLC_SUMMONCHEAT, bSetAngle, sAngle );
				break;
			case DEM_SUMMONFRIEND:
				CLIENTCOMMANDS_SummonCheat( type->TypeName.GetChars( ), CLC_SUMMONFRIENDCHEAT, bSetAngle, sAngle );
				break;
			case DEM_SUMMONFOE:
				CLIENTCOMMANDS_SummonCheat( type->TypeName.GetChars( ), CLC_SUMMONFOECHEAT, bSetAngle, sAngle );
				break;
			}
			return;
		}

		Net_WriteByte (argv.argc() > 2 ? command2 : command);
		Net_WriteString (type->TypeName.GetChars());

		if (argv.argc () > 2) {
			Net_WriteWord (atoi (argv[2])); // angle
			if (argv.argc () > 3) Net_WriteWord (atoi (argv[3])); // TID
			else Net_WriteWord (0);
			if (argv.argc () > 4) Net_WriteByte (atoi (argv[4])); // special
			else Net_WriteByte (0);
			for(int i = 5; i < 10; i++) { // args[5]
				if(i < argv.argc()) Net_WriteLong (atoi (argv[i]));
				else Net_WriteLong (0);
			}
		}
	}
}

CCMD (summon)
{
	SummonActor (DEM_SUMMON, DEM_SUMMON2, argv);
}

CCMD (summonfriend)
{
	SummonActor (DEM_SUMMONFRIEND, DEM_SUMMONFRIEND2, argv);
}

CCMD (summonmbf)
{
	SummonActor (DEM_SUMMONMBF, DEM_SUMMONFRIEND2, argv);
}

CCMD (summonfoe)
{
	SummonActor (DEM_SUMMONFOE, DEM_SUMMONFOE2, argv);
}
