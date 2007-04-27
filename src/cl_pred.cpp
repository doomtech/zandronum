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
// Filename: cl_pred.cpp
//
// Description: Contains variables and routines related to the client prediction
// portion of the program.
//
//-----------------------------------------------------------------------------

#include "doomtype.h"
#include "doomstat.h"
#include "d_player.h"
#include "g_game.h"
#include "d_net.h"
#include "p_local.h"
#include "gi.h"
#include "i_system.h"
#include "c_dispatch.h"
#include "st_stuff.h"
#include "m_argv.h"
#include "p_effect.h"
#include "c_console.h"
#include "cl_main.h"

void P_MovePlayer (player_s *player, ticcmd_t *cmd);
void P_CalcHeight (player_s *player);
void P_DeathThink (player_s *player);
bool	P_AdjustFloorCeil (AActor *thing);

//*****************************************************************************
//	VARIABLES

// Are we predicting?
static	bool	g_bPredicting = false;

// Store crucial player attributes for prediction.
static	ticcmd_t	g_SavedTiccmd[MAXSAVETICS];
static	angle_t		g_SavedAngle[MAXSAVETICS];
static	fixed_t		g_SavedPitch[MAXSAVETICS];
static	LONG		g_lSavedJumpTicks[MAXSAVETICS];
static	LONG		g_lSavedReactionTime[MAXSAVETICS];
static	LONG		g_lSavedWaterLevel[MAXSAVETICS];
static	fixed_t		g_SavedViewHeight[MAXSAVETICS];
static	fixed_t		g_SavedDeltaViewHeight[MAXSAVETICS];
//static	fixed_t		g_SavedFloorZ[MAXSAVETICS];

#ifdef	_DEBUG
CVAR( Bool, cl_showpredictionsuccess, false, 0 );
CVAR( Bool, cl_showonetickpredictionerrors, false, 0 );
#endif

//*****************************************************************************
//	FUNCTIONS

