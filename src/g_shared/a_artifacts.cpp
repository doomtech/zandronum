#include "info.h"
#include "a_pickups.h"
#include "d_player.h"
#include "p_local.h"
#include "c_dispatch.h"
#include "gi.h"
#include "s_sound.h"
#include "p_local.h"
#include "p_spec.h"
#include "p_lnspec.h"
#include "p_enemy.h"
#include "p_effect.h"
#include "a_artifacts.h"
#include "sbar.h"
#include "d_player.h"
#include "m_random.h"
#include "v_video.h"
#include "templates.h"
#include "network.h"
#include "g_game.h"
#include "deathmatch.h"
#include "possession.h"

static FRandom pr_torch ("Torch");

#define	INVULNTICS (30*TICRATE)
#define	INVISTICS (60*TICRATE)
#define	INFRATICS (120*TICRATE)
#define	IRONTICS (60*TICRATE)
#define WPNLEV2TICS (40*TICRATE)
#define FLIGHTTICS (60*TICRATE)
#define SPEEDTICS (45*TICRATE)
#define MAULATORTICS (25*TICRATE)

// [BC] New Skulltag power duration defines.
#define	TIMEFREEZE_TICS			( 12 * TICRATE )
#define	QUADDAMAGE_TICS			( 25 * TICRATE )
#define	QUARTERDAMAGE_TICS		( 25 * TICRATE )
#define	TRANSLUCENCY_TICS		( 45 * TICRATE )

EXTERN_CVAR (Bool, r_drawfuzz);

IMPLEMENT_ABSTRACT_ACTOR (APowerup)
// [BC]
IMPLEMENT_ABSTRACT_ACTOR (ARune)

// Powerup-Giver -------------------------------------------------------------

//===========================================================================
//
// APowerupGiver :: Use
//
//===========================================================================

bool APowerupGiver::Use (bool pickup)
{
	APowerup *power = static_cast<APowerup *> (Spawn (PowerupType, 0, 0, 0, NO_REPLACE));

	if (EffectTics != 0)
	{
		power->EffectTics = EffectTics;
	}
	if (BlendColor != 0)
	{
		power->BlendColor = BlendColor;
	}
	if (mode != NAME_None)
	{
		power->mode = mode;
	}

	power->ItemFlags |= ItemFlags & IF_ALWAYSPICKUP;
	if (power->TryPickup (Owner))
	{
		return true;
	}
	power->GoAwayAndDie ();
	return false;
}

//===========================================================================
//
// APowerupGiver :: Serialize
//
//===========================================================================

void APowerupGiver::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << PowerupType;
	arc << EffectTics << BlendColor << mode;
}

// Powerup -------------------------------------------------------------------

//===========================================================================
//
// APowerup :: Tick
//
//===========================================================================

void APowerup::Tick ()
{
	// Powerups cannot exist outside an inventory
	if (Owner == NULL)
	{
		Destroy ();
	}
	if (EffectTics > 0 && --EffectTics == 0)
	{
		Destroy ();
	}
}

//===========================================================================
//
// APowerup :: Serialize
//
//===========================================================================

void APowerup::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << EffectTics << BlendColor << mode;
}

//===========================================================================
//
// APowerup :: GetBlend
//
//===========================================================================

PalEntry APowerup::GetBlend ()
{
	if (EffectTics <= BLINKTHRESHOLD && !(EffectTics & 8))
		return 0;

	if (BlendColor == INVERSECOLOR ||
		BlendColor == GOLDCOLOR ||
		// [BC] HAX!
		BlendColor == REDCOLOR ||
		BlendColor == GREENCOLOR) 
		return 0;

	return BlendColor;
}

//===========================================================================
//
// APowerup :: InitEffect
//
//===========================================================================

void APowerup::InitEffect ()
{
}

//===========================================================================
//
// APowerup :: DoEffect
//
//===========================================================================

void APowerup::DoEffect ()
{
	if (Owner == NULL)
	{
		return;
	}

	if (EffectTics > 0)
	{
		int oldcolormap = Owner->player->fixedcolormap;
		if (EffectTics > BLINKTHRESHOLD || (EffectTics & 8))
		{
			// [BC] Apply the colormap to the player's body, also.
			if (BlendColor == INVERSECOLOR)
			{
				Owner->player->fixedcolormap = INVERSECOLORMAP;
				Owner->lFixedColormap = INVERSECOLORMAP;
			}
			else if (BlendColor == GOLDCOLOR)
			{
				Owner->player->fixedcolormap = GOLDCOLORMAP;
				Owner->lFixedColormap = GOLDCOLORMAP;
			}
			else if (BlendColor == REDCOLOR)
			{
				Owner->player->fixedcolormap = REDCOLORMAP;
				Owner->lFixedColormap = REDCOLORMAP;
			}
			else if (BlendColor == GREENCOLOR)
			{
				Owner->player->fixedcolormap = GREENCOLORMAP;
				Owner->lFixedColormap = GREENCOLORMAP;
			}
		}
		else if ((BlendColor == INVERSECOLOR && Owner->player->fixedcolormap == INVERSECOLORMAP) || 
				 (BlendColor == GOLDCOLOR && Owner->player->fixedcolormap == GOLDCOLORMAP) ||
				 (BlendColor == REDCOLOR && Owner->player->fixedcolormap == REDCOLORMAP) ||
				 (BlendColor == GREENCOLOR && Owner->player->fixedcolormap == GREENCOLORMAP))
		{
			Owner->player->fixedcolormap = 0;
			Owner->lFixedColormap = 0;
		}
	}
}

//===========================================================================
//
// APowerup :: EndEffect
//
//===========================================================================

void APowerup::EndEffect ()
{
}

//===========================================================================
//
// APowerup :: Destroy
//
//===========================================================================

void APowerup::Destroy ()
{
	EndEffect ();
	Super::Destroy ();
}

//===========================================================================
//
// APowerup :: DrawPowerup
//
//===========================================================================

bool APowerup::DrawPowerup (int x, int y)
{
	if (Icon <= 0)
	{
		return false;
	}
	if (EffectTics > BLINKTHRESHOLD || !(EffectTics & 16))
	{
		FTexture *pic = TexMan(Icon);
		screen->DrawTexture (pic, x, y,
			DTA_HUDRules, HUD_Normal,
//			DTA_TopOffset, pic->GetHeight()/2,
//			DTA_LeftOffset, pic->GetWidth()/2,
			TAG_DONE);
	}
	return true;
}

//===========================================================================
//
// APowerup :: HandlePickup
//
//===========================================================================

