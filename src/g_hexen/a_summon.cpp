#include "info.h"
#include "a_pickups.h"
#include "a_artifacts.h"
#include "gstrings.h"
#include "p_local.h"
#include "p_enemy.h"
#include "s_sound.h"
#include "ravenshared.h"
#include "cl_demo.h"
#include "network.h"
#include "sv_commands.h"

void A_Summon (AActor *);

// Dark Servant Artifact ----------------------------------------------------

class AArtiDarkServant : public AInventory
{
	DECLARE_ACTOR (AArtiDarkServant, AInventory)
public:
	bool Use (bool pickup);
};

FState AArtiDarkServant::States[] =
{
#define S_ARTI_SUMMON 0
	S_NORMAL (SUMN, 'A',  350, NULL					    , &States[S_ARTI_SUMMON]),
};

IMPLEMENT_ACTOR (AArtiDarkServant, Hexen, 86, 16)
	PROP_Flags (MF_SPECIAL|MF_COUNTITEM)
	PROP_Flags2 (MF2_FLOATBOB)
	PROP_SpawnState (S_ARTI_SUMMON)
	PROP_Inventory_RespawnTics (30+4200)
	PROP_Inventory_DefMaxAmount
	PROP_Inventory_PickupFlash (1)
	PROP_Inventory_FlagsSet (IF_INVBAR|IF_FANCYPICKUPSOUND)
	PROP_Inventory_Icon ("ARTISUMN")
	PROP_Inventory_PickupSound ("misc/p_pkup")
	PROP_Inventory_PickupMessage("$TXT_ARTISUMMON")
END_DEFAULTS


//============================================================================
//
// Activate the summoning artifact
//
//============================================================================

bool AArtiDarkServant::Use (bool pickup)
{
	// [BC] Don't do this in client mode.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return ( true );
	}

	AActor *mo = P_SpawnPlayerMissile (Owner, PClass::FindClass ("SummoningDoll"));
	if (mo)
	{
		mo->target = Owner;
		mo->tracer = Owner;
		mo->momz = 5*FRACUNIT;

		// [BC] If we're the server, send clients this missile's updated properties.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_MoveThing( mo, CM_MOMZ );
	}
	return true;
}

//============================================================================
//
// A_Summon
//
//============================================================================

void A_Summon (AActor *actor)
{
	AMinotaurFriend *mo;

	// [BC] Don't do this in client mode.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return;
	}

	mo = Spawn<AMinotaurFriend> (actor->x, actor->y, actor->z, ALLOW_REPLACE);
	if (mo)
	{
		if (P_TestMobjLocation(mo) == false || !actor->tracer)
		{ // Didn't fit - change back to artifact
			mo->Destroy ();
			AActor *arti = Spawn<AArtiDarkServant> (actor->x, actor->y, actor->z, ALLOW_REPLACE);
			if (arti)
			{
				arti->flags |= MF_DROPPED;
				
				// [BC] If we're the server, spawn this item to clients.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_SpawnThing( arti );
			}
			return;
		}
				
		// [BC] If we're the server, spawn this item to clients.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SpawnThing( mo );

		mo->StartTime = level.maptime;
		if (actor->tracer->flags & MF_CORPSE)
		{	// Master dead
			mo->tracer = NULL;		// No master
		}
		else
		{
			mo->tracer = actor->tracer;		// Pointer to master
			AInventory *power = Spawn<APowerMinotaur> (0, 0, 0, NO_REPLACE);
			power->TryPickup (actor->tracer);
			if (actor->tracer->player != NULL)
			{
				mo->FriendPlayer = int(actor->tracer->player - players + 1);
			}
		}

		// Make smoke puff
		// [BC]
		AActor	*pSmoke;
		pSmoke = Spawn ("MinotaurSmoke", actor->x, actor->y, actor->z, ALLOW_REPLACE);
		S_Sound (actor, CHAN_VOICE, mo->ActiveSound, 1, ATTN_NORM);
				
		// [BC] If we're the server, spawn the smoke, and play the active sound.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		{
			if ( pSmoke )
				SERVERCOMMANDS_SpawnThing( pSmoke );
			SERVERCOMMANDS_SoundActor( actor, CHAN_VOICE, S_GetName( mo->ActiveSound ), 1, ATTN_NORM );
		}
	}
}
