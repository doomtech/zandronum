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
// Filename: cl_main.cpp
//
// Description: Contains variables and routines related to the client portion
// of the program.
//
//-----------------------------------------------------------------------------

#include "networkheaders.h"
#include "a_action.h"
#include "a_sharedglobal.h"
#include "a_doomglobal.h"
#include "announcer.h"
#include "browser.h"
#include "cl_commands.h"
#include "cl_demo.h"
#include "cl_statistics.h"
#include "cooperative.h"
#include "doomerrors.h"
#include "doomtype.h"
#include "doomstat.h"
#include "d_player.h"
#include "g_game.h"
#include "gamemode.h"
#include "d_net.h"
#include "p_local.h"
#include "s_sound.h"
#include "gi.h"
#include "i_net.h"
#include "i_system.h"
#include "c_dispatch.h"
#include "st_stuff.h"
#include "m_argv.h"
#include "m_cheat.h"
#include "cl_main.h"
#include "p_effect.h"
#include "p_lnspec.h"
#include "possession.h"
#include "c_console.h"
#include "s_sndseq.h"
#include "version.h"
#include "p_acs.h"
#include "p_enemy.h"
#include "survival.h"
#include "v_palette.h"
#include "v_video.h"
#include "w_wad.h"
#include "deathmatch.h"
#include "duel.h"
#include "team.h"
#include "chat.h"
#include "announcer.h"
#include "network.h"
#include "sv_main.h"
#include "sbar.h"
#include "m_random.h"
#include "templates.h"
#include "stats.h"
#include "lastmanstanding.h"
#include "scoreboard.h"
#include "joinqueue.h"
#include "callvote.h"
#include "invasion.h"
#include "r_sky.h"
#include "r_translate.h"
#include "domination.h"
#include "p_3dmidtex.h"
#include "a_lightning.h"
#include "a_movingcamera.h"
#include "d_netinf.h"
#include "po_man.h"
#include "network/cl_auth.h"

//*****************************************************************************
//	MISC CRAP THAT SHOULDN'T BE HERE BUT HAS TO BE BECAUSE OF SLOPPY CODING

//void	ChangeSpy (bool forward);
int		D_PlayerClassToInt (const char *classname);
bool	P_OldAdjustFloorCeil (AActor *thing);
void	ClientObituary (AActor *self, AActor *inflictor, AActor *attacker, FName MeansOfDeath);
void	P_CrouchMove(player_t * player, int direction);
extern	bool	SpawningMapThing;
extern FILE *Logfile;
bool	ClassOwnsState( const PClass *pClass, const FState *pState );
bool	ActorOwnsState( const AActor *pActor, const FState *pState );
void	D_ErrorCleanup ();
extern bool FriendlyFire;
DECLARE_ACTION(A_RestoreSpecialPosition)

EXTERN_CVAR( Bool, telezoom )
EXTERN_CVAR( Bool, sv_cheats )
EXTERN_CVAR( Int, cl_bloodtype )
EXTERN_CVAR( Int, cl_pufftype )
EXTERN_CVAR( String, playerclass )
EXTERN_CVAR( Int, am_cheat )
EXTERN_CVAR( Bool, cl_oldfreelooklimit )
EXTERN_CVAR( Float, turbo )
EXTERN_CVAR( Float, sv_gravity )
EXTERN_CVAR( Float, sv_aircontrol )

//*****************************************************************************
//	CONSOLE COMMANDS/VARIABLES

#ifdef	_DEBUG
CVAR( Bool, cl_emulatepacketloss, false, 0 )
#endif
// [BB]
CVAR( Bool, cl_connectsound, true, CVAR_ARCHIVE )

//*****************************************************************************
//	PROTOTYPES

// Protocol functions.
static	void	client_Header( BYTESTREAM_s *pByteStream );
static	void	client_Ping( BYTESTREAM_s *pByteStream );
static	void	client_BeginSnapshot( BYTESTREAM_s *pByteStream );
static	void	client_EndSnapshot( BYTESTREAM_s *pByteStream );

// Player functions.
static	void	client_SpawnPlayer( BYTESTREAM_s *pByteStream, bool bMorph = false );
static	void	client_MovePlayer( BYTESTREAM_s *pByteStream );
static	void	client_DamagePlayer( BYTESTREAM_s *pByteStream );
static	void	client_KillPlayer( BYTESTREAM_s *pByteStream );
static	void	client_SetPlayerHealth( BYTESTREAM_s *pByteStream );
static	void	client_SetPlayerArmor( BYTESTREAM_s *pByteStream );
static	void	client_SetPlayerState( BYTESTREAM_s *pByteStream );
static	void	client_SetPlayerUserInfo( BYTESTREAM_s *pByteStream );
static	void	client_SetPlayerFrags( BYTESTREAM_s *pByteStream );
static	void	client_SetPlayerPoints( BYTESTREAM_s *pByteStream );
static	void	client_SetPlayerWins( BYTESTREAM_s *pByteStream );
static	void	client_SetPlayerKillCount( BYTESTREAM_s *pByteStream );
static	void	client_SetPlayerChatStatus( BYTESTREAM_s *pByteStream );
static	void	client_SetPlayerConsoleStatus( BYTESTREAM_s *pByteStream );
static	void	client_SetPlayerLaggingStatus( BYTESTREAM_s *pByteStream );
static	void	client_SetPlayerReadyToGoOnStatus( BYTESTREAM_s *pByteStream );
static	void	client_SetPlayerTeam( BYTESTREAM_s *pByteStream );
static	void	client_SetPlayerCamera( BYTESTREAM_s *pByteStream );
static	void	client_SetPlayerPoisonCount( BYTESTREAM_s *pByteStream );
static	void	client_SetPlayerAmmoCapacity( BYTESTREAM_s *pByteStream );
static	void	client_SetPlayerCheats( BYTESTREAM_s *pByteStream );
static	void	client_SetPlayerPendingWeapon( BYTESTREAM_s *pByteStream );
// [BB] Does not work with the latest ZDoom changes. Check if it's still necessary.
//static	void	client_SetPlayerPieces( BYTESTREAM_s *pByteStream );
static	void	client_SetPlayerPSprite( BYTESTREAM_s *pByteStream );
static	void	client_SetPlayerBlend( BYTESTREAM_s *pByteStream );
static	void	client_SetPlayerMaxHealth( BYTESTREAM_s *pByteStream );
static	void	client_SetPlayerLivesLeft( BYTESTREAM_s *pByteStream );
static	void	client_UpdatePlayerPing( BYTESTREAM_s *pByteStream );
static	void	client_UpdatePlayerExtraData( BYTESTREAM_s *pByteStream );
static	void	client_UpdatePlayerTime( BYTESTREAM_s *pByteStream );
static	void	client_MoveLocalPlayer( BYTESTREAM_s *pByteStream );
static	void	client_DisconnectPlayer( BYTESTREAM_s *pByteStream );
static	void	client_SetConsolePlayer( BYTESTREAM_s *pByteStream );
static	void	client_ConsolePlayerKicked( BYTESTREAM_s *pByteStream );
static	void	client_GivePlayerMedal( BYTESTREAM_s *pByteStream );
static	void	client_ResetAllPlayersFragcount( BYTESTREAM_s *pByteStream );
static	void	client_PlayerIsSpectator( BYTESTREAM_s *pByteStream );
static	void	client_PlayerSay( BYTESTREAM_s *pByteStream );
static	void	client_PlayerTaunt( BYTESTREAM_s *pByteStream );
static	void	client_PlayerRespawnInvulnerability( BYTESTREAM_s *pByteStream );
static	void	client_PlayerUseInventory( BYTESTREAM_s *pByteStream );
static	void	client_PlayerDropInventory( BYTESTREAM_s *pByteStream );
static	void	client_IgnorePlayer( BYTESTREAM_s *pByteStream );

// Thing functions.
static	void	client_SpawnThing( BYTESTREAM_s *pByteStream );
static	void	client_SpawnThingNoNetID( BYTESTREAM_s *pByteStream );
static	void	client_SpawnThingExact( BYTESTREAM_s *pByteStream );
static	void	client_SpawnThingExactNoNetID( BYTESTREAM_s *pByteStream );
static	void	client_MoveThing( BYTESTREAM_s *pByteStream );
static	void	client_MoveThingExact( BYTESTREAM_s *pByteStream );
static	void	client_DamageThing( BYTESTREAM_s *pByteStream );
static	void	client_KillThing( BYTESTREAM_s *pByteStream );
static	void	client_SetThingState( BYTESTREAM_s *pByteStream );
static	void	client_SetThingTarget( BYTESTREAM_s *pByteStream );
static	void	client_DestroyThing( BYTESTREAM_s *pByteStream );
static	void	client_SetThingAngle( BYTESTREAM_s *pByteStream );
static	void	client_SetThingAngleExact( BYTESTREAM_s *pByteStream );
static	void	client_SetThingWaterLevel( BYTESTREAM_s *pByteStream );
static	void	client_SetThingFlags( BYTESTREAM_s *pByteStream );
static	void	client_SetThingArguments( BYTESTREAM_s *pByteStream );
static	void	client_SetThingTranslation( BYTESTREAM_s *pByteStream );
static	void	client_SetThingProperty( BYTESTREAM_s *pByteStream );
static	void	client_SetThingSound( BYTESTREAM_s *pByteStream );
static	void	client_SetThingSpawnPoint( BYTESTREAM_s *pByteStream );
static	void	client_SetThingSpecial1( BYTESTREAM_s *pByteStream );
static	void	client_SetThingSpecial2( BYTESTREAM_s *pByteStream );
static	void	client_SetThingTics( BYTESTREAM_s *pByteStream );
static	void	client_SetThingTID( BYTESTREAM_s *pByteStream );
static	void	client_SetThingGravity( BYTESTREAM_s *pByteStream );
static	void	client_SetThingFrame( BYTESTREAM_s *pByteStream, bool bCallStateFunction );
static	void	client_SetWeaponAmmoGive( BYTESTREAM_s *pByteStream );
static	void	client_ThingIsCorpse( BYTESTREAM_s *pByteStream );
static	void	client_HideThing( BYTESTREAM_s *pByteStream );
static	void	client_TeleportThing( BYTESTREAM_s *pByteStream );
static	void	client_ThingActivate( BYTESTREAM_s *pByteStream );
static	void	client_ThingDeactivate( BYTESTREAM_s *pByteStream );
static	void	client_RespawnDoomThing( BYTESTREAM_s *pByteStream );
static	void	client_RespawnRavenThing( BYTESTREAM_s *pByteStream );
static	void	client_SpawnBlood( BYTESTREAM_s *pByteStream );
static	void	client_SpawnBloodSplatter( BYTESTREAM_s *pByteStream, bool bIsBloodSplatter2 );
static	void	client_SpawnPuff( BYTESTREAM_s *pByteStream );

// Print commands.
static	void	client_Print( BYTESTREAM_s *pByteStream );
static	void	client_PrintMid( BYTESTREAM_s *pByteStream );
static	void	client_PrintMOTD( BYTESTREAM_s *pByteStream );
static	void	client_PrintHUDMessage( BYTESTREAM_s *pByteStream );
static	void	client_PrintHUDMessageFadeOut( BYTESTREAM_s *pByteStream );
static	void	client_PrintHUDMessageFadeInOut( BYTESTREAM_s *pByteStream );
static	void	client_PrintHUDMessageTypeOnFadeOut( BYTESTREAM_s *pByteStream );

// Game commands.
static	void	client_SetGameMode( BYTESTREAM_s *pByteStream );
static	void	client_SetGameSkill( BYTESTREAM_s *pByteStream );
static	void	client_SetGameDMFlags( BYTESTREAM_s *pByteStream );
static	void	client_SetGameModeLimits( BYTESTREAM_s *pByteStream );
static	void	client_SetGameEndLevelDelay( BYTESTREAM_s *pByteStream );
static	void	client_SetGameModeState( BYTESTREAM_s *pByteStream );
static	void	client_SetDuelNumDuels( BYTESTREAM_s *pByteStream );
static	void	client_SetLMSSpectatorSettings( BYTESTREAM_s *pByteStream );
static	void	client_SetLMSAllowedWeapons( BYTESTREAM_s *pByteStream );
static	void	client_SetInvasionNumMonstersLeft( BYTESTREAM_s *pByteStream );
static	void	client_SetInvasionWave( BYTESTREAM_s *pByteStream );
static	void	client_SetSimpleCTFSTMode( BYTESTREAM_s *pByteStream );
static	void	client_DoPossessionArtifactPickedUp( BYTESTREAM_s *pByteStream );
static	void	client_DoPossessionArtifactDropped( BYTESTREAM_s *pByteStream );
static	void	client_DoGameModeFight( BYTESTREAM_s *pByteStream );
static	void	client_DoGameModeCountdown( BYTESTREAM_s *pByteStream );
static	void	client_DoGameModeWinSequence( BYTESTREAM_s *pByteStream );
static	void	client_SetDominationState( BYTESTREAM_s *pByteStream );
static	void	client_SetDominationPointOwnership( BYTESTREAM_s *pByteStream );

// Team commands.
static	void	client_SetTeamFrags( BYTESTREAM_s *pByteStream );
static	void	client_SetTeamScore( BYTESTREAM_s *pByteStream );
static	void	client_SetTeamWins( BYTESTREAM_s *pByteStream );
static	void	client_SetTeamReturnTicks( BYTESTREAM_s *pByteStream );
static	void	client_TeamFlagReturned( BYTESTREAM_s *pByteStream );
static	void	client_TeamFlagDropped( BYTESTREAM_s *pByteStream );

// Missile commands.
static	void	client_SpawnMissile( BYTESTREAM_s *pByteStream );
static	void	client_SpawnMissileExact( BYTESTREAM_s *pByteStream );
static	void	client_MissileExplode( BYTESTREAM_s *pByteStream );

// Weapon commands.
static	void	client_WeaponSound( BYTESTREAM_s *pByteStream );
static	void	client_WeaponChange( BYTESTREAM_s *pByteStream );
static	void	client_WeaponRailgun( BYTESTREAM_s *pByteStream );

// Sector commands.
static	void	client_SetSectorFloorPlane( BYTESTREAM_s *pByteStream );
static	void	client_SetSectorCeilingPlane( BYTESTREAM_s *pByteStream );
static	void	client_SetSectorFloorPlaneSlope( BYTESTREAM_s *pByteStream );
static	void	client_SetSectorCeilingPlaneSlope( BYTESTREAM_s *pByteStream );
static	void	client_SetSectorLightLevel( BYTESTREAM_s *pByteStream );
static	void	client_SetSectorColor( BYTESTREAM_s *pByteStream, bool bIdentifySectorsByTag = false );
static	void	client_SetSectorFade( BYTESTREAM_s *pByteStream, bool bIdentifySectorsByTag = false );
static	void	client_SetSectorFlat( BYTESTREAM_s *pByteStream );
static	void	client_SetSectorPanning( BYTESTREAM_s *pByteStream );
static	void	client_SetSectorRotation( BYTESTREAM_s *pByteStream, bool bIdentifySectorsByTag = false );
static	void	client_SetSectorScale( BYTESTREAM_s *pByteStream );
static	void	client_SetSectorSpecial( BYTESTREAM_s *pByteStream );
static	void	client_SetSectorFriction( BYTESTREAM_s *pByteStream );
static	void	client_SetSectorAngleYOffset( BYTESTREAM_s *pByteStream );
static	void	client_SetSectorGravity( BYTESTREAM_s *pByteStream );
static	void	client_SetSectorReflection( BYTESTREAM_s *pByteStream );
static	void	client_StopSectorLightEffect( BYTESTREAM_s *pByteStream );
static	void	client_DestroyAllSectorMovers( BYTESTREAM_s *pByteStream );
static	void	client_SetSectorLink( BYTESTREAM_s *pByteStream );

// Sector light commands.
static	void	client_DoSectorLightFireFlicker( BYTESTREAM_s *pByteStream );
static	void	client_DoSectorLightFlicker( BYTESTREAM_s *pByteStream );
static	void	client_DoSectorLightLightFlash( BYTESTREAM_s *pByteStream );
static	void	client_DoSectorLightStrobe( BYTESTREAM_s *pByteStream );
static	void	client_DoSectorLightGlow( BYTESTREAM_s *pByteStream );
static	void	client_DoSectorLightGlow2( BYTESTREAM_s *pByteStream );
static	void	client_DoSectorLightPhased( BYTESTREAM_s *pByteStream );

// Line commands.
static	void	client_SetLineAlpha( BYTESTREAM_s *pByteStream );
static	void	client_SetLineTexture( BYTESTREAM_s *pByteStream, bool bIdentifyLinesByID = false );
static	void	client_SetSomeLineFlags( BYTESTREAM_s *pByteStream );

// Side commands.
static	void	client_SetSideFlags( BYTESTREAM_s *pByteStream );

// ACS commands.
static	void	client_ACSScriptExecute( BYTESTREAM_s *pByteStream );

// Sound commands.
static	void	client_Sound( BYTESTREAM_s *pByteStream );
static	void	client_SoundActor( BYTESTREAM_s *pByteStream, bool bRespectActorPlayingSomething = false );
static	void	client_SoundPoint( BYTESTREAM_s *pByteStream );
static	void	client_AnnouncerSound( BYTESTREAM_s *pByteSream );

// Sector sequence commands.
static	void	client_StartSectorSequence( BYTESTREAM_s *pByteStream );
static	void	client_StopSectorSequence( BYTESTREAM_s *pByteStream );

// Vote commands.
static	void	client_CallVote( BYTESTREAM_s *pByteStream );
static	void	client_PlayerVote( BYTESTREAM_s *pByteStream );
static	void	client_VoteEnded( BYTESTREAM_s *pByteStream );

// Map commands.
static	void	client_MapLoad( BYTESTREAM_s *pByteStream );
static	void	client_MapNew( BYTESTREAM_s *pByteStream );
static	void	client_MapExit( BYTESTREAM_s *pByteStream );
static	void	client_MapAuthenticate( BYTESTREAM_s *pByteStream );
static	void	client_SetMapTime( BYTESTREAM_s *pByteStream );
static	void	client_SetMapNumKilledMonsters( BYTESTREAM_s *pByteStream );
static	void	client_SetMapNumFoundItems( BYTESTREAM_s *pByteStream );
static	void	client_SetMapNumFoundSecrets( BYTESTREAM_s *pByteStream );
static	void	client_SetMapNumTotalMonsters( BYTESTREAM_s *pByteStream );
static	void	client_SetMapNumTotalItems( BYTESTREAM_s *pByteStream );
static	void	client_SetMapMusic( BYTESTREAM_s *pByteStream );
static	void	client_SetMapSky( BYTESTREAM_s *pByteStream );

// Inventory commands.
static	void	client_GiveInventory( BYTESTREAM_s *pByteStream );
static	void	client_TakeInventory( BYTESTREAM_s *pByteStream );
static	void	client_GivePowerup( BYTESTREAM_s *pByteStream );
static	void	client_DoInventoryPickup( BYTESTREAM_s *pByteStream );
static	void	client_DestroyAllInventory( BYTESTREAM_s *pByteStream );
static	void	client_SetInventoryIcon( BYTESTREAM_s *pByteStream );

// Door commands.
static	void	client_DoDoor( BYTESTREAM_s *pByteStream );
static	void	client_DestroyDoor( BYTESTREAM_s *pByteStream );
static	void	client_ChangeDoorDirection( BYTESTREAM_s *pByteStream );

// Floor commands.
static	void	client_DoFloor( BYTESTREAM_s *pByteStream );
static	void	client_DestroyFloor( BYTESTREAM_s *pByteStream );
static	void	client_ChangeFloorDirection( BYTESTREAM_s *pByteStream );
static	void	client_ChangeFloorType( BYTESTREAM_s *pByteStream );
static	void	client_ChangeFloorDestDist( BYTESTREAM_s *pByteStream );
static	void	client_StartFloorSound( BYTESTREAM_s *pByteStream );

// Ceiling commands.
static	void	client_DoCeiling( BYTESTREAM_s *pByteStream );
static	void	client_DestroyCeiling( BYTESTREAM_s *pByteStream );
static	void	client_ChangeCeilingDirection( BYTESTREAM_s *pByteStream );
static	void	client_ChangeCeilingSpeed( BYTESTREAM_s *pByteStream );
static	void	client_PlayCeilingSound( BYTESTREAM_s *pByteStream );

// Plat commands.
static	void	client_DoPlat( BYTESTREAM_s *pByteStream );
static	void	client_DestroyPlat( BYTESTREAM_s *pByteStream );
static	void	client_ChangePlatStatus( BYTESTREAM_s *pByteStream );
static	void	client_PlayPlatSound( BYTESTREAM_s *pByteStream );

// Elevator commands.
static	void	client_DoElevator( BYTESTREAM_s *pByteStream );
static	void	client_DestroyElevator( BYTESTREAM_s *pByteStream );
static	void	client_StartElevatorSound( BYTESTREAM_s *pByteStream );

// Pillar commands.
static	void	client_DoPillar( BYTESTREAM_s *pByteStream );
static	void	client_DestroyPillar( BYTESTREAM_s *pByteStream );

// Waggle commands.
static	void	client_DoWaggle( BYTESTREAM_s *pByteStream );
static	void	client_DestroyWaggle( BYTESTREAM_s *pByteStream );
static	void	client_UpdateWaggle( BYTESTREAM_s *pByteStream );

// Rotate poly commands.
static	void	client_DoRotatePoly( BYTESTREAM_s *pByteStream );
static	void	client_DestroyRotatePoly( BYTESTREAM_s *pByteStream );

// Move poly commands.
static	void	client_DoMovePoly( BYTESTREAM_s *pByteStream );
static	void	client_DestroyMovePoly( BYTESTREAM_s *pByteStream );

// Poly door commands.
static	void	client_DoPolyDoor( BYTESTREAM_s *pByteStream );
static	void	client_DestroyPolyDoor( BYTESTREAM_s *pByteStream );
static	void	client_SetPolyDoorSpeedPosition( BYTESTREAM_s *pByteStream );
static  void	client_SetPolyDoorSpeedRotation( BYTESTREAM_s *pByteStream );

// Generic polyobject commands.
static	void	client_PlayPolyobjSound( BYTESTREAM_s *pByteStream );
static	void	client_StopPolyobjSound( BYTESTREAM_s *pByteStream );
static	void	client_SetPolyobjPosition( BYTESTREAM_s *pByteStream );
static	void	client_SetPolyobjRotation( BYTESTREAM_s *pByteStream );

// Miscellaneous commands.
static	void	client_EarthQuake( BYTESTREAM_s *pByteStream );
static	void	client_SetQueuePosition( BYTESTREAM_s *pByteStream );
static	void	client_DoScroller( BYTESTREAM_s *pByteStream );
static	void	client_SetScroller( BYTESTREAM_s *pByteStream );
static	void	client_SetWallScroller( BYTESTREAM_s *pByteStream );
static	void	client_DoFlashFader( BYTESTREAM_s *pByteStream );
static	void	client_GenericCheat( BYTESTREAM_s *pByteStream );
static	void	client_SetCameraToTexture( BYTESTREAM_s *pByteStream );
static	void	client_CreateTranslation( BYTESTREAM_s *pByteStream, bool bIsTypeTwo );
static	void	client_DoPusher( BYTESTREAM_s *pByteStream );
static	void	client_AdjustPusher( BYTESTREAM_s *pByteStream );
static	const char* client_ReadBytestreamSound( BYTESTREAM_s *pByteStream );

class STClient {
public:
	// [BB] Needs to be encapsulated in STClient, because STClient is friend of DLevelScript.
	static void ReplaceTextures( BYTESTREAM_s *pByteStream );
};

//*****************************************************************************
//	VARIABLES

// Local network buffer for the client.
static	NETBUFFER_s			g_LocalBuffer;

// The address of the server we're connected to or trying to connect to.
static	NETADDRESS_s		g_AddressServer;

// The address of the last server we tried to connect to/we're connected to.
static	NETADDRESS_s		g_AddressLastConnected;

// Last time we heard from the server.
static	ULONG				g_ulLastServerTick;

// State of the client's connection to the server.
static	CONNECTIONSTATE_e	g_ConnectionState;

// Have we heard from the server recently?
static	bool				g_bServerLagging;

// What's the time of the last message the server got from us?
static	bool				g_bClientLagging;

// [BB] Time we received the end of the last full update from the server.
static ULONG				g_ulEndFullUpdateTic = 0;

// Used for item respawning client-side.
static	FRandom				g_RestorePositionSeed( "ClientRestorePos" );

// Is it okay to send user info, or is that currently disabled?
static	bool				g_bAllowSendingOfUserInfo = true;

// The name of the map we need to authenticate.
static	char				g_szMapName[9];

// Last console player update tick.
static	ULONG				g_ulLastConsolePlayerUpdateTick;

// This is how many ticks left before we try again to connect to the server,
// request a snapshot, etc.
static	ULONG				g_ulRetryTicks;

// The "message of the day" string. Display it when we're done receiving
// our snapshot.
static	FString				g_MOTD;

// Is the client module parsing a packet?
static	bool				g_bIsParsingPacket;

// This contains the last PACKET_BUFFER_SIZE packets we've received.
static	PACKETBUFFER_s		g_ReceivedPacketBuffer;

// This is the start position of each packet within that buffer.
static	LONG				g_lPacketBeginning[PACKET_BUFFER_SIZE];

// This is the sequences of the last PACKET_BUFFER_SIZE packets we've received.
static	LONG				g_lPacketSequence[PACKET_BUFFER_SIZE];

// This is the  size of the last PACKET_BUFFER_SIZE packets we've received.
static	LONG				g_lPacketSize[PACKET_BUFFER_SIZE];

// This is the index of the incoming packet.
static	BYTE				g_bPacketNum;

// This is the current position in the received packet buffer.
static	LONG				g_lCurrentPosition;

// This is the sequence of the last packet we parsed.
static	LONG				g_lLastParsedSequence;

// This is the highest sequence of any packet we've received.
static	LONG				g_lHighestReceivedSequence;

// Delay for sending a request missing packets.
static	LONG				g_lMissingPacketTicks;

// Debugging variables.
static	LONG				g_lLastCmd;

// [CK] Our sndinfo string lookup table.
static	TArray<FSoundID>	g_SndinfoLookupTable;

//*****************************************************************************
//	FUNCTIONS

//*****************************************************************************
//
void CLIENT_ClearAllPlayers( void )
{
	for ( ULONG ulIdx = 0; ulIdx < MAXPLAYERS; ++ulIdx )
	{
		playeringame[ulIdx] = false;

		// Zero out all the player information.
		PLAYER_ResetPlayerData( &players[ulIdx] );
	}
}

//*****************************************************************************
//
void CLIENT_LimitProtectedCVARs( void )
{
	if ( turbo > 100.f )
		turbo = 100.f;
}

//*****************************************************************************
//
void CLIENT_Construct( void )
{
	const char	*pszPort;
	const char	*pszIPAddress;
	const char	*pszDemoName;
	USHORT		usPort;
	UCVarValue	Val;

	// Start off as being disconnected.
	g_ConnectionState = CTS_DISCONNECTED;
	g_ulRetryTicks = 0;

	// Check if the user wants to use an alternate port for the server.
	pszPort = Args->CheckValue( "-port" );
    if ( pszPort )
    {
       usPort = atoi( pszPort );
       Printf( PRINT_HIGH, "Connecting using alternate port %i.\n", usPort );
    }
	else 
	   usPort = DEFAULT_CLIENT_PORT;

	// Set up a socket and network message buffer.
	NETWORK_Construct( usPort, true );

	NETWORK_InitBuffer( &g_LocalBuffer, MAX_UDP_PACKET * 8, BUFFERTYPE_WRITE );
	NETWORK_ClearBuffer( &g_LocalBuffer );

	// Initialize the stored packets buffer.
	g_ReceivedPacketBuffer.lMaxSize = MAX_UDP_PACKET * PACKET_BUFFER_SIZE;
	memset( g_ReceivedPacketBuffer.abData, 0, MAX_UDP_PACKET * PACKET_BUFFER_SIZE );

	// Connect to a server right off the bat.
    pszIPAddress = Args->CheckValue( "-connect" );
    if ( pszIPAddress )
    {
		// Convert the given IP string into our server address.
		NETWORK_StringToAddress( pszIPAddress, &g_AddressServer );

		// If the user didn't specify a port, use the default one.
		if ( g_AddressServer.usPort == 0 )
			NETWORK_SetAddressPort( g_AddressServer, DEFAULT_SERVER_PORT );

		// If we try to reconnect, use this address.
		g_AddressLastConnected = g_AddressServer;

		// Put us in a "connection attempt" state.
		g_ConnectionState = CTS_ATTEMPTINGCONNECTION;

		// Put the game in client mode.
		NETWORK_SetState( NETSTATE_CLIENT );

		// Make sure cheats are off.
		Val.Bool = false;
		sv_cheats.ForceSet( Val, CVAR_Bool );
		am_cheat = 0;

		// Make sure our visibility is normal.
		R_SetVisibility( 8.0f );

		CLIENT_ClearAllPlayers();

		// If we've elected to record a demo, begin that process now.
		pszDemoName = Args->CheckValue( "-record" );
		if ( pszDemoName )
			CLIENTDEMO_BeginRecording( pszDemoName );
    }
	// User didn't specify an IP to connect to, so don't try to connect to anything.
	else
		g_ConnectionState = CTS_DISCONNECTED;

	g_MOTD = "";
	g_bIsParsingPacket = false;

	// Call CLIENT_Destruct() when Skulltag closes.
	atterm( CLIENT_Destruct );
}

//*****************************************************************************
//
void CLIENT_Destruct( void )
{
	// [BC] Tell the server we're leaving the game.
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
		CLIENT_QuitNetworkGame( NULL );

	// Free our local buffer.
	NETWORK_FreeBuffer( &g_LocalBuffer );
}

//*****************************************************************************
//
void CLIENT_Tick( void )
{
	// Determine what to do based on our connection state.
	switch ( g_ConnectionState )
	{
	// Just chillin' at the full console. Nothing to do.
	case CTS_DISCONNECTED:

		break;
	// At the fullconsole, attempting to connect to a server.
	case CTS_ATTEMPTINGCONNECTION:

		// If we're not connected to a server, and have an IP specified, try to connect.
		if ( g_AddressServer.abIP[0] )
			CLIENT_AttemptConnection( );
		break;
	// A connection has been established with the server; now authenticate the level.
	case CTS_ATTEMPTINGAUTHENTICATION:

		CLIENT_AttemptAuthentication( g_szMapName );
		break;
	// The level has been authenticated. Request level data and send user info.
	case CTS_REQUESTINGSNAPSHOT:

		// Send data request and userinfo.
		CLIENT_RequestSnapshot( );
		break;
	default:

		break;
	}
}

//*****************************************************************************
//
void CLIENT_EndTick( void )
{
	// If there's any data in our packet, send it to the server.
	if ( NETWORK_CalcBufferSize( &g_LocalBuffer ) > 0 )
		CLIENT_SendServerPacket( );
}

//*****************************************************************************
//*****************************************************************************
//
CONNECTIONSTATE_e CLIENT_GetConnectionState( void )
{
	return ( g_ConnectionState );
}

//*****************************************************************************
//
void CLIENT_SetConnectionState( CONNECTIONSTATE_e State )
{
	UCVarValue	Val;

	g_ConnectionState = State;
	switch ( g_ConnectionState )
	{
	case CTS_RECEIVINGSNAPSHOT:

		Val.Bool = false;
		cooperative.ForceSet( Val, CVAR_Bool );
		survival.ForceSet( Val, CVAR_Bool );
		invasion.ForceSet( Val, CVAR_Bool );

		deathmatch.ForceSet( Val, CVAR_Bool );
		teamplay.ForceSet( Val, CVAR_Bool );
		duel.ForceSet( Val, CVAR_Bool );
		terminator.ForceSet( Val, CVAR_Bool );
		lastmanstanding.ForceSet( Val, CVAR_Bool );
		teamlms.ForceSet( Val, CVAR_Bool );
		possession.ForceSet( Val, CVAR_Bool );
		teampossession.ForceSet( Val, CVAR_Bool );
		domination.ForceSet( Val, CVAR_Bool );

		teamgame.ForceSet( Val, CVAR_Bool );
		ctf.ForceSet( Val, CVAR_Bool );
		oneflagctf.ForceSet( Val, CVAR_Bool );
		skulltag.ForceSet( Val, CVAR_Bool );

		sv_cheats.ForceSet( Val, CVAR_Bool );

		Val.Int = false;
		dmflags.ForceSet( Val, CVAR_Int );
		dmflags2.ForceSet( Val, CVAR_Int );
		compatflags.ForceSet( Val, CVAR_Int );

		gameskill.ForceSet( Val, CVAR_Int );
		botskill.ForceSet( Val, CVAR_Int );

		fraglimit.ForceSet( Val, CVAR_Int );
		timelimit.ForceSet( Val, CVAR_Int );
		pointlimit.ForceSet( Val, CVAR_Int );
		duellimit.ForceSet( Val, CVAR_Int );
		winlimit.ForceSet( Val, CVAR_Int );
		wavelimit.ForceSet( Val, CVAR_Int );
		break;
	default:
		break;
	}
}

//*****************************************************************************
//
NETBUFFER_s *CLIENT_GetLocalBuffer( void )
{
	return ( &g_LocalBuffer );
}

//*****************************************************************************
//
void CLIENT_SetLocalBuffer( NETBUFFER_s *pBuffer )
{
	g_LocalBuffer = *pBuffer;
}

//*****************************************************************************
//
ULONG CLIENT_GetLastServerTick( void )
{
	return ( g_ulLastServerTick );
}

//*****************************************************************************
//
void CLIENT_SetLastServerTick( ULONG ulTick )
{
	g_ulLastServerTick = ulTick;
}

//*****************************************************************************
//
ULONG CLIENT_GetLastConsolePlayerUpdateTick( void )
{
	return ( g_ulLastConsolePlayerUpdateTick );
}

//*****************************************************************************
//
void CLIENT_SetLastConsolePlayerUpdateTick( ULONG ulTick )
{
	g_ulLastConsolePlayerUpdateTick = ulTick;
}

//*****************************************************************************
//
bool CLIENT_GetServerLagging( void )
{
	return ( g_bServerLagging );
}

//*****************************************************************************
//
void CLIENT_SetServerLagging( bool bLagging )
{
	g_bServerLagging = bLagging;
}

//*****************************************************************************
//
bool CLIENT_GetClientLagging( void )
{
	return ( g_bClientLagging );
}

//*****************************************************************************
//
void CLIENT_SetClientLagging( bool bLagging )
{
	g_bClientLagging = bLagging;
}

//*****************************************************************************
//
NETADDRESS_s CLIENT_GetServerAddress( void )
{
	return ( g_AddressServer );
}

//*****************************************************************************
//
void CLIENT_SetServerAddress( NETADDRESS_s Address )
{
	g_AddressServer = Address;
}

//*****************************************************************************
//
bool CLIENT_GetAllowSendingOfUserInfo( void )
{
	return ( g_bAllowSendingOfUserInfo );
}

//*****************************************************************************
//
void CLIENT_SetAllowSendingOfUserInfo( bool bAllow )
{
	g_bAllowSendingOfUserInfo = bAllow;
}

//*****************************************************************************
//
void CLIENT_SendServerPacket( void )
{
	// Add the size of the packet to the number of bytes sent.
	CLIENTSTATISTICS_AddToBytesSent( NETWORK_CalcBufferSize( &g_LocalBuffer ));

	// Launch the packet, and clear out the buffer.
	NETWORK_LaunchPacket( &g_LocalBuffer, g_AddressServer );
	NETWORK_ClearBuffer( &g_LocalBuffer );
}

//*****************************************************************************
//*****************************************************************************
//
void CLIENT_AttemptConnection( void )
{
	ULONG	ulIdx;

	if ( g_ulRetryTicks )
	{
		g_ulRetryTicks--;
		return;
	}

	g_ulRetryTicks = CONNECTION_RESEND_TIME;
	Printf( "Connecting to %s\n", NETWORK_AddressToString( g_AddressServer ));

	// Reset a bunch of stuff.
	NETWORK_ClearBuffer( &g_LocalBuffer );
	memset( g_ReceivedPacketBuffer.abData, 0, MAX_UDP_PACKET * 32 );
	for ( ulIdx = 0; ulIdx < PACKET_BUFFER_SIZE; ulIdx++ )
	{
		g_lPacketBeginning[ulIdx] = 0;
		g_lPacketSequence[ulIdx] = -1;
		g_lPacketSize[ulIdx] = 0;
	}

	g_bServerLagging = false;
	g_bClientLagging = false;

	g_bPacketNum = 0;
	g_lCurrentPosition = 0;
	g_lLastParsedSequence = -1;
	g_lHighestReceivedSequence = -1;

	g_lMissingPacketTicks = 0;

	 // Send connection signal to the server.
	NETWORK_WriteByte( &g_LocalBuffer.ByteStream, CLCC_ATTEMPTCONNECTION );
	NETWORK_WriteString( &g_LocalBuffer.ByteStream, DOTVERSIONSTR );
	NETWORK_WriteString( &g_LocalBuffer.ByteStream, cl_password.GetGenericRep( CVAR_String ).String );
	NETWORK_WriteByte( &g_LocalBuffer.ByteStream, cl_connect_flags );
	NETWORK_WriteByte( &g_LocalBuffer.ByteStream, NETGAMEVERSION );
	NETWORK_WriteString( &g_LocalBuffer.ByteStream, g_lumpsAuthenticationChecksum.GetChars() );
}

//*****************************************************************************
//
void CLIENT_AttemptAuthentication( char *pszMapName )
{
	if ( g_ulRetryTicks )
	{
		g_ulRetryTicks--;
		return;
	}

	g_ulRetryTicks = CONNECTION_RESEND_TIME;
	Printf( "Authenticating level...\n" );

	memset( g_lPacketSequence, -1, sizeof(g_lPacketSequence) );
	g_bPacketNum = 0;

	NETWORK_WriteByte( &g_LocalBuffer.ByteStream, CLCC_ATTEMPTAUTHENTICATION );

	// Send a checksum of our verticies, linedefs, sidedefs, and sectors.
	CLIENT_AuthenticateLevel( pszMapName );

	// Make sure all players are gone from the level.
	CLIENT_ClearAllPlayers();
}

//*****************************************************************************
//
void CLIENT_RequestSnapshot( void )
{
	if ( g_ulRetryTicks )
	{
		g_ulRetryTicks--;
		return;
	}

	g_ulRetryTicks = GAMESTATE_RESEND_TIME;
	Printf( "Requesting snapshot...\n" );

	memset( g_lPacketSequence, -1, sizeof (g_lPacketSequence) );
	g_bPacketNum = 0;

	// Send them a message to get data from the server, along with our userinfo.
	NETWORK_WriteByte( &g_LocalBuffer.ByteStream, CLCC_REQUESTSNAPSHOT );
	CLIENTCOMMANDS_UserInfo( USERINFO_ALL );

	// Make sure all players are gone from the level.
	CLIENT_ClearAllPlayers();

	// [BB] Clear the value of the FriendlyFire bool defined in p_interaction.cpp. This is pretty hackish,
	// but prevents the clients from needing to call P_DamageMobj.
	FriendlyFire = false;
}

//*****************************************************************************
//
bool CLIENT_GetNextPacket( void )
{
	ULONG	ulIdx;

	// Find the next packet in the sequence.
	for ( ulIdx = 0; ulIdx < PACKET_BUFFER_SIZE; ulIdx++ )
	{
		// Found it!
		if ( g_lPacketSequence[ulIdx] == ( g_lLastParsedSequence + 1 ))
		{
			memset( NETWORK_GetNetworkMessageBuffer( )->pbData, -1, MAX_UDP_PACKET );
			memcpy( NETWORK_GetNetworkMessageBuffer( )->pbData, g_ReceivedPacketBuffer.abData + g_lPacketBeginning[ulIdx], g_lPacketSize[ulIdx] );
			NETWORK_GetNetworkMessageBuffer( )->ulCurrentSize = g_lPacketSize[ulIdx];
			NETWORK_GetNetworkMessageBuffer( )->ByteStream.pbStream = NETWORK_GetNetworkMessageBuffer( )->pbData;
			NETWORK_GetNetworkMessageBuffer( )->ByteStream.pbStreamEnd = NETWORK_GetNetworkMessageBuffer( )->ByteStream.pbStream + g_lPacketSize[ulIdx];

			return ( true );
		}
	}

	// We didn't find it!
	return ( false );
}

//*****************************************************************************
//
void CLIENT_GetPackets( void )
{
	LONG lSize;
#ifdef	_DEBUG
	static	ULONG	s_ulEmulatingPacketLoss = 0;
#endif
	while (( lSize = NETWORK_GetPackets( )) > 0 )
	{
		UCVarValue		Val;
		BYTESTREAM_s	*pByteStream;

		pByteStream = &NETWORK_GetNetworkMessageBuffer( )->ByteStream;

		// If we're a client and receiving a message from the server...
		if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) &&
			( NETWORK_CompareAddress( NETWORK_GetFromAddress( ), CLIENT_GetServerAddress( ), false )))
		{
			// Statistics.
			CLIENTSTATISTICS_AddToBytesReceived( lSize );

#ifdef	_DEBUG
			// Emulate packet loss for debugging.
			if (( cl_emulatepacketloss ) && gamestate == GS_LEVEL )
			{
				if ( s_ulEmulatingPacketLoss )
				{
					--s_ulEmulatingPacketLoss;
					break;
				}

				// Should activate once every two/three seconds or so.
				if ( M_Random( ) < 4 )
				{
					// This should give a range anywhere from 1 to 128 ticks.
					s_ulEmulatingPacketLoss = ( M_Random( ) / 4 );
				}

				//if (( M_Random( ) < 170 ) && gamestate == GS_LEVEL )
				//	break;
			}
#endif
			// We've gotten a packet! Now figure out what it's saying.
			if ( CLIENT_ReadPacketHeader( pByteStream ))
			{
				CLIENT_ParsePacket( pByteStream, false );
			}
			else
			{
				while ( CLIENT_GetNextPacket( ))
				{
					pByteStream = &NETWORK_GetNetworkMessageBuffer( )->ByteStream;

					// Parse this packet.
					CLIENT_ParsePacket( pByteStream, true );

					// Don't continue parsing if we've exited the network game.
					if ( CLIENT_GetConnectionState( ) == CTS_DISCONNECTED )
						break;
				}
			}

			// We've heard from the server; don't time out!
			CLIENT_SetLastServerTick( gametic );
			CLIENT_SetServerLagging( false );
		}
		else
		{
			const char		*pszPrefix1 = "127.0.0.1";
			const char		*pszPrefix2 = "10.";
			const char		*pszPrefix3 = "172.16.";
			const char		*pszPrefix4 = "192.168.";
			const char		*pszAddressBuf;
			NETADDRESS_s	AddressFrom;
			LONG			lCommand;
			NETADDRESS_s	MasterAddress;
			const char		*pszMasterPort;
			Val = masterhostname.GetGenericRep( CVAR_String );
			// [BB] This conversion potentially does a DNS lookup.
			// There is absolutely no reason to call this at beginning of the while loop above (like done before). 
			NETWORK_StringToAddress( Val.String, &MasterAddress );

			// Allow the user to specify which port the master server is on.
			pszMasterPort = Args->CheckValue( "-masterport" );
			if ( pszMasterPort )
				MasterAddress.usPort = NETWORK_ntohs( atoi( pszMasterPort ));
			else 
				MasterAddress.usPort = NETWORK_ntohs( DEFAULT_MASTER_PORT );


			pszAddressBuf = NETWORK_AddressToString( NETWORK_GetFromAddress( ));

			// Skulltag is receiving a message from something on the LAN.
			if (( strncmp( pszAddressBuf, pszPrefix1, 9 ) == 0 ) || 
				( strncmp( pszAddressBuf, pszPrefix2, 3 ) == 0 ) ||
				( strncmp( pszAddressBuf, pszPrefix3, 7 ) == 0 ) ||
				( strncmp( pszAddressBuf, pszPrefix4, 8 ) == 0 ))
			{
				AddressFrom = MasterAddress;

				// Keep the same port as the from address.
				AddressFrom.usPort = NETWORK_GetFromAddress( ).usPort;
			}
			else
				AddressFrom = NETWORK_GetFromAddress( );

			// If we're receiving info from the master server...
			if ( NETWORK_CompareAddress( AddressFrom, MasterAddress, false ))
			{
				lCommand = NETWORK_ReadLong( pByteStream );
				switch ( lCommand )
				{
				case MSC_BEGINSERVERLISTPART:
					{
						ULONG ulPacketNum = NETWORK_ReadByte( pByteStream );

						// Get the list of servers.
						bool bLastPartReceived = BROWSER_GetServerList( pByteStream );

						// [BB] We received the final part of the server list, now query the servers.
						if ( bLastPartReceived )
						{
							// Now, query all the servers on the list.
							BROWSER_QueryAllServers( );

							// Finally, clear the server list. Server slots will be reactivated when
							// they come in.
							BROWSER_DeactivateAllServers( );
						}
					}
					break;

				case MSC_REQUESTIGNORED:

					Printf( "Refresh request ignored. Please wait 10 seconds before refreshing the list again.\n" );
					break;

				case MSC_IPISBANNED:

					Printf( "You are banned from the master server.\n" );
					break;

				case MSC_WRONGVERSION:

					Printf( "The master server is using a different version of the launcher-master protocol.\n" );
					break;

				default:

					Printf( "Unknown command from master server: %d\n", static_cast<int> (lCommand) );
					break;
				}
			}
			// Perhaps it's a message from a server we're queried.
			else
			{
				lCommand = NETWORK_ReadLong( pByteStream );
				if ( lCommand == SERVER_LAUNCHER_CHALLENGE )
					BROWSER_ParseServerQuery( pByteStream, false );
				else if ( lCommand == SERVER_LAUNCHER_IGNORING )
					Printf( "WARNING! Please wait a full 10 seconds before refreshing the server list.\n" );
				//else
				//	Printf( "Unknown network message from %s.\n", NETWORK_AddressToString( g_AddressFrom ));
			}
		}
	}
}

//*****************************************************************************
//
void CLIENT_CheckForMissingPackets( void )
{
	LONG	lIdx;
	LONG	lIdx2;

	// We already told the server we're missing packets a little bit ago. No need
	// to do it again.
	if ( g_lMissingPacketTicks > 0 )
	{
		g_lMissingPacketTicks--;
		return;
	}

	if ( g_lLastParsedSequence != g_lHighestReceivedSequence )
	{
		// If we've missed more than PACKET_BUFFER_SIZE packets, there's no hope that we can recover from this
		// since the server only backs up PACKET_BUFFER_SIZE of our packets. We have to end the game.
		if (( g_lHighestReceivedSequence - g_lLastParsedSequence ) >= PACKET_BUFFER_SIZE )
		{
			FString quitMessage;
			quitMessage.Format ( "CLIENT_CheckForMissingPackets: Missing more than %d packets. Unable to recover.", PACKET_BUFFER_SIZE );
			CLIENT_QuitNetworkGame( quitMessage.GetChars() );
			return;
		}

		NETWORK_WriteByte( &g_LocalBuffer.ByteStream, CLC_MISSINGPACKET );

		// Now, go through and figure out what packets we're missing. Request these from the server.
		for ( lIdx = g_lLastParsedSequence + 1; lIdx <= g_lHighestReceivedSequence - 1; lIdx++ )
		{
			for ( lIdx2 = 0; lIdx2 < PACKET_BUFFER_SIZE; lIdx2++ )
			{
				// We've found this packet! No need to tell the server we're missing it.
				if ( g_lPacketSequence[lIdx2] == lIdx )
				{
					if ( debugfile )
						fprintf( debugfile, "We have packet %d.\n", static_cast<int> (lIdx) );

					break;
				}
			}

			// If we didn't find the packet, tell the server we're missing it.
			if ( lIdx2 == PACKET_BUFFER_SIZE )
			{
				if ( debugfile )
					fprintf( debugfile, "Missing packet %d.\n", static_cast<int> (lIdx) );

				NETWORK_WriteLong( &g_LocalBuffer.ByteStream, lIdx );
			}
		}

		// When we're done, write -1 to indicate that we're finished.
		NETWORK_WriteLong( &g_LocalBuffer.ByteStream, -1 );
	}

	// Don't send out a request for the missing packets for another 1/4 second.
	g_lMissingPacketTicks = ( TICRATE / 4 );
}

//*****************************************************************************
//
bool CLIENT_ReadPacketHeader( BYTESTREAM_s *pByteStream )
{
	LONG	lIdx;
	LONG	lCommand;
	LONG	lSequence;

	// Read in the command. Since it's the first one in the packet, it should be
	// SVC_HEADER or SVC_UNRELIABLEPACKET.
	lCommand = NETWORK_ReadByte( pByteStream );

	// If this is an unreliable packet, just break out of here and begin parsing it. There's no
	// need to store it.
	if ( lCommand == SVC_UNRELIABLEPACKET )
		return ( true );
	else

	// Read in the sequence. This is the # of the packet the server has sent us.
	lSequence = NETWORK_ReadLong( pByteStream );
	if ( lCommand != SVC_HEADER )
		Printf( "CLIENT_ReadPacketHeader: WARNING! Expected SVC_HEADER or SVC_UNRELIABLEPACKET!\n" );

	// Check to see if we've already received this packet. If so, skip it.
	for ( lIdx = 0; lIdx < PACKET_BUFFER_SIZE; lIdx++ )
	{
		if ( g_lPacketSequence[lIdx] == lSequence )
			return ( false );
	}

	// The end of the buffer has been reached.
	if (( g_lCurrentPosition + ( NETWORK_CalcBufferSize( NETWORK_GetNetworkMessageBuffer( )))) >= g_ReceivedPacketBuffer.lMaxSize )
		g_lCurrentPosition = 0;

	// Save a bunch of information about this incoming packet.
	g_lPacketBeginning[g_bPacketNum] = g_lCurrentPosition;
	g_lPacketSize[g_bPacketNum] = NETWORK_CalcBufferSize( NETWORK_GetNetworkMessageBuffer( ));
	g_lPacketSequence[g_bPacketNum] = lSequence;

	// Save the received packet.
	memcpy( g_ReceivedPacketBuffer.abData + g_lPacketBeginning[g_bPacketNum], NETWORK_GetNetworkMessageBuffer( )->ByteStream.pbStream, NETWORK_CalcBufferSize( NETWORK_GetNetworkMessageBuffer( )));
	g_lCurrentPosition += NETWORK_CalcBufferSize( NETWORK_GetNetworkMessageBuffer( ));

	if ( lSequence > g_lHighestReceivedSequence )
		g_lHighestReceivedSequence = lSequence;

	g_bPacketNum++;

	return ( false );
}

//*****************************************************************************
//
void CLIENT_ParsePacket( BYTESTREAM_s *pByteStream, bool bSequencedPacket )
{
	LONG	lCommand;

	// We're currently parsing a packet.
	g_bIsParsingPacket = true;

	while ( 1 )
	{  
		// [BB] Processing this command will possibly make use write additional client-side demo
		// commands to our demo stream. We have to make sure that all the commands end up in the
		// demo stream exactly in the order they are processed. We should write the command we
		// are parsing to the demo stream right now, but unfortunately we don't know the size
		// of the command. Thus, we memorize where this command started, process the command,
		// check the new position of the processed stream to determine the command size
		// and then insert the commend into the demo stream before any additional client-side
		// demo commands we wrote while processing the command.

		// [BB] Memorize where the current server command started.
		BYTESTREAM_s commandAsStream;
		commandAsStream.pbStream = pByteStream->pbStream;
		commandAsStream.pbStreamEnd = pByteStream->pbStream;

		// [BB] Memorize the current position in our demo stream.
		CLIENTDEMO_MarkCurrentPosition();
		lCommand = NETWORK_ReadByte( pByteStream );

		// End of message.
		if ( lCommand == -1 )
			break;

#ifdef _DEBUG
		// Option to print commands for debugging purposes.
		if ( cl_showcommands )
			CLIENT_PrintCommand( lCommand );
#endif

		// Process this command.
		CLIENT_ProcessCommand( lCommand, pByteStream );

		g_lLastCmd = lCommand;

		// [BB] If we're recording a demo, write the contents of this command at the position
		// our demo was at when we started to process the server command.
		if ( CLIENTDEMO_IsRecording( ) )
		{
			// [BB] Since we just processed the server command, this command ends where we are now.
			commandAsStream.pbStreamEnd = pByteStream->pbStream;
			CLIENTDEMO_InsertPacketAtMarkedPosition( &commandAsStream );
		}

	}

	// All done!
	g_bIsParsingPacket = false;

	if ( debugfile )
	{
		if ( bSequencedPacket )
			fprintf( debugfile, "End parsing packet %d.\n", static_cast<int> (g_lLastParsedSequence + 1) );
		else
			fprintf( debugfile, "End parsing packet.\n" );
	}

	if ( bSequencedPacket )
		g_lLastParsedSequence++;
}

//*****************************************************************************
//
void CLIENT_ProcessCommand( LONG lCommand, BYTESTREAM_s *pByteStream )
{
	char	szString[128];

	switch ( lCommand )
	{
	case SVCC_AUTHENTICATE:

		// Print a status message.
		Printf( "Connected!\n" );

		// Read in the map name we now need to authenticate.
		strncpy( g_szMapName, NETWORK_ReadString( pByteStream ), 8 );
		g_szMapName[8] = 0;

		// [BB] If we don't have the map, something went horribly wrong.
		if ( P_CheckIfMapExists( g_szMapName ) == false )
			I_Error ( "SVCC_AUTHENTICATE: Unknown map: %s\n", g_szMapName );

		// The next step is the authenticate the level.
		if ( CLIENTDEMO_IsPlaying( ) == false )
		{
			g_ulRetryTicks = 0;
			CLIENT_SetConnectionState( CTS_ATTEMPTINGAUTHENTICATION );
			CLIENT_AttemptAuthentication( g_szMapName );
		}
		// [BB] Make sure there is no old player information left. This is possible if
		// the demo shows a "map" map change.
		else
			CLIENT_ClearAllPlayers();
		break;
	case SVCC_MAPLOAD:
		{
			// [BB] We are about to change the map, so if we are playing a demo right now
			// and wanted to skip the current map, we are done with it now.
			CLIENTDEMO_SetSkippingToNextMap ( false );

			// [BB] Setting the game mode is necessary to decide whether 3D floors should be spawned or not.
			GAMEMODE_SetCurrentMode ( static_cast<GAMEMODE_e>(NETWORK_ReadByte( pByteStream )) );

			bool	bPlaying;

			// Print a status message.
			Printf( "Level authenticated!\n" );

			// Check to see if we have the map.
			if ( P_CheckIfMapExists( g_szMapName ))
			{
				// Save our demo recording status since G_InitNew resets it.
				bPlaying = CLIENTDEMO_IsPlaying( );

				// Start new level.
				G_InitNew( g_szMapName, false );

				// For right now, the view is not active.
				viewactive = false;

				g_ulLastConsolePlayerUpdateTick = 0;

				// Restore our demo recording status.
				CLIENTDEMO_SetPlaying( bPlaying );
			}
			// [BB] If we don't have the map, something went horribly wrong.
			else
				I_Error ( "SVCC_MAPLOAD: Unknown map: %s\n", g_szMapName );

			// Now that we've loaded the map, request a snapshot from the server.
			if ( CLIENTDEMO_IsPlaying( ) == false )
			{
				g_ulRetryTicks = 0;
				CLIENT_SetConnectionState( CTS_REQUESTINGSNAPSHOT );
				CLIENT_RequestSnapshot( );
			}
		}
		break;
	case SVCC_ERROR:
		{
			char	szErrorString[256];
			ULONG	ulErrorCode;

			// Read in the error code.
			ulErrorCode = NETWORK_ReadByte( pByteStream );

			// Build the error string based on the error code.
			switch ( ulErrorCode )
			{
			case NETWORK_ERRORCODE_WRONGPASSWORD:

				sprintf( szErrorString, "Incorrect password." );
				break;
			case NETWORK_ERRORCODE_WRONGVERSION:

				sprintf( szErrorString, "Failed connect. Your version is different.\nThis server is using version: %s\nPlease check http://www." DOMAIN_NAME "/ for updates.", NETWORK_ReadString( pByteStream ));
				break;
			case NETWORK_ERRORCODE_WRONGPROTOCOLVERSION:

				sprintf( szErrorString, "Failed connect. Your version uses outdated network code.\nPlease check http://www." DOMAIN_NAME "/ for updates." );
				break;
			case NETWORK_ERRORCODE_BANNED:

				{
					sprintf( szErrorString, "Couldn't connect. \\cgYou have been banned from this server!\\c-" );

					// [RC] Read the reason for this ban.
					const char		*pszBanReason = NETWORK_ReadString( pByteStream );
					char			szShortBanReason[128]; // Don't allow servers to overflow szErrorString.
					if ( strlen( pszBanReason ))
					{
						strncpy( szShortBanReason, pszBanReason, 127 );
						szShortBanReason[127] = 0;
						sprintf( szErrorString, "%s\nReason for ban: %s", szErrorString, szShortBanReason );
					}

					// [RC] Read the expiration date for this ban.
					time_t			tExpiration = (time_t) NETWORK_ReadLong( pByteStream );
					if ( tExpiration > 0 )
					{
						struct tm	*pTimeInfo;
						char		szDate[32];

						pTimeInfo = localtime( &tExpiration );
						strftime( szDate, 32, "%m/%d/%Y %H:%M", pTimeInfo);
						sprintf( szErrorString, "%s\nYour ban expires on: %s (server time)", szErrorString, szDate );
					}
					break;
				}
			case NETWORK_ERRORCODE_SERVERISFULL:

				sprintf( szErrorString, "Server is full." );
				break;
			case NETWORK_ERRORCODE_AUTHENTICATIONFAILED:
			case NETWORK_ERRORCODE_PROTECTED_LUMP_AUTHENTICATIONFAILED:
				{
					std::list<std::pair<FString, FString> > serverPWADs;
					const int numServerPWADs = NETWORK_ReadByte( pByteStream );
					for ( int i = 0; i < numServerPWADs; ++i )
					{
						std::pair<FString, FString> pwad;
						pwad.first = NETWORK_ReadString( pByteStream );
						pwad.second = NETWORK_ReadString( pByteStream );
						serverPWADs.push_back ( pwad );
					}

					sprintf( szErrorString, "%s authentication failed.\nPlease make sure you are using the exact same WAD(s) as the server, and try again.", ( ulErrorCode == NETWORK_ERRORCODE_PROTECTED_LUMP_AUTHENTICATIONFAILED ) ? "Protected lump" : "Level" );

					Printf ( "The server reports %d pwad(s):\n", numServerPWADs );
					for( std::list<std::pair<FString, FString> >::iterator i = serverPWADs.begin( ); i != serverPWADs.end( ); ++i )
						Printf( "PWAD: %s - %s\n", i->first.GetChars(), i->second.GetChars() );
					Printf ( "You have loaded %d pwad(s):\n", static_cast<int>( NETWORK_GetPWADList( )->size() ));
					for( std::list<std::pair<FString, FString> >::iterator i = NETWORK_GetPWADList( )->begin( ); i != NETWORK_GetPWADList( )->end( ); ++i )
						Printf( "PWAD: %s - %s\n", i->first.GetChars(), i->second.GetChars() );

					break;
				}
			case NETWORK_ERRORCODE_FAILEDTOSENDUSERINFO:

				sprintf( szErrorString, "Failed to send userinfo." );
				break;
			case NETWORK_ERRORCODE_TOOMANYCONNECTIONSFROMIP:

				sprintf( szErrorString, "Too many connections from your IP." );
				break;
			case NETWORK_ERRORCODE_USERINFOREJECTED:

				sprintf( szErrorString, "The server rejected the userinfo." );
				break;
			default:

				sprintf( szErrorString, "Unknown error code: %d!\n\nYour version may be different. Please check http://www." DOMAIN_NAME "/ for updates.", static_cast<unsigned int> (ulErrorCode) );
				break;
			}

			CLIENT_QuitNetworkGame( szErrorString );
		}
		return;
	case SVC_HEADER:

		client_Header( pByteStream );
		break;
	case SVC_PING:

		client_Ping( pByteStream );
		break;
	case SVC_NOTHING:

		break;
	case SVC_BEGINSNAPSHOT:

		client_BeginSnapshot( pByteStream );
		break;
	case SVC_ENDSNAPSHOT:

		client_EndSnapshot( pByteStream );
		break;
	case SVC_SPAWNPLAYER:

		client_SpawnPlayer( pByteStream );
		break;
	case SVC_SPAWNMORPHPLAYER:

		client_SpawnPlayer( pByteStream, true );
		break;
	case SVC_MOVEPLAYER:

		client_MovePlayer( pByteStream );
		break;
	case SVC_DAMAGEPLAYER:

		client_DamagePlayer( pByteStream );
		break;
	case SVC_KILLPLAYER:

		client_KillPlayer( pByteStream );
		break;
	case SVC_SETPLAYERHEALTH:

		client_SetPlayerHealth( pByteStream );
		break;
	case SVC_SETPLAYERARMOR:

		client_SetPlayerArmor( pByteStream );
		break;
	case SVC_SETPLAYERSTATE:

		client_SetPlayerState( pByteStream );
		break;
	case SVC_SETPLAYERUSERINFO:

		client_SetPlayerUserInfo( pByteStream );
		break;
	case SVC_SETPLAYERFRAGS:

		client_SetPlayerFrags( pByteStream );
		break;
	case SVC_SETPLAYERPOINTS:

		client_SetPlayerPoints( pByteStream );
		break;
	case SVC_SETPLAYERWINS:

		client_SetPlayerWins( pByteStream );
		break;
	case SVC_SETPLAYERKILLCOUNT:

		client_SetPlayerKillCount( pByteStream );
		break;
	case SVC_SETPLAYERCHATSTATUS:

		client_SetPlayerChatStatus( pByteStream );
		break;
	case SVC_SETPLAYERCONSOLESTATUS:

		client_SetPlayerConsoleStatus( pByteStream );
		break;
	case SVC_SETPLAYERLAGGINGSTATUS:

		client_SetPlayerLaggingStatus( pByteStream );
		break;
	case SVC_SETPLAYERREADYTOGOONSTATUS:

		client_SetPlayerReadyToGoOnStatus( pByteStream );
		break;
	case SVC_SETPLAYERTEAM:

		client_SetPlayerTeam( pByteStream );
		break;
	case SVC_SETPLAYERCAMERA:

		client_SetPlayerCamera( pByteStream );
		break;
	case SVC_SETPLAYERPOISONCOUNT:

		client_SetPlayerPoisonCount( pByteStream );
		break;
	case SVC_SETPLAYERAMMOCAPACITY:

		client_SetPlayerAmmoCapacity( pByteStream );
		break;
	case SVC_SETPLAYERCHEATS:

		client_SetPlayerCheats( pByteStream );
		break;
	case SVC_SETPLAYERPENDINGWEAPON:

		client_SetPlayerPendingWeapon( pByteStream );
		break;
	/* [BB] Does not work with the latest ZDoom changes. Check if it's still necessary.
	case SVC_SETPLAYERPIECES:

		client_SetPlayerPieces( pByteStream );
		break;
	*/
	case SVC_SETPLAYERPSPRITE:

		client_SetPlayerPSprite( pByteStream );
		break;
	case SVC_SETPLAYERBLEND:

		client_SetPlayerBlend( pByteStream );
		break;
	case SVC_SETPLAYERMAXHEALTH:

		client_SetPlayerMaxHealth( pByteStream );
		break;
	case SVC_SETPLAYERLIVESLEFT:

		client_SetPlayerLivesLeft( pByteStream );
		break;
	case SVC_UPDATEPLAYERPING:

		client_UpdatePlayerPing( pByteStream );
		break;
	case SVC_UPDATEPLAYEREXTRADATA:

		client_UpdatePlayerExtraData( pByteStream );
		break;
	case SVC_UPDATEPLAYERTIME:

		client_UpdatePlayerTime( pByteStream );
		break;
	case SVC_MOVELOCALPLAYER:

		client_MoveLocalPlayer( pByteStream );
		break;
	case SVC_DISCONNECTPLAYER:

		client_DisconnectPlayer( pByteStream );
		break;
	case SVC_SETCONSOLEPLAYER:

		client_SetConsolePlayer( pByteStream );
		break;
	case SVC_CONSOLEPLAYERKICKED:

		client_ConsolePlayerKicked( pByteStream );
		break;
	case SVC_GIVEPLAYERMEDAL:

		client_GivePlayerMedal( pByteStream );
		break;
	case SVC_RESETALLPLAYERSFRAGCOUNT:

		client_ResetAllPlayersFragcount( pByteStream );
		break;
	case SVC_PLAYERISSPECTATOR:

		client_PlayerIsSpectator( pByteStream );
		break;
	case SVC_PLAYERSAY:

		client_PlayerSay( pByteStream );
		break;
	case SVC_PLAYERTAUNT:

		client_PlayerTaunt( pByteStream );
		break;
	case SVC_PLAYERRESPAWNINVULNERABILITY:

		client_PlayerRespawnInvulnerability( pByteStream );
		break;
	case SVC_PLAYERUSEINVENTORY:

		client_PlayerUseInventory( pByteStream );
		break;
	case SVC_PLAYERDROPINVENTORY:

		client_PlayerDropInventory( pByteStream );
		break;
	case SVC_SPAWNTHING:

		client_SpawnThing( pByteStream );
		break;
	case SVC_SPAWNTHINGNONETID:

		client_SpawnThingNoNetID( pByteStream );
		break;
	case SVC_SPAWNTHINGEXACT:

		client_SpawnThingExact( pByteStream );
		break;
	case SVC_SPAWNTHINGEXACTNONETID:

		client_SpawnThingExactNoNetID( pByteStream );
		break;
	case SVC_MOVETHING:

		client_MoveThing( pByteStream );
		break;
	case SVC_MOVETHINGEXACT:

		client_MoveThingExact( pByteStream );
		break;
	case SVC_DAMAGETHING:

		client_DamageThing( pByteStream );
		break;
	case SVC_KILLTHING:

		client_KillThing( pByteStream );
		break;
	case SVC_SETTHINGSTATE:

		client_SetThingState( pByteStream );
		break;
	case SVC_SETTHINGTARGET:

		client_SetThingTarget( pByteStream );
		break;
	case SVC_DESTROYTHING:

		client_DestroyThing( pByteStream );
		break;
	case SVC_SETTHINGANGLE:

		client_SetThingAngle( pByteStream );
		break;
	case SVC_SETTHINGANGLEEXACT:

		client_SetThingAngleExact( pByteStream );
		break;
	case SVC_SETTHINGWATERLEVEL:

		client_SetThingWaterLevel( pByteStream );
		break;
	case SVC_SETTHINGFLAGS:

		client_SetThingFlags( pByteStream );
		break;
	case SVC_SETTHINGARGUMENTS:

		client_SetThingArguments( pByteStream );
		break;
	case SVC_SETTHINGTRANSLATION:

		client_SetThingTranslation( pByteStream );
		break;
	case SVC_SETTHINGPROPERTY:

		client_SetThingProperty( pByteStream );
		break;
	case SVC_SETTHINGSOUND:

		client_SetThingSound( pByteStream );
		break;
	case SVC_SETTHINGSPAWNPOINT:

		client_SetThingSpawnPoint( pByteStream );
		break;
	case SVC_SETTHINGSPECIAL1:

		client_SetThingSpecial1( pByteStream );
		break;
	case SVC_SETTHINGSPECIAL2:

		client_SetThingSpecial2( pByteStream );
		break;
	case SVC_SETTHINGTICS:

		client_SetThingTics( pByteStream );
		break;
	case SVC_SETTHINGTID:

		client_SetThingTID( pByteStream );
		break;
	case SVC_SETTHINGGRAVITY:

		client_SetThingGravity( pByteStream );
		break;
	case SVC_SETTHINGFRAME:

		client_SetThingFrame( pByteStream, true );
		break;
	case SVC_SETTHINGFRAMENF:

		client_SetThingFrame( pByteStream, false );
		break;
	case SVC_SETWEAPONAMMOGIVE:

		client_SetWeaponAmmoGive( pByteStream );
		break;
	case SVC_THINGISCORPSE:

		client_ThingIsCorpse( pByteStream );
		break;
	case SVC_HIDETHING:

		client_HideThing( pByteStream );
		break;
	case SVC_TELEPORTTHING:

		client_TeleportThing( pByteStream );
		break;
	case SVC_THINGACTIVATE:

		client_ThingActivate( pByteStream );
		break;
	case SVC_THINGDEACTIVATE:

		client_ThingDeactivate( pByteStream );
		break;
	case SVC_RESPAWNDOOMTHING:

		client_RespawnDoomThing( pByteStream );
		break;
	case SVC_RESPAWNRAVENTHING:

		client_RespawnRavenThing( pByteStream );
		break;
	case SVC_SPAWNBLOOD:

		client_SpawnBlood( pByteStream );
		break;
	case SVC_SPAWNBLOODSPLATTER:

		client_SpawnBloodSplatter( pByteStream, false );
		break;
	case SVC_SPAWNBLOODSPLATTER2:

		client_SpawnBloodSplatter( pByteStream, true );
		break;
	case SVC_SPAWNPUFF:

		client_SpawnPuff( pByteStream );
		break;
	case SVC_PRINT:

		client_Print( pByteStream );
		break;
	case SVC_PRINTMID:

		client_PrintMid( pByteStream );
		break;
	case SVC_PRINTMOTD:

		client_PrintMOTD( pByteStream );
		break;
	case SVC_PRINTHUDMESSAGE:

		client_PrintHUDMessage( pByteStream );
		break;
	case SVC_PRINTHUDMESSAGEFADEOUT:

		client_PrintHUDMessageFadeOut( pByteStream );
		break;
	case SVC_PRINTHUDMESSAGEFADEINOUT:

		client_PrintHUDMessageFadeInOut( pByteStream );
		break;
	case SVC_PRINTHUDMESSAGETYPEONFADEOUT:

		client_PrintHUDMessageTypeOnFadeOut( pByteStream );
		break;
	case SVC_SETGAMEMODE:

		client_SetGameMode( pByteStream );
		break;
	case SVC_SETGAMESKILL:

		client_SetGameSkill( pByteStream );
		break;
	case SVC_SETGAMEDMFLAGS:

		client_SetGameDMFlags( pByteStream );
		break;
	case SVC_SETGAMEMODELIMITS:

		client_SetGameModeLimits( pByteStream );
		break;
	case SVC_SETGAMEENDLEVELDELAY:
		
		client_SetGameEndLevelDelay( pByteStream );
		break;
	case SVC_SETGAMEMODESTATE:

		client_SetGameModeState( pByteStream );
		break;
	case SVC_SETDUELNUMDUELS:

		client_SetDuelNumDuels( pByteStream );
		break;
	case SVC_SETLMSSPECTATORSETTINGS:

		client_SetLMSSpectatorSettings( pByteStream );
		break;
	case SVC_SETLMSALLOWEDWEAPONS:

		client_SetLMSAllowedWeapons( pByteStream );
		break;
	case SVC_SETINVASIONNUMMONSTERSLEFT:

		client_SetInvasionNumMonstersLeft( pByteStream );
		break;
	case SVC_SETINVASIONWAVE:

		client_SetInvasionWave( pByteStream );
		break;
	case SVC_SETSIMPLECTFSTMODE:

		client_SetSimpleCTFSTMode( pByteStream );
		break;
	case SVC_DOPOSSESSIONARTIFACTPICKEDUP:

		client_DoPossessionArtifactPickedUp( pByteStream );
		break;
	case SVC_DOPOSSESSIONARTIFACTDROPPED:

		client_DoPossessionArtifactDropped( pByteStream );
		break;
	case SVC_DOGAMEMODEFIGHT:

		client_DoGameModeFight( pByteStream );
		break;
	case SVC_DOGAMEMODECOUNTDOWN:

		client_DoGameModeCountdown( pByteStream );
		break;
	case SVC_DOGAMEMODEWINSEQUENCE:

		client_DoGameModeWinSequence( pByteStream );
		break;
	case SVC_SETDOMINATIONSTATE:

		client_SetDominationState( pByteStream );
		break;
	case SVC_SETDOMINATIONPOINTOWNER:

		client_SetDominationPointOwnership( pByteStream );
		break;
	case SVC_SETTEAMFRAGS:

		client_SetTeamFrags( pByteStream );
		break;
	case SVC_SETTEAMSCORE:

		client_SetTeamScore( pByteStream );
		break;
	case SVC_SETTEAMWINS:

		client_SetTeamWins( pByteStream );
		break;
	case SVC_SETTEAMRETURNTICKS:

		client_SetTeamReturnTicks( pByteStream );
		break;
	case SVC_TEAMFLAGRETURNED:

		client_TeamFlagReturned( pByteStream );
		break;
	case SVC_TEAMFLAGDROPPED:

		client_TeamFlagDropped( pByteStream );
		break;
	case SVC_SPAWNMISSILE:

		client_SpawnMissile( pByteStream );
		break;
	case SVC_SPAWNMISSILEEXACT:

		client_SpawnMissileExact( pByteStream );
		break;
	case SVC_MISSILEEXPLODE:

		client_MissileExplode( pByteStream );
		break;
	case SVC_WEAPONSOUND:

		client_WeaponSound( pByteStream );
		break;
	case SVC_WEAPONCHANGE:

		client_WeaponChange( pByteStream );
		break;
	case SVC_WEAPONRAILGUN:

		client_WeaponRailgun( pByteStream );
		break;
	case SVC_SETSECTORFLOORPLANE:

		client_SetSectorFloorPlane( pByteStream );
		break;
	case SVC_SETSECTORCEILINGPLANE:

		client_SetSectorCeilingPlane( pByteStream );
		break;
	case SVC_SETSECTORFLOORPLANESLOPE:

		client_SetSectorFloorPlaneSlope( pByteStream );
		break;
	case SVC_SETSECTORCEILINGPLANESLOPE:

		client_SetSectorCeilingPlaneSlope( pByteStream );
		break;
	case SVC_SETSECTORLIGHTLEVEL:

		client_SetSectorLightLevel( pByteStream );
		break;
	case SVC_SETSECTORCOLOR:

		client_SetSectorColor( pByteStream );
		break;
	case SVC_SETSECTORCOLORBYTAG:

		client_SetSectorColor( pByteStream, true );
		break;
	case SVC_SETSECTORFADE:

		client_SetSectorFade( pByteStream );
		break;
	case SVC_SETSECTORFADEBYTAG:

		client_SetSectorFade( pByteStream, true );
		break;
	case SVC_SETSECTORFLAT:

		client_SetSectorFlat( pByteStream );
		break;
	case SVC_SETSECTORPANNING:

		client_SetSectorPanning( pByteStream );
		break;
	case SVC_SETSECTORROTATION:

		client_SetSectorRotation( pByteStream );
		break;
	case SVC_SETSECTORROTATIONBYTAG:

		client_SetSectorRotation( pByteStream, true );
		break;
	case SVC_SETSECTORSCALE:

		client_SetSectorScale( pByteStream );
		break;
	case SVC_SETSECTORSPECIAL:

		client_SetSectorSpecial( pByteStream );
		break;
	case SVC_SETSECTORFRICTION:

		client_SetSectorFriction( pByteStream );
		break;
	case SVC_SETSECTORANGLEYOFFSET:

		client_SetSectorAngleYOffset( pByteStream );
		break;
	case SVC_SETSECTORGRAVITY:

		client_SetSectorGravity( pByteStream );
		break;
	case SVC_SETSECTORREFLECTION:

		client_SetSectorReflection( pByteStream );
		break;
	case SVC_STOPSECTORLIGHTEFFECT:

		client_StopSectorLightEffect( pByteStream );
		break;
	case SVC_DESTROYALLSECTORMOVERS:

		client_DestroyAllSectorMovers( pByteStream );
		break;
	case SVC_DOSECTORLIGHTFIREFLICKER:

		client_DoSectorLightFireFlicker( pByteStream );
		break;
	case SVC_DOSECTORLIGHTFLICKER:

		client_DoSectorLightFlicker( pByteStream );
		break;
	case SVC_DOSECTORLIGHTLIGHTFLASH:

		client_DoSectorLightLightFlash( pByteStream );
		break;
	case SVC_DOSECTORLIGHTSTROBE:

		client_DoSectorLightStrobe( pByteStream );
		break;
	case SVC_DOSECTORLIGHTGLOW:

		client_DoSectorLightGlow( pByteStream );
		break;
	case SVC_DOSECTORLIGHTGLOW2:

		client_DoSectorLightGlow2( pByteStream );
		break;
	case SVC_DOSECTORLIGHTPHASED:

		client_DoSectorLightPhased( pByteStream );
		break;
	case SVC_SETLINEALPHA:

		client_SetLineAlpha( pByteStream );
		break;
	case SVC_SETLINETEXTURE:

		client_SetLineTexture( pByteStream );
		break;
	case SVC_SETLINETEXTUREBYID:

		client_SetLineTexture( pByteStream, true );
		break;
	case SVC_SETSOMELINEFLAGS:

		client_SetSomeLineFlags( pByteStream );
		break;
	case SVC_SETSIDEFLAGS:

		client_SetSideFlags( pByteStream );
		break;
	case SVC_ACSSCRIPTEXECUTE:

		client_ACSScriptExecute( pByteStream );
		break;
	case SVC_SOUND:

		client_Sound( pByteStream );
		break;
	case SVC_SOUNDACTOR:

		client_SoundActor( pByteStream );
		break;
	case SVC_SOUNDACTORIFNOTPLAYING:

		client_SoundActor( pByteStream, true );
		break;
	case SVC_SOUNDPOINT:

		client_SoundPoint( pByteStream );
		break;
	case SVC_STARTSECTORSEQUENCE:

		client_StartSectorSequence( pByteStream );
		break;
	case SVC_STOPSECTORSEQUENCE:

		client_StopSectorSequence( pByteStream );
		break;
	case SVC_CALLVOTE:

		client_CallVote( pByteStream );
		break;
	case SVC_PLAYERVOTE:

		client_PlayerVote( pByteStream );
		break;
	case SVC_VOTEENDED:

		client_VoteEnded( pByteStream );
		break;
	case SVC_MAPLOAD:

		// [BB] We are about to change the map, so if we are playing a demo right now
		// and wanted to skip the current map, we are done with it now.
		CLIENTDEMO_SetSkippingToNextMap ( false );
		client_MapLoad( pByteStream );
		break;
	case SVC_MAPNEW:

		// [BB] We are about to change the map, so if we are playing a demo right now
		// and wanted to skip the current map, we are done with it now.
		if ( CLIENTDEMO_IsSkippingToNextMap() == true )
		{
			// [BB] All the skipping seems to mess up the information which player is in game.
			// Clearing everything will take care of this.
			CLIENT_ClearAllPlayers();
			CLIENTDEMO_SetSkippingToNextMap ( false );
		}
		client_MapNew( pByteStream );
		break;
	case SVC_MAPEXIT:

		// [BB] We are about to change the map, so if we are playing a demo right now
		// and wanted to skip the current map, we are done with it now.
		CLIENTDEMO_SetSkippingToNextMap ( false );
		client_MapExit( pByteStream );
		break;
	case SVC_MAPAUTHENTICATE:

		client_MapAuthenticate( pByteStream );
		break;
	case SVC_SETMAPTIME:

		client_SetMapTime( pByteStream );
		break;
	case SVC_SETMAPNUMKILLEDMONSTERS:

		client_SetMapNumKilledMonsters( pByteStream );
		break;
	case SVC_SETMAPNUMFOUNDITEMS:

		client_SetMapNumFoundItems( pByteStream );
		break;
	case SVC_SETMAPNUMFOUNDSECRETS:

		client_SetMapNumFoundSecrets( pByteStream );
		break;
	case SVC_SETMAPNUMTOTALMONSTERS:

		client_SetMapNumTotalMonsters( pByteStream );
		break;
	case SVC_SETMAPNUMTOTALITEMS:

		client_SetMapNumTotalItems( pByteStream );
		break;
	case SVC_SETMAPMUSIC:

		client_SetMapMusic( pByteStream );
		break;
	case SVC_SETMAPSKY:

		client_SetMapSky( pByteStream );
		break;
	case SVC_GIVEINVENTORY:

		client_GiveInventory( pByteStream );
		break;
	case SVC_TAKEINVENTORY:

		client_TakeInventory( pByteStream );
		break;
	case SVC_GIVEPOWERUP:

		client_GivePowerup( pByteStream );
		break;
	case SVC_DOINVENTORYPICKUP:

		client_DoInventoryPickup( pByteStream );
		break;
	case SVC_DESTROYALLINVENTORY:

		client_DestroyAllInventory( pByteStream );
		break;
	case SVC_DODOOR:

		client_DoDoor( pByteStream );
		break;
	case SVC_DESTROYDOOR:

		client_DestroyDoor( pByteStream );
		break;
	case SVC_CHANGEDOORDIRECTION:

		client_ChangeDoorDirection( pByteStream );
		break;
	case SVC_DOFLOOR:

		client_DoFloor( pByteStream );
		break;
	case SVC_DESTROYFLOOR:

		client_DestroyFloor( pByteStream );
		break;
	case SVC_CHANGEFLOORDIRECTION:

		client_ChangeFloorDirection( pByteStream );
		break;
	case SVC_CHANGEFLOORTYPE:

		client_ChangeFloorType( pByteStream );
		break;
	case SVC_CHANGEFLOORDESTDIST:

		client_ChangeFloorDestDist( pByteStream );
		break;
	case SVC_STARTFLOORSOUND:

		client_StartFloorSound( pByteStream );
		break;
	case SVC_DOCEILING:

		client_DoCeiling( pByteStream );
		break;
	case SVC_DESTROYCEILING:

		client_DestroyCeiling( pByteStream );
		break;
	case SVC_CHANGECEILINGDIRECTION:

		client_ChangeCeilingDirection( pByteStream );
		break;
	case SVC_CHANGECEILINGSPEED:

		client_ChangeCeilingSpeed( pByteStream );
		break;
	case SVC_PLAYCEILINGSOUND:

		client_PlayCeilingSound( pByteStream );
		break;
	case SVC_DOPLAT:

		client_DoPlat( pByteStream );
		break;
	case SVC_DESTROYPLAT:

		client_DestroyPlat( pByteStream );
		break;
	case SVC_CHANGEPLATSTATUS:

		client_ChangePlatStatus( pByteStream );
		break;
	case SVC_PLAYPLATSOUND:

		client_PlayPlatSound( pByteStream );
		break;
	case SVC_DOELEVATOR:

		client_DoElevator( pByteStream );
		break;
	case SVC_DESTROYELEVATOR:

		client_DestroyElevator( pByteStream );
		break;
	case SVC_STARTELEVATORSOUND:

		client_StartElevatorSound( pByteStream );
		break;
	case SVC_DOPILLAR:

		client_DoPillar( pByteStream );
		break;
	case SVC_DESTROYPILLAR:

		client_DestroyPillar( pByteStream );
		break;
	case SVC_DOWAGGLE:

		client_DoWaggle( pByteStream );
		break;
	case SVC_DESTROYWAGGLE:

		client_DestroyWaggle( pByteStream );
		break;
	case SVC_UPDATEWAGGLE:

		client_UpdateWaggle( pByteStream );
		break;
	case SVC_DOROTATEPOLY:

		client_DoRotatePoly( pByteStream );
		break;
	case SVC_DESTROYROTATEPOLY:

		client_DestroyRotatePoly( pByteStream );
		break;
	case SVC_DOMOVEPOLY:

		client_DoMovePoly( pByteStream );
		break;
	case SVC_DESTROYMOVEPOLY:

		client_DestroyMovePoly( pByteStream );
		break;
	case SVC_DOPOLYDOOR:

		client_DoPolyDoor( pByteStream );
		break;
	case SVC_DESTROYPOLYDOOR:

		client_DestroyPolyDoor( pByteStream );
		break;
	case SVC_SETPOLYDOORSPEEDPOSITION:

		client_SetPolyDoorSpeedPosition( pByteStream );
		break;
	case SVC_SETPOLYDOORSPEEDROTATION:

		client_SetPolyDoorSpeedRotation( pByteStream );
		break;
	case SVC_PLAYPOLYOBJSOUND:

		client_PlayPolyobjSound( pByteStream );
		break;
	case SVC_SETPOLYOBJPOSITION:

		client_SetPolyobjPosition( pByteStream );
		break;
	case SVC_SETPOLYOBJROTATION:

		client_SetPolyobjRotation( pByteStream );
		break;
	case SVC_EARTHQUAKE:

		client_EarthQuake( pByteStream );
		break;
	case SVC_SETQUEUEPOSITION:

		client_SetQueuePosition( pByteStream );
		break;
	case SVC_DOSCROLLER:

		client_DoScroller( pByteStream );
		break;
	case SVC_SETSCROLLER:

		client_SetScroller( pByteStream );
		break;
	case SVC_SETWALLSCROLLER:

		client_SetWallScroller( pByteStream );
		break;
	case SVC_DOFLASHFADER:

		client_DoFlashFader( pByteStream );
		break;
	case SVC_GENERICCHEAT:

		client_GenericCheat( pByteStream );
		break;
	case SVC_SETCAMERATOTEXTURE:

		client_SetCameraToTexture( pByteStream );
		break;
	case SVC_CREATETRANSLATION:

		client_CreateTranslation( pByteStream, false );
		break;
	case SVC_CREATETRANSLATION2:

		client_CreateTranslation( pByteStream, true );
		break;
	case SVC_REPLACETEXTURES:

		STClient::ReplaceTextures( pByteStream );
		break;

	case SVC_SETSECTORLINK:

		client_SetSectorLink( pByteStream );
		break;

	case SVC_DOPUSHER:

		client_DoPusher( pByteStream );
		break;

	case SVC_ADJUSTPUSHER:

		client_AdjustPusher( pByteStream );
		break;

	case SVC_IGNOREPLAYER:

		client_IgnorePlayer( pByteStream );
		break;

	case SVC_ANNOUNCERSOUND:

		client_AnnouncerSound( pByteStream );
		break;

	case SVC_EXTENDEDCOMMAND:
		{
			const LONG lExtCommand = NETWORK_ReadByte( pByteStream );

#ifdef _DEBUG
			if ( cl_showcommands )
				Printf( "%s\n", GetStringSVC2 ( static_cast<SVC2> ( lExtCommand ) ) );
#endif

			switch ( lExtCommand )
			{
			case SVC2_SETINVENTORYICON:

				client_SetInventoryIcon( pByteStream );
				break;

			case SVC2_FULLUPDATECOMPLETED:

				// [BB] The server doesn't send any info with this packet, it's just there to allow us
				// keeping track of the current time so that we don't think we are lagging immediately after receiving a full update.
				g_ulEndFullUpdateTic = gametic;
				g_bClientLagging = false;
				// [BB] Tell the server that we received the full update.
				CLIENTCOMMANDS_FullUpdateReceived();

				break;

			case SVC2_SETIGNOREWEAPONSELECT:
				{
					const bool bIgnoreWeaponSelect = !!NETWORK_ReadByte( pByteStream );
					CLIENT_IgnoreWeaponSelect ( bIgnoreWeaponSelect );
				}

				break;

			case SVC2_CLEARCONSOLEPLAYERWEAPON:
				{
					PLAYER_ClearWeapon ( &players[consoleplayer] );
				}

				break;

			case SVC2_LIGHTNING:
				{
					// [Dusk] The client doesn't care about the mode given to P_ForceLightning since
					// it doesn't do the next lightning calculations anyway.
					P_ForceLightning( 0 );
				}

				break;

			case SVC2_CANCELFADE:
				{
					const ULONG ulPlayer = NETWORK_ReadByte( pByteStream );
					AActor *activator = NULL;
					// [BB] ( ulPlayer == MAXPLAYERS ) means that CancelFade was called with NULL as activator.
					if ( PLAYER_IsValidPlayer ( ulPlayer ) )
						activator = players[ulPlayer].mo;

					// [BB] Needed to copy the code below from the implementation of PCD_CANCELFADE.
					TThinkerIterator<DFlashFader> iterator;
					DFlashFader *fader;

					while ( (fader = iterator.Next()) )
					{
						if (activator == NULL || fader->WhoFor() == activator)
						{
							fader->Cancel ();
						}
					}
				}

				break;

			case SVC2_PLAYBOUNCESOUND:
				{
					AActor *pActor = CLIENT_FindThingByNetID( NETWORK_ReadShort( pByteStream ) );
					const bool bOnfloor = !!NETWORK_ReadByte( pByteStream );
					if ( pActor )
						pActor->PlayBounceSound ( bOnfloor );
				}
				break;

			case SVC2_GIVEWEAPONHOLDER:
				{
					const ULONG ulPlayer = NETWORK_ReadByte( pByteStream );
					const int iPieceMask = NETWORK_ReadShort( pByteStream );
					const USHORT usNetIndex = NETWORK_ReadShort( pByteStream );

					if ( PLAYER_IsValidPlayerWithMo( ulPlayer ) == false )
						break;

					const PClass *pPieceWeapon = NETWORK_GetClassFromIdentification( usNetIndex );
					if ( !pPieceWeapon ) break;

					AWeaponHolder *holder = static_cast<AWeaponHolder *>( players[ulPlayer].mo->FindInventory( RUNTIME_CLASS( AWeaponHolder ) ) );
					if ( holder == NULL )
						holder = static_cast<AWeaponHolder *>( players[ulPlayer].mo->GiveInventoryType( RUNTIME_CLASS( AWeaponHolder ) ) );

					if (!holder)
					{
#ifdef CLIENT_WARNING_MESSAGES
						Printf( "GIVEWEAPONHOLDER: Failed to give AWeaponHolder!\n");
#endif
						break;
					}

					// Set the special fields. This is why this function exists in the first place.
					holder->PieceMask = iPieceMask;
					holder->PieceWeapon = pPieceWeapon;
				}
				break;

			// [Dusk]
			case SVC2_SETHEXENARMORSLOTS:
				{
					const ULONG ulPlayer = NETWORK_ReadByte( pByteStream );
					AHexenArmor *aHXArmor = PLAYER_IsValidPlayerWithMo( ulPlayer ) ? static_cast<AHexenArmor *>( players[ulPlayer].mo->FindInventory( RUNTIME_CLASS (AHexenArmor ))) : NULL;

					if ( aHXArmor == NULL )
					{
						// [BB] Even if we can't set the values, we still have to parse them.
						for (int i = 0; i <= 4; i++) NETWORK_ReadLong( pByteStream );
						break;
					}
					
					for (int i = 0; i <= 4; i++)
						aHXArmor->Slots[i] = NETWORK_ReadLong( pByteStream );
				}

				break;

			case SVC2_SETTHINGREACTIONTIME:
				{
					const LONG lID = NETWORK_ReadShort( pByteStream ); 
					const LONG lReactionTime = NETWORK_ReadShort( pByteStream );
					AActor *pActor = CLIENT_FindThingByNetID( lID );

					if ( pActor == NULL )
					{
#ifdef CLIENT_WARNING_MESSAGES
						Printf( "SETTHINGREACTIONTIME: Couldn't find thing: %d\n", lID );
#endif
						break;
					}
					pActor->reactiontime = lReactionTime;
				}
				break;

			// [Dusk]
			case SVC2_SETFASTCHASESTRAFECOUNT:
				{
					const LONG lID = NETWORK_ReadShort( pByteStream );
					const LONG lStrafeCount = NETWORK_ReadByte( pByteStream ); 
					AActor *pActor = CLIENT_FindThingByNetID( lID );

					if ( pActor == NULL )
					{
#ifdef CLIENT_WARNING_MESSAGES
						Printf( "SETFASTCHASESTRAFECOUNT: Couldn't find thing: %d\n", lID );
#endif
						break;
					}
					pActor->FastChaseStrafeCount = lStrafeCount;
				}
				break;

			case SVC2_RESETMAP:
				GAME_ResetMap();
				break;

			case SVC2_SETPOWERUPBLENDCOLOR:
				{
					// Read in the player ID.
					const ULONG ulPlayer = NETWORK_ReadByte( pByteStream );

					// Read in the identification of the type of item to give.
					const USHORT usActorNetworkIndex = NETWORK_ReadShort( pByteStream );

					// Read in the blend color of the powerup.
					const ULONG ulBlendColor = NETWORK_ReadLong( pByteStream );

					// Check to make sure everything is valid. If not, break out.
					if ( PLAYER_IsValidPlayerWithMo( ulPlayer ) == false ) 
						break;

					const PClass *pType = NETWORK_GetClassFromIdentification( usActorNetworkIndex );
					if ( pType == NULL )
						break;

					// If this isn't a powerup, just quit.
					if ( pType->IsDescendantOf( RUNTIME_CLASS( APowerup )) == false )
						break;

					// Try to find this object within the player's personal inventory.
					AInventory *pInventory = players[ulPlayer].mo->FindInventory( pType );

					// [WS] If the player has this powerup, set its blendcolor appropriately.
					if ( pInventory )
						static_cast<APowerup*>(pInventory)->BlendColor = ulBlendColor;
					break;
				}

			// [Dusk]
			case SVC2_SETPLAYERHAZARDCOUNT:
				{
					const ULONG ulPlayer = NETWORK_ReadByte( pByteStream );
					const int hz = NETWORK_ReadShort( pByteStream );

					if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
						break;

					players[ulPlayer].hazardcount = hz;
				}
				break;

			case SVC2_SCROLL3DMIDTEX:
				{
					const int i = NETWORK_ReadByte ( pByteStream );
					const int move = NETWORK_ReadLong ( pByteStream );
					const bool ceiling = !!NETWORK_ReadByte ( pByteStream );

					if ( i < 0 || i >= numsectors || !move )
						break;

					P_Scroll3dMidtex( &sectors[i], 0, move, ceiling );
					break;
				}
			case SVC2_SETPLAYERLOGNUMBER:
				{
					const ULONG ulPlayer = NETWORK_ReadByte( pByteStream );
					const int arg0 = NETWORK_ReadShort( pByteStream );

					if ( PLAYER_IsValidPlayerWithMo( ulPlayer ) == false ) 
						break;

					if ( players[ulPlayer].mo->FindInventory(NAME_Communicator) )
						players[ulPlayer].SetLogNumber ( arg0 );
				}
				break;

			case SVC2_SETTHINGSPECIAL:
				{
					const LONG lID = NETWORK_ReadShort( pByteStream ); 
					const LONG lSpecial = NETWORK_ReadShort( pByteStream );
					AActor *pActor = CLIENT_FindThingByNetID( lID );

					if ( pActor == NULL )
					{
#ifdef CLIENT_WARNING_MESSAGES
						Printf( "SVC2_SETTHINGSPECIAL: Couldn't find thing: %d\n", lID );
#endif
						break;
					}
					pActor->special = lSpecial;
				}
				break;

			case SVC2_SYNCPATHFOLLOWER:
				{
					APathFollower::InitFromStream ( pByteStream );
				}
				break;

			case SVC2_SRP_USER_START_AUTHENTICATION:
			case SVC2_SRP_USER_PROCESS_CHALLENGE:
			case SVC2_SRP_USER_VERIFY_SESSION:
				CLIENT_ProcessSRPServerCommand ( lExtCommand, pByteStream );
				break;

			case SVC2_SETTHINGHEALTH:
				{
					const LONG lID = NETWORK_ReadShort( pByteStream );
					const int health = NETWORK_ReadByte( pByteStream );
					AActor* mo = CLIENT_FindThingByNetID( lID );

					if ( mo == NULL )
					{
#ifdef CLIENT_WARNING_MESSAGES
						Printf( "SVC2_SETTHINGSPECIAL: Couldn't find thing: %d\n", lID );
#endif
						break;
					}

					mo->health = health;
				}
				break;

			case SVC2_SETCVAR:
				{
					const FString cvarName = NETWORK_ReadString( pByteStream );
					const FString cvarValue = NETWORK_ReadString( pByteStream );

					// [TP] Only allow the server to set mod CVARs.
					FBaseCVar* cvar = FindCVar( cvarName, NULL );

					if (( cvar == NULL ) || (( cvar->GetFlags() & CVAR_MOD ) == 0 ))
					{
#ifdef CLIENT_WARNING_MESSAGES
						Printf( "SVC2_SETCVAR: The server attempted to set the value of "
							"non-existant or non-mod CVAR \"%s\" to \"%s\"\n",
							cvarName.GetChars(), cvarValue.GetChars() );
#endif
						break;
					}

					UCVarValue vval;
					vval.String = cvarValue;
					cvar->ForceSet( vval, CVAR_String );
				}
				break;

			// [CK]
			case SVC2_SNDINFOLOOKUPTABLEENTRY:
				{
					const int bytesteamSoundIndex = NETWORK_ReadShort( pByteStream );

					// Sound index 0 isn't used, so we will make use of it by
					// having an index of zero be a marker for the incoming 
					// table size.
					if ( bytesteamSoundIndex == 0 )
					{
						const int tableSize = NETWORK_ReadShort( pByteStream );
						g_SndinfoLookupTable.Clear( );
						g_SndinfoLookupTable.Resize( static_cast<unsigned int>( tableSize ) );
						break;
					}

					const char *pszSoundStr = NETWORK_ReadString( pByteStream );

					// The sound index must be in a valid range.
					if ( bytesteamSoundIndex <= 0 || bytesteamSoundIndex > static_cast<int>( g_SndinfoLookupTable.Size( ) ) )
						I_Error( "Server sent us an invalid sound index: %d\n", bytesteamSoundIndex );

					// We will be sending an index that is one ahead of what it
					// should be. Therefore we subtract one to get it's actual
					// index.
					g_SndinfoLookupTable[bytesteamSoundIndex - 1] = pszSoundStr;
				}
				break;

			// [EP]
			case SVC2_STOPPOLYOBJSOUND:
				client_StopPolyobjSound( pByteStream );
				break;

			default:
				sprintf( szString, "CLIENT_ParsePacket: Illegible server message: %d\nLast command: %d\n", static_cast<int> (lExtCommand), static_cast<int> (g_lLastCmd) );
				CLIENT_QuitNetworkGame( szString );
				return;
			}
		}
		break;

	default:

#ifdef _DEBUG
		sprintf( szString, "CLIENT_ParsePacket: Illegible server message: %d\nLast command: (%s)\n", static_cast<int> (lCommand), GetStringSVC ( static_cast<SVC> ( g_lLastCmd ) ) );
#else
		sprintf( szString, "CLIENT_ParsePacket: Illegible server message: %d\nLast command: %d\n", static_cast<int> (lCommand), static_cast<int> (g_lLastCmd) );
#endif
		CLIENT_QuitNetworkGame( szString );
		return;
	}
}

//*****************************************************************************
//
#ifdef _DEBUG
void CLIENT_PrintCommand( LONG lCommand )
{
	const char	*pszString;

	if ( lCommand < 0 )
		return;

	if ( lCommand < NUM_SERVERCONNECT_COMMANDS )
	{
		switch ( lCommand )
		{
		case SVCC_AUTHENTICATE:

			pszString = "SVCC_AUTHENTICATE";
			break;
		case SVCC_MAPLOAD:

			pszString = "SVCC_MAPLOAD";
			break;
		case SVCC_ERROR:

			pszString = "SVCC_ERROR";
			break;
		}
	}
	else
	{
		if (( cl_showcommands >= 2 ) && ( lCommand == SVC_MOVELOCALPLAYER ))
			return;
		if (( cl_showcommands >= 3 ) && ( lCommand == SVC_MOVEPLAYER ))
			return;
		if (( cl_showcommands >= 4 ) && ( lCommand == SVC_UPDATEPLAYEREXTRADATA ))
			return;
		if (( cl_showcommands >= 5 ) && ( lCommand == SVC_UPDATEPLAYERPING ))
			return;
		if (( cl_showcommands >= 6 ) && ( lCommand == SVC_PING ))
			return;

		if ( ( lCommand - NUM_SERVERCONNECT_COMMANDS ) < 0 )
			return;

		pszString = GetStringSVC ( static_cast<SVC> ( lCommand ) );
	}

	Printf( "%s\n", pszString );

	if ( debugfile )
		fprintf( debugfile, "%s\n", pszString );
}
#endif

//*****************************************************************************
//
void CLIENT_QuitNetworkGame( const char *pszString )
{
	if ( pszString )
		Printf( "%s\n", pszString );

	// Clear out the existing players.
	CLIENT_ClearAllPlayers();

	// If we're connected in any way, send a disconnect signal.
	// [BB] But only if we are actually a client. Otherwise we can't send the signal anywhere.
	if ( ( g_ConnectionState != CTS_DISCONNECTED ) && ( NETWORK_GetState() == NETSTATE_CLIENT ) )
	{
		NETWORK_WriteByte( &g_LocalBuffer.ByteStream, CLC_QUIT );

		// Send the server our packet.
		CLIENT_SendServerPacket( );
	}

	// Clear out our copy of the server address.
	memset( &g_AddressServer, 0, sizeof( g_AddressServer ));
	CLIENT_SetConnectionState( CTS_DISCONNECTED );

	// Go back to the full console.
	gameaction = ga_fullconsole;

	// View is no longer active.
	viewactive = false;

	g_lLastParsedSequence = -1;
	g_lHighestReceivedSequence = -1;

	g_lMissingPacketTicks = 0;

	// Set the network state back to single player.
	NETWORK_SetState( NETSTATE_SINGLE );

	// [BB] This prevents the status bar from showing up shortly, if you start a new game, while connected to a server.
	gamestate = GS_FULLCONSOLE;

	// [BB] Reset gravity to its default, discarding the setting the server used.
	// Although this is not done for any other sv_* CVAR, it is necessary here,
	// since the server sent us level.gravity instead of its own sv_gravity value,
	// see SERVERCOMMANDS_SetGameModeLimits.
	sv_gravity = sv_gravity.GetGenericRepDefault( CVAR_Float ).Float;

	// If we're recording a demo, then finish it!
	if ( CLIENTDEMO_IsRecording( ))
		CLIENTDEMO_FinishRecording( );

	// [BB] Also, if we're playing a demo, finish it
	if ( CLIENTDEMO_IsPlaying( ))
		CLIENTDEMO_FinishPlaying( );
}

//*****************************************************************************
//
void CLIENT_SendCmd( void )
{		
	if (( gametic < 1 ) ||
		( players[consoleplayer].mo == NULL ) ||
		(( gamestate != GS_LEVEL ) && ( gamestate != GS_INTERMISSION )))
	{
		return;
	}

	// If we're at intermission, and toggling our "ready to go" status, tell the server.
	if ( gamestate == GS_INTERMISSION )
	{
		if (( players[consoleplayer].cmd.ucmd.buttons ^ players[consoleplayer].oldbuttons ) &&
			(( players[consoleplayer].cmd.ucmd.buttons & players[consoleplayer].oldbuttons ) == players[consoleplayer].oldbuttons ))
		{
			CLIENTCOMMANDS_ReadyToGoOn( );
		}

		players[consoleplayer].oldbuttons = players[consoleplayer].cmd.ucmd.buttons;
		return;
	}

	// Don't send movement information if we're spectating!
	if ( players[consoleplayer].bSpectating )
	{
		if (( gametic % ( TICRATE * 2 )) == 0 )
		{
			// Send the gametic.
			CLIENTCOMMANDS_SpectateInfo( );
		}

		return;
	}

	// Send the move header and the gametic.
	CLIENTCOMMANDS_ClientMove( );
}

//*****************************************************************************
//
void CLIENT_WaitForServer( void )
{
	if ( players[consoleplayer].bSpectating )
	{
		// If the last time we heard from the server exceeds five seconds, we're lagging!
		if ((( gametic - CLIENTDEMO_GetGameticOffset( ) - g_ulLastServerTick ) >= ( TICRATE * 5 )) &&
			(( gametic - CLIENTDEMO_GetGameticOffset( )) > ( TICRATE * 5 )))
		{
			g_bServerLagging = true;
		}
	}
	else
	{
		// If the last time we heard from the server exceeds one second, we're lagging!
		if ((( gametic - CLIENTDEMO_GetGameticOffset( ) - g_ulLastServerTick ) >= TICRATE ) &&
			(( gametic - CLIENTDEMO_GetGameticOffset( )) > TICRATE ))
		{
			g_bServerLagging = true;
		}
	}
}

//*****************************************************************************
//
void CLIENT_AuthenticateLevel( const char *pszMapName )
{
	FString		Checksum;
	MapData		*pMap;

	// [BB] Check if the wads contain the map at all. If not, don't send any checksums.
	pMap = P_OpenMapData( pszMapName );

	if ( pMap == NULL )
	{
		Printf( "CLIENT_AuthenticateLevel: Map %s not found!\n", pszMapName );
		return;
	}

	// [Dusk] Include a byte to check if this is an UDMF or a non-UDMF map.
	NETWORK_WriteByte( &g_LocalBuffer.ByteStream, pMap->isText );

	if ( pMap->isText )
	{
		// [Dusk] If this is an UDMF map, send the TEXTMAP checksum.
		NETWORK_GenerateMapLumpMD5Hash( pMap, ML_TEXTMAP, Checksum );
		NETWORK_WriteString( &g_LocalBuffer.ByteStream, Checksum.GetChars() );
	}
	else
	{
		// Generate and send checksums for the map lumps.
		const int ids[4] = { ML_VERTEXES, ML_LINEDEFS, ML_SIDEDEFS, ML_SECTORS };
		for( ULONG i = 0; i < 4; ++i )
		{
			NETWORK_GenerateMapLumpMD5Hash( pMap, ids[i], Checksum );
			NETWORK_WriteString( &g_LocalBuffer.ByteStream, Checksum.GetChars() );
		}
	}

	if ( pMap->HasBehavior )
		NETWORK_GenerateMapLumpMD5Hash( pMap, ML_BEHAVIOR, Checksum );
	else
		Checksum = "";
	NETWORK_WriteString( &g_LocalBuffer.ByteStream, Checksum.GetChars() );

	// Finally, free the map.
	delete ( pMap );
}

//*****************************************************************************
//
AActor *CLIENT_SpawnThing( const PClass *pType, fixed_t X, fixed_t Y, fixed_t Z, LONG lNetID )
{
	AActor			*pActor;

	// Only spawn actors if we're actually in a level.
	if ( gamestate != GS_LEVEL )
		return ( NULL );

	if ( pType == NULL )
		return NULL;

	// Potentially print the name, position, and network ID of the thing spawning.
	if ( cl_showspawnnames )
		Printf( "Name: %s: (%d, %d, %d), %d\n", pType->TypeName.GetChars( ), X >> FRACBITS, Y >> FRACBITS, Z >> FRACBITS, static_cast<int> (lNetID) );

	// If there's already an actor with the network ID of the thing we're spawning, kill it!
	pActor = CLIENT_FindThingByNetID( lNetID );
	if ( pActor )
	{
#ifdef	_DEBUG
		if ( pActor == players[consoleplayer].mo )
		{
			Printf( "CLIENT_SpawnThing: WARNING! Tried to delete console player's body! lNetID = %ld\n", lNetID );
			return NULL;
		}
#endif
		pActor->Destroy( );
	}

	// Handle sprite/particle display options.
	if ( stricmp( pType->TypeName.GetChars( ), "blood" ) == 0 )
	{
		if ( cl_bloodtype >= 1 )
		{
			angle_t	Angle;

			Angle = ( M_Random( ) - 128 ) << 24;
			P_DrawSplash2( 32, X, Y, Z, Angle, 2, 0 );
		}

		// Just do particles.
		if ( cl_bloodtype == 2 )
			return ( NULL );
	}

	if ( stricmp( pType->TypeName.GetChars( ), "BulletPuff" ) == 0 )
	{
		if ( cl_pufftype )
		{
			angle_t	Angle;

			Angle = ( M_Random( ) - 128 ) << 24;
			P_DrawSplash2( 32, X, Y, Z, Angle, 1, 1 );
			return ( NULL );
		}
	}

	// Now that all checks have been done, spawn the actor.
	pActor = Spawn( pType, X, Y, Z, NO_REPLACE );
	if ( pActor )
	{
		pActor->lNetID = lNetID;
		if ( lNetID != -1 )
		{
			g_NetIDList[lNetID].bFree = false;
			g_NetIDList[lNetID].pActor = pActor;
		}

		pActor->SpawnPoint[0] = X;
		pActor->SpawnPoint[1] = Y;
		pActor->SpawnPoint[2] = Z;

		// [BB] The current position of the actor comes straight from the server, so it's safe
		// to assume that it's correct and thus a valid value for the last updated position.
		pActor->lastX = X;
		pActor->lastY = Y;
		pActor->lastZ = Z;

		// Whenever blood spawns, its velz is always 2 * FRACUNIT.
		if ( stricmp( pType->TypeName.GetChars( ), "blood" ) == 0 )
			pActor->velz = FRACUNIT*2;

		// Allow for client-side body removal in invasion mode.
		if ( invasion )
			pActor->ulInvasionWave = INVASION_GetCurrentWave( );
	}

	return ( pActor );
}

//*****************************************************************************
//
void CLIENT_SpawnMissile( const PClass *pType, fixed_t X, fixed_t Y, fixed_t Z, fixed_t MomX, fixed_t MomY, fixed_t MomZ, LONG lNetID, LONG lTargetNetID )
{
	AActor				*pActor;

	// Only spawn missiles if we're actually in a level.
	if ( gamestate != GS_LEVEL )
		return;

	if ( pType == NULL )
		return;

	// Potentially print the name, position, and network ID of the thing spawning.
	if ( cl_showspawnnames )
		Printf( "Name: %s: (%d, %d, %d), %d\n", pType->TypeName.GetChars( ), X >> FRACBITS, Y >> FRACBITS, Z >> FRACBITS, static_cast<int> (lNetID) );

	// If there's already an actor with the network ID of the thing we're spawning, kill it!
	pActor = CLIENT_FindThingByNetID( lNetID );
	if ( pActor )
	{
/*
		if ( pActor == players[consoleplayer].mo )
		{
#ifdef CLIENT_WARNING_MESSAGES
			Printf( "CLIENT_SpawnMissile: WARNING! Tried to delete console player's body!\n" );
#endif
			return;
		}
*/
		pActor->Destroy( );
	}

	// Now that all checks have been done, spawn the actor.
	pActor = Spawn( pType, X, Y, Z, NO_REPLACE );
	if ( pActor == NULL )
	{
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "CLIENT_SpawnMissile: Failed to spawn missile: %d\n", lNetID );
#endif
		return;
	}

	// Set the thing's momentum.
	pActor->velx = MomX;
	pActor->vely = MomY;
	pActor->velz = MomZ;

	// Derive the thing's angle from its momentum.
	pActor->angle = R_PointToAngle2( 0, 0, MomX, MomY );

	pActor->lNetID = lNetID;
	if ( lNetID != -1 )
	{
		g_NetIDList[lNetID].bFree = false;
		g_NetIDList[lNetID].pActor = pActor;
	}

	// Play the seesound if this missile has one.
	if ( pActor->SeeSound )
		S_Sound( pActor, CHAN_VOICE, pActor->SeeSound, 1, ATTN_NORM );

	pActor->target = CLIENT_FindThingByNetID( lTargetNetID );
}

//*****************************************************************************
//
void CLIENT_MoveThing( AActor *pActor, fixed_t X, fixed_t Y, fixed_t Z )
{
	if (( pActor == NULL ) || ( gamestate != GS_LEVEL ))
		return;

	pActor->SetOrigin( X, Y, Z );

	// [BB] SetOrigin doesn't set the actor's floorz value properly, so we need to correct this.
	if ( ( pActor->flags & MF_NOBLOCKMAP ) == false )
	{
		// [BB] Unfortunately, P_OldAdjustFloorCeil messes up the floorz value under some circumstances.
		// Save the old value, so that we can restore it if necessary.
		fixed_t oldfloorz = pActor->floorz;
		P_OldAdjustFloorCeil( pActor );
		// [BB] An actor can't be below its floorz, if the value is correct.
		// In this case, P_OldAdjustFloorCeil apparently didn't work, so revert to the old value.
		// [BB] But don't do this for the console player, it messes up the prediction.
		if ( ( NETWORK_IsConsolePlayer ( pActor ) == false ) && ( pActor->floorz > pActor->z ) )
			pActor->floorz = oldfloorz;
	}
}

//*****************************************************************************
//
void CLIENT_AdjustPredictionToServerSideConsolePlayerMove( fixed_t X, fixed_t Y, fixed_t Z )
{
	players[consoleplayer].ServerXYZ[0] = X;
	players[consoleplayer].ServerXYZ[1] = Y;
	players[consoleplayer].ServerXYZ[2] = Z;
	CLIENT_PREDICT_PlayerTeleported( );
}

//*****************************************************************************
//
// [WS] These are specials checks for client-side actors of whether or not they are
// allowed to move through other actors/walls/ceilings/floors.
bool CLIENT_CanClipMovement( AActor *pActor )
{
	// [WS] If it's not a client, of course clip its movement.
	if ( !NETWORK_InClientMode( ) )
		return true;

	// [Dusk] Clients clip missiles the server has no control over.
	if ( NETWORK_IsActorClientHandled ( pActor ) )
		return true;

	// [BB] The client needs to clip its own movement for the prediction.
	if ( pActor->player == &players[consoleplayer] )
		return true;

	// [WS] Non-bouncing client missiles do not get their movement clipped.
	if ( pActor->flags & MF_MISSILE && !pActor->BounceFlags )
		return false;

	return true;
}

//*****************************************************************************
//
// :(. This is needed so that the MOTD can be printed in the color the user wishes to print
// mid-screen messages in.
extern	int PrintColors[7];
void CLIENT_DisplayMOTD( void )
{
	FString	ConsoleString;

	if ( g_MOTD.Len( ) <= 0 )
		return;

	// Add pretty colors/formatting!
	V_ColorizeString( g_MOTD );

	ConsoleString.AppendFormat( TEXTCOLOR_RED
		"\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36"
		"\36\36\36\36\36\36\36\36\36\36\36\36\37" TEXTCOLOR_TAN
		"\n\n%s\n" TEXTCOLOR_RED
		"\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36"
		"\36\36\36\36\36\36\36\36\36\36\36\36\37" TEXTCOLOR_NORMAL "\n\n" ,
		g_MOTD.GetChars() );

	// Add this message to the console window.
	AddToConsole( -1, ConsoleString );

	// We cannot create the message if there's no status bar to attach it to.
	if ( StatusBar == NULL )
		return;

	StatusBar->AttachMessage( new DHUDMessageFadeOut( SmallFont, g_MOTD,
		1.5f,
		0.375f,
		0,
		0,
		(EColorRange)PrintColors[5],
		cl_motdtime,
		0.35f ), MAKE_ID('M','O','T','D') );
}

//*****************************************************************************
//
AActor *CLIENT_FindThingByNetID( LONG lNetID )
{
	if (( lNetID < 0 ) || ( lNetID >= 65536 ))
		return ( NULL );

	if (( g_NetIDList[lNetID].bFree == false ) && ( g_NetIDList[lNetID].pActor ))
		return ( g_NetIDList[lNetID].pActor );

    return ( NULL );
}

//*****************************************************************************
//
void CLIENT_RestoreSpecialPosition( AActor *pActor )
{
	CALL_ACTION(A_RestoreSpecialPosition, pActor);
}

//*****************************************************************************
//
void CLIENT_RestoreSpecialDoomThing( AActor *pActor, bool bFog )
{
	// Change an actor that's been put into a hidden for respawning back to its spawn state
	// (respawning it). The reason we need to duplicate this function (this same code basically
	// exists in A_RestoreSpecialDoomThing( )) is because normally, when items are touched and
	// we want to respawn them, we put them into a hidden state, with the first frame being 1050
	// ticks long. Once that frame is finished, it calls A_ResotoreSpecialDoomThing and Position.
	// We don't want that to happen on the client end, because we don't want items to respawn on
	// the client end. However, there really is no good way to do that. If we don't call the
	// function while in client mode, items won't respawn at all. We could do it with external
	// variables, but that's just not clean. We could just not tick actors, but we definitely 
	// don't want that. Anyway, the solution we're going to use here is to break out of restore
	// special doomthing( ) if we're in client mode to avoid client-side respawning, and just
	// call a different function all together when the server wants us to respawn an item that
	// does basically the same thing.

	// Make the item visible and touchable again.
	pActor->renderflags &= ~RF_INVISIBLE;
	pActor->flags |= MF_SPECIAL;

	if (( pActor->GetDefault( )->flags & MF_NOGRAVITY ) == false )
		pActor->flags &= ~MF_NOGRAVITY;

	// Should this item even respawn?
	// [BB] You can call DoRespawn only on AInventory and descendants.
	if ( !(pActor->IsKindOf( RUNTIME_CLASS( AInventory ))) )
		return;
	if ( static_cast<AInventory *>( pActor )->DoRespawn( ))
	{
		// Put the actor back to its spawn state.
		pActor->SetState( pActor->SpawnState );

		// Play the spawn sound, and make a little fog.
		if ( bFog )
		{
			S_Sound( pActor, CHAN_VOICE, "misc/spawn", 1, ATTN_IDLE );
			Spawn( "ItemFog", pActor->x, pActor->y, pActor->z, ALLOW_REPLACE );
		}
	}
}

//*****************************************************************************
//
AInventory *CLIENT_FindPlayerInventory( ULONG ulPlayer, const PClass *pType )
{
	AInventory		*pInventory;

	// Try to find this object within the player's personal inventory.
	pInventory = players[ulPlayer].mo->FindInventory( pType );

	// If the player doesn't have this type, give it to him.
	if ( pInventory == NULL )
		pInventory = players[ulPlayer].mo->GiveInventoryType( pType );

	// If he still doesn't have the object after trying to give it to him... then YIKES!
	if ( pInventory == NULL )
	{
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "CLIENT_FindPlayerInventory: Failed to give inventory type, %s!\n", pType->TypeName.GetChars( ));
#endif
	}

	return ( pInventory );
}

//*****************************************************************************
//
AInventory *CLIENT_FindPlayerInventory( ULONG ulPlayer, const char *pszName )
{
	const PClass	*pType;

	pType = PClass::FindClass( pszName );
	if ( pType == NULL )
		return ( NULL );

	return ( CLIENT_FindPlayerInventory( ulPlayer, pType ));
}

//*****************************************************************************
//
/* [BB] Causes major problems with Archviles at the moment, therefore deactivated.
void CLIENT_RemoveMonsterCorpses( void )
{
	AActor	*pActor;
	ULONG	ulMonsterCorpseCount;

	// Allow infinite corpses.
	if ( cl_maxmonstercorpses == 0 )
		return;

	// Initialize the number of corpses.
	ulMonsterCorpseCount = 0;

	TThinkerIterator<AActor> iterator;
	while (( pActor = iterator.Next( )))
	{
		if (( pActor->IsKindOf( RUNTIME_CLASS( APlayerPawn )) == true ) ||
			!( pActor->flags & MF_CORPSE ))
		{
			continue;
		}

		ulMonsterCorpseCount++;
		if ( ulMonsterCorpseCount >= cl_maxmonstercorpses )
		{
			pActor->Destroy( );
		}
	}
}
*/

//*****************************************************************************
//
sector_t *CLIENT_FindSectorByID( ULONG ulID )
{
	if ( ulID >= static_cast<ULONG>(numsectors) )
		return ( NULL );

	return ( &sectors[ulID] );
}

//*****************************************************************************
//
bool CLIENT_IsParsingPacket( void )
{
	return ( g_bIsParsingPacket );
}

//*****************************************************************************
//
void CLIENT_ResetConsolePlayerCamera( void )
{
	players[consoleplayer].camera = players[consoleplayer].mo;
	if ( players[consoleplayer].camera != NULL )
		S_UpdateSounds( players[consoleplayer].camera );
	if ( StatusBar != NULL )
		StatusBar->AttachToPlayer( &players[consoleplayer] );
}

//*****************************************************************************
//
void PLAYER_ResetPlayerData( player_t *pPlayer )
{
	pPlayer->mo = NULL;
	pPlayer->playerstate = 0;
	pPlayer->cls = 0;
	pPlayer->DesiredFOV = 0;
	pPlayer->FOV = 0;
	pPlayer->viewz = 0;
	pPlayer->viewheight = 0;
	pPlayer->deltaviewheight = 0;
	pPlayer->bob = 0;
	pPlayer->velx = 0;
	pPlayer->vely = 0;
	pPlayer->centering = 0;
	pPlayer->turnticks = 0;
	pPlayer->oldbuttons = 0;
	pPlayer->attackdown = 0;
	pPlayer->health = 0;
	pPlayer->inventorytics = 0;
	pPlayer->CurrentPlayerClass = 0;
	pPlayer->backpack = 0;
	pPlayer->fragcount = 0;
	pPlayer->ReadyWeapon = 0;
	pPlayer->PendingWeapon = 0;
	pPlayer->cheats = 0;
	pPlayer->refire = 0;
	pPlayer->killcount = 0;
	pPlayer->itemcount = 0;
	pPlayer->secretcount = 0;
	pPlayer->damagecount = 0;
	pPlayer->bonuscount = 0;
	pPlayer->hazardcount = 0;
	pPlayer->poisoncount = 0;
	pPlayer->poisoner = 0;
	pPlayer->attacker = 0;
	pPlayer->extralight = 0;
	pPlayer->morphTics = 0;
	pPlayer->PremorphWeapon = 0;
	pPlayer->chickenPeck = 0;
	pPlayer->jumpTics = 0;
	pPlayer->respawn_time = 0;
	pPlayer->camera = 0;
	pPlayer->air_finished = 0;
	pPlayer->accuracy = 0;
	pPlayer->stamina = 0;
	pPlayer->BlendR = 0;
	pPlayer->BlendG = 0;
	pPlayer->BlendB = 0;
	pPlayer->BlendA = 0;
// 		pPlayer->LogText(),
	pPlayer->crouching = 0;
	pPlayer->crouchdir = 0;
	pPlayer->crouchfactor = 0;
	pPlayer->crouchoffset = 0;
	pPlayer->crouchviewdelta = 0;
	pPlayer->bOnTeam = 0;
	pPlayer->ulTeam = 0;
	pPlayer->lPointCount = 0;
	pPlayer->ulDeathCount = 0;
	PLAYER_ResetSpecialCounters ( pPlayer );
	pPlayer->bChatting = 0;
	pPlayer->bInConsole = 0;
	pPlayer->bSpectating = 0;
	pPlayer->bIgnoreChat = 0;
	pPlayer->lIgnoreChatTicks = -1;
	pPlayer->bDeadSpectator = 0;
	pPlayer->ulLivesLeft = 0;
	pPlayer->bStruckPlayer = 0;
	pPlayer->pIcon = 0;
	pPlayer->lMaxHealthBonus = 0;
	pPlayer->ulWins = 0;
	pPlayer->pSkullBot = 0;
	pPlayer->bIsBot = 0;
	pPlayer->ulPing = 0;
	pPlayer->ulPingAverages = 0;
	pPlayer->bReadyToGoOn = 0;
	pPlayer->bSpawnOkay = 0;
	pPlayer->SpawnX = 0;
	pPlayer->SpawnY = 0;
	pPlayer->SpawnAngle = 0;
	pPlayer->OldPendingWeapon = 0;
	pPlayer->bLagging = 0;
	pPlayer->bSpawnTelefragged = 0;
	pPlayer->ulTime = 0;

	memset( &pPlayer->cmd, 0, sizeof( pPlayer->cmd ));
	if (( pPlayer - players ) != consoleplayer )
	{
		memset( &pPlayer->userinfo, 0, sizeof( pPlayer->userinfo ));
		// [BB] For now Zandronum doesn't let the player use the color sets.
		pPlayer->userinfo.colorset = -1;
	}
	memset( pPlayer->psprites, 0, sizeof( pPlayer->psprites ));

	memset( &pPlayer->ulMedalCount, 0, sizeof( ULONG ) * NUM_MEDALS );
	memset( &pPlayer->ServerXYZ, 0, sizeof( fixed_t ) * 3 );
	memset( &pPlayer->ServerXYZMom, 0, sizeof( fixed_t ) * 3 );
}

//*****************************************************************************
//
LONG CLIENT_AdjustDoorDirection( LONG lDirection )
{
	lDirection -= 1;

	// Not a valid door direction.
	if (( lDirection < -1 ) || ( lDirection > 2 ))
		return ( INT_MAX );

	return ( lDirection );
}

//*****************************************************************************
//
LONG CLIENT_AdjustFloorDirection( LONG lDirection )
{
	lDirection -= 1;

	// Not a valid door direction.
	if (( lDirection < -1 ) || ( lDirection > 1 ))
		return ( INT_MAX );

	return ( lDirection );
}

//*****************************************************************************
//
LONG CLIENT_AdjustCeilingDirection( LONG lDirection )
{
	lDirection -= 1;

	// Not a valid door direction.
	if (( lDirection < -1 ) || ( lDirection > 1 ))
		return ( INT_MAX );

	return ( lDirection );
}

//*****************************************************************************
//
LONG CLIENT_AdjustElevatorDirection( LONG lDirection )
{
	lDirection -= 1;

	// Not a valid door direction.
	if (( lDirection < -1 ) || ( lDirection > 1 ))
		return ( INT_MAX );

	return ( lDirection );
}

//*****************************************************************************
//
void CLIENT_LogHUDMessage( char *pszString, LONG lColor )
{
	static const char	szBar[] = TEXTCOLOR_ORANGE "\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36"
"\36\36\36\36\36\36\36\36\36\36\36\36\37" TEXTCOLOR_NORMAL "\n";
	static const char	szLogBar[] = "\n<------------------------------->\n";
	char				acLogColor[3];

	acLogColor[0] = '\x1c';
	acLogColor[1] = (( lColor >= CR_BRICK ) && ( lColor <= CR_YELLOW )) ? ( lColor + 'A' ) : ( '-' );
	acLogColor[2] = '\0';

	AddToConsole( -1, szBar );
	AddToConsole( -1, acLogColor );
	AddToConsole( -1, pszString );
	AddToConsole( -1, szBar );

	// If we have a log file open, log it too.
	if ( Logfile )
	{
		fputs( szLogBar, Logfile );
		fputs( pszString, Logfile );
		fputs( szLogBar, Logfile );
		fflush( Logfile );
	}
}

//*****************************************************************************
//
void CLIENT_UpdatePendingWeapon( const player_t *pPlayer )
{
	// [BB] Only the client needs to do this.
	if ( NETWORK_GetState( ) != NETSTATE_CLIENT )
		return;

	// [BB] Invalid argument, nothing to do.
	if ( pPlayer == NULL )
		return;

	// [BB] The current PendingWeapon is invalid.
	if ( ( pPlayer->PendingWeapon == WP_NOCHANGE ) || ( pPlayer->PendingWeapon == NULL ) )
		return;

	// [BB] A client only needs to handle its own weapon changes.
	if ( static_cast<LONG>( pPlayer - players ) == consoleplayer )
	{
		CLIENTCOMMANDS_WeaponSelect( pPlayer->PendingWeapon->GetClass( ));

		if ( CLIENTDEMO_IsRecording( ))
			CLIENTDEMO_WriteLocalCommand( CLD_LCMD_INVUSE, pPlayer->PendingWeapon->GetClass( )->TypeName.GetChars( ) );
	}
}

//*****************************************************************************
//
void CLIENT_SetActorToLastDeathStateFrame ( AActor *pActor )
{
	FState	*pDeadState = pActor->FindState( NAME_Death );
	FState	*pBaseState = NULL;

	do
	{
		pBaseState = pDeadState;
		if ( pBaseState != NULL )
		{
			pActor->SetState( pBaseState, true );
			pDeadState = pBaseState->GetNextState( );
		}
	} while (( pDeadState != NULL ) && ( pDeadState == pBaseState + 1 ) && ( pBaseState->GetTics() != -1 ) );
	// [BB] The "pBaseState->GetTics() != -1" check prevents jumping over frames with ininite duration.
	// This matters if the death state is not ended by "Stop".
}

//*****************************************************************************
//*****************************************************************************
//
static void client_Header( BYTESTREAM_s *pByteStream )
{
	LONG	lSequence;

	// Read in the sequence. This is the # of the packet the server has sent us.
	// This function shouldn't ever be called since the packet header is parsed seperately.
	lSequence = NETWORK_ReadLong( pByteStream );
//	Printf( "client_Header: Received packet %d\n", lSequence );
}
//*****************************************************************************
//
/*
static void client_ResetSequence( BYTESTREAM_s *pByteStream )
{
//	Printf( "SEQUENCE RESET! g_lLastParsedSequence = %d\n", g_lLastParsedSequence );
	g_lLastParsedSequence = g_lHighestReceivedSequence - 2;
}
*/
//*****************************************************************************
//
static void client_Ping( BYTESTREAM_s *pByteStream )
{
	ULONG	ulTime;

	// Read in the time on the server end. We send this back to them, and then the server can
	// tell how many milliseconds passed since it sent the ping message to us.
	ulTime = NETWORK_ReadLong( pByteStream );

	// Send back the server's time.
	CLIENTCOMMANDS_Pong( ulTime );
}

//*****************************************************************************
//
static void client_BeginSnapshot( BYTESTREAM_s *pByteStream )
{
	// Display in the console that we're receiving a snapshot.
	Printf( "Receiving snapshot...\n" );

	// Set the client connection state to receiving a snapshot. This disables things like
	// announcer sounds.
	CLIENT_SetConnectionState( CTS_RECEIVINGSNAPSHOT );
}

//*****************************************************************************
//
static void client_EndSnapshot( BYTESTREAM_s *pByteStream )
{
	// We're all done! Set the new client connection state to active.
	CLIENT_SetConnectionState( CTS_ACTIVE );

	// Display in the console that we have the snapshot now.
	Printf( "Snapshot received.\n" );

	// Hide the console.
	C_HideConsole( );

	// Make the view active.
	viewactive = true;

	// Clear the notify strings.
	C_FlushDisplay( );

	// Now that we've received the snapshot, create the status bar.
	if ( StatusBar != NULL )
	{
		StatusBar->Destroy();
		StatusBar = NULL;
	}

	StatusBar = CreateStatusBar ();
	/*  [BB] Moved to CreateStatusBar()
	switch ( gameinfo.gametype )
	{
	case GAME_Doom:

		StatusBar = CreateDoomStatusBar( );
		break;
	case GAME_Heretic:

		StatusBar = CreateHereticStatusBar( );
		break;
	case GAME_Hexen:

		StatusBar = CreateHexenStatusBar( );
		break;
	case GAME_Strife:

		StatusBar = CreateStrifeStatusBar( );
		break;
	default:

		StatusBar = new FBaseStatusBar( 0 );
		break;
	}
	*/

	if ( StatusBar )
	{
		StatusBar->AttachToPlayer( &players[consoleplayer] );
		StatusBar->NewGame( );
	}

	// Display the message of the day.
	CLIENT_DisplayMOTD( );
}

//*****************************************************************************
//
static void client_SpawnPlayer( BYTESTREAM_s *pByteStream, bool bMorph )
{
	ULONG			ulPlayer;
	player_t		*pPlayer;
	APlayerPawn		*pActor;
	LONG			lID;
	fixed_t			X;
	fixed_t			Y;
	fixed_t			Z;
	angle_t			Angle;
	bool			bSpectating;
	bool			bDeadSpectator;
	bool			bIsBot;
	LONG			lPlayerState;
	LONG			lState;
	LONG			lPlayerClass;
	LONG			lSkin;
	bool			bWasWatchingPlayer;
	AActor			*pCameraActor;
	APlayerPawn		*pOldActor;
	USHORT			usActorNetworkIndex = 0;
	const PClass	*pType;

	// Which player is being spawned?
	ulPlayer = NETWORK_ReadByte( pByteStream );

	// Read in the player's state prior to being spawned. This determines whether or not we
	// should wipe his weapons, etc.
	lPlayerState = NETWORK_ReadByte( pByteStream );

	// Is the player a bot?
	bIsBot = !!NETWORK_ReadByte( pByteStream );

	// State of the player?
	lState = NETWORK_ReadByte( pByteStream );

	// Is he spectating?
	bSpectating = !!NETWORK_ReadByte( pByteStream );

	// Is he a dead spectator?
	bDeadSpectator = !!NETWORK_ReadByte( pByteStream );

	// Network ID of the player's body.
	lID = NETWORK_ReadShort( pByteStream );

	// Angle of the player.
	Angle = NETWORK_ReadLong( pByteStream );

	// XYZ position of the player.
	X = NETWORK_ReadLong( pByteStream );
	Y = NETWORK_ReadLong( pByteStream );
	Z = NETWORK_ReadLong( pByteStream );

	lPlayerClass = NETWORK_ReadShort( pByteStream );

	if ( bMorph )
	{
		// [BB] Read in the identification of the morphed playerpawn
		usActorNetworkIndex = NETWORK_ReadShort( pByteStream );
	}

	// Invalid player ID or not in a level.
	if (( ulPlayer >= MAXPLAYERS ) || ( gamestate != GS_LEVEL ))
		return;

	// If there's already an actor with this net ID, kill it!
	if ( g_NetIDList[lID].pActor != NULL )
	{
/*
		if ( g_NetIDList[lID].pActor == players[consoleplayer].mo )
		{
#ifdef CLIENT_WARNING_MESSAGES
			Printf( "client_SpawnPlayer: WARNING! Tried to delete console player's body!\n" );
#endif
			return;
		}
*/
		g_NetIDList[lID].pActor->Destroy( );
		g_NetIDList[lID].pActor = NULL;
		g_NetIDList[lID].bFree = true;
	}

	// [BB] Remember if we were already ignoring WeaponSelect commands. If so, the server
	// told us to ignore them and we need to continue to do so after spawning the player.
	const bool bSavedIgnoreWeaponSelect = CLIENT_GetIgnoreWeaponSelect ();
	// [BB] Don't let the client send the server WeaponSelect commands for all weapons that
	// are temporarily selected while getting the inventory. 
	CLIENT_IgnoreWeaponSelect ( true );

	// [BB] Possibly play a connect sound.
	if ( cl_connectsound && ( playeringame[ulPlayer] == false ) && bSpectating && ( ulPlayer != static_cast<ULONG>(consoleplayer) )
		&& ( CLIENT_GetConnectionState() != CTS_RECEIVINGSNAPSHOT ) )
		S_Sound( CHAN_AUTO, "zandronum/connect", 1.f, ATTN_NONE );

	// This player is now in the game!
	playeringame[ulPlayer] = true;
	pPlayer = &players[ulPlayer];

	// Kill the player's old icon if necessary.
	if ( pPlayer->pIcon )
	{
		pPlayer->pIcon->Destroy( );
		pPlayer->pIcon = NULL;
	}

	// If the console player is being respawned, and watching another player in demo
	// mode, allow the player to continue watching that player.
	if ((( pPlayer - players ) == consoleplayer ) &&
		( pPlayer->camera ) &&
		( pPlayer->camera != pPlayer->mo ) &&
		( CLIENTDEMO_IsPlaying( )))
	{
		pCameraActor = pPlayer->camera;
	}
	else
		pCameraActor = NULL;

	// First, disassociate the player's corpse.
	bWasWatchingPlayer = false;
	pOldActor = pPlayer->mo;
	if ( pOldActor )
	{
		// If the player's old body is not in a death state, put the body into the last
		// frame of its death state.
		if ( pOldActor->health > 0 )
		{
			CALL_ACTION(A_NoBlocking, pOldActor);

			// Put him in the last frame of his death state.
			CLIENT_SetActorToLastDeathStateFrame ( pOldActor );
		}

		// Check to see if the console player is spectating through this player's eyes.
		// If so, we need to reattach his camera to it when the player respawns.
		if ( pOldActor->CheckLocalView( consoleplayer ))
			bWasWatchingPlayer = true;

		if (( lPlayerState == PST_REBORN ) ||
			( lPlayerState == PST_REBORNNOINVENTORY ) ||
			( lPlayerState == PST_ENTER ) ||
			( lPlayerState == PST_ENTERNOINVENTORY ))
		{
			pOldActor->player = NULL;
			pOldActor->id = -1;
			// [BB] This will eventually free the player's body's network ID.
			G_QueueBody (pOldActor);
		}
		else
		{
			pOldActor->Destroy( );
			pOldActor->player = NULL;
			pOldActor = NULL;
		}
	}

	// [BB] We may not filter coop inventory if the player changed the player class.
	// Thus we need to keep track of the old class.
	const BYTE oldPlayerClass = pPlayer->CurrentPlayerClass;

	// Set up the player class.
	pPlayer->CurrentPlayerClass = lPlayerClass;
	pPlayer->cls = PlayerClasses[pPlayer->CurrentPlayerClass].Type;

	if ( bMorph )
	{
		// [BB] Get the morphed player class.
		pType = NETWORK_GetClassFromIdentification( usActorNetworkIndex );
		// [BB] We'll be casting the spawned body to APlayerPawn, so we must check
		// if the desired class is valid.
		if ( pType && pType->IsDescendantOf( RUNTIME_CLASS( APlayerPawn ) ) )
			pPlayer->cls = pType;
	}

	// Spawn the body.
	pActor = static_cast<APlayerPawn *>( Spawn( pPlayer->cls, X, Y, Z, NO_REPLACE ));

	pPlayer->mo = pActor;
	pActor->player = pPlayer;
	pPlayer->playerstate = lState;

	// If we were watching through this player's eyes, reattach the camera.
	if ( bWasWatchingPlayer )
		players[consoleplayer].camera = pPlayer->mo;

	// Set the network ID.
	pPlayer->mo->lNetID = lID;
	if ( lID != -1 )
	{
		g_NetIDList[lID].bFree = false;
		g_NetIDList[lID].pActor = pPlayer->mo;
	}

	// Set the spectator variables [after G_PlayerReborn so our data doesn't get lost] [BB] Why?.
	// [BB] To properly handle that spectators don't get default inventory, we need to set this
	// before calling G_PlayerReborn (which in turn calls GiveDefaultInventory).
	pPlayer->bSpectating = bSpectating;
	pPlayer->bDeadSpectator = bDeadSpectator;

	if (( lPlayerState == PST_REBORN ) ||
		( lPlayerState == PST_REBORNNOINVENTORY ) ||
		( lPlayerState == PST_ENTER ) ||
		( lPlayerState == PST_ENTERNOINVENTORY ))
	{
		G_PlayerReborn( ulPlayer );
	}
	// [BB] The player possibly changed the player class, so we have to correct the health (usually done in G_PlayerReborn).
	else if ( lPlayerState == PST_LIVE )
		pPlayer->health = pPlayer->mo->GetDefault ()->health;

	// Give all cards in death match mode.
	if ( deathmatch )
		pPlayer->mo->GiveDeathmatchInventory( );
	// [BC] Don't filter coop inventory in teamgame mode.
	// Special inventory handling for respawning in coop.
	// [BB] Also don't do so if the player changed the player class.
	else if (( teamgame == false ) &&
			 ( lPlayerState == PST_REBORN ) &&
			 ( oldPlayerClass == pPlayer->CurrentPlayerClass ) &&
			 ( pOldActor ))
	{
		pPlayer->mo->FilterCoopRespawnInventory( pOldActor );
	}

	// Also, destroy all of the old actor's inventory.
	if ( pOldActor )
		pOldActor->DestroyAllInventory( );

	// If this is the console player, and he's spawned as a regular player, he's definitely not
	// in line anymore!
	if ((( pPlayer - players ) == consoleplayer ) && ( pPlayer->bSpectating == false ))
		JOINQUEUE_SetClientPositionInLine( -1 );

	// Set the player's bot status.
	pPlayer->bIsBot = bIsBot;

	// [BB] If this if not "our" player, clear the weapon selected from the inventory and wait for
	// the server to tell us the selected weapon.
	if ( ( ( pPlayer - players ) != consoleplayer ) && ( pPlayer->bIsBot == false ) )
		PLAYER_ClearWeapon ( pPlayer );

	// [GRB] Reset skin
	pPlayer->userinfo.skin = R_FindSkin (skins[pPlayer->userinfo.skin].name, pPlayer->CurrentPlayerClass);

	// [WS] Don't set custom skin color when the player is morphed.
	// [BB] In team games (where we assume that all players are on a team), we allow the team color for morphed players.
	if (!(pActor->flags2 & MF2_DONTTRANSLATE) && ( !bMorph || ( GAMEMODE_GetFlags( GAMEMODE_GetCurrentMode( )) & GMF_PLAYERSONTEAMS )))
	{
		// [RH] Be sure the player has the right translation
		R_BuildPlayerTranslation ( ulPlayer );

		// [RH] set color translations for player sprites
		pActor->Translation = TRANSLATION( TRANSLATION_Players, ulPlayer );
	}
	pActor->angle = Angle;
	pActor->pitch = pActor->roll = 0;
	pActor->health = pPlayer->health;
	pActor->lFixedColormap = NOFIXEDCOLORMAP;

	//Added by MC: Identification (number in the players[MAXPLAYERS] array)
	pActor->id = ulPlayer;

	// [RH] Set player sprite based on skin
	// [BC] Handle cl_skins here.
	if ( cl_skins <= 0 )
	{
		lSkin = R_FindSkin( "base", pPlayer->CurrentPlayerClass );
		pActor->flags4 |= MF4_NOSKIN;
	}
	else if ( cl_skins >= 2 )
	{
		if ( skins[pPlayer->userinfo.skin].bCheat )
		{
			lSkin = R_FindSkin( "base", pPlayer->CurrentPlayerClass );
			pActor->flags4 |= MF4_NOSKIN;
		}
		else
			lSkin = pPlayer->userinfo.skin;
	}
	else
		lSkin = pPlayer->userinfo.skin;

	if (( lSkin < 0 ) || ( lSkin >= static_cast<LONG>(skins.Size()) ))
		lSkin = R_FindSkin( "base", pPlayer->CurrentPlayerClass );

	// [BB] There is no skin for the morphed class.
	if ( !bMorph )
	{
		pActor->sprite = skins[lSkin].sprite;
		pActor->scaleX = skins[lSkin].ScaleX;
		pActor->scaleY = skins[lSkin].ScaleY;
	}

	pPlayer->DesiredFOV = pPlayer->FOV = 90.f;
	// If the console player was watching another player in demo mode, continue to follow
	// that other player.
	if ( pCameraActor )
		pPlayer->camera = pCameraActor;
	else
		pPlayer->camera = pActor;
	pPlayer->playerstate = PST_LIVE;
	pPlayer->refire = 0;
	pPlayer->damagecount = 0;
	pPlayer->bonuscount = 0;
	// [BB] Remember if the player was morphed.
	const bool bPlayerWasMorphed = ( pPlayer->morphTics != 0 );
	pPlayer->morphTics = 0;
	pPlayer->extralight = 0;
	pPlayer->fixedcolormap = NOFIXEDCOLORMAP;
	pPlayer->fixedlightlevel = -1;
	pPlayer->viewheight = pPlayer->mo->ViewHeight;

	pPlayer->attacker = NULL;
	pPlayer->BlendR = pPlayer->BlendG = pPlayer->BlendB = pPlayer->BlendA = 0.f;
	pPlayer->cheats = 0;
	pPlayer->Uncrouch( );

	// killough 10/98: initialize bobbing to 0.
	pPlayer->velx = 0;
	pPlayer->vely = 0;
/*
	// If the console player is being respawned, place the camera back in his own body.
	if ( ulPlayer == consoleplayer )
		players[consoleplayer].camera = players[consoleplayer].mo;
*/
	// setup gun psprite
	P_SetupPsprites (pPlayer);

	// If this console player is looking through this player's eyes, attach the status
	// bar to this player.
	if (( StatusBar ) && ( players[ulPlayer].mo->CheckLocalView( consoleplayer )))
		StatusBar->AttachToPlayer( pPlayer );
/*
	if (( StatusBar ) &&
		( players[consoleplayer].camera ) &&
		( players[consoleplayer].camera->player ))
	{
		StatusBar->AttachToPlayer( players[consoleplayer].camera->player );
	}
*/
	// If the player is a spectator, set some properties.
	if ( pPlayer->bSpectating )
	{
		// [BB] Set a bunch of stuff, e.g. make the player unshootable, etc.
		PLAYER_SetDefaultSpectatorValues ( pPlayer );

		// Don't lag anymore if we're a spectator.
		if ( ulPlayer == static_cast<ULONG>(consoleplayer) )
			g_bClientLagging = false;
	}
	// [BB] Don't spawn fog when receiving a snapshot.
	// [WS] Don't spawn fog when a player is morphing. The server will tell us.
	else if ( CLIENT_GetConnectionState() != CTS_RECEIVINGSNAPSHOT && !bMorph && !bPlayerWasMorphed )
	{
		// Spawn the respawn fog.
		unsigned an = pActor->angle >> ANGLETOFINESHIFT;
		if (!(pActor->angle == ANGLE_180 && (zacompatflags & ZACOMPATF_SILENT_WEST_SPAWNS)))
			Spawn( "TeleportFog", pActor->x + 20 * finecosine[an],
				pActor->y + 20 * finesine[an],
				pActor->z + TELEFOGHEIGHT, ALLOW_REPLACE );
	}

	pPlayer->playerstate = PST_LIVE;
	
	// [BB] If the player is reborn, we have to substitute all pointers
	// to the old body to the new one. Otherwise (among other things) CLIENTSIDE
	// ENTER scripts stop working after the corresponding player is respawned.
	if (lPlayerState == PST_REBORN || lPlayerState == PST_REBORNNOINVENTORY)
	{
		if ( pOldActor != NULL )
		{
			DObject::StaticPointerSubstitution (pOldActor, pPlayer->mo);
			// PointerSubstitution() will also affect the bodyque, so undo that now.
			for (int ii=0; ii < BODYQUESIZE; ++ii)
				if (bodyque[ii] == pPlayer->mo)
					bodyque[ii] = pOldActor;
		}
	}

	if ( bMorph )
	{
		// [BB] Bring up the weapon of the morphed class.
		pPlayer->mo->ActivateMorphWeapon();
		// [BB] This marks the player as morphed. The client doesn't need to know the real
		// morph time since the server handles the timing of the unmorphing.
		pPlayer->morphTics = -1;
	}
	else
	{
		pPlayer->morphTics = 0;

		// [BB] If the player was just unmorphed, we need to set reactiontime to the same value P_UndoPlayerMorph uses.
		if ( bPlayerWasMorphed )
			pPlayer->mo->reactiontime = 18;
	}


	// If this is the consoleplayer, set the realorigin and ServerXYZMom.
	if ( ulPlayer == static_cast<ULONG>(consoleplayer) )
	{
		CLIENT_AdjustPredictionToServerSideConsolePlayerMove( pPlayer->mo->x, pPlayer->mo->y, pPlayer->mo->z );

		pPlayer->ServerXYZMom[0] = 0;
		pPlayer->ServerXYZMom[1] = 0;
		pPlayer->ServerXYZMom[2] = 0;
	}

	// [BB] Now that we have our inventory, tell the server the weapon we selected from it.
	// [BB] Only do this if the server didn't tell us to ignore the WeaponSelect commands.
	if ( bSavedIgnoreWeaponSelect == false ) {
		CLIENT_IgnoreWeaponSelect ( false );
		if ((( pPlayer - players ) == consoleplayer ) && ( pPlayer->ReadyWeapon ) )
		{
			CLIENTCOMMANDS_WeaponSelect( pPlayer->ReadyWeapon->GetClass( ));

			if ( CLIENTDEMO_IsRecording( ))
				CLIENTDEMO_WriteLocalCommand( CLD_LCMD_INVUSE, pPlayer->ReadyWeapon->GetClass( )->TypeName.GetChars( ) );
			// [BB] When playing a demo, we will bring up what we recorded with CLD_LCMD_INVUSE.
			else if ( CLIENTDEMO_IsPlaying() )
				PLAYER_ClearWeapon ( pPlayer );
		}
	}

	// [TP] If we're overriding colors, rebuild translations now.
	// If we just joined the game, rebuild all translations,
	// otherwise recoloring the player in question is sufficient.
	if ( D_ShouldOverridePlayerColors() )
	{
		bool joinedgame = ( ulPlayer == static_cast<ULONG>( consoleplayer ))
			&& ( lPlayerState == PST_ENTER || lPlayerState == PST_ENTERNOINVENTORY );
		D_UpdatePlayerColors( joinedgame ? MAXPLAYERS : ulPlayer );
	}

	// Refresh the HUD because this is potentially a new player.
	SCOREBOARD_RefreshHUD( );
}

//*****************************************************************************
//
static void client_MovePlayer( BYTESTREAM_s *pByteStream )
{
	ULONG		ulPlayer;
	bool		bVisible;
	fixed_t		X = 0;
	fixed_t		Y = 0;
	fixed_t		Z = 0;
	angle_t		Angle = 0;
	fixed_t		MomX = 0;
	fixed_t		MomY = 0;
	fixed_t		MomZ = 0;
	bool		bCrouching = false;

	// Read in the player number.
	ulPlayer = NETWORK_ReadByte( pByteStream );

	// Is this player visible? If not, there's no other information to read in.
	ULONG ulFlags = NETWORK_ReadByte( pByteStream );
	bVisible = ( ulFlags & PLAYER_VISIBLE );

	// The server only sends position, angle, etc. information if the player is actually
	// visible to us.
	if ( bVisible )
	{
		// Read in the player's XYZ position.
		// [BB] The x/y position has to be sent at full precision.
		X = NETWORK_ReadLong( pByteStream );
		Y = NETWORK_ReadLong( pByteStream );
		Z = NETWORK_ReadShort( pByteStream ) << FRACBITS;

		// Read in the player's angle.
		Angle = NETWORK_ReadLong( pByteStream );

		// Read in the player's XYZ momentum.
		MomX = NETWORK_ReadShort( pByteStream ) << FRACBITS;
		MomY = NETWORK_ReadShort( pByteStream ) << FRACBITS;
		MomZ = NETWORK_ReadShort( pByteStream ) << FRACBITS;

		// Read in whether or not the player's crouching.
		bCrouching = !!NETWORK_ReadByte( pByteStream );
	}

	// Check to make sure everything is valid. If not, break out.
	if (( PLAYER_IsValidPlayer( ulPlayer ) == false ) || ( players[ulPlayer].mo == NULL ) || ( gamestate != GS_LEVEL ))
		return;

	// If we're not allowed to know the player's location, then just make him invisible.
	if ( bVisible == false )
	{
		players[ulPlayer].mo->renderflags |= RF_INVISIBLE;

		// Don't move the player since the server didn't send any useful position information.
		return;
	}
	else
		players[ulPlayer].mo->renderflags &= ~RF_INVISIBLE;

	// Set the player's XYZ position.
	// [BB] But don't just set the position, but also properly set floorz and ceilingz, etc.
	CLIENT_MoveThing( players[ulPlayer].mo, X, Y, Z );

	// Set the player's angle.
	players[ulPlayer].mo->angle = Angle;

	// Set the player's XYZ momentum.
	players[ulPlayer].mo->velx = MomX;
	players[ulPlayer].mo->vely = MomY;
	players[ulPlayer].mo->velz = MomZ;

	// Is the player crouching?
	players[ulPlayer].crouchdir = ( bCrouching ) ? 1 : -1;

	if (( players[ulPlayer].crouchdir == 1 ) &&
		( players[ulPlayer].crouchfactor < FRACUNIT ) &&
		(( players[ulPlayer].mo->z + players[ulPlayer].mo->height ) < players[ulPlayer].mo->ceilingz ))
	{
		P_CrouchMove( &players[ulPlayer], 1 );
	}
	else if (( players[ulPlayer].crouchdir == -1 ) &&
		( players[ulPlayer].crouchfactor > FRACUNIT/2 ))
	{
		P_CrouchMove( &players[ulPlayer], -1 );
	}

	// [BB] Set whether the player is attacking or not.
	// Check: Is it a good idea to only do this, when the player is visible?
	if ( ulFlags & PLAYER_ATTACK )
		players[ulPlayer].cmd.ucmd.buttons |= BT_ATTACK;
	else
		players[ulPlayer].cmd.ucmd.buttons &= ~BT_ATTACK;

	if ( ulFlags & PLAYER_ALTATTACK )
		players[ulPlayer].cmd.ucmd.buttons |= BT_ALTATTACK;
	else
		players[ulPlayer].cmd.ucmd.buttons &= ~BT_ALTATTACK;
}

//*****************************************************************************
//
static void client_DamagePlayer( BYTESTREAM_s *pByteStream )
{
	ULONG		ulPlayer;
	LONG		lHealth;
	LONG		lArmor;
	LONG		lDamage;
	ABasicArmor	*pArmor;
	FState		*pPainState;

	// Read in the player being damaged.
	ulPlayer = NETWORK_ReadByte( pByteStream );

	// Read in the new health and armor values.
	lHealth = NETWORK_ReadShort( pByteStream );
	lArmor = NETWORK_ReadShort( pByteStream );

	// [BB] Read in the NetID of the damage inflictor and find the corresponding actor.
	AActor *pAttacker = CLIENT_FindThingByNetID( NETWORK_ReadShort( pByteStream ) );

	// Level not loaded, ignore...
	if ( gamestate != GS_LEVEL )
		return;

	// Check to make sure everything is valid. If not, break out.
	if (( PLAYER_IsValidPlayer( ulPlayer ) == false ) || ( players[ulPlayer].mo == NULL ))
		return;

	// Calculate the amount of damage being taken based on the old health value, and the
	// new health value.
	lDamage = players[ulPlayer].health - lHealth;

	// Do the damage.
//	P_DamageMobj( players[ulPlayer].mo, NULL, NULL, lDamage, 0, 0 );

	// Set the new health value.
	players[ulPlayer].mo->health = players[ulPlayer].health = lHealth;

	pArmor = players[ulPlayer].mo->FindInventory<ABasicArmor>( );
	if ( pArmor )
		pArmor->Amount = lArmor;

	// [BB] Set the inflictor of the damage (necessary to let the HUD mugshot look in direction of the inflictor).
	players[ulPlayer].attacker = pAttacker;

	// Set the damagecount, for blood on the screen.
	players[ulPlayer].damagecount += lDamage;
	if ( players[ulPlayer].damagecount > 100 )
		players[ulPlayer].damagecount = 100;
	if ( players[ulPlayer].damagecount < 0 )
		players[ulPlayer].damagecount = 0;

	if ( players[ulPlayer].mo->CheckLocalView( consoleplayer ))
	{
		if ( lDamage > 100 )
			lDamage = 100;

		I_Tactile( 40,10,40 + lDamage * 2 );
	}

	// Also, make sure they get put into the pain state.
	pPainState = players[ulPlayer].mo->FindState( NAME_Pain );
	if ( pPainState )
		players[ulPlayer].mo->SetState( pPainState );
}

//*****************************************************************************
//
static void client_KillPlayer( BYTESTREAM_s *pByteStream )
{
	ULONG		ulPlayer;
	LONG		lSourceID;
	LONG		lInflictorID;
	LONG		lHealth;
	FName		MOD;
	USHORT		usActorNetworkIndex;
	FName		DamageType;
	AActor		*pSource;
	AActor		*pInflictor;
	AWeapon		*pWeapon;
	ULONG		ulIdx;
	ULONG		ulSourcePlayer;

	// Read in the player who's dying.
	ulPlayer = NETWORK_ReadByte( pByteStream );

	// Read in the actor that killed the player.
	lSourceID = NETWORK_ReadShort( pByteStream );

	// Read in the network ID of the inflictor.
	lInflictorID = NETWORK_ReadShort( pByteStream );

	// Read in how much health they currently have (for gibs).
	lHealth = NETWORK_ReadShort( pByteStream );

	// Read in the means of death.
	MOD = NETWORK_ReadString( pByteStream );

	// Read in the thing's damage type.
	DamageType = NETWORK_ReadString( pByteStream );

	// Read in the player who did the killing's ready weapon's identification so we can properly do obituary
	// messages.
	usActorNetworkIndex = NETWORK_ReadShort( pByteStream );

	// Check to make sure everything is valid. If not, break out.
	if (( PLAYER_IsValidPlayer( ulPlayer ) == false ) || ( players[ulPlayer].mo == NULL ))
		return;

	// Find the actor associated with the source. It's okay if this actor does not exist.
	if ( lSourceID != -1 )
		pSource = CLIENT_FindThingByNetID( lSourceID );
	else
		pSource = NULL;

	// Find the actor associated with the inflictor. It's okay if this actor does not exist.
	if ( lInflictorID != -1 )
		pInflictor = CLIENT_FindThingByNetID( lInflictorID );
	else
		pInflictor = NULL;

	// Set the player's new health.
	players[ulPlayer].health = players[ulPlayer].mo->health = lHealth;

	// Set the player's damage type.
	players[ulPlayer].mo->DamageType = DamageType;

	// Kill the player.
	players[ulPlayer].mo->Die( pSource, pInflictor );

	// [BB] Set the attacker, necessary to let the death view follow the killer.
	players[ulPlayer].attacker = pSource;

	// If health on the status bar is less than 0%, make it 0%.
	if ( players[ulPlayer].health <= 0 )
		players[ulPlayer].health = 0;

	ulSourcePlayer = MAXPLAYERS;
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if (( playeringame[ulIdx] == false ) ||
			( players[ulIdx].mo == NULL ))
		{
			continue;
		}

		if ( players[ulIdx].mo == pSource )
		{
			ulSourcePlayer = ulIdx;
			break;
		}
	}

	if (( (GAMEMODE_GetFlags( GAMEMODE_GetCurrentMode( )) & GMF_COOPERATIVE) == false ) &&
		( cl_showlargefragmessages ) &&
		( ulSourcePlayer < MAXPLAYERS ) &&
		( ulPlayer != ulSourcePlayer ) &&
		( MOD != NAME_SpawnTelefrag ) &&
		( GAMEMODE_IsGameInProgress() ))
	{
		if ((( ( GAMEMODE_GetFlags(GAMEMODE_GetCurrentMode()) & GMF_PLAYERSEARNFRAGS ) == false ) || (( fraglimit == 0 ) || ( players[ulSourcePlayer].fragcount < fraglimit ))) &&
			(( ( ( GAMEMODE_GetFlags(GAMEMODE_GetCurrentMode()) & GMF_PLAYERSEARNWINS ) && !( GAMEMODE_GetFlags(GAMEMODE_GetCurrentMode()) & GMF_PLAYERSONTEAMS ) ) == false ) || (( winlimit == 0 ) || ( players[ulSourcePlayer].ulWins < static_cast<ULONG>(winlimit) ))) &&
			(( ( ( GAMEMODE_GetFlags(GAMEMODE_GetCurrentMode()) & GMF_PLAYERSEARNWINS ) && ( GAMEMODE_GetFlags(GAMEMODE_GetCurrentMode()) & GMF_PLAYERSONTEAMS ) ) == false ) || (( winlimit == 0 ) || ( TEAM_GetWinCount( players[ulSourcePlayer].ulTeam ) < winlimit ))))
		{
			// Display a large "You were fragged by <name>." message in the middle of the screen.
			if ( ulPlayer == static_cast<ULONG>(consoleplayer) )
				SCOREBOARD_DisplayFraggedMessage( &players[ulSourcePlayer] );
			// Display a large "You fragged <name>!" message in the middle of the screen.
			else if ( ulSourcePlayer == static_cast<ULONG>(consoleplayer) )
				SCOREBOARD_DisplayFragMessage( &players[ulPlayer] );
		}
	}

	// [BB] Temporarily change the ReadyWeapon of ulSourcePlayer to the one the server told us.
	AWeapon *pSavedReadyWeapon = ( ulSourcePlayer < MAXPLAYERS ) ? players[ulSourcePlayer].ReadyWeapon : NULL;

	if ( ulSourcePlayer < MAXPLAYERS )
	{
		if ( NETWORK_GetClassFromIdentification( usActorNetworkIndex ) == NULL )
			players[ulSourcePlayer].ReadyWeapon = NULL;
		else if ( players[ulSourcePlayer].mo )
		{
			pWeapon = static_cast<AWeapon *>( players[ulSourcePlayer].mo->FindInventory( NETWORK_GetClassFromIdentification( usActorNetworkIndex )));
			if ( pWeapon == NULL )
				pWeapon = static_cast<AWeapon *>( players[ulSourcePlayer].mo->GiveInventoryType( NETWORK_GetClassFromIdentification( usActorNetworkIndex )));

			if ( pWeapon )
				players[ulSourcePlayer].ReadyWeapon = pWeapon;
		}
	}

	// Finally, print the obituary string.
	ClientObituary( players[ulPlayer].mo, pInflictor, pSource, MOD );

	// [BB] Restore the weapon the player actually is using now.
	if ( ( ulSourcePlayer < MAXPLAYERS ) && ( players[ulSourcePlayer].ReadyWeapon != pSavedReadyWeapon ) )
		players[ulSourcePlayer].ReadyWeapon = pSavedReadyWeapon;
/*
	if ( ulSourcePlayer < MAXPLAYERS )
		ClientObituary( players[ulPlayer].mo, pInflictor, players[ulSourcePlayer].mo, MOD );
	else
		ClientObituary( players[ulPlayer].mo, pInflictor, NULL, MOD );
*/

	// Refresh the HUD, since this could affect the number of players left in an LMS game.
	SCOREBOARD_RefreshHUD( );
}

//*****************************************************************************
//
static void client_SetPlayerHealth( BYTESTREAM_s *pByteStream )
{
	LONG	lHealth;
	ULONG	ulPlayer;

	// Read in the player whose health is being altered.
	ulPlayer = NETWORK_ReadByte( pByteStream );

	// Read in the health;
	lHealth = NETWORK_ReadShort( pByteStream );

	// If this is an invalid player, break out.
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	players[ulPlayer].health = lHealth;
	if ( players[ulPlayer].mo )
		players[ulPlayer].mo->health = lHealth;
}

//*****************************************************************************
//
static void client_SetPlayerArmor( BYTESTREAM_s *pByteStream )
{
	ULONG		ulPlayer;
	LONG		lArmorAmount;
	const char	*pszArmorIconName;
	AInventory	*pArmor;

	// Read in the player whose armor display is updated.
	ulPlayer = NETWORK_ReadByte( pByteStream );

	// Read in the armor amount and icon.
	lArmorAmount = NETWORK_ReadShort( pByteStream );
	pszArmorIconName = NETWORK_ReadString( pByteStream );

	// If this is an invalid player, break out.
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	pArmor = players[ulPlayer].mo ? players[ulPlayer].mo->FindInventory<ABasicArmor>( ) : NULL;
	if ( pArmor != NULL )
	{
		pArmor->Amount = lArmorAmount;
		pArmor->Icon = TexMan.GetTexture( pszArmorIconName, 0 );
	}
}

//*****************************************************************************
//
static void client_SetPlayerState( BYTESTREAM_s *pByteStream )
{
	ULONG			ulPlayer;
	PLAYERSTATE_e	ulState;

	// Read in the player whose state is being updated.
	ulPlayer = NETWORK_ReadByte( pByteStream );
	
	// Read in the state to update him to.
	ulState = static_cast<PLAYERSTATE_e>(NETWORK_ReadByte( pByteStream ));

	// If this isn't a valid player, break out.
	if (( PLAYER_IsValidPlayer( ulPlayer ) == false ) || ( players[ulPlayer].mo == NULL ))
	{
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_SetPlayerState: No player object for player: %d\n", ulPlayer );
#endif
		return;
	}

	// If the player is dead, then we shouldn't have to update his state.
	if ( players[ulPlayer].mo->health <= 0 )
		return;

	// Finally, change the player's state to whatever the server told us it was.
	switch( ulState )
	{
	case STATE_PLAYER_IDLE:

		players[ulPlayer].mo->PlayIdle( );
		break;
	case STATE_PLAYER_SEE:

		players[ulPlayer].mo->SetState( players[ulPlayer].mo->SpawnState );
		players[ulPlayer].mo->PlayRunning( );
		break;
	case STATE_PLAYER_ATTACK:
	case STATE_PLAYER_ATTACK_ALTFIRE:

		players[ulPlayer].mo->PlayAttacking( );
		// [BB] This partially fixes the problem that attack animations are not displayed in demos
		// if you are spying a player that you didn't spy when recording the demo. Still has problems
		// with A_ReFire.
		//
		// [BB] This is also needed to update the ammo count of the other players in coop game modes.
		//
		// [BB] It's necessary at all, because the server only informs a client about the cmd.ucmd.buttons
		// value (containing the information if a player uses BT_ATTACK or BT_ALTATTACK) of the player
		// who's eyes the client is spying through.
		// [BB] SERVERCOMMANDS_MovePlayer/client_MovePlayer now informs a client about BT_ATTACK or BT_ALTATTACK
		// of every player. This hopefully properly fixes these problems once and for all.
		/*
		if ( ( CLIENTDEMO_IsPlaying( ) || ( ( GAMEMODE_GetFlags( GAMEMODE_GetCurrentMode( )) & GMF_COOPERATIVE ) && static_cast<signed> (ulPlayer) != consoleplayer ) )
				&& players[ulPlayer].ReadyWeapon )
		{
			if ( ulState == STATE_PLAYER_ATTACK )
				P_SetPsprite (&players[ulPlayer], ps_weapon, players[ulPlayer].ReadyWeapon->GetAtkState(!!players[ulPlayer].refire));
			else
				P_SetPsprite (&players[ulPlayer], ps_weapon, players[ulPlayer].ReadyWeapon->GetAltAtkState(!!players[ulPlayer].refire));

		}
		*/
		break;
	case STATE_PLAYER_ATTACK2:

		players[ulPlayer].mo->PlayAttacking2( );
		break;
	default:

#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_SetPlayerState: Unknown state: %d\n", ulState );
#endif
		break;
	}
}

//*****************************************************************************
//
static void client_SetPlayerUserInfo( BYTESTREAM_s *pByteStream )
{
	ULONG		ulIdx;
    player_t	*pPlayer;
	ULONG		ulPlayer;
	ULONG		ulFlags;
	char		szName[MAXPLAYERNAME + 1];
	LONG		lGender = 0;
	LONG		lColor = 0;
	const char	*pszSkin = NULL;
	LONG		lRailgunTrailColor = 0;
	LONG		lHandicap = 0;
	LONG		lSkin;
	ULONG		ulTicsPerUpdate;
	ULONG		ulConnectionType;
	BYTE		clientFlags;

	// Read in the player whose userinfo is being sent to us.
	ulPlayer = NETWORK_ReadByte( pByteStream );

	// Read in what userinfo entries are going to be updated.
	ulFlags = NETWORK_ReadShort( pByteStream );

	// Read in the player's name.
	if ( ulFlags & USERINFO_NAME )
	{
		strncpy( szName, NETWORK_ReadString( pByteStream ), MAXPLAYERNAME );
		szName[MAXPLAYERNAME] = 0;
	}

	// Read in the player's gender.
	if ( ulFlags & USERINFO_GENDER )
		lGender = NETWORK_ReadByte( pByteStream );

	// Read in the player's color.
	if ( ulFlags & USERINFO_COLOR )
		lColor = NETWORK_ReadLong( pByteStream );

	// Read in the player's railgun trail color.
	if ( ulFlags & USERINFO_RAILCOLOR )
		lRailgunTrailColor = NETWORK_ReadByte( pByteStream );

	// Read in the player's skin.
	if ( ulFlags & USERINFO_SKIN )
		pszSkin = NETWORK_ReadString( pByteStream );

	// Read in the player's handicap.
	if ( ulFlags & USERINFO_HANDICAP )
		lHandicap = NETWORK_ReadByte( pByteStream );

	// [BB] Read in the player's respawnonfire setting.
	if ( ulFlags & USERINFO_TICSPERUPDATE )
		ulTicsPerUpdate = NETWORK_ReadByte( pByteStream );

	// [BB]
	if ( ulFlags & USERINFO_CONNECTIONTYPE )
		ulConnectionType = NETWORK_ReadByte( pByteStream );

	// [CK] We do bitfields now.
	if ( ulFlags & USERINFO_CLIENTFLAGS )
		clientFlags = NETWORK_ReadByte( pByteStream );

	// If this isn't a valid player, break out.
	// We actually send the player's userinfo before he gets spawned, thus putting him in
	// the game. Therefore, this call won't work unless the way the server sends the data
	// changes.
//	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
//		return;
	if ( ulPlayer >= MAXPLAYERS )
		return;

	// Now that everything's been read in, actually set the player's userinfo properties.
	// Player's name.
    pPlayer = &players[ulPlayer];
	if ( ulFlags & USERINFO_NAME )
	{
		if ( strlen( szName ) > MAXPLAYERNAME )
			szName[MAXPLAYERNAME] = '\0';
		strcpy( pPlayer->userinfo.netname, szName );

		// Remove % signs from names.
		for ( ulIdx = 0; ulIdx < strlen( pPlayer->userinfo.netname ); ulIdx++ )
		{
			if ( pPlayer->userinfo.netname[ulIdx] == '%' )
				pPlayer->userinfo.netname[ulIdx] = ' ';
		}
	}

	// Other info.
	// [BB] Make sure that the gender is valid.
	if ( ulFlags & USERINFO_GENDER )
		pPlayer->userinfo.gender = clamp ( static_cast<int>(lGender), 0, 2 );
	if ( ulFlags & USERINFO_COLOR )
	    pPlayer->userinfo.color = lColor;
	if ( ulFlags & USERINFO_RAILCOLOR )
		pPlayer->userinfo.lRailgunTrailColor = lRailgunTrailColor;

	// Make sure the skin is valid.
	if ( ulFlags & USERINFO_SKIN )
	{
		pPlayer->userinfo.skin = R_FindSkin( pszSkin, pPlayer->CurrentPlayerClass );

		// [BC] Handle cl_skins here.
		if ( cl_skins <= 0 )
		{
			lSkin = R_FindSkin( "base", pPlayer->CurrentPlayerClass );
			if ( pPlayer->mo )
				pPlayer->mo->flags4 |= MF4_NOSKIN;
		}
		else if ( cl_skins >= 2 )
		{
			if ( skins[pPlayer->userinfo.skin].bCheat )
			{
				lSkin = R_FindSkin( "base", pPlayer->CurrentPlayerClass );
				if ( pPlayer->mo )
					pPlayer->mo->flags4 |= MF4_NOSKIN;
			}
			else
				lSkin = pPlayer->userinfo.skin;
		}
		else
			lSkin = pPlayer->userinfo.skin;

		if (( lSkin < 0 ) || ( lSkin >= static_cast<LONG>(skins.Size()) ))
			lSkin = R_FindSkin( "base", pPlayer->CurrentPlayerClass );

		if ( pPlayer->mo )
		{
			pPlayer->mo->sprite = skins[lSkin].sprite;
			pPlayer->mo->scaleX = skins[lSkin].ScaleX;
			pPlayer->mo->scaleY = skins[lSkin].ScaleY;
		}
	}

	// Read in the player's handicap.
	if ( ulFlags & USERINFO_HANDICAP )
	{
		pPlayer->userinfo.lHandicap = lHandicap;
		if ( pPlayer->userinfo.lHandicap < 0 )
			pPlayer->userinfo.lHandicap = 0;
		else if ( pPlayer->userinfo.lHandicap > deh.MaxSoulsphere )
			pPlayer->userinfo.lHandicap = deh.MaxSoulsphere;
	}

	if ( ulFlags & USERINFO_TICSPERUPDATE )
		pPlayer->userinfo.ulTicsPerUpdate = ulTicsPerUpdate;

	if ( ulFlags & USERINFO_CONNECTIONTYPE )
		pPlayer->userinfo.ulConnectionType = ulConnectionType;

	// [CK] We do compressed bitfields now.
	if ( ulFlags & USERINFO_CLIENTFLAGS )
		pPlayer->userinfo.clientFlags = clientFlags;

	// Build translation tables, always gotta do this!
	R_BuildPlayerTranslation( ulPlayer );
}

//*****************************************************************************
//
static void client_SetPlayerFrags( BYTESTREAM_s *pByteStream )
{
	ULONG	ulPlayer;
	LONG	lFragCount;

	// Read in the player whose frags are being updated.
	ulPlayer = NETWORK_ReadByte( pByteStream );

	// Read in the number of points he's supposed to get.
	lFragCount = NETWORK_ReadShort( pByteStream );

	// If this isn't a valid player, break out.
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	if (( g_ConnectionState == CTS_ACTIVE ) &&
		( GAMEMODE_GetFlags(GAMEMODE_GetCurrentMode()) & GMF_PLAYERSEARNFRAGS ) &&
		!( GAMEMODE_GetFlags( GAMEMODE_GetCurrentMode( )) & GMF_PLAYERSONTEAMS ) &&
		( GAMEMODE_IsGameInProgress() ) &&
		// [BB] If we are still in the first tic of the level, we are receiving the frag count
		// as part of the full update (that is not considered as a snapshot after a "changemap"
		// map change). Thus don't announce anything in this case.
		( level.time != 0 ))
	{
		ANNOUNCER_PlayFragSounds( ulPlayer, players[ulPlayer].fragcount, lFragCount );
	}

	// Finally, set the player's frag count, and refresh the HUD.
	players[ulPlayer].fragcount = lFragCount;
	SCOREBOARD_RefreshHUD( );
}

//*****************************************************************************
//
static void client_SetPlayerPoints( BYTESTREAM_s *pByteStream )
{
	ULONG	ulPlayer;
	LONG	lPointCount;

	// Read in the player whose frags are being updated.
	ulPlayer = NETWORK_ReadByte( pByteStream );

	// Read in the number of points he's supposed to get.
	lPointCount = NETWORK_ReadShort( pByteStream );

	// If this isn't a valid player, break out.
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	// Finally, set the player's point count, and refresh the HUD.
	players[ulPlayer].lPointCount = lPointCount;
	SCOREBOARD_RefreshHUD( );
}

//*****************************************************************************
//
static void client_SetPlayerWins( BYTESTREAM_s *pByteStream )
{
	ULONG	ulPlayer;
	LONG	lWins;

	// Read in the player whose wins are being updated.
	ulPlayer = NETWORK_ReadByte( pByteStream );

	// Read in the number of wins he's supposed to have.
	lWins = NETWORK_ReadByte( pByteStream );

	// If this is an invalid player, break out.
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	// Finally, set the player's win count, and refresh the HUD.
	players[ulPlayer].ulWins = lWins;
	SCOREBOARD_RefreshHUD( );
}

//*****************************************************************************
//
static void client_SetPlayerKillCount( BYTESTREAM_s *pByteStream )
{
	ULONG	ulPlayer;
	LONG	lKillCount;

	// Read in the player whose kill count being updated.
	ulPlayer = NETWORK_ReadByte( pByteStream );

	// Read in the number of kills he's supposed to have.
	lKillCount = NETWORK_ReadShort( pByteStream );

	// If this is an invalid player, break out.
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	// Finally, set the player's kill count, and refresh the HUD.
	players[ulPlayer].killcount = lKillCount;
	SCOREBOARD_RefreshHUD( );
}

//*****************************************************************************
//
void client_SetPlayerChatStatus( BYTESTREAM_s *pByteStream )
{
	// Read in the player.
	ULONG ulPlayer = NETWORK_ReadByte( pByteStream );
	bool bChatting = !!NETWORK_ReadByte( pByteStream );

	// Ensure that he's valid.
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	// Read and set his chat status.
	players[ulPlayer].bChatting = bChatting;
}

//*****************************************************************************
//
void client_SetPlayerConsoleStatus( BYTESTREAM_s *pByteStream )
{
	// Read in the player.
	ULONG ulPlayer = NETWORK_ReadByte( pByteStream );
	bool bInConsole = !!NETWORK_ReadByte( pByteStream );

	// Ensure that he's valid.
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	// Read and set his "in console" status.
	players[ulPlayer].bInConsole = bInConsole;
}

//*****************************************************************************
//
void client_SetPlayerLaggingStatus( BYTESTREAM_s *pByteStream )
{
	// Read in the player.
	ULONG ulPlayer = NETWORK_ReadByte( pByteStream );
	bool bLagging = !!NETWORK_ReadByte( pByteStream );

	// Ensure that he's valid.
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	// Read and set his lag status.
	players[ulPlayer].bLagging = bLagging;
}

//*****************************************************************************
//
static void client_SetPlayerReadyToGoOnStatus( BYTESTREAM_s *pByteStream )
{
	// Read in the player.
	ULONG ulPlayer = NETWORK_ReadByte( pByteStream );
	bool bReadyToGoOn = !!NETWORK_ReadByte( pByteStream );

	// Ensure that he's valid.
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	// Read and set his "ready to go on" status.
	players[ulPlayer].bReadyToGoOn = bReadyToGoOn;
}

//*****************************************************************************
//
static void client_SetPlayerTeam( BYTESTREAM_s *pByteStream )
{
	ULONG	ulPlayer;
	ULONG	ulTeam;

	// Read in the player having his team set.
	ulPlayer = NETWORK_ReadByte( pByteStream );

	// Read in the player's team.
	ulTeam = NETWORK_ReadByte( pByteStream );

	// If this is an invalid player, break out.
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	// Update the player's team.
	PLAYER_SetTeam( &players[ulPlayer], ulTeam, false );
}

//*****************************************************************************
//
static void client_SetPlayerCamera( BYTESTREAM_s *pByteStream )
{
	AActor	*pCamera;
	LONG	lID;
	bool	bRevertPleaseStatus;

	// Read in the network ID of the camera.
	lID = NETWORK_ReadShort( pByteStream );

	// Read in the "revert please" status.
	bRevertPleaseStatus = !!NETWORK_ReadByte( pByteStream );

	AActor *oldcamera = players[consoleplayer].camera;

	// Find the camera by the network ID.
	pCamera = CLIENT_FindThingByNetID( lID );
	if ( pCamera == NULL )
	{
		players[consoleplayer].camera = players[consoleplayer].mo;
		players[consoleplayer].cheats &= ~CF_REVERTPLEASE;
		if (oldcamera != players[consoleplayer].camera)
			R_ClearPastViewer (players[consoleplayer].camera);
		return;
	}

	// Finally, change the player's camera.
	players[consoleplayer].camera = pCamera;
	if ( bRevertPleaseStatus == false )
		players[consoleplayer].cheats &= ~CF_REVERTPLEASE;
	else
		players[consoleplayer].cheats |= CF_REVERTPLEASE;

	if (oldcamera != players[consoleplayer].camera)
		R_ClearPastViewer (players[consoleplayer].camera);
}

//*****************************************************************************
//
static void client_SetPlayerPoisonCount( BYTESTREAM_s *pByteStream )
{
	ULONG	ulPlayer;
	ULONG	ulPoisonCount;

	// Read in the player being poisoned.
	ulPlayer = NETWORK_ReadByte( pByteStream );

	// Read in the poison count.
	ulPoisonCount = NETWORK_ReadShort( pByteStream );

	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	// Finally, set the player's poison count.
	players[ulPlayer].poisoncount = ulPoisonCount;
}

//*****************************************************************************
//
static void client_SetPlayerAmmoCapacity( BYTESTREAM_s *pByteStream )
{
	ULONG			ulPlayer;
	USHORT			usActorNetworkIndex;
	LONG			lMaxAmount;
	AInventory		*pAmmo;

	// Read in the player ID.
	ulPlayer = NETWORK_ReadByte( pByteStream );

	// Read in the identification of the type of item to give.
	usActorNetworkIndex = NETWORK_ReadShort( pByteStream );

	// Read in the amount of this inventory type the player has.
	lMaxAmount = NETWORK_ReadShort( pByteStream );

	// Check to make sure everything is valid. If not, break out.
	if (( PLAYER_IsValidPlayer( ulPlayer ) == false ) || ( players[ulPlayer].mo == NULL ))
		return;

	// [BB] Remember whether we already had this ammo.
	const bool hadAmmo = ( players[ulPlayer].mo->FindInventory( NETWORK_GetClassFromIdentification( usActorNetworkIndex ) ) != NULL );

	pAmmo = CLIENT_FindPlayerInventory( ulPlayer, NETWORK_GetClassFromIdentification( usActorNetworkIndex ));

	if ( pAmmo == NULL )
		return;

	if ( !(pAmmo->GetClass()->IsDescendantOf (RUNTIME_CLASS(AAmmo))) )
		return;

	// [BB] If we didn't have this kind of ammo yet, CLIENT_FindPlayerInventory gave it to us.
	// In this case make sure that the amount is zero.
	if ( hadAmmo == false )
		pAmmo->Amount = 0;

	// Set the new maximum amount of the inventory object.
	pAmmo->MaxAmount = lMaxAmount;

	// Since an item displayed on the HUD may have been given, refresh the HUD.
	SCOREBOARD_RefreshHUD( );
}

//*****************************************************************************
//
static void client_SetPlayerCheats( BYTESTREAM_s *pByteStream )
{
	ULONG			ulPlayer;
	ULONG			ulCheats;

	// Read in the player ID.
	ulPlayer = NETWORK_ReadByte( pByteStream );

	// Read in the cheats value.
	ulCheats = NETWORK_ReadLong( pByteStream );

	// Check to make sure everything is valid. If not, break out.
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	players[ulPlayer].cheats = ulCheats;
}

//*****************************************************************************
//
static void client_SetPlayerPendingWeapon( BYTESTREAM_s *pByteStream )
{
	ULONG			ulPlayer;
	USHORT			usActorNetworkIndex;
	const PClass	*pType = NULL;
	AWeapon			*pWeapon = NULL;

	// Read in the player whose info is about to be updated.
	ulPlayer = NETWORK_ReadByte( pByteStream );

	// Read in the identification of the weapon.
	usActorNetworkIndex = NETWORK_ReadShort( pByteStream );

	// If the player doesn't exist, get out!
	if ( PLAYER_IsValidPlayerWithMo( ulPlayer ) == false )
		return;

	pType = NETWORK_GetClassFromIdentification( usActorNetworkIndex );
	if (( pType != NULL ) &&
		( pType->IsDescendantOf( RUNTIME_CLASS( AWeapon ))))
	{
		// If we dont have this weapon already, we do now!
		pWeapon = static_cast<AWeapon *>( players[ulPlayer].mo->FindInventory( pType ));
		if ( pWeapon == NULL )
			pWeapon = static_cast<AWeapon *>( players[ulPlayer].mo->GiveInventoryType( pType ));

		// If he still doesn't have the object after trying to give it to him... then YIKES!
		if ( pWeapon == NULL )
		{
#ifdef CLIENT_WARNING_MESSAGES
			Printf( "client_SetPlayerPendingWeapon: Failed to give inventory type, %s!\n", NETWORK_GetClassNameFromIdentification( usActorNetworkIndex ));
#endif
			return;
		}

		players[ulPlayer].PendingWeapon = pWeapon;
	}
}

//*****************************************************************************
//
/* [BB] Does not work with the latest ZDoom changes. Check if it's still necessary.
static void client_SetPlayerPieces( BYTESTREAM_s *pByteStream )
{
	ULONG			ulPlayer;
	ULONG			ulPieces;

	// Read in the player.
	ulPlayer = NETWORK_ReadByte( pByteStream );

	// Read in the player's pieces.
	ulPieces = NETWORK_ReadByte( pByteStream );

	// If the player doesn't exist, get out!
	if ( playeringame[ulPlayer] == false )
		return;

	players[ulPlayer].pieces = ulPieces;
}
*/

//*****************************************************************************
//
static void client_SetPlayerPSprite( BYTESTREAM_s *pByteStream )
{
	ULONG			ulPlayer;
	const char		*pszState;
	LONG			lOffset;
	LONG			lPosition;
	FState			*pNewState;

	// Read in the player.
	ulPlayer = NETWORK_ReadByte( pByteStream );

	// Read in the state.
	pszState = NETWORK_ReadString( pByteStream );

	// Offset into the state label.
	lOffset = NETWORK_ReadByte( pByteStream );

	// Read in the position (ps_weapon, etc.).
	lPosition = NETWORK_ReadByte( pByteStream );

	// Not in a level; nothing to do (shouldn't happen!)
	if ( gamestate != GS_LEVEL )
		return;

	// Check to make sure everything is valid. If not, break out.
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	if ( players[ulPlayer].ReadyWeapon == NULL )
		return;

	// [BB] In this case lOffset is just the offset from the ready state.
	// Handle this accordingly.
	if ( stricmp( pszState, ":R" ) == 0 )
	{
		pNewState = players[ulPlayer].ReadyWeapon->GetReadyState( );
	}
	// [BB] In this case lOffset is just the offset from the flash state.
	else if ( stricmp( pszState, ":F" ) == 0 )
	{
		pNewState = players[ulPlayer].ReadyWeapon->FindState(NAME_Flash);
	}
	else
	{
		// Build the state name list.
		TArray<FName> &StateList = MakeStateNameList( pszState );

		// [BB] Obviously, we can't access StateList[0] if StateList is empty.
		if ( StateList.Size( ) == 0 )
			return;

		pNewState = players[ulPlayer].ReadyWeapon->GetClass( )->ActorInfo->FindState( StateList.Size( ), &StateList[0] );

		// [BB] The offset was calculated by FindStateLabelAndOffset using GetNextState(),
		// so we have to use this here, too, to propely find the target state.
		// Note: The same loop is used in client_SetThingFrame and could be moved to a new function.
		for ( int i = 0; i < lOffset; i++ )
		{
			if ( pNewState != NULL )
				pNewState = pNewState->GetNextState( );
		}

		if ( pNewState == NULL )
			return;

		P_SetPsprite( &players[ulPlayer], lPosition, pNewState );
		return;
	}
	if ( pNewState )
	{
		// [BB] The offset is only guaranteed to work if the actor owns the state.
		if ( lOffset != 0 )
		{
			if ( ActorOwnsState ( players[ulPlayer].ReadyWeapon, pNewState ) == false )
			{
#ifdef CLIENT_WARNING_MESSAGES
				Printf ( "client_SetPlayerPSprite: %s doesn't own %s\n", players[ulPlayer].ReadyWeapon->GetClass()->TypeName.GetChars(), pszState );
#endif
				return;
			}
			if ( ActorOwnsState ( players[ulPlayer].ReadyWeapon, pNewState + lOffset ) == false )
			{
#ifdef CLIENT_WARNING_MESSAGES
				Printf ( "client_SetPlayerPSprite: %s doesn't own %s + %d\n", players[ulPlayer].ReadyWeapon->GetClass()->TypeName.GetChars(), pszState, lOffset );
#endif
				return;
			}
		}
		P_SetPsprite( &players[ulPlayer], lPosition, pNewState + lOffset );
	}
}

//*****************************************************************************
//
static void client_SetPlayerBlend( BYTESTREAM_s *pByteStream )
{
	ULONG			ulPlayer;

	// Read in the player.
	ulPlayer = NETWORK_ReadByte( pByteStream );

	if ( ulPlayer < MAXPLAYERS )
	{
		players[ulPlayer].BlendR = NETWORK_ReadFloat( pByteStream );
		players[ulPlayer].BlendG = NETWORK_ReadFloat( pByteStream );
		players[ulPlayer].BlendB = NETWORK_ReadFloat( pByteStream );
		players[ulPlayer].BlendA = NETWORK_ReadFloat( pByteStream );
	}
	else
	{
		NETWORK_ReadFloat( pByteStream );
		NETWORK_ReadFloat( pByteStream );
		NETWORK_ReadFloat( pByteStream );
		NETWORK_ReadFloat( pByteStream );
	}
}

//*****************************************************************************
//
static void client_SetPlayerMaxHealth( BYTESTREAM_s *pByteStream )
{
	// [BB] Read in the player and the new MaxHealth.
	ULONG ulPlayer = NETWORK_ReadByte( pByteStream );
	LONG lMaxHealth =  NETWORK_ReadLong( pByteStream );

	// [BB] Check to make sure everything is valid. If not, break out.
	if (( PLAYER_IsValidPlayer( ulPlayer ) == false ) || ( players[ulPlayer].mo == NULL ))
		return;

	players[ulPlayer].mo->MaxHealth = lMaxHealth;
}

//*****************************************************************************
//
static void client_SetPlayerLivesLeft( BYTESTREAM_s *pByteStream )
{
	// [BB] Read in the player and the new LivesLeft.
	ULONG ulPlayer = NETWORK_ReadByte( pByteStream );
	ULONG ulLivesLeft =  NETWORK_ReadByte( pByteStream );

	// [BB] Check to make sure everything is valid. If not, break out.
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	players[ulPlayer].ulLivesLeft = ulLivesLeft;
}

//*****************************************************************************
//
static void client_UpdatePlayerPing( BYTESTREAM_s *pByteStream )
{
	ULONG	ulPlayer;
	ULONG	ulPing;

	// Read in the player whose ping is being updated.
	ulPlayer = NETWORK_ReadByte( pByteStream );

	// Read in the player's ping.
	ulPing = NETWORK_ReadShort( pByteStream );

	// If this is an invalid player, break out.
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	// Finally, set the player's ping.
	players[ulPlayer].ulPing = ulPing;
}

//*****************************************************************************
//
static void client_UpdatePlayerExtraData( BYTESTREAM_s *pByteStream )
{
	ULONG	ulPlayer;
//	ULONG	ulPendingWeapon;
//	ULONG	ulReadyWeapon;
	LONG	lPitch;
	ULONG	ulWaterLevel;
	ULONG	ulButtons;
	LONG	lViewZ;
	LONG	lBob;

	// Read in the player who's info is about to be updated.
	ulPlayer = NETWORK_ReadByte( pByteStream );

	// Read in the player's pitch.
	lPitch = NETWORK_ReadLong( pByteStream );

	// Read in the player's water level.
	ulWaterLevel = NETWORK_ReadByte( pByteStream );

	// Read in the player's buttons.
	ulButtons = NETWORK_ReadByte( pByteStream );

	// Read in the view and weapon bob.
	lViewZ = NETWORK_ReadLong( pByteStream );
	lBob = NETWORK_ReadLong( pByteStream );

	// If the player doesn't exist, get out!
	if (( players[ulPlayer].mo == NULL ) || ( playeringame[ulPlayer] == false ))
		return;

	// [BB] If the spectated player uses the GL renderer and we are using software,
	// the viewangle has to be limited.	We don't care about cl_disallowfullpitch here.
	if ( !currentrenderer )
	{
		// [BB] The user can restore ZDoom's freelook limit.
		const fixed_t pitchLimit = -ANGLE_1*( cl_oldfreelooklimit ? 32 : 56 );
		if (lPitch < pitchLimit)
			lPitch = pitchLimit;
		if (lPitch > ANGLE_1*56)
			lPitch = ANGLE_1*56;
	}
	players[ulPlayer].mo->pitch = lPitch;
	players[ulPlayer].mo->waterlevel = ulWaterLevel;
	// [BB] The attack buttons are now already set in *_MovePlayer, so additionally setting
	// them here is obsolete. I don't want to change this before 97D2 final though.
	players[ulPlayer].cmd.ucmd.buttons = ulButtons;
//	players[ulPlayer].velx = lMomX;
//	players[ulPlayer].vely = lMomY;
	players[ulPlayer].viewz = lViewZ;
	players[ulPlayer].bob = lBob;
}

//*****************************************************************************
//
static void client_UpdatePlayerTime( BYTESTREAM_s *pByteStream )
{
	ULONG	ulPlayer;
	ULONG	ulTime;

	// Read in the player.
	ulPlayer = NETWORK_ReadByte( pByteStream );

	// Read in the time.
	ulTime = NETWORK_ReadShort( pByteStream );

	// If this is an invalid player, break out.
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	players[ulPlayer].ulTime = ulTime * ( TICRATE * 60 );
}

//*****************************************************************************
//
static void client_MoveLocalPlayer( BYTESTREAM_s *pByteStream )
{
	player_t	*pPlayer;
	ULONG		ulClientTicOnServerEnd;
	fixed_t		X;
	fixed_t		Y;
	fixed_t		Z;
	fixed_t		MomX;
	fixed_t		MomY;
	fixed_t		MomZ;

	pPlayer = &players[consoleplayer];
	
	// Read in the last tick that we sent to the server.
	ulClientTicOnServerEnd = NETWORK_ReadLong( pByteStream );

	// Get XYZ.
	X = NETWORK_ReadLong( pByteStream );
	Y = NETWORK_ReadLong( pByteStream );
	Z = NETWORK_ReadLong( pByteStream );

	// Get XYZ momentum.
	MomX = NETWORK_ReadLong( pByteStream );
	MomY = NETWORK_ReadLong( pByteStream );
	MomZ = NETWORK_ReadLong( pByteStream );

	// No player object to update.
	if ( pPlayer->mo == NULL )
		return;

	// "ulClientTicOnServerEnd" is the gametic of the last time we sent a movement command.
	CLIENT_SetLastConsolePlayerUpdateTick( ulClientTicOnServerEnd );

	// If the last time the server heard from us exceeds one second, the client is lagging!
	// [BB] But don't think we are lagging immediately after receiving a full update.
	if (( gametic - CLIENTDEMO_GetGameticOffset( ) - ulClientTicOnServerEnd >= TICRATE ) && (( gametic + CLIENTDEMO_GetGameticOffset( ) - g_ulEndFullUpdateTic ) > TICRATE ))
		g_bClientLagging = true;
	else
		g_bClientLagging = false;

	// If the player is dead, simply ignore this (remember, this could be parsed from an
	// out-of-order packet, since it's sent unreliably)!
	if ( pPlayer->playerstate == PST_DEAD )
		return;

	// Now that everything's check out, update stuff.
	if ( pPlayer->bSpectating == false )
	{
		pPlayer->ServerXYZ[0] = X;
		pPlayer->ServerXYZ[1] = Y;
		pPlayer->ServerXYZ[2] = Z;

		pPlayer->ServerXYZMom[0] = MomX;
		pPlayer->ServerXYZMom[1] = MomY;
		pPlayer->ServerXYZMom[2] = MomZ;
	}
	else
	{
		// [BB] Calling CLIENT_MoveThing instead of setting the x,y,z position directly should make
		// sure that the spectator body doesn't get stuck.
		CLIENT_MoveThing ( pPlayer->mo, X, Y, Z );

		pPlayer->mo->velx = MomX;
		pPlayer->mo->vely = MomY;
		pPlayer->mo->velz = MomZ;
	}
}

//*****************************************************************************
//
void client_DisconnectPlayer( BYTESTREAM_s *pByteStream )
{
	ULONG	ulPlayer;

	// Read in the player who's being disconnected (could be us!).
	ulPlayer = NETWORK_ReadByte( pByteStream );

	// If this is an invalid player, break out.
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	// If we were a spectator and looking through this player's eyes, revert them.
	if ( players[ulPlayer].mo->CheckLocalView( consoleplayer ))
	{
		CLIENT_ResetConsolePlayerCamera( );
	}

	// Create a little disconnect particle effect thingamabobber!
	// [BB] Only do this if a non-spectator disconnects.
	if ( players[ulPlayer].bSpectating == false )
	{
		P_DisconnectEffect( players[ulPlayer].mo );

		// [BB] Stop all CLIENTSIDE scripts of the player that are still running.
		if ( !( zacompatflags & ZACOMPATF_DONT_STOP_PLAYER_SCRIPTS_ON_DISCONNECT ) )
			FBehavior::StaticStopMyScripts ( players[ulPlayer].mo );
	}

	// Destroy the actor associated with the player.
	if ( players[ulPlayer].mo )
	{
		players[ulPlayer].mo->Destroy( );
		players[ulPlayer].mo = NULL;
	}

	playeringame[ulPlayer] = false;

	// Zero out all the player information.
	PLAYER_ResetPlayerData( &players[ulPlayer] );

	// Refresh the HUD because this affects the number of players in the game.
	SCOREBOARD_RefreshHUD( );
}

//*****************************************************************************
//
static void client_SetConsolePlayer( BYTESTREAM_s *pByteStream )
{
	LONG	lConsolePlayer;

	// Read in what our local player index is.
	lConsolePlayer = NETWORK_ReadByte( pByteStream );

	// If this index is invalid, break out.
	if ( lConsolePlayer >= MAXPLAYERS )
		return;

	// In a client demo, don't lose the userinfo we gave to our console player.
	if ( CLIENTDEMO_IsPlaying( ))
		memcpy( &players[lConsolePlayer].userinfo, &players[consoleplayer].userinfo, sizeof( userinfo_t ));

	// Otherwise, since it's valid, set our local player index to this.
	consoleplayer = lConsolePlayer;

	// Finally, apply our local userinfo to this player slot.
	D_SetupUserInfo( );
}

//*****************************************************************************
//
static void client_ConsolePlayerKicked( BYTESTREAM_s *pByteStream )
{
	// Set the connection state to "disconnected" before calling CLIENT_QuitNetworkGame( ),
	// so that we don't send a disconnect signal to the server.
	CLIENT_SetConnectionState( CTS_DISCONNECTED );

	// End the network game.
	CLIENT_QuitNetworkGame( "You have been kicked from the server.\n" );
}

//*****************************************************************************
//
static void client_GivePlayerMedal( BYTESTREAM_s *pByteStream )
{
	ULONG	ulPlayer;
	ULONG	ulMedal;

	// Read in the player to award the medal to.
	ulPlayer = NETWORK_ReadByte( pByteStream );
	
	// Read in the medal to be awarded.
	ulMedal = NETWORK_ReadByte( pByteStream );

	// If this is an invalid player, break out.
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	// Award the medal.
	MEDAL_GiveMedal( ulPlayer, ulMedal );
}

//*****************************************************************************
//
static void client_ResetAllPlayersFragcount( BYTESTREAM_s *pByteStream )
{
	// This function pretty much takes care of everything we need to do!
	PLAYER_ResetAllPlayersFragcount( );
}

//*****************************************************************************
//
static void client_PlayerIsSpectator( BYTESTREAM_s *pByteStream )
{
	ULONG	ulPlayer;
	bool	bDeadSpectator;

	// Read in the player who's going to be spectating.
	ulPlayer = NETWORK_ReadByte( pByteStream );

	// Read in if he's becoming a dead spectator.
	bDeadSpectator = !!NETWORK_ReadByte( pByteStream );

	// If this is an invalid player, break out.
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	// [BB] If the player turns into a true spectator, create the disconnect particle effect.
	// Note: We have to do this before the player is actually turned into a spectator, because
	// the tiny height of a spectator's body would alter the effect.
	if ( ( bDeadSpectator == false ) && players[ulPlayer].mo )
		P_DisconnectEffect( players[ulPlayer].mo );

	// Make the player a spectator.
	PLAYER_SetSpectator( &players[ulPlayer], false, bDeadSpectator );

	// If we were a spectator and looking through this player's eyes, revert them.
	if ( players[ulPlayer].mo->CheckLocalView( consoleplayer ))
	{
		CLIENT_ResetConsolePlayerCamera();
	}

	// Don't lag anymore if we're a spectator.
	if ( ulPlayer == static_cast<ULONG>(consoleplayer) )
		g_bClientLagging = false;
}

//*****************************************************************************
//
static void client_PlayerSay( BYTESTREAM_s *pByteStream )
{
	ULONG		ulPlayer;
	ULONG		ulMode;
	const char	*pszString;

	// Read in the player who's supposed to be talking.
	ulPlayer = NETWORK_ReadByte( pByteStream );

	// Read in the chat mode. Could be global, team-only, etc.
	ulMode = NETWORK_ReadByte( pByteStream );

	// Read in the actual chat string.
	pszString = NETWORK_ReadString( pByteStream );

	// If ulPlayer == MAXPLAYERS, that means the server is talking.
	if ( ulPlayer != MAXPLAYERS )
	{
		// If this is an invalid player, break out.
		if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
			return;
	}

	// Finally, print out the chat string.
	CHAT_PrintChatString( ulPlayer, ulMode, pszString );
}

//*****************************************************************************
//
static void client_PlayerTaunt( BYTESTREAM_s *pByteStream )
{
	ULONG	ulPlayer;

	// Read in the player index who's taunting.
	ulPlayer = NETWORK_ReadByte( pByteStream );

	// Don't taunt if we're not in a level!
	if ( gamestate != GS_LEVEL )
		return;

	// If this is an invalid player, break out.
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	if (( players[ulPlayer].bSpectating ) ||
		( players[ulPlayer].health <= 0 ) ||
		( players[ulPlayer].mo == NULL ) ||
		( zacompatflags & ZACOMPATF_DISABLETAUNTS ))
	{
		return;
	}

	// Play the taunt sound!
	if ( cl_taunts )
		S_Sound( players[ulPlayer].mo, CHAN_VOICE, "*taunt", 1, ATTN_NORM );
}

//*****************************************************************************
//
static void client_PlayerRespawnInvulnerability( BYTESTREAM_s *pByteStream )
{
	ULONG		ulPlayer;
	AActor		*pActor;
	AInventory	*pInventory;

	// Read in the player index who has respawn invulnerability.
	ulPlayer = NETWORK_ReadByte( pByteStream );

	// Don't taunt if we're not in a level!
	if ( gamestate != GS_LEVEL )
		return;

	// If this is an invalid player, break out.
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	// We can't apply the effect to the player's body if the player's body doesn't exist!
	pActor = players[ulPlayer].mo;
	if ( pActor == NULL )
		return;

	// First, we need to adjust the blend color, so the player's screen doesn't go white.
	pInventory = pActor->FindInventory( RUNTIME_CLASS( APowerInvulnerable ));
	if ( pInventory == NULL )
		return;

	static_cast<APowerup *>( pInventory )->BlendColor = 0;

	// Apply respawn invulnerability effect.
	switch ( cl_respawninvuleffect )
	{
	case 1:

		pActor->RenderStyle = STYLE_Translucent;
		pActor->effects |= FX_VISIBILITYFLICKER;
		break;
	case 2:

		pActor->effects |= FX_RESPAWNINVUL;
		break;
	}
}

//*****************************************************************************
//
static void client_PlayerUseInventory( BYTESTREAM_s *pByteStream )
{
	ULONG			ulPlayer;
	USHORT			usActorNetworkIndex;
	const PClass	*pType;
	AInventory		*pInventory;

	// Read in the player using an inventory item.
	ulPlayer = NETWORK_ReadByte( pByteStream );

	// Read the name of the inventory item we shall use.
	usActorNetworkIndex = NETWORK_ReadShort( pByteStream );

	// Check to make sure everything is valid. If not, break out.
	if (( PLAYER_IsValidPlayer( ulPlayer ) == false ) || ( players[ulPlayer].mo == NULL ))
		return;

	pType = NETWORK_GetClassFromIdentification( usActorNetworkIndex );
	if ( pType == NULL )
		return;

	// Try to find this object within the player's personal inventory.
	pInventory = players[ulPlayer].mo->FindInventory( pType );

	// If the player doesn't have this type, give it to him.
	if ( pInventory == NULL )
		pInventory = players[ulPlayer].mo->GiveInventoryType( pType );

	// If he still doesn't have the object after trying to give it to him... then YIKES!
	if ( pInventory == NULL )
	{
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_PlayerUseInventory: Failed to give inventory type, %s!\n", NETWORK_GetClassNameFromIdentification( usActorNetworkIndex ));
#endif
		return;
	}

	// Finally, use the item.
	const bool bSuccess = players[ulPlayer].mo->UseInventory( pInventory );

	// [BB] The server only instructs the client to use the item, if the use was successful.
	// So using it on the client also should be successful. If it is not, something went wrong,
	// e.g. the use has a A_JumpIfHealthLower(100,"Success") check and a successful use heals the
	// player such that the check will fail after the item was used. Since the server informs the
	// client about the effects of the use before it tells the client to use the item, the use
	// on the client will fail. In this case at least reduce the inventory amount according to a
	// successful use.
	if ( ( bSuccess == false ) && !( dmflags2 & DF2_INFINITE_INVENTORY ) )
	{
		if (--pInventory->Amount <= 0 && !(pInventory->ItemFlags & IF_KEEPDEPLETED))
		{
			pInventory->Destroy ();
		}
	}
}

//*****************************************************************************
//
static void client_PlayerDropInventory( BYTESTREAM_s *pByteStream )
{
	ULONG			ulPlayer;
	USHORT			usActorNetworkIndex;
	const PClass	*pType;
	AInventory		*pInventory;

	// Read in the player dropping an inventory item.
	ulPlayer = NETWORK_ReadByte( pByteStream );

	// Read the identification of the inventory item we shall drop.
	usActorNetworkIndex = NETWORK_ReadShort( pByteStream );

	// Check to make sure everything is valid. If not, break out.
	if (( PLAYER_IsValidPlayer( ulPlayer ) == false ) || ( players[ulPlayer].mo == NULL ))
		return;

	pType = NETWORK_GetClassFromIdentification( usActorNetworkIndex );
	if ( pType == NULL )
		return;

	// Try to find this object within the player's personal inventory.
	pInventory = players[ulPlayer].mo->FindInventory( pType );

	// If the player doesn't have this type, give it to him.
	if ( pInventory == NULL )
		pInventory = players[ulPlayer].mo->GiveInventoryType( pType );

	// If he still doesn't have the object after trying to give it to him... then YIKES!
	if ( pInventory == NULL )
	{
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_PlayerDropInventory: Failed to give inventory type, %s!\n", NETWORK_GetClassNameFromIdentification( usActorNetworkIndex ));
#endif
		return;
	}

	// Finally, drop the item.
	players[ulPlayer].mo->DropInventory( pInventory );
}

//*****************************************************************************
//
static void client_SpawnThing( BYTESTREAM_s *pByteStream )
{
	fixed_t			X;
	fixed_t			Y;
	fixed_t			Z;
	USHORT			usActorNetworkIndex;
	LONG			lID;

	// Read in the XYZ location of the item.
	X = NETWORK_ReadShort( pByteStream ) << FRACBITS;
	Y = NETWORK_ReadShort( pByteStream ) << FRACBITS;
	Z = NETWORK_ReadShort( pByteStream ) << FRACBITS;

	// Read in the identification of the item class.
	usActorNetworkIndex = NETWORK_ReadShort( pByteStream );

	// Read in the network ID of the item.
	lID = NETWORK_ReadShort( pByteStream );

	// Finally, spawn the thing.
	CLIENT_SpawnThing( NETWORK_GetClassFromIdentification( usActorNetworkIndex ), X, Y, Z, lID );
}

//*****************************************************************************
//
static void client_SpawnThingNoNetID( BYTESTREAM_s *pByteStream )
{
	fixed_t			X;
	fixed_t			Y;
	fixed_t			Z;
	USHORT			usActorNetworkIndex;

	// Read in the XYZ location of the item.
	X = NETWORK_ReadShort( pByteStream ) << FRACBITS;
	Y = NETWORK_ReadShort( pByteStream ) << FRACBITS;
	Z = NETWORK_ReadShort( pByteStream ) << FRACBITS;

	// Read in the identification of the item class.
	usActorNetworkIndex = NETWORK_ReadShort( pByteStream );

	// Finally, spawn the thing.
	CLIENT_SpawnThing( NETWORK_GetClassFromIdentification( usActorNetworkIndex ), X, Y, Z, -1 );
}

//*****************************************************************************
//
static void client_SpawnThingExact( BYTESTREAM_s *pByteStream )
{
	fixed_t			X;
	fixed_t			Y;
	fixed_t			Z;
	USHORT			usActorNetworkIndex;
	LONG			lID;

	// Read in the XYZ location of the item.
	X = NETWORK_ReadLong( pByteStream );
	Y = NETWORK_ReadLong( pByteStream );
	Z = NETWORK_ReadLong( pByteStream );

	// Read in the identification of the item class.
	usActorNetworkIndex = NETWORK_ReadShort( pByteStream );

	// Read in the network ID of the item.
	lID = NETWORK_ReadShort( pByteStream );

	// Finally, spawn the thing.
	CLIENT_SpawnThing( NETWORK_GetClassFromIdentification( usActorNetworkIndex ), X, Y, Z, lID );
}

//*****************************************************************************
//
static void client_SpawnThingExactNoNetID( BYTESTREAM_s *pByteStream )
{
	fixed_t			X;
	fixed_t			Y;
	fixed_t			Z;
	USHORT			usActorNetworkIndex;

	// Read in the XYZ location of the item.
	X = NETWORK_ReadLong( pByteStream );
	Y = NETWORK_ReadLong( pByteStream );
	Z = NETWORK_ReadLong( pByteStream );

	// Read in the identification of the item class.
	usActorNetworkIndex = NETWORK_ReadShort( pByteStream );

	// Finally, spawn the thing.
	CLIENT_SpawnThing( NETWORK_GetClassFromIdentification( usActorNetworkIndex ), X, Y, Z, -1 );
}

//*****************************************************************************
//
static void client_MoveThing( BYTESTREAM_s *pByteStream )
{
	LONG	lID;
	LONG	lBits;
	AActor	*pActor;
	fixed_t	X;
	fixed_t	Y;
	fixed_t	Z;

	// Read in the network ID of the thing to update.
	lID = NETWORK_ReadShort( pByteStream );

	// Read in the data that will be updated.
	lBits = NETWORK_ReadShort( pByteStream );

	// Try to find the corresponding actor.
	pActor = CLIENT_FindThingByNetID( lID );

	if (( pActor == NULL ) || gamestate != GS_LEVEL )
	{
		// No thing up update; skip the rest of the message.
		if ( lBits & CM_X ) NETWORK_ReadShort( pByteStream );
		if ( lBits & CM_Y ) NETWORK_ReadShort( pByteStream );
		if ( lBits & CM_Z ) NETWORK_ReadShort( pByteStream );
		if ( lBits & CM_LAST_X ) NETWORK_ReadShort( pByteStream );
		if ( lBits & CM_LAST_Y ) NETWORK_ReadShort( pByteStream );
		if ( lBits & CM_LAST_Z ) NETWORK_ReadShort( pByteStream );
		if ( lBits & CM_ANGLE ) NETWORK_ReadLong( pByteStream );
		if ( lBits & CM_MOMX ) NETWORK_ReadShort( pByteStream );
		if ( lBits & CM_MOMY ) NETWORK_ReadShort( pByteStream );
		if ( lBits & CM_MOMZ ) NETWORK_ReadShort( pByteStream );
		if ( lBits & CM_PITCH ) NETWORK_ReadLong( pByteStream );
		if ( lBits & CM_MOVEDIR ) NETWORK_ReadByte( pByteStream );

		return;
	}

	X = pActor->x;
	Y = pActor->y;
	Z = pActor->z;

	// Read in the position data.
	if ( lBits & CM_X )
	{
		X = NETWORK_ReadShort( pByteStream ) << FRACBITS;
		if ( !(lBits & CM_NOLAST) )
			pActor->lastX = X;
	}
	if ( lBits & CM_Y )
	{
		Y = NETWORK_ReadShort( pByteStream ) << FRACBITS;
		if ( !(lBits & CM_NOLAST) )
			pActor->lastY = Y;
	}
	if ( lBits & CM_Z )
	{
		Z = NETWORK_ReadShort( pByteStream ) << FRACBITS;
		if ( !(lBits & CM_NOLAST) )
			pActor->lastZ = Z;
	}

	// Read in the last position data.
	if ( lBits & CM_LAST_X )
		pActor->lastX = NETWORK_ReadShort( pByteStream ) << FRACBITS;
	if ( lBits & CM_LAST_Y )
		pActor->lastY = NETWORK_ReadShort( pByteStream ) << FRACBITS;
	if ( lBits & CM_LAST_Z )
		pActor->lastZ = NETWORK_ReadShort( pByteStream ) << FRACBITS;

	// [WS] Clients will reuse their last updated position.
	if ( lBits & CM_REUSE_X )
		X = pActor->lastX;
	if ( lBits & CM_REUSE_Y )
		Y = pActor->lastY;
	if ( lBits & CM_REUSE_Z )
		Z = pActor->lastZ;

	// Update the thing's position.
	if ( lBits & (CM_X|CM_Y|CM_Z|CM_REUSE_X|CM_REUSE_Y|CM_REUSE_Z) )
		CLIENT_MoveThing( pActor, X, Y, Z );

	// Read in the angle data.
	if ( lBits & CM_ANGLE )
		pActor->angle = NETWORK_ReadLong( pByteStream );

	// Read in the momentum data.
	if ( lBits & CM_MOMX )
		pActor->velx = NETWORK_ReadShort( pByteStream ) << FRACBITS;
	if ( lBits & CM_MOMY )
		pActor->vely = NETWORK_ReadShort( pByteStream ) << FRACBITS;
	if ( lBits & CM_MOMZ )
		pActor->velz = NETWORK_ReadShort( pByteStream ) << FRACBITS;

	// [Dusk] if the actor that's being moved is a player and his momentum
	// is being zeroed (i.e. we're stopping him), we need to stop his bobbing
	// as well.
	if ((pActor->player != NULL) && (pActor->velx == 0) && (pActor->vely == 0)) {
		pActor->player->velx = 0;
		pActor->player->vely = 0;
	}

	// Read in the pitch data.
	if ( lBits & CM_PITCH )
		pActor->pitch = NETWORK_ReadLong( pByteStream );

	// Read in the movedir data.
	if ( lBits & CM_MOVEDIR )
		pActor->movedir = NETWORK_ReadByte( pByteStream );

	// If the server is moving us, don't let our prediction get messed up.
	if ( pActor == players[consoleplayer].mo )
	{
		players[consoleplayer].ServerXYZ[0] = X;
		players[consoleplayer].ServerXYZ[1] = Y;
		players[consoleplayer].ServerXYZ[2] = Z;
		CLIENT_PREDICT_PlayerTeleported( );
	}
}

//*****************************************************************************
//
static void client_MoveThingExact( BYTESTREAM_s *pByteStream )
{
	LONG	lID;
	LONG	lBits;
	AActor	*pActor;
	fixed_t	X;
	fixed_t	Y;
	fixed_t	Z;

	// Read in the network ID of the thing to update.
	lID = NETWORK_ReadShort( pByteStream );

	// Read in the data that will be updated.
	lBits = NETWORK_ReadShort( pByteStream );

	// Try to find the corresponding actor.
	pActor = CLIENT_FindThingByNetID( lID );

	if (( pActor == NULL ) || gamestate != GS_LEVEL )
	{
		// No thing up update; skip the rest of the message.
		if ( lBits & CM_X ) NETWORK_ReadLong( pByteStream );
		if ( lBits & CM_Y ) NETWORK_ReadLong( pByteStream );
		if ( lBits & CM_Z ) NETWORK_ReadLong( pByteStream );
		if ( lBits & CM_LAST_X ) NETWORK_ReadLong( pByteStream );
		if ( lBits & CM_LAST_Y ) NETWORK_ReadLong( pByteStream );
		if ( lBits & CM_LAST_Z ) NETWORK_ReadLong( pByteStream );
		if ( lBits & CM_ANGLE ) NETWORK_ReadLong( pByteStream );
		if ( lBits & CM_MOMX ) NETWORK_ReadLong( pByteStream );
		if ( lBits & CM_MOMY ) NETWORK_ReadLong( pByteStream );
		if ( lBits & CM_MOMZ ) NETWORK_ReadLong( pByteStream );
		if ( lBits & CM_PITCH ) NETWORK_ReadLong( pByteStream );
		if ( lBits & CM_MOVEDIR ) NETWORK_ReadByte( pByteStream );

		return;
	}

	X = pActor->x;
	Y = pActor->y;
	Z = pActor->z;

	// Read in the position data.
	if ( lBits & CM_X )
	{
		X = NETWORK_ReadLong( pByteStream );
		if ( !(lBits & CM_NOLAST) )
			pActor->lastX = X;
	}
	if ( lBits & CM_Y )
	{
		Y = NETWORK_ReadLong( pByteStream );
		if ( !(lBits & CM_NOLAST) )
			pActor->lastY = Y;
	}
	if ( lBits & CM_Z )
	{
		Z = NETWORK_ReadLong( pByteStream );
		if ( !(lBits & CM_NOLAST) )
			pActor->lastZ = Z;
	}

	// Read in the last position data.
	if ( lBits & CM_LAST_X )
		pActor->lastX = NETWORK_ReadLong( pByteStream );
	if ( lBits & CM_LAST_Y )
		pActor->lastY = NETWORK_ReadLong( pByteStream );
	if ( lBits & CM_LAST_Z )
		pActor->lastZ = NETWORK_ReadLong( pByteStream );

	// [WS] Clients will reuse their last updated position.
	if ( lBits & CM_REUSE_X )
		X = pActor->lastX;
	if ( lBits & CM_REUSE_Y )
		Y = pActor->lastY;
	if ( lBits & CM_REUSE_Z )
		Z = pActor->lastZ;

	// Update the thing's position.
	if ( lBits & (CM_X|CM_Y|CM_Z|CM_REUSE_X|CM_REUSE_Y|CM_REUSE_Z) )
		CLIENT_MoveThing( pActor, X, Y, Z );

	// Read in the angle data.
	if ( lBits & CM_ANGLE )
		pActor->angle = NETWORK_ReadLong( pByteStream );

	// Read in the momentum data.
	if ( lBits & CM_MOMX )
		pActor->velx = NETWORK_ReadLong( pByteStream );
	if ( lBits & CM_MOMY )
		pActor->vely = NETWORK_ReadLong( pByteStream );
	if ( lBits & CM_MOMZ )
		pActor->velz = NETWORK_ReadLong( pByteStream );

	// [Dusk] if the actor that's being moved is a player and his momentum
	// is being zeroed (i.e. we're stopping him), we need to stop his bobbing
	// as well.
	if ((pActor->player != NULL) && (pActor->velx == 0) && (pActor->vely == 0)) {
		pActor->player->velx = 0;
		pActor->player->vely = 0;
	}

	// Read in the pitch data.
	if ( lBits & CM_PITCH )
		pActor->pitch = NETWORK_ReadLong( pByteStream );

	// Read in the movedir data.
	if ( lBits & CM_MOVEDIR )
		pActor->movedir = NETWORK_ReadByte( pByteStream );
}

//*****************************************************************************
//
static void client_DamageThing( BYTESTREAM_s *pByteStream )
{
	LONG		lID;
	AActor		*pActor;

	// Read in ID of the thing being damaged.
	lID = NETWORK_ReadShort( pByteStream );

	// Not in a level; nothing to do!
	if ( gamestate != GS_LEVEL )
		return;

	pActor = CLIENT_FindThingByNetID( lID );

	// Nothing to damage.
	if ( pActor == NULL )
	{
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_DamageThing: Couldn't find thing: %d\n", lID );
#endif
		return;
	}

	// Damage the thing.
	P_DamageMobj( pActor, NULL, NULL, 0, NAME_None );
}

//*****************************************************************************
//
static void client_KillThing( BYTESTREAM_s *pByteStream )
{
	LONG		lID;
	LONG		lHealth;
	FName		DamageType;
	LONG		lSourceID;
	LONG		lInflictorID;
	AActor		*pActor;
	AActor		*pSource;
	AActor		*pInflictor;

	// Read in the network ID of the thing that died.
	lID = NETWORK_ReadShort( pByteStream );

	// Read in the thing's health.
	lHealth = NETWORK_ReadShort( pByteStream );

	// Read in the thing's damage type.
	DamageType = NETWORK_ReadString( pByteStream );

	// Read in the actor that killed the player.Thi
	lSourceID = NETWORK_ReadShort( pByteStream );

	// Read in the network ID of the inflictor.
	lInflictorID = NETWORK_ReadShort( pByteStream );

	// Level not loaded; ingore.
	if ( gamestate != GS_LEVEL )
		return;

	// Find the actor that matches the given network ID.
	pActor = CLIENT_FindThingByNetID( lID );
	if ( pActor == NULL )
	{
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_KillThing: Couldn't find thing: %d\n", lID );
#endif
		return;
	}

	// Find the actor associated with the source. It's okay if this actor does not exist.
	if ( lSourceID != -1 )
		pSource = CLIENT_FindThingByNetID( lSourceID );
	else
		pSource = NULL;

	// Find the actor associated with the inflictor. It's okay if this actor does not exist.
	if ( lInflictorID != -1 )
		pInflictor = CLIENT_FindThingByNetID( lInflictorID );
	else
		pInflictor = NULL;

	// Set the thing's health. This should enable the proper death state to play.
	pActor->health = lHealth;

	// Set the thing's damage type.
	pActor->DamageType = DamageType;

	// Kill the thing.
	pActor->Die( pSource, pInflictor );
}

//*****************************************************************************
//
static void client_SetThingState( BYTESTREAM_s *pByteStream )
{
	AActor		*pActor;
	LONG		lID;
	LONG		lState;
	FState		*pNewState = NULL;

	// Read in the network ID for the object to have its state changed.
	lID = NETWORK_ReadShort( pByteStream );

	// Read in the state.
	lState = NETWORK_ReadByte( pByteStream );

	// Not in a level; nothing to do (shouldn't happen!)
	if ( gamestate != GS_LEVEL )
		return;

	// Find the actor associated with the ID.
	pActor = CLIENT_FindThingByNetID( lID );
	if ( pActor == NULL )
	{
		// There should probably be the potential for a warning message here.
		return;
	}

	switch ( lState )
	{
	case STATE_SEE:

		pNewState = pActor->SeeState;
		break;
	case STATE_SPAWN:

		pNewState = pActor->SpawnState;
		break;
	case STATE_PAIN:

		pNewState = pActor->FindState(NAME_Pain);
		break;
	case STATE_MELEE:

		pNewState = pActor->MeleeState;
		break;
	case STATE_MISSILE:

		pNewState = pActor->MissileState;
		break;
	case STATE_DEATH:

		pNewState = pActor->FindState(NAME_Death);
		break;
	case STATE_XDEATH:

		pNewState = pActor->FindState(NAME_XDeath);
		break;
	case STATE_RAISE:

		// When an actor raises, we need to do a whole bunch of other stuff.
		P_Thing_Raise( pActor, true );
		return;
	case STATE_HEAL:

		// [BB] The monster count is increased when STATE_RAISE is set, so
		// don't do it here.
		if ( pActor->FindState(NAME_Heal) )
		{
			pNewState = pActor->FindState(NAME_Heal);
			S_Sound( pActor, CHAN_BODY, "vile/raise", 1, ATTN_IDLE );
		}
		else if ( pActor->IsKindOf( PClass::FindClass("Archvile")))
		{
			const PClass *archvile = PClass::FindClass("Archvile");
			if (archvile != NULL)
			{
				pNewState = archvile->ActorInfo->FindState(NAME_Heal);
			}
			S_Sound( pActor, CHAN_BODY, "vile/raise", 1, ATTN_IDLE );
		}
		else
			return;

		break;

	case STATE_IDLE:

		pActor->SetIdle();
		return;

	// [Dusk]
	case STATE_WOUND:

		pNewState = pActor->FindState( NAME_Wound );
		break;
	default:

#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_SetThingState: Unknown state: %d\n", lState );
#endif
		return; 
	}

	// [BB] We don't allow pNewState == NULL here. This function should set a state
	// not destroy the thing, which happens if you call SetState with NULL as argument.
	if( pNewState == NULL )
		return;
	// Set the angle.
//	pActor->angle = Angle;
	pActor->SetState( pNewState );
//	pActor->SetStateNF( pNewState );
}

//*****************************************************************************
//
static void client_SetThingTarget( BYTESTREAM_s *pByteStream )
{
	AActor		*pActor;
	AActor		*pTarget;
	LONG		lID;
	LONG		lTargetID;

	// Read in the network ID for the object to have its target changed.
	lID = NETWORK_ReadShort( pByteStream );

	// Read in the network ID of the new target.
	lTargetID = NETWORK_ReadShort( pByteStream );

	// Find the actor associated with the ID.
	pActor = CLIENT_FindThingByNetID( lID );
	if ( pActor == NULL )
	{
		// There should probably be the potential for a warning message here.
		return;
	}

	// Find the target associated with the ID.
	pTarget = CLIENT_FindThingByNetID( lTargetID );
	if ( pTarget == NULL )
	{
		// There should probably be the potential for a warning message here.
		return;
	}

	pActor->target = pTarget;
}

//*****************************************************************************
//
static void client_DestroyThing( BYTESTREAM_s *pByteStream )
{
	AActor	*pActor;
	LONG	lID;

	// Read the actor's network ID.
	lID = NETWORK_ReadShort( pByteStream );

	// Find the actor based on the net ID.
	pActor = CLIENT_FindThingByNetID( lID );
	if ( pActor == NULL )
	{
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_DestroyThing: Couldn't find thing: %d\n", lID );
#endif
		return;
	}

	// [BB] If we spied the actor we are supposed to destory, reset our camera.
	if ( pActor->CheckLocalView( consoleplayer ) )
		CLIENT_ResetConsolePlayerCamera( );

	// [BB] If we are destroying a player's body here, we must NULL the corresponding pointer.
	if ( pActor->player && ( pActor->player->mo == pActor ) )
	{
		// [BB] We also have to stop all its associated CLIENTSIDE scripts. Otherwise 
		// they would get disassociated and continue to run even if the player disconnects later.
		if ( !( zacompatflags & ZACOMPATF_DONT_STOP_PLAYER_SCRIPTS_ON_DISCONNECT ) )
			FBehavior::StaticStopMyScripts ( pActor->player->mo );

		pActor->player->mo = NULL;
	}

	// Destroy the thing.
	pActor->Destroy( );
}

//*****************************************************************************
//
static void client_SetThingAngle( BYTESTREAM_s *pByteStream )
{
	AActor		*pActor;
	LONG		lID;
	fixed_t		Angle;

	// Read in the thing's network ID.
	lID = NETWORK_ReadShort( pByteStream );

	// Read in the thing's new angle.
	Angle = NETWORK_ReadShort( pByteStream ) << FRACBITS;

	// Now try to find the thing.
	pActor = CLIENT_FindThingByNetID( lID );
	if ( pActor == NULL )
	{
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_SetThingAngle: Couldn't find thing: %d\n", lID );
#endif
		return;
	}

	// Finally, set the angle.
	pActor->angle = Angle;
}

//*****************************************************************************
//
static void client_SetThingAngleExact( BYTESTREAM_s *pByteStream )
{
	AActor		*pActor;
	LONG		lID;
	fixed_t		Angle;

	// Read in the thing's network ID.
	lID = NETWORK_ReadShort( pByteStream );

	// Read in the thing's new angle.
	Angle = NETWORK_ReadLong( pByteStream );

	// Now try to find the thing.
	pActor = CLIENT_FindThingByNetID( lID );
	if ( pActor == NULL )
	{
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_SetThingAngleExact: Couldn't find thing: %d\n", lID );
#endif
		return;
	}

	// Finally, set the angle.
	pActor->angle = Angle;
}

//*****************************************************************************
//
static void client_SetThingWaterLevel( BYTESTREAM_s *pByteStream )
{
	LONG	lID;
	AActor	*pActor;
	LONG	lWaterLevel;

	// Get the ID of the actor whose water level is being updated.
	lID = NETWORK_ReadShort( pByteStream );

	// Read in the water level.
	lWaterLevel = NETWORK_ReadByte( pByteStream );

	// Now try to find the corresponding actor.
	pActor = CLIENT_FindThingByNetID( lID );
	if ( pActor == NULL )
	{
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_SetThingWaterLevel: Couldn't find thing: %d\n", lID );
#endif
		return;
	}

	pActor->waterlevel = lWaterLevel;
}

//*****************************************************************************
//
static void client_SetThingFlags( BYTESTREAM_s *pByteStream )
{
	LONG	lID;
	AActor	*pActor;
	ULONG	ulFlagSet;
	ULONG	ulFlags;

	// Get the ID of the actor whose flags are being updated.
	lID = NETWORK_ReadShort( pByteStream );

	// Read in the which flags are being updated.
	ulFlagSet = NETWORK_ReadByte( pByteStream );

	// Read in the flags.
	ulFlags = NETWORK_ReadLong( pByteStream );

	// Now try to find the corresponding actor.
	pActor = CLIENT_FindThingByNetID( lID );
	if ( pActor == NULL )
	{
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_SetThingFlags: Couldn't find thing: %d\n", lID );
#endif
		return;
	}

	switch ( ulFlagSet )
	{
	case FLAGSET_FLAGS:
		{
			// [BB] Before adding MF_NOBLOCKMAP, we have to unlink the actor from all blocks.
			const bool relinkActor = ( ( ulFlags & MF_NOBLOCKMAP ) && ( ( pActor->flags & MF_NOBLOCKMAP ) == false ) );
			// [BB] Unlink based on the old flags.
			if ( relinkActor )
				pActor->UnlinkFromWorld ();

			pActor->flags = ulFlags;

			// [BB] Link based on the new flags.
			if ( relinkActor )
				pActor->LinkToWorld ();
		}
		break;
	case FLAGSET_FLAGS2:

		pActor->flags2 = ulFlags;
		break;
	case FLAGSET_FLAGS3:

		pActor->flags3 = ulFlags;
		break;
	case FLAGSET_FLAGS4:

		pActor->flags4 = ulFlags;
		break;
	case FLAGSET_FLAGS5:

		pActor->flags5 = ulFlags;
		break;
	case FLAGSET_FLAGSST:

		pActor->ulSTFlags = ulFlags;
		break;
	}
}

//*****************************************************************************
//
static void client_SetThingArguments( BYTESTREAM_s *pByteStream )
{
	LONG	lID;
	ULONG	ulArgs0;
	ULONG	ulArgs1;
	ULONG	ulArgs2;
	ULONG	ulArgs3;
	ULONG	ulArgs4;
	AActor	*pActor;

	// Get the ID of the actor whose arguments are being updated.
	lID = NETWORK_ReadShort( pByteStream );

	// Read in the actor's arguments.
	ulArgs0 = NETWORK_ReadByte( pByteStream );
	ulArgs1 = NETWORK_ReadByte( pByteStream );
	ulArgs2 = NETWORK_ReadByte( pByteStream );
	ulArgs3 = NETWORK_ReadByte( pByteStream );
	ulArgs4 = NETWORK_ReadByte( pByteStream );

	// Now try to find the corresponding actor.
	pActor = CLIENT_FindThingByNetID( lID );
	if ( pActor == NULL )
	{
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_SetThingArguments: Couldn't find thing: %d\n", lID );
#endif
		return;
	}

	// Finally, set the thing's arguments.
	pActor->args[0] = ulArgs0;
	pActor->args[1] = ulArgs1;
	pActor->args[2] = ulArgs2;
	pActor->args[3] = ulArgs3;
	pActor->args[4] = ulArgs4;

	// Gross hack for invisible bridges, since they set their height/radius
	// based on their args the instant they're spawned.
	// [WS] Added check for CustomBridge
	if (( pActor->IsKindOf( PClass::FindClass( "InvisibleBridge" ) )) ||
		( pActor->IsKindOf( PClass::FindClass( "AmbientSound" ) )) ||
		( pActor->IsKindOf( PClass::FindClass( "CustomBridge" ) )))
	{
		pActor->BeginPlay( );
	}
}

//*****************************************************************************
//
static void client_SetThingTranslation( BYTESTREAM_s *pByteStream )
{
	LONG	lID;
	LONG	lTranslation;
	AActor	*pActor;

	// Get the ID of the actor whose translation is being updated.
	lID = NETWORK_ReadShort( pByteStream );

	// Read in the actor's translation.
	lTranslation = NETWORK_ReadLong( pByteStream );

	// Now try to find the corresponding actor.
	pActor = CLIENT_FindThingByNetID( lID );
	if ( pActor == NULL )
	{
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_SetThingTranslation: Couldn't find thing: %d\n", lID );
#endif
		return;
	}

	// Finally, set the thing's translation.
	pActor->Translation = lTranslation;
}

//*****************************************************************************
//
static void client_SetThingProperty( BYTESTREAM_s *pByteStream )
{
	LONG	lID;
	ULONG	ulProperty;
	ULONG	ulPropertyValue;
	AActor	*pActor;

	// Get the ID of the actor whose translation is being updated.
	lID = NETWORK_ReadShort( pByteStream );

	// Read in which property is being updated.
	ulProperty = NETWORK_ReadByte( pByteStream );

	// Read in the actor's property.
	ulPropertyValue = NETWORK_ReadLong( pByteStream );

	// Now try to find the corresponding actor.
	pActor = CLIENT_FindThingByNetID( lID );
	if ( pActor == NULL )
	{
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_SetThingProperty: Couldn't find thing: %d\n", lID );
#endif
		return;
	}

	// Set one of the actor's properties, depending on what was read in.
	switch ( ulProperty )
	{
	case APROP_Speed:

		pActor->Speed = ulPropertyValue;
		break;
	case APROP_Alpha:

		pActor->alpha = ulPropertyValue;
		break;
	case APROP_RenderStyle:

		pActor->RenderStyle.AsDWORD = ulPropertyValue;
		break;
	case APROP_JumpZ:

		if ( pActor->IsKindOf( RUNTIME_CLASS( APlayerPawn )))
			static_cast<APlayerPawn *>( pActor )->JumpZ = ulPropertyValue;
		break;
	default:

		Printf( "client_SetThingProperty: Unknown property, %d!\n", static_cast<unsigned int> (ulProperty) );
		return;
	}
}

//*****************************************************************************
//
static void client_SetThingSound( BYTESTREAM_s *pByteStream )
{
	LONG		lID;
	ULONG		ulSound;
	AActor		*pActor;

	// Get the ID of the actor whose translation is being updated.
	lID = NETWORK_ReadShort( pByteStream );

	// Read in which sound is being updated.
	ulSound = NETWORK_ReadByte( pByteStream );

	// Read in the actor's new sound.
	const char* pszSound = client_ReadBytestreamSound( pByteStream );

	// Now try to find the corresponding actor.
	pActor = CLIENT_FindThingByNetID( lID );
	if ( pActor == NULL )
	{
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_SetThingSound: Couldn't find thing: %d\n", lID );
#endif
		return;
	}

	// Set one of the actor's sounds, depending on what was read in.
	switch ( ulSound )
	{
	case ACTORSOUND_SEESOUND:

		pActor->SeeSound = pszSound;
		break;
	case ACTORSOUND_ATTACKSOUND:

		pActor->AttackSound = pszSound;
		break;
	case ACTORSOUND_PAINSOUND:

		pActor->PainSound = pszSound;
		break;
	case ACTORSOUND_DEATHSOUND:

		pActor->DeathSound = pszSound;
		break;
	case ACTORSOUND_ACTIVESOUND:

		pActor->ActiveSound = pszSound;
		break;
	default:

		Printf( "client_SetThingSound: Unknown sound, %d!\n", static_cast<unsigned int> (ulSound) );
		return;
	}
}

//*****************************************************************************
//
static void client_SetThingSpawnPoint( BYTESTREAM_s *pByteStream )
{
	// [BB] Get the ID of the actor whose SpawnPoint is being updated.
	const LONG lID = NETWORK_ReadShort( pByteStream );

	// [BB] Get the actor's SpawnPoint.
	const LONG lSpawnPointX = NETWORK_ReadLong( pByteStream );
	const LONG lSpawnPointY = NETWORK_ReadLong( pByteStream );
	const LONG lSpawnPointZ = NETWORK_ReadLong( pByteStream );

	// Now try to find the corresponding actor.
	AActor *pActor = CLIENT_FindThingByNetID( lID );
	if ( pActor == NULL )
	{
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_SetThingSpawnPoint: Couldn't find thing: %d\n", lID );
#endif
		return;
	}

	// [BB] Set the actor's SpawnPoint.
	pActor->SpawnPoint[0] = lSpawnPointX;
	pActor->SpawnPoint[1] = lSpawnPointY;
	pActor->SpawnPoint[2] = lSpawnPointZ;
}

//*****************************************************************************
//
static void client_SetThingSpecial1( BYTESTREAM_s *pByteStream )
{
	LONG	lID;
	LONG	lSpecial1;
	AActor	*pActor;

	// Get the ID of the actor whose special2 is being updated.
	lID = NETWORK_ReadShort( pByteStream );

	// Get the actor's special2.
	lSpecial1 = NETWORK_ReadShort( pByteStream );

	// Now try to find the corresponding actor.
	pActor = CLIENT_FindThingByNetID( lID );
	if ( pActor == NULL )
	{
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_SetThingSpecial1: Couldn't find thing: %d\n", lID );
#endif
		return;
	}

	// Set one of the actor's special1.
	pActor->special1 = lSpecial1;
}

//*****************************************************************************
//
static void client_SetThingSpecial2( BYTESTREAM_s *pByteStream )
{
	LONG	lID;
	LONG	lSpecial2;
	AActor	*pActor;

	// Get the ID of the actor whose special2 is being updated.
	lID = NETWORK_ReadShort( pByteStream );

	// Get the actor's special2.
	lSpecial2 = NETWORK_ReadShort( pByteStream );

	// Now try to find the corresponding actor.
	pActor = CLIENT_FindThingByNetID( lID );
	if ( pActor == NULL )
	{
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_SetThingSpecial2: Couldn't find thing: %d\n", lID );
#endif
		return;
	}

	// Set one of the actor's special2.
	pActor->special2 = lSpecial2;
}

//*****************************************************************************
//
static void client_SetThingTics( BYTESTREAM_s *pByteStream )
{
	AActor		*pActor;
	LONG		lID;
	LONG		lTics;

	// Read in the thing's network ID.
	lID = NETWORK_ReadShort( pByteStream );

	// Read in the thing's new tics.
	lTics = NETWORK_ReadShort( pByteStream );

	// Now try to find the thing.
	pActor = CLIENT_FindThingByNetID( lID );
	if ( pActor == NULL )
	{
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_SetThingTics: Couldn't find thing: %d\n", lID );
#endif
		return;
	}

	// Finally, set the tics.
	pActor->tics = lTics;
}

//*****************************************************************************
//
static void client_SetThingTID( BYTESTREAM_s *pByteStream )
{
	AActor		*pActor;
	LONG		lID;
	LONG		lTid;

	// Read in the thing's network ID.
	lID = NETWORK_ReadShort( pByteStream );

	// Read in the thing's new angle.
	lTid = NETWORK_ReadShort( pByteStream );

	// Now try to find the thing.
	pActor = CLIENT_FindThingByNetID( lID );
	if ( pActor == NULL )
	{
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_SetThingTID: Couldn't find thing: %d\n", lID );
#endif
		return;
	}

	// [BB] Finally, set the tid, but be careful doing so (cf. FUNC(LS_Thing_ChangeTID)).
	if (!(pActor->ObjectFlags & OF_EuthanizeMe))
	{
		pActor->RemoveFromHash ();
		pActor->tid = lTid;
		pActor->AddToHash();
	}
}

//*****************************************************************************
//
static void client_SetThingGravity( BYTESTREAM_s *pByteStream )
{
	LONG	lID;
	LONG	lGravity;
	AActor	*pActor;

	// Get the ID of the actor whose gravity is being updated.
	lID = NETWORK_ReadShort( pByteStream );

	// Get the actor's gravity.
	lGravity = NETWORK_ReadLong( pByteStream );

	// Now try to find the corresponding actor.
	pActor = CLIENT_FindThingByNetID( lID );
	if ( pActor == NULL )
	{
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_SetThingGravity: Couldn't find thing: %d\n", lID );
#endif
		return;
	}

	// Set the actor's gravity.
	pActor->gravity = lGravity;
}

//*****************************************************************************
//
static void client_SetThingFrame( BYTESTREAM_s *pByteStream, bool bCallStateFunction )
{
	LONG			lID;
	const char		*pszState;
	LONG			lOffset;
	AActor			*pActor;
	FState			*pNewState = NULL;

	// Read in the network ID for the object to have its state changed.
	lID = NETWORK_ReadShort( pByteStream );

	// Read in the state.
	pszState = NETWORK_ReadString( pByteStream );

	// Offset into the state label.
	lOffset = NETWORK_ReadByte( pByteStream );

	// Not in a level; nothing to do (shouldn't happen!)
	if ( gamestate != GS_LEVEL )
		return;

	// Find the actor associated with the ID.
	pActor = CLIENT_FindThingByNetID( lID );
	if ( pActor == NULL )
	{
		// There should probably be the potential for a warning message here.
		return;
	}

	// [BB] In this case lOffset is just the offset from one of the default states of the actor or the state owner.
	// Handle this accordingly.
	if ( pszState[0] == ':' || pszState[0] == ';' || pszState[0] == '+' )
	{
		FState* pBaseState = NULL;

		if ( pszState[0] == ':' )
		{
			switch ( pszState[1] )
			{
			case 'S':
				pBaseState = pActor->SpawnState;

				break;
			case 'M':
				pBaseState = pActor->MissileState;

				break;
			case 'T':
				pBaseState = pActor->SeeState;

				break;
			case 'N':
				pBaseState = pActor->MeleeState;

				break;
			default:
				// [BB] Unknown base state specified. We can't do anythig.
				return;
			}
			// [BB] The offset is only guaranteed to work if the actor owns the state.
			if ( ( lOffset != 0 ) && ( ( ActorOwnsState ( pActor, pBaseState ) == false ) || ( ActorOwnsState ( pActor, pBaseState + lOffset ) == false ) ) )
			{
#ifdef CLIENT_WARNING_MESSAGES
				if ( ActorOwnsState ( pActor, pBaseState ) == false )
					Printf ( "client_SetThingFrame: %s doesn't own %s\n", pActor->GetClass()->TypeName.GetChars(), pszState );
				if ( ActorOwnsState ( pActor, pBaseState + lOffset ) == false )
					Printf ( "client_SetThingFrame: %s doesn't own %s + %d\n", pActor->GetClass()->TypeName.GetChars(), pszState, lOffset );
#endif
				return;
			}
		}
		else if ( pszState[0] == ';' || pszState[0] == '+' )
		{
			const PClass *pStateOwnerClass = PClass::FindClass ( pszState+1 );
			const AActor *pStateOwner = ( pStateOwnerClass != NULL ) ? GetDefaultByType ( pStateOwnerClass ) : NULL;
			if ( pStateOwner )
			{
				if ( pszState[0] == ';' )
					pBaseState = pStateOwner->SpawnState;
				else
					pBaseState = pStateOwnerClass->ActorInfo->FindState(NAME_Death);
				// [BB] The offset is only guaranteed to work if the actor owns the state and pBaseState.
				// Note: Looks like one can't call GetClass() on an actor pointer obtained by GetDefaultByType.
				if ( ( lOffset != 0 ) && ( ( ClassOwnsState ( pStateOwnerClass, pBaseState ) == false ) || ( ClassOwnsState ( pStateOwnerClass, pBaseState + lOffset ) == false ) ) )
				{
#ifdef CLIENT_WARNING_MESSAGES
					Printf ( "client_SetThingFrame: %s doesn't own %s + %d\n", pStateOwnerClass->TypeName.GetChars(), pszState, lOffset );
#endif
					return;
				}
			}
		}

		// [BB] We can only set the state, if the actor has pBaseState. But unless the server
		// is sending us garbage or this client has altered actor defintions, this check
		// should always succeed.
		if ( pBaseState )
		{
			if ( bCallStateFunction )
				pActor->SetState( pBaseState + lOffset );
			else
				pActor->SetState( pBaseState + lOffset, true );
		}
		return;
	}

	// Build the state name list.
	TArray<FName> &StateList = MakeStateNameList( pszState );

	pNewState = RUNTIME_TYPE( pActor )->ActorInfo->FindState( StateList.Size( ), &StateList[0] );
	if ( pNewState )
	{
		// [BB] The offset was calculated by SERVERCOMMANDS_SetThingFrame using GetNextState(),
		// so we have to use this here, too, to propely find the target state.
		for ( int i = 0; i < lOffset; i++ )
		{
			if ( pNewState != NULL )
				pNewState = pNewState->GetNextState( );
		}

		if ( pNewState == NULL )
			return;

		if ( bCallStateFunction )
			pActor->SetState( pNewState );
		else
			pActor->SetState( pNewState, true );
	}
}

//*****************************************************************************
//
static void client_SetWeaponAmmoGive( BYTESTREAM_s *pByteStream )
{
	LONG	lID;
	AActor	*pActor;
	ULONG	ulAmmoGive1;
	ULONG	ulAmmoGive2;

	// Get the ID of the actor whose water level is being updated.
	lID = NETWORK_ReadShort( pByteStream );

	// Read in the amount of ammo type 1 this weapon gives.
	ulAmmoGive1 = NETWORK_ReadShort( pByteStream );

	// Read in the amount of ammo type 2 this weapon gives.
	ulAmmoGive2 = NETWORK_ReadShort( pByteStream );

	// Now try to find the corresponding actor.
	pActor = CLIENT_FindThingByNetID( lID );
	if ( pActor == NULL )
	{
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_SetWeaponAmmoGive: Couldn't find thing: %d\n", lID );
#endif
		return;
	}

	// If this actor isn't a weapon, break out.
	if ( pActor->IsKindOf( RUNTIME_CLASS( AWeapon )) == false )
		return;

	// Finally, actually set the amount of ammo this weapon gives us.
	static_cast<AWeapon *>( pActor )->AmmoGive1 = ulAmmoGive1;
	static_cast<AWeapon *>( pActor )->AmmoGive2 = ulAmmoGive2;
}

//*****************************************************************************
//
static void client_ThingIsCorpse( BYTESTREAM_s *pByteStream )
{
	AActor	*pActor;
	LONG	lID;
	bool	bIsMonster;

	// Read in the network ID of the thing to make dead.
	lID = NETWORK_ReadShort( pByteStream );

	// Is this thing a monster?
	bIsMonster = !!NETWORK_ReadByte( pByteStream );

	pActor = CLIENT_FindThingByNetID( lID );
	if ( pActor == NULL )
	{
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_ThingIsCorpse: Couldn't find thing: %d\n", lID );
#endif
		return;
	}

	CALL_ACTION(A_NoBlocking, pActor );	// [RH] Use this instead of A_PainDie

	// Do some other stuff done in AActor::Die.
	pActor->flags &= ~(MF_SHOOTABLE|MF_FLOAT|MF_SKULLFLY|MF_NOGRAVITY);
	pActor->flags |= MF_CORPSE|MF_DROPOFF;
	pActor->height >>= 2;

	// Set the thing to the last frame of its death state.
	CLIENT_SetActorToLastDeathStateFrame ( pActor );

	if ( bIsMonster )
		level.killed_monsters++;

	// If this is a player, put the player in his dead state.
	if ( pActor->player )
	{
		pActor->player->playerstate = PST_DEAD;
		// [BB] Also set the health to 0.
		pActor->player->health = pActor->health = 0;
	}
}

//*****************************************************************************
//
static void client_HideThing( BYTESTREAM_s *pByteStream )
{
	AActor	*pActor;
	LONG	lID;

	// Read in the network ID of the thing to hide.
	lID = NETWORK_ReadShort( pByteStream );

	// Didn't find it.
	pActor = CLIENT_FindThingByNetID( lID );
	if ( pActor == NULL )
	{
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_HideThing: Couldn't find thing: %d\n", lID );
#endif
		return;
	}

	// Put the item in a hidden state.
	// [BB] You can call HideIndefinitely only on AInventory and descendants.
	if ( !(pActor->IsKindOf( RUNTIME_CLASS( AInventory ))) )
		return;
	static_cast<AInventory *>( pActor )->HideIndefinitely( );
}

//*****************************************************************************
//
static void client_TeleportThing( BYTESTREAM_s *pByteStream )
{
	LONG		lID;
	fixed_t		NewX;
	fixed_t		NewY;
	fixed_t		NewZ;
	fixed_t		NewMomX;
	fixed_t		NewMomY;
	fixed_t		NewMomZ;
	LONG		lNewReactionTime;
	angle_t		NewAngle;
	bool		bSourceFog;
	bool		bDestFog;
	bool		bTeleZoom;
	angle_t		Angle;
	AActor		*pActor;

	// Read in the network ID of the thing being teleported.
	lID = NETWORK_ReadShort( pByteStream );

	// Read in the thing's new position.
	NewX = NETWORK_ReadShort( pByteStream ) << FRACBITS;
	NewY = NETWORK_ReadShort( pByteStream ) << FRACBITS;
	NewZ = NETWORK_ReadShort( pByteStream ) << FRACBITS;

	// Read in the thing's new momentum.
	NewMomX = NETWORK_ReadShort( pByteStream ) << FRACBITS;
	NewMomY = NETWORK_ReadShort( pByteStream ) << FRACBITS;
	NewMomZ = NETWORK_ReadShort( pByteStream ) << FRACBITS;

	// Read in the thing's new reaction time.
	lNewReactionTime = NETWORK_ReadShort( pByteStream );

	// Read in the thing's new angle.
	NewAngle = NETWORK_ReadLong( pByteStream );

	// Should we spawn a teleport fog at the spot the thing is teleporting from?
	// What about the spot the thing is teleporting to?
	bSourceFog = !!NETWORK_ReadByte( pByteStream );
	bDestFog = !!NETWORK_ReadByte( pByteStream );

	// Should be do the teleport zoom?
	bTeleZoom = !!NETWORK_ReadByte( pByteStream );

	pActor = CLIENT_FindThingByNetID( lID );
	if ( pActor == NULL )
		return;

	// Move the player to his new position.
//	P_TeleportMove( pActor, NewX, NewY, NewZ, false );

	// Spawn teleport fog at the source.
	if ( bSourceFog )
		Spawn<ATeleportFog>( pActor->x, pActor->y, pActor->z + (( pActor->flags & MF_MISSILE ) ? 0 : TELEFOGHEIGHT ), ALLOW_REPLACE );

	// Set the thing's new position.
	CLIENT_MoveThing( pActor, NewX, NewY, NewZ );

	// Spawn a teleport fog at the destination.
	if ( bDestFog )
	{
		// Spawn the fog slightly in front of the thing's destination.
		Angle = NewAngle >> ANGLETOFINESHIFT;

		Spawn<ATeleportFog>( pActor->x + ( 20 * finecosine[Angle] ),
			pActor->y + ( 20 * finesine[Angle] ),
			pActor->z + (( pActor->flags & MF_MISSILE ) ? 0 : TELEFOGHEIGHT ),
			ALLOW_REPLACE );
	}

	// Set the thing's new momentum.
	pActor->velx = NewMomX;
	pActor->vely = NewMomY;
	pActor->velz = NewMomZ;

	// Also, if this is a player, set his bobbing appropriately.
	if ( pActor->player )
	{
		pActor->player->velx = NewMomX;
		pActor->player->vely = NewMomY;

		// [BB] If the server is teleporting us, don't let our prediction get messed up.
		if ( pActor == players[consoleplayer].mo )
			CLIENT_AdjustPredictionToServerSideConsolePlayerMove( NewX, NewY, NewZ );
	}

	// Reset the thing's new reaction time.
	pActor->reactiontime = lNewReactionTime;

	// Set the thing's new angle.
	pActor->angle = NewAngle;

	// User variable to do a weird zoom thingy when you teleport.
	if (( bTeleZoom ) && ( telezoom ) && ( pActor->player ))
		pActor->player->FOV = MIN( 175.f, pActor->player->DesiredFOV + 45.f );
}

//*****************************************************************************
//
static void client_ThingActivate( BYTESTREAM_s *pByteStream )
{
	LONG	lID;
	AActor	*pActor;
	AActor	*pActivator;

	// Get the ID of the actor to activate.
	lID = NETWORK_ReadShort( pByteStream );

	// Now try to find the corresponding actor.
	pActor = CLIENT_FindThingByNetID( lID );

	// Get the ID of the activator.
	lID = NETWORK_ReadShort( pByteStream );
	if ( lID == -1 )
		pActivator = NULL;
	else
		pActivator = CLIENT_FindThingByNetID( lID );

	if ( pActor == NULL )
	{
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_ThingActivate: Couldn't find thing: %d\n", lID );
#endif
		return;
	}

	// Finally, activate the actor.
	pActor->Activate( pActivator );
}

//*****************************************************************************
//
static void client_ThingDeactivate( BYTESTREAM_s *pByteStream )
{
	LONG	lID;
	AActor	*pActor;
	AActor	*pActivator;

	// Get the ID of the actor to deactivate.
	lID = NETWORK_ReadShort( pByteStream );

	// Now try to find the corresponding actor.
	pActor = CLIENT_FindThingByNetID( lID );

	// Get the ID of the activator.
	lID = NETWORK_ReadShort( pByteStream );
	if ( lID == -1 )
		pActivator = NULL;
	else
		pActivator = CLIENT_FindThingByNetID( lID );

	if ( pActor == NULL )
	{
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_ThingDeactivate: Couldn't find thing: %d\n", lID );
#endif
		return;
	}

	// Finally, deactivate the actor.
	pActor->Deactivate( pActivator );
}

//*****************************************************************************
//
static void client_RespawnDoomThing( BYTESTREAM_s *pByteStream )
{
	LONG	lID;
	bool	bFog;
	AActor	*pActor;

	// Read in the thing's network ID.
	lID = NETWORK_ReadShort( pByteStream );

	// Should a fog be spawned when the item respawns?
	bFog = !!NETWORK_ReadByte( pByteStream );

	// Nothing to do if the level isn't loaded!
	if ( gamestate != GS_LEVEL )
		return;

	pActor = CLIENT_FindThingByNetID( lID );

	// Couldn't find a matching actor. Ignore...
	if ( pActor == NULL )
	{
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_RespawnDoomThing: Couldn't find thing: %d\n", lID );
#endif
		return; 
	}

	// Finally, respawn the item.
	CLIENT_RestoreSpecialPosition( pActor );
	CLIENT_RestoreSpecialDoomThing( pActor, bFog );
}

//*****************************************************************************
//
static void client_RespawnRavenThing( BYTESTREAM_s *pByteStream )
{
	LONG	lID;
	AActor	*pActor;

	// Read in the thing's network ID.
	lID = NETWORK_ReadShort( pByteStream );

	// Nothing to do if the level isn't loaded!
	if ( gamestate != GS_LEVEL )
		return;

	pActor = CLIENT_FindThingByNetID( lID );

	// Couldn't find a matching actor. Ignore...
	if ( pActor == NULL )
	{
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_RespawnSpecialThing1: Couldn't find thing: %d\n", lID );
#endif
		return; 
	}

	pActor->renderflags &= ~RF_INVISIBLE;
	S_Sound( pActor, CHAN_VOICE, "misc/spawn", 1, ATTN_IDLE );

	pActor->SetState( RUNTIME_CLASS ( AInventory )->ActorInfo->FindState("HideSpecial") + 3 );
}

//*****************************************************************************
//
static void client_SpawnBlood( BYTESTREAM_s *pByteStream )
{
	fixed_t			X;
	fixed_t			Y;
	fixed_t			Z;
	angle_t			Dir;
	int				Damage;
	LONG			lID;
	AActor			*pOriginator;

	// Read in the XYZ location of the blood.
	X = NETWORK_ReadShort( pByteStream ) << FRACBITS;
	Y = NETWORK_ReadShort( pByteStream ) << FRACBITS;
	Z = NETWORK_ReadShort( pByteStream ) << FRACBITS;

	// Read in the direction.
	Dir = NETWORK_ReadShort( pByteStream ) << FRACBITS;

	// Read in the damage.
	Damage = NETWORK_ReadByte( pByteStream );

	// Read in the NetID of the originator.
	lID = NETWORK_ReadShort( pByteStream );

	// Find the originator by its NetID.
	pOriginator = CLIENT_FindThingByNetID( lID );

	// [BB] P_SpawnBlood crashes if pOriginator is a NULL pointer.
	if ( pOriginator )
		P_SpawnBlood (X, Y, Z, Dir, Damage, pOriginator);
}

//*****************************************************************************
//
static void client_SpawnBloodSplatter( BYTESTREAM_s *pByteStream, bool bIsBloodSplatter2 )
{
	// Read in the XYZ location of the blood.
	fixed_t X = NETWORK_ReadShort( pByteStream ) << FRACBITS;
	fixed_t Y = NETWORK_ReadShort( pByteStream ) << FRACBITS;
	fixed_t Z = NETWORK_ReadShort( pByteStream ) << FRACBITS;

	// Read in the NetID of the originator.
	LONG lID = NETWORK_ReadShort( pByteStream );

	// Find the originator by its NetID.
	AActor *pOriginator = CLIENT_FindThingByNetID( lID );

	if ( pOriginator )
	{
		if ( bIsBloodSplatter2 )
			P_BloodSplatter2 (X, Y, Z, pOriginator);
		else
			P_BloodSplatter (X, Y, Z, pOriginator);
	}
}

//*****************************************************************************
//
static void client_SpawnPuff( BYTESTREAM_s *pByteStream )
{
	fixed_t			X;
	fixed_t			Y;
	fixed_t			Z;
	USHORT			usActorNetworkIndex;
	ULONG			ulState;
	bool			bReceiveTranslation;
	AActor			*pActor;
	FState			*pState;

	// Read in the XYZ location of the item.
	X = NETWORK_ReadShort( pByteStream ) << FRACBITS;
	Y = NETWORK_ReadShort( pByteStream ) << FRACBITS;
	Z = NETWORK_ReadShort( pByteStream ) << FRACBITS;

	// Read in the identification of the item.
	usActorNetworkIndex = NETWORK_ReadShort( pByteStream );

	// Read in the state of the puff.
	ulState = NETWORK_ReadByte( pByteStream );

	// Read in whether or not the translation will be sent.
	bReceiveTranslation = !!NETWORK_ReadByte( pByteStream );

	// Finally, spawn the thing.
	pActor = CLIENT_SpawnThing( NETWORK_GetClassFromIdentification( usActorNetworkIndex ), X, Y, Z, -1 );
	if ( pActor == NULL )
		return;

	// [BB] If we are supposed to set the translation, read in the translation
	// and set it, if we sucessfully spawned the actor.
	if( bReceiveTranslation )
	{
		LONG lTranslation = NETWORK_ReadLong( pByteStream );
		if( pActor )
			pActor->Translation = lTranslation;
	}

	// Put the puff in the proper state.
	switch ( ulState )
	{
	case STATE_CRASH:

		pState = pActor->FindState( NAME_Crash );
		if ( pState )
			pActor->SetState( pState );
		break;
	case STATE_MELEE:

		pState = pActor->MeleeState;
		if ( pState )
			pActor->SetState( pState );
		break;
	}
}

//*****************************************************************************
//
static void client_Print( BYTESTREAM_s *pByteStream )
{
	ULONG		ulPrintLevel;
	const char	*pszString;

	// Read in the print level.
	ulPrintLevel = NETWORK_ReadByte( pByteStream );

	// Read in the string to be printed.
	pszString = NETWORK_ReadString( pByteStream );

	// Print out the message.
	Printf( ulPrintLevel, "%s", pszString );
}

//*****************************************************************************
//
static void client_PrintMid( BYTESTREAM_s *pByteStream )
{
	const char	*pszString;
	bool		bBold;

	// Read in the string that's supposed to be printed.
	pszString = NETWORK_ReadString( pByteStream );

	// Read in whether or not it's a bold message.
	bBold = !!NETWORK_ReadByte( pByteStream );

	// Print the message.
	if ( bBold )
		C_MidPrintBold( SmallFont, pszString );
	else
		C_MidPrint( SmallFont, pszString );
}

//*****************************************************************************
//
static void client_PrintMOTD( BYTESTREAM_s *pByteStream )
{
	// Read in the MOTD, and display it later.
	g_MOTD = NETWORK_ReadString( pByteStream );
}

//*****************************************************************************
//
static void client_PrintHUDMessage( BYTESTREAM_s *pByteStream )
{
	char		szString[MAX_NETWORK_STRING];
	float		fX;
	float		fY;
	LONG		lHUDWidth;
	LONG		lHUDHeight;
	LONG		lColor;
	float		fHoldTime;
	const char	*pszFont;
	bool		bLog;
	LONG		lID;
	DHUDMessage	*pMsg;

	// Read in the string.
	strncpy( szString, NETWORK_ReadString( pByteStream ), MAX_NETWORK_STRING );
	szString[MAX_NETWORK_STRING - 1] = 0;

	// Read in the XY.
	fX = NETWORK_ReadFloat( pByteStream );
	fY = NETWORK_ReadFloat( pByteStream );

	// Read in the HUD size.
	lHUDWidth = NETWORK_ReadShort( pByteStream );
	lHUDHeight = NETWORK_ReadShort( pByteStream );

	// Read in the color.
	lColor = NETWORK_ReadByte( pByteStream );

	// Read in the hold time.
	fHoldTime = NETWORK_ReadFloat( pByteStream );

	// Read in the font being used.
	pszFont = NETWORK_ReadString( pByteStream );

	// Read in whether or not the message should be logged.
	bLog = !!NETWORK_ReadByte( pByteStream );

	// Read in the ID.
	lID = NETWORK_ReadLong( pByteStream );

	// We cannot create the message if there's no status bar to attach it to.
	if ( StatusBar == NULL )
		return;

	// [BB] We can't create the message if the font doesn't exist.
	FFont *font = V_GetFont( pszFont );
	if ( font == NULL )
		return;

	// Create the message.
	pMsg = new DHUDMessage( font, szString,
		fX,
		fY,
		lHUDWidth,
		lHUDHeight,
		(EColorRange)lColor,
		fHoldTime );

	// Now attach the message.
	StatusBar->AttachMessage( pMsg, lID );

	// Log the message if desired.
	if ( bLog )
		CLIENT_LogHUDMessage( szString, lColor );
}

//*****************************************************************************
//
static void client_PrintHUDMessageFadeOut( BYTESTREAM_s *pByteStream )
{
	char				szString[MAX_NETWORK_STRING];
	float				fX;
	float				fY;
	LONG				lHUDWidth;
	LONG				lHUDHeight;
	LONG				lColor;
	float				fHoldTime;
	float				fFadeOutTime;
	const char			*pszFont;
	bool				bLog;
	LONG				lID;
	DHUDMessageFadeOut	*pMsg;

	// Read in the string.
	strncpy( szString, NETWORK_ReadString( pByteStream ), MAX_NETWORK_STRING );
	szString[MAX_NETWORK_STRING - 1] = 0;

	// Read in the XY.
	fX = NETWORK_ReadFloat( pByteStream );
	fY = NETWORK_ReadFloat( pByteStream );

	// Read in the HUD size.
	lHUDWidth = NETWORK_ReadShort( pByteStream );
	lHUDHeight = NETWORK_ReadShort( pByteStream );

	// Read in the color.
	lColor = NETWORK_ReadByte( pByteStream );

	// Read in the hold time.
	fHoldTime = NETWORK_ReadFloat( pByteStream );

	// Read in the fade time.
	fFadeOutTime = NETWORK_ReadFloat( pByteStream );

	// Read in the font being used.
	pszFont = NETWORK_ReadString( pByteStream );

	// Read in whether or not the message should be logged.
	bLog = !!NETWORK_ReadByte( pByteStream );

	// Read in the ID.
	lID = NETWORK_ReadLong( pByteStream );

	// We cannot create the message if there's no status bar to attach it to.
	if ( StatusBar == NULL )
		return;

	// [BB] We can't create the message if the font doesn't exist.
	FFont *font = V_GetFont( pszFont );
	if ( font == NULL )
		return;

	// Create the message.
	pMsg = new DHUDMessageFadeOut( font, szString,
		fX,
		fY,
		lHUDWidth,
		lHUDHeight,
		(EColorRange)lColor,
		fHoldTime,
		fFadeOutTime );

	// Now attach the message.
	StatusBar->AttachMessage( pMsg, lID );

	// Log the message if desired.
	if ( bLog )
		CLIENT_LogHUDMessage( szString, lColor );
}

//*****************************************************************************
//
static void client_PrintHUDMessageFadeInOut( BYTESTREAM_s *pByteStream )
{
	char					szString[MAX_NETWORK_STRING];
	float					fX;
	float					fY;
	LONG					lHUDWidth;
	LONG					lHUDHeight;
	LONG					lColor;
	float					fHoldTime;
	float					fFadeInTime;
	float					fFadeOutTime;
	const char				*pszFont;
	bool					bLog;
	LONG					lID;
	DHUDMessageFadeInOut	*pMsg;

	// Read in the string.
	strncpy( szString, NETWORK_ReadString( pByteStream ), MAX_NETWORK_STRING );
	szString[MAX_NETWORK_STRING - 1] = 0;

	// Read in the XY.
	fX = NETWORK_ReadFloat( pByteStream );
	fY = NETWORK_ReadFloat( pByteStream );

	// Read in the HUD size.
	lHUDWidth = NETWORK_ReadShort( pByteStream );
	lHUDHeight = NETWORK_ReadShort( pByteStream );

	// Read in the color.
	lColor = NETWORK_ReadByte( pByteStream );

	// Read in the hold time.
	fHoldTime = NETWORK_ReadFloat( pByteStream );

	// Read in the fade in time.
	fFadeInTime = NETWORK_ReadFloat( pByteStream );

	// Read in the fade out time.
	fFadeOutTime = NETWORK_ReadFloat( pByteStream );

	// Read in the font being used.
	pszFont = NETWORK_ReadString( pByteStream );

	// Read in whether or not the message should be logged.
	bLog = !!NETWORK_ReadByte( pByteStream );

	// Read in the ID.
	lID = NETWORK_ReadLong( pByteStream );

	// We cannot create the message if there's no status bar to attach it to.
	if ( StatusBar == NULL )
		return;

	// [BB] We can't create the message if the font doesn't exist.
	FFont *font = V_GetFont( pszFont );
	if ( font == NULL )
		return;

	// Create the message.
	pMsg = new DHUDMessageFadeInOut( font, szString,
		fX,
		fY,
		lHUDWidth,
		lHUDHeight,
		(EColorRange)lColor,
		fHoldTime,
		fFadeInTime,
		fFadeOutTime );

	// Now attach the message.
	StatusBar->AttachMessage( pMsg, lID );

	// Log the message if desired.
	if ( bLog )
		CLIENT_LogHUDMessage( szString, lColor );
}

//*****************************************************************************
//
static void client_PrintHUDMessageTypeOnFadeOut( BYTESTREAM_s *pByteStream )
{
	char						szString[MAX_NETWORK_STRING];
	float						fX;
	float						fY;
	LONG						lHUDWidth;
	LONG						lHUDHeight;
	LONG						lColor;
	float						fTypeOnTime;
	float						fHoldTime;
	float						fFadeOutTime;
	const char					*pszFont;
	bool						bLog;
	LONG						lID;
	DHUDMessageTypeOnFadeOut	*pMsg;

	// Read in the string.
	strncpy( szString, NETWORK_ReadString( pByteStream ), MAX_NETWORK_STRING );
	szString[MAX_NETWORK_STRING - 1] = 0;

	// Read in the XY.
	fX = NETWORK_ReadFloat( pByteStream );
	fY = NETWORK_ReadFloat( pByteStream );

	// Read in the HUD size.
	lHUDWidth = NETWORK_ReadShort( pByteStream );
	lHUDHeight = NETWORK_ReadShort( pByteStream );

	// Read in the color.
	lColor = NETWORK_ReadByte( pByteStream );

	// Read in the type on time.
	fTypeOnTime = NETWORK_ReadFloat( pByteStream );

	// Read in the hold time.
	fHoldTime = NETWORK_ReadFloat( pByteStream );

	// Read in the fade out time.
	fFadeOutTime = NETWORK_ReadFloat( pByteStream );

	// Read in the font being used.
	pszFont = NETWORK_ReadString( pByteStream );

	// Read in whether or not the message should be logged.
	bLog = !!NETWORK_ReadByte( pByteStream );

	// Read in the ID.
	lID = NETWORK_ReadLong( pByteStream );

	// We cannot create the message if there's no status bar to attach it to.
	if ( StatusBar == NULL )
		return;

	// [BB] We can't create the message if the font doesn't exist.
	FFont *font = V_GetFont( pszFont );
	if ( font == NULL )
		return;

	// Create the message.
	pMsg = new DHUDMessageTypeOnFadeOut( font, szString,
		fX,
		fY,
		lHUDWidth,
		lHUDHeight,
		(EColorRange)lColor,
		fTypeOnTime,
		fHoldTime,
		fFadeOutTime );

	// Now attach the message.
	StatusBar->AttachMessage( pMsg, lID );

	// Log the message if desired.
	if ( bLog )
		CLIENT_LogHUDMessage( szString, lColor );
}

//*****************************************************************************
//
static void client_SetGameMode( BYTESTREAM_s *pByteStream )
{
	UCVarValue	Value;

	GAMEMODE_SetCurrentMode ( static_cast<GAMEMODE_e> ( NETWORK_ReadByte( pByteStream ) ) );

	// [BB] The client doesn't necessarily know the game mode in P_SetupLevel, so we have to call this here.
	if ( domination )
		DOMINATION_Init();

	Value.Bool = !!NETWORK_ReadByte( pByteStream );
	instagib.ForceSet( Value, CVAR_Bool );

	Value.Bool = !!NETWORK_ReadByte( pByteStream );
	buckshot.ForceSet( Value, CVAR_Bool );
}

//*****************************************************************************
//
static void client_SetGameSkill( BYTESTREAM_s *pByteStream )
{
	UCVarValue	Value;

	// Read in the gameskill setting, and set gameskill to this setting.
	Value.Int = NETWORK_ReadByte( pByteStream );
	gameskill.ForceSet( Value, CVAR_Int );

	// Do the same for botskill.
	Value.Int = NETWORK_ReadByte( pByteStream );
	botskill.ForceSet( Value, CVAR_Int );
}

//*****************************************************************************
//
static void client_SetGameDMFlags( BYTESTREAM_s *pByteStream )
{
	UCVarValue	Value;

	// Read in the dmflags value, and set it to this value.
	Value.Int = NETWORK_ReadLong( pByteStream );
	dmflags.ForceSet( Value, CVAR_Int );

	// Do the same for dmflags2.
	Value.Int = NETWORK_ReadLong( pByteStream );
	dmflags2.ForceSet( Value, CVAR_Int );

	// ... and compatflags.
	Value.Int = NETWORK_ReadLong( pByteStream );
	compatflags.ForceSet( Value, CVAR_Int );

	// [BB] ... and zacompatflags.
	Value.Int = NETWORK_ReadLong( pByteStream );
	zacompatflags.ForceSet( Value, CVAR_Int );

	// [BB] ... and zadmflags.
	Value.Int = NETWORK_ReadLong( pByteStream );
	zadmflags.ForceSet( Value, CVAR_Int );
}

//*****************************************************************************
//
static void client_SetGameModeLimits( BYTESTREAM_s *pByteStream )
{
	UCVarValue	Value;

	// Read in, and set the value for fraglimit.
	Value.Int = NETWORK_ReadShort( pByteStream );
	fraglimit.ForceSet( Value, CVAR_Int );

	// Read in, and set the value for timelimit.
	Value.Float = NETWORK_ReadFloat( pByteStream );
	timelimit.ForceSet( Value, CVAR_Float );

	// Read in, and set the value for pointlimit.
	Value.Int = NETWORK_ReadShort( pByteStream );
	pointlimit.ForceSet( Value, CVAR_Int );

	// Read in, and set the value for duellimit.
	Value.Int = NETWORK_ReadByte( pByteStream );
	duellimit.ForceSet( Value, CVAR_Int );

	// Read in, and set the value for winlimit.
	Value.Int = NETWORK_ReadByte( pByteStream );
	winlimit.ForceSet( Value, CVAR_Int );

	// Read in, and set the value for wavelimit.
	Value.Int = NETWORK_ReadByte( pByteStream );
	wavelimit.ForceSet( Value, CVAR_Int );

	// Read in, and set the value for sv_cheats.
	Value.Int = NETWORK_ReadByte( pByteStream );
	sv_cheats.ForceSet( Value, CVAR_Int );
	// [BB] This ensures that am_cheat respects the sv_cheats value we just set.
	am_cheat = am_cheat;

	// Read in, and set the value for sv_fastweapons.
	Value.Int = NETWORK_ReadByte( pByteStream );
	sv_fastweapons.ForceSet( Value, CVAR_Int );

	// Read in, and set the value for sv_maxlives.
	Value.Int = NETWORK_ReadByte( pByteStream );
	sv_maxlives.ForceSet( Value, CVAR_Int );

	// Read in, and set the value for sv_maxteams.
	Value.Int = NETWORK_ReadByte( pByteStream );
	sv_maxteams.ForceSet( Value, CVAR_Int );

	// [BB] Read in, and set the value for sv_gravity.
	Value.Float = NETWORK_ReadFloat( pByteStream );
	sv_gravity.ForceSet( Value, CVAR_Float );

	// [BB] Read in, and set the value for sv_aircontrol.
	Value.Float = NETWORK_ReadFloat( pByteStream );
	sv_aircontrol.ForceSet( Value, CVAR_Float );

	// [WS] Read in, and set the value for sv_coop_damagefactor.
	Value.Float = NETWORK_ReadFloat( pByteStream );
	sv_coop_damagefactor.ForceSet( Value, CVAR_Float );

	// [WS] Read in, and set the value for alwaysapplydmflags.
	Value.Bool = !!NETWORK_ReadByte( pByteStream );
	alwaysapplydmflags.ForceSet( Value, CVAR_Bool );

	// [AM] Read in, and set the value for lobby.
	Value.String = const_cast<char*>(NETWORK_ReadString( pByteStream ));
	lobby.ForceSet( Value, CVAR_String );

	// [TP] Yea.
	Value.Bool = !!NETWORK_ReadByte( pByteStream );
	sv_limitcommands.ForceSet( Value, CVAR_Bool );
}

//*****************************************************************************
//
static void client_SetGameEndLevelDelay( BYTESTREAM_s *pByteStream )
{
	ULONG	ulDelay;

	ulDelay = NETWORK_ReadShort( pByteStream );

	GAME_SetEndLevelDelay( ulDelay );
}

//*****************************************************************************
//
static void client_SetGameModeState( BYTESTREAM_s *pByteStream )
{
	ULONG	ulModeState;
	ULONG	ulCountdownTicks;

	ulModeState = NETWORK_ReadByte( pByteStream );
	ulCountdownTicks = NETWORK_ReadShort( pByteStream );

	if ( duel )
	{
		DUEL_SetState( (DUELSTATE_e)ulModeState );
		DUEL_SetCountdownTicks( ulCountdownTicks );
	}
	else if ( lastmanstanding || teamlms )
	{
		LASTMANSTANDING_SetState( (LMSSTATE_e)ulModeState );
		LASTMANSTANDING_SetCountdownTicks( ulCountdownTicks );
	}
	else if ( possession || teampossession )
	{
		POSSESSION_SetState( (PSNSTATE_e)ulModeState );
		if ( (PSNSTATE_e)ulModeState == PSNS_ARTIFACTHELD )
			POSSESSION_SetArtifactHoldTicks( ulCountdownTicks );
		else
			POSSESSION_SetCountdownTicks( ulCountdownTicks );
	}
	else if ( survival )
	{
		SURVIVAL_SetState( (SURVIVALSTATE_e)ulModeState );
		SURVIVAL_SetCountdownTicks( ulCountdownTicks );
	}
	else if ( invasion )
	{
		INVASION_SetState( (INVASIONSTATE_e)ulModeState );
		INVASION_SetCountdownTicks( ulCountdownTicks );
	}
}

//*****************************************************************************
//
static void client_SetDuelNumDuels( BYTESTREAM_s *pByteStream )
{
	ULONG	ulNumDuels;

	// Read in the number of duels that have occured.
	ulNumDuels = NETWORK_ReadByte( pByteStream );

	DUEL_SetNumDuels( ulNumDuels );
}

//*****************************************************************************
//
static void client_SetLMSSpectatorSettings( BYTESTREAM_s *pByteStream )
{
	UCVarValue	Value;

	Value.Int = NETWORK_ReadLong( pByteStream );
	lmsspectatorsettings.ForceSet( Value, CVAR_Int );
}

//*****************************************************************************
//
static void client_SetLMSAllowedWeapons( BYTESTREAM_s *pByteStream )
{
	UCVarValue	Value;

	Value.Int = NETWORK_ReadLong( pByteStream );
	lmsallowedweapons.ForceSet( Value, CVAR_Int );
}

//*****************************************************************************
//
static void client_SetInvasionNumMonstersLeft( BYTESTREAM_s *pByteStream )
{
	ULONG	ulNumMonstersLeft;
	ULONG	ulNumArchVilesLeft;

	// Read in the number of monsters left.
	ulNumMonstersLeft = NETWORK_ReadShort( pByteStream );

	// Read in the number of arch-viles left.
	ulNumArchVilesLeft = NETWORK_ReadShort( pByteStream );

	// Set the number of monsters/archies left.
	INVASION_SetNumMonstersLeft( ulNumMonstersLeft );
	INVASION_SetNumArchVilesLeft( ulNumArchVilesLeft );
}

//*****************************************************************************
//
static void client_SetInvasionWave( BYTESTREAM_s *pByteStream )
{
	ULONG	ulWave;

	// Read in the current wave we're on.
	ulWave = NETWORK_ReadByte( pByteStream );

	// Set the current wave in the invasion module.
	INVASION_SetCurrentWave( ulWave );
}

//*****************************************************************************
//
static void client_SetSimpleCTFSTMode( BYTESTREAM_s *pByteStream )
{
	bool	bSimpleCTFST;

	// Read in whether or not we're in the simple version of these game modes.
	bSimpleCTFST = !!NETWORK_ReadByte( pByteStream );

	// Set the simple CTF/ST mode.
	TEAM_SetSimpleCTFSTMode( bSimpleCTFST );
}

//*****************************************************************************
//
static void client_DoPossessionArtifactPickedUp( BYTESTREAM_s *pByteStream )
{
	ULONG	ulPlayer;
	ULONG	ulTicks;

	// Read in the player who picked up the possession artifact.
	ulPlayer = NETWORK_ReadByte( pByteStream );

	// Read in how many ticks remain until the player potentially scores a point.
	ulTicks = NETWORK_ReadShort( pByteStream );

	// If this is an invalid player, break out.
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	// If we're not playing possession, there's no need to do this.
	if (( possession == false ) && ( teampossession == false ))
		return;

	// Finally, call the function that handles the picking up of the artifact.
	POSSESSION_ArtifactPickedUp( &players[ulPlayer], ulTicks );
}

//*****************************************************************************
//
static void client_DoPossessionArtifactDropped( BYTESTREAM_s *pByteStream )
{
	// If we're not playing possession, there's no need to do this.
	if (( possession == false ) && ( teampossession == false ))
		return;

	// Simply call this function.
	POSSESSION_ArtifactDropped( );
}

//*****************************************************************************
//
static void client_DoGameModeFight( BYTESTREAM_s *pByteStream )
{
	ULONG	ulWave;

	// What wave are we starting? (invasion only).
	ulWave = NETWORK_ReadByte( pByteStream );

	// Play fight sound, and draw gfx.
	if ( duel )
		DUEL_DoFight( );
	else if ( lastmanstanding || teamlms )
		LASTMANSTANDING_DoFight( );
	else if ( possession || teampossession )
		POSSESSION_DoFight( );
	else if ( survival )
		SURVIVAL_DoFight( );
	else if ( invasion )
		INVASION_BeginWave( ulWave );
}

//*****************************************************************************
//
static void client_DoGameModeCountdown( BYTESTREAM_s *pByteStream )
{
	ULONG	ulTicks;

	ulTicks = NETWORK_ReadShort( pByteStream );

	// Begin the countdown.
	if ( duel )
		DUEL_StartCountdown( ulTicks );
	else if ( lastmanstanding || teamlms )
		LASTMANSTANDING_StartCountdown( ulTicks );
	else if ( possession || teampossession )
		POSSESSION_StartCountdown( ulTicks );
	else if ( survival )
		SURVIVAL_StartCountdown( ulTicks );
	else if ( invasion )
		INVASION_StartCountdown( ulTicks );
}

//*****************************************************************************
//
static void client_DoGameModeWinSequence( BYTESTREAM_s *pByteStream )
{
	ULONG	ulWinner;

	ulWinner = NETWORK_ReadByte( pByteStream );

	// Begin the win sequence.
	if ( duel )
		DUEL_DoWinSequence( ulWinner );
	else if ( lastmanstanding || teamlms )
	{
		if ( lastmanstanding && ( ulWinner == static_cast<ULONG>(consoleplayer) ))
			ANNOUNCER_PlayEntry( cl_announcer, "YouWin" );
		else if ( teamlms && players[consoleplayer].bOnTeam && ( ulWinner == players[consoleplayer].ulTeam ))
			ANNOUNCER_PlayEntry( cl_announcer, "YouWin" );

		LASTMANSTANDING_DoWinSequence( ulWinner );
	}
//	else if ( possession || teampossession )
//		POSSESSION_DoWinSequence( ulWinner );
	else if ( invasion )
		INVASION_DoWaveComplete( );
}

//*****************************************************************************
//
static void client_SetDominationState( BYTESTREAM_s *pByteStream )
{
	unsigned int NumPoints = NETWORK_ReadLong( pByteStream );

	// [BB] It's impossible that the server sends us this many points
	// in a single packet, so something must be wrong. Just parse
	// what the server has claimed to have send, but don't try to store
	// it or allocate memory for it.
	if ( NumPoints > MAX_UDP_PACKET )
	{
		for ( unsigned int i = 0; i < NumPoints; ++i )
			NETWORK_ReadByte( pByteStream );
		return;
	}

	unsigned int *PointOwners = new unsigned int[NumPoints];
	for(unsigned int i = 0;i < NumPoints;i++)
	{
		PointOwners[i] = NETWORK_ReadByte( pByteStream );
	}
	DOMINATION_LoadInit(NumPoints, PointOwners);
}

//*****************************************************************************
//
static void client_SetDominationPointOwnership( BYTESTREAM_s *pByteStream )
{
	unsigned int ulPoint = NETWORK_ReadByte( pByteStream );
	unsigned int ulPlayer = NETWORK_ReadByte( pByteStream );

	// If this is an invalid player, break out.
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	DOMINATION_SetOwnership(ulPoint, &players[ulPlayer]);
}

//*****************************************************************************
//
static void client_SetTeamFrags( BYTESTREAM_s *pByteStream )
{
	ULONG	ulTeam;
	LONG	lFragCount;
	bool	bAnnounce;

	// Read in the team.
	ulTeam = NETWORK_ReadByte( pByteStream );

	// Read in the fragcount.
	lFragCount = NETWORK_ReadShort( pByteStream );

	// Announce a lead change... but don't do it if we're receiving a snapshot of the level!
	bAnnounce = !!NETWORK_ReadByte( pByteStream );
	if ( g_ConnectionState != CTS_ACTIVE )
		bAnnounce = false;

	// Finally, set the team's fragcount.
	TEAM_SetFragCount( ulTeam, lFragCount, bAnnounce );
}

//*****************************************************************************
//
static void client_SetTeamScore( BYTESTREAM_s *pByteStream )
{
	ULONG	ulTeam;
	LONG	lScore;
	bool	bAnnounce;

	// Read in the team having its score updated.
	ulTeam = NETWORK_ReadByte( pByteStream );

	// Read in the team's new score.
	lScore = NETWORK_ReadShort( pByteStream );

	// Should it be announced?
	bAnnounce = !!NETWORK_ReadByte( pByteStream );
	
	// Don't announce the score change if we're receiving a snapshot of the level!
	if ( g_ConnectionState != CTS_ACTIVE )
		bAnnounce = false;

	TEAM_SetScore( ulTeam, lScore, bAnnounce );

	SCOREBOARD_RefreshHUD( );
}

//*****************************************************************************
//
static void client_SetTeamWins( BYTESTREAM_s *pByteStream )
{
	ULONG	ulTeamIdx;
	LONG	lWinCount;
	bool	bAnnounce;

	// Read in the team.
	ulTeamIdx = NETWORK_ReadByte( pByteStream );

	// Read in the wins.
	lWinCount = NETWORK_ReadShort( pByteStream );

	// Read in whether or not it should be announced.	
	bAnnounce = !!NETWORK_ReadByte( pByteStream );

	// Don't announce if we're receiving a snapshot of the level!
	if ( g_ConnectionState != CTS_ACTIVE )
		bAnnounce = false;

	// Finally, set the team's win count.
	TEAM_SetWinCount( ulTeamIdx, lWinCount, bAnnounce );
}

//*****************************************************************************
//
static void client_SetTeamReturnTicks( BYTESTREAM_s *pByteStream )
{
	ULONG	ulTeam;
	ULONG	ulTicks;

	// Read in the team having its return ticks altered.
	ulTeam = NETWORK_ReadByte( pByteStream );

	// Read in the return ticks value.
	ulTicks = NETWORK_ReadShort( pByteStream );

	// Finally, set the return ticks for the given team.
	TEAM_SetReturnTicks( ulTeam, ulTicks );
}

//*****************************************************************************
//
static void client_TeamFlagReturned( BYTESTREAM_s *pByteStream )
{
	ULONG	ulTeam;

	// Read in the team that the flag has been returned for.
	ulTeam = NETWORK_ReadByte( pByteStream );

	// Finally, just call this function that does all the dirty work.
	TEAM_ExecuteReturnRoutine( ulTeam, NULL );
}

//*****************************************************************************
//
static void client_TeamFlagDropped( BYTESTREAM_s *pByteStream )
{
	ULONG	ulPlayer;
	ULONG	ulTeamIdx;

	// Read in the player that dropped a flag.
	ulPlayer = NETWORK_ReadByte( pByteStream );
	ulTeamIdx = NETWORK_ReadByte( pByteStream );

	// Finally, just call this function that does all the dirty work.
	TEAM_FlagDropped( &players[ulPlayer], ulTeamIdx );
}

//*****************************************************************************
//
static void client_SpawnMissile( BYTESTREAM_s *pByteStream )
{
	USHORT				usActorNetworkIndex;
	fixed_t				X;
	fixed_t				Y;
	fixed_t				Z;
	fixed_t				MomX;
	fixed_t				MomY;
	fixed_t				MomZ;
	LONG				lID;
	LONG				lTargetID;

	// Read in the XYZ location of the missile.
	X = NETWORK_ReadShort( pByteStream ) << FRACBITS;
	Y = NETWORK_ReadShort( pByteStream ) << FRACBITS;
	Z = NETWORK_ReadShort( pByteStream ) << FRACBITS;

	// Read in the XYZ momentum of the missile.
	MomX = NETWORK_ReadLong( pByteStream );
	MomY = NETWORK_ReadLong( pByteStream );
	MomZ = NETWORK_ReadLong( pByteStream );

	// Read in the identification of the missile.
	usActorNetworkIndex = NETWORK_ReadShort( pByteStream );

	// Read in the network ID of the missile, and its target.
	lID = NETWORK_ReadShort( pByteStream );
	lTargetID = NETWORK_ReadShort( pByteStream );

	// Finally, spawn the missile.
	CLIENT_SpawnMissile( NETWORK_GetClassFromIdentification( usActorNetworkIndex ), X, Y, Z, MomX, MomY, MomZ, lID, lTargetID );
}

//*****************************************************************************
//
static void client_SpawnMissileExact( BYTESTREAM_s *pByteStream )
{
	USHORT				usActorNetworkIndex;
	fixed_t				X;
	fixed_t				Y;
	fixed_t				Z;
	fixed_t				MomX;
	fixed_t				MomY;
	fixed_t				MomZ;
	LONG				lID;
	LONG				lTargetID;

	// Read in the XYZ location of the missile.
	X = NETWORK_ReadLong( pByteStream );
	Y = NETWORK_ReadLong( pByteStream );
	Z = NETWORK_ReadLong( pByteStream );

	// Read in the XYZ momentum of the missile.
	MomX = NETWORK_ReadLong( pByteStream );
	MomY = NETWORK_ReadLong( pByteStream );
	MomZ = NETWORK_ReadLong( pByteStream );

	// Read in the identification of the missile.
	usActorNetworkIndex = NETWORK_ReadShort( pByteStream );

	// Read in the network ID of the missile, and its target.
	lID = NETWORK_ReadShort( pByteStream );
	lTargetID = NETWORK_ReadShort( pByteStream );

	// Finally, spawn the missile.
	CLIENT_SpawnMissile( NETWORK_GetClassFromIdentification( usActorNetworkIndex ), X, Y, Z, MomX, MomY, MomZ, lID, lTargetID );
}

//*****************************************************************************
//
static void client_MissileExplode( BYTESTREAM_s *pByteStream )
{
	AActor		*pActor;
	LONG		lLine;
	line_t		*pLine;
	LONG		lID;
	fixed_t		X;
	fixed_t		Y;
	fixed_t		Z;

	// Read in the network ID of the exploding missile.
	lID = NETWORK_ReadShort( pByteStream );

	// Read in the line that the missile struck.
	lLine = NETWORK_ReadShort( pByteStream );
	if ( lLine >= 0 && lLine < numlines )
		pLine = &lines[lLine];
	else
		pLine = NULL;

	// Read in the XYZ of the explosion point.
	X = NETWORK_ReadShort( pByteStream ) << FRACBITS;
	Y = NETWORK_ReadShort( pByteStream ) << FRACBITS;
	Z = NETWORK_ReadShort( pByteStream ) << FRACBITS;

	// Find the actor associated with the given network ID.
	pActor = CLIENT_FindThingByNetID( lID );
	if ( pActor == NULL )
	{
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_MissileExplode: Couldn't find thing: %d\n", lID );
#endif
		return;
	}

	// Move the new actor to the position.
	CLIENT_MoveThing( pActor, X, Y, Z );

	// Blow it up!
	// [BB] Only if it's not already in its death state.
	if ( pActor->InState ( NAME_Death ) == false )
		P_ExplodeMissile( pActor, pLine, NULL );
}

//*****************************************************************************
//
static void client_WeaponSound( BYTESTREAM_s *pByteStream )
{
	ULONG		ulPlayer;

	// Read in the player who's creating a weapon sound.
	ulPlayer = NETWORK_ReadByte( pByteStream );

	// Read in the sound that's being played.
	const char* pszSound = client_ReadBytestreamSound( pByteStream );

	// Check to make sure everything is valid. If not, break out. Also, don't
	// play the sound if the console player is spying through this player's eyes.
	if (( PLAYER_IsValidPlayer( ulPlayer ) == false ) ||
		( players[ulPlayer].mo == NULL ) ||
		( players[ulPlayer].mo->CheckLocalView( consoleplayer )))
	{
		return;
	}

	// Finally, play the sound.
	S_Sound( players[ulPlayer].mo, CHAN_WEAPON, pszSound, 1, ATTN_NORM );
}

//*****************************************************************************
//
static void client_WeaponChange( BYTESTREAM_s *pByteStream )
{
	ULONG			ulPlayer;
	USHORT			usActorNetworkIndex;
	const PClass	*pType;
	AWeapon			*pWeapon;

	// Read in the player whose info is about to be updated.
	ulPlayer = NETWORK_ReadByte( pByteStream );

	// Read in the identification of the weapon we're supposed to switch to.
	usActorNetworkIndex = NETWORK_ReadShort( pByteStream );

	// If the player doesn't exist, get out!
	if (( players[ulPlayer].mo == NULL ) || ( playeringame[ulPlayer] == false ))
		return;

	// If it's an invalid class, or not a weapon, don't switch.
	pType = NETWORK_GetClassFromIdentification( usActorNetworkIndex );
	if (( pType == NULL ) || !( pType->IsDescendantOf( RUNTIME_CLASS( AWeapon ))))
		return;

	// If we dont have this weapon already, we do now!
	pWeapon = static_cast<AWeapon *>( players[ulPlayer].mo->FindInventory( pType ));
	if ( pWeapon == NULL )
		pWeapon = static_cast<AWeapon *>( players[ulPlayer].mo->GiveInventoryType( pType ));

	// If he still doesn't have the object after trying to give it to him... then YIKES!
	if ( pWeapon == NULL )
	{
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_WeaponChange: Failed to give inventory type, %s!\n", NETWORK_GetClassNameFromIdentification( usActorNetworkIndex ));
#endif
		return;
	}

	// Bring the weapon up if necessary.
	if ( ( players[ulPlayer].ReadyWeapon != pWeapon ) || ( ( players[ulPlayer].PendingWeapon != WP_NOCHANGE ) && ( players[ulPlayer].PendingWeapon != pWeapon ) ) )
		players[ulPlayer].PendingWeapon = pWeapon;

	// Confirm to the server that this is the weapon we're using.
	CLIENT_UpdatePendingWeapon( &players[ulPlayer] );

	// [BB] Ensure that the weapon is brought up. Note: Needs to be done after calling
	// CLIENT_UpdatePendingWeapon because P_BringUpWeapon clears PendingWeapon to WP_NOCHANGE.
	P_BringUpWeapon (&players[ulPlayer]);
}

//*****************************************************************************
//
static void client_WeaponRailgun( BYTESTREAM_s *pByteStream )
{
	LONG		lID;
	FVector3	Start;
	FVector3	End;
	LONG		lColor1;
	LONG		lColor2;
	float		fMaxDiff;
	bool		bSilent;
	AActor		*pActor;

	// Read in the network ID of the source actor.
	lID = NETWORK_ReadShort( pByteStream );

	// Read in the XYZ position of the start of the trail.
	Start.X = NETWORK_ReadFloat( pByteStream );
	Start.Y = NETWORK_ReadFloat( pByteStream );
	Start.Z = NETWORK_ReadFloat( pByteStream );

	// Read in the XYZ position of the end of the trail.
	End.X = NETWORK_ReadFloat( pByteStream );
	End.Y = NETWORK_ReadFloat( pByteStream );
	End.Z = NETWORK_ReadFloat( pByteStream );

	// Read in the colors of the trail.
	lColor1 = NETWORK_ReadLong( pByteStream );
	lColor2 = NETWORK_ReadLong( pByteStream );

	// Read in maxdiff (whatever that is).
	fMaxDiff = NETWORK_ReadFloat( pByteStream );

	// Read in whether or not the trail should make a nosie.
	bSilent = !!NETWORK_ReadByte( pByteStream );

	// Find the actor associated with the given network ID.
	pActor = CLIENT_FindThingByNetID( lID );
	if ( pActor == NULL )
	{
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_WeaponRailgun: Couldn't find thing: %d\n", lID );
#endif
		return;
	}

	P_DrawRailTrail( pActor, Start, End, lColor1, lColor2, fMaxDiff, bSilent );
}

//*****************************************************************************
//
static void client_SetSectorFloorPlane( BYTESTREAM_s *pByteStream )
{
	LONG		lSectorID;
	LONG		lHeight;
	sector_t	*pSector;
	LONG		lDelta;
	LONG		lLastPos;

	// Read in the sector network ID.
	lSectorID = NETWORK_ReadShort( pByteStream );

	// Read in the height.
	lHeight = NETWORK_ReadShort( pByteStream ) << FRACBITS;

	// Find the sector associated with this network ID.
	pSector = CLIENT_FindSectorByID( lSectorID );
	if ( pSector == NULL )
	{
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_SetSectorFloorPlane: Couldn't find sector: %d\n", lSectorID );
#endif
		return;
	}

	// Calculate the change in floor height.
	lDelta = lHeight - pSector->floorplane.d;

	// Store the original height position.
	lLastPos = pSector->floorplane.d;

	// Change the height.
	pSector->floorplane.ChangeHeight( -lDelta );

	// Call this to update various actor's within the sector.
	P_ChangeSector( pSector, false, -lDelta, 0, false );

	// Finally, adjust textures.
	pSector->SetPlaneTexZ(sector_t::floor, pSector->GetPlaneTexZ(sector_t::floor) + pSector->floorplane.HeightDiff( lLastPos ) );

	// [BB] We also need to move any linked sectors.
	P_MoveLinkedSectors(pSector, false, -lDelta, false);
}

//*****************************************************************************
//
static void client_SetSectorCeilingPlane( BYTESTREAM_s *pByteStream )
{
	LONG		lSectorID;
	LONG		lHeight;
	sector_t	*pSector;
	LONG		lDelta;
	LONG		lLastPos;

	// Read in the sector network ID.
	lSectorID = NETWORK_ReadShort( pByteStream );

	// Read in the height.
	lHeight = NETWORK_ReadShort( pByteStream ) << FRACBITS;

	// Find the sector associated with this network ID.
	pSector = CLIENT_FindSectorByID( lSectorID );
	if ( pSector == NULL )
	{
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_SetSectorCeilingPlane: Couldn't find sector: %d\n", lSectorID );
#endif
		return;
	}

	// Calculate the change in ceiling height.
	lDelta = lHeight - pSector->ceilingplane.d;

	// Store the original height position.
	lLastPos = pSector->ceilingplane.d;

	// Change the height.
	pSector->ceilingplane.ChangeHeight( lDelta );

	// Finally, adjust textures.
	pSector->SetPlaneTexZ(sector_t::ceiling, pSector->GetPlaneTexZ(sector_t::ceiling) + pSector->ceilingplane.HeightDiff( lLastPos ) );

	// [BB] We also need to move any linked sectors.
	P_MoveLinkedSectors(pSector, false, lDelta, true);
}

//*****************************************************************************
//
static void client_SetSectorFloorPlaneSlope( BYTESTREAM_s *pByteStream )
{
	LONG		lSectorID;
	LONG		lA;
	LONG		lB;
	LONG		lC;
	sector_t	*pSector;

	// Read in the sector network ID.
	lSectorID = NETWORK_ReadShort( pByteStream );

	// Read in the various variables needed to calculate the slope.
	lA = NETWORK_ReadShort( pByteStream ) << FRACBITS;
	lB = NETWORK_ReadShort( pByteStream ) << FRACBITS;
	lC = NETWORK_ReadShort( pByteStream ) << FRACBITS;

	// Find the sector associated with this network ID.
	pSector = CLIENT_FindSectorByID( lSectorID );
	if ( pSector == NULL )
	{
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_SetSectorFloorPlaneSlope: Couldn't find sector: %d\n", lSectorID );
#endif
		return;
	}

	pSector->floorplane.a = lA;
	pSector->floorplane.b = lB;
	pSector->floorplane.c = lC;
	pSector->floorplane.ic = DivScale32( 1, pSector->floorplane.c );
}

//*****************************************************************************
//
static void client_SetSectorCeilingPlaneSlope( BYTESTREAM_s *pByteStream )
{
	LONG		lSectorID;
	LONG		lA;
	LONG		lB;
	LONG		lC;
	sector_t	*pSector;

	// Read in the sector network ID.
	lSectorID = NETWORK_ReadShort( pByteStream );

	// Read in the various variables needed to calculate the slope.
	lA = NETWORK_ReadShort( pByteStream ) << FRACBITS;
	lB = NETWORK_ReadShort( pByteStream ) << FRACBITS;
	lC = NETWORK_ReadShort( pByteStream ) << FRACBITS;

	// Find the sector associated with this network ID.
	pSector = CLIENT_FindSectorByID( lSectorID );
	if ( pSector == NULL )
	{
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_SetSectorCeilingPlaneSlope: Couldn't find sector: %d\n", lSectorID );
#endif
		return;
	}

	pSector->ceilingplane.a = lA;
	pSector->ceilingplane.b = lB;
	pSector->ceilingplane.c = lC;
	pSector->ceilingplane.ic = DivScale32( 1, pSector->ceilingplane.c );
}

//*****************************************************************************
//
static void client_SetSectorLightLevel( BYTESTREAM_s *pByteStream )
{
	LONG		lSectorID;
	LONG		lLightLevel;
	sector_t	*pSector;

	// Read in the sector network ID.
	lSectorID = NETWORK_ReadShort( pByteStream );

	// Read in the new light level.
	lLightLevel = NETWORK_ReadByte( pByteStream );

	// Find the sector associated with this network ID.
	pSector = CLIENT_FindSectorByID( lSectorID );
	if ( pSector == NULL )
	{ 
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_SetSectorLightLevel: Cannot find sector: %d\n", lSectorID );
#endif
		return; 
	}

	// Finally, set the light level.
	pSector->lightlevel = lLightLevel;
}

//*****************************************************************************
//
static void client_SetSectorColor( BYTESTREAM_s *pByteStream, bool bIdentifySectorsByTag )
{
	LONG		lSectorIDOrTag;
	LONG		lR;
	LONG		lG;
	LONG		lB;
	LONG		lDesaturate;
	sector_t	*pSector;
	PalEntry	Color;

	// Read in the sector to have its panning altered.
	lSectorIDOrTag = NETWORK_ReadShort( pByteStream );

	// Read in the RGB and desaturate.
	lR = NETWORK_ReadByte( pByteStream );
	lG = NETWORK_ReadByte( pByteStream );
	lB = NETWORK_ReadByte( pByteStream );
	lDesaturate = NETWORK_ReadByte( pByteStream );

	if ( bIdentifySectorsByTag )
	{
		int secnum = -1;

		while ((secnum = P_FindSectorFromTag (lSectorIDOrTag, secnum)) >= 0)
			sectors[secnum].SetColor(lR, lG, lB, lDesaturate, false, true);
	}
	else
	{
		// Now find the sector.
		pSector = CLIENT_FindSectorByID( lSectorIDOrTag );
		if ( pSector == NULL )
		{ 
#ifdef CLIENT_WARNING_MESSAGES
			Printf( "client_SetSectorColor: Cannot find sector: %d\n", lSectorIDOrTag );
#endif
			return; 
		}

		// Finally, set the color.
		pSector->SetColor(lR, lG, lB, lDesaturate, false, true);
	}
}

//*****************************************************************************
//
static void client_SetSectorFade( BYTESTREAM_s *pByteStream, bool bIdentifySectorsByTag )
{
	LONG		lSectorIDOrTag;
	LONG		lR;
	LONG		lG;
	LONG		lB;
	sector_t	*pSector;
	PalEntry	Fade;

	// Read in the sector to have its panning altered.
	lSectorIDOrTag = NETWORK_ReadShort( pByteStream );

	// Read in the RGB.
	lR = NETWORK_ReadByte( pByteStream );
	lG = NETWORK_ReadByte( pByteStream );
	lB = NETWORK_ReadByte( pByteStream );

	if ( bIdentifySectorsByTag )
	{
		int secnum = -1;

		while ((secnum = P_FindSectorFromTag (lSectorIDOrTag, secnum)) >= 0)
			sectors[secnum].SetFade(lR, lG, lB, false, true);
	}
	else
	{
		// Now find the sector.
		pSector = CLIENT_FindSectorByID( lSectorIDOrTag );
		if ( pSector == NULL )
		{ 
#ifdef CLIENT_WARNING_MESSAGES
			Printf( "client_SetSectorFade: Cannot find sector: %d\n", lSectorIDOrTag );
#endif
			return; 
		}

		// Finally, set the fade.
		pSector->SetFade(lR, lG, lB, false, true);
	}
}

//*****************************************************************************
//
static void client_SetSectorFlat( BYTESTREAM_s *pByteStream )
{
	sector_t		*pSector;
	LONG			lSectorID;
	char			szCeilingFlatName[MAX_NETWORK_STRING];
	const char		*pszFloorFlatName;
	FTextureID		flatLump;

	// Read in the sector ID.
	lSectorID = NETWORK_ReadShort( pByteStream );

	// Read in the ceiling flat name.
	sprintf( szCeilingFlatName, "%s", NETWORK_ReadString( pByteStream ));

	// Read in the floor flat name.
	pszFloorFlatName = NETWORK_ReadString( pByteStream );

	// Not in a level. Nothing to do!
	if ( gamestate != GS_LEVEL )
		return;

	// Find the sector associated with this network ID.
	pSector = CLIENT_FindSectorByID( lSectorID );
	if ( pSector == NULL )
	{
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_SetSectorFlat: Couldn't find sector: %d\n", lSectorID );
#endif
		return; 
	}

	flatLump = TexMan.GetTexture( szCeilingFlatName, FTexture::TEX_Flat );
	pSector->SetTexture(sector_t::ceiling, flatLump);

	flatLump = TexMan.GetTexture( pszFloorFlatName, FTexture::TEX_Flat );
	pSector->SetTexture(sector_t::floor, flatLump);
}

//*****************************************************************************
//
static void client_SetSectorPanning( BYTESTREAM_s *pByteStream )
{
	LONG		lSectorID;
	LONG		lCeilingXOffset;
	LONG		lCeilingYOffset;
	LONG		lFloorXOffset;
	LONG		lFloorYOffset;
	sector_t	*pSector;

	// Read in the sector to have its panning altered.
	lSectorID = NETWORK_ReadShort( pByteStream );

	// Read it's ceiling X offset.
	lCeilingXOffset = NETWORK_ReadShort( pByteStream );

	// Read it's ceiling Y offset.
	lCeilingYOffset = NETWORK_ReadShort( pByteStream );

	// Read it's floor X offset.
	lFloorXOffset = NETWORK_ReadShort( pByteStream );

	// Read it's floor Y offset.
	lFloorYOffset = NETWORK_ReadShort( pByteStream );

	// Now find the sector.
	pSector = CLIENT_FindSectorByID( lSectorID );
	if ( pSector == NULL )
	{ 
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_SetSectorPanning: Cannot find sector: %d\n", lSectorID );
#endif
		return; 
	}

	// Finally, set the offsets.
	pSector->SetXOffset(sector_t::ceiling, lCeilingXOffset * FRACUNIT);
	pSector->SetYOffset(sector_t::ceiling, lCeilingYOffset * FRACUNIT);

	pSector->SetXOffset(sector_t::floor, lFloorXOffset * FRACUNIT);
	pSector->SetYOffset(sector_t::floor, lFloorYOffset * FRACUNIT);
}

//*****************************************************************************
//
static void client_SetSectorRotation( BYTESTREAM_s *pByteStream, bool bIdentifySectorsByTag )
{
	// Read in the sector to have its panning altered.
	const LONG lSectorIDOrTag = NETWORK_ReadShort( pByteStream );

	// Read in the ceiling and floor rotation.
	const LONG lCeilingRotation = NETWORK_ReadShort( pByteStream );
	const LONG lFloorRotation = NETWORK_ReadShort( pByteStream );

	if ( bIdentifySectorsByTag )
	{
		int secnum = -1;

		while ((secnum = P_FindSectorFromTag (lSectorIDOrTag, secnum)) >= 0)
		{
			sectors[secnum].SetAngle(sector_t::floor, lFloorRotation * ANGLE_1);
			sectors[secnum].SetAngle(sector_t::ceiling, lCeilingRotation * ANGLE_1);
		}
	}
	else
	{
		// Now find the sector.
		sector_t *pSector = CLIENT_FindSectorByID( lSectorIDOrTag );
		if ( pSector == NULL )
		{ 
#ifdef CLIENT_WARNING_MESSAGES
			Printf( "client_SetSectorRotation: Cannot find sector: %d\n", lSectorID );
#endif
			return; 
		}

		// Finally, set the rotation.
		pSector->SetAngle(sector_t::ceiling, ( lCeilingRotation * ANGLE_1 ) );
		pSector->SetAngle(sector_t::floor, ( lFloorRotation * ANGLE_1 ) );
	}
}

//*****************************************************************************
//
static void client_SetSectorScale( BYTESTREAM_s *pByteStream )
{
	LONG		lSectorID;
	LONG		lCeilingXScale;
	LONG		lCeilingYScale;
	LONG		lFloorXScale;
	LONG		lFloorYScale;
	sector_t	*pSector;

	// Read in the sector to have its panning altered.
	lSectorID = NETWORK_ReadShort( pByteStream );

	// Read in the ceiling and floor scale.
	lCeilingXScale = NETWORK_ReadShort( pByteStream ) * FRACBITS;
	lCeilingYScale = NETWORK_ReadShort( pByteStream ) * FRACBITS;
	lFloorXScale = NETWORK_ReadShort( pByteStream ) * FRACBITS;
	lFloorYScale = NETWORK_ReadShort( pByteStream ) * FRACBITS;

	// Now find the sector.
	pSector = CLIENT_FindSectorByID( lSectorID );
	if ( pSector == NULL )
	{ 
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_SetSectorScale: Cannot find sector: %d\n", lSectorID );
#endif
		return; 
	}

	// Finally, set the scale.
	pSector->SetXScale(sector_t::ceiling, lCeilingXScale);
	pSector->SetYScale(sector_t::ceiling, lCeilingYScale);
	pSector->SetXScale(sector_t::floor, lFloorXScale);
	pSector->SetYScale(sector_t::floor, lFloorYScale);
}

//*****************************************************************************
//
static void client_SetSectorSpecial( BYTESTREAM_s *pByteStream )
{
	LONG		lSectorID;
	SHORT		sSpecial;
	sector_t	*pSector;

	// Read in the sector to have its special altered.
	lSectorID = NETWORK_ReadShort( pByteStream );

	// Read in the new special.
	sSpecial = NETWORK_ReadShort( pByteStream );

	// Now find the sector.
	pSector = CLIENT_FindSectorByID( lSectorID );
	if ( pSector == NULL )
	{ 
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_SetSectorSpecial: Cannot find sector: %d\n", lSectorID );
#endif
		return; 
	}

	// Finally, set the special.
	pSector->special = sSpecial;
}

//*****************************************************************************
//
static void client_SetSectorFriction( BYTESTREAM_s *pByteStream )
{
	LONG		lSectorID;
	LONG		lFriction;
	LONG		lMoveFactor;
	sector_t	*pSector;

	// Read in the sector to have its friction altered.
	lSectorID = NETWORK_ReadShort( pByteStream );

	// Read in the ceiling and floor scale.
	lFriction = NETWORK_ReadLong( pByteStream );
	lMoveFactor = NETWORK_ReadLong( pByteStream );

	// Now find the sector.
	pSector = CLIENT_FindSectorByID( lSectorID );
	if ( pSector == NULL )
	{ 
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_SetSectorScale: Cannot find sector: %d\n", lSectorID );
#endif
		return; 
	}

	// Set the friction.
	pSector->friction = lFriction;
	pSector->movefactor = lMoveFactor;

	// I'm not sure if we need to do this, but let's do it anyway.
	if ( lFriction == ORIG_FRICTION )
		pSector->special &= ~FRICTION_MASK;
	else
		pSector->special |= FRICTION_MASK;
}

//*****************************************************************************
//
static void client_SetSectorAngleYOffset( BYTESTREAM_s *pByteStream )
{
	LONG		lSectorID;
	LONG		lCeilingAngle;
	LONG		lCeilingYOffset;
	LONG		lFloorAngle;
	LONG		lFloorYOffset;
	sector_t	*pSector;

	// Read in the sector to have its friction altered.
	lSectorID = NETWORK_ReadShort( pByteStream );

	// Read in the sector's ceiling and floor angle and y-offset.
	lCeilingAngle = NETWORK_ReadLong( pByteStream );
	lCeilingYOffset = NETWORK_ReadLong( pByteStream );
	lFloorAngle = NETWORK_ReadLong( pByteStream );
	lFloorYOffset = NETWORK_ReadLong( pByteStream );

	// Now find the sector.
	pSector = CLIENT_FindSectorByID( lSectorID );
	if ( pSector == NULL )
	{ 
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_SetSectorScale: Cannot find sector: %d\n", lSectorID );
#endif
		return; 
	}

	// Set the sector's angle and y-offset.
	pSector->planes[sector_t::ceiling].xform.base_angle = lCeilingAngle;
	pSector->planes[sector_t::ceiling].xform.base_yoffs = lCeilingYOffset;
	pSector->planes[sector_t::floor].xform.base_angle = lFloorAngle;
	pSector->planes[sector_t::floor].xform.base_yoffs = lFloorYOffset;
}

//*****************************************************************************
//
static void client_SetSectorGravity( BYTESTREAM_s *pByteStream )
{
	LONG		lSectorID;
	float		fGravity;
	sector_t	*pSector;

	// Read in the sector to have its friction altered.
	lSectorID = NETWORK_ReadShort( pByteStream );

	// Read in the sector's gravity.
	fGravity = NETWORK_ReadFloat( pByteStream );

	// Now find the sector.
	pSector = CLIENT_FindSectorByID( lSectorID );
	if ( pSector == NULL )
	{ 
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_SetSectorScale: Cannot find sector: %d\n", lSectorID );
#endif
		return; 
	}

	// Set the sector's gravity.
	pSector->gravity = fGravity;
}

//*****************************************************************************
//
static void client_SetSectorReflection( BYTESTREAM_s *pByteStream )
{
	LONG		lSectorID;
	float		fCeilingReflect;
	float		fFloorReflect;
	sector_t	*pSector;

	// Read in the sector to have its reflection altered.
	lSectorID = NETWORK_ReadShort( pByteStream );

	// Read in the sector's ceiling and floor reflection.
	fCeilingReflect = NETWORK_ReadFloat( pByteStream );
	fFloorReflect = NETWORK_ReadFloat( pByteStream );

	// Now find the sector.
	pSector = CLIENT_FindSectorByID( lSectorID );
	if ( pSector == NULL )
	{ 
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_SetSectorScale: Cannot find sector: %d\n", lSectorID );
#endif
		return; 
	}

	// Set the sector's reflection.
	pSector->ceiling_reflect = fCeilingReflect;
	pSector->floor_reflect = fFloorReflect;
}

//*****************************************************************************
//
static void client_StopSectorLightEffect( BYTESTREAM_s *pByteStream )
{
	LONG							lSectorID;
	sector_t						*pSector;
	TThinkerIterator<DLighting>		Iterator;
	DLighting						*pEffect;

	// Read in the sector to have its light effect stopped.
	lSectorID = NETWORK_ReadShort( pByteStream );

	// Now find the sector.
	pSector = CLIENT_FindSectorByID( lSectorID );
	if ( pSector == NULL )
	{ 
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_StopSectorLightEffect: Cannot find sector: %d\n", lSectorID );
#endif
		return; 
	}

	// Finally, delete any effects this sector has.
	while (( pEffect = Iterator.Next( )) != NULL )
	{
		if ( pEffect->GetSector( ) == pSector )
		{
			pEffect->Destroy( );
			return;
		}
	}
}

//*****************************************************************************
//
static void client_DestroyAllSectorMovers( BYTESTREAM_s *pByteStream )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < (ULONG)numsectors; ulIdx++ )
	{
		if ( sectors[ulIdx].ceilingdata )
		{
			// Stop the sound sequence (if any) associated with this sector.
			SN_StopSequence( &sectors[ulIdx], CHAN_CEILING );

			sectors[ulIdx].ceilingdata->Destroy( );
			sectors[ulIdx].ceilingdata = NULL;
		}

		if ( sectors[ulIdx].floordata )
		{
			// Stop the sound sequence (if any) associated with this sector.
			SN_StopSequence( &sectors[ulIdx], CHAN_FLOOR );

			sectors[ulIdx].floordata->Destroy( );
			sectors[ulIdx].floordata = NULL;
		}
	}
}

//*****************************************************************************
//
static void client_SetSectorLink( BYTESTREAM_s *pByteStream )
{
	// [BB] Read in the sector network ID.
	ULONG ulSector = NETWORK_ReadShort( pByteStream );
	int iArg1 = NETWORK_ReadShort( pByteStream );
	int iArg2 = NETWORK_ReadByte( pByteStream );
	int iArg3 = NETWORK_ReadByte( pByteStream );

	// [BB] Find the sector associated with this network ID.
	sector_t *pSector = CLIENT_FindSectorByID( ulSector );
	if ( pSector == NULL )
	{
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_SetSectorLink: Couldn't find sector: %d\n", ulSector );
#endif
		return;
	}

	P_AddSectorLinks( pSector, iArg1, iArg2, iArg3);
}

//*****************************************************************************
//
static void client_DoSectorLightFireFlicker( BYTESTREAM_s *pByteStream )
{
	LONG			lSectorID;
	sector_t		*pSector;
	LONG			lMaxLight;
	LONG			lMinLight;
	DFireFlicker	*pFireFlicker;

	// Read in the sector the light effect is attached to.
	lSectorID = NETWORK_ReadShort( pByteStream );

	// Read in the sector's light level when the light effect is in its bright phase.
	lMaxLight = NETWORK_ReadByte( pByteStream );

	// Read in the sector's light level when the light effect is in its dark phase.
	lMinLight = NETWORK_ReadByte( pByteStream );

	// Now find the sector.
	pSector = CLIENT_FindSectorByID( lSectorID );
	if ( pSector == NULL )
	{ 
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_DoSectorLightFireFlicker: Cannot find sector: %d\n", lSectorID );
#endif
		return; 
	}

	// Create the light effect.
	pFireFlicker = new DFireFlicker( pSector, lMaxLight, lMinLight );
}

//*****************************************************************************
//
static void client_DoSectorLightFlicker( BYTESTREAM_s *pByteStream )
{
	LONG			lSectorID;
	sector_t		*pSector;
	LONG			lMaxLight;
	LONG			lMinLight;
	DFlicker		*pFlicker;

	// Read in the sector the light effect is attached to.
	lSectorID = NETWORK_ReadShort( pByteStream );

	// Read in the sector's light level when the light effect is in its bright phase.
	lMaxLight = NETWORK_ReadByte( pByteStream );

	// Read in the sector's light level when the light effect is in its dark phase.
	lMinLight = NETWORK_ReadByte( pByteStream );

	// Now find the sector.
	pSector = CLIENT_FindSectorByID( lSectorID );
	if ( pSector == NULL )
	{ 
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_DoSectorLightFireFlicker: Cannot find sector: %d\n", lSectorID );
#endif
		return; 
	}

	// Create the light effect.
	pFlicker = new DFlicker( pSector, lMaxLight, lMinLight );
}

//*****************************************************************************
//
static void client_DoSectorLightLightFlash( BYTESTREAM_s *pByteStream )
{
	LONG			lSectorID;
	sector_t		*pSector;
	LONG			lMaxLight;
	LONG			lMinLight;
	DLightFlash		*pLightFlash;

	// Read in the sector the light effect is attached to.
	lSectorID = NETWORK_ReadShort( pByteStream );

	// Read in the sector's light level when the light effect is in its bright phase.
	lMaxLight = NETWORK_ReadByte( pByteStream );

	// Read in the sector's light level when the light effect is in its dark phase.
	lMinLight = NETWORK_ReadByte( pByteStream );

	// Now find the sector.
	pSector = CLIENT_FindSectorByID( lSectorID );
	if ( pSector == NULL )
	{ 
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_DoSectorLightLightFlash: Cannot find sector: %d\n", lSectorID );
#endif
		return; 
	}

	// Create the light effect.
	pLightFlash = new DLightFlash( pSector, lMinLight, lMaxLight );
}

//*****************************************************************************
//
static void client_DoSectorLightStrobe( BYTESTREAM_s *pByteStream )
{
	LONG			lSectorID;
	sector_t		*pSector;
	LONG			lDarkTime;
	LONG			lBrightTime;
	LONG			lMaxLight;
	LONG			lMinLight;
	LONG			lCount;
	DStrobe			*pStrobe;

	// Read in the sector the light effect is attached to.
	lSectorID = NETWORK_ReadShort( pByteStream );

	// Read in how long the effect stays in its dark phase.
	lDarkTime = NETWORK_ReadShort( pByteStream );

	// Read in how long the effect stays in its bright phase.
	lBrightTime = NETWORK_ReadShort( pByteStream );

	// Read in the sector's light level when the light effect is in its bright phase.
	lMaxLight = NETWORK_ReadByte( pByteStream );

	// Read in the sector's light level when the light effect is in its dark phase.
	lMinLight = NETWORK_ReadByte( pByteStream );

	// Read in the amount of time left until the light changes from bright to dark, or vice
	// versa.
	lCount = NETWORK_ReadByte( pByteStream );

	// Now find the sector.
	pSector = CLIENT_FindSectorByID( lSectorID );
	if ( pSector == NULL )
	{ 
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_DoSectorLightStrobe: Cannot find sector: %d\n", lSectorID );
#endif
		return; 
	}

	// Create the light effect.
	pStrobe = new DStrobe( pSector, lMaxLight, lMinLight, lBrightTime, lDarkTime );
	pStrobe->SetCount( lCount );
}

//*****************************************************************************
//
static void client_DoSectorLightGlow( BYTESTREAM_s *pByteStream )
{
	LONG			lSectorID;
	sector_t		*pSector;
	DGlow			*pGlow;

	// Read in the sector the light effect is attached to.
	lSectorID = NETWORK_ReadShort( pByteStream );

	// Now find the sector.
	pSector = CLIENT_FindSectorByID( lSectorID );
	if ( pSector == NULL )
	{ 
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_DoSectorLightGlow: Cannot find sector: %d\n", lSectorID );
#endif
		return; 
	}

	// Create the light effect.
	pGlow = new DGlow( pSector );
}

//*****************************************************************************
//
static void client_DoSectorLightGlow2( BYTESTREAM_s *pByteStream )
{
	LONG			lSectorID;
	sector_t		*pSector;
	LONG			lStart;
	LONG			lEnd;
	LONG			lTics;
	LONG			lMaxTics;
	bool			bOneShot;
	DGlow2			*pGlow2;

	// Read in the sector the light effect is attached to.
	lSectorID = NETWORK_ReadShort( pByteStream );

	// Read in the start light level of the effect.
	lStart = NETWORK_ReadByte( pByteStream );

	// Read in the end light level of the effect.
	lEnd = NETWORK_ReadByte( pByteStream );

	// Read in the current progression of the effect.
	lTics = NETWORK_ReadShort( pByteStream );

	// Read in how many tics it takes to get from start to end.
	lMaxTics = NETWORK_ReadShort( pByteStream );

	// Read in whether or not the glow loops, or ends after one shot.
	bOneShot = !!NETWORK_ReadByte( pByteStream );

	// Now find the sector.
	pSector = CLIENT_FindSectorByID( lSectorID );
	if ( pSector == NULL )
	{ 
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_DoSectorLightStrobe: Cannot find sector: %d\n", lSectorID );
#endif
		return; 
	}

	// Create the light effect.
	pGlow2 = new DGlow2( pSector, lStart, lEnd, lMaxTics, bOneShot );
	pGlow2->SetTics( lTics );
}

//*****************************************************************************
//
static void client_DoSectorLightPhased( BYTESTREAM_s *pByteStream )
{
	LONG			lSectorID;
	sector_t		*pSector;
	LONG			lBaseLevel;
	LONG			lPhase;
	DPhased			*pPhased;

	// Read in the sector the light effect is attached to.
	lSectorID = NETWORK_ReadShort( pByteStream );

	// Read in the effect's base level parameter.
	lBaseLevel = NETWORK_ReadByte( pByteStream );

	// Read in the sector's phase parameter.
	lPhase = NETWORK_ReadByte( pByteStream );

	// Now find the sector.
	pSector = CLIENT_FindSectorByID( lSectorID );
	if ( pSector == NULL )
	{ 
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_DoSectorLightFireFlicker: Cannot find sector: %d\n", lSectorID );
#endif
		return; 
	}

	// Create the light effect.
	pPhased = new DPhased( pSector, lBaseLevel, lPhase );
}

//*****************************************************************************
//
static void client_SetLineAlpha( BYTESTREAM_s *pByteStream )
{
	line_t	*pLine;
	ULONG	ulLineIdx;
	fixed_t	alpha;

	// Read in the line to have its alpha altered.
	ulLineIdx = NETWORK_ReadShort( pByteStream );

	// Read in the new alpha.
	alpha = NETWORK_ReadLong( pByteStream );

	pLine = &lines[ulLineIdx];
	if (( pLine == NULL ) || ( ulLineIdx >= static_cast<ULONG>(numlines) ))
	{
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_SetLineAlpha: Couldn't find line: %d\n", ulLineIdx );
#endif
		return;
	}

	// Finally, set the alpha.
	pLine->Alpha = alpha;
}

//*****************************************************************************
//
static void client_SetLineTextureHelper ( ULONG ulLineIdx, ULONG ulSide, ULONG ulPosition, FTextureID texture )
{
	line_t *pLine = NULL;

	if ( ulLineIdx < static_cast<ULONG>(numlines) )
		pLine = &lines[ulLineIdx];

	if ( pLine == NULL )
	{
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_SetLineTexture: Couldn't find line: %d\n", ulLineIdx );
#endif
		return;
	}

	if ( pLine->sidedef[ulSide] == NULL )
		return;

	side_t *pSide = pLine->sidedef[ulSide];

	switch ( ulPosition )
	{
	case 0 /*TEXTURE_TOP*/:

		pSide->SetTexture(side_t::top, texture);
		break;
	case 1 /*TEXTURE_MIDDLE*/:

		pSide->SetTexture(side_t::mid, texture);
		break;
	case 2 /*TEXTURE_BOTTOM*/:

		pSide->SetTexture(side_t::bottom, texture);
		break;
	default:

		break;
	}
}

//*****************************************************************************
//
static void client_SetLineTexture( BYTESTREAM_s *pByteStream, bool bIdentifyLinesByID )
{
	ULONG		ulLineIdx;
	const char	*pszTextureName;
	ULONG		ulSide;
	ULONG		ulPosition;
	FTextureID	texture;

	// Read in the line to have its alpha altered.
	ulLineIdx = NETWORK_ReadShort( pByteStream );

	// Read in the new texture name.
	pszTextureName = NETWORK_ReadString( pByteStream );

	// Read in the side.
	ulSide = !!NETWORK_ReadByte( pByteStream );

	// Read in the position.
	ulPosition = NETWORK_ReadByte( pByteStream );

	texture = TexMan.GetTexture( pszTextureName, FTexture::TEX_Wall, FTextureManager::TEXMAN_Overridable );

	if ( !texture.Exists() )
		return;

	if ( bIdentifyLinesByID )
	{
		int linenum = -1;
		while ((linenum = P_FindLineFromID (ulLineIdx, linenum)) >= 0)
			client_SetLineTextureHelper ( linenum, ulSide, ulPosition, texture );
	}
	else
	{
		client_SetLineTextureHelper ( ulLineIdx, ulSide, ulPosition, texture );
	}
}

//*****************************************************************************
//
static void client_SetSomeLineFlags( BYTESTREAM_s *pByteStream )
{
	LONG	lLine;
	LONG	lBlockFlags;

	// Read in the line ID.
	lLine = NETWORK_ReadShort( pByteStream );

	// Read in the blocking flags.
	lBlockFlags = NETWORK_ReadLong( pByteStream );

	// Invalid line ID.
	if (( lLine >= numlines ) || ( lLine < 0 ))
		return;

	lines[lLine].flags &= ~(ML_BLOCKING|ML_BLOCK_PLAYERS|ML_BLOCKEVERYTHING|ML_RAILING|ML_ADDTRANS);
	lines[lLine].flags |= lBlockFlags;
}

//*****************************************************************************
//
static void client_SetSideFlags( BYTESTREAM_s *pByteStream )
{
	LONG	lSide;
	LONG	lFlags;

	// Read in the side ID.
	lSide = NETWORK_ReadShort( pByteStream );

	// Read in the flags.
	lFlags = NETWORK_ReadByte( pByteStream );

	// Invalid line ID.
	if (( lSide >= numsides ) || ( lSide < 0 ))
		return;

	sides[lSide].Flags = lFlags;
}

//*****************************************************************************
//
static void client_ACSScriptExecute( BYTESTREAM_s *pByteStream )
{
	ULONG			ulScript;
	LONG			lID;
	LONG			lLineIdx;
	bool			bBackSide;
	int				args[3] = {0};
	bool			bAlways;
	AActor			*pActor;
	line_t			*pLine;
	FString			mapname;
	level_info_t*	levelinfo;
	int				levelnum;
	BYTE			argheader;

	// Read in the script to be executed.
	ulScript = NETWORK_ReadShort( pByteStream );

	// Read in the ID of the activator.
	lID = NETWORK_ReadShort( pByteStream );

	// Read in the line index.
	lLineIdx = NETWORK_ReadShort( pByteStream );

	// [TP] Read in the levelnum of the map to execute the script on
	levelnum = NETWORK_ReadByte( pByteStream );

	// [TP] Argument header, see notes in sv_commands.cpp
	argheader = NETWORK_ReadByte( pByteStream );

	// [TP] Read in the arguments using the argument header data
	for( int i = 0; i < 3; ++i )
	{
		switch (( argheader >> ( 2 * i )) & 3 )
		{
			case 1: args[i] = NETWORK_ReadByte( pByteStream ); break;
			case 2: args[i] = NETWORK_ReadShort( pByteStream ); break;
			case 3: args[i] = NETWORK_ReadLong( pByteStream ); break;
			default: break;
		}
	}

	// [TP] Unpack bBackSide and bAlways
	bBackSide = !!(( argheader >> 6 ) & 1 );
	bAlways = !!(( argheader >> 7 ) & 1 );

	// [TP] Make a name out of the levelnum
	if ( levelnum == 0 )
	{
		mapname = level.mapname;
	}
	else if (( levelinfo = FindLevelByNum ( levelnum )))
	{
		mapname = levelinfo->mapname;
	}
	else
	{
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_ACSScriptExecute: Couldn't find map by levelnum: %d\n", levelnum );
#endif
		return;
	}

	// Find the actor that matches the given network ID.
	// [BB] If the netID is invalid, assume that there is no activator, i.e. the world activated the script.
	if ( lID == -1 )
		pActor = NULL;
	else
	{
		pActor = CLIENT_FindThingByNetID( lID );
		if ( pActor == NULL )
		{
#ifdef CLIENT_WARNING_MESSAGES
			Printf( "client_ACSScriptExecute: Couldn't find thing: %d\n", lID );
#endif
			return;
		}
	}

	if (( lLineIdx <= 0 ) || ( lLineIdx >= numlines ))
		pLine = NULL;
	else
		pLine = &lines[lLineIdx];

	P_StartScript( pActor, pLine, ulScript, mapname, bBackSide, args[0], args[1], args[2], bAlways, false );
}

//*****************************************************************************
//
static void client_Sound( BYTESTREAM_s *pByteStream )
{
	LONG		lChannel;
	LONG		lVolume;
	LONG		lAttenuation;

	// Read in the channel.
	lChannel = NETWORK_ReadByte( pByteStream );

	// Read in the name of the sound to play.
	const char* pszSoundString = client_ReadBytestreamSound( pByteStream );

	// Read in the volume.
	lVolume = NETWORK_ReadByte( pByteStream );
	if ( lVolume > 127 )
		lVolume = 127;

	// Read in the attenuation.
	lAttenuation = NETWORK_ReadByte( pByteStream );

	// Finally, play the sound.
	S_Sound( lChannel, pszSoundString, (float)lVolume / 127.f, NETWORK_AttenuationIntToFloat ( lAttenuation ) );
}

//*****************************************************************************
//
static void client_SoundActor( BYTESTREAM_s *pByteStream, bool bRespectActorPlayingSomething )
{
	LONG		lID;
	LONG		lChannel;
	LONG		lVolume;
	LONG		lAttenuation;
	AActor		*pActor;

	// Read in the spot ID.
	lID = NETWORK_ReadShort( pByteStream );

	// Read in the channel.
	lChannel = NETWORK_ReadShort( pByteStream );

	// Read in the name of the sound to play.
	const char* pszSoundString = client_ReadBytestreamSound( pByteStream );

	// Read in the volume.
	lVolume = NETWORK_ReadByte( pByteStream );
	if ( lVolume > 127 )
		lVolume = 127;

	// Read in the attenuation.
	lAttenuation = NETWORK_ReadByte( pByteStream );

	// Find the actor from the ID.
	pActor = CLIENT_FindThingByNetID( lID );
	if ( pActor == NULL )
	{
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_SoundActor: Couldn't find thing: %d\n", lID );
#endif
		return;
	}

	// [BB] If instructed to, check whether the actor is already playing something.
	// Note: The checking may not include CHAN_LOOP.
	if ( bRespectActorPlayingSomething && S_IsActorPlayingSomething (pActor, lChannel & (~CHAN_LOOP), S_FindSound ( pszSoundString ) ) )
		return;

	// Finally, play the sound.
	S_Sound( pActor, lChannel, pszSoundString, (float)lVolume / 127.f, NETWORK_AttenuationIntToFloat ( lAttenuation ) );
}

//*****************************************************************************
//
static void client_SoundPoint( BYTESTREAM_s *pByteStream )
{
	LONG		lChannel;
	LONG		lVolume;
	LONG		lAttenuation;
	fixed_t		X;
	fixed_t		Y;
	fixed_t		Z;

	// Read in the XY of the sound.
	X = NETWORK_ReadShort( pByteStream ) << FRACBITS;
	Y = NETWORK_ReadShort( pByteStream ) << FRACBITS;
	Z = NETWORK_ReadShort( pByteStream ) << FRACBITS;

	// Read in the channel.
	lChannel = NETWORK_ReadByte( pByteStream );

	// Read in the name of the sound to play.
	const char* pszSoundString = client_ReadBytestreamSound( pByteStream );

	// Read in the volume.
	lVolume = NETWORK_ReadByte( pByteStream );
	if ( lVolume > 127 )
		lVolume = 127;

	// Read in the attenuation.
	lAttenuation = NETWORK_ReadByte( pByteStream );

	// Finally, play the sound.
	S_Sound ( X, Y, Z, lChannel, S_FindSound(pszSoundString), (float)lVolume / 127.f, NETWORK_AttenuationIntToFloat ( lAttenuation ) );
}

//*****************************************************************************
//
static void client_AnnouncerSound( BYTESTREAM_s *pByteStream )
{
	const char* pszEntry = client_ReadBytestreamSound( pByteStream );
	ANNOUNCER_PlayEntry ( cl_announcer, pszEntry );
}

//*****************************************************************************
//
static void client_StartSectorSequence( BYTESTREAM_s *pByteStream )
{
	LONG		lSectorID;
	const char	*pszSequence;
	sector_t	*pSector;

	// Read in the sector ID.
	lSectorID = NETWORK_ReadShort( pByteStream );

	const int channel = NETWORK_ReadByte( pByteStream );

	// Read in the sound sequence to play.
	pszSequence = NETWORK_ReadString( pByteStream );

	const int modenum = NETWORK_ReadByte( pByteStream );

	// Make sure the sector ID is valid.
	if (( lSectorID >= 0 ) && ( lSectorID < numsectors ))
		pSector = &sectors[lSectorID];
	else
		return;

	// Finally, play the given sound sequence for this sector.
	SN_StartSequence( pSector, channel, pszSequence, modenum );
}

//*****************************************************************************
//
static void client_StopSectorSequence( BYTESTREAM_s *pByteStream )
{
	LONG		lSectorID;
	sector_t	*pSector;

	// Read in the sector ID.
	lSectorID = NETWORK_ReadShort( pByteStream );

	// Make sure the sector ID is valid.
	if (( lSectorID >= 0 ) && ( lSectorID < numsectors ))
		pSector = &sectors[lSectorID];
	else
		return;

	// Finally, stop the sound sequence for this sector.
	// [BB] We don't know which channel is supposed to stop, so just stop floor and ceiling for now.
	SN_StopSequence( pSector, CHAN_CEILING );
	SN_StopSequence( pSector, CHAN_FLOOR );
}

//*****************************************************************************
//
static void client_CallVote( BYTESTREAM_s *pByteStream )
{
	FString		command;
	FString		parameters;
	FString		reason;
	ULONG		ulVoteCaller;

	// Read in the vote starter.
	ulVoteCaller = NETWORK_ReadByte( pByteStream );

	// Read in the command.
	command = NETWORK_ReadString( pByteStream );

	// Read in the parameters.
	parameters = NETWORK_ReadString( pByteStream );
	
	// Read in the reason.
	reason = NETWORK_ReadString( pByteStream );

	// Begin the vote!
	CALLVOTE_BeginVote( command, parameters, reason, ulVoteCaller );
}

//*****************************************************************************
//
static void client_PlayerVote( BYTESTREAM_s *pByteStream )
{
	ULONG	ulPlayer;
	bool	bYes;

	// Read in the player making the vote.
	ulPlayer = NETWORK_ReadByte( pByteStream );

	// Did the player vote yes?
	bYes = !!NETWORK_ReadByte( pByteStream );

	if ( bYes )
		CALLVOTE_VoteYes( ulPlayer );
	else
		CALLVOTE_VoteNo( ulPlayer );
}

//*****************************************************************************
//
static void client_VoteEnded( BYTESTREAM_s *pByteStream )
{
	bool	bPassed;

	// Did the vote pass?
	bPassed = !!NETWORK_ReadByte( pByteStream );

	CALLVOTE_EndVote( bPassed );
}

//*****************************************************************************
//
static void client_MapLoad( BYTESTREAM_s *pByteStream )
{
	bool	bPlaying;
	const char	*pszMap;
	
	// Read in the lumpname of the map we're about to load.
	pszMap = NETWORK_ReadString( pByteStream );

	// Check to see if we have the map.
	if ( P_CheckIfMapExists( pszMap ))
	{
		// Save our demo recording status since G_InitNew resets it.
		bPlaying = CLIENTDEMO_IsPlaying( );

		// Start new level.
		G_InitNew( pszMap, false );

		// Restore our demo recording status.
		CLIENTDEMO_SetPlaying( bPlaying );

		// [BB] viewactive is set in G_InitNew
		// For right now, the view is not active.
		//viewactive = false;

		// Kill the console.
		C_HideConsole( );
	}
	else
		Printf( "client_MapLoad: Unknown map: %s\n", pszMap );
}

//*****************************************************************************
//
static void client_MapNew( BYTESTREAM_s *pByteStream )
{
	const char	*pszMapName;

	// Read in the new mapname the server is switching the level to.
	pszMapName = NETWORK_ReadString( pByteStream );

	// Clear out our local buffer.
	NETWORK_ClearBuffer( &g_LocalBuffer );

	// Back to the full console.
	gameaction = ga_fullconsole;

	// Also, the view is no longer active.
	viewactive = false;

	// [Dusk] We're also no longer in line at this point.
	JOINQUEUE_SetClientPositionInLine( -1 );

	Printf( "Connecting to %s\n%s\n", NETWORK_AddressToString( g_AddressServer ), pszMapName );

	// Update the connection state, and begin trying to reconnect.
	CLIENT_SetConnectionState( CTS_ATTEMPTINGCONNECTION );
	CLIENT_AttemptConnection( );
}

//*****************************************************************************
//
static void client_MapExit( BYTESTREAM_s *pByteStream )
{
	LONG	lPos;
	const char	*pszNextMap;

	// Read in the position we're supposed to spawn at (is this needed?).
	lPos = NETWORK_ReadByte( pByteStream );

	// Read in the next map.
	pszNextMap = NETWORK_ReadString( pByteStream );

	if (( gamestate == GS_FULLCONSOLE ) ||
		( gamestate == GS_INTERMISSION ))
	{
		return;
	}

	G_ChangeLevel( pszNextMap, lPos, true );
}

//*****************************************************************************
//
static void client_MapAuthenticate( BYTESTREAM_s *pByteStream )
{
	const char	*pszMapName;

	pszMapName = NETWORK_ReadString( pByteStream );

	// Nothing to do in demo mode.
	if ( CLIENTDEMO_IsPlaying( ))
		return;

	NETWORK_WriteByte( &g_LocalBuffer.ByteStream, CLC_AUTHENTICATELEVEL );

	// [BB] Send the name of the map we are authenticating, this allows the
	// server to check whether we try to authenticate the correct map.
	NETWORK_WriteString( &g_LocalBuffer.ByteStream, pszMapName );

	// Send a checksum of our verticies, linedefs, sidedefs, and sectors.
	CLIENT_AuthenticateLevel( pszMapName );
}

//*****************************************************************************
//
static void client_SetMapTime( BYTESTREAM_s *pByteStream )
{
	level.time = NETWORK_ReadLong( pByteStream );
}

//*****************************************************************************
//
static void client_SetMapNumKilledMonsters( BYTESTREAM_s *pByteStream )
{
	level.killed_monsters = NETWORK_ReadShort( pByteStream );
}

//*****************************************************************************
//
static void client_SetMapNumFoundItems( BYTESTREAM_s *pByteStream )
{
	level.found_items = NETWORK_ReadShort( pByteStream );
}

//*****************************************************************************
//
static void client_SetMapNumFoundSecrets( BYTESTREAM_s *pByteStream )
{
	level.found_secrets = NETWORK_ReadShort( pByteStream );
}

//*****************************************************************************
//
static void client_SetMapNumTotalMonsters( BYTESTREAM_s *pByteStream )
{
	level.total_monsters = NETWORK_ReadShort( pByteStream );
}

//*****************************************************************************
//
static void client_SetMapNumTotalItems( BYTESTREAM_s *pByteStream )
{
	level.total_items = NETWORK_ReadShort( pByteStream );
}

//*****************************************************************************
//
static void client_SetMapMusic( BYTESTREAM_s *pByteStream )
{
	const char	*pszMusicString;

	// Read in the music string.
	pszMusicString = NETWORK_ReadString( pByteStream );

	// Change the music.
	S_ChangeMusic( pszMusicString );
}

//*****************************************************************************
//
static void client_SetMapSky( BYTESTREAM_s *pByteStream )
{
	const char	*pszSky1;
	const char	*pszSky2;

	// Read in the texture name of the first sky.
	pszSky1 = NETWORK_ReadString( pByteStream );

	if ( pszSky1 != NULL )
	{
		strncpy( level.skypic1, pszSky1, 8 );
		sky1texture = TexMan.GetTexture( pszSky1, FTexture::TEX_Wall, FTextureManager::TEXMAN_Overridable );
	}

	// Read in the texture name of the second sky.
	pszSky2 = NETWORK_ReadString( pByteStream );

	if ( pszSky2 != NULL )
	{
		strncpy( level.skypic2, pszSky2, 8 );
		sky2texture = TexMan.GetTexture( pszSky2, FTexture::TEX_Wall, FTextureManager::TEXMAN_Overridable );
	}

	// Set some other sky properties.
	R_InitSkyMap( );
}

//*****************************************************************************
//
static void client_GiveInventory( BYTESTREAM_s *pByteStream )
{
	const PClass	*pType;
	ULONG			ulPlayer;
	USHORT			usActorNetworkIndex;
	LONG			lAmount;
	AInventory		*pInventory;

	// Read in the player ID.
	ulPlayer = NETWORK_ReadByte( pByteStream );

	// Read in the identification of the type of item to give.
	usActorNetworkIndex = NETWORK_ReadShort( pByteStream );

	// Read in the amount of this inventory type the player has.
	lAmount = NETWORK_ReadShort( pByteStream );

	// Check to make sure everything is valid. If not, break out.
	if (( PLAYER_IsValidPlayer( ulPlayer ) == false ) || ( players[ulPlayer].mo == NULL ))
		return;

	pType = NETWORK_GetClassFromIdentification( usActorNetworkIndex );
	if ( pType == NULL )
		return;

	// [BB] If pType is not derived from AInventory, the cast below will fail.
	if ( !(pType->IsDescendantOf( RUNTIME_CLASS( AInventory ))) )
		return;

	// [BB] Keep track of whether the player had a weapon.
	const bool playerHadNoWeapon = ( ( players[ulPlayer].ReadyWeapon == NULL ) &&  ( players[ulPlayer].PendingWeapon == WP_NOCHANGE ) );

	// Try to give the player the item.
	pInventory = static_cast<AInventory *>( Spawn( pType, 0, 0, 0, NO_REPLACE ));
	if ( pInventory != NULL )
	{
		if ( lAmount > 0 )
		{
			if ( pType->IsDescendantOf( RUNTIME_CLASS( ABasicArmorPickup )))
			{
				if ( static_cast<ABasicArmorPickup*>( pInventory )->SaveAmount != 0 )
					static_cast<ABasicArmorPickup*>( pInventory )->SaveAmount *= lAmount;
				else
					static_cast<ABasicArmorPickup*>( pInventory )->SaveAmount *= lAmount;
			}
			else if ( pType->IsDescendantOf( RUNTIME_CLASS( ABasicArmorBonus )))
			{
				static_cast<ABasicArmorBonus*>( pInventory )->SaveAmount *= lAmount;
				static_cast<ABasicArmorBonus*>( pInventory )->BonusCount *= lAmount;
			}
			else if ( pType->IsDescendantOf( RUNTIME_CLASS( AHealth ) ) )
			{
				if ( pInventory->MaxAmount > 0 )
					pInventory->Amount = MIN( lAmount, (LONG)pInventory->MaxAmount );
				else
					pInventory->Amount = lAmount;
			}
			else
				pInventory->Amount = lAmount;
		}
		if ( pInventory->CallTryPickup( players[ulPlayer].mo ) == false )
		{
			pInventory->Destroy( );
			pInventory = NULL;
		}
	}

	// If he still doesn't have the object after trying to give it to him... then YIKES!
	if ( pInventory == NULL )
	{
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_GiveInventory: Failed to give inventory type, %s!\n", NETWORK_GetClassNameFromIdentification( usActorNetworkIndex ));
#endif
		return;
	}

	// Don't count this towards the level statistics.
	if ( pInventory->flags & MF_COUNTITEM )
	{
		pInventory->flags &= ~MF_COUNTITEM;
		level.total_items--;
	}

	// Set the new amount of the inventory object.
	pInventory = players[ulPlayer].mo->FindInventory( pType );
	if ( pInventory )
		pInventory->Amount = lAmount;

	// [BC] For some weird reason, the KDIZD new pistol has the amount of 0 when
	// picked up, so we can't actually destroy items when the amount is 0 or less.
	// Doesn't seem to make any sense, but whatever.
/*
	if ( pInventory->Amount <= 0 )
	{
		// We can't actually destroy ammo, since it's vital for weapons.
		if ( pInventory->ItemFlags & IF_KEEPDEPLETED )
			pInventory->Amount = 0;
		// But, we can destroy everything else.
		else
			pInventory->Destroy( );
	}
*/
	// [BB] Prevent the client from trying to switch to a different weapon while morphed.
	if ( players[ulPlayer].morphTics )
		players[ulPlayer].PendingWeapon = WP_NOCHANGE;

	// Since an item displayed on the HUD may have been given, refresh the HUD.
	SCOREBOARD_RefreshHUD( );

	// [BB] If this is not "our" player and this player didn't have a weapon before, we assume
	// that he was just spawned and didn't tell the server yet which weapon he selected. In this
	// case make sure that this pickup doesn't cause him to bring up a weapon and wait for the
	// server to tell us which weapon the player uses.
	if ( playerHadNoWeapon  && ( players[ulPlayer].bIsBot == false )&& ( ulPlayer != (ULONG)consoleplayer ) )
		PLAYER_ClearWeapon ( &players[ulPlayer] );
}

//*****************************************************************************
//
static void client_TakeInventory( BYTESTREAM_s *pByteStream )
{
	const PClass	*pType;
	ULONG			ulPlayer;
	const char		*pszName;
	LONG			lAmount;
	AInventory		*pInventory;

	// Read in the player ID.
	ulPlayer = NETWORK_ReadByte( pByteStream );

	// Read in the name of the type of item to take away.
	pszName = NETWORK_ReadString( pByteStream );

	// Read in the new amount of this inventory type the player has.
	lAmount = NETWORK_ReadShort( pByteStream );

	// Check to make sure everything is valid. If not, break out.
	if (( PLAYER_IsValidPlayer( ulPlayer ) == false ) || ( players[ulPlayer].mo == NULL ))
		return;

	pType = PClass::FindClass( pszName );
	if ( pType == NULL )
		return;

	// Try to find this object within the player's personal inventory.
	pInventory = players[ulPlayer].mo->FindInventory( pType );

	// If the player doesn't have this type, give it to him.
	if ( pInventory == NULL )
		pInventory = players[ulPlayer].mo->GiveInventoryType( pType );

	// If he still doesn't have the object after trying to give it to him... then YIKES!
	if ( pInventory == NULL )
	{
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_TakeInventory: Failed to give inventory type, %s!\n", pszName );
#endif
		return;
	}

	// Set the new amount of the inventory object.
	pInventory->Amount = lAmount;
	if ( pInventory->Amount <= 0 )
	{
		if ( pInventory->ItemFlags & IF_KEEPDEPLETED )
			pInventory->Amount = 0;
		else
			pInventory->Destroy( );
	}

	// Since an item displayed on the HUD may have been taken away, refresh the HUD.
	SCOREBOARD_RefreshHUD( );
}

//*****************************************************************************
//
static void client_GivePowerup( BYTESTREAM_s *pByteStream )
{
	const PClass	*pType;
	ULONG			ulPlayer;
	USHORT			usActorNetworkIndex;
	LONG			lAmount;
	LONG			lEffectTics;
	AInventory		*pInventory;

	// Read in the player ID.
	ulPlayer = NETWORK_ReadByte( pByteStream );

	// Read in the identification of the type of item to give.
	usActorNetworkIndex = NETWORK_ReadShort( pByteStream );

	// Read in the amount of this inventory type the player has.
	lAmount = NETWORK_ReadShort( pByteStream );

	// Read in the amount of time left on this powerup.
	lEffectTics = NETWORK_ReadShort( pByteStream );

	// Check to make sure everything is valid. If not, break out.
	if (( PLAYER_IsValidPlayer( ulPlayer ) == false ) || ( players[ulPlayer].mo == NULL ))
		return;

	pType = NETWORK_GetClassFromIdentification( usActorNetworkIndex );
	if ( pType == NULL )
		return;

	// If this isn't a powerup, just quit.
	if ( pType->IsDescendantOf( RUNTIME_CLASS( APowerup )) == false )
		return;

	// Try to find this object within the player's personal inventory.
	pInventory = players[ulPlayer].mo->FindInventory( pType );

	// If the player doesn't have this type, give it to him.
	if ( pInventory == NULL )
		pInventory = players[ulPlayer].mo->GiveInventoryType( pType );

	// If he still doesn't have the object after trying to give it to him... then YIKES!
	if ( pInventory == NULL )
	{
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_TakeInventory: Failed to give inventory type, %s!\n", NETWORK_GetClassNameFromIdentification( usActorNetworkIndex ));
#endif
		return;
	}

	// Set the new amount of the inventory object.
	pInventory->Amount = lAmount;
	if ( pInventory->Amount <= 0 )
	{
		pInventory->Destroy( );
		pInventory = NULL;
	}

	if ( pInventory )
		static_cast<APowerup *>( pInventory )->EffectTics = lEffectTics;

	// Since an item displayed on the HUD may have been given, refresh the HUD.
	SCOREBOARD_RefreshHUD( );
}

//*****************************************************************************
//
static void client_DoInventoryPickup( BYTESTREAM_s *pByteStream )
{
	ULONG			ulPlayer;
	char			szClassName[64];
	const char		*pszPickupMessage;
	AInventory		*pInventory;

	static LONG			s_lLastMessageTic = 0;
	static char			s_szLastMessage[256];

	// Read in the player ID.
	ulPlayer = NETWORK_ReadByte( pByteStream );

	// Read in the class name of the item.
	sprintf( szClassName, "%s", NETWORK_ReadString( pByteStream ));

	// Read in the pickup message.
	pszPickupMessage = NETWORK_ReadString( pByteStream );

	// Check to make sure everything is valid. If not, break out.
	if (( PLAYER_IsValidPlayer( ulPlayer ) == false ) || ( players[ulPlayer].mo == NULL ))
		return;

	// If the player doesn't have this inventory item, break out.
	pInventory = static_cast<AInventory *>( Spawn( PClass::FindClass( szClassName ), 0, 0, 0, NO_REPLACE ));
	if ( pInventory == NULL )
		return;

	// Don't count this towards the level statistics.
	if ( pInventory->flags & MF_COUNTITEM )
	{
		// [BB] The server doesn't tell us about itemcount updates,
		// so try to keep track of this locally.
		players[ulPlayer].itemcount++;

		pInventory->flags &= ~MF_COUNTITEM;
		level.total_items--;
	}

	// Print out the pickup message.
	if (( players[ulPlayer].mo->CheckLocalView( consoleplayer )) &&
		(( s_lLastMessageTic != gametic ) || ( stricmp( s_szLastMessage, pszPickupMessage ) != 0 )))
	{
		s_lLastMessageTic = gametic;
		strcpy( s_szLastMessage, pszPickupMessage );

		// This code is from PrintPickupMessage().
		if ( pszPickupMessage != NULL )
		{
			if ( pszPickupMessage[0] == '$' )
				pszPickupMessage = GStrings( pszPickupMessage + 1 );

			Printf( PRINT_LOW, "%s\n", pszPickupMessage );
		}

		StatusBar->FlashCrosshair( );
	}

	// Play the inventory pickup sound and blend the screen.
	pInventory->PlayPickupSound( players[ulPlayer].mo );
	players[ulPlayer].bonuscount = BONUSADD;

	// Play the announcer pickup entry as well.
	if ( players[ulPlayer].mo->CheckLocalView( consoleplayer ))
		ANNOUNCER_PlayEntry( cl_announcer, pInventory->PickupAnnouncerEntry( ));

	// Finally, destroy the temporarily spawned inventory item.
	pInventory->Destroy( );
}

//*****************************************************************************
//
static void client_DestroyAllInventory( BYTESTREAM_s *pByteStream )
{
	ULONG			ulPlayer;

	// Read in the player ID.
	ulPlayer = NETWORK_ReadByte( pByteStream );

	// Check to make sure everything is valid. If not, break out.
	if (( PLAYER_IsValidPlayer( ulPlayer ) == false ) || ( players[ulPlayer].mo == NULL ))
		return;

	// Finally, destroy the player's inventory.
	// [BB] Be careful here, we may not use mo->DestroyAllInventory( ), otherwise
	// AHexenArmor messes up.
	DoClearInv ( players[ulPlayer].mo );
}

//*****************************************************************************
//
static void client_SetInventoryIcon( BYTESTREAM_s *pByteStream )
{
	const ULONG ulPlayer = NETWORK_ReadByte( pByteStream );
	const ULONG usActorNetworkIndex = NETWORK_ReadShort( pByteStream );
	const FString iconTexName = NETWORK_ReadString( pByteStream );

	// Check to make sure everything is valid. If not, break out.
	if (( PLAYER_IsValidPlayer( ulPlayer ) == false ) || ( players[ulPlayer].mo == NULL ))
		return;

	const PClass *pType = NETWORK_GetClassFromIdentification( usActorNetworkIndex );
	if ( pType == NULL )
		return;

	AInventory *pInventory = players[ulPlayer].mo->FindInventory( pType );

	if ( pInventory )
		pInventory->Icon = TexMan.GetTexture( iconTexName, 0 );
}

//*****************************************************************************
//
static void client_DoDoor( BYTESTREAM_s *pByteStream )
{
	LONG			lSectorID;
	sector_t		*pSector;
	BYTE			type;
	LONG			lSpeed;
	LONG			lDirection;
	LONG			lLightTag;
	LONG			lDoorID;
	DDoor			*pDoor;

	// Read in the sector ID.
	lSectorID = NETWORK_ReadShort( pByteStream );

	// Read in the door type.
	type = NETWORK_ReadByte( pByteStream );

	// Read in the speed.
	lSpeed = NETWORK_ReadLong( pByteStream );

	// Read in the direction.
	lDirection = NETWORK_ReadByte( pByteStream );

	// Read in the delay.
	lLightTag = NETWORK_ReadShort( pByteStream );

	// Read in the door ID.
	lDoorID = NETWORK_ReadShort( pByteStream );

	// Make sure the sector ID is valid.
	if (( lSectorID >= 0 ) && ( lSectorID < numsectors ))
		pSector = &sectors[lSectorID];
	else
		return;

	// Since we still want to receive direction as a byte, but -1 can't be represented in byte
	// form, adjust the value into something that can be represented.
	lDirection = CLIENT_AdjustDoorDirection( lDirection );
	if ( lDirection == INT_MAX )
		return;

	// If door already has a thinker, we can't spawn a new door on it.
	if ( pSector->ceilingdata )
	{
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_DoDoor: WARNING! Door's sector already has a ceiling mover attached to it!" );
#endif
		return;
	}

	// Create the new door.
	if ( (pDoor = new DDoor( pSector, (DDoor::EVlDoor)type, lSpeed, 0, lLightTag, g_ConnectionState != CTS_ACTIVE )) )
	{
		pDoor->SetID( lDoorID );
		pDoor->SetDirection( lDirection );
	}
}

//*****************************************************************************
//
static void client_DestroyDoor( BYTESTREAM_s *pByteStream )
{
	DDoor	*pDoor;
	LONG	lDoorID;

	// Read in the door ID.
	lDoorID = NETWORK_ReadShort( pByteStream );

	pDoor = P_GetDoorByID( lDoorID );
	if ( pDoor == NULL )
	{
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_DestroyDoor: Couldn't find door with ID: %d!\n", lDoorID );
#endif
		return;
	}

	pDoor->Destroy( );
}

//*****************************************************************************
//
static void client_ChangeDoorDirection( BYTESTREAM_s *pByteStream )
{
	DDoor	*pDoor;
	LONG	lDoorID;
	LONG	lDirection;

	// Read in the door ID.
	lDoorID = NETWORK_ReadShort( pByteStream );

	// Read in the new direction the door should move in.
	lDirection = NETWORK_ReadByte( pByteStream );

	// Since we still want to receive direction as a byte, but -1 can't be represented in byte
	// form, adjust the value into something that can be represented.
	lDirection = CLIENT_AdjustDoorDirection( lDirection );
	if ( lDirection == INT_MAX )
		return;

	pDoor = P_GetDoorByID( lDoorID );
	if ( pDoor == NULL )
	{
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_ChangeDoorDirection: Couldn't find door with ID: %d!\n", lDoorID );
#endif
		return;
	}

	pDoor->SetDirection( lDirection );

	// Don't play a sound if the door is now motionless!
	if ( lDirection != 0 )
		pDoor->DoorSound( lDirection == 1 );
}

//*****************************************************************************
//
static void client_DoFloor( BYTESTREAM_s *pByteStream )
{
	LONG			lType;
	LONG			lDirection;
	LONG			FloorDestDist;
	LONG			lSpeed;
	LONG			lSectorID;
	LONG			Crush;
	bool			Hexencrush;
	LONG			lFloorID;
	sector_t		*pSector;
	DFloor			*pFloor;

	// Read in the type of floor.
	lType = NETWORK_ReadByte( pByteStream );

	// Read in the sector ID.
	lSectorID = NETWORK_ReadShort( pByteStream );

	// Read in the direction of the floor.
	lDirection = NETWORK_ReadByte( pByteStream );

	// Read in the speed of the floor.
	lSpeed = NETWORK_ReadLong( pByteStream );

	// Read in the floor's destination height.
	FloorDestDist = NETWORK_ReadLong( pByteStream );

	// Read in the floor's crush.
	Crush = static_cast<SBYTE>( NETWORK_ReadByte( pByteStream ) );

	// Read in the floor's crush type.
	Hexencrush = NETWORK_ReadByte( pByteStream );

	// Read in the floor's network ID.
	lFloorID = NETWORK_ReadShort( pByteStream );

	// Since we still want to receive direction as a byte, but -1 can't be represented in byte
	// form, adjust the value into something that can be represented.
	lDirection = CLIENT_AdjustFloorDirection( lDirection );
	if ( lDirection == INT_MAX )
		return;

	// Invalid sector.
	if (( lSectorID >= numsectors ) || ( lSectorID < 0 ))
		return;

	pSector = &sectors[lSectorID];

	// If the sector already has activity, don't override it.
	if ( pSector->floordata )
		return;

	pFloor = new DFloor( pSector );
	pFloor->SetType( (DFloor::EFloor)lType );
	pFloor->SetCrush( Crush );
	pFloor->SetHexencrush( Hexencrush );
	pFloor->SetDirection( lDirection );
	pFloor->SetFloorDestDist( FloorDestDist );
	pFloor->SetSpeed( lSpeed );
	pFloor->SetID( lFloorID );
}

//*****************************************************************************
//
static void client_DestroyFloor( BYTESTREAM_s *pByteStream )
{
	DFloor		*pFloor;
	LONG		lFloorID;

	// Read in the floor ID.
	lFloorID = NETWORK_ReadShort( pByteStream );

	pFloor = P_GetFloorByID( lFloorID );
	if ( pFloor == NULL )
	{
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_ChangeFloorType: Couldn't find ceiling with ID: %d!\n", lFloorID );
#endif
		return;
	}

	SN_StopSequence( pFloor->GetSector( ), CHAN_FLOOR );
	pFloor->Destroy( );
}

//*****************************************************************************
//
static void client_ChangeFloorDirection( BYTESTREAM_s *pByteStream )
{
	DFloor		*pFloor;
	LONG		lFloorID;
	LONG		lDirection;

	// Read in the floor ID.
	lFloorID = NETWORK_ReadShort( pByteStream );

	// Read in the new floor direction.
	lDirection = NETWORK_ReadByte( pByteStream );

	// Since we still want to receive direction as a byte, but -1 can't be represented in byte
	// form, adjust the value into something that can be represented.
	lDirection = CLIENT_AdjustFloorDirection( lDirection );
	if ( lDirection == INT_MAX )
		return;

	pFloor = P_GetFloorByID( lFloorID );
	if ( pFloor == NULL )
	{
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_ChangeFloorType: Couldn't find ceiling with ID: %d!\n", lFloorID );
#endif
		return;
	}

	pFloor->SetDirection( lDirection );
}

//*****************************************************************************
//
static void client_ChangeFloorType( BYTESTREAM_s *pByteStream )
{
	DFloor		*pFloor;
	LONG		lFloorID;
	LONG		lType;

	// Read in the floor ID.
	lFloorID = NETWORK_ReadShort( pByteStream );

	// Read in the new type of floor this is.
	lType = NETWORK_ReadByte( pByteStream );

	pFloor = P_GetFloorByID( lFloorID );
	if ( pFloor == NULL )
	{
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_ChangeFloorType: Couldn't find ceiling with ID: %d!\n", lFloorID );
#endif
		return;
	}

	pFloor->SetType( (DFloor::EFloor)lType );
}

//*****************************************************************************
//
static void client_ChangeFloorDestDist( BYTESTREAM_s *pByteStream )
{
	DFloor		*pFloor;
	LONG		lFloorID;
	fixed_t		DestDist;

	// Read in the floor ID.
	lFloorID = NETWORK_ReadShort( pByteStream );

	// Read in the new floor destination distance.
	DestDist = NETWORK_ReadLong( pByteStream );

	pFloor = P_GetFloorByID( lFloorID );
	if ( pFloor == NULL )
	{
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_ChangeFloorType: Couldn't find ceiling with ID: %d!\n", lFloorID );
#endif
		return;
	}

	pFloor->SetFloorDestDist( DestDist );
}

//*****************************************************************************
//
static void client_StartFloorSound( BYTESTREAM_s *pByteStream )
{
	DFloor		*pFloor;
	LONG		lFloorID;

	// Read in the floor ID.
	lFloorID = NETWORK_ReadShort( pByteStream );

	pFloor = P_GetFloorByID( lFloorID );
	if ( pFloor == NULL )
	{
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_StartFloorSound: Couldn't find ceiling with ID: %d!\n", lFloorID );
#endif
		return;
	}

	// Finally, start playing the floor's sound sequence.
	pFloor->StartFloorSound( );
}

//*****************************************************************************
//
static void client_DoCeiling( BYTESTREAM_s *pByteStream )
{
	LONG			lType;
	fixed_t			BottomHeight;
	fixed_t			TopHeight;
	LONG			lSpeed;
	LONG			lCrush;
	bool			Hexencrush;
	LONG			lSilent;
	LONG			lDirection;
	LONG			lSectorID;
	LONG			lCeilingID;
	sector_t		*pSector;
	DCeiling		*pCeiling;

	// Read in the type of ceiling this is.
	lType = NETWORK_ReadByte( pByteStream );

	// Read in the sector this ceiling is attached to.
	lSectorID = NETWORK_ReadShort( pByteStream );

	// Read in the direction this ceiling is moving in.
	lDirection = NETWORK_ReadByte( pByteStream );

	// Read in the lowest distance the ceiling can travel before it stops.
	BottomHeight = NETWORK_ReadLong( pByteStream );

	// Read in the highest distance the ceiling can travel before it stops.
	TopHeight = NETWORK_ReadLong( pByteStream );

	// Read in the speed of the ceiling.
	lSpeed = NETWORK_ReadLong( pByteStream );

	// Does this ceiling damage those who get squashed by it?
	lCrush = static_cast<SBYTE>( NETWORK_ReadByte( pByteStream ) );

	// Is this ceiling crush Hexen style?
	Hexencrush = NETWORK_ReadByte( pByteStream );

	// Does this ceiling make noise?
	lSilent = NETWORK_ReadShort( pByteStream );

	// Read in the network ID of the ceiling.
	lCeilingID = NETWORK_ReadShort( pByteStream );

	// Since we still want to receive direction as a byte, but -1 can't be represented in byte
	// form, adjust the value into something that can be represented.
	lDirection = CLIENT_AdjustCeilingDirection( lDirection );
	if ( lDirection == INT_MAX )
		return;

	// Invalid sector.
	if (( lSectorID >= numsectors ) || ( lSectorID < 0 ))
		return;

	pSector = &sectors[lSectorID];

	pCeiling = new DCeiling( pSector, lSpeed, 0, lSilent );
	pCeiling->SetBottomHeight( BottomHeight );
	pCeiling->SetTopHeight( TopHeight );
	pCeiling->SetCrush( lCrush );
	pCeiling->SetHexencrush( Hexencrush );
	pCeiling->SetDirection( lDirection );
	pCeiling->SetID( lCeilingID );
}

//*****************************************************************************
//
static void client_DestroyCeiling( BYTESTREAM_s *pByteStream )
{
	DCeiling	*pCeiling;
	LONG		lCeilingID;

	// Read in the ceiling ID.
	lCeilingID = NETWORK_ReadShort( pByteStream );

	pCeiling = P_GetCeilingByID( lCeilingID );
	if ( pCeiling == NULL )
	{
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_DestroyCeiling: Couldn't find ceiling with ID: %d!\n", lCeilingID );
#endif
		return;
	}

	SN_StopSequence( pCeiling->GetSector( ), CHAN_CEILING );
	pCeiling->Destroy( );
}

//*****************************************************************************
//
static void client_ChangeCeilingDirection( BYTESTREAM_s *pByteStream )
{
	DCeiling	*pCeiling;
	LONG		lCeilingID;
	LONG		lDirection;

	// Read in the ceiling ID.
	lCeilingID = NETWORK_ReadShort( pByteStream );

	// Read in the new ceiling direction.
	lDirection = NETWORK_ReadByte( pByteStream );

	// Since we still want to receive direction as a byte, but -1 can't be represented in byte
	// form, adjust the value into something that can be represented.
	lDirection = CLIENT_AdjustCeilingDirection( lDirection );
	if ( lDirection == INT_MAX )
		return;

	pCeiling = P_GetCeilingByID( lCeilingID );
	if ( pCeiling == NULL )
	{
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_ChangeCeilingDirection: Couldn't find ceiling with ID: %d!\n", lCeilingID );
#endif
		return;
	}

	// Finally, set the new ceiling direction.
	pCeiling->SetDirection( lDirection );
}

//*****************************************************************************
//
static void client_ChangeCeilingSpeed( BYTESTREAM_s *pByteStream )
{
	DCeiling	*pCeiling;
	LONG		lCeilingID;
	LONG		lSpeed;

	// Read in the ceiling ID.
	lCeilingID = NETWORK_ReadShort( pByteStream );

	// Read in the new ceiling speed.
	lSpeed = NETWORK_ReadLong( pByteStream );

	pCeiling = P_GetCeilingByID( lCeilingID );
	if ( pCeiling == NULL )
	{
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_ChangeCeilingSpeed: Couldn't find ceiling with ID: %d!\n", lCeilingID );
#endif
		return;
	}

	pCeiling->SetSpeed( lSpeed );
}

//*****************************************************************************
//
static void client_PlayCeilingSound( BYTESTREAM_s *pByteStream )
{
	DCeiling	*pCeiling;
	LONG		lCeilingID;

	// Read in the ceiling ID.
	lCeilingID = NETWORK_ReadShort( pByteStream );

	pCeiling = P_GetCeilingByID( lCeilingID );
	if ( pCeiling == NULL )
	{
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_PlayCeilingSound: Couldn't find ceiling with ID: %d!\n", lCeilingID );
#endif
		return;
	}

	pCeiling->PlayCeilingSound( );
}

//*****************************************************************************
//
static void client_DoPlat( BYTESTREAM_s *pByteStream )
{
	LONG			lType;
	LONG			lStatus;
	fixed_t			High;
	fixed_t			Low;
	LONG			lSpeed;
	LONG			lSectorID;
	LONG			lPlatID;
	sector_t		*pSector;
	DPlat			*pPlat;

	// Read in the type of plat.
	lType = NETWORK_ReadByte( pByteStream );

	// Read in the sector ID.
	lSectorID = NETWORK_ReadShort( pByteStream );

	// Read in the plat status (moving up, down, etc.).
	lStatus = NETWORK_ReadByte( pByteStream );

	// Read in the high range of the plat.
	High = NETWORK_ReadLong( pByteStream );

	// Read in the low range of the plat.
	Low = NETWORK_ReadLong( pByteStream );

	// Read in the speed.
	lSpeed = NETWORK_ReadLong( pByteStream );

	// Read in the plat ID.
	lPlatID = NETWORK_ReadShort( pByteStream );

	// Invalid sector.
	if (( lSectorID >= numsectors ) || ( lSectorID < 0 ))
		return;

	pSector = &sectors[lSectorID];

	// Create the plat, and set all its attributes that were read in.
	pPlat = new DPlat( pSector );
	pPlat->SetType( (DPlat::EPlatType)lType );
	pPlat->SetStatus( lStatus );
	pPlat->SetHigh( High );
	pPlat->SetLow( Low );
	pPlat->SetSpeed( lSpeed );
	pPlat->SetID( lPlatID );

	// Now, set other properties that don't really matter.
	pPlat->SetCrush( -1 );
	pPlat->SetTag( 0 );

	// Just set the delay to 0. The server will tell us when it should move again.
	pPlat->SetDelay( 0 );
}

//*****************************************************************************
//
static void client_DestroyPlat( BYTESTREAM_s *pByteStream )
{
	DPlat	*pPlat;
	LONG	lPlatID;

	// Read in the plat ID.
	lPlatID = NETWORK_ReadShort( pByteStream );

	pPlat = P_GetPlatByID( lPlatID );
	if ( pPlat == NULL )
	{
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_DestroyPlat: Couldn't find plat with ID: %d!\n", lPlatID );
#endif
		return;
	}

	pPlat->Destroy( );
}

//*****************************************************************************
//
static void client_ChangePlatStatus( BYTESTREAM_s *pByteStream )
{
	DPlat	*pPlat;
	LONG	lPlatID;
	LONG	lStatus;

	// Read in the plat ID.
	lPlatID = NETWORK_ReadShort( pByteStream );

	// Read in the direction (aka status).
	lStatus = NETWORK_ReadByte( pByteStream );

	pPlat = P_GetPlatByID( lPlatID );
	if ( pPlat == NULL )
	{
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_ChangePlatStatus: Couldn't find plat with ID: %d!\n", lPlatID );
#endif
		return;
	}

	pPlat->SetStatus( lStatus );
}

//*****************************************************************************
//
static void client_PlayPlatSound( BYTESTREAM_s *pByteStream )
{
	DPlat	*pPlat;
	LONG	lPlatID;
	LONG	lSoundType;

	// Read in the plat ID.
	lPlatID = NETWORK_ReadShort( pByteStream );

	// Read in the type of sound to be played.
	lSoundType = NETWORK_ReadByte( pByteStream );

	pPlat = P_GetPlatByID( lPlatID );
	if ( pPlat == NULL )
	{
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_PlayPlatSound: Couldn't find plat with ID: %d!\n", lPlatID );
#endif
		return;
	}

	switch ( lSoundType )
	{
	case 0:

		SN_StopSequence( pPlat->GetSector( ), CHAN_FLOOR );
		break;
	case 1:

		pPlat->PlayPlatSound( "Platform" );
		break;
	case 2:

		SN_StartSequence( pPlat->GetSector( ), CHAN_FLOOR, "Silence", 0 );
		break;
	case 3:

		pPlat->PlayPlatSound( "Floor" );
		break;
	}
}

//*****************************************************************************
//
static void client_DoElevator( BYTESTREAM_s *pByteStream )
{
	LONG			lType;
	LONG			lSectorID;
	LONG			lSpeed;
	LONG			lDirection;
	LONG			lFloorDestDist;
	LONG			lCeilingDestDist;
	LONG			lElevatorID;
	sector_t		*pSector;
	DElevator		*pElevator;

	// Read in the type of elevator.
	lType = NETWORK_ReadByte( pByteStream );

	// Read in the sector ID.
	lSectorID = NETWORK_ReadShort( pByteStream );

	// Read in the speed.
	lSpeed = NETWORK_ReadLong( pByteStream );

	// Read in the direction.
	lDirection = NETWORK_ReadByte( pByteStream );

	// Read in the floor's destination distance.
	lFloorDestDist = NETWORK_ReadLong( pByteStream );

	// Read in the ceiling's destination distance.
	lCeilingDestDist = NETWORK_ReadLong( pByteStream );

	// Read in the elevator ID.
	lElevatorID = NETWORK_ReadShort( pByteStream );

	// Invalid sector.
	if (( lSectorID >= numsectors ) || ( lSectorID < 0 ))
		return;

	// Since we still want to receive direction as a byte, but -1 can't be represented in byte
	// form, adjust the value into something that can be represented.
	lDirection = CLIENT_AdjustElevatorDirection( lDirection );
	if ( lDirection == INT_MAX )
		return;

	pSector = &sectors[lSectorID];

	// Create the elevator, and set all its attributes that were read in.
	pElevator = new DElevator( pSector );
	pElevator->SetType( (DElevator::EElevator)lType );
	pElevator->SetSpeed( lSpeed );
	pElevator->SetDirection( lDirection );
	pElevator->SetFloorDestDist( lFloorDestDist );
	pElevator->SetCeilingDestDist( lCeilingDestDist );
	pElevator->SetID( lElevatorID );
}

//*****************************************************************************
//
static void client_DestroyElevator( BYTESTREAM_s *pByteStream )
{
	LONG		lElevatorID;
	DElevator	*pElevator;

	// Read in the elevator ID.
	lElevatorID = NETWORK_ReadShort( pByteStream );

	pElevator = P_GetElevatorByID( lElevatorID );
	if ( pElevator == NULL )
	{
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_DestroyElevator: Couldn't find elevator with ID: %d!\n", lElevatorID );
#endif
		return;
	}

	/* [BB] I think ZDoom does all this is Destroy now.
	pElevator->GetSector( )->floordata = NULL;
	pElevator->GetSector( )->ceilingdata = NULL;
	stopinterpolation( INTERP_SectorFloor, pElevator->GetSector( ));
	stopinterpolation( INTERP_SectorCeiling, pElevator->GetSector( ));
	*/

	// Finally, destroy the elevator.
	pElevator->Destroy( );
}

//*****************************************************************************
//
static void client_StartElevatorSound( BYTESTREAM_s *pByteStream )
{
	LONG		lElevatorID;
	DElevator	*pElevator;

	// Read in the elevator ID.
	lElevatorID = NETWORK_ReadShort( pByteStream );

	pElevator = P_GetElevatorByID( lElevatorID );
	if ( pElevator == NULL )
	{
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_StartElevatorSound: Couldn't find elevator with ID: %d!\n", lElevatorID );
#endif
		return;
	}

	// Finally, start the elevator sound.
	pElevator->StartFloorSound( );
}

//*****************************************************************************
//
static void client_DoPillar( BYTESTREAM_s *pByteStream )
{
	LONG			lType;
	LONG			lSectorID;
	LONG			lFloorSpeed;
	LONG			lCeilingSpeed;
	LONG			lFloorTarget;
	LONG			lCeilingTarget;
	LONG			Crush;
	bool			Hexencrush;
	LONG			lPillarID;
	sector_t		*pSector;
	DPillar			*pPillar;

	// Read in the type of pillar.
	lType = NETWORK_ReadByte( pByteStream );

	// Read in the sector ID.
	lSectorID = NETWORK_ReadShort( pByteStream );

	// Read in the speeds.
	lFloorSpeed = NETWORK_ReadLong( pByteStream );
	lCeilingSpeed = NETWORK_ReadLong( pByteStream );

	// Read in the targets.
	lFloorTarget = NETWORK_ReadLong( pByteStream );
	lCeilingTarget = NETWORK_ReadLong( pByteStream );

	// Read in the crush info.
	Crush = static_cast<SBYTE>( NETWORK_ReadByte( pByteStream ) );
	Hexencrush = NETWORK_ReadByte( pByteStream );

	// Read in the pillar ID.
	lPillarID = NETWORK_ReadShort( pByteStream );

	// Invalid sector.
	if (( lSectorID >= numsectors ) || ( lSectorID < 0 ))
		return;

	pSector = &sectors[lSectorID];

	// Create the pillar, and set all its attributes that were read in.
	pPillar = new DPillar( pSector );
	pPillar->SetType( (DPillar::EPillar)lType );
	pPillar->SetFloorSpeed( lFloorSpeed );
	pPillar->SetCeilingSpeed( lCeilingSpeed );
	pPillar->SetFloorTarget( lFloorTarget );
	pPillar->SetCeilingTarget( lCeilingTarget );
	pPillar->SetCrush( Crush );
	pPillar->SetHexencrush( Hexencrush );
	pPillar->SetID( lPillarID );

	// Begin playing the sound sequence for the pillar.
	if ( pSector->seqType >= 0 )
		SN_StartSequence( pSector, CHAN_FLOOR, pSector->seqType, SEQ_PLATFORM, 0 );
	else
		SN_StartSequence( pSector, CHAN_FLOOR, "Floor", 0 );
}

//*****************************************************************************
//
static void client_DestroyPillar( BYTESTREAM_s *pByteStream )
{
	LONG		lPillarID;
	DPillar		*pPillar;

	// Read in the elevator ID.
	lPillarID = NETWORK_ReadShort( pByteStream );

	pPillar = P_GetPillarByID( lPillarID );
	if ( pPillar == NULL )
	{
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_DestroyPillar: Couldn't find pillar with ID: %d!\n", lPillarID );
#endif
		return;
	}

	// Finally, destroy the pillar.
	pPillar->Destroy( );
}

//*****************************************************************************
//
static void client_DoWaggle( BYTESTREAM_s *pByteStream )
{
	bool			bCeiling;
	LONG			lSectorID;
	LONG			lOriginalDistance;
	LONG			lAccumulator;
	LONG			lAccelerationDelta;
	LONG			lTargetScale;
	LONG			lScale;
	LONG			lScaleDelta;
	LONG			lTicker;
	LONG			lState;
	LONG			lWaggleID;
	sector_t		*pSector;
	DWaggleBase		*pWaggle;

	// Read in whether or not this is a ceiling waggle.
	bCeiling = !!NETWORK_ReadByte( pByteStream );

	// Read in the sector ID.
	lSectorID = NETWORK_ReadShort( pByteStream );

	// Read in the waggle's attributes.
	lOriginalDistance = NETWORK_ReadLong( pByteStream );
	lAccumulator = NETWORK_ReadLong( pByteStream );
	lAccelerationDelta = NETWORK_ReadLong( pByteStream );
	lTargetScale = NETWORK_ReadLong( pByteStream );
	lScale = NETWORK_ReadLong( pByteStream );
	lScaleDelta = NETWORK_ReadLong( pByteStream );
	lTicker = NETWORK_ReadLong( pByteStream );

	// Read in the state the waggle is in.
	lState = NETWORK_ReadByte( pByteStream );

	// Read in the waggle ID.
	lWaggleID = NETWORK_ReadShort( pByteStream );

	// Invalid sector.
	if (( lSectorID >= numsectors ) || ( lSectorID < 0 ))
		return;

	pSector = &sectors[lSectorID];

	// Create the waggle, and set all its attributes that were read in.
	if ( bCeiling )
		pWaggle = static_cast<DWaggleBase *>( new DCeilingWaggle( pSector ));
	else
		pWaggle = static_cast<DWaggleBase *>( new DFloorWaggle( pSector ));
	pWaggle->SetOriginalDistance( lOriginalDistance );
	pWaggle->SetAccumulator( lAccumulator );
	pWaggle->SetAccelerationDelta( lAccelerationDelta );
	pWaggle->SetTargetScale( lTargetScale );
	pWaggle->SetScale( lScale );
	pWaggle->SetScaleDelta( lScaleDelta );
	pWaggle->SetTicker( lTicker );
	pWaggle->SetState( lState );
	pWaggle->SetID( lWaggleID );
}

//*****************************************************************************
//
static void client_DestroyWaggle( BYTESTREAM_s *pByteStream )
{
	LONG			lWaggleID;
	DWaggleBase		*pWaggle;

	// Read in the waggle ID.
	lWaggleID = NETWORK_ReadShort( pByteStream );

	pWaggle = P_GetWaggleByID( lWaggleID );
	if ( pWaggle == NULL )
	{
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_DestroyWaggle: Couldn't find waggle with ID: %d!\n", lWaggleID );
#endif
		return;
	}

	// Finally, destroy the waggle.
	pWaggle->Destroy( );
}

//*****************************************************************************
//
static void client_UpdateWaggle( BYTESTREAM_s *pByteStream )
{
	LONG			lWaggleID;
	LONG			lAccumulator;
	DWaggleBase		*pWaggle;

	// Read in the waggle ID.
	lWaggleID = NETWORK_ReadShort( pByteStream );

	// Read in the waggle's accumulator.
	lAccumulator = NETWORK_ReadLong( pByteStream );

	pWaggle = P_GetWaggleByID( lWaggleID );
	if ( pWaggle == NULL )
	{
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_DestroyWaggle: Couldn't find waggle with ID: %d!\n", lWaggleID );
#endif
		return;
	}

	// Finally, update the waggle's accumulator.
	pWaggle->SetAccumulator( lAccumulator );
}

//*****************************************************************************
//
static void client_DoRotatePoly( BYTESTREAM_s *pByteStream )
{
	LONG			lSpeed;
	LONG			lPolyNum;
	FPolyObj		*pPoly;
	DRotatePoly		*pRotatePoly;

	// Read in the speed.
	lSpeed = NETWORK_ReadLong( pByteStream );

	// Read in the polyobject ID.
	lPolyNum = NETWORK_ReadShort( pByteStream );

	// Make sure the polyobj exists before we try to work with it.
	pPoly = PO_GetPolyobj( lPolyNum );
	if ( pPoly == NULL )
	{
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_DoRotatePoly: Invalid polyobj number: %d\n", lPolyNum );
#endif
		return;
	}

	// Create the polyobject.
	pRotatePoly = new DRotatePoly( lPolyNum );
	pRotatePoly->SetSpeed( lSpeed );

	// Attach the new polyobject to this ID.
	pPoly->specialdata = pRotatePoly;

	// Also, start the sound sequence associated with this polyobject.
	SN_StartSequence( pPoly, pPoly->seqType, SEQ_DOOR, 0 );
}

//*****************************************************************************
//
static void client_DestroyRotatePoly( BYTESTREAM_s *pByteStream )
{
	LONG							lID;
	DRotatePoly						*pPoly;
	DRotatePoly						*pTempPoly;
	TThinkerIterator<DRotatePoly>	Iterator;

	// Read in the DRotatePoly ID.
	lID = NETWORK_ReadShort( pByteStream );

	// Try to find the object from the ID. If it exists, destroy it.
	pPoly = NULL;
	while ( (pTempPoly = Iterator.Next( )) )
	{
		if ( pTempPoly->GetPolyObj( ) == lID )
		{
			pPoly = pTempPoly;
			break;
		}
	}

	if ( pPoly )
		pPoly->Destroy( );
}

//*****************************************************************************
//
static void client_DoMovePoly( BYTESTREAM_s *pByteStream )
{
	LONG			lXSpeed;
	LONG			lYSpeed;
	LONG			lPolyNum;
	FPolyObj		*pPoly;
	DMovePoly		*pMovePoly;

	// Read in the speed.
	lXSpeed = NETWORK_ReadLong( pByteStream );
	lYSpeed = NETWORK_ReadLong( pByteStream );

	// Read in the polyobject ID.
	lPolyNum = NETWORK_ReadShort( pByteStream );

	// Make sure the polyobj exists before we try to work with it.
	pPoly = PO_GetPolyobj( lPolyNum );
	if ( pPoly == NULL )
	{
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_DoRotatePoly: Invalid polyobj number: %d\n", lPolyNum );
#endif
		return;
	}

	// Create the polyobject.
	pMovePoly = new DMovePoly( lPolyNum );
	pMovePoly->SetXSpeed( lXSpeed );
	pMovePoly->SetYSpeed( lYSpeed );

	// Attach the new polyobject to this ID.
	pPoly->specialdata = pMovePoly;

	// Also, start the sound sequence associated with this polyobject.
	SN_StartSequence( pPoly, pPoly->seqType, SEQ_DOOR, 0 );
}

//*****************************************************************************
//
static void client_DestroyMovePoly( BYTESTREAM_s *pByteStream )
{
	LONG							lID;
	DMovePoly						*pPoly;
	DMovePoly						*pTempPoly;
	TThinkerIterator<DMovePoly>		Iterator;

	// Read in the DMovePoly ID.
	lID = NETWORK_ReadShort( pByteStream );

	// Try to find the object from the ID. If it exists, destroy it.
	pPoly = NULL;
	while ( (pTempPoly = Iterator.Next( )) )
	{
		if ( pTempPoly->GetPolyObj( ) == lID )
		{
			pPoly = pTempPoly;
			break;
		}
	}

	if ( pPoly )
		pPoly->Destroy( );
}

//*****************************************************************************
//
static void client_DoPolyDoor( BYTESTREAM_s *pByteStream )
{
	LONG			lType;
	LONG			lXSpeed;
	LONG			lYSpeed;
	LONG			lSpeed;
	LONG			lPolyNum;
	FPolyObj		*pPoly;
	DPolyDoor		*pPolyDoor;

	// Read in the type of poly door (swing or slide).
	lType = NETWORK_ReadByte( pByteStream );

	// Read in the speed.
	lXSpeed = NETWORK_ReadLong( pByteStream );
	lYSpeed = NETWORK_ReadLong( pByteStream );
	lSpeed = NETWORK_ReadLong( pByteStream );

	// Read in the polyobject ID.
	lPolyNum = NETWORK_ReadShort( pByteStream );

	// Make sure the polyobj exists before we try to work with it.
	pPoly = PO_GetPolyobj( lPolyNum );
	if ( pPoly == NULL )
	{
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_DoPolyDoor: Invalid polyobj number: %d\n", lPolyNum );
#endif
		return;
	}

	// Create the polyobject.
	pPolyDoor = new DPolyDoor( lPolyNum, (podoortype_t)lType );
	pPolyDoor->SetXSpeed( lXSpeed );
	pPolyDoor->SetYSpeed( lYSpeed );
	pPolyDoor->SetSpeed( lSpeed );

	// Attach the new polyobject to this ID.
	pPoly->specialdata = pPolyDoor;
}

//*****************************************************************************
//
static void client_DestroyPolyDoor( BYTESTREAM_s *pByteStream )
{
	LONG							lID;
	DPolyDoor						*pPoly;
	DPolyDoor						*pTempPoly;
	TThinkerIterator<DPolyDoor>		Iterator;

	// Read in the DPolyDoor ID.
	lID = NETWORK_ReadShort( pByteStream );

	// Try to find the object from the ID. If it exists, destroy it.
	pPoly = NULL;
	while ( (pTempPoly = Iterator.Next( )) )
	{
		if ( pTempPoly->GetPolyObj( ) == lID )
		{
			pPoly = pTempPoly;
			break;
		}
	}

	if ( pPoly )
		pPoly->Destroy( );
}

//*****************************************************************************
//
static void client_SetPolyDoorSpeedPosition( BYTESTREAM_s *pByteStream )
{
	LONG			lPolyID;
	LONG			lXSpeed;
	LONG			lYSpeed;
	LONG			lX;
	LONG			lY;
	FPolyObj		*pPoly;
	LONG			lDeltaX;
	LONG			lDeltaY;

	// Read in the polyobject ID.
	lPolyID = NETWORK_ReadShort( pByteStream );

	// Read in the polyobject x/yspeed.
	lXSpeed = NETWORK_ReadLong( pByteStream );
	lYSpeed = NETWORK_ReadLong( pByteStream );

	// Read in the polyobject X/.
	lX = NETWORK_ReadLong( pByteStream );
	lY = NETWORK_ReadLong( pByteStream );

	pPoly = PO_GetPolyobj( lPolyID );
	if ( pPoly == NULL )
		return;

	lDeltaX = lX - pPoly->StartSpot.x;
	lDeltaY = lY - pPoly->StartSpot.y;

	pPoly->MovePolyobj( lDeltaX, lDeltaY );
	
	if ( pPoly->specialdata == NULL )
		return;

	static_cast<DPolyDoor *>( pPoly->specialdata )->SetXSpeed( lXSpeed );
	static_cast<DPolyDoor *>( pPoly->specialdata )->SetYSpeed( lYSpeed );
}

//*****************************************************************************
//
static void client_SetPolyDoorSpeedRotation( BYTESTREAM_s *pByteStream )
{
	LONG			lPolyID;
	LONG			lSpeed;
	LONG			lAngle;
	FPolyObj		*pPoly;
	LONG			lDeltaAngle;

	// Read in the polyobject ID.
	lPolyID = NETWORK_ReadShort( pByteStream );

	// Read in the polyobject speed.
	lSpeed = NETWORK_ReadLong( pByteStream );

	// Read in the polyobject angle.
	lAngle = NETWORK_ReadLong( pByteStream );

	pPoly = PO_GetPolyobj( lPolyID );
	if ( pPoly == NULL )
		return;

	lDeltaAngle = lAngle - pPoly->angle;

	pPoly->RotatePolyobj( lDeltaAngle );

	if ( pPoly->specialdata == NULL )
		return;

	static_cast<DPolyDoor *>( pPoly->specialdata )->SetSpeed( lSpeed );
}

//*****************************************************************************
//
static void client_PlayPolyobjSound( BYTESTREAM_s *pByteStream )
{
	LONG		lID;
	bool		PolyMode;
	FPolyObj	*pPoly;

	// Read in the polyobject ID.
	lID = NETWORK_ReadShort( pByteStream );

	// Read in the polyobject mode.
	PolyMode = NETWORK_ReadByte( pByteStream );

	pPoly = PO_GetPolyobj( lID );
	if ( pPoly == NULL )
		return;

	SN_StartSequence( pPoly, pPoly->seqType, SEQ_DOOR, PolyMode );
}

//*****************************************************************************
//
static void client_StopPolyobjSound( BYTESTREAM_s *pByteStream )
{
	LONG		lID;
	FPolyObj	*pPoly;

	// Read in the polyobject ID.
	lID = NETWORK_ReadShort( pByteStream );

	pPoly = PO_GetPolyobj( lID );
	if ( pPoly == NULL )
		return;

	SN_StopSequence( pPoly );
}

//*****************************************************************************
//
static void client_SetPolyobjPosition( BYTESTREAM_s *pByteStream )
{
	LONG			lPolyNum;
	FPolyObj		*pPoly;
	LONG			lX;
	LONG			lY;
	LONG			lDeltaX;
	LONG			lDeltaY;

	// Read in the polyobject number.
	lPolyNum = NETWORK_ReadShort( pByteStream );

	// Read in the XY position of the polyobj.
	lX = NETWORK_ReadLong( pByteStream );
	lY = NETWORK_ReadLong( pByteStream );

	// Get the polyobject from the index given.
	pPoly = PO_GetPolyobj( lPolyNum );
	if ( pPoly == NULL )
	{
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_SetPolyobjPosition: Invalid polyobj number: %d\n", lPolyNum );
#endif
		return;
	}

	lDeltaX = lX - pPoly->StartSpot.x;
	lDeltaY = lY - pPoly->StartSpot.y;

//	Printf( "DeltaX: %d\nDeltaY: %d\n", lDeltaX, lDeltaY );

	// Finally, set the polyobject action.
	pPoly->MovePolyobj( lDeltaX, lDeltaY );
}

//*****************************************************************************
//
static void client_SetPolyobjRotation( BYTESTREAM_s *pByteStream )
{
	LONG			lPolyNum;
	FPolyObj		*pPoly;
	LONG			lAngle;
	LONG			lDeltaAngle;

	// Read in the polyobject number.
	lPolyNum = NETWORK_ReadShort( pByteStream );

	// Read in the angle of the polyobj.
	lAngle = NETWORK_ReadLong( pByteStream );

	// Make sure the polyobj exists before we try to work with it.
	pPoly = PO_GetPolyobj( lPolyNum );
	if ( pPoly == NULL )
	{
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_SetPolyobjRotation: Invalid polyobj number: %d\n", lPolyNum );
#endif
		return;
	}

	lDeltaAngle = lAngle - pPoly->angle;

	// Finally, set the polyobject action.
	pPoly->RotatePolyobj( lDeltaAngle );
}

//*****************************************************************************
//
static void client_EarthQuake( BYTESTREAM_s *pByteStream )
{
	AActor	*pCenter;
	LONG	lID;
	LONG	lIntensity;
	LONG	lDuration;
	LONG	lTremorRadius;

	// Read in the center's network ID.
	lID = NETWORK_ReadShort( pByteStream );

	// Read in the intensity of the quake.
	lIntensity = NETWORK_ReadByte( pByteStream );

	// Read in the duration of the quake.
	lDuration = NETWORK_ReadShort( pByteStream );

	// Read in the tremor radius of the quake.
	lTremorRadius = NETWORK_ReadShort( pByteStream );

	// [BB] Read in the quake sound.
	FSoundID quakesound = NETWORK_ReadString( pByteStream );

	// Find the actor that represents the center of the quake based on the network
	// ID sent. If we can't find the actor, then the quake has no center.
	pCenter = CLIENT_FindThingByNetID( lID );
	if ( pCenter == NULL )
		return;

	// Create the earthquake. Since this is client-side, damage is always 0.
	new DEarthquake( pCenter, lIntensity, lDuration, 0, lTremorRadius, quakesound );
}

//*****************************************************************************
//
static void client_SetQueuePosition( BYTESTREAM_s *pByteStream )
{
	LONG	lPosition;

	// Read in our position in the join queue.
	lPosition = NETWORK_ReadByte( pByteStream );

	// [BB] We were removed from the queue for whatever reason.
	if ( lPosition == 255 )
	{
		JOINQUEUE_SetClientPositionInLine( -1 );
		return;
	}
	// [BB] This should never happen!
	else if ( lPosition == MAXPLAYERS )
	{
		Printf( "Join queue full!\n" );
		JOINQUEUE_SetClientPositionInLine( -1 );
		return;
	}
	else
		Printf( "Your position in line is: %d\n", static_cast<int> (lPosition + 1) );

	// Update the joinqueue module with our position in line.
	JOINQUEUE_SetClientPositionInLine( lPosition );
}

//*****************************************************************************
//
static void client_DoScroller( BYTESTREAM_s *pByteStream )
{
	DScroller::EScrollType	Type;
	fixed_t					dX;
	fixed_t					dY;
	LONG					lAffectee;

	// Read in the type of scroller.
	Type = (DScroller::EScrollType)NETWORK_ReadByte( pByteStream );

	// Read in the X speed.
	dX = NETWORK_ReadLong( pByteStream );

	// Read in the Y speed.
	dY = NETWORK_ReadLong( pByteStream );

	// Read in the sector/side being scrolled.
	lAffectee = NETWORK_ReadShort( pByteStream );

	// Check to make sure what we've read in is valid.
	// [BB] sc_side is allowed, too, but we need to make a different check for it.
	if (( Type != DScroller::sc_floor ) && ( Type != DScroller::sc_ceiling ) &&
		( Type != DScroller::sc_carry ) && ( Type != DScroller::sc_carry_ceiling ) && ( Type != DScroller::sc_side ) )
	{
		Printf( "client_DoScroller: Unknown type: %d!\n", static_cast<int> (Type) );
		return;
	}

	if( Type == DScroller::sc_side )
	{
		if (( lAffectee < 0 ) || ( lAffectee >= numsides ))
		{
#ifdef CLIENT_WARNING_MESSAGES
			Printf( "client_DoScroller: Invalid side ID: %d!\n", lAffectee );
#endif
			return;
		}
	}
	else if (( lAffectee < 0 ) || ( lAffectee >= numsectors ))
	{
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "client_DoScroller: Invalid sector ID: %d!\n", lAffectee );
#endif
		return;
	}

	// Finally, create the scroller.
	new DScroller( Type, dX, dY, -1, lAffectee, 0 );
}

//*****************************************************************************
//
// [BB] SetScroller is defined in p_lnspec.cpp.
void SetScroller (int tag, DScroller::EScrollType type, fixed_t dx, fixed_t dy);

static void client_SetScroller( BYTESTREAM_s *pByteStream )
{
	DScroller::EScrollType	Type;
	fixed_t					dX;
	fixed_t					dY;
	LONG					lTag;

	// Read in the type of scroller.
	Type = (DScroller::EScrollType)NETWORK_ReadByte( pByteStream );

	// Read in the X speed.
	dX = NETWORK_ReadLong( pByteStream );

	// Read in the Y speed.
	dY = NETWORK_ReadLong( pByteStream );

	// Read in the sector being scrolled.
	lTag = NETWORK_ReadShort( pByteStream );

	// Check to make sure what we've read in is valid.
	if (( Type != DScroller::sc_floor ) && ( Type != DScroller::sc_ceiling ) &&
		( Type != DScroller::sc_carry ) && ( Type != DScroller::sc_carry_ceiling ))
	{
		Printf( "client_SetScroller: Unknown type: %d!\n", static_cast<int> (Type) );
		return;
	}

	// Finally, create or update the scroller.
	SetScroller (lTag, Type, dX, dY );
}

//*****************************************************************************
//
// [BB] SetWallScroller is defined in p_lnspec.cpp.
void SetWallScroller(int id, int sidechoice, fixed_t dx, fixed_t dy, int Where);

static void client_SetWallScroller( BYTESTREAM_s *pByteStream )
{
	LONG					lId;
	LONG					lSidechoice;
	fixed_t					dX;
	fixed_t					dY;
	LONG					lWhere;

	// Read in the id.
	lId = NETWORK_ReadLong( pByteStream );

	// Read in the side choice.
	lSidechoice = NETWORK_ReadByte( pByteStream );

	// Read in the X speed.
	dX = NETWORK_ReadLong( pByteStream );

	// Read in the Y speed.
	dY = NETWORK_ReadLong( pByteStream );

	// Read in where.
	lWhere = NETWORK_ReadLong( pByteStream );

	// Finally, create or update the scroller.
	SetWallScroller (lId, lSidechoice, dX, dY, lWhere );
}

//*****************************************************************************
//
static void client_DoFlashFader( BYTESTREAM_s *pByteStream )
{
	float	fR1;
	float	fG1;
	float	fB1;
	float	fA1;
	float	fR2;
	float	fG2;
	float	fB2;
	float	fA2;
	float	fTime;
	ULONG	ulPlayer;

	// Read in the colors, time for the flash fader and which player to apply the effect to.
	fR1 = NETWORK_ReadFloat( pByteStream );
	fG1 = NETWORK_ReadFloat( pByteStream );
	fB1 = NETWORK_ReadFloat( pByteStream );
	fA1 = NETWORK_ReadFloat( pByteStream );

	fR2 = NETWORK_ReadFloat( pByteStream );
	fG2 = NETWORK_ReadFloat( pByteStream );
	fB2 = NETWORK_ReadFloat( pByteStream );
	fA2 = NETWORK_ReadFloat( pByteStream );

	fTime = NETWORK_ReadFloat( pByteStream );

	ulPlayer = NETWORK_ReadByte( pByteStream );

	// [BB] Sanity check.
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	// Create the flash fader.
	if ( players[ulPlayer].mo )
		new DFlashFader( fR1, fG1, fB1, fA1, fR2, fG2, fB2, fA2, fTime, players[ulPlayer].mo );
}

//*****************************************************************************
//
static void client_GenericCheat( BYTESTREAM_s *pByteStream )
{
	ULONG	ulPlayer;
	ULONG	ulCheat;

	// Read in the player who's doing the cheat.
	ulPlayer = NETWORK_ReadByte( pByteStream );

	// Read in the cheat.
	ulCheat = NETWORK_ReadByte( pByteStream );

	if ( playeringame[ulPlayer] == false )
		return;

	// Finally, do the cheat.
	cht_DoCheat( &players[ulPlayer], ulCheat );
}

//*****************************************************************************
//
static void client_SetCameraToTexture( BYTESTREAM_s *pByteStream )
{
	LONG		lID;
	const char	*pszTexture;
	LONG		lFOV;
	AActor		*pCamera;
	FTextureID	picNum;

	// Read in the ID of the camera.
	lID = NETWORK_ReadShort( pByteStream );

	// Read in the name of the texture.
	pszTexture = NETWORK_ReadString( pByteStream );

	// Read in the FOV of the camera.
	lFOV = NETWORK_ReadByte( pByteStream );

	// Find the actor that represents the camera. If we can't find the actor, then
	// break out.
	pCamera = CLIENT_FindThingByNetID( lID );
	if ( pCamera == NULL )
		return;

	picNum = TexMan.CheckForTexture( pszTexture, FTexture::TEX_Wall, FTextureManager::TEXMAN_Overridable );
	if ( !picNum.Exists() )
	{
		Printf( "client_SetCameraToTexture: %s is not a texture\n", pszTexture );
		return;
	}

	FCanvasTextureInfo::Add( pCamera, picNum, lFOV );
}

//*****************************************************************************
//
static void client_CreateTranslation( BYTESTREAM_s *pByteStream, bool bIsTypeTwo )
{
	EDITEDTRANSLATION_s	Translation;
	FRemapTable	*pTranslation;

	// Read in which translation is being created.
	Translation.ulIdx = NETWORK_ReadShort( pByteStream );

	const bool bIsEdited = !!NETWORK_ReadByte( pByteStream );

	// Read in the range that's being translated.
	Translation.ulStart = NETWORK_ReadByte( pByteStream );
	Translation.ulEnd = NETWORK_ReadByte( pByteStream );

	if ( bIsTypeTwo == false )
	{
		Translation.ulPal1 = NETWORK_ReadByte( pByteStream );
		Translation.ulPal2 = NETWORK_ReadByte( pByteStream );
		Translation.ulType = DLevelScript::PCD_TRANSLATIONRANGE1;
	}
	else
	{
		Translation.ulR1 = NETWORK_ReadByte( pByteStream );
		Translation.ulG1 = NETWORK_ReadByte( pByteStream );
		Translation.ulB1 = NETWORK_ReadByte( pByteStream );
		Translation.ulR2 = NETWORK_ReadByte( pByteStream );
		Translation.ulG2 = NETWORK_ReadByte( pByteStream );
		Translation.ulB2 = NETWORK_ReadByte( pByteStream );
		Translation.ulType = DLevelScript::PCD_TRANSLATIONRANGE2;
	}

	// [BB] We need to do this check here, otherwise the client could be crashed
	// by sending a SVC_CREATETRANSLATION packet with an illegal tranlation number.
	if ( Translation.ulIdx < 1 || Translation.ulIdx > MAX_ACS_TRANSLATIONS )
	{
		return;
	}

	pTranslation = translationtables[TRANSLATION_LevelScripted].GetVal(Translation.ulIdx - 1);

	if (pTranslation == NULL)
	{
		pTranslation = new FRemapTable;
		translationtables[TRANSLATION_LevelScripted].SetVal(Translation.ulIdx - 1, pTranslation);
		// [BB] It's mandatory to initialize the translation with the identity, because the server can only send
		// "bIsEdited == false" if the client is already in the game when the translation is created.
		pTranslation->MakeIdentity();
	}

	if ( bIsEdited == false )
		pTranslation->MakeIdentity();

	if ( Translation.ulType == DLevelScript::PCD_TRANSLATIONRANGE1 )
		pTranslation->AddIndexRange( Translation.ulStart, Translation.ulEnd, Translation.ulPal1, Translation.ulPal2 );
	else
		pTranslation->AddColorRange( Translation.ulStart, Translation.ulEnd, Translation.ulR1, Translation.ulG1, Translation.ulB1, Translation.ulR2, Translation.ulG2, Translation.ulB2 );
	pTranslation->UpdateNative();
}

//*****************************************************************************
//
static void client_IgnorePlayer( BYTESTREAM_s *pByteStream )
{
	ULONG	ulPlayer = NETWORK_ReadByte( pByteStream );
	LONG	lTicks = NETWORK_ReadLong( pByteStream );

	if ( ulPlayer < MAXPLAYERS )
	{
		players[ulPlayer].bIgnoreChat = true;
		players[ulPlayer].lIgnoreChatTicks = lTicks;

		Printf( "%s\\c- will be ignored, because you're ignoring %s IP.\n", players[ulPlayer].userinfo.netname, players[ulPlayer].userinfo.gender == GENDER_MALE ? "his" : players[ulPlayer].userinfo.gender == GENDER_FEMALE ? "her" : "its" );
	}
}

//*****************************************************************************
//
static void client_DoPusher( BYTESTREAM_s *pByteStream )
{
	const ULONG ulType = NETWORK_ReadByte( pByteStream );
	const int iLineNum = NETWORK_ReadShort( pByteStream );
	const int iMagnitude = NETWORK_ReadLong( pByteStream );
	const int iAngle = NETWORK_ReadLong( pByteStream );
	const LONG lSourceNetID = NETWORK_ReadShort( pByteStream );
	const int iAffectee = NETWORK_ReadShort( pByteStream );

	line_t *pLine = ( iLineNum >= 0 && iLineNum < numlines ) ? &lines[iLineNum] : NULL;
	new DPusher ( static_cast<DPusher::EPusher> ( ulType ), pLine, iMagnitude, iAngle, CLIENT_FindThingByNetID( lSourceNetID ), iAffectee );
}

//*****************************************************************************
//
void AdjustPusher (int tag, int magnitude, int angle, DPusher::EPusher type);
static void client_AdjustPusher( BYTESTREAM_s *pByteStream )
{
	const int iTag = NETWORK_ReadShort( pByteStream );
	const int iMagnitude = NETWORK_ReadLong( pByteStream );
	const int iAngle = NETWORK_ReadLong( pByteStream );
	const ULONG ulType = NETWORK_ReadByte( pByteStream );
	AdjustPusher (iTag, iMagnitude, iAngle, static_cast<DPusher::EPusher> ( ulType ));
}

//*****************************************************************************
//
void STClient::ReplaceTextures( BYTESTREAM_s *pByteStream )
{
	int iFromname = NETWORK_ReadLong( pByteStream );
	int iToname = NETWORK_ReadLong( pByteStream );
	int iTexFlags = NETWORK_ReadByte( pByteStream );

	DLevelScript::ReplaceTextures ( iFromname, iToname, iTexFlags );
}

//*****************************************************************************
//
void APathFollower::InitFromStream ( BYTESTREAM_s *pByteStream )
{
	APathFollower *pPathFollower = static_cast<APathFollower*> ( CLIENT_FindThingByNetID( NETWORK_ReadShort( pByteStream ) ) );
	const int currNodeId = NETWORK_ReadShort( pByteStream );
	const int prevNodeId = NETWORK_ReadShort( pByteStream );
	const float serverTime = NETWORK_ReadFloat( pByteStream );

	if ( pPathFollower )
	{
		pPathFollower->lServerCurrNodeId = currNodeId;
		pPathFollower->lServerPrevNodeId = prevNodeId;
		pPathFollower->fServerTime = serverTime;
	}
	else
	{
#ifdef CLIENT_WARNING_MESSAGES
		Printf( "APathFollower::InitFromStream: Couldn't find actor.\n" );
#endif
		return;
	}
}

//*****************************************************************************
// [CK] Reads the bytestream for what sound is incoming.
//
static const char* client_ReadBytestreamSound( BYTESTREAM_s *pByteStream )
{
	const LONG soundIndex = NETWORK_ReadShort( pByteStream );
	return ( soundIndex < 0 || soundIndex >= g_SndinfoLookupTable.Size( ) ) ? "" : g_SndinfoLookupTable[soundIndex];
}

//*****************************************************************************
//	CONSOLE COMMANDS

CCMD( connect )
{
	const char	*pszDemoName;
	UCVarValue	Val;

	// Servers can't connect to other servers!
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		return;

	// No IP specified.
	if ( argv.argc( ) <= 1 )
	{
		Printf( "Usage: connect <server IP>\n" );
		return;
	}

	// Potentially disconnect from the current server.
	CLIENT_QuitNetworkGame( NULL );

	// Put the game in client mode.
	NETWORK_SetState( NETSTATE_CLIENT );

	// Make sure cheats are off.
	Val.Bool = false;
	sv_cheats.ForceSet( Val, CVAR_Bool );
	am_cheat = 0;

	// Make sure our visibility is normal.
	R_SetVisibility( 8.0f );

	// Create a server IP from the given string.
	NETWORK_StringToAddress( argv[1], &g_AddressServer );

	// If the user didn't specify a port, use the default port.
	if ( g_AddressServer.usPort == 0 )
		NETWORK_SetAddressPort( g_AddressServer, DEFAULT_SERVER_PORT );

	g_AddressLastConnected = g_AddressServer;

	// Put the game in the full console.
	gameaction = ga_fullconsole;

	// Send out a connection signal.
	CLIENT_AttemptConnection( );

	// Update the connection state.
	CLIENT_SetConnectionState( CTS_ATTEMPTINGCONNECTION );

	// If we've elected to record a demo, begin that process now.
	pszDemoName = Args->CheckValue( "-record" );
	if (( gamestate == GS_STARTUP ) && ( pszDemoName ))
		CLIENTDEMO_BeginRecording( pszDemoName );
}

//*****************************************************************************
//
CCMD( disconnect )
{
	// Nothing to do if we're not in client mode!
	if ( NETWORK_GetState( ) != NETSTATE_CLIENT )
		return;

	// [BB] While disconnecting is not an error, I_Error is a convenient way to abort the current game and reset everything.
	// [Dusk] Use a whitespace to suppress GCC warnings.
	I_Error ( " " );
}

//*****************************************************************************
//
#ifdef	_DEBUG
CCMD( timeout )
{
	// Nothing to do if we're not in client mode!
	if ( NETWORK_GetState( ) != NETSTATE_CLIENT )
		return;

	// Clear out the existing players.
	CLIENT_ClearAllPlayers();
/*
	// If we're connected in any way, send a disconnect signal.
	if ( g_ConnectionState != CTS_DISCONNECTED )
	{
		NETWORK_WriteByte( &g_LocalBuffer, CONNECT_QUIT );
		g_lBytesSent += g_LocalBuffer.cursize;
		if ( g_lBytesSent > g_lMaxBytesSent )
			g_lMaxBytesSent = g_lBytesSent;
		NETWORK_LaunchPacket( g_LocalBuffer, g_AddressServer );
		NETWORK_ClearBuffer( &g_LocalBuffer );
	}
*/
	// Clear out our copy of the server address.
	memset( &g_AddressServer, 0, sizeof( g_AddressServer ));
	CLIENT_SetConnectionState( CTS_DISCONNECTED );

	// Go back to the full console.
	gameaction = ga_fullconsole;
}
#endif
//*****************************************************************************
//
CCMD( reconnect )
{
	UCVarValue	Val;

	// If we're in the middle of a game, we first need to disconnect from the server.
	if ( g_ConnectionState != CTS_DISCONNECTED )
		CLIENT_QuitNetworkGame( NULL );
	
	// Store the address of the server we were on.
	if ( g_AddressLastConnected.abIP[0] == 0 )
	{
		Printf( "Unknown IP for last server. Use \"connect <server ip>\".\n" );
		return;
	}

	// Put the game in client mode.
	NETWORK_SetState( NETSTATE_CLIENT );

	// Make sure cheats are off.
	Val.Bool = false;
	sv_cheats.ForceSet( Val, CVAR_Bool );
	am_cheat = 0;

	// Make sure our visibility is normal.
	R_SetVisibility( 8.0f );

	// Set the address of the server we're trying to connect to to the previously connected to server.
	g_AddressServer = g_AddressLastConnected;

	// Put the game in the full console.
	gameaction = ga_fullconsole;

	// Send out a connection signal.
	CLIENT_AttemptConnection( );

	// Update the connection state.
	CLIENT_SetConnectionState( CTS_ATTEMPTINGCONNECTION );
}

//*****************************************************************************
//
CCMD( rcon )
{
	char		szString[1024];
	char		szAppend[256];

	if ( g_ConnectionState != CTS_ACTIVE )
		return;

	if ( argv.argc( ) > 1 )
	{
		LONG	lLast;
		LONG	lIdx;
		ULONG	ulIdx2;
		bool	bHasSpace;

		memset( szString, 0, 1024 );
		memset( szAppend, 0, 256 );
		
		lLast = argv.argc( );

		// Since we don't want "rcon" to be part of our string, start at 1.
		for ( lIdx = 1; lIdx < lLast; lIdx++ )
		{
			memset( szAppend, 0, 256 );

			bHasSpace = false;
			for ( ulIdx2 = 0; ulIdx2 < strlen( argv[lIdx] ); ulIdx2++ )
			{
				if ( argv[lIdx][ulIdx2] == ' ' )
				{
					bHasSpace = true;
					break;
				}
			}

			if ( bHasSpace )
				strcat( szAppend, "\"" );
			strcat( szAppend, argv[lIdx] );
			strcat( szString, szAppend );
			if ( bHasSpace )
				strcat( szString, "\"" );
			if (( lIdx + 1 ) < lLast )
				strcat( szString, " " );
		}
	
		// Alright, enviorment is correct, the string has been built.
		// SEND IT!
		CLIENTCOMMANDS_RCONCommand( szString );
	}
	else
		Printf( "Usage: rcon <command>\n" );
}

//*****************************************************************************
//
CCMD( send_password )
{
	if ( argv.argc( ) <= 1 )
	{
		Printf( "Usage: send_password <password>\n" );
		return;
	}

	if ( g_ConnectionState == CTS_ACTIVE )
		CLIENTCOMMANDS_RequestRCON( argv[1] );
}

//*****************************************************************************
// [Dusk] Redisplay the MOTD
CCMD( motd ) {CLIENT_DisplayMOTD();}

//*****************************************************************************
//	CONSOLE VARIABLES

CVAR( Bool, cl_predict_players, true, CVAR_ARCHIVE )
//CVAR( Int, cl_maxmonstercorpses, 0, CVAR_ARCHIVE )
CVAR( Float, cl_motdtime, 5.0, CVAR_ARCHIVE )
CVAR( Bool, cl_taunts, true, CVAR_ARCHIVE )
CVAR( Int, cl_showcommands, 0, CVAR_ARCHIVE )
CVAR( Int, cl_showspawnnames, 0, CVAR_ARCHIVE )
CVAR( Int, cl_connect_flags, CCF_STARTASSPECTATOR, CVAR_ARCHIVE );
CVAR( Flag, cl_startasspectator, cl_connect_flags, CCF_STARTASSPECTATOR );
CVAR( Flag, cl_dontrestorefrags, cl_connect_flags, CCF_DONTRESTOREFRAGS )
CVAR( Flag, cl_hidecountry, cl_connect_flags, CCF_HIDECOUNTRY )
// [BB] Don't archive the passwords! Otherwise Skulltag would always send
// the last used passwords to all servers it connects to.
CVAR( String, cl_password, "password", 0 )
CVAR( String, cl_joinpassword, "password", 0 )
CVAR( Bool, cl_hitscandecalhack, true, CVAR_ARCHIVE )

//*****************************************************************************
//	STATISTICS
/*
// [BC] TEMPORARY
ADD_STAT( momentum )
{
	FString	Out;

	Out.Format( "X: %3d     Y: %3d", players[consoleplayer].velx, players[consoleplayer].vely );

	return ( Out );
}
*/
