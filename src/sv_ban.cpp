//-----------------------------------------------------------------------------
//
// Skulltag Source
// Copyright (C) 2003 Brad Carney
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
// Filename: sv_ban.cpp
//
// Description: Contains variables and routines related to the server ban
// of the program.
//
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <errno.h>

#include "c_dispatch.h"
#include "network.h"
#include "sv_ban.h"
#include "v_text.h"

//*****************************************************************************
//	PROTOTYPES

static	char	serverban_SkipWhitespace( FILE *pFile );
static	char	serverban_SkipComment( FILE *pFile );
static	bool	serverban_ParseNextLine( FILE *pFile );
static	void	serverban_LoadBans( void );
static	void	serverban_CouldNotOpenFile( char *pszFunctionHeader, char *pszName, LONG lErrorCode );

//*****************************************************************************
//	VARIABLES

static	char	g_cCurChar;
static	BAN_t	g_ServerBans[MAX_SERVER_BANS];
static	LONG	g_lBanIdx;
static	ULONG	g_ulBanReParseTicker;

//*****************************************************************************
//	CONSOLE VARIABLES

CVAR( Bool, sv_enforcebans, true, CVAR_ARCHIVE )
CVAR( Int, sv_banfilereparsetime, 0, CVAR_ARCHIVE )
CUSTOM_CVAR( String, sv_banfile, "banlist.txt", CVAR_ARCHIVE )
{
	ULONG		ulIdx;
	UCVarValue	Val;

	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
		return;

	for ( ulIdx = 0; ulIdx < MAX_SERVER_BANS; ulIdx++ )
	{
		sprintf( g_ServerBans[ulIdx].szBannedIP[0], "0" );
		sprintf( g_ServerBans[ulIdx].szBannedIP[1], "0" );
		sprintf( g_ServerBans[ulIdx].szBannedIP[2], "0" );
		sprintf( g_ServerBans[ulIdx].szBannedIP[3], "0" );

		g_ServerBans[ulIdx].szComment[0] = 0;
	}

	// Load the banfile, and initialize the bans.
	serverban_LoadBans( );

	// Re-parse the ban file in a certain amount of time.
	Val = sv_banfilereparsetime.GetGenericRep( CVAR_Int );
	g_ulBanReParseTicker = Val.Int * TICRATE;
}

//*****************************************************************************
//	FUNCTIONS

void SERVERBAN_Tick( void )
{
	ULONG		ulIdx;
	UCVarValue	Val;

	if ( g_ulBanReParseTicker )
	{
		// Re-parse the ban list.
		if (( --g_ulBanReParseTicker ) == 0 )
		{
			Val = sv_banfilereparsetime.GetGenericRep( CVAR_Int );
			g_ulBanReParseTicker = Val.Int * TICRATE;

			for ( ulIdx = 0; ulIdx < MAX_SERVER_BANS; ulIdx++ )
			{
				sprintf( g_ServerBans[ulIdx].szBannedIP[0], "0" );
				sprintf( g_ServerBans[ulIdx].szBannedIP[1], "0" );
				sprintf( g_ServerBans[ulIdx].szBannedIP[2], "0" );
				sprintf( g_ServerBans[ulIdx].szBannedIP[3], "0" );

				g_ServerBans[ulIdx].szComment[0] = 0;
			}

			// Load the banfile, and initialize the bans.
			serverban_LoadBans( );
		}
	}
}

//*****************************************************************************
//
bool SERVERBAN_IsIPBanned( char *pszIP0, char *pszIP1, char *pszIP2, char *pszIP3 )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAX_SERVER_BANS; ulIdx++ )
	{
		if ((( g_ServerBans[ulIdx].szBannedIP[0][0] == '*' ) || ( stricmp( pszIP0, g_ServerBans[ulIdx].szBannedIP[0] ) == 0 )) &&
			(( g_ServerBans[ulIdx].szBannedIP[1][0] == '*' ) || ( stricmp( pszIP1, g_ServerBans[ulIdx].szBannedIP[1] ) == 0 )) &&
			(( g_ServerBans[ulIdx].szBannedIP[2][0] == '*' ) || ( stricmp( pszIP2, g_ServerBans[ulIdx].szBannedIP[2] ) == 0 )) &&
			(( g_ServerBans[ulIdx].szBannedIP[3][0] == '*' ) || ( stricmp( pszIP3, g_ServerBans[ulIdx].szBannedIP[3] ) == 0 )))
		{
			return ( true );
		}
	}

	return ( false );
}

