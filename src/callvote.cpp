//-----------------------------------------------------------------------------
//
// Skulltag Source
// Copyright (C) 2003-2005 Brad Carney
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
// Date created:  8/15/05
//
//
// Filename: callvote.cpp
//
// Description: 
//
//-----------------------------------------------------------------------------

#include "announcer.h"
#include "c_dispatch.h"
#include "cl_commands.h"
#include "cl_demo.h"
#include "cl_main.h"
#include "callvote.h"
#include "network.h"
#include "templates.h"
#include "sbar.h"
#include "sv_commands.h"
#include "v_video.h"
#include "maprotation.h"

//*****************************************************************************
//	VARIABLES

static	VOTESTATE_e				g_VoteState;
static	FString					g_VoteCommand;
static	ULONG					g_ulVoteCaller;
static	ULONG					g_ulVoteCountdownTicks = 0;
static	ULONG					g_ulVoteCompletedTicks = 0;
static	bool					g_bVotePassed;
static	ULONG					g_ulPlayersWhoVotedYes[(MAXPLAYERS / 2) + 1];
static	ULONG					g_ulPlayersWhoVotedNo[(MAXPLAYERS / 2) + 1];
static	ULONG					g_ulKickVoteTargetPlayerIdx;

//*****************************************************************************
//	PROTOTYPES

static	void			callvote_EndVote( void );
static	ULONG			callvote_CountPlayersWhoVotedYes( void );
static	ULONG			callvote_CountPlayersWhoVotedNo( void );
static	bool			callvote_CheckValidity( FString &Command, FString &Parameters );

//*****************************************************************************
//	FUNCTIONS

void CALLVOTE_Construct( void )
{
	// Calling this function initialized everything.
	CALLVOTE_ClearVote( );
}

//*****************************************************************************
//
void CALLVOTE_Tick( void )
{
	ULONG	ulNumYes;
	ULONG	ulNumNo;

	switch ( g_VoteState )
	{
	case VOTESTATE_NOVOTE:

		break;
	case VOTESTATE_INVOTE:

		if ( g_ulVoteCountdownTicks )
		{
			g_ulVoteCountdownTicks--;
			if (( NETWORK_GetState( ) != NETSTATE_CLIENT ) &&
				( CLIENTDEMO_IsPlaying( ) == false ) &&
				( g_ulVoteCountdownTicks == 0 ))
			{
				ulNumYes = callvote_CountPlayersWhoVotedYes( );
				ulNumNo = callvote_CountPlayersWhoVotedNo( );

				if (( ulNumYes > 0 ) && ( ulNumYes > ulNumNo ))
					g_bVotePassed = true;
				else
					g_bVotePassed = false;

				callvote_EndVote( );
			}
		}
		break;
	case VOTESTATE_VOTECOMPLETED:

		if ( g_ulVoteCompletedTicks )
		{
			if ( --g_ulVoteCompletedTicks == 0 )
			{
				// If the vote passed, execute the command string.
				if (( g_bVotePassed ) &&
					( NETWORK_GetState( ) != NETSTATE_CLIENT ) &&
					( CLIENTDEMO_IsPlaying( ) == false ))
				{
					// If the vote is a kick vote, we have to alter g_VoteCommand to kick the cached player idx.
					if ( strncmp( g_VoteCommand, "kick ", 5 ) == 0 )
					{
						g_VoteCommand = "kick_idx ";
						g_VoteCommand.AppendFormat( "%d", static_cast<unsigned int> (g_ulKickVoteTargetPlayerIdx) );
					}

					AddCommandString( (char *)g_VoteCommand.GetChars( ));
				}
				// Reset the module.
				CALLVOTE_ClearVote( );
			}
		}
		break;
	}
}

