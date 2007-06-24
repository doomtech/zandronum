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

#include "huffman.h"
#include "network.h"

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

//*****************************************************************************
//	PROTOTYPES

static	void	network_Error( char *pszError );
static	SOCKET	network_AllocateSocket( void );
static	bool	network_BindSocketToPort( int Socket, USHORT usPort );
static	void	network_GetLocalAddress( void );

//*****************************************************************************
//	FUNCTIONS

//*****************************************************************************
//
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

	printf( "Winsock initialization succeeded!\n" );
#endif

	g_NetworkSocket = network_AllocateSocket( );
	if ( network_BindSocketToPort( g_NetworkSocket, g_usLocalPort ) == false )
	{
		USHORT	usPort;
		bool	bSuccess;

		bSuccess = true;
		usPort = g_usLocalPort;
		while ( network_BindSocketToPort( g_NetworkSocket, ++usPort ) == false )
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
			printf( "NETWORK_Initialize: Couldn't bind to %d. Binding to %d instead...\n", g_usLocalPort, usPort );
			g_usLocalPort = usPort;
		}
	}
	if ( ioctlsocket( g_NetworkSocket, FIONBIO, &_true ) == -1 )
		printf( "network_AllocateSocket: ioctl FIONBIO: %s", strerror( errno ));
/*
	// If we're not starting a server, setup a socket to listen for LAN servers.
	if ( Args.CheckParm( "-host" ) == false )
	{
		g_LANSocket = network_AllocateSocket( );
		if ( network_BindSocketToPort( g_LANSocket, DEFAULT_BROADCAST_PORT ) == false )
		{
			sprintf( szString, "network_BindSocketToPort: Couldn't bind LAN socket to port: %d. You will not be able to see LAN servers in the browser.", DEFAULT_BROADCAST_PORT );
			network_Error( szString );
		}
		if ( ioctlsocket( g_LANSocket, FIONBIO, &_true ) == -1 )
			printf( "network_AllocateSocket: ioctl FIONBIO: %s", strerror( errno ));
	}
*/
	// Init our read buffer.
	NETWORK_InitBuffer( &g_NetworkMessage, MAX_UDP_PACKET );
	NETWORK_ClearBuffer( &g_NetworkMessage );

	// Determine local name & address.
	network_GetLocalAddress( );

	printf( "UDP Initialized.\n" );
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
	byte	*pbBuf;
	
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
	byte	*pbBuf;
	
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
	byte	*pbBuf;
	
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
		byte	b[4];
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
//	if ( g_lNetworkState == NETSTATE_SERVER )
//	{
//		if ( debugfile )
//			fprintf( debugfile, "Wrote: %s\n", g_pszServerHeaderNames[Byte] );
//		Printf( "Wrote: %s\n", g_pszServerHeaderNames[Byte] );
//	}
//#endif
	NETWORK_WriteByte( pBuffer, Byte );
}

//*****************************************************************************
//
void NETWORK_CheckBuffer( ULONG ulClient, ULONG ulSize )
{
/*
//	if ( debugfile )
//		fprintf( debugfile, "Current packet size: %d\nSize of data being added to packet: %d\n", clients[ulClient].netbuf.cursize, ulSize );

	// Make sure we have enough room for the upcoming message. If not, send
	// out the current buffer and clear the packet.
//	if (( clients[ulClient].netbuf.cursize + ( ulSize + 5 )) >= (ULONG)clients[ulClient].netbuf.maxsize )
	if (( clients[ulClient].netbuf.cursize + ( ulSize + 5 )) >= (ULONG)sv_maxpacketsize )
	{
//		Printf( "Launching premature packet\n" );
		if ( debugfile )
			fprintf( debugfile, "Launching premature packet\n" );

		// Lanch the packet so we can prepare another.
		NETWORK_SendPacket( ulClient );

		// Now that the packet has been sent, clear it.
		NETWORK_ClearBuffer( &clients[ulClient].netbuf );
	}
*/
}

