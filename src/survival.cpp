//-----------------------------------------------------------------------------
//
// Skulltag Source
// Copyright (C) 2004 Brad Carney
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
// Date created:  1/14/06
//
//
// Filename: survival.cpp
//
// Description: Contains survival co-op routines
//
//-----------------------------------------------------------------------------

#include "announcer.h"
#include "cl_main.h"
#include "cooperative.h"
#include "g_game.h"
#include "i_system.h"
#include "joinqueue.h"
#include "network.h"
#include "p_acs.h"
#include "p_local.h"
#include "p_setup.h"
#include "sbar.h"
#include "scoreboard.h"
#include "survival.h"
#include "sv_commands.h"
#include "v_video.h"

//*****************************************************************************
//	VARIABLES

static	ULONG			g_ulSurvivalCountdownTicks = 0;
static	SURVIVALSTATE_e	g_SurvivalState;

//*****************************************************************************
//	FUNCTIONS

void SURVIVAL_Construct( void )
{
	g_SurvivalState = SURVS_WAITINGFORPLAYERS;
}

//*****************************************************************************
//
void SURVIVAL_Tick( void )
{
	// Not in survival mode.
	if ( survival == false )
		return;

	switch ( g_SurvivalState )
	{
	case SURVS_WAITINGFORPLAYERS:

		if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
			break;

		// Someone is here! Begin the countdown.
		if ( SURVIVAL_CountActivePlayers( false ) > 0 )
		{
			if ( sv_survivalcountdowntime > 0 )
				SURVIVAL_StartCountdown(( sv_survivalcountdowntime * TICRATE ) - 1 );
			else
				SURVIVAL_StartCountdown(( 20 * TICRATE ) - 1 );
		}
		break;
	case SURVS_COUNTDOWN:

		if ( g_ulSurvivalCountdownTicks )
		{
			g_ulSurvivalCountdownTicks--;

			// FIGHT!
			if (( g_ulSurvivalCountdownTicks == 0 ) && ( NETWORK_GetState( ) != NETSTATE_CLIENT ))
				SURVIVAL_DoFight( );
			// Play "3... 2... 1..." sounds.
			else if ( g_ulSurvivalCountdownTicks == ( 3 * TICRATE ))
				ANNOUNCER_PlayEntry( cl_announcer, "Three" );
			else if ( g_ulSurvivalCountdownTicks == ( 2 * TICRATE ))
				ANNOUNCER_PlayEntry( cl_announcer, "Two" );
			else if ( g_ulSurvivalCountdownTicks == ( 1 * TICRATE ))
				ANNOUNCER_PlayEntry( cl_announcer, "One" );
		}
		break;
	case SURVS_INPROGRESS:

		if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
			break;

		// If everyone is dead, the mission has failed!
		if ( SURVIVAL_CountActivePlayers( true ) == 0 )
		{
			// Put the game state in the mission failed state.
			SURVIVAL_SetState( SURVS_MISSIONFAILED );

			// Pause for five seconds for the failed sequence.
			GAME_SetEndLevelDelay( 5 * TICRATE );
		}
	}
}

//*****************************************************************************
//
ULONG SURVIVAL_CountActivePlayers( bool bLiving )
{
	ULONG	ulIdx;
	ULONG	ulPlayers;

	ulPlayers = 0;
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if (( playeringame[ulIdx] ) && ( players[ulIdx].bSpectating == false ))
		{
			if (( bLiving ) && ( players[ulIdx].health == 0 ))
				continue;

			ulPlayers++;
		}
	}

	return ( ulPlayers );
}

//*****************************************************************************
//
void SURVIVAL_StartCountdown( ULONG ulTicks )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if (( playeringame[ulIdx] ) && ( players[ulIdx].pSkullBot ))
			players[ulIdx].pSkullBot->PostEvent( BOTEVENT_LMS_STARTINGCOUNTDOWN );
	}
