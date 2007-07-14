//-----------------------------------------------------------------------------
//
// Skulltag Source
// Copyright (C) 2002 Brad Carney
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
// Filename: team.cpp
//
// Description: Contains team routines
//
//-----------------------------------------------------------------------------

#include "a_doomglobal.h"
#include "a_pickups.h"
#include "announcer.h"
#include "c_cvars.h"
#include "c_dispatch.h"
#include "campaign.h"
#include "cl_commands.h"
#include "cl_demo.h"
#include "cl_main.h"
#include "cooperative.h"
#include "deathmatch.h"
#include "g_game.h"
#include "info.h"
#include "m_random.h"
#include "network.h"
#include "p_acs.h"
#include "p_effect.h"
#include "p_local.h"
#include "s_sound.h"
#include "sbar.h"
#include "scoreboard.h"
#include "sv_commands.h"
#include "sv_main.h"
#include "team.h"
#include "v_text.h"
#include "v_video.h"

//*****************************************************************************
//	MISC CRAP THAT SHOULDN'T BE HERE BUT HAS TO BE BECAUSE OF SLOPPY CODING

void	G_PlayerReborn( int player );

void	SERVERCONSOLE_UpdateScoreboard( void );
EXTERN_CVAR( Int,  cl_respawninvuleffect )

//*****************************************************************************
//	VARIABLES

static	TEAM_t	g_Team[NUM_TEAMS];
static	bool	g_bSimpleCTFMode;
static	bool	g_bSimpleSTMode;
static	bool	g_bBlueFlagTaken;
static	bool	g_bRedFlagTaken;
static	bool	g_bWhiteFlagTaken;
static	bool	g_bBlueSkullTaken;
static	bool	g_bRedSkullTaken;
static	POS_t	g_BlueFlagOrigin;
static	POS_t	g_RedFlagOrigin;
static	POS_t	g_WhiteFlagOrigin;
static	POS_t	g_BlueSkullOrigin;
static	POS_t	g_RedSkullOrigin;
static	ULONG	g_ulWhiteFlagReturnTicks;

// Keep track of players eligable for assist medals.
static	ULONG	g_ulAssistPlayer[NUM_TEAMS] = { MAXPLAYERS, MAXPLAYERS };

static FRandom	g_JoinTeamSeed( "JoinTeamSeed" );

//*****************************************************************************
//	FUNCTIONS

void TEAM_Construct( void )
{
	g_Team[TEAM_BLUE].FlagItem = PClass::FindClass( "BlueSkullST" );
//	g_Team[TEAM_BLUE].Icon = S_BSKULL;
	g_Team[TEAM_BLUE].lScore = 0;
	g_Team[TEAM_BLUE].lRailColor = V_GetColorFromString( NULL, "00 00 ff" );
	g_Team[TEAM_BLUE].ulReturnTicks = 0;
	g_Team[TEAM_BLUE].ulTextColor = CR_BLUE;
	g_Team[TEAM_BLUE].ulReturnScriptOffset = SCRIPT_BlueReturn;
	g_Team[TEAM_BLUE].lFragCount = 0;
	g_Team[TEAM_BLUE].lDeathCount = 0;
	g_Team[TEAM_BLUE].lWinCount = 0;

	sprintf( g_Team[TEAM_BLUE].szColor, "00 00 bf" );
	sprintf( g_Team[TEAM_BLUE].szName, "Blue" );

	g_Team[TEAM_RED].FlagItem = PClass::FindClass( "RedSkullST" );
//	g_Team[TEAM_RED].Icon = S_RSKULL;
	g_Team[TEAM_RED].lScore = 0;
	g_Team[TEAM_RED].lRailColor = V_GetColorFromString( NULL, "ff 00 00" );
	g_Team[TEAM_RED].ulReturnTicks = 0;
	g_Team[TEAM_RED].ulTextColor = CR_RED;
	g_Team[TEAM_RED].ulReturnScriptOffset = SCRIPT_RedReturn;
	g_Team[TEAM_RED].lFragCount = 0;
	g_Team[TEAM_RED].lDeathCount = 0;
	g_Team[TEAM_RED].lWinCount = 0;

	sprintf( g_Team[TEAM_RED].szColor, "bf 00 00" );
	sprintf( g_Team[TEAM_RED].szName, "Red" );

	// Start off in regular CTF mode.
	g_bSimpleCTFMode = false;
	g_bBlueFlagTaken = false;
	g_bRedFlagTaken = false;
	g_bWhiteFlagTaken = false;

	// Start off in regular ST mode.
	g_bSimpleSTMode = false;
	g_bBlueSkullTaken = false;
	g_bRedSkullTaken = false;

	g_ulWhiteFlagReturnTicks = 0;
}

//*****************************************************************************
//
void TEAM_Tick( void )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < NUM_TEAMS; ulIdx++ )
	{
		if ( g_Team[ulIdx].ulReturnTicks )
		{
			g_Team[ulIdx].ulReturnTicks--;
			if ( g_Team[ulIdx].ulReturnTicks == 0 )
				TEAM_ExecuteReturnRoutine( ulIdx, NULL );
		}
	}

	if ( g_ulWhiteFlagReturnTicks )
	{
		g_ulWhiteFlagReturnTicks--;
		if ( g_ulWhiteFlagReturnTicks == 0 )
			TEAM_ExecuteReturnRoutine( ulIdx, NULL );
	}
}

//*****************************************************************************
//
void TEAM_Reset( void )
{
	g_Team[TEAM_BLUE].lScore = 0;
	g_Team[TEAM_BLUE].ulReturnTicks = 0;
	g_Team[TEAM_BLUE].lFragCount = 0;
	g_Team[TEAM_BLUE].lDeathCount = 0;
	g_Team[TEAM_BLUE].lWinCount = 0;

	g_Team[TEAM_RED].lScore = 0;
	g_Team[TEAM_RED].ulReturnTicks = 0;
	g_Team[TEAM_RED].lFragCount = 0;
	g_Team[TEAM_RED].lDeathCount = 0;
	g_Team[TEAM_RED].lWinCount = 0;

	g_bBlueFlagTaken = false;
	g_bRedFlagTaken = false;
	g_bWhiteFlagTaken = false;

	g_bBlueSkullTaken = false;
	g_bRedSkullTaken = false;

	g_ulWhiteFlagReturnTicks = 0;
}

//*****************************************************************************
//
ULONG TEAM_CountPlayers( ULONG ulTeamIdx )
{
	ULONG	ulIdx;
	ULONG	ulCount;

	ulCount = 0;
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		// Don't count players not in the game or spectators.
		if (( playeringame[ulIdx] == false ) || ( PLAYER_IsTrueSpectator( &players[ulIdx] )))
			continue;

		if ( players[ulIdx].bOnTeam && ( players[ulIdx].ulTeam == ulTeamIdx ))
			ulCount++;
	}

	return ( ulCount );
}

//*****************************************************************************
//
ULONG TEAM_CountLivingPlayers( ULONG ulTeamIdx )
{
	ULONG	ulIdx;
	ULONG	ulCount;

	ulCount = 0;
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		// Don't count players not in the game or spectators.
		if (( playeringame[ulIdx] == false ) || ( PLAYER_IsTrueSpectator( &players[ulIdx] )))
			continue;

		// Don't count dead players.
		if ( players[ulIdx].health > 0 )
			continue;

		if ( players[ulIdx].bOnTeam && ( players[ulIdx].ulTeam == ulTeamIdx ))
			ulCount++;
	}

	return ( ulCount );
}