//*****************************************************************************
//
void CALLVOTE_BeginVote( FString Command, FString Parameters, ULONG ulPlayer )
{
	// Don't allow a vote in the middle of another vote.
	if ( g_VoteState != VOTESTATE_NOVOTE )
		return;

	// Check and make sure all the parameters are valid.
	if ( callvote_CheckValidity( Command, Parameters ) == false )
		return;

	// Play the announcer sound for this.
	ANNOUNCER_PlayEntry( cl_announcer, "VoteNow" );

	g_VoteCommand = Command;
	g_VoteCommand += " ";
	g_VoteCommand += Parameters;
	g_ulVoteCaller = ulPlayer;

	g_VoteState = VOTESTATE_INVOTE;
	g_ulVoteCountdownTicks = VOTE_COUNTDOWN_TIME * TICRATE;

	// Inform clients about the vote being called.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_CallVote( ulPlayer, Command, Parameters );
}

//*****************************************************************************
//
void CALLVOTE_ClearVote( void )
{
	ULONG	ulIdx;

	g_VoteState = VOTESTATE_NOVOTE;
	g_VoteCommand = "";
	g_ulVoteCaller = MAXPLAYERS;
	g_ulVoteCountdownTicks = 0;

	for ( ulIdx = 0; ulIdx < (( MAXPLAYERS / 2 ) + 1 ); ulIdx++ )
	{
		g_ulPlayersWhoVotedYes[ulIdx] = MAXPLAYERS;
		g_ulPlayersWhoVotedNo[ulIdx] = MAXPLAYERS;
	}
}

//*****************************************************************************
//
bool CALLVOTE_VoteYes( ULONG ulPlayer )
{
	ULONG	ulIdx;
	ULONG	ulNumYes;
	ULONG	ulNumNo;

	// Don't allow the vote unless we're in the middle of a vote.
	if ( g_VoteState != VOTESTATE_INVOTE )
		return ( false );

	// If this player has already voted, ignore his vote.
	for ( ulIdx = 0; ulIdx < ( MAXPLAYERS / 2 ) + 1; ulIdx++ )
	{
		if ( g_ulPlayersWhoVotedYes[ulIdx] == ulPlayer )
			return ( false );

		if ( g_ulPlayersWhoVotedNo[ulIdx] == ulPlayer )
			return ( false );

		// If this person matches the IP of a person who already voted, don't let him vote.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		{
			if ( g_ulPlayersWhoVotedYes[ulIdx] < MAXPLAYERS )
			{
				if ( NETWORK_CompareAddress( SERVER_GetClient( g_ulPlayersWhoVotedYes[ulIdx] )->Address, SERVER_GetClient( ulPlayer )->Address, true ))
					return ( false );
			}

			if ( g_ulPlayersWhoVotedNo[ulIdx] < MAXPLAYERS )
			{
				if ( NETWORK_CompareAddress( SERVER_GetClient( g_ulPlayersWhoVotedNo[ulIdx] )->Address, SERVER_GetClient( ulPlayer )->Address, true ))
					return ( false );
			}
		}
	}

	// Add this player's vote.
	for ( ulIdx = 0; ulIdx < ( MAXPLAYERS / 2 ) + 1; ulIdx++ )
	{
		if ( g_ulPlayersWhoVotedYes[ulIdx] == MAXPLAYERS )
		{
			g_ulPlayersWhoVotedYes[ulIdx] = ulPlayer;
			break;
		}
	}

	// Nothing more to do here for clients.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return ( true );
	}

	ulNumYes = callvote_CountPlayersWhoVotedYes( );
	ulNumNo = callvote_CountPlayersWhoVotedNo( );

	// If more than half of the total eligible voters have voted, we must have a majority!
	if ( MAX( ulNumYes, ulNumNo ) > ( CALLVOTE_CountNumEligibleVoters( ) / 2 ))
	{
		g_bVotePassed = ( ulNumYes > ulNumNo );

		callvote_EndVote( );
	}

	return ( true );
}

