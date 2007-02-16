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
// Date created:  11/26/03
//
//
// Filename: browser.cpp
//
// Description: Contains browser global variables and functions
//
//-----------------------------------------------------------------------------

#include <winsock2.h>

#define USE_WINDOWS_DWORD
#include "browser.h"
#include "c_dispatch.h"
#include "cl_main.h"
#include "deathmatch.h"
#include "doomtype.h"
#include "i_system.h"
#include "version.h"

//*****************************************************************************
//	VARIABLES

// List of all parsed servers.
static	SERVER_t	g_BrowserServerList[MAX_BROWSER_SERVERS];

// Address of master server.
static	netadr_t	g_AddressMasterServer;

// Message buffer for sending messages to the master server.
static	sizebuf_t	g_MasterServerBuffer;

// Message buffer for sending messages to each individual server.
static	sizebuf_t	g_ServerBuffer;

// Port the master server is located on.
static	LONG		g_lMasterPort;

// Are we waiting for master server response?
static	bool		g_bWaitingForMasterResponse;

//*****************************************************************************
//	PROTOTYPES

static	LONG	browser_GetNewListID( void );
static	LONG	browser_GetListIDByAddress( netadr_t Address );
static	void	browser_QueryServer( ULONG ulServer );

//*****************************************************************************
//	FUNCTIONS

void BROWSER_Construct( void )
{
	char	*pszPort;

	g_bWaitingForMasterResponse = false;

	// Setup our master server message buffer.
	NETWORK_InitBuffer( &g_MasterServerBuffer, MAX_UDP_PACKET );
	NETWORK_ClearBuffer( &g_MasterServerBuffer );

	// Setup our server message buffer.
	NETWORK_InitBuffer( &g_ServerBuffer, MAX_UDP_PACKET );
	NETWORK_ClearBuffer( &g_ServerBuffer );

	// Allow the user to specify which port the master server is on.
	pszPort = Args.CheckValue( "-masterport" );
    if ( pszPort )
    {
       g_lMasterPort = atoi( pszPort );
       Printf( PRINT_HIGH, "Alternate master server port: %d.\n", g_lMasterPort );
    }
	else 
	   g_lMasterPort = DEFAULT_MASTER_PORT;

	// Initialize the browser list.
	BROWSER_ClearServerList( );
}

//*****************************************************************************
//*****************************************************************************
//
bool BROWSER_IsActive( ULONG ulServer )
{
	if ( ulServer >= MAX_BROWSER_SERVERS )
		return ( false );

	return ( g_BrowserServerList[ulServer].ulActiveState == AS_ACTIVE );
}

//*****************************************************************************
//
bool BROWSER_IsLAN( ULONG ulServer )
{
	if ( ulServer >= MAX_BROWSER_SERVERS )
		return ( false );

	return ( g_BrowserServerList[ulServer].bLAN );
}

//*****************************************************************************
//
netadr_t BROWSER_GetAddress( ULONG ulServer )
{
	if (( ulServer >= MAX_BROWSER_SERVERS ) || ( g_BrowserServerList[ulServer].ulActiveState != AS_ACTIVE ))
	{
		netadr_t	Dummy;

		Dummy.ip[0] = 0;
		Dummy.ip[1] = 0;
		Dummy.ip[2] = 0;
		Dummy.ip[3] = 0;
		Dummy.port = 0;

		return ( Dummy );
	}

	return ( g_BrowserServerList[ulServer].Address );
}

//*****************************************************************************
//
char *BROWSER_GetHostName( ULONG ulServer )
{
	if (( ulServer >= MAX_BROWSER_SERVERS ) || ( g_BrowserServerList[ulServer].ulActiveState != AS_ACTIVE ))
		return ( " " );

	return ( g_BrowserServerList[ulServer].szHostName );
}

//*****************************************************************************
//
char *BROWSER_GetWadURL( ULONG ulServer )
{
	if (( ulServer >= MAX_BROWSER_SERVERS ) || ( g_BrowserServerList[ulServer].ulActiveState != AS_ACTIVE ))
		return ( " " );

	return ( g_BrowserServerList[ulServer].szWadURL );
}