bool APowerup::HandlePickup (AInventory *item)
{
	if (item->GetClass() == GetClass())
	{
		APowerup *power = static_cast<APowerup*>(item);
		if (power->EffectTics == 0)
		{
			power->ItemFlags |= IF_PICKUPGOOD;
			return true;
		}
		// If it's not blinking yet, you can't replenish the power unless the
		// powerup is required to be picked up.
		if (EffectTics > BLINKTHRESHOLD && !(power->ItemFlags & IF_ALWAYSPICKUP))
		{
			return true;
		}
		// Only increase the EffectTics, not decrease it.
		// Color also gets transferred only when the new item has an effect.
		if (power->EffectTics > EffectTics)
		{
			EffectTics = power->EffectTics;
			BlendColor = power->BlendColor;
		}
		power->ItemFlags |= IF_PICKUPGOOD;
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
// APowerup :: CreateCopy
//
//===========================================================================

AInventory *APowerup::CreateCopy (AActor *other)
{
	EffectTics = abs (EffectTics);
	Owner = other;
	InitEffect ();
	Owner = NULL;
	return this;
}

//===========================================================================
//
// APowerup :: CreateTossable
//
// Powerups are never droppable, even without IF_UNDROPPABLE set.
//
//===========================================================================

AInventory *APowerup::CreateTossable ()
{
	return NULL;
}

//===========================================================================
//
// APowerup :: OwnerDied
//
// Powerups don't last beyond death.
//
//===========================================================================

void APowerup::OwnerDied ()
{
	Destroy ();
}

// Invulnerability Powerup ---------------------------------------------------

IMPLEMENT_STATELESS_ACTOR (APowerInvulnerable, Any, -1, 0)
	PROP_Powerup_EffectTics (INVULNTICS)
	PROP_Inventory_Icon ("SPSHLD0")
END_DEFAULTS

// Need to set the default for each game here
AT_GAME_SET(PowerInvulnerable)
{
	APowerInvulnerable * invul = GetDefault<APowerInvulnerable>();
	switch (gameinfo.gametype)
	{
	case GAME_Doom:
	case GAME_Strife:
		invul->BlendColor = INVERSECOLOR;
		break;

	case GAME_Heretic:
		invul->BlendColor = GOLDCOLOR;
		break;

	default:
		break;
	}
}
//===========================================================================
//
// APowerInvulnerable :: InitEffect
//
//===========================================================================

void APowerInvulnerable::InitEffect ()
{
	Owner->effects &= ~FX_RESPAWNINVUL;
	Owner->flags2 |= MF2_INVULNERABLE;
	if (mode == NAME_None)
	{
		mode = (ENamedName)RUNTIME_TYPE(Owner)->Meta.GetMetaInt(APMETA_InvulMode);
	}
	if (mode == NAME_Reflective) Owner->flags2 |= MF2_REFLECTIVE;
}

//===========================================================================
//
// APowerInvulnerable :: DoEffect
//
//===========================================================================

void APowerInvulnerable::DoEffect ()
{
	Super::DoEffect ();

	if (Owner == NULL)
	{
		return;
	}

	if (mode == NAME_Ghost)
	{
		if (!(Owner->flags & MF_SHADOW))
		{
			// Don't mess with the translucency settings if an
			// invisibility powerup is active.
			Owner->RenderStyle = STYLE_Translucent;
			if (!(level.time & 7) && Owner->alpha > 0 && Owner->alpha < OPAQUE)
			{
				if (Owner->alpha == HX_SHADOW)
				{
					Owner->alpha = HX_ALTSHADOW;
				}
				else
				{
					Owner->alpha = 0;
					Owner->flags2 |= MF2_NONSHOOTABLE;
				}
			}
			if (!(level.time & 31))
			{
				if (Owner->alpha == 0)
				{
					Owner->flags2 &= ~MF2_NONSHOOTABLE;
					Owner->alpha = HX_ALTSHADOW;
				}
				else
				{
					Owner->alpha = HX_SHADOW;
				}
			}
		}
		else
		{
			Owner->flags2 &= ~MF2_NONSHOOTABLE;
		}
	}
}

//===========================================================================
//
// APowerInvulnerable :: EndEffect
//
//===========================================================================

void APowerInvulnerable::EndEffect ()
{
	if (Owner == NULL)
	{
		return;
	}

	Owner->flags2 &= ~MF2_INVULNERABLE;
	Owner->effects &= ~FX_RESPAWNINVUL;
	if ( Owner->effects & FX_VISIBILITYFLICKER )
	{
		Owner->effects &= ~FX_VISIBILITYFLICKER;
		// [BC] If the owner is a spectating player, don't make him visible!
		if (( Owner->player == NULL ) || ( Owner->player->bSpectating == false ))
			Owner->RenderStyle = STYLE_Normal;
		else
			Owner->RenderStyle = STYLE_None;
	}
	if (mode == NAME_Ghost)
	{
		Owner->flags2 &= ~MF2_NONSHOOTABLE;
		if (!(Owner->flags & MF_SHADOW))
		{
			// Don't mess with the translucency settings if an
			// invisibility powerup is active.
			Owner->RenderStyle = STYLE_Normal;
			Owner->alpha = OPAQUE;
		}
	}
	else if (mode == NAME_Reflective)
	{
		Owner->flags2 &= ~MF2_REFLECTIVE;
	}

	if (Owner->player != NULL)
	{
		Owner->player->fixedcolormap = 0;
	}
}

//===========================================================================
//
// APowerInvulnerable :: AlterWeaponSprite
//
//===========================================================================

int APowerInvulnerable::AlterWeaponSprite (vissprite_t *vis)
{
	int changed = Inventory == NULL? false : Inventory->AlterWeaponSprite(vis);
	if (Owner != NULL)
	{
		if (mode == NAME_Ghost && !(Owner->flags & MF_SHADOW))
		{
			fixed_t wp_alpha = MIN<fixed_t>(FRACUNIT/4 + Owner->alpha*3/4, FRACUNIT);
			if (wp_alpha != FIXED_MAX) vis->alpha = wp_alpha;
		}
	}
	return changed;
}

// Strength (aka Berserk) Powerup --------------------------------------------

IMPLEMENT_STATELESS_ACTOR (APowerStrength, Any, -1, 0)
	PROP_Powerup_EffectTics (1)
	PROP_Powerup_Color (128, 255, 0, 0)
	PROP_Inventory_FlagsSet (IF_HUBPOWER)
END_DEFAULTS

//===========================================================================
//
// APowerStrength :: HandlePickup
//
//===========================================================================

bool APowerStrength::HandlePickup (AInventory *item)
{
	if (item->GetClass() == GetClass())
	{ // Setting EffectTics to 0 will force Powerup's HandlePickup()
	  // method to reset the tic count so you get the red flash again.
		EffectTics = 0;
	}
	return Super::HandlePickup (item);
}

//===========================================================================
//
// APowerStrength :: InitEffect
//
//===========================================================================

void APowerStrength::InitEffect ()
{
}

//===========================================================================
//
// APowerStrength :: DoEffect
//
//===========================================================================

void APowerStrength::Tick ()
{
	// Strength counts up to diminish the fade.
	EffectTics += 2;
	Super::Tick();
}

//===========================================================================
//
// APowerStrength :: GetBlend
//
//===========================================================================

PalEntry APowerStrength::GetBlend ()
{
	// slowly fade the berzerk out
	int cnt = 12 - (EffectTics >> 6);

	if (cnt > 0)
	{
		cnt = (cnt + 7) >> 3;
		return PalEntry (BlendColor.a*cnt*255/9,
			BlendColor.r, BlendColor.g, BlendColor.b);
	}
	return 0;
}

// Invisibility Powerup ------------------------------------------------------

IMPLEMENT_STATELESS_ACTOR (APowerInvisibility, Any, -1, 0)
	PROP_Powerup_EffectTics (INVISTICS)
END_DEFAULTS

//===========================================================================
//
// APowerInvisibility :: InitEffect
//
//===========================================================================

void APowerInvisibility::InitEffect ()
{
	Owner->flags |= MF_SHADOW;
	Owner->alpha = FRACUNIT/5;
	Owner->RenderStyle = STYLE_OptFuzzy;
}

void APowerInvisibility::DoEffect ()
{
	Super::DoEffect();
	// Due to potential interference with other PowerInvisibility items
	// the effect has to be refreshed each tic.
	InitEffect();
}

//===========================================================================
//
// APowerInvisibility :: EndEffect
//
//===========================================================================

void APowerInvisibility::EndEffect ()
{
	if (Owner != NULL)
	{
		Owner->flags &= ~MF_SHADOW;
		Owner->flags3 &= ~MF3_GHOST;
		// [BC] If the owner is a spectating player, don't make him visible!
		if (( Owner->player == NULL ) || ( Owner->player->bSpectating == false ))
			Owner->RenderStyle = STYLE_Normal;
		else
			Owner->RenderStyle = STYLE_None;
		Owner->alpha = OPAQUE;

		// Check whether there are other invisibility items and refresh their effect.
		// If this isn't done there will be one incorrectly drawn frame when this
		// item expires.
		AInventory *item = Owner->Inventory;
		while (item != NULL)
		{
			if (item->IsKindOf(RUNTIME_CLASS(APowerInvisibility)) && item != this)
			{
				static_cast<APowerInvisibility*>(item)->InitEffect();
			}
			item = item->Inventory;
		}
	}
}

//===========================================================================
//
// APowerInvisibility :: AlterWeaponSprite
//
//===========================================================================

int APowerInvisibility::AlterWeaponSprite (vissprite_t *vis)
{
	int changed = Inventory == NULL? false : Inventory->AlterWeaponSprite(vis);

	// Blink if the powerup is wearing off
	if (changed == 0 && EffectTics < 4*32 && !(EffectTics & 8))
	{
		vis->RenderStyle = STYLE_Normal;
		return 1;
	}
	else if (changed == 1)
	{
		// something else set the weapon sprite back to opaque but this item is still active.
		vis->alpha = FRACUNIT/5;
		vis->RenderStyle = STYLE_OptFuzzy;
	}
	return -1;	// This item is valid so another one shouldn't reset the translucency
}

// Ghost Powerup (Heretic's version of invisibility) -------------------------

IMPLEMENT_STATELESS_ACTOR (APowerGhost, Any, -1, 0)
	PROP_Powerup_EffectTics (INVISTICS)
END_DEFAULTS

//===========================================================================
//
// APowerGhost :: InitEffect
//
//===========================================================================

void APowerGhost::InitEffect ()
{
	Owner->flags |= MF_SHADOW;
	Owner->flags3 |= MF3_GHOST;
	Owner->alpha = HR_SHADOW;
	Owner->RenderStyle = STYLE_Translucent;
}

//===========================================================================
//
// APowerGhost :: AlterWeaponSprite
//
//===========================================================================

int APowerGhost::AlterWeaponSprite (vissprite_t *vis)
{
	int changed = Inventory == NULL? false : Inventory->AlterWeaponSprite(vis);

	// Blink if the powerup is wearing off
	if (changed == 0 && EffectTics < 4*32 && !(EffectTics & 8))
	{
		vis->RenderStyle = STYLE_Normal;
		return 1;
	}
	else if (changed == 1)
	{
		// something else set the weapon sprite back to opaque but this item is still active.
		vis->alpha = HR_SHADOW;
		vis->RenderStyle = STYLE_Translucent;
	}
	return -1;	// This item is valid so another one shouldn't reset the translucency
}

// Shadow Powerup (Strife's version of invisibility) -------------------------

IMPLEMENT_STATELESS_ACTOR (APowerShadow, Any, -1, 0)
	PROP_Powerup_EffectTics (55*TICRATE)
	PROP_Inventory_FlagsSet (IF_HUBPOWER)
END_DEFAULTS

//===========================================================================
//
// APowerShadow :: InitEffect
//
//===========================================================================

void APowerShadow::InitEffect ()
{
	Owner->flags |= MF_SHADOW;
	Owner->alpha = TRANSLUC25;
	Owner->RenderStyle = STYLE_Translucent;
}

//===========================================================================
//
// APowerShadow :: AlterWeaponSprite
//
//===========================================================================

int APowerShadow::AlterWeaponSprite (vissprite_t *vis)
{
	int changed = Inventory == NULL? false : Inventory->AlterWeaponSprite(vis);

	// Blink if the powerup is wearing off
	if (changed == 0 && EffectTics < 4*32 && !(EffectTics & 8))
	{
		vis->RenderStyle = STYLE_Normal;
		return 1;
	}
	else if (changed == 1)
	{
		// something else set the weapon sprite back to opaque but this item is still active.
		vis->alpha = TRANSLUC25;
		vis->RenderStyle = STYLE_Translucent;
	}
	return -1;	// This item is valid so another one shouldn't reset the translucency
}

// Ironfeet Powerup ----------------------------------------------------------

IMPLEMENT_STATELESS_ACTOR (APowerIronFeet, Any, -1, 0)
	PROP_Powerup_EffectTics (IRONTICS)
	PROP_Powerup_Color (32, 0, 255, 0)
END_DEFAULTS

//===========================================================================
//
// APowerIronFeet :: AbsorbDamage
//
//===========================================================================

void APowerIronFeet::AbsorbDamage (int damage, FName damageType, int &newdamage)
{
	if (damageType == NAME_Drowning)
	{
		newdamage = 0;
		if (Owner->player != NULL)
		{
			Owner->player->air_finished = level.time + level.airsupply;
		}
	}
	else if (Inventory != NULL)
	{
		Inventory->AbsorbDamage (damage, damageType, newdamage);
	}
}

// Strife Environment Suit Powerup -------------------------------------------

IMPLEMENT_STATELESS_ACTOR (APowerMask, Any, -1, 0)
	PROP_Powerup_EffectTics (80*TICRATE)
	PROP_Powerup_Color (0, 0, 0, 0)
	PROP_Inventory_FlagsSet (IF_HUBPOWER)
	PROP_Inventory_Icon ("I_MASK")
END_DEFAULTS

//===========================================================================
//
// APowerMask :: AbsorbDamage
//
//===========================================================================

void APowerMask::AbsorbDamage (int damage, FName damageType, int &newdamage)
{
	if (damageType == NAME_Fire)
	{
		newdamage = 0;
	}
	else
	{
		Super::AbsorbDamage (damage, damageType, newdamage);
	}
}

//===========================================================================
//
// APowerMask :: DoEffect
//
//===========================================================================

void APowerMask::DoEffect ()
{
	Super::DoEffect ();
	if (!(level.time & 0x3f))
	{
		S_Sound (Owner, CHAN_AUTO, "misc/mask", 1, ATTN_STATIC);
	}
}

// Light-Amp Powerup ---------------------------------------------------------

IMPLEMENT_STATELESS_ACTOR (APowerLightAmp, Any, -1, 0)
	PROP_Powerup_EffectTics (INFRATICS)
END_DEFAULTS

//===========================================================================
//
// APowerLightAmp :: DoEffect
//
//===========================================================================

void APowerLightAmp::DoEffect ()
{
	Super::DoEffect ();

	if (Owner->player != NULL && Owner->player->fixedcolormap < NUMCOLORMAPS)
	{
		if (EffectTics > BLINKTHRESHOLD || (EffectTics & 8))
		{	
			Owner->player->fixedcolormap = 1;
		}
		else
		{
			Owner->player->fixedcolormap = 0;
		}
	}
}

//===========================================================================
//
// APowerLightAmp :: EndEffect
//
//===========================================================================

void APowerLightAmp::EndEffect ()
{
	if (Owner != NULL && Owner->player != NULL && Owner->player->fixedcolormap < NUMCOLORMAPS)
	{
		Owner->player->fixedcolormap = 0;
	}
}

// Torch Powerup -------------------------------------------------------------

IMPLEMENT_STATELESS_ACTOR (APowerTorch, Any, -1, 0)
	PROP_Powerup_EffectTics (INFRATICS)
END_DEFAULTS

//===========================================================================
//
// APowerTorch :: Serialize
//
//===========================================================================

void APowerTorch::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << NewTorch << NewTorchDelta;
}

//===========================================================================
//
// APowerTorch :: DoEffect
//
//===========================================================================

void APowerTorch::DoEffect ()
{
	if (Owner == NULL || Owner->player == NULL)
	{
		return;
	}

	if (EffectTics <= BLINKTHRESHOLD || Owner->player->fixedcolormap >= NUMCOLORMAPS)
	{
		Super::DoEffect ();
	}
	else 
	{
		APowerup::DoEffect ();

		if (!(level.time & 16) && Owner->player != NULL)
		{
			if (NewTorch != 0)
			{
				if (Owner->player->fixedcolormap + NewTorchDelta > 7
					|| Owner->player->fixedcolormap + NewTorchDelta < 1
					|| NewTorch == Owner->player->fixedcolormap)
				{
					NewTorch = 0;
				}
				else
				{
					Owner->player->fixedcolormap += NewTorchDelta;
				}
			}
			else
			{
				NewTorch = (pr_torch() & 7) + 1;
				NewTorchDelta = (NewTorch == Owner->player->fixedcolormap) ?
					0 : ((NewTorch > Owner->player->fixedcolormap) ? 1 : -1);
			}
		}
	}
}

// Flight (aka Wings of Wrath) powerup ---------------------------------------

IMPLEMENT_STATELESS_ACTOR (APowerFlight, Any, -1, 0)
	PROP_Powerup_EffectTics (FLIGHTTICS)
	PROP_Inventory_FlagsSet (IF_HUBPOWER)
END_DEFAULTS

//===========================================================================
//
// APowerFlight :: Serialize
//
//===========================================================================

void APowerFlight::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << HitCenterFrame;
}

