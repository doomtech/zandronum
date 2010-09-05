/*
#include "actor.h"
#include "info.h"
#include "s_sound.h"
#include "m_random.h"
#include "a_pickups.h"
#include "d_player.h"
#include "p_pspr.h"
#include "p_local.h"
#include "gstrings.h"
#include "p_effect.h"
#include "gi.h"
#include "templates.h"
#include "thingdef/thingdef.h"
#include "doomstat.h"
*/

static FRandom pr_punch ("Punch");
static FRandom pr_saw ("Saw");
static FRandom pr_fireshotgun2 ("FireSG2");
static FRandom pr_fireplasma ("FirePlasma");
static FRandom pr_firerail ("FireRail");
static FRandom pr_bfgspray ("BFGSpray");

//
// A_Punch
//
DEFINE_ACTION_FUNCTION(AActor, A_Punch)
{
	angle_t 	angle;
	int 		damage;
	int 		pitch;
	AActor		*linetarget;

	// [BC] Weapons are handled by the server.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return;
	}

	if (self->player != NULL)
	{
		AWeapon *weapon = self->player->ReadyWeapon;
		if (weapon != NULL)
		{
			if (!weapon->DepleteAmmo (weapon->bAltFire))
				return;
		}
	}

	damage = (pr_punch()%10+1)<<1;

	if (self->FindInventory<APowerStrength>())	
		damage *= 10;

	angle = self->angle;

	angle += pr_punch.Random2() << 18;
	pitch = P_AimLineAttack (self, angle, MELEERANGE, &linetarget);
	P_LineAttack (self, angle, MELEERANGE, pitch, damage, NAME_Melee, NAME_BulletPuff, true);

	// [BC] Apply spread.
	if (( self->player ) && ( self->player->cheats & CF_SPREAD ))
	{
		P_LineAttack( self, angle + ( ANGLE_45 / 3 ), MELEERANGE, pitch, damage, NAME_Melee, NAME_BulletPuff, true);
		P_LineAttack( self, angle - ( ANGLE_45 / 3 ), MELEERANGE, pitch, damage, NAME_Melee, NAME_BulletPuff, true);
	}

	// [BC] If the player hit a player with his attack, potentially give him a medal.
	if ( self->player )
	{
		if ( self->player->bStruckPlayer )
			PLAYER_StruckPlayer( self->player );
		else
			self->player->ulConsecutiveHits = 0;

		// Tell all the bots that a weapon was fired.
		BOTS_PostWeaponFiredEvent( ULONG( self->player - players ), BOTEVENT_USEDFIST, BOTEVENT_ENEMY_USEDFIST, BOTEVENT_PLAYER_USEDFIST );
	}

	// turn to face target
	if (linetarget)
	{
		S_Sound (self, CHAN_WEAPON, "*fist", 1, ATTN_NORM);
		self->angle = R_PointToAngle2 (self->x,
										self->y,
										linetarget->x,
										linetarget->y);

		// [BC] Play the hit sound to clients.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		{
			SERVERCOMMANDS_SoundActor( self, CHAN_WEAPON, "*fist", 1, ATTN_NORM );
			SERVERCOMMANDS_SetThingAngleExact( self );
		}
	}
}

//
// A_FirePistol
//
void A_CustomFireBullets( AActor *self,
						  angle_t Spread_XY,
						  angle_t Spread_Z, 
						  int NumberOfBullets,
						  int DamagePerBullet,
						  const PClass * PuffType,
						  bool UseAmmo = true,
						  fixed_t Range = 0);

DEFINE_ACTION_FUNCTION(AActor, A_FirePistol)
{
	// [BB] A_FirePistol is only kept to stay compatible with Dehacked.
	A_CustomFireBullets( self, angle_t( 5.6 * ANGLE_1), angle_t( 0 * ANGLE_1), 1, 5, PClass::FindClass("BulletPuff") );
	CALL_ACTION ( A_GunFlash, self );
/*
	bool accurate;

	if (self->player != NULL)
	{
		AWeapon *weapon = self->player->ReadyWeapon;
		if (weapon != NULL)
		{
			if (!weapon->DepleteAmmo (weapon->bAltFire))
				return;

			P_SetPsprite (self->player, ps_flash, weapon->FindState(NAME_Flash));
		}
		self->player->mo->PlayAttacking2 ();

		// [BC] If we're the server, tell clients to update this player's state.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetPlayerState( ULONG( self->player - players ), STATE_PLAYER_ATTACK2, ULONG( self->player - players ), SVCF_SKIPTHISCLIENT );

		accurate = !self->player->refire;
	}
	else
	{
		accurate = true;
	}

	// [BC] If we're the server, tell clients that a weapon is being fired.
	if (( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( actor->player ))
		SERVERCOMMANDS_WeaponSound( ULONG( self->player - players ), "weapons/pistol", ULONG( self->player - players ), SVCF_SKIPTHISCLIENT );

	S_Sound (self, CHAN_WEAPON, "weapons/pistol", 1, ATTN_NORM);


	// [BC] Weapons are handled by the server.
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
		return;

	P_GunShot (self, accurate, PClass::FindClass(NAME_BulletPuff), P_BulletSlope (self));

	// [BC] Apply spread.
	if (( actor->player ) && ( actor->player->cheats & CF_SPREAD ))
	{
		fixed_t		SavedActorAngle;

		SavedActorAngle = actor->angle;
		actor->angle += ( ANGLE_45 / 3 );
		P_GunShot (actor, accurate, PClass::FindClass(NAME_BulletPuff), P_BulletSlope (actor));
		actor->angle = SavedActorAngle;

		SavedActorAngle = actor->angle;
		actor->angle -= ( ANGLE_45 / 3 );
		P_GunShot (actor, accurate, PClass::FindClass(NAME_BulletPuff), P_BulletSlope (actor));
		actor->angle = SavedActorAngle;
	}

	// [BC] If the player hit a player with his attack, potentially give him a medal.
	if ( actor->player )
	{
		if ( actor->player->bStruckPlayer )
			PLAYER_StruckPlayer( actor->player );
		else
			actor->player->ulConsecutiveHits = 0;

		// Tell all the bots that a weapon was fired.
		BOTS_PostWeaponFiredEvent( ULONG( actor->player - players ), BOTEVENT_FIREDPISTOL, BOTEVENT_ENEMY_FIREDPISTOL, BOTEVENT_PLAYER_FIREDPISTOL );
	}
*/
}

//
// A_Saw
//
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_Saw)
{
	angle_t 	angle;
	player_t *player;
	AActor *linetarget;

	// [BC] Weapons are handled by the server.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return;
	}
	
	ACTION_PARAM_START(4);
	ACTION_PARAM_SOUND(fullsound, 0);
	ACTION_PARAM_SOUND(hitsound, 1);
	ACTION_PARAM_INT(damage, 2);
	ACTION_PARAM_CLASS(pufftype, 3);



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

	if (pufftype == NULL) pufftype = PClass::FindClass(NAME_BulletPuff);
	if (damage == 0) damage = 2;
	
	damage *= (pr_saw()%10+1);
	angle = self->angle;
	angle += pr_saw.Random2() << 18;
	
	// use meleerange + 1 so the puff doesn't skip the flash (i.e. plays all states)
	P_LineAttack (self, angle, MELEERANGE+1,
				  P_AimLineAttack (self, angle, MELEERANGE+1, &linetarget), damage,
				  GetDefaultByType(pufftype)->DamageType, pufftype);

	// [BC] Apply spread.
	if ( player->cheats & CF_SPREAD )
	{
		P_LineAttack( self, angle + ( ANGLE_45 / 3 ), MELEERANGE + 1,
					  P_AimLineAttack( self, angle + ( ANGLE_45 / 3 ), MELEERANGE + 1, &linetarget ), damage,
					  NAME_None, pufftype );

		P_LineAttack( self, angle - ( ANGLE_45 / 3 ), MELEERANGE + 1,
					  P_AimLineAttack( self, angle - ( ANGLE_45 / 3 ), MELEERANGE + 1, &linetarget ), damage,
					  NAME_None, pufftype );
	}

	// [BC] If the player hit a player with his attack, potentially give him a medal.
	if ( self->player->bStruckPlayer )
		PLAYER_StruckPlayer( self->player );
	else
		self->player->ulConsecutiveHits = 0;

	// [BC] If we're the server, tell clients that a weapon is being fired.
