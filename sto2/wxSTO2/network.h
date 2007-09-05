//-----------------------------------------------------------------------------
//
// Skulltag Online 2
// Copyright (C) 2007 Rivecoder, Brad Carney, Benjamin Berkels
// Date created: 8/28/07
//
// Skulltag networking code.
//-----------------------------------------------------------------------------

#ifndef __NETWORK_H__
#define __NETWORK_H__

#include "../../src/networkheaders.h"
#include "../../src/networkshared.h"

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

#define DEFAULT_STO2_PORT		10856
#define	DEFAULT_SERVER_PORT		10666
#define	DEFAULT_CLIENT_PORT		10667
#define	DEFAULT_MASTER_PORT		15300
#define	DEFAULT_BROADCAST_PORT	15101

// Should we use huffman compression?
#define	USE_HUFFMAN_COMPRESSION

// This is the longest possible string we can pass over the network.
#define	MAX_NETWORK_STRING			2048

//*****************************************************************************
enum
{
	// Client has the wrong password.
	NETWORK_ERRORCODE_WRONGPASSWORD,

	// Client has the wrong version.
	NETWORK_ERRORCODE_WRONGVERSION,

	// Client is using a version with different network protocol.
	NETWORK_ERRORCODE_WRONGPROTOCOLVERSION,

	// Client has been banned.
	NETWORK_ERRORCODE_BANNED,

	// The server is full.
	NETWORK_ERRORCODE_SERVERISFULL,

	// Client has the wrong version of the current level.
	NETWORK_ERRORCODE_AUTHENTICATIONFAILED,

	// Client failed to send userinfo when connecting.
	NETWORK_ERRORCODE_FAILEDTOSENDUSERINFO,

	NUM_NETWORK_ERRORCODES
};

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
//bool			NETWORK_StringToAddress( char *pszString, NETADDRESS_s *pAddress );
bool			NETWORK_CompareAddress( NETADDRESS_s Address1, NETADDRESS_s Address2, bool bIgnorePort );
//void			NETWORK_SocketAddressToNetAddress( struct sockaddr_in *s, NETADDRESS_s *a );
void			NETWORK_NetAddressToSocketAddress( NETADDRESS_s &Address, struct sockaddr_in &SocketAddress );
void			NETWORK_SetAddressPort( NETADDRESS_s &Address, USHORT usPort );
NETADDRESS_s	NETWORK_GetLocalAddress( void );
NETBUFFER_s		*NETWORK_GetNetworkMessageBuffer( void );
ULONG			NETWORK_ntohs( ULONG ul );
USHORT			NETWORK_GetLocalPort( void );

void			I_DoSelect( void );

#endif	// __NETWORK_H__
