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
//		Handling interactions (i.e., collisions).
//
//-----------------------------------------------------------------------------




// Data.
#include "doomdef.h"
#include "gstrings.h"

#include "doomstat.h"

#include "m_random.h"
#include "i_system.h"
#include "announcer.h"

#include "am_map.h"

#include "c_console.h"
#include "c_dispatch.h"

#include "p_local.h"

#include "p_lnspec.h"
#include "p_effect.h"
#include "p_acs.h"

#include "ravenshared.h"
#include "a_hexenglobal.h"
#include "a_sharedglobal.h"
#include "a_pickups.h"
#include "gi.h"
#include "templates.h"
#include "sbar.h"
#include "s_sound.h"
#include "g_level.h"
#include "d_net.h"
#include "d_netinf.h"
// [BB] New #includes.
#include "deathmatch.h"
#include "duel.h"
#include "d_event.h"
#include "medal.h"
#include "network.h"
#include "sv_commands.h"
#include "g_game.h"
#include "gamemode.h"
#include "cl_main.h"
#include "joinqueue.h"
#include "lastmanstanding.h"
#include "scoreboard.h"
#include "cooperative.h"
#include "invasion.h"
#include "survival.h"
#include "team.h"
#include "cl_commands.h"
#include "cl_demo.h"
#include "win32/g15/g15.h"
#include "r_translate.h"
#include "p_enemy.h"

// [BC] Ugh.
void SERVERCONSOLE_UpdatePlayerInfo( LONG lPlayer, ULONG ulUpdateFlags );
void SERVERCONSOLE_UpdateScoreboard( void );

static FRandom pr_obituary ("Obituary");
static FRandom pr_botrespawn ("BotRespawn");
static FRandom pr_killmobj ("ActorDie");
static FRandom pr_damagemobj ("ActorTakeDamage");
static FRandom pr_lightning ("LightningDamage");
static FRandom pr_poison ("PoisonDamage");
static FRandom pr_switcher ("SwitchTarget");

EXTERN_CVAR (Bool, show_obituaries)


FName MeansOfDeath;
bool FriendlyFire;

//
// GET STUFF
//

//
// P_TouchSpecialThing
//
void P_TouchSpecialThing (AActor *special, AActor *toucher)
{
	fixed_t delta = special->z - toucher->z;

	// The pickup is at or above the toucher's feet OR
	// The pickup is below the toucher.
	if (delta > toucher->height || delta < MIN(-32*FRACUNIT, -special->height))
	{ // out of reach
		return;
	}

	// Dead thing touching.
	// Can happen with a sliding player corpse.
	if (toucher->health <= 0)
		return;

	// [BC] Don't allow non-players to touch special things.
	if ( toucher->player == NULL )
		return;

	// [BC] Don't allow spectating players to pickup items.
	if ( toucher->player->bSpectating )
		return;

	special->Touch (toucher);
}


// [RH]
// SexMessage: Replace parts of strings with gender-specific pronouns
//
// The following expansions are performed:
//		%g -> he/she/it
//		%h -> him/her/it
//		%p -> his/her/its
//		%o -> other (victim)
//		%k -> killer
//
void SexMessage (const char *from, char *to, int gender, const char *victim, const char *killer)
{
	static const char *genderstuff[3][3] =
	{
		{ "he",  "him", "his" },
		{ "she", "her", "her" },
		{ "it",  "it",  "its" }
	};
	static const int gendershift[3][3] =
	{
		{ 2, 3, 3 },
		{ 3, 3, 3 },
		{ 2, 2, 3 }
	};
	const char *subst = NULL;

	do
	{
		if (*from != '%')
		{
			*to++ = *from;
		}
		else
		{
			int gendermsg = -1;
			
			switch (from[1])
			{
			case 'g':	gendermsg = 0;	break;
			case 'h':	gendermsg = 1;	break;
			case 'p':	gendermsg = 2;	break;
			case 'o':	subst = victim;	break;
			case 'k':	subst = killer;	break;
			}
			if (subst != NULL)
			{
				size_t len = strlen (subst);
				memcpy (to, subst, len);
				to += len;
				from++;
				subst = NULL;
			}
			else if (gendermsg < 0)
			{
				*to++ = '%';
			}
			else
			{
				strcpy (to, genderstuff[gender][gendermsg]);
				to += gendershift[gender][gendermsg];
				from++;
			}
		}
	} while (*from++);
}

// [RH]
// ClientObituary: Show a message when a player dies
//
// [BC] Allow passing in of the MOD so clients can use this function too.
void ClientObituary (AActor *self, AActor *inflictor, AActor *attacker, FName MeansOfDeath)
{
	FName	mod;
	const char *message;
	const char *messagename;
	char gendermessage[1024];
	bool friendly;
	int  gender;
	// We enough characters for the player's name, the terminating zero, and 4 characters
	// to strip the color codes (I actually believe it's 3, but let's play it safe).
	char	szAttacker[MAXPLAYERNAME+1+4];
	char	szVictim[MAXPLAYERNAME+1+4];

	// No obituaries for non-players, voodoo dolls or when not wanted
	if (self->player == NULL || self->player->mo != self || !show_obituaries)
		return;

	gender = self->player->userinfo.gender;

	// Treat voodoo dolls as unknown deaths
	if (inflictor && inflictor->player == self->player)
		MeansOfDeath = NAME_None;

	// Must be in cooperative mode.
	if (( NETWORK_GetState( ) != NETSTATE_SINGLE ) && ( deathmatch == false ) && ( teamgame == false ))
		FriendlyFire = true;

	friendly = FriendlyFire;
	mod = MeansOfDeath;
	message = NULL;
	messagename = NULL;

	switch (mod)
	{
	case NAME_Suicide:		messagename = "OB_SUICIDE";		break;
	case NAME_Falling:		messagename = "OB_FALLING";		break;
	case NAME_Crush:		messagename = "OB_CRUSH";		break;
	case NAME_Exit:			messagename = "OB_EXIT";		break;
	case NAME_Drowning:		messagename = "OB_WATER";		break;
	case NAME_Slime:		messagename = "OB_SLIME";		break;
	case NAME_Fire:			if (attacker == NULL) messagename = "OB_LAVA";		break;
	}

	if (messagename != NULL)
		message = GStrings(messagename);

	if (attacker != NULL && message == NULL)
	{
		if (attacker == self)
		{
			// [BB] Added switch here.
			switch (mod)
			{
			// [BC] Obituaries for killing yourself with Skulltag weapons.
			case NAME_Grenade:	messagename = "OB_GRENADE_SELF";	break;
			case NAME_BFG10k:	messagename = "OB_BFG10K_SELF";		break;
			}
			if (messagename != NULL)
				message = GStrings(messagename);
			else
				message = GStrings("OB_KILLEDSELF");
		}
		else if (attacker->player == NULL)
		{
			if ((mod == NAME_Telefrag) || (mod == NAME_SpawnTelefrag))
			{
				message = GStrings("OB_MONTELEFRAG");
			}
			else if (mod == NAME_Melee)
			{
				message = attacker->GetClass()->Meta.GetMetaString (AMETA_HitObituary);
				if (message == NULL)
				{
					message = attacker->GetClass()->Meta.GetMetaString (AMETA_Obituary);
				}
			}
			else
			{
				message = attacker->GetClass()->Meta.GetMetaString (AMETA_Obituary);
			}
		}
	}

	if (message == NULL && attacker != NULL && attacker->player != NULL)
	{
		if (friendly)
		{
			// [BC] We'll do this elsewhere.
//			attacker->player->fragcount -= 2;
			self = attacker;
			gender = self->player->userinfo.gender;
			mysnprintf (gendermessage, countof(gendermessage), "OB_FRIENDLY%c", '1' + (pr_obituary() & 3));
			message = GStrings(gendermessage);
		}
		else
		{
			// [BC] NAME_SpawnTelefrag, too.
			if ((mod == NAME_Telefrag) || (mod == NAME_SpawnTelefrag)) message = GStrings("OB_MPTELEFRAG");
			// [BC] Handle Skulltag's reflection rune.
			// [RC] Moved here to fix the "[victim] was killed via [victim]'s reflection rune" bug.
			else if ( mod == NAME_Reflection )
			{
				messagename = "OB_REFLECTION";
				message = GStrings(messagename);
			}

			if (message == NULL)
			{
				if (inflictor != NULL)
				{
					message = inflictor->GetClass()->Meta.GetMetaString (AMETA_Obituary);
				}
				if (message == NULL && attacker->player->ReadyWeapon != NULL)
				{
					message = attacker->player->ReadyWeapon->GetClass()->Meta.GetMetaString (AMETA_Obituary);
				}
				if (message == NULL)
				{
					switch (mod)
					{
					case NAME_BFGSplash:	messagename = "OB_MPBFG_SPLASH";	break;
					case NAME_Railgun:		messagename = "OB_RAILGUN";			break;
					}
					if (messagename != NULL)
						message = GStrings(messagename);
				}
			}
		}
	}
	else attacker = self;	// for the message creation

	if (message != NULL && message[0] == '$') 
	{
		message=GStrings[message+1];
	}

	if (message == NULL)
	{
		message = GStrings("OB_DEFAULT");
	}

	// [BC] Stop the color codes after we display a name in the obituary string.
	if ( attacker && attacker->player )
		sprintf( szAttacker, "%s\\c-", attacker->player->userinfo.netname );
	else
		szAttacker[0] = 0;

	if ( self && self->player )
		sprintf( szVictim, "%s\\c-", self->player->userinfo.netname );
	else
		szVictim[0] = 0;

	SexMessage (message, gendermessage, gender,
		szVictim[0] ? szVictim : self->player->userinfo.netname,
		szAttacker[0] ? szAttacker : attacker->player->userinfo.netname);
	Printf (PRINT_MEDIUM, "%s\n", gendermessage);
}


//
// KillMobj
//
EXTERN_CVAR (Int, fraglimit)

static int GibHealth(AActor *actor)
{
	return -abs(
		actor->GetClass()->Meta.GetMetaInt (
			AMETA_GibHealth,
			gameinfo.gametype & GAME_DoomChex ?
				-actor->GetDefault()->health :
				-actor->GetDefault()->health/2));
}

