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
// Filename: network.cpp
//
// Description: Contains network definitions and functions not specifically
// related to the server or client.
//
//-----------------------------------------------------------------------------

// [Petteri] Check if compiling for Win32:
#if defined(__WINDOWS__) || defined(__NT__) || defined(_MSC_VER) || defined(_WIN32)
#	define __WIN32__
#endif
// Follow #ifdef __WIN32__ marks
/*
#include <stdio.h>
#ifdef	WIN32
#include <conio.h>
#endif

// [Petteri] Use Winsock for Win32:
#ifdef __WIN32__
#	define WIN32_LEAN_AND_MEAN
#	include <windows.h>
#	include <winsock.h>
#else
#	include <sys/socket.h>
#	include <netinet/in.h>
#	include <arpa/inet.h>
#	include <errno.h>
#	include <unistd.h>
#	include <netdb.h>
#	include <sys/ioctl.h>
#endif

#ifndef __WIN32__
typedef int SOCKET;
#define SOCKET_ERROR -1
#define INVALID_SOCKET -1
#define closesocket close
#define ioctlsocket ioctl
#define Sleep(x)        usleep (x * 1000)
#endif
*/
#include <winsock2.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <ctype.h>
#include <math.h>

#define USE_WINDOWS_DWORD
#include "c_dispatch.h"
#include "cl_main.h"
#include "doomtype.h"
#include "huffman.h"
#include "i_system.h"
#include "sv_main.h"
#include "m_random.h"
#include "network.h"
#include "sbar.h"

//*****************************************************************************
//	VARIABLES

static	LONG		g_lNetworkState = NETSTATE_SINGLE;
static	sizebuf_t	g_NetworkMessage;
static	netadr_t	g_LocalNetworkAddress;
/*static*/	netadr_t	g_AddressFrom;
//static	int			g_lMessagePosition;
static	SOCKET		g_NetworkSocket;
static	SOCKET		g_LANSocket;
static	USHORT		g_usLocalPort;

// Buffer for the Huffman encoding.
static	UCHAR		g_ucHuffmanBuffer[131072];

static	char	*g_pszServerHeaderNames[NUM_SERVER_COMMANDS] =
{
	"SVC_HEADER",
	"SVC_PRINT",
	"SVC_SETCONSOLEPLAYER",
	"SVC_DMFLAGS",
	"SVC_GAMESKILL",
	"SVC_GAMETYPE",
	"SVC_LIMITS",
	"SVC_LOADMAP",
	"SVC_SPAWNPLAYER",
	"SVC_DISCONNECTPLAYER",
	"SVC_SETPLAYERCHATSTATUS",
	"SVC_PLAYERSAY",
	"SVC_MOVEPLAYER",
	"SVC_UPDATELOCALPLAYER",
	"SVC_SPAWNTHING",
	"SVC_UNRELIABLEPACKET",
	"SVC_UPDATEPING",
	"SVC_PING",
	"SVC_UPDATEPLAYERUSERINFO",
	"SVC_SETPLAYERFRAGS",
	"SVC_THINGANGLE",
	"SVC_CORPSE",
	"SVC_SPAWNMISSILE",
	"SVC_EXPLODEMISSILE",
	"SVC_RESPAWNITEM",
	"SVC_DAMAGETHING",
	"SVC_DAMAGEPLAYER",
	"SVC_MOVETHING",
	"SVC_KILLTHING",
	"SVC_KILLPLAYER",
	"SVC_GIVEMEDAL",
	"SVC_TOGGLELINE",
	"SVC_TAUNT",
	"SVC_THINGSTATE",
	"SVC_FIST",
	"SVC_SAW",
	"SVC_FIREPISTOL",
	"SVC_FIRESHOTGUN",
	"SVC_FIRESUPERSHOTGUN",
	"SVC_OPENSUPERSHOTGUN",
	"SVC_LOADSUPERSHOTGUN",
	"SVC_CLOSESUPERSHOTGUN",
	"SVC_FIRECHAINGUN",
	"SVC_FIREMINIGUN",
	"SVC_FIREROCKETLAUNCHER",
	"SVC_FIREGRENADELAUNCHER",
	"SVC_FIREPLASMAGUN",
	"SVC_FIRERAILGUN",
	"SVC_FIREBFG",
	"SVC_FIREBFG10K",
	"SVC_UPDATESECTORFLAT",
	"SVC_NEWMAP",
	"SVC_EXITLEVEL",
	"SVC_SECTORSOUND",
	"SVC_SECTORSOUNDID",
//	"SVC_STOPSECTORSEQUENCE",
	"SVC_ENDLEVELDELAY",
	"SVC_UPDATETEAMFRAGS",
	"SVC_UPDATETEAMSCORES",
	"SVC_SETPLAYERTEAM",
	"SVC_TELEPORT",
	"SVC_HIDEMOBJ",
	"SVC_FLAGRETURNED",
	"SVC_REMOVEPLAYERITEM",
	"SVC_DESTROYTHING",
	"SVC_GIVETHING",
	"SVC_MIDPRINT",
	"SVC_UPDATEPLAYERPOINTS",
	"SVC_PLAYERSPECTATING",
	"SVC_WEAPONCHANGE",
	"SVC_UPDATEPLAYERKILLCOUNT",
	"SVC_SETPLAYERSTATE",
	"SVC_MOTD",
	"SVC_CONSOLEPLAYERKICKED",
	"SVC_UPDATEPLAYEREXTRA",
	"SVC_LEVELTIME",
	"SVC_NOTHING",
	"SVC_CLIENTLAGGING",
	"SVC_CLIENTNOTLAGGING",
	"SVC_SPAWNPLASMABALL",
	"SVC_SPAWNROCKET",
	"SVC_SPAWNGRENADE",
	"SVC_RESPAWNITEMNOFOG",
	"SVC_CEILINGPANNING",
	"SVC_FLOORPANNING",
	"SVC_SECTORCOLOR",
	"SVC_SECTORROTATION",
	"SVC_SECTORFADE",
	"SVC_ROTATEPOLY",
	"SVC_MOVEPOLY",
	"SVC_CREATEPOLYDOOR",
	"SVC_SPAWNFATSHOT",
	"SVC_HUDMESSAGE",
	"SVC_ACTORPROPERTY",
	"SVC_PLAYERPROPERTY",
	"SVC_SECTORLIGHTLEVEL",
	"SVC_ACTORACTIVATE",
	"SVC_ACTORDEACTIVATE",
//	"SVC_GIVEINVENTORY",
	"SVC_BEGINSNAPSHOT",
	"SVC_ENDSNAPSHOT",
	"SVC_RETURNTICKS",
	"SVC_LINEALPHA",
	"SVC_LINETEXTURE",
	"SVC_SOUND",
	"SVC_SOUNDACTOR",
	"SVC_SOUNDPOINT",
//	"SVC_USEINVENTORY",
	"SVC_PLAYERHEALTH",
//	"SVC_TAKEINVENTORY",
	"SVC_FLASHFADER",
	"SVC_CHANGEMUSIC",
	"SVC_DODOOR",
	"SVC_DOFLOOR",
	"SVC_DOCEILING",
	"SVC_DOPLAT",
	"SVC_BUILDSTAIRS",
	"SVC_GENERICCHEAT",
	"SVC_GIVECHEAT",
	"SVC_DOELEVATOR",
	"SVC_STARTWAGGLE",
	"SVC_SETFLOORPLANE",
	"SVC_SETCEILINGPLANE",
	"SVC_SPAWNBULLETPUFF",
	"SVC_DODONUT",
	"SVC_SPRINGPADZONE",
//	"SVC_ACTORFLAGS",
	"SVC_SETPOLYPOSITION",
	"SVC_FIGHT",
	"SVC_STARTCOUNTDOWN",
	"SVC_DOWINSEQUENCE",
	"SVC_UPDATEWINS",
//	"SVC_UPDATEPOWERUP",
//	"SVC_UPDATERUNE",
	"SVC_REQUESTCHECKSUM",
	"SVC_UPDATEDUELS",
//	"SVC_RANDOMPOWERUP",
	"SVC_READYTOGOON",
	"SVC_UPDATEPLAYERTIME",
	"SVC_MODESTATE",

};

