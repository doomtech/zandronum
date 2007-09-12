#include "actor.h"
#include "info.h"
#include "m_random.h"
#include "p_local.h"
#include "p_enemy.h"
#include "gstrings.h"
#include "a_action.h"
#include "s_sound.h"
#include "deathmatch.h"
#include "sv_commands.h"
#include "network.h"

static FRandom pr_headattack ("HeadAttack");

void A_HeadAttack (AActor *);
void A_CacolanternAttack (AActor *);
void A_AbaddonAttack (AActor *);

class ACacodemon : public AActor
{
	DECLARE_ACTOR (ACacodemon, AActor)
};

FState ACacodemon::States[] =
{
#define S_HEAD_STND 0
	S_NORMAL (HEAD, 'A',   10, A_Look						, &States[S_HEAD_STND]),

#define S_HEAD_RUN (S_HEAD_STND+1)
	S_NORMAL (HEAD, 'A',	3, A_Chase						, &States[S_HEAD_RUN+0]),

#define S_HEAD_ATK (S_HEAD_RUN+1)
	S_NORMAL (HEAD, 'B',	5, A_FaceTarget 				, &States[S_HEAD_ATK+1]),
	S_NORMAL (HEAD, 'C',	5, A_FaceTarget 				, &States[S_HEAD_ATK+2]),
	S_BRIGHT (HEAD, 'D',	5, A_HeadAttack 				, &States[S_HEAD_RUN+0]),

#define S_HEAD_PAIN (S_HEAD_ATK+3)
	S_NORMAL (HEAD, 'E',	3, NULL 						, &States[S_HEAD_PAIN+1]),
	S_NORMAL (HEAD, 'E',	3, A_Pain						, &States[S_HEAD_PAIN+2]),
	S_NORMAL (HEAD, 'F',	6, NULL 						, &States[S_HEAD_RUN+0]),

#define S_HEAD_DIE (S_HEAD_PAIN+3)
	S_NORMAL (HEAD, 'G',	8, A_NoBlocking					, &States[S_HEAD_DIE+1]),
	S_NORMAL (HEAD, 'H',	8, A_Scream 					, &States[S_HEAD_DIE+2]),
	S_NORMAL (HEAD, 'I',	8, NULL 						, &States[S_HEAD_DIE+3]),
	S_NORMAL (HEAD, 'J',	8, NULL 						, &States[S_HEAD_DIE+4]),
	S_NORMAL (HEAD, 'K',	8, NULL							, &States[S_HEAD_DIE+5]),
	S_NORMAL (HEAD, 'L',   -1, A_SetFloorClip				, NULL),
/*
	S_NORMAL (HEAD, 'G',	8, NULL							, &States[S_HEAD_DIE+1]),
	S_NORMAL (HEAD, 'H',	8, A_Scream 					, &States[S_HEAD_DIE+2]),
	S_NORMAL (HEAD, 'I',	8, NULL 						, &States[S_HEAD_DIE+3]),
	S_NORMAL (HEAD, 'J',	8, NULL 						, &States[S_HEAD_DIE+4]),
	S_NORMAL (HEAD, 'K',	8, A_NoBlocking					, &States[S_HEAD_DIE+5]),
	S_NORMAL (HEAD, 'L',   -1, A_SetFloorClip				, NULL),
*/
#define S_HEAD_RAISE (S_HEAD_DIE+6)
	S_NORMAL (HEAD, 'L',	8, A_UnSetFloorClip				, &States[S_HEAD_RAISE+1]),
	S_NORMAL (HEAD, 'K',	8, NULL 						, &States[S_HEAD_RAISE+2]),
	S_NORMAL (HEAD, 'J',	8, NULL 						, &States[S_HEAD_RAISE+3]),
	S_NORMAL (HEAD, 'I',	8, NULL 						, &States[S_HEAD_RAISE+4]),
	S_NORMAL (HEAD, 'H',	8, NULL 						, &States[S_HEAD_RAISE+5]),
	S_NORMAL (HEAD, 'G',	8, NULL							, &States[S_HEAD_RUN+0])
};

