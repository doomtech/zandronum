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
#include "g_game.h"
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
#include "c_bind.h"	// [RC] To tell user what key to press to vote.

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

// [RC] How many allies are alive in Survival, or Team LMS?
static	LONG	g_lNumAlliesLeft = 0;

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

// Current position of our "pen".
static	ULONG		g_ulCurYPos;

// Is text scaling enabled?
static	bool		g_bScale;

// What are the virtual dimensions of our screen?
static	UCVarValue	g_ValWidth;
static	UCVarValue	g_ValHeight;

// How much bigger is the virtual screen than the base 320x200 screen?
static	float		g_fXScale;
static	float		g_fYScale;

// How many columns are we using in our scoreboard display?
static	ULONG		g_ulNumColumnsUsed = 0;

// Array that has the type of each column.
static	ULONG		g_aulColumnType[MAX_COLUMNS];

// X position of each column.
static	ULONG		g_aulColumnX[MAX_COLUMNS];

// What font are the column headers using?
static	FFont		*g_pColumnHeaderFont = NULL;

// This is the header for each column type.
static	const char	*g_pszColumnHeaders[NUM_COLUMN_TYPES] =
{
	"",
	"NAME",
	"TIME",
	"PING",
	"FRAGS",
	"POINTS",
	"DEATHS",
	"WINS",
	"KILLS",
	"POINTSASSISTS",
	"SECRETS",
	"MEDALS",
};

//*****************************************************************************
//	PROTOTYPES

static	void			scoreboard_SortPlayers( ULONG ulSortType );
static	int	STACK_ARGS	scoreboard_FragCompareFunc( const void *arg1, const void *arg2 );
static	int	STACK_ARGS	scoreboard_PointsCompareFunc( const void *arg1, const void *arg2 );
static	int	STACK_ARGS	scoreboard_KillsCompareFunc( const void *arg1, const void *arg2 );
static	int	STACK_ARGS	scoreboard_WinsCompareFunc( const void *arg1, const void *arg2 );
static	void			scoreboard_RenderIndividualPlayer( ULONG ulPlayer );
static	void			scoreboard_DrawHeader( void );
static	void			scoreboard_DrawLimits( void );
static	void			scoreboard_DrawTeamScores( ULONG ulPlayer );
static	void			scoreboard_DrawMyRank( ULONG ulPlayer );
static	void			scoreboard_ClearColumns( void );
static	void			scoreboard_Prepare5ColumnDisplay( void );
static	void			scoreboard_Prepare4ColumnDisplay( void );
static	void			scoreboard_Prepare3ColumnDisplay( void );
static	void			scoreboard_DoRankingListPass( LONG lSpectators, LONG lDead, LONG lNotPlaying, LONG lNoTeam, LONG lWrongTeam, ULONG ulDesiredTeam );
static	void			scoreboard_DrawRankings( void );

//*****************************************************************************
//	FUNCTIONS

