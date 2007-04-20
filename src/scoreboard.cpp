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
// Filename: scoreboard.cpp
//
// Description: Contains scoreboard routines and globals
//
//-----------------------------------------------------------------------------

#include "a_pickups.h"
#include "c_dispatch.h"
#include "callvote.h"
#include "chat.h"
#include "cooperative.h"
#include "deathmatch.h"
#include "duel.h"
#include "doomtype.h"
#include "d_player.h"
#include "gi.h"
#include "invasion.h"
#include "joinqueue.h"
#include "lastmanstanding.h"
#include "network.h"
#include "possession.h"
#include "sbar.h"
#include "scoreboard.h"
#include "st_stuff.h"
#include "survival.h"
#include "team.h"
#include "templates.h"
#include "v_text.h"
#include "v_video.h"
#include "w_wad.h"
#include "c_bind.h" // [RC] To tell user what key to press to vote

//*****************************************************************************
//	VARIABLES

// Player list according to rank.
static	int		g_iSortedPlayers[MAXPLAYERS];

// How many players are currently in the game?
static	ULONG	g_ulNumPlayers = 0;

// What is our current rank?
static	ULONG	g_ulRank = 0;

// What is the spread between us and the person in 1st/2nd?
static	LONG	g_lSpread = 0;

// Is this player tied with another?
static	bool	g_bIsTied = false;

// How many opponents are left standing in LMS?
static	LONG	g_lNumOpponentsLeft = 0;

// Who has the terminator artifact?
static	player_s	*g_pTerminatorArtifactCarrier = NULL;

// Who has the possession artifact?
static	player_s	*g_pPossessionArtifactCarrier = NULL;

// Who is carrying the red flag/skull?
static	player_s	*g_pRedCarrier = NULL;

// Who is carrying the red flag/skull?
static	player_s	*g_pBlueCarrier = NULL;

// Who is carrying the white flag?
static	player_s	*g_pWhiteCarrier = NULL;

// Centered text that displays in the bottom of the screen.
static	FString		g_BottomString;

//*****************************************************************************
//	PROTOTYPES

static	void			scoreboard_SortPlayers( ULONG ulSortType );
static	int	STACK_ARGS	scoreboard_FragCompareFunc( const void *arg1, const void *arg2 );
static	int	STACK_ARGS	scoreboard_PointsCompareFunc( const void *arg1, const void *arg2 );
static	int	STACK_ARGS	scoreboard_KillsCompareFunc( const void *arg1, const void *arg2 );
static	int	STACK_ARGS	scoreboard_WinsCompareFunc( const void *arg1, const void *arg2 );
static	void			scoreboard_RenderIndividualPlayer( bool bScale, ULONG ulYPos, ULONG ulColor, char *pszColumn1, char *pszColumn2, char *pszColumn3, char *pszColumn4, bool bIsBot, bool bReadyToGoOn, ULONG ulHandicap );

//*****************************************************************************
//	FUNCTIONS

void SCOREBOARD_Render( player_s *pPlayer )
{
	DHUDMessageFadeOut	*pMsg;
	LONG				lPosition;

	// Draw the main scoreboard.
	if (
		(( NETWORK_GetState( ) != NETSTATE_SINGLE ) || ( deathmatch || teamgame || invasion )) &&
		( Button_ShowScores.bDown || (( pPlayer->camera && pPlayer->camera->health <= 0 ) && (( lastmanstanding || teamlms ) && (( LASTMANSTANDING_GetState( ) == LMSS_COUNTDOWN ) || ( LASTMANSTANDING_GetState( ) == LMSS_WAITINGFORPLAYERS )))  && ( teamlms == false ) && ( duel == false || ( DUEL_GetState( ) != DS_WINSEQUENCE ))))
		)
	{
		SCOREBOARD_RenderBoard( pPlayer );
	}

	g_BottomString = "";

	// If the console player is looking through someone else's eyes, draw the following message.
	if (( players[consoleplayer].camera ) && ( players[consoleplayer].camera != players[consoleplayer].mo ) && ( players[consoleplayer].camera->player ))
	{
		g_BottomString += "\\cgFOLLOWING - ";
		g_BottomString += players[consoleplayer].camera->player->userinfo.netname;
	}

	// If the console player is spectating, draw the spectator message.
	if ( players[consoleplayer].bSpectating )
	{
		g_BottomString += "\n";
		lPosition = JOINQUEUE_GetPositionInLine( ULONG( pPlayer - players ));
		if ( pPlayer->bDeadSpectator )
			g_BottomString += "\\cdSPECTATING - WAITING TO RESPAWN";
		else if ( lPosition != -1 )
		{
			switch ( lPosition )
			{
			case 0:

				g_BottomString += "\\cdWAITING TO PLAY - 1ST IN LINE";
				break;
			case 1:

				g_BottomString += "\\cdWAITING TO PLAY - 2ND IN LINE";
				break;
			case 2:

				g_BottomString += "\\cdWAITING TO PLAY - 3RD IN LINE";
				break;
			default:

				g_BottomString += "\\cdWAITING TO PLAY - ";
				g_BottomString.AppendFormat( "%d", lPosition + 1 );
				g_BottomString += "TH IN LINE";
				break;
			}
		}
		else
			g_BottomString += "\\cdSPECTATING - SPACE TO JOIN";

		if (( lastmanstanding || teamlms ) && 
			(( lmsspectatorsettings & LMS_SPF_CHAT ) == false ) &&
			( LASTMANSTANDING_GetState( ) == LMSS_INPROGRESS ))
		{
			g_BottomString += "\n\\cgNOTE: \\ccPLAYERS CANNOT HEAR YOU CHAT";
		}
/*
		pMsg = new DHUDMessageFadeOut( szString,
			1.5f,
			1.0f,
			0,
			0,
			CR_GREEN,
			0.10f,
			0.15f );

		StatusBar->AttachMessage( pMsg, 'WAIT' );
*/
	}

	if ( CALLVOTE_GetVoteState( ) != VOTESTATE_NOVOTE )
	{
		switch ( CALLVOTE_GetVoteState( ))
		{
		case VOTESTATE_INVOTE:
			// [RC] Display either the fullscreen or minimized vote screen
			if(cl_showfullscreenvote)
				SCOREBOARD_RenderInVoteClassic( );
			else
				SCOREBOARD_RenderInVote( );
			break;
		}

		// Display the message before we get out of here.
		V_ColorizeString( (char *)g_BottomString.GetChars( ));

		pMsg = new DHUDMessageFadeOut( g_BottomString,
			1.5f,
			1.0f,
			0,
			0,
			CR_WHITE,
			0.10f,
			0.15f );

		StatusBar->AttachMessage( pMsg, 'WAIT' );

		// This overrides everything.
		return;
	}

	if ( duel )
	{
		// Determine what to draw based on the duel state.
		switch ( DUEL_GetState( ))
		{
		case DS_COUNTDOWN:

			// Render "x vs. x" text.
			SCOREBOARD_RenderDuelCountdown( DUEL_GetCountdownTicks( ) + TICRATE );
			break;
		case DS_WAITINGFORPLAYERS:

			if ( pPlayer->bSpectating == false )
			{
				g_BottomString += "\\cgWAITING FOR PLAYERS";

				// Display the message before we get out of here.
				V_ColorizeString( (char *)g_BottomString.GetChars( ));

				pMsg = new DHUDMessageFadeOut( g_BottomString,
					1.5f,
					1.0f,
					0,
					0,
					CR_WHITE,
					0.10f,
					0.15f );

				StatusBar->AttachMessage( pMsg, 'WAIT' );

				// Nothing more to do if we're just waiting for players.
				return;
			}
			break;
		}
	}

	if ( lastmanstanding || teamlms )
	{
		// Determine what to draw based on the duel state.
		switch ( LASTMANSTANDING_GetState( ))
		{
		case LMSS_COUNTDOWN:

			// Render title text.
			SCOREBOARD_RenderLMSCountdown( LASTMANSTANDING_GetCountdownTicks( ) + TICRATE );
			break;
		case LMSS_WAITINGFORPLAYERS:

			if ( pPlayer->bSpectating == false )
			{
				g_BottomString += "\\cgWAITING FOR PLAYERS";

				// Display the message before we get out of here.
				V_ColorizeString( (char *)g_BottomString.GetChars( ));

				pMsg = new DHUDMessageFadeOut( g_BottomString,
					1.5f,
					1.0f,
					0,
					0,
					CR_WHITE,
					0.10f,
					0.15f );

				StatusBar->AttachMessage( pMsg, 'WAIT' );

				// Nothing more to do if we're just waiting for players.
				return;
			}
			break;
		}
	}

	if ( possession || teampossession )
	{
		// Determine what to draw based on the duel state.
		switch ( POSSESSION_GetState( ))
		{
		case PSNS_COUNTDOWN:
		case PSNS_NEXTROUNDCOUNTDOWN:

			// Render title text.
			SCOREBOARD_RenderPossessionCountdown(( POSSESSION_GetState( ) == PSNS_COUNTDOWN ) ? (( possession ) ? "POSSESSION" : "TEAM POSSESSION" ) : "NEXT ROUND IN...", POSSESSION_GetCountdownTicks( ) + TICRATE );
			break;
		case PSNS_WAITINGFORPLAYERS:

			if ( pPlayer->bSpectating == false )
			{
				g_BottomString += "\\cgWAITING FOR PLAYERS";

				// Display the message before we get out of here.
				V_ColorizeString( (char *)g_BottomString.GetChars( ));

				pMsg = new DHUDMessageFadeOut( g_BottomString,
					1.5f,
					1.0f,
					0,
					0,
					CR_WHITE,
					0.10f,
					0.15f );

				StatusBar->AttachMessage( pMsg, 'WAIT' );

				// Nothing more to do if we're just waiting for players.
				return;
			}
			break;
		}
	}

	if ( survival )
	{
		// Determine what to draw based on the survival state.
		switch ( SURVIVAL_GetState( ))
		{
		case SURVS_COUNTDOWN:

			// Render title text.
			SCOREBOARD_RenderSurvivalCountdown( SURVIVAL_GetCountdownTicks( ) + TICRATE );
			break;
		case SURVS_WAITINGFORPLAYERS:

			if ( pPlayer->bSpectating == false )
			{
				g_BottomString += "\\cgWAITING FOR PLAYERS";

				// Display the message before we get out of here.
				V_ColorizeString( (char *)g_BottomString.GetChars( ));

				pMsg = new DHUDMessageFadeOut( g_BottomString,
					1.5f,
					1.0f,
					0,
					0,
					CR_WHITE,
					0.10f,
					0.15f );

				StatusBar->AttachMessage( pMsg, 'WAIT' );

				// Nothing more to do if we're just waiting for players.
				return;
			}
			break;
		}
	}

	if ( invasion )
	{
		// Determine what to draw based on the invasion state.
		switch ( INVASION_GetState( ))
		{
		case IS_FIRSTCOUNTDOWN:

			// Render title text.
			SCOREBOARD_RenderInvasionFirstCountdown( INVASION_GetCountdownTicks( ) + TICRATE );
			break;
		case IS_COUNTDOWN:

			// Render title text.
			SCOREBOARD_RenderInvasionCountdown( INVASION_GetCountdownTicks( ) + TICRATE );
			break;
		case IS_INPROGRESS:
		case IS_BOSSFIGHT:

			// Render the number of monsters left, etc.
			SCOREBOARD_RenderInvasionStats( );
			break;
		}
	}

	// Don't draw this other stuff for spectators.
	if ( pPlayer->bSpectating )
	{
		// Display the message before we get out of here.
		V_ColorizeString( (char *)g_BottomString.GetChars( ));

		pMsg = new DHUDMessageFadeOut( g_BottomString,
			1.5f,
			1.0f,
			0,
			0,
			CR_WHITE,
			0.10f,
			0.15f );

		StatusBar->AttachMessage( pMsg, 'WAIT' );

		return;
	}

	// Allow the client to always draw certain deathmatch stats.
	if ( deathmatch && ( cl_alwaysdrawdmstats || automapactive ))
		SCOREBOARD_RenderDMStats( );
	// Allow the client to always draw certain team stats.
	else if ( teamgame && ( cl_alwaysdrawteamstats || automapactive ))
		SCOREBOARD_RenderTeamStats( pPlayer );

	// Display the message before we get out of here.
	V_ColorizeString( (char *)g_BottomString.GetChars( ));

	if ( g_BottomString.Len( ) > 0 )
	{
		pMsg = new DHUDMessageFadeOut( g_BottomString,
			1.5f,
			1.0f,
			0,
			0,
			CR_WHITE,
			0.10f,
			0.15f );

		StatusBar->AttachMessage( pMsg, 'WAIT' );
	}
}