//*****************************************************************************
//	PROTOTYPES

static	void	network_Error( char *pszError );
static	SOCKET	network_AllocateSocket( void );
static	bool	network_BindSocketToPort( int Socket, USHORT usPort, bool bReUse );
static	void	network_GetLocalAddress( void );

//*****************************************************************************
//	FUNCTIONS

void NETWORK_Construct( void )
{
	if ( Args.CheckParm( "-host" ))
	{
		char	*pszMaxClients;

		SERVER_Construct( );

		// If they used "-host <#>", make <#> the max number of players.
		pszMaxClients = Args.CheckValue( "-host" );
		if ( pszMaxClients )
			sv_maxclients = atoi( pszMaxClients );
	}
	else
		CLIENT_Construct( );
}

//*****************************************************************************
//
LONG NETWORK_GetState( void )
{
	return ( g_lNetworkState );
}

//*****************************************************************************
//
void NETWORK_SetState( LONG lState )
{
	if ( lState >= NUM_NETSTATES || lState < 0 )
		return;

	g_lNetworkState = lState;

	// Alert the status bar that multiplayer status has changed.
	if ( g_lNetworkState != NETSTATE_SERVER && StatusBar )
		StatusBar->MultiplayerChanged( );
}

//*****************************************************************************
//
USHORT NETWORK_GetLocalPort( void )
{
	return ( g_usLocalPort );
}

//*****************************************************************************
//
void NETWORK_SetLocalPort( USHORT usPort )
{
	g_usLocalPort = usPort;
}

//*****************************************************************************
//
void NETWORK_Initialize( void )
{
	char	szString[128];
	unsigned long _true = true;

	HuffInit( );

#ifdef __WIN32__
	WSADATA   wsad;
	int r = WSAStartup( 0x0101, &wsad );

	if (r)
		network_Error( "Winsock initialization failed!\n" );

	Printf( "Winsock initialization succeeded!\n" );
#endif

	g_NetworkSocket = network_AllocateSocket( );
	if ( network_BindSocketToPort( g_NetworkSocket, g_usLocalPort, false ) == false )
	{
		USHORT	usPort;
		bool	bSuccess;

		bSuccess = true;
		usPort = g_usLocalPort;
		while ( network_BindSocketToPort( g_NetworkSocket, ++usPort, false ) == false )
		{
			// Didn't find an available port. Oh well...
			if ( usPort == g_usLocalPort )
			{
				bSuccess = false;
				break;
			}
		}

		if ( bSuccess == false )
		{
			sprintf( szString, "network_BindSocketToPort: Couldn't bind socket to port: %d\n", g_usLocalPort );
			network_Error( szString );
		}
		else
		{
			Printf( "NETWORK_Initialize: Couldn't bind to %d. Binding to %d instead...\n", g_usLocalPort, usPort );
			g_usLocalPort = usPort;
		}
	}
	if ( ioctlsocket( g_NetworkSocket, FIONBIO, &_true ) == -1 )
		printf( "network_AllocateSocket: ioctl FIONBIO: %s", strerror( errno ));

	// If we're not starting a server, setup a socket to listen for LAN servers.
	if ( Args.CheckParm( "-host" ) == false )
	{
		g_LANSocket = network_AllocateSocket( );
		if ( network_BindSocketToPort( g_LANSocket, DEFAULT_BROADCAST_PORT, true ) == false )
		{
			sprintf( szString, "network_BindSocketToPort: Couldn't bind LAN socket to port: %d. You will not be able to see LAN servers in the browser.", DEFAULT_BROADCAST_PORT );
			network_Error( szString );
		}
		if ( ioctlsocket( g_LANSocket, FIONBIO, &_true ) == -1 )
			printf( "network_AllocateSocket: ioctl FIONBIO: %s", strerror( errno ));
	}

	// Init our read buffer.
	NETWORK_InitBuffer( &g_NetworkMessage, MAX_UDP_PACKET );
	NETWORK_ClearBuffer( &g_NetworkMessage );

	// Determine local name & address.
	network_GetLocalAddress( );

	Printf( "UDP Initialized.\n" );
}

//*****************************************************************************
//
void NETWORK_BeginReading( void )
{
	g_NetworkMessage.readcount = 0;
}

