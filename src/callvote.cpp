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
// Description: Handles client-called votes.
//
// Possible improvements:
//	- Remove all the Yes( ) and No( ) code duplication. (suggested by Rivecoder)
//
//-----------------------------------------------------------------------------

#include "announcer.h"
#include "c_dispatch.h"
#include "cl_commands.h"
#include "cl_demo.h"
#include "cl_main.h"
#include "callvote.h"
#include "doomstat.h"
#include "network.h"
#include "templates.h"
#include "sbar.h"
#include "sv_commands.h"
#include "sv_main.h"
#include "v_video.h"
#include "maprotation.h"
#include <list>

//*****************************************************************************
//	VARIABLES

static	VOTESTATE_e				g_VoteState;
static	FString					g_VoteCommand;
static	FString					g_VoteReason;
static	ULONG					g_ulVoteCaller;
static	ULONG					g_ulVoteCountdownTicks = 0;
static	ULONG					g_ulVoteCompletedTicks = 0;
static	ULONG					g_ulShowVoteScreenTicks = 0;
static	bool					g_bVotePassed;
static	bool					g_bVoteCancelled;
static	ULONG					g_ulPlayersWhoVotedYes[(MAXPLAYERS / 2) + 1];
static	ULONG					g_ulPlayersWhoVotedNo[(MAXPLAYERS / 2) + 1];
static	NETADDRESS_s			g_KickVoteVictimAddress;
static	std::list<VOTE_s>		g_PreviousVotes;

//*****************************************************************************
//	PROTOTYPES

