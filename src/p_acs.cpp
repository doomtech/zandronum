/*
** p_acs.cpp
** General BEHAVIOR management and ACS execution environment
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
** This code at one time made lots of little-endian assumptions.
** I think it should be fine on big-endian machines now, but I have no
** real way to test it.
*/

#include <assert.h>

#include "templates.h"
#include "doomdef.h"
#include "p_local.h"
#include "p_spec.h"
#include "g_level.h"
#include "s_sound.h"
#include "p_acs.h"
#include "p_saveg.h"
#include "p_lnspec.h"
#include "m_random.h"
#include "doomstat.h"
#include "c_console.h"
#include "c_dispatch.h"
#include "s_sndseq.h"
#include "i_system.h"
#include "i_movie.h"
#include "sbar.h"
#include "m_swap.h"
#include "a_sharedglobal.h"
#include "a_doomglobal.h"
#include "a_strifeglobal.h"
#include "v_video.h"
#include "w_wad.h"
#include "r_sky.h"
#include "gstrings.h"
#include "gi.h"
#include "sc_man.h"
#include "c_bind.h"
#include "info.h"
#include "r_translate.h"
#include "cmdlib.h"
#include "m_png.h"
#include "p_setup.h"
// [BB] New #includes.
#include "announcer.h"
#include "deathmatch.h"
#include "gamemode.h"
#include "g_game.h"
#include "team.h"
#include "cl_demo.h"
#include "cooperative.h"
#include "invasion.h"
#include "sv_commands.h"
#include "network/nettraffic.h"

#include "g_shared/a_pickups.h"

extern FILE *Logfile;

FRandom pr_acs ("ACS");


// I imagine this much stack space is probably overkill, but it could
// potentially get used with recursive functions.
#define STACK_SIZE 4096

#define CLAMPCOLOR(c)		(EColorRange)((unsigned)(c) >= NUM_TEXT_COLORS ? CR_UNTRANSLATED : (c))
#define HUDMSG_LOG			(0x80000000)
#define HUDMSG_COLORSTRING	(0x40000000)
#define LANGREGIONMASK		MAKE_ID(0,0,0xff,0xff)

// Flags for ReplaceTextures
#define NOT_BOTTOM			1
#define NOT_MIDDLE			2
#define NOT_TOP				4
#define NOT_FLOOR			8
#define NOT_CEILING			16

// Flags for SectorDamage

#define DAMAGE_PLAYERS				1
#define DAMAGE_NONPLAYERS			2
#define DAMAGE_IN_AIR				4
#define DAMAGE_SUBCLASSES_PROTECT	8

struct CallReturn
{
	CallReturn(int pc, ScriptFunction *func, FBehavior *module, SDWORD *locals, bool discard, FString &str)
		: ReturnFunction(func),
		  ReturnModule(module),
		  ReturnLocals(locals),
		  ReturnAddress(pc),
		  bDiscardResult(discard),
		  StringBuilder(str)
	{}

	ScriptFunction *ReturnFunction;
	FBehavior *ReturnModule;
	SDWORD *ReturnLocals;
	int ReturnAddress;
	int bDiscardResult;
	FString StringBuilder;
};

static DLevelScript *P_GetScriptGoing (AActor *who, line_t *where, int num, const ScriptPtr *code, FBehavior *module,
	bool lineSide, int arg0, int arg1, int arg2, int always);

struct FBehavior::ArrayInfo
{
	DWORD ArraySize;
	SDWORD *Elements;
};

TArray<FBehavior *> FBehavior::StaticModules;

// [BC] When true, any console commands/line specials were executed via the ConsoleCommand p-code.
static	bool	g_bCalledFromConsoleCommand = false;


//============================================================================
//
// Global and world variables
//
//============================================================================

// ACS variables with world scope
SDWORD ACS_WorldVars[NUM_WORLDVARS];
FWorldGlobalArray ACS_WorldArrays[NUM_WORLDVARS];

// ACS variables with global scope
SDWORD ACS_GlobalVars[NUM_GLOBALVARS];
FWorldGlobalArray ACS_GlobalArrays[NUM_GLOBALVARS];


//============================================================================
//
// On the fly strings
//
//============================================================================

#define LIB_ACSSTRINGS_ONTHEFLY 0x7fff
#define ACSSTRING_OR_ONTHEFLY (LIB_ACSSTRINGS_ONTHEFLY<<16)

TArray<FString>
	ACS_StringsOnTheFly,
	ACS_StringBuilderStack;

#define STRINGBUILDER_START(Builder) if (*Builder.GetChars() || ACS_StringBuilderStack.Size()) { ACS_StringBuilderStack.Push(Builder); Builder = ""; }
#define STRINGBUILDER_FINISH(Builder) if (!ACS_StringBuilderStack.Pop(Builder)) Builder = "";

//============================================================================
//
//
//
//============================================================================

void P_ClearACSVars(bool alsoglobal)
{
	int i;

	memset (ACS_WorldVars, 0, sizeof(ACS_WorldVars));
	for (i = 0; i < NUM_WORLDVARS; ++i)
	{
		ACS_WorldArrays[i].Clear ();
	}
	if (alsoglobal)
	{
		memset (ACS_GlobalVars, 0, sizeof(ACS_GlobalVars));
		for (i = 0; i < NUM_GLOBALVARS; ++i)
		{
			ACS_GlobalArrays[i].Clear ();
		}
	}
}

//============================================================================
//
//
//
//============================================================================

static void WriteVars (FILE *file, SDWORD *vars, size_t count, DWORD id)
{
	size_t i, j;

	for (i = 0; i < count; ++i)
	{
		if (vars[i] != 0)
			break;
	}
	if (i < count)
	{
		// Find last non-zero var. Anything beyond the last stored variable
		// will be zeroed at load time.
		for (j = count-1; j > i; --j)
		{
			if (vars[j] != 0)
				break;
		}
		FPNGChunkArchive arc (file, id);
		for (i = 0; i <= j; ++i)
		{
			DWORD var = vars[i];
			arc << var;
		}
	}
}

//============================================================================
//
//
//
//============================================================================

static void ReadVars (PNGHandle *png, SDWORD *vars, size_t count, DWORD id)
{
	size_t len = M_FindPNGChunk (png, id);
	size_t used = 0;

	if (len != 0)
	{
		DWORD var;
		size_t i;
		FPNGChunkArchive arc (png->File->GetFile(), id, len);
		used = len / 4;

		for (i = 0; i < used; ++i)
		{
			arc << var;
			vars[i] = var;
		}
		png->File->ResetFilePtr();
	}
	if (used < count)
	{
		memset (&vars[used], 0, (count-used)*4);
	}
}

//============================================================================
//
//
//
//============================================================================

static void WriteArrayVars (FILE *file, FWorldGlobalArray *vars, unsigned int count, DWORD id)
{
	unsigned int i, j;

	// Find the first non-empty array.
	for (i = 0; i < count; ++i)
	{
		if (vars[i].CountUsed() != 0)
			break;
	}
	if (i < count)
	{
		// Find last non-empty array. Anything beyond the last stored array
		// will be emptied at load time.
		for (j = count-1; j > i; --j)
		{
			if (vars[j].CountUsed() != 0)
				break;
		}
		FPNGChunkArchive arc (file, id);
		arc.WriteCount (i);
		arc.WriteCount (j);
		for (; i <= j; ++i)
		{
			arc.WriteCount (vars[i].CountUsed());

			FWorldGlobalArray::ConstIterator it(vars[i]);
			const FWorldGlobalArray::Pair *pair;

			while (it.NextPair (pair))
			{
				arc.WriteCount (pair->Key);
				arc.WriteCount (pair->Value);
			}
		}
	}
}

//============================================================================
//
//
//
//============================================================================

static void ReadArrayVars (PNGHandle *png, FWorldGlobalArray *vars, size_t count, DWORD id)
{
	size_t len = M_FindPNGChunk (png, id);
	unsigned int i, k;

	for (i = 0; i < count; ++i)
	{
		vars[i].Clear ();
	}

	if (len != 0)
	{
		DWORD max, size;
		FPNGChunkArchive arc (png->File->GetFile(), id, len);

		i = arc.ReadCount ();
		max = arc.ReadCount ();

		for (; i <= max; ++i)
		{
			size = arc.ReadCount ();
			for (k = 0; k < size; ++k)
			{
				SDWORD key, val;
				key = arc.ReadCount();
				val = arc.ReadCount();
				vars[i].Insert (key, val);
			}
		}
		png->File->ResetFilePtr();
	}
}

//============================================================================
//
//
//
//============================================================================

void P_ReadACSVars(PNGHandle *png)
{
	ReadVars (png, ACS_WorldVars, NUM_WORLDVARS, MAKE_ID('w','v','A','r'));
	ReadVars (png, ACS_GlobalVars, NUM_GLOBALVARS, MAKE_ID('g','v','A','r'));
	ReadArrayVars (png, ACS_WorldArrays, NUM_WORLDVARS, MAKE_ID('w','a','R','r'));
	ReadArrayVars (png, ACS_GlobalArrays, NUM_GLOBALVARS, MAKE_ID('g','a','R','r'));
}

//============================================================================
//
//
//
//============================================================================

void P_WriteACSVars(FILE *stdfile)
{
	WriteVars (stdfile, ACS_WorldVars, NUM_WORLDVARS, MAKE_ID('w','v','A','r'));
	WriteVars (stdfile, ACS_GlobalVars, NUM_GLOBALVARS, MAKE_ID('g','v','A','r'));
	WriteArrayVars (stdfile, ACS_WorldArrays, NUM_WORLDVARS, MAKE_ID('w','a','R','r'));
	WriteArrayVars (stdfile, ACS_GlobalArrays, NUM_GLOBALVARS, MAKE_ID('g','a','R','r'));
}

//---- Inventory functions --------------------------------------//
//

//============================================================================
//
// DoClearInv
//
// Clears the inventory of a single actor.
//
//============================================================================

// [BL] We're exporting this now
void DoClearInv (AActor *actor)
{
	AInventory *inv = actor->Inventory;

	while (inv != NULL)
	{
		AInventory *next = inv->Inventory;
		if (!(inv->ItemFlags & IF_UNDROPPABLE))
		{
			// Fix for undroppable weapons. Is this ok?
			if (inv->IsKindOf(RUNTIME_CLASS(AAmmo)))
			{
				AAmmo *ammo = static_cast<AAmmo*>(inv);
				ammo->Amount = 0;
			}
			// [BB] Don't delete BasicArmor, just clear it. Probably ZDoom should do it like this
			// as well, but to be on the safe side only do this on the client for now.
			else if ( NETWORK_InClientMode() && ( inv->GetClass() == RUNTIME_CLASS(ABasicArmor) ) )
			{
				ABasicArmor *barmor = static_cast<ABasicArmor *> (inv);
				barmor->SavePercent = 0;
				barmor->Amount = 0;
			}
			else
				inv->Destroy ();
		}
		else if (inv->GetClass() == RUNTIME_CLASS(AHexenArmor))
		{
			AHexenArmor *harmor = static_cast<AHexenArmor *> (inv);
			harmor->Slots[3] = harmor->Slots[2] = harmor->Slots[1] = harmor->Slots[0] = 0;
		}
		inv = next;
	}
	if (actor->player != NULL)
	{
		actor->player->ReadyWeapon = NULL;
		actor->player->PendingWeapon = WP_NOCHANGE;
		actor->player->psprites[ps_weapon].state = NULL;
		actor->player->psprites[ps_flash].state = NULL;
		// [BB] If we're the server, tell the client he lost his inventory.
		if (( NETWORK_GetState( ) == NETSTATE_SERVER ))
			SERVERCOMMANDS_DestroyAllInventory( actor->player - players );
	}
}

//============================================================================
//
// ClearInventory
//
// Clears the inventory for one or more actors.
//
//============================================================================

static void ClearInventory (AActor *activator)
{
	if (activator == NULL)
	{
		for (int i = 0; i < MAXPLAYERS; ++i)
		{
			if (playeringame[i])
				DoClearInv (players[i].mo);
		}
	}
	else
	{
		DoClearInv (activator);
	}
}

//============================================================================
//
// DoGiveInv
//
// Gives an item to a single actor.
//
//============================================================================

