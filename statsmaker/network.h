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
// Filename: network.h
//
// Description: Contains network definitions and functions not specifically
// related to the server or client.
//
//-----------------------------------------------------------------------------

#ifndef __NETWORK_H__
#define __NETWORK_H__

//*****************************************************************************
//	DEFINES

enum
{
	// Server is letting master server of its existance.
	SERVER_MASTER_CHALLENGE = 5660020,

	// Server is letting master server of its existance, along with sending an IP the master server
	// should use for this server.
	SERVER_MASTER_CHALLENGE_OVERRIDE,

	// Server is sending some statistics to the master server.
	SERVER_MASTER_STATISTICS,

	// Server is sending its info to the launcher.
	SERVER_LAUNCHER_CHALLENGE,

	// Server is telling a launcher that it's ignoring it.
	SERVER_LAUNCHER_IGNORING,

	// Server is telling a launcher that his IP is banned from the server.
	SERVER_LAUNCHER_BANNED,

	// Client is trying to create a new account with the master server.
	CLIENT_MASTER_NEWACCOUNT,

	// Client is trying to log in with the master server.
	CLIENT_MASTER_LOGIN,

};

// Launcher is querying the server, or master server.
#define	LAUNCHER_SERVER_CHALLENGE	199

#define	DEFAULT_SERVER_PORT			10666
#define	DEFAULT_CLIENT_PORT			10667
#define	DEFAULT_MASTER_PORT			15300
#define	DEFAULT_BROADCAST_PORT		15101
#define	DEFAULT_STATS_PORT			15201

// Should we use huffman compression?
#define	USE_HUFFMAN_COMPRESSION

// This is the longest possible string we can pass over the network.
#define	MAX_NETWORK_STRING			2048

//*****************************************************************************
enum
{
	MSC_BEGINSERVERLIST,
	MSC_SERVER,
	MSC_ENDSERVERLIST,
	MSC_IPISBANNED,
	MSC_REQUESTIGNORED,
	MSC_AUTHENTICATEUSER,
	MSC_INVALIDUSERNAMEORPASSWORD,
	MSC_ACCOUNTALREADYEXISTS,

};

//*****************************************************************************
//	PROTOTYPES

void			NETWORK_Construct( USHORT usPort, bool bAllocateLANSocket );
void			NETWORK_Destruct( void );

int				NETWORK_ReadByte( BYTESTREAM_s *pByteStream );
void			NETWORK_WriteByte( BYTESTREAM_s *pByteStream, int Byte );

int				NETWORK_ReadShort( BYTESTREAM_s *pByteStream );
void			NETWORK_WriteShort( BYTESTREAM_s *pByteStream, int Short );

int				NETWORK_ReadLong( BYTESTREAM_s *pByteStream );
void			NETWORK_WriteLong( BYTESTREAM_s *pByteStream, int Long );

float			NETWORK_ReadFloat( BYTESTREAM_s *pByteStream );
void			NETWORK_WriteFloat( BYTESTREAM_s *pByteStream, float Float );

char			*NETWORK_ReadString( BYTESTREAM_s *pByteStream );
void			NETWORK_WriteString( BYTESTREAM_s *pByteStream, const char *pszString );

void			NETWORK_WriteBuffer( BYTESTREAM_s *pByteStream, const void *pvBuffer, int nLength );

// Debugging function.
void			NETWORK_WriteHeader( BYTESTREAM_s *pByteStream, int Byte );

int				NETWORK_GetPackets( void );
int				NETWORK_GetLANPackets( void );
NETADDRESS_s	NETWORK_GetFromAddress( void );
void			NETWORK_LaunchPacket( NETBUFFER_s *pBuffer, NETADDRESS_s Address );
void			NETWORK_InitBuffer( NETBUFFER_s *pBuffer, ULONG ulLength, BUFFERTYPE_e BufferType );
void			NETWORK_FreeBuffer( NETBUFFER_s *pBuffer );
void			NETWORK_ClearBuffer( NETBUFFER_s *pBuffer );
LONG			NETWORK_CalcBufferSize( NETBUFFER_s *pBuffer );
char			*NETWORK_AddressToString( NETADDRESS_s Address );
char			*NETWORK_AddressToStringIgnorePort( NETADDRESS_s Address );
bool			NETWORK_CompareAddress( NETADDRESS_s Address1, NETADDRESS_s Address2, bool bIgnorePort );
void			NETWORK_NetAddressToSocketAddress( NETADDRESS_s &Address, struct sockaddr_in &SocketAddress );
void			NETWORK_SetAddressPort( NETADDRESS_s &Address, USHORT usPort );
NETADDRESS_s	NETWORK_GetLocalAddress( void );
NETBUFFER_s		*NETWORK_GetNetworkMessageBuffer( void );
ULONG			NETWORK_ntohs( ULONG ul );
USHORT			NETWORK_GetLocalPort( void );

void			I_DoSelect( void );

#endif	// __NETWORK_H__
