/*
#include "actor.h"
#include "info.h"
#include "m_random.h"
#include "s_sound.h"
#include "p_local.h"
#include "p_enemy.h"
#include "a_action.h"
#include "gstrings.h"
#include "thingdef/thingdef.h"
*/

static FRandom pr_wizatk3 ("WizAtk3");

//----------------------------------------------------------------------------
//
// PROC A_GhostOff
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_GhostOff)
{
	self->RenderStyle = STYLE_Normal;
	self->flags3 &= ~MF3_GHOST;
}

//----------------------------------------------------------------------------
//
// PROC A_WizAtk1
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_WizAtk1)
{
	A_FaceTarget (self);
	CALL_ACTION(A_GhostOff, self);
}

//----------------------------------------------------------------------------
//
// PROC A_WizAtk2
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_WizAtk2)
{
	A_FaceTarget (self);
	self->alpha = HR_SHADOW;
	self->RenderStyle = STYLE_Translucent;
	self->flags3 |= MF3_GHOST;
}

//----------------------------------------------------------------------------
//
// PROC A_WizAtk3
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_WizAtk3)
{
	AActor *mo;

	CALL_ACTION(A_GhostOff, self);

	// [BB] This is server-side, the client only needs to run A_GhostOff.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return;
	}

	if (!self->target)
	{
		return;
	}
	S_Sound (self, CHAN_WEAPON, self->AttackSound, 1, ATTN_NORM);

	// [BB] If we're the server, tell the clients to play the sound.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SoundActor( self, CHAN_WEAPON, S_GetName( self->AttackSound ), 1, ATTN_NORM );

	if (self->CheckMeleeRange())
	{
		int damage = pr_wizatk3.HitDice (4);
		P_DamageMobj (self->target, self, self, damage, NAME_Melee);
		P_TraceBleed (damage, self->target, self);
		return;
	}
	const PClass *fx = PClass::FindClass("WizardFX1");
	mo = P_SpawnMissile (self, self->target, fx);
	if (mo != NULL)
	{
		AActor *missile1 = P_SpawnMissileAngle(self, fx, mo->angle-(ANG45/8), mo->momz);
		AActor *missile2 = P_SpawnMissileAngle(self, fx, mo->angle+(ANG45/8), mo->momz);

		// [BB] If we're the server, tell the clients to spawn the missiles.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		{
			SERVERCOMMANDS_SpawnMissile( mo );
			if ( missile1 )
				SERVERCOMMANDS_SpawnMissile( missile1 );
			if ( missile2 )
				SERVERCOMMANDS_SpawnMissile( missile2 );
		}

	}
}
