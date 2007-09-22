#include <assert.h>

#include "info.h"
#include "gi.h"
#include "r_data.h"
#include "a_pickups.h"
#include "templates.h"


IMPLEMENT_STATELESS_ACTOR (AArmor, Any, -1, 0)
	PROP_Inventory_PickupSound ("misc/armor_pkup")
END_DEFAULTS

IMPLEMENT_STATELESS_ACTOR (ABasicArmor, Any, -1, 0)
END_DEFAULTS

IMPLEMENT_STATELESS_ACTOR (ABasicArmorPickup, Any, -1, 0)
	PROP_Inventory_MaxAmount (0)
	PROP_Inventory_FlagsSet (IF_AUTOACTIVATE)
END_DEFAULTS

IMPLEMENT_STATELESS_ACTOR (ABasicArmorBonus, Any, -1, 0)
	PROP_Inventory_MaxAmount (0)
	PROP_Inventory_FlagsSet (IF_AUTOACTIVATE|IF_ALWAYSPICKUP)
	PROP_BasicArmorBonus_SavePercent (FRACUNIT/3)
END_DEFAULTS

// [BC] Implement the basic max. armor bonus.
IMPLEMENT_STATELESS_ACTOR( ABasicMaxArmorBonus, Any, -1, 0 )
	PROP_Inventory_MaxAmount( 0 )
	PROP_Inventory_FlagsSet( IF_AUTOACTIVATE|IF_ALWAYSPICKUP )
	PROP_BasicArmorBonus_SavePercent( FRACUNIT/3 )
END_DEFAULTS

IMPLEMENT_STATELESS_ACTOR (AHexenArmor, Any, -1, 0)
	PROP_Inventory_FlagsSet (IF_UNDROPPABLE)
END_DEFAULTS


//===========================================================================
//
// ABasicArmor :: Serialize
//
//===========================================================================

void ABasicArmor::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << SavePercent;
}

//===========================================================================
//
// ABasicArmor :: Tick
//
// If BasicArmor is given to the player by means other than a
// BasicArmorPickup, then it may not have an icon set. Fix that here.
//
//===========================================================================

void ABasicArmor::Tick ()
{
	Super::Tick ();
	if (Icon == 0)
	{
		switch (gameinfo.gametype)
		{
		case GAME_Doom:
			Icon = TexMan.CheckForTexture (SavePercent == FRACUNIT/3 ? "ARM1A0" : "ARM2A0", FTexture::TEX_Any);
			break;

		case GAME_Heretic:
			Icon = TexMan.CheckForTexture (SavePercent == FRACUNIT/2 ? "SHLDA0" : "SHD2A0", FTexture::TEX_Any);
			break;

		case GAME_Strife:
			Icon = TexMan.CheckForTexture (SavePercent == FRACUNIT/3 ? "I_ARM2" : "I_ARM1", FTexture::TEX_Any);
			break;
		
		default:
			break;
		}
	}
}

//===========================================================================
//
// ABasicArmor :: CreateCopy
//
//===========================================================================

AInventory *ABasicArmor::CreateCopy (AActor *other)
{
	// BasicArmor that is in use is stored in the inventory as BasicArmor.
	// BasicArmor that is in reserve is not.
	ABasicArmor *copy = Spawn<ABasicArmor> (0, 0, 0, NO_REPLACE);
	copy->SavePercent = SavePercent != 0 ? SavePercent : FRACUNIT/3;
	copy->Amount = Amount;
	copy->MaxAmount = MaxAmount;
	copy->Icon = Icon;
	GoAwayAndDie ();
	return copy;
}

//===========================================================================
//
// ABasicArmor :: HandlePickup
//
//===========================================================================

bool ABasicArmor::HandlePickup (AInventory *item)
{
	if (item->GetClass() == RUNTIME_CLASS(ABasicArmor))
	{
		// You shouldn't be picking up BasicArmor anyway.
		return true;
	}
	if (Inventory != NULL)
	{
		return Inventory->HandlePickup (item);
	}
	return false;
}

//===========================================================================
//
// ABasicArmor :: AbsorbDamage
//
//===========================================================================