void AActor::Die (AActor *source, AActor *inflictor)
{
	// [BB] Potentially get rid of some corpses. This isn't necessarily client-only.
	//CLIENT_RemoveMonsterCorpses();

	// [BC]
	bool	bPossessedTerminatorArtifact;

	// Handle possible unmorph on death
	bool wasgibbed = (health < GibHealth(this));
	AActor *realthis = NULL;
	int realstyle = 0;
	int realhealth = 0;
	if (P_MorphedDeath(this, &realthis, &realstyle, &realhealth))
	{
		if (!(realstyle & MORPH_UNDOBYDEATHSAVES))
		{
			if (wasgibbed)
			{
				int realgibhealth = GibHealth(realthis);
				if (realthis->health >= realgibhealth)
				{
					realthis->health = realgibhealth -1; // if morphed was gibbed, so must original be (where allowed)
				}
			}
			realthis->Die(source, inflictor);
		}
		return;
	}

	// [SO] 9/2/02 -- It's rather funny to see an exploded player body with the invuln sparkle active :) 
	effects &= ~FX_RESPAWNINVUL;

	// Also kill the alpha flicker cause by the visibility flicker.
	if ( effects & FX_VISIBILITYFLICKER )
	{
		RenderStyle = STYLE_Normal;
		effects &= ~FX_VISIBILITYFLICKER;
	}

	//flags &= ~MF_INVINCIBLE;
/*
	if (debugfile && this->player)
	{
		static int dieticks[MAXPLAYERS];
		int pnum = int(this->player-players);
		if (dieticks[pnum] == gametic)
			gametic=gametic;
		dieticks[pnum] = gametic;
		fprintf (debugfile, "died (%d) on tic %d (%s)\n", pnum, gametic,
		this->player->cheats&CF_PREDICTING?"predicting":"real");
	}
*/
	// [BC] Since the player loses his terminator status after we tell his inventory
	// that we died, check for it before doing so.
	bPossessedTerminatorArtifact = !!(( player ) && ( player->cheats2 & CF2_TERMINATORARTIFACT ));

	// [BC] Check to see if any medals need to be awarded.
	if (( player ) &&
		( NETWORK_GetState( ) != NETSTATE_CLIENT ) &&
		( CLIENTDEMO_IsPlaying( ) == false ))
	{
		if (( source ) &&
			( source->player ))
		{
			MEDAL_PlayerDied( ULONG( player - players ), ULONG( source->player - players ));
		}
		else
			MEDAL_PlayerDied( ULONG( player - players ), MAXPLAYERS );
	}

	// [RH] Notify this actor's items.
	for (AInventory *item = Inventory; item != NULL; )
	{
		AInventory *next = item->Inventory;
		item->OwnerDied();
		item = next;
	}

	if (flags & MF_MISSILE)
	{ // [RH] When missiles die, they just explode
		P_ExplodeMissile (this, NULL, NULL);
		return;
	}
	// [RH] Set the target to the thing that killed it. Strife apparently does this.
	if (source != NULL)
	{
		target = source;
	}

	flags &= ~(MF_SHOOTABLE|MF_FLOAT|MF_SKULLFLY);
	if (!(flags4 & MF4_DONTFALL)) flags&=~MF_NOGRAVITY;
	flags |= MF_DROPOFF;
	if ((flags3 & MF3_ISMONSTER) || FindState(NAME_Raise) != NULL || IsKindOf(RUNTIME_CLASS(APlayerPawn)))
	{	// [RH] Only monsters get to be corpses.
		// Objects with a raise state should get the flag as well so they can
		// be revived by an Arch-Vile. Batman Doom needs this.
		flags |= MF_CORPSE;
		// [BB] Potentially get rid of one corpse in invasion from the previous wave.
		if ( invasion )
			INVASION_RemoveMonsterCorpse();
	}
	// [RH] Allow the death height to be overridden using metadata.
	fixed_t metaheight = 0;
	if (DamageType == NAME_Fire)
	{
		metaheight = GetClass()->Meta.GetMetaFixed (AMETA_BurnHeight);
	}
	if (metaheight == 0)
	{
		metaheight = GetClass()->Meta.GetMetaFixed (AMETA_DeathHeight);
	}
	if (metaheight != 0)
	{
		height = MAX<fixed_t> (metaheight, 0);
	}
	else
	{
		height >>= 2;
	}

	// [RH] If the thing has a special, execute and remove it
	//		Note that the thing that killed it is considered
	//		the activator of the script.
	// New: In Hexen, the thing that died is the activator,
	//		so now a level flag selects who the activator gets to be.
	if (special && (!(flags & MF_SPECIAL) || (flags3 & MF3_ISMONSTER)))
	{
		LineSpecials[special] (NULL, level.flags & LEVEL_ACTOWNSPECIAL
			? this : source, false, args[0], args[1], args[2], args[3], args[4]);
		special = 0;
	}

	if (CountsAsKill())
	{
		level.killed_monsters++;
		
		// [BB] The number of remaining monsters decreased, update the invasion monster count accordingly.
		INVASION_UpdateMonsterCount ( this, true );
	}

	if (source && source->player)
	{
		// [BC] Don't do this in client mode.
		if ((CountsAsKill()) &&
			( NETWORK_GetState( ) != NETSTATE_CLIENT ) &&
			( CLIENTDEMO_IsPlaying( ) == false ))
		{ // count for intermission
			source->player->killcount++;
			
			// Update the clients with this player's updated killcount.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			{
				SERVERCOMMANDS_SetPlayerKillCount( ULONG( source->player - players ) );

				// Also, update the scoreboard.
				SERVERCONSOLE_UpdatePlayerInfo( ULONG( source->player - players ), UDF_FRAGS );
				SERVERCONSOLE_UpdateScoreboard( );
			}
		}

		// Don't count any frags at level start, because they're just telefrags
		// resulting from insufficient deathmatch starts, and it wouldn't be
		// fair to count them toward a player's score.
		if (player && ( MeansOfDeath != NAME_SpawnTelefrag ))
		{
			if ( source->player->pSkullBot )
			{
				source->player->pSkullBot->m_ulPlayerKilled = player - players;
				if (( player - players ) == static_cast<int> (source->player->pSkullBot->m_ulPlayerEnemy) )
					source->player->pSkullBot->PostEvent( BOTEVENT_ENEMY_KILLED );
			}

			if (player == source->player)	// [RH] Cumulative frag count
			{
				// [BC] Frags are server side.
				if (( NETWORK_GetState( ) != NETSTATE_CLIENT ) && ( CLIENTDEMO_IsPlaying( ) == false ))
					PLAYER_SetFragcount( player, player->fragcount - (( bPossessedTerminatorArtifact ) ? 10 : 1 ), true, true );
			}
			else
			{
				// [BB] A player just fragged another player.
				GAMEMODE_HandleEvent ( GAMEEVENT_PLAYERFRAGS, source->player->mo, static_cast<int> ( player - players ) );

				// [BC] Frags are server side.
				// [BC] Player receives 10 frags for killing the terminator!
				if (( NETWORK_GetState( ) != NETSTATE_CLIENT ) && ( CLIENTDEMO_IsPlaying( ) == false ))
				{
					if ((dmflags2 & DF2_YES_LOSEFRAG) && deathmatch)
						PLAYER_SetFragcount( player, player->fragcount - 1, true, true );

					if ( source->IsTeammate( this ))
						PLAYER_SetFragcount( source->player, source->player->fragcount - (( bPossessedTerminatorArtifact ) ? 10 : 1 ), true, true );
					else
						PLAYER_SetFragcount( source->player, source->player->fragcount + (( bPossessedTerminatorArtifact ) ? 10 : 1 ), true, true );
				}

				// [BC] Add this frag to the server's statistic module.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVER_STATISTIC_AddToTotalFrags( );

				if (source->player->morphTics)
				{ // Make a super chicken
					source->GiveInventoryType (RUNTIME_CLASS(APowerWeaponLevel2));
				}
			}

			// Play announcer sounds for amount of frags remaining.
			if ( ( GAMEMODE_GetFlags(GAMEMODE_GetCurrentMode()) & GMF_PLAYERSEARNFRAGS ) && fraglimit )
			{
				// [RH] Implement fraglimit
				// [BC] Betterized!
				// [BB] Clients may not do this.
				if ( fraglimit <= D_GetFragCount( source->player )
					 && ( NETWORK_GetState( ) != NETSTATE_CLIENT ) && ( CLIENTDEMO_IsPlaying( ) == false ) )
				{
					if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					{
						SERVER_Printf( PRINT_HIGH, "%s\n", GStrings( "TXT_FRAGLIMIT" ));
						if ( teamplay && ( source->player->bOnTeam ))
							SERVER_Printf( PRINT_HIGH, "%s \\c-wins!\n", TEAM_GetName( source->player->ulTeam ));
						else
							SERVER_Printf( PRINT_HIGH, "%s \\c-wins!\n", source->player->userinfo.netname );
					}
					else
					{
						Printf( "%s\n", GStrings( "TXT_FRAGLIMIT" ));
						if ( teamplay && ( source->player->bOnTeam ))
							Printf( "%s \\c-wins!\n", TEAM_GetName( source->player->ulTeam ));
						else
							Printf( "%s \\c-wins!\n", source->player->userinfo.netname );

						if (( duel == false ) && ( source->player - players == consoleplayer ))
							ANNOUNCER_PlayEntry( cl_announcer, "YouWin" );
					}

					// Pause for five seconds for the win sequence.
					if ( duel )
					{
						// Also, do the win sequence for the player.
						DUEL_SetLoser( player - players );
						DUEL_DoWinSequence( source->player - players );

						// Give the winner a win.
						PLAYER_SetWins( source->player, source->player->ulWins + 1 );

						GAME_SetEndLevelDelay( 5 * TICRATE );
					}
					// End the level after five seconds.
					else
					{
						char				szString[64];
						DHUDMessageFadeOut	*pMsg;

						// Just print "YOU WIN!" in single player.
						if (( NETWORK_GetState( ) == NETSTATE_SINGLE_MULTIPLAYER ) && ( players[consoleplayer].mo->CheckLocalView( source->player - players )))
							sprintf( szString, "\\cGYOU WIN!" );
						else if (( teamplay ) && ( source->player->bOnTeam ))
							sprintf( szString, "\\c%c%s wins!\n", V_GetColorChar( TEAM_GetTextColor( source->player->ulTeam )), TEAM_GetName( source->player->ulTeam ));
						else
							sprintf( szString, "%s \\cGWINS!", players[source->player - players].userinfo.netname );
						V_ColorizeString( szString );

						if ( NETWORK_GetState( ) != NETSTATE_SERVER )
						{
							// Display "%s WINS!" HUD message.
							pMsg = new DHUDMessageFadeOut( BigFont, szString,
								160.4f,
								75.0f,
								320,
								200,
								CR_WHITE,
								3.0f,
								2.0f );

							StatusBar->AttachMessage( pMsg, MAKE_ID('C','N','T','R') );

							szString[0] = 0;
							pMsg = new DHUDMessageFadeOut( SmallFont, szString,
								0.0f,
								0.0f,
								0,
								0,
								CR_RED,
								3.0f,
								2.0f );

							StatusBar->AttachMessage( pMsg, MAKE_ID('F','R','A','G') );

							pMsg = new DHUDMessageFadeOut( SmallFont, szString,
								0.0f,
								0.0f,
								0,
								0,
								CR_RED,
								3.0f,
								2.0f );

							StatusBar->AttachMessage( pMsg, MAKE_ID('P','L','A','C') );
						}
						else
						{
							SERVERCOMMANDS_PrintHUDMessageFadeOut( szString, 160.4f, 75.0f, 320, 200, CR_WHITE, 3.0f, 2.0f, "BigFont", false, MAKE_ID('C','N','T','R') );
							szString[0] = 0;
							SERVERCOMMANDS_PrintHUDMessageFadeOut( szString, 0.0f, 0.0f, 0, 0, CR_WHITE, 3.0f, 2.0f, "BigFont", false, MAKE_ID('F','R','A','G') );
							SERVERCOMMANDS_PrintHUDMessageFadeOut( szString, 0.0f, 0.0f, 0, 0, CR_WHITE, 3.0f, 2.0f, "BigFont", false, MAKE_ID('P','L','A','C') );
						}

						GAME_SetEndLevelDelay( 5 * TICRATE );
					}
				}
			}
		}

		// If the player got telefragged by a player trying to spawn, allow him to respawn.
		if (( player ) && ( GAMEMODE_AreLivesLimited ( ) ) && ( MeansOfDeath == NAME_SpawnTelefrag ))
			player->bSpawnTelefragged = true;
	}
	else if (( NETWORK_GetState( ) != NETSTATE_CLIENT ) && ( CLIENTDEMO_IsPlaying( ) == false ) && (CountsAsKill()))
	{
		// count all monster deaths,
		// even those caused by other monsters

		// Why give player 0 credit? :P
		// Meh, just do it in single player.
		if ( NETWORK_GetState( ) == NETSTATE_SINGLE )
			players[0].killcount++;

		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		{
//			SERVER_PlayerKilledMonster( MAXPLAYERS );

			// Also, update the scoreboard.
			SERVERCONSOLE_UpdateScoreboard( );
		}
	}
	
	// [BC] Don't do this block in client mode.
	if (player &&
		( NETWORK_GetState( ) != NETSTATE_CLIENT ) &&
		( CLIENTDEMO_IsPlaying( ) == false ))
	{
		// [BC] If this is a bot, tell it it died.
		if ( player->pSkullBot )
		{
			if ( source && source->player )
				player->pSkullBot->m_ulPlayerKilledBy = source->player - players;
			else
				player->pSkullBot->m_ulPlayerKilledBy = MAXPLAYERS;

			if ( source && source->player )
			{
				if ( source->player == player )
					player->pSkullBot->PostEvent( BOTEVENT_KILLED_BYSELF );
				else if (( source->player - players ) == static_cast<int> (player->pSkullBot->m_ulPlayerEnemy) )
					player->pSkullBot->PostEvent( BOTEVENT_KILLED_BYENEMY );
				else
					player->pSkullBot->PostEvent( BOTEVENT_KILLED_BYPLAYER );
			}
			else
				player->pSkullBot->PostEvent( BOTEVENT_KILLED_BYENVIORNMENT );
		}

		// [BC] Keep track of where we died for the "same spot respawn" dmflags.
		player->SpawnX = x;
		player->SpawnY = y;
		player->SpawnAngle = angle;
		player->bSpawnOkay = true;

		// Death script execution, care of Skull Tag
		FBehavior::StaticStartTypedScripts (SCRIPT_Death, this, true);

		// [RH] Force a delay between death and respawn
		if ((( i_compatflags & COMPATF_INSTANTRESPAWN ) == false ) ||
			( player->bSpawnTelefragged ))
		{
			player->respawn_time = level.time + TICRATE;

			// [BC] Don't respawn quite so fast on forced respawn. It sounds weird when your
			// scream isn't completed.
			if ( dmflags & DF_FORCE_RESPAWN )
				player->respawn_time += TICRATE/2;
		}

		// count environment kills against you
		if (!source)
		{
			PLAYER_SetFragcount( player, player->fragcount - (( bPossessedTerminatorArtifact ) ? 10 : 1 ), true, true );	// [RH] Cumulative frag count

			// Spawning in nukage or getting crushed is NOT
			// somewhere where you would want to be at when you respawn again
			player->bSpawnOkay = false;
		}
						
		// [BC] Increment team deathcount.
		if (( GAMEMODE_GetFlags( GAMEMODE_GetCurrentMode( )) & GMF_PLAYERSONTEAMS ) && ( player->bOnTeam ))
			TEAM_SetDeathCount( player->ulTeam, TEAM_GetDeathCount( player->ulTeam ) + 1 );
	}

	// [BC] We can do this stuff in client mode.
	if (player)
	{
		flags &= ~MF_SOLID;
		player->playerstate = PST_DEAD;

		P_DropWeapon (player);

		if (this == players[consoleplayer].camera && automapactive)
		{
			// don't die in auto map, switch view prior to dying
			AM_Stop ();
		}

		// [GRB] Clear extralight. When you killed yourself with weapon that
		// called A_Light1/2 before it called A_Light0, extraligh remained.
		player->extralight = 0;
	}

	// [RH] If this is the unmorphed version of another monster, destroy this
	// actor, because the morphed version is the one that will stick around in
	// the level.
	if (flags & MF_UNMORPHED)
	{
		Destroy ();
		return;
	}

	// [BC] Tell clients that this thing died.
	// [BC] We need to move this block a little higher, otherwise things can be destroyed
	// before we tell the client to kill them.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		// If there isn't a player attached to this object, treat it like a normal thing.
		if ( player == NULL )
			SERVERCOMMANDS_KillThing( this, source, inflictor );
		else
			SERVERCOMMANDS_KillPlayer( ULONG( player - players ), source, inflictor, MeansOfDeath );
	}

	FState *diestate = NULL;

	if (DamageType != NAME_None)
	{
		diestate = FindState (NAME_Death, DamageType, true);
		if (diestate == NULL)
		{
			if (DamageType == NAME_Ice)
			{ // If an actor doesn't have an ice death, we can still give them a generic one.

				if (!deh.NoAutofreeze && !(flags4 & MF4_NOICEDEATH) && (player || (flags3 & MF3_ISMONSTER)))
				{
					diestate = FindState(NAME_GenericFreezeDeath);
				}
			}
		}
	}
	if (diestate == NULL)
	{
		int flags4 = inflictor == NULL ? 0 : inflictor->flags4;

		int gibhealth = GibHealth(this);
		
		// Don't pass on a damage type this actor cannot handle.
		// (most importantly, prevent barrels from passing on ice damage.)
		// Massacre must be preserved though.
		if (DamageType != NAME_Massacre)
		{
			DamageType = NAME_None;	
		}

		if ((health < gibhealth || flags4 & MF4_EXTREMEDEATH) && !(flags4 & MF4_NOEXTREMEDEATH))
		{ // Extreme death
			diestate = FindState (NAME_Death, NAME_Extreme, true);
			// If a non-player, mark as extremely dead for the crash state.
			if (diestate != NULL && player == NULL && health >= gibhealth)
			{
				health = gibhealth - 1;
			}
			// For players, mark the appropriate flag.
			else if (player != NULL)
			{
				player->cheats |= CF_EXTREMELYDEAD;
			}
		}
		if (diestate == NULL)
		{ // Normal death
			diestate = FindState (NAME_Death);
		}
	}

	if (diestate != NULL)
	{
		SetState (diestate);

		tics -= pr_killmobj() & 3;
		if (tics < 1)
			tics = 1;
	}
	else
	{
		Destroy();
	}

	if (( (GAMEMODE_GetFlags( GAMEMODE_GetCurrentMode( )) & GMF_COOPERATIVE) == false ) &&
		( NETWORK_GetState( ) != NETSTATE_SERVER ) &&
		( NETWORK_GetState( ) != NETSTATE_CLIENT ) &&
		( CLIENTDEMO_IsPlaying( ) == false ) &&
		( GAMEMODE_IsGameInProgress() ))
	{
		if (( player ) && ( source ) && ( source->player ) && ( player != source->player ) && ( MeansOfDeath != NAME_SpawnTelefrag ))
		{
			if ((( ( GAMEMODE_GetFlags(GAMEMODE_GetCurrentMode()) & GMF_PLAYERSEARNFRAGS ) == false ) || (( fraglimit == 0 ) || ( source->player->fragcount < fraglimit ))) &&
				(( ( ( GAMEMODE_GetFlags(GAMEMODE_GetCurrentMode()) & GMF_PLAYERSEARNWINS ) && !( GAMEMODE_GetFlags(GAMEMODE_GetCurrentMode()) & GMF_PLAYERSONTEAMS ) ) == false ) || (( winlimit == 0 ) || ( source->player->ulWins < static_cast<ULONG>(winlimit) ))) &&
				(( ( ( GAMEMODE_GetFlags(GAMEMODE_GetCurrentMode()) & GMF_PLAYERSEARNWINS ) && ( GAMEMODE_GetFlags(GAMEMODE_GetCurrentMode()) & GMF_PLAYERSONTEAMS ) ) == false ) || (( winlimit == 0 ) || ( TEAM_GetWinCount( source->player->ulTeam ) < winlimit ))))
			{
				// Display a large "You were fragged by <name>." message in the middle of the screen.
				if (( player - players ) == consoleplayer )
				{
					if ( cl_showlargefragmessages )
						SCOREBOARD_DisplayFraggedMessage( source->player );
					
					if ( G15_IsReady() ) // [RC] Also show the message on the Logitech G15 (if enabled).
						G15_ShowLargeFragMessage( source->player->userinfo.netname, false );
				}

				// Display a large "You fragged <name>!" message in the middle of the screen.
				else if (( source->player - players ) == consoleplayer )
				{
					if ( cl_showlargefragmessages )
						SCOREBOARD_DisplayFragMessage( player );
					
					if ( G15_IsReady() ) // [RC] Also show the message on the Logitech G15 (if enabled).
						G15_ShowLargeFragMessage( player->userinfo.netname, true );
				}
			}
		}
	}

	// [RH] Death messages
	if (( player ) && ( NETWORK_GetState( ) != NETSTATE_CLIENT ) && ( CLIENTDEMO_IsPlaying( ) == false ))
		ClientObituary (this, inflictor, source, MeansOfDeath);

}




