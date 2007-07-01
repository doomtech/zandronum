//-----------------------------------------------------------------------------
//
// Skulltag Master Server Source
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
// Date (re)created:  3/7/05
//
//
// Filename: main.cpp
//
// Description: 
//
//-----------------------------------------------------------------------------

#include "../src/networkheaders.h"
#include "../src/networkshared.h"
#include "network.h"
#include "main.h"

// [BB] Needed for I_GetTime.
#ifdef _MSC_VER
#include <mmsystem.h>
#endif
#ifndef _WIN32
#include <sys/time.h>
#endif

//*****************************************************************************
//	VARIABLES

// Global server list.
static	SERVER_t	g_Servers[MAX_SERVERS];

// Message buffer we write our commands to.
static	sizebuf_t	g_MessageBuffer;

static	int		    nowtime;

static	IPAddress_t		g_BannedIPs[MAX_BANNED_IPS];

static	char		g_cCurChar;

static	long		g_lCurBanIdx = 0;

// List of IP address that this server has been queried by recently.
static	STORED_QUERY_IP_t	g_StoredQueryIPs[MAX_STORED_QUERY_IPS];

static	LONG				g_lStoredQueryIPHead;
static	LONG				g_lStoredQueryIPTail;

//*****************************************************************************
//	FUNCTIONS

#ifdef _MSC_VER
int I_GetTime (void)
{
	static DWORD  basetime;
	DWORD         tm;

	tm = timeGetTime();
	if (!basetime)
		basetime = tm;

	return (tm-basetime)/1000;
}

#else

// Returns time in seconds
int I_GetTime(void)
{
        struct timeval tv;
        struct timezone tz;
        long long int thistimereply;
	static long long int basetime;	

        gettimeofday(&tv, &tz);

        thistimereply = tv.tv_sec * 1000 + tv.tv_usec / 1000;

	if (!basetime)
            basetime = thistimereply;
        
	return (thistimereply - basetime)/1000;
}

#endif

//*****************************************************************************
//
long MASTERSERVER_CheckIfServerAlreadyExists( netadr_t Address )
{
	unsigned long	ulIdx;

	for ( ulIdx = 0; ulIdx < MAX_SERVERS; ulIdx++ )
	{
		// This slot is not active. Skip over it.
		if ( g_Servers[ulIdx].bAvailable == true )
			continue;

		// The IP of the server we just heard from matches a server in our list.
		if ( NETWORK_CompareAddress( g_Servers[ulIdx].Address, Address, false ))
			return ( ulIdx );
	}

	// IP was not found in server list.
	return ( -1 );
}

//*****************************************************************************
//
long MASTERSERVER_AddServerToList( netadr_t Address )
{
	unsigned long	ulIdx;

	for ( ulIdx = 0; ulIdx < MAX_SERVERS; ulIdx++ )
	{
		// This slot is not active. Use it.
		if ( g_Servers[ulIdx].bAvailable == true )
		{
			// Slot is in use; no longer available to be slotted with a new server.
			g_Servers[ulIdx].bAvailable = false;

			// Set the server IP.
			g_Servers[ulIdx].Address = Address;

			// A slot was found; return the index.
			return ( ulIdx );
		}
	}
	
	// Could not find an available slot to put the server in.
	return ( -1 );
}

//*****************************************************************************
//
char MASTERSERVER_SkipWhitespace( FILE *pFile )
{
	g_cCurChar = fgetc( pFile );
	while (( g_cCurChar == ' ' ) && ( g_cCurChar != -1 ))
		g_cCurChar = fgetc( pFile );

	return ( g_cCurChar );
}

//*****************************************************************************
//
char MASTERSERVER_SkipComment( FILE *pFile )
{
	g_cCurChar = fgetc( pFile );
	while (( g_cCurChar != '\r' ) && ( g_cCurChar != '\n' ) && ( g_cCurChar != -1 ))
		g_cCurChar = fgetc( pFile );

	return ( g_cCurChar );
}