//*****************************************************************************
//
void SCOREBOARD_RenderBoard( player_s *pPlayer )
{
	bool		bScale;
	char		szString[128];
	ULONG		ulPlayerNum;
	ULONG		ulCurYPos = 0;
	ULONG		ulIdx;
	ULONG		ulColor;
	char		szColumn1[16];
	char		szColumn3[16];
	char		szColumn4[16];
	ULONG		ulNumPlayers;
	UCVarValue	ValWidth;
	UCVarValue	ValHeight;
	float		fXScale;
	float		fYScale;

	ValWidth = con_virtualwidth.GetGenericRep( CVAR_Int );
	ValHeight = con_virtualheight.GetGenericRep( CVAR_Int );

	if (( con_scaletext ) && ( con_virtualwidth > 0 ) && ( con_virtualheight > 0 ))
	{
		fXScale =  (float)ValWidth.Int / 320.0f;
		fYScale =  (float)ValHeight.Int / 200.0f;
		bScale = true;
	}
	else
		bScale = false;

	// Calculate the playernum of the given player.
	ulPlayerNum = ULONG( pPlayer - players );

	if ( lastmanstanding )
		scoreboard_SortPlayers( ST_WINCOUNT );
	else if ( possession || teampossession )
		scoreboard_SortPlayers( ST_POINTCOUNT );
	else if ( deathmatch )
		scoreboard_SortPlayers( ST_FRAGCOUNT );
	else if ( teamgame )
		scoreboard_SortPlayers( ST_POINTCOUNT );
	else
		scoreboard_SortPlayers( ST_KILLCOUNT );

	// Start by drawing "RANKINGS" 4 pixels from the top. Don't draw it if we're in intermission.
	ulCurYPos = 4;

	if ( gamestate == GS_LEVEL )
	{
		screen->SetFont( BigFont );

		if ( bScale )
		{
			screen->DrawText( gameinfo.gametype == GAME_Doom ? CR_RED : CR_UNTRANSLATED,
				(LONG)(( ValWidth.Int / 2 ) - ( BigFont->StringWidth( "RANKINGS" ) / 2 )),
				ulCurYPos,
				"RANKINGS",
				DTA_VirtualWidth, ValWidth.Int,
				DTA_VirtualHeight, ValHeight.Int,
				TAG_DONE );
		}
		else
		{
			screen->DrawText( gameinfo.gametype == GAME_Doom ? CR_RED : CR_UNTRANSLATED,
				( SCREENWIDTH / 2 ) - ( BigFont->StringWidth( "RANKINGS" ) / 2 ),
				ulCurYPos,
				"RANKINGS",
				TAG_DONE );
		}

		screen->SetFont( SmallFont );
	}

	// Next, move the "cursor", and render the fraglimit string.
//	if ( bScale )
//		ulCurYPos += ( 22 * (LONG)ceil( fYScale ));
//	else
		ulCurYPos += 22;
	if (( lastmanstanding == false ) && ( teamlms == false ) && ( possession == false ) && ( teampossession == false ) && deathmatch && fraglimit && gamestate == GS_LEVEL )
	{
		LONG	lHighestFragcount;
		ULONG	ulFragsLeft;

		// If we're in a teamplay, just go by whichever team has the most frags.
		if ( teamplay )
		{
			if ( TEAM_GetFragCount( TEAM_BLUE ) >= TEAM_GetFragCount( TEAM_RED ))
				lHighestFragcount = TEAM_GetFragCount( TEAM_BLUE );
			else
				lHighestFragcount = TEAM_GetFragCount( TEAM_RED );
		}
		// Otherwise, find the player with the most frags.
		else
		{
			lHighestFragcount = INT_MIN;
			for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
			{
				if ( playeringame[ulIdx] == false )
					continue;

				if ( players[ulIdx].fragcount > lHighestFragcount )
					lHighestFragcount = players[ulIdx].fragcount;
			}
		}

		ulFragsLeft = fraglimit - lHighestFragcount;
		sprintf( szString, "%d frag%s remain%s", ulFragsLeft, ( ulFragsLeft != 1 ) ? "s" : "", ( ulFragsLeft == 1 ) ? "s" : "" );

		if ( bScale )
		{
			screen->DrawText( CR_GREY,
				(LONG)(( ValWidth.Int / 2 ) - ( SmallFont->StringWidth( szString ) / 2 )),
				ulCurYPos,
				szString,
				DTA_VirtualWidth, ValWidth.Int,
				DTA_VirtualHeight, ValHeight.Int,
				TAG_DONE );
		}
		else
		{
			screen->DrawText( CR_GREY,
				( SCREENWIDTH / 2 ) - ( SmallFont->StringWidth( szString ) / 2 ),
				ulCurYPos,
				szString,
				TAG_DONE );
		}

		ulCurYPos += 10;
	}

	// Render the duellimit string.
	if ( duellimit && duel && gamestate == GS_LEVEL )
	{
		LONG	lNumDuels;

		// Get the number of duels that have been played.
		lNumDuels = DUEL_GetNumDuels( );

		sprintf( szString, "%d duel%s remain%s", duellimit - lNumDuels, (( duellimit - lNumDuels ) == 1 ) ? "" : "s", (( duellimit - lNumDuels ) == 1 ) ? "s" : "" );

		if ( bScale )
		{
			screen->DrawText( CR_GREY,
				(LONG)(( ValWidth.Int / 2 ) - ( SmallFont->StringWidth( szString ) / 2 )),
				ulCurYPos,
				szString,
				DTA_VirtualWidth, ValWidth.Int,
				DTA_VirtualHeight, ValHeight.Int,
				TAG_DONE );
		}
		else
		{
			screen->DrawText( CR_GREY,
				( SCREENWIDTH / 2 ) - ( SmallFont->StringWidth( szString ) / 2 ),
				ulCurYPos,
				szString,
				TAG_DONE );
		}

		ulCurYPos += 10;
	}

	// Render the "wins" string.
	if ( duel && gamestate == GS_LEVEL )
	{
		LONG	lWinner = -1;
		ULONG	ulIdx;
		bool	bDraw = true;

		for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
		{
			if ( playeringame[ulIdx] && players[ulIdx].ulWins )
			{
				lWinner = ulIdx;
				break;
			}
		}

		if ( lWinner == -1 )
		{
			if ( DUEL_CountActiveDuelers( ) == 2 )
				sprintf( szString, "First match between the two" );
			else
				bDraw = false;
		}
		else
			sprintf( szString, "Champion is %s \\c-with %d win%s", players[lWinner].userinfo.netname, players[lWinner].ulWins, players[lWinner].ulWins == 1 ? "" : "s" );

		if ( bDraw )
		{
			V_ColorizeString( szString );

			if ( bScale )
			{
				screen->DrawText( CR_GREY,
					(LONG)(( ValWidth.Int / 2 ) - ( SmallFont->StringWidth( szString ) / 2 )),
					ulCurYPos,
					szString,
					DTA_VirtualWidth, ValWidth.Int,
					DTA_VirtualHeight, ValHeight.Int,
					TAG_DONE );
			}
			else
			{
				screen->DrawText( CR_GREY,
					( SCREENWIDTH / 2 ) - ( SmallFont->StringWidth( szString ) / 2 ),
					ulCurYPos,
					szString,
					TAG_DONE );
			}
			
			ulCurYPos += 10;
		}
	}

	// Render the pointlimit string.
	if (( teamgame || possession || teampossession ) && ( pointlimit ) && ( gamestate == GS_LEVEL ))
	{
		ULONG	ulPointsLeft;
		ULONG	ulBluePoints;
		ULONG	ulRedPoints;
		ULONG	ulIdx;
		LONG	lHighestPointCount;

		if ( teamgame || teampossession )
		{
			ulBluePoints = TEAM_GetScore( TEAM_BLUE );
			ulRedPoints = TEAM_GetScore( TEAM_RED );

			ulPointsLeft = pointlimit - (( ulBluePoints >= ulRedPoints ) ? ulBluePoints : ulRedPoints );
		}
		// Must be possession mode.
		else
		{
			lHighestPointCount = INT_MIN;
			for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
			{
				if ( playeringame[ulIdx] == false )
					continue;

				if ( (LONG)players[ulIdx].lPointCount > lHighestPointCount )
					lHighestPointCount = players[ulIdx].lPointCount;
			}

			ulPointsLeft = pointlimit - (ULONG)lHighestPointCount;
		}

		sprintf( szString, "%d points remain", ulPointsLeft );

		if ( bScale )
		{
			screen->DrawText( CR_GREY,
				(LONG)(( ValWidth.Int / 2 ) - ( SmallFont->StringWidth( szString ) / 2 )),
				ulCurYPos,
				szString,
				DTA_VirtualWidth, ValWidth.Int,
				DTA_VirtualHeight, ValHeight.Int,
				TAG_DONE );
		}
		else
		{
			screen->DrawText( CR_GREY,
				( SCREENWIDTH / 2 ) - ( SmallFont->StringWidth( szString ) / 2 ),
				ulCurYPos,
				szString,
				TAG_DONE );
		}

		ulCurYPos += 10;
	}

	// Render the winlimit string.
	if (( lastmanstanding || teamlms ) && winlimit && gamestate == GS_LEVEL )
	{
		bool	bFoundPlayer = false;
		LONG	lHighestWincount;
		ULONG	ulWinsLeft;

		// If we're in a teamplay, just go by whichever team has the most frags.
		if ( teamlms )
		{
			if ( TEAM_GetWinCount( TEAM_BLUE ) >= TEAM_GetWinCount( TEAM_RED ))
				lHighestWincount = TEAM_GetWinCount( TEAM_BLUE );
			else
				lHighestWincount = TEAM_GetWinCount( TEAM_RED );
		}
		// Otherwise, find the player with the most frags.
		else
		{
			for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
			{
				if ( playeringame[ulIdx] == false )
					continue;

				if ( bFoundPlayer == false )
				{
					lHighestWincount = players[ulIdx].ulWins;
					bFoundPlayer = true;
					continue;
				}
				else if ( players[ulIdx].ulWins > (ULONG)lHighestWincount )
					lHighestWincount = players[ulIdx].ulWins;
			}
		}

		ulWinsLeft = winlimit - lHighestWincount;
		sprintf( szString, "%d win%s remain%s", ulWinsLeft, ( ulWinsLeft != 1 ) ? "s" : "", ( ulWinsLeft == 1 ) ? "s" : "" );
		
		if ( bScale )
		{
			screen->DrawText( CR_GREY,
				(LONG)(( ValWidth.Int / 2 ) - ( SmallFont->StringWidth( szString ) / 2 )),
				ulCurYPos,
				szString,
				DTA_VirtualWidth, ValWidth.Int,
				DTA_VirtualHeight, ValHeight.Int,
				TAG_DONE );
		}
		else
		{
			screen->DrawText( CR_GREY,
				( SCREENWIDTH / 2 ) - ( SmallFont->StringWidth( szString ) / 2 ),
				ulCurYPos,
				szString,
				TAG_DONE );
		}

		ulCurYPos += 10;
	}

	// Render the wavelimit string.
	if ( invasion && wavelimit && gamestate == GS_LEVEL )
	{
		ULONG	ulWavesLeft;

		ulWavesLeft = wavelimit - INVASION_GetCurrentWave( );
		sprintf( szString, "%d wave%s remain%s", ulWavesLeft, ( ulWavesLeft != 1 ) ? "s" : "", ( ulWavesLeft == 1 ) ? "s" : "" );
		
		if ( bScale )
		{
			screen->DrawText( CR_GREY,
				(LONG)(( ValWidth.Int / 2 ) - ( SmallFont->StringWidth( szString ) / 2 )),
				ulCurYPos,
				szString,
				DTA_VirtualWidth, ValWidth.Int,
				DTA_VirtualHeight, ValHeight.Int,
				TAG_DONE );
		}
		else
		{
			screen->DrawText( CR_GREY,
				( SCREENWIDTH / 2 ) - ( SmallFont->StringWidth( szString ) / 2 ),
				ulCurYPos,
				szString,
				TAG_DONE );
		}

		ulCurYPos += 10;
	}

	// Render the timelimit string.
	if (( deathmatch || teamgame ) && timelimit && gamestate == GS_LEVEL )
	{
		LONG	lTimeLeft = (LONG)( timelimit * ( TICRATE * 60 )) - level.time;
		ULONG	ulHours;
		ULONG	ulMinutes;
		ULONG	ulSeconds;

		if ( lTimeLeft <= 0 )
			ulHours = ulMinutes = ulSeconds = 0;
		else
		{
			ulHours = lTimeLeft / ( TICRATE * 3600 );
			lTimeLeft -= ulHours * TICRATE * 3600;
			ulMinutes = lTimeLeft / ( TICRATE * 60 );
			lTimeLeft -= ulMinutes * TICRATE * 60;
			ulSeconds = lTimeLeft / TICRATE;
		}

		if ( lastmanstanding || teamlms )
		{
			if ( ulHours )
				sprintf( szString, "Round ends in %02d:%02d:%02d", ulHours, ulMinutes, ulSeconds );
			else
				sprintf( szString, "Round ends in %02d:%02d", ulMinutes, ulSeconds );
		}
		else
		{
			if ( ulHours )
				sprintf( szString, "Level ends in %02d:%02d:%02d", ulHours, ulMinutes, ulSeconds );
			else
				sprintf( szString, "Level ends in %02d:%02d", ulMinutes, ulSeconds );
		}
		
		if ( bScale )
		{
			screen->DrawText( CR_GREY,
				(LONG)(( ValWidth.Int / 2 ) - ( SmallFont->StringWidth( szString ) / 2 )),
				ulCurYPos,
				szString,
				DTA_VirtualWidth, ValWidth.Int,
				DTA_VirtualHeight, ValHeight.Int,
				TAG_DONE );
		}
		else
		{
			screen->DrawText( CR_GREY,
				( SCREENWIDTH / 2 ) - ( SmallFont->StringWidth( szString ) / 2 ),
				ulCurYPos,
				szString,
				TAG_DONE );
		}

		ulCurYPos += 10;
	}

	// Render the current team scores.
	if ( teamplay || teamgame || teamlms || teampossession )
	{
		if ( gamestate == GS_LEVEL )
		{
			if ( teamplay )
			{
				// If the teams are tied...
				if ( TEAM_GetFragCount( TEAM_RED ) == TEAM_GetFragCount( TEAM_BLUE ))
				{
					sprintf( szString, "Teams are tied at %d\n", TEAM_GetFragCount( TEAM_RED ));
					if ( bScale )
					{
						screen->DrawText( CR_GREY,
							(LONG)(( ValWidth.Int / 2 ) - ( SmallFont->StringWidth( szString ) / 2 )),
							ulCurYPos,
							szString,
							DTA_VirtualWidth, ValWidth.Int,
							DTA_VirtualHeight, ValHeight.Int,
							TAG_DONE );
					}
					else
					{
						screen->DrawText( CR_GREY,
							( SCREENWIDTH / 2 ) - ( SmallFont->StringWidth( szString ) / 2 ),
							ulCurYPos,
							szString,
							TAG_DONE );
					}
				}
				else
				{
					if ( TEAM_GetFragCount( TEAM_RED ) > TEAM_GetFragCount( TEAM_BLUE ))
						sprintf( szString, "%s leads %d to %d", TEAM_GetName( TEAM_RED ), TEAM_GetFragCount( TEAM_RED ), TEAM_GetFragCount( TEAM_BLUE ));
					else
						sprintf( szString, "%s leads %d to %d", TEAM_GetName( TEAM_BLUE ), TEAM_GetFragCount( TEAM_BLUE ), TEAM_GetFragCount( TEAM_RED ));

					if ( bScale )
					{
						screen->DrawText( CR_GREY,
							(LONG)(( ValWidth.Int / 2 ) - ( SmallFont->StringWidth( szString ) / 2 )),
							ulCurYPos,
							szString,
							DTA_VirtualWidth, ValWidth.Int,
							DTA_VirtualHeight, ValHeight.Int,
							TAG_DONE );
					}
					else
					{
						screen->DrawText( CR_GREY,
							( SCREENWIDTH / 2 ) - ( SmallFont->StringWidth( szString ) / 2 ),
							ulCurYPos,
							szString,
							TAG_DONE );
					}
				}
			}
			else if ( teamlms )
			{
				// If the teams are tied...
				if ( TEAM_GetWinCount( TEAM_RED ) == TEAM_GetWinCount( TEAM_BLUE ))
				{
					sprintf( szString, "Teams are tied at %d\n", TEAM_GetWinCount( TEAM_RED ));
					if ( bScale )
					{
						screen->DrawText( CR_GREY,
							(LONG)(( ValWidth.Int / 2 ) - ( SmallFont->StringWidth( szString ) / 2 )),
							ulCurYPos,
							szString,
							DTA_VirtualWidth, ValWidth.Int,
							DTA_VirtualHeight, ValHeight.Int,
							TAG_DONE );
					}
					else
					{
						screen->DrawText( CR_GREY,
							( SCREENWIDTH / 2 ) - ( SmallFont->StringWidth( szString ) / 2 ),
							ulCurYPos,
							szString,
							TAG_DONE );
					}
				}
				else
				{
					if ( TEAM_GetWinCount( TEAM_RED ) > TEAM_GetWinCount( TEAM_BLUE ))
						sprintf( szString, "%s leads %d to %d", TEAM_GetName( TEAM_RED ), TEAM_GetWinCount( TEAM_RED ), TEAM_GetWinCount( TEAM_BLUE ));
					else
						sprintf( szString, "%s leads %d to %d", TEAM_GetName( TEAM_BLUE ), TEAM_GetWinCount( TEAM_BLUE ), TEAM_GetWinCount( TEAM_RED ));

					if ( bScale )
					{
						screen->DrawText( CR_GREY,
							(LONG)(( ValWidth.Int / 2 ) - ( SmallFont->StringWidth( szString ) / 2 )),
							ulCurYPos,
							szString,
							DTA_VirtualWidth, ValWidth.Int,
							DTA_VirtualHeight, ValHeight.Int,
							TAG_DONE );
					}
					else
					{
						screen->DrawText( CR_GREY,
							( SCREENWIDTH / 2 ) - ( SmallFont->StringWidth( szString ) / 2 ),
							ulCurYPos,
							szString,
							TAG_DONE );
					}
				}
			}
			else
			{
				// If the teams are tied...
				if ( TEAM_GetScore( TEAM_RED ) == TEAM_GetScore( TEAM_BLUE ))
				{
					sprintf( szString, "Teams are tied at %d\n", TEAM_GetScore( TEAM_RED ));
					if ( bScale )
					{
						screen->DrawText( CR_GREY,
							(LONG)(( ValWidth.Int / 2 ) - ( SmallFont->StringWidth( szString ) / 2 )),
							ulCurYPos,
							szString,
							DTA_VirtualWidth, ValWidth.Int,
							DTA_VirtualHeight, ValHeight.Int,
							TAG_DONE );
					}
					else
					{
						screen->DrawText( CR_GREY,
							( SCREENWIDTH / 2 ) - ( SmallFont->StringWidth( szString ) / 2 ),
							ulCurYPos,
							szString,
							TAG_DONE );
					}
				}
				else
				{
					if ( TEAM_GetScore( TEAM_RED ) > TEAM_GetScore( TEAM_BLUE ))
						sprintf( szString, "%s leads %d to %d", TEAM_GetName( TEAM_RED ), TEAM_GetScore( TEAM_RED ), TEAM_GetScore( TEAM_BLUE ));
					else
						sprintf( szString, "%s leads %d to %d", TEAM_GetName( TEAM_BLUE ), TEAM_GetScore( TEAM_BLUE ), TEAM_GetScore( TEAM_RED ));

					if ( bScale )
					{
						screen->DrawText( CR_GREY,
							(LONG)(( ValWidth.Int / 2 ) - ( SmallFont->StringWidth( szString ) / 2 )),
							ulCurYPos,
							szString,
							DTA_VirtualWidth, ValWidth.Int,
							DTA_VirtualHeight, ValHeight.Int,
							TAG_DONE );
					}
					else
					{
						screen->DrawText( CR_GREY,
							( SCREENWIDTH / 2 ) - ( SmallFont->StringWidth( szString ) / 2 ),
							ulCurYPos,
							szString,
							TAG_DONE );
					}
				}
			}

			// Draw the team info information.
			// If this variable is true, it's already been drawn. No need to do it again.
			if ( cl_alwaysdrawteamstats == false )
				SCOREBOARD_RenderTeamStats( pPlayer );
		}
		else
		{
			ulCurYPos += 10;
			if ( teamplay )
			{
				// If the teams are tied...
				if ( TEAM_GetFragCount( TEAM_RED ) == TEAM_GetFragCount( TEAM_BLUE ))
				{
					sprintf( szString, "Teams tied at %d\n", TEAM_GetFragCount( TEAM_RED ));
					if ( bScale )
					{
						screen->DrawText( CR_GREY,
							(LONG)(( ValWidth.Int / 2 ) - ( SmallFont->StringWidth( szString ) / 2 )),
							ulCurYPos,
							szString,
							DTA_VirtualWidth, ValWidth.Int,
							DTA_VirtualHeight, ValHeight.Int,
							TAG_DONE );
					}
					else
					{
						screen->DrawText( CR_GREY,
							( SCREENWIDTH / 2 ) - ( SmallFont->StringWidth( szString ) / 2 ),
							ulCurYPos,
							szString,
							TAG_DONE );
					}
				}
				else
				{
					if ( TEAM_GetFragCount( TEAM_RED ) > TEAM_GetFragCount( TEAM_BLUE ))
						sprintf( szString, "%s has won %d to %d", TEAM_GetName( TEAM_RED ), TEAM_GetFragCount( TEAM_RED ), TEAM_GetFragCount( TEAM_BLUE ));
					else
						sprintf( szString, "%s has won %d to %d", TEAM_GetName( TEAM_BLUE ), TEAM_GetFragCount( TEAM_BLUE ), TEAM_GetFragCount( TEAM_RED ));

					if ( bScale )
					{
						screen->DrawText( CR_GREY,
							(LONG)(( ValWidth.Int / 2 ) - ( SmallFont->StringWidth( szString ) / 2 )),
							ulCurYPos,
							szString,
							DTA_VirtualWidth, ValWidth.Int,
							DTA_VirtualHeight, ValHeight.Int,
							TAG_DONE );
					}
					else
					{
						screen->DrawText( CR_GREY,
							( SCREENWIDTH / 2 ) - ( SmallFont->StringWidth( szString ) / 2 ),
							ulCurYPos,
							szString,
							TAG_DONE );
					}
				}
			}
			else if ( teamlms )
			{
				// If the teams are tied...
				if ( TEAM_GetWinCount( TEAM_RED ) == TEAM_GetWinCount( TEAM_BLUE ))
				{
					sprintf( szString, "Teams tied at %d\n", TEAM_GetWinCount( TEAM_RED ));
					if ( bScale )
					{
						screen->DrawText( CR_GREY,
							(LONG)(( ValWidth.Int / 2 ) - ( SmallFont->StringWidth( szString ) / 2 )),
							ulCurYPos,
							szString,
							DTA_VirtualWidth, ValWidth.Int,
							DTA_VirtualHeight, ValHeight.Int,
							TAG_DONE );
					}
					else
					{
						screen->DrawText( CR_GREY,
							( SCREENWIDTH / 2 ) - ( SmallFont->StringWidth( szString ) / 2 ),
							ulCurYPos,
							szString,
							TAG_DONE );
					}
				}
				else
				{
					if ( TEAM_GetWinCount( TEAM_RED ) > TEAM_GetWinCount( TEAM_BLUE ))
						sprintf( szString, "%s has won %d to %d", TEAM_GetName( TEAM_RED ), TEAM_GetWinCount( TEAM_RED ), TEAM_GetWinCount( TEAM_BLUE ));
					else
						sprintf( szString, "%s has won %d to %d", TEAM_GetName( TEAM_BLUE ), TEAM_GetWinCount( TEAM_BLUE ), TEAM_GetWinCount( TEAM_RED ));

					if ( bScale )
					{
						screen->DrawText( CR_GREY,
							(LONG)(( ValWidth.Int / 2 ) - ( SmallFont->StringWidth( szString ) / 2 )),
							ulCurYPos,
							szString,
							DTA_VirtualWidth, ValWidth.Int,
							DTA_VirtualHeight, ValHeight.Int,
							TAG_DONE );
					}
					else
					{
						screen->DrawText( CR_GREY,
							( SCREENWIDTH / 2 ) - ( SmallFont->StringWidth( szString ) / 2 ),
							ulCurYPos,
							szString,
							TAG_DONE );
					}
				}
			}
			else
			{
				// If the teams are tied...
				if ( TEAM_GetScore( TEAM_RED ) == TEAM_GetScore( TEAM_BLUE ))
				{
					sprintf( szString, "Teams tied at %d\n", TEAM_GetScore( TEAM_RED ));
					if ( bScale )
					{
						screen->DrawText( CR_GREY,
							(LONG)(( ValWidth.Int / 2 ) - ( SmallFont->StringWidth( szString ) / 2 )),
							ulCurYPos,
							szString,
							DTA_VirtualWidth, ValWidth.Int,
							DTA_VirtualHeight, ValHeight.Int,
							TAG_DONE );
					}
					else
					{
						screen->DrawText( CR_GREY,
							( SCREENWIDTH / 2 ) - ( SmallFont->StringWidth( szString ) / 2 ),
							ulCurYPos,
							szString,
							TAG_DONE );
					}
				}
				else
				{
					if ( TEAM_GetScore( TEAM_RED ) > TEAM_GetScore( TEAM_BLUE ))
						sprintf( szString, "%s has won %d to %d", TEAM_GetName( TEAM_RED ), TEAM_GetScore( TEAM_RED ), TEAM_GetScore( TEAM_BLUE ));
					else
						sprintf( szString, "%s has won %d to %d", TEAM_GetName( TEAM_BLUE ), TEAM_GetScore( TEAM_BLUE ), TEAM_GetScore( TEAM_RED ));

					if ( bScale )
					{
						screen->DrawText( CR_GREY,
							(LONG)(( ValWidth.Int / 2 ) - ( SmallFont->StringWidth( szString ) / 2 )),
							ulCurYPos,
							szString,
							DTA_VirtualWidth, ValWidth.Int,
							DTA_VirtualHeight, ValHeight.Int,
							TAG_DONE );
					}
					else
					{
						screen->DrawText( CR_GREY,
							( SCREENWIDTH / 2 ) - ( SmallFont->StringWidth( szString ) / 2 ),
							ulCurYPos,
							szString,
							TAG_DONE );
					}
				}
			}
		}

		ulCurYPos += 10;
	}

	// Render the current ranking string.
	if ( deathmatch && ( teamplay == false ) && ( teamlms == false ) && ( teampossession == false ) && ( PLAYER_IsTrueSpectator( &players[ulPlayerNum] ) == false ) && gamestate == GS_LEVEL )
	{
		bool	bIsTied;

		bIsTied	= SCOREBOARD_IsTied( ulPlayerNum );

		// If the player is tied with someone else, add a "tied for" to their string.
		if ( bIsTied )
			sprintf( szString, "Tied for " );
		else
			sprintf( szString, "" );

		// Determine  what color and number to print for their rank.
		switch ( g_ulRank )
		{
		case 0:

			sprintf( szString, "%s\\cH1st ", szString );
			break;
		case 1:

			sprintf( szString, "%s\\cG2nd ", szString );
			break;
		case 2:

			sprintf( szString, "%s\\cD3rd ", szString );
			break;
		default:

			sprintf( szString, "%s%dth ", szString, ( g_ulRank + 1 ));
			break;
		}

		// Tack on the rest of the string.
		if ( lastmanstanding )
			sprintf( szString, "%s\\c-place with %d win%s", szString, players[ulPlayerNum].ulWins, players[ulPlayerNum].ulWins == 1 ? "" : "s" );
		else if ( possession )
			sprintf( szString, "%s\\c-place with %d point%s", szString, players[ulPlayerNum].lPointCount, players[ulPlayerNum].fragcount == 1 ? "" : "s" );
		else
			sprintf( szString, "%s\\c-place with %d frag%s", szString, players[ulPlayerNum].fragcount, players[ulPlayerNum].fragcount == 1 ? "" : "s" );
		V_ColorizeString( szString );

		if ( bScale )
		{
			screen->DrawText( CR_GREY,
				(LONG)(( ValWidth.Int / 2 ) - ( SmallFont->StringWidth( szString ) / 2 )),
				ulCurYPos,
				szString,
				DTA_VirtualWidth, ValWidth.Int,
				DTA_VirtualHeight, ValHeight.Int,
				TAG_DONE );
		}
		else
		{
			screen->DrawText( CR_GREY,
				( SCREENWIDTH / 2 ) - ( SmallFont->StringWidth( szString ) / 2 ),
				ulCurYPos,
				szString,
				TAG_DONE );
		}

		ulCurYPos += 10;
	}
	
	// Render the number of monsters left in coop.
	if (( deathmatch == false ) && ( teamgame == false ) && ( gamestate == GS_LEVEL ))
	{
		LONG	lNumMonstersRemaining;

		if ( invasion )
			lNumMonstersRemaining = (LONG)INVASION_GetNumMonstersLeft( );
		else
			lNumMonstersRemaining = level.total_monsters - level.killed_monsters;
		sprintf( szString, "%d monster%s remaining", lNumMonstersRemaining, lNumMonstersRemaining == 1 ? "" : "s" );
		
		if ( bScale )
		{
			screen->DrawText( CR_GREY,
				(LONG)(( ValWidth.Int / 2 ) - ( SmallFont->StringWidth( szString ) / 2 )),
				ulCurYPos,
				szString,
				DTA_VirtualWidth, ValWidth.Int,
				DTA_VirtualHeight, ValHeight.Int,
				TAG_DONE );
		}
		else
		{
			screen->DrawText( CR_GREY,
				( SCREENWIDTH / 2 ) - ( SmallFont->StringWidth( szString ) / 2 ),
				ulCurYPos,
				szString,
				TAG_DONE );
		}

		ulCurYPos += 10;
	}

	ulCurYPos += 8;

	// Center this a little better in intermission
	if ( gamestate != GS_LEVEL )
		ulCurYPos = ( bScale == true ) ? (LONG)( 48 * fYScale ) : (LONG)( 48 * CleanYfac );

	if ( bScale )
	{
		if ( ValWidth.Int >= 640 )
			screen->SetFont( BigFont );
		else
			screen->SetFont( SmallFont );
	}
	else if ( CleanXfac >= 2 )
		screen->SetFont( BigFont );
	else
		screen->SetFont( SmallFont );

	// Draw the titles for the 3 colums.
	if ( teamgame )
	{
		if ( bScale )
		{
			screen->DrawText( CR_RED,
				(LONG)( COLUMN1_XPOS * fXScale ),
				ulCurYPos,
				"SCORE",
				DTA_VirtualWidth, ValWidth.Int,
				DTA_VirtualHeight, ValHeight.Int,
				TAG_DONE );
		}
		else
		{
			screen->DrawText( CR_RED,
				(LONG)( COLUMN1_XPOS * CleanXfac ),
				ulCurYPos,
				"SCORE",
				TAG_DONE );
		}
	}
	else if ( lastmanstanding )
	{
		if ( bScale )
		{
			screen->DrawText( CR_RED,
				(LONG)( COLUMN1_XPOS * fXScale ),
				ulCurYPos,
				"WINS",
				DTA_VirtualWidth, ValWidth.Int,
				DTA_VirtualHeight, ValHeight.Int,
				TAG_DONE );
		}
		else
		{
			screen->DrawText( CR_RED,
				(LONG)( COLUMN1_XPOS * CleanXfac ),
				ulCurYPos,
				"WINS",
				TAG_DONE );
		}
	}
	else if ( possession || teampossession )
	{
		if ( bScale )
		{
			screen->DrawText( CR_RED,
				(LONG)( COLUMN1_XPOS * fXScale ),
				ulCurYPos,
				"POINTS",
				DTA_VirtualWidth, ValWidth.Int,
				DTA_VirtualHeight, ValHeight.Int,
				TAG_DONE );
		}
		else
		{
			screen->DrawText( CR_RED,
				(LONG)( COLUMN1_XPOS * CleanXfac ),
				ulCurYPos,
				"POINTS",
				TAG_DONE );
		}
	}
	else if ( deathmatch )
	{
		if ( bScale )
		{
			screen->DrawText( CR_RED,
				(LONG)( COLUMN1_XPOS * fXScale ),
				ulCurYPos,
				"FRAGS",
				DTA_VirtualWidth, ValWidth.Int,
				DTA_VirtualHeight, ValHeight.Int,
				TAG_DONE );
		}
		else
		{
			screen->DrawText( CR_RED,
				(LONG)( COLUMN1_XPOS * CleanXfac ),
				ulCurYPos,
				"FRAGS",
				TAG_DONE );
		}
	}
	else
	{
		if ( bScale )
		{
			screen->DrawText( CR_RED,
				(LONG)( COLUMN1_XPOS * fXScale ),
				ulCurYPos,
				"KILLS",
				DTA_VirtualWidth, ValWidth.Int,
				DTA_VirtualHeight, ValHeight.Int,
				TAG_DONE );
		}
		else
		{
			screen->DrawText( CR_RED,
				(LONG)( COLUMN1_XPOS * CleanXfac ),
				ulCurYPos,
				"KILLS",
				TAG_DONE );
		}
	}

	if ( bScale )
	{
		screen->DrawText( CR_RED,
			(LONG)( COLUMN2_XPOS * fXScale ),
			ulCurYPos,
			"NAME",
			DTA_VirtualWidth, ValWidth.Int,
			DTA_VirtualHeight, ValHeight.Int,
			TAG_DONE );
	}
	else
	{
		screen->DrawText( CR_RED,
			(LONG)( COLUMN2_XPOS * CleanXfac ),
			ulCurYPos,
			"NAME",
			TAG_DONE );
	}

	// Third column. In a network game, we draw ping. In a teamgame, we draw frags. Otherwise, we draw deaths.
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
	{
		if ( bScale )
		{
			screen->DrawText( CR_RED,
				(LONG)( COLUMN3_XPOS * fXScale ),
				ulCurYPos,
				"PING",
				DTA_VirtualWidth, ValWidth.Int,
				DTA_VirtualHeight, ValHeight.Int,
				TAG_DONE );
		}
		else
		{
			screen->DrawText( CR_RED,
				(LONG)( COLUMN3_XPOS * CleanXfac ),
				ulCurYPos,
				"PING",
				TAG_DONE );
		}
	}
	else if ( teamgame || possession || teampossession )
	{
		if ( bScale )
		{
			screen->DrawText( CR_RED,
				(LONG)( COLUMN3_XPOS * fXScale ),
				ulCurYPos,
				"FRAGS",
				DTA_VirtualWidth, ValWidth.Int,
				DTA_VirtualHeight, ValHeight.Int,
				TAG_DONE );
		}
		else
		{
			screen->DrawText( CR_RED,
				(LONG)( COLUMN3_XPOS * CleanXfac ),
				ulCurYPos,
				"FRAGS",
				TAG_DONE );
		}
	}
	else
	{
		if ( bScale )
		{
			screen->DrawText( CR_RED,
				(LONG)( COLUMN3_XPOS * fXScale ),
				ulCurYPos,
				"DEATHS",
				DTA_VirtualWidth, ValWidth.Int,
				DTA_VirtualHeight, ValHeight.Int,
				TAG_DONE );
		}
		else
		{
			screen->DrawText( CR_RED,
				(LONG)( COLUMN3_XPOS * CleanXfac ),
				ulCurYPos,
				"DEATHS",
				TAG_DONE );
		}
	}

	if ( bScale )
	{
		screen->DrawText( CR_RED,
			(LONG)( COLUMN4_XPOS * fXScale ),
			ulCurYPos,
			"TIME",
			DTA_VirtualWidth, ValWidth.Int,
			DTA_VirtualHeight, ValHeight.Int,
			TAG_DONE );
	}
	else
	{
		screen->DrawText( CR_RED,
			(LONG)( COLUMN4_XPOS * CleanXfac ),
			ulCurYPos,
			"TIME",
			TAG_DONE );
	}

	ulCurYPos += 24;

	screen->SetFont( SmallFont );

	if ( teamgame || teamplay || teamlms || teampossession )
	{
		ULONG	ulTeamIdx;

		for ( ulTeamIdx = 0; ulTeamIdx < NUM_TEAMS; ulTeamIdx++ )
		{
			// Loop through all the players. If they're in this team, draw their name
			// on the scoreboard.
			ulNumPlayers = 0;
			for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
			{
				// Skip players that aren't playing.
				if ( playeringame[g_iSortedPlayers[ulIdx]] == false ) 
					continue;

				// Skip spectating players, or players not on a team.
				if ( PLAYER_IsTrueSpectator( &players[g_iSortedPlayers[ulIdx]] ) || ( players[g_iSortedPlayers[ulIdx]].bOnTeam == false ))
					continue;

				// Skip players that aren't on this team.
				if ( players[g_iSortedPlayers[ulIdx]].ulTeam != ulTeamIdx )
					continue;

				// If this is team LMS, they're dead, and it's not intermission, don't draw them right now.
				if (( teamlms ) && ( players[g_iSortedPlayers[ulIdx]].health <= 0 ) && ( gamestate != GS_INTERMISSION ) && ( LASTMANSTANDING_GetState( ) != LMSS_COUNTDOWN ) && ( LASTMANSTANDING_GetState( ) != LMSS_WAITINGFORPLAYERS ))
					continue;

				// Draw the individual player's points/frags.
				if ( teamgame || teampossession )
					sprintf( szColumn1, "%d", players[g_iSortedPlayers[ulIdx]].lPointCount );
				else
					sprintf( szColumn1, "%d", players[g_iSortedPlayers[ulIdx]].fragcount );
/*			
				if ( &players[g_iSortedPlayers[ulIdx]] == TEAM_GetLeader( ulTeamIdx ))
					ulColor = CR_GOLD;
				else
*/
					ulColor = TEAM_GetTextColor( ulTeamIdx );

				// Draw the ping, or fragcount, or deathcount.
				if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
					sprintf( szColumn3, "%d", players[g_iSortedPlayers[ulIdx]].ulPing );
				else if ( teamgame || teampossession )
					sprintf( szColumn3, "%d", players[g_iSortedPlayers[ulIdx]].fragcount );
				else if ( teamplay || teamlms )
					sprintf( szColumn3, "%d", players[g_iSortedPlayers[ulIdx]].ulDeathCount );

				// Draw the time.
				sprintf( szColumn4, "%d", ( players[g_iSortedPlayers[ulIdx]].ulTime / ( TICRATE * 60 )));

				scoreboard_RenderIndividualPlayer( bScale,
					ulCurYPos,
					ulColor,
					szColumn1,
					players[g_iSortedPlayers[ulIdx]].userinfo.netname,
					szColumn3,
					szColumn4,
					players[g_iSortedPlayers[ulIdx]].bIsBot,
					players[g_iSortedPlayers[ulIdx]].bReadyToGoOn,
					players[g_iSortedPlayers[ulIdx]].userinfo.lHandicap );

				ulCurYPos += 10;
				ulNumPlayers++;
			}

			if ( ulNumPlayers )
			{
				ulNumPlayers = 0;
				ulCurYPos += 10;
			}
			// Now, if this is team LMS, draw dead players.
			if (( teamlms ) && ( gamestate != GS_INTERMISSION ) && ( LASTMANSTANDING_GetState( ) != LMSS_COUNTDOWN ) && ( LASTMANSTANDING_GetState( ) != LMSS_WAITINGFORPLAYERS ))
			{
				for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
				{
					// Skip players that aren't playing.
					if ( playeringame[g_iSortedPlayers[ulIdx]] == false ) 
						continue;

					// Skip spectating players, or players not on a team.
					if ( PLAYER_IsTrueSpectator( &players[g_iSortedPlayers[ulIdx]] ) || ( players[g_iSortedPlayers[ulIdx]].bOnTeam == false ))
						continue;

					// Skip players that aren't on this team.
					if ( players[g_iSortedPlayers[ulIdx]].ulTeam != ulTeamIdx )
						continue;

					if ((( players[g_iSortedPlayers[ulIdx]].health <= 0 ) && ( gamestate != GS_INTERMISSION ) && ( LASTMANSTANDING_GetState( ) != LMSS_COUNTDOWN ) && ( LASTMANSTANDING_GetState( ) != LMSS_WAITINGFORPLAYERS )) == false )
						continue;
/*
					if ( &players[g_iSortedPlayers[ulIdx]] == TEAM_GetLeader( ulTeamIdx ))
						ulColor = CR_GOLD;
					else
*/
						ulColor = TEAM_GetTextColor( ulTeamIdx );

					// Draw the ping, or fragcount, or deathcount.
					if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
						sprintf( szColumn3, "%d", players[g_iSortedPlayers[ulIdx]].ulPing );
					else
						sprintf( szColumn3, "%d", players[g_iSortedPlayers[ulIdx]].ulDeathCount );

					// Draw the time.
					sprintf( szColumn4, "%d", ( players[g_iSortedPlayers[ulIdx]].ulTime / ( TICRATE * 60 )));

					scoreboard_RenderIndividualPlayer( bScale,
						ulCurYPos,
						ulColor,
						"DEAD",
						players[g_iSortedPlayers[ulIdx]].userinfo.netname,
						szColumn3,
						szColumn4,
						players[g_iSortedPlayers[ulIdx]].bIsBot,
						players[g_iSortedPlayers[ulIdx]].bReadyToGoOn,
						players[g_iSortedPlayers[ulIdx]].userinfo.lHandicap );

					ulCurYPos += 10;
					ulNumPlayers++;
				}
			}

			if ( ulNumPlayers )
			{
				ulNumPlayers = 0;
				ulCurYPos += 10;
			}
		}

		// Now, draw players who aren't on a team.
		ulNumPlayers = 0;
		for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
		{
			// Skip players that aren't playing.
			if ( playeringame[g_iSortedPlayers[ulIdx]] == false ) 
				continue;

			// Skip spectating players, or players not on a team.
			if (( PLAYER_IsTrueSpectator( &players[g_iSortedPlayers[ulIdx]] )) || ( players[g_iSortedPlayers[ulIdx]].bOnTeam ))
				continue;

			ulColor = ( g_iSortedPlayers[ulIdx] == (int)ulPlayerNum ) ? CR_GREEN : CR_GRAY;

			// Draw the ping, or fragcount, or deathcount.
			if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
				sprintf( szColumn3, "%d", players[g_iSortedPlayers[ulIdx]].ulPing );
			else if ( teamgame || teampossession )
				sprintf( szColumn3, "%d", players[g_iSortedPlayers[ulIdx]].fragcount );
			else if ( teamplay || teamlms )
				sprintf( szColumn3, "%d", players[g_iSortedPlayers[ulIdx]].ulDeathCount );

			// Draw the time.
			sprintf( szColumn4, "%d", ( players[g_iSortedPlayers[ulIdx]].ulTime / ( TICRATE * 60 )));

			scoreboard_RenderIndividualPlayer( bScale,
				ulCurYPos,
				ulColor,
				"NO TEAM",
				players[g_iSortedPlayers[ulIdx]].userinfo.netname,
				szColumn3,
				szColumn4,
				players[g_iSortedPlayers[ulIdx]].bIsBot,
				players[g_iSortedPlayers[ulIdx]].bReadyToGoOn,
				players[g_iSortedPlayers[ulIdx]].userinfo.lHandicap );

			ulCurYPos += 10;
			ulNumPlayers++;
		}

		if ( ulNumPlayers )
		{
			ulNumPlayers = 0;
			ulCurYPos += 10;
		}

		// Finally, draw spectating players.
		for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
		{
			// Skip players that aren't playing.
			if ( playeringame[g_iSortedPlayers[ulIdx]] == false ) 
				continue;

			// Skip spectating players, or players not on a team.
			if ( PLAYER_IsTrueSpectator( &players[g_iSortedPlayers[ulIdx]] ) == false )
				continue;

			ulColor = ( g_iSortedPlayers[ulIdx] == (int)ulPlayerNum ) ? CR_GREEN : CR_GRAY;

			// Draw the ping, or fragcount, or deathcount.
			if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
				sprintf( szColumn3, "%d", players[g_iSortedPlayers[ulIdx]].ulPing );
			else if ( teamgame || teampossession )
				sprintf( szColumn3, "%d", players[g_iSortedPlayers[ulIdx]].fragcount );
			else if ( teamplay || teamlms )
				sprintf( szColumn3, "%d", players[g_iSortedPlayers[ulIdx]].ulDeathCount );

			// Draw the time.
			sprintf( szColumn4, "%d", ( players[g_iSortedPlayers[ulIdx]].ulTime / ( TICRATE * 60 )));

			scoreboard_RenderIndividualPlayer( bScale,
				ulCurYPos,
				ulColor,
				"SPECT",
				players[g_iSortedPlayers[ulIdx]].userinfo.netname,
				szColumn3,
				szColumn4,
				players[g_iSortedPlayers[ulIdx]].bIsBot,
				players[g_iSortedPlayers[ulIdx]].bReadyToGoOn,
				players[g_iSortedPlayers[ulIdx]].userinfo.lHandicap );

			ulCurYPos += 10;
			ulNumPlayers++;
		}
	}	
	// Finally, loop through all the players, and draw their name, fragcount, etc. for non-team
	// modes.
	else 
	{
		// First, draw players who are alive, and not spectating.
		ulNumPlayers = 0;
		for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
		{
			// Skip players not in the game.
			if ( playeringame[g_iSortedPlayers[ulIdx]] == false )
				continue;

			// Skip true spectators.
			if ( PLAYER_IsTrueSpectator( &players[g_iSortedPlayers[ulIdx]] ))
				continue;

			// In LMS, skip dead players.
			if (( gamestate != GS_INTERMISSION ) && 
				( players[g_iSortedPlayers[ulIdx]].health <= 0 ) &&
				(
				(( lastmanstanding ) && (( LASTMANSTANDING_GetState( ) == LMSS_INPROGRESS ) || ( LASTMANSTANDING_GetState( ) == LMSS_WINSEQUENCE ))) ||
				(( survival ) && (( SURVIVAL_GetState( ) == SURVS_INPROGRESS ) || ( SURVIVAL_GetState( ) == SURVS_MISSIONFAILED )))
				)
				)
			{
				continue;
			}

			if ( lastmanstanding )
				sprintf( szColumn1, "%d", players[g_iSortedPlayers[ulIdx]].ulWins );
			else if ( possession || teampossession )
				sprintf( szColumn1, "%d", players[g_iSortedPlayers[ulIdx]].lPointCount );
			else if ( deathmatch )
				sprintf( szColumn1, "%d", players[g_iSortedPlayers[ulIdx]].fragcount );
			else
				sprintf( szColumn1, "%d", players[g_iSortedPlayers[ulIdx]].killcount );

			// Determine letter color.
			if (( terminator ) && ( players[g_iSortedPlayers[ulIdx]].Powers & PW_TERMINATORARTIFACT ))
				ulColor = CR_RED;
			else if ( g_iSortedPlayers[ulIdx] == (int)ulPlayerNum )
				ulColor = demoplayback ? CR_GOLD : CR_GREEN;
			else
				ulColor = CR_GREY;

			if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
				sprintf( szColumn3, "%d", players[g_iSortedPlayers[ulIdx]].ulPing );
			else if ( possession )
				sprintf( szColumn3, "%d", players[g_iSortedPlayers[ulIdx]].fragcount );
			else
				sprintf( szColumn3, "%d", players[g_iSortedPlayers[ulIdx]].ulDeathCount );

			sprintf( szColumn4, "%d", ( players[g_iSortedPlayers[ulIdx]].ulTime / ( TICRATE * 60 )));

			scoreboard_RenderIndividualPlayer( bScale,
				ulCurYPos,
				ulColor,
				szColumn1,
				players[g_iSortedPlayers[ulIdx]].userinfo.netname,
				szColumn3,
				szColumn4,
				players[g_iSortedPlayers[ulIdx]].bIsBot,
				players[g_iSortedPlayers[ulIdx]].bReadyToGoOn,
				players[g_iSortedPlayers[ulIdx]].userinfo.lHandicap );

			ulCurYPos += 10;
			ulNumPlayers++;
		}

		if ( ulNumPlayers )
		{
			ulNumPlayers = 0;
			ulCurYPos += 10;
		}

		// Now, draw players who are dead.
		if (( gamestate != GS_INTERMISSION ) &&
			(
			(( lastmanstanding ) &&  (( LASTMANSTANDING_GetState( ) == LMSS_INPROGRESS ) || ( LASTMANSTANDING_GetState( ) == LMSS_WINSEQUENCE ))) ||
			(( survival ) &&  (( SURVIVAL_GetState( ) == SURVS_INPROGRESS ) || ( SURVIVAL_GetState( ) == SURVS_MISSIONFAILED )))
			)
			)
		{
			for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
			{
				// Skip players not in the game.
				if ( playeringame[g_iSortedPlayers[ulIdx]] == false )
					continue;

				if (( players[g_iSortedPlayers[ulIdx]].health > 0 ) || ( PLAYER_IsTrueSpectator( &players[g_iSortedPlayers[ulIdx]] )))
					continue;

				// Determine letter color.
				if ( g_iSortedPlayers[ulIdx] == (int)ulPlayerNum )
					ulColor = demoplayback ? CR_GOLD : CR_GREEN;
				else
					ulColor = CR_GREY;

				if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
					sprintf( szColumn3, "%d", players[g_iSortedPlayers[ulIdx]].ulPing );
				else
					sprintf( szColumn3, "%d", players[g_iSortedPlayers[ulIdx]].ulDeathCount );

				sprintf( szColumn4, "%d", ( players[g_iSortedPlayers[ulIdx]].ulTime / ( TICRATE * 60 )));

				scoreboard_RenderIndividualPlayer( bScale,
					ulCurYPos,
					ulColor,
					"DEAD",
					players[g_iSortedPlayers[ulIdx]].userinfo.netname,
					szColumn3,
					szColumn4,
					players[g_iSortedPlayers[ulIdx]].bIsBot,
					players[g_iSortedPlayers[ulIdx]].bReadyToGoOn,
					players[g_iSortedPlayers[ulIdx]].userinfo.lHandicap );

				ulCurYPos += 10;
			}
		}

		if ( ulNumPlayers )
		{
			ulNumPlayers = 0;
			ulCurYPos += 10;
		}

		// Finally, draw spectators.
		for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
		{
			// Skip players not in the game.
			if ( playeringame[g_iSortedPlayers[ulIdx]] == false )
				continue;

			if ( PLAYER_IsTrueSpectator( &players[g_iSortedPlayers[ulIdx]] ) == false )
				continue;

			// Determine letter color.
			if (( terminator ) && ( players[g_iSortedPlayers[ulIdx]].Powers & PW_TERMINATORARTIFACT ))
				ulColor = CR_RED;
			else if ( g_iSortedPlayers[ulIdx] == (int)ulPlayerNum )
				ulColor = demoplayback ? CR_GOLD : CR_GREEN;
			else
				ulColor = CR_GREY;

			if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
				sprintf( szColumn3, "%d", players[g_iSortedPlayers[ulIdx]].ulPing );
			else if ( possession )
				sprintf( szColumn3, "%d", players[g_iSortedPlayers[ulIdx]].fragcount );
			else
				sprintf( szColumn3, "%d", players[g_iSortedPlayers[ulIdx]].ulDeathCount );

			sprintf( szColumn4, "%d", ( players[g_iSortedPlayers[ulIdx]].ulTime / ( TICRATE * 60 )));

			scoreboard_RenderIndividualPlayer( bScale,
				ulCurYPos,
				ulColor,
				"SPECT",
				players[g_iSortedPlayers[ulIdx]].userinfo.netname,
				szColumn3,
				szColumn4,
				players[g_iSortedPlayers[ulIdx]].bIsBot,
				players[g_iSortedPlayers[ulIdx]].bReadyToGoOn,
				players[g_iSortedPlayers[ulIdx]].userinfo.lHandicap );

			ulCurYPos += 10;
		}
	}

	BorderNeedRefresh = true;
}

