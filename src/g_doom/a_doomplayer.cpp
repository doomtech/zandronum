#include "actor.h"
#include "gi.h"
#include "m_random.h"
#include "s_sound.h"
#include "d_player.h"
#include "a_action.h"
#include "p_local.h"
#include "a_doomglobal.h"
#include "w_wad.h"
#include "deathmatch.h"
#include "lastmanstanding.h"
#include "team.h"
#include "cl_commands.h"
#include "cl_demo.h"

void A_Pain (AActor *);
void A_PlayerScream (AActor *);
void A_XScream (AActor *);
void A_DoomSkinCheck1 (AActor *);
void A_DoomSkinCheck2 (AActor *);

FState ADoomPlayer::States[] =
{
#define S_PLAY 0
	S_NORMAL (PLAY, 'A',   -1, NULL 						, NULL),

#define S_PLAY_RUN (S_PLAY+1)
	S_NORMAL (PLAY, 'A',	4, NULL 						, &States[S_PLAY_RUN+1]),
	S_NORMAL (PLAY, 'B',	4, NULL 						, &States[S_PLAY_RUN+2]),
	S_NORMAL (PLAY, 'C',	4, NULL 						, &States[S_PLAY_RUN+3]),
	S_NORMAL (PLAY, 'D',	4, NULL 						, &States[S_PLAY_RUN+0]),

#define S_PLAY_ATK (S_PLAY_RUN+4)
	S_NORMAL (PLAY, 'E',   12, NULL 						, &States[S_PLAY]),
	S_BRIGHT (PLAY, 'F',	6, NULL 						, &States[S_PLAY_ATK+0]),

#define S_PLAY_PAIN (S_PLAY_ATK+2)
	S_NORMAL (PLAY, 'G',	4, NULL 						, &States[S_PLAY_PAIN+1]),
	S_NORMAL (PLAY, 'G',	4, A_Pain						, &States[S_PLAY]),

#define S_PLAY_DIE (S_PLAY_PAIN+2)
	S_NORMAL (PLAY, 'H',   10, A_DoomSkinCheck1				, &States[S_PLAY_DIE+1]),
	S_NORMAL (PLAY, 'I',   10, A_PlayerScream				, &States[S_PLAY_DIE+2]),
	S_NORMAL (PLAY, 'J',   10, A_NoBlocking					, &States[S_PLAY_DIE+3]),
	S_NORMAL (PLAY, 'K',   10, NULL 						, &States[S_PLAY_DIE+4]),
	S_NORMAL (PLAY, 'L',   10, NULL 						, &States[S_PLAY_DIE+5]),
	S_NORMAL (PLAY, 'M',   10, NULL 						, &States[S_PLAY_DIE+6]),
	S_NORMAL (PLAY, 'N',   -1, NULL							, NULL),

#define S_PLAY_XDIE (S_PLAY_DIE+7)
	S_NORMAL (PLAY, 'O',	5, A_DoomSkinCheck2				, &States[S_PLAY_XDIE+1]),
	S_NORMAL (PLAY, 'P',	5, A_XScream					, &States[S_PLAY_XDIE+2]),
	S_NORMAL (PLAY, 'Q',	5, A_NoBlocking					, &States[S_PLAY_XDIE+3]),
	S_NORMAL (PLAY, 'R',	5, NULL 						, &States[S_PLAY_XDIE+4]),
	S_NORMAL (PLAY, 'S',	5, NULL 						, &States[S_PLAY_XDIE+5]),
	S_NORMAL (PLAY, 'T',	5, NULL 						, &States[S_PLAY_XDIE+6]),
	S_NORMAL (PLAY, 'U',	5, NULL 						, &States[S_PLAY_XDIE+7]),
	S_NORMAL (PLAY, 'V',	5, NULL 						, &States[S_PLAY_XDIE+8]),
	S_NORMAL (PLAY, 'W',   -1, NULL							, NULL),

#define S_HTIC_DIE (S_PLAY_XDIE+9)
	S_NORMAL (PLAY, 'H',	6, NULL							, &States[S_HTIC_DIE+1]),
	S_NORMAL (PLAY, 'I',	6, A_PlayerScream				, &States[S_HTIC_DIE+2]),
	S_NORMAL (PLAY, 'J',	6, NULL 						, &States[S_HTIC_DIE+3]),
	S_NORMAL (PLAY, 'K',	6, NULL 						, &States[S_HTIC_DIE+4]),
	S_NORMAL (PLAY, 'L',	6, A_NoBlocking 				, &States[S_HTIC_DIE+5]),
	S_NORMAL (PLAY, 'M',	6, NULL 						, &States[S_HTIC_DIE+6]),
	S_NORMAL (PLAY, 'N',	6, NULL 						, &States[S_HTIC_DIE+7]),
	S_NORMAL (PLAY, 'O',	6, NULL 						, &States[S_HTIC_DIE+8]),
	S_NORMAL (PLAY, 'P',   -1, NULL							, NULL),

#define S_HTIC_XDIE (S_HTIC_DIE+9)
	S_NORMAL (PLAY, 'Q',	5, A_PlayerScream				, &States[S_HTIC_XDIE+1]),
	S_NORMAL (PLAY, 'R',	0, A_NoBlocking					, &States[S_HTIC_XDIE+2]),
	S_NORMAL (PLAY, 'R',	5, A_SkullPop					, &States[S_HTIC_XDIE+3]),
	S_NORMAL (PLAY, 'S',	5, NULL			 				, &States[S_HTIC_XDIE+4]),
	S_NORMAL (PLAY, 'T',	5, NULL 						, &States[S_HTIC_XDIE+5]),
	S_NORMAL (PLAY, 'U',	5, NULL 						, &States[S_HTIC_XDIE+6]),
	S_NORMAL (PLAY, 'V',	5, NULL 						, &States[S_HTIC_XDIE+7]),
	S_NORMAL (PLAY, 'W',	5, NULL 						, &States[S_HTIC_XDIE+8]),
	S_NORMAL (PLAY, 'X',	5, NULL 						, &States[S_HTIC_XDIE+9]),
	S_NORMAL (PLAY, 'Y',   -1, NULL							, NULL),
};

