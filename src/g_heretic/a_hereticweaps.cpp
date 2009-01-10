#include "templates.h"
#include "actor.h"
#include "info.h"
#include "s_sound.h"
#include "m_random.h"
#include "a_pickups.h"
#include "a_hereticglobal.h"
#include "d_player.h"
#include "p_pspr.h"
#include "p_local.h"
#include "gstrings.h"
#include "p_effect.h"
#include "gstrings.h"
#include "p_enemy.h"
#include "gi.h"
#include "r_translate.h"
#include "thingdef/thingdef.h"
// [BB] New #includes.
#include "cl_demo.h"
#include "deathmatch.h"
#include "network.h"
#include "sv_commands.h"
#include "gamemode.h"

static FRandom pr_sap ("StaffAtkPL1");
static FRandom pr_sap2 ("StaffAtkPL2");
static FRandom pr_fgw ("FireWandPL1");
static FRandom pr_fgw2 ("FireWandPL2");
static FRandom pr_boltspark ("BoltSpark");
static FRandom pr_macerespawn ("MaceRespawn");
static FRandom pr_maceatk ("FireMacePL1");
static FRandom pr_gatk ("GauntletAttack");
static FRandom pr_bfx1 ("BlasterFX1");
static FRandom pr_ripd ("RipperD");
static FRandom pr_fb1 ("FireBlasterPL1");
static FRandom pr_bfx1t ("BlasterFX1Tick");
static FRandom pr_hrfx2 ("HornRodFX2");
static FRandom pr_rp ("RainPillar");
static FRandom pr_fsr1 ("FireSkullRodPL1");
static FRandom pr_storm ("SkullRodStorm");
static FRandom pr_impact ("RainImpact");
static FRandom pr_pfx1 ("PhoenixFX1");
static FRandom pr_pfx2 ("PhoenixFX2");
static FRandom pr_fp2 ("FirePhoenixPL2");

#define FLAME_THROWER_TICS (10*TICRATE)


#define USE_BLSR_AMMO_1 1
#define USE_BLSR_AMMO_2 5
#define USE_SKRD_AMMO_1 1
#define USE_SKRD_AMMO_2 5
#define USE_PHRD_AMMO_1 1
#define USE_PHRD_AMMO_2 1
#define USE_MACE_AMMO_1 1
#define USE_MACE_AMMO_2 5

extern bool P_AutoUseChaosDevice (player_t *player);

// Base Heretic weapon class ------------------------------------------------

IMPLEMENT_STATELESS_ACTOR (AHereticWeapon, Heretic, -1, 0)
	PROP_Weapon_Kickback (150)
END_DEFAULTS

// --- Staff ----------------------------------------------------------------

//----------------------------------------------------------------------------
//
// PROC A_StaffAttackPL1
//
//----------------------------------------------------------------------------

void A_StaffAttack (AActor *actor)
{
	angle_t angle;
	int damage;
	int slope;
	player_t *player;
	AActor *linetarget;

	if (NULL == (player = actor->player))
	{
		return;
	}

	int index = CheckIndex (2, NULL);
	if (index < 0) return;

	damage = EvalExpressionI (StateParameters[index], actor);
	const PClass *puff = PClass::FindClass ((ENamedName)StateParameters[index+1]);


	AWeapon *weapon = player->ReadyWeapon;
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire))
			return;
	}

	// [BC] Weapons are handled by the server.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return;
	}

	angle = actor->angle;
	angle += pr_sap.Random2() << 18;
	slope = P_AimLineAttack (actor, angle, MELEERANGE, &linetarget);
	P_LineAttack (actor, angle, MELEERANGE, slope, damage, NAME_Melee, puff, true);

	// [BC] Apply spread.
	if ( player->cheats & CF_SPREAD )
	{
		P_LineAttack(actor, angle + ( ANGLE_45 / 3 ), MELEERANGE, slope, damage, NAME_Melee, puff, true);
		P_LineAttack(actor, angle - ( ANGLE_45 / 3 ), MELEERANGE, slope, damage, NAME_Melee, puff, true);
	}

	if (linetarget)
	{
		//S_StartSound(player->mo, sfx_stfhit);
		// turn to face target
		actor->angle = R_PointToAngle2 (actor->x,
			actor->y, linetarget->x, linetarget->y);

		// [BC] If we're the server, tell clients to adjust the player's angle.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetThingAngleExact( player->mo );
	}
}


//----------------------------------------------------------------------------
//
// PROC A_FireGoldWandPL1
//
//----------------------------------------------------------------------------

void A_FireGoldWandPL1 (AActor *actor)
{
	angle_t angle;
	int damage;
	player_t *player;

	if (NULL == (player = actor->player))
	{
		return;
	}

	AWeapon *weapon = player->ReadyWeapon;
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire))
			return;
	}

	// [BC] If we're the client, just play the sound and get out.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		S_Sound( player->mo, CHAN_WEAPON, "weapons/wandhit", 1, ATTN_NORM );
		return;
	}

	angle_t pitch = P_BulletSlope(actor);
	damage = 7+(pr_fgw()&7);
	angle = actor->angle;
	if (player->refire)
	{
		angle += pr_fgw.Random2() << 18;
	}
	P_LineAttack (actor, angle, PLAYERMISSILERANGE, pitch, damage, NAME_None, "GoldWandPuff1");

	// [BC] Apply spread.
	if ( player->cheats & CF_SPREAD )
	{
		angle = actor->angle;
		if (player->refire)
		{
			angle += pr_fgw.Random2() << 18;
		}
		P_LineAttack(actor, angle + ( ANGLE_45 / 3 ), PLAYERMISSILERANGE, pitch, damage, NAME_None, "GoldWandPuff1");

		angle = actor->angle;
		if (player->refire)
		{
			angle += pr_fgw.Random2() << 18;
		}
		P_LineAttack(actor, angle - ( ANGLE_45 / 3 ), PLAYERMISSILERANGE, pitch, damage, NAME_None, "GoldWandPuff1");
	}

	S_Sound (actor, CHAN_WEAPON, "weapons/wandhit", 1, ATTN_NORM);

	// [BC] If we're the server, tell clients that a weapon is being fired.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_WeaponSound( ULONG( player - players ), "weapons/wandhit", ULONG( player - players ), SVCF_SKIPTHISCLIENT );
}

//----------------------------------------------------------------------------
//
// PROC A_FireGoldWandPL2
//
//----------------------------------------------------------------------------