//*****************************************************************************
//
void SCOREBOARD_RenderDMStats( void )
{
	ULONG		ulYPos;
	ULONG		ulTextHeight;
	char		szString[160];
	bool		bScale;
	UCVarValue	ValWidth;
	UCVarValue	ValHeight;
	float		fXScale;
	float		fYScale;
	char		szPatchName[9];
	char		szName[32];
	LONG		lRedScore;
	LONG		lBlueScore;

	// No need to do anything if the automap is active or there's no status bar (we do something different then).
	if (( automapactive ) || 
		(( lastmanstanding ) && ( LASTMANSTANDING_GetState( ) == LMSS_INPROGRESS ) && ( players[consoleplayer].camera->health <= 0 )))
	{
		return;
	}

	ValWidth = con_virtualwidth.GetGenericRep( CVAR_Int );
	ValHeight = con_virtualheight.GetGenericRep( CVAR_Int );
	
	// Initialization.
	if (( con_scaletext ) && ( con_virtualwidth > 0 ) && ( con_virtualheight > 0 ))
	{
		fXScale = (float)con_virtualwidth / SCREENWIDTH;
		fYScale = (float)con_virtualheight / SCREENHEIGHT;
		bScale = true;

		ulTextHeight = Scale( SCREENHEIGHT, SmallFont->GetHeight( ) + 1, con_virtualheight );
	}
	else
	{
		bScale = false;

		ulTextHeight = SmallFont->GetHeight( ) + 1;
	}

	ulYPos = ST_Y - ulTextHeight + 1;

	if ( terminator )
	{
		sprintf( szPatchName, "TERMINAT" );

		if ( bScale )
		{
			screen->DrawTexture( TexMan[szPatchName],
				( ValWidth.Int ) - ( TexMan[szPatchName]->GetWidth( )),
				(LONG)( ulYPos * fYScale ),
				DTA_VirtualWidth, ValWidth.Int,
				DTA_VirtualHeight, ValHeight.Int,
				TAG_DONE );

			sprintf( szString, "\\cC%s \\cG:", g_pTerminatorArtifactCarrier ? g_pTerminatorArtifactCarrier->userinfo.netname : "-" );
			V_ColorizeString( szString );

			screen->DrawText( CR_GRAY,
				( ValWidth.Int ) - ( TexMan[szPatchName]->GetWidth( )) - ( SmallFont->StringWidth( szString )),
				(LONG)( ulYPos * fYScale ),
				szString,
				DTA_VirtualWidth, ValWidth.Int,
				DTA_VirtualHeight, ValHeight.Int,
				TAG_DONE );
		}
		else
		{
			screen->DrawTexture( TexMan[szPatchName],
				( SCREENWIDTH ) - ( TexMan[szPatchName]->GetWidth( )),
				(LONG)ulYPos,
				TAG_DONE );

			sprintf( szString, "\\cC%s \\cG:", g_pTerminatorArtifactCarrier ? g_pTerminatorArtifactCarrier->userinfo.netname : "-" );
			V_ColorizeString( szString );

			screen->DrawText( CR_GRAY,
				( SCREENWIDTH ) - ( TexMan[szPatchName]->GetWidth( )) - ( SmallFont->StringWidth( szString )),
				(LONG)ulYPos,
				szString,
				TAG_DONE );
		}
	}

	if ( possession || teampossession )
	{
		sprintf( szPatchName, "HELLSTON" );

		if ( g_pPossessionArtifactCarrier )
		{
			sprintf( szName, "%s", g_pPossessionArtifactCarrier->userinfo.netname );
			if ( teampossession )
			{
				V_RemoveColorCodes( szName );
				if ( g_pPossessionArtifactCarrier->ulTeam == TEAM_BLUE )
					sprintf( szString, "\\cH%s \\cG:", szName );
				else
					sprintf( szString, "\\cG%s :", szName );
			}
			else
				sprintf( szString, "%s \\cG:", szName );
		}
		else
			sprintf( szString, "\\cC- \\cG:" );

		V_ColorizeString( szString );

		if ( bScale )
		{
			screen->DrawTexture( TexMan[szPatchName],
				( ValWidth.Int ) - ( TexMan[szPatchName]->GetWidth( )),
				(LONG)( ulYPos * fYScale ),
				DTA_VirtualWidth, ValWidth.Int,
				DTA_VirtualHeight, ValHeight.Int,
				TAG_DONE );

			screen->DrawText( CR_GRAY,
				( con_virtualwidth ) - ( TexMan[szPatchName]->GetWidth( )) - ( SmallFont->StringWidth( szString )),
				(LONG)( ulYPos * fYScale ),
				szString,
				DTA_VirtualWidth, ValWidth.Int,
				DTA_VirtualHeight, ValHeight.Int,
				TAG_DONE );
		}
		else
		{
			screen->DrawTexture( TexMan[szPatchName],
				( SCREENWIDTH ) - ( TexMan[szPatchName]->GetWidth( )),
				(LONG)ulYPos,
				TAG_DONE );

			screen->DrawText( CR_GRAY,
				( SCREENWIDTH ) - ( TexMan[szPatchName]->GetWidth( )) - ( SmallFont->StringWidth( szString )),
				(LONG)ulYPos,
				szString,
				TAG_DONE );
		}
	}

	// In fullscreen, don't display anything else. We have another display for that.
	if (( realviewheight == SCREENHEIGHT ) && viewactive )
		return;

	ulYPos = ST_Y - ( ulTextHeight * 2 ) + 1;

	if ( teamplay || teamlms || teampossession )
	{
		if ( teamlms )
		{
			lRedScore = TEAM_GetWinCount( TEAM_RED );
			lBlueScore = TEAM_GetWinCount( TEAM_BLUE );
		}
		else if ( teampossession )
		{
			lRedScore = TEAM_GetScore( TEAM_RED );
			lBlueScore = TEAM_GetScore( TEAM_BLUE );
		}
		else
		{
			lRedScore = TEAM_GetFragCount( TEAM_RED );
			lBlueScore = TEAM_GetFragCount( TEAM_BLUE );
		}

		sprintf( szString , "\\cG%s: \\cC%d", TEAM_GetName( TEAM_RED ), lRedScore );
		V_ColorizeString( szString );

		if ( bScale )
		{
			screen->DrawText( CR_GRAY,
				0,
				(LONG)( ulYPos * fYScale ),
				szString,
				DTA_VirtualWidth, ValWidth.Int,
				DTA_VirtualHeight, ValHeight.Int,
				TAG_DONE );
		}
		else
		{
			screen->DrawText( CR_GRAY,
				0,
				ulYPos,
				szString,
				TAG_DONE );
		}

		ulYPos += ulTextHeight;

		sprintf( szString , "\\cG%s: \\cC%d", TEAM_GetName( TEAM_BLUE ), lBlueScore );
		V_ColorizeString( szString );

		if ( bScale )
		{
			screen->DrawText( CR_GRAY,
				0,
				(LONG)( ulYPos * fYScale ),
				szString,
				DTA_VirtualWidth, ValWidth.Int,
				DTA_VirtualHeight, ValHeight.Int,
				TAG_DONE );
		}
		else
		{
			screen->DrawText( CR_GRAY,
				0,
				ulYPos,
				szString,
				TAG_DONE );
		}
	}
	// Otherwise, render a RANK\SPREAD display.
	else
	{
		sprintf( szString, "\\cGRANK: \\cC%d/%s%d", g_ulRank + 1, g_bIsTied ? "\\cG" : "", g_ulNumPlayers );
		V_ColorizeString( szString );

		if ( bScale )
		{
			screen->DrawText( CR_GRAY,
				0,
				(LONG)( ulYPos * fYScale ),
				szString,
				DTA_VirtualWidth, ValWidth.Int,
				DTA_VirtualHeight, ValHeight.Int,
				TAG_DONE );
		}
		else
		{
			screen->DrawText( CR_GRAY,
				0,
				ulYPos,
				szString,
				TAG_DONE );
		}

		ulYPos += ulTextHeight;

		sprintf( szString, "\\cGSPREAD: \\cC%s%d", g_lSpread > 0 ? "+" : "", g_lSpread );
		V_ColorizeString( szString );

		if ( bScale )
		{
			screen->DrawText( CR_GRAY,
				0,
				(LONG)( ulYPos * fYScale ),
				szString,
				DTA_VirtualWidth, ValWidth.Int,
				DTA_VirtualHeight, ValHeight.Int,
				TAG_DONE );
		}
		else
		{
			screen->DrawText( CR_GRAY,
				0,
				ulYPos,
				szString,
				TAG_DONE );
		}
	}

	if (( duel ) && ( players[consoleplayer].camera->player->ulWins > 0 ))
	{
		sprintf( szString, "\\cGWINS: \\cC%d", players[consoleplayer].camera->player->ulWins );
		V_ColorizeString( szString );

		if ( bScale )
		{
			screen->DrawText( CR_GRAY,
				ValWidth.Int - SmallFont->StringWidth( szString ),
				(LONG)( ulYPos * fYScale ),
				szString,
				DTA_VirtualWidth, ValWidth.Int,
				DTA_VirtualHeight, ValHeight.Int,
				TAG_DONE );
		}
		else
		{
			screen->DrawText( CR_GRAY,
				SCREENWIDTH - SmallFont->StringWidth( szString ),
				ulYPos,
				szString,
				TAG_DONE );
		}
	}

	if ( lastmanstanding )
	{
		if ( players[consoleplayer].camera->player->ulWins > 0 )
		{
			sprintf( szString, "\\cGWINS: \\cC%d", players[consoleplayer].camera->player->ulWins );
			V_ColorizeString( szString );

			if ( bScale )
			{
				screen->DrawText( CR_GRAY,
					ValWidth.Int - SmallFont->StringWidth( szString ),
					(LONG)( ulYPos * fYScale ),
					szString,
					DTA_VirtualWidth, ValWidth.Int,
					DTA_VirtualHeight, ValHeight.Int,
					TAG_DONE );
			}
			else
			{
				screen->DrawText( CR_GRAY,
					SCREENWIDTH - SmallFont->StringWidth( szString ),
					ulYPos,
					szString,
					TAG_DONE );
			}
		}

		if ( LASTMANSTANDING_GetState( ) == LMSS_INPROGRESS )
		{
			g_BottomString += "\\cC";
			g_BottomString.AppendFormat( "%d ", g_lNumOpponentsLeft );
			g_BottomString.AppendFormat( "\\cGOPPONENT%s LEFT", ( g_lNumOpponentsLeft != 1 ) ? "s" : "" );
/*
			sprintf( szString, "\\cC%d \\cGOPPONENT%s LEFT", g_lNumOpponentsLeft, ( g_lNumOpponentsLeft != 1 ) ? "s" : "" );
			V_ColorizeString( szString );

			if ( bScale )
			{
				screen->DrawText( CR_GRAY,
					( ValWidth.Int / 2 ) - ( SmallFont->StringWidth( szString ) / 2 ),
					(LONG)( ulYPos * fYScale ),
					szString,
					DTA_VirtualWidth, ValWidth.Int,
					DTA_VirtualHeight, ValHeight.Int,
					TAG_DONE );
			}
			else
			{
				screen->DrawText( CR_GRAY,
					( SCREENWIDTH / 2 ) - ( SmallFont->StringWidth( szString ) / 2 ),
					ulYPos,
					szString,
					TAG_DONE );
			}
*/
		}
	}
}

