//-----------------------------------------------------------------------------
//
// Skulltag Source
// Copyright (C) 2004-2005 Brad Carney
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
// Filename: callvote.h
//
// Description: 
//
//-----------------------------------------------------------------------------

#ifndef	__CALLVOTE_H__
#define	__CALLVOTE_H__

#include "c_cvars.h"

//*****************************************************************************
//	DEFINES

#define	VOTE_COUNTDOWN_TIME			20
#define	VOTE_PASSED_TIME			5

//*****************************************************************************
enum
{
	VOTECMD_KICK,
	VOTECMD_MAP,
	VOTECMD_CHANGEMAP,
	VOTECMD_FRAGLIMIT,
	VOTECMD_TIMELIMIT,
	VOTECMD_WINLIMIT,
	VOTECMD_DUELLIMIT,
	VOTECMD_POINTLIMIT,
};

//*****************************************************************************
typedef enum
{
	VOTESTATE_NOVOTE,
	VOTESTATE_INVOTE,
	VOTESTATE_VOTECOMPLETED,

} VOTESTATE_e;

//*****************************************************************************
//	PROTOTYPES

void			CALLVOTE_Construct( void );
void			CALLVOTE_Tick( void );
//void			CALLVOTE_Render( void );
void			CALLVOTE_BeginVote( char *pszCommand, char *pszParameters, ULONG ulPlayer );
void			CALLVOTE_ClearVote( void );
bool			CALLVOTE_VoteYes( ULONG ulPlayer );
bool			CALLVOTE_VoteNo( ULONG ulPlayer );
ULONG			CALLVOTE_CountNumEligibleVoters( void );
void			CALLVOTE_EndVote( bool bPassed );

//void			CALLVOTE_SetCommand( char *pszCommand );
char			*CALLVOTE_GetCommand( void );

//void			CALLVOTE_SetVoteCaller( ULONG ulPlayer );
ULONG			CALLVOTE_GetVoteCaller( void );

VOTESTATE_e		CALLVOTE_GetVoteState( void );

ULONG			CALLVOTE_GetCountdownTicks( void );
ULONG			*CALLVOTE_GetPlayersWhoVotedYes( void );
ULONG			*CALLVOTE_GetPlayersWhoVotedNo( void );

//*****************************************************************************
//	EXTERNAL CONSOLE VARIABLES

EXTERN_CVAR( Bool, sv_nocallvote )

#endif	// __CALLVOTE_H__