//===========================================================================
//
// APowerFlight :: InitEffect
//
//===========================================================================

void APowerFlight::InitEffect ()
{
	Owner->flags2 |= MF2_FLY;
	Owner->flags |= MF_NOGRAVITY;
	if (Owner->z <= Owner->floorz)
	{
		Owner->momz = 4*FRACUNIT;	// thrust the player in the air a bit
	}
	if (Owner->momz <= -35*FRACUNIT)
	{ // stop falling scream
		S_StopSound (Owner, CHAN_VOICE);
	}
}

//===========================================================================
//
// APowerFlight :: DoEffect
//
//===========================================================================

void APowerFlight::Tick ()
{
	// The Wings of Wrath only expire in multiplayer and non-hub games
	if (( NETWORK_GetState( ) == NETSTATE_SINGLE ) && (level.clusterflags & CLUSTER_HUB))
	{
		EffectTics++;
	}

	Super::Tick ();

//	Owner->flags |= MF_NOGRAVITY;
//	Owner->flags2 |= MF2_FLY;
}

//===========================================================================
//
// APowerFlight :: EndEffect
//
//===========================================================================

void APowerFlight::EndEffect ()
{
	if (Owner == NULL || Owner->player == NULL)
	{
		return;
	}
	if (!(Owner->player->cheats & CF_FLY))
	{
		if (Owner->z != Owner->floorz)
		{
			Owner->player->centering = true;
		}
		Owner->flags2 &= ~MF2_FLY;
		Owner->flags &= ~MF_NOGRAVITY;
	}
//	BorderTopRefresh = screen->GetPageCount (); //make sure the sprite's cleared out
}