//*****************************************************************************
//
static unsigned char huffbuff[65536];
int NETWORK_GetPackets( void )
{
	int 	ret;
	struct sockaddr_in	from;
	int		fromlen;

    fromlen = sizeof(from);
    
#ifdef	WIN32
	ret = recvfrom( g_NetworkSocket, (char *)huffbuff, sizeof(huffbuff), 0, (struct sockaddr *)&from, &fromlen);
#else
	ret = recvfrom( g_NetworkSocket, (char *)huffbuff, sizeof(huffbuff), 0, (struct sockaddr *)&from, (socklen_t *)&fromlen);
#endif

    if (ret == -1) 
    { 
#ifdef __WIN32__
        errno = WSAGetLastError();

        if (errno == WSAEWOULDBLOCK)
            return false;
		
		// connection reset by peer...dosent mean anything to the server
		if (errno == WSAECONNRESET)
			return false;

        if (errno == WSAEMSGSIZE) 
		{
             printf ("Warning:  Oversize packet from %s\n",
                      NETWORK_AddressToString( g_AddressFrom ));
             return false;
        }

        printf ("ZD_GetPackets(%d) Error(%d): %s\n", ret, errno, strerror(errno));
		return false;
#else
        if (errno == EWOULDBLOCK)
            return false;
        if (errno == ECONNREFUSED)
            return false;
	    
        printf ("ZD_GetPackets: %s\n", strerror(errno));
        return false;
#endif
    }

	// No packets or an error, dont process anything.
	if ( ret <= 0 )
		return 0;

	// Not using anymore gay ass huffman encoding
	if ( g_lNetworkState == NETSTATE_SERVER )
		HuffDecode(huffbuff, (unsigned char *)g_NetworkMessage.pbData,ret,&ret);
	else
		HuffDecode(huffbuff, (unsigned char *)g_NetworkMessage.bData,ret,&ret);

	g_NetworkMessage.readcount = 0;
	g_NetworkMessage.cursize = ret;

    NETWORK_SocketAddressToNetAddress( &from, &g_AddressFrom );

//	Printf( PRINT_HIGH, "*** got THIS: %d\n", g_NetworkMessage.cursize );
	
	return ( g_NetworkMessage.cursize );
}

//*****************************************************************************
//
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
	}
	else
		HuffEncode((unsigned char *)netbuf.bData, huffbuff, netbuf.cursize, &outlen);

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

			  printf( "NETWORK_LaunchPacket: Error #%d, WSAEACCES: Permission denied for address: %s\n", err, NETWORK_AddressToString( to ));
			  break;
		  case WSAEADDRNOTAVAIL:

			  printf( "NETWORK_LaunchPacket: Error #%d, WSAEADDRENOTAVAIL: Address %s not available\n", err, NETWORK_AddressToString( to ));
			  break;
		  default:

			printf( "NETWORK_LaunchPacket: Error #%d\n", err );
			break;
		  }
#else	  
          if (errno == EWOULDBLOCK)
              return;
          if (errno == ECONNREFUSED)
              return;
          printf ("NET_SendPacket: %s\n", strerror(errno));
#endif	  
    }

}

//*****************************************************************************
//
void NETWORK_LaunchPacket( sizebuf_t netbuf, netadr_t to )
{
	int ret;
	struct sockaddr_in	addr;
	//int		 r;
	int		outlen;

	if ( netbuf.cursize <= 0 )
		return;

    NETWORK_NetAddressToSocketAddress(&to, &addr);

	if ( g_lNetworkState == NETSTATE_SERVER )
		HuffEncode((unsigned char *)netbuf.pbData,huffbuff,netbuf.cursize,&outlen);
	else
		HuffEncode((unsigned char *)netbuf.bData,huffbuff,netbuf.cursize,&outlen);

	ret = sendto (g_NetworkSocket, (const char*)huffbuff, outlen, 0, (struct sockaddr *)&addr, sizeof(addr));
    
    if (ret == -1) 
    {
#ifdef __WIN32__
          int err = WSAGetLastError();

          // wouldblock is silent
          if (err == WSAEWOULDBLOCK)
              return;
#else	  
          if (errno == EWOULDBLOCK)
              return;
          if (errno == ECONNREFUSED)
              return;
          Printf ("NET_SendPacket: %s\n", strerror(errno));
#endif	  
    }

//	zd_outbytes += outlen;
}

