#include "actor.h"
#include "info.h"
#include "m_random.h"
#include "s_sound.h"
#include "p_local.h"
#include "p_enemy.h"
#include "a_action.h"
#include "a_sharedglobal.h"
#include "gstrings.h"
// [BB] New #includes.
#include "sv_commands.h"
#include "cl_demo.h"

static FRandom pr_dripblood ("DripBlood");
static FRandom pr_knightatk ("KnightAttack");

//----------------------------------------------------------------------------
//
// PROC A_DripBlood
//
//----------------------------------------------------------------------------

void A_DripBlood (AActor *actor)
{
	AActor *mo;
	fixed_t x, y;

	x = actor->x + (pr_dripblood.Random2 () << 11);
	y = actor->y + (pr_dripblood.Random2 () << 11);
	mo = Spawn ("Blood", x, y, actor->z, ALLOW_REPLACE);
	mo->momx = pr_dripblood.Random2 () << 10;
	mo->momy = pr_dripblood.Random2 () << 10;
	mo->gravity = FRACUNIT/8;
}

//----------------------------------------------------------------------------
//
// PROC A_KnightAttack
//
//----------------------------------------------------------------------------

void A_KnightAttack (AActor *actor)
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
	if (actor->CheckMeleeRange ())
	{
		int damage = pr_knightatk.HitDice (3);
		P_DamageMobj (actor->target, actor, actor, damage, NAME_Melee);
		P_TraceBleed (damage, actor->target, actor);
		S_Sound (actor, CHAN_BODY, "hknight/melee", 1, ATTN_NORM);

		// [BB] If we're the server, tell the clients to play the sound.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SoundActor( actor, CHAN_BODY, "hknight/melee", 1, ATTN_NORM );

		return;
	}
	// Throw axe
	S_Sound (actor, CHAN_BODY, actor->AttackSound, 1, ATTN_NORM);
	if (actor->flags & MF_SHADOW || pr_knightatk () < 40)
	{ // Red axe
		AActor *missile = P_SpawnMissileZ (actor, actor->z + 36*FRACUNIT, actor->target, PClass::FindClass("RedAxe"));

		// [BB] If we're the server, tell the clients to spawn this missile.
		if ( ( NETWORK_GetState( ) == NETSTATE_SERVER ) && missile )
			SERVERCOMMANDS_SpawnMissile( missile );

		return;
	}
	// Green axe
	AActor *missile = P_SpawnMissileZ (actor, actor->z + 36*FRACUNIT, actor->target, PClass::FindClass("KnightAxe"));

	// [BB] If we're the server, tell the clients to spawn this missile.
	if ( ( NETWORK_GetState( ) == NETSTATE_SERVER ) && missile )
		SERVERCOMMANDS_SpawnMissile( missile );
}

