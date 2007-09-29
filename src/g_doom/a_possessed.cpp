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
#include "cl_demo.h"

static FRandom pr_posattack ("PosAttack");
static FRandom pr_sposattack ("SPosAttack");
static FRandom pr_cposattack ("CPosAttack");
static FRandom pr_gposattack ("GPosAttack");
static FRandom pr_cposrefire ("CPosRefire");

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

void A_CPosAttack (AActor *self)
{
	int angle;
	int bangle;
	int damage;
	int slope;
		
	// [BC] Server takes care of the rest of this.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		// [RH] Andy Baker's stealth monsters
		if (self->flags & MF_STEALTH)
		{
			self->visdir = 1;
		}

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
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return;
	}

	if (pr_cposrefire() < 40)
		return;

	if (!self->target
		|| P_HitFriend (self)
		|| self->target->health <= 0
		|| !P_CheckSight (self, self->target, 0) )
	{
		// [BC] If we're the server, tell clients to update this thing's state.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetThingState( self, STATE_SEE );

		self->SetState (self->SeeState);
	}
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