//---------------------------------------------------------------------------
//
// PROC P_AutoUseHealth
//
//---------------------------------------------------------------------------
static int CountHealth(TArray<AInventory *> &Items)
{
	int counted = 0;
	for(unsigned i = 0; i < Items.Size(); i++)
	{
		counted += Items[i]->Amount * Items[i]->health;
	}
	return counted;
}

static int UseHealthItems(TArray<AInventory *> &Items, int &saveHealth)
{
	int saved = 0;

	while (Items.Size() > 0 && saveHealth > 0)
	{
		int maxhealth = 0;
		int index = -1;

		// Find the largest item in the list
		for(unsigned i = 0; i < Items.Size(); i++)
		{
			if (Items[i]->health > maxhealth)
			{
				index = i;
				maxhealth = Items[i]->health;
			}
		}

		// [BB] Keep track of some values to inform the client how many items were used.
		int oldAmount = Items[index]->Amount;
		int newAmount = oldAmount;
		const FString itemName = Items[index]->GetClass()->TypeName.GetChars();
		ULONG ulPlayer = MAXPLAYERS;
		if ( Items[index]->Owner && Items[index]->Owner->player )
			ulPlayer = static_cast<ULONG>(Items[index]->Owner->player - players);

		// Now apply the health items, using the same logic as Heretic and Hexen.
		int count = (saveHealth + maxhealth-1) / maxhealth;
		for(int i = 0; i < count; i++)
		{
			saved += maxhealth;
			saveHealth -= maxhealth;

			// [BB] Update newAmount separately from Items[index]->Amount. If the item is depleted
			// we can't access Items[index]->Amount after the i-loop.
			--newAmount;

			if (--Items[index]->Amount == 0)
			{
				if (!(Items[index]->ItemFlags & IF_KEEPDEPLETED))
				{
					Items[index]->Destroy ();
				}
				Items.Delete(index);
				break;
			}
		}

		// [BB] Inform the client about ths used items. 
		// Note: SERVERCOMMANDS_TakeInventory checks the validity of the ulPlayer value.
		if ( ( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( newAmount != oldAmount ) )
			SERVERCOMMANDS_TakeInventory ( ulPlayer, itemName.GetChars(), newAmount );
	}
	return saved;
}

void P_AutoUseHealth(player_t *player, int saveHealth)
{
	TArray<AInventory *> NormalHealthItems;
	TArray<AInventory *> LargeHealthItems;

	for(AInventory *inv = player->mo->Inventory; inv != NULL; inv = inv->Inventory)
	{
		if (inv->Amount > 0 && inv->IsKindOf(RUNTIME_CLASS(AHealthPickup)))
		{
			int mode = static_cast<AHealthPickup*>(inv)->autousemode;

			if (mode == 1) NormalHealthItems.Push(inv);
			else if (mode == 2) LargeHealthItems.Push(inv);
		}
	}

	int normalhealth = CountHealth(NormalHealthItems);
	int largehealth = CountHealth(LargeHealthItems);

	bool skilluse = !!G_SkillProperty(SKILLP_AutoUseHealth);

	if (skilluse && normalhealth >= saveHealth)
	{ // Use quartz flasks
		player->health += UseHealthItems(NormalHealthItems, saveHealth);
	}
	else if (largehealth >= saveHealth)
	{ 
		// Use mystic urns
		player->health += UseHealthItems(LargeHealthItems, saveHealth);
	}
	else if (skilluse && normalhealth + largehealth >= saveHealth)
	{ // Use mystic urns and quartz flasks
		player->health += UseHealthItems(NormalHealthItems, saveHealth);
		if (saveHealth > 0) player->health += UseHealthItems(LargeHealthItems, saveHealth);
	}
	player->mo->health = player->health;
}

//============================================================================
//
// P_AutoUseStrifeHealth
//
//============================================================================
CVAR(Bool, sv_disableautohealth, false, CVAR_ARCHIVE|CVAR_SERVERINFO)

void P_AutoUseStrifeHealth (player_t *player)
{
	TArray<AInventory *> Items;

	for(AInventory *inv = player->mo->Inventory; inv != NULL; inv = inv->Inventory)
	{
		if (inv->Amount > 0 && inv->IsKindOf(RUNTIME_CLASS(AHealthPickup)))
		{
			int mode = static_cast<AHealthPickup*>(inv)->autousemode;

			if (mode == 3) Items.Push(inv);
		}
	}

	if (!sv_disableautohealth)
	{
		while (Items.Size() > 0)
		{
			int maxhealth = 0;
			int index = -1;

			// Find the largest item in the list
			for(unsigned i = 0; i < Items.Size(); i++)
			{
				if (Items[i]->health > maxhealth)
				{
					index = i;
					maxhealth = Items[i]->Amount;
				}
			}

			while (player->health < 50)
			{
				if (!player->mo->UseInventory (Items[index]))
					break;
			}
			if (player->health >= 50) return;
			// Using all of this item was not enough so delete it and restart with the next best one
			Items.Delete(index);
		}
	}
}

/*
=================
=
= P_DamageMobj
=
= Damages both enemies and players
= inflictor is the thing that caused the damage
= 		creature or missile, can be NULL (slime, etc)
= source is the thing to target after taking damage
=		creature or NULL
= Source and inflictor are the same for melee attacks
= source can be null for barrel explosions and other environmental stuff
==================
*/

// [TIHan/Spleen] Factor for damage dealt to players by monsters.
CUSTOM_CVAR (Float, sv_coop_damagefactor, 1.0f, CVAR_SERVERINFO)
{
	if (self <= 0)
		self = 1.0f;

	if (( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( gamestate != GS_STARTUP ))
	{
		SERVER_Printf( PRINT_HIGH, "%s changed to: %.2f\n", self.GetName( ), (float)self );
		SERVERCOMMANDS_SetGameModeLimits( );
		SERVERCONSOLE_UpdateScoreboard();
	}
}

// [TIHan/Spleen] Apply factor for damage dealt to players by monsters.
void ApplyCoopDamagefactor(int &damage, AActor *source)
{
	if ((sv_coop_damagefactor != 1.0f) && (source != NULL) && (source->flags3 & MF3_ISMONSTER))
		damage = int(damage * sv_coop_damagefactor);
}

void P_DamageMobj (AActor *target, AActor *inflictor, AActor *source, int damage, FName mod, int flags)
{
	unsigned ang;
	player_t *player = NULL;
	fixed_t thrust;
	int temp;
	// [BC]
	LONG	lOldTargetHealth;

	// [BC] Game is currently in a suspended state; don't hurt anyone.
	if ( GAME_GetEndLevelDelay( ))
		return;

	if (target == NULL || !(target->flags & MF_SHOOTABLE))
	{ // Shouldn't happen
		return;
	}

	// [BB] For the time being, unassigned voodoo dolls can't be damaged.
	if ( target->player == COOP_GetVoodooDollDummyPlayer() )
		return;

	// Spectral targets only take damage from spectral projectiles.
	if (target->flags4 & MF4_SPECTRAL && damage < 1000000)
	{
		if (inflictor == NULL || !(inflictor->flags4 & MF4_SPECTRAL))
		{
			/*
			if (target->MissileState != NULL)
			{
				target->SetState (target->MissileState);
			}
			*/
			return;
		}
	}
	if (target->health <= 0)
	{
		if (inflictor && mod == NAME_Ice)
		{
			return;
		}
		else if (target->flags & MF_ICECORPSE) // frozen
		{
			target->tics = 1;
			target->momx = target->momy = target->momz = 0;

			// [BC] If we're the server, tell clients to update this thing's tics and
			// momentum.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			{
				SERVERCOMMANDS_SetThingTics( target );
				SERVERCOMMANDS_MoveThing( target, CM_MOMX|CM_MOMY|CM_MOMZ );
			}
		}
		return;
	}
	if ((target->flags2 & MF2_INVULNERABLE) && damage < 1000000 && !(flags & DMG_FORCED))
	{ // actor is invulnerable
		if (!target->player)
		{
			if (!inflictor || !(inflictor->flags3 & MF3_FOILINVUL))
			{
				return;
			}
		}
		else
		{
			// Only in Hexen invulnerable players are excluded from getting
			// thrust by damage.
			if (gameinfo.gametype == GAME_Hexen) return;
		}
		
	}
	if (inflictor != NULL)
	{
		if (inflictor->flags5 & MF5_PIERCEARMOR) flags |= DMG_NO_ARMOR;
	}
	
	MeansOfDeath = mod;
	FriendlyFire = false;
	// [RH] Andy Baker's Stealth monsters
	if (target->flags & MF_STEALTH)
	{
		target->alpha = OPAQUE;
		target->visdir = -1;
	}
	// [BB] The clients may not do this.
	if ( (target->flags & MF_SKULLFLY)
	     && ( NETWORK_GetState( ) != NETSTATE_CLIENT ) && ( CLIENTDEMO_IsPlaying( ) == false ) )
	{
		target->momx = target->momy = target->momz = 0;

		// [BC] If we're the server, tell clients to update this thing's momentum
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_MoveThing( target, CM_MOMX|CM_MOMY|CM_MOMZ );
	}
	if (!(flags & DMG_FORCED))	// DMG_FORCED skips all special damage checks
	{
		if (target->flags2 & MF2_DORMANT)
		{
			// Invulnerable, and won't wake up
			return;
		}
		player = target->player;
		if (player && damage > 1)
		{
			// Take half damage in trainer mode
			damage = FixedMul(damage, G_SkillProperty(SKILLP_DamageFactor));
		}
		// Special damage types
		if (inflictor)
		{
			if (inflictor->flags4 & MF4_SPECTRAL)
			{
				if (player != NULL)
				{
					if (!deathmatch && inflictor->health == -1)
						return;
				}
				else if (target->flags4 & MF4_SPECTRAL)
				{
					if (inflictor->health == -2 && !target->IsHostile(inflictor))
						return;
				}
			}

			damage = inflictor->DoSpecialDamage (target, damage);
			if (damage == -1)
			{
				return;
			}

		}
		// Handle active damage modifiers (e.g. PowerDamage)
		if (source != NULL && source->Inventory != NULL)
		{
			int olddam = damage;
			source->Inventory->ModifyDamage(olddam, mod, damage, false);
			if (olddam != damage && damage <= 0) return;
		}
		// Handle passive damage modifiers (e.g. PowerProtection)
		if (target->Inventory != NULL)
		{
			int olddam = damage;
			target->Inventory->ModifyDamage(olddam, mod, damage, true);
			if (olddam != damage && damage <= 0) return;
		}

		// [Dusk] Unblocked players don't telefrag each other, they
		// just pass through each other.
		// [BB] Voodoo dolls still telefrag.
		if (( dmflags3 & DF3_UNBLOCK_PLAYERS ) &&
			( source != NULL ) && ( source->player ) && ( source->player->mo == source ) && ( target->player ) && ( target->player->mo == target ) &&
			( mod == NAME_Telefrag || mod == NAME_SpawnTelefrag ))
			return;

		DmgFactors * df = target->GetClass()->ActorInfo->DamageFactors;
		if (df != NULL)
		{
			fixed_t * pdf = df->CheckKey(mod);
			if (pdf== NULL && mod != NAME_None) pdf = df->CheckKey(NAME_None);
			if (pdf != NULL)
			{
				damage = FixedMul(damage, *pdf);
				if (damage <= 0) return;
			}
		}

		damage = target->TakeSpecialDamage (inflictor, source, damage, mod);
	}

	if (damage == -1)
	{
		return;
	}

	// [BC] If the target player has the reflection rune, damage the source with 50% of the
	// this player is being damaged with.
	if (( target->player ) &&
		( target->player->cheats & CF_REFLECTION ) &&
		( source ) &&
		( mod != NAME_Reflection ) &&
		( NETWORK_GetState( ) != NETSTATE_CLIENT ) &&
		( CLIENTDEMO_IsPlaying( ) == false ))
	{
		if ( target != source )
		{
			P_DamageMobj( source, NULL, target, (( damage * 3 ) / 4 ), NAME_Reflection );

			// Reset means of death flag.
			MeansOfDeath = mod;
		}
	}

	// Push the target unless the source's weapon's kickback is 0.
	// (i.e. Guantlets/Chainsaw)
	// [BB] The server handles this.
	if (inflictor && inflictor != target	// [RH] Not if hurting own self
		&& !(target->flags & MF_NOCLIP)
		&& !(inflictor->flags2 & MF2_NODMGTHRUST)
		&& !(flags & DMG_THRUSTLESS)
		&& ( NETWORK_GetState( ) != NETSTATE_CLIENT )
		&& ( CLIENTDEMO_IsPlaying( ) == false ) )
	{
		int kickback;

		if (!source || !source->player || !source->player->ReadyWeapon)
			kickback = gameinfo.defKickback;
		else
			kickback = source->player->ReadyWeapon->Kickback;

		if (kickback)
		{
			// [BB] Safe the original z-momentum of the target. This way we can check if we need to update it.
			const fixed_t oldTargetMomz = target->momz;

			AActor *origin = (source && (flags & DMG_INFLICTOR_IS_PUFF))? source : inflictor;
			
			ang = R_PointToAngle2 (origin->x, origin->y,
				target->x, target->y);
			thrust = damage*(FRACUNIT>>3)*kickback / target->Mass;
			// [RH] If thrust overflows, use a more reasonable amount
			if (thrust < 0 || thrust > 10*FRACUNIT)
			{
				thrust = 10*FRACUNIT;
			}
			// make fall forwards sometimes
			if ((damage < 40) && (damage > target->health)
				 && (target->z - origin->z > 64*FRACUNIT)
				 && (pr_damagemobj()&1)
				 // [RH] But only if not too fast and not flying
				 && thrust < 10*FRACUNIT
				 && !(target->flags & MF_NOGRAVITY))
			{
				ang += ANG180;
				thrust *= 4;
			}
			ang >>= ANGLETOFINESHIFT;
			if (source && source->player && (source == inflictor)
				&& source->player->ReadyWeapon != NULL &&
				(source->player->ReadyWeapon->WeaponFlags & WIF_STAFF2_KICKBACK))
			{
				// Staff power level 2
				target->momx += FixedMul (10*FRACUNIT, finecosine[ang]);
				target->momy += FixedMul (10*FRACUNIT, finesine[ang]);
				if (!(target->flags & MF_NOGRAVITY))
				{
					target->momz += 5*FRACUNIT;
				}
			}
			else
			{
				target->momx += FixedMul (thrust, finecosine[ang]);
				target->momy += FixedMul (thrust, finesine[ang]);
			}

			// [BC] Set the thing's momentum.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			{
				// [BB] Only update z-momentum if it has changed.
				SERVER_UpdateThingMomentum ( target, oldTargetMomz != target->momz );
			}
		}
	}

	// [RH] Avoid friendly fire if enabled
	if (source != NULL &&
		((player && player != source->player) || (!player && target != source)) &&
		target->IsTeammate (source))
	{
		// [BL] Some adjustments for Skulltag
		if (player && (( teamlms || survival ) && ( MeansOfDeath == NAME_SpawnTelefrag )) == false )
			FriendlyFire = true;
		if (damage < 1000000)
		{ // Still allow telefragging :-(
			damage = (int)((float)damage * level.teamdamage);
			if (damage <= 0)
				return;
		}
	}

	//
	// player specific
	//
	// [BC]
	lOldTargetHealth = target->health;
	if (player)
	{
		// [TIHan/Spleen] Apply factor for damage dealt to players by monsters.
		ApplyCoopDamagefactor(damage, source);

		// end of game hell hack
		if ((target->Sector->special & 255) == dDamage_End
			&& damage >= target->health
			// [BB] A player who tries to exit a map in a competitive game mode when DF_NO_EXIT is set,
			// should not be saved by the hack, but killed.
			&& MeansOfDeath != NAME_Exit)
		{
			damage = target->health - 1;
		}

		if (!(flags & DMG_FORCED))
		{
			if ((target->flags2 & MF2_INVULNERABLE) && damage < 1000000)
			{ // player is invulnerable, so don't hurt him
				return;
			}

			if (damage < 1000 && ((target->player->cheats & CF_GODMODE)
				|| (target->player->mo->flags2 & MF2_INVULNERABLE)))
			{
				return;
			}

			if (!(flags & DMG_NO_ARMOR) && player->mo->Inventory != NULL)
			{
				int newdam = damage;
				player->mo->Inventory->AbsorbDamage (damage, mod, newdam);
				damage = newdam;
				if (damage <= 0)
				{
				// [BB] The player didn't lose health but armor. The server needs
				// to tell the client about this.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_SetPlayerArmor( player - players );
					return;
				}
			}
			
			if (damage >= player->health
				&& (G_SkillProperty(SKILLP_AutoUseHealth) || deathmatch)
				&& !player->morphTics)
			{ // Try to use some inventory health
				P_AutoUseHealth (player, damage - player->health + 1);
			}
		}

		player->health -= damage;		// mirror mobj health here for Dave
		// [RH] Make voodoo dolls and real players record the same health
		target->health = player->mo->health -= damage;
		if (player->health < 50 && !deathmatch && !(flags & DMG_FORCED))
		{
			P_AutoUseStrifeHealth (player);
			player->mo->health = player->health;
		}
		if (player->health < 0)
		{
			player->health = 0;
		}
		player->LastDamageType = mod;

		if ( player->pSkullBot )
		{
			// Tell the bot he's been damaged by a player.
			if ( source && source->player && ( source->player != player ))
			{
				player->pSkullBot->m_ulLastPlayerDamagedBy = source->player - players;
				player->pSkullBot->PostEvent( BOTEVENT_DAMAGEDBY_PLAYER );
			}
		}

		player->attacker = source;
		player->damagecount += damage;	// add damage after armor / invuln
		if (player->damagecount > 100)
		{
			player->damagecount = 100;	// teleport stomp does 10k points...
		}
		if ( player->damagecount < 0 )
			player->damagecount = 0;
		temp = damage < 100 ? damage : 100;
		if (player == &players[consoleplayer])
		{
			I_Tactile (40,10,40+temp*2);
		}
	}
	else
	{
		// Armor for monsters.
		if (!(flags & (DMG_NO_ARMOR|DMG_FORCED)) && target->Inventory != NULL && damage > 0)
		{
			int newdam = damage;
			target->Inventory->AbsorbDamage (damage, mod, newdam);
			damage = newdam;
			if (damage <= 0)
			{
				return;
			}
		}
	
		target->health -= damage;	
	}

	//
	// the damage has been dealt; now deal with the consequences
	//

	// If the damaging player has the power of drain, give the player 50% of the damage
	// done in health.
	if ( source && source->player && source->player->cheats & CF_DRAIN &&
		( NETWORK_GetState( ) != NETSTATE_CLIENT ) &&
		( CLIENTDEMO_IsPlaying( ) == false ))
	{
		if (( target->player == false ) || ( target->player != source->player ))
		{
			if ( P_GiveBody( source, MIN( (int)lOldTargetHealth, damage ) / 2 ))
			{
				// [BC] If we're the server, send out the health change, and play the
				// health sound.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				{
					SERVERCOMMANDS_SetPlayerHealth( source->player - players );
					SERVERCOMMANDS_SoundActor( source, CHAN_ITEM, "misc/i_pkup", 1, ATTN_NORM );
				}

				S_Sound( source, CHAN_ITEM, "misc/i_pkup", 1, ATTN_NORM );
			}
		}
	}

	// [BB] Save the damage the player has dealt to monsters here, it's only converted to points
	// though if DF2_AWARD_DAMAGE_INSTEAD_KILLS is set.
	if ( source && source->player
	     && ( target->player == false )
	     && ( NETWORK_GetState( ) != NETSTATE_CLIENT )
	     && ( CLIENTDEMO_IsPlaying( ) == false ))
	{
		source->player->ulUnrewardedDamageDealt += MIN( (int)lOldTargetHealth, damage );
	}

	// [BC] Tell clients that this thing was damaged.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		if ( player )
			SERVERCOMMANDS_DamagePlayer( ULONG( player - players ));
		else
			// Is this even necessary?
			SERVERCOMMANDS_DamageThing( target );
	}

	if (target->health <= 0)
	{ // Death
		target->special1 = damage;
		// check for special fire damage or ice damage deaths
		if (mod == NAME_Fire)
		{
			if (player && !player->morphTics)
			{ // Check for flame death
				if (!inflictor ||
					((target->health > -50) && (damage > 25)) ||
					!(inflictor->flags5 & MF5_SPECIALFIREDAMAGE))
				{
					target->DamageType = NAME_Fire;
				}
			}
			else
			{
				target->DamageType = NAME_Fire;
			}
		}
		else
		{
			target->DamageType = mod;
		}
		if (source && source->tracer && (source->flags5 & MF5_SUMMONEDMONSTER))
		{ // Minotaur's kills go to his master
			// Make sure still alive and not a pointer to fighter head
			if (source->tracer->player && (source->tracer->player->mo == source->tracer))
			{
				source = source->tracer;
			}
		}
		// kgKILL start
		else if ( source && source->FriendPlayer ) {
			const player_t *pl = &players[source->FriendPlayer - 1];
			if (pl) source = pl->mo;
		}
		// kgKILL end

		// Deaths are server side.
		if (( NETWORK_GetState( ) != NETSTATE_CLIENT ) &&
			( CLIENTDEMO_IsPlaying( ) == false ))
		{
			target->Die (source, inflictor);
		}
		return;
	}

	FState * woundstate = target->FindState(NAME_Wound, mod);
	// [BB] The server takes care of this.
	if ( (woundstate != NULL) && ( NETWORK_InClientMode( ) == false ) )
	{
		int woundhealth = RUNTIME_TYPE(target)->Meta.GetMetaInt (AMETA_WoundHealth, 6);

		if (target->health <= woundhealth)
		{
			// [Dusk] As the server, update the clients on the state
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetThingFrame( target, woundstate );

			target->SetState (woundstate);
			return;
		}
	}
	
	PainChanceList * pc = target->GetClass()->ActorInfo->PainChances;
	int painchance = target->PainChance;
	if (pc != NULL)
	{
		BYTE * ppc = pc->CheckKey(mod);
		if (ppc != NULL)
		{
			painchance = *ppc;
		}
	}
	
	if (!(target->flags5 & MF5_NOPAIN) && (inflictor == NULL || !(inflictor->flags5 & MF5_PAINLESS)) && 
		(pr_damagemobj() < painchance) && !(target->flags & MF_SKULLFLY) &&
		( NETWORK_GetState( ) != NETSTATE_CLIENT ) && ( CLIENTDEMO_IsPlaying( ) == false ))
	{
		if (mod == NAME_Electric)
		{
			if (pr_lightning() < 96)
			{
				target->flags |= MF_JUSTHIT; // fight back!
				FState * painstate = target->FindState(NAME_Pain, mod);
				if (painstate != NULL)
				{
					// If we are the server, tell clients about the state change.
					if ( NETWORK_GetState( ) == NETSTATE_SERVER )
						SERVERCOMMANDS_SetThingFrame( target, painstate );

					target->SetState (painstate);
				}
			}
			else
			{ // "electrocute" the target
				target->renderflags |= RF_FULLBRIGHT;
				if ((target->flags3 & MF3_ISMONSTER) && pr_lightning() < 128)
				{
					target->Howl ();
				}
			}
		}
		else
		{
			target->flags |= MF_JUSTHIT; // fight back!
			FState * painstate = target->FindState(NAME_Pain, mod);
			if (painstate != NULL)
			{
				// If we are the server, tell clients about the state change.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_SetThingFrame( target, painstate );

				target->SetState (painstate);
			}
			if (mod == NAME_PoisonCloud)
			{
				if ((target->flags3 & MF3_ISMONSTER) && pr_poison() < 128)
				{
					target->Howl ();
				}
			}
		}
	}

	// Nothing more to do!
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) || ( CLIENTDEMO_IsPlaying( )))
		return;

	target->reactiontime = 0;			// we're awake now...	
	if (source)
	{
		if (source == target->target)
		{
			target->threshold = BASETHRESHOLD;
			if (target->state == target->SpawnState && target->SeeState != NULL)
			{
				// [BB] If we are the server, tell clients about the state change.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_SetThingState( target, STATE_SEE );

				target->SetState (target->SeeState);
			}
		}
		else if (source != target->target && target->OkayToSwitchTarget (source))
		{
			// Target actor is not intent on another actor,
			// so make him chase after source

			// killough 2/15/98: remember last enemy, to prevent
			// sleeping early; 2/21/98: Place priority on players

			if (target->lastenemy == NULL ||
				(target->lastenemy->player == NULL && target->TIDtoHate == 0) ||
				target->lastenemy->health <= 0)
			{
				target->lastenemy = target->target; // remember last enemy - killough
			}
			target->target = source;
			target->threshold = BASETHRESHOLD;
			if (target->state == target->SpawnState && target->SeeState != NULL)
			{
				// If we are the server, tell clients about the state change.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_SetThingState( target, STATE_SEE );

				target->SetState (target->SeeState);
			}
		}
	}
}

