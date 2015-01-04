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
#include "p_acs.h"
#include "templates.h"
#include "a_movingcamera.h"

CVAR (Bool, sv_showwarnings, false, CVAR_GLOBALCONFIG|CVAR_ARCHIVE)

// :((((((
FPolyObj	*GetPolyobj( int polyNum );

EXTERN_CVAR( Float, sv_aircontrol )

//*****************************************************************************
//	CLASSES

/**
 * \brief Iterate over all clients, possibly skipping one or all but one.
 *
 * \author Benjamin Berkels
 */
class ClientIterator {
	const ULONG _ulPlayerExtra;
	const ULONG _ulFlags;
	ULONG _current;

	bool isCurrentValid ( ) const {
		if ( SERVER_IsValidClient( _current ) == false )
			return false;

		if ((( _ulFlags & SVCF_SKIPTHISCLIENT ) && ( _ulPlayerExtra == _current )) ||
			(( _ulFlags & SVCF_ONLYTHISCLIENT ) && ( _ulPlayerExtra != _current )))
		{
			return false;
		}

		if ( ( _ulFlags & SVCF_ONLY_CONNECTIONTYPE_0 ) && ( players[_current].userinfo.ulConnectionType != 0 ) )
			return false;

		if ( ( _ulFlags & SVCF_ONLY_CONNECTIONTYPE_1 ) && ( players[_current].userinfo.ulConnectionType != 1 ) )
			return false;

		return true;
	}

	void incremntCurrentTillValid ( ) {
		while ( ( isCurrentValid() == false ) && notAtEnd() )
			++_current;
	}

public:
  ClientIterator ( const ULONG ulPlayerExtra = MAXPLAYERS, const ULONG ulFlags = 0 )
		: _ulPlayerExtra ( ulPlayerExtra ),
			_ulFlags ( ulFlags ),
			_current ( 0 )
	{
		incremntCurrentTillValid();
	}

	inline bool notAtEnd ( ) const {
		return ( _current < MAXPLAYERS );
	}

	const ULONG operator* ( ) const {
		return ( _current );
	}

	const ULONG operator++ ( ) {
		++_current;
		incremntCurrentTillValid();
		return ( _current );
	}
};

/**
 * \brief Creates and sends network commands to the clients.
 *
 * \author Benjamin Berkels
 */
class NetCommand {
	NETBUFFER_s _buffer;
public:
	NetCommand ( const int Header )
	{
		NETWORK_InitBuffer( &_buffer, MAX_UDP_PACKET, BUFFERTYPE_WRITE );
		NETWORK_ClearBuffer( &_buffer );
		addInteger<BYTE> ( Header );
	}
	~NetCommand ( )
	{
		NETWORK_FreeBuffer ( &_buffer );
	}

	template <typename IntType>
	void addInteger( const IntType IntValue )
	{
		if ( ( _buffer.ByteStream.pbStream + sizeof ( IntType ) ) > _buffer.ByteStream.pbStreamEnd )
		{
			Printf( "NetCommand::AddInteger: Overflow!\n" );
			return;
		}

		for ( int i = 0; i < (int)sizeof ( IntType ); ++i )
			_buffer.ByteStream.pbStream[i] = ( IntValue >> ( 8*i ) ) & 0xff;

		_buffer.ByteStream.pbStream += sizeof ( IntType );
		_buffer.ulCurrentSize = NETWORK_CalcBufferSize ( &_buffer );
	}

	void addByte ( const int ByteValue )
	{
		addInteger<BYTE> ( static_cast<BYTE> ( ByteValue ) );
	}

	void addShort ( const int ShortValue )
	{
		addInteger<SWORD> ( static_cast<SWORD> ( ShortValue ) );
	}

	void addLong ( const SDWORD LongValue )
	{
		addInteger<SDWORD> ( LongValue );
	}

	void addFloat ( const float FloatValue )
	{
		union
		{
			float	f;
			SDWORD	l;
		} dat;
		dat.f = FloatValue;
		addInteger<SDWORD> ( dat.l );
	}

	void addString ( const char *pszString )
	{
		const int len = ( pszString != NULL ) ? strlen( pszString ) : 0;

		if ( len > MAX_NETWORK_STRING )
		{
			Printf( "NETWORK_WriteString: String exceeds %d characters!\n", MAX_NETWORK_STRING );
			return;
		}

		for ( int i = 0; i < len; ++i )
			addByte( pszString[i] );
		addByte( 0 );
	}

	void writeCommandToStream ( BYTESTREAM_s &ByteStream ) const {
		// [BB] This also handles the traffic counting (NETWORK_StartTrafficMeasurement/NETWORK_StopTrafficMeasurement).
		NETWORK_WriteBuffer( &ByteStream, _buffer.pbData, _buffer.ulCurrentSize );
	}

	void sendCommandToClients ( ULONG ulPlayerExtra = MAXPLAYERS, ULONG ulFlags = 0 ) const {
		for ( ClientIterator it ( ulPlayerExtra, ulFlags ); it.notAtEnd(); ++it )
		{
			SERVER_CheckClientBuffer( *it, _buffer.ulCurrentSize, true );
			writeCommandToStream ( SERVER_GetClient( *it )->PacketBuffer.ByteStream );
		}
	}
};

//*****************************************************************************
//	FUNCTIONS