void A_FireGoldWandPL2 (AActor *actor)
{
	int i;
	angle_t angle;
	int damage;
	fixed_t momz;
	player_t *player;
	AActor	*pMissile;

	if (NULL == (player = actor->player))
	{
		return;
	}

	AWeapon *weapon = player->ReadyWeapon;
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire))
			return;
	}

	// [BC] If we're the client, just play the sound and get out.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		S_Sound( player->mo, CHAN_WEAPON, "weapons/wandhit", 1, ATTN_NORM );
		return;
	}

	angle_t pitch = P_BulletSlope(actor);
	momz = FixedMul (GetDefaultByName("GoldWandFX2")->Speed,
		finetangent[FINEANGLES/4-((signed)pitch>>ANGLETOFINESHIFT)]);
	pMissile = P_SpawnMissileAngle (actor, PClass::FindClass("GoldWandFX2"), actor->angle-(ANG45/8), momz);
	if ( pMissile && NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SpawnMissileExact( pMissile );
	pMissile = P_SpawnMissileAngle (actor, PClass::FindClass("GoldWandFX2"), actor->angle+(ANG45/8), momz);
	if ( pMissile && NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SpawnMissileExact( pMissile );
	angle = actor->angle-(ANG45/8);

	for(i = 0; i < 5; i++)
	{
		damage = 1+(pr_fgw2()&7);
		P_LineAttack (actor, angle, PLAYERMISSILERANGE, pitch, damage, NAME_None, "GoldWandPuff2");
		angle += ((ANG45/8)*2)/4;
	}

	// [BC] Apply spread.
	if ( player->cheats & CF_SPREAD )
	{
		pMissile = P_SpawnMissileAngle( actor, PClass::FindClass("GoldWandFX2"), actor->angle - ( ANG45 / 8 ) + ( ANGLE_45 / 3 ), momz );
		if ( pMissile && NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SpawnMissileExact( pMissile );

		pMissile = P_SpawnMissileAngle( actor, PClass::FindClass("GoldWandFX2"), actor->angle + ( ANG45 / 8 ) + ( ANGLE_45 / 3 ), momz );
		if ( pMissile && NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SpawnMissileExact( pMissile );

		angle = actor->angle - ( ANG45 / 8 ) + ( ANGLE_45 / 3 );
		for ( i = 0; i < 5; i++ )
		{
			damage = 1+(pr_fgw2()&7);
			P_LineAttack (actor, angle, PLAYERMISSILERANGE, pitch, damage, NAME_None, "GoldWandPuff2");
			angle += ((ANG45/8)*2)/4;
		}

		pMissile = P_SpawnMissileAngle( actor, PClass::FindClass("GoldWandFX2"), actor->angle - ( ANG45 / 8 ) - ( ANGLE_45 / 3 ), momz );
		if ( pMissile && NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SpawnMissileExact( pMissile );

		pMissile = P_SpawnMissileAngle( actor, PClass::FindClass("GoldWandFX2"), actor->angle + ( ANG45 / 8 ) - ( ANGLE_45 / 3 ), momz );
		if ( pMissile && NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SpawnMissileExact( pMissile );

		angle = actor->angle - ( ANG45 / 8 ) - ( ANGLE_45 / 3 );
		for ( i = 0; i < 5; i++ )
		{
			damage = 1+(pr_fgw2()&7);
			P_LineAttack (actor, angle, PLAYERMISSILERANGE, pitch, damage, NAME_None, "GoldWandPuff2");
			angle += ((ANG45/8)*2)/4;
		}
	}

	S_Sound (actor, CHAN_WEAPON, "weapons/wandhit", 1, ATTN_NORM);

	// [BC] If we're the server, tell clients that a weapon is being fired.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_WeaponSound( ULONG( player - players ), "weapons/wandhit", ULONG( player - players ), SVCF_SKIPTHISCLIENT );
}

//----------------------------------------------------------------------------
//
// PROC A_FireCrossbowPL1
//
//----------------------------------------------------------------------------

void A_FireCrossbowPL1 (AActor *actor)
{
	player_t *player;

	if (NULL == (player = actor->player))
	{
		return;
	}

	AWeapon *weapon = player->ReadyWeapon;
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire))
			return;
	}

	// [BC] Weapons are handled by the server.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return;
	}

	P_SpawnPlayerMissile (actor, PClass::FindClass("CrossbowFX1"));
	P_SpawnPlayerMissile (actor, PClass::FindClass("CrossbowFX3"), actor->angle-(ANG45/10));
	P_SpawnPlayerMissile (actor, PClass::FindClass("CrossbowFX3"), actor->angle+(ANG45/10));

	// [BC] Apply spread.
	if ( player->cheats & CF_SPREAD )
	{
		P_SpawnPlayerMissile (actor, PClass::FindClass("CrossbowFX1"), actor->angle + ( ANGLE_45 / 3 ));
		P_SpawnPlayerMissile (actor, PClass::FindClass("CrossbowFX3"), actor->angle-(ANG45/10) + ( ANGLE_45 / 3 ));
		P_SpawnPlayerMissile (actor, PClass::FindClass("CrossbowFX3"), actor->angle+(ANG45/10) + ( ANGLE_45 / 3 ));

		P_SpawnPlayerMissile (actor, PClass::FindClass("CrossbowFX1"), actor->angle - ( ANGLE_45 / 3 ));
		P_SpawnPlayerMissile (actor, PClass::FindClass("CrossbowFX3"), actor->angle-(ANG45/10) - ( ANGLE_45 / 3 ));
		P_SpawnPlayerMissile (actor, PClass::FindClass("CrossbowFX3"), actor->angle+(ANG45/10) - ( ANGLE_45 / 3 ));
	}
}

//----------------------------------------------------------------------------
//
// PROC A_FireCrossbowPL2
//
//----------------------------------------------------------------------------

void A_FireCrossbowPL2(AActor *actor)
{
	player_t *player;

	if (NULL == (player = actor->player))
	{
		return;
	}

	AWeapon *weapon = actor->player->ReadyWeapon;
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire))
			return;
	}

	// [BC] Weapons are handled by the server.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{		
		return;
	}

	P_SpawnPlayerMissile (actor, PClass::FindClass("CrossbowFX2"));
	P_SpawnPlayerMissile (actor, PClass::FindClass("CrossbowFX2"), actor->angle-(ANG45/10));
	P_SpawnPlayerMissile (actor, PClass::FindClass("CrossbowFX2"), actor->angle+(ANG45/10));
	P_SpawnPlayerMissile (actor, PClass::FindClass("CrossbowFX3"), actor->angle-(ANG45/5));
	P_SpawnPlayerMissile (actor, PClass::FindClass("CrossbowFX3"), actor->angle+(ANG45/5));

	// [BC] Apply spread.
	if ( player->cheats & CF_SPREAD )
	{
		P_SpawnPlayerMissile (actor, PClass::FindClass("CrossbowFX2"), actor->angle + ( ANGLE_45 / 3 ));
		P_SpawnPlayerMissile (actor, PClass::FindClass("CrossbowFX2"), actor->angle-(ANG45/10) + ( ANGLE_45 / 3 ));
		P_SpawnPlayerMissile (actor, PClass::FindClass("CrossbowFX2"), actor->angle+(ANG45/10) + ( ANGLE_45 / 3 ));
		P_SpawnPlayerMissile (actor, PClass::FindClass("CrossbowFX3"), actor->angle-(ANG45/5) + ( ANGLE_45 / 3 ));
		P_SpawnPlayerMissile (actor, PClass::FindClass("CrossbowFX3"), actor->angle+(ANG45/5) + ( ANGLE_45 / 3 ));

		P_SpawnPlayerMissile (actor, PClass::FindClass("CrossbowFX2"), actor->angle - ( ANGLE_45 / 3 ));
		P_SpawnPlayerMissile (actor, PClass::FindClass("CrossbowFX2"), actor->angle-(ANG45/10) - ( ANGLE_45 / 3 ));
		P_SpawnPlayerMissile (actor, PClass::FindClass("CrossbowFX2"), actor->angle+(ANG45/10) - ( ANGLE_45 / 3 ));
		P_SpawnPlayerMissile (actor, PClass::FindClass("CrossbowFX3"), actor->angle-(ANG45/5) - ( ANGLE_45 / 3 ));
		P_SpawnPlayerMissile (actor, PClass::FindClass("CrossbowFX3"), actor->angle+(ANG45/5) - ( ANGLE_45 / 3 ));
	}
}

//---------------------------------------------------------------------------
//
// PROC A_GauntletAttack
//
//---------------------------------------------------------------------------

void A_GauntletAttack (AActor *actor)
{
	angle_t angle;
	int damage;
	int slope;
	int randVal;
	fixed_t dist;
	player_t *player;
	const PClass *pufftype;
	int power;
	AActor *linetarget;

	if (NULL == (player = actor->player))
	{
		return;
	}

	int index = CheckIndex (1, NULL);
	if (index < 0) return;

	power = EvalExpressionI (StateParameters[index], actor);

	AWeapon *weapon = player->ReadyWeapon;
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire))
			return;
	}
	player->psprites[ps_weapon].sx = ((pr_gatk()&3)-2) * FRACUNIT;
	player->psprites[ps_weapon].sy = WEAPONTOP + (pr_gatk()&3) * FRACUNIT;
	angle = actor->angle;
	if (power)
	{
		damage = pr_gatk.HitDice (2);
		dist = 4*MELEERANGE;
		angle += pr_gatk.Random2() << 17;
		pufftype = PClass::FindClass("GauntletPuff2");
	}
	else
	{
		damage = pr_gatk.HitDice (2);
		dist = MELEERANGE+1;
		angle += pr_gatk.Random2() << 18;
		pufftype = PClass::FindClass("GauntletPuff1");
	}
	slope = P_AimLineAttack (actor, angle, dist, &linetarget);
	P_LineAttack (actor, angle, dist, slope, damage, NAME_Melee, pufftype);
	if (!linetarget)
	{
		if (pr_gatk() > 64)
		{
			player->extralight = !player->extralight;
		}
		S_Sound (actor, CHAN_AUTO, "weapons/gauntletson", 1, ATTN_NORM);
		return;
	}
	randVal = pr_gatk();
	if (randVal < 64)
	{
		player->extralight = 0;
	}
	else if (randVal < 160)
	{
		player->extralight = 1;
	}
	else
	{
		player->extralight = 2;
	}
	if (power)
	{
		P_GiveBody (actor, damage>>1);
		S_Sound (actor, CHAN_AUTO, "weapons/gauntletspowhit", 1, ATTN_NORM);
	}
	else
	{
		S_Sound (actor, CHAN_AUTO, "weapons/gauntletshit", 1, ATTN_NORM);
	}
	// turn to face target
	angle = R_PointToAngle2 (actor->x, actor->y,
		linetarget->x, linetarget->y);
	if (angle-actor->angle > ANG180)
	{
		if ((int)(angle-actor->angle) < -ANG90/20)
			actor->angle = angle+ANG90/21;
		else
			actor->angle -= ANG90/20;
	}
	else
	{
		if (angle-actor->angle > ANG90/20)
			actor->angle = angle-ANG90/21;
		else
			actor->angle += ANG90/20;
	}
	actor->flags |= MF_JUSTATTACKED;
}

// --- Mace -----------------------------------------------------------------

#define MAGIC_JUNK 1234

// Mace FX4 -----------------------------------------------------------------

class AMaceFX4 : public AActor
{
	DECLARE_CLASS (AMaceFX4, AActor)
public:
	int DoSpecialDamage (AActor *target, int damage);
};

IMPLEMENT_CLASS (AMaceFX4)

int AMaceFX4::DoSpecialDamage (AActor *target, int damage)
{
	if ((target->flags2 & MF2_BOSS) || (target->flags3 & MF3_DONTSQUASH) || target->IsTeammate (this->target))
	{ // Don't allow cheap boss kills and don't instagib teammates
		return damage;
	}
	else if (target->player)
	{ // Player specific checks
		if (target->player->mo->flags2 & MF2_INVULNERABLE)
		{ // Can't hurt invulnerable players
			return -1;
		}
		if (P_AutoUseChaosDevice (target->player))
		{ // Player was saved using chaos device
			return -1;
		}
	}
	return 1000000; // Something's gonna die
}

//----------------------------------------------------------------------------
//
// PROC A_FireMacePL1B
//
//----------------------------------------------------------------------------