//*****************************************************************************
//
bool CALLVOTE_VoteNo( ULONG ulPlayer )
{
	ULONG	ulIdx;
	ULONG	ulNumYes;
	ULONG	ulNumNo;

	// Don't allow the vote unless we're in the middle of a vote.
	if ( g_VoteState != VOTESTATE_INVOTE )
		return ( false );

	// If this player has already voted, ignore his vote.
	for ( ulIdx = 0; ulIdx < ( MAXPLAYERS / 2 ) + 1; ulIdx++ )
	{
		if ( g_ulPlayersWhoVotedYes[ulIdx] == ulPlayer )
			return ( false );

		if ( g_ulPlayersWhoVotedNo[ulIdx] == ulPlayer )
			return ( false );

		// If this person matches the IP of a person who already voted, don't let him vote.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		{
			if ( g_ulPlayersWhoVotedYes[ulIdx] < MAXPLAYERS )
			{
				if ( NETWORK_CompareAddress( SERVER_GetClient( g_ulPlayersWhoVotedYes[ulIdx] )->Address, SERVER_GetClient( ulPlayer )->Address, true ))
					return ( false );
			}

			if ( g_ulPlayersWhoVotedNo[ulIdx] < MAXPLAYERS )
			{
				if ( NETWORK_CompareAddress( SERVER_GetClient( g_ulPlayersWhoVotedNo[ulIdx] )->Address, SERVER_GetClient( ulPlayer )->Address, true ))
					return ( false );
			}
		}
	}

	// Add this player's vote.
	for ( ulIdx = 0; ulIdx < ( MAXPLAYERS / 2 ) + 1; ulIdx++ )
	{
		if ( g_ulPlayersWhoVotedNo[ulIdx] == MAXPLAYERS )
		{
			g_ulPlayersWhoVotedNo[ulIdx] = ulPlayer;
			break;
		}
	}

	// Nothing more to do here for clients.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return ( true );
	}

	ulNumYes = callvote_CountPlayersWhoVotedYes( );
	ulNumNo = callvote_CountPlayersWhoVotedNo( );

	// If more than half of the total eligible voters have voted, we must have a majority!
	if ( MAX( ulNumYes, ulNumNo ) > ( CALLVOTE_CountNumEligibleVoters( ) / 2 ))
	{
		g_bVotePassed = ( ulNumYes > ulNumNo );

		callvote_EndVote( );
	}

	return ( true );
}

//*****************************************************************************
//
ULONG CALLVOTE_CountNumEligibleVoters( void )
{
	ULONG	ulIdx;
	ULONG	ulIdx2;
	ULONG	ulNumVoters;

	ulNumVoters = 0;
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		// A voter is anyone in the game who isn't a bot.
		if (( playeringame[ulIdx] ) &&
			( players[ulIdx].bIsBot == false ))
		{
			// Go through and see if anyone has an IP that matches this person's.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			{
				for ( ulIdx2 = ulIdx; ulIdx2 < MAXPLAYERS; ulIdx2++ )
				{
					if (( playeringame[ulIdx2] == false ) ||
						( ulIdx == ulIdx2 ) ||
						( SERVER_IsValidClient( ulIdx2 ) == false ))
					{
						continue;
					}

					// If the two IP addresses match, break out.
//					if ( NETWORK_CompareAddress( SERVER_GetClient( ulIdx )->Address, SERVER_GetClient( ulIdx2 )->Address, true ))
//						break;
				}

				if ( ulIdx2 == MAXPLAYERS )
					ulNumVoters++;
			}
			else
				ulNumVoters++;
		}
	}

	return ( ulNumVoters );
}

//*****************************************************************************
//
void CALLVOTE_EndVote( bool bPassed )
{
	// This is a client-only function.
	if (( NETWORK_GetState( ) != NETSTATE_CLIENT ) &&
		( CLIENTDEMO_IsPlaying( ) == false ))
	{
		return;
	}

	g_bVotePassed = bPassed;
	callvote_EndVote( );
}

//*****************************************************************************
//
const char *CALLVOTE_GetCommand( void )
{
	return ( g_VoteCommand.GetChars( ));
}

//*****************************************************************************
//
ULONG CALLVOTE_GetVoteCaller( void )
{
	return ( g_ulVoteCaller );
}

//*****************************************************************************
//
VOTESTATE_e CALLVOTE_GetVoteState( void )
{
	return ( g_VoteState );
}

//*****************************************************************************
//
ULONG CALLVOTE_GetCountdownTicks( void )
{
	return ( g_ulVoteCountdownTicks );
}

//*****************************************************************************
//
ULONG *CALLVOTE_GetPlayersWhoVotedYes( void )
{
	return ( g_ulPlayersWhoVotedYes );
}