static	void			callvote_EndVote( void );
static	ULONG			callvote_CountPlayersWhoVotedYes( void );
static	ULONG			callvote_CountPlayersWhoVotedNo( void );
static	bool			callvote_CheckForFlooding( FString &Command, FString &Parameters, ULONG ulPlayer );
static	bool			callvote_CheckValidity( FString &Command, FString &Parameters );
static	ULONG			callvote_GetVoteType( const char *pszCommand );

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

		// [RC] Hide the voteing screen shortly after voting.
		if ( g_ulShowVoteScreenTicks )
			g_ulShowVoteScreenTicks--;

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
				g_PreviousVotes.back( ).bPassed = g_bVotePassed;

				// If the vote passed, execute the command string.
				if (( g_bVotePassed ) && ( !g_bVoteCancelled ) &&
					( NETWORK_GetState( ) != NETSTATE_CLIENT ) &&
					( CLIENTDEMO_IsPlaying( ) == false ))
				{
					// [BB, RC] If the vote is a kick vote, we have to rewrite g_VoteCommand to both use the stored IP, and temporarily ban it.
					// [Dusk] Write the kick reason into the ban reason, [BB] but only if it's not empty.
					if ( strncmp( g_VoteCommand, "kick", 4 ) == 0 )
					{
						g_VoteCommand.Format( "addban %s 10min \"Vote kick, %d to %d", NETWORK_AddressToString( g_KickVoteVictimAddress ), static_cast<int>(callvote_CountPlayersWhoVotedYes( )), static_cast<int>(callvote_CountPlayersWhoVotedNo( )) );
						if ( g_VoteReason.IsNotEmpty() )
							g_VoteCommand.AppendFormat ( " (%s)", g_VoteReason.GetChars( ) );
						g_VoteCommand += ".\"";
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
void CALLVOTE_BeginVote( FString Command, FString Parameters, FString Reason, ULONG ulPlayer )
{
	// Don't allow a vote in the middle of another vote.
	if ( g_VoteState != VOTESTATE_NOVOTE )
	{
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVER_PrintfPlayer( PRINT_HIGH, SERVER_GetCurrentClient( ), "Another vote is already underway.\n" );
		return;
	}

	// Check and make sure all the parameters are valid.
	if ( callvote_CheckValidity( Command, Parameters ) == false )
		return;

	// Prevent excessive re-voting.
	if (( NETWORK_GetState( ) == NETSTATE_SERVER ) && callvote_CheckForFlooding( Command, Parameters, ulPlayer ) == false )
		return;

	// Play the announcer sound for this.
	ANNOUNCER_PlayEntry( cl_announcer, "VoteNow" );

	// Create the vote console command.
	g_VoteCommand = Command;
	g_VoteCommand += " ";
	g_VoteCommand += Parameters;
	g_ulVoteCaller = ulPlayer;
	g_VoteReason = Reason.Left(25);

	// Create the record of the vote for flood prevention.
	{
		VOTE_s VoteRecord;
		VoteRecord.fsParameter = Parameters;
		time_t tNow;
		time( &tNow );
		VoteRecord.tTimeCalled = tNow;
		VoteRecord.Address = SERVER_GetClient( g_ulVoteCaller )->Address;
		VoteRecord.ulVoteType = callvote_GetVoteType( Command );

		if ( VoteRecord.ulVoteType == VOTECMD_KICK )
			VoteRecord.KickAddress = g_KickVoteVictimAddress; 

		g_PreviousVotes.push_back( VoteRecord );
	}

	// Display the message in the console.
	{
		FString	ReasonBlurb = ( g_VoteReason.Len( )) ? ( ", reason: \"" + g_VoteReason + "\"" ) : "";
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			Printf( "%s\\c- (%s) has called a vote (\"%s\"%s).\n", players[ulPlayer].userinfo.netname, NETWORK_AddressToString( SERVER_GetClient( ulPlayer )->Address ), g_VoteCommand.GetChars(), ReasonBlurb.GetChars() );
		else
			Printf( "%s\\c- has called a vote (\"%s\"%s).\n", players[ulPlayer].userinfo.netname, g_VoteCommand.GetChars(), ReasonBlurb.GetChars() );
	}

	g_VoteState = VOTESTATE_INVOTE;
	g_ulVoteCountdownTicks = VOTE_COUNTDOWN_TIME * TICRATE;
	g_ulShowVoteScreenTicks = g_ulVoteCountdownTicks;
	g_bVoteCancelled = false;

	// Inform clients about the vote being called.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_CallVote( ulPlayer, Command, Parameters, Reason );
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
	g_ulShowVoteScreenTicks = 0;

	for ( ulIdx = 0; ulIdx < (( MAXPLAYERS / 2 ) + 1 ); ulIdx++ )
	{
		g_ulPlayersWhoVotedYes[ulIdx] = MAXPLAYERS;
		g_ulPlayersWhoVotedNo[ulIdx] = MAXPLAYERS;
	}

	g_bVoteCancelled = false;
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

	// [RC] If this is our vote, hide the vote screen soon.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) && ( static_cast<LONG>(ulPlayer) == consoleplayer ) )
		g_ulShowVoteScreenTicks = 1 * TICRATE;

	// Also, don't allow spectator votes if the server has them disabled.
	if (( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( sv_nocallvote == 2 && players[ulPlayer].bSpectating ))
	{
		SERVER_PrintfPlayer( PRINT_HIGH, ulPlayer, "This server requires spectators to join the game to vote.\n" );
		return false;
	}

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

	// Display the message in the console.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		Printf( "%s\\c- (%s) votes \"yes\".\n", players[ulPlayer].userinfo.netname, NETWORK_AddressToString( SERVER_GetClient( ulPlayer )->Address ));
	else
		Printf( "%s\\c- votes \"yes\".\n", players[ulPlayer].userinfo.netname );

	// Nothing more to do here for clients.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return ( true );
	}

	SERVERCOMMANDS_PlayerVote( ulPlayer, true );

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

	// [RC] If this is our vote, hide the vote screen soon.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) && ( static_cast<LONG>(ulPlayer) == consoleplayer ) )
		g_ulShowVoteScreenTicks = 1 * TICRATE;

	// [RC] Vote callers can cancel their votes by voting "no".
	if ( ulPlayer == g_ulVoteCaller && ( NETWORK_GetState( ) == NETSTATE_SERVER ))
	{
		// [BB] If a player canceled his own vote, don't prevent others from making this type of vote again.
		g_PreviousVotes.back( ).ulVoteType = NUM_VOTECMDS;

		SERVER_Printf( PRINT_HIGH, "Vote caller cancelled the vote.\n" );
		g_bVoteCancelled = true;
		g_bVotePassed = false;
		callvote_EndVote( );
		return ( true );
	}

	// Also, don't allow spectator votes if the server has them disabled.
	if (( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( sv_nocallvote == 2 && players[ulPlayer].bSpectating ))
	{
		SERVER_PrintfPlayer( PRINT_HIGH, ulPlayer, "This server requires spectators to join the game to vote.\n" );
		return false;
	}

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

	// Display the message in the console.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		Printf( "%s\\c- (%s) votes \"no\".\n", players[ulPlayer].userinfo.netname, NETWORK_AddressToString( SERVER_GetClient( ulPlayer )->Address ));
	else
		Printf( "%s\\c- votes \"no\".\n", players[ulPlayer].userinfo.netname );

	// Nothing more to do here for clients.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return ( true );
	}
	
	SERVERCOMMANDS_PlayerVote( ulPlayer, false );

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
const char *CALLVOTE_GetReason( void )
{
	return ( g_VoteReason.GetChars( ));
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
//
bool CALLVOTE_ShouldShowVoteScreen( void )
{
	return (( CALLVOTE_GetVoteState( ) == VOTESTATE_INVOTE ) && g_ulShowVoteScreenTicks );
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
		if ( g_bVotePassed )
			sprintf( szString, "VOTE PASSED!" );
		else
			sprintf( szString, "VOTE FAILED!" );

		// Display "%s WINS!" HUD message.
		pMsg = new DHUDMessageFadeOut( BigFont, szString,
			160.4f,
			14.0f,
			320,
			200,
			CR_RED,
			3.0f,
			2.0f );

		StatusBar->AttachMessage( pMsg, MAKE_ID('C','N','T','R') );
	}

	// Log to the console.
	Printf( "Vote %s!\n", g_bVotePassed ? "passed" : "failed" );

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
static bool callvote_CheckForFlooding( FString &Command, FString &Parameters, ULONG ulPlayer )
{
	NETADDRESS_s	Address = SERVER_GetClient( ulPlayer )->Address;
	ULONG			ulVoteType = callvote_GetVoteType( Command );
	time_t tNow;
	time( &tNow );

	// Remove old votes that no longer affect flooding.
	while ( g_PreviousVotes.size( ) > 0 && (( tNow - g_PreviousVotes.front( ).tTimeCalled ) > VOTE_LONGEST_INTERVAL * MINUTE ))
		g_PreviousVotes.pop_front( );

	// [BB] If the server doesn't want to limit the number of votes, there is no check anything.
	if ( sv_limitnumvotes == false )
		return true;

	// Run through the vote cache (backwards, from recent to old) and search for grounds on which to reject the vote.
	for( std::list<VOTE_s>::reverse_iterator i = g_PreviousVotes.rbegin(); i != g_PreviousVotes.rend(); ++i )
	{
		// One *type* of vote per voter per ## minutes (excluding kick votes if they passed).
		if ( !( i->ulVoteType == VOTECMD_KICK && i->bPassed ) && NETWORK_CompareAddress( i->Address, Address, true ) && ( ulVoteType == i->ulVoteType ) && (( tNow - i->tTimeCalled ) < VOTER_VOTETYPE_INTERVAL * MINUTE ))
		{
			int iMinutesLeft = static_cast<int>( 1 + ( i->tTimeCalled + VOTER_VOTETYPE_INTERVAL * MINUTE - tNow ) / MINUTE );
			SERVER_PrintfPlayer( PRINT_HIGH, ulPlayer, "You must wait %d minute%s to call another %s vote.\n", iMinutesLeft, ( iMinutesLeft == 1 ? "" : "s" ), Command.GetChars() );
			return false;
		}

		// One vote per voter per ## minutes.
		if ( NETWORK_CompareAddress( i->Address, Address, true ) && (( tNow - i->tTimeCalled ) < VOTER_NEWVOTE_INTERVAL * MINUTE ))
		{
			int iMinutesLeft = static_cast<int>( 1 + ( i->tTimeCalled + VOTER_NEWVOTE_INTERVAL * MINUTE - tNow ) / MINUTE );
			SERVER_PrintfPlayer( PRINT_HIGH, ulPlayer, "You must wait %d minute%s to call another vote.\n", iMinutesLeft, ( iMinutesLeft == 1 ? "" : "s" ));
			return false;
		}

		// Specific votes ("map map30") that fail can't be re-proposed for ## minutes.
		if (( ulVoteType == i->ulVoteType ) && ( !i->bPassed ) && (( tNow - i->tTimeCalled ) < VOTE_LITERALREVOTE_INTERVAL * MINUTE ))
		{
			int iMinutesLeft = static_cast<int>( 1 + ( i->tTimeCalled + VOTE_LITERALREVOTE_INTERVAL * MINUTE - tNow ) / MINUTE );

			// Kickvotes (can't give the IP to clients!).
			if (( i->ulVoteType == VOTECMD_KICK ) && ( !i->bPassed ) && NETWORK_CompareAddress( i->KickAddress, g_KickVoteVictimAddress, true ))
			{
				SERVER_PrintfPlayer( PRINT_HIGH, ulPlayer, "That specific player was recently on voted to be kicked, but the vote failed. You must wait %d minute%s to call it again.\n", iMinutesLeft, ( iMinutesLeft == 1 ? "" : "s" ));
				return false;
			}

			// Other votes.
			if (( i->ulVoteType != VOTECMD_KICK ) && ( stricmp( i->fsParameter.GetChars(), Parameters.GetChars() ) == 0 ))
			{
				SERVER_PrintfPlayer( PRINT_HIGH, ulPlayer, "That specific vote (\"%s %s\") was recently called, and failed. You must wait %d minute%s to call it again.\n", Command.GetChars(), Parameters.GetChars(), iMinutesLeft, ( iMinutesLeft == 1 ? "" : "s" ));
				return false;
			}
		}
	}

	return true;
}

//*****************************************************************************
//
static bool callvote_CheckValidity( FString &Command, FString &Parameters )
{
	// Get the type of vote this is.
	ULONG	ulVoteCmd = callvote_GetVoteType( Command.GetChars( ));
	if ( ulVoteCmd == NUM_VOTECMDS )
		return ( false );

	// Check for any illegal characters.
	if ( ulVoteCmd != VOTECMD_KICK )
	{
		int i = 0;
		while ( Parameters.GetChars()[i] != '\0' )
		{
			if ( Parameters.GetChars()[i] == ';' || Parameters.GetChars()[i] == ' ' )
			{
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVER_PrintfPlayer( PRINT_HIGH, SERVER_GetCurrentClient( ), "That vote command contained illegal characters.\n" );
  				return ( false );
			}
			i++;
		}
	}

	// Then, make sure the parameter for each vote is valid.
	int parameterInt = atoi( Parameters.GetChars() );
	switch ( ulVoteCmd )
	{
	case VOTECMD_KICK:
		{
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			{
				// Store the player's IP so he can't get away.
				ULONG ulIdx = SERVER_GetPlayerIndexFromName( Parameters.GetChars( ), true, false );
				if ( ulIdx < MAXPLAYERS )
				{
					if ( static_cast<LONG>(ulIdx) == SERVER_GetCurrentClient( ))
					{
						SERVER_PrintfPlayer( PRINT_HIGH, SERVER_GetCurrentClient( ), "You cannot votekick yourself!\n" );
  						return ( false );
					}
					// [BB] Don't allow anyone to kick somebody who is on the admin list. [K6] ...or is logged into RCON.
					if ( SERVER_GetAdminList()->isIPInList( SERVER_GetClient( ulIdx )->Address )
						|| SERVER_GetClient( ulIdx )->bRCONAccess )
					{
						SERVER_PrintfPlayer( PRINT_HIGH, SERVER_GetCurrentClient( ), "This player is a server admin and thus can't be kicked!\n" );
  						return ( false );
					}
					g_KickVoteVictimAddress = SERVER_GetClient( ulIdx )->Address;
					return ( true );
				}
				else
				{
					SERVER_PrintfPlayer( PRINT_HIGH, SERVER_GetCurrentClient( ), "That player doesn't exist.\n" );
					return ( false );
				}
			}
		}
		break;
	case VOTECMD_MAP:
	case VOTECMD_CHANGEMAP:

		// Don't allow the command if the map doesn't exist.
		if ( !P_CheckIfMapExists( Parameters.GetChars( )))
		{
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVER_PrintfPlayer( PRINT_HIGH, SERVER_GetCurrentClient( ), "That map does not exist.\n" );
			return ( false );
		}
		
		// Don't allow us to leave the map rotation.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		{
			if ( sv_maprotation && ( MAPROTATION_IsMapInRotation( Parameters.GetChars( ) ) == false ) )
			{
				SERVER_PrintfPlayer( PRINT_HIGH, SERVER_GetCurrentClient( ), "That map is not in the map rotation.\n" );
				return ( false );
			}
		}
		break;
	case VOTECMD_FRAGLIMIT:
	case VOTECMD_WINLIMIT:
	case VOTECMD_DUELLIMIT:

		// Parameteter be between 0 and 255.
		if (( parameterInt < 0 ) || ( parameterInt >= 256 ))
		{
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVER_PrintfPlayer( PRINT_HIGH, SERVER_GetCurrentClient( ), "%s parameters must be between 0 and 255.\n", Command.GetChars() );
			return ( false );
		}
		else if ( parameterInt == 0 )
		{
			if (( Parameters.GetChars()[0] != '0' ) || ( Parameters.Len() != 1 ))
				return ( false );
		}
		Parameters.Format( "%d", parameterInt );
		break;
	case VOTECMD_TIMELIMIT:
	case VOTECMD_POINTLIMIT:

		// Parameteter must be between 0 and 65535.
		if (( parameterInt < 0 ) || ( parameterInt >= 65536 ))
		{
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVER_PrintfPlayer( PRINT_HIGH, SERVER_GetCurrentClient( ), "%s parameters must be between 0 and 65535.\n", Command.GetChars() );
			return ( false );
		}
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
//
static ULONG callvote_GetVoteType( const char *pszCommand )
{
	if ( stricmp( "kick", pszCommand ) == 0 )
		return VOTECMD_KICK;
	else if ( stricmp( "map", pszCommand ) == 0 )
		return VOTECMD_MAP;
	else if ( stricmp( "changemap", pszCommand ) == 0 )
		return VOTECMD_CHANGEMAP;
	else if ( stricmp( "fraglimit", pszCommand ) == 0 )
		return VOTECMD_FRAGLIMIT;
	else if ( stricmp( "timelimit", pszCommand ) == 0 )
		return VOTECMD_TIMELIMIT;
	else if ( stricmp( "winlimit", pszCommand ) == 0 )
		return VOTECMD_WINLIMIT;
	else if ( stricmp( "duellimit", pszCommand ) == 0 )
		return VOTECMD_DUELLIMIT;
	else if ( stricmp( "pointlimit", pszCommand ) == 0 )
		return VOTECMD_POINTLIMIT;

	return NUM_VOTECMDS;
}

//*****************************************************************************
//	CONSOLE COMMANDS/VARIABLES

CUSTOM_CVAR( Int, sv_minvoters, 1, CVAR_ARCHIVE )
{
	if ( self < 1 )
		self = 1;
}
CVAR( Int, sv_nocallvote, 0, CVAR_ARCHIVE ); // 0 - everyone can call votes. 1 - nobody can. 2 - only players can.
CVAR( Bool, sv_nokickvote, false, CVAR_ARCHIVE );
CVAR( Bool, sv_nomapvote, false, CVAR_ARCHIVE );
CVAR( Bool, sv_nochangemapvote, false, CVAR_ARCHIVE );
CVAR( Bool, sv_nofraglimitvote, false, CVAR_ARCHIVE );
CVAR( Bool, sv_notimelimitvote, false, CVAR_ARCHIVE );
CVAR( Bool, sv_nowinlimitvote, false, CVAR_ARCHIVE );
CVAR( Bool, sv_noduellimitvote, false, CVAR_ARCHIVE );
CVAR( Bool, sv_nopointlimitvote, false, CVAR_ARCHIVE );
CVAR( Bool, sv_limitnumvotes, true, CVAR_ARCHIVE );
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

	if ( argv.argc( ) < 3 )
	{
		Printf( "callvote <command> <parameters> [reason]: Calls a vote\n" );
		return;
	}

	// [BB] No voting when not in a level, e.g. during intermission.
	if ( gamestate != GS_LEVEL )
	{
		Printf( "You cannot call a vote when not in a level!\n" );
		return;
	}

	ulVoteCmd = callvote_GetVoteType( argv[1] );
	if ( ulVoteCmd == NUM_VOTECMDS )
	{
		Printf( "Invalid callvote command.\n" );
		return;
	}

	if ( argv.argc( ) >= 4 )
		CLIENTCOMMANDS_CallVote( ulVoteCmd, argv[2], argv[3] );
	else
		CLIENTCOMMANDS_CallVote( ulVoteCmd, argv[2], "" );
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

//*****************************************************************************
//
CCMD ( cancelvote )
{
	if ( g_VoteState != VOTESTATE_INVOTE )
		return;

	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		SERVER_Printf( PRINT_HIGH, "Server cancelled the vote.\n" );
		g_bVoteCancelled = true;
		g_bVotePassed = false;
		callvote_EndVote( );
	}
	else if ( static_cast<LONG>(g_ulVoteCaller) == consoleplayer )
	{
		// Just vote no; we're the original caller, so it will be cancelled.
		if ( CLIENT_GetConnectionState( ) == CTS_ACTIVE )
		{
			CLIENTCOMMANDS_VoteNo( );
			NETWORK_LaunchPacket( CLIENT_GetLocalBuffer( ), CLIENT_GetServerAddress( ));
			NETWORK_ClearBuffer( CLIENT_GetLocalBuffer( ));
		}
	}
}