IMPLEMENT_ACTOR (ADoomPlayer, Doom, -1, 0)
	PROP_SpawnHealth (100)
	PROP_RadiusFixed (16)
	PROP_HeightFixed (56)
	PROP_Mass (100)
	PROP_PainChance (255)
	PROP_SpeedFixed (1)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_DROPOFF|MF_PICKUP|MF_NOTDMATCH|MF_FRIENDLY)
	PROP_Flags2 (MF2_SLIDE|MF2_PASSMOBJ|MF2_PUSHWALL|MF2_FLOORCLIP|MF2_WINDTHRUST)
	PROP_Flags3 (MF3_NOBLOCKMONST)

	PROP_SpawnState (S_PLAY)
	PROP_SeeState (S_PLAY_RUN)
	PROP_PainState (S_PLAY_PAIN)
	PROP_MissileState (S_PLAY_ATK)
	PROP_MeleeState (S_PLAY_ATK+1)
	PROP_DeathState (S_PLAY_DIE)
	PROP_XDeathState (S_PLAY_XDIE)

	// [GRB]
	PROP_PlayerPawn_ColorRange (112, 127)
	PROP_PlayerPawn_DisplayName ("Marine")
	PROP_PlayerPawn_CrouchSprite ("PLYC")
END_DEFAULTS