//	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
//		SERVERCOMMANDS_WeaponSound( ULONG( player - players ), linetarget ? hitsound : fullsound, ULONG( player - players ), SVCF_SKIPTHISCLIENT );

	if (!linetarget)
	{
		S_Sound (self, CHAN_WEAPON, fullsound, 1, ATTN_NORM);

		// [BC] If we're the server, tell clients to play the saw sound.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SoundActor( self, CHAN_WEAPON, S_GetName( fullsound ), 1, ATTN_NORM );
		return;
	}
	S_Sound (self, CHAN_WEAPON, hitsound, 1, ATTN_NORM);
	// [BC] If we're the server, tell clients to play the saw sound.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SoundActor( self, CHAN_WEAPON, S_GetName( hitsound ), 1, ATTN_NORM );
		
	// turn to face target
	angle = R_PointToAngle2 (self->x, self->y,
							 linetarget->x, linetarget->y);
	if (angle - self->angle > ANG180)
	{
		if (angle - self->angle < (angle_t)(-ANG90/20))
			self->angle = angle + ANG90/21;
		else
			self->angle -= ANG90/20;
	}
	else
	{
		if (angle - self->angle > ANG90/20)
			self->angle = angle - ANG90/21;
		else
			self->angle += ANG90/20;
	}
	self->flags |= MF_JUSTATTACKED;

	// [BC] If we're the server, tell clients to adjust the player's angle.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SetThingAngleExact( self );

	// [BC] Tell all the bots that a weapon was fired.
	BOTS_PostWeaponFiredEvent( ULONG( self->player - players ), BOTEVENT_USEDCHAINSAW, BOTEVENT_ENEMY_USEDCHAINSAW, BOTEVENT_PLAYER_USEDCHAINSAW );
}

//
// A_FireShotgun
//
DEFINE_ACTION_FUNCTION(AActor, A_FireShotgun)
{
	// [BB] A_FireShotgun is only kept to stay compatible with Dehacked.
	A_CustomFireBullets( self, angle_t( 5.6 * ANGLE_1), angle_t( 0 * ANGLE_1), 7, 5, PClass::FindClass("BulletPuff") );
	CALL_ACTION ( A_GunFlash, self );
/*
	int i;
	player_t *player;

	if (NULL == (player = self->player))
	{
		return;
	}

	// [BC] If we're the server, tell clients that a weapon is being fired.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_WeaponSound( ULONG( player - players ), "weapons/shotgf", ULONG( player - players ), SVCF_SKIPTHISCLIENT );

	S_Sound (self, CHAN_WEAPON,  "weapons/shotgf", 1, ATTN_NORM);
	AWeapon *weapon = self->player->ReadyWeapon;
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire))
			return;
		P_SetPsprite (player, ps_flash, weapon->FindState(NAME_Flash));
	}
	player->mo->PlayAttacking2 ();

	angle_t pitch = P_BulletSlope (self);

	// [BC] If we're the server, tell clients to update this player's state.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SetPlayerState( ULONG( player - players ), STATE_PLAYER_ATTACK2, ULONG( player - players ), SVCF_SKIPTHISCLIENT );

	// [BC] Weapons are handled by the server.
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
		return;


	for (i=0 ; i<7 ; i++)
		P_GunShot (self, false, PClass::FindClass(NAME_BulletPuff), pitch);

	// [BC] Apply spread.
	if ( player->cheats & CF_SPREAD )
	{
		fixed_t		SavedActorAngle;

		SavedActorAngle = self->angle;
		self->angle += ( ANGLE_45 / 3 );
		for (i=0 ; i<7 ; i++)
			P_GunShot (self, false, PClass::FindClass(NAME_BulletPuff), pitch);
		self->angle = SavedActorAngle;

		SavedActorAngle = actor->angle;
		self->angle -= ( ANGLE_45 / 3 );
		for (i=0 ; i<7 ; i++)
			P_GunShot (self, false, PClass::FindClass(NAME_BulletPuff), pitch);
		self->angle = SavedActorAngle;
	}

	// [BC] If the player hit a player with his attack, potentially give him a medal.
	if ( player->bStruckPlayer )
		PLAYER_StruckPlayer( player );
	else
		player->ulConsecutiveHits = 0;

	// [BC] Tell all the bots that a weapon was fired.
	BOTS_PostWeaponFiredEvent( ULONG( player - players ), BOTEVENT_FIREDSHOTGUN, BOTEVENT_ENEMY_FIREDSHOTGUN, BOTEVENT_PLAYER_FIREDSHOTGUN );
*/
}

//
// A_FireShotgun2
//
DEFINE_ACTION_FUNCTION(AActor, A_FireShotgun2)
{
	// [BB] A_FireShotgun2 is only kept to stay compatible with Dehacked.
	A_CustomFireBullets( self, angle_t( 11.2 * ANGLE_1), angle_t( 7.1 * ANGLE_1), 20, 5, PClass::FindClass("BulletPuff") );
	CALL_ACTION ( A_GunFlash, self );
/*
	int 		i;
	angle_t 	angle;
	int 		damage;
	player_t *player;

	if (NULL == (player = self->player))
	{
		return;
	}

	// [BC] If we're the server, tell clients that a weapon is being fired.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_WeaponSound( ULONG( player - players ), "weapons/sshotf", ULONG( player - players ), SVCF_SKIPTHISCLIENT );

	S_Sound (self, CHAN_WEAPON, "weapons/sshotf", 1, ATTN_NORM);
	AWeapon *weapon = self->player->ReadyWeapon;
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire))
			return;
		P_SetPsprite (player, ps_flash, weapon->FindState(NAME_Flash));
	}
	player->mo->PlayAttacking2 ();

	// [BC] If we're the server, tell clients to update this player's state.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SetPlayerState( ULONG( player - players ), STATE_PLAYER_ATTACK2, ULONG( player - players ), SVCF_SKIPTHISCLIENT );

	// [BC] Weapons are handled by the server.
	// [BB] To make hitscan decals kinda work online, we may not stop here yet.
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT && !cl_hitscandecalhack )
		return;

	angle_t pitch = P_BulletSlope (self);
		
	for (i=0 ; i<20 ; i++)
	{
		damage = 5*(pr_fireshotgun2()%3+1);
		angle = self->angle;
		angle += pr_fireshotgun2.Random2() << 19;

		// Doom adjusts the bullet slope by shifting a random number [-255,255]
		// left 5 places. At 2048 units away, this means the vertical position
		// of the shot can deviate as much as 255 units from nominal. So using
		// some simple trigonometry, that means the vertical angle of the shot
		// can deviate by as many as ~7.097 degrees or ~84676099 BAMs.

		P_LineAttack (self,
					  angle,
					  PLAYERMISSILERANGE,
					  pitch + (pr_fireshotgun2.Random2() * 332063), damage,
					  NAME_None, NAME_BulletPuff);

		// [BC] Apply spread.
		if ( player->cheats & CF_SPREAD )
		{
			P_LineAttack (actor,
						  angle + ( ANGLE_45 / 3 ),
						  PLAYERMISSILERANGE,
						  pitch + (pr_fireshotgun2.Random2() * 332063), damage,
						  NAME_None, NAME_BulletPuff);

			P_LineAttack (actor,
						  angle - ( ANGLE_45 / 3 ),
						  PLAYERMISSILERANGE,
						  pitch + (pr_fireshotgun2.Random2() * 332063), damage,
						  NAME_None, NAME_BulletPuff);
		}
	}

	// [BB] Even with the online hitscan decal hack, a client has to stop here.
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
		return;

	// [BC] If the player hit a player with his attack, potentially give him a medal.
	if ( player->bStruckPlayer )
		PLAYER_StruckPlayer( player );
	else
		player->ulConsecutiveHits = 0;

	// [BC] Tell all the bots that a weapon was fired.
	BOTS_PostWeaponFiredEvent( ULONG( player - players ), BOTEVENT_FIREDSSG, BOTEVENT_ENEMY_FIREDSSG, BOTEVENT_PLAYER_FIREDSSG );
*/
}

