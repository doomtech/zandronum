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

#include "networkheaders.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <ctype.h>
#include <math.h>

#include "c_dispatch.h"
#include "cl_main.h"
#include "doomtype.h"
#include "huffman.h"
#include "i_system.h"
#include "sv_main.h"
#include "m_random.h"
#include "network.h"
#include "sbar.h"

#include "MD5Checksum.h"

//*****************************************************************************
//	VARIABLES

// [BB]: Some optimization. For some actors that are sent in bunches, to reduce the size,
// just send some key letter that identifies the actor, instead of the full name.
char	*g_szActorKeyLetter[NUMBER_OF_ACTOR_NAME_KEY_LETTERS] = 
{
	"1",
	"2",
};

//*****************************************************************************
char	*g_szActorFullName[NUMBER_OF_ACTOR_NAME_KEY_LETTERS] =
{
	"BulletPuff",
	"Blood",
};

//*****************************************************************************
char	*g_szWeaponKeyLetter[NUMBER_OF_WEAPON_NAME_KEY_LETTERS] = 
{
	"1",
	"2",
	"3",
	"4",
	"5",
	"6",
	"7",
	"8",
	"9",
	"0",
};

//*****************************************************************************
char	*g_szWeaponFullName[NUMBER_OF_WEAPON_NAME_KEY_LETTERS] =
{
	"Fist",
	"Pistol",
	"Shotgun",
	"SuperShotgun",
	"RocketLauncher",
	"GrenadeLauncher",
	"PlasmaRifle",
	"Railgun",
	"BFG9000",
	"BFG10K",
};

// The current network state. Single player, client, server, etc.
static	LONG			g_lNetworkState = NETSTATE_SINGLE;

// Buffer that holds the data from the most recently received packet.
static	NETBUFFER_s		g_NetworkMessage;

// Network address that the most recently received packet came from.
static	NETADDRESS_s	g_AddressFrom;

// Our network socket.
static	SOCKET			g_NetworkSocket;

// Socket for listening for LAN games.
static	SOCKET			g_LANSocket;

// Our local port.
static	USHORT			g_usLocalPort;

// Buffer for the Huffman encoding.
static	UCHAR			g_ucHuffmanBuffer[131072];

//*****************************************************************************
//	PROTOTYPES

static	void			network_Error( char *pszError );
static	SOCKET			network_AllocateSocket( void );
static	bool			network_BindSocketToPort( SOCKET Socket, USHORT usPort, bool bReUse );

//*****************************************************************************
//	FUNCTIONS

void NETWORK_Construct( USHORT usPort, bool bAllocateLANSocket )
{
	char			szString[128];
	ULONG			ulArg;
	USHORT			usNewPort;
	NETADDRESS_s	LocalAddress;
	WSADATA			WSAData;
	bool			bSuccess;

	// Initialize the Huffman buffer.
	HuffInit( );

#ifdef __WIN32__
	if ( WSAStartup( 0x0101, &WSAData ))
		network_Error( "Winsock initialization failed!\n" );

	Printf( "Winsock initialization succeeded!\n" );
#endif

	g_usLocalPort = usPort;

	// Allocate a socket, and attempt to bind it to the given port.
	g_NetworkSocket = network_AllocateSocket( );
	if ( network_BindSocketToPort( g_NetworkSocket, g_usLocalPort, false ) == false )
	{
		bSuccess = true;
		usNewPort = g_usLocalPort;
		while ( network_BindSocketToPort( g_NetworkSocket, ++usNewPort, false ) == false )
		{
			// Didn't find an available port. Oh well...
			if ( usNewPort == g_usLocalPort )
			{
				usNewPort = false;
				break;
			}
		}

		if ( bSuccess == false )
		{
			sprintf( szString, "NETWORK_Construct: Couldn't bind socket to port: %d\n", g_usLocalPort );
			network_Error( szString );
		}
		else
		{
			Printf( "NETWORK_Construct: Couldn't bind to %d. Binding to %d instead...\n", g_usLocalPort, usNewPort );
			g_usLocalPort = usNewPort;
		}
	}

	ulArg = true;
	if ( ioctlsocket( g_NetworkSocket, FIONBIO, &ulArg ) == -1 )
		printf( "network_AllocateSocket: ioctl FIONBIO: %s", strerror( errno ));

	// If we're not starting a server, setup a socket to listen for LAN servers.
	if ( bAllocateLANSocket )
	{
		g_LANSocket = network_AllocateSocket( );
		if ( network_BindSocketToPort( g_LANSocket, DEFAULT_BROADCAST_PORT, true ) == false )
		{
			sprintf( szString, "network_BindSocketToPort: Couldn't bind LAN socket to port: %d. You will not be able to see LAN servers in the browser.", DEFAULT_BROADCAST_PORT );
			network_Error( szString );
		}

		if ( ioctlsocket( g_LANSocket, FIONBIO, &ulArg ) == -1 )
			printf( "network_AllocateSocket: ioctl FIONBIO: %s", strerror( errno ));
	}

	// Init our read buffer.
	NETWORK_InitBuffer( &g_NetworkMessage, MAX_UDP_PACKET );
	NETWORK_ClearBuffer( &g_NetworkMessage );

	// Print out our local IP address.
	LocalAddress = NETWORK_GetLocalAddress( );
	Printf( "IP address %s\n", NETWORK_AddressToString( LocalAddress ));

	Printf( "UDP Initialized.\n" );
}