//*****************************************************************************
//
ULONG SERVERBAN_DoesBanExist( char *pszIP0, char *pszIP1, char *pszIP2, char *pszIP3 )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAX_SERVER_BANS; ulIdx++ )
	{
		if (( stricmp( pszIP0, g_ServerBans[ulIdx].szBannedIP[0] ) == 0 ) &&
			( stricmp( pszIP1, g_ServerBans[ulIdx].szBannedIP[1] ) == 0 ) &&
			( stricmp( pszIP2, g_ServerBans[ulIdx].szBannedIP[2] ) == 0 ) &&
			( stricmp( pszIP3, g_ServerBans[ulIdx].szBannedIP[3] ) == 0 ))
		{
			return ( ulIdx );
		}
	}

	return ( MAX_SERVER_BANS );
}

//*****************************************************************************
//
void SERVERBAN_AddBan( char *pszIP0, char *pszIP1, char *pszIP2, char *pszIP3, char *pszPlayerName, char *pszComment )
{
	FILE		*pFile;
	UCVarValue	Val;
	char		szOutString[512];
	ULONG		ulBanIdx;

	Val = sv_banfile.GetGenericRep( CVAR_String );

	// Address is already banned.
	ulBanIdx = SERVERBAN_DoesBanExist( pszIP0, pszIP1, pszIP2, pszIP3 );
	if ( ulBanIdx != MAX_SERVER_BANS )
	{
		Printf( "Ban for %s.%s.%s.%s already exists.\n", pszIP0, pszIP1, pszIP2, pszIP3 );
		return;
	}

	szOutString[0] = 0;
	if ( pszPlayerName )
	{
		sprintf( szOutString, "%s", szOutString, pszPlayerName );
		if ( pszComment )
			sprintf( szOutString, "%s:", szOutString );
	}
	if ( pszComment )
		sprintf( szOutString, "%s%s", szOutString, pszComment );

	// Add the ban and comment into memory.
	sprintf( g_ServerBans[g_lBanIdx].szBannedIP[0], pszIP0 );
	sprintf( g_ServerBans[g_lBanIdx].szBannedIP[1], pszIP1 );
	sprintf( g_ServerBans[g_lBanIdx].szBannedIP[2], pszIP2 );
	sprintf( g_ServerBans[g_lBanIdx].szBannedIP[3], pszIP3 );
	sprintf( g_ServerBans[g_lBanIdx].szComment, "%s", szOutString );

	// Finally, append the banfile.
	if ( pFile = fopen( Val.String, "a" ))
	{
		sprintf( szOutString, "\n%s.%s.%s.%s", pszIP0, pszIP1, pszIP2, pszIP3 );
		if ( pszPlayerName )
			sprintf( szOutString, "%s:%s", szOutString, pszPlayerName );
		if ( pszComment )
			sprintf( szOutString, "%s:%s", szOutString, pszComment );
		fputs( szOutString, pFile );
		fclose( pFile );

		Printf( "Ban for %s.%s.%s.%s added.\n", pszIP0, pszIP1, pszIP2, pszIP3 );
	}
}