DEFINE_ACTION_FUNCTION(AActor, A_OpenShotgun2)
{
	S_Sound (self, CHAN_WEAPON, "weapons/sshoto", 1, ATTN_NORM);

	// [BC] If we're the server, tell clients that a weapon is being fired.
	if (( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( self->player ))
		SERVERCOMMANDS_WeaponSound( ULONG( self->player - players ), "weapons/sshoto", ULONG( self->player - players ), SVCF_SKIPTHISCLIENT );
}

DEFINE_ACTION_FUNCTION(AActor, A_LoadShotgun2)
{
	S_Sound (self, CHAN_WEAPON, "weapons/sshotl", 1, ATTN_NORM);

	// [BC] If we're the server, tell clients that a weapon is being fired.
	if (( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( self->player ))
		SERVERCOMMANDS_WeaponSound( ULONG( self->player - players ), "weapons/sshotl", ULONG( self->player - players ), SVCF_SKIPTHISCLIENT );
}

DEFINE_ACTION_FUNCTION(AActor, A_CloseShotgun2)
{
	S_Sound (self, CHAN_WEAPON, "weapons/sshotc", 1, ATTN_NORM);
	CALL_ACTION(A_ReFire, self);

	// [BC] If we're the server, tell clients that a weapon is being fired.
	if (( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( self->player ))
		SERVERCOMMANDS_WeaponSound( ULONG( self->player - players ), "weapons/sshotc", ULONG( self->player - players ), SVCF_SKIPTHISCLIENT );
}

//------------------------------------------------------------------------------------
//
// Setting a random flash like some of Doom's weapons can easily crash when the
// definition is overridden incorrectly so let's check that the state actually exists.
// Be aware though that this will not catch all DEHACKED related problems. But it will
// find all DECORATE related ones.
//
//------------------------------------------------------------------------------------

void P_SetSafeFlash(AWeapon * weapon, player_t * player, FState * flashstate, int index)
{

	const PClass * cls = weapon->GetClass();
	while (cls != RUNTIME_CLASS(AWeapon))
	{
		FActorInfo * info = cls->ActorInfo;
		if (flashstate >= info->OwnedStates && flashstate < info->OwnedStates + info->NumOwnedStates)
		{
			// The flash state belongs to this class.
			// Now let's check if the actually wanted state does also
			if (flashstate+index < info->OwnedStates + info->NumOwnedStates)
			{
				// we're ok so set the state
				P_SetPsprite (player, ps_flash, flashstate + index);
				return;
			}
			else
			{
				// oh, no! The state is beyond the end of the state table so use the original flash state.
				P_SetPsprite (player, ps_flash, flashstate);
				return;
			}
		}
		// try again with parent class
		cls = cls->ParentClass;
	}
	// if we get here the state doesn't seem to belong to any class in the inheritance chain
	// This can happen with Dehacked if the flash states are remapped. 
	// The only way to check this would be to go through all Dehacked modifiable actors and
	// find the correct one.
	// For now let's assume that it will work.
	P_SetPsprite (player, ps_flash, flashstate + index);
}

//
// A_FireCGun
//
DEFINE_ACTION_FUNCTION(AActor, A_FireCGun)
{
	// [BC] I guess we have to do all this old stuff to ensure that the flash state on the
	// chaingun ends up on the right frame. Shouldn't it just work?
	player_t *player = self->player;

	if (NULL == player)
	{
		return;
	}
	player->mo->PlayAttacking2 ();

	AWeapon *weapon = player->ReadyWeapon;
	if (weapon != NULL)
	{

		FState *flash = weapon->FindState(NAME_Flash);
		if (flash != NULL)
		{
			FState * atk = weapon->FindState(NAME_Fire);

			int theflash = clamp (int(player->psprites[ps_weapon].state - atk), 0, 1);

			if (flash[theflash].sprite != flash->sprite)
			{
				theflash = 0;
			}
			P_SetSafeFlash (weapon, player, flash, theflash);
		}
	}
	// [BB] A_FireCGun is only kept to stay compatible with Dehacked.
	A_CustomFireBullets( self, angle_t( 5.6 * ANGLE_1), angle_t( 0 * ANGLE_1), 1, 5, PClass::FindClass("BulletPuff") );
//	A_GunFlash( self );
/*
	player_t *player;

	if (self == NULL || NULL == (player = self->player))
	{
		return;
	}

	AWeapon *weapon = player->ReadyWeapon;
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire))
			return;
		
		S_Sound (self, CHAN_WEAPON, "weapons/chngun", 1, ATTN_NORM);

		// [BC] If we're the server, tell clients that a weapon is being fired.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_WeaponSound( ULONG( player - players ), "weapons/chngun", ULONG( player - players ), SVCF_SKIPTHISCLIENT );

		FState *flash = weapon->FindState(NAME_Flash);
		if (flash != NULL)
		{
			// [RH] Fix for Sparky's messed-up Dehacked patch! Blargh!
			FState * atk = weapon->FindState(NAME_Fire);

			int theflash = clamp (int(player->psprites[ps_weapon].state - atk), 0, 1);

			if (flash[theflash].sprite != flash->sprite)
			{
				theflash = 0;
			}

			P_SetSafeFlash (weapon, player, flash, theflash);
		}

	}
	player->mo->PlayAttacking2 ();

	// [BC] If we're the server, tell clients to update this player's state.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SetPlayerState( ULONG( player - players ), STATE_PLAYER_ATTACK2, ULONG( player - players ), SVCF_SKIPTHISCLIENT );

	// [BC] Weapons are handled by the server.
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
		return;

	P_GunShot (self, !player->refire, PClass::FindClass(NAME_BulletPuff), P_BulletSlope (self));

	// [BC] Apply apread.
	if ( player->cheats & CF_SPREAD )
	{
		fixed_t		SavedActorAngle;

		SavedActorAngle = actor->angle;
		self->angle += ( ANGLE_45 / 3 );
		P_GunShot (self, !player->refire, PClass::FindClass(NAME_BulletPuff), P_BulletSlope (self));
		self->angle = SavedActorAngle;
	
		SavedActorAngle = self->angle;
		self->angle -= ( ANGLE_45 / 3 );
		P_GunShot (self, !player->refire, PClass::FindClass(NAME_BulletPuff), P_BulletSlope (self));
		self->angle = SavedActorAngle;
	}

	// [BC] If the player hit a player with his attack, potentially give him a medal.
	if ( player->bStruckPlayer )
		PLAYER_StruckPlayer( player );
	else
		player->ulConsecutiveHits = 0;

	// [BC] Tell all the bots that a weapon was fired.
	BOTS_PostWeaponFiredEvent( ULONG( player - players ), BOTEVENT_FIREDCHAINGUN, BOTEVENT_ENEMY_FIREDCHAINGUN, BOTEVENT_PLAYER_FIREDCHAINGUN );
*/
}

// [BB] Once the DECORATE version of the Minigun is sufficiently tested, the code here should be removed.
// Minigun -----------------------------------------------------------------
/*
void A_FireMiniGun (AActor *);

class AMinigun : public AWeapon
{
	DECLARE_ACTOR (AMinigun, AWeapon)
};

FState AMinigun::States[] =
{
#define S_MINI 0
	S_NORMAL (MNGG, 'A',	1, A_WeaponReady		, &States[S_MINI]),

#define S_MINIDOWN (S_MINI+1)
	S_NORMAL (MNGG, 'A',	1, A_Lower				, &States[S_MINIDOWN]),

#define S_MINIUP (S_MINIDOWN+1)
	S_NORMAL (MNGG, 'A',	1, A_Raise				, &States[S_MINIUP]),

#define S_MINI1 (S_MINIUP+1)
	S_NORMAL (MNGG, 'A',	2, A_FireMiniGun		, &States[S_MINI1+1]),
	S_NORMAL (MNGG, 'B',	2, A_FireMiniGun		, &States[S_MINI1+2]),
	S_NORMAL (MNGG, 'A',	2, A_ReFire 			, &States[S_MINI1+3]),
	S_NORMAL (MNGG, 'B',	2, NULL		 			, &States[S_MINI1+4]),
	S_NORMAL (MNGG, 'A',	4, NULL		 			, &States[S_MINI1+5]),
	S_NORMAL (MNGG, 'B',	4, NULL		 			, &States[S_MINI1+6]),
	S_NORMAL (MNGG, 'A',	8, NULL					, &States[S_MINI1+7]),
	S_NORMAL (MNGG, 'B',	8, NULL		 			, &States[S_MINI]),

#define S_MINIFLASH (S_MINI1+8)
	S_BRIGHT (MNGF, 'A',	5, A_Light1 			, &AWeapon::States[S_LIGHTDONE]),
	S_BRIGHT (MNGF, 'B',	5, A_Light2 			, &AWeapon::States[S_LIGHTDONE]),

#define S_MINIGUN (S_MINIFLASH+2)
	S_NORMAL (MNGN, 'A',   -1, NULL 				, NULL)
};

IMPLEMENT_ACTOR (AMinigun, Doom, 5014, 214)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (16)
	PROP_Flags (MF_SPECIAL)
	PROP_SpawnState (S_MINIGUN)

	PROP_Weapon_SelectionOrder (700)
	PROP_Weapon_AmmoUse1 (1)
	PROP_Weapon_AmmoGive1 (20)
	PROP_Weapon_UpState (S_MINIUP)
	PROP_Weapon_DownState (S_MINIDOWN)
	PROP_Weapon_ReadyState (S_MINI)
	PROP_Weapon_AtkState (S_MINI1)
	PROP_Weapon_FlashState (S_MINIFLASH)
	PROP_Weapon_Kickback (100)
	PROP_Weapon_AmmoType1 ("Clip")
	PROP_Obituary("$OB_MINIGUN")
	PROP_Inventory_PickupMessage("$PICKUP_MINIGUN")
END_DEFAULTS

//
// A_FireMiniGun
//
void A_FireMiniGun( AActor *actor )
{
	player_t	*pPlayer;
	AWeapon		*pWeapon;

	if (( actor == NULL ) || ( actor->player == NULL ))
		return;

	pPlayer = actor->player;

	pWeapon = pPlayer->ReadyWeapon;
	if ( pWeapon != NULL )
	{
		// Break out if we don't have enough ammo to fire the weapon. This shouldn't ever happen
		// because the function that puts the weapon into its fire state first checks to see if
		// the weapon has enough ammo to fire.
		if ( pWeapon->DepleteAmmo( pWeapon->bAltFire ) == false )
			return;

		// Play the weapon sound effect.
		S_Sound( actor, CHAN_WEAPON, "weapons/minigun", 1, ATTN_NORM );

		// [BC] If we're the server, tell clients that a weapon is being fired.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_WeaponSound( ULONG( pPlayer - players ), "weapons/minigun", ULONG( pPlayer - players ), SVCF_SKIPTHISCLIENT );

		FState *flash = pWeapon->FindState(NAME_Flash);
		if (flash != NULL)
		{
			// [RH] Fix for Sparky's messed-up Dehacked patch! Blargh!
			FState * atk = pWeapon->FindState(NAME_Fire);

			int theflash = clamp (int(pPlayer->psprites[ps_weapon].state - atk), 0, 1);

			if (flash[theflash].sprite.index != flash->sprite.index)
			{
				theflash = 0;
			}

			P_SetSafeFlash (pWeapon, pPlayer, flash, theflash);
		}
	}

	pPlayer->mo->PlayAttacking2( );

	// If we're the server, tell clients to update this player's state.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SetPlayerState( ULONG( pPlayer - players ), STATE_PLAYER_ATTACK2, ULONG( pPlayer - players ), SVCF_SKIPTHISCLIENT );

	// Weapon firing is server side.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return;
	}

	P_BulletSlope( actor );

	// Minigun is always inaccurate.
	P_GunShot( actor, false, PClass::FindClass(NAME_BulletPuff));

	// Apply spread.
	if ( pPlayer->cheats & CF_SPREAD )
	{
		fixed_t		SavedActorAngle;

		SavedActorAngle = actor->angle;

		actor->angle += ( ANGLE_45 / 3 );
		P_GunShot( actor, false, PClass::FindClass(NAME_BulletPuff));
		actor->angle = SavedActorAngle;

		actor->angle -= ( ANGLE_45 / 3 );
		P_GunShot( actor, false, PClass::FindClass(NAME_BulletPuff));
		actor->angle = SavedActorAngle;
	}

	// If the player hit a player with his attack, potentially give him a medal.
	if ( pPlayer->bStruckPlayer )
		PLAYER_StruckPlayer( pPlayer );
	else
		pPlayer->ulConsecutiveHits = 0;

	// Tell all the bots that a weapon was fired.
	BOTS_PostWeaponFiredEvent( ULONG( pPlayer - players ), BOTEVENT_FIREDMINIGUN, BOTEVENT_ENEMY_FIREDMINIGUN, BOTEVENT_PLAYER_FIREDMINIGUN );
}
*/
//
// A_FireMissile
//
DEFINE_ACTION_FUNCTION(AActor, A_FireMissile)
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
		return;
	}

	P_SpawnPlayerMissile (self, PClass::FindClass("Rocket"));

	// [BC] Apply spread.
	if ( player->cheats & CF_SPREAD )
	{
		P_SpawnPlayerMissile( self, PClass::FindClass("Rocket"), self->angle + ( ANGLE_45 / 3 ), false );
		P_SpawnPlayerMissile( self, PClass::FindClass("Rocket"), self->angle - ( ANGLE_45 / 3 ), false );
	}

	// [BC] Tell all the bots that a weapon was fired.
	BOTS_PostWeaponFiredEvent( ULONG( player - players ), BOTEVENT_FIREDROCKET, BOTEVENT_ENEMY_FIREDROCKET, BOTEVENT_PLAYER_FIREDROCKET );
}

// Grenade launcher ---------------------------------------------------------

/*
void A_FireSTGrenade (AActor *);
void A_Explode (AActor *);
class AGrenadeLauncher : public AWeapon
{
	DECLARE_ACTOR (AGrenadeLauncher, AWeapon)
};

FState AGrenadeLauncher::States[] =
{
#define S_GRENADELAUNCHER 0
	S_NORMAL (GRLG, 'A',	1, A_WeaponReady		, &States[S_GRENADELAUNCHER]),

#define S_GRENADELAUNCHERDOWN (S_GRENADELAUNCHER+1)
	S_NORMAL (GRLG, 'A',	1, A_Lower				, &States[S_GRENADELAUNCHERDOWN]),

#define S_GRENADELAUNCHERUP (S_GRENADELAUNCHERDOWN+1)
	S_NORMAL (GRLG, 'A',	1, A_Raise				, &States[S_GRENADELAUNCHERUP]),

#define S_GRENADELAUNCHER1 (S_GRENADELAUNCHERUP+1)
	S_NORMAL (GRLG, 'B',	8, A_GunFlash			, &States[S_GRENADELAUNCHER1+1]),
	S_NORMAL (GRLG, 'B',   12, A_FireSTGrenade		, &States[S_GRENADELAUNCHER1+2]),
	S_NORMAL (GRLG, 'B',	0, A_ReFire 			, &States[S_GRENADELAUNCHER]),

#define S_GRENADELAUNCHERFLASH (S_GRENADELAUNCHER1+3)
	S_BRIGHT (GRLF, 'A',	3, A_Light1 			, &States[S_GRENADELAUNCHERFLASH+1]),
	S_BRIGHT (GRLF, 'B',	4, NULL 				, &States[S_GRENADELAUNCHERFLASH+2]),
	S_BRIGHT (GRLF, 'C',	4, A_Light2 			, &States[S_GRENADELAUNCHERFLASH+3]),
	S_BRIGHT (GRLF, 'D',	4, A_Light2 			, &AWeapon::States[S_LIGHTDONE]),

#define S_GLAU (S_GRENADELAUNCHERFLASH+4)
	S_NORMAL (GLAU, 'A',   -1, NULL 				, NULL)
};

IMPLEMENT_ACTOR (AGrenadeLauncher, Doom, 5011, 163)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (16)
	PROP_Flags (MF_SPECIAL)
	PROP_SpawnState (S_GLAU)

	PROP_Weapon_SelectionOrder (2500)
	// [BB] Added WIF_NOAUTOAIM.
	PROP_Weapon_Flags (WIF_NOAUTOFIRE|WIF_NOAUTOAIM)
	PROP_Weapon_AmmoUse1 (1)
	PROP_Weapon_AmmoGive1 (2)
	PROP_Weapon_UpState (S_GRENADELAUNCHERUP)
	PROP_Weapon_DownState (S_GRENADELAUNCHERDOWN)
	PROP_Weapon_ReadyState (S_GRENADELAUNCHER)
	PROP_Weapon_AtkState (S_GRENADELAUNCHER1)
	PROP_Weapon_FlashState (S_GRENADELAUNCHERFLASH)
	PROP_Weapon_Kickback (100)
	PROP_Weapon_AmmoType1 ("RocketAmmo")
	// [BB] This doesn't seem to work with the DECORATE version of the rocktet.
	//PROP_Weapon_ProjectileType ("Rocket")
	PROP_Inventory_PickupMessage("$PICKUP_GRENADELAUNCHER")
END_DEFAULTS
*/

// [BC]
class AGrenade : public AActor
{
	DECLARE_CLASS (AGrenade, AActor)
public:
	bool	FloorBounceMissile( secplane_t &plane );
};

IMPLEMENT_CLASS ( AGrenade )
/*
FState AGrenade::States[] =
{
#define S_GRENADE 0
	S_BRIGHT (SGRN, 'A',	1, NULL 						, &States[S_GRENADE]),

#define S_GRENADE_EXPLODE (S_GRENADE+1)
	S_BRIGHT (MISL, 'B',	8, A_Explode					, &States[S_GRENADE_EXPLODE+1]),
	S_BRIGHT (MISL, 'C',	6, NULL 						, &States[S_GRENADE_EXPLODE+2]),
	S_BRIGHT (MISL, 'D',	4, NULL 						, NULL)
};

IMPLEMENT_ACTOR (AGrenade, Doom, -1, 216)
	PROP_RadiusFixed (8)
	PROP_HeightFixed (8)
	PROP_SpeedFixed (25)
	PROP_Damage (20)
	PROP_Flags (MF_NOBLOCKMAP|MF_MISSILE|MF_DROPOFF)
	PROP_Flags2 (MF2_PCROSS|MF2_IMPACT|MF2_DOOMBOUNCE|MF2_NOTELEPORT)
	PROP_Flags4 (MF4_RANDOMIZE)
	PROP_Flags5 (MF5_DEHEXPLOSION)
	PROP_FXFlags (FX_GRENADE)
	PROP_FlagsST( STFL_QUARTERGRAVITY|STFL_EXPLODEONDEATH )

	PROP_SpawnState (S_GRENADE)
	PROP_DeathState (S_GRENADE_EXPLODE)

	PROP_DamageType( NAME_Grenade )
	PROP_SeeSound ("weapons/grenlf")
	PROP_DeathSound ("weapons/grenlx")
	PROP_Obituary("$OB_GRENADE")

END_DEFAULTS
*/

bool AGrenade::FloorBounceMissile( secplane_t &plane )
{
	fixed_t bouncemomx = momx / 4;
	fixed_t bouncemomy = momy / 4;
	fixed_t bouncemomz = FixedMul (momz, (fixed_t)(-0.6*FRACUNIT));
/*
	if (abs (bouncemomz) < (FRACUNIT/2))
	{
		P_ExplodeMissile( this, NULL );
	}
	else
	{
*/
		if (!Super::FloorBounceMissile (plane))
		{
			momx = bouncemomx;
			momy = bouncemomy;
			momz = bouncemomz;
			return false;
		}
//	}
	
	return true;
}

//
// A_FireSTGrenade
//
DEFINE_ACTION_FUNCTION(AActor, A_FireSTGrenade)
{
//	AActor		*pGrenade;
	player_t	*player;
	fixed_t		SavedActorPitch;

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

	// Weapons are handled by the server.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return;
	}

	SavedActorPitch = self->pitch;

	self->pitch = self->pitch - ( 1152 << FRACBITS );
	P_SpawnPlayerMissile( self, RUNTIME_CLASS( AGrenade ));

	// Apply spread.
	if ( player->cheats & CF_SPREAD )
	{
		P_SpawnPlayerMissile( self, RUNTIME_CLASS( AGrenade ), self->angle + ( ANGLE_45 / 3 ), false );
		P_SpawnPlayerMissile( self, RUNTIME_CLASS( AGrenade ), self->angle - ( ANGLE_45 / 3 ), false );
	}
	
	self->pitch = SavedActorPitch;

#if 0
	Printf( "%d\n", self->pitch >> FRACBITS );
	pGrenade = P_SpawnPlayerMissile( self, RUNTIME_CLASS( AGrenade ));
	if ( pGrenade )
		pGrenade->momz += ( 3 * FRACUNIT );

	// Apply spread.
	if ( player->cheats & CF_SPREAD )
	{
		pGrenade = P_SpawnPlayerMissile( self, RUNTIME_CLASS( AGrenade ), self->angle + ( ANGLE_45 / 3 ), false );
		if ( pGrenade )
			pGrenade->momz += ( 3 * FRACUNIT );

		pGrenade = P_SpawnPlayerMissile( self, RUNTIME_CLASS( AGrenade ), self->angle - ( ANGLE_45 / 3 ), false );
		if ( pGrenade )
			pGrenade->momz += ( 3 * FRACUNIT );
	}
#endif

	// Tell all the bots that a weapon was fired.
	BOTS_PostWeaponFiredEvent( ULONG( player - players ), BOTEVENT_FIREDGRENADE, BOTEVENT_ENEMY_FIREDGRENADE, BOTEVENT_PLAYER_FIREDGRENADE );
}