//===========================================================================
//
// APowerFlight :: DrawPowerup
//
//===========================================================================

bool APowerFlight::DrawPowerup (int x, int y)
{
	if (EffectTics > BLINKTHRESHOLD || !(EffectTics & 16))
	{
		int picnum = TexMan.CheckForTexture ("SPFLY0", FTexture::TEX_MiscPatch);
		int frame = (level.time/3) & 15;

		if (picnum <= 0)
		{
			return false;
		}
		if (Owner->flags & MF_NOGRAVITY)
		{
			if (HitCenterFrame && (frame != 15 && frame != 0))
			{
				screen->DrawTexture (TexMan[picnum+15], x, y,
					DTA_HUDRules, HUD_Normal, TAG_DONE);
			}
			else
			{
				screen->DrawTexture (TexMan[picnum+frame], x, y,
					DTA_HUDRules, HUD_Normal, TAG_DONE);
				HitCenterFrame = false;
			}
		}
		else
		{
			if (!HitCenterFrame && (frame != 15 && frame != 0))
			{
				screen->DrawTexture (TexMan[picnum+frame], x, y,
					DTA_HUDRules, HUD_Normal, TAG_DONE);
				HitCenterFrame = false;
			}
			else
			{
				screen->DrawTexture (TexMan[picnum+15], x, y,
					DTA_HUDRules, HUD_Normal, TAG_DONE);
				HitCenterFrame = true;
			}
		}
	}
	return true;
}

// Weapon Level 2 (aka Tome of Power) Powerup --------------------------------

IMPLEMENT_STATELESS_ACTOR (APowerWeaponLevel2, Any, -1, 0)
	PROP_Powerup_EffectTics (WPNLEV2TICS)
	PROP_Inventory_Icon ("SPINBK0")
END_DEFAULTS

//===========================================================================
//
// APowerWeaponLevel2 :: InitEffect
//
//===========================================================================

void APowerWeaponLevel2::InitEffect ()
{
	AWeapon *weapon, *sister;

	if (Owner->player == NULL)
		return;

	weapon = Owner->player->ReadyWeapon;

	if (weapon == NULL)
		return;

	sister = weapon->SisterWeapon;

	if (sister == NULL)
		return;

	if (!(sister->WeaponFlags & WIF_POWERED_UP))
		return;

	Owner->player->ReadyWeapon = sister;

	if (weapon->GetReadyState() != sister->GetReadyState())
	{
		P_SetPsprite (Owner->player, ps_weapon, sister->GetReadyState());
	}
}

//===========================================================================
//
// APowerWeaponLevel2 :: EndEffect
//
//===========================================================================

void APowerWeaponLevel2::EndEffect ()
{
	player_t *player = Owner != NULL ? Owner->player : NULL;

	if (player != NULL &&
		player->ReadyWeapon != NULL &&
		player->ReadyWeapon->WeaponFlags & WIF_POWERED_UP)
	{
		player->ReadyWeapon->EndPowerup ();
	}
//	BorderTopRefresh = screen->GetPageCount ();
}

// Player Speed Trail (used by the Speed Powerup) ----------------------------

class APlayerSpeedTrail : public AActor
{
	DECLARE_STATELESS_ACTOR (APlayerSpeedTrail, AActor)
public:
	void Tick ();
};

IMPLEMENT_STATELESS_ACTOR (APlayerSpeedTrail, Any, -1, 0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY)
	PROP_Alpha (FRACUNIT*6/10)
	PROP_RenderStyle (STYLE_Translucent)
END_DEFAULTS

//===========================================================================
//
// APlayerSpeedTrail :: Tick
//
//===========================================================================

void APlayerSpeedTrail::Tick ()
{
	const int fade = OPAQUE*6/10/8;
	if (alpha <= fade)
	{
		Destroy ();
	}
	else
	{
		alpha -= fade;
	}
}

// Speed Powerup -------------------------------------------------------------

IMPLEMENT_STATELESS_ACTOR (APowerSpeed, Any, -1, 0)
	PROP_Powerup_EffectTics (SPEEDTICS)
	PROP_Inventory_Icon ("SPBOOT0")
END_DEFAULTS

//===========================================================================
//
// APowerSpeed :: InitEffect
//
//===========================================================================

void APowerSpeed::InitEffect ()
{
	Owner->player->Powers |= PW_SPEED;
}

//===========================================================================
//
// APowerSpeed :: DoEffect
//
//===========================================================================

