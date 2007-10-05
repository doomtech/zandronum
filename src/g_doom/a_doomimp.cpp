#include "actor.h"
#include "info.h"
#include "m_random.h"
#include "s_sound.h"
#include "p_local.h"
#include "p_enemy.h"
#include "gstrings.h"
#include "a_action.h"
#include "sv_commands.h"
#include "network.h"

static FRandom pr_troopattack ("TroopAttack");

void A_TroopAttack (AActor *);
void A_DarkImpAttack (AActor *);

class ADoomImp : public AActor
{
	DECLARE_ACTOR (ADoomImp, AActor)
};

FState ADoomImp::States[] =
{
#define S_TROO_STND 0
	S_NORMAL (TROO, 'A',   10, A_Look						, &States[S_TROO_STND+1]),
	S_NORMAL (TROO, 'B',   10, A_Look						, &States[S_TROO_STND]),

#define S_TROO_RUN (S_TROO_STND+2)
	S_NORMAL (TROO, 'A',	3, A_Chase						, &States[S_TROO_RUN+1]),
	S_NORMAL (TROO, 'A',	3, A_Chase						, &States[S_TROO_RUN+2]),
	S_NORMAL (TROO, 'B',	3, A_Chase						, &States[S_TROO_RUN+3]),
	S_NORMAL (TROO, 'B',	3, A_Chase						, &States[S_TROO_RUN+4]),
	S_NORMAL (TROO, 'C',	3, A_Chase						, &States[S_TROO_RUN+5]),
	S_NORMAL (TROO, 'C',	3, A_Chase						, &States[S_TROO_RUN+6]),
	S_NORMAL (TROO, 'D',	3, A_Chase						, &States[S_TROO_RUN+7]),
	S_NORMAL (TROO, 'D',	3, A_Chase						, &States[S_TROO_RUN+0]),

#define S_TROO_ATK (S_TROO_RUN+8)
	S_NORMAL (TROO, 'E',	8, A_FaceTarget 				, &States[S_TROO_ATK+1]),
	S_NORMAL (TROO, 'F',	8, A_FaceTarget 				, &States[S_TROO_ATK+2]),
	S_NORMAL (TROO, 'G',	6, A_TroopAttack				, &States[S_TROO_RUN+0]),

#define S_TROO_PAIN (S_TROO_ATK+3)
	S_NORMAL (TROO, 'H',	2, NULL 						, &States[S_TROO_PAIN+1]),
	S_NORMAL (TROO, 'H',	2, A_Pain						, &States[S_TROO_RUN+0]),

#define S_TROO_DIE (S_TROO_PAIN+2)
	S_NORMAL (TROO, 'I',	8, NULL 						, &States[S_TROO_DIE+1]),
	S_NORMAL (TROO, 'J',	8, A_Scream 					, &States[S_TROO_DIE+2]),
	S_NORMAL (TROO, 'K',	6, NULL 						, &States[S_TROO_DIE+3]),
	S_NORMAL (TROO, 'L',	6, A_NoBlocking					, &States[S_TROO_DIE+4]),
	S_NORMAL (TROO, 'M',   -1, NULL 						, NULL),

#define S_TROO_XDIE (S_TROO_DIE+5)
	S_NORMAL (TROO, 'N',	5, NULL 						, &States[S_TROO_XDIE+1]),
	S_NORMAL (TROO, 'O',	5, A_XScream					, &States[S_TROO_XDIE+2]),
	S_NORMAL (TROO, 'P',	5, NULL 						, &States[S_TROO_XDIE+3]),
	S_NORMAL (TROO, 'Q',	5, A_NoBlocking					, &States[S_TROO_XDIE+4]),
	S_NORMAL (TROO, 'R',	5, NULL 						, &States[S_TROO_XDIE+5]),
	S_NORMAL (TROO, 'S',	5, NULL 						, &States[S_TROO_XDIE+6]),
	S_NORMAL (TROO, 'T',	5, NULL 						, &States[S_TROO_XDIE+7]),
	S_NORMAL (TROO, 'U',   -1, NULL 						, NULL),

#define S_TROO_RAISE (S_TROO_XDIE+8)
	S_NORMAL (TROO, 'M',	8, NULL 						, &States[S_TROO_RAISE+1]),
	S_NORMAL (TROO, 'L',	8, NULL 						, &States[S_TROO_RAISE+2]),
	S_NORMAL (TROO, 'K',	6, NULL 						, &States[S_TROO_RAISE+3]),
	S_NORMAL (TROO, 'J',	6, NULL 						, &States[S_TROO_RAISE+4]),
	S_NORMAL (TROO, 'I',	6, NULL 						, &States[S_TROO_RUN+0])
};

