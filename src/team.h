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
// Filename: team.h
//
// Description: Contains team structures and prototypes
//
//-----------------------------------------------------------------------------

#ifndef __TEAM_H__
#define __TEAM_H__

#include "a_pickups.h"
#include "c_cvars.h"
//#include "keycard.h"
#include "d_player.h"

//*****************************************************************************
//	DEFINES

enum
{
	TEAM_BLUE,
	TEAM_RED,

	NUM_TEAMS
};

//*****************************************************************************
//	STRUCTURES

typedef struct
{
	// Name of this team ("TEH LEETZORZ!")
	char		szName[32];

	// Color the players of this team will be.
	char		szColor[32];

	// Text color we print various team messages in.
	ULONG		ulTextColor;

	// Color of the railgun trail made by all team members.
	LONG		lRailColor;

	// Current amount of points this team has.
	LONG		lScore;

	// Object type of this team's "flag".
	const PClass		*FlagItem;

	// Icon that appears over a player that posseses this team's "flag".
//	statenum_t	Icon;

	// Amount of time left before this team's "flag" is returned to it's proper place.
	ULONG		ulReturnTicks;

	// Offset for the type of scripts that are run to return this team's "flag".
	ULONG		ulReturnScriptOffset;

	// Number of frags this team has (teamplay deathmatch).
	LONG		lFragCount;

	// Number of combined deaths this team has (teamplay deathmatch).
	LONG		lDeathCount;

	// Number of wins this team has (team LMS).
	LONG		lWinCount;
} TEAM_t;

//*****************************************************************************
//	PROTOTYPES

void		TEAM_Construct( void );
void		TEAM_Tick( void );
void		TEAM_Reset( void );
ULONG		TEAM_CountPlayers( ULONG ulTeamIdx );
ULONG		TEAM_CountLivingPlayers( ULONG ulTeamIdx );
void		TEAM_ExecuteReturnRoutine( ULONG ulTeamIdx, AActor *pReturner );
bool		TEAM_IsFlagMode( void );
ULONG		TEAM_ChooseBestTeamForPlayer( void );
void		TEAM_ScoreSkulltagPoint( player_s *pPlayer, ULONG ulNumPoints, AActor *pPillar );
void		TEAM_DisplayNeedToReturnSkullMessage( player_s *pPlayer );
void		TEAM_FlagDropped( player_s *pPlayer );
WORD		TEAM_GetReturnScriptOffset( ULONG ulTeamIdx );

// Access functions
char		*TEAM_GetName( ULONG ulTeamIdx );
void		TEAM_SetName( ULONG ulTeamIdx, char *pszName );

char		*TEAM_GetColor( ULONG ulTeamIdx );
void		TEAM_SetColor( ULONG ulTeamIdx, char *pszColor );

ULONG		TEAM_GetTextColor( ULONG ulTeamIdx );
void		TEAM_SetTextColor( ULONG ulTeamIdx, USHORT usColor );

LONG		TEAM_GetRailgunColor( ULONG ulTeamIdx );
void		TEAM_SetRailgunColor( ULONG ulTeamIdx, LONG lColor );

LONG		TEAM_GetScore( ULONG ulTeamIdx );
void		TEAM_SetScore( ULONG ulTeamIdx, LONG lScore, bool bAnnouncer );

const PClass	*TEAM_GetFlagItem( ULONG ulTeamIdx );
void		TEAM_SetFlagItem( ULONG ulTeamIdx, const PClass *pType );

ULONG		TEAM_GetReturnTicks( ULONG ulTeamIdx );
void		TEAM_SetReturnTicks( ULONG ulTeamIdx, ULONG ulTicks );

LONG		TEAM_GetFragCount( ULONG ulTeamIdx );
void		TEAM_SetFragCount( ULONG ulTeamIdx, LONG lFragCount, bool bAnnounce );

LONG		TEAM_GetDeathCount( ULONG ulTeamIdx );
void		TEAM_SetDeathCount( ULONG ulTeamIdx, LONG lDeathCount );

LONG		TEAM_GetWinCount( ULONG ulTeamIdx );
void		TEAM_SetWinCount( ULONG ulTeamIdx, LONG lWinCount, bool bAnnounce );

bool		TEAM_GetSimpleCTFMode( void );
void		TEAM_SetSimpleCTFMode( bool bSimple );

bool		TEAM_GetSimpleSTMode( void );
void		TEAM_SetSimpleSTMode( bool bSimple );

bool		TEAM_GetBlueFlagTaken( void );
void		TEAM_SetBlueFlagTaken( bool bTaken );

bool		TEAM_GetRedFlagTaken( void );
void		TEAM_SetRedFlagTaken( bool bTaken );

bool		TEAM_GetWhiteFlagTaken( void );
void		TEAM_SetWhiteFlagTaken( bool bTaken );

bool		TEAM_GetBlueSkullTaken( void );
void		TEAM_SetBlueSkullTaken( bool bTaken );

bool		TEAM_GetRedSkullTaken( void );
void		TEAM_SetRedSkullTaken( bool bTaken );

POS_t		TEAM_GetBlueFlagOrigin( void );
void		TEAM_SetBlueFlagOrigin( POS_t Origin );

POS_t		TEAM_GetRedFlagOrigin( void );
void		TEAM_SetRedFlagOrigin( POS_t Origin );

POS_t		TEAM_GetWhiteFlagOrigin( void );
void		TEAM_SetWhiteFlagOrigin( POS_t Origin );

POS_t		TEAM_GetBlueSkullOrigin( void );
void		TEAM_SetBlueSkullOrigin( POS_t Origin );

POS_t		TEAM_GetRedSkullOrigin( void );
void		TEAM_SetRedSkullOrigin( POS_t Origin );

ULONG		TEAM_GetAssistPlayer( ULONG ulTeamIdx );
void		TEAM_SetAssistPlayer( ULONG ulTeamIdx, ULONG ulPlayer );

//*****************************************************************************
//  EXTERNAL CONSOLE VARIABLES

EXTERN_CVAR( Bool, teamgame )
EXTERN_CVAR( Bool, ctf )
EXTERN_CVAR( Bool, oneflagctf )
EXTERN_CVAR( Bool, skulltag )

EXTERN_CVAR( Int, pointlimit )
EXTERN_CVAR( Int, sv_flagreturntime )

#endif	// __TEAM_H__
