#include "actor.h"
#include "info.h"
#include "a_hereticglobal.h"
#include "p_local.h"
#include "p_enemy.h"
#include "a_action.h"
#include "s_sound.h"
#include "m_random.h"
#include "a_sharedglobal.h"
#include "gstrings.h"
#include "a_specialspot.h"
// [BB] New #includes.
#include "cl_demo.h"
#include "network.h"
#include "sv_commands.h"

static FRandom pr_s2fx1 ("S2FX1");
static FRandom pr_scrc1atk ("Srcr1Attack");
static FRandom pr_dst ("D'SparilTele");
static FRandom pr_s2d ("Srcr2Decide");
static FRandom pr_s2a ("Srcr2Attack");
static FRandom pr_bluespark ("BlueSpark");

//----------------------------------------------------------------------------
//
// PROC A_Sor1Pain
//
//----------------------------------------------------------------------------

void A_Sor1Pain (AActor *actor)
{
	actor->special1 = 20; // Number of steps to walk fast
	A_Pain (actor);
}

//----------------------------------------------------------------------------
//
// PROC A_Sor1Chase
//
//----------------------------------------------------------------------------

void A_Sor1Chase (AActor *actor)
{
	if (actor->special1)
	{
		actor->special1--;
		actor->tics -= 3;
	}
	A_Chase(actor);
}

//----------------------------------------------------------------------------
//
// PROC A_Srcr1Attack
//
// Sorcerer demon attack.
//
//----------------------------------------------------------------------------

void A_Srcr1Attack (AActor *actor)
{
	AActor *mo;
	fixed_t momz;
	angle_t angle;

	// [BC] In client mode, just play the attack sound and get out.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		S_Sound (actor, CHAN_BODY, actor->AttackSound, 1, ATTN_NORM);
		return;
	}

	if (!actor->target)
	{
		return;
	}
	S_Sound (actor, CHAN_BODY, actor->AttackSound, 1, ATTN_NORM);
	if (actor->CheckMeleeRange ())
	{
		int damage = pr_scrc1atk.HitDice (8);
		P_DamageMobj (actor->target, actor, actor, damage, NAME_Melee);
		P_TraceBleed (damage, actor->target, actor);
		return;
	}

	const PClass *fx = PClass::FindClass("SorcererFX1");
	if (actor->health > (actor->GetDefault()->health/3)*2)
	{ // Spit one fireball
		mo = P_SpawnMissileZ (actor, actor->z + 48*FRACUNIT, actor->target, fx );

		// [BC] Spawn this to clients.
		if (( mo ) && ( NETWORK_GetState( ) == NETSTATE_SERVER ))
			SERVERCOMMANDS_SpawnMissile( mo );
	}
	else
	{ // Spit three fireballs
		mo = P_SpawnMissileZ (actor, actor->z + 48*FRACUNIT, actor->target, fx);
		if (mo != NULL)
		{
			momz = mo->momz;
			angle = mo->angle;

			// [BC] Spawn this to clients.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SpawnMissile( mo );

			mo = P_SpawnMissileAngleZ (actor, actor->z + 48*FRACUNIT, fx, angle-ANGLE_1*3, momz);
			
			// [BC] Spawn this to clients.
			if (( mo ) && ( NETWORK_GetState( ) == NETSTATE_SERVER ))
				SERVERCOMMANDS_SpawnMissile( mo );

			mo = P_SpawnMissileAngleZ (actor, actor->z + 48*FRACUNIT, fx, angle+ANGLE_1*3, momz);

			// [BC] Spawn this to clients.
			if (( mo ) && ( NETWORK_GetState( ) == NETSTATE_SERVER ))
				SERVERCOMMANDS_SpawnMissile( mo );
		}
		if (actor->health < actor->GetDefault()->health/3)
		{ // Maybe attack again
			if (actor->special1)
			{ // Just attacked, so don't attack again
				actor->special1 = 0;
			}
			else
			{ // Set state to attack again
				actor->special1 = 1;
				actor->SetState (actor->FindState("Missile2"));
			}
		}
	}
}

//----------------------------------------------------------------------------
//
// PROC A_SorcererRise
//
//----------------------------------------------------------------------------

void A_SorcererRise (AActor *actor)
{
	AActor *mo;

	actor->flags &= ~MF_SOLID;

	// [BC] Let the server spawn this in client mode.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return;
	}

	mo = Spawn("Sorcerer2", actor->x, actor->y, actor->z, ALLOW_REPLACE);
	mo->SetState (mo->FindState("Rise"));
	mo->angle = actor->angle;
	mo->CopyFriendliness (actor, true);

	// [BC] If we're the server, spawn the sorcerer for clients.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SpawnThing( mo );
}

//----------------------------------------------------------------------------
//
// PROC P_DSparilTeleport
//
//----------------------------------------------------------------------------

void P_DSparilTeleport (AActor *actor)
{
	fixed_t prevX;
	fixed_t prevY;
	fixed_t prevZ;
	AActor *mo;
	AActor *spot;

	// [BC] Don't do this in client mode.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return;
	}

	DSpotState *state = DSpotState::GetSpotState();
	if (state == NULL) return;

	spot = state->GetSpotWithMinDistance(PClass::FindClass("BossSpot"), actor->x, actor->y, 128*FRACUNIT);
	if (spot == NULL) return;

	prevX = actor->x;
	prevY = actor->y;
	prevZ = actor->z;
	if (P_TeleportMove (actor, spot->x, spot->y, spot->z, false))
	{
		mo = Spawn("Sorcerer2Telefade", prevX, prevY, prevZ, ALLOW_REPLACE);
		S_Sound (mo, CHAN_BODY, "misc/teleport", 1, ATTN_NORM);

		// [BC] Spawn the actor to clients and play the sound.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		{
			SERVERCOMMANDS_SpawnThing( mo );
			SERVERCOMMANDS_SoundActor( mo, CHAN_BODY, "misc/teleport", 1, ATTN_NORM );
			SERVERCOMMANDS_SoundActor( actor, CHAN_BODY, "misc/teleport", 1, ATTN_NORM );
		}

		actor->SetState (actor->FindState("Teleport"));
		S_Sound (actor, CHAN_BODY, "misc/teleport", 1, ATTN_NORM);
		actor->z = actor->floorz;
		actor->angle = spot->angle;
		actor->momx = actor->momy = actor->momz = 0;
	}
}