//*****************************************************************************
//
void TEAM_ExecuteReturnRoutine( ULONG ulTeamIdx, AActor *pReturner )
{
	AActor						*pFlag;
	const PClass				*pClass;
	TThinkerIterator<AActor>	Iterator;

	if ( ulTeamIdx > NUM_TEAMS )
		return;

	// Execute the return scripts.
	if (( NETWORK_GetState( ) != NETSTATE_CLIENT ) && ( CLIENTDEMO_IsPlaying( ) == false ))
	{
		// I don't like these hacks :((((((((((((((
		if ( ulTeamIdx == NUM_TEAMS )
			FBehavior::StaticStartTypedScripts( SCRIPT_WhiteReturn, NULL, true );
		else
			FBehavior::StaticStartTypedScripts( TEAM_GetReturnScriptOffset( ulTeamIdx ), NULL, true );
	}

	if ( ulTeamIdx == NUM_TEAMS )
		pClass = PClass::FindClass( "WhiteFlag" );
	else
		pClass = TEAM_GetFlagItem( ulTeamIdx );

	pFlag = Spawn( pClass, 0, 0, 0, NO_REPLACE );
	if ( pFlag->IsKindOf( PClass::FindClass( "Flag" )) == false )
	{
		pFlag->Destroy( );
		return;
	}

	// In non-simple CTF mode, scripts take care of the returning and displaying messages.
	if ( TEAM_GetSimpleCTFMode( ) || TEAM_GetSimpleSTMode( ))
	{
		if (( NETWORK_GetState( ) != NETSTATE_CLIENT ) && ( CLIENTDEMO_IsPlaying( ) == false ))
			static_cast<AFlag *>( pFlag )->ReturnFlag( pReturner );
		static_cast<AFlag *>( pFlag )->DisplayFlagReturn( );
	}

	static_cast<AFlag *>( pFlag )->ResetReturnTicks( );
	static_cast<AFlag *>( pFlag )->AnnounceFlagReturn( );

	// Destroy the temporarily created flag.
	pFlag->Destroy( );
	pFlag = NULL;

	// Destroy any sitting flags that being returned from the return ticks running out,
	// or whatever reason.
	if (( NETWORK_GetState( ) != NETSTATE_CLIENT ) && ( CLIENTDEMO_IsPlaying( ) == false ))
	{
		while ( pFlag = Iterator.Next( ))
		{
			if (( pFlag->GetClass( ) != pClass ) || (( pFlag->flags & MF_DROPPED ) == false ))
				continue;

			// If we're the server, tell clients to destroy the flag.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_DestroyThing( pFlag );

			pFlag->Destroy( );
		}
	}

	// Tell clients that the flag has been returned.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		// I don't like these hacks :((((((((((((((
		if ( ulTeamIdx == NUM_TEAMS )
			SERVERCOMMANDS_TeamFlagReturned( NUM_TEAMS );
		else if ( ulTeamIdx == TEAM_BLUE )
			SERVERCOMMANDS_TeamFlagReturned( TEAM_BLUE );
		else if ( ulTeamIdx == TEAM_RED )
			SERVERCOMMANDS_TeamFlagReturned( TEAM_RED );
	}
	else
		SCOREBOARD_RefreshHUD( );
