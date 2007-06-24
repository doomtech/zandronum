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
// Filename: sv_commands.cpp
//
// Description: Contains a set of functions that correspond to each message a server
// can send out. Each functions handles the send out of each message.
//
//-----------------------------------------------------------------------------

#include "chat.h"
#include "cooperative.h"
#include "deathmatch.h"
#include "doomtype.h"
#include "duel.h"
#include "g_game.h"
#include "gi.h"
#include "invasion.h"
#include "joinqueue.h"
#include "lastmanstanding.h"
#include "network.h"
#include "p_local.h"
#include "p_spec.h"
#include "r_state.h"
#include "sbar.h"
#include "sv_commands.h"
#include "sv_main.h"
#include "team.h"
#include "vectors.h"

// :((((((
polyobj_t	*GetPolyobj( int polyNum );
extern char *g_szActorKeyLetter[NUMBER_OF_ACTOR_NAME_KEY_LETTERS];
extern char *g_szActorFullName[NUMBER_OF_ACTOR_NAME_KEY_LETTERS];

EXTERN_CVAR( Bool, sv_stay97c3compatible )

//*****************************************************************************
//	FUNCTIONS

inline void initNetNameString( AActor *pActor, const char *&pszName ){
	pszName = NULL;
	for( int i = 0; i < NUMBER_OF_ACTOR_NAME_KEY_LETTERS; i++ ){
		if ( stricmp( pActor->GetClass( )->TypeName.GetChars( ), g_szActorFullName[i] ) == 0 ){
			pszName = g_szActorKeyLetter[i];
			break;
		}
	}
	if( pszName == NULL )
		pszName = pActor->GetClass( )->TypeName.GetChars( );
}

//*****************************************************************************
//
void SERVERCOMMANDS_Ping( ULONG ulTime )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		NETWORK_CheckBuffer( ulIdx, 5 );
		NETWORK_WriteHeader( &clients[ulIdx].UnreliablePacketBuffer, SVC_PING );
		NETWORK_WriteLong( &clients[ulIdx].UnreliablePacketBuffer, ulTime );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_Nothing( ULONG ulPlayer )
{
	if ( SERVER_IsValidClient( ulPlayer ) == false )
		return;

	NETWORK_CheckBuffer( ulPlayer, 1 );
	NETWORK_WriteHeader( &clients[ulPlayer].UnreliablePacketBuffer, SVC_NOTHING );
}

//*****************************************************************************
//
void SERVERCOMMANDS_ResetSequence( ULONG ulPlayer )
{
	if ( SERVER_IsValidClient( ulPlayer ) == false )
		return;

	NETWORK_CheckBuffer( ulPlayer, 1 );
	NETWORK_WriteHeader( &clients[ulPlayer].netbuf, SVC_RESETSEQUENCE );
}

//*****************************************************************************
//
void SERVERCOMMANDS_BeginSnapshot( ULONG ulPlayer )
{
	if ( SERVER_IsValidClient( ulPlayer ) == false )
		return;

	NETWORK_CheckBuffer( ulPlayer, 1 );
	NETWORK_WriteHeader( &clients[ulPlayer].netbuf, SVC_BEGINSNAPSHOT );
}

//*****************************************************************************
//
void SERVERCOMMANDS_EndSnapshot( ULONG ulPlayer )
{
	if ( SERVER_IsValidClient( ulPlayer ) == false )
		return;

	NETWORK_CheckBuffer( ulPlayer, 1 );
	NETWORK_WriteHeader( &clients[ulPlayer].netbuf, SVC_ENDSNAPSHOT );
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_SpawnPlayer( ULONG ulPlayer, LONG lPlayerState, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( SERVER_IsValidPlayer( ulPlayer ) == false )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue; 

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 25 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SPAWNPLAYER );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, ulPlayer );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, lPlayerState );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, players[ulPlayer].bIsBot );
		// Do we really need to send this? Shouldn't it always be PST_LIVE?
		NETWORK_WriteByte( &clients[ulIdx].netbuf, players[ulPlayer].playerstate );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, players[ulPlayer].bSpectating );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, players[ulPlayer].bDeadSpectator );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, players[ulPlayer].mo->lNetID );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, players[ulPlayer].mo->angle );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, players[ulPlayer].mo->x );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, players[ulPlayer].mo->y );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, players[ulPlayer].mo->z );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, players[ulPlayer].CurrentPlayerClass );
		//NETWORK_WriteByte( &clients[ulIdx].netbuf, players[ulPlayer].userinfo.PlayerClass );
	}
	// [BB]: Inform the player about its health, otherwise it won't be displayed properly.
	// The armor display is handled in SERVER_ResetInventory.
	SERVERCOMMANDS_SetPlayerHealth( ulPlayer, ulPlayer, SVCF_ONLYTHISCLIENT );
	// [BB]: If the player still has NOCLIP activated from the last level, tell
	// him about it. Not doing this leads to jerky movement on client side.
	if( players[ulPlayer].cheats & CF_NOCLIP )
		SERVERCOMMANDS_GenericCheat( ulPlayer, CF_NOCLIP, ulPlayer, SVCF_ONLYTHISCLIENT );
}

//*****************************************************************************
//
void SERVERCOMMANDS_MovePlayer( ULONG ulPlayer, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( SERVER_IsValidPlayer( ulPlayer ) == false )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 20 );
		NETWORK_WriteHeader( &clients[ulIdx].UnreliablePacketBuffer, SVC_MOVEPLAYER );
		NETWORK_WriteByte( &clients[ulIdx].UnreliablePacketBuffer, ulPlayer );

		// If this player cannot be seen by (or is not allowed to be seen by) the
		// player, don't send position information.
		if ( SERVER_IsPlayerVisible( ulIdx, ulPlayer ) == false )
			NETWORK_WriteByte( &clients[ulIdx].UnreliablePacketBuffer, false );
		else
		{
			// The player IS visible, so his info is coming!
			NETWORK_WriteByte( &clients[ulIdx].UnreliablePacketBuffer, true );

			// Write position.
			NETWORK_WriteShort( &clients[ulIdx].UnreliablePacketBuffer, players[ulPlayer].mo->x >> FRACBITS );
			NETWORK_WriteShort( &clients[ulIdx].UnreliablePacketBuffer, players[ulPlayer].mo->y >> FRACBITS );
			NETWORK_WriteShort( &clients[ulIdx].UnreliablePacketBuffer, players[ulPlayer].mo->z >> FRACBITS );

			// Write angle.
			NETWORK_WriteLong( &clients[ulIdx].UnreliablePacketBuffer, players[ulPlayer].mo->angle );

			// Write velocity.
			NETWORK_WriteShort( &clients[ulIdx].UnreliablePacketBuffer, players[ulPlayer].mo->momx >> FRACBITS );
			NETWORK_WriteShort( &clients[ulIdx].UnreliablePacketBuffer, players[ulPlayer].mo->momy >> FRACBITS );
			NETWORK_WriteShort( &clients[ulIdx].UnreliablePacketBuffer, players[ulPlayer].mo->momz >> FRACBITS );

			// Write whether or not the player is crouching.
			NETWORK_WriteByte( &clients[ulIdx].UnreliablePacketBuffer, ( players[ulPlayer].crouchdir >= 0 ) ? true : false );
		}
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_DamagePlayer( ULONG ulPlayer )
{
	ULONG		ulIdx;
	ULONG		ulArmorPoints;
	AInventory	*pArmor;

	if ( SERVER_IsValidPlayer( ulPlayer ) == false )
		return;

	// Determine how much armor the damaged player now has, so we can send that information.
	if ( players[ulPlayer].mo == NULL )
		ulArmorPoints = 0;
	else
	{
		// Search the player's inventory for armor.
		pArmor = players[ulPlayer].mo->FindInventory< ABasicArmor >( );

		// If the player doesn't possess any armor, then his armor points are 0.
		ulArmorPoints = ( pArmor != NULL ) ? pArmor->Amount : 0;
	}

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		NETWORK_CheckBuffer( ulIdx, 4 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_DAMAGEPLAYER );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, ulPlayer );

		// Only send the player who's being damaged to this player if this player is
		// allowed to know what his health is. Otherwise, just tell them it's 100/100
		// (WHICH IS A LIE!!!!!!).
		if ( SERVER_IsPlayerAllowedToKnowHealth( ulIdx, ulPlayer ))
		{
			NETWORK_WriteShort( &clients[ulIdx].netbuf, players[ulPlayer].health );
			NETWORK_WriteShort( &clients[ulIdx].netbuf, ulArmorPoints );
		}
		else
		{
			NETWORK_WriteShort( &clients[ulIdx].netbuf, 100 );
			NETWORK_WriteShort( &clients[ulIdx].netbuf, 100 );
		}
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_KillPlayer( ULONG ulPlayer, AActor *pSource, AActor *pInflictor, ULONG ulMOD )
{
	ULONG	ulIdx;
	char	*pszString;
	LONG	lSourceID;
	LONG	lInflictorID;

	if ( SERVER_IsValidPlayer( ulPlayer ) == false )
		return;

	pszString = NULL;
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if (( playeringame[ulIdx] == false ) ||
			( players[ulIdx].mo == NULL ))
		{
			continue;
		}

		if ( players[ulIdx].mo == pSource )
		{
			if ( players[ulIdx].ReadyWeapon != NULL )
				pszString = (char *)players[ulIdx].ReadyWeapon->GetClass( )->TypeName.GetChars( );
			else
				pszString = NULL;
			break;
		}
	}

	if ( pSource )
		lSourceID = pSource->lNetID;
	else
		lSourceID = -1;

	if ( pInflictor )
		lInflictorID = pInflictor->lNetID;
	else
		lInflictorID = -1;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ( pszString )
			NETWORK_CheckBuffer( ulIdx, 9 + (ULONG)strlen( pszString ));
		else
			NETWORK_CheckBuffer( ulIdx, 9 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_KILLPLAYER );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, ulPlayer );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, lSourceID );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, lInflictorID );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, players[ulPlayer].mo->health );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, ulMOD );
		NETWORK_WriteString( &clients[ulIdx].netbuf, pszString );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerHealth( ULONG ulPlayer, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( SERVER_IsValidPlayer( ulPlayer ) == false )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		if ( SERVER_IsPlayerAllowedToKnowHealth( ulIdx, ulPlayer ))
		{
			NETWORK_CheckBuffer( ulIdx, 3 );
			NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SETPLAYERHEALTH );
			NETWORK_WriteByte( &clients[ulIdx].netbuf, ulPlayer );
			NETWORK_WriteShort( &clients[ulIdx].netbuf, players[ulPlayer].health );
		}
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_UpdatePlayerArmorDisplay( ULONG ulPlayer )
{
	// [BB] 97c3 clients don't know this command.
	if( sv_stay97c3compatible )
		return;

	AInventory *pArmor = players[ulPlayer].mo->FindInventory< ABasicArmor >( );
	ULONG ulArmorPoints = ( pArmor != NULL ) ? pArmor->Amount : 0;
	if ( ulArmorPoints > 0 ){
		NETWORK_CheckBuffer( ulPlayer, 3 + (ULONG)strlen( TexMan( pArmor->Icon )->Name ) );
		NETWORK_WriteHeader( &clients[ulPlayer].netbuf, SVC_UPDATEPLAYERARMORDISPLAY );
		NETWORK_WriteByte( &clients[ulPlayer].netbuf, ulPlayer );
		NETWORK_WriteShort( &clients[ulPlayer].netbuf, ulArmorPoints );
		NETWORK_WriteString( &clients[ulPlayer].netbuf, TexMan( pArmor->Icon )->Name );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerState( ULONG ulPlayer, ULONG ulState, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( SERVER_IsValidPlayer( ulPlayer ) == false )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 3 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SETPLAYERSTATE );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, ulPlayer );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, ulState );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerUserInfo( ULONG ulPlayer, ULONG ulUserInfoFlags, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG		ulIdx;

	if ( SERVER_IsValidPlayer( ulPlayer ) == false )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 13 + (ULONG)( strlen( players[ulPlayer].userinfo.netname ) + strlen( skins[players[ulPlayer].userinfo.skin].name )));
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SETPLAYERUSERINFO );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, ulPlayer );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, ulUserInfoFlags );
		if ( ulUserInfoFlags & USERINFO_NAME )
			NETWORK_WriteString( &clients[ulIdx].netbuf, players[ulPlayer].userinfo.netname );

		if ( ulUserInfoFlags & USERINFO_GENDER )
			NETWORK_WriteByte( &clients[ulIdx].netbuf, players[ulPlayer].userinfo.gender );

		if ( ulUserInfoFlags & USERINFO_COLOR )
			NETWORK_WriteLong( &clients[ulIdx].netbuf, players[ulPlayer].userinfo.color );

		if ( ulUserInfoFlags & USERINFO_RAILCOLOR )
			NETWORK_WriteByte( &clients[ulIdx].netbuf, players[ulPlayer].userinfo.lRailgunTrailColor );

		if ( ulUserInfoFlags & USERINFO_SKIN )
			NETWORK_WriteString( &clients[ulIdx].netbuf, (char *)skins[players[ulPlayer].userinfo.skin].name );

		if ( ulUserInfoFlags & USERINFO_HANDICAP )
			NETWORK_WriteByte( &clients[ulIdx].netbuf, players[ulPlayer].userinfo.lHandicap );

		if ( ulUserInfoFlags & USERINFO_PLAYERCLASS )
			NETWORK_WriteByte( &clients[ulIdx].netbuf, players[ulPlayer].userinfo.PlayerClass );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerFrags( ULONG ulPlayer, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG		ulIdx;

	if ( SERVER_IsValidPlayer( ulPlayer ) == false )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 4 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SETPLAYERFRAGS );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, ulPlayer );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, players[ulPlayer].fragcount );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerPoints( ULONG ulPlayer, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG		ulIdx;

	if ( SERVER_IsValidPlayer( ulPlayer ) == false )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 4 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SETPLAYERPOINTS );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, ulPlayer );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, players[ulPlayer].lPointCount );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerWins( ULONG ulPlayer, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( SERVER_IsValidPlayer( ulPlayer ) == false )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 3 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SETPLAYERWINS );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, ulPlayer );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, players[ulPlayer].ulWins );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerKillCount( ULONG ulPlayer, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( SERVER_IsValidPlayer( ulPlayer ) == false )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 3 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SETPLAYERKILLCOUNT );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, ulPlayer );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, players[ulPlayer].killcount );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerChatStatus( ULONG ulPlayer, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( SERVER_IsValidPlayer( ulPlayer ) == false )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 3 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SETPLAYERCHATSTATUS );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, ulPlayer );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, players[ulPlayer].bChatting );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerLaggingStatus( ULONG ulPlayer, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( SERVER_IsValidPlayer( ulPlayer ) == false )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 3 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SETPLAYERLAGGINGSTATUS );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, ulPlayer );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, players[ulPlayer].bLagging );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerReadyToGoOnStatus( ULONG ulPlayer, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( SERVER_IsValidPlayer( ulPlayer ) == false )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 3 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SETPLAYERREADYTOGOONSTATUS );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, ulPlayer );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, players[ulPlayer].bReadyToGoOn );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerTeam( ULONG ulPlayer, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( SERVER_IsValidPlayer( ulPlayer ) == false )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 3 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SETPLAYERTEAM );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, ulPlayer );
		if ( players[ulPlayer].bOnTeam == false )
			NETWORK_WriteByte( &clients[ulIdx].netbuf, NUM_TEAMS );
		else
			NETWORK_WriteByte( &clients[ulIdx].netbuf, players[ulPlayer].ulTeam );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerCamera( ULONG ulPlayer, LONG lCameraNetID, bool bRevertPlease )
{
	if ( SERVER_IsValidClient( ulPlayer ) == false )
		return;

	NETWORK_CheckBuffer( ulPlayer, 4 );
	NETWORK_WriteHeader( &clients[ulPlayer].netbuf, SVC_SETPLAYERCAMERA );
	NETWORK_WriteShort( &clients[ulPlayer].netbuf, lCameraNetID );
	NETWORK_WriteByte( &clients[ulPlayer].netbuf, bRevertPlease );
}

