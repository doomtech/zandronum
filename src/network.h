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

#include "c_cvars.h"
#include "d_player.h"
#include "i_net.h"
#include "p_setup.h"
#include "sv_main.h"

//*****************************************************************************
//	DEFINES

// Movement stuff.
enum
{
	CM_X			= 1 << 0,
	CM_Y			= 1 << 1,
	CM_Z			= 1 << 2,
	CM_ANGLE		= 1 << 3,
	CM_MOMX			= 1 << 4,
	CM_MOMY			= 1 << 5,
	CM_MOMZ			= 1 << 6,
	CM_PITCH		= 1 << 7,
	CM_MOVEDIR		= 1 << 8,
};

// [BB] Flags for client_MovePlayer/SERVERCOMMANDS_MovePlayer
enum
{
	PLAYER_VISIBLE		= 1 << 0,
	PLAYER_ATTACK			= 1 << 1,
	PLAYER_ALTATTACK	= 1 << 2,
};

/* [BB] This is not used anywhere anymore.
// Should we use huffman compression?
#define	USE_HUFFMAN_COMPRESSION

// Extra player update info for spectators.
#define	PLAYER_UPDATE_WEAPON	1
#define	PLAYER_UPDATE_PITCH		2
*/

// Movement flags being sent by the client.
#define	CLIENT_UPDATE_YAW				0x01
#define	CLIENT_UPDATE_PITCH				0x02
#define	CLIENT_UPDATE_ROLL				0x04
#define	CLIENT_UPDATE_BUTTONS			0x08
#define	CLIENT_UPDATE_FORWARDMOVE		0x10
#define	CLIENT_UPDATE_SIDEMOVE			0x20
#define	CLIENT_UPDATE_UPMOVE			0x40
#define	CLIENT_UPDATE_BUTTONS_LONG		0x80

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
#define	STATE_CRASH				10

// Identifying player states (again, cheap & easy)
typedef enum
{
	STATE_PLAYER_IDLE,
	STATE_PLAYER_SEE,
	STATE_PLAYER_ATTACK,
	STATE_PLAYER_ATTACK2,
	STATE_PLAYER_ATTACK_ALTFIRE,
} PLAYERSTATE_e;

// HUD message types.
#define	HUDMESSAGETYPE_NORMAL			1
#define	HUDMESSAGETYPE_FADEOUT			2
#define	HUDMESSAGETYPE_TYPEONFADEOUT	3
#define	HUDMESSAGETYPE_FADEINOUT		4

// Different levels of network messages.
#define	NETMSG_LITE		0
#define	NETMSG_MEDIUM	1
#define	NETMSG_HIGH		2

// Which actor flags are being updated?
#define	FLAGSET_FLAGS		1
#define	FLAGSET_FLAGS2		2
#define	FLAGSET_FLAGS3		3
#define	FLAGSET_FLAGS4		4
#define	FLAGSET_FLAGS5		5
#define	FLAGSET_FLAGSST		6

// Which actor sound is being updated?
#define	ACTORSOUND_SEESOUND			1
#define	ACTORSOUND_ATTACKSOUND		2
#define	ACTORSOUND_PAINSOUND		3
#define	ACTORSOUND_DEATHSOUND		4
#define	ACTORSOUND_ACTIVESOUND		5

// Which userinfo categories are being updated?
#define	USERINFO_NAME				1
#define	USERINFO_GENDER				2
#define	USERINFO_COLOR				4
#define	USERINFO_AIMDISTANCE		8
#define	USERINFO_SKIN				16
#define	USERINFO_RAILCOLOR			32
#define	USERINFO_HANDICAP			64
#define	USERINFO_PLAYERCLASS		128

#define	USERINFO_ALL				( USERINFO_NAME | USERINFO_GENDER | USERINFO_COLOR | \
									USERINFO_AIMDISTANCE | USERINFO_SKIN | USERINFO_RAILCOLOR | \
									USERINFO_HANDICAP | USERINFO_PLAYERCLASS )

// [BB]: Some optimization. For some actors that are sent in bunches, to reduce the size,
// just send some key letter that identifies the actor, instead of the full name.
#define NUMBER_OF_ACTOR_NAME_KEY_LETTERS	3
#define NUMBER_OF_WEAPON_NAME_KEY_LETTERS	10

//*****************************************************************************
typedef enum
{
	POLYSOUND_STOPSEQUENCE,
	POLYSOUND_SEQ_DOOR,

	NUM_NETWORK_POLYOBJSOUNDS,

} NETWORK_POLYOBJSOUND_e;

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

	// [RC] Too many connections from the IP.
	NETWORK_ERRORCODE_TOOMANYCONNECTIONSFROMIP,

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
enum
{
	// The server has properly received the client's challenge, and is telling
	// the client to authenticate his map.
	SVCC_AUTHENTICATE,

