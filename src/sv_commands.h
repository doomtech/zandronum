//-----------------------------------------------------------------------------
//
// Skulltag Source
// Copyright (C) 2003 Brad Carney
// Copyright (C) 2007-2012 Skulltag Development Team
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
// 3. Neither the name of the Skulltag Development Team nor the names of its
//    contributors may be used to endorse or promote products derived from this
//    software without specific prior written permission.
// 4. Redistributions in any form must be accompanied by information on how to
//    obtain complete source code for the software and any accompanying
//    software that uses the software. The source code must either be included
//    in the distribution or be available for no more than the cost of
//    distribution plus a nominal fee, and must be freely redistributable
//    under reasonable conditions. For an executable file, complete source
//    code means the source code for all modules it contains. It does not
//    include source code for modules or files that typically accompany the
//    major components of the operating system on which the executable file
//    runs.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
//
//
// Filename: sv_commands.h
//
// Description: Contains prototypes for a set of functions that correspond to each
// message a server can send out. Each functions handles the send out of each message.
//
//-----------------------------------------------------------------------------

#ifndef __SV_COMMANDS_H__
#define __SV_COMMANDS_H__

#include "doomtype.h"
#include "network.h"
#include "p_local.h"
#include "p_spec.h"
#include "vectors.h"

//*****************************************************************************
//	DEFINES

#define	SVCF_SKIPTHISCLIENT			( 1 << 0 )
#define	SVCF_ONLYTHISCLIENT			( 1 << 1 )

//*****************************************************************************
//	PROTOTYPES

// General protocol commands. These handle connecting to and being part of the server.
void	SERVERCOMMANDS_Ping( ULONG ulTime );
void	SERVERCOMMANDS_Nothing( ULONG ulPlayer );
void	SERVERCOMMANDS_ResetSequence( ULONG ulPlayer );
void	SERVERCOMMANDS_BeginSnapshot( ULONG ulPlayer );
void	SERVERCOMMANDS_EndSnapshot( ULONG ulPlayer );