bool AActor::OkayToSwitchTarget (AActor *other)
{
	if (other == this)
		return false;		// [RH] Don't hate self (can happen when shooting barrels)

	if (!(other->flags & MF_SHOOTABLE))
		return false;		// Don't attack things that can't be hurt

	if ((flags4 & MF4_NOTARGETSWITCH) && target != NULL)
		return false;		// Don't switch target if not allowed

	if ((master != NULL && other->IsA(master->GetClass())) ||		// don't attack your master (or others of its type)
		(other->master != NULL && IsA(other->master->GetClass())))	// don't attack your minion (or those of others of your type)
	{
		if (!IsHostile (other) &&								// allow target switch if other is considered hostile
			(other->tid != TIDtoHate || TIDtoHate == 0) &&		// or has the tid we hate
			other->TIDtoHate == TIDtoHate)						// or has different hate information
		{
			return false;
		}
	}

	if ((other->flags3 & MF3_NOTARGET) &&
		(other->tid != TIDtoHate || TIDtoHate == 0) &&
		!IsHostile (other))
		return false;
	if (threshold != 0 && !(flags4 & MF4_QUICKTORETALIATE))
		return false;
	if (IsFriend (other))
	{ // [RH] Friendlies don't target other friendlies
		return false;
	}
	
	int infight;
	if (flags5 & MF5_NOINFIGHTING) infight=-1;	
	else if (level.flags2 & LEVEL2_TOTALINFIGHTING) infight=1;
	else if (level.flags2 & LEVEL2_NOINFIGHTING) infight=-1;	
	else infight = infighting;
	
	// [BC] No infighting during invasion mode.
	if ((infight < 0 || invasion )&&	other->player == NULL && !IsHostile (other))
	{
		return false;	// infighting off: Non-friendlies don't target other non-friendlies
	}
	if (TIDtoHate != 0 && TIDtoHate == other->TIDtoHate)
		return false;		// [RH] Don't target "teammates"
	if (other->player != NULL && (flags4 & MF4_NOHATEPLAYERS))
		return false;		// [RH] Don't target players
	if (target != NULL && target->health > 0 &&
		TIDtoHate != 0 && target->tid == TIDtoHate && pr_switcher() < 128 &&
		P_CheckSight (this, target))
		return false;		// [RH] Don't be too quick to give up things we hate

	return true;
}