//*****************************************************************************
//
void SCOREBOARD_RenderTeamStats( player_s *pPlayer )
{
	ULONG		ulYPos;
	ULONG		ulTextHeight;
	char		szString[160];
	bool		bScale;
	UCVarValue	ValWidth;
	UCVarValue	ValHeight;
	float		fXScale;
	float		fYScale;
	char		szPatchName[9];
	char		szName[32];

	// No need to do anything if the automap is active or there's no status bar (we do something different then).
	if ( automapactive )
		return;

	ValWidth = con_virtualwidth.GetGenericRep( CVAR_Int );
	ValHeight = con_virtualheight.GetGenericRep( CVAR_Int );

	// Initialization.
	if (( con_scaletext ) && ( con_virtualwidth > 0 ) && ( con_virtualheight > 0 ))
	{
		fXScale = (float)con_virtualwidth / SCREENWIDTH;
		fYScale = (float)con_virtualheight / SCREENHEIGHT;
		bScale = true;

		ulTextHeight = Scale( SCREENHEIGHT, SmallFont->GetHeight( ) + 1, con_virtualheight );
	}
	else
	{
		bScale = false;

		ulTextHeight = SmallFont->GetHeight( ) + 1;
	}

	ulYPos = ST_Y - ( ulTextHeight * 2 ) + 1;

	if ( oneflagctf )
	{
		ulYPos += ulTextHeight;

		sprintf( szPatchName, "STFLA3" );

		if ( g_pWhiteCarrier )
		{
			sprintf( szName, "%s", g_pWhiteCarrier->userinfo.netname );
			V_RemoveColorCodes( szName );
			if ( g_pWhiteCarrier->ulTeam == TEAM_BLUE )
				sprintf( szString, "\\cH%s \\cG:", szName );
			else
				sprintf( szString, "\\cG%s :", szName );
		}
		else
			sprintf( szString, "\\cC%s \\cG:", TEAM_GetReturnTicks( NUM_TEAMS ) ? "?" : "-" );

		V_ColorizeString( szString );

		if ( bScale )
		{
			screen->DrawTexture( TexMan[szPatchName],
				( ValWidth.Int ) - ( TexMan[szPatchName]->GetWidth( )),
				(LONG)( ulYPos * fYScale ),
				DTA_VirtualWidth, ValWidth.Int,
				DTA_VirtualHeight, ValHeight.Int,
				TAG_DONE );

			screen->DrawText( CR_GRAY,
				( ValWidth.Int ) - ( TexMan[szPatchName]->GetWidth( )) - ( SmallFont->StringWidth( szString )),
				(LONG)( ulYPos * fYScale ),
				szString,
				DTA_VirtualWidth, ValWidth.Int,
				DTA_VirtualHeight, ValHeight.Int,
				TAG_DONE );
		}
		else
		{
			screen->DrawTexture( TexMan[szPatchName],
				( SCREENWIDTH ) - ( TexMan[szPatchName]->GetWidth( )),
				ulYPos,
				TAG_DONE );

			sprintf( szString, "\\cC%s \\cG:", g_pWhiteCarrier ? g_pWhiteCarrier->userinfo.netname : TEAM_GetReturnTicks( NUM_TEAMS ) ? "?" : "-" );
			V_ColorizeString( szString );

			screen->DrawText( CR_GRAY,
				( SCREENWIDTH ) - ( TexMan[szPatchName]->GetWidth( )) - ( SmallFont->StringWidth( szString )),
				ulYPos,
				szString,
				TAG_DONE );
		}
	}
	else if ( ctf || skulltag )
	{
		if ( ctf )
			sprintf( szPatchName, "STFLA2" );
		else
			sprintf( szPatchName, "STKEYS5" );
		
		if ( bScale )
		{
			screen->DrawTexture( TexMan[szPatchName],
				( ValWidth.Int ) - ( TexMan[szPatchName]->GetWidth( )),
				(LONG)( ulYPos * fYScale ),
				DTA_VirtualWidth, ValWidth.Int,
				DTA_VirtualHeight, ValHeight.Int,
				TAG_DONE );

			sprintf( szString, "\\cC%s \\cG:", g_pRedCarrier ? g_pRedCarrier->userinfo.netname : TEAM_GetReturnTicks( TEAM_RED ) ? "?" : "-" );
			V_ColorizeString( szString );

			screen->DrawText( CR_GRAY,
				( ValWidth.Int ) - ( TexMan[szPatchName]->GetWidth( )) - ( SmallFont->StringWidth( szString )),
				(LONG)( ulYPos * fYScale ),
				szString,
				DTA_VirtualWidth, ValWidth.Int,
				DTA_VirtualHeight, ValHeight.Int,
				TAG_DONE );
		}
		else
		{
			screen->DrawTexture( TexMan[szPatchName],
				( SCREENWIDTH ) - ( TexMan[szPatchName]->GetWidth( )),
				ulYPos,
				TAG_DONE );

			sprintf( szString, "\\cC%s \\cG:", g_pRedCarrier ? g_pRedCarrier->userinfo.netname : TEAM_GetReturnTicks( TEAM_RED ) ? "?" : "-" );
			V_ColorizeString( szString );

			screen->DrawText( CR_GRAY,
				( SCREENWIDTH ) - ( TexMan[szPatchName]->GetWidth( )) - ( SmallFont->StringWidth( szString )),
				ulYPos,
				szString,
				TAG_DONE );
		}

		ulYPos += ulTextHeight;

		if ( ctf )
			sprintf( szPatchName, "STFLA1" );
		else
			sprintf( szPatchName, "STKEYS3" );

		if ( bScale )
		{
			screen->DrawTexture( TexMan[szPatchName],
				( ValWidth.Int ) - ( TexMan[szPatchName]->GetWidth( )),
				(LONG)( ulYPos * fYScale ),
				DTA_VirtualWidth, ValWidth.Int,
				DTA_VirtualHeight, ValHeight.Int,
				TAG_DONE );

			sprintf( szString, "\\cC%s \\cG:", g_pBlueCarrier ? g_pBlueCarrier->userinfo.netname : TEAM_GetReturnTicks( TEAM_BLUE ) ? "?" : "-" );
			V_ColorizeString( szString );

			screen->DrawText( CR_GRAY,
				( ValWidth.Int ) - ( TexMan[szPatchName]->GetWidth( )) - ( SmallFont->StringWidth( szString )),
				(LONG)( ulYPos * fYScale ),
				szString,
				DTA_VirtualWidth, ValWidth.Int,
				DTA_VirtualHeight, ValHeight.Int,
				TAG_DONE );
		}
		else
		{
			screen->DrawTexture( TexMan[szPatchName],
				( SCREENWIDTH ) - ( TexMan[szPatchName]->GetWidth( )),
				ulYPos,
				TAG_DONE );

			sprintf( szString, "\\cC%s \\cG:", g_pBlueCarrier ? g_pBlueCarrier->userinfo.netname : TEAM_GetReturnTicks( TEAM_BLUE ) ? "?" : "-" );
			V_ColorizeString( szString );

			screen->DrawText( CR_GRAY,
				( SCREENWIDTH ) - ( TexMan[szPatchName]->GetWidth( )) - ( SmallFont->StringWidth( szString )),
				ulYPos,
				szString,
				TAG_DONE );
		}
	}

	// Don't display scores or anything if the status bar is minimized. We
	// have a different display for that.
	if (( realviewheight == SCREENHEIGHT ) && viewactive )
		return;

	// Now, draw the blue and red team scores.
	ulYPos = ST_Y - ( ulTextHeight * 2 ) + 1;

	sprintf( szString , "\\cG%s: \\cC%d", TEAM_GetName( TEAM_RED ), TEAM_GetScore( TEAM_RED ));
	V_ColorizeString( szString );

	if ( bScale )
	{
		screen->DrawText( CR_GRAY,
			0,
			(LONG)( ulYPos * fYScale ),
			szString,
			DTA_VirtualWidth, ValWidth.Int,
			DTA_VirtualHeight, ValHeight.Int,
			TAG_DONE );
	}
	else
	{
		screen->DrawText( CR_GRAY,
			0,
			ulYPos,
			szString,
			TAG_DONE );
	}

	ulYPos += ulTextHeight;

	sprintf( szString , "\\cG%s: \\cC%d", TEAM_GetName( TEAM_BLUE ), TEAM_GetScore( TEAM_BLUE ));
	V_ColorizeString( szString );

	if ( bScale )
	{
		screen->DrawText( CR_GRAY,
			0,
			(LONG)( ulYPos * fYScale ),
			szString,
			DTA_VirtualWidth, ValWidth.Int,
			DTA_VirtualHeight, ValHeight.Int,
			TAG_DONE );
	}
	else
	{
		screen->DrawText( CR_GRAY,
			0,
			ulYPos,
			szString,
			TAG_DONE );
	}
}

