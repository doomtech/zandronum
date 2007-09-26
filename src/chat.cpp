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
// Filename: chat.cpp
//
// Description: Contains chat routines
//
//-----------------------------------------------------------------------------

#include "botcommands.h"
#include "c_console.h"
#include "c_dispatch.h"
#include "cl_commands.h"
#include "cl_main.h"
#include "chat.h"
#include "d_gui.h"
#include "deathmatch.h"
#include "gi.h"
#include "gamemode.h"
#include "hu_stuff.h"
#include "i_input.h"
#include "d_gui.h"
#include "d_player.h"
#include "network.h"
#include "s_sound.h"
#include "scoreboard.h"
#include "st_stuff.h"
//#include "sv_main.h"
#include "team.h"
#include "v_text.h"
#include "v_video.h"
#include "w_wad.h"
#include "lastmanstanding.h"
#include "sbar.h"

//*****************************************************************************
//	VARIABLES

static	ULONG	g_ulChatMode;
static	char	g_szChatBuffer[MAX_CHATBUFFER_LENGTH];
static	LONG	g_lStringLength;
static	char	*g_pszChatPrompt = "SAY: ";

//*****************************************************************************
//	CONSOLE VARIABLES

CVAR( String, chatmacro1, "I'm ready to kick butt!", CVAR_ARCHIVE )
CVAR( String, chatmacro2, "I'm OK.", CVAR_ARCHIVE )
CVAR( String, chatmacro3, "I'm not looking too good!", CVAR_ARCHIVE )
CVAR( String, chatmacro4, "Help!", CVAR_ARCHIVE )
CVAR( String, chatmacro5, "You suck!", CVAR_ARCHIVE )
CVAR( String, chatmacro6, "Next time, scumbag...", CVAR_ARCHIVE )
CVAR( String, chatmacro7, "Come here!", CVAR_ARCHIVE )
CVAR( String, chatmacro8, "I'll take care of it.", CVAR_ARCHIVE )
CVAR( String, chatmacro9, "Yes", CVAR_ARCHIVE )
CVAR( String, chatmacro0, "No", CVAR_ARCHIVE )

//*****************************************************************************
FStringCVar	*g_ChatMacros[10] =
{
	&chatmacro0,
	&chatmacro1,
	&chatmacro2,
	&chatmacro3,
	&chatmacro4,
	&chatmacro5,
	&chatmacro6,
	&chatmacro7,
	&chatmacro8,
	&chatmacro9
};

//*****************************************************************************
//	PROTOTYPES

void	chat_SetChatMode( ULONG ulMode );
void	chat_SendMessage( ULONG ulMode, const char *pszString );
void	chat_ClearChatMessage( void );
void	chat_AddChar( char cChar );
void	chat_DeleteChar( void );

//*****************************************************************************
//	FUNCTIONS

void CHAT_Construct( void )
{
	// Initialize the chat mode.
	g_ulChatMode = CHATMODE_NONE;

	// Clear out the chat buffer.
	chat_ClearChatMessage( );
}

