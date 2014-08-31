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
// Filename: cl_commands.cpp
//
// Description: Contains a set of functions that correspond to each message a client
// can send out. Each functions handles the send out of each message.
//
//-----------------------------------------------------------------------------

#include "cl_commands.h"
#include "cl_demo.h"
#include "cl_main.h"
#include "doomstat.h"
#include "d_event.h"
#include "gi.h"
#include "network.h"
#include "r_state.h"
#include "v_text.h" // [RC] To conform player names
#include "gamemode.h"
#include "lastmanstanding.h"
#include "deathmatch.h"
#include "chat.h"

//*****************************************************************************
//	VARIABLES

static	ULONG	g_ulLastChangeTeamTime = 0;
static	ULONG	g_ulLastSuicideTime = 0;
static	ULONG	g_ulLastJoinTime = 0;
static	ULONG	g_ulLastDropTime = 0;
static	ULONG	g_ulLastSVCheatMessageTime = 0;
static	bool g_bIgnoreWeaponSelect = false;
SDWORD g_sdwCheckCmd = 0;

//*****************************************************************************
//	FUNCTIONS

void CLIENT_ResetFloodTimers( void )
{
	g_ulLastChangeTeamTime = 0;
	g_ulLastSuicideTime = 0;
	g_ulLastJoinTime = 0;
	g_ulLastDropTime = 0;
	g_ulLastSVCheatMessageTime = 0;
}

//*****************************************************************************
//
void CLIENT_IgnoreWeaponSelect( bool bIgnore )
{
	g_bIgnoreWeaponSelect = bIgnore;
}

//*****************************************************************************
//
bool CLIENT_GetIgnoreWeaponSelect( void )
{
	return g_bIgnoreWeaponSelect;
}

//*****************************************************************************
//
bool CLIENT_AllowSVCheatMessage( void )
{
	if ( ( g_ulLastSVCheatMessageTime > 0 ) && ( (ULONG)gametic < ( g_ulLastSVCheatMessageTime + ( TICRATE * 1 ))))
		return false;
	else
	{
		g_ulLastSVCheatMessageTime = gametic;
		return true;
	}
}

//*****************************************************************************
//
void CLIENTCOMMANDS_UserInfo( ULONG ulFlags )
{
	// Temporarily disable userinfo for when the player setup menu updates our userinfo. Then
	// we can just send all our userinfo in one big bulk, instead of each time it updates
	// a userinfo property.
	if ( CLIENT_GetAllowSendingOfUserInfo( ) == false )
		return;

	// [BB] It's pointless to tell the server of the class, if only one class is available.
	if (( PlayerClasses.Size( ) == 1 ) && ( ulFlags & USERINFO_PLAYERCLASS ))
		ulFlags  &= ~USERINFO_PLAYERCLASS;

	// [BB] Nothing changed, nothing to send.
	if ( ulFlags == 0 )
		return;

	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_USERINFO );

	// Tell the server which items are being updated.
	NETWORK_WriteShort( &CLIENT_GetLocalBuffer( )->ByteStream, ulFlags );

	if ( ulFlags & USERINFO_NAME )
	{ // [RC] Clean the name before we use it
		V_CleanPlayerName(players[consoleplayer].userinfo.netname);
		NETWORK_WriteString( &CLIENT_GetLocalBuffer( )->ByteStream, players[consoleplayer].userinfo.netname );
	}
	if ( ulFlags & USERINFO_GENDER )
		NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, players[consoleplayer].userinfo.gender );
	if ( ulFlags & USERINFO_COLOR )
		NETWORK_WriteLong( &CLIENT_GetLocalBuffer( )->ByteStream, players[consoleplayer].userinfo.color );
	if ( ulFlags & USERINFO_AIMDISTANCE )
		NETWORK_WriteLong( &CLIENT_GetLocalBuffer( )->ByteStream, players[consoleplayer].userinfo.aimdist );
	if ( ulFlags & USERINFO_SKIN )
		NETWORK_WriteString( &CLIENT_GetLocalBuffer( )->ByteStream, skins[players[consoleplayer].userinfo.skin].name );
	if ( ulFlags & USERINFO_RAILCOLOR )
		NETWORK_WriteLong( &CLIENT_GetLocalBuffer( )->ByteStream, players[consoleplayer].userinfo.lRailgunTrailColor );
	if ( ulFlags & USERINFO_HANDICAP )
		NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, players[consoleplayer].userinfo.lHandicap );
	if ( ulFlags & USERINFO_UNLAGGED )
		NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, players[consoleplayer].userinfo.bUnlagged );
	if ( ulFlags & USERINFO_RESPAWNONFIRE )
		NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, players[consoleplayer].userinfo.bRespawnonfire );
	if ( ulFlags & USERINFO_TICSPERUPDATE )
		NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, players[consoleplayer].userinfo.ulTicsPerUpdate );
	if ( ulFlags & USERINFO_CONNECTIONTYPE )
		NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, players[consoleplayer].userinfo.ulConnectionType );
	if (( PlayerClasses.Size( ) > 1 ) && ( ulFlags & USERINFO_PLAYERCLASS ))
	{
		if ( players[consoleplayer].userinfo.PlayerClass == -1 )
			NETWORK_WriteString( &CLIENT_GetLocalBuffer( )->ByteStream, "random" );
		else
			NETWORK_WriteString( &CLIENT_GetLocalBuffer( )->ByteStream, PlayerClasses[players[consoleplayer].userinfo.PlayerClass].Type->Meta.GetMetaString( APMETA_DisplayName ));
	}
}

