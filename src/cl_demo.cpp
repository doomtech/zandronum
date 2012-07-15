//-----------------------------------------------------------------------------
//
// Skulltag Source
// Copyright (C) 2007 Brad Carney
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
// Date created:  7/2/07
//
//
// Filename: cl_demo.cpp
//
// Description: 
//
//-----------------------------------------------------------------------------

#include "c_console.h"
#include "c_dispatch.h"
#include "cl_demo.h"
#include "cl_main.h"
#include "cmdlib.h"
#include "d_event.h"
#include "d_netinf.h"
#include "d_protocol.h"
#include "doomstat.h"
#include "doomtype.h"
#include "i_system.h"
#include "m_misc.h"
#include "m_random.h"
#include "network.h"
#include "networkshared.h"
#include "p_local.h"
#include "p_tick.h"
#include "r_draw.h"
#include "r_state.h"
#include "sbar.h"
#include "version.h"
#include "templates.h"
#include "r_translate.h"

//*****************************************************************************
//	PROTOTYPES

static	void				clientdemo_CheckDemoBuffer( ULONG ulSize );

//*****************************************************************************
//	VARIABLES

// Are we recording a demo?
static	bool				g_bDemoRecording;

// Are we playing a demo?
static	bool				g_bDemoPlaying;
static	bool				g_bDemoPlayingHonest;

// [BB] Do we want to skip to the next map in the demo we are playing at the moment?
static	bool				g_bSkipToNextMap = false;

// [BB] How many tics are we still supposed to skip in the demo we are playing at the moment?
static	ULONG				g_ulTicsToSkip = 0;

// Buffer for our demo.
static	BYTE				*g_pbDemoBuffer;

// Our byte stream that points to where we are in our demo.
static	BYTESTREAM_s		g_ByteStream;

// Name of our demo.
static	char				g_szDemoName[256];

// Length of the demo.
static	LONG				g_lDemoLength;

// This is the gametic we started playing the demo on.
static	LONG				g_lGameticOffset;

// Maximum length our current demo can be.
static	LONG				g_lMaxDemoLength;

// [BB] Special player that is used to control the camera when playing demos in free spectate mode.
static	player_t			g_demoCameraPlayer;

//*****************************************************************************
//	FUNCTIONS

void CLIENTDEMO_BeginRecording( const char *pszDemoName )
{
	if ( pszDemoName == NULL )
		return;

	// First, setup the demo name.
	strcpy( g_szDemoName, pszDemoName );
	FixPathSeperator( g_szDemoName );
	DefaultExtension( g_szDemoName, ".cld" );

	// Allocate 128KB of memory for the demo buffer.
	g_bDemoRecording = true;
	g_lMaxDemoLength = 0x20000;
	g_pbDemoBuffer = (BYTE *)M_Malloc( g_lMaxDemoLength );
	g_ByteStream.pbStream = g_pbDemoBuffer;
	g_ByteStream.pbStreamEnd = g_pbDemoBuffer + g_lMaxDemoLength;

	// Write our header.
	NETWORK_WriteByte( &g_ByteStream, CLD_DEMOSTART );
	NETWORK_WriteLong( &g_ByteStream, 12345678 );

	// Write the length of the demo. Of course, we can't complete this quite yet!
	NETWORK_WriteByte( &g_ByteStream, CLD_DEMOLENGTH );
	g_ByteStream.pbStream += 4;

	// Write version information helpful for this demo.
	NETWORK_WriteByte( &g_ByteStream, CLD_DEMOVERSION );
	NETWORK_WriteShort( &g_ByteStream, DEMOGAMEVERSION );
	NETWORK_WriteString( &g_ByteStream, DOTVERSIONSTR_REV );
	NETWORK_WriteLong( &g_ByteStream, rngseed );
/*
	// Write cvars chunk.
	StartChunk( CLD_CVARS, &g_pbDemoBuffer );
	C_WriteCVars( &g_pbDemoBuffer, CVAR_SERVERINFO|CVAR_DEMOSAVE );
	FinishChunk( &g_pbDemoBuffer );
*/
	// Write the console player's userinfo.
	CLIENTDEMO_WriteUserInfo( );

	// Indicate that we're done with header information, and are ready
	// to move onto the body of the demo.
	NETWORK_WriteByte( &g_ByteStream, CLD_BODYSTART );

	CLIENT_SetServerLagging( false );
}

