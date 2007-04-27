#ifndef __A_DOOMGLOBAL_H__
#define __A_DOOMGLOBAL_H__

#include "dobject.h"
#include "info.h"
#include "d_player.h"
#include "gstrings.h"
#include "network.h"
#include "sv_commands.h"

class ABossBrain : public AActor
{
	DECLARE_ACTOR (ABossBrain, AActor)
};

class AExplosiveBarrel : public AActor
{
	DECLARE_ACTOR (AExplosiveBarrel, AActor)
};

class ABulletPuff : public AActor
{
	DECLARE_ACTOR (ABulletPuff, AActor)
public:
	void BeginPlay ();
};

class ARocket : public AActor
{
	DECLARE_ACTOR (ARocket, AActor)
public:
};

// [BC]
class AGrenade : public AActor
{
	DECLARE_ACTOR (AGrenade, AActor)
public:
	void BeginPlay ();
	void Tick ();
	bool	FloorBounceMissile( secplane_t &plane );

	void	PreExplode( );
};

class AArchvile : public AActor
{
	DECLARE_ACTOR (AArchvile, AActor)
};

class ALostSoul : public AActor
{
	DECLARE_ACTOR (ALostSoul, AActor)
};

class APlasmaBall : public AActor
{
	DECLARE_ACTOR (APlasmaBall, AActor)
};

class ABFGBall : public AActor
{
	DECLARE_ACTOR (ABFGBall, AActor)
};

class AScriptedMarine : public AActor
{
	DECLARE_ACTOR (AScriptedMarine, AActor)
public:
	enum EMarineWeapon
	{
		WEAPON_Dummy,
		WEAPON_Fist,
		WEAPON_BerserkFist,
		WEAPON_Chainsaw,
		WEAPON_Pistol,
		WEAPON_Shotgun,
		WEAPON_SuperShotgun,
		WEAPON_Chaingun,
		WEAPON_RocketLauncher,
		WEAPON_PlasmaRifle,
		WEAPON_Railgun,
		WEAPON_BFG
	};

	void Activate (AActor *activator);
	void Deactivate (AActor *activator);
	void BeginPlay ();
	void Tick ();
	void SetWeapon (EMarineWeapon);
	void SetSprite (const PClass *source);
	void Serialize (FArchive &arc);

protected:
	int SpriteOverride;
};

class ADoomPlayer : public APlayerPawn
{
	DECLARE_ACTOR (ADoomPlayer, APlayerPawn)
public:
	void GiveDefaultInventory ();
};

// [BC]
class AFlag : public AInventory
{
	DECLARE_STATELESS_ACTOR( AFlag, AInventory )
public:
	virtual bool ShouldRespawn( );
	virtual bool TryPickup( AActor *pToucher );
	virtual bool HandlePickup( AInventory *pItem );
	virtual LONG AllowFlagPickup( AActor *pToucher );
	virtual void AnnounceFlagPickup( AActor *pToucher );
	virtual void DisplayFlagTaken( AActor *pToucher );
	virtual void MarkFlagTaken( bool bTaken );
	virtual void ResetReturnTicks( void );
	virtual void ReturnFlag( AActor *pReturner );
	virtual void AnnounceFlagReturn( void );
	virtual void DisplayFlagReturn( void );
};

#endif //__A_DOOMGLOBAL_H__