void APowerSpeed::DoEffect ()
{
	Super::DoEffect ();

//	if (Owner->player->cheats & CF_PREDICTING)
//		return;

	// [BC] In Skulltag, display a little message saying that the powerup is about to wear off,
	// since there isn't any screen blend that we can blink.
	if ( gameinfo.gametype == GAME_Doom )
	{
		if (( EffectTics == ( 4 * TICRATE )) && ( Owner ) && ( Owner->CheckLocalView( consoleplayer )))
		{
//			Printf( "%s wearing off...\n", this->GetClass( )->TypeName.GetChars( ));
			Printf( "Turbosphere wearing off...\n" );
		}
	}

	if (level.time & 1)
		return;

	if ( gameinfo.gametype != GAME_Doom )
	{
		if (P_AproxDistance (Owner->momx, Owner->momy) <= 12*FRACUNIT)
			return;
	}

	AActor *speedMo = Spawn<APlayerSpeedTrail> (Owner->x, Owner->y, Owner->z, NO_REPLACE);
	if (speedMo)
	{
		speedMo->angle = Owner->angle;
		speedMo->Translation = Owner->Translation;
		speedMo->target = Owner;
		speedMo->sprite = Owner->sprite;
		speedMo->frame = Owner->frame;
		speedMo->floorclip = Owner->floorclip;

		// [BC] Also get the scale from the owner.
		speedMo->scaleX = Owner->scaleX;
		speedMo->scaleY = Owner->scaleY;

		if (Owner == players[consoleplayer].camera &&
			!(Owner->player->cheats & CF_CHASECAM))
		{
			speedMo->renderflags |= RF_INVISIBLE;
		}
	}
}

//===========================================================================
//
// APowerSpeed :: EndEffect
//
//===========================================================================

void APowerSpeed::EndEffect ()
{
	if (Owner != NULL && Owner->player != NULL)
	{
		Owner->player->Powers &= ~PW_SPEED;
	}
}

// Minotaur (aka Dark Servant) powerup ---------------------------------------

IMPLEMENT_STATELESS_ACTOR (APowerMinotaur, Any, -1, 0)
	PROP_Powerup_EffectTics (MAULATORTICS)
	PROP_Inventory_Icon ("SPMINO0")
END_DEFAULTS

// Targeter powerup ---------------------------------------------------------

FState APowerTargeter::States[] =
{
	S_NORMAL (TRGT, 'A', -1, NULL, NULL),
	S_NORMAL (TRGT, 'B', -1, NULL, NULL),
	S_NORMAL (TRGT, 'C', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (APowerTargeter, Any, -1, 0)
	PROP_Powerup_EffectTics (160*TICRATE)
	PROP_Inventory_FlagsSet (IF_HUBPOWER)
END_DEFAULTS

void APowerTargeter::Travelled ()
{
	InitEffect ();
}

void APowerTargeter::InitEffect ()
{
	player_t *player;

	if ((player = Owner->player) == NULL)
		return;

	P_SetPsprite (player, ps_targetcenter, &States[0]);
	P_SetPsprite (player, ps_targetleft, &States[1]);
	P_SetPsprite (player, ps_targetright, &States[2]);

	player->psprites[ps_targetcenter].sx = (160-3)*FRACUNIT;
	player->psprites[ps_targetcenter].sy =
		player->psprites[ps_targetleft].sy =
		player->psprites[ps_targetright].sy = (100-3)*FRACUNIT;
	PositionAccuracy ();
}

void APowerTargeter::DoEffect ()
{
	Super::DoEffect ();

	if (Owner != NULL && Owner->player != NULL)
	{
		player_t *player = Owner->player;

		PositionAccuracy ();
		if (EffectTics < 5*TICRATE)
		{
			if (EffectTics & 32)
			{
				P_SetPsprite (player, ps_targetright, NULL);
				P_SetPsprite (player, ps_targetleft, &States[1]);
			}
			else if (EffectTics & 16)
			{
				P_SetPsprite (player, ps_targetright, &States[2]);
				P_SetPsprite (player, ps_targetleft, NULL);
			}
		}
	}
}

void APowerTargeter::EndEffect ()
{
	if (Owner != NULL && Owner->player != NULL)
	{
		P_SetPsprite (Owner->player, ps_targetcenter, NULL);
		P_SetPsprite (Owner->player, ps_targetleft, NULL);
		P_SetPsprite (Owner->player, ps_targetright, NULL);
	}
}

void APowerTargeter::PositionAccuracy ()
{
	player_t *player = Owner->player;

	player->psprites[ps_targetleft].sx = (160-3)*FRACUNIT - ((100 - player->accuracy) << FRACBITS);
	player->psprites[ps_targetright].sx = (160-3)*FRACUNIT + ((100 - player->accuracy) << FRACBITS);
}

// Frightener Powerup --------------------------------

IMPLEMENT_STATELESS_ACTOR (APowerFrightener, Any, -1, 0)
	PROP_Powerup_EffectTics (60*TICRATE)
END_DEFAULTS

//===========================================================================
//
// APowerFrightener :: InitEffect
//
//===========================================================================

void APowerFrightener::InitEffect ()
{
	if (Owner== NULL || Owner->player == NULL)
		return;

	Owner->player->cheats |= CF_FRIGHTENING;
}

//===========================================================================
//
// APowerFrightener :: EndEffect
//
//===========================================================================

void APowerFrightener::EndEffect ()
{
	if (Owner== NULL || Owner->player == NULL)
		return;

	Owner->player->cheats &= ~CF_FRIGHTENING;
}

// Scanner powerup ----------------------------------------------------------

IMPLEMENT_STATELESS_ACTOR (APowerScanner, Any, -1, 0)
	PROP_Powerup_EffectTics (80*TICRATE)
	PROP_Inventory_FlagsSet (IF_HUBPOWER)
END_DEFAULTS

// [BC] Start of new Skulltag powerup types.
// Time freezer powerup -----------------------------------------------------

IMPLEMENT_STATELESS_ACTOR( APowerTimeFreezer, Any, -1, 0 )
	PROP_Powerup_EffectTics( TIMEFREEZE_TICS )
END_DEFAULTS

// Need to set the default for each game here
AT_GAME_SET( PowerTimeFreezer )
{
	APowerTimeFreezer	*pTimeFreezer;
	
	pTimeFreezer = GetDefault<APowerTimeFreezer>( );
	switch ( gameinfo.gametype )
	{
	case GAME_Doom:

		pTimeFreezer->BlendColor = GOLDCOLORMAP;
		break;
	}
}

//===========================================================================
//
// APowerTimeFreezer :: InitEffect
//
//===========================================================================

void APowerTimeFreezer::InitEffect( )
{
	ULONG	ulIdx;

	// When this powerup is in effect, pause the music.
	S_PauseSound( false );

	// Give the player and his teammates the power to move when time is frozen.
	Owner->player->Powers |= PW_TIMEFREEZE;
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if (( playeringame[ulIdx] == false ) ||
			( ulIdx == ( Owner->player - players )) ||
			( players[ulIdx].mo == NULL ) ||
			( players[ulIdx].mo->IsTeammate( Owner ) == false ))
		{
			continue;
		}

		players[ulIdx].Powers |= PW_TIMEFREEZE;
	}

	// Finally, freeze the game.
	GAME_SetFreezeMode( true );
}

//===========================================================================
//
// APowerTimeFreezer :: DoEffect
//
//===========================================================================

void APowerTimeFreezer::DoEffect( )
{
	Super::DoEffect ();
	if ( EffectTics > 4*32 
		|| (( EffectTics > 3*32 && EffectTics <= 4*32 ) && EffectTics % 16 != 0 )
		|| (( EffectTics > 2*32 && EffectTics <= 3*32 ) && EffectTics % 8 != 0 ) 
		|| (( EffectTics > 32 && EffectTics <= 2*32 ) && EffectTics % 4 != 0 )
		|| (( EffectTics > 0 && EffectTics <= 1*32 ) && EffectTics % 2 != 0 ))
		GAME_SetFreezeMode( true );
	else
		GAME_SetFreezeMode( false );
}

//===========================================================================
//
// APowerTimeFreezer :: EndEffect
//
//===========================================================================

void APowerTimeFreezer::EndEffect( )
{
	ULONG	ulIdx;

	// Allow other actors to move about freely once again.
	GAME_SetFreezeMode( false );

	// Nothing more to do if there's no owner.
	if (( Owner == NULL ) || ( Owner->player == NULL ))
	{
		return;
	}

	// Take away the time freeze power, and his teammates.
	Owner->player->Powers &= ~PW_TIMEFREEZE;
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if (( playeringame[ulIdx] == false ) ||
			( ulIdx == ( Owner->player - players )) ||
			( players[ulIdx].mo == NULL ) ||
			( players[ulIdx].mo->IsTeammate( Owner ) == false ))
		{
			continue;
		}

		players[ulIdx].Powers &= ~PW_TIMEFREEZE;
	}

	// If nobody has a time freeze sphere anymore, turn the music back on.
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if (( playeringame[ulIdx] == false ) ||
			(( players[ulIdx].Powers & PW_TIMEFREEZE ) == false ))
		{
			continue;
		}

		S_ResumeSound( );
		break;
	}

	// Reset the player's view colormap, as well as the colormap that's applied to
	// his body.
	Owner->player->fixedcolormap = 0;
	Owner->lFixedColormap = 0;
}