/*
	char						szString[256];
	DHUDMessageFadeOut			*pMsg;
	AActor						*pActor;
	POS_t						FlagOrigin;
	TThinkerIterator<AActor>	iterator;

	if ( ulTeamIdx > NUM_TEAMS )
		return;

	// Zero out the return ticks in case this hasn't been done already.
	if ( ulTeamIdx == NUM_TEAMS )
		g_ulWhiteFlagReturnTicks = 0;
	else
		g_Team[ulTeamIdx].ulReturnTicks = 0;

	// Execute the return scripts.
	if ( NETWORK_GetState( ) != NETSTATE_CLIENT )
	{
		if ( ulTeamIdx == NUM_TEAMS )
			FBehavior::StaticStartTypedScripts( SCRIPT_WhiteReturn, NULL, true );
		else
			FBehavior::StaticStartTypedScripts( (WORD)g_Team[ulTeamIdx].ulReturnScriptOffset, NULL, true );
	}

	// Play the announcer sound for the flag/skull being returned.
	if ( ulTeamIdx == TEAM_BLUE )
	{
		if ( ctf )
			ANNOUNCER_PlayEntry( cl_announcer, "BlueFlagReturned" );
		else
			ANNOUNCER_PlayEntry( cl_announcer, "BlueSkullReturned" );
	}
	else if ( ulTeamIdx == TEAM_RED )
	{
		if ( ctf )
			ANNOUNCER_PlayEntry( cl_announcer, "RedFlagReturned" );
		else
			ANNOUNCER_PlayEntry( cl_announcer, "RedSkullReturned" );
	}
	else
		ANNOUNCER_PlayEntry( cl_announcer, "WhiteFlagReturned" );

	// If this is simple CTF mode, do the return stuff.
	if (( TEAM_GetSimpleCTFMode( ) || TEAM_GetSimpleSTMode( )) && ( NETWORK_GetState( ) != NETSTATE_CLIENT ))
	{
		if ( ctf )
		{
			if ( ulTeamIdx == TEAM_BLUE )
			{
				// Remove any blue flags on the ground.
				while ( pActor = iterator.Next( ))
				{
					if ( pActor->IsKindOf( PClass::FindClass( "BlueFlag" )))
					{
						if ( NETWORK_GetState( ) == NETSTATE_SERVER )
							SERVERCOMMANDS_DestroyThing( pActor );

						pActor->Destroy( );
					}
				}

				// Respawn the blue flag.
				FlagOrigin = TEAM_GetBlueFlagOrigin( );
				pActor = Spawn( TEAM_GetFlagItem( ulTeamIdx ), FlagOrigin.x, FlagOrigin.y, FlagOrigin.z, NO_REPLACE );

				// If we're the server, tell clients to spawn the new flag.
				if (( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( pActor ))
					SERVERCOMMANDS_SpawnThing( pActor );

				// Since all inventory spawns with the MF_DROPPED flag, we need to unset it.
				if ( pActor )
					pActor->flags &= ~MF_DROPPED;

				// Create the "returned" message.
				sprintf( szString, "\\cHBlue flag returned" );
				V_ColorizeString( szString );

				// Now, print it.
				if ( NETWORK_GetState( ) != NETSTATE_SERVER )
				{
					screen->SetFont( BigFont );
					pMsg = new DHUDMessageFadeOut( szString,
						1.5f,
						0.425f,
						0,
						0,
						CR_BLUE,
						3.0f,
						0.25f );
					StatusBar->AttachMessage( pMsg, 'CNTR' );
					screen->SetFont( SmallFont );
				}
				// If necessary, send it to clients.
				else
				{
					SERVERCOMMANDS_PrintHUDMessageFadeOut( szString, 1.5f, 0.425f, 0, 0, CR_BLUE, 3.0f, 0.25f, "BigFont", 'CNTR' );
				}

				// Mark the blue flag as no longer being taken.
				TEAM_SetBlueFlagTaken( false );

				// If the opposing team's flag has been taken, the player who returned this flag
				// has the chance to earn an "Assist!" medal.
				if ( TEAM_GetRedFlagTaken( ))
				{
					if ( pReturner && pReturner->player )
						TEAM_SetAssistPlayer( TEAM_BLUE, ULONG( pReturner->player - players ));
				}
			}
			else
			{
				// Remove any red flags on the ground.
				while ( pActor = iterator.Next( ))
				{
					if ( pActor->IsKindOf( PClass::FindClass( "RedFlag" )))
					{
						if ( NETWORK_GetState( ) == NETSTATE_SERVER )
							SERVERCOMMANDS_DestroyThing( pActor );

						pActor->Destroy( );
					}
				}

				// Respawn the red flag.
				FlagOrigin = TEAM_GetRedFlagOrigin( );
				pActor = Spawn( TEAM_GetFlagItem( ulTeamIdx ), FlagOrigin.x, FlagOrigin.y, FlagOrigin.z, NO_REPLACE );

				// If we're the server, tell clients to spawn the new flag.
				if (( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( pActor ))
					SERVERCOMMANDS_SpawnThing( pActor );

				// Since all inventory spawns with the MF_DROPPED flag, we need to unset it.
				if ( pActor )
					pActor->flags &= ~MF_DROPPED;

				// Create the "returned" message.
				sprintf( szString, "\\cGRed flag returned" );
				V_ColorizeString( szString );

				// Now, print it.
				if ( NETWORK_GetState( ) != NETSTATE_SERVER )
				{
					screen->SetFont( BigFont );
					pMsg = new DHUDMessageFadeOut( szString,
						1.5f,
						0.425f,
						0,
						0,
						CR_RED,
						3.0f,
						0.25f );
					StatusBar->AttachMessage( pMsg, 'CNTR' );
					screen->SetFont( SmallFont );
				}
				// If necessary, send it to clients.
				else
				{
					SERVERCOMMANDS_PrintHUDMessageFadeOut( szString, 1.5f, 0.425f, 0, 0, CR_RED, 3.0f, 0.25f, "BigFont", 'CNTR' );
				}

				// Mark the red flag as no longer being taken.
				TEAM_SetRedFlagTaken( false );

				// If the opposing team's flag has been taken, the player who returned this flag
				// has the chance to earn an "Assist!" medal.
				if ( TEAM_GetBlueFlagTaken( ))
				{
					if ( pReturner && pReturner->player )
						TEAM_SetAssistPlayer( TEAM_RED, ULONG( pReturner->player - players ));
				}
			}
		}
		else if ( oneflagctf )
		{
			// Remove any white flags on the ground.
			while ( pActor = iterator.Next( ))
			{
				if ( pActor->IsKindOf( PClass::FindClass( "WhiteFlag" )))
				{
					if ( NETWORK_GetState( ) == NETSTATE_SERVER )
						SERVERCOMMANDS_DestroyThing( pActor );

					pActor->Destroy( );
				}
			}


			// Respawn the white flag.
			FlagOrigin = TEAM_GetWhiteFlagOrigin( );
			pActor = Spawn( PClass::FindClass( "WhiteFlag" ), FlagOrigin.x, FlagOrigin.y, FlagOrigin.z, NO_REPLACE );

			// If we're the server, tell clients to spawn the new flag.
			if (( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( pActor ))
				SERVERCOMMANDS_SpawnThing( pActor );

			// Since all inventory spawns with the MF_DROPPED flag, we need to unset it.
			if ( pActor )
				pActor->flags &= ~MF_DROPPED;

			// Create the "returned" message.
			sprintf( szString, "\\cCWhite flag returned" );
			V_ColorizeString( szString );

			// Now, print it.
			if ( NETWORK_GetState( ) != NETSTATE_SERVER )
			{
				screen->SetFont( BigFont );
				pMsg = new DHUDMessageFadeOut( szString,
					1.5f,
					0.425f,
					0,
					0,
					CR_WHITE,
					3.0f,
					0.25f );
				StatusBar->AttachMessage( pMsg, 'CNTR' );
				screen->SetFont( SmallFont );
			}
			// If necessary, send it to clients.
			else
			{
				SERVERCOMMANDS_PrintHUDMessageFadeOut( szString, 1.5f, 0.425f, 0, 0, CR_WHITE, 3.0f, 0.25f, "BigFont", 'CNTR' );
			}

			// Mark the blue flag as no longer being taken.
			TEAM_SetWhiteFlagTaken( false );
		}
	}

	// If this is simple ST mode, do the return stuff.
	if (( TEAM_GetSimpleSTMode( )) && ( NETWORK_GetState( ) != NETSTATE_CLIENT ))
	{
		if ( ulTeamIdx == TEAM_BLUE )
		{
			// Remove any blue flags on the ground.
			while ( pActor = iterator.Next( ))
			{
				if ( pActor->IsKindOf( PClass::FindClass( "BlueSkullST" )))
				{
					if ( NETWORK_GetState( ) == NETSTATE_SERVER )
						SERVERCOMMANDS_DestroyThing( pActor );

					pActor->Destroy( );
				}
			}

			// Respawn the blue skull.
			FlagOrigin = TEAM_GetBlueSkullOrigin( );
			pActor = Spawn( TEAM_GetFlagItem( ulTeamIdx ), FlagOrigin.x, FlagOrigin.y, FlagOrigin.z, NO_REPLACE );

			// If we're the server, tell clients to spawn the new skull.
			if (( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( pActor ))
				SERVERCOMMANDS_SpawnThing( pActor );

			// Since all inventory spawns with the MF_DROPPED flag, we need to unset it.
			if ( pActor )
				pActor->flags &= ~MF_DROPPED;

			// Create the "returned" message.
			sprintf( szString, "\\cHBlue skull returned" );
			V_ColorizeString( szString );

			// Now, print it.
			if ( NETWORK_GetState( ) != NETSTATE_SERVER )
			{
				screen->SetFont( BigFont );
				pMsg = new DHUDMessageFadeOut( szString,
					1.5f,
					0.425f,
					0,
					0,
					CR_BLUE,
					3.0f,
					0.25f );
				StatusBar->AttachMessage( pMsg, 'CNTR' );
				screen->SetFont( SmallFont );
			}
			// If necessary, send it to clients.
			else
			{
				SERVERCOMMANDS_PrintHUDMessageFadeOut( szString, 1.5f, 0.425f, 0, 0, CR_BLUE, 3.0f, 0.25f, "BigFont", 'CNTR' );
			}

			// Mark the blue skull as no longer being taken.
			TEAM_SetBlueSkullTaken( false );

			// If the opposing team's flag has been taken, the player who returned this skull
			// has the chance to earn an "Assist!" medal.
			if ( TEAM_GetRedSkullTaken( ))
			{
				if ( pReturner && pReturner->player )
					TEAM_SetAssistPlayer( TEAM_BLUE, ULONG( pReturner->player - players ));
			}
		}
		else
		{
			// Remove any red skulls on the ground.
			while ( pActor = iterator.Next( ))
			{
				if ( pActor->IsKindOf( PClass::FindClass( "RedSkullST" )))
				{
					if ( NETWORK_GetState( ) == NETSTATE_SERVER )
						SERVERCOMMANDS_DestroyThing( pActor );

					pActor->Destroy( );
				}
			}

			// Respawn the red skull.
			FlagOrigin = TEAM_GetRedSkullOrigin( );
			pActor = Spawn( TEAM_GetFlagItem( ulTeamIdx ), FlagOrigin.x, FlagOrigin.y, FlagOrigin.z, NO_REPLACE );

			// If we're the server, tell clients to spawn the new skull.
			if (( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( pActor ))
				SERVERCOMMANDS_SpawnThing( pActor );

			// Since all inventory spawns with the MF_DROPPED flag, we need to unset it.
			if ( pActor )
				pActor->flags &= ~MF_DROPPED;

			// Create the "returned" message.
			sprintf( szString, "\\cGRed skull returned" );
			V_ColorizeString( szString );

			// Now, print it.
			if ( NETWORK_GetState( ) != NETSTATE_SERVER )
			{
				screen->SetFont( BigFont );
				pMsg = new DHUDMessageFadeOut( szString,
					1.5f,
					0.425f,
					0,
					0,
					CR_RED,
					3.0f,
					0.25f );
				StatusBar->AttachMessage( pMsg, 'CNTR' );
				screen->SetFont( SmallFont );
			}
			// If necessary, send it to clients.
			else
			{
				SERVERCOMMANDS_PrintHUDMessageFadeOut( szString, 1.5f, 0.425f, 0, 0, CR_RED, 3.0f, 0.25f, "BigFont", 'CNTR' );
			}

			// Mark the red skull as no longer being taken.
			TEAM_SetRedSkullTaken( false );

			// If the opposing team's flag has been taken, the player who returned this skull
			// has the chance to earn an "Assist!" medal.
			if ( TEAM_GetBlueSkullTaken( ))
			{
				if ( pReturner && pReturner->player )
					TEAM_SetAssistPlayer( TEAM_RED, ULONG( pReturner->player - players ));
			}
		}
	}

	// Tell clients that the flag has been returned.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_TeamFlagReturned( ulTeamIdx );
	else
		SCOREBOARD_RefreshHUD( );
*/
}

