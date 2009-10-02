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
#include "gamemode.h"
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
#include "survival.h"
#include "vectors.h"
#include "v_palette.h"
#include "r_translate.h"
#include "domination.h"

CVAR (Bool, sv_showwarnings, false, CVAR_GLOBALCONFIG|CVAR_ARCHIVE)

// :((((((
FPolyObj	*GetPolyobj( int polyNum );

//*****************************************************************************
//	FUNCTIONS

// [BB] Check if the actor has a valid net ID. Returns true if it does, returns false and prints a warning if not.
bool EnsureActorHasNetID( AActor *pActor )
{
	if ( pActor == NULL )
		return false;

	if ( pActor->lNetID == -1 )
	{
		if ( sv_showwarnings )
			Printf ( "Warning: Actor %s doesn't have a netID and therefore can't be manipulated online!\n", pActor->GetClass()->TypeName.GetChars() );
		return false;
	}
	else
		return true;
}

//*****************************************************************************
//
// [BB] Check if the actor already was updated during this tic, and alters ulBits accorindly.
void RemoveUnnecessaryPositionUpdateFlags( AActor *pActor, ULONG &ulBits )
{
	if ( (pActor->lastNetXUpdateTic == gametic) && (pActor->lastX == pActor->x) )
		ulBits  &= ~CM_X;
	if ( (pActor->lastNetYUpdateTic == gametic) && (pActor->lastY == pActor->y) )
		ulBits  &= ~CM_Y;
	if ( (pActor->lastNetZUpdateTic == gametic) && (pActor->lastZ == pActor->z) )
		ulBits  &= ~CM_Z;
	if ( (pActor->lastNetMomXUpdateTic == gametic) && (pActor->lastMomx == pActor->momx) )
		ulBits  &= ~CM_MOMX;
	if ( (pActor->lastNetMomYUpdateTic == gametic) && (pActor->lastMomy == pActor->momy) )
		ulBits  &= ~CM_MOMY;
	if ( (pActor->lastNetMomZUpdateTic == gametic) && (pActor->lastMomz == pActor->momz) )
		ulBits  &= ~CM_MOMZ;
	if ( (pActor->lastNetMovedirUpdateTic == gametic) && (pActor->lastMovedir == pActor->movedir) )
		ulBits  &= ~CM_MOVEDIR;
}


//*****************************************************************************
//
// [BB] Try to find the state label and the correspoding offset belonging to the target state.
void FindStateLabelAndOffset( const PClass *pClass, FState *pState, FString &stateLabel, LONG &lOffset )
{
	stateLabel = "";
	lOffset = 0;
	FState *pCompareState;

	// Begin searching through the actor's state labels to find the state that corresponds
	// to the given state.
	for ( ULONG ulIdx = 0; ulIdx < (ULONG)pClass->ActorInfo->StateList->NumLabels; ulIdx++ )
	{
		if ( stateLabel.IsNotEmpty() )
			break;

		// See if any of the states in this label match the given state.
		lOffset = 0;
		pCompareState = pClass->ActorInfo->StateList->Labels[ulIdx].State;
		while ( pCompareState )
		{
			if ( pState == pCompareState )
			{
				stateLabel = pClass->ActorInfo->StateList->Labels[ulIdx].Label.GetChars( );
				break;
			}

			if ( pCompareState->GetNextState( ) != pCompareState + 1 )
				break;

			lOffset++;
			pCompareState = pCompareState->GetNextState( );
		}
	}
}

//*****************************************************************************
//
// [BB] Mark the actor as updated according to ulBits.
void ActorNetPositionUpdated( AActor *pActor, ULONG &ulBits )
{
	if ( ulBits & CM_X )
	{
		pActor->lastNetXUpdateTic = gametic;
		pActor->lastX = pActor->x;
	}
	if ( ulBits & CM_Y )
	{
		pActor->lastNetYUpdateTic = gametic;
		pActor->lastY = pActor->y;
	}
	if ( ulBits & CM_Z )
	{
		pActor->lastNetZUpdateTic = gametic;
		pActor->lastZ = pActor->z;
	}
	if ( ulBits & CM_MOMX )
	{
		pActor->lastNetMomXUpdateTic = gametic;
		pActor->lastMomx = pActor->momx;
	}
	if ( ulBits & CM_MOMY )
	{
		pActor->lastNetMomYUpdateTic = gametic;
		pActor->lastMomy = pActor->momy;
	}
	if ( ulBits & CM_MOMZ )
	{
		pActor->lastNetMomZUpdateTic = gametic;
		pActor->lastMomz = pActor->momz;
	}
	if ( ulBits & CM_MOVEDIR )
	{
		pActor->lastNetMovedirUpdateTic = gametic;
		pActor->lastMovedir = pActor->movedir;
	}
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

		SERVER_CheckClientBuffer( ulIdx, 5, false );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->UnreliablePacketBuffer.ByteStream, SVC_PING );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->UnreliablePacketBuffer.ByteStream, ulTime );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_Nothing( ULONG ulPlayer )
{
	if ( SERVER_IsValidClient( ulPlayer ) == false )
		return;

	SERVER_CheckClientBuffer( ulPlayer, 1, false );
	NETWORK_WriteHeader( &SERVER_GetClient( ulPlayer )->UnreliablePacketBuffer.ByteStream, SVC_NOTHING );
}

//*****************************************************************************
//
void SERVERCOMMANDS_BeginSnapshot( ULONG ulPlayer )
{
	if ( SERVER_IsValidClient( ulPlayer ) == false )
		return;

	SERVER_CheckClientBuffer( ulPlayer, 1, true );
	NETWORK_WriteHeader( &SERVER_GetClient( ulPlayer )->PacketBuffer.ByteStream, SVC_BEGINSNAPSHOT );
}

//*****************************************************************************
//
void SERVERCOMMANDS_EndSnapshot( ULONG ulPlayer )
{
	if ( SERVER_IsValidClient( ulPlayer ) == false )
		return;

	SERVER_CheckClientBuffer( ulPlayer, 1, true );
	NETWORK_WriteHeader( &SERVER_GetClient( ulPlayer )->PacketBuffer.ByteStream, SVC_ENDSNAPSHOT );
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_SpawnPlayer( ULONG ulPlayer, LONG lPlayerState, ULONG ulPlayerExtra, ULONG ulFlags, bool bMorph )
{
	ULONG	ulIdx;

	if ( SERVER_IsValidPlayer( ulPlayer ) == false )
		return;

	USHORT usActorNetworkIndex = 0;

	if ( players[ulPlayer].mo )
		usActorNetworkIndex = players[ulPlayer].mo->GetClass( )->getActorNetworkIndex();

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue; 

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		if ( bMorph )
		{
			SERVER_CheckClientBuffer( ulIdx, 28, true );
			NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SPAWNMORPHPLAYER );
		}
		else
		{
			SERVER_CheckClientBuffer( ulIdx, 26, true );
			NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SPAWNPLAYER );
		}
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulPlayer );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lPlayerState );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, players[ulPlayer].bIsBot );
		// Do we really need to send this? Shouldn't it always be PST_LIVE?
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, players[ulPlayer].playerstate );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, players[ulPlayer].bSpectating );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, players[ulPlayer].bDeadSpectator );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, players[ulPlayer].mo->lNetID );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, players[ulPlayer].mo->angle );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, players[ulPlayer].mo->x );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, players[ulPlayer].mo->y );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, players[ulPlayer].mo->z );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, players[ulPlayer].CurrentPlayerClass );
		//NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, players[ulPlayer].userinfo.PlayerClass );
		if ( bMorph )
			NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, usActorNetworkIndex );
	}
	// [BB]: Inform the player about its health, otherwise it won't be displayed properly.
	// The armor display is handled in SERVER_ResetInventory.
	SERVERCOMMANDS_SetPlayerHealth( ulPlayer, ulPlayer, SVCF_ONLYTHISCLIENT );

	// [BB]: If the player still has any cheats activated from the last level, tell
	// him about it. Not doing this leads for example to jerky movement on client side
	// in case of NOCLIP.
	// We don't have to tell about CF_NOTARGET, since it's only used on the server.
	if( players[ulPlayer].cheats && players[ulPlayer].cheats != CF_NOTARGET )
		SERVERCOMMANDS_SetPlayerCheats( ulPlayer, ulPlayer, SVCF_ONLYTHISCLIENT );
}