//*****************************************************************************
//
char *BROWSER_GetEmailAddress( ULONG ulServer )
{
	if (( ulServer >= MAX_BROWSER_SERVERS ) || ( g_BrowserServerList[ulServer].ulActiveState != AS_ACTIVE ))
		return ( " " );

	return ( g_BrowserServerList[ulServer].szEmailAddress );
}

//*****************************************************************************
//
char *BROWSER_GetMapname( ULONG ulServer )
{
	if (( ulServer >= MAX_BROWSER_SERVERS ) || ( g_BrowserServerList[ulServer].ulActiveState != AS_ACTIVE ))
		return ( " " );

	return ( g_BrowserServerList[ulServer].szMapname );
}

//*****************************************************************************
//
LONG BROWSER_GetMaxClients( ULONG ulServer )
{
	if (( ulServer >= MAX_BROWSER_SERVERS ) || ( g_BrowserServerList[ulServer].ulActiveState != AS_ACTIVE ))
		return ( false );

	return ( g_BrowserServerList[ulServer].lMaxClients );
}

//*****************************************************************************
//
LONG BROWSER_GetNumPWADs( ULONG ulServer )
{
	if (( ulServer >= MAX_BROWSER_SERVERS ) || ( g_BrowserServerList[ulServer].ulActiveState != AS_ACTIVE ))
		return ( false );

	return ( g_BrowserServerList[ulServer].lNumPWADs );
}

//*****************************************************************************
//
char *BROWSER_GetPWADName( ULONG ulServer, ULONG ulWadIdx )
{
	if (( ulServer >= MAX_BROWSER_SERVERS ) || ( g_BrowserServerList[ulServer].ulActiveState != AS_ACTIVE ))
		return ( " " );

	return ( g_BrowserServerList[ulServer].szPWADNames[ulWadIdx] );
}

//*****************************************************************************
//
char *BROWSER_GetIWADName( ULONG ulServer )
{
	if (( ulServer >= MAX_BROWSER_SERVERS ) || ( g_BrowserServerList[ulServer].ulActiveState != AS_ACTIVE ))
		return ( " " );

	return ( g_BrowserServerList[ulServer].szIWADName );
}

//*****************************************************************************
//
LONG BROWSER_GetGameType( ULONG ulServer )
{
	if (( ulServer >= MAX_BROWSER_SERVERS ) || ( g_BrowserServerList[ulServer].ulActiveState != AS_ACTIVE ))
		return ( false );

	return ( g_BrowserServerList[ulServer].lGameType );
}

//*****************************************************************************
//
LONG BROWSER_GetNumPlayers( ULONG ulServer )
{
	if (( ulServer >= MAX_BROWSER_SERVERS ) || ( g_BrowserServerList[ulServer].ulActiveState != AS_ACTIVE ))
		return ( false );

	return ( g_BrowserServerList[ulServer].lNumPlayers );
}

//*****************************************************************************
//
char *BROWSER_GetPlayerName( ULONG ulServer, ULONG ulPlayer )
{
	if (( ulServer >= MAX_BROWSER_SERVERS ) || ( g_BrowserServerList[ulServer].ulActiveState != AS_ACTIVE ))
		return ( " " );

	if ( ulPlayer >= (ULONG)g_BrowserServerList[ulServer].lNumPlayers )
		return ( " " );

	return ( g_BrowserServerList[ulServer].Players[ulPlayer].szName );
}

//*****************************************************************************
//
LONG BROWSER_GetPlayerFragcount( ULONG ulServer, ULONG ulPlayer )
{
	if (( ulServer >= MAX_BROWSER_SERVERS ) || ( g_BrowserServerList[ulServer].ulActiveState != AS_ACTIVE ))
		return ( false );

	if ( ulPlayer >= (ULONG)g_BrowserServerList[ulServer].lNumPlayers )
		return ( false );

	return ( g_BrowserServerList[ulServer].Players[ulPlayer].lFragcount );
}

//*****************************************************************************
//
LONG BROWSER_GetPlayerPing( ULONG ulServer, ULONG ulPlayer )
{
	if (( ulServer >= MAX_BROWSER_SERVERS ) || ( g_BrowserServerList[ulServer].ulActiveState != AS_ACTIVE ))
		return ( false );

	if ( ulPlayer >= (ULONG)g_BrowserServerList[ulServer].lNumPlayers )
		return ( false );

	return ( g_BrowserServerList[ulServer].Players[ulPlayer].lPing );
}

