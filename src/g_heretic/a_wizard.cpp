#include "actor.h"
#include "info.h"
#include "m_random.h"
#include "s_sound.h"
#include "p_local.h"
#include "p_enemy.h"
#include "a_hereticglobal.h"
#include "a_action.h"
#include "gstrings.h"
// [BB] New #includes.
#include "sv_commands.h"
#include "cl_demo.h"

static FRandom pr_wizatk3 ("WizAtk3");

//----------------------------------------------------------------------------
//
// PROC A_GhostOff
//
//----------------------------------------------------------------------------

void A_GhostOff (AActor *actor)
{
	actor->RenderStyle = STYLE_Normal;
	actor->flags3 &= ~MF3_GHOST;
}

//----------------------------------------------------------------------------
//
// PROC A_WizAtk1
//
//----------------------------------------------------------------------------

void A_WizAtk1 (AActor *actor)
{
	A_FaceTarget (actor);
	A_GhostOff (actor);
}

//----------------------------------------------------------------------------
//
// PROC A_WizAtk2
//
//----------------------------------------------------------------------------

void A_WizAtk2 (AActor *actor)
{
	A_FaceTarget (actor);
	actor->alpha = HR_SHADOW;
	actor->RenderStyle = STYLE_Translucent;
	actor->flags3 |= MF3_GHOST;
}

//----------------------------------------------------------------------------
//
// PROC A_WizAtk3
//
//----------------------------------------------------------------------------

void A_WizAtk3 (AActor *actor)
{
	AActor *mo;

	A_GhostOff (actor);

	// [BB] This is server-side, the client only needs to run A_GhostOff.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return;
	}

	if (!actor->target)
	{
		return;
	}
	S_Sound (actor, CHAN_WEAPON, actor->AttackSound, 1, ATTN_NORM);

	// [BB] If we're the server, tell the clients to play the sound.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SoundActor( actor, CHAN_WEAPON, S_GetName( actor->AttackSound ), 1, ATTN_NORM );

	if (actor->CheckMeleeRange())
	{
		int damage = pr_wizatk3.HitDice (4);
		P_DamageMobj (actor->target, actor, actor, damage, NAME_Melee);
		P_TraceBleed (damage, actor->target, actor);
		return;
	}
	const PClass *fx = PClass::FindClass("WizardFX1");
	mo = P_SpawnMissile (actor, actor->target, fx);
	if (mo != NULL)
	{
		AActor *missile1 = P_SpawnMissileAngle(actor, fx, mo->angle-(ANG45/8), mo->momz);
		AActor *missile2 = P_SpawnMissileAngle(actor, fx, mo->angle+(ANG45/8), mo->momz);

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