//*****************************************************************************
//
void SCOREBOARD_RenderInvasionStats( void )
{
	char			szString[128];
	DHUDMessage		*pMsg;

	sprintf( szString, "WAVE: %d  MONSTERS: %d  ARCH-VILES: %d", INVASION_GetCurrentWave( ), INVASION_GetNumMonstersLeft( ), INVASION_GetNumArchVilesLeft( ));
	pMsg = new DHUDMessage( szString, 0.5f, 0.075f, 0, 0, CR_RED, 0.1f );

	StatusBar->AttachMessage( pMsg, 'INVS' );
}

//*****************************************************************************
//
void SCOREBOARD_RenderInVoteClassic( void )
{
	char				szString[128];
	ULONG				ulCurYPos = 0;
	ULONG				ulIdx;
	ULONG				ulNumYes;
	ULONG				ulNumNo;
	ULONG				*pulPlayersWhoVotedYes;
	ULONG				*pulPlayersWhoVotedNo;

	// Start with the "VOTE NOW!" title.
	ulCurYPos = 16;

	screen->SetFont( BigFont );

	// Render the title.
	sprintf( szString, "VOTE NOW!" );
	screen->DrawText( gameinfo.gametype == GAME_Doom ? CR_RED : CR_UNTRANSLATED,
		160 - ( BigFont->StringWidth( szString ) / 2 ),
		ulCurYPos,
		szString,
		DTA_Clean, true, TAG_DONE );

	screen->SetFont( SmallFont );

	// Render who called the vote.
	ulCurYPos += 24;
	sprintf( szString, "Vote called by: %s", players[CALLVOTE_GetVoteCaller( )].userinfo.netname );
	screen->DrawText( CR_UNTRANSLATED,
		160 - ( SmallFont->StringWidth( szString ) / 2 ),
		ulCurYPos,
		szString,
		DTA_Clean, true, TAG_DONE );

	// Render the command being voted on.
	ulCurYPos += 16;
	sprintf( szString, "%s", CALLVOTE_GetCommand( ));
	screen->DrawText( CR_WHITE,
		160 - ( SmallFont->StringWidth( szString ) / 2 ),
		ulCurYPos,
		szString,
		DTA_Clean, true, TAG_DONE );

	// Render how much time is left to vote.
	ulCurYPos += 16;
	sprintf( szString, "Vote ends in: %d", ( CALLVOTE_GetCountdownTicks( ) + TICRATE ) / TICRATE );
	screen->DrawText( CR_RED,
		160 - ( SmallFont->StringWidth( szString ) / 2 ),
		ulCurYPos,
		szString,
		DTA_Clean, true, TAG_DONE );

	pulPlayersWhoVotedYes = CALLVOTE_GetPlayersWhoVotedYes( );
	pulPlayersWhoVotedNo = CALLVOTE_GetPlayersWhoVotedNo( );
	ulNumYes = 0;
	ulNumNo = 0;

	// Count how many players voted for what.
	for ( ulIdx = 0; ulIdx < ( MAXPLAYERS / 2 ) + 1; ulIdx++ )
	{
		if ( pulPlayersWhoVotedYes[ulIdx] != MAXPLAYERS )
			ulNumYes++;

		if ( pulPlayersWhoVotedNo[ulIdx] != MAXPLAYERS )
			ulNumNo++;
	}

	// Display how many have voted for "Yes" and "No".
	ulCurYPos += 16;
	sprintf( szString, "YES: %d", ulNumYes );
	screen->DrawText( CR_UNTRANSLATED,
		32,
		ulCurYPos,
		szString,
		DTA_Clean, true, TAG_DONE );

	sprintf( szString, "NO: %d", ulNumNo );
	screen->DrawText( CR_UNTRANSLATED,
		320 - 32 - SmallFont->StringWidth( szString ),
		ulCurYPos,
		szString,
		DTA_Clean, true, TAG_DONE );

	// Show all the players who have voted, and what they voted for.
	ulCurYPos += 8;
	for ( ulIdx = 0; ulIdx < MAX( ulNumYes, ulNumNo ); ulIdx++ )
	{
		ulCurYPos += 8;
		if ( pulPlayersWhoVotedYes[ulIdx] != MAXPLAYERS )
		{
			sprintf( szString, "%s", players[pulPlayersWhoVotedYes[ulIdx]].userinfo.netname );
			screen->DrawText( CR_UNTRANSLATED,
				32,
				ulCurYPos,
				szString,
				DTA_Clean, true, TAG_DONE );
		}

		if ( pulPlayersWhoVotedNo[ulIdx] != MAXPLAYERS )
		{
			sprintf( szString, "%s", players[pulPlayersWhoVotedNo[ulIdx]].userinfo.netname );
			screen->DrawText( CR_UNTRANSLATED,
				320 - 32 - SmallFont->StringWidth( szString ),
				ulCurYPos,
				szString,
				DTA_Clean, true, TAG_DONE );
		}
	}
}

