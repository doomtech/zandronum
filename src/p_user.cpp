// Emacs style mode select	 -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// $Log:$
//
// DESCRIPTION:
//		Player related stuff.
//		Bobbing POV/weapon, movement.
//		Pending weapon.
//
//-----------------------------------------------------------------------------

#include "templates.h"
#include "doomdef.h"
#include "d_event.h"
#include "p_local.h"
#include "doomstat.h"
#include "s_sound.h"
#include "i_system.h"
#include "r_draw.h"
#include "gi.h"
#include "m_random.h"
#include "p_pspr.h"
#include "p_enemy.h"
#include "s_sound.h"
#include "a_sharedglobal.h"
#include "a_keys.h"
#include "statnums.h"
#include "v_palette.h"
#include "v_video.h"
#include "w_wad.h"
#include "cmdlib.h"
#include "sbar.h"
#include "f_finale.h"
#include "c_console.h"
#include "doomdef.h"
#include "c_dispatch.h"
#include "tarray.h"
#include "thingdef/thingdef.h"
#include "g_level.h"
#include "d_net.h"
// [BB] New #includes.
#include "sv_commands.h"
#include "a_doomglobal.h"
#include "deathmatch.h"
#include "duel.h"
#include "g_game.h"
#include "team.h"
#include "network.h"
#include "joinqueue.h"
#include "m_menu.h"
#include "d_gui.h"
#include "lastmanstanding.h"
#include "cooperative.h"
#include "survival.h"
#include "sv_main.h"
#include "cl_demo.h"
#include "cl_main.h"
#include "scoreboard.h"
#include "p_acs.h"
#include "possession.h"
#include "cl_commands.h"
#include "gamemode.h"
#include "invasion.h"

// [BB] Use ZDoom's freelook limit for the sotfware renderer.
// Note: ZDoom's limit is chosen such that the sky is rendered properly.
CVAR (Bool, cl_oldfreelooklimit, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

static FRandom pr_skullpop ("SkullPop");


// [RH] # of ticks to complete a turn180
#define TURN180_TICKS	((TICRATE / 4) + 1)

// Variables for prediction
CVAR (Bool, cl_noprediction, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
static player_t PredictionPlayerBackup;
static BYTE PredictionActorBackup[sizeof(AActor)];
static TArray<sector_t *> PredictionTouchingSectorsBackup;

// [Dusk] Determine speed spectators will move at
CUSTOM_CVAR (Float, cl_spectatormove, 1.0, CVAR_ARCHIVE|CVAR_GLOBALCONFIG) {
	if (self > 100.0)
		self = 100.0;
	else if (self < -100.0)
		self = -100.0;
}

// [GRB] Custom player classes
TArray<FPlayerClass> PlayerClasses;

FPlayerClass::FPlayerClass ()
{
	Type = NULL;
	Flags = 0;
}

FPlayerClass::FPlayerClass (const FPlayerClass &other)
{
	Type = other.Type;
	Flags = other.Flags;
	Skins = other.Skins;
}

FPlayerClass::~FPlayerClass ()
{
}

bool FPlayerClass::CheckSkin (int skin)
{
	for (unsigned int i = 0; i < Skins.Size (); i++)
	{
		if (Skins[i] == skin)
			return true;
	}

	return false;
}

void SetupPlayerClasses ()
{
	FPlayerClass newclass;

	newclass.Flags = 0;

	if (gameinfo.gametype == GAME_Doom)
	{
		newclass.Type = PClass::FindClass (NAME_DoomPlayer);
		PlayerClasses.Push (newclass);
	}
	else if (gameinfo.gametype == GAME_Heretic)
	{
		newclass.Type = PClass::FindClass (NAME_HereticPlayer);
		PlayerClasses.Push (newclass);
	}
	else if (gameinfo.gametype == GAME_Hexen)
	{
		newclass.Type = PClass::FindClass (NAME_FighterPlayer);
		PlayerClasses.Push (newclass);
		newclass.Type = PClass::FindClass (NAME_ClericPlayer);
		PlayerClasses.Push (newclass);
		newclass.Type = PClass::FindClass (NAME_MagePlayer);
		PlayerClasses.Push (newclass);
	}
	else if (gameinfo.gametype == GAME_Strife)
	{
		newclass.Type = PClass::FindClass (NAME_StrifePlayer);
		PlayerClasses.Push (newclass);
	}
	else if (gameinfo.gametype == GAME_Chex)
	{
		newclass.Type = PClass::FindClass (NAME_ChexPlayer);
		PlayerClasses.Push (newclass);
	}
}

CCMD (clearplayerclasses)
{
	if (ParsingKeyConf)
	{
		PlayerClasses.Clear ();
	}
}

CCMD (addplayerclass)
{
	if (ParsingKeyConf && argv.argc () > 1)
	{
		const PClass *ti = PClass::FindClass (argv[1]);

		if (!ti)
		{
			Printf ("Unknown player class '%s'\n", argv[1]);
		}
		else if (!ti->IsDescendantOf (RUNTIME_CLASS (APlayerPawn)))
		{
			Printf ("Invalid player class '%s'\n", argv[1]);
		}
		else if (ti->Meta.GetMetaString (APMETA_DisplayName) == NULL)
		{
			Printf ("Missing displayname for player class '%s'\n", argv[1]);
		}
		else
		{
			FPlayerClass newclass;

			newclass.Type = ti;
			newclass.Flags = 0;

			int arg = 2;
			while (arg < argv.argc ())
			{
				if (!stricmp (argv[arg], "nomenu"))
				{
					newclass.Flags |= PCF_NOMENU;
				}
				else
				{
					Printf ("Unknown flag '%s' for player class '%s'\n", argv[arg], argv[1]);
				}

				arg++;
			}

			PlayerClasses.Push (newclass);
		}
	}
}

CCMD (playerclasses)
{
	for (unsigned int i = 0; i < PlayerClasses.Size (); i++)
	{
		Printf ("% 3d %s\n", i,
			PlayerClasses[i].Type->Meta.GetMetaString (APMETA_DisplayName));
	}
}


//
// Movement.
//

// 16 pixels of bob
#define MAXBOB			0x100000

bool onground;

// The player_t constructor. Since LogText is not a POD, we cannot just
// memset it all to 0.
player_t::player_t()
: mo(0),
  playerstate(0),
  cls(0),
  DesiredFOV(0),
  FOV(0),
  viewz(0),
  viewheight(0),
  deltaviewheight(0),
  bob(0),
  momx(0),
  momy(0),
  centering(0),
  turnticks(0),
  attackdown(0),
  oldbuttons(0),
  health(0),
  inventorytics(0),
  CurrentPlayerClass(0),
  backpack(0),
  fragcount(0),
  ReadyWeapon(0),
  PendingWeapon(0),
  cheats(0),
  cheats2(0), // [BB]
  refire(0),
  killcount(0),
  itemcount(0),
  secretcount(0),
  damagecount(0),
  bonuscount(0),
  hazardcount(0),
  poisoncount(0),
  poisoner(0),
  attacker(0),
  extralight(0),
  morphTics(0),
  MorphedPlayerClass(0),
  MorphStyle(0),
  MorphExitFlash(0),
  PremorphWeapon(0),
  chickenPeck(0),
  jumpTics(0),
  respawn_time(0),
  camera(0),
  air_finished(0),
  accuracy(0),
  stamina(0),
  BlendR(0),
  BlendG(0),
  BlendB(0),
  BlendA(0),
  LogText(),
  crouching(0),
  crouchdir(0),
  crouchfactor(0),
  crouchoffset(0),
  crouchviewdelta(0),
  ConversationNPC(0),
  ConversationPC(0),
  ConversationNPCAngle(0),
  ConversationFaceTalker(0),
  // [BC] Initialize ST's additional properties.
  bOnTeam( 0 ),
  ulTeam( 0 ),
  lPointCount( 0 ),
  ulDeathCount( 0 ),
  ulLastFragTick( 0 ),
  ulLastExcellentTick( 0 ),
  ulLastBFGFragTick( 0 ),
  ulConsecutiveHits( 0 ),
  ulConsecutiveRailgunHits( 0 ),
  ulFragsWithoutDeath( 0 ),
  ulDeathsWithoutFrag( 0 ),
  ulUnrewardedDamageDealt( 0 ),
  bChatting( 0 ),
  bInConsole( 0 ),
  bSpectating( 0 ),
  bDeadSpectator( 0 ),
  ulLivesLeft( 0 ),
  bStruckPlayer( 0 ),
  ulRailgunShots( 0 ),
  pIcon( 0 ),
  lMaxHealthBonus( 0 ),
  ulWins( 0 ),
  pSkullBot( 0 ),
  bIsBot( 0 ),
  bIgnoreChat( 0 ),
  lIgnoreChatTicks( -1 ),
  ulPing( 0 ),
  ulPingAverages( 0 ),
  bReadyToGoOn( 0 ),
  bSpawnOkay( 0 ),
  SpawnX( 0 ),
  SpawnY( 0 ),
  SpawnAngle( 0 ),
  OldPendingWeapon( 0 ),
  bLagging( 0 ),
  bSpawnTelefragged( 0 ),
  ulTime( 0 ),
  bUnarmed( false )
{
	memset (&cmd, 0, sizeof(cmd));
	memset (&userinfo, 0, sizeof(userinfo));
	memset (psprites, 0, sizeof(psprites));

	// [BC] Initialize additonal ST properties.
	memset( &ulMedalCount, 0, sizeof( ULONG ) * NUM_MEDALS );
	memset( &ServerXYZ, 0, sizeof( fixed_t ) * 3 );
	memset( &ServerXYZMom, 0, sizeof( fixed_t ) * 3 );
}

// This function supplements the pointer cleanup in dobject.cpp, because
// player_t is not derived from DObject. (I tried it, and DestroyScan was
// unable to properly determine the player object's type--possibly
// because it gets staticly allocated in an array.)
//
// This function checks all the DObject pointers in a player_t and NULLs any
// that match the pointer passed in. If you add any pointers that point to
// DObject (or a subclass), add them here too.

size_t player_t::FixPointers (const DObject *old, DObject *rep)
{
	APlayerPawn *replacement = static_cast<APlayerPawn *>(rep);
	size_t changed = 0;
	if (mo == old)				mo = replacement, changed++;
	if (poisoner == old)		poisoner = replacement, changed++;
	if (attacker == old)		attacker = replacement, changed++;
	if (camera == old)			camera = replacement, changed++;
	/* [BB] ST doesn't have these.
	if (dest == old)			dest = replacement, changed++;
	if (prev == old)			prev = replacement, changed++;
	if (enemy == old)			enemy = replacement, changed++;
	if (missile == old)			missile = replacement, changed++;
	if (mate == old)			mate = replacement, changed++;
	if (last_mate == old)		last_mate = replacement, changed++;
	*/
	if (ReadyWeapon == old)		ReadyWeapon = static_cast<AWeapon *>(rep), changed++;
	if (PendingWeapon == old)	PendingWeapon = static_cast<AWeapon *>(rep), changed++;
	if (PremorphWeapon == old)	PremorphWeapon = static_cast<AWeapon *>(rep), changed++;
	if (ConversationNPC == old)	ConversationNPC = replacement, changed++;
	if (ConversationPC == old)	ConversationPC = replacement, changed++;
	// [BC]
	if ( pIcon == old )		pIcon = static_cast<AFloatyIcon *>( rep ), changed++;
	if ( OldPendingWeapon == old )		OldPendingWeapon = static_cast<AWeapon *>( rep ), changed++;

	return changed;
}

size_t player_t::PropagateMark()
{
	GC::Mark(mo);
	GC::Mark(poisoner);
	GC::Mark(attacker);
	GC::Mark(camera);
	/* [BB] ST doesn't have these.
	GC::Mark(dest);
	GC::Mark(prev);
	GC::Mark(enemy);
	GC::Mark(missile);
	GC::Mark(mate);
	GC::Mark(last_mate);
	*/
	GC::Mark(ReadyWeapon);
	GC::Mark(ConversationNPC);
	GC::Mark(ConversationPC);
	GC::Mark(PremorphWeapon);
	if (PendingWeapon != WP_NOCHANGE)
	{
		GC::Mark(PendingWeapon);
	}
	// [BB]
	GC::Mark(pIcon);
	if (OldPendingWeapon != WP_NOCHANGE)
		GC::Mark(OldPendingWeapon);
	return sizeof(*this);
}

void player_t::SetLogNumber (int num)
{
	char lumpname[16];
	int lumpnum;

	mysnprintf (lumpname, countof(lumpname), "LOG%d", num);
	lumpnum = Wads.CheckNumForName (lumpname);
	if (lumpnum == -1)
	{
		// Leave the log message alone if this one doesn't exist.
		//SetLogText (lumpname);
	}
	else
	{
		int length=Wads.LumpLength(lumpnum);
		char *data= new char[length+1];
		Wads.ReadLump (lumpnum, data);
		data[length]=0;
		SetLogText (data);
		delete[] data;

		// Print log text to console
		AddToConsole(-1, TEXTCOLOR_GOLD);
		AddToConsole(-1, LogText);
		AddToConsole(-1, "\n");
	}
}

void player_t::SetLogText (const char *text)
{
	LogText = text;
}

int player_t::GetSpawnClass()
{
	const PClass * type = PlayerClasses[CurrentPlayerClass].Type;
	return static_cast<APlayerPawn*>(GetDefaultByType(type))->SpawnMask;
}

//===========================================================================
//
// APlayerPawn
//
//===========================================================================

IMPLEMENT_POINTY_CLASS (APlayerPawn)
 DECLARE_POINTER(InvFirst)
 DECLARE_POINTER(InvSel)
END_POINTERS

IMPLEMENT_CLASS (APlayerChunk)

void APlayerPawn::Serialize (FArchive &arc)
{
	Super::Serialize (arc);

	arc << JumpZ
		<< MaxHealth
		<< RunHealth
		<< SpawnMask
		<< ForwardMove1
		<< ForwardMove2
		<< SideMove1
		<< SideMove2
		<< ScoreIcon
		<< InvFirst
		<< InvSel
		<< MorphWeapon
		<< RedDamageFade
		<< GreenDamageFade
		<< BlueDamageFade;
}

//===========================================================================
//
// APlayerPawn :: BeginPlay
//
//===========================================================================

void APlayerPawn::BeginPlay ()
{
	Super::BeginPlay ();
	ChangeStatNum (STAT_PLAYER);

	// Check whether a PWADs normal sprite is to be combined with the base WADs
	// crouch sprite. In such a case the sprites normally don't match and it is
	// best to disable the crouch sprite.
	if (crouchsprite > 0)
	{
		// This assumes that player sprites always exist in rotated form and
		// that the front view is always a separate sprite. So far this is
		// true for anything that exists.
		FString normspritename = sprites[SpawnState->sprite].name;
		FString crouchspritename = sprites[crouchsprite].name;

		int spritenorm = Wads.CheckNumForName(normspritename + "A1", ns_sprites);
		int spritecrouch = Wads.CheckNumForName(crouchspritename + "A1", ns_sprites);
		
		if (spritenorm==-1 || spritecrouch ==-1) 
		{
			// Sprites do not exist so it is best to disable the crouch sprite.
			crouchsprite = 0;
			return;
		}
	
		int wadnorm = Wads.GetLumpFile(spritenorm);
		int wadcrouch = Wads.GetLumpFile(spritenorm);
		
		if (wadnorm > FWadCollection::IWAD_FILENUM && wadcrouch <= FWadCollection::IWAD_FILENUM) 
		{
			// Question: Add an option / disable crouching or do what?
			crouchsprite = 0;
		}
	}
}

//===========================================================================
//
// APlayerPawn :: Tick
//
//===========================================================================

void APlayerPawn::Tick()
{
	if (player != NULL && player->mo == this && player->morphTics == 0 && player->playerstate != PST_DEAD)
	{
		// [BC] Make the player flat, so he can travel under doors and such.
		if ( player->bSpectating )
			height = 0;
		else
			height = FixedMul(GetDefault()->height, player->crouchfactor);
	}
	else
	{
		if (health > 0) height = GetDefault()->height;
	}
	Super::Tick();
}

//===========================================================================
//
// APlayerPawn :: PostBeginPlay
//
//===========================================================================

void APlayerPawn::PostBeginPlay()
{
	SetupWeaponSlots();
}

//===========================================================================
//
// APlayerPawn :: SetupWeaponSlots
//
// Sets up the default weapon slots for this player. If this is also the
// local player, determines local modifications and sends those across the
// network. Ignores voodoo dolls.
//
//===========================================================================

void APlayerPawn::SetupWeaponSlots()
{
	if (player != NULL && player->mo == this)
	{
		player->weapons.StandardSetup(GetClass());
		// [BB] Since ST doesn't use the DEM_* stuff let clients do this
		// for all players. Otherwise the KEYCONF lump would be completely ignored
		// on clients for all players other than the consoleplayer.
		if ( (player - players == consoleplayer) || NETWORK_InClientMode() )
		{ // If we're the local player, then there's a bit more work to do.
			FWeaponSlots local_slots(player->weapons);
			local_slots.LocalSetup(GetClass());
			// [BB] SendDifferences uses DEM_* stuff, that ST never uses in
			// multiplayer games, so we have to handle this differently.
			if ( NETWORK_GetState( ) != NETSTATE_CLIENT )
				local_slots.SendDifferences(player->weapons);
			else
				player->weapons = local_slots;
		}
	}
}

//===========================================================================
//
// APlayerPawn :: AddInventory
//
//===========================================================================

void APlayerPawn::AddInventory (AInventory *item)
{
	// Adding inventory to a voodoo doll should add it to the real player instead.
	if (player != NULL && player->mo != this && player->mo != NULL)
	{
		player->mo->AddInventory (item);
		return;
	}
	Super::AddInventory (item);

	// If nothing is selected, select this item.
	if (InvSel == NULL && (item->ItemFlags & IF_INVBAR))
	{
		InvSel = item;
	}
}

//===========================================================================
//
// APlayerPawn :: RemoveInventory
//
//===========================================================================

void APlayerPawn::RemoveInventory (AInventory *item)
{
	bool pickWeap = false;

	// Since voodoo dolls aren't supposed to have an inventory, there should be
	// no need to redirect them to the real player here as there is with AddInventory.

	// If the item removed is the selected one, select something else, either the next
	// item, if there is one, or the previous item.
	if (player != NULL)
	{
		if (InvSel == item)
		{
			InvSel = item->NextInv ();
			if (InvSel == NULL)
			{
				InvSel = item->PrevInv ();
			}
		}
		if (InvFirst == item)
		{
			InvFirst = item->NextInv ();
			if (InvFirst == NULL)
			{
				InvFirst = item->PrevInv ();
			}
		}
		if (item == player->PendingWeapon)
		{
			player->PendingWeapon = WP_NOCHANGE;
		}
		if (item == player->ReadyWeapon)
		{
			// If the current weapon is removed, clear the refire counter and pick a new one.
			// [BC] Don't pick a new weapon if the owner is dead.
			if (( item->Owner ) && ( item->Owner->health <= 0 ))
				pickWeap = false;
			else
				pickWeap = true;
			player->ReadyWeapon = NULL;
			player->refire = 0;
		}
	}
	Super::RemoveInventory (item);
	if (pickWeap && player->mo == this && player->PendingWeapon == WP_NOCHANGE)
	{
		PickNewWeapon (NULL);
	}
}

//===========================================================================
//
// APlayerPawn :: UseInventory
//
//===========================================================================

bool APlayerPawn::UseInventory (AInventory *item)
{
	// [BB] Morphing clears the unmorphed actor's "player" field, so remember which player number we are.
	const ULONG ulPlayer = static_cast<ULONG>( this->player - players );

	const PClass *itemtype = item->GetClass();

	if (player->cheats & CF_TOTALLYFROZEN)
	{ // You can't use items if you're totally frozen
		return false;
	}
	if (!Super::UseInventory (item))
	{
		// [BB] The server won't call SERVERCOMMANDS_PlayerUseInventory in this case, so we have to 
		// notify the clients when a weapon change happened because of the use. Probably it would be better 
		// to do this in AWeapon::Use, but it's not obvious how to prevent the server from doing it there
		// when the client calls AWeapon::Use for other reasons than APlayerPawn::UseInventory. 
		if ( ( NETWORK_GetState( ) == NETSTATE_SERVER ) && item->Owner && item->Owner->player && item->IsKindOf ( RUNTIME_CLASS(AWeapon) ) )
		{
			const player_t *pPlayer = item->Owner->player;
			const ULONG ulPlayer = static_cast<ULONG> ( pPlayer - players );
			if ( ( pPlayer->PendingWeapon == item ) && ( pPlayer->ReadyWeapon != item ) )
				SERVERCOMMANDS_SetPlayerPendingWeapon( ulPlayer );
		}

		// Heretic and Hexen advance the inventory cursor if the use failed.
		// Should this behavior be retained?
		return false;
	}
	// [BB] The server only has to tell the client, that he successfully used
	// the item. The sound and the status bar flashing are handled by the client.
	if( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		// [BB] Use the player number we stored above.
		SERVERCOMMANDS_PlayerUseInventory( ulPlayer, item );
	}
	else if (player == &players[consoleplayer])
	{
		S_Sound (this, CHAN_ITEM, item->UseSound, 1, ATTN_NORM);
		StatusBar->FlashItem (itemtype);
	}
	return true;
}

//===========================================================================
//
// APlayerPawn :: BestWeapon
//
// Returns the best weapon a player has, possibly restricted to a single
// type of ammo.
//
//===========================================================================

AWeapon *APlayerPawn::BestWeapon (const PClass *ammotype)
{
	AWeapon *bestMatch = NULL;
	int bestOrder = INT_MAX;
	AInventory *item;
	AWeapon *weap;
	bool tomed = NULL != FindInventory (RUNTIME_CLASS(APowerWeaponLevel2));

	// Find the best weapon the player has.
	for (item = Inventory; item != NULL; item = item->Inventory)
	{
		if (!item->IsKindOf (RUNTIME_CLASS(AWeapon)))
			continue;

		weap = static_cast<AWeapon *> (item);

		// Don't select it if it's worse than what was already found.
		if (weap->SelectionOrder > bestOrder)
			continue;

		// Don't select it if its primary fire doesn't use the desired ammo.
		if (ammotype != NULL &&
			(weap->Ammo1 == NULL ||
			 weap->Ammo1->GetClass() != ammotype))
			continue;

		// Don't select it if the Tome is active and this isn't the powered-up version.
		if (tomed && weap->SisterWeapon != NULL && weap->SisterWeapon->WeaponFlags & WIF_POWERED_UP)
			continue;

		// Don't select it if it's powered-up and the Tome is not active.
		if (!tomed && weap->WeaponFlags & WIF_POWERED_UP)
			continue;

		// Don't select it if there isn't enough ammo to use its primary fire.
		if (!(weap->WeaponFlags & WIF_AMMO_OPTIONAL) &&
			!weap->CheckAmmo (AWeapon::PrimaryFire, false))
			continue;

		// This weapon is usable!
		bestOrder = weap->SelectionOrder;
		bestMatch = weap;
	}
	return bestMatch;
}

//===========================================================================
//
// APlayerPawn :: PickNewWeapon
//
// Picks a new weapon for this player. Used mostly for running out of ammo,
// but it also works when an ACS script explicitly takes the ready weapon
// away or the player picks up some ammo they had previously run out of.
//
//===========================================================================

AWeapon *APlayerPawn::PickNewWeapon (const PClass *ammotype)
{
	AWeapon *best = BestWeapon (ammotype);

	if (best != NULL)
	{
		player->PendingWeapon = best;
		if (player->ReadyWeapon != NULL)
		{
			P_SetPsprite (player, ps_weapon, player->ReadyWeapon->GetDownState());
		}
		else if (player->PendingWeapon != WP_NOCHANGE)
		{
			P_BringUpWeapon (player);
		}

		// [BC] In client mode, tell the server which weapon we're using.
		if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) && ( player - players == consoleplayer ))
		{
			CLIENTCOMMANDS_WeaponSelect( best->GetClass( ));

			if (( CLIENTDEMO_IsRecording( )) &&
				( CLIENT_IsParsingPacket( ) == false ))
			{
				CLIENTDEMO_WriteLocalCommand( CLD_LCMD_INVUSE, best->GetClass( )->TypeName.GetChars( ) );
			}
		}
		// [BB] The server needs to tell the clients about bot weapon changes.
		else if ( ( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( player->bIsBot == true ) )
			SERVERCOMMANDS_SetPlayerPendingWeapon( static_cast<ULONG> ( player - players ) );
	}
	return best;
}


//===========================================================================
//
// APlayerPawn :: CheckWeaponSwitch
//
// Checks if weapons should be changed after picking up ammo
//
//===========================================================================

void APlayerPawn::CheckWeaponSwitch(const PClass *ammotype)
{
	if (( NETWORK_GetState( ) != NETSTATE_SERVER ) && // [BC] Let clients decide if they want to switch weapons.
		( player->userinfo.switchonpickup > 0 ) &&
		player->PendingWeapon == WP_NOCHANGE && 
		(player->ReadyWeapon == NULL ||
		 (player->ReadyWeapon->WeaponFlags & WIF_WIMPY_WEAPON)))
	{
		AWeapon *best = BestWeapon (ammotype);
		if (best != NULL && (player->ReadyWeapon == NULL ||
			best->SelectionOrder < player->ReadyWeapon->SelectionOrder))
		{
			player->PendingWeapon = best;

			// [BC] If we're a client, tell the server we're switching weapons.
			if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) && (( player - players ) == consoleplayer ))
			{
				CLIENTCOMMANDS_WeaponSelect( best->GetClass( ));

				if ( CLIENTDEMO_IsRecording( ))
					CLIENTDEMO_WriteLocalCommand( CLD_LCMD_INVUSE, best->GetClass( )->TypeName.GetChars( ) );
			}
		}
	}
}