//*****************************************************************************
//
void SERVERCOMMANDS_UpdatePlayerPing( ULONG ulPlayer, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( SERVER_IsValidPlayer( ulPlayer ) == false )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 4 );
		NETWORK_WriteHeader( &clients[ulIdx].UnreliablePacketBuffer, SVC_UPDATEPLAYERPING );
		NETWORK_WriteByte( &clients[ulIdx].UnreliablePacketBuffer, ulPlayer );
		NETWORK_WriteShort( &clients[ulIdx].UnreliablePacketBuffer, players[ulPlayer].ulPing );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_UpdatePlayerExtraData( ULONG ulPlayer, ULONG ulDisplayPlayer )
{
	if (( SERVER_IsValidClient( ulPlayer ) == false ) || ( SERVER_IsValidPlayer( ulDisplayPlayer ) == false ))
		return;

	NETWORK_CheckBuffer( ulPlayer, 18 );
	NETWORK_WriteHeader( &clients[ulPlayer].UnreliablePacketBuffer, SVC_UPDATEPLAYEREXTRADATA );
	NETWORK_WriteByte( &clients[ulPlayer].UnreliablePacketBuffer, ulDisplayPlayer );

//	NETWORK_WriteByte( &clients[ulPlayer].UnreliablePacketBuffer, players[ulDisplayPlayer].pendingweapon );
//	NETWORK_WriteByte( &clients[ulPlayer].UnreliablePacketBuffer, players[ulDisplayPlayer].readyweapon );
	NETWORK_WriteLong( &clients[ulPlayer].UnreliablePacketBuffer, players[ulDisplayPlayer].mo->pitch );
	NETWORK_WriteByte( &clients[ulPlayer].UnreliablePacketBuffer, players[ulDisplayPlayer].mo->waterlevel );
	NETWORK_WriteByte( &clients[ulPlayer].UnreliablePacketBuffer, players[ulDisplayPlayer].cmd.ucmd.buttons );
	NETWORK_WriteLong( &clients[ulPlayer].UnreliablePacketBuffer, players[ulDisplayPlayer].viewz );
	NETWORK_WriteLong( &clients[ulPlayer].UnreliablePacketBuffer, players[ulDisplayPlayer].bob );

}

//*****************************************************************************
//
void SERVERCOMMANDS_UpdatePlayerPendingWeapon( ULONG ulPlayer )
{
	// [BB] 97c3 clients don't know this command.
	if( sv_stay97c3compatible )
		return;

	if ( SERVER_IsValidPlayer( ulPlayer ) == false )
		return;

	const char* pszPendingWeaponString = "NULL";
	if ( players[ulPlayer].PendingWeapon != WP_NOCHANGE && players[ulPlayer].PendingWeapon != NULL )
		pszPendingWeaponString = players[ulPlayer].PendingWeapon->GetClass( )->TypeName.GetChars( );
	else
		return;
	// Some optimization. For standard Doom weapons, to reduce the size of the string
	// that's sent out, just send some key character that identifies the weapon, instead
	// of the full name.
	convertWeaponNameToKeyLetter( pszPendingWeaponString );

	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ( ulPlayer == ulIdx )
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 2 + (ULONG)strlen( pszPendingWeaponString ) );
		NETWORK_WriteHeader( &clients[ulIdx].UnreliablePacketBuffer, SVC_UPDATEPLAYEREPENDINGWEAPON );
		NETWORK_WriteByte( &clients[ulIdx].UnreliablePacketBuffer, ulPlayer );
		NETWORK_WriteString( &clients[ulIdx].UnreliablePacketBuffer, pszPendingWeaponString );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_DoInventoryUse( ULONG ulPlayer, AInventory *item )
{
	// [BB] 97c3 clients don't know this command.
	if( sv_stay97c3compatible )
		return;

	if ( SERVER_IsValidPlayer( ulPlayer ) == false )
		return;

	const char *pszString = item->GetClass( )->TypeName.GetChars( );

	if ( pszString == NULL )
		return;

	NETWORK_CheckBuffer( ulPlayer, 1 + (ULONG)strlen( pszString ));
	NETWORK_WriteHeader( &clients[ulPlayer].netbuf, SVC_USEINVENTORY );
	NETWORK_WriteString( &clients[ulPlayer].netbuf, pszString );
}

//*****************************************************************************
//
void SERVERCOMMANDS_ChangePlayerWeapon( ULONG ulPlayer )
{
	// [BB] 97c3 clients don't know this command.
	if( sv_stay97c3compatible )
		return;

	if ( SERVER_IsValidPlayer( ulPlayer ) == false )
		return;

	const char* pszWeaponString = "NULL";
	if ( players[ulPlayer].ReadyWeapon != NULL )
		pszWeaponString = players[ulPlayer].ReadyWeapon->GetClass( )->TypeName.GetChars( );
	else
		return;
	// Some optimization. For standard Doom weapons, to reduce the size of the string
	// that's sent out, just send some key character that identifies the weapon, instead
	// of the full name.
	convertWeaponNameToKeyLetter( pszWeaponString );

	NETWORK_CheckBuffer( ulPlayer, 1 + (ULONG)strlen( pszWeaponString ) );
	NETWORK_WriteHeader( &clients[ulPlayer].UnreliablePacketBuffer, SVC_WEAPONCHANGE );
	NETWORK_WriteString( &clients[ulPlayer].UnreliablePacketBuffer, pszWeaponString );
}