void A_FireMacePL1B (AActor *actor)
{
	AActor *ball;
	angle_t angle;
	player_t *player;

	if (NULL == (player = actor->player))
	{
		return;
	}

	AWeapon *weapon = player->ReadyWeapon;
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire))
			return;
	}

	// [BC] Weapons are handled by the server.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return;
	}

	ball = Spawn("MaceFX2", actor->x, actor->y, actor->z + 28*FRACUNIT 
		- actor->floorclip, ALLOW_REPLACE);
	ball->momz = 2*FRACUNIT+/*((player->lookdir)<<(FRACBITS-5))*/
		finetangent[FINEANGLES/4-(actor->pitch>>ANGLETOFINESHIFT)];
	angle = actor->angle;
	ball->target = actor;
	ball->angle = angle;
	ball->z += 2*finetangent[FINEANGLES/4-(actor->pitch>>ANGLETOFINESHIFT)];
	angle >>= ANGLETOFINESHIFT;
	ball->momx = (actor->momx>>1)+FixedMul(ball->Speed, finecosine[angle]);
	ball->momy = (actor->momy>>1)+FixedMul(ball->Speed, finesine[angle]);
	S_Sound (ball, CHAN_BODY, "weapons/maceshoot", 1, ATTN_NORM);

	// [BC] If we're the server, spawn the ball and play the sound.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		SERVERCOMMANDS_SpawnMissileExact( ball );
		SERVERCOMMANDS_SoundActor( ball, CHAN_BODY, "weapons/maceshoot", 1, ATTN_NORM );
	}

	// [BC] Apply spread.
	if ( player->cheats & CF_SPREAD )
	{
		ball = Spawn("MaceFX2", actor->x, actor->y, actor->z + 28*FRACUNIT 
			- actor->floorclip, ALLOW_REPLACE);
		ball->momz = 2*FRACUNIT+/*((player->lookdir)<<(FRACBITS-5))*/
			finetangent[FINEANGLES/4-(actor->pitch>>ANGLETOFINESHIFT)];
		angle = actor->angle + ( ANGLE_45 / 3 );
		ball->target = actor;
		ball->angle = angle;
		ball->z += 2*finetangent[FINEANGLES/4-(actor->pitch>>ANGLETOFINESHIFT)];
		angle >>= ANGLETOFINESHIFT;
		ball->momx = (actor->momx>>1)+FixedMul(ball->Speed, finecosine[angle]);
		ball->momy = (actor->momy>>1)+FixedMul(ball->Speed, finesine[angle]);
		S_Sound (ball, CHAN_BODY, "weapons/maceshoot", 1, ATTN_NORM);

		// [BC] If we're the server, spawn the ball and play the sound.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		{
			SERVERCOMMANDS_SpawnMissileExact( ball );
			SERVERCOMMANDS_SoundActor( ball, CHAN_BODY, "weapons/maceshoot", 1, ATTN_NORM );
		}

		ball = Spawn("MaceFX2", actor->x, actor->y, actor->z + 28*FRACUNIT 
			- actor->floorclip, ALLOW_REPLACE);
		ball->momz = 2*FRACUNIT+/*((player->lookdir)<<(FRACBITS-5))*/
			finetangent[FINEANGLES/4-(actor->pitch>>ANGLETOFINESHIFT)];
		angle = actor->angle - ( ANGLE_45 / 3 );
		ball->target = actor;
		ball->angle = angle;
		ball->z += 2*finetangent[FINEANGLES/4-(actor->pitch>>ANGLETOFINESHIFT)];
		angle >>= ANGLETOFINESHIFT;
		ball->momx = (actor->momx>>1)+FixedMul(ball->Speed, finecosine[angle]);
		ball->momy = (actor->momy>>1)+FixedMul(ball->Speed, finesine[angle]);
		S_Sound (ball, CHAN_BODY, "weapons/maceshoot", 1, ATTN_NORM);

		// [BC] If we're the server, spawn the ball and play the sound.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		{
			SERVERCOMMANDS_SpawnMissileExact( ball );
			SERVERCOMMANDS_SoundActor( ball, CHAN_BODY, "weapons/maceshoot", 1, ATTN_NORM );
		}
	}

	P_CheckMissileSpawn (ball);
}

//----------------------------------------------------------------------------
//
// PROC A_FireMacePL1
//
//----------------------------------------------------------------------------

void A_FireMacePL1 (AActor *actor)
{
	AActor *ball;
	player_t *player;

	if (NULL == (player = actor->player))
	{
		return;
	}

	if (pr_maceatk() < 28)
	{
		A_FireMacePL1B (actor);
		return;
	}
	AWeapon *weapon = player->ReadyWeapon;
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire))
			return;
	}
	player->psprites[ps_weapon].sx = ((pr_maceatk()&3)-2)*FRACUNIT;
	player->psprites[ps_weapon].sy = WEAPONTOP+(pr_maceatk()&3)*FRACUNIT;

	// [BC] Weapons are handled by the server.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return;
	}

	ball = P_SpawnPlayerMissile (actor, PClass::FindClass("MaceFX1"),
		actor->angle+(((pr_maceatk()&7)-4)<<24));
	if (ball)
	{
		ball->special1 = 16; // tics till dropoff
	}

	// [BC] Apply spread.
	if ( player->cheats & CF_SPREAD )
	{
		ball = P_SpawnPlayerMissile (actor, PClass::FindClass("MaceFX1"),
			actor->angle+(((pr_maceatk()&7)-4)<<24) + ( ANGLE_45 / 3 ));
		if (ball)
		{
			ball->special1 = 16; // tics till dropoff
		}

		ball = P_SpawnPlayerMissile (actor, PClass::FindClass("MaceFX1"),
			actor->angle+(((pr_maceatk()&7)-4)<<24) - ( ANGLE_45 / 3 ));
		if (ball)
		{
			ball->special1 = 16; // tics till dropoff
		}
	}
}

//----------------------------------------------------------------------------
//
// PROC A_MacePL1Check
//
//----------------------------------------------------------------------------

void A_MacePL1Check (AActor *ball)
{
	// [BC] Let the server handle this.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return;
	}

	if (ball->special1 == 0)
	{
		return;
	}
	ball->special1 -= 4;
	if (ball->special1 > 0)
	{
		return;
	}
	ball->special1 = 0;
	ball->flags &= ~MF_NOGRAVITY;
	ball->gravity = FRACUNIT/8;
	// [RH] Avoid some precision loss by scaling the momentum directly
#if 0
	angle_t angle = ball->angle>>ANGLETOFINESHIFT;
	ball->momx = FixedMul(7*FRACUNIT, finecosine[angle]);
	ball->momy = FixedMul(7*FRACUNIT, finesine[angle]);
#else
	float momscale = sqrtf ((float)ball->momx * (float)ball->momx +
							(float)ball->momy * (float)ball->momy);
	momscale = 458752.f / momscale;
	ball->momx = (int)(ball->momx * momscale);
	ball->momy = (int)(ball->momy * momscale);
#endif
	ball->momz -= ball->momz>>1;

	// [BC] If we're the server, tell clients to move the object.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		SERVERCOMMANDS_SetThingFlags( ball, FLAGSET_FLAGS );
		SERVERCOMMANDS_SetThingFlags( ball, FLAGSET_FLAGS2 );
		SERVERCOMMANDS_MoveThingExact( ball, CM_X|CM_Y|CM_Z|CM_MOMX|CM_MOMY|CM_MOMZ );
		SERVERCOMMANDS_SetThingGravity( ball );
	}
}

//----------------------------------------------------------------------------
//
// PROC A_MaceBallImpact
//
//----------------------------------------------------------------------------

void A_MaceBallImpact (AActor *ball)
{
	// [BC] Let the server handle this.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		// We need to make sure the ball doesn't temporary go into it's death frame.
		if ( ball->flags & MF_INBOUNCE )
			ball->SetState (ball->SpawnState);

		return;
	}

	if ((ball->health != MAGIC_JUNK) && (ball->flags & MF_INBOUNCE))
	{ // Bounce
		ball->health = MAGIC_JUNK;
		ball->momz = (ball->momz * 192) >> 8;
		ball->flags2 &= ~MF2_BOUNCETYPE;

		// [BC] If we're the server, tell clients to move the object.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		{
			SERVERCOMMANDS_SetThingFlags( ball, FLAGSET_FLAGS2 );
			SERVERCOMMANDS_SetThingState( ball, STATE_SPAWN );
			SERVERCOMMANDS_MoveThingExact( ball, CM_X|CM_Y|CM_Z|CM_MOMX|CM_MOMY|CM_MOMZ );
			SERVERCOMMANDS_SoundActor( ball, CHAN_BODY, "weapons/macebounce", 1, ATTN_NORM );
		}

		ball->SetState (ball->SpawnState);
		S_Sound (ball, CHAN_BODY, "weapons/macebounce", 1, ATTN_NORM);
	}
	else
	{ // Explode
		ball->momx = ball->momy = ball->momz = 0;
		ball->flags |= MF_NOGRAVITY;
		ball->gravity = FRACUNIT;
		S_Sound (ball, CHAN_BODY, "weapons/macehit", 1, ATTN_NORM);

		// [BC] If we're the server, tell clients to move the object.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		{
			SERVERCOMMANDS_SetThingFlags( ball, FLAGSET_FLAGS );
			SERVERCOMMANDS_SetThingFlags( ball, FLAGSET_FLAGS2 );
			SERVERCOMMANDS_MoveThingExact( ball, CM_X|CM_Y|CM_Z|CM_MOMX|CM_MOMY|CM_MOMZ );
			SERVERCOMMANDS_SetThingGravity( ball );
			SERVERCOMMANDS_SoundActor( ball, CHAN_BODY, "weapons/macebounce", 1, ATTN_NORM );
		}
	}
}

