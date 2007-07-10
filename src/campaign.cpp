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
// Date created:  1/15/05
//
//
// Filename: campaign.cpp
//
// Description: 
//
//-----------------------------------------------------------------------------

#include "campaign.h"
#include "cl_demo.h"
#include "deathmatch.h"
#include "i_system.h"
#include "network.h"
#include "sc_man.h"
#include "scoreboard.h"
#include "team.h"
#include "w_wad.h"

//*****************************************************************************
//	VARIABLES

static	CAMPAIGNINFO_t		*g_pCampaignInfoRoot;
static	bool				g_bDisableCampaign;
static	bool				g_bInCampaign;

//*****************************************************************************
//	FUNCTIONS

void CAMPAIGN_Construct( void )
{
	g_pCampaignInfoRoot = NULL;
	g_bDisableCampaign = false;
	g_bInCampaign = false;

	// Call CAMPAIGN_Destruct() when Skulltag closes.
	atterm( CAMPAIGN_Destruct );
}

//*****************************************************************************
//
void CAMPAIGN_Destruct( void )
{
	LONG			lNumEntries;
	ULONG			ulIdx;
	CAMPAIGNINFO_t	*pInfo;
	CAMPAIGNINFO_t	*pParent;

	lNumEntries = 0;
	pInfo = g_pCampaignInfoRoot;
	while ( pInfo != NULL )
	{
		lNumEntries++;
		pInfo = pInfo->pNextInfo;
	}

	for ( ulIdx = 0; ulIdx < (ULONG)lNumEntries; ulIdx++ )
	{
		pParent = NULL;
		pInfo = g_pCampaignInfoRoot;
		while ( pInfo != NULL )
		{
			if ( pInfo->pNextInfo == NULL )
			{
				free( pInfo );
				pInfo = NULL;
				if ( pParent )
					pParent->pNextInfo = NULL;
				break;
			}
			else
			{
				pParent = pInfo;
				pInfo = pInfo->pNextInfo;
			}
		}
	}
}