// Quad damage powerup ------------------------------------------------------

IMPLEMENT_STATELESS_ACTOR( APowerQuadDamage, Any, -1, 0 )
	PROP_Powerup_EffectTics( QUADDAMAGE_TICS )
END_DEFAULTS

// Need to set the default for each game here
AT_GAME_SET( PowerQuadDamage )
{
	APowerQuadDamage	*pQuadDamage;
	
	pQuadDamage = GetDefault<APowerQuadDamage>( );
	switch ( gameinfo.gametype )
	{
	case GAME_Doom:

		pQuadDamage->BlendColor = REDCOLORMAP;
		break;
	}
}

//===========================================================================
//
// APowerQuadDamage :: InitEffect
//
//===========================================================================

void APowerQuadDamage::InitEffect( )
{
	// Give the player the power to deal four times the amount of damage he normally would.
	Owner->player->Powers |= PW_QUADDAMAGE;
}

//===========================================================================
//
// APowerQuadDamage :: EndEffect
//
//===========================================================================

void APowerQuadDamage::EndEffect( )
{
	// Nothing to do if there's no owner.
	if (( Owner == NULL ) || ( Owner->player == NULL ))
	{
		return;
	}

	// Take away the quad damage power.
	Owner->player->Powers &= ~PW_QUADDAMAGE;
}

// Quarter damage powerup ------------------------------------------------------

IMPLEMENT_STATELESS_ACTOR( APowerQuarterDamage, Any, -1, 0 )
	PROP_Powerup_EffectTics( QUARTERDAMAGE_TICS )
END_DEFAULTS

// Need to set the default for each game here
AT_GAME_SET( PowerQuarterDamage )
{
	APowerQuarterDamage	*pQuarterDamage;
	
	pQuarterDamage = GetDefault<APowerQuarterDamage>( );
	switch ( gameinfo.gametype )
	{
	case GAME_Doom:

		pQuarterDamage->BlendColor = GREENCOLORMAP;
		break;
	}
}

//===========================================================================
//
// APowerQuarterDamage :: InitEffect
//
//===========================================================================

void APowerQuarterDamage::InitEffect( )
{
	// Give the player the power to take four times less damage than he normally would.
	Owner->player->Powers |= PW_QUARTERDAMAGE;
}

//===========================================================================
//
// APowerQuarterDamage :: EndEffect
//
//===========================================================================

void APowerQuarterDamage::EndEffect( )
{
	// Nothing to do if there's no owner.
	if (( Owner == NULL ) || ( Owner->player == NULL ))
	{
		return;
	}

	// Take away the quarter damage power.
	Owner->player->Powers &= ~PW_QUARTERDAMAGE;
}

// Possession artifact powerup -------------------------------------------------

IMPLEMENT_STATELESS_ACTOR( APowerPossessionArtifact, Any, -1, 0 )
	PROP_Powerup_EffectTics( 0 )
END_DEFAULTS

//===========================================================================
//
// APowerPossessionArtifact :: InitEffect
//
//===========================================================================

void APowerPossessionArtifact::InitEffect( )
{
	// Flag the player as carrying the possession artifact.
	Owner->player->Powers |= PW_POSSESSIONARTIFACT;

	// Tell the possession module that the artifact has been picked up.
	if (( possession || teampossession ) && ( NETWORK_GetState( ) != NETSTATE_CLIENT ))
		POSSESSION_ArtifactPickedUp( Owner->player, sv_possessionholdtime * TICRATE );
}

//===========================================================================
//
// APowerPossessionArtifact :: DoEffect
//
//===========================================================================

void APowerPossessionArtifact::DoEffect( )
{
	Super::DoEffect ();
	// Nothing to do if there's no owner.
	if (( Owner == NULL ) || ( Owner->player == NULL ))
	{
		return;
	}

	// When the player carries the possession artifact, the player's weapon
	// must always be lowered.
	P_SetPsprite( Owner->player, ps_weapon, NULL );
}

//===========================================================================
//
// APowerPossessionArtifact :: EndEffect
//
//===========================================================================

void APowerPossessionArtifact::EndEffect( )
{
	// Nothing to do if there's no owner.
	if (( Owner == NULL ) || ( Owner->player == NULL ))
	{
		return;
	}

	// Take away the possession artifact flag.
	Owner->player->Powers &= ~PW_POSSESSIONARTIFACT;
}

// Terminator artifact powerup -------------------------------------------------

IMPLEMENT_STATELESS_ACTOR( APowerTerminatorArtifact, Any, -1, 0 )
	PROP_Powerup_EffectTics( 0 )
END_DEFAULTS

//===========================================================================
//
// APowerTerminatorArtifact :: InitEffect
//
//===========================================================================

void APowerTerminatorArtifact::InitEffect( )
{
	// Flag the player as carrying the terminator artifact.
	Owner->player->Powers |= PW_TERMINATORARTIFACT;

	// Also, give the player a megasphere as part of the bonus.
	Owner->GiveInventoryType( PClass::FindClass( "Megasphere" ));
}

//===========================================================================
//
// APowerTerminatorArtifact :: DoEffect
//
//===========================================================================

void APowerTerminatorArtifact::DoEffect( )
{
	Super::DoEffect ();
}

//===========================================================================
//
// APowerTerminatorArtifact :: EndEffect
//
//===========================================================================

void APowerTerminatorArtifact::EndEffect( )
{
	// Nothing to do if there's no owner.
	if (( Owner == NULL ) || ( Owner->player == NULL ))
	{
		return;
	}

	// Take away the terminator artifact flag.
	Owner->player->Powers &= ~PW_TERMINATORARTIFACT;
}

// Translucency Powerup (Skulltag's version of invisibility) ----------------

IMPLEMENT_STATELESS_ACTOR( APowerTranslucency, Any, -1, 0 )
	PROP_Powerup_EffectTics( TRANSLUCENCY_TICS )
END_DEFAULTS

//===========================================================================
//
// APowerTranslucency :: InitEffect
//
//===========================================================================