//----------------------------------------------------------------------------
//
// PROC A_MaceBallImpact2
//
//----------------------------------------------------------------------------

void A_MaceBallImpact2 (AActor *ball)
{
	AActor *tiny;
	angle_t angle;

	// [BC] Let the server handle this.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		// We need to make sure the ball doesn't temporary go into it's death frame.
		if ( ball->flags & MF_INBOUNCE )
		{
			fixed_t floordist = ball->z - ball->floorz;
			fixed_t ceildist = ball->ceilingz - ball->z;
			fixed_t vel;

			if (floordist <= ceildist)
			{
				vel = MulScale32 (ball->momz, ball->Sector->floorplane.c);
			}
			else
			{
				vel = MulScale32 (ball->momz, ball->Sector->ceilingplane.c);
			}
			if (vel >= 2)
				ball->SetState (ball->SpawnState);
		}

		return;
	}

	if (ball->flags & MF_INBOUNCE)
	{
		fixed_t floordist = ball->z - ball->floorz;
		fixed_t ceildist = ball->ceilingz - ball->z;
		fixed_t vel;

		if (floordist <= ceildist)
		{
			vel = MulScale32 (ball->momz, ball->Sector->floorplane.c);
		}
		else
		{
			vel = MulScale32 (ball->momz, ball->Sector->ceilingplane.c);
		}
		if (vel < 2)
		{
			goto boom;
		}

		// Bounce
		ball->momz = (ball->momz * 192) >> 8;
		ball->SetState (ball->SpawnState);

		// [BC] If we're the server, send the state change and move it.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_MoveThingExact( ball, CM_X|CM_Y|CM_Z|CM_MOMX|CM_MOMY|CM_MOMZ );

		tiny = Spawn("MaceFX3", ball->x, ball->y, ball->z, ALLOW_REPLACE);
		angle = ball->angle+ANG90;
		tiny->target = ball->target;
		tiny->angle = angle;
		angle >>= ANGLETOFINESHIFT;
		tiny->momx = (ball->momx>>1)+FixedMul(ball->momz-FRACUNIT,
			finecosine[angle]);
		tiny->momy = (ball->momy>>1)+FixedMul(ball->momz-FRACUNIT,
			finesine[angle]);
		tiny->momz = ball->momz;

		// [BC] If we're the server, spawn this missile.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SpawnMissileExact( tiny );

		P_CheckMissileSpawn (tiny);

		tiny = Spawn("MaceFX3", ball->x, ball->y, ball->z, ALLOW_REPLACE);
		angle = ball->angle-ANG90;
		tiny->target = ball->target;
		tiny->angle = angle;
		angle >>= ANGLETOFINESHIFT;
		tiny->momx = (ball->momx>>1)+FixedMul(ball->momz-FRACUNIT,
			finecosine[angle]);
		tiny->momy = (ball->momy>>1)+FixedMul(ball->momz-FRACUNIT,
			finesine[angle]);
		tiny->momz = ball->momz;

		// [BC] If we're the server, spawn this missile.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SpawnMissileExact( tiny );

		P_CheckMissileSpawn (tiny);
	}
	else
	{ // Explode
boom:
		ball->momx = ball->momy = ball->momz = 0;
		ball->flags |= MF_NOGRAVITY;
		ball->flags2 &= ~MF2_BOUNCETYPE;
		ball->gravity = FRACUNIT;

		// [BC] If we're the server, tell clients to move the object.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		{
			SERVERCOMMANDS_SetThingState( ball, STATE_DEATH );
			SERVERCOMMANDS_MoveThingExact( ball, CM_X|CM_Y|CM_Z|CM_MOMX|CM_MOMY|CM_MOMZ );
			SERVERCOMMANDS_SetThingFlags( ball, FLAGSET_FLAGS );
			SERVERCOMMANDS_SetThingFlags( ball, FLAGSET_FLAGS2 );
			SERVERCOMMANDS_SetThingGravity( ball );
		}
	}
}

//----------------------------------------------------------------------------
//
// PROC A_FireMacePL2
//
//----------------------------------------------------------------------------

void A_FireMacePL2 (AActor *actor)
{
	AActor *mo;
	player_t *player;
	AActor *linetarget;

	if (NULL == (player = actor->player))
	{
		return;
	}

	AWeapon *weapon = player->ReadyWeapon;
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire))
			return;
	}

	// [BC] If we're the client, play the sound and get out.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		S_Sound( player->mo, CHAN_WEAPON, "weapons/maceshoot", 1, ATTN_NORM );
		return;
	}

	mo = P_SpawnPlayerMissile (actor, 0,0,0, RUNTIME_CLASS(AMaceFX4), actor->angle, &linetarget);
	if (mo)
	{
		mo->momx += actor->momx;
		mo->momy += actor->momy;
		mo->momz = 2*FRACUNIT+
			clamp<fixed_t>(finetangent[FINEANGLES/4-(actor->pitch>>ANGLETOFINESHIFT)], -5*FRACUNIT, 5*FRACUNIT);
		if (linetarget)
		{
			mo->tracer = linetarget;
		}
	}
	S_Sound (actor, CHAN_WEAPON, "weapons/maceshoot", 1, ATTN_NORM);

	// [BC] If we're the server, play the sound.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		SERVERCOMMANDS_MoveThingExact( mo, CM_X|CM_Y|CM_Z|CM_MOMX|CM_MOMY|CM_MOMZ );
		SERVERCOMMANDS_WeaponSound( ULONG( player - players ), "weapons/maceshoot", ULONG( player - players ), SVCF_SKIPTHISCLIENT );
	}

	if ( player->cheats & CF_SPREAD )
	{
		mo = P_SpawnPlayerMissile (actor, 0,0,0, RUNTIME_CLASS(AMaceFX4), actor->angle + ( ANGLE_45 / 3 ), &linetarget);
		if (mo)
		{
			mo->momx += actor->momx;
			mo->momy += actor->momy;
			mo->momz = 2*FRACUNIT+
				clamp<fixed_t>(finetangent[FINEANGLES/4-(actor->pitch>>ANGLETOFINESHIFT)], -5*FRACUNIT, 5*FRACUNIT);
			if (linetarget)
			{
				mo->tracer = linetarget;
			}

			// [BC] If we're the server, play the sound.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_MoveThingExact( mo, CM_X|CM_Y|CM_Z|CM_MOMX|CM_MOMY|CM_MOMZ );
		}

		mo = P_SpawnPlayerMissile (actor, 0,0,0, RUNTIME_CLASS(AMaceFX4), actor->angle - ( ANGLE_45 / 3 ), &linetarget);
		if (mo)
		{
			mo->momx += actor->momx;
			mo->momy += actor->momy;
			mo->momz = 2*FRACUNIT+
				clamp<fixed_t>(finetangent[FINEANGLES/4-(actor->pitch>>ANGLETOFINESHIFT)], -5*FRACUNIT, 5*FRACUNIT);
			if (linetarget)
			{
				mo->tracer = linetarget;
			}

			// [BC] If we're the server, play the sound.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_MoveThingExact( mo, CM_X|CM_Y|CM_Z|CM_MOMX|CM_MOMY|CM_MOMZ );
		}
	}
}

//----------------------------------------------------------------------------
//
// PROC A_DeathBallImpact
//
//----------------------------------------------------------------------------

void A_DeathBallImpact (AActor *ball)
{
	int i;
	AActor *target;
	angle_t angle = 0;
	bool newAngle;
	AActor *linetarget;

	// [BC] Let the server handle this.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		// We need to make sure the ball doesn't temporary go into it's death frame.
		if ( ball->flags & MF_INBOUNCE )
		{
			fixed_t floordist = ball->z - ball->floorz;
			fixed_t ceildist = ball->ceilingz - ball->z;
			fixed_t vel;

			if (floordist <= ceildist)
			{
				vel = MulScale32 (ball->momz, ball->Sector->floorplane.c);
			}
			else
			{
				vel = MulScale32 (ball->momz, ball->Sector->ceilingplane.c);
			}
			if (vel >= 2)
				ball->SetState (ball->SpawnState);
		}

		return;
	}

	if ((ball->z <= ball->floorz) && P_HitFloor (ball))
	{ // Landed in some sort of liquid
		ball->Destroy ();
		return;
	}
	if (ball->flags & MF_INBOUNCE)
	{
		fixed_t floordist = ball->z - ball->floorz;
		fixed_t ceildist = ball->ceilingz - ball->z;
		fixed_t vel;

		if (floordist <= ceildist)
		{
			vel = MulScale32 (ball->momz, ball->Sector->floorplane.c);
		}
		else
		{
			vel = MulScale32 (ball->momz, ball->Sector->ceilingplane.c);
		}
		if (vel < 2)
		{
			goto boom;
		}

		// Bounce
		newAngle = false;
		target = ball->tracer;
		if (target)
		{
			if (!(target->flags&MF_SHOOTABLE))
			{ // Target died
				ball->tracer = NULL;
			}
			else
			{ // Seek
				angle = R_PointToAngle2(ball->x, ball->y,
					target->x, target->y);
				newAngle = true;
			}
		}
		else
		{ // Find new target
			angle = 0;
			for (i = 0; i < 16; i++)
			{
				P_AimLineAttack (ball, angle, 10*64*FRACUNIT, &linetarget);
				if (linetarget && ball->target != linetarget)
				{
					ball->tracer = linetarget;
					angle = R_PointToAngle2 (ball->x, ball->y,
						linetarget->x, linetarget->y);
					newAngle = true;
					break;
				}
				angle += ANGLE_45/2;
			}
		}
		if (newAngle)
		{
			ball->angle = angle;
			angle >>= ANGLETOFINESHIFT;
			ball->momx = FixedMul (ball->Speed, finecosine[angle]);
			ball->momy = FixedMul (ball->Speed, finesine[angle]);
		}
		ball->SetState (ball->SpawnState);
		S_Sound (ball, CHAN_BODY, "weapons/macestop", 1, ATTN_NORM);

		// [BC] If we're the server, send the state change and move it.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		{
			SERVERCOMMANDS_MoveThingExact( ball, CM_X|CM_Y|CM_Z|CM_MOMX|CM_MOMY|CM_MOMZ );
			SERVERCOMMANDS_SoundActor( ball, CHAN_BODY, "weapons/macestop", 1, ATTN_NORM );
		}
	}
	else
	{ // Explode
boom:
		ball->momx = ball->momy = ball->momz = 0;
		ball->flags |= MF_NOGRAVITY;
		ball->gravity = FRACUNIT;
		S_Sound (ball, CHAN_BODY, "weapons/maceexplode", 1, ATTN_NORM);

		// [BC] If we're the server, do some stuff.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		{
			SERVERCOMMANDS_SetThingState( ball, STATE_DEATH );
			SERVERCOMMANDS_MoveThing( ball, CM_X|CM_Y|CM_Z|CM_MOMX|CM_MOMY|CM_MOMZ );
			SERVERCOMMANDS_SetThingFlags( ball, FLAGSET_FLAGS );
			SERVERCOMMANDS_SetThingFlags( ball, FLAGSET_FLAGS2 );
			SERVERCOMMANDS_SetThingGravity( ball );
			SERVERCOMMANDS_SoundActor( ball, CHAN_BODY, "weapons/maceexplode", 1, ATTN_NORM );
		}
	}
}