//*****************************************************************************
//
int NETWORK_ReadChar( void )
{
	int	Char;
	
	// Don't read past the size of the packet.
	if ( g_NetworkMessage.readcount + 1 > g_NetworkMessage.cursize )
		Char = -1;
	else
	{
		if ( g_lNetworkState == NETSTATE_SERVER )
			Char = (signed char)g_NetworkMessage.pbData[g_NetworkMessage.readcount];
		else
			Char = (signed char)g_NetworkMessage.bData[g_NetworkMessage.readcount];
	}

	// Move the "pointer".
	g_NetworkMessage.readcount++;
	
	return ( Char );
}

//*****************************************************************************
//
void NETWORK_WriteChar( sizebuf_t *pBuffer, char cChar )
{
	BYTE	*pbBuf;
	
#ifdef PARANOID
	if ( cChar < -128 || cChar > 127 )
		Printf( "NETWORK_WriteChar: Range error!\n" );
#endif

	pbBuf = NETWORK_GetSpace( pBuffer, 1 );
	pbBuf[0] = cChar;
}

//*****************************************************************************
//
int NETWORK_ReadByte( void )
{
	int	Byte;
	
	// Don't read past the size of the packet.
	if ( g_NetworkMessage.readcount + 1 > g_NetworkMessage.cursize )
		Byte = -1;
	else
	{
		if ( g_lNetworkState == NETSTATE_SERVER )
			Byte = (unsigned char)g_NetworkMessage.pbData[g_NetworkMessage.readcount];
		else
			Byte = (unsigned char)g_NetworkMessage.bData[g_NetworkMessage.readcount];
	}

	// Move the "pointer".
	g_NetworkMessage.readcount++;
	
	return ( Byte );
}

//*****************************************************************************
//
void NETWORK_WriteByte( sizebuf_t *pBuffer, int Byte )
{
	BYTE	*pbBuf;
	
#ifdef PARANOID
	if ( Byte < 0 || Byte > 255 )
		Printf( "NETWORK_WriteByte: Range error %d\n", Byte );
#endif

	pbBuf = NETWORK_GetSpace( pBuffer, 1 );
	pbBuf[0] = Byte;
}

//*****************************************************************************
//
int NETWORK_ReadShort( void )
{
	int	Short;
	
	// Don't read past the size of the packet.
	if ( g_NetworkMessage.readcount + 2 > g_NetworkMessage.cursize )
		Short = -1;
	else		
	{
		if ( g_lNetworkState == NETSTATE_SERVER )
		{
			Short = (short)( g_NetworkMessage.pbData[g_NetworkMessage.readcount]
			+ ( g_NetworkMessage.pbData[g_NetworkMessage.readcount+1]<<8 ));
		}
		else
		{
			Short = (short)( g_NetworkMessage.bData[g_NetworkMessage.readcount]
			+ ( g_NetworkMessage.bData[g_NetworkMessage.readcount+1]<<8 ));
		}
	}

	// Move the "pointer".
	g_NetworkMessage.readcount += 2;
	
	return ( Short );
}

//*****************************************************************************
//
void NETWORK_WriteShort( sizebuf_t *pBuffer, int Short )
{
	byte	*pbBuf;
	
#ifdef PARANOID
	if ( Short < ((short)0x8000) || Short > (short)0x7fff )
		Printf( "NETWORK_WriteShort: Range error %d\n", Short );
#endif

	pbBuf = NETWORK_GetSpace( pBuffer, 2 );
	pbBuf[0] = Short & 0xff;
	pbBuf[1] = Short >> 8;
}

//*****************************************************************************
//
int NETWORK_ReadLong( void )
{
	int	Long;
	
	// Don't read past the size of the packet.
	if ( g_NetworkMessage.readcount + 4 > g_NetworkMessage.cursize )
		Long = -1;
	else
	{
		if ( g_lNetworkState == NETSTATE_SERVER )
		{
			Long = g_NetworkMessage.pbData[g_NetworkMessage.readcount]
			+ ( g_NetworkMessage.pbData[g_NetworkMessage.readcount+1] << 8 )
			+ ( g_NetworkMessage.pbData[g_NetworkMessage.readcount+2] << 16 )
			+ ( g_NetworkMessage.pbData[g_NetworkMessage.readcount+3] << 24 );
		}
		else
		{
			Long = g_NetworkMessage.bData[g_NetworkMessage.readcount]
			+ ( g_NetworkMessage.bData[g_NetworkMessage.readcount+1] << 8 )
			+ ( g_NetworkMessage.bData[g_NetworkMessage.readcount+2] << 16 )
			+ ( g_NetworkMessage.bData[g_NetworkMessage.readcount+3] << 24 );
		}
	}
	
	// Move the "pointer".
	g_NetworkMessage.readcount += 4;
	
	return ( Long );
}

//*****************************************************************************
//
void NETWORK_WriteLong( sizebuf_t *pBuffer, int Long )
{
	BYTE	*pbBuf;
	
	pbBuf = NETWORK_GetSpace( pBuffer, 4 );
	pbBuf[0] = Long & 0xff;
	pbBuf[1] = ( Long >> 8 ) & 0xff;
	pbBuf[2] = ( Long >> 16 ) & 0xff;
	pbBuf[3] = ( Long >> 24 );
}

//*****************************************************************************
//
float NETWORK_ReadFloat( void )
{
	union
	{
		BYTE	b[4];
		float	f;
		int	l;
	} dat;
	
	// Don't read past the size of the packet.
	if ( g_NetworkMessage.readcount + 4 > g_NetworkMessage.cursize )
		dat.f = -1;
	else
	{
		if ( g_lNetworkState == NETSTATE_SERVER )
		{
			dat.b[0] =	g_NetworkMessage.pbData[g_NetworkMessage.readcount];
			dat.b[1] =	g_NetworkMessage.pbData[g_NetworkMessage.readcount+1];
			dat.b[2] =	g_NetworkMessage.pbData[g_NetworkMessage.readcount+2];
			dat.b[3] =	g_NetworkMessage.pbData[g_NetworkMessage.readcount+3];
		}
		else
		{
			dat.b[0] =	g_NetworkMessage.bData[g_NetworkMessage.readcount];
			dat.b[1] =	g_NetworkMessage.bData[g_NetworkMessage.readcount+1];
			dat.b[2] =	g_NetworkMessage.bData[g_NetworkMessage.readcount+2];
			dat.b[3] =	g_NetworkMessage.bData[g_NetworkMessage.readcount+3];
		}
	}

	// Move the "pointer".
	g_NetworkMessage.readcount += 4;
	
	return ( dat.f );	
}