//*****************************************************************************
//
void SERVERCOMMANDS_UpdatePlayerTime( ULONG ulPlayer, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( SERVER_IsValidPlayer( ulPlayer ) == false )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulPlayer, 4 );
		NETWORK_WriteHeader( &clients[ulIdx].UnreliablePacketBuffer, SVC_UPDATEPLAYERTIME );
		NETWORK_WriteByte( &clients[ulIdx].UnreliablePacketBuffer, ulPlayer );
		NETWORK_WriteShort( &clients[ulIdx].UnreliablePacketBuffer, ( players[ulPlayer].ulTime / ( TICRATE * 60 )));
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_MoveLocalPlayer( ULONG ulPlayer )
{
	if ( SERVER_IsValidClient( ulPlayer ) == false )
		return;

	NETWORK_CheckBuffer( ulPlayer, 30 );
	NETWORK_WriteHeader( &clients[ulPlayer].UnreliablePacketBuffer, SVC_MOVELOCALPLAYER );

	NETWORK_WriteLong( &clients[ulPlayer].UnreliablePacketBuffer, clients[ulPlayer].gametics );

	// Write position.
	NETWORK_WriteLong( &clients[ulPlayer].UnreliablePacketBuffer, players[ulPlayer].mo->x );
	NETWORK_WriteLong( &clients[ulPlayer].UnreliablePacketBuffer, players[ulPlayer].mo->y );
	NETWORK_WriteLong( &clients[ulPlayer].UnreliablePacketBuffer, players[ulPlayer].mo->z );

	// Write velocity.
	NETWORK_WriteLong( &clients[ulPlayer].UnreliablePacketBuffer, players[ulPlayer].mo->momx );
	NETWORK_WriteLong( &clients[ulPlayer].UnreliablePacketBuffer, players[ulPlayer].mo->momy );
	NETWORK_WriteLong( &clients[ulPlayer].UnreliablePacketBuffer, players[ulPlayer].mo->momz );

	// Write waterlevel.
	NETWORK_WriteByte( &clients[ulPlayer].UnreliablePacketBuffer, players[ulPlayer].mo->waterlevel );
}

//*****************************************************************************
//
void SERVERCOMMANDS_DisconnectPlayer( ULONG ulPlayer, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( SERVER_IsValidPlayer( ulPlayer ) == false )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 2 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_DISCONNECTPLAYER );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, ulPlayer );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetConsolePlayer( ULONG ulPlayer )
{
	if ( SERVER_IsValidClient( ulPlayer ) == false )
		return;

	NETWORK_CheckBuffer( ulPlayer, 2 );
	NETWORK_WriteHeader( &clients[ulPlayer].netbuf, SVC_SETCONSOLEPLAYER );
	NETWORK_WriteByte( &clients[ulPlayer].netbuf, ulPlayer );
}

//*****************************************************************************
//
void SERVERCOMMANDS_ConsolePlayerKicked( ULONG ulPlayer )
{
	if ( SERVER_IsValidClient( ulPlayer ) == false )
		return;

	NETWORK_CheckBuffer( ulPlayer, 1 );
	NETWORK_WriteHeader( &clients[ulPlayer].netbuf, SVC_CONSOLEPLAYERKICKED );
}

//*****************************************************************************
//
void SERVERCOMMANDS_GivePlayerMedal( ULONG ulPlayer, ULONG ulMedal, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( SERVER_IsValidPlayer( ulPlayer ) == false )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 3 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_GIVEPLAYERMEDAL );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, ulPlayer );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, ulMedal );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_ResetAllPlayersFragcount( ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 1 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_RESETALLPLAYERSFRAGCOUNT );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_PlayerIsSpectator( ULONG ulPlayer, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( SERVER_IsValidPlayer( ulPlayer ) == false )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 3 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_PLAYERISSPECTATOR );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, ulPlayer );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, players[ulPlayer].bDeadSpectator );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_PlayerSay( ULONG ulPlayer, char *pszString, ULONG ulMode, bool bForbidChatToPlayers, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( ulPlayer != MAXPLAYERS )
	{
		if ( SERVER_IsValidPlayer( ulPlayer ) == false )
			return;
	}

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		// Don't allow the chat message to be broadcasted to this player.
		if (( bForbidChatToPlayers ) && ( players[ulIdx].bSpectating == false ))
			continue;

		// The player is sending a message to his teammates.
		if ( ulMode == CHATMODE_TEAM )
		{
			if ( teamgame || teamplay || teamlms || teampossession )
			{
				// If either player is not on a team, don't send the message.
				if (( players[ulIdx].bOnTeam == false ) || ( players[ulPlayer].bOnTeam == false ))
					continue;

				// If the players are not on the same team, don't send the message.
				if ( players[ulIdx].ulTeam != players[ulPlayer].ulTeam )
					continue;
			}
			// Not in a team mode.
			else
				continue;
		}

		NETWORK_CheckBuffer( ulIdx, 3 + (ULONG)strlen( pszString ));
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_PLAYERSAY );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, ulPlayer );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, ulMode );
		NETWORK_WriteString( &clients[ulIdx].netbuf, pszString );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_PlayerTaunt( ULONG ulPlayer, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( SERVER_IsValidPlayer( ulPlayer ) == false )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 2 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_PLAYERTAUNT );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, ulPlayer );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_PlayerRespawnInvulnerability( ULONG ulPlayer, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( SERVER_IsValidPlayer( ulPlayer ) == false )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 2 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_PLAYERRESPAWNINVULNERABILITY );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, ulPlayer );
	}
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_SpawnThing( AActor *pActor, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG		ulIdx;
	const char	*pszName;

	if ( pActor == NULL )
		return;

	// If the actor doesn't have a network ID, it's better to send it ID-less.
	if ( pActor->lNetID == -1 )
	{
		SERVERCOMMANDS_SpawnThingNoNetID( pActor, ulPlayerExtra, ulFlags );
		return;
	}

	// Some optimization. For some actors that are sent in bunches, to reduce the size,
	// just send some key letter that identifies the actor, instead of the full name.
	initNetNameString( pActor, pszName );

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		// [BB] I'm sure it has to be strlen(pszName) here.
		//NETWORK_CheckBuffer( ulIdx, 9 + (ULONG)strlen( pActor->GetClass( )->TypeName.GetChars( )));
		NETWORK_CheckBuffer( ulIdx, 9 + (ULONG)strlen( pszName ));
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SPAWNTHING );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, pActor->x >> FRACBITS );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, pActor->y >> FRACBITS );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, pActor->z >> FRACBITS );
		NETWORK_WriteString( &clients[ulIdx].netbuf, pszName );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, pActor->lNetID );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SpawnThingNoNetID( AActor *pActor, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG		ulIdx;
	const char	*pszName;

	if ( pActor == NULL )
		return;

	// Some optimization. For some actors that are sent in bunches, to reduce the size,
	// just send some key letter that identifies the actor, instead of the full name.
	initNetNameString( pActor, pszName );

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		// [BB] I'm sure it has to be strlen(pszName) here.
		//NETWORK_CheckBuffer( ulIdx, 7 + (ULONG)strlen( pActor->GetClass( )->TypeName.GetChars( )));
		NETWORK_CheckBuffer( ulIdx, 7 + (ULONG)strlen( pszName ));
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SPAWNTHINGNONETID );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, pActor->x >> FRACBITS );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, pActor->y >> FRACBITS );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, pActor->z >> FRACBITS );
		NETWORK_WriteString( &clients[ulIdx].netbuf, pszName );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SpawnThingExact( AActor *pActor, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG		ulIdx;
	const char	*pszName;

	if ( pActor == NULL )
		return;

	// If the actor doesn't have a network ID, it's better to send it ID-less.
	if ( pActor->lNetID == -1 )
	{
		SERVERCOMMANDS_SpawnThingExactNoNetID( pActor, ulPlayerExtra, ulFlags );
		return;
	}

	// Some optimization. For some actors that are sent in bunches, to reduce the size,
	// just send some key letter that identifies the actor, instead of the full name.
	initNetNameString( pActor, pszName );

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		// [BB] I'm sure it has to be strlen(pszName) here.
		//NETWORK_CheckBuffer( ulIdx, 15 + (ULONG)strlen( pActor->GetClass( )->TypeName.GetChars( )));
		NETWORK_CheckBuffer( ulIdx, 15 + (ULONG)strlen( pszName ));
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SPAWNTHINGEXACT );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, pActor->x );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, pActor->y );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, pActor->z );
		NETWORK_WriteString( &clients[ulIdx].netbuf, pszName );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, pActor->lNetID );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SpawnThingExactNoNetID( AActor *pActor, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG		ulIdx;
	const char	*pszName;

	if ( pActor == NULL )
		return;

	// Some optimization. For some actors that are sent in bunches, to reduce the size,
	// just send some key letter that identifies the actor, instead of the full name.
	initNetNameString( pActor, pszName );

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		// [BB] I'm sure it has to be strlen(pszName) here.
		//NETWORK_CheckBuffer( ulIdx, 13 + (ULONG)strlen( pActor->GetClass( )->TypeName.GetChars( )));
		NETWORK_CheckBuffer( ulIdx, 13 + (ULONG)strlen( pszName ));
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SPAWNTHINGEXACTNONETID );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, pActor->x );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, pActor->y );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, pActor->z );
		NETWORK_WriteString( &clients[ulIdx].netbuf, pszName );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_MoveThing( AActor *pActor, ULONG ulBits, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;
	ULONG	ulSize;

	if ( pActor == NULL )
		return;

	ulSize = 0;
	if ( ulBits & CM_X )
		ulSize += 2;
	if ( ulBits & CM_Y )
		ulSize += 2;
	if ( ulBits & CM_Z )
		ulSize += 2;
	if ( ulBits & CM_ANGLE )
		ulSize += 4;
	if ( ulBits & CM_MOMX )
		ulSize += 2;
	if ( ulBits & CM_MOMY )
		ulSize += 2;
	if ( ulBits & CM_MOMZ )
		ulSize += 2;

	// Nothing to update.
	if ( ulSize == 0 )
		return;

	ulSize += 5;
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{ 
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;
		
		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, ulSize );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_MOVETHING );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, pActor->lNetID );

		NETWORK_WriteShort( &clients[ulIdx].netbuf, ulBits );

		// Write position.
		if ( ulBits & CM_X )
			NETWORK_WriteShort( &clients[ulIdx].netbuf, pActor->x >> FRACBITS );				
		if ( ulBits & CM_Y )
			NETWORK_WriteShort( &clients[ulIdx].netbuf, pActor->y >> FRACBITS );
		if ( ulBits & CM_Z )
			NETWORK_WriteShort( &clients[ulIdx].netbuf, pActor->z >> FRACBITS );
		
		// Write angle.
		if ( ulBits & CM_ANGLE )
			NETWORK_WriteLong( &clients[ulIdx].netbuf, pActor->angle );

		// Write velocity.
		if ( ulBits & CM_MOMX )
			NETWORK_WriteShort( &clients[ulIdx].netbuf, pActor->momx >> FRACBITS );
		if ( ulBits & CM_MOMY )
			NETWORK_WriteShort( &clients[ulIdx].netbuf, pActor->momy >> FRACBITS );
		if ( ulBits & CM_MOMZ )
			NETWORK_WriteShort( &clients[ulIdx].netbuf, pActor->momz >> FRACBITS );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_DamageThing( AActor *pActor )
{
	ULONG	ulIdx;

	if ( pActor == NULL )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		NETWORK_CheckBuffer( ulIdx, 3 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_DAMAGETHING );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, pActor->lNetID );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_KillThing( AActor *pActor, bool bGib )
{
	ULONG	ulIdx;

	if ( pActor == NULL )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		NETWORK_CheckBuffer( ulIdx, 3 );
		// [BB] Tell the client if the thing should be gibbed or just killed.
		// [BB] 97c3 clients don't know SVC_GIBTHING.
		if( bGib == false || sv_stay97c3compatible == true )
			NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_KILLTHING );
		else
			NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_GIBTHING );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, pActor->lNetID );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetThingState( AActor *pActor, ULONG ulState )
{
	ULONG	ulIdx;

	if ( pActor == NULL )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		NETWORK_CheckBuffer( ulIdx, 4 );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, SVC_SETTHINGSTATE );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, pActor->lNetID );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, ulState );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_DestroyThing( AActor *pActor )
{
	ULONG	ulIdx;

	if ( pActor == NULL )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		NETWORK_CheckBuffer( ulIdx, 3 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_DESTROYTHING );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, pActor->lNetID );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetThingAngle( AActor *pActor, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( pActor == NULL )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 7 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SETTHINGANGLE );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, pActor->lNetID );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, pActor->angle );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetThingTID( AActor *pActor, ULONG ulPlayerExtra, ULONG ulFlags )
{
	// [BB] 97c3 clients don't know this command.
	if( sv_stay97c3compatible )
		return;

	ULONG	ulIdx;

	if ( pActor == NULL )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 5 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SETTHINGTID );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, pActor->lNetID );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, pActor->tid );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetThingWaterLevel( AActor *pActor, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( pActor == NULL )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 4 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SETTHINGWATERLEVEL );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, pActor->lNetID );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, pActor->waterlevel );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetThingFlags( AActor *pActor, ULONG ulFlagSet, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;
	ULONG	ulActorFlags;

	if ( pActor == NULL )
		return;

	switch ( ulFlagSet )
	{
	case FLAGSET_FLAGS:

		ulActorFlags = pActor->flags;
		break;
	case FLAGSET_FLAGS2:

		ulActorFlags = pActor->flags2;
		break;
	case FLAGSET_FLAGS3:

		ulActorFlags = pActor->flags3;
		break;
	case FLAGSET_FLAGS4:

		ulActorFlags = pActor->flags4;
		break;
	case FLAGSET_FLAGS5:

		ulActorFlags = pActor->flags5;
		break;
	case FLAGSET_FLAGSST:

		ulActorFlags = pActor->ulSTFlags;
		break;
	default:

		return;
	}

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 8 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SETTHINGFLAGS );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, pActor->lNetID );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, ulFlagSet );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, ulActorFlags );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetThingArguments( AActor *pActor, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( pActor == NULL )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 8 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SETTHINGARGUMENTS );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, pActor->lNetID );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, pActor->args[0] );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, pActor->args[1] );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, pActor->args[2] );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, pActor->args[3] );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, pActor->args[4] );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetThingTranslation( AActor *pActor, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( pActor == NULL )
		return;

	// If the actor doesn't have a network ID, we can't tell clients which actor to update.
	if ( pActor->ulNetworkFlags & NETFL_NONETID )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 7 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SETTHINGTRANSLATION );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, pActor->lNetID );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, pActor->Translation );
	}
}