//*****************************************************************************
// Renders some HUD strings, and the main board if the player is pushing the keys.
void SCOREBOARD_Render( player_s *pPlayer )
{
	DHUDMessageFadeOut	*pMsg;
	LONG				lPosition;

	g_ValWidth = con_virtualwidth.GetGenericRep( CVAR_Int );
	g_ValHeight = con_virtualheight.GetGenericRep( CVAR_Int );

	if (( con_scaletext ) && ( con_virtualwidth > 0 ) && ( con_virtualheight > 0 ))
	{
		g_fXScale =  (float)g_ValWidth.Int / 320.0f;
		g_fYScale =  (float)g_ValHeight.Int / 200.0f;
		g_bScale = true;
	}
	else
		g_bScale = false;

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
	}

	if ( CALLVOTE_GetVoteState( ) != VOTESTATE_NOVOTE )
	{
		switch ( CALLVOTE_GetVoteState( ))
		{
		case VOTESTATE_INVOTE:
			
			// [RC] Display either the fullscreen or minimized vote screen.
			if ( cl_showfullscreenvote )
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
	
	// [RC] In Survival, print how many other players are alive
	if ( SURVIVAL_GetState( ) == SURVS_INPROGRESS )
		{
			if(g_lNumAlliesLeft < 1)
				g_BottomString += "\\cgLAST PLAYER ALIVE"; // Uh-oh.
			else {
				g_BottomString.AppendFormat( "\\cc%d ", g_lNumAlliesLeft );
				g_BottomString.AppendFormat( "\\cGALL%s LEFT", ( g_lNumAlliesLeft != 1 ) ? "IES" : "Y" );
			}
		}

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
//*****************************************************************************
//
void SCOREBOARD_RenderBoard( player_s *pPlayer )
{
	ULONG	ulPlayerNum;
	ULONG	ulNumIdealColumns;

	// Calculate the playernum of the given player.
	ulPlayerNum = ULONG( pPlayer - players );

	// Draw the "RANKINGS" text at the top.
	scoreboard_DrawHeader( );
	
	// Draw the time, frags, points, or kills we have left until the level ends.
	if ( gamestate == GS_LEVEL )
		scoreboard_DrawLimits( );

	// Draw the team scores and their relation (tied, red leads, etc).
	scoreboard_DrawTeamScores( ulPlayerNum );
	
	// Draw my rank and my frags, points, etc.
	if ( gamestate == GS_LEVEL )
		scoreboard_DrawMyRank( ulPlayerNum );
	
	// Draw the player list and its data.
	// First, determine how many columns we can use, based on our screen resolution.
	ulNumIdealColumns = 3;
	if ( g_bScale )
	{
		if ( g_ValWidth.Int >= 480 )
			ulNumIdealColumns = 4;
		if ( g_ValWidth.Int >= 800 )
			ulNumIdealColumns = 5;
	}
	else
	{
		if ( SCREENWIDTH >= 480 )
			ulNumIdealColumns = 4;
		if ( SCREENWIDTH >= 800 )
			ulNumIdealColumns = 5;
	}

	if (( ulNumIdealColumns == 5 ) && !( teamgame || possession || teampossession || ctf || skulltag || teamlms || lastmanstanding ))
		ulNumIdealColumns = 4;

	if ( ulNumIdealColumns == 5 )
		scoreboard_Prepare5ColumnDisplay( );
	else if( ulNumIdealColumns == 4 )
		scoreboard_Prepare4ColumnDisplay( );
	else
		scoreboard_Prepare3ColumnDisplay( );

	// Draw the headers, list, entries, everything.
	scoreboard_DrawRankings( );
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

	if ( LASTMANSTANDING_GetState( ) == LMSS_INPROGRESS )
		{
			if( teamlms ) {
				g_BottomString += "\\cC";
				g_BottomString.AppendFormat( "%d ", g_lNumOpponentsLeft );
				g_BottomString.AppendFormat( "\\cGOPPONENT%s", ( g_lNumOpponentsLeft != 1 ) ? "s" : "" );
				g_BottomString += "\\cC";
				if(g_lNumAlliesLeft > 0) {
					g_BottomString.AppendFormat( ", %d ", g_lNumAlliesLeft );
					g_BottomString.AppendFormat( "\\cGALL%s LEFT ", ( g_lNumAlliesLeft != 1 ) ? "ies" : "y" );
				}
				else
					g_BottomString += "\\cG LEFT - ALLIES DEAD";


			}
			else {
				g_BottomString += "\\cC";
				g_BottomString.AppendFormat( "%d ", g_lNumOpponentsLeft );
				g_BottomString.AppendFormat( "\\cGOPPONENT%s LEFT", ( g_lNumOpponentsLeft != 1 ) ? "s" : "" );
			}
		}

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
EXTERN_CVAR( Bool, cl_stfullscreenhud );
void SCOREBOARD_RenderInvasionStats( void )
{
	// [RC] In fullscreen, don't render these as they're done with the other fullscreen elements in doom_sbar.
	if (( realviewheight == SCREENHEIGHT ) && viewactive && cl_stfullscreenhud)
		return;

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

//*****************************************************************************
// [RC] New compact version; RenderInVoteClassic is the fullscreen version
void SCOREBOARD_RenderInVote( void )
{
	char				szString[128];
	char				szKeyYes[16];
	char				szKeyNo[16];
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

	// Get how many players voted for what.
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

	// Render the numbers.
	ulCurYPos += 8;
	sprintf( szString, "Yes: %d, No: %d",  ulNumYes, ulNumNo );
	screen->DrawText( CR_DARKBROWN,
		160 - ( SmallFont->StringWidth( szString ) / 2 ),
		ulCurYPos,
		szString,
		DTA_Clean, true, TAG_DONE );

	// Render the explanation of keys.
	ulCurYPos += 8;

	C_FindBind( "vote_yes", szKeyYes );
	C_FindBind( "vote_no", szKeyNo );
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

		lMenLeftStanding = GAME_CountLivingPlayers( ) - 1;
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

		//  "x opponents left", "x allies alive", etc
		if ( lastmanstanding )
			g_lNumOpponentsLeft = GAME_CountLivingPlayers( ) - 1;

		if ( teamlms )
		{
			g_lNumOpponentsLeft = LASTMANSTANDING_TeamCountEnemiesStanding( players[consoleplayer].ulTeam );
			g_lNumAlliesLeft = LASTMANSTANDING_TeamCountMenStanding( players[consoleplayer].ulTeam) - 1;
		}

		if ( survival )
			g_lNumAlliesLeft = GAME_CountLivingPlayers( ) - 1;
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
LONG SCOREBOARD_GetLeftToLimit( void )
{
	ULONG	ulIdx;

	// If we're not in a level, then clearly there's no need for this.
	if ( gamestate != GS_LEVEL )
		return ( 0 );

	// FRAG-based mode.
	if (( lastmanstanding == false ) && ( teamlms == false ) && ( possession == false ) && ( teampossession == false ) && deathmatch && ( fraglimit > 0 ))
	{
		LONG	lHighestFragcount;
		
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

		return ( fraglimit - lHighestFragcount );
	}
	// POINT-based mode.
	else if (( teamgame || possession || teampossession ) && ( pointlimit > 0 ))
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
		return ulPointsLeft;
	}
	// KILL-based mode.
	else if (( deathmatch == false ) && ( teamgame == false ))
	{
		if ( invasion )
			return (LONG)INVASION_GetNumMonstersLeft( );
		else
			return ( level.total_monsters - level.killed_monsters );
	}
	// WIN-based mode (LMS).
	else if (( lastmanstanding || teamlms ) && ( winlimit > 0 ))
	{
		bool	bFoundPlayer = false;
		LONG	lHighestWincount;

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

		return ( winlimit - lHighestWincount );
	}

	// None of the above.
	return ( -1 );
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
static void scoreboard_RenderIndividualPlayer( ULONG ulPlayer )
{
	ULONG	ulIdx;
	ULONG	ulColor;
	char	szPatchName[9];
	char	szString[64];

	// Draw the data for each column.
	for( ulIdx = 0; ulIdx < g_ulNumColumnsUsed; ulIdx++ )
	{
		szString[0] = 0;

		ulColor = CR_GRAY;
		if (( terminator ) && ( players[ulIdx].Powers & PW_TERMINATORARTIFACT ))
			ulColor = CR_RED;

		if ( players[ulPlayer].bOnTeam == true )
			ulColor = TEAM_GetTextColor( players[ulPlayer].ulTeam );
		else if ( ulIdx == ulPlayer )
			ulColor = demoplayback ? CR_GOLD : CR_GREEN;

		// Determine what needs to be displayed in this column.
		switch ( g_aulColumnType[ulIdx] )
		{
			case COLUMN_NAME:

				sprintf( szString, "%s", players[ulPlayer].userinfo.netname );

				// Draw a chat icon if this player is chatting.
				if ( players[ulPlayer].bChatting )
				{
					sprintf( szPatchName, "TLKMINI");

					if ( g_bScale )
					{
						screen->DrawTexture( TexMan[szPatchName],
							(LONG)(  g_aulColumnX[ulIdx] * g_fXScale ) - SmallFont->StringWidth("  ") - TexMan[szPatchName]->GetWidth( ),
							g_ulCurYPos - 1,
							DTA_VirtualWidth, g_ValWidth.Int,
							DTA_VirtualHeight, g_ValHeight.Int,
							TAG_DONE );
					}
					else
					{
						screen->DrawTexture( TexMan[szPatchName],
							(LONG)( g_aulColumnX[ulIdx] * CleanXfac ) - SmallFont->StringWidth("  ") - TexMan[szPatchName]->GetWidth( ),
							g_ulCurYPos - 1,
							DTA_Clean,
							g_bScale,
							TAG_DONE );
					}
				}
				// Draw a bot icon if this player is a bot.
				else if ( players[ulPlayer].bIsBot )
				{
					sprintf( szPatchName, "BOTSKIL%d", botskill.GetGenericRep( CVAR_Int ));

					if ( g_bScale )
					{
						screen->DrawTexture( TexMan[szPatchName],
							(LONG)(  g_aulColumnX[ulIdx] * g_fXScale ) - SmallFont->StringWidth("  ") - TexMan[szPatchName]->GetWidth( ),
							g_ulCurYPos,
							DTA_VirtualWidth, g_ValWidth.Int,
							DTA_VirtualHeight, g_ValHeight.Int,
							TAG_DONE );
					}
					else
					{
						screen->DrawTexture( TexMan[szPatchName],
							(LONG)( g_aulColumnX[ulIdx] * CleanXfac ) - SmallFont->StringWidth("  ") - TexMan[szPatchName]->GetWidth( ),
							g_ulCurYPos,
							DTA_Clean,
							g_bScale,
							TAG_DONE );
					}
				}
				// Draw an icon if this player is a ready to go on.
				else if ( players[ulPlayer].bReadyToGoOn )
				{
					sprintf( szPatchName, "RDYTOGO" );
					if ( g_bScale )
					{
						screen->DrawTexture( TexMan[szPatchName],
							(LONG)( g_aulColumnX[ulIdx] * g_fXScale ) - SmallFont->StringWidth("  ") - TexMan[szPatchName]->GetWidth( ),
							g_ulCurYPos - (( TexMan[szPatchName]->GetHeight( ) - SmallFont->GetHeight( )) / 2 ),
							DTA_VirtualWidth, g_ValWidth.Int,
							DTA_VirtualHeight, g_ValHeight.Int,
							TAG_DONE );
					}
					else
					{
						screen->DrawTexture( TexMan[szPatchName],
							(LONG)( g_aulColumnX[ulIdx] * CleanXfac ) - SmallFont->StringWidth("  ") - TexMan[szPatchName]->GetWidth( ),
							g_ulCurYPos - (( TexMan[szPatchName]->GetHeight( ) - SmallFont->GetHeight( )) / 2 ),
							TAG_DONE );
					}
				}

				// Handicap text
				if ( players[ulPlayer].userinfo.lHandicap > 0 )
				{
					char	szHandicapString[8];

					if ( lastmanstanding || teamlms )
					{
						if (( deh.MaxSoulsphere - (LONG)players[ulPlayer].userinfo.lHandicap ) < 1 )
							sprintf( szHandicapString, "(1)" );
						else
							sprintf( szHandicapString, "(%d)", deh.MaxArmor - (LONG)players[ulPlayer].userinfo.lHandicap );
					}
					else
					{
						if (( deh.StartHealth - (LONG)players[ulPlayer].userinfo.lHandicap ) < 1 )
							sprintf( szHandicapString, "(1)" );
						else
							sprintf( szHandicapString, "(%d)", deh.StartHealth - (LONG)players[ulPlayer].userinfo.lHandicap );
					}
					
					if ( g_bScale )
					{
						screen->DrawText( ulColor,
							(LONG)( g_aulColumnX[ulIdx] * g_fXScale ) - SmallFont->StringWidth (" ") - SmallFont->StringWidth( szHandicapString ),
							g_ulCurYPos,
							szHandicapString,
							DTA_VirtualWidth, g_ValWidth.Int,
							DTA_VirtualHeight, g_ValHeight.Int,
							TAG_DONE );
					}
					else
					{
						screen->DrawText( ulColor,
							(LONG)( g_aulColumnX[ulIdx] * CleanXfac ) - SmallFont->StringWidth (" ") - SmallFont->StringWidth( szHandicapString ),
							g_ulCurYPos,
							szHandicapString,
							DTA_Clean,
							g_bScale,
							TAG_DONE );
					}
				}
				break;
			case COLUMN_TIME:	

				sprintf( szString, "%d", players[ulPlayer].ulTime / ( TICRATE * 60 ));
				break;
			case COLUMN_PING:

				sprintf( szString, "%d", players[ulPlayer].ulPing );
				break;
			case COLUMN_FRAGS:

				sprintf( szString, "%d", players[ulPlayer].fragcount );

				// If the player isn't really playing, change this.
				if(!players[ulPlayer].bOnTeam && teamplay)
					sprintf(szString, "NO TEAM");
				if(PLAYER_IsTrueSpectator( &players[ulPlayer] ))
					sprintf(szString, "SPECT");
				break;
			case COLUMN_POINTS:

				sprintf( szString, "%d", players[ulPlayer].lPointCount );
				
				// If the player isn't really playing, change this.
				if(!players[ulPlayer].bOnTeam)
					sprintf(szString, "NO TEAM");
				if(PLAYER_IsTrueSpectator( &players[ulPlayer] ))
					sprintf(szString, "SPECT");
				break;

			case COLUMN_POINTSASSISTS:
				sprintf(szString, "%d / %d", players[ulPlayer].lPointCount, players[ulPlayer].ulMedalCount[14]);

				// If the player isn't really playing, change this.
				if(!players[ulPlayer].bOnTeam)
					sprintf(szString, "NO TEAM");
				if(PLAYER_IsTrueSpectator( &players[ulPlayer] ))
					sprintf(szString, "SPECT");
				break;

			case COLUMN_DEATHS:
				sprintf(szString, "%d", players[ulPlayer].ulDeathCount);
				break;

			case COLUMN_WINS:
				sprintf(szString, "%d", players[ulPlayer].ulWins);
					// If the player isn't really playing, change this.
					if(!players[ulPlayer].bOnTeam && teamlms)
						sprintf(szString, "NO TEAM");
					if(PLAYER_IsTrueSpectator( &players[ulPlayer] ))
						sprintf(szString, "SPECT");
					if((players[ulPlayer].health <= 0) && (gamestate != GS_INTERMISSION))
						sprintf(szString, "DEAD");
				break;

			case COLUMN_KILLS:
				sprintf(szString, "%d", players[ulPlayer].killcount);
					// If the player isn't really playing, change this.
					if(PLAYER_IsTrueSpectator( &players[ulPlayer] ))
						sprintf(szString, "SPECT");
					if((players[ulPlayer].health <= 0) && (gamestate != GS_INTERMISSION))
						sprintf(szString, "DEAD");
				break;
			case COLUMN_SECRETS:
				sprintf(szString, "%d", players[ulPlayer].secretcount);
					// If the player isn't really playing, change this.
					if(PLAYER_IsTrueSpectator( &players[ulPlayer] ))
						sprintf(szString, "SPECT");
					if((players[ulPlayer].health <= 0) && (gamestate != GS_INTERMISSION))
						sprintf(szString, "DEAD");
				break;
				
		}

		if ( szString[0] != 0 )
		{
			if ( g_bScale )
			{
				screen->DrawText( ulColor,
						(LONG)( g_aulColumnX[ulIdx] * g_fXScale ),
						g_ulCurYPos,
						szString,
						DTA_VirtualWidth, g_ValWidth.Int,
						DTA_VirtualHeight, g_ValHeight.Int,
						TAG_DONE );
			}
			else
			{
				screen->DrawText( ulColor,
						(LONG)( g_aulColumnX[ulIdx] * CleanXfac ),
						g_ulCurYPos,
						szString,
						TAG_DONE );
			}
		}
	}
}

//*****************************************************************************
//
static void scoreboard_DrawHeader( void )
{
	g_ulCurYPos = 4;

	// Don't draw it if we're in intermission.
	if ( gamestate == GS_LEVEL )
	{
		screen->SetFont( BigFont );

		if ( g_bScale )
		{
			screen->DrawText( gameinfo.gametype == GAME_Doom ? CR_RED : CR_UNTRANSLATED,
				(LONG)(( g_ValWidth.Int / 2 ) - ( BigFont->StringWidth( "RANKINGS" ) / 2 )),
				g_ulCurYPos,
				"RANKINGS",
				DTA_VirtualWidth, g_ValWidth.Int,
				DTA_VirtualHeight, g_ValHeight.Int,
				TAG_DONE );
		}
		else
		{
			screen->DrawText( gameinfo.gametype == GAME_Doom ? CR_RED : CR_UNTRANSLATED,
				( SCREENWIDTH / 2 ) - ( BigFont->StringWidth( "RANKINGS" ) / 2 ),
				g_ulCurYPos,
				"RANKINGS",
				TAG_DONE );
		}

		screen->SetFont( SmallFont );
	}

	g_ulCurYPos += 22;
}

//*****************************************************************************
//
static void scoreboard_DrawLimits( void )
{
	LONG	lNumDuels;
	LONG	lWinner;
	LONG	lFragsLeft;
	ULONG	ulIdx;
	bool	bDraw;
	char	szString[128];

	if (( lastmanstanding == false ) && ( teamlms == false ) && ( possession == false ) && ( teampossession == false ) && deathmatch && fraglimit )
	{
		lFragsLeft = SCOREBOARD_GetLeftToLimit( );

		if ( lFragsLeft > 0 )
		{
			if ( lFragsLeft < 0 )
				lFragsLeft = 0;

			sprintf( szString, "%d frag%s remain%s", lFragsLeft, ( lFragsLeft != 1 ) ? "s" : "", ( lFragsLeft == 1 ) ? "s" : "" );

			if ( g_bScale )
			{
				screen->DrawText( CR_GREY,
					(LONG)(( g_ValWidth.Int / 2 ) - ( SmallFont->StringWidth( szString ) / 2 )),
					g_ulCurYPos,
					szString,
					DTA_VirtualWidth, g_ValWidth.Int,
					DTA_VirtualHeight, g_ValHeight.Int,
					TAG_DONE );
			}
			else
			{
				screen->DrawText( CR_GREY,
					( SCREENWIDTH / 2 ) - ( SmallFont->StringWidth( szString ) / 2 ),
					g_ulCurYPos,
					szString,
					TAG_DONE );
			}

			g_ulCurYPos += 10;
		}
	}

	// Render the duellimit string.
	if ( duellimit && duel )
	{
		// Get the number of duels that have been played.
		lNumDuels = DUEL_GetNumDuels( );

		sprintf( szString, "%d duel%s remain%s", duellimit - lNumDuels, (( duellimit - lNumDuels ) == 1 ) ? "" : "s", (( duellimit - lNumDuels ) == 1 ) ? "s" : "" );

		if ( g_bScale )
		{
			screen->DrawText( CR_GREY,
				(LONG)(( g_ValWidth.Int / 2 ) - ( SmallFont->StringWidth( szString ) / 2 )),
				g_ulCurYPos,
				szString,
				DTA_VirtualWidth, g_ValWidth.Int,
				DTA_VirtualHeight, g_ValHeight.Int,
				TAG_DONE );
		}
		else
		{
			screen->DrawText( CR_GREY,
				( SCREENWIDTH / 2 ) - ( SmallFont->StringWidth( szString ) / 2 ),
				g_ulCurYPos,
				szString,
				TAG_DONE );
		}

		g_ulCurYPos += 10;
	}

	// Render the "wins" string.
	if ( duel && gamestate == GS_LEVEL )
	{
		lWinner = -1;
		for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
		{
			if ( playeringame[ulIdx] && players[ulIdx].ulWins )
			{
				lWinner = ulIdx;
				break;
			}
		}

		bDraw = true;
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

			if ( g_bScale )
			{
				screen->DrawText( CR_GREY,
					(LONG)(( g_ValWidth.Int / 2 ) - ( SmallFont->StringWidth( szString ) / 2 )),
					g_ulCurYPos,
					szString,
					DTA_VirtualWidth, g_ValWidth.Int,
					DTA_VirtualHeight, g_ValHeight.Int,
					TAG_DONE );
			}
			else
			{
				screen->DrawText( CR_GREY,
					( SCREENWIDTH / 2 ) - ( SmallFont->StringWidth( szString ) / 2 ),
					g_ulCurYPos,
					szString,
					TAG_DONE );
			}
			
			g_ulCurYPos += 10;
		}
	}

	// Render the pointlimit string.
	if (( teamgame || possession || teampossession ) && ( pointlimit ) && ( gamestate == GS_LEVEL ))
	{
		ULONG	ulPointsLeft;
		
		ulPointsLeft = SCOREBOARD_GetLeftToLimit( );
		if ( ulPointsLeft > 0 )
		{
			sprintf( szString, "%d point%s remain%s", ulPointsLeft, ( ulPointsLeft != 1 ) ? "s" : "", ( ulPointsLeft == 1 ) ? "s" : "" );

			if ( g_bScale )
			{
				screen->DrawText( CR_GREY,
					(LONG)(( g_ValWidth.Int / 2 ) - ( SmallFont->StringWidth( szString ) / 2 )),
					g_ulCurYPos,
					szString,
					DTA_VirtualWidth, g_ValWidth.Int,
					DTA_VirtualHeight, g_ValHeight.Int,
					TAG_DONE );
			}
			else
			{
				screen->DrawText( CR_GREY,
					( SCREENWIDTH / 2 ) - ( SmallFont->StringWidth( szString ) / 2 ),
					g_ulCurYPos,
					szString,
					TAG_DONE );
			}

			g_ulCurYPos += 10;
		}
	}

	// Render the winlimit string.
	if (( lastmanstanding || teamlms ) && winlimit && gamestate == GS_LEVEL )
	{
		ULONG	ulWinsLeft = SCOREBOARD_GetLeftToLimit( );
		sprintf( szString, "%d win%s remain%s", ulWinsLeft, ( ulWinsLeft != 1 ) ? "s" : "", ( ulWinsLeft == 1 ) ? "s" : "" );
		
		if ( g_bScale )
		{
			screen->DrawText( CR_GREY,
				(LONG)(( g_ValWidth.Int / 2 ) - ( SmallFont->StringWidth( szString ) / 2 )),
				g_ulCurYPos,
				szString,
				DTA_VirtualWidth, g_ValWidth.Int,
				DTA_VirtualHeight, g_ValHeight.Int,
				TAG_DONE );
		}
		else
		{
			screen->DrawText( CR_GREY,
				( SCREENWIDTH / 2 ) - ( SmallFont->StringWidth( szString ) / 2 ),
				g_ulCurYPos,
				szString,
				TAG_DONE );
		}

		g_ulCurYPos += 10;
	}

	// Render the wavelimit string.
	if ( invasion && wavelimit && gamestate == GS_LEVEL )
	{
		ULONG	ulWavesLeft;

		ulWavesLeft = wavelimit - INVASION_GetCurrentWave( );
		sprintf( szString, "%d wave%s remain%s", ulWavesLeft, ( ulWavesLeft != 1 ) ? "s" : "", ( ulWavesLeft == 1 ) ? "s" : "" );
		
		if ( g_bScale )
		{
			screen->DrawText( CR_GREY,
				(LONG)(( g_ValWidth.Int / 2 ) - ( SmallFont->StringWidth( szString ) / 2 )),
				g_ulCurYPos,
				szString,
				DTA_VirtualWidth, g_ValWidth.Int,
				DTA_VirtualHeight, g_ValHeight.Int,
				TAG_DONE );
		}
		else
		{
			screen->DrawText( CR_GREY,
				( SCREENWIDTH / 2 ) - ( SmallFont->StringWidth( szString ) / 2 ),
				g_ulCurYPos,
				szString,
				TAG_DONE );
		}

		g_ulCurYPos += 10;
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
		
		if ( g_bScale )
		{
			screen->DrawText( CR_GREY,
				(LONG)(( g_ValWidth.Int / 2 ) - ( SmallFont->StringWidth( szString ) / 2 )),
				g_ulCurYPos,
				szString,
				DTA_VirtualWidth, g_ValWidth.Int,
				DTA_VirtualHeight, g_ValHeight.Int,
				TAG_DONE );
		}
		else
		{
			screen->DrawText( CR_GREY,
				( SCREENWIDTH / 2 ) - ( SmallFont->StringWidth( szString ) / 2 ),
				g_ulCurYPos,
				szString,
				TAG_DONE );
		}

		g_ulCurYPos += 10;
	}

	// Render the number of monsters left in coop.
	if (( deathmatch == false ) && ( teamgame == false ) && ( gamestate == GS_LEVEL ))
	{
		LONG	lNumMonstersRemaining = SCOREBOARD_GetLeftToLimit( );
		sprintf( szString, "%d monster%s remaining", lNumMonstersRemaining, lNumMonstersRemaining == 1 ? "" : "s" );
		
		if ( g_bScale )
		{
			screen->DrawText( CR_GREY,
				(LONG)(( g_ValWidth.Int / 2 ) - ( SmallFont->StringWidth( szString ) / 2 )),
				g_ulCurYPos,
				szString,
				DTA_VirtualWidth, g_ValWidth.Int,
				DTA_VirtualHeight, g_ValHeight.Int,
				TAG_DONE );
		}
		else
		{
			screen->DrawText( CR_GREY,
				( SCREENWIDTH / 2 ) - ( SmallFont->StringWidth( szString ) / 2 ),
				g_ulCurYPos,
				szString,
				TAG_DONE );
		}

		g_ulCurYPos += 10;
	}
}

//*****************************************************************************
//
static void scoreboard_DrawTeamScores( ULONG ulPlayer )
{
	char	szString[128];

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
					if ( g_bScale )
					{
						screen->DrawText( CR_GREY,
							(LONG)(( g_ValWidth.Int / 2 ) - ( SmallFont->StringWidth( szString ) / 2 )),
							g_ulCurYPos,
							szString,
							DTA_VirtualWidth, g_ValWidth.Int,
							DTA_VirtualHeight, g_ValHeight.Int,
							TAG_DONE );
					}
					else
					{
						screen->DrawText( CR_GREY,
							( SCREENWIDTH / 2 ) - ( SmallFont->StringWidth( szString ) / 2 ),
							g_ulCurYPos,
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

					if ( g_bScale )
					{
						screen->DrawText( CR_GREY,
							(LONG)(( g_ValWidth.Int / 2 ) - ( SmallFont->StringWidth( szString ) / 2 )),
							g_ulCurYPos,
							szString,
							DTA_VirtualWidth, g_ValWidth.Int,
							DTA_VirtualHeight, g_ValHeight.Int,
							TAG_DONE );
					}
					else
					{
						screen->DrawText( CR_GREY,
							( SCREENWIDTH / 2 ) - ( SmallFont->StringWidth( szString ) / 2 ),
							g_ulCurYPos,
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
					if ( g_bScale )
					{
						screen->DrawText( CR_GREY,
							(LONG)(( g_ValWidth.Int / 2 ) - ( SmallFont->StringWidth( szString ) / 2 )),
							g_ulCurYPos,
							szString,
							DTA_VirtualWidth, g_ValWidth.Int,
							DTA_VirtualHeight, g_ValHeight.Int,
							TAG_DONE );
					}
					else
					{
						screen->DrawText( CR_GREY,
							( SCREENWIDTH / 2 ) - ( SmallFont->StringWidth( szString ) / 2 ),
							g_ulCurYPos,
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

					if ( g_bScale )
					{
						screen->DrawText( CR_GREY,
							(LONG)(( g_ValWidth.Int / 2 ) - ( SmallFont->StringWidth( szString ) / 2 )),
							g_ulCurYPos,
							szString,
							DTA_VirtualWidth, g_ValWidth.Int,
							DTA_VirtualHeight, g_ValHeight.Int,
							TAG_DONE );
					}
					else
					{
						screen->DrawText( CR_GREY,
							( SCREENWIDTH / 2 ) - ( SmallFont->StringWidth( szString ) / 2 ),
							g_ulCurYPos,
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
					if ( g_bScale )
					{
						screen->DrawText( CR_GREY,
							(LONG)(( g_ValWidth.Int / 2 ) - ( SmallFont->StringWidth( szString ) / 2 )),
							g_ulCurYPos,
							szString,
							DTA_VirtualWidth, g_ValWidth.Int,
							DTA_VirtualHeight, g_ValHeight.Int,
							TAG_DONE );
					}
					else
					{
						screen->DrawText( CR_GREY,
							( SCREENWIDTH / 2 ) - ( SmallFont->StringWidth( szString ) / 2 ),
							g_ulCurYPos,
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

					if ( g_bScale )
					{
						screen->DrawText( CR_GREY,
							(LONG)(( g_ValWidth.Int / 2 ) - ( SmallFont->StringWidth( szString ) / 2 )),
							g_ulCurYPos,
							szString,
							DTA_VirtualWidth, g_ValWidth.Int,
							DTA_VirtualHeight, g_ValHeight.Int,
							TAG_DONE );
					}
					else
					{
						screen->DrawText( CR_GREY,
							( SCREENWIDTH / 2 ) - ( SmallFont->StringWidth( szString ) / 2 ),
							g_ulCurYPos,
							szString,
							TAG_DONE );
					}
				}
			}

			// Draw the team info information.
			// If this variable is true, it's already been drawn. No need to do it again.
			if ( cl_alwaysdrawteamstats == false )
				SCOREBOARD_RenderTeamStats( &players[ulPlayer] );
		}
		else
		{
			g_ulCurYPos += 10;
			if ( teamplay )
			{
				// If the teams are tied...
				if ( TEAM_GetFragCount( TEAM_RED ) == TEAM_GetFragCount( TEAM_BLUE ))
				{
					sprintf( szString, "Teams tied at %d\n", TEAM_GetFragCount( TEAM_RED ));
					if ( g_bScale )
					{
						screen->DrawText( CR_GREY,
							(LONG)(( g_ValWidth.Int / 2 ) - ( SmallFont->StringWidth( szString ) / 2 )),
							g_ulCurYPos,
							szString,
							DTA_VirtualWidth, g_ValWidth.Int,
							DTA_VirtualHeight, g_ValHeight.Int,
							TAG_DONE );
					}
					else
					{
						screen->DrawText( CR_GREY,
							( SCREENWIDTH / 2 ) - ( SmallFont->StringWidth( szString ) / 2 ),
							g_ulCurYPos,
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

					if ( g_bScale )
					{
						screen->DrawText( CR_GREY,
							(LONG)(( g_ValWidth.Int / 2 ) - ( SmallFont->StringWidth( szString ) / 2 )),
							g_ulCurYPos,
							szString,
							DTA_VirtualWidth, g_ValWidth.Int,
							DTA_VirtualHeight, g_ValHeight.Int,
							TAG_DONE );
					}
					else
					{
						screen->DrawText( CR_GREY,
							( SCREENWIDTH / 2 ) - ( SmallFont->StringWidth( szString ) / 2 ),
							g_ulCurYPos,
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
					if ( g_bScale )
					{
						screen->DrawText( CR_GREY,
							(LONG)(( g_ValWidth.Int / 2 ) - ( SmallFont->StringWidth( szString ) / 2 )),
							g_ulCurYPos,
							szString,
							DTA_VirtualWidth, g_ValWidth.Int,
							DTA_VirtualHeight, g_ValHeight.Int,
							TAG_DONE );
					}
					else
					{
						screen->DrawText( CR_GREY,
							( SCREENWIDTH / 2 ) - ( SmallFont->StringWidth( szString ) / 2 ),
							g_ulCurYPos,
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

					if ( g_bScale )
					{
						screen->DrawText( CR_GREY,
							(LONG)(( g_ValWidth.Int / 2 ) - ( SmallFont->StringWidth( szString ) / 2 )),
							g_ulCurYPos,
							szString,
							DTA_VirtualWidth, g_ValWidth.Int,
							DTA_VirtualHeight, g_ValHeight.Int,
							TAG_DONE );
					}
					else
					{
						screen->DrawText( CR_GREY,
							( SCREENWIDTH / 2 ) - ( SmallFont->StringWidth( szString ) / 2 ),
							g_ulCurYPos,
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
					if ( g_bScale )
					{
						screen->DrawText( CR_GREY,
							(LONG)(( g_ValWidth.Int / 2 ) - ( SmallFont->StringWidth( szString ) / 2 )),
							g_ulCurYPos,
							szString,
							DTA_VirtualWidth, g_ValWidth.Int,
							DTA_VirtualHeight, g_ValHeight.Int,
							TAG_DONE );
					}
					else
					{
						screen->DrawText( CR_GREY,
							( SCREENWIDTH / 2 ) - ( SmallFont->StringWidth( szString ) / 2 ),
							g_ulCurYPos,
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

					if ( g_bScale )
					{
						screen->DrawText( CR_GREY,
							(LONG)(( g_ValWidth.Int / 2 ) - ( SmallFont->StringWidth( szString ) / 2 )),
							g_ulCurYPos,
							szString,
							DTA_VirtualWidth, g_ValWidth.Int,
							DTA_VirtualHeight, g_ValHeight.Int,
							TAG_DONE );
					}
					else
					{
						screen->DrawText( CR_GREY,
							( SCREENWIDTH / 2 ) - ( SmallFont->StringWidth( szString ) / 2 ),
							g_ulCurYPos,
							szString,
							TAG_DONE );
					}
				}
			}
		}

		g_ulCurYPos += 10;
	}
}

//*****************************************************************************
//
static void scoreboard_DrawMyRank( ULONG ulPlayer )
{
	char	szString[128];
	bool	bIsTied;

	// Render the current ranking string.
	if ( deathmatch && ( teamplay == false ) && ( teamlms == false ) && ( teampossession == false ) && ( PLAYER_IsTrueSpectator( &players[ulPlayer] ) == false ))
	{
		bIsTied	= SCOREBOARD_IsTied( ulPlayer );

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
			sprintf( szString, "%s\\c-place with %d win%s", szString, players[ulPlayer].ulWins, players[ulPlayer].ulWins == 1 ? "" : "s" );
		else if ( possession )
			sprintf( szString, "%s\\c-place with %d point%s", szString, players[ulPlayer].lPointCount, players[ulPlayer].fragcount == 1 ? "" : "s" );
		else
			sprintf( szString, "%s\\c-place with %d frag%s", szString, players[ulPlayer].fragcount, players[ulPlayer].fragcount == 1 ? "" : "s" );
		V_ColorizeString( szString );

		if ( g_bScale )
		{
			screen->DrawText( CR_GREY,
				(LONG)(( g_ValWidth.Int / 2 ) - ( SmallFont->StringWidth( szString ) / 2 )),
				g_ulCurYPos,
				szString,
				DTA_VirtualWidth, g_ValWidth.Int,
				DTA_VirtualHeight, g_ValHeight.Int,
				TAG_DONE );
		}
		else
		{
			screen->DrawText( CR_GREY,
				( SCREENWIDTH / 2 ) - ( SmallFont->StringWidth( szString ) / 2 ),
				g_ulCurYPos,
				szString,
				TAG_DONE );
		}

		g_ulCurYPos += 10;
	}
}

//*****************************************************************************
//
static void scoreboard_ClearColumns( void )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAX_COLUMNS; ulIdx++ )
		g_aulColumnType[ulIdx] = COLUMN_EMPTY;

	g_ulNumColumnsUsed = 0;
}

//*****************************************************************************
//
static void scoreboard_Prepare5ColumnDisplay( void )
{
	// Set all to empty.
	scoreboard_ClearColumns( );

	g_ulNumColumnsUsed = 5;
	g_pColumnHeaderFont = BigFont;

	// Set up the location of each column.
	g_aulColumnX[0] = 8;
	g_aulColumnX[1] = 48;
	g_aulColumnX[2] = 96;
	g_aulColumnX[3] = 212;
	g_aulColumnX[4] = 272;

	// Build columns for modes in which players try to earn points.
	if ( teamgame || possession || teampossession || ctf || skulltag )
	{
		g_aulColumnType[0] = COLUMN_POINTS;
		// [BC] Doesn't look like this is being used right now (at least not properly).
/*
		// Can have assists.
		if ( ctf || skulltag )
			g_aulColumnType[0] = COLUMN_POINTS;
*/
		g_aulColumnType[1] = COLUMN_FRAGS;
		g_aulColumnType[2] = COLUMN_NAME;
		g_aulColumnType[3] = COLUMN_DEATHS;
		if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
			g_aulColumnType[3] = COLUMN_PING;
		g_aulColumnType[4] = COLUMN_TIME;

		// Sort players based on their pointcount.
		scoreboard_SortPlayers( ST_POINTCOUNT );
	}

	// Build columns for modes in which players try to earn wins.
	if ( lastmanstanding || teamlms )
	{
		g_aulColumnType[0] = COLUMN_WINS;
		g_aulColumnType[1] = COLUMN_FRAGS;
		g_aulColumnType[2] = COLUMN_NAME;
		g_aulColumnType[3] = COLUMN_EMPTY;
		if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
			g_aulColumnType[3] = COLUMN_PING;
		g_aulColumnType[4] = COLUMN_TIME;

		// Sort players based on their pointcount.
		scoreboard_SortPlayers( ST_WINCOUNT );
	}
}

//*****************************************************************************
//
static void scoreboard_Prepare4ColumnDisplay( void )
{
	// Set all to empty.
	scoreboard_ClearColumns( );

	g_ulNumColumnsUsed = 4;
	g_pColumnHeaderFont = BigFont;

	// Set up the location of each column.
	g_aulColumnX[0] = 24;
	g_aulColumnX[1] = 84;
	g_aulColumnX[2] = 192;
	g_aulColumnX[3] = 256;

	// Build columns for modes in which players try to earn kills.
	if ( cooperative || survival || invasion )
	{
		g_aulColumnType[0] = COLUMN_KILLS;
		g_aulColumnType[1] = COLUMN_NAME;
		g_aulColumnType[2] = COLUMN_DEATHS;
		if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
			g_aulColumnType[2] = COLUMN_PING;
		g_aulColumnType[3] = COLUMN_TIME;

		// Sort players based on their killcount.
		scoreboard_SortPlayers( ST_KILLCOUNT );
	}

	// Build columns for modes in which players try to earn frags.
	if ( deathmatch || teamplay )
	{
		g_aulColumnType[0] = COLUMN_FRAGS;
		g_aulColumnType[1] = COLUMN_NAME;
		g_aulColumnType[2] = COLUMN_DEATHS;
		if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
			g_aulColumnType[2] = COLUMN_PING;
		g_aulColumnType[3] = COLUMN_TIME;

		// Sort players based on their fragcount.
		scoreboard_SortPlayers( ST_FRAGCOUNT );
	}
	
	// Build columns for modes in which players try to earn points.
	if ( teamgame || possession || teampossession || ctf || skulltag )
	{
//		if ( ctf || skulltag ) // Can have assists
//			g_aulColumnType[0] = COL_POINTS;
		g_aulColumnType[0] = COLUMN_POINTS;
		g_aulColumnType[1] = COLUMN_NAME;
		g_aulColumnType[2] = COLUMN_DEATHS;
		if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
			g_aulColumnType[2] = COLUMN_PING;
		g_aulColumnType[3] = COLUMN_TIME;

		// Sort players based on their pointcount.
		scoreboard_SortPlayers( ST_POINTCOUNT );
	}

	// Build columns for modes in which players try to earn wins.
	if ( lastmanstanding || teamlms )
	{
		g_aulColumnType[0] = COLUMN_WINS;
		g_aulColumnType[1] = COLUMN_NAME;
		g_aulColumnType[2] = COLUMN_FRAGS;
		if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
			g_aulColumnType[2] = COLUMN_PING;
		g_aulColumnType[3] = COLUMN_TIME;

		// Sort players based on their wincount.
		scoreboard_SortPlayers( ST_WINCOUNT );
	}
}

//*****************************************************************************
//
static void scoreboard_Prepare3ColumnDisplay( void )
{
	// Set all to empty.
	scoreboard_ClearColumns( );

	g_ulNumColumnsUsed = 3;
	g_pColumnHeaderFont = SmallFont;

	// Set up the location of each column.
	g_aulColumnX[0] = 16;
	g_aulColumnX[1] = 96;
	g_aulColumnX[2] = 272;

	// Build columns for modes in which players try to earn kills.
	if ( cooperative || survival || invasion )
	{
		g_aulColumnType[0] = COLUMN_KILLS;
		g_aulColumnType[1] = COLUMN_NAME;
		g_aulColumnType[2] = COLUMN_TIME;

		// Sort players based on their killcount.
		scoreboard_SortPlayers( ST_KILLCOUNT );
	}

	// Build columns for modes in which players try to earn frags.
	if ( deathmatch || teamplay )
	{
		g_aulColumnType[0] = COLUMN_FRAGS;
		g_aulColumnType[1] = COLUMN_NAME;
		g_aulColumnType[2] = COLUMN_TIME;

		// Sort players based on their fragcount.
		scoreboard_SortPlayers( ST_FRAGCOUNT );
	}
	
	// Build columns for modes in which players try to earn points.
	if ( teamgame || possession || teampossession || ctf || skulltag )
	{
//		if ( ctf || skulltag ) // Can have assists
//			g_aulColumnType[0] = COL_POINTS;

		g_aulColumnType[0] = COLUMN_POINTS;
		g_aulColumnType[1] = COLUMN_NAME;
		g_aulColumnType[2] = COLUMN_TIME;

		// Sort players based on their pointcount.
		scoreboard_SortPlayers( ST_POINTCOUNT );
	}

	// Build columns for modes in which players try to earn wins.
	if ( lastmanstanding || teamlms )
	{
		g_aulColumnType[0] = COLUMN_WINS;
		g_aulColumnType[1] = COLUMN_NAME;
		g_aulColumnType[2] = COLUMN_TIME;

		// Sort players based on their wincount.
		scoreboard_SortPlayers( ST_WINCOUNT );
	}
}

//*****************************************************************************
//	These parameters are filters.
//	If 1, players with this trait will be skipped.
//	If 2, players *without* this trait will be skipped.
static void scoreboard_DoRankingListPass( LONG lSpectators, LONG lDead, LONG lNotPlaying, LONG lNoTeam, LONG lWrongTeam, ULONG ulDesiredTeam )
{
	ULONG	ulIdx;
	ULONG	ulNumPlayers;

	ulNumPlayers = 0;
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		// Skip or require players not in the game.
		if (((lNotPlaying == 1) && (playeringame[g_iSortedPlayers[ulIdx]] == false )) ||
			((lNotPlaying == 2) && (!playeringame[g_iSortedPlayers[ulIdx]] == false )))
			continue;

		// Skip or require players not on a team.
		 if(((lNoTeam == 1) && (!players[g_iSortedPlayers[ulIdx]].bOnTeam)) ||
			 ((lNoTeam == 2) && (players[g_iSortedPlayers[ulIdx]].bOnTeam)))
			continue;

		// Skip or require spectators.
		if (((lSpectators == 1) && PLAYER_IsTrueSpectator( &players[g_iSortedPlayers[ulIdx]])) ||
			((lSpectators == 2) && !PLAYER_IsTrueSpectator( &players[g_iSortedPlayers[ulIdx]])))
			continue;

		// In LMS, skip or require dead players.
		if( gamestate != GS_INTERMISSION ){
			/*(( lastmanstanding ) && (( LASTMANSTANDING_GetState( ) == LMSS_INPROGRESS ) || ( LASTMANSTANDING_GetState( ) == LMSS_WINSEQUENCE ))) ||
			(( survival ) && (( SURVIVAL_GetState( ) == SURVS_INPROGRESS ) || ( SURVIVAL_GetState( ) == SURVS_MISSIONFAILED )))*/
			
			if((( players[g_iSortedPlayers[ulIdx]].health <= 0) && (lDead == 1)) || 
				(( players[g_iSortedPlayers[ulIdx]].health > 0) && (lDead == 2)))
				continue;
		}

		// Skip or require players that aren't on this team.
		if (((lWrongTeam == 1) && (players[g_iSortedPlayers[ulIdx]].ulTeam != ulDesiredTeam)) ||
			((lWrongTeam == 2) && (players[g_iSortedPlayers[ulIdx]].ulTeam == ulDesiredTeam)))
			continue;

		scoreboard_RenderIndividualPlayer( g_iSortedPlayers[ulIdx] );
		g_ulCurYPos += 10;
		ulNumPlayers++;
	}

	if ( ulNumPlayers )
		g_ulCurYPos += 10;
}

//*****************************************************************************
//
static void scoreboard_DrawRankings( void )
{
	ULONG	ulIdx;
	ULONG	ulTeamIdx;
	char	szString[16];

	// Nothing to do.
	if ( g_ulNumColumnsUsed < 1 )
		return;

	g_ulCurYPos += 8;

	// Center this a little better in intermission
	if ( gamestate != GS_LEVEL )
		g_ulCurYPos = ( g_bScale == true ) ? (LONG)( 48 * g_fYScale ) : (LONG)( 48 * CleanYfac );

	screen->SetFont( g_pColumnHeaderFont );

	// Draw the titles for the columns.
	for ( ulIdx = 0; ulIdx < g_ulNumColumnsUsed; ulIdx++ )
	{
		sprintf( szString, "%s", g_pszColumnHeaders[g_aulColumnType[ulIdx]] );
		if ( g_bScale )
		{
			screen->DrawText( CR_RED,
				(LONG)( g_aulColumnX[ulIdx] * g_fXScale ),
				g_ulCurYPos,
				szString,
				DTA_VirtualWidth, g_ValWidth.Int,
				DTA_VirtualHeight, g_ValHeight.Int,
				TAG_DONE );
		}
		else
		{
			screen->DrawText( CR_RED,
				(LONG)( g_aulColumnX[ulIdx] * CleanXfac ),
				g_ulCurYPos,
				szString,
				TAG_DONE );
		}
	}

	// Draw the player list.
	screen->SetFont( SmallFont );
	g_ulCurYPos += 24;

	// Team-based games: Divide up the teams.
	if ( teamgame || teamplay || teamlms || teampossession )
	{
		// Draw players on teams.
		for ( ulTeamIdx = 0; ulTeamIdx < NUM_TEAMS; ulTeamIdx++ )
		{
			// In team LMS, separate the dead players from the living.
			if (( teamlms ) && ( gamestate != GS_INTERMISSION ) && ( LASTMANSTANDING_GetState( ) != LMSS_COUNTDOWN ) && ( LASTMANSTANDING_GetState( ) != LMSS_WAITINGFORPLAYERS ))
			{
				scoreboard_DoRankingListPass(1, 1, 1, 1, 1, ulTeamIdx ); // Living in this team
				scoreboard_DoRankingListPass(1, 2, 1, 1, 1, ulTeamIdx ); // Dead in this team
			}
			// Otherwise, draw all players all in one group.
			else
				scoreboard_DoRankingListPass(1, 0, 1, 1, 1, ulTeamIdx ); 

		}

		// Players that aren't on a team.
		scoreboard_DoRankingListPass(1, 1, 1, 2, 0, 0); 

		// Spectators are last.
		scoreboard_DoRankingListPass(2, 0, 1, 0, 0, 0);
	}
	// Other modes: Just players and spectators.
	else
	{
		// In LMS or Survival, dead players are drawn after living ones.
		if (( gamestate != GS_INTERMISSION ) && (
			(( lastmanstanding ) && (( LASTMANSTANDING_GetState( ) == LMSS_INPROGRESS ) || ( LASTMANSTANDING_GetState( ) == LMSS_WINSEQUENCE ))) ||
			(( survival ) && (( SURVIVAL_GetState( ) == SURVS_INPROGRESS ) || ( SURVIVAL_GetState( ) == SURVS_MISSIONFAILED )))))
		{
			scoreboard_DoRankingListPass(1, 1, 1, 0, 0, 0); // Living
			scoreboard_DoRankingListPass(1, 2, 1, 0, 0, 0); // Dead
		}
		// Othrwise, draw all active players in the game together.
		else
			scoreboard_DoRankingListPass(1, 0, 1, 0, 0, 0);

		// Spectators are last.
		scoreboard_DoRankingListPass(2, 0, 1, 0, 0, 0);
	}

	BorderNeedRefresh = true;
}

//*****************************************************************************
//	CONSOLE VARIABLES

CVAR( Bool, cl_alwaysdrawdmstats, true, CVAR_ARCHIVE )
CVAR( Bool, cl_alwaysdrawteamstats, true, CVAR_ARCHIVE )
