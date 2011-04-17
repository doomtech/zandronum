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
// Filename: joinqueue.h
//
// Description: Contains join queue structures and prototypes
//
//-----------------------------------------------------------------------------

#ifndef __JOINQUEUE_H__
#define __JOINQUEUE_H__

#include "doomtype.h"

//*****************************************************************************
//	STRUCTURES

typedef struct {

	// Player ID of the incoming player.
	ULONG	ulPlayer;

	// Team the incoming player will be on.
	ULONG	ulTeam;

} JOINSLOT_t;

//*****************************************************************************
//	PROTOTYPES

void	JOINQUEUE_Construct( void );

void	JOINQUEUE_RemovePlayerFromQueue ( ULONG ulPlayer, bool bBroadcast );
void	JOINQUEUE_PlayerLeftGame( bool bWantPop );
void	JOINQUEUE_SpectatorLeftGame( ULONG ulPlayer );
void	JOINQUEUE_PopQueue( LONG lNumSlots );
ULONG	JOINQUEUE_AddPlayer( JOINSLOT_t JoinSlot );
void	JOINQUEUE_ClearList( void );
LONG	JOINQUEUE_GetPositionInLine( ULONG ulPlayer );
void	JOINQUEUE_SetClientPositionInLine( LONG lPosition );
void	JOINQUEUE_AddConsolePlayer( ULONG ulDesiredTeam );

#endif	// __JOINQUEUE_H__
