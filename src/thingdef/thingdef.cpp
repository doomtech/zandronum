/*
** thingdef.cpp
**
** Actor definitions
**
**---------------------------------------------------------------------------
** Copyright 2002-2008 Christoph Oelckers
** Copyright 2004-2008 Randy Heit
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
** 4. When not used as part of ZDoom or a ZDoom derivative, this code will be
**    covered by the terms of the GNU General Public License as published by
**    the Free Software Foundation; either version 2 of the License, or (at
**    your option) any later version.
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

#include "gi.h"
#include "actor.h"
#include "info.h"
#include "sc_man.h"
#include "tarray.h"
#include "w_wad.h"
#include "templates.h"
#include "r_defs.h"
#include "r_draw.h"
#include "a_pickups.h"
#include "s_sound.h"
#include "cmdlib.h"
#include "p_lnspec.h"
#include "a_action.h"
#include "decallib.h"
#include "m_random.h"
#include "i_system.h"
#include "p_local.h"
#include "doomerrors.h"
#include "a_hexenglobal.h"
#include "a_weaponpiece.h"
#include "p_conversation.h"
#include "v_text.h"
#include "thingdef.h"
#include "thingdef_exp.h"
#include "a_sharedglobal.h"

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------
void InitThingdef();
void ParseDecorate (FScanner &sc);

// STATIC FUNCTION PROTOTYPES --------------------------------------------
const PClass *QuestItemClasses[31];
PSymbolTable		 GlobalSymbols;

//==========================================================================
//
// Starts a new actor definition
//
//==========================================================================
FActorInfo *CreateNewActor(const FScriptPosition &sc, FName typeName, FName parentName, FName replaceName, 
								  int DoomEdNum, bool native)
{
	const PClass *replacee = NULL;
	PClass *ti = NULL;
	FActorInfo *info = NULL;

	PClass *parent = RUNTIME_CLASS(AActor);

	if (parentName != NAME_None)
	{
		parent = const_cast<PClass *> (PClass::FindClass (parentName));

		if (parent == NULL)
		{
			sc.Message(MSG_FATAL, "Parent type '%s' not found in %s", parentName.GetChars(), typeName.GetChars());
		}
		else if (!parent->IsDescendantOf(RUNTIME_CLASS(AActor)))
		{
			sc.Message(MSG_FATAL, "Parent type '%s' is not an actor in %s", parentName.GetChars(), typeName.GetChars());
		}
		else if (parent->ActorInfo == NULL)
		{
			sc.Message(MSG_FATAL, "uninitialized parent type '%s' in %s", parentName.GetChars(), typeName.GetChars());
		}
	}

	// Check for "replaces"
	if (replaceName != NAME_None)
	{
		// Get actor name
		replacee = PClass::FindClass (replaceName);

		if (replacee == NULL)
		{
			sc.Message(MSG_FATAL, "Replaced type '%s' not found in %s", replaceName.GetChars(), typeName.GetChars());
		}
		else if (replacee->ActorInfo == NULL)
		{
			sc.Message(MSG_FATAL, "Replaced type '%s' is not an actor in %s", replaceName.GetChars(), typeName.GetChars());
		}
	}

	if (native)
	{
		ti = (PClass*)PClass::FindClass(typeName);
		if (ti == NULL)
		{
			sc.Message(MSG_FATAL, "Unknown native class '%s'", typeName.GetChars());
		}
		else if (ti != RUNTIME_CLASS(AActor) && ti->ParentClass->NativeClass() != parent->NativeClass())
		{
			sc.Message(MSG_FATAL, "Native class '%s' does not inherit from '%s'", typeName.GetChars(), parentName.GetChars());
		}
		else if (ti->ActorInfo != NULL)
		{
			sc.Message(MSG_FATAL, "Redefinition of internal class '%s'", typeName.GetChars());
		}
		ti->InitializeActorInfo();
		info = ti->ActorInfo;
	}
	else
	{
		ti = parent->CreateDerivedClass (typeName, parent->Size);
		info = ti->ActorInfo;
	}

	info->DoomEdNum = -1;
	if (parent->ActorInfo->DamageFactors != NULL)
	{
		// copy damage factors from parent
		info->DamageFactors = new DmgFactors;
		*info->DamageFactors = *parent->ActorInfo->DamageFactors;
	}
	if (parent->ActorInfo->PainChances != NULL)
	{
		// copy pain chances from parent
		info->PainChances = new PainChanceList;
		*info->PainChances = *parent->ActorInfo->PainChances;
	}

	if (replacee != NULL)
	{
		replacee->ActorInfo->Replacement = ti->ActorInfo;
		ti->ActorInfo->Replacee = replacee->ActorInfo;
	}

	if (DoomEdNum > 0) info->DoomEdNum = DoomEdNum;
	return info;
}

//==========================================================================
//
// Finalizes an actor definition
//
//==========================================================================

void FinishActor(const FScriptPosition &sc, FActorInfo *info, Baggage &bag)
{
	AActor *defaults = (AActor*)info->Class->Defaults;

	try
	{
		bag.statedef.FinishStates (info, defaults, bag.StateArray);
	}
	catch (CRecoverableError &err)
	{
		sc.Message(MSG_FATAL, "%s", err.GetMessage());
	}
	bag.statedef.InstallStates (info, defaults);
	bag.StateArray.Clear ();
	if (bag.DropItemSet)
	{
		if (bag.DropItemList == NULL)
		{
			if (info->Class->Meta.GetMetaInt (ACMETA_DropItems) != 0)
			{
				info->Class->Meta.SetMetaInt (ACMETA_DropItems, 0);
			}
		}
		else
		{
			info->Class->Meta.SetMetaInt (ACMETA_DropItems,
				StoreDropItemChain(bag.DropItemList));
		}
	}
	if (info->Class->IsDescendantOf (RUNTIME_CLASS(AInventory)))
	{
		defaults->flags |= MF_SPECIAL;
	}
}

//==========================================================================
//
// Do some postprocessing after everything has been defined
// This also processes all the internal actors to adjust the type
// fields in the weapons
//
//==========================================================================

static int ResolvePointer(const PClass **pPtr, const PClass *owner, const PClass *destclass, const char *description)
{
	fuglyname v;

	v = *pPtr;
	if (v != NAME_None && v.IsValidName())
	{
		*pPtr = PClass::FindClass(v);
		if (!*pPtr)
		{
			Printf("Unknown %s '%s' in '%s'\n", description, v.GetChars(), owner->TypeName.GetChars());
			return 1;
		}
		else if (!(*pPtr)->IsDescendantOf(destclass))
		{
			*pPtr = NULL;
			Printf("Invalid %s '%s' in '%s'\n", description, v.GetChars(), owner->TypeName.GetChars());
			return 1;
		}
	}
	return 0;
}


static void FinishThingdef()
{
	unsigned int i;
	int errorcount;

	errorcount = StateParams.ResolveAll();

	for (i = 0;i < PClass::m_Types.Size(); i++)
	{
		PClass * ti = PClass::m_Types[i];

		// Skip non-actors
		if (!ti->IsDescendantOf(RUNTIME_CLASS(AActor))) continue;

		AActor *def = GetDefaultByType(ti);

		if (!def)
		{
			Printf("No ActorInfo defined for class '%s'\n", ti->TypeName.GetChars());
			errorcount++;
			continue;
		}

		// Friendlies never count as kills!
		if (def->flags & MF_FRIENDLY)
		{
			def->flags &=~MF_COUNTKILL;
		}

		if (ti->IsDescendantOf(RUNTIME_CLASS(AInventory)))
		{
			AInventory * defaults=(AInventory *)def;
			errorcount += ResolvePointer(&defaults->PickupFlash, ti, RUNTIME_CLASS(AActor), "pickup flash");
		}

		if (ti->IsDescendantOf(RUNTIME_CLASS(APowerupGiver)) && ti != RUNTIME_CLASS(APowerupGiver))
		{
			FString typestr;
			APowerupGiver * defaults=(APowerupGiver *)def;
			fuglyname v;

			v = defaults->PowerupType;
			if (v != NAME_None && v.IsValidName())
			{
				typestr.Format ("Power%s", v.GetChars());
				const PClass * powertype=PClass::FindClass(typestr);
				if (!powertype) powertype=PClass::FindClass(v.GetChars());

				if (!powertype)
				{
					Printf("Unknown powerup type '%s' in '%s'\n", v.GetChars(), ti->TypeName.GetChars());
					errorcount++;
				}
				else if (!powertype->IsDescendantOf(RUNTIME_CLASS(APowerup)))
				{
					Printf("Invalid powerup type '%s' in '%s'\n", v.GetChars(), ti->TypeName.GetChars());
					errorcount++;
				}
				else
				{
					defaults->PowerupType=powertype;
				}
			}
			else if (v == NAME_None)
			{
				Printf("No powerup type specified in '%s'\n", ti->TypeName.GetChars());
				errorcount++;
			}
		}

		// the typeinfo properties of weapons have to be fixed here after all actors have been declared
		if (ti->IsDescendantOf(RUNTIME_CLASS(AWeapon)))
		{
			AWeapon * defaults=(AWeapon *)def;
			errorcount += ResolvePointer(&defaults->AmmoType1, ti, RUNTIME_CLASS(AAmmo), "ammo type");
			errorcount += ResolvePointer(&defaults->AmmoType2, ti, RUNTIME_CLASS(AAmmo), "ammo type");
			errorcount += ResolvePointer(&defaults->SisterWeaponType, ti, RUNTIME_CLASS(AWeapon), "sister weapon type");

			FState * ready = ti->ActorInfo->FindState(NAME_Ready);
			FState * select = ti->ActorInfo->FindState(NAME_Select);
			FState * deselect = ti->ActorInfo->FindState(NAME_Deselect);
			FState * fire = ti->ActorInfo->FindState(NAME_Fire);

			// Consider any weapon without any valid state abstract and don't output a warning
			// This is for creating base classes for weapon groups that only set up some properties.
			if (ready || select || deselect || fire)
			{
				// Do some consistency checks. If these states are undefined the weapon cannot work!
				if (!ready)
				{
					Printf("Weapon %s doesn't define a ready state.\n", ti->TypeName.GetChars());
					errorcount++;
				}
				if (!select) 
				{
					Printf("Weapon %s doesn't define a select state.\n", ti->TypeName.GetChars());
					errorcount++;
				}
				if (!deselect) 
				{
					Printf("Weapon %s doesn't define a deselect state.\n", ti->TypeName.GetChars());
					errorcount++;
				}
				if (!fire) 
				{
					Printf("Weapon %s doesn't define a fire state.\n", ti->TypeName.GetChars());
					errorcount++;
				}
			}
		}
		// same for the weapon type of weapon pieces.
		else if (ti->IsDescendantOf(RUNTIME_CLASS(AWeaponPiece)))
		{
			AWeaponPiece * defaults=(AWeaponPiece *)def;
			errorcount += ResolvePointer(&defaults->WeaponClass, ti, RUNTIME_CLASS(AWeapon), "weapon type");
		}
	}
	if (errorcount > 0)
	{
		I_Error("%d errors during actor postprocessing", errorcount);
	}

	// Since these are defined in DECORATE now the table has to be initialized here.
	for(int i=0;i<31;i++)
	{
		char fmt[20];
		mysnprintf(fmt, countof(fmt), "QuestItem%d", i+1);
		QuestItemClasses[i] = PClass::FindClass(fmt);
	}

}



//==========================================================================
//
// LoadActors
//
// Called from FActor::StaticInit()
//
//==========================================================================

void LoadActors ()
{
	int lastlump, lump;

	InitThingdef();
	lastlump = 0;
	while ((lump = Wads.FindLump ("DECORATE", &lastlump)) != -1)
	{
		FScanner sc(lump);
		ParseDecorate (sc);
	}
	FinishThingdef();
}

