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
// Date created:  2/3/03
//
//
// Filename: cl_main.h
//
// Description: 
//
//-----------------------------------------------------------------------------

#ifndef __CL_MAIN_H__
#define __CL_MAIN_H__

#include "i_net.h"
#include "d_ticcmd.h"
#include "sv_main.h"

//*****************************************************************************
//	DEFINES

#define	CONNECTION_RESEND_TIME		( 3 * TICRATE )
#define	GAMESTATE_RESEND_TIME		( 3 * TICRATE )

// For prediction (?)
#define MAXSAVETICS					70

// Display "couldn't find thing" messages.
//#define	CLIENT_WARNING_MESSAGES

//*****************************************************************************
typedef enum 
{
	// Full screen console with no connection.
	CTS_DISCONNECTED,

	// Full screen console, trying to connect to a server.
	CTS_TRYCONNECT,

	// On server, waiting for data.
	CTS_CONNECTED,

	// Got response from server, now authenticating level.
	CTS_AUTHENTICATINGLEVEL,

	// Client is receiving snapshot of the level.
	CTS_RECEIVINGSNAPSHOT,

    // Snapshot is finished! Everything is done, fully in the level.
	CTS_ACTIVE,

} CONNECTIONSTATE_e;

//*****************************************************************************
//	STRUCTURES

typedef struct
{
	// This array of bytes is the storage for the packet data.
	BYTE	abData[MAX_UDP_PACKET * 256];

	// What is the current position in the array?
	LONG	lCurrentPosition;

	// This is the number of bytes in paData.
	LONG	lMaxSize;

} PACKETBUFFER_s;

//*****************************************************************************
//	PROTOTYPES

void	CLIENT_Construct( void );
void	CLIENT_Tick( void );

CONNECTIONSTATE_e	CLIENT_GetConnectionState( void );
void				CLIENT_SetConnectionState( CONNECTIONSTATE_e State );

NETBUFFER_s	*CLIENT_GetLocalBuffer( void );
void		CLIENT_SetLocalBuffer( NETBUFFER_s *pBuffer );

ULONG	CLIENT_GetLastServerTick( void );
void	CLIENT_SetLastServerTick( ULONG ulTick );

ULONG	CLIENT_GetLastConsolePlayerUpdateTick( void );
void	CLIENT_SetLastConsolePlayerUpdateTick( ULONG ulTick );

bool	CLIENT_GetServerLagging( void );
void	CLIENT_SetServerLagging( bool bLagging );

bool	CLIENT_GetClientLagging( void );
void	CLIENT_SetClientLagging( bool bLagging );

NETADDRESS_s	CLIENT_GetServerAddress( void );
void			CLIENT_SetServerAddress( NETADDRESS_s Address );

ULONG	CLIENT_GetBytesReceived( void );
void	CLIENT_AddBytesReceived( ULONG ulBytes );

void	CLIENT_SendConnectionSignal( void );
void	CLIENT_AttemptAuthentication( char *pszMapName );
void	CLIENT_AttemptConnection( void );
void	CLIENT_QuitNetworkGame( void );
void	CLIENT_QuitNetworkGame( char *pszError );
void	CLIENT_SendUserInfo( ULONG ulFlags );
void	CLIENT_SendCmd( void );
void	CLIENT_AuthenticateLevel( char *pszMapName );
bool	CLIENT_ReadPacketHeader( BYTE **pbStream );
void	CLIENT_ParsePacket( BYTE **pbStream, LONG lLength, bool bSequencedPacket );
void	CLIENT_PrintCommand( LONG lCommand );
void	CLIENT_ProcessCommand( LONG lCommand, BYTE **pbStream );
void	CLIENT_SpawnThing( char *pszName, fixed_t X, fixed_t Y, fixed_t Z, LONG lNetID );
void	CLIENT_SpawnMissile( char *pszName, fixed_t X, fixed_t Y, fixed_t Z, fixed_t MomX, fixed_t MomY, fixed_t MomZ, LONG lNetID, LONG lTargetNetID );
void	CLIENT_MoveThing( AActor *pActor, fixed_t X, fixed_t Y, fixed_t Z );
void	CLIENT_RestoreSpecialPosition( AActor *pActor );
void	CLIENT_RestoreSpecialDoomThing( AActor *pActor, bool bFog );
void	CLIENT_WaitForServer( void );
void	CLIENT_RemoveCorpses( void );
sector_t	*CLIENT_FindSectorByID( ULONG ulID );
bool	CLIENT_IsValidPlayer( ULONG ulPlayer );
void	CLIENT_AllowSendingOfUserInfo( bool bAllow );
void	CLIENT_ResetPlayerData( player_s *pPlayer );
LONG	CLIENT_AdjustDoorDirection( LONG lDirection );
LONG	CLIENT_AdjustFloorDirection( LONG lDirection );
LONG	CLIENT_AdjustCeilingDirection( LONG lDirection );
LONG	CLIENT_AdjustElevatorDirection( LONG lDirection );
void	CLIENT_CheckForMissingPackets( void );
bool	CLIENT_GetNextPacket( BYTE **pbStream );

void	CLIENT_PREDICT_PlayerPredict( void );
bool	CLIENT_PREDICT_IsPredicting( void );

//*****************************************************************************
//	EXTERNAL CONSOLE VARIABLES

EXTERN_CVAR( Bool, cl_predict_players )
EXTERN_CVAR( Int, cl_maxcorpses )
EXTERN_CVAR( Float, cl_motdtime )
EXTERN_CVAR( Bool, cl_taunts )
EXTERN_CVAR( Int, cl_showcommands )
EXTERN_CVAR( Int, cl_showspawnnames )
EXTERN_CVAR( Bool, cl_startasspectator )
EXTERN_CVAR( Bool, cl_dontrestorefrags )
EXTERN_CVAR( String, cl_password )
EXTERN_CVAR( String, cl_joinpassword )
EXTERN_CVAR( Bool, cl_hitscandecalhack )

// Not in cl_main.cpp, but this seems like a good enough place for it.
EXTERN_CVAR( Int, cl_skins )

#endif // __CL_MAIN__