//*****************************************************************************
//
bool CHAT_Input( event_t *pEvent )
{
	if ( pEvent->type != EV_GUI_Event )
		return ( false );

	// Server doesn't use this at all.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		return ( false );

	// Determine to do with our keypress.
	if ( g_ulChatMode != CHATMODE_NONE )
	{
		if ( pEvent->subtype == EV_GUI_KeyDown || pEvent->subtype == EV_GUI_KeyRepeat )
		{
			if ( pEvent->data1 == '\r' )
			{
				chat_SendMessage( g_ulChatMode, g_szChatBuffer );
				chat_SetChatMode( CHATMODE_NONE );
				return ( true );
			}
			else if ( pEvent->data1 == GK_ESCAPE )
			{
				chat_SetChatMode( CHATMODE_NONE );
				return ( true );
			}
			else if ( pEvent->data1 == '\b' )
			{
				chat_DeleteChar( );
				return ( true );
			}
			// Ctrl+C. 
			else if ( pEvent->data1 == 'C' && ( pEvent->data3 & GKM_CTRL ))
			{
				I_PutInClipboard((char *)g_szChatBuffer );
				return ( true );
			}
			// Ctrl+V.
			else if ( pEvent->data1 == 'V' && ( pEvent->data3 & GKM_CTRL ))
			{
				char *clip = I_GetFromClipboard ();
				if (clip != NULL)
				{
					char *clip_p = clip;
					strtok (clip, "\n\r\b");
					while (*clip_p)
					{
						chat_AddChar( *clip_p++ );
					}
					delete[] clip;
				}
			}
		}
		else if ( pEvent->subtype == EV_GUI_Char )
		{
			// Send a macro.
			if ( pEvent->data2 && (( pEvent->data1 >= '0' ) && ( pEvent->data1 <= '9' )))
			{
				chat_SendMessage( g_ulChatMode, *g_ChatMacros[pEvent->data1 - '0'] );
				chat_SetChatMode( CHATMODE_NONE );
			}
			else
				chat_AddChar( pEvent->data1 );

			return ( true );
		}
	}

	return ( false );
}