	// The server received the client's checksum, and it's valid. Now the server
	// is telling the client to load the map.
	SVCC_MAPLOAD,

	// There was an error during the course of the client trying to connect.
	SVCC_ERROR,

	NUM_SERVERCONNECT_COMMANDS
};

//*****************************************************************************
// Note: If the number of enumerated messages goes beyond 255, commands will need 
// to be changed to a short. Hopefully that won't have to happen.
enum
{
	SVC_HEADER = NUM_SERVERCONNECT_COMMANDS,	// GENERAL PROTOCOL COMMANDS
	SVC_UNRELIABLEPACKET,
	SVC_PING,
	SVC_NOTHING,
	SVC_BEGINSNAPSHOT,
	SVC_ENDSNAPSHOT,
	SVC_SPAWNPLAYER,					// PLAYER COMMANDS
	SVC_SPAWNMORPHPLAYER,
	SVC_MOVEPLAYER,
	SVC_DAMAGEPLAYER,
	SVC_KILLPLAYER,
	SVC_SETPLAYERHEALTH,
	SVC_SETPLAYERARMOR,
	SVC_SETPLAYERSTATE,
	SVC_SETPLAYERUSERINFO,
	SVC_SETPLAYERFRAGS,
	SVC_SETPLAYERPOINTS,
	SVC_SETPLAYERWINS,
	SVC_SETPLAYERKILLCOUNT,
	SVC_SETPLAYERCHATSTATUS,
	SVC_SETPLAYERCONSOLESTATUS,
	SVC_SETPLAYERLAGGINGSTATUS,
	SVC_SETPLAYERREADYTOGOONSTATUS,
	SVC_SETPLAYERTEAM,
	SVC_SETPLAYERCAMERA,
	SVC_SETPLAYERPOISONCOUNT,
	SVC_SETPLAYERAMMOCAPACITY,
	SVC_SETPLAYERCHEATS,
	SVC_SETPLAYERPENDINGWEAPON,
	SVC_SETPLAYERPIECES,
	SVC_SETPLAYERPSPRITE,
	SVC_SETPLAYERBLEND,
	SVC_SETPLAYERMAXHEALTH,
	SVC_UPDATEPLAYERPING,
	SVC_UPDATEPLAYEREXTRADATA,
	SVC_UPDATEPLAYERTIME,
	SVC_MOVELOCALPLAYER,
	SVC_DISCONNECTPLAYER,
	SVC_SETCONSOLEPLAYER,
	SVC_CONSOLEPLAYERKICKED,
	SVC_GIVEPLAYERMEDAL,
	SVC_RESETALLPLAYERSFRAGCOUNT,
	SVC_PLAYERISSPECTATOR,
	SVC_PLAYERSAY,
	SVC_PLAYERTAUNT,
	SVC_PLAYERRESPAWNINVULNERABILITY,
	SVC_PLAYERUSEINVENTORY,
	SVC_PLAYERDROPINVENTORY,
	SVC_SPAWNTHING,						// THING COMMANDS
	SVC_SPAWNTHINGNONETID,
	SVC_SPAWNTHINGEXACT,
	SVC_SPAWNTHINGEXACTNONETID,
	SVC_MOVETHING,
	SVC_MOVETHINGEXACT,
	SVC_DAMAGETHING,
	SVC_KILLTHING,
	SVC_SETTHINGSTATE,
	SVC_SETTHINGTARGET,
	SVC_DESTROYTHING,
	SVC_SETTHINGANGLE,
	SVC_SETTHINGANGLEEXACT,
	SVC_SETTHINGMOVEDIR,
	SVC_SETTHINGWATERLEVEL,
	SVC_SETTHINGFLAGS,
	SVC_SETTHINGARGUMENTS,
	SVC_SETTHINGTRANSLATION,
	SVC_SETTHINGPROPERTY,
	SVC_SETTHINGSOUND,
	SVC_SETTHINGSPAWNPOINT,
	SVC_SETTHINGSPECIAL1,
	SVC_SETTHINGSPECIAL2,
	SVC_SETTHINGTICS,
	SVC_SETTHINGTID,
	SVC_SETTHINGGRAVITY,
	SVC_SETTHINGFRAME,
	SVC_SETTHINGFRAMENF,
	SVC_SETWEAPONAMMOGIVE,
	SVC_THINGISCORPSE,
	SVC_HIDETHING,
	SVC_TELEPORTTHING,
	SVC_THINGACTIVATE,
	SVC_THINGDEACTIVATE,
	SVC_RESPAWNDOOMTHING,
	SVC_RESPAWNRAVENTHING,
	SVC_SPAWNBLOOD,
	SVC_SPAWNPUFF,
	SVC_PRINT,							// PRINT COMMANDS
	SVC_PRINTMID,
	SVC_PRINTMOTD,
	SVC_PRINTHUDMESSAGE,
	SVC_PRINTHUDMESSAGEFADEOUT,
	SVC_PRINTHUDMESSAGEFADEINOUT,
	SVC_PRINTHUDMESSAGETYPEONFADEOUT,
	SVC_SETGAMEMODE,					// GAME COMMANDS
	SVC_SETGAMESKILL,
	SVC_SETGAMEDMFLAGS,
	SVC_SETGAMEMODELIMITS,
	SVC_SETGAMEENDLEVELDELAY,
	SVC_SETGAMEMODESTATE,
	SVC_SETDUELNUMDUELS,
	SVC_SETLMSSPECTATORSETTINGS,
	SVC_SETLMSALLOWEDWEAPONS,
	SVC_SETINVASIONNUMMONSTERSLEFT,
	SVC_SETINVASIONWAVE,
	SVC_SETSIMPLECTFSTMODE,
	SVC_DOPOSSESSIONARTIFACTPICKEDUP,
	SVC_DOPOSSESSIONARTIFACTDROPPED,
	SVC_DOGAMEMODEFIGHT,
	SVC_DOGAMEMODECOUNTDOWN,
	SVC_DOGAMEMODEWINSEQUENCE,
	SVC_SETDOMINATIONSTATE,
	SVC_SETDOMINATIONPOINTOWNER,
	SVC_SETTEAMFRAGS,					// TEAM COMMANDS
	SVC_SETTEAMSCORE,
	SVC_SETTEAMWINS,
	SVC_SETTEAMRETURNTICKS,
	SVC_TEAMFLAGRETURNED,
	SVC_TEAMFLAGDROPPED,
	SVC_SPAWNMISSILE,					// MISSILE COMMANDS
	SVC_SPAWNMISSILEEXACT,
	SVC_MISSILEEXPLODE,
	SVC_WEAPONSOUND,					// WEAPON COMMANDS
	SVC_WEAPONCHANGE,
	SVC_WEAPONRAILGUN,
	SVC_SETSECTORFLOORPLANE,			// SECTOR COMMANDS
	SVC_SETSECTORCEILINGPLANE,
	SVC_SETSECTORFLOORPLANESLOPE,
	SVC_SETSECTORCEILINGPLANESLOPE,
	SVC_SETSECTORLIGHTLEVEL,
	SVC_SETSECTORCOLOR,
	SVC_SETSECTORCOLORBYTAG,
	SVC_SETSECTORFADE,
	SVC_SETSECTORFADEBYTAG,
	SVC_SETSECTORFLAT,
	SVC_SETSECTORPANNING,
	SVC_SETSECTORROTATION,
	SVC_SETSECTORSCALE,
	SVC_SETSECTORSPECIAL,
	SVC_SETSECTORFRICTION,
	SVC_SETSECTORANGLEYOFFSET,
	SVC_SETSECTORGRAVITY,
	SVC_SETSECTORREFLECTION,
	SVC_STOPSECTORLIGHTEFFECT,
	SVC_DESTROYALLSECTORMOVERS,
	SVC_DOSECTORLIGHTFIREFLICKER,		// SECTOR LIGHT COMMANDS
	SVC_DOSECTORLIGHTFLICKER,
	SVC_DOSECTORLIGHTLIGHTFLASH,
	SVC_DOSECTORLIGHTSTROBE,
	SVC_DOSECTORLIGHTGLOW,
	SVC_DOSECTORLIGHTGLOW2,
	SVC_DOSECTORLIGHTPHASED,
	SVC_SETLINEALPHA,					// LINE COMMANDS
	SVC_SETLINETEXTURE,
	SVC_SETLINETEXTUREBYID,
	SVC_SETSOMELINEFLAGS,
	SVC_SETSIDEFLAGS,					// SIDE COMMANDS
	SVC_ACSSCRIPTEXECUTE,				// ACS COMMANDS
	SVC_SOUND,							// SOUND COMMANDS
	SVC_SOUNDACTOR,
	SVC_SOUNDACTORIFNOTPLAYING,
	SVC_SOUNDPOINT,
	SVC_STARTSECTORSEQUENCE,			// SECTOR SEQUENCE COMMANDS
	SVC_STOPSECTORSEQUENCE,
	SVC_CALLVOTE,						// VOTING COMMANDS
	SVC_PLAYERVOTE,
	SVC_VOTEENDED,
	SVC_MAPLOAD,						// MAP COMMANDS
	SVC_MAPNEW,
	SVC_MAPEXIT,
	SVC_MAPAUTHENTICATE,
	SVC_SETMAPTIME,
	SVC_SETMAPNUMKILLEDMONSTERS,
	SVC_SETMAPNUMFOUNDITEMS,
	SVC_SETMAPNUMFOUNDSECRETS,
	SVC_SETMAPNUMTOTALMONSTERS,
	SVC_SETMAPNUMTOTALITEMS,
	SVC_SETMAPMUSIC,
	SVC_SETMAPSKY,
	SVC_GIVEINVENTORY,					// INVENTORY COMMANDS
	SVC_TAKEINVENTORY,
	SVC_GIVEPOWERUP,
	SVC_DOINVENTORYPICKUP,
	SVC_DESTROYALLINVENTORY,
	SVC_DODOOR,							// DOOR COMMANDS
	SVC_DESTROYDOOR,
	SVC_CHANGEDOORDIRECTION,
	SVC_DOFLOOR,						// FLOOR COMMANDS
	SVC_DESTROYFLOOR,
	SVC_CHANGEFLOORDIRECTION,
	SVC_CHANGEFLOORTYPE,
	SVC_CHANGEFLOORDESTDIST,
	SVC_STARTFLOORSOUND,
	SVC_DOCEILING,						// CEILING COMMANDS
	SVC_DESTROYCEILING,
	SVC_CHANGECEILINGDIRECTION,
	SVC_CHANGECEILINGSPEED,
	SVC_PLAYCEILINGSOUND,
	SVC_DOPLAT,							// PLAT COMMANDS
	SVC_DESTROYPLAT,
	SVC_CHANGEPLATSTATUS,
	SVC_PLAYPLATSOUND,
	SVC_DOELEVATOR,						// ELEVATOR COMMANDS
	SVC_DESTROYELEVATOR,
	SVC_STARTELEVATORSOUND,
	SVC_DOPILLAR,						// PILLAR COMMANDS
	SVC_DESTROYPILLAR,
	SVC_DOWAGGLE,						// WAGGLE COMMANDS
	SVC_DESTROYWAGGLE,
	SVC_UPDATEWAGGLE,
	SVC_DOROTATEPOLY,					// ROTATEPOLY COMMANDS
	SVC_DESTROYROTATEPOLY,
	SVC_DOMOVEPOLY,						// MOVEPOLY COMMANDS
	SVC_DESTROYMOVEPOLY,
	SVC_DOPOLYDOOR,						// POLYDOOR COMMANDS
	SVC_DESTROYPOLYDOOR,
	SVC_SETPOLYDOORSPEEDPOSITION,
	SVC_PLAYPOLYOBJSOUND,				// GENERIC POLYOBJECT COMMANDS
	SVC_SETPOLYOBJPOSITION,
	SVC_SETPOLYOBJROTATION,
	SVC_EARTHQUAKE,						// MISC. COMMANDS
	SVC_SETQUEUEPOSITION,
	SVC_DOSCROLLER,
	SVC_SETSCROLLER,
	SVC_SETWALLSCROLLER,
	SVC_DOFLASHFADER,
	SVC_GENERICCHEAT,
	SVC_SETCAMERATOTEXTURE,
	SVC_CREATETRANSLATION,
	SVC_IGNOREPLAYER,
	SVC_SPAWNBLOODSPLATTER,
	SVC_SPAWNBLOODSPLATTER2,
	SVC_CREATETRANSLATION2,
	SVC_REPLACETEXTURES,