//*****************************************************************************
//
void NETWORK_InitBuffer( sizebuf_t *pBuffer, USHORT usLength )
{
	memset( pBuffer, 0, sizeof( *pBuffer ));
	pBuffer->maxsize = usLength;

	if ( g_lNetworkState == NETSTATE_SERVER )
		pBuffer->pbData = new byte[usLength];
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
byte *NETWORK_GetSpace( sizebuf_t *pBuffer, USHORT usLength )
{
	byte	*pbData;

	// Make sure we have enough room left in the packet.
	if ( pBuffer->cursize + usLength > pBuffer->maxsize )
	{
		if ( usLength > pBuffer->maxsize )
			printf( "NETWORK_GetSpace: %i is > full buffer size.\n", usLength );
		else
			printf( "NETWORK_GetSpace: Overflow!\n" );

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
	pBuffer->cursize += usLength;
	
	return ( pbData );
}

//*****************************************************************************
//
void NETWORK_Write( sizebuf_t *pBuffer, void *pvData, int nLength )
{
	byte	*pbDatapos;

	if ( g_lNetworkState == NETSTATE_CLIENT )
		memcpy( NETWORK_GetSpace( pBuffer, nLength ), pvData, nLength );
	else
	{
		pbDatapos = NETWORK_GetSpace( pBuffer, nLength );

		// Bad getspace.
		if( pbDatapos == 0 )
			return;
		
		memcpy( pbDatapos, pvData, nLength );
	}
}

//*****************************************************************************
//
void NETWORK_Write( sizebuf_t *pBuffer, byte *pbData, int nStartPos, int nLength )
{
	byte	*pbDatapos;

	if ( g_lNetworkState == NETSTATE_CLIENT )
	{
		pbData += nStartPos;
		memcpy( NETWORK_GetSpace( pBuffer, nLength ), pbData, nLength );
	}
	else
	{
		pbDatapos = NETWORK_GetSpace( pBuffer, nLength );

		// Bad getspace.
		if( pbDatapos == 0 )
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
			memcpy ((byte *)NETWORK_GetSpace( pBuffer, usLength ), pszData, usLength ); // no trailing 0
		else
			memcpy ((byte *)NETWORK_GetSpace( pBuffer, usLength - 1 ) - 1, pszData, usLength ); // write over trailing 0
	}
	else
		memcpy ((byte *)NETWORK_GetSpace( pBuffer, usLength ), pszData, usLength );
*/
}

//*****************************************************************************
//
char *NETWORK_AddressToString( netadr_t a )
{
     static  char    s[64];

     sprintf (s, "%i.%i.%i.%i:%i", a.ip[0], a.ip[1], a.ip[2], a.ip[3], ntohs(a.port));

     return s;
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
//*****************************************************************************
//
void network_Error( char *pszError )
{
	printf( "\\cd%s\n", pszError );
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
bool network_BindSocketToPort( int Socket, USHORT usPort )
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

	iErrorCode = bind( Socket, (sockaddr *)&address, sizeof( address ));
	if ( iErrorCode == SOCKET_ERROR )
	{
/*
		char	szString[128];

		sprintf( szString, "network_BindSocketToPort: Couldn't bind socket to port: %d", usPort );
		network_Error( szString );
*/
		return ( false );
	}

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

	printf( "IP address %s\n", NETWORK_AddressToString( g_LocalNetworkAddress ));
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