//*****************************************************************************
//
bool SERVERBAN_StringToBan( char *pszAddress, char *pszIP0, char *pszIP1, char *pszIP2, char *pszIP3 )
{
	char	szCopy[16];
	char	*pszCopy;
	char	szTemp[4];
	char	*pszTemp;
	ULONG	ulIdx;
	ULONG	ulNumPeriods;

	// Too long.
	if ( strlen( pszAddress ) > 15 )
		return ( false );

	// First, get rid of anything after the colon (if it exists).
	strcpy( szCopy, pszAddress );
	for ( pszCopy = szCopy; *pszCopy; pszCopy++ )
	{
		if ( *pszCopy == ':' )
		{
			*pszCopy = 0;
			break;
		}
	}

	// Next, make sure there's at least 3 periods.
	ulNumPeriods = 0;
	for ( pszCopy = szCopy; *pszCopy; pszCopy++ )
	{
		if ( *pszCopy == '.' )
			ulNumPeriods++;
	}

	// If there weren't 3 periods, then it's not a valid ban string.
	if ( ulNumPeriods != 3 )
		return ( false );

	ulIdx = 0;
	pszTemp = szTemp;
	*pszTemp = 0;
	for ( pszCopy = szCopy; *pszCopy; pszCopy++ )
	{
		if ( *pszCopy == '.' )
		{
			// Shouldn't happen.
			if ( ulIdx > 3 )
				return ( false );

			switch ( ulIdx )
			{
			case 0:

				strcpy( pszIP0, szTemp );
				break;
			case 1:

				strcpy( pszIP1, szTemp );
				break;
			case 2:

				strcpy( pszIP2, szTemp );
				break;
			case 3:

				strcpy( pszIP3, szTemp );
				break;
			}
			ulIdx++;
//			strcpy( szBan[ulIdx++], szTemp );
			pszTemp = szTemp;
		}
		else
		{
			*pszTemp++ = *pszCopy;
			*pszTemp = 0;
			if ( strlen( szTemp ) > 3 )
				return ( false );
		}
	}

	strcpy( pszIP3, szTemp );

	// Finally, make sure each entry of our string is valid.
	if ((( atoi( pszIP0 ) < 0 ) || ( atoi( pszIP0 ) > 255 )) && ( stricmp( "*", pszIP0 ) != 0 ))
		return ( false );
	if ((( atoi( pszIP1 ) < 0 ) || ( atoi( pszIP1 ) > 255 )) && ( stricmp( "*", pszIP1 ) != 0 ))
		return ( false );
	if ((( atoi( pszIP2 ) < 0 ) || ( atoi( pszIP2 ) > 255 )) && ( stricmp( "*", pszIP2 ) != 0 ))
		return ( false );
	if ((( atoi( pszIP3 ) < 0 ) || ( atoi( pszIP3 ) > 255 )) && ( stricmp( "*", pszIP3 ) != 0 ))
		return ( false );

    return ( true );
}

//*****************************************************************************
//
void SERVERBAN_ClearBans( void )
{
	ULONG		ulIdx;
	FILE		*pFile;
	UCVarValue	Val;

	// Clear out the existing bans in memory.
	for ( ulIdx = 0; ulIdx < MAX_SERVER_BANS; ulIdx++ )
	{
		sprintf( g_ServerBans[ulIdx].szBannedIP[0], "0" );
		sprintf( g_ServerBans[ulIdx].szBannedIP[1], "0" );
		sprintf( g_ServerBans[ulIdx].szBannedIP[2], "0" );
		sprintf( g_ServerBans[ulIdx].szBannedIP[3], "0" );

		g_ServerBans[ulIdx].szComment[0] = 0;
	}

	// Also, export the new (cleared) banlist.
	Val = sv_banfile.GetGenericRep( CVAR_String );
	if (( pFile = fopen( Val.String, "w" )) != NULL )
	{
		char	szString[1024];

		sprintf( szString, "%s", "// This is the Skulltag server ban list.\n// Below are the IP addresses of those who pissed the admin off.\n" );
		fputs( szString, pFile );
		fclose( pFile );

		Printf( "Banlist cleared.\n" );
	}
	else
		serverban_CouldNotOpenFile( "SERVERBAN_ClearBans", Val.String, errno );
}