//*****************************************************************************
//

// These are directly pasted from p_acs.cpp. This list needs to be updated whenever
// that changes.
#define APROP_Health		0
#define APROP_Speed			1
#define APROP_Damage		2
#define APROP_Alpha			3
#define APROP_RenderStyle	4
#define APROP_Ambush		10
#define APROP_Invulnerable	11
#define APROP_JumpZ			12	// [GRB]
#define APROP_ChaseGoal		13
#define APROP_Frightened	14
#define APROP_SeeSound		5	// Sounds can only be set, not gotten
#define APROP_AttackSound	6
#define APROP_PainSound		7
#define APROP_DeathSound	8
#define APROP_ActiveSound	9

void SERVERCOMMANDS_SetThingProperty( AActor *pActor, ULONG ulProperty, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;
	ULONG	ulPropertyValue;

	if ( pActor == NULL )
		return;

	// Set one of the actor's properties, depending on what was read in.
	switch ( ulProperty )
	{
	case APROP_Speed:

		ulPropertyValue = pActor->Speed;
		break;
	case APROP_Alpha:

		ulPropertyValue = pActor->alpha;
		break;
	case APROP_RenderStyle:

		ulPropertyValue = pActor->RenderStyle;
		break;
	case APROP_JumpZ:

		if ( pActor->IsKindOf( RUNTIME_CLASS( APlayerPawn )))
			ulPropertyValue = static_cast<APlayerPawn *>( pActor )->JumpZ;
		break;
	default:

		return;
	}

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 7 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SETTHINGPROPERTY );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, pActor->lNetID );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, ulProperty );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, ulPropertyValue );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetThingSound( AActor *pActor, ULONG ulSound, char *pszSound, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( pActor == NULL )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 4 + static_cast<ULONG>(strlen( pszSound )));
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SETTHINGSOUND );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, pActor->lNetID );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, ulSound );
		NETWORK_WriteString( &clients[ulIdx].netbuf, pszSound );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetWeaponAmmoGive( AActor *pActor, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( pActor == NULL )
		return;

	if ( pActor->IsKindOf( RUNTIME_CLASS( AWeapon )) == false )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 7 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SETWEAPONAMMOGIVE );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, pActor->lNetID );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, static_cast<AWeapon *>( pActor )->AmmoGive1 );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, static_cast<AWeapon *>( pActor )->AmmoGive2 );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_ThingIsCorpse( AActor *pActor, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( pActor == NULL )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 4 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_THINGISCORPSE );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, pActor->lNetID );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, pActor->CountsAsKill( ) ? true : false );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_HideThing( AActor *pActor, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( pActor == NULL )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 3 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_HIDETHING );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, pActor->lNetID );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_TeleportThing( AActor *pActor, bool bSourceFog, bool bDestFog, bool bTeleZoom, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( pActor == NULL )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;
	
		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 24 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_TELEPORTTHING );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, pActor->lNetID );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, pActor->x >> FRACBITS );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, pActor->y >> FRACBITS );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, pActor->z >> FRACBITS );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, pActor->momx >> FRACBITS );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, pActor->momy >> FRACBITS );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, pActor->momz >> FRACBITS );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, pActor->reactiontime );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, pActor->angle );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, bSourceFog );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, bDestFog );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, bTeleZoom );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_ThingActivate( AActor *pActor, AActor *pActivator, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( pActor == NULL )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 5 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_THINGACTIVATE );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, pActor->lNetID );
		if ( pActivator )
			NETWORK_WriteShort( &clients[ulIdx].netbuf, pActivator->lNetID );
		else
			NETWORK_WriteShort( &clients[ulIdx].netbuf, -1 );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_ThingDeactivate( AActor *pActor, AActor *pActivator, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( pActor == NULL )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 5 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_THINGDEACTIVATE );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, pActor->lNetID );
		if ( pActivator )
			NETWORK_WriteShort( &clients[ulIdx].netbuf, pActivator->lNetID );
		else
			NETWORK_WriteShort( &clients[ulIdx].netbuf, -1 );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_RespawnThing( AActor *pActor, bool bFog, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( pActor == NULL )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 4 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_RESPAWNTHING );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, pActor->lNetID );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, bFog );
	}
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_Print( char *pszString, ULONG ulPrintLevel, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 2 + (ULONG)strlen( pszString ));
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_PRINT );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, ulPrintLevel );
		NETWORK_WriteString( &clients[ulIdx].netbuf, pszString );
    }
}

//*****************************************************************************
//
void SERVERCOMMANDS_PrintMid( char *pszString, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 1 + (ULONG)strlen( pszString ));
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_PRINTMID );
		NETWORK_WriteString( &clients[ulIdx].netbuf, pszString );
    }
}

//*****************************************************************************
//
void SERVERCOMMANDS_PrintMOTD( char *pszString, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 1 + (ULONG)strlen( pszString ));
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_PRINTMOTD );
		NETWORK_WriteString( &clients[ulIdx].netbuf, pszString );
    }
}

//*****************************************************************************
//
void SERVERCOMMANDS_PrintHUDMessage( char *pszString, float fX, float fY, LONG lHUDWidth, LONG lHUDHeight, LONG lColor, float fHoldTime, char *pszFont, LONG lID, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 22 + (ULONG)strlen( pszString ) + (ULONG)strlen( pszFont ));
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_PRINTHUDMESSAGE );
		NETWORK_WriteString( &clients[ulIdx].netbuf, pszString );
		NETWORK_WriteFloat( &clients[ulIdx].netbuf, fX );
		NETWORK_WriteFloat( &clients[ulIdx].netbuf, fY );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, lHUDWidth );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, lHUDHeight );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, lColor );
		NETWORK_WriteFloat( &clients[ulIdx].netbuf, fHoldTime );
		NETWORK_WriteString( &clients[ulIdx].netbuf, pszFont );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, lID );
    }
}

//*****************************************************************************
//
void SERVERCOMMANDS_PrintHUDMessageFadeOut( char *pszString, float fX, float fY, LONG lHUDWidth, LONG lHUDHeight, LONG lColor, float fHoldTime, float fFadeOutTime, char *pszFont, LONG lID, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 26 + (ULONG)strlen( pszString ) + (ULONG)strlen( pszFont ));
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_PRINTHUDMESSAGEFADEOUT );
		NETWORK_WriteString( &clients[ulIdx].netbuf, pszString );
		NETWORK_WriteFloat( &clients[ulIdx].netbuf, fX );
		NETWORK_WriteFloat( &clients[ulIdx].netbuf, fY );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, lHUDWidth );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, lHUDHeight );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, lColor );
		NETWORK_WriteFloat( &clients[ulIdx].netbuf, fHoldTime );
		NETWORK_WriteFloat( &clients[ulIdx].netbuf, fFadeOutTime );
		NETWORK_WriteString( &clients[ulIdx].netbuf, pszFont );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, lID );
    }
}

