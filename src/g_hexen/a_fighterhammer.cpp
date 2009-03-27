/*
#include "actor.h"
#include "gi.h"
#include "m_random.h"
#include "s_sound.h"
#include "d_player.h"
#include "a_action.h"
#include "p_local.h"
#include "a_action.h"
#include "p_pspr.h"
#include "gstrings.h"
#include "a_hexenglobal.h"
#include "thingdef/thingdef.h"
*/

const fixed_t HAMMER_RANGE = MELEERANGE+MELEERANGE/2;

static FRandom pr_hammeratk ("FHammerAtk");

extern void AdjustPlayerAngle (AActor *pmo, AActor *linetarget);

//============================================================================
//
// A_FHammerAttack
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_FHammerAttack)
{
	angle_t angle;
	int damage;
	fixed_t power;
	int slope;
	int i;
	player_t *player;
	AActor *linetarget;

	if (NULL == (player = self->player))
	{
		return;
	}
	AActor *pmo=player->mo;

	damage = 60+(pr_hammeratk()&63);
	power = 10*FRACUNIT;
	for (i = 0; i < 16; i++)
	{
		angle = pmo->angle + i*(ANG45/32);
		slope = P_AimLineAttack (pmo, angle, HAMMER_RANGE, &linetarget, 0, false, true);
		if (linetarget)
		{
			P_LineAttack (pmo, angle, HAMMER_RANGE, slope, damage, NAME_Melee, PClass::FindClass ("HammerPuff"), true);
			AdjustPlayerAngle(pmo, linetarget);
			if (linetarget->flags3&MF3_ISMONSTER || linetarget->player)
			{
				P_ThrustMobj (linetarget, angle, power);
			}

			// [BC] Apply spread.
			if ( player->cheats & CF_SPREAD )
			{
				P_LineAttack (pmo, angle + ( ANGLE_45 / 3 ), HAMMER_RANGE, slope, damage, NAME_Melee, PClass::FindClass ("HammerPuff"), true);
				AdjustPlayerAngle(pmo, linetarget);
				if ( linetarget->flags3 & MF3_ISMONSTER || linetarget->player )
				{
					P_ThrustMobj( linetarget, angle + ( ANGLE_45 / 3 ), power );
				}

				P_LineAttack (pmo, angle - ( ANGLE_45 / 3 ), HAMMER_RANGE, slope, damage, NAME_Melee, PClass::FindClass ("HammerPuff"), true);
				AdjustPlayerAngle(pmo, linetarget);
				if ( linetarget->flags3 & MF3_ISMONSTER || linetarget->player )
				{
					P_ThrustMobj( linetarget, angle - ( ANGLE_45 / 3 ), power );
				}
			}

			pmo->special1 = false; // Don't throw a hammer
			goto hammerdone;
		}
		angle = pmo->angle-i*(ANG45/32);
		slope = P_AimLineAttack(pmo, angle, HAMMER_RANGE, &linetarget, 0, false, true);
		if(linetarget)
		{
			P_LineAttack(pmo, angle, HAMMER_RANGE, slope, damage, NAME_Melee, PClass::FindClass ("HammerPuff"), true);
			AdjustPlayerAngle(pmo, linetarget);
			if (linetarget->flags3&MF3_ISMONSTER || linetarget->player)
			{
				P_ThrustMobj(linetarget, angle, power);
			}

			// [BC] Apply spread.
			if ( player->cheats & CF_SPREAD )
			{
				P_LineAttack(pmo, angle + ( ANGLE_45 / 3 ), HAMMER_RANGE, slope, damage, NAME_Melee, PClass::FindClass ("HammerPuff"), true);
				AdjustPlayerAngle(pmo, linetarget);
				if ( linetarget->flags3 & MF3_ISMONSTER || linetarget->player )
				{
					P_ThrustMobj( linetarget, angle + ( ANGLE_45 / 3 ), power );
				}

				P_LineAttack(pmo, angle - ( ANGLE_45 / 3 ), HAMMER_RANGE, slope, damage, NAME_Melee, PClass::FindClass ("HammerPuff"), true);
				AdjustPlayerAngle(pmo, linetarget);
				if ( linetarget->flags3 & MF3_ISMONSTER || linetarget->player )
				{
					P_ThrustMobj( linetarget, angle - ( ANGLE_45 / 3 ), power );
				}
			}

			pmo->special1 = false; // Don't throw a hammer
			goto hammerdone;
		}
	}
	// didn't find any targets in meleerange, so set to throw out a hammer
	angle = pmo->angle;
	slope = P_AimLineAttack (pmo, angle, HAMMER_RANGE, &linetarget, 0, false, true);
	if (P_LineAttack (pmo, angle, HAMMER_RANGE, slope, damage, NAME_Melee, PClass::FindClass ("HammerPuff"), true) != NULL)
	{
		pmo->special1 = false;
	}
	else
	{
		pmo->special1 = true;
	}

	// [BC] Apply spread.
	if ( player->cheats & CF_SPREAD )
	{
		if (P_LineAttack (pmo, angle + ( ANGLE_45 / 3 ), HAMMER_RANGE, slope, damage, NAME_Melee, PClass::FindClass ("HammerPuff"), true) != NULL)
			pmo->special1 = false;
		else
			pmo->special1 = true;

		if (P_LineAttack (pmo, angle - ( ANGLE_45 / 3 ), HAMMER_RANGE, slope, damage, NAME_Melee, PClass::FindClass ("HammerPuff"), true) != NULL)
			pmo->special1 = false;
		else
			pmo->special1 = true;
	}

hammerdone:
	// Don't spawn a hammer if the player doesn't have enough mana
	if (player->ReadyWeapon == NULL ||
		!player->ReadyWeapon->CheckAmmo (player->ReadyWeapon->bAltFire ?
			AWeapon::AltFire : AWeapon::PrimaryFire, false, true))
	{ 
		pmo->special1 = false;
	}
	return;		
}

//============================================================================
//
// A_FHammerThrow
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_FHammerThrow)
{
	AActor *mo;
	player_t *player;

	if (NULL == (player = self->player))
	{
		return;
	}

	if (!player->mo->special1)
	{
		return;
	}
	AWeapon *weapon = player->ReadyWeapon;
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire, false))
			return;
	}

	// [BC] Weapons are handled by the server.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return;
	}

	mo = P_SpawnPlayerMissile (player->mo, PClass::FindClass ("HammerMissile")); 
	if (mo)
	{
		mo->special1 = 0;
	}	

	// [BC] Apply spread.
	if ( player->cheats & CF_SPREAD )
	{
		mo = P_SpawnPlayerMissile( player->mo, PClass::FindClass ("HammerMissile"), self->angle + ( ANGLE_45 / 3 )); 
		if ( mo )
		{
			mo->special1 = 0;
		}	

		mo = P_SpawnPlayerMissile( player->mo, PClass::FindClass ("HammerMissile"), self->angle - ( ANGLE_45 / 3 )); 
		if ( mo )
		{
			mo->special1 = 0;
		}	
	}
}