// [BB] I need this outside p_acs.cpp. Furthermore it now returns whether CallTryPickup was successful.
/*static*/ bool DoGiveInv (AActor *actor, const PClass *info, int amount)
{
	// [BB]
	bool bSuccess = true;

	AWeapon *savedPendingWeap = actor->player != NULL
		? actor->player->PendingWeapon : NULL;
	bool hadweap = actor->player != NULL ? actor->player->ReadyWeapon != NULL : true;

	AInventory *item = static_cast<AInventory *>(Spawn (info, 0,0,0, NO_REPLACE));

	// This shouldn't count for the item statistics!
	if (item->flags & MF_COUNTITEM)
	{
		level.total_items--;
		item->flags &= ~MF_COUNTITEM;
	}
	if (info->IsDescendantOf (RUNTIME_CLASS(ABasicArmorPickup)))
	{
		if (static_cast<ABasicArmorPickup*>(item)->SaveAmount != 0)
		{
			static_cast<ABasicArmorPickup*>(item)->SaveAmount *= amount;
		}
		else
		{
			static_cast<ABasicArmorPickup*>(item)->SaveAmount *= amount;
		}
	}
	else if (info->IsDescendantOf (RUNTIME_CLASS(ABasicArmorBonus)))
	{
		static_cast<ABasicArmorBonus*>(item)->SaveAmount *= amount;
	}
	else
	{
		item->Amount = amount;
	}
	if (!item->CallTryPickup (actor))
	{
		item->Destroy ();
		// [BC] Also set item to NULL.
		item = NULL;
		// [BB] CallTryPickup was not successful.
		bSuccess = false;
	}
	// If the item was a weapon, don't bring it up automatically
	// unless the player was not already using a weapon.
	if (savedPendingWeap != NULL && hadweap && actor->player != NULL)
	{
		actor->player->PendingWeapon = savedPendingWeap;
	}

	// [BB] Since SERVERCOMMANDS_GiveInventory overwrites the item amount
	// of the client with item->Amount, we have have to set this to the
	// correct amount the player has.
	AInventory *pInventory = NULL;
	if( actor->player )
	{
		if ( actor->player->mo )
			pInventory = actor->player->mo->FindInventory( info );
	}
	if ( pInventory != NULL && item != NULL )
		item->Amount = pInventory->Amount;

	// [BC] If we're the server, give the item to clients.
	if (( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( actor->player ) && ( item ))
	{
		SERVERCOMMANDS_GiveInventory( actor->player - players, item );
		// [BB] The armor display amount has to be updated separately.
		if( item->GetClass()->IsDescendantOf (RUNTIME_CLASS(AArmor)))
		  SERVERCOMMANDS_SetPlayerArmor( actor->player - players );
	}

	// [BB]
	return bSuccess;
}

//============================================================================
//
// GiveInventory
//
// Gives an item to one or more actors.
//
//============================================================================

static void GiveInventory (AActor *activator, const char *type, int amount)
{
	const PClass *info;

	if (amount <= 0 || type == NULL)
	{
		return;
	}
	if (stricmp (type, "Armor") == 0)
	{
		type = "BasicArmorPickup";
	}
	info = PClass::FindClass (type);
	if (info == NULL)
	{
		Printf ("ACS: I don't know what %s is.\n", type);
	}
	else if (!info->IsDescendantOf (RUNTIME_CLASS(AInventory)))
	{
		Printf ("ACS: %s is not an inventory item.\n", type);
	}
	else if (activator == NULL)
	{
		for (int i = 0; i < MAXPLAYERS; ++i)
		{
			if (playeringame[i])
				DoGiveInv (players[i].mo, info, amount);
		}
	}
	else
	{
		DoGiveInv (activator, info, amount);
	}
}

//============================================================================
//
// DoTakeInv
//
// Takes an item from a single actor.
//
//============================================================================

static void DoTakeInv (AActor *actor, const PClass *info, int amount)
{
	AInventory *item = actor->FindInventory (info);
	if (item != NULL)
	{
		item->Amount -= amount;
		// [BC] If we're the server, tell clients to take the item away.
		// [BB] We may not pass a negative amount to SERVERCOMMANDS_TakeInventory.
		if (( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( actor->player ))
			SERVERCOMMANDS_TakeInventory( actor->player - players, item->GetClass( )->TypeName.GetChars( ), MAX ( 0, item->Amount ) );
		if (item->Amount <= 0)
		{
			// If it's not ammo or an internal armor, destroy it.
			// Ammo needs to stick around, even when it's zero for the benefit
			// of the weapons that use it and to maintain the maximum ammo
			// amounts a backpack might have given.
			// Armor shouldn't be removed because they only work properly when
			// they are the last items in the inventory.
			if (item->ItemFlags & IF_KEEPDEPLETED)
			{
				item->Amount = 0;
			}
			else
			{
				item->Destroy ();
			}
		}
	}
}

//============================================================================
//
// TakeInventory
//
// Takes an item from one or more actors.
//
//============================================================================

static void TakeInventory (AActor *activator, const char *type, int amount)
{
	const PClass *info;

	if (type == NULL)
	{
		return;
	}
	if (strcmp (type, "Armor") == 0)
	{
		type = "BasicArmor";
	}
	if (amount <= 0)
	{
		return;
	}
	info = PClass::FindClass (type);
	if (info == NULL)
	{
		return;
	}
	if (activator == NULL)
	{
		for (int i = 0; i < MAXPLAYERS; ++i)
		{
			if (playeringame[i])
				DoTakeInv (players[i].mo, info, amount);
		}
	}
	else
	{
		DoTakeInv (activator, info, amount);
	}
}

//============================================================================
//
// DoUseInv
//
// Makes a single actor use an inventory item
//
//============================================================================

static bool DoUseInv (AActor *actor, const PClass *info)
{
	AInventory *item = actor->FindInventory (info);
	if (item != NULL)
	{
		return actor->UseInventory(item);
	}
	return false;
}

//============================================================================
//
// UseInventory
//
// makes one or more actors use an inventory item.
//
//============================================================================

static int UseInventory (AActor *activator, const char *type)
{
	const PClass *info;
	int ret = 0;

	if (type == NULL)
	{
		return 0;
	}
	info = PClass::FindClass (type);
	if (info == NULL)
	{
		return 0;
	}
	if (activator == NULL)
	{
		for (int i = 0; i < MAXPLAYERS; ++i)
		{
			if (playeringame[i])
				ret += DoUseInv (players[i].mo, info);
		}
	}
	else
	{
		ret = DoUseInv (activator, info);
	}
	return ret;
}

//============================================================================
//
// CheckInventory
//
// Returns how much of a particular item an actor has.
//
//============================================================================

static int CheckInventory (AActor *activator, const char *type)
{
	if (activator == NULL || type == NULL)
		return 0;

	if (stricmp (type, "Armor") == 0)
	{
		type = "BasicArmor";
	}
	else if (stricmp (type, "Health") == 0)
	{
		return activator->health;
	}

	const PClass *info = PClass::FindClass (type);
	AInventory *item = activator->FindInventory (info);
	return item ? item->Amount : 0;
}

//============================================================================
//
// GetTeamScore
//
// [Dusk] Moved this out of ACSF_GetTeamScore
//
//============================================================================
static LONG GetTeamScore (ULONG team) {
	if ( GAMEMODE_GetFlags(GAMEMODE_GetCurrentMode()) & GMF_PLAYERSEARNFRAGS )
		return TEAM_GetFragCount( team );
	else if ( GAMEMODE_GetFlags(GAMEMODE_GetCurrentMode()) & GMF_PLAYERSEARNWINS )
		return TEAM_GetWinCount( team );
	return TEAM_GetScore( team );
}

//============================================================================
//
// [Dusk] GetTeamProperty
//
// Returns some information of a team
//
//============================================================================
static int GetTeamProperty (unsigned int team, int prop) {
	switch (prop) {
		case TPROP_NumLivePlayers:
			return TEAM_CountLivingAndRespawnablePlayers (team);
		case TPROP_IsValid:
			return TEAM_CheckIfValid (team);
		case TPROP_Score:
			return GetTeamScore (team);
		case TPROP_TextColor:
			return TEAM_GetTextColor (team);
		case TPROP_PlayerStartNum:
			return TEAM_GetPlayerStartThingNum (team);
		case TPROP_Spread:
			return TEAM_GetSpread (team, &GetTeamScore);
		case TPROP_Assister:
		{
			// [Dusk] Assister is MAXPLAYERS if there is nobody eligible for an assist.
			// We return -1 instead of MAXPLAYERS for consistency (see PlayerNumber()).
			// It's also easier to check against in ACS due to lack of MAXPLAYERS in ACS.
			if (TEAM_GetAssistPlayer (team) == MAXPLAYERS)
				return -1;
			return TEAM_GetAssistPlayer (team);
		}
		case TPROP_Carrier:
			// [Dusk] However, carrier player is a player_t* and is NULL if there is no carrier.
			if (!TEAM_GetCarrier (team))
				return -1;
			return TEAM_GetCarrier (team) - players;
		case TPROP_FragCount:
			return TEAM_GetFragCount (team);
		case TPROP_DeathCount:
			return TEAM_GetDeathCount (team);
		case TPROP_WinCount:
			return TEAM_GetWinCount (team);
		case TPROP_PointCount:
			return TEAM_GetScore (team);
		case TPROP_ReturnTics:
			return TEAM_GetReturnTicks (team);
		case TPROP_NumPlayers:
			return TEAM_CountPlayers (team);
		// [Dusk] Now for the strings! Using dynamic strings for this.
		case TPROP_TeamItem:
		case TPROP_WinnerTheme:
		case TPROP_LoserTheme:
		case TPROP_Name:
		{
			FString work;
			int res;

			STRINGBUILDER_START(work);
			switch (prop) {
			case TPROP_TeamItem:
				work += TEAM_GetTeamItemName (team);
				break;
			case TPROP_WinnerTheme:
			case TPROP_LoserTheme:
				work += TEAM_GetIntermissionTheme (team, prop == TPROP_WinnerTheme);
				break;
			case TPROP_Name:
				work += TEAM_GetName (team);
				break;
			}

			// [Dusk] Copied over from PCD_SAVESTRING - not touching ZDoom code like this..
			unsigned int str_otf = ACS_StringsOnTheFly.Push(strbin1(work));
			if (str_otf > 0xffff)
				res = (-1);
			else
				res = ((SDWORD)str_otf|ACSSTRING_OR_ONTHEFLY);

			STRINGBUILDER_FINISH(work);
			return res;
		}
	}
	return 0;
}

//---- Plane watchers ----//

class DPlaneWatcher : public DThinker
{
	DECLARE_CLASS (DPlaneWatcher, DThinker)
	HAS_OBJECT_POINTERS
public:
	DPlaneWatcher (AActor *it, line_t *line, int lineSide, bool ceiling,
		int tag, int height, int special,
		int arg0, int arg1, int arg2, int arg3, int arg4);
	void Tick ();
	void Serialize (FArchive &arc);
private:
	sector_t *Sector;
	fixed_t WatchD, LastD;
	int Special, Arg0, Arg1, Arg2, Arg3, Arg4;
	TObjPtr<AActor> Activator;
	line_t *Line;
	bool LineSide;
	bool bCeiling;

	DPlaneWatcher() {}
};

IMPLEMENT_POINTY_CLASS (DPlaneWatcher)
 DECLARE_POINTER (Activator)
END_POINTERS

DPlaneWatcher::DPlaneWatcher (AActor *it, line_t *line, int lineSide, bool ceiling,
	int tag, int height, int special,
	int arg0, int arg1, int arg2, int arg3, int arg4)
	: Special (special), Arg0 (arg0), Arg1 (arg1), Arg2 (arg2), Arg3 (arg3), Arg4 (arg4),
	  Activator (it), Line (line), LineSide (!!lineSide), bCeiling (ceiling)
{
	int secnum;

	secnum = P_FindSectorFromTag (tag, -1);
	if (secnum >= 0)
	{
		secplane_t plane;

		Sector = &sectors[secnum];
		if (bCeiling)
		{
			plane = Sector->ceilingplane;
		}
		else
		{
			plane = Sector->floorplane;
		}
		LastD = plane.d;
		plane.ChangeHeight (height << FRACBITS);
		WatchD = plane.d;
	}
	else
	{
		Sector = NULL;
		WatchD = LastD = 0;
	}
}

void DPlaneWatcher::Serialize (FArchive &arc)
{
	Super::Serialize (arc);

	arc << Special << Arg0 << Arg1 << Arg2 << Arg3 << Arg4
		<< Sector << bCeiling << WatchD << LastD << Activator
		<< Line << LineSide << bCeiling;
}

void DPlaneWatcher::Tick ()
{
	if (Sector == NULL)
	{
		Destroy ();
		return;
	}

	fixed_t newd;

	if (bCeiling)
	{
		newd = Sector->ceilingplane.d;
	}
	else
	{
		newd = Sector->floorplane.d;
	}

	if ((LastD < WatchD && newd >= WatchD) ||
		(LastD > WatchD && newd <= WatchD))
	{
		LineSpecials[Special] (Line, Activator, LineSide, Arg0, Arg1, Arg2, Arg3, Arg4);
		Destroy ();
	}

}

//---- ACS lump manager ----//

// Load user-specified default modules. This must be called after the level's
// own behavior is loaded (if it has one).
void FBehavior::StaticLoadDefaultModules ()
{
	// When playing Strife, STRFHELP is always loaded.
	if (gameinfo.gametype == GAME_Strife)
	{
		StaticLoadModule (Wads.CheckNumForName ("STRFHELP", ns_acslibrary));
	}

	// Scan each LOADACS lump and load the specified modules in order
	int lump, lastlump = 0;

	while ((lump = Wads.FindLump ("LOADACS", &lastlump)) != -1)
	{
		FScanner sc(lump);
		while (sc.GetString())
		{
			int acslump = Wads.CheckNumForName (sc.String, ns_acslibrary);
			if (acslump >= 0)
			{
				StaticLoadModule (acslump);
			}
			else
			{
				Printf ("Could not find autoloaded ACS library %s\n", sc.String);
			}
		}
	}
}

FBehavior *FBehavior::StaticLoadModule (int lumpnum, FileReader *fr, int len)
{
	if (lumpnum == -1 && fr == NULL) return NULL;

	for (unsigned int i = 0; i < StaticModules.Size(); ++i)
	{
		if (StaticModules[i]->LumpNum == lumpnum)
		{
			return StaticModules[i];
		}
	}

	return new FBehavior (lumpnum, fr, len);
}

bool FBehavior::StaticCheckAllGood ()
{
	for (unsigned int i = 0; i < StaticModules.Size(); ++i)
	{
		if (!StaticModules[i]->IsGood())
		{
			return false;
		}
	}
	return true;
}

void FBehavior::StaticUnloadModules ()
{
	for (unsigned int i = StaticModules.Size(); i-- > 0; )
	{
		delete StaticModules[i];
	}
	StaticModules.Clear ();
}

FBehavior *FBehavior::StaticGetModule (int lib)
{
	if ((size_t)lib >= StaticModules.Size())
	{
		return NULL;
	}
	return StaticModules[lib];
}

void FBehavior::StaticSerializeModuleStates (FArchive &arc)
{
	DWORD modnum;

	modnum = StaticModules.Size();
	arc << modnum;

	if (modnum != StaticModules.Size())
	{
		I_Error ("Level was saved with a different number of ACS modules.");
	}

	for (modnum = 0; modnum < StaticModules.Size(); ++modnum)
	{
		FBehavior *module = StaticModules[modnum];

		if (arc.IsStoring())
		{
			arc.WriteString (module->ModuleName);
		}
		else
		{
			char *modname = NULL;
			arc << modname;
			if (stricmp (modname, module->ModuleName) != 0)
			{
				delete[] modname;
				I_Error ("Level was saved with a different set of ACS modules.");
			}
			delete[] modname;
		}
		module->SerializeVars (arc);
	}
}

void FBehavior::SerializeVars (FArchive &arc)
{
	SerializeVarSet (arc, MapVarStore, NUM_MAPVARS);
	for (int i = 0; i < NumArrays; ++i)
	{
		SerializeVarSet (arc, ArrayStore[i].Elements, ArrayStore[i].ArraySize);
	}
}

void FBehavior::SerializeVarSet (FArchive &arc, SDWORD *vars, int max)
{
	SDWORD arcval;
	SDWORD first, last;

	if (arc.IsStoring ())
	{
		// Find first non-zero variable
		for (first = 0; first < max; ++first)
		{
			if (vars[first] != 0)
			{
				break;
			}
		}

		// Find last non-zero variable
		for (last = max - 1; last >= first; --last)
		{
			if (vars[last] != 0)
			{
				break;
			}
		}

		if (last < first)
		{ // no non-zero variables
			arcval = 0;
			arc << arcval;
			return;
		}

		arcval = last - first + 1;
		arc << arcval;
		arcval = first;
		arc << arcval;

		while (first <= last)
		{
			arc << vars[first];
			++first;
		}
	}
	else
	{
		SDWORD truelast;

		arc << last;
		if (last == 0)
		{
			return;
		}
		arc << first;
		last += first;
		truelast = last;

		if (last > max)
		{
			last = max;
		}

		memset (vars, 0, max*sizeof(*vars));

		while (first < last)
		{
			arc << vars[first];
			++first;
		}
		while (first < truelast)
		{
			arc << arcval;
			++first;
		}
	}
}

FBehavior::FBehavior (int lumpnum, FileReader * fr, int len)
{
	BYTE *object;
	int i;

	NumScripts = 0;
	NumFunctions = 0;
	NumArrays = 0;
	NumTotalArrays = 0;
	Scripts = NULL;
	Functions = NULL;
	Arrays = NULL;
	ArrayStore = NULL;
	Chunks = NULL;
	Data = NULL;
	Format = ACS_Unknown;
	LumpNum = lumpnum;
	memset (MapVarStore, 0, sizeof(MapVarStore));
	ModuleName[0] = 0;

	// Now that everything is set up, record this module as being among the loaded modules.
	// We need to do this before resolving any imports, because an import might (indirectly)
	// need to resolve exports in this module. The only things that can be exported are
	// functions and map variables, which must already be present if they're exported, so
	// this is okay.

	// This must be done first for 2 reasons:
	// 1. If not, corrupt modules cause memory leaks
	// 2. Corrupt modules won't be reported when a level is being loaded if this function quits before
	//    adding it to the list.
    LibraryID = StaticModules.Push (this) << 16;

	if (fr == NULL) len = Wads.LumpLength (lumpnum);

	// Any behaviors smaller than 32 bytes cannot possibly contain anything useful.
	// (16 bytes for a completely empty behavior + 12 bytes for one script header
	//  + 4 bytes for PCD_TERMINATE for an old-style object. A new-style object
	// has 24 bytes if it is completely empty. An empty SPTR chunk adds 8 bytes.)
	if (len < 32)
	{
		return;
	}

	object = new BYTE[len];
	if (fr == NULL)
	{
		Wads.ReadLump (lumpnum, object);
	}
	else
	{
		fr->Read (object, len);
	}

	if (object[0] != 'A' || object[1] != 'C' || object[2] != 'S')
	{
		delete[] object;
		return;
	}

	switch (object[3])
	{
	case 0:
		Format = ACS_Old;
		break;
	case 'E':
		Format = ACS_Enhanced;
		break;
	case 'e':
		Format = ACS_LittleEnhanced;
		break;
	default:
		delete[] object;
		return;
	}

	if (fr == NULL)
	{
		Wads.GetLumpName (ModuleName, lumpnum);
		ModuleName[8] = 0;
	}
	else
	{
		strcpy(ModuleName, "BEHAVIOR");
	}

	Data = object;
	DataSize = len;

	if (Format == ACS_Old)
	{
		DWORD dirofs = LittleLong(((DWORD *)object)[1]);
		DWORD pretag = ((DWORD *)(object + dirofs))[-1];

		Chunks = object + len;
		// Check for redesigned ACSE/ACSe
		if (dirofs >= 6*4 &&
			(pretag == MAKE_ID('A','C','S','e') ||
			 pretag == MAKE_ID('A','C','S','E')))
		{
			Format = (pretag == MAKE_ID('A','C','S','e')) ? ACS_LittleEnhanced : ACS_Enhanced;
			Chunks = object + LittleLong(((DWORD *)(object + dirofs))[-2]);
			// Forget about the compatibility cruft at the end of the lump
			DataSize = LittleLong(((DWORD *)object)[1]) - 8;
		}
	}
	else
	{
		Chunks = object + LittleLong(((DWORD *)object)[1]);
	}

	LoadScriptsDirectory ();

	if (Format == ACS_Old)
	{
		StringTable = ((DWORD *)Data)[1];
		StringTable += ((DWORD *)(Data + StringTable))[0] * 12 + 4;
	}
	else
	{
		UnencryptStrings ();
		BYTE *strings = FindChunk (MAKE_ID('S','T','R','L'));
		if (strings != NULL)
		{
			StringTable = DWORD(strings - Data + 8);
		}
		else
		{
			StringTable = 0;
		}
	}

	if (Format == ACS_Old)
	{
		// Do initialization for old-style behavior lumps
		for (i = 0; i < NUM_MAPVARS; ++i)
		{
			MapVars[i] = &MapVarStore[i];
		}
		//LibraryID = StaticModules.Push (this) << 16;
	}
	else
	{
		DWORD *chunk;

		Functions = FindChunk (MAKE_ID('F','U','N','C'));
		if (Functions != NULL)
		{
			NumFunctions = LittleLong(((DWORD *)Functions)[1]) / 8;
			Functions += 8;
		}

		// Initialize this object's map variables
		memset (MapVarStore, 0, sizeof(MapVarStore));
		chunk = (DWORD *)FindChunk (MAKE_ID('M','I','N','I'));
		while (chunk != NULL)
		{
			int numvars = LittleLong(chunk[1])/4 - 1;
			int firstvar = LittleLong(chunk[2]);
			for (i = 0; i < numvars; ++i)
			{
				MapVarStore[i+firstvar] = LittleLong(chunk[3+i]);
			}
			chunk = (DWORD *)NextChunk ((BYTE *)chunk);
		}

		// Initialize this object's map variable pointers to defaults. They can be changed
		// later once the imported modules are loaded.
		for (i = 0; i < NUM_MAPVARS; ++i)
		{
			MapVars[i] = &MapVarStore[i];
		}

		// Create arrays for this module
		chunk = (DWORD *)FindChunk (MAKE_ID('A','R','A','Y'));
		if (chunk != NULL)
		{
			NumArrays = LittleLong(chunk[1])/8;
			ArrayStore = new ArrayInfo[NumArrays];
			memset (ArrayStore, 0, sizeof(*Arrays)*NumArrays);
			for (i = 0; i < NumArrays; ++i)
			{
				MapVarStore[LittleLong(chunk[2+i*2])] = i;
				ArrayStore[i].ArraySize = LittleLong(chunk[3+i*2]);
				ArrayStore[i].Elements = new SDWORD[ArrayStore[i].ArraySize];
				memset(ArrayStore[i].Elements, 0, ArrayStore[i].ArraySize*sizeof(DWORD));
			}
		}

		// Initialize arrays for this module
		chunk = (DWORD *)FindChunk (MAKE_ID('A','I','N','I'));
		while (chunk != NULL)
		{
			int arraynum = MapVarStore[LittleLong(chunk[2])];
			if ((unsigned)arraynum < (unsigned)NumArrays)
			{
				int initsize = MIN<int> (ArrayStore[arraynum].ArraySize, (LittleLong(chunk[1])-4)/4);
				SDWORD *elems = ArrayStore[arraynum].Elements;
				for (i = 0; i < initsize; ++i)
				{
					elems[i] = LittleLong(chunk[3+i]);
				}
			}
			chunk = (DWORD *)NextChunk((BYTE *)chunk);
		}

		// Start setting up array pointers
		NumTotalArrays = NumArrays;
		chunk = (DWORD *)FindChunk (MAKE_ID('A','I','M','P'));
		if (chunk != NULL)
		{
			NumTotalArrays += LittleLong(chunk[2]);
		}
		if (NumTotalArrays != 0)
		{
			Arrays = new ArrayInfo *[NumTotalArrays];
			for (i = 0; i < NumArrays; ++i)
			{
				Arrays[i] = &ArrayStore[i];
			}
		}

		// Tag the library ID to any map variables that are initialized with strings
		if (LibraryID != 0)
		{
			chunk = (DWORD *)FindChunk (MAKE_ID('M','S','T','R'));
			if (chunk != NULL)
			{
				for (DWORD i = 0; i < chunk[1]/4; ++i)
				{
					MapVarStore[chunk[i+2]] |= LibraryID;
				}
			}

			chunk = (DWORD *)FindChunk (MAKE_ID('A','S','T','R'));
			if (chunk != NULL)
			{
				for (DWORD i = 0; i < chunk[1]/4; ++i)
				{
					int arraynum = MapVarStore[LittleLong(chunk[i+2])];
					if ((unsigned)arraynum < (unsigned)NumArrays)
					{
						SDWORD *elems = ArrayStore[arraynum].Elements;
						for (int j = ArrayStore[arraynum].ArraySize; j > 0; --j, ++elems)
						{
							*elems |= LibraryID;
						}
					}
				}
			}
		}

		if (NULL != (chunk = (DWORD *)FindChunk (MAKE_ID('L','O','A','D'))))
		{
			const char *const parse = (char *)&chunk[2];
			DWORD i;

			for (i = 0; i < chunk[1]; )
			{
				if (parse[i])
				{
					FBehavior *module = NULL;
					int lump = Wads.CheckNumForName (&parse[i], ns_acslibrary);
					if (lump < 0)
					{
						Printf ("Could not find ACS library %s.\n", &parse[i]);
					}
					else
					{
						module = StaticLoadModule (lump);
					}
					if (module != NULL) Imports.Push (module);
					do ; while (parse[++i]);
				}
				++i;
			}

			// Go through each imported module in order and resolve all imported functions
			// and map variables.
			for (i = 0; i < Imports.Size(); ++i)
			{
				FBehavior *lib = Imports[i];
				int j;

				if (lib == NULL)
					continue;

				// Resolve functions
				chunk = (DWORD *)FindChunk(MAKE_ID('F','N','A','M'));
				for (j = 0; j < NumFunctions; ++j)
				{
					ScriptFunction *func = &((ScriptFunction *)Functions)[j];
					if (func->Address == 0 && func->ImportNum == 0)
					{
						int libfunc = lib->FindFunctionName ((char *)(chunk + 2) + chunk[3+j]);
						if (libfunc >= 0)
						{
							ScriptFunction *realfunc = &((ScriptFunction *)lib->Functions)[libfunc];
							// Make sure that the library really defines this function. It might simply
							// be importing it itself.
							if (realfunc->Address != 0 && realfunc->ImportNum == 0)
							{
								func->Address = libfunc;
								func->ImportNum = i+1;
								if (realfunc->ArgCount != func->ArgCount)
								{
									Printf ("Function %s in %s has %d arguments. %s expects it to have %d.\n",
										(char *)(chunk + 2) + chunk[3+j], lib->ModuleName, realfunc->ArgCount,
										ModuleName, func->ArgCount);
									Format = ACS_Unknown;
								}
								// The next two properties do not effect code compatibility, so it is
								// okay for them to be different in the imported module than they are
								// in this one, as long as we make sure to use the real values.
								func->LocalCount = realfunc->LocalCount;
								func->HasReturnValue = realfunc->HasReturnValue;
							}
						}
					}
				}

				// Resolve map variables
				chunk = (DWORD *)FindChunk(MAKE_ID('M','I','M','P'));
				if (chunk != NULL)
				{
					char *parse = (char *)&chunk[2];
					for (DWORD j = 0; j < chunk[1]; )
					{
						DWORD varNum = LittleLong(*(DWORD *)&parse[j]);
						j += 4;
						int impNum = lib->FindMapVarName (&parse[j]);
						if (impNum >= 0)
						{
							MapVars[varNum] = &lib->MapVarStore[impNum];
						}
						do ; while (parse[++j]);
						++j;
					}
				}

				// Resolve arrays
				if (NumTotalArrays > NumArrays)
				{
					chunk = (DWORD *)FindChunk(MAKE_ID('A','I','M','P'));
					char *parse = (char *)&chunk[3];
					for (DWORD j = 0; j < LittleLong(chunk[2]); ++j)
					{
						DWORD varNum = LittleLong(*(DWORD *)parse);
						parse += 4;
						DWORD expectedSize = LittleLong(*(DWORD *)parse);
						parse += 4;
						int impNum = lib->FindMapArray (parse);
						if (impNum >= 0)
						{
							Arrays[NumArrays + j] = &lib->ArrayStore[impNum];
							MapVarStore[varNum] = NumArrays + j;
							if (lib->ArrayStore[impNum].ArraySize != expectedSize)
							{
								Format = ACS_Unknown;
								Printf ("The array %s in %s has %u elements, but %s expects it to only have %u.\n",
									parse, lib->ModuleName, lib->ArrayStore[impNum].ArraySize,
									ModuleName, expectedSize);
							}
						}
						do ; while (*++parse);
						++parse;
					}
				}
			}
		}
	}

	DPrintf ("Loaded %d scripts, %d functions\n", NumScripts, NumFunctions);
}

FBehavior::~FBehavior ()
{
	if (Scripts != NULL)
	{
		delete[] Scripts;
		Scripts = NULL;
	}
	if (Arrays != NULL)
	{
		delete[] Arrays;
		Arrays = NULL;
	}
	if (ArrayStore != NULL)
	{
		for (int i = 0; i < NumArrays; ++i)
		{
			if (ArrayStore[i].Elements != NULL)
			{
				delete[] ArrayStore[i].Elements;
				ArrayStore[i].Elements = NULL;
			}
		}
		delete[] ArrayStore;
		ArrayStore = NULL;
	}
	if (Data != NULL)
	{
		delete[] Data;
		Data = NULL;
	}
}

void FBehavior::LoadScriptsDirectory ()
{
	union
	{
		BYTE *b;
		DWORD *dw;
		WORD *w;
		ScriptPtr2 *po;		// Old
		ScriptPtr1 *pi;		// Intermediate
		ScriptPtr3 *pe;		// LittleEnhanced
	} scripts;
	int i, max;

	NumScripts = 0;
	Scripts = NULL;

	// Load the main script directory
	switch (Format)
	{
	case ACS_Old:
		scripts.dw = (DWORD *)(Data + ((DWORD *)Data)[1]);	// FIXME: Has this been byte-swapped before-hand?
		NumScripts = scripts.dw[0];
		if (NumScripts != 0)
		{
			scripts.dw++;

			Scripts = new ScriptPtr[NumScripts];

			for (i = 0; i < NumScripts; ++i)
			{
				ScriptPtr2 *ptr1 = &scripts.po[i];
				ScriptPtr  *ptr2 = &Scripts[i];

				ptr2->Number = LittleLong(ptr1->Number) % 1000;
				ptr2->Type = LittleLong(ptr1->Number) / 1000;
				ptr2->ArgCount = LittleLong(ptr1->ArgCount);
				ptr2->Address = LittleLong(ptr1->Address);
			}
		}
		break;

	case ACS_Enhanced:
	case ACS_LittleEnhanced:
		scripts.b = FindChunk (MAKE_ID('S','P','T','R'));
		if (scripts.b == NULL)
		{
			// There are no scripts!
		}
		else if (*(DWORD *)Data != MAKE_ID('A','C','S',0))
		{
			NumScripts = scripts.dw[1] / 12;
			Scripts = new ScriptPtr[NumScripts];
			scripts.dw += 2;

			for (i = 0; i < NumScripts; ++i)
			{
				ScriptPtr1 *ptr1 = &scripts.pi[i];
				ScriptPtr  *ptr2 = &Scripts[i];

				ptr2->Number = LittleShort(ptr1->Number);
				ptr2->Type = BYTE(LittleShort(ptr1->Type));
				ptr2->ArgCount = LittleLong(ptr1->ArgCount);
				ptr2->Address = LittleLong(ptr1->Address);
			}
		}
		else
		{
			NumScripts = scripts.dw[1] / 8;
			Scripts = new ScriptPtr[NumScripts];
			scripts.dw += 2;

			for (i = 0; i < NumScripts; ++i)
			{
				ScriptPtr3 *ptr1 = &scripts.pe[i];
				ScriptPtr  *ptr2 = &Scripts[i];

				ptr2->Number = LittleShort(ptr1->Number);
				ptr2->Type = ptr1->Type;
				ptr2->ArgCount = ptr1->ArgCount;
				ptr2->Address = LittleLong(ptr1->Address);
			}
		}
		break;

	default:
		break;
	}
	for (i = 0; i < NumScripts; ++i)
	{
		Scripts[i].Flags = 0;
		Scripts[i].VarCount = LOCAL_SIZE;
	}

	// Sort scripts, so we can use a binary search to find them
	if (NumScripts > 1)
	{
		qsort (Scripts, NumScripts, sizeof(ScriptPtr), SortScripts);
		// Check for duplicates because ACC originally did not enforce
		// script number uniqueness across different script types. We
		// only need to do this for old format lumps, because the ACCs
		// that produce new format lumps won't let you do this.
		if (Format == ACS_Old)
		{
			for (i = 0; i < NumScripts - 1; ++i)
			{
				if (Scripts[i].Number == Scripts[i+1].Number)
				{
					Printf("Script %d appears more than once.\n", Scripts[i].Number);
					// Make the closed version the first one.
					if (Scripts[i+1].Type == SCRIPT_Closed)
					{
						swapvalues(Scripts[i], Scripts[i+1]);
					}
				}
			}
		}
	}

	if (Format == ACS_Old)
		return;

	// Load script flags
	scripts.b = FindChunk (MAKE_ID('S','F','L','G'));
	if (scripts.dw != NULL)
	{
		max = scripts.dw[1] / 4;
		scripts.dw += 2;
		for (i = max; i > 0; --i, scripts.w += 2)
		{
			ScriptPtr *ptr = const_cast<ScriptPtr *>(FindScript (LittleShort(scripts.w[0])));
			if (ptr != NULL)
			{
				ptr->Flags = LittleShort(scripts.w[1]);
			}
		}
	}

	// Load script var counts. (Only recorded for scripts that use more than LOCAL_SIZE variables.)
	scripts.b = FindChunk (MAKE_ID('S','V','C','T'));
	if (scripts.dw != NULL)
	{
		max = scripts.dw[1] / 4;
		scripts.dw += 2;
		for (i = max; i > 0; --i, scripts.w += 2)
		{
			ScriptPtr *ptr = const_cast<ScriptPtr *>(FindScript (LittleShort(scripts.w[0])));
			if (ptr != NULL)
			{
				ptr->VarCount = LittleShort(scripts.w[1]);
			}
		}
	}
}

int STACK_ARGS FBehavior::SortScripts (const void *a, const void *b)
{
	ScriptPtr *ptr1 = (ScriptPtr *)a;
	ScriptPtr *ptr2 = (ScriptPtr *)b;
	return ptr1->Number - ptr2->Number;
}

void FBehavior::UnencryptStrings ()
{
	DWORD *prevchunk = NULL;
	DWORD *chunk = (DWORD *)FindChunk(MAKE_ID('S','T','R','E'));
	while (chunk != NULL)
	{
		for (DWORD strnum = 0; strnum < chunk[3]; ++strnum)
		{
			int ofs = chunk[5+strnum];
			BYTE *data = (BYTE *)chunk + ofs + 8, last;
			int p = (BYTE)(ofs*157135);
			int i = 0;
			do
			{
				last = (data[i] ^= (BYTE)(p+(i>>1)));
				++i;
			} while (last != 0);
		}
		prevchunk = chunk;
		chunk = (DWORD *)NextChunk ((BYTE *)chunk);
		*prevchunk = MAKE_ID('S','T','R','L');
	}
	if (prevchunk != NULL)
	{
		*prevchunk = MAKE_ID('S','T','R','L');
	}
}

bool FBehavior::IsGood ()
{
	bool bad;
	int i;

	// Check that the data format was understood
	if (Format == ACS_Unknown)
	{
		return false;
	}

	// Check that all functions are resolved
	bad = false;
	for (i = 0; i < NumFunctions; ++i)
	{
		ScriptFunction *funcdef = (ScriptFunction *)Functions + i;
		if (funcdef->Address == 0 && funcdef->ImportNum == 0)
		{
			DWORD *chunk = (DWORD *)FindChunk (MAKE_ID('F','N','A','M'));
			Printf ("Could not find ACS function %s for use in %s.\n",
				(char *)(chunk + 2) + chunk[3+i], ModuleName);
			bad = true;
		}
	}

	// Check that all imported modules were loaded
	for (i = Imports.Size() - 1; i >= 0; --i)
	{
		if (Imports[i] == NULL)
		{
			Printf ("Not all the libraries used by %s could be found.\n", ModuleName);
			return false;
		}
	}

	return !bad;
}

const ScriptPtr *FBehavior::FindScript (int script) const
{
	const ScriptPtr *ptr = BinarySearch<ScriptPtr, WORD>
		((ScriptPtr *)Scripts, NumScripts, &ScriptPtr::Number, (WORD)script);

	// If the preceding script has the same number, return it instead.
	// See the note by the script sorting above for why.
	if (ptr > Scripts)
	{
		if (ptr[-1].Number == script)
		{
			ptr--;
		}
	}
	return ptr;
}

const ScriptPtr *FBehavior::StaticFindScript (int script, FBehavior *&module)
{
	for (DWORD i = 0; i < StaticModules.Size(); ++i)
	{
		const ScriptPtr *code = StaticModules[i]->FindScript (script);
		if (code != NULL)
		{
			module = StaticModules[i];
			return code;
		}
	}
	return NULL;
}

ScriptFunction *FBehavior::GetFunction (int funcnum, FBehavior *&module) const
{
	if ((unsigned)funcnum >= (unsigned)NumFunctions)
	{
		return NULL;
	}
	ScriptFunction *funcdef = (ScriptFunction *)Functions + funcnum;
	if (funcdef->ImportNum)
	{
		return Imports[funcdef->ImportNum - 1]->GetFunction (funcdef->Address, module);
	}
	// Should I just un-const this function instead of using a const_cast?
	module = const_cast<FBehavior *>(this);
	return funcdef;
}

int FBehavior::FindFunctionName (const char *funcname) const
{
	return FindStringInChunk ((DWORD *)FindChunk (MAKE_ID('F','N','A','M')), funcname);
}

int FBehavior::FindMapVarName (const char *varname) const
{
	return FindStringInChunk ((DWORD *)FindChunk (MAKE_ID('M','E','X','P')), varname);
}

int FBehavior::FindMapArray (const char *arrayname) const
{
	int var = FindMapVarName (arrayname);
	if (var >= 0)
	{
		return MapVarStore[var];
	}
	return -1;
}

int FBehavior::FindStringInChunk (DWORD *names, const char *varname) const
{
	if (names != NULL)
	{
		DWORD i;

		for (i = 0; i < names[2]; ++i)
		{
			if (stricmp (varname, (char *)(names + 2) + names[3+i]) == 0)
			{
				return (int)i;
			}
		}
	}
	return -1;
}

int FBehavior::GetArrayVal (int arraynum, int index) const
{
	if ((unsigned)arraynum >= (unsigned)NumTotalArrays)
		return 0;
	const ArrayInfo *array = Arrays[arraynum];
	if ((unsigned)index >= (unsigned)array->ArraySize)
		return 0;
	return array->Elements[index];
}

void FBehavior::SetArrayVal (int arraynum, int index, int value)
{
	if ((unsigned)arraynum >= (unsigned)NumTotalArrays)
		return;
	const ArrayInfo *array = Arrays[arraynum];
	if ((unsigned)index >= (unsigned)array->ArraySize)
		return;
	array->Elements[index] = value;
}

BYTE *FBehavior::FindChunk (DWORD id) const
{
	BYTE *chunk = Chunks;

	while (chunk != NULL && chunk < Data + DataSize)
	{
		if (((DWORD *)chunk)[0] == id)
		{
			return chunk;
		}
		chunk += ((DWORD *)chunk)[1] + 8;
	}
	return NULL;
}

BYTE *FBehavior::NextChunk (BYTE *chunk) const
{
	DWORD id = *(DWORD *)chunk;
	chunk += ((DWORD *)chunk)[1] + 8;
	while (chunk != NULL && chunk < Data + DataSize)
	{
		if (((DWORD *)chunk)[0] == id)
		{
			return chunk;
		}
		chunk += ((DWORD *)chunk)[1] + 8;
	}
	return NULL;
}

const char *FBehavior::StaticLookupString (DWORD index)
{
	DWORD lib = index >> 16;
	
	switch (lib)
	{
	case LIB_ACSSTRINGS_ONTHEFLY:
		index &= 0xffff;
		return (ACS_StringsOnTheFly.Size() > index) ? ACS_StringsOnTheFly[index].GetChars() : NULL;
	}

	if (lib >= (DWORD)StaticModules.Size())
	{
		return NULL;
	}
	return StaticModules[lib]->LookupString (index & 0xffff);
}

const char *FBehavior::LookupString (DWORD index) const
{
	if (StringTable == 0)
	{
		return NULL;
	}
	if (Format == ACS_Old)
	{
		DWORD *list = (DWORD *)(Data + StringTable);

		if (index >= list[0])
			return NULL;	// Out of range for this list;
		return (const char *)(Data + list[1+index]);
	}
	else
	{
		DWORD *list = (DWORD *)(Data + StringTable);

		if (index >= list[1])
			return NULL;	// Out of range for this list
		return (const char *)(Data + StringTable + list[3+index]);
	}
}

void FBehavior::StaticStartTypedScripts (WORD type, AActor *activator, bool always, int arg1, bool runNow, bool onlyClientSideScripts)
{
	DPrintf("Starting all scripts of type %d\n", type);
	for (unsigned int i = 0; i < StaticModules.Size(); ++i)
	{
		StaticModules[i]->StartTypedScripts (type, activator, always, arg1, runNow, onlyClientSideScripts);
	}
}

void FBehavior::StartTypedScripts (WORD type, AActor *activator, bool always, int arg1, bool runNow, bool onlyClientSideScripts)
{
	const ScriptPtr *ptr;
	int i;

	for (i = 0; i < NumScripts; ++i)
	{
		ptr = &Scripts[i];
		if (ptr->Type == type)
		{
			// [BB] This is not a client side script, so skip it if onlyClientSideScripts is true.
			if ( onlyClientSideScripts && !ACS_IsScriptClientSide( ptr ) )
			{
				continue;
			}

			// [BC] If this script is client side, just let clients execute it themselves.
			if (( NETWORK_GetState( ) == NETSTATE_SERVER ) &&
				ACS_IsScriptClientSide( ptr ))
			{
				SERVERCOMMANDS_ACSScriptExecute( ptr->Number, activator, NULL, level.mapname, 0, arg1, 0, 0, always );
				continue;
			}

			DLevelScript *runningScript = P_GetScriptGoing (activator, NULL, ptr->Number,
				ptr, this, 0, arg1, 0, 0, always);
			if (runNow)
			{
				runningScript->RunScript ();
			}
		}
	}
}

// [BC] These next two functions count how many of a type of script are present on the map.
int FBehavior::StaticCountTypedScripts( WORD type )
{
	int	iCount;

	iCount = 0;
	for (unsigned int i = 0; i < StaticModules.Size(); ++i)
	{
		iCount += StaticModules[i]->CountTypedScripts (type);
	}

	return ( iCount );
}

int FBehavior::CountTypedScripts( WORD type )
{
	const ScriptPtr *ptr;
	int i;
	int	iCount;

	iCount = 0;
	for (i = 0; i < NumScripts; ++i)
	{
		ptr = &Scripts[i];
		if (ptr->Type == type)
			iCount++;
	}

	return ( iCount );
}
// [BC] End of new functions.

// FBehavior :: StaticStopMyScripts
//
// Stops any scripts started by the specified actor. Used by the net code
// when a player disconnects. Should this be used in general whenever an
// actor is destroyed?

void FBehavior::StaticStopMyScripts (AActor *actor)
{
	DACSThinker *controller = DACSThinker::ActiveThinker;

	if (controller != NULL)
	{
		controller->StopScriptsFor (actor);
	}
}

//---- The ACS Interpreter ----//

IMPLEMENT_POINTY_CLASS (DACSThinker)
 DECLARE_POINTER(LastScript)
 DECLARE_POINTER(Scripts)
END_POINTERS

TObjPtr<DACSThinker> DACSThinker::ActiveThinker;

DACSThinker::DACSThinker ()
: DThinker(STAT_SCRIPTS)
{
	if (ActiveThinker)
	{
		I_Error ("Only one ACSThinker is allowed to exist at a time.\nCheck your code.");
	}
	else
	{
		ActiveThinker = this;
		Scripts = NULL;
		LastScript = NULL;
		for (int i = 0; i < 1000; i++)
			RunningScripts[i] = NULL;
	}
}

DACSThinker::~DACSThinker ()
{
	Scripts = NULL;
	ActiveThinker = NULL;
}

void DACSThinker::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << Scripts << LastScript;
	if (arc.IsStoring ())
	{
		WORD i;
		for (i = 0; i < 1000; i++)
		{
			if (RunningScripts[i])
				arc << RunningScripts[i] << i;
		}
		DLevelScript *nilptr = NULL;
		arc << nilptr;
	}
	else
	{
		WORD scriptnum;
		DLevelScript *script = NULL;
		arc << script;
		while (script)
		{
			arc << scriptnum;
			RunningScripts[scriptnum] = script;
			arc << script;
		}
	}
}

void DACSThinker::Tick ()
{
	DLevelScript *script = Scripts;

	while (script)
	{
		DLevelScript *next = script->next;
		script->RunScript ();
		script = next;
	}

	ACS_StringsOnTheFly.Clear();

	if (ACS_StringBuilderStack.Size())
	{
		int size = ACS_StringBuilderStack.Size();
		ACS_StringBuilderStack.Clear();
		I_Error("Error: %d garbage entries on ACS string builder stack.", size);
	}
}

void DACSThinker::StopScriptsFor (AActor *actor)
{
	DLevelScript *script = Scripts;

	while (script != NULL)
	{
		DLevelScript *next = script->next;
		if (script->activator == actor)
		{
			script->SetState (DLevelScript::SCRIPT_PleaseRemove);
		}
		script = next;
	}
}

void DACSThinker::StopAndDestroyAllScripts ()
{
	// [BB] Unlink and destroy all running scripts.
	for (int i = 0; i < 1000; i++)
	{
		DLevelScript *script = RunningScripts[i];
		if ( script != NULL )
		{
			script->Unlink ();
			RunningScripts[i] = NULL;
			script->Destroy ();
		}
	}

	DLevelScript *script = Scripts;

	// [BB] Now remove all remaining scripts.
	while (script != NULL)
	{
		DLevelScript *next = script->next;
		script->Unlink ();
		script->Destroy ();
		script = next;
	}
}

IMPLEMENT_POINTY_CLASS (DLevelScript)
 DECLARE_POINTER(next)
 DECLARE_POINTER(prev)
 DECLARE_POINTER(activator)
END_POINTERS

void DLevelScript::Serialize (FArchive &arc)
{
	DWORD i;

	Super::Serialize (arc);
	arc << next << prev
		<< script;

	arc	<< state
		<< statedata
		<< activator
		<< activationline
		<< backSide
		<< numlocalvars;

	if (arc.IsLoading())
	{
		localvars = new SDWORD[numlocalvars];
	}
	for (i = 0; i < (DWORD)numlocalvars; i++)
	{
		arc << localvars[i];
	}

	if (arc.IsStoring ())
	{
		WORD lib = activeBehavior->GetLibraryID() >> 16;
		arc << lib;
		i = activeBehavior->PC2Ofs (pc);
		arc << i;
	}
	else
	{
		WORD lib;
		arc << lib << i;
		activeBehavior = FBehavior::StaticGetModule (lib);
		pc = activeBehavior->Ofs2PC (i);
	}

	// [BC] The server doesn't have an active font.
	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
		arc << activefont;

	arc << hudwidth << hudheight;
}

DLevelScript::DLevelScript ()
{
	next = prev = NULL;
	if (DACSThinker::ActiveThinker == NULL)
		new DACSThinker;
	activefont = SmallFont;
	localvars = NULL;
}

DLevelScript::~DLevelScript ()
{
	if (localvars != NULL)
		delete[] localvars;
	localvars = NULL;
}

void DLevelScript::Unlink ()
{
	DACSThinker *controller = DACSThinker::ActiveThinker;

	if (controller->LastScript == this)
	{
		controller->LastScript = prev;
		GC::WriteBarrier(controller, prev);
	}
	if (controller->Scripts == this)
	{
		controller->Scripts = next;
		GC::WriteBarrier(controller, next);
	}
	if (prev)
	{
		prev->next = next;
		GC::WriteBarrier(prev, next);
	}
	if (next)
	{
		next->prev = prev;
		GC::WriteBarrier(next, prev);
	}
}

void DLevelScript::Link ()
{
	DACSThinker *controller = DACSThinker::ActiveThinker;

	next = controller->Scripts;
	GC::WriteBarrier(this, next);
	if (controller->Scripts)
	{
		controller->Scripts->prev = this;
		GC::WriteBarrier(controller->Scripts, this);
	}
	prev = NULL;
	controller->Scripts = this;
	GC::WriteBarrier(controller, this);
	if (controller->LastScript == NULL)
	{
		controller->LastScript = this;
	}
}

void DLevelScript::PutLast ()
{
	DACSThinker *controller = DACSThinker::ActiveThinker;

	if (controller->LastScript == this)
		return;

	Unlink ();
	if (controller->Scripts == NULL)
	{
		Link ();
	}
	else
	{
		if (controller->LastScript)
			controller->LastScript->next = this;
		prev = controller->LastScript;
		next = NULL;
		controller->LastScript = this;
	}
}

void DLevelScript::PutFirst ()
{
	DACSThinker *controller = DACSThinker::ActiveThinker;

	if (controller->Scripts == this)
		return;

	Unlink ();
	Link ();
}

int DLevelScript::Random (int min, int max)
{
	if (max < min)
	{
		swapvalues (max, min);
	}

	return min + pr_acs(max - min + 1);
}

int DLevelScript::ThingCount (int type, int stringid, int tid, int tag)
{
	AActor *actor;
	const PClass *kind;
	int count = 0;
	bool replacemented = false;

	if (type >= MAX_SPAWNABLES)
	{
		return 0;
	}
	else if (type > 0)
	{
		kind = SpawnableThings[type];
		if (kind == NULL)
			return 0;
	}
	else if (stringid >= 0)
	{
		const char *type_name = FBehavior::StaticLookupString (stringid);
		if (type_name == NULL)
			return 0;

		kind = PClass::FindClass (type_name);
		if (kind == NULL || kind->ActorInfo == NULL)
			return 0;

	}
	else
	{
		kind = NULL;
	}

do_count:
	if (tid)
	{
		FActorIterator iterator (tid);
		while ( (actor = iterator.Next ()) )
		{
			if (actor->health > 0 &&
				(kind == NULL || actor->IsA (kind)))
			{
				if (actor->Sector->tag == tag || tag == -1)
				{
					// Don't count items in somebody's inventory
					if (!actor->IsKindOf (RUNTIME_CLASS(AInventory)) ||
						static_cast<AInventory *>(actor)->Owner == NULL)
					{
						count++;
					}
				}
			}
		}
	}
	else
	{
		TThinkerIterator<AActor> iterator;
		while ( (actor = iterator.Next ()) )
		{
			if (actor->health > 0 &&
				(kind == NULL || actor->IsA (kind)))
			{
				if (actor->Sector->tag == tag || tag == -1)
				{
					// Don't count items in somebody's inventory
					if (!actor->IsKindOf (RUNTIME_CLASS(AInventory)) ||
						static_cast<AInventory *>(actor)->Owner == NULL)
					{
						count++;
					}
				}
			}
		}
	}
	if (!replacemented && kind != NULL)
	{
		// Again, with decorate replacements
		replacemented = true;
		PClass *newkind = kind->ActorInfo->GetReplacement()->Class;
		if (newkind != kind)
		{
			kind = newkind;
			goto do_count;
		}
	}
	return count;
}

void DLevelScript::ChangeFlat (int tag, int name, bool floorOrCeiling)
{
	FTextureID flat;
	int secnum = -1;
	const char *flatname = FBehavior::StaticLookupString (name);

	if (flatname == NULL)
		return;

	flat = TexMan.GetTexture (flatname, FTexture::TEX_Flat, FTextureManager::TEXMAN_Overridable);

	while ((secnum = P_FindSectorFromTag (tag, secnum)) >= 0)
	{
		int pos = floorOrCeiling? sector_t::ceiling : sector_t::floor;
		sectors[secnum].SetTexture(pos, flat);

		// [BC] Update clients about this flat change.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetSectorFlat( secnum );

		// [BC] Also, mark this sector as having its flat changed.
		sectors[secnum].bFlatChange = true;
	}
}

int DLevelScript::CountPlayers ()
{
	int count = 0, i;

	// [BB] Skulltag doesn't count spectators as players.
	for (i = 0; i < MAXPLAYERS; i++)
		if (( playeringame[i] ) && ( players[i].bSpectating == false ))
			count++;

	return count;
}

void DLevelScript::SetLineTexture (int lineid, int side, int position, int name)
{
	FTextureID texture;
	int linenum = -1;
	const char *texname = FBehavior::StaticLookupString (name);

	if (texname == NULL)
		return;

	side = !!side;

	texture = TexMan.GetTexture (texname, FTexture::TEX_Wall, FTextureManager::TEXMAN_Overridable);

	while ((linenum = P_FindLineFromID (lineid, linenum)) >= 0)
	{
		side_t *sidedef;

		sidedef = lines[linenum].sidedef[side];
		if (sidedef == NULL)
			continue;

		// [BC] Line texture changed during the course of the level.
		{
			ULONG	ulShift;

			ulShift = 0;
			ulShift += position;
			if ( side )
				ulShift += 3;

			lines[linenum].ulTexChangeFlags |= 1 << ulShift;
/*
			if (( 1 << ulShift ) == TEXCHANGE_FRONTTOP )
				Printf( "FRONT TOP: %d\n", linenum );
			if (( 1 << ulShift ) ==  TEXCHANGE_FRONTMEDIUM )
				Printf( "FRONT MED: %d\n", linenum );
			if (( 1 << ulShift ) == TEXCHANGE_FRONTBOTTOM )
				Printf( "FRONT BOT: %d\n", linenum );
			if (( 1 << ulShift ) == TEXCHANGE_BACKTOP )
				Printf( "BACK TOP: %d\n", linenum );
			if (( 1 << ulShift ) == TEXCHANGE_BACKMEDIUM )
				Printf( "BACK MED: %d\n", linenum );
			if (( 1 << ulShift ) == TEXCHANGE_BACKBOTTOM )
				Printf( "BACK BOT: %d\n", linenum );
*/
		}

		switch (position)
		{
		case TEXTURE_TOP:
			sidedef->SetTexture(side_t::top, texture);
			break;
		case TEXTURE_MIDDLE:
			sidedef->SetTexture(side_t::mid, texture);
			break;
		case TEXTURE_BOTTOM:
			sidedef->SetTexture(side_t::bottom, texture);
			break;
		default:
			break;
		}
	}

	// [BB] If we're the server, tell clients about this texture change.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SetLineTextureByID( lineid, side, position, texname );
}

void DLevelScript::ReplaceTextures (int fromnamei, int tonamei, int flags)
{
	const char *fromname = FBehavior::StaticLookupString (fromnamei);
	const char *toname = FBehavior::StaticLookupString (tonamei);
	FTextureID picnum1, picnum2;

	if (fromname == NULL)
		return;

	// [BB] If we're the server, tell the clients to call DLevelScript::ReplaceTextures with the same
	// arguments. This way the amount of nettraffic needed is fixed and doesn't depend of the number
	// of lines or sectors that use the replaced texture.
	if (( NETWORK_GetState( ) == NETSTATE_SERVER ))
		SERVERCOMMANDS_ReplaceTextures ( fromnamei, tonamei, flags );

	if ((flags ^ (NOT_BOTTOM | NOT_MIDDLE | NOT_TOP)) != 0)
	{
		picnum1 = TexMan.GetTexture (fromname, FTexture::TEX_Wall, FTextureManager::TEXMAN_Overridable);
		picnum2 = TexMan.GetTexture (toname, FTexture::TEX_Wall, FTextureManager::TEXMAN_Overridable);

		for (int i = 0; i < numsides; ++i)
		{
			side_t *wal = &sides[i];

			for(int j=0;j<3;j++)
			{
				static BYTE bits[]={NOT_TOP, NOT_MIDDLE, NOT_BOTTOM};
				if (!(flags & bits[j]) && wal->GetTexture(j) == picnum1)
				{
					wal->SetTexture(j, picnum2);

					// [BB] We have to mark the texture as changed to restore it when the map resets.
					ULONG ulShift = 0;
					ulShift += j;
					if ( wal->linedef->sidedef[1] == wal )
						ulShift += 3;
					wal->linedef->ulTexChangeFlags |= 1 << ulShift;
				}
			}
		}
	}
	if ((flags ^ (NOT_FLOOR | NOT_CEILING)) != 0)
	{
		picnum1 = TexMan.GetTexture (fromname, FTexture::TEX_Flat, FTextureManager::TEXMAN_Overridable);
		picnum2 = TexMan.GetTexture (toname, FTexture::TEX_Flat, FTextureManager::TEXMAN_Overridable);

		for (int i = 0; i < numsectors; ++i)
		{
			sector_t *sec = &sectors[i];

			if (!(flags & NOT_FLOOR) && sec->GetTexture(sector_t::floor) == picnum1) 
			{
				sec->SetTexture(sector_t::floor, picnum2);

				// [BB] Mark this sector as having its flat changed.
				sec->bFlatChange = true;
			}
			if (!(flags & NOT_CEILING) && sec->GetTexture(sector_t::ceiling) == picnum1)	
			{
				sec->SetTexture(sector_t::ceiling, picnum2);

				// [BB] Mark this sector as having its flat changed.
				sec->bFlatChange = true;
			}
		}
	}
}

int DLevelScript::DoSpawn (int type, fixed_t x, fixed_t y, fixed_t z, int tid, int angle, bool force)
{
	const PClass *info = PClass::FindClass (FBehavior::StaticLookupString (type));
	AActor *actor = NULL;
	int spawncount = 0;

	if (info != NULL)
	{
		actor = Spawn (info, x, y, z, ALLOW_REPLACE);
		if (actor != NULL)
		{
			DWORD oldFlags2 = actor->flags2;
			actor->flags2 |= MF2_PASSMOBJ;
			if (force || P_TestMobjLocation (actor))
			{
				actor->angle = angle << 24;
				actor->tid = tid;
				actor->AddToHash ();
				if (actor->flags & MF_SPECIAL)
					actor->flags |= MF_DROPPED;  // Don't respawn
				actor->flags2 = oldFlags2;
				spawncount++;

				// [BC] If we're the server, tell clients to spawn the thing.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				{
					SERVERCOMMANDS_SpawnThing( actor );

					if ( actor->angle != 0 )
						SERVERCOMMANDS_SetThingAngle( actor );
				}
			}
			else
			{
				// If this is a monster, subtract it from the total monster
				// count, because it already added to it during spawning.
				if (actor->CountsAsKill())
				{
					level.total_monsters--;

					// [BB] The monster didn't spawn at all, so we need to correct the number of monsters in invasion mode.
					INVASION_UpdateMonsterCount( actor, true );
				}
				// Same, for items
				if (actor->flags & MF_COUNTITEM)
				{
					level.total_items--;
				}
				actor->Destroy ();
				actor = NULL;
			}
		}
	}
	return spawncount;
}