//===========================================================================
//
// APlayerPawn :: GiveDeathmatchInventory
//
// Gives players items they should have in addition to their default
// inventory when playing deathmatch. (i.e. all keys)
//
//===========================================================================

void APlayerPawn::GiveDeathmatchInventory()
{
	// [BB] Spectators are supposed to have no inventory.
	if ( player && player->bSpectating ) return;

	for (unsigned int i = 0; i < PClass::m_Types.Size(); ++i)
	{
		if (PClass::m_Types[i]->IsDescendantOf (RUNTIME_CLASS(AKey)))
		{
			AKey *key = (AKey *)GetDefaultByType (PClass::m_Types[i]);
			if (key->KeyNumber != 0)
			{
				key = static_cast<AKey *>(Spawn (PClass::m_Types[i], 0,0,0, NO_REPLACE));
				if (!key->CallTryPickup (this))
				{
					key->Destroy ();
				}
			}
		}
	}
}

//===========================================================================
//
// APlayerPawn :: FilterCoopRespawnInventory
//
// When respawning in coop, this function is called to walk through the dead
// player's inventory and modify it according to the current game flags so
// that it can be transferred to the new live player. This player currently
// has the default inventory, and the oldplayer has the inventory at the time
// of death.
//
//===========================================================================

void APlayerPawn::FilterCoopRespawnInventory (APlayerPawn *oldplayer)
{
	AInventory *item, *next, *defitem;

	// If we're losing everything, this is really simple.
	if (dmflags & DF_COOP_LOSE_INVENTORY)
	{
		oldplayer->DestroyAllInventory();
		return;
	}

	if (dmflags &  (DF_COOP_LOSE_KEYS |
					DF_COOP_LOSE_WEAPONS |
					DF_COOP_LOSE_AMMO |
					DF_COOP_HALVE_AMMO |
					DF_COOP_LOSE_ARMOR |
					DF_COOP_LOSE_POWERUPS))
	{
		// Walk through the old player's inventory and destroy or modify
		// according to dmflags.
		for (item = oldplayer->Inventory; item != NULL; item = next)
		{
			next = item->Inventory;

			// If this item is part of the default inventory, we never want
			// to destroy it, although we might want to copy the default
			// inventory amount.
			defitem = FindInventory (item->GetClass());

			if ((dmflags & DF_COOP_LOSE_KEYS) &&
				defitem == NULL &&
				item->IsKindOf(RUNTIME_CLASS(AKey)))
			{
				item->Destroy();
			}
			else if ((dmflags & DF_COOP_LOSE_WEAPONS) &&
				defitem == NULL &&
				item->IsKindOf(RUNTIME_CLASS(AWeapon)))
			{
				item->Destroy();
			}
			else if ((dmflags & DF_COOP_LOSE_ARMOR) &&
				item->IsKindOf(RUNTIME_CLASS(AArmor)))
			{
				if (defitem != NULL)
				{
					item->Destroy();
				}
				else if (item->IsKindOf(RUNTIME_CLASS(ABasicArmor)))
				{
					static_cast<ABasicArmor*>(item)->SavePercent = static_cast<ABasicArmor*>(defitem)->SavePercent;
					item->Amount = defitem->Amount;
				}
				else if (item->IsKindOf(RUNTIME_CLASS(AHexenArmor)))
				{
					static_cast<AHexenArmor*>(item)->Slots[0] = static_cast<AHexenArmor*>(defitem)->Slots[0];
					static_cast<AHexenArmor*>(item)->Slots[1] = static_cast<AHexenArmor*>(defitem)->Slots[1];
					static_cast<AHexenArmor*>(item)->Slots[2] = static_cast<AHexenArmor*>(defitem)->Slots[2];
					static_cast<AHexenArmor*>(item)->Slots[3] = static_cast<AHexenArmor*>(defitem)->Slots[3];
				}
			}
			else if ((dmflags & DF_COOP_LOSE_POWERUPS) &&
				defitem == NULL &&
				item->IsKindOf(RUNTIME_CLASS(APowerupGiver)))
			{
				item->Destroy();
			}
			else if ((dmflags & (DF_COOP_LOSE_AMMO | DF_COOP_HALVE_AMMO)) &&
				item->IsKindOf(RUNTIME_CLASS(AAmmo)))
			{
				if (defitem == NULL)
				{
					if (dmflags & DF_COOP_LOSE_AMMO)
					{
						// Do NOT destroy the ammo, because a weapon might reference it.
						item->Amount = 0;
					}
					else if (item->Amount > 1)
					{
						item->Amount /= 2;
					}
				}
				else
				{
					// When set to lose ammo, you get to keep all your starting ammo.
					// When set to halve ammo, you won't be left with less than your starting amount.
					if (dmflags & DF_COOP_LOSE_AMMO)
					{
						item->Amount = defitem->Amount;
					}
					else if (item->Amount > 1)
					{
						item->Amount = MAX(item->Amount / 2, defitem->Amount);
					}
				}
			}
		}
	}

	// Now destroy the default inventory this player is holding and move
	// over the old player's remaining inventory.
	DestroyAllInventory();
	ObtainInventory (oldplayer);

	player->ReadyWeapon = NULL;
	// [BB] The server waits for the client to select the weapon (but has to handle bots).
	if ( ( NETWORK_GetState( ) != NETSTATE_SERVER ) || player->bIsBot || ( SERVER_GetClient( player - players )->State == CLS_SPAWNED_BUT_NEEDS_AUTHENTICATION ) )
		PickNewWeapon (NULL);
	else
		PLAYER_ClearWeapon( player );
}