EXTERN_CVAR (Bool, r_drawtrans)
void APowerTranslucency::InitEffect( )
{
	// [BC] If r_drawtrans is false, then just give the same effect as partial invisibility.
	if ( r_drawtrans == false )
	{
		Owner->flags |= MF_SHADOW;
		Owner->alpha = FRACUNIT/5;
		Owner->RenderStyle = STYLE_OptFuzzy;
	}
	else
	{
		Owner->alpha = ( FRACUNIT / 10 );
		Owner->RenderStyle = STYLE_Translucent;
	}
}

// Rune-Giver -------------------------------------------------------------

//===========================================================================
//
// ARuneGiver :: Use
//
//===========================================================================

bool ARuneGiver::Use (bool pickup)
{
	ARune *rune = static_cast<ARune *> (Spawn (RuneType, 0, 0, 0, NO_REPLACE));

	if (BlendColor != 0)
	{
		rune->BlendColor = BlendColor;
	}

	// [RC] Apply rune icons. For some obscure reason, the bulk of which is in debugger hieroglyphics, only the RuneGivers have icons.
	rune->Icon = ARuneGiver::Icon;

	rune->ItemFlags |= ItemFlags & IF_ALWAYSPICKUP;
	if (rune->TryPickup (Owner))
	{
		return true;
	}
	rune->GoAwayAndDie ();
	return false;
}

//===========================================================================
//
// ARuneGiver :: Serialize
//
//===========================================================================

void ARuneGiver::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << RuneType;
	arc << BlendColor;
}

// Rune ---------------------------------------------------------------------

//===========================================================================
//
// ARune :: Tick
//
//===========================================================================

void ARune::Tick ()
{
	if (Owner == NULL)
	{
		Destroy ();
	}
}

//===========================================================================
//
// ARune :: Serialize
//
//===========================================================================

void ARune::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << BlendColor;
}

//===========================================================================
//
// ARune :: GetBlend
//
//===========================================================================

PalEntry ARune::GetBlend ()
{
	if (BlendColor == INVERSECOLOR ||
		BlendColor == GOLDCOLOR ||
		// [BC] HAX!
		BlendColor == REDCOLOR ||
		BlendColor == GREENCOLOR) 
		return 0;

	return BlendColor;
}

//===========================================================================
//
// ARune :: InitEffect
//
//===========================================================================

void ARune::InitEffect ()
{
}

//===========================================================================
//
// ARune :: DoEffect
//
//===========================================================================

void ARune::DoEffect ()
{
	if (Owner == NULL)
	{
		return;
	}

	// [BC] Apply the colormap to the player's body, also.
	if (BlendColor == INVERSECOLOR)
	{
		Owner->player->fixedcolormap = INVERSECOLORMAP;
		Owner->lFixedColormap = INVERSECOLORMAP;
	}
	else if (BlendColor == GOLDCOLOR)
	{
		Owner->player->fixedcolormap = GOLDCOLORMAP;
		Owner->lFixedColormap = GOLDCOLORMAP;
	}
	// [BC] HAX!
	else if (BlendColor == REDCOLOR)
	{
		Owner->player->fixedcolormap = REDCOLORMAP;
		Owner->lFixedColormap = REDCOLORMAP;
	}
	else if (BlendColor == GREENCOLOR)
	{
		Owner->player->fixedcolormap = GREENCOLORMAP;
		Owner->lFixedColormap = GREENCOLORMAP;
	}
}

//===========================================================================
//
// ARune :: EndEffect
//
//===========================================================================

void ARune::EndEffect ()
{
}

//===========================================================================
//
// ARune :: Destroy
//
//===========================================================================

void ARune::Destroy ()
{
	EndEffect ();
	Super::Destroy ();
}

//===========================================================================
//
// ARune :: DrawPowerup
//
//===========================================================================

bool ARune::DrawPowerup (int x, int y)
{
/* [RC] Runes are drawn by the HUD.

	if (Icon <= 0)
	{
		return false;
	}

	FTexture *pic = TexMan(Icon);
	screen->DrawTexture (pic, x, y,
		DTA_HUDRules, HUD_Normal,
//		DTA_TopOffset, pic->GetHeight()/2,
//		DTA_LeftOffset, pic->GetWidth()/2,
		TAG_DONE);
*/
	return true;
}

//===========================================================================
//
// ARune :: HandlePickup
//
//===========================================================================

bool ARune::HandlePickup (AInventory *item)
{
	if (item->GetClass() == GetClass())
	{
		// We can't pickup two of the same rune!
		return ( true );
	}
	// If this new item is a rune, pick it up, but get rid of our current rune.
	if ( item->IsKindOf( RUNTIME_CLASS( ARune )))
	{
		Owner->RemoveInventory( this );
		return ( false );
	}
	if (Inventory != NULL)
	{
		return Inventory->HandlePickup (item);
	}
	return false;
}

//===========================================================================
//
// ARune :: CreateCopy
//
//===========================================================================

AInventory *ARune::CreateCopy (AActor *other)
{
	Owner = other;
	InitEffect ();
	Owner = NULL;
	return this;
}

//===========================================================================
//
// ARune :: CreateTossable
//
//
//===========================================================================

AInventory *ARune::CreateTossable ()
{
	return NULL;
}

//===========================================================================
//
// ARune :: OwnerDied
//
// Runes don't last beyond death.
//
//===========================================================================

void ARune::OwnerDied ()
{
	Destroy ();
}

//===========================================================================
//
// ARune :: DetachFromOwner
//
// When the rune is removed from the owner, take its powers away from its owner.
//
//===========================================================================

void ARune::DetachFromOwner( )
{
	EndEffect( );
	Super::DetachFromOwner( );
}

// Double damage rune -------------------------------------------------------

IMPLEMENT_STATELESS_ACTOR( ARuneDoubleDamage, Any, -1, 0 )
END_DEFAULTS

//===========================================================================
//
// ARuneDoubleDamage :: InitEffect
//
//===========================================================================

void ARuneDoubleDamage::InitEffect( )
{
	// Give the player the power to deal double the amount of damage he normally would.
	Owner->player->Powers |= PW_DOUBLEDAMAGE;
}

//===========================================================================
//
// ARuneDoubleDamage :: EndEffect
//
//===========================================================================

void ARuneDoubleDamage::EndEffect( )
{
	// Nothing to do if there's no owner.
	if (( Owner == NULL ) || ( Owner->player == NULL ))
	{
		return;
	}

	// Take away the double damage power.
	Owner->player->Powers &= ~PW_DOUBLEDAMAGE;
}

// Double firing speed rune -------------------------------------------------

IMPLEMENT_STATELESS_ACTOR( ARuneDoubleFiringSpeed, Any, -1, 0 )
END_DEFAULTS

//===========================================================================
//
// ARuneDoubleFiringSpeed :: InitEffect
//
//===========================================================================

void ARuneDoubleFiringSpeed::InitEffect( )
{
	// Give the player the power to fire twice as fast.
	Owner->player->Powers |= PW_DOUBLEFIRINGSPEED;
}

//===========================================================================
//
// ARuneDoubleFiringSpeed :: EndEffect
//
//===========================================================================

void ARuneDoubleFiringSpeed::EndEffect( )
{
	// Nothing to do if there's no owner.
	if (( Owner == NULL ) || ( Owner->player == NULL ))
	{
		return;
	}

	// Take away the double firing speed power.
	Owner->player->Powers &= ~PW_DOUBLEFIRINGSPEED;
}

// Drain rune -------------------------------------------------------

IMPLEMENT_STATELESS_ACTOR( ARuneDrain, Any, -1, 0 )
END_DEFAULTS

