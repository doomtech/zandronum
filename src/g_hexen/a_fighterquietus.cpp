/*
#include "actor.h"
#include "gi.h"
#include "m_random.h"
#include "s_sound.h"
#include "d_player.h"
#include "a_action.h"
#include "p_local.h"
#include "p_pspr.h"
#include "gstrings.h"
#include "a_hexenglobal.h"
#include "a_weaponpiece.h"
#include "thingdef/thingdef.h"
*/

static FRandom pr_quietusdrop ("QuietusDrop");
static FRandom pr_fswordflame ("FSwordFlame");

//==========================================================================

class AFighterWeaponPiece : public AWeaponPiece
{
	DECLARE_CLASS (AFighterWeaponPiece, AWeaponPiece)
protected:
	bool TryPickup (AActor *&toucher);
};

IMPLEMENT_CLASS (AFighterWeaponPiece)

bool AFighterWeaponPiece::TryPickup (AActor *&toucher)
{
	if (!toucher->IsKindOf (PClass::FindClass(NAME_ClericPlayer)) &&
		!toucher->IsKindOf (PClass::FindClass(NAME_MagePlayer)))
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

//============================================================================
//
// A_DropQuietusPieces
//
//============================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_DropWeaponPieces)
{
	ACTION_PARAM_START(3);
	ACTION_PARAM_CLASS(p1, 0);
	ACTION_PARAM_CLASS(p2, 1);
	ACTION_PARAM_CLASS(p3, 2);

	for (int i = 0, j = 0, fineang = 0; i < 3; ++i)
	{
		const PClass *cls = j==0? p1 : j==1? p2 : p3;
		if (cls)
		{
			AActor *piece = Spawn (cls, self->x, self->y, self->z, ALLOW_REPLACE);
			if (piece != NULL)
			{
				piece->momx = self->momx + finecosine[fineang];
				piece->momy = self->momy + finesine[fineang];
				piece->momz = self->momz;
				piece->flags |= MF_DROPPED;
				fineang += FINEANGLES/3;
				j = (j == 0) ? (pr_quietusdrop() & 1) + 1 : 3-j;
			}
		}
	}
}



// Fighter Sword Missile ----------------------------------------------------

class AFSwordMissile : public AActor
{
	DECLARE_CLASS (AFSwordMissile, AActor)
public:
	int DoSpecialDamage(AActor *victim, AActor *source, int damage);
};

IMPLEMENT_CLASS (AFSwordMissile)

int AFSwordMissile::DoSpecialDamage(AActor *victim, AActor *source, int damage)
{
	if (victim->player)
	{
		damage -= damage >> 2;
	}
	return damage;
}

//============================================================================
//
// A_FSwordAttack
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_FSwordAttack)
{
	player_t *player;

	if (NULL == (player = self->player))
	{
		return;
	}
	AWeapon *weapon = self->player->ReadyWeapon;
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire))
			return;
	}

	// [BC] Weapons are handled by the server.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		S_Sound (self, CHAN_WEAPON, "FighterSwordFire", 1, ATTN_NORM);
		return;
	}

	P_SpawnPlayerMissile (self, 0, 0, -10*FRACUNIT, RUNTIME_CLASS(AFSwordMissile), self->angle+ANGLE_45/4);
	P_SpawnPlayerMissile (self, 0, 0,  -5*FRACUNIT, RUNTIME_CLASS(AFSwordMissile), self->angle+ANGLE_45/8);
	P_SpawnPlayerMissile (self, 0, 0,   0,		   RUNTIME_CLASS(AFSwordMissile), self->angle);
	P_SpawnPlayerMissile (self, 0, 0,   5*FRACUNIT, RUNTIME_CLASS(AFSwordMissile), self->angle-ANGLE_45/8);
	P_SpawnPlayerMissile (self, 0, 0,  10*FRACUNIT, RUNTIME_CLASS(AFSwordMissile), self->angle-ANGLE_45/4);

	// [BC] Apply spread.
	if ( player->cheats & CF_SPREAD )
	{
		P_SpawnPlayerMissile (self, 0, 0, -10*FRACUNIT, RUNTIME_CLASS(AFSwordMissile), self->angle+ANGLE_45/4 + ( ANGLE_45 / 3 ));
		P_SpawnPlayerMissile (self, 0, 0,  -5*FRACUNIT, RUNTIME_CLASS(AFSwordMissile), self->angle+ANGLE_45/8 + ( ANGLE_45 / 3 ));
		P_SpawnPlayerMissile (self, 0, 0,   0,		   RUNTIME_CLASS(AFSwordMissile), self->angle + ( ANGLE_45 / 3 ));
		P_SpawnPlayerMissile (self, 0, 0,   5*FRACUNIT, RUNTIME_CLASS(AFSwordMissile), self->angle-ANGLE_45/8 + ( ANGLE_45 / 3 ));
		P_SpawnPlayerMissile (self, 0, 0,  10*FRACUNIT, RUNTIME_CLASS(AFSwordMissile), self->angle-ANGLE_45/4 + ( ANGLE_45 / 3 ));

		P_SpawnPlayerMissile (self, 0, 0, -10*FRACUNIT, RUNTIME_CLASS(AFSwordMissile), self->angle+ANGLE_45/4 - ( ANGLE_45 / 3 ));
		P_SpawnPlayerMissile (self, 0, 0,  -5*FRACUNIT, RUNTIME_CLASS(AFSwordMissile), self->angle+ANGLE_45/8 - ( ANGLE_45 / 3 ));
		P_SpawnPlayerMissile (self, 0, 0,   0,		   RUNTIME_CLASS(AFSwordMissile), self->angle - ( ANGLE_45 / 3 ));
		P_SpawnPlayerMissile (self, 0, 0,   5*FRACUNIT, RUNTIME_CLASS(AFSwordMissile), self->angle-ANGLE_45/8 - ( ANGLE_45 / 3 ));
		P_SpawnPlayerMissile (self, 0, 0,  10*FRACUNIT, RUNTIME_CLASS(AFSwordMissile), self->angle-ANGLE_45/4 - ( ANGLE_45 / 3 ));
	}

	S_Sound (self, CHAN_WEAPON, "FighterSwordFire", 1, ATTN_NORM);

	// [BB] If we're the server, tell the clients to play the sound.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_WeaponSound( ULONG( player - players ), "FighterSwordFire", ULONG( player - players ), SVCF_SKIPTHISCLIENT );
}