//*****************************************************************************
//
void NETWORK_WriteFloat( sizebuf_t *pBuffer, float Float )
{
	union
	{
		float	f;
		int	l;
	} dat;
	
	dat.f = Float;
	//dat.l = LittleLong (dat.l);

	NETWORK_Write( pBuffer, &dat.l, 4 );
}

//*****************************************************************************
//
char *NETWORK_ReadString( void )
{
	static char	string[2048];
	//int		l,c;
	signed	char	c;
	unsigned int	l;

	l = 0;
	do
	{
		c = NETWORK_ReadChar ();
		if (c == -1 || c == 0)
			break;
		string[l] = c;
		l++;
	} while (l < sizeof(string)-1);

	string[l] = '\0';

	return string;
}

//*****************************************************************************
//
void NETWORK_WriteString ( sizebuf_t *pBuffer, char *pszString )
{
#ifdef	WIN32
	if ( pszString == NULL )
		NETWORK_Write( pBuffer, "", 1 );
	else
		NETWORK_Write( pBuffer, pszString, strlen( pszString ) + 1 );
#else
	if ( pszString == NULL )
		NETWORK_WriteByte( pszBuffer, 0 );
	else
	{
		NETWORK_Write( pBuffer, pszString, strlen( pszString ));
		NETWORK_WriteByte( pBuffer, 0 );
	}
#endif
}

//*****************************************************************************
//
void NETWORK_WriteHeader( sizebuf_t *pBuffer, int Byte )
{
//#ifdef	_DEBUG
	if ( g_lNetworkState == NETSTATE_SERVER )
	{
		if ( debugfile )
			fprintf( debugfile, "Wrote: %s\n", g_pszServerHeaderNames[Byte] );
//		Printf( "Wrote: %s\n", g_pszServerHeaderNames[Byte] );
	}
//#endif
	NETWORK_WriteByte( pBuffer, Byte );
}

//*****************************************************************************
//
void NETWORK_CheckBuffer( ULONG ulClient, ULONG ulSize )
{
	if ( debugfile )
		fprintf( debugfile, "Current packet size: %d\nSize of data being added to packet: %d\n", clients[ulClient].netbuf.cursize, ulSize );

	// Make sure we have enough room for the upcoming message. If not, send
	// out the current buffer and clear the packet.
//	if (( clients[ulClient].netbuf.cursize + ( ulSize + 5 )) >= (ULONG)clients[ulClient].netbuf.maxsize )
	if (( clients[ulClient].netbuf.cursize + ( ulSize + 5 )) >= SERVER_GetMaxPacketSize( ))
	{
//		Printf( "Launching premature packet (%d bytes)\n", clients[ulClient].netbuf.cursize );
		if ( debugfile )
			fprintf( debugfile, "Launching premature packet (reliable)\n" );

		// Lanch the packet so we can prepare another.
		NETWORK_SendPacket( ulClient );

		// Now that the packet has been sent, clear it.
		NETWORK_ClearBuffer( &clients[ulClient].netbuf );
	}

	if (( clients[ulClient].UnreliablePacketBuffer.cursize + ( ulSize + 5 )) >= MAX_UDP_PACKET )
	{
//		Printf( "Launching premature packet (unreliable) (%d bytes)\n", clients[ulClient].netbuf.cursize );
		if ( debugfile )
			fprintf( debugfile, "Launching premature packet (unreliable)\n" );

		// Lanch the packet so we can prepare another.
		NETWORK_SendUnreliablePacket( ulClient );

		// Now that the packet has been sent, clear it.
		NETWORK_ClearBuffer( &clients[ulClient].UnreliablePacketBuffer );
	}
}

//*****************************************************************************
//
int NETWORK_GetPackets( void )
{
	LONG				lNumBytes;
	INT					iDecodedNumBytes;
	struct sockaddr_in	SocketFrom;
	INT					iSocketFromLength;

    iSocketFromLength = sizeof( SocketFrom );
    
#ifdef	WIN32
	lNumBytes = recvfrom( g_NetworkSocket, (char *)g_ucHuffmanBuffer, sizeof( g_ucHuffmanBuffer ), 0, (struct sockaddr *)&SocketFrom, &iSocketFromLength );
#else
	lNumBytes = recvfrom( g_NetworkSocket, (char *)g_ucHuffmanBuffer, sizeof( g_ucHuffmanBuffer ), 0, (struct sockaddr *)&SocketFrom, (socklen_t *)&iSocketFromLength );
#endif

	// If the number of bytes returned is -1, an error has occured.
    if ( lNumBytes == -1 ) 
    { 
#ifdef __WIN32__
        errno = WSAGetLastError( );

        if ( errno == WSAEWOULDBLOCK )
            return ( false );
		
		// Connection reset by peer. Doesn't mean anything to the server.
		if ( errno == WSAECONNRESET )
			return ( false );

        if ( errno == WSAEMSGSIZE )
		{
             Printf( "NETWORK_GetPackets:  WARNING! Oversize packet from %s\n", NETWORK_AddressToString( g_AddressFrom ));
             return ( false );
        }

        Printf( "NETWORK_GetPackets: WARNING!: Error #%d: %s\n", errno, strerror( errno ));
		return ( false );
#else
        if ( errno == EWOULDBLOCK )
            return ( false );

        if ( errno == ECONNREFUSED )
            return ( false );
	    
        Printf( "NETWORK_GetPackets: WARNING!: Error #%d: %s\n", errno, strerror( errno ));
        return ( false );
#endif
    }

	// No packets or an error, dont process anything.
	if ( lNumBytes <= 0 )
		return ( 0 );

	// Record this for our statistics window.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVER_STATISTIC_AddToInboundDataTransfer( lNumBytes );

	// Decode the huffman-encoded message we received.
	if ( g_lNetworkState == NETSTATE_SERVER )
		HuffDecode( g_ucHuffmanBuffer, (unsigned char *)g_NetworkMessage.pbData, lNumBytes, &iDecodedNumBytes );
	else
		HuffDecode( g_ucHuffmanBuffer, (unsigned char *)g_NetworkMessage.bData, lNumBytes, &iDecodedNumBytes );

	g_NetworkMessage.readcount = 0;
	g_NetworkMessage.cursize = iDecodedNumBytes;

	// Store the IP address of the sender.
    NETWORK_SocketAddressToNetAddress( &SocketFrom, &g_AddressFrom );

	return ( g_NetworkMessage.cursize );
}