//*****************************************************************************
//
bool CLIENTDEMO_ProcessDemoHeader( void )
{
	bool	bBodyStart;
	LONG	lDemoVersion;
	LONG	lCommand;

	if (( NETWORK_ReadByte( &g_ByteStream ) != CLD_DEMOSTART ) ||
		( NETWORK_ReadLong( &g_ByteStream ) != 12345678 ))
	{
		I_Error( "CLIENTDEMO_ProcessDemoHeader: Expected CLD_DEMOSTART.\n" );
		return ( false );
	}

	if ( NETWORK_ReadByte( &g_ByteStream ) != CLD_DEMOLENGTH )
	{
		I_Error( "CLIENTDEMO_ProcessDemoHeader: Expected CLD_DEMOLENGTH.\n" );
		return ( false );
	}

	g_lDemoLength = NETWORK_ReadLong( &g_ByteStream );
	g_ByteStream.pbStreamEnd = g_pbDemoBuffer + g_lDemoLength + ( g_lDemoLength & 1 );

	// Continue to read header commands until we reach the body of the demo.
	bBodyStart = false;
	while ( bBodyStart == false )
	{  
		lCommand = NETWORK_ReadByte( &g_ByteStream );

		// End of message.
		if ( lCommand == -1 )
			break;

		switch ( lCommand )
		{
		case CLD_DEMOVERSION:

			// Read in the DEMOGAMEVERSION the demo was recorded with.
			lDemoVersion = NETWORK_ReadShort( &g_ByteStream );
			if ( lDemoVersion < MINDEMOVERSION )
				I_Error( "Demo requires an older version of " GAMENAME "!\n" );

			// Read in the DOTVERSIONSTR the demo was recorded with.
			Printf( "Version %s demo\n", NETWORK_ReadString( &g_ByteStream ));

			// Read in the random number generator seed.
			rngseed = NETWORK_ReadLong( &g_ByteStream );
			FRandom::StaticClearRandom( );
			break;
/*
		case CLD_CVARS:

			C_ReadCVars( &g_pbDemoBuffer );
			break;
*/
		case CLD_USERINFO:

			CLIENTDEMO_ReadUserInfo( );
			break;
		case CLD_BODYSTART:

			bBodyStart = true;
			break;
		}
	}

	return ( bBodyStart );
}

//*****************************************************************************
//
void CLIENTDEMO_WriteUserInfo( void )
{
	// First, make sure we have enough space to write this command. If not, add
	// more space.
	clientdemo_CheckDemoBuffer( 16 +
		(ULONG)strlen( players[consoleplayer].userinfo.netname ) +
		(ULONG)strlen( skins[players[consoleplayer].userinfo.skin].name ) +
		(ULONG)strlen( PlayerClasses[players[consoleplayer].CurrentPlayerClass].Type->Meta.GetMetaString( APMETA_DisplayName )));

	// Write the header.
	NETWORK_WriteByte( &g_ByteStream, CLD_USERINFO );

	// Write the player's userinfo.
	NETWORK_WriteString( &g_ByteStream, players[consoleplayer].userinfo.netname );
	NETWORK_WriteByte( &g_ByteStream, players[consoleplayer].userinfo.gender );
	NETWORK_WriteLong( &g_ByteStream, players[consoleplayer].userinfo.color );
	NETWORK_WriteLong( &g_ByteStream, players[consoleplayer].userinfo.aimdist );
	NETWORK_WriteString( &g_ByteStream, skins[players[consoleplayer].userinfo.skin].name );
	NETWORK_WriteLong( &g_ByteStream, players[consoleplayer].userinfo.lRailgunTrailColor );
	NETWORK_WriteByte( &g_ByteStream, players[consoleplayer].userinfo.lHandicap );
	NETWORK_WriteByte( &g_ByteStream, players[consoleplayer].userinfo.bUnlagged );
	NETWORK_WriteByte( &g_ByteStream, players[consoleplayer].userinfo.bRespawnonfire );
	NETWORK_WriteByte( &g_ByteStream, players[consoleplayer].userinfo.ulTicsPerUpdate );
	NETWORK_WriteString( &g_ByteStream, PlayerClasses[players[consoleplayer].CurrentPlayerClass].Type->Meta.GetMetaString( APMETA_DisplayName ));
}