int DLevelScript::DoSpawnSpot (int type, int spot, int tid, int angle, bool force)
{
	int spawned = 0;

	if (spot != 0)
	{
		FActorIterator iterator (spot);
		AActor *aspot;

		while ( (aspot = iterator.Next ()) )
		{
			spawned += DoSpawn (type, aspot->x, aspot->y, aspot->z, tid, angle, force);
		}
	}
	else if (activator != NULL)
	{
			spawned += DoSpawn (type, activator->x, activator->y, activator->z, tid, angle, force);
	}
	return spawned;
}

int DLevelScript::DoSpawnSpotFacing (int type, int spot, int tid, bool force)
{
	int spawned = 0;

	if (spot != 0)
	{
		FActorIterator iterator (spot);
		AActor *aspot;

		while ( (aspot = iterator.Next ()) )
		{
			spawned += DoSpawn (type, aspot->x, aspot->y, aspot->z, tid, aspot->angle >> 24, force);
		}
	}
	else if (activator != NULL)
	{
			spawned += DoSpawn (type, activator->x, activator->y, activator->z, tid, activator->angle >> 24, force);
	}
	return spawned;
}

void DLevelScript::DoFadeTo (int r, int g, int b, int a, fixed_t time)
{
	DoFadeRange (0, 0, 0, -1, r, g, b, a, time);
}

void DLevelScript::DoFadeRange (int r1, int g1, int b1, int a1,
								int r2, int g2, int b2, int a2, fixed_t time)
{
	player_t *viewer;
	float ftime = (float)time / 65536.f;
	bool fadingFrom = a1 >= 0;
	float fr1 = 0, fg1 = 0, fb1 = 0, fa1 = 0;
	float fr2, fg2, fb2, fa2;
	int i;

	fr2 = (float)r2 / 255.f;
	fg2 = (float)g2 / 255.f;
	fb2 = (float)b2 / 255.f;
	fa2 = (float)a2 / 65536.f;

	if (fadingFrom)
	{
		fr1 = (float)r1 / 255.f;
		fg1 = (float)g1 / 255.f;
		fb1 = (float)b1 / 255.f;
		fa1 = (float)a1 / 65536.f;
	}

	if (activator != NULL)
	{
		viewer = activator->player;
		if (viewer == NULL)
			return;
		i = MAXPLAYERS;
		goto showme;
	}
	else
	{
		for (i = 0; i < MAXPLAYERS; ++i)
		{
			if (playeringame[i])
			{
				viewer = &players[i];
showme:
				if (ftime <= 0.f)
				{
					viewer->BlendR = fr2;
					viewer->BlendG = fg2;
					viewer->BlendB = fb2;
					viewer->BlendA = fa2;

					// [BB] Inform the clients about the blend.
					if ( NETWORK_GetState( ) == NETSTATE_SERVER )
						SERVERCOMMANDS_SetPlayerBlend ( ULONG ( viewer - players ) );
				}
				else
				{
					if (!fadingFrom)
					{
						if (viewer->BlendA <= 0.f)
						{
							fr1 = fr2;
							fg1 = fg2;
							fb1 = fb2;
							fa1 = 0.f;
						}
						else
						{
							fr1 = viewer->BlendR;
							fg1 = viewer->BlendG;
							fb1 = viewer->BlendB;
							fa1 = viewer->BlendA;
						}
					}

					new DFlashFader (fr1, fg1, fb1, fa1, fr2, fg2, fb2, fa2, ftime, viewer->mo);
				}
			}
		}
	}
}

void DLevelScript::DoSetFont (int fontnum)
{
	const char *fontname = FBehavior::StaticLookupString (fontnum);
	activefont = V_GetFont (fontname);

	// [BC] Since the server doesn't have a screen, we have to save the active font some
	// other way.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		if ( activefont == NULL )
			SERVER_SetScriptActiveFont( "SmallFont" );
		else
			SERVER_SetScriptActiveFont( fontname );
		SERVER_SetCurrentFont( SERVER_GetScriptActiveFont() );
		return;
	}

	if (activefont == NULL)
	{
		activefont = SmallFont;
	}
}

/* [BB] Moved to p_acs.h. Skulltag also needs this enum outside p_acs.cpp.
enum
{
	APROP_Health		= 0,
	APROP_Speed			= 1,
	APROP_Damage		= 2,
	APROP_Alpha			= 3,
	APROP_RenderStyle	= 4,
	APROP_SeeSound		= 5,	// Sounds can only be set, not gotten
	APROP_AttackSound	= 6,
	APROP_PainSound		= 7,
	APROP_DeathSound	= 8,
	APROP_ActiveSound	= 9,
	APROP_Ambush		= 10,
	APROP_Invulnerable	= 11,
	APROP_JumpZ			= 12,	// [GRB]
	APROP_ChaseGoal		= 13,
	APROP_Frightened	= 14,
	APROP_Gravity		= 15,
	APROP_Friendly		= 16,
	APROP_SpawnHealth   = 17,
	APROP_Dropped		= 18,
	APROP_Notarget		= 19,
	APROP_Species		= 20,
	APROP_NameTag		= 21,
	APROP_Score			= 22,
	APROP_Notrigger		= 23,
	APROP_DamageFactor	= 24,
};	
*/

// These are needed for ACS's APROP_RenderStyle
static const int LegacyRenderStyleIndices[] =
{
	0,	// STYLE_None,
	1,  // STYLE_Normal,
	2,  // STYLE_Fuzzy,
	3,	// STYLE_SoulTrans,
	4,	// STYLE_OptFuzzy,
	5,	// STYLE_Stencil,
	64,	// STYLE_Translucent
	65,	// STYLE_Add,
	66,	// STYLE_Shaded,
	67,	// STYLE_TranslucentStencil,
	-1
};

void DLevelScript::SetActorProperty (int tid, int property, int value)
{
	if (tid == 0)
	{
		DoSetActorProperty (activator, property, value);
	}
	else
	{
		AActor *actor;
		FActorIterator iterator (tid);

		while ((actor = iterator.Next()) != NULL)
		{
			DoSetActorProperty (actor, property, value);
		}
	}
}

void DLevelScript::DoSetActorProperty (AActor *actor, int property, int value)
{
	if (actor == NULL)
	{
		return;
	}

	// [WS/BB] Do not do this for spectators.
	if ( actor->player && actor->player->bSpectating )
		return;

	switch (property)
	{
	case APROP_Health:
		actor->health = value;
		if (actor->player != NULL)
		{
			actor->player->health = value;

			// [BC/BB] If we're the server, tell all clients about the new health value.
			if (( NETWORK_GetState( ) == NETSTATE_SERVER ) &&
				( SERVER_IsValidClient( actor->player - players )))
			{
				SERVERCOMMANDS_SetPlayerHealth( static_cast<ULONG>( actor->player - players ) );
			}
		}
		break;

	case APROP_Speed:
		actor->Speed = value;

		// [BC] If we're the server, tell clients to update this actor property.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetThingProperty( actor, APROP_Speed );
		break;

	case APROP_Damage:
		actor->Damage = value;
		break;

	case APROP_Alpha:
		actor->alpha = value;

		// [BC] If we're the server, tell clients to update this actor property.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetThingProperty( actor, APROP_Alpha );
		break;

	case APROP_RenderStyle:
		for(int i=0; LegacyRenderStyleIndices[i] >= 0; i++)
		{
			if (LegacyRenderStyleIndices[i] == value)
			{
				actor->RenderStyle = ERenderStyle(i);
				break;
			}
		}

		// [BC] If we're the server, tell clients to update this actor property.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetThingProperty( actor, APROP_RenderStyle );

		break;

	case APROP_Ambush:
		if (value) actor->flags |= MF_AMBUSH; else actor->flags &= ~MF_AMBUSH;
		break;

	case APROP_Dropped:
		if (value) actor->flags |= MF_DROPPED; else actor->flags &= ~MF_DROPPED;
		break;

	case APROP_Invulnerable:
		if (value) actor->flags2 |= MF2_INVULNERABLE; else actor->flags2 &= ~MF2_INVULNERABLE;

		// [BC] If we're the server, tell clients to update this actor property.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetThingFlags( actor, FLAGSET_FLAGS2 );
		break;

	case APROP_Notarget:
		if (value) actor->flags3 |= MF3_NOTARGET; else actor->flags3 &= ~MF3_NOTARGET;
		break;

	case APROP_Notrigger:
		if (value) actor->flags6 |= MF6_NOTRIGGER; else actor->flags6 &= ~MF6_NOTRIGGER;
		break;

	case APROP_JumpZ:
		if (actor->IsKindOf (RUNTIME_CLASS (APlayerPawn)))
			static_cast<APlayerPawn *>(actor)->JumpZ = value;

		// [BC] If we're the server, tell clients to update this actor property.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetThingProperty( actor, APROP_JumpZ );
		break; 	// [GRB]

	case APROP_ChaseGoal:
		if (value)
			actor->flags5 |= MF5_CHASEGOAL;
		else
			actor->flags5 &= ~MF5_CHASEGOAL;
		break;

	case APROP_Frightened:
		if (value)
			actor->flags4 |= MF4_FRIGHTENED;
		else
			actor->flags4 &= ~MF4_FRIGHTENED;
		break;

	case APROP_Friendly:
		if (value)
			actor->flags |= MF_FRIENDLY;
		else
			actor->flags &= ~MF_FRIENDLY;
		break;


	case APROP_SpawnHealth:
		if (actor->IsKindOf (RUNTIME_CLASS (APlayerPawn)))
		{
			static_cast<APlayerPawn *>(actor)->MaxHealth = value;

			// [BB] If we're the server, tell clients to update this actor property.
			// Note: Don't do this if the actor is a voodoo doll, the client would
			// alter the value of the real player body in this case.
			if ( ( NETWORK_GetState( ) == NETSTATE_SERVER ) && actor->player && ( actor->player->mo == actor ) )
				SERVERCOMMANDS_SetPlayerMaxHealth( static_cast<ULONG>( actor->player - players ) );
		}
		break;

	case APROP_Gravity:
		actor->gravity = value;

		// [BB] If we're the server, tell clients to update this actor's gravity.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetThingGravity( actor );
		break;

	case APROP_SeeSound:
		actor->SeeSound = FBehavior::StaticLookupString(value);

		// [BC] If we're the server, tell clients to update this actor property.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetThingSound( actor, ACTORSOUND_SEESOUND, FBehavior::StaticLookupString( value ));
		break;

	case APROP_AttackSound:
		actor->AttackSound = FBehavior::StaticLookupString(value);

		// [BC] If we're the server, tell clients to update this actor property.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetThingSound( actor, ACTORSOUND_ATTACKSOUND, FBehavior::StaticLookupString( value ));
		break;

	case APROP_PainSound:
		actor->PainSound = FBehavior::StaticLookupString(value);

		// [BC] If we're the server, tell clients to update this actor property.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetThingSound( actor, ACTORSOUND_PAINSOUND, FBehavior::StaticLookupString( value ));
		break;

	case APROP_DeathSound:
		actor->DeathSound = FBehavior::StaticLookupString(value);

		// [BC] If we're the server, tell clients to update this actor property.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetThingSound( actor, ACTORSOUND_DEATHSOUND, FBehavior::StaticLookupString( value ));
		break;

	case APROP_ActiveSound:
		actor->ActiveSound = FBehavior::StaticLookupString(value);

		// [BC] If we're the server, tell clients to update this actor property.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetThingSound( actor, ACTORSOUND_ACTIVESOUND, FBehavior::StaticLookupString( value ));
		break;

	case APROP_Species:
		actor->Species = FBehavior::StaticLookupString(value);
		break;

	case APROP_Score:
		actor->Score = value;

	case APROP_NameTag:
		actor->Tag = FBehavior::StaticLookupString(value);
		break;

	case APROP_DamageFactor:
		actor->DamageFactor = value;
		break;

	default:
		// do nothing.
		break;
	}
}

static AActor *SingleActorFromTID (int tid, AActor *defactor)
{
	if (tid == 0)
	{
		return defactor;
	}
	else
	{
		FActorIterator iterator (tid);
		return iterator.Next();
	}
}