//*****************************************************************************
//
ULONG *CALLVOTE_GetPlayersWhoVotedNo( void )
{
	return ( g_ulPlayersWhoVotedNo );
}

//*****************************************************************************
//*****************************************************************************
//
static void callvote_EndVote( void )
{
	char				szString[32];
	DHUDMessageFadeOut	*pMsg;

	if ( g_VoteState != VOTESTATE_INVOTE )
		return;

	g_VoteState = VOTESTATE_VOTECOMPLETED;
	g_ulVoteCompletedTicks = VOTE_PASSED_TIME * TICRATE;

	// If we're the server, inform the clients that the vote has ended.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_VoteEnded( g_bVotePassed );
	else
	{
		screen->SetFont( BigFont );

		if ( g_bVotePassed )
			sprintf( szString, "VOTE PASSED!" );
		else
			sprintf( szString, "VOTE FAILED!" );

		// Display "%s WINS!" HUD message.
		pMsg = new DHUDMessageFadeOut( szString,
			160.4f,
			75.0f,
			320,
			200,
			CR_RED,
			3.0f,
			2.0f );

		StatusBar->AttachMessage( pMsg, MAKE_ID('C','N','T','R') );
		screen->SetFont( SmallFont );
	}

	// Play the announcer sound associated with this event.
	if ( g_bVotePassed )
		ANNOUNCER_PlayEntry( cl_announcer, "VotePassed" );
	else
		ANNOUNCER_PlayEntry( cl_announcer, "VoteFailed" );
}

//*****************************************************************************
//
static ULONG callvote_CountPlayersWhoVotedYes( void )
{
	ULONG	ulIdx;
	ULONG	ulNumYes;

	ulNumYes = 0;
	for ( ulIdx = 0; ulIdx < ( MAXPLAYERS / 2 ) + 1; ulIdx++ )
	{
		if ( g_ulPlayersWhoVotedYes[ulIdx] != MAXPLAYERS )
			ulNumYes++;
	}

	return ( ulNumYes );
}

//*****************************************************************************
//
static ULONG callvote_CountPlayersWhoVotedNo( void )
{
	ULONG	ulIdx;
	ULONG	ulNumNo;

	ulNumNo = 0;
	for ( ulIdx = 0; ulIdx < ( MAXPLAYERS / 2 ) + 1; ulIdx++ )
	{
		if ( g_ulPlayersWhoVotedNo[ulIdx] != MAXPLAYERS )
			ulNumNo++;
	}

	return ( ulNumNo );
}