//*****************************************************************************
//
int NETWORK_GetLANPackets( void )
{
	LONG				lNumBytes;
	INT					iDecodedNumBytes;
	struct sockaddr_in	SocketFrom;
	INT					iSocketFromLength;

    iSocketFromLength = sizeof( SocketFrom );
    
#ifdef	WIN32
	lNumBytes = recvfrom( g_LANSocket, (char *)g_ucHuffmanBuffer, sizeof( g_ucHuffmanBuffer ), 0, (struct sockaddr *)&SocketFrom, &iSocketFromLength );
#else
	lNumBytes = recvfrom( g_LANSocket, (char *)g_ucHuffmanBuffer, sizeof( g_ucHuffmanBuffer ), 0, (struct sockaddr *)&SocketFrom, (socklen_t *)&iSocketFromLength );
#endif

	// If the number of bytes returned is -1, an error has occured.
    if ( lNumBytes == -1 ) 
    { 
#ifdef __WIN32__
        errno = WSAGetLastError( );

        if ( errno == WSAEWOULDBLOCK )
            return ( false );
		
		// Connection reset by peer. Doesn't mean anything to the server.
		if ( errno == WSAECONNRESET )
			return ( false );

        if ( errno == WSAEMSGSIZE )
		{
             Printf( "NETWORK_GetPackets:  WARNING! Oversize packet from %s\n", NETWORK_AddressToString( g_AddressFrom ));
             return ( false );
        }

        Printf( "NETWORK_GetPackets: WARNING!: Error #%d: %s\n", errno, strerror( errno ));
		return ( false );
#else
        if ( errno == EWOULDBLOCK )
            return ( false );

        if ( errno == ECONNREFUSED )
            return ( false );
	    
        Printf( "NETWORK_GetPackets: WARNING!: Error #%d: %s\n", errno, strerror( errno ));
        return ( false );
#endif
    }

	// No packets or an error, dont process anything.
	if ( lNumBytes <= 0 )
		return ( 0 );

	// Record this for our statistics window.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVER_STATISTIC_AddToInboundDataTransfer( lNumBytes );

	// Decode the huffman-encoded message we received.
	if ( g_lNetworkState == NETSTATE_SERVER )
		HuffDecode( g_ucHuffmanBuffer, (unsigned char *)g_NetworkMessage.pbData, lNumBytes, &iDecodedNumBytes );
	else
		HuffDecode( g_ucHuffmanBuffer, (unsigned char *)g_NetworkMessage.bData, lNumBytes, &iDecodedNumBytes );

	g_NetworkMessage.readcount = 0;
	g_NetworkMessage.cursize = iDecodedNumBytes;

	// Store the IP address of the sender.
    NETWORK_SocketAddressToNetAddress( &SocketFrom, &g_AddressFrom );

	return ( g_NetworkMessage.cursize );
}

//*****************************************************************************
//
/*
void NETWORK_LaunchPacket( sizebuf_t netbuf, netadr_t to, bool bCompression )
{
	int ret;
	int	outlen;
	struct sockaddr_in	addr;

	if ( netbuf.cursize <= 0 )
		return;

	if ( g_lNetworkState == NETSTATE_SERVER )
	{
		if ( bCompression )
			HuffEncode((unsigned char *)netbuf.pbData, huffbuff, netbuf.cursize, &outlen);
		else
		{
			memcpy( huffbuff, netbuf.pbData, netbuf.cursize );
			outlen = netbuf.cursize;
		}

		SERVER_STATISTIC_AddToOutboundDataTransfer( outlen );
	}
	else
		HuffEncode((unsigned char *)netbuf.bData, huffbuff, netbuf.cursize, &outlen);

	Printf( "NETWORK_LaunchPacket1: Huffman saved %d bytes\n", netbuf.cursize - outlen );
	g_lRunningHuffmanTotal += ( netbuf.cursize - outlen );
	Printf( "Running total: %d bytes\n", g_lRunningHuffmanTotal );

	NETWORK_NetAddressToSocketAddress( &to, &addr );

	ret = sendto( g_NetworkSocket, (const char*)huffbuff, outlen, 0, (struct sockaddr *)&addr, sizeof(addr));
    
    if (ret == -1) 
    {
#ifdef __WIN32__
          int err = WSAGetLastError();

          // wouldblock is silent
          if (err == WSAEWOULDBLOCK)
              return;

		  switch ( err )
		  {
		  case WSAEACCES:

			  Printf( "NETWORK_LaunchPacket: Error #%d, WSAEACCES: Permission denied for address: %s\n", err, NETWORK_AddressToString( to ));
			  break;
		  case WSAEADDRNOTAVAIL:

			  Printf( "NETWORK_LaunchPacket: Error #%d, WSAEADDRENOTAVAIL: Address %s not available\n", err, NETWORK_AddressToString( to ));
			  break;
		  default:

			Printf( "NETWORK_LaunchPacket: Error #%d\n", err );
			break;
		  }
#else	  
          if (errno == EWOULDBLOCK)
              return;
          if (errno == ECONNREFUSED)
              return;
          Printf ("NET_SendPacket: %s\n", strerror(errno));
#endif	  
    }

}
*/
//*****************************************************************************
//
void NETWORK_LaunchPacket( sizebuf_t *netbuf, netadr_t to )
{
	LONG				lNumBytes;
	INT					iNumBytesOut;
	struct sockaddr_in	SocketAddress;

	// Nothing to do.
	if ( netbuf->cursize <= 0 )
		return;

	// Convert the IP address to a socket address.
    NETWORK_NetAddressToSocketAddress( &to, &SocketAddress );

	if ( g_lNetworkState == NETSTATE_SERVER )
		HuffEncode( (unsigned char *)netbuf->pbData, g_ucHuffmanBuffer, netbuf->cursize, &iNumBytesOut );
	else
		HuffEncode( (unsigned char *)netbuf->bData, g_ucHuffmanBuffer, netbuf->cursize, &iNumBytesOut );

	lNumBytes = sendto( g_NetworkSocket, (const char*)g_ucHuffmanBuffer, iNumBytesOut, 0, (struct sockaddr *)&SocketAddress, sizeof( SocketAddress ));

	// If sendto returns -1, there was an error.
    if ( lNumBytes == -1 )
    {
#ifdef __WIN32__
          INT	iError = WSAGetLastError( );

          // Wouldblock is silent.
          if ( iError == WSAEWOULDBLOCK )
              return;

		  switch ( iError )
		  {
		  case WSAEACCES:

			  Printf( "NETWORK_LaunchPacket: Error #%d, WSAEACCES: Permission denied for address: %s\n", iError, NETWORK_AddressToString( to ));
			  return;
		  case WSAEADDRNOTAVAIL:

			  Printf( "NETWORK_LaunchPacket: Error #%d, WSAEADDRENOTAVAIL: Address %s not available\n", iError, NETWORK_AddressToString( to ));
			  return;
		  default:

			Printf( "NETWORK_LaunchPacket: Error #%d\n", iError );
			return;
		  }
#else	  
          if ( errno == EWOULDBLOCK )
              return;

          if ( errno == ECONNREFUSED )
              return;

          Printf( "NETWORK_LaunchPacket: %s\n", strerror( errno ));
#endif	  
    }

	// Record this for our statistics window.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVER_STATISTIC_AddToOutboundDataTransfer( lNumBytes );
}