void SCOREBOARD_RenderInVote( void ) // [RC] New compact version; RenderInVoteClassic is the fullscreen version
{
	char				szString[128];
	ULONG				ulCurYPos = 0;
	ULONG				ulIdx;
	ULONG				ulNumYes;
	ULONG				ulNumNo;
	ULONG				*pulPlayersWhoVotedYes;
	ULONG				*pulPlayersWhoVotedNo;

	// Start at the top of the screen
	ulCurYPos = 8;

	screen->SetFont( BigFont );

	// Render the title and time left.
	sprintf( szString, "VOTE NOW! ( %d )", ( CALLVOTE_GetCountdownTicks( ) + TICRATE ) / TICRATE );
	screen->DrawText( gameinfo.gametype == GAME_Doom ? CR_RED : CR_UNTRANSLATED,
		160 - ( BigFont->StringWidth( szString ) / 2 ),
		ulCurYPos,
		szString,
		DTA_Clean, true, TAG_DONE );

	screen->SetFont( SmallFont );

	// Render the command being voted on.
	ulCurYPos += 14;
	sprintf( szString, "%s", CALLVOTE_GetCommand( ));
	screen->DrawText( CR_WHITE,
		160 - ( SmallFont->StringWidth( szString ) / 2 ),
		ulCurYPos,
		szString,
		DTA_Clean, true, TAG_DONE );


	// Count how many players voted for what.
	pulPlayersWhoVotedYes = CALLVOTE_GetPlayersWhoVotedYes( );
	pulPlayersWhoVotedNo = CALLVOTE_GetPlayersWhoVotedNo( );
	ulNumYes = 0;
	ulNumNo = 0;
	for ( ulIdx = 0; ulIdx < ( MAXPLAYERS / 2 ) + 1; ulIdx++ )
	{
		if ( pulPlayersWhoVotedYes[ulIdx] != MAXPLAYERS )
			ulNumYes++;

		if ( pulPlayersWhoVotedNo[ulIdx] != MAXPLAYERS )
			ulNumNo++;
	}

	// Render the numbers
	ulCurYPos += 8;
	sprintf( szString, "Yes: %d, No: %d",  ulNumYes, ulNumNo);
	screen->DrawText( CR_DARKBROWN,
		160 - ( SmallFont->StringWidth( szString ) / 2 ),
		ulCurYPos,
		szString,
		DTA_Clean, true, TAG_DONE );

	// Render the explanation of keys
	ulCurYPos += 8;
	char	szKeyYes[16];
	char	szKeyNo[16];

	C_FindBind("vote_yes", szKeyYes);
	C_FindBind("vote_no", szKeyNo);
	sprintf( szString, "%s | %s", szKeyYes, szKeyNo);
	screen->DrawText( CR_BLACK,
		160 - ( SmallFont->StringWidth( szString ) / 2 ),
		ulCurYPos,
		szString,
		DTA_Clean, true, TAG_DONE );

}

//*****************************************************************************
//
void SCOREBOARD_RenderDuelCountdown( ULONG ulTimeLeft )
{
	char				szString[128];
	LONG				lDueler1 = -1;
	LONG				lDueler2 = -1;
	ULONG				ulCurYPos = 0;
	ULONG				ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( playeringame[ulIdx] && ( players[ulIdx].bSpectating == false ))
		{
			if ( lDueler1 == -1 )
				lDueler1 = ulIdx;
			else if ( lDueler2 == -1 )
				lDueler2 = ulIdx;
		}
	}

	// This really should not happen, because if we can't find two duelers, we're shouldn't be
	// in the countdown phase.
	if (( lDueler1 == -1 ) || ( lDueler2 == -1 ))
		return;

	// Start by drawing the "RANKINGS" patch 4 pixels from the top. Don't draw it if we're in intermission.
	ulCurYPos = 16;
	if ( gamestate == GS_LEVEL )
	{
		screen->SetFont( BigFont );

		sprintf( szString, "%s", players[lDueler1].userinfo.netname );
		screen->DrawText( gameinfo.gametype == GAME_Doom ? CR_RED : CR_UNTRANSLATED,
			160 - ( BigFont->StringWidth( szString ) / 2 ),
			ulCurYPos,
			szString,
			DTA_Clean, true, TAG_DONE );

		ulCurYPos += 16;
		sprintf( szString, "vs." );
		screen->DrawText( CR_UNTRANSLATED,
			160 - ( BigFont->StringWidth( szString ) / 2 ),
			ulCurYPos,
			szString,
			DTA_Clean, true, TAG_DONE );

		ulCurYPos += 16;
		sprintf( szString, "%s", players[lDueler2].userinfo.netname );
		screen->DrawText( gameinfo.gametype == GAME_Doom ? CR_RED : CR_UNTRANSLATED,
			160 - ( BigFont->StringWidth( szString ) / 2 ),
			ulCurYPos,
			szString,
			DTA_Clean, true, TAG_DONE );

		screen->SetFont( SmallFont );
	}

	ulCurYPos += 24;
	sprintf( szString, "Match begins in: %d", ulTimeLeft / TICRATE );
	screen->DrawText( CR_UNTRANSLATED,
		160 - ( SmallFont->StringWidth( szString ) / 2 ),
		ulCurYPos,
		szString,
		DTA_Clean, true, TAG_DONE );
}

//*****************************************************************************
//
void SCOREBOARD_RenderLMSCountdown( ULONG ulTimeLeft )
{
	char				szString[128];
	ULONG				ulCurYPos = 0;

	if ( lastmanstanding )
	{
		// Start the "LAST MAN STANDING" title.
		ulCurYPos = 32;
		if ( gamestate == GS_LEVEL )
		{
			screen->SetFont( BigFont );

			sprintf( szString, "LAST MAN STANDING" );
			screen->DrawText( gameinfo.gametype == GAME_Doom ? CR_RED : CR_UNTRANSLATED,
				160 - ( BigFont->StringWidth( szString ) / 2 ),
				ulCurYPos,
				szString,
				DTA_Clean, true, TAG_DONE );

			screen->SetFont( SmallFont );
		}
	}
	else if ( teamlms )
	{
		ulCurYPos = 32;
		if ( gamestate == GS_LEVEL )
		{
			screen->SetFont( BigFont );

			sprintf( szString, "TEAM LAST MAN STANDING" );
			screen->DrawText( gameinfo.gametype == GAME_Doom ? CR_RED : CR_UNTRANSLATED,
				160 - ( BigFont->StringWidth( szString ) / 2 ),
				ulCurYPos,
				szString,
				DTA_Clean, true, TAG_DONE );

			screen->SetFont( SmallFont );
		}
	}

	ulCurYPos += 24;
	sprintf( szString, "Match begins in: %d", ulTimeLeft / TICRATE );
	screen->DrawText( CR_UNTRANSLATED,
		160 - ( SmallFont->StringWidth( szString ) / 2 ),
		ulCurYPos,
		szString,
		DTA_Clean, true, TAG_DONE );
}