//============================================================================
//
// A_FSwordFlames
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_FSwordFlames)
{
	int i;

	for (i = 1+(pr_fswordflame()&3); i; i--)
	{
		fixed_t x = self->x+((pr_fswordflame()-128)<<12);
		fixed_t y = self->y+((pr_fswordflame()-128)<<12);
		fixed_t z = self->z+((pr_fswordflame()-128)<<11);
		Spawn ("FSwordFlame", x, y, z, ALLOW_REPLACE);
	}
}

//============================================================================
//
// A_FighterAttack
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_FighterAttack)
{
	// [Dusk] Zedek's attack is handled by the server
	if ( NETWORK_InClientMode( )) return;

	if (!self->target) return;

	angle_t angle = self->angle;

	// [Dusk] Using a for-loop to avoid a copy/paste nightmare here.
	/*
	P_SpawnMissileAngle (self, RUNTIME_CLASS(AFSwordMissile), angle+ANG45/4, 0);
	P_SpawnMissileAngle (self, RUNTIME_CLASS(AFSwordMissile), angle+ANG45/8, 0);
	P_SpawnMissileAngle (self, RUNTIME_CLASS(AFSwordMissile), angle,         0);
	P_SpawnMissileAngle (self, RUNTIME_CLASS(AFSwordMissile), angle-ANG45/8, 0);
	P_SpawnMissileAngle (self, RUNTIME_CLASS(AFSwordMissile), angle-ANG45/4, 0);
	*/

	AActor *aMissile;
	for (int i = -2; i <= 2; i++) {
		aMissile = P_SpawnMissileAngle (self, RUNTIME_CLASS(AFSwordMissile), angle + (i*ANG45)/8, 0);
		if( NETWORK_GetState( ) == NETSTATE_SERVER && aMissile )
			SERVERCOMMANDS_SpawnMissile( aMissile );
	}

	S_Sound (self, CHAN_WEAPON, "FighterSwordFire", 1, ATTN_NORM);

	// [Dusk] inform of the sound.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SoundActor( self, CHAN_WEAPON, "FighterSwordFire", 1, ATTN_NORM );
}