//*****************************************************************************
//
LONG BROWSER_GetPlayerSpectating( ULONG ulServer, ULONG ulPlayer )
{
	if (( ulServer >= MAX_BROWSER_SERVERS ) || ( g_BrowserServerList[ulServer].ulActiveState != AS_ACTIVE ))
		return ( false );

	if ( ulPlayer >= (ULONG)g_BrowserServerList[ulServer].lNumPlayers )
		return ( false );

	return ( g_BrowserServerList[ulServer].Players[ulPlayer].bSpectating );
}

//*****************************************************************************
//
LONG BROWSER_GetPing( ULONG ulServer )
{
	if (( ulServer >= MAX_BROWSER_SERVERS ) || ( g_BrowserServerList[ulServer].ulActiveState != AS_ACTIVE ))
		return ( false );

	return ( g_BrowserServerList[ulServer].lPing );
}

//*****************************************************************************
//
char *BROWSER_GetVersion( ULONG ulServer )
{
	if (( ulServer >= MAX_BROWSER_SERVERS ) || ( g_BrowserServerList[ulServer].ulActiveState != AS_ACTIVE ))
		return ( " " );

	return ( g_BrowserServerList[ulServer].szVersion );
}

//*****************************************************************************
//*****************************************************************************
//
void BROWSER_ClearServerList( void )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAX_BROWSER_SERVERS; ulIdx++ )
	{
		g_BrowserServerList[ulIdx].ulActiveState = AS_INACTIVE;

		g_BrowserServerList[ulIdx].Address.ip[0] = 0;
		g_BrowserServerList[ulIdx].Address.ip[1] = 0;
		g_BrowserServerList[ulIdx].Address.ip[2] = 0;
		g_BrowserServerList[ulIdx].Address.ip[3] = 0;
		g_BrowserServerList[ulIdx].Address.port = 0;
	}
}

//*****************************************************************************
//
void BROWSER_DeactivateAllServers( void )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAX_BROWSER_SERVERS; ulIdx++ )
	{
		if ( g_BrowserServerList[ulIdx].ulActiveState == AS_ACTIVE )
			g_BrowserServerList[ulIdx].ulActiveState = AS_INACTIVE;
	}
}

//*****************************************************************************
//
void BROWSER_GetServerList( void )
{
	ULONG	ulServer;

	// No longer waiting for a master server response.
	g_bWaitingForMasterResponse = false;

	while ( NETWORK_ReadByte( ) != MSC_ENDSERVERLIST )
	{
		// Receiving information about a new server.
		ulServer = browser_GetNewListID( );
		if ( ulServer == -1 )
			I_Error( "BROWSER_GetServerList: Server limit exceeded (>=%d servers)", MAX_BROWSER_SERVERS );

		// This server is now active.
		g_BrowserServerList[ulServer].ulActiveState = AS_WAITINGFORREPLY;

		// Read in address information.
		g_BrowserServerList[ulServer].Address.ip[0] = NETWORK_ReadByte( );
		g_BrowserServerList[ulServer].Address.ip[1] = NETWORK_ReadByte( );
		g_BrowserServerList[ulServer].Address.ip[2] = NETWORK_ReadByte( );
		g_BrowserServerList[ulServer].Address.ip[3] = NETWORK_ReadByte( );
		g_BrowserServerList[ulServer].Address.port = htons( NETWORK_ReadShort( ));
	}
}

//*****************************************************************************
//

// geh
void M_BuildServerList( void );