//*****************************************************************************
//
void SERVERCOMMANDS_MovePlayer( ULONG ulPlayer, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;
	ULONG ulPlayerAttackFlags = 0;

	if ( SERVER_IsValidPlayer( ulPlayer ) == false )
		return;

	// [BB] Check if ulPlayer is pressing any attack buttons.
	if ( players[ulPlayer].cmd.ucmd.buttons & BT_ATTACK )
		ulPlayerAttackFlags |= PLAYER_ATTACK;
	if ( players[ulPlayer].cmd.ucmd.buttons & BT_ALTATTACK )
		ulPlayerAttackFlags |= PLAYER_ALTATTACK;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		SERVER_CheckClientBuffer( ulIdx, 20, false );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->UnreliablePacketBuffer.ByteStream, SVC_MOVEPLAYER );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->UnreliablePacketBuffer.ByteStream, ulPlayer );

		// If this player cannot be seen by (or is not allowed to be seen by) the
		// player, don't send position information.
		if ( SERVER_IsPlayerVisible( ulIdx, ulPlayer ) == false )
			NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->UnreliablePacketBuffer.ByteStream, ulPlayerAttackFlags );
		else
		{
			// The player IS visible, so his info is coming!
			NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->UnreliablePacketBuffer.ByteStream, PLAYER_VISIBLE|ulPlayerAttackFlags );

			// Write position.
			NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->UnreliablePacketBuffer.ByteStream, players[ulPlayer].mo->x >> FRACBITS );
			NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->UnreliablePacketBuffer.ByteStream, players[ulPlayer].mo->y >> FRACBITS );
			NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->UnreliablePacketBuffer.ByteStream, players[ulPlayer].mo->z >> FRACBITS );

			// Write angle.
			NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->UnreliablePacketBuffer.ByteStream, players[ulPlayer].mo->angle );

			// Write velocity.
			NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->UnreliablePacketBuffer.ByteStream, players[ulPlayer].mo->momx >> FRACBITS );
			NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->UnreliablePacketBuffer.ByteStream, players[ulPlayer].mo->momy >> FRACBITS );
			NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->UnreliablePacketBuffer.ByteStream, players[ulPlayer].mo->momz >> FRACBITS );

			// Write whether or not the player is crouching.
			NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->UnreliablePacketBuffer.ByteStream, ( players[ulPlayer].crouchdir >= 0 ) ? true : false );
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

		SERVER_CheckClientBuffer( ulIdx, 4, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_DAMAGEPLAYER );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulPlayer );

		// Only send the player who's being damaged to this player if this player is
		// allowed to know what his health is. Otherwise, just tell them it's 100/100
		// (WHICH IS A LIE!!!!!!).
		if ( SERVER_IsPlayerAllowedToKnowHealth( ulIdx, ulPlayer ))
		{
			NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, players[ulPlayer].health );
			NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulArmorPoints );
		}
		else
		{
			NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, 100 );
			NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, 100 );
		}
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_KillPlayer( ULONG ulPlayer, AActor *pSource, AActor *pInflictor, ULONG ulMOD )
{
	ULONG	ulIdx;
	USHORT	usActorNetworkIndex = 0;
	LONG	lSourceID;
	LONG	lInflictorID;

	if ( SERVER_IsValidPlayer( ulPlayer ) == false )
		return;

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
				usActorNetworkIndex = players[ulIdx].ReadyWeapon->GetClass( )->getActorNetworkIndex();
			else
				usActorNetworkIndex = 0;
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

		SERVER_CheckClientBuffer( ulIdx, 11 + (ULONG)strlen(players[ulPlayer].mo->DamageType.GetChars()), true );

		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_KILLPLAYER );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulPlayer );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lSourceID );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lInflictorID );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, players[ulPlayer].mo->health );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulMOD );
		NETWORK_WriteString( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, players[ulPlayer].mo->DamageType.GetChars() );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, usActorNetworkIndex );
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
			SERVER_CheckClientBuffer( ulIdx, 3, true );
			NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETPLAYERHEALTH );
			NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulPlayer );
			NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, players[ulPlayer].health );
		}
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerArmor( ULONG ulPlayer )
{
	AInventory *pArmor = players[ulPlayer].mo->FindInventory< ABasicArmor >( );
	ULONG ulArmorPoints = ( pArmor != NULL ) ? pArmor->Amount : 0;
	FString armorIconTexName = pArmor->Icon.isValid() ? TexMan( pArmor->Icon )->Name : "";
	if ( ulArmorPoints > 0 ){
		SERVER_CheckClientBuffer( ulPlayer, 4 + (ULONG)strlen( armorIconTexName.GetChars() ), true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulPlayer )->PacketBuffer.ByteStream, SVC_SETPLAYERARMOR );
		NETWORK_WriteByte( &SERVER_GetClient( ulPlayer )->PacketBuffer.ByteStream, ulPlayer );
		NETWORK_WriteShort( &SERVER_GetClient( ulPlayer )->PacketBuffer.ByteStream, ulArmorPoints );
		NETWORK_WriteString( &SERVER_GetClient( ulPlayer )->PacketBuffer.ByteStream, armorIconTexName.GetChars() );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerState( ULONG ulPlayer, PLAYERSTATE_e ulState, ULONG ulPlayerExtra, ULONG ulFlags )
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

		SERVER_CheckClientBuffer( ulIdx, 3, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETPLAYERSTATE );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulPlayer );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulState );
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

		SERVER_CheckClientBuffer( ulIdx, 13 + (ULONG)( strlen( players[ulPlayer].userinfo.netname ) + strlen( skins[players[ulPlayer].userinfo.skin].name )), true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETPLAYERUSERINFO );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulPlayer );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulUserInfoFlags );
		if ( ulUserInfoFlags & USERINFO_NAME )
			NETWORK_WriteString( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, players[ulPlayer].userinfo.netname );

		if ( ulUserInfoFlags & USERINFO_GENDER )
			NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, players[ulPlayer].userinfo.gender );

		if ( ulUserInfoFlags & USERINFO_COLOR )
			NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, players[ulPlayer].userinfo.color );

		if ( ulUserInfoFlags & USERINFO_RAILCOLOR )
			NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, players[ulPlayer].userinfo.lRailgunTrailColor );

		if ( ulUserInfoFlags & USERINFO_SKIN )
			NETWORK_WriteString( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SERVER_GetClient( ulPlayer )->szSkin );

		if ( ulUserInfoFlags & USERINFO_HANDICAP )
			NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, players[ulPlayer].userinfo.lHandicap );
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

		SERVER_CheckClientBuffer( ulIdx, 4, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETPLAYERFRAGS );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulPlayer );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, players[ulPlayer].fragcount );
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

		SERVER_CheckClientBuffer( ulIdx, 4, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETPLAYERPOINTS );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulPlayer );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, players[ulPlayer].lPointCount );
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

		SERVER_CheckClientBuffer( ulIdx, 3, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETPLAYERWINS );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulPlayer );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, players[ulPlayer].ulWins );
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

		SERVER_CheckClientBuffer( ulIdx, 3, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETPLAYERKILLCOUNT );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulPlayer );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, players[ulPlayer].killcount );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerChatStatus( ULONG ulPlayer )
{
	SERVERCOMMANDS_SetPlayerStatus( ulPlayer, SVC_SETPLAYERCHATSTATUS, players[ulPlayer].bChatting );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerLaggingStatus( ULONG ulPlayer )
{
	SERVERCOMMANDS_SetPlayerStatus( ulPlayer, SVC_SETPLAYERLAGGINGSTATUS, players[ulPlayer].bLagging );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerConsoleStatus( ULONG ulPlayer )
{
	SERVERCOMMANDS_SetPlayerStatus( ulPlayer, SVC_SETPLAYERCONSOLESTATUS, players[ulPlayer].bInConsole );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerReadyToGoOnStatus( ULONG ulPlayer )
{
	SERVERCOMMANDS_SetPlayerStatus( ulPlayer, SVC_SETPLAYERREADYTOGOONSTATUS, players[ulPlayer].bReadyToGoOn );
}

//*****************************************************************************
// [RC] Notifies all players about a player's boolean flag.
//
void SERVERCOMMANDS_SetPlayerStatus( ULONG ulPlayer, int iHeader, bool bValue )
{
	ULONG	ulIdx;

	if ( SERVER_IsValidPlayer( ulPlayer ) == false )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		SERVER_CheckClientBuffer( ulIdx, 3, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, iHeader );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulPlayer );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, bValue );
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

		SERVER_CheckClientBuffer( ulIdx, 3, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETPLAYERTEAM );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulPlayer );
		if ( players[ulPlayer].bOnTeam == false )
			NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, teams.Size( ) );
		else
			NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, players[ulPlayer].ulTeam );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerCamera( ULONG ulPlayer, LONG lCameraNetID, bool bRevertPlease )
{
	if ( SERVER_IsValidClient( ulPlayer ) == false )
		return;

	SERVER_CheckClientBuffer( ulPlayer, 4, true );
	NETWORK_WriteHeader( &SERVER_GetClient( ulPlayer )->PacketBuffer.ByteStream, SVC_SETPLAYERCAMERA );
	NETWORK_WriteShort( &SERVER_GetClient( ulPlayer )->PacketBuffer.ByteStream, lCameraNetID );
	NETWORK_WriteByte( &SERVER_GetClient( ulPlayer )->PacketBuffer.ByteStream, bRevertPlease );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerPoisonCount( ULONG ulPlayer, ULONG ulPlayerExtra, ULONG ulFlags )
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

		SERVER_CheckClientBuffer( ulIdx, 4, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETPLAYERPOISONCOUNT );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulPlayer );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, players[ulPlayer].poisoncount );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerAmmoCapacity( ULONG ulPlayer, AInventory *pAmmo, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( SERVER_IsValidPlayer( ulPlayer ) == false )
		return;

	if ( pAmmo == NULL )
		return;

	if ( !(pAmmo->GetClass()->IsDescendantOf (RUNTIME_CLASS(AAmmo))) )
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

		SERVER_CheckClientBuffer( ulIdx, 6, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETPLAYERAMMOCAPACITY );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulPlayer );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pAmmo->GetClass( )->getActorNetworkIndex() );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pAmmo->MaxAmount );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerCheats( ULONG ulPlayer, ULONG ulPlayerExtra, ULONG ulFlags )
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

		SERVER_CheckClientBuffer( ulIdx, 6, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETPLAYERCHEATS );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulPlayer );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, players[ulPlayer].cheats );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerPendingWeapon( ULONG ulPlayer, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG		ulIdx;
	USHORT		usActorNetworkIndex = 0;

	if ( SERVER_IsValidPlayer( ulPlayer ) == false )
		return;

	if (( players[ulPlayer].PendingWeapon != WP_NOCHANGE ) &&
		( players[ulPlayer].PendingWeapon != NULL ))
	{
		usActorNetworkIndex = players[ulPlayer].PendingWeapon->GetClass( )->getActorNetworkIndex();
	}
	else
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		// Only send this info to spectators.
		// [BB] Or if this is a COOP game.
		// [BB] Everybody needs to know this. Otherwise the Railgun sound is broken and spying in demos doesn't work properly.
		//if ( (PLAYER_IsTrueSpectator( &players[ulIdx] ) == false) && !(GAMEMODE_GetFlags( GAMEMODE_GetCurrentMode( )) & GMF_COOPERATIVE ) )
		//	continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		SERVER_CheckClientBuffer( ulIdx, 4, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETPLAYERPENDINGWEAPON );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulPlayer );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, usActorNetworkIndex );
	}
}

//*****************************************************************************
//
/* [BB] Does not work with the latest ZDoom changes. Check if it's still necessary.
void SERVERCOMMANDS_SetPlayerPieces( ULONG ulPlayer, ULONG ulPlayerExtra, ULONG ulFlags )
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

		SERVER_CheckClientBuffer( ulIdx, 3, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETPLAYERPIECES );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulPlayer );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, players[ulPlayer].pieces );
	}
}
*/

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerPSprite( ULONG ulPlayer, FState *pState, LONG lPosition, ULONG ulPlayerExtra, ULONG ulFlags )
{
	FString			stateLabel;
	LONG			lOffset;
	ULONG			ulIdx;
	const PClass	*pClass;

	if ( SERVER_IsValidPlayer( ulPlayer ) == false )
		return;

	if ( players[ulPlayer].ReadyWeapon == NULL )
		return;

	pClass = players[ulPlayer].ReadyWeapon->GetClass( );

	// [BB] Try to find the state label and the correspoding offset belonging to the target state.
	FindStateLabelAndOffset( pClass, pState, stateLabel, lOffset );

	// Couldn't find the state, so just try to go based off the spawn state.
	if ( stateLabel.IsEmpty() )
	{
		stateLabel = "Ready";
		lOffset = LONG( pState - players[ulPlayer].ReadyWeapon->GetReadyState( ));
		if (( lOffset < 0 ) ||
			( lOffset > 255 ))
		{
			return;
		}
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

		SERVER_CheckClientBuffer( ulIdx, 4 + static_cast<ULONG>( stateLabel.Len() ), true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETPLAYERPSPRITE );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulPlayer );
		NETWORK_WriteString( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, stateLabel.GetChars() );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lOffset );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lPosition );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerBlend( ULONG ulPlayer, ULONG ulPlayerExtra, ULONG ulFlags )
{
	if ( SERVER_IsValidPlayer( ulPlayer ) == false )
		return;

	for ( ULONG ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		SERVER_CheckClientBuffer( ulIdx, 18, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETPLAYERBLEND );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulPlayer );
		NETWORK_WriteFloat( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, players[ulPlayer].BlendR );
		NETWORK_WriteFloat( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, players[ulPlayer].BlendG );
		NETWORK_WriteFloat( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, players[ulPlayer].BlendB );
		NETWORK_WriteFloat( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, players[ulPlayer].BlendA );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerMaxHealth( ULONG ulPlayer, ULONG ulPlayerExtra, ULONG ulFlags )
{
	if ( SERVER_IsValidPlayer( ulPlayer ) == false )
		return;

	if ( players[ulPlayer].mo == NULL )
		return;

	for ( ULONG ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		SERVER_CheckClientBuffer( ulIdx, 6, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETPLAYERMAXHEALTH );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulPlayer );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, players[ulPlayer].mo->MaxHealth );
	}
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

		SERVER_CheckClientBuffer( ulIdx, 4, false );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->UnreliablePacketBuffer.ByteStream, SVC_UPDATEPLAYERPING );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->UnreliablePacketBuffer.ByteStream, ulPlayer );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->UnreliablePacketBuffer.ByteStream, players[ulPlayer].ulPing );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_UpdatePlayerExtraData( ULONG ulPlayer, ULONG ulDisplayPlayer )
{
	if (( SERVER_IsValidClient( ulPlayer ) == false ) || ( SERVER_IsValidPlayer( ulDisplayPlayer ) == false ))
		return;

	SERVER_CheckClientBuffer( ulPlayer, 18, false );
	NETWORK_WriteHeader( &SERVER_GetClient( ulPlayer )->UnreliablePacketBuffer.ByteStream, SVC_UPDATEPLAYEREXTRADATA );
	NETWORK_WriteByte( &SERVER_GetClient( ulPlayer )->UnreliablePacketBuffer.ByteStream, ulDisplayPlayer );

//	NETWORK_WriteByte( &SERVER_GetClient( ulPlayer )->UnreliablePacketBuffer.ByteStream, players[ulDisplayPlayer].pendingweapon );
//	NETWORK_WriteByte( &SERVER_GetClient( ulPlayer )->UnreliablePacketBuffer.ByteStream, players[ulDisplayPlayer].readyweapon );
	NETWORK_WriteLong( &SERVER_GetClient( ulPlayer )->UnreliablePacketBuffer.ByteStream, players[ulDisplayPlayer].mo->pitch );
	NETWORK_WriteByte( &SERVER_GetClient( ulPlayer )->UnreliablePacketBuffer.ByteStream, players[ulDisplayPlayer].mo->waterlevel );
	NETWORK_WriteByte( &SERVER_GetClient( ulPlayer )->UnreliablePacketBuffer.ByteStream, players[ulDisplayPlayer].cmd.ucmd.buttons );
	NETWORK_WriteLong( &SERVER_GetClient( ulPlayer )->UnreliablePacketBuffer.ByteStream, players[ulDisplayPlayer].viewz );
	NETWORK_WriteLong( &SERVER_GetClient( ulPlayer )->UnreliablePacketBuffer.ByteStream, players[ulDisplayPlayer].bob );

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

		SERVER_CheckClientBuffer( ulPlayer, 4, false );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->UnreliablePacketBuffer.ByteStream, SVC_UPDATEPLAYERTIME );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->UnreliablePacketBuffer.ByteStream, ulPlayer );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->UnreliablePacketBuffer.ByteStream, ( players[ulPlayer].ulTime / ( TICRATE * 60 )));
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_MoveLocalPlayer( ULONG ulPlayer )
{
	if ( SERVER_IsValidClient( ulPlayer ) == false )
		return;

	SERVER_CheckClientBuffer( ulPlayer, 29, false );
	NETWORK_WriteHeader( &SERVER_GetClient( ulPlayer )->UnreliablePacketBuffer.ByteStream, SVC_MOVELOCALPLAYER );

	NETWORK_WriteLong( &SERVER_GetClient( ulPlayer )->UnreliablePacketBuffer.ByteStream, SERVER_GetClient( ulPlayer )->ulClientGameTic );

	// Write position.
	NETWORK_WriteLong( &SERVER_GetClient( ulPlayer )->UnreliablePacketBuffer.ByteStream, players[ulPlayer].mo->x );
	NETWORK_WriteLong( &SERVER_GetClient( ulPlayer )->UnreliablePacketBuffer.ByteStream, players[ulPlayer].mo->y );
	NETWORK_WriteLong( &SERVER_GetClient( ulPlayer )->UnreliablePacketBuffer.ByteStream, players[ulPlayer].mo->z );

	// Write velocity.
	NETWORK_WriteLong( &SERVER_GetClient( ulPlayer )->UnreliablePacketBuffer.ByteStream, players[ulPlayer].mo->momx );
	NETWORK_WriteLong( &SERVER_GetClient( ulPlayer )->UnreliablePacketBuffer.ByteStream, players[ulPlayer].mo->momy );
	NETWORK_WriteLong( &SERVER_GetClient( ulPlayer )->UnreliablePacketBuffer.ByteStream, players[ulPlayer].mo->momz );
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

		SERVER_CheckClientBuffer( ulIdx, 2, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_DISCONNECTPLAYER );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulPlayer );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetConsolePlayer( ULONG ulPlayer )
{
	if ( SERVER_IsValidClient( ulPlayer ) == false )
		return;

	SERVER_CheckClientBuffer( ulPlayer, 2, true );
	NETWORK_WriteHeader( &SERVER_GetClient( ulPlayer )->PacketBuffer.ByteStream, SVC_SETCONSOLEPLAYER );
	NETWORK_WriteByte( &SERVER_GetClient( ulPlayer )->PacketBuffer.ByteStream, ulPlayer );
}

