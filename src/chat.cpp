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
#include "cl_demo.h"
#include "cl_main.h"
#include "chat.h"
#include "doomstat.h"
#include "d_gui.h"
#include "d_net.h"
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
#include "sbar.h"
#include "st_hud.h"
#include "sectinfo.h"
#include "g_level.h"
#include "p_acs.h"

//*****************************************************************************
//	VARIABLES

static	ULONG	g_ulChatMode;
static	char	g_szChatBuffer[MAX_CHATBUFFER_LENGTH];
static	LONG	g_lStringLength;
static	const char	*g_pszChatPrompt = "SAY: ";

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

// [CW]
CVAR( Bool, chat_substitution, false, CVAR_ARCHIVE )

EXTERN_CVAR( Int, con_colorinmessages );
EXTERN_CVAR( Int, chat_sound );

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
void	chat_GetIgnoredPlayers( FString &Destination ); // [RC]
void	chat_DoSubstitution( FString &Input ); // [CW]

// [BB] From ZDoom
//===========================================================================
//
// CT_PasteChat
//
//===========================================================================

void CT_PasteChat(const char *clip)
{
	if (clip != NULL && *clip != '\0')
	{
		// Only paste the first line.
		while (*clip != '\0')
		{
			if (*clip == '\n' || *clip == '\r' || *clip == '\b')
			{
				break;
			}
			chat_AddChar (*clip++);
		}
	}
}

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
void CHAT_Tick( void )
{
	// Check the chat ignore timers.
	for ( ULONG i = 0; i < MAXPLAYERS; i++ )
	{
		// [BB] Nothing to do for players that are not in the game.
		if ( playeringame[i] == false )
			continue;

		// Decrement this player's timer.
		if ( players[i].bIgnoreChat && ( players[i].lIgnoreChatTicks > 0 ))
			players[i].lIgnoreChatTicks--;

		// Is it time to un-ignore him?
		if ( players[i].lIgnoreChatTicks == 0 )
		{
			players[i].bIgnoreChat = false;
			// [BB] The player is unignored indefinitely. If we wouldn't do this,
			// bIgnoreChat would be set to false every tic once lIgnoreChatTicks reaches 0.
			players[i].lIgnoreChatTicks = -1;
		}
	}
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
				I_PutInClipboard(g_szChatBuffer );
				return ( true );
			}
			// Ctrl+V.
			else if ( pEvent->data1 == 'V' && ( pEvent->data3 & GKM_CTRL ))
			{
				CT_PasteChat(I_GetFromClipboard(false));
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
#ifdef unix
		else if (pEvent->subtype == EV_GUI_MButtonDown)
		{
			CT_PasteChat(I_GetFromClipboard(true));
		}
#endif
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
	
	lX = static_cast<LONG>(SmallFont->GetCharWidth( '_' ) * fXScale * 2 + SmallFont->StringWidth( g_pszChatPrompt ));

	// figure out if the text is wider than the screen->
	// if so, only draw the right-most portion of it.
	for ( lIdx = g_lStringLength - 1; (( lIdx >= 0 ) && ( lX < ( (float)SCREENWIDTH * fXScale ))); lIdx-- )
	{
		lX += SmallFont->GetCharWidth( g_szChatBuffer[lIdx] & 0x7f );
	}

	// If lIdx is >= 0, then this chat string goes beyond the edge of the screen.
	if ( lIdx >= 0 )
		lIdx++;
	else
		lIdx = 0;

	// Temporarily h4x0r the chat buffer string to include the cursor.
	g_szChatBuffer[g_lStringLength] = gameinfo.gametype == GAME_Doom ? '_' : '[';
	g_szChatBuffer[g_lStringLength+1] = 0;
	if ( g_ulChatMode == CHATMODE_GLOBAL )
	{
		HUD_DrawText( SmallFont, CR_GREEN,
			0,
			(LONG)( ulYPos * fYScale ),
			g_pszChatPrompt );

		HUD_DrawText( SmallFont, CR_GRAY,
			SmallFont->StringWidth( g_pszChatPrompt ),
			(LONG)( ulYPos * fYScale ),
			g_szChatBuffer + lIdx );
	}
	else
	{
		HUD_DrawText( SmallFont, CR_GREY,
			0,
			(LONG)( ulYPos * fYScale ),
			g_pszChatPrompt );

		HUD_DrawText( SmallFont, (TEAM_GetTextColor (players[consoleplayer].ulTeam)),
			SmallFont->StringWidth( g_pszChatPrompt ),
			(LONG)( ulYPos * fYScale ),
			g_szChatBuffer + lIdx );
	}

	// [RC] Tell chatters about the iron curtain of LMS chat.
	if ( GAMEMODE_AreSpectatorsFordiddenToChatToPlayers() )
	{
		// Is this the spectator talking?
		if ( players[consoleplayer].bSpectating )
			sprintf( szString, "\\cdNOTE: \\ccPlayers cannot hear you chat" );
		else
			sprintf( szString, "\\cdNOTE: \\ccSpectators cannot talk to you" );

		V_ColorizeString( szString );
		HUD_DrawText( SmallFont, CR_UNTRANSLATED,
			(LONG)(( ( bScale ? ValWidth.Int : SCREENWIDTH )/ 2 ) - ( SmallFont->StringWidth( szString ) / 2 )),
			(LONG)(( ulYPos * fYScale ) - ( SmallFont->GetHeight( ) * 2 ) + 1 ),
			szString );
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
void CHAT_PrintChatString( ULONG ulPlayer, ULONG ulMode, const char *pszString )
{
	ULONG		ulChatLevel = 0;
	FString		OutString;
	FString		ChatString;

	// [RC] Are we ignoring this player?
	if (( ulPlayer != MAXPLAYERS ) && players[ulPlayer].bIgnoreChat )
		return;

	// If ulPlayer == MAXPLAYERS, it is the server talking.
	if ( ulPlayer == MAXPLAYERS )
	{
		// Special support for "/me" commands.
		ulChatLevel = PRINT_HIGH;
		if ( strnicmp( "/me", pszString, 3 ) == 0 )
		{
			pszString += 3;
			OutString = "* <server>";
		}
		else
			OutString = "<server>: ";
	}
	else if ( ulMode == CHATMODE_GLOBAL )
	{
		ulChatLevel = PRINT_CHAT;

		// Special support for "/me" commands.
		if ( strnicmp( "/me", pszString, 3 ) == 0 )
		{
			ulChatLevel = PRINT_HIGH;
			pszString += 3;
			OutString.AppendFormat( "* %s\\cc", players[ulPlayer].userinfo.netname );
		}
		else
		{
			OutString.AppendFormat( "%s" TEXTCOLOR_CHAT ": ", players[ulPlayer].userinfo.netname );
		}
	}
	else if ( ulMode == CHATMODE_TEAM )
	{
		ulChatLevel = PRINT_TEAMCHAT;
		if ( PLAYER_IsTrueSpectator ( &players[consoleplayer] ) )
			OutString += "<SPEC> ";
		else
		{
			OutString = "\\c";
			OutString += V_GetColorChar( TEAM_GetTextColor( players[consoleplayer].ulTeam ));
			OutString += "<TEAM> ";
		}

		// Special support for "/me" commands.
		if ( strnicmp( "/me", pszString, 3 ) == 0 )
		{
			ulChatLevel = PRINT_HIGH;
			pszString += 3;
			OutString.AppendFormat( "\\cc* %s\\cc", players[ulPlayer].userinfo.netname );
		}
		else
		{
			OutString.AppendFormat( "\\cd%s" TEXTCOLOR_TEAMCHAT ": ", players[ulPlayer].userinfo.netname );
		}
	}

	ChatString = pszString;

	// [RC] Remove linebreaks and other escape codes from chat.
	ChatString.Substitute("\\", "\\\\");

	// [RC] ...but allow chat colors.
	ChatString.Substitute("\\\\c", "\\c");

	// [BB] Remove invalid color codes, those can confuse the printing and create new lines.
	V_RemoveInvalidColorCodes( ChatString );

	// [RC] ...if the user wants them.
	if ( con_colorinmessages == 2)
		V_RemoveColorCodes( ChatString );

	// [BB] Remove any kind of trailing crap.
	V_RemoveTrailingCrapFromFString ( ChatString );

	// [BB] If the chat string is empty now, it only contained crap and is ignored.
	if ( ChatString.IsEmpty() )
		return;

	OutString += ChatString;

	Printf( ulChatLevel, "%s\n", OutString.GetChars() );

	// [BB] If the user doesn't want to see the messages, they shouldn't make a sound.
	if ( show_messages )
	{
		// [RC] User can choose the chat sound.
		if ( chat_sound == 1 ) // Default
			S_Sound( CHAN_VOICE, gameinfo.chatSound, 1, ATTN_NONE );
		else if ( chat_sound == 2 ) // Doom 1
			S_Sound( CHAN_VOICE, "misc/chat2", 1, ATTN_NONE );
		else if ( chat_sound == 3 ) // Doom 2
			S_Sound( CHAN_VOICE, "misc/chat", 1, ATTN_NONE );
	}

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
		player_t	*pPlayer = &players[consoleplayer];

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
	FString ChatMessage = pszString;

	// [CW] Substitute the message if necessary.
	chat_DoSubstitution( ChatMessage );

	// If we're the client, let the server handle formatting/sending the msg to other players.
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
	{
		CLIENTCOMMANDS_Say( ulMode, ChatMessage.GetChars( ));
		return;
	}

	if ( demorecording )
	{
		Net_WriteByte( DEM_SAY );
		Net_WriteByte( ulMode );
		Net_WriteString( ChatMessage.GetChars( ));
	}
	else
		CHAT_PrintChatString( consoleplayer, ulMode, ChatMessage.GetChars( ));
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
//
// [RC] Fills Destination with a list of ignored players.
//
void chat_GetIgnoredPlayers( FString &Destination )
{
	Destination = "";

	// Append all the players' names.
	for ( ULONG i = 0; i < MAXPLAYERS; i++ )
	{
		if ( players[i].bIgnoreChat )
		{
			Destination += players[i].userinfo.netname;
			Destination += "\\c-";
			
			// Add the time remaining.
			if ( players[i].lIgnoreChatTicks > 0 )
			{
				int iMinutesLeft = static_cast<int>( 1 + players[i].lIgnoreChatTicks / ( MINUTE * TICRATE ));
				Destination.AppendFormat( " (%d minute%s left)", iMinutesLeft, ( iMinutesLeft == 1 ? "" : "s" ));
			}

			Destination += ", ";
		}
	}

	// Remove the last ", ".
	if ( Destination.Len( ) )
		Destination = Destination.Left( Destination.Len( ) - 2 );
}

//*****************************************************************************
//
// [CW]
void chat_DoSubstitution( FString &Input )
{
	player_t *pPlayer = &players[consoleplayer];
	AWeapon *pReadyWeapon = pPlayer->ReadyWeapon;

	if ( chat_substitution )
	{
		FString Output;
		const char *pszString = Input.GetChars( );

		for ( ; *pszString != 0; pszString++ )
		{
			if ( !strncmp( pszString, "$ammocount", 10 ))
			{
				if ( pReadyWeapon && pReadyWeapon->Ammo1 )
				{
					Output.AppendFormat( "%d", pReadyWeapon->Ammo1->Amount );

					if ( pReadyWeapon->Ammo2 )
						Output.AppendFormat( "/%d", pReadyWeapon->Ammo2->Amount );
				}
				else
				{
					Output.AppendFormat( "no ammo" );
				}

				pszString += 9;
			}
			else if ( !strncmp( pszString, "$ammo", 5 ))
			{
				if ( pReadyWeapon && pReadyWeapon->Ammo1 )
				{
					Output.AppendFormat( "%s", pReadyWeapon->Ammo1->GetClass( )->TypeName.GetChars( ) );

					if ( pReadyWeapon->Ammo2 )
					{
						Output.AppendFormat( "/%s", pReadyWeapon->Ammo2->GetClass( )->TypeName.GetChars( ));
					}
				}
				else
				{
					Output.AppendFormat( "no ammo" );
				}

				pszString += 4;
			}
			else if ( !strncmp( pszString, "$armor", 6 ))
			{
				AInventory *pArmor = pPlayer->mo->FindInventory<ABasicArmor>( );
				int iArmorCount = 0;
				
				if ( pArmor )
					iArmorCount = pArmor->Amount;

				Output.AppendFormat( "%d", iArmorCount );

				pszString += 5;
			}
			else if ( !strncmp( pszString, "$health", 7 ))
			{
				Output.AppendFormat ("%d", pPlayer->health);

				pszString += 6;
			}
			else if ( !strncmp( pszString, "$weapon", 7 ))
			{
				if ( pReadyWeapon )
					Output.AppendFormat( "%s", pReadyWeapon->GetClass( )->TypeName.GetChars( ) );
				else
					Output.AppendFormat( "no weapon" );

				pszString += 6;
			}
			else if ( !strncmp( pszString, "$location", 9 ))
			{
				Output += SECTINFO_GetPlayerLocation( consoleplayer );
				pszString += 8;
			}
			else
			{
				Output.AppendCStrPart( pszString, 1 );
			}
		}

		Input = Output;
	}
}

//*****************************************************************************
//	CONSOLE COMMANDS/VARIABLES

CCMD( say )
{
	ULONG		ulIdx;
	FString		ChatString;

	// [BB] Mods are not allowed to say anything in the player's name.
	if ( ACS_IsCalledFromConsoleCommand( ))
		return;

	// [BB] No chatting while playing a demo.
	if ( CLIENTDEMO_IsPlaying( ) == true )
	{
		Printf ( "You can't chat during demo playback.\n" );
		return;
	}

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
		for ( ulIdx = 1; ulIdx < static_cast<unsigned int>(argv.argc( )); ulIdx++ )
			ChatString.AppendFormat( "%s ", argv[ulIdx] );

		// Send the server's chat string out to clients, and print it in the console.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVER_SendChatMessage( MAXPLAYERS, CHATMODE_GLOBAL, ChatString.GetChars( ));
		else
			// We typed out our message in the console or with a macro. Go ahead and send the message now.
			chat_SendMessage( CHATMODE_GLOBAL, ChatString.GetChars( ));
	}
}

//*****************************************************************************
//
CCMD( say_team )
{
	ULONG		ulIdx;
	FString		ChatString;

	// [BB] Mods are not allowed to say anything in the player's name.
	if ( ACS_IsCalledFromConsoleCommand( ))
		return;

	// [BB] No chatting while playing a demo.
	if ( CLIENTDEMO_IsPlaying( ) == true )
	{
		Printf ( "You can't chat during demo playback.\n" );
		return;
	}

	// Make sure we have teammates to talk to before we use team chat.
	if ( GAMEMODE_GetFlags( GAMEMODE_GetCurrentMode( )) & GMF_PLAYERSONTEAMS )
	{
		// Not on a team. No one to talk to.
		if ( ( players[consoleplayer].bOnTeam == false ) && ( PLAYER_IsTrueSpectator ( &players[consoleplayer] ) == false ) )
			return;
	}
	// Not in any team mode. Nothing to do!
	else
		return;

	// [BB] Don't allow dead spectators to team chat.
	if ( players[consoleplayer].bDeadSpectator )
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
		for ( ulIdx = 1; ulIdx < static_cast<unsigned int>(argv.argc( )); ulIdx++ )
			ChatString.AppendFormat( "%s ", argv[ulIdx] );

		// We typed out our message in the console or with a macro. Go ahead and send the message now.
		chat_SendMessage( CHATMODE_TEAM, ChatString.GetChars( ));
	}
}

//*****************************************************************************
//
// [RC] Lets clients ignore an annoying player's chat messages.
//
void chat_IgnorePlayer( FCommandLine &argv, const ULONG ulPlayer )
{
	// Print the explanation message.
	if ( argv.argc( ) < 2 )
	{
		// Create a list of currently ignored players.
		FString PlayersIgnored;
		chat_GetIgnoredPlayers( PlayersIgnored );

		if ( PlayersIgnored.Len( ))
			Printf( "\\cgIgnored players: \\c-%s\nUse \"unignore\" or \"unignore_idx\" to undo.\n", PlayersIgnored.GetChars() );
		else
			Printf( "Ignores a certain player's chat messages.\nUsage: ignore <name> [duration, in minutes]\n" );

		return;
	}
	
	LONG	lTicks = -1;
	const LONG lArgv2 = ( argv.argc( ) >= 3 ) ? atoi( argv[2] ) : -1;
	
	// Did the user specify a set duration?
	if ( ( lArgv2 > 0 ) && ( lArgv2 < LONG_MAX / ( TICRATE * MINUTE )))
		lTicks = lArgv2 * TICRATE * MINUTE;

	if ( ulPlayer == MAXPLAYERS )
		Printf( "There isn't a player named %s\\c-.\n", argv[1] );
	else if ( ( ulPlayer == (ULONG)consoleplayer ) && ( NETWORK_GetState( ) != NETSTATE_SERVER ) )
		Printf( "You can't ignore yourself.\n" );
	else if ( players[ulPlayer].bIgnoreChat && ( players[ulPlayer].lIgnoreChatTicks == lTicks ))
		Printf( "You're already ignoring %s\\c-.\n", players[ulPlayer].userinfo.netname );
	else
	{
		players[ulPlayer].bIgnoreChat = true;
		players[ulPlayer].lIgnoreChatTicks = lTicks;
		Printf( "%s\\c- will now be ignored", players[ulPlayer].userinfo.netname );
		if ( lTicks > 0 )
			Printf( ", for %d minutes", static_cast<int>(lArgv2));
		Printf( ".\n" );

		// Add a helpful note about bots.
		if ( players[ulPlayer].bIsBot )
			Printf( "Note: you can disable all bot chat by setting the CVAR bot_allowchat to false.\n" );

		// Notify the server so that others using this IP are also ignored.
		if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
			CLIENTCOMMANDS_Ignore( ulPlayer, true, lTicks );
	}
}

CCMD( ignore )
{
	// Find the player and ignore him.
	chat_IgnorePlayer( argv, argv.argc( ) >= 2 ? SERVER_GetPlayerIndexFromName( argv[1], true, true ) : MAXPLAYERS );
}

CCMD( ignore_idx )
{
	const ULONG ulPlayer = ( argv.argc( ) >= 2 ) ? atoi( argv[1] ) : MAXPLAYERS;

	if ( PLAYER_IsValidPlayer( ulPlayer ) == false ) 
		return;

	chat_IgnorePlayer( argv, ulPlayer );
}

//*****************************************************************************
//
// [RC] Undos "ignore".
//
void chat_UnignorePlayer( FCommandLine &argv, const ULONG ulPlayer )
{
	// Print the explanation message.
	if ( argv.argc( ) < 2 )
	{
		// Create a list of currently ignored players.
		FString PlayersIgnored = "";
		chat_GetIgnoredPlayers( PlayersIgnored );

		if ( PlayersIgnored.Len( ))
			Printf( "\\cgIgnored players: \\c-%s\n", PlayersIgnored.GetChars() );
		else
			Printf( "Un-ignores a certain player's chat messages.\nUsage: unignore <name>\n" );

		return;
	}
	
	if ( ulPlayer == MAXPLAYERS )
		Printf( "There isn't a player named %s\\c-.\n", argv[1] );
	else if ( ( ulPlayer == (ULONG)consoleplayer ) && ( NETWORK_GetState( ) != NETSTATE_SERVER ) )
		Printf( "You can't unignore yourself.\n" );
	else if ( !players[ulPlayer].bIgnoreChat )
		Printf( "You're not ignoring %s\\c-.\n", players[ulPlayer].userinfo.netname );
	else 
	{
		players[ulPlayer].bIgnoreChat = false;
		players[ulPlayer].lIgnoreChatTicks = -1;
		Printf( "%s\\c- will no longer be ignored.\n", players[ulPlayer].userinfo.netname );

		// Notify the server so that others using this IP are also ignored.
		if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
			CLIENTCOMMANDS_Ignore( ulPlayer, false );
	}
}

CCMD( unignore )
{
	chat_UnignorePlayer( argv, argv.argc( ) >= 2 ? SERVER_GetPlayerIndexFromName( argv[1], true, true ) : MAXPLAYERS );
}

CCMD( unignore_idx )
{
	const ULONG ulPlayer = ( argv.argc( ) >= 2 ) ? atoi( argv[1] ) : MAXPLAYERS;

	if ( PLAYER_IsValidPlayer( ulPlayer ) == false ) 
		return;

	chat_UnignorePlayer( argv, ulPlayer );
}