void BROWSER_ParseServerQuery( bool bLAN )
{
	LONG		lGameType = GAMETYPE_COOPERATIVE;
	ULONG		ulIdx;
	LONG		lServer;
	ULONG		ulFlags;
	bool		bResortList = true;

	lServer = browser_GetListIDByAddress( g_AddressFrom );

	// If this is a LAN server and it's already on the list, there's no
	// need to resort the server list.
	if ( bLAN && ( lServer != -1 ))
		bResortList = false;

	// If this is a LAN server, and it doesn't exist on the server list, add it.
	if (( lServer == -1 ) && bLAN )
		lServer = browser_GetNewListID( );

	if ( lServer == -1 )
	{
		ULONG	ulDummyNumPlayers;
		ULONG	ulDummyNumPWADs;

		// If the version doesn't match ours, remove it from the list.
		if ( stricmp( NETWORK_ReadString( ), DOTVERSIONSTR ) != 0 )
		{
			g_BrowserServerList[lServer].ulActiveState = AS_INACTIVE;
			while ( 1 )
			{
				if ( NETWORK_ReadByte( ) == -1 )
					return;
			}
		}

		// Read in the data that will be sent to us.
		ulFlags = NETWORK_ReadLong( );

		// Read in the time we sent to the server.
		NETWORK_ReadLong( );

		// Read the server name.
		if ( ulFlags & SQF_NAME )
			NETWORK_ReadString( );

		// Read the website URL.
		if ( ulFlags & SQF_URL )
			NETWORK_ReadString( );

		// Read the host's e-mail address.
		if ( ulFlags & SQF_EMAIL )
			NETWORK_ReadString( );

		// Mapname.
		if ( ulFlags & SQF_MAPNAME )
			NETWORK_ReadString( );

		// Max clients.
		if ( ulFlags & SQF_MAXCLIENTS )
			NETWORK_ReadByte( );

		// Maximum slots.
		if ( ulFlags & SQF_MAXPLAYERS )
			NETWORK_ReadByte( );

		// Read in the PWAD information.
		if ( ulFlags & SQF_PWADS )
		{
			ulDummyNumPWADs = NETWORK_ReadByte( );
			for ( ulIdx = 0; ulIdx < ulDummyNumPWADs; ulIdx++ )
				NETWORK_ReadString( );
		}

		if ( ulFlags & SQF_GAMETYPE )
		{
			lGameType = NETWORK_ReadByte( );
			NETWORK_ReadByte( );
			NETWORK_ReadByte( );
		}

		// Game name.
		if ( ulFlags & SQF_GAMENAME )
			NETWORK_ReadString( );

		// IWAD name.
		if ( ulFlags & SQF_IWAD )
			NETWORK_ReadString( );

		// Force password.
		if ( ulFlags & SQF_FORCEPASSWORD )
			NETWORK_ReadByte( );

		// Force join password.
		if ( ulFlags & SQF_FORCEJOINPASSWORD )
			NETWORK_ReadByte( );

		// Game skill.
		if ( ulFlags & SQF_GAMESKILL )
			NETWORK_ReadByte( );

		// Bot skill.
		if ( ulFlags & SQF_BOTSKILL )
			NETWORK_ReadByte( );

		if ( ulFlags & SQF_DMFLAGS )
		{
			// DMFlags.
			NETWORK_ReadLong( );

			// DMFlags2.
			NETWORK_ReadLong( );

			// Compatflags.
			NETWORK_ReadLong( );
		}

		if ( ulFlags & SQF_LIMITS )
		{
			// Fraglimit.
			NETWORK_ReadShort( );

			// Timelimit.
			if ( NETWORK_ReadShort( ))
			{
				// Time left.
				NETWORK_ReadShort( );
			}

			// Duellimit.
			NETWORK_ReadShort( );

			// Pointlimit.
			NETWORK_ReadShort( );

			// Winlimit.
			NETWORK_ReadShort( );
		}

		// Team damage scale.
		if ( ulFlags & SQF_TEAMDAMAGE )
			NETWORK_ReadFloat( );

		if ( ulFlags & SQF_TEAMSCORES )
		{
			// Blue score.
			NETWORK_ReadShort( );

			// Red score.
			NETWORK_ReadShort( );
		}

		// Read in the number of players.
		if ( ulFlags & SQF_NUMPLAYERS )
			ulDummyNumPlayers = NETWORK_ReadByte( );
		if ( ulFlags & SQF_PLAYERDATA )
		{
			for ( ulIdx = 0; ulIdx < ulDummyNumPlayers; ulIdx++ )
			{
				// Player's name.
				NETWORK_ReadString( );

				// Fragcount, points, etc.
				NETWORK_ReadShort( );

				// Ping.
				NETWORK_ReadShort( );

				// Spectating.
				NETWORK_ReadByte( );

				// Is bot.
				NETWORK_ReadByte( );

				if (( lGameType == GAMETYPE_TEAMPLAY ) ||
					( lGameType == GAMETYPE_TEAMLMS ) ||
					( lGameType == GAMETYPE_TEAMPOSSESSION ) ||
					( lGameType == GAMETYPE_SKULLTAG ) ||
					( lGameType == GAMETYPE_CTF ) ||
					( lGameType == GAMETYPE_ONEFLAGCTF ))
				{
					// Team.
					NETWORK_ReadByte( );
				}

				// Time.
				NETWORK_ReadByte( );
			}
		}

		return;
	}

	// This server is now active.
	g_BrowserServerList[lServer].ulActiveState = AS_ACTIVE;

	// Is this a LAN server?
	g_BrowserServerList[lServer].bLAN = bLAN;

	// We heard back from this server, so calculate ping right away.
	if ( bLAN )
	{
		// If this is a LAN server, the IP address has not be set up yet.
		g_BrowserServerList[lServer].Address = g_AddressFrom;
		g_BrowserServerList[lServer].lPing = 0;
	}
	else
		g_BrowserServerList[lServer].lPing = I_MSTime( ) - g_BrowserServerList[lServer].lMSTime;

	// Read in the time we sent to the server.
	NETWORK_ReadLong( );

	// Read in the version.
	sprintf( g_BrowserServerList[lServer].szVersion, NETWORK_ReadString( ));

	// If the version doesn't match ours, remove it from the list.
	if ( stricmp( g_BrowserServerList[lServer].szVersion, DOTVERSIONSTR ) != 0 )
	{
		g_BrowserServerList[lServer].ulActiveState = AS_INACTIVE;
		while ( 1 )
		{
			if ( NETWORK_ReadByte( ) == -1 )
				return;
		}
	}

	// Read in the data that will be sent to us.
	ulFlags = NETWORK_ReadLong( );

	// Read the server name.
	if ( ulFlags & SQF_NAME )
		sprintf( g_BrowserServerList[lServer].szHostName, NETWORK_ReadString( ));

	// Read the website URL.
	if ( ulFlags & SQF_URL )
		sprintf( g_BrowserServerList[lServer].szWadURL, NETWORK_ReadString( ));

	// Read the host's e-mail address.
	if ( ulFlags & SQF_EMAIL )
		sprintf( g_BrowserServerList[lServer].szEmailAddress, NETWORK_ReadString( ));

	if ( ulFlags & SQF_MAPNAME )
		sprintf( g_BrowserServerList[lServer].szMapname, NETWORK_ReadString( ));
	if ( ulFlags & SQF_MAXCLIENTS )
		g_BrowserServerList[lServer].lMaxClients = NETWORK_ReadByte( );

	// Maximum slots.
	if ( ulFlags & SQF_MAXPLAYERS )
		NETWORK_ReadByte( );

	// Read in the PWAD information.
	if ( ulFlags & SQF_PWADS )
	{
		g_BrowserServerList[lServer].lNumPWADs = NETWORK_ReadByte( );
		for ( ulIdx = 0; ulIdx < (ULONG)g_BrowserServerList[lServer].lNumPWADs; ulIdx++ )
			sprintf( g_BrowserServerList[lServer].szPWADNames[ulIdx], NETWORK_ReadString( ));
	}

	if ( ulFlags & SQF_GAMETYPE )
	{
		g_BrowserServerList[lServer].lGameType = NETWORK_ReadByte( );
		NETWORK_ReadByte( );
		NETWORK_ReadByte( );
	}

	// Game name.
	if ( ulFlags & SQF_GAMENAME )
		NETWORK_ReadString( );

	// Read in the IWAD name.
	if ( ulFlags & SQF_IWAD )
		sprintf( g_BrowserServerList[lServer].szIWADName, NETWORK_ReadString( ));

	// Force password.
	if ( ulFlags & SQF_FORCEPASSWORD )
		NETWORK_ReadByte( );

	// Force join password.
	if ( ulFlags & SQF_FORCEJOINPASSWORD )
		NETWORK_ReadByte( );

	// Game skill.
	if ( ulFlags & SQF_GAMESKILL )
		NETWORK_ReadByte( );

	// Bot skill.
	if ( ulFlags & SQF_BOTSKILL )
		NETWORK_ReadByte( );

	if ( ulFlags & SQF_DMFLAGS )
	{
		// DMFlags.
		NETWORK_ReadLong( );

		// DMFlags2.
		NETWORK_ReadLong( );

		// Compatflags.
		NETWORK_ReadLong( );
	}

	if ( ulFlags & SQF_LIMITS )
	{
		// Fraglimit.
		NETWORK_ReadShort( );

		// Timelimit.
		if ( NETWORK_ReadShort( ))
		{
			// Time left.
			NETWORK_ReadShort( );
		}

		// Duellimit.
		NETWORK_ReadShort( );

		// Pointlimit.
		NETWORK_ReadShort( );

		// Winlimit.
		NETWORK_ReadShort( );
	}

	// Team damage scale.
	if ( ulFlags & SQF_TEAMDAMAGE )
		NETWORK_ReadFloat( );

	if ( ulFlags & SQF_TEAMSCORES )
	{
		// Blue score.
		NETWORK_ReadShort( );

		// Red score.
		NETWORK_ReadShort( );
	}

	// Read in the number of players.
	if ( ulFlags & SQF_NUMPLAYERS )
		g_BrowserServerList[lServer].lNumPlayers = NETWORK_ReadByte( );

	if ( ulFlags & SQF_PLAYERDATA )
	{
		for ( ulIdx = 0; ulIdx < (ULONG)g_BrowserServerList[lServer].lNumPlayers; ulIdx++ )
		{
			// Read in this player's name.
			sprintf( g_BrowserServerList[lServer].Players[ulIdx].szName, NETWORK_ReadString( ));

			// Read in "fragcount" (could be frags, points, etc.)
			g_BrowserServerList[lServer].Players[ulIdx].lFragcount = NETWORK_ReadShort( );

			// Read in the player's ping.
			g_BrowserServerList[lServer].Players[ulIdx].lPing = NETWORK_ReadShort( );

			// Read in whether or not the player is spectating.
			g_BrowserServerList[lServer].Players[ulIdx].bSpectating = !!NETWORK_ReadByte( );

			// Read in whether or not the player is a bot.
			g_BrowserServerList[lServer].Players[ulIdx].bIsBot = !!NETWORK_ReadByte( );

			if (( g_BrowserServerList[lServer].lGameType == GAMETYPE_TEAMPLAY ) ||
				( g_BrowserServerList[lServer].lGameType == GAMETYPE_TEAMLMS ) ||
				( g_BrowserServerList[lServer].lGameType == GAMETYPE_TEAMPOSSESSION ) ||
				( g_BrowserServerList[lServer].lGameType == GAMETYPE_SKULLTAG ) ||
				( g_BrowserServerList[lServer].lGameType == GAMETYPE_CTF ) ||
				( g_BrowserServerList[lServer].lGameType == GAMETYPE_ONEFLAGCTF ))
			{
				// Team.
				NETWORK_ReadByte( );
			}

			// Time.
			NETWORK_ReadByte( );
		}
	}

	// Now that this server has been read in, resort the servers in the menu.
	if ( bResortList )
		M_BuildServerList( );
}

