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
// File created:  8/27/03
//
//
// Filename: sv_master.cpp
//
// Description: Server-to-Master and Server-to-Launcher protocol.
//
//-----------------------------------------------------------------------------

#include "networkheaders.h"
#include "c_dispatch.h"
#include "cooperative.h"
#include "deathmatch.h"
#include "doomstat.h"
#include "d_player.h"
#include "duel.h"
#include "g_game.h"
#include "gamemode.h"
#include "gi.h"
#include "g_level.h"
#include "i_system.h"
#include "lastmanstanding.h"
#include "team.h"
#include "network.h"
#include "sv_main.h"
#include "sv_ban.h"
#include "version.h"

//--------------------------------------------------------------------------------------------------------------------------------------------------
//-- VARIABLES -------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------

// Address of master server.
static	NETADDRESS_s		g_AddressMasterServer;

// Message buffer for sending messages to the master server.
static	NETBUFFER_s			g_MasterServerBuffer;

// Port the master server is located on.
static	USHORT				g_usMasterPort;

// List of IP address that this server has been queried by recently.
static	STORED_QUERY_IP_s	g_StoredQueryIPs[MAX_STORED_QUERY_IPS];

static	LONG				g_lStoredQueryIPHead;
static	LONG				g_lStoredQueryIPTail;

extern	NETADDRESS_s		g_LocalAddress;

//*****************************************************************************
//	CONSOLE VARIABLES

#if (BUILD_ID != BUILD_RELEASE)
// [BB] Name of the testing binary archive found in http://zandronum.com/
CVAR( String, sv_testingbinary, "downloads/testing/" GAMEVER_STRING "/ZandroDev" GAMEVER_STRING "-" SVN_REVISION_STRING "windows.zip", CVAR_SERVERINFO )
#endif

//--------------------------------------------------------------------------------------------------------------------------------------------------
//-- FUNCTIONS -------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------

//*****************************************************************************
//
void SERVER_MASTER_Construct( void )
{
	char	*pszPort;

	// Setup our message buffer.
	NETWORK_InitBuffer( &g_MasterServerBuffer, MAX_UDP_PACKET, BUFFERTYPE_WRITE );
	NETWORK_ClearBuffer( &g_MasterServerBuffer );

	// Allow the user to specify which port the master server is on.
	pszPort = Args->CheckValue( "-masterport" );
    if ( pszPort )
    {
       g_usMasterPort = atoi( pszPort );
       Printf( PRINT_HIGH, "Alternate master server port: %d.\n", g_usMasterPort );
    }
	else 
	   g_usMasterPort = DEFAULT_MASTER_PORT;

	g_lStoredQueryIPHead = 0;
	g_lStoredQueryIPTail = 0;

	// Call SERVER_MASTER_Destruct() when Skulltag closes.
	atterm( SERVER_MASTER_Destruct );
}

//*****************************************************************************
//
void SERVER_MASTER_Destruct( void )
{
	// Free our local buffer.
	NETWORK_FreeBuffer( &g_MasterServerBuffer );
}

//*****************************************************************************
//
void SERVER_MASTER_Tick( void )
{
	UCVarValue		Val;

	while (( g_lStoredQueryIPHead != g_lStoredQueryIPTail ) && ( gametic >= g_StoredQueryIPs[g_lStoredQueryIPHead].lNextAllowedGametic ))
	{
		g_lStoredQueryIPHead++;
		g_lStoredQueryIPHead = g_lStoredQueryIPHead % MAX_STORED_QUERY_IPS;
	}

	// Send an update to the master server every 30 seconds.
	if ( gametic % ( TICRATE * 30 ))
		return;

	// User doesn't wish to update the master server.
	if ( sv_updatemaster == false )
		return;

	NETWORK_ClearBuffer( &g_MasterServerBuffer );

	Val = masterhostname.GetGenericRep( CVAR_String );

	// [BB] If we can't find the master address, we can't tick the master.
	if ( NETWORK_StringToAddress( Val.String, &g_AddressMasterServer ) == false )
	{
		Printf ( "Warning: Can't find masterhostname %s! Either correct masterhostname or set sv_updatemaster to false.\n", Val.String );
		return;
	}

	NETWORK_SetAddressPort( g_AddressMasterServer, g_usMasterPort );

	// Write to our packet a challenge to the master server.
	NETWORK_WriteLong( &g_MasterServerBuffer.ByteStream, SERVER_MASTER_CHALLENGE );
	// [BB] Also send a string that will allow us to verify that a master banlist was actually sent from the master.
	NETWORK_WriteString( &g_MasterServerBuffer.ByteStream, SERVER_GetMasterBanlistVerificationString().GetChars() );
	// [BB] Also tell the master whether we are enforcing its ban list.
	NETWORK_WriteByte( &g_MasterServerBuffer.ByteStream, sv_enforcemasterbanlist );
	// [BB] And tell which code revision number the server was built with.
	NETWORK_WriteLong( &g_MasterServerBuffer.ByteStream, SVN_REVISION_NUMBER );

	// Send the master server our packet.
//	NETWORK_LaunchPacket( &g_MasterServerBuffer, g_AddressMasterServer, true );
	NETWORK_LaunchPacket( &g_MasterServerBuffer, g_AddressMasterServer );
}

