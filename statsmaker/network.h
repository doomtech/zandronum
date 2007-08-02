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

// Maximum size of the packets sent out by the server.
#define	MAX_UDP_PACKET				8192

// Launcher is querying the server, or master server.
#define	LAUNCHER_CHALLENGE		199

#define	DEFAULT_SERVER_PORT		10666
#define	DEFAULT_CLIENT_PORT		10667
#define	DEFAULT_MASTER_PORT		15200
#define	DEFAULT_BROADCAST_PORT	15101
#define	DEFAULT_STATS_PORT		15201

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
	// Client has the wrong password.
	NETWORK_ERRORCODE_WRONGPASSWORD,

	// Client has the wrong version.
	NETWORK_ERRORCODE_WRONGVERSION,

	// Client has been banned.
	NETWORK_ERRORCODE_BANNED,

	// The server is full.
	NETWORK_ERRORCODE_SERVERISFULL,

	NUM_NETWORK_ERRORCODES
};

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
// Note: If the number of enumerated messages goes beyond 255, commands will need 
// to be changed to a short.
typedef enum
{
	SVC_HEADER,
	SVC_PRINT,
	SVC_CONSOLEPLAYER,
	SVC_DMFLAGS,
	SVC_GAMESKILL,
	SVC_GAMETYPE,
	SVC_LIMITS,
	SVC_LOADMAP,
	SVC_SPAWNPLAYER,
	SVC_DISCONNECTCLIENT,
	SVC_TOUCHTHING,				// 10
	SVC_STARTCHAT,
	SVC_ENDCHAT,
	SVC_SAY,
	SVC_MOVEPLAYER,
	SVC_UPDATELOCALPLAYER,
	SVC_SPAWNMOBJ,
	SVC_MISSEDPACKET,
	SVC_UPDATEPING,
	SVC_PING,
	SVC_USERINFO,				// 20
	SVC_UPDATEFRAGS,
	SVC_MOBJANGLE,
	SVC_CORPSE,
	SVC_SPAWNGENERICMISSILE,
	SVC_EXPLODEMISSILE,
	SVC_RESPAWNITEM,
	SVC_DAMAGEMOBJ,
	SVC_DAMAGEPLAYER,
	SVC_MOVETHING,
	SVC_KILLMOBJ,				// 30
	SVC_KILLPLAYER,
	SVC_GIVEMEDAL,
	SVC_TOGGLELINE,
	SVC_TAUNT,
	SVC_MOBJSTATE,
	SVC_FIST,
	SVC_SAW,
	SVC_FIREPISTOL,
	SVC_FIRESHOTGUN,
	SVC_FIRESUPERSHOTGUN,		// 40
	SVC_OPENSUPERSHOTGUN,
	SVC_LOADSUPERSHOTGUN,
	SVC_CLOSESUPERSHOTGUN,
	SVC_FIRECHAINGUN,
	SVC_FIREMINIGUN,
	SVC_FIREROCKETLAUNCHER,
	SVC_FIREGRENADELAUNCHER,
	SVC_FIREPLASMAGUN,
	SVC_FIRERAILGUN,
	SVC_FIREBFG,				// 50
	SVC_FIREBFG10K,
	SVC_UPDATESECTORFLAT,
	SVC_NEWMAP,
	SVC_EXITLEVEL,
	SVC_NEWMAP2,
	SVC_SECTORSOUND,
	SVC_SECTORSOUNDID,
	SVC_STOPSECTORSEQUENCE,
	SVC_ENDLEVELDELAY,
	SVC_UPDATETEAMFRAGS,		// 60
	SVC_UPDATETEAMSCORES,
	SVC_UPDATEPLAYERTEAM,
//	SVC_FLAGTAKEN,
	SVC_TELEPORT,
	SVC_HIDEMOBJ,
	SVC_FLAGRETURNED,
	SVC_REMOVEPLAYERITEM,
	SVC_DESTROYTHING,
	SVC_GIVETHING,
	SVC_MIDPRINT,				// 70
	SVC_UPDATEPOINTS,
	SVC_DROPPEDITEM,
	SVC_PLAYERSPECTATING,
	SVC_WEAPONCHANGE,
	SVC_PLAYERNOWAY,
	SVC_FLATTEXTURE,
	SVC_UPDATEKILLCOUNT,
	SVC_PLAYERKILLEDMONSTER,
	SVC_PLAYERSTATE,
	SVC_MOTD,					// 80
	SVC_KEYFAIL,
	SVC_KICKED,
	SVC_UPDATECOLORMAP,
	SVC_UPDATEPLAYEREXTRA,
	SVC_LEVELTIME,
	SVC_NOTHING,
	SVC_CLIENTLAGGING,
	SVC_CLIENTNOTLAGGING,
	SVC_ARCHVILEATTACK,
	SVC_SETFRAME,				// 90
	SVC_SPAWNPLASMABALL,
	SVC_SPAWNROCKET,
	SVC_SPAWNGRENADE,
	SVC_RESPAWNITEMNOFOG,
	SVC_CEILINGPANNING,
	SVC_FLOORPANNING,
	SVC_SECTORCOLOR,
	SVC_SECTORROTATION,
	SVC_SECTORFADE,
	SVC_ROTATEPOLY,				// 100
	SVC_MOVEPOLY,
	SVC_OPENPOLYDOOR,
	SVC_SPAWNFATSHOT,
	SVC_HUDMESSAGE,
	SVC_ACTORPROPERTY,
	SVC_PLAYERPROPERTY,
	SVC_SECTORLIGHTLEVEL,
	SVC_ACTORACTIVATE,
	SVC_ACTORDEACTIVATE,
	SVC_GIVEINVENTORY,			// 110
	SVC_BEGINSNAPSHOT,
	SVC_ENDSNAPSHOT,
	SVC_RETURNTICKS,
	SVC_LINEALPHA,
	SVC_LINETEXTURE,
	SVC_SOUND,
	SVC_SOUNDACTOR,
	SVC_SOUNDPOINT,
	SVC_USEINVENTORY,	
	SVC_PLAYERHEALTH,			// 120
	SVC_PLAYERLANDED,
	SVC_TAKEINVENTORY,
	SVC_FLASHFADER,
	SVC_CHANGEMUSIC,
	SVC_DODOOR,
	SVC_DOFLOOR,
	SVC_DOCEILING,
	SVC_DOPLAT,
	SVC_BUILDSTAIRS,
	SVC_GENERICCHEAT,			// 130
	SVC_GIVECHEAT,
	SVC_DOELEVATOR,
	SVC_STARTWAGGLE,
	SVC_SETFLOORPLANE,
	SVC_SETCEILINGPLANE,
	SVC_SPAWNBULLETPUFF,
	SVC_DODONUT,
	SVC_SPRINGPADZONE,
	SVC_ACTORFLAGS,
	SVC_SETPOLYPOSITION,		// 140
	SVC_FIGHT,
	SVC_STARTCOUNTDOWN,
	SVC_DOWINSEQUENCE,
	SVC_UPDATEWINS,
	SVC_PLAYWEAPONIDLESOUND,
	SVC_UPDATEPOWERUP,
	SVC_UPDATERUNE,
	SVC_REQUESTCHECKSUM,
	SVC_UPDATEDUELS,
	SVC_RANDOMPOWERUP,			// 150
	SVC_READYTOGOON,
	SVC_UPDATETIME,
	SVC_MODESTATE,
	SVC_RAILOFFSET,
	SVC_CEILINGINPROGRESS,

	NUM_SERVER_COMMANDS

};

