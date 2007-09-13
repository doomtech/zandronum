#include "actor.h"
#include "info.h"
#include "m_random.h"
#include "s_sound.h"
#include "p_local.h"
#include "p_enemy.h"
#include "gstrings.h"
#include "a_action.h"
#include "a_doomglobal.h"
#include "deathmatch.h"

static FRandom pr_posattack ("PosAttack");
static FRandom pr_sposattack ("SPosAttack");
static FRandom pr_cposattack ("CPosAttack");
static FRandom pr_gposattack ("GPosAttack");
static FRandom pr_cposrefire ("CPosRefire");

void A_PosAttack (AActor *);
void A_SPosAttackUseAtkSound (AActor *);
void A_GPosAttack (AActor *);
void A_CPosAttack (AActor *);
void A_CPosRefire (AActor *);

// Zombie man --------------------------------------------------------------

class AZombieMan : public AActor
{
	DECLARE_ACTOR (AZombieMan, AActor)
public:
	void NoBlockingSet ();
};

FState AZombieMan::States[] =
{
#define S_POSS_STND 0
	S_NORMAL (POSS, 'A',   10, A_Look						, &States[S_POSS_STND+1]),
	S_NORMAL (POSS, 'B',   10, A_Look						, &States[S_POSS_STND]),

#define S_POSS_RUN (S_POSS_STND+2)
	S_NORMAL (POSS, 'A',	4, A_Chase						, &States[S_POSS_RUN+1]),
	S_NORMAL (POSS, 'A',	4, A_Chase						, &States[S_POSS_RUN+2]),
	S_NORMAL (POSS, 'B',	4, A_Chase						, &States[S_POSS_RUN+3]),
	S_NORMAL (POSS, 'B',	4, A_Chase						, &States[S_POSS_RUN+4]),
	S_NORMAL (POSS, 'C',	4, A_Chase						, &States[S_POSS_RUN+5]),
	S_NORMAL (POSS, 'C',	4, A_Chase						, &States[S_POSS_RUN+6]),
	S_NORMAL (POSS, 'D',	4, A_Chase						, &States[S_POSS_RUN+7]),
	S_NORMAL (POSS, 'D',	4, A_Chase						, &States[S_POSS_RUN+0]),

#define S_POSS_ATK (S_POSS_RUN+8)
	S_NORMAL (POSS, 'E',   10, A_FaceTarget 				, &States[S_POSS_ATK+1]),
	S_NORMAL (POSS, 'F',	8, A_PosAttack					, &States[S_POSS_ATK+2]),
	S_NORMAL (POSS, 'E',	8, NULL 						, &States[S_POSS_RUN+0]),

#define S_POSS_PAIN (S_POSS_ATK+3)
	S_NORMAL (POSS, 'G',	3, NULL 						, &States[S_POSS_PAIN+1]),
	S_NORMAL (POSS, 'G',	3, A_Pain						, &States[S_POSS_RUN+0]),

#define S_POSS_DIE (S_POSS_PAIN+2)
	S_NORMAL (POSS, 'H',	5, NULL 						, &States[S_POSS_DIE+1]),
	S_NORMAL (POSS, 'I',	5, A_Scream 					, &States[S_POSS_DIE+2]),
	S_NORMAL (POSS, 'J',	5, A_NoBlocking					, &States[S_POSS_DIE+3]),
	S_NORMAL (POSS, 'K',	5, NULL 						, &States[S_POSS_DIE+4]),
	S_NORMAL (POSS, 'L',   -1, NULL 						, NULL),

#define S_POSS_XDIE (S_POSS_DIE+5)
	S_NORMAL (POSS, 'M',	5, NULL 						, &States[S_POSS_XDIE+1]),
	S_NORMAL (POSS, 'N',	5, A_XScream					, &States[S_POSS_XDIE+2]),
	S_NORMAL (POSS, 'O',	5, A_NoBlocking					, &States[S_POSS_XDIE+3]),
	S_NORMAL (POSS, 'P',	5, NULL 						, &States[S_POSS_XDIE+4]),
	S_NORMAL (POSS, 'Q',	5, NULL 						, &States[S_POSS_XDIE+5]),
	S_NORMAL (POSS, 'R',	5, NULL 						, &States[S_POSS_XDIE+6]),
	S_NORMAL (POSS, 'S',	5, NULL 						, &States[S_POSS_XDIE+7]),
	S_NORMAL (POSS, 'T',	5, NULL 						, &States[S_POSS_XDIE+8]),
	S_NORMAL (POSS, 'U',   -1, NULL 						, NULL),

#define S_POSS_RAISE (S_POSS_XDIE+9)
	S_NORMAL (POSS, 'K',	5, NULL 						, &States[S_POSS_RAISE+1]),
	S_NORMAL (POSS, 'J',	5, NULL 						, &States[S_POSS_RAISE+2]),
	S_NORMAL (POSS, 'I',	5, NULL 						, &States[S_POSS_RAISE+3]),
	S_NORMAL (POSS, 'H',	5, NULL 						, &States[S_POSS_RUN+0])
};