//===========================================================================
//
// APlayerPawn :: GetSoundClass
//
//===========================================================================

const char *APlayerPawn::GetSoundClass ()
{
	// [BC] If this player's skin is disabled, just use the base sound class.
	if (( cl_skins == 1 ) || (( cl_skins >= 2 ) &&
		( player != NULL ) &&
		( player->userinfo.skin < static_cast<signed> (skins.Size()) ) &&
		( skins[player->userinfo.skin].bCheat == false )))
	{
		if (player != NULL &&
			(unsigned int)player->userinfo.skin >= PlayerClasses.Size () &&
			(size_t)player->userinfo.skin < skins.Size())
		{
			return skins[player->userinfo.skin].name;
		}
	}

	// [GRB]
	const char *sclass = GetClass ()->Meta.GetMetaString (APMETA_SoundClass);
	return sclass != NULL ? sclass : "player";
}

//===========================================================================
//
// APlayerPawn :: GetMaxHealth
//
// only needed because Boom screwed up Dehacked.
//
//===========================================================================

int APlayerPawn::GetMaxHealth() const 
{ 
	return MaxHealth > 0? MaxHealth : ((i_compatflags&COMPATF_DEHHEALTH)? 100 : deh.MaxHealth);
}

//===========================================================================
//
// APlayerPawn :: UpdateWaterLevel
//
// Plays surfacing and diving sounds, as appropriate.
//
//===========================================================================

bool APlayerPawn::UpdateWaterLevel (fixed_t oldz, bool splash)
{
	int oldlevel = waterlevel;
	bool retval = Super::UpdateWaterLevel (oldz, splash);
	if (player != NULL )
	{
		if (oldlevel < 3 && waterlevel == 3)
		{ // Our head just went under.
			S_Sound (this, CHAN_VOICE, "*dive", 1, ATTN_NORM);
		}
		else if (oldlevel == 3 && waterlevel < 3)
		{ // Our head just came up.
			if (player->air_finished > level.time)
			{ // We hadn't run out of air yet.
				S_Sound (this, CHAN_VOICE, "*surface", 1, ATTN_NORM);
			}
			// If we were running out of air, then ResetAirSupply() will play *gasp.
		}
	}
	return retval;
}

//===========================================================================
//
// APlayerPawn :: ResetAirSupply
//
// Gives the player a full "tank" of air. If they had previously completely
// run out of air, also plays the *gasp sound. Returns true if the player
// was drowning.
//
//===========================================================================

bool APlayerPawn::ResetAirSupply (bool playgasp)
{
	bool wasdrowning = (player->air_finished < level.time);

	if (playgasp && wasdrowning)
	{
		S_Sound (this, CHAN_VOICE, "*gasp", 1, ATTN_NORM);
	}
	if (level.airsupply> 0) player->air_finished = level.time + level.airsupply;
	else player->air_finished = INT_MAX;
	return wasdrowning;
}

//===========================================================================
//
// Animations
//
//===========================================================================

void APlayerPawn::PlayIdle ()
{
	if (InStateSequence(state, SeeState))
	{
		// [BC] If we're the server, tell clients to update this player's state.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetPlayerState( ULONG( player - players ), STATE_PLAYER_IDLE, ULONG( player - players ), SVCF_SKIPTHISCLIENT );

		SetState (SpawnState);
	}
}

void APlayerPawn::PlayRunning ()
{
	if (InStateSequence(state, SpawnState) && SeeState != NULL)
	{
		// [BC] If we're the server, tell clients to update this player's state.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetPlayerState( ULONG( player - players ), STATE_PLAYER_SEE, ULONG( player - players ), SVCF_SKIPTHISCLIENT );

		SetState (SeeState);
	}
}

void APlayerPawn::PlayAttacking ()
{
	if (MissileState != NULL) SetState (MissileState);
}

void APlayerPawn::PlayAttacking2 ()
{
	if (MeleeState != NULL) SetState (MeleeState);
}

void APlayerPawn::ThrowPoisonBag ()
{
}

//===========================================================================
//
// APlayerPawn :: GiveDefaultInventory
//
//===========================================================================