//*****************************************************************************
//
bool TEAM_IsFlagMode( void )
{
	if ( ctf || oneflagctf )
		return ( true );

	return ( false );
}

//*****************************************************************************
//
ULONG TEAM_ChooseBestTeamForPlayer( void )
{
	LONG	lBlueScore;
	LONG	lRedScore;

	if ( teamplay )
	{
		lBlueScore = TEAM_GetFragCount( TEAM_BLUE );
		lRedScore = TEAM_GetFragCount( TEAM_RED );
	}
	else if ( teamlms )
	{
		lBlueScore = TEAM_GetWinCount( TEAM_BLUE );
		lRedScore = TEAM_GetWinCount( TEAM_RED );
	}
	else
	{
		lBlueScore = TEAM_GetScore( TEAM_BLUE );
		lRedScore = TEAM_GetScore( TEAM_RED );
	}

	// Blue has less players, so stick him on blue.
	if ( TEAM_CountPlayers( TEAM_BLUE ) < TEAM_CountPlayers( TEAM_RED ))
		return ( TEAM_BLUE );
	// Red has less players, so stick him on red.
	else if ( TEAM_CountPlayers( TEAM_RED ) < TEAM_CountPlayers( TEAM_BLUE ))
		return ( TEAM_RED );
	// Blue is losing, so stick him on blue.
	else if ( lBlueScore < lRedScore )
		return ( TEAM_BLUE );
	// Red is losing, so stick him on red.
	else if ( lRedScore < lBlueScore )
		return ( TEAM_RED );
	// Teams are tied and have equal players; pick a random team.
	else
		return ( M_Random( ) % NUM_TEAMS );
}