int DLevelScript::GetActorProperty (int tid, int property)
{
	AActor *actor = SingleActorFromTID (tid, activator);

	if (actor == NULL)
	{
		return 0;
	}
	switch (property)
	{
	case APROP_Health:		return actor->health;
	case APROP_Speed:		return actor->Speed;
	case APROP_Damage:		return actor->Damage;	// Should this call GetMissileDamage() instead?
	case APROP_DamageFactor:return actor->DamageFactor;
	case APROP_Alpha:		return actor->alpha;
	case APROP_RenderStyle:	for (int style = STYLE_None; style < STYLE_Count; ++style)
							{ // Check for a legacy render style that matches.
								if (LegacyRenderStyles[style] == actor->RenderStyle)
								{
									return LegacyRenderStyleIndices[style];
								}
							}
							// The current render style isn't expressable as a legacy style,
							// so pretends it's normal.
							return STYLE_Normal;
	case APROP_Gravity:		return actor->gravity;
	case APROP_Ambush:		return !!(actor->flags & MF_AMBUSH);
	case APROP_Dropped:		return !!(actor->flags & MF_DROPPED);
	case APROP_ChaseGoal:	return !!(actor->flags5 & MF5_CHASEGOAL);
	case APROP_Frightened:	return !!(actor->flags4 & MF4_FRIGHTENED);
	case APROP_Friendly:	return !!(actor->flags & MF_FRIENDLY);
	case APROP_Notarget:	return !!(actor->flags3 & MF3_NOTARGET);
	case APROP_Notrigger:	return !!(actor->flags6 & MF6_NOTRIGGER);
	case APROP_SpawnHealth: if (actor->IsKindOf (RUNTIME_CLASS (APlayerPawn)))
							{
								return static_cast<APlayerPawn *>(actor)->MaxHealth;
							}
							else
							{
								return actor->SpawnHealth();
							}

	case APROP_JumpZ:		if (actor->IsKindOf (RUNTIME_CLASS (APlayerPawn)))
							{
								return static_cast<APlayerPawn *>(actor)->JumpZ;	// [GRB]
							}
							else
							{
								return 0;
							}
	case APROP_Score:		return actor->Score;
	default:				return 0;
	}
}



int DLevelScript::CheckActorProperty (int tid, int property, int value)
{
	AActor *actor = SingleActorFromTID (tid, activator);
	const char *string = NULL;
	if (actor == NULL)
	{
		return 0;
	}
	switch (property)
	{
		// Default
		default:				return 0;

		// Straightforward integer values:
		case APROP_Health:
		case APROP_Speed:
		case APROP_Damage:
		case APROP_DamageFactor:
		case APROP_Alpha:
		case APROP_RenderStyle:
		case APROP_Gravity:
		case APROP_SpawnHealth:
		case APROP_JumpZ:
		case APROP_Score:
			return (GetActorProperty(tid, property) == value);

		// Boolean values need to compare to a binary version of value
		case APROP_Ambush:
		case APROP_Dropped:
		case APROP_ChaseGoal:
		case APROP_Frightened:
		case APROP_Friendly:
		case APROP_Notarget:
		case APROP_Notrigger:
			return (GetActorProperty(tid, property) == (!!value));

		// Strings are not covered by GetActorProperty, so make the check here
		case APROP_SeeSound:	string = actor->SeeSound; break;
		case APROP_AttackSound:	string = actor->AttackSound; break;
		case APROP_PainSound:	string = actor->PainSound; break;
		case APROP_DeathSound:	string = actor->DeathSound; break;
		case APROP_ActiveSound:	string = actor->ActiveSound; break; 
		case APROP_Species:		string = actor->GetSpecies(); break;
		case APROP_NameTag:		string = actor->GetTag(); break;
	}
	if (string == NULL) string = "";
	return (!stricmp(string, FBehavior::StaticLookupString(value)));
}

bool DLevelScript::DoCheckActorTexture(int tid, AActor *activator, int string, bool floor)
{
	AActor *actor = SingleActorFromTID(tid, activator);
	if (actor == NULL)
	{
		return 0;
	}
	FTexture *tex = TexMan.FindTexture(FBehavior::StaticLookupString(string));
	if (tex == NULL)
	{ // If the texture we want to check against doesn't exist, then
	  // they're obviously not the same.
		return 0;
	}
	int i, numff;
	FTextureID secpic;
	sector_t *sec = actor->Sector;
	numff = sec->e->XFloor.ffloors.Size();

	if (floor)
	{
		// Looking through planes from top to bottom
		for (i = 0; i < numff; ++i)
		{
			F3DFloor *ff = sec->e->XFloor.ffloors[i];
			
			if (actor->z >= ff->top.plane->ZatPoint(actor->x, actor->y))
			{ // This floor is beneath our feet.
				secpic = *ff->top.texture;
				break;
			}
		}
		if (i == numff)
		{ // Use sector's floor
			secpic = sec->GetTexture(sector_t::floor);
		}
	}
	else
	{
		fixed_t z = actor->z + actor->height;
		// Looking through planes from bottom to top
		for (i = numff-1; i >= 0; --i)
		{
			F3DFloor *ff = sec->e->XFloor.ffloors[i];

			if (z <= ff->bottom.plane->ZatPoint(actor->x, actor->y))
			{ // This floor is above our eyes.
				secpic = *ff->bottom.texture;
				break;
			}
		}
		if (i < 0)
		{ // Use sector's ceiling
			secpic = sec->GetTexture(sector_t::ceiling);
		}
	}
	return tex == TexMan[secpic];
}

enum
{
	// These are the original inputs sent by the player.
	INPUT_OLDBUTTONS,
	INPUT_BUTTONS,
	INPUT_PITCH,
	INPUT_YAW,
	INPUT_ROLL,
	INPUT_FORWARDMOVE,
	INPUT_SIDEMOVE,
	INPUT_UPMOVE,

	// These are the inputs, as modified by P_PlayerThink().
	// Most of the time, these will match the original inputs, but
	// they can be different if a player is frozen or using a
	// chainsaw.
	MODINPUT_OLDBUTTONS,
	MODINPUT_BUTTONS,
	MODINPUT_PITCH,
	MODINPUT_YAW,
	MODINPUT_ROLL,
	MODINPUT_FORWARDMOVE,
	MODINPUT_SIDEMOVE,
	MODINPUT_UPMOVE
};

int DLevelScript::GetPlayerInput(int playernum, int inputnum)
{
	player_t *p;

	if (playernum < 0)
	{
		// [BB] In world activated CLIENTSIDE scripts, return the input of the console player.
		if ( NETWORK_InClientMode( ) && ( activator == NULL ) )
		{
			// [BB] For spectators the original input is not saved (since it should be the same
			// as modinput), thus just return the corresponding modinput in this case.
			if ( players[consoleplayer].bSpectating && ( inputnum < MODINPUT_OLDBUTTONS ) )
				inputnum += MODINPUT_OLDBUTTONS;

			p = &players[consoleplayer];
		}
		else
		{
			if (activator == NULL)
			{
				return 0;
			}
			p = activator->player;
		}
	}
	else if (playernum >= MAXPLAYERS || !playeringame[playernum])
	{
		return 0;
	}
	else
	{
		p = &players[playernum];
	}
	if (p == NULL)
	{
		return 0;
	}

	switch (inputnum)
	{
	case INPUT_OLDBUTTONS:		return p->original_oldbuttons;		break;
	case INPUT_BUTTONS:			return p->original_cmd.buttons;		break;
	case INPUT_PITCH:			return p->original_cmd.pitch;		break;
	case INPUT_YAW:				return p->original_cmd.yaw;			break;
	case INPUT_ROLL:			return p->original_cmd.roll;		break;
	case INPUT_FORWARDMOVE:		return p->original_cmd.forwardmove;	break;
	case INPUT_SIDEMOVE:		return p->original_cmd.sidemove;	break;
	case INPUT_UPMOVE:			return p->original_cmd.upmove;		break;

	case MODINPUT_OLDBUTTONS:	return p->oldbuttons;				break;
	case MODINPUT_BUTTONS:		return p->cmd.ucmd.buttons;			break;
	case MODINPUT_PITCH:		return p->cmd.ucmd.pitch;			break;
	case MODINPUT_YAW:			return p->cmd.ucmd.yaw;				break;
	case MODINPUT_ROLL:			return p->cmd.ucmd.roll;			break;
	case MODINPUT_FORWARDMOVE:	return p->cmd.ucmd.forwardmove;		break;
	case MODINPUT_SIDEMOVE:		return p->cmd.ucmd.sidemove;		break;
	case MODINPUT_UPMOVE:		return p->cmd.ucmd.upmove;			break;

	default:					return 0;							break;
	}
}

enum
{
	ACTOR_NONE				= 0x00000000,
	ACTOR_WORLD				= 0x00000001,
	ACTOR_PLAYER			= 0x00000002,
	ACTOR_BOT				= 0x00000004,
	ACTOR_VOODOODOLL		= 0x00000008,
	ACTOR_MONSTER			= 0x00000010,
	ACTOR_ALIVE				= 0x00000020,
	ACTOR_DEAD				= 0x00000040,
	ACTOR_MISSILE			= 0x00000080,
	ACTOR_GENERIC			= 0x00000100
};

int DLevelScript::DoClassifyActor(int tid)
{
	AActor *actor;
	int classify;

	if (tid == 0)
	{
		actor = activator;
		if (actor == NULL)
		{
			return ACTOR_WORLD;
		}
	}
	else
	{
		FActorIterator it(tid);
		actor = it.Next();
	}
	if (actor == NULL)
	{
		return ACTOR_NONE;
	}

	classify = 0;
	if (actor->player != NULL)
	{
		classify |= ACTOR_PLAYER;
		if (actor->player->playerstate == PST_DEAD)
		{
			classify |= ACTOR_DEAD;
		}
		else
		{
			classify |= ACTOR_ALIVE;
		}
		if (actor->player->mo != actor)
		{
			classify |= ACTOR_VOODOODOLL;
		}
		// [BB] ST's bots are different.
		if (actor->player->bIsBot)
		{
			classify |= ACTOR_BOT;
		}
	}
	else if (actor->flags3 & MF3_ISMONSTER)
	{
		classify |= ACTOR_MONSTER;
		if (actor->health <= 0)
		{
			classify |= ACTOR_DEAD;
		}
		else
		{
			classify |= ACTOR_ALIVE;
		}
	}
	else if (actor->flags & MF_MISSILE)
	{
		classify |= ACTOR_MISSILE;
	}
	else
	{
		classify |= ACTOR_GENERIC;
	}
	return classify;
}

enum EACSFunctions
{
	ACSF_GetLineUDMFInt=1,
	ACSF_GetLineUDMFFixed,
	ACSF_GetThingUDMFInt,
	ACSF_GetThingUDMFFixed,
	ACSF_GetSectorUDMFInt,
	ACSF_GetSectorUDMFFixed,
	ACSF_GetSideUDMFInt,
	ACSF_GetSideUDMFFixed,
	ACSF_GetActorVelX,
	ACSF_GetActorVelY,
	ACSF_GetActorVelZ,
	ACSF_SetActivator,
	ACSF_SetActivatorToTarget,
	ACSF_GetActorViewHeight,
	ACSF_GetChar,
	ACSF_GetAirSupply,
	ACSF_SetAirSupply,
	ACSF_SetSkyScrollSpeed,
	ACSF_GetArmorType,
	ACSF_SpawnSpotForced,
	ACSF_SpawnSpotFacingForced,
	ACSF_CheckActorProperty,
    ACSF_SetActorVelocity,
	ACSF_SetUserVariable,
	ACSF_GetUserVariable,
	ACSF_Radius_Quake2,
	ACSF_CheckActorClass,
	ACSF_SetUserArray,
	ACSF_GetUserArray,
	ACSF_SoundSequenceOnActor,
	ACSF_SoundSequenceOnSector,
	ACSF_SoundSequenceOnPolyobj,
	ACSF_AnnouncerSound=37, // [BL] Skulltag Function

	// [BB] Skulltag functions
	ACSF_ResetMap = 100,
	ACSF_PlayerIsSpectator,
	ACSF_ConsolePlayerNumber,
	ACSF_GetTeamProperty, // [Dusk]
	ACSF_GetPlayerLivesLeft,
	ACSF_SetPlayerLivesLeft,
	ACSF_KickFromGame,
	// ZDaemon
	ACSF_GetTeamScore = 19620,
};

int DLevelScript::SideFromID(int id, int side)
{
	if (side != 0 && side != 1) return -1;
	
	if (id == 0)
	{
		if (activationline == NULL) return -1;
		if (activationline->sidedef[side] == NULL) return -1;
		return activationline->sidedef[side]->Index;
	}
	else
	{
		int line = P_FindLineFromID(id, -1);
		if (line == -1) return -1;
		if (lines[line].sidedef[side] == NULL) return -1;
		return lines[line].sidedef[side]->Index;
	}
}

int DLevelScript::LineFromID(int id)
{
	if (id == 0)
	{
		if (activationline == NULL) return -1;
		return int(activationline - lines);
	}
	else
	{
		return P_FindLineFromID(id, -1);
	}
}

static void SetUserVariable(AActor *self, FName varname, int index, int value)
{
	PSymbol *sym = self->GetClass()->Symbols.FindSymbol(varname, true);
	int max;
	PSymbolVariable *var;

	if (sym == NULL || sym->SymbolType != SYM_Variable ||
		!(var = static_cast<PSymbolVariable *>(sym))->bUserVar)
	{
		return;
	}
	if (var->ValueType.Type == VAL_Int)
	{
		max = 1;
	}
	else if (var->ValueType.Type == VAL_Array && var->ValueType.BaseType == VAL_Int)
	{
		max = var->ValueType.size;
	}
	else
	{
		return;
	}
	// Set the value of the specified user variable.
	if (index >= 0 && index < max)
	{
		((int *)(reinterpret_cast<BYTE *>(self) + var->offset))[index] = value;
	}
}

static int GetUserVariable(AActor *self, FName varname, int index)
{
	PSymbol *sym = self->GetClass()->Symbols.FindSymbol(varname, true);
	int max;
	PSymbolVariable *var;

	if (sym == NULL || sym->SymbolType != SYM_Variable ||
		!(var = static_cast<PSymbolVariable *>(sym))->bUserVar)
	{
		return 0;
	}
	if (var->ValueType.Type == VAL_Int)
	{
		max = 1;
	}
	else if (var->ValueType.Type == VAL_Array && var->ValueType.BaseType == VAL_Int)
	{
		max = var->ValueType.size;
	}
	else
	{
		return 0;
	}
	// Get the value of the specified user variable.
	if (index >= 0 && index < max)
	{
		return ((int *)(reinterpret_cast<BYTE *>(self) + var->offset))[index];
	}
	return 0;
}

int DLevelScript::CallFunction(int argCount, int funcIndex, SDWORD *args)
{
	AActor *actor;
	switch(funcIndex)
	{
		case ACSF_GetLineUDMFInt:
			return GetUDMFInt(UDMF_Line, LineFromID(args[0]), FBehavior::StaticLookupString(args[1]));

		case ACSF_GetLineUDMFFixed:
			return GetUDMFFixed(UDMF_Line, LineFromID(args[0]), FBehavior::StaticLookupString(args[1]));

		case ACSF_GetThingUDMFInt:
		case ACSF_GetThingUDMFFixed:
			return 0;	// Not implemented yet

		case ACSF_GetSectorUDMFInt:
			return GetUDMFInt(UDMF_Sector, P_FindSectorFromTag(args[0], -1), FBehavior::StaticLookupString(args[1]));

		case ACSF_GetSectorUDMFFixed:
			return GetUDMFFixed(UDMF_Sector, P_FindSectorFromTag(args[0], -1), FBehavior::StaticLookupString(args[1]));

		case ACSF_GetSideUDMFInt:
			return GetUDMFInt(UDMF_Side, SideFromID(args[0], args[1]), FBehavior::StaticLookupString(args[2]));

		case ACSF_GetSideUDMFFixed:
			return GetUDMFFixed(UDMF_Side, SideFromID(args[0], args[1]), FBehavior::StaticLookupString(args[2]));

		case ACSF_GetActorVelX:
			actor = SingleActorFromTID(args[0], activator);
			return actor != NULL? actor->velx : 0;

		case ACSF_GetActorVelY:
			actor = SingleActorFromTID(args[0], activator);
			return actor != NULL? actor->vely : 0;

		case ACSF_GetActorVelZ:
			actor = SingleActorFromTID(args[0], activator);
			return actor != NULL? actor->velz : 0;

		case ACSF_SetActivator:
			activator = SingleActorFromTID(args[0], NULL);
			return activator != NULL;
		
		case ACSF_SetActivatorToTarget:
			// [KS] I revised this a little bit
			actor = SingleActorFromTID(args[0], activator);
			if (actor != NULL)
			{
				if (actor->player != NULL && actor->player->playerstate == PST_LIVE)
				{
					P_BulletSlope(actor, &actor);
				}
				else
				{
					actor = actor->target;
				}
			}
			if (actor != NULL)
			{
				activator = actor;
				return 1;
			}
			return 0;

		case ACSF_GetActorViewHeight:
			actor = SingleActorFromTID(args[0], NULL);
			if (actor != NULL)
			{
				if (actor->player != NULL)
				{
					return actor->player->mo->ViewHeight + actor->player->crouchviewdelta;
				}
				else
				{
					return actor->GetClass()->Meta.GetMetaFixed(AMETA_CameraHeight, actor->height/2);
				}
			}
			else return 0;

		case ACSF_GetChar:
		{
			const char *p = FBehavior::StaticLookupString(args[0]);
			if (p != NULL && args[1] >= 0 && args[1] < int(strlen(p)))
			{
				return p[args[1]];
			}
			else 
			{
				return 0;
			}
		}

		case ACSF_GetAirSupply:
		{
			if (args[0] < 0 || args[0] >= MAXPLAYERS || !playeringame[args[0]])
			{
				return 0;
			}
			else
			{
				return players[args[0]].air_finished - level.time;
			}
		}

		case ACSF_SetAirSupply:
		{
			if (args[0] < 0 || args[0] >= MAXPLAYERS || !playeringame[args[0]])
			{
				return 0;
			}
			else
			{
				players[args[0]].air_finished = args[1] + level.time;
				return 1;
			}
		}

		case ACSF_SetSkyScrollSpeed:
		{
			if (args[0] == 1) level.skyspeed1 = FIXED2FLOAT(args[1]);
			else if (args[0] == 2) level.skyspeed2 = FIXED2FLOAT(args[1]);
			return 1;
		}

		case ACSF_GetArmorType:
		{
			if (args[1] < 0 || args[1] >= MAXPLAYERS || !playeringame[args[1]])
			{
				return 0;
			}
			else
			{
				FName p(FBehavior::StaticLookupString(args[0]));
				ABasicArmor * armor = (ABasicArmor *) players[args[1]].mo->FindInventory(NAME_BasicArmor);
				if (armor && armor->ArmorType == p) return armor->Amount;
			}
			return 0;
		}

		case ACSF_SpawnSpotForced:
			return DoSpawnSpot(args[0], args[1], args[2], args[3], true);

		case ACSF_SpawnSpotFacingForced:
			return DoSpawnSpotFacing(args[0], args[1], args[2], true);

		case ACSF_CheckActorProperty:
			return (CheckActorProperty(args[0], args[1], args[2]));
        
        case ACSF_SetActorVelocity:
            if (args[0] == 0)
            {
				P_Thing_SetVelocity(activator, args[1], args[2], args[3], !!args[4], !!args[5]);
            }
            else
            {
                TActorIterator<AActor> iterator (args[0]);
                
                while ( (actor = iterator.Next ()) )
                {
					P_Thing_SetVelocity(actor, args[1], args[2], args[3], !!args[4], !!args[5]);
                }
            }
			return 0;

		case ACSF_SetUserVariable:
		{
			int cnt = 0;
			FName varname(FBehavior::StaticLookupString(args[1]), true);
			if (varname != NAME_None)
			{
				if (args[0] == 0)
				{
					if (activator != NULL)
					{
						SetUserVariable(activator, varname, 0, args[2]);
					}
					cnt++;
				}
				else
				{
					TActorIterator<AActor> iterator(args[0]);
	                
					while ( (actor = iterator.Next()) )
					{
						SetUserVariable(actor, varname, 0, args[2]);
						cnt++;
					}
				}
			}
			return cnt;
		}
		
		case ACSF_GetUserVariable:
		{
			FName varname(FBehavior::StaticLookupString(args[1]), true);
			if (varname != NAME_None)
			{
				AActor *a = args[0] == 0 ? (AActor *)activator : SingleActorFromTID(args[0], NULL); 
				return a != NULL ? GetUserVariable(a, varname, 0) : 0;
			}
			return 0;
		}

		case ACSF_SetUserArray:
		{
			int cnt = 0;
			FName varname(FBehavior::StaticLookupString(args[1]), true);
			if (varname != NAME_None)
			{
				if (args[0] == 0)
				{
					if (activator != NULL)
					{
						SetUserVariable(activator, varname, args[2], args[3]);
					}
					cnt++;
				}
				else
				{
					TActorIterator<AActor> iterator(args[0]);
	                
					while ( (actor = iterator.Next()) )
					{
						SetUserVariable(actor, varname, args[2], args[3]);
						cnt++;
					}
				}
			}
			return cnt;
		}
		
		case ACSF_GetUserArray:
		{
			FName varname(FBehavior::StaticLookupString(args[1]), true);
			if (varname != NAME_None)
			{
				AActor *a = args[0] == 0 ? (AActor *)activator : SingleActorFromTID(args[0], NULL); 
				return a != NULL ? GetUserVariable(a, varname, args[2]) : 0;
			}
			return 0;
		}

		case ACSF_Radius_Quake2:
			P_StartQuake(activator, args[0], args[1], args[2], args[3], args[4], FBehavior::StaticLookupString(args[5]));
			break;

		case ACSF_CheckActorClass:
		{
			AActor *a = args[0] == 0 ? (AActor *)activator : SingleActorFromTID(args[0], NULL);
			return a == NULL ? false : a->GetClass()->TypeName == FName(FBehavior::StaticLookupString(args[1]));
		}

		case ACSF_SoundSequenceOnActor:
			{
				const char *seqname = FBehavior::StaticLookupString(args[1]);
				if (seqname != NULL)
				{
					if (args[0] == 0)
					{
						if (activator != NULL)
						{
							SN_StartSequence(activator, seqname, 0);
						}
					}
					else
					{
						FActorIterator it(args[0]);
						AActor *actor;

						while ( (actor = it.Next()) )
						{
							SN_StartSequence(actor, seqname, 0);
						}
					}
				}
			}
			break;

		case ACSF_SoundSequenceOnSector:
			{
				const char *seqname = FBehavior::StaticLookupString(args[1]);
				int space = args[2] < CHAN_FLOOR || args[2] > CHAN_INTERIOR ? CHAN_FULLHEIGHT : args[2];
				if (seqname != NULL)
				{
					int secnum = -1;

					while ((secnum = P_FindSectorFromTag(args[0], secnum)) >= 0)
					{
						SN_StartSequence(&sectors[secnum], args[2], seqname, 0);
					}
				}
			}
			break;

		case ACSF_SoundSequenceOnPolyobj:
			{
				const char *seqname = FBehavior::StaticLookupString(args[1]);
				if (seqname != NULL)
				{
					FPolyObj *poly = PO_GetPolyobj(args[0]);
					if (poly != NULL)
					{
						SN_StartSequence(poly, seqname, 0);
					}
				}
			}
			break;

		// [BL] Skulltag function
		case ACSF_AnnouncerSound:
			if (args[1] == 0)
			{
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_AnnouncerSound(FBehavior::StaticLookupString(args[0]));
			}
			else
			{
				// Local announcement, needs player to activate.
				if (activator == NULL || activator->player == NULL)
					break;
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_AnnouncerSound(FBehavior::StaticLookupString(args[0]), activator->player - players, SVCF_ONLYTHISCLIENT);
			}
			ANNOUNCER_PlayEntry(cl_announcer, FBehavior::StaticLookupString(args[0]));
			return 0;

		// [BB]
		case ACSF_ResetMap:
			if ( GAMEMODE_GetFlags( GAMEMODE_GetCurrentMode( )) & GMF_MAPRESETS )
			{
				GAME_RequestMapRest ( );
				return 1;
			}
			else
			{
				Printf ( "ResetMap can only be used in game modes that support map resets.\n" );
				return 0;
			}

		// [BB]
		case ACSF_PlayerIsSpectator:
			{
				const ULONG ulPlayer = static_cast<ULONG> ( args[0] );
				if ( PLAYER_IsValidPlayer ( ulPlayer ) )
				{
					if ( ( GAMEMODE_GetFlags( GAMEMODE_GetCurrentMode( )) & GMF_DEADSPECTATORS ) && players[ulPlayer].bDeadSpectator )
						return 2;
					else
						return players[ulPlayer].bSpectating;
				}
				else
					return 0;
			}

		// [BB]
		case ACSF_ConsolePlayerNumber:
			{
				// [BB] The server doesn't have a reasonable associated player.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					return -1;
				else
					return consoleplayer;
			}

		case ACSF_GetTeamScore:
			{
				// [Dusk] Exported this to a new function
				return GetTeamScore (args[0]);
			}

		// [Dusk]
		case ACSF_GetTeamProperty:
			{
				return GetTeamProperty (args[0], args[1]);
			}

		case ACSF_GetPlayerLivesLeft:
			{
				const ULONG ulPlayer = static_cast<ULONG> ( args[0] );
				if ( PLAYER_IsValidPlayer ( ulPlayer ) )
					return PLAYER_GetLivesLeft ( ulPlayer );
				else
					return -1;
			}

		case ACSF_SetPlayerLivesLeft:
			{
				const ULONG ulPlayer = static_cast<ULONG> ( args[0] );
				if ( PLAYER_IsValidPlayer ( ulPlayer ) )
				{
					PLAYER_SetLivesLeft ( &players[ulPlayer], static_cast<ULONG> ( args[1] ) );
					return 1;
				}
				else
					return 0;
			}

		case ACSF_KickFromGame:
			{
				const ULONG ulPlayer = static_cast<ULONG> ( args[0] );
				if ( ( NETWORK_GetState( ) == NETSTATE_SERVER ) && PLAYER_IsValidPlayer ( ulPlayer ) && ( PLAYER_IsTrueSpectator ( &players[ulPlayer] ) == false ) )
				{
					SERVER_KickPlayerFromGame ( ulPlayer, FBehavior::StaticLookupString ( args[1] ) );
					return 1;
				}
				else
					return 0;
			}

		default:
			break;
	}

	return 0;
}

enum
{
	PRINTNAME_LEVELNAME		= -1,
	PRINTNAME_LEVEL			= -2,
	PRINTNAME_SKILL			= -3,
};


#define NEXTWORD	(LittleLong(*pc++))
#define NEXTBYTE	(fmt==ACS_LittleEnhanced?getbyte(pc):NEXTWORD)
#define NEXTSHORT	(fmt==ACS_LittleEnhanced?getshort(pc):NEXTWORD)
#define STACK(a)	(Stack[sp - (a)])
#define PushToStack(a)	(Stack[sp++] = (a))

inline int getbyte (int *&pc)
{
	int res = *(BYTE *)pc;
	pc = (int *)((BYTE *)pc+1);
	return res;
}

inline int getshort (int *&pc)
{
	int res = LittleShort( *(SWORD *)pc);
	pc = (int *)((BYTE *)pc+2);
	return res;
}