//==========================================================================
//
// P_PoisonPlayer - Sets up all data concerning poisoning
//
// poisoner is the object directly responsible for poisoning the player,
// such as a missile. source is the actor responsible for creating the
// poisoner.
//
//==========================================================================

void P_PoisonPlayer (player_t *player, AActor *poisoner, AActor *source, int poison)
{
	// [BC] This is handled server side.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) || ( CLIENTDEMO_IsPlaying( )))
		return;

	if((player->cheats&CF_GODMODE) || (player->mo->flags2 & MF2_INVULNERABLE))
	{
		return;
	}
	if (source != NULL && source->player != player && player->mo->IsTeammate (source))
	{
		poison = (int)((float)poison * level.teamdamage);
	}
	if (poison > 0)
	{
		player->poisoncount += poison;
		player->poisoner = source;
		if(player->poisoncount > 100)
		{
			player->poisoncount = 100;
		}

		// [BC] Update the player's poisoncount.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetPlayerPoisonCount( ULONG( player - players ));
	}
}

//==========================================================================
//
// P_PoisonDamage - Similar to P_DamageMobj
//
//==========================================================================

void P_PoisonDamage (player_t *player, AActor *source, int damage,
	bool playPainSound)
{
	// [Dusk] clients shouldn't execute any of this
	if (NETWORK_InClientMode())
		return;

	AActor *target;
	AActor *inflictor;

	target = player->mo;
	inflictor = source;
	if (target->health <= 0)
	{
		return;
	}
	if (target->flags2&MF2_INVULNERABLE && damage < 1000000)
	{ // target is invulnerable
		return;
	}
	if (player)
	{
		// Take half damage in trainer mode
		damage = FixedMul(damage, G_SkillProperty(SKILLP_DamageFactor));

		// [TIHan/Spleen] Apply factor for damage dealt to players by monsters.
		ApplyCoopDamagefactor(damage, source);
	}
	if(damage < 1000 && ((player->cheats&CF_GODMODE)
		|| (player->mo->flags2 & MF2_INVULNERABLE)))
	{
		return;
	}
	if (damage >= player->health
		&& (G_SkillProperty(SKILLP_AutoUseHealth) || deathmatch)
		&& !player->morphTics)
	{ // Try to use some inventory health
		P_AutoUseHealth (player, damage - player->health+1);
	}
	player->health -= damage; // mirror mobj health here for Dave
	if (player->health < 50 && !deathmatch)
	{
		P_AutoUseStrifeHealth (player);
	}
	if (player->health < 0)
	{
		player->health = 0;
	}
	player->attacker = source;

	// [Dusk] Update the player health
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SetPlayerHealth( player - players );

	//
	// do the damage
	//
	target->health -= damage;
	if (target->health <= 0)
	{ // Death
		target->special1 = damage;
		if (player && inflictor && !player->morphTics)
		{ // Check for flame death
			if ((inflictor->DamageType == NAME_Fire)
				&& (target->health > -50) && (damage > 25))
			{
				target->DamageType = NAME_Fire;
			}
			else target->DamageType = inflictor->DamageType;
		}
		target->Die (source, source);
		return;
	}
	if (!(level.time&63) && playPainSound)
	{
		// If we are the server, tell clients about the state change.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetThingState( target, STATE_PAIN );

		FState * painstate = target->FindState(NAME_Pain, target->DamageType);
		if (painstate != NULL) target->SetState (painstate);
	}
/*
	if((P_Random() < target->info->painchance)
		&& !(target->flags&MF_SKULLFLY))
	{
		target->flags |= MF_JUSTHIT; // fight back!
		P_SetMobjState(target, target->info->painstate);
	}
*/
}

bool CheckCheatmode ();

//*****************************************************************************
//
void PLAYER_SetFragcount( player_t *pPlayer, LONG lFragCount, bool bAnnounce, bool bUpdateTeamFrags )
{
	// Don't bother with fragcount during warm-ups.
	if ((( duel ) && ( DUEL_GetState( ) == DS_COUNTDOWN )) ||
		(( lastmanstanding || teamlms ) && ( LASTMANSTANDING_GetState( ) == LMSS_COUNTDOWN )))
	{
		return;
	}

	// Don't announce events related to frag changes during teamplay, LMS,
	// or possession games.
	if (( bAnnounce ) &&
		( GAMEMODE_GetFlags(GAMEMODE_GetCurrentMode()) & GMF_PLAYERSEARNFRAGS ) &&
		!( GAMEMODE_GetFlags( GAMEMODE_GetCurrentMode( )) & GMF_PLAYERSONTEAMS ))
	{
		ANNOUNCER_PlayFragSounds( pPlayer - players, pPlayer->fragcount, lFragCount );
	}

	// If this is a teamplay deathmatch, update the team frags.
	if ( bUpdateTeamFrags )
	{
		if (( GAMEMODE_GetFlags( GAMEMODE_GetCurrentMode( )) & GMF_PLAYERSONTEAMS ) && ( pPlayer->bOnTeam ))
			TEAM_SetFragCount( pPlayer->ulTeam, TEAM_GetFragCount( pPlayer->ulTeam ) + ( lFragCount - pPlayer->fragcount ), bAnnounce );
	}

	// Set the player's fragcount.
	pPlayer->fragcount = lFragCount;

	// If we're the server, notify the clients of the fragcount change.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		SERVERCOMMANDS_SetPlayerFrags( pPlayer - players );

		// Also, update the scoreboard.
		SERVERCONSOLE_UpdatePlayerInfo( pPlayer - players, UDF_FRAGS );
		SERVERCONSOLE_UpdateScoreboard( );
	}

	// Refresh the HUD since a score has changed.
	SCOREBOARD_RefreshHUD( );
}

//*****************************************************************************
//
void PLAYER_ResetAllScoreCounters( player_t *pPlayer )
{
	// [BB] Sanity check.
	if ( pPlayer == NULL )
		return;

	// [BB] Do this first, PLAYER_SetFragcount / PLAYER_SetWins will update the scoreboard and the serverconsole.
	if ( pPlayer->lPointCount > 0 )
	{
		pPlayer->lPointCount = 0;

		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetPlayerPoints( pPlayer - players );
	}

	if ( pPlayer->fragcount > 0 )
		PLAYER_SetFragcount( pPlayer, 0, false, false );

	if ( pPlayer->ulWins > 0 )
		PLAYER_SetWins( pPlayer, 0 );
}

//*****************************************************************************
//
void PLAYER_ResetAllPlayersFragcount( void )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( playeringame[ulIdx] == false )
			continue;

		players[ulIdx].fragcount = 0;

		// If we're the server, 
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		{
			SERVERCONSOLE_UpdatePlayerInfo( ulIdx, UDF_FRAGS );
			SERVERCONSOLE_UpdateScoreboard( );
		}
	}

	// Refresh the HUD since a score has changed.
	SCOREBOARD_RefreshHUD( );
}

