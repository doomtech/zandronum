#include "actor.h"
#include "info.h"
#include "p_local.h"
#include "s_sound.h"
#include "p_enemy.h"
#include "a_action.h"
#include "m_random.h"
#include "a_hexenglobal.h"
// [BB] New #includes.
#include "sv_commands.h"
#include "cl_demo.h"

static FRandom pr_boom ("BishopBoom");
static FRandom pr_atk ("BishopAttack");
static FRandom pr_decide ("BishopDecide");
static FRandom pr_doblur ("BishopDoBlur");
static FRandom pr_sblur ("BishopSpawnBlur");
static FRandom pr_pain ("BishopPainBlur");

//============================================================================
//
// A_BishopAttack
//
//============================================================================

void A_BishopAttack (AActor *actor)
{
	// [BB] This is server-side.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return;
	}

	if (!actor->target)
	{
		return;
	}
	S_Sound (actor, CHAN_BODY, actor->AttackSound, 1, ATTN_NORM);

	// [BB] If we're the server, tell the clients to play the sound.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SoundActor( actor, CHAN_BODY, S_GetName(actor->AttackSound), 1, ATTN_NORM );

	if (actor->CheckMeleeRange())
	{
		int damage = pr_atk.HitDice (4);
		P_DamageMobj (actor->target, actor, actor, damage, NAME_Melee);
		P_TraceBleed (damage, actor->target, actor);
		return;
	}
	actor->special1 = (pr_atk() & 3) + 5;
}

//============================================================================
//
// A_BishopAttack2
//
//		Spawns one of a string of bishop missiles
//============================================================================

void A_BishopAttack2 (AActor *actor)
{
	AActor *mo;

	// [BB] This is server-side.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return;
	}

	if (!actor->target || !actor->special1)
	{
		actor->special1 = 0;
		actor->SetState (actor->SeeState);

		// [BB] If we're the server, tell the clients of the state change.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetThingState( actor, STATE_SEE );

		return;
	}
	mo = P_SpawnMissile (actor, actor->target, PClass::FindClass("BishopFX"));
	if (mo != NULL)
	{
		mo->tracer = actor->target;
		mo->special2 = 16; // High word == x/y, Low word == z

		// [BB] If we're the server, tell the clients to spawn this missile.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		{
			SERVERCOMMANDS_SpawnMissile( mo );
			SERVERCOMMANDS_SetThingSpecial2( mo );
		}
	}
	actor->special1--;
}

//============================================================================
//
// A_BishopMissileWeave
//
//============================================================================

void A_BishopMissileWeave (AActor *actor)
{
	fixed_t newX, newY;
	int weaveXY, weaveZ;
	int angle;

	if (actor->special2 == 0) actor->special2 = 16;

	weaveXY = actor->special2 >> 16;
	weaveZ = actor->special2 & 0xFFFF;
	angle = (actor->angle + ANG90) >> ANGLETOFINESHIFT;
	newX = actor->x - FixedMul (finecosine[angle], FloatBobOffsets[weaveXY]<<1);
	newY = actor->y - FixedMul (finesine[angle], FloatBobOffsets[weaveXY]<<1);
	weaveXY = (weaveXY + 2) & 63;
	newX += FixedMul (finecosine[angle], FloatBobOffsets[weaveXY]<<1);
	newY += FixedMul (finesine[angle], FloatBobOffsets[weaveXY]<<1);
	P_TryMove (actor, newX, newY, true);
	actor->z -= FloatBobOffsets[weaveZ];
	weaveZ = (weaveZ + 2) & 63;
	actor->z += FloatBobOffsets[weaveZ];	
	actor->special2 = weaveZ + (weaveXY<<16);
}

//============================================================================
//
// A_BishopDecide
//
//============================================================================

void A_BishopDecide (AActor *actor)
{
	// [BB] This is server-side.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return;
	}

	if (pr_decide() < 220)
	{
		return;
	}
	else
	{
		// [BB] If we're the server, tell the clients to update this thing's state.
		if ( ( NETWORK_GetState( ) == NETSTATE_SERVER ) )
			SERVERCOMMANDS_SetThingFrame( actor, actor->FindState ("Blur") );

		actor->SetState (actor->FindState ("Blur"));
	}		
}

//============================================================================
//
// A_BishopDoBlur
//
//============================================================================

