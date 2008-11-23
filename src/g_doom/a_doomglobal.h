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

#endif //__A_DOOMGLOBAL_H__