//*****************************************************************************
//
void PLAYER_ResetAllPlayersSpecialCounters ( )
{
	for ( ULONG ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
		PLAYER_ResetSpecialCounters ( &players[ulIdx] );
}

//*****************************************************************************
//
void PLAYER_ResetSpecialCounters ( player_t *pPlayer )
{
	if ( pPlayer == NULL )
		return;

	pPlayer->ulLastExcellentTick = 0;
	pPlayer->ulLastFragTick = 0;
	pPlayer->ulLastBFGFragTick = 0;
	pPlayer->ulConsecutiveHits = 0;
	pPlayer->ulConsecutiveRailgunHits = 0;
	pPlayer->ulDeathsWithoutFrag = 0;
	pPlayer->ulFragsWithoutDeath = 0;
	pPlayer->ulRailgunShots = 0;
	pPlayer->ulUnrewardedDamageDealt = 0;
}

//*****************************************************************************
//
void PLAYER_GivePossessionPoint( player_t *pPlayer )
{
	char				szString[64];
	DHUDMessageFadeOut	*pMsg;

	// If we're in possession mode, give the player a point. Also, determine if the player's lead
	// state changed. If it did, announce it.
	if ( possession )
	{
		pPlayer->lPointCount++;

		// If we're the server, notify the clients of the pointcount change.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetPlayerPoints( pPlayer - players );
	}
	// If this is team possession, also give the player's team a point.
	else if ( teampossession && pPlayer->bOnTeam )
	{
		TEAM_SetScore( pPlayer->ulTeam, TEAM_GetScore( pPlayer->ulTeam ) + 1, true );

		pPlayer->lPointCount++;

		// If we're the server, notify the clients of the pointcount change.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetPlayerPoints( pPlayer - players );
	}
	else
		return;

	SCOREBOARD_RefreshHUD( );

	// If a pointlimit has been set, determine if the game has been won.
	if ( pointlimit )
	{
		if ( possession && ( pPlayer->lPointCount >= pointlimit ))
		{
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			{
				SERVER_Printf( PRINT_HIGH, "Pointlimit hit.\n" );
				SERVER_Printf( PRINT_HIGH, "%s \\c-wins!\n", pPlayer->userinfo.netname );
			}
			else
			{
				Printf( "Pointlimit hit.\n" );
				Printf( "%s \\c-wins!\n", pPlayer->userinfo.netname );

				if ( pPlayer->mo->CheckLocalView( consoleplayer ))
					ANNOUNCER_PlayEntry( cl_announcer, "YouWin" );
			}

			// Just print "YOU WIN!" in single player.
			if (( NETWORK_GetState( ) == NETSTATE_SINGLE_MULTIPLAYER ) && ( pPlayer->mo->CheckLocalView( consoleplayer )))
				sprintf( szString, "YOU WIN!" );
			else
				sprintf( szString, "%s \\c-WINS!", pPlayer->userinfo.netname );
			V_ColorizeString( szString );

			if ( NETWORK_GetState( ) != NETSTATE_SERVER )
			{
				// Display "%s WINS!" HUD message.
				pMsg = new DHUDMessageFadeOut( BigFont, szString,
					160.4f,
					75.0f,
					320,
					200,
					CR_RED,
					3.0f,
					2.0f );

				StatusBar->AttachMessage( pMsg, MAKE_ID('C','N','T','R') );
			}
			else
			{
				SERVERCOMMANDS_PrintHUDMessageFadeOut( szString, 160.4f, 75.0f, 320, 200, CR_RED, 3.0f, 2.0f, "BigFont", false, MAKE_ID('C','N','T','R') );
			}

			// End the level after five seconds.
			GAME_SetEndLevelDelay( 5 * TICRATE );
		}
		else if ( teampossession && ( TEAM_GetScore( pPlayer->ulTeam ) >= pointlimit ))
		{
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			{
				SERVER_Printf( PRINT_HIGH, "Pointlimit hit.\n" );
				SERVER_Printf( PRINT_HIGH, "%s \\c-wins!\n", TEAM_GetName( pPlayer->ulTeam ));
			}
			else
			{
				Printf( "Pointlimit hit.\n" );
				Printf( "%s \\c-wins!\n", TEAM_GetName( pPlayer->ulTeam ));

				if ( pPlayer->mo->IsTeammate( players[consoleplayer].camera ))
					ANNOUNCER_PlayEntry( cl_announcer, "YouWin" );
			}

			sprintf( szString, "\\c%c%s WINS!", V_GetColorChar( TEAM_GetTextColor( pPlayer->ulTeam )), TEAM_GetName( pPlayer->ulTeam ));
			V_ColorizeString( szString );

			if ( NETWORK_GetState( ) != NETSTATE_SERVER )
			{
				// Display "%s WINS!" HUD message.
				pMsg = new DHUDMessageFadeOut( BigFont, szString,
					1.5f,
					0.375f,
					320,
					200,
					CR_RED,
					3.0f,
					2.0f );

				StatusBar->AttachMessage( pMsg, MAKE_ID('C','N','T','R') );
			}
			else
			{
				SERVERCOMMANDS_PrintHUDMessageFadeOut( szString, 1.5f, 0.375f, 320, 200, CR_RED, 3.0f, 2.0f, "BigFont", false, MAKE_ID('C','N','T','R') );
			}

			// End the level after five seconds.
			GAME_SetEndLevelDelay( 5 * TICRATE );
		}
	}
}

//*****************************************************************************
//
void PLAYER_SetTeam( player_t *pPlayer, ULONG ulTeam, bool bNoBroadcast )
{
	bool	bBroadcastChange = false;

	if (
		// Player was removed from a team.
		( pPlayer->bOnTeam && ulTeam == teams.Size( ) ) || 

		// Player was put on a team after not being on one.
		( pPlayer->bOnTeam == false && ulTeam < teams.Size( ) ) ||
		
		// Player is on a team, but is being put on a different team.
		( pPlayer->bOnTeam && ( ulTeam < teams.Size( ) ) && ( ulTeam != pPlayer->ulTeam ))
		)
	{
		bBroadcastChange = true;
	}

	// We don't want to broadcast a print message.
	if ( bNoBroadcast )
		bBroadcastChange = false;

	// Set whether or not this player is on a team.
	if ( ulTeam == teams.Size( ) )
		pPlayer->bOnTeam = false;
	else
		pPlayer->bOnTeam = true;

	// Set the team.
	pPlayer->ulTeam = ulTeam;

	// [BB] Keep track of the original playerstate. In case it's altered by TEAM_EnsurePlayerHasValidClass,
	// the player had a class forbidden to the new team and needs to be respawned.
	const int origPlayerstate = pPlayer->playerstate;

	// [BB] Make sure that the player only uses a class available to his team.
	TEAM_EnsurePlayerHasValidClass ( pPlayer );

	// [BB] The class was changed, so we remove the old player body and respawn the player immediately.
	if ( ( pPlayer->playerstate != origPlayerstate ) && ( pPlayer->playerstate == PST_REBORNNOINVENTORY ) )
	{
		// [BB] Morphed players need to be unmorphed before changing teams.
		if ( pPlayer->morphTics )
			P_UndoPlayerMorph ( pPlayer, pPlayer );

		if ( pPlayer->mo )
		{
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_DestroyThing( pPlayer->mo );

			pPlayer->mo->Destroy( );
			pPlayer->mo = NULL;
		}

		GAMEMODE_SpawnPlayer ( pPlayer - players );
	}

	// If we're the server, tell clients about this team change.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		SERVERCOMMANDS_SetPlayerTeam( pPlayer - players );

		// Player has changed his team! Tell clients.
		if ( bBroadcastChange )
		{
			SERVER_Printf( PRINT_HIGH, "%s \\c-joined the \\c%c%s \\c-team.\n", pPlayer->userinfo.netname, V_GetColorChar( TEAM_GetTextColor( ulTeam ) ), TEAM_GetName( ulTeam )); 
		}		
	}

	// Finally, update the player's color.
	R_BuildPlayerTranslation( pPlayer - players );
	if ( StatusBar && pPlayer->mo && ( pPlayer->mo->CheckLocalView( consoleplayer )))
		StatusBar->AttachToPlayer( pPlayer );

	// Update this player's info on the scoreboard.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCONSOLE_UpdatePlayerInfo( pPlayer - players, UDF_FRAGS );

	// [BL] If the player was "unarmed" give back his inventory now.
	// [BB] Note: On the clients bUnarmed is never true!
	// [BB] This may be called to set team of a player who wasn't spawned yet (for instance in the CSkullBot constructor).
	if ( pPlayer->bUnarmed && pPlayer->mo )
	{
		pPlayer->mo->GiveDefaultInventory();
		if ( deathmatch )
			pPlayer->mo->GiveDeathmatchInventory();

		pPlayer->bUnarmed = false;

		// [BB] Since the clients never come here, tell them about their new inventory (includes bringing up the weapon on the client).
		if ( NETWORK_GetState() == NETSTATE_SERVER )
		{
			const ULONG ulPlayer = static_cast<ULONG>( pPlayer-players );
			SERVER_ResetInventory( ulPlayer );
			// [BB] SERVER_ResetInventory only informs the player ulPlayer. Let the others know of at least the ammo of the player.
			SERVERCOMMANDS_SyncPlayerAmmoAmount ( ulPlayer, ulPlayer, SVCF_SKIPTHISCLIENT );
		}

		P_BringUpWeapon(pPlayer);
	}

	// [Dusk] Update player translations if we override the colors, odds are they're very different now.
	if (( NETWORK_GetState() != NETSTATE_SERVER ) && ( cl_overrideplayercolors ))
		R_BuildAllPlayerTranslations();
}

//*****************************************************************************
//
// [BC] *grumble*
void	G_DoReborn (int playernum, bool freshbot);
void PLAYER_SetSpectator( player_t *pPlayer, bool bBroadcast, bool bDeadSpectator )
{
	AActor	*pOldBody;

	// Already a spectator. Check if their spectating state is changing.
	if ( pPlayer->bSpectating == true )
	{
		// Player is trying to become a true spectator (if not one already).
		if ( bDeadSpectator == false )
		{
			// If they're becoming a true spectator after being a dead spectator, do all the
			// special spectator stuff we didn't do before.
			if ( pPlayer->bDeadSpectator )
			{
				pPlayer->bDeadSpectator = false;

				// Run the disconnect scripts now that the player is leaving.
				PLAYER_LeavesGame ( pPlayer - players );

				if (( NETWORK_GetState( ) != NETSTATE_CLIENT ) &&
					( CLIENTDEMO_IsPlaying( ) == false ))
				{
					// Tell the join queue module that a player is leaving the game.
					JOINQUEUE_PlayerLeftGame( true );
				}

				pPlayer->health = pPlayer->mo->health = deh.StartHealth;

				if ( bBroadcast )
				{
					// Send out a message saying this player joined the spectators.
					if ( NETWORK_GetState( ) == NETSTATE_SERVER )
						SERVER_Printf( PRINT_HIGH, "%s \\c-joined the spectators.\n", pPlayer->userinfo.netname );
					else
						Printf( "%s \\c-joined the spectators.\n", pPlayer->userinfo.netname );
				}

				// This player no longer has a team affiliation.
				pPlayer->bOnTeam = false;
			}
		}

		// [BB] Make sure the player loses his frags, wins and points.
		if ( NETWORK_InClientMode( ) == false )
			PLAYER_ResetAllScoreCounters ( pPlayer );

		return;
	}

	// [BB] Morphed players need to be unmorphed before being changed to spectators.
	// [WS] This needs to be done before we turn our player into a spectator.
	if (( pPlayer->morphTics ) &&
		( NETWORK_GetState( ) != NETSTATE_CLIENT ) &&
		( CLIENTDEMO_IsPlaying( ) == false ))
	{
		P_UndoPlayerMorph ( pPlayer, pPlayer );
	}

	// Flag this player as being a spectator.
	pPlayer->bSpectating = true;
	pPlayer->bDeadSpectator = bDeadSpectator;
	// [BB] Spectators have to be excluded from the special handling that prevents selection room pistol-fights.
	pPlayer->bUnarmed = false;

	// Run the disconnect scripts if the player is leaving the game.
	if ( bDeadSpectator == false )
	{
		PLAYER_LeavesGame( pPlayer - players );
	}

	// If this player was eligible to get an assist, cancel that.
	if (( NETWORK_GetState( ) != NETSTATE_CLIENT ) && ( CLIENTDEMO_IsPlaying( ) == false ))
		TEAM_CancelAssistsOfPlayer ( static_cast<unsigned>( pPlayer - players ) );

	if ( pPlayer->mo )
	{
		// [BB] Stop all scripts of the player that are still running.
		if ( !( compatflags2 & COMPATF2_DONT_STOP_PLAYER_SCRIPTS_ON_DISCONNECT ) )
			FBehavior::StaticStopMyScripts ( pPlayer->mo );
		// Before we start fucking with the player's body, drop important items
		// like flags, etc.
		if (( NETWORK_GetState( ) != NETSTATE_CLIENT ) && ( CLIENTDEMO_IsPlaying( ) == false ))
			pPlayer->mo->DropImportantItems( false );

		// Take away all of the player's inventory.
		// [BB] Needs to be done before G_DoReborn is called for dead spectators. Otherwise ReadyWeapon is not NULLed.
		pPlayer->mo->DestroyAllInventory( );

		// Is this player tagged as a dead spectator, give him life.
		pPlayer->playerstate = PST_LIVE;
		if ( bDeadSpectator == false )
		{
			pPlayer->health = pPlayer->mo->health = deh.StartHealth;
			// [BB] Spectators don't crouch. We don't need to uncrouch dead spectators, that is done automatically
			// when they are reborn as dead spectator.
			pPlayer->Uncrouch();
		}

		// If this player is being forced into spectatorship, don't destroy his or her
		// old body.
		if ( bDeadSpectator )
		{
			// Save the player's old body, and respawn him or her.
			pOldBody = pPlayer->mo;
			G_DoReborn( pPlayer - players, false );

			// Set the player's new body to the position of his or her old body.
			if (( pPlayer->mo ) &&
				( pOldBody ))
			{
				// [BB] It's possible that the old body is at a place that's inaccessible to spectators
				// (whatever source killed the player possibly moved the body after the player's death).
				// If that's the case, don't move the spectator to the old body position, but to the place
				// where G_DoReborn spawned him.
				fixed_t playerSpawnX = pPlayer->mo->x;
				fixed_t playerSpawnY = pPlayer->mo->y;
				fixed_t playerSpawnZ = pPlayer->mo->z;
				pPlayer->mo->SetOrigin( pOldBody->x, pOldBody->y, pOldBody->z );
				if ( P_TestMobjLocation ( pPlayer->mo ) == false )
					pPlayer->mo->SetOrigin( playerSpawnX, playerSpawnY, playerSpawnZ );

				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_MoveLocalPlayer( ULONG( pPlayer - players ));
			}
		}
		// [BB] In case the player is not respawned as dead spectator, we have to manually clear its TID.
		else
		{
			pPlayer->mo->RemoveFromHash ();
			pPlayer->mo->tid = 0;
		}

		// [BB] Set a bunch of stuff, e.g. make the player unshootable, etc.
		PLAYER_SetDefaultSpectatorValues ( pPlayer );

		// [BB] We also need to stop all sounds associated to the player pawn, spectators
		// aren't supposed to make any sounds. This is especially crucial if a looping sound
		// is played by the player pawn.
		S_StopAllSoundsFromActor ( pPlayer->mo );

		if (( bDeadSpectator == false ) && bBroadcast )
		{
			// Send out a message saying this player joined the spectators.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVER_Printf( PRINT_HIGH, "%s \\c-joined the spectators.\n", pPlayer->userinfo.netname );
			else
				Printf( "%s \\c-joined the spectators.\n", pPlayer->userinfo.netname );
		}
	}

	// This player no longer has a team affiliation.
	if ( bDeadSpectator == false )
		pPlayer->bOnTeam = false;

	// Player's lose all their frags when they become a spectator.
	if ( bDeadSpectator == false )
	{
		// [BB] Make sure the player loses his frags, wins and points.
		if ( NETWORK_InClientMode( ) == false )
			PLAYER_ResetAllScoreCounters ( pPlayer );

		// Also, tell the joinqueue module that a player has left the game.
		if (( NETWORK_GetState( ) != NETSTATE_CLIENT ) && ( CLIENTDEMO_IsPlaying( ) == false ))
		{
			// Tell the join queue module that a player is leaving the game.
			JOINQUEUE_PlayerLeftGame( true );
		}

		if ( pPlayer->pSkullBot )
			pPlayer->pSkullBot->PostEvent( BOTEVENT_SPECTATING );
	}

	// Update this player's info on the scoreboard.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCONSOLE_UpdatePlayerInfo( pPlayer - players, UDF_FRAGS );

	// [Dusk] If we left the game, we need to rebuild player translations if we overrid them.
	if (( NETWORK_GetState() != NETSTATE_SERVER ) && ( cl_overrideplayercolors ) && ( pPlayer - players == consoleplayer ))
		R_BuildAllPlayerTranslations();
}