//*****************************************************************************
//
void CLIENTDEMO_ReadUserInfo( void )
{
	sprintf( players[consoleplayer].userinfo.netname, "%s", NETWORK_ReadString( &g_ByteStream ));
	// [BB] Make sure that the gender is valid.
	players[consoleplayer].userinfo.gender = clamp ( NETWORK_ReadByte( &g_ByteStream ), 0, 2 );
	players[consoleplayer].userinfo.color = NETWORK_ReadLong( &g_ByteStream );
	players[consoleplayer].userinfo.aimdist = NETWORK_ReadLong( &g_ByteStream );
	players[consoleplayer].userinfo.skin = R_FindSkin( NETWORK_ReadString( &g_ByteStream ), players[consoleplayer].CurrentPlayerClass );
	players[consoleplayer].userinfo.lRailgunTrailColor = NETWORK_ReadLong( &g_ByteStream );
	players[consoleplayer].userinfo.lHandicap = NETWORK_ReadByte( &g_ByteStream );
	players[consoleplayer].userinfo.bUnlagged = NETWORK_ReadByte( &g_ByteStream );
	players[consoleplayer].userinfo.bRespawnonfire = NETWORK_ReadByte( &g_ByteStream );
	players[consoleplayer].userinfo.ulTicsPerUpdate = NETWORK_ReadByte( &g_ByteStream );
	players[consoleplayer].userinfo.PlayerClass = D_PlayerClassToInt( NETWORK_ReadString( &g_ByteStream ));

	R_BuildPlayerTranslation( consoleplayer );
	if ( StatusBar )
		StatusBar->AttachToPlayer( &players[consoleplayer] );

	// Apply the skin properties to the player's body.
	if ( players[consoleplayer].mo != NULL )
	{
		if (players[consoleplayer].cls != NULL &&
			players[consoleplayer].mo->state->sprite ==
			GetDefaultByType (players[consoleplayer].cls)->SpawnState->sprite)
		{ // Only change the sprite if the player is using a standard one
			players[consoleplayer].mo->sprite = skins[players[consoleplayer].userinfo.skin].sprite;
			players[consoleplayer].mo->scaleX = skins[players[consoleplayer].userinfo.skin].ScaleX;
			players[consoleplayer].mo->scaleY = skins[players[consoleplayer].userinfo.skin].ScaleY;
		}
	}
}

//*****************************************************************************
//
void CLIENTDEMO_WriteTiccmd( ticcmd_t *pCmd )
{
	// First, make sure we have enough space to write this command. If not, add
	// more space.
	clientdemo_CheckDemoBuffer( 14 );

	// Write the header.
	NETWORK_WriteByte( &g_ByteStream, CLD_TICCMD );

	// Write the contents of the ticcmd.
	NETWORK_WriteShort( &g_ByteStream, pCmd->ucmd.yaw );
	NETWORK_WriteShort( &g_ByteStream, pCmd->ucmd.roll );
	NETWORK_WriteShort( &g_ByteStream, pCmd->ucmd.pitch );
	NETWORK_WriteByte( &g_ByteStream, pCmd->ucmd.buttons );
	NETWORK_WriteShort( &g_ByteStream, pCmd->ucmd.upmove );
	NETWORK_WriteShort( &g_ByteStream, pCmd->ucmd.forwardmove );
	NETWORK_WriteShort( &g_ByteStream, pCmd->ucmd.sidemove );
}

//*****************************************************************************
//
void CLIENTDEMO_ReadTiccmd( ticcmd_t *pCmd )
{
	// Read the contents of ticcmd.
	pCmd->ucmd.yaw = NETWORK_ReadShort( &g_ByteStream );
	pCmd->ucmd.roll = NETWORK_ReadShort( &g_ByteStream );
	pCmd->ucmd.pitch = NETWORK_ReadShort( &g_ByteStream );
	pCmd->ucmd.buttons = NETWORK_ReadByte( &g_ByteStream );
	pCmd->ucmd.upmove = NETWORK_ReadShort( &g_ByteStream );
	pCmd->ucmd.forwardmove = NETWORK_ReadShort( &g_ByteStream );
	pCmd->ucmd.sidemove = NETWORK_ReadShort( &g_ByteStream );
}