void ABasicArmor::AbsorbDamage (int damage, FName damageType, int &newdamage)
{
	if (damageType != NAME_Water)
	{
		int saved = FixedMul (damage, SavePercent);
		if (Amount < saved)
		{
			saved = Amount;
		}
		newdamage -= saved;
		Amount -= saved;
		if (Amount == 0)
		{
			// The armor has become useless
			SavePercent = 0;

			// [BC] Take away the power of fire resistance.
			if ( Owner->player )
				Owner->player->Powers &= ~PW_FIRERESISTANT;

			// Now see if the player has some more armor in their inventory
			// and use it if so. As in Strife, the best armor is used up first.
			ABasicArmorPickup *best = NULL;
			AInventory *probe = Owner->Inventory;
			while (probe != NULL)
			{
				if (probe->IsKindOf (RUNTIME_CLASS(ABasicArmorPickup)))
				{
					ABasicArmorPickup *inInv = static_cast<ABasicArmorPickup*>(probe);
					if (best == NULL || best->SavePercent < inInv->SavePercent)
					{
						best = inInv;
					}
				}
				probe = probe->Inventory;
			}
			if (best != NULL)
			{
				Owner->UseInventory (best);
			}
		}
	}
	if (Inventory != NULL)
	{
		Inventory->AbsorbDamage (damage, damageType, newdamage);
	}
}

//===========================================================================
//
// ABasicArmorPickup :: Serialize
//
//===========================================================================

void ABasicArmorPickup::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << SavePercent << SaveAmount;
	arc << DropTime;
}

//===========================================================================
//
// ABasicArmorPickup :: CreateCopy
//
//===========================================================================

AInventory *ABasicArmorPickup::CreateCopy (AActor *other)
{
	ABasicArmorPickup *copy = static_cast<ABasicArmorPickup *> (Super::CreateCopy (other));
	copy->SavePercent = SavePercent;
	copy->SaveAmount = SaveAmount;
	return copy;
}

//===========================================================================
//
// ABasicArmorPickup :: Use
//
// Either gives you new armor or replaces the armor you already have (if
// the SaveAmount is greater than the amount of armor you own). When the
// item is auto-activated, it will only be activated if its max amount is 0
// or if you have no armor active already.
//
//===========================================================================

bool ABasicArmorPickup::Use (bool pickup)
{
	ABasicArmor *armor = Owner->FindInventory<ABasicArmor> ();
	LONG	lMaxAmount;

	if (armor == NULL)
	{
		armor = Spawn<ABasicArmor> (0,0,0, NO_REPLACE);
		armor->BecomeItem ();
		armor->SavePercent = SavePercent;
		armor->Amount = armor->MaxAmount = SaveAmount;
		armor->Icon = Icon;
		Owner->AddInventory (armor);
		return true;
	}

	// [BC] Handle max. armor bonuses, and the prosperity rune.
	lMaxAmount = SaveAmount;
	if ( Owner->player )
	{
		if ( Owner->player->Powers & PW_PROSPERITY )
			lMaxAmount = ( deh.BlueAC * 100 ) + 50;
		else
			lMaxAmount += Owner->player->lMaxArmorBonus;
	}

	// [BC] Changed ">=" to ">" so we can do a check below to potentially pick up armor
	// that offers the same amount of armor, but a better SavePercent.

	// If you already have more armor than this item gives you, you can't
	// use it.
	if (armor->Amount > lMaxAmount)
	{
		return false;
	}

	// [BC] If we have the same amount of the armor we're trying to use, but our armor offers
	// better protection, don't pick it up.
	if (( armor->Amount == lMaxAmount ) && ( armor->SavePercent >= SavePercent ))
		return ( false );

	// Don't use it if you're picking it up and already have some.
	if (pickup && armor->Amount > 0 && MaxAmount > 0)
	{
		return false;
	}
	armor->SavePercent = SavePercent;
	armor->MaxAmount = SaveAmount;
	armor->Amount += SaveAmount;
	if ( armor->Amount > lMaxAmount )
		armor->Amount = lMaxAmount;
	armor->Icon = Icon;

	// [BC] Take away the power of fire resistance since we're changing
	// the armor type.
	if ( Owner->player )
		Owner->player->Powers &= ~PW_FIRERESISTANT;

	return true;
}

//===========================================================================
//
// ABasicArmorBonus :: Serialize
//
//===========================================================================

void ABasicArmorBonus::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << SavePercent << SaveAmount << MaxSaveAmount;
}

//===========================================================================
//
// ABasicArmorBonus :: CreateCopy
//
//===========================================================================

AInventory *ABasicArmorBonus::CreateCopy (AActor *other)
{
	ABasicArmorBonus *copy = static_cast<ABasicArmorBonus *> (Super::CreateCopy (other));
	copy->SavePercent = SavePercent;
	copy->SaveAmount = SaveAmount;
	copy->MaxSaveAmount = MaxSaveAmount;
	return copy;
}

//===========================================================================
//
// ABasicArmorBonus :: Use
//
// Tries to add to the amount of BasicArmor a player has.
//
//===========================================================================