IMPLEMENT_ACTOR (AZombieMan, Doom, 3004, 4)
	PROP_SpawnHealth (20)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (56)
	PROP_Mass (100)
	PROP_SpeedFixed (8)
	PROP_PainChance (200)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_COUNTKILL)
	PROP_Flags2 (MF2_MCROSS|MF2_PASSMOBJ|MF2_PUSHWALL|MF2_FLOORCLIP)
	PROP_FlagsNetwork( NETFL_UPDATEPOSITION )

	PROP_SpawnState (S_POSS_STND)
	PROP_SeeState (S_POSS_RUN)
	PROP_PainState (S_POSS_PAIN)
	PROP_MissileState (S_POSS_ATK)
	PROP_DeathState (S_POSS_DIE)
	PROP_XDeathState (S_POSS_XDIE)
	PROP_RaiseState (S_POSS_RAISE)

	PROP_SeeSound ("grunt/sight")
	PROP_AttackSound ("grunt/attack")
	PROP_PainSound ("grunt/pain")
	PROP_DeathSound ("grunt/death")
	PROP_ActiveSound ("grunt/active")
	PROP_Obituary("$OB_ZOMBIE")
END_DEFAULTS

void AZombieMan::NoBlockingSet ()
{
	P_DropItem (this, "Clip", -1, 256);
}

//
// A_PosAttack
//
void A_PosAttack (AActor *self)
{
	int angle;
	int damage;
	int slope;
		
	// [BC] Server takes care of the rest of this.
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
	{
		S_Sound( self, CHAN_WEAPON, "grunt/attack", 1, ATTN_NORM );
		return;
	}

	if (!self->target)
		return;
				
	A_FaceTarget (self);
	angle = self->angle;
	slope = P_AimLineAttack (self, angle, MISSILERANGE);

	S_Sound (self, CHAN_WEAPON, "grunt/attack", 1, ATTN_NORM);
	angle += pr_posattack.Random2() << 20;
	damage = ((pr_posattack()%5)+1)*3;
	P_LineAttack (self, angle, MISSILERANGE, slope, damage, NAME_None, NAME_BulletPuff);
}

// Shotgun guy -------------------------------------------------------------

class AShotgunGuy : public AActor
{
	DECLARE_ACTOR (AShotgunGuy, AActor)
public:
	void NoBlockingSet ();
};