//*****************************************************************************
//
void CLIENTCOMMANDS_StartChat( void )
{
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_STARTCHAT );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_EndChat( void )
{
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_ENDCHAT );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_EnterConsole( void )
{
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_ENTERCONSOLE );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_ExitConsole( void )
{
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_EXITCONSOLE );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_Say( ULONG ulMode, const char *pszString )
{
	// [TP] Limit messages to certain length.
	FString chatstring ( pszString );

	if ( chatstring.Len() > MAX_CHATBUFFER_LENGTH )
		chatstring = chatstring.Left( MAX_CHATBUFFER_LENGTH );

	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_SAY );
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, ulMode );
	NETWORK_WriteString( &CLIENT_GetLocalBuffer( )->ByteStream, chatstring );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_Ignore( ULONG ulPlayer, bool bIgnore, LONG lTicks )
{
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_IGNORE );
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, ulPlayer );
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, bIgnore );
	NETWORK_WriteLong( &CLIENT_GetLocalBuffer( )->ByteStream, lTicks );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_ClientMove( void )
{
	ticcmd_t	*pCmd;
	ULONG		ulBits;

	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_CLIENTMOVE );
	NETWORK_WriteLong( &CLIENT_GetLocalBuffer( )->ByteStream, gametic );

	// Decide what additional information needs to be sent.
	ulBits = 0;
	pCmd = &players[consoleplayer].cmd;

	// [BB] If we think that we can't move, don't even try to tell the server that we
	// want to move.
	if ( players[consoleplayer].mo->reactiontime )
	{
		pCmd->ucmd.forwardmove = 0;
		pCmd->ucmd.sidemove = 0;
		pCmd->ucmd.buttons &= ~BT_JUMP;
	}

	if ( pCmd->ucmd.yaw )
		ulBits |= CLIENT_UPDATE_YAW;
	if ( pCmd->ucmd.pitch )
		ulBits |= CLIENT_UPDATE_PITCH;
	if ( pCmd->ucmd.roll )
		ulBits |= CLIENT_UPDATE_ROLL;
	if ( pCmd->ucmd.buttons )
	{
		ulBits |= CLIENT_UPDATE_BUTTONS;
		if ( compatflags2 & COMPATF2_CLIENTS_SEND_FULL_BUTTON_INFO )
			ulBits |= CLIENT_UPDATE_BUTTONS_LONG;
	}
	if ( pCmd->ucmd.forwardmove )
		ulBits |= CLIENT_UPDATE_FORWARDMOVE;
	if ( pCmd->ucmd.sidemove )
		ulBits |= CLIENT_UPDATE_SIDEMOVE;
	if ( pCmd->ucmd.upmove )
		ulBits |= CLIENT_UPDATE_UPMOVE;

	// Tell the server what information we'll be sending.
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, ulBits );

	// Send the necessary movement/steering information.
	if ( ulBits & CLIENT_UPDATE_YAW )
		NETWORK_WriteShort( &CLIENT_GetLocalBuffer( )->ByteStream, pCmd->ucmd.yaw );
	if ( ulBits & CLIENT_UPDATE_PITCH )
		NETWORK_WriteShort( &CLIENT_GetLocalBuffer( )->ByteStream, pCmd->ucmd.pitch );
	if ( ulBits & CLIENT_UPDATE_ROLL )
		NETWORK_WriteShort( &CLIENT_GetLocalBuffer( )->ByteStream, pCmd->ucmd.roll );
	if ( ulBits & CLIENT_UPDATE_BUTTONS )
	{
		if ( ulBits & CLIENT_UPDATE_BUTTONS_LONG )
			NETWORK_WriteLong( &CLIENT_GetLocalBuffer( )->ByteStream, pCmd->ucmd.buttons );
		else
			NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, pCmd->ucmd.buttons );
	}
	if ( ulBits & CLIENT_UPDATE_FORWARDMOVE )
		NETWORK_WriteShort( &CLIENT_GetLocalBuffer( )->ByteStream, pCmd->ucmd.forwardmove );
	if ( ulBits & CLIENT_UPDATE_SIDEMOVE )
		NETWORK_WriteShort( &CLIENT_GetLocalBuffer( )->ByteStream, pCmd->ucmd.sidemove );
	if ( ulBits & CLIENT_UPDATE_UPMOVE )
		NETWORK_WriteShort( &CLIENT_GetLocalBuffer( )->ByteStream, pCmd->ucmd.upmove );

	NETWORK_WriteLong( &CLIENT_GetLocalBuffer( )->ByteStream, players[consoleplayer].mo->angle );
	NETWORK_WriteLong( &CLIENT_GetLocalBuffer( )->ByteStream, players[consoleplayer].mo->pitch );
	// [BB] Send the checksum of our ticcmd we calculated when we originally generated the ticcmd from the user input.
	NETWORK_WriteLong( &CLIENT_GetLocalBuffer( )->ByteStream, g_sdwCheckCmd );

	// Attack button.
	if ( pCmd->ucmd.buttons & BT_ATTACK )
	{
		if ( players[consoleplayer].ReadyWeapon == NULL )
			NETWORK_WriteShort( &CLIENT_GetLocalBuffer( )->ByteStream, 0 );
		else
			NETWORK_WriteShort( &CLIENT_GetLocalBuffer( )->ByteStream, players[consoleplayer].ReadyWeapon->GetClass( )->getActorNetworkIndex() );
	}
}

