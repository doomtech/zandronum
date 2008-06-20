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
//#include <vector>

#include "c_dispatch.h"
#include "network.h"
#include "sv_ban.h"
#include "v_text.h"

//*****************************************************************************
//	PROTOTYPES

static	void	serverban_LoadBans( void );
static	void	serverban_LoadBansAndBanExemptions( void );

//*****************************************************************************
//	VARIABLES

static	IPList	g_ServerBans;
static	IPList	g_ServerBanExemptions;
static	ULONG	g_ulBanReParseTicker;

//*****************************************************************************
//	CONSOLE VARIABLES

CVAR( Bool, sv_enforcebans, true, CVAR_ARCHIVE )
CVAR( Int, sv_banfilereparsetime, 0, CVAR_ARCHIVE )
CUSTOM_CVAR( String, sv_banfile, "banlist.txt", CVAR_ARCHIVE )
{
	UCVarValue	Val;

	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
		return;

	g_ServerBans.clear();

	// Load the banfile, and initialize the bans.
	serverban_LoadBans( );

	// Re-parse the ban file in a certain amount of time.
	Val = sv_banfilereparsetime.GetGenericRep( CVAR_Int );
	g_ulBanReParseTicker = Val.Int * TICRATE;
}
CUSTOM_CVAR( String, sv_banexemptionfile, "whitelist.txt", CVAR_ARCHIVE )
{
	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
		return;

	if ( !(g_ServerBanExemptions.clearAndLoadFromFile( sv_banexemptionfile.GetGenericRep( CVAR_String ).String )) )
		Printf( "%s", g_ServerBanExemptions.getErrorMessage() );
}

//*****************************************************************************
//	FUNCTIONS

void SERVERBAN_Tick( void )
{
	UCVarValue	Val;

	if ( g_ulBanReParseTicker )
	{
		// Re-parse the ban list.
		if (( --g_ulBanReParseTicker ) == 0 )
		{
			Val = sv_banfilereparsetime.GetGenericRep( CVAR_Int );
			g_ulBanReParseTicker = Val.Int * TICRATE;

			g_ServerBans.clear();

			// Load the banfile, and initialize the bans.
			serverban_LoadBansAndBanExemptions( );
		}
	}
}

//*****************************************************************************
//
bool SERVERBAN_IsIPBanned( char *pszIP0, char *pszIP1, char *pszIP2, char *pszIP3 )
{
	if( g_ServerBanExemptions.isIPInList( pszIP0, pszIP1, pszIP2, pszIP3 ) )
		return false;
	else
		return g_ServerBans.isIPInList( pszIP0, pszIP1, pszIP2, pszIP3 );
}

//*****************************************************************************
//
ULONG SERVERBAN_DoesBanExist( char *pszIP0, char *pszIP1, char *pszIP2, char *pszIP3 )
{
	return g_ServerBans.doesEntryExist( pszIP0, pszIP1, pszIP2, pszIP3 );
}

//*****************************************************************************
//
void SERVERBAN_AddBan( char *pszIP0, char *pszIP1, char *pszIP2, char *pszIP3, char *pszPlayerName, char *pszComment )
{
	std::string message;
	g_ServerBans.addEntry( pszIP0, pszIP1, pszIP2, pszIP3, pszPlayerName, pszComment, message );
	Printf( "addban: %s", message.c_str() );
}

//*****************************************************************************
//
void SERVERBAN_ClearBans( void )
{
	FILE		*pFile;
	UCVarValue	Val;

	// Clear out the existing bans in memory.
	g_ServerBans.clear();

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
		Printf( "%s", GenerateCouldNotOpenFileErrorString( "SERVERBAN_ClearBans", Val.String, errno ).c_str() );
}

//*****************************************************************************

ULONG SERVERBAN_GetNumBans( void )
{
	return ( g_ServerBans.size() );
}

//*****************************************************************************
//
IPADDRESSBAN_s SERVERBAN_GetBan( ULONG ulIdx )
{
	return g_ServerBans.getEntry( ulIdx );
}

//*****************************************************************************
//
static void serverban_LoadBans( void )
{
	UCVarValue	Val;

	// Open the banfile.
	Val = sv_banfile.GetGenericRep( CVAR_String );

	// [RC] Escape backslashes so that paths can be used.
	// [BB] Why is this necessary? Paths can be used with slashes anyway.
	FString fsFilePath = Val.String;
	int index;
	int last_index = -1;

	while( true )
	{
		index = fsFilePath.IndexOf("\\");

		// Don't get stuck in the loop.
		if(( index == last_index ) || ( index  ==  last_index + 1 ))
			break;

		last_index = index;
		fsFilePath.Insert(index, '\\');
	}

	if ( !(g_ServerBans.clearAndLoadFromFile( fsFilePath.GetChars() )) )
		Printf( "%s", g_ServerBans.getErrorMessage() );
}