//*****************************************************************************
//
void BROWSER_QueryMasterServer( void )
{
	// We are currently waiting to hear back from the master server.
	g_bWaitingForMasterResponse = true;

	// Setup the master server IP.
	NETWORK_StringToAddress( cl_masterip.GetGenericRep( CVAR_String ).String, &g_AddressMasterServer );
	I_SetPort( g_AddressMasterServer, g_lMasterPort );

	// Clear out the buffer, and write out launcher challenge.
	NETWORK_ClearBuffer( &g_MasterServerBuffer );
	NETWORK_WriteLong( &g_MasterServerBuffer, LAUNCHER_SERVER_CHALLENGE );

	// Send the master server our packet.
//	NETWORK_LaunchPacket( &g_MasterServerBuffer, g_AddressMasterServer, true );
	NETWORK_LaunchPacket( &g_MasterServerBuffer, g_AddressMasterServer );
}

//*****************************************************************************
//
bool BROWSER_WaitingForMasterResponse( void )
{
	return ( g_bWaitingForMasterResponse );
}

//*****************************************************************************
//
void BROWSER_QueryAllServers( void )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAX_BROWSER_SERVERS; ulIdx++ )
	{
		if ( g_BrowserServerList[ulIdx].ulActiveState == AS_WAITINGFORREPLY )
			browser_QueryServer( ulIdx );
	}
}