//*****************************************************************************
//
void NETWORK_SendPacket( ULONG ulClient )
{
    sizebuf_t		TempBuffer;
	BYTE			abData[MAX_UDP_PACKET];

	TempBuffer.pbData = abData;
    TempBuffer.maxsize = sizeof(abData);
	TempBuffer.cursize = 0;

	// If we've reached the end of our reliable packets buffer, start writing at the beginning.
	if (( clients[ulClient].relpackets.cursize + clients[ulClient].netbuf.cursize ) >= clients[ulClient].relpackets.maxsize )
		clients[ulClient].relpackets.cursize = 0;

	// Save where the beginning is and the size of each packet within the reliable packets
	// buffer.
	clients[ulClient].lPacketBeginning[( clients[ulClient].ulPacketSequence ) % 256] = clients[ulClient].relpackets.cursize;
	clients[ulClient].lPacketSize[( clients[ulClient].ulPacketSequence ) % 256] = clients[ulClient].netbuf.cursize;
	clients[ulClient].lPacketSequence[( clients[ulClient].ulPacketSequence ) % 256] = clients[ulClient].ulPacketSequence;

	// Write what we want to send out to our reliable packets buffer, so that it can be
	// retransmitted later if necessary.
	if ( clients[ulClient].netbuf.cursize )
		NETWORK_Write( &clients[ulClient].relpackets, clients[ulClient].netbuf.pbData, clients[ulClient].netbuf.cursize );

	// Write the header to our temporary buffer.
	NETWORK_WriteByte( &TempBuffer, SVC_HEADER );
	NETWORK_WriteLong( &TempBuffer, clients[ulClient].ulPacketSequence );

	// Write the body of the message to our temporary buffer.
    if ( clients[ulClient].netbuf.cursize )
		NETWORK_Write( &TempBuffer, clients[ulClient].netbuf.pbData, clients[ulClient].netbuf.cursize );

	clients[ulClient].ulPacketSequence++;

	// Finally, send the packet.
	NETWORK_LaunchPacket( &TempBuffer, clients[ulClient].address );
}

//*****************************************************************************
//
void NETWORK_SendUnreliablePacket( ULONG ulClient )
{
    sizebuf_t		TempBuffer;
	BYTE			abData[MAX_UDP_PACKET];

	TempBuffer.pbData = abData;
    TempBuffer.maxsize = sizeof(abData);
	TempBuffer.cursize = 0;

	// Write the header to our temporary buffer.
	NETWORK_WriteByte( &TempBuffer, SVC_UNRELIABLEPACKET );

	// Write the body of the message to our temporary buffer.
    if ( clients[ulClient].UnreliablePacketBuffer.cursize )
		NETWORK_Write( &TempBuffer, clients[ulClient].UnreliablePacketBuffer.pbData, clients[ulClient].UnreliablePacketBuffer.cursize );

	// Finally, send the packet.
	NETWORK_LaunchPacket( &TempBuffer, clients[ulClient].address );
}

//*****************************************************************************
//
void NETWORK_InitBuffer( sizebuf_t *pBuffer, ULONG ulLength )
{
	memset( pBuffer, 0, sizeof( *pBuffer ));
	pBuffer->maxsize = ulLength;

	if ( g_lNetworkState == NETSTATE_SERVER )
		pBuffer->pbData = new BYTE[ulLength];
}

//*****************************************************************************
//
void NETWORK_FreeBuffer( sizebuf_t *pBuffer )
{
	if ( g_lNetworkState == NETSTATE_SERVER )
	{
		if ( pBuffer->pbData )
		{
			delete ( pBuffer->pbData );
			pBuffer->pbData = NULL;
		}
	}
}

//*****************************************************************************
//
void NETWORK_ClearBuffer( sizebuf_t *pBuffer )
{
	pBuffer->cursize = 0;
//	pBuffer->overflowed = false;
}

//*****************************************************************************
//
BYTE *NETWORK_GetSpace( sizebuf_t *pBuffer, ULONG ulLength )
{
	BYTE	*pbData;

	// Make sure we have enough room left in the packet.
	if ( pBuffer->cursize + ulLength > (ULONG)pBuffer->maxsize )
	{
		if ( ulLength > (ULONG)pBuffer->maxsize )
			Printf( "NETWORK_GetSpace: %i is > full buffer size.\n", ulLength );
		else
			Printf( "NETWORK_GetSpace: Overflow!\n" );

// [NightFang] Purposely make this crash to find out what caused the overflow
#ifdef	_DEBUG
		int one=1, two=0;
		int t;
		t = one/two;
#endif

		NETWORK_ClearBuffer( pBuffer );

		if ( g_lNetworkState == NETSTATE_SERVER )
			return ( 0 );
	}

	if ( g_lNetworkState == NETSTATE_SERVER )
		pbData = pBuffer->pbData + pBuffer->cursize;
	else
		pbData = pBuffer->bData + pBuffer->cursize;
	pBuffer->cursize += ulLength;
	
	return ( pbData );
}