//*****************************************************************************
//
void CHAT_Render( void )
{
	UCVarValue	ValWidth;
	UCVarValue	ValHeight;
	bool		bScale;
	float		fXScale;
	float		fYScale;
	ULONG		ulYPos;
	LONG		lIdx;
	LONG		lX;
	char		szString[64];

	if ( g_ulChatMode == CHATMODE_NONE )
		return;

	ValWidth = con_virtualwidth.GetGenericRep( CVAR_Int );
	ValHeight = con_virtualheight.GetGenericRep( CVAR_Int );

	// Initialization.
	if (( con_scaletext ) && ( con_virtualwidth > 0 ) && ( con_virtualheight > 0 ))
	{
		fXScale = (float)ValWidth.Int / SCREENWIDTH;
		fYScale = (float)ValHeight.Int / SCREENHEIGHT;
		bScale = true;

		ulYPos = (( gamestate == GS_INTERMISSION ) ? SCREENHEIGHT : ST_Y ) - ( Scale( SCREENHEIGHT, SmallFont->GetHeight( ) + 1, ValHeight.Int )) + 1;

	}
	else
	{
		fXScale = 1.0f;
		fYScale = 1.0f;
		bScale = false;

		ulYPos = (( gamestate == GS_INTERMISSION ) ? SCREENHEIGHT : ST_Y ) - SmallFont->GetHeight( ) + 1;
	}
	
	// Use the small font.
	screen->SetFont( SmallFont );

	lX = SmallFont->GetCharWidth( '_' ) * fXScale * 2 + SmallFont->StringWidth( g_pszChatPrompt );

	// figure out if the text is wider than the screen->
	// if so, only draw the right-most portion of it.
	for ( lIdx = g_lStringLength - 1; (( lIdx >= 0 ) && ( lX < ( (float)SCREENWIDTH * fXScale ))); lIdx-- )
	{
		lX += screen->Font->GetCharWidth( g_szChatBuffer[lIdx] & 0x7f );
	}

	// If lIdx is >= 0, then this chat string goes beyond the edge of the screen.
	if ( lIdx >= 0 )
		lIdx++;
	else
		lIdx = 0;

	// Temporarily h4x0r the chat buffer string to include the cursor.
	g_szChatBuffer[g_lStringLength] = gameinfo.gametype == GAME_Doom ? '_' : '[';
	g_szChatBuffer[g_lStringLength+1] = 0;
	if ( bScale )
	{
		if ( g_ulChatMode == CHATMODE_GLOBAL )
		{
			screen->DrawText( CR_GREEN,
				0,
				(LONG)( ulYPos * fYScale ),
				g_pszChatPrompt,
				DTA_VirtualWidth, ValWidth.Int,
				DTA_VirtualHeight, ValHeight.Int,
				TAG_DONE );

			screen->DrawText( CR_GRAY,
				SmallFont->StringWidth( g_pszChatPrompt ),
				(LONG)( ulYPos * fYScale ),
				g_szChatBuffer + lIdx,
				DTA_VirtualWidth, ValWidth.Int,
				DTA_VirtualHeight, ValHeight.Int,
				TAG_DONE );
		}
		else
		{
			screen->DrawText( CR_GREY,
				0,
				(LONG)( ulYPos * fYScale ),
				g_pszChatPrompt,
				DTA_VirtualWidth, ValWidth.Int,
				DTA_VirtualHeight, ValHeight.Int,
				TAG_DONE );

			screen->DrawText(( players[consoleplayer].ulTeam == TEAM_BLUE ) ? CR_BLUE : CR_RED,
				SmallFont->StringWidth( g_pszChatPrompt ),
				(LONG)( ulYPos * fYScale ),
				g_szChatBuffer + lIdx,
				DTA_VirtualWidth, ValWidth.Int,
				DTA_VirtualHeight, ValHeight.Int,
				TAG_DONE );
		}
	}
	else
	{
		if ( g_ulChatMode == CHATMODE_GLOBAL )
		{
			screen->DrawText( CR_GREEN,
				0,
				ulYPos,
				g_pszChatPrompt,
				TAG_DONE );

			screen->DrawText( CR_GRAY,
				SmallFont->StringWidth( g_pszChatPrompt ),
				ulYPos,
				g_szChatBuffer + lIdx,
				TAG_DONE );
		}
		else
		{
			screen->DrawText( CR_GREY,
				0,
				ulYPos,
				g_pszChatPrompt,
				TAG_DONE );

			screen->DrawText(( players[consoleplayer].ulTeam == TEAM_BLUE ) ? CR_BLUE : CR_RED,
				SmallFont->StringWidth( g_pszChatPrompt ),
				ulYPos,
				g_szChatBuffer + lIdx,
				TAG_DONE );
		}
	}

	// [RC] Tell chatters about the iron curtain of LMS chat.
	if (( lastmanstanding || teamlms ) && 
		(( lmsspectatorsettings & LMS_SPF_CHAT ) == false ) &&
		( LASTMANSTANDING_GetState( ) == LMSS_INPROGRESS ))
	{
		// Is this the spectator talking?
		if ( players[consoleplayer].bSpectating )
		{
			sprintf( szString, "\\cdNOTE: \\ccPlayers cannot hear you chat" );

			V_ColorizeString( szString );
			if ( bScale )
			{
				screen->DrawText( CR_UNTRANSLATED,
					(LONG)(( ValWidth.Int / 2 ) - ( SmallFont->StringWidth( szString ) / 2 )),
					(LONG)(( ulYPos * fYScale ) - ( SmallFont->GetHeight( ) * 2 ) + 1 ),
					szString,
					DTA_VirtualWidth, ValWidth.Int,
					DTA_VirtualHeight, ValHeight.Int,
					TAG_DONE );
			}
			else
			{
				screen->DrawText( CR_UNTRANSLATED,
					(LONG)(( SCREENWIDTH / 2 ) - ( SmallFont->StringWidth( szString ) / 2 )),
					(LONG)(( ulYPos * fYScale ) - ( SmallFont->GetHeight( ) * 2 ) + 1 ),
					szString,
					TAG_DONE );
			}
		}
		else
		{
			sprintf( szString, "\\cdNOTE: \\ccSpectators cannot talk to you" );

			V_ColorizeString( szString );
			if ( bScale )
			{
				screen->DrawText( CR_UNTRANSLATED,
					(LONG)(( ValWidth.Int / 2 ) - ( SmallFont->StringWidth( szString ) / 2 )),
					(LONG)(( ulYPos * fYScale ) - ( SmallFont->GetHeight( ) * 2 ) + 1 ),
					szString,
					DTA_VirtualWidth, ValWidth.Int,
					DTA_VirtualHeight, ValHeight.Int,
					TAG_DONE );
			}
			else
			{
				screen->DrawText( CR_UNTRANSLATED,
					(LONG)(( SCREENWIDTH / 2 ) - ( SmallFont->StringWidth( szString ) / 2 )),
					(LONG)(( ulYPos * fYScale ) - ( SmallFont->GetHeight( ) * 2 ) + 1 ),
					szString,
					TAG_DONE );
			}
		}
	}

	g_szChatBuffer[g_lStringLength] = 0;
	BorderTopRefresh = screen->GetPageCount( );
}

