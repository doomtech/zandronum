#include "actor.h"
#include "info.h"
#include "p_local.h"
#include "s_sound.h"
#include "p_enemy.h"
#include "a_doomglobal.h"
#include "gstrings.h"
#include "a_action.h"

//
// PIT_VileCheck
// Detect a corpse that could be raised.
//
void A_Fire (AActor *self);



//
// A_VileStart
//
void A_VileStart (AActor *self)
{
	S_Sound (self, CHAN_VOICE, "vile/start", 1, ATTN_NORM);
}


//
// A_Fire
// Keep fire in front of player unless out of sight
//
void A_StartFire (AActor *self)
{
	S_Sound (self, CHAN_BODY, "vile/firestrt", 1, ATTN_NORM);
	A_Fire (self);
}

void A_FireCrackle (AActor *self)
{
	S_Sound (self, CHAN_BODY, "vile/firecrkl", 1, ATTN_NORM);
	A_Fire (self);
}

void A_Fire (AActor *self)
{
	AActor *dest;
	angle_t an;
				
	// [BC] Fire movement is server-side.
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
		return;

	dest = self->tracer;
	if (!dest)
		return;
				
	// don't move it if the vile lost sight
	if (!P_CheckSight (self->target, dest, 0) )
		return;

	an = dest->angle >> ANGLETOFINESHIFT;

	self->SetOrigin (dest->x + FixedMul (24*FRACUNIT, finecosine[an]),
					 dest->y + FixedMul (24*FRACUNIT, finesine[an]),
					 dest->z);

	// [BC] Tell clients of the fire update.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_MoveThingExact( self, CM_X|CM_Y|CM_Z );
}



//
// A_VileTarget
// Spawn the hellfire
//
void A_VileTarget (AActor *actor)
{
	AActor *fog;
		
	// [BC] Fire movement is server-side.
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
		return;

	if (!actor->target)
		return;

	A_FaceTarget (actor);

	fog = Spawn ("ArchvileFire", actor->target->x, actor->target->y,
		actor->target->z, ALLOW_REPLACE);
	
	// [BC] If we're the server, tell clients to spawn the thing.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SpawnThing( fog );

	actor->tracer = fog;
	fog->target = actor;
	fog->tracer = actor->target;
	A_Fire (fog);
}




//
// A_VileAttack
//
void A_VileAttack (AActor *actor)
{		
	AActor *fire;
	int an;
		
	if (!actor->target)
		return;
	
	A_FaceTarget (actor);

	if (!P_CheckSight (actor, actor->target, 0) )
		return;

	S_Sound (actor, CHAN_WEAPON, "vile/stop", 1, ATTN_NORM);
	P_DamageMobj (actor->target, actor, actor, 20, NAME_None);
	P_TraceBleed (20, actor->target);
	actor->target->momz = 1000 * FRACUNIT / actor->target->Mass;
		
	// [BC] Tell clients to play the arch-vile sound on their end.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SoundActor( actor, CHAN_WEAPON, "vile/stop", 1, ATTN_NORM );

	an = actor->angle >> ANGLETOFINESHIFT;

	fire = actor->tracer;

	if (!fire)
		return;
				
	// move the fire between the vile and the player
	fire->SetOrigin (actor->target->x + FixedMul (24*FRACUNIT, finecosine[an]),
					 actor->target->y + FixedMul (24*FRACUNIT, finesine[an]),
					 actor->target->z);
	
	P_RadiusAttack (fire, actor, 70, 70, NAME_Fire, false);
}