//*****************************************************************************
//
static void serverban_LoadBansAndBanExemptions( void )
{
	serverban_LoadBans();

	if ( !(g_ServerBanExemptions.clearAndLoadFromFile( sv_banexemptionfile.GetGenericRep( CVAR_String ).String )) )
		Printf( "%s", g_ServerBanExemptions.getErrorMessage() );
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

	// Only the server can look this up.
	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
		return;

	if ( argv.argc( ) < 2 )
	{
		Printf( "Usage: getIP_idx <player index>\nYou can get the list of players and indexes with the ccmd playerinfo.\n" );
		return;
	}

	ulIdx = atoi(argv[1]);

	// Make sure the target is valid and applicable.
	if (( ulIdx >= MAXPLAYERS ) || ( !playeringame[ulIdx] ))
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
CCMD( ban_idx )
{
	ULONG	ulIdx;
	char	szPlayerName[64];
	char	szBanAddress[4][4];

	// Only the server can ban players!
	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
		return;

	if ( argv.argc( ) < 2 )
	{
		Printf( "Usage: ban_idx <player index> [reason]\nYou can get the list of players and indexes with the ccmd playerinfo.\n" );
		return;
	}

	ulIdx =  atoi(argv[1]);

	// Make sure the target is valid and applicable.
	if (( ulIdx >= MAXPLAYERS ) || ( !playeringame[ulIdx] ))
		return;

	// Removes the color codes from the player name so it appears as the server sees it in the window.
	sprintf( szPlayerName, players[ulIdx].userinfo.netname );
	V_RemoveColorCodes( szPlayerName );

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
	char	szString[256];
	if ( argv.argc( ) >= 3 )
	{
		SERVERBAN_AddBan( szBanAddress[0], szBanAddress[1], szBanAddress[2], szBanAddress[3], szPlayerName, argv[2] );
		sprintf( szString, "kick_idx %d \"%s\"", static_cast<unsigned int> (ulIdx), argv[2] );
	}
	else
	{
		SERVERBAN_AddBan( szBanAddress[0], szBanAddress[1], szBanAddress[2], szBanAddress[3], szPlayerName, NULL );
		sprintf( szString, "kick_idx %d", static_cast<unsigned int> (ulIdx) );
	}

	SERVER_AddCommand( szString );
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
			char	szString[256];

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
// [BB] Reduce code duplication by making this exactly like addbanexemption.
// Don't change this before the final release of 97D though.
CCMD( addban )
{
	NETADDRESS_s	BanAddress;
	char			szStringBan[4][4];

	if ( argv.argc( ) < 2 )
	{
		Printf( "Usage: addban <IP address> [comment]\n" );
		return;
	}

	if ( NETWORK_StringToIP( argv[1], szStringBan[0], szStringBan[1], szStringBan[2], szStringBan[3] ))
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
CCMD( delban )
{
	if ( argv.argc( ) < 2 )
	{
		Printf( "Usage: delban <IP address>\n" );
		return;
	}

	std::string message;
	g_ServerBans.removeEntry( argv[1], message );
	Printf( "delban: %s", message.c_str() );
}

//*****************************************************************************
//
CCMD( addbanexemption )
{
	if ( argv.argc( ) < 2 )
	{
		Printf( "Usage: addbanexemption <IP address> [comment]\n" );
		return;
	}

	std::string message;
	g_ServerBanExemptions.addEntry( argv[1], NULL, (argv.argc( ) >= 3) ? argv[2] : NULL, message );
	Printf( "addbanexemption: %s", message.c_str() );
}

//*****************************************************************************
//
CCMD( delbanexemption )
{
	if ( argv.argc( ) < 2 )
	{
		Printf( "Usage: delbanexemption <IP address>\n" );
		return;
	}

	std::string message;
	g_ServerBanExemptions.removeEntry( argv[1], message );
	Printf( "delbanexemption: %s", message.c_str() );
}

//*****************************************************************************
//
CCMD( viewbanlist )
{
	ULONG		ulIdx;

	for ( ulIdx = 0; ulIdx < g_ServerBans.size(); ulIdx++ )
	{
		Printf( "%s", g_ServerBans.getEntryAsString(ulIdx).c_str() );
	}
}

//*****************************************************************************
//
CCMD( viewbanexemptionlist )
{
	for ( ULONG ulIdx = 0; ulIdx < g_ServerBanExemptions.size(); ulIdx++ )
		Printf( "%s", g_ServerBanExemptions.getEntryAsString(ulIdx).c_str() );
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
	g_ServerBans.clear();

	// Load the banfile, and initialize the bans.
	serverban_LoadBansAndBanExemptions( );
}