	NUM_SERVER_COMMANDS
};

//*****************************************************************************
enum
{
	// Client is telling the server he wishes to connect.
	CLCC_ATTEMPTCONNECTION,

	// Client is attempting to authenticate the map.
	CLCC_ATTEMPTAUTHENTICATION,

	// Client has loaded the map, and is requesting the snapshot.
	CLCC_REQUESTSNAPSHOT,

	NUM_CLIENTCONNECT_COMMANDS
};

//*****************************************************************************
enum
{
	CLC_USERINFO = NUM_CLIENTCONNECT_COMMANDS,
	CLC_QUIT,
	CLC_STARTCHAT,
	CLC_ENDCHAT,
	CLC_SAY,
	CLC_CLIENTMOVE,
	CLC_MISSINGPACKET,
	CLC_PONG,
	CLC_WEAPONSELECT,
	CLC_TAUNT,
	CLC_SPECTATE,
	CLC_REQUESTJOIN,
	CLC_REQUESTRCON,
	CLC_RCONCOMMAND,
	CLC_SUICIDE,
	CLC_CHANGETEAM,
	CLC_SPECTATEINFO,
	CLC_GENERICCHEAT,
	CLC_GIVECHEAT,
	CLC_SUMMONCHEAT,
	CLC_READYTOGOON,
	CLC_CHANGEDISPLAYPLAYER,
	CLC_AUTHENTICATELEVEL,
	CLC_CALLVOTE,
	CLC_VOTEYES,
	CLC_VOTENO,
	CLC_INVENTORYUSEALL,
	CLC_INVENTORYUSE,
	CLC_INVENTORYDROP,
	CLC_SUMMONFRIENDCHEAT,
	CLC_SUMMONFOECHEAT,
	CLC_ENTERCONSOLE,
	CLC_EXITCONSOLE,
	CLC_IGNORE,
	CLC_PUKE,
	CLC_MORPHEX,

