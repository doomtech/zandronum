//-----------------------------------------------------------------------------
//
// Skulltag Source
// Copyright (C) 2003-2005 Brad Carney
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
// Date created:  9/20/05
//
//
// Filename: cooperative.cpp
//
// Description: 
//
//-----------------------------------------------------------------------------

#include "cooperative.h"
#include "deathmatch.h"
#include "doomstat.h"
#include "gamemode.h"
#include "team.h"

//*****************************************************************************
//	CONSOLE VARIABLES

CVAR( Bool, sv_coopspawnvoodoodolls, false, CVAR_SERVERINFO | CVAR_LATCH );

//*****************************************************************************
//	PROTOTYPES

void COOP_DestroyVoodooDollsOfPlayer ( const ULONG ulPlayer )
{
	// [BB] The current game mode doesn't need voodoo dolls.
	if ( COOP_VoodooDollsSelectedByGameMode() == false )
		return;

	// [BB] The map doesn't have any voodoo doll starts for this player.
	if ( AllPlayerStarts[ulPlayer].Size() <= 1 )
		return;

	TThinkerIterator<AActor>	Iterator;
	AActor						*pActor;
	while (( pActor = Iterator.Next( )))
	{
		// [BB] This actor doesn't belong to a player, so it can't be a voodoo doll.
		if ( pActor->player == NULL )
			continue;

		// [BB] Belongs to a different player.
		if ( static_cast<LONG>(pActor->player - players) != ulPlayer )
			continue;

		// [BB] If we come here, we found a body belonging to the player.

		if (
			// [BB] The current player doesn't have a body assigned, so we found a voodoo doll.
			( players[ulPlayer].mo == NULL )
			// [BB] A different body is assigned to the player, so we found a voodoo doll.
			|| ( players[ulPlayer].mo != pActor )
			)
		{
			// [BB] Now that we found a voodoo doll, finally destroy it.
			pActor->Destroy();
		}
	}
}

//*****************************************************************************
//
bool COOP_PlayersVoodooDollsNeedToBeSpawned ( const ULONG ulPlayer )
{
	// [BB] The current game mode doesn't need voodoo dolls.
	if ( COOP_VoodooDollsSelectedByGameMode() == false )
		return false;

	// [BB] The map doesn't have any voodoo doll starts for this player.
	if ( AllPlayerStarts[ulPlayer].Size() <= 1 )
		return false;

	TThinkerIterator<AActor>	Iterator;
	AActor						*pActor;
	while (( pActor = Iterator.Next( )))
	{
		// [BB] This actor doesn't belong to a player, so it can't be a voodoo doll.
		if ( pActor->player == NULL )
			continue;

		// [BB] Belongs to a different player.
		if ( static_cast<LONG>(pActor->player - players) !=  ulPlayer )
			continue;

		// [BB] If we come here, we found a body belonging to the player.

		if (
			// [BB] The current player doesn't have a body assigned, so we found a voodoo doll.
			( players[ulPlayer].mo == NULL )
			// [BB] A different body is assigned to the player, so we found a voodoo doll.
			|| ( players[ulPlayer].mo != pActor )
			)
		{
			// [BB] There already is a voodoo doll for the player, so we don't need to spawn it.
			return false;
		}

	}

	return true;
}

//*****************************************************************************
//
void COOP_SpawnVoodooDollsForPlayerIfNecessary ( const ULONG ulPlayer )
{
	// [BB] Only the server spawns voodoo dolls.
	if ( NETWORK_GetState() != NETSTATE_SERVER )
		return;

	// [BB] The current game mode doesn't need voodoo dolls.
	if ( COOP_VoodooDollsSelectedByGameMode() == false )
		return;

	// [BB] The map doesn't have any voodoo doll starts for this player.
	if ( AllPlayerStarts[ulPlayer].Size() <= 1 )
		return;

	APlayerPawn *pDoll = P_SpawnPlayer ( &(AllPlayerStarts[ulPlayer][AllPlayerStarts[ulPlayer].Size()-2]), false, NULL );
	// [BB] Mark the voodoo doll as spawned by the map.
	// P_SpawnPlayer won't spawn anything for a player not in game, therefore we need to check if pDoll is NULL.
	if ( pDoll )
		pDoll->ulSTFlags |= STFL_LEVELSPAWNED;
}

//*****************************************************************************
//
bool COOP_VoodooDollsSelectedByGameMode ( void )
{
	// [BB] Voodoo dolls are only used in coop.
	if ( !( GAMEMODE_GetFlags( GAMEMODE_GetCurrentMode( )) & GMF_COOPERATIVE ) )
		return false;

	// [BB] Only use them if the compat flag tells us to.
	if ( sv_coopspawnvoodoodolls == false )
		return false;

	return true;
}

//*****************************************************************************
//	CONSOLE COMMANDS/VARIABLES

CUSTOM_CVAR( Bool, cooperative, true, CVAR_SERVERINFO | CVAR_LATCH | CVAR_CAMPAIGNLOCK )
{
	UCVarValue	Val;

	if ( self == true )
	{
		Val.Bool = false;

		// Disable deathmatch and teamgame if we're playing cooperative.
		if ( deathmatch != false )
			deathmatch.ForceSet( Val, CVAR_Bool );
		if ( teamgame != false )
			teamgame.ForceSet( Val, CVAR_Bool );
	}
	else
	{
		Val.Bool = false;

		// Cooperative, so disable all related modes.
		survival.ForceSet( Val, CVAR_Bool );
		invasion.ForceSet( Val, CVAR_Bool );
	}

	// Reset what the current game mode is.
	GAMEMODE_DetermineGameMode( );
}

//*****************************************************************************
//
CUSTOM_CVAR( Bool, survival, false, CVAR_SERVERINFO | CVAR_LATCH | CVAR_CAMPAIGNLOCK )
{
	UCVarValue	Val;

	if ( self == true )
	{
		Val.Bool = true;

		// Enable cooperative.
		cooperative.ForceSet( Val, CVAR_Bool );

		Val.Bool = false;

		// Disable other modes.
		invasion.ForceSet( Val, CVAR_Bool );
	}

	// Reset what the current game mode is.
	GAMEMODE_DetermineGameMode( );
}

//*****************************************************************************
//
CUSTOM_CVAR( Bool, invasion, false, CVAR_SERVERINFO | CVAR_LATCH | CVAR_CAMPAIGNLOCK )
{
	UCVarValue	Val;

	if ( self == true )
	{
		Val.Bool = true;

		// Enable cooperative.
		cooperative.ForceSet( Val, CVAR_Bool );

		Val.Bool = false;

		// Disable other modes.
		survival.ForceSet( Val, CVAR_Bool );
	}

	// Reset what the current game mode is.
	GAMEMODE_DetermineGameMode( );
}

//*****************************************************************************
//*****************************************************************************
//	CONSOLE VARIABLES