//
// A_FirePlasma
//
DEFINE_ACTION_FUNCTION(AActor, A_FirePlasma)
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

		FState *flash = weapon->FindState(NAME_Flash);
		if (flash != NULL)
		{
			P_SetSafeFlash(weapon, player, flash, (pr_fireplasma()&1));
		}
	}

	// [BC] Weapons are handled by the server.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return;
	}

	P_SpawnPlayerMissile (self, PClass::FindClass("PlasmaBall"));

	// [BC] Apply spread.
	if ( player->cheats & CF_SPREAD )
	{
		P_SpawnPlayerMissile( self, PClass::FindClass("PlasmaBall"), self->angle + ( ANGLE_45 / 3 ), false );
		P_SpawnPlayerMissile( self, PClass::FindClass("PlasmaBall"), self->angle - ( ANGLE_45 / 3 ), false );
	}

	// [BC] Tell all the bots that a weapon was fired.
	BOTS_PostWeaponFiredEvent( ULONG( player - players ), BOTEVENT_FIREDPLASMA, BOTEVENT_ENEMY_FIREDPLASMA, BOTEVENT_PLAYER_FIREDPLASMA );
}

//
// [RH] A_FireRailgun
//
static void FireRailgun(AActor *self, int RailOffset)
{
	int damage;
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

		FState *flash = weapon->FindState(NAME_Flash);
		if (flash != NULL)
		{
			P_SetSafeFlash(weapon, player, flash, (pr_firerail()&1));
		}
	}

	// Weapons are handled by the server.
	// [Spleen] But railgun is an exception if it's unlagged, to make it look nicer
	if ( ( ( NETWORK_GetState( ) == NETSTATE_CLIENT ) || CLIENTDEMO_IsPlaying( ) )
		&& !UNLAGGED_DrawRailClientside( self ) )
	{
		return;
	}

	damage = 200;
	if ( deathmatch || teamgame )
	{
		if ( instagib )
			damage = 1000;
		else
			damage = 75;
	}

	// [BB] This also handles color and spread.
	P_RailAttackWithPossibleSpread (self, damage, RailOffset);

	// [BC] Tell all the bots that a weapon was fired.
	BOTS_PostWeaponFiredEvent( ULONG( player - players ), BOTEVENT_FIREDRAILGUN, BOTEVENT_ENEMY_FIREDRAILGUN, BOTEVENT_PLAYER_FIREDRAILGUN );
}

