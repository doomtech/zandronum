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
#include "cl_main.h"
#include "gi.h"
#include "network.h"
#include "r_state.h"
#include "v_text.h" // [RC] To conform player names

//*****************************************************************************
//	FUNCTIONS

static	ULONG	g_ulLastChangeTeamTime = 0;

//*****************************************************************************
//	FUNCTIONS

void CLIENTCOMMANDS_UserInfo( ULONG ulFlags )
{
	NETWORK_WriteByte( CLIENT_GetLocalBuffer( ), CLC_USERINFO );

	// [BB] This prevents accessing PlayerClasses[-1], when trying to send the class name.
	if ( (ulFlags & USERINFO_PLAYERCLASS) && (players[consoleplayer].userinfo.PlayerClass == -1) )
		ulFlags ^= USERINFO_PLAYERCLASS;

	// Tell the server which items are being updated.
	NETWORK_WriteShort( CLIENT_GetLocalBuffer( ), ulFlags );

	if ( ulFlags & USERINFO_NAME )
	{ // [RC] Clean the name before we use it
		V_CleanPlayerName(players[consoleplayer].userinfo.netname);
		NETWORK_WriteString( CLIENT_GetLocalBuffer( ), players[consoleplayer].userinfo.netname );
	}
	if ( ulFlags & USERINFO_GENDER )
		NETWORK_WriteByte( CLIENT_GetLocalBuffer( ), players[consoleplayer].userinfo.gender );
	if ( ulFlags & USERINFO_COLOR )
		NETWORK_WriteLong( CLIENT_GetLocalBuffer( ), players[consoleplayer].userinfo.color );
	if ( ulFlags & USERINFO_AIMDISTANCE )
		NETWORK_WriteLong( CLIENT_GetLocalBuffer( ), players[consoleplayer].userinfo.aimdist );
	if ( ulFlags & USERINFO_SKIN )
		NETWORK_WriteString( CLIENT_GetLocalBuffer( ), skins[players[consoleplayer].userinfo.skin].name );
	if ( ulFlags & USERINFO_RAILCOLOR )
		NETWORK_WriteLong( CLIENT_GetLocalBuffer( ), players[consoleplayer].userinfo.lRailgunTrailColor );
	if ( ulFlags & USERINFO_HANDICAP )
		NETWORK_WriteByte( CLIENT_GetLocalBuffer( ), players[consoleplayer].userinfo.lHandicap );
	if ( ulFlags & USERINFO_CONNECTIONTYPE )
		NETWORK_WriteByte( CLIENT_GetLocalBuffer( ), players[consoleplayer].userinfo.lConnectionType );
	if (( (gameinfo.gametype == GAME_Hexen) || (PlayerClasses.Size() > 1) ) && ( ulFlags & USERINFO_PLAYERCLASS ))
		NETWORK_WriteString( CLIENT_GetLocalBuffer( ), PlayerClasses[players[consoleplayer].userinfo.PlayerClass].Type->Meta.GetMetaString (APMETA_DisplayName));
}