bool ABasicArmorBonus::Use (bool pickup)
{
	ABasicArmor *armor = Owner->FindInventory<ABasicArmor> ();
	// [BC] The true max. save amount is the armor's max. save amount, plus the player's
	// max. armor bonus.
	LONG	lMaxSaveAmount;
	int saveAmount;

	lMaxSaveAmount = MaxSaveAmount;
	if ( Owner->player )
	{
		// [BC] Apply the prosperity power.
		if ( Owner->player->Powers & PW_PROSPERITY )
			lMaxSaveAmount = ( deh.BlueAC * 100 ) + 50;
		else
			lMaxSaveAmount += Owner->player->lMaxArmorBonus;
	}
 
	saveAmount = MIN (SaveAmount, (int)lMaxSaveAmount);
	if (saveAmount <= 0)
	{ // If it can't give you anything, it's as good as used.
		return true;
	}
	if (armor == NULL)
	{
		armor = Spawn<ABasicArmor> (0,0,0, NO_REPLACE);
		armor->BecomeItem ();
		armor->SavePercent = SavePercent;
		armor->Amount = saveAmount;
		armor->MaxAmount = lMaxSaveAmount;
		armor->Icon = Icon;
		Owner->AddInventory (armor);
		return true;
	}
	// If you already have more armor than this item can give you, you can't
	// use it.
	if (armor->Amount >= lMaxSaveAmount)
	{
		return false;
	}
	if (armor->Amount <= 0)
	{ // Should never be less than 0, but might as well check anyway
		armor->Amount = 0;
		armor->Icon = Icon;
		armor->SavePercent = SavePercent;
	}
	armor->Amount += saveAmount;
	armor->MaxAmount = MAX (armor->MaxAmount, (int)lMaxSaveAmount);
	if ( armor->Amount > armor->MaxAmount )
		armor->Amount = armor->MaxAmount;
	return true;
}

//===========================================================================
//
// [BC] ABasicMaxArmorBonus :: Use
//
// Tries to add to the amount of BasicArmor a player has, and increased the
// player's max. armor bonus.
//
//===========================================================================

bool ABasicMaxArmorBonus::Use( bool bPickup )
{
	ABasicArmor		*pArmor;
	LONG			lSaveAmount;
	LONG			lMaxSaveAmount;
	bool			bGaveMaxBonus;

	pArmor = Owner->FindInventory<ABasicArmor>( );

	// Always give the bonus if the player's max. armor bonus isn't maxed out.
	bGaveMaxBonus = Owner->player->lMaxArmorBonus < this->lMaxBonusMax;

	lMaxSaveAmount = MaxSaveAmount;
	if ( Owner->player )
	{
		// Give him the max. armor bonus.
		Owner->player->lMaxArmorBonus += this->lMaxBonus;
		if ( Owner->player->lMaxArmorBonus > this->lMaxBonusMax )
			Owner->player->lMaxArmorBonus = this->lMaxBonusMax;

		// Apply the prosperity power.
		if ( Owner->player->Powers & PW_PROSPERITY )
			lMaxSaveAmount = ( deh.BlueAC * 100 ) + 50;
		else
			lMaxSaveAmount += Owner->player->lMaxArmorBonus;
	}

	lSaveAmount = MIN( SaveAmount, (int)lMaxSaveAmount );

	// If this armor bonus doesn't give us any armor, and it doesn't offer any
	// max. armor bonus, pick it up.
	if (( lSaveAmount <= 0 ) && ( this->lMaxBonus <= 0 ))
		return ( false );

	// If the owner doesn't possess any armor, then give him armor, and give it this
	// bonus's properties (oh yeah, and give him the max. armor bonus).
	if ( pArmor == NULL )
	{
		pArmor = Spawn<ABasicArmor>( 0, 0, 0, NO_REPLACE );
		pArmor->BecomeItem( );
		pArmor->SavePercent = SavePercent;
		pArmor->Amount = lSaveAmount;
		pArmor->MaxAmount = MaxSaveAmount;
		pArmor->Icon = Icon;
		Owner->AddInventory( pArmor );

		// Return true since the object was used successfully.
		return ( true );
	}

	// If you already have more armor than this item can give you, and it
	// doesn't offer any bonus to the player's max. armor bonus, you can't
	// use it.
	if ( pArmor->Amount >= lMaxSaveAmount )
		return ( bGaveMaxBonus );

	// Should never be less than 0, but might as well check anyway
	if ( pArmor->Amount <= 0 )
	{
		pArmor->Amount = 0;
		pArmor->Icon = Icon;
		pArmor->SavePercent = SavePercent;
	}

	pArmor->Amount += lSaveAmount;
	pArmor->MaxAmount = MAX( pArmor->MaxAmount, (int)lMaxSaveAmount );
	if ( pArmor->Amount > pArmor->MaxAmount )
		pArmor->Amount = pArmor->MaxAmount;

	// Return true since the object was used successfully.
	return ( true );
}

