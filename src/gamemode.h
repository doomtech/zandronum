//-----------------------------------------------------------------------------
//
// Skulltag Source
// Copyright (C) 2007 Brad Carney
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
// Date created:  7/12/07
//
//
// Filename: gamemode.h
//
// Description: 
//
//-----------------------------------------------------------------------------

#ifndef __GAMEMODE_H__
#define __GAMEMODE_H__

#include "c_cvars.h"
#include "doomtype.h"
#include "doomdef.h"

//*****************************************************************************
//	DEFINES

#define	GMF_COOPERATIVE					0x00000001
#define	GMF_DEATHMATCH					0x00000002
#define	GMF_TEAMGAME					0x00000004
#define	GMF_PLAYERSEARNKILLS			0x00000010
#define	GMF_PLAYERSEARNFRAGS			0x00000020
#define	GMF_PLAYERSEARNPOINTS			0x00000040
#define	GMF_PLAYERSEARNWINS				0x00000080
#define	GMF_DONTSPAWNMAPTHINGS			0x00000100
#define	GMF_MAPRESETS					0x00000200
#define	GMF_DEADSPECTATORS				0x00000400
#define	GMF_PLAYERSONTEAMS				0x00000800
#define	GMF_USEMAXLIVES					0x00001000

//*****************************************************************************
//  EXTERNAL CONSOLE VARIABLES

EXTERN_CVAR( Bool, sv_suddendeath )

//*****************************************************************************
typedef enum
{
	GAMEMODE_COOPERATIVE,
	GAMEMODE_SURVIVAL,
	GAMEMODE_INVASION,
	GAMEMODE_DEATHMATCH,
	GAMEMODE_TEAMPLAY,
	GAMEMODE_DUEL,
	GAMEMODE_TERMINATOR,
	GAMEMODE_LASTMANSTANDING,
	GAMEMODE_TEAMLMS,
	GAMEMODE_POSSESSION,
	GAMEMODE_TEAMPOSSESSION,
	GAMEMODE_TEAMGAME,
	GAMEMODE_CTF,
	GAMEMODE_ONEFLAGCTF,
	GAMEMODE_SKULLTAG,
	GAMEMODE_DOMINATION,

	NUM_GAMEMODES

} GAMEMODE_e;

//*****************************************************************************
typedef enum
{
	MODIFIER_NONE,
	MODIFIER_INSTAGIB,
	MODIFIER_BUCKSHOT,

	NUM_MODIFIERS

} MODIFIER_e;

//*****************************************************************************
//	STRUCTURES

typedef struct
{
	// Flags for this game mode.
	ULONG	ulFlags;

	// This is what's displayed in the internal browser for a server's game mode.
	char	szShortName[9];

	// This is the name of the texture that displays when we press the F1 key in
	// this game mode.
	char	szF1Texture[9];

} GAMEMODE_s;

//*****************************************************************************
//	PROTOTYPES

void		GAMEMODE_Construct( void );
ULONG		GAMEMODE_GetFlags( GAMEMODE_e GameMode );
char		*GAMEMODE_GetShortName( GAMEMODE_e GameMode );
char		*GAMEMODE_GetF1Texture( GAMEMODE_e GameMode );
void		GAMEMODE_DetermineGameMode( void );
void		GAMEMODE_RespawnDeadSpectatorsAndPopQueue( void );
void		GAMEMODE_RespawnAllPlayers( void );
void		GAMEMODE_ResetPlayersKillCount( const bool bInformClients );

// [BB] This function doesn't really belong here. Find a better place for it.
void		GAMEMODE_DisplayStandardMessage( const char *pszMessage );
void		GAMEMODE_DisplayCNTRMessage( const char *pszMessage, const bool bInformClients, const ULONG ulPlayerExtra = MAXPLAYERS, const ULONG ulFlags = 0 );
void		GAMEMODE_DisplaySUBSMessage( const char *pszMessage, const bool bInformClients, const ULONG ulPlayerExtra = MAXPLAYERS, const ULONG ulFlags = 0 );

GAMEMODE_e	GAMEMODE_GetCurrentMode( void );
void		GAMEMODE_SetCurrentMode( GAMEMODE_e GameMode );
MODIFIER_e	GAMEMODE_GetModifier( void );
void		GAMEMODE_SetModifier( MODIFIER_e Modifier );

#endif // __GAMEMODE_H__
