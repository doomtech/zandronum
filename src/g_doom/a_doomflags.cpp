//-----------------------------------------------------------------------------
//
// Skulltag Source
// Copyright (C) 2002-2006 Brad Carney
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
// Date created:  7/5/06
//
//
// Filename: a_doomflags.cpp
//
// Description: Contains definitions for the flags, as well as skulltag's skulls.
//
//-----------------------------------------------------------------------------

#include "a_doomglobal.h"
#include "announcer.h"
#include "cl_demo.h"
#include "cl_main.h"
#include "network.h"
#include "p_acs.h"
#include "sbar.h"
#include "scoreboard.h"
#include "sv_commands.h"
#include "team.h"
#include "v_text.h"
#include "v_video.h"

//*****************************************************************************
//	DEFINES

#define	DENY_PICKUP			0
#define	ALLOW_PICKUP		1
#define	RETURN_FLAG			2

// Base flag ----------------------------------------------------------------

IMPLEMENT_STATELESS_ACTOR( AFlag, Any, -1, 0 )
 PROP_Inventory_FlagsSet( IF_INTERHUBSTRIP )
 PROP_Inventory_PickupSound( "misc/k_pkup" )
END_DEFAULTS

//===========================================================================
//
// AFlag :: ShouldRespawn
//
// A flag should never respawn, so this function should always return false.
//
//===========================================================================

bool AFlag::ShouldRespawn( )
{
	return ( false );
}

//===========================================================================
//
// AFlag :: TryPickup
//
//===========================================================================

bool AFlag::TryPickup( AActor *pToucher )
{
	AInventory	*pCopy;
	AInventory	*pInventory;

	// If we're not in teamgame mode, just use the default pickup handling.
	if ( teamgame == false )
		return ( Super::TryPickup( pToucher ));

	// First, check to see if any of the toucher's inventory items want to
	// handle the picking up of this flag (other flags, perhaps?).

	// If HandlePickup() returns true, it will set the IF_PICKUPGOOD flag
	// to indicate that this item has been picked up. If the item cannot be
	// picked up, then it leaves the flag cleared.
	ItemFlags &= ~IF_PICKUPGOOD;
	if (( pToucher->Inventory != NULL ) && ( pToucher->Inventory->HandlePickup( this )))
	{
		// Let something else the player is holding intercept the pickup.
		if (( ItemFlags & IF_PICKUPGOOD ) == false )
			return ( false );

		ItemFlags &= ~IF_PICKUPGOOD;
		GoAwayAndDie( );

		// Nothing more to do in this case.
		return ( true );
	}

	// Only players that are on a team may pickup flags.
	if (( pToucher->player == NULL ) || ( pToucher->player->bOnTeam == false ))
		return ( false );

	switch ( AllowFlagPickup( pToucher ))
	{
	case DENY_PICKUP:

		// If we're not allowed to pickup this flag, return false.
		return ( false );
	case RETURN_FLAG:

		// Execute the return scripts.
		if (( NETWORK_GetState( ) != NETSTATE_CLIENT ) && ( CLIENTDEMO_IsPlaying( ) == false ))
		{
			// I don't like these hacks :((((((((((((((
			if ( this->GetClass( ) == PClass::FindClass( "WhiteFlag" ))
				FBehavior::StaticStartTypedScripts( SCRIPT_WhiteReturn, NULL, true );
			else if (( this->GetClass( ) == PClass::FindClass( "BlueFlag" )) || ( this->GetClass( ) == PClass::FindClass( "BlueSkullST" )))
				FBehavior::StaticStartTypedScripts( TEAM_GetReturnScriptOffset( TEAM_BLUE ), NULL, true );
			else if (( this->GetClass( ) == PClass::FindClass( "RedFlag" )) || ( this->GetClass( ) == PClass::FindClass( "RedSkullST" )))
				FBehavior::StaticStartTypedScripts( TEAM_GetReturnScriptOffset( TEAM_RED ), NULL, true );
		}

		// In non-simple CTF mode, scripts take care of the returning and displaying messages.
		if ( TEAM_GetSimpleCTFMode( ) || TEAM_GetSimpleSTMode( ))
		{
			if (( NETWORK_GetState( ) != NETSTATE_CLIENT ) &&
				( CLIENTDEMO_IsPlaying( ) == false ))
			{
				// The player is touching his own dropped flag; return it now.
				ReturnFlag( pToucher );

				// Mark the flag as no longer being taken.
				MarkFlagTaken( false );
			}

			// Display text saying that the flag has been returned.
			DisplayFlagReturn( );
		}

		// Reset the return ticks for this flag.
		ResetReturnTicks( );

		// Announce that the flag has been returned.
		AnnounceFlagReturn( );

		// Delete the flag.
		GoAwayAndDie( );

		// If we're the server, tell clients to destroy the flag.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_DestroyThing( this );

		// Tell clients that the flag has been returned.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		{
			// I don't like these hacks :((((((((((((((
			if ( this->GetClass( ) == PClass::FindClass( "WhiteFlag" ))
				SERVERCOMMANDS_TeamFlagReturned( NUM_TEAMS );
			else if (( this->GetClass( ) == PClass::FindClass( "BlueFlag" )) || ( this->GetClass( ) == PClass::FindClass( "BlueSkullST" )))
				SERVERCOMMANDS_TeamFlagReturned( TEAM_BLUE );
			else if (( this->GetClass( ) == PClass::FindClass( "RedFlag" )) || ( this->GetClass( ) == PClass::FindClass( "RedSkullST" )))
				SERVERCOMMANDS_TeamFlagReturned( TEAM_RED );
		}
		else
			SCOREBOARD_RefreshHUD( );

		return ( false );
	}

	// Announce the pickup of this flag.
	AnnounceFlagPickup( pToucher );

	// Player is picking up the flag.
	if (( NETWORK_GetState( ) != NETSTATE_CLIENT ) && ( CLIENTDEMO_IsPlaying( ) == false ))
	{
		FBehavior::StaticStartTypedScripts( SCRIPT_Pickup, pToucher, true );

		// If we're in simple CTF mode, we need to display the pickup messages.
		if ( TEAM_GetSimpleCTFMode( ) || TEAM_GetSimpleSTMode( ))
		{
			// Display the flag taken message.
			DisplayFlagTaken( pToucher );

			// Also, mark the flag as being taken.
			MarkFlagTaken( true );
		}

		// Reset the return ticks for this flag.
		ResetReturnTicks( );

		// Also, refresh the HUD.
		SCOREBOARD_RefreshHUD( );
	}

	pCopy = CreateCopy( pToucher );
	if ( pCopy == NULL )
		return ( false );

	pCopy->AttachToOwner( pToucher );

	// When we pick up a flag, take away any invisibility objects the player has.
	pInventory = pToucher->Inventory;
	while ( pInventory )
	{
		if (( pInventory->IsKindOf( RUNTIME_CLASS( APowerInvisibility ))) ||
			( pInventory->IsKindOf( RUNTIME_CLASS( APowerTranslucency ))))
		{
			// If we're the server, tell clients to destroy this inventory item.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_TakeInventory( ULONG( pToucher->player - players ), pInventory->GetClass( )->TypeName.GetChars( ), 0 );

			pInventory->Destroy( );
		}

		pInventory = pInventory->Inventory;
	}

	return ( true );
}

//===========================================================================
//
// AFlag :: HandlePickup
//
//===========================================================================

bool AFlag::HandlePickup( AInventory *pItem )
{
	// Don't allow the pickup of invisibility objects when carrying a flag.
	if (( pItem->IsKindOf( RUNTIME_CLASS( APowerInvisibility ))) ||
		( pItem->IsKindOf( RUNTIME_CLASS( APowerTranslucency ))))
	{
		ItemFlags &= ~IF_PICKUPGOOD;

		return ( true );
	}

	return ( Super::HandlePickup( pItem ));
}

//===========================================================================
//
// AFlag :: AllowFlagPickup
//
// Determine whether or not we should be allowed to pickup this flag.
//
//===========================================================================

LONG AFlag::AllowFlagPickup( AActor *pToucher )
{
	return ( ALLOW_PICKUP );
}

//===========================================================================
//
// AFlag :: AnnounceFlagPickup
//
// Play the announcer sound for picking up this flag.
//
//===========================================================================

void AFlag::AnnounceFlagPickup( AActor *pToucher )
{
}

//===========================================================================
//
// AFlag :: DisplayFlagTaken
//
// Display the text for picking up this flag.
//
//===========================================================================

void AFlag::DisplayFlagTaken( AActor *pToucher )
{
}

//===========================================================================
//
// AFlag :: MarkFlagTaken
//
// Signal to the team module whether or not this flag has been taken.
//
//===========================================================================

void AFlag::MarkFlagTaken( bool bTaken )
{
}

//===========================================================================
//
// AFlag :: ResetReturnTicks
//
// Reset the return ticks for the team associated with this flag.
//
//===========================================================================

void AFlag::ResetReturnTicks( void )
{
}

//===========================================================================
//
// AFlag :: ReturnFlag
//
// Spawn a new flag at its original location.
//
//===========================================================================

void AFlag::ReturnFlag( AActor *pReturner )
{
}

//===========================================================================
//
// AFlag :: AnnounceFlagReturn
//
// Play the announcer sound for this flag being returned.
//
//===========================================================================

void AFlag::AnnounceFlagReturn( void )
{
}

//===========================================================================
//
// AFlag :: DisplayFlagReturn
//
// Display the text for this flag being returned.
//
//===========================================================================

void AFlag::DisplayFlagReturn( void )
{
}

// White flag ---------------------------------------------------------------

class AWhiteFlag : public AFlag
{
	DECLARE_ACTOR( AWhiteFlag, AFlag )
protected:

	virtual bool HandlePickup( AInventory *pItem );
	virtual LONG AllowFlagPickup( AActor *pToucher );
	virtual void AnnounceFlagPickup( AActor *pToucher );
	virtual void DisplayFlagTaken( AActor *pToucher );
	virtual void MarkFlagTaken( bool bTaken );
	virtual void ResetReturnTicks( void );
	virtual void ReturnFlag( AActor *pReturner );
	virtual void AnnounceFlagReturn( void );
	virtual void DisplayFlagReturn( void );
};