//*****************************************************************************
/*
LONG SERVERBAN_GetNumBans( void )
{
	ULONG	ulIdx;
	LONG	lNumBans;

	lNumBans = 0;
	for ( ulIdx = 0; ulIdx < MAX_SERVER_BANS; ulIdx++ )
	{
		if (( stricmp( g_ServerBans[ulIdx].szBannedIP[0], "0" ) != 0 ) ||
			( stricmp( g_ServerBans[ulIdx].szBannedIP[1], "0" ) != 0 ) ||
			( stricmp( g_ServerBans[ulIdx].szBannedIP[2], "0" ) != 0 ) ||
			( stricmp( g_ServerBans[ulIdx].szBannedIP[3], "0" ) != 0 ))
		{
			lNumBans++;
		}
	}

	return ( lNumBans );
}
*/
//*****************************************************************************
//
BAN_t SERVERBAN_GetBan( ULONG ulIdx )
{
	if ( ulIdx >= MAX_SERVER_BANS )
	{
		BAN_t	ZeroBan;

		sprintf( ZeroBan.szBannedIP[0], "0" );
		sprintf( ZeroBan.szBannedIP[1], "0" );
		sprintf( ZeroBan.szBannedIP[2], "0" );
		sprintf( ZeroBan.szBannedIP[3], "0" );

		ZeroBan.szComment[0] = 0;

		return ( ZeroBan );
	}

	return ( g_ServerBans[ulIdx] );
}

//*****************************************************************************
//*****************************************************************************
//
static char serverban_SkipWhitespace( FILE *pFile )
{
	g_cCurChar = fgetc( pFile );
	while (( g_cCurChar == ' ' ) && ( g_cCurChar != -1 ))
		g_cCurChar = fgetc( pFile );

	return ( g_cCurChar );
}

//*****************************************************************************
//
static char serverban_SkipComment( FILE *pFile )
{
	g_cCurChar = fgetc( pFile );
	while (( g_cCurChar != '\r' ) && ( g_cCurChar != '\n' ) && ( g_cCurChar != -1 ))
		g_cCurChar = fgetc( pFile );

	return ( g_cCurChar );
}

//*****************************************************************************
//
static bool serverban_ParseNextLine( FILE *pFile )
{
	NETADDRESS_s	BanAddress;
	char			szIP[256];
	char			lPosition;

	lPosition = 0;
	szIP[0] = 0;

	g_cCurChar = fgetc( pFile );

	// Skip whitespace.
	if ( g_cCurChar == ' ' )
	{
		g_cCurChar = serverban_SkipWhitespace( pFile );

		if ( feof( pFile ))
		{
			fclose( pFile );
			return ( false );
		}
	}

	while ( 1 )
	{
		if ( g_cCurChar == '\r' || g_cCurChar == '\n' || g_cCurChar == ':' || g_cCurChar == '/' || g_cCurChar == -1 )
		{
			if ( lPosition > 0 )
			{
				if ( SERVERBAN_StringToBan( szIP, g_ServerBans[g_lBanIdx].szBannedIP[0], g_ServerBans[g_lBanIdx].szBannedIP[1], g_ServerBans[g_lBanIdx].szBannedIP[2], g_ServerBans[g_lBanIdx].szBannedIP[3] ))
				{
					if ( g_lBanIdx == MAX_SERVER_BANS )
					{
						Printf( "serverban_ParseNextLine: WARNING! Maximum number of bans (%d) exceeded!\n", MAX_SERVER_BANS );
						return ( false );
					}

					g_lBanIdx++;
					return ( true );
				}
				else if ( NETWORK_StringToAddress( szIP, &BanAddress ))
				{
					if ( g_lBanIdx == MAX_SERVER_BANS )
					{
						Printf( "serverban_ParseNextLine: WARNING! Maximum number of bans (%d) exceeded!\n", MAX_SERVER_BANS );
						return ( false );
					}

					itoa( BanAddress.abIP[0], g_ServerBans[g_lBanIdx].szBannedIP[0], 10 );
					itoa( BanAddress.abIP[1], g_ServerBans[g_lBanIdx].szBannedIP[1], 10 );
					itoa( BanAddress.abIP[2], g_ServerBans[g_lBanIdx].szBannedIP[2], 10 );
					itoa( BanAddress.abIP[3], g_ServerBans[g_lBanIdx].szBannedIP[3], 10 );
					g_lBanIdx++;
					return ( true );
				}
				else
				{
					g_ServerBans[g_lBanIdx].szBannedIP[0][0] = 0;
					g_ServerBans[g_lBanIdx].szBannedIP[1][0] = 0;
					g_ServerBans[g_lBanIdx].szBannedIP[2][0] = 0;
					g_ServerBans[g_lBanIdx].szBannedIP[3][0] = 0;
				}
			}

			if ( feof( pFile ))
			{
				fclose( pFile );
				return ( false );
			}
			// If we've hit a comment, skip until the end of the line (or the end of the file) and get out.
			else if ( g_cCurChar == ':' || g_cCurChar == '/' )
			{
				serverban_SkipComment( pFile );
				return ( true );
			}
			else
				return ( true );
		}

		szIP[lPosition++] = g_cCurChar;
		szIP[lPosition] = 0;

		if ( lPosition == 256 )
		{
			fclose( pFile );
			return ( false );
		}

		g_cCurChar = fgetc( pFile );
	}
}