//*****************************************************************************
//
void CLIENTDEMO_WritePacket( BYTESTREAM_s *pByteStream )
{
	// First, make sure we have enough space to write this command. If not, add
	// more space.
	clientdemo_CheckDemoBuffer( pByteStream->pbStreamEnd - pByteStream->pbStream );

	NETWORK_WriteBuffer( &g_ByteStream, pByteStream->pbStream, pByteStream->pbStreamEnd - pByteStream->pbStream );
}

//*****************************************************************************
//
void CLIENTDEMO_ReadPacket( void )
{
	LONG		lCommand;
	const char	*pszString;

	while ( 1 )
	{  
		lCommand = NETWORK_ReadByte( &g_ByteStream );

		// End of message.
		if ( lCommand == -1 )
		{
			// [BB] When we reach the end of the demo stream, we need to stop the demo.
			CLIENTDEMO_FinishPlaying( );
			break;
		}

		switch ( lCommand )
		{
		case CLD_USERINFO:

			CLIENTDEMO_ReadUserInfo( );
			break;
		case CLD_TICCMD:

			CLIENTDEMO_ReadTiccmd( &players[consoleplayer].cmd );

			// After we write our ticcmd, we're done for this tic.
			if ( CLIENTDEMO_IsSkipping() == false )
				return;
			// [BB] If we don't return here, we essentially skip a tic and have to adjust the tic offset.
			else
			{
				g_lGameticOffset--;
				// [BB] If we are supposed to skip over a certain amount of tics, record that we have skipped one now.
				if ( g_ulTicsToSkip > 0 )
				{
					// [BB] When skipping a tic, we still need to process the current ticcmd_t.
					P_Ticker ();
					--g_ulTicsToSkip;
				}
			}
			break;
		case CLD_INVUSE:

			{
				AInventory	*pInventory;

				pszString = NETWORK_ReadString( &g_ByteStream );

				if ( players[consoleplayer].mo )
				{
					pInventory = players[consoleplayer].mo->FindInventory( pszString );
					if ( pInventory )
						players[consoleplayer].mo->UseInventory( pInventory );
				}
			}
			break;
		case CLD_CENTERVIEW:

			if ( players[consoleplayer].mo )
				players[consoleplayer].mo->pitch = 0;
			break;
		case CLD_TAUNT:

			PLAYER_Taunt( &players[consoleplayer] );
			break;
		case CLD_DEMOEND:

			CLIENTDEMO_FinishPlaying( );
			return;
		default:

#ifdef _DEBUG
			// Option to print commands for debugging purposes.
			if ( cl_showcommands )
				CLIENT_PrintCommand( lCommand );
#endif

			CLIENT_ProcessCommand( lCommand, &g_ByteStream );
			break;
		}
	}
}

//*****************************************************************************
//
void CLIENTDEMO_FinishRecording( void )
{
	LONG			lDemoLength;
	BYTESTREAM_s	ByteStream;

	// Write our header.
	NETWORK_WriteByte( &g_ByteStream, CLD_DEMOEND );

	// Go back real quick and write the length of this demo.
	lDemoLength = g_ByteStream.pbStream - g_pbDemoBuffer;
	ByteStream.pbStream = g_pbDemoBuffer + 6;
	ByteStream.pbStreamEnd = g_ByteStream.pbStreamEnd;
	NETWORK_WriteLong( &ByteStream, lDemoLength );

	// Write the contents of the buffer to the file, and free the memory we
	// allocated for the demo.
	M_WriteFile( g_szDemoName, g_pbDemoBuffer, lDemoLength ); 
	M_Free( g_pbDemoBuffer );
	g_pbDemoBuffer = NULL;

	// We're no longer recording a demo.
	g_bDemoRecording = false;

	// All done!
	Printf( "Demo \"%s\" successfully recorded!\n", g_szDemoName ); 
}