// Blaster FX 1 -------------------------------------------------------------

class ABlasterFX1 : public AActor
{
	DECLARE_CLASS(ABlasterFX1, AActor)
public:
	void Tick ();
	int DoSpecialDamage (AActor *target, int damage);
};


int ABlasterFX1::DoSpecialDamage (AActor *target, int damage)
{
	if (target->IsKindOf (PClass::FindClass ("Ironlich")))
	{ // Less damage to Ironlich bosses
		damage = pr_bfx1() & 1;
		if (!damage)
		{
			return -1;
		}
	}
	return damage;
}

IMPLEMENT_CLASS(ABlasterFX1)

// Ripper -------------------------------------------------------------------

class ARipper : public AActor
{
	DECLARE_CLASS (ARipper, AActor)
public:
	int DoSpecialDamage (AActor *target, int damage);
};

IMPLEMENT_CLASS(ARipper)

int ARipper::DoSpecialDamage (AActor *target, int damage)
{
	if (target->IsKindOf (PClass::FindClass ("Ironlich")))
	{ // Less damage to Ironlich bosses
		damage = pr_ripd() & 1;
		if (!damage)
		{
			return -1;
		}
	}
	return damage;
}

//----------------------------------------------------------------------------
//
// PROC A_FireBlasterPL1
//
//----------------------------------------------------------------------------

void A_FireBlasterPL1 (AActor *actor)
{
	angle_t angle;
	int damage;
	player_t *player;

	if (NULL == (player = actor->player))
	{
		return;
	}

	AWeapon *weapon = actor->player->ReadyWeapon;
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire))
			return;
	}
	// [BC] If we're the client, just play the sound and get out.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		S_Sound( player->mo, CHAN_WEAPON, "weapons/blastershoot", 1, ATTN_NORM );
		return;
	}

	angle_t pitch = P_BulletSlope(actor);
	damage = pr_fb1.HitDice (4);
	angle = actor->angle;
	if (player->refire)
	{
		angle += pr_fb1.Random2() << 18;
	}
	P_LineAttack (actor, angle, PLAYERMISSILERANGE, pitch, damage, NAME_None, "BlasterPuff");

	// [BC] Apply spread.
	if ( player->cheats & CF_SPREAD )
	{
		P_LineAttack( actor, angle + ( ANGLE_45 / 3 ), PLAYERMISSILERANGE, pitch, damage, NAME_None, "BlasterPuff");
		P_LineAttack( actor, angle - ( ANGLE_45 / 3 ), PLAYERMISSILERANGE, pitch, damage, NAME_None, "BlasterPuff");
	}


	// [BC] If we're the server, tell clients that a weapon is being fired.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_WeaponSound( ULONG( player - players ), "weapons/blastershoot", ULONG( player - players ), SVCF_SKIPTHISCLIENT );

	// [BC] If we're the client, just play the sound and get out.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		S_Sound( player->mo, CHAN_WEAPON, "weapons/blastershoot", 1, ATTN_NORM );
		return;
	}


	// [BC] Apply spread.
	if ( player->cheats & CF_SPREAD )
	{
		P_SpawnPlayerMissile( actor, RUNTIME_CLASS( ABlasterFX1 ), actor->angle + ( ANGLE_45 / 3 ));
		P_SpawnPlayerMissile( actor, RUNTIME_CLASS( ABlasterFX1 ), actor->angle - ( ANGLE_45 / 3 ));
	}

	S_Sound (actor, CHAN_WEAPON, "weapons/blastershoot", 1, ATTN_NORM);
	// [BC] If we're the server, tell clients that a weapon is being fired.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_WeaponSound( ULONG( player - players ), "weapons/blastershoot", ULONG( player - players ), SVCF_SKIPTHISCLIENT );
}

//----------------------------------------------------------------------------
//
// PROC A_SpawnRippers
//
//----------------------------------------------------------------------------

void A_SpawnRippers (AActor *actor)
{
	int i;
	angle_t angle;
	AActor *ripper;

	for(i = 0; i < 8; i++)
	{
		ripper = Spawn<ARipper> (actor->x, actor->y, actor->z, ALLOW_REPLACE);
		angle = i*ANG45;
		ripper->target = actor->target;
		ripper->angle = angle;
		angle >>= ANGLETOFINESHIFT;
		ripper->momx = FixedMul (ripper->Speed, finecosine[angle]);
		ripper->momy = FixedMul (ripper->Speed, finesine[angle]);
		P_CheckMissileSpawn (ripper);
	}
}

//----------------------------------------------------------------------------
//
// PROC P_BlasterMobjThinker
//
// Thinker for the ultra-fast blaster PL2 ripper-spawning missile.
//
//----------------------------------------------------------------------------

void ABlasterFX1::Tick ()
{
	int i;
	fixed_t xfrac;
	fixed_t yfrac;
	fixed_t zfrac;
	int changexy;

	PrevX = x;
	PrevY = y;
	PrevZ = z;

	// Handle movement
	if (momx || momy || (z != floorz) || momz)
	{
		xfrac = momx>>3;
		yfrac = momy>>3;
		zfrac = momz>>3;
		changexy = xfrac | yfrac;
		for (i = 0; i < 8; i++)
		{
			if (changexy)
			{
				if (!P_TryMove (this, x + xfrac, y + yfrac, true))
				{ // Blocked move
					P_ExplodeMissile (this, BlockingLine, BlockingMobj);
					return;
				}
			}
			z += zfrac;
			if (z <= floorz)
			{ // Hit the floor
				z = floorz;
				P_HitFloor (this);
				P_ExplodeMissile (this, NULL, NULL);
				return;
			}
			if (z + height > ceilingz)
			{ // Hit the ceiling
				z = ceilingz - height;
				P_ExplodeMissile (this, NULL, NULL);
				return;
			}
			if (changexy && (pr_bfx1t() < 64))
			{
				Spawn("BlasterSmoke", x, y, MAX<fixed_t> (z - 8 * FRACUNIT, floorz), ALLOW_REPLACE);
			}
		}
	}
	// Advance the state
	if (tics != -1)
	{
		tics--;
		while (!tics)
		{
			if (!SetState (state->GetNextState ()))
			{ // mobj was removed
				return;
			}
		}
	}
}

// --- Skull rod ------------------------------------------------------------


// Horn Rod FX 2 ------------------------------------------------------------

class AHornRodFX2 : public AActor
{
	DECLARE_CLASS (AHornRodFX2, AActor)
public:
	int DoSpecialDamage (AActor *target, int damage);
};

IMPLEMENT_CLASS (AHornRodFX2)

int AHornRodFX2::DoSpecialDamage (AActor *target, int damage)
{
	if (target->IsKindOf (PClass::FindClass("Sorcerer2")) && pr_hrfx2() < 96)
	{ // D'Sparil teleports away
		P_DSparilTeleport (target);
		return -1;
	}
	return damage;
}

// Rain pillar 1 ------------------------------------------------------------

class ARainPillar : public AActor
{
	DECLARE_CLASS (ARainPillar, AActor)
public:
	int DoSpecialDamage (AActor *target, int damage);
};

IMPLEMENT_CLASS (ARainPillar)

int ARainPillar::DoSpecialDamage (AActor *target, int damage)
{
	if (target->flags2 & MF2_BOSS)
	{ // Decrease damage for bosses
		damage = (pr_rp() & 7) + 1;
	}
	return damage;
}

// Rain tracker "inventory" item --------------------------------------------