/*
	// First, reset everyone's fragcount. This must be done before setting the state to LMSS_COUNTDOWN
	// otherwise PLAYER_SetFragcount will ignore our request.
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( playeringame[ulIdx] )
			PLAYER_SetFragcount( &players[ulIdx], 0, false, false );
	}
*/
	// Put the game in a countdown state.
	if ( NETWORK_GetState( ) != NETSTATE_CLIENT )
		SURVIVAL_SetState( SURVS_COUNTDOWN );

	// Set the survival countdown ticks.
	SURVIVAL_SetCountdownTicks( ulTicks );

	// Announce that the fight will soon start.
	ANNOUNCER_PlayEntry( cl_announcer, "PrepareToFight" );

	// Tell clients to start the countdown.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_DoGameModeCountdown( ulTicks );
}

//*****************************************************************************
//
void SURVIVAL_DoFight( void )
{
	ULONG				ulIdx;
	DHUDMessageFadeOut	*pMsg;
/*
	MapData				*pMap;
	AActor				*pActor;
	AActor				*pNewActor;
	AActor				*pActorInfo;
	fixed_t				X;
	fixed_t				Y;
	fixed_t				Z;
*/
	// The battle is now in progress.
	if ( NETWORK_GetState( ) != NETSTATE_CLIENT )
		SURVIVAL_SetState( SURVS_INPROGRESS );

	// Make sure this is 0. Can be non-zero in network games if they're slightly out of sync.
	g_ulSurvivalCountdownTicks = 0;

	// Reset level time to 0.
	level.time = 0;

	// Reset everyone's kill count.
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		players[ulIdx].killcount = 0;
		players[ulIdx].ulRailgunShots = 0;
	}

	// Tell clients to "fight!".
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_DoGameModeFight( 0 );

	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
	{
		// Play fight sound.
		ANNOUNCER_PlayEntry( cl_announcer, "Fight" );

		screen->SetFont( BigFont );

		// Display "FIGHT!" HUD message.
		pMsg = new DHUDMessageFadeOut( "FIGHT!",
			160.4f,
			75.0f,
			320,
			200,
			CR_RED,
			2.0f,
			1.0f );

		StatusBar->AttachMessage( pMsg, 'CNTR' );
		screen->SetFont( SmallFont );
	}
	// Display a little thing in the server window so servers can know when matches begin.
	else
		Printf( "FIGHT!\n" );

	// Revert the map to how it was in its original state.
	if ( NETWORK_GetState( ) != NETSTATE_CLIENT )
		GAME_ResetMap( );