//===========================================================================
//
// AHexenArmor :: Serialize
//
//===========================================================================

void AHexenArmor::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << Slots[0] << Slots[1] << Slots[2] << Slots[3]
		<< Slots[4]
		<< SlotsIncrement[0] << SlotsIncrement[1] << SlotsIncrement[2]
		<< SlotsIncrement[3];
}

//===========================================================================
//
// AHexenArmor :: CreateCopy
//
//===========================================================================

AInventory *AHexenArmor::CreateCopy (AActor *other)
{
	// Like BasicArmor, HexenArmor is used in the inventory but not the map.
	// health is the slot this armor occupies.
	// Amount is the quantity to give (0 = normal max).
	AHexenArmor *copy = Spawn<AHexenArmor> (0, 0, 0, NO_REPLACE);
	copy->AddArmorToSlot (other, health, Amount);
	GoAwayAndDie ();
	return copy;
}

//===========================================================================
//
// AHexenArmor :: CreateTossable
//
// Since this isn't really a single item, you can't drop it. Ever.
//
//===========================================================================

AInventory *AHexenArmor::CreateTossable ()
{
	return NULL;
}

//===========================================================================
//
// AHexenArmor :: HandlePickup
//
//===========================================================================

bool AHexenArmor::HandlePickup (AInventory *item)
{
	if (item->IsKindOf (RUNTIME_CLASS(AHexenArmor)))
	{
		if (AddArmorToSlot (Owner, item->health, item->Amount))
		{
			item->ItemFlags |= IF_PICKUPGOOD;
		}
		return true;
	}
	else if (Inventory != NULL)
	{
		return Inventory->HandlePickup (item);
	}
	return false;
}

//===========================================================================
//
// AHexenArmor :: AddArmorToSlot
//
//===========================================================================

bool AHexenArmor::AddArmorToSlot (AActor *actor, int slot, int amount)
{
	APlayerPawn *ppawn;
	int hits;

	if (actor->player != NULL)
	{
		ppawn = static_cast<APlayerPawn *>(actor);
	}
	else
	{
		ppawn = NULL;
	}

	if (slot < 0 || slot > 3)
	{
		return false;
	}
	if (amount <= 0)
	{
		hits = SlotsIncrement[slot];
		if (Slots[slot] < hits)
		{
			Slots[slot] = hits;
			return true;
		}
	}
	else
	{
		hits = amount * 5 * FRACUNIT;
		fixed_t total = Slots[0]+Slots[1]+Slots[2]+Slots[3]+Slots[4];
		fixed_t max = SlotsIncrement[0]+SlotsIncrement[1]+SlotsIncrement[2]+SlotsIncrement[3]+Slots[4]+4*5*FRACUNIT;
		if (total < max)
		{
			Slots[slot] += hits;
			return true;
		}
	}
	return false;
}

//===========================================================================
//
// AHexenArmor :: AbsorbDamage
//
//===========================================================================

void AHexenArmor::AbsorbDamage (int damage, FName damageType, int &newdamage)
{
	if (damageType != NAME_Water)
	{
		fixed_t savedPercent = Slots[0] + Slots[1] + Slots[2] + Slots[3] + Slots[4];

		if (savedPercent)
		{ // armor absorbed some damage
			if (savedPercent > 100*FRACUNIT)
			{
				savedPercent = 100*FRACUNIT;
			}
			for (int i = 0; i < 4; i++)
			{
				if (Slots[i])
				{
					// 300 damage always wipes out the armor unless some was added
					// with the dragon skin bracers.
					if (damage < 10000)
					{
						Slots[i] -= Scale (damage, SlotsIncrement[i], 300);
						if (Slots[i] < 2*FRACUNIT)
						{
							Slots[i] = 0;
						}
					}
					else
					{
						Slots[i] = 0;
					}
				}
			}
			int saved = Scale (damage, savedPercent, 100*FRACUNIT);
			if (saved > savedPercent >> (FRACBITS-1))
			{	
				saved = savedPercent >> (FRACBITS-1);
			}
			newdamage -= saved;
		}
	}
	if (Inventory != NULL)
	{
		Inventory->AbsorbDamage (damage, damageType, newdamage);
	}
}