//*****************************************************************************
//
static void serverban_LoadBans( void )
{
	UCVarValue	Val;
	FILE		*pFile;

	// Open the banfile.
	Val = sv_banfile.GetGenericRep( CVAR_String );

	g_lBanIdx = 0;
	g_cCurChar = 0;
	if (( pFile = fopen( Val.String, "r" )) != NULL )
	{
		// -1 == EOF char.
		while ( g_cCurChar != -1 )
		{
			// This function returns true if Address is banned.
			if ( serverban_ParseNextLine( pFile ) == false )
				return;
		}
	}
	else
		serverban_CouldNotOpenFile( "serverban_LoadBans", Val.String, errno );
}

//*****************************************************************************
//
static void serverban_CouldNotOpenFile( char *pszFunctionHeader, char *pszFileName, LONG lErrorCode )
{
	Printf( "%s: Couldn't open file: %s!\nREASON: ", pszFunctionHeader, pszFileName );
	switch ( lErrorCode )
	{
	case EACCES:

		Printf( "EACCES: Search permission is denied on a component of the path prefix, or the file exists and the permissions specified by mode are denied, or the file does not exist and write permission is denied for the parent directory of the file to be created.\n" );
		break;
	case EINTR:

		Printf( "EINTR: A signal was caught during fopen().\n" );
		break;
	case EISDIR:

		Printf( "EISDIR: The named file is a directory and mode requires write access.\n" );
		break;
	case EMFILE:

		Printf( "EMFILE: {OPEN_MAX} file descriptors are currently open in the calling process.\n" );
		break;
	case ENAMETOOLONG:

		Printf( "ENAMETOOLONG: Pathname resolution of a symbolic link produced an intermediate result whose length exceeds {PATH_MAX}.\n" );
		break;
	case ENFILE:

		Printf( "ENFILE: The maximum allowable number of files is currently open in the system.\n" );
		break;
	case ENOENT:

		Printf( "ENOENT: A component of filename does not name an existing file or filename is an empty string.\n" );
		break;
	case ENOSPC:

		Printf( "ENOSPC: The directory or file system that would contain the new file cannot be expanded, the file does not exist, and it was to be created.\n" );
		break;
	case ENOTDIR:

		Printf( "ENOTDIR: A component of the path prefix is not a directory.\n" );
		break;
	case ENXIO:

		Printf( "ENXIO: The named file is a character special or block special file, and the device associated with this special file does not exist.\n" );
		break;
	case EROFS:

		Printf( "EROFS: The named file resides on a read-only file system and mode requires write access.\n" );
		break;
	case EINVAL:

		Printf( "EINVAL: The value of the mode argument is not valid.\n" );
		break;
	case ENOMEM:

		Printf( "ENOMEM: Insufficient storage space is available.\n" );
		break;
//	case EOVERFLOW:
//	case ETXTBSY:
//	case ELOOP:
	default:

		Printf( "UNKNOWN\n" );
		break;
	}
}