DEFINE_ACTION_FUNCTION(AActor, A_FireRailgun)
{
	FireRailgun(self, 0);
}

DEFINE_ACTION_FUNCTION(AActor, A_FireRailgunRight)
{
	FireRailgun(self, 10);
}

DEFINE_ACTION_FUNCTION(AActor, A_FireRailgunLeft)
{
	FireRailgun(self, -10);
}

DEFINE_ACTION_FUNCTION(AActor, A_RailWait)
{
	// Okay, this was stupid. Just use a NULL function instead of this.
}

// Railgun ------------------------------------------------------------
/*
void A_CheckRailReload( AActor *pActor );

class ARailgun : public AWeapon
{
	DECLARE_ACTOR (ARailgun, AWeapon)
};

FState ARailgun::States[] =
{
#define S_RAILGUN 0
	S_NORMAL (RLGG, 'A',	1, A_WeaponReady		, &States[S_RAILGUN]),

#define S_RAILGUNDOWN (S_RAILGUN+1)
	S_NORMAL (RLGG, 'A',	1, A_Lower				, &States[S_RAILGUNDOWN]),

#define S_RAILGUNUP (S_RAILGUNDOWN+1)
	S_NORMAL (RLGG, 'A',	1, A_Raise				, &States[S_RAILGUNUP]),

#define S_RAILGUN1 (S_RAILGUNUP+1)
	S_NORMAL (RLGG, 'E',	12, A_FireRailgun 		, &States[S_RAILGUN1+1]),
	S_NORMAL (RLGG, 'F',	6, A_CheckRailReload	, &States[S_RAILGUN1+2]),
	S_NORMAL (RLGG, 'G',	6, NULL					, &States[S_RAILGUN1+3]),
	S_NORMAL (RLGG, 'H',	6, NULL					, &States[S_RAILGUN1+4]),
	S_NORMAL (RLGG, 'I',	6, NULL					, &States[S_RAILGUN1+5]),
	S_NORMAL (RLGG, 'J',	6, NULL					, &States[S_RAILGUN1+6]),
	S_NORMAL (RLGG, 'K',	6, NULL					, &States[S_RAILGUN1+7]),
	S_NORMAL (RLGG, 'L',	6, NULL					, &States[S_RAILGUN1+8]),
	S_NORMAL (RLGG, 'A',	6, NULL					, &States[S_RAILGUN1+9]),
	S_NORMAL (RLGG, 'M',	0, A_ReFire				, &States[S_RAILGUN]),

#define S_RAILGUNFLASH (S_RAILGUN1+10)
	S_BRIGHT (TNT1, 'A',	5, A_Light1 			, &States[S_RAILGUNFLASH+1]),
	S_BRIGHT (TNT1, 'A',	5, A_Light2 			, &AWeapon::States[S_LIGHTDONE]),

#define S_RAIL (S_RAILGUNFLASH+2)
	S_NORMAL (RAIL, 'A',	-1, NULL 				, NULL)
};

IMPLEMENT_ACTOR (ARailgun, Doom, 5012, 164)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (16)
	PROP_Flags (MF_SPECIAL)
	PROP_SpawnState (S_RAIL)

	PROP_Weapon_SelectionOrder (100)
	PROP_Weapon_AmmoUse1 (10)
	PROP_Weapon_AmmoGive1 (40)
	PROP_Weapon_UpState (S_RAILGUNUP)
	PROP_Weapon_DownState (S_RAILGUNDOWN)
	PROP_Weapon_ReadyState (S_RAILGUN)
	PROP_Weapon_AtkState (S_RAILGUN1)
	PROP_Weapon_FlashState (S_RAILGUNFLASH)
	PROP_Weapon_Kickback (100)
	PROP_Weapon_AmmoType1 ("Cell")
	PROP_Inventory_PickupMessage("$PICKUP_RAILGUN")
END_DEFAULTS
*/
DEFINE_ACTION_FUNCTION(AActor, A_CheckRailReload)
{
	if ( self->player == NULL )
		return;

	self->player->ulRailgunShots++;
	// If we have not made our 4th shot...
	if ((( self->player->ulRailgunShots % 4 ) == 0 ) == false )
	{
		// Go back to the refire frames, instead of continuing on to the reload frames.
		if ( self->player->ReadyWeapon->GetClass( ) == PClass::FindClass("Railgun" ))
			P_SetPsprite( self->player, ps_weapon, self->player->ReadyWeapon->FindState(NAME_Fire) + 8 );
	}
	else
		// We need to reload. However, don't reload if we're out of ammo.
		self->player->ReadyWeapon->CheckAmmo( false, false );
}