void A_BishopDoBlur (AActor *actor)
{
	// [BB] This is server-side.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return;
	}

	actor->special1 = (pr_doblur() & 3) + 3; // Random number of blurs
	if (pr_doblur() < 120)
	{
		P_ThrustMobj (actor, actor->angle + ANG90, 11*FRACUNIT);
	}
	else if (pr_doblur() > 125)
	{
		P_ThrustMobj (actor, actor->angle - ANG90, 11*FRACUNIT);
	}
	else
	{ // Thrust forward
		P_ThrustMobj (actor, actor->angle, 11*FRACUNIT);
	}
	S_Sound (actor, CHAN_BODY, "BishopBlur", 1, ATTN_NORM);

	// [BB] If we're the server, update the thing's momentum and play the sound.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		SERVERCOMMANDS_MoveThingExact( actor, CM_MOMX|CM_MOMY );
		SERVERCOMMANDS_SoundActor( actor, CHAN_BODY, "BishopBlur", 1, ATTN_NORM );
	}
}

//============================================================================
//
// A_BishopSpawnBlur
//
//============================================================================

void A_BishopSpawnBlur (AActor *actor)
{
	// [BB] This is server-side.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return;
	}

	AActor *mo;

	if (!--actor->special1)
	{
		actor->momx = 0;
		actor->momy = 0;

		// [BB] If we're the server, update the thing's momentum.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_MoveThingExact( actor, CM_MOMX|CM_MOMY );

		if (pr_sblur() > 96)
		{
			// [BB] If we're the server, tell the clients of the state change.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetThingState( actor, STATE_SEE );

			actor->SetState (actor->SeeState);
		}
		else
		{
			// [BB] If we're the server, tell the clients of the state change.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetThingState( actor, STATE_MISSILE );
			
			actor->SetState (actor->MissileState);
		}
	}
	mo = Spawn ("BishopBlur", actor->x, actor->y, actor->z, ALLOW_REPLACE);
	if (mo)
	{
		mo->angle = actor->angle;

		// [BB] If we're the server, tell the clients to spawn the thing and set its angle.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		{
			SERVERCOMMANDS_SpawnThing( mo );
			SERVERCOMMANDS_SetThingAngle( mo );
		}
	}
}

//============================================================================
//
// A_BishopChase
//
//============================================================================

void A_BishopChase (AActor *actor)
{
	// [BB] This is server-side. The z coordinate seems to go out of sync
	// on client and server, if you make this client side.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return;
	}

	actor->z -= FloatBobOffsets[actor->special2] >> 1;
	actor->special2 = (actor->special2 + 4) & 63;
	actor->z += FloatBobOffsets[actor->special2] >> 1;

	// [BB] If we're the server, update the thing's z coordinate.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_MoveThingExact( actor, CM_Z );
}

//============================================================================
//
// A_BishopPuff
//
//============================================================================

void A_BishopPuff (AActor *actor)
{
	AActor *mo;

	mo = Spawn ("BishopPuff", actor->x, actor->y, actor->z + 40*FRACUNIT, ALLOW_REPLACE);
	if (mo)
	{
		mo->momz = FRACUNIT/2;
	}
}

//============================================================================
//
// A_BishopPainBlur
//
//============================================================================

void A_BishopPainBlur (AActor *actor)
{
	AActor *mo;

	// [BB] This is server-side.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return;
	}

	if (pr_pain() < 64)
	{
		// [BB] If we're the server, tell the clients to update this thing's state.
		if ( ( NETWORK_GetState( ) == NETSTATE_SERVER ) )
			SERVERCOMMANDS_SetThingFrame( actor, actor->FindState ("Blur") );

		actor->SetState (actor->FindState ("Blur"));

		return;
	}
	fixed_t x = actor->x + (pr_pain.Random2()<<12);
	fixed_t y = actor->y + (pr_pain.Random2()<<12);
	fixed_t z = actor->z + (pr_pain.Random2()<<11);
	mo = Spawn ("BishopPainBlur", x, y, z, ALLOW_REPLACE);
	if (mo)
	{
		mo->angle = actor->angle;

		// [BB] If we're the server, tell the clients to spawn the thing and set its angle.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		{
			SERVERCOMMANDS_SpawnThing( mo );
			SERVERCOMMANDS_SetThingAngle( mo );
		}
	}
}