FState AWhiteFlag::States[] =
{
	S_NORMAL (WFLA, 'A',   3, NULL 				, &States[1]),
	S_NORMAL (WFLA, 'B',   3, NULL 				, &States[2]),
	S_NORMAL (WFLA, 'C',   3, NULL 				, &States[3]),
	S_BRIGHT (WFLA, 'D',   3, NULL 				, &States[4]),
	S_BRIGHT (WFLA, 'E',   3, NULL 				, &States[5]),
	S_BRIGHT (WFLA, 'F',   3, NULL 				, &States[0]),
};

IMPLEMENT_ACTOR( AWhiteFlag, Any, 5132, 179 )
	PROP_RadiusFixed( 20 )
	PROP_HeightFixed( 16 )
	PROP_Flags( MF_SPECIAL|MF_NOTDMATCH )
	PROP_SpawnState( 0 )

	PROP_Inventory_PickupMessage( "$PICKUP_WHITEFLAG" )
	PROP_Inventory_Icon( "WFLAB0" )
END_DEFAULTS

//===========================================================================
//
// AWhiteFlag :: HandlePickup
//
// Ask this item in the actor's inventory to potentially react to this object
// attempting to be picked up.
//
//===========================================================================

bool AWhiteFlag::HandlePickup( AInventory *pItem )
{
	char				szString[256];
	DHUDMessageFadeOut	*pMsg;
	AInventory			*pInventory;

	// If this object being given isn't a flag, then we don't really care.
	if ( pItem->GetClass( )->IsDescendantOf( RUNTIME_CLASS( AFlag )) == false )
		return ( Super::HandlePickup( pItem ));

	// If this isn't one flag CTF mode, then we don't really care here.
	if ( oneflagctf == false )
		return ( Super::HandlePickup( pItem ));

	// If we're trying to pick up the opponent's flag, award a point since we're
	// carrying the white flag.
	if ( pItem->GetClass( ) == TEAM_GetFlagItem( !Owner->player->ulTeam ))
	{
		if (( TEAM_GetSimpleCTFMode( )) && ( NETWORK_GetState( ) != NETSTATE_CLIENT ) && ( CLIENTDEMO_IsPlaying( ) == false ))
		{
			// Give his team a point.
			TEAM_SetScore( Owner->player->ulTeam, TEAM_GetScore( Owner->player->ulTeam ) + 1, true );
			Owner->player->lPointCount++;

			// Award the scorer with a "Capture!" medal.
			MEDAL_GiveMedal( ULONG( Owner->player - players ), MEDAL_CAPTURE );

			// Tell clients about the medal that been given.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_GivePlayerMedal( ULONG( Owner->player - players ), MEDAL_CAPTURE );

			// If someone just recently returned the flag, award him with an "Assist!" medal.
			if ( TEAM_GetAssistPlayer( Owner->player->ulTeam ) != MAXPLAYERS )
			{
				MEDAL_GiveMedal( TEAM_GetAssistPlayer( Owner->player->ulTeam ), MEDAL_ASSIST );

				// Tell clients about the medal that been given.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_GivePlayerMedal( TEAM_GetAssistPlayer( Owner->player->ulTeam ), MEDAL_ASSIST );

				TEAM_SetAssistPlayer( Owner->player->ulTeam, MAXPLAYERS );
			}

			// Create the "captured" message.
			if ( Owner->player->ulTeam == TEAM_BLUE )
				sprintf( szString, "\\cHBlue team scores!" );
			else
				sprintf( szString, "\\cGRed team scores!" );
			V_ColorizeString( szString );

			// Now, print it.
			if ( NETWORK_GetState( ) != NETSTATE_SERVER )
			{
				screen->SetFont( BigFont );
				pMsg = new DHUDMessageFadeOut( szString,
					1.5f,
					TEAM_MESSAGE_Y_AXIS,
					0,
					0,
					CR_WHITE,
					3.0f,
					0.25f );
				StatusBar->AttachMessage( pMsg, MAKE_ID('C','N','T','R') );
				screen->SetFont( SmallFont );
			}
			// If necessary, send it to clients.
			else
			{
				SERVERCOMMANDS_SetPlayerPoints( ULONG( Owner->player - players ));
				SERVERCOMMANDS_PrintHUDMessageFadeOut( szString, 1.5f, TEAM_MESSAGE_Y_AXIS, 0, 0, CR_WHITE, 3.0f, 0.25f, "BigFont", false, MAKE_ID('C','N','T','R') );
			}

			// [BC] Rivecoder's "scored by" message.
			if ( Owner->player->ulTeam == TEAM_BLUE )
				sprintf( szString, "\\cHScored by: %s", Owner->player->userinfo.netname );
			else
				sprintf( szString, "\\cGScored by: %s", Owner->player->userinfo.netname );

			V_ColorizeString( szString );

			// Now, print it.
			if ( NETWORK_GetState( ) != NETSTATE_SERVER )
			{
				screen->SetFont( SmallFont );
				pMsg = new DHUDMessageFadeOut( szString,
					1.5f,
					TEAM_MESSAGE_Y_AXIS_SUB,
					0,
					0,
					CR_BLUE,
					3.0f,
					0.5f );
				StatusBar->AttachMessage( pMsg, MAKE_ID('S','U','B','S') );
			}
			// If necessary, send it to clients.
			else
				SERVERCOMMANDS_PrintHUDMessageFadeOut( szString, 1.5f, TEAM_MESSAGE_Y_AXIS_SUB, 0, 0, CR_BLUE, 3.0f, 0.5f, "SmallFont", false, MAKE_ID('S','U','B','S') );

			// Take the flag away.
			pInventory = Owner->FindInventory( this->GetClass( ));
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_TakeInventory( ULONG( Owner->player - players ), this->GetClass( )->TypeName.GetChars( ), 0 );
			if ( pInventory )
				Owner->RemoveInventory( pInventory );

			this->ReturnFlag( NULL );

			// Also, refresh the HUD.
			SCOREBOARD_RefreshHUD( );
		}

		return ( true );
	}

	return ( Super::HandlePickup( pItem ));
}

//===========================================================================
//
// AWhiteFlag :: AllowFlagPickup
//
// Determine whether or not we should be allowed to pickup this flag.
//
//===========================================================================

LONG AWhiteFlag::AllowFlagPickup( AActor *pToucher )
{
	return ( ALLOW_PICKUP );
}

//===========================================================================
//
// AWhiteFlag :: AnnounceFlagPickup
//
// Play the announcer sound for picking up this flag.
//
//===========================================================================

void AWhiteFlag::AnnounceFlagPickup( AActor *pToucher )
{
	// Don't announce the pickup if the flag is being given to someone as part of a snapshot.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) && ( CLIENT_GetConnectionState( ) == CTS_RECEIVINGSNAPSHOT ))
		return;

	if (( pToucher == NULL ) || ( pToucher->player == NULL ))
		return;

	if ( playeringame[consoleplayer] && players[consoleplayer].bOnTeam && players[consoleplayer].mo )
	{
		if (( pToucher->player - players ) == consoleplayer )
			ANNOUNCER_PlayEntry( cl_announcer, "YouHaveTheFlag" );
		else if ( players[consoleplayer].mo->IsTeammate( pToucher ))
			ANNOUNCER_PlayEntry( cl_announcer, "YourTeamHasTheFlag" );
		else
			ANNOUNCER_PlayEntry( cl_announcer, "TheEnemyHasTheFlag" );
	}
}

//===========================================================================
//
// AWhiteFlag :: DisplayFlagTaken
//
// Display the text for picking up this flag.
//
//===========================================================================

void AWhiteFlag::DisplayFlagTaken( AActor *pToucher )
{
	ULONG				ulPlayer;
	char				szString[256];
	char				szName[48];
	DHUDMessageFadeOut	*pMsg;

	// Create the "pickup" message.
	if (( pToucher != NULL ) && ( pToucher->player != NULL ) && ( pToucher->player - players ) == consoleplayer )
		sprintf( szString, "\\cCYou have the flag!" );
	else
		sprintf( szString, "\\cCWhite flag taken!" );
	V_ColorizeString( szString );

	// Now, print it.
	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
	{
		screen->SetFont( BigFont );
		pMsg = new DHUDMessageFadeOut( szString,
			1.5f,
			TEAM_MESSAGE_Y_AXIS,
			0,
			0,
			CR_WHITE,
			3.0f,
			0.25f );
		StatusBar->AttachMessage( pMsg, MAKE_ID('C','N','T','R') );
		screen->SetFont( SmallFont );
	}
	// If necessary, send it to clients.
	else
	{
		sprintf( szString, "\\cCYou have the flag!" );
		V_ColorizeString( szString );
		SERVERCOMMANDS_PrintHUDMessageFadeOut( szString, 1.5f, TEAM_MESSAGE_Y_AXIS, 0, 0, CR_WHITE, 3.0f, 0.25f, "BigFont", false, MAKE_ID('C','N','T','R'), ULONG( pToucher->player - players ), SVCF_ONLYTHISCLIENT );

		sprintf( szString, "\\cCWhite flag taken!" );
		V_ColorizeString( szString );
		SERVERCOMMANDS_PrintHUDMessageFadeOut( szString, 1.5f, TEAM_MESSAGE_Y_AXIS, 0, 0, CR_WHITE, 3.0f, 0.25f, "BigFont", false, MAKE_ID('C','N','T','R'), ULONG( pToucher->player - players ), SVCF_SKIPTHISCLIENT );
	}

	// [BC] Rivecoder's "held by" messages.
	ulPlayer = ULONG( pToucher->player - players );
	sprintf( szName, "%s", players[ulPlayer].userinfo.netname );
	V_RemoveColorCodes( szName );

	if ( players[ulPlayer].ulTeam == TEAM_BLUE )
		sprintf( szString, "\\ccHeld by: \\ch%s", szName );
	else
		sprintf( szString, "\\ccHeld by: \\cg%s", szName );

	V_ColorizeString( szString );

	// Now, print it.
	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
	{
		if (( pToucher->player - players ) != consoleplayer )
		{
			screen->SetFont( SmallFont );
			pMsg = new DHUDMessageFadeOut( szString,
				1.5f,
				TEAM_MESSAGE_Y_AXIS_SUB,
				0,
				0,
				CR_BLUE,
				3.0f,
				0.25f );
			StatusBar->AttachMessage( pMsg, MAKE_ID('S','U','B','S') );
		}
	}
	// If necessary, send it to clients.
	else
		SERVERCOMMANDS_PrintHUDMessageFadeOut( szString, 1.5f, TEAM_MESSAGE_Y_AXIS_SUB, 0, 0, CR_BLUE, 3.0f, 0.25f, "SmallFont", false, MAKE_ID('S','U','B','S'), ULONG( pToucher->player - players ), SVCF_SKIPTHISCLIENT  );
}