//*****************************************************************************
//
void SERVER_MASTER_Broadcast( void )
{
	NETADDRESS_s	AddressBroadcast;
	sockaddr_in		broadcast_addr;

	// Send an update to the master server every second.
	if ( gametic % TICRATE )
		return;

	// User doesn't wish to broadcast this server.
	if (( sv_broadcast == false ) || ( Args->CheckParm( "-nobroadcast" )))
		return;

//	NETWORK_ClearBuffer( &g_MasterServerBuffer );

	broadcast_addr.sin_family = AF_INET;
	broadcast_addr.sin_addr.s_addr = INADDR_BROADCAST;
	broadcast_addr.sin_port = htons( DEFAULT_BROADCAST_PORT );
	NETWORK_SocketAddressToNetAddress( &broadcast_addr, &AddressBroadcast );

	// [BB] Under all Windows versions broadcasts to INADDR_BROADCAST seem to work fine
	// while class A broadcasts don't work under Vista/7. So just use INADDR_BROADCAST.
#ifndef _WIN32
	// [BB] Based on the local adress, we find out the class
	// of the network, we are in and set the broadcast address
	// accordingly. Broadcasts to INADDR_BROADCAST = 255.255.255.255
	// should be circumvented if possible and is seem that they
	// aren't	even permitted in the Linux kernel at all.
	// If the server has the ip A.B.C.D depending on the class
	// broadcasts should go to:
	// Class A: A.255.255.255
	// Class B: A. B .255.255
	// Class C: A. B . C .255
	// 
	// Class A comprises networks 1.0.0.0 through 127.0.0.0. The network number is contained in the first octet.
	// Class B contains networks 128.0.0.0 through 191.255.0.0; the network number is in the first two octets.
	// Class C networks range from 192.0.0.0 through 223.255.255.0, with the network number contained in the first three octets.

	int classIndex = 0;

	const int locIP0 = g_LocalAddress.abIP[0];
	if ( (locIP0 >= 1) && (locIP0 <= 127) )
		classIndex = 1;
	else if ( (locIP0 >= 128 ) && (locIP0 <= 191) )
		classIndex = 2;
	else if ( (locIP0 >= 192 ) && (locIP0 <= 223) )
		classIndex = 3;

	for( int i = 0; i < classIndex; i++ )
		AddressBroadcast.abIP[i] = g_LocalAddress.abIP[i];
#endif

	// Broadcast our packet.
	SERVER_MASTER_SendServerInfo( AddressBroadcast, SQF_ALL, 0, true );
//	NETWORK_WriteLong( &g_MasterServerBuffer, MASTER_CHALLENGE );
//	NETWORK_LaunchPacket( g_MasterServerBuffer, AddressBroadcast, true );
}