//*****************************************************************************
//
void SERVERCOMMANDS_ConsolePlayerKicked( ULONG ulPlayer )
{
	if ( SERVER_IsValidClient( ulPlayer ) == false )
		return;

	SERVER_CheckClientBuffer( ulPlayer, 1, true );
	NETWORK_WriteHeader( &SERVER_GetClient( ulPlayer )->PacketBuffer.ByteStream, SVC_CONSOLEPLAYERKICKED );
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

		SERVER_CheckClientBuffer( ulIdx, 3, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_GIVEPLAYERMEDAL );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulPlayer );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulMedal );
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

		SERVER_CheckClientBuffer( ulIdx, 1, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_RESETALLPLAYERSFRAGCOUNT );
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

		SERVER_CheckClientBuffer( ulIdx, 3, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_PLAYERISSPECTATOR );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulPlayer );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, players[ulPlayer].bDeadSpectator );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_PlayerSay( ULONG ulPlayer, const char *pszString, ULONG ulMode, bool bForbidChatToPlayers, ULONG ulPlayerExtra, ULONG ulFlags )
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
			if ( GAMEMODE_GetFlags( GAMEMODE_GetCurrentMode( )) & GMF_PLAYERSONTEAMS )
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

		SERVER_CheckClientBuffer( ulIdx, 3 + (ULONG)strlen( pszString ), true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_PLAYERSAY );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulPlayer );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulMode );
		NETWORK_WriteString( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pszString );
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

		SERVER_CheckClientBuffer( ulIdx, 2, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_PLAYERTAUNT );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulPlayer );
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

		SERVER_CheckClientBuffer( ulIdx, 2, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_PLAYERRESPAWNINVULNERABILITY );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulPlayer );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_PlayerUseInventory( ULONG ulPlayer, AInventory *pItem, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG		ulIdx;
	USHORT		usActorNetworkIndex = 0;

	if (( SERVER_IsValidPlayer( ulPlayer ) == false ) ||
		( pItem == NULL ))
	{
		return;
	}

	usActorNetworkIndex = pItem->GetClass( )->getActorNetworkIndex();
	if ( NETWORK_GetClassNameFromIdentification ( usActorNetworkIndex ) == NULL )
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

		SERVER_CheckClientBuffer( ulIdx, 4, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_PLAYERUSEINVENTORY );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulPlayer );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, usActorNetworkIndex );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_PlayerDropInventory( ULONG ulPlayer, AInventory *pItem, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG		ulIdx;
	USHORT		usActorNetworkIndex = 0;

	if (( SERVER_IsValidPlayer( ulPlayer ) == false ) ||
		( pItem == NULL ))
	{
		return;
	}

	usActorNetworkIndex = pItem->GetClass( )->getActorNetworkIndex();
	if ( NETWORK_GetClassNameFromIdentification ( usActorNetworkIndex ) == NULL )
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

		SERVER_CheckClientBuffer( ulIdx, 4, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_PLAYERDROPINVENTORY );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulPlayer );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, usActorNetworkIndex );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_PotentiallyIgnorePlayer( ULONG ulPlayer )
{
	for ( ULONG ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false || ( ulIdx == ulPlayer ))
			continue; 

		// Check whether this player is ignoring the newcomer's address.
		LONG lTicks = SERVER_GetPlayerIgnoreTic( ulIdx, SERVER_GetClient( ulPlayer )->Address );
		if ( lTicks != 0 )
		{
			SERVER_CheckClientBuffer( ulIdx, 6, true );
			NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_IGNOREPLAYER );
			NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulPlayer );
			NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lTicks );
		}
	}
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_SpawnThing( AActor *pActor, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG		ulIdx;
	USHORT		usActorNetworkIndex = 0;

	if ( pActor == NULL )
		return;

	// If the actor doesn't have a network ID, it's better to send it ID-less.
	if ( pActor->lNetID == -1 )
	{
		SERVERCOMMANDS_SpawnThingNoNetID( pActor, ulPlayerExtra, ulFlags );
		return;
	}

	usActorNetworkIndex = pActor->GetClass( )->getActorNetworkIndex();

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		SERVER_CheckClientBuffer( ulIdx, 11, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SPAWNTHING );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->x >> FRACBITS );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->y >> FRACBITS );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->z >> FRACBITS );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, usActorNetworkIndex );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->lNetID );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SpawnThingNoNetID( AActor *pActor, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG		ulIdx;
	USHORT		usActorNetworkIndex = 0;

	if ( pActor == NULL )
		return;

	usActorNetworkIndex = pActor->GetClass( )->getActorNetworkIndex();

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		SERVER_CheckClientBuffer( ulIdx, 9, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SPAWNTHINGNONETID );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->x >> FRACBITS );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->y >> FRACBITS );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->z >> FRACBITS );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, usActorNetworkIndex );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SpawnThingExact( AActor *pActor, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG		ulIdx;
	USHORT		usActorNetworkIndex = 0;

	if ( pActor == NULL )
		return;

	// If the actor doesn't have a network ID, it's better to send it ID-less.
	if ( pActor->lNetID == -1 )
	{
		SERVERCOMMANDS_SpawnThingExactNoNetID( pActor, ulPlayerExtra, ulFlags );
		return;
	}

	usActorNetworkIndex = pActor->GetClass( )->getActorNetworkIndex();

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		SERVER_CheckClientBuffer( ulIdx, 17, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SPAWNTHINGEXACT );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->x );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->y );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->z );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, usActorNetworkIndex );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->lNetID );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SpawnThingExactNoNetID( AActor *pActor, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG		ulIdx;
	USHORT		usActorNetworkIndex = 0;

	if ( pActor == NULL )
		return;

	usActorNetworkIndex = pActor->GetClass( )->getActorNetworkIndex();

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		SERVER_CheckClientBuffer( ulIdx, 15, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SPAWNTHINGEXACTNONETID );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->x );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->y );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->z );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, usActorNetworkIndex );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_MoveThing( AActor *pActor, ULONG ulBits, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;
	ULONG	ulSize;

	if ( !EnsureActorHasNetID (pActor) )
		return;

	// [BB] Only skip updates, if sent to all players.
	if ( ulFlags == 0 )
		RemoveUnnecessaryPositionUpdateFlags ( pActor, ulBits );

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
	if ( ulBits & CM_PITCH )
		ulSize += 4;
	if ( ulBits & CM_MOVEDIR )
		ulSize += 1;

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

		SERVER_CheckClientBuffer( ulIdx, ulSize, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_MOVETHING );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->lNetID );

		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulBits );

		// Write position.
		if ( ulBits & CM_X )
			NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->x >> FRACBITS );
		if ( ulBits & CM_Y )
			NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->y >> FRACBITS );
		if ( ulBits & CM_Z )
			NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->z >> FRACBITS );

		// Write angle.
		if ( ulBits & CM_ANGLE )
			NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->angle );

		// Write velocity.
		if ( ulBits & CM_MOMX )
			NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->momx >> FRACBITS );
		if ( ulBits & CM_MOMY )
			NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->momy >> FRACBITS );
		if ( ulBits & CM_MOMZ )
			NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->momz >> FRACBITS );

		// Write pitch.
		if ( ulBits & CM_PITCH )
			NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->pitch );

		// Write movedir.
		if ( ulBits & CM_MOVEDIR )
			NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->movedir );
	}

	// [BB] Only mark something as updated, if it the update was sent to all players.
	if ( ulFlags == 0 )
		ActorNetPositionUpdated ( pActor, ulBits );
}

//*****************************************************************************
//
void SERVERCOMMANDS_MoveThingExact( AActor *pActor, ULONG ulBits, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;
	ULONG	ulSize;

	if ( !EnsureActorHasNetID (pActor) )
		return;

	// [BB] Only skip updates, if sent to all players.
	if ( ulFlags == 0 )
		RemoveUnnecessaryPositionUpdateFlags ( pActor, ulBits );

	ulSize = 0;
	if ( ulBits & CM_X )
		ulSize += 4;
	if ( ulBits & CM_Y )
		ulSize += 4;
	if ( ulBits & CM_Z )
		ulSize += 4;
	if ( ulBits & CM_ANGLE )
		ulSize += 4;
	if ( ulBits & CM_MOMX )
		ulSize += 4;
	if ( ulBits & CM_MOMY )
		ulSize += 4;
	if ( ulBits & CM_MOMZ )
		ulSize += 4;
	if ( ulBits & CM_PITCH )
		ulSize += 4;
	if ( ulBits & CM_MOVEDIR )
		ulSize += 1;

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

		SERVER_CheckClientBuffer( ulIdx, ulSize, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_MOVETHINGEXACT );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->lNetID );

		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulBits );

		// Write position.
		if ( ulBits & CM_X )
			NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->x );
		if ( ulBits & CM_Y )
			NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->y );
		if ( ulBits & CM_Z )
			NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->z );

		// Write angle.
		if ( ulBits & CM_ANGLE )
			NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->angle );

		// Write velocity.
		if ( ulBits & CM_MOMX )
			NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->momx );
		if ( ulBits & CM_MOMY )
			NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->momy );
		if ( ulBits & CM_MOMZ )
			NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->momz );

		// Write pitch.
		if ( ulBits & CM_PITCH )
			NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->pitch );

		// Write movedir.
		if ( ulBits & CM_MOVEDIR )
			NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->movedir );
	}

	// [BB] Only mark something as updated, if it the update was sent to all players.
	if ( ulFlags == 0 )
		ActorNetPositionUpdated ( pActor, ulBits );
}