//*****************************************************************************
//
static bool callvote_CheckValidity( FString &Command, FString &Parameters )
{
	ULONG	ulIdx;
	ULONG	ulVoteCmd;
	char	szPlayerName[64];

	// First, figure out what kind of command we're trying to vote on.
	if ( Command.CompareNoCase( "kick" ) == 0 )
		ulVoteCmd = VOTECMD_KICK;
	else {
		int i = 0;
		while( Parameters.GetChars()[i] != '\0' )
		{
		  if( Parameters.GetChars()[i] == ';' || Parameters.GetChars()[i] == ' ' )
			  	return ( false );
		  i++;
		}
		if ( Command.CompareNoCase( "map" ) == 0 )
			ulVoteCmd = VOTECMD_MAP;
		else if ( Command.CompareNoCase( "changemap" ) == 0 )
			ulVoteCmd = VOTECMD_CHANGEMAP;
		else if ( Command.CompareNoCase( "fraglimit" ) == 0 )
			ulVoteCmd = VOTECMD_FRAGLIMIT;
		else if ( Command.CompareNoCase( "timelimit" ) == 0 )
			ulVoteCmd = VOTECMD_TIMELIMIT;
		else if ( Command.CompareNoCase( "winlimit" ) == 0 )
			ulVoteCmd = VOTECMD_WINLIMIT;
		else if ( Command.CompareNoCase( "duellimit" ) == 0 )
			ulVoteCmd = VOTECMD_DUELLIMIT;
		else if ( Command.CompareNoCase( "pointlimit" ) == 0 )
			ulVoteCmd = VOTECMD_POINTLIMIT;
		else
			return ( false );
	}

	// Then, make sure the parameter for each vote is valid.
	int parameterInt = atoi( Parameters.GetChars() );
	switch ( ulVoteCmd )
	{
	case VOTECMD_KICK:

		for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
		{
			if (( playeringame[ulIdx] == false ) ||
				( players[ulIdx].bIsBot ))
			{
				continue;
			}

			// Compare the parameter to the version of the player's name without color codes.
			sprintf( szPlayerName, players[ulIdx].userinfo.netname );
			V_RemoveColorCodes( szPlayerName );
			if ( Parameters.CompareNoCase( szPlayerName ) == 0 ){
				// to prevent a player from escaping a kick vote by renaming, we store his ID at the beginning of the vote
				g_ulKickVoteTargetPlayerIdx = ulIdx;
				break;
			}
		}

		// If we didn't find the player, then don't allow the vote.
		if ( ulIdx == MAXPLAYERS )
			return ( false );
		break;
	case VOTECMD_MAP:
	case VOTECMD_CHANGEMAP:

		// Don't allow the command if the map doesn't exist.
		if ( !P_CheckIfMapExists( Parameters.GetChars() ) )
			return ( false );
		// Don't allow to leave the maprotation (Only the server knows the maprotation)
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		{
			if ( sv_maprotation && ( MAPROTATION_IsMapInRotation( Parameters.GetChars() ) == false ) )
			{
				SERVER_PrintfPlayer( PRINT_HIGH, SERVER_GetCurrentClient(), "This map is not in the map rotation.\n" );
				Printf ( "map %s is not in the map rotation\n", Parameters.GetChars() );
				return ( false );
			}
		}
		break;
	case VOTECMD_FRAGLIMIT:
	case VOTECMD_WINLIMIT:
	case VOTECMD_DUELLIMIT:

		// limit must be from 0-255.
		if (( parameterInt < 0 ) || ( parameterInt >= 256 ))
			return ( false );
		else if ( parameterInt == 0 )
		{
			if (( Parameters.GetChars()[0] != '0' ) || ( Parameters.Len() != 1 ))
				return ( false );
		}
		Parameters.Format( "%d", parameterInt );
		break;
	case VOTECMD_TIMELIMIT:
	case VOTECMD_POINTLIMIT:

		// limit must be from 0-65535.
		if (( parameterInt < 0 ) || ( parameterInt >= 65536 ))
			return ( false );
		else if ( parameterInt == 0 )
		{
			if (( Parameters.GetChars()[0] != '0' ) || ( Parameters.Len() != 1 ))
				return ( false );
		}
		Parameters.Format( "%d", parameterInt );
		break;
	default:

		return ( false );
	}

	// Passed all checks!
	return ( true );
}

//*****************************************************************************
//	CONSOLE COMMANDS/VARIABLES

