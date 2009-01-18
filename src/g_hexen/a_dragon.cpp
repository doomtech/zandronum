#include "actor.h"
#include "info.h"
#include "p_enemy.h"
#include "p_local.h"
#include "a_action.h"
#include "m_random.h"
#include "s_sound.h"
// [BB] New #includes.
#include "cl_demo.h"
#include "sv_commands.h"

static FRandom pr_dragonseek ("DragonSeek");
static FRandom pr_dragonflight ("DragonFlight");
static FRandom pr_dragonflap ("DragonFlap");
static FRandom pr_dragonfx2 ("DragonFX2");

void A_DragonInitFlight (AActor *);
void A_DragonFlap (AActor *);
void A_DragonFlight (AActor *);
void A_DragonPain (AActor *);
void A_DragonAttack (AActor *);
void A_DragonCheckCrash (AActor *);
void A_DragonFX2 (AActor *);

//============================================================================
//
// DragonSeek
//
//============================================================================

static void DragonSeek (AActor *actor, angle_t thresh, angle_t turnMax)
{
	int dir;
	int dist;
	angle_t delta;
	angle_t angle;
	AActor *target;
	int i;
	angle_t bestAngle;
	angle_t angleToSpot, angleToTarget;
	AActor *mo;

	// [BB] Let the server do this.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return;
	}

	target = actor->tracer;
	if(target == NULL)
	{
		return;
	}
	dir = P_FaceMobj (actor, target, &delta);
	if (delta > thresh)
	{
		delta >>= 1;
		if (delta > turnMax)
		{
			delta = turnMax;
		}
	}
	if (dir)
	{ // Turn clockwise
		actor->angle += delta;
	}
	else
	{ // Turn counter clockwise
		actor->angle -= delta;
	}
	angle = actor->angle>>ANGLETOFINESHIFT;
	actor->momx = FixedMul (actor->Speed, finecosine[angle]);
	actor->momy = FixedMul (actor->Speed, finesine[angle]);
	if (actor->z+actor->height < target->z ||
		target->z+target->height < actor->z)
	{
		dist = P_AproxDistance(target->x-actor->x, target->y-actor->y);
		dist = dist/actor->Speed;
		if (dist < 1)
		{
			dist = 1;
		}
		actor->momz = (target->z-actor->z)/dist;
	}
	else
	{
		dist = P_AproxDistance (target->x-actor->x, target->y-actor->y);
		dist = dist/actor->Speed;
	}
	// [BB] If we're the server, update the thing's momentum and angle.
	// Unfortunately there are sync issues, if we don't also update the actual position.
	// Is there a way to fix this without sending the position?
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_MoveThingExact( actor, CM_X|CM_Y|CM_Z|CM_ANGLE|CM_MOMX|CM_MOMY|CM_MOMZ );

	if (target->flags&MF_SHOOTABLE && pr_dragonseek() < 64)
	{ // attack the destination mobj if it's attackable
		AActor *oldTarget;
	
		if (abs(actor->angle-R_PointToAngle2(actor->x, actor->y, 
			target->x, target->y)) < ANGLE_45/2)
		{
			oldTarget = actor->target;
			actor->target = target;
			if (actor->CheckMeleeRange ())
			{
				int damage = pr_dragonseek.HitDice (10);
				P_DamageMobj (actor->target, actor, actor, damage, NAME_Melee);
				P_TraceBleed (damage, actor->target, actor);
				S_Sound (actor, CHAN_WEAPON, actor->AttackSound, 1, ATTN_NORM);

				// [BB] If we're the server, tell the clients to play the sound.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_SoundActor( actor, CHAN_WEAPON, S_GetName(actor->AttackSound), 1, ATTN_NORM );
			}
			else if (pr_dragonseek() < 128 && P_CheckMissileRange(actor))
			{
				AActor *missile = P_SpawnMissile(actor, target, PClass::FindClass ("DragonFireball"));						
				S_Sound (actor, CHAN_WEAPON, actor->AttackSound, 1, ATTN_NORM);

				// [BB] If we're the server, tell the clients to play the sound and spawn the missile.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				{
					SERVERCOMMANDS_SoundActor( actor, CHAN_WEAPON, S_GetName(actor->AttackSound), 1, ATTN_NORM );
					if ( missile )
						SERVERCOMMANDS_SpawnMissile( missile );
				}
			}
			actor->target = oldTarget;
		}
	}
	if (dist < 4)
	{ // Hit the target thing
		if (actor->target && pr_dragonseek() < 200)
		{
			AActor *bestActor = NULL;
			bestAngle = ANGLE_MAX;
			angleToTarget = R_PointToAngle2(actor->x, actor->y,
				actor->target->x, actor->target->y);
			for (i = 0; i < 5; i++)
			{
				if (!target->args[i])
				{
					continue;
				}
				FActorIterator iterator (target->args[i]);
				mo = iterator.Next ();
				if (mo == NULL)
				{
					continue;
				}
				angleToSpot = R_PointToAngle2(actor->x, actor->y, 
					mo->x, mo->y);
				if ((angle_t)abs(angleToSpot-angleToTarget) < bestAngle)
				{
					bestAngle = abs(angleToSpot-angleToTarget);
					bestActor = mo;
				}
			}
			if (bestActor != NULL)
			{
				actor->tracer = bestActor;
			}
		}
		else
		{
			// [RH] Don't lock up if the dragon doesn't have any
			// targets defined
			for (i = 0; i < 5; ++i)
			{
				if (target->args[i] != 0)
				{
					break;
				}
			}
			if (i < 5)
			{
				do
				{
					i = (pr_dragonseek()>>2)%5;
				} while(!target->args[i]);
				FActorIterator iterator (target->args[i]);
				actor->tracer = iterator.Next ();
			}
		}
	}
}