//*****************************************************************************
//
void TEAM_ScoreSkulltagPoint( player_s *pPlayer, ULONG ulNumPoints, AActor *pPillar )
{
	char				szString[256];
	POS_t				SkullOrigin;
	DHUDMessageFadeOut	*pMsg;
	AActor				*pActor;
	AInventory			*pInventory;

	// Create the "scored" message.
	switch ( ulNumPoints )
	{
	case 0:

		return;
	case 1:

		sprintf( szString, "\\c%s team scores!", pPlayer->ulTeam == TEAM_BLUE ? "H Blue" : "G Red" );
		break;
	case 2:

		sprintf( szString, "\\c%s scores two points!", pPlayer->ulTeam == TEAM_BLUE ? "H Blue" : "G Red" );
		break;
	case 3:

		sprintf( szString, "\\c%s scores three points!", pPlayer->ulTeam == TEAM_BLUE ? "H Blue" : "G Red" );
		break;
	case 4:

		sprintf( szString, "\\c%s scores four points!", pPlayer->ulTeam == TEAM_BLUE ? "H Blue" : "G Red" );
		break;
	case 5:

		sprintf( szString, "\\c%s scores five points!", pPlayer->ulTeam == TEAM_BLUE ? "H Blue" : "G Red" );
		break;
	default:

		sprintf( szString, "\\c%s scores %d points!", pPlayer->ulTeam == TEAM_BLUE ? "H Blue" : "G Red", ulNumPoints );
		break;
	}

	V_ColorizeString( szString );

	// Now, print it.
	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
	{
		screen->SetFont( BigFont );
		pMsg = new DHUDMessageFadeOut( szString,
			1.5,
			0.425f,
			0,
			0,
			CR_BLUE,
			3.0f,
			0.5f );
		StatusBar->AttachMessage( pMsg, 'CNTR' );
		screen->SetFont( SmallFont );
	}
	// If necessary, send it to clients.
	else
	{
		SERVERCOMMANDS_PrintHUDMessageFadeOut( szString, 1.5f, 0.425f, 0, 0, CR_BLUE, 3.0f, 0.5f, "BigFont", 'CNTR' );
	}

	// [RC] Create the "scored by" and "assisted by" message.
	sprintf( szString, "\\c%sScored by: %s", pPlayer->ulTeam == TEAM_BLUE ? "H" : "G", pPlayer->userinfo.netname);
	if ( TEAM_GetAssistPlayer( pPlayer->ulTeam ) != MAXPLAYERS ) {
		bool selfAssist = false;
		for(ULONG i = 0; i < MAXPLAYERS; i++)
			if(&players[i] == pPlayer)
				if( TEAM_GetAssistPlayer( pPlayer->ulTeam ) == i)
					selfAssist = true;

		if ( selfAssist )
			sprintf( szString, "%s\\n\\c%[ Self-Assisted ]", szString, pPlayer->ulTeam == TEAM_BLUE ? "H" : "G");
		else
			sprintf( szString, "%s\\n\\c%sAssisted by: %s", szString, pPlayer->ulTeam == TEAM_BLUE ? "H" : "G", players[TEAM_GetAssistPlayer( pPlayer->ulTeam )].userinfo.netname);
	}
	
	V_ColorizeString( szString );

	// Now, print it.
	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
	{
		screen->SetFont( SmallFont );
		pMsg = new DHUDMessageFadeOut( szString,
			1.5f,
			0.475f,
			0,
			0,
			pPlayer->ulTeam == TEAM_BLUE ? CR_BLUE : CR_RED,
			3.0f,
			0.5f );
		StatusBar->AttachMessage( pMsg, 'SUBS' );
	}
	// If necessary, send it to clients.
	else
		SERVERCOMMANDS_PrintHUDMessageFadeOut( szString, 1.5f, 0.475f, 0, 0, CR_BLUE, 3.0f, 0.5f, "SmallFont", 'SUBS' );

	// Give his team a point.
	TEAM_SetScore( pPlayer->ulTeam, TEAM_GetScore( pPlayer->ulTeam ) + ulNumPoints, true );
	pPlayer->lPointCount += ulNumPoints;

	// If we're the server, notify the clients of the pointcount change.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SetPlayerPoints( ULONG( pPlayer - players ));

	// Take the skull away.
	pInventory = pPlayer->mo->FindInventory( TEAM_GetFlagItem( !pPlayer->ulTeam ));
	if ( pInventory )
		pPlayer->mo->RemoveInventory( pInventory );

	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_TakeInventory( ULONG( pPlayer - players ), (char *)TEAM_GetFlagItem( !pPlayer->ulTeam )->TypeName.GetChars( ), 0 );
	else
		SCOREBOARD_RefreshHUD( );

	// Respawn the skull.
	SkullOrigin = pPlayer->ulTeam == TEAM_BLUE ? TEAM_GetRedSkullOrigin( ) : TEAM_GetBlueSkullOrigin( );

	pActor = Spawn( TEAM_GetFlagItem( !pPlayer->ulTeam ), SkullOrigin.x, SkullOrigin.y, SkullOrigin.z, NO_REPLACE );

	// Since all inventory spawns with the MF_DROPPED flag, we need to unset it.
	if ( pActor )
		pActor->flags &= ~MF_DROPPED;

	// If we're the server, tell clients to spawn the new skull.
	if (( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( pActor ))
		SERVERCOMMANDS_SpawnThing( pActor );

	// Mark the skull as no longer being taken.
	( pPlayer->ulTeam == TEAM_BLUE ) ? TEAM_SetRedSkullTaken( false ) : TEAM_SetBlueSkullTaken( false );

	// Award the scorer with a "Tag!" medal.
	MEDAL_GiveMedal( ULONG( pPlayer - players ), MEDAL_TAG );

	// Tell clients about the medal that been given.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_GivePlayerMedal( ULONG( pPlayer - players ), MEDAL_TAG );

	// If someone just recently returned the skull, award him with an "Assist!" medal.
	if ( TEAM_GetAssistPlayer( pPlayer->ulTeam ) != MAXPLAYERS )
	{
		MEDAL_GiveMedal( TEAM_GetAssistPlayer( pPlayer->ulTeam ), MEDAL_ASSIST );

		// Tell clients about the medal that been given.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_GivePlayerMedal( TEAM_GetAssistPlayer( pPlayer->ulTeam ), MEDAL_ASSIST );

		TEAM_SetAssistPlayer( pPlayer->ulTeam, MAXPLAYERS );
	}

	// Finally, set the appropriate state of the score pillar to that show the skull in hand.
	if ( pPlayer->ulTeam == TEAM_RED )
	{
		if ( pPillar->MeleeState )
		{
			pPillar->SetState( pPillar->MeleeState );

			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetThingState( pPillar, STATE_MELEE );
		}
	}
	else
	{
		if ( pPillar->MissileState )
		{
			pPillar->SetState( pPillar->MissileState );

			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetThingState( pPillar, STATE_MELEE );
		}
	}
}

//*****************************************************************************
//
void TEAM_DisplayNeedToReturnSkullMessage( player_s *pPlayer )
{
	char						szString[256];
	DHUDMessageFadeOut			*pMsg;

	if (( pPlayer == NULL ) || ( pPlayer->bOnTeam == false ))
		return;

	// Create the message.
	if ( pPlayer->ulTeam == TEAM_BLUE )
		sprintf( szString, "\\cHThe blue skull must be returned first!" );
	else
		sprintf( szString, "\\cGThe red skull must be returned first!" );
	V_ColorizeString( szString );

	// Now, print it.
	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
	{
		pMsg = new DHUDMessageFadeOut( szString,
			1.5f,
			0.425f,
			0,
			0,
			CR_RED,
			1.0f,
			0.25f );
		StatusBar->AttachMessage( pMsg, 'CNTR' );
	}
	// If necessary, send it to clients.
	else
	{
		SERVERCOMMANDS_PrintHUDMessageFadeOut( szString, 1.5f, 0.425f, 0, 0, CR_RED, 1.0f, 0.25f, "SmallFont", 'CNTR', ULONG( pPlayer - players ), SVCF_ONLYTHISCLIENT );
	}
}

//*****************************************************************************
//
void TEAM_FlagDropped( player_s *pPlayer )
{
	DHUDMessageFadeOut	*pMsg;
	char				szString[64];

	// First, make sure the player is valid, and on a valid team.
	if (( pPlayer == NULL ) ||
		(( pPlayer - players ) >= MAXPLAYERS ) ||
		(( pPlayer - players ) < 0 ) ||
		( pPlayer->bOnTeam == false ) ||
		( pPlayer->ulTeam >= NUM_TEAMS ))
	{
		return;
	}

	// Next, build the dropped message.
	sprintf( szString, "\\c%s%s %s dropped!", ( pPlayer->ulTeam != TEAM_BLUE ) ? "H" : "G", ( pPlayer->ulTeam != TEAM_BLUE ) ? "Blue" : "Red", ( skulltag ) ? "skull" : "flag" );

	// Colorize it.
	V_ColorizeString( szString );

	// Now, print it.
	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
	{
		screen->SetFont( BigFont );
		pMsg = new DHUDMessageFadeOut( szString,
			1.5f,
			0.425f,
			0,
			0,
			CR_WHITE,
			3.0f,
			0.25f );
		StatusBar->AttachMessage( pMsg, 'CNTR' );
		screen->SetFont( SmallFont );
	}
	// If necessary, send it to clients.
	else
		SERVERCOMMANDS_PrintHUDMessageFadeOut( szString, 1.5f, 0.425f, 0, 0, CR_WHITE, 3.0f, 0.25f, "BigFont", 'CNTR' );

	// Finally, play the announcer entry associated with this event.
	sprintf( szString, "%s%sDropped", ( pPlayer->ulTeam != TEAM_BLUE ) ? "Blue" : "Red", ( skulltag ) ? "skull" : "flag" );
	ANNOUNCER_PlayEntry( cl_announcer, szString );
}

//*****************************************************************************
//
WORD TEAM_GetReturnScriptOffset( ULONG ulTeamIdx )
{
	if ( ulTeamIdx < NUM_TEAMS )
		return ( (WORD)g_Team[ulTeamIdx].ulReturnScriptOffset );
	else
		return ( -1 );
}


//*****************************************************************************
//*****************************************************************************
//
char *TEAM_GetName( ULONG ulTeamIdx )
{
	if ( ulTeamIdx < NUM_TEAMS )
		return ( g_Team[ulTeamIdx].szName );
	else
		return ( NULL );
}

//*****************************************************************************
//
void TEAM_SetName( ULONG ulTeamIdx, char *pszName )
{
	if ( ulTeamIdx < NUM_TEAMS )
		sprintf( g_Team[ulTeamIdx].szName, pszName );
}

//*****************************************************************************
//
char *TEAM_GetColor( ULONG ulTeamIdx )
{
	if ( ulTeamIdx < NUM_TEAMS )
		return ( g_Team[ulTeamIdx].szColor );
	else
		return ( NULL );
}

//*****************************************************************************
//
void TEAM_SetColor( ULONG ulTeamIdx, char *pszColor )
{
	if ( ulTeamIdx < NUM_TEAMS )
		sprintf( g_Team[ulTeamIdx].szColor, pszColor );
}

//*****************************************************************************
//
ULONG TEAM_GetTextColor( ULONG ulTeamIdx )
{
	if ( ulTeamIdx < NUM_TEAMS )
		return ( g_Team[ulTeamIdx].ulTextColor );
	else
		return ( NULL );
}

//*****************************************************************************
//
void TEAM_SetTextColor( ULONG ulTeamIdx, ULONG ulColor )
{
	if ( ulTeamIdx < NUM_TEAMS )
		g_Team[ulTeamIdx].ulTextColor = ulColor;
}

//*****************************************************************************
//
LONG TEAM_GetRailgunColor( ULONG ulTeamIdx )
{
	if ( ulTeamIdx < NUM_TEAMS )
		return ( g_Team[ulTeamIdx].lRailColor );
	else
		return ( NULL );
}

//*****************************************************************************
//
void TEAM_SetRailgunColor( ULONG ulTeamIdx, LONG lColor )
{
	if ( ulTeamIdx < NUM_TEAMS )
		g_Team[ulTeamIdx].lRailColor = lColor;
}

//*****************************************************************************
//
LONG TEAM_GetScore( ULONG ulTeamIdx )
{
	if ( ulTeamIdx < NUM_TEAMS )
		return ( g_Team[ulTeamIdx].lScore );
	else
		return ( NULL );
}

//*****************************************************************************
//
void TEAM_SetScore( ULONG ulTeamIdx, LONG lScore, bool bAnnouncer )
{
	LONG				lOldScore;
	char				szString[32];
	DHUDMessageFadeOut	*pMsg;

	if ( ulTeamIdx >= NUM_TEAMS )
		return;

	lOldScore = g_Team[ulTeamIdx].lScore;
	g_Team[ulTeamIdx].lScore = lScore;
	if ( bAnnouncer && ( g_Team[ulTeamIdx].lScore > lOldScore ))
	{
		// Play the announcer sound for scoring.
		if ( ulTeamIdx == TEAM_BLUE )
			ANNOUNCER_PlayEntry( cl_announcer, "BlueScores" );
		else
			ANNOUNCER_PlayEntry( cl_announcer, "RedScores" );
	}

	// If we're the server, tell clients about the team score update.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		SERVERCOMMANDS_SetTeamScore( ulTeamIdx, lScore, bAnnouncer );

		// Also, update the scoreboard.
		SERVERCONSOLE_UpdateScoreboard( );
	}

	// Implement the pointlimit.
	if ( pointlimit <= 0 || ( NETWORK_GetState( ) == NETSTATE_CLIENT ) || ( CLIENTDEMO_IsPlaying( ) == false ))
		return;
/*
	// Potentially play the "3 points left", etc. announcer sounds.
	if ( teampossession && pointlimit )
	{
		if (( pointlimit - g_Team[ulTeamIdx].lScore ) == 3 && ( g_bThreePointsLeftSoundPlayed == false ))
		{
			if ( ANNOUNCER_GetThreePointsLeftSound( cl_announcer ))
				S_Sound( CHAN_VOICE, ANNOUNCER_GetThreePointsLeftSound( cl_announcer ), 1, ATTN_NONE );

			g_bThreePointsLeftSoundPlayed = true;
		}
		if (( pointlimit - g_Team[ulTeamIdx].lScore ) == 2 && ( g_bTwoPointsLeftSoundPlayed == false ))
		{
			if ( ANNOUNCER_GetTwoPointsLeftSound( cl_announcer ))
				S_Sound( CHAN_VOICE, ANNOUNCER_GetTwoPointsLeftSound( cl_announcer ), 1, ATTN_NONE );

			g_bTwoPointsLeftSoundPlayed = true;
		}
		if (( pointlimit - g_Team[ulTeamIdx].lScore ) == 1 && ( g_bOnePointLeftSoundPlayed == false ))
		{
			if ( ANNOUNCER_GetOnePointLeftSound( cl_announcer ))
				S_Sound( CHAN_VOICE, ANNOUNCER_GetOnePointLeftSound( cl_announcer ), 1, ATTN_NONE );

			g_bOnePointLeftSoundPlayed = true;
		}
	}
*/
	if ( g_Team[ulTeamIdx].lScore >= (LONG)pointlimit )
	{
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVER_Printf( PRINT_HIGH, "%s has won the game!\n", g_Team[ulTeamIdx].szName );
		else
			Printf( "%s has won the game!\n", g_Team[ulTeamIdx].szName );

		// Display "%s WINS!" HUD message.
		if ( ulTeamIdx == TEAM_BLUE )
			sprintf( szString, "\\chBLUE WINS!" );
		else
			sprintf( szString, "\\cgRED WINS!" );

		V_ColorizeString( szString );

		if ( NETWORK_GetState( ) != NETSTATE_SERVER )
		{
			screen->SetFont( BigFont );
			pMsg = new DHUDMessageFadeOut( szString,
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
		else
		{
			SERVERCOMMANDS_PrintHUDMessageFadeOut( szString, 160.4f, 75.0f, 320, 200, CR_RED, 3.0f, 0.25f, "BigFont", 'CNTR' );
		}

		// End the level after five seconds.
		GAME_SetEndLevelDelay( 5 * TICRATE );
	}
}

//*****************************************************************************
//
const PClass *TEAM_GetFlagItem( ULONG ulTeamIdx )
{
	if ( ulTeamIdx < NUM_TEAMS )
		return ( g_Team[ulTeamIdx].FlagItem );
	else
		return ( NULL );
}

//*****************************************************************************
//
ULONG TEAM_GetReturnTicks( ULONG ulTeamIdx )
{
	if ( ulTeamIdx < NUM_TEAMS )
		return ( g_Team[ulTeamIdx].ulReturnTicks );
	else if ( ulTeamIdx == NUM_TEAMS )
		return ( g_ulWhiteFlagReturnTicks );
	else
		return ( NULL );
}

//*****************************************************************************
//
void TEAM_SetReturnTicks( ULONG ulTeamIdx, ULONG ulTicks )
{
	switch ( ulTeamIdx )
	{
	case TEAM_BLUE:
	case TEAM_RED:

		g_Team[ulTeamIdx].ulReturnTicks = ulTicks;
		break;
	case NUM_TEAMS:

		g_ulWhiteFlagReturnTicks = ulTicks;
		break;
	default:

		return;
	}

	// If we're the server, tell clients to update their team return ticks.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SetTeamReturnTicks( ulTeamIdx, ( ulTeamIdx == NUM_TEAMS ) ? g_ulWhiteFlagReturnTicks : g_Team[ulTeamIdx].ulReturnTicks );
}

//*****************************************************************************
//
LONG TEAM_GetFragCount( ULONG ulTeamIdx )
{
	if ( ulTeamIdx < NUM_TEAMS )
		return ( g_Team[ulTeamIdx].lFragCount );
	else
		return ( NULL );
}

//*****************************************************************************
//
void TEAM_SetFragCount( ULONG ulTeamIdx, LONG lFragCount, bool bAnnounce )
{
	// Invalid team.
	if ( ulTeamIdx >= NUM_TEAMS )
		return;

	// Potentially play some announcer sounds resulting from this frag ("Teams are tied!"),
	// etc.
	if (( bAnnounce ) &&
		( teamlms == false ) &&
		( teampossession == false ) &&
		( teamgame == false ))
	{
		ANNOUNCER_PlayTeamFragSounds( ulTeamIdx, g_Team[ulTeamIdx].lFragCount, lFragCount );
	}

	// Set the fragcount.
	g_Team[ulTeamIdx].lFragCount = lFragCount;

	// If we're the server, let clients know that the score has changed.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		SERVERCOMMANDS_SetTeamFrags( ulTeamIdx, lFragCount, bAnnounce );

		// Also, update the scoreboard.
		SERVERCONSOLE_UpdateScoreboard( );
	}
}

//*****************************************************************************
//
LONG TEAM_GetDeathCount( ULONG ulTeamIdx )
{
	if ( ulTeamIdx < NUM_TEAMS )
		return ( g_Team[ulTeamIdx].lDeathCount );
	else
		return ( NULL );
}

//*****************************************************************************
//
void TEAM_SetDeathCount( ULONG ulTeamIdx, LONG lDeathCount )
{
	// Invalid team.
	if ( ulTeamIdx >= NUM_TEAMS )
		return;
	else
		g_Team[ulTeamIdx].lDeathCount = lDeathCount;
}

//*****************************************************************************
//
LONG TEAM_GetWinCount( ULONG ulTeamIdx )
{
	if ( ulTeamIdx < NUM_TEAMS )
		return ( g_Team[ulTeamIdx].lWinCount );
	else
		return ( NULL );
}

//*****************************************************************************
//
void TEAM_SetWinCount( ULONG ulTeamIdx, LONG lWinCount, bool bAnnounce )
{
	LONG	lOldWinCount;

	if ( ulTeamIdx >= NUM_TEAMS )
		return;

	lOldWinCount = g_Team[ulTeamIdx].lWinCount;
	g_Team[ulTeamIdx].lWinCount = lWinCount;
	if (( bAnnounce ) && ( g_Team[ulTeamIdx].lWinCount > lOldWinCount ))
	{
		// Play the announcer sound for scoring.
		if ( ulTeamIdx == TEAM_BLUE )
			ANNOUNCER_PlayEntry( cl_announcer, "BlueScores" );
		else
			ANNOUNCER_PlayEntry( cl_announcer, "RedScores" );
	}

	// If we're the server, tell clients about the team score update.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		SERVERCOMMANDS_SetTeamWins( ulTeamIdx, lWinCount, bAnnounce );

		// Also, update the scoreboard.
		SERVERCONSOLE_UpdateScoreboard( );
	}
}

