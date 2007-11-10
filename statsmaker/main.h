//-----------------------------------------------------------------------------
//
// Skulltag Statistics Reporter Source
// Copyright (C) 2005 Brad Carney
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
// Date created:  4/4/05
//
//
// Filename: main.cpp
//
// Description: 
//
//-----------------------------------------------------------------------------


#ifndef __MAIN_H__
#define __MAIN_H__
#define _WIN32_IE 0x0501
#include "network.h"

//*****************************************************************************
//	DEFINES

#define	MAIN_TITLESTRING	"Skulltag Statistics Reporter"

#define	MAX_NUM_SERVERS		256

#define	SERVERCONSOLE_TEXTLENGTH	4096

#define	DESIRED_FRAMERATE	50
/*
#define	SECOND				1000
#define	MINUTE				( SECOND * 60 )
#define	HOUR				( MINUTE * 60 )
#define	DAY					( HOUR * 24 )
*/

#define	SECOND				50
#define	MINUTE				( SECOND * 60 )
#define	HOUR				( MINUTE * 60 )
#define	DAY					( HOUR * 24 )

// Server query flags.
#define	SQF_NAME					0x00000001
#define	SQF_URL						0x00000002
#define	SQF_EMAIL					0x00000004
#define	SQF_MAPNAME					0x00000008
#define	SQF_MAXCLIENTS				0x00000010
#define	SQF_MAXPLAYERS				0x00000020
#define	SQF_PWADS					0x00000040
#define	SQF_GAMETYPE				0x00000080
#define	SQF_GAMENAME				0x00000100
#define	SQF_IWAD					0x00000200
#define	SQF_FORCEPASSWORD			0x00000400
#define	SQF_FORCEJOINPASSWORD		0x00000800
#define	SQF_GAMESKILL				0x00001000
#define	SQF_BOTSKILL				0x00002000
#define	SQF_DMFLAGS					0x00004000
#define	SQF_LIMITS					0x00010000
#define	SQF_TEAMDAMAGE				0x00020000
#define	SQF_TEAMSCORES				0x00040000
#define	SQF_NUMPLAYERS				0x00080000
#define	SQF_PLAYERDATA				0x00100000

//*****************************************************************************
enum
{
	AS_INACTIVE,
	AS_WAITINGFORREPLY,
	AS_ACTIVE,
//	AS_IGNORED,
	AS_BANNED,

	NUM_ACTIVESTATES
};

//*****************************************************************************
enum
{
	GAMETYPE_COOPERATIVE,
	GAMETYPE_SURVIVAL,
	GAMETYPE_INVASION,
	GAMETYPE_DEATHMATCH,
	GAMETYPE_TEAMPLAY,
	GAMETYPE_DUEL,
	GAMETYPE_TERMINATOR,
	GAMETYPE_LASTMANSTANDING,
	GAMETYPE_TEAMLMS,
	GAMETYPE_POSSESSION,
	GAMETYPE_TEAMPOSSESSION,
	GAMETYPE_TEAMGAME,
	GAMETYPE_CTF,
	GAMETYPE_ONEFLAGCTF,
	GAMETYPE_SKULLTAG,

	NUM_GAMETYPES
};

//*****************************************************************************
//	STRUCTURES

typedef struct
{
	// What's the state of this server's activity?
	ULONG		ulActiveState;

	// IP address of this server.
	NETADDRESS_s	Address;

	// Name of the server.
	char		szHostName[256];

	// Gametype of the server.
	LONG		lGameType;

	// Number of players on the server.
	LONG		lNumPlayers;

	// Version of the server.
	char		szVersion[32];

} SERVERINFO_s;

//*****************************************************************************
typedef struct
{
	LONG		lMonth;

	LONG		lDay;

	LONG		lHour;

	LONG		lMinute;

	LONG		lSecond;

} UPDATETIME_s;

//*****************************************************************************
//	PROTOTYPES

BOOL CALLBACK	MAIN_MainDialogBoxCallback( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam );

DWORD WINAPI	MAIN_RunMainThread( LPVOID );

void	Printf( const char *pszString, ... );
void	VPrintf( const char *pszString, va_list Parms );
void	MAIN_Print( char *pszString );

#endif	// __MAIN_H__