//===========================================================================
//
// AWhiteFlag :: MarkFlagTaken
//
// Signal to the team module whether or not this flag has been taken.
//
//===========================================================================

void AWhiteFlag::MarkFlagTaken( bool bTaken )
{
	TEAM_SetWhiteFlagTaken( bTaken );
}

//===========================================================================
//
// AWhiteFlag :: ResetReturnTicks
//
// Reset the return ticks for the team associated with this flag.
//
//===========================================================================

void AWhiteFlag::ResetReturnTicks( void )
{
	TEAM_SetReturnTicks( NUM_TEAMS, 0 );
}

//===========================================================================
//
// AWhiteFlag :: ReturnFlag
//
// Spawn a new flag at its original location.
//
//===========================================================================

void AWhiteFlag::ReturnFlag( AActor *pReturner )
{
	POS_t	WhiteFlagOrigin;
	AActor	*pActor;

	// Respawn the white flag.
	WhiteFlagOrigin = TEAM_GetWhiteFlagOrigin( );
	pActor = Spawn( this->GetClass( ), WhiteFlagOrigin.x, WhiteFlagOrigin.y, WhiteFlagOrigin.z, NO_REPLACE );

	// If we're the server, tell clients to spawn the new skull.
	if (( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( pActor ))
		SERVERCOMMANDS_SpawnThing( pActor );

	// Since all inventory spawns with the MF_DROPPED flag, we need to unset it.
	if ( pActor )
		pActor->flags &= ~MF_DROPPED;

	// Mark the white flag as no longer being taken.
	TEAM_SetWhiteFlagTaken( false );
}

//===========================================================================
//
// AWhiteFlag :: AnnounceFlagReturn
//
// Play the announcer sound for this flag being returned.
//
//===========================================================================

void AWhiteFlag::AnnounceFlagReturn( void )
{
	ANNOUNCER_PlayEntry( cl_announcer, "WhiteFlagReturned" );
}

//===========================================================================
//
// AWhiteFlag :: DisplayFlagReturn
//
// Display the text for this flag being returned.
//
//===========================================================================

void AWhiteFlag::DisplayFlagReturn( void )
{
	char						szString[256];
	DHUDMessageFadeOut			*pMsg;

	// Create the "returned" message.
	sprintf( szString, "\\cCWhite flag returned" );
	V_ColorizeString( szString );

	// Now, print it.
	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
	{
		screen->SetFont( BigFont );
		pMsg = new DHUDMessageFadeOut( szString,
			1.5f,
			TEAM_MESSAGE_Y_AXIS,
			0,
			0,
			CR_WHITE,
			3.0f,
			0.25f );
		StatusBar->AttachMessage( pMsg, MAKE_ID('C','N','T','R') );
		screen->SetFont( SmallFont );
	}
}

// Blue flag ----------------------------------------------------------------

class ABlueFlag : public AFlag
{
	DECLARE_ACTOR( ABlueFlag, AFlag )
protected:

	virtual bool HandlePickup( AInventory *pItem );
	virtual LONG AllowFlagPickup( AActor *pToucher );
	virtual void AnnounceFlagPickup( AActor *pToucher );
	virtual void DisplayFlagTaken( AActor *pToucher );
	virtual void MarkFlagTaken( bool bTaken );
	virtual void ResetReturnTicks( void );
	virtual void ReturnFlag( AActor *pReturner );
	virtual void AnnounceFlagReturn( void );
	virtual void DisplayFlagReturn( void );
};

FState ABlueFlag::States[] =
{
	S_NORMAL (BFLA, 'A',   3, NULL 				, &States[1]),
	S_NORMAL (BFLA, 'B',   3, NULL 				, &States[2]),
	S_NORMAL (BFLA, 'C',   3, NULL 				, &States[3]),
	S_BRIGHT (BFLA, 'D',   3, NULL 				, &States[4]),
	S_BRIGHT (BFLA, 'E',   3, NULL 				, &States[5]),
	S_BRIGHT (BFLA, 'F',   3, NULL 				, &States[0]),
};

IMPLEMENT_ACTOR( ABlueFlag, Any, 5130, 177 )
	PROP_RadiusFixed( 20 )
	PROP_HeightFixed( 16 )
	PROP_Flags( MF_SPECIAL|MF_NOTDMATCH )
	PROP_SpawnState( 0 )

	PROP_Inventory_PickupMessage( "$PICKUP_BLUEFLAG" )
	PROP_Inventory_Icon( "BFLAB0" )
END_DEFAULTS

//===========================================================================
//
// ABlueFlag :: HandlePickup
//
// Ask this item in the actor's inventory to potentially react to this object
// attempting to be picked up.
//
//===========================================================================

bool ABlueFlag::HandlePickup( AInventory *pItem )
{
	char				szString[256];
	DHUDMessageFadeOut	*pMsg;
	AInventory			*pInventory;

	// If this object being given isn't a flag, then we don't really care.
	if ( pItem->GetClass( )->IsDescendantOf( RUNTIME_CLASS( AFlag )) == false )
		return ( Super::HandlePickup( pItem ));

	// If we're carrying the opposing team's flag, and trying to pick up our flag,
	// then that means we've captured the flag. Award a point.
	if (( this->GetClass( ) == TEAM_GetFlagItem( !Owner->player->ulTeam )) &&
		( pItem->GetClass( ) == TEAM_GetFlagItem( Owner->player->ulTeam )))
	{
		// Don't award a point if we're touching a dropped version of our flag.
		if ( static_cast<AFlag *>( pItem )->AllowFlagPickup( Owner ) == RETURN_FLAG )
			return ( Super::HandlePickup( pItem ));

		if (( TEAM_GetSimpleCTFMode( )) && ( NETWORK_GetState( ) != NETSTATE_CLIENT ) && ( CLIENTDEMO_IsPlaying( ) == false ))
		{
			// Give his team a point.
			TEAM_SetScore( Owner->player->ulTeam, TEAM_GetScore( Owner->player->ulTeam ) + 1, true );
			Owner->player->lPointCount++;

			// Award the scorer with a "Capture!" medal.
			MEDAL_GiveMedal( ULONG( Owner->player - players ), MEDAL_CAPTURE );

			// Tell clients about the medal that been given.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_GivePlayerMedal( ULONG( Owner->player - players ), MEDAL_CAPTURE );

			// [RC] Clear the 'returned automatically' message. A bit hackish, but leaves the flag structure unchanged.
			this->ReturnFlag( NULL );
			if ( NETWORK_GetState( ) != NETSTATE_SERVER )
			{
				screen->SetFont( SmallFont );
				pMsg = new DHUDMessageFadeOut( "", 1.5f, TEAM_MESSAGE_Y_AXIS_SUB, 0, 0, CR_RED, 3.0f, 0.5f );
				StatusBar->AttachMessage( pMsg, MAKE_ID('S','U','B','S') );
			}
			// If necessary, send it to clients.
			else
				SERVERCOMMANDS_PrintHUDMessageFadeOut( "", 1.5f, TEAM_MESSAGE_Y_AXIS_SUB, 0, 0, CR_RED, 3.0f, 0.5f, "SmallFont", false, MAKE_ID('S','U','B','S') );

			// Create the "captured" message.
			sprintf( szString, "\\cGRed team scores!" );
			V_ColorizeString( szString );

			// Now, print it.
			if ( NETWORK_GetState( ) != NETSTATE_SERVER )
			{
				screen->SetFont( BigFont );
				pMsg = new DHUDMessageFadeOut( szString,
					1.5f,
					TEAM_MESSAGE_Y_AXIS,
					0,
					0,
					CR_RED,
					3.0f,
					0.5f );
				StatusBar->AttachMessage( pMsg, MAKE_ID('C','N','T','R') );
				screen->SetFont( SmallFont );
			}
			// If necessary, send it to clients.
			else
			{
				SERVERCOMMANDS_SetPlayerPoints( ULONG( Owner->player - players ));
				SERVERCOMMANDS_PrintHUDMessageFadeOut( szString, 1.5f, TEAM_MESSAGE_Y_AXIS, 0, 0, CR_RED, 3.0f, 0.5f, "BigFont", false, MAKE_ID('C','N','T','R') );
			}

			// [RC] Create the "scored by" and "assisted by" message.
			sprintf( szString, "\\cGScored by: %s", Owner->player->userinfo.netname );
			if(TEAM_GetAssistPlayer(Owner->player->ulTeam) != MAXPLAYERS)
			{
				bool selfAssist = false;
				for(ULONG i = 0; i < MAXPLAYERS; i++)
					if(&players[i] == Owner->player)
						if( TEAM_GetAssistPlayer( Owner->player->ulTeam) == i)
							selfAssist = true;

				if ( selfAssist )
					sprintf( szString, "%s\\n\\cG[ Self-Assisted ]", szString);
				else
					sprintf( szString, "%s\\n\\cGAssisted by: %s", szString, players[TEAM_GetAssistPlayer( Owner->player->ulTeam )].userinfo.netname);
			}

			V_ColorizeString( szString );

			// Now, print it.
			if ( NETWORK_GetState( ) != NETSTATE_SERVER )
			{
				screen->SetFont( SmallFont );
				pMsg = new DHUDMessageFadeOut( szString,
					1.5f,
					TEAM_MESSAGE_Y_AXIS_SUB,
					0,
					0,
					CR_RED,
					3.0f,
					0.5f );
				StatusBar->AttachMessage( pMsg, MAKE_ID('S','U','B','S') );
			}
			// If necessary, send it to clients.
			else
				SERVERCOMMANDS_PrintHUDMessageFadeOut( szString, 1.5f, TEAM_MESSAGE_Y_AXIS_SUB, 0, 0, CR_RED, 3.0f, 0.5f, "SmallFont", false, MAKE_ID('S','U','B','S') );

			
			// If someone just recently returned the flag, award him with an "Assist!" medal.
			if ( TEAM_GetAssistPlayer( Owner->player->ulTeam ) != MAXPLAYERS )
			{
				MEDAL_GiveMedal( TEAM_GetAssistPlayer( Owner->player->ulTeam ), MEDAL_ASSIST );

				// Tell clients about the medal that been given.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_GivePlayerMedal( TEAM_GetAssistPlayer( Owner->player->ulTeam ), MEDAL_ASSIST );

				TEAM_SetAssistPlayer( Owner->player->ulTeam, MAXPLAYERS );
			}

			// Take the flag away.
			pInventory = Owner->FindInventory( this->GetClass( ));
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_TakeInventory( ULONG( Owner->player - players ), TEAM_GetFlagItem( !Owner->player->ulTeam )->TypeName.GetChars( ), 0 );
			if ( pInventory )
				Owner->RemoveInventory( pInventory );


			// Also, refresh the HUD.
			SCOREBOARD_RefreshHUD( );
		}

		return ( true );
	}

	return ( Super::HandlePickup( pItem ));
}

//===========================================================================
//
// ABlueFlag :: AllowFlagPickup
//
// Determine whether or not we should be allowed to pickup this flag.
//
//===========================================================================

LONG ABlueFlag::AllowFlagPickup( AActor *pToucher )
{
	// Don't allow the pickup of red/blue flags in One Flag CTF.
	if ( oneflagctf )
		return ( DENY_PICKUP );

	if (( pToucher == NULL ) || ( pToucher->player == NULL ) || ( pToucher->player->bOnTeam == false ))
		return ( DENY_PICKUP );

	// Player is touching the enemy flag.
	if ( this->GetClass( ) == TEAM_GetFlagItem( !pToucher->player->ulTeam ))
		return ( ALLOW_PICKUP );

	// Player is touching his own flag. If it's dropped, allow him to return it.
	if (( this->GetClass( ) == TEAM_GetFlagItem( pToucher->player->ulTeam )) &&
		( this->flags & MF_DROPPED ))
	{
		return ( RETURN_FLAG );
	}

	return ( DENY_PICKUP );
}

//===========================================================================
//
// ABlueFlag :: AnnounceFlagPickup
//
// Play the announcer sound for picking up this flag.
//
//===========================================================================

void ABlueFlag::AnnounceFlagPickup( AActor *pToucher )
{
	// Don't announce the pickup if the flag is being given to someone as part of a snapshot.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) && ( CLIENT_GetConnectionState( ) == CTS_RECEIVINGSNAPSHOT ))
		return;

	ANNOUNCER_PlayEntry( cl_announcer, "BlueFlagTaken" );
}

//===========================================================================
//
// ABlueFlag :: DisplayFlagTaken
//
// Display the text for picking up this flag.
//
//===========================================================================

void ABlueFlag::DisplayFlagTaken( AActor *pToucher )
{
	char				szString[256];
	DHUDMessageFadeOut	*pMsg;

	// Create the "pickup" message.
	if (( pToucher->player - players ) == consoleplayer )
		sprintf( szString, "\\cHYou have the blue flag!" );
	else
		sprintf( szString, "\\cHBlue flag taken!" );
	V_ColorizeString( szString );

	// Now, print it.
	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
	{
		screen->SetFont( BigFont );
		pMsg = new DHUDMessageFadeOut( szString,
			1.5f,
			TEAM_MESSAGE_Y_AXIS,
			0,
			0,
			CR_BLUE,
			3.0f,
			0.25f );
		StatusBar->AttachMessage( pMsg, MAKE_ID('C','N','T','R') );
		screen->SetFont( SmallFont );
	}
	// If necessary, send it to clients.
	else
	{
		sprintf( szString, "\\cHYou have the blue flag!" );
		V_ColorizeString( szString );
		SERVERCOMMANDS_PrintHUDMessageFadeOut( szString, 1.5f, TEAM_MESSAGE_Y_AXIS, 0, 0, CR_BLUE, 3.0f, 0.25f, "BigFont", false, MAKE_ID('C','N','T','R'), ULONG( pToucher->player - players ), SVCF_ONLYTHISCLIENT );

		sprintf( szString, "\\cHBlue flag taken!" );
		V_ColorizeString( szString );
		SERVERCOMMANDS_PrintHUDMessageFadeOut( szString, 1.5f, TEAM_MESSAGE_Y_AXIS, 0, 0, CR_BLUE, 3.0f, 0.25f, "BigFont", false, MAKE_ID('C','N','T','R'), ULONG( pToucher->player - players ), SVCF_SKIPTHISCLIENT );
	}

	// [RC] Create the "held by" message for blue.
		ULONG playerIndex = ULONG( pToucher->player - players );
		sprintf( szString, "\\chHeld by: %s",players[playerIndex].userinfo.netname);

		V_ColorizeString( szString );

		// Now, print it.
		if ( NETWORK_GetState( ) != NETSTATE_SERVER )
		{
			if (( pToucher->player - players ) != consoleplayer )
			{
				screen->SetFont( SmallFont );
				pMsg = new DHUDMessageFadeOut( szString,
					1.5f,
					TEAM_MESSAGE_Y_AXIS_SUB,
					0,
					0,
					CR_BLUE,
					3.0f,
					0.25f );
				StatusBar->AttachMessage( pMsg, MAKE_ID('S','U','B','S') );
			}
		}
		// If necessary, send it to clients.
		else
			SERVERCOMMANDS_PrintHUDMessageFadeOut( szString, 1.5f, TEAM_MESSAGE_Y_AXIS_SUB, 0, 0, CR_BLUE, 3.0f, 0.25f, "SmallFont", false, MAKE_ID('S','U','B','S'), ULONG( pToucher->player - players ), SVCF_SKIPTHISCLIENT  );
}

//===========================================================================
//
// ABlueFlag :: MarkFlagTaken
//
// Signal to the team module whether or not this flag has been taken.
//
//===========================================================================

void ABlueFlag::MarkFlagTaken( bool bTaken )
{
	TEAM_SetBlueFlagTaken( bTaken );
}

//===========================================================================
//
// ABlueFlag :: ResetReturnTicks
//
// Reset the return ticks for the team associated with this flag.
//
//===========================================================================

void ABlueFlag::ResetReturnTicks( void )
{
	TEAM_SetReturnTicks( TEAM_BLUE, 0 );
}

//===========================================================================
//
// ABlueFlag :: ReturnFlag
//
// Spawn a new flag at its original location.
//
//===========================================================================

void ABlueFlag::ReturnFlag( AActor *pReturner )
{
	POS_t	BlueFlagOrigin;
	AActor	*pActor;

	// Respawn the blue flag.
	BlueFlagOrigin = TEAM_GetBlueFlagOrigin( );
	pActor = Spawn( this->GetClass( ), BlueFlagOrigin.x, BlueFlagOrigin.y, BlueFlagOrigin.z, NO_REPLACE );

	// If we're the server, tell clients to spawn the new skull.
	if (( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( pActor ))
		SERVERCOMMANDS_SpawnThing( pActor );

	// Since all inventory spawns with the MF_DROPPED flag, we need to unset it.
	if ( pActor )
		pActor->flags &= ~MF_DROPPED;

	// Mark the blue flag as no longer being taken.
	TEAM_SetBlueFlagTaken( false );

	// If the opposing team's flag has been taken, the player who returned this flag
	// has the chance to earn an "Assist!" medal.
	if ( TEAM_GetRedFlagTaken( ))
	{
		if ( pReturner && pReturner->player )
			TEAM_SetAssistPlayer( TEAM_BLUE, ULONG( pReturner->player - players ));
	}

	if ( pReturner && pReturner->player )
	{
		// [RC] Create the "returned by" message for blue.
		char szString[256];
		DHUDMessageFadeOut	*pMsg;
		ULONG playerIndex = ULONG( pReturner->player - players );
		sprintf( szString, "\\chReturned by: %s", players[playerIndex].userinfo.netname);

		V_ColorizeString( szString );

		// Now, print it.
		if ( NETWORK_GetState( ) != NETSTATE_SERVER )
		{
			screen->SetFont( SmallFont );
			pMsg = new DHUDMessageFadeOut( szString,
				1.5f,
				TEAM_MESSAGE_Y_AXIS_SUB,
				0,
				0,
				CR_BLUE,
				3.0f,
				0.25f );
			StatusBar->AttachMessage( pMsg, MAKE_ID('S','U','B','S') );
		}
		// If necessary, send it to clients.
		else
			SERVERCOMMANDS_PrintHUDMessageFadeOut( szString, 1.5f, TEAM_MESSAGE_Y_AXIS_SUB, 0, 0, CR_BLUE, 3.0f, 0.25f, "SmallFont", false, MAKE_ID('S','U','B','S') );
	}
	else
	{
		// [RC] Create the "returned automatically" message for blue.
		char szString[256];
		DHUDMessageFadeOut	*pMsg;
		sprintf( szString, "\\chReturned automatically.");

		V_ColorizeString( szString );

		// Now, print it.
		if ( NETWORK_GetState( ) != NETSTATE_SERVER )
		{
			screen->SetFont( SmallFont );
			pMsg = new DHUDMessageFadeOut( szString,
				1.5f,
				TEAM_MESSAGE_Y_AXIS_SUB,
				0,
				0,
				CR_BLUE,
				3.0f,
				0.25f );
			StatusBar->AttachMessage( pMsg, MAKE_ID('S','U','B','S') );
		}
		// If necessary, send it to clients.
		else
			SERVERCOMMANDS_PrintHUDMessageFadeOut( szString, 1.5f, TEAM_MESSAGE_Y_AXIS_SUB, 0, 0, CR_BLUE, 3.0f, 0.25f, "SmallFont", false, MAKE_ID('S','U','B','S') );
	}
}

//===========================================================================
//
// ABlueFlag :: AnnounceFlagReturn
//
// Play the announcer sound for this flag being returned.
//
//===========================================================================

void ABlueFlag::AnnounceFlagReturn( void )
{
	ANNOUNCER_PlayEntry( cl_announcer, "BlueFlagReturned" );
}

//===========================================================================
//
// ABlueFlag :: DisplayFlagReturn
//
// Display the text for this flag being returned.
//
//===========================================================================

void ABlueFlag::DisplayFlagReturn( void )
{
	char						szString[256];
	DHUDMessageFadeOut			*pMsg;

	// Create the "returned" message.
	sprintf( szString, "\\cHBlue flag returned" );
	V_ColorizeString( szString );

	// Now, print it.
	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
	{
		screen->SetFont( BigFont );
		pMsg = new DHUDMessageFadeOut( szString,
			1.5f,
			TEAM_MESSAGE_Y_AXIS,
			0,
			0,
			CR_BLUE,
			3.0f,
			0.25f );
		StatusBar->AttachMessage( pMsg, MAKE_ID('C','N','T','R') );
		screen->SetFont( SmallFont );
	}
}

// Red flag -----------------------------------------------------------------

class ARedFlag : public AFlag
{
	DECLARE_ACTOR( ARedFlag, AFlag )
protected:

	virtual bool HandlePickup( AInventory *pItem );
	virtual LONG AllowFlagPickup( AActor *pToucher );
	virtual void AnnounceFlagPickup( AActor *pToucher );
	virtual void DisplayFlagTaken( AActor *pToucher );
	virtual void MarkFlagTaken( bool bTaken );
	virtual void ResetReturnTicks( void );
	virtual void ReturnFlag( AActor *pReturner );
	virtual void AnnounceFlagReturn( void );
	virtual void DisplayFlagReturn( void );
};

FState ARedFlag::States[] =
{
	S_NORMAL (RFLA, 'A',   3, NULL 				, &States[1]),
	S_NORMAL (RFLA, 'B',   3, NULL 				, &States[2]),
	S_NORMAL (RFLA, 'C',   3, NULL 				, &States[3]),
	S_BRIGHT (RFLA, 'D',   3, NULL 				, &States[4]),
	S_BRIGHT (RFLA, 'E',   3, NULL 				, &States[5]),
	S_BRIGHT (RFLA, 'F',   3, NULL 				, &States[0]),
};

IMPLEMENT_ACTOR( ARedFlag, Any, 5131, 178 )
	PROP_RadiusFixed( 20 )
	PROP_HeightFixed( 16 )
	PROP_Flags( MF_SPECIAL|MF_NOTDMATCH )
	PROP_SpawnState( 0 )

	PROP_Inventory_PickupMessage( "$PICKUP_REDFLAG" )
	PROP_Inventory_Icon( "RFLAB0" )
END_DEFAULTS

//===========================================================================
//
// ARedFlag :: HandlePickup
//
// Ask this item in the actor's inventory to potentially react to this object
// attempting to be picked up.
//
//===========================================================================

bool ARedFlag::HandlePickup( AInventory *pItem )
{
	char				szString[256];
	DHUDMessageFadeOut	*pMsg;
	AInventory			*pInventory;

	// If this object being given isn't a flag, then we don't really care.
	if ( pItem->GetClass( )->IsDescendantOf( RUNTIME_CLASS( AFlag )) == false )
		return ( Super::HandlePickup( pItem ));

	// If we're carrying the opposing team's flag, and trying to pick up our flag,
	// then that means we've captured the flag. Award a point.
	if (( this->GetClass( ) == TEAM_GetFlagItem( !Owner->player->ulTeam )) &&
		( pItem->GetClass( ) == TEAM_GetFlagItem( Owner->player->ulTeam )))
	{
		// Don't award a point if we're touching a dropped version of our flag.
		if ( static_cast<AFlag *>( pItem )->AllowFlagPickup( Owner ) == RETURN_FLAG )
			return ( Super::HandlePickup( pItem ));

		if (( TEAM_GetSimpleCTFMode( )) && ( NETWORK_GetState( ) != NETSTATE_CLIENT ) && ( CLIENTDEMO_IsPlaying( ) == false ))
		{
			// Give his team a point.
			TEAM_SetScore( Owner->player->ulTeam, TEAM_GetScore( Owner->player->ulTeam ) + 1, true );
			Owner->player->lPointCount++;

			// Award the scorer with a "Capture!" medal.
			MEDAL_GiveMedal( ULONG( Owner->player - players ), MEDAL_CAPTURE );

			// Tell clients about the medal that been given.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_GivePlayerMedal( ULONG( Owner->player - players ), MEDAL_CAPTURE );

			// [RC] Clear the 'returned automatically' message. A bit hackish, but leaves the flag structure unchanged.
			this->ReturnFlag( NULL );
			if ( NETWORK_GetState( ) != NETSTATE_SERVER )
			{
				screen->SetFont( SmallFont );
				pMsg = new DHUDMessageFadeOut( "",1.5f,TEAM_MESSAGE_Y_AXIS_SUB,0,0,CR_RED,3.0f,0.5f );
				StatusBar->AttachMessage( pMsg, MAKE_ID('S','U','B','S') );
			}
			// If necessary, send it to clients.
			else
				SERVERCOMMANDS_PrintHUDMessageFadeOut( "", 1.5f, TEAM_MESSAGE_Y_AXIS_SUB, 0, 0, CR_RED, 3.0f, 0.5f, "SmallFont", false, MAKE_ID('S','U','B','S') );


			// Create the "captured" message.
			sprintf( szString, "\\cHBlue team scores!" );
			V_ColorizeString( szString );

			// Now, print it.
			if ( NETWORK_GetState( ) != NETSTATE_SERVER )
			{
				screen->SetFont( BigFont );
				pMsg = new DHUDMessageFadeOut( szString,
					1.5f,
					TEAM_MESSAGE_Y_AXIS,
					0,
					0,
					CR_BLUE,
					3.0f,
					0.5f );
				StatusBar->AttachMessage( pMsg, MAKE_ID('C','N','T','R') );
				screen->SetFont( SmallFont );
			}
			// If necessary, send it to clients.
			else
			{
				SERVERCOMMANDS_SetPlayerPoints( ULONG( Owner->player - players ));
				SERVERCOMMANDS_PrintHUDMessageFadeOut( szString, 1.5f, TEAM_MESSAGE_Y_AXIS, 0, 0, CR_BLUE, 3.0f, 0.5f, "BigFont", false, MAKE_ID('C','N','T','R') );
			}
			
			// [RC] Create the "scored by" and "assisted by" message.
			sprintf( szString, "\\cHScored by: %s", Owner->player->userinfo.netname );
			if(TEAM_GetAssistPlayer(Owner->player->ulTeam) != MAXPLAYERS)
			{
				bool selfAssist = false;
				for(ULONG i = 0; i < MAXPLAYERS; i++)
					if(&players[i] == Owner->player)
						if( TEAM_GetAssistPlayer( Owner->player->ulTeam) == i)
							selfAssist = true;

				if ( selfAssist )
					sprintf( szString, "%s\\n\\cH[ Self-Assisted ]", szString);
				else
					sprintf( szString, "%s\\n\\cHAssisted by: %s", szString, players[TEAM_GetAssistPlayer( Owner->player->ulTeam )].userinfo.netname);
			}

			V_ColorizeString( szString );

			// Now, print it.
			if ( NETWORK_GetState( ) != NETSTATE_SERVER )
			{
				screen->SetFont( SmallFont );
				pMsg = new DHUDMessageFadeOut( szString,
					1.5f,
					TEAM_MESSAGE_Y_AXIS_SUB,
					0,
					0,
					CR_BLUE,
					3.0f,
					0.5f );
				StatusBar->AttachMessage( pMsg, MAKE_ID('S','U','B','S') );
			}
			// If necessary, send it to clients.
			else
				SERVERCOMMANDS_PrintHUDMessageFadeOut( szString, 1.5f, TEAM_MESSAGE_Y_AXIS_SUB, 0, 0, CR_BLUE, 3.0f, 0.5f, "SmallFont", false, MAKE_ID('S','U','B','S') );
			

			// If someone just recently returned the flag, award him with an "Assist!" medal.
			if ( TEAM_GetAssistPlayer( Owner->player->ulTeam ) != MAXPLAYERS )
			{
				MEDAL_GiveMedal( TEAM_GetAssistPlayer( Owner->player->ulTeam ), MEDAL_ASSIST );

				// Tell clients about the medal that been given.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_GivePlayerMedal( TEAM_GetAssistPlayer( Owner->player->ulTeam ), MEDAL_ASSIST );

				TEAM_SetAssistPlayer( Owner->player->ulTeam, MAXPLAYERS );
			}

			// Take the flag away.
			pInventory = Owner->FindInventory( this->GetClass( ));
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_TakeInventory( ULONG( Owner->player - players ), TEAM_GetFlagItem( !Owner->player->ulTeam )->TypeName.GetChars( ), 0 );
			if ( pInventory )
				Owner->RemoveInventory( pInventory );

			// Also, refresh the HUD.
			SCOREBOARD_RefreshHUD( );
		}

		return ( true );
	}

	return ( Super::HandlePickup( pItem ));
}

//===========================================================================
//
// ARedFlag :: AllowFlagPickup
//
// Determine whether or not we should be allowed to pickup this flag.
//
//===========================================================================

LONG ARedFlag::AllowFlagPickup( AActor *pToucher )
{
	// Don't allow the pickup of red/blue flags in One Flag CTF.
	if ( oneflagctf )
		return ( DENY_PICKUP );

	if (( pToucher == NULL ) || ( pToucher->player == NULL ) || ( pToucher->player->bOnTeam == false ))
		return ( DENY_PICKUP );

	// Player is touching the enemy flag.
	if ( this->GetClass( ) == TEAM_GetFlagItem( !pToucher->player->ulTeam ))
		return ( ALLOW_PICKUP );

	// Player is touching his own flag. If it's dropped, allow him to return it.
	if (( this->GetClass( ) == TEAM_GetFlagItem( pToucher->player->ulTeam )) &&
		( this->flags & MF_DROPPED ))
	{
		return ( RETURN_FLAG );
	}

	return ( DENY_PICKUP );
}

//===========================================================================
//
// ARedFlag :: AnnounceFlagPickup
//
// Play the announcer sound for picking up this flag.
//
//===========================================================================

void ARedFlag::AnnounceFlagPickup( AActor *pToucher )
{
	// Don't announce the pickup if the flag is being given to someone as part of a snapshot.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) && ( CLIENT_GetConnectionState( ) == CTS_RECEIVINGSNAPSHOT ))
		return;

	ANNOUNCER_PlayEntry( cl_announcer, "RedFlagTaken" );
}

//===========================================================================
//
// ARedFlag :: DisplayFlagTaken
//
// Display the text for picking up this flag.
//
//===========================================================================

void ARedFlag::DisplayFlagTaken( AActor *pToucher )
{
	char				szString[256];
	DHUDMessageFadeOut	*pMsg;

	// Create the "pickup" message.
	if (( pToucher->player - players ) == consoleplayer )
		sprintf( szString, "\\cGYou have the red flag!" );
	else
		sprintf( szString, "\\cGRed flag taken!" );
	V_ColorizeString( szString );

	// Now, print it.
	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
	{
		screen->SetFont( BigFont );
		pMsg = new DHUDMessageFadeOut( szString,
			1.5f,
			TEAM_MESSAGE_Y_AXIS,
			0,
			0,
			CR_RED,
			3.0f,
			0.25f );
		StatusBar->AttachMessage( pMsg, MAKE_ID('C','N','T','R') );
		screen->SetFont( SmallFont );
	}
	// If necessary, send it to clients.
	else
	{
		sprintf( szString, "\\cGYou have the red flag!" );
		V_ColorizeString( szString );
		SERVERCOMMANDS_PrintHUDMessageFadeOut( szString, 1.5f, TEAM_MESSAGE_Y_AXIS, 0, 0, CR_RED, 3.0f, 0.25f, "BigFont", false, MAKE_ID('C','N','T','R'), ULONG( pToucher->player - players ), SVCF_ONLYTHISCLIENT );

		sprintf( szString, "\\cGRed flag taken!" );
		V_ColorizeString( szString );
		SERVERCOMMANDS_PrintHUDMessageFadeOut( szString, 1.5f, TEAM_MESSAGE_Y_AXIS, 0, 0, CR_RED, 3.0f, 0.25f, "BigFont", false, MAKE_ID('C','N','T','R'), ULONG( pToucher->player - players ), SVCF_SKIPTHISCLIENT );
	}

	// [RC] Create the "held by" message for red.
		ULONG playerIndex = ULONG( pToucher->player - players );
		sprintf( szString, "\\cgHeld by: %s",players[playerIndex].userinfo.netname);

		V_ColorizeString( szString );

		// Now, print it.
		if ( NETWORK_GetState( ) != NETSTATE_SERVER )
		{
			if (( pToucher->player - players ) != consoleplayer )
			{
				screen->SetFont( SmallFont );
				pMsg = new DHUDMessageFadeOut( szString,
					1.5f,
					TEAM_MESSAGE_Y_AXIS_SUB,
					0,
					0,
					CR_RED,
					3.0f,
					0.25f );
				StatusBar->AttachMessage( pMsg, MAKE_ID('S','U','B','S') );
			}
		}
		// If necessary, send it to clients.
		else
			SERVERCOMMANDS_PrintHUDMessageFadeOut( szString, 1.5f, TEAM_MESSAGE_Y_AXIS_SUB, 0, 0, CR_RED, 3.0f, 0.25f, "SmallFont", false, MAKE_ID('S','U','B','S'), ULONG( pToucher->player - players ), SVCF_SKIPTHISCLIENT );
}

//===========================================================================
//
// ARedFlag :: MarkFlagTaken
//
// Signal to the team module whether or not this flag has been taken.
//
//===========================================================================

void ARedFlag::MarkFlagTaken( bool bTaken )
{
	TEAM_SetRedFlagTaken( bTaken );
}

//===========================================================================
//
// ARedFlag :: ResetReturnTicks
//
// Reset the return ticks for the team associated with this flag.
//
//===========================================================================

void ARedFlag::ResetReturnTicks( void )
{
	TEAM_SetReturnTicks( TEAM_RED, 0 );
}

//===========================================================================
//
// ARedFlag :: ReturnFlag
//
// Spawn a new flag at its original location.
//
//===========================================================================

void ARedFlag::ReturnFlag( AActor *pReturner )
{
	POS_t	RedFlagOrigin;
	AActor	*pActor;

	// Respawn the blue flag.
	RedFlagOrigin = TEAM_GetRedFlagOrigin( );
	pActor = Spawn( this->GetClass( ), RedFlagOrigin.x, RedFlagOrigin.y, RedFlagOrigin.z, NO_REPLACE );

	// If we're the server, tell clients to spawn the new skull.
	if (( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( pActor ))
		SERVERCOMMANDS_SpawnThing( pActor );

	// Since all inventory spawns with the MF_DROPPED flag, we need to unset it.
	if ( pActor )
		pActor->flags &= ~MF_DROPPED;

	// Mark the red flag as no longer being taken.
	TEAM_SetRedFlagTaken( false );

	// If the opposing team's flag has been taken, the player who returned this flag
	// has the chance to earn an "Assist!" medal.
	if ( TEAM_GetBlueFlagTaken( ))
	{
		if ( pReturner && pReturner->player )
			TEAM_SetAssistPlayer( TEAM_RED, ULONG( pReturner->player - players ));
	}

	if ( pReturner && pReturner->player )
	{
		// [RC] Create the "returned by" message for red.
		char szString[256];
		DHUDMessageFadeOut	*pMsg;
		ULONG playerIndex = ULONG( pReturner->player - players );
		sprintf( szString, "\\cgReturned by: %s", players[playerIndex].userinfo.netname);

		V_ColorizeString( szString );

		// Now, print it.
		if ( NETWORK_GetState( ) != NETSTATE_SERVER )
		{
			screen->SetFont( SmallFont );
			pMsg = new DHUDMessageFadeOut( szString,
				1.5f,
				TEAM_MESSAGE_Y_AXIS_SUB,
				0,
				0,
				CR_RED,
				3.0f,
				0.25f );
			StatusBar->AttachMessage( pMsg, MAKE_ID('S','U','B','S') );
		}
		// If necessary, send it to clients.
		else
			SERVERCOMMANDS_PrintHUDMessageFadeOut( szString, 1.5f, TEAM_MESSAGE_Y_AXIS_SUB, 0, 0, CR_RED, 3.0f, 0.25f, "SmallFont", false, MAKE_ID('S','U','B','S') );
	}
	else
	{
		// [RC] Create the "returned automatically" message for red.
		char szString[256];
		DHUDMessageFadeOut	*pMsg;
		sprintf( szString, "\\cgReturned automatically.");

		V_ColorizeString( szString );

		// Now, print it.
		if ( NETWORK_GetState( ) != NETSTATE_SERVER )
		{
			screen->SetFont( SmallFont );
			pMsg = new DHUDMessageFadeOut( szString,
				1.5f,
				TEAM_MESSAGE_Y_AXIS_SUB,
				0,
				0,
				CR_RED,
				3.0f,
				0.25f );
			StatusBar->AttachMessage( pMsg, MAKE_ID('S','U','B','S') );
		}
		// If necessary, send it to clients.
		else
			SERVERCOMMANDS_PrintHUDMessageFadeOut( szString, 1.5f, TEAM_MESSAGE_Y_AXIS_SUB, 0, 0, CR_RED, 3.0f, 0.25f, "SmallFont", false, MAKE_ID('S','U','B','S') );
	}

}

//===========================================================================
//
// ARedFlag :: AnnounceFlagReturn
//
// Play the announcer sound for this flag being returned.
//
//===========================================================================

void ARedFlag::AnnounceFlagReturn( void )
{
	ANNOUNCER_PlayEntry( cl_announcer, "RedFlagReturned" );
}

//===========================================================================
//
// ARedFlag :: DisplayFlagReturn
//
// Display the text for this flag being returned.
//
//===========================================================================

void ARedFlag::DisplayFlagReturn( void )
{
	char						szString[256];
	DHUDMessageFadeOut			*pMsg;

	// Create the "returned" message.
	sprintf( szString, "\\cGRed flag returned" );
	V_ColorizeString( szString );

	// Now, print it.
	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
	{
		screen->SetFont( BigFont );
		pMsg = new DHUDMessageFadeOut( szString,
			1.5f,
			TEAM_MESSAGE_Y_AXIS,
			0,
			0,
			CR_RED,
			3.0f,
			0.25f );
		StatusBar->AttachMessage( pMsg, MAKE_ID('C','N','T','R') );
		screen->SetFont( SmallFont );
	}
}

// Blue skulltag skull ------------------------------------------------------

class ABlueSkullST : public AFlag
{
	DECLARE_ACTOR( ABlueSkullST, AFlag )
protected:

	virtual LONG AllowFlagPickup( AActor *pToucher );
	virtual void AnnounceFlagPickup( AActor *pToucher );
	virtual void DisplayFlagTaken( AActor *pToucher );
	virtual void MarkFlagTaken( bool bTaken );
	virtual void ResetReturnTicks( void );
	virtual void ReturnFlag( AActor *pReturner );
	virtual void AnnounceFlagReturn( void );
	virtual void DisplayFlagReturn( void );
};

FState ABlueSkullST::States[] =
{
	S_NORMAL( BSKU, 'A',   10, NULL 				, &States[1] ),
	S_BRIGHT( BSKU, 'B',   10, NULL 				, &States[0] ),
};

IMPLEMENT_ACTOR( ABlueSkullST, Any, -1, 0 )
	PROP_RadiusFixed( 20 )
	PROP_HeightFixed( 16 )
	PROP_Flags( MF_SPECIAL|MF_NOTDMATCH )
	PROP_SpawnState( 0 )

	PROP_Inventory_PickupMessage( "$PICKUP_BLUESKULL_ST" )
	PROP_Inventory_Icon( "BSKUB0" )
END_DEFAULTS

//===========================================================================
//
// ABlueSkullST :: AllowFlagPickup
//
// Determine whether or not we should be allowed to pickup this flag.
//
//===========================================================================

LONG ABlueSkullST::AllowFlagPickup( AActor *pToucher )
{
	if (( pToucher == NULL ) || ( pToucher->player == NULL ) || ( pToucher->player->bOnTeam == false ))
		return ( DENY_PICKUP );

	// Player is touching the enemy flag.
	if ( this->GetClass( ) == TEAM_GetFlagItem( !pToucher->player->ulTeam ))
		return ( ALLOW_PICKUP );

	// Player is touching his own flag. If it's dropped, allow him to return it.
	if (( this->GetClass( ) == TEAM_GetFlagItem( pToucher->player->ulTeam )) &&
		( this->flags & MF_DROPPED ))
	{
		return ( RETURN_FLAG );
	}

	return ( DENY_PICKUP );
}

//===========================================================================
//
// ABlueSkullST :: AnnounceFlagPickup
//
// Play the announcer sound for picking up this flag.
//
//===========================================================================

void ABlueSkullST::AnnounceFlagPickup( AActor *pToucher )
{
	// Don't announce the pickup if the flag is being given to someone as part of a snapshot.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) && ( CLIENT_GetConnectionState( ) == CTS_RECEIVINGSNAPSHOT ))
		return;

	ANNOUNCER_PlayEntry( cl_announcer, "BlueSkullTaken" );
}

//===========================================================================
//
// ABlueSkullST :: DisplayFlagTaken
//
// Display the text for picking up this flag.
//
//===========================================================================

void ABlueSkullST::DisplayFlagTaken( AActor *pToucher )
{
	char				szString[256];
	DHUDMessageFadeOut	*pMsg;

	// Create the "pickup" message.
	if (( pToucher->player - players ) == consoleplayer )
		sprintf( szString, "\\cHYou have the blue skull!" );
	else
		sprintf( szString, "\\cHBlue skull taken!" );
	V_ColorizeString( szString );

	// Now, print it.
	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
	{
		screen->SetFont( BigFont );
		pMsg = new DHUDMessageFadeOut( szString,
			1.5f,
			TEAM_MESSAGE_Y_AXIS,
			0,
			0,
			CR_BLUE,
			3.0f,
			0.25f );
		StatusBar->AttachMessage( pMsg, MAKE_ID('C','N','T','R') );
		screen->SetFont( SmallFont );
	}
	// If necessary, send it to clients.
	else
	{
		sprintf( szString, "\\cHYou have the blue skull!" );
		V_ColorizeString( szString );
		SERVERCOMMANDS_PrintHUDMessageFadeOut( szString, 1.5f, TEAM_MESSAGE_Y_AXIS, 0, 0, CR_BLUE, 3.0f, 0.25f, "BigFont", false, MAKE_ID('C','N','T','R'), ULONG( pToucher->player - players ), SVCF_ONLYTHISCLIENT );

		sprintf( szString, "\\cHBlue skull taken!" );
		V_ColorizeString( szString );
		SERVERCOMMANDS_PrintHUDMessageFadeOut( szString, 1.5f, TEAM_MESSAGE_Y_AXIS, 0, 0, CR_BLUE, 3.0f, 0.25f, "BigFont", false, MAKE_ID('C','N','T','R'), ULONG( pToucher->player - players ), SVCF_SKIPTHISCLIENT );
	}

	// [RC] Create the "held by" message for blue.
		ULONG playerIndex = ULONG( pToucher->player - players );
		sprintf( szString, "\\chHeld by: %s",players[playerIndex].userinfo.netname);

		V_ColorizeString( szString );

		// Now, print it.
		if ( NETWORK_GetState( ) != NETSTATE_SERVER )
		{
			if (( pToucher->player - players ) != consoleplayer )
			{
				screen->SetFont( SmallFont );
				pMsg = new DHUDMessageFadeOut( szString,
					1.5f,
					TEAM_MESSAGE_Y_AXIS_SUB,
					0,
					0,
					CR_BLUE,
					3.0f,
					0.25f );
				StatusBar->AttachMessage( pMsg, MAKE_ID('S','U','B','S') );
			}
		}
		// If necessary, send it to clients.
		else
			SERVERCOMMANDS_PrintHUDMessageFadeOut( szString, 1.5f, TEAM_MESSAGE_Y_AXIS_SUB, 0, 0, CR_BLUE, 3.0f, 0.25f, "SmallFont", false, MAKE_ID('S','U','B','S'), ULONG( pToucher->player - players ), SVCF_SKIPTHISCLIENT  );
}

//===========================================================================
//
// ABlueSkullST :: MarkFlagTaken
//
// Signal to the team module whether or not this flag has been taken.
//
//===========================================================================

void ABlueSkullST::MarkFlagTaken( bool bTaken )
{
	TEAM_SetBlueSkullTaken( bTaken );
}

//===========================================================================
//
// ABlueSkullST :: ResetReturnTicks
//
// Reset the return ticks for the team associated with this flag.
//
//===========================================================================

void ABlueSkullST::ResetReturnTicks( void )
{
	TEAM_SetReturnTicks( TEAM_BLUE, 0 );
}

//===========================================================================
//
// ABlueSkullST :: ReturnFlag
//
// Spawn a new flag at its original location.
//
//===========================================================================

void ABlueSkullST::ReturnFlag( AActor *pReturner )
{
	POS_t	BlueSkullOrigin;
	AActor	*pActor;

	// Respawn the blue skull.
	BlueSkullOrigin = TEAM_GetBlueSkullOrigin( );
	pActor = Spawn( this->GetClass( ), BlueSkullOrigin.x, BlueSkullOrigin.y, BlueSkullOrigin.z, NO_REPLACE );

	// If we're the server, tell clients to spawn the new skull.
	if (( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( pActor ))
		SERVERCOMMANDS_SpawnThing( pActor );

	// Since all inventory spawns with the MF_DROPPED flag, we need to unset it.
	if ( pActor )
		pActor->flags &= ~MF_DROPPED;

	// Mark the blue skull as no longer being taken.
	TEAM_SetBlueSkullTaken( false );

	// If the opposing team's flag has been taken, the player who returned this flag
	// has the chance to earn an "Assist!" medal.
	if ( TEAM_GetRedSkullTaken( ))
	{
		if ( pReturner && pReturner->player )
			TEAM_SetAssistPlayer( TEAM_BLUE, ULONG( pReturner->player - players ));
	}
	if ( pReturner && pReturner->player )
	{
		// [RC] Create the "returned by" message for blue.
		char szString[256];
		DHUDMessageFadeOut	*pMsg;
		ULONG playerIndex = ULONG( pReturner->player - players );
		sprintf( szString, "\\chReturned by: %s", players[playerIndex].userinfo.netname);

		V_ColorizeString( szString );

		// Now, print it.
		if ( NETWORK_GetState( ) != NETSTATE_SERVER )
		{
			screen->SetFont( SmallFont );
			pMsg = new DHUDMessageFadeOut( szString,
				1.5f,
				TEAM_MESSAGE_Y_AXIS_SUB,
				0,
				0,
				CR_BLUE,
				3.0f,
				0.25f );
			StatusBar->AttachMessage( pMsg, MAKE_ID('S','U','B','S') );
		}
		// If necessary, send it to clients.
		else
			SERVERCOMMANDS_PrintHUDMessageFadeOut( szString, 1.5f, TEAM_MESSAGE_Y_AXIS_SUB, 0, 0, CR_BLUE, 3.0f, 0.25f, "SmallFont", false, MAKE_ID('S','U','B','S') );
	}
	else
	{
		// [RC] Create the "returned automatically" message for red.
		char szString[256];
		DHUDMessageFadeOut	*pMsg;
		sprintf( szString, "\\chReturned automatically.");

		V_ColorizeString( szString );

		// Now, print it.
		if ( NETWORK_GetState( ) != NETSTATE_SERVER )
		{
			screen->SetFont( SmallFont );
			pMsg = new DHUDMessageFadeOut( szString,
				1.5f,
				TEAM_MESSAGE_Y_AXIS_SUB,
				0,
				0,
				CR_RED,
				3.0f,
				0.25f );
			StatusBar->AttachMessage( pMsg, MAKE_ID('S','U','B','S') );
		}
		// If necessary, send it to clients.
		else
			SERVERCOMMANDS_PrintHUDMessageFadeOut( szString, 1.5f, TEAM_MESSAGE_Y_AXIS_SUB, 0, 0, CR_RED, 3.0f, 0.25f, "SmallFont", false, MAKE_ID('S','U','B','S') );
	}

}

//===========================================================================
//
// ABlueSkullST :: AnnounceFlagReturn
//
// Play the announcer sound for this flag being returned.
//
//===========================================================================

void ABlueSkullST::AnnounceFlagReturn( void )
{
	ANNOUNCER_PlayEntry( cl_announcer, "BlueSkullReturned" );
}

//===========================================================================
//
// ABlueSkullST :: DisplayFlagReturn
//
// Display the text for this flag being returned.
//
//===========================================================================

void ABlueSkullST::DisplayFlagReturn( void )
{
	char						szString[256];
	DHUDMessageFadeOut			*pMsg;

	// Create the "returned" message.
	sprintf( szString, "\\cHBlue skull returned" );
	V_ColorizeString( szString );

	// Now, print it.
	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
	{
		screen->SetFont( BigFont );
		pMsg = new DHUDMessageFadeOut( szString,
			1.5f,
			TEAM_MESSAGE_Y_AXIS,
			0,
			0,
			CR_BLUE,
			3.0f,
			0.25f );
		StatusBar->AttachMessage( pMsg, MAKE_ID('C','N','T','R') );
		screen->SetFont( SmallFont );
	}
}

// Red skulltag skull -------------------------------------------------------

class ARedSkullST : public AFlag
{
	DECLARE_ACTOR( ARedSkullST, AFlag )
protected:

	virtual LONG AllowFlagPickup( AActor *pToucher );
	virtual void AnnounceFlagPickup( AActor *pToucher );
	virtual void DisplayFlagTaken( AActor *pToucher );
	virtual void MarkFlagTaken( bool bTaken );
	virtual void ResetReturnTicks( void );
	virtual void ReturnFlag( AActor *pReturner );
	virtual void AnnounceFlagReturn( void );
	virtual void DisplayFlagReturn( void );
};

FState ARedSkullST::States[] =
{
	S_NORMAL( RSKU, 'A',   10, NULL 				, &States[1] ),
	S_BRIGHT( RSKU, 'B',   10, NULL 				, &States[0] ),
};

IMPLEMENT_ACTOR( ARedSkullST, Any, -1, 0 )
	PROP_RadiusFixed( 20 )
	PROP_HeightFixed( 16 )
	PROP_Flags( MF_SPECIAL|MF_NOTDMATCH )
	PROP_SpawnState( 0 )

	PROP_Inventory_PickupMessage( "$PICKUP_REDSKULL_ST" )
	PROP_Inventory_Icon( "RSKUB0" )
END_DEFAULTS

//===========================================================================
//
// ARedSkullST :: AllowFlagPickup
//
// Determine whether or not we should be allowed to pickup this flag.
//
//===========================================================================

LONG ARedSkullST::AllowFlagPickup( AActor *pToucher )
{
	if (( pToucher == NULL ) || ( pToucher->player == NULL ) || ( pToucher->player->bOnTeam == false ))
		return ( DENY_PICKUP );

	// Player is touching the enemy flag.
	if ( this->GetClass( ) == TEAM_GetFlagItem( !pToucher->player->ulTeam ))
		return ( ALLOW_PICKUP );

	// Player is touching his own flag. If it's dropped, allow him to return it.
	if (( this->GetClass( ) == TEAM_GetFlagItem( pToucher->player->ulTeam )) &&
		( this->flags & MF_DROPPED ))
	{
		return ( RETURN_FLAG );
	}

	return ( DENY_PICKUP );
}

//===========================================================================
//
// ARedSkullST :: AnnounceFlagPickup
//
// Play the announcer sound for picking up this flag.
//
//===========================================================================

void ARedSkullST::AnnounceFlagPickup( AActor *pToucher )
{
	// Don't announce the pickup if the flag is being given to someone as part of a snapshot.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) && ( CLIENT_GetConnectionState( ) == CTS_RECEIVINGSNAPSHOT ))
		return;

	ANNOUNCER_PlayEntry( cl_announcer, "RedSkullTaken" );
}

//===========================================================================
//
// ARedSkullST :: DisplayFlagTaken
//
// Display the text for picking up this flag.
//
//===========================================================================

void ARedSkullST::DisplayFlagTaken( AActor *pToucher )
{
	char				szString[256];
	DHUDMessageFadeOut	*pMsg;

	// Create the "pickup" message.
	if (( pToucher->player - players ) == consoleplayer )
		sprintf( szString, "\\cGYou have the red skull!" );
	else
		sprintf( szString, "\\cGRed skull taken!" );
	V_ColorizeString( szString );

	// Now, print it.
	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
	{
		screen->SetFont( BigFont );
		pMsg = new DHUDMessageFadeOut( szString,
			1.5f,
			TEAM_MESSAGE_Y_AXIS,
			0,
			0,
			CR_RED,
			3.0f,
			0.25f );
		StatusBar->AttachMessage( pMsg, MAKE_ID('C','N','T','R') );
		screen->SetFont( SmallFont );
	}
	// If necessary, send it to clients.
	else
	{
		sprintf( szString, "\\cGYou have the red skull!" );
		V_ColorizeString( szString );
		SERVERCOMMANDS_PrintHUDMessageFadeOut( szString, 1.5f, TEAM_MESSAGE_Y_AXIS, 0, 0, CR_RED, 3.0f, 0.25f, "BigFont", false, MAKE_ID('C','N','T','R'), ULONG( pToucher->player - players ), SVCF_ONLYTHISCLIENT );

		sprintf( szString, "\\cGRed skull taken!" );
		V_ColorizeString( szString );
		SERVERCOMMANDS_PrintHUDMessageFadeOut( szString, 1.5f, TEAM_MESSAGE_Y_AXIS, 0, 0, CR_RED, 3.0f, 0.25f, "BigFont", false, MAKE_ID('C','N','T','R'), ULONG( pToucher->player - players ), SVCF_SKIPTHISCLIENT );
	}

	// [RC] Create the "held by" message for red.
		ULONG playerIndex = ULONG( pToucher->player - players );
		sprintf( szString, "\\cgHeld by: %s",players[playerIndex].userinfo.netname);

		V_ColorizeString( szString );

		// Now, print it.
		if ( NETWORK_GetState( ) != NETSTATE_SERVER )
		{
			if (( pToucher->player - players ) != consoleplayer )
			{
				screen->SetFont( SmallFont );
				pMsg = new DHUDMessageFadeOut( szString,
					1.5f,
					TEAM_MESSAGE_Y_AXIS_SUB,
					0,
					0,
					CR_RED,
					3.0f,
					0.25f );
				StatusBar->AttachMessage( pMsg, MAKE_ID('S','U','B','S') );
			}
		}
		// If necessary, send it to clients.
		else
			SERVERCOMMANDS_PrintHUDMessageFadeOut( szString, 1.5f, TEAM_MESSAGE_Y_AXIS_SUB, 0, 0, CR_RED, 3.0f, 0.25f, "SmallFont", false, MAKE_ID('S','U','B','S'), ULONG( pToucher->player - players ), SVCF_SKIPTHISCLIENT );
}

//===========================================================================
//
// ARedSkullST :: MarkFlagTaken
//
// Signal to the team module whether or not this flag has been taken.
//
//===========================================================================

void ARedSkullST::MarkFlagTaken( bool bTaken )
{
	TEAM_SetRedSkullTaken( bTaken );
}

//===========================================================================
//
// ARedSkullST :: ResetReturnTicks
//
// Reset the return ticks for the team associated with this flag.
//
//===========================================================================

void ARedSkullST::ResetReturnTicks( void )
{
	TEAM_SetReturnTicks( TEAM_RED, 0 );
}

//===========================================================================
//
// ARedSkullST :: ReturnFlag
//
// Spawn a new flag at its original location.
//
//===========================================================================

void ARedSkullST::ReturnFlag( AActor *pReturner )
{
	POS_t	RedSkullOrigin;
	AActor	*pActor;

	// Respawn the red skull.
	RedSkullOrigin = TEAM_GetRedSkullOrigin( );
	pActor = Spawn( this->GetClass( ), RedSkullOrigin.x, RedSkullOrigin.y, RedSkullOrigin.z, NO_REPLACE );

	// If we're the server, tell clients to spawn the new skull.
	if (( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( pActor ))
		SERVERCOMMANDS_SpawnThing( pActor );

	// Since all inventory spawns with the MF_DROPPED flag, we need to unset it.
	if ( pActor )
		pActor->flags &= ~MF_DROPPED;

	// Mark the red skull as no longer being taken.
	TEAM_SetRedSkullTaken( false );

	// If the opposing team's flag has been taken, the player who returned this flag
	// has the chance to earn an "Assist!" medal.
	if ( TEAM_GetBlueSkullTaken( ))
	{
		if ( pReturner && pReturner->player )
			TEAM_SetAssistPlayer( TEAM_RED, ULONG( pReturner->player - players ));
	}
	if ( pReturner && pReturner->player )
	{
		// [RC] Create the "returned by" message for red.
		char szString[256];
		DHUDMessageFadeOut	*pMsg;
		ULONG playerIndex = ULONG( pReturner->player - players );
		sprintf( szString, "\\cgReturned by: %s",players[playerIndex].userinfo.netname);

		V_ColorizeString( szString );

		// Now, print it.
		if ( NETWORK_GetState( ) != NETSTATE_SERVER )
		{
			screen->SetFont( SmallFont );
			pMsg = new DHUDMessageFadeOut( szString,
				1.5f,
				TEAM_MESSAGE_Y_AXIS_SUB,
				0,
				0,
				CR_RED,
				3.0f,
				0.25f );
			StatusBar->AttachMessage( pMsg, MAKE_ID('S','U','B','S') );
		}
		// If necessary, send it to clients.
		else
			SERVERCOMMANDS_PrintHUDMessageFadeOut( szString, 1.5f, TEAM_MESSAGE_Y_AXIS_SUB, 0, 0, CR_RED, 3.0f, 0.25f, "SmallFont", false, MAKE_ID('S','U','B','S') );
	}
	else
	{
		// [RC] Create the "returned automatically" message for red.
		char szString[256];
		DHUDMessageFadeOut	*pMsg;
		sprintf( szString, "\\cgReturned automatically.");

		V_ColorizeString( szString );

		// Now, print it.
		if ( NETWORK_GetState( ) != NETSTATE_SERVER )
		{
			screen->SetFont( SmallFont );
			pMsg = new DHUDMessageFadeOut( szString,
				1.5f,
				TEAM_MESSAGE_Y_AXIS_SUB,
				0,
				0,
				CR_RED,
				3.0f,
				0.25f );
			StatusBar->AttachMessage( pMsg, MAKE_ID('S','U','B','S') );
		}
		// If necessary, send it to clients.
		else
			SERVERCOMMANDS_PrintHUDMessageFadeOut( szString, 1.5f, TEAM_MESSAGE_Y_AXIS_SUB, 0, 0, CR_RED, 3.0f, 0.25f, "SmallFont", false, MAKE_ID('S','U','B','S') );
	}
}

//===========================================================================
//
// ARedSkullST :: AnnounceFlagReturn
//
// Play the announcer sound for this flag being returned.
//
//===========================================================================

void ARedSkullST::AnnounceFlagReturn( void )
{
	ANNOUNCER_PlayEntry( cl_announcer, "RedSkullReturned" );
}

//===========================================================================
//
// ABlueSkullST :: DisplayFlagReturn
//
// Display the text for this flag being returned.
//
//===========================================================================

void ARedSkullST::DisplayFlagReturn( void )
{
	char						szString[256];
	DHUDMessageFadeOut			*pMsg;

	// Create the "returned" message.
	sprintf( szString, "\\cGRed skull returned" );
	V_ColorizeString( szString );

	// Now, print it.
	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
	{
		screen->SetFont( BigFont );
		pMsg = new DHUDMessageFadeOut( szString,
			1.5f,
			TEAM_MESSAGE_Y_AXIS,
			0,
			0,
			CR_RED,
			3.0f,
			0.25f );
		StatusBar->AttachMessage( pMsg, MAKE_ID('C','N','T','R') );
		screen->SetFont( SmallFont );
	}
}