class ARainTracker : public AInventory
{
	DECLARE_CLASS (ARainTracker, AInventory)
public:
	void Serialize (FArchive &arc);
	AActor *Rain1, *Rain2;
};

IMPLEMENT_CLASS (ARainTracker)
	
void ARainTracker::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << Rain1 << Rain2;
}

//----------------------------------------------------------------------------
//
// PROC A_FireSkullRodPL1
//
//----------------------------------------------------------------------------

void A_FireSkullRodPL1 (AActor *actor)
{
	AActor *mo;
	player_t *player;

	if (NULL == (player = actor->player))
	{
		return;
	}

	AWeapon *weapon = player->ReadyWeapon;
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire))
			return;
	}

	// [BC] Weapons are handled by the server.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return;
	}

	mo = P_SpawnPlayerMissile (actor, PClass::FindClass("HornRodFX1"));
	// Randomize the first frame
	if (mo && pr_fsr1() > 128)
	{
		mo->SetState (mo->state->GetNextState());
	}


	// [BC] Apply spread.
	if ( player->cheats & CF_SPREAD )
	{
		mo = P_SpawnPlayerMissile( actor, PClass::FindClass("HornRodFX1"), actor->angle + ( ANGLE_45 / 3 ));
		// Randomize the first frame
		if (mo && pr_fsr1() > 128)
		{
			mo->SetState (mo->state->GetNextState());
		}

		mo = P_SpawnPlayerMissile( actor, PClass::FindClass("HornRodFX1"), actor->angle - ( ANGLE_45 / 3 ));
		// Randomize the first frame
		if (mo && pr_fsr1() > 128)
		{
			mo->SetState (mo->state->GetNextState());
		}
	}

}

//----------------------------------------------------------------------------
//
// PROC A_FireSkullRodPL2
//
// The special2 field holds the player number that shot the rain missile.
// The special1 field holds the id of the rain sound.
//
//----------------------------------------------------------------------------

void A_FireSkullRodPL2 (AActor *actor)
{
	player_t *player;
	AActor *MissileActor;
	AActor *linetarget;

	if (NULL == (player = actor->player))
	{
		return;
	}
	AWeapon *weapon = player->ReadyWeapon;
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire))
			return;
	}

	// [BC] Weapons are handled by the server.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return;
	}

	P_SpawnPlayerMissile (actor, 0,0,0, RUNTIME_CLASS(AHornRodFX2), actor->angle, &linetarget, &MissileActor);
	// Use MissileActor instead of the return value from
	// P_SpawnPlayerMissile because we need to give info to the mobj
	// even if it exploded immediately.
	if (MissileActor != NULL)
	{
		MissileActor->special2 = (int)(player - players);
		if (linetarget)
		{
			MissileActor->tracer = linetarget;
		}
		S_Sound (MissileActor, CHAN_WEAPON, "weapons/hornrodpowshoot", 1, ATTN_NORM);

		// [BC] If we're the server, play this sound.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SoundActor( MissileActor, CHAN_WEAPON, "weapons/hornrodpowshoot", 1, ATTN_NORM );
	}

	// [BC] Apply spread.
	if ( player->cheats & CF_SPREAD )
	{
		P_SpawnPlayerMissile (actor, 0,0,0, RUNTIME_CLASS(AHornRodFX2), actor->angle + ( ANGLE_45 / 3 ), &linetarget, &MissileActor);
		// Use MissileActor instead of the return value from
		// P_SpawnPlayerMissile because we need to give info to the mobj
		// even if it exploded immediately.
		if (MissileActor != NULL)
		{
			MissileActor->special2 = (int)(player - players);
			if (linetarget)
			{
				MissileActor->tracer = linetarget;
			}
			S_Sound (MissileActor, CHAN_WEAPON, "weapons/hornrodpowshoot", 1, ATTN_NORM);

			// [BC] If we're the server, play this sound.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SoundActor( MissileActor, CHAN_WEAPON, "weapons/hornrodpowshoot", 1, ATTN_NORM );
		}

		P_SpawnPlayerMissile (actor, 0,0,0, RUNTIME_CLASS(AHornRodFX2), actor->angle - ( ANGLE_45 / 3 ), &linetarget, &MissileActor);
		// Use MissileActor instead of the return value from
		// P_SpawnPlayerMissile because we need to give info to the mobj
		// even if it exploded immediately.
		if (MissileActor != NULL)
		{
			MissileActor->special2 = (int)(player - players);
			if (linetarget)
			{
				MissileActor->tracer = linetarget;
			}
			S_Sound (MissileActor, CHAN_WEAPON, "weapons/hornrodpowshoot", 1, ATTN_NORM);

			// [BC] If we're the server, play this sound.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SoundActor( MissileActor, CHAN_WEAPON, "weapons/hornrodpowshoot", 1, ATTN_NORM );
		}
	}
}

//----------------------------------------------------------------------------
//
// PROC A_SkullRodPL2Seek
//
//----------------------------------------------------------------------------

void A_SkullRodPL2Seek (AActor *actor)
{
	P_SeekerMissile (actor, ANGLE_1*10, ANGLE_1*30);
}

//----------------------------------------------------------------------------
//
// PROC A_AddPlayerRain
//
//----------------------------------------------------------------------------

void A_AddPlayerRain (AActor *actor)
{
	ARainTracker *tracker;

	// [BC] Let the server spawn rain.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return;
	}

	if (actor->target == NULL || actor->target->health <= 0)
	{ // Shooter is dead or nonexistant
		return;
	}

	tracker = actor->target->FindInventory<ARainTracker> ();

	// They player is only allowed two rainstorms at a time. Shooting more
	// than that will cause the oldest one to terminate.
	if (tracker != NULL)
	{
		if (tracker->Rain1 && tracker->Rain2)
		{ // Terminate an active rain
			if (tracker->Rain1->health < tracker->Rain2->health)
			{
				if (tracker->Rain1->health > 16)
				{
					tracker->Rain1->health = 16;
				}
				tracker->Rain1 = NULL;
			}
			else
			{
				if (tracker->Rain2->health > 16)
				{
					tracker->Rain2->health = 16;
				}
				tracker->Rain2 = NULL;
			}
		}
	}
	else
	{
		tracker = static_cast<ARainTracker *> (actor->target->GiveInventoryType (RUNTIME_CLASS(ARainTracker)));
	}
	// Add rain mobj to list
	if (tracker->Rain1)
	{
		tracker->Rain2 = actor;
	}
	else
	{
		tracker->Rain1 = actor;
	}
	actor->special1 = S_FindSound ("misc/rain");
}

//----------------------------------------------------------------------------
//
// PROC A_SkullRodStorm
//
//----------------------------------------------------------------------------

void A_SkullRodStorm (AActor *actor)
{
	fixed_t x;
	fixed_t y;
	AActor *mo;
	ARainTracker *tracker;

	if (actor->health-- == 0)
	{
		S_StopSound (actor, CHAN_BODY);
		if (actor->target == NULL)
		{ // Player left the game
			actor->Destroy ();
			return;
		}
		tracker = actor->target->FindInventory<ARainTracker> ();
		if (tracker != NULL)
		{
			if (tracker->Rain1 == actor)
			{
				tracker->Rain1 = NULL;
			}
			else if (tracker->Rain2 == actor)
			{
				tracker->Rain2 = NULL;
			}
		}
		actor->Destroy ();
		return;
	}
	if (pr_storm() < 25)
	{ // Fudge rain frequency
		return;
	}
	x = actor->x + ((pr_storm()&127) - 64) * FRACUNIT;
	y = actor->y + ((pr_storm()&127) - 64) * FRACUNIT;
	mo = Spawn<ARainPillar> (x, y, ONCEILINGZ, ALLOW_REPLACE);
	mo->Translation = ( NETWORK_GetState( ) != NETSTATE_SINGLE ) ?
		TRANSLATION(TRANSLATION_PlayersExtra,actor->special2) : 0;
	mo->target = actor->target;
	mo->momx = 1; // Force collision detection
	mo->momz = -mo->Speed;
	mo->special2 = actor->special2; // Transfer player number
	P_CheckMissileSpawn (mo);
	if (actor->special1 != -1 && !S_IsActorPlayingSomething (actor, CHAN_BODY, -1))
	{
		S_Sound (actor, CHAN_BODY|CHAN_LOOP, actor->special1, 1, ATTN_NORM);
	}
}

//----------------------------------------------------------------------------
//
// PROC A_RainImpact
//
//----------------------------------------------------------------------------

void A_RainImpact (AActor *actor)
{
	if (actor->z > actor->floorz)
	{
		actor->SetState (actor->FindState("NotFloor"));
	}
	else if (pr_impact() < 40)
	{
		P_HitFloor (actor);
	}
}

//----------------------------------------------------------------------------
//
// PROC A_HideInCeiling
//
//----------------------------------------------------------------------------

void A_HideInCeiling (AActor *actor)
{
	actor->z = actor->ceilingz + 4*FRACUNIT;
}

// --- Phoenix Rod ----------------------------------------------------------

void A_FirePhoenixPL1 (AActor *);
void A_InitPhoenixPL2 (AActor *);
void A_FirePhoenixPL2 (AActor *);
void A_ShutdownPhoenixPL2 (AActor *);
void A_PhoenixPuff (AActor *);
void A_FlameEnd (AActor *);
void A_FloatPuff (AActor *);

// Phoenix Rod --------------------------------------------------------------