CVAR( Int, sv_nocallvote, 0, CVAR_ARCHIVE ); // 0 - everyone can call votes. 1 - nobody can. 2 - only players can.
CVAR( Bool, sv_nokickvote, false, CVAR_ARCHIVE );
CVAR( Bool, sv_nomapvote, false, CVAR_ARCHIVE );
CVAR( Bool, sv_nochangemapvote, false, CVAR_ARCHIVE );
CVAR( Bool, sv_nofraglimitvote, false, CVAR_ARCHIVE );
CVAR( Bool, sv_notimelimitvote, false, CVAR_ARCHIVE );
CVAR( Bool, sv_nowinlimitvote, false, CVAR_ARCHIVE );
CVAR( Bool, sv_noduellimitvote, false, CVAR_ARCHIVE );
CVAR( Bool, sv_nopointlimitvote, false, CVAR_ARCHIVE );
CVAR( Bool, cl_showfullscreenvote, false, CVAR_ARCHIVE );
CCMD( callvote )
{
	ULONG	ulVoteCmd;

	// Don't allow a vote unless the player is a client.
	if ( NETWORK_GetState( ) != NETSTATE_CLIENT )
	{
		Printf( "You cannot call a vote if you're not a client!\n" );
		return;
	}

	if ( CLIENT_GetConnectionState( ) != CTS_ACTIVE )
		return;

	if ( argv.argc( ) < 2 )
	{
		Printf( "callvote <command> [parameters]: Calls a vote\n" );
		return;
	}

	// [BB] No voting when not in a level, e.g. during intermission.
	if ( gamestate != GS_LEVEL )
	{
		Printf( "You cannot call a vote when not in a level!\n" );
		return;
	}

	// Don't allow one person to call a vote, and vote by himself.
	if ( CALLVOTE_CountNumEligibleVoters( ) < 2 )
	{
		Printf( "There must be at least two eligible voters to call a vote!\n" );
		return;
	}

	if ( stricmp( "kick", argv[1] ) == 0 )
		ulVoteCmd = VOTECMD_KICK;
	else if ( stricmp( "map", argv[1] ) == 0 )
		ulVoteCmd = VOTECMD_MAP;
	else if ( stricmp( "changemap", argv[1] ) == 0 )
		ulVoteCmd = VOTECMD_CHANGEMAP;
	else if ( stricmp( "fraglimit", argv[1] ) == 0 )
		ulVoteCmd = VOTECMD_FRAGLIMIT;
	else if ( stricmp( "timelimit", argv[1] ) == 0 )
		ulVoteCmd = VOTECMD_TIMELIMIT;
	else if ( stricmp( "winlimit", argv[1] ) == 0 )
		ulVoteCmd = VOTECMD_WINLIMIT;
	else if ( stricmp( "duellimit", argv[1] ) == 0 )
		ulVoteCmd = VOTECMD_DUELLIMIT;
	else if ( stricmp( "pointlimit", argv[1] ) == 0 )
		ulVoteCmd = VOTECMD_POINTLIMIT;
	else
	{
		Printf( "Invalid callvote command.\n" );
		return;
	}

	if ( argv.argc( ) >= 3 )
		CLIENTCOMMANDS_CallVote( ulVoteCmd, argv[2] );
	else
		CLIENTCOMMANDS_CallVote( ulVoteCmd, 0 );
/*
	g_lBytesSent += g_LocalBuffer.cursize;
	if ( g_lBytesSent > g_lMaxBytesSent )
		g_lMaxBytesSent = g_lBytesSent;
*/
	NETWORK_LaunchPacket( CLIENT_GetLocalBuffer( ), CLIENT_GetServerAddress( ));
	NETWORK_ClearBuffer( CLIENT_GetLocalBuffer( ));
}

//*****************************************************************************
//
CCMD( vote_yes )
{
	if ( NETWORK_GetState( ) != NETSTATE_CLIENT )
		return;

	if ( CLIENT_GetConnectionState( ) != CTS_ACTIVE )
		return;

	if ( g_VoteState != VOTESTATE_INVOTE )
		return;

	CLIENTCOMMANDS_VoteYes( );
/*
	g_lBytesSent += g_LocalBuffer.cursize;
	if ( g_lBytesSent > g_lMaxBytesSent )
		g_lMaxBytesSent = g_lBytesSent;
*/
	NETWORK_LaunchPacket( CLIENT_GetLocalBuffer( ), CLIENT_GetServerAddress( ));
	NETWORK_ClearBuffer( CLIENT_GetLocalBuffer( ));
}

//*****************************************************************************
//
CCMD( vote_no )
{
	if ( NETWORK_GetState( ) != NETSTATE_CLIENT )
		return;

	if ( CLIENT_GetConnectionState( ) != CTS_ACTIVE )
		return;

	if ( g_VoteState != VOTESTATE_INVOTE )
		return;

	CLIENTCOMMANDS_VoteNo( );
/*
	g_lBytesSent += g_LocalBuffer.cursize;
	if ( g_lBytesSent > g_lMaxBytesSent )
		g_lMaxBytesSent = g_lBytesSent;
*/
	NETWORK_LaunchPacket( CLIENT_GetLocalBuffer( ), CLIENT_GetServerAddress( ));
	NETWORK_ClearBuffer( CLIENT_GetLocalBuffer( ));
}