//*****************************************************************************
//
LONG BROWSER_CalcNumServers( void )
{
	ULONG	ulIdx;
	ULONG	ulNumServers;

	ulNumServers = 0;
	for ( ulIdx = 0; ulIdx < MAX_BROWSER_SERVERS; ulIdx++ )
	{
		if ( g_BrowserServerList[ulIdx].ulActiveState == AS_ACTIVE )
			ulNumServers++;
	}

	return ( ulNumServers );
}

//*****************************************************************************
//*****************************************************************************
//
static LONG browser_GetNewListID( void )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAX_BROWSER_SERVERS; ulIdx++ )
	{
		if ( g_BrowserServerList[ulIdx].ulActiveState == AS_INACTIVE )
			return ( ulIdx );
	}

	return ( -1 );
}

//*****************************************************************************
//
static LONG browser_GetListIDByAddress( netadr_t Address )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAX_BROWSER_SERVERS; ulIdx++ )
	{
		if ( NETWORK_CompareAddress( g_BrowserServerList[ulIdx].Address, Address, false ))
			return ( ulIdx );
	}

	return ( -1 );
}

//*****************************************************************************
//
static void browser_QueryServer( ULONG ulServer )
{
	// Don't query a server that we're already connected to.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) &&
		( NETWORK_CompareAddress( g_BrowserServerList[ulServer].Address, CLIENT_GetServerAddress( ), false )))
	{
		return;
	}

	// Clear out the buffer, and write out launcher challenge.
	NETWORK_ClearBuffer( &g_ServerBuffer );
	NETWORK_WriteLong( &g_ServerBuffer, LAUNCHER_SERVER_CHALLENGE );
	NETWORK_WriteLong( &g_ServerBuffer, SQF_NAME|SQF_URL|SQF_EMAIL|SQF_MAPNAME|SQF_MAXCLIENTS|SQF_PWADS|SQF_GAMETYPE|SQF_IWAD|SQF_NUMPLAYERS|SQF_PLAYERDATA );
	NETWORK_WriteLong( &g_ServerBuffer, I_MSTime( ));

	// Send the server our packet.