IMPLEMENT_ACTOR (ACacodemon, Doom, 3005, 19)
	PROP_SpawnHealth (400)
	PROP_RadiusFixed (31)
	PROP_HeightFixed (56)
	PROP_Mass (400)
	PROP_SpeedFixed (8)
	PROP_PainChance (128)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_FLOAT|MF_NOGRAVITY|MF_COUNTKILL)
	PROP_Flags2 (MF2_MCROSS|MF2_PASSMOBJ|MF2_PUSHWALL)
	PROP_FlagsNetwork( NETFL_UPDATEPOSITION )

	PROP_SpawnState (S_HEAD_STND)
	PROP_SeeState (S_HEAD_RUN)
	PROP_PainState (S_HEAD_PAIN)
	PROP_MissileState (S_HEAD_ATK)
	PROP_DeathState (S_HEAD_DIE)
	PROP_RaiseState (S_HEAD_RAISE)

	PROP_SeeSound ("caco/sight")
	PROP_PainSound ("caco/pain")
	PROP_DeathSound ("caco/death")
	PROP_ActiveSound ("caco/active")
	PROP_AttackSound ("caco/melee")
	PROP_Obituary("$OB_CACO")
	PROP_HitObituary("$OB_CACOHIT")
END_DEFAULTS

class ACacolantern : public ACacodemon
{
	DECLARE_ACTOR (ACacolantern, ACacodemon)
};

FState ACacolantern::States[] =
{
#define S_HEAD2_STND 0
	S_NORMAL (HED2, 'A',   10, A_Look						, &States[S_HEAD2_STND]),

#define S_HEAD2_RUN (S_HEAD2_STND+1)
	S_NORMAL (HED2, 'A',	3, A_Chase						, &States[S_HEAD2_RUN+0]),

#define S_HEAD2_ATK (S_HEAD2_RUN+1)
	S_NORMAL (HED2, 'B',	5, A_FaceTarget 				, &States[S_HEAD2_ATK+1]),
	S_NORMAL (HED2, 'C',	5, A_FaceTarget 				, &States[S_HEAD2_ATK+2]),
	S_BRIGHT (HED2, 'D',	5, A_CacolanternAttack			, &States[S_HEAD2_RUN+0]),

#define S_HEAD2_PAIN (S_HEAD2_ATK+3)
	S_NORMAL (HED2, 'E',	3, NULL 						, &States[S_HEAD2_PAIN+1]),
	S_NORMAL (HED2, 'E',	3, A_Pain						, &States[S_HEAD2_PAIN+2]),
	S_NORMAL (HED2, 'F',	6, NULL 						, &States[S_HEAD2_RUN+0]),

#define S_HEAD2_DIE (S_HEAD2_PAIN+3)
	S_NORMAL (HED2, 'G',	8, A_NoBlocking					, &States[S_HEAD2_DIE+1]),
	S_NORMAL (HED2, 'H',	8, A_Scream 					, &States[S_HEAD2_DIE+2]),
	S_NORMAL (HED2, 'I',	8, NULL 						, &States[S_HEAD2_DIE+3]),
	S_NORMAL (HED2, 'J',	8, NULL 						, &States[S_HEAD2_DIE+4]),
	S_NORMAL (HED2, 'K',	8, NULL							, &States[S_HEAD2_DIE+5]),
	S_NORMAL (HED2, 'L',   -1, A_SetFloorClip				, NULL),
/*
	S_NORMAL (HED2, 'G',	8, NULL							, &States[S_HEAD2_DIE+1]),
	S_NORMAL (HED2, 'H',	8, A_Scream 					, &States[S_HEAD2_DIE+2]),
	S_NORMAL (HED2, 'I',	8, NULL 						, &States[S_HEAD2_DIE+3]),
	S_NORMAL (HED2, 'J',	8, NULL 						, &States[S_HEAD2_DIE+4]),
	S_NORMAL (HED2, 'K',	8, A_NoBlocking					, &States[S_HEAD2_DIE+5]),
	S_NORMAL (HED2, 'L',   -1, A_SetFloorClip				, NULL),
*/
#define S_HEAD2_RAISE (S_HEAD2_DIE+6)
	S_NORMAL (HED2, 'L',	8, A_UnSetFloorClip				, &States[S_HEAD2_RAISE+1]),
	S_NORMAL (HED2, 'K',	8, NULL 						, &States[S_HEAD2_RAISE+2]),
	S_NORMAL (HED2, 'J',	8, NULL 						, &States[S_HEAD2_RAISE+3]),
	S_NORMAL (HED2, 'I',	8, NULL 						, &States[S_HEAD2_RAISE+4]),
	S_NORMAL (HED2, 'H',	8, NULL 						, &States[S_HEAD2_RAISE+5]),
	S_NORMAL (HED2, 'G',	8, NULL							, &States[S_HEAD2_RUN+0])
};

