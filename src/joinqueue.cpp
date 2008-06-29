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
// Date created:  3/25/04
//
//
// Filename: joinqueue.cpp
//
// Description: Contains join queue routines
//
//-----------------------------------------------------------------------------

#include "announcer.h"
#include "c_cvars.h"
#include "cl_demo.h"
#include "cooperative.h"
#include "deathmatch.h"
#include "duel.h"
#include "g_game.h"
#include "gamemode.h"
#include "invasion.h"
#include "lastmanstanding.h"
#include "joinqueue.h"
#include "network.h"
#include "possession.h"
#include "survival.h"
#include "sv_commands.h"
#include "team.h"

//*****************************************************************************
//	VARIABLES

static	JOINSLOT_t	g_lJoinQueue[MAXPLAYERS];
static	LONG		g_lClientQueuePosition;

//*****************************************************************************
//	FUNCTIONS

void JOINQUEUE_Construct( void )
{
	g_lClientQueuePosition = -1;

	// Initialize the join queue.
	JOINQUEUE_ClearList( );
}

//*****************************************************************************
//
void JOINQUEUE_PlayerLeftGame( bool bWantPop )
{
	bool	bPop = true;
	ULONG	ulIdx;

	// If we're in a duel, revert to the "waiting for players" state.
	if ( duel )
		DUEL_SetState( DS_WAITINGFORPLAYERS );

	// If only one (or zero) person is left, go back to "waiting for players".
	if ( lastmanstanding )
	{
		// Someone just won by default!
		if (( GAME_CountLivingPlayers( ) == 1 ) && ( LASTMANSTANDING_GetState( ) == LMSS_INPROGRESS ))
		{
			LONG	lWinner;

			lWinner = LASTMANSTANDING_GetLastManStanding( );
			if ( lWinner != -1 )
			{
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVER_Printf( PRINT_HIGH, "%s \\c-wins!\n", players[lWinner].userinfo.netname );
				else
				{
					Printf( "%s \\c-wins!\n", players[lWinner].userinfo.netname );

					if ( lWinner == consoleplayer )
						ANNOUNCER_PlayEntry( cl_announcer, "YouWin" );
				}

				// Give the winner a win.
				PLAYER_SetWins( &players[lWinner], players[lWinner].ulWins + 1 );

				// Pause for five seconds for the win sequence.
				LASTMANSTANDING_DoWinSequence( lWinner );
			}

			// Join queue will be popped upon state change.
			bPop = false;

			GAME_SetEndLevelDelay( 5 * TICRATE );
		}
		else if ( SERVER_CalcNumNonSpectatingPlayers( MAXPLAYERS ) <= 1 )
			LASTMANSTANDING_SetState( LMSS_WAITINGFORPLAYERS );
	}

	if ( teamlms )
	{
		// Someone just won by default!
		if (( LASTMANSTANDING_GetState( ) == LMSS_INPROGRESS ) &&
			(( LASTMANSTANDING_TeamCountMenStanding( TEAM_BLUE ) <= 0 ) ||
			( LASTMANSTANDING_TeamCountMenStanding( TEAM_RED ) <= 0 )))
		{
			LONG	lWinner;

			lWinner = LASTMANSTANDING_TeamGetLastManStanding( );
			if ( lWinner != -1 )
			{
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVER_Printf( PRINT_HIGH, "%s \\c-wins!\n", TEAM_GetName( lWinner ));
				else
				{
					Printf( "%s \\c-wins!\n", TEAM_GetName( lWinner ));

					if ( players[consoleplayer].bOnTeam && ( lWinner == (LONG)players[consoleplayer].ulTeam ))
						ANNOUNCER_PlayEntry( cl_announcer, "YouWin" );
				}

				// Give the team a win.
				TEAM_SetWinCount( lWinner, TEAM_GetWinCount( lWinner ) + 1, false );

				// Pause for five seconds for the win sequence.
				LASTMANSTANDING_DoWinSequence( lWinner );
			}

			// Join queue will be popped upon state change.
			bPop = false;

			GAME_SetEndLevelDelay( 5 * TICRATE );
		}
		else if (( TEAM_CountPlayers( TEAM_BLUE ) == 0 ) || ( TEAM_CountPlayers( TEAM_RED ) == 0 ))
			LASTMANSTANDING_SetState( LMSS_WAITINGFORPLAYERS );
	}

	// If we're in possession mode, revert to the "waiting for players" state.
	if ( possession && ( SERVER_CalcNumNonSpectatingPlayers( MAXPLAYERS ) < 1 ))
		POSSESSION_SetState( PSNS_WAITINGFORPLAYERS );

	if ( teampossession && (( TEAM_CountPlayers( TEAM_BLUE ) == 0 ) || ( TEAM_CountPlayers( TEAM_RED ) == 0 )))
		POSSESSION_SetState( PSNS_WAITINGFORPLAYERS );

	// If we're in invasion mode, revert to the "waiting for players" state.
	if ( invasion && ( SERVER_CalcNumNonSpectatingPlayers( MAXPLAYERS ) < 1 ))
		INVASION_SetState( IS_WAITINGFORPLAYERS );

	// If we're in survival co-op mode, revert to the "waiting for players" state.
	if ( survival && ( SERVER_CalcNumNonSpectatingPlayers( MAXPLAYERS ) < 1 ))
		SURVIVAL_SetState( SURVS_WAITINGFORPLAYERS );

	// Potentially let one person join the game.
	if ( bPop && bWantPop )
		JOINQUEUE_PopQueue( 1 );
	return;

	// Nothing to do if there's nobody waiting in the queue.
	if ( g_lJoinQueue[0].ulPlayer == MAXPLAYERS )
		return;

	// Try to find the next person in line.
	ulIdx = 0;
	while ( 1 )
	{
		// Found end of list.
		if (( g_lJoinQueue[ulIdx].ulPlayer == MAXPLAYERS ) || ( ulIdx == MAXPLAYERS ))
			break;

		// Found a player waiting in line. They will now join the game!
		if ( playeringame[g_lJoinQueue[ulIdx].ulPlayer] )
		{
			players[g_lJoinQueue[ulIdx].ulPlayer].playerstate = PST_ENTERNOINVENTORY;
			players[g_lJoinQueue[ulIdx].ulPlayer].bSpectating = false;
			players[g_lJoinQueue[ulIdx].ulPlayer].bDeadSpectator = false;

			if ( players[g_lJoinQueue[ulIdx].ulPlayer].pSkullBot )
				players[g_lJoinQueue[ulIdx].ulPlayer].pSkullBot->PostEvent( BOTEVENT_JOINEDGAME );

			// Begin the duel countdown.
			if ( duel )
			{
				if ( sv_duelcountdowntime > 0 )
					DUEL_StartCountdown(( sv_duelcountdowntime * TICRATE ) - 1 );
				else
					DUEL_StartCountdown(( 10 * TICRATE ) - 1 );
			}
			// Begin the LMS countdown.
			else if ( lastmanstanding )
			{
				if ( sv_lmscountdowntime > 0 )
					LASTMANSTANDING_StartCountdown(( sv_lmscountdowntime * TICRATE ) - 1 );
				else
					LASTMANSTANDING_StartCountdown(( 10 * TICRATE ) - 1 );
			}
			else
			{
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVER_Printf( PRINT_HIGH, "%s \\c-joined the game.\n", players[g_lJoinQueue[ulIdx].ulPlayer].userinfo.netname );
				else
					Printf( "%s \\c-joined the game.\n", players[g_lJoinQueue[ulIdx].ulPlayer].userinfo.netname );
			}

			break;
		}

		ulIdx++;
	}

	// Shift all the slot positions up one.
	for ( ulIdx = 0; ulIdx < ( MAXPLAYERS - 1 ); ulIdx++ )
	{
		g_lJoinQueue[ulIdx].ulPlayer = g_lJoinQueue[ulIdx + 1].ulPlayer;
		g_lJoinQueue[ulIdx].ulTeam = g_lJoinQueue[ulIdx + 1].ulTeam;
	}

	// Clear out the last slot.
	g_lJoinQueue[MAXPLAYERS - 1].ulPlayer = MAXPLAYERS;

	// If we're the server, tell everyone their new position in line.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SetQueuePosition( );
}