	NUM_CLIENT_COMMANDS

};

//*****************************************************************************
//	VARIABLES

extern FString g_lumpsAuthenticationChecksum;
extern FString g_SkulltagDataFileMD5Sum;
extern FString g_SkulltagDataFileName;

//*****************************************************************************
//	PROTOTYPES

void			NETWORK_Construct( USHORT usPort, bool bAllocateLANSocket );
void			NETWORK_Destruct( void );

int				NETWORK_GetPackets( void );
int				NETWORK_GetLANPackets( void );
NETADDRESS_s	NETWORK_GetFromAddress( void );
void			NETWORK_LaunchPacket( NETBUFFER_s *pBuffer, NETADDRESS_s Address );
const char		*NETWORK_AddressToString( NETADDRESS_s Address );
const char		*NETWORK_AddressToStringIgnorePort( NETADDRESS_s Address );
void			NETWORK_SetAddressPort( NETADDRESS_s &Address, USHORT usPort );
NETADDRESS_s	NETWORK_GetLocalAddress( void );
NETADDRESS_s	NETWORK_GetCachedLocalAddress( void );
NETBUFFER_s		*NETWORK_GetNetworkMessageBuffer( void );
ULONG			NETWORK_ntohs( ULONG ul );
USHORT			NETWORK_GetLocalPort( void );