FState AShotgunGuy::States[] =
{

#define S_SPOS_STND 0
	S_NORMAL (SPOS, 'A',   10, A_Look						, &States[S_SPOS_STND+1]),
	S_NORMAL (SPOS, 'B',   10, A_Look						, &States[S_SPOS_STND]),

#define S_SPOS_RUN (S_SPOS_STND+2)
	S_NORMAL (SPOS, 'A',	3, A_Chase						, &States[S_SPOS_RUN+1]),
	S_NORMAL (SPOS, 'A',	3, A_Chase						, &States[S_SPOS_RUN+2]),
	S_NORMAL (SPOS, 'B',	3, A_Chase						, &States[S_SPOS_RUN+3]),
	S_NORMAL (SPOS, 'B',	3, A_Chase						, &States[S_SPOS_RUN+4]),
	S_NORMAL (SPOS, 'C',	3, A_Chase						, &States[S_SPOS_RUN+5]),
	S_NORMAL (SPOS, 'C',	3, A_Chase						, &States[S_SPOS_RUN+6]),
	S_NORMAL (SPOS, 'D',	3, A_Chase						, &States[S_SPOS_RUN+7]),
	S_NORMAL (SPOS, 'D',	3, A_Chase						, &States[S_SPOS_RUN+0]),

#define S_SPOS_ATK (S_SPOS_RUN+8)
	S_NORMAL (SPOS, 'E',   10, A_FaceTarget 				, &States[S_SPOS_ATK+1]),
	S_BRIGHT (SPOS, 'F',   10, A_SPosAttackUseAtkSound		, &States[S_SPOS_ATK+2]),
	S_NORMAL (SPOS, 'E',   10, NULL 						, &States[S_SPOS_RUN+0]),

#define S_SPOS_PAIN (S_SPOS_ATK+3)
	S_NORMAL (SPOS, 'G',	3, NULL 						, &States[S_SPOS_PAIN+1]),
	S_NORMAL (SPOS, 'G',	3, A_Pain						, &States[S_SPOS_RUN+0]),

#define S_SPOS_DIE (S_SPOS_PAIN+2)
	S_NORMAL (SPOS, 'H',	5, NULL 						, &States[S_SPOS_DIE+1]),
	S_NORMAL (SPOS, 'I',	5, A_Scream 					, &States[S_SPOS_DIE+2]),
	S_NORMAL (SPOS, 'J',	5, A_NoBlocking					, &States[S_SPOS_DIE+3]),
	S_NORMAL (SPOS, 'K',	5, NULL 						, &States[S_SPOS_DIE+4]),
	S_NORMAL (SPOS, 'L',   -1, NULL 						, NULL),

#define S_SPOS_XDIE (S_SPOS_DIE+5)
	S_NORMAL (SPOS, 'M',	5, NULL 						, &States[S_SPOS_XDIE+1]),
	S_NORMAL (SPOS, 'N',	5, A_XScream					, &States[S_SPOS_XDIE+2]),
	S_NORMAL (SPOS, 'O',	5, A_NoBlocking					, &States[S_SPOS_XDIE+3]),
	S_NORMAL (SPOS, 'P',	5, NULL 						, &States[S_SPOS_XDIE+4]),
	S_NORMAL (SPOS, 'Q',	5, NULL 						, &States[S_SPOS_XDIE+5]),
	S_NORMAL (SPOS, 'R',	5, NULL 						, &States[S_SPOS_XDIE+6]),
	S_NORMAL (SPOS, 'S',	5, NULL 						, &States[S_SPOS_XDIE+7]),
	S_NORMAL (SPOS, 'T',	5, NULL 						, &States[S_SPOS_XDIE+8]),
	S_NORMAL (SPOS, 'U',   -1, NULL 						, NULL),

#define S_SPOS_RAISE (S_SPOS_XDIE+9)
	S_NORMAL (SPOS, 'L',	5, NULL 						, &States[S_SPOS_RAISE+1]),
	S_NORMAL (SPOS, 'K',	5, NULL 						, &States[S_SPOS_RAISE+2]),
	S_NORMAL (SPOS, 'J',	5, NULL 						, &States[S_SPOS_RAISE+3]),
	S_NORMAL (SPOS, 'I',	5, NULL 						, &States[S_SPOS_RAISE+4]),
	S_NORMAL (SPOS, 'H',	5, NULL 						, &States[S_SPOS_RUN+0])
};

IMPLEMENT_ACTOR (AShotgunGuy, Doom, 9, 1)
	PROP_SpawnHealth (30)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (56)
	PROP_Mass (100)
	PROP_SpeedFixed (8)
	PROP_PainChance (170)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_COUNTKILL)
	PROP_Flags2 (MF2_MCROSS|MF2_PASSMOBJ|MF2_PUSHWALL|MF2_FLOORCLIP)
	PROP_FlagsNetwork( NETFL_UPDATEPOSITION )

	PROP_SpawnState (S_SPOS_STND)
	PROP_SeeState (S_SPOS_RUN)
	PROP_PainState (S_SPOS_PAIN)
	PROP_MissileState (S_SPOS_ATK)
	PROP_DeathState (S_SPOS_DIE)
	PROP_XDeathState (S_SPOS_XDIE)
	PROP_RaiseState (S_SPOS_RAISE)

	PROP_SeeSound ("shotguy/sight")
	PROP_AttackSound ("shotguy/attack")
	PROP_PainSound ("shotguy/pain")
	PROP_DeathSound ("shotguy/death")
	PROP_ActiveSound ("shotguy/active")
	PROP_Obituary("$OB_SHOTGUY")
END_DEFAULTS

void AShotgunGuy::NoBlockingSet ()
{
	P_DropItem (this, "Shotgun", -1, 256);
}

static void A_SPosAttack2 (AActor *self)
{
	int i;
	int bangle;
	int slope;
		
	A_FaceTarget (self);
	bangle = self->angle;
	slope = P_AimLineAttack (self, bangle, MISSILERANGE);

	for (i=0 ; i<3 ; i++)
    {
		int angle = bangle + (pr_sposattack.Random2() << 20);
		int damage = ((pr_sposattack()%5)+1)*3;
		P_LineAttack(self, angle, MISSILERANGE, slope, damage, NAME_None, NAME_BulletPuff);
    }
}