//*****************************************************************************
//
ULONG CHAT_GetChatMode( void )
{
	return ( g_ulChatMode );
}

//*****************************************************************************
//
void CHAT_PrintChatString( ULONG ulPlayer, ULONG ulMode, char *pszString )
{
	ULONG	ulChatLevel;
	char	szChatHeader[MAXPLAYERNAME+3];

	// If ulPlayer == MAXPLAYERS, it is the server talking.
	if ( ulPlayer == MAXPLAYERS )
	{
		// Special support for "/me" commands.
		if ( strnicmp( "/me", pszString, 3 ) == 0 )
		{
			ulChatLevel = PRINT_HIGH;
			pszString += 3;
			sprintf( szChatHeader, "* <server>" );
		}
		else
		{
			ulChatLevel = PRINT_HIGH;
			sprintf( szChatHeader, "<server>: " );
		}
	}
	else if ( ulMode == CHATMODE_GLOBAL )
	{
		// Special support for "/me" commands.
		if ( strnicmp( "/me", pszString, 3 ) == 0 )
		{
			ulChatLevel = PRINT_HIGH;
			pszString += 3;
			sprintf( szChatHeader, "* %s\\cc", players[ulPlayer].userinfo.netname );
		}
		else
		{
			ulChatLevel = PRINT_CHAT;
			sprintf( szChatHeader, "%s\\cd: ", players[ulPlayer].userinfo.netname );
		}
	}
	else if ( ulMode == CHATMODE_TEAM )
	{
		ulChatLevel = PRINT_CHAT;

		if ( players[consoleplayer].ulTeam == TEAM_BLUE )
			sprintf( szChatHeader, "\\cH<TEAM> " );
		else
			sprintf( szChatHeader, "\\cG<TEAM> " );

		// Special support for "/me" commands.
		if ( strnicmp( "/me", pszString, 3 ) == 0 )
		{
			ulChatLevel = PRINT_HIGH;
			pszString += 3;
			sprintf( szChatHeader, "%s\\cc* %s\\cc", szChatHeader, players[ulPlayer].userinfo.netname );
		}
		else
		{
			ulChatLevel = PRINT_CHAT;
			sprintf( szChatHeader, "%s \\cd%s\\cd: ", szChatHeader, players[ulPlayer].userinfo.netname );
		}
	}

	Printf( ulChatLevel, "%s%s\n", szChatHeader, pszString );

	if ( show_messages )
		S_Sound( CHAN_VOICE, gameinfo.chatSound, 1, ATTN_NONE );

	BOTCMD_SetLastChatString( pszString );
	BOTCMD_SetLastChatPlayer( players[ulPlayer].userinfo.netname );

	{
		ULONG	ulIdx;

		for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
		{
			if ( playeringame[ulIdx] == false )
				continue;

			// Don't tell the bot someone talked if it was it who talked.
			if ( ulIdx == ulPlayer )
				continue;

			// If this is a bot, tell it a player said something.
			if ( players[ulIdx].pSkullBot )
				players[ulIdx].pSkullBot->PostEvent( BOTEVENT_PLAYER_SAY );
		}
	}
}

//*****************************************************************************
//*****************************************************************************
//
void chat_SetChatMode( ULONG ulMode )
{
	if ( ulMode < NUM_CHATMODES )
	{
		player_s	*pPlayer = &players[consoleplayer];

		g_ulChatMode = ulMode;

		if ( ulMode != CHATMODE_NONE )
		{
			pPlayer->bChatting = true;

			// Tell the server we're beginning to chat.
			if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
				CLIENTCOMMANDS_StartChat( );
		}
		else
		{
			pPlayer->bChatting = false;

			// Tell the server we're done chatting.
			if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
				CLIENTCOMMANDS_EndChat( );
		}

	}
}