//*****************************************************************************
//
void MASTERSERVER_InitializeBans( void )
{
	std::cerr << "Initializing ban list...\n";

	IPFileParser parser( MAX_BANNED_IPS );
	if ( !(parser.parseIPList( "banlist.txt", g_BannedIPs )) )
		std::cerr << parser.getErrorMessage() ;
/*
	// [BB] Print all banned IPs, to make sure the IP list has been parsed successfully.
	for ( ULONG ulIdx = 0; ulIdx < MAX_BANNED_IPS; ulIdx++ )
	{
		std::cerr << g_BannedIPs[ulIdx].szIP[0] << "." << g_BannedIPs[ulIdx].szIP[1] << "." << g_BannedIPs[ulIdx].szIP[2] << "." << g_BannedIPs[ulIdx].szIP[3] << std::endl;
	}
*/
}

//*****************************************************************************
//
bool MASTERSERVER_IsIPBanned( char *pszIP0, char *pszIP1, char *pszIP2, char *pszIP3 )
{
	unsigned long	ulIdx;

	for ( ulIdx = 0; ulIdx < MAX_BANNED_IPS; ulIdx++ )
	{
		if ((( g_BannedIPs[ulIdx].szIP[0][0] == '*' ) || ( _stricmp( pszIP0, g_BannedIPs[ulIdx].szIP[0] ) == 0 )) &&
			(( g_BannedIPs[ulIdx].szIP[1][0] == '*' ) || ( _stricmp( pszIP1, g_BannedIPs[ulIdx].szIP[1] ) == 0 )) &&
			(( g_BannedIPs[ulIdx].szIP[2][0] == '*' ) || ( _stricmp( pszIP2, g_BannedIPs[ulIdx].szIP[2] ) == 0 )) &&
			(( g_BannedIPs[ulIdx].szIP[3][0] == '*' ) || ( _stricmp( pszIP3, g_BannedIPs[ulIdx].szIP[3] ) == 0 )))
		{
			return ( true );
		}
	}

	return ( false );
}