//
// A_FireBFG
//

DEFINE_ACTION_FUNCTION(AActor, A_FireBFG)
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
		return;
	}


	P_SpawnPlayerMissile (self,  0, 0, 0, PClass::FindClass("BFGBall"), self->angle, NULL, NULL, !(dmflags2 & DF2_YES_FREEAIMBFG));

	// [BC] Apply spread.
	if ( player->cheats & CF_SPREAD )
	{
		P_SpawnPlayerMissile (self,  0, 0, 0, PClass::FindClass("BFGBall"), self->angle + ( ANGLE_45 / 3 ), NULL, NULL, !(dmflags2 & DF2_YES_FREEAIMBFG), false );
		P_SpawnPlayerMissile (self,  0, 0, 0, PClass::FindClass("BFGBall"), self->angle - ( ANGLE_45 / 3 ), NULL, NULL, !(dmflags2 & DF2_YES_FREEAIMBFG), false );
	}

}

//
// A_BFGSpray
// Spawn a BFG explosion on every monster in view
//
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_BFGSpray)
{
	int 				i;
	int 				j;
	int 				damage;
	angle_t 			an;
	AActor				*thingToHit;
	AActor				*linetarget;

	// [BC] This is not done on the client end.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return;
	}

	ACTION_PARAM_START(3);
	ACTION_PARAM_CLASS(spraytype, 0);
	ACTION_PARAM_INT(numrays, 1);
	ACTION_PARAM_INT(damagecnt, 2);

	if (spraytype == NULL) spraytype = PClass::FindClass("BFGExtra");
	if (numrays <= 0) numrays = 40;
	if (damagecnt <= 0) damagecnt = 15;

	// [RH] Don't crash if no target
	if (!self->target)
		return;

	// offset angles from its attack angle
	for (i = 0; i < numrays; i++)
	{
		an = self->angle - ANG90/2 + ANG90/numrays*i;

		// self->target is the originator (player) of the missile
		P_AimLineAttack (self->target, an, 16*64*FRACUNIT, &linetarget, ANGLE_1*32);

		if (!linetarget)
			continue;

		AActor *spray = Spawn (spraytype, linetarget->x, linetarget->y,
			linetarget->z + (linetarget->height>>2), ALLOW_REPLACE);

		if (spray && (spray->flags5 & MF5_PUFFGETSOWNER))
			spray->target = self->target;
		
		// [BC] Tell clients to spawn the tracers.
		if (( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( spray ))
			SERVERCOMMANDS_SpawnThing( spray );

		damage = 0;
		for (j = 0; j < damagecnt; ++j)
			damage += (pr_bfgspray() & 7) + 1;

		thingToHit = linetarget;
		P_DamageMobj (thingToHit, self->target, self->target, damage, NAME_BFGSplash);
		P_TraceBleed (damage, thingToHit, self->target);
	}
}