//*****************************************************************************
//
bool TEAM_GetSimpleCTFMode( void )
{
	return ( g_bSimpleCTFMode );
}

//*****************************************************************************
//
void TEAM_SetSimpleCTFMode( bool bSimple )
{
	g_bSimpleCTFMode = bSimple;
}

//*****************************************************************************
//
bool TEAM_GetSimpleSTMode( void )
{
	return ( g_bSimpleSTMode );
}

//*****************************************************************************
//
void TEAM_SetSimpleSTMode( bool bSimple )
{
	g_bSimpleSTMode = bSimple;
}

//*****************************************************************************
//
bool TEAM_GetBlueFlagTaken( void )
{
	return ( g_bBlueFlagTaken );
}

//*****************************************************************************
//
void TEAM_SetBlueFlagTaken( bool bTaken )
{
	g_bBlueFlagTaken = bTaken;
}

//*****************************************************************************
//
bool TEAM_GetRedFlagTaken( void )
{
	return ( g_bRedFlagTaken );
}

//*****************************************************************************
//
void TEAM_SetRedFlagTaken( bool bTaken )
{
	g_bRedFlagTaken = bTaken;
}

//*****************************************************************************
//
bool TEAM_GetWhiteFlagTaken( void )
{
	return ( g_bWhiteFlagTaken );
}

