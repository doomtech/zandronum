#include "actor.h"
#include "m_random.h"
#include "a_action.h"
#include "p_local.h"
#include "p_enemy.h"
#include "s_sound.h"
// [BB] New #includes.
#include "cl_demo.h"
#include "network.h"
#include "sv_commands.h"

static FRandom pr_inq ("Inquisitor");

void A_InquisitorWalk (AActor *self)
{
	S_Sound (self, CHAN_BODY, "inquisitor/walk", 1, ATTN_NORM);
	A_Chase (self);
}

bool InquisitorCheckDistance (AActor *self)
{
	if (self->reactiontime == 0 && P_CheckSight (self, self->target))
	{
		return P_AproxDistance (self->x - self->target->x, self->y - self->target->y) < 264*FRACUNIT;
	}
	return false;
}

void A_InquisitorDecide (AActor *self)
{
	// [BC] This is handled server-side.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return;
	}

	if (self->target == NULL)
		return;

	A_FaceTarget (self);
	if (!InquisitorCheckDistance (self))
	{
		// [BC] Set the thing's state.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetThingFrame( self, self->FindState("Grenade") );

		self->SetState (self->FindState("Grenade"));
	}
	if (self->target->z != self->z)
	{
		if (self->z + self->height + 54*FRACUNIT < self->ceilingz)
		{
			// [BC] Set the thing's state.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetThingFrame( self, self->FindState("Jump") );

			self->SetState (self->FindState("Jump"));
		}
	}
}

void A_InquisitorAttack (AActor *self)
{
	AActor *proj;

	// [BC] This is handled server-side.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return;
	}

	if (self->target == NULL)
		return;

	A_FaceTarget (self);

	self->z += 32*FRACBITS;
	self->angle -= ANGLE_45/32;
	proj = P_SpawnMissileZAimed (self, self->z, self->target, PClass::FindClass("InquisitorShot"));
	if (proj != NULL)
	{
		proj->momz += 9*FRACUNIT;

		// [BC] Tell clients to spawn the missile.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SpawnMissile( proj );
	}
	self->angle += ANGLE_45/16;
	proj = P_SpawnMissileZAimed (self, self->z, self->target, PClass::FindClass("InquisitorShot"));
	if (proj != NULL)
	{
		proj->momz += 16*FRACUNIT;

		// [BC] Tell clients to spawn the missile.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SpawnMissile( proj );
	}
	self->z -= 32*FRACBITS;
}

void A_InquisitorJump (AActor *self)
{
	fixed_t dist;
	fixed_t speed;
	angle_t an;

	// [BC] This is handled server-side.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return;
	}

	if (self->target == NULL)
		return;

	S_Sound (self, CHAN_ITEM|CHAN_LOOP, "inquisitor/jump", 1, ATTN_NORM);
	self->z += 64*FRACUNIT;
	A_FaceTarget (self);
	an = self->angle >> ANGLETOFINESHIFT;
	speed = self->Speed * 2/3;
	self->momx += FixedMul (speed, finecosine[an]);
	self->momy += FixedMul (speed, finesine[an]);
	dist = P_AproxDistance (self->target->x - self->x, self->target->y - self->y);
	dist /= speed;
	if (dist < 1)
	{
		dist = 1;
	}
	self->momz = (self->target->z - self->z) / dist;
	self->reactiontime = 60;
	self->flags |= MF_NOGRAVITY;

	// [BC] If we're the server, update the thing's position.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		SERVERCOMMANDS_MoveThingExact( self, CM_Z|CM_MOMX|CM_MOMY|CM_MOMZ );

		// [CW] Also, set the flags to ensure the actor can fly.
		SERVERCOMMANDS_SetThingFlags( self, FLAGSET_FLAGS );
	}
}

void A_InquisitorCheckLand (AActor *self)
{
	// [BC] This is handled server-side.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return;
	}

	self->reactiontime--;
	if (self->reactiontime < 0 ||
		self->momx == 0 ||
		self->momy == 0 ||
		self->z <= self->floorz)
	{
		// [BC] Set the thing's state.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetThingState( self, STATE_SEE );

		self->SetState (self->SeeState);
		self->reactiontime = 0;
		self->flags &= ~MF_NOGRAVITY;
		S_StopSound (self, CHAN_ITEM);
		return;
	}
	if (!S_IsActorPlayingSomething (self, CHAN_ITEM, -1))
	{
		S_Sound (self, CHAN_ITEM|CHAN_LOOP, "inquisitor/jump", 1, ATTN_NORM);
	}

}

void A_TossArm (AActor *self)
{
	AActor *foo = Spawn("InquisitorArm", self->x, self->y, self->z + 24*FRACUNIT, ALLOW_REPLACE);
	foo->angle = self->angle - ANGLE_90 + (pr_inq.Random2() << 22);
	foo->momx = FixedMul (foo->Speed, finecosine[foo->angle >> ANGLETOFINESHIFT]) >> 3;
	foo->momy = FixedMul (foo->Speed, finesine[foo->angle >> ANGLETOFINESHIFT]) >> 3;
	foo->momz = pr_inq() << 10;
}