//*****************************************************************************
//
void JOINQUEUE_SpectatorLeftGame( ULONG ulPlayer )
{
	ULONG	ulIdx;
	ULONG	ulIdx2;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		// Hit the end of the list.
		if ( g_lJoinQueue[ulIdx].ulPlayer == MAXPLAYERS )
			continue;

		// This player was in line. Bump everyone in line after him up one position.
		if ( g_lJoinQueue[ulIdx].ulPlayer == ulPlayer )
			break;
	}

	if ( ulIdx == MAXPLAYERS )
		return;

	for ( ulIdx2 = ulIdx; ulIdx2 < ( MAXPLAYERS - 1 ); ulIdx2++ )
		g_lJoinQueue[ulIdx2].ulPlayer = g_lJoinQueue[ulIdx2+1].ulPlayer;

	// If we're the server, tell everyone their new position in line.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SetQueuePosition( );
}

//*****************************************************************************
//
void JOINQUEUE_PopQueue( LONG lNumSlots )
{
	ULONG	ulIdx;
	ULONG	ulIdx2;

	// Nothing to do if there's nobody waiting in the queue.
	if ( g_lJoinQueue[0].ulPlayer == MAXPLAYERS )
		return;

	// Don't allow joining during LMS or team LMS games.
	if (( lastmanstanding || teamlms ) &&
		(( LASTMANSTANDING_GetState( ) == LMSS_INPROGRESS ) || ( LASTMANSTANDING_GetState( ) == LMSS_WINSEQUENCE )))
		return;

	// Don't allow joining during survival games.
	if (( survival ) &&
		(( SURVIVAL_GetState( ) == SURVS_INPROGRESS ) || ( SURVIVAL_GetState( ) == SURVS_MISSIONFAILED )))
		return;

	// Try to find the next person in line.
	ulIdx = 0;
	while ( 1 )
	{
		// Found end of list.
		if (( ulIdx == MAXPLAYERS ) ||
			( g_lJoinQueue[ulIdx].ulPlayer == MAXPLAYERS ) ||
			( lNumSlots == 0 ))
		{
			break;
		}

		// No more room for an additional player.
		if (( NETWORK_GetState( ) == NETSTATE_SERVER ) &&
			( sv_maxplayers >= 0 ) &&
			( SERVER_CalcNumNonSpectatingPlayers( MAXPLAYERS ) >= (ULONG)sv_maxplayers ))
		{
			break;
		}
		if (( duel ) &&
			( DUEL_CountActiveDuelers( ) >= 2 ))
		{
			break;
		}

		// Found a player waiting in line. They will now join the game!
		if ( playeringame[g_lJoinQueue[ulIdx].ulPlayer] )
		{
			players[g_lJoinQueue[ulIdx].ulPlayer].playerstate = PST_ENTERNOINVENTORY;
			players[g_lJoinQueue[ulIdx].ulPlayer].bSpectating = false;
			players[g_lJoinQueue[ulIdx].ulPlayer].bDeadSpectator = false;

			if ( players[g_lJoinQueue[ulIdx].ulPlayer].pSkullBot )
				players[g_lJoinQueue[ulIdx].ulPlayer].pSkullBot->PostEvent( BOTEVENT_JOINEDGAME );

			if ( GAMEMODE_GetFlags( GAMEMODE_GetCurrentMode( )) & GMF_PLAYERSONTEAMS )
			{
				if (( g_lJoinQueue[ulIdx].ulTeam == TEAM_BLUE ) || ( g_lJoinQueue[ulIdx].ulTeam == TEAM_RED ))
					PLAYER_SetTeam( &players[g_lJoinQueue[ulIdx].ulPlayer], g_lJoinQueue[ulIdx].ulTeam, true );
				else
					PLAYER_SetTeam( &players[g_lJoinQueue[ulIdx].ulPlayer], TEAM_ChooseBestTeamForPlayer( ), true );
			}

			// Begin the duel countdown.
			if ( duel )
			{
				if ( sv_duelcountdowntime > 0 )
					DUEL_StartCountdown(( sv_duelcountdowntime * TICRATE ) - 1 );
				else
					DUEL_StartCountdown(( 10 * TICRATE ) - 1 );
			}
			// Begin the LMS countdown.
			else if ( lastmanstanding )
			{
				if ( sv_lmscountdowntime > 0 )
					LASTMANSTANDING_StartCountdown(( sv_lmscountdowntime * TICRATE ) - 1 );
				else
					LASTMANSTANDING_StartCountdown(( 10 * TICRATE ) - 1 );
			}
			else
			{
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVER_Printf( PRINT_HIGH, "%s \\c-joined the game.\n", players[g_lJoinQueue[ulIdx].ulPlayer].userinfo.netname );
				else
					Printf( "%s \\c-joined the game.\n", players[g_lJoinQueue[ulIdx].ulPlayer].userinfo.netname );
			}

			// Shift all the slot positions up one.
			for ( ulIdx2 = 0; ulIdx2 < ( MAXPLAYERS - 1 ); ulIdx2++ )
			{
				g_lJoinQueue[ulIdx2].ulPlayer = g_lJoinQueue[ulIdx2 + 1].ulPlayer;
				g_lJoinQueue[ulIdx2].ulTeam = g_lJoinQueue[ulIdx2 + 1].ulTeam;
			}

			// Clear out the last slot.
			g_lJoinQueue[MAXPLAYERS - 1].ulPlayer = -1;

			if ( lNumSlots > 0 )
				lNumSlots--;
		}
		else
			ulIdx++;
	}

	// If we're the server, tell everyone their new position in line.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SetQueuePosition( );
}