//*****************************************************************************
//
void CLIENTDEMO_DoPlayDemo( const char *pszDemoName )
{
	LONG	lDemoLump;
	LONG	lDemoLength;
	FString demoName = pszDemoName;

	// First, check if the demo is in a lump.
	lDemoLump = Wads.CheckNumForName( demoName );
	if ( lDemoLump >= 0 )
	{
		lDemoLength = Wads.LumpLength( lDemoLump );

		// Read the data from the lump into our demo buffer.
		g_pbDemoBuffer = new BYTE[lDemoLength];
		Wads.ReadLump( lDemoLump, g_pbDemoBuffer );
	}
	else
	{
		FixPathSeperator( demoName );
		DefaultExtension( demoName, ".cld" );
		lDemoLength = M_ReadFile( demoName, &g_pbDemoBuffer );
	}

	g_ByteStream.pbStream = g_pbDemoBuffer;
	g_ByteStream.pbStreamEnd = g_pbDemoBuffer + lDemoLength;

	if ( CLIENTDEMO_ProcessDemoHeader( ))
	{
		C_HideConsole( );
		g_bDemoPlaying = true;
		g_bDemoPlayingHonest = true;
		CLIENTDEMO_SetSkippingToNextMap ( false );

		g_lGameticOffset = gametic;
	}
	else
	{
		gameaction = ga_nothing;
		g_bDemoPlaying = false;
	}
}

//*****************************************************************************
//
void CLIENTDEMO_FinishPlaying( void )
{
//	C_RestoreCVars ();		// [RH] Restore cvars demo might have changed

	// Free our demo buffer.
	delete[] ( g_pbDemoBuffer );
	g_pbDemoBuffer = NULL;

	// We're no longer playing a demo.
	g_bDemoPlaying = false;
	g_bDemoPlayingHonest = false;
	CLIENTDEMO_SetSkippingToNextMap ( false );
	g_ulTicsToSkip = 0;

	// Clear out the existing players.
	CLIENT_ClearAllPlayers();

	consoleplayer = 0;
//	playeringame[consoleplayer] = true;
	players[consoleplayer].camera = NULL;
	if ( StatusBar )
		StatusBar->AttachToPlayer( &players[0] );

	if ( gameaction == ga_nothing )
	{
		D_AdvanceDemo( );

		// Go back to the full console.
		gameaction = ga_fullconsole;
		gamestate = GS_FULLCONSOLE;
	}

	// View is no longer active.
	viewactive = false;

	Printf( "Demo ended.\n" );
}

//*****************************************************************************
//
LONG CLIENTDEMO_GetGameticOffset( void )
{
	if ( g_bDemoPlaying == false )
		return ( 0 );

	return ( g_lGameticOffset );
}

//*****************************************************************************
//
void CLIENTDEMO_WriteLocalCommand( LONG lCommand, const char *pszArg )
{
	if ( pszArg )
		clientdemo_CheckDemoBuffer( (ULONG)strlen( pszArg ) + 1 );
	else
		clientdemo_CheckDemoBuffer( 1 );

	switch ( lCommand )
	{
	case CLD_INVUSE:

		NETWORK_WriteByte( &g_ByteStream, CLD_INVUSE );
		NETWORK_WriteString( &g_ByteStream, pszArg );
		break;
	case CLD_CENTERVIEW:

		NETWORK_WriteByte( &g_ByteStream, CLD_CENTERVIEW );
		break;
	case CLD_TAUNT:

		NETWORK_WriteByte( &g_ByteStream, CLD_TAUNT );
		break;
	}
}

//*****************************************************************************
//*****************************************************************************
//
bool CLIENTDEMO_IsRecording( void )
{
	return ( g_bDemoRecording );
}

//*****************************************************************************
//
void CLIENTDEMO_SetRecording( bool bRecording )
{
	g_bDemoRecording = bRecording;
}

//*****************************************************************************
//
bool CLIENTDEMO_IsPlaying( void )
{
	return ( g_bDemoPlaying || g_bDemoPlayingHonest );
}

//*****************************************************************************
//
void CLIENTDEMO_SetPlaying( bool bPlaying )
{
	g_bDemoPlaying = bPlaying;
}

//*****************************************************************************
//
bool CLIENTDEMO_IsSkipping( void )
{
	return ( g_ulTicsToSkip > 0 ) || CLIENTDEMO_IsSkippingToNextMap();
}

//*****************************************************************************
//
bool CLIENTDEMO_IsSkippingToNextMap( void )
{
	return g_bSkipToNextMap;
}