std::list<FString>	*NETWORK_GetPWADList( void ); // [RC]
const char		*NETWORK_GetIWAD( void );
void			NETWORK_AddLumpForAuthentication( const LONG LumpNumber );
void			NETWORK_GenerateMapLumpMD5Hash( MapData *Map, const LONG LumpNumber, FString &MD5Hash );
void			NETWORK_GenerateLumpMD5Hash( const int LumpNum, FString &MD5Hash );

const char		*NETWORK_GetClassNameFromIdentification( USHORT usActorNetworkIndex );
const PClass	*NETWORK_GetClassFromIdentification( USHORT usActorNetworkIndex );

// [BB] Sound attenuation is a float, but we only want to sent a byte for the 
// attenuation to instructs clients to play a sound. The enum is used for the
// conversion of the neccessary attenuation values.
enum
{
	ATTN_INT_NONE,
	ATTN_INT_NORM,
	ATTN_INT_IDLE,
	ATTN_INT_STATIC,
};

int				NETWORK_AttenuationFloatToInt ( const float fAttenuation );
float			NETWORK_AttenuationIntToFloat ( const int iAttenuation );


// Access functions.
LONG			NETWORK_GetState( void );
void			NETWORK_SetState( LONG lState );

void			I_DoSelect( void );

// DEBUG FUNCTION!
#ifdef	_DEBUG
void	NETWORK_FillBufferWithShit( NETBUFFER_s *pBuffer, ULONG ulSize );
#endif

/**
 * \author BB
 */
template <typename T>
LONG NETWORK_GetFirstFreeID ( void )
{
	T	*pT;

	std::vector<bool> idUsed ( 8192 );

	TThinkerIterator<T>		Iterator;

	while (( pT = Iterator.Next( )))
	{
		if ( (pT->GetID( ) >= 0) )
			idUsed[pT->GetID( )] = true;
	}

	for ( unsigned int i = 0; i < idUsed.size(); i++ )
	{
		if ( idUsed[i] == false )
			return i;
	}

	Printf( "NETWORK_GetFirstFreeID: ID limit reached (>=8192)\n" );
	return ( -1 );
}

#endif	// __NETWORK_H__
