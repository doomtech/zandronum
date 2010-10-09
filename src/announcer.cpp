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
// Filename: announcer.cpp
//
// Description: Contains announcer functions
//
//-----------------------------------------------------------------------------

#include "announcer.h"
#include "c_cvars.h"
#include "c_dispatch.h"
#include "cmdlib.h"
#include "doomstat.h"
#include "d_player.h"
#include "deathmatch.h"
#include "doomtype.h"
#include "i_system.h"
#include "s_sound.h"
#include "sc_man.h"
#include "team.h"
#include "w_wad.h"

//*****************************************************************************
//	VARIABLES

static	ANNOUNCERPROFILE_s	*g_AnnouncerProfile[MAX_ANNOUNCERPROFILES];

// Have the "Three frags left!", etc. sounds been played yet?
static	bool			g_bThreeFragsLeftSoundPlayed;
static	bool			g_bTwoFragsLeftSoundPlayed;
static	bool			g_bOneFragLeftSoundPlayed;

// Have the "Three points left!", etc. sounds been played yet?
static	bool			g_bThreePointsLeftSoundPlayed;
static	bool			g_bTwoPointsLeftSoundPlayed;
static	bool			g_bOnePointLeftSoundPlayed;

static	LONG			g_lLastSoundID = 0;

CVAR (Bool, cl_alwaysplayfragsleft, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Bool, cl_allowmultipleannouncersounds, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

//*****************************************************************************
//	PROTOTYPES

static	void					announcer_ParseAnnouncerInfoLump( FScanner &sc );
static	bool					announcer_AddAnnouncerProfile( ANNOUNCERPROFILE_s *pInfo );
static	void					announcer_AddProfileEntry( ANNOUNCERPROFILE_s *pInfo, const char *pszEntry, const char *pszSound );
static	ANNOUNCERENTRY_s		*announcer_FindEntry( ANNOUNCERPROFILE_s *pInfo, const char *pszEntry );
static	void					announcer_FinishAddingEntries( ANNOUNCERPROFILE_s *pInfo );

//*****************************************************************************
//	FUNCTIONS

void ANNOUNCER_Construct( void )
{
	ULONG	ulIdx;

	// Initialize all announcer info pointers.
	for ( ulIdx = 0; ulIdx < MAX_ANNOUNCERPROFILES; ulIdx++ )
		g_AnnouncerProfile[ulIdx] = NULL;

	// Call ANNOUNCER_Destruct() when Skulltag closes.
	atterm( ANNOUNCER_Destruct );
}

//*****************************************************************************
//
void ANNOUNCER_Destruct( void )
{
	ULONG	ulIdx;
	ULONG	ulIdx2;

	// First, go through and free all additional announcer info's.
	for ( ulIdx = 0; ulIdx < MAX_ANNOUNCERPROFILES; ulIdx++ )
	{
		if ( g_AnnouncerProfile[ulIdx] != NULL )
		{
			if ( g_AnnouncerProfile[ulIdx]->paAnnouncerEntries != NULL )
			{
				for ( ulIdx2 = 0; ulIdx2 < MAX_ANNOUNCERPROFILE_ENTRIES; ulIdx2++ )
				{
					if ( g_AnnouncerProfile[ulIdx]->paAnnouncerEntries[ulIdx2] )
					{
						M_Free( g_AnnouncerProfile[ulIdx]->paAnnouncerEntries[ulIdx2] );
						g_AnnouncerProfile[ulIdx]->paAnnouncerEntries[ulIdx2] = NULL;
					}
				}

				M_Free( g_AnnouncerProfile[ulIdx]->paAnnouncerEntries );
				g_AnnouncerProfile[ulIdx]->paAnnouncerEntries = NULL;
			}

			M_Free( g_AnnouncerProfile[ulIdx] );
			g_AnnouncerProfile[ulIdx] = NULL;
		}
	}
}

//*****************************************************************************
//
void ANNOUNCER_ParseAnnouncerInfo( void )
{
	LONG		lCurLump;
	LONG		lLastLump = 0;

	// Search through all loaded wads for a lump called "ANCRINFO".
	while (( lCurLump = Wads.FindLump( "ANCRINFO", (int *)&lLastLump )) != -1 )
	{
		// Make pszBotInfo point to the raw data (which should be a text file) in the ANCRINFO lump.
		FScanner sc( lCurLump );

		// Parse the lump.
		announcer_ParseAnnouncerInfoLump( sc );
	}
}

//*****************************************************************************
//
ULONG ANNOUNCER_GetNumProfiles( void )
{
	ULONG	ulIdx;
	ULONG	ulNumProfiles = 0;

	for ( ulIdx = 0; ulIdx < MAX_ANNOUNCERPROFILES; ulIdx++ )
	{
		if ( g_AnnouncerProfile[ulIdx] != NULL )
			ulNumProfiles++;
		else
			return ( ulNumProfiles );
	}

	return ( ulNumProfiles );
}

//*****************************************************************************
//
bool ANNOUNCER_DoesEntryExist( ULONG ulProfileIdx, const char *pszEntry )
{
	// Return false if the profile index is invalid, or a profile doesn't exist on this index.
	if (( ulProfileIdx >= MAX_ANNOUNCERPROFILES ) || ( g_AnnouncerProfile[ulProfileIdx] == NULL ))
		return ( false );

	// If the profile doesn't have any entries defined, return false.
	if ( g_AnnouncerProfile[ulProfileIdx]->paAnnouncerEntries == NULL )
		return ( false );

	// If the entry exists in the profile, return true.
	return ( announcer_FindEntry( g_AnnouncerProfile[ulProfileIdx], pszEntry ) != NULL );
}

//*****************************************************************************
//
void ANNOUNCER_PlayEntry( ULONG ulProfileIdx, const char *pszEntry )
{
	ANNOUNCERENTRY_s	*pEntry;

	// Return false if the profile index is invalid, or a profile doesn't exist on this index.
	if (( ulProfileIdx >= MAX_ANNOUNCERPROFILES ) || ( g_AnnouncerProfile[ulProfileIdx] == NULL ))
		return;

	// If the profile doesn't have any entries defined, return false.
	if ( g_AnnouncerProfile[ulProfileIdx]->paAnnouncerEntries == NULL )
		return;

	// If the entry exists and has a sound, play it.
	pEntry = announcer_FindEntry( g_AnnouncerProfile[ulProfileIdx], pszEntry );
	if (( pEntry ) &&
		( pEntry->szSound ) &&
		( strlen( pEntry->szSound ) > 0 ))
	{
		// Stop any existing announcer sounds.
		// [BB] Only do this, if the user doesn't like multiple announcer sounds at once
		if ( cl_allowmultipleannouncersounds == false )
			S_StopSoundID( g_lLastSoundID, CHAN_VOICE );

		// Play the sound.
		g_lLastSoundID = S_FindSound( pEntry->szSound );
		S_Sound( CHAN_VOICE, pEntry->szSound, 1, ATTN_NONE );
	}
}

//*****************************************************************************
//
void ANNOUNCER_PotentiallyPlayFragsLeftSound( LONG lFragsLeft )
{
	switch ( lFragsLeft )
	{
	case 3:

		if ( (( g_bThreeFragsLeftSoundPlayed == false ) &&
			( g_bTwoFragsLeftSoundPlayed == false ) &&
			( g_bOneFragLeftSoundPlayed == false ))
			|| cl_alwaysplayfragsleft )
		{
			ANNOUNCER_PlayEntry( cl_announcer, "ThreeFragsLeft" );
			g_bThreeFragsLeftSoundPlayed = true;
		}
		break;
	case 2:

		if ( (( g_bTwoFragsLeftSoundPlayed == false ) &&
			( g_bOneFragLeftSoundPlayed == false ))
			|| cl_alwaysplayfragsleft )
		{
			ANNOUNCER_PlayEntry( cl_announcer, "TwoFragsLeft" );
			g_bTwoFragsLeftSoundPlayed = true;
		}
		break;
	case 1:

		if ( (g_bOneFragLeftSoundPlayed == false)
			|| cl_alwaysplayfragsleft )
		{
			ANNOUNCER_PlayEntry( cl_announcer, "OneFragLeft" );
			g_bOneFragLeftSoundPlayed = true;
		}
		break;
	}
}

//*****************************************************************************
//
void ANNOUNCER_PlayFragSounds( ULONG ulPlayer, LONG lOldFragCount, LONG lNewFragCount )
{
	ULONG			ulIdx;
	LONG			lOldLeaderFrags;
	LONG			lNewLeaderFrags;
	LEADSTATE_e		OldLeadState;
	LEADSTATE_e		LeadState;

	// Announce a lead change. Only do if the consoleplayer's camera is valid.
	if (( players[consoleplayer].mo != NULL ) &&
		( players[consoleplayer].camera != NULL ) &&
		( players[consoleplayer].camera->player != NULL ) &&
		( players[consoleplayer].camera->player->bSpectating == false ))
	{
		// Find the highest fragcount of a player other than the player whose eye's we're
		// looking through. This is compared to the player's old fragcount, and the player's
		// new fragcount. If the player's lead state has changed, play a sound.
		lOldLeaderFrags = INT_MIN;
		lNewLeaderFrags = INT_MIN;
		for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
		{
			// Don't factor our fragcount into the highest fragcount.
			if ( players[ulIdx].mo->CheckLocalView( consoleplayer ))
				continue;

			// Don't factor in players who are not in the game, or who are spectating.
			if (( playeringame[ulIdx] == false ) || ( players[ulIdx].bSpectating ))
				continue;

			if ( ulIdx == ulPlayer )
			{
				if ( lOldLeaderFrags < lOldFragCount )
					lOldLeaderFrags = lOldFragCount;
				if ( lNewLeaderFrags < lNewFragCount )
					lNewLeaderFrags = lNewFragCount;
			}
			else
			{
				if ( lOldLeaderFrags < players[ulIdx].fragcount )
					lOldLeaderFrags = players[ulIdx].fragcount;
				if ( lNewLeaderFrags < players[ulIdx].fragcount )
					lNewLeaderFrags = players[ulIdx].fragcount;
			}
		}

		// If players were found in game, just break out, since our lead couldn't have possibly changed.
		if (( lOldLeaderFrags == INT_MIN ) && ( lNewLeaderFrags == INT_MIN ))
			return;

		if ( players[ulPlayer].mo->CheckLocalView( consoleplayer ))
		{
			OldLeadState = ( lOldFragCount > lOldLeaderFrags ) ? LEADSTATE_INTHELEAD : ( lOldFragCount == lOldLeaderFrags ) ? LEADSTATE_TIEDFORTHELEAD : LEADSTATE_NOTINTHELEAD;
			LeadState = ( lNewFragCount > lNewLeaderFrags ) ? LEADSTATE_INTHELEAD : ( lNewFragCount == lNewLeaderFrags ) ? LEADSTATE_TIEDFORTHELEAD : LEADSTATE_NOTINTHELEAD;
		}
		else
		{
			OldLeadState = ( players[consoleplayer].camera->player->fragcount > lOldLeaderFrags ) ? LEADSTATE_INTHELEAD : ( players[consoleplayer].camera->player->fragcount == lOldLeaderFrags ) ? LEADSTATE_TIEDFORTHELEAD : LEADSTATE_NOTINTHELEAD;
			LeadState = ( players[consoleplayer].camera->player->fragcount > lNewLeaderFrags ) ? LEADSTATE_INTHELEAD : ( players[consoleplayer].camera->player->fragcount == lNewLeaderFrags ) ? LEADSTATE_TIEDFORTHELEAD : LEADSTATE_NOTINTHELEAD;
		}

		// If our lead state has changed, play a sound.
		if ( OldLeadState != LeadState )
		{
			switch ( LeadState )
			{
			// Display player has taken the lead!
			case LEADSTATE_INTHELEAD:

				ANNOUNCER_PlayEntry( cl_announcer, "YouveTakenTheLead" );
				break;
			// Display player is tied for the lead!
			case LEADSTATE_TIEDFORTHELEAD:

				ANNOUNCER_PlayEntry( cl_announcer, "YouAreTiedForTheLead" );
				break;
			// Display player has lost the lead.
			case LEADSTATE_NOTINTHELEAD:

				ANNOUNCER_PlayEntry( cl_announcer, "YouveLostTheLead" );
				break;
			}
		}
	}

	// Potentially play the "3 frags left", etc. announcer sounds.
	if (( lastmanstanding == false ) && ( teamlms == false ) && ( deathmatch ) && ( fraglimit ))
	{
		ANNOUNCER_PotentiallyPlayFragsLeftSound( fraglimit - lNewFragCount );
	}
}

//*****************************************************************************
//
void ANNOUNCER_PlayTeamFragSounds( ULONG ulTeam, LONG lOldFragCount, LONG lNewFragCount )
{
	LONG			lScore[MAX_TEAMS];
	LONG			lHighestFragCount = LONG_MIN;
	bool			bPossibleTeams[MAX_TEAMS];
	ULONG			ulNumberOfPossibleTeams = 0;
	ULONG			ulMaxPossibleTeams = 0;

	// The given team is not valid.
	if ( TEAM_CheckIfValid( ulTeam ) == false )
		return;

	// [BB] Don't announce anything if there is at most one team with players.
	if ( TEAM_TeamsWithPlayersOn() < 2 )
		return;

	// Set all possible teams to false.
	for ( ULONG i = 0; i < MAX_TEAMS; i++ )
	{
		bPossibleTeams[i] = false;
		lScore[i] = LONG_MIN;
	}

	// Find the scores for each of the teams.
	// [BB] Only set the frag count of the available teams (the others stay at LONG_MIN).
	for ( ULONG i = 0; i < TEAM_GetNumAvailableTeams(); i++ )
	{
		if ( i == ulTeam )
			lScore[i] = lNewFragCount;
		else
			lScore[i] = TEAM_GetFragCount( i );

		if ( lScore[i] > lHighestFragCount)
			lHighestFragCount = lScore[i];
	}

	for ( ULONG i = 0; i < teams.Size( ); i++ )
	{
		// Set the possible teams.
		if ( lScore[i] == lHighestFragCount )
		{
			bPossibleTeams[i] = true;
			ulNumberOfPossibleTeams++;
		}

		// Count the maximum teams used.
		if ( TEAM_CountPlayers( i ) < 1 )
			continue;

		ulMaxPossibleTeams++;
	}

	// Play the "teams are tied" sound if all active teams have equal scores.
	if ( ulNumberOfPossibleTeams == ulMaxPossibleTeams )
	{
		// [BB] Make sure that the next leading team will be announced again.
		for ( ULONG j = 0; j < teams.Size( ); ++j )
			TEAM_SetAnnouncedLeadState( j, false );

		ANNOUNCER_PlayEntry( cl_announcer, "TeamsAreTied");
		return;
	}

	// Don't allow any sounds to play if more than one teams are leading.
	if ( ulNumberOfPossibleTeams > 1 )
	{
		return;
	}

	// Announce the team leading message.
	for ( ULONG i = 0; i < teams.Size( ); i++ )
	{
		if ( bPossibleTeams[i] == false || TEAM_GetAnnouncedLeadState( i ))
			continue;

		// Lead state - make sure we only announce a team leading once and not
		// every frag.
		for ( ULONG j = 0; j < teams.Size( ); j++ )
			TEAM_SetAnnouncedLeadState( j, false );

		TEAM_SetAnnouncedLeadState( i, true );

		// Build the message.
		// Whatever the team's name is, is the first part of the message. For example:
		// if the "blue" team has been defined then the message will be "BlueLeads".
		// This way we don't have to change every announcer to use a new system. 
		FString name;
		name += TEAM_GetName( i );
		name += "Leads";

		// Finally, play the built message.
		ANNOUNCER_PlayEntry( cl_announcer, name.GetChars( ));
	}

	// Potentially play the "3 frags left", etc. announcer sounds.
	if ( fraglimit )
	{
		ANNOUNCER_PotentiallyPlayFragsLeftSound( fraglimit - lNewFragCount );
	}
}

//*****************************************************************************
//
/*
void ANNOUNCER_PlayScoreSounds( ULONG ulPlayer, LONG lOldPointCount, LONG lNewPointCount )
{
	ULONG			ulIdx;
	LONG			lHighestPointCount;
	LEADSTATE_e		OldLeadState;
	LEADSTATE_e		LeadState;

	// Announce a lead change. Only do if the consoleplayer's camera is valid.
	if (( players[consoleplayer].mo != NULL ) &&
		( players[consoleplayer].camera != NULL ) &&
		( players[consoleplayer].camera->player != NULL ) &&
		( players[consoleplayer].camera->player->bSpectating == false ))
	{
		// Find the highest fragcount of a player other than the player whose eye's we're
		// looking through. This is compared to the player's old fragcount, and the player's
		// new fragcount. If the player's lead state has changed, play a sound.
		lHighestPointCount = INT_MIN;
		for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
		{
			// Don't factor our fragcount into the highest fragcount.
			if ( players[ulIdx].mo->CheckLocalView( consoleplayer ))
				continue;

			// Don't factor in players who are not in the game, or who are spectating.
			if (( playeringame[ulIdx] == false ) || ( players[ulIdx].bSpectating ))
				continue;

			if ( lHighestPointCount < players[ulIdx].lPointCount )
				lHighestPointCount = players[ulIdx].lPointCount;
		}

		// If players were found in game, just break out, since our lead couldn't have possibly changed.
		if ( lHighestPointCount == INT_MIN )
			return;

		// Determine the lead state by comparing our score to whoever has the highest score that isn't us.
		if ( players[ulPlayer].mo->CheckLocalView( consoleplayer ))
		{
			OldLeadState = ( lOldPointCount > lHighestPointCount ) ? LEADSTATE_INTHELEAD : ( lOldPointCount == lHighestPointCount ) ? LEADSTATE_TIEDFORTHELEAD : LEADSTATE_NOTINTHELEAD;
			LeadState = ( lNewPointCount > lHighestPointCount ) ? LEADSTATE_INTHELEAD : ( lNewPointCount == lHighestPointCount ) ? LEADSTATE_TIEDFORTHELEAD : LEADSTATE_NOTINTHELEAD;
		}
		else
		{
			OldLeadState = ( players[consoleplayer].lPointCount > lHighestPointCount ) ? LEADSTATE_INTHELEAD : ( players[consoleplayer].lPointCount == lHighestPointCount ) ? LEADSTATE_TIEDFORTHELEAD : LEADSTATE_NOTINTHELEAD;

			if ( lHighestPointCount < lNewPointCount )
				lHighestPointCount = lNewPointCount;

			LeadState = ( players[consoleplayer].lPointCount > lHighestPointCount ) ? LEADSTATE_INTHELEAD : ( players[consoleplayer].lPointCount == lHighestPointCount ) ? LEADSTATE_TIEDFORTHELEAD : LEADSTATE_NOTINTHELEAD;
		}

		// If our lead state has changed, play a sound.
		if ( OldLeadState != LeadState )
		{
			switch ( LeadState )
			{
			// Display player has taken the lead!
			case LEADSTATE_INTHELEAD:

				ANNOUNCER_PlayEntry( cl_announcer, "YouveTakenTheLead" );
				break;
			// Display player is tied for the lead!
			case LEADSTATE_TIEDFORTHELEAD:

				ANNOUNCER_PlayEntry( cl_announcer, "YouAreTiedForTheLead" );
				break;
			// Display player has lost the lead.
			case LEADSTATE_NOTINTHELEAD:

				ANNOUNCER_PlayEntry( cl_announcer, "YouveLostTheLead" );
				break;
			}
		}
	}

	// Potentially play the "3 points left", etc. announcer sounds.
	if (( lastmanstanding == false ) && ( teamlms == false ) && ( deathmatch ) && ( pointlimit ))
	{
		switch ( pointlimit - lNewPointCount )
		{
		case 3:

			if ( g_bThreePointsLeftSoundPlayed == false )
			{
				ANNOUNCER_PlayEntry( cl_announcer, "ThreePointsLeft" );
				g_bThreePointsLeftSoundPlayed = true;
			}
			break;
		case 2:
			
			if ( g_bTwoPointsLeftSoundPlayed == false )
			{
				ANNOUNCER_PlayEntry( cl_announcer, "TwoPointsLeft" );
				g_bTwoPointsLeftSoundPlayed = true;
			}
			break;
		case 1:

			if ( g_bOnePointLeftSoundPlayed == false )
			{
				ANNOUNCER_PlayEntry( cl_announcer, "OnePointLeft" );
				g_bOnePointLeftSoundPlayed = true;
			}
			break;
		}
	}
}
*/
//*****************************************************************************
//
void ANNOUNCER_AllowNumFragsAndPointsLeftSounds( void )
{
	g_bThreeFragsLeftSoundPlayed = false;
	g_bTwoFragsLeftSoundPlayed = false;
	g_bOneFragLeftSoundPlayed = false;

	g_bThreePointsLeftSoundPlayed = false;
	g_bTwoPointsLeftSoundPlayed = false;
	g_bOnePointLeftSoundPlayed = false;
}

//*****************************************************************************
//*****************************************************************************
//
char *ANNOUNCER_GetName( ULONG ulIdx )
{
	if ( ulIdx >= MAX_ANNOUNCERPROFILES || ( g_AnnouncerProfile[ulIdx] == NULL ))
		return ( NULL );

	return ( g_AnnouncerProfile[ulIdx]->szName );
}

//*****************************************************************************
//*****************************************************************************
//
static void announcer_ParseAnnouncerInfoLump( FScanner &sc )
{
	char				szKey[64];
	char				szValue[64];
	ULONG				ulIdx;
	ANNOUNCERPROFILE_s	AnnouncerProfile;

	// Begin parsing that text. COM_Parse will create a token (com_token), and
	// pszBotInfo will skip past the token.
	while ( sc.GetString( ))
	{
		// Initialize our announcer info variable.
		strncpy( AnnouncerProfile.szName, "UNNAMED ANNOUNCER", 63 );
		AnnouncerProfile.szName[63] = 0;

		AnnouncerProfile.paAnnouncerEntries = (ANNOUNCERENTRY_s **)M_Malloc( sizeof( ANNOUNCERENTRY_s ** ) * MAX_ANNOUNCERPROFILE_ENTRIES );
		for ( ulIdx = 0; ulIdx < MAX_ANNOUNCERPROFILE_ENTRIES; ulIdx++ )
		{
			AnnouncerProfile.paAnnouncerEntries[ulIdx] = (ANNOUNCERENTRY_s *)M_Malloc( sizeof( ANNOUNCERENTRY_s ));

			AnnouncerProfile.paAnnouncerEntries[ulIdx]->szName[0] = '\0';
			AnnouncerProfile.paAnnouncerEntries[ulIdx]->szSound[0] = '\0';
		}

		while ( sc.String[0] != '{' )
			sc.GetString( );

		// We've encountered a starting bracket. Now continue to parse until we hit an end bracket.
		while ( sc.String[0] != '}' )
		{
			// The current token should be our key. (key = value) If it's an end bracket, break.
			sc.GetString( );
			strncpy( szKey, sc.String, 63 );
			szKey[63] = 0;
			if ( sc.String[0] == '}' )
				break;

			// The following key must be an = sign. If not, the user made an error!
			sc.GetString( );
			if ( stricmp( sc.String, "=" ) != 0 )
					I_Error( "ANNOUNCER_ParseAnnouncerInfo: Missing \"=\" in ANCRINFO lump for field \"%s\".\n", szKey );

			// The last token should be our value.
			sc.GetString( );
			strncpy( szValue, sc.String, 63 );
			szValue[63] = 0;

			// If we're specifying the name of the profile, set it here.
			if ( stricmp( szKey, "name" ) == 0 )
			{
				strncpy( AnnouncerProfile.szName, szValue, 63 );
				AnnouncerProfile.szName[63] = 0;
			}
			// Add the new key, along with its value to the profile.
			else
				announcer_AddProfileEntry( &AnnouncerProfile, (const char *)szKey, (const char *)szValue );
		}

		// Now that we're done adding entries, sort them alphabetically.
		announcer_FinishAddingEntries( &AnnouncerProfile );

		// Add our completed announcer profile.
		announcer_AddAnnouncerProfile( &AnnouncerProfile );

		// Finally, free all the memory allocated for this temporary profile.
		for ( ulIdx = 0; ulIdx < MAX_ANNOUNCERPROFILE_ENTRIES; ulIdx++ )
		{
			M_Free( AnnouncerProfile.paAnnouncerEntries[ulIdx] );
			AnnouncerProfile.paAnnouncerEntries[ulIdx] = NULL;
		}

		M_Free( AnnouncerProfile.paAnnouncerEntries );
		AnnouncerProfile.paAnnouncerEntries = NULL;
	}
}

//*****************************************************************************
//
static bool announcer_AddAnnouncerProfile( ANNOUNCERPROFILE_s *pInfo )
{
	ULONG	ulIdx;
	ULONG	ulIdx2;

	// First, find a free slot to add the announcer profile.
	for ( ulIdx = 0; ulIdx < MAX_ANNOUNCERPROFILES; ulIdx++ )
	{
		if ( g_AnnouncerProfile[ulIdx] != NULL )
			continue;

		// Allocate some memory for this new block.
		g_AnnouncerProfile[ulIdx] = (ANNOUNCERPROFILE_s *)M_Malloc( sizeof( ANNOUNCERPROFILE_s ));

		// Now copy all the data we passed in into this block.
		strncpy( g_AnnouncerProfile[ulIdx]->szName, pInfo->szName, 63 );
		g_AnnouncerProfile[ulIdx]->szName[63] = 0;

		g_AnnouncerProfile[ulIdx]->paAnnouncerEntries = (ANNOUNCERENTRY_s **)M_Malloc( sizeof( ANNOUNCERENTRY_s ) * MAX_ANNOUNCERPROFILE_ENTRIES );
		for ( ulIdx2 = 0; ulIdx2 < MAX_ANNOUNCERPROFILE_ENTRIES; ulIdx2++ )
		{
			g_AnnouncerProfile[ulIdx]->paAnnouncerEntries[ulIdx2] = (ANNOUNCERENTRY_s *)M_Malloc( sizeof( ANNOUNCERENTRY_s ));

			strncpy( g_AnnouncerProfile[ulIdx]->paAnnouncerEntries[ulIdx2]->szName, pInfo->paAnnouncerEntries[ulIdx2]->szName, 31 );
			strncpy( g_AnnouncerProfile[ulIdx]->paAnnouncerEntries[ulIdx2]->szSound, pInfo->paAnnouncerEntries[ulIdx2]->szSound, 63 );
			g_AnnouncerProfile[ulIdx]->paAnnouncerEntries[ulIdx2]->szName[31] = 0;
			g_AnnouncerProfile[ulIdx]->paAnnouncerEntries[ulIdx2]->szSound[63] = 0;
		}

		return ( true );
	}

	return ( false );
}

//*****************************************************************************
//
static void announcer_AddProfileEntry( ANNOUNCERPROFILE_s *pInfo, const char *pszEntry, const char *pszSound )
{
	ULONG	ulIdx;

	// Invalid profile.
	if ( pInfo == NULL )
		return;

	// The profile has no entries.
	if ( pInfo->paAnnouncerEntries == NULL )
		return;

	// Add the entry to the first available slot.
	for ( ulIdx = 0; ulIdx < MAX_ANNOUNCERPROFILE_ENTRIES; ulIdx++ )
	{
		// Entry already exists.
		if ( stricmp( pInfo->paAnnouncerEntries[ulIdx]->szName, pszEntry ) == 0 )
			return;

		if ( pInfo->paAnnouncerEntries[ulIdx]->szName[0] == '\0' )
		{
			strncpy( pInfo->paAnnouncerEntries[ulIdx]->szName, pszEntry, 31 );
			strncpy( pInfo->paAnnouncerEntries[ulIdx]->szSound, pszSound, 63 );
			pInfo->paAnnouncerEntries[ulIdx]->szName[31] = 0;
			pInfo->paAnnouncerEntries[ulIdx]->szSound[63] = 0;

			// All done.
			return;
		}
	}
}

//*****************************************************************************
//
static ANNOUNCERENTRY_s *announcer_FindEntry( ANNOUNCERPROFILE_s *pInfo, const char *pszEntry )
{
	ULONG	ulIdx;

	// Invalid profile.
	if ( pInfo == NULL )
		return ( NULL );

	// The profile has no entries.
	if ( pInfo->paAnnouncerEntries == NULL )
		return ( NULL );

	// Search the announcer profile for the entry. If it exists, return it.
	// FIXME: Use the binary search here.
	for ( ulIdx = 0; ulIdx < MAX_ANNOUNCERPROFILE_ENTRIES; ulIdx++ )
	{
		if ( stricmp( pInfo->paAnnouncerEntries[ulIdx]->szName, pszEntry ) == 0 )
			return ( pInfo->paAnnouncerEntries[ulIdx] );
	}

	return ( NULL );
}

//*****************************************************************************
//
static void announcer_FinishAddingEntries( ANNOUNCERPROFILE_s *pInfo )
{
	// FIXME: Sort this list alphabetically.
}

//*****************************************************************************
//	CONSOLE VARIABLES/COMMANDS

CVAR( Int, cl_announcer, 0, CVAR_ARCHIVE )

// Display all the announce profiles that are loaded.
CCMD( announcers )
{
	ULONG	ulIdx;
	ULONG	ulNumProfiles = 0;

	for ( ulIdx = 0; ulIdx < MAX_ANNOUNCERPROFILES; ulIdx++ )
	{
		if ( g_AnnouncerProfile[ulIdx] != NULL )
		{
			Printf( "%d. %s\n", static_cast<unsigned int> (ulIdx + 1), g_AnnouncerProfile[ulIdx]->szName );
			ulNumProfiles++;
		}
	}

	Printf( "\n%d announcer profile(s) loaded.\n", static_cast<unsigned int> (ulNumProfiles) );
}