int DLevelScript::RunScript ()
{
	DACSThinker *controller = DACSThinker::ActiveThinker;
	SDWORD *locals = localvars;
	ScriptFunction *activeFunction = NULL;
	FRemapTable *translation = 0;
	int resultValue = 1;

	// [BB] Start to measure how much outbound net traffic this call of DLevelScript::RunScript() needs.
	NETWORK_StartTrafficMeasurement ( );

	switch (state)
	{
	case SCRIPT_Delayed:
		// Decrement the delay counter and enter state running
		// if it hits 0
		if (--statedata == 0)
			state = SCRIPT_Running;
		break;

	case SCRIPT_TagWait:
		// Wait for tagged sector(s) to go inactive, then enter
		// state running
	{
		int secnum = -1;

		while ((secnum = P_FindSectorFromTag (statedata, secnum)) >= 0)
			if (sectors[secnum].floordata || sectors[secnum].ceilingdata)
				return resultValue;

		// If we got here, none of the tagged sectors were busy
		state = SCRIPT_Running;
	}
	break;

	case SCRIPT_PolyWait:
		// Wait for polyobj(s) to stop moving, then enter state running
		if (!PO_Busy (statedata))
		{
			state = SCRIPT_Running;
		}
		break;

	case SCRIPT_ScriptWaitPre:
		// Wait for a script to start running, then enter state scriptwait
		if (controller->RunningScripts[statedata])
			state = SCRIPT_ScriptWait;
		break;

	case SCRIPT_ScriptWait:
		// Wait for a script to stop running, then enter state running
		if (controller->RunningScripts[statedata])
			return resultValue;

		state = SCRIPT_Running;
		PutFirst ();
		break;

	default:
		break;
	}

	SDWORD Stack[STACK_SIZE];
	int sp = 0;
	int *pc = this->pc;
	ACSFormat fmt = activeBehavior->GetFormat();
	int runaway = 0;	// used to prevent infinite loops
	int pcd;
	FString work;
	const char *lookup;
	int optstart = -1;
	int temp;

	// [BC] Since the server doesn't have a screen, we have to save the active font some
	// other way.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVER_SetCurrentFont( SERVER_GetScriptActiveFont( ));

	while (state == SCRIPT_Running)
	{
		if (++runaway > 500000)
		{
			Printf ("Runaway script %d terminated\n", script);
			state = SCRIPT_PleaseRemove;
			break;
		}

		if (fmt == ACS_LittleEnhanced)
		{
			pcd = getbyte(pc);
			if (pcd >= 256-16)
			{
				pcd = (256-16) + ((pcd - (256-16)) << 8) + getbyte(pc);
			}
		}
		else
		{
			pcd = NEXTWORD;
		}

		switch (pcd)
		{
		default:
			Printf ("Unknown P-Code %d in script %d\n", pcd, script);
			// fall through
		case PCD_TERMINATE:
			DPrintf ("Script %d finished\n", script);
			state = SCRIPT_PleaseRemove;
			break;

		case PCD_NOP:
			break;

		case PCD_SUSPEND:
			state = SCRIPT_Suspended;
			break;

		case PCD_TAGSTRING:
			Stack[sp-1] |= activeBehavior->GetLibraryID();
			break;

		case PCD_PUSHNUMBER:
			PushToStack (NEXTWORD);
			break;

		case PCD_PUSHBYTE:
			PushToStack (*(BYTE *)pc);
			pc = (int *)((BYTE *)pc + 1);
			break;

		case PCD_PUSH2BYTES:
			Stack[sp] = ((BYTE *)pc)[0];
			Stack[sp+1] = ((BYTE *)pc)[1];
			sp += 2;
			pc = (int *)((BYTE *)pc + 2);
			break;

		case PCD_PUSH3BYTES:
			Stack[sp] = ((BYTE *)pc)[0];
			Stack[sp+1] = ((BYTE *)pc)[1];
			Stack[sp+2] = ((BYTE *)pc)[2];
			sp += 3;
			pc = (int *)((BYTE *)pc + 3);
			break;

		case PCD_PUSH4BYTES:
			Stack[sp] = ((BYTE *)pc)[0];
			Stack[sp+1] = ((BYTE *)pc)[1];
			Stack[sp+2] = ((BYTE *)pc)[2];
			Stack[sp+3] = ((BYTE *)pc)[3];
			sp += 4;
			pc = (int *)((BYTE *)pc + 4);
			break;

		case PCD_PUSH5BYTES:
			Stack[sp] = ((BYTE *)pc)[0];
			Stack[sp+1] = ((BYTE *)pc)[1];
			Stack[sp+2] = ((BYTE *)pc)[2];
			Stack[sp+3] = ((BYTE *)pc)[3];
			Stack[sp+4] = ((BYTE *)pc)[4];
			sp += 5;
			pc = (int *)((BYTE *)pc + 5);
			break;

		case PCD_PUSHBYTES:
			temp = *(BYTE *)pc;
			pc = (int *)((BYTE *)pc + temp + 1);
			for (temp = -temp; temp; temp++)
			{
				PushToStack (*((BYTE *)pc + temp));
			}
			break;

		case PCD_DUP:
			Stack[sp] = Stack[sp-1];
			sp++;
			break;

		case PCD_SWAP:
			swapvalues(Stack[sp-2], Stack[sp-1]);
			break;

		case PCD_LSPEC1:
			LineSpecials[NEXTBYTE] (activationline, activator, backSide,
									STACK(1), 0, 0, 0, 0);
			sp -= 1;
			break;

		case PCD_LSPEC2:
			LineSpecials[NEXTBYTE] (activationline, activator, backSide,
									STACK(2), STACK(1), 0, 0, 0);
			sp -= 2;
			break;

		case PCD_LSPEC3:
			LineSpecials[NEXTBYTE] (activationline, activator, backSide,
									STACK(3), STACK(2), STACK(1), 0, 0);
			sp -= 3;
			break;

		case PCD_LSPEC4:
			LineSpecials[NEXTBYTE] (activationline, activator, backSide,
									STACK(4), STACK(3), STACK(2),
									STACK(1), 0);
			sp -= 4;
			break;

		case PCD_LSPEC5:
			LineSpecials[NEXTBYTE] (activationline, activator, backSide,
									STACK(5), STACK(4), STACK(3),
									STACK(2), STACK(1));
			sp -= 5;
			break;

		case PCD_LSPEC5RESULT:
			STACK(5) = LineSpecials[NEXTBYTE] (activationline, activator, backSide,
									STACK(5), STACK(4), STACK(3),
									STACK(2), STACK(1));
			sp -= 4;
			break;

		case PCD_LSPEC1DIRECT:
			temp = NEXTBYTE;
			LineSpecials[temp] (activationline, activator, backSide,
								pc[0], 0, 0, 0, 0);
			pc += 1;
			break;

		case PCD_LSPEC2DIRECT:
			temp = NEXTBYTE;
			LineSpecials[temp] (activationline, activator, backSide,
								pc[0], pc[1], 0, 0, 0);
			pc += 2;
			break;

		case PCD_LSPEC3DIRECT:
			temp = NEXTBYTE;
			LineSpecials[temp] (activationline, activator, backSide,
								pc[0], pc[1], pc[2], 0, 0);
			pc += 3;
			break;

		case PCD_LSPEC4DIRECT:
			temp = NEXTBYTE;
			LineSpecials[temp] (activationline, activator, backSide,
								pc[0], pc[1], pc[2], pc[3], 0);
			pc += 4;
			break;

		case PCD_LSPEC5DIRECT:
			temp = NEXTBYTE;
			LineSpecials[temp] (activationline, activator, backSide,
								pc[0], pc[1], pc[2], pc[3], pc[4]);
			pc += 5;
			break;

		case PCD_LSPEC1DIRECTB:
			LineSpecials[((BYTE *)pc)[0]] (activationline, activator, backSide,
				((BYTE *)pc)[1], 0, 0, 0, 0);
			pc = (int *)((BYTE *)pc + 2);
			break;

		case PCD_LSPEC2DIRECTB:
			LineSpecials[((BYTE *)pc)[0]] (activationline, activator, backSide,
				((BYTE *)pc)[1], ((BYTE *)pc)[2], 0, 0, 0);
			pc = (int *)((BYTE *)pc + 3);
			break;

		case PCD_LSPEC3DIRECTB:
			LineSpecials[((BYTE *)pc)[0]] (activationline, activator, backSide,
				((BYTE *)pc)[1], ((BYTE *)pc)[2], ((BYTE *)pc)[3], 0, 0);
			pc = (int *)((BYTE *)pc + 4);
			break;

		case PCD_LSPEC4DIRECTB:
			LineSpecials[((BYTE *)pc)[0]] (activationline, activator, backSide,
				((BYTE *)pc)[1], ((BYTE *)pc)[2], ((BYTE *)pc)[3],
				((BYTE *)pc)[4], 0);
			pc = (int *)((BYTE *)pc + 5);
			break;

		case PCD_LSPEC5DIRECTB:
			LineSpecials[((BYTE *)pc)[0]] (activationline, activator, backSide,
				((BYTE *)pc)[1], ((BYTE *)pc)[2], ((BYTE *)pc)[3],
				((BYTE *)pc)[4], ((BYTE *)pc)[5]);
			pc = (int *)((BYTE *)pc + 6);
			break;

		case PCD_CALLFUNC:
			{
				int argCount = NEXTBYTE;
				int funcIndex = NEXTSHORT;

				int retval = CallFunction(argCount, funcIndex, &STACK(argCount));
				sp -= argCount-1;
				STACK(1) = retval;
			}
			break;

		case PCD_CALL:
		case PCD_CALLDISCARD:
			{
				int funcnum;
				int i;
				ScriptFunction *func;
				FBehavior *module = activeBehavior;
				SDWORD *mylocals;

				funcnum = NEXTBYTE;
				func = activeBehavior->GetFunction (funcnum, module);
				if (func == NULL)
				{
					Printf ("Function %d in script %d out of range\n", funcnum, script);
					state = SCRIPT_PleaseRemove;
					break;
				}
				if (sp + func->LocalCount + 64 > STACK_SIZE)
				{ // 64 is the margin for the function's working space
					Printf ("Out of stack space in script %d\n", script);
					state = SCRIPT_PleaseRemove;
					break;
				}
				mylocals = locals;
				// The function's first argument is also its first local variable.
				locals = &Stack[sp - func->ArgCount];
				// Make space on the stack for any other variables the function uses.
				for (i = 0; i < func->LocalCount; ++i)
				{
					Stack[sp+i] = 0;
				}
				sp += i;
				::new(&Stack[sp]) CallReturn(activeBehavior->PC2Ofs(pc), activeFunction,
					activeBehavior, mylocals, pcd == PCD_CALLDISCARD, work);
				sp += (sizeof(CallReturn) + sizeof(int) - 1) / sizeof(int);
				pc = module->Ofs2PC (func->Address);
				activeFunction = func;
				activeBehavior = module;
				fmt = module->GetFormat();
			}
			break;

		case PCD_RETURNVOID:
		case PCD_RETURNVAL:
			{
				int value;
				union
				{
					SDWORD *retsp;
					CallReturn *ret;
				};

				if (pcd == PCD_RETURNVAL)
				{
					value = Stack[--sp];
				}
				else
				{
					value = 0;
				}
				sp -= sizeof(CallReturn)/sizeof(int);
				retsp = &Stack[sp];
				sp = int(locals - Stack);
				pc = ret->ReturnModule->Ofs2PC(ret->ReturnAddress);
				activeFunction = ret->ReturnFunction;
				activeBehavior = ret->ReturnModule;
				fmt = activeBehavior->GetFormat();
				locals = ret->ReturnLocals;
				if (!ret->bDiscardResult)
				{
					Stack[sp++] = value;
				}
				work = ret->StringBuilder;
				ret->~CallReturn();
			}
			break;

		case PCD_ADD:
			STACK(2) = STACK(2) + STACK(1);
			sp--;
			break;

		case PCD_SUBTRACT:
			STACK(2) = STACK(2) - STACK(1);
			sp--;
			break;

		case PCD_MULTIPLY:
			STACK(2) = STACK(2) * STACK(1);
			sp--;
			break;

		case PCD_DIVIDE:
			if (STACK(1) == 0)
			{
				state = SCRIPT_DivideBy0;
			}
			else
			{
				STACK(2) = STACK(2) / STACK(1);
				sp--;
			}
			break;

		case PCD_MODULUS:
			if (STACK(1) == 0)
			{
				state = SCRIPT_ModulusBy0;
			}
			else
			{
				STACK(2) = STACK(2) % STACK(1);
				sp--;
			}
			break;

		case PCD_EQ:
			STACK(2) = (STACK(2) == STACK(1));
			sp--;
			break;

		case PCD_NE:
			STACK(2) = (STACK(2) != STACK(1));
			sp--;
			break;

		case PCD_LT:
			STACK(2) = (STACK(2) < STACK(1));
			sp--;
			break;

		case PCD_GT:
			STACK(2) = (STACK(2) > STACK(1));
			sp--;
			break;

		case PCD_LE:
			STACK(2) = (STACK(2) <= STACK(1));
			sp--;
			break;

		case PCD_GE:
			STACK(2) = (STACK(2) >= STACK(1));
			sp--;
			break;

		case PCD_ASSIGNSCRIPTVAR:
			locals[NEXTBYTE] = STACK(1);
			sp--;
			break;


		case PCD_ASSIGNMAPVAR:
			*(activeBehavior->MapVars[NEXTBYTE]) = STACK(1);
			sp--;
			break;

		case PCD_ASSIGNWORLDVAR:
			ACS_WorldVars[NEXTBYTE] = STACK(1);
			sp--;
			break;

		case PCD_ASSIGNGLOBALVAR:
			ACS_GlobalVars[NEXTBYTE] = STACK(1);
			sp--;
			break;

		case PCD_ASSIGNMAPARRAY:
			activeBehavior->SetArrayVal (*(activeBehavior->MapVars[NEXTBYTE]), STACK(2), STACK(1));
			sp -= 2;
			break;

		case PCD_ASSIGNWORLDARRAY:
			ACS_WorldArrays[NEXTBYTE][STACK(2)] = STACK(1);
			sp -= 2;
			break;

		case PCD_ASSIGNGLOBALARRAY:
			ACS_GlobalArrays[NEXTBYTE][STACK(2)] = STACK(1);
			sp -= 2;
			break;

		case PCD_PUSHSCRIPTVAR:
			PushToStack (locals[NEXTBYTE]);
			break;

		case PCD_PUSHMAPVAR:
			PushToStack (*(activeBehavior->MapVars[NEXTBYTE]));
			break;

		case PCD_PUSHWORLDVAR:
			PushToStack (ACS_WorldVars[NEXTBYTE]);
			break;

		case PCD_PUSHGLOBALVAR:
			PushToStack (ACS_GlobalVars[NEXTBYTE]);
			break;

		case PCD_PUSHMAPARRAY:
			STACK(1) = activeBehavior->GetArrayVal (*(activeBehavior->MapVars[NEXTBYTE]), STACK(1));
			break;

		case PCD_PUSHWORLDARRAY:
			STACK(1) = ACS_WorldArrays[NEXTBYTE][STACK(1)];
			break;

		case PCD_PUSHGLOBALARRAY:
			STACK(1) = ACS_GlobalArrays[NEXTBYTE][STACK(1)];
			break;

		case PCD_ADDSCRIPTVAR:
			locals[NEXTBYTE] += STACK(1);
			sp--;
			break;

		case PCD_ADDMAPVAR:
			*(activeBehavior->MapVars[NEXTBYTE]) += STACK(1);
			sp--;
			break;

		case PCD_ADDWORLDVAR:
			ACS_WorldVars[NEXTBYTE] += STACK(1);
			sp--;
			break;

		case PCD_ADDGLOBALVAR:
			ACS_GlobalVars[NEXTBYTE] += STACK(1);
			sp--;
			break;

		case PCD_ADDMAPARRAY:
			{
				int a = *(activeBehavior->MapVars[NEXTBYTE]);
				int i = STACK(2);
				activeBehavior->SetArrayVal (a, i, activeBehavior->GetArrayVal (a, i) + STACK(1));
				sp -= 2;
			}
			break;

		case PCD_ADDWORLDARRAY:
			{
				int a = NEXTBYTE;
				ACS_WorldArrays[a][STACK(2)] += STACK(1);
				sp -= 2;
			}
			break;

		case PCD_ADDGLOBALARRAY:
			{
				int a = NEXTBYTE;
				ACS_GlobalArrays[a][STACK(2)] += STACK(1);
				sp -= 2;
			}
			break;

		case PCD_SUBSCRIPTVAR:
			locals[NEXTBYTE] -= STACK(1);
			sp--;
			break;

		case PCD_SUBMAPVAR:
			*(activeBehavior->MapVars[NEXTBYTE]) -= STACK(1);
			sp--;
			break;

		case PCD_SUBWORLDVAR:
			ACS_WorldVars[NEXTBYTE] -= STACK(1);
			sp--;
			break;

		case PCD_SUBGLOBALVAR:
			ACS_GlobalVars[NEXTBYTE] -= STACK(1);
			sp--;
			break;

		case PCD_SUBMAPARRAY:
			{
				int a = *(activeBehavior->MapVars[NEXTBYTE]);
				int i = STACK(2);
				activeBehavior->SetArrayVal (a, i, activeBehavior->GetArrayVal (a, i) - STACK(1));
				sp -= 2;
			}
			break;

		case PCD_SUBWORLDARRAY:
			{
				int a = NEXTBYTE;
				ACS_WorldArrays[a][STACK(2)] -= STACK(1);
				sp -= 2;
			}
			break;

		case PCD_SUBGLOBALARRAY:
			{
				int a = NEXTBYTE;
				ACS_GlobalArrays[a][STACK(2)] -= STACK(1);
				sp -= 2;
			}
			break;

		case PCD_MULSCRIPTVAR:
			locals[NEXTBYTE] *= STACK(1);
			sp--;
			break;

		case PCD_MULMAPVAR:
			*(activeBehavior->MapVars[NEXTBYTE]) *= STACK(1);
			sp--;
			break;

		case PCD_MULWORLDVAR:
			ACS_WorldVars[NEXTBYTE] *= STACK(1);
			sp--;
			break;

		case PCD_MULGLOBALVAR:
			ACS_GlobalVars[NEXTBYTE] *= STACK(1);
			sp--;
			break;

		case PCD_MULMAPARRAY:
			{
				int a = *(activeBehavior->MapVars[NEXTBYTE]);
				int i = STACK(2);
				activeBehavior->SetArrayVal (a, i, activeBehavior->GetArrayVal (a, i) * STACK(1));
				sp -= 2;
			}
			break;

		case PCD_MULWORLDARRAY:
			{
				int a = NEXTBYTE;
				ACS_WorldArrays[a][STACK(2)] *= STACK(1);
				sp -= 2;
			}
			break;

		case PCD_MULGLOBALARRAY:
			{
				int a = NEXTBYTE;
				ACS_GlobalArrays[a][STACK(2)] *= STACK(1);
				sp -= 2;
			}
			break;

		case PCD_DIVSCRIPTVAR:
			if (STACK(1) == 0)
			{
				state = SCRIPT_DivideBy0;
			}
			else
			{
				locals[NEXTBYTE] /= STACK(1);
				sp--;
			}
			break;

		case PCD_DIVMAPVAR:
			if (STACK(1) == 0)
			{
				state = SCRIPT_DivideBy0;
			}
			else
			{
				*(activeBehavior->MapVars[NEXTBYTE]) /= STACK(1);
				sp--;
			}
			break;

		case PCD_DIVWORLDVAR:
			if (STACK(1) == 0)
			{
				state = SCRIPT_DivideBy0;
			}
			else
			{
				ACS_WorldVars[NEXTBYTE] /= STACK(1);
				sp--;
			}
			break;

		case PCD_DIVGLOBALVAR:
			if (STACK(1) == 0)
			{
				state = SCRIPT_DivideBy0;
			}
			else
			{
				ACS_GlobalVars[NEXTBYTE] /= STACK(1);
				sp--;
			}
			break;

		case PCD_DIVMAPARRAY:
			if (STACK(1) == 0)
			{
				state = SCRIPT_DivideBy0;
			}
			else
			{
				int a = *(activeBehavior->MapVars[NEXTBYTE]);
				int i = STACK(2);
				activeBehavior->SetArrayVal (a, i, activeBehavior->GetArrayVal (a, i) / STACK(1));
				sp -= 2;
			}
			break;

		case PCD_DIVWORLDARRAY:
			if (STACK(1) == 0)
			{
				state = SCRIPT_DivideBy0;
			}
			else
			{
				int a = NEXTBYTE;
				ACS_WorldArrays[a][STACK(2)] /= STACK(1);
				sp -= 2;
			}
			break;

		case PCD_DIVGLOBALARRAY:
			if (STACK(1) == 0)
			{
				state = SCRIPT_DivideBy0;
			}
			else
			{
				int a = NEXTBYTE;
				ACS_GlobalArrays[a][STACK(2)] /= STACK(1);
				sp -= 2;
			}
			break;

		case PCD_MODSCRIPTVAR:
			if (STACK(1) == 0)
			{
				state = SCRIPT_ModulusBy0;
			}
			else
			{
				locals[NEXTBYTE] %= STACK(1);
				sp--;
			}
			break;

		case PCD_MODMAPVAR:
			if (STACK(1) == 0)
			{
				state = SCRIPT_ModulusBy0;
			}
			else
			{
				*(activeBehavior->MapVars[NEXTBYTE]) %= STACK(1);
				sp--;
			}
			break;

		case PCD_MODWORLDVAR:
			if (STACK(1) == 0)
			{
				state = SCRIPT_ModulusBy0;
			}
			else
			{
				ACS_WorldVars[NEXTBYTE] %= STACK(1);
				sp--;
			}
			break;

		case PCD_MODGLOBALVAR:
			if (STACK(1) == 0)
			{
				state = SCRIPT_ModulusBy0;
			}
			else
			{
				ACS_GlobalVars[NEXTBYTE] %= STACK(1);
				sp--;
			}
			break;

		case PCD_MODMAPARRAY:
			if (STACK(1) == 0)
			{
				state = SCRIPT_ModulusBy0;
			}
			else
			{
				int a = *(activeBehavior->MapVars[NEXTBYTE]);
				int i = STACK(2);
				activeBehavior->SetArrayVal (a, i, activeBehavior->GetArrayVal (a, i) % STACK(1));
				sp -= 2;
			}
			break;

		case PCD_MODWORLDARRAY:
			if (STACK(1) == 0)
			{
				state = SCRIPT_ModulusBy0;
			}
			else
			{
				int a = NEXTBYTE;
				ACS_WorldArrays[a][STACK(2)] %= STACK(1);
				sp -= 2;
			}
			break;

		case PCD_MODGLOBALARRAY:
			if (STACK(1) == 0)
			{
				state = SCRIPT_ModulusBy0;
			}
			else
			{
				int a = NEXTBYTE;
				ACS_GlobalArrays[a][STACK(2)] %= STACK(1);
				sp -= 2;
			}
			break;

		//[MW] start
		case PCD_ANDSCRIPTVAR:
			locals[NEXTBYTE] &= STACK(1);
			sp--;
			break;

		case PCD_ANDMAPVAR:
			*(activeBehavior->MapVars[NEXTBYTE]) &= STACK(1);
			sp--;
			break;

		case PCD_ANDWORLDVAR:
			ACS_WorldVars[NEXTBYTE] &= STACK(1);
			sp--;
			break;

		case PCD_ANDGLOBALVAR:
			ACS_GlobalVars[NEXTBYTE] &= STACK(1);
			sp--;
			break;

		case PCD_ANDMAPARRAY:
			{
				int a = *(activeBehavior->MapVars[NEXTBYTE]);
				int i = STACK(2);
				activeBehavior->SetArrayVal (a, i, activeBehavior->GetArrayVal (a, i) & STACK(1));
				sp -= 2;
			}
			break;

		case PCD_ANDWORLDARRAY:
			{
				int a = NEXTBYTE;
				ACS_WorldArrays[a][STACK(2)] &= STACK(1);
				sp -= 2;
			}
			break;

		case PCD_ANDGLOBALARRAY:
			{
				int a = NEXTBYTE;
				ACS_GlobalArrays[a][STACK(2)] &= STACK(1);
				sp -= 2;
			}
			break;

		case PCD_EORSCRIPTVAR:
			locals[NEXTBYTE] ^= STACK(1);
			sp--;
			break;

		case PCD_EORMAPVAR:
			*(activeBehavior->MapVars[NEXTBYTE]) ^= STACK(1);
			sp--;
			break;

		case PCD_EORWORLDVAR:
			ACS_WorldVars[NEXTBYTE] ^= STACK(1);
			sp--;
			break;

		case PCD_EORGLOBALVAR:
			ACS_GlobalVars[NEXTBYTE] ^= STACK(1);
			sp--;
			break;

		case PCD_EORMAPARRAY:
			{
				int a = *(activeBehavior->MapVars[NEXTBYTE]);
				int i = STACK(2);
				activeBehavior->SetArrayVal (a, i, activeBehavior->GetArrayVal (a, i) ^ STACK(1));
				sp -= 2;
			}
			break;

		case PCD_EORWORLDARRAY:
			{
				int a = NEXTBYTE;
				ACS_WorldArrays[a][STACK(2)] ^= STACK(1);
				sp -= 2;
			}
			break;

		case PCD_EORGLOBALARRAY:
			{
				int a = NEXTBYTE;
				ACS_GlobalArrays[a][STACK(2)] ^= STACK(1);
				sp -= 2;
			}
			break;

		case PCD_ORSCRIPTVAR:
			locals[NEXTBYTE] |= STACK(1);
			sp--;
			break;

		case PCD_ORMAPVAR:
			*(activeBehavior->MapVars[NEXTBYTE]) |= STACK(1);
			sp--;
			break;

		case PCD_ORWORLDVAR:
			ACS_WorldVars[NEXTBYTE] |= STACK(1);
			sp--;
			break;

		case PCD_ORGLOBALVAR:
			ACS_GlobalVars[NEXTBYTE] |= STACK(1);
			sp--;
			break;

		case PCD_ORMAPARRAY:
			{
				int a = *(activeBehavior->MapVars[NEXTBYTE]);
				int i = STACK(2);
				activeBehavior->SetArrayVal (a, i, activeBehavior->GetArrayVal (a, i) | STACK(1));
				sp -= 2;
			}
			break;

		case PCD_ORWORLDARRAY:
			{
				int a = NEXTBYTE;
				ACS_WorldArrays[a][STACK(2)] |= STACK(1);
				sp -= 2;
			}
			break;

		case PCD_ORGLOBALARRAY:
			{
				int a = NEXTBYTE;
				int i = STACK(2);
				ACS_GlobalArrays[a][STACK(2)] |= STACK(1);
				sp -= 2;
			}
			break;

		case PCD_LSSCRIPTVAR:
			locals[NEXTBYTE] <<= STACK(1);
			sp--;
			break;

		case PCD_LSMAPVAR:
			*(activeBehavior->MapVars[NEXTBYTE]) <<= STACK(1);
			sp--;
			break;

		case PCD_LSWORLDVAR:
			ACS_WorldVars[NEXTBYTE] <<= STACK(1);
			sp--;
			break;

		case PCD_LSGLOBALVAR:
			ACS_GlobalVars[NEXTBYTE] <<= STACK(1);
			sp--;
			break;

		case PCD_LSMAPARRAY:
			{
				int a = *(activeBehavior->MapVars[NEXTBYTE]);
				int i = STACK(2);
				activeBehavior->SetArrayVal (a, i, activeBehavior->GetArrayVal (a, i) << STACK(1));
				sp -= 2;
			}
			break;

		case PCD_LSWORLDARRAY:
			{
				int a = NEXTBYTE;
				ACS_WorldArrays[a][STACK(2)] <<= STACK(1);
				sp -= 2;
			}
			break;

		case PCD_LSGLOBALARRAY:
			{
				int a = NEXTBYTE;
				ACS_GlobalArrays[a][STACK(2)] <<= STACK(1);
				sp -= 2;
			}
			break;

		case PCD_RSSCRIPTVAR:
			locals[NEXTBYTE] >>= STACK(1);
			sp--;
			break;

		case PCD_RSMAPVAR:
			*(activeBehavior->MapVars[NEXTBYTE]) >>= STACK(1);
			sp--;
			break;

		case PCD_RSWORLDVAR:
			ACS_WorldVars[NEXTBYTE] >>= STACK(1);
			sp--;
			break;

		case PCD_RSGLOBALVAR:
			ACS_GlobalVars[NEXTBYTE] >>= STACK(1);
			sp--;
			break;

		case PCD_RSMAPARRAY:
			{
				int a = *(activeBehavior->MapVars[NEXTBYTE]);
				int i = STACK(2);
				activeBehavior->SetArrayVal (a, i, activeBehavior->GetArrayVal (a, i) >> STACK(1));
				sp -= 2;
			}
			break;

		case PCD_RSWORLDARRAY:
			{
				int a = NEXTBYTE;
				ACS_WorldArrays[a][STACK(2)] >>= STACK(1);
				sp -= 2;
			}
			break;

		case PCD_RSGLOBALARRAY:
			{
				int a = NEXTBYTE;
				ACS_GlobalArrays[a][STACK(2)] >>= STACK(1);
				sp -= 2;
			}
			break;
		//[MW] end

		case PCD_INCSCRIPTVAR:
			++locals[NEXTBYTE];
			break;

		case PCD_INCMAPVAR:
			*(activeBehavior->MapVars[NEXTBYTE]) += 1;
			break;

		case PCD_INCWORLDVAR:
			++ACS_WorldVars[NEXTBYTE];
			break;

		case PCD_INCGLOBALVAR:
			++ACS_GlobalVars[NEXTBYTE];
			break;

		case PCD_INCMAPARRAY:
			{
				int a = *(activeBehavior->MapVars[NEXTBYTE]);
				int i = STACK(1);
				activeBehavior->SetArrayVal (a, i, activeBehavior->GetArrayVal (a, i) + 1);
				sp--;
			}
			break;

		case PCD_INCWORLDARRAY:
			{
				int a = NEXTBYTE;
				ACS_WorldArrays[a][STACK(1)] += 1;
				sp--;
			}
			break;

		case PCD_INCGLOBALARRAY:
			{
				int a = NEXTBYTE;
				ACS_GlobalArrays[a][STACK(1)] += 1;
				sp--;
			}
			break;

		case PCD_DECSCRIPTVAR:
			--locals[NEXTBYTE];
			break;

		case PCD_DECMAPVAR:
			*(activeBehavior->MapVars[NEXTBYTE]) -= 1;
			break;

		case PCD_DECWORLDVAR:
			--ACS_WorldVars[NEXTBYTE];
			break;

		case PCD_DECGLOBALVAR:
			--ACS_GlobalVars[NEXTBYTE];
			break;

		case PCD_DECMAPARRAY:
			{
				int a = *(activeBehavior->MapVars[NEXTBYTE]);
				int i = STACK(1);
				activeBehavior->SetArrayVal (a, i, activeBehavior->GetArrayVal (a, i) - 1);
				sp--;
			}
			break;

		case PCD_DECWORLDARRAY:
			{
				int a = NEXTBYTE;
				ACS_WorldArrays[a][STACK(1)] -= 1;
				sp--;
			}
			break;

		case PCD_DECGLOBALARRAY:
			{
				int a = NEXTBYTE;
				int i = STACK(1);
				ACS_GlobalArrays[a][STACK(1)] -= 1;
				sp--;
			}
			break;

		case PCD_GOTO:
			pc = activeBehavior->Ofs2PC (*pc);
			break;

		case PCD_IFGOTO:
			if (STACK(1))
				pc = activeBehavior->Ofs2PC (*pc);
			else
				pc++;
			sp--;
			break;

		case PCD_DROP:
		case PCD_SETRESULTVALUE:
			resultValue = STACK(1);
			sp--;
			break;

		case PCD_DELAY:
			statedata = STACK(1) + (fmt == ACS_Old && gameinfo.gametype == GAME_Hexen);
			if (statedata > 0)
			{
				state = SCRIPT_Delayed;
			}
			sp--;
			break;

		case PCD_DELAYDIRECT:
			statedata = NEXTWORD + (fmt == ACS_Old && gameinfo.gametype == GAME_Hexen);
			if (statedata > 0)
			{
				state = SCRIPT_Delayed;
			}
			break;

		case PCD_DELAYDIRECTB:
			statedata = *(BYTE *)pc + (fmt == ACS_Old && gameinfo.gametype == GAME_Hexen);
			if (statedata > 0)
			{
				state = SCRIPT_Delayed;
			}
			pc = (int *)((BYTE *)pc + 1);
			break;

		case PCD_RANDOM:
			STACK(2) = Random (STACK(2), STACK(1));
			sp--;
			break;

		case PCD_RANDOMDIRECT:
			PushToStack (Random (pc[0], pc[1]));
			pc += 2;
			break;

		case PCD_RANDOMDIRECTB:
			PushToStack (Random (((BYTE *)pc)[0], ((BYTE *)pc)[1]));
			pc = (int *)((BYTE *)pc + 2);
			break;

		case PCD_THINGCOUNT:
			STACK(2) = ThingCount (STACK(2), -1, STACK(1), -1);
			sp--;
			break;

		case PCD_THINGCOUNTDIRECT:
			PushToStack (ThingCount (pc[0], -1, pc[1], -1));
			pc += 2;
			break;

		case PCD_THINGCOUNTNAME:
			STACK(2) = ThingCount (-1, STACK(2), STACK(1), -1);
			sp--;
			break;

		case PCD_THINGCOUNTNAMESECTOR:
			STACK(3) = ThingCount (-1, STACK(3), STACK(2), STACK(1));
			sp -= 2;
			break;

		case PCD_THINGCOUNTSECTOR:
			STACK(3) = ThingCount (STACK(3), -1, STACK(2), STACK(1));
			sp -= 2;
			break;

		case PCD_TAGWAIT:
			state = SCRIPT_TagWait;
			statedata = STACK(1);
			sp--;
			break;

		case PCD_TAGWAITDIRECT:
			state = SCRIPT_TagWait;
			statedata = NEXTWORD;
			break;

		case PCD_POLYWAIT:
			state = SCRIPT_PolyWait;
			statedata = STACK(1);
			sp--;
			break;

		case PCD_POLYWAITDIRECT:
			state = SCRIPT_PolyWait;
			statedata = NEXTWORD;
			break;

		case PCD_CHANGEFLOOR:
			ChangeFlat (STACK(2), STACK(1), 0);
			sp -= 2;
			break;

		case PCD_CHANGEFLOORDIRECT:
			ChangeFlat (pc[0], pc[1], 0);
			pc += 2;
			break;

		case PCD_CHANGECEILING:
			ChangeFlat (STACK(2), STACK(1), 1);
			sp -= 2;
			break;

		case PCD_CHANGECEILINGDIRECT:
			ChangeFlat (pc[0], pc[1], 1);
			pc += 2;
			break;

		case PCD_RESTART:
			{
				const ScriptPtr *scriptp;

				scriptp = activeBehavior->FindScript (script);
				pc = activeBehavior->GetScriptAddress (scriptp);
			}
			break;

		case PCD_ANDLOGICAL:
			STACK(2) = (STACK(2) && STACK(1));
			sp--;
			break;

		case PCD_ORLOGICAL:
			STACK(2) = (STACK(2) || STACK(1));
			sp--;
			break;

		case PCD_ANDBITWISE:
			STACK(2) = (STACK(2) & STACK(1));
			sp--;
			break;

		case PCD_ORBITWISE:
			STACK(2) = (STACK(2) | STACK(1));
			sp--;
			break;

		case PCD_EORBITWISE:
			STACK(2) = (STACK(2) ^ STACK(1));
			sp--;
			break;

		case PCD_NEGATELOGICAL:
			STACK(1) = !STACK(1);
			break;

		case PCD_NEGATEBINARY:
			STACK(1) = ~STACK(1);
			break;

		case PCD_LSHIFT:
			STACK(2) = (STACK(2) << STACK(1));
			sp--;
			break;

		case PCD_RSHIFT:
			STACK(2) = (STACK(2) >> STACK(1));
			sp--;
			break;

		case PCD_UNARYMINUS:
			STACK(1) = -STACK(1);
			break;

		case PCD_IFNOTGOTO:
			if (!STACK(1))
				pc = activeBehavior->Ofs2PC (*pc);
			else
				pc++;
			sp--;
			break;

		case PCD_LINESIDE:
			PushToStack (backSide);
			break;

		case PCD_SCRIPTWAIT:
			statedata = STACK(1);
			if (controller->RunningScripts[statedata])
				state = SCRIPT_ScriptWait;
			else
				state = SCRIPT_ScriptWaitPre;
			sp--;
			PutLast ();
			break;

		case PCD_SCRIPTWAITDIRECT:
			state = SCRIPT_ScriptWait;
			statedata = NEXTWORD;
			PutLast ();
			break;

		case PCD_CLEARLINESPECIAL:
			if (activationline)
				activationline->special = 0;
			break;

		case PCD_CASEGOTO:
			if (STACK(1) == NEXTWORD)
			{
				pc = activeBehavior->Ofs2PC (*pc);
				sp--;
			}
			else
			{
				pc++;
			}
			break;

		case PCD_CASEGOTOSORTED:
			// The count and jump table are 4-byte aligned
			pc = (int *)(((size_t)pc + 3) & ~3);
			{
				int numcases = NEXTWORD;
				int min = 0, max = numcases-1;
				while (min <= max)
				{
					int mid = (min + max) / 2;
					SDWORD caseval = pc[mid*2];
					if (caseval == STACK(1))
					{
						pc = activeBehavior->Ofs2PC (pc[mid*2+1]);
						sp--;
						break;
					}
					else if (caseval < STACK(1))
					{
						min = mid + 1;
					}
					else
					{
						max = mid - 1;
					}
				}
				if (min > max)
				{
					// The case was not found, so go to the next instruction.
					pc += numcases * 2;
				}
			}
			break;

		case PCD_BEGINPRINT:
			STRINGBUILDER_START(work);
			break;

		case PCD_PRINTSTRING:
		case PCD_PRINTLOCALIZED:
			lookup = FBehavior::StaticLookupString (STACK(1));
			if (pcd == PCD_PRINTLOCALIZED)
			{
				lookup = GStrings(lookup);
			}
			if (lookup != NULL)
			{
				work += lookup;
			}
			--sp;
			break;

		case PCD_PRINTNUMBER:
			work.AppendFormat ("%d", STACK(1));
			--sp;
			break;

		case PCD_PRINTBINARY:
			work.AppendFormat ("%B", STACK(1));
			--sp;
			break;

		case PCD_PRINTHEX:
			work.AppendFormat ("%X", STACK(1));
			--sp;
			break;

		case PCD_PRINTCHARACTER:
			work += (char)STACK(1);
			--sp;
			break;

		case PCD_PRINTFIXED:
			work.AppendFormat ("%g", FIXED2FLOAT(STACK(1)));
			--sp;
			break;

		// [BC] Print activator's name
		// [RH] Fancied up a bit
		case PCD_PRINTNAME:
			{
				player_t *player = NULL;

				if (STACK(1) < 0)
				{
					switch (STACK(1))
					{
					case PRINTNAME_LEVELNAME:
						work += level.LevelName;
						break;

					case PRINTNAME_LEVEL:
						work += level.mapname;
						break;

					case PRINTNAME_SKILL:
						work += G_SkillName();
						break;

					default:
						work += ' ';
						break;
					}
					sp--;
					break;

				}
				else if (STACK(1) == 0 || (unsigned)STACK(1) > MAXPLAYERS)
				{
					if (activator)
					{
						player = activator->player;
					}
				}
				else if (playeringame[STACK(1)-1])
				{
					player = &players[STACK(1)-1];
				}
				else
				{
					work.AppendFormat ("Player %d", STACK(1));
					sp--;
					break;
				}
				if (player)
				{
					work += player->userinfo.netname;
				}
				else if (activator)
				{
					work += RUNTIME_TYPE(activator)->TypeName.GetChars();
				}
				else
				{
					work += ' ';
				}
				sp--;
			}
			break;

		// [JB] Print map character array
		case PCD_PRINTMAPCHARARRAY:
			{
				int a = *(activeBehavior->MapVars[STACK(1)]);
				int offset = STACK(2);
				int c;
				while((c = activeBehavior->GetArrayVal (a, offset)) != '\0') {
					work += (char)c;
					offset++;
				}
				sp-=2;
			}
			break;

		// [JB] Print world character array
		case PCD_PRINTWORLDCHARARRAY:
			{
				int a = STACK(1);
				int offset = STACK(2);
				int c;
				while((c = ACS_WorldArrays[a][offset]) != '\0') {
					work += (char)c;
					offset++;
				}
				sp-=2;
			}
			break;

		// [JB] Print global character array
		case PCD_PRINTGLOBALCHARARRAY:
			{
				int a = STACK(1);
				int offset = STACK(2);
				int c;
				while((c = ACS_GlobalArrays[a][offset]) != '\0') {
					work += (char)c;
					offset++;
				}
				sp-=2;
			}
			break;

		// [GRB] Print key name(s) for a command
		case PCD_PRINTBIND:
			lookup = FBehavior::StaticLookupString (STACK(1));
			if (lookup != NULL)
			{
				int key1 = 0, key2 = 0;

				C_GetKeysForCommand ((char *)lookup, &key1, &key2);

				if (key2)
					work << KeyNames[key1] << " or " << KeyNames[key2];
				else if (key1)
					work << KeyNames[key1];
				else
					work << "??? (" << (char *)lookup << ')';
			}
			--sp;
			break;

		case PCD_ENDPRINT:
		case PCD_ENDPRINTBOLD:
		case PCD_MOREHUDMESSAGE:
		case PCD_ENDLOG:
			work = strbin1 (work);
			if (pcd == PCD_ENDLOG)
			{
				Printf ("%s\n", work.GetChars());
				STRINGBUILDER_FINISH(work);
			}
			else if (pcd != PCD_MOREHUDMESSAGE)
			{
				AActor *screen = activator;
				// If a missile is the activator, make the thing that
				// launched the missile the target of the print command.
				if (screen != NULL &&
					screen->player == NULL &&
					(screen->flags & MF_MISSILE) &&
					screen->target != NULL)
				{
					screen = screen->target;
				}
				if (pcd == PCD_ENDPRINTBOLD || screen == NULL ||
					screen->CheckLocalView (consoleplayer))
				{
					C_MidPrint (activefont, work);
				}

				// [BC] Potentially tell clients to print this message.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				{
					// Printbold displays to everyone.
					if (( pcd == PCD_ENDPRINTBOLD ) || ( screen == NULL ))
						SERVERCOMMANDS_PrintMid( work.GetChars( ), false );
					// Otherwise, if a player is the activator, send him the message.
					else if ( screen->player )
						SERVERCOMMANDS_PrintMid( work.GetChars( ), false, screen->player - players, SVCF_ONLYTHISCLIENT );
				}

				STRINGBUILDER_FINISH(work);
			}
			else
			{
				optstart = -1;
			}
			break;

		case PCD_OPTHUDMESSAGE:
			optstart = sp;
			break;

		case PCD_ENDHUDMESSAGE:
		case PCD_ENDHUDMESSAGEBOLD:
			if (optstart == -1)
			{
				optstart = sp;
			}
			{
				AActor *screen = activator;
				if (screen != NULL &&
					screen->player == NULL &&
					(screen->flags & MF_MISSILE) &&
					screen->target != NULL)
				{
					screen = screen->target;
				}
				if (pcd == PCD_ENDHUDMESSAGEBOLD || screen == NULL ||
					players[consoleplayer].mo == screen || NETWORK_GetState( ) == NETSTATE_SERVER )
				{
					int type = Stack[optstart-6];
					int id = Stack[optstart-5];
					EColorRange color;
					float x = FIXED2FLOAT(Stack[optstart-3]);
					float y = FIXED2FLOAT(Stack[optstart-2]);
					float holdTime = FIXED2FLOAT(Stack[optstart-1]);
					DHUDMessage *msg = NULL;

					if (type & HUDMSG_COLORSTRING)
					{
						color = V_FindFontColor(FBehavior::StaticLookupString(Stack[optstart-4]));
					}
					else
					{
						color = CLAMPCOLOR(Stack[optstart-4]);
					}

					switch (type & 0xFFFF)
					{
					default:	// normal

						// [BC] Tell clients to print this message.
						if ( NETWORK_GetState( ) == NETSTATE_SERVER )
						{
							if (( pcd == PCD_ENDHUDMESSAGEBOLD ) || ( screen == NULL ))
								SERVERCOMMANDS_PrintHUDMessage( work.GetChars( ), x, y, hudwidth, hudheight, color, holdTime, SERVER_GetCurrentFont( ), !!( type & HUDMSG_LOG ), id );
							else if ( screen->player )
								SERVERCOMMANDS_PrintHUDMessage( work.GetChars( ), x, y, hudwidth, hudheight, color, holdTime, SERVER_GetCurrentFont( ), !!( type & HUDMSG_LOG ), id, screen->player - players, SVCF_ONLYTHISCLIENT );
						}
						else
							msg = new DHUDMessage (activefont, work, x, y, hudwidth, hudheight, color, holdTime);
						break;
					case 1:		// fade out
						{
							float fadeTime = (optstart < sp) ? FIXED2FLOAT(Stack[optstart]) : 0.5f;

							// [BC] Tell clients to print this message.
							if ( NETWORK_GetState( ) == NETSTATE_SERVER )
							{
								if (( pcd == PCD_ENDHUDMESSAGEBOLD ) || ( screen == NULL ))
									SERVERCOMMANDS_PrintHUDMessageFadeOut( work.GetChars( ), x, y, hudwidth, hudheight, color, holdTime, fadeTime, SERVER_GetCurrentFont( ), !!( type & HUDMSG_LOG ), id );
								else if ( screen->player )
									SERVERCOMMANDS_PrintHUDMessageFadeOut( work.GetChars( ), x, y, hudwidth, hudheight, color, holdTime, fadeTime, SERVER_GetCurrentFont( ), !!( type & HUDMSG_LOG ), id, screen->player - players, SVCF_ONLYTHISCLIENT );
							}
							else
								msg = new DHUDMessageFadeOut (activefont, work, x, y, hudwidth, hudheight, color, holdTime, fadeTime);
						}
						break;
					case 2:		// type on, then fade out
						{
							float typeTime = (optstart < sp) ? FIXED2FLOAT(Stack[optstart]) : 0.05f;
							float fadeTime = (optstart < sp-1) ? FIXED2FLOAT(Stack[optstart+1]) : 0.5f;

							// [BC] Tell clients to print this message.
							if ( NETWORK_GetState( ) == NETSTATE_SERVER )
							{
								if (( pcd == PCD_ENDHUDMESSAGEBOLD ) || ( screen == NULL ))
									SERVERCOMMANDS_PrintHUDMessageTypeOnFadeOut( work.GetChars( ), x, y, hudwidth, hudheight, color, typeTime, holdTime, fadeTime, SERVER_GetCurrentFont( ), !!( type & HUDMSG_LOG ), id );
								else if ( screen->player )
									SERVERCOMMANDS_PrintHUDMessageTypeOnFadeOut( work.GetChars( ), x, y, hudwidth, hudheight, color, typeTime, holdTime, fadeTime, SERVER_GetCurrentFont( ), !!( type & HUDMSG_LOG ), id, screen->player - players, SVCF_ONLYTHISCLIENT );
							}
							else
								msg = new DHUDMessageTypeOnFadeOut (activefont, work, x, y, hudwidth, hudheight, color, typeTime, holdTime, fadeTime);
						}
						break;
					case 3:		// fade in, then fade out
						{
							float inTime = (optstart < sp) ? FIXED2FLOAT(Stack[optstart]) : 0.5f;
							float outTime = (optstart < sp-1) ? FIXED2FLOAT(Stack[optstart+1]) : 0.5f;

							// [BC] Tell clients to print this message.
							if ( NETWORK_GetState( ) == NETSTATE_SERVER )
							{
								if (( pcd == PCD_ENDHUDMESSAGEBOLD ) || ( screen == NULL ))
									SERVERCOMMANDS_PrintHUDMessageFadeInOut( work.GetChars( ), x, y, hudwidth, hudheight, color, holdTime, inTime, outTime, SERVER_GetCurrentFont( ), !!( type & HUDMSG_LOG ), id );
								else if ( screen->player )
									SERVERCOMMANDS_PrintHUDMessageFadeInOut( work.GetChars( ), x, y, hudwidth, hudheight, color, holdTime, inTime, outTime, SERVER_GetCurrentFont( ), !!( type & HUDMSG_LOG ), id, screen->player - players, SVCF_ONLYTHISCLIENT );
							}
							else
								msg = new DHUDMessageFadeInOut (activefont, work, x, y, hudwidth, hudheight, color, holdTime, inTime, outTime);
						}
						break;
					}
					if ( NETWORK_GetState( ) != NETSTATE_SERVER )
						StatusBar->AttachMessage (msg, id ? 0xff000000|id : 0);
					if (type & HUDMSG_LOG)
					{
						static const char bar[] = TEXTCOLOR_ORANGE "\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36"
					"\36\36\36\36\36\36\36\36\36\36\36\36\37" TEXTCOLOR_NORMAL "\n";
						static const char logbar[] = "\n<------------------------------->\n";
						char consolecolor[3];

						consolecolor[0] = '\x1c';
						consolecolor[1] = color >= CR_BRICK && color <= CR_YELLOW ? color + 'A' : '-';
						consolecolor[2] = '\0';
						AddToConsole (-1, bar);
						AddToConsole (-1, consolecolor);
						AddToConsole (-1, work);
						AddToConsole (-1, bar);
						if (Logfile)
						{
							fputs (logbar, Logfile);
							fputs (work, Logfile);
							fputs (logbar, Logfile);
							fflush (Logfile);
						}
					}
				}
			}
			STRINGBUILDER_FINISH(work);
			sp = optstart-6;
			break;

		case PCD_SETFONT:
			DoSetFont (STACK(1));
			sp--;
			break;

		case PCD_SETFONTDIRECT:
			DoSetFont (pc[0]);
			pc++;
			break;

		case PCD_PLAYERCOUNT:
			PushToStack (CountPlayers ());
			break;

		case PCD_GAMETYPE:
			if (gamestate == GS_TITLELEVEL)
				PushToStack (GAME_TITLE_MAP);
			else if (deathmatch)
				PushToStack (GAME_NET_DEATHMATCH);
			else if ( teamgame )
				PushToStack( GAME_NET_TEAMGAME );
			else if ( NETWORK_GetState( ) != NETSTATE_SINGLE )
				PushToStack (GAME_NET_COOPERATIVE);
			else
				PushToStack (GAME_SINGLE_PLAYER);
			break;

		case PCD_GAMESKILL:
			PushToStack (G_SkillProperty(SKILLP_ACSReturn));
			break;

// There aren't used anymore.
		case PCD_PLAYERBLUESKULL:

			PushToStack( -1 );
			break;
		case PCD_PLAYERREDSKULL:

			PushToStack( -1 );
			break;
		case PCD_PLAYERYELLOWSKULL:

			PushToStack( -1 );
			break;
		case PCD_PLAYERBLUECARD:

			PushToStack( -1 );
			break;
		case PCD_PLAYERREDCARD:

			PushToStack( -1 );
			break;
		case PCD_PLAYERYELLOWCARD:

			PushToStack( -1 );
			break;
		case PCD_ISMULTIPLAYER:
			
			PushToStack(( NETWORK_GetState( ) == NETSTATE_SERVER ) ||
				( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
				( CLIENTDEMO_IsPlaying( )));
			break;
		case PCD_PLAYERTEAM:

			if ( activator && activator->player )
				PushToStack( activator->player->ulTeam );
			else
				PushToStack( 0 );
			break;
		case PCD_PLAYERHEALTH:
			if (activator)
				PushToStack (activator->health);
			else
				PushToStack (0);
			break;

		case PCD_PLAYERARMORPOINTS:
			if (activator)
			{
				ABasicArmor *armor = activator->FindInventory<ABasicArmor>();
				PushToStack (armor ? armor->Amount : 0);
			}
			else
			{
				PushToStack (0);
			}
			break;

		case PCD_PLAYERFRAGS:
			if (activator && activator->player)
				PushToStack (activator->player->fragcount);
			else
				PushToStack (0);
			break;

		case PCD_BLUETEAMCOUNT:
			
			PushToStack( TEAM_CountPlayers( 0 ));
			break;
		case PCD_REDTEAMCOUNT:
			
			PushToStack( TEAM_CountPlayers( 1 ));
			break;
		case PCD_BLUETEAMSCORE:
			
			if ( GAMEMODE_GetFlags(GAMEMODE_GetCurrentMode()) & GMF_PLAYERSEARNFRAGS )
				PushToStack( TEAM_GetFragCount( 0 ));
			else if ( GAMEMODE_GetFlags(GAMEMODE_GetCurrentMode()) & GMF_PLAYERSEARNWINS )
				PushToStack( TEAM_GetWinCount( 0 ));
			else
				PushToStack( TEAM_GetScore( 0 ));
			break;
		case PCD_REDTEAMSCORE:
			
			if ( GAMEMODE_GetFlags(GAMEMODE_GetCurrentMode()) & GMF_PLAYERSEARNFRAGS )
				PushToStack( TEAM_GetFragCount( 1 ));
			else if ( GAMEMODE_GetFlags(GAMEMODE_GetCurrentMode()) & GMF_PLAYERSEARNWINS )
				PushToStack( TEAM_GetWinCount( 1 ));
			else
				PushToStack( TEAM_GetScore( 1 ));
			break;
		case PCD_ISONEFLAGCTF:

			PushToStack( oneflagctf );
			break;
		case PCD_GETINVASIONWAVE:

			if ( invasion == false )
				PushToStack( -1 );
			else
				PushToStack( (LONG)INVASION_GetCurrentWave( ));
			break;
		case PCD_GETINVASIONSTATE:

			if ( invasion == false )
				PushToStack( -1 );
			else
				PushToStack( (LONG)INVASION_GetState( ));
			break;
		case PCD_CONSOLECOMMAND:

			g_bCalledFromConsoleCommand = true;
			if ( FBehavior::StaticLookupString( STACK( 3 )))
				C_DoCommand( FBehavior::StaticLookupString( STACK( 3 )));
			g_bCalledFromConsoleCommand = false;
			sp -= 3;
			break;
		case PCD_CONSOLECOMMANDDIRECT:

			g_bCalledFromConsoleCommand = true;
			if ( FBehavior::StaticLookupString( pc[0] ))
				C_DoCommand( FBehavior::StaticLookupString (pc[0]));
			g_bCalledFromConsoleCommand = false;
			pc += 3;
			break;

		case PCD_MUSICCHANGE:
			lookup = FBehavior::StaticLookupString (STACK(2));
			if (lookup != NULL)
			{
				S_ChangeMusic (lookup, STACK(1));

				// [BC] If we're the server, tell clients to change the music, and
				// save the current music setting for when new clients connect.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				{
					SERVERCOMMANDS_SetMapMusic( (char *)lookup );
					SERVER_SetMapMusic( lookup );
				}
			}
			sp -= 2;
			break;

		case PCD_SINGLEPLAYER:
			PushToStack(( NETWORK_GetState( ) == NETSTATE_SINGLE ));
			break;
// [BC] End ST PCD's

		case PCD_TIMER:
			PushToStack (level.time);
			break;

		case PCD_SECTORSOUND:
			lookup = FBehavior::StaticLookupString (STACK(2));
			if (lookup != NULL)
			{
				if (activationline)
				{
					S_Sound (
						activationline->frontsector,
						CHAN_AUTO,	// Not CHAN_AREA, because that'd probably break existing scripts.
						lookup,
						(float)(STACK(1)) / 127.f,
						ATTN_NORM);

					// [BC] If we're the server, tell clients to play this sound.
					if ( NETWORK_GetState( ) == NETSTATE_SERVER )
						SERVERCOMMANDS_SoundPoint( activationline->frontsector->soundorg[0], activationline->frontsector->soundorg[1], activationline->frontsector->soundorg[2], CHAN_AUTO, (char *)lookup, (float)(STACK(1)) / 127.f, ATTN_NORM );
				}
				else
				{
					S_Sound (
						CHAN_AUTO,
						lookup,
						(float)(STACK(1)) / 127.f,
						ATTN_NORM);

					// [BC] If we're the server, tell clients to play this sound.
					if ( NETWORK_GetState( ) == NETSTATE_SERVER )
						SERVERCOMMANDS_Sound( CHAN_AUTO, (char *)lookup, (float)( STACK( 1 ) / 127.f ), ATTN_NORM );
				}
			}
			sp -= 2;
			break;

		case PCD_AMBIENTSOUND:
			lookup = FBehavior::StaticLookupString (STACK(2));
			if (lookup != NULL)
			{
				S_Sound (CHAN_AUTO,
						 lookup,
						 (float)(STACK(1)) / 127.f, ATTN_NONE);

				// [BC] If we're the server, tell clients to play this sound.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_Sound( CHAN_AUTO, (char *)lookup, (float)( STACK( 1 ) / 127.f ), ATTN_NONE );
			}
			sp -= 2;
			break;

		case PCD_LOCALAMBIENTSOUND:
			// [BB] With Skulltag's in game joining / leaving, it's possible that activator is NULL.
			if ( activator != NULL )
			{
				lookup = FBehavior::StaticLookupString (STACK(2));
				if (lookup != NULL && activator->CheckLocalView (consoleplayer))
				{
					S_Sound (CHAN_AUTO,
						lookup,
						(float)(STACK(1)) / 127.f, ATTN_NONE);
				}

				// [BC] If we're the server, tell clients to play this sound.
				if (( lookup != NULL ) && ( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( activator->player ))
					SERVERCOMMANDS_Sound( CHAN_AUTO, (char *)lookup, (float)( STACK( 1 ) / 127.f ), ATTN_NONE, activator->player - players, SVCF_ONLYTHISCLIENT );
			}

			sp -= 2;
			break;

		case PCD_ACTIVATORSOUND:
			lookup = FBehavior::StaticLookupString (STACK(2));
			if (lookup != NULL)
			{
				if (activator != NULL)
				{
					S_Sound (activator, CHAN_AUTO,
							 lookup,
							 (float)(STACK(1)) / 127.f, ATTN_NORM);

					// [BC] If we're the server, tell clients to play this sound.
					if ( NETWORK_GetState( ) == NETSTATE_SERVER )
						SERVERCOMMANDS_SoundActor( activator, CHAN_AUTO, lookup, (float)STACK( 1 ) / 127.f, ATTN_NORM );
				}
				else
				{
					S_Sound (CHAN_AUTO,
							 lookup,
							 (float)(STACK(1)) / 127.f, ATTN_NONE);

					// [BB] If we're the server, tell clients to play this sound.
					if ( NETWORK_GetState( ) == NETSTATE_SERVER )
						SERVERCOMMANDS_Sound( CHAN_AUTO, lookup, (float)STACK( 1 ) / 127.f, ATTN_NORM );
				}
			}
			sp -= 2;
			break;

		case PCD_SOUNDSEQUENCE:
			lookup = FBehavior::StaticLookupString (STACK(1));
			if (lookup != NULL)
			{
				if (activationline != NULL)
				{
					SN_StartSequence (activationline->frontsector, CHAN_FULLHEIGHT, lookup, 0);

					// [BB] Tell the clients to play the sound.
					if ( NETWORK_GetState( ) == NETSTATE_SERVER )
						SERVERCOMMANDS_StartSectorSequence( activationline->frontsector, CHAN_FULLHEIGHT, lookup, 0 );
				}
			}
			sp--;
			break;

		case PCD_SETLINETEXTURE:
			SetLineTexture (STACK(4), STACK(3), STACK(2), STACK(1));
			sp -= 4;
			break;

		case PCD_REPLACETEXTURES:
			ReplaceTextures (STACK(3), STACK(2), STACK(1));
			sp -= 3;
			break;

		case PCD_SETLINEBLOCKING:
			{
				int line = -1;

				while ((line = P_FindLineFromID (STACK(2), line)) >= 0)
				{
					switch (STACK(1))
					{
					case BLOCK_NOTHING:
						lines[line].flags &= ~(ML_BLOCKING|ML_BLOCKEVERYTHING|ML_RAILING|ML_BLOCK_PLAYERS);
						break;
					case BLOCK_CREATURES:
					default:
						lines[line].flags &= ~(ML_BLOCKEVERYTHING|ML_RAILING|ML_BLOCK_PLAYERS);
						lines[line].flags |= ML_BLOCKING;
						break;
					case BLOCK_EVERYTHING:
						lines[line].flags &= ~(ML_RAILING|ML_BLOCK_PLAYERS);
						lines[line].flags |= ML_BLOCKING|ML_BLOCKEVERYTHING;
						break;
					case BLOCK_RAILING:
						lines[line].flags &= ~(ML_BLOCKEVERYTHING|ML_BLOCK_PLAYERS);
						lines[line].flags |= ML_RAILING|ML_BLOCKING;
						break;
					case BLOCK_PLAYERS:
						lines[line].flags &= ~(ML_BLOCKEVERYTHING|ML_BLOCKING|ML_RAILING);
						lines[line].flags |= ML_BLOCK_PLAYERS;
						break;
					}

					// If we're the server, tell clients to update this line.
					if ( NETWORK_GetState( ) == NETSTATE_SERVER )
						SERVERCOMMANDS_SetSomeLineFlags( line );
				}

				sp -= 2;
			}
			break;

		case PCD_SETLINEMONSTERBLOCKING:
			{
				int line = -1;

				while ((line = P_FindLineFromID (STACK(2), line)) >= 0)
				{
					if (STACK(1))
						lines[line].flags |= ML_BLOCKMONSTERS;
					else
						lines[line].flags &= ~ML_BLOCKMONSTERS;
				}

				sp -= 2;
			}
			break;

		case PCD_SETLINESPECIAL:
			{
				int linenum = -1;

				while ((linenum = P_FindLineFromID (STACK(7), linenum)) >= 0) {
					line_t *line = &lines[linenum];

					line->special = STACK(6);
					line->args[0] = STACK(5);
					line->args[1] = STACK(4);
					line->args[2] = STACK(3);
					line->args[3] = STACK(2);
					line->args[4] = STACK(1);
				}
				sp -= 7;
			}
			break;

		case PCD_SETTHINGSPECIAL:
			{
				if (STACK(7) != 0)
				{
					FActorIterator iterator (STACK(7));
					AActor *actor;

					while ( (actor = iterator.Next ()) )
					{
						actor->special = STACK(6);
						actor->args[0] = STACK(5);
						actor->args[1] = STACK(4);
						actor->args[2] = STACK(3);
						actor->args[3] = STACK(2);
						actor->args[4] = STACK(1);

						// [BB] If we're the server, tell the clients to set the thing specials.
						if ( NETWORK_GetState( ) == NETSTATE_SERVER )
						{
							// [BC] The client doesn't need to know the thing's special (does it?).
//							if( actor->special != 0 )
//								Printf( "Actor special is %d, updating this to the clients is not implemented yet.\n", actor->special );
							SERVERCOMMANDS_SetThingArguments( actor );
						}
					}
				}
				else if (activator != NULL)
				{
					activator->special = STACK(6);
					activator->args[0] = STACK(5);
					activator->args[1] = STACK(4);
					activator->args[2] = STACK(3);
					activator->args[3] = STACK(2);
					activator->args[4] = STACK(1);

					// [BB] If we're the server, tell the clients to set the thing specials.
					if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					{
						// [BC] The client doesn't need to know the thing's special (does it?).
//						if ( activator->special != 0 )
//							Printf( "Activator special is %d, updating this to the clients is not implemented yet.\n", activator->special );
						SERVERCOMMANDS_SetThingArguments( activator );
					}
				}
				sp -= 7;
			}
			break;

		case PCD_THINGSOUND:
			lookup = FBehavior::StaticLookupString (STACK(2));
			if (lookup != NULL)
			{
				FActorIterator iterator (STACK(3));
				AActor *spot;

				while ( (spot = iterator.Next ()) )
				{
					S_Sound (spot, CHAN_AUTO,
							 lookup,
							 (float)(STACK(1))/127.f, ATTN_NORM);

					// If we're the server, tell clients to play this sound.
					if ( NETWORK_GetState( ) == NETSTATE_SERVER )
						SERVERCOMMANDS_SoundActor( spot, CHAN_AUTO, (char *)lookup, (float)STACK( 1 ) / 127.f, ATTN_NORM );
				}
			}
			sp -= 3;
			break;

		case PCD_FIXEDMUL:
			STACK(2) = FixedMul (STACK(2), STACK(1));
			sp--;
			break;

		case PCD_FIXEDDIV:
			STACK(2) = FixedDiv (STACK(2), STACK(1));
			sp--;
			break;

		case PCD_SETGRAVITY:
			level.gravity = (float)STACK(1) / 65536.f;

			// [BB] The level gravity is handled as part of the gamemode limits.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetGameModeLimits( );

			sp--;
			break;

		case PCD_SETGRAVITYDIRECT:
			level.gravity = (float)pc[0] / 65536.f;

			// [BB] The level gravity is handled as part of the gamemode limits.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetGameModeLimits( );

			pc++;
			break;

		case PCD_SETAIRCONTROL:
			level.aircontrol = STACK(1);

			// [BB] The level aircontrol is handled as part of the gamemode limits.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetGameModeLimits( );

			sp--;
			G_AirControlChanged ();
			break;

		case PCD_SETAIRCONTROLDIRECT:
			level.aircontrol = pc[0];

			// [BB] The level aircontrol is handled as part of the gamemode limits.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetGameModeLimits( );

			pc++;
			G_AirControlChanged ();
			break;

		case PCD_SPAWN:
			STACK(6) = DoSpawn (STACK(6), STACK(5), STACK(4), STACK(3), STACK(2), STACK(1), false);
			sp -= 5;
			break;

		case PCD_SPAWNDIRECT:
			PushToStack (DoSpawn (pc[0], pc[1], pc[2], pc[3], pc[4], pc[5], false));
			pc += 6;
			break;

		case PCD_SPAWNSPOT:
			STACK(4) = DoSpawnSpot (STACK(4), STACK(3), STACK(2), STACK(1), false);
			sp -= 3;
			break;

		case PCD_SPAWNSPOTDIRECT:
			PushToStack (DoSpawnSpot (pc[0], pc[1], pc[2], pc[3], false));
			pc += 4;
			break;

		case PCD_SPAWNSPOTFACING:
			STACK(3) = DoSpawnSpotFacing (STACK(3), STACK(2), STACK(1), false);
			sp -= 2;
			break;

		case PCD_CLEARINVENTORY:
			ClearInventory (activator);
			break;

		case PCD_CLEARACTORINVENTORY:
			if (STACK(1) == 0)
			{
				ClearInventory(NULL);
			}
			else
			{
				FActorIterator it(STACK(1));
				AActor *actor;
				for (actor = it.Next(); actor != NULL; actor = it.Next())
				{
					ClearInventory(actor);
				}
			}
			sp--;
			break;

		case PCD_GIVEINVENTORY:
			GiveInventory (activator, FBehavior::StaticLookupString (STACK(2)), STACK(1));
			sp -= 2;
			break;

		case PCD_GIVEACTORINVENTORY:
			{
				const char *type = FBehavior::StaticLookupString(STACK(2));
				if (STACK(3) == 0)
				{
					GiveInventory(NULL, FBehavior::StaticLookupString(STACK(2)), STACK(1));
				}
				else
				{
					FActorIterator it(STACK(3));
					AActor *actor;
					for (actor = it.Next(); actor != NULL; actor = it.Next())
					{
						GiveInventory(actor, type, STACK(1));
					}
				}
				sp -= 3;
			}
			break;

		case PCD_GIVEINVENTORYDIRECT:
			GiveInventory (activator, FBehavior::StaticLookupString (pc[0]), pc[1]);
			pc += 2;
			break;

		case PCD_TAKEINVENTORY:
			TakeInventory (activator, FBehavior::StaticLookupString (STACK(2)), STACK(1));
			sp -= 2;
			break;

		case PCD_TAKEACTORINVENTORY:
			{
				const char *type = FBehavior::StaticLookupString(STACK(2));
				if (STACK(3) == 0)
				{
					TakeInventory(NULL, type, STACK(1));
				}
				else
				{
					FActorIterator it(STACK(3));
					AActor *actor;
					for (actor = it.Next(); actor != NULL; actor = it.Next())
					{
						TakeInventory(actor, type, STACK(1));
					}
				}
				sp -= 3;
			}
			break;

		case PCD_TAKEINVENTORYDIRECT:
			TakeInventory (activator, FBehavior::StaticLookupString (pc[0]), pc[1]);
			pc += 2;
			break;

		case PCD_CHECKINVENTORY:
			STACK(1) = CheckInventory (activator, FBehavior::StaticLookupString (STACK(1)));
			break;

		case PCD_CHECKACTORINVENTORY:
			STACK(2) = CheckInventory (SingleActorFromTID(STACK(2), NULL),
										FBehavior::StaticLookupString (STACK(1)));
			sp--;
			break;

		case PCD_CHECKINVENTORYDIRECT:
			PushToStack (CheckInventory (activator, FBehavior::StaticLookupString (pc[0])));
			pc += 1;
			break;

		case PCD_USEINVENTORY:
			STACK(1) = UseInventory (activator, FBehavior::StaticLookupString (STACK(1)));
			break;

		case PCD_USEACTORINVENTORY:
			{
				int ret = 0;
				const char *type = FBehavior::StaticLookupString(STACK(1));
				if (STACK(2) == 0)
				{
					ret = UseInventory(NULL, type);
				}
				else
				{
					FActorIterator it(STACK(2));
					AActor *actor;
					for (actor = it.Next(); actor != NULL; actor = it.Next())
					{
						ret += UseInventory(actor, type);
					}
				}
				STACK(2) = ret;
				sp--;
			}
			break;

		case PCD_GETSIGILPIECES:
			{
				ASigil *sigil;

				if (activator == NULL || (sigil = activator->FindInventory<ASigil>()) == NULL)
				{
					PushToStack (0);
				}
				else
				{
					PushToStack (sigil->NumPieces);
				}
			}
			break;

		case PCD_GETAMMOCAPACITY:
			if (activator != NULL)
			{
				const PClass *type = PClass::FindClass (FBehavior::StaticLookupString (STACK(1)));
				AInventory *item;

				if (type != NULL && type->ParentClass == RUNTIME_CLASS(AAmmo))
				{
					item = activator->FindInventory (type);
					if (item != NULL)
					{
						STACK(1) = item->MaxAmount;
					}
					else
					{
						STACK(1) = ((AInventory *)GetDefaultByType (type))->MaxAmount;
					}
				}
				else
				{
					STACK(1) = 0;
				}
			}
			else
			{
				STACK(1) = 0;
			}
			break;

		case PCD_SETAMMOCAPACITY:
			if (activator != NULL)
			{
				const PClass *type = PClass::FindClass (FBehavior::StaticLookupString (STACK(2)));
				AInventory *item;

				if (type != NULL && type->ParentClass == RUNTIME_CLASS(AAmmo))
				{
					item = activator->FindInventory (type);
					if (item != NULL)
					{
						item->MaxAmount = STACK(1);
					}
					else
					{
						item = activator->GiveInventoryType (type);
						item->MaxAmount = STACK(1);
						item->Amount = 0;
					}
					// [BB] If the activator is a player, tell the clients about the changed capacity.
					if ( activator->player && NETWORK_GetState() == NETSTATE_SERVER )
						SERVERCOMMANDS_SetPlayerAmmoCapacity( activator->player - players, item );
				}
			}
			sp -= 2;
			break;

		case PCD_SETMUSIC:

			// [BC] Tell clients about this music change, and save the music setting for when
			// new clients connect.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			{
				SERVERCOMMANDS_SetMapMusic( (char *)FBehavior::StaticLookupString( STACK( 3 )));
				SERVER_SetMapMusic( (char *)FBehavior::StaticLookupString( STACK( 3 )));
			}

			S_ChangeMusic (FBehavior::StaticLookupString (STACK(3)), STACK(2));
			sp -= 3;
			break;

		case PCD_SETMUSICDIRECT:

			// [BC] Tell clients about this music change.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			{
				SERVERCOMMANDS_SetMapMusic( (char *)FBehavior::StaticLookupString( pc[0] ));
				SERVER_SetMapMusic( (char *)FBehavior::StaticLookupString( pc[0] ));
			}

			S_ChangeMusic (FBehavior::StaticLookupString (pc[0]), pc[1]);
			pc += 3;
			break;

		case PCD_LOCALSETMUSIC:

			// [BC] Tell clients about this music change.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			{
				if ( activator && activator->player )
					SERVERCOMMANDS_SetMapMusic( (char *)FBehavior::StaticLookupString( STACK( 3 )), activator->player - players, SVCF_ONLYTHISCLIENT );
			}

			if (activator == players[consoleplayer].mo)
			{
				S_ChangeMusic (FBehavior::StaticLookupString (STACK(3)), STACK(2));
			}
			sp -= 3;
			break;

		case PCD_LOCALSETMUSICDIRECT:

			// Tell clients about this music change.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			{
				if ( activator && activator->player )
					SERVERCOMMANDS_SetMapMusic( (char *)FBehavior::StaticLookupString( pc[0] ), activator->player - players, SVCF_ONLYTHISCLIENT );
			}

			if (activator == players[consoleplayer].mo)
			{
				S_ChangeMusic (FBehavior::StaticLookupString (pc[0]), pc[1]);
			}
			pc += 3;
			break;

		case PCD_FADETO:
			DoFadeTo (STACK(5), STACK(4), STACK(3), STACK(2), STACK(1));
			sp -= 5;
			break;

		case PCD_FADERANGE:
			DoFadeRange (STACK(9), STACK(8), STACK(7), STACK(6),
						 STACK(5), STACK(4), STACK(3), STACK(2), STACK(1));
			sp -= 9;
			break;

		case PCD_CANCELFADE:
			{
				// [BB] Tell the clients to cancel the fade.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				{
					// [BB] Encode "activator == NULL" as MAXPLAYERS.
					const ULONG ulPlayer = ( activator && activator->player ) ? static_cast<ULONG> ( activator->player - players ) : MAXPLAYERS;
					SERVERCOMMANDS_CancelFade ( ulPlayer );
				}

				TThinkerIterator<DFlashFader> iterator;
				DFlashFader *fader;

				while ( (fader = iterator.Next()) )
				{
					if (activator == NULL || fader->WhoFor() == activator)
					{
						fader->Cancel ();
					}
				}
			}
			break;

		case PCD_PLAYMOVIE:
			STACK(1) = I_PlayMovie (FBehavior::StaticLookupString (STACK(1)));
			break;

		case PCD_SETACTORPOSITION:
			{
				bool result = false;
				AActor *actor = SingleActorFromTID (STACK(5), activator);
				if (actor != NULL)
					result = P_MoveThing(actor, STACK(4), STACK(3), STACK(2), !!STACK(1));
				sp -= 4;
				STACK(1) = result;
			}
			break;

		case PCD_GETACTORX:
		case PCD_GETACTORY:
		case PCD_GETACTORZ:
			{
				AActor *actor = SingleActorFromTID (STACK(1), activator);

				if (actor == NULL)
				{
					STACK(1) = 0;
				}
				else
				{
					STACK(1) = (&actor->x)[pcd - PCD_GETACTORX];
				}
			}
			break;

		case PCD_GETACTORFLOORZ:
		case PCD_GETACTORCEILINGZ:
			{
				AActor *actor = SingleActorFromTID (STACK(1), activator);

				if (actor == NULL)
				{
					STACK(1) = 0;
				}
				else if (pcd == PCD_GETACTORFLOORZ)
				{
					STACK(1) = actor->floorz;
				}
				else
				{
					STACK(1) = actor->ceilingz;
				}

			}
			break;

		case PCD_GETACTORANGLE:
			{
				AActor *actor = SingleActorFromTID (STACK(1), activator);

				if (actor == NULL)
				{
					STACK(1) = 0;
				}
				else
				{
					STACK(1) = actor->angle >> 16;
				}
			}
			break;

		case PCD_GETACTORPITCH:
			{
				AActor *actor = SingleActorFromTID (STACK(1), activator);

				if (actor == NULL)
				{
					STACK(1) = 0;
				}
				else
				{
					STACK(1) = actor->pitch >> 16;
				}
			}
			break;

		case PCD_GETLINEROWOFFSET:
			if (activationline)
			{
				PushToStack (activationline->sidedef[0]->GetTextureYOffset(side_t::mid) >> FRACBITS);
			}
			else
			{
				PushToStack (0);
			}
			break;

		case PCD_GETSECTORFLOORZ:
		case PCD_GETSECTORCEILINGZ:
			// Arguments are (tag, x, y). If you don't use slopes, then (x, y) don't
			// really matter and can be left as (0, 0) if you like.
			{
				int secnum = P_FindSectorFromTag (STACK(3), -1);
				fixed_t z = 0;

				if (secnum >= 0)
				{
					fixed_t x = STACK(2) << FRACBITS;
					fixed_t y = STACK(1) << FRACBITS;
					if (pcd == PCD_GETSECTORFLOORZ)
					{
						z = sectors[secnum].floorplane.ZatPoint (x, y);
					}
					else
					{
						z = sectors[secnum].ceilingplane.ZatPoint (x, y);
					}
				}
				sp -= 2;
				STACK(1) = z;
			}
			break;

		case PCD_GETSECTORLIGHTLEVEL:
			{
				int secnum = P_FindSectorFromTag (STACK(1), -1);
				int z = -1;

				if (secnum >= 0)
				{
					z = sectors[secnum].lightlevel;
				}
				STACK(1) = z;
			}
			break;

		case PCD_SETFLOORTRIGGER:
			new DPlaneWatcher (activator, activationline, backSide, false, STACK(8),
				STACK(7), STACK(6), STACK(5), STACK(4), STACK(3), STACK(2), STACK(1));
			sp -= 8;
			break;

		case PCD_SETCEILINGTRIGGER:
			new DPlaneWatcher (activator, activationline, backSide, true, STACK(8),
				STACK(7), STACK(6), STACK(5), STACK(4), STACK(3), STACK(2), STACK(1));
			sp -= 8;
			break;

		case PCD_STARTTRANSLATION:
			{
				int i = STACK(1);
				sp--;
				if (i >= 1 && i <= MAX_ACS_TRANSLATIONS)
				{
					translation = translationtables[TRANSLATION_LevelScripted].GetVal(i - 1);
					if (translation == NULL)
					{
						translation = new FRemapTable;
						translationtables[TRANSLATION_LevelScripted].SetVal(i - 1, translation);
					}
					translation->MakeIdentity();

					if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					{
						const int translationindex = ACS_GetTranslationIndex( translation );
						SERVER_RemoveEditedTranslation ( translationindex );
					}
				}
			}
			break;

		case PCD_TRANSLATIONRANGE1:
			{ // translation using palette shifting
				int start = STACK(4);
				int end = STACK(3);
				int pal1 = STACK(2);
				int pal2 = STACK(1);
				sp -= 4;

				if (translation != NULL)
					translation->AddIndexRange(start, end, pal1, pal2);

				// [BC] If we're the server, send the new translation off to clients, and
				// store it in our list so we can tell new clients who connect about the
				// translation.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				{
					// [BB] Obtain the index of the translation.
					const int translationindex = ACS_GetTranslationIndex( translation );
					SERVERCOMMANDS_CreateTranslation(translationindex, start, end, pal1, pal2 );
					SERVER_AddEditedTranslation(translationindex, start, end, pal1, pal2 );
				}
			}
			break;

		case PCD_TRANSLATIONRANGE2:
			{ // translation using RGB values
			  // (would HSV be a good idea too?)
				int start = STACK(8);
				int end = STACK(7);
				int r1 = STACK(6);
				int g1 = STACK(5);
				int b1 = STACK(4);
				int r2 = STACK(3);
				int g2 = STACK(2);
				int b2 = STACK(1);
				sp -= 8;

				if (translation != NULL)
					translation->AddColorRange(start, end, r1, g1, b1, r2, g2, b2);

				// [BC] If we're the server, send the new translation off to clients, and
				// store it in our list so we can tell new clients who connect about the
				// translation.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				{
					// [BB] Obtain the index of the translation.
					const int translationindex = ACS_GetTranslationIndex( translation );
					SERVERCOMMANDS_CreateTranslation( translationindex, start, end, r1, g1, b1, r2, g2, b2 );
					SERVER_AddEditedTranslation( translationindex, start, end, r1, g1, b1, r2, g2, b2 );
				}
			}
			break;

		case PCD_ENDTRANSLATION:
			// This might be useful for hardware rendering, but
			// for software it is superfluous.
			translation->UpdateNative();
			translation = NULL;
			break;

		case PCD_SIN:
			STACK(1) = finesine[angle_t(STACK(1)<<16)>>ANGLETOFINESHIFT];
			break;

		case PCD_COS:
			STACK(1) = finecosine[angle_t(STACK(1)<<16)>>ANGLETOFINESHIFT];
			break;

		case PCD_VECTORANGLE:
			STACK(2) = R_PointToAngle2 (0, 0, STACK(2), STACK(1)) >> 16;
			sp--;
			break;

		case PCD_CHECKWEAPON:
			// [BB] Workaround to let CheckWeaponn return something reasonable even before the client selected the starting weapon.
			if ( ( NETWORK_GetState( ) == NETSTATE_SERVER ) && activator && activator->player && ( activator->player->bClientSelectedWeapon == false )
				&& ( activator->player->ReadyWeapon == NULL ) && ( activator->player->PendingWeapon == WP_NOCHANGE ) )
			{
				STACK(1) = ( 0 == stricmp (FBehavior::StaticLookupString (STACK(1)), activator->player->StartingWeaponName.GetChars() ) );
			}
			else if (activator == NULL || activator->player == NULL || // Non-players do not have weapons
				activator->player->ReadyWeapon == NULL)
			{
				STACK(1) = 0;
			}
			else
			{
				STACK(1) = 0 == stricmp (FBehavior::StaticLookupString (STACK(1)),
					activator->player->ReadyWeapon->GetClass()->TypeName.GetChars());
			}
			break;

		case PCD_SETWEAPON:
			if (activator == NULL || activator->player == NULL)
			{
				STACK(1) = 0;
			}
			else
			{
				AInventory *item = activator->FindInventory (PClass::FindClass (
					FBehavior::StaticLookupString (STACK(1))));

				if (item == NULL || !item->IsKindOf (RUNTIME_CLASS(AWeapon)))
				{
					STACK(1) = 0;
				}
				else if (activator->player->ReadyWeapon == item)
				{
					// The weapon is already selected, so setweapon succeeds by default,
					// but make sure the player isn't switching away from it.
					activator->player->PendingWeapon = WP_NOCHANGE;
					STACK(1) = 1;
				}
				else
				{
					AWeapon *weap = static_cast<AWeapon *> (item);

					if (weap->CheckAmmo (AWeapon::EitherFire, false))
					{
						// There's enough ammo, so switch to it.
						STACK(1) = 1;
						activator->player->PendingWeapon = weap;

						// [BC] If we're the server, tell the client to change his weapon.
						if ( NETWORK_GetState( ) == NETSTATE_SERVER )
							SERVERCOMMANDS_SetPlayerPendingWeapon( ULONG( activator->player - players ));
					}
					else
					{
						STACK(1) = 0;
					}
				}
			}
			break;

		case PCD_SETMARINEWEAPON:
			if (STACK(2) != 0)
			{
				AScriptedMarine *marine;
				TActorIterator<AScriptedMarine> iterator (STACK(2));

				while ((marine = iterator.Next()) != NULL)
				{
					marine->SetWeapon ((AScriptedMarine::EMarineWeapon)STACK(1));
				}
			}
			else
			{
				if (activator != NULL && activator->IsKindOf (RUNTIME_CLASS(AScriptedMarine)))
				{
					barrier_cast<AScriptedMarine *>(activator)->SetWeapon (
						(AScriptedMarine::EMarineWeapon)STACK(1));
				}
			}
			sp -= 2;
			break;

		case PCD_SETMARINESPRITE:
			{
				const PClass *type = PClass::FindClass (FBehavior::StaticLookupString (STACK(1)));

				if (type != NULL)
				{
					if (STACK(2) != 0)
					{
						AScriptedMarine *marine;
						TActorIterator<AScriptedMarine> iterator (STACK(2));

						while ((marine = iterator.Next()) != NULL)
						{
							marine->SetSprite (type);
						}
					}
					else
					{
						if (activator != NULL && activator->IsKindOf (RUNTIME_CLASS(AScriptedMarine)))
						{
							barrier_cast<AScriptedMarine *>(activator)->SetSprite (type);
						}
					}
				}
				else
				{
					Printf ("Unknown actor type: %s\n", FBehavior::StaticLookupString (STACK(1)));
				}
			}
			sp -= 2;
			break;

		case PCD_SETACTORPROPERTY:
			SetActorProperty (STACK(3), STACK(2), STACK(1));
			sp -= 3;
			break;

		case PCD_GETACTORPROPERTY:
			STACK(2) = GetActorProperty (STACK(2), STACK(1));
			sp -= 1;
			break;

		case PCD_GETPLAYERINPUT:
			STACK(2) = GetPlayerInput (STACK(2), STACK(1));
			sp -= 1;
			break;

		case PCD_PLAYERNUMBER:
			if (activator == NULL || activator->player == NULL)
			{
				PushToStack (-1);
			}
			else
			{
				PushToStack (int(activator->player - players));
			}
			break;

		case PCD_PLAYERINGAME:
			if (STACK(1) < 0 || STACK(1) > MAXPLAYERS)
			{
				STACK(1) = false;
			}
			else
			{
				// [BB] Skulltag doesn't count spectators as players.
				STACK(1) = playeringame[STACK(1)] && ( players[STACK(1)].bSpectating == false );
			}
			break;

		case PCD_PLAYERISBOT:
			if (STACK(1) < 0 || STACK(1) > MAXPLAYERS || !playeringame[STACK(1)])
			{
				STACK(1) = false;
			}
			else
			{
				STACK(1) = players[STACK(1)].bIsBot;
			}
			break;

		case PCD_ACTIVATORTID:
			if (activator == NULL)
			{
				PushToStack (0);
			}
			else
			{
				PushToStack (activator->tid);
			}
			break;

		case PCD_GETSCREENWIDTH:
			// [BC] The server doesn't have a screen.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				PushToStack( 0 );
			else
				PushToStack (SCREENWIDTH);
			break;

		case PCD_GETSCREENHEIGHT:
			// [BC] The server doesn't have a screen.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				PushToStack( 0 );
			else
				PushToStack (SCREENHEIGHT);
			break;

		case PCD_THING_PROJECTILE2:
			// Like Thing_Projectile(Gravity) specials, but you can give the
			// projectile a TID.
			// Thing_Projectile2 (tid, type, angle, speed, vspeed, gravity, newtid);
			P_Thing_Projectile (STACK(7), activator, STACK(6), NULL, ((angle_t)(STACK(5)<<24)),
				STACK(4)<<(FRACBITS-3), STACK(3)<<(FRACBITS-3), 0, NULL, STACK(2), STACK(1), false);
			sp -= 7;
			break;

		case PCD_SPAWNPROJECTILE:
			// Same, but takes an actor name instead of a spawn ID.
			P_Thing_Projectile (STACK(7), activator, 0, FBehavior::StaticLookupString (STACK(6)), ((angle_t)(STACK(5)<<24)),
				STACK(4)<<(FRACBITS-3), STACK(3)<<(FRACBITS-3), 0, NULL, STACK(2), STACK(1), false);
			sp -= 7;
			break;

		case PCD_STRLEN:
			STACK(1) = SDWORD(strlen(FBehavior::StaticLookupString (STACK(1))));
			break;

		case PCD_GETCVAR:
			{
				FBaseCVar *cvar = FindCVar (FBehavior::StaticLookupString (STACK(1)), NULL);
				if (cvar == NULL)
				{
					STACK(1) = 0;
				}
				else
				{
					UCVarValue val = cvar->GetGenericRep (CVAR_Int);
					STACK(1) = val.Int;
				}
			}
			break;

		case PCD_SETHUDSIZE:
			hudwidth = abs (STACK(3));
			hudheight = abs (STACK(2));
			if (STACK(1) != 0)
			{ // Negative height means to cover the status bar
				hudheight = -hudheight;
			}
			sp -= 3;
			break;

		case PCD_GETLEVELINFO:
			switch (STACK(1))
			{
			case LEVELINFO_PAR_TIME:		STACK(1) = level.partime;			break;
			case LEVELINFO_SUCK_TIME:		STACK(1) = level.sucktime;			break;
			case LEVELINFO_CLUSTERNUM:		STACK(1) = level.cluster;			break;
			case LEVELINFO_LEVELNUM:		STACK(1) = level.levelnum;			break;
			case LEVELINFO_TOTAL_SECRETS:	STACK(1) = level.total_secrets;		break;
			case LEVELINFO_FOUND_SECRETS:	STACK(1) = level.found_secrets;		break;
			case LEVELINFO_TOTAL_ITEMS:		STACK(1) = level.total_items;		break;
			case LEVELINFO_FOUND_ITEMS:		STACK(1) = level.found_items;		break;
			case LEVELINFO_TOTAL_MONSTERS:	STACK(1) = level.total_monsters;	break;
			case LEVELINFO_KILLED_MONSTERS:	STACK(1) = level.killed_monsters;	break;
			default:						STACK(1) = 0;						break;
			}
			break;

		case PCD_CHANGESKY:
			{
				const char *sky1name, *sky2name;

				sky1name = FBehavior::StaticLookupString (STACK(2));
				sky2name = FBehavior::StaticLookupString (STACK(1));
				if (sky1name[0] != 0)
				{
					strncpy (level.skypic1, sky1name, 8);
					sky1texture = TexMan.GetTexture (sky1name, FTexture::TEX_Wall, FTextureManager::TEXMAN_Overridable|FTextureManager::TEXMAN_ReturnFirst);
				}
				if (sky2name[0] != 0)
				{
					strncpy (level.skypic2, sky2name, 8);
					sky2texture = TexMan.GetTexture (sky2name, FTexture::TEX_Wall, FTextureManager::TEXMAN_Overridable|FTextureManager::TEXMAN_ReturnFirst);
				}
				R_InitSkyMap ();
				sp -= 2;

				// [BC] If we're the server, tell clients to update their sky.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_SetMapSky( );
			}
			break;

		case PCD_SETCAMERATOTEXTURE:
			{
				const char *picname = FBehavior::StaticLookupString (STACK(2));
				AActor *camera;

				if (STACK(3) == 0)
				{
					camera = activator;
				}
				else
				{
					FActorIterator it (STACK(3));
					camera = it.Next ();
				}

				if (camera != NULL)
				{
					FTextureID picnum = TexMan.CheckForTexture (picname, FTexture::TEX_Wall, FTextureManager::TEXMAN_Overridable);
					if (!picnum.Exists())
					{
						Printf ("SetCameraToTexture: %s is not a texture\n", picname);
					}
					else
					{
						FCanvasTextureInfo::Add (camera, picnum, STACK(1));

						// [BC] If we're the server, tell clients to set this camera to this
						// texture.
						if ( NETWORK_GetState( ) == NETSTATE_SERVER )
							SERVERCOMMANDS_SetCameraToTexture( camera, (char *)picname, STACK( 1 ));
					}
				}
				sp -= 3;
			}
			break;

		case PCD_SETACTORANGLE:		// [GRB]
			if (STACK(2) == 0)
			{
				if (activator != NULL)
					activator->angle = STACK(1) << 16;
				// [BB] Tell the clients about the changed angle.
				if( NETWORK_GetState() == NETSTATE_SERVER )
					SERVERCOMMANDS_SetThingAngleExact( activator );
			}
			else
			{
				FActorIterator iterator (STACK(2));
				AActor *actor;

				while ( (actor = iterator.Next ()) )
				{
					actor->angle = STACK(1) << 16;
					// [BB] Tell the clients about the changed angle.
					// This fixes the "rave room" in SPACEDM5.wad.
					if( NETWORK_GetState() == NETSTATE_SERVER )
						SERVERCOMMANDS_SetThingAngleExact( actor );
				}
			}
			sp -= 2;
			break;

		case PCD_SETACTORPITCH:
			if (STACK(2) == 0)
			{
				if (activator != NULL)
					activator->pitch = STACK(1) << 16;

				// [BB] Tell the clients about the changed pitch.
				if( NETWORK_GetState() == NETSTATE_SERVER )
					SERVERCOMMANDS_MoveThingExact( activator, CM_PITCH );
			}
			else
			{
				FActorIterator iterator (STACK(2));
				AActor *actor;

				while ( (actor = iterator.Next ()) )
				{
					actor->pitch = STACK(1) << 16;

					// [BB] Tell the clients about the changed pitch.
					if( NETWORK_GetState() == NETSTATE_SERVER )
						SERVERCOMMANDS_MoveThingExact( actor, CM_PITCH );
				}
			}
			sp -= 2;
			break;

		case PCD_SETACTORSTATE:
			{
				const char *statename = FBehavior::StaticLookupString (STACK(2));
				FState *state;

				if (STACK(3) == 0)
				{
					if (activator != NULL)
					{
						state = activator->GetClass()->ActorInfo->FindStateByString (statename, !!STACK(1));
						if (state != NULL)
						{
						// [BC] Tell clients to change this thing's state.
						if ( NETWORK_GetState( ) == NETSTATE_SERVER )
							SERVERCOMMANDS_SetThingFrame( activator, state );

							activator->SetState (state);
							STACK(3) = 1;
						}
						else
						{
							STACK(3) = 0;
						}
					}
				}
				else
				{
					FActorIterator iterator (STACK(3));
					AActor *actor;
					int count = 0;

					while ( (actor = iterator.Next ()) )
					{
						state = actor->GetClass()->ActorInfo->FindStateByString (statename, !!STACK(1));
						if (state != NULL)
						{
							// [BC] Tell clients to change this thing's state.
							if ( NETWORK_GetState( ) == NETSTATE_SERVER )
								SERVERCOMMANDS_SetThingFrame( actor, state );

							actor->SetState (state);
							count++;
						}
					}
					STACK(3) = count;
				}
				sp -= 2;
			}
			break;

		case PCD_PLAYERCLASS:		// [GRB]
			if (STACK(1) < 0 || STACK(1) >= MAXPLAYERS || !playeringame[STACK(1)])
			{
				STACK(1) = -1;
			}
			else
			{
				STACK(1) = players[STACK(1)].CurrentPlayerClass;
			}
			break;

		case PCD_GETPLAYERINFO:		// [GRB]
			if (STACK(2) < 0 || STACK(2) >= MAXPLAYERS || !playeringame[STACK(2)])
			{
				STACK(2) = -1;
			}
			else
			{
				userinfo_t *userinfo = &players[STACK(2)].userinfo;
				switch (STACK(1))
				{
				// [CW] PLAYERINFO_TEAM needs to use ulTeam rather than the one in userinfo_t.
				case PLAYERINFO_TEAM:			STACK(2) = players[STACK( 2 )].ulTeam; break;
				case PLAYERINFO_AIMDIST:		STACK(2) = userinfo->GetAimDist(); break;
				case PLAYERINFO_COLOR:			STACK(2) = userinfo->color; break;
				case PLAYERINFO_GENDER:			STACK(2) = userinfo->gender; break;
				case PLAYERINFO_NEVERSWITCH:	STACK(2) = userinfo->switchonpickup; break;
				case PLAYERINFO_MOVEBOB:		STACK(2) = userinfo->MoveBob; break;
				case PLAYERINFO_STILLBOB:		STACK(2) = userinfo->StillBob; break;
				case PLAYERINFO_PLAYERCLASS:	STACK(2) = userinfo->PlayerClass; break;
				default:						STACK(2) = 0; break;
				}
			}
			sp -= 1;
			break;

		case PCD_CHANGELEVEL:
			{
				G_ChangeLevel(FBehavior::StaticLookupString(STACK(4)), STACK(3), STACK(2), STACK(1));
				sp -= 4;
			}
			break;

		case PCD_SECTORDAMAGE:
			{
				int tag = STACK(5);
				int amount = STACK(4);
				FName type = FBehavior::StaticLookupString(STACK(3));
				FName protection = FName (FBehavior::StaticLookupString(STACK(2)), true);
				const PClass *protectClass = PClass::FindClass (protection);
				int flags = STACK(1);
				sp -= 5;

				int secnum = -1;

				while ((secnum = P_FindSectorFromTag (tag, secnum)) >= 0)
				{
					AActor *actor, *next;
					sector_t *sec = &sectors[secnum];

					for (actor = sec->thinglist; actor != NULL; actor = next)
					{
						next = actor->snext;

						if (!(actor->flags & MF_SHOOTABLE))
							continue;

						if (!(flags & DAMAGE_NONPLAYERS) && actor->player == NULL)
							continue;

						if (!(flags & DAMAGE_PLAYERS) && actor->player != NULL)
							continue;

						if (!(flags & DAMAGE_IN_AIR) && actor->z != sec->floorplane.ZatPoint (actor->x, actor->y) && !actor->waterlevel)
							continue;

						if (protectClass != NULL)
						{
							if (!(flags & DAMAGE_SUBCLASSES_PROTECT))
							{
								if (actor->FindInventory (protectClass))
									continue;
							}
							else
							{
								AInventory *item;

								for (item = actor->Inventory; item != NULL; item = item->Inventory)
								{
									if (item->IsKindOf (protectClass))
										break;
								}
								if (item != NULL)
									continue;
							}
						}

						P_DamageMobj (actor, NULL, NULL, amount, type);
					}
				}
			}
			break;

		case PCD_THINGDAMAGE2:
			STACK(3) = P_Thing_Damage (STACK(3), activator, STACK(2), FName(FBehavior::StaticLookupString(STACK(1))));
			sp -= 2;
			break;

		case PCD_CHECKACTORCEILINGTEXTURE:
			STACK(2) = DoCheckActorTexture(STACK(2), activator, STACK(1), false);
			sp--;
			break;

		case PCD_CHECKACTORFLOORTEXTURE:
			STACK(2) = DoCheckActorTexture(STACK(2), activator, STACK(1), true);
			sp--;
			break;

		case PCD_GETACTORLIGHTLEVEL:
		{
			AActor *actor = SingleActorFromTID(STACK(1), activator);
			if (actor != NULL)
			{
				STACK(1) = actor->Sector->lightlevel;
			}
			else STACK(1) = 0;
			break;
		}

		case PCD_SETMUGSHOTSTATE:
			StatusBar->SetMugShotState(FBehavior::StaticLookupString(STACK(1)));
			sp--;
			break;

		case PCD_CHECKPLAYERCAMERA:
			{
				int playernum = STACK(1);

				if (playernum < 0 || playernum >= MAXPLAYERS || !playeringame[playernum] || players[playernum].camera == NULL)
				{
					STACK(1) = -1;
				}
				else
				{
					STACK(1) = players[playernum].camera->tid;
				}
			}
			break;

		case PCD_CLASSIFYACTOR:
			STACK(1) = DoClassifyActor(STACK(1));
			break;

		case PCD_MORPHACTOR:
			{
				int tag = STACK(7);
				FName playerclass_name = FBehavior::StaticLookupString(STACK(6));
				const PClass *playerclass = PClass::FindClass (playerclass_name);
				FName monsterclass_name = FBehavior::StaticLookupString(STACK(5));
				const PClass *monsterclass = PClass::FindClass (monsterclass_name);
				int duration = STACK(4);
				int style = STACK(3);
				FName morphflash_name = FBehavior::StaticLookupString(STACK(2));
				const PClass *morphflash = PClass::FindClass (morphflash_name);
				FName unmorphflash_name = FBehavior::StaticLookupString(STACK(1));
				const PClass *unmorphflash = PClass::FindClass (unmorphflash_name);
				int changes = 0;

				if (tag == 0)
				{
					if (activator != NULL && activator->player)
					{
						changes += P_MorphPlayer(activator->player, activator->player, playerclass, duration, style, morphflash, unmorphflash);
					}
					else
					{
						changes += P_MorphMonster(activator, monsterclass, duration, style, morphflash, unmorphflash);
					}
				}
				else
				{
					FActorIterator iterator (tag);
					AActor *actor;

					while ( (actor = iterator.Next ()) )
					{
						if (actor->player)
						{
							changes += P_MorphPlayer(activator == NULL ? NULL : activator->player,
								actor->player, playerclass, duration, style, morphflash, unmorphflash);
						}
						else
						{
							changes += P_MorphMonster(actor, monsterclass, duration, style, morphflash, unmorphflash);
						}
					}
				}

				STACK(7) = changes;
				sp -= 6;
			}	
			break;

		case PCD_UNMORPHACTOR:
			{
				int tag = STACK(2);
				bool force = !!STACK(1);
				int changes = 0;

				// [BB] Added activator check.
				if ( (tag == 0) && (activator != NULL) )
				{
					if (activator->player)
					{
						if (P_UndoPlayerMorph(activator->player, activator->player, force))
						{
							changes++;
						}
					}
					else
					{
						if (activator->GetClass()->IsDescendantOf(RUNTIME_CLASS(AMorphedMonster)))
						{
							AMorphedMonster *morphed_actor = barrier_cast<AMorphedMonster *>(activator);
							if (P_UndoMonsterMorph(morphed_actor, force))
							{
								changes++;
							}
						}
					}
				}
				else
				{
					FActorIterator iterator (tag);
					AActor *actor;

					while ( (actor = iterator.Next ()) )
					{
						if (actor->player)
						{
							if (P_UndoPlayerMorph(activator->player, actor->player, force))
							{
								changes++;
							}
						}
						else
						{
							if (actor->GetClass()->IsDescendantOf(RUNTIME_CLASS(AMorphedMonster)))
							{
								AMorphedMonster *morphed_actor = static_cast<AMorphedMonster *>(actor);
								if (P_UndoMonsterMorph(morphed_actor, force))
								{
									changes++;
								}
							}
						}
					}
				}

				STACK(2) = changes;
				sp -= 1;
			}	
			break;

		case PCD_SAVESTRING:
			// Saves the string
			{
				unsigned int str_otf = ACS_StringsOnTheFly.Push(strbin1(work));
				if (str_otf > 0xffff)
				{
					PushToStack(-1);
				}
				else
				{
					PushToStack((SDWORD)str_otf|ACSSTRING_OR_ONTHEFLY);
				}
				STRINGBUILDER_FINISH(work);
			}		
			break;

		// [CW] Begin team additions.
		case PCD_GETTEAMPLAYERCOUNT:
			STACK( 1 ) = TEAM_CountPlayers( STACK( 1 ));
			sp--;
			break;
		// [CW] End team additions.
 		}
 	}

	if (state == SCRIPT_DivideBy0)
	{
		Printf ("Divide by zero in script %d\n", script);
		state = SCRIPT_PleaseRemove;
	}
	else if (state == SCRIPT_ModulusBy0)
	{
		Printf ("Modulus by zero in script %d\n", script);
		state = SCRIPT_PleaseRemove;
	}
	if (state == SCRIPT_PleaseRemove)
	{
		Unlink ();
		if (controller->RunningScripts[script] == this)
			controller->RunningScripts[script] = NULL;
	}
	else
	{
		this->pc = pc;
		assert (sp == 0);
	}

	// [BC] Since the server doesn't have a screen, we have to save the active font some
	// other way.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVER_SetCurrentFont( "SmallFont" );

	// [BB] Stop the net traffic measurement and add the result to this script's traffic.
	NETTRAFFIC_AddACSScriptTraffic ( script, NETWORK_StopTrafficMeasurement ( ) );

	return resultValue;
}

