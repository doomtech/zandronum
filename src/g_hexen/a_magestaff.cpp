/*
#include "actor.h"
#include "info.h"
#include "p_local.h"
#include "m_random.h"
#include "s_sound.h"
#include "a_hexenglobal.h"
#include "gstrings.h"
#include "a_weaponpiece.h"
#include "thingdef/thingdef.h"
#include "doomstat.h"
*/

static FRandom pr_mstafftrack ("MStaffTrack");
static FRandom pr_bloodscourgedrop ("BloodScourgeDrop");

void A_MStaffTrack (AActor *);
void A_DropBloodscourgePieces (AActor *);
void A_MStaffAttack (AActor *actor);
void A_MStaffPalette (AActor *actor);

static AActor *FrontBlockCheck (AActor *mo, int index, void *);
static divline_t BlockCheckLine;

//==========================================================================

class AMageWeaponPiece : public AWeaponPiece
{
	DECLARE_CLASS (AMageWeaponPiece, AWeaponPiece)
protected:
	bool TryPickup (AActor *&toucher);
};

IMPLEMENT_CLASS (AMageWeaponPiece)

bool AMageWeaponPiece::TryPickup (AActor *&toucher)
{
	if (!toucher->IsKindOf (PClass::FindClass(NAME_ClericPlayer)) &&
		!toucher->IsKindOf (PClass::FindClass(NAME_FighterPlayer)))
	{
		return Super::TryPickup(toucher);
	}
	else
	{ // Wrong class, but try to pick up for ammo
		if (ShouldStay())
		{
			// Can't pick up weapons for other classes in coop netplay
			return false;
		}

		AWeapon * Defaults=(AWeapon*)GetDefaultByType(WeaponClass);

		bool gaveSome = !!(toucher->GiveAmmo (Defaults->AmmoType1, Defaults->AmmoGive1) +
						   toucher->GiveAmmo (Defaults->AmmoType2, Defaults->AmmoGive2));

		if (gaveSome)
		{
			GoAwayAndDie ();
		}
		return gaveSome;
	}
}

// The Mages's Staff (Bloodscourge) -----------------------------------------

class AMWeapBloodscourge : public AMageWeapon
{
	DECLARE_CLASS (AMWeapBloodscourge, AMageWeapon)
public:
	void Serialize (FArchive &arc)
	{
		Super::Serialize (arc);
		arc << MStaffCount;
	}
	PalEntry GetBlend ()
	{
		return PalEntry (MStaffCount * 128 / 3, 151, 110, 0);
	}
	BYTE MStaffCount;
};

IMPLEMENT_CLASS (AMWeapBloodscourge)

// Mage Staff FX2 (Bloodscourge) --------------------------------------------

class AMageStaffFX2 : public AActor
{
	DECLARE_CLASS(AMageStaffFX2, AActor)
public:
	int SpecialMissileHit (AActor *victim);
	bool IsOkayToAttack (AActor *link);
	bool SpecialBlastHandling (AActor *source, fixed_t strength);
};

IMPLEMENT_CLASS (AMageStaffFX2)

int AMageStaffFX2::SpecialMissileHit (AActor *victim)
{
	if (victim != target &&
		!victim->player &&
		!(victim->flags2 & MF2_BOSS))
	{
		P_DamageMobj (victim, this, target, 10, NAME_Fire);
		return 1;	// Keep going
	}
	return -1;
}

bool AMageStaffFX2::IsOkayToAttack (AActor *link)
{
	if (((link->flags3 & MF3_ISMONSTER) || link->player) && !(link->flags2 & MF2_DORMANT))
	{
		if (!(link->flags & MF_SHOOTABLE))
		{
			return false;
		}
		// [BB] Added the target check.
		if (( NETWORK_GetState( ) != NETSTATE_SINGLE ) && !deathmatch && link->player && target && target->player)
		{
			return false;
		}
		if (link == target)
		{
			return false;
		}
		if (target != NULL && target->IsFriend(link))
		{
			return false;
		}
		// [BB] Added the target check. Note: This will lead to conflicts when porting the changes from ZDoom revision 1905.
		if (target != NULL && P_CheckSight (this, link))
		{
			AActor *master = target;
			angle_t angle = R_PointToAngle2 (master->x, master->y,
							link->x, link->y) - master->angle;
			angle >>= 24;
			if (angle>226 || angle<30)
			{
				return true;
			}
		}
	}
	return false;
}

bool AMageStaffFX2::SpecialBlastHandling (AActor *source, fixed_t strength)
{
	// Reflect to originator
	tracer = target;	
	target = source;
	return true;
}

//============================================================================

//============================================================================
//
// MStaffSpawn
//
//============================================================================