//===========================================================================
//
// ARuneDrain :: InitEffect
//
//===========================================================================

void ARuneDrain::InitEffect( )
{
	// Give the player the power to drain life from opponents when he damages them.
	Owner->player->Powers |= PW_DRAIN;
}

//===========================================================================
//
// ARuneDrain :: EndEffect
//
//===========================================================================

void ARuneDrain::EndEffect( )
{
	// Nothing to do if there's no owner.
	if (( Owner == NULL ) || ( Owner->player == NULL ))
	{
		return;
	}

	// Take away the drain power.
	Owner->player->Powers &= ~PW_DRAIN;
}

// Spread rune -------------------------------------------------------

IMPLEMENT_STATELESS_ACTOR( ARuneSpread, Any, -1, 0 )
END_DEFAULTS

//===========================================================================
//
// ARuneSpread :: InitEffect
//
//===========================================================================

void ARuneSpread::InitEffect( )
{
	// Give the player the power to shoot 3x the number of missiles he normally would.
	Owner->player->Powers |= PW_SPREAD;
}

//===========================================================================
//
// ARuneSpread :: EndEffect
//
//===========================================================================

void ARuneSpread::EndEffect( )
{
	// Nothing to do if there's no owner.
	if (( Owner == NULL ) || ( Owner->player == NULL ))
	{
		return;
	}

	// Take away the spread power.
	Owner->player->Powers &= ~PW_SPREAD;
}

// Half damage rune -------------------------------------------------------

IMPLEMENT_STATELESS_ACTOR( ARuneHalfDamage, Any, -1, 0 )
END_DEFAULTS

//===========================================================================
//
// ARuneHalfDamage :: InitEffect
//
//===========================================================================

void ARuneHalfDamage::InitEffect( )
{
	// Give the player the power to take half the amount of damage he normally would.
	Owner->player->Powers |= PW_HALFDAMAGE;
}

//===========================================================================
//
// ARuneHalfDamage :: EndEffect
//
//===========================================================================

void ARuneHalfDamage::EndEffect( )
{
	// Nothing to do if there's no owner.
	if (( Owner == NULL ) || ( Owner->player == NULL ))
	{
		return;
	}

	// Take away the half damage power.
	Owner->player->Powers &= ~PW_HALFDAMAGE;
}

// Regeneration rune -------------------------------------------------------

IMPLEMENT_STATELESS_ACTOR( ARuneRegeneration, Any, -1, 0 )
END_DEFAULTS

//===========================================================================
//
// ARuneRegeneration :: InitEffect
//
//===========================================================================

void ARuneRegeneration::InitEffect( )
{
	// Give the player the power to regnerate lost life.
	Owner->player->Powers |= PW_REGENERATION;
}

//===========================================================================
//
// ARuneRegeneration :: EndEffect
//
//===========================================================================

void ARuneRegeneration::EndEffect( )
{
	// Nothing to do if there's no owner.
	if (( Owner == NULL ) || ( Owner->player == NULL ))
	{
		return;
	}

	// Take away the regeneration power.
	Owner->player->Powers &= ~PW_REGENERATION;
}

// Prosperity rune -------------------------------------------------------

IMPLEMENT_STATELESS_ACTOR( ARuneProsperity, Any, -1, 0 )
END_DEFAULTS

//===========================================================================
//
// ARuneProsperity :: InitEffect
//
//===========================================================================

void ARuneProsperity::InitEffect( )
{
	// Give the player the power to pickup base health artifacts past 100%.
	Owner->player->Powers |= PW_PROSPERITY;
}

//===========================================================================
//
// ARuneProsperity :: EndEffect
//
//===========================================================================

void ARuneProsperity::EndEffect( )
{
	// Nothing to do if there's no owner.
	if (( Owner == NULL ) || ( Owner->player == NULL ))
	{
		return;
	}

	// Take away the prosperity power.
	Owner->player->Powers &= ~PW_PROSPERITY;
}

// Reflection rune -------------------------------------------------------

IMPLEMENT_STATELESS_ACTOR( ARuneReflection, Any, -1, 0 )
END_DEFAULTS

//===========================================================================
//
// ARuneReflection :: InitEffect
//
//===========================================================================

void ARuneReflection::InitEffect( )
{
	// Give the player the power to reflect damage back at their attacker.
	Owner->player->Powers |= PW_REFLECTION;
}

//===========================================================================
//
// ARuneReflection :: EndEffect
//
//===========================================================================

void ARuneReflection::EndEffect( )
{
	// Nothing to do if there's no owner.
	if (( Owner == NULL ) || ( Owner->player == NULL ))
	{
		return;
	}

	// Take away the reflection power.
	Owner->player->Powers &= ~PW_REFLECTION;
}

// High jump rune -------------------------------------------------------

IMPLEMENT_STATELESS_ACTOR( ARuneHighJump, Any, -1, 0 )
END_DEFAULTS

//===========================================================================
//
// ARuneHighJump :: InitEffect
//
//===========================================================================

void ARuneHighJump::InitEffect( )
{
	// Give the player the power to jump much higher.
	Owner->player->Powers |= PW_HIGHJUMP;
}

//===========================================================================
//
// ARuneHighJump :: EndEffect
//
//===========================================================================

void ARuneHighJump::EndEffect( )
{
	// Nothing to do if there's no owner.
	if (( Owner == NULL ) || ( Owner->player == NULL ))
	{
		return;
	}

	// Take away the high jump power.
	Owner->player->Powers &= ~PW_HIGHJUMP;
}

// Speed +25% rune -------------------------------------------------------

IMPLEMENT_STATELESS_ACTOR( ARuneSpeed25, Any, -1, 0 )
END_DEFAULTS

//===========================================================================
//
// ARuneDoubleDamage :: InitEffect
//
//===========================================================================

void ARuneSpeed25::InitEffect( )
{
	// Give the player the power to run 25% faster.
	Owner->player->Powers |= PW_SPEED25;
}

//===========================================================================
//
// ARuneSpeed25 :: DoEffect
//
//===========================================================================

void ARuneSpeed25::DoEffect ()
{
//	if (Owner->player->cheats & CF_PREDICTING)
//		return;

	Super::DoEffect ();

	if (level.time & 1)
		return;

	if ( gameinfo.gametype != GAME_Doom )
	{
		if (P_AproxDistance (Owner->momx, Owner->momy) <= 12*FRACUNIT)
			return;
	}

	AActor *speedMo = Spawn<APlayerSpeedTrail> (Owner->x, Owner->y, Owner->z, NO_REPLACE);
	if (speedMo)
	{
		speedMo->angle = Owner->angle;
		speedMo->Translation = Owner->Translation;
		speedMo->target = Owner;
		speedMo->sprite = Owner->sprite;
		speedMo->frame = Owner->frame;
		speedMo->floorclip = Owner->floorclip;

		// [BC] Also get the scale from the owner.
		// [GZDoom]
		speedMo->scaleX = Owner->scaleX;
		speedMo->scaleY = Owner->scaleY;
		//speedMo->xscale = Owner->xscale;
		//speedMo->yscale = Owner->yscale;

		if (Owner == players[consoleplayer].camera &&
			!(Owner->player->cheats & CF_CHASECAM))
		{
			speedMo->renderflags |= RF_INVISIBLE;
		}
	}
}

//===========================================================================
//
// ARuneDoubleDamage :: EndEffect
//
//===========================================================================

void ARuneSpeed25::EndEffect( )
{
	// Nothing to do if there's no owner.
	if (( Owner == NULL ) || ( Owner->player == NULL ))
	{
		return;
	}

	// Take away the speed power.
	Owner->player->Powers &= ~PW_SPEED25;
}