void A_SPosAttackUseAtkSound (AActor *self)
{
	// [BC] Server takes care of the rest of this.
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
	{
		S_SoundID ( self, CHAN_WEAPON, self->AttackSound, 1, ATTN_NORM );
		return;
	}

	if (!self->target)
		return;

	S_SoundID (self, CHAN_WEAPON, self->AttackSound, 1, ATTN_NORM);
	A_SPosAttack2 (self);
}

// This version of the function, which uses a hard-coded sound, is
// meant for Dehacked only.
void A_SPosAttack (AActor *self)
{
	// [BC] Server takes care of the rest of this.
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
	{
		S_Sound ( self, CHAN_WEAPON, "shotguy/attack", 1, ATTN_NORM );
		return;
	}

	if (!self->target)
		return;

	S_Sound (self, CHAN_WEAPON, "shotguy/attack", 1, ATTN_NORM);
	A_SPosAttack2 (self);
}

// Chaingun guy ------------------------------------------------------------

class AChaingunGuy : public AActor
{
	DECLARE_ACTOR (AChaingunGuy, AActor)
public:
	void NoBlockingSet ();
};

FState AChaingunGuy::States[] =
{
#define S_CPOS_STND 0
	S_NORMAL (CPOS, 'A',   10, A_Look						, &States[S_CPOS_STND+1]),
	S_NORMAL (CPOS, 'B',   10, A_Look						, &States[S_CPOS_STND]),

#define S_CPOS_RUN (S_CPOS_STND+2)
	S_NORMAL (CPOS, 'A',	3, A_Chase						, &States[S_CPOS_RUN+1]),
	S_NORMAL (CPOS, 'A',	3, A_Chase						, &States[S_CPOS_RUN+2]),
	S_NORMAL (CPOS, 'B',	3, A_Chase						, &States[S_CPOS_RUN+3]),
	S_NORMAL (CPOS, 'B',	3, A_Chase						, &States[S_CPOS_RUN+4]),
	S_NORMAL (CPOS, 'C',	3, A_Chase						, &States[S_CPOS_RUN+5]),
	S_NORMAL (CPOS, 'C',	3, A_Chase						, &States[S_CPOS_RUN+6]),
	S_NORMAL (CPOS, 'D',	3, A_Chase						, &States[S_CPOS_RUN+7]),
	S_NORMAL (CPOS, 'D',	3, A_Chase						, &States[S_CPOS_RUN+0]),

#define S_CPOS_ATK (S_CPOS_RUN+8)
	S_NORMAL (CPOS, 'E',   10, A_FaceTarget 				, &States[S_CPOS_ATK+1]),
	S_BRIGHT (CPOS, 'F',	4, A_CPosAttack 				, &States[S_CPOS_ATK+2]),
	S_BRIGHT (CPOS, 'E',	4, A_CPosAttack 				, &States[S_CPOS_ATK+3]),
	S_NORMAL (CPOS, 'F',	1, A_CPosRefire 				, &States[S_CPOS_ATK+1]),

#define S_CPOS_PAIN (S_CPOS_ATK+4)
	S_NORMAL (CPOS, 'G',	3, NULL 						, &States[S_CPOS_PAIN+1]),
	S_NORMAL (CPOS, 'G',	3, A_Pain						, &States[S_CPOS_RUN+0]),

#define S_CPOS_DIE (S_CPOS_PAIN+2)
	S_NORMAL (CPOS, 'H',	5, NULL 						, &States[S_CPOS_DIE+1]),
	S_NORMAL (CPOS, 'I',	5, A_Scream 					, &States[S_CPOS_DIE+2]),
	S_NORMAL (CPOS, 'J',	5, A_NoBlocking					, &States[S_CPOS_DIE+3]),
	S_NORMAL (CPOS, 'K',	5, NULL 						, &States[S_CPOS_DIE+4]),
	S_NORMAL (CPOS, 'L',	5, NULL 						, &States[S_CPOS_DIE+5]),
	S_NORMAL (CPOS, 'M',	5, NULL 						, &States[S_CPOS_DIE+6]),
	S_NORMAL (CPOS, 'N',   -1, NULL 						, NULL),

#define S_CPOS_XDIE (S_CPOS_DIE+7)
	S_NORMAL (CPOS, 'O',	5, NULL 						, &States[S_CPOS_XDIE+1]),
	S_NORMAL (CPOS, 'P',	5, A_XScream					, &States[S_CPOS_XDIE+2]),
	S_NORMAL (CPOS, 'Q',	5, A_NoBlocking					, &States[S_CPOS_XDIE+3]),
	S_NORMAL (CPOS, 'R',	5, NULL 						, &States[S_CPOS_XDIE+4]),
	S_NORMAL (CPOS, 'S',	5, NULL 						, &States[S_CPOS_XDIE+5]),
	S_NORMAL (CPOS, 'T',   -1, NULL 						, NULL),

#define S_CPOS_RAISE (S_CPOS_XDIE+6)
	S_NORMAL (CPOS, 'N',	5, NULL 						, &States[S_CPOS_RAISE+1]),
	S_NORMAL (CPOS, 'M',	5, NULL 						, &States[S_CPOS_RAISE+2]),
	S_NORMAL (CPOS, 'L',	5, NULL 						, &States[S_CPOS_RAISE+3]),
	S_NORMAL (CPOS, 'K',	5, NULL 						, &States[S_CPOS_RAISE+4]),
	S_NORMAL (CPOS, 'J',	5, NULL 						, &States[S_CPOS_RAISE+5]),
	S_NORMAL (CPOS, 'I',	5, NULL 						, &States[S_CPOS_RAISE+6]),
	S_NORMAL (CPOS, 'H',	5, NULL 						, &States[S_CPOS_RUN+0])
};

