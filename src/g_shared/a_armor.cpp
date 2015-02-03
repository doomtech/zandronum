#include <assert.h>

#include "info.h"
#include "gi.h"
#include "r_data.h"
#include "a_pickups.h"
#include "templates.h"
#include "g_level.h"
// [BB] New #includes.
#include "doomstat.h"
#include "sv_commands.h"


IMPLEMENT_CLASS (AArmor)
IMPLEMENT_CLASS (ABasicArmor)
IMPLEMENT_CLASS (ABasicArmorPickup)
IMPLEMENT_CLASS (ABasicArmorBonus)
IMPLEMENT_CLASS (AHexenArmor)

//===========================================================================
//
// ABasicArmor :: Serialize
//
//===========================================================================

void ABasicArmor::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << SavePercent << BonusCount << MaxAbsorb << MaxFullAbsorb << AbsorbCount << ArmorType;
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
	AbsorbCount = 0;
	if (!Icon.isValid())
	{
		const char *icon = gameinfo.ArmorIcon1;

		if (SavePercent >= gameinfo.Armor2Percent && gameinfo.ArmorIcon2[0] != 0)
			icon = gameinfo.ArmorIcon2;

		if (icon[0] != 0)
			Icon = TexMan.CheckForTexture (icon, FTexture::TEX_Any);

		// [BB] If the icon is now valid, let the clients know about it. This is necessary because the
		// clients don't necessarily know the correct SavePercent value.
		if ( ( NETWORK_GetState( ) == NETSTATE_SERVER ) && Icon.isValid() && Owner && Owner->player )
			SERVERCOMMANDS_SetPlayerArmor( static_cast<ULONG>( Owner->player - players ) );
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
	copy->BonusCount = BonusCount;
	copy->ArmorType = ArmorType;
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
	if (item->IsKindOf(RUNTIME_CLASS(ABasicArmorBonus)) && !(item->ItemFlags & IF_IGNORESKILL))
	{
		ABasicArmorBonus *armor = static_cast<ABasicArmorBonus*>(item);

		armor->SaveAmount = FixedMul(armor->SaveAmount, G_SkillProperty(SKILLP_ArmorFactor));
	}
	else if (item->IsKindOf(RUNTIME_CLASS(ABasicArmorPickup)) && !(item->ItemFlags & IF_IGNORESKILL))
	{
		ABasicArmorPickup *armor = static_cast<ABasicArmorPickup*>(item);

		armor->SaveAmount = FixedMul(armor->SaveAmount, G_SkillProperty(SKILLP_ArmorFactor));
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
	int saved;

	if (damageType != NAME_Drowning)
	{
		int full = MAX(0, MaxFullAbsorb - AbsorbCount);
		if (damage < full)
		{
			saved = damage;
		}
		else
		{
			saved = full + FixedMul (damage - full, SavePercent);
			if (MaxAbsorb > 0 && saved + AbsorbCount > MaxAbsorb) 
			{
				saved = MAX(0,  MaxAbsorb - AbsorbCount);
			}
		}

		if (Amount < saved)
		{
			saved = Amount;
		}
		newdamage -= saved;
		Amount -= saved;
		AbsorbCount += saved;
		if (Amount == 0)
		{
			// The armor has become useless
			SavePercent = 0;
			ArmorType = NAME_None; // Not NAME_BasicArmor.
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
		damage = newdamage;
	}

	// Once the armor has absorbed its part of the damage, then apply its damage factor, if any, to the player
	if ((damage > 0) && (ArmorType != NAME_None)) // BasicArmor is not going to have any damage factor, so skip it.
	{
		// This code is taken and adapted from APowerProtection::ModifyDamage().
		// The differences include not checking for the NAME_None key (doesn't seem appropriate here), 
		// not using a default value, and of course the way the damage factor info is obtained.
		const fixed_t *pdf = NULL;
		DmgFactors *df = PClass::FindClass(ArmorType)->ActorInfo->DamageFactors;
		if (df != NULL && df->CountUsed() != 0)
		{
			pdf = df->CheckKey(damageType);
			if (pdf != NULL)
			{
				damage = newdamage = FixedMul(damage, *pdf);
			}
		}
	}

	// Once the armor has absorbed its part of the damage, then apply its damage factor, if any, to the player
	if ((damage > 0) && (ArmorType != NAME_None)) // BasicArmor is not going to have any damage factor, so skip it.
	{
		// This code is taken and adapted from APowerProtection::ModifyDamage().
		// The differences include not checking for the NAME_None key (doesn't seem appropriate here), 
		// not using a default value, and of course the way the damage factor info is obtained.
		const fixed_t *pdf = NULL;
		DmgFactors *df = PClass::FindClass(ArmorType)->ActorInfo->DamageFactors;
		if (df != NULL && df->CountUsed() != 0)
		{
			pdf = df->CheckKey(damageType);
			if (pdf != NULL)
			{
				damage = newdamage = FixedMul(damage, *pdf);
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
	arc << SavePercent << SaveAmount << MaxAbsorb << MaxFullAbsorb;
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

	if (!(ItemFlags & IF_IGNORESKILL))
	{
		SaveAmount = FixedMul(SaveAmount, G_SkillProperty(SKILLP_ArmorFactor));
	}

	copy->SavePercent = SavePercent;
	copy->SaveAmount = SaveAmount;
	copy->MaxAbsorb = MaxAbsorb;
	copy->MaxFullAbsorb = MaxFullAbsorb;

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
		if ( Owner->player->cheats & CF_PROSPERITY )
			lMaxAmount = ( deh.BlueAC * 100 ) + 50;
		else
			lMaxAmount += armor->BonusCount;
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
	armor->MaxAbsorb = MaxAbsorb;
	armor->MaxFullAbsorb = MaxFullAbsorb;
	armor->ArmorType = this->GetClass()->TypeName;
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
	arc << SavePercent << SaveAmount << MaxSaveAmount << BonusCount << BonusMax
		 << MaxAbsorb << MaxFullAbsorb;
}

//===========================================================================
//
// ABasicArmorBonus :: CreateCopy
//
//===========================================================================

AInventory *ABasicArmorBonus::CreateCopy (AActor *other)
{
	ABasicArmorBonus *copy = static_cast<ABasicArmorBonus *> (Super::CreateCopy (other));

	if (!(ItemFlags & IF_IGNORESKILL))
	{
		SaveAmount = FixedMul(SaveAmount, G_SkillProperty(SKILLP_ArmorFactor));
	}

	copy->SavePercent = SavePercent;
	copy->SaveAmount = SaveAmount;
	copy->MaxSaveAmount = MaxSaveAmount;
	copy->BonusCount = BonusCount;
	copy->BonusMax = BonusMax;
	copy->MaxAbsorb = MaxAbsorb;
	copy->MaxFullAbsorb = MaxFullAbsorb;

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
	bool result = false;

	if (armor == NULL)
	{
		armor = Spawn<ABasicArmor> (0,0,0, NO_REPLACE);
		armor->BecomeItem ();
		armor->Amount = 0;
		armor->MaxAmount = MaxSaveAmount;
		Owner->AddInventory (armor);
	}

	if (BonusCount > 0 && armor->BonusCount < BonusMax)
	{
		armor->BonusCount = MIN (armor->BonusCount + BonusCount, BonusMax);
		result = true;
	}

	int saveAmount = MIN (SaveAmount, MaxSaveAmount);

	if (saveAmount <= 0)
	{ // If it can't give you anything, it's as good as used.
		return BonusCount > 0 ? result : true;
	}

	// [BB] Handle the prosperity rune.
	int maxAmountPlusBonus;
	if ( Owner->player && Owner->player->cheats & CF_PROSPERITY )
		maxAmountPlusBonus = ( deh.BlueAC * 100 ) + 50;
	else
		maxAmountPlusBonus = MaxSaveAmount + armor->BonusCount;

	// If you already have more armor than this item can give you, you can't
	// use it.
	if (armor->Amount >= maxAmountPlusBonus)
	{
		return result;
	}

	if (armor->Amount <= 0)
	{ // Should never be less than 0, but might as well check anyway
		armor->Amount = 0;
		armor->Icon = Icon;
		armor->SavePercent = SavePercent;
		armor->MaxAbsorb = MaxAbsorb;
		armor->ArmorType = this->GetClass()->TypeName;
		armor->MaxFullAbsorb = MaxFullAbsorb;
	}

	armor->Amount = MIN(armor->Amount + saveAmount, maxAmountPlusBonus);
	armor->MaxAmount = MAX (armor->MaxAmount, MaxSaveAmount);
	return true;
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
	bool bAbsorbed = false; // [Dusk]

	if (damageType != NAME_Drowning)
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

					// [Dusk] mark down that we need to update the armor
					// values back to the client afterwards
					bAbsorbed = true;
				}
			}
			int saved = Scale (damage, savedPercent, 100*FRACUNIT);
			if (saved > savedPercent >> (FRACBITS-1))
			{	
				saved = savedPercent >> (FRACBITS-1);
			}
			newdamage -= saved;
			damage = newdamage;
		}
	}
	if (Inventory != NULL)
	{
		Inventory->AbsorbDamage (damage, damageType, newdamage);
	}

	if ( bAbsorbed && ( NETWORK_GetState( ) == NETSTATE_SERVER ) ) // [Dusk]
		SERVERCOMMANDS_SetHexenArmorSlots( Owner->player - players, this );
}