IMPLEMENT_ACTOR (ADoomImp, Doom, 3001, 5)
	PROP_SpawnHealth (60)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (56)
	PROP_Mass (100)
	PROP_SpeedFixed (8)
	PROP_PainChance (200)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_COUNTKILL)
	PROP_Flags2 (MF2_MCROSS|MF2_PASSMOBJ|MF2_PUSHWALL|MF2_FLOORCLIP)

	PROP_SpawnState (S_TROO_STND)
	PROP_SeeState (S_TROO_RUN)
	PROP_PainState (S_TROO_PAIN)
	PROP_MeleeState (S_TROO_ATK)
	PROP_MissileState (S_TROO_ATK)
	PROP_DeathState (S_TROO_DIE)
	PROP_XDeathState (S_TROO_XDIE)
	PROP_RaiseState (S_TROO_RAISE)

	PROP_SeeSound ("imp/sight")
	PROP_PainSound ("imp/pain")
	PROP_DeathSound ("imp/death")
	PROP_ActiveSound ("imp/active")
	PROP_Obituary("$OB_IMP")
	PROP_HitObituary("$OB_IMPHIT")
END_DEFAULTS

class ADarkImp : public ADoomImp
{
	DECLARE_ACTOR (ADarkImp, ADoomImp)
};