//*****************************************************************************
//
void CLIENTCOMMANDS_StartChat( void )
{
	NETWORK_WriteByte( CLIENT_GetLocalBuffer( ), CLC_STARTCHAT );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_EndChat( void )
{
	NETWORK_WriteByte( CLIENT_GetLocalBuffer( ), CLC_ENDCHAT );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_Say( ULONG ulMode, char *pszString )
{
	NETWORK_WriteByte( CLIENT_GetLocalBuffer( ), CLC_SAY );
	NETWORK_WriteByte( CLIENT_GetLocalBuffer( ), ulMode );
	NETWORK_WriteString( CLIENT_GetLocalBuffer( ), (char *)pszString );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_ClientMove( void )
{
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
	NETWORK_WriteByte( CLIENT_GetLocalBuffer( ), CLC_PONG );
	NETWORK_WriteLong( CLIENT_GetLocalBuffer( ), ulTime );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_WeaponSelect( const char *pszWeapon )
{
	// Some optimization. For standard Doom weapons, to reduce the size of the string
	// that's sent out, just send some key character that identifies the weapon, instead
	// of the full name.
	convertWeaponNameToKeyLetter( pszWeapon );

	NETWORK_WriteByte( CLIENT_GetLocalBuffer( ), CLC_WEAPONSELECT );
	NETWORK_WriteString( CLIENT_GetLocalBuffer( ), pszWeapon );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_Taunt( void )
{
	NETWORK_WriteByte( CLIENT_GetLocalBuffer( ), CLC_TAUNT );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_Spectate( void )
{
	NETWORK_WriteByte( CLIENT_GetLocalBuffer( ), CLC_SPECTATE );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_RequestJoin( char *pszJoinPassword )
{
	NETWORK_WriteByte( CLIENT_GetLocalBuffer( ), CLC_REQUESTJOIN );
	NETWORK_WriteString( CLIENT_GetLocalBuffer( ), pszJoinPassword );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_RequestRCON( char *pszRCONPassword )
{
	NETWORK_WriteByte( CLIENT_GetLocalBuffer( ), CLC_REQUESTRCON );
	NETWORK_WriteString( CLIENT_GetLocalBuffer( ), pszRCONPassword );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_RCONCommand( char *pszCommand )
{
	NETWORK_WriteByte( CLIENT_GetLocalBuffer( ), CLC_RCONCOMMAND );
	NETWORK_WriteString( CLIENT_GetLocalBuffer( ), pszCommand );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_Suicide( void )
{
	NETWORK_WriteByte( CLIENT_GetLocalBuffer( ), CLC_SUICIDE );
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
	NETWORK_WriteByte( CLIENT_GetLocalBuffer( ), CLC_CHANGETEAM );
	NETWORK_WriteString( CLIENT_GetLocalBuffer( ), pszJoinPassword );
	NETWORK_WriteByte( CLIENT_GetLocalBuffer( ), lDesiredTeam );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_SpectateInfo( void )
{
	NETWORK_WriteByte( CLIENT_GetLocalBuffer( ), CLC_SPECTATEINFO );
	NETWORK_WriteLong( CLIENT_GetLocalBuffer( ), gametic );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_GenericCheat( LONG lCheat )
{
	NETWORK_WriteByte( CLIENT_GetLocalBuffer( ), CLC_GENERICCHEAT );
	NETWORK_WriteByte( CLIENT_GetLocalBuffer( ), lCheat );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_GiveCheat( char *pszItem, LONG lAmount )
{
	NETWORK_WriteByte( CLIENT_GetLocalBuffer( ), CLC_GIVECHEAT );
	NETWORK_WriteString( CLIENT_GetLocalBuffer( ), pszItem );
	NETWORK_WriteByte( CLIENT_GetLocalBuffer( ), lAmount );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_SummonCheat( char *pszItem, bool bFriend )
{
	if( !bFriend )
		NETWORK_WriteByte( CLIENT_GetLocalBuffer( ), CLC_SUMMONCHEAT );
	else
		NETWORK_WriteByte( CLIENT_GetLocalBuffer( ), CLC_SUMMONFRIENDCHEAT );
	NETWORK_WriteString( CLIENT_GetLocalBuffer( ), pszItem );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_ReadyToGoOn( void )
{
	NETWORK_WriteByte( CLIENT_GetLocalBuffer( ), CLC_READYTOGOON );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_ChangeDisplayPlayer( LONG lDisplayPlayer )
{
	NETWORK_WriteByte( CLIENT_GetLocalBuffer( ), CLC_CHANGEDISPLAYPLAYER );
	NETWORK_WriteByte( CLIENT_GetLocalBuffer( ), lDisplayPlayer );
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
	NETWORK_WriteByte( CLIENT_GetLocalBuffer( ), CLC_CALLVOTE );
	NETWORK_WriteByte( CLIENT_GetLocalBuffer( ), lVoteCommand );
	NETWORK_WriteString( CLIENT_GetLocalBuffer( ), pszArgument );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_VoteYes( void )
{
	NETWORK_WriteByte( CLIENT_GetLocalBuffer( ), CLC_VOTEYES );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_VoteNo( void )
{
	NETWORK_WriteByte( CLIENT_GetLocalBuffer( ), CLC_VOTENO );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_RequestInventoryUseAll( void )
{
	NETWORK_WriteByte( CLIENT_GetLocalBuffer( ), CLC_INVENTORYUSEALL );
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

	NETWORK_WriteByte( CLIENT_GetLocalBuffer( ), CLC_INVENTORYUSE );
	NETWORK_WriteString( CLIENT_GetLocalBuffer( ), pszString );
}