//*****************************************************************************
//	CONSOLE COMMANDS

CCMD( getIP )
{
	ULONG	ulIdx;
	char	szPlayerName[64];

	// Only the server can look this up.
	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
		return;

	if ( argv.argc( ) < 2 )
	{
		Printf( "Usage: getIP <playername> \n" );
		return;
	}

	// Loop through all the players, and try to find one that matches the given name.
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( playeringame[ulIdx] == false )
			continue;

		// Removes the color codes from the player name so it appears as the server sees it in the window.
		sprintf( szPlayerName, players[ulIdx].userinfo.netname );
		V_RemoveColorCodes( szPlayerName );

		if ( stricmp( szPlayerName, argv[1] ) == 0 )
		{
			// Bots do not have IPs.
			if ( players[ulIdx].bIsBot )
			{
				Printf( "That user is a bot.\n" );
				return;
			}

			Printf("IP is: %d.%d.%d.%d\n", 
				SERVER_GetClient( ulIdx )->Address.abIP[0],
				SERVER_GetClient( ulIdx )->Address.abIP[1],
				SERVER_GetClient( ulIdx )->Address.abIP[2],
				SERVER_GetClient( ulIdx )->Address.abIP[3]);
			return;
		}
	}
	
	// Didn't find a player that matches the name.
	Printf( "Unknown player: %s\n", argv[1] );
	return;
}

//*****************************************************************************
//
CCMD( getIP_idx )
{
	ULONG	ulIdx;
	char	szPlayerName[64];

	// Only the server can look this up.
	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
		return;

	if ( argv.argc( ) < 2 )
	{
		Printf( "Usage: getIP_idx <player index>\nYou can get the list of players and indexes with the ccmd playerinfo.\n" );
		return;
	}

	ulIdx = atoi(argv[1]);

	if ( playeringame[ulIdx] == false )
		return;

	// Bots do not have IPs.
	if ( players[ulIdx].bIsBot )
	{
		Printf( "That user is a bot.\n" );
		return;
	}

	Printf("IP is: %d.%d.%d.%d\n", 
		SERVER_GetClient( ulIdx )->Address.abIP[0],
		SERVER_GetClient( ulIdx )->Address.abIP[1],
		SERVER_GetClient( ulIdx )->Address.abIP[2],
		SERVER_GetClient( ulIdx )->Address.abIP[3]);
	return;

}

//*****************************************************************************
//
CCMD( ban )
{
	ULONG	ulIdx;
	char	szPlayerName[64];
	char	szBanAddress[4][4];

	// Only the server can ban players!
	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
		return;

	if ( argv.argc( ) < 2 )
	{
		Printf( "Usage: ban <playername> [reason]\n" );
		return;
	}

	// Loop through all the players, and try to find one that matches the given name.
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( playeringame[ulIdx] == false )
			continue;

		// Removes the color codes from the player name so it appears as the server sees it in the window.
		sprintf( szPlayerName, players[ulIdx].userinfo.netname );
		V_RemoveColorCodes( szPlayerName );

		if ( stricmp( szPlayerName, argv[1] ) == 0 )
		{
			char	szString[128];

			// Can't ban a bot!
			if ( players[ulIdx].bIsBot )
			{
				Printf( "You cannot ban a bot!\n" );
				return;
			}

			itoa( SERVER_GetClient( ulIdx )->Address.abIP[0], szBanAddress[0], 10 );
			itoa( SERVER_GetClient( ulIdx )->Address.abIP[1], szBanAddress[1], 10 );
			itoa( SERVER_GetClient( ulIdx )->Address.abIP[2], szBanAddress[2], 10 );
			itoa( SERVER_GetClient( ulIdx )->Address.abIP[3], szBanAddress[3], 10 );
			// Add the new ban and kick the player.
			if ( argv.argc( ) >= 3 )
			{
				SERVERBAN_AddBan( szBanAddress[0], szBanAddress[1], szBanAddress[2], szBanAddress[3], argv[1], argv[2] );
				sprintf( szString, "kick \"%s\" \"%s\"", argv[1], argv[2] );
			}
			else
			{
				SERVERBAN_AddBan( szBanAddress[0], szBanAddress[1], szBanAddress[2], szBanAddress[3], argv[1], NULL );
				sprintf( szString, "kick \"%s\"", argv[1] );
			}

			SERVER_AddCommand( szString );
			return;
		}
	}

	// Didn't find a player that matches the name.
	Printf( "Unknown player: %s\n", argv[1] );
	return;
}