//*****************************************************************************
//
void PLAYER_SetDefaultSpectatorValues( player_t *pPlayer )
{
	if ( ( pPlayer == NULL ) || ( pPlayer->mo == NULL ) )
		return;

	// Make the player unshootable, etc.
	pPlayer->mo->flags2 |= (MF2_CANNOTPUSH|MF2_SLIDE|MF2_THRUACTORS);
	pPlayer->mo->flags &= ~(MF_CORPSE|MF_SOLID|MF_SHOOTABLE|MF_PICKUP);
	pPlayer->mo->flags2 &= ~(MF2_PASSMOBJ|MF2_FLOATBOB);
	pPlayer->mo->flags3 = MF3_NOBLOCKMONST;
	pPlayer->mo->flags4 = 0;
	pPlayer->mo->flags5 = 0;
	pPlayer->mo->RenderStyle = STYLE_None;

	// [BB] Speed and viewheight of spectators should be independent of the player class.
	pPlayer->mo->Speed = FRACUNIT;
	pPlayer->mo->ForwardMove1 = pPlayer->mo->ForwardMove2 = FRACUNIT;
	pPlayer->mo->SideMove1 = pPlayer->mo->SideMove2 = FRACUNIT;
	pPlayer->mo->ViewHeight = 41*FRACUNIT;
	// [BB] Also can't hurt to reset gravity.
	pPlayer->mo->gravity = FRACUNIT;

	// Make the player flat, so he can travel under doors and such.
	pPlayer->mo->height = 0;

	// [Dusk] Player is now a spectator so he no longer is damaged by anything.
	pPlayer->mo->DamageType = NAME_None;

	// Make monsters unable to "see" this player.
	pPlayer->cheats |= CF_NOTARGET;

	// Reset a bunch of other stuff.
	pPlayer->extralight = 0;
	pPlayer->fixedcolormap = 0;
	pPlayer->damagecount = 0;
	pPlayer->bonuscount = 0;
	pPlayer->hazardcount= 0;
	pPlayer->poisoncount = 0;
	pPlayer->inventorytics = 0;

	// [BB] Reset any screen fade.
	pPlayer->BlendR = 0;
	pPlayer->BlendG = 0;
	pPlayer->BlendB = 0;
	pPlayer->BlendA = 0;

	// [BB] Also cancel any active faders.
	{
		TThinkerIterator<DFlashFader> iterator;
		DFlashFader *fader;

		while ( (fader = iterator.Next()) )
		{
			if ( fader->WhoFor() == pPlayer->mo )
			{
				fader->Cancel ();
			}
		}
	}

	// [BB] Remove all dynamic lights associated to the player's body.
	for ( unsigned int i = 0; i < pPlayer->mo->dynamiclights.Size(); ++i )
		pPlayer->mo->dynamiclights[i]->Destroy();
	pPlayer->mo->dynamiclights.Clear();
}

//*****************************************************************************
//
void PLAYER_SpectatorJoinsGame( player_t *pPlayer )
{
	if ( pPlayer == NULL )
		return;

	pPlayer->playerstate = PST_ENTERNOINVENTORY;
	// [BB] Mark the spectator body as obsolete, but don't delete it before the
	// player gets a new body.
	if ( pPlayer->mo && pPlayer->bSpectating )
	{
		pPlayer->mo->ulSTFlags |= STFL_OBSOLETE_SPECTATOR_BODY;
		// [BB] Also stop all associated scripts. Otherwise they would get disassociated
		// and continue to run even if the player disconnects later.
		if ( !( compatflags2 & COMPATF2_DONT_STOP_PLAYER_SCRIPTS_ON_DISCONNECT ) )
			FBehavior::StaticStopMyScripts (pPlayer->mo);
	}

	pPlayer->bSpectating = false;
	pPlayer->bDeadSpectator = false;

	// [BB] If the spectator used the chasecam or noclip cheat (which is always allowed for spectators)
	// remove it now that he joins the game.
	if ( pPlayer->cheats & ( CF_CHASECAM|CF_NOCLIP ))
	{
		pPlayer->cheats &= ~(CF_CHASECAM|CF_NOCLIP);
		if ( NETWORK_GetState() == NETSTATE_SERVER  )
			SERVERCOMMANDS_SetPlayerCheats( static_cast<ULONG>( pPlayer - players ), static_cast<ULONG>( pPlayer - players ), SVCF_ONLYTHISCLIENT );
	}

	// [BB] If he's a bot, tell him that he successfully joined.
	if ( pPlayer->pSkullBot )
		pPlayer->pSkullBot->PostEvent( BOTEVENT_JOINEDGAME );
}

//*****************************************************************************
//
void PLAYER_SetWins( player_t *pPlayer, ULONG ulWins )
{
	// Set the player's fragcount.
	pPlayer->ulWins = ulWins;

	// Refresh the HUD since a score has changed.
	SCOREBOARD_RefreshHUD( );

	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		// If we're the server, notify the clients of the win count change.
		SERVERCOMMANDS_SetPlayerWins( pPlayer - players );

		// Also, update the scoreboard.
		SERVERCONSOLE_UpdatePlayerInfo( pPlayer - players, UDF_FRAGS );
		SERVERCONSOLE_UpdateScoreboard( );
	}
}

//*****************************************************************************
//
void PLAYER_GetName( player_t *pPlayer, char *pszOutBuf )
{
	// Build the buffer, which has a "remove color code" tag at the end of it.
	sprintf( pszOutBuf, "%s\\c-", pPlayer->userinfo.netname );
}

//*****************************************************************************
//
LONG PLAYER_GetHealth( ULONG ulPlayer )
{
	return players[ulPlayer].health;
}

//*****************************************************************************
//
LONG PLAYER_GetLivesLeft( ULONG ulPlayer )
{
	return players[ulPlayer].ulLivesLeft;
}

//*****************************************************************************
//
void PLAYER_SelectPlayersWithHighestValue ( LONG (*GetValue) ( ULONG ulPlayer ), TArray<ULONG> &Players )
{
	LONG lHighestValue;

	TArray<ULONG> selectedPlayers;

	for ( unsigned int i = 0; i < Players.Size(); ++i )
	{
		const ULONG ulIdx = Players[i];

		if (( playeringame[ulIdx] == false ) ||
			( players[ulIdx].bSpectating ))
		{
			continue;
		}

		if ( selectedPlayers.Size() == 0 )
		{
			lHighestValue = GetValue ( ulIdx );
			selectedPlayers.Push ( ulIdx );
		}
		else
		{
			if ( GetValue ( ulIdx ) > lHighestValue )
			{
				lHighestValue = GetValue ( ulIdx );
				selectedPlayers.Clear();
				selectedPlayers.Push ( ulIdx );
			}
			else if ( GetValue ( ulIdx ) == lHighestValue )
				selectedPlayers.Push ( ulIdx );
		}
	}
	Players = selectedPlayers;
}

//*****************************************************************************
//
bool PLAYER_IsValidPlayer( const ULONG ulPlayer )
{
	// If the player index is out of range, or this player is not in the game, then the
	// player index is not valid.
	if (( ulPlayer >= MAXPLAYERS ) || ( playeringame[ulPlayer] == false ))
		return ( false );

	return ( true );
}

//*****************************************************************************
//
bool PLAYER_IsValidPlayerWithMo( const ULONG ulPlayer )
{
	return ( PLAYER_IsValidPlayer ( ulPlayer ) && players[ulPlayer].mo );
}

//*****************************************************************************
//
bool PLAYER_IsTrueSpectator( player_t *pPlayer )
{
	if ( GAMEMODE_GetFlags( GAMEMODE_GetCurrentMode( )) & GMF_DEADSPECTATORS )
		return (( pPlayer->bSpectating ) && ( pPlayer->bDeadSpectator == false ));

	return ( pPlayer->bSpectating );
}

//*****************************************************************************
//
void PLAYER_CheckStruckPlayer( AActor *pActor )
{
	if ( pActor && pActor->player )
	{
		if ( pActor->player->bStruckPlayer )
			PLAYER_StruckPlayer( pActor->player );
		else
			pActor->player->ulConsecutiveHits = 0;
	}
}

//*****************************************************************************
//
void PLAYER_StruckPlayer( player_t *pPlayer )
{
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) || ( CLIENTDEMO_IsPlaying( )))
		return;

	pPlayer->ulConsecutiveHits++;

	// If the player has made 5 straight consecutive hits with a weapon, award a medal.
	if (( pPlayer->ulConsecutiveHits % 5 ) == 0 )
	{
		// If this player has made 10+ consecuvite hits, award a "Precision" medal.
		if ( pPlayer->ulConsecutiveHits >= 10 )
		{
			MEDAL_GiveMedal( pPlayer - players, MEDAL_PRECISION );

			// Tell clients about the medal that been given.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_GivePlayerMedal( pPlayer - players, MEDAL_PRECISION );
		}
		// Otherwise, award an "Accuracy" medal.
		else
		{
			MEDAL_GiveMedal( pPlayer - players, MEDAL_ACCURACY );

			// Tell clients about the medal that been given.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_GivePlayerMedal( pPlayer - players, MEDAL_ACCURACY );
		}
	}

	// Reset the struck player flag.
	pPlayer->bStruckPlayer = false;
}

//*****************************************************************************
//
bool PLAYER_ShouldSpawnAsSpectator( player_t *pPlayer )
{
	UCVarValue	Val;

	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		// [BB] Possibly check if the player is authenticated.
		if ( sv_forcelogintojoin )
		{
			const ULONG ulPlayer = static_cast<ULONG> ( pPlayer - players );
			if ( SERVER_IsValidClient ( ulPlayer ) && SERVER_GetClient ( ulPlayer )->loggedIn == false )
				return true;
		}

		// If there's a join password, the player should start as a spectator.
		Val = sv_joinpassword.GetGenericRep( CVAR_String );
		if (( sv_forcejoinpassword ) && ( strlen( Val.String )))
		{
			// [BB] Only force the player to start as spectator if he didn't already join.
			// In that case the join password was already checked.
			const ULONG ulPlayer = static_cast<ULONG> ( pPlayer - players );
			if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
				return ( true );

			// [BB] If the player is already spectating, he should still spawn as spectator.
			if ( pPlayer->bSpectating )
				return ( true );

			// [BB] If this is a client that hasn't been spawned yet, he didn't pass the password check yet, so force him to start as spectator.
			if ( ( pPlayer->bIsBot == false ) && ( SERVER_GetClient( ulPlayer )->State < CLS_SPAWNED_BUT_NEEDS_AUTHENTICATION ) )
				return ( true );
		}
	}

	// [BB] Check if the any reason prevents this particular player from joining.
	if ( GAMEMODE_PreventPlayersFromJoining( static_cast<ULONG> ( pPlayer - players ) ) )
		return ( true );

	// Players entering a teamplay game must choose a team first before joining the fray.
	if (( pPlayer->bOnTeam == false ) || ( playeringame[pPlayer - players] == false ))
	{
		if ( ( GAMEMODE_GetFlags( GAMEMODE_GetCurrentMode( )) & GMF_PLAYERSONTEAMS ) &&
			( !( GAMEMODE_GetFlags( GAMEMODE_GetCurrentMode( )) & GMF_TEAMGAME ) || ( TemporaryTeamStarts.Size( ) == 0 ) ) &&
			(( dmflags2 & DF2_NO_TEAM_SELECT ) == false ))
		{
			return ( true );
		}
	}

	// Passed all checks!
	return ( false );
}