//*****************************************************************************
//
void CAMPAIGN_ParseCampaignInfo( void )
{
	LONG			lCurLump;
	LONG			lLastLump = 0;
	char			szKey[32];
	char			szValue[32];
	ULONG			ulIdx;
	CAMPAIGNINFO_t	*pInfo;

	pInfo = g_pCampaignInfoRoot;
	while ( pInfo != NULL )
		pInfo = pInfo->pNextInfo;

	// Search through all loaded wads for a lump called "CMPGNINF".
	while (( lCurLump = Wads.FindLump( "CMPGNINF", (int *)&lLastLump )) != -1 )
	{
		// Make pszBotInfo point to the raw data (which should be a text file) in the BOTINFO lump.
		SC_OpenLumpNum( lCurLump, "CMPGNINF" );

		// Begin parsing that text. COM_Parse will create a token (com_token), and
		// pszBotInfo will skip past the token.
		while ( SC_GetString( ))
		{
			if ( pInfo == NULL )
			{
				pInfo = new CAMPAIGNINFO_t;
				g_pCampaignInfoRoot = pInfo;
			}
			else
			{
				pInfo->pNextInfo = new CAMPAIGNINFO_t;
				pInfo = pInfo->pNextInfo;
			}

			pInfo->lFragLimit			= 0;
			pInfo->fTimeLimit			= 0.0f;
			pInfo->lPointLimit			= 0;
			pInfo->lDuelLimit			= 0;
			pInfo->lWinLimit			= 0;
			pInfo->lWaveLimit			= 0;
			pInfo->lGameMode			= GAMETYPE_DEATHMATCH;
			pInfo->lDMFlags				= -1;
			pInfo->lDMFlags2			= -1;
			pInfo->lCompatFlags			= -1;
			pInfo->szMapName[0]			= 0;
			pInfo->szPlayerTeam[0]		= 0;
			pInfo->bMustWinAllDuels		= true;
			pInfo->pNextInfo			= NULL;
			pInfo->lPossessionHoldTime	= 0;
			for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
			{
				pInfo->BotSpawn[ulIdx].szBotName[0] = 0;
				pInfo->BotSpawn[ulIdx].szBotTeam[0] = 0;
			}

			while ( sc_String[0] != '{' )
				SC_GetString( );

			// We've encountered a starting bracket. Now continue to parse until we hit an end bracket.
			while ( sc_String[0] != '}' )
			{
				// The current token should be our key. (key = value) If it's an end bracket, break.
				SC_GetString( );
				sprintf( szKey, sc_String );
				if ( sc_String[0] == '}' )
					break;

				// The following key must be an = sign. If not, the user made an error!
				SC_GetString( );
				if ( stricmp( sc_String, "=" ) != 0 )
					I_Error( "CAMPAIGN_ParseCampaignInfo: Missing \"=\" in CMPGNINF lump for field \"%s\"!\n", szKey );

				// The last token should be our value.
				SC_GetString( );
				sprintf( szValue, sc_String );

				// Now try to match our key with a valid bot info field.
				if ( stricmp( szKey, "mapname" ) == 0 )
				{
					sprintf( pInfo->szMapName, szValue );
				}
				else if ( stricmp( szKey, "fraglimit" ) == 0 )
				{
					pInfo->lFragLimit = atoi( szValue );
				}
				else if ( stricmp( szKey, "timelimit" ) == 0 )
				{
					pInfo->fTimeLimit = (float)atof( szValue );
				}
				else if ( stricmp( szKey, "pointlimit" ) == 0 )
				{
					pInfo->lPointLimit = atoi( szValue );
				}
				else if ( stricmp( szKey, "duellimit" ) == 0 )
				{
					pInfo->lDuelLimit = atoi( szValue );
				}
				else if ( stricmp( szKey, "winlimit" ) == 0 )
				{
					pInfo->lWinLimit = atoi( szValue );
				}
				else if ( stricmp( szKey, "wavelimit" ) == 0 )
				{
					pInfo->lWaveLimit = atoi( szValue );
				}
				else if ( stricmp( szKey, "gamemode" ) == 0 )
				{
					// Find out what gamemode they want.
					if ( stricmp( szValue, "cooperative" ) == 0 )
						pInfo->lGameMode = GAMETYPE_COOPERATIVE;
					else if ( stricmp( szValue, "survival" ) == 0 )
						pInfo->lGameMode = GAMETYPE_SURVIVAL;
					else if ( stricmp( szValue, "invasion" ) == 0 )
						pInfo->lGameMode = GAMETYPE_INVASION;
					else if ( stricmp( szValue, "deathmatch" ) == 0 )
						pInfo->lGameMode = GAMETYPE_DEATHMATCH;
					else if ( stricmp( szValue, "teamplay" ) == 0 )
						pInfo->lGameMode = GAMETYPE_TEAMPLAY;
					else if ( stricmp( szValue, "duel" ) == 0 )
						pInfo->lGameMode = GAMETYPE_DUEL;
					else if ( stricmp( szValue, "terminator" ) == 0 )
						pInfo->lGameMode = GAMETYPE_TERMINATOR;
					else if ( stricmp( szValue, "lastmanstanding" ) == 0 )
						pInfo->lGameMode = GAMETYPE_LASTMANSTANDING;
					else if ( stricmp( szValue, "teamlms" ) == 0 )
						pInfo->lGameMode = GAMETYPE_TEAMLMS;
					else if ( stricmp( szValue, "possession" ) == 0 )
						pInfo->lGameMode = GAMETYPE_POSSESSION;
					else if ( stricmp( szValue, "teampossession" ) == 0 )
						pInfo->lGameMode = GAMETYPE_TEAMPOSSESSION;
					else if ( stricmp( szValue, "teamgame" ) == 0 )
						pInfo->lGameMode = GAMETYPE_TEAMGAME;
					else if ( stricmp( szValue, "ctf" ) == 0 )
						pInfo->lGameMode = GAMETYPE_CTF;
					else if ( stricmp( szValue, "oneflagctf" ) == 0 )
						pInfo->lGameMode = GAMETYPE_ONEFLAGCTF;
					else if ( stricmp( szValue, "skulltag" ) == 0 )
						pInfo->lGameMode = GAMETYPE_SKULLTAG;
					else
						I_Error( "CAMPAIGN_ParseCampaignInfo: Unknown gamemode type, \"%s\"!", szValue );
				}
				else if ( stricmp( szKey, "dmflags" ) == 0 )
				{
					pInfo->lDMFlags = atoi( szValue );
				}
				else if ( stricmp( szKey, "dmflags2" ) == 0 )
				{
					pInfo->lDMFlags2 = atoi( szValue );
				}
				else if ( stricmp( szKey, "compatflags" ) == 0 )
				{
					pInfo->lCompatFlags = atoi( szValue );
				}
				else if ( stricmp( szKey, "playerteam" ) == 0 )
				{
					sprintf( pInfo->szPlayerTeam, szValue );
				}
				else if ( stricmp( szKey, "mustwinallduels" ) == 0 )
				{
					if ( stricmp( szValue, "true" ) == 0 )
						pInfo->bMustWinAllDuels = true;
					else if ( stricmp( szValue, "false" ) == 0 )
						pInfo->bMustWinAllDuels = false;
					else
						pInfo->bMustWinAllDuels = !!atoi( szValue );
				}
				else if ( stricmp( szKey, "possessionholdtime" ) == 0 )
				{
					pInfo->lPossessionHoldTime = atoi( szValue );
				}
				else if ( strnicmp( szKey, "botteam", strlen( "botteam" )) == 0 )
				{
					char	*pszString;
					char	szIndex[16];
					char	*pszIndex;
					LONG	lBotIndex;

					pszString = szKey + strlen( "botteam" );
					if ( *pszString != '[' )
						I_Error( "CAMPAIGN_ParseCampaignInfo: Expected '[' after \"botteam\"!" );
					pszString++;

					pszIndex = szIndex;
					while ( *pszString != ']' )
					{
						if ( *pszString == 0 )
							I_Error( "CAMPAIGN_ParseCampaignInfo: Missing ']' after \"botteam\"!" );

						*pszIndex = *pszString;
						pszIndex++;
						pszString++;
					}

					lBotIndex = atoi( szIndex );
					if (( lBotIndex < 0 ) || ( lBotIndex >= MAXPLAYERS ))
						I_Error( "CAMPAIGN_ParseCampaignInfo: Invalid \"botteam\" index, %d!", lBotIndex );

					sprintf( pInfo->BotSpawn[lBotIndex].szBotTeam, szValue );
				}
				else if ( strnicmp( szKey, "bot", strlen( "bot" )) == 0 )
				{
					char	*pszString;
					char	szIndex[16];
					char	*pszIndex;
					LONG	lBotIndex;
					int	iIndex = 0;

					pszString = szKey + strlen( "bot" );
					if ( *pszString != '[' )
						I_Error( "CAMPAIGN_ParseCampaignInfo: Expected '[' after \"bot\"!" );
					pszString++;

					pszIndex = szIndex;
					while ( *pszString != ']' )
					{
						if ( *pszString == 0 )
							I_Error( "CAMPAIGN_ParseCampaignInfo: Missing ']' after \"bot\"!" );

						if ( iIndex == 15 )
							I_Error( "CAMPAIGN_ParseCampaignInfo: Too many chars after \"bot\" and before ']'!" );

						*pszIndex = *pszString;
						pszIndex++;
						pszString++;
						iIndex++;
					}
					// [BB] We need to terminate szIndex. 
					*pszIndex = '\0';

					lBotIndex = atoi( szIndex );
					if (( lBotIndex < 0 ) || ( lBotIndex >= MAXPLAYERS ))
						I_Error( "CAMPAIGN_ParseCampaignInfo: Invalid \"bot\" index, %d!", lBotIndex );

					sprintf( pInfo->BotSpawn[lBotIndex].szBotName, szValue );
				}
				else
					I_Error( "CAMPAIGN_ParseCampaignInfo: Unknown CMPGNINF property, \"%s\"!", szKey );
			}
		}
	}
}

