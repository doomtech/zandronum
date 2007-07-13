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
// Filename: gamemode.cpp
//
// Description: 
//
//-----------------------------------------------------------------------------

#include "cooperative.h"
#include "deathmatch.h"
#include "gamemode.h"
#include "team.h"

//*****************************************************************************
//	VARIABLES

// Data for all our game modes.
static	GAMEMODE_s				g_GameModes[NUM_GAMEMODES];

// Our current game mode.
static	GAMEMODE_e				g_CurrentGameMode;

//*****************************************************************************
//	FUNCTIONS

void GAMEMODE_Construct( void )
{
	// Regular co-op.
	g_GameModes[GAMEMODE_COOPERATIVE].ulFlags = GMF_COOPERATIVE|GMF_PLAYERSEARNKILLS;
	strncpy( g_GameModes[GAMEMODE_COOPERATIVE].szShortName, "CO-OP", 8 );
	strncpy( g_GameModes[GAMEMODE_COOPERATIVE].szF1Texture, "F1_COOP", 8 );

	// Survival co-op.
	g_GameModes[GAMEMODE_SURVIVAL].ulFlags = GMF_COOPERATIVE|GMF_PLAYERSEARNKILLS|GMF_MAPRESETS|GMF_DEADSPECTATORS;
	strncpy( g_GameModes[GAMEMODE_SURVIVAL].szShortName, "SURV", 8 );
	strncpy( g_GameModes[GAMEMODE_SURVIVAL].szF1Texture, "F1_SCP", 8 );

	// Invasion.
	g_GameModes[GAMEMODE_INVASION].ulFlags = GMF_COOPERATIVE|GMF_PLAYERSEARNKILLS|GMF_MAPRESETS;
	strncpy( g_GameModes[GAMEMODE_INVASION].szShortName, "INVAS", 8 );
	strncpy( g_GameModes[GAMEMODE_INVASION].szF1Texture, "F1_INV", 8 );

	// Regular deathmatch.
	g_GameModes[GAMEMODE_DEATHMATCH].ulFlags = GMF_DEATHMATCH|GMF_PLAYERSEARNFRAGS;
	strncpy( g_GameModes[GAMEMODE_DEATHMATCH].szShortName, "DM", 8 );
	strncpy( g_GameModes[GAMEMODE_DEATHMATCH].szF1Texture, "F1_DM", 8 );

	// Teamplay DM.
	g_GameModes[GAMEMODE_TEAMPLAY].ulFlags = GMF_DEATHMATCH|GMF_PLAYERSEARNFRAGS|GMF_PLAYERSONTEAMS;
	strncpy( g_GameModes[GAMEMODE_TEAMPLAY].szShortName, "TDM", 8 );
	strncpy( g_GameModes[GAMEMODE_TEAMPLAY].szF1Texture, "F1_TDM", 8 );

	// Duel.
	g_GameModes[GAMEMODE_DUEL].ulFlags = GMF_DEATHMATCH|GMF_PLAYERSEARNFRAGS|GMF_MAPRESETS;
	strncpy( g_GameModes[GAMEMODE_DUEL].szShortName, "DUEL", 8 );
	strncpy( g_GameModes[GAMEMODE_DUEL].szF1Texture, "F1_DUEL", 8 );

	// Terminator DM.
	g_GameModes[GAMEMODE_TERMINATOR].ulFlags = GMF_DEATHMATCH|GMF_PLAYERSEARNFRAGS;
	strncpy( g_GameModes[GAMEMODE_TERMINATOR].szShortName, "TERM", 8 );
	strncpy( g_GameModes[GAMEMODE_TERMINATOR].szF1Texture, "F1_TERM", 8 );

	// Last man standing.
	g_GameModes[GAMEMODE_LASTMANSTANDING].ulFlags = GMF_DEATHMATCH|GMF_PLAYERSEARNWINS|GMF_DONTSPAWNMAPTHINGS|GMF_MAPRESETS|GMF_DEADSPECTATORS;
	strncpy( g_GameModes[GAMEMODE_LASTMANSTANDING].szShortName, "LMS", 8 );
	strncpy( g_GameModes[GAMEMODE_LASTMANSTANDING].szF1Texture, "F1_LMS", 8 );

	// Team LMS.
	g_GameModes[GAMEMODE_TEAMLMS].ulFlags = GMF_DEATHMATCH|GMF_PLAYERSEARNWINS|GMF_DONTSPAWNMAPTHINGS|GMF_MAPRESETS|GMF_DEADSPECTATORS|GMF_PLAYERSONTEAMS;
	strncpy( g_GameModes[GAMEMODE_TEAMLMS].szShortName, "TM LMS", 8 );
	strncpy( g_GameModes[GAMEMODE_TEAMLMS].szF1Texture, "F1_TLMS", 8 );

	// Possession DM.
	g_GameModes[GAMEMODE_POSSESSION].ulFlags = GMF_DEATHMATCH|GMF_PLAYERSEARNPOINTS;
	strncpy( g_GameModes[GAMEMODE_POSSESSION].szShortName, "POSS", 8 );
	strncpy( g_GameModes[GAMEMODE_POSSESSION].szF1Texture, "F1_POSS", 8 );

	// Team possession.
	g_GameModes[GAMEMODE_TEAMPOSSESSION].ulFlags = GMF_DEATHMATCH|GMF_PLAYERSEARNPOINTS|GMF_PLAYERSONTEAMS;
	strncpy( g_GameModes[GAMEMODE_TEAMPOSSESSION].szShortName, "TM POSS", 8 );
	strncpy( g_GameModes[GAMEMODE_TEAMPOSSESSION].szF1Texture, "F1_TPOSS", 8 );

	// Regular teamgame.
	g_GameModes[GAMEMODE_TEAMGAME].ulFlags = GMF_TEAMGAME|GMF_PLAYERSEARNPOINTS|GMF_PLAYERSONTEAMS;
	strncpy( g_GameModes[GAMEMODE_TEAMGAME].szShortName, "TM GAME", 8 );
	strncpy( g_GameModes[GAMEMODE_TEAMGAME].szF1Texture, "F1_POSS", 8 );

	// Capture the flag.
	g_GameModes[GAMEMODE_CTF].ulFlags = GMF_TEAMGAME|GMF_PLAYERSEARNPOINTS|GMF_PLAYERSONTEAMS;
	strncpy( g_GameModes[GAMEMODE_CTF].szShortName, "CTF", 8 );
	strncpy( g_GameModes[GAMEMODE_CTF].szF1Texture, "F1_CTF", 8 );

	// One flag CTF.
	g_GameModes[GAMEMODE_ONEFLAGCTF].ulFlags = GMF_TEAMGAME|GMF_PLAYERSEARNPOINTS|GMF_PLAYERSONTEAMS;
	strncpy( g_GameModes[GAMEMODE_ONEFLAGCTF].szShortName, "OFCTF", 8 );
	strncpy( g_GameModes[GAMEMODE_ONEFLAGCTF].szF1Texture, "F1_1FCTF", 8 );

	// Skulltag.
	g_GameModes[GAMEMODE_SKULLTAG].ulFlags = GMF_TEAMGAME|GMF_PLAYERSEARNPOINTS|GMF_PLAYERSONTEAMS;
	strncpy( g_GameModes[GAMEMODE_SKULLTAG].szShortName, "ST", 8 );
	strncpy( g_GameModes[GAMEMODE_SKULLTAG].szF1Texture, "F1_ST", 8 );

	// Our default game mode is co-op.
	g_CurrentGameMode = GAMEMODE_COOPERATIVE;
}

