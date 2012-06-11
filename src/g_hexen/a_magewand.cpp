/*
#include "actor.h"
#include "gi.h"
#include "m_random.h"
#include "s_sound.h"
#include "d_player.h"
#include "a_action.h"
#include "p_local.h"
#include "a_action.h"
#include "p_pspr.h"
#include "gstrings.h"
#include "a_hexenglobal.h"
#include "thingdef/thingdef.h"
*/

static FRandom pr_smoke ("MWandSmoke");

void A_MWandAttack (AActor *actor);

// Wand Missile -------------------------------------------------------------

class AMageWandMissile : public AFastProjectile
{
	DECLARE_CLASS(AMageWandMissile, AFastProjectile)
public:
	void Effect ();
};

IMPLEMENT_CLASS (AMageWandMissile)

void AMageWandMissile::Effect ()
{
	fixed_t hitz;

	//if (pr_smoke() < 128)	// [RH] I think it looks better if it's consistent
	{
		hitz = z-8*FRACUNIT;
		if (hitz < floorz)
		{
			hitz = floorz;
		}
		// [BB] Assume that the smoke is just for decorative purposes and let the
		// client spawn it on its own.
		AActor *mo = Spawn ("MageWandSmoke", x, y, hitz, ALLOW_REPLACE);
		if ( mo && NETWORK_InClientMode( ) )
			mo->ulNetworkFlags |= NETFL_CLIENTSIDEONLY;
	}
}

//============================================================================
//
// A_MWandAttack
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_MWandAttack)
{
	AActor *mo;

	// [BC] If we're the client, just play the sound and get out.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		S_Sound (self, CHAN_WEAPON, "MageWandFire", 1, ATTN_NORM);
		return;
	}

	mo = P_SpawnPlayerMissile (self, RUNTIME_CLASS(AMageWandMissile));

	// [BC] Apply spread.
	if (( self->player ) &&
		( self->player->cheats & CF_SPREAD ))
	{
		mo = P_SpawnPlayerMissile( self, RUNTIME_CLASS( AMageWandMissile ), self->angle + ( ANGLE_45 / 3 ));
		mo = P_SpawnPlayerMissile( self, RUNTIME_CLASS( AMageWandMissile ), self->angle - ( ANGLE_45 / 3 ));
	}

	S_Sound (self, CHAN_WEAPON, "MageWandFire", 1, ATTN_NORM);

	// [BC] If we're the server, play the sound.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_WeaponSound( ULONG( self->player - players ), "MageWandFire", ULONG( self->player - players ), SVCF_SKIPTHISCLIENT );
}