/*
	if ( NETWORK_GetState( ) != NETSTATE_CLIENT )
	{
		for ( ulIdx = 0; ulIdx < (ULONG)numlines; ulIdx++ )
		{
			// Reset the specials for lines that do not have a repeatable special.
			if ( lines[ulIdx].special == 0 )
				lines[ulIdx].special = lines[ulIdx].SavedSpecial;

			// Also, restore any changed textures.
			if ( lines[ulIdx].ulTexChangeFlags != 0 )
			{
				// [BC] If we're the server, tell clients about this texture change.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_SetLineTexture( ulIdx );

				if ( lines[ulIdx].sidenum[0] != 0xffff )
				{
					sides[lines[ulIdx].sidenum[0]].toptexture = sides[lines[ulIdx].sidenum[0]].SavedTopTexture;
					sides[lines[ulIdx].sidenum[0]].midtexture = sides[lines[ulIdx].sidenum[0]].SavedMidTexture;
					sides[lines[ulIdx].sidenum[0]].bottomtexture = sides[lines[ulIdx].sidenum[0]].SavedBottomTexture;
				}

				if ( lines[ulIdx].sidenum[1] != NO_INDEX )
				{
					sides[lines[ulIdx].sidenum[1]].toptexture = sides[lines[ulIdx].sidenum[1]].SavedTopTexture;
					sides[lines[ulIdx].sidenum[1]].midtexture = sides[lines[ulIdx].sidenum[1]].SavedMidTexture;
					sides[lines[ulIdx].sidenum[1]].bottomtexture = sides[lines[ulIdx].sidenum[1]].SavedBottomTexture;
				}

				lines[ulIdx].ulTexChangeFlags = 0;
			}
		}

		// Restore sector heights, flat changes, and light changes.
		for ( ulIdx = 0; ulIdx < (ULONG)numsectors; ulIdx++ )
		{
			if ( sectors[ulIdx].bCeilingHeightChange )
			{
				sectors[ulIdx].ceilingplane = sectors[ulIdx].SavedCeilingPlane;
				sectors[ulIdx].ceilingtexz = sectors[ulIdx].SavedCeilingTexZ;
				sectors[ulIdx].bCeilingHeightChange = false;

				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_SetSectorCeilingPlane( ulIdx );
			}

			if ( sectors[ulIdx].bFloorHeightChange )
			{
				sectors[ulIdx].floorplane = sectors[ulIdx].SavedFloorPlane;
				sectors[ulIdx].floortexz = sectors[ulIdx].SavedFloorTexZ;
				sectors[ulIdx].bFloorHeightChange = false;

				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_SetSectorFloorPlane( ulIdx );
			}

			if ( sectors[ulIdx].bFlatChange )
			{
				sectors[ulIdx].floorpic = sectors[ulIdx].SavedFloorPic;
				sectors[ulIdx].ceilingpic = sectors[ulIdx].SavedCeilingPic;
				sectors[ulIdx].bFlatChange = false;

				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_SetSectorFlat( ulIdx );
			}

			if ( sectors[ulIdx].bLightChange )
			{
				sectors[ulIdx].lightlevel = sectors[ulIdx].SavedLightLevel;
				sectors[ulIdx].bLightChange = false;

				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_SetSectorLightLevel( ulIdx );
			}
		}

		// Reload the items on this level.
		TThinkerIterator<AActor> iterator;
		while (( pActor = iterator.Next( )) != NULL )
		{
			// Destroy any object not present when the map loaded.
			if (( pActor->ulSTFlags & STFL_LEVELSPAWNED ) == false )
			{
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_DestroyThing( pActor );

				pActor->Destroy( );
				continue;
			}

			// Restore any items in a hidden state.
			if (( pActor->state == &AInventory::States[0] ) ||	// S_HIDEDOOMISH
				( pActor->state == &AInventory::States[3] ) ||	// S_HIDESPECIAL
				( pActor->state == &AInventory::States[17] ))	// S_HIDEINDEFINITELY
			{
				CLIENT_RestoreSpecialPosition( pActor );
				CLIENT_RestoreSpecialDoomThing( pActor, false );

				// Tell clients to respawn this item (without fog).
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_RespawnThing( pActor, false );
			}

			// If this is an enemy, respawn it.
			if ( pActor->flags & MF_COUNTKILL )
			{
				// Get the default information for this actor, so we can determine how to
				// respawn it.
				pActorInfo = pActor->GetDefault( );

				// This monster appears to be untouched; no need to respawn it.
				if (( pActor->x == pActor->SpawnPoint[0] << FRACBITS ) &&
					( pActor->y == pActor->SpawnPoint[1] << FRACBITS ) &&
					( pActor->health == pActorInfo->health ))
				{
					continue;
				}

				// Determine the Z position to spawn this monster in.
				if ( pActorInfo->flags & MF_SPAWNCEILING )
					Z = ONCEILINGZ;
				else if ( pActorInfo->flags2 & MF2_SPAWNFLOAT )
					Z = FLOATRANDZ;
				else if ( pActorInfo->flags2 & MF2_FLOATBOB )
					Z = pActor->SpawnPoint[2] << FRACBITS;
				else
					Z = ONFLOORZ;

				// Spawn the new monster.
				X = pActor->SpawnPoint[0] << FRACBITS;
				Y = pActor->SpawnPoint[1] << FRACBITS;
				pNewActor = Spawn( RUNTIME_TYPE( pActor ), X, Y, Z, NO_REPLACE );

				if ( Z == ONFLOORZ )
					pNewActor->z += pNewActor->SpawnPoint[2] << FRACBITS;
				else if ( Z == ONCEILINGZ )
					pNewActor->z -= pNewActor->SpawnPoint[2] << FRACBITS;

				// Inherit attributes from the old monster.
				pNewActor->SpawnPoint[0] = pActor->SpawnPoint[0];
				pNewActor->SpawnPoint[1] = pActor->SpawnPoint[1];
				pNewActor->SpawnPoint[2] = pActor->SpawnPoint[2];
				pNewActor->SpawnAngle = pActor->SpawnAngle;
				pNewActor->SpawnFlags = pActor->SpawnFlags;
				pNewActor->angle = ANG45 * ( pActor->SpawnAngle / 45 );

				if ( pActor->SpawnFlags & MTF_AMBUSH )
					pNewActor->flags |= MF_AMBUSH;

				pNewActor->reactiontime = 18;

				pNewActor->TIDtoHate = pActor->TIDtoHate;
				pNewActor->LastLook = pActor->LastLook;
				pNewActor->flags3 |= pActor->flags3 & MF3_HUNTPLAYERS;
				pNewActor->flags4 |= pActor->flags4 & MF4_NOHATEPLAYERS;
				pNewActor->ulSTFlags |= STFL_LEVELSPAWNED;

				// Remove the old monster.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_DestroyThing( pActor );
				pActor->Destroy( );
			}
		}
	}

	// Also, delete all floors, plats, etc. that are in progress. This can be done on both
	// the server side, and the client side.
	for ( ulIdx = 0; ulIdx < (ULONG)numsectors; ulIdx++ )
	{
		if ( sectors[ulIdx].ceilingdata )
			sectors[ulIdx].ceilingdata->Destroy( );
		if ( sectors[ulIdx].floordata )
			sectors[ulIdx].floordata->Destroy( );
	}

	// Reset the scripts.
	if ( NETWORK_GetState( ) != NETSTATE_CLIENT )
	{
		FBehavior::StaticUnloadModules( );
		
		pMap = P_OpenMapData( level.mapname );
		if ( pMap == NULL )
			I_Error( "SURVIVAL_DoFight: Unable to open map '%s'\n", level.mapname );

		P_LoadBehavior( pMap );

//		if ( Wads.CheckLumpName( Wads.GetNumForName( level.mapname ) + ML_BEHAVIOR, "BEHAVIOR" ))
//			P_LoadBehavior( Wads.GetNumForName( level.mapname ) + ML_BEHAVIOR );
	}
*/
	if ( NETWORK_GetState( ) != NETSTATE_CLIENT )
	{
		// Respawn the players.
		for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
		{
			if (( playeringame[ulIdx] == false ) ||
				( PLAYER_IsTrueSpectator( &players[ulIdx] )))
			{
				continue;
			}
			
			if ( players[ulIdx].mo )
			{
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_DestroyThing( players[ulIdx].mo );

				players[ulIdx].mo->Destroy( );
				players[ulIdx].mo = NULL;
			}

			// Set the player's state to PST_REBORNNOINVENTORY so they everything is cleared (weapons, etc.)
			players[ulIdx].playerstate = PST_REBORNNOINVENTORY;
			G_CooperativeSpawnPlayer( ulIdx, true );

			if ( players[ulIdx].pSkullBot )
				players[ulIdx].pSkullBot->PostEvent( BOTEVENT_LMS_FIGHT );
		}
	}

	SCOREBOARD_RefreshHUD( );
}