// [BB] Check if the actor has a valid net ID. Returns true if it does, returns false and prints a warning if not.
bool EnsureActorHasNetID( const AActor *pActor )
{
	if ( pActor == NULL )
		return false;

	if ( pActor->lNetID == -1 )
	{
		if ( sv_showwarnings && !( pActor->ulNetworkFlags & NETFL_SERVERSIDEONLY ) )
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
// [Dusk] Pretty much rewrote this based on dumpstates code, this now recurses through state
// children now to check them as well. This is needed to find stuff like custom pain states.

bool FindStateAddressRecursor( AActor *pActor, FState *pState, FString prefix, FStateLabels *list, FString &label, LONG &lOffset )
{
	for ( int i = 0; i < list->NumLabels; ++i ) {
		if ( list->Labels[i].State != NULL ) {
			unsigned int offset;
			if ( pActor->InState( list->Labels[i].State, &offset, pState ) && ( offset <= 255 ) ) {
				// Found it!
				label = prefix + list->Labels[i].Label.GetChars();
				lOffset = static_cast<LONG>( offset );
				return true;
			}
		}

		// Not in there, check the children
		if ( list->Labels[i].Children != NULL ) {
			FString newprefix = prefix + list->Labels[i].Label.GetChars() + '.';
			bool found = FindStateAddressRecursor( pActor, pState, newprefix, list->Labels[i].Children, label, lOffset );
			if ( found == true )
				return true;
		}
	}

	return false;
}

void FindStateLabelAndOffset( AActor *pActor, FState *pState, FString &stateLabel, LONG &lOffset, const bool bSuppressWarning = false )
{
	stateLabel = "";
	lOffset = 0;
	FStateLabels *pStateList = pActor->GetClass()->ActorInfo->StateList;

	bool found = FindStateAddressRecursor( pActor, pState, "", pStateList, stateLabel, lOffset );
	if ( found == false && sv_showwarnings && ( bSuppressWarning == false ) )
		Printf ("FindStateLabelAndOffset: Couldn't find state!\n");
}

//*****************************************************************************
//
// [BB]
bool ClassOwnsState( const PClass *pClass, const FState *pState )
{
	if ( ( pState == NULL ) || ( pClass == NULL ) )
		return false;

	const PClass *pStateOwnerClass = FState::StaticFindStateOwner ( pState );

	if ( pStateOwnerClass == NULL )
		return false;

	if ( pStateOwnerClass == pClass )
		return true;

	return false;
}

// [BB]
bool ActorOwnsState( const AActor *pActor, const FState *pState )
{
	if ( pActor == NULL )
		return false;

	return ClassOwnsState( pActor->GetClass(), pState );
}

//*****************************************************************************
//
// [BB] Helper function for SERVERCOMMANDS_SetThingFrame.
bool OffsetAndStateOwnershipValidityCheck ( const LONG lOffset, const PClass *pClass, const FState *pState )
{
	// [BB] The offset is out of range.
	if (( lOffset < 0 ) || ( lOffset > 255 ) )
		return false;

	// [BB] If the offset is zero, it doesn't matter whether the actor owns the state.
	if ( lOffset == 0 )
		return true;

	if ( ClassOwnsState( pClass, pState ) )
		return true;

	// [BB] A non-zero but otherwise valid offset can only be used of the actor owns the state.
	return false;
}

//*****************************************************************************
//
// [BB] Helper function for SERVERCOMMANDS_SetThingFrame.
bool OffsetAndStateOwnershipValidityCheck ( const LONG lOffset, const AActor *pActor, const FState *pState )
{
	if ( pActor == NULL )
		return false;

	return OffsetAndStateOwnershipValidityCheck ( lOffset, pActor->GetClass(), pState );
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
// [BB/WS] Tell clients to re-use their last updated position. This saves a lot of bandwidth.
void CheckPositionReuse( AActor *pActor, ULONG &ulBits )
{
	if ( ulBits & CM_X && pActor->lastX == pActor->x )
	{
		ulBits &= ~CM_X;
		ulBits |= CM_REUSE_X;
	}
	if ( ulBits & CM_Y && pActor->lastY == pActor->y )
	{
		ulBits &= ~CM_Y;
		ulBits |= CM_REUSE_Y;
	}
	if ( ulBits & CM_Z && pActor->lastZ == pActor->z )
	{
		ulBits &= ~CM_Z;
		ulBits |= CM_REUSE_Z;
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
void SERVERCOMMANDS_Nothing( ULONG ulPlayer, bool bReliable )
{
	if ( SERVER_IsValidClient( ulPlayer ) == false )
		return;

	SERVER_CheckClientBuffer( ulPlayer, 1, bReliable );
	NETWORK_WriteHeader( bReliable ? &SERVER_GetClient( ulPlayer )->PacketBuffer.ByteStream : &SERVER_GetClient( ulPlayer )->UnreliablePacketBuffer.ByteStream, SVC_NOTHING );
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

	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
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

	if ( PLAYER_IsValidPlayerWithMo( ulPlayer ) == false )
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

		SERVER_CheckClientBuffer( ulIdx, 24, false );
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
			// [BB] The x/y position has to be sent at full precision, otherwise the player may be rounded to a neighboring sector
			// on the clients, potentially completely changing its Z position.
			NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->UnreliablePacketBuffer.ByteStream, players[ulPlayer].mo->x );
			NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->UnreliablePacketBuffer.ByteStream, players[ulPlayer].mo->y );
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

	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
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

		// [EP] Send the updated health and armor of the player who's being damaged to this client
		// only if this client is allowed to know (still, don't forget the pain state!).
		if ( SERVER_IsPlayerAllowedToKnowHealth( ulIdx, ulPlayer ) == false ) {
			SERVERCOMMANDS_SetThingState( players[ulPlayer].mo, STATE_PAIN, ulIdx, SVCF_ONLYTHISCLIENT );
			continue;
		}

		SERVER_CheckClientBuffer( ulIdx, 8, true );

		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_DAMAGEPLAYER );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulPlayer );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, players[ulPlayer].health );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulArmorPoints );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, players[ulPlayer].attacker ? players[ulPlayer].attacker->lNetID : -1 );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_KillPlayer( ULONG ulPlayer, AActor *pSource, AActor *pInflictor, FName MOD )
{
	ULONG	ulIdx;
	USHORT	usActorNetworkIndex = 0;
	LONG	lSourceID;
	LONG	lInflictorID;

	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
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

	// [BB] We only send the health as short, so make sure that it doesn't exceed the corresponding range.
	WORD wHealth = clamp ( players[ulPlayer].mo->health, SHRT_MIN, SHRT_MAX );

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		SERVER_CheckClientBuffer( ulIdx, 10 + static_cast<ULONG> ( strlen ( MOD.GetChars() ) ) + (ULONG)strlen(players[ulPlayer].mo->DamageType.GetChars()), true );

		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_KILLPLAYER );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulPlayer );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lSourceID );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lInflictorID );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, wHealth );
		NETWORK_WriteString( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, MOD.GetChars() );
		NETWORK_WriteString( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, players[ulPlayer].mo->DamageType.GetChars() );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, usActorNetworkIndex );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerHealth( ULONG ulPlayer, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
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
void SERVERCOMMANDS_SetPlayerArmor( ULONG ulPlayer, ULONG ulPlayerExtra, ULONG ulFlags )
{
	if ( PLAYER_IsValidPlayerWithMo( ulPlayer ) == false )
		return;

	AInventory *pArmor = players[ulPlayer].mo->FindInventory< ABasicArmor >( );
	ULONG ulArmorPoints = ( pArmor != NULL ) ? pArmor->Amount : 0;

	// [BB] The ( ulArmorPoints > 0 ) check ensures ( pArmor != NULL ) inside the block below.
	if ( ulArmorPoints > 0 ){
		FString armorIconTexName = pArmor->Icon.isValid() ? TexMan( pArmor->Icon )->Name : "";

		for ( ULONG ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
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
				SERVER_CheckClientBuffer( ulPlayer, 4 + (ULONG)strlen( armorIconTexName.GetChars() ), true );
				NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETPLAYERARMOR );
				NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulPlayer );
				NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulArmorPoints );
				NETWORK_WriteString( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, armorIconTexName.GetChars() );
			}
		}
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerHealthAndMaxHealthBonus( ULONG ulPlayer, ULONG ulPlayerExtra, ULONG ulFlags )
{
	if ( PLAYER_IsValidPlayerWithMo( ulPlayer ) == false )
		return;

	// [BB] Workaround to set max health bonus for the player on the client(s).
	if ( players[ulPlayer].lMaxHealthBonus > 0 ) {
		AInventory *pInventory = Spawn<AMaxHealth>(0,0,0, NO_REPLACE);
		if ( pInventory )
		{
			pInventory->Amount = players[ulPlayer].lMaxHealthBonus;
			SERVERCOMMANDS_GiveInventory( ulPlayer, pInventory, ulPlayerExtra, ulFlags );
			pInventory->Destroy ();
			pInventory = NULL;
		}
	}
	SERVERCOMMANDS_SetPlayerHealth( ulPlayer, ulPlayerExtra, ulFlags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerArmorAndMaxArmorBonus( ULONG ulPlayer, ULONG ulPlayerExtra, ULONG ulFlags )
{
	if ( PLAYER_IsValidPlayerWithMo( ulPlayer ) == false )
		return;

	// [BB] Workaround to set max armor bonus for the player on the client(s).
	ABasicArmor *pArmor = players[ulPlayer].mo->FindInventory<ABasicArmor>( );

	if ( pArmor && ( pArmor->BonusCount > 0 ) )
	{
		AInventory *pInventory = static_cast<AInventory*> ( Spawn( PClass::FindClass( "MaxArmorBonus" ), 0,0,0, NO_REPLACE) );
		if ( pInventory ) 
		{
			pInventory->Amount = pArmor->BonusCount;
			SERVERCOMMANDS_GiveInventory( ulPlayer, pInventory, ulPlayerExtra, ulFlags );
			pInventory->Destroy ();
			pInventory = NULL;
		}
	}
	SERVERCOMMANDS_SetPlayerArmor( ulPlayer, ulPlayerExtra, ulFlags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerState( ULONG ulPlayer, PLAYERSTATE_e ulState, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
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

	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
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

		if ( ulUserInfoFlags & USERINFO_TICSPERUPDATE )
			NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, players[ulPlayer].userinfo.ulTicsPerUpdate );

		if ( ulUserInfoFlags & USERINFO_CONNECTIONTYPE )
			NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, players[ulPlayer].userinfo.ulConnectionType );

		// [CK] We use a bitfield now.
		if ( ulUserInfoFlags & USERINFO_CLIENTFLAGS )
			NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, players[ulPlayer].userinfo.clientFlags );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerFrags( ULONG ulPlayer, ULONG ulPlayerExtra, ULONG ulFlags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	NetCommand command ( SVC_SETPLAYERFRAGS );
	command.addByte ( ulPlayer );
	command.addShort ( players[ulPlayer].fragcount );
	command.sendCommandToClients ( ulPlayerExtra, ulFlags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerPoints( ULONG ulPlayer, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG		ulIdx;

	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
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

	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
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

	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
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
void SERVERCOMMANDS_SetPlayerChatStatus( ULONG ulPlayer, ULONG ulPlayerExtra, ULONG ulFlags )
{
	SERVERCOMMANDS_SetPlayerStatus( ulPlayer, SVC_SETPLAYERCHATSTATUS, players[ulPlayer].bChatting, ulPlayerExtra, ulFlags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerLaggingStatus( ULONG ulPlayer )
{
	SERVERCOMMANDS_SetPlayerStatus( ulPlayer, SVC_SETPLAYERLAGGINGSTATUS, players[ulPlayer].bLagging );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerConsoleStatus( ULONG ulPlayer, ULONG ulPlayerExtra, ULONG ulFlags )
{
	SERVERCOMMANDS_SetPlayerStatus( ulPlayer, SVC_SETPLAYERCONSOLESTATUS, players[ulPlayer].bInConsole, ulPlayerExtra, ulFlags );
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
void SERVERCOMMANDS_SetPlayerStatus( ULONG ulPlayer, int iHeader, bool bValue, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
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

	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
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

	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
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

	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
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

	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
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

	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
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

	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
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

	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	if ( players[ulPlayer].ReadyWeapon == NULL )
		return;

	// [BB] Try to find the state label and the correspoding offset belonging to the target state.
	FindStateLabelAndOffset( players[ulPlayer].ReadyWeapon, pState, stateLabel, lOffset, true );

	// Couldn't find the state, so just try to go based off the spawn state.
	if ( stateLabel.IsEmpty() )
	{
		lOffset = LONG( pState - players[ulPlayer].ReadyWeapon->GetReadyState( ));
		// [BB] Spawn state didn't work either, try flash state.
		if ( OffsetAndStateOwnershipValidityCheck ( lOffset, players[ulPlayer].ReadyWeapon, players[ulPlayer].ReadyWeapon->GetReadyState( ) ) == false )
		{
			const FState *pFlashState = players[ulPlayer].ReadyWeapon->FindState(NAME_Flash);
			lOffset = LONG( pState - pFlashState );
			if ( OffsetAndStateOwnershipValidityCheck ( lOffset, players[ulPlayer].ReadyWeapon, pFlashState ) == false )
			{
				// [BB] If we still couldn't find the state, show a warning.
				if ( sv_showwarnings )
					Printf ( "SERVERCOMMANDS_SetPlayerPSprite: Couldn't find state!\n" );

				return;
			}
			else
				stateLabel = ":F";
		}
		else
			stateLabel = ":R";
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
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
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
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
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
void SERVERCOMMANDS_SetPlayerLivesLeft( ULONG ulPlayer, ULONG ulPlayerExtra, ULONG ulFlags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
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

		SERVER_CheckClientBuffer( ulIdx, 3, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETPLAYERLIVESLEFT );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulPlayer );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, players[ulPlayer].ulLivesLeft );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SyncPlayerAmmoAmount( ULONG ulPlayer, ULONG ulPlayerExtra, ULONG ulFlags )
{
	if ( PLAYER_IsValidPlayerWithMo( ulPlayer ) == false )
		return;

	AInventory *pInventory = NULL;

	for ( pInventory = players[ulPlayer].mo->Inventory; pInventory != NULL; pInventory = pInventory->Inventory )
	{
		if ( pInventory->IsKindOf( RUNTIME_CLASS( AAmmo )) == false )
			continue;

		SERVERCOMMANDS_GiveInventory( ulPlayer, pInventory, ulPlayerExtra, ulFlags );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_UpdatePlayerPing( ULONG ulPlayer, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;

	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
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
	if (( SERVER_IsValidClient( ulPlayer ) == false ) || ( PLAYER_IsValidPlayer( ulDisplayPlayer ) == false ))
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

	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
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

	if ( players[ulPlayer].mo == NULL )
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

	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
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

	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
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

	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
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
		if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
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
				if ( (( players[ulIdx].bOnTeam == false ) || ( players[ulPlayer].bOnTeam == false ))
					&& (( PLAYER_IsTrueSpectator ( &players[ulIdx] ) == false ) || ( PLAYER_IsTrueSpectator ( &players[ulPlayer] ) == false )) )
					continue;

				// If the players are not on the same team, don't send the message.
				if ( ( players[ulIdx].ulTeam != players[ulPlayer].ulTeam ) && ( ( PLAYER_IsTrueSpectator ( &players[ulIdx] ) != PLAYER_IsTrueSpectator ( &players[ulPlayer] ) ) || ( PLAYER_IsTrueSpectator ( &players[ulIdx] ) == false ) ) )
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

	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
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

	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
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

	if (( PLAYER_IsValidPlayer( ulPlayer ) == false ) ||
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

	if (( PLAYER_IsValidPlayer( ulPlayer ) == false ) ||
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

	NetCommand command ( SVC_SPAWNTHING );
	command.addShort( pActor->x >> FRACBITS );
	command.addShort( pActor->y >> FRACBITS );
	command.addShort( pActor->z >> FRACBITS );
	command.addShort( usActorNetworkIndex );
	command.addShort( pActor->lNetID );
	command.sendCommandToClients( ulPlayerExtra, ulFlags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SpawnThingNoNetID( AActor *pActor, ULONG ulPlayerExtra, ULONG ulFlags )
{
	USHORT		usActorNetworkIndex = 0;

	if ( pActor == NULL )
		return;

	if ( pActor->ulNetworkFlags & NETFL_SERVERSIDEONLY )
		return;

	usActorNetworkIndex = pActor->GetClass( )->getActorNetworkIndex();

	NetCommand command ( SVC_SPAWNTHINGNONETID );
	command.addShort( pActor->x >> FRACBITS );
	command.addShort( pActor->y >> FRACBITS );
	command.addShort( pActor->z >> FRACBITS );
	command.addShort( usActorNetworkIndex );
	command.sendCommandToClients( ulPlayerExtra, ulFlags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SpawnThingExact( AActor *pActor, ULONG ulPlayerExtra, ULONG ulFlags )
{
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

	NetCommand command ( SVC_SPAWNTHINGEXACT );
	command.addLong( pActor->x );
	command.addLong( pActor->y );
	command.addLong( pActor->z );
	command.addShort( usActorNetworkIndex );
	command.addShort( pActor->lNetID );
	command.sendCommandToClients( ulPlayerExtra, ulFlags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SpawnThingExactNoNetID( AActor *pActor, ULONG ulPlayerExtra, ULONG ulFlags )
{
	USHORT		usActorNetworkIndex = 0;

	if ( pActor == NULL )
		return;

	usActorNetworkIndex = pActor->GetClass( )->getActorNetworkIndex();

	NetCommand command ( SVC_SPAWNTHINGEXACTNONETID );
	command.addLong( pActor->x );
	command.addLong( pActor->y );
	command.addLong( pActor->z );
	command.addShort( usActorNetworkIndex );
	command.sendCommandToClients( ulPlayerExtra, ulFlags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_MoveThing( AActor *pActor, ULONG ulBits, ULONG ulPlayerExtra, ULONG ulFlags )
{
	if ( !EnsureActorHasNetID (pActor) )
		return;

	// [BB] Only skip updates, if sent to all players.
	if ( ulFlags == 0 )
		RemoveUnnecessaryPositionUpdateFlags ( pActor, ulBits );
	else // [WS] This will inform clients not to set their lastX/Y/Z with the new position.
		ulBits |= CM_NOLAST;

	// [WS] Check to see if the position can be re-used by the client.
	CheckPositionReuse( pActor, ulBits );

	// Nothing to update.
	if ( ulBits == 0 )
		return;

	NetCommand command ( SVC_MOVETHING );
	command.addShort( pActor->lNetID );
	command.addShort( ulBits );

	// Write position.
	if ( ulBits & CM_X )
		command.addShort( pActor->x >> FRACBITS );
	if ( ulBits & CM_Y )
		command.addShort( pActor->y >> FRACBITS );
	if ( ulBits & CM_Z )
		command.addShort( pActor->z >> FRACBITS );

	// Write last position.
	if ( ulBits & CM_LAST_X )
		command.addShort( pActor->lastX >> FRACBITS );
	if ( ulBits & CM_LAST_Y )
		command.addShort( pActor->lastY >> FRACBITS );
	if ( ulBits & CM_LAST_Z )
		command.addShort( pActor->lastZ >> FRACBITS );

	// Write angle.
	if ( ulBits & CM_ANGLE )
		command.addLong( pActor->angle );

	// Write velocity.
	if ( ulBits & CM_MOMX )
		command.addShort( pActor->momx >> FRACBITS );
	if ( ulBits & CM_MOMY )
		command.addShort( pActor->momy >> FRACBITS );
	if ( ulBits & CM_MOMZ )
		command.addShort( pActor->momz >> FRACBITS );

	// Write pitch.
	if ( ulBits & CM_PITCH )
		command.addLong( pActor->pitch );

	// Write movedir.
	if ( ulBits & CM_MOVEDIR )
		command.addByte( pActor->movedir );

	command.sendCommandToClients( ulPlayerExtra, ulFlags );

	// [BB] Only mark something as updated, if it the update was sent to all players.
	if ( ulFlags == 0 )
		ActorNetPositionUpdated ( pActor, ulBits );
}

//*****************************************************************************
//
void SERVERCOMMANDS_MoveThingExact( AActor *pActor, ULONG ulBits, ULONG ulPlayerExtra, ULONG ulFlags )
{
	if ( !EnsureActorHasNetID (pActor) )
		return;

	// [BB] Only skip updates, if sent to all players.
	if ( ulFlags == 0 )
		RemoveUnnecessaryPositionUpdateFlags ( pActor, ulBits );
	else // [WS] This will inform clients not to set their lastX/Y/Z with the new position.
		ulBits |= CM_NOLAST;

	// [WS] Check to see if the position can be re-used by the client.
	CheckPositionReuse( pActor, ulBits );

	// Nothing to update.
	if ( ulBits == 0 )
		return;

	NetCommand command ( SVC_MOVETHINGEXACT );
	command.addShort( pActor->lNetID );
	command.addShort( ulBits );

	// Write position.
	if ( ulBits & CM_X )
		command.addLong( pActor->x );
	if ( ulBits & CM_Y )
		command.addLong( pActor->y );
	if ( ulBits & CM_Z )
		command.addLong( pActor->z );

	// Write last position.
	if ( ulBits & CM_LAST_X )
		command.addLong( pActor->lastX );
	if ( ulBits & CM_LAST_Y )
		command.addLong( pActor->lastY );
	if ( ulBits & CM_LAST_Z )
		command.addLong( pActor->lastZ );

	// Write angle.
	if ( ulBits & CM_ANGLE )
		command.addLong( pActor->angle );

	// Write velocity.
	if ( ulBits & CM_MOMX )
		command.addLong( pActor->momx );
	if ( ulBits & CM_MOMY )
		command.addLong( pActor->momy );
	if ( ulBits & CM_MOMZ )
		command.addLong( pActor->momz );

	// Write pitch.
	if ( ulBits & CM_PITCH )
		command.addLong( pActor->pitch );

	// Write movedir.
	if ( ulBits & CM_MOVEDIR )
		command.addByte( pActor->movedir );

	command.sendCommandToClients( ulPlayerExtra, ulFlags );

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
void SERVERCOMMANDS_SetThingState( AActor *pActor, ULONG ulState, ULONG ulPlayerExtra, ULONG ulFlags )
{
	if ( !EnsureActorHasNetID (pActor) )
		return;

	NetCommand command( SVC_SETTHINGSTATE );
	command.addShort( pActor->lNetID );
	command.addByte( ulState );
	command.sendCommandToClients( ulPlayerExtra, ulFlags );

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
void SERVERCOMMANDS_SetThingSound( AActor *pActor, ULONG ulSound, const char *pszSound, ULONG ulPlayerExtra, ULONG ulFlags )
{
	if ( !EnsureActorHasNetID (pActor) )
		return;

	NetCommand command( SVC_SETTHINGSOUND );
	command.addShort( pActor->lNetID );
	command.addByte( ulSound );
	command.addString( pszSound );
	command.sendCommandToClients( ulPlayerExtra, ulFlags );
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
void SERVERCOMMANDS_SetThingSpecial( AActor *pActor, ULONG ulPlayerExtra, ULONG ulFlags )
{
	if ( !EnsureActorHasNetID (pActor) )
		return;

	NetCommand command( SVC_EXTENDEDCOMMAND );
	command.addByte( SVC2_SETTHINGSPECIAL );
	command.addShort( pActor->lNetID );
	command.addShort( pActor->special );
	command.sendCommandToClients( ulPlayerExtra, ulFlags );
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
void SERVERCOMMANDS_SetThingReactionTime( AActor *pActor, ULONG ulPlayerExtra, ULONG ulFlags )
{
	if ( !EnsureActorHasNetID (pActor) )
		return;

	NetCommand command( SVC_EXTENDEDCOMMAND );
	command.addByte( SVC2_SETTHINGREACTIONTIME );
	command.addShort( pActor->lNetID );
	command.addShort( pActor->reactiontime );
	command.sendCommandToClients( ulPlayerExtra, ulFlags );
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

		SERVER_CheckClientBuffer( ulIdx, 7, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETTHINGGRAVITY );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->lNetID );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->gravity );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetThingFrame( AActor *pActor, FState *pState, ULONG ulPlayerExtra, ULONG ulFlags, bool bCallStateFunction )
{
	FString stateLabel;
	LONG		lOffset = 0;

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
		else if ( pState == pActor->FindState( NAME_Wound ))
		{
			SERVERCOMMANDS_SetThingState( pActor, STATE_WOUND );
			return;
		}
		else if ( pState == pActor->FindState( NAME_Pain ))
		{
			SERVERCOMMANDS_SetThingState( pActor, STATE_PAIN );
			return;
		}
	}

	// [BB] Try to find the state label and the correspoding offset belonging to the target state.
	FindStateLabelAndOffset( pActor, pState, stateLabel, lOffset, true );

	// Couldn't find the state, so just try to go based off one of the standard states.
	// [BB] This is a workaround. Therefore let the name of the state string begin
	// with ':' so that the client can handle this differently.
	// [BB] Because of inheritance it's not sufficient to only try the SpawnState.
	if ( stateLabel.IsEmpty() )
	{
		lOffset = LONG( pState - pActor->SpawnState );
		if ( OffsetAndStateOwnershipValidityCheck ( lOffset, pActor, pActor->SpawnState ) == false )
		{
			// [BB] SpawnState doesn't work. Try MissileState.
			lOffset = LONG( pState - pActor->MissileState );
			if ( OffsetAndStateOwnershipValidityCheck ( lOffset, pActor, pActor->MissileState ) == false )
			{
				// [BB] Try SeeState.
				lOffset = LONG( pState - pActor->SeeState );
				if ( OffsetAndStateOwnershipValidityCheck ( lOffset, pActor, pActor->SeeState ) == false )
				{
					// [BB] Try MeleeState.
					lOffset = LONG( pState - pActor->MeleeState );
					if ( OffsetAndStateOwnershipValidityCheck ( lOffset, pActor, pActor->MeleeState ) == false )
					{
						// [BB] Apparently pActor doesn't own the state. Find out who does.
						const PClass *pStateOwnerClass = FState::StaticFindStateOwner ( pState );
						const AActor *pStateOwner = ( pStateOwnerClass != NULL ) ? GetDefaultByType ( pStateOwnerClass ) : NULL;
						if ( pStateOwner )
						{
							lOffset = LONG( pState - pStateOwner->SpawnState );
							if ( OffsetAndStateOwnershipValidityCheck ( lOffset, pStateOwnerClass, pStateOwner->SpawnState ) == false )
							{
								lOffset = LONG( pState - pStateOwnerClass->ActorInfo->FindState(NAME_Death) );
								if ( OffsetAndStateOwnershipValidityCheck ( lOffset, pStateOwnerClass, pStateOwnerClass->ActorInfo->FindState(NAME_Death) ) == false )
								{
									if ( sv_showwarnings )
										Printf ( "Warning: SERVERCOMMANDS_SetThingFrame failed to set the frame for actor %s.\n", pActor->GetClass()->TypeName.GetChars() );
									return;
								}
								{
									stateLabel = "+";
									stateLabel += pStateOwnerClass->TypeName;
								}
							}
							else
							{
								stateLabel = ";";
								stateLabel += pStateOwnerClass->TypeName;
							}

						}
						else
						{
							if ( sv_showwarnings )
								Printf ( "Warning: SERVERCOMMANDS_SetThingFrame failed to set the frame for actor %s (can't find state owner).\n", pActor->GetClass()->TypeName.GetChars() );
							return;
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

	// [Dusk] Use NetCommand
	NetCommand command( bCallStateFunction ? SVC_SETTHINGFRAME : SVC_SETTHINGFRAMENF );
	command.addShort( pActor->lNetID );
	command.addString( stateLabel.GetChars( ) );
	command.addByte( lOffset );
	command.sendCommandToClients( ulPlayerExtra, ulFlags );
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
	if ( originator == NULL )
		return;

	NetCommand command ( SVC_SPAWNBLOOD );
	command.addShort( x >> FRACBITS );
	command.addShort( y >> FRACBITS );
	command.addShort( z >> FRACBITS );
	command.addShort( dir >> FRACBITS );
	command.addByte( damage );
	command.addShort( originator->lNetID );
	command.sendCommandToClients( ulPlayerExtra, ulFlags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SpawnBloodSplatter( fixed_t x, fixed_t y, fixed_t z, AActor *originator, bool isBloodSplatter2, ULONG ulPlayerExtra, ULONG ulFlags )
{
	if ( originator == NULL )
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

		SERVER_CheckClientBuffer( ulIdx, 9, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, isBloodSplatter2 ? SVC_SPAWNBLOODSPLATTER2 : SVC_SPAWNBLOODSPLATTER );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, x >> FRACBITS );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, y >> FRACBITS );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, z >> FRACBITS );
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
	NetCommand command ( SVC_PRINTMOTD );
	command.addString ( pszString );
	command.sendCommandToClients ( ulPlayerExtra, ulFlags );
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
	NetCommand command ( SVC_SETGAMEDMFLAGS );
	command.addLong ( dmflags );
	command.addLong ( dmflags2 );
	command.addLong ( compatflags );
	command.addLong ( compatflags2 );
	command.addLong ( dmflags3 );
	command.sendCommandToClients( ulPlayerExtra, ulFlags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetGameModeLimits( ULONG ulPlayerExtra, ULONG ulFlags )
{
	NetCommand command ( SVC_SETGAMEMODELIMITS );
	command.addShort( fraglimit );
	command.addFloat( timelimit );
	command.addShort( pointlimit );
	command.addByte( duellimit );
	command.addByte( winlimit );
	command.addByte( wavelimit );
	command.addByte( sv_cheats );
	command.addByte( sv_fastweapons );
	command.addByte( sv_maxlives );
	command.addByte( sv_maxteams );
	// [BB] The value of sv_gravity is irrelevant when sv_gravity hasn't been changed since the map start
	// and the map has a custom gravity value in MAPINFO. level.gravity always contains the used gravity value,
	// so we just send this one instead of sv_gravity.
	command.addFloat( level.gravity );
	// [BB] sv_aircontrol needs to be handled similarly to sv_gravity.
	command.addFloat( static_cast<float>(level.aircontrol) / 65536.f );
	// [WS] Send in sv_coop_damagefactor.
	command.addFloat( sv_coop_damagefactor );
	// [WS] Send in alwaysapplydmflags.
	command.addByte( alwaysapplydmflags );
	// [AM] Send lobby map.
	command.addString( lobby );
	// [TP] Send sv_limitcommands
	command.addByte( sv_limitcommands );
	command.sendCommandToClients( ulPlayerExtra, ulFlags );
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

	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
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

	// [BB] Allow teams.Size( ) here, this handles the white flag.
	if ( ( TEAM_CheckIfValid ( ulTeam ) == false ) && ( ulTeam != teams.Size( ) ) )
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

	// [BB] Allow teams.Size( ) here, this handles the white flag.
	if ( ( TEAM_CheckIfValid ( ulTeam ) == false ) && ( ulTeam != teams.Size( ) ) )
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

	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
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

		SERVER_CheckClientBuffer( ulIdx, 11, true );
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
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	NetCommand command( SVC_WEAPONSOUND );
	command.addByte( ulPlayer );
	command.addString( pszSound );
	command.sendCommandToClients( ulPlayerExtra, ulFlags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_WeaponChange( ULONG ulPlayer, ULONG ulPlayerExtra, ULONG ulFlags )
{
	ULONG	ulIdx;
	USHORT	usActorNetworkIndex = 0;

	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
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
	// Evidently, to draw a railgun trail, there must be a source actor.
	if ( !EnsureActorHasNetID (pSource) )
		return;

	NetCommand command ( SVC_WEAPONRAILGUN );
	command.addShort( pSource->lNetID );
	command.addFloat( Start.X );
	command.addFloat( Start.Y );
	command.addFloat( Start.Z );
	command.addFloat( End.X );
	command.addFloat( End.Y );
	command.addFloat( End.Z );
	command.addLong( lColor1 );
	command.addLong( lColor2 );
	command.addFloat( fMaxDiff );
	command.addByte( bSilent );
	command.sendCommandToClients ( ulPlayerExtra, ulFlags );
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

		SERVER_CheckClientBuffer( ulIdx, 11, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETSECTORPANNING );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulSector );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, sectors[ulSector].GetXOffset(sector_t::ceiling) / FRACUNIT );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, sectors[ulSector].GetYOffset(sector_t::ceiling, false) / FRACUNIT );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, sectors[ulSector].GetXOffset(sector_t::floor) / FRACUNIT );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, sectors[ulSector].GetYOffset(sector_t::floor,false) / FRACUNIT );
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
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ( sectors[ulSector].GetAngle(sector_t::ceiling,false) / ANGLE_1 ));
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ( sectors[ulSector].GetAngle(sector_t::floor,false) / ANGLE_1 ));
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetSectorRotationByTag( ULONG ulTag, LONG lFloorRot, LONG lCeilingRot, ULONG ulPlayerExtra, ULONG ulFlags )
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
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETSECTORROTATIONBYTAG );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulTag );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lCeilingRot );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lFloorRot );
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
void SERVERCOMMANDS_ACSScriptExecute( ULONG ulScript, AActor *pActivator, LONG lLineIdx, int levelnum, bool bBackSide, int iArg0, int iArg1, int iArg2, bool bAlways, ULONG ulPlayerExtra, ULONG ulFlags )
{
	LONG lActivatorID = ( pActivator ? pActivator->lNetID : -1 );

	// [TP] Argument header:
	// Bits 0-1: length of arg0
	// Bits 2-3: length of arg1
	// Bits 4-5: length of arg2
	// Bit 6: bBackSide
	// Bit 7: bAlways
	//
	// Length is:
	//	0: not sent, assume 0
	//	1: sent as a signed byte [-128, 127]
	//	2: sent as a signed short [-32768, 32767]
	//	3: sent as a signed long (<= -32769 || >= -32768)
	//
	BYTE argheader = 0;
	const int args[] = {iArg0, iArg1, iArg2};
	int arglength[3];

	for ( int i = 0; i < 3; ++i )
	{
		if ( args[i] == 0 )
			arglength[i] = 0;
		else if (( args[i] <= 0x7F ) && ( args[i] >= -0x80 ))
			arglength[i] = 1;
		else if (( args[i] <= 0x7FFF ) && ( args[i] >= -0x8000 ))
			arglength[i] = 2;
		else
			arglength[i] = 3;

		// [TP] Store length in the argument header
		argheader |= arglength[i] << ( 2 * i );
	}

	argheader |= ( bBackSide ? 1 : 0 ) << 6;
	argheader |= ( bAlways ? 1 : 0 ) << 7;

	NetCommand command ( SVC_ACSSCRIPTEXECUTE );
	command.addShort( ulScript );
	command.addShort( lActivatorID );
	command.addShort( lLineIdx );
	command.addByte( levelnum );
	command.addByte( argheader );

	// [TP] Now send the arguments.
	for ( int i = 0; i < 3; ++i )
	{
		switch( arglength[i] )
		{
			case 1: command.addByte( args[i] ); break;
			case 2: command.addShort( args[i] ); break;
			case 3: command.addLong( args[i] ); break;
			default: break;
		}
	}

	command.sendCommandToClients( ulPlayerExtra, ulFlags );
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
	NetCommand command ( SVC_SOUND );
	command.addByte ( lChannel );
	command.addString ( pszSound );
	command.addByte ( LONG(fVolume*127) );
	command.addByte ( NETWORK_AttenuationFloatToInt ( fAttenuation ) );
	command.sendCommandToClients ( ulPlayerExtra, ulFlags );
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

	NetCommand command ( bRespectActorPlayingSomething ? SVC_SOUNDACTORIFNOTPLAYING : SVC_SOUNDACTOR );
	command.addShort( pActor->lNetID );
	command.addShort( lChannel );
	command.addString( pszSound );
	command.addByte( LONG( fVolume * 127 ));
	command.addByte( NETWORK_AttenuationFloatToInt ( fAttenuation ));
	command.sendCommandToClients( ulPlayerExtra, ulFlags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SoundPoint( LONG lX, LONG lY, LONG lZ, LONG lChannel, const char *pszSound, float fVolume, float fAttenuation, ULONG ulPlayerExtra, ULONG ulFlags )
{
	NetCommand command ( SVC_SOUNDPOINT );
	command.addShort ( lX>>FRACBITS );
	command.addShort ( lY>>FRACBITS );
	command.addShort ( lZ>>FRACBITS );
	command.addByte ( lChannel );
	command.addString ( pszSound );
	command.addByte ( LONG(fVolume*127) );
	command.addByte ( NETWORK_AttenuationFloatToInt ( fAttenuation ) );
	command.sendCommandToClients ( ulPlayerExtra, ulFlags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_AnnouncerSound( const char *pszSound, ULONG ulPlayerExtra, ULONG ulFlags )
{
	NetCommand command ( SVC_ANNOUNCERSOUND );
	command.addString ( pszSound );
	command.sendCommandToClients ( ulPlayerExtra, ulFlags );
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_StartSectorSequence( sector_t *pSector, const int Channel, const char *pszSequence, const int Modenum, ULONG ulPlayerExtra, ULONG ulFlags )
{
	const LONG lSectorID = LONG( pSector - sectors );
	if (( lSectorID < 0 ) || ( lSectorID >= numsectors ))
		return;

	NetCommand command ( SVC_STARTSECTORSEQUENCE );
	command.addShort ( lSectorID );
	command.addByte ( Channel );
	command.addString ( pszSequence );
	command.addByte ( Modenum );
	command.sendCommandToClients ( ulPlayerExtra, ulFlags );
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

	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
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

	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
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

	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	if ( pInventory == NULL )
		return;

	if ( pInventory->ulNetworkFlags & NETFL_SERVERSIDEONLY )
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

	// [BB] Clients don't know that a BackpackItem may be depleted. In this case we have to resync the ammo count.
	if ( pInventory->IsKindOf (RUNTIME_CLASS(ABackpackItem)) && static_cast<ABackpackItem*> ( pInventory )->bDepleted )
		SERVERCOMMANDS_SyncPlayerAmmoAmount( ulPlayer, ulPlayerExtra, ulFlags );
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

	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	const PClass *pType = PClass::FindClass( pszClassName );
	if ( pType == NULL || ( GetDefaultByType ( pType )->ulNetworkFlags & NETFL_SERVERSIDEONLY ) )
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
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	if ( pPowerup == NULL )
		return;

	NetCommand command ( SVC_GIVEPOWERUP );
	command.addByte( ulPlayer );
	command.addShort( pPowerup->GetClass( )->getActorNetworkIndex() );
	// Can we have multiple amounts of a powerup? Probably not, but I'll be safe for now.
	command.addShort( pPowerup->Amount );
	command.addShort( pPowerup->EffectTics );
	command.sendCommandToClients( ulPlayerExtra, ulFlags );
}

//*****************************************************************************
// [WS]
void SERVERCOMMANDS_SetPowerupBlendColor( ULONG ulPlayer, APowerup *pPowerup, ULONG ulPlayerExtra, ULONG ulFlags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	if ( pPowerup == NULL )
		return;

	NetCommand command( SVC_EXTENDEDCOMMAND );
	command.addByte( SVC2_SETPOWERUPBLENDCOLOR );
	command.addByte( ulPlayer );
	command.addShort( pPowerup->GetClass( )->getActorNetworkIndex() );
	command.addLong( pPowerup->BlendColor );
	command.sendCommandToClients( ulPlayerExtra, ulFlags );
}

//*****************************************************************************
// [Dusk]
void SERVERCOMMANDS_GiveWeaponHolder( ULONG ulPlayer, AWeaponHolder *pHolder, ULONG ulPlayerExtra, ULONG ulFlags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	if ( pHolder == NULL )
		return;

	if ( pHolder->ulNetworkFlags & NETFL_SERVERSIDEONLY )
		return;

	NetCommand command( SVC_EXTENDEDCOMMAND );
	command.addByte( SVC2_GIVEWEAPONHOLDER );
	command.addByte( ulPlayer );
	command.addShort( pHolder->PieceMask );
	command.addShort( pHolder->PieceWeapon->getActorNetworkIndex() );
	command.sendCommandToClients( ulPlayerExtra, ulFlags );
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

	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
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
//
void SERVERCOMMANDS_SetInventoryIcon( ULONG ulPlayer, AInventory *pInventory, ULONG ulPlayerExtra, ULONG ulFlags )
{
	if ( ( pInventory == NULL ) || ( pInventory->Icon.isValid() == false ) )
		return;

	FString iconTexName = TexMan( pInventory->Icon )->Name;

	for ( ULONG ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		SERVER_CheckClientBuffer( ulIdx, 5 + (ULONG)strlen( iconTexName.GetChars() ), true );

		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_EXTENDEDCOMMAND );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC2_SETINVENTORYICON );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulPlayer );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pInventory->GetClass( )->getActorNetworkIndex() );
		NETWORK_WriteString( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, iconTexName.GetChars() );
	}
}

//*****************************************************************************
// [Dusk]
void SERVERCOMMANDS_SetHexenArmorSlots( ULONG ulPlayer, AHexenArmor *aHXArmor, ULONG ulPlayerExtra, ULONG ulFlags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	if ( aHXArmor == NULL )
		return;

	NetCommand command( SVC_EXTENDEDCOMMAND );
	command.addByte( SVC2_SETHEXENARMORSLOTS );
	command.addByte( ulPlayer );
	for (short i = 0; i <= 4; i++)
		command.addLong( aHXArmor->Slots[i] );
	command.sendCommandToClients( ulPlayerExtra, ulFlags );
}

//*****************************************************************************
void SERVERCOMMANDS_SyncHexenArmorSlots( ULONG ulPlayer, ULONG ulPlayerExtra, ULONG ulFlags )
{
	if ( PLAYER_IsValidPlayerWithMo( ulPlayer ) == false )
		return;

	AHexenArmor *aHXArmor = static_cast<AHexenArmor *>( players[ulPlayer].mo->FindInventory( RUNTIME_CLASS( AHexenArmor )));
	SERVERCOMMANDS_SetHexenArmorSlots( ulPlayer, aHXArmor, ulPlayerExtra, ulFlags );
}

//*****************************************************************************
// [Dusk]
void SERVERCOMMANDS_SetFastChaseStrafeCount( AActor *mobj, ULONG ulPlayerExtra, ULONG ulFlags )
{
	if ( !EnsureActorHasNetID (mobj) )
		return;

	NetCommand command( SVC_EXTENDEDCOMMAND );
	command.addByte( SVC2_SETFASTCHASESTRAFECOUNT );
	command.addShort( mobj->lNetID );
	command.addByte( mobj->FastChaseStrafeCount );
	command.sendCommandToClients( ulPlayerExtra, ulFlags );
}

//*****************************************************************************
//
// [Dusk] This function is called to set an actor's health directly on the
// client. I don't expect many things to call it (it was created for the sake
// of syncing hellstaff rain health fields) so it's an extended command for now
// instead of a regular one, despite its genericness.
//
void SERVERCOMMANDS_SetThingHealth( AActor* mobj, ULONG ulPlayerExtra, ULONG ulFlags )
{
	if ( !EnsureActorHasNetID (mobj) )
		return;

	NetCommand command( SVC_EXTENDEDCOMMAND );
	command.addByte( SVC2_SETTHINGHEALTH );
	command.addShort( mobj->lNetID );
	command.addByte( mobj->health );
	command.sendCommandToClients( ulPlayerExtra, ulFlags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_FullUpdateCompleted( ULONG ulClient )
{
	SERVER_CheckClientBuffer( ulClient, 2, true );
	NETWORK_WriteHeader( &SERVER_GetClient( ulClient )->PacketBuffer.ByteStream, SVC_EXTENDEDCOMMAND );
	NETWORK_WriteByte( &SERVER_GetClient( ulClient )->PacketBuffer.ByteStream, SVC2_FULLUPDATECOMPLETED );
}

//*****************************************************************************
//
void SERVERCOMMANDS_ResetMap( ULONG ulPlayerExtra, ULONG ulFlags )
{
	NetCommand command( SVC_EXTENDEDCOMMAND );
	command.addByte( SVC2_RESETMAP );
	command.sendCommandToClients( ulPlayerExtra, ulFlags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetIgnoreWeaponSelect( ULONG ulClient, const bool bIgnoreWeaponSelect )
{
	SERVER_CheckClientBuffer( ulClient, 3, true );
	NETWORK_WriteHeader( &SERVER_GetClient( ulClient )->PacketBuffer.ByteStream, SVC_EXTENDEDCOMMAND );
	NETWORK_WriteByte( &SERVER_GetClient( ulClient )->PacketBuffer.ByteStream, SVC2_SETIGNOREWEAPONSELECT );
	NETWORK_WriteByte( &SERVER_GetClient( ulClient )->PacketBuffer.ByteStream, bIgnoreWeaponSelect );
}

//*****************************************************************************
//
void SERVERCOMMANDS_ClearConsoleplayerWeapon( ULONG ulClient )
{
	SERVER_CheckClientBuffer( ulClient, 2, true );
	NETWORK_WriteHeader( &SERVER_GetClient( ulClient )->PacketBuffer.ByteStream, SVC_EXTENDEDCOMMAND );
	NETWORK_WriteByte( &SERVER_GetClient( ulClient )->PacketBuffer.ByteStream, SVC2_CLEARCONSOLEPLAYERWEAPON );
}

//*****************************************************************************
//
void SERVERCOMMANDS_Lightning( ULONG ulPlayerExtra, ULONG ulFlags )
{
	NetCommand command ( SVC_EXTENDEDCOMMAND );
	command.addByte( SVC2_LIGHTNING );
	command.sendCommandToClients( ulPlayerExtra, ulFlags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_CancelFade( const ULONG ulPlayer, ULONG ulPlayerExtra, ULONG ulFlags )
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

		SERVER_CheckClientBuffer( ulIdx, 3, true );

		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_EXTENDEDCOMMAND );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC2_CANCELFADE );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulPlayer );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_PlayBounceSound( const AActor *pActor, const bool bOnfloor, ULONG ulPlayerExtra, ULONG ulFlags )
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

		SERVER_CheckClientBuffer( ulIdx, 5, true );

		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_EXTENDEDCOMMAND );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC2_PLAYBOUNCESOUND );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, pActor->lNetID );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, bOnfloor );
	}
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_DoDoor( sector_t *pSector, DDoor::EVlDoor type, LONG lSpeed, LONG lDirection, LONG lLightTag, LONG lID, ULONG ulPlayerExtra, ULONG ulFlags )
{
	LONG	lSectorID;

	lSectorID = LONG( pSector - sectors );
	if (( lSectorID < 0 ) || ( lSectorID >= numsectors ))
		return;

	// Since we still want to send direction as a byte, but -1 can't be represented in byte
	// form, adjust the value into something that can be represented.
	lDirection = SERVER_AdjustDoorDirection( lDirection );
	if ( lDirection == INT_MIN )
		return;

	NetCommand command ( SVC_DODOOR );
	command.addShort ( lSectorID );
	command.addByte ( (BYTE)type );
	command.addLong ( lSpeed );
	command.addByte ( lDirection );
	command.addShort ( lLightTag );
	command.addShort ( lID );
	command.sendCommandToClients ( ulPlayerExtra, ulFlags );
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
void SERVERCOMMANDS_DoFloor( DFloor::EFloor Type, sector_t *pSector, LONG lDirection, LONG lSpeed, LONG lFloorDestDist, LONG Crush, bool Hexencrush, LONG lID, ULONG ulPlayerExtra, ULONG ulFlags )
{
	LONG	lSectorID;

	lSectorID = LONG( pSector - sectors );
	if (( lSectorID < 0 ) || ( lSectorID >= numsectors ))
		return;

	// Since we still want to send direction as a byte, but -1 can't be represented in byte
	// form, adjust the value into something that can be represented.
	lDirection = SERVER_AdjustFloorDirection( lDirection );
	if ( lDirection == INT_MIN )
		return;

	NetCommand command ( SVC_DOFLOOR );
	command.addByte ( (ULONG)Type );
	command.addShort ( lSectorID );
	command.addByte ( lDirection );
	command.addLong ( lSpeed );
	command.addLong ( lFloorDestDist );
	command.addByte ( clamp<LONG>(Crush,-128,127) );
	command.addByte ( Hexencrush );
	command.addShort ( lID );
	command.sendCommandToClients ( ulPlayerExtra, ulFlags );
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
void SERVERCOMMANDS_DoCeiling( DCeiling::ECeiling Type, sector_t *pSector, LONG lDirection, LONG lBottomHeight, LONG lTopHeight, LONG lSpeed, LONG lCrush, bool Hexencrush, LONG lSilent, LONG lID, ULONG ulPlayerExtra, ULONG ulFlags )
{
	LONG	lSectorID;

	lSectorID = LONG( pSector - sectors );
	if (( lSectorID < 0 ) || ( lSectorID >= numsectors ))
		return;

	// Since we still want to send direction as a byte, but -1 can't be represented in byte
	// form, adjust the value into something that can be represented.
	lDirection = SERVER_AdjustCeilingDirection( lDirection );
	if ( lDirection == INT_MIN )
		return;

	NetCommand command ( SVC_DOCEILING );
	command.addByte ( (ULONG)Type );
	command.addShort ( lSectorID );
	command.addByte ( lDirection );
	command.addLong ( lBottomHeight );
	command.addLong ( lTopHeight );
	command.addLong ( lSpeed );
	command.addByte ( clamp<LONG>(lCrush,-128,127) );
	command.addByte ( Hexencrush );
	command.addShort ( lSilent );
	command.addShort ( lID );
	command.sendCommandToClients ( ulPlayerExtra, ulFlags );
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
void SERVERCOMMANDS_DoPillar( DPillar::EPillar Type, sector_t *pSector, LONG lFloorSpeed, LONG lCeilingSpeed, LONG lFloorTarget, LONG lCeilingTarget, LONG Crush, bool Hexencrush, LONG lID, ULONG ulPlayerExtra, ULONG ulFlags )
{
	LONG	lSectorID;

	lSectorID = LONG( pSector - sectors );
	if (( lSectorID < 0 ) || ( lSectorID >= numsectors ))
		return;

	NetCommand command ( SVC_DOPILLAR );
	command.addByte ( Type );
	command.addShort ( lSectorID );
	command.addLong ( lFloorSpeed );
	command.addLong ( lCeilingSpeed );
	command.addLong ( lFloorTarget );
	command.addLong ( lCeilingTarget );
	command.addByte ( clamp<LONG>(Crush,-128,127) );
	command.addByte ( Hexencrush );
	command.addShort ( lID );
	command.sendCommandToClients ( ulPlayerExtra, ulFlags );
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
//
void SERVERCOMMANDS_SetPolyDoorSpeedRotation( LONG lPolyNum, LONG lSpeed, LONG lAngle, ULONG ulPlayerExtra, ULONG ulFlags )
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

		SERVER_CheckClientBuffer( ulIdx, 11, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETPOLYDOORSPEEDROTATION );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lPolyNum );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lSpeed );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lAngle );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_PlayPolyobjSound( LONG lPolyNum, bool PolyMode, ULONG ulPlayerExtra, ULONG ulFlags )
{
	NetCommand command ( SVC_PLAYPOLYOBJSOUND );
	command.addShort ( lPolyNum );
	command.addByte ( PolyMode );
	command.sendCommandToClients ( ulPlayerExtra, ulFlags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_StopPolyobjSound( LONG lPolyNum, ULONG ulPlayerExtra, ULONG ulFlags )
{
	NetCommand command ( SVC_EXTENDEDCOMMAND );
	command.addByte ( SVC2_STOPPOLYOBJSOUND );
	command.addShort ( lPolyNum );
	command.sendCommandToClients ( ulPlayerExtra, ulFlags );
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
		SERVER_CheckClientBuffer( ulIdx, 2, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETQUEUEPOSITION );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ( lPosition != -1  ) ? lPosition : 255 );
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

	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
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
	const bool bIsEdited = SERVER_IsTranslationEdited ( ulTranslation );

	for ( ULONG ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		SERVER_CheckClientBuffer( ulIdx, 8, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_CREATETRANSLATION );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulTranslation );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, bIsEdited );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulStart );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulEnd );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulPal1 );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulPal2 );
	}
}
//*****************************************************************************
//
void SERVERCOMMANDS_CreateTranslation( ULONG ulTranslation, ULONG ulStart, ULONG ulEnd, ULONG ulR1, ULONG ulG1, ULONG ulB1, ULONG ulR2, ULONG ulG2, ULONG ulB2, ULONG ulPlayerExtra, ULONG ulFlags )
{
	const bool bIsEdited = SERVER_IsTranslationEdited ( ulTranslation );

	for ( ULONG ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		SERVER_CheckClientBuffer( ulIdx, 12, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_CREATETRANSLATION2 );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulTranslation );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, bIsEdited );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulStart );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulEnd );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulR1 );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulG1 );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulB1 );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulR2 );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulG2 );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulB2 );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_ReplaceTextures( int iFromname, int iToname, int iTexFlags, ULONG ulPlayerExtra, ULONG ulFlags )
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

		SERVER_CheckClientBuffer( ulIdx, 10, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_REPLACETEXTURES );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, iFromname );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, iToname );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, iTexFlags );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetSectorLink( ULONG ulSector, int iArg1, int iArg2, int iArg3, ULONG ulPlayerExtra, ULONG ulFlags )
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

		SERVER_CheckClientBuffer( ulIdx, 7, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_SETSECTORLINK );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulSector );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, iArg1 );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, iArg2 );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, iArg3 );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_DoPusher( ULONG ulType, line_t *pLine, int iMagnitude, int iAngle, AActor *pSource, int iAffectee, ULONG ulPlayerExtra, ULONG ulFlags )
{
	const int iLineNum = pLine ? static_cast<ULONG>( pLine - lines ) : -1;
	const LONG lSourceNetID = pSource ? pSource->lNetID : -1;

	for ( ULONG ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ((( ulFlags & SVCF_SKIPTHISCLIENT ) && ( ulPlayerExtra == ulIdx )) ||
			(( ulFlags & SVCF_ONLYTHISCLIENT ) && ( ulPlayerExtra != ulIdx )))
		{
			continue;
		}

		SERVER_CheckClientBuffer( ulIdx, 16, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_DOPUSHER );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulType );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, iLineNum );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, iMagnitude );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, iAngle );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, lSourceNetID );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, iAffectee );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_AdjustPusher( int iTag, int iMagnitude, int iAngle, ULONG ulType, ULONG ulPlayerExtra, ULONG ulFlags )
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

		SERVER_CheckClientBuffer( ulIdx, 12, true );
		NETWORK_WriteHeader( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, SVC_ADJUSTPUSHER );
		NETWORK_WriteShort( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, iTag );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, iMagnitude );
		NETWORK_WriteLong( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, iAngle );
		NETWORK_WriteByte( &SERVER_GetClient( ulIdx )->PacketBuffer.ByteStream, ulType );
	}
}