//*****************************************************************************
//
CCMD( addban )
{
	NETADDRESS_s	BanAddress;
	char			szStringBan[4][4];

	if ( argv.argc( ) < 2 )
	{
		Printf( "Usage: addban <IP address> [comment]\n" );
		return;
	}

	if ( SERVERBAN_StringToBan( argv[1], szStringBan[0], szStringBan[1], szStringBan[2], szStringBan[3] ))
	{
		if ( argv.argc( ) >= 3 )
			SERVERBAN_AddBan( szStringBan[0], szStringBan[1], szStringBan[2], szStringBan[3], NULL, argv[2] );
		else
			SERVERBAN_AddBan( szStringBan[0], szStringBan[1], szStringBan[2], szStringBan[3], NULL, NULL );
	}
	else if ( NETWORK_StringToAddress( argv[1], &BanAddress ))
	{
		itoa( BanAddress.abIP[0], szStringBan[0], 10 );
		itoa( BanAddress.abIP[1], szStringBan[1], 10 );
		itoa( BanAddress.abIP[2], szStringBan[2], 10 );
		itoa( BanAddress.abIP[3], szStringBan[3], 10 );

		if ( argv.argc( ) >= 3 )
			SERVERBAN_AddBan( szStringBan[0], szStringBan[1], szStringBan[2], szStringBan[3], NULL, argv[2] );
		else
			SERVERBAN_AddBan( szStringBan[0], szStringBan[1], szStringBan[2], szStringBan[3], NULL, NULL );
	}
	else
		Printf( "Invalid ban string: %s\n", argv[1] );
}

//*****************************************************************************
//
CCMD( viewbanlist )
{
	ULONG		ulIdx;

	for ( ulIdx = 0; ulIdx < MAX_SERVER_BANS; ulIdx++ )
	{
		if (( stricmp( g_ServerBans[ulIdx].szBannedIP[0], "0" ) != 0 ) ||
			( stricmp( g_ServerBans[ulIdx].szBannedIP[1], "0" ) != 0 ) ||
			( stricmp( g_ServerBans[ulIdx].szBannedIP[2], "0" ) != 0 ) ||
			( stricmp( g_ServerBans[ulIdx].szBannedIP[3], "0" ) != 0 ))
		{
			Printf( "%s.%s.%s.%s", g_ServerBans[ulIdx].szBannedIP[0],
				g_ServerBans[ulIdx].szBannedIP[1],
				g_ServerBans[ulIdx].szBannedIP[2],
				g_ServerBans[ulIdx].szBannedIP[3] );
			if ( g_ServerBans[ulIdx].szComment[0] )
				Printf( ":%s", g_ServerBans[ulIdx].szComment );
			Printf( "\n" );
		}
	}
}

//*****************************************************************************
//
CCMD( clearbans )
{
	SERVERBAN_ClearBans( );
}

//*****************************************************************************
//
CCMD( reloadbans )
{
	ULONG		ulIdx;

	for ( ulIdx = 0; ulIdx < MAX_SERVER_BANS; ulIdx++ )
	{
		sprintf( g_ServerBans[ulIdx].szBannedIP[0], "0" );
		sprintf( g_ServerBans[ulIdx].szBannedIP[1], "0" );
		sprintf( g_ServerBans[ulIdx].szBannedIP[2], "0" );
		sprintf( g_ServerBans[ulIdx].szBannedIP[3], "0" );

		g_ServerBans[ulIdx].szComment[0] = 0;
	}

	// Load the banfile, and initialize the bans.
	serverban_LoadBans( );
}