#undef PushtoStack

static DLevelScript *P_GetScriptGoing (AActor *who, line_t *where, int num, const ScriptPtr *code, FBehavior *module,
	bool backSide, int arg0, int arg1, int arg2, int always)
{
	DACSThinker *controller = DACSThinker::ActiveThinker;

	if (controller && !always && controller->RunningScripts[num])
	{
		if (controller->RunningScripts[num]->GetState () == DLevelScript::SCRIPT_Suspended)
		{
			controller->RunningScripts[num]->SetState (DLevelScript::SCRIPT_Running);
			return controller->RunningScripts[num];
		}
		return NULL;
	}

	return new DLevelScript (who, where, num, code, module, backSide, arg0, arg1, arg2, always);
}

DLevelScript::DLevelScript (AActor *who, line_t *where, int num, const ScriptPtr *code, FBehavior *module,
							bool backside, int arg0, int arg1, int arg2, int always)
	: activeBehavior (module)
{
	if (DACSThinker::ActiveThinker == NULL)
		new DACSThinker;

	script = num;
	assert(code->VarCount >= code->ArgCount);
	numlocalvars = code->VarCount;
	localvars = new SDWORD[code->VarCount];
	if (code->VarCount > 0)
	{
		localvars[0] = arg0;
		if (code->VarCount > 1)
		{
			localvars[1] = arg1;
			if (code->VarCount > 2)
			{
				localvars[2] = arg2;
			}
		}
	}
	memset (localvars+code->ArgCount, 0, (code->VarCount-code->ArgCount)*sizeof(SDWORD));
	pc = module->GetScriptAddress (code);
	activator = who;
	activationline = where;
	backSide = backside;
	activefont = SmallFont;

	// [BC] Since the server doesn't have a screen, we have to save the active font some
	// other way.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVER_SetScriptActiveFont( "SmallFont" );

	hudwidth = hudheight = 0;
	state = SCRIPT_Running;
	// Hexen waited one second before executing any open scripts. I didn't realize
	// this when I wrote my ACS implementation. Now that I know, it's still best to
	// run them right away because there are several map properties that can't be
	// set in an editor. If an open script sets them, it looks dumb if a second
	// goes by while they're in their default state.

	if (!always)
		DACSThinker::ActiveThinker->RunningScripts[num] = this;

	Link ();

	if (level.flags2 & LEVEL2_HEXENHACK)
	{
		PutLast();
	}

	DPrintf ("Script %d started.\n", num);
}

static void SetScriptState (int script, DLevelScript::EScriptState state)
{
	DACSThinker *controller = DACSThinker::ActiveThinker;

	if (controller != NULL && controller->RunningScripts[script])
		controller->RunningScripts[script]->SetState (state);
}

void P_DoDeferedScripts ()
{
	acsdefered_t *def;
	const ScriptPtr *scriptdata;
	FBehavior *module;

	// Handle defered scripts in this step, too
	def = level.info->defered;
	while (def)
	{
		acsdefered_t *next = def->next;
		switch (def->type)
		{
		case acsdefered_t::defexecute:
		case acsdefered_t::defexealways:
			scriptdata = FBehavior::StaticFindScript (def->script, module);
			if (scriptdata)
			{
				P_GetScriptGoing ((unsigned)def->playernum < MAXPLAYERS &&
					playeringame[def->playernum] ? players[def->playernum].mo : NULL,
					NULL, def->script,
					scriptdata, module,
					0, def->arg0, def->arg1, def->arg2,
					def->type == acsdefered_t::defexealways);
			}
			else
			{
				Printf ("P_DoDeferredScripts: Unknown script %d\n", def->script);
			}
			break;

		case acsdefered_t::defsuspend:
			SetScriptState (def->script, DLevelScript::SCRIPT_Suspended);
			DPrintf ("Defered suspend of script %d\n", def->script);
			break;

		case acsdefered_t::defterminate:
			SetScriptState (def->script, DLevelScript::SCRIPT_PleaseRemove);
			DPrintf ("Defered terminate of script %d\n", def->script);
			break;
		}
		delete def;
		def = next;
	}
	level.info->defered = NULL;
}