//*****************************************************************************
//
ULONG GAMEMODE_GetFlags( GAMEMODE_e GameMode )
{
	if ( GameMode >= NUM_GAMEMODES )
		return ( 0 );

	return ( g_GameModes[GameMode].ulFlags );
}

//*****************************************************************************
//
char *GAMEMODE_GetShortName( GAMEMODE_e GameMode )
{
	if ( GameMode >= NUM_GAMEMODES )
		return ( NULL );

	return ( g_GameModes[GameMode].szShortName );
}

//*****************************************************************************
//
char *GAMEMODE_GetF1Texture( GAMEMODE_e GameMode )
{
	if ( GameMode >= NUM_GAMEMODES )
		return ( NULL );

	return ( g_GameModes[GameMode].szF1Texture );
}

//*****************************************************************************
//
void GAMEMODE_DetermineGameMode( void )
{
	g_CurrentGameMode = GAMEMODE_COOPERATIVE;
	if ( survival )
		g_CurrentGameMode = GAMEMODE_SURVIVAL;
	if ( invasion )
		g_CurrentGameMode = GAMEMODE_INVASION;
	if ( deathmatch )
		g_CurrentGameMode = GAMEMODE_DEATHMATCH;
	if ( teamplay )
		g_CurrentGameMode = GAMEMODE_TEAMPLAY;
	if ( duel )
		g_CurrentGameMode = GAMEMODE_DUEL;
	if ( terminator )
		g_CurrentGameMode = GAMEMODE_TERMINATOR;
	if ( lastmanstanding )
		g_CurrentGameMode = GAMEMODE_LASTMANSTANDING;
	if ( teamlms )
		g_CurrentGameMode = GAMEMODE_TEAMLMS;
	if ( possession )
		g_CurrentGameMode = GAMEMODE_POSSESSION;
	if ( teampossession )
		g_CurrentGameMode = GAMEMODE_TEAMPOSSESSION;
	if ( teamgame )
		g_CurrentGameMode = GAMEMODE_TEAMGAME;
	if ( ctf )
		g_CurrentGameMode = GAMEMODE_CTF;
	if ( oneflagctf )
		g_CurrentGameMode = GAMEMODE_ONEFLAGCTF;
	if ( skulltag )
		g_CurrentGameMode = GAMEMODE_SKULLTAG;
}

//*****************************************************************************
//*****************************************************************************
//
GAMEMODE_e GAMEMODE_GetCurrentMode( void )
{
	return ( g_CurrentGameMode );
}

//*****************************************************************************
//
void GAMEMODE_SetCurrentMode( GAMEMODE_e GameMode )
{
	g_CurrentGameMode = GameMode;
}