// Player commands. These involve manipulating a player in some way.
void	SERVERCOMMANDS_SpawnPlayer( ULONG ulPlayer, LONG lPlayerState, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_MovePlayer( ULONG ulPlayer, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_DamagePlayer( ULONG ulPlayer );
void	SERVERCOMMANDS_KillPlayer( ULONG ulPlayer, AActor *pSource, AActor *pInflictor, ULONG ulMOD );
void	SERVERCOMMANDS_SetPlayerHealth( ULONG ulPlayer, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_UpdatePlayerArmorDisplay( ULONG ulPlayer);
void	SERVERCOMMANDS_SetPlayerState( ULONG ulPlayer, ULONG ulState, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_SetPlayerUserInfo( ULONG ulPlayer, ULONG ulUserInfoFlags, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_SetPlayerFrags( ULONG ulPlayer, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_SetPlayerPoints( ULONG ulPlayer, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_SetPlayerWins( ULONG ulPlayer, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_SetPlayerKillCount( ULONG ulPlayer, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_SetPlayerChatStatus( ULONG ulPlayer, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_SetPlayerLaggingStatus( ULONG ulPlayer, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_SetPlayerReadyToGoOnStatus( ULONG ulPlayer, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_SetPlayerTeam( ULONG ulPlayer, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_SetPlayerCamera( ULONG ulPlayer, LONG lCameraNetID, bool bRevertPlease );
void	SERVERCOMMANDS_UpdatePlayerPing( ULONG ulPlayer, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_UpdatePlayerExtraData( ULONG ulPlayer, ULONG ulDisplayPlayer );
void	SERVERCOMMANDS_UpdatePlayerPendingWeapon( ULONG ulPlayer );
void	SERVERCOMMANDS_DoInventoryUse( ULONG ulPlayer, AInventory *pItem, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_ChangePlayerWeapon( ULONG ulPlayer );
void	SERVERCOMMANDS_UpdatePlayerTime( ULONG ulPlayer, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_MoveLocalPlayer( ULONG ulPlayer );
void	SERVERCOMMANDS_DisconnectPlayer( ULONG ulPlayer, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_SetConsolePlayer( ULONG ulPlayer );
void	SERVERCOMMANDS_ConsolePlayerKicked( ULONG ulPlayer );
void	SERVERCOMMANDS_GivePlayerMedal( ULONG ulPlayer, ULONG ulMedal, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_ResetAllPlayersFragcount( ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_PlayerIsSpectator( ULONG ulPlayer, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_PlayerSay( ULONG ulPlayer, char *pszString, ULONG ulMode, bool bForbidChatToPlayers, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_PlayerTaunt( ULONG ulPlayer, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_PlayerRespawnInvulnerability( ULONG ulPlayer, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_SetPlayerAmmoCapacity( ULONG ulPlayer, AInventory *pAmmo, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_SetPlayerCheats( ULONG ulPlayer, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );

// Thing commands. This involve handling of actors.
void	SERVERCOMMANDS_SpawnThing( AActor *pActor, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_SpawnThingNoNetID( AActor *pActor, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0, bool bSendTranslation = false );
void	SERVERCOMMANDS_SpawnThingExact( AActor *pActor, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_SpawnThingExactNoNetID( AActor *pActor, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_MoveThing( AActor *pActor, ULONG ulBits, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_MoveThingExact( AActor *pActor, ULONG ulBits, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_DamageThing( AActor *pActor );
void	SERVERCOMMANDS_KillThing( AActor *pActor );
void	SERVERCOMMANDS_SetThingState( AActor *pActor, ULONG ulState );
void	SERVERCOMMANDS_DestroyThing( AActor *pActor );
void	SERVERCOMMANDS_SetThingAngle( AActor *pActor, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_SetThingWaterLevel( AActor *pActor, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_SetThingFlags( AActor *pActor, ULONG ulFlagSet, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_SetThingArguments( AActor *pActor, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_SetThingTranslation( AActor *pActor, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_SetThingProperty( AActor *pActor, ULONG ulProperty, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_SetThingSound( AActor *pActor, ULONG ulSound, char *pszSound, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_SetThingSpecial2( AActor *pActor, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_SetThingTID( AActor *pActor, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_SetThingTics( AActor *pActor, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_SetWeaponAmmoGive( AActor *pActor, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_ThingIsCorpse( AActor *pActor, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_HideThing( AActor *pActor, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_TeleportThing( AActor *pActor, bool bSourceFog, bool bDestFog, bool bTeleZoom, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_ThingActivate( AActor *pActor, AActor *pActivator, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_ThingDeactivate( AActor *pActor, AActor *pActivator, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_RespawnDoomThing( AActor *pActor, bool bFog, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_RespawnRavenThing( AActor *pActor, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );

// Print commands. These print some sort of message to the screen.
void	SERVERCOMMANDS_Print( char *pszString, ULONG ulPrintLevel, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_PrintMid( char *pszString, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_PrintMOTD( char *pszString, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_PrintHUDMessage( char *pszString, float fX, float fY, LONG lHUDWidth, LONG lHUDHeight, LONG lColor, float fHoldTime, char *pszFont, LONG lID, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_PrintHUDMessageFadeOut( char *pszString, float fX, float fY, LONG lHUDWidth, LONG lHUDHeight, LONG lColor, float fHoldTime, float fFadeOutTime, char *pszFont, LONG lID, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_PrintHUDMessageFadeInOut( char *pszString, float fX, float fY, LONG lHUDWidth, LONG lHUDHeight, LONG lColor, float fHoldTime, float fFadeInTime, float fFadeOutTime, char *pszFont, LONG lID, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_PrintHUDMessageTypeOnFadeOut( char *pszString, float fX, float fY, LONG lHUDWidth, LONG lHUDHeight, LONG lColor, float fTypeTime, float fHoldTime, float fFadeOutTime, char *pszFont, LONG lID, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );

// Game commands. These manipulate some aspect of the gameplay.
void	SERVERCOMMANDS_SetGameMode( ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_SetGameSkill( ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_SetGameDMFlags( ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_SetGameModeLimits( ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_SetGameEndLevelDelay( ULONG ulEndLevelDelay, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_SetGameModeState( ULONG ulState, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_SetDuelNumDuels( ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_SetLMSSpectatorSettings( ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_SetLMSAllowedWeapons( ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_SetInvasionNumMonstersLeft( ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_SetInvasionWave( ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_DoPossessionArtifactPickedUp( ULONG ulPlayer, ULONG ulTicks, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_DoPossessionArtifactDropped( ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_DoGameModeFight( ULONG ulCurrentWave, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_DoGameModeCountdown( ULONG ulTicks, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_DoGameModeWinSequence( ULONG ulWinner, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );

// Team commands. These involve one of the teams in teamgame mode.
void	SERVERCOMMANDS_SetTeamFrags( ULONG ulTeam, LONG lFrags, bool bAnnounce, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_SetTeamScore( ULONG ulTeam, LONG lScore, bool bAnnounce, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_SetTeamWins( ULONG ulTeam, LONG lWins, bool bAnnounce, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_SetTeamReturnTicks( ULONG ulTeam, ULONG ulReturnTicks, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_TeamFlagReturned( ULONG ulTeam, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );

// Missile commands. These handle missiles in some way.
void	SERVERCOMMANDS_SpawnMissile( AActor *pMissile, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_SpawnMissileExact( AActor *pMissile, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_MissileExplode( AActor *pMissile, line_t *pLine, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );

// Weapon commands. These handle firing weapons, weapon changes, etc.
void	SERVERCOMMANDS_WeaponSound( ULONG ulPlayer, char *pszSound, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_WeaponChange( ULONG ulPlayer, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_WeaponRailgun( AActor *pSource, const FVector3 &Start, const FVector3 &End, LONG lColor1, LONG lColor2, float fMaxDiff, bool bSilent, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );

// Sector commands. These manipulate some property of sectors.
void	SERVERCOMMANDS_SetSectorFloorPlane( ULONG ulSector, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_SetSectorCeilingPlane( ULONG ulSector, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_SetSectorLightLevel( ULONG ulSector, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_SetSectorColor( ULONG ulSector, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_SetSectorFade( ULONG ulSector, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_SetSectorFlat( ULONG ulSector, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_SetSectorPanning( ULONG ulSector, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_SetSectorRotation( ULONG ulSector, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_SetSectorScale( ULONG ulSector, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_SetSectorFriction( ULONG ulSector, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_SetSectorAngleYOffset( ULONG ulSector, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_SetSectorGravity( ULONG ulSector, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_StopSectorLightEffect( ULONG ulSector, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_DestroyAllSectorMovers( ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );

// Sector light commands. These start a sector light effect, such as strobe effects.
void	SERVERCOMMANDS_DoSectorLightFireFlicker( ULONG ulSector, LONG lMaxLight, LONG lMinLight, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_DoSectorLightFlicker( ULONG ulSector, LONG lMaxLight, LONG lMinLight, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_DoSectorLightLightFlash( ULONG ulSector, LONG lMaxLight, LONG lMinLight, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_DoSectorLightStrobe( ULONG ulSector, LONG lDarkTime, LONG lBrightTime, LONG lMaxLight, LONG lMinLight, LONG lCount, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_DoSectorLightGlow( ULONG ulSector, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_DoSectorLightGlow2( ULONG ulSector, LONG lStart, LONG lEnd, LONG lTics, LONG lMaxTics, bool bOneShot, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_DoSectorLightPhased( ULONG ulSector, LONG lBaseLevel, LONG lPhase, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );

// Line commands. These have something to do with lines.
void	SERVERCOMMANDS_SetLineAlpha( ULONG ulLine, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_SetLineTexture( ULONG ulLine, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_SetLineBlocking( ULONG ulLine, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );

// Sound commands. These play a sound.
void	SERVERCOMMANDS_Sound( LONG lChannel, char *pszSound, LONG lVolume, LONG lAttenuation, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_SoundID( LONG lX, LONG lY, LONG lChannel, LONG lSoundID, LONG lVolume, LONG lAttenuation, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_SoundActor( AActor *pActor, LONG lChannel, char *pszSound, LONG lVolume, LONG lAttenuation, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_SoundIDActor( AActor *pActor, LONG lChannel, LONG lSoundID, LONG lVolume, LONG lAttenuation, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_SoundPoint( LONG lX, LONG lY, LONG lChannel, char *pszSound, LONG lVolume, LONG lAttenuation, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );

// Sector sequence commands. These handle sector sound sequences.
void	SERVERCOMMANDS_StartSectorSequence( sector_t *pSector, char *pszSequence, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_StopSectorSequence( sector_t *pSector, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );

// Voting commands. These handle the voting.
void	SERVERCOMMANDS_CallVote( ULONG ulPlayer, char *pszCommand, char *pszParameters, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_PlayerVote( ULONG ulPlayer, bool bVoteYes, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_VoteEnded( bool bVotePassed, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );

// Map commands. These load maps, exit maps, or manipulate some property of the current map.
void	SERVERCOMMANDS_MapLoad( ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_MapNew( char *pszMapName, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_MapExit( LONG lPosition, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_MapAuthenticate( char *pszMapName, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_SetMapTime( ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_SetMapNumKilledMonsters( ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_SetMapNumFoundItems( ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_SetMapNumFoundSecrets( ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_SetMapNumTotalMonsters( ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_SetMapNumTotalItems( ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_SetMapMusic( const char *pszMusic, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_SetMapSky( ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );

// Inventory commands. These give the player inventory items, takes them away, etc.
void	SERVERCOMMANDS_GiveInventory( ULONG ulPlayer, AInventory *pInventory, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_TakeInventory( ULONG ulPlayer, char *pszClassName, ULONG ulAmount, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_GivePowerup( ULONG ulPlayer, APowerup *pPowerup, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_DoInventoryPickup( ULONG ulPlayer, char *pszClassName, char *pszPickupMessage, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_DestroyAllInventory( ULONG ulPlayer, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );

// Door commands. These create, destroy, and manipulate doors.
void	SERVERCOMMANDS_DoDoor( sector_t *pSector, LONG lSpeed, LONG lDirection, LONG lLightTag, LONG lID, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_DestroyDoor( LONG lID, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_ChangeDoorDirection( LONG lID, LONG lDirection, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );

// Floor commands. Ditto.
void	SERVERCOMMANDS_DoFloor( DFloor::EFloor Type, sector_t *pSector, LONG lDirection, LONG lSpeed, LONG lFloorDestDist, LONG lID, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_DestroyFloor( LONG lID, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_ChangeFloorDirection( LONG lID, LONG lDirection, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_ChangeFloorType( LONG lID, DFloor::EFloor Type, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_ChangeFloorDestDist( LONG lID, LONG lFloorDestDist, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_StartFloorSound( LONG lID, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );

// Ceiling commands. Ditto.
void	SERVERCOMMANDS_DoCeiling( DCeiling::ECeiling Type, sector_t *pSector, LONG lDirection, LONG lBottomHeight, LONG lTopHeight, LONG lSpeed, LONG lCrush, bool bSilent, LONG lID, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_DestroyCeiling( LONG lID, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_ChangeCeilingDirection( LONG lID, LONG lDirection, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_ChangeCeilingSpeed( LONG lID, LONG lSpeed, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_PlayCeilingSound( LONG lID, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );

// Plat commands. Ditto.
void	SERVERCOMMANDS_DoPlat( DPlat::EPlatType Type, sector_t *pSector, DPlat::EPlatState Status, LONG lHigh, LONG lLow, LONG lSpeed, LONG lID, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_DestroyPlat( LONG lID, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_ChangePlatStatus( LONG lID, DPlat::EPlatState Status, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_PlayPlatSound( LONG lID, LONG lSound, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );

// Elevator commands. Ditto.
void	SERVERCOMMANDS_DoElevator( DElevator::EElevator Type, sector_t *pSector, LONG lSpeed, LONG lDirection, LONG lFloorDestDist, LONG lCeilingDestDist, LONG lID, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_DestroyElevator( LONG lID, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_StartElevatorSound( LONG lID, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );

// Pillar commands. Ditto.
void	SERVERCOMMANDS_DoPillar( DPillar::EPillar Type, sector_t *pSector, LONG lFloorSpeed, LONG lCeilingSpeed, LONG lFloorTarget, LONG lCeilingTarget, LONG lID, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_DestroyPillar( LONG lID, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );

// Waggle commands. Ditto.
void	SERVERCOMMANDS_DoWaggle( bool bCeiling, sector_t *pSector, LONG lOriginalDistance, LONG lAccumulator, LONG lAccelerationDelta, LONG lTargetScale, LONG lScale, LONG lScaleDelta, LONG lTicker, LONG lState, LONG lID, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_DestroyWaggle( LONG lID, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_UpdateWaggle( LONG lID, LONG lAccumulator, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );

// Rotate poly commands. Ditto.
void	SERVERCOMMANDS_DoRotatePoly( LONG lSpeed, LONG lPolyNum, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_DestroyRotatePoly( LONG lPolyNum, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );

// Move poly commands. Ditto.
void	SERVERCOMMANDS_DoMovePoly( LONG lXSpeed, LONG lYSpeed, LONG lPolyNum, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_DestroyMovePoly( LONG lPolyNum, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );

// Poly door commands. Ditto.
void	SERVERCOMMANDS_DoPolyDoor( LONG lType, LONG lXSpeed, LONG lYSpeed, LONG lSpeed, LONG lPolyNum, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_DestroyPolyDoor( LONG lPolyNum, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_SetPolyDoorSpeedPosition( LONG lPolyNum, LONG lXSpeed, LONG lYSpeed, LONG lX, LONG lY, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );

// Generic polyobject commands. These work with polyobjects of any type.
void	SERVERCOMMANDS_PlayPolyobjSound( LONG lPolyNum, NETWORK_POLYOBJSOUND_e Sound, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_SetPolyobjPosition( LONG lPolyNum, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_SetPolyobjRotation( LONG lPolyNum, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );

// Miscellaneous commands. These are commands that don't fit into any other group.
void	SERVERCOMMANDS_Earthquake( AActor *pCenter, LONG lIntensity, LONG lDuration, LONG lTemorRadius, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_SetQueuePosition( ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_DoScroller( LONG lType, LONG lXSpeed, LONG lYSpeed, LONG lAffectee, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_SetScroller( LONG lType, LONG lXSpeed, LONG lYSpeed, LONG lTag, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_SetWallScroller( LONG lId, LONG lSidechoice, LONG lXSpeed, LONG lYSpeed, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_GenericCheat( ULONG ulPlayer, ULONG ulCheat, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_SetCameraToTexture( AActor *pCamera, char *pszTexture, LONG lFOV, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );
void	SERVERCOMMANDS_DoFlashFader( float fR1, float fG1, float fB1, float fA1, float fR2, float fG2, float fB2, float fA2, float fTime, ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 );

#endif	// __SV_COMMANDS_H__
