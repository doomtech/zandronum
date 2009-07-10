/*
#include "actor.h"
#include "p_enemy.h"
#include "a_action.h"
#include "p_local.h"
#include "m_random.h"
#include "thingdef/thingdef.h"
*/

static FRandom pr_sentinelrefire ("SentinelRefire");

DEFINE_ACTION_FUNCTION(AActor, A_SentinelBob)
{
	fixed_t minz, maxz;

	// [CW] This is handled by the server.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) || ( CLIENTDEMO_IsPlaying( )))
		return;

	if (self->flags & MF_INFLOAT)
	{
		self->momz = 0;

		// [CW] Moving the actor is server side.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_MoveThing( self, CM_MOMZ );

		return;
	}
	if (self->threshold != 0)
		return;

	maxz =  self->ceilingz - self->height - 16*FRACUNIT;
	minz = self->floorz + 96*FRACUNIT;
	if (minz > maxz)
	{
		minz = maxz;
	}
	if (minz < self->z)
	{
		self->momz -= FRACUNIT;
	}
	else
	{
		self->momz += FRACUNIT;
	}
	self->reactiontime = (minz >= self->z) ? 4 : 0;

	// [CW] Moving the actor is server side.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_MoveThingExact( self, CM_MOMZ );
}

DEFINE_ACTION_FUNCTION(AActor, A_SentinelAttack)
{
	AActor *missile = NULL, *trail;

	// [BB] Without a target the P_SpawnMissileZAimed call will crash.
	if (!self->target)
	{
		return;
	}

	// [CW] If we aren't a client, spawn the missile.
	if (( NETWORK_GetState( ) != NETSTATE_CLIENT ) && ( !CLIENTDEMO_IsPlaying( )))
	{
		missile = P_SpawnMissileZAimed (self, self->z + 32*FRACUNIT, self->target, PClass::FindClass("SentinelFX2"));

		// [CW] Spawn the missile server side.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SpawnMissile( missile );
	}

	if (missile != NULL && (missile->momx|missile->momy) != 0)
	{
		for (int i = 8; i > 1; --i)
		{
			trail = Spawn("SentinelFX1",
				self->x + FixedMul (missile->radius * i, finecosine[missile->angle >> ANGLETOFINESHIFT]),
				self->y + FixedMul (missile->radius * i, finesine[missile->angle >> ANGLETOFINESHIFT]),
				missile->z + (missile->momz / 4 * i), ALLOW_REPLACE);
			if (trail != NULL)
			{
				trail->target = self;
				trail->momx = missile->momx;
				trail->momy = missile->momy;
				trail->momz = missile->momz;
				P_CheckMissileSpawn (trail);
			}
		}
		missile->z += missile->momz >> 2;
	}
}

DEFINE_ACTION_FUNCTION(AActor, A_SentinelRefire)
{
	A_FaceTarget (self);

	// [CW] Clients may not do this.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) || ( CLIENTDEMO_IsPlaying( )))
		return;

	if (pr_sentinelrefire() >= 30)
	{
		if (self->target == NULL ||
			self->target->health <= 0 ||
			!P_CheckSight (self, self->target) ||
			P_HitFriend(self) ||
			pr_sentinelrefire() < 40)
		{
			// [CW] Tell clients to set the frame.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetThingFrame( self, self->SeeState );

			self->SetState (self->SeeState);
		}
	}
}