//*****************************************************************************
//
void SCOREBOARD_RenderPossessionCountdown( const char *pszString, ULONG ulTimeLeft )
{
	char				szString[128];
	ULONG				ulCurYPos = 0;

	if ( possession )
	{
		// Start the "POSSESSION" title.
		ulCurYPos = 32;
		if ( gamestate == GS_LEVEL )
		{
			screen->SetFont( BigFont );

			sprintf( szString, pszString );
			screen->DrawText( gameinfo.gametype == GAME_Doom ? CR_RED : CR_UNTRANSLATED,
				160 - ( BigFont->StringWidth( szString ) / 2 ),
				ulCurYPos,
				szString,
				DTA_Clean, true, TAG_DONE );

			screen->SetFont( SmallFont );
		}
	}
	else if ( teampossession )
	{
		ulCurYPos = 32;
		if ( gamestate == GS_LEVEL )
		{
			screen->SetFont( BigFont );

			sprintf( szString, pszString );
			screen->DrawText( gameinfo.gametype == GAME_Doom ? CR_RED : CR_UNTRANSLATED,
				160 - ( BigFont->StringWidth( szString ) / 2 ),
				ulCurYPos,
				szString,
				DTA_Clean, true, TAG_DONE );

			screen->SetFont( SmallFont );
		}
	}

	ulCurYPos += 24;
	sprintf( szString, "Match begins in: %d", ulTimeLeft / TICRATE );
	screen->DrawText( CR_UNTRANSLATED,
		160 - ( SmallFont->StringWidth( szString ) / 2 ),
		ulCurYPos,
		szString,
		DTA_Clean, true, TAG_DONE );
}

//*****************************************************************************
//
void SCOREBOARD_RenderSurvivalCountdown( ULONG ulTimeLeft )
{
	char				szString[128];
	ULONG				ulCurYPos = 0;

	// Start the "SURVIVAL CO-OP" title.
	ulCurYPos = 32;
	if ( gamestate == GS_LEVEL )
	{
		screen->SetFont( BigFont );

		sprintf( szString, "SURVIVAL CO-OP" );
		screen->DrawText( gameinfo.gametype == GAME_Doom ? CR_RED : CR_UNTRANSLATED,
			160 - ( BigFont->StringWidth( szString ) / 2 ),
			ulCurYPos,
			szString,
			DTA_Clean, true, TAG_DONE );

		screen->SetFont( SmallFont );
	}

	ulCurYPos += 24;
	sprintf( szString, "Match begins in: %d", ulTimeLeft / TICRATE );
	screen->DrawText( CR_UNTRANSLATED,
		160 - ( SmallFont->StringWidth( szString ) / 2 ),
		ulCurYPos,
		szString,
		DTA_Clean, true, TAG_DONE );
}

//*****************************************************************************
//
void SCOREBOARD_RenderInvasionFirstCountdown( ULONG ulTimeLeft )
{
	char				szString[128];
	ULONG				ulCurYPos = 0;

	// Start the "PREPARE FOR INVASION!" title.
	ulCurYPos = 32;
	if ( gamestate == GS_LEVEL )
	{
		screen->SetFont( BigFont );

		sprintf( szString, "PREPARE FOR INVASION!" );
		screen->DrawText( gameinfo.gametype == GAME_Doom ? CR_RED : CR_UNTRANSLATED,
			160 - ( BigFont->StringWidth( szString ) / 2 ),
			ulCurYPos,
			szString,
			DTA_Clean, true, TAG_DONE );

		screen->SetFont( SmallFont );
	}

	ulCurYPos += 24;
	sprintf( szString, "First wave begins in: %d", ulTimeLeft / TICRATE );
	screen->DrawText( CR_UNTRANSLATED,
		160 - ( SmallFont->StringWidth( szString ) / 2 ),
		ulCurYPos,
		szString,
		DTA_Clean, true, TAG_DONE );
}

//*****************************************************************************
//
void SCOREBOARD_RenderInvasionCountdown( ULONG ulTimeLeft )
{
	char				szString[128];
	ULONG				ulCurYPos = 0;

	// Start the title.
	ulCurYPos = 32;
	if ( gamestate == GS_LEVEL )
	{
		screen->SetFont( BigFont );

		// Build the string to use.
		if ((LONG)( INVASION_GetCurrentWave( ) + 1 ) == wavelimit )
			sprintf( szString, "FINAL WAVE!" );
		else
		{
			switch ( INVASION_GetCurrentWave( ) + 1 )
			{
			case 2:

				sprintf( szString, "Second wave" );
				break;
			case 3:

				sprintf( szString, "Third wave" );
				break;
			case 4:

				sprintf( szString, "Fourth wave" );
				break;
			case 5:

				sprintf( szString, "Fifth wave" );
				break;
			case 6:

				sprintf( szString, "Sixth wave" );
				break;
			case 7:

				sprintf( szString, "Seventh wave" );
				break;
			case 8:

				sprintf( szString, "Eighth wave" );
				break;
			case 9:

				sprintf( szString, "Ninth wave" );
				break;
			case 10:

				sprintf( szString, "Tenth wave" );
				break;
			default:

				{
					ULONG	ulWave;
					char	szSuffix[3];

					ulWave = INVASION_GetCurrentWave( ) + 1;

					// xx11-13 are exceptions; they're always "th".
					if ((( ulWave % 100 ) >= 11 ) && (( ulWave % 100 ) <= 13 ))
					{
						sprintf( szSuffix, "th" );
					}
					else
					{
						if (( ulWave % 10 ) == 1 )
							sprintf( szSuffix, "st" );
						else if (( ulWave % 10 ) == 2 )
							sprintf( szSuffix, "nd" );
						else if (( ulWave % 10 ) == 3 )
							sprintf( szSuffix, "rd" );
						else
							sprintf( szSuffix, "th" );
					}

					sprintf( szString, "%d%s wave", ulWave, szSuffix );
				}
				break;
			}
		}

		screen->DrawText( gameinfo.gametype == GAME_Doom ? CR_RED : CR_UNTRANSLATED,
			160 - ( BigFont->StringWidth( szString ) / 2 ),
			ulCurYPos,
			szString,
			DTA_Clean, true, TAG_DONE );

		screen->SetFont( SmallFont );
	}

	ulCurYPos += 24;
	sprintf( szString, "begins in: %d", ulTimeLeft / TICRATE );
	screen->DrawText( CR_UNTRANSLATED,
		160 - ( SmallFont->StringWidth( szString ) / 2 ),
		ulCurYPos,
		szString,
		DTA_Clean, true, TAG_DONE );
}

//*****************************************************************************
//
LONG SCOREBOARD_CalcSpread( ULONG ulPlayerNum )
{
	bool	bInit = true;
	ULONG	ulIdx;
	LONG	lHighestFrags;

	// First, find the highest fragcount that isn't ours.
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( lastmanstanding )
		{
			if (( ulPlayerNum == ulIdx ) || ( playeringame[ulIdx] == false ) || ( PLAYER_IsTrueSpectator( &players[ulIdx] )))
				continue;

			if ( bInit )
			{
				lHighestFrags = players[ulIdx].ulWins;
				bInit = false;
			}

			if ( players[ulIdx].ulWins > (ULONG)lHighestFrags )
				lHighestFrags = players[ulIdx].ulWins;
		}
		else if ( possession )
		{
			if (( ulPlayerNum == ulIdx ) || ( playeringame[ulIdx] == false ) || ( players[ulIdx].bSpectating ))
				continue;

			if ( bInit )
			{
				lHighestFrags = players[ulIdx].lPointCount;
				bInit = false;
			}

			if ( players[ulIdx].lPointCount > lHighestFrags )
				lHighestFrags = players[ulIdx].lPointCount;
		}
		else
		{
			if ( ulPlayerNum == ulIdx || ( playeringame[ulIdx] == false ) || ( players[ulIdx].bSpectating ))
				continue;

			if ( bInit )
			{
				lHighestFrags = players[ulIdx].fragcount;
				bInit = false;
			}

			if ( players[ulIdx].fragcount > lHighestFrags )
				lHighestFrags = players[ulIdx].fragcount;
		}
	}

	// If we're the only person in the game...
	if ( bInit )
	{
		if ( lastmanstanding )
			lHighestFrags = players[ulPlayerNum].ulWins;
		else if ( possession )
			lHighestFrags = players[ulPlayerNum].lPointCount;
		else
			lHighestFrags = players[ulPlayerNum].fragcount;
	}

	// Finally, simply return the difference.
	if ( lastmanstanding )
		return ( players[ulPlayerNum].ulWins - lHighestFrags );
	else if ( possession )
		return ( players[ulPlayerNum].lPointCount - lHighestFrags );
	else
		return ( players[ulPlayerNum].fragcount - lHighestFrags );
}

//*****************************************************************************
//
ULONG SCOREBOARD_CalcRank( ULONG ulPlayerNum )
{
	ULONG	ulIdx;
	ULONG	ulRank;

	ulRank = 0;
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( ulIdx == ulPlayerNum || ( playeringame[ulIdx] == false ) || ( PLAYER_IsTrueSpectator( &players[ulIdx] )))
			continue;

		if ( lastmanstanding )
		{
			if ( players[ulIdx].ulWins > players[ulPlayerNum].ulWins )
				ulRank++;
		}
		else if ( possession )
		{
			if ( players[ulIdx].lPointCount > players[ulPlayerNum].lPointCount )
				ulRank++;
		}
		else
		{
			if ( players[ulIdx].fragcount > players[ulPlayerNum].fragcount )
				ulRank++;
		}
	}

	return ( ulRank );
}

//*****************************************************************************
//
bool SCOREBOARD_IsTied( ULONG ulPlayerNum )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( ulIdx == ulPlayerNum || ( playeringame[ulIdx] == false ) || ( PLAYER_IsTrueSpectator( &players[ulIdx] )))
			continue;

		if ( lastmanstanding )
		{
			if ( players[ulIdx].ulWins == players[ulPlayerNum].ulWins )
				return ( true );
		}
		else if ( possession )
		{
			if ( players[ulIdx].lPointCount == players[ulPlayerNum].lPointCount )
				return ( true );
		}
		else
		{
			if ( players[ulIdx].fragcount == players[ulPlayerNum].fragcount )
				return ( true );
		}
	}

	return ( false );
}

//*****************************************************************************
//
void SCOREBOARD_DisplayFragMessage( player_s *pFraggedPlayer )
{
	char	szString[128];
	DHUDMessageFadeOut	*pMsg;

	sprintf( szString, "You fragged %s\\c-!\n", pFraggedPlayer->userinfo.netname );

	// Print the frag message out in the console.
	Printf( szString );

	V_ColorizeString( szString );

	screen->SetFont( BigFont );
	pMsg = new DHUDMessageFadeOut( szString,
		1.5f,
		0.325f,
		0,
		0,
		CR_RED,
		2.5f,
		0.5f );

	StatusBar->AttachMessage( pMsg, 'FRAG' );
	screen->SetFont( SmallFont );

	if ( teamplay )
	{
		// Build the score message.
		if ( TEAM_GetFragCount( TEAM_RED ) == TEAM_GetFragCount( TEAM_BLUE ))
			sprintf( szString, "Teams are tied at %d", TEAM_GetFragCount( TEAM_RED ));
		else if ( TEAM_GetFragCount( TEAM_RED ) > TEAM_GetFragCount( TEAM_BLUE ))
			sprintf( szString, "\\cG%s\\c- leads %d to %d", TEAM_GetName( TEAM_RED ), TEAM_GetFragCount( TEAM_RED ), TEAM_GetFragCount( TEAM_BLUE ));
		else
			sprintf( szString, "\\cH%s\\c- leads %d to %d", TEAM_GetName( TEAM_BLUE ), TEAM_GetFragCount( TEAM_BLUE ), TEAM_GetFragCount( TEAM_RED ));

		V_ColorizeString( szString );
		pMsg = new DHUDMessageFadeOut( szString,
			1.5f,
			0.375f,
			0,
			0,
			CR_RED,
			2.5f,
			0.5f );

		StatusBar->AttachMessage( pMsg, 'PLAC' );
	}
	else if ( teamgame || teampossession )
	{
		// Build the score message.
		if ( TEAM_GetScore( TEAM_RED ) == TEAM_GetScore( TEAM_BLUE ))
			sprintf( szString, "Teams are tied at %d", TEAM_GetScore( TEAM_RED ));
		else if ( TEAM_GetScore( TEAM_RED ) > TEAM_GetScore( TEAM_BLUE ))
			sprintf( szString, "\\cG%s\\c- leads %d to %d", TEAM_GetName( TEAM_RED ), TEAM_GetScore( TEAM_RED ), TEAM_GetScore( TEAM_BLUE ));
		else
			sprintf( szString, "\\cH%s\\c- leads %d to %d", TEAM_GetName( TEAM_BLUE ), TEAM_GetScore( TEAM_BLUE ), TEAM_GetScore( TEAM_RED ));

		V_ColorizeString( szString );
		pMsg = new DHUDMessageFadeOut( szString,
			1.5f,
			0.375f,
			0,
			0,
			CR_RED,
			2.5f,
			0.5f );

		StatusBar->AttachMessage( pMsg, 'PLAC' );
	}
	else if (( deathmatch ) && ( lastmanstanding == false ) && ( teamlms == false ))
	{
		bool	bIsTied;

		bIsTied	= SCOREBOARD_IsTied( consoleplayer );

		// If the player is tied with someone else, add a "tied for" to their string.
		if ( bIsTied )
			sprintf( szString, "Tied for " );
		else
			sprintf( szString, "" );

		// Determine  what color and number to print for their rank.
		switch ( g_ulRank )
		{
		case 0:

			sprintf( szString, "%s\\cH1st ", szString );
			break;
		case 1:

			sprintf( szString, "%s\\cG2nd ", szString );
			break;
		case 2:

			sprintf( szString, "%s\\cD3rd ", szString );
			break;
		default:

			sprintf( szString, "%s%dth ", szString, ( g_ulRank + 1 ));
			break;
		}

		// Tack on the rest of the string.
		if ( possession || teampossession )
			sprintf( szString, "%s\\c-place with %d point%s", szString, players[consoleplayer].lPointCount, players[consoleplayer].lPointCount == 1 ? "" : "s" );
		else
			sprintf( szString, "%s\\c-place with %d frag%s", szString, players[consoleplayer].fragcount, players[consoleplayer].fragcount == 1 ? "" : "s" );

		V_ColorizeString( szString );
		pMsg = new DHUDMessageFadeOut( szString,
			1.5f,
			0.375f,
			0,
			0,
			CR_RED,
			2.5f,
			0.5f );

		StatusBar->AttachMessage( pMsg, 'PLAC' );
	}
	else if ( lastmanstanding )
	{
		LONG	lMenLeftStanding;

		lMenLeftStanding = LASTMANSTANDING_CountMenStanding( ) - 1;
		sprintf( szString, "%d opponent%s left standing", lMenLeftStanding, ( lMenLeftStanding != 1 ) ? "s" : "" );

		V_ColorizeString( szString );
		pMsg = new DHUDMessageFadeOut( szString,
			1.5f,
			0.375f,
			0,
			0,
			CR_RED,
			2.5f,
			0.5f );

		StatusBar->AttachMessage( pMsg, 'PLAC' );
	}
	else if (( teamlms ) && ( players[consoleplayer].bOnTeam ))
	{
		LONG	lMenLeftStanding;

		lMenLeftStanding = LASTMANSTANDING_TeamCountMenStanding( !players[consoleplayer].ulTeam );
		sprintf( szString, "%d opponent%s left standing", lMenLeftStanding, ( lMenLeftStanding != 1 ) ? "s" : "" );

		V_ColorizeString( szString );
		pMsg = new DHUDMessageFadeOut( szString,
			1.5f,
			0.375f,
			0,
			0,
			CR_RED,
			2.5f,
			0.5f );

		StatusBar->AttachMessage( pMsg, 'PLAC' );
	}
}