//*****************************************************************************
//
void CLIENTDEMO_SetSkippingToNextMap( bool bSkipToNextMap )
{
	g_bSkipToNextMap = bSkipToNextMap;
}

//*****************************************************************************
//
bool CLIENTDEMO_IsInFreeSpectateMode( void )
{
	const AActor *pCamera = players[consoleplayer].camera;
	return ( pCamera && ( pCamera == g_demoCameraPlayer.mo ) );
}
//*****************************************************************************
//
void CLIENTDEMO_SetFreeSpectatorTiccmd( ticcmd_t *pCmd )
{
	memcpy( &(g_demoCameraPlayer.cmd), pCmd, sizeof( ticcmd_t ));
}

//*****************************************************************************
//
void CLIENTDEMO_FreeSpectatorPlayerThink( void )
{
	P_PlayerThink ( &g_demoCameraPlayer );
}

//*****************************************************************************
//
bool CLIENTDEMO_IsFreeSpectatorPlayer( player_t *pPlayer )
{
	return ( &g_demoCameraPlayer == pPlayer );
}

//*****************************************************************************
//
void CLIENTDEMO_ClearFreeSpectatorPlayer( void )
{
	if ( g_demoCameraPlayer.mo != NULL )
	{
		if ( players[consoleplayer].camera == g_demoCameraPlayer.mo )
			players[consoleplayer].camera = players[consoleplayer].mo;

		g_demoCameraPlayer.mo->Destroy();
		g_demoCameraPlayer.mo = NULL;
	}

	player_t *p = &g_demoCameraPlayer;
	// Reset player structure to its defaults
	p->~player_t();
	::new(p) player_t;
}

//*****************************************************************************
//*****************************************************************************
//
static void clientdemo_CheckDemoBuffer( ULONG ulSize )
{
	LONG	lPosition;

	// We may need to allocate more memory for our demo buffer.
	if (( g_ByteStream.pbStream + ulSize ) > g_ByteStream.pbStreamEnd )
	{
		// Give us another 128KB of memory.
		g_lMaxDemoLength += 0x20000;
		lPosition = g_ByteStream.pbStream - g_pbDemoBuffer;
		g_pbDemoBuffer = (BYTE *)M_Realloc( g_pbDemoBuffer, g_lMaxDemoLength );
		g_ByteStream.pbStream = g_pbDemoBuffer + lPosition;
		g_ByteStream.pbStreamEnd = g_pbDemoBuffer + g_lMaxDemoLength;
	}
}

//*****************************************************************************
//	CONSOLE COMMANDS

CCMD( demo_skiptonextmap )
{
	// [Spleen] This command shouldn't do anything if a demo isn't playing.
	if ( CLIENTDEMO_IsPlaying( ) == false )
		return;

	CLIENTDEMO_SetSkippingToNextMap ( true );
}

CCMD( demo_skiptics )
{
	// This command shouldn't do anything if a demo isn't playing.
	if ( CLIENTDEMO_IsPlaying( ) == false )
		return;

	if ( argv.argc() > 1 )
		g_ulTicsToSkip = atoi ( argv[1] );
}

CCMD( demo_spectatefreely )
{
	// [Spleen] This command shouldn't do anything if a demo isn't playing.
	if ( CLIENTDEMO_IsPlaying( ) == false )
		return;

	const AActor *pCamera = players[consoleplayer].camera;
	if ( pCamera != g_demoCameraPlayer.mo )
	{
		CLIENTDEMO_ClearFreeSpectatorPlayer();
		player_t *p = &g_demoCameraPlayer;
		p->bSpectating = true;
		p->cls = PlayerClasses[p->CurrentPlayerClass].Type;
		p->mo = static_cast<APlayerPawn *> (Spawn (p->cls, pCamera->x, pCamera->y, pCamera->z + pCamera->height , NO_REPLACE));
		p->mo->angle = pCamera->angle;
		p->mo->flags |= (MF_NOGRAVITY);
		p->mo->player = p;
		p->DesiredFOV = p->FOV = 90.f;
		p->crouchfactor = FRACUNIT;
		PLAYER_SetDefaultSpectatorValues ( p );
		players[consoleplayer].camera = g_demoCameraPlayer.mo;
		if ( StatusBar )
			StatusBar->AttachToPlayer ( p );
	}
}