void ADoomPlayer::GiveDefaultInventory ()
{
	AInventory *fist, *pistol, *bullets;
	ULONG						ulIdx;
	const PClass				*pType;
	AWeapon						*pWeapon;
	APowerStrength				*pBerserk;
	AWeapon						*pPendingWeapon;
	AInventory					*pInventory;

	Super::GiveDefaultInventory ();

	// [BC] In instagib mode, give the player the railgun, and the maximum amount of cells
	// possible.
	if (( instagib ) && ( deathmatch || teamgame ))
	{
		// Give the player the weapon.
		pInventory = player->mo->GiveInventoryType( PClass::FindClass( "Railgun" ));

		// Make the weapon the player's ready weapon.
		player->ReadyWeapon = player->PendingWeapon = static_cast<AWeapon *>( pInventory );

		// [BC] If we're a client, tell the server we're switching weapons.
		if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) && (( player - players ) == consoleplayer ))
		{
			CLIENTCOMMANDS_WeaponSelect( (char *)pInventory->GetClass( )->TypeName.GetChars( ));

			if ( CLIENTDEMO_IsRecording( ))
				CLIENTDEMO_WriteLocalCommand( CLD_INVUSE, (char *)pInventory->GetClass( )->TypeName.GetChars( ));
		}

		// Find the player's ammo for the weapon in his inventory, and max. out the amount.
		pInventory = player->mo->FindInventory( PClass::FindClass( "CellAmmo" ));
		if ( pInventory != NULL )
			pInventory->Amount = pInventory->MaxAmount;
	}
	// [BC] In buckshot mode, give the player the SSG, and the maximum amount of shells
	// possible.
	else if (( buckshot ) && ( deathmatch || teamgame ))
	{
		// Give the player the weapon.
		pInventory = player->mo->GiveInventoryType( PClass::FindClass( "SuperShotgun" ));

		// Make the weapon the player's ready weapon.
		player->ReadyWeapon = player->PendingWeapon = static_cast<AWeapon *>( pInventory );

		// Find the player's ammo for the weapon in his inventory, and max. out the amount.
		pInventory = player->mo->FindInventory( PClass::FindClass( "ShellAmmo" ));
		if ( pInventory != NULL )
			pInventory->Amount = pInventory->MaxAmount;

		// [BC] If we're a client, tell the server we're switching weapons.
		if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) && (( player - players ) == consoleplayer ))
		{
			CLIENTCOMMANDS_WeaponSelect( (char *)pInventory->GetClass( )->TypeName.GetChars( ));

			if ( CLIENTDEMO_IsRecording( ))
				CLIENTDEMO_WriteLocalCommand( CLD_INVUSE, (char *)pInventory->GetClass( )->TypeName.GetChars( ));
		}
	}
	// [BC] Give a bunch of weapons in LMS mode, depending on the LMS allowed weapon flags.
	else if ( lastmanstanding || teamlms )
	{
		// Give the player all the weapons, and the maximum amount of every type of
		// ammunition.
		pPendingWeapon = NULL;
		for ( ulIdx = 0; ulIdx < PClass::m_Types.Size( ); ulIdx++ )
		{
			pType = PClass::m_Types[ulIdx];

			// Potentially disallow certain weapons.
			if ((( lmsallowedweapons & LMS_AWF_CHAINSAW ) == false ) &&
				( pType == PClass::FindClass( "Chainsaw" )))
			{
				continue;
			}
			if ((( lmsallowedweapons & LMS_AWF_PISTOL ) == false ) &&
				( pType == PClass::FindClass( "Pistol" )))
			{
				continue;
			}
			if ((( lmsallowedweapons & LMS_AWF_SHOTGUN ) == false ) &&
				( pType == PClass::FindClass( "Shotgun" )))
			{
				continue;
			}
			if (( pType == PClass::FindClass( "SuperShotgun" )) &&
				((( lmsallowedweapons & LMS_AWF_SSG ) == false ) ||
				(( gameinfo.flags & GI_MAPxx ) == false )))
			{
				continue;
			}
			if ((( lmsallowedweapons & LMS_AWF_CHAINGUN ) == false ) &&
				( pType == PClass::FindClass( "Chaingun" )))
			{
				continue;
			}
			if ((( lmsallowedweapons & LMS_AWF_MINIGUN ) == false ) &&
				( pType == PClass::FindClass( "Minigun" )))
			{
				continue;
			}
			if ((( lmsallowedweapons & LMS_AWF_ROCKETLAUNCHER ) == false ) &&
				( pType == PClass::FindClass( "RocketLauncher" )))
			{
				continue;
			}
			if ((( lmsallowedweapons & LMS_AWF_GRENADELAUNCHER ) == false ) &&
				( pType == PClass::FindClass( "GrenadeLauncher" )))
			{
				continue;
			}
			if ((( lmsallowedweapons & LMS_AWF_PLASMA ) == false ) &&
				( pType == PClass::FindClass( "PlasmaRifle" )))
			{
				continue;
			}
			if ((( lmsallowedweapons & LMS_AWF_RAILGUN ) == false ) &&
				( pType == PClass::FindClass( "Railgun" )))
			{
				continue;
			}

			if ( pType->ParentClass->IsDescendantOf( RUNTIME_CLASS( AWeapon )))
			{
				pInventory = player->mo->GiveInventoryType( pType );

				// Make this weapon the player's pending weapon if it ranks higher.
				pWeapon = static_cast<AWeapon *>( pInventory );
				if ( pWeapon != NULL )
				{
					if ( pWeapon->WeaponFlags & WIF_NOLMS )
					{
						player->mo->RemoveInventory( pWeapon );
						continue;
					}

					if (( pPendingWeapon == NULL ) || 
						( pWeapon->SelectionOrder < pPendingWeapon->SelectionOrder ))
					{
						pPendingWeapon = static_cast<AWeapon *>( pInventory );
					}

					if ( pWeapon->Ammo1 )
					{
						pInventory = player->mo->FindInventory( pWeapon->Ammo1->GetClass( ));

						// Give the player the maximum amount of this type of ammunition.
						if ( pInventory != NULL )
							pInventory->Amount = pInventory->MaxAmount;
					}
					if ( pWeapon->Ammo2 )
					{
						pInventory = player->mo->FindInventory( pWeapon->Ammo2->GetClass( ));

						// Give the player the maximum amount of this type of ammunition.
						if ( pInventory != NULL )
							pInventory->Amount = pInventory->MaxAmount;
					}
				}
			}
		}

		// Also give the player berserk.
		player->mo->GiveInventoryType( PClass::FindClass( "Berserk" ));
		pBerserk = static_cast<APowerStrength *>( player->mo->FindInventory( PClass::FindClass( "PowerStrength" )));
		if ( pBerserk )
			pBerserk->EffectTics = 768;

		player->health = deh.MegasphereHealth;
		player->mo->GiveInventoryType( PClass::FindClass( "GreenArmor" ));
		player->health -= player->userinfo.lHandicap;

		// Don't allow player to be DOA.
		if ( player->health <= 0 )
			player->health = 1;

		// Finally, set the ready and pending weapon.
		player->ReadyWeapon = player->PendingWeapon = pPendingWeapon;

		// [BC] If we're a client, tell the server we're switching weapons.
		if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) && (( player - players ) == consoleplayer ))
		{
			CLIENTCOMMANDS_WeaponSelect( (char *)pPendingWeapon->GetClass( )->TypeName.GetChars( ));

			if ( CLIENTDEMO_IsRecording( ))
				CLIENTDEMO_WriteLocalCommand( CLD_INVUSE, (char *)pPendingWeapon->GetClass( )->TypeName.GetChars( ));
		}
	}
	else if (!Inventory)
	{
		fist = player->mo->GiveInventoryType (PClass::FindClass ("Fist"));
		pistol = player->mo->GiveInventoryType (PClass::FindClass ("Pistol"));
		// Adding the pistol automatically adds bullets
		bullets = player->mo->FindInventory (PClass::FindClass ("Clip"));
		if (bullets != NULL)
		{
			bullets->Amount = deh.StartBullets;		// [RH] Used to be 50
		}
		player->ReadyWeapon = player->PendingWeapon =
			static_cast<AWeapon *> (deh.StartBullets > 0 ? pistol : fist);

		// [BC] If the user has the shotgun start flag set, do that!
		if (( dmflags2 & DF2_COOP_SHOTGUNSTART ) &&
			( deathmatch == false ) &&
			( teamgame == false ))
		{
			pInventory = player->mo->GiveInventoryType( PClass::FindClass( "Shotgun" ));
			if ( pInventory )
			{
				player->ReadyWeapon = player->PendingWeapon = static_cast<AWeapon *>( pInventory );

				// Start them off with two clips.
				pInventory = player->mo->FindInventory( PClass::FindClass( "Shell" ));
				if ( pInventory != NULL )
					pInventory->Amount = static_cast<AWeapon *>( player->ReadyWeapon )->AmmoGive1 * 2;
			}
		}

		// [BC] If we're a client, tell the server we're switching weapons.
		if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) && (( player - players ) == consoleplayer ))
		{
			CLIENTCOMMANDS_WeaponSelect( (char *)player->ReadyWeapon->GetClass( )->TypeName.GetChars( ));

			if ( CLIENTDEMO_IsRecording( ))
				CLIENTDEMO_WriteLocalCommand( CLD_INVUSE, (char *)player->ReadyWeapon->GetClass( )->TypeName.GetChars( ));
		}
	}
}