void MStaffSpawn (AActor *pmo, angle_t angle)
{
	AActor *mo;
	AActor *linetarget;

	// [BC] Weapons are handled by the server.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return;
	}

	mo = P_SpawnPlayerMissile (pmo, 0, 0, 8*FRACUNIT,
		RUNTIME_CLASS(AMageStaffFX2), angle, &linetarget);
	if (mo)
	{
		mo->target = pmo;
		mo->tracer = linetarget;
	}
}

//============================================================================
//
// A_MStaffAttack
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_MStaffAttack)
{
	angle_t angle;
	player_t *player;
	AActor *linetarget;

	if (NULL == (player = self->player))
	{
		return;
	}

	AMWeapBloodscourge *weapon = static_cast<AMWeapBloodscourge *> (self->player->ReadyWeapon);
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire))
			return;
	}
	angle = self->angle;
	
	// [RH] Let's try and actually track what the player aimed at
	P_AimLineAttack (self, angle, PLAYERMISSILERANGE, &linetarget, ANGLE_1*32);
	if (linetarget == NULL)
	{
		BlockCheckLine.x = self->x;
		BlockCheckLine.y = self->y;
		BlockCheckLine.dx = -finesine[angle >> ANGLETOFINESHIFT];
		BlockCheckLine.dy = -finecosine[angle >> ANGLETOFINESHIFT];
		linetarget = P_BlockmapSearch (self, 10, FrontBlockCheck);
	}
	MStaffSpawn (self, angle);
	MStaffSpawn (self, angle-ANGLE_1*5);
	MStaffSpawn (self, angle+ANGLE_1*5);

	// [BC] Apply spread.
	if ( player->cheats & CF_SPREAD )
	{
		MStaffSpawn (self, angle + ( ANGLE_45 / 3 ));
		MStaffSpawn (self, angle-ANGLE_1*5 + ( ANGLE_45 / 3 ));
		MStaffSpawn (self, angle+ANGLE_1*5 + ( ANGLE_45 / 3 ));

		MStaffSpawn (self, angle - ( ANGLE_45 / 3 ));
		MStaffSpawn (self, angle-ANGLE_1*5 - ( ANGLE_45 / 3 ));
		MStaffSpawn (self, angle+ANGLE_1*5 - ( ANGLE_45 / 3 ));
	}

	S_Sound (self, CHAN_WEAPON, "MageStaffFire", 1, ATTN_NORM);
	weapon->MStaffCount = 3;
}

//============================================================================
//
// A_MStaffPalette
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_MStaffPalette)
{
	if (self->player != NULL)
	{
		AMWeapBloodscourge *weapon = static_cast<AMWeapBloodscourge *> (self->player->ReadyWeapon);
		if (weapon != NULL && weapon->MStaffCount != 0)
		{
			weapon->MStaffCount--;
		}
	}
}

//============================================================================
//
// A_MStaffTrack
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_MStaffTrack)
{
	if ((self->tracer == 0) && (pr_mstafftrack()<50))
	{
		self->tracer = P_RoughMonsterSearch (self, 10);
	}
	P_SeekerMissile (self, ANGLE_1*2, ANGLE_1*10);
}

//============================================================================
//
// FrontBlockCheck
//
// [RH] Like RoughBlockCheck, but it won't return anything behind a line.
//
//============================================================================

static AActor *FrontBlockCheck (AActor *mo, int index, void *)
{
	FBlockNode *link;

	for (link = blocklinks[index]; link != NULL; link = link->NextActor)
	{
		if (link->Me != mo)
		{
			if (P_PointOnDivlineSide (link->Me->x, link->Me->y, &BlockCheckLine) == 0 &&
				mo->IsOkayToAttack (link->Me))
			{
				return link->Me;
			}
		}
	}
	return NULL;
}

//============================================================================
//
// MStaffSpawn2 - for use by mage class boss
//
//============================================================================

void MStaffSpawn2 (AActor *actor, angle_t angle)
{
	AActor *mo;

	mo = P_SpawnMissileAngleZ (actor, actor->z+40*FRACUNIT,
		RUNTIME_CLASS(AMageStaffFX2), angle, 0);
	if (mo)
	{
		mo->target = actor;
		mo->tracer = P_BlockmapSearch (mo, 10, FrontBlockCheck);

		// [BB] Tell the clients to spawn the missile.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SpawnMissile( mo );
	}
}

//============================================================================
//
// A_MStaffAttack2 - for use by mage class boss
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_MageAttack)
{
	// [BB] Weapons are handled by the server.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return;
	}


	if (!self->target) return;

	angle_t angle;
	angle = self->angle;
	MStaffSpawn2 (self, angle);
	MStaffSpawn2 (self, angle-ANGLE_1*5);
	MStaffSpawn2 (self, angle+ANGLE_1*5);
	S_Sound (self, CHAN_WEAPON, "MageStaffFire", 1, ATTN_NORM);

	// [BB] If we're the server, tell the clients to play the sound.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SoundActor( self, CHAN_WEAPON, "MageStaffFire", 1, ATTN_NORM );
}