//*****************************************************************************
//
void SERVERCOMMANDS_DamageThing( AActor *pActor )
{
	ULONG	ulIdx;

	if ( !EnsureActorHasNetID (pActor) )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		SERVER_CheckClientBuffer( ulIdx, 3, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_DAMAGETHING );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->lNetID );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_KillThing( AActor *pActor, AActor *pSource, AActor *pInflictor )
{
	ULONG	ulIdx;
	LONG	lSourceID;
	LONG	lInflictorID;

	if ( !EnsureActorHasNetID (pActor) )
		return;

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

		SERVER_CheckClientBuffer( ulIdx, 9 + ULONG(strlen(pActor->DamageType.GetChars())), true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_KILLTHING );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->lNetID );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->health );
		NETWORK_WriteString( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->DamageType.GetChars() );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lSourceID );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lInflictorID );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetThingState( AActor *pActor, ULONG ulState )
{
	ULONG	ulIdx;

	if ( !EnsureActorHasNetID (pActor) )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		SERVER_CheckClientBuffer( ulIdx, 4, true );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETTHINGSTATE );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->lNetID );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulState );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetThingTarget( AActor *pActor )
{
	ULONG	ulIdx;

	if ( !EnsureActorHasNetID (pActor) || !EnsureActorHasNetID(pActor->target) )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		SERVER_CheckClientBuffer( ulIdx, 5, true );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETTHINGTARGET );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->lNetID );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->target->lNetID );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_DestroyThing( AActor *pActor )
{
	ULONG	ulIdx;

	if ( !EnsureActorHasNetID (pActor) )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		SERVER_CheckClientBuffer( ulIdx, 3, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_DESTROYTHING );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->lNetID );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetThingAngle( AActor *pActor, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( !EnsureActorHasNetID (pActor) )
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

		SERVER_CheckClientBuffer( ulIdx, 5, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETTHINGANGLE );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->lNetID );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->angle >> FRACBITS );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetThingAngleExact( AActor *pActor, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( !EnsureActorHasNetID (pActor) )
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

		SERVER_CheckClientBuffer( ulIdx, 7, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETTHINGANGLEEXACT );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->lNetID );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->angle );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetThingMoveDir( AActor *pActor, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( !EnsureActorHasNetID (pActor) )
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

		SERVER_CheckClientBuffer( ulIdx, 4, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETTHINGMOVEDIR );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->lNetID );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->movedir );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetThingWaterLevel( AActor *pActor, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( !EnsureActorHasNetID (pActor) )
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

		SERVER_CheckClientBuffer( ulIdx, 4, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETTHINGWATERLEVEL );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->lNetID );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->waterlevel );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetThingFlags( AActor *pActor, ULONG ulFlagSet, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;
	ULONG	ulActorFlags;

	if ( !EnsureActorHasNetID (pActor) )
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

		SERVER_CheckClientBuffer( ulIdx, 8, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETTHINGFLAGS );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->lNetID );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulFlagSet );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulActorFlags );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_UpdateThingFlagsNotAtDefaults( AActor *pActor, ULONG ulPlayerExtra, ULONG ulFlags )
{
	if ( pActor->flags != pActor->GetDefault( )->flags )
	{
		SERVERCOMMANDS_SetThingFlags( pActor, FLAGSET_FLAGS, ulPlayerExtra, ulFlags );
	}
	if ( pActor->flags2 != pActor->GetDefault( )->flags2 )
	{
		SERVERCOMMANDS_SetThingFlags( pActor, FLAGSET_FLAGS2, ulPlayerExtra, ulFlags );
	}
	if ( pActor->flags3 != pActor->GetDefault( )->flags3 )
	{
		SERVERCOMMANDS_SetThingFlags( pActor, FLAGSET_FLAGS3, ulPlayerExtra, ulFlags );
	}
	if ( pActor->flags4 != pActor->GetDefault( )->flags4 )
	{
		SERVERCOMMANDS_SetThingFlags( pActor, FLAGSET_FLAGS4, ulPlayerExtra, ulFlags );
	}
	if ( pActor->flags5 != pActor->GetDefault( )->flags5 )
	{
		SERVERCOMMANDS_SetThingFlags( pActor, FLAGSET_FLAGS5, ulPlayerExtra, ulFlags );
	}
	// [BB] ulSTFlags is intentionally left out here.
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetThingArguments( AActor *pActor, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( !EnsureActorHasNetID (pActor) )
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

		SERVER_CheckClientBuffer( ulIdx, 8, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETTHINGARGUMENTS );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->lNetID );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->args[0] );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->args[1] );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->args[2] );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->args[3] );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->args[4] );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetThingTranslation( AActor *pActor, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( !EnsureActorHasNetID (pActor) )
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

		SERVER_CheckClientBuffer( ulIdx, 7, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETTHINGTRANSLATION );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->lNetID );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->Translation );
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
	ULONG	ulPropertyValue = 0;

	if ( !EnsureActorHasNetID (pActor) )
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

		ulPropertyValue = pActor->RenderStyle.AsDWORD;
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

		SERVER_CheckClientBuffer( ulIdx, 7, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETTHINGPROPERTY );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->lNetID );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulProperty );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulPropertyValue );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetThingSound( AActor *pActor, ULONG ulSound, char *pszSound, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( !EnsureActorHasNetID (pActor) )
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

		SERVER_CheckClientBuffer( ulIdx, 4 + static_cast<ULONG>(strlen( pszSound )), true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETTHINGSOUND );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->lNetID );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulSound );
		NETWORK_WriteString( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pszSound );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetThingSpawnPoint( AActor *pActor, ULONG ulPlayerExtra, ULONG ulFlags )
{
	if ( !EnsureActorHasNetID (pActor) )
		return;

	for ( ULONG ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		SERVER_CheckClientBuffer( ulIdx, 15, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETTHINGSPAWNPOINT );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->lNetID );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->SpawnPoint[0] );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->SpawnPoint[1] );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->SpawnPoint[2] );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetThingSpecial1( AActor *pActor, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( !EnsureActorHasNetID (pActor) )
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

		SERVER_CheckClientBuffer( ulIdx, 5, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETTHINGSPECIAL1 );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->lNetID );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->special1 );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetThingSpecial2( AActor *pActor, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( !EnsureActorHasNetID (pActor) )
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

		SERVER_CheckClientBuffer( ulIdx, 5, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETTHINGSPECIAL2 );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->lNetID );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->special2 );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetThingTics( AActor *pActor, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( !EnsureActorHasNetID (pActor) )
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

		SERVER_CheckClientBuffer( ulIdx, 5, true );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETTHINGTICS );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->lNetID );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->tics );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetThingTID( AActor *pActor, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( !EnsureActorHasNetID (pActor) )
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

		SERVER_CheckClientBuffer( ulIdx, 5, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETTHINGTID );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->lNetID );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->tid );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetThingGravity( AActor *pActor, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( !EnsureActorHasNetID (pActor) )
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

		SERVER_CheckClientBuffer( ulIdx, 5, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETTHINGGRAVITY );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->lNetID );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->gravity );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetThingFrame( AActor *pActor, FState *pState, ULONG ulPlayerExtra, ULONG ulFlags, bool bCallStateFunction )
{
	FString stateLabel;
	LONG		lOffset = 0;
	ULONG		ulIdx;

	if ( !EnsureActorHasNetID (pActor) || (pState == NULL) )
		return;

	// [BB] Special handling of certain states. This perhaps is not necessary
	// but at least saves a little net traffic.
	if ( bCallStateFunction 
		 && (ulPlayerExtra == MAXPLAYERS)
		 && (ulFlags == 0)
		)
	{
		if ( pState == pActor->MeleeState )
		{
			SERVERCOMMANDS_SetThingState( pActor, STATE_MELEE );
			return;
		}
		else if ( pState == pActor->MissileState )
		{
			SERVERCOMMANDS_SetThingState( pActor, STATE_MISSILE );
			return;
		}
	}

	// [BB] Try to find the state label and the correspoding offset belonging to the target state.
	FindStateLabelAndOffset( pActor->GetClass( ), pState, stateLabel, lOffset );

	// Couldn't find the state, so just try to go based off one of the standard states.
	// [BB] This is a workaround. Therefore let the name of the state string begin
	// with ':' so that the client can handle this differently.
	// [BB] Because of inheritance it's not sufficient to only try the SpawnState.
	if ( stateLabel.IsEmpty() )
	{
		lOffset = LONG( pState - pActor->SpawnState );
		if (( lOffset < 0 ) || ( lOffset > 255 ))
		{
			// [BB] SpawnState doesn't work. Try MissileState.
			lOffset = LONG( pState - pActor->MissileState );
			if (( lOffset < 0 ) || ( lOffset > 255 ))
			{
				// [BB] Try SeeState.
				lOffset = LONG( pState - pActor->SeeState );
				if (( lOffset < 0 ) || ( lOffset > 255 ))
				{
					// [BB] Try MeleeState.
					lOffset = LONG( pState - pActor->MeleeState );
					if (( lOffset < 0 ) || ( lOffset > 255 ))
					{
						// [BB] Apparently pActor doesn't own the state. Find out who does.
						const PClass *pStateOwnerClass = FState::StaticFindStateOwner ( pState );
						const AActor *pStateOwner = ( pStateOwnerClass != NULL ) ? GetDefaultByType ( pStateOwnerClass ) : NULL;
						if ( pStateOwner )
						{
							lOffset = LONG( pState - pStateOwner->SpawnState );
						}
						if (( lOffset < 0 ) || ( lOffset > 255 ))
						{
							if ( sv_showwarnings )
								Printf ( "Warning: SERVERCOMMANDS_SetThingFrame failed to set the frame for actor %s.\n", pActor->GetClass()->TypeName.GetChars() );
							return;
						}
						else
						{
							stateLabel = ";";
							stateLabel += pStateOwnerClass->TypeName;
						}
					}
					else
						stateLabel = ":N";
				}
				else
					stateLabel = ":T";
			}
			else
				stateLabel = ":M";
		}
		else
			stateLabel = ":S";
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

		SERVER_CheckClientBuffer( ulIdx, 4 + static_cast<ULONG>( stateLabel.Len() ), true );
		if ( bCallStateFunction )
			NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETTHINGFRAME );
		else
			NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETTHINGFRAMENF );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->lNetID );
		NETWORK_WriteString( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, stateLabel.GetChars() );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lOffset );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetWeaponAmmoGive( AActor *pActor, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( !EnsureActorHasNetID (pActor) )
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

		SERVER_CheckClientBuffer( ulIdx, 7, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETWEAPONAMMOGIVE );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->lNetID );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, static_cast<AWeapon *>( pActor )->AmmoGive1 );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, static_cast<AWeapon *>( pActor )->AmmoGive2 );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_ThingIsCorpse( AActor *pActor, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( !EnsureActorHasNetID (pActor) )
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

		SERVER_CheckClientBuffer( ulIdx, 4, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_THINGISCORPSE );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->lNetID );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->CountsAsKill( ) ? true : false );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_HideThing( AActor *pActor, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	// [BB] The client will call HideIndefinitely on the actor, which only is possible on AInventory and descendants.
	if ( !EnsureActorHasNetID (pActor) || !(pActor->IsKindOf( RUNTIME_CLASS( AInventory ))) )
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

		SERVER_CheckClientBuffer( ulIdx, 3, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_HIDETHING );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->lNetID );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_TeleportThing( AActor *pActor, bool bSourceFog, bool bDestFog, bool bTeleZoom, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( !EnsureActorHasNetID (pActor) )
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

		SERVER_CheckClientBuffer( ulIdx, 24, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_TELEPORTTHING );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->lNetID );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->x >> FRACBITS );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->y >> FRACBITS );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->z >> FRACBITS );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->momx >> FRACBITS );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->momy >> FRACBITS );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->momz >> FRACBITS );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->reactiontime );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->angle );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, bSourceFog );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, bDestFog );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, bTeleZoom );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_ThingActivate( AActor *pActor, AActor *pActivator, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( !EnsureActorHasNetID (pActor) )
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

		SERVER_CheckClientBuffer( ulIdx, 5, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_THINGACTIVATE );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->lNetID );
		if ( pActivator )
			NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActivator->lNetID );
		else
			NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, -1 );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_ThingDeactivate( AActor *pActor, AActor *pActivator, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( !EnsureActorHasNetID (pActor) )
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

		SERVER_CheckClientBuffer( ulIdx, 5, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_THINGDEACTIVATE );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->lNetID );
		if ( pActivator )
			NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActivator->lNetID );
		else
			NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, -1 );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_RespawnDoomThing( AActor *pActor, bool bFog, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( !EnsureActorHasNetID (pActor) )
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

		SERVER_CheckClientBuffer( ulIdx, 4, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_RESPAWNDOOMTHING );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->lNetID );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, bFog );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_RespawnRavenThing( AActor *pActor, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( !EnsureActorHasNetID (pActor) )
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

		SERVER_CheckClientBuffer( ulIdx, 3, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_RESPAWNRAVENTHING );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->lNetID );
	}
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_SpawnBlood( fixed_t x, fixed_t y, fixed_t z, angle_t dir, int damage, AActor *originator, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG		ulIdx;

	if ( originator == NULL )
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

		SERVER_CheckClientBuffer( ulIdx, 12, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SPAWNBLOOD );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, x >> FRACBITS );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, y >> FRACBITS );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, z >> FRACBITS );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, dir >> FRACBITS );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, damage );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, originator->lNetID );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SpawnPuff( AActor *pActor, ULONG ulState, bool bSendTranslation, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG		ulIdx;
	USHORT		usActorNetworkIndex = 0;

	if ( pActor == NULL )
		return;

	usActorNetworkIndex = pActor->GetClass( )->getActorNetworkIndex();

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		if ( bSendTranslation )
			SERVER_CheckClientBuffer( ulIdx, 15, true );
		else
			SERVER_CheckClientBuffer( ulIdx, 11, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SPAWNPUFF );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->x >> FRACBITS );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->y >> FRACBITS );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->z >> FRACBITS );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, usActorNetworkIndex );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulState );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, !!bSendTranslation );
		if ( bSendTranslation )
			NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->Translation );
	}
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_Print( const char *pszString, ULONG ulPrintLevel, ULONG ulPlayerExtra, ULONG ulFlags )
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

		SERVER_CheckClientBuffer( ulIdx, 2 + (ULONG)strlen( pszString ), true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_PRINT );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulPrintLevel );
		NETWORK_WriteString( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pszString );
    }
}