//
// A_BFGsound
//
DEFINE_ACTION_FUNCTION(AActor, A_BFGsound)
{
	S_Sound (self, CHAN_WEAPON, "weapons/bfgf", 1, ATTN_NORM);

	// [BC] Tell the clients to trigger the BFG firing sound.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		if ( self->player )
			SERVERCOMMANDS_SoundActor( self, CHAN_WEAPON, "weapons/bfgf", 1, ATTN_NORM, ULONG( self->player - players ), SVCF_SKIPTHISCLIENT );
		else
			SERVERCOMMANDS_SoundActor( self, CHAN_WEAPON, "weapons/bfgf", 1, ATTN_NORM );
	}

	if ( self->player )
	{
		// Tell all the bots that a weapon was fired.
		BOTS_PostWeaponFiredEvent( ULONG( self->player - players ), BOTEVENT_FIREDBFG, BOTEVENT_ENEMY_FIREDBFG, BOTEVENT_PLAYER_FIREDBFG );
	}
}

// BFG 10k -----------------------------------------------------------------
/*
void A_FireBFG10k( AActor *pActor );
void A_BFG10kSound( AActor *pActor );
void A_BFG10kCoolDown( AActor *pActor );

class ABFG10K : public AWeapon
{
	DECLARE_ACTOR (ABFG10K, AWeapon)
};

FState ABFG10K::States[] =
{
#define S_BFG10K 0
	S_NORMAL (BFG2, 'A',   -1, NULL 				, NULL),

#define S_BFG10KIDLE (S_BFG10K + 1)
	S_NORMAL (BG2G, 'A',	1, A_WeaponReady		, &States[S_BFG10KIDLE+1]),
	S_NORMAL (BG2G, 'A',	1, A_WeaponReady		, &States[S_BFG10KIDLE+2]),
	S_NORMAL (BG2G, 'A',	1, A_WeaponReady		, &States[S_BFG10KIDLE+3]),
	S_NORMAL (BG2G, 'B',	1, A_WeaponReady		, &States[S_BFG10KIDLE+4]),
	S_NORMAL (BG2G, 'B',	1, A_WeaponReady		, &States[S_BFG10KIDLE+5]),
	S_NORMAL (BG2G, 'B',	1, A_WeaponReady		, &States[S_BFG10KIDLE+6]),
	S_NORMAL (BG2G, 'C',	1, A_WeaponReady		, &States[S_BFG10KIDLE+7]),
	S_NORMAL (BG2G, 'C',	1, A_WeaponReady		, &States[S_BFG10KIDLE+8]),
	S_NORMAL (BG2G, 'C',	1, A_WeaponReady		, &States[S_BFG10KIDLE+9]),
	S_NORMAL (BG2G, 'D',	1, A_WeaponReady		, &States[S_BFG10KIDLE+10]),
	S_NORMAL (BG2G, 'D',	1, A_WeaponReady		, &States[S_BFG10KIDLE+11]),
	S_NORMAL (BG2G, 'D',	1, A_WeaponReady		, &States[S_BFG10KIDLE]),

#define S_BFG10KDOWN (S_BFG10KIDLE+12)
	S_NORMAL (BG2G, 'E',	1, A_Lower				, &States[S_BFG10KDOWN]),

#define S_BFG10KUP (S_BFG10KDOWN+1)
	S_NORMAL (BG2G, 'E',	1, A_Raise				, &States[S_BFG10KUP]),

#define S_BFG10KATK1 (S_BFG10KUP+1)
	S_NORMAL (BG2G, 'E',   20, A_BFG10kSound			, &States[S_BFG10KATK1+1]),
	S_NORMAL (BG2G, 'F',   4, NULL					, &States[S_BFG10KATK1+2]),
	S_NORMAL (BG2G, 'G',   1, NULL					, &States[S_BFG10KATK1+3]),
	S_NORMAL (BG2G, 'H',   1, NULL					, &States[S_BFG10KATK1+4]),
	S_NORMAL (BG2G, 'I',   1, NULL					, &States[S_BFG10KATK1+5]),
	S_NORMAL (BG2G, 'J',   1, NULL					, &States[S_BFG10KATK1+6]),
	S_NORMAL (BG2G, 'K',   2, A_GunFlash			, &States[S_BFG10KATK1+7]),
	S_NORMAL (BG2G, 'L',   2, A_FireBFG10k			, &States[S_BFG10KATK1+8]),
	S_NORMAL (BG2G, 'M',   2, NULL					, &States[S_BFG10KATK1+9]),
	S_NORMAL (BG2G, 'N',   0, A_ReFire 				, &States[S_BFG10KATK1+10]),
	S_NORMAL (BG2G, 'O',   35, A_BFG10kCoolDown		, &States[S_BFG10KIDLE]),

#define S_BFG10KFLASH (S_BFG10KATK1+11)
	S_BRIGHT (TNT1, 'A',	2, A_Light1 			, &States[S_BFG10KFLASH+1]),
	S_BRIGHT (TNT1, 'A',	3, NULL		 			, &AWeapon::States[S_LIGHTDONE]),
	S_BRIGHT (TNT1, 'A',	3, A_Light2 			, &AWeapon::States[S_LIGHTDONE]),
	S_BRIGHT (TNT1, 'A',	3, A_Light2 			, &AWeapon::States[S_LIGHTDONE]),
};

IMPLEMENT_ACTOR (ABFG10K, Doom, 5013, 165)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (20)
	PROP_Flags (MF_SPECIAL)
	PROP_SpawnState (S_BFG10K)

	PROP_Weapon_Flags (WIF_NOAUTOFIRE|WIF_RADIUSDAMAGE_BOSSES|WIF_NOLMS)
	PROP_Weapon_SelectionOrder (2800)
	PROP_Weapon_AmmoUse1 (5)
//	PROP_Weapon_AmmoUseDM1 (10)
	PROP_Weapon_AmmoGive1 (40)
	PROP_Weapon_UpState (S_BFG10KUP)
	PROP_Weapon_DownState (S_BFG10KDOWN)
	PROP_Weapon_ReadyState (S_BFG10KIDLE)
	PROP_Weapon_AtkState (S_BFG10KATK1)
	PROP_Weapon_HoldAtkState (S_BFG10KATK1+6)
	PROP_Weapon_FlashState (S_BFG10KFLASH)
	PROP_Weapon_Kickback (100)
	PROP_Weapon_AmmoType1 ("Cell")
	PROP_Weapon_ProjectileType ("BFG10kShot")
	PROP_Weapon_ReadySound ("weapons/bfg10kidle")
	PROP_Inventory_PickupMessage("$PICKUP_BFG10K")
END_DEFAULTS

class ABFG10kShot : public AActor
{
	DECLARE_ACTOR (ABFG10kShot, AActor)
public:
	void BeginPlay ();
};

FState ABFG10kShot::States[] =
{
// [BB] Due to Carn's request I removed the 1 frame delay between when the BFG10K shot
// explodes and the damages is dealt. This probably breaks the obituaries of the 10K online.
// The delay causes a huge problem with time freeze spheres: Turns out if you fire it a bunch
// and run up to the explosions, once the time freeze wears off you kind of die unexpectedly.
#define S_BFG10KSHOT (0)
	S_BRIGHT (BFE1, 'A',	3, A_Detonate					, &States[S_BFG10KSHOT+1]),
	S_BRIGHT (BFE1, 'B',	3, NULL 						, &States[S_BFG10KSHOT+2]),
	S_BRIGHT (BFE1, 'C',	3, NULL							, &States[S_BFG10KSHOT+3]),
	S_BRIGHT (BFE1, 'D',	3, NULL 						, &States[S_BFG10KSHOT+4]),
	S_BRIGHT (BFE1, 'E',	3, NULL 						, &States[S_BFG10KSHOT+5]),
	S_BRIGHT (BFE1, 'F',	3, NULL 						, NULL)
};

IMPLEMENT_ACTOR (ABFG10kShot, Doom, -1, 217)
	PROP_RadiusFixed (11)
	PROP_HeightFixed (8)
	PROP_SpeedFixed (20)
	PROP_Damage (160)
	PROP_Flags (MF_NOBLOCKMAP|MF_DROPOFF|MF_NOGRAVITY)
	PROP_Flags2 (MF2_PCROSS|MF2_IMPACT|MF2_NOTELEPORT)
	PROP_Flags3 (MF3_PUFFONACTORS)
	PROP_RenderStyle (STYLE_Add)
	PROP_Alpha (TRANSLUC75)
	PROP_DamageType( NAME_BFG10k )

	PROP_SpawnState (S_BFG10KSHOT)

	PROP_SeeSound ("weapons/bfg10kx")
	PROP_AttackSound ("weapons/bfg10kx")
	PROP_Obituary( "$OB_BFG10K" )
END_DEFAULTS

// [BC] EWEWEWEWEEEWEEW
extern	AActor	*shootthing;
void ABFG10kShot::BeginPlay ()
{
	Super::BeginPlay ( );

	// BFG10k shots create an explosion, in which case we need to know the target.
	if ( shootthing && shootthing->player )
		target = shootthing;

	SetState( this->SpawnState );
}

//
// A_FireBFG10k
//
void A_FireBFG10k( AActor *pActor )
{
	player_t	*pPlayer;
	AWeapon		*pWeapon;

	pPlayer = pActor->player;
	if ( pPlayer == NULL )
		return;

	pWeapon = pPlayer->ReadyWeapon;
	if ( pWeapon != NULL )
	{
		// Break out if we don't have enough ammo to fire the weapon. This shouldn't ever happen
		// because the function that puts the weapon into its fire state first checks to see if
		// the weapon has enough ammo to fire.
		if ( pWeapon->DepleteAmmo( pWeapon->bAltFire ) == false )
			return;
	}

	// Weapons are handled by the server.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return;
	}

	P_GunShot( pActor, true, RUNTIME_CLASS( ABFG10kShot ), P_BulletSlope( pActor ));

	// Apply spread.
	if ( pPlayer->cheats & CF_SPREAD )
	{
		fixed_t		SavedActorAngle;

		SavedActorAngle = pActor->angle;

		pActor->angle += ( ANGLE_45 / 3 );
		P_GunShot( pActor, true, RUNTIME_CLASS( ABFG10kShot ), P_BulletSlope( pActor ));
		pActor->angle = SavedActorAngle;

		pActor->angle -= ( ANGLE_45 / 3 );
		P_GunShot( pActor, true, RUNTIME_CLASS( ABFG10kShot ), P_BulletSlope( pActor ));
		pActor->angle = SavedActorAngle;
	}

	// Tell all the bots that a weapon was fired.
	BOTS_PostWeaponFiredEvent( ULONG( pPlayer - players ), BOTEVENT_FIREDBFG10K, BOTEVENT_ENEMY_FIREDBFG10K, BOTEVENT_PLAYER_FIREDBFG10K );
}

//
// A_BFG10kSound
//
void A_BFG10kSound( AActor *actor )
{
	S_Sound( actor, CHAN_WEAPON, "weapons/bfg10kf", 1, ATTN_NORM );

	// Tell the clients to trigger the BFG firing sound.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		if ( actor->player )
			SERVERCOMMANDS_SoundActor( actor, CHAN_WEAPON, "weapons/bfg10kf", 1, ATTN_NORM, ULONG( actor->player - players ), SVCF_SKIPTHISCLIENT );
		else
			SERVERCOMMANDS_SoundActor( actor, CHAN_WEAPON, "weapons/bfg10kf", 1, ATTN_NORM );
	}
}

//
// A_BFG10kCoolDown
//
void A_BFG10kCoolDown( AActor *actor )
{
	S_Sound( actor, CHAN_WEAPON, "weapons/bfg10kcool", 1, ATTN_NORM );

	// Tell the clients to trigger the BFG firing sound.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		if ( actor->player )
			SERVERCOMMANDS_SoundActor( actor, CHAN_WEAPON, "weapons/bfg10kcool", 1, ATTN_NORM, ULONG( actor->player - players ), SVCF_SKIPTHISCLIENT );
		else
			SERVERCOMMANDS_SoundActor( actor, CHAN_WEAPON, "weapons/bfg10kcool", 1, ATTN_NORM );
	}
}
*/
