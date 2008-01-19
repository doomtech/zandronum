// Emacs style mode select	 -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// DESCRIPTION:
//	 Duh.
// 
//-----------------------------------------------------------------------------


#ifndef __G_GAME__
#define __G_GAME__

#include "doomdef.h"
#include "d_event.h"



//
// GAME
//
void G_DeathMatchSpawnPlayer( int playernum, bool bClientUpdate );
void G_TemporaryTeamSpawnPlayer( ULONG ulPlayer, bool bClientUpdate );
void G_TeamgameSpawnPlayer( ULONG ulPlayer, ULONG ulTeam, bool bClientUpdate );
void G_CooperativeSpawnPlayer( ULONG ulPlayer, bool bClientUpdate, bool bTempPlayer = false );

// [BC] Determines the game type by map spots and other items placed on the level.
void	GAME_CheckMode( void );

// [BC] Function that reverts the map into its original state when it first loaded, without
// actually reloading the map.
void	GAME_ResetMap( void );

// [BC] Spawn the terminator artifact at a random deathmatch spot for terminator games.
void	GAME_SpawnTerminatorArtifact( void );

// [BC] Spawn the possession artifact at a random deathmatch spot for possession/team possession games.
void	GAME_SpawnPossessionArtifact( void );

// [BC] Access functions.
void	GAME_SetEndLevelDelay( ULONG ulTicks );
ULONG	GAME_GetEndLevelDelay( void );

void	GAME_SetLevelIntroTicks( USHORT usTicks );
USHORT	GAME_GetLevelIntroTicks( void );

// [BC] Rivecoder's function.
LONG	GAME_CountLivingPlayers( void );

// [BC] End changes.

void G_DeferedPlayDemo (char* demo);

// Can be called by the startup code or M_Responder,
// calls P_SetupLevel or W_EnterWorld.
void G_LoadGame (const char* name);

void G_DoLoadGame (void);

// Called by M_Responder.
void G_SaveGame (const char *filename, const char *description);

// Only called by startup code.
void G_RecordDemo (char* name);

void G_BeginRecording (const char *startmap);

void G_PlayDemo (char* name);
void G_TimeDemo (char* name);
bool G_CheckDemoStatus (void);

void G_WorldDone (void);

void G_Ticker (void);
bool G_Responder (event_t*	ev);

void G_ScreenShot (char *filename);

FString G_BuildSaveName (const char *prefix, int slot);

struct PNGHandle;
bool G_CheckSaveGameWads (PNGHandle *png, bool printwarn);

enum EFinishLevelType
{
	FINISH_SameHub,
	FINISH_NextHub,
	FINISH_NoHub
};

void G_PlayerFinishLevel (int player, EFinishLevelType mode, bool resetinventory);

void G_DoReborn (int playernum, bool freshbot);

// Adds pitch to consoleplayer's viewpitch and clamps it
void G_AddViewPitch (int look);

// Adds to consoleplayer's viewangle if allowed
void G_AddViewAngle (int yaw);

#define BODYQUESIZE 	32
class AActor;
extern AActor *bodyque[BODYQUESIZE]; 
extern int bodyqueslot; 


#endif