FState ADarkImp::States[] =
{
#define S_DIMP_STND 0
	S_NORMAL (DIMP, 'A',   10, A_Look						, &States[S_DIMP_STND+1]),
	S_NORMAL (DIMP, 'B',   10, A_Look						, &States[S_DIMP_STND]),

#define S_DIMP_RUN (S_DIMP_STND+2)
	S_NORMAL (DIMP, 'A',	3, A_Chase						, &States[S_DIMP_RUN+1]),
	S_NORMAL (DIMP, 'A',	3, A_Chase						, &States[S_DIMP_RUN+2]),
	S_NORMAL (DIMP, 'B',	3, A_Chase						, &States[S_DIMP_RUN+3]),
	S_NORMAL (DIMP, 'B',	3, A_Chase						, &States[S_DIMP_RUN+4]),
	S_NORMAL (DIMP, 'C',	3, A_Chase						, &States[S_DIMP_RUN+5]),
	S_NORMAL (DIMP, 'C',	3, A_Chase						, &States[S_DIMP_RUN+6]),
	S_NORMAL (DIMP, 'D',	3, A_Chase						, &States[S_DIMP_RUN+7]),
	S_NORMAL (DIMP, 'D',	3, A_Chase						, &States[S_DIMP_RUN+0]),

#define S_DIMP_ATK (S_DIMP_RUN+8)
	S_NORMAL (DIMP, 'E',	8, A_FaceTarget 				, &States[S_DIMP_ATK+1]),
	S_NORMAL (DIMP, 'F',	8, A_FaceTarget 				, &States[S_DIMP_ATK+2]),
	S_NORMAL (DIMP, 'G',	6, A_DarkImpAttack				, &States[S_DIMP_RUN+0]),

#define S_DIMP_PAIN (S_DIMP_ATK+3)
	S_NORMAL (DIMP, 'H',	2, NULL 						, &States[S_DIMP_PAIN+1]),
	S_NORMAL (DIMP, 'H',	2, A_Pain						, &States[S_DIMP_RUN+0]),

#define S_DIMP_DIE (S_DIMP_PAIN+2)
	S_NORMAL (DIMP, 'I',	8, NULL 						, &States[S_DIMP_DIE+1]),
	S_NORMAL (DIMP, 'J',	8, A_Scream 					, &States[S_DIMP_DIE+2]),
	S_NORMAL (DIMP, 'K',	6, NULL 						, &States[S_DIMP_DIE+3]),
	S_NORMAL (DIMP, 'L',	6, A_NoBlocking					, &States[S_DIMP_DIE+4]),
	S_NORMAL (DIMP, 'M',   -1, NULL 						, NULL),

#define S_DIMP_XDIE (S_DIMP_DIE+5)
	S_NORMAL (DIMP, 'N',	5, NULL 						, &States[S_DIMP_XDIE+1]),
	S_NORMAL (DIMP, 'O',	5, A_XScream					, &States[S_DIMP_XDIE+2]),
	S_NORMAL (DIMP, 'P',	5, NULL 						, &States[S_DIMP_XDIE+3]),
	S_NORMAL (DIMP, 'Q',	5, A_NoBlocking					, &States[S_DIMP_XDIE+4]),
	S_NORMAL (DIMP, 'R',	5, NULL 						, &States[S_DIMP_XDIE+5]),
	S_NORMAL (DIMP, 'S',	5, NULL 						, &States[S_DIMP_XDIE+6]),
	S_NORMAL (DIMP, 'T',	5, NULL 						, &States[S_DIMP_XDIE+7]),
	S_NORMAL (DIMP, 'U',   -1, NULL 						, NULL),

#define S_DIMP_RAISE (S_DIMP_XDIE+8)
	S_NORMAL (DIMP, 'M',	8, NULL 						, &States[S_DIMP_RAISE+1]),
	S_NORMAL (DIMP, 'L',	8, NULL 						, &States[S_DIMP_RAISE+2]),
	S_NORMAL (DIMP, 'K',	6, NULL 						, &States[S_DIMP_RAISE+3]),
	S_NORMAL (DIMP, 'J',	6, NULL 						, &States[S_DIMP_RAISE+4]),
	S_NORMAL (DIMP, 'I',	6, NULL 						, &States[S_DIMP_RUN+0])
};

IMPLEMENT_ACTOR (ADarkImp, Doom, 5003, 155)
	PROP_SpawnHealth (120)

	PROP_SpawnState (S_DIMP_STND)
	PROP_SeeState (S_DIMP_RUN)
	PROP_PainState (S_DIMP_PAIN)
	PROP_MeleeState (S_DIMP_ATK)
	PROP_MissileState (S_DIMP_ATK)
	PROP_DeathState (S_DIMP_DIE)
	PROP_XDeathState (S_DIMP_XDIE)
	PROP_RaiseState (S_DIMP_RAISE)
	PROP_Obituary("$OB_DARKIMP")
	PROP_HitObituary("$OB_DARKIMP_MELEE")
END_DEFAULTS

class ADoomImpBall : public AActor
{
	DECLARE_ACTOR (ADoomImpBall, AActor)
};

FState ADoomImpBall::States[] =
{
#define S_TBALL 0
	S_BRIGHT (BAL1, 'A',	4, NULL 						, &States[S_TBALL+1]),
	S_BRIGHT (BAL1, 'B',	4, NULL 						, &States[S_TBALL+0]),

#define S_TBALLX (S_TBALL+2)
	S_BRIGHT (BAL1, 'C',	6, NULL 						, &States[S_TBALLX+1]),
	S_BRIGHT (BAL1, 'D',	6, NULL 						, &States[S_TBALLX+2]),
	S_BRIGHT (BAL1, 'E',	6, NULL 						, NULL)
};