//*****************************************************************************
//
void NETWORK_Write( sizebuf_t *pBuffer, void *pvData, int nLength )
{
	BYTE	*pbDatapos;

	if ( g_lNetworkState == NETSTATE_CLIENT )
		memcpy( NETWORK_GetSpace( pBuffer, nLength ), pvData, nLength );
	else
	{
		pbDatapos = NETWORK_GetSpace( pBuffer, nLength );

		// Bad getspace.
		if ( pbDatapos == 0 )
		{
			Printf( "NETWORK_Write: Couldn't get %d bytes of space!\n", nLength );
			return;
		}
		
		memcpy( pbDatapos, pvData, nLength );
	}
}

//*****************************************************************************
//
void NETWORK_Write( sizebuf_t *pBuffer, BYTE *pbData, int nStartPos, int nLength )
{
	BYTE	*pbDatapos;

	if ( g_lNetworkState == NETSTATE_CLIENT )
	{
		pbData += nStartPos;
		memcpy( NETWORK_GetSpace( pBuffer, nLength ), pbData, nLength );
	}
	else
	{
		pbDatapos = NETWORK_GetSpace( pBuffer, nLength );

		// Bad getspace.
		if ( pbDatapos == 0 )
			return;
		
		pbData += nStartPos;
		memcpy( pbDatapos, pbData, nLength );
	}
}

//*****************************************************************************
//
void NETWORK_Print( sizebuf_t *pBuffer, char *pszData )
{
/*
	USHORT	usLength;
	
	usLength = strlen( pszData ) + 1;

	if ( pBuffer->cursize )
	{
		if ( pBuffer->data[pBuffer->cursize - 1] )
			memcpy ((BYTE *)NETWORK_GetSpace( pBuffer, usLength ), pszData, usLength ); // no trailing 0
		else
			memcpy ((BYTE *)NETWORK_GetSpace( pBuffer, usLength - 1 ) - 1, pszData, usLength ); // write over trailing 0
	}
	else
		memcpy ((BYTE *)NETWORK_GetSpace( pBuffer, usLength ), pszData, usLength );
*/
}

//*****************************************************************************
//
char *NETWORK_AddressToString( netadr_t a )
{
	static char	s_szAddress[64];

	sprintf( s_szAddress, "%i.%i.%i.%i:%i", a.ip[0], a.ip[1], a.ip[2], a.ip[3], ntohs( a.port ));

	return ( s_szAddress );
}

//*****************************************************************************
//
char *NETWORK_AddressToStringIgnorePort( netadr_t a )
{
	static char	s_szAddress[64];

	sprintf( s_szAddress, "%i.%i.%i.%i", a.ip[0], a.ip[1], a.ip[2], a.ip[3] );

	return ( s_szAddress );
}

//*****************************************************************************
//
bool NETWORK_StringToAddress( char *s, netadr_t *a )
{
     struct hostent  *h;
     struct sockaddr_in sadr;
     char    *colon;
     char    copy[128];

     memset (&sadr, 0, sizeof(sadr));
     sadr.sin_family = AF_INET;

     sadr.sin_port = 0;

     strcpy (copy, s);
     // strip off a trailing :port if present
     for (colon = copy ; *colon ; colon++)
          if (*colon == ':')
          {
             *colon = 0;
             sadr.sin_port = htons(atoi(colon+1));
          }

	{
		LONG	lRet;

		lRet = inet_addr( copy );

		// If our return value is INADDR_NONE, the IP specified is not a valid IPv4 string.
		if ( lRet == INADDR_NONE )
		{
			// If the string cannot be resolved to a valid IP address, return false.
          if (( h = gethostbyname( copy )) == NULL )
                return ( false );
          *(int *)&sadr.sin_addr = *(int *)h->h_addr_list[0];
		}
		else
			*(int *)&sadr.sin_addr = lRet;
	}

	NETWORK_SocketAddressToNetAddress (&sadr, a);

     return true;
}

//*****************************************************************************
//
void NETWORK_NetAddressToSocketAddress( netadr_t *a, struct sockaddr_in *s )
{
     memset (s, 0, sizeof(*s));
     s->sin_family = AF_INET;

     *(int *)&s->sin_addr = *(int *)&a->ip;
     s->sin_port = a->port;
}
//*****************************************************************************
//
bool NETWORK_CompareAddress( netadr_t a, netadr_t b, bool bIgnorePort )
{
	if (( a.ip[0] == b.ip[0] ) &&
		( a.ip[1] == b.ip[1] ) &&
		( a.ip[2] == b.ip[2] ) &&
		( a.ip[3] == b.ip[3] ) &&
		( bIgnorePort ? 1 : ( a.port == b.port )))
		return ( true );

	return ( false );
}

//*****************************************************************************
//
void NETWORK_SocketAddressToNetAddress( struct sockaddr_in *s, netadr_t *a )
{
     *(int *)&a->ip = *(int *)&s->sin_addr;
     a->port = s->sin_port;
}

//*****************************************************************************
//
AActor *NETWORK_FindThingByNetID( LONG lID )
{
	if (( lID < 0 ) || ( lID >= 65536 ))
		return ( NULL );

	if (( g_NetIDList[lID].bFree == false ) && ( g_NetIDList[lID].pActor ))
		return ( g_NetIDList[lID].pActor );

    return ( NULL );
}

//*****************************************************************************
//
netadr_t NETWORK_GetLocalAddress( void )
{
	char	buff[512];
	struct sockaddr_in	address;
	int		namelen;

	gethostname(buff, 512);
	buff[512-1] = 0;

	NETWORK_StringToAddress( buff, &g_LocalNetworkAddress );

	namelen = sizeof(address);
#ifndef	WIN32
	if (getsockname ( g_NetworkSocket (struct sockaddr *)&address, (socklen_t *)&namelen) == -1)
#else
	if (getsockname ( g_NetworkSocket, (struct sockaddr *)&address, &namelen) == -1)
#endif
	{
		network_Error( "NET_Init: getsockname:" );//, strerror(errno));
	}
	g_LocalNetworkAddress.port = address.sin_port;

	return ( g_LocalNetworkAddress );
}