//*****************************************************************************
//
void MASTERSERVER_ParseCommands( void )
{
	long		lCommand;
	netadr_t	AddressTemp;

	lCommand = NETWORK_ReadLong( );

	// First, is this IP banned from the master server? If so, ignore the request.
	AddressTemp = g_AddressFrom;
	AddressTemp.port = 0;
	char		szAddress[4][4];
	_itoa( AddressTemp.ip[0], szAddress[0], 10 );
	_itoa( AddressTemp.ip[1], szAddress[1], 10 );
	_itoa( AddressTemp.ip[2], szAddress[2], 10 );
	_itoa( AddressTemp.ip[3], szAddress[3], 10 );
	if ( MASTERSERVER_IsIPBanned( szAddress[0], szAddress[1], szAddress[2], szAddress[3] ))
	{
		printf( "Ignoring challenge from banned IP: %s.\n", NETWORK_AddressToString( g_AddressFrom ));

		// Clear out the message buffer.
		NETWORK_ClearBuffer( &g_MessageBuffer );

		// Write our message header to the launcher.
		NETWORK_WriteLong( &g_MessageBuffer, MSC_IPISBANNED );

		// Send the launcher our packet.
		NETWORK_LaunchPacket( g_MessageBuffer, g_AddressFrom, true );
		return;
	}

	switch ( lCommand )
	{
	// Server is telling master server of its existance.
	case MASTER_CHALLENGE:
	case MASTER_CHALLENGE_OVERRIDE:

		{
			long		lServerIdx;
			netadr_t	Address;

			if ( lCommand == MASTER_CHALLENGE )
			{
				Address = g_AddressFrom;
				lServerIdx = MASTERSERVER_CheckIfServerAlreadyExists( Address );
			}
			else
			{
				char	szAddress[32];
				ULONG	ulIP1;
				ULONG	ulIP2;
				ULONG	ulIP3;
				ULONG	ulIP4;
				ULONG	ulPort;

				// Read in the data for the overridden IP the server is sending us.
				ulIP1 = NETWORK_ReadByte( );
				ulIP2 = NETWORK_ReadByte( );
				ulIP3 = NETWORK_ReadByte( );
				ulIP4 = NETWORK_ReadByte( );
				ulPort = NETWORK_ReadShort( );

				// Make sure it's valid.
				if (( ulIP1 > 255 ) ||
					( ulIP2 > 255 ) ||
					( ulIP3 > 255 ) ||
					( ulIP4 > 255 ) ||
					( ulPort > 65535 ))
				{
					printf( "Invalid overriden IP (%d.%d.%d.%d:%d) from %s.\n", ulIP1, ulIP2, ulIP3, ulIP4, ulPort, NETWORK_AddressToString( g_AddressFrom ));
					return;
				}

				// Build the IP string.
				sprintf( szAddress, "%d.%d.%d.%d:%d", ulIP1, ulIP2, ulIP3, ulIP4, ulPort );

				NETWORK_StringToAddress( szAddress, &Address );
				lServerIdx = MASTERSERVER_CheckIfServerAlreadyExists( Address );
			}
			if ( lServerIdx != -1 )
			{
				// Command is from a server already on the list. It's
				// just sending us a heartbeat.
				g_Servers[lServerIdx].lLastReceived = nowtime;
				return;
			}
			else
			{
				// This is a new server; add it to the list.
				lServerIdx = MASTERSERVER_AddServerToList( Address );
				if ( lServerIdx == -1 )
				{
					printf( "ERROR: Server list full!\n" );
					return;
				}

				g_Servers[lServerIdx].lLastReceived = nowtime;
				printf( "Server challenge from: %s.\n", NETWORK_AddressToString( g_Servers[lServerIdx].Address ));

				return;
			}
		}
	// Launcher is asking master server for server list.
	case LAUNCHER_CHALLENGE:

		{
			unsigned long	ulIdx;

			// Display the launcher challenge in the main window.
			printf( "Launcher challenge from: %s.\n", NETWORK_AddressToString( g_AddressFrom ));

			// Clear out the message buffer.
			NETWORK_ClearBuffer( &g_MessageBuffer );

			// First, check to see if we've been queried by this address recently.
			if ( g_lStoredQueryIPHead != g_lStoredQueryIPTail )
			{
				ulIdx = g_lStoredQueryIPHead;
				while ( ulIdx != (ULONG)g_lStoredQueryIPTail )
				{
					// Check to see if this IP exists in our stored query IP list. If it does, then
					// ignore it, since it queried us less than 10 seconds ago.
					if ( NETWORK_CompareAddress( g_AddressFrom, g_StoredQueryIPs[ulIdx].Address, true ))
					{
						// Write our header.
						NETWORK_WriteLong( &g_MessageBuffer, MSC_REQUESTIGNORED );

						// Send the packet.
						NETWORK_LaunchPacket( g_MessageBuffer, g_AddressFrom, true );

						printf( "Ignored launcher challenge.\n" );

						// Nothing more to do here.
						return;
					}

					ulIdx++;
					ulIdx = ulIdx % MAX_STORED_QUERY_IPS;
				}
			}
			
			// This IP didn't exist in the list. and it wasn't banned. 
			// So, add it, and keep it there for 10 seconds.
			g_StoredQueryIPs[g_lStoredQueryIPTail].Address = g_AddressFrom;
			g_StoredQueryIPs[g_lStoredQueryIPTail].lNextAllowedTime = nowtime + 10;

			g_lStoredQueryIPTail++;
			g_lStoredQueryIPTail = g_lStoredQueryIPTail % MAX_STORED_QUERY_IPS;
			if ( g_lStoredQueryIPTail == g_lStoredQueryIPHead )
				printf( "WARNING! g_lStoredQueryIPTail == g_lStoredQueryIPHead\n" );

			// Write our message header to the launcher.
			NETWORK_WriteLong( &g_MessageBuffer, MSC_BEGINSERVERLIST );

			// Now, loop through and send back the data of each active server.
			for ( ulIdx = 0; ulIdx < MAX_SERVERS; ulIdx++ )
			{
				// Not an active server.
				if ( g_Servers[ulIdx].bAvailable )
					continue;

				// Tell the launcher the IP of this server on the list.
				NETWORK_WriteByte( &g_MessageBuffer, MSC_SERVER );
				NETWORK_WriteByte( &g_MessageBuffer, g_Servers[ulIdx].Address.ip[0] );
				NETWORK_WriteByte( &g_MessageBuffer, g_Servers[ulIdx].Address.ip[1] );
				NETWORK_WriteByte( &g_MessageBuffer, g_Servers[ulIdx].Address.ip[2] );
				NETWORK_WriteByte( &g_MessageBuffer, g_Servers[ulIdx].Address.ip[3] );
				NETWORK_WriteShort( &g_MessageBuffer, ntohs( g_Servers[ulIdx].Address.port ));
			}

			// Tell the launcher that we're done sending servers.
			NETWORK_WriteByte( &g_MessageBuffer, MSC_ENDSERVERLIST );

			// Send the launcher our packet.
			NETWORK_LaunchPacket( g_MessageBuffer, g_AddressFrom, true );
			return;
		}
	}

	printf( "WARNING: Unknown challenge (%d) from: %s!\n", lCommand, NETWORK_AddressToString( g_AddressFrom ));
}