class APhoenixRod : public AHereticWeapon
{
	DECLARE_ACTOR (APhoenixRod, AHereticWeapon)
public:
	void Serialize (FArchive &arc)
	{
		Super::Serialize (arc);
		arc << FlameCount;
	}
	int FlameCount;		// for flamethrower duration
};

class APhoenixRodPowered : public APhoenixRod
{
	DECLARE_STATELESS_ACTOR (APhoenixRodPowered, APhoenixRod)
public:
	void EndPowerup ();
};

FState APhoenixRod::States[] =
{
#define S_WPHX 0
	S_NORMAL (WPHX, 'A',   -1, NULL 						, NULL),

#define S_PHOENIXREADY (S_WPHX+1)
	S_NORMAL (PHNX, 'A',	1, A_WeaponReady				, &States[S_PHOENIXREADY]),

#define S_PHOENIXDOWN (S_PHOENIXREADY+1)
	S_NORMAL (PHNX, 'A',	1, A_Lower						, &States[S_PHOENIXDOWN]),

#define S_PHOENIXUP (S_PHOENIXDOWN+1)
	S_NORMAL (PHNX, 'A',	1, A_Raise						, &States[S_PHOENIXUP]),

#define S_PHOENIXATK1 (S_PHOENIXUP+1)
	S_NORMAL (PHNX, 'B',	5, NULL 						, &States[S_PHOENIXATK1+1]),
	S_NORMAL (PHNX, 'C',	7, A_FirePhoenixPL1 			, &States[S_PHOENIXATK1+2]),
	S_NORMAL (PHNX, 'D',	4, NULL 						, &States[S_PHOENIXATK1+3]),
	S_NORMAL (PHNX, 'B',	4, NULL 						, &States[S_PHOENIXATK1+4]),
	S_NORMAL (PHNX, 'B',	0, A_ReFire 					, &States[S_PHOENIXREADY]),

#define S_PHOENIXATK2 (S_PHOENIXATK1+5)
	S_NORMAL (PHNX, 'B',	3, A_InitPhoenixPL2 			, &States[S_PHOENIXATK2+1]),
	S_BRIGHT (PHNX, 'C',	1, A_FirePhoenixPL2 			, &States[S_PHOENIXATK2+2]),
	S_NORMAL (PHNX, 'B',	4, A_ReFire 					, &States[S_PHOENIXATK2+3]),
	S_NORMAL (PHNX, 'B',	4, A_ShutdownPhoenixPL2 		, &States[S_PHOENIXREADY])
};

IMPLEMENT_ACTOR (APhoenixRod, Heretic, 2003, 29)
	PROP_Flags (MF_SPECIAL)
	PROP_SpawnState (S_WPHX)

	PROP_Weapon_Flags (WIF_NOAUTOFIRE)
	PROP_Weapon_SelectionOrder (2600)
	PROP_Weapon_AmmoUse1 (USE_PHRD_AMMO_1)
	PROP_Weapon_AmmoGive1 (2)
	PROP_Weapon_UpState (S_PHOENIXUP)
	PROP_Weapon_DownState (S_PHOENIXDOWN)
	PROP_Weapon_ReadyState (S_PHOENIXREADY)
	PROP_Weapon_AtkState (S_PHOENIXATK1)
	PROP_Weapon_YAdjust (15)
	PROP_Weapon_AmmoType1 ("PhoenixRodAmmo")
	PROP_Weapon_SisterType ("PhoenixRodPowered")
	PROP_Weapon_ProjectileType ("PhoenixFX1")
	PROP_Inventory_PickupMessage("$TXT_WPNPHOENIXROD")
END_DEFAULTS

IMPLEMENT_STATELESS_ACTOR (APhoenixRodPowered, Heretic, -1, 0)
	PROP_Weapon_Flags (WIF_NOAUTOFIRE|WIF_POWERED_UP)
	PROP_Weapon_AmmoUse1 (USE_PHRD_AMMO_2)
	PROP_Weapon_AmmoGive1 (0)
	PROP_Weapon_AtkState (S_PHOENIXATK2)
	PROP_Weapon_HoldAtkState (S_PHOENIXATK2+1)
	PROP_Weapon_SisterType ("PhoenixRod")
	PROP_Weapon_ProjectileType ("PhoenixFX2")
END_DEFAULTS

void APhoenixRodPowered::EndPowerup ()
{
	P_SetPsprite (Owner->player, ps_weapon, &APhoenixRod::States[S_PHOENIXREADY]);
	DepleteAmmo (bAltFire);
	Owner->player->refire = 0;
	S_StopSound (Owner, CHAN_WEAPON);
	Owner->player->ReadyWeapon = SisterWeapon;
}

// Phoenix FX 1 -------------------------------------------------------------

FState APhoenixFX1::States[] =
{
#define S_PHOENIXFX1 0
	S_BRIGHT (FX04, 'A',	4, A_PhoenixPuff			, &States[S_PHOENIXFX1+0]),

#define S_PHOENIXFXI1 (S_PHOENIXFX1+1)
	S_BRIGHT (FX08, 'A',	6, A_Explode				, &States[S_PHOENIXFXI1+1]),
	S_BRIGHT (FX08, 'B',	5, NULL 					, &States[S_PHOENIXFXI1+2]),
	S_BRIGHT (FX08, 'C',	5, NULL 					, &States[S_PHOENIXFXI1+3]),
	S_BRIGHT (FX08, 'D',	4, NULL 					, &States[S_PHOENIXFXI1+4]),
	S_BRIGHT (FX08, 'E',	4, NULL 					, &States[S_PHOENIXFXI1+5]),
	S_BRIGHT (FX08, 'F',	4, NULL 					, &States[S_PHOENIXFXI1+6]),
	S_BRIGHT (FX08, 'G',	4, NULL 					, &States[S_PHOENIXFXI1+7]),
	S_BRIGHT (FX08, 'H',	4, NULL 					, NULL)
};

IMPLEMENT_ACTOR (APhoenixFX1, Heretic, -1, 163)
	PROP_RadiusFixed (11)
	PROP_HeightFixed (8)
	PROP_SpeedFixed (20)
	PROP_Damage (20)
	PROP_DamageType (NAME_Fire)
	PROP_Flags (MF_NOBLOCKMAP|MF_MISSILE|MF_DROPOFF|MF_NOGRAVITY)
	PROP_Flags2 (MF2_THRUGHOST|MF2_NOTELEPORT|MF2_PCROSS|MF2_IMPACT)
	PROP_RenderStyle (STYLE_Add)

	PROP_SpawnState (S_PHOENIXFX1)
	PROP_DeathState (S_PHOENIXFXI1)

	PROP_SeeSound ("weapons/phoenixshoot")
	PROP_DeathSound ("weapons/phoenixhit")
END_DEFAULTS

int APhoenixFX1::DoSpecialDamage (AActor *target, int damage)
{
	if (target->IsKindOf (PClass::FindClass("Sorcerer2")) && pr_hrfx2() < 96)
	{ // D'Sparil teleports away
		P_DSparilTeleport (target);
		return -1;
	}
	return damage;
}

// Phoenix puff -------------------------------------------------------------

FState APhoenixPuff::States[] =
{
	S_NORMAL (FX04, 'B',	4, NULL 					, &States[1]),
	S_NORMAL (FX04, 'C',	4, NULL 					, &States[2]),
	S_NORMAL (FX04, 'D',	4, NULL 					, &States[3]),
	S_NORMAL (FX04, 'E',	4, NULL 					, &States[4]),
	S_NORMAL (FX04, 'F',	4, NULL 					, NULL),
};

IMPLEMENT_ACTOR (APhoenixPuff, Heretic, -1, 0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY)
	PROP_Flags2 (MF2_NOTELEPORT|MF2_CANNOTPUSH)
	PROP_RenderStyle (STYLE_Translucent)
	PROP_Alpha (HR_SHADOW)

	PROP_SpawnState (0)
END_DEFAULTS

// Phoenix FX 2 -------------------------------------------------------------

class APhoenixFX2 : public AActor
{
	DECLARE_ACTOR (APhoenixFX2, AActor)
public:
	int DoSpecialDamage (AActor *target, int damage);
};

FState APhoenixFX2::States[] =
{
#define S_PHOENIXFX2 0
	S_BRIGHT (FX09, 'A',	2, NULL 					, &States[S_PHOENIXFX2+1]),
	S_BRIGHT (FX09, 'B',	2, NULL 					, &States[S_PHOENIXFX2+2]),
	S_BRIGHT (FX09, 'A',	2, NULL 					, &States[S_PHOENIXFX2+3]),
	S_BRIGHT (FX09, 'B',	2, NULL 					, &States[S_PHOENIXFX2+4]),
	S_BRIGHT (FX09, 'A',	2, NULL 					, &States[S_PHOENIXFX2+5]),
	S_BRIGHT (FX09, 'B',	2, A_FlameEnd				, &States[S_PHOENIXFX2+6]),
	S_BRIGHT (FX09, 'C',	2, NULL 					, &States[S_PHOENIXFX2+7]),
	S_BRIGHT (FX09, 'D',	2, NULL 					, &States[S_PHOENIXFX2+8]),
	S_BRIGHT (FX09, 'E',	2, NULL 					, &States[S_PHOENIXFX2+9]),
	S_BRIGHT (FX09, 'F',	2, NULL 					, NULL),

#define S_PHOENIXFXI2 (S_PHOENIXFX2+10)
	S_BRIGHT (FX09, 'G',	3, NULL 					, &States[S_PHOENIXFXI2+1]),
	S_BRIGHT (FX09, 'H',	3, A_FloatPuff				, &States[S_PHOENIXFXI2+2]),
	S_BRIGHT (FX09, 'I',	4, NULL 					, &States[S_PHOENIXFXI2+3]),
	S_BRIGHT (FX09, 'J',	5, NULL 					, &States[S_PHOENIXFXI2+4]),
	S_BRIGHT (FX09, 'K',	5, NULL 					, NULL)
};

