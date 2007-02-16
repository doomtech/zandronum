#ifndef __A_HEXENGLOBAL_H__
#define __A_HEXENGLOBAL_H__

#include "d_player.h"

class ALightning : public AActor
{
	DECLARE_STATELESS_ACTOR (ALightning, AActor)
public:
	int SpecialMissileHit (AActor *victim);
};

class APoisonCloud : public AActor
{
	DECLARE_ACTOR (APoisonCloud, AActor)
public:
	void GetExplodeParms (int &damage, int &distance, bool &hurtSource);
	int DoSpecialDamage (AActor *target, int damage);
	void BeginPlay ();
};

class ABishop : public AActor
{
	DECLARE_ACTOR (ABishop, AActor)
public:
	void GetExplodeParms (int &damage, int &distance, bool &hurtSource);
};

class AHeresiarch : public AActor
{
	DECLARE_ACTOR (AHeresiarch, AActor)
public:
	const PClass *StopBall;

	void Serialize (FArchive &arc);
	void Die (AActor *source, AActor *inflictor);
};

class AHolySpirit : public AActor
{
	DECLARE_ACTOR (AHolySpirit, AActor)
public:
	bool Slam (AActor *thing);
	bool SpecialBlastHandling (AActor *source, fixed_t strength);
	bool IsOkayToAttack (AActor *link);
};

class AHammerPuff : public AActor
{
	DECLARE_ACTOR (AHammerPuff, AActor)
public:
	void BeginPlay ();
};

class ACentaur : public AActor
{
	DECLARE_ACTOR (ACentaur, AActor)
public:
	void Howl ();
};

class AFourthWeaponPiece : public AInventory
{
	DECLARE_STATELESS_ACTOR (AFourthWeaponPiece, AInventory)
	HAS_OBJECT_POINTERS
public:
	void Serialize (FArchive &arc);
	bool TryPickup (AActor *toucher);
	const char *PickupMessage ();
	bool ShouldStay ();
	void PlayPickupSound (AActor *toucher);
protected:
	virtual bool MatchPlayerClass (AActor *toucher);
	const PClass *FourthWeaponClass;
	int PieceValue;
	AInventory *TempFourthWeapon;
	bool PrivateShouldStay ();
};

class AFighterPlayer : public APlayerPawn
{
	DECLARE_ACTOR (AFighterPlayer, APlayerPawn)
public:
	void GiveDefaultInventory ();
	bool DoHealingRadius (APlayerPawn *other);
};

class AFighterWeapon : public AWeapon
{
	DECLARE_STATELESS_ACTOR (AFighterWeapon, AWeapon);
public:
	bool TryPickup (AActor *toucher);
};

class AClericPlayer : public APlayerPawn
{
	DECLARE_ACTOR (AClericPlayer, APlayerPawn)
public:
	void GiveDefaultInventory ();
	void SpecialInvulnerabilityHandling (EInvulState state, fixed_t * pAlpha);
};

class AClericWeapon : public AWeapon
{
	DECLARE_STATELESS_ACTOR (AClericWeapon, AWeapon);
public:
	bool TryPickup (AActor *toucher);
};

class AMagePlayer : public APlayerPawn
{
	DECLARE_ACTOR (AMagePlayer, APlayerPawn)
public:
	void GiveDefaultInventory ();
	bool DoHealingRadius (APlayerPawn *other);
	void SpecialInvulnerabilityHandling (EInvulState state, fixed_t * pAlpha);
};

class AMageWeapon : public AWeapon
{
	DECLARE_STATELESS_ACTOR (AMageWeapon, AWeapon);
public:
	bool TryPickup (AActor *toucher);
};

class ASwitchableDecoration : public AActor
{
	DECLARE_STATELESS_ACTOR (ASwitchableDecoration, AActor)
public:
	void Activate (AActor *activator);
	void Deactivate (AActor *activator);
};

#endif //__A_HEXENGLOBAL_H__