//*****************************************************************************
//
void SERVER_MASTER_SendServerInfo( NETADDRESS_s Address, ULONG ulFlags, ULONG ulTime, bool bBroadcasting )
{
	UCVarValue	Val;
	char		szAddress[4][4];
	ULONG		ulIdx;
	ULONG		ulBits;

	// Let's just use the master server buffer! It gets cleared again when we need it anyway!
	NETWORK_ClearBuffer( &g_MasterServerBuffer );

	if ( bBroadcasting == false )
	{
		// First, check to see if we've been queried by this address recently.
		if ( g_lStoredQueryIPHead != g_lStoredQueryIPTail )
		{
			ulIdx = g_lStoredQueryIPHead;
			while ( ulIdx != (ULONG)g_lStoredQueryIPTail )
			{
				// Check to see if this IP exists in our stored query IP list. If it does, then
				// ignore it, since it queried us less than 10 seconds ago.
				if ( NETWORK_CompareAddress( Address, g_StoredQueryIPs[ulIdx].Address, true ))
				{
					// Write our header.
					NETWORK_WriteLong( &g_MasterServerBuffer.ByteStream, SERVER_LAUNCHER_IGNORING );

					// Send the time the launcher sent to us.
					NETWORK_WriteLong( &g_MasterServerBuffer.ByteStream, ulTime );

					// Send the packet.
	//				NETWORK_LaunchPacket( &g_MasterServerBuffer, Address, true );
					NETWORK_LaunchPacket( &g_MasterServerBuffer, Address );

					if ( sv_showlauncherqueries )
						Printf( "Ignored IP launcher challenge.\n" );

					// Nothing more to do here.
					return;
				}

				ulIdx++;
				ulIdx = ulIdx % MAX_STORED_QUERY_IPS;
			}
		}
	
		// Now, check to see if this IP has been banend from this server.
		NETWORK_AddressToIPStringArray( Address, szAddress );
		if ( SERVERBAN_IsIPBanned( szAddress ))
		{
			// Write our header.
			NETWORK_WriteLong( &g_MasterServerBuffer.ByteStream, SERVER_LAUNCHER_BANNED );

			// Send the time the launcher sent to us.
			NETWORK_WriteLong( &g_MasterServerBuffer.ByteStream, ulTime );

			// Send the packet.
			NETWORK_LaunchPacket( &g_MasterServerBuffer, Address );

			if ( sv_showlauncherqueries )
				Printf( "Denied BANNED IP launcher challenge.\n" );

			// Nothing more to do here.
			return;
		}

		// This IP didn't exist in the list. and it wasn't banned. 
		// So, add it, and keep it there for 10 seconds.
		g_StoredQueryIPs[g_lStoredQueryIPTail].Address = Address;
		g_StoredQueryIPs[g_lStoredQueryIPTail].lNextAllowedGametic = gametic + ( TICRATE * ( sv_queryignoretime ));

		g_lStoredQueryIPTail++;
		g_lStoredQueryIPTail = g_lStoredQueryIPTail % MAX_STORED_QUERY_IPS;
		if ( g_lStoredQueryIPTail == g_lStoredQueryIPHead )
			Printf( "SERVER_MASTER_SendServerInfo: WARNING! g_lStoredQueryIPTail == g_lStoredQueryIPHead\n" );
	}

	// Write our header.
	NETWORK_WriteLong( &g_MasterServerBuffer.ByteStream, SERVER_LAUNCHER_CHALLENGE );

	// Send the time the launcher sent to us.
	NETWORK_WriteLong( &g_MasterServerBuffer.ByteStream, ulTime );

	// Send our version.
	NETWORK_WriteString( &g_MasterServerBuffer.ByteStream, DOTVERSIONSTR_REV );

	// Send the information about the data that will be sent.
	ulBits = ulFlags;

	// [BB] Remove all unknown flags from our answer.
	ulBits &= SQF_ALL;

	// If the launcher desires to know the team damage, but we're not in a game mode where
	// team damage applies, then don't send back team damage information.
	if (( teamplay || teamgame || teamlms || teampossession || (( deathmatch == false ) && ( teamgame == false ))) == false )
	{
		if ( ulBits & SQF_TEAMDAMAGE )
			ulBits &= ~SQF_TEAMDAMAGE;
	}

	// If the launcher desires to know the team score, but we're not in a game mode where
	// teams have scores, then don't send back team score information.
	if (( GAMEMODE_GetFlags( GAMEMODE_GetCurrentMode( )) & GMF_PLAYERSONTEAMS ) == false )
	{
		if ( ulBits & SQF_TEAMSCORES )
			ulBits &= ~SQF_TEAMSCORES;

		// [CW] Don't send these either.
		{
			if ( ulBits & SQF_TEAMINFO_NUMBER )
				ulBits &= ~SQF_TEAMINFO_NUMBER;
			if ( ulBits & SQF_TEAMINFO_NAME )
				ulBits &= ~SQF_TEAMINFO_NAME;
			if ( ulBits & SQF_TEAMINFO_COLOR )
				ulBits &= ~SQF_TEAMINFO_COLOR;
			if ( ulBits & SQF_TEAMINFO_SCORE )
				ulBits &= ~SQF_TEAMINFO_SCORE;
		}
	}

	// If the launcher wants to know player data, then we have to tell them how many players
	// are in the server.
	if ( ulBits & SQF_PLAYERDATA )
		ulBits |= SQF_NUMPLAYERS;

	NETWORK_WriteLong( &g_MasterServerBuffer.ByteStream, ulBits );

	// Send the server name.
	if ( ulBits & SQF_NAME )
	{
		Val = sv_hostname.GetGenericRep( CVAR_String );
		NETWORK_WriteString( &g_MasterServerBuffer.ByteStream, Val.String );
	}

	// Send the website URL.
	if ( ulBits & SQF_URL )
	{
		Val = sv_website.GetGenericRep( CVAR_String );
		NETWORK_WriteString( &g_MasterServerBuffer.ByteStream, Val.String );
	}

	// Send the host's e-mail address.
	if ( ulBits & SQF_EMAIL )
	{
		Val = sv_hostemail.GetGenericRep( CVAR_String );
		NETWORK_WriteString( &g_MasterServerBuffer.ByteStream, Val.String );
	}

	if ( ulBits & SQF_MAPNAME )
		NETWORK_WriteString( &g_MasterServerBuffer.ByteStream, level.mapname );

	if ( ulBits & SQF_MAXCLIENTS )
		NETWORK_WriteByte( &g_MasterServerBuffer.ByteStream, sv_maxclients );

	if ( ulBits & SQF_MAXPLAYERS )
		NETWORK_WriteByte( &g_MasterServerBuffer.ByteStream, sv_maxplayers );

	// Send out the PWAD information.
	if ( ulBits & SQF_PWADS )
	{
		NETWORK_WriteByte( &g_MasterServerBuffer.ByteStream, NETWORK_GetPWADList( )->size( ));
		for( std::list<std::pair<FString, FString> >::iterator i = NETWORK_GetPWADList( )->begin( ); i != NETWORK_GetPWADList( )->end(); ++i )
			NETWORK_WriteString( &g_MasterServerBuffer.ByteStream, i->first.GetChars( ));
	}

	if ( ulBits & SQF_GAMETYPE )
	{
		NETWORK_WriteByte( &g_MasterServerBuffer.ByteStream, GAMEMODE_GetCurrentMode( ));
		NETWORK_WriteByte( &g_MasterServerBuffer.ByteStream, instagib );
		NETWORK_WriteByte( &g_MasterServerBuffer.ByteStream, buckshot );
	}

	if ( ulBits & SQF_GAMENAME )
		NETWORK_WriteString( &g_MasterServerBuffer.ByteStream, SERVER_MASTER_GetGameName( ));

	if ( ulBits & SQF_IWAD )
		NETWORK_WriteString( &g_MasterServerBuffer.ByteStream, NETWORK_GetIWAD( ));

	if ( ulBits & SQF_FORCEPASSWORD )
		NETWORK_WriteByte( &g_MasterServerBuffer.ByteStream, sv_forcepassword );

	if ( ulBits & SQF_FORCEJOINPASSWORD )
		NETWORK_WriteByte( &g_MasterServerBuffer.ByteStream, sv_forcejoinpassword );

	if ( ulBits & SQF_GAMESKILL )
		NETWORK_WriteByte( &g_MasterServerBuffer.ByteStream, gameskill );

	if ( ulBits & SQF_BOTSKILL )
		NETWORK_WriteByte( &g_MasterServerBuffer.ByteStream, botskill );

	if ( ulBits & SQF_DMFLAGS )
	{
		NETWORK_WriteLong( &g_MasterServerBuffer.ByteStream, dmflags );
		NETWORK_WriteLong( &g_MasterServerBuffer.ByteStream, dmflags2 );
		NETWORK_WriteLong( &g_MasterServerBuffer.ByteStream, compatflags );
	}

	if ( ulBits & SQF_LIMITS )
	{
		NETWORK_WriteShort( &g_MasterServerBuffer.ByteStream, fraglimit );
		NETWORK_WriteShort( &g_MasterServerBuffer.ByteStream, static_cast<SHORT>(timelimit) );
		// [BB] We have to base the decision on whether to send "time left" on the same rounded
		// timelimit value we just sent to the client.
		if ( static_cast<SHORT>(timelimit) )
		{
			LONG	lTimeLeft;

			lTimeLeft = (LONG)( timelimit - ( level.time / ( TICRATE * 60 )));
			if ( lTimeLeft < 0 )
				lTimeLeft = 0;
			NETWORK_WriteShort( &g_MasterServerBuffer.ByteStream, lTimeLeft );
		}
		NETWORK_WriteShort( &g_MasterServerBuffer.ByteStream, duellimit );
		NETWORK_WriteShort( &g_MasterServerBuffer.ByteStream, pointlimit );
		NETWORK_WriteShort( &g_MasterServerBuffer.ByteStream, winlimit );
	}

	// Send the team damage scale.
	if ( teamplay || teamgame || teamlms || teampossession || (( deathmatch == false ) && ( teamgame == false )))
	{
		if ( ulBits & SQF_TEAMDAMAGE )
			NETWORK_WriteFloat( &g_MasterServerBuffer.ByteStream, teamdamage );
	}

	if ( GAMEMODE_GetFlags( GAMEMODE_GetCurrentMode( )) & GMF_PLAYERSONTEAMS )
	{
		// [CW] This command is now deprecated as there are now more than two teams.
		// Send the team scores.
		if ( ulBits & SQF_TEAMSCORES )
		{
			for ( ulIdx = 0; ulIdx < 2; ulIdx++ )
			{
				if ( GAMEMODE_GetFlags(GAMEMODE_GetCurrentMode()) & GMF_PLAYERSEARNFRAGS )
					NETWORK_WriteShort( &g_MasterServerBuffer.ByteStream, TEAM_GetFragCount( ulIdx ));
				else if ( GAMEMODE_GetFlags(GAMEMODE_GetCurrentMode()) & GMF_PLAYERSEARNWINS )
					NETWORK_WriteShort( &g_MasterServerBuffer.ByteStream, TEAM_GetWinCount( ulIdx ));
				else
					NETWORK_WriteShort( &g_MasterServerBuffer.ByteStream, TEAM_GetScore( ulIdx ));
			}
		}
	}

	if ( ulBits & SQF_NUMPLAYERS )
		NETWORK_WriteByte( &g_MasterServerBuffer.ByteStream, SERVER_CalcNumPlayers( ));

	if ( ulBits & SQF_PLAYERDATA )
	{
		for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
		{
			if ( playeringame[ulIdx] == false )
				continue;

			NETWORK_WriteString( &g_MasterServerBuffer.ByteStream, players[ulIdx].userinfo.netname );
			if ( GAMEMODE_GetFlags(GAMEMODE_GetCurrentMode()) & GMF_PLAYERSEARNPOINTS )
				NETWORK_WriteShort( &g_MasterServerBuffer.ByteStream, players[ulIdx].lPointCount );
			else if ( GAMEMODE_GetFlags(GAMEMODE_GetCurrentMode()) & GMF_PLAYERSEARNWINS )
				NETWORK_WriteShort( &g_MasterServerBuffer.ByteStream, players[ulIdx].ulWins );
			else if ( GAMEMODE_GetFlags(GAMEMODE_GetCurrentMode()) & GMF_PLAYERSEARNFRAGS )
				NETWORK_WriteShort( &g_MasterServerBuffer.ByteStream, players[ulIdx].fragcount );
			else
				NETWORK_WriteShort( &g_MasterServerBuffer.ByteStream, players[ulIdx].killcount );

			NETWORK_WriteShort( &g_MasterServerBuffer.ByteStream, players[ulIdx].ulPing );
			NETWORK_WriteByte( &g_MasterServerBuffer.ByteStream, PLAYER_IsTrueSpectator( &players[ulIdx] ));
			NETWORK_WriteByte( &g_MasterServerBuffer.ByteStream, players[ulIdx].bIsBot );

			if ( GAMEMODE_GetFlags( GAMEMODE_GetCurrentMode( )) & GMF_PLAYERSONTEAMS )
			{
				if ( players[ulIdx].bOnTeam == false )
					NETWORK_WriteByte( &g_MasterServerBuffer.ByteStream, 255 );
				else
					NETWORK_WriteByte( &g_MasterServerBuffer.ByteStream, players[ulIdx].ulTeam );
			}

			NETWORK_WriteByte( &g_MasterServerBuffer.ByteStream, players[ulIdx].ulTime / ( TICRATE * 60 ));
		}
	}

	if ( GAMEMODE_GetFlags( GAMEMODE_GetCurrentMode( )) & GMF_PLAYERSONTEAMS )
	{
		if ( ulBits & SQF_TEAMINFO_NUMBER )
			NETWORK_WriteByte( &g_MasterServerBuffer.ByteStream, TEAM_GetNumAvailableTeams( ));

		if ( ulBits & SQF_TEAMINFO_NAME )
			for ( ulIdx = 0; ulIdx < TEAM_GetNumAvailableTeams( ); ulIdx++ )
				NETWORK_WriteString( &g_MasterServerBuffer.ByteStream, TEAM_GetName( ulIdx ));

		if ( ulBits & SQF_TEAMINFO_COLOR )
			for ( ulIdx = 0; ulIdx < TEAM_GetNumAvailableTeams( ); ulIdx++ )
				NETWORK_WriteLong( &g_MasterServerBuffer.ByteStream, TEAM_GetColor( ulIdx ));

		if ( ulBits & SQF_TEAMINFO_SCORE )
		{
			for ( ulIdx = 0; ulIdx < TEAM_GetNumAvailableTeams( ); ulIdx++ )
			{
				if ( GAMEMODE_GetFlags(GAMEMODE_GetCurrentMode()) & GMF_PLAYERSEARNFRAGS )
					NETWORK_WriteShort( &g_MasterServerBuffer.ByteStream, TEAM_GetFragCount( ulIdx ));
				else if ( GAMEMODE_GetFlags(GAMEMODE_GetCurrentMode()) & GMF_PLAYERSEARNWINS )
					NETWORK_WriteShort( &g_MasterServerBuffer.ByteStream, TEAM_GetWinCount( ulIdx ));
				else
					NETWORK_WriteShort( &g_MasterServerBuffer.ByteStream, TEAM_GetScore( ulIdx ));
			}
		}
	}

	// [BB] Testing server and what's the binary name?
	if ( ulBits & SQF_TESTING_SERVER )
	{
#if ( BUILD_ID == BUILD_RELEASE )
		NETWORK_WriteByte( &g_MasterServerBuffer.ByteStream, 0 );
		NETWORK_WriteString( &g_MasterServerBuffer.ByteStream, "" );
#else
		NETWORK_WriteByte( &g_MasterServerBuffer.ByteStream, 1 );
		NETWORK_WriteString( &g_MasterServerBuffer.ByteStream, sv_testingbinary.GetGenericRep( CVAR_String ).String );
#endif
	}

	// [BB] We don't have a mandatory main data file anymore, so just send an empty string.
	if ( ulBits & SQF_DATA_MD5SUM )
		NETWORK_WriteString( &g_MasterServerBuffer.ByteStream, "" );

	// [BB] Send all dmflags and compatflags.
	if ( ulBits & SQF_ALL_DMFLAGS )
	{
		NETWORK_WriteByte( &g_MasterServerBuffer.ByteStream, 5 );
		NETWORK_WriteLong( &g_MasterServerBuffer.ByteStream, dmflags );
		NETWORK_WriteLong( &g_MasterServerBuffer.ByteStream, dmflags2 );
		NETWORK_WriteLong( &g_MasterServerBuffer.ByteStream, dmflags3 );
		NETWORK_WriteLong( &g_MasterServerBuffer.ByteStream, compatflags );
		NETWORK_WriteLong( &g_MasterServerBuffer.ByteStream, zacompatflags );
	}

	// [BB] Send special security settings like sv_enforcemasterbanlist.
	if ( ulBits & SQF_SECURITY_SETTINGS )
		NETWORK_WriteByte( &g_MasterServerBuffer.ByteStream, sv_enforcemasterbanlist );

//	NETWORK_LaunchPacket( &g_MasterServerBuffer, Address, true );
	NETWORK_LaunchPacket( &g_MasterServerBuffer, Address );
}