//*****************************************************************************
enum
{
	CLC_USERINFO,
	CLC_STARTCHAT,
	CLC_ENDCHAT,
	CLC_SAY,
	CLC_CLIENTMOVE,
	CLC_ACK,
	CLC_PONG,
	CLC_WEAPONSLOT,
	CLC_WEAPONSELECT,
	CLC_MISSINGPLAYER,
	CLC_TAUNT,
	CLC_SPECTATE,
	CLC_REQUESTJOIN,
	CLC_QUERYTHING,
	CLC_REQUESTRCON,
	CLC_RCONCOMMAND,
	CLC_SUICIDE,
	CLC_CHANGETEAM,
	CLC_USEINVENTORY,
	CLC_SPECTATEINFO,
	CLC_GENERICCHEAT,
	CLC_GIVECHEAT,
	CLC_SUMMONCHEAT,
	CLC_CHECKSUM,
	CLC_READYTOGOON,

	NUM_CLIENT_COMMANDS

};

//*****************************************************************************
enum
{
	MSC_BEGINSERVERLIST,
	MSC_SERVER,
	MSC_ENDSERVERLIST,
};

typedef	unsigned short		USHORT;
typedef	short				SHORT;

typedef	unsigned long		ULONG;
typedef	long				LONG;

//typedef	signed char			BYTE;

typedef struct
{
   BYTE    ip[4];
   unsigned short  port;
   unsigned short  pad;
} netadr_t;

//*****************************************************************************
typedef struct sizebuf_s
{
	bool	allowoverflow;	// if false, do a Com_Error
	bool	overflowed;		// set to true if the buffer size failed

	// Unfortunaly, ZDaemon uses two different definitions of sizebuf_t. Attempt
	// to combine the structures here by having two sets of data.
	// Servers use this.
	BYTE	*pbData;

	// Clients use this this.
	BYTE	bData[MAX_UDP_PACKET];

	int		maxsize;
	int		cursize;
	int		readcount;

} sizebuf_t;

//*****************************************************************************
//	PROTOTYPES

void	NETWORK_Construct( void );

LONG	NETWORK_GetState( void );
void	NETWORK_SetState( LONG lState );

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
bool	NETWORK_StringToAddress( char *pszString, netadr_t *pAddress );
bool	NETWORK_CompareAddress( netadr_t a, netadr_t b );
void	NETWORK_SocketAddressToNetAddress( struct sockaddr_in *s, netadr_t *a );
void	NETWORK_NetAddressToSocketAddress( netadr_t *a, struct sockaddr_in *s );

void	I_DoSelect( void );
void	I_SetPort( netadr_t &addr, int port );

// DEBUG FUNCTION!
void	NETWORK_FillBufferWithShit( sizebuf_t *pBuffer, ULONG ulSize );

//*****************************************************************************
//	EXTERNAL VARIABLES THAT MUST BE FIXED

extern	netadr_t	g_AddressFrom;


#endif	// __NETWORK_H__