//----------------------------------------------------------------------------
//
// PROC A_Srcr2Decide
//
//----------------------------------------------------------------------------

void A_Srcr2Decide (AActor *actor)
{

	static const int chance[] =
	{
		192, 120, 120, 120, 64, 64, 32, 16, 0
	};

	// [BC] Don't do this in client mode.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return;
	}

	unsigned int chanceindex = actor->health / (actor->GetDefault()->health/8);
	if (chanceindex >= countof(chance))
	{
		chanceindex = countof(chance) - 1;
	}

	if (pr_s2d() < chance[chanceindex])
	{
		P_DSparilTeleport (actor);
	}
}

//----------------------------------------------------------------------------
//
// PROC A_Srcr2Attack
//
//----------------------------------------------------------------------------

void A_Srcr2Attack (AActor *actor)
{
	// [BC]
	AActor	*mo;
	int chance;

	// [BC] Don't do this in client mode.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		S_Sound (actor, CHAN_BODY, actor->AttackSound, 1, ATTN_NONE);
		return;
	}

	if (!actor->target)
	{
		return;
	}
	S_Sound (actor, CHAN_BODY, actor->AttackSound, 1, ATTN_NONE);
	if (actor->CheckMeleeRange())
	{
		int damage = pr_s2a.HitDice (20);
		P_DamageMobj (actor->target, actor, actor, damage, NAME_Melee);
		P_TraceBleed (damage, actor->target, actor);
		return;
	}
	chance = actor->health < actor->GetDefault()->health/2 ? 96 : 48;
	if (pr_s2a() < chance)
	{ // Wizard spawners

		const PClass *fx = PClass::FindClass("Sorcerer2FX2");
		if (fx)
		{
			mo = P_SpawnMissileAngle (actor, fx, actor->angle-ANG45, FRACUNIT/2);

			// [BC]
			if (( mo ) && ( NETWORK_GetState( ) == NETSTATE_SERVER ))
				SERVERCOMMANDS_SpawnMissile( mo );

			mo = P_SpawnMissileAngle (actor, fx, actor->angle+ANG45, FRACUNIT/2);

			// [BC]
			if (( mo ) && ( NETWORK_GetState( ) == NETSTATE_SERVER ))
				SERVERCOMMANDS_SpawnMissile( mo );
		}
	}
	else
	{ // Blue bolt
		mo = P_SpawnMissile (actor, actor->target, PClass::FindClass("Sorcerer2FX1"));
		
		// [BC]
		if (( mo ) && ( NETWORK_GetState( ) == NETSTATE_SERVER ))
			SERVERCOMMANDS_SpawnMissile( mo );

	}
}

//----------------------------------------------------------------------------
//
// PROC A_BlueSpark
//
//----------------------------------------------------------------------------

void A_BlueSpark (AActor *actor)
{
	int i;
	AActor *mo;

	// [BC] Don't do this in client mode.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return;
	}

	for (i = 0; i < 2; i++)
	{
		mo = Spawn("Sorcerer2FXSpark", actor->x, actor->y, actor->z, ALLOW_REPLACE);
		mo->momx = pr_bluespark.Random2() << 9;
		mo->momy = pr_bluespark.Random2() << 9;
		mo->momz = FRACUNIT + (pr_bluespark()<<8);

		// [BC]
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SpawnMissile( mo );
	}
}

//----------------------------------------------------------------------------
//
// PROC A_GenWizard
//
//----------------------------------------------------------------------------

void A_GenWizard (AActor *actor)
{
	AActor *mo;

	// [BC] Don't do this in client mode.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return;
	}

	mo = Spawn("Wizard", actor->x, actor->y, actor->z, ALLOW_REPLACE);
	if (mo != NULL)
	{
		mo->z -= mo->GetDefault()->height/2;
		if (!P_TestMobjLocation (mo))
		{ // Didn't fit
			mo->Destroy ();
			level.total_monsters--;
		}
		else
		{ // [RH] Make the new wizards inherit D'Sparil's target
			mo->CopyFriendliness (actor->target, true);

			actor->momx = actor->momy = actor->momz = 0;
			actor->SetState (actor->FindState(NAME_Death));
			actor->flags &= ~MF_MISSILE;
			mo->master = actor->target;

			// [BC]
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SpawnThing( mo );

			// Heretic did not offset it by TELEFOGHEIGHT, so I won't either.
			mo = Spawn<ATeleportFog> (actor->x, actor->y, actor->z, ALLOW_REPLACE);

			// [BC]
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SpawnThing( mo );
		}
	}
}

//----------------------------------------------------------------------------
//
// PROC A_Sor2DthInit
//
//----------------------------------------------------------------------------

void A_Sor2DthInit (AActor *actor)
{
	actor->special1 = 7; // Animation loop counter
	P_Massacre (); // Kill monsters early
}

//----------------------------------------------------------------------------
//
// PROC A_Sor2DthLoop
//
//----------------------------------------------------------------------------

void A_Sor2DthLoop (AActor *actor)
{
	if (--actor->special1)
	{ // Need to loop
		actor->SetState (actor->FindState("DeathLoop"));
	}
}