//*****************************************************************************
//
LONG NETWORK_GetPacketSize( void )
{
	return ( g_NetworkMessage.cursize );
}

//*****************************************************************************
//
sizebuf_t *NETWORK_GetNetworkMessageBuffer( void )
{
	return ( &g_NetworkMessage );
}

//*****************************************************************************
//
ULONG NETWORK_ntohs( ULONG ul )
{
	return ( ntohs( (u_short)ul ));
}

//*****************************************************************************
//*****************************************************************************
//
void network_Error( char *pszError )
{
	Printf( "\\cd%s\n", pszError );
}

//*****************************************************************************
//
static SOCKET network_AllocateSocket( void )
{
	SOCKET	Socket;

	// Allocate a socket.
	Socket = socket( PF_INET, SOCK_DGRAM, IPPROTO_UDP );
	if ( Socket == INVALID_SOCKET )
		network_Error( "network_AllocateSocket: Couldn't create socket!" );

	return ( Socket );
}

//*****************************************************************************
//
bool network_BindSocketToPort( int Socket, USHORT usPort, bool bReUse )
{
	int		iErrorCode;
	struct sockaddr_in address;
	bool	bBroadCast = true;

	memset (&address, 0, sizeof(address));
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons( usPort );

	// Allow the network socket to broadcast.
	setsockopt( Socket, SOL_SOCKET, SO_BROADCAST, (const char *)&bBroadCast, sizeof( bBroadCast ));
	if ( bReUse )
		setsockopt( Socket, SOL_SOCKET, SO_REUSEADDR, (const char *)&bBroadCast, sizeof( bBroadCast ));

	iErrorCode = bind( Socket, (sockaddr *)&address, sizeof( address ));
	if ( iErrorCode == SOCKET_ERROR )
		return ( false );

	return ( true );
}

//*****************************************************************************
//
void network_GetLocalAddress( void )
{
	char	buff[512];
	struct sockaddr_in	address;
	int		namelen;

	gethostname(buff, 512);
	buff[512-1] = 0;

	NETWORK_StringToAddress( buff, &g_LocalNetworkAddress );

	namelen = sizeof(address);
#ifndef	WIN32
	if (getsockname ( g_NetworkSocket (struct sockaddr *)&address, (socklen_t *)&namelen) == -1)
#else
	if (getsockname ( g_NetworkSocket, (struct sockaddr *)&address, &namelen) == -1)
#endif
	{
		network_Error( "NET_Init: getsockname:" );//, strerror(errno));
	}
	g_LocalNetworkAddress.port = address.sin_port;

	Printf( "IP address %s\n", NETWORK_AddressToString( g_LocalNetworkAddress ));
}

void I_DoSelect (void)
{
#ifdef		WIN32
    struct timeval   timeout;
    fd_set           fdset;

    FD_ZERO(&fdset);
    FD_SET(g_NetworkSocket, &fdset);
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    if (select (g_NetworkSocket+1, &fdset, NULL, NULL, &timeout) == -1)
        return;
#else
    struct timeval   timeout;
    fd_set           fdset;

    FD_ZERO(&fdset);
    if (do_stdin)
    	FD_SET(0, &fdset);
    FD_SET(g_NetworkSocket, &fdset);
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    if (select (g_NetworkSocket+1, &fdset, NULL, NULL, &timeout) == -1)
        return;

    stdin_ready = FD_ISSET(0, &fdset);
#endif
}

//*****************************************************************************
//
void I_SetPort( netadr_t &addr, int port )
{
   addr.port = htons(port);
}

//*****************************************************************************
//	CONSOLE COMMANDS

CCMD( ip )
{
	if (( NETWORK_GetState( ) == NETSTATE_SERVER ) || ( NETWORK_GetState( ) == NETSTATE_CLIENT ))
		network_GetLocalAddress( );

//	Printf( PRINT_HIGH, "IP address is %s\n", NETWORK_AddressToString( g_LocalNetworkAddress ));
}

//*****************************************************************************
//
CCMD( netstate )
{
	switch ( g_lNetworkState )
	{
	case NETSTATE_SINGLE:

		Printf( "Game being run as in SINGLE PLAYER.\n" );
		break;
	case NETSTATE_SINGLE_MULTIPLAYER:

		Printf( "Game being run as in MULTIPLAYER EMULATION.\n" );
		break;
	case NETSTATE_CLIENT:

		Printf( "Game being run as a CLIENT.\n" );
		break;
	case NETSTATE_SERVER:

		Printf( "Game being run as a SERVER.\n" );
		break;
	}
}

#ifdef	_DEBUG
// DEBUG FUNCTION!
void NETWORK_FillBufferWithShit( sizebuf_t *pBuffer, ULONG ulSize )
{
	ULONG	ulIdx;

	// Fill the packet with 1k of SHIT!
	for ( ulIdx = 0; ulIdx < ulSize; ulIdx++ )
		NETWORK_WriteByte( pBuffer, M_Random( ));

//	NETWORK_ClearBuffer( &g_NetworkMessage );
}

CCMD( fillbufferwithshit )
{
	NETWORK_FillBufferWithShit( &g_NetworkMessage, 1024 );
/*
	ULONG	ulIdx;

	// Fill the packet with 1k of SHIT!
	for ( ulIdx = 0; ulIdx < 1024; ulIdx++ )
		NETWORK_WriteByte( &g_NetworkMessage, M_Random( ));

//	NETWORK_ClearBuffer( &g_NetworkMessage );
*/
}

CCMD( testnetstring )
{
	if ( argv.argc( ) < 2 )
		return;

//	Printf( "%s: %d", argv[1], gethostbyname( argv[1] ));
	Printf( "%s: %d\n", argv[1], inet_addr( argv[1] ));
	if ( inet_addr( argv[1] ) == INADDR_NONE )
		Printf( "FAIL!\n" );
}
#endif	// _DEBUG