IMPLEMENT_ACTOR (AChaingunGuy, Doom, 65, 2)
	PROP_SpawnHealth (70)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (56)
	PROP_Mass (100)
	PROP_SpeedFixed (8)
	PROP_PainChance (170)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_COUNTKILL)
	PROP_Flags2 (MF2_MCROSS|MF2_PASSMOBJ|MF2_PUSHWALL|MF2_FLOORCLIP)
	PROP_FlagsNetwork( NETFL_UPDATEPOSITION )

	PROP_SpawnState (S_CPOS_STND)
	PROP_SeeState (S_CPOS_RUN)
	PROP_PainState (S_CPOS_PAIN)
	PROP_MissileState (S_CPOS_ATK)
	PROP_DeathState (S_CPOS_DIE)
	PROP_XDeathState (S_CPOS_XDIE)
	PROP_RaiseState (S_CPOS_RAISE)

	PROP_SeeSound ("chainguy/sight")
	PROP_PainSound ("chainguy/pain")
	PROP_DeathSound ("chainguy/death")
	PROP_ActiveSound ("chainguy/active")
	PROP_AttackSound ("chainguy/attack")
	PROP_Obituary("$OB_CHAINGUY")
END_DEFAULTS

void AChaingunGuy::NoBlockingSet ()
{
	P_DropItem (this, "Chaingun", -1, 256);
}

// Super Shotgun guy -------------------------------------------------------------

class ASuperShotgunGuy : public AActor
{
	DECLARE_ACTOR (ASuperShotgunGuy, AActor)
public:
	void NoBlockingSet ();
};

