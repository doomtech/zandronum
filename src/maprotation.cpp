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
// Filename: maprotation.cpp
//
// Description: Contains maprotation functions
//
//-----------------------------------------------------------------------------

#include <string.h>

#include "c_cvars.h"
#include "c_dispatch.h"
#include "g_level.h"
#include "m_random.h"
#include "maprotation.h"
#include "w_wad.h"

//*****************************************************************************
//	VARIABLES

static	MAPROTATIONENTRY_t		g_MapRotationEntries[MAX_MAPROTATIONLIST_ENTRIES];
static	ULONG					g_ulCurMapInList;

//*****************************************************************************
//	PROTOTYPES

static	void		maprotation_AddMap( char *pszMapName );

//*****************************************************************************
//	FUNCTIONS

void MAPROTATION_Construct( void )
{
	ULONG	ulIdx;

	g_ulCurMapInList = 0;
	for ( ulIdx = 0; ulIdx < MAX_MAPROTATIONLIST_ENTRIES; ulIdx++ )
	{
		g_MapRotationEntries[ulIdx].szMapName[0] = 0;
		g_MapRotationEntries[ulIdx].bUsed = false;
	}
}

//*****************************************************************************
//
ULONG MAPROTATION_GetNumEntries( void )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAX_MAPROTATIONLIST_ENTRIES; ulIdx++ )
	{
		if ( g_MapRotationEntries[ulIdx].szMapName[0] == 0 )
			return ( ulIdx );
	}

	return ( ulIdx );
}

//*****************************************************************************
//
void MAPROTATION_AdvanceMap( void )
{
	if ( MAPROTATION_GetNumEntries( ) <= 1 )
		return;

	if ( sv_randommaprotation )
	{
		ULONG	ulNumEntriesLeft;
		ULONG	ulIdx;
		ULONG	ulLastMap;

		// Mark the current map in the list as being used.
		g_MapRotationEntries[g_ulCurMapInList].bUsed = true;

		ulNumEntriesLeft = 0;
		for ( ulIdx = 0; ulIdx < MAX_MAPROTATIONLIST_ENTRIES; ulIdx++ )
		{
			if ( g_MapRotationEntries[ulIdx].szMapName[0] == 0 )
				break;

			if ( g_MapRotationEntries[ulIdx].bUsed == false )
				ulNumEntriesLeft++;
		}

		// If all of the maps in this list have been used, flag them all as being unused, so that
		// they're all selectable once again in the random map rotation list.
		if ( ulNumEntriesLeft == 0 )
		{
			for ( ulIdx = 0; ulIdx < MAX_MAPROTATIONLIST_ENTRIES; ulIdx++ )
			{
				if ( g_MapRotationEntries[ulIdx].szMapName[0] == 0 )
					break;

				g_MapRotationEntries[ulIdx].bUsed = false;
			}
		}

		// Save the current map we're on, so that we don't select the same map two
		// times in a row!
		ulLastMap = g_ulCurMapInList;

		do
		{
			g_ulCurMapInList = M_Random.Random( ) % MAX_MAPROTATIONLIST_ENTRIES;
		} while (( g_MapRotationEntries[g_ulCurMapInList].szMapName[0] == 0 ) ||
				 ( g_MapRotationEntries[g_ulCurMapInList].bUsed == true ) ||
				 ( g_ulCurMapInList == ulLastMap ));
	}
	else
		g_ulCurMapInList = ( ++g_ulCurMapInList % MAPROTATION_GetNumEntries( ));
}

//*****************************************************************************
//
char *MAPROTATION_GetCurrentMapName( void )
{
	return ( g_MapRotationEntries[g_ulCurMapInList].szMapName );
}

//*****************************************************************************
//
char *MAPROTATION_GetMapName( ULONG ulIdx )
{
	if ( ulIdx > MAX_MAPROTATIONLIST_ENTRIES )
		return ( NULL );

	return ( g_MapRotationEntries[ulIdx].szMapName );
}

//*****************************************************************************
//
void MAPROTATION_SetPositionToMap( char *pszMapName )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAX_MAPROTATIONLIST_ENTRIES; ulIdx++ )
	{
		if ( g_MapRotationEntries[ulIdx].szMapName[0] == 0 )
			return;

		if ( stricmp( g_MapRotationEntries[ulIdx].szMapName, pszMapName ) == 0 )
		{
			g_ulCurMapInList = ulIdx;
			return;
		}
	}
}

//*****************************************************************************
//
bool MAPROTATION_IsMapInRotaion( char *pszMapName )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAX_MAPROTATIONLIST_ENTRIES; ulIdx++ )
	{
		if ( g_MapRotationEntries[ulIdx].szMapName[0] == 0 )
			return false;

		if ( stricmp( g_MapRotationEntries[ulIdx].szMapName, pszMapName ) == 0 )
		{
			return true;
		}
	}
	return false;
}

//*****************************************************************************
//*****************************************************************************
//
static void maprotation_AddMap( char *pszMapName )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAX_MAPROTATIONLIST_ENTRIES; ulIdx++ )
	{
		if ( g_MapRotationEntries[ulIdx].szMapName[0] == 0 )
			break;
	}

	if ( ulIdx == MAX_MAPROTATIONLIST_ENTRIES )
		return;

	sprintf( g_MapRotationEntries[ulIdx].szMapName, pszMapName );
	g_MapRotationEntries[ulIdx].bUsed = false;

	MAPROTATION_SetPositionToMap( level.mapname );
}

//*****************************************************************************
//	CONSOLE COMMANDS

CCMD( addmap )
{
	if ( argv.argc( ) > 1 )
		maprotation_AddMap( argv[1] );
	else
		Printf( "addmap <lumpname>: Adds a map to the map rotation list.\n" );
}

//*****************************************************************************
//
CCMD( maplist )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAX_MAPROTATIONLIST_ENTRIES; ulIdx++ )
	{
		if ( g_MapRotationEntries[ulIdx].szMapName[0] == '\0' )
			return;
		
		Printf( "%s\n", g_MapRotationEntries[ulIdx].szMapName );
	}
}

//*****************************************************************************
//
CCMD( clearmaplist )
{
	// Reset the map list.
	MAPROTATION_Construct( );

	Printf( "Map rotation list cleared.\n" );
}

//*****************************************************************************
//	CONSOLE VARIABLES

CVAR( Bool, sv_maprotation, true, CVAR_ARCHIVE );
CVAR( Bool, sv_randommaprotation, false, CVAR_ARCHIVE );