//*****************************************************************************
//
void chat_SendMessage( ULONG ulMode, const char *pszString )
{
	// If we're the client, let the server handle formatting/sending the msg to other players.
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
	{
		CLIENTCOMMANDS_Say( ulMode, (char *)pszString );
		return;
	}

	if ( demorecording )
	{
		Net_WriteByte( DEM_SAY );
		Net_WriteByte( ulMode );
		Net_WriteString( (char *)pszString );
	}
	else
		CHAT_PrintChatString( consoleplayer, ulMode, (char *)pszString );
}

//*****************************************************************************
//
void chat_ClearChatMessage( void )
{
	ULONG	ulIdx;

	// Clear out the chat string buffer.
	for ( ulIdx = 0; ulIdx < MAX_CHATBUFFER_LENGTH; ulIdx++ )
		g_szChatBuffer[ulIdx] = 0;

	// String buffer is of zero length.
	g_lStringLength = 0;
}

//*****************************************************************************
//
void chat_AddChar( char cChar )
{
	if ( g_lStringLength >= ( MAX_CHATBUFFER_LENGTH - 2 ))
		return;

	g_szChatBuffer[g_lStringLength++] = cChar;
	g_szChatBuffer[g_lStringLength] = 0;
}

//*****************************************************************************
//
void chat_DeleteChar( void )
{
	if ( g_lStringLength )
		g_szChatBuffer[--g_lStringLength] = 0;
}

//*****************************************************************************
//	CONSOLE COMMANDS/VARIABLES

CCMD( say )
{
	ULONG	ulIdx;
	char	szChatString[256];

	if ( argv.argc( ) < 2 )
	{
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			return;

		// The message we send will be a global chat message to everyone.
		chat_SetChatMode( CHATMODE_GLOBAL );
		
		// Hide the console.
		C_HideConsole( );

		// Clear out the chat buffer.
		chat_ClearChatMessage( );
	}
	else
	{
		szChatString[0] = '\0';
		for ( ulIdx = 1; ulIdx < argv.argc( ); ulIdx++ )
		{
			if ( szChatString[0] == '\0' )
				sprintf( szChatString, "%s ", argv[ulIdx] );
			else
				sprintf( szChatString, "%s%s ", szChatString, argv[ulIdx] );
		}

		// Send the server's chat string out to clients, and print it in the console.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVER_SendChatMessage( MAXPLAYERS, CHATMODE_GLOBAL, szChatString );
		else
			// We typed out our message in the console or with a macro. Go ahead and send the message now.
			chat_SendMessage( CHATMODE_GLOBAL, szChatString );
	}
}

//*****************************************************************************
//
CCMD( say_team )
{
	ULONG	ulIdx;
	char	szChatString[128];

	// Make sure we have teammates to talk to before we use team chat.
	if ( GAMEMODE_GetFlags( GAMEMODE_GetCurrentMode( )) & GMF_PLAYERSONTEAMS )
	{
		// Not on a team. No one to talk to.
		if ( players[consoleplayer].bOnTeam == false )
			return;
	}
	// Not in any team mode. Nothing to do!
	else
		return;

	// Don't allow spectators to team chat.
	if ( players[consoleplayer].bSpectating )
		return;

	// The server never should have a team!
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		return;

	if ( argv.argc( ) < 2 )
	{
		// The message we send is a message to teammates only.
		chat_SetChatMode( CHATMODE_TEAM );
		
		// Hide the console.
		C_HideConsole( );

		// Clear out the chat buffer.
		chat_ClearChatMessage( );
	}
	else
	{
		szChatString[0] = '\0';
		for ( ulIdx = 1; ulIdx < argv.argc( ); ulIdx++ )
		{
			if ( szChatString[0] == '\0' )
				sprintf( szChatString, "%s ", argv[ulIdx] );
			else
				sprintf( szChatString, "%s%s ", szChatString, argv[ulIdx] );
		}

		// We typed out our message in the console or with a macro. Go ahead and send the message now.
		chat_SendMessage( CHATMODE_TEAM, szChatString );
	}
}