//	NETWORK_LaunchPacket( &g_ServerBuffer, g_BrowserServerList[ulServer].Address, true );
	NETWORK_LaunchPacket( &g_ServerBuffer, g_BrowserServerList[ulServer].Address );

	// Keep track of the time we queried this server at.
	g_BrowserServerList[ulServer].lMSTime = I_MSTime( );
}

//*****************************************************************************
//	CONSOLE VARIABLES/COMMANDS

// IP address of the master server.
CVAR( String, cl_masterip, "skulltag.kicks-ass.net", CVAR_ARCHIVE )

//*****************************************************************************
//
CCMD( dumpserverlist )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAX_BROWSER_SERVERS; ulIdx++ )
	{
		if ( g_BrowserServerList[ulIdx].ulActiveState == AS_ACTIVE )
			continue;

		Printf( "\nServer #%d\n----------------\n", ulIdx );
		Printf( "Name: %s\n", g_BrowserServerList[ulIdx].szHostName );
		Printf( "Address: %s\n", NETWORK_AddressToString( g_BrowserServerList[ulIdx].Address ));
		Printf( "Gametype: %d\n", g_BrowserServerList[ulIdx].lGameType );
		Printf( "Num PWADs: %d\n", g_BrowserServerList[ulIdx].lNumPWADs );
		Printf( "Players: %d/%d\n", g_BrowserServerList[ulIdx].lNumPlayers, g_BrowserServerList[ulIdx].lMaxClients );
		Printf( "Ping: %d\n", g_BrowserServerList[ulIdx].lPing );
	}
}