//*****************************************************************************
//
int NETWORK_ReadChar( void )
{
	int	Char;

	// Don't read past the size of the packet.
	if ( g_NetworkMessage.ulCurrentPosition + 1 > g_NetworkMessage.ulCurrentSize )
		Char = -1;
	else
		Char = (signed char)g_NetworkMessage.pbData[g_NetworkMessage.ulCurrentPosition];

	// Move the "pointer".
	g_NetworkMessage.ulCurrentPosition++;

	return ( Char );
}

//*****************************************************************************
//
void NETWORK_WriteChar( NETBUFFER_s *pBuffer, char cChar )
{
	BYTE	*pbBuf;

	pbBuf = NETWORK_GetSpace( pBuffer, 1 );
	pbBuf[0] = cChar;
}

//*****************************************************************************
//
int NETWORK_ReadByte( void )
{
	int	Byte;

	// Don't read past the size of the packet.
	if ( g_NetworkMessage.ulCurrentPosition + 1 > g_NetworkMessage.ulCurrentSize )
		Byte = -1;
	else
		Byte = (unsigned char)g_NetworkMessage.pbData[g_NetworkMessage.ulCurrentPosition];

	// Move the "pointer".
	g_NetworkMessage.ulCurrentPosition++;

	return ( Byte );
}

//*****************************************************************************
//
void NETWORK_WriteByte( NETBUFFER_s *pBuffer, int Byte )
{
	BYTE	*pbBuf;

	pbBuf = NETWORK_GetSpace( pBuffer, 1 );
	pbBuf[0] = Byte;
}

//*****************************************************************************
//
int NETWORK_ReadShort( void )
{
	int	Short;

	// Don't read past the size of the packet.
	if ( g_NetworkMessage.ulCurrentPosition + 2 > g_NetworkMessage.ulCurrentSize )
		Short = -1;
	else		
	{
		Short = (short)(( g_NetworkMessage.pbData[g_NetworkMessage.ulCurrentPosition] )
		+ ( g_NetworkMessage.pbData[g_NetworkMessage.ulCurrentPosition + 1] << 8 ));
	}

	// Move the "pointer".
	g_NetworkMessage.ulCurrentPosition += 2;

	return ( Short );
}