//*****************************************************************************
//
void TEAM_SetWhiteFlagTaken( bool bTaken )
{
	g_bWhiteFlagTaken = bTaken;
}

//*****************************************************************************
//
bool TEAM_GetBlueSkullTaken( void )
{
	return ( g_bBlueSkullTaken );
}

//*****************************************************************************
//
void TEAM_SetBlueSkullTaken( bool bTaken )
{
	g_bBlueSkullTaken = bTaken;
}

//*****************************************************************************
//
bool TEAM_GetRedSkullTaken( void )
{
	return ( g_bRedSkullTaken );
}

//*****************************************************************************
//
void TEAM_SetRedSkullTaken( bool bTaken )
{
	g_bRedSkullTaken = bTaken;
}

//*****************************************************************************
//
POS_t TEAM_GetBlueFlagOrigin( void )
{
	return ( g_BlueFlagOrigin );
}

//*****************************************************************************
//
void TEAM_SetBlueFlagOrigin( POS_t Origin )
{
	g_BlueFlagOrigin = Origin;
}

//*****************************************************************************
//
POS_t TEAM_GetRedFlagOrigin( void )
{
	return ( g_RedFlagOrigin );
}

//*****************************************************************************
//
void TEAM_SetRedFlagOrigin( POS_t Origin )
{
	g_RedFlagOrigin = Origin;
}

//*****************************************************************************
//
POS_t TEAM_GetWhiteFlagOrigin( void )
{
	return ( g_WhiteFlagOrigin );
}

//*****************************************************************************
//
void TEAM_SetWhiteFlagOrigin( POS_t Origin )
{
	g_WhiteFlagOrigin = Origin;
}

//*****************************************************************************
//
POS_t TEAM_GetBlueSkullOrigin( void )
{
	return ( g_BlueSkullOrigin );
}

//*****************************************************************************
//
void TEAM_SetBlueSkullOrigin( POS_t Origin )
{
	g_BlueSkullOrigin = Origin;
}

//*****************************************************************************
//
POS_t TEAM_GetRedSkullOrigin( void )
{
	return ( g_RedSkullOrigin );
}

//*****************************************************************************
//
void TEAM_SetRedSkullOrigin( POS_t Origin )
{
	g_RedSkullOrigin = Origin;
}

//*****************************************************************************
//
ULONG TEAM_GetAssistPlayer( ULONG ulTeamIdx )
{
	if ( ulTeamIdx >= NUM_TEAMS )
		return ( MAXPLAYERS );

	return ( g_ulAssistPlayer[ulTeamIdx] );
}

//*****************************************************************************
//
void TEAM_SetAssistPlayer( ULONG ulTeamIdx, ULONG ulPlayer )
{
	if (( ulTeamIdx >= NUM_TEAMS ) || ( ulPlayer > MAXPLAYERS ))
		return;

	g_ulAssistPlayer[ulTeamIdx] = ulPlayer;
}

//*****************************************************************************
//	CONSOLE COMMANDS/VARIABLES

CUSTOM_CVAR( Bool, teamgame, false, CVAR_SERVERINFO | CVAR_LATCH | CVAR_CAMPAIGNLOCK )
{
	UCVarValue	Val;

	if ( self == true )
	{
		Val.Bool = false;

		// Disable deathmatch if we're setting a teamgame.
		deathmatch.ForceSet( Val, CVAR_Bool );

		// Disable cooperative if we're setting a teamgame.
		cooperative.ForceSet( Val, CVAR_Bool );
	}
	else
	{
		Val.Bool = false;
		// Teamgame has been disabled, so disable all related modes.
		ctf.ForceSet( Val, CVAR_Bool );
		oneflagctf.ForceSet( Val, CVAR_Bool );
		skulltag.ForceSet( Val, CVAR_Bool );

		// If deathmatch is also disabled, enable cooperative mode.
		if ( deathmatch == false )
		{
			Val.Bool = true;

			if ( cooperative != true )
				cooperative.ForceSet( Val, CVAR_Bool );
		}
	}

	// Reset what the current game mode is.
	GAMEMODE_DetermineGameMode( );
}

//*****************************************************************************
//
CUSTOM_CVAR( Bool, ctf, false, CVAR_SERVERINFO | CVAR_LATCH | CVAR_CAMPAIGNLOCK )
{
	UCVarValue	Val;

	if ( self == true )
	{
		Val.Bool = true;
		// Enable teamgame if we're playing CTF.
		teamgame.ForceSet( Val, CVAR_Bool );

		Val.Bool = false;
		// Disable other modes.
		oneflagctf.ForceSet( Val, CVAR_Bool );
		skulltag.ForceSet( Val, CVAR_Bool );

		g_Team[TEAM_BLUE].FlagItem = PClass::FindClass( "BlueFlag" );
		g_Team[TEAM_RED].FlagItem = PClass::FindClass( "RedFlag" );
	}
/*
	else
	{
		if ( TEAM_IsFlagMode( ) == false )
		{
			g_Team[TEAM_BLUE].FlagHeld = it_blueskull;
			g_Team[TEAM_BLUE].FlagItem = RUNTIME_CLASS( ABlueSkull );

			g_Team[TEAM_RED].FlagHeld = it_redskull;
			g_Team[TEAM_RED].FlagItem = RUNTIME_CLASS( ARedSkull );
		}
	}
*/

	// Reset what the current game mode is.
	GAMEMODE_DetermineGameMode( );
}

//*****************************************************************************
//
CUSTOM_CVAR( Bool, oneflagctf, false, CVAR_SERVERINFO | CVAR_LATCH | CVAR_CAMPAIGNLOCK )
{
	UCVarValue	Val;

	if ( self == true )
	{
		Val.Bool = true;
		// Enable teamgame if we're playing CTF.
		teamgame.ForceSet( Val, CVAR_Bool );

		Val.Bool = false;
		// Disable other modes.
		ctf.ForceSet( Val, CVAR_Bool );
		skulltag.ForceSet( Val, CVAR_Bool );

		g_Team[TEAM_BLUE].FlagItem = PClass::FindClass( "BlueFlag" );
		g_Team[TEAM_RED].FlagItem = PClass::FindClass( "RedFlag" );
	}

	// Reset what the current game mode is.
	GAMEMODE_DetermineGameMode( );
}

//*****************************************************************************
//
CUSTOM_CVAR( Bool, skulltag, false, CVAR_SERVERINFO | CVAR_LATCH | CVAR_CAMPAIGNLOCK )
{
	UCVarValue	Val;

	if ( self == true )
	{
		Val.Bool = true;
		// Enable teamgame if we're playing CTF.
		teamgame.ForceSet( Val, CVAR_Bool );

		Val.Bool = false;
		// Disable other modes.
		ctf.ForceSet( Val, CVAR_Bool );
		oneflagctf.ForceSet( Val, CVAR_Bool );

		g_Team[TEAM_BLUE].FlagItem = PClass::FindClass( "BlueSkullST" );
		g_Team[TEAM_RED].FlagItem = PClass::FindClass( "RedSkullST" );
	}

	// Reset what the current game mode is.
	GAMEMODE_DetermineGameMode( );
}

