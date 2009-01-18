#include "actor.h"
#include "m_random.h"
#include "a_action.h"
#include "p_local.h"
#include "p_enemy.h"
#include "s_sound.h"
// [BB] New #includes.
#include "cl_demo.h"
#include "sv_commands.h"

static FRandom pr_stalker ("Stalker");


void A_StalkerChaseDecide (AActor *self)
{
	// [BC] This is handled server-side.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return;
	}

	if (!(self->flags & MF_NOGRAVITY))
	{
		// [BC] Set the thing's state.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetThingFrame( self, self->FindState("SeeFloor") );

		self->SetState (self->FindState("SeeFloor"));
	}
	else if (self->ceilingz - self->height > self->z)
	{
		// [BC] Set the thing's state.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetThingFrame( self, self->FindState("Drop") );

		self->SetState (self->FindState("Drop"));
	}
}

void A_StalkerLookInit (AActor *self)
{
	FState *state;

	// [BC] This is handled server-side.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return;
	}

	if (self->flags & MF_NOGRAVITY)
	{
		// [BC] Set the thing's state.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetThingFrame( self, self->FindState("LookCeiling") );

		state = self->FindState("LookCeiling");
	}
	else
	{
		state = self->FindState("LookFloor");
	}
	if (self->state->NextState != state)
	{
		// [BC] Set the thing's state.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetThingFrame( self, state );

		self->SetState (state);

	}
}

void A_StalkerDrop (AActor *self)
{
	self->flags5 &= ~MF5_NOVERTICALMELEERANGE;
	self->flags &= ~MF_NOGRAVITY;
}

void A_StalkerAttack (AActor *self)
{
	// [BC] This is handled server-side.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return;
	}

	if (self->flags & MF_NOGRAVITY)
	{
		// [BC] Set the thing's state.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetThingFrame( self, self->FindState("Drop") );

		self->SetState (self->FindState("Drop"));
	}
	else if (self->target != NULL)
	{
		A_FaceTarget (self);
		if (self->CheckMeleeRange ())
		{
			int damage = (pr_stalker() & 7) * 2 + 2;

			P_DamageMobj (self->target, self, self, damage, NAME_Melee);
			P_TraceBleed (damage, self->target, self);
		}
	}
}

void A_StalkerWalk (AActor *self)
{
	S_Sound (self, CHAN_BODY, "stalker/walk", 1, ATTN_NORM);
	A_Chase (self);
}