FState ASuperShotgunGuy::States[] =
{

#define S_GPOS_STND 0
	S_NORMAL (GPOS, 'A',   10, A_Look						, &States[S_GPOS_STND+1]),
	S_NORMAL (GPOS, 'B',   10, A_Look						, &States[S_GPOS_STND]),

#define S_GPOS_RUN (S_GPOS_STND+2)
	S_NORMAL (GPOS, 'A',	4, A_Chase						, &States[S_GPOS_RUN+1]),
	S_NORMAL (GPOS, 'A',	4, A_Chase						, &States[S_GPOS_RUN+2]),
	S_NORMAL (GPOS, 'B',	4, A_Chase						, &States[S_GPOS_RUN+3]),
	S_NORMAL (GPOS, 'B',	4, A_Chase						, &States[S_GPOS_RUN+4]),
	S_NORMAL (GPOS, 'C',	4, A_Chase						, &States[S_GPOS_RUN+5]),
	S_NORMAL (GPOS, 'C',	4, A_Chase						, &States[S_GPOS_RUN+6]),
	S_NORMAL (GPOS, 'D',	4, A_Chase						, &States[S_GPOS_RUN+7]),
	S_NORMAL (GPOS, 'D',	4, A_Chase						, &States[S_GPOS_RUN+0]),

#define S_GPOS_ATK (S_GPOS_RUN+8)
	S_NORMAL (GPOS, 'E',   10, A_FaceTarget 				, &States[S_GPOS_ATK+1]),
	S_BRIGHT (GPOS, 'F',   8, A_GPosAttack					, &States[S_GPOS_ATK+2]),
	S_NORMAL (GPOS, 'E',   8, NULL							, &States[S_GPOS_RUN+0]),

#define S_GPOS_PAIN (S_GPOS_ATK+3)
	S_NORMAL (GPOS, 'G',	3, NULL 						, &States[S_GPOS_PAIN+1]),
	S_NORMAL (GPOS, 'G',	3, A_Pain						, &States[S_GPOS_RUN+0]),

#define S_GPOS_DIE (S_GPOS_PAIN+2)
	S_NORMAL (GPOS, 'H',	5, NULL 						, &States[S_GPOS_DIE+1]),
	S_NORMAL (GPOS, 'I',	5, A_Scream 					, &States[S_GPOS_DIE+2]),
	S_NORMAL (GPOS, 'J',	5, A_NoBlocking					, &States[S_GPOS_DIE+3]),
	S_NORMAL (GPOS, 'K',	5, NULL 						, &States[S_GPOS_DIE+4]),
	S_NORMAL (GPOS, 'L',	5, NULL 						, &States[S_GPOS_DIE+5]),
	S_NORMAL (GPOS, 'M',	5, NULL 						, &States[S_GPOS_DIE+6]),
	S_NORMAL (GPOS, 'N',   -1, NULL 						, NULL),

#define S_GPOS_XDIE (S_GPOS_DIE+7)
	S_NORMAL (GPOS, 'O',	5, NULL 						, &States[S_GPOS_XDIE+1]),
	S_NORMAL (GPOS, 'P',	5, A_XScream					, &States[S_GPOS_XDIE+2]),
	S_NORMAL (GPOS, 'Q',	5, A_NoBlocking					, &States[S_GPOS_XDIE+3]),
	S_NORMAL (GPOS, 'R',	5, NULL 						, &States[S_GPOS_XDIE+4]),
	S_NORMAL (GPOS, 'S',	5, NULL 						, &States[S_GPOS_XDIE+5]),
	S_NORMAL (GPOS, 'T',	-1, NULL 						, NULL),

#define S_GPOS_RAISE (S_GPOS_XDIE+6)
	S_NORMAL (GPOS, 'L',	5, NULL 						, &States[S_GPOS_RAISE+1]),
	S_NORMAL (GPOS, 'K',	5, NULL 						, &States[S_GPOS_RAISE+2]),
	S_NORMAL (GPOS, 'J',	5, NULL 						, &States[S_GPOS_RAISE+3]),
	S_NORMAL (GPOS, 'I',	5, NULL 						, &States[S_GPOS_RAISE+4]),
	S_NORMAL (GPOS, 'H',	5, NULL 						, &States[S_GPOS_RUN+0])
};

IMPLEMENT_ACTOR (ASuperShotgunGuy, Doom, 5005, 157)
	PROP_SpawnHealth (120)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (56)
	PROP_Mass (100)
	PROP_SpeedFixed (8)
	PROP_PainChance (170)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_COUNTKILL)
	PROP_Flags2 (MF2_MCROSS|MF2_PASSMOBJ|MF2_PUSHWALL|MF2_FLOORCLIP)
	PROP_FlagsNetwork( NETFL_UPDATEPOSITION )

	PROP_SpawnState (S_GPOS_STND)
	PROP_SeeState (S_GPOS_RUN)
	PROP_PainState (S_GPOS_PAIN)
	PROP_MissileState (S_GPOS_ATK)
	PROP_DeathState (S_GPOS_DIE)
	PROP_XDeathState (S_GPOS_XDIE)
	PROP_RaiseState (S_GPOS_RAISE)

	PROP_SeeSound ("chainguy/sight")
	PROP_AttackSound ("chainguy/attack")
	PROP_PainSound ("chainguy/pain")
	PROP_DeathSound ("chainguy/death")
	PROP_ActiveSound ("chainguy/active")
	PROP_Obituary("$OB_SSGGUY")
END_DEFAULTS

void ASuperShotgunGuy::NoBlockingSet ()
{
	P_DropItem (this, "SuperShotgun", -1, 256);
}