//*****************************************************************************
//
void NETWORK_WriteShort( NETBUFFER_s *pBuffer, int Short )
{
	BYTE	*pbBuf;

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
	if ( g_NetworkMessage.ulCurrentPosition + 4 > g_NetworkMessage.ulCurrentSize )
		Long = -1;
	else
	{
		Long = ( g_NetworkMessage.pbData[g_NetworkMessage.ulCurrentPosition] )
		+ ( g_NetworkMessage.pbData[g_NetworkMessage.ulCurrentPosition + 1] << 8 )
		+ ( g_NetworkMessage.pbData[g_NetworkMessage.ulCurrentPosition + 2] << 16 )
		+ ( g_NetworkMessage.pbData[g_NetworkMessage.ulCurrentPosition + 3] << 24 );
	}

	// Move the "pointer".
	g_NetworkMessage.ulCurrentPosition += 4;

	return ( Long );
}

//*****************************************************************************
//
void NETWORK_WriteLong( NETBUFFER_s *pBuffer, int Long )
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

	dat.l = NETWORK_ReadLong( );
	return ( dat.f );
/*
	// Don't read past the size of the packet.
	if ( g_NetworkMessage.ulCurrentPosition + 4 > g_NetworkMessage.ulCurrentSize )
		dat.f = -1;
	else
	{
		if ( g_lNetworkState == NETSTATE_SERVER )
		{
			dat.b[0] =	g_NetworkMessage.pbData[g_NetworkMessage.ulCurrentPosition];
			dat.b[1] =	g_NetworkMessage.pbData[g_NetworkMessage.ulCurrentPosition+1];
			dat.b[2] =	g_NetworkMessage.pbData[g_NetworkMessage.ulCurrentPosition+2];
			dat.b[3] =	g_NetworkMessage.pbData[g_NetworkMessage.ulCurrentPosition+3];
		}
		else
		{
			dat.b[0] =	g_NetworkMessage.bData[g_NetworkMessage.ulCurrentPosition];
			dat.b[1] =	g_NetworkMessage.bData[g_NetworkMessage.ulCurrentPosition+1];
			dat.b[2] =	g_NetworkMessage.bData[g_NetworkMessage.ulCurrentPosition+2];
			dat.b[3] =	g_NetworkMessage.bData[g_NetworkMessage.ulCurrentPosition+3];
		}
	}

	// Move the "pointer".
	g_NetworkMessage.ulCurrentPosition += 4;
	
	return ( dat.f );
*/
}