IMPLEMENT_ACTOR (ACacolantern, Doom, 5006, 159)
	PROP_SpawnHealth (800)

	PROP_SpawnState (S_HEAD2_STND)
	PROP_SeeState (S_HEAD2_RUN)
	PROP_PainState (S_HEAD2_PAIN)
	PROP_MissileState (S_HEAD2_ATK)
	PROP_DeathState (S_HEAD2_DIE)
	PROP_RaiseState (S_HEAD2_RAISE)

	PROP_Obituary("$OB_CACOLANTERN")
	PROP_HitObituary("$OB_CACOLANTERN_MELEE")
END_DEFAULTS

class AAbaddon : public ACacolantern
{
	DECLARE_ACTOR (AAbaddon, ACacolantern)
};

FState AAbaddon::States[] =
{
#define S_HEAD3_STND 0
	S_NORMAL (HED3, 'A',   10, A_Look						, &States[S_HEAD3_STND]),

#define S_HEAD3_RUN (S_HEAD3_STND+1)
	S_NORMAL (HED3, 'A',	3, A_Chase						, &States[S_HEAD3_RUN+0]),

#define S_HEAD3_ATK (S_HEAD3_RUN+1)
	S_NORMAL (HED3, 'B',	5, A_FaceTarget 				, &States[S_HEAD3_ATK+1]),
	S_NORMAL (HED3, 'C',	5, A_FaceTarget 				, &States[S_HEAD3_ATK+2]),
	S_BRIGHT (HED3, 'D',	5, A_AbaddonAttack				, &States[S_HEAD3_ATK+3]),
	S_NORMAL (HED3, 'B',	5, A_FaceTarget 				, &States[S_HEAD3_ATK+4]),
	S_NORMAL (HED3, 'C',	5, A_FaceTarget 				, &States[S_HEAD3_ATK+5]),
	S_BRIGHT (HED3, 'D',	5, A_AbaddonAttack				, &States[S_HEAD3_RUN+0]),


#define S_HEAD3_PAIN (S_HEAD3_ATK+6)
	S_NORMAL (HED3, 'E',	3, NULL 						, &States[S_HEAD3_PAIN+1]),
	S_NORMAL (HED3, 'E',	3, A_Pain						, &States[S_HEAD3_PAIN+2]),
	S_NORMAL (HED3, 'F',	6, NULL 						, &States[S_HEAD3_RUN+0]),

#define S_HEAD3_DIE (S_HEAD3_PAIN+3)
	S_NORMAL (HED3, 'G',	8, A_NoBlocking					, &States[S_HEAD3_DIE+1]),
	S_NORMAL (HED3, 'H',	8, A_Scream 					, &States[S_HEAD3_DIE+2]),
	S_NORMAL (HED3, 'I',	8, NULL 						, &States[S_HEAD3_DIE+3]),
	S_NORMAL (HED3, 'J',	8, NULL 						, &States[S_HEAD3_DIE+4]),
	S_NORMAL (HED3, 'K',	8, NULL							, &States[S_HEAD3_DIE+5]),
	S_NORMAL (HED3, 'L',   -1, A_SetFloorClip				, NULL),
/*
	S_NORMAL (HED3, 'G',	8, NULL							, &States[S_HEAD3_DIE+1]),
	S_NORMAL (HED3, 'H',	8, A_Scream 					, &States[S_HEAD3_DIE+2]),
	S_NORMAL (HED3, 'I',	8, NULL 						, &States[S_HEAD3_DIE+3]),
	S_NORMAL (HED3, 'J',	8, NULL 						, &States[S_HEAD3_DIE+4]),
	S_NORMAL (HED3, 'K',	8, A_NoBlocking					, &States[S_HEAD3_DIE+5]),
	S_NORMAL (HED3, 'L',   -1, A_SetFloorClip				, NULL),
*/
#define S_HEAD3_RAISE (S_HEAD3_DIE+6)
	S_NORMAL (HED3, 'L',	8, A_UnSetFloorClip				, &States[S_HEAD3_RAISE+1]),
	S_NORMAL (HED3, 'K',	8, NULL 						, &States[S_HEAD3_RAISE+2]),
	S_NORMAL (HED3, 'J',	8, NULL 						, &States[S_HEAD3_RAISE+3]),
	S_NORMAL (HED3, 'I',	8, NULL 						, &States[S_HEAD3_RAISE+4]),
	S_NORMAL (HED3, 'H',	8, NULL 						, &States[S_HEAD3_RAISE+5]),
	S_NORMAL (HED3, 'G',	8, NULL							, &States[S_HEAD3_RUN+0])
};