//*****************************************************************************
// [Dusk]
void SERVERCOMMANDS_SetPlayerHazardCount ( ULONG ulPlayer, ULONG ulPlayerExtra, ULONG ulFlags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	NetCommand command ( SVC_EXTENDEDCOMMAND );
	command.addByte ( SVC2_SETPLAYERHAZARDCOUNT );
	command.addByte ( ulPlayer );
	command.addShort ( players[ulPlayer].hazardcount );
	command.sendCommandToClients ( ulPlayerExtra, ulFlags );
}


//*****************************************************************************
// [Dusk] Used in map resets to move a 3d midtexture moves without sector it's attached to.
void SERVERCOMMANDS_Scroll3dMidtexture ( sector_t* sector, fixed_t move, bool ceiling, ULONG ulPlayerExtra, ULONG ulFlags )
{
	NetCommand command ( SVC_EXTENDEDCOMMAND );
	command.addByte ( SVC2_SCROLL3DMIDTEX );
	command.addByte ( sector - sectors );
	command.addLong ( move );
	command.addByte ( ceiling );
	command.sendCommandToClients ( ulPlayerExtra, ulFlags );
}

//*****************************************************************************
void SERVERCOMMANDS_SetPlayerLogNumber ( const ULONG ulPlayer, const int Arg0, ULONG ulPlayerExtra, ULONG ulFlags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	NetCommand command ( SVC_EXTENDEDCOMMAND );
	command.addByte ( SVC2_SETPLAYERLOGNUMBER );
	command.addByte ( ulPlayer );
	command.addShort ( Arg0 );
	command.sendCommandToClients ( ulPlayerExtra, ulFlags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetCVar( const FBaseCVar &CVar, ULONG ulPlayerExtra, ULONG ulFlags )
{
	NetCommand command( SVC_EXTENDEDCOMMAND );
	command.addByte( SVC2_SETCVAR );
	command.addString( CVar.GetName() );
	command.addString( CVar.GetGenericRep (CVAR_String).String );
	command.sendCommandToClients( ulPlayerExtra, ulFlags );
}

//*****************************************************************************
void SERVERCOMMANDS_SRPUserStartAuthentication ( const ULONG ulClient )
{
	if ( SERVER_IsValidClient( ulClient ) == false )
		return;

	CLIENT_s *pClient = SERVER_GetClient ( ulClient );
	NetCommand command ( SVC_EXTENDEDCOMMAND );
	command.addByte ( SVC2_SRP_USER_START_AUTHENTICATION );
	command.addString ( pClient->username.GetChars() );
	command.sendCommandToClients ( ulClient, SVCF_ONLYTHISCLIENT );
}

//*****************************************************************************
void SERVERCOMMANDS_SRPUserProcessChallenge ( const ULONG ulClient )
{
	if ( SERVER_IsValidClient( ulClient ) == false )
		return;

	CLIENT_s *pClient = SERVER_GetClient ( ulClient );
	NetCommand command ( SVC_EXTENDEDCOMMAND );
	command.addByte ( SVC2_SRP_USER_PROCESS_CHALLENGE );
	command.addByte ( pClient->salt.Size() );
	for ( unsigned int i = 0; i < pClient->salt.Size(); ++i )
		command.addByte ( pClient->salt[i] );
	command.addShort ( pClient->bytesB.Size() );
	for ( unsigned int i = 0; i < pClient->bytesB.Size(); ++i )
		command.addByte ( pClient->bytesB[i] );
	command.sendCommandToClients ( ulClient, SVCF_ONLYTHISCLIENT );
}

//*****************************************************************************
void SERVERCOMMANDS_SRPUserVerifySession ( const ULONG ulClient )
{
	if ( SERVER_IsValidClient( ulClient ) == false )
		return;

	CLIENT_s *pClient = SERVER_GetClient ( ulClient );
	NetCommand command ( SVC_EXTENDEDCOMMAND );
	command.addByte ( SVC2_SRP_USER_VERIFY_SESSION );
	command.addShort ( pClient->bytesHAMK.Size() );
	for ( unsigned int i = 0; i < pClient->bytesHAMK.Size(); ++i )
		command.addByte ( pClient->bytesHAMK[i] );
	command.sendCommandToClients ( ulClient, SVCF_ONLYTHISCLIENT );
}

//*****************************************************************************
void APathFollower::SyncWithClient ( const ULONG ulClient )
{
	if ( !EnsureActorHasNetID (this) )
		return;

	NetCommand command( SVC_EXTENDEDCOMMAND );
	command.addByte( SVC2_SYNCPATHFOLLOWER );
	command.addShort( this->lNetID );
	command.addShort( this->CurrNode ? this->CurrNode->lNetID : -1 );
	command.addShort( this->PrevNode ? this->PrevNode->lNetID : -1 );
	command.addFloat( this->Time );
	command.sendCommandToClients( ulClient, SVCF_ONLYTHISCLIENT );
}