void APlayerPawn::GiveDefaultInventory ()
{
	if (player == NULL) return;

	AInventory *fist, *pistol, *bullets;
	ULONG						ulIdx;
	const PClass				*pType;
	AWeapon						*pWeapon;
	APowerStrength				*pBerserk;
	AWeapon						*pPendingWeapon;
	AInventory					*pInventory;

	// [GRB] Give inventory specified in DECORATE
	player->health = GetDefault ()->health;

	// [BB] Spectators are supposed to have no inventory, but they should get their health.
	if ( player->bSpectating ) return;

	// [BC] Initialize the max. health bonus.
	player->lMaxHealthBonus = 0;

	// [BC] If the user has chosen to handicap himself, do that now.
	if (( deathmatch || teamgame || alwaysapplydmflags ) && player->userinfo.lHandicap )
	{
		player->health -= player->userinfo.lHandicap;

		// Don't allow player to be DOA.
		if ( player->health <= 0 )
			player->health = 1;
	}

	// HexenArmor must always be the first item in the inventory because
	// it provides player class based protection that should not affect
	// any other protection item.
	fixed_t hx[5];
	for(int i=0;i<5;i++)
	{
		hx[i] = GetClass()->Meta.GetMetaFixed(APMETA_Hexenarmor0+i);
	}
	GiveInventoryType (RUNTIME_CLASS(AHexenArmor));
	AHexenArmor *harmor = FindInventory<AHexenArmor>();
	harmor->Slots[4] = hx[0];
	harmor->SlotsIncrement[0] = hx[1];
	harmor->SlotsIncrement[1] = hx[2];
	harmor->SlotsIncrement[2] = hx[3];
	harmor->SlotsIncrement[3] = hx[4];

	// BasicArmor must come right after that. It should not affect any
	// other protection item as well but needs to process the damage
	// before the HexenArmor does.
	ABasicArmor *barmor = Spawn<ABasicArmor> (0,0,0, NO_REPLACE);
	barmor->BecomeItem ();
	barmor->SavePercent = 0;
	barmor->Amount = 0;
	AddInventory (barmor);

	// Now add the items from the DECORATE definition
	FDropItem *di = GetDropItems();

	// [BB] Buckshot only makes sense if this is a Doom, but not a Doom 1 game.
	const bool bBuckshotPossible = ((gameinfo.gametype == GAME_Doom) && (gameinfo.flags & GI_MAPxx) );

	// [BB] Ugly hack: Stuff for the Doom player. The instagib and buckshot stuff
	// has to be done before giving the default items.
	if ( this->GetClass()->IsDescendantOf( PClass::FindClass( "DoomPlayer" ) ) )
	{
		// [BB] The icon of ABasicArmor is the one of the blue armor. Change this here
		// to fix the fullscreen hud display.
		barmor->Icon = TexMan.GetTexture( "ARM1A0", 0 );
		// [BC] In instagib mode, give the player the railgun, and the maximum amount of cells
		// possible.
		if (( instagib ) && ( deathmatch || teamgame ))
		{
			// [BL] This used to call GiveInventoryTypeRespectingReplacements, but we also want to be sure
			// the railgun is a weapon so that we can be sure we give the player the proper kind of ammo.
			// [Dusk] Since Railgun was moved out to skulltag_actors, its presence must be checked for.
			const PClass *pRailgunClass = PClass::FindClass( "Railgun" );
			if(!pRailgunClass)
				I_Error("Tried to play instagib without a railgun!\n");

			const PClass *pRailgun = pRailgunClass->ActorInfo->GetReplacement( )->Class;
			if(!pRailgun->IsDescendantOf( RUNTIME_CLASS( AWeapon ) ))
				I_Error("Tried to give an improperly defined railgun.\n");

			// Give the player the weapon.
			pInventory = player->mo->GiveInventoryType( pRailgun );

			if ( pInventory )
			{
				// Make the weapon the player's ready weapon.
				// [BB] PLAYER_SetWeapon takes care of the special client / server and demo handling.
				PLAYER_SetWeapon ( player, static_cast<AWeapon *>( pInventory ), true );

				// Find the player's ammo for the weapon in his inventory, and max. out the amount.
				AInventory *ammo1 = player->mo->FindInventory( static_cast<AWeapon *> (pInventory)->AmmoType1 );
				AInventory *ammo2 = player->mo->FindInventory( static_cast<AWeapon *> (pInventory)->AmmoType2 );
				if ( ammo1 != NULL )
					ammo1->Amount = ammo1->MaxAmount;
				if ( ammo2 != NULL )
					ammo2->Amount = ammo2->MaxAmount;
			}

			return;
		}
		// [BC] In buckshot mode, give the player the SSG, and the maximum amount of shells
		// possible.
		else if (( buckshot && bBuckshotPossible ) && ( deathmatch || teamgame ))
		{
			// Give the player the weapon.
			pInventory = player->mo->GiveInventoryTypeRespectingReplacements( PClass::FindClass( "SuperShotgun" ) );

			if ( pInventory )
			{
				// Make the weapon the player's ready weapon.
				// [BB] PLAYER_SetWeapon takes care of the special client / server and demo handling.
				PLAYER_SetWeapon ( player, static_cast<AWeapon *>( pInventory ), true );
			}

			// Find the player's ammo for the weapon in his inventory, and max. out the amount.
			pInventory = player->mo->FindInventory( PClass::FindClass( "Shell" ));
			if ( pInventory != NULL )
				pInventory->Amount = pInventory->MaxAmount;
			return;
		}
	}

	while (di)
	{
		const PClass *ti = PClass::FindClass (di->Name);
		// [BB] Don't give out weapons in LMS that are supposed to be forbidden.
		if (ti && ( LASTMANSTANDING_IsWeaponDisallowed ( ti ) == false ) )
		{
			AInventory *item = FindInventory (ti);
			if (item != NULL)
			{
				item->Amount = clamp<int>(
					item->Amount + (di->amount ? di->amount : ((AInventory *)item->GetDefault ())->Amount),
					0, item->MaxAmount);
			}
			else
			{
				item = static_cast<AInventory *>(Spawn (ti, 0,0,0, NO_REPLACE));
				item->ItemFlags|=IF_IGNORESKILL;	// no skill multiplicators here
				item->Amount = di->amount;
				if (item->IsKindOf (RUNTIME_CLASS (AWeapon)))
				{
					// To allow better control any weapon is emptied of
					// ammo before being given to the player.
					static_cast<AWeapon*>(item)->AmmoGive1 =
					static_cast<AWeapon*>(item)->AmmoGive2 = 0;
				}
				AActor *check;
				if (!item->CallTryPickup(this, &check))
				{
					if (check != this)
					{
						// Player was morphed. This is illegal at game start.
						// This problem is only detectable when it's too late to do something about it...
						I_Error("Cannot give morph items when starting a game");
					}
					item->Destroy ();
					item = NULL;
				}
			}
			if (item != NULL && item->IsKindOf (RUNTIME_CLASS (AWeapon)) &&
				static_cast<AWeapon*>(item)->CheckAmmo(AWeapon::EitherFire, false))
			{
				// [BB] The server waits for the client to select the weapon (but has to handle bots and clients that are still loading the level).
				if ( ( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( player->bIsBot == false ) && ( SERVER_GetClient( player - players )->State != CLS_SPAWNED_BUT_NEEDS_AUTHENTICATION ) ) {
					PLAYER_ClearWeapon ( player );
					player->StartingWeaponName = item ? item->GetClass()->TypeName : NAME_None;
				}
				// [BB] When playing a client side demo, the default weapon for the consoleplayer
				// will be selected by a recorded CLD_LCMD_INVUSE command.
				else if ( ( CLIENTDEMO_IsPlaying() == false ) || ( player - players ) != consoleplayer )
					player->ReadyWeapon = player->PendingWeapon = static_cast<AWeapon *> (item);
			}
		}
		di = di->Next;
	}

	// [BB] If we're a client, tell the server the weapon we selected from the default inventory.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) && (( player - players ) == consoleplayer ) && player->PendingWeapon )
	{
		CLIENTCOMMANDS_WeaponSelect( player->PendingWeapon->GetClass( ));

		if ( CLIENTDEMO_IsRecording( ))
			CLIENTDEMO_WriteLocalCommand( CLD_LCMD_INVUSE, player->PendingWeapon->GetClass( )->TypeName.GetChars( ) );
	}

	// [BB] Ugly hack: Stuff for the Doom player. Moved here since the Doom player
	// was converted to DECORATE. TO-DO: Find a better place for this and perhaps
	// make this work for arbitraty player classes.
	if ( this->GetClass()->IsDescendantOf( PClass::FindClass( "DoomPlayer" ) ) )
	{
		// [BC] Give a bunch of weapons in LMS mode, depending on the LMS allowed weapon flags.
		if ( lastmanstanding || teamlms )
		{
			// Give the player all the weapons, and the maximum amount of every type of
			// ammunition.
			pPendingWeapon = NULL;

			// [BB] If we already have a pending weapon, use that one to initialize pPendingWeapon.
			// This will prevent us from starting with no weapon in hand online in case we are not
			// given additional weapons here, e.g. if everything additional is forbidden by lmsallowedweapons.
			if ( player->PendingWeapon != WP_NOCHANGE )
				pPendingWeapon = player->PendingWeapon;

			for ( ulIdx = 0; ulIdx < PClass::m_Types.Size( ); ulIdx++ )
			{
				pType = PClass::m_Types[ulIdx];

				// [BB] Don't give anything that is not allowed for our game.
				if ( pType->ActorInfo && ( pType->ActorInfo->GameFilter != GAME_Any ) && ( ( pType->ActorInfo->GameFilter & gameinfo.gametype ) == 0 ) )
					continue;

				// Potentially disallow certain weapons.
				if ( LASTMANSTANDING_IsWeaponDisallowed ( pType ) )
					continue;

				if ( pType->ParentClass->IsDescendantOf( RUNTIME_CLASS( AWeapon )))
				{
					pInventory = player->mo->GiveInventoryTypeRespectingReplacements( pType );

					// Make this weapon the player's pending weapon if it ranks higher.
					// [BB] We obviously only can do the cast when the possible replacement is still a weapon.
					if ( pInventory && pInventory->GetClass()->IsDescendantOf( RUNTIME_CLASS( AWeapon )) )
						pWeapon = static_cast<AWeapon *>( pInventory );
					else
						pWeapon = NULL;

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
			player->mo->GiveInventoryTypeRespectingReplacements( PClass::FindClass( "Berserk" ) );
			pBerserk = static_cast<APowerStrength *>( player->mo->FindInventory( PClass::FindClass( "PowerStrength" )));
			if ( pBerserk )
			{
				pBerserk->EffectTics = 768;
				// [BB] This is a workaround to set the EffectTics property of the powerup on the clients.
				// Note: The server already tells the clients that they got PowerStrength, because
				// Berserk uses A_GiveInventory("PowerStrength").
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_GivePowerup( static_cast<ULONG>(player - players), pBerserk );
			}

			player->health = deh.MegasphereHealth;
			player->mo->GiveInventoryTypeRespectingReplacements( PClass::FindClass( "GreenArmor" ) );
			player->health -= player->userinfo.lHandicap;

			// Don't allow player to be DOA.
			if ( player->health <= 0 )
				player->health = 1;

			// Finally, set the ready and pending weapon.
			// [BB] PLAYER_SetWeapon takes care of the special client / server and demo handling.
			PLAYER_SetWeapon( player, pPendingWeapon, true );
		}
		// [BC] If the user has the shotgun start flag set, do that!
		else if (( dmflags2 & DF2_COOP_SHOTGUNSTART ) &&
			( deathmatch == false ) &&
			( teamgame == false ))
		{
			pInventory = player->mo->GiveInventoryTypeRespectingReplacements( PClass::FindClass( "Shotgun" ) );
			if ( pInventory )
			{
				// [BB] PLAYER_SetWeapon takes care of the special client / server and demo handling.
				PLAYER_SetWeapon( player, static_cast<AWeapon *>( pInventory ), true );

				// Start them off with two clips.
				// [BB] PLAYER_SetWeapon doesn't set the consoleplayer's ReadyWeapon/PendingWeapon.
				// Thus, we can't use those pointers, but need to rely on pInventory.
				AInventory *pAmmo = player->mo->FindInventory( PClass::FindClass( "Shell" )->ActorInfo->GetReplacement( )->Class );
				if ( pAmmo != NULL )
					pAmmo->Amount = static_cast<AWeapon *>( pInventory )->AmmoGive1 * 2;
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
			// [BB] PLAYER_SetWeapon takes care of the special client / server and demo handling.
			PLAYER_SetWeapon( player,
				static_cast<AWeapon *> (deh.StartBullets > 0 ? pistol : fist), true );
			return;
		}
	}

	// [BB] LMS Stuff for the Heretic player. Moved here since the Heretic player
	// was converted to DECORATE. TO-DO: Find a better place for this and perhaps
	// make this work for arbitraty player classes.
	if ( this->GetClass()->IsDescendantOf( PClass::FindClass( "HereticPlayer" ) ) )
	{
		ULONG			ulIdx;
		const PClass	*pType;
		AWeapon			*pWeapon;
		AWeapon			*pPendingWeapon;
		AInventory		*pInventory;

		// [BC] Give a bunch of weapons in LMS mode, depending on the LMS allowed weapon flags.
		if ( lastmanstanding || teamlms )
		{
			// Give the player all the weapons, and the maximum amount of every type of
			// ammunition.
			pPendingWeapon = NULL;
			for ( ulIdx = 0; ulIdx < PClass::m_Types.Size( ); ulIdx++ )
			{
				pType = PClass::m_Types[ulIdx];

				// [BB] Don't give anything that is not allowed for our game.
				if ( pType->ActorInfo && ( ( pType->ActorInfo->GameFilter & gameinfo.gametype ) == 0 ) )
					continue;

				if ( pType->ParentClass->IsDescendantOf( RUNTIME_CLASS( AWeapon )))
				{
					pInventory = player->mo->GiveInventoryTypeRespectingReplacements( pType );

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

			player->health = deh.MegasphereHealth;
			player->mo->GiveInventoryTypeRespectingReplacements( PClass::FindClass( "SilverShield" ) );
			player->health -= player->userinfo.lHandicap;

			// Don't allow player to be DOA.
			if ( player->health <= 0 )
				player->health = 1;

			// Finally, set the ready and pending weapon.
			// [BB] PLAYER_SetWeapon takes care of the special client / server and demo handling.
			PLAYER_SetWeapon( player, pPendingWeapon, true );
		}
	}
}

void APlayerPawn::MorphPlayerThink ()
{
}

void APlayerPawn::ActivateMorphWeapon ()
{
	const PClass *morphweapon = PClass::FindClass (MorphWeapon);
	player->PendingWeapon = WP_NOCHANGE;
	player->psprites[ps_weapon].sy = WEAPONTOP;

	if (morphweapon == NULL || !morphweapon->IsDescendantOf (RUNTIME_CLASS(AWeapon)))
	{ // No weapon at all while morphed!
		player->ReadyWeapon = NULL;
		P_SetPsprite (player, ps_weapon, NULL);
	}
	else
	{
		player->ReadyWeapon = static_cast<AWeapon *>(player->mo->FindInventory (morphweapon));
		if (player->ReadyWeapon == NULL)
		{
			player->ReadyWeapon = static_cast<AWeapon *>(player->mo->GiveInventoryType (morphweapon));
			if (player->ReadyWeapon != NULL)
			{
				player->ReadyWeapon->GivenAsMorphWeapon = true; // flag is used only by new beastweap semantics in P_UndoPlayerMorph
			}
		}
		if (player->ReadyWeapon != NULL)
		{
			P_SetPsprite (player, ps_weapon, player->ReadyWeapon->GetReadyState());
		}
		else
		{
			P_SetPsprite (player, ps_weapon, NULL);
		}
	}
	P_SetPsprite (player, ps_flash, NULL);

	player->PendingWeapon = WP_NOCHANGE;
}

//===========================================================================
//
// APlayerPawn :: Die
//
//===========================================================================

void APlayerPawn::Die (AActor *source, AActor *inflictor)
{
	// [BB] Drop any important items the player may be carrying before handling
	// any other part of the death logic.
	if ( !NETWORK_InClientMode() )
		DropImportantItems ( false, source );

	Super::Die (source, inflictor);

	if (player != NULL && player->mo == this) player->bonuscount = 0;

	// [BC] Nothing for the client to do here.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return;
	}

	if (player != NULL && player->mo != this)
	{ // Make the real player die, too
		player->mo->Die ( source, inflictor );
		return;
	}
	// [BC] There was an "else" here that was completely unnecessary.

	if (player != NULL && (dmflags2 & DF2_YES_WEAPONDROP))
	{ // Voodoo dolls don't drop weapons
		AWeapon *weap = player->ReadyWeapon;
		if (weap != NULL)
		{
			AInventory *item;

			// kgDROP - start - modified copy from a_action.cpp
			FDropItem *di = weap->GetDropItems();

			if (di != NULL)
			{
				while (di != NULL)
				{
					if (di->Name != NAME_None)
					{
						const PClass *ti = PClass::FindClass(di->Name);
						if (ti) P_DropItem (player->mo, ti, di->amount, di->probability);
					}
					di = di->Next;
				}
			} else
			// kgDROP - end
			if (weap->SpawnState != NULL &&
				weap->SpawnState != ::GetDefault<AActor>()->SpawnState)
			{
				item = P_DropItem (this, weap->GetClass(), -1, 256);
				if (item != NULL)
				{
					if (weap->AmmoGive1 && weap->Ammo1)
					{
						static_cast<AWeapon *>(item)->AmmoGive1 = weap->Ammo1->Amount;
					}
					if (weap->AmmoGive2 && weap->Ammo2)
					{
						static_cast<AWeapon *>(item)->AmmoGive2 = weap->Ammo2->Amount;
					}
					item->ItemFlags |= IF_IGNORESKILL;

					// [BB] Now that the ammo amount from weapon pickups is handled on the server
					// this shouldn't be necessary anymore. Remove after thorough testing.
					// [BC] If we're the server, tell clients that the thing is dropped.
					//if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					//	SERVERCOMMANDS_SetWeaponAmmoGive( item );
				}
			}
			else
			{
				item = P_DropItem (this, weap->AmmoType1, -1, 256);
				if (item != NULL)
				{
					item->Amount = weap->Ammo1->Amount;
					item->ItemFlags |= IF_IGNORESKILL;
				}
				item = P_DropItem (this, weap->AmmoType2, -1, 256);
				if (item != NULL)
				{
					item->Amount = weap->Ammo2->Amount;
					item->ItemFlags |= IF_IGNORESKILL;
				}
			}
		}
	}

	// [BC] The following require player to be non-NULL.
	if (( player == NULL ) || ( player->mo == NULL ))
		return;

/*
	// If this is cooperative mode, drop a backpack full of the player's stuff.
	if (( deathmatch == false ) && ( teamgame == false ) &&
		(( i_compatflags & COMPATF_DISABLECOOPERATIVEBACKPACKS ) == false ) &&
		( NETWORK_GetState( ) != NETSTATE_SINGLE ))
	{
		AActor	*pBackpack;

		// Spawn the backpack.
		pBackpack = Spawn( RUNTIME_CLASS( ACooperativeBackpack ), x, y, z, NO_REPLACE );

		// If we're the server, tell clients to spawn the backpack.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SpawnThing( pBackpack );

		// Finally, fill the backpack with the player's inventory items.
		if ( pBackpack )
			static_cast<ACooperativeBackpack *>( pBackpack )->FillBackpack( player );
	}
*/
	if (( NETWORK_GetState( ) == NETSTATE_SINGLE ) && (level.flags2 & LEVEL2_DEATHSLIDESHOW))
	{
		F_StartSlideshow ();
	}
}

void APlayerPawn::DropImportantItems( bool bLeavingGame, AActor *pSource )
{
	AActor		*pTeamItem;
	AInventory	*pInventory;

	if ( player == NULL )
		return;

	// If we're in a teamgame, don't allow him to "take" flags or skulls with him. If
	// he was carrying any, spawn what he was carrying on the ground when he leaves.
	if (( teamgame ) && ( player->bOnTeam ))
	{
		// Check if he's carrying the opponents' flag.
		for ( ULONG i = 0; i < teams.Size( ); i++ )
		{
			pInventory = this->FindInventory( TEAM_GetItem( i ));

			if ( pInventory )
			{
				this->RemoveInventory( pInventory );

				// Tell the clients that this player no longer possesses a flag.
				if (( bLeavingGame == false ) && ( NETWORK_GetState( ) == NETSTATE_SERVER ))
					SERVERCOMMANDS_TakeInventory( player - players, TEAM_GetItem( i )->TypeName.GetChars( ), 0 );
				if ( NETWORK_GetState( ) != NETSTATE_SERVER )
					SCOREBOARD_RefreshHUD( );

				// Spawn a new flag.
				pTeamItem = Spawn( TEAM_GetItem( i ), x, y, z, NO_REPLACE );

				if ( pTeamItem )
				{
					pTeamItem->flags |= MF_DROPPED;

					// If the flag spawned in an instant return zone, the return routine
					// has already been executed. No need to do anything!
					if ( pTeamItem->Sector && (( pTeamItem->Sector->MoreFlags & SECF_RETURNZONE ) == false ))
					{
						if ( dmflags2 & DF2_INSTANT_RETURN )
							TEAM_ExecuteReturnRoutine( i, NULL ); // [BB] Flags returned by DF2_INSTANT_RETURN are considered to have no "returner" player.
						else
						{
							TEAM_SetReturnTicks( i, sv_flagreturntime * TICRATE );

							// Print flag dropped message and do announcer stuff.
							TEAM_FlagDropped( player, i );

							// If we're the server, spawn the item to clients.
							if ( NETWORK_GetState( ) == NETSTATE_SERVER )
								SERVERCOMMANDS_SpawnThing( pTeamItem );
						}
					}
				}

				// Cancel out the potential for an assist.
				TEAM_SetAssistPlayer( player->ulTeam, MAXPLAYERS );

				// Award a "Defense!" medal to the player who fragged this flag carrier.
				// [BB] but only if the flag belongs to the team of the fragger.
				if (( pSource ) && ( pSource->player ) && ( pSource->IsTeammate( this ) == false ) && ( pSource->player->ulTeam == i ))
				{
					MEDAL_GiveMedal( pSource->player - players, MEDAL_DEFENSE );
					if ( NETWORK_GetState( ) == NETSTATE_SERVER )
						SERVERCOMMANDS_GivePlayerMedal( pSource->player - players, MEDAL_DEFENSE );
				}
			}
		}

		// Check if the player is carrying the white flag.
		pInventory = this->FindInventory( PClass::FindClass( "WhiteFlag" ));
		if (( oneflagctf ) && ( pInventory ))
		{
			this->RemoveInventory( pInventory );

			// Tell the clients that this player no longer possesses a flag.
			if (( bLeavingGame == false ) && ( NETWORK_GetState( ) == NETSTATE_SERVER ))
				SERVERCOMMANDS_TakeInventory( player - players, "WhiteFlag", 0 );
			if ( NETWORK_GetState( ) != NETSTATE_SERVER )
				SCOREBOARD_RefreshHUD( );

			// Spawn a new flag.
			pTeamItem = Spawn( PClass::FindClass( "WhiteFlag" ), x, y, ONFLOORZ, NO_REPLACE );
			if ( pTeamItem )
			{
				pTeamItem->flags |= MF_DROPPED;

				pTeamItem->tid = 668;
				pTeamItem->AddToHash( );

				// If the flag spawned in an instant return zone, the return routine
				// has already been executed. No need to do anything!
				if ( pTeamItem->Sector && (( pTeamItem->Sector->MoreFlags & SECF_RETURNZONE ) == false ))
				{
					if ( dmflags2 & DF2_INSTANT_RETURN )
						TEAM_ExecuteReturnRoutine( teams.Size( ), NULL );
					else
					{
						TEAM_SetReturnTicks( teams.Size( ), sv_flagreturntime * TICRATE );

						// If we're the server, spawn the item to clients.
						if ( NETWORK_GetState( ) == NETSTATE_SERVER )
							SERVERCOMMANDS_SpawnThing( pTeamItem );
					}
				}
			}

			// Award a "Defense!" medal to the player who fragged this flag carrier.
			if ( pSource && pSource->player && ( pSource->IsTeammate( this ) == false ))
			{
				MEDAL_GiveMedal( pSource->player - players, MEDAL_DEFENSE );
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_GivePlayerMedal( pSource->player - players, MEDAL_DEFENSE );
			}
		}
	}

	// If we're in a terminator game, don't allow the player to "take" the terminator
	// artifact with him.
	if ( terminator )
	{
		if ( player->cheats2 & CF2_TERMINATORARTIFACT )
		{
			P_DropItem( this, PClass::FindClass( "Terminator" ), -1, 256 );

			// Tell the clients that this player no longer possesses the terminator orb.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_TakeInventory( player - players, "PowerTerminatorArtifact", 0 );
			else
				SCOREBOARD_RefreshHUD( );
		}
	}

	// If we're in a possession game, don't allow the player to "take" the possession
	// artifact with him.
	if ( possession || teampossession )
	{
		if ( player->cheats2 & CF2_POSSESSIONARTIFACT )
		{
			P_DropItem( this, PClass::FindClass( "PossessionStone" ), -1, 256 );

			// Tell the clients that this player no longer possesses the stone.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_TakeInventory( player - players, "PowerPossessionArtifact", 0 );
			else
				SCOREBOARD_RefreshHUD( );

			// Tell the possession module that the artifact has been dropped.
			if ( possession || teampossession )
				POSSESSION_ArtifactDropped( );
		}
	}
}

//===========================================================================
//
// APlayerPawn :: TweakSpeeds
//
//===========================================================================

void APlayerPawn::TweakSpeeds (int &forward, int &side)
{
	// [Dusk] Let the user move at whatever speed they desire when spectating.
	if (player->bSpectating) {
		fixed_t factor = FLOAT2FIXED(cl_spectatormove);
		forward = FixedMul(forward, factor);
		side = FixedMul(side, factor);
		return;
	}

	// Strife's player can't run when its healh is below 10
	if (health <= RunHealth)
	{
		forward = clamp(forward, -0x1900, 0x1900);
		side = clamp(side, -0x1800, 0x1800);
	}

	// [GRB]
	if ((unsigned int)(forward + 0x31ff) < 0x63ff)
	{
		forward = FixedMul (forward, ForwardMove1);
	}
	else
	{
		forward = FixedMul (forward, ForwardMove2);
	}
	if ((unsigned int)(side + 0x27ff) < 0x4fff)
	{
		side = FixedMul (side, SideMove1);
	}
	else
	{
		side = FixedMul (side, SideMove2);
	}

	// [BC] This comes out to 50%, so we can use this for the turbosphere.
	if (!player->morphTics && Inventory != NULL)
	{
		fixed_t factor = Inventory->GetSpeedFactor ();
		forward = FixedMul(forward, factor);
		side = FixedMul(side, factor);
	}

	// [BC] Apply the 25% speed increase power.
	if ( player->cheats & CF_SPEED25 )
	{
		forward = (LONG)( forward * 1.25 );
		side = (LONG)( side * 1.25 );
	}
}

//===========================================================================
//
// [BC] APlayerPawn :: Destroy
//
// All this function does is set the actor's health to 0, and then call the
// super function. This is done to prevent the player from cycling through
// weapons when it dies.
//
//===========================================================================

void APlayerPawn::Destroy( void )
{
	// Set the actor's health to 0, so that when the player's inventory
	// is destroyed, a new weapon isn't chosen for the player as his
	// ready weapon isn't deleted, preventing the playing of upsounds.
	this->health = 0;

	// That's it. Now proceed as normal.
	Super::Destroy( );
}

//===========================================================================
//
// [Dusk] This is in a separate function now.
//
//===========================================================================
fixed_t APlayerPawn::CalcJumpMomz( )
{
	fixed_t z = JumpZ * 35 / TICRATE;

	// [BC] If the player has the high jump power, double his jump velocity.
	if ( player->cheats & CF_HIGHJUMP )
		z *= 2;

	// [BC] If the player is standing on a spring pad, halve his jump velocity.
	if ( player->mo->floorsector->GetFlags(sector_t::floor) & SECF_SPRINGPAD )
		z /= 2;

	return z;
}

//===========================================================================
//
// [Dusk] Calculate the height a player can reach with a jump
//
//===========================================================================
fixed_t APlayerPawn::CalcJumpHeight( bool bAddStepZ )
{
	// To get the jump height we simulate a jump with the player's jumpZ with
	// the environment's gravity. The grav equation was copied from p_mobj.cpp.
	// Should it be made a function?
	fixed_t momz = CalcJumpMomz( ),
	        grav = (fixed_t)(level.gravity * Sector->gravity * FIXED2FLOAT(gravity) * 81.92),
	        z = 0;

	// This hangs if grav is 0. I'm not sure what to return in such a scenario
	// so we'll just return 0.
	if ( grav <= 0 )
		return 0;

	// Simulate the jump now.
	while ( momz > 0 )
	{
		z += momz;
		momz -= grav;
	}

	// The total height the player can reach is the calculated max Z plus the
	// max step height. I guess the bare max Z value can also be of interest
	// so the step Z an optional, yes-defaulting parameter.
	if ( bAddStepZ )
		z += MaxStepHeight;

	return z;
}

//===========================================================================
//
// A_PlayerScream
//
// try to find the appropriate death sound and use suitable 
// replacements if necessary
//
//===========================================================================

DEFINE_ACTION_FUNCTION(AActor, A_PlayerScream)
{
	int sound = 0;
	int chan = CHAN_VOICE;

	if (self->player == NULL || self->DeathSound != 0)
	{
		if (self->DeathSound != 0)
		{
			S_Sound (self, CHAN_VOICE, self->DeathSound, 1, ATTN_NORM);
		}
		else
		{
			S_Sound (self, CHAN_VOICE, "*death", 1, ATTN_NORM);
		}
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

	if (!sound && self->special1<10)
	{ // Wimpy death sound
		sound = S_FindSkinnedSoundEx (self, "*wimpydeath", self->player->LastDamageType);
	}
	if (!sound && self->health <= -50)
	{
		if (self->health > -100)
		{ // Crazy death sound
			sound = S_FindSkinnedSoundEx (self, "*crazydeath", self->player->LastDamageType);
		}
		if (!sound)
		{ // Extreme death sound
			sound = S_FindSkinnedSoundEx (self, "*xdeath", self->player->LastDamageType);
			if (!sound)
			{
				sound = S_FindSkinnedSoundEx (self, "*gibbed", self->player->LastDamageType);
				chan = CHAN_BODY;
			}
		}
	}
	if (!sound)
	{ // Normal death sound
		sound = S_FindSkinnedSoundEx (self, "*death", self->player->LastDamageType);
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
	S_Sound (self, chan, sound, 1, ATTN_NORM);
}


//----------------------------------------------------------------------------
//
// PROC A_SkullPop
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_SkullPop)
{
	ACTION_PARAM_START(1);
	ACTION_PARAM_CLASS(spawntype, 0);

	APlayerPawn *mo;
	player_t *player;

	// [GRB] Parameterized version
	if (!spawntype || !spawntype->IsDescendantOf (RUNTIME_CLASS (APlayerChunk)))
	{
		spawntype = PClass::FindClass("BloodySkull");
		if (spawntype == NULL) return;
	}

	self->flags &= ~MF_SOLID;
	mo = (APlayerPawn *)Spawn (spawntype, self->x, self->y, self->z + 48*FRACUNIT, NO_REPLACE);
	//mo->target = self;
	mo->momx = pr_skullpop.Random2() << 9;
	mo->momy = pr_skullpop.Random2() << 9;
	mo->momz = 2*FRACUNIT + (pr_skullpop() << 6);
	// Attach player mobj to bloody skull
	player = self->player;
	self->player = NULL;
	mo->ObtainInventory (self);
	mo->player = player;
	mo->health = self->health;
	mo->angle = self->angle;
	if (player != NULL)
	{
		player->mo = mo;
		if (player->camera == self)
		{
			player->camera = mo;
		}
		player->damagecount = 32;

		// [BC] Attach the player's icon to the skull.
		if ( player->pIcon )
			player->pIcon->SetTracer( mo );
	}
}

//----------------------------------------------------------------------------
//
// PROC A_CheckSkullDone
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_CheckPlayerDone)
{
	if (self->player == NULL)
	{
		self->Destroy ();
	}
}

//===========================================================================
//
// P_CheckPlayerSprites
//
// Here's the place where crouching sprites are handled
// This must be called each frame before rendering
//
//===========================================================================

void P_CheckPlayerSprites()
{
	LONG	lSkin;

	for(int i=0; i<MAXPLAYERS; i++)
	{
		player_t * player = &players[i];
		APlayerPawn * mo = player->mo;

		if (playeringame[i] && mo != NULL)
		{
			int crouchspriteno;
			fixed_t defscaleY = mo->GetDefault()->scaleY;
			fixed_t defscaleX = mo->GetDefault()->scaleX;
			
			// [BC] Because of cl_skins, we might not necessarily use the player's
			// desired skin.
			lSkin = player->userinfo.skin;

			// [BB] MF4_NOSKIN should force the player to have the base skin too, the same is true for morphed players.
			if (( cl_skins <= 0 ) || ((( cl_skins >= 2 ) && ( skins[player->userinfo.skin].bCheat ))) || (player->mo->flags4 & MF4_NOSKIN) || player->morphTics )
				lSkin = R_FindSkin( "base", player->CurrentPlayerClass );

			// [BB] If the weapon has a PreferredSkin defined, make the player use it here.
			if ( player->ReadyWeapon && ( player->ReadyWeapon->PreferredSkin != NAME_None ) )
			{
				LONG lDesiredSkin = R_FindSkin( player->ReadyWeapon->PreferredSkin.GetChars(), player->CurrentPlayerClass );
				if ( lDesiredSkin != lSkin )
				{
					lSkin = lDesiredSkin;
					mo->sprite = skins[lSkin].sprite;
				}
			}
			// [BB] No longer using a weapon with a preferred skin, reset the sprite.
			else if ( ( mo->sprite != skins[lSkin].sprite ) && ( mo->sprite != skins[lSkin].crouchsprite )
					&& ( mo->sprite != mo->state->sprite ) )
			{
				mo->sprite = skins[lSkin].sprite;
			}

			if (lSkin != 0 && !(player->mo->flags4 & MF4_NOSKIN))
			{
				defscaleY = skins[lSkin].ScaleY;
				defscaleX = skins[lSkin].ScaleX;
			}
			
			// Set the crouch sprite
			if (player->crouchfactor < FRACUNIT*3/4)
			{

				if (mo->sprite == mo->SpawnState->sprite || mo->sprite == mo->crouchsprite) 
				{
					crouchspriteno = mo->crouchsprite;
				}
				else if (!(player->mo->flags4 & MF4_NOSKIN) &&
						(mo->sprite == skins[lSkin].sprite ||
						 mo->sprite == skins[lSkin].crouchsprite))
				{
					crouchspriteno = skins[lSkin].crouchsprite;
				}
				else
				{
					// no sprite -> squash the existing one
					crouchspriteno = -1;
				}

				if (crouchspriteno > 0) 
				{
					mo->sprite = crouchspriteno;
					mo->scaleY = defscaleY;
				}
				else if (player->playerstate != PST_DEAD)
				{
					mo->scaleY = player->crouchfactor < FRACUNIT*3/4 ? defscaleY/2 : defscaleY;
				}
			}
			else	// Set the normal sprite
			{
				if (mo->sprite == mo->crouchsprite)
				{
					mo->sprite = mo->SpawnState->sprite;
				}
				else if (mo->sprite == skins[lSkin].crouchsprite)
				{
					mo->sprite = skins[lSkin].sprite;
				}
				mo->scaleY = defscaleY;
			}
			mo->scaleX = defscaleX;
		}
	}
}

/*
==================
=
= P_Thrust
=
= moves the given origin along a given angle
=
==================
*/

void P_SideThrust (player_t *player, angle_t angle, fixed_t move)
{
	angle = (angle - ANGLE_90) >> ANGLETOFINESHIFT;

	player->mo->momx += FixedMul (move, finecosine[angle]);
	player->mo->momy += FixedMul (move, finesine[angle]);
}

void P_ForwardThrust (player_t *player, angle_t angle, fixed_t move)
{
	angle >>= ANGLETOFINESHIFT;

	if ((player->mo->waterlevel || (player->mo->flags & MF_NOGRAVITY))
		&& player->mo->pitch != 0)
	{
		angle_t pitch = (angle_t)player->mo->pitch >> ANGLETOFINESHIFT;
		fixed_t zpush = FixedMul (move, finesine[pitch]);
		if (player->mo->waterlevel && player->mo->waterlevel < 2 && zpush < 0)
			zpush = 0;
		player->mo->momz -= zpush;
		move = FixedMul (move, finecosine[pitch]);
	}
	player->mo->momx += FixedMul (move, finecosine[angle]);
	player->mo->momy += FixedMul (move, finesine[angle]);
}

//
// P_Bob
// Same as P_Thrust, but only affects bobbing.
//
// killough 10/98: We apply thrust separately between the real physical player
// and the part which affects bobbing. This way, bobbing only comes from player
// motion, nothing external, avoiding many problems, e.g. bobbing should not
// occur on conveyors, unless the player walks on one, and bobbing should be
// reduced at a regular rate, even on ice (where the player coasts).
//

void P_Bob (player_t *player, angle_t angle, fixed_t move)
{
	angle >>= ANGLETOFINESHIFT;

	player->momx += FixedMul(move,finecosine[angle]);
	player->momy += FixedMul(move,finesine[angle]);
}

/*
==================
=
= P_CalcHeight
=
=
Calculate the walking / running height adjustment
=
==================
*/

void P_CalcHeight (player_t *player) 
{
	int 		angle;
	fixed_t 	bob;
	bool		still = false;

	// [BB] Clients don't calculate the viewheight of the player whose eyes they are looking through
	// they receive that from the server (except if they are looking through their own eyes).
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) && ( player->mo->CheckLocalView( consoleplayer ) ) &&
		(( player - players ) != consoleplayer ))
	{
		return;
	}

	// [BC] If we're predicting, nothing to do here.
	if ( CLIENT_PREDICT_IsPredicting( ))
		return;

	// Regular movement bobbing
	// (needs to be calculated for gun swing even if not on ground)

	// killough 10/98: Make bobbing depend only on player-applied motion.
	//
	// Note: don't reduce bobbing here if on ice: if you reduce bobbing here,
	// it causes bobbing jerkiness when the player moves from ice to non-ice,
	// and vice-versa.

	if ((player->mo->flags & MF_NOGRAVITY) && !onground)
	{
		player->bob = FRACUNIT / 2;
	}
	else
	{
		player->bob = DMulScale16 (player->momx, player->momx, player->momy, player->momy);
		if (player->bob == 0)
		{
			still = true;
		}
		else
		{
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				player->bob = FixedMul( player->bob, 16384 );
			else
				player->bob = FixedMul (player->bob, player->userinfo.MoveBob);

			// [BB] I've seen bob becoming negative for a high ping clients (250+) on low gravity servers (sv_gravity 200)
			// when moving forward in the air for too long. Overflow problem? Nevertheless, setting negative values
			// to MAXBOB seems to fix the issue.
			if ( (player->bob > MAXBOB) || player->bob < 0 )
				player->bob = MAXBOB;
		}
	}

	fixed_t defaultviewheight = player->mo->ViewHeight + player->crouchviewdelta;

	if (player->cheats & CF_NOMOMENTUM)
	{
		player->viewz = player->mo->z + defaultviewheight;

		if (player->viewz > player->mo->ceilingz-4*FRACUNIT)
			player->viewz = player->mo->ceilingz-4*FRACUNIT;

		return;
	}

	if (still)
	{
		if (player->health > 0)
		{
			// [BC] We need to cap level.time, because if it gets too big, DivScale
			// can crash.
			angle = DivScale13 (level.time % 65536, 120*TICRATE/35) & FINEMASK;
			bob = FixedMul (player->userinfo.StillBob, finesine[angle]);
		}
		else
		{
			bob = 0;
		}
	}
	else
	{
		// DivScale 13 because FINEANGLES == (1<<13)
		// [BC] We need to cap level.time, because if it gets too big, DivScale
		// can crash.
		angle = DivScale13 (level.time % 65536, 20*TICRATE/35) & FINEMASK;
		bob = FixedMul (player->bob>>(player->mo->waterlevel > 1 ? 2 : 1), finesine[angle]);
	}

	// move viewheight
	if (player->playerstate == PST_LIVE)
	{
		player->viewheight += player->deltaviewheight;

		if (player->viewheight > defaultviewheight)
		{
			player->viewheight = defaultviewheight;
			player->deltaviewheight = 0;
		}
		else if (player->viewheight < (defaultviewheight>>1))
		{
			player->viewheight = defaultviewheight>>1;
			if (player->deltaviewheight <= 0)
				player->deltaviewheight = 1;
		}
		
		if (player->deltaviewheight)	
		{
			player->deltaviewheight += FRACUNIT/4;
			if (!player->deltaviewheight)
				player->deltaviewheight = 1;
		}
	}

	if (player->morphTics)
	{
		bob = 0;
	}
	player->viewz = player->mo->z + player->viewheight + bob;
	if (player->mo->floorclip && player->playerstate != PST_DEAD
		&& player->mo->z <= player->mo->floorz)
	{
		player->viewz -= player->mo->floorclip;
	}
	if (player->viewz > player->mo->ceilingz - 4*FRACUNIT)
	{
		player->viewz = player->mo->ceilingz - 4*FRACUNIT;
	}
	if (player->viewz < player->mo->floorz + 4*FRACUNIT)
	{
		player->viewz = player->mo->floorz + 4*FRACUNIT;
	}
}

/*
=================
=
= P_MovePlayer
=
=================
*/
CUSTOM_CVAR (Float, sv_aircontrol, 0.00390625f, CVAR_SERVERINFO|CVAR_NOSAVE)
{
	level.aircontrol = (fixed_t)(self * 65536.f);
	G_AirControlChanged ();

	// [BB] Let the clients know about the change.
	if (( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( gamestate != GS_STARTUP ))
	{
		SERVER_Printf( PRINT_HIGH, "%s changed to: %f\n", self.GetName( ), (float)self );
		SERVERCOMMANDS_SetGameModeLimits( );
	}
}

void P_MovePlayer (player_t *player, ticcmd_t *cmd)
{
	// [BB] A client doesn't know enough about the other players to make their movement.
	if ((( NETWORK_GetState( ) == NETSTATE_CLIENT ) || ( CLIENTDEMO_IsPlaying( ))) &&
		(( player - players ) != consoleplayer ) && !CLIENTDEMO_IsFreeSpectatorPlayer ( player ))
	{
		return;
	}

	APlayerPawn *mo = player->mo;

	// [RH] 180-degree turn overrides all other yaws
	if (player->turnticks)
	{
		player->turnticks--;
		mo->angle += (ANGLE_180 / TURN180_TICKS);
	}
	else
	{
		mo->angle += cmd->ucmd.yaw << 16;
	}

	if ( GAME_GetEndLevelDelay( ))
		memset( cmd, 0, sizeof( ticcmd_t ));

	onground = (mo->z <= mo->floorz) || (mo->flags2 & MF2_ONMOBJ);

	// killough 10/98:
	//
	// We must apply thrust to the player and bobbing separately, to avoid
	// anomalies. The thrust applied to bobbing is always the same strength on
	// ice, because the player still "works just as hard" to move, while the
	// thrust applied to the movement varies with 'movefactor'.

	if (cmd->ucmd.forwardmove | cmd->ucmd.sidemove)
	{
		fixed_t forwardmove, sidemove;
		int bobfactor;
		int friction, movefactor;
		int fm, sm;

		movefactor = P_GetMoveFactor (mo, &friction);
		bobfactor = friction < ORIG_FRICTION ? movefactor : ORIG_FRICTION_FACTOR;
		if (!onground && !(player->mo->flags & MF_NOGRAVITY) && !player->mo->waterlevel)
		{
			// [RH] allow very limited movement if not on ground.
			if ( i_compatflags & COMPATF_LIMITED_AIRMOVEMENT )
			{
				movefactor = FixedMul (movefactor, level.aircontrol);
				bobfactor = FixedMul (bobfactor, level.aircontrol);
			}
			else
			{
				// Skulltag needs increased air movement for jump pads, etc.
				movefactor /= 4;
				bobfactor /= 4;
			}
		}

		fm = cmd->ucmd.forwardmove;
		sm = cmd->ucmd.sidemove;
		mo->TweakSpeeds (fm, sm);
		fm = FixedMul (fm, player->mo->Speed);
		sm = FixedMul (sm, player->mo->Speed);

		// When crouching speed and bobbing have to be reduced
		if (player->morphTics==0 && player->crouchfactor != FRACUNIT)
		{
			fm = FixedMul(fm, player->crouchfactor);
			sm = FixedMul(sm, player->crouchfactor);
			bobfactor = FixedMul(bobfactor, player->crouchfactor);
		}

		forwardmove = Scale (fm, movefactor * 35, TICRATE << 8);
		sidemove = Scale (sm, movefactor * 35, TICRATE << 8);

		if (forwardmove)
		{
			P_Bob (player, mo->angle, (cmd->ucmd.forwardmove * bobfactor) >> 8);
			P_ForwardThrust (player, mo->angle, forwardmove);
		}
		if (sidemove)
		{
			P_Bob (player, mo->angle-ANG90, (cmd->ucmd.sidemove * bobfactor) >> 8);
			P_SideThrust (player, mo->angle, sidemove);
		}
/*
		if (debugfile)
		{
			fprintf (debugfile, "move player for pl %d%c: (%d,%d,%d) (%d,%d) %d %d w%d [", int(player-players),
				player->cheats&CF_PREDICTING?'p':' ',
				player->mo->x, player->mo->y, player->mo->z,forwardmove, sidemove, movefactor, friction, player->mo->waterlevel);
			msecnode_t *n = player->mo->touching_sectorlist;
			while (n != NULL)
			{
				fprintf (debugfile, "%td ", n->m_sector-sectors);
				n = n->m_tnext;
			}
			fprintf (debugfile, "]\n");
		}
*/
		// [BB] Spectators shall stay in their spawn state and don't execute any code pointers.
		if ( (CLIENT_PREDICT_IsPredicting( ) == false) && (forwardmove|sidemove) && (player->bSpectating == false) )//(!(player->cheats & CF_PREDICTING))
		{
			player->mo->PlayRunning ();
		}

		if (player->cheats & CF_REVERTPLEASE)
		{
			player->cheats &= ~CF_REVERTPLEASE;
			player->camera = player->mo;
		}
	}

	// [RH] check for jump
	if ( cmd->ucmd.buttons & BT_JUMP && ( player->bSpectating == false ))
	{
		if (player->mo->waterlevel >= 2)
		{
			player->mo->momz = 4*FRACUNIT;
		}
		else if (player->mo->flags2 & MF2_FLY)
		{
			player->mo->momz = 3*FRACUNIT;
		}
		else if (level.IsJumpingAllowed() && onground && !player->jumpTics )
		{
			fixed_t	JumpMomz;
			ULONG	ulJumpTicks;

			// Set base jump velocity.
			// [Dusk] Exported this into a function as I need it elsewhere as well.
			JumpMomz = player->mo->CalcJumpMomz( );

			// Set base jump ticks.
			ulJumpTicks = 18 * TICRATE / 35;

			// [BB] We may not play the sound while predicting, otherwise it'll stutter.
			if ( CLIENT_PREDICT_IsPredicting( ) == false )
				S_Sound (player->mo, CHAN_BODY, "*jump", 1, ATTN_NORM);
			player->mo->flags2 &= ~MF2_ONMOBJ;

			// [BC] Increase jump delay if the player has the high jump power.
			if ( player->cheats & CF_HIGHJUMP )
				ulJumpTicks *= 2;

			// [BC] Remove jump delay if the player is on a spring pad.
			if ( player->mo->floorsector->GetFlags(sector_t::floor) & SECF_SPRINGPAD )
				ulJumpTicks = 0;

			player->mo->momz += JumpMomz;
			player->jumpTics = ulJumpTicks;
		}
	}
}		

//==========================================================================
//
// P_FallingDamage
//
//==========================================================================

void P_FallingDamage (AActor *actor)
{
	int damagestyle;
	int damage;
	fixed_t mom;

	// [BB] This is handled server-side.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return;
	}

	damagestyle = ((level.flags >> 15) | (dmflags)) &
		(DF_FORCE_FALLINGZD | DF_FORCE_FALLINGHX);

	if (damagestyle == 0)
		return;
		
	if (actor->floorsector->Flags & SECF_NOFALLINGDAMAGE)
		return;

	mom = abs (actor->momz);

	// Since Hexen falling damage is stronger than ZDoom's, it takes
	// precedence. ZDoom falling damage may not be as strong, but it
	// gets felt sooner.

	switch (damagestyle)
	{
	case DF_FORCE_FALLINGHX:		// Hexen falling damage
		if (mom <= 23*FRACUNIT)
		{ // Not fast enough to hurt
			return;
		}
		if (mom >= 63*FRACUNIT)
		{ // automatic death
			damage = 1000000;
		}
		else
		{
			mom = FixedMul (mom, 16*FRACUNIT/23);
			damage = ((FixedMul (mom, mom) / 10) >> FRACBITS) - 24;
			if (actor->momz > -39*FRACUNIT && damage > actor->health
				&& actor->health != 1)
			{ // No-death threshold
				damage = actor->health-1;
			}
		}
		break;
	
	case DF_FORCE_FALLINGZD:		// ZDoom falling damage
		if (mom <= 19*FRACUNIT)
		{ // Not fast enough to hurt
			return;
		}
		if (mom >= 84*FRACUNIT)
		{ // automatic death
			damage = 1000000;
		}
		else
		{
			damage = ((MulScale23 (mom, mom*11) >> FRACBITS) - 30) / 2;
			if (damage < 1)
			{
				damage = 1;
			}
		}
		break;

	case DF_FORCE_FALLINGST:		// Strife falling damage
		if (mom <= 20*FRACUNIT)
		{ // Not fast enough to hurt
			return;
		}
		// The minimum amount of damage you take from falling in Strife
		// is 52. Ouch!
		damage = mom / 25000;
		break;

	default:
		return;
	}

	if (actor->player)
	{
		S_Sound (actor, CHAN_AUTO, "*land", 1, ATTN_NORM);
		P_NoiseAlert (actor, actor, true);
		if (damage == 1000000 && (actor->player->cheats & CF_GODMODE))
		{
			damage = 999;
		}
	}
	P_DamageMobj (actor, NULL, NULL, damage, NAME_Falling);
}

//==========================================================================
//
// P_DeathThink
//
//==========================================================================

void P_DeathThink (player_t *player)
{
	int dir;
	angle_t delta;
	int lookDelta;

	P_MovePsprites (player);

	onground = (player->mo->z <= player->mo->floorz);
	if (player->mo->IsKindOf (RUNTIME_CLASS(APlayerChunk)))
	{ // Flying bloody skull or flying ice chunk
		player->viewheight = 6 * FRACUNIT;
		player->deltaviewheight = 0;
		if (onground)
		{
			if (player->mo->pitch > -(int)ANGLE_1*19)
			{
				lookDelta = (-(int)ANGLE_1*19 - player->mo->pitch) / 8;
				player->mo->pitch += lookDelta;
			}
		}
	}
	else if (!(player->mo->flags & MF_ICECORPSE))
	{ // Fall to ground (if not frozen)
		player->deltaviewheight = 0;
		if (player->viewheight > 6*FRACUNIT)
		{
			player->viewheight -= FRACUNIT;
		}
		if (player->viewheight < 6*FRACUNIT)
		{
			player->viewheight = 6*FRACUNIT;
		}
		if (player->mo->pitch < 0)
		{
			player->mo->pitch += ANGLE_1*3;
		}
		else if (player->mo->pitch > 0)
		{
			player->mo->pitch -= ANGLE_1*3;
		}
		if (abs(player->mo->pitch) < ANGLE_1*3)
		{
			player->mo->pitch = 0;
		}
	}
	P_CalcHeight (player);
		
	if (player->attacker && player->attacker != player->mo)
	{ // Watch killer
		dir = P_FaceMobj (player->mo, player->attacker, &delta);
		if (delta < ANGLE_1*10)
		{ // Looking at killer, so fade damage and poison counters
			if (player->damagecount)
			{
				player->damagecount--;
			}
			if (player->poisoncount)
			{
				player->poisoncount--;
			}
		}
		delta /= 8;
		if (delta > ANGLE_1*5)
		{
			delta = ANGLE_1*5;
		}
		if (dir)
		{ // Turn clockwise
			player->mo->angle += delta;
		}
		else
		{ // Turn counter clockwise
			player->mo->angle -= delta;
		}
	}
	else
	{
		if (player->damagecount)
		{
			player->damagecount--;
		}
		if (player->poisoncount)
		{
			player->poisoncount--;
		}
	}		

	// [BC] Respawning is server-side.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return;
	}

	// [BB] If lives are limited and the game is in progess, possibly put the player in dead spectator mode.
	if ( GAMEMODE_AreLivesLimited ( ) && GAMEMODE_IsGameInProgress ( ) )
	{
		if ( level.time >= player->respawn_time )
		{
			// If a player got telefragged at the beginning of a LMS or survival round, don't
			// penalize them for it.
			if ( player->bSpawnTelefragged )
			{
				player->bSpawnTelefragged = false;

				player->cls = NULL;		// Force a new class if the player is using a random class
				player->playerstate = ( NETWORK_GetState( ) != NETSTATE_SINGLE ) ? PST_REBORN : PST_ENTER;
				if (player->mo->special1 > 2)
				{
					player->mo->special1 = 0;
				}

				return;
			}
			else
			{
				// [BB] No lives left, make this player a dead spectator.
				if ( player->ulLivesLeft == 0 )
				{
					PLAYER_SetSpectator( player, false, true );

					// Tell the other players to mark this player as a spectator.
					if ( NETWORK_GetState( ) == NETSTATE_SERVER )
						SERVERCOMMANDS_PlayerIsSpectator( player - players );
				}
			}
		}

		// [BB] No lives left, no need to do anything else in this function.
		if ( player->ulLivesLeft == 0 )
			return;
	}

	if ( level.time >= player->respawn_time )
	{
		if (((( player->cmd.ucmd.buttons & BT_USE ) || ( ( player->userinfo.bRespawnonfire == true ) && ( player->cmd.ucmd.buttons & BT_ATTACK ) && (( player->oldbuttons & BT_ATTACK ) == false ))) || 
			(( deathmatch || teamgame || alwaysapplydmflags ) &&
			( dmflags & DF_FORCE_RESPAWN ))) && !(dmflags2 & DF2_NO_RESPAWN) )
		{
			player->cls = NULL;		// Force a new class if the player is using a random class
			player->playerstate = ( ( NETWORK_GetState( ) != NETSTATE_SINGLE ) || (level.flags2 & LEVEL2_ALLOWRESPAWN)) ? PST_REBORN : PST_ENTER;
			if (player->mo->special1 > 2)
			{
				player->mo->special1 = 0;
			}
			// [BB] The player will be reborn, so take away one life, but only if the game is already in progress.
			if ( ( player->ulLivesLeft > 0 ) && GAMEMODE_IsGameInProgress ( ) )
			{
				PLAYER_SetLivesLeft ( player, player->ulLivesLeft - 1 );
			}
		}
//		else if ( player->pSkullBot )
//		{
//			Printf( "WARNING! Bot %s dead and not hitting repawn in state %s!\n", player->userinfo.netname, player->pSkullBot->m_ScriptData.szStateName[player->pSkullBot->m_ScriptData.lCurrentStateIdx] );
//		}
	}
/* [BB] For some reason Skulltag does this a little differently above. Todo: Unify this, to make merging ZDoom updates easier.
	if ((player->cmd.ucmd.buttons & BT_USE ||
		((multiplayer || alwaysapplydmflags) && (dmflags & DF_FORCE_RESPAWN))) && !(dmflags2 & DF2_NO_RESPAWN))
	{
		if (level.time >= player->respawn_time || ((player->cmd.ucmd.buttons & BT_USE) && !player->isbot))
		{
			player->cls = NULL;		// Force a new class if the player is using a random class
			player->playerstate = (multiplayer || (level.flags2 & LEVEL2_ALLOWRESPAWN)) ? PST_REBORN : PST_ENTER;
			if (player->mo->special1 > 2)
			{
				player->mo->special1 = 0;
			}
		}
	}
*/
}



//*****************************************************************************
//
void PLAYER_JoinGameFromSpectators( int iChar )
{
	if ( iChar != 'y' )
		return;

	// [BB] No joining while playing a demo.
	if ( CLIENTDEMO_IsPlaying( ) == true )
	{
		Printf ( "You can't join during demo playback.\n" );
		return;
	}

	// Inform the server that we wish to join the current game.
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
	{
		UCVarValue	Val;

		Val = cl_joinpassword.GetGenericRep( CVAR_String );

		CLIENTCOMMANDS_RequestJoin( Val.String );
		return;
	}

	// [BB] Already joined, the player can't join again.
	if ( players[consoleplayer].bSpectating == false )
		return;

	// [BB] If players aren't allowed to join at the moment, just put the consoleplayer in line.
	if ( GAMEMODE_PreventPlayersFromJoining() )
	{
		JOINQUEUE_AddConsolePlayer ( teams.Size( ) );
		return;
	}

	// [BB] In single player, allow the player to switch its class when changing from spectator to player.
	if ( ( NETWORK_GetState( ) == NETSTATE_SINGLE ) || ( NETWORK_GetState( ) == NETSTATE_SINGLE_MULTIPLAYER ) )
		SinglePlayerClass[consoleplayer] = players[consoleplayer].userinfo.PlayerClass;

	players[consoleplayer].playerstate = PST_ENTERNOINVENTORY;
	players[consoleplayer].bSpectating = false;
	players[consoleplayer].bDeadSpectator = false;
	players[consoleplayer].camera = players[consoleplayer].mo;
	Printf( "%s \\c-joined the game.\n", players[consoleplayer].userinfo.netname );

	// [BB] If players are supposed to be on teams, select one for the player now.
	if ( GAMEMODE_GetFlags( GAMEMODE_GetCurrentMode( )) & GMF_PLAYERSONTEAMS )
		PLAYER_SetTeam( &players[consoleplayer], TEAM_ChooseBestTeamForPlayer( ), true );
}

CCMD( join ) {
	// [BB] The server can't use this.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		Printf ( "CCMD join can't be used on the server\n" );
		return;
	}

	PLAYER_JoinGameFromSpectators('y');
}

//*****************************************************************************
//
bool PLAYER_Responder( event_t *pEvent )
{
	// [RC] This isn't used ATM.
	return false;
}

//----------------------------------------------------------------------------
//
// PROC P_CrouchMove
//
//----------------------------------------------------------------------------

void P_CrouchMove(player_t * player, int direction)
{
	fixed_t defaultheight = player->mo->GetDefault()->height;
	fixed_t savedheight = player->mo->height;
	fixed_t crouchspeed = direction * CROUCHSPEED;
	fixed_t oldheight = player->viewheight;

	player->crouchdir = (signed char) direction;
	player->crouchfactor += crouchspeed;

	// check whether the move is ok
	player->mo->height = FixedMul(defaultheight, player->crouchfactor);
	if (!P_TryMove(player->mo, player->mo->x, player->mo->y, false, false))
	{
		player->mo->height = savedheight;
		if (direction > 0)
		{
			// doesn't fit
			player->crouchfactor -= crouchspeed;
			return;
		}
	}
	player->mo->height = savedheight;

	player->crouchfactor = clamp<fixed_t>(player->crouchfactor, FRACUNIT/2, FRACUNIT);
	player->viewheight = FixedMul(player->mo->ViewHeight, player->crouchfactor);
	player->crouchviewdelta = player->viewheight - player->mo->ViewHeight;

	// Check for eyes going above/below fake floor due to crouching motion.
	P_CheckFakeFloorTriggers(player->mo, player->mo->z + oldheight, true);
}

//----------------------------------------------------------------------------
//
// PROC P_PlayerThink
//
//----------------------------------------------------------------------------

CVAR( Bool, cl_disallowfullpitch, false, CVAR_ARCHIVE )

void P_PlayerThink (player_t *player, ticcmd_t *pCmd)
{
	ticcmd_t *cmd;

	if (player->mo == NULL)
	{
		// Just print an error if a bot tried to spawn.
		if ( player->pSkullBot )
		{
			Printf( "%s \\c-left: No player %td start\n", player->userinfo.netname, player - players + 1 );
			BOTS_RemoveBot( player - players, false );
			return;
		}
		else
		{
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			{
//				// Don't really bitch here, because this tends to happen if people use the "map"
//				// rcon command.
				Printf( "No player %td start\n", player - players + 1 );
				SERVER_DisconnectClient( player - players, true, true );
				return;
			}
			else
			{
				if ((( NETWORK_GetState( ) == NETSTATE_CLIENT ) || ( CLIENTDEMO_IsPlaying( ))) &&
					(( player - players ) != consoleplayer ))
				{
					//PLAYER_SetSpectator(player, true, false);
					Printf( "P_PlayerThink: No body for player %td!\n", player - players + 1 );
					return;
				}
				else
					I_Error ("No player %td start\n", player - players + 1);
			}
		}
	}

/*
	if (debugfile && !(player->cheats & CF_PREDICTING))
	{
		fprintf (debugfile, "tic %d for pl %td: (%d, %d, %d, %u) b:%02x p:%d y:%d f:%d s:%d u:%d\n",
			gametic, player-players, player->mo->x, player->mo->y, player->mo->z,
			player->mo->angle>>ANGLETOFINESHIFT, player->cmd.ucmd.buttons,
			player->cmd.ucmd.pitch, player->cmd.ucmd.yaw, player->cmd.ucmd.forwardmove,
			player->cmd.ucmd.sidemove, player->cmd.ucmd.upmove);
	}
*/
	if ( CLIENT_PREDICT_IsPredicting( ) == false )
	{
		// [RH] Zoom the player's FOV
		float desired = player->DesiredFOV;
		// Adjust FOV using on the currently held weapon.
		if (player->playerstate != PST_DEAD &&		// No adjustment while dead.
			player->ReadyWeapon != NULL &&			// No adjustment if no weapon.
			player->ReadyWeapon->FOVScale != 0)		// No adjustment if the adjustment is zero.
		{
			// A negative scale is used top prevent G_AddViewAngle/G_AddViewPitch
			// from scaling with the FOV scale.
			desired *= fabs(player->ReadyWeapon->FOVScale);
		}
		if (player->FOV != desired)
		{
			if (fabsf (player->FOV - desired) < 7.f)
			{
				player->FOV = desired;
			}
			else
			{
				float zoom = MAX(7.f, fabsf(player->FOV - desired) * 0.025f);
				if (player->FOV > desired)
				{
					player->FOV = player->FOV - zoom;
				}
				else
				{
					player->FOV = player->FOV + zoom;
				}
			}
		}
	}

	// Allow the spectator to do a few things, then break out.
	if ( player->bSpectating )
	{
		int	look;

		// Lower the weapon completely.
		P_SetPsprite( player, ps_weapon, NULL );

		// [RH] Look up/down stuff
		cmd = &player->cmd;
		look = cmd->ucmd.pitch << 16;

		// Allow the player to fly around when a spectator.
		player->mo->flags |= MF_NOGRAVITY;
		player->mo->flags2 |= MF2_FLY;

		// No-clip cheat
		if (player->cheats & CF_NOCLIP)
			player->mo->flags |= MF_NOCLIP;
		else
			player->mo->flags &= ~MF_NOCLIP;

		// The player's view pitch is clamped between -32 and +56 degrees,
		// which translates to about half a screen height up and (more than)
		// one full screen height down from straight ahead when view panning
		// is used.
		if (look)
		{
			if (look == -32768 << 16)
			{ // center view
				player->mo->pitch = 0;
			}
			else
			{
				player->mo->pitch -= look;
				if (look > 0)
				{ 
					// look up
					if (( currentrenderer ) && ( cl_disallowfullpitch == false ))
					{
						if (player->mo->pitch < -ANGLE_1*90)
							player->mo->pitch = -ANGLE_1*90;
					}
					else
					{
						// [BB] The user can restore ZDoom's freelook limit.
						const fixed_t pitchLimit = -ANGLE_1*( cl_oldfreelooklimit ? 32 : 56 );
						if (player->mo->pitch < pitchLimit)
							player->mo->pitch = pitchLimit;
					}
				}
				else
				{ 
					// look down
					if (( currentrenderer ) && ( cl_disallowfullpitch == false ))
					{
						if (player->mo->pitch > ANGLE_1*90)
							player->mo->pitch = ANGLE_1*90;
					}
					else
					{
						if (player->mo->pitch > ANGLE_1*56)
							player->mo->pitch = ANGLE_1*56;
					}
				}
			}
		}

		// Update player's velocity by input.
		if ( player->mo->reactiontime )
			player->mo->reactiontime--;
		else
			P_MovePlayer( player, &player->cmd );

		// [RH] check for jump
		// [Dusk] Apply cl_spectatormove
		if ( cmd->ucmd.buttons & BT_JUMP )
			player->mo->momz = 3 * cl_spectatormove * FRACUNIT;

		if ( cmd->ucmd.upmove == -32768 )
		{ // Only land if in the air
			if ((player->mo->flags2 & MF2_FLY) && player->mo->waterlevel < 2)
			{
				player->mo->flags2 &= ~MF2_FLY;
				player->mo->flags &= ~MF_NOGRAVITY;
			}
		}
		// [Dusk] Apply cl_spectatormove here
		else if ( cmd->ucmd.upmove != 0 )
			player->mo->momz = (cmd->ucmd.upmove << 9) * cl_spectatormove;

		// Calculate player's viewheight.
		P_CalcHeight( player );

		// If this is a bot, run its logic.
		if ( player->pSkullBot )
			player->pSkullBot->Tick( );

		// [RH] Check for fast turn around
		if (cmd->ucmd.buttons & BT_TURN180 && !(player->oldbuttons & BT_TURN180))
		{
			player->turnticks = TURN180_TICKS;
		}

		// Done with spectator specific logic.
		return;
	}

	if ( CLIENT_PREDICT_IsPredicting( ) == false )
	{
		if (player->inventorytics)
		{
			player->inventorytics--;
		}
	}
	// No-clip cheat
	if (player->cheats & CF_NOCLIP)
	{
		player->mo->flags |= MF_NOCLIP;
	}
	else
	{
		player->mo->flags &= ~MF_NOCLIP;
	}

	// If we're predicting, use the ticcmd we pass in.
	if ( CLIENT_PREDICT_IsPredicting( ))
		cmd = pCmd;
	else
		cmd = &player->cmd;

	// Make unmodified copies for ACS's GetPlayerInput.
	player->original_oldbuttons = player->original_cmd.buttons;
	player->original_cmd = cmd->ucmd;

	if (player->mo->flags & MF_JUSTATTACKED &&
		(( NETWORK_GetState( ) == NETSTATE_CLIENT ) || ( CLIENTDEMO_IsPlaying( ))))
	{ // Chainsaw/Gauntlets attack auto forward motion
		cmd->ucmd.yaw = 0;
		cmd->ucmd.forwardmove = 0xc800/2;
		cmd->ucmd.sidemove = 0;
		player->mo->flags &= ~MF_JUSTATTACKED;
	}

	// [BB] Why should a predicting client ignore CF_TOTALLYFROZEN and CF_FROZEN?
	//if ( CLIENT_PREDICT_IsPredicting( ) == false )
	{
		// [RH] Being totally frozen zeros out most input parameters.
		if (player->cheats & CF_TOTALLYFROZEN || gamestate == GS_TITLELEVEL)
		{
			if (gamestate == GS_TITLELEVEL)
			{
				cmd->ucmd.buttons = 0;
			}
			else
			{
				cmd->ucmd.buttons &= BT_USE;
			}
			cmd->ucmd.pitch = 0;
			cmd->ucmd.yaw = 0;
			cmd->ucmd.roll = 0;
			cmd->ucmd.forwardmove = 0;
			cmd->ucmd.sidemove = 0;
			cmd->ucmd.upmove = 0;
			player->turnticks = 0;
		}
		else if (player->cheats & CF_FROZEN)
		{
			cmd->ucmd.forwardmove = 0;
			cmd->ucmd.sidemove = 0;
			cmd->ucmd.upmove = 0;
		}
	}

	// If this is a bot, run its logic.
	if ( player->pSkullBot )
		player->pSkullBot->Tick( );

	// [BB] Since the game is currently suspended, prevent the player from doing anything.
	// Note: This needs to be done after ticking the bot, otherwise the bot could still act.
	if ( GAME_GetEndLevelDelay( ) )
		memset( cmd, 0, sizeof( ticcmd_t ));

	// Handle crouching
	if (player->cmd.ucmd.buttons & BT_JUMP)
	{
		player->cmd.ucmd.buttons &= ~BT_CROUCH;
	}
	// [BC] Don't do this for clients other than ourself in client mode.
	// [BB] Also, don't do this while predicting.
	if ((( NETWORK_GetState( ) != NETSTATE_CLIENT ) && ( CLIENTDEMO_IsPlaying( ) == false )) || ( (( player - players ) == consoleplayer ) && ( CLIENT_PREDICT_IsPredicting( ) == false ) ))
	{
		if (player->morphTics == 0 && player->health > 0 && level.IsCrouchingAllowed())
		{
			if (!(player->cheats & CF_TOTALLYFROZEN))
			{
				int crouchdir = player->crouching;
			
				if (crouchdir == 0)
				{
					crouchdir = (player->cmd.ucmd.buttons & BT_CROUCH)? -1 : 1;
				}
				else if (player->cmd.ucmd.buttons & BT_CROUCH)
				{
					player->crouching=0;
				}
				if (crouchdir == 1 && player->crouchfactor < FRACUNIT &&
					player->mo->z + player->mo->height < player->mo->ceilingz)
				{
					P_CrouchMove(player, 1);
				}
				else if (crouchdir == -1 && player->crouchfactor > FRACUNIT/2)
				{
					P_CrouchMove(player, -1);
				}
			}
		}
		else
		{
			player->Uncrouch();
		}
	}

	player->crouchoffset = -FixedMul(player->mo->ViewHeight, (FRACUNIT - player->crouchfactor));


	if (player->playerstate == PST_DEAD)
	{
		player->Uncrouch();
		P_DeathThink (player);

		// [BC] Update oldbuttons.
		player->oldbuttons = player->cmd.ucmd.buttons;
		return;
	}

	if (player->jumpTics)
	{
		player->jumpTics--;
	}
	if (player->morphTics)// && !(player->cheats & CF_PREDICTING))
	{
		player->mo->MorphPlayerThink ();
	}

	// [RH] Look up/down stuff
	if (!level.IsFreelookAllowed())
	{
		player->mo->pitch = 0;
	}
	else
	{
		// Servers read in the pitch value. It is not calculated.
		if (( NETWORK_GetState( ) != NETSTATE_SERVER ) || ( player->pSkullBot != NULL ))
		{
			int look = cmd->ucmd.pitch << 16;

			// The player's view pitch is clamped between -32 and +56 degrees,
			// which translates to about half a screen height up and (more than)
			// one full screen height down from straight ahead when view panning
			// is used.
			if (look)
			{
				if (look == -32768 << 16)
				{ // center view
					player->mo->pitch = 0;
				}
				else
				{
					player->mo->pitch -= look;
					if (look > 0)
					{
						// look up
						if (( currentrenderer ) && ( cl_disallowfullpitch == false ))
						{
							if (player->mo->pitch < -ANGLE_1*90)
								player->mo->pitch = -ANGLE_1*90;
						}
						else
						{
							// [BB] The user can restore ZDoom's freelook limit.
							const fixed_t pitchLimit = -ANGLE_1*( cl_oldfreelooklimit ? 32 : 56 );
							if (player->mo->pitch < pitchLimit)
								player->mo->pitch = pitchLimit;
						}
					}
					else
					{
						// look down
						if (( currentrenderer ) && ( cl_disallowfullpitch == false ))
						{
							if (player->mo->pitch > ANGLE_1*90)
								player->mo->pitch = ANGLE_1*90;
						}
						else
						{
							if (player->mo->pitch > ANGLE_1*56)
								player->mo->pitch = ANGLE_1*56;
						}
					}
				}
			}
		}
	}

	// [RH] Check for fast turn around
	if (cmd->ucmd.buttons & BT_TURN180 && !(player->oldbuttons & BT_TURN180))
	{
		player->turnticks = TURN180_TICKS;
	}

	// Handle movement
	if (player->mo->reactiontime)
	{ // Player is frozen
			player->mo->reactiontime--;
	}
	else
	{
		P_MovePlayer (player, cmd);

		if (cmd->ucmd.upmove == -32768)
		{ // Only land if in the air
			if ((player->mo->flags & MF_NOGRAVITY) && player->mo->waterlevel < 2)
			{
				//player->mo->flags2 &= ~MF2_FLY;
				player->mo->flags &= ~MF_NOGRAVITY;
			}
		}
		else if (cmd->ucmd.upmove != 0)
		{
			// Clamp the speed to some reasonable maximum.
			int magnitude = abs (cmd->ucmd.upmove);
			if (magnitude > 0x300)
			{
				cmd->ucmd.upmove = ksgn (cmd->ucmd.upmove) * 0x300;
			}
			if (player->mo->waterlevel >= 2 || (player->mo->flags2 & MF2_FLY))
			{
				player->mo->momz = cmd->ucmd.upmove << 9;
				if (player->mo->waterlevel < 2 && !(player->mo->flags & MF_NOGRAVITY))
				{
					player->mo->flags2 |= MF2_FLY;
					player->mo->flags |= MF_NOGRAVITY;
					if (player->mo->momz <= -39*FRACUNIT)
					{ // Stop falling scream
						S_StopSound (player->mo, CHAN_VOICE);
					}
				}
			}
			else if (cmd->ucmd.upmove > 0)// && !(player->cheats & CF_PREDICTING))
			{
				AInventory *fly = player->mo->FindInventory (NAME_ArtiFly);
				if (fly != NULL)
				{
					// [BB] The server tells the client that it used the fly item.
					if( NETWORK_GetState( ) != NETSTATE_CLIENT )
						player->mo->UseInventory (fly);
				}
			}
		}
	}

	P_CalcHeight (player);

	if ( CLIENT_PREDICT_IsPredicting( ) == false )
	{
		P_PlayerOnSpecial3DFloor (player);
		if (player->mo->Sector->special || player->mo->Sector->damage)
		{
			P_PlayerInSpecialSector (player);
		}
		P_PlayerOnSpecialFlat (player, P_GetThingFloorType (player->mo));
		if (player->mo->momz <= -35*FRACUNIT &&
			player->mo->momz >= -40*FRACUNIT && !player->morphTics &&
			player->mo->waterlevel == 0)
		{
			int id = S_FindSkinnedSound (player->mo, "*falling");
			if (id != 0 && !S_IsActorPlayingSomething (player->mo, CHAN_VOICE, id))
			{
				S_Sound (player->mo, CHAN_VOICE, id, 1, ATTN_NORM);
			}
		}
		// check for use
		if ((cmd->ucmd.buttons & BT_USE) && !(player->oldbuttons & BT_USE))
		{
			P_UseLines (player);
			//P_UseItems (player);
		}
		// Morph counter
		if (player->morphTics)
		{
			if (player->chickenPeck)
			{ // Chicken attack counter
				player->chickenPeck -= 3;
			}
			if (!--player->morphTics)
			{ // Attempt to undo the chicken/pig
				P_UndoPlayerMorph (player, player);
			}
		}

		// Cycle psprites
		P_MovePsprites (player);

		// Other Counters
		if (player->damagecount)
			player->damagecount--;

		if (player->bonuscount)
			player->bonuscount--;

		if (player->hazardcount)
		{
			player->hazardcount--;
			// [BB] The clients only tick down, the server handles the damage.
			if ( NETWORK_InClientMode( ) == false )
			{
				if (!(level.time & 31) && player->hazardcount > 16*TICRATE)
					P_DamageMobj (player->mo, NULL, NULL, 5, NAME_Slime);
			}
		}

		if (player->poisoncount && !(level.time & 15))
		{
			player->poisoncount -= 5;
			if (player->poisoncount < 0)
			{
				player->poisoncount = 0;
			}
			P_PoisonDamage (player, player->poisoner, 1, true);
		}

		// [BC] Don't do the following block in client mode.
		if (( NETWORK_GetState( ) != NETSTATE_CLIENT ) &&
			( CLIENTDEMO_IsPlaying( ) == false ))
		{
			// [BC] Apply regeneration.
			if (( level.time & 31 ) == 0 && ( player->cheats & CF_REGENERATION ) && ( player->health ))
			{
				if ( P_GiveBody( player->mo, 5 ))
				{
					S_Sound( player->mo, CHAN_ITEM, "misc/i_pkup", 1, ATTN_NORM );

					// [BC] If we're the server, send out the health change, and play the
					// health sound.
					if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					{
						SERVERCOMMANDS_SetPlayerHealth( player - players );
						SERVERCOMMANDS_SoundActor( player->mo, CHAN_ITEM, "misc/i_pkup", 1, ATTN_NORM );
					}
				}
			}

			// Apply degeneration.
			if ( dmflags2 & DF2_YES_DEGENERATION )
			{
				if ((( level.time & 127 ) == 0 ) && ( player->health > ( deh.StartHealth + player->lMaxHealthBonus )))
				{
					player->health--;
					player->mo->health--;

					// [BC] If we're the server, send out the health change.
					if ( NETWORK_GetState( ) == NETSTATE_SERVER )
						SERVERCOMMANDS_SetPlayerHealth( player - players );
				}
				/* [BB] This is ZDoom's way of degeneration. Should we use this?
				if ((level.time % TICRATE) == 0 && player->health > deh.MaxHealth)
				{
					if (player->health - 5 < deh.MaxHealth)
						player->health = deh.MaxHealth;
					else
						player->health--;

					player->mo->health = player->health;
				}
				*/
			}

		}

		// Handle air supply
		//if (level.airsupply > 0)
		{
			if (player->mo->waterlevel < 3 ||
				(player->mo->flags2 & MF2_INVULNERABLE) ||
				(player->cheats & CF_GODMODE))
			{
				player->mo->ResetAirSupply ();
			}
			else if (player->air_finished <= level.time && !(level.time & 31))
			{
				// [BB] The server handles damaging the players.
				if (( NETWORK_GetState( ) != NETSTATE_CLIENT ) &&
					( CLIENTDEMO_IsPlaying( ) == false ))
				{
					P_DamageMobj (player->mo, NULL, NULL, 2 + ((level.time-player->air_finished)/TICRATE), NAME_Drowning);
				}
			}
		}
	}
}

/*
void P_PredictPlayer (player_t *player)
{
	int maxtic;

	if (cl_noprediction ||
		singletics ||
		demoplayback ||
		player->mo == NULL ||
		player != &players[consoleplayer] ||
		player->playerstate != PST_LIVE ||
		!netgame ||
		player->morphTics ||*
		(player->cheats & CF_PREDICTING))
	{
		return;
	}

	maxtic = maketic;

	if (gametic == maxtic)
	{
		return;
	}

	// Save original values for restoration later
	PredictionPlayerBackup = *player;

	AActor *act = player->mo;
	memcpy (PredictionActorBackup, &act->x, sizeof(AActor)-((BYTE *)&act->x-(BYTE *)act));

	act->flags &= ~MF_PICKUP;
	act->flags2 &= ~MF2_PUSHWALL;
	player->cheats |= CF_PREDICTING;

	// The ordering of the touching_sectorlist needs to remain unchanged
	msecnode_t *mnode = act->touching_sectorlist;
	PredictionTouchingSectorsBackup.Clear ();

	while (mnode != NULL)
	{
		PredictionTouchingSectorsBackup.Push (mnode->m_sector);
		mnode = mnode->m_tnext;
	}

	// Blockmap ordering also needs to stay the same, so unlink the block nodes
	// without releasing them. (They will be used again in P_UnpredictPlayer).
	FBlockNode *block = act->BlockNode;

	while (block != NULL)
	{
		if (block->NextActor != NULL)
		{
			block->NextActor->PrevActor = block->PrevActor;
		}
		*(block->PrevActor) = block->NextActor;
		block = block->NextBlock;
	}
	act->BlockNode = NULL;


	for (int i = gametic; i < maxtic; ++i)
	{
		player->cmd = localcmds[i % LOCALCMDTICS];
		P_PlayerThink (player);
		player->mo->Tick ();
	}
}
*/

extern msecnode_t *P_AddSecnode (sector_t *s, AActor *thing, msecnode_t *nextnode);

/*
void P_UnPredictPlayer ()
{
	player_t *player = &players[consoleplayer];

	if (player->cheats & CF_PREDICTING)
	{
		AActor *act = player->mo;

		*player = PredictionPlayerBackup;

		act->UnlinkFromWorld ();
		memcpy (&act->x, PredictionActorBackup, sizeof(AActor)-((BYTE *)&act->x-(BYTE *)act));

		// Make the sector_list match the player's touching_sectorlist before it got predicted.
		P_DelSeclist (sector_list);
		sector_list = NULL;
		for (unsigned int i = PredictionTouchingSectorsBackup.Size (); i-- > 0; )
		{
			sector_list = P_AddSecnode (PredictionTouchingSectorsBackup[i], act, sector_list);
		}

		// The blockmap ordering needs to remain unchanged, too. Right now, act has the right
		// pointers, so temporarily set its MF_NOBLOCKMAP flag so that LinkToWorld() does not
		// mess with them.
		act->flags |= MF_NOBLOCKMAP;
		act->LinkToWorld ();
		act->flags &= ~MF_NOBLOCKMAP;

		// Now fix the pointers in the blocknode chain
		FBlockNode *block = act->BlockNode;

		while (block != NULL)
		{
			*(block->PrevActor) = block;
			if (block->NextActor != NULL)
			{
				block->NextActor->PrevActor = &block->NextActor;
			}
			block = block->NextBlock;
		}
	}
}
*/

void player_t::Serialize (FArchive &arc)
{
	int i;

	arc << cls
		<< mo
		<< camera
		<< playerstate
		<< cmd
		<< userinfo
		<< DesiredFOV << FOV
		<< viewz
		<< viewheight
		<< deltaviewheight
		<< bob
		<< momx
		<< momy
		<< centering
		<< health
		<< inventorytics
		<< backpack
		<< fragcount
		<< ReadyWeapon << PendingWeapon
		<< cheats
		<< refire
		<< killcount
		<< itemcount
		<< secretcount
		<< damagecount
		<< bonuscount
		<< hazardcount
		<< poisoncount
		<< poisoner
		<< attacker
		<< extralight
		<< fixedcolormap
		<< morphTics
		<< MorphedPlayerClass
		<< MorphStyle
		<< MorphExitFlash
		<< PremorphWeapon
		<< chickenPeck
		<< jumpTics
		<< respawn_time
		<< air_finished
		<< turnticks
		<< oldbuttons
		<< BlendR
		<< BlendG
		<< BlendB
		<< BlendA
		<< accuracy << stamina
		// [BB] Skulltag additions - start
		<< bOnTeam
		<< (DWORD &)ulTeam
		<< bChatting
		<< bInConsole
		<< (DWORD &)ulRailgunShots
		<< (DWORD &)lMaxHealthBonus
		// [BB] Skulltag additions - end
		<< LogText
		<< ConversationNPC
		<< ConversationPC
		<< ConversationNPCAngle
		<< ConversationFaceTalker;
	for (i = 0; i < NUMPSPRITES; i++)
		arc << psprites[i];

	arc << CurrentPlayerClass;

	arc << crouchfactor
		<< crouching 
		<< crouchdir
		<< crouchviewdelta
		<< original_cmd
		<< original_oldbuttons;

	// [BL] is the player unarmed?
	arc << bUnarmed;

	if (arc.IsLoading ())
	{
		// If the player reloaded because they pressed +use after dying, we
		// don't want +use to still be down after the game is loaded.
		oldbuttons = ~0;
		original_oldbuttons = ~0;
	}
}
