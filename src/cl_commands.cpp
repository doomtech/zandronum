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
#include "gi.h"
#include "network.h"
#include "r_state.h"
#include "v_text.h" // [RC] To conform player names

//*****************************************************************************
//	VARIABLES

static	ULONG	g_ulLastChangeTeamTime = 0;

//*****************************************************************************
//	FUNCTIONS

void CLIENTCOMMANDS_UserInfo( ULONG ulFlags )
{
	// Temporarily disable userinfo for when the player setup menu updates our userinfo. Then
	// we can just send all our userinfo in one big bulk, instead of each time it updates
	// a userinfo property.
	if ( CLIENT_GetAllowSendingOfUserInfo( ) == false )
		return;

	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_USERINFO );

	// [BB] This prevents accessing PlayerClasses[-1], when trying to send the class name.
	if ( (ulFlags & USERINFO_PLAYERCLASS) && (players[consoleplayer].userinfo.PlayerClass == -1) )
		ulFlags ^= USERINFO_PLAYERCLASS;

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
	if ( ulFlags & USERINFO_CONNECTIONTYPE )
		NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, players[consoleplayer].userinfo.lConnectionType );
	if (( (gameinfo.gametype == GAME_Hexen) || (PlayerClasses.Size() > 1) ) && ( ulFlags & USERINFO_PLAYERCLASS ))
		NETWORK_WriteString( &CLIENT_GetLocalBuffer( )->ByteStream, PlayerClasses[players[consoleplayer].userinfo.PlayerClass].Type->Meta.GetMetaString (APMETA_DisplayName));
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
void CLIENTCOMMANDS_Say( ULONG ulMode, const char *pszString )
{
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_SAY );
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, ulMode );
	NETWORK_WriteString( &CLIENT_GetLocalBuffer( )->ByteStream, pszString );
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
	if ( pCmd->ucmd.yaw )
		ulBits |= CLIENT_UPDATE_YAW;
	if ( pCmd->ucmd.pitch )
		ulBits |= CLIENT_UPDATE_PITCH;
	if ( pCmd->ucmd.roll )
		ulBits |= CLIENT_UPDATE_ROLL;
	if ( pCmd->ucmd.buttons )
		ulBits |= CLIENT_UPDATE_BUTTONS;
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
		if ( iwanttousecrouchingeventhoughitsretardedandunnecessaryanditsimplementationishorribleimeanverticallyshrinkingskinscomeonthatsinsanebutwhatevergoaheadandhaveyourcrouching == false )
			NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, pCmd->ucmd.buttons & ~BT_DUCK );
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

	// Attack button.
	if ( pCmd->ucmd.buttons & BT_ATTACK )
	{
		if ( players[consoleplayer].ReadyWeapon == NULL )
			NETWORK_WriteString( &CLIENT_GetLocalBuffer( )->ByteStream, "NULL" );
		else
			NETWORK_WriteString( &CLIENT_GetLocalBuffer( )->ByteStream, players[consoleplayer].ReadyWeapon->GetClass( )->TypeName.GetChars( ));
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
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_PONG );
	NETWORK_WriteLong( &CLIENT_GetLocalBuffer( )->ByteStream, ulTime );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_WeaponSelect( const char *pszWeapon )
{
	// Some optimization. For standard Doom weapons, to reduce the size of the string
	// that's sent out, just send some key character that identifies the weapon, instead
	// of the full name.
	NETWORK_ConvertWeaponNameToKeyLetter( pszWeapon );

	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_WEAPONSELECT );
	NETWORK_WriteString( &CLIENT_GetLocalBuffer( )->ByteStream, pszWeapon );
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
void CLIENTCOMMANDS_RequestJoin( char *pszJoinPassword )
{
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_REQUESTJOIN );
	NETWORK_WriteString( &CLIENT_GetLocalBuffer( )->ByteStream, pszJoinPassword );
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
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_SUICIDE );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_ChangeTeam( char *pszJoinPassword, LONG lDesiredTeam )
{
	if (( g_ulLastChangeTeamTime > 0 ) && ( (ULONG)gametic < ( g_ulLastChangeTeamTime + ( TICRATE * 10 ))))
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
void CLIENTCOMMANDS_SummonCheat( char *pszItem, bool bFriend )
{
	if( !bFriend )
		NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_SUMMONCHEAT );
	else
		NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_SUMMONFRIENDCHEAT );
	NETWORK_WriteString( &CLIENT_GetLocalBuffer( )->ByteStream, pszItem );
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
void CLIENTCOMMANDS_CallVote( LONG lVoteCommand, char *pszArgument )
{
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_CALLVOTE );
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, lVoteCommand );
	NETWORK_WriteString( &CLIENT_GetLocalBuffer( )->ByteStream, pszArgument );
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

	const char *pszString = item->GetClass( )->TypeName.GetChars( );

	if ( pszString == NULL )
		return;

	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_INVENTORYUSE );
	NETWORK_WriteString( &CLIENT_GetLocalBuffer( )->ByteStream, pszString );
}