IMPLEMENT_ACTOR (ADoomImpBall, Doom, -1, 10)
	PROP_RadiusFixed (6)
	PROP_HeightFixed (8)
	PROP_SpeedFixed (10)
	PROP_Damage (3)
	PROP_Flags (MF_NOBLOCKMAP|MF_MISSILE|MF_DROPOFF|MF_NOGRAVITY)
	PROP_Flags2 (MF2_PCROSS|MF2_IMPACT|MF2_NOTELEPORT)
	PROP_Flags4 (MF4_RANDOMIZE)
	PROP_RenderStyle (STYLE_Add)

	PROP_SpawnState (S_TBALL)
	PROP_DeathState (S_TBALLX)

	PROP_SeeSound ("imp/attack")
	PROP_DeathSound ("imp/shotx")
END_DEFAULTS

AT_SPEED_SET (ADoomImpBall, speed)
{
	SimpleSpeedSetter (ADoomImpBall, 10*FRACUNIT, 20*FRACUNIT, speed);
}

class ADarkImpBall : public ADoomImpBall
{
	DECLARE_ACTOR (ADarkImpBall, ADoomImpBall)
};

FState ADarkImpBall::States[] =
{
#define S_DARKIMPBALL 0
	S_BRIGHT (BAL4, 'A',	4, NULL 						, &States[S_DARKIMPBALL+1]),
	S_BRIGHT (BAL4, 'B',	4, NULL 						, &States[S_DARKIMPBALL+0]),

#define S_DARKIMPBALLX (S_DARKIMPBALL+2)
	S_BRIGHT (BAL4, 'C',	6, NULL 						, &States[S_DARKIMPBALLX+1]),
	S_BRIGHT (BAL4, 'D',	6, NULL 						, &States[S_DARKIMPBALLX+2]),
	S_BRIGHT (BAL4, 'E',	6, NULL 						, NULL)
};

IMPLEMENT_ACTOR (ADarkImpBall, Doom, -1, 218)
	PROP_SpeedFixed (20)

	PROP_SpawnState (S_DARKIMPBALL)
	PROP_DeathState (S_DARKIMPBALLX)
END_DEFAULTS

//
// A_TroopAttack
//
void A_TroopAttack (AActor *self)
{
	AActor	*pMissile;

	if (!self->target)
		return;
				
	A_FaceTarget (self);
	if (self->CheckMeleeRange ())
	{
		int damage = (pr_troopattack()%8+1)*3;
		S_Sound (self, CHAN_WEAPON, "imp/melee", 1, ATTN_NORM);

		// [BC] If we're the server, tell clients play this sound.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SoundActor( self, CHAN_WEAPON, "imp/melee", 1, ATTN_NORM );

		P_DamageMobj (self->target, self, self, damage, NAME_Melee);
		P_TraceBleed (damage, self->target, self);
		return;
	}
	
	// launch a missile
	pMissile = P_SpawnMissile (self, self->target, RUNTIME_CLASS(ADoomImpBall));
	
	// [BC] If we're the server, tell clients to spawn the missile.
	if (( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( pMissile ))
		SERVERCOMMANDS_SpawnMissile( pMissile );
}

//
// A_DarkImpAttack
//
void A_DarkImpAttack (AActor *self)
{
	AActor	*pMissile;

	if (!self->target)
		return;
				
	A_FaceTarget (self);
	if (self->CheckMeleeRange ())
	{
		int damage = (pr_troopattack()%8+1)*3;
		S_Sound (self, CHAN_WEAPON, "imp/melee", 1, ATTN_NORM);

		// [BC] If we're the server, tell clients play this sound.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SoundActor( self, CHAN_WEAPON, "imp/melee", 1, ATTN_NORM );

		P_DamageMobj (self->target, self, self, damage, NAME_Melee);
		P_TraceBleed (damage, self->target, self);
		return;
	}

	// launch a missile
	pMissile = P_SpawnMissile (self, self->target, RUNTIME_CLASS(ADarkImpBall));

	// [BC] If we're the server, tell clients to spawn the missile.
	if (( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( pMissile ))
		SERVERCOMMANDS_SpawnMissile( pMissile );
}