//*****************************************************************************
//
const char *SERVER_MASTER_GetGameName( void )
{	
	switch ( gameinfo.gametype )
	{
	case GAME_Doom:

		if ( !(gameinfo.flags & GI_MAPxx) )
			return ( "DOOM" );
		else
			return ( "DOOM II" );
		break;
	case GAME_Heretic:

		return ( "Heretic" );
		break;
	case GAME_Hexen:

		return ( "Hexen" );
		break;
	default:
		
		return ( "ERROR!" );
		break;
	}
}

//*****************************************************************************
//
NETADDRESS_s SERVER_MASTER_GetMasterAddress( void )
{
	return g_AddressMasterServer;
}

//*****************************************************************************
//
void SERVER_MASTER_HandleVerificationRequest( BYTESTREAM_s *pByteStream  )
{
	LONG lVerificationNumber = NETWORK_ReadLong( pByteStream );

	NETWORK_ClearBuffer( &g_MasterServerBuffer );
	NETWORK_WriteLong( &g_MasterServerBuffer.ByteStream, SERVER_MASTER_VERIFICATION );
	NETWORK_WriteString( &g_MasterServerBuffer.ByteStream, SERVER_GetMasterBanlistVerificationString().GetChars() );
	NETWORK_WriteLong( &g_MasterServerBuffer.ByteStream, lVerificationNumber );

	// [BB] Send the master server our packet.
	NETWORK_LaunchPacket( &g_MasterServerBuffer, SERVER_MASTER_GetMasterAddress () );
}