IMPLEMENT_ACTOR (APhoenixFX2, Heretic, -1, 0)
	PROP_RadiusFixed (6)
	PROP_HeightFixed (8)
	PROP_SpeedFixed (10)
	PROP_Damage (2)
	PROP_DamageType (NAME_Fire)
	PROP_Flags (MF_NOBLOCKMAP|MF_MISSILE|MF_DROPOFF|MF_NOGRAVITY)
	PROP_Flags2 (MF2_NOTELEPORT|MF2_PCROSS|MF2_IMPACT)
	PROP_RenderStyle (STYLE_Add)

	PROP_SpawnState (S_PHOENIXFX2)
	PROP_DeathState (S_PHOENIXFXI2)
END_DEFAULTS

int APhoenixFX2::DoSpecialDamage (AActor *target, int damage)
{
	if (target->player && pr_pfx2 () < 128)
	{ // Freeze player for a bit
		target->reactiontime += 4;
	}
	return damage;
}

//----------------------------------------------------------------------------
//
// PROC A_FirePhoenixPL1
//
//----------------------------------------------------------------------------

void A_FirePhoenixPL1 (AActor *actor)
{
	angle_t angle;
	player_t *player;

	if (NULL == (player = actor->player))
	{
		return;
	}

	AWeapon *weapon = actor->player->ReadyWeapon;
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire))
			return;
	}

	// [BC] Weapons are handled by the server.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return;
	}

	P_SpawnPlayerMissile (actor, RUNTIME_CLASS(APhoenixFX1));

	// [BC] Apply spread.
	if ( player->cheats & CF_SPREAD )
	{
		P_SpawnPlayerMissile( actor, RUNTIME_CLASS( APhoenixFX1 ), actor->angle + ( ANGLE_45 / 3 ));
		P_SpawnPlayerMissile( actor, RUNTIME_CLASS( APhoenixFX1 ), actor->angle - ( ANGLE_45 / 3 ));
	}

	angle = actor->angle + ANG180;
	angle >>= ANGLETOFINESHIFT;
	actor->momx += FixedMul (4*FRACUNIT, finecosine[angle]);
	actor->momy += FixedMul (4*FRACUNIT, finesine[angle]);

	// [BC] Push the player back even more if they are using spread.
	if ( player->cheats & CF_SPREAD )
	{
		actor->momx += FixedMul( 4*FRACUNIT, finecosine[angle] ) * 2;
		actor->momy += FixedMul( 4*FRACUNIT, finesine[angle] ) * 2;
	}
}

//----------------------------------------------------------------------------
//
// PROC A_PhoenixPuff
//
//----------------------------------------------------------------------------

void A_PhoenixPuff (AActor *actor)
{
	AActor *puff;
	angle_t angle;

	//[RH] Heretic never sets the target for seeking
	//P_SeekerMissile (actor, ANGLE_1*5, ANGLE_1*10);
	puff = Spawn<APhoenixPuff> (actor->x, actor->y, actor->z, ALLOW_REPLACE);
	angle = actor->angle + ANG90;
	angle >>= ANGLETOFINESHIFT;
	puff->momx = FixedMul (FRACUNIT*13/10, finecosine[angle]);
	puff->momy = FixedMul (FRACUNIT*13/10, finesine[angle]);
	puff->momz = 0;
	puff = Spawn<APhoenixPuff> (actor->x, actor->y, actor->z, ALLOW_REPLACE);
	angle = actor->angle - ANG90;
	angle >>= ANGLETOFINESHIFT;
	puff->momx = FixedMul (FRACUNIT*13/10, finecosine[angle]);
	puff->momy = FixedMul (FRACUNIT*13/10, finesine[angle]);
	puff->momz = 0;
}

//----------------------------------------------------------------------------
//
// PROC A_InitPhoenixPL2
//
//----------------------------------------------------------------------------

void A_InitPhoenixPL2 (AActor *actor)
{
	if (actor->player != NULL)
	{
		APhoenixRod *flamethrower = static_cast<APhoenixRod *> (actor->player->ReadyWeapon);
		if (flamethrower != NULL)
		{
			flamethrower->FlameCount = FLAME_THROWER_TICS;
		}
	}
}

//----------------------------------------------------------------------------
//
// PROC A_FirePhoenixPL2
//
// Flame thrower effect.
//
//----------------------------------------------------------------------------

void A_FirePhoenixPL2 (AActor *actor)
{
	AActor *mo;
	angle_t angle;
	fixed_t x, y, z;
	fixed_t slope;
	FSoundID soundid;
	player_t *player;
	APhoenixRod *flamethrower;

	if (NULL == (player = actor->player))
	{
		return;
	}

	soundid = "weapons/phoenixpowshoot";

	flamethrower = static_cast<APhoenixRod *> (player->ReadyWeapon);
	if (flamethrower == NULL || --flamethrower->FlameCount == 0)
	{ // Out of flame
		P_SetPsprite (player, ps_weapon, &APhoenixRod::States[S_PHOENIXATK2+3]);
		player->refire = 0;
		S_StopSound (actor, CHAN_WEAPON);
		return;
	}

	// [BC] If we're the client, just play the sound and get out.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		if (!player->refire || !S_IsActorPlayingSomething (player->mo, CHAN_WEAPON, -1))
		{
			S_Sound (player->mo, CHAN_WEAPON|CHAN_LOOP, soundid, 1, ATTN_NORM);
		}
		return;
	}

	angle = actor->angle;
	x = actor->x + (pr_fp2.Random2() << 9);
	y = actor->y + (pr_fp2.Random2() << 9);
	z = actor->z + 26*FRACUNIT + finetangent[FINEANGLES/4-(actor->pitch>>ANGLETOFINESHIFT)];
	z -= actor->floorclip;
	slope = finetangent[FINEANGLES/4-(actor->pitch>>ANGLETOFINESHIFT)] + (FRACUNIT/10);
	mo = Spawn<APhoenixFX2> (x, y, z, ALLOW_REPLACE);
	mo->target = actor;
	mo->angle = angle;
	mo->momx = actor->momx + FixedMul (mo->Speed, finecosine[angle>>ANGLETOFINESHIFT]);
	mo->momy = actor->momy + FixedMul (mo->Speed, finesine[angle>>ANGLETOFINESHIFT]);
	mo->momz = FixedMul (mo->Speed, slope);
	if (!player->refire || !S_IsActorPlayingSomething (actor, CHAN_WEAPON, -1))
	{
		S_Sound (actor, CHAN_WEAPON|CHAN_LOOP, soundid, 1, ATTN_NORM);
	}	

	// [BC] Apply spread.
	if ( player->cheats & CF_SPREAD )
	{
		angle = actor->angle + ( ANGLE_45 / 3 );
		mo = Spawn<APhoenixFX2> (x, y, z, ALLOW_REPLACE);
		mo->target = actor;
		mo->angle = angle;
		mo->momx = actor->momx + FixedMul (mo->Speed, finecosine[angle>>ANGLETOFINESHIFT]);
		mo->momy = actor->momy + FixedMul (mo->Speed, finesine[angle>>ANGLETOFINESHIFT]);
		mo->momz = FixedMul (mo->Speed, slope);
		if (!player->refire || !S_IsActorPlayingSomething (actor, CHAN_WEAPON, -1))
		{
			S_Sound (actor, CHAN_WEAPON|CHAN_LOOP, soundid, 1, ATTN_NORM);
		}

		angle = actor->angle - ( ANGLE_45 / 3 );
		mo = Spawn<APhoenixFX2> (x, y, z, ALLOW_REPLACE);
		mo->target = actor;
		mo->angle = angle;
		mo->momx = actor->momx + FixedMul (mo->Speed, finecosine[angle>>ANGLETOFINESHIFT]);
		mo->momy = actor->momy + FixedMul (mo->Speed, finesine[angle>>ANGLETOFINESHIFT]);
		mo->momz = FixedMul (mo->Speed, slope);
		if (!player->refire || !S_IsActorPlayingSomething (actor, CHAN_WEAPON, -1))
		{
			S_Sound (actor, CHAN_WEAPON|CHAN_LOOP, soundid, 1, ATTN_NORM);
		}
	}

	// [BC] If we're the server, spawn this missile.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SpawnMissile( mo );

	P_CheckMissileSpawn (mo);
}

//----------------------------------------------------------------------------
//
// PROC A_ShutdownPhoenixPL2
//
//----------------------------------------------------------------------------

void A_ShutdownPhoenixPL2 (AActor *actor)
{
	player_t *player;

	if (NULL == (player = actor->player))
	{
		return;
	}
	S_StopSound (actor, CHAN_WEAPON);
	AWeapon *weapon = player->ReadyWeapon;
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire))
			return;
	}
}

//----------------------------------------------------------------------------
//
// PROC A_FlameEnd
//
//----------------------------------------------------------------------------

void A_FlameEnd (AActor *actor)
{
	actor->momz += FRACUNIT*3/2;
}

//----------------------------------------------------------------------------
//
// PROC A_FloatPuff
//
//----------------------------------------------------------------------------

void A_FloatPuff (AActor *puff)
{
	puff->momz += FRACUNIT*18/10;
}