//*****************************************************************************
//
bool PLAYER_Taunt( player_t *pPlayer )
{
	// Don't taunt if we're not in a level!
	if ( gamestate != GS_LEVEL )
		return ( false );

	// Spectators or dead people can't taunt!
	if (( pPlayer->bSpectating ) ||
		( pPlayer->health <= 0 ) ||
		( pPlayer->mo == NULL ) ||
		( i_compatflags & COMPATF_DISABLETAUNTS ))
	{
		return ( false );
	}

	if ( cl_taunts )
		S_Sound( pPlayer->mo, CHAN_VOICE, "*taunt", 1, ATTN_NORM );

	return ( true );
}

//*****************************************************************************
//
LONG PLAYER_GetRailgunColor( player_t *pPlayer )
{
	// Determine the railgun trail's color.
	switch ( pPlayer->userinfo.lRailgunTrailColor )
	{
	case RAILCOLOR_BLUE:
	default:

		return ( V_GetColorFromString( NULL, "00 00 ff" ));
	case RAILCOLOR_RED:

		return ( V_GetColorFromString( NULL, "ff 00 00" ));
	case RAILCOLOR_YELLOW:

		return ( V_GetColorFromString( NULL, "ff ff 00" ));
	case RAILCOLOR_BLACK:

		return ( V_GetColorFromString( NULL, "0f 0f 0f" ));
	case RAILCOLOR_SILVER:

		return ( V_GetColorFromString( NULL, "9f 9f 9f" ));
	case RAILCOLOR_GOLD:

		return ( V_GetColorFromString( NULL, "bf 8f 2f" ));
	case RAILCOLOR_GREEN:

		return ( V_GetColorFromString( NULL, "00 ff 00" ));
	case RAILCOLOR_WHITE:

		return ( V_GetColorFromString( NULL, "ff ff ff" ));
	case RAILCOLOR_PURPLE:

		return ( V_GetColorFromString( NULL, "ff 00 ff" ));
	case RAILCOLOR_ORANGE:

		return ( V_GetColorFromString( NULL, "ff 8f 00" ));
	case RAILCOLOR_RAINBOW:

		return ( -2 );
	}
}

//*****************************************************************************
//
void PLAYER_AwardDamagePointsForAllPlayers( void )
{
	if ( (dmflags2 & DF2_AWARD_DAMAGE_INSTEAD_KILLS)
		&& (GAMEMODE_GetFlags( GAMEMODE_GetCurrentMode( )) & GMF_PLAYERSEARNKILLS) )
	{
		for ( ULONG ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
		{
			if ( playeringame[ulIdx] == false )
				continue;

			player_t *p = &players[ulIdx];

			int points = p->ulUnrewardedDamageDealt / 100;
			if ( points > 0 )
			{
				p->lPointCount += points;
				p->ulUnrewardedDamageDealt -= 100 * points;

				// Update the clients with this player's updated points.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				{
					SERVERCOMMANDS_SetPlayerPoints( static_cast<ULONG>( p - players ) );

					// Also, update the scoreboard.
					SERVERCONSOLE_UpdatePlayerInfo( static_cast<ULONG>( p - players ), UDF_FRAGS );
					SERVERCONSOLE_UpdateScoreboard( );
				}
			}
		}
	}
}

//*****************************************************************************
//
void PLAYER_SetWeapon( player_t *pPlayer, AWeapon *pWeapon, bool bClearWeaponForClientOnServer )
{
	// [BB] Validity check.
	if ( pPlayer == NULL )
		return;

	// [BB] If the server should just clear the weapon for the client (and this player is a client and not a bot), do so and return.
	// [BB] The server also has to do the weapon change if the coressponding client is still loading the level.
	if ( bClearWeaponForClientOnServer && ( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( pPlayer->bIsBot == false ) && ( SERVER_GetClient( pPlayer - players )->State != CLS_SPAWNED_BUT_NEEDS_AUTHENTICATION ) )
	{
		PLAYER_ClearWeapon ( pPlayer );
		// [BB] Since PLAYER_SetWeapon is hopefully only called with bClearWeaponForClientOnServer == true in 
		// APlayerPawn::GiveDefaultInventory() assume that the weapon here is the player's starting weapon.
		// PLAYER_SetWeapon is possibly called multiple times, but the last call should be the starting weapon.
		pPlayer->StartingWeaponName = pWeapon ? pWeapon->GetClass()->TypeName : NAME_None;
		return;
	}

	// Set the ready and pending weapon.
	// [BB] When playing a client side demo, the weapon for the consoleplayer will
	// be selected by a recorded CLD_LCMD_INVUSE command.
	if ( ( CLIENTDEMO_IsPlaying() == false ) || ( pPlayer - players ) != consoleplayer )
		pPlayer->ReadyWeapon = pPlayer->PendingWeapon = pWeapon;

	// [BC] If we're a client, tell the server we're switching weapons.
	// [BB] It's possible, that a mod doesn't give the player any weapons. Therefore we also must check pWeapon
	// and can allow PLAYER_SetWeapon to be called with pWeapon == NULL.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) && (( pPlayer - players ) == consoleplayer ) && pWeapon )
	{
		CLIENTCOMMANDS_WeaponSelect( pWeapon->GetClass( ));

		if ( CLIENTDEMO_IsRecording( ))
			CLIENTDEMO_WriteLocalCommand( CLD_LCMD_INVUSE, pWeapon->GetClass( )->TypeName.GetChars( ) );
	}
	// [BB] Make sure to inform clients of bot weapon changes.
	else if ( ( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( pPlayer->bIsBot == true ) )
		SERVERCOMMANDS_SetPlayerPendingWeapon( static_cast<ULONG> ( pPlayer - players ) );
}

//*****************************************************************************
//
void PLAYER_ClearWeapon( player_t *pPlayer )
{
	// [BB] Validity check.
	if ( pPlayer == NULL )
		return;

	pPlayer->ReadyWeapon = NULL;
	pPlayer->PendingWeapon = WP_NOCHANGE;
	pPlayer->psprites[ps_weapon].state = NULL;
	pPlayer->psprites[ps_flash].state = NULL;
	// [BB] Assume that it was not the client's decision to clear the weapon.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		pPlayer->bClientSelectedWeapon = false;
}

//*****************************************************************************
//
void PLAYER_SetLivesLeft( player_t *pPlayer, ULONG ulLivesLeft )
{
	// [BB] Validity check.
	if ( pPlayer == NULL )
		return;

	pPlayer->ulLivesLeft = ulLivesLeft;

	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SetPlayerLivesLeft ( static_cast<ULONG> ( pPlayer - players ) );
}

//*****************************************************************************
//
bool PLAYER_IsAliveOrCanRespawn( player_t *pPlayer )
{
	// [BB] Validity check.
	if ( pPlayer == NULL )
		return false;

	return ( ( pPlayer->health > 0 ) || ( pPlayer->ulLivesLeft > 0 ) || ( pPlayer->bSpawnTelefragged == true ) || ( pPlayer->playerstate != PST_DEAD ) );
}

//*****************************************************************************
//
void PLAYER_RemoveFriends( const ULONG ulPlayer )
{
	// [BB] Validity check.
	if ( ulPlayer >= MAXPLAYERS )
		return;

	AActor *pActor = NULL;
	TThinkerIterator<AActor> iterator;

	while ( (pActor = iterator.Next ()) )
	{
		if ((pActor->flags3 & MF3_ISMONSTER) &&
			(pActor->flags & MF_FRIENDLY) &&
			pActor->FriendPlayer &&
			( (ULONG)( pActor->FriendPlayer - 1 ) == ulPlayer) )
		{
			pActor->FriendPlayer = 0;
			if ( !(GAMEMODE_GetFlags( GAMEMODE_GetCurrentMode( )) & GMF_COOPERATIVE)
				// [BB] In a gamemode with teams, monsters with DesignatedTeam need to keep MF_FRIENDLY.
				&& ( !(GAMEMODE_GetFlags( GAMEMODE_GetCurrentMode( )) & GMF_PLAYERSONTEAMS) || !TEAM_CheckIfValid ( pActor->DesignatedTeam ) ) )
				pActor->flags &= ~MF_FRIENDLY; // this is required in DM or monsters will be friendly to all players
		}
	}
}

//*****************************************************************************
//
void PLAYER_LeavesGame( const ULONG ulPlayer )
{
	// [BB] Validity check.
	if ( ulPlayer >= MAXPLAYERS )
		return;

	// [BB] Offline we need to create the disconnect particle effect here.
	if ( ( ( NETWORK_GetState( ) == NETSTATE_SINGLE ) || ( NETWORK_GetState( ) == NETSTATE_SINGLE_MULTIPLAYER ) ) && players[ulPlayer].mo )
		P_DisconnectEffect( players[ulPlayer].mo );

	// Run the disconnect scripts now that the player is leaving.
	if (( NETWORK_GetState( ) != NETSTATE_CLIENT ) &&
		( CLIENTDEMO_IsPlaying( ) == false ))
	{
		FBehavior::StaticStartTypedScripts( SCRIPT_Disconnect, NULL, true, ulPlayer );
		PLAYER_RemoveFriends ( ulPlayer );
		PLAYER_ClearEnemySoundFields( ulPlayer );
	}

	// [BB] Clear the players medals and the medal related counters. The former is something also clients need to do.
	memset( players[ulPlayer].ulMedalCount, 0, sizeof( ULONG ) * NUM_MEDALS );
	PLAYER_ResetSpecialCounters ( &players[ulPlayer] );
}

//*****************************************************************************
//
// [Dusk] Remove sound targets from the given player
//
void PLAYER_ClearEnemySoundFields( const ULONG ulPlayer )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	TThinkerIterator<AActor> it;
	AActor* mo;

	while (( mo = it.Next() ) != NULL )
	{
		if ( mo->LastHeard != NULL && mo->LastHeard->player == &players[ulPlayer] )
			mo->LastHeard = NULL;
	}

	for ( int i = 0; i < numsectors; ++i )
	{
		if ( sectors[i].SoundTarget != NULL && sectors[i].SoundTarget->player == &players[ulPlayer] )
			sectors[i].SoundTarget = NULL;
	}
}

CCMD (kill)
{
	// Only allow it in a level.
	if ( gamestate != GS_LEVEL )
		return;

	// [BB] The server can't suicide, so it can ignore this checks.
	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
	{
		// [BC] Don't let spectators kill themselves.
		if ( players[consoleplayer].bSpectating == true )
			return;

		// [BC] Don't allow suiciding during a duel.
		if ( duel && ( DUEL_GetState( ) == DS_INDUEL ))
		{
			Printf( "You cannot suicide during a duel.\n" );
			return;
		}
	}

	if (argv.argc() > 1)
	{
		// [BB] Special handling for "kill monsters" on the server: It's allowed
		// independent of the sv_cheats setting and bypasses the DEM_* handling.
		if ( ( NETWORK_GetState( ) == NETSTATE_SERVER ) && !stricmp (argv[1], "monsters") )
		{
			const int killcount = P_Massacre ();
			SERVER_Printf( PRINT_HIGH, "%d Monster%s Killed\n", killcount, killcount==1 ? "" : "s" );
			return;
		}

		if (CheckCheatmode ())
			return;

		if (!stricmp (argv[1], "monsters"))
		{
			// Kill all the monsters
			if (CheckCheatmode ())
				return;

			Net_WriteByte (DEM_GENERICCHEAT);
			Net_WriteByte (CHT_MASSACRE);
		}
		else
		{
			Net_WriteByte (DEM_KILLCLASSCHEAT);
			Net_WriteString (argv[1]);
		}
	}
	else
	{
		// If suiciding is disabled, then don't do it.
		if (dmflags2 & DF2_NOSUICIDE)
			return;

		// [BC] Tell the server we wish to suicide.
		if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) && ( players[consoleplayer].bSpectating == false ))
			CLIENTCOMMANDS_Suicide( );

		// Kill the player
		Net_WriteByte (DEM_SUICIDE);
	}
	C_HideConsole ();
}

//*****************************************************************************
//
CCMD( taunt )
{
	// [BB] No taunting while playing a demo.
	if ( CLIENTDEMO_IsPlaying( ) == true )
	{
		Printf ( "You can't taunt during demo playback.\n" );
		return;
	}

	if ( PLAYER_Taunt( &players[consoleplayer] ))
	{
		// Tell the server we taunted!
		if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
			CLIENTCOMMANDS_Taunt( );

		if ( CLIENTDEMO_IsRecording( ))
			CLIENTDEMO_WriteLocalCommand( CLD_LCMD_TAUNT, NULL );
	}
}