//*****************************************************************************
//
CCMD( team )
{
	// Not a valid team mode. Ignore.
	if (( teamgame == false ) && ( teamplay == false ) && ( teamlms == false ) && ( teampossession == false ))
		return;

	// If the played inputted a team they'd like to join (such as, "team red"), handle that
	// with the changeteam command.
	if ( argv.argc( ) > 1 )
	{
		char	szCommand[64];

		sprintf( szCommand, "changeteam %s", argv[1] );
		AddCommandString( szCommand );
	}
	// If they didn't, just display which team they're on.
	else
	{
		if ( players[consoleplayer].bOnTeam )
		{
			if ( players[consoleplayer].ulTeam == TEAM_RED )
				Printf( "You are on the \\cgRED \\c-team.\n" );
			else
				Printf( "You are on the \\chBLUE \\c-team.\n" );
		}
		else
			Printf( "You are not currently on a team.\n" );
	}
}

//*****************************************************************************
//
CCMD( changeteam )
{
	LONG	lDesiredTeam;

	// The server can't do this!
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		return;

	if ( playeringame[consoleplayer] == false )
		return;

	// Not in a level.
	if ( gamestate != GS_LEVEL )
		return;

	// Not a team mode.
	if (( GAMEMODE_GetFlags( GAMEMODE_GetCurrentMode( )) & GMF_PLAYERSONTEAMS ) == false )
	{
		Printf( "You can only change your team in a team game.\n" );
		return;
	}

	// "No team change" dmflag is set. Ignore this.
	if ( dmflags2 & DF2_NO_TEAM_SWITCH )
	{
		Printf( "You are not allowed to change your team!\n" );
		return;
	}

	// Can't change teams in a campaign!
	if ( CAMPAIGN_InCampaign( ))
	{
		Printf( "You cannot change teams in the middle of a campaign!\n" );
		return;
	}

	lDesiredTeam = NUM_TEAMS;

	// If we're a client, just send out the "change team" message.
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
	{
		UCVarValue	Val;

		if ( argv.argc( ) > 1 )
		{
			if ( stricmp( argv[1], "random" ) == 0 )
			{
				if ( g_JoinTeamSeed.Random2( ) % NUM_TEAMS == TEAM_BLUE )
					lDesiredTeam = TEAM_BLUE;
				else
					lDesiredTeam = TEAM_RED;
			}
			else if ( stricmp( argv[1], "autoselect" ) == 0 )
				lDesiredTeam = NUM_TEAMS;
			else
			{
				ULONG	ulIdx;

				// If the arg we passed in matches the team index or the team name, set that
				// team as the desired team.
				for ( ulIdx = 0; ulIdx < NUM_TEAMS; ulIdx++ )
				{
					if (( (ULONG)atoi( argv[1] ) == ulIdx ) || ( stricmp( argv[1], TEAM_GetName( ulIdx )) == 0 ))
						lDesiredTeam = ulIdx;
				}
			}
		}
		// We did not pass in a team, so we must want to toggle our team.
		else if ( lDesiredTeam == NUM_TEAMS )
		{
			// Can't toggle our teams if we're not on a team!
			if ( players[consoleplayer].bOnTeam == false )
				return;

			lDesiredTeam = !players[consoleplayer].ulTeam;
		}

		Val = cl_joinpassword.GetGenericRep( CVAR_String );
		CLIENTCOMMANDS_ChangeTeam( Val.String, lDesiredTeam );
		return;
	}

	if ( argv.argc( ) > 1 )
	{
		if ( stricmp( argv[1], "random" ) == 0 )
		{
			if ( g_JoinTeamSeed.Random2( ) % NUM_TEAMS == TEAM_BLUE )
				lDesiredTeam = TEAM_BLUE;
			else
				lDesiredTeam = TEAM_RED;
		}
		else if ( stricmp( argv[1], "autoselect" ) == 0 )
			lDesiredTeam = TEAM_ChooseBestTeamForPlayer( );
		else
		{
			ULONG	ulIdx;

			// If the arg we passed in matches the team index or the team name, set that
			// team as the desired team.
			for ( ulIdx = 0; ulIdx < NUM_TEAMS; ulIdx++ )
			{
				if (( (ULONG)atoi( argv[1] ) == ulIdx ) || ( stricmp( argv[1], TEAM_GetName( ulIdx )) == 0 ))
					lDesiredTeam = ulIdx;
			}
		}
	}
	// We did not pass in a team, so we must want to toggle our team.
	else
	{
		// Can't toggle our teams if we're not on a team!
		if ( players[consoleplayer].bOnTeam == false )
			return;

		lDesiredTeam = !players[consoleplayer].ulTeam;
	}

	// If the desired team matches our current team, break out.
	if (( players[consoleplayer].bOnTeam ) && ( lDesiredTeam == players[consoleplayer].ulTeam ))
		return;

	{
		bool	bOnTeam;

		// Don't allow him to bring flags, skulls, terminators, etc. with him.
		if ( players[consoleplayer].mo )
			players[consoleplayer].mo->DropImportantItems( false );

		// Save this. This will determine our message.
		bOnTeam = players[consoleplayer].bOnTeam;

		// Set the new team.
		PLAYER_SetTeam( &players[consoleplayer], lDesiredTeam, true );

		// Player was on a team, so tell everyone that he's changing teams.
		if ( bOnTeam )
		{
			if ( players[consoleplayer].ulTeam == TEAM_BLUE )
				Printf( "%s \\c-defected to the \\ch%s \\c-team.\n", players[consoleplayer].userinfo.netname, TEAM_GetName( players[consoleplayer].ulTeam ));
			else
				Printf( "%s \\c-defected to the \\cg%s \\c-team.\n", players[consoleplayer].userinfo.netname, TEAM_GetName( players[consoleplayer].ulTeam ));
		}
		// Otherwise, tell everyone he's joining a team.
		else
		{
			if ( players[consoleplayer].ulTeam == TEAM_BLUE )
				Printf( "%s \\c-joined the \\ch%s \\c-team.\n", players[consoleplayer].userinfo.netname, TEAM_GetName( players[consoleplayer].ulTeam ));
			else
				Printf( "%s \\c-joined the \\cg%s \\c-team.\n", players[consoleplayer].userinfo.netname, TEAM_GetName( players[consoleplayer].ulTeam ));
		}

		if ( players[consoleplayer].mo )
		{
			players[consoleplayer].mo->Destroy( );
			players[consoleplayer].mo = NULL;
		}

		// Now respawn the player at the appropriate spot. Set the player state to
		// PST_REBORNNOINVENTORY so everything (weapons, etc.) is cleared.
		players[consoleplayer].playerstate = PST_REBORNNOINVENTORY;

		// Also, take away spectator status.
		players[consoleplayer].bSpectating = false;

		if ( teamgame )
			G_TeamgameSpawnPlayer( consoleplayer, players[consoleplayer].ulTeam, true );
		else
			G_DeathMatchSpawnPlayer( consoleplayer, true );
	}
}

//*****************************************************************************
//*****************************************************************************
//
CUSTOM_CVAR( Int, pointlimit, 0, CVAR_SERVERINFO | CVAR_CAMPAIGNLOCK )
{
	if (( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( gamestate != GS_STARTUP ))
	{
		SERVER_Printf( PRINT_HIGH, "%s changed to: %d\n", self.GetName( ), (LONG)self );
		SERVERCOMMANDS_SetGameModeLimits( );

		// Update the scoreboard.
		SERVERCONSOLE_UpdateScoreboard( );
	}
}

// Allow the server to set the return time for flags/skulls.
CVAR( Int, sv_flagreturntime, 15, CVAR_CAMPAIGNLOCK );