//*****************************************************************************
//
void MASTERSERVER_CheckTimeouts( void )
{
	unsigned long	ulIdx;

	for ( ulIdx = 0; ulIdx < MAX_SERVERS; ulIdx++ )
	{
		// An open slot; not an active server.
		if ( g_Servers[ulIdx].bAvailable )
			continue;

		// If the server has timed out, make it an open slot!
		if (( nowtime - g_Servers[ulIdx].lLastReceived ) >= 60 )
		{
			g_Servers[ulIdx].bAvailable = true;
			printf( "Server %s timed out.\n", NETWORK_AddressToString( g_Servers[ulIdx].Address ));
		}
	}
}

//*****************************************************************************
//
int main( )
{
	unsigned long	ulIdx;

	printf( "=== S K U L L T A G ===\n" );
	printf( "\nMaster server v1.5\n" );

	printf( "Initializing on port: %d\n", DEFAULT_MASTER_PORT );
	NETWORK_SetLocalPort( DEFAULT_MASTER_PORT );

	// Initialize the network system.
	NETWORK_Initialize( );

	for ( ulIdx = 0; ulIdx < MAX_SERVERS; ulIdx++ )
		g_Servers[ulIdx].bAvailable = true;

	// Initialize the message buffer we send messages to the launcher in.
	NETWORK_InitBuffer( &g_MessageBuffer, MAX_UDP_PACKET );
	NETWORK_ClearBuffer( &g_MessageBuffer );

	// Initialize the bans subsystem.
	MASTERSERVER_InitializeBans( );

	g_lStoredQueryIPHead = 0;
	g_lStoredQueryIPTail = 0;

	// Done setting up!
	printf( "Master server initialized!\n\n" );

	while ( 1 )
	{
		nowtime = I_GetTime( );
		I_DoSelect( );
		
		while ( NETWORK_GetPackets( ))
			MASTERSERVER_ParseCommands( );

		while (( g_lStoredQueryIPHead != g_lStoredQueryIPTail ) && ( nowtime >= g_StoredQueryIPs[g_lStoredQueryIPHead].lNextAllowedTime ))
		{
			g_lStoredQueryIPHead++;
			g_lStoredQueryIPHead = g_lStoredQueryIPHead % MAX_STORED_QUERY_IPS;
		}

		MASTERSERVER_CheckTimeouts( );
	}	
	
	return ( 0 );
}