//*****************************************************************************
//
ULONG JOINQUEUE_AddPlayer( JOINSLOT_t JoinSlot )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		// Player is already in the queue!
		if ( g_lJoinQueue[ulIdx].ulPlayer == JoinSlot.ulPlayer )
			return ( ulIdx );

		// Found an empty slot.
		if ( g_lJoinQueue[ulIdx].ulPlayer == MAXPLAYERS )
		{
			g_lJoinQueue[ulIdx].ulPlayer	= JoinSlot.ulPlayer;
			g_lJoinQueue[ulIdx].ulTeam		= JoinSlot.ulTeam;
			return ( ulIdx );
		}
	}

	// Queue is full (should be impossible).
	return ( ulIdx );
}

//*****************************************************************************
//
void JOINQUEUE_ClearList( void )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
		g_lJoinQueue[ulIdx].ulPlayer = MAXPLAYERS;
}

//*****************************************************************************
//
LONG JOINQUEUE_GetPositionInLine( ULONG ulPlayer )
{
	ULONG	ulIdx;

	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return ( g_lClientQueuePosition );
	}

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( g_lJoinQueue[ulIdx].ulPlayer == ulPlayer )
			return ( ulIdx );
	}

	return ( -1 );
}

//*****************************************************************************
//
void JOINQUEUE_SetClientPositionInLine( LONG lPosition )
{
	g_lClientQueuePosition = lPosition;
}
