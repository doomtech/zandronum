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

#include <stdio.h>
//#include "c_cvars.h"
//#include "d_player.h"
//#include "i_net.h"
//#include "sv_main.h"
#include "networkshared.h"

//*****************************************************************************
//	DEFINES

// Server is letting master server of its existance, or sending info to the launcher.
#define	MASTER_CHALLENGE		5660020

// Server is letting master server of its existance, along with sending an IP the master server
// should use for this server.
#define	MASTER_CHALLENGE_OVERRIDE	5660021

// Launcher is querying the server, or master server.
#define	LAUNCHER_CHALLENGE		199

#define	DEFAULT_SERVER_PORT		10666
#define	DEFAULT_CLIENT_PORT		10667
#define	DEFAULT_MASTER_PORT		15300
#define	DEFAULT_BROADCAST_PORT	15101

// Connection messages.
#define CONNECT_CHALLENGE		200
#define	CONNECT_READY			201
#define	CONNECT_GETDATA			202
#define	CONNECT_QUIT			203

// Network messages (universal)
#define	NETWORK_ERROR			254

// Movement stuff.
#define CM_X			1
#define CM_Y			2
#define CM_Z			4
#define CM_ANGLE		8
#define CM_MOMX			16
#define CM_MOMY			32
#define CM_MOMZ			64
#define CM_COLORMAP		128
#define	CM_WATERLEVEL	256

// Extra player update info for spectators.
#define	PLAYER_UPDATE_WEAPON	1
#define	PLAYER_UPDATE_PITCH		2

// Movement flags being sent by the client.
#define	CLIENT_UPDATE_BUTTONS		1
#define	CLIENT_UPDATE_PITCH			2
#define	CLIENT_UPDATE_YAW			4
#define	CLIENT_UPDATE_FORWARDMOVE	8
#define	CLIENT_UPDATE_SIDEMOVE		16
#define	CLIENT_UPDATE_UPMOVE		32
#define	CLIENT_UPDATE_ROLL			64

// Identifying states (the cheap & easy way out)
#define	STATE_SPAWN				1
#define	STATE_SEE				2
#define	STATE_PAIN				3
#define	STATE_MELEE				4					
#define	STATE_MISSILE			5
#define	STATE_DEATH				6
#define	STATE_XDEATH			7
#define	STATE_RAISE				8
#define	STATE_HEAL				9

// Identifying player states (again, cheap & easy)
#define	STATE_PLAYER_IDLE		1
#define	STATE_PLAYER_SEE		2
#define	STATE_PLAYER_ATTACK		3
#define	STATE_PLAYER_ATTACK2	4

// HUD message types.
#define	HUDMESSAGETYPE_NORMAL			1
#define	HUDMESSAGETYPE_FADEOUT			2
#define	HUDMESSAGETYPE_TYPEONFADEOUT	3

// Different levels of network messages.
#define	NETMSG_LITE		0
#define	NETMSG_MEDIUM	1
#define	NETMSG_HIGH		2

// Which actor flags are being updated?
#define	UPDATE_ACTORFLAGS		1
#define	UPDATE_ACTORFLAGS2		2
#define	UPDATE_ACTORFLAGS3		4
#define	UPDATE_ACTORFLAGS4		8
#define	UPDATE_ACTORFLAGS5		16
#define	UPDATE_ACTORFLAGS6		32

// Which userinfo categories are being updated?
#define	USERINFO_NAME				1
#define	USERINFO_GENDER				2
#define	USERINFO_COLOR				4
#define	USERINFO_AIMDISTANCE		8
#define	USERINFO_SKIN				16
#define	USERINFO_RAILCOLOR			32
#define	USERINFO_HANDICAP			64
#define	USERINFO_CONNECTIONTYPE		128
#define	USERINFO_PLAYERCLASS		256

//*****************************************************************************
enum
{
	// Program is being run in single player mode.
	NETSTATE_SINGLE,

	// Program is being run in single player mode, emulating a network game (bots, etc).
	NETSTATE_SINGLE_MULTIPLAYER,

	// Program is a client playing a network game.
	NETSTATE_CLIENT,

	// Program is a server, hosting a game.
	NETSTATE_SERVER,

	NUM_NETSTATES
};

//*****************************************************************************
enum
{
	MSC_BEGINSERVERLIST,
	MSC_SERVER,
	MSC_ENDSERVERLIST,
	MSC_IPISBANNED,
	MSC_REQUESTIGNORED,
};

typedef	unsigned short		USHORT;
typedef	short				SHORT;

typedef	unsigned long		ULONG;
typedef	long				LONG;

//typedef	signed char			BYTE;

//*****************************************************************************

//*****************************************************************************
//	PROTOTYPES

USHORT	NETWORK_GetLocalPort( void );
void	NETWORK_SetLocalPort( USHORT usPort );

void	NETWORK_Initialize( void );

int		NETWORK_ReadChar( void );
void	NETWORK_WriteChar( sizebuf_t *pBuffer, int Char );

int		NETWORK_ReadByte( void );
void	NETWORK_WriteByte( sizebuf_t *pBuffer, int Byte );

int		NETWORK_ReadShort( void );
void	NETWORK_WriteShort( sizebuf_t *pBuffer, int Short );

int		NETWORK_ReadLong( void );
void	NETWORK_WriteLong( sizebuf_t *pBuffer, int Long );

float	NETWORK_ReadFloat( void );
void	NETWORK_WriteFloat( sizebuf_t *pBuffer, float Float );

char	*NETWORK_ReadString( void );
void	NETWORK_WriteString( sizebuf_t *pBuffer, char *pszString );

// Debugging function.
void	NETWORK_WriteHeader( sizebuf_t *pBuffer, int Byte );

void	NETWORK_CheckBuffer( ULONG ulClient, ULONG ulSize );
int		NETWORK_GetPackets( void );
void	NETWORK_LaunchPacket( sizebuf_t netbuf, netadr_t to, bool bCompression );
void	NETWORK_LaunchPacket( sizebuf_t netbuf, netadr_t to );
void	NETWORK_InitBuffer( sizebuf_t *pBuffer, USHORT usLength );
void	NETWORK_FreeBuffer( sizebuf_t *pBuffer );
void	NETWORK_ClearBuffer( sizebuf_t *pBuffer );
BYTE	*NETWORK_GetSpace( sizebuf_t *pBuffer, USHORT usLength );
void	NETWORK_Write( sizebuf_t *pBuffer, void *pvData, int nLength );
void	NETWORK_Write( sizebuf_t *pBuffer, BYTE *pbData, int nStartPos, int nLength );
void	NETWORK_Print( sizebuf_t *pBuffer, char *pszData );	// strcats onto the sizebuf
char	*NETWORK_AddressToString( netadr_t Address );
//bool	NETWORK_StringToAddress( char *pszString, netadr_t *pAddress );
bool	NETWORK_CompareAddress( netadr_t a, netadr_t b, bool bIgnorePort );
//void	NETWORK_SocketAddressToNetAddress( struct sockaddr_in *s, netadr_t *a );
void	NETWORK_NetAddressToSocketAddress( netadr_t *a, struct sockaddr_in *s );

void	I_DoSelect( void );
void	I_SetPort( netadr_t &addr, int port );

// DEBUG FUNCTION!
void	NETWORK_FillBufferWithShit( sizebuf_t *pBuffer, ULONG ulSize );

//*****************************************************************************
//	EXTERNAL VARIABLES THAT MUST BE FIXED

extern	netadr_t	g_AddressFrom;


#endif	// __NETWORK_H__