//*****************************************************************************
//
CAMPAIGNINFO_t *CAMPAIGN_GetCampaignInfo( char *pszMapName )
{
	CAMPAIGNINFO_t	*pInfo;
	CAMPAIGNINFO_t	*pBestInfo;

	pBestInfo = NULL;
	pInfo = g_pCampaignInfoRoot;
	while ( pInfo != NULL )
	{
		if ( stricmp( pszMapName, pInfo->szMapName ) == 0 )
			pBestInfo = pInfo;

		pInfo = pInfo->pNextInfo;
	}

	return ( pBestInfo );
}

//*****************************************************************************
//
void CAMPAIGN_EnableCampaign( void )
{
	g_bDisableCampaign = false;
}

//*****************************************************************************
//
void CAMPAIGN_DisableCampaign( void )
{
	g_bDisableCampaign = true;
}

//*****************************************************************************
//
bool CAMPAIGN_AllowCampaign( void )
{
	return (( g_bDisableCampaign == false ) &&
		( NETWORK_GetState( ) != NETSTATE_SERVER ) &&
		( NETWORK_GetState( ) != NETSTATE_CLIENT ) &&
		( CLIENTDEMO_IsPlaying( ) == false ));
}

//*****************************************************************************
//
bool CAMPAIGN_InCampaign( void )
{
	return (( CAMPAIGN_AllowCampaign( )) && g_bInCampaign );
}

//*****************************************************************************
//
void CAMPAIGN_SetInCampaign( bool bInCampaign )
{
	g_bInCampaign = bInCampaign;
}

//*****************************************************************************
//
bool CAMPAIGN_DidPlayerBeatMap( void )
{
	// Preliminary check.
	if ( teamplay || teampossession || teamlms || teamgame )
	{
		// If the console player isn't on a team, he DEFINITELY lost.
		if ( players[consoleplayer].bOnTeam == false )
			return ( false );
	}

	// If this is teamplay, compare the fragcount of the two teams.
	if ( teamplay )
	{
		if ( TEAM_GetFragCount( players[consoleplayer].ulTeam ) < TEAM_GetFragCount( !players[consoleplayer].ulTeam ))
			return ( false );
	}

	// If this is a teamgame or team possession, compare the team scores.
	if ( teamgame || teampossession )
	{
		if ( TEAM_GetScore( players[consoleplayer].ulTeam ) < TEAM_GetScore( !players[consoleplayer].ulTeam ))
			return ( false );
	}

	// If this is teamlms, compare the team wins.
	if ( teamlms )
	{
		if ( TEAM_GetWinCount( players[consoleplayer].ulTeam ) < TEAM_GetWinCount( !players[consoleplayer].ulTeam ))
			return ( false );
	}

	// If it's a deathmatch, check the player's spread.
	if (( deathmatch ) && ( teampossession == false ) && ( teamlms == false ) && ( teamplay == false ))
	{
		if ( SCOREBOARD_CalcSpread( consoleplayer ) < 0 )
			return ( false );
	}

	// Passed all checks!
	return ( true );
}
