/*
#include "actor.h"
#include "info.h"
#include "p_local.h"
#include "s_sound.h"
#include "p_enemy.h"
#include "gstrings.h"
#include "a_action.h"
#include "thingdef/thingdef.h"
*/

//
// PIT_VileCheck
// Detect a corpse that could be raised.
//
void A_Fire(AActor *self, int height);


//
// A_VileStart
//
DEFINE_ACTION_FUNCTION(AActor, A_VileStart)
{
	S_Sound (self, CHAN_VOICE, "vile/start", 1, ATTN_NORM);
}


//
// A_Fire
// Keep fire in front of player unless out of sight
//
DEFINE_ACTION_FUNCTION(AActor, A_StartFire)
{
	S_Sound (self, CHAN_BODY, "vile/firestrt", 1, ATTN_NORM);
	A_Fire (self, 0);
}

DEFINE_ACTION_FUNCTION(AActor, A_FireCrackle)
{
	S_Sound (self, CHAN_BODY, "vile/firecrkl", 1, ATTN_NORM);
	A_Fire (self, 0);
}

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_Fire)
{
	ACTION_PARAM_START(1);
	ACTION_PARAM_FIXED(height,0);
	
	A_Fire(self, height);
}

void A_Fire(AActor *self, int height)
{
	AActor *dest;
	angle_t an;
				
	// [BC] Fire movement is server-side.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return;
	}

	dest = self->tracer;
	if (dest == NULL || self->target == NULL)
		return;
				
	// don't move it if the vile lost sight
	if (!P_CheckSight (self->target, dest, 0) )
		return;

	an = dest->angle >> ANGLETOFINESHIFT;

	self->SetOrigin (dest->x + FixedMul (24*FRACUNIT, finecosine[an]),
					 dest->y + FixedMul (24*FRACUNIT, finesine[an]),
					 dest->z + height);

	// [BC] Tell clients of the fire update.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_MoveThingExact( self, CM_X|CM_Y|CM_Z );
}



//
// A_VileTarget
// Spawn the hellfire
//
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_VileTarget)
{
	ACTION_PARAM_START(1);
	ACTION_PARAM_CLASS(fire,0);
	AActor *fog;
		
	// [BC] Fire movement is server-side.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return;
	}

	if (!self->target)
		return;

	A_FaceTarget (self);

	fog = Spawn (fire, self->target->x, self->target->y,
		self->target->z, ALLOW_REPLACE);
	
	// [BC] If we're the server, tell clients to spawn the thing.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SpawnThing( fog );

	self->tracer = fog;
	fog->target = self;
	fog->tracer = self->target;
	A_Fire(fog, 0);
}




//
// A_VileAttack
//
DEFINE_ACTION_FUNCTION(AActor, A_VileAttack)
{		
	AActor *fire, *target;
	int an;
		
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return;
	}

	if (NULL == (target = self->target))
		return;
	
	A_FaceTarget (self);

	if (!P_CheckSight (self, target, 0) )
		return;

	S_Sound (self, CHAN_WEAPON, "vile/stop", 1, ATTN_NORM);
	P_TraceBleed (20, target);
	P_DamageMobj (target, self, self, 20, NAME_None);
	target->velz = 1000 * FRACUNIT / target->Mass;
		
	// [BC] Tell clients to play the arch-vile sound on their end.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SoundActor( self, CHAN_WEAPON, "vile/stop", 1, ATTN_NORM );

	an = self->angle >> ANGLETOFINESHIFT;

	fire = self->tracer;

	if (fire != NULL)
	{
		// move the fire between the vile and the player
		fire->SetOrigin (target->x - FixedMul (24*FRACUNIT, finecosine[an]),
						 target->y - FixedMul (24*FRACUNIT, finesine[an]),
						 target->z);
		
		// [BC] Tell clients of the fire update.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_MoveThingExact( fire, CM_X|CM_Y|CM_Z );

		P_RadiusAttack (fire, self, 70, 70, NAME_Fire, false);
	}
}