void A_GPosAttack (AActor *self)
{
	int 		i;
	angle_t 	angle;
	int 		damage;
				
	// [BC] Server takes care of the rest of this.
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
	{
		S_Sound( self, CHAN_WEAPON, "ssgguy/attack", 1, ATTN_NORM );
		return;
	}
				
	if (!self->target)
		return;

	A_FaceTarget (self);		
	S_Sound (self, CHAN_WEAPON, "ssgguy/attack", 1, ATTN_NORM);

	P_BulletSlope (self);
		
	for (i=0 ; i<7 ; i++)
	{
		damage = 5*(pr_gposattack()%3+1);
		angle = self->angle;
		angle += pr_gposattack.Random2() << 19;

		// Doom adjusts the bullet slope by shifting a random number [-255,255]
		// left 5 places. At 2048 units away, this means the vertical position
		// of the shot can deviate as much as 255 units from nominal. So using
		// some simple trigonometry, that means the vertical angle of the shot
		// can deviate by as many as ~7.097 degrees or ~84676099 BAMs.

		P_LineAttack (self,
					  angle,
					  MISSILERANGE,
					  bulletpitch + (pr_gposattack.Random2() * 332063), damage, NAME_None, NAME_BulletPuff);
	}
}

// Wolfenstein SS ----------------------------------------------------------

class AWolfensteinSS : public AActor
{
	DECLARE_ACTOR (AWolfensteinSS, AActor)
public:
	void NoBlockingSet ();
};

FState AWolfensteinSS::States[] =
{
#define S_SSWV_STND 0
	S_NORMAL (SSWV, 'A',   10, A_Look						, &States[S_SSWV_STND+1]),
	S_NORMAL (SSWV, 'B',   10, A_Look						, &States[S_SSWV_STND]),

#define S_SSWV_RUN (S_SSWV_STND+2)
	S_NORMAL (SSWV, 'A',	3, A_Chase						, &States[S_SSWV_RUN+1]),
	S_NORMAL (SSWV, 'A',	3, A_Chase						, &States[S_SSWV_RUN+2]),
	S_NORMAL (SSWV, 'B',	3, A_Chase						, &States[S_SSWV_RUN+3]),
	S_NORMAL (SSWV, 'B',	3, A_Chase						, &States[S_SSWV_RUN+4]),
	S_NORMAL (SSWV, 'C',	3, A_Chase						, &States[S_SSWV_RUN+5]),
	S_NORMAL (SSWV, 'C',	3, A_Chase						, &States[S_SSWV_RUN+6]),
	S_NORMAL (SSWV, 'D',	3, A_Chase						, &States[S_SSWV_RUN+7]),
	S_NORMAL (SSWV, 'D',	3, A_Chase						, &States[S_SSWV_RUN+0]),

#define S_SSWV_ATK (S_SSWV_RUN+8)
	S_NORMAL (SSWV, 'E',   10, A_FaceTarget 				, &States[S_SSWV_ATK+1]),
	S_NORMAL (SSWV, 'F',   10, A_FaceTarget 				, &States[S_SSWV_ATK+2]),
	S_BRIGHT (SSWV, 'G',	4, A_CPosAttack 				, &States[S_SSWV_ATK+3]),
	S_NORMAL (SSWV, 'F',	6, A_FaceTarget 				, &States[S_SSWV_ATK+4]),
	S_BRIGHT (SSWV, 'G',	4, A_CPosAttack 				, &States[S_SSWV_ATK+5]),
	S_NORMAL (SSWV, 'F',	1, A_CPosRefire 				, &States[S_SSWV_ATK+1]),

#define S_SSWV_PAIN (S_SSWV_ATK+6)
	S_NORMAL (SSWV, 'H',	3, NULL 						, &States[S_SSWV_PAIN+1]),
	S_NORMAL (SSWV, 'H',	3, A_Pain						, &States[S_SSWV_RUN+0]),

#define S_SSWV_DIE (S_SSWV_PAIN+2)
	S_NORMAL (SSWV, 'I',	5, NULL 						, &States[S_SSWV_DIE+1]),
	S_NORMAL (SSWV, 'J',	5, A_Scream 					, &States[S_SSWV_DIE+2]),
	S_NORMAL (SSWV, 'K',	5, A_NoBlocking					, &States[S_SSWV_DIE+3]),
	S_NORMAL (SSWV, 'L',	5, NULL 						, &States[S_SSWV_DIE+4]),
	S_NORMAL (SSWV, 'M',   -1, NULL 						, NULL),

#define S_SSWV_XDIE (S_SSWV_DIE+5)
	S_NORMAL (SSWV, 'N',	5, NULL 						, &States[S_SSWV_XDIE+1]),
	S_NORMAL (SSWV, 'O',	5, A_XScream					, &States[S_SSWV_XDIE+2]),
	S_NORMAL (SSWV, 'P',	5, A_NoBlocking					, &States[S_SSWV_XDIE+3]),
	S_NORMAL (SSWV, 'Q',	5, NULL 						, &States[S_SSWV_XDIE+4]),
	S_NORMAL (SSWV, 'R',	5, NULL 						, &States[S_SSWV_XDIE+5]),
	S_NORMAL (SSWV, 'S',	5, NULL 						, &States[S_SSWV_XDIE+6]),
	S_NORMAL (SSWV, 'T',	5, NULL 						, &States[S_SSWV_XDIE+7]),
	S_NORMAL (SSWV, 'U',	5, NULL 						, &States[S_SSWV_XDIE+8]),
	S_NORMAL (SSWV, 'V',   -1, NULL 						, NULL),

#define S_SSWV_RAISE (S_SSWV_XDIE+9)
	S_NORMAL (SSWV, 'M',	5, NULL 						, &States[S_SSWV_RAISE+1]),
	S_NORMAL (SSWV, 'L',	5, NULL 						, &States[S_SSWV_RAISE+2]),
	S_NORMAL (SSWV, 'K',	5, NULL 						, &States[S_SSWV_RAISE+3]),
	S_NORMAL (SSWV, 'J',	5, NULL 						, &States[S_SSWV_RAISE+4]),
	S_NORMAL (SSWV, 'I',	5, NULL 						, &States[S_SSWV_RUN+0])
};