IMPLEMENT_ACTOR (AAbaddon, Doom, 5015, 220)
	PROP_SpawnHealth (1200)
	PROP_SpeedFixed (12)
	PROP_PainChance (40)

	PROP_SpawnState (S_HEAD3_STND)
	PROP_SeeState (S_HEAD3_RUN)
	PROP_PainState (S_HEAD3_PAIN)
	PROP_MissileState (S_HEAD3_ATK)
	PROP_DeathState (S_HEAD3_DIE)
	PROP_RaiseState (S_HEAD3_RAISE)

	PROP_Obituary("$OB_ABADDON")
	PROP_HitObituary("$OB_ABADDON_MELEE")
END_DEFAULTS

class ACacodemonBall : public AActor
{
	DECLARE_ACTOR (ACacodemonBall, AActor)
};

FState ACacodemonBall::States[] =
{
#define S_RBALL 0
	S_BRIGHT (BAL2, 'A',	4, NULL 						, &States[S_RBALL+1]),
	S_BRIGHT (BAL2, 'B',	4, NULL 						, &States[S_RBALL+0]),

#define S_RBALLX (S_RBALL+2)
	S_BRIGHT (BAL2, 'C',	6, NULL 						, &States[S_RBALLX+1]),
	S_BRIGHT (BAL2, 'D',	6, NULL 						, &States[S_RBALLX+2]),
	S_BRIGHT (BAL2, 'E',	6, NULL 						, NULL)
};

IMPLEMENT_ACTOR (ACacodemonBall, Doom, -1, 126)
	PROP_RadiusFixed (6)
	PROP_HeightFixed (8)
	PROP_SpeedFixed (10)
	PROP_Damage (5)
	PROP_Flags (MF_NOBLOCKMAP|MF_MISSILE|MF_DROPOFF|MF_NOGRAVITY)
	PROP_Flags2 (MF2_PCROSS|MF2_IMPACT|MF2_NOTELEPORT)
	PROP_Flags4 (MF4_RANDOMIZE)
	PROP_RenderStyle (STYLE_Add)

	PROP_SpawnState (S_RBALL)
	PROP_DeathState (S_RBALLX)

	PROP_SeeSound ("caco/attack")
	PROP_DeathSound ("caco/shotx")
END_DEFAULTS

AT_SPEED_SET (CacodemonBall, speed)
{
	SimpleSpeedSetter (ACacodemonBall, 10*FRACUNIT, 20*FRACUNIT, speed);
}

class ACacolanternBall : public AActor
{
	DECLARE_ACTOR (ACacolanternBall, AActor)
};

FState ACacolanternBall::States[] =
{
#define S_CLBALL 0
	S_BRIGHT (BAL8, 'A',	4, NULL 						, &States[S_CLBALL+1]),
	S_BRIGHT (BAL8, 'B',	4, NULL 						, &States[S_CLBALL+0]),

#define S_CLBALLX (S_CLBALL+2)
	S_BRIGHT (BAL8, 'C',	6, NULL 						, &States[S_CLBALLX+1]),
	S_BRIGHT (BAL8, 'D',	6, NULL 						, &States[S_CLBALLX+2]),
	S_BRIGHT (BAL8, 'E',	6, NULL 						, NULL)
};

IMPLEMENT_ACTOR (ACacolanternBall, Doom, -1, 219)
	PROP_RadiusFixed (6)
	PROP_HeightFixed (8)
	PROP_SpeedFixed (20)
	PROP_Damage (5)
	PROP_Flags (MF_NOBLOCKMAP|MF_MISSILE|MF_DROPOFF|MF_NOGRAVITY)
	PROP_Flags2 (MF2_PCROSS|MF2_IMPACT|MF2_NOTELEPORT)
	PROP_Flags4 (MF4_RANDOMIZE)
	PROP_RenderStyle (STYLE_Add)

	PROP_SpawnState (S_CLBALL)
	PROP_DeathState (S_CLBALLX)

	PROP_SeeSound ("caco/attack")
	PROP_DeathSound ("caco/shotx")
END_DEFAULTS

AT_SPEED_SET (CacolanternBall, speed)
{
	SimpleSpeedSetter (ACacodemonBall, 20*FRACUNIT, 20*FRACUNIT, speed);
}

class AAbaddonBall : public AActor
{
	DECLARE_ACTOR (AAbaddonBall, AActor)
};