//*****************************************************************************
//
void SERVERCOMMANDS_PrintHUDMessageFadeInOut( char *pszString, float fX, float fY, LONG lHUDWidth, LONG lHUDHeight, LONG lColor, float fHoldTime, float fFadeInTime, float fFadeOutTime, char *pszFont, LONG lID, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 30 + (ULONG)strlen( pszString ) + (ULONG)strlen( pszFont ));
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_PRINTHUDMESSAGEFADEINOUT );
		NETWORK_WriteString( &clients[ulIdx].netbuf, pszString );
		NETWORK_WriteFloat( &clients[ulIdx].netbuf, fX );
		NETWORK_WriteFloat( &clients[ulIdx].netbuf, fY );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, lHUDWidth );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, lHUDHeight );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, lColor );
		NETWORK_WriteFloat( &clients[ulIdx].netbuf, fHoldTime );
		NETWORK_WriteFloat( &clients[ulIdx].netbuf, fFadeInTime );
		NETWORK_WriteFloat( &clients[ulIdx].netbuf, fFadeOutTime );
		NETWORK_WriteString( &clients[ulIdx].netbuf, pszFont );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, lID );
    }
}

//*****************************************************************************
//
void SERVERCOMMANDS_PrintHUDMessageTypeOnFadeOut( char *pszString, float fX, float fY, LONG lHUDWidth, LONG lHUDHeight, LONG lColor, float fTypeTime, float fHoldTime, float fFadeOutTime, char *pszFont, LONG lID, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 30 + (ULONG)strlen( pszString ) + (ULONG)strlen( pszFont ));
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_PRINTHUDMESSAGETYPEONFADEOUT );
		NETWORK_WriteString( &clients[ulIdx].netbuf, pszString );
		NETWORK_WriteFloat( &clients[ulIdx].netbuf, fX );
		NETWORK_WriteFloat( &clients[ulIdx].netbuf, fY );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, lHUDWidth );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, lHUDHeight );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, lColor );
		NETWORK_WriteFloat( &clients[ulIdx].netbuf, fTypeTime );
		NETWORK_WriteFloat( &clients[ulIdx].netbuf, fHoldTime );
		NETWORK_WriteFloat( &clients[ulIdx].netbuf, fFadeOutTime );
		NETWORK_WriteString( &clients[ulIdx].netbuf, pszFont );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, lID );
    }
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_SetGameMode( ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 4 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SETGAMEMODE );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, GAME_GetGameType( ));
		NETWORK_WriteByte( &clients[ulIdx].netbuf, instagib );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, buckshot );
    }
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetGameSkill( ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 3 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SETGAMESKILL );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, gameskill );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, botskill );
    }
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetGameDMFlags( ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;
	LONG	lDMFlags;

	lDMFlags = dmflags;
	if ( iwanttousecrouchingeventhoughitsretardedandunnecessaryanditsimplementationishorribleimeanverticallyshrinkingskinscomeonthatsinsanebutwhatevergoaheadandhaveyourcrouching == false )
		lDMFlags |= DF_NO_CROUCH;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 13 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SETGAMEDMFLAGS );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, lDMFlags );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, dmflags2 );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, compatflags );
    }
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetGameModeLimits( ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 12 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SETGAMEMODELIMITS );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, fraglimit );
		NETWORK_WriteFloat( &clients[ulIdx].netbuf, timelimit );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, pointlimit );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, duellimit );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, winlimit );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, wavelimit );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, sv_cheats );
    }
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetGameEndLevelDelay( ULONG ulEndLevelDelay, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 3 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SETGAMEENDLEVELDELAY );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, ulEndLevelDelay );
    }
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetGameModeState( ULONG ulState, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 2 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SETGAMEMODESTATE );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, ulState );
    }
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetDuelNumDuels( ULONG ulNumDuels, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 2 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SETDUELNUMDUELS );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, ulNumDuels );
    }
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetLMSSpectatorSettings( ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 5 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SETLMSSPECTATORSETTINGS );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, lmsspectatorsettings );
    }
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetLMSAllowedWeapons( ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 5 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SETLMSALLOWEDWEAPONS );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, lmsallowedweapons );
    }
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetInvasionNumMonstersLeft( ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 5 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SETINVASIONNUMMONSTERSLEFT );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, INVASION_GetNumMonstersLeft( ));
		NETWORK_WriteShort( &clients[ulIdx].netbuf, INVASION_GetNumArchVilesLeft( ));
    }
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetInvasionWave( ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 2 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SETINVASIONWAVE );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, INVASION_GetCurrentWave( ));
    }
}

//*****************************************************************************
//
void SERVERCOMMANDS_DoPossessionArtifactPickedUp( ULONG ulPlayer, ULONG ulTicks, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( SERVER_IsValidPlayer( ulPlayer ) == false )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 4 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_DOPOSSESSIONARTIFACTPICKEDUP );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, ulPlayer );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, ulTicks );
    }
}

//*****************************************************************************
//
void SERVERCOMMANDS_DoPossessionArtifactDropped( ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 1 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_DOPOSSESSIONARTIFACTDROPPED );
    }
}

//*****************************************************************************
//
void SERVERCOMMANDS_DoGameModeFight( ULONG ulCurrentWave, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 2 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_DOGAMEMODEFIGHT );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, ulCurrentWave );
    }
}

//*****************************************************************************
//
void SERVERCOMMANDS_DoGameModeCountdown( ULONG ulTicks, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 3 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_DOGAMEMODECOUNTDOWN );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, ulTicks );
    }
}

//*****************************************************************************
//
void SERVERCOMMANDS_DoGameModeWinSequence( ULONG ulWinner, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 2 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_DOGAMEMODEWINSEQUENCE );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, ulWinner );
    }
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_SetTeamFrags( ULONG ulTeam, LONG lFrags, bool bAnnounce, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( ulTeam >= NUM_TEAMS )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 5 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SETTEAMFRAGS );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, ulTeam );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, lFrags );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, bAnnounce );
    }
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetTeamScore( ULONG ulTeam, LONG lScore, bool bAnnounce, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( ulTeam >= NUM_TEAMS )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 5 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SETTEAMSCORE );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, ulTeam );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, lScore );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, bAnnounce );
    }
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetTeamWins( ULONG ulTeam, LONG lWins, bool bAnnounce, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( ulTeam >= NUM_TEAMS )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 5 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SETTEAMWINS );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, ulTeam );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, lWins );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, bAnnounce );
    }
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetTeamReturnTicks( ULONG ulTeam, ULONG ulReturnTicks, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	// Allow NUM_TEAMS here, since we could be updating the return ticks of the white flag.
	if ( ulTeam > NUM_TEAMS )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 4 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SETTEAMRETURNTICKS );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, ulTeam );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, ulReturnTicks );
    }
}