static void addDefered (level_info_t *i, acsdefered_t::EType type, int script, int arg0, int arg1, int arg2, AActor *who)
{
	if (i)
	{
		acsdefered_t *def = new acsdefered_t;

		def->next = i->defered;
		def->type = type;
		def->script = script;
		def->arg0 = arg0;
		def->arg1 = arg1;
		def->arg2 = arg2;
		if (who != NULL && who->player != NULL)
		{
			def->playernum = int(who->player - players);
		}
		else
		{
			def->playernum = -1;
		}
		i->defered = def;
		DPrintf ("Script %d on map %s defered\n", script, i->mapname);
	}
}

EXTERN_CVAR (Bool, sv_cheats)

int P_StartScript (AActor *who, line_t *where, int script, const char *map, bool backSide,
					int arg0, int arg1, int arg2, int always, bool wantResultCode, bool net)
{
	if (map == NULL || 0 == strnicmp (level.mapname, map, 8))
	{
		FBehavior *module = NULL;
		const ScriptPtr *scriptdata;

		if ((scriptdata = FBehavior::StaticFindScript (script, module)) != NULL)
		{
			if (net && (( NETWORK_GetState( ) == NETSTATE_CLIENT ) || ( CLIENTDEMO_IsPlaying( ))) && !sv_cheats)
			{
				// If playing multiplayer and cheats are disallowed, check to
				// make sure only net scripts are run.
				if (!(scriptdata->Flags & SCRIPTF_Net))
				{
					Printf (PRINT_BOLD, "%s tried to puke script %d (%d, %d, %d)\n",
						who->player->userinfo.netname, script, arg0, arg1, arg2);
					return false;
				}
			}
			DLevelScript *runningScript = P_GetScriptGoing (who, where, script,
				scriptdata, module, backSide, arg0, arg1, arg2, always);
			if (runningScript != NULL)
			{
				if (wantResultCode)
				{
					return runningScript->RunScript ();
				}
				return true;
			}
			return false;
		}
		else
		{
			if (!net || (who && who->player == &players[consoleplayer]))
			{
				Printf ("P_StartScript: Unknown script %d\n", script);
			}
		}
	}
	else
	{
		addDefered (FindLevelInfo (map),
					always ? acsdefered_t::defexealways : acsdefered_t::defexecute,
					script, arg0, arg1, arg2, who);
		return true;
	}
	return false;
}

void P_SuspendScript (int script, char *map)
{
	if (strnicmp (level.mapname, map, 8))
		addDefered (FindLevelInfo (map), acsdefered_t::defsuspend, script, 0, 0, 0, NULL);
	else
		SetScriptState (script, DLevelScript::SCRIPT_Suspended);
}

void P_TerminateScript (int script, char *map)
{
	if (strnicmp (level.mapname, map, 8))
		addDefered (FindLevelInfo (map), acsdefered_t::defterminate, script, 0, 0, 0, NULL);
	else
		SetScriptState (script, DLevelScript::SCRIPT_PleaseRemove);
}

FArchive &operator<< (FArchive &arc, acsdefered_t *&defertop)
{
	BYTE more;

	if (arc.IsStoring ())
	{
		acsdefered_t *defer = defertop;
		more = 1;
		while (defer)
		{
			BYTE type;
			arc << more;
			type = (BYTE)defer->type;
			arc << type << defer->script << defer->playernum
				<< defer->arg0 << defer->arg1 << defer->arg2;
			defer = defer->next;
		}
		more = 0;
		arc << more;
	}
	else
	{
		acsdefered_t **defer = &defertop;

		arc << more;
		while (more)
		{
			*defer = new acsdefered_t;
			arc << more;
			(*defer)->type = (acsdefered_t::EType)more;
			arc << (*defer)->script << (*defer)->playernum
				<< (*defer)->arg0 << (*defer)->arg1 << (*defer)->arg2;
			defer = &((*defer)->next);
			arc << more;
		}
		*defer = NULL;
	}
	return arc;
}

CCMD (scriptstat)
{
	if (DACSThinker::ActiveThinker == NULL)
	{
		Printf ("No scripts are running.\n");
	}
	else
	{
		DACSThinker::ActiveThinker->DumpScriptStatus ();
	}
}

void DACSThinker::DumpScriptStatus ()
{
	static const char *stateNames[] =
	{
		"Running",
		"Suspended",
		"Delayed",
		"TagWait",
		"PolyWait",
		"ScriptWaitPre",
		"ScriptWait",
		"PleaseRemove"
	};
	DLevelScript *script = Scripts;

	while (script != NULL)
	{
		Printf ("%d: %s\n", script->script, stateNames[script->state]);
		script = script->next;
	}
}

#undef STRINGBUILDER_START
#undef STRINGBUILDER_FINISH

//*****************************************************************************
//
bool ACS_IsCalledFromConsoleCommand( void )
{
	return ( g_bCalledFromConsoleCommand );
}

//*****************************************************************************
//
bool ACS_IsScriptClientSide( ULONG ulScript )
{
	FBehavior		*pModule = NULL;

	return ACS_IsScriptClientSide ( FBehavior::StaticFindScript( ulScript, pModule ) );
}

//*****************************************************************************
//
bool ACS_IsScriptClientSide( const ScriptPtr *pScriptData )
{
	if ( pScriptData == NULL )
		return ( false );

	// [BB] Some existing maps rely on Skulltag's old net script handling.
	if ( ( pScriptData->Flags & SCRIPTF_Net ) && ( zacompatflags & ZACOMPATF_NETSCRIPTS_ARE_CLIENTSIDE ) )
		return ( true );

	if ( pScriptData->Flags & SCRIPTF_ClientSide )
		return ( true );

	return ( false );
}

//*****************************************************************************
//
bool ACS_IsScriptPukeable( ULONG ulScript )
{
	FBehavior *pModule = NULL;

	const ScriptPtr *pScriptData = FBehavior::StaticFindScript( ulScript, pModule );

	if ( pScriptData == NULL )
		return ( false );

	if ( pScriptData->Flags & SCRIPTF_Net )
		return ( true );

	if ( sv_cheats )
		return ( true );

	return ( false );
}

//*****************************************************************************
//
int ACS_GetTranslationIndex( FRemapTable *pTranslation )
{
	int translationindex;
	for ( translationindex = 1; translationindex <= MAX_ACS_TRANSLATIONS; translationindex++ )
	{
		if ( pTranslation == translationtables[TRANSLATION_LevelScripted].GetVal(translationindex - 1) )
			break;
	}
	return translationindex;
}