void A_FireScream (AActor *self)
{
	S_Sound (self, CHAN_BODY, "*burndeath", 1, ATTN_NORM);
}

void A_PlayerScream (AActor *self)
{
	int sound = 0;
	int chan = CHAN_VOICE;

	if (self->player == NULL || self->DeathSound != 0)
	{
		S_SoundID (self, CHAN_VOICE, self->DeathSound, 1, ATTN_NORM);
		return;
	}

	// Handle the different player death screams
	if ((((level.flags >> 15) | (dmflags)) &
		(DF_FORCE_FALLINGZD | DF_FORCE_FALLINGHX)) &&
		self->momz <= -39*FRACUNIT)
	{
		sound = S_FindSkinnedSound (self, "*splat");
		chan = CHAN_BODY;
	}

	// try to find the appropriate death sound and use suitable replacements if necessary
	if (!sound && self->special1<10)
	{ // Wimpy death sound
		sound = S_FindSkinnedSound (self, "*wimpydeath");
	}
	if (!sound && self->health <= -50)
	{
		if (self->health > -100)
		{ // Crazy death sound
			sound = S_FindSkinnedSound (self, "*crazydeath");
		}
		if (!sound)
		{ // Extreme death sound
			sound = S_FindSkinnedSound (self, "*xdeath");
			if (!sound)
			{
				sound = S_FindSkinnedSound (self, "*gibbed");
				chan = CHAN_BODY;
			}
		}
	}
	if (!sound)
	{ // Normal death sound
		sound=S_FindSkinnedSound (self, "*death");
	}

	if (chan != CHAN_VOICE)
	{
		for (int i = 0; i < 8; ++i)
		{ // Stop most playing sounds from this player.
		  // This is mainly to stop *land from messing up *splat.
			if (i != CHAN_WEAPON && i != CHAN_VOICE)
			{
				S_StopSound (self, i);
			}
		}
	}
	S_SoundID (self, chan, sound, 1, ATTN_NORM);
}


//==========================================================================
//
// A_DoomSkinCheck1
//
//==========================================================================

void A_DoomSkinCheck1 (AActor *actor)
{
	if (actor->player != NULL &&
		skins[actor->player->userinfo.skin].othergame)
	{
		actor->SetState (&ADoomPlayer::States[S_HTIC_DIE]);
	}
}

//==========================================================================
//
// A_DoomSkinCheck2
//
//==========================================================================

void A_DoomSkinCheck2 (AActor *actor)
{
	if (actor->player != NULL &&
		skins[actor->player->userinfo.skin].othergame)
	{
		actor->SetState (&ADoomPlayer::States[S_HTIC_XDIE]);
	}
}