//*****************************************************************************
//
void SERVERCOMMANDS_TeamFlagReturned( ULONG ulTeam, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( ulTeam >= NUM_TEAMS )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 2 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_TEAMFLAGRETURNED );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, ulTeam );
    }
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_SpawnMissile( AActor *pMissile, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG		ulIdx;
	const char	*pszName;

	if ( pMissile == NULL )
		return;

	// Some optimization. For some actors that are sent in bunches, to reduce the size,
	// just send some key letter that identifies the actor, instead of the full name.
	if ( stricmp( pMissile->GetClass( )->TypeName.GetChars( ), "PlasmaBall" ) == 0 )
		pszName = "1";
	else
		pszName = pMissile->GetClass( )->TypeName.GetChars( );

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 23 + (ULONG)strlen( pMissile->GetClass( )->TypeName.GetChars( )));
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SPAWNMISSILE );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, pMissile->x >> FRACBITS );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, pMissile->y >> FRACBITS );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, pMissile->z >> FRACBITS );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, pMissile->momx );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, pMissile->momy );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, pMissile->momz );
		NETWORK_WriteString( &clients[ulIdx].netbuf, (char *)pszName );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, pMissile->lNetID );
		if ( pMissile->target )
			NETWORK_WriteShort( &clients[ulIdx].netbuf, pMissile->target->lNetID );
		else
			NETWORK_WriteShort( &clients[ulIdx].netbuf, -1 );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SpawnMissileExact( AActor *pMissile, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG		ulIdx;
	const char	*pszName;

	if ( pMissile == NULL )
		return;

	// Some optimization. For some actors that are sent in bunches, to reduce the size,
	// just send some key letter that identifies the actor, instead of the full name.
	if ( stricmp( pMissile->GetClass( )->TypeName.GetChars( ), "PlasmaBall" ) == 0 )
		pszName = "1";
	else
		pszName = pMissile->GetClass( )->TypeName.GetChars( );

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 29 + (ULONG)strlen( pMissile->GetClass( )->TypeName.GetChars( )));
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SPAWNMISSILEEXACT );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, pMissile->x );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, pMissile->y );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, pMissile->z );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, pMissile->momx );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, pMissile->momy );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, pMissile->momz );
		NETWORK_WriteString( &clients[ulIdx].netbuf, (char *)pszName );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, pMissile->lNetID );
		if ( pMissile->target )
			NETWORK_WriteShort( &clients[ulIdx].netbuf, pMissile->target->lNetID );
		else
			NETWORK_WriteShort( &clients[ulIdx].netbuf, -1 );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_MissileExplode( AActor *pMissile, line_t *pLine, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( pMissile == NULL )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 9 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_MISSILEEXPLODE );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, pMissile->lNetID );
		if ( pLine )
			NETWORK_WriteShort( &clients[ulIdx].netbuf, ULONG( pLine - lines ));
		else
			NETWORK_WriteShort( &clients[ulIdx].netbuf, -1 );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, pMissile->x >> FRACBITS );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, pMissile->y >> FRACBITS );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, pMissile->z >> FRACBITS );
	}
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_WeaponSound( ULONG ulPlayer, char *pszSound, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( SERVER_IsValidPlayer( ulPlayer ) == false )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 2 + (ULONG)strlen( pszSound ));
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_WEAPONSOUND );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, ulPlayer );
		NETWORK_WriteString( &clients[ulIdx].netbuf, pszSound );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_WeaponChange( ULONG ulPlayer, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( SERVER_IsValidPlayer( ulPlayer ) == false )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_WeaponRailgun( AActor *pSource, const FVector3 &Start, const FVector3 &End, LONG lColor1, LONG lColor2, float fMaxDiff, bool bSilent, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	// Evidently, to draw a railgun trail, there must be a source actor.
	if ( pSource == NULL )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 40 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_WEAPONRAILGUN );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, pSource->lNetID );
		NETWORK_WriteFloat( &clients[ulIdx].netbuf, Start.X );
		NETWORK_WriteFloat( &clients[ulIdx].netbuf, Start.Y );
		NETWORK_WriteFloat( &clients[ulIdx].netbuf, Start.Z );
		NETWORK_WriteFloat( &clients[ulIdx].netbuf, End.X );
		NETWORK_WriteFloat( &clients[ulIdx].netbuf, End.Y );
		NETWORK_WriteFloat( &clients[ulIdx].netbuf, End.Z );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, lColor1 );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, lColor2 );
		NETWORK_WriteFloat( &clients[ulIdx].netbuf, fMaxDiff );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, bSilent );
	}
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_SetSectorFloorPlane( ULONG ulSector, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( ulSector >= (ULONG)numsectors )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 5 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SETSECTORFLOORPLANE );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, ulSector );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, sectors[ulSector].floorplane.d >> FRACBITS );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetSectorCeilingPlane( ULONG ulSector, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( ulSector >= (ULONG)numsectors )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 5 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SETSECTORCEILINGPLANE );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, ulSector );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, sectors[ulSector].ceilingplane.d >> FRACBITS );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetSectorLightLevel( ULONG ulSector, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( ulSector >= (ULONG)numsectors )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 4 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SETSECTORLIGHTLEVEL );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, ulSector );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, sectors[ulSector].lightlevel );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetSectorColor( ULONG ulSector, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( ulSector >= (ULONG)numsectors )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 7 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SETSECTORCOLOR );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, ulSector );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, sectors[ulSector].ColorMap->Color.r );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, sectors[ulSector].ColorMap->Color.g );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, sectors[ulSector].ColorMap->Color.b );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, sectors[ulSector].ColorMap->Desaturate );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetSectorFade( ULONG ulSector, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( ulSector >= (ULONG)numsectors )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 6 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SETSECTORFADE );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, ulSector );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, sectors[ulSector].ColorMap->Fade.r );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, sectors[ulSector].ColorMap->Fade.g );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, sectors[ulSector].ColorMap->Fade.b );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetSectorFlat( ULONG ulSector, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( ulSector >= (ULONG)numsectors )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 3 + (ULONG)strlen( TexMan( sectors[ulSector].floorpic )->Name ) + (ULONG)strlen( TexMan( sectors[ulSector].ceilingpic )->Name ));
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SETSECTORFLAT );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, ulSector );
		NETWORK_WriteString( &clients[ulIdx].netbuf, TexMan( sectors[ulSector].ceilingpic )->Name );
		NETWORK_WriteString( &clients[ulIdx].netbuf, TexMan( sectors[ulSector].floorpic )->Name );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetSectorPanning( ULONG ulSector, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( ulSector >= (ULONG)numsectors )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 7 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SETSECTORPANNING );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, ulSector );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, sectors[ulSector].ceiling_xoffs / FRACUNIT );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, sectors[ulSector].ceiling_yoffs / FRACUNIT );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, sectors[ulSector].floor_xoffs / FRACUNIT );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, sectors[ulSector].floor_yoffs / FRACUNIT );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetSectorRotation( ULONG ulSector, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( ulSector >= (ULONG)numsectors )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 7 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SETSECTORROTATION );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, ulSector );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, ( sectors[ulSector].ceiling_angle / ANGLE_1 ));
		NETWORK_WriteShort( &clients[ulIdx].netbuf, ( sectors[ulSector].floor_angle / ANGLE_1 ));
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetSectorScale( ULONG ulSector, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( ulSector >= (ULONG)numsectors )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 11 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SETSECTORSCALE );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, ulSector );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, ( sectors[ulSector].ceiling_xscale / FRACBITS ));
		NETWORK_WriteShort( &clients[ulIdx].netbuf, ( sectors[ulSector].ceiling_yscale / FRACBITS ));
		NETWORK_WriteShort( &clients[ulIdx].netbuf, ( sectors[ulSector].floor_xscale / FRACBITS ));
		NETWORK_WriteShort( &clients[ulIdx].netbuf, ( sectors[ulSector].floor_yscale / FRACBITS ));
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetSectorFriction( ULONG ulSector, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( ulSector >= (ULONG)numsectors )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 11 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SETSECTORFRICTION );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, ulSector );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, sectors[ulSector].friction );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, sectors[ulSector].movefactor );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetSectorAngleYOffset( ULONG ulSector, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( ulSector >= (ULONG)numsectors )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 19 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SETSECTORANGLEYOFFSET );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, ulSector );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, sectors[ulSector].base_ceiling_angle );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, sectors[ulSector].base_ceiling_yoffs );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, sectors[ulSector].base_floor_angle );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, sectors[ulSector].base_floor_yoffs );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetSectorGravity( ULONG ulSector, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( ulSector >= (ULONG)numsectors )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 7 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SETSECTORGRAVITY );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, ulSector );
		NETWORK_WriteFloat( &clients[ulIdx].netbuf, sectors[ulSector].gravity );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_StopSectorLightEffect( ULONG ulSector, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( ulSector >= (ULONG)numsectors )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 3 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_STOPSECTORLIGHTEFFECT );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, ulSector );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_DestroyAllSectorMovers( ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 1 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_DESTROYALLSECTORMOVERS );
	}
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_DoSectorLightFireFlicker( ULONG ulSector, LONG lMaxLight, LONG lMinLight, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( ulSector >= (ULONG)numsectors )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 5 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_DOSECTORLIGHTFIREFLICKER );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, ulSector );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, lMaxLight );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, lMinLight );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_DoSectorLightFlicker( ULONG ulSector, LONG lMaxLight, LONG lMinLight, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( ulSector >= (ULONG)numsectors )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 3 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_DOSECTORLIGHTFLICKER );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, ulSector );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, lMaxLight );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, lMinLight );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_DoSectorLightLightFlash( ULONG ulSector, LONG lMaxLight, LONG lMinLight, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( ulSector >= (ULONG)numsectors )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 5 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_DOSECTORLIGHTLIGHTFLASH );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, ulSector );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, lMaxLight );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, lMinLight );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_DoSectorLightStrobe( ULONG ulSector, LONG lDarkTime, LONG lBrightTime, LONG lMaxLight, LONG lMinLight, LONG lCount, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( ulSector >= (ULONG)numsectors )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 10 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_DOSECTORLIGHTSTROBE );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, ulSector );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, lDarkTime );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, lBrightTime );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, lMaxLight );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, lMinLight );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, lCount );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_DoSectorLightGlow( ULONG ulSector, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( ulSector >= (ULONG)numsectors )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 3 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_DOSECTORLIGHTGLOW );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, ulSector );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_DoSectorLightGlow2( ULONG ulSector, LONG lStart, LONG lEnd, LONG lTics, LONG lMaxTics, bool bOneShot, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( ulSector >= (ULONG)numsectors )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 10 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_DOSECTORLIGHTGLOW2 );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, ulSector );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, lStart );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, lEnd );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, lTics );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, lMaxTics );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, bOneShot );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_DoSectorLightPhased( ULONG ulSector, LONG lBaseLevel, LONG lPhase, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( ulSector >= (ULONG)numsectors )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 5 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_DOSECTORLIGHTPHASED );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, ulSector );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, lBaseLevel );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, lPhase );
	}
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_SetLineAlpha( ULONG ulLine, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( ulLine >= (ULONG)numlines )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 4 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SETLINEALPHA );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, ulLine );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, lines[ulLine].alpha );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetLineTexture( ULONG ulLine, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( ulLine >= (ULONG)numlines )
		return;

	// No changed textures to update.
	if ( lines[ulLine].ulTexChangeFlags == 0 )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		if ( lines[ulLine].ulTexChangeFlags & TEXCHANGE_FRONTTOP )
		{
			NETWORK_CheckBuffer( ulIdx, 5 + 8 );
			NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SETLINETEXTURE );
			NETWORK_WriteShort( &clients[ulIdx].netbuf, ulLine );
			NETWORK_WriteString( &clients[ulIdx].netbuf, sides[lines[ulLine].sidenum[0]].toptexture ? TexMan[sides[lines[ulLine].sidenum[0]].toptexture]->Name : "-" );
			NETWORK_WriteByte( &clients[ulIdx].netbuf, 0 );
			NETWORK_WriteByte( &clients[ulIdx].netbuf, 0 );
		}

		if ( lines[ulLine].ulTexChangeFlags & TEXCHANGE_FRONTMEDIUM )
		{
			NETWORK_CheckBuffer( ulIdx, 5 + 8 );
			NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SETLINETEXTURE );
			NETWORK_WriteShort( &clients[ulIdx].netbuf, ulLine );
			NETWORK_WriteString( &clients[ulIdx].netbuf, sides[lines[ulLine].sidenum[0]].midtexture ? TexMan[sides[lines[ulLine].sidenum[0]].midtexture]->Name : "-" );
			NETWORK_WriteByte( &clients[ulIdx].netbuf, 0 );
			NETWORK_WriteByte( &clients[ulIdx].netbuf, 1 );
		}

		if ( lines[ulLine].ulTexChangeFlags & TEXCHANGE_FRONTBOTTOM )
		{
			NETWORK_CheckBuffer( ulIdx, 5 + 8 );
			NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SETLINETEXTURE );
			NETWORK_WriteShort( &clients[ulIdx].netbuf, ulLine );
			NETWORK_WriteString( &clients[ulIdx].netbuf, sides[lines[ulLine].sidenum[0]].bottomtexture ? TexMan[sides[lines[ulLine].sidenum[0]].bottomtexture]->Name : "-" );
			NETWORK_WriteByte( &clients[ulIdx].netbuf, 0 );
			NETWORK_WriteByte( &clients[ulIdx].netbuf, 2 );
		}

		if (( lines[ulLine].sidenum[1] == NO_INDEX ) || ( lines[ulLine].sidenum[1] >= (ULONG)numsides ))
			continue;

		if ( lines[ulLine].ulTexChangeFlags & TEXCHANGE_BACKTOP )
		{
			NETWORK_CheckBuffer( ulIdx, 5 + 8 );
			NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SETLINETEXTURE );
			NETWORK_WriteShort( &clients[ulIdx].netbuf, ulLine );
			NETWORK_WriteString( &clients[ulIdx].netbuf, sides[lines[ulLine].sidenum[1]].toptexture ? TexMan[sides[lines[ulLine].sidenum[1]].toptexture]->Name : "-" );
			NETWORK_WriteByte( &clients[ulIdx].netbuf, 1 );
			NETWORK_WriteByte( &clients[ulIdx].netbuf, 0 );
		}

		if ( lines[ulLine].ulTexChangeFlags & TEXCHANGE_BACKMEDIUM )
		{
			NETWORK_CheckBuffer( ulIdx, 5 + 8 );
			NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SETLINETEXTURE );
			NETWORK_WriteShort( &clients[ulIdx].netbuf, ulLine );
			NETWORK_WriteString( &clients[ulIdx].netbuf, sides[lines[ulLine].sidenum[1]].midtexture ? TexMan[sides[lines[ulLine].sidenum[1]].midtexture]->Name : "-" );
			NETWORK_WriteByte( &clients[ulIdx].netbuf, 1 );
			NETWORK_WriteByte( &clients[ulIdx].netbuf, 1 );
		}

		if ( lines[ulLine].ulTexChangeFlags & TEXCHANGE_BACKBOTTOM )
		{
			NETWORK_CheckBuffer( ulIdx, 5 + 8 );
			NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SETLINETEXTURE );
			NETWORK_WriteShort( &clients[ulIdx].netbuf, ulLine );
			NETWORK_WriteString( &clients[ulIdx].netbuf, sides[lines[ulLine].sidenum[1]].bottomtexture ? TexMan[sides[lines[ulLine].sidenum[1]].bottomtexture]->Name : "-" );
			NETWORK_WriteByte( &clients[ulIdx].netbuf, 1 );
			NETWORK_WriteByte( &clients[ulIdx].netbuf, 2 );
		}
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetLineBlocking( ULONG ulLine, ULONG ulPlayerExtra, ULONG ulFlags )
{
	LONG	lBlockFlags;
	ULONG	ulIdx;

	if ( ulLine >= (ULONG)numlines )
		return;

	// I bet there's a better way to do this... alas, my knowledge of bits is not
	// wonderful.
	lBlockFlags = 0;
	if ( lines[ulLine].flags & ML_BLOCKING )
		lBlockFlags |= ML_BLOCKING;
	if ( lines[ulLine].flags & ML_BLOCKEVERYTHING )
		lBlockFlags |= ML_BLOCKEVERYTHING;
	if ( lines[ulLine].flags & ML_RAILING )
		lBlockFlags |= ML_RAILING;
	if ( lines[ulLine].flags & ML_BLOCKPLAYERS )
		lBlockFlags |= ML_BLOCKPLAYERS;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 7 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SETLINEBLOCKING );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, ulLine );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, lBlockFlags );
	}
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_Sound( LONG lChannel, char *pszSound, LONG lVolume, LONG lAttenuation, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 4 + (ULONG)strlen( pszSound ));
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SOUND );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, lChannel );
		NETWORK_WriteString( &clients[ulIdx].netbuf, pszSound );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, lVolume );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, lAttenuation );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SoundID( LONG lX, LONG lY, LONG lChannel, LONG lSoundID, LONG lVolume, LONG lAttenuation, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 10 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SOUNDID );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, lX >> FRACBITS );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, lY >> FRACBITS );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, lSoundID );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, lChannel );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, lVolume );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, lAttenuation );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SoundActor( AActor *pActor, LONG lChannel, char *pszSound, LONG lVolume, LONG lAttenuation, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( pActor == NULL )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 6 + (ULONG)strlen( pszSound ));
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SOUNDACTOR );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, pActor->lNetID );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, lChannel );
		NETWORK_WriteString( &clients[ulIdx].netbuf, pszSound );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, lVolume );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, lAttenuation );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SoundIDActor( AActor *pActor, LONG lChannel, LONG lSoundID, LONG lVolume, LONG lAttenuation, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( pActor == NULL )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 8 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SOUNDIDACTOR );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, pActor->lNetID );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, lChannel );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, lSoundID );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, lVolume );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, lAttenuation );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SoundPoint( LONG lX, LONG lY, LONG lChannel, char *pszSound, LONG lVolume, LONG lAttenuation, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 8 + (ULONG)strlen( pszSound ));
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SOUNDPOINT );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, lX >> FRACBITS );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, lY >> FRACBITS );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, lChannel );
		NETWORK_WriteString( &clients[ulIdx].netbuf, pszSound );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, lVolume );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, lAttenuation );
	}
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_StartSectorSequence( sector_t *pSector, char *pszSequence, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;
	LONG	lSectorID;

	lSectorID = LONG( pSector - sectors );
	if (( lSectorID < 0 ) || ( lSectorID >= numsectors ))
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 3 + (ULONG)strlen( pszSequence ));
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_STARTSECTORSEQUENCE );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, lSectorID );
		NETWORK_WriteString( &clients[ulIdx].netbuf, pszSequence );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_StopSectorSequence( sector_t *pSector, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;
	LONG	lSectorID;

	lSectorID = LONG( pSector - sectors );
	if (( lSectorID < 0 ) || ( lSectorID >= numsectors ))
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 3 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_STOPSECTORSEQUENCE );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, lSectorID );
	}
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_CallVote( ULONG ulPlayer, char *pszCommand, char *pszParameters, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( SERVER_IsValidPlayer( ulPlayer ) == false )
		return;

	if (( pszCommand == NULL ) || ( pszParameters == NULL ))
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 2 + (ULONG)strlen( pszCommand ) + (ULONG)strlen( pszParameters ));
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_CALLVOTE );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, ulPlayer );
		NETWORK_WriteString( &clients[ulIdx].netbuf, pszCommand );
		NETWORK_WriteString( &clients[ulIdx].netbuf, pszParameters );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_PlayerVote( ULONG ulPlayer, bool bVoteYes, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( SERVER_IsValidPlayer( ulPlayer ) == false )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 3 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_PLAYERVOTE );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, ulPlayer );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, bVoteYes );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_VoteEnded( bool bVotePassed, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 2 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_VOTEENDED );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, bVotePassed );
	}
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_MapLoad( ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 1 + (ULONG)strlen( level.mapname ));
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_MAPLOAD );
		NETWORK_WriteString( &clients[ulIdx].netbuf, level.mapname );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_MapNew( char *pszMapName, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 1 + (ULONG)strlen( pszMapName ));
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_MAPNEW );
		NETWORK_WriteString( &clients[ulIdx].netbuf, pszMapName );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_MapExit( LONG lPosition, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 2 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_MAPEXIT );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, lPosition );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_MapAuthenticate( char *pszMapName, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 1 + (ULONG)strlen( pszMapName ));
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_MAPAUTHENTICATE );
		NETWORK_WriteString( &clients[ulIdx].netbuf, pszMapName );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetMapTime( ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 5 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SETMAPTIME );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, level.maptime );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetMapNumKilledMonsters( ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 3 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SETMAPNUMKILLEDMONSTERS );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, level.killed_monsters );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetMapNumFoundItems( ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 3 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SETMAPNUMFOUNDITEMS );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, level.found_items );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetMapNumFoundSecrets( ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 3 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SETMAPNUMFOUNDSECRETS );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, level.found_secrets );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetMapNumTotalMonsters( ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 3 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SETMAPNUMTOTALMONSTERS );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, level.total_monsters );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetMapNumTotalItems( ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 3 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SETMAPNUMTOTALITEMS );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, level.total_items );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetMapMusic( char *pszMusic, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 1 + (ULONG)strlen( pszMusic ));
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SETMAPMUSIC );
		NETWORK_WriteString( &clients[ulIdx].netbuf, pszMusic );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetMapSky( ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 1 + (ULONG)strlen( level.skypic1 ) + (ULONG)strlen( level.skypic2 ));
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SETMAPSKY );
		NETWORK_WriteString( &clients[ulIdx].netbuf, level.skypic1 );
		NETWORK_WriteString( &clients[ulIdx].netbuf, level.skypic2 );
	}
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_GiveInventory( ULONG ulPlayer, AInventory *pInventory, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( SERVER_IsValidPlayer( ulPlayer ) == false )
		return;

	if ( pInventory == NULL )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 4 + (ULONG)strlen( pInventory->GetClass( )->TypeName.GetChars( )));
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_GIVEINVENTORY );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, ulPlayer );
		NETWORK_WriteString( &clients[ulIdx].netbuf, (char *)pInventory->GetClass( )->TypeName.GetChars( ));
		NETWORK_WriteShort( &clients[ulIdx].netbuf, pInventory->Amount );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_TakeInventory( ULONG ulPlayer, char *pszClassName, ULONG ulAmount, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( SERVER_IsValidPlayer( ulPlayer ) == false )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 4 + (ULONG)strlen( pszClassName ));
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_TAKEINVENTORY );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, ulPlayer );
		NETWORK_WriteString( &clients[ulIdx].netbuf, pszClassName );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, ulAmount );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_GivePowerup( ULONG ulPlayer, APowerup *pPowerup, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( SERVER_IsValidPlayer( ulPlayer ) == false )
		return;

	if ( pPowerup == NULL )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 4 + (ULONG)strlen( pPowerup->GetClass( )->TypeName.GetChars( )));
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_GIVEPOWERUP );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, ulPlayer );
		NETWORK_WriteString( &clients[ulIdx].netbuf, (char *)pPowerup->GetClass( )->TypeName.GetChars( ));
		// Can we have multiple amounts of a powerup? Probably not, but I'll be safe for now.
		NETWORK_WriteShort( &clients[ulIdx].netbuf, pPowerup->Amount );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, pPowerup->EffectTics );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_DoInventoryPickup( ULONG ulPlayer, char *pszClassName, char *pszPickupMessage, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;
	LONG	lLength;

	lLength = 0;
	if ( pszClassName )
		lLength += (LONG)strlen( pszClassName );
	if ( pszPickupMessage )
		lLength += (LONG)strlen( pszPickupMessage );

	if ( SERVER_IsValidPlayer( ulPlayer ) == false )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 2 + lLength );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_DOINVENTORYPICKUP );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, ulPlayer );
		NETWORK_WriteString( &clients[ulIdx].netbuf, pszClassName );
		NETWORK_WriteString( &clients[ulIdx].netbuf, pszPickupMessage );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_DestroyAllInventory( ULONG ulPlayer, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 2 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_DESTROYALLINVENTORY );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, ulPlayer );
	}
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_DoDoor( sector_t *pSector, LONG lSpeed, LONG lDirection, LONG lLightTag, LONG lID, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;
	LONG	lSectorID;

	lSectorID = LONG( pSector - sectors );
	if (( lSectorID < 0 ) || ( lSectorID >= numsectors ))
		return;

	// Since we still want to send direction as a byte, but -1 can't be represented in byte
	// form, adjust the value into something that can be represented.
	lDirection = SERVER_AdjustDoorDirection( lDirection );
	if ( lDirection == INT_MIN )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 12 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_DODOOR );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, lSectorID );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, lSpeed );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, lDirection );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, lLightTag );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, lID );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_DestroyDoor( LONG lID, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 3 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_DESTROYDOOR );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, lID );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_ChangeDoorDirection( LONG lID, LONG lDirection, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	// Since we still want to send direction as a byte, but -1 can't be represented in byte
	// form, adjust the value into something that can be represented.
	lDirection = SERVER_AdjustDoorDirection( lDirection );
	if ( lDirection == INT_MIN )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 4 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_CHANGEDOORDIRECTION );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, lID );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, lDirection );
	}
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_DoFloor( DFloor::EFloor Type, sector_t *pSector, LONG lDirection, LONG lSpeed, LONG lFloorDestDist, LONG lID, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;
	LONG	lSectorID;

	lSectorID = LONG( pSector - sectors );
	if (( lSectorID < 0 ) || ( lSectorID >= numsectors ))
		return;

	// Since we still want to send direction as a byte, but -1 can't be represented in byte
	// form, adjust the value into something that can be represented.
	lDirection = SERVER_AdjustFloorDirection( lDirection );
	if ( lDirection == INT_MIN )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 15 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_DOFLOOR );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, (ULONG)Type );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, lSectorID );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, lDirection );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, lSpeed );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, lFloorDestDist );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, lID );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_DestroyFloor( LONG lID, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 3 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_DESTROYFLOOR );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, lID );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_ChangeFloorDirection( LONG lID, LONG lDirection, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	// Since we still want to send direction as a byte, but -1 can't be represented in byte
	// form, adjust the value into something that can be represented.
	lDirection = SERVER_AdjustFloorDirection( lDirection );
	if ( lDirection == INT_MIN )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 4 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_CHANGEFLOORDIRECTION );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, lID );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, lDirection );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_ChangeFloorType( LONG lID, DFloor::EFloor Type, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 4 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_CHANGEFLOORTYPE );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, lID );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, (ULONG)Type );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_ChangeFloorDestDist( LONG lID, LONG lFloorDestDist, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 7 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_CHANGEFLOORDESTDIST );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, lID );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, lFloorDestDist );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_StartFloorSound( LONG lID, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 3 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_STARTFLOORSOUND );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, lID );
	}
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_DoCeiling( DCeiling::ECeiling Type, sector_t *pSector, LONG lDirection, LONG lBottomHeight, LONG lTopHeight, LONG lSpeed, LONG lCrush, bool bSilent, LONG lID, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;
	LONG	lSectorID;

	lSectorID = LONG( pSector - sectors );
	if (( lSectorID < 0 ) || ( lSectorID >= numsectors ))
		return;

	// Since we still want to send direction as a byte, but -1 can't be represented in byte
	// form, adjust the value into something that can be represented.
	lDirection = SERVER_AdjustCeilingDirection( lDirection );
	if ( lDirection == INT_MIN )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 22 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_DOCEILING );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, (ULONG)Type );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, lSectorID );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, lDirection );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, lBottomHeight );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, lTopHeight );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, lSpeed );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, ( lCrush == 1 ));
		NETWORK_WriteByte( &clients[ulIdx].netbuf, bSilent );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, lID );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_DestroyCeiling( LONG lID, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 3 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_DESTROYCEILING );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, lID );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_ChangeCeilingDirection( LONG lID, LONG lDirection, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	// Since we still want to send direction as a byte, but -1 can't be represented in byte
	// form, adjust the value into something that can be represented.
	lDirection = SERVER_AdjustCeilingDirection( lDirection );
	if ( lDirection == INT_MIN )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 4 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_CHANGECEILINGDIRECTION );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, lID );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, lDirection );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_ChangeCeilingSpeed( LONG lID, LONG lSpeed, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 7 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_CHANGECEILINGSPEED );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, lID );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, lSpeed );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_PlayCeilingSound( LONG lID, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 3 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_PLAYCEILINGSOUND );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, lID );
	}
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_DoPlat( DPlat::EPlatType Type, sector_t *pSector, DPlat::EPlatState Status, LONG lHigh, LONG lLow, LONG lSpeed, LONG lID, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;
	LONG	lSectorID;

	lSectorID = LONG( pSector - sectors );
	if (( lSectorID < 0 ) || ( lSectorID >= numsectors ))
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 19 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_DOPLAT );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, (ULONG)Type );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, lSectorID );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, (ULONG)Status );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, lHigh );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, lLow );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, lSpeed );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, lID );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_DestroyPlat( LONG lID, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 3 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_DESTROYPLAT );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, lID );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_ChangePlatStatus( LONG lID, DPlat::EPlatState Status, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 4 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_CHANGEPLATSTATUS );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, lID );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, (ULONG)Status );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_PlayPlatSound( LONG lID, LONG lSound, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 4 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_PLAYPLATSOUND );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, lID );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, lSound );
	}
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_DoElevator( DElevator::EElevator Type, sector_t *pSector, LONG lSpeed, LONG lDirection, LONG lFloorDestDist, LONG lCeilingDestDist, LONG lID, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;
	LONG	lSectorID;

	lSectorID = LONG( pSector - sectors );
	if (( lSectorID < 0 ) || ( lSectorID >= numsectors ))
		return;

	// Since we still want to send direction as a byte, but -1 can't be represented in byte
	// form, adjust the value into something that can be represented.
	lDirection = SERVER_AdjustElevatorDirection( lDirection );
	if ( lDirection == INT_MIN )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 13 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_DOELEVATOR );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, Type );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, lSectorID );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, lSpeed );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, lDirection );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, lFloorDestDist );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, lCeilingDestDist );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, lID );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_DestroyElevator( LONG lID, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 3 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_DESTROYELEVATOR );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, lID );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_StartElevatorSound( LONG lID, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 3 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_STARTELEVATORSOUND );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, lID );
	}
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_DoPillar( DPillar::EPillar Type, sector_t *pSector, LONG lFloorSpeed, LONG lCeilingSpeed, LONG lFloorTarget, LONG lCeilingTarget, LONG lID, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;
	LONG	lSectorID;

	lSectorID = LONG( pSector - sectors );
	if (( lSectorID < 0 ) || ( lSectorID >= numsectors ))
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 22 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_DOPILLAR );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, Type );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, lSectorID );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, lFloorSpeed );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, lCeilingSpeed );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, lFloorTarget );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, lCeilingTarget );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, lID );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_DestroyPillar( LONG lID, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 3 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_DESTROYPILLAR );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, lID );
	}
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_DoWaggle( bool bCeiling, sector_t *pSector, LONG lOriginalDistance, LONG lAccumulator, LONG lAccelerationDelta, LONG lTargetScale, LONG lScale, LONG lScaleDelta, LONG lTicker, LONG lState, LONG lID, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;
	LONG	lSectorID;

	lSectorID = LONG( pSector - sectors );
	if (( lSectorID < 0 ) || ( lSectorID >= numsectors ))
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 35 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_DOWAGGLE );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, !!bCeiling );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, lSectorID );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, lOriginalDistance );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, lAccumulator );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, lAccelerationDelta );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, lTargetScale );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, lScale );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, lScaleDelta );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, lTicker );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, lState );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, lID );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_DestroyWaggle( LONG lID, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 3 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_DESTROYWAGGLE );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, lID );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_UpdateWaggle( LONG lID, LONG lAccumulator, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 7 );
		NETWORK_WriteHeader( &clients[ulIdx].UnreliablePacketBuffer, SVC_UPDATEWAGGLE );
		NETWORK_WriteShort( &clients[ulIdx].UnreliablePacketBuffer, lID );
		NETWORK_WriteLong( &clients[ulIdx].UnreliablePacketBuffer, lAccumulator );
	}
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_DoRotatePoly( LONG lSpeed, LONG lPolyNum, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 7 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_DOROTATEPOLY );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, lSpeed );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, lPolyNum );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_DestroyRotatePoly( LONG lPolyNum, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 3 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_DESTROYROTATEPOLY );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, lPolyNum );
	}
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_DoMovePoly( LONG lXSpeed, LONG lYSpeed, LONG lPolyNum, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 11 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_DOMOVEPOLY );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, lXSpeed );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, lYSpeed );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, lPolyNum );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_DestroyMovePoly( LONG lPolyNum, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 3 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_DESTROYMOVEPOLY );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, lPolyNum );
	}
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_DoPolyDoor( LONG lType, LONG lXSpeed, LONG lYSpeed, LONG lSpeed, LONG lPolyNum, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 16 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_DOPOLYDOOR );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, lType );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, lXSpeed );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, lYSpeed );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, lSpeed );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, lPolyNum );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_DestroyPolyDoor( LONG lPolyNum, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 3 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_DESTROYPOLYDOOR );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, lPolyNum );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPolyDoorSpeedPosition( LONG lPolyNum, LONG lXSpeed, LONG lYSpeed, LONG lX, LONG lY, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 19 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SETPOLYDOORSPEEDPOSITION );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, lPolyNum );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, lXSpeed );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, lYSpeed );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, lX );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, lY );
	}
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_PlayPolyobjSound( LONG lPolyNum, NETWORK_POLYOBJSOUND_e Sound, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 4 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_PLAYPOLYOBJSOUND );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, lPolyNum );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, (ULONG)Sound );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPolyobjPosition( LONG lPolyNum, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG		ulIdx;
	polyobj_t	*pPoly;

	pPoly = GetPolyobj( lPolyNum );
	if ( pPoly == NULL )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 11 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SETPOLYOBJPOSITION );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, lPolyNum );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, pPoly->startSpot[0] );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, pPoly->startSpot[1] );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPolyobjRotation( LONG lPolyNum, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG		ulIdx;
	polyobj_t	*pPoly;

	pPoly = GetPolyobj( lPolyNum );
	if ( pPoly == NULL )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 7 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SETPOLYOBJROTATION );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, lPolyNum );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, pPoly->angle );
	}
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_Earthquake( AActor *pCenter, LONG lIntensity, LONG lDuration, LONG lTemorRadius, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( pCenter == NULL )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 6 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_EARTHQUAKE );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, pCenter->lNetID );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, lIntensity );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, lDuration );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, lTemorRadius );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetQueuePosition( ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;
	LONG	lPosition;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		lPosition = JOINQUEUE_GetPositionInLine( ulIdx );
		if ( lPosition != -1 )
		{
			NETWORK_CheckBuffer( ulIdx, 2 );
			NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SETQUEUEPOSITION );
			NETWORK_WriteByte( &clients[ulIdx].netbuf, lPosition );
		}
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_DoScroller( LONG lType, LONG lXSpeed, LONG lYSpeed, LONG lAffectee, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 8 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_DOSCROLLER );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, lType );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, lXSpeed );
		NETWORK_WriteLong( &clients[ulIdx].netbuf, lYSpeed );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, lAffectee );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_GenericCheat( ULONG ulPlayer, ULONG ulCheat, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( SERVER_IsValidPlayer( ulPlayer ) == false )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 3 );
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_GENERICCHEAT );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, ulPlayer );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, ulCheat );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetCameraToTexture( AActor *pCamera, char *pszTexture, LONG lFOV, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if (( pCamera == NULL ) ||
		( pszTexture == NULL ))
	{
		return;
	}

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		NETWORK_CheckBuffer( ulIdx, 3 + (ULONG)strlen( pszTexture ));
		NETWORK_WriteHeader( &clients[ulIdx].netbuf, SVC_SETCAMERATOTEXTURE );
		NETWORK_WriteShort( &clients[ulIdx].netbuf, pCamera->lNetID );
		NETWORK_WriteString( &clients[ulIdx].netbuf, pszTexture );
		NETWORK_WriteByte( &clients[ulIdx].netbuf, lFOV );
	}
}