//*****************************************************************************
//
void SERVER_MASTER_SendBanlistReceipt ( void )
{
	NETWORK_ClearBuffer( &g_MasterServerBuffer );
	NETWORK_WriteLong( &g_MasterServerBuffer.ByteStream, SERVER_MASTER_BANLIST_RECEIPT );
	NETWORK_WriteString( &g_MasterServerBuffer.ByteStream, SERVER_GetMasterBanlistVerificationString().GetChars() );

	// [BB] Send the master server our packet.
	NETWORK_LaunchPacket( &g_MasterServerBuffer, SERVER_MASTER_GetMasterAddress () );
}

//--------------------------------------------------------------------------------------------------------------------------------------------------
//-- CONSOLE ---------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------

// [BB] Unless all the declarations of the SERVERCONSOLE_* functions are moved to a platfrom independent header,
// we need to keep those awful declarations everywhere.
void SERVERCONSOLE_UpdateBroadcasting( void );
void SERVERCONSOLE_UpdateTitleString( const char *pszString );
// Should the server inform the master server of its existence?
CUSTOM_CVAR( Bool, sv_updatemaster, true, CVAR_SERVERINFO )
{
	SERVERCONSOLE_UpdateBroadcasting( );
}

// Should the server broadcast so LAN clients can hear it?
CUSTOM_CVAR( Bool, sv_broadcast, true, CVAR_ARCHIVE )
{
	SERVERCONSOLE_UpdateBroadcasting( );
}

// Name of this server on launchers.
CUSTOM_CVAR( String, sv_hostname, "Unnamed " GAMENAME " server", CVAR_ARCHIVE )
{
	SERVERCONSOLE_UpdateTitleString( (const char *)self );
}

// Website that has the wad this server is using, possibly with other info.
CVAR( String, sv_website, "", CVAR_ARCHIVE )

// E-mail address of the person running this server.
CVAR( String, sv_hostemail, "", CVAR_ARCHIVE )

// IP address of the master server.
// [BB] Client and server use this now, therefore the name doesn't begin with "sv_"
CVAR( String, masterhostname, "master.zandronum.com", CVAR_ARCHIVE|CVAR_GLOBALCONFIG )

CCMD( wads )
{
	Printf( "IWAD: %s\n", NETWORK_GetIWAD( ) );
	Printf( "Num PWADs: %d\n", static_cast<unsigned int> (NETWORK_GetPWADList( )->size( )));
	for( std::list<std::pair<FString, FString> >::iterator i = NETWORK_GetPWADList( )->begin( ); i != NETWORK_GetPWADList( )->end( ); ++i )
		Printf( "PWAD: %s - %s\n", i->first.GetChars(), i->second.GetChars() );
}