void CLIENT_PREDICT_PlayerPredict( void )
{
	player_t	*pPlayer;
	LONG		lTick;
	ULONG		ulPredictionTicks;
	bool		bUpdateViewHeight = false;
#ifdef	_DEBUG
	fixed_t		SavedX;
	fixed_t		SavedY;
	fixed_t		SavedZ;
	LONG		lPredictedTicks = gametic - CLIENT_GetLastConsolePlayerUpdateTick( ) - 1;
#endif

	// Always predict only the console player.
	pPlayer = &players[consoleplayer];

	// Precaution.
	if ( pPlayer->mo == NULL )
		return;

	// For spectators, we don't care about prediction. Just think and leave.
	if (( pPlayer->bSpectating ) || ( pPlayer->playerstate == PST_DEAD ))
	{
		P_PlayerThink( pPlayer );
		pPlayer->mo->Tick( );
		return;
	}

	// Just came out of a teleport.
	if ( pPlayer->mo->reactiontime )
	{
		pPlayer->cmd.ucmd.forwardmove = 0;
		pPlayer->cmd.ucmd.sidemove = 0;
	
		P_PlayerThink( pPlayer );
		pPlayer->mo->Tick( );
		return;
	}

	// Save a bunch of crucial attributes of the player that are necessary for prediction.
	g_SavedAngle[gametic % MAXSAVETICS] = pPlayer->mo->angle;
	g_SavedPitch[gametic % MAXSAVETICS] = pPlayer->mo->pitch;
	g_lSavedJumpTicks[gametic % MAXSAVETICS] = pPlayer->jumpTics;
	g_lSavedReactionTime[gametic % MAXSAVETICS] = pPlayer->mo->reactiontime;
	g_lSavedWaterLevel[gametic % MAXSAVETICS] = pPlayer->mo->waterlevel;
	g_SavedViewHeight[gametic % MAXSAVETICS] = pPlayer->viewheight;
	g_SavedDeltaViewHeight[gametic % MAXSAVETICS] = pPlayer->deltaviewheight;
//	g_SavedFloorZ[gametic % MAXSAVETICS] = pPlayer->mo->floorz;
	memcpy( &g_SavedTiccmd[gametic % MAXSAVETICS], &pPlayer->cmd, sizeof( ticcmd_t ));

#ifdef	_DEBUG
	SavedX	= pPlayer->mo->x;
	SavedY	= pPlayer->mo->y;
	SavedZ	= pPlayer->mo->z;
#endif

	// Used for reading stored commands.
	lTick = CLIENT_GetLastConsolePlayerUpdateTick( ) + 1;

		// How many ticks of prediction do we need?
	if (( CLIENT_GetLastConsolePlayerUpdateTick( ) - 1 ) > gametic )
		ulPredictionTicks = 0;
	else
		ulPredictionTicks = gametic - CLIENT_GetLastConsolePlayerUpdateTick( ) - 1;

#ifdef	_DEBUG
	if (( cl_showonetickpredictionerrors ) && ( ulPredictionTicks == 0 ))
	{
		if (( pPlayer->ServerXYZ[0] != pPlayer->mo->x ) ||
			( pPlayer->ServerXYZ[1] != pPlayer->mo->y ) ||
			( pPlayer->ServerXYZ[2] != pPlayer->mo->z ))
		{
			Printf( "(%d) WARNING! ServerXYZ does not match local origin after 1 tick!\n", gametic );
			Printf( "     X: %d, %d\n", pPlayer->ServerXYZ[0], pPlayer->mo->x );
			Printf( "     Y: %d, %d\n", pPlayer->ServerXYZ[1], pPlayer->mo->y );
			Printf( "     Z: %d, %d\n", pPlayer->ServerXYZ[2], pPlayer->mo->z );
		}

		if (( pPlayer->ServerXYZMom[0] != pPlayer->mo->momx ) ||
			( pPlayer->ServerXYZMom[1] != pPlayer->mo->momy ) ||
			( pPlayer->ServerXYZMom[2] != pPlayer->mo->momz ))
		{
			Printf( "(%d) WARNING! ServerXYZMom does not match local origin after 1 tick!\n", gametic );
			Printf( "     X: %d, %d\n", pPlayer->ServerXYZMom[0], pPlayer->mo->momx );
			Printf( "     Y: %d, %d\n", pPlayer->ServerXYZMom[1], pPlayer->mo->momy );
			Printf( "     Z: %d, %d\n", pPlayer->ServerXYZMom[2], pPlayer->mo->momz );
		}
	}
#endif

	// Set the player's position as told to him by the server.
	CLIENT_MoveThing( pPlayer->mo,
		pPlayer->ServerXYZ[0],
		pPlayer->ServerXYZ[1],
		pPlayer->ServerXYZ[2] );

	// Since plats, and other types of moving sectors may have started later on the client
	// end, we may need to adjust the player's view height later.
	if ( pPlayer->ServerXYZ[2] < pPlayer->mo->floorz )
		bUpdateViewHeight = true;

	// Set the player's velocity as told to him by the server.
	pPlayer->mo->momx = pPlayer->ServerXYZMom[0];
	pPlayer->mo->momy = pPlayer->ServerXYZMom[1];
	pPlayer->mo->momz = pPlayer->ServerXYZMom[2];
	
	if ( pPlayer->playerstate == PST_DEAD )
	{
		if ( cl_predict_players )
		{
			// Predict forward until tic >= gametic.
			while ( ulPredictionTicks )
			{
				// Disable bobbing, sounds, etc.
				g_bPredicting = true;
				pPlayer->mo->Tick( );				
				ulPredictionTicks--;
			}
		}

		// Done predicting.
		g_bPredicting = false;

		// Now do our updates for this tick.
		pPlayer->mo->Tick( );				
		P_DeathThink( pPlayer );

		return;
	}

	if ( cl_predict_players )
	{
		// Predict forward until tic >= gametic.
		while ( ulPredictionTicks )
		{
			// Disable bobbing, sounds, etc.
			g_bPredicting = true;

			// Use backed up values for prediction.
			pPlayer->mo->angle = g_SavedAngle[lTick % MAXSAVETICS];
			pPlayer->mo->pitch = g_SavedPitch[lTick % MAXSAVETICS];
			pPlayer->jumpTics = g_lSavedJumpTicks[lTick % MAXSAVETICS];
			pPlayer->mo->reactiontime = g_lSavedReactionTime[lTick % MAXSAVETICS];
			pPlayer->mo->waterlevel = g_lSavedWaterLevel[lTick % MAXSAVETICS];
			pPlayer->viewheight = g_SavedViewHeight[lTick % MAXSAVETICS];
			pPlayer->deltaviewheight = g_SavedDeltaViewHeight[lTick % MAXSAVETICS];
//			pPlayer->mo->floorz = g_SavedFloorZ[lTick % MAXSAVETICS];

			P_PlayerThink( pPlayer, &g_SavedTiccmd[lTick % MAXSAVETICS] );

			pPlayer->mo->Tick( );				
			ulPredictionTicks--;
			lTick++;
		}

		// Done predicting.
		g_bPredicting = false;
	}

	// Restore crucial attributes for this tick.
	pPlayer->mo->angle = g_SavedAngle[gametic % MAXSAVETICS];
	pPlayer->mo->pitch = g_SavedPitch[gametic % MAXSAVETICS];
	pPlayer->jumpTics = g_lSavedJumpTicks[gametic % MAXSAVETICS];
	pPlayer->mo->reactiontime = g_lSavedReactionTime[gametic % MAXSAVETICS];
	pPlayer->mo->waterlevel = g_lSavedWaterLevel[gametic % MAXSAVETICS];
	pPlayer->viewheight = g_SavedViewHeight[gametic % MAXSAVETICS];
	pPlayer->deltaviewheight = g_SavedDeltaViewHeight[gametic % MAXSAVETICS];
//	pPlayer->mo->floorz = g_SavedFloorZ[gametic % MAXSAVETICS];
	
#ifdef	_DEBUG
	if ( cl_showpredictionsuccess )
	{
		if (( SavedX == pPlayer->mo->x ) &&
			( SavedY == pPlayer->mo->y ) &&
			( SavedZ == pPlayer->mo->z ))
		{
			Printf( "SUCCESSFULLY predicted %d ticks!\n", lPredictedTicks );
		}
		else
		{
			Printf( "FAILED to predict %d ticks.\n", lPredictedTicks );
		}
	}
#endif

	// Now that all of the prediction has been done, do our movement for this tick.
	P_PlayerThink( pPlayer );
	pPlayer->mo->Tick( );

	// Finally, reset the viewheight if necessary.
	if ( bUpdateViewHeight )
	{
		pPlayer->viewheight = pPlayer->mo->ViewHeight;
		pPlayer->viewz = pPlayer->viewheight + pPlayer->mo->z;
	}
	// Finally, reset the viewheight.
//	pPlayer->viewheight = VIEWHEIGHT;
//	P_CalcHeight( pPlayer );
}

//*****************************************************************************
//
void CLIENT_PREDICT_SaveCmd( void )
{
	memcpy( &g_SavedTiccmd[gametic % MAXSAVETICS], &players[consoleplayer].cmd, sizeof( ticcmd_t ));
}

//*****************************************************************************
//*****************************************************************************
//
bool CLIENT_PREDICT_IsPredicting( void )
{
	return ( g_bPredicting );
}