//*****************************************************************************
//
void CLIENTCOMMANDS_MissingPacket( void )
{
}

//*****************************************************************************
//
void CLIENTCOMMANDS_Pong( ULONG ulTime )
{
	// [BB] CLIENTCOMMANDS_Pong is the only client command function that
	// immediately launches a network packet. This is something that
	// we obviously don't want to do when playing a demo.
	if ( CLIENTDEMO_IsPlaying( ) )
		return;

	// [BB] For an accurate ping measurement, the client has to answer
	// immediately instead of sending the answer together with the the
	// other commands tic-synced in CLIENT_EndTick().
	NETBUFFER_s	TempBuffer;
	NETWORK_InitBuffer( &TempBuffer, MAX_UDP_PACKET, BUFFERTYPE_WRITE );
	NETWORK_ClearBuffer( &TempBuffer );
	NETWORK_WriteByte( &TempBuffer.ByteStream, CLC_PONG );
	NETWORK_WriteLong( &TempBuffer.ByteStream, ulTime );
	NETWORK_LaunchPacket( &TempBuffer, NETWORK_GetFromAddress( ) );
	NETWORK_FreeBuffer( &TempBuffer );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_WeaponSelect( const PClass *pType )
{
	if ( ( pType == NULL ) || g_bIgnoreWeaponSelect )
		return;

	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_WEAPONSELECT );
	NETWORK_WriteShort( &CLIENT_GetLocalBuffer( )->ByteStream, pType->getActorNetworkIndex() );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_Taunt( void )
{
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_TAUNT );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_Spectate( void )
{
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_SPECTATE );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_RequestJoin( const char *pszJoinPassword )
{
	if (( g_ulLastJoinTime > 0 ) && ( (ULONG)gametic < ( g_ulLastJoinTime + ( TICRATE * 10 ))))
	{
		Printf( "You must wait at least 10 seconds before joining again.\n" );
		return;
	}

	g_ulLastJoinTime = gametic;
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_REQUESTJOIN );
	NETWORK_WriteString( &CLIENT_GetLocalBuffer( )->ByteStream, pszJoinPassword );

	// [BB/Spleen] Send the gametic so that the client doesn't think it's lagging.
	NETWORK_WriteLong( &CLIENT_GetLocalBuffer( )->ByteStream, gametic );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_RequestRCON( char *pszRCONPassword )
{
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_REQUESTRCON );
	NETWORK_WriteString( &CLIENT_GetLocalBuffer( )->ByteStream, pszRCONPassword );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_RCONCommand( char *pszCommand )
{
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_RCONCOMMAND );
	NETWORK_WriteString( &CLIENT_GetLocalBuffer( )->ByteStream, pszCommand );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_Suicide( void )
{
	if (( g_ulLastSuicideTime > 0 ) && ( (ULONG)gametic < ( g_ulLastSuicideTime + ( TICRATE * 10 ))))
	{
		Printf( "You must wait at least 10 seconds before suiciding again.\n" );
		return;
	}

	g_ulLastSuicideTime = gametic;
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_SUICIDE );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_ChangeTeam( const char *pszJoinPassword, LONG lDesiredTeam )
{
	if (!( ( lastmanstanding || teamlms ) && ( LASTMANSTANDING_GetState( ) == LMSS_COUNTDOWN ) ) && ( g_ulLastChangeTeamTime > 0 ) && ( (ULONG)gametic < ( g_ulLastChangeTeamTime + ( TICRATE * 10 ))))
	{
		Printf( "You must wait at least 10 seconds before changing teams again.\n" );
		return;
	}

	g_ulLastChangeTeamTime = gametic;
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_CHANGETEAM );
	NETWORK_WriteString( &CLIENT_GetLocalBuffer( )->ByteStream, pszJoinPassword );
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, lDesiredTeam );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_SpectateInfo( void )
{
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_SPECTATEINFO );
	NETWORK_WriteLong( &CLIENT_GetLocalBuffer( )->ByteStream, gametic );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_GenericCheat( LONG lCheat )
{
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_GENERICCHEAT );
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, lCheat );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_GiveCheat( char *pszItem, LONG lAmount )
{
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_GIVECHEAT );
	NETWORK_WriteString( &CLIENT_GetLocalBuffer( )->ByteStream, pszItem );
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, lAmount );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_SummonCheat( const char *pszItem, LONG lType, const bool bSetAngle, const SHORT sAngle )
{
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, lType );
	NETWORK_WriteString( &CLIENT_GetLocalBuffer( )->ByteStream, pszItem );
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, bSetAngle );
	if ( bSetAngle )
		NETWORK_WriteShort( &CLIENT_GetLocalBuffer( )->ByteStream, sAngle );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_ReadyToGoOn( void )
{
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_READYTOGOON );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_ChangeDisplayPlayer( LONG lDisplayPlayer )
{
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_CHANGEDISPLAYPLAYER );
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, lDisplayPlayer );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_AuthenticateLevel( void )
{
}