FState AAbaddonBall::States[] =
{
#define S_ABBALL 0
	S_BRIGHT (BAL3, 'A',	4, NULL 						, &States[S_ABBALL+1]),
	S_BRIGHT (BAL3, 'B',	4, NULL 						, &States[S_ABBALL+0]),

#define S_ABBALLX (S_ABBALL+2)
	S_BRIGHT (BAL3, 'C',	6, NULL 						, &States[S_ABBALLX+1]),
	S_BRIGHT (BAL3, 'D',	6, NULL 						, &States[S_ABBALLX+2]),
	S_BRIGHT (BAL3, 'E',	6, NULL 						, NULL)
};

IMPLEMENT_ACTOR (AAbaddonBall, Doom, -1, 221)
	PROP_RadiusFixed (6)
	PROP_HeightFixed (8)
	PROP_SpeedFixed (20)
	PROP_Damage (10)
	PROP_Flags (MF_NOBLOCKMAP|MF_MISSILE|MF_DROPOFF|MF_NOGRAVITY)
	PROP_Flags2 (MF2_PCROSS|MF2_IMPACT|MF2_NOTELEPORT)
	PROP_Flags4 (MF4_RANDOMIZE)
	PROP_RenderStyle (STYLE_Add)

	PROP_SpawnState (S_ABBALL)
	PROP_DeathState (S_ABBALLX)

	PROP_SeeSound ("caco/attack")
	PROP_DeathSound ("caco/shotx")
END_DEFAULTS

AT_SPEED_SET (AbaddonBall, speed)
{
	SimpleSpeedSetter (AAbaddonBall, 20*FRACUNIT, 20*FRACUNIT, speed);
}

void A_HeadAttack (AActor *self)
{
	AActor	*pMissile;

	if (!self->target)
		return;
				
	A_FaceTarget (self);
	if (self->CheckMeleeRange ())
	{
		int damage = (pr_headattack()%6+1)*10;
		S_SoundID (self, CHAN_WEAPON, self->AttackSound, 1, ATTN_NORM);

		// [BC] If we're the server, tell clients play this sound.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SoundIDActor( self, CHAN_WEAPON, self->AttackSound, 127, ATTN_NORM );

		P_DamageMobj (self->target, self, self, damage, NAME_Melee);
		P_TraceBleed (damage, self->target, self);
		return;
	}
	
	// launch a missile
	pMissile = P_SpawnMissile (self, self->target, RUNTIME_CLASS(ACacodemonBall));

	// [BC] If we're the server, tell clients to spawn the missile.
	if (( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( pMissile ))
		SERVERCOMMANDS_SpawnMissile( pMissile );
}

void A_CacolanternAttack (AActor *self)
{
	AActor	*pMissile;

	if (!self->target)
		return;
				
	A_FaceTarget (self);
	if (self->CheckMeleeRange ())
	{
		int damage = (pr_headattack()%6+1)*10;
		S_SoundID (self, CHAN_WEAPON, self->AttackSound, 1, ATTN_NORM);

		// [BC] If we're the server, tell clients play this sound.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SoundIDActor( self, CHAN_WEAPON, self->AttackSound, 127, ATTN_NORM );

		P_DamageMobj (self->target, self, self, damage, NAME_Melee);
		P_TraceBleed (damage, self->target, self);
		return;
	}
	
	// launch a missile
	pMissile = P_SpawnMissile (self, self->target, RUNTIME_CLASS(ACacolanternBall));

	// [BC] If we're the server, tell clients to spawn the missile.
	if (( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( pMissile ))
		SERVERCOMMANDS_SpawnMissile( pMissile );
}

void A_AbaddonAttack (AActor *self)
{
	AActor	*pMissile;

	if (!self->target)
		return;
				
	A_FaceTarget (self);
	if (self->CheckMeleeRange ())
	{
		int damage = (pr_headattack()%6+1)*10;
		S_SoundID (self, CHAN_WEAPON, self->AttackSound, 1, ATTN_NORM);

		// [BC] If we're the server, tell clients play this sound.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SoundIDActor( self, CHAN_WEAPON, self->AttackSound, 127, ATTN_NORM );

		P_DamageMobj (self->target, self, self, damage, NAME_Melee);
		P_TraceBleed (damage, self->target, self);
		return;
	}
	
	// launch a missile
	pMissile = P_SpawnMissile (self, self->target, RUNTIME_CLASS(AAbaddonBall));

	// [BC] If we're the server, tell clients to spawn the missile.
	if (( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( pMissile ))
		SERVERCOMMANDS_SpawnMissile( pMissile );
}
