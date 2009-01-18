#include "actor.h"
#include "gi.h"
#include "m_random.h"
#include "s_sound.h"
#include "d_player.h"
#include "a_action.h"
#include "p_local.h"
#include "p_enemy.h"
#include "a_action.h"
#include "p_pspr.h"
#include "gstrings.h"
#include "a_hexenglobal.h"
// [BB] New #includes.
#include "cl_demo.h"
#include "network.h"
#include "sv_commands.h"

static FRandom pr_smoke ("MWandSmoke");

void A_MWandAttack (AActor *actor);

// Wand Missile -------------------------------------------------------------

class AMageWandMissile : public AActor
{
	DECLARE_CLASS(AMageWandMissile, AActor)
public:
	void Tick ();
};

IMPLEMENT_CLASS (AMageWandMissile)

void AMageWandMissile::Tick ()
{
	int i;
	fixed_t xfrac;
	fixed_t yfrac;
	fixed_t zfrac;
	fixed_t hitz;
	bool changexy;

	PrevX = x;
	PrevY = y;
	PrevZ = z;

	// [RH] Ripping is a little different than it was in Hexen
	FCheckPosition tm(!!(flags2 & MF2_RIP));

	// Handle movement
	if (momx || momy || (z != floorz) || momz)
	{
		xfrac = momx>>3;
		yfrac = momy>>3;
		zfrac = momz>>3;
		changexy = xfrac || yfrac;
		for (i = 0; i < 8; i++)
		{
			if (changexy)
			{
				tm.LastRipped = NULL;	// [RH] Do rip damage each step, like Hexen
				if (!P_TryMove (this, x+xfrac,y+yfrac, true, false, tm))
				{ // Blocked move
					P_ExplodeMissile (this, BlockingLine, BlockingMobj);
					return;
				}
			}
			z += zfrac;
			if (z <= floorz)
			{ // Hit the floor
				z = floorz;
				P_HitFloor (this);
				P_ExplodeMissile (this, NULL, NULL);
				return;
			}
			if (z+height > ceilingz)
			{ // Hit the ceiling
				z = ceilingz-height;
				P_ExplodeMissile (this, NULL, NULL);
				return;
			}
			if (changexy)
			{
				//if (pr_smoke() < 128)	// [RH] I think it looks better if it's consistant
				{
					hitz = z-8*FRACUNIT;
					if (hitz < floorz)
					{
						hitz = floorz;
					}
					Spawn ("MageWandSmoke", x, y, hitz, ALLOW_REPLACE);
				}
			}
		}
	}
	
	// Advance the state
	if (tics != -1)
	{
		tics--;
		while (!tics)
		{
			if (!SetState (state->GetNextState()))
			{ // actor was removed
				return;
			}
		}
	}
}

//============================================================================
//
// A_MWandAttack
//
//============================================================================

void A_MWandAttack (AActor *actor)
{
	AActor *mo;

	// [BC] If we're the client, just play the sound and get out.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		S_Sound (actor, CHAN_WEAPON, "MageWandFire", 1, ATTN_NORM);
		return;
	}

	mo = P_SpawnPlayerMissile (actor, RUNTIME_CLASS(AMageWandMissile));

	// [BC] Apply spread.
	if (( actor->player ) &&
		( actor->player->cheats & CF_SPREAD ))
	{
		mo = P_SpawnPlayerMissile( actor, RUNTIME_CLASS( AMageWandMissile ), actor->angle + ( ANGLE_45 / 3 ));
		mo = P_SpawnPlayerMissile( actor, RUNTIME_CLASS( AMageWandMissile ), actor->angle - ( ANGLE_45 / 3 ));
	}

	S_Sound (actor, CHAN_WEAPON, "MageWandFire", 1, ATTN_NORM);

	// [BC] If we're the server, play the sound.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_WeaponSound( ULONG( actor->player - players ), "MageWandFire", ULONG( actor->player - players ), SVCF_SKIPTHISCLIENT );
}