//*****************************************************************************
//
void NETWORK_WriteFloat( NETBUFFER_s *pBuffer, float Float )
{
	union
	{
		float	f;
		int	l;
	} dat;

	dat.f = Float;
	//dat.l = LittleLong (dat.l);

	NETWORK_WriteLong( pBuffer, dat.l );
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
void NETWORK_WriteString ( NETBUFFER_s *pBuffer, const char *pszString )
{
#ifdef	WIN32
	if ( pszString == NULL )
		NETWORK_Write( pBuffer, "", 1 );
	else
		NETWORK_Write( pBuffer, pszString, static_cast<int>(strlen( pszString )) + 1 );
#else
	if ( pszString == NULL )
		NETWORK_WriteByte( pBuffer, 0 );
	else
	{
		NETWORK_Write( pBuffer, pszString, strlen( pszString ));
		NETWORK_WriteByte( pBuffer, 0 );
	}
#endif
}

//*****************************************************************************
//
void NETWORK_WriteHeader( NETBUFFER_s *pBuffer, int Byte )
{
//	Printf( "%s\n", g_pszHeaderNames[Byte] );
	NETWORK_WriteByte( pBuffer, Byte );
}

//*****************************************************************************
//
void NETWORK_CheckBuffer( ULONG ulClient, ULONG ulSize )
{
	CLIENT_s	*pClient;

	pClient = SERVER_GetClient( ulClient );
	if ( pClient == NULL )
		return;

	if ( debugfile )
		fprintf( debugfile, "Current packet size: %d\nSize of data being added to packet: %d\n", pClient->PacketBuffer.ulCurrentSize, ulSize );

	// Make sure we have enough room for the upcoming message. If not, send
	// out the current buffer and clear the packet.
//	if (( clients[ulClient].netbuf.ulCurrentSize + ( ulSize + 5 )) >= (ULONG)clients[ulClient].netbuf.maxsize )
	if (( pClient->PacketBuffer.ulCurrentSize + ( ulSize + 5 )) >= SERVER_GetMaxPacketSize( ))
	{
//		Printf( "Launching premature packet (%d bytes)\n", clients[ulClient].netbuf.ulCurrentSize );
		if ( debugfile )
			fprintf( debugfile, "Launching premature packet (reliable)\n" );

		// Lanch the packet so we can prepare another.
		NETWORK_SendPacket( ulClient );

		// Now that the packet has been sent, clear it.
		NETWORK_ClearBuffer( &pClient->PacketBuffer );
	}

	if (( pClient->UnreliablePacketBuffer.ulCurrentSize + ( ulSize + 5 )) >= MAX_UDP_PACKET )
	{
//		Printf( "Launching premature packet (unreliable) (%d bytes)\n", clients[ulClient].netbuf.ulCurrentSize );
		if ( debugfile )
			fprintf( debugfile, "Launching premature packet (unreliable)\n" );

		// Lanch the packet so we can prepare another.
		NETWORK_SendUnreliablePacket( ulClient );

		// Now that the packet has been sent, clear it.
		NETWORK_ClearBuffer( &pClient->UnreliablePacketBuffer );
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
	HuffDecode( g_ucHuffmanBuffer, (unsigned char *)g_NetworkMessage.pbData, lNumBytes, &iDecodedNumBytes );

	g_NetworkMessage.ulCurrentPosition = 0;
	g_NetworkMessage.ulCurrentSize = iDecodedNumBytes;

	// Store the IP address of the sender.
    NETWORK_SocketAddressToNetAddress( &SocketFrom, &g_AddressFrom );

	return ( g_NetworkMessage.ulCurrentSize );
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
	HuffDecode( g_ucHuffmanBuffer, (unsigned char *)g_NetworkMessage.pbData, lNumBytes, &iDecodedNumBytes );

	g_NetworkMessage.ulCurrentPosition = 0;
	g_NetworkMessage.ulCurrentSize = iDecodedNumBytes;

	// Store the IP address of the sender.
    NETWORK_SocketAddressToNetAddress( &SocketFrom, &g_AddressFrom );

	return ( g_NetworkMessage.ulCurrentSize );
}

//*****************************************************************************
//
BYTE *NETWORK_GetBuffer( void )
{
	return ( &g_NetworkMessage.pbData[g_NetworkMessage.ulCurrentPosition] );
}

//*****************************************************************************
//
NETADDRESS_s NETWORK_GetFromAddress( void )
{
	return ( g_AddressFrom );
}

//*****************************************************************************
//
/*
void NETWORK_LaunchPacket( NETBUFFER_s netbuf, NETADDRESS_s to, bool bCompression )
{
	int ret;
	int	outlen;
	struct sockaddr_in	addr;

	if ( netbuf.ulCurrentSize <= 0 )
		return;

	if ( g_lNetworkState == NETSTATE_SERVER )
	{
		if ( bCompression )
			HuffEncode((unsigned char *)netbuf.pbData, huffbuff, netbuf.ulCurrentSize, &outlen);
		else
		{
			memcpy( huffbuff, netbuf.pbData, netbuf.ulCurrentSize );
			outlen = netbuf.ulCurrentSize;
		}

		SERVER_STATISTIC_AddToOutboundDataTransfer( outlen );
	}
	else
		HuffEncode((unsigned char *)netbuf.bData, huffbuff, netbuf.ulCurrentSize, &outlen);

	Printf( "NETWORK_LaunchPacket1: Huffman saved %d bytes\n", netbuf.ulCurrentSize - outlen );
	g_lRunningHuffmanTotal += ( netbuf.ulCurrentSize - outlen );
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
void NETWORK_LaunchPacket( NETBUFFER_s *pBuffer, NETADDRESS_s Address )
{
	LONG				lNumBytes;
	INT					iNumBytesOut;
	struct sockaddr_in	SocketAddress;

	// Nothing to do.
	if ( pBuffer->ulCurrentSize == 0 )
		return;

	// Convert the IP address to a socket address.
    NETWORK_NetAddressToSocketAddress( Address, SocketAddress );

	HuffEncode( (unsigned char *)pBuffer->pbData, g_ucHuffmanBuffer, pBuffer->ulCurrentSize, &iNumBytesOut );

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

			  Printf( "NETWORK_LaunchPacket: Error #%d, WSAEACCES: Permission denied for address: %s\n", iError, NETWORK_AddressToString( Address ));
			  return;
		  case WSAEADDRNOTAVAIL:

			  Printf( "NETWORK_LaunchPacket: Error #%d, WSAEADDRENOTAVAIL: Address %s not available\n", iError, NETWORK_AddressToString( Address ));
			  return;
		  case WSAEHOSTUNREACH:

				Printf( "NETWORK_LaunchPacket: Error #%d, WSAEHOSTUNREACH: Address %s unreachable\n", iError, NETWORK_AddressToString( Address ));
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
    NETBUFFER_s		TempBuffer;
	BYTE			abData[MAX_UDP_PACKET];
	CLIENT_s		*pClient;

	pClient = SERVER_GetClient( ulClient );
	if ( pClient == NULL )
		return;

	TempBuffer.pbData = abData;
	TempBuffer.ulMaxSize = sizeof( abData );
	TempBuffer.ulCurrentSize = 0;

	// If we've reached the end of our reliable packets buffer, start writing at the beginning.
	if (( pClient->SavedPacketBuffer.ulCurrentSize + pClient->PacketBuffer.ulCurrentSize ) >= pClient->SavedPacketBuffer.ulMaxSize )
		pClient->SavedPacketBuffer.ulCurrentSize = 0;

	// Save where the beginning is and the size of each packet within the reliable packets
	// buffer.
	pClient->lPacketBeginning[( pClient->ulPacketSequence ) % 256] = pClient->SavedPacketBuffer.ulCurrentSize;
	pClient->lPacketSize[( pClient->ulPacketSequence ) % 256] = pClient->PacketBuffer.ulCurrentSize;
	pClient->lPacketSequence[( pClient->ulPacketSequence ) % 256] = pClient->ulPacketSequence;

	// Write what we want to send out to our reliable packets buffer, so that it can be
	// retransmitted later if necessary.
	if ( pClient->PacketBuffer.ulCurrentSize )
		NETWORK_Write( &pClient->SavedPacketBuffer, pClient->PacketBuffer.pbData, pClient->PacketBuffer.ulCurrentSize );

	// Write the header to our temporary buffer.
	NETWORK_WriteByte( &TempBuffer, SVC_HEADER );
	NETWORK_WriteLong( &TempBuffer, pClient->ulPacketSequence );

	// Write the body of the message to our temporary buffer.
    if ( pClient->PacketBuffer.ulCurrentSize )
		NETWORK_Write( &TempBuffer, pClient->PacketBuffer.pbData, pClient->PacketBuffer.ulCurrentSize );

	pClient->ulPacketSequence++;

	// Finally, send the packet.
	NETWORK_LaunchPacket( &TempBuffer, pClient->Address );
}

//*****************************************************************************
//
void NETWORK_SendUnreliablePacket( ULONG ulClient )
{
    NETBUFFER_s		TempBuffer;
	BYTE			abData[MAX_UDP_PACKET];
	CLIENT_s		*pClient;

	pClient = SERVER_GetClient( ulClient );
	if ( pClient == NULL )
		return;

	TempBuffer.pbData = abData;
	TempBuffer.ulMaxSize = sizeof(abData);
	TempBuffer.ulCurrentSize = 0;

	// Write the header to our temporary buffer.
	NETWORK_WriteByte( &TempBuffer, SVC_UNRELIABLEPACKET );

	// Write the body of the message to our temporary buffer.
    if ( pClient->UnreliablePacketBuffer.ulCurrentSize )
		NETWORK_Write( &TempBuffer, pClient->UnreliablePacketBuffer.pbData, pClient->UnreliablePacketBuffer.ulCurrentSize );

	// Finally, send the packet.
	NETWORK_LaunchPacket( &TempBuffer, pClient->Address );
}

//*****************************************************************************
//
void NETWORK_InitBuffer( NETBUFFER_s *pBuffer, ULONG ulLength )
{
	memset( pBuffer, 0, sizeof( *pBuffer ));
	pBuffer->ulMaxSize = ulLength;
	pBuffer->pbData = new BYTE[ulLength];
}

//*****************************************************************************
//
void NETWORK_FreeBuffer( NETBUFFER_s *pBuffer )
{
	if ( pBuffer->pbData )
	{
		delete ( pBuffer->pbData );
		pBuffer->pbData = NULL;
	}
}

//*****************************************************************************
//
void NETWORK_ClearBuffer( NETBUFFER_s *pBuffer )
{
	pBuffer->ulCurrentSize = 0;
}

//*****************************************************************************
//
BYTE *NETWORK_GetSpace( NETBUFFER_s *pBuffer, ULONG ulLength )
{
	BYTE	*pbData;

	// Make sure we have enough room left in the packet.
	if (( pBuffer->ulCurrentSize + ulLength ) > pBuffer->ulMaxSize )
	{
		if ( ulLength > pBuffer->ulMaxSize )
			Printf( "NETWORK_GetSpace: %i is > full buffer size.\n", ulLength );
		else
			Printf( "NETWORK_GetSpace: Overflow!\n" );

		NETWORK_ClearBuffer( pBuffer );
		return ( NULL );
	}

	pbData = pBuffer->pbData + pBuffer->ulCurrentSize;
	pBuffer->ulCurrentSize += ulLength;
	
	return ( pbData );
}

//*****************************************************************************
//
void NETWORK_Write( NETBUFFER_s *pBuffer, const void *pvData, int nLength )
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
void NETWORK_Write( NETBUFFER_s *pBuffer, BYTE *pbData, int nStartPos, int nLength )
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
char *NETWORK_AddressToString( NETADDRESS_s Address )
{
	static char	s_szAddress[64];

	sprintf( s_szAddress, "%i.%i.%i.%i:%i", Address.abIP[0], Address.abIP[1], Address.abIP[2], Address.abIP[3], ntohs( Address.usPort ));

	return ( s_szAddress );
}

//*****************************************************************************
//
char *NETWORK_AddressToStringIgnorePort( NETADDRESS_s Address )
{
	static char	s_szAddress[64];

	sprintf( s_szAddress, "%i.%i.%i.%i", Address.abIP[0], Address.abIP[1], Address.abIP[2], Address.abIP[3] );

	return ( s_szAddress );
}

//*****************************************************************************
//
void NETWORK_NetAddressToSocketAddress( NETADDRESS_s &Address, struct sockaddr_in &SocketAddress )
{
	// Initialize the socket address.
	memset( &SocketAddress, 0, sizeof( SocketAddress ));

	// Set the socket's address and port.
	*(int *)&SocketAddress.sin_addr = *(int *)&Address.abIP;
	SocketAddress.sin_port = Address.usPort;

	// Set the socket address's family (what does this do?).
	SocketAddress.sin_family = AF_INET;
}

//*****************************************************************************
//
void NETWORK_SetAddressPort( NETADDRESS_s &Address, USHORT usPort )
{
	Address.usPort = htons( usPort );
}

//*****************************************************************************
//
bool NETWORK_CompareAddress( NETADDRESS_s Address1, NETADDRESS_s Address2, bool bIgnorePort )
{
	if (( Address1.abIP[0] == Address2.abIP[0] ) &&
		( Address1.abIP[1] == Address2.abIP[1] ) &&
		( Address1.abIP[2] == Address2.abIP[2] ) &&
		( Address1.abIP[3] == Address2.abIP[3] ) &&
		( bIgnorePort ? 1 : ( Address1.usPort == Address2.usPort )))
	{
		return ( true );
	}

	return ( false );
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
NETADDRESS_s NETWORK_GetLocalAddress( void )
{
	char				szBuffer[512];
	struct sockaddr_in	SocketAddress;
	NETADDRESS_s		Address;
	int					iNameLength;

#ifndef __WINE__
	gethostname( szBuffer, 512 );
#endif
	szBuffer[512-1] = 0;

	// Convert the host name to our local 
	NETWORK_StringToAddress( szBuffer, &Address );

	iNameLength = sizeof( SocketAddress );
#ifndef	WIN32
	if ( getsockname ( g_NetworkSocket, (struct sockaddr *)&SocketAddress, (socklen_t *)&iNameLength) == -1 )
#else
	if ( getsockname ( g_NetworkSocket, (struct sockaddr *)&SocketAddress, &iNameLength ) == -1 )
#endif
	{
		Printf( "NETWORK_GetLocalAddress: Error getting socket name: %s", strerror( errno ));
	}

	Address.usPort = SocketAddress.sin_port;
	return ( Address );
}

//*****************************************************************************
//
LONG NETWORK_GetPacketSize( void )
{
	return ( g_NetworkMessage.ulCurrentSize );
}

//*****************************************************************************
//
NETBUFFER_s *NETWORK_GetNetworkMessageBuffer( void )
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
//
USHORT NETWORK_GetLocalPort( void )
{
	return ( g_usLocalPort );
}

//*****************************************************************************
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
bool network_BindSocketToPort( SOCKET Socket, USHORT usPort, bool bReUse )
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

void I_DoSelect (void)
{
    struct timeval   timeout;
    fd_set           fdset;

    FD_ZERO(&fdset);
    FD_SET(g_NetworkSocket, &fdset);
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    if (select (static_cast<int>(g_NetworkSocket)+1, &fdset, NULL, NULL, &timeout) == -1)
        return;
}

//*****************************************************************************
//
void convertWeaponNameToKeyLetter( const char *&pszName ){
	//Printf( "converted %s ", pszName );
	for( int i = 0; i < NUMBER_OF_WEAPON_NAME_KEY_LETTERS; i++ ){
		if ( stricmp( pszName, g_szWeaponFullName[i] ) == 0 ){
			pszName = g_szWeaponKeyLetter[i];
			break;
		}
	}
	//Printf( "to to %s\n", pszName );
}

//*****************************************************************************
//
void convertWeaponKeyLetterToFullString( const char *&pszName ){
	//Printf( "recieved %s ", pszName );
	for( int i = 0; i < NUMBER_OF_WEAPON_NAME_KEY_LETTERS; i++ ){
		if ( stricmp( pszName, g_szWeaponKeyLetter[i] ) == 0 ){
			pszName = g_szWeaponFullName[i];
			break;
		}
	}
	//Printf( "converted to %s\n", pszName );
}

//*****************************************************************************
//
void generateMapLumpMD5Hash( MapData *Map, const LONG LumpNumber, char *MD5Hash ){
	LONG lLumpSize = Map->Size( LumpNumber );
	BYTE *pbData = new BYTE[lLumpSize];

	// Dump the data from the lump into our data buffer.
	Map->Read( LumpNumber, pbData );

	// Perform the checksum on our buffer, and free it.
	CMD5Checksum::GetMD5( pbData, lLumpSize, MD5Hash );
	delete ( pbData );
}

//*****************************************************************************
//	CONSOLE COMMANDS

CCMD( ip )
{
	NETADDRESS_s	LocalAddress;

	// The network module isn't initialized in these cases.
	if (( NETWORK_GetState( ) != NETSTATE_SERVER ) &&
		( NETWORK_GetState( ) != NETSTATE_CLIENT ))
	{
		return;
	}

	LocalAddress = NETWORK_GetLocalAddress( );

	Printf( PRINT_HIGH, "IP address is %s\n", NETWORK_AddressToString( LocalAddress ));
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
void NETWORK_FillBufferWithShit( NETBUFFER_s *pBuffer, ULONG ulSize )
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