//*****************************************************************************
//
/*
void SURVIVAL_DoMissionFailed( void )
{
	ULONG	ulIdx;

	// Put the game state in the mission failed state.
	if ( NETWORK_GetState( ) != NETSTATE_CLIENT )
		SURVIVAL_SetState( SURVS_MISSIONFAILED );

	// Tell clients to do the mission failed sequence.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SetGameModeState( SURVS_MISSIONFAILED );

	// Display "%s WINS!" HUD message.
	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
	{
		DHUDMessageFadeOut	*pMsg;

		screen->SetFont( BigFont );

		pMsg = new DHUDMessageFadeOut( "MISSION FAILED!",
			160.4f,
			75.0f,
			320,
			200,
			CR_RED,
			3.0f,
			2.0f );

		StatusBar->AttachMessage( pMsg, 'CNTR' );
		screen->SetFont( SmallFont );
	}
}
*/
//*****************************************************************************
//*****************************************************************************
//
ULONG SURVIVAL_GetCountdownTicks( void )
{
	return ( g_ulSurvivalCountdownTicks );
}

//*****************************************************************************
//
void SURVIVAL_SetCountdownTicks( ULONG ulTicks )
{
	g_ulSurvivalCountdownTicks = ulTicks;
}

//*****************************************************************************
//
SURVIVALSTATE_e SURVIVAL_GetState( void )
{
	return ( g_SurvivalState );
}