IMPLEMENT_ACTOR (AWolfensteinSS, Doom, 84, 116)
	PROP_SpawnHealth (50)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (56)
	PROP_Mass (100)
	PROP_SpeedFixed (8)
	PROP_PainChance (170)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_COUNTKILL)
	PROP_Flags2 (MF2_MCROSS|MF2_PASSMOBJ|MF2_PUSHWALL|MF2_FLOORCLIP)
	PROP_FlagsNetwork( NETFL_UPDATEPOSITION )

	PROP_SpawnState (S_SSWV_STND)
	PROP_SeeState (S_SSWV_RUN)
	PROP_PainState (S_SSWV_PAIN)
	PROP_MissileState (S_SSWV_ATK)
	PROP_DeathState (S_SSWV_DIE)
	PROP_XDeathState (S_SSWV_XDIE)
	PROP_RaiseState (S_SSWV_RAISE)

	PROP_SeeSound ("wolfss/sight")
	PROP_PainSound ("wolfss/pain")
	PROP_DeathSound ("wolfss/death")
	PROP_ActiveSound ("wolfss/active")
	PROP_AttackSound ("wolfss/attack")
	PROP_Obituary("$OB_WOLFSS")
END_DEFAULTS

void AWolfensteinSS::NoBlockingSet ()
{
	P_DropItem (this, "Clip", -1, 256);
}

void A_CPosAttack (AActor *self)
{
	int angle;
	int bangle;
	int damage;
	int slope;
		
	// [BC] Server takes care of the rest of this.
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
	{
		S_SoundID ( self, CHAN_WEAPON, self->AttackSound, 1, ATTN_NORM );
		return;
	}

	if (!self->target)
		return;

	// [RH] Andy Baker's stealth monsters
	if (self->flags & MF_STEALTH)
	{
		self->visdir = 1;
	}

	S_SoundID (self, CHAN_WEAPON, self->AttackSound, 1, ATTN_NORM);
	A_FaceTarget (self);
	bangle = self->angle;
	slope = P_AimLineAttack (self, bangle, MISSILERANGE);

	angle = bangle + (pr_cposattack.Random2() << 20);
	damage = ((pr_cposattack()%5)+1)*3;
	P_LineAttack (self, angle, MISSILERANGE, slope, damage, NAME_None, NAME_BulletPuff);
}

void A_CPosRefire (AActor *self)
{
	// keep firing unless target got out of sight
	A_FaceTarget (self);

	// [BC] Client chaingunners continue to fire until told by the server to stop.
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
		return;

	if (pr_cposrefire() < 40)
		return;

	if (!self->target
		|| P_HitFriend (self)
		|| self->target->health <= 0
		|| !P_CheckSight (self, self->target, 0) )
	{
		self->SetState (self->SeeState);

		// [BC] If we're the server, tell clients to update this thing's state.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetThingState( self, STATE_SEE );
	}
}
