#ifndef __I_CLMAIN_H__
#define __I_CLMAIN_H__

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

sizebuf_t	*CLIENT_GetLocalBuffer( void );
void		CLIENT_SetLocalBuffer( sizebuf_t *pBuffer );

ULONG	CLIENT_GetLastServerTick( void );
void	CLIENT_SetLastServerTick( ULONG ulTick );

ULONG	CLIENT_GetLastConsolePlayerUpdateTick( void );
void	CLIENT_SetLastConsolePlayerUpdateTick( ULONG ulTick );

bool	CLIENT_GetServerLagging( void );
void	CLIENT_SetServerLagging( bool bLagging );

bool	CLIENT_GetClientLagging( void );
void	CLIENT_SetClientLagging( bool bLagging );

netadr_t	CLIENT_GetServerAddress( void );
void		CLIENT_SetServerAddress( netadr_t Address );

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
bool	CLIENT_ReadPacketHeader( void );
void	CLIENT_ParsePacket( bool bSequencedPacket );
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
bool	CLIENT_GetNextPacket( void );

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

// Not in cl_main.cpp, but this seems like a good enough place for it.
EXTERN_CVAR( Int, cl_skins )

#endif // __CL_MAIN__