//*****************************************************************************
//
void SERVERCONSOLE_UpdatePlayerInfo( LONG lPlayer, ULONG ulUpdateFlags );
void SURVIVAL_SetState( SURVIVALSTATE_e State )
{
	ULONG	ulIdx;

	g_SurvivalState = State;

	// Tell clients about the state change.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SetGameModeState( State );

	switch ( State )
	{
	case SURVS_WAITINGFORPLAYERS:

		// Zero out the countdown ticker.
		SURVIVAL_SetCountdownTicks( 0 );

		if (( NETWORK_GetState( ) != NETSTATE_CLIENT ) && ( survival ))
		{
			// Respawn any players who were downed during the previous round.
			for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
			{
				if (( playeringame[ulIdx] == false ) ||
					( PLAYER_IsTrueSpectator( &players[ulIdx] )))
				{
					continue;
				}

				players[ulIdx].bSpectating = false;
				players[ulIdx].bDeadSpectator = false;
				players[ulIdx].playerstate = PST_REBORNNOINVENTORY;

				if (( players[ulIdx].mo ) && ( players[ulIdx].mo->health > 0 ))
				{
					if ( NETWORK_GetState( ) == NETSTATE_SERVER )
						SERVERCOMMANDS_DestroyThing( players[ulIdx].mo );

					players[ulIdx].mo->Destroy( );
					players[ulIdx].mo = NULL;
				}

				G_CooperativeSpawnPlayer( ulIdx, true );
			}

			// Let anyone who's been waiting in line join now.
			JOINQUEUE_PopQueue( -1 );
		}
		break;
	case SURVS_NEWMAP:

		if (( NETWORK_GetState( ) != NETSTATE_CLIENT ) && ( survival ))
		{
			// Respawn any players who were downed during the previous round.
			for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
			{
				if (( playeringame[ulIdx] == false ) ||
					( PLAYER_IsTrueSpectator( &players[ulIdx] )) ||
					( players[ulIdx].bSpectating == false ))
				{
					continue;
				}

				players[ulIdx].bSpectating = false;
				players[ulIdx].bDeadSpectator = false;
				players[ulIdx].playerstate = PST_REBORNNOINVENTORY;

				if ( players[ulIdx].mo )
				{
					if ( NETWORK_GetState( ) == NETSTATE_SERVER )
						SERVERCOMMANDS_DestroyThing( players[ulIdx].mo );

					players[ulIdx].mo->Destroy( );
					players[ulIdx].mo = NULL;
				}

				G_CooperativeSpawnPlayer( ulIdx, true );
			}

			// Also, let anyone who's been waiting in line join now.
			JOINQUEUE_PopQueue( -1 );
		}

		// Now, go back to the "in progress" state.
		SURVIVAL_SetState( SURVS_INPROGRESS );
		break;
	case SURVS_MISSIONFAILED:

		if ( NETWORK_GetState( ) != NETSTATE_SERVER )
		{
			DHUDMessageFadeOut	*pMsg;

			screen->SetFont( BigFont );

			// Display "%s WINS!" HUD message.
			pMsg = new DHUDMessageFadeOut( "MISSION FAILED!",
				160.4f,
				75.0f,
				320,
				200,
				CR_RED,
				3.0f,
				2.0f );

			StatusBar->AttachMessage( pMsg, 'CNTR' );
			screen->SetFont( SmallFont );
		}
		break;
	}

	// Since some players might have respawned, update the server console window.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
		{
			if ( playeringame[ulIdx] )
				SERVERCONSOLE_UpdatePlayerInfo( ulIdx, UDF_FRAGS );
		}
	}
}

//*****************************************************************************
//	CONSOLE COMMANDS/VARIABLES

CVAR( Int, sv_survivalcountdowntime, 10, CVAR_ARCHIVE );