//============================================================================
//
// A_DragonInitFlight
//
//============================================================================

void A_DragonInitFlight (AActor *actor)
{
	// [BB] Let the server do this.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return;
	}

	FActorIterator iterator (actor->tid);

	do
	{ // find the first tid identical to the dragon's tid
		actor->tracer = iterator.Next ();
		if (actor->tracer == NULL)
		{
			// [BB] If we're the server, tell the clients to update the thing's state.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetThingState( actor, STATE_SPAWN );

			actor->SetState (actor->SpawnState);
			return;
		}
	} while (actor->tracer == actor);
	actor->RemoveFromHash ();
}

//============================================================================
//
// A_DragonFlight
//
//============================================================================

void A_DragonFlight (AActor *actor)
{
	angle_t angle;

	// [BB] Let the server do this.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return;
	}

	DragonSeek (actor, 4*ANGLE_1, 8*ANGLE_1);
	if (actor->target)
	{
		if(!(actor->target->flags&MF_SHOOTABLE))
		{ // target died
			actor->target = NULL;
			return;
		}
		angle = R_PointToAngle2(actor->x, actor->y, actor->target->x,
			actor->target->y);
		if (abs(actor->angle-angle) < ANGLE_45/2 && actor->CheckMeleeRange())
		{
			int damage = pr_dragonflight.HitDice (8);
			P_DamageMobj (actor->target, actor, actor, damage, NAME_Melee);
			P_TraceBleed (damage, actor->target, actor);
			S_Sound (actor, CHAN_WEAPON, actor->AttackSound, 1, ATTN_NORM);

			// [BB] If we're the server, tell the clients to play the sound.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SoundActor( actor, CHAN_WEAPON, S_GetName(actor->AttackSound), 1, ATTN_NORM );
		}
		else if (abs(actor->angle-angle) <= ANGLE_1*20)
		{
			// [BB] If we're the server, tell the clients to update the thing's state and play the sound.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			{
				SERVERCOMMANDS_SetThingState( actor, STATE_MISSILE );
				SERVERCOMMANDS_SoundActor( actor, CHAN_WEAPON, S_GetName(actor->AttackSound), 1, ATTN_NORM );
			}

			actor->SetState (actor->MissileState);
			S_Sound (actor, CHAN_WEAPON, actor->AttackSound, 1, ATTN_NORM);
		}
	}
	else
	{
		P_LookForPlayers (actor, true);
	}
}

//============================================================================
//
// A_DragonFlap
//
//============================================================================

void A_DragonFlap (AActor *actor)
{
	A_DragonFlight (actor);
	if (pr_dragonflap() < 240)
	{
		S_Sound (actor, CHAN_BODY, "DragonWingflap", 1, ATTN_NORM);
	}
	else
	{
		actor->PlayActiveSound ();
	}
}

//============================================================================
//
// A_DragonAttack
//
//============================================================================

void A_DragonAttack (AActor *actor)
{
	// [BB] Let the server do this.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return;
	}

	AActor *missile = P_SpawnMissile (actor, actor->target, PClass::FindClass ("DragonFireball"));						

	// [BB] If we're the server, tell the clients to spawn the missile.
	if ( (NETWORK_GetState( ) == NETSTATE_SERVER) && missile )
		SERVERCOMMANDS_SpawnMissile( missile );
}

//============================================================================
//
// A_DragonFX2
//
//============================================================================

void A_DragonFX2 (AActor *actor)
{
	AActor *mo;
	int i;
	int delay;

	delay = 16+(pr_dragonfx2()>>3);
	for (i = 1+(pr_dragonfx2()&3); i; i--)
	{
		fixed_t x = actor->x+((pr_dragonfx2()-128)<<14);
		fixed_t y = actor->y+((pr_dragonfx2()-128)<<14);
		fixed_t z = actor->z+((pr_dragonfx2()-128)<<12);

		mo = Spawn ("DragonExplosion", x, y, z, ALLOW_REPLACE);
		if (mo)
		{
			mo->tics = delay+(pr_dragonfx2()&3)*i*2;
			mo->target = actor->target;
		}
	} 
}

//============================================================================
//
// A_DragonPain
//
//============================================================================

void A_DragonPain (AActor *actor)
{
	A_Pain (actor);

	// [BB] Let the server do this.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return;
	}

	if (!actor->tracer)
	{ // no destination spot yet
		// [BB] If we're the server, tell the clients to update the thing's state.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetThingState( actor, STATE_SEE );

		actor->SetState (actor->SeeState);
	}
}

//============================================================================
//
// A_DragonCheckCrash
//
//============================================================================

void A_DragonCheckCrash (AActor *actor)
{
	if (actor->z <= actor->floorz)
	{
		actor->SetState (actor->FindState ("Crash"));
	}
}