//*****************************************************************************
//
void SERVERCOMMANDS_PrintMid( const char *pszString, bool bBold, ULONG ulPlayerExtra, ULONG ulFlags )
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

		SERVER_CheckClientBuffer( ulIdx, 2 + (ULONG)strlen( pszString ), true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_PRINTMID );
		NETWORK_WriteString( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pszString );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, !!bBold );
    }
}

//*****************************************************************************
//
void SERVERCOMMANDS_PrintMOTD( const char *pszString, ULONG ulPlayerExtra, ULONG ulFlags )
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

		SERVER_CheckClientBuffer( ulIdx, 1 + (ULONG)strlen( pszString ), true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_PRINTMOTD );
		NETWORK_WriteString( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pszString );
    }
}

//*****************************************************************************
//
void SERVERCOMMANDS_PrintHUDMessage( const char *pszString, float fX, float fY, LONG lHUDWidth, LONG lHUDHeight, LONG lColor, float fHoldTime, const char *pszFont, bool bLog, LONG lID, ULONG ulPlayerExtra, ULONG ulFlags )
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

		SERVER_CheckClientBuffer( ulIdx, 23 + (ULONG)strlen( pszString ) + (ULONG)strlen( pszFont ), true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_PRINTHUDMESSAGE );
		NETWORK_WriteString( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pszString );
		NETWORK_WriteFloat( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, fX );
		NETWORK_WriteFloat( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, fY );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lHUDWidth );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lHUDHeight );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lColor );
		NETWORK_WriteFloat( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, fHoldTime );
		NETWORK_WriteString( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pszFont );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, !!bLog );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lID );
    }
}

//*****************************************************************************
//
void SERVERCOMMANDS_PrintHUDMessageFadeOut( const char *pszString, float fX, float fY, LONG lHUDWidth, LONG lHUDHeight, LONG lColor, float fHoldTime, float fFadeOutTime, const char *pszFont, bool bLog, LONG lID, ULONG ulPlayerExtra, ULONG ulFlags )
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

		SERVER_CheckClientBuffer( ulIdx, 27 + (ULONG)strlen( pszString ) + (ULONG)strlen( pszFont ), true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_PRINTHUDMESSAGEFADEOUT );
		NETWORK_WriteString( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pszString );
		NETWORK_WriteFloat( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, fX );
		NETWORK_WriteFloat( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, fY );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lHUDWidth );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lHUDHeight );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lColor );
		NETWORK_WriteFloat( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, fHoldTime );
		NETWORK_WriteFloat( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, fFadeOutTime );
		NETWORK_WriteString( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pszFont );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, !!bLog );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lID );
    }
}

//*****************************************************************************
//
void SERVERCOMMANDS_PrintHUDMessageFadeInOut( const char *pszString, float fX, float fY, LONG lHUDWidth, LONG lHUDHeight, LONG lColor, float fHoldTime, float fFadeInTime, float fFadeOutTime, const char *pszFont, bool bLog, LONG lID, ULONG ulPlayerExtra, ULONG ulFlags )
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

		SERVER_CheckClientBuffer( ulIdx, 31 + (ULONG)strlen( pszString ) + (ULONG)strlen( pszFont ), true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_PRINTHUDMESSAGEFADEINOUT );
		NETWORK_WriteString( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pszString );
		NETWORK_WriteFloat( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, fX );
		NETWORK_WriteFloat( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, fY );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lHUDWidth );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lHUDHeight );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lColor );
		NETWORK_WriteFloat( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, fHoldTime );
		NETWORK_WriteFloat( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, fFadeInTime );
		NETWORK_WriteFloat( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, fFadeOutTime );
		NETWORK_WriteString( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pszFont );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, !!bLog );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lID );
    }
}

//*****************************************************************************
//
void SERVERCOMMANDS_PrintHUDMessageTypeOnFadeOut( const char *pszString, float fX, float fY, LONG lHUDWidth, LONG lHUDHeight, LONG lColor, float fTypeTime, float fHoldTime, float fFadeOutTime, const char *pszFont, bool bLog, LONG lID, ULONG ulPlayerExtra, ULONG ulFlags )
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

		SERVER_CheckClientBuffer( ulIdx, 31 + (ULONG)strlen( pszString ) + (ULONG)strlen( pszFont ), true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_PRINTHUDMESSAGETYPEONFADEOUT );
		NETWORK_WriteString( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pszString );
		NETWORK_WriteFloat( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, fX );
		NETWORK_WriteFloat( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, fY );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lHUDWidth );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lHUDHeight );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lColor );
		NETWORK_WriteFloat( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, fTypeTime );
		NETWORK_WriteFloat( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, fHoldTime );
		NETWORK_WriteFloat( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, fFadeOutTime );
		NETWORK_WriteString( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pszFont );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, !!bLog );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lID );
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

		SERVER_CheckClientBuffer( ulIdx, 4, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETGAMEMODE );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, GAMEMODE_GetCurrentMode( ));
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, instagib );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, buckshot );
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

		SERVER_CheckClientBuffer( ulIdx, 3, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETGAMESKILL );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, gameskill );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, botskill );
    }
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetGameDMFlags( ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;
	LONG	lDMFlags;

	lDMFlags = dmflags;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		SERVER_CheckClientBuffer( ulIdx, 13, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETGAMEDMFLAGS );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lDMFlags );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, dmflags2 );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, compatflags );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, compatflags2 );
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

		SERVER_CheckClientBuffer( ulIdx, 15, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETGAMEMODELIMITS );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, fraglimit );
		NETWORK_WriteFloat( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, timelimit );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pointlimit );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, duellimit );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, winlimit );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, wavelimit );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, sv_cheats );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, sv_fastweapons );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, sv_maxlives );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, sv_maxteams );
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

		SERVER_CheckClientBuffer( ulIdx, 3, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETGAMEENDLEVELDELAY );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulEndLevelDelay );
    }
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetGameModeState( ULONG ulState, ULONG ulCountdownTicks, ULONG ulPlayerExtra, ULONG ulFlags )
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

		SERVER_CheckClientBuffer( ulIdx, 4, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETGAMEMODESTATE );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulState );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulCountdownTicks );
    }
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetDuelNumDuels( ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;
	ULONG	ulNumDuels;

	ulNumDuels = DUEL_GetNumDuels( );

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		SERVER_CheckClientBuffer( ulIdx, 2, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETDUELNUMDUELS );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulNumDuels );
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

		SERVER_CheckClientBuffer( ulIdx, 5, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETLMSSPECTATORSETTINGS );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lmsspectatorsettings );
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

		SERVER_CheckClientBuffer( ulIdx, 5, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETLMSALLOWEDWEAPONS );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lmsallowedweapons );
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

		SERVER_CheckClientBuffer( ulIdx, 5, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETINVASIONNUMMONSTERSLEFT );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, INVASION_GetNumMonstersLeft( ));
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, INVASION_GetNumArchVilesLeft( ));
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

		SERVER_CheckClientBuffer( ulIdx, 2, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETINVASIONWAVE );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, INVASION_GetCurrentWave( ));
    }
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetSimpleCTFSTMode( ULONG ulPlayerExtra, ULONG ulFlags )
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

		SERVER_CheckClientBuffer( ulIdx, 2, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETSIMPLECTFSTMODE );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, !!TEAM_GetSimpleCTFSTMode( ));
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

		SERVER_CheckClientBuffer( ulIdx, 4, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_DOPOSSESSIONARTIFACTPICKEDUP );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulPlayer );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulTicks );
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

		SERVER_CheckClientBuffer( ulIdx, 1, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_DOPOSSESSIONARTIFACTDROPPED );
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

		SERVER_CheckClientBuffer( ulIdx, 2, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_DOGAMEMODEFIGHT );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulCurrentWave );
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

		SERVER_CheckClientBuffer( ulIdx, 3, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_DOGAMEMODECOUNTDOWN );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulTicks );
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

		SERVER_CheckClientBuffer( ulIdx, 2, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_DOGAMEMODEWINSEQUENCE );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulWinner );
    }
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetDominationState( ULONG ulPlayerExtra, ULONG ulFlags )
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

		unsigned int NumPoints = DOMINATION_NumPoints();
		unsigned int *PointOwners = DOMINATION_PointOwners();
		SERVER_CheckClientBuffer( ulIdx, NumPoints + 5, true);
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETDOMINATIONSTATE );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, NumPoints );
		for(unsigned int i = 0;i < NumPoints;i++)
		{
			//one byte should be enough to hold the value of the team.
			NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, PointOwners[i] );
		}
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetDominationPointOwnership( ULONG ulPoint, ULONG ulPlayer, ULONG ulPlayerExtra, ULONG ulFlags )
{
	for ( ULONG ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		SERVER_CheckClientBuffer( ulIdx, 3, true);
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETDOMINATIONPOINTOWNER );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulPoint );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulPlayer );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetTeamFrags( ULONG ulTeam, LONG lFrags, bool bAnnounce, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( TEAM_CheckIfValid( ulTeam ) == false )
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

		SERVER_CheckClientBuffer( ulIdx, 5, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETTEAMFRAGS );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulTeam );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lFrags );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, bAnnounce );
    }
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetTeamScore( ULONG ulTeam, LONG lScore, bool bAnnounce, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( TEAM_CheckIfValid( ulTeam ) == false )
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

		SERVER_CheckClientBuffer( ulIdx, 5, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETTEAMSCORE );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulTeam );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lScore );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, bAnnounce );
    }
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetTeamWins( ULONG ulTeam, LONG lWins, bool bAnnounce, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( TEAM_CheckIfValid( ulTeam ) == false )
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

		SERVER_CheckClientBuffer( ulIdx, 5, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETTEAMWINS );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulTeam );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lWins );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, bAnnounce );
    }
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetTeamReturnTicks( ULONG ulTeam, ULONG ulReturnTicks, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	// Allow NUM_TEAMS here, since we could be updating the return ticks of the white flag.
	if ( TEAM_CheckIfValid ( ulTeam ) == false )
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

		SERVER_CheckClientBuffer( ulIdx, 4, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETTEAMRETURNTICKS );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulTeam );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulReturnTicks );
    }
}