//*****************************************************************************
//
void SCOREBOARD_DisplayFraggedMessage( player_s *pFraggingPlayer )
{
	char	szString[128];
	DHUDMessageFadeOut	*pMsg;

	sprintf( szString, "You were fragged by %s\\c-.\n", pFraggingPlayer->userinfo.netname );

	// Print the frag message out in the console.
	Printf( szString );

	V_ColorizeString( szString );

	screen->SetFont( BigFont );
	pMsg = new DHUDMessageFadeOut( szString,
		1.5f,
		0.325f,
		0,
		0,
		CR_RED,
		2.5f,
		0.5f );

	StatusBar->AttachMessage( pMsg, 'FRAG' );
	screen->SetFont( SmallFont );

	if ( teamplay )
	{
		// Build the score message.
		if ( TEAM_GetFragCount( TEAM_RED ) == TEAM_GetFragCount( TEAM_BLUE ))
			sprintf( szString, "Teams are tied at %d", TEAM_GetFragCount( TEAM_RED ));
		else if ( TEAM_GetFragCount( TEAM_RED ) > TEAM_GetFragCount( TEAM_BLUE ))
			sprintf( szString, "\\cG%s\\c- leads %d to %d", TEAM_GetName( TEAM_RED ), TEAM_GetFragCount( TEAM_RED ), TEAM_GetFragCount( TEAM_BLUE ));
		else
			sprintf( szString, "\\cH%s\\c- leads %d to %d", TEAM_GetName( TEAM_BLUE ), TEAM_GetFragCount( TEAM_BLUE ), TEAM_GetFragCount( TEAM_RED ));

		V_ColorizeString( szString );
		pMsg = new DHUDMessageFadeOut( szString,
			1.5f,
			0.375f,
			0,
			0,
			CR_RED,
			2.5f,
			0.5f );

		StatusBar->AttachMessage( pMsg, 'PLAC' );
	}
	else if ( teamgame || teampossession )
	{
		// Build the score message.
		if ( TEAM_GetScore( TEAM_RED ) == TEAM_GetScore( TEAM_BLUE ))
			sprintf( szString, "Teams are tied at %d", TEAM_GetScore( TEAM_RED ));
		else if ( TEAM_GetScore( TEAM_RED ) > TEAM_GetScore( TEAM_BLUE ))
			sprintf( szString, "\\cG%s\\c- leads %d to %d", TEAM_GetName( TEAM_RED ), TEAM_GetScore( TEAM_RED ), TEAM_GetScore( TEAM_BLUE ));
		else
			sprintf( szString, "\\cH%s\\c- leads %d to %d", TEAM_GetName( TEAM_BLUE ), TEAM_GetScore( TEAM_BLUE ), TEAM_GetScore( TEAM_RED ));

		V_ColorizeString( szString );
		pMsg = new DHUDMessageFadeOut( szString,
			1.5f,
			0.375f,
			0,
			0,
			CR_RED,
			2.5f,
			0.5f );

		StatusBar->AttachMessage( pMsg, 'PLAC' );
	}
	else if (( deathmatch ) && ( lastmanstanding == false ) && ( teamlms == false ))
	{
		bool	bIsTied;

		bIsTied	= SCOREBOARD_IsTied( consoleplayer );

		// If the player is tied with someone else, add a "tied for" to their string.
		if ( bIsTied )
			sprintf( szString, "Tied for " );
		else
			sprintf( szString, "" );

		// Determine  what color and number to print for their rank.
		switch ( g_ulRank )
		{
		case 0:

			sprintf( szString, "%s\\cH1st ", szString );
			break;
		case 1:

			sprintf( szString, "%s\\cG2nd ", szString );
			break;
		case 2:

			sprintf( szString, "%s\\cD3rd ", szString );
			break;
		default:

			sprintf( szString, "%s%dth ", szString, ( g_ulRank + 1 ));
			break;
		}

		// Tack on the rest of the string.
		if ( possession || teampossession )
			sprintf( szString, "%s\\c-place with %d point%s", szString, players[consoleplayer].lPointCount, players[consoleplayer].lPointCount == 1 ? "" : "s" );
		else
			sprintf( szString, "%s\\c-place with %d frag%s", szString, players[consoleplayer].fragcount, players[consoleplayer].fragcount == 1 ? "" : "s" );

		V_ColorizeString( szString );
		pMsg = new DHUDMessageFadeOut( szString,
			1.5f,
			0.375f,
			0,
			0,
			CR_RED,
			2.5f,
			0.5f );

		StatusBar->AttachMessage( pMsg, 'PLAC' );
	}
}

//*****************************************************************************
//
void SCOREBOARD_RefreshHUD( void )
{
	ULONG	ulIdx;

	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		return;

	g_ulNumPlayers = 0;
	g_pTerminatorArtifactCarrier = NULL;
	g_pPossessionArtifactCarrier = NULL;
	g_pRedCarrier = NULL;
	g_pBlueCarrier = NULL;
	g_pWhiteCarrier = NULL;
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( lastmanstanding || teamlms )
		{
			if ( playeringame[ulIdx] == false )
				continue;

			if ( PLAYER_IsTrueSpectator( &players[ulIdx] ) == false )
				g_ulNumPlayers++;
		}
		else
		{
			if ( playeringame[ulIdx] && ( players[ulIdx].bSpectating == false ))
			{
				g_ulNumPlayers++;

				if ( players[ulIdx].Powers & PW_TERMINATORARTIFACT )
					g_pTerminatorArtifactCarrier = &players[ulIdx];
				else if ( players[ulIdx].Powers & PW_POSSESSIONARTIFACT )
					g_pPossessionArtifactCarrier = &players[ulIdx];
				else if ( players[ulIdx].mo )
				{
					if ( players[ulIdx].mo->FindInventory( TEAM_GetFlagItem( TEAM_BLUE )))
						g_pBlueCarrier = &players[ulIdx];
					else if ( players[ulIdx].mo->FindInventory( TEAM_GetFlagItem( TEAM_RED )))
						g_pRedCarrier = &players[ulIdx];
					else if ( players[ulIdx].mo->FindInventory( PClass::FindClass( "WhiteFlag" )))
						g_pWhiteCarrier = &players[ulIdx];
				}
			}
		}
	}

	if ( players[consoleplayer].camera && players[consoleplayer].camera->player )
	{
		g_ulRank = SCOREBOARD_CalcRank( ULONG( players[consoleplayer].camera->player - players ));
		g_lSpread = SCOREBOARD_CalcSpread( ULONG( players[consoleplayer].camera->player - players ));
		g_bIsTied = SCOREBOARD_IsTied( ULONG( players[consoleplayer].camera->player - players ));
		g_lNumOpponentsLeft = LASTMANSTANDING_CountMenStanding( ) - 1;
	}
}

//*****************************************************************************
//
ULONG SCOREBOARD_GetNumPlayers( void )
{
	return ( g_ulNumPlayers );
}

//*****************************************************************************
//
ULONG SCOREBOARD_GetRank( void )
{
	return ( g_ulRank );
}

//*****************************************************************************
//
LONG SCOREBOARD_GetSpread( void )
{
	return ( g_lSpread );
}

//*****************************************************************************
//
bool SCOREBOARD_IsTied( void )
{
	return ( g_bIsTied );
}

//*****************************************************************************
//*****************************************************************************
//
static void scoreboard_SortPlayers( ULONG ulSortType )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
		g_iSortedPlayers[ulIdx] = ulIdx;

	if ( ulSortType == ST_FRAGCOUNT )
		qsort( g_iSortedPlayers, MAXPLAYERS, sizeof( int ), scoreboard_FragCompareFunc );
	else if ( ulSortType == ST_POINTCOUNT )
		qsort( g_iSortedPlayers, MAXPLAYERS, sizeof( int ), scoreboard_PointsCompareFunc );
	else if ( ulSortType == ST_WINCOUNT )
		qsort( g_iSortedPlayers, MAXPLAYERS, sizeof( int ), scoreboard_WinsCompareFunc );
	else
		qsort( g_iSortedPlayers, MAXPLAYERS, sizeof( int ), scoreboard_KillsCompareFunc );
}

//*****************************************************************************
//
static int STACK_ARGS scoreboard_FragCompareFunc( const void *arg1, const void *arg2 )
{
	return ( players[*(int *)arg2].fragcount - players[*(int *)arg1].fragcount );
}

//*****************************************************************************
//
static int STACK_ARGS scoreboard_PointsCompareFunc( const void *arg1, const void *arg2 )
{
	return ( players[*(int *)arg2].lPointCount - players[*(int *)arg1].lPointCount );
}

//*****************************************************************************
//
static int STACK_ARGS scoreboard_KillsCompareFunc( const void *arg1, const void *arg2 )
{
	return ( players[*(int *)arg2].killcount - players[*(int *)arg1].killcount );
}

//*****************************************************************************
//
static int STACK_ARGS scoreboard_WinsCompareFunc( const void *arg1, const void *arg2 )
{
	return ( players[*(int *)arg2].ulWins - players[*(int *)arg1].ulWins );
}

//*****************************************************************************
//
static void scoreboard_RenderIndividualPlayer( bool bScale, ULONG ulYPos, ULONG ulColor, char *pszColumn1, char *pszColumn2, char *pszColumn3, char *pszColumn4, bool bIsBot, bool bReadyToGoOn, ULONG ulHandicap )
{
	char		szPatchName[9];
	UCVarValue	ValWidth;
	UCVarValue	ValHeight;
	float		fXScale;
	float		fYScale;

	ValWidth = con_virtualwidth.GetGenericRep( CVAR_Int );
	ValHeight = con_virtualheight.GetGenericRep( CVAR_Int );

	fXScale =  (float)ValWidth.Int / 320.0f;
	fYScale =  (float)ValHeight.Int / 200.0f;

	if ( bReadyToGoOn )
	{
		sprintf( szPatchName, "RDYTOGO" );
		if ( bScale )
		{
			screen->DrawTexture( TexMan[szPatchName],
				(LONG)( COLUMN1_XPOS * fXScale ) - SmallFont->StringWidth("  ") - TexMan[szPatchName]->GetWidth( ),
				ulYPos - (( TexMan[szPatchName]->GetHeight( ) - SmallFont->GetHeight( )) / 2 ),
				DTA_VirtualWidth, ValWidth.Int,
				DTA_VirtualHeight, ValHeight.Int,
				TAG_DONE );
		}
		else
		{
			screen->DrawTexture( TexMan[szPatchName],
				(LONG)( COLUMN1_XPOS * CleanXfac ) - SmallFont->StringWidth("  ") - TexMan[szPatchName]->GetWidth( ),
				ulYPos - (( TexMan[szPatchName]->GetHeight( ) - SmallFont->GetHeight( )) / 2 ),
				TAG_DONE );
		}
	}

	if ( bScale )
	{
		screen->DrawText( ulColor,
			(LONG)( COLUMN1_XPOS * fXScale ),
			ulYPos,
			pszColumn1,
			DTA_VirtualWidth, ValWidth.Int,
			DTA_VirtualHeight, ValHeight.Int,
			TAG_DONE );
	}
	else
	{
		screen->DrawText( ulColor,
			(LONG)( COLUMN1_XPOS * CleanXfac ),
			ulYPos,
			pszColumn1,
			TAG_DONE );
	}

	if ( bScale )
	{
		screen->DrawText( ulColor,
			(LONG)( COLUMN2_XPOS * fXScale ),
			ulYPos,
			pszColumn2,
			DTA_VirtualWidth, ValWidth.Int,
			DTA_VirtualHeight, ValHeight.Int,
			TAG_DONE );
	}
	else
	{
		screen->DrawText( ulColor,
			(LONG)( COLUMN2_XPOS * CleanXfac ),
			ulYPos,
			pszColumn2,
			TAG_DONE );
	}

	if ( ulHandicap > 0 )
	{
		char	szHandicapString[8];

		if ( lastmanstanding || teamlms )
		{
			if (( deh.MaxSoulsphere - (LONG)ulHandicap ) < 1 )
				sprintf( szHandicapString, "(1)" );
			else
				sprintf( szHandicapString, "(%d)", deh.MaxArmor - (LONG)ulHandicap );
		}
		else
		{
			if (( deh.StartHealth - (LONG)ulHandicap ) < 1 )
				sprintf( szHandicapString, "(1)" );
			else
				sprintf( szHandicapString, "(%d)", deh.StartHealth - (LONG)ulHandicap );
		}
		
		if ( bScale )
		{
			screen->DrawText( ulColor,
				(LONG)( COLUMN2_XPOS * fXScale ) - SmallFont->StringWidth (" ") - SmallFont->StringWidth( szHandicapString ),
				ulYPos,
				szHandicapString,
				DTA_VirtualWidth, ValWidth.Int,
				DTA_VirtualHeight, ValHeight.Int,
				TAG_DONE );
		}
		else
		{
			screen->DrawText( ulColor,
				(LONG)( COLUMN2_XPOS * CleanXfac ) - SmallFont->StringWidth (" ") - SmallFont->StringWidth( szHandicapString ),
				ulYPos,
				szHandicapString,
				DTA_Clean,
				bScale,
				TAG_DONE );
		}
	}

	// Draw a little skull to the left of their name if this is a bot.
	if ( bIsBot )
	{
		sprintf( szPatchName, "BOTSKIL%d", botskill.GetGenericRep( CVAR_Int ));

		if ( bScale )
		{
			screen->DrawTexture( TexMan[szPatchName],
				(LONG)( COLUMN1_XPOS * fXScale ) - SmallFont->StringWidth("  ") - TexMan[szPatchName]->GetWidth( ),
				ulYPos,
				DTA_VirtualWidth, ValWidth.Int,
				DTA_VirtualHeight, ValHeight.Int,
				TAG_DONE );
		}
		else
		{
			screen->DrawTexture( TexMan[szPatchName],
				(LONG)( COLUMN1_XPOS * CleanXfac ) - SmallFont->StringWidth("  ") - TexMan[szPatchName]->GetWidth( ),
				ulYPos,
				DTA_Clean,
				bScale,
				TAG_DONE );
		}
	}

	if ( bScale )
	{
		screen->DrawText( ulColor,
			(LONG)( COLUMN3_XPOS * fXScale ),
			ulYPos,
			pszColumn3,
			DTA_VirtualWidth, ValWidth.Int,
			DTA_VirtualHeight, ValHeight.Int,
			TAG_DONE );
	}
	else
	{
		screen->DrawText( ulColor,
			(LONG)( COLUMN3_XPOS * CleanXfac ),
			ulYPos,
			pszColumn3,
			TAG_DONE );
	}
	
	if ( bScale )
	{
		screen->DrawText( ulColor,
			(LONG)( COLUMN4_XPOS * fXScale ),
			ulYPos,
			pszColumn4,
			DTA_VirtualWidth, ValWidth.Int,
			DTA_VirtualHeight, ValHeight.Int,
			TAG_DONE );
	}
	else
	{
		screen->DrawText( ulColor,
			(LONG)( COLUMN4_XPOS * CleanXfac ),
			ulYPos,
			pszColumn4,
			TAG_DONE );
	}
}

//*****************************************************************************
//	CONSOLE VARIABLES

CVAR( Bool, cl_alwaysdrawdmstats, true, CVAR_ARCHIVE )
CVAR( Bool, cl_alwaysdrawteamstats, true, CVAR_ARCHIVE )