//*****************************************************************************
//
void CLIENTCOMMANDS_CallVote( LONG lVoteCommand, const char *pszArgument, const char *pszReason )
{
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_CALLVOTE );
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, lVoteCommand );
	NETWORK_WriteString( &CLIENT_GetLocalBuffer( )->ByteStream, pszArgument );
	NETWORK_WriteString( &CLIENT_GetLocalBuffer( )->ByteStream, pszReason );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_VoteYes( void )
{
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_VOTEYES );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_VoteNo( void )
{
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_VOTENO );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_RequestInventoryUseAll( void )
{
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_INVENTORYUSEALL );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_RequestInventoryUse( AInventory *item )
{
	if ( item == NULL )
		return;

	const USHORT usActorNetworkIndex = item->GetClass( )->getActorNetworkIndex();

	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_INVENTORYUSE );
	NETWORK_WriteShort( &CLIENT_GetLocalBuffer( )->ByteStream, usActorNetworkIndex );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_RequestInventoryDrop( AInventory *pItem )
{
	if ( !(GAMEMODE_GetFlags( GAMEMODE_GetCurrentMode( )) & GMF_COOPERATIVE) )
	{
		Printf( "Dropping is not allowed in non-cooperative game modes.\n" );
		return;
	}

	if (( g_ulLastDropTime > 0 ) && ( (ULONG)gametic < ( g_ulLastDropTime + ( TICRATE ))))
	{
		Printf( "You must wait at least one second before using drop again.\n" );
		return;
	}

	if ( pItem == NULL )
		return;

	const USHORT usActorNetworkIndex = pItem->GetClass( )->getActorNetworkIndex();

	g_ulLastDropTime = gametic;

	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_INVENTORYDROP );
	NETWORK_WriteShort( &CLIENT_GetLocalBuffer( )->ByteStream, usActorNetworkIndex );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_Puke ( LONG lScript, int args[3] )
{
	// [Dusk] Calculate argn from args.
	int argn = ( args[2] != 0 ) ? 3 :
	           ( args[1] != 0 ) ? 2 :
	           ( args[0] != 0 ) ? 1 : 0;

	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_PUKE );
	NETWORK_WriteShort( &CLIENT_GetLocalBuffer( )->ByteStream, (lScript < 0) ? -lScript : lScript );
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, argn );

	for ( ULONG ulIdx = 0; ulIdx < argn; ++ulIdx )
		NETWORK_WriteLong ( &CLIENT_GetLocalBuffer( )->ByteStream, args[ulIdx] );

	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, (lScript < 0) );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_MorphCheat ( const char *pszMorphClass )
{
	if ( pszMorphClass == NULL )
		return;

	if ( PClass::FindClass ( pszMorphClass ) == NULL )
	{
		Printf ( "Unknown class %s\n", pszMorphClass );
		return;
	}

	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_MORPHEX );
	NETWORK_WriteString( &CLIENT_GetLocalBuffer( )->ByteStream, pszMorphClass );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_FullUpdateReceived ( void )
{
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_FULLUPDATE );
}

//*****************************************************************************
// [Dusk]
void CLIENTCOMMANDS_Linetarget( AActor* mobj )
{
	if ( mobj == NULL || mobj->lNetID == -1 )
		return;

	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_LINETARGET );
	NETWORK_WriteShort( &CLIENT_GetLocalBuffer( )->ByteStream, mobj->lNetID );
}