//*****************************************************************************
//
void SERVERCOMMANDS_TeamFlagReturned( ULONG ulTeam, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( TEAM_CheckIfValid ( ulTeam ) == false )
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

		SERVER_CheckClientBuffer( ulIdx, 2, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_TEAMFLAGRETURNED );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulTeam );
    }
}

//*****************************************************************************
//
void SERVERCOMMANDS_TeamFlagDropped( ULONG ulPlayer, ULONG ulTeam, ULONG ulPlayerExtra, ULONG ulFlags )
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

		SERVER_CheckClientBuffer( ulIdx, 2, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_TEAMFLAGDROPPED );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulPlayer );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulTeam );

    }
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_SpawnMissile( AActor *pMissile, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG		ulIdx;
	USHORT		usActorNetworkIndex = 0;

	if ( pMissile == NULL )
		return;

	usActorNetworkIndex = pMissile->GetClass( )->getActorNetworkIndex();

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		SERVER_CheckClientBuffer( ulIdx, 25, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SPAWNMISSILE );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pMissile->x >> FRACBITS );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pMissile->y >> FRACBITS );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pMissile->z >> FRACBITS );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pMissile->momx );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pMissile->momy );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pMissile->momz );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, usActorNetworkIndex );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pMissile->lNetID );
		if ( pMissile->target )
			NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pMissile->target->lNetID );
		else
			NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, -1 );
	}
	// [BB] It's possible that the angle can't be derived from the momentum
	// of the missle. In this case the correct angle has to be told to the clients.
 	if( pMissile->angle != R_PointToAngle2( 0, 0, pMissile->momx, pMissile->momy ) )
		SERVERCOMMANDS_SetThingAngle( pMissile, ulPlayerExtra, ulFlags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SpawnMissileExact( AActor *pMissile, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG		ulIdx;
	USHORT		usActorNetworkIndex = 0;

	if ( pMissile == NULL )
		return;

	usActorNetworkIndex = pMissile->GetClass( )->getActorNetworkIndex();

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		SERVER_CheckClientBuffer( ulIdx, 31, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SPAWNMISSILEEXACT );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pMissile->x );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pMissile->y );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pMissile->z );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pMissile->momx );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pMissile->momy );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pMissile->momz );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, usActorNetworkIndex );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pMissile->lNetID );
		if ( pMissile->target )
			NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pMissile->target->lNetID );
		else
			NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, -1 );
	}
	// [BB] It's possible that the angle can't be derived from the momentum
	// of the missle. In this case the correct angle has to be told to the clients.
 	if( pMissile->angle != R_PointToAngle2( 0, 0, pMissile->momx, pMissile->momy ) )
		SERVERCOMMANDS_SetThingAngleExact( pMissile, ulPlayerExtra, ulFlags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_MissileExplode( AActor *pMissile, line_t *pLine, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( !EnsureActorHasNetID (pMissile) )
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

		SERVER_CheckClientBuffer( ulIdx, 9, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_MISSILEEXPLODE );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pMissile->lNetID );
		if ( pLine )
			NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ULONG( pLine - lines ));
		else
			NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, -1 );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pMissile->x >> FRACBITS );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pMissile->y >> FRACBITS );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pMissile->z >> FRACBITS );
	}
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_WeaponSound( ULONG ulPlayer, const char *pszSound, ULONG ulPlayerExtra, ULONG ulFlags )
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

		SERVER_CheckClientBuffer( ulIdx, 2 + (ULONG)strlen( pszSound ), true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_WEAPONSOUND );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulPlayer );
		NETWORK_WriteString( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pszSound );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_WeaponChange( ULONG ulPlayer, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;
	USHORT	usActorNetworkIndex = 0;

	if ( SERVER_IsValidPlayer( ulPlayer ) == false )
		return;

	if ( players[ulPlayer].ReadyWeapon != NULL )
		usActorNetworkIndex = players[ulPlayer].ReadyWeapon->GetClass( )->getActorNetworkIndex();
	else
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

		SERVER_CheckClientBuffer( ulIdx, 4, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_WEAPONCHANGE );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulPlayer );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, usActorNetworkIndex );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_WeaponRailgun( AActor *pSource, const FVector3 &Start, const FVector3 &End, LONG lColor1, LONG lColor2, float fMaxDiff, bool bSilent, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	// Evidently, to draw a railgun trail, there must be a source actor.
	if ( !EnsureActorHasNetID (pSource) )
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

		SERVER_CheckClientBuffer( ulIdx, 40, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_WEAPONRAILGUN );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pSource->lNetID );
		NETWORK_WriteFloat( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, Start.X );
		NETWORK_WriteFloat( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, Start.Y );
		NETWORK_WriteFloat( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, Start.Z );
		NETWORK_WriteFloat( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, End.X );
		NETWORK_WriteFloat( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, End.Y );
		NETWORK_WriteFloat( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, End.Z );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lColor1 );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lColor2 );
		NETWORK_WriteFloat( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, fMaxDiff );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, bSilent );
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

		SERVER_CheckClientBuffer( ulIdx, 5, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETSECTORFLOORPLANE );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulSector );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, sectors[ulSector].floorplane.d >> FRACBITS );
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

		SERVER_CheckClientBuffer( ulIdx, 5, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETSECTORCEILINGPLANE );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulSector );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, sectors[ulSector].ceilingplane.d >> FRACBITS );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetSectorFloorPlaneSlope( ULONG ulSector, ULONG ulPlayerExtra, ULONG ulFlags )
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

		SERVER_CheckClientBuffer( ulIdx, 11, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETSECTORFLOORPLANESLOPE );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulSector );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, sectors[ulSector].floorplane.a >> FRACBITS );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, sectors[ulSector].floorplane.b >> FRACBITS );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, sectors[ulSector].floorplane.c >> FRACBITS );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, sectors[ulSector].floorplane.ic >> FRACBITS );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetSectorCeilingPlaneSlope( ULONG ulSector, ULONG ulPlayerExtra, ULONG ulFlags )
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

		SERVER_CheckClientBuffer( ulIdx, 11, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETSECTORCEILINGPLANESLOPE );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulSector );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, sectors[ulSector].ceilingplane.a >> FRACBITS );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, sectors[ulSector].ceilingplane.b >> FRACBITS );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, sectors[ulSector].ceilingplane.c >> FRACBITS );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, sectors[ulSector].ceilingplane.ic >> FRACBITS );
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

		SERVER_CheckClientBuffer( ulIdx, 4, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETSECTORLIGHTLEVEL );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulSector );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, sectors[ulSector].lightlevel );
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

		SERVER_CheckClientBuffer( ulIdx, 7, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETSECTORCOLOR );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulSector );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, sectors[ulSector].ColorMap->Color.r );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, sectors[ulSector].ColorMap->Color.g );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, sectors[ulSector].ColorMap->Color.b );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, sectors[ulSector].ColorMap->Desaturate );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetSectorColorByTag( ULONG ulTag, ULONG ulRed, ULONG ulGreen, ULONG ulBlue, ULONG ulDesaturate, ULONG ulPlayerExtra, ULONG ulFlags )
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

		SERVER_CheckClientBuffer( ulIdx, 7, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETSECTORCOLORBYTAG );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulTag );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulRed );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulGreen );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulBlue );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulDesaturate );
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

		SERVER_CheckClientBuffer( ulIdx, 6, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETSECTORFADE );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulSector );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, sectors[ulSector].ColorMap->Fade.r );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, sectors[ulSector].ColorMap->Fade.g );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, sectors[ulSector].ColorMap->Fade.b );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetSectorFadeByTag( ULONG ulTag, ULONG ulRed, ULONG ulGreen, ULONG ulBlue, ULONG ulPlayerExtra, ULONG ulFlags )
{
	for ( ULONG	ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		SERVER_CheckClientBuffer( ulIdx, 6, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETSECTORFADEBYTAG );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulTag );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulRed );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulGreen );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulBlue );
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

		SERVER_CheckClientBuffer( ulIdx, 3 + (ULONG)strlen( TexMan( sectors[ulSector].GetTexture(sector_t::floor) )->Name ) + (ULONG)strlen( TexMan( sectors[ulSector].GetTexture(sector_t::ceiling) )->Name ), true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETSECTORFLAT );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulSector );
		NETWORK_WriteString( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, TexMan( sectors[ulSector].GetTexture(sector_t::ceiling) )->Name );
		NETWORK_WriteString( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, TexMan( sectors[ulSector].GetTexture(sector_t::floor) )->Name );
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

		SERVER_CheckClientBuffer( ulIdx, 7, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETSECTORPANNING );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulSector );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, sectors[ulSector].GetXOffset(sector_t::ceiling) / FRACUNIT );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, sectors[ulSector].GetYOffset(sector_t::ceiling) / FRACUNIT );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, sectors[ulSector].GetXOffset(sector_t::floor) / FRACUNIT );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, sectors[ulSector].GetYOffset(sector_t::floor) / FRACUNIT );
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

		SERVER_CheckClientBuffer( ulIdx, 7, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETSECTORROTATION );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulSector );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ( sectors[ulSector].GetAngle(sector_t::ceiling) / ANGLE_1 ));
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ( sectors[ulSector].GetAngle(sector_t::floor) / ANGLE_1 ));
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

		SERVER_CheckClientBuffer( ulIdx, 11, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETSECTORSCALE );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulSector );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ( sectors[ulSector].GetXScale(sector_t::ceiling) / FRACBITS ));
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ( sectors[ulSector].GetYScale(sector_t::ceiling) / FRACBITS ));
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ( sectors[ulSector].GetXScale(sector_t::floor) / FRACBITS ));
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ( sectors[ulSector].GetYScale(sector_t::floor) / FRACBITS ));
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetSectorSpecial( ULONG ulSector, ULONG ulPlayerExtra, ULONG ulFlags )
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

		SERVER_CheckClientBuffer( ulIdx, 5, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETSECTORSPECIAL );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulSector );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, sectors[ulSector].special );
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

		SERVER_CheckClientBuffer( ulIdx, 11, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETSECTORFRICTION );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulSector );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, sectors[ulSector].friction );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, sectors[ulSector].movefactor );
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

		SERVER_CheckClientBuffer( ulIdx, 19, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETSECTORANGLEYOFFSET );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulSector );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, sectors[ulSector].planes[sector_t::ceiling].xform.base_angle );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, sectors[ulSector].planes[sector_t::ceiling].xform.base_yoffs );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, sectors[ulSector].planes[sector_t::floor].xform.base_angle );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, sectors[ulSector].planes[sector_t::floor].xform.base_yoffs );
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

		SERVER_CheckClientBuffer( ulIdx, 7, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETSECTORGRAVITY );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulSector );
		NETWORK_WriteFloat( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, sectors[ulSector].gravity );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetSectorReflection( ULONG ulSector, ULONG ulPlayerExtra, ULONG ulFlags )
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

		SERVER_CheckClientBuffer( ulIdx, 11, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETSECTORREFLECTION );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulSector );
		NETWORK_WriteFloat( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, sectors[ulSector].ceiling_reflect );
		NETWORK_WriteFloat( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, sectors[ulSector].floor_reflect );
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

		SERVER_CheckClientBuffer( ulIdx, 3, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_STOPSECTORLIGHTEFFECT );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulSector );
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

		SERVER_CheckClientBuffer( ulIdx, 1, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_DESTROYALLSECTORMOVERS );
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

		SERVER_CheckClientBuffer( ulIdx, 5, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_DOSECTORLIGHTFIREFLICKER );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulSector );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lMaxLight );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lMinLight );
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

		SERVER_CheckClientBuffer( ulIdx, 3, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_DOSECTORLIGHTFLICKER );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulSector );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lMaxLight );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lMinLight );
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

		SERVER_CheckClientBuffer( ulIdx, 5, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_DOSECTORLIGHTLIGHTFLASH );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulSector );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lMaxLight );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lMinLight );
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

		SERVER_CheckClientBuffer( ulIdx, 10, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_DOSECTORLIGHTSTROBE );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulSector );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lDarkTime );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lBrightTime );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lMaxLight );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lMinLight );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lCount );
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

		SERVER_CheckClientBuffer( ulIdx, 3, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_DOSECTORLIGHTGLOW );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulSector );
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

		SERVER_CheckClientBuffer( ulIdx, 10, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_DOSECTORLIGHTGLOW2 );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulSector );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lStart );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lEnd );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lTics );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lMaxTics );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, bOneShot );
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

		SERVER_CheckClientBuffer( ulIdx, 5, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_DOSECTORLIGHTPHASED );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulSector );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lBaseLevel );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lPhase );
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

		SERVER_CheckClientBuffer( ulIdx, 7, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETLINEALPHA );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulLine );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lines[ulLine].Alpha );
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
			SERVER_CheckClientBuffer( ulIdx, 5 + 8, true );
			NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETLINETEXTURE );
			NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulLine );
			NETWORK_WriteString( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, sides[lines[ulLine].sidenum[0]].GetTexture(side_t::top).isValid() ? TexMan[sides[lines[ulLine].sidenum[0]].GetTexture(side_t::top)]->Name : "-" );
			NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, 0 );
			NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, 0 );
		}

		if ( lines[ulLine].ulTexChangeFlags & TEXCHANGE_FRONTMEDIUM )
		{
			SERVER_CheckClientBuffer( ulIdx, 5 + 8, true );
			NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETLINETEXTURE );
			NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulLine );
			NETWORK_WriteString( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, sides[lines[ulLine].sidenum[0]].GetTexture(side_t::mid).isValid() ? TexMan[sides[lines[ulLine].sidenum[0]].GetTexture(side_t::mid)]->Name : "-" );
			NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, 0 );
			NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, 1 );
		}

		if ( lines[ulLine].ulTexChangeFlags & TEXCHANGE_FRONTBOTTOM )
		{
			SERVER_CheckClientBuffer( ulIdx, 5 + 8, true );
			NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETLINETEXTURE );
			NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulLine );
			NETWORK_WriteString( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, sides[lines[ulLine].sidenum[0]].GetTexture(side_t::bottom).isValid() ? TexMan[sides[lines[ulLine].sidenum[0]].GetTexture(side_t::bottom)]->Name : "-" );
			NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, 0 );
			NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, 2 );
		}

		if (( lines[ulLine].sidenum[1] == NO_SIDE ) || ( lines[ulLine].sidenum[1] >= (ULONG)numsides ))
			continue;

		if ( lines[ulLine].ulTexChangeFlags & TEXCHANGE_BACKTOP )
		{
			SERVER_CheckClientBuffer( ulIdx, 5 + 8, true );
			NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETLINETEXTURE );
			NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulLine );
			NETWORK_WriteString( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, sides[lines[ulLine].sidenum[1]].GetTexture(side_t::top).isValid() ? TexMan[sides[lines[ulLine].sidenum[1]].GetTexture(side_t::top)]->Name : "-" );
			NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, 1 );
			NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, 0 );
		}

		if ( lines[ulLine].ulTexChangeFlags & TEXCHANGE_BACKMEDIUM )
		{
			SERVER_CheckClientBuffer( ulIdx, 5 + 8, true );
			NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETLINETEXTURE );
			NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulLine );
			NETWORK_WriteString( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, sides[lines[ulLine].sidenum[1]].GetTexture(side_t::mid).isValid() ? TexMan[sides[lines[ulLine].sidenum[1]].GetTexture(side_t::mid)]->Name : "-" );
			NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, 1 );
			NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, 1 );
		}

		if ( lines[ulLine].ulTexChangeFlags & TEXCHANGE_BACKBOTTOM )
		{
			SERVER_CheckClientBuffer( ulIdx, 5 + 8, true );
			NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETLINETEXTURE );
			NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulLine );
			NETWORK_WriteString( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, sides[lines[ulLine].sidenum[1]].GetTexture(side_t::bottom).isValid() ? TexMan[sides[lines[ulLine].sidenum[1]].GetTexture(side_t::bottom)]->Name : "-" );
			NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, 1 );
			NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, 2 );
		}
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetLineTextureByID( ULONG ulLineID, ULONG ulSide, ULONG ulPosition, const char *pszTexName, ULONG ulPlayerExtra, ULONG ulFlags )
{
	for ( ULONG	ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		SERVER_CheckClientBuffer( ulIdx, 5 + (ULONG)strlen(pszTexName) , true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETLINETEXTUREBYID );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulLineID );
		NETWORK_WriteString( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pszTexName );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulSide );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulPosition );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetSomeLineFlags( ULONG ulLine, ULONG ulPlayerExtra, ULONG ulFlags )
{
	LONG	lSelectedFlags;
	ULONG	ulIdx;

	if ( ulLine >= (ULONG)numlines )
		return;

	// I bet there's a better way to do this... alas, my knowledge of bits is not
	// wonderful.
	lSelectedFlags = 0;
	if ( lines[ulLine].flags & ML_BLOCKING )
		lSelectedFlags |= ML_BLOCKING;
	if ( lines[ulLine].flags & ML_BLOCKEVERYTHING )
		lSelectedFlags |= ML_BLOCKEVERYTHING;
	if ( lines[ulLine].flags & ML_RAILING )
		lSelectedFlags |= ML_RAILING;
	if ( lines[ulLine].flags & ML_BLOCK_PLAYERS )
		lSelectedFlags |= ML_BLOCK_PLAYERS;
	if ( lines[ulLine].flags & ML_ADDTRANS )
		lSelectedFlags |= ML_ADDTRANS;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		SERVER_CheckClientBuffer( ulIdx, 7, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETSOMELINEFLAGS );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulLine );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lSelectedFlags );
	}
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_ACSScriptExecute( ULONG ulScript, AActor *pActivator, LONG lLineIdx, char *pszMap, bool bBackSide, ULONG ulArg0, ULONG ulArg1, ULONG ulArg2, bool bAlways, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;
	LONG	lActivatorID;

	if ( pActivator == NULL )
		lActivatorID = -1;
	else
		lActivatorID = pActivator->lNetID;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		SERVER_CheckClientBuffer( ulIdx, 12 + (ULONG)strlen( pszMap ), true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_ACSSCRIPTEXECUTE );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulScript );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lActivatorID );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lLineIdx );
		NETWORK_WriteString( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pszMap );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, bBackSide );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulArg0 );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulArg1 );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulArg2 );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, bAlways );
	}
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_SetSideFlags( ULONG ulSide, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( ulSide >= (ULONG)numsides )
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

		SERVER_CheckClientBuffer( ulIdx, 4, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETSIDEFLAGS );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulSide );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, sides[ulSide].Flags );
	}
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_Sound( LONG lChannel, const char *pszSound, float fVolume, float fAttenuation, ULONG ulPlayerExtra, ULONG ulFlags )
{
	LONG lAttenuation = NETWORK_AttenuationFloatToInt ( fAttenuation );

	for ( ULONG ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		SERVER_CheckClientBuffer( ulIdx, 4 + (ULONG)strlen( pszSound ), true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SOUND );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lChannel );
		NETWORK_WriteString( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pszSound );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, LONG( fVolume * 127 ));
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lAttenuation );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SoundActor( AActor *pActor, LONG lChannel, const char *pszSound, float fVolume, float fAttenuation, ULONG ulPlayerExtra, ULONG ulFlags, bool bRespectActorPlayingSomething )
{
	if ( pActor == NULL )
		return;

	// [BB] If the actor doesn't have a NetID, we have to instruct the clients differently how to play the sound.
	if ( pActor->lNetID == -1 )
	{
		SERVERCOMMANDS_SoundPoint( pActor->x, pActor->y, pActor->z, lChannel, pszSound, fVolume, fAttenuation, ulPlayerExtra, ulFlags );
		return;
	}

	LONG lAttenuation = NETWORK_AttenuationFloatToInt ( fAttenuation );

	for ( ULONG ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		SERVER_CheckClientBuffer( ulIdx, 6 + (ULONG)strlen( pszSound ), true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, bRespectActorPlayingSomething ? SVC_SOUNDACTORIFNOTPLAYING : SVC_SOUNDACTOR );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->lNetID );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lChannel );
		NETWORK_WriteString( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pszSound );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, LONG( fVolume * 127 ));
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lAttenuation );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SoundPoint( LONG lX, LONG lY, LONG lZ, LONG lChannel, const char *pszSound, float fVolume, float fAttenuation, ULONG ulPlayerExtra, ULONG ulFlags )
{
	LONG lAttenuation = NETWORK_AttenuationFloatToInt ( fAttenuation );

	for ( ULONG ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		SERVER_CheckClientBuffer( ulIdx, 10 + (ULONG)strlen( pszSound ), true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SOUNDPOINT );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lX >> FRACBITS );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lY >> FRACBITS );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lZ >> FRACBITS );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lChannel );
		NETWORK_WriteString( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pszSound );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, LONG( fVolume * 127 ));
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lAttenuation );
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

		SERVER_CheckClientBuffer( ulIdx, 3 + (ULONG)strlen( pszSequence ), true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_STARTSECTORSEQUENCE );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lSectorID );
		NETWORK_WriteString( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pszSequence );
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

		SERVER_CheckClientBuffer( ulIdx, 3, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_STOPSECTORSEQUENCE );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lSectorID );
	}
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_CallVote( ULONG ulPlayer, FString Command, FString Parameters, FString Reason, ULONG ulPlayerExtra, ULONG ulFlags )
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

		SERVER_CheckClientBuffer( ulIdx, 2 + ULONG( Command.Len() ) + ULONG ( Parameters.Len() ), true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_CALLVOTE );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulPlayer );
		NETWORK_WriteString( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, Command.GetChars() );
		NETWORK_WriteString( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, Parameters.GetChars() );
		NETWORK_WriteString( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, Reason.GetChars() );
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

		SERVER_CheckClientBuffer( ulIdx, 3, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_PLAYERVOTE );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulPlayer );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, bVoteYes );
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

		SERVER_CheckClientBuffer( ulIdx, 2, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_VOTEENDED );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, bVotePassed );
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

		SERVER_CheckClientBuffer( ulIdx, 1 + (ULONG)strlen( level.mapname ), true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_MAPLOAD );
		NETWORK_WriteString( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, level.mapname );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_MapNew( const char *pszMapName, ULONG ulPlayerExtra, ULONG ulFlags )
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

		SERVER_CheckClientBuffer( ulIdx, 1 + (ULONG)strlen( pszMapName ), true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_MAPNEW );
		NETWORK_WriteString( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pszMapName );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_MapExit( LONG lPosition, const char *pszNextMap, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( pszNextMap == NULL )
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

		SERVER_CheckClientBuffer( ulIdx, 2 + (ULONG)strlen( pszNextMap ), true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_MAPEXIT );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lPosition );
		NETWORK_WriteString( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pszNextMap );
	}

	// [BB] The clients who are authenticated, but still didn't finish loading
	// the map are not covered by the code above and need special treatment.
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_GetClient( ulIdx )->State == CLS_AUTHENTICATED )
			SERVER_GetClient( ulIdx )->State = CLS_AUTHENTICATED_BUT_OUTDATED_MAP;
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_MapAuthenticate( const char *pszMapName, ULONG ulPlayerExtra, ULONG ulFlags )
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

		SERVER_CheckClientBuffer( ulIdx, 1 + (ULONG)strlen( pszMapName ), true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_MAPAUTHENTICATE );
		NETWORK_WriteString( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pszMapName );
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

		SERVER_CheckClientBuffer( ulIdx, 5, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETMAPTIME );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, level.time );
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

		SERVER_CheckClientBuffer( ulIdx, 3, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETMAPNUMKILLEDMONSTERS );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, level.killed_monsters );
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

		SERVER_CheckClientBuffer( ulIdx, 3, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETMAPNUMFOUNDITEMS );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, level.found_items );
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

		SERVER_CheckClientBuffer( ulIdx, 3, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETMAPNUMFOUNDSECRETS );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, level.found_secrets );
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

		SERVER_CheckClientBuffer( ulIdx, 3, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETMAPNUMTOTALMONSTERS );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, level.total_monsters );
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

		SERVER_CheckClientBuffer( ulIdx, 3, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETMAPNUMTOTALITEMS );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, level.total_items );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetMapMusic( const char *pszMusic, ULONG ulPlayerExtra, ULONG ulFlags )
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

		SERVER_CheckClientBuffer( ulIdx, 1 + (ULONG)strlen( pszMusic ), true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETMAPMUSIC );
		NETWORK_WriteString( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pszMusic );
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

		SERVER_CheckClientBuffer( ulIdx, 1 + (ULONG)strlen( level.skypic1 ) + (ULONG)strlen( level.skypic2 ), true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETMAPSKY );
		NETWORK_WriteString( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, level.skypic1 );
		NETWORK_WriteString( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, level.skypic2 );
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

		SERVER_CheckClientBuffer( ulIdx, 6, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_GIVEINVENTORY );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulPlayer );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pInventory->GetClass( )->getActorNetworkIndex() );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pInventory->Amount );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_GiveInventoryNotOverwritingAmount( AActor *pReceiver, AInventory *pItem )
{
	if( (pItem == NULL) || (pReceiver == NULL) || (pReceiver->player == NULL) )
		return;
	
	// [BB] Since SERVERCOMMANDS_GiveInventory overwrites the item amount
	// of the client with pItem->Amount, we have have to set this to the
	// correct amount the player has.
	AInventory *pInventory = NULL;
	if( pReceiver->player && pReceiver->player->mo )
		pInventory = pReceiver->player->mo->FindInventory( pItem->GetClass() );
	if ( pInventory != NULL )
		pItem->Amount = pInventory->Amount;

	SERVERCOMMANDS_GiveInventory( ULONG( pReceiver->player - players ), pItem );
	// [BB] The armor display amount has to be updated separately.
	if( pItem->GetClass()->IsDescendantOf (RUNTIME_CLASS(AArmor)))
		SERVERCOMMANDS_SetPlayerArmor( ULONG( pReceiver->player - players ));
}
//*****************************************************************************
//
void SERVERCOMMANDS_TakeInventory( ULONG ulPlayer, const char *pszClassName, ULONG ulAmount, ULONG ulPlayerExtra, ULONG ulFlags )
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

		SERVER_CheckClientBuffer( ulIdx, 4 + (ULONG)strlen( pszClassName ), true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_TAKEINVENTORY );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulPlayer );
		NETWORK_WriteString( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pszClassName );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulAmount );
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

		SERVER_CheckClientBuffer( ulIdx, 6, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_GIVEPOWERUP );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulPlayer );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pPowerup->GetClass( )->getActorNetworkIndex() );
		// Can we have multiple amounts of a powerup? Probably not, but I'll be safe for now.
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pPowerup->Amount );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pPowerup->EffectTics );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_DoInventoryPickup( ULONG ulPlayer, const char *pszClassName, const char *pszPickupMessage, ULONG ulPlayerExtra, ULONG ulFlags )
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

		SERVER_CheckClientBuffer( ulIdx, 2 + lLength, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_DOINVENTORYPICKUP );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulPlayer );
		NETWORK_WriteString( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pszClassName );
		NETWORK_WriteString( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pszPickupMessage );
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

		SERVER_CheckClientBuffer( ulIdx, 2, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_DESTROYALLINVENTORY );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulPlayer );
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

		SERVER_CheckClientBuffer( ulIdx, 12, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_DODOOR );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lSectorID );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lSpeed );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lDirection );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lLightTag );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lID );
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

		SERVER_CheckClientBuffer( ulIdx, 3, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_DESTROYDOOR );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lID );
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

		SERVER_CheckClientBuffer( ulIdx, 4, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_CHANGEDOORDIRECTION );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lID );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lDirection );
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

		SERVER_CheckClientBuffer( ulIdx, 15, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_DOFLOOR );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, (ULONG)Type );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lSectorID );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lDirection );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lSpeed );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lFloorDestDist );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lID );
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

		SERVER_CheckClientBuffer( ulIdx, 3, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_DESTROYFLOOR );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lID );
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

		SERVER_CheckClientBuffer( ulIdx, 4, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_CHANGEFLOORDIRECTION );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lID );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lDirection );
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

		SERVER_CheckClientBuffer( ulIdx, 4, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_CHANGEFLOORTYPE );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lID );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, (ULONG)Type );
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

		SERVER_CheckClientBuffer( ulIdx, 7, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_CHANGEFLOORDESTDIST );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lID );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lFloorDestDist );
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

		SERVER_CheckClientBuffer( ulIdx, 3, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_STARTFLOORSOUND );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lID );
	}
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_DoCeiling( DCeiling::ECeiling Type, sector_t *pSector, LONG lDirection, LONG lBottomHeight, LONG lTopHeight, LONG lSpeed, LONG lCrush, LONG lSilent, LONG lID, ULONG ulPlayerExtra, ULONG ulFlags )
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

		SERVER_CheckClientBuffer( ulIdx, 23, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_DOCEILING );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, (ULONG)Type );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lSectorID );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lDirection );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lBottomHeight );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lTopHeight );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lSpeed );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lCrush );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lSilent );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lID );
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

		SERVER_CheckClientBuffer( ulIdx, 3, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_DESTROYCEILING );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lID );
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

		SERVER_CheckClientBuffer( ulIdx, 4, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_CHANGECEILINGDIRECTION );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lID );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lDirection );
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

		SERVER_CheckClientBuffer( ulIdx, 7, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_CHANGECEILINGSPEED );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lID );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lSpeed );
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

		SERVER_CheckClientBuffer( ulIdx, 3, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_PLAYCEILINGSOUND );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lID );
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

		SERVER_CheckClientBuffer( ulIdx, 19, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_DOPLAT );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, (ULONG)Type );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lSectorID );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, (ULONG)Status );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lHigh );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lLow );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lSpeed );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lID );
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

		SERVER_CheckClientBuffer( ulIdx, 3, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_DESTROYPLAT );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lID );
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

		SERVER_CheckClientBuffer( ulIdx, 4, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_CHANGEPLATSTATUS );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lID );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, (ULONG)Status );
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

		SERVER_CheckClientBuffer( ulIdx, 4, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_PLAYPLATSOUND );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lID );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lSound );
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

		SERVER_CheckClientBuffer( ulIdx, 13, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_DOELEVATOR );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, Type );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lSectorID );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lSpeed );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lDirection );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lFloorDestDist );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lCeilingDestDist );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lID );
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

		SERVER_CheckClientBuffer( ulIdx, 3, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_DESTROYELEVATOR );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lID );
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

		SERVER_CheckClientBuffer( ulIdx, 3, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_STARTELEVATORSOUND );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lID );
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

		SERVER_CheckClientBuffer( ulIdx, 22, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_DOPILLAR );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, Type );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lSectorID );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lFloorSpeed );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lCeilingSpeed );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lFloorTarget );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lCeilingTarget );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lID );
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

		SERVER_CheckClientBuffer( ulIdx, 3, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_DESTROYPILLAR );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lID );
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

		SERVER_CheckClientBuffer( ulIdx, 35, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_DOWAGGLE );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, !!bCeiling );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lSectorID );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lOriginalDistance );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lAccumulator );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lAccelerationDelta );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lTargetScale );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lScale );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lScaleDelta );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lTicker );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lState );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lID );
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

		SERVER_CheckClientBuffer( ulIdx, 3, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_DESTROYWAGGLE );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lID );
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

		SERVER_CheckClientBuffer( ulIdx, 7, false );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->UnreliablePacketBuffer.ByteStream, SVC_UPDATEWAGGLE );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->UnreliablePacketBuffer.ByteStream, lID );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->UnreliablePacketBuffer.ByteStream, lAccumulator );
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

		SERVER_CheckClientBuffer( ulIdx, 7, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_DOROTATEPOLY );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lSpeed );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lPolyNum );
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

		SERVER_CheckClientBuffer( ulIdx, 3, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_DESTROYROTATEPOLY );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lPolyNum );
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

		SERVER_CheckClientBuffer( ulIdx, 11, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_DOMOVEPOLY );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lXSpeed );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lYSpeed );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lPolyNum );
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

		SERVER_CheckClientBuffer( ulIdx, 3, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_DESTROYMOVEPOLY );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lPolyNum );
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

		SERVER_CheckClientBuffer( ulIdx, 16, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_DOPOLYDOOR );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lType );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lXSpeed );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lYSpeed );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lSpeed );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lPolyNum );
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

		SERVER_CheckClientBuffer( ulIdx, 3, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_DESTROYPOLYDOOR );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lPolyNum );
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

		SERVER_CheckClientBuffer( ulIdx, 19, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETPOLYDOORSPEEDPOSITION );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lPolyNum );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lXSpeed );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lYSpeed );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lX );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lY );
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

		SERVER_CheckClientBuffer( ulIdx, 4, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_PLAYPOLYOBJSOUND );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lPolyNum );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, (ULONG)Sound );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPolyobjPosition( LONG lPolyNum, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG		ulIdx;
	FPolyObj	*pPoly;

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

		SERVER_CheckClientBuffer( ulIdx, 11, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETPOLYOBJPOSITION );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lPolyNum );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pPoly->startSpot[0] );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pPoly->startSpot[1] );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPolyobjRotation( LONG lPolyNum, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG		ulIdx;
	FPolyObj	*pPoly;

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

		SERVER_CheckClientBuffer( ulIdx, 7, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETPOLYOBJROTATION );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lPolyNum );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pPoly->angle );
	}
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_Earthquake( AActor *pCenter, LONG lIntensity, LONG lDuration, LONG lTemorRadius, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( !EnsureActorHasNetID (pCenter) )
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

		SERVER_CheckClientBuffer( ulIdx, 6, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_EARTHQUAKE );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pCenter->lNetID );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lIntensity );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lDuration );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lTemorRadius );
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
			SERVER_CheckClientBuffer( ulIdx, 2, true );
			NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETQUEUEPOSITION );
			NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lPosition );
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

		// [BB] Shouldn't the 8 be a 10? (1+1+4+4+2)
		// [BC] Should be 12 :)
		SERVER_CheckClientBuffer( ulIdx, 12, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_DOSCROLLER );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lType );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lXSpeed );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lYSpeed );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lAffectee );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetScroller( LONG lType, LONG lXSpeed, LONG lYSpeed, LONG lTag, ULONG ulPlayerExtra, ULONG ulFlags )
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

		// [BB] Shouldn't the 8 be a 10? (1+1+4+4+2)
		SERVER_CheckClientBuffer( ulIdx, 8, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETSCROLLER );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lType );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lXSpeed );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lYSpeed );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lTag );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetWallScroller( LONG lId, LONG lSidechoice, LONG lXSpeed, LONG lYSpeed, LONG lWhere, ULONG ulPlayerExtra, ULONG ulFlags )
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

		SERVER_CheckClientBuffer( ulIdx, 18, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETWALLSCROLLER );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lId );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lSidechoice );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lXSpeed );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lYSpeed );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lWhere );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_DoFlashFader( float fR1, float fG1, float fB1, float fA1, float fR2, float fG2, float fB2, float fA2, float fTime, ULONG ulPlayer, ULONG ulPlayerExtra, ULONG ulFlags )
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

		SERVER_CheckClientBuffer( ulIdx, 38, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_DOFLASHFADER );
		NETWORK_WriteFloat( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, fR1 );
		NETWORK_WriteFloat( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, fG1 );
		NETWORK_WriteFloat( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, fB1 );
		NETWORK_WriteFloat( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, fA1 );
		NETWORK_WriteFloat( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, fR2 );
		NETWORK_WriteFloat( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, fG2 );
		NETWORK_WriteFloat( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, fB2 );
		NETWORK_WriteFloat( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, fA2 );
		NETWORK_WriteFloat( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, fTime );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulPlayer);
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

		SERVER_CheckClientBuffer( ulIdx, 3, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_GENERICCHEAT );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulPlayer );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulCheat );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetCameraToTexture( AActor *pCamera, char *pszTexture, LONG lFOV, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ((!EnsureActorHasNetID (pCamera) ) ||
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

		SERVER_CheckClientBuffer( ulIdx, 3 + (ULONG)strlen( pszTexture ), true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETCAMERATOTEXTURE );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pCamera->lNetID );
		NETWORK_WriteString( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pszTexture );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lFOV );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_CreateTranslation( ULONG ulTranslation, ULONG ulStart, ULONG ulEnd, ULONG ulPal1, ULONG ulPal2, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;
	FRemapTable	*pTranslation;

	
	pTranslation = translationtables[TRANSLATION_LevelScripted].GetVal(ulTranslation - 1);
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		SERVER_CheckClientBuffer( ulIdx, 7, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_CREATETRANSLATION );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulTranslation );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulStart );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulEnd );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulPal1 );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulPal2 );
	}
}
