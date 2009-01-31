/*
#include "info.h"
#include "a_pickups.h"
#include "a_artifacts.h"
#include "gstrings.h"
#include "p_local.h"
#include "s_sound.h"
#include "ravenshared.h"
#include "thingdef/thingdef.h"
#include "g_level.h"
*/

void A_Summon (AActor *);

// Dark Servant Artifact ----------------------------------------------------

class AArtiDarkServant : public AInventory
{
	DECLARE_CLASS (AArtiDarkServant, AInventory)
public:
	bool Use (bool pickup);
};

IMPLEMENT_CLASS (AArtiDarkServant)

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

DEFINE_ACTION_FUNCTION(AActor, A_Summon)
{
	AMinotaurFriend *mo;

	// [BC] Don't do this in client mode.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return;
	}

	mo = Spawn<AMinotaurFriend> (self->x, self->y, self->z, ALLOW_REPLACE);
	if (mo)
	{
		if (P_TestMobjLocation(mo) == false || !self->tracer)
		{ // Didn't fit - change back to artifact
			mo->Destroy ();
			AActor *arti = Spawn<AArtiDarkServant> (self->x, self->y, self->z, ALLOW_REPLACE);
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
		if (self->tracer->flags & MF_CORPSE)
		{	// Master dead
			mo->tracer = NULL;		// No master
		}
		else
		{
			mo->tracer = self->tracer;		// Pointer to master
			AInventory *power = Spawn<APowerMinotaur> (0, 0, 0, NO_REPLACE);
			power->CallTryPickup (self->tracer);
			if (self->tracer->player != NULL)
			{
				mo->FriendPlayer = int(self->tracer->player - players + 1);
			}
		}

		// Make smoke puff
		// [BC]
		AActor	*pSmoke;
		pSmoke = Spawn ("MinotaurSmoke", self->x, self->y, self->z, ALLOW_REPLACE);
		S_Sound (self, CHAN_VOICE, mo->ActiveSound, 1, ATTN_NORM);

				
		// [BC] If we're the server, spawn the smoke, and play the active sound.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		{
			if ( pSmoke )
				SERVERCOMMANDS_SpawnThing( pSmoke );
			SERVERCOMMANDS_SoundActor( self, CHAN_VOICE, S_GetName( mo->ActiveSound ), 1, ATTN_NORM );
		}
	}
}
