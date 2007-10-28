//-----------------------------------------------------------------------------
//
// Skulltag Source
// Copyright (C) 2004 Brad Carney
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
// Created:  4/5/04
//
//
// Filename: serverconsole.cpp
//
// Description: Contains variables and routines related to the GUI server console
//
//-----------------------------------------------------------------------------

#include <time.h>
#include <windows.h>
#include <commctrl.h>
#define USE_WINDOWS_DWORD
#include "i_system.h"

#include "c_dispatch.h"
#include "cmdlib.h"
#include "cooperative.h"
#include "deathmatch.h"
#include "duel.h"
#include "lastmanstanding.h"
#include "maprotation.h"
#include "network.h"
#include "resource.h"
#include "serverconsole.h"
#include "sv_ban.h"
#include "sv_main.h"
#include "team.h"
#include "v_text.h"
#include "version.h"
#include "network.h"

#ifdef	GUI_SERVER_CONSOLE

// YUCK
DWORD WINAPI MainDoomThread( LPVOID );

//*****************************************************************************
//	VARIABLES

// Global handle for the server console dialog box.
static	HWND				g_hDlg = NULL;
static	HWND				g_hDlgStatusBar = NULL;

// Global handle for the server statistic dialog box.
static	HWND				g_hStatisticDlg = NULL;

// Thread handle for D_DoomMain.
HANDLE						g_hThread;

// Number of lines present in the console box.
static	ULONG				g_ulNumLines = 0;
static	bool				g_bScrollConsoleOnNewline = false;

// Stored values of cvars (only send updates them if they change!)
static	ULONG				g_ulStoredDMFlags;
static	ULONG				g_ulStoredDMFlags2;
static	ULONG				g_ulStoredCompatflags;
static	ULONG				g_ulStoredLMSAllowedWeapons;
static	ULONG				g_ulStoredLMSSpectatorSettings;
static	ULONG				g_ulStoredFraglimit;
static	ULONG				g_ulStoredTimelimit;
static	ULONG				g_ulStoredPointlimit;
static	ULONG				g_ulStoredDuellimit;
static	ULONG				g_ulStoredWinlimit;

// Array of player indicies for the scoreboard.
static	LONG				g_lPlayerIndicies[MAXPLAYERS];

static	char				g_szBanEditString[256];

static	char				g_szOperatingSystem[256];
static	char				g_szCPUSpeed[256];
static	char				g_szVendor[256];

static	NOTIFYICONDATA		g_NotifyIconData;

static	HICON				g_hSmallIcon = NULL;

static	bool				g_bSmallIconClicked = false;

//*****************************************************************************
//	GLOBAL VARIABLES

extern	HINSTANCE	g_hInst;
extern	FILE		*Logfile;
extern	char		g_szLogFilename[256];

// For the right click|ban dialogs.
bool	g_Scoreboard_IsBan;
char	g_szScoreboard_SelectedUser[64];

//*****************************************************************************
//	FUNCTIONS
BOOL CALLBACK SERVERCONSOLE_ServerDialogBoxCallback( HWND hDlg, UINT Message, WPARAM wParam, LPARAM lParam )
{
	switch ( Message )
	{
	case WM_CLOSE:

		if ( MessageBox( hDlg, "Are you sure you want to quit?", SERVERCONSOLE_TITLESTRING, MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2 ) == IDYES )
		{
			SERVER_AddCommand( "quit" );
/*
			Shell_NotifyIcon( NIM_DELETE, &g_NotifyIconData );
			EndDialog( hDlg, -1 );
			CloseHandle( g_hThread );
			exit( 0 );
*/
		}
		break;
	case WM_DESTROY:

		Shell_NotifyIcon( NIM_DELETE, &g_NotifyIconData );
		PostQuitMessage( 0 );
		break;
	case WM_INITDIALOG:
		{
			LVCOLUMN	ColumnData;
			char		szString[256];
			char		szColumnTitle[64];
			LONG		lIndex;
			ULONG		ulIdx;

			// Set the global handle for the dialog box.
			g_hDlg = hDlg;

			// Load the icons.
			g_hSmallIcon = (HICON)LoadImage( g_hInst,
					MAKEINTRESOURCE( IDI_ICON5 ),
					IMAGE_ICON,
					16,
					16,
					LR_SHARED );

			SendMessage( hDlg, WM_SETICON, (WPARAM)ICON_SMALL, (LPARAM)g_hSmallIcon );
			SendMessage( hDlg, WM_SETICON, (WPARAM)ICON_BIG, (LPARAM)LoadIcon( g_hInst, MAKEINTRESOURCE( IDI_ICON5 )));

			// Set up the server dialog's status bar.
			g_hDlgStatusBar = CreateStatusWindow(WS_CHILD | WS_VISIBLE, (LPCTSTR)NULL, hDlg, IDC_SERVER_STATUSBAR);
			const int aDivideWidths[] = {40, 70, 185, 320, 600};
			SendMessage(g_hDlgStatusBar, SB_SETPARTS, (WPARAM)5, (LPARAM) aDivideWidths);
			SendMessage(g_hDlgStatusBar, SB_SETTEXT, (WPARAM)4, (LPARAM)DOTVERSIONSTR_REV);

			// Create and set our fonts for the titescreen and console.
			HFONT hTitleFont = CreateFont(14, 0, 0, 0, 600, 0, 0, 0, 0, 0, 0, 0, 0, "Tahoma"); 
			SendMessage(GetDlgItem( g_hDlg, IDC_TITLEBOX), WM_SETFONT, (WPARAM) hTitleFont, (LPARAM) 1);

			HFONT hConsoleFont = CreateFont(12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "Tahoma"); 
			SendMessage(GetDlgItem( g_hDlg, IDC_CONSOLEBOX), WM_SETFONT, (WPARAM) hConsoleFont, (LPARAM) 1);

			// Initialize the server console text.
			SetDlgItemText( hDlg, IDC_CONSOLEBOX, "=== S K U L L T A G | S E R V E R ===" );
			Printf( "\nRunning version: %s\n", DOTVERSIONSTR_REV );

			// Append the time.
			struct	tm		*pTimeInfo;
			time_t			clock;
			time (&clock);
			pTimeInfo = localtime (&clock);
			Printf("Started on %d/%d/%d at %d:%d:%d.\n", pTimeInfo->tm_mon, pTimeInfo->tm_mday,
				(1900+pTimeInfo->tm_year), pTimeInfo->tm_hour, pTimeInfo->tm_min, pTimeInfo->tm_sec);
				// [RC] TODO: AM/PM, localization, central time class

			Printf("\n");

			// Initialize the title string.
			sprintf( szString, "Server running version: %s", DOTVERSIONSTR_REV );
			SERVERCONSOLE_UpdateTitleString( szString );

			// Set the text limits for the console and input boxes.
			SendDlgItemMessage( hDlg, IDC_CONSOLEBOX, EM_SETLIMITTEXT, 4096, 0 );
			SendDlgItemMessage( hDlg, IDC_INPUTBOX, EM_SETLIMITTEXT, 256, 0 );

			// Insert the name column.
			sprintf( szColumnTitle, "Name" );
			ColumnData.mask = LVCF_FMT|LVCF_TEXT|LVCF_WIDTH;
			ColumnData.fmt = LVCFMT_LEFT;
			ColumnData.cx = 192;
			ColumnData.pszText = szColumnTitle;
			ColumnData.cchTextMax = 64;
			ColumnData.iSubItem = 0;
			lIndex = SendDlgItemMessage( hDlg, IDC_PLAYERLIST, LVM_INSERTCOLUMN, COLUMN_NAME, (LPARAM)&ColumnData );

			// Insert the frags column.
			sprintf( szColumnTitle, "Frags" );
			ColumnData.mask = LVCF_FMT|LVCF_TEXT|LVCF_WIDTH;
			ColumnData.fmt = LVCFMT_LEFT;
			ColumnData.cx = 64;
			ColumnData.pszText = szColumnTitle;
			ColumnData.cchTextMax = 64;
			ColumnData.iSubItem = 0;
			lIndex = SendDlgItemMessage( hDlg, IDC_PLAYERLIST, LVM_INSERTCOLUMN, COLUMN_FRAGS, (LPARAM)&ColumnData );

			// Insert the ping column.
			sprintf( szColumnTitle, "Ping" );
			ColumnData.mask = LVCF_FMT|LVCF_TEXT|LVCF_WIDTH;
			ColumnData.fmt = LVCFMT_LEFT;
			ColumnData.cx = 64;
			ColumnData.pszText = szColumnTitle;
			ColumnData.cchTextMax = 64;
			ColumnData.iSubItem = 0;
			lIndex = SendDlgItemMessage( hDlg, IDC_PLAYERLIST, LVM_INSERTCOLUMN, COLUMN_PING, (LPARAM)&ColumnData );

			// Insert the time column.
			sprintf( szColumnTitle, "Time" );
			ColumnData.mask = LVCF_FMT|LVCF_TEXT|LVCF_WIDTH;
			ColumnData.fmt = LVCFMT_LEFT;
			ColumnData.cx = 60;
			ColumnData.pszText = szColumnTitle;
			ColumnData.cchTextMax = 64;
			ColumnData.iSubItem = 0;
			lIndex = SendDlgItemMessage( hDlg, IDC_PLAYERLIST, LVM_INSERTCOLUMN, COLUMN_TIME, (LPARAM)&ColumnData );

			// Initialize the player indicies array.
			for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
				g_lPlayerIndicies[ulIdx] = -1;

			// Create the thread that runs the game.
			I_DetectOS( );
			g_hThread = CreateThread( NULL, 0, MainDoomThread, 0, 0, 0 );
		}
		break;
	case WM_NOTIFY:
		{
			// Show a pop-up menu to kick or ban users.
			LPNMHDR	nmhdr = (LPNMHDR)lParam;
			if((nmhdr->code == NM_RCLICK) && (nmhdr->idFrom == IDC_PLAYERLIST))
			{										
				char	szString[64];

				HWND	hList = GetDlgItem( hDlg, IDC_PLAYERLIST );
				LVITEM	pItem;

				// First, we check if the selected user is a bot by looking at the ping column.
				pItem.pszText = szString;
				pItem.mask = LVIF_TEXT;
				pItem.iItem = ListView_GetHotItem(hList);
				pItem.iSubItem = COLUMN_PING;
				pItem.cchTextMax = 16;

				// Grab it. If it fails, nothing is selected.
				if(!ListView_GetItem(hList, &pItem))
					break;

				// So, is it a bot?
				bool bIsBot = (strcmp(pItem.pszText, "Bot") == 0);
				
				// Next get the player/bot/aardvark's name.
				pItem.pszText = g_szScoreboard_SelectedUser;
				pItem.mask = LVIF_TEXT;
				pItem.iItem = ListView_GetHotItem(hList);
				pItem.iSubItem = COLUMN_NAME;
				pItem.cchTextMax = 64;

				if(!ListView_GetItem(hList, &pItem))
					break; // This shouldn't really happen by here.

				// Show our menus, based on whether it's a bot or a human player.
				if( bIsBot )
				{
					// Set up our menu.
					HMENU hMenu = CreatePopupMenu();
					AppendMenu(hMenu, MF_STRING, IDR_BOT_CLONE, "Clone");
					AppendMenu(hMenu, MF_STRING, IDR_BOT_REMOVE, "Remove");
					AppendMenu(hMenu, MF_STRING, IDR_BOT_REMOVEALL, "Remove all bots");

					// Show the menu and get the selected item.
					POINT pt;
					GetCursorPos(&pt);
					int iSelection = ::TrackPopupMenu(hMenu,
					TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RETURNCMD | TPM_HORIZONTAL,
					pt.x,pt.y, 0,hDlg,NULL);
					DestroyMenu(hMenu);

					char	szCommand[96];

					// Clone the bot; add him in again.
					if(iSelection == IDR_BOT_CLONE)
						sprintf( szCommand, "addbot \"%s\" red", g_szScoreboard_SelectedUser );

					// Remove the bot.
					else if(iSelection == IDR_BOT_REMOVE)
						sprintf( szCommand, "removebot \"%s\"", g_szScoreboard_SelectedUser );
					
					// Clear all bots.
					else if(iSelection == IDR_BOT_REMOVEALL)
						strcpy( szCommand, "removebots" );

					SERVER_AddCommand( szCommand );
				}
				else
				{

					// Set up our menu.
					HMENU hMenu = CreatePopupMenu();
					AppendMenu(hMenu, MF_STRING, IDR_PLAYER_KICK, "Kick");
					AppendMenu(hMenu, MF_STRING, IDR_PLAYER_KICK_WHY, "Kick (why)");
					AppendMenu(hMenu, MF_STRING, IDR_PLAYER_BAN, "Ban and kick");
					AppendMenu(hMenu, MF_STRING, IDR_PLAYER_BAN_WHY, "Ban and kick (why)");
					AppendMenu(hMenu, MF_STRING, IDR_PLAYER_GETIP, "Get IP address");

					// Show the menu and get the selected item.
					POINT pt;
					GetCursorPos(&pt);
					int iSelection = ::TrackPopupMenu(hMenu,
					TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RETURNCMD | TPM_HORIZONTAL,
					pt.x,pt.y, 0,hDlg,NULL);
					DestroyMenu(hMenu);

					char	szCommand[96];

					// Immediately kick or ban the player.
					if(iSelection == IDR_PLAYER_KICK)
					{
						sprintf( szCommand, "kick \"%s\"", g_szScoreboard_SelectedUser );
						SERVER_AddCommand( szCommand );
					}
					else if(iSelection == IDR_PLAYER_BAN)
					{
						sprintf( szCommand, "ban \"%s\"", g_szScoreboard_SelectedUser );
						SERVER_AddCommand( szCommand );
					}

					// Show a dialog first and handle things there.
					else if((iSelection == IDR_PLAYER_KICK_WHY) || (iSelection == IDR_PLAYER_BAN_WHY))
					{
						g_Scoreboard_IsBan = (iSelection == IDR_PLAYER_BAN_WHY);
						DialogBox( g_hInst, MAKEINTRESOURCE( IDD_REASON ), hDlg, SERVERCONSOLE_ReasonCallback );			
					}

					// Pop up his IP.
					else if(iSelection == IDR_PLAYER_GETIP)
						DialogBox( g_hInst, MAKEINTRESOURCE( IDD_GOTIP ), hDlg, SERVERCONSOLE_GotIPCallback );
				}

			}
			break;
		}
	case WM_COMMAND:

		{

			switch ( LOWORD( wParam ))
			{
/*
			// This even occurs when esc is pressed.
			case IDCANCEL:

				if ( MessageBox( hDlg, "Are you sure you want to quit?", SERVERCONSOLE_TITLESTRING, MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2 ) == IDYES )
				{
					EndDialog( hDlg, -1 );
					CloseHandle( g_hThread );
					exit( 0 );
				}
				break;
*/
			// Server admin has inputted a command.
			case IDC_SEND:
				{
					char	szBuffer[1024];

					GetDlgItemText( hDlg, IDC_INPUTBOX, szBuffer, sizeof( szBuffer ));

					// If the text in the send buffer doesn't begin with a slash, the admin is just
					// talking.
					if ( szBuffer[0] != '/' )
					{
						char	szBuffer2[2056];

						sprintf( szBuffer2, "say %s", szBuffer );
						SERVER_AddCommand( szBuffer2 );
					}
					else
						SERVER_AddCommand( szBuffer + 1 );

					SetDlgItemText( hDlg, IDC_INPUTBOX, "" );
				}
				break;
			case IDC_CONSOLEBOX:
/*
				{
					int	i;

//					eM_SCROLL
					i = EM_LINESCROLL;
					i = EM_CANUNDO;
					i = EM_CHARFROMPOS;
					i = EM_EMPTYUNDOBUFFER;
					i = EM_FMTLINES;
					i = EM_GETFIRSTVISIBLELINE;
					i = EM_GETHANDLE;
					i = EM_GETIMESTATUS;
					i = EM_GETLIMITTEXT;
					i = EM_GETLINE;
					i = EM_GETLINECOUNT;
					i = EM_GETMARGINS; 		// 10
					i = EM_GETMODIFY;
					i = EM_GETPASSWORDCHAR;
					i = EM_GETRECT;
					i = EM_GETSEL;
					i = EM_GETTHUMB;
					i = EM_GETWORDBREAKPROC;
					i = EM_LIMITTEXT;
					i = EM_LINEFROMCHAR;
					i = EM_LINEINDEX;
					i = EM_LINELENGTH;		// 20
					i = EM_LINESCROLL;
					i = EM_POSFROMCHAR;
					i = EM_REPLACESEL;
					i = EM_SCROLL;
					i = EM_SCROLLCARET;
					i = EM_SETHANDLE;
					i = EM_SETIMESTATUS;
					i = EM_SETLIMITTEXT;
					i = EM_SETMARGINS;
					i = EM_SETMODIFY;		// 30
					i = EM_SETPASSWORDCHAR;
					i = EM_SETREADONLY;
					i = EM_SETRECT;
					i = EM_SETRECTNP;
					i = EM_SETSEL;
					i = EM_SETTABSTOPS;
					i = EM_SETWORDBREAKPROC;
					i = EM_UNDO;
					i = EN_CHANGE;  
					i = EN_ERRSPACE;  		// 40
					i = EN_HSCROLL;
					i = EN_KILLFOCUS;
					i = EN_MAXTEXT;
					i = EN_SETFOCUS;
					i = EN_UPDATE;
					i = EN_VSCROLL; 
					i = WM_COMMAND;
					i = WM_COPY;
					i = WM_CTLCOLOREDIT;
					i = WM_CUT;  		// 50
					i = WM_PASTE;  
					i = WM_UNDO;

					i = IDC_CONSOLEBOX;
					i = WM_COMMAND;
					i = LOWORD( lParam );
					i = HIWORD( lParam );
					i = LOWORD( wParam );
					i = HIWORD( wParam );
					i = LOWORD( Message );
					i = HIWORD( Message );
					i = lParam;
					i = wParam;
					i = Message;
				}
				switch ( LOWORD( lParam ))
				{
				case EM_LINESCROLL:

					{
						int	i;

						i = 0;
					}
					break;
				}

				switch ( HIWORD( lParam ))
				{
				case EM_LINESCROLL:

					{
						int	i;

						i = 0;
					}
					break;
				}
				switch ( LOWORD( wParam ))
				{
				case EM_LINESCROLL:

					{
						int	i;

						i = 0;
					}
					break;
				}
				switch ( HIWORD( wParam ))
				{
				case EM_LINESCROLL:

					{
						int	i;

						i = 0;
					}
					break;
				}
*/
				break;
			case ID_FILE_EXIT:

				if ( MessageBox( hDlg, "Are you sure you want to quit?", SERVERCONSOLE_TITLESTRING, MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2 ) == IDYES )
				{
					EndDialog( hDlg, -1 );
					SuspendThread( g_hThread );
					CloseHandle( g_hThread );
					exit( 0 );
				}
				break;
			case ID_FILE_IMPORTCONFIGURATION:

				MessageBox( hDlg, "IMPORT!", "OMG", MB_OK );
				break;
			case ID_FILE_EXPORTCONFIGURATION:

				MessageBox( hDlg, "EXPORT!", "OMG", MB_OK );
				break;
			case ID_SETTINGS_GENERALSETTINGS:

				DialogBox( g_hInst, MAKEINTRESOURCE( IDD_GENERALSETTINGS ), hDlg, SERVERCONSOLE_GeneralSettingsCallback );
				break;
			case ID_SETTINGS_CONFIGREDMFLAGS:

				DialogBox( g_hInst, MAKEINTRESOURCE( IDD_DMFLAGS ), hDlg, SERVERCONSOLE_DMFlagsCallback );
				break;
			case ID_SETTINGS_MAPROTATION:

				DialogBox( g_hInst, MAKEINTRESOURCE( IDD_MAPROTATION ), hDlg, SERVERCONSOLE_MapRotationCallback );
				break;
			case ID_SETTINGS_LMSSETTINGS:

				DialogBox( g_hInst, MAKEINTRESOURCE( IDD_LMSSETTINGS ), hDlg, SERVERCONSOLE_LMSSettingsCallback );
				break;
			case ID_SETTINGS_MESSAGES:

				DialogBox( g_hInst, MAKEINTRESOURCE( IDD_MESSAGES ), hDlg, SERVERCONSOLE_MessagesCallback );
				break;
			case ID_SERVER_GENERALINFORMATION:

				DialogBox( g_hInst, MAKEINTRESOURCE( IDD_SERVERINFO ), hDlg, SERVERCONSOLE_ServerInformationCallback );
				break;
			case ID_SERVER_STATISTICS:

				DialogBox( g_hInst, MAKEINTRESOURCE( IDD_SERVERSTATISTICS ), hDlg, SERVERCONSOLE_ServerStatisticsCallback );
				break;
			case ID_ADMIN_ADDBOT:

				DialogBox( g_hInst, MAKEINTRESOURCE( IDD_ADDBOT ), hDlg, SERVERCONSOLE_AddBotCallback );
				break;
			case ID_ADMIN_REMOVEBOT:

				DialogBox( g_hInst, MAKEINTRESOURCE( IDD_REMOVEBOT ), hDlg, SERVERCONSOLE_RemoveBotCallback );
				break;
			case ID_ADMIN_KICKBAN:

				DialogBox( g_hInst, MAKEINTRESOURCE( IDD_KICKBAN ), hDlg, SERVERCONSOLE_KickBanCallback );
				break;
			case ID_ADMIN_CHANGEMAP:

				DialogBox( g_hInst, MAKEINTRESOURCE( IDD_CHANGEMAP ), hDlg, SERVERCONSOLE_ChangeMapCallback );
				break;
			case ID_ADMIN_BANIP:

				DialogBox( g_hInst, MAKEINTRESOURCE( IDD_BANIP ), hDlg, SERVERCONSOLE_BanIPCallback );
				break;
			case ID_ADMIN_VIEWBANLIST:

				DialogBox( g_hInst, MAKEINTRESOURCE( IDD_BANLIST ), hDlg, SERVERCONSOLE_BanListCallback );
				break;
			}
		}
		break;
	case WM_SYSCOMMAND:

		if (( wParam == SC_MINIMIZE ) && ( sv_minimizetosystray ))
		{
			RECT			DesktopRect;
			RECT			ThisWindowRect;
			ANIMATIONINFO	AnimationInfo;
			NOTIFYICONDATA	NotifyIconData;
			UCVarValue		Val;
			char			szString[64];

			AnimationInfo.cbSize = sizeof( AnimationInfo );
			SystemParametersInfo( SPI_GETANIMATION, sizeof( AnimationInfo ), &AnimationInfo, 0 );

			// Animations are turned ON, go ahead with the animation.
			if ( AnimationInfo.iMinAnimate )
			{
				GetWindowRect( GetDesktopWindow( ),&DesktopRect );
				GetWindowRect( hDlg,&ThisWindowRect );

				// Set the destination rect to the lower right corner of the screen
				DesktopRect.left = DesktopRect.right;
				DesktopRect.top = DesktopRect.bottom;

				// Do the little animation showing the window moving to the systray.
#ifndef __WINE__
				DrawAnimatedRects( hDlg, IDANI_CAPTION, &ThisWindowRect,&DesktopRect );
#endif
			}

			// Hide the window.
			ShowWindow( hDlg, SW_HIDE );

			// Show the notification icon.
			ZeroMemory( &NotifyIconData, sizeof( NotifyIconData ));
			NotifyIconData.cbSize = sizeof( NOTIFYICONDATA );
			NotifyIconData.hWnd = hDlg;
			NotifyIconData.uID = 0;
			NotifyIconData.uFlags = NIF_ICON|NIF_MESSAGE|NIF_TIP;

			NotifyIconData.uCallbackMessage = UWM_TRAY_TRAYID;
			NotifyIconData.hIcon = g_hSmallIcon;
			
			Val = sv_hostname.GetGenericRep( CVAR_String );
			sprintf( szString, "%s", Val.String );
			lstrcpy( NotifyIconData.szTip, szString );

			Shell_NotifyIcon( NIM_ADD, &NotifyIconData );
			break;
		}

		DefWindowProc( hDlg, Message, wParam, lParam );
		break;
//		return ( FALSE );
	case UWM_TRAY_TRAYID:

		switch ( lParam )
		{
		case WM_LBUTTONDOWN:

			g_bSmallIconClicked = true;
			return true;
		case WM_LBUTTONUP:

			{
				RECT			DesktopRect;
				RECT			ThisWindowRect;
				NOTIFYICONDATA	NotifyIconData;
				UCVarValue		Val;
				char			szString[64];

				GetWindowRect( GetDesktopWindow( ), &DesktopRect );
				DesktopRect.left = DesktopRect.right;
				DesktopRect.top = DesktopRect.bottom;
				GetWindowRect( hDlg, &ThisWindowRect );

				// Animate the maximization.
#ifndef __WINE__
				DrawAnimatedRects( hDlg, IDANI_CAPTION, &DesktopRect, &ThisWindowRect );
#endif

				ShowWindow( hDlg, SW_SHOW );
				SetActiveWindow( hDlg );
				SetForegroundWindow( hDlg );

				// Hide the notification icon.
				ZeroMemory( &NotifyIconData, sizeof( NotifyIconData ));
				NotifyIconData.cbSize = sizeof( NOTIFYICONDATA );
				NotifyIconData.hWnd = hDlg;
				NotifyIconData.uID = 0;
				NotifyIconData.uFlags = NIF_ICON|NIF_MESSAGE|NIF_TIP;
				NotifyIconData.uCallbackMessage = UWM_TRAY_TRAYID;
				NotifyIconData.hIcon = g_hSmallIcon;//LoadIcon( g_hInst, MAKEINTRESOURCE( IDI_ICON1 ));

				Val = sv_hostname.GetGenericRep( CVAR_String );
				sprintf( szString, "%s", Val.String );
				lstrcpy( g_NotifyIconData.szTip, szString );

				Shell_NotifyIcon( NIM_DELETE, &NotifyIconData );
				g_bSmallIconClicked = false;
			}
			return ( TRUE );
		default:

			break;
		}

		return ( FALSE );
	default:

		return ( FALSE );
	}

	return ( TRUE );
}

//*****************************************************************************
//
BOOL CALLBACK SERVERCONSOLE_DMFlagsCallback( HWND hDlg, UINT Message, WPARAM wParam, LPARAM lParam )
{
	switch ( Message )
	{
	case WM_CLOSE:

		EndDialog( hDlg, -1 );
		break;
	case WM_INITDIALOG:

		// Initialize the checks on the dmflags/dmflags2/compatflags.
		SERVERCONSOLE_InitializeDMFlagsDisplay( hDlg );

		// Initialize the dmflags/dmflags2/compatflags display.
		SERVERCONSOLE_UpdateDMFlagsDisplay( hDlg );
		break;
	case WM_COMMAND:

		{
			switch ( LOWORD( wParam ))
			{
			case IDOK:

				SERVERCONSOLE_UpdateDMFlags( hDlg );
				EndDialog( hDlg, -1 );
				break;
			case IDCANCEL:

				EndDialog( hDlg, -1 );
				break;
			case IDC_FORCE_FALLINGZD:
			case IDC_FORCE_FALLINGHX:
			case IDC_WEAPONS_STAY:
			case IDC_NO_ITEMS:
			case IDC_NO_HEALTH:
			case IDC_NO_ARMOR:
			case IDC_SPAWN_FARTHEST:
			case IDC_SAME_LEVEL:
			case IDC_FORCE_RESPAWN:
			case IDC_NO_EXIT:
			case IDC_INFINITE_AMMO:
			case IDC_NO_MONSTERS:
			case IDC_MONSTERS_RESPAWN:
			case IDC_ITEMS_RESPAWN:
			case IDC_RESPAWN_SUPER:
			case IDC_FAST_MONSTERS:
			case IDC_NO_JUMP:
			case IDC_NO_FREELOOK:
			case IDC_NO_FOV:
			case IDC_NO_COOP_MP_WEAPON_SPAWN:
			case IDC_YES_WEAPONDROP:
			case IDC_NO_RUNES:
			case IDC_INSTANT_RETURN:
			case IDC_DISALLOW_CROUCH:
			case IDC_NO_TEAM_SWITCH:
			case IDC_NO_TEAM_SELECT:
			case IDC_COOP_LOSE_INVENTORY:
			case IDC_COOP_LOSE_ARMOR:
			case IDC_YES_DOUBLEAMMO:
			case IDC_COOP_LOSE_POWERUPS:
			case IDC_YES_DEGENERATION:
			case IDC_COOP_LOSE_AMMO:
			case IDC_YES_FREEAIMBFG:
			case IDC_BARRELS_RESPAWN:
			case IDC_NO_RESPAWN_INVUL:
			case IDC_KEEP_KEYS:
			case IDC_KEEP_WEAPONS:
			case IDC_SHOTGUN_START:
			case IDC_SAME_SPAWN_SPOT:
			case IDC_SHORTTEX:
			case IDC_STAIRINDEX:
			case IDC_LIMITPAIN:
			case IDC_SILENTPICKUP:
			case IDC_NO_PASSMOBJ:
			case IDC_MAGICSILENCE:
			case IDC_WALLRUN:
			case IDC_NOTOSSDROPS:
			case IDC_USEBLOCKING:
			case IDC_NODOORLIGHT:
			case IDC_LIMITED_AIRMOVEMENT:
			case IDC_PLASMA_BUMP_BUG:
			case IDC_INSTANTRESPAWN:
			case IDC_DISABLETAUNTS:
			case IDC_ORIGINALSOUNDCURVE:
			case IDC_OLDINTERMISSION:
			case IDC_DISABLESTEALTHMONSTERS:

				// Now that the dmflags/dmflags2/compatflags have changed, update the display.
				SERVERCONSOLE_UpdateDMFlagsDisplay( hDlg );
				break;
			}
		}
		break;
	default:

		return ( FALSE );
	}

	return ( TRUE );
}

//*****************************************************************************
//
BOOL CALLBACK SERVERCONSOLE_MapRotationCallback( HWND hDlg, UINT Message, WPARAM wParam, LPARAM lParam )
{
	switch ( Message )
	{
	case WM_CLOSE:

		EndDialog( hDlg, -1 );
		break;
	case WM_INITDIALOG:

		{
			ULONG	ulIdx;

			// Set the text limits for the input box.
			SendDlgItemMessage( hDlg, IDC_MAPNAME, EM_SETLIMITTEXT, 32, 0 );

			// Initialize the box.
			SendDlgItemMessage( hDlg, IDC_MAPLISTBOX, LB_RESETCONTENT, 0, 0 );

			// Initialize the "use rotation" checkbox.
			if ( sv_maprotation )
				SendDlgItemMessage( hDlg, IDC_USEMAPROTATIONLIST, BM_SETCHECK, BST_CHECKED, 0 );
			else
				SendDlgItemMessage( hDlg, IDC_USEMAPROTATIONLIST, BM_SETCHECK, BST_UNCHECKED, 0 );

			// Initialize the "randomize" checkbox.
			if ( sv_randommaprotation )
				SendDlgItemMessage( hDlg, IDC_USERANDOMMAP, BM_SETCHECK, BST_CHECKED, 0 );
			else
				SendDlgItemMessage( hDlg, IDC_USERANDOMMAP, BM_SETCHECK, BST_UNCHECKED, 0 );

			// Populate the box with the current map rotation list.
			for ( ulIdx = 0; ulIdx < MAPROTATION_GetNumEntries( ); ulIdx++ )
				SendDlgItemMessage( hDlg, IDC_MAPLISTBOX, LB_INSERTSTRING, -1, (LPARAM) (LPCTSTR)MAPROTATION_GetMapName( ulIdx ));
		}

		break;
	case WM_COMMAND:

		{
			switch ( LOWORD( wParam ))
			{
			case IDOK:

				{
					LONG	lIdx;
					char	szString[32];
					LONG	lCount;
				
					// First, clear the existing map rotation list.
					MAPROTATION_Construct( );

					// Now, add the maps in the listbox to the map rotation list.
					lCount = SendDlgItemMessage( hDlg, IDC_MAPLISTBOX, LB_GETCOUNT, 0, 0 );
					if ( lCount != LB_ERR )
					{
						char	szBuffer[32];

						for ( lIdx = 0; lIdx < lCount; lIdx++ )
						{
							SendDlgItemMessage( hDlg, IDC_MAPLISTBOX, LB_GETTEXT, lIdx, (LPARAM) (LPCTSTR)szBuffer );

							sprintf( szString, "addmap %s", szBuffer );
							SERVER_AddCommand( szString );
						}
					}

					// Build and execute the command string.
					if ( SendDlgItemMessage( hDlg, IDC_USEMAPROTATIONLIST, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
						sv_maprotation = true;
					else
						sv_maprotation = false;

					// Build and execute the command string.
					if ( SendDlgItemMessage( hDlg, IDC_USERANDOMMAP, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
						sv_randommaprotation = true;
					else
						sv_randommaprotation = false;
				}
				EndDialog( hDlg, -1 );
				break;
			case IDCANCEL:

				EndDialog( hDlg, -1 );
				break;
			case IDC_ADD:

				{
					char	szString[32];

					// Get the text from the input box.
					GetDlgItemText( hDlg, IDC_MAPNAME, szString, 32 );

					// Add the string to the end of the list.
					if ( szString[0] )
						SendDlgItemMessage( hDlg, IDC_MAPLISTBOX, LB_INSERTSTRING, -1, (LPARAM)szString );

					// Clear out the input box.
					SetDlgItemText( hDlg, IDC_MAPNAME, "" );
				}
				break;
			case IDC_REMOVE:

				{
					// When the user clicks the Remove button, we first get the number
					// of selected items

					HWND	hList = GetDlgItem( hDlg, IDC_MAPLISTBOX );
					LONG	lCount = SendMessage( hList, LB_GETSELCOUNT, 0, 0 );

					if ( lCount != LB_ERR )
					{
						if( lCount != 0 )
						{
							// And then allocate room to store the list of selected items.
							LONG	lIdx;
							LONG	*palBuffer;

							palBuffer = (LONG *)malloc( sizeof( LONG ) * lCount );

							SendMessage( hList, LB_GETSELITEMS, (WPARAM)lCount, (LPARAM)palBuffer );
							
							// Now we loop through the list and remove each item that was
							// selected.  

							// WARNING!!!  
							// We loop backwards, because if we removed items
							// from top to bottom, it would change the indices of the other
							// items!!!

							for( lIdx = lCount - 1; lIdx >= 0; lIdx-- )
								SendMessage( hList, LB_DELETESTRING, palBuffer[lIdx], 0 );

							free( palBuffer );
						}
					}
					else
					{
						MessageBox( hDlg, "Error removing items!", "Warning", MB_OK );
					}
				}
				break;
			case IDC_CLEAR:

				if ( MessageBox( hDlg, "Are you sure you want to clear the map rotation list?", SERVERCONSOLE_TITLESTRING, MB_YESNO|MB_ICONQUESTION ) == IDYES )
					SendDlgItemMessage( hDlg, IDC_MAPLISTBOX, LB_RESETCONTENT, 0, 0 );
				break;
			}
		}
		break;
	default:

		return ( FALSE );
	}

	return ( TRUE );
}

//*****************************************************************************
//
BOOL CALLBACK SERVERCONSOLE_LMSSettingsCallback( HWND hDlg, UINT Message, WPARAM wParam, LPARAM lParam )
{
	switch ( Message )
	{
	case WM_CLOSE:

		EndDialog( hDlg, -1 );
		break;
	case WM_INITDIALOG:

		// Initialize the checks on the LMS settings checkboxes.
		SERVERCONSOLE_InitializeLMSSettingsDisplay( hDlg );

		// Initialize the lmsallowedweapons/lmsspectatorsettings display.
		SERVERCONSOLE_UpdateLMSSettingsDisplay( hDlg );
		break;
	case WM_COMMAND:

		{
			switch ( LOWORD( wParam ))
			{
			case IDOK:

				SERVERCONSOLE_UpdateLMSSettings( hDlg );
				EndDialog( hDlg, -1 );
				break;
			case IDCANCEL:

				EndDialog( hDlg, -1 );
				break;
			case IDC_LMS_ALLOWCHAINSAW:
			case IDC_LMS_ALLOWPISTOL:
			case IDC_LMS_ALLOWSHOTGUN:
			case IDC_LMS_ALLOWSSG:
			case IDC_LMS_ALLOWCHAINGUN:
			case IDC_LMS_ALLOWMINIGUN:
			case IDC_LMS_ALLOWROCKETLAUNCHER:
			case IDC_LMS_ALLOWGRENADELAUNCHER:
			case IDC_LMS_ALLOWPLASMA:
			case IDC_LMS_ALLOWRAILGUN:
			case IDC_LMS_SPECTATORVIEW:
			case IDC_LMS_SPECTATORTALK:

				// Now that the lmsallowedweapons/lmsspectatorsettings have changed, update the display.
				SERVERCONSOLE_UpdateLMSSettingsDisplay( hDlg );
				break;
			}
		}
		break;
	default:

		return ( FALSE );
	}

	return ( TRUE );
}

//*****************************************************************************
//
BOOL CALLBACK SERVERCONSOLE_MessagesCallback( HWND hDlg, UINT Message, WPARAM wParam, LPARAM lParam )
{
	switch ( Message )
	{
	case WM_CLOSE:

		EndDialog( hDlg, -1 );
		break;
	case WM_INITDIALOG:

		{
			// Set the text limits for the input box.
			SendDlgItemMessage( hDlg, IDC_LOGFILE, EM_SETLIMITTEXT, 128, 0 );

			// Initialize the "Enable timestamp" checkbox.
			if ( sv_timestamp )
				SendDlgItemMessage( hDlg, IDC_TIMESTAMP, BM_SETCHECK, BST_CHECKED, 0 );
			else
				SendDlgItemMessage( hDlg, IDC_TIMESTAMP, BM_SETCHECK, BST_UNCHECKED, 0 );

			SendDlgItemMessage( hDlg, IDC_TIMESTAMPFORMAT, CB_INSERTSTRING, -1, (WPARAM)(LPSTR)"[HH:MM:SS]" );
			SendDlgItemMessage( hDlg, IDC_TIMESTAMPFORMAT, CB_INSERTSTRING, -1, (WPARAM)(LPSTR)"[HH:MM:SS AM/PM]" );
			SendDlgItemMessage( hDlg, IDC_TIMESTAMPFORMAT, CB_INSERTSTRING, -1, (WPARAM)(LPSTR)"[HH:MM:SS am/pm]" );
			SendDlgItemMessage( hDlg, IDC_TIMESTAMPFORMAT, CB_INSERTSTRING, -1, (WPARAM)(LPSTR)"[HH:MM]" );
			SendDlgItemMessage( hDlg, IDC_TIMESTAMPFORMAT, CB_INSERTSTRING, -1, (WPARAM)(LPSTR)"[HH:MM AM/PM]" );
			SendDlgItemMessage( hDlg, IDC_TIMESTAMPFORMAT, CB_INSERTSTRING, -1, (WPARAM)(LPSTR)"[HH:MM am/pm]" );

			switch ( sv_timestampformat )
			{
			case 0:

				SendDlgItemMessage( hDlg, IDC_TIMESTAMPFORMAT, CB_SETCURSEL, 0, 0 );
				break;
			case 1:

				SendDlgItemMessage( hDlg, IDC_TIMESTAMPFORMAT, CB_SETCURSEL, 1, 0 );
				break;
			case 2:

				SendDlgItemMessage( hDlg, IDC_TIMESTAMPFORMAT, CB_SETCURSEL, 2, 0 );
				break;
			case 3:

				SendDlgItemMessage( hDlg, IDC_TIMESTAMPFORMAT, CB_SETCURSEL, 3, 0 );
				break;
			case 4:

				SendDlgItemMessage( hDlg, IDC_TIMESTAMPFORMAT, CB_SETCURSEL, 4, 0 );
				break;
			case 5:

				SendDlgItemMessage( hDlg, IDC_TIMESTAMPFORMAT, CB_SETCURSEL, 5, 0 );
				break;
			default:

				SendDlgItemMessage( hDlg, IDC_TIMESTAMPFORMAT, CB_SETCURSEL, 0, 0 );
				break;
			}

			switch ( sv_colorstripmethod )
			{
			// Strip color codes.
			case 0:

				SendDlgItemMessage( hDlg, IDC_STRIPCODES, BM_SETCHECK, BST_CHECKED, 0 );
				break;
			// Don't strip color codes.
			case 1:

				SendDlgItemMessage( hDlg, IDC_DONOTSTRIPCODES, BM_SETCHECK, BST_CHECKED, 0 );
				break;
			// Leave in "\c<X>" format.
			case 2:

				SendDlgItemMessage( hDlg, IDC_LEAVEINFORMAT, BM_SETCHECK, BST_CHECKED, 0 );
				break;
			default:

				SendDlgItemMessage( hDlg, IDC_STRIPCODES, BM_SETCHECK, BST_CHECKED, 0 );
				break;
			}

			if ( Logfile )
				SendDlgItemMessage( hDlg, IDC_ENABLELOGGING, BM_SETCHECK, BST_CHECKED, 0 );
			else
				SendDlgItemMessage( hDlg, IDC_ENABLELOGGING, BM_SETCHECK, BST_UNCHECKED, 0 );

			SetDlgItemText( hDlg, IDC_LOGFILE, g_szLogFilename );
		}

		break;
	case WM_COMMAND:

		{
			switch ( LOWORD( wParam ))
			{
			case IDOK:

				{
					// Enable timestamping if the user has the checkbox checked.
					if ( SendDlgItemMessage( hDlg, IDC_TIMESTAMP, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
						sv_timestamp = true;
					else
						sv_timestamp = false;

					sv_timestampformat = SendDlgItemMessage( hDlg, IDC_TIMESTAMPFORMAT, CB_GETCURSEL, 0, 0 );

					// Set the method of color code stripping.
					if ( SendDlgItemMessage( hDlg, IDC_STRIPCODES, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
						sv_colorstripmethod = 0;
					else if ( SendDlgItemMessage( hDlg, IDC_DONOTSTRIPCODES, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
						sv_colorstripmethod = 1;
					else if ( SendDlgItemMessage( hDlg, IDC_LEAVEINFORMAT, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
						sv_colorstripmethod = 2;
					else
						sv_colorstripmethod = 0;

					GetDlgItemText( hDlg, IDC_LOGFILE, g_szLogFilename, 1024 );

					if ( SendDlgItemMessage( hDlg, IDC_ENABLELOGGING, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
					{
						// If a logfile is already open, close it.
						if ( Logfile )
						{
							Printf( "Log stopped: %s\n", myasctime( ));
							fclose( Logfile );
							Logfile = NULL;
						}

						if ( Logfile = fopen( g_szLogFilename, "w" ))
							Printf( "Log started: %s\n", myasctime( ));
						else
							Printf( "Could not start log\n" );
					}
					else
					{
						// If a logfile is already open, close it.
						if ( Logfile )
						{
							Printf( "Log stopped: %s\n", myasctime( ));
							fclose( Logfile );
							Logfile = NULL;
						}
					}
				}
				EndDialog( hDlg, -1 );
				break;
			case IDCANCEL:

				EndDialog( hDlg, -1 );
				break;
			}
		}
		break;
	default:

		return ( FALSE );
	}

	return ( TRUE );
}

//*****************************************************************************
//
BOOL CALLBACK SERVERCONSOLE_AddBotCallback( HWND hDlg, UINT Message, WPARAM wParam, LPARAM lParam )
{
	switch ( Message )
	{
	case WM_CLOSE:

		EndDialog( hDlg, -1 );
		break;
	case WM_INITDIALOG:

		{
			ULONG	ulIdx;
			ULONG	ulNumBotInfos;

			// Initialize the box.
			SendDlgItemMessage( hDlg, IDC_BOTLIST, LB_RESETCONTENT, 0, 0 );

			// Populate the box with the name of all the bots.
			ulNumBotInfos = BOTINFO_GetNumBotInfos( );
			for ( ulIdx = 0; ulIdx < ulNumBotInfos; ulIdx++ )
			{
				char	szName[64];

				// Don't put unrevealed bots in the list!
				if ( BOTINFO_GetRevealed( ulIdx ) == false )
					continue;

				sprintf( szName, BOTINFO_GetName( ulIdx ));
				V_ColorizeString( szName );
				V_RemoveColorCodes( szName );
//				SendDlgItemMessage( hDlg, IDC_BOTLIST, LB_INSERTSTRING, -1, (LPARAM) (LPCTSTR)szName );
				SendDlgItemMessage( hDlg, IDC_BOTLIST, LB_ADDSTRING, 0, (LPARAM) (LPCTSTR)szName );
			}
		}

		break;
	case WM_COMMAND:

		{
			switch ( LOWORD( wParam ))
			{
			case IDC_ADD:

				{
					LONG	lIdx;
					char	szBuffer[256];
					char	szString[256];
				
					lIdx = SendDlgItemMessage( hDlg, IDC_BOTLIST, LB_GETCURSEL, 0, 0 ); 
					if ( lIdx != LB_ERR )
					{
						SendDlgItemMessage( hDlg, IDC_BOTLIST, LB_GETTEXT, lIdx, (LPARAM) (LPCTSTR)szBuffer );
						sprintf( szString, "addbot \"%s\"", szBuffer );
						
						SERVER_AddCommand( szString );
					}
				}
				break;
			case IDOK:

				EndDialog( hDlg, -1 );
				break;
			}
		}
		break;
	default:

		return ( FALSE );
	}

	return ( TRUE );
}

//*****************************************************************************
//
BOOL CALLBACK SERVERCONSOLE_RemoveBotCallback( HWND hDlg, UINT Message, WPARAM wParam, LPARAM lParam )
{
	switch ( Message )
	{
	case WM_CLOSE:

		EndDialog( hDlg, -1 );
		break;
	case WM_INITDIALOG:

		{
			ULONG	ulIdx;

			// Initialize the box.
			SendDlgItemMessage( hDlg, IDC_BOTLIST, LB_RESETCONTENT, 0, 0 );

			// Populate the box with the current player list.
			for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
			{
				char	szName[64];

				if (( playeringame[ulIdx] == false ) || ( players[ulIdx].bIsBot == false ))
					continue;

				sprintf( szName, players[ulIdx].userinfo.netname );
//				V_UnColorizeString( szName );
				V_RemoveColorCodes( szName );
				SendDlgItemMessage( hDlg, IDC_PLAYERNAMES, LB_INSERTSTRING, -1, (LPARAM) (LPCTSTR)szName );
			}
		}

		break;
	case WM_COMMAND:

		{
			switch ( LOWORD( wParam ))
			{
			case IDC_REMOVE:

				{
					LONG	lIdx;
					char	szBuffer[256];
					char	szString[256];
				
					lIdx = SendDlgItemMessage( hDlg, IDC_BOTLIST, LB_GETCURSEL, 0, 0 ); 
					if ( lIdx != LB_ERR )
					{
						SendDlgItemMessage( hDlg, IDC_BOTLIST, LB_GETTEXT, lIdx, (LPARAM) (LPCTSTR)szBuffer );
						V_RemoveColorCodes( szBuffer );
						sprintf( szString, "removebot \"%s\"", szBuffer );
						
						SERVER_AddCommand( szString );
						SendDlgItemMessage( hDlg, IDC_BOTLIST, LB_DELETESTRING, lIdx, 0 );
					}
				}
				break;
				EndDialog( hDlg, -1 );
			case IDC_REMOVEALLBOTS:

				if ( MessageBox( hDlg, "Are you sure you want to remove all the bots?", SERVERCONSOLE_TITLESTRING, MB_YESNO|MB_ICONQUESTION ) == IDYES )
				{
					SERVER_AddCommand( "removebots" );
					EndDialog( hDlg, -1 );
				}
				break;
			case IDOK:

				EndDialog( hDlg, -1 );
				break;
			}
		}
		break;
	default:

		return ( FALSE );
	}

	return ( TRUE );
}

//*****************************************************************************
//
BOOL CALLBACK SERVERCONSOLE_KickBanCallback( HWND hDlg, UINT Message, WPARAM wParam, LPARAM lParam )
{
	switch ( Message )
	{
	case WM_CLOSE:

		EndDialog( hDlg, -1 );
		break;
	case WM_INITDIALOG:

		{
			ULONG	ulIdx;

			// Set the text limits for the input box.
			SendDlgItemMessage( hDlg, IDC_REASON, EM_SETLIMITTEXT, 191, 0 );

			// Initialize the box.
			SendDlgItemMessage( hDlg, IDC_PLAYERNAMES, LB_RESETCONTENT, 0, 0 );

			// Set the radio buttons for kick/ban.
			SendDlgItemMessage( hDlg, IDC_KICKBAN_KICK, BM_SETCHECK, BST_CHECKED, 0 );

			// Populate the box with the current player list.
			for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
			{
				char	szName[64];

				if ( playeringame[ulIdx] == false )
					continue;

				sprintf( szName, players[ulIdx].userinfo.netname );
				V_RemoveColorCodes( szName );
				SendDlgItemMessage( hDlg, IDC_PLAYERNAMES, LB_INSERTSTRING, -1, (LPARAM) (LPCTSTR)szName );
			}
		}

		break;
	case WM_COMMAND:

		{
			switch ( LOWORD( wParam ))
			{
			case IDOK:

				{
					LONG	lIdx;
					// [RC] Resusing a single buffer string gives an error if you use all availible characters
					// for either the name or reason, even though they 'should' only use 64 and 191 characters.
					char	szName[64];
					char	szReason[192];
					char	szCommand[256];
				
					lIdx = SendDlgItemMessage( hDlg, IDC_PLAYERNAMES, LB_GETCURSEL, 0, 0 ); 
					if ( lIdx != LB_ERR )
					{
						// Get the selected player's name and the reason.
						SendDlgItemMessage( hDlg, IDC_PLAYERNAMES, LB_GETTEXT, lIdx, (LPARAM) (LPCTSTR)szName );
						GetDlgItemText( hDlg, IDC_REASON, (LPTSTR)szReason, 192 );
						szReason[191] = 0;

						// Build our string and execute.
						if( SendDlgItemMessage( hDlg, IDC_KICKBAN_KICKANDBAN, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
							sprintf( szCommand, "ban \"%s\" \"%s\"", szName, szReason );
						else
							sprintf( szCommand, "kick \"%s\" \"%s\"", szName, szReason );
						
						SERVER_AddCommand( szCommand );
					}
				}
				EndDialog( hDlg, -1 );
				break;
			case IDCANCEL:

				EndDialog( hDlg, -1 );
				break;
			}
		}
		break;
	default:

		return ( FALSE );
	}

	return ( TRUE );
}

//*****************************************************************************
//
BOOL CALLBACK SERVERCONSOLE_ReasonCallback( HWND hDlg, UINT Message, WPARAM wParam, LPARAM lParam )
{
	// Whether we need to refresh the form to reflect kicking or kickbanning.
	bool	bUpdateLabels = false;
	char	szString[256];
	char	szBuffer[192];

	switch ( Message )
	{
	case WM_CLOSE:

		EndDialog( hDlg, -1 );
		break;
	case WM_INITDIALOG:

		{		
			// Set the text limits for the input box.
			SendDlgItemMessage( hDlg, IDC_REASON, EM_SETLIMITTEXT, 191, 0 );

			// Set our labels.
			bUpdateLabels = true;

		}

		break;
	case WM_COMMAND:

		{
			switch ( LOWORD( wParam ))
			{
			case IDC_REASON_BAN:
				g_Scoreboard_IsBan = ( SendDlgItemMessage( hDlg, IDC_REASON_BAN, BM_GETCHECK, 0, 0 ) == BST_CHECKED );
				bUpdateLabels = true;
				break;
			case IDOK:
				GetDlgItemText( hDlg, IDC_REASON, (LPTSTR)szBuffer, 192 );
				szBuffer[191] = 0;
				
				if( g_Scoreboard_IsBan )
					sprintf( szString, "ban \"%s\" \"%s\"", g_szScoreboard_SelectedUser, szBuffer );
				else
					sprintf( szString, "kick \"%s\" \"%s\"", g_szScoreboard_SelectedUser, szBuffer );
				SERVER_AddCommand( szString );

				EndDialog( hDlg, -1 );
				break;
			case IDCANCEL:

				EndDialog( hDlg, -1 );
				break;
			}
		}
		break;
	default:

		return ( FALSE );
	}

	if(bUpdateLabels)
	{
		if ( g_Scoreboard_IsBan )
		{
			// Set the 'details' label.
			SetDlgItemText( hDlg, IDC_REASON_DETAILS, "He will be IP banned and will not be able to reconnect.");
			// Update the window name.
			SetWindowText( hDlg, "Ready to ban." );

		}
		else
		{
			SetDlgItemText( hDlg, IDC_REASON_DETAILS, "Not being banned, he will be able to reconnect afterwards.");
			// Update the window name.
			SetWindowText( hDlg, "Ready to kick." );

		}

		// Set the 'name' label.
		sprintf( szString, "Ready to %s %s.", g_Scoreboard_IsBan ? "kick and ban":"kick", g_szScoreboard_SelectedUser);
		SetDlgItemText( hDlg, IDC_REASON_NAME, szString);	

		// Set the status of the 'ban' checkbox.
		SendDlgItemMessage( hDlg, IDC_REASON_BAN, BM_SETCHECK, g_Scoreboard_IsBan ? BST_CHECKED : BST_UNCHECKED, 0 );

	}

	return ( TRUE );
}

//*****************************************************************************
//
char		szIPString[127];
BOOL CALLBACK SERVERCONSOLE_GotIPCallback( HWND hDlg, UINT Message, WPARAM wParam, LPARAM lParam )
{
	switch ( Message )
	{
	case WM_CLOSE:

		EndDialog( hDlg, -1 );
		break;
	case WM_INITDIALOG:

		{		
			
			char		szPlayerName[64];

			// Loop through all the players, and try to find one that matches the given name.
			for ( ULONG ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
			{
				if ( playeringame[ulIdx] == false )
					continue;

				// Removes the color codes from the player name so it appears as the server sees it in the window.
				sprintf( szPlayerName, players[ulIdx].userinfo.netname );
				V_RemoveColorCodes( szPlayerName );

				if ( stricmp( szPlayerName, g_szScoreboard_SelectedUser ) == 0 )
				{
					// Bots do not have IPs. This should never happen.
					if ( players[ulIdx].bIsBot )
						continue;

					sprintf(szIPString, "IP address for %s: %d.%d.%d.%d.",
						g_szScoreboard_SelectedUser,
						SERVER_GetClient( ulIdx )->Address.abIP[0],
						SERVER_GetClient( ulIdx )->Address.abIP[1],
						SERVER_GetClient( ulIdx )->Address.abIP[2],
						SERVER_GetClient( ulIdx )->Address.abIP[3]);

					SetDlgItemText(hDlg, IDC_GOTIP_IP, szIPString);
				}
			}


		}

		break;
	case WM_COMMAND:

		{
			switch ( LOWORD( wParam ))
			{
			case IDOK:
				
				if(OpenClipboard(hDlg))
				{
					HGLOBAL clipbuffer;
					char * buffer;
					EmptyClipboard();
					clipbuffer = GlobalAlloc(GMEM_DDESHARE, strlen((char *)szIPString)+1);
					buffer = (char*)GlobalLock(clipbuffer);
					strcpy(buffer, LPCSTR(szIPString));
					GlobalUnlock(clipbuffer);
					SetClipboardData(CF_TEXT,clipbuffer);
					CloseClipboard();
				}

				EndDialog( hDlg, -1 );
				break;
			case IDCANCEL:

				EndDialog( hDlg, -1 );
				break;
			}
		}
		break;
	default:

		return ( FALSE );
	}

	return ( TRUE );
}

//*****************************************************************************
//
BOOL CALLBACK SERVERCONSOLE_ChangeMapCallback( HWND hDlg, UINT Message, WPARAM wParam, LPARAM lParam )
{
	switch ( Message )
	{
	case WM_CLOSE:

		EndDialog( hDlg, -1 );
		break;
	case WM_INITDIALOG:

		{
			// Set the text limit for the map name box. I don't think a lump will ever be more
			// than 8 characters in length, but just in case, I'll allow for 32 chars of text.
			SendDlgItemMessage( hDlg, IDC_MAPNAME, EM_SETLIMITTEXT, 32, 0 );
		}

		break;
	case WM_COMMAND:

		{
			switch ( LOWORD( wParam ))
			{
			case IDOK:

				{
					char	szBuffer[32];
					char	szString[48];

					// Get the text from the input box.
					GetDlgItemText( hDlg, IDC_MAPNAME, szBuffer, 32 );

					if ( strlen( szBuffer ) > 0 )
					{
						// Build and execute the command string.
						if ( SendDlgItemMessage( hDlg, IDC_INTERMISSION, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
							sprintf( szString, "changemap %s", szBuffer );
						else
							sprintf( szString, "map %s", szBuffer );
						SERVER_AddCommand( szString );
					}
				}
				EndDialog( hDlg, -1 );
				break;
			case IDCANCEL:

				EndDialog( hDlg, -1 );
				break;
			}
		}
		break;
	default:

		return ( FALSE );
	}

	return ( TRUE );
}

//*****************************************************************************
//
BOOL CALLBACK SERVERCONSOLE_BanIPCallback( HWND hDlg, UINT Message, WPARAM wParam, LPARAM lParam )
{
	switch ( Message )
	{
	case WM_CLOSE:

		EndDialog( hDlg, -1 );
		break;
	case WM_INITDIALOG:

		{
			// Set the text limit for the IP box.
			SendDlgItemMessage( hDlg, IDC_MAPNAME, EM_SETLIMITTEXT, 256, 0 );
		}

		break;
	case WM_COMMAND:

		{
			switch ( LOWORD( wParam ))
			{
			case IDOK:

				{
					char	szBuffer[64];
					char	szString[384];

					// Get the text from the input box.
					GetDlgItemText( hDlg, IDC_IPADDRESS, szBuffer, 256 );

					if ( strlen( szBuffer ) > 0 )
					{
						char	szComment[256];

						// Get the text from the input box.
						GetDlgItemText( hDlg, IDC_BANCOMMENT, szComment, 256 );

						// Build and execute the command string.
						if ( strlen( szComment ) > 0 )
							sprintf( szString, "addban %s \"%s\"", szBuffer, szComment );
						else
							sprintf( szString, "addban %s", szBuffer );
						SERVER_AddCommand( szString );
					}
				}
				EndDialog( hDlg, -1 );
				break;
			case IDCANCEL:

				EndDialog( hDlg, -1 );
				break;
			}
		}
		break;
	default:

		return ( FALSE );
	}

	return ( TRUE );
}

//*****************************************************************************
//
BOOL CALLBACK SERVERCONSOLE_BanListCallback( HWND hDlg, UINT Message, WPARAM wParam, LPARAM lParam )
{
	ULONG		ulIdx;
	UCVarValue	Val;
	char		szString[256];
	IPADDRESSBAN_s		Ban;

	Val = sv_banfile.GetGenericRep( CVAR_String );

	switch ( Message )
	{
	case WM_CLOSE:

		EndDialog( hDlg, -1 );
		break;
	case WM_INITDIALOG:

		{
			// Set the text limit for the IP box.
			SendDlgItemMessage( hDlg, IDC_BANFILE, EM_SETLIMITTEXT, 256, 0 );

			// Set the text limit for the IP box.
			SetDlgItemText( hDlg, IDC_BANFILE, Val.String );

			if ( sv_enforcebans )
				SendDlgItemMessage( hDlg, IDC_ENFORCEBANS, BM_SETCHECK, BST_CHECKED, 0 );
			else
				SendDlgItemMessage( hDlg, IDC_ENFORCEBANS, BM_SETCHECK, BST_UNCHECKED, 0 );

			// Populate the box with the current ban list.
			for ( ulIdx = 0; ulIdx < SERVERBAN_GetNumBans(); ulIdx++ )
			{
				Ban = SERVERBAN_GetBan( ulIdx );
				if (( stricmp( Ban.szIP[0], "0" ) != 0 ) ||
					( stricmp( Ban.szIP[1], "0" ) != 0 ) ||
					( stricmp( Ban.szIP[2], "0" ) != 0 ) ||
					( stricmp( Ban.szIP[3], "0" ) != 0 ))
				{
					sprintf( szString, "%s.%s.%s.%s", Ban.szIP[0],
						Ban.szIP[1],
						Ban.szIP[2],
						Ban.szIP[3] );
					if ( Ban.szComment[0] )
						sprintf( szString, "%s:%s", szString, Ban.szComment );

					SendDlgItemMessage( hDlg, IDC_BANLIST, LB_INSERTSTRING, -1, (LPARAM)szString );
				}
			}
		}

		break;
	case WM_COMMAND:

		{
			switch ( LOWORD( wParam ))
			{
			case IDC_EDIT:

				{
					LONG	lIdx;

					lIdx = SendDlgItemMessage( hDlg, IDC_BANLIST, LB_GETCURSEL, 0, 0 );
					if ( lIdx != LB_ERR )
					{
						SendDlgItemMessage( hDlg, IDC_BANLIST, LB_GETTEXT, lIdx, (LPARAM)g_szBanEditString );
						if ( DialogBox( g_hInst, MAKEINTRESOURCE( IDD_EDITBAN ), hDlg, SERVERCONSOLE_EditBanCallback ))
						{
							SendDlgItemMessage( hDlg, IDC_BANLIST, LB_DELETESTRING, lIdx, 0 );
							SendDlgItemMessage( hDlg, IDC_BANLIST, LB_INSERTSTRING, lIdx, (LPARAM)g_szBanEditString );
						}
					}
					else
						MessageBox( hDlg, "Please select a ban to edit first.", SERVERCONSOLE_TITLESTRING, MB_OK );
				}
				break;
			case IDC_REMOVE:

				{
					LONG	lIdx;

					lIdx = SendDlgItemMessage( hDlg, IDC_BANLIST, LB_GETCURSEL, 0, 0 );
					if ( lIdx != LB_ERR )
						SendDlgItemMessage( hDlg, IDC_BANLIST, LB_DELETESTRING, lIdx, 0 );
					else
						MessageBox( hDlg, "Please select a ban to remove first.", SERVERCONSOLE_TITLESTRING, MB_OK );
				}
				break;
			case IDC_CLEAR:

				// Clear out the ban list box.
				if ( MessageBox( hDlg, "Are you sure you want to clear the ban list?", SERVERCONSOLE_TITLESTRING, MB_YESNO|MB_ICONQUESTION ) == IDYES )
					SendDlgItemMessage( hDlg, IDC_BANLIST, LB_RESETCONTENT, 0, 0 );
				break;
			case IDOK:

				{
					LONG	lIdx;
					LONG	lCount;
					char	szBuffer[256];
					char	szString[256+32];

					// Get the text from the input box.
					GetDlgItemText( hDlg, IDC_BANFILE, szBuffer, 256 );

					sprintf( szString, "sv_banfile %s", szBuffer );
					SERVER_AddCommand( szString );

					if ( SendDlgItemMessage( hDlg, IDC_ENFORCEBANS, BM_GETCHECK, BST_CHECKED, 0 ))
						sv_enforcebans = true;
					else
						sv_enforcebans = false;

					// Clear out the ban list, and then add all the bans in the ban list.
					SERVERBAN_ClearBans( );

					// Now, add the maps in the listbox to the map rotation list.
					lCount = SendDlgItemMessage( hDlg, IDC_BANLIST, LB_GETCOUNT, 0, 0 );
					if ( lCount != LB_ERR )
					{
						char	*pszIP;
						char	szIP[32];
						char	*pszComment;
						char	szComment[224];
						char	*pszBuffer;

						for ( lIdx = 0; lIdx < lCount; lIdx++ )
						{
							SendDlgItemMessage( hDlg, IDC_BANLIST, LB_GETTEXT, lIdx, (LPARAM) (LPCTSTR)szBuffer );

							pszIP = szIP;
							*pszIP = 0;
							pszComment = szComment;
							*pszComment = 0;
							pszBuffer = szBuffer;
							while ( *pszBuffer != 0 && *pszBuffer != ':' && *pszBuffer != '/' )
							{
								*pszIP = *pszBuffer;
								pszBuffer++;
								pszIP++;
								*pszIP = 0;
							}

							// Don't include the comment denotion character in the comment string.
							if ( *pszBuffer == ':' || *pszBuffer == '/' )
								pszBuffer++;

							while ( *pszBuffer != 0 )
							{
								*pszComment = *pszBuffer;
								pszBuffer++;
								pszComment++;
								*pszComment = 0;
							}

							if ( strlen( szComment ))
								sprintf( szString, "addban %s \"%s\"", szIP, szComment );
							else
								sprintf( szString, "addban %s", szIP );
							SERVER_AddCommand( szString );
						}
					}
				}
				EndDialog( hDlg, -1 );
				break;
			case IDCANCEL:

				EndDialog( hDlg, -1 );
				break;
			}
		}
		break;
	default:

		return ( FALSE );
	}

	return ( TRUE );
}

//*****************************************************************************
//
BOOL CALLBACK SERVERCONSOLE_EditBanCallback( HWND hDlg, UINT Message, WPARAM wParam, LPARAM lParam )
{
	switch ( Message )
	{
	case WM_CLOSE:

		EndDialog( hDlg, false );
		break;
	case WM_INITDIALOG:

		// Set the text limit for the IP box.
		SendDlgItemMessage( hDlg, IDC_BANBOX, EM_SETLIMITTEXT, 256, 0 );

		// Set the text limit for the ban box.
		SetDlgItemText( hDlg, IDC_BANBOX, g_szBanEditString );
		break;
	case WM_COMMAND:

		{
			switch ( LOWORD( wParam ))
			{
			case IDOK:

				// Get the text from the input box.
				GetDlgItemText( hDlg, IDC_BANFILE, g_szBanEditString, 256 );

				EndDialog( hDlg, true );
				break;
			case IDCANCEL:

				EndDialog( hDlg, false );
				break;
			}
		}
		break;
	default:

		return ( FALSE );
	}

	return ( TRUE );
}

//*****************************************************************************
//
BOOL CALLBACK SERVERCONSOLE_ServerInformationCallback( HWND hDlg, UINT Message, WPARAM wParam, LPARAM lParam )
{
	ULONG	ulIdx;
	ULONG	ulNumPWADs;
	ULONG	ulRealIWADIdx;

	switch ( Message )
	{
	case WM_CLOSE:

		EndDialog( hDlg, -1 );
		break;
	case WM_INITDIALOG:

		{
			char		szString[256];
			UCVarValue	Val;

			sprintf( szString, "Skulltag v%s server @ %s", DOTVERSIONSTR, NETWORK_AddressToString( NETWORK_GetLocalAddress( )));
			SetDlgItemText( hDlg, IDC_VERSION, szString );

			sprintf( szString, "Operating system: %s", g_szOperatingSystem );
			SetDlgItemText( hDlg, IDC_OPERATINGSYSTEM, szString );

			sprintf( szString, "CPU Speed: %s", g_szCPUSpeed );
			SetDlgItemText( hDlg, IDC_CPUSPEED, szString );

			sprintf( szString, "Vendor: %s", g_szVendor );
			SetDlgItemText( hDlg, IDC_VENDOR, szString );

			Val = sv_maxclients.GetGenericRep( CVAR_Int );
			sprintf( szString, "Max. clients: %d", Val.Int );
			SetDlgItemText( hDlg, IDC_MAXCLIENTS, szString );

			Val = sv_maxplayers.GetGenericRep( CVAR_Int );
			sprintf( szString, "Max. players: %d", Val.Int );
			SetDlgItemText( hDlg, IDC_MAXPLAYERS, szString );

			Val = fraglimit.GetGenericRep( CVAR_Int );
			sprintf( szString, "Fraglimit: %d", Val.Int );
			SetDlgItemText( hDlg, IDC_FRAGLIMIT, szString );

			Val = timelimit.GetGenericRep( CVAR_Float );
			sprintf( szString, "Timelimit: %5.2f", Val.Float );
			SetDlgItemText( hDlg, IDC_TIMELIMIT, szString );

			Val = pointlimit.GetGenericRep( CVAR_Int );
			sprintf( szString, "Pointlimit: %d", Val.Int );
			SetDlgItemText( hDlg, IDC_POINTLIMIT, szString );

			Val = winlimit.GetGenericRep( CVAR_Int );
			sprintf( szString, "Winlimit: %d", Val.Int );
			SetDlgItemText( hDlg, IDC_WINLIMIT, szString );

			sprintf( szString, "Gameplay mode: %s", skulltag ? "Skulltag" :
				oneflagctf ? "One flag CTF" :
				ctf ? "Capture the flag" :
				teamgame ? "Teamgame" :
				teampossession ? "Team possession" :
				possession ? "Possession" :
				teamlms ? "Team LMS" :
				lastmanstanding ? "Last man standing" :
				terminator ? "Terminator" :
				duel ? "Duel" :
				teamplay ? "Teamplay deathmatch" :
				deathmatch ? "Deathmatch" :
				"Cooperative" );
			SetDlgItemText( hDlg, IDC_GAMEPLAYMODE, szString );

			sprintf( szString, "Game: %s", SERVER_MASTER_GetGameName( ));
			SetDlgItemText( hDlg, IDC_GAME, szString );

			Val = sv_hostemail.GetGenericRep( CVAR_String );
			sprintf( szString, "E-mail address: %s", Val.String );
			SetDlgItemText( hDlg, IDC_EMAILADDRESS, szString );

			Val = sv_website.GetGenericRep( CVAR_String );
			sprintf( szString, "WAD URL: %s", Val.String );
			SetDlgItemText( hDlg, IDC_WADURL, szString );

			// This is a little tricky. Since WADs can now be loaded within pk3 files, we have
			// to skip over all the ones automatically loaded. To my knowledge, the only way to
			// do this is to skip wads that have a colon in them.
			ulNumPWADs = 0;
			for ( ulIdx = 0; Wads.GetWadName( ulIdx ) != NULL; ulIdx++ )
			{
				if ( strchr( Wads.GetWadName( ulIdx ), ':' ) == NULL )
				{
					if ( ulNumPWADs == FWadCollection::IWAD_FILENUM )
					{
						ulRealIWADIdx = ulIdx;
						break;
					}

					ulNumPWADs++;
				}
			}

			ulNumPWADs = 0;
			for ( ulIdx = 0; Wads.GetWadName( ulIdx ) != NULL; ulIdx++ )
			{
				// Skip the IWAD file index, skulltag.wad/pk3, files that were automatically
				// loaded from subdirectories (such as skin files), and WADs loaded automatically
				// within pk3 files.
				if (( ulIdx == ulRealIWADIdx ) ||
					( stricmp( Wads.GetWadName( ulIdx ), "skulltag.pk3" ) == 0 ) ||
					( Wads.GetLoadedAutomatically( ulIdx )) ||
					( strchr( Wads.GetWadName( ulIdx ), ':' ) != NULL ))
				{
					continue;
				}

				ulNumPWADs++;
			}

			if ( ulNumPWADs == 0 )
				sprintf( szString, "PWADs: None" );
			else
			{
				sprintf( szString, "PWADs:" );

				for ( ulIdx = 0; Wads.GetWadName( ulIdx ) != NULL; ulIdx++ )
				{
					// Skip the IWAD file index, skulltag.wad/pk3, files that were automatically
					// loaded from subdirectories (such as skin files), and WADs loaded automatically
					// within pk3 files.
					if (( ulIdx == ulRealIWADIdx ) ||
						( stricmp( Wads.GetWadName( ulIdx ), "skulltag.pk3" ) == 0 ) ||
						( Wads.GetLoadedAutomatically( ulIdx )) ||
						( strchr( Wads.GetWadName( ulIdx ), ':' ) != NULL ))
					{
						continue;
					}

					sprintf( szString, "%s %s", szString, Wads.GetWadName( ulIdx ));
				}
			}

			SetDlgItemText( hDlg, IDC_PWADS, szString );
		}
		break;
	case WM_COMMAND:

		{
			switch ( LOWORD( wParam ))
			{
			case IDOK:

				EndDialog( hDlg, -1 );
				break;
			}
		}
		break;
	default:

		return ( FALSE );
	}

	return ( TRUE );
}

//*****************************************************************************
//
BOOL CALLBACK SERVERCONSOLE_ServerStatisticsCallback( HWND hDlg, UINT Message, WPARAM wParam, LPARAM lParam )
{
	switch ( Message )
	{
	case WM_CLOSE:

		g_hStatisticDlg = NULL;

		EndDialog( hDlg, -1 );
		break;
	case WM_INITDIALOG:

		{
			char		szString[256];
			LONG		lData;

			g_hStatisticDlg = hDlg;

			// Total INBOUND data transfer.
			lData = SERVER_STATISTIC_GetTotalInboundDataTransferred( ); 
			if ( lData > GIGABYTE )
				sprintf( szString, "Total data transfer (in): %0.2fKB (%0.2fGB)", (float)lData / (float)KILOBYTE, (float)lData / GIGABYTE );
			else if ( lData > MEGABYTE )
				sprintf( szString, "Total data transfer (in): %0.2fKB (%0.2fMB)", (float)lData / (float)KILOBYTE, (float)lData / MEGABYTE );
			else
				sprintf( szString, "Total data transfer (in): %0.2fKB", (float)lData / (float)KILOBYTE );
			SetDlgItemText( hDlg, IDC_TOTALINBOUNDDATATRANSFER, szString );

			// Total OUTBOUND data transfer.
			lData = SERVER_STATISTIC_GetTotalOutboundDataTransferred( ); 
			if ( lData > GIGABYTE )
				sprintf( szString, "Total data transfer (out): %0.2fKB (%0.2fGB)", (float)lData / (float)KILOBYTE, (float)lData / GIGABYTE );
			else if ( lData > MEGABYTE )
				sprintf( szString, "Total data transfer (out): %0.2fKB (%0.2fMB)", (float)lData / (float)KILOBYTE, (float)lData / MEGABYTE );
			else
				sprintf( szString, "Total data transfer (out): %0.2fKB", (float)lData / (float)KILOBYTE );
			SetDlgItemText( hDlg, IDC_TOTALOUTBOUNDDATATRANSFER, szString );

			// Average INBOUND data transfer.
			lData = SERVER_STATISTIC_GetTotalInboundDataTransferred( ); 
			if ( SERVER_STATISTIC_GetTotalSecondsElapsed( ) == 0 )
				sprintf( szString, "Average data transfer: 0B/s (in)" );
			else
			{
				float	fDataPerSecond;

				fDataPerSecond = (float)lData / (float)SERVER_STATISTIC_GetTotalSecondsElapsed( );
				if ( fDataPerSecond > KILOBYTE )
					sprintf( szString, "Average data transfer (in): %0.2fB/s (%0.2fKB/s)", fDataPerSecond, fDataPerSecond / (float)KILOBYTE );
				else
					sprintf( szString, "Average data transfer (in): %0.2fB/s", fDataPerSecond / (float)KILOBYTE );
			}
			SetDlgItemText( hDlg, IDC_AVERAGEINBOUNDDATATRANSFER, szString );

			// Average OUTBOUND data transfer.
			lData = SERVER_STATISTIC_GetTotalOutboundDataTransferred( ); 
			if ( SERVER_STATISTIC_GetTotalSecondsElapsed( ) == 0 )
				sprintf( szString, "Average data transfer (out): 0B/s" );
			else
			{
				float	fDataPerSecond;

				fDataPerSecond = (float)lData / (float)SERVER_STATISTIC_GetTotalSecondsElapsed( );
				if ( fDataPerSecond > KILOBYTE )
					sprintf( szString, "Average data transfer (out): %0.2fB/s (%0.2fKB/s)", fDataPerSecond, fDataPerSecond / (float)KILOBYTE );
				else
					sprintf( szString, "Average data transfer (out): %0.2fB/s", fDataPerSecond / (float)KILOBYTE );
			}
			SetDlgItemText( hDlg, IDC_AVERAGEOUTBOUNDDATATRANSFER, szString );

			// Peak INBOUND data transfer.
			lData = SERVER_STATISTIC_GetPeakInboundDataTransfer( );
			if ( lData > KILOBYTE )
				sprintf( szString, "Peak data transfer (in): %ldB/s (%0.2fKB/s)", lData, (float)lData / (float)KILOBYTE );
			else
				sprintf( szString, "Peak data transfer (in): %ldB/s", lData );
			SetDlgItemText( hDlg, IDC_PEAKINBOUNDDATATRANSFER, szString );

			// Peak OUTBOUND data transfer.
			lData = SERVER_STATISTIC_GetPeakOutboundDataTransfer( );
			if ( lData > KILOBYTE )
				sprintf( szString, "Peak data transfer (out): %ldB/s (%0.2fKB/s)", lData, (float)lData / (float)KILOBYTE );
			else
				sprintf( szString, "Peak data transfer (out): %ldB/s", lData );
			SetDlgItemText( hDlg, IDC_PEAKOUTBOUNDDATATRANSFER, szString );

			// Current INBOUND data transfer.
			lData = SERVER_STATISTIC_GetCurrentInboundDataTransfer( );
			if ( lData > KILOBYTE )
				sprintf( szString, "Current data transfer (in): %ldB/s (%0.2fKB/s)", lData, (float)lData / (float)KILOBYTE );
			else
				sprintf( szString, "Current data transfer (in): %ldB/s", lData );
			SetDlgItemText( hDlg, IDC_CURRENTINBOUNDDATATRANSFER, szString );

			// Current OUTBOUND data transfer.
			lData = SERVER_STATISTIC_GetCurrentOutboundDataTransfer( );
			if ( lData > KILOBYTE )
				sprintf( szString, "Current data transfer (out): %ldB/s (%0.2fKB/s)", lData, (float)lData / (float)KILOBYTE );
			else
				sprintf( szString, "Current data transfer (out): %ldB/s", lData );
			SetDlgItemText( hDlg, IDC_CURRENTOUTBOUNDDATATRANSFER, szString );

			if ( SERVER_STATISTIC_GetTotalSecondsElapsed( ) == 0 )
				sprintf( szString, "Average number of players: 0.00" );
			else
				sprintf( szString, "Average number of players: %0.2f", (float)SERVER_STATISTIC_GetTotalPlayers( ) / (float)SERVER_STATISTIC_GetTotalSecondsElapsed( ));
			SetDlgItemText( hDlg, IDC_AVGNUMPLAYERS, szString );

			sprintf( szString, "Max. players at one time: %ld", SERVER_STATISTIC_GetMaxNumPlayers( ));
			SetDlgItemText( hDlg, IDC_MAXPLAYERSATONETIME, szString );

			sprintf( szString, "Total number of frags: %ld", SERVER_STATISTIC_GetTotalFrags( ));
			SetDlgItemText( hDlg, IDC_TOTALFRAGS, szString );

			lData = SERVER_STATISTIC_GetTotalSecondsElapsed( );
			if ( lData >= DAY )
				sprintf( szString, "Total uptime: %ldd%ldh%ldm%lds", lData / DAY, ( lData / HOUR ) % 24, ( lData / MINUTE ) % 60, lData % MINUTE );
			else if ( lData >= HOUR )
				sprintf( szString, "Total uptime: %ldh%ldm%lds", ( lData / HOUR ) % 24, ( lData / MINUTE ) % 60, lData % MINUTE  );
			else if ( lData >= MINUTE )
				sprintf( szString, "Total uptime: %ldm%lds", ( lData / MINUTE ) % 60, lData % MINUTE  );
			else
				sprintf( szString, "Total uptime: %lds", lData );
			SetDlgItemText( hDlg, IDC_TOTALUPTIME, szString );
		}
		break;
	case WM_COMMAND:

		{
			switch ( LOWORD( wParam ))
			{
			case IDOK:

				g_hStatisticDlg = false;

				EndDialog( hDlg, -1 );
				break;
			}
		}
		break;
	default:

		return ( FALSE );
	}

	return ( TRUE );
}

//*****************************************************************************
//
BOOL CALLBACK SERVERCONSOLE_GeneralSettingsCallback( HWND hDlg, UINT Message, WPARAM wParam, LPARAM lParam )
{
	switch ( Message )
	{
	case WM_CLOSE:

		EndDialog( hDlg, -1 );
		break;
	case WM_INITDIALOG:

		// Set the text limits for the input boxes.
		SendDlgItemMessage( hDlg, IDC_SERVERNAME, EM_SETLIMITTEXT, 96, 0 );
		SendDlgItemMessage( hDlg, IDC_WADURL, EM_SETLIMITTEXT, 96, 0 );
		SendDlgItemMessage( hDlg, IDC_EMAIL, EM_SETLIMITTEXT, 96, 0 );
		SendDlgItemMessage( hDlg, IDC_MASTERIP, EM_SETLIMITTEXT, 96, 0 );
		SendDlgItemMessage( hDlg, IDC_MOTD, EM_SETLIMITTEXT, 512, 0 );
		SendDlgItemMessage( hDlg, IDC_FRAGLIMIT, EM_SETLIMITTEXT, 4, 0 );
		SendDlgItemMessage( hDlg, IDC_TIMELIMIT, EM_SETLIMITTEXT, 4, 0 );
		SendDlgItemMessage( hDlg, IDC_POINTLIMIT, EM_SETLIMITTEXT, 4, 0 );
		SendDlgItemMessage( hDlg, IDC_DUELLIMIT, EM_SETLIMITTEXT, 4, 0 );
		SendDlgItemMessage( hDlg, IDC_WINLIMIT, EM_SETLIMITTEXT, 4, 0 );
		SendDlgItemMessage( hDlg, IDC_PASSWORD, EM_SETLIMITTEXT, 32, 0 );
		SendDlgItemMessage( hDlg, IDC_JOINPASSWORD, EM_SETLIMITTEXT, 32, 0 );
		SendDlgItemMessage( hDlg, IDC_RCONPASSWORD, EM_SETLIMITTEXT, 32, 0 );
		SendDlgItemMessage( hDlg, IDC_MAXCLIENTS, EM_SETLIMITTEXT, 4, 0 );
		SendDlgItemMessage( hDlg, IDC_MAXPLAYERS, EM_SETLIMITTEXT, 4, 0 );

		SendDlgItemMessage( hDlg, IDC_SPIN2, UDM_SETRANGE, 0, MAKELONG( 9999,0 ));
		SendDlgItemMessage( hDlg, IDC_SPIN3, UDM_SETRANGE, 0, MAKELONG( 9999,0 ));
		SendDlgItemMessage( hDlg, IDC_SPIN4, UDM_SETRANGE, 0, MAKELONG( 9999,0 ));
		SendDlgItemMessage( hDlg, IDC_SPIN5, UDM_SETRANGE, 0, MAKELONG( 9999,0 ));
		SendDlgItemMessage( hDlg, IDC_SPIN7, UDM_SETRANGE, 0, MAKELONG( 9999,0 ));
		SendDlgItemMessage( hDlg, IDC_SPIN1, UDM_SETRANGE, 0, MAKELONG( MAXPLAYERS,0 ));
		SendDlgItemMessage( hDlg, IDC_SPIN6, UDM_SETRANGE, 0, MAKELONG( MAXPLAYERS,0 ));

		// Initialize the boxes.
		SERVERCONSOLE_InitializeGeneralSettingsDisplay( hDlg );
		break;
	case WM_COMMAND:

		{
			switch ( LOWORD( wParam ))
			{
			case IDOK:

				if ( SERVERCONSOLE_IsRestartNeeded( hDlg ))
				{
					switch ( MessageBox( hDlg, "One or more settings have changed that will not take effect until the map changes. Do you want to restart the map now?", SERVERCONSOLE_TITLESTRING, MB_YESNOCANCEL|MB_ICONQUESTION ))
					{
					case IDYES:

						{
							char	szBuffer[64];

							sprintf( szBuffer, "map %s", level.mapname );

							SERVERCONSOLE_UpdateGeneralSettings( hDlg );
							SERVER_AddCommand( szBuffer );
							EndDialog( hDlg, -1 );
						}
						break;
					case IDNO:

						SERVERCONSOLE_UpdateGeneralSettings( hDlg );
						EndDialog( hDlg, -1 );
					case IDCANCEL:

						break;
					}
				}
				else
				{
					SERVERCONSOLE_UpdateGeneralSettings( hDlg );
					EndDialog( hDlg, -1 );
				}
				break;
			case IDCANCEL:

				EndDialog( hDlg, -1 );
				break;
			}
		}
		break;
	default:

		return ( FALSE );
	}

	return ( TRUE );
}

//*****************************************************************************
/*
DWORD WINAPI MainDoomThread( LPVOID )
{
	// Setup our main thread ID so that if we have a crash, it crashes properly.
	MainThread = INVALID_HANDLE_VALUE;
	DuplicateHandle (GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &MainThread,
		0, FALSE, DUPLICATE_SAME_ACCESS);
	MainThreadID = GetCurrentThreadId();

	D_DoomMain( );

	return ( 0 );
}
*/
//*****************************************************************************
//
void SERVERCONSOLE_UpdateTitleString( char *pszString )
{
	char		szString[256];

	sprintf( szString, "%s", pszString );
	SetDlgItemText( g_hDlg, IDC_TITLEBOX, szString );
	SetWindowText( g_hDlg, szString );
}

//*****************************************************************************
//
void SERVERCONSOLE_UpdateIP( NETADDRESS_s LocalAddress )
{
	SendMessage(g_hDlgStatusBar, SB_SETTEXT, (WPARAM)2, (LPARAM) NETWORK_AddressToString( LocalAddress ));
	SERVERCONSOLE_UpdateBroadcasting( );
}

//*****************************************************************************
//
void SERVERCONSOLE_UpdateBroadcasting( void )
{
	SendMessage(g_hDlgStatusBar, SB_SETTEXT, (WPARAM)0, (LPARAM) (sv_updatemaster ? "Public" : "Private"));
	SendMessage(g_hDlgStatusBar, SB_SETTEXT, (WPARAM)1, (LPARAM) (sv_broadcast ? "LAN" : ""));
}

//*****************************************************************************
//
void SERVERCONSOLE_UpdateScoreboard( void )
{
	char		szString[256];
	ULONG		ulLine;
	ULONG		ulIdx;
	LONG		lTextBox;

	szString[0] = 0;
	SetDlgItemText( g_hDlg, IDC_SCOREBOARD1, szString );
	SetDlgItemText( g_hDlg, IDC_SCOREBOARD2, szString );
	SetDlgItemText( g_hDlg, IDC_SCOREBOARD3, szString );
	SetDlgItemText( g_hDlg, IDC_SCOREBOARD4, szString );

	ulLine = 0;
	if (( lastmanstanding == false ) && ( teamlms == false ) && deathmatch && fraglimit && gamestate == GS_LEVEL )
	{
		bool	bFoundPlayer = false;
		LONG	lHighestFragcount;
		ULONG	ulFragsLeft;

		// If we're in a teamplay, just go by whichever team has the most frags.
		if ( teamplay )
		{
			if ( TEAM_GetFragCount( TEAM_BLUE ) >= TEAM_GetFragCount( TEAM_RED ))
				lHighestFragcount = TEAM_GetFragCount( TEAM_BLUE );
			else
				lHighestFragcount = TEAM_GetFragCount( TEAM_RED );
		}
		// Otherwise, find the player with the most frags.
		else
		{
			for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
			{
				if ( playeringame[ulIdx] == false )
					continue;

				if ( bFoundPlayer == false )
				{
					lHighestFragcount = players[ulIdx].fragcount;
					bFoundPlayer = true;
					continue;
				}
				else if ( players[ulIdx].fragcount > lHighestFragcount )
					lHighestFragcount = players[ulIdx].fragcount;
			}
		}

		if ( bFoundPlayer == false )
			ulFragsLeft = fraglimit;
		else
			ulFragsLeft = fraglimit - lHighestFragcount;
		sprintf( szString, "%ld frag%s remain%s", ulFragsLeft, ( ulFragsLeft != 1 ) ? "s" : "", ( ulFragsLeft == 1 ) ? "s" : "" );
		switch ( ulLine )
		{
		case 0:
			
			lTextBox = IDC_SCOREBOARD1;
			break;
		case 1:
			
			lTextBox = IDC_SCOREBOARD2;
			break;
		case 2:
			
			lTextBox = IDC_SCOREBOARD3;
			break;
		case 3:
			
			lTextBox = IDC_SCOREBOARD4;
			break;
		default:

			lTextBox = IDC_SCOREBOARD4;
		}
		SetDlgItemText( g_hDlg, lTextBox, szString );
		ulLine++;
	}

	// Render the duellimit string.
	if ( duellimit && duel && gamestate == GS_LEVEL )
	{
		LONG	lNumDuels;

		// Get the number of duels that have been played.
		lNumDuels = DUEL_GetNumDuels( );

		sprintf( szString, "%ld duel%s remain%s", duellimit - lNumDuels, (( duellimit - lNumDuels ) == 1 ) ? "" : "s", (( duellimit - lNumDuels ) == 1 ) ? "s" : "" );

		switch ( ulLine )
		{
		case 0:
			
			lTextBox = IDC_SCOREBOARD1;
			break;
		case 1:
			
			lTextBox = IDC_SCOREBOARD2;
			break;
		case 2:
			
			lTextBox = IDC_SCOREBOARD3;
			break;
		case 3:
			
			lTextBox = IDC_SCOREBOARD4;
			break;
		default:

			lTextBox = IDC_SCOREBOARD4;
		}
		SetDlgItemText( g_hDlg, lTextBox, szString );
		ulLine++;
	}

	// Render the "wins" string.
	if ( duel && gamestate == GS_LEVEL )
	{
		LONG	lWinner = -1;
		ULONG	ulIdx;
		bool	bDraw = true;

		for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
		{
			if ( playeringame[ulIdx] && players[ulIdx].ulWins )
			{
				lWinner = ulIdx;
				break;
			}
		}

		if ( lWinner == -1 )
		{
			if ( DUEL_CountActiveDuelers( ) == 2 )
				sprintf( szString, "First match between the two" );
			else
				bDraw = false;
		}
		else
			sprintf( szString, "Champion is %s \\c-with %ld win%s", players[lWinner].userinfo.netname, players[lWinner].ulWins, players[lWinner].ulWins == 1 ? "" : "s" );

		if ( bDraw )
		{
			V_ColorizeString( szString );
			V_RemoveColorCodes( szString );
			switch ( ulLine )
			{
			case 0:

				lTextBox = IDC_SCOREBOARD1;
				break;
			case 1:

				lTextBox = IDC_SCOREBOARD2;
				break;
			case 2:

				lTextBox = IDC_SCOREBOARD3;
				break;
			case 3:

				lTextBox = IDC_SCOREBOARD4;
				break;
			default:

				lTextBox = IDC_SCOREBOARD4;
			}
			SetDlgItemText( g_hDlg, lTextBox, szString );
			ulLine++;
		}
	}

	// Render the pointlimit string.
	if ( teamgame && pointlimit && gamestate == GS_LEVEL )
	{
		ULONG	ulPointsLeft;
		ULONG	ulBluePoints;
		ULONG	ulRedPoints;

		ulBluePoints = TEAM_GetScore( TEAM_BLUE );
		ulRedPoints = TEAM_GetScore( TEAM_RED );

		ulPointsLeft = pointlimit - (( ulBluePoints >= ulRedPoints ) ? ulBluePoints : ulRedPoints );
		sprintf( szString, "%ld points remain", ulPointsLeft );
		switch ( ulLine )
		{
		case 0:
			
			lTextBox = IDC_SCOREBOARD1;
			break;
		case 1:
			
			lTextBox = IDC_SCOREBOARD2;
			break;
		case 2:
			
			lTextBox = IDC_SCOREBOARD3;
			break;
		case 3:
			
			lTextBox = IDC_SCOREBOARD4;
			break;
		default:

			lTextBox = IDC_SCOREBOARD4;
		}
		SetDlgItemText( g_hDlg, lTextBox, szString );
		ulLine++;
	}

	// Render the winlimit string.
	if (( lastmanstanding || teamlms ) && winlimit && gamestate == GS_LEVEL )
	{
		bool	bFoundPlayer = false;
		LONG	lHighestWincount;
		ULONG	ulWinsLeft;

		// If we're in a teamplay, just go by whichever team has the most frags.
		if ( teamlms )
		{
			if ( TEAM_GetScore( TEAM_BLUE ) >= TEAM_GetScore( TEAM_RED ))
				lHighestWincount = TEAM_GetScore( TEAM_BLUE );
			else
				lHighestWincount = TEAM_GetScore( TEAM_RED );
		}
		// Otherwise, find the player with the most frags.
		else
		{
			for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
			{
				if ( playeringame[ulIdx] == false )
					continue;

				if ( bFoundPlayer == false )
				{
					lHighestWincount = players[ulIdx].ulWins;
					bFoundPlayer = true;
					continue;
				}
				else if ( players[ulIdx].ulWins > (ULONG)lHighestWincount )
					lHighestWincount = players[ulIdx].ulWins;
			}
		}

		if ( bFoundPlayer == false )
			ulWinsLeft = winlimit;
		else
			ulWinsLeft = winlimit - lHighestWincount;
		sprintf( szString, "%ld win%s remain%s", ulWinsLeft, ( ulWinsLeft != 1 ) ? "s" : "", ( ulWinsLeft == 1 ) ? "s" : "" );
		switch ( ulLine )
		{
		case 0:
			
			lTextBox = IDC_SCOREBOARD1;
			break;
		case 1:
			
			lTextBox = IDC_SCOREBOARD2;
			break;
		case 2:
			
			lTextBox = IDC_SCOREBOARD3;
			break;
		case 3:
			
			lTextBox = IDC_SCOREBOARD4;
			break;
		default:

			lTextBox = IDC_SCOREBOARD4;
		}
		SetDlgItemText( g_hDlg, lTextBox, szString );
		ulLine++;
	}

	// Render the timelimit string.
	if (( deathmatch || teamgame ) && timelimit && gamestate == GS_LEVEL )
	{
		LONG	lTimeLeft = (LONG)( timelimit * ( TICRATE * 60 )) - level.time;
		ULONG	ulHours;
		ULONG	ulMinutes;
		ULONG	ulSeconds;

		if ( lTimeLeft <= 0 )
			ulHours = ulMinutes = ulSeconds = 0;
		else
		{
			ulHours = lTimeLeft / ( TICRATE * 3600 );
			lTimeLeft -= ulHours * TICRATE * 3600;
			ulMinutes = lTimeLeft / ( TICRATE * 60 );
			lTimeLeft -= ulMinutes * TICRATE * 60;
			ulSeconds = lTimeLeft / TICRATE;
		}

		if ( lastmanstanding || teamlms )
		{
			if ( ulHours )
				sprintf( szString, "Round ends in %02ld:%02ld:%02ld", ulHours, ulMinutes, ulSeconds );
			else
				sprintf( szString, "Round ends in %02ld:%02ld", ulMinutes, ulSeconds );
		}
		else
		{
			if ( ulHours )
				sprintf( szString, "Level ends in %02ld:%02ld:%02ld", ulHours, ulMinutes, ulSeconds );
			else
				sprintf( szString, "Level ends in %02ld:%02ld", ulMinutes, ulSeconds );
		}
		
		switch ( ulLine )
		{
		case 0:
			
			lTextBox = IDC_SCOREBOARD1;
			break;
		case 1:
			
			lTextBox = IDC_SCOREBOARD2;
			break;
		case 2:
			
			lTextBox = IDC_SCOREBOARD3;
			break;
		case 3:
			
			lTextBox = IDC_SCOREBOARD4;
			break;
		default:

			lTextBox = IDC_SCOREBOARD4;
		}
		SetDlgItemText( g_hDlg, lTextBox, szString );
		ulLine++;
	}

	// Render the current team scores.
	if ( teamplay || teamgame || teamlms )
	{
		if ( gamestate == GS_LEVEL )
		{
			if ( teamplay )
			{
				// If the teams are tied...
				if ( TEAM_GetFragCount( TEAM_RED ) == TEAM_GetFragCount( TEAM_BLUE ))
				{
					sprintf( szString, "Teams are tied at %ld\n", TEAM_GetFragCount( TEAM_RED ));
					switch ( ulLine )
					{
					case 0:
						
						lTextBox = IDC_SCOREBOARD1;
						break;
					case 1:
						
						lTextBox = IDC_SCOREBOARD2;
						break;
					case 2:
						
						lTextBox = IDC_SCOREBOARD3;
						break;
					case 3:
						
						lTextBox = IDC_SCOREBOARD4;
						break;
					default:

						lTextBox = IDC_SCOREBOARD4;
					}
					SetDlgItemText( g_hDlg, lTextBox, szString );
					ulLine++;
				}
				else
				{
					if ( TEAM_GetFragCount( TEAM_RED ) > TEAM_GetFragCount( TEAM_BLUE ))
						sprintf( szString, "%s leads %ld to %ld", TEAM_GetName( TEAM_RED ), TEAM_GetFragCount( TEAM_RED ), TEAM_GetFragCount( TEAM_BLUE ));
					else
						sprintf( szString, "%s leads %ld to %ld", TEAM_GetName( TEAM_BLUE ), TEAM_GetFragCount( TEAM_BLUE ), TEAM_GetFragCount( TEAM_RED ));

					switch ( ulLine )
					{
					case 0:
						
						lTextBox = IDC_SCOREBOARD1;
						break;
					case 1:
						
						lTextBox = IDC_SCOREBOARD2;
						break;
					case 2:
						
						lTextBox = IDC_SCOREBOARD3;
						break;
					case 3:
						
						lTextBox = IDC_SCOREBOARD4;
						break;
					default:

						lTextBox = IDC_SCOREBOARD4;
					}
					SetDlgItemText( g_hDlg, lTextBox, szString );
					ulLine++;
				}
			}
			else if ( teamlms )
			{
				// If the teams are tied...
				if ( TEAM_GetWinCount( TEAM_RED ) == TEAM_GetWinCount( TEAM_BLUE ))
				{
					sprintf( szString, "Teams are tied at %ld\n", TEAM_GetWinCount( TEAM_RED ));
					switch ( ulLine )
					{
					case 0:
						
						lTextBox = IDC_SCOREBOARD1;
						break;
					case 1:
						
						lTextBox = IDC_SCOREBOARD2;
						break;
					case 2:
						
						lTextBox = IDC_SCOREBOARD3;
						break;
					case 3:
						
						lTextBox = IDC_SCOREBOARD4;
						break;
					default:

						lTextBox = IDC_SCOREBOARD4;
					}
					SetDlgItemText( g_hDlg, lTextBox, szString );
					ulLine++;
				}
				else
				{
					if ( TEAM_GetWinCount( TEAM_RED ) > TEAM_GetWinCount( TEAM_BLUE ))
						sprintf( szString, "%s leads %ld to %ld", TEAM_GetName( TEAM_RED ), TEAM_GetWinCount( TEAM_RED ), TEAM_GetWinCount( TEAM_BLUE ));
					else
						sprintf( szString, "%s leads %ld to %ld", TEAM_GetName( TEAM_BLUE ), TEAM_GetWinCount( TEAM_BLUE ), TEAM_GetWinCount( TEAM_RED ));

					switch ( ulLine )
					{
					case 0:
						
						lTextBox = IDC_SCOREBOARD1;
						break;
					case 1:
						
						lTextBox = IDC_SCOREBOARD2;
						break;
					case 2:
						
						lTextBox = IDC_SCOREBOARD3;
						break;
					case 3:
						
						lTextBox = IDC_SCOREBOARD4;
						break;
					default:

						lTextBox = IDC_SCOREBOARD4;
					}
					SetDlgItemText( g_hDlg, lTextBox, szString );
					ulLine++;
				}
			}
			else
			{
				// If the teams are tied...
				if ( TEAM_GetScore( TEAM_RED ) == TEAM_GetScore( TEAM_BLUE ))
				{
					sprintf( szString, "Teams are tied at %ld\n", TEAM_GetScore( TEAM_RED ));
					switch ( ulLine )
					{
					case 0:
						
						lTextBox = IDC_SCOREBOARD1;
						break;
					case 1:
						
						lTextBox = IDC_SCOREBOARD2;
						break;
					case 2:
						
						lTextBox = IDC_SCOREBOARD3;
						break;
					case 3:
						
						lTextBox = IDC_SCOREBOARD4;
						break;
					default:

						lTextBox = IDC_SCOREBOARD4;
					}
					SetDlgItemText( g_hDlg, lTextBox, szString );
					ulLine++;
				}
				else
				{
					if ( TEAM_GetScore( TEAM_RED ) > TEAM_GetScore( TEAM_BLUE ))
						sprintf( szString, "%s leads %ld to %ld", TEAM_GetName( TEAM_RED ), TEAM_GetScore( TEAM_RED ), TEAM_GetScore( TEAM_BLUE ));
					else
						sprintf( szString, "%s leads %ld to %ld", TEAM_GetName( TEAM_BLUE ), TEAM_GetScore( TEAM_BLUE ), TEAM_GetScore( TEAM_RED ));

					switch ( ulLine )
					{
					case 0:
						
						lTextBox = IDC_SCOREBOARD1;
						break;
					case 1:
						
						lTextBox = IDC_SCOREBOARD2;
						break;
					case 2:
						
						lTextBox = IDC_SCOREBOARD3;
						break;
					case 3:
						
						lTextBox = IDC_SCOREBOARD4;
						break;
					default:

						lTextBox = IDC_SCOREBOARD4;
					}
					SetDlgItemText( g_hDlg, lTextBox, szString );
					ulLine++;
				}
			}
		}
		else
		{
			if ( teamplay )
			{
				// If the teams are tied...
				if ( TEAM_GetFragCount( TEAM_RED ) == TEAM_GetFragCount( TEAM_BLUE ))
				{
					sprintf( szString, "Teams tied at %ld\n", TEAM_GetFragCount( TEAM_RED ));
					switch ( ulLine )
					{
					case 0:
						
						lTextBox = IDC_SCOREBOARD1;
						break;
					case 1:
						
						lTextBox = IDC_SCOREBOARD2;
						break;
					case 2:
						
						lTextBox = IDC_SCOREBOARD3;
						break;
					case 3:
						
						lTextBox = IDC_SCOREBOARD4;
						break;
					default:

						lTextBox = IDC_SCOREBOARD4;
					}
					SetDlgItemText( g_hDlg, lTextBox, szString );
					ulLine++;
				}
				else
				{
					if ( TEAM_GetFragCount( TEAM_RED ) > TEAM_GetFragCount( TEAM_BLUE ))
						sprintf( szString, "%s has won %ld to %ld", TEAM_GetName( TEAM_RED ), TEAM_GetFragCount( TEAM_RED ), TEAM_GetFragCount( TEAM_BLUE ));
					else
						sprintf( szString, "%s has won %ld to %ld", TEAM_GetName( TEAM_BLUE ), TEAM_GetFragCount( TEAM_BLUE ), TEAM_GetFragCount( TEAM_RED ));

					switch ( ulLine )
					{
					case 0:
						
						lTextBox = IDC_SCOREBOARD1;
						break;
					case 1:
						
						lTextBox = IDC_SCOREBOARD2;
						break;
					case 2:
						
						lTextBox = IDC_SCOREBOARD3;
						break;
					case 3:
						
						lTextBox = IDC_SCOREBOARD4;
						break;
					default:

						lTextBox = IDC_SCOREBOARD4;
					}
					SetDlgItemText( g_hDlg, lTextBox, szString );
					ulLine++;
				}
			}
			else
			{
				// If the teams are tied...
				if ( TEAM_GetScore( TEAM_RED ) == TEAM_GetScore( TEAM_BLUE ))
				{
					sprintf( szString, "Teams tied at %ld\n", TEAM_GetScore( TEAM_RED ));
					switch ( ulLine )
					{
					case 0:
						
						lTextBox = IDC_SCOREBOARD1;
						break;
					case 1:
						
						lTextBox = IDC_SCOREBOARD2;
						break;
					case 2:
						
						lTextBox = IDC_SCOREBOARD3;
						break;
					case 3:
						
						lTextBox = IDC_SCOREBOARD4;
						break;
					default:

						lTextBox = IDC_SCOREBOARD4;
					}
					SetDlgItemText( g_hDlg, lTextBox, szString );
					ulLine++;
				}
				else
				{
					if ( TEAM_GetScore( TEAM_RED ) > TEAM_GetScore( TEAM_BLUE ))
						sprintf( szString, "%s has won %ld to %ld", TEAM_GetName( TEAM_RED ), TEAM_GetScore( TEAM_RED ), TEAM_GetScore( TEAM_BLUE ));
					else
						sprintf( szString, "%s has won %ld to %ld", TEAM_GetName( TEAM_BLUE ), TEAM_GetScore( TEAM_BLUE ), TEAM_GetScore( TEAM_RED ));

					switch ( ulLine )
					{
					case 0:
						
						lTextBox = IDC_SCOREBOARD1;
						break;
					case 1:
						
						lTextBox = IDC_SCOREBOARD2;
						break;
					case 2:
						
						lTextBox = IDC_SCOREBOARD3;
						break;
					case 3:
						
						lTextBox = IDC_SCOREBOARD4;
						break;
					default:

						lTextBox = IDC_SCOREBOARD4;
					}
					SetDlgItemText( g_hDlg, lTextBox, szString );
					ulLine++;
				}
			}
		}
	}

	// Render the number of monsters left in coop.
	if (( deathmatch == false ) && ( teamgame == false ) && ( gamestate == GS_LEVEL ))
	{
		LONG	lNumMonstersRemaining;

		lNumMonstersRemaining = level.total_monsters - level.killed_monsters;
		sprintf( szString, "%ld monster%s remaining", lNumMonstersRemaining, lNumMonstersRemaining == 1 ? "" : "s" );
		switch ( ulLine )
		{
		case 0:
			
			lTextBox = IDC_SCOREBOARD1;
			break;
		case 1:
			
			lTextBox = IDC_SCOREBOARD2;
			break;
		case 2:
			
			lTextBox = IDC_SCOREBOARD3;
			break;
		case 3:
			
			lTextBox = IDC_SCOREBOARD4;
			break;
		default:

			lTextBox = IDC_SCOREBOARD4;
		}
		SetDlgItemText( g_hDlg, lTextBox, szString );
		ulLine++;
	}
}

//*****************************************************************************
//
void SERVERCONSOLE_UpdateTotalOutboundDataTransfer( LONG lData )
{
	char	szString[256];

	if ( g_hStatisticDlg == NULL )
		return;

	if ( lData > GIGABYTE )
		sprintf( szString, "Total data transfer (out) %0.2fKB (%0.2fGB)", (float)lData / (float)KILOBYTE, (float)lData / GIGABYTE );
	else if ( lData > MEGABYTE )
		sprintf( szString, "Total data transfer (out): %0.2fKB (%0.2fMB)", (float)lData / (float)KILOBYTE, (float)lData / MEGABYTE );
	else
		sprintf( szString, "Total data transfer (out): %0.2fKB", (float)lData / (float)KILOBYTE );

	SetDlgItemText( g_hStatisticDlg, IDC_TOTALOUTBOUNDDATATRANSFER, szString );
}

//*****************************************************************************
//
void SERVERCONSOLE_UpdateAverageOutboundDataTransfer( LONG lData )
{
	char	szString[256];
	float	fDataPerSecond;

	if ( g_hStatisticDlg == NULL )
		return;

	if ( SERVER_STATISTIC_GetTotalSecondsElapsed( ) == 0 )
		sprintf( szString, "Average data transfer (out): 0B/s" );
	else
	{
		fDataPerSecond = (float)lData / (float)SERVER_STATISTIC_GetTotalSecondsElapsed( );
		if ( fDataPerSecond > KILOBYTE )
			sprintf( szString, "Average data transfer (out): %0.2fB/s (%0.2fKB/s)", fDataPerSecond, fDataPerSecond / (float)KILOBYTE );
		else
			sprintf( szString, "Average data transfer (out): %0.2fB/s", fDataPerSecond / (float)KILOBYTE );
	}

	SetDlgItemText( g_hStatisticDlg, IDC_AVERAGEOUTBOUNDDATATRANSFER, szString );
}

//*****************************************************************************
//
void SERVERCONSOLE_UpdatePeakOutboundDataTransfer( LONG lData )
{
	char	szString[256];

	if ( g_hStatisticDlg == NULL )
		return;

	if ( lData > KILOBYTE )
		sprintf( szString, "Peak data transfer (out): %ldB/s (%0.2fKB/s)", lData, (float)lData / (float)KILOBYTE );
	else
		sprintf( szString, "Peak data transfer (out): %ldB/s", lData );

	SetDlgItemText( g_hStatisticDlg, IDC_PEAKOUTBOUNDDATATRANSFER, szString );
}

//*****************************************************************************
//
void SERVERCONSOLE_UpdateCurrentOutboundDataTransfer( LONG lData )
{
	char	szString[256];

	if ( g_hStatisticDlg == NULL )
		return;

	if ( lData > KILOBYTE )
		sprintf( szString, "Current data transfer (out): %ldB/s (%0.2fKB/s)", lData, (float)lData / (float)KILOBYTE );
	else
		sprintf( szString, "Current data transfer (out): %ldB/s", lData );

	SetDlgItemText( g_hStatisticDlg, IDC_CURRENTOUTBOUNDDATATRANSFER, szString );
}

//*****************************************************************************
//
void SERVERCONSOLE_UpdateTotalInboundDataTransfer( LONG lData )
{
	char	szString[256];

	if ( g_hStatisticDlg == NULL )
		return;

	if ( lData > GIGABYTE )
		sprintf( szString, "Total data transfer (in): %0.2fKB (%0.2fGB)", (float)lData / (float)KILOBYTE, (float)lData / GIGABYTE );
	else if ( lData > MEGABYTE )
		sprintf( szString, "Total data transfer (in): %0.2fKB (%0.2fMB)", (float)lData / (float)KILOBYTE, (float)lData / MEGABYTE );
	else
		sprintf( szString, "Total data transfer (in): %0.2fKB", (float)lData / (float)KILOBYTE );

	SetDlgItemText( g_hStatisticDlg, IDC_TOTALINBOUNDDATATRANSFER, szString );
}

//*****************************************************************************
//
void SERVERCONSOLE_UpdateAverageInboundDataTransfer( LONG lData )
{
	char	szString[256];
	float	fDataPerSecond;

	if ( g_hStatisticDlg == NULL )
		return;

	if ( SERVER_STATISTIC_GetTotalSecondsElapsed( ) == 0 )
		sprintf( szString, "Average data transfer (in): 0B/s" );
	else
	{
		fDataPerSecond = (float)lData / (float)SERVER_STATISTIC_GetTotalSecondsElapsed( );
		if ( fDataPerSecond > KILOBYTE )
			sprintf( szString, "Average data transfer (in): %0.2fB/s (%0.2fKB/s)", fDataPerSecond, fDataPerSecond / (float)KILOBYTE );
		else
			sprintf( szString, "Average data transfer (in): %0.2fB/s", fDataPerSecond / (float)KILOBYTE );
	}

	SetDlgItemText( g_hStatisticDlg, IDC_AVERAGEINBOUNDDATATRANSFER, szString );
}

//*****************************************************************************
//
void SERVERCONSOLE_UpdatePeakInboundDataTransfer( LONG lData )
{
	char	szString[256];

	if ( g_hStatisticDlg == NULL )
		return;

	if ( lData > KILOBYTE )
		sprintf( szString, "Peak data transfer (in): %ldB/s (%0.2fKB/s)", lData, (float)lData / (float)KILOBYTE );
	else
		sprintf( szString, "Peak data transfer (in): %ldB/s", lData );

	SetDlgItemText( g_hStatisticDlg, IDC_PEAKINBOUNDDATATRANSFER, szString );
}

//*****************************************************************************
//
void SERVERCONSOLE_UpdateCurrentInboundDataTransfer( LONG lData )
{
	char	szString[256];

	if ( g_hStatisticDlg == NULL )
		return;

	if ( lData > KILOBYTE )
		sprintf( szString, "Current data transfer (in): %ldB/s (%0.2fKB/s)", lData, (float)lData / (float)KILOBYTE );
	else
		sprintf( szString, "Current data transfer (in): %ldB/s", lData );

	SetDlgItemText( g_hStatisticDlg, IDC_CURRENTINBOUNDDATATRANSFER, szString );
}

//*****************************************************************************
//
void SERVERCONSOLE_UpdateTotalUptime( LONG lData )
{
	char	szString[256];

	if ( g_hStatisticDlg == NULL )
		return;

	if ( lData >= DAY )
		sprintf( szString, "Total uptime: %ldd%ldh%ldm%lds", lData / DAY, ( lData / HOUR ) % 24, ( lData / MINUTE ) % 60, lData % MINUTE );
	else if ( lData >= HOUR )
		sprintf( szString, "Total uptime: %ldh%ldm%lds", ( lData / HOUR ) % 24, ( lData / MINUTE ) % 60, lData % MINUTE  );
	else if ( lData >= MINUTE )
		sprintf( szString, "Total uptime: %ldm%lds", ( lData / MINUTE ) % 60, lData % MINUTE  );
	else
		sprintf( szString, "Total uptime: %lds", lData );
	SetDlgItemText( g_hStatisticDlg, IDC_TOTALUPTIME, szString );
}

//*****************************************************************************
//
void SERVERCONSOLE_SetCurrentMapname( char *pszString )
{
	SendMessage(g_hDlgStatusBar, SB_SETTEXT, (WPARAM)3, (LPARAM)pszString);
}

//*****************************************************************************
//
void SERVERCONSOLE_SetupColumns( void )
{
	LVCOLUMN	ColumnData;

	if ( SendDlgItemMessage( g_hDlg, IDC_PLAYERLIST, LVM_GETCOLUMN, COLUMN_FRAGS, (LPARAM)&ColumnData ))
	{
		ColumnData.mask = LVCF_TEXT;

		if ( lastmanstanding )
			ColumnData.pszText = "Wins";
		else if ( deathmatch )
			ColumnData.pszText = "Frags";
		else if ( teamgame )
			ColumnData.pszText = "Points";
		else
			ColumnData.pszText = "Kills";

		SendDlgItemMessage( g_hDlg, IDC_PLAYERLIST, LVM_SETCOLUMN, COLUMN_FRAGS, (LPARAM)&ColumnData );
	}
	else
		Printf( "SERVERCONSOLE_SetupColumns: Couldn't get column!\n" );
}

//*****************************************************************************
//
void SERVERCONSOLE_ReListPlayers( void )
{
	LVITEM		Item;
	char		szString[32];
	LONG		lIndex;
	LONG		lIdx;

	// Find the player in the global player indicies array.
	for ( lIdx = 0; lIdx < MAXPLAYERS; lIdx++ )
	{
		g_lPlayerIndicies[lIdx] = -1;

		// Delete the list view item.
		SendDlgItemMessage( g_hDlg, IDC_PLAYERLIST, LVM_DELETEITEM, 0, 0 );
	}

	Item.mask = LVIF_TEXT;
	Item.iSubItem = COLUMN_NAME;
	Item.iItem = MAXPLAYERS;

	// Add each player.
	for ( lIdx = 0; lIdx < MAXPLAYERS; lIdx++ )
	{
		if ( playeringame[lIdx] == false )
			continue;

		sprintf( szString, "%s", players[lIdx].userinfo.netname );
		V_RemoveColorCodes( szString );
		Item.pszText = szString;

		lIndex = SendDlgItemMessage( g_hDlg, IDC_PLAYERLIST, LVM_INSERTITEM, 0, (LPARAM)&Item );
		if ( lIndex == -1 )
			return;

		g_lPlayerIndicies[lIndex] = lIdx;

		// Initialize all the fields for this player.
		SERVERCONSOLE_UpdatePlayerInfo( lIdx, UDF_NAME|UDF_FRAGS|UDF_PING|UDF_TIME );
	}
}

//*****************************************************************************
//
void SERVERCONSOLE_UpdatePlayerInfo( LONG lPlayer, ULONG ulUpdateFlags )
{
	LVITEM		Item;
	char		szString[32];
	LONG		lIndex = -1;
	LONG		lIdx;

	for ( lIdx = 0; lIdx < MAXPLAYERS; lIdx++ )
	{
		if ( g_lPlayerIndicies[lIdx] == lPlayer )
		{
			lIndex = lIdx;
			break;
		}
	}

	if ( lIndex == -1 )
		return;

	Item.mask = LVIF_TEXT;
	Item.iItem = lIndex;

	if ( ulUpdateFlags & UDF_NAME )
	{
		Item.iSubItem = COLUMN_NAME;
		sprintf( szString, "%s", players[lPlayer].userinfo.netname );
		Item.pszText = szString;
		V_RemoveColorCodes( szString );

		SendDlgItemMessage( g_hDlg, IDC_PLAYERLIST, LVM_SETITEM, lIndex, (LPARAM)&Item );
	}

	if ( ulUpdateFlags & UDF_FRAGS )
	{
		Item.iSubItem = COLUMN_FRAGS;
		if ( PLAYER_IsTrueSpectator( &players[lPlayer] ))
			sprintf( szString, "Spectating" );
		else if (( teamgame || teamplay || teamlms ) && ( players[lPlayer].bOnTeam == false ))
			sprintf( szString, "No team" );
		else if ( lastmanstanding || teamlms )
		{
			if ( players[lPlayer].health <=0 )
				sprintf( szString, "Dead" );
			else if ( lastmanstanding )
				sprintf( szString, "%ld", players[lPlayer].ulWins );
			else
				sprintf( szString, "%d", players[lPlayer].fragcount );
		}
		else if ( possession || teampossession )
			sprintf( szString, "%ld", players[lPlayer].lPointCount );
		else if ( deathmatch )
			sprintf( szString, "%d", players[lPlayer].fragcount );
		else if ( teamgame )
			sprintf( szString, "%ld", players[lPlayer].lPointCount );
		else
			sprintf( szString, "%d", players[lPlayer].killcount );
		Item.pszText = szString;

		SendDlgItemMessage( g_hDlg, IDC_PLAYERLIST, LVM_SETITEM, lIndex, (LPARAM)&Item );
	}

	if ( ulUpdateFlags & UDF_PING )
	{
		Item.iSubItem = COLUMN_PING;
		if ( players[lPlayer].bIsBot )
			sprintf( szString, "Bot" );
		else
			sprintf( szString, "%ld", players[lPlayer].ulPing );
		Item.pszText = szString;

		SendDlgItemMessage( g_hDlg, IDC_PLAYERLIST, LVM_SETITEM, lIndex, (LPARAM)&Item );
	}

	if ( ulUpdateFlags & UDF_TIME )
	{
		Item.iSubItem = COLUMN_TIME;
		sprintf( szString, "%ld", ( players[lPlayer].ulTime / ( TICRATE * 60 )));
		Item.pszText = szString;

		SendDlgItemMessage( g_hDlg, IDC_PLAYERLIST, LVM_SETITEM, lIndex, (LPARAM)&Item );
	}
}

//*****************************************************************************
//
void SERVERCONSOLE_InitializeDMFlagsDisplay( HWND hDlg )
{
	UCVarValue	Val;

	// DMFLAGS
	if ( dmflags & DF_FORCE_FALLINGZD )
		SendDlgItemMessage( hDlg, IDC_FORCE_FALLINGZD, BM_SETCHECK, BST_CHECKED, 0 );
	else
		SendDlgItemMessage( hDlg, IDC_FORCE_FALLINGZD, BM_SETCHECK, BST_UNCHECKED, 0 );

	if ( dmflags & DF_FORCE_FALLINGHX )
		SendDlgItemMessage( hDlg, IDC_FORCE_FALLINGHX, BM_SETCHECK, BST_CHECKED, 0 );
	else
		SendDlgItemMessage( hDlg, IDC_FORCE_FALLINGHX, BM_SETCHECK, BST_UNCHECKED, 0 );

	if ( dmflags & DF_WEAPONS_STAY )
		SendDlgItemMessage( hDlg, IDC_WEAPONS_STAY, BM_SETCHECK, BST_CHECKED, 0 );
	else
		SendDlgItemMessage( hDlg, IDC_WEAPONS_STAY, BM_SETCHECK, BST_UNCHECKED, 0 );

	if ( dmflags & DF_NO_ITEMS )
		SendDlgItemMessage( hDlg, IDC_NO_ITEMS, BM_SETCHECK, BST_CHECKED, 0 );
	else
		SendDlgItemMessage( hDlg, IDC_NO_ITEMS, BM_SETCHECK, BST_UNCHECKED, 0 );

	if ( dmflags & DF_NO_HEALTH )
		SendDlgItemMessage( hDlg, IDC_NO_HEALTH, BM_SETCHECK, BST_CHECKED, 0 );
	else
		SendDlgItemMessage( hDlg, IDC_NO_HEALTH, BM_SETCHECK, BST_UNCHECKED, 0 );

	if ( dmflags & DF_NO_ARMOR )
		SendDlgItemMessage( hDlg, IDC_NO_ARMOR, BM_SETCHECK, BST_CHECKED, 0 );
	else
		SendDlgItemMessage( hDlg, IDC_NO_ARMOR, BM_SETCHECK, BST_UNCHECKED, 0 );

	if ( dmflags & DF_SPAWN_FARTHEST )
		SendDlgItemMessage( hDlg, IDC_SPAWN_FARTHEST, BM_SETCHECK, BST_CHECKED, 0 );
	else
		SendDlgItemMessage( hDlg, IDC_SPAWN_FARTHEST, BM_SETCHECK, BST_UNCHECKED, 0 );

	if ( dmflags & DF_SAME_LEVEL )
		SendDlgItemMessage( hDlg, IDC_SAME_LEVEL, BM_SETCHECK, BST_CHECKED, 0 );
	else
		SendDlgItemMessage( hDlg, IDC_SAME_LEVEL, BM_SETCHECK, BST_UNCHECKED, 0 );

	if ( dmflags & DF_FORCE_RESPAWN )
		SendDlgItemMessage( hDlg, IDC_FORCE_RESPAWN, BM_SETCHECK, BST_CHECKED, 0 );
	else
		SendDlgItemMessage( hDlg, IDC_FORCE_RESPAWN, BM_SETCHECK, BST_UNCHECKED, 0 );

	if ( dmflags & DF_NO_EXIT )
		SendDlgItemMessage( hDlg, IDC_NO_EXIT, BM_SETCHECK, BST_CHECKED, 0 );
	else
		SendDlgItemMessage( hDlg, IDC_NO_EXIT, BM_SETCHECK, BST_UNCHECKED, 0 );

	if ( dmflags & DF_INFINITE_AMMO )
		SendDlgItemMessage( hDlg, IDC_INFINITE_AMMO, BM_SETCHECK, BST_CHECKED, 0 );
	else
		SendDlgItemMessage( hDlg, IDC_INFINITE_AMMO, BM_SETCHECK, BST_UNCHECKED, 0 );

	if ( dmflags & DF_NO_MONSTERS )
		SendDlgItemMessage( hDlg, IDC_NO_MONSTERS, BM_SETCHECK, BST_CHECKED, 0 );
	else
		SendDlgItemMessage( hDlg, IDC_NO_MONSTERS, BM_SETCHECK, BST_UNCHECKED, 0 );

	if ( dmflags & DF_MONSTERS_RESPAWN )
		SendDlgItemMessage( hDlg, IDC_MONSTERS_RESPAWN, BM_SETCHECK, BST_CHECKED, 0 );
	else
		SendDlgItemMessage( hDlg, IDC_MONSTERS_RESPAWN, BM_SETCHECK, BST_UNCHECKED, 0 );

	if ( dmflags & DF_ITEMS_RESPAWN )
		SendDlgItemMessage( hDlg, IDC_ITEMS_RESPAWN, BM_SETCHECK, BST_CHECKED, 0 );
	else
		SendDlgItemMessage( hDlg, IDC_ITEMS_RESPAWN, BM_SETCHECK, BST_UNCHECKED, 0 );

	if ( dmflags & DF_RESPAWN_SUPER )
		SendDlgItemMessage( hDlg, IDC_RESPAWN_SUPER, BM_SETCHECK, BST_CHECKED, 0 );
	else
		SendDlgItemMessage( hDlg, IDC_RESPAWN_SUPER, BM_SETCHECK, BST_UNCHECKED, 0 );

	if ( dmflags & DF_FAST_MONSTERS )
		SendDlgItemMessage( hDlg, IDC_FAST_MONSTERS, BM_SETCHECK, BST_CHECKED, 0 );
	else
		SendDlgItemMessage( hDlg, IDC_FAST_MONSTERS, BM_SETCHECK, BST_UNCHECKED, 0 );

	if ( dmflags & DF_NO_JUMP )
		SendDlgItemMessage( hDlg, IDC_NO_JUMP, BM_SETCHECK, BST_CHECKED, 0 );
	else
		SendDlgItemMessage( hDlg, IDC_NO_JUMP, BM_SETCHECK, BST_UNCHECKED, 0 );

	if ( dmflags & DF_NO_FREELOOK )
		SendDlgItemMessage( hDlg, IDC_NO_FREELOOK, BM_SETCHECK, BST_CHECKED, 0 );
	else
		SendDlgItemMessage( hDlg, IDC_NO_FREELOOK, BM_SETCHECK, BST_UNCHECKED, 0 );

	if ( dmflags & DF_NO_FOV )
		SendDlgItemMessage( hDlg, IDC_NO_FOV, BM_SETCHECK, BST_CHECKED, 0 );
	else
		SendDlgItemMessage( hDlg, IDC_NO_FOV, BM_SETCHECK, BST_UNCHECKED, 0 );

	if ( dmflags & DF_NO_COOP_WEAPON_SPAWN )
		SendDlgItemMessage( hDlg, IDC_NO_COOP_MP_WEAPON_SPAWN, BM_SETCHECK, BST_CHECKED, 0 );
	else
		SendDlgItemMessage( hDlg, IDC_NO_COOP_MP_WEAPON_SPAWN, BM_SETCHECK, BST_UNCHECKED, 0 );

	if ( dmflags & DF_NO_CROUCH )
		SendDlgItemMessage( hDlg, IDC_DISALLOW_CROUCH, BM_SETCHECK, BST_CHECKED, 0 );
	else
		SendDlgItemMessage( hDlg, IDC_DISALLOW_CROUCH, BM_SETCHECK, BST_UNCHECKED, 0 );

	if ( dmflags & DF_COOP_LOSE_INVENTORY )
		SendDlgItemMessage( hDlg, IDC_COOP_LOSE_INVENTORY, BM_SETCHECK, BST_CHECKED, 0 );
	else
		SendDlgItemMessage( hDlg, IDC_COOP_LOSE_INVENTORY, BM_SETCHECK, BST_UNCHECKED, 0 );

	if ( dmflags & DF_COOP_LOSE_KEYS )
		SendDlgItemMessage( hDlg, IDC_KEEP_KEYS, BM_SETCHECK, BST_CHECKED, 0 );
	else
		SendDlgItemMessage( hDlg, IDC_KEEP_KEYS, BM_SETCHECK, BST_UNCHECKED, 0 );

	if ( dmflags & DF_COOP_LOSE_WEAPONS )
		SendDlgItemMessage( hDlg, IDC_KEEP_WEAPONS, BM_SETCHECK, BST_CHECKED, 0 );
	else
		SendDlgItemMessage( hDlg, IDC_KEEP_WEAPONS, BM_SETCHECK, BST_UNCHECKED, 0 );

	if ( dmflags & DF_COOP_LOSE_ARMOR )
		SendDlgItemMessage( hDlg, IDC_COOP_LOSE_ARMOR, BM_SETCHECK, BST_CHECKED, 0 );
	else
		SendDlgItemMessage( hDlg, IDC_COOP_LOSE_ARMOR, BM_SETCHECK, BST_UNCHECKED, 0 );

	if ( dmflags & DF_COOP_LOSE_POWERUPS )
		SendDlgItemMessage( hDlg, IDC_COOP_LOSE_POWERUPS, BM_SETCHECK, BST_CHECKED, 0 );
	else
		SendDlgItemMessage( hDlg, IDC_COOP_LOSE_POWERUPS, BM_SETCHECK, BST_UNCHECKED, 0 );

	if ( dmflags & DF_COOP_LOSE_AMMO )
		SendDlgItemMessage( hDlg, IDC_COOP_LOSE_AMMO, BM_SETCHECK, BST_CHECKED, 0 );
	else
		SendDlgItemMessage( hDlg, IDC_COOP_LOSE_AMMO, BM_SETCHECK, BST_UNCHECKED, 0 );

	if ( dmflags & DF_COOP_HALVE_AMMO )
		SendDlgItemMessage( hDlg, IDC_COOP_HALVE_AMMO, BM_SETCHECK, BST_CHECKED, 0 );
	else
		SendDlgItemMessage( hDlg, IDC_COOP_HALVE_AMMO, BM_SETCHECK, BST_UNCHECKED, 0 );

	// DMFLAGS2
	if ( dmflags2 & DF2_YES_WEAPONDROP )
		SendDlgItemMessage( hDlg, IDC_YES_WEAPONDROP, BM_SETCHECK, BST_CHECKED, 0 );
	else
		SendDlgItemMessage( hDlg, IDC_YES_WEAPONDROP, BM_SETCHECK, BST_UNCHECKED, 0 );

	if ( dmflags2 & DF2_NO_RUNES )
		SendDlgItemMessage( hDlg, IDC_NO_RUNES, BM_SETCHECK, BST_CHECKED, 0 );
	else
		SendDlgItemMessage( hDlg, IDC_NO_RUNES, BM_SETCHECK, BST_UNCHECKED, 0 );

	if ( dmflags2 & DF2_INSTANT_RETURN )
		SendDlgItemMessage( hDlg, IDC_INSTANT_RETURN, BM_SETCHECK, BST_CHECKED, 0 );
	else
		SendDlgItemMessage( hDlg, IDC_INSTANT_RETURN, BM_SETCHECK, BST_UNCHECKED, 0 );

	if ( dmflags2 & DF2_NO_TEAM_SWITCH )
		SendDlgItemMessage( hDlg, IDC_NO_TEAM_SWITCH, BM_SETCHECK, BST_CHECKED, 0 );
	else
		SendDlgItemMessage( hDlg, IDC_NO_TEAM_SWITCH, BM_SETCHECK, BST_UNCHECKED, 0 );

	if ( dmflags2 & DF2_NO_TEAM_SELECT )
		SendDlgItemMessage( hDlg, IDC_NO_TEAM_SELECT, BM_SETCHECK, BST_CHECKED, 0 );
	else
		SendDlgItemMessage( hDlg, IDC_NO_TEAM_SELECT, BM_SETCHECK, BST_UNCHECKED, 0 );

	if ( dmflags2 & DF2_YES_DOUBLEAMMO )
		SendDlgItemMessage( hDlg, IDC_YES_DOUBLEAMMO, BM_SETCHECK, BST_CHECKED, 0 );
	else
		SendDlgItemMessage( hDlg, IDC_YES_DOUBLEAMMO, BM_SETCHECK, BST_UNCHECKED, 0 );

	if ( dmflags2 & DF2_YES_DEGENERATION )
		SendDlgItemMessage( hDlg, IDC_YES_DEGENERATION, BM_SETCHECK, BST_CHECKED, 0 );
	else
		SendDlgItemMessage( hDlg, IDC_YES_DEGENERATION, BM_SETCHECK, BST_UNCHECKED, 0 );

	if ( dmflags2 & DF2_YES_FREEAIMBFG )
		SendDlgItemMessage( hDlg, IDC_YES_FREEAIMBFG, BM_SETCHECK, BST_CHECKED, 0 );
	else
		SendDlgItemMessage( hDlg, IDC_YES_FREEAIMBFG, BM_SETCHECK, BST_UNCHECKED, 0 );

	if ( dmflags2 & DF2_BARRELS_RESPAWN )
		SendDlgItemMessage( hDlg, IDC_BARRELS_RESPAWN, BM_SETCHECK, BST_CHECKED, 0 );
	else
		SendDlgItemMessage( hDlg, IDC_BARRELS_RESPAWN, BM_SETCHECK, BST_UNCHECKED, 0 );

	if ( dmflags2 & DF2_NO_RESPAWN_INVUL )
		SendDlgItemMessage( hDlg, IDC_NO_RESPAWN_INVUL, BM_SETCHECK, BST_CHECKED, 0 );
	else
		SendDlgItemMessage( hDlg, IDC_NO_RESPAWN_INVUL, BM_SETCHECK, BST_UNCHECKED, 0 );


	if ( dmflags2 & DF2_COOP_SHOTGUNSTART )
		SendDlgItemMessage( hDlg, IDC_SHOTGUN_START, BM_SETCHECK, BST_CHECKED, 0 );
	else
		SendDlgItemMessage( hDlg, IDC_SHOTGUN_START, BM_SETCHECK, BST_UNCHECKED, 0 );

	if ( dmflags2 & DF2_SAME_SPAWN_SPOT )
		SendDlgItemMessage( hDlg, IDC_SAME_SPAWN_SPOT, BM_SETCHECK, BST_CHECKED, 0 );
	else
		SendDlgItemMessage( hDlg, IDC_SAME_SPAWN_SPOT, BM_SETCHECK, BST_UNCHECKED, 0 );

	// COMPATFLAGS
	if ( compatflags & COMPATF_SHORTTEX )
		SendDlgItemMessage( hDlg, IDC_SHORTTEX, BM_SETCHECK, BST_CHECKED, 0 );
	else
		SendDlgItemMessage( hDlg, IDC_SHORTTEX, BM_SETCHECK, BST_UNCHECKED, 0 );

	if ( compatflags & COMPATF_STAIRINDEX )
		SendDlgItemMessage( hDlg, IDC_STAIRINDEX, BM_SETCHECK, BST_CHECKED, 0 );
	else
		SendDlgItemMessage( hDlg, IDC_STAIRINDEX, BM_SETCHECK, BST_UNCHECKED, 0 );

	if ( compatflags & COMPATF_SILENTPICKUP )
		SendDlgItemMessage( hDlg, IDC_SILENTPICKUP, BM_SETCHECK, BST_CHECKED, 0 );
	else
		SendDlgItemMessage( hDlg, IDC_SILENTPICKUP, BM_SETCHECK, BST_UNCHECKED, 0 );

	if ( compatflags & COMPATF_LIMITPAIN )
		SendDlgItemMessage( hDlg, IDC_LIMITPAIN, BM_SETCHECK, BST_CHECKED, 0 );
	else
		SendDlgItemMessage( hDlg, IDC_LIMITPAIN, BM_SETCHECK, BST_UNCHECKED, 0 );

	if ( compatflags & COMPATF_NO_PASSMOBJ )
		SendDlgItemMessage( hDlg, IDC_NO_PASSMOBJ, BM_SETCHECK, BST_CHECKED, 0 );
	else
		SendDlgItemMessage( hDlg, IDC_NO_PASSMOBJ, BM_SETCHECK, BST_UNCHECKED, 0 );

	if ( compatflags & COMPATF_MAGICSILENCE )
		SendDlgItemMessage( hDlg, IDC_MAGICSILENCE, BM_SETCHECK, BST_CHECKED, 0 );
	else
		SendDlgItemMessage( hDlg, IDC_MAGICSILENCE, BM_SETCHECK, BST_UNCHECKED, 0 );

	if ( compatflags & COMPATF_WALLRUN )
		SendDlgItemMessage( hDlg, IDC_WALLRUN, BM_SETCHECK, BST_CHECKED, 0 );
	else
		SendDlgItemMessage( hDlg, IDC_WALLRUN, BM_SETCHECK, BST_UNCHECKED, 0 );

	if ( compatflags & COMPATF_NOTOSSDROPS )
		SendDlgItemMessage( hDlg, IDC_NOTOSSDROPS, BM_SETCHECK, BST_CHECKED, 0 );
	else
		SendDlgItemMessage( hDlg, IDC_NOTOSSDROPS, BM_SETCHECK, BST_UNCHECKED, 0 );

	if ( compatflags & COMPATF_USEBLOCKING )
		SendDlgItemMessage( hDlg, IDC_USEBLOCKING, BM_SETCHECK, BST_CHECKED, 0 );
	else
		SendDlgItemMessage( hDlg, IDC_USEBLOCKING, BM_SETCHECK, BST_UNCHECKED, 0 );

	if ( compatflags & COMPATF_NODOORLIGHT )
		SendDlgItemMessage( hDlg, IDC_NODOORLIGHT, BM_SETCHECK, BST_CHECKED, 0 );
	else
		SendDlgItemMessage( hDlg, IDC_NODOORLIGHT, BM_SETCHECK, BST_UNCHECKED, 0 );

	if ( compatflags & COMPATF_LIMITED_AIRMOVEMENT )
		SendDlgItemMessage( hDlg, IDC_LIMITED_AIRMOVEMENT, BM_SETCHECK, BST_CHECKED, 0 );
	else
		SendDlgItemMessage( hDlg, IDC_LIMITED_AIRMOVEMENT, BM_SETCHECK, BST_UNCHECKED, 0 );

	if ( compatflags & COMPATF_PLASMA_BUMP_BUG )
		SendDlgItemMessage( hDlg, IDC_PLASMA_BUMP_BUG, BM_SETCHECK, BST_CHECKED, 0 );
	else
		SendDlgItemMessage( hDlg, IDC_PLASMA_BUMP_BUG, BM_SETCHECK, BST_UNCHECKED, 0 );

	if ( compatflags & COMPATF_INSTANTRESPAWN )
		SendDlgItemMessage( hDlg, IDC_INSTANTRESPAWN, BM_SETCHECK, BST_CHECKED, 0 );
	else
		SendDlgItemMessage( hDlg, IDC_INSTANTRESPAWN, BM_SETCHECK, BST_UNCHECKED, 0 );

	if ( compatflags & COMPATF_DISABLETAUNTS )
		SendDlgItemMessage( hDlg, IDC_DISABLETAUNTS, BM_SETCHECK, BST_CHECKED, 0 );
	else
		SendDlgItemMessage( hDlg, IDC_DISABLETAUNTS, BM_SETCHECK, BST_UNCHECKED, 0 );

	if ( compatflags & COMPATF_ORIGINALSOUNDCURVE )
		SendDlgItemMessage( hDlg, IDC_ORIGINALSOUNDCURVE, BM_SETCHECK, BST_CHECKED, 0 );
	else
		SendDlgItemMessage( hDlg, IDC_ORIGINALSOUNDCURVE, BM_SETCHECK, BST_UNCHECKED, 0 );

	if ( compatflags & COMPATF_OLDINTERMISSION )
		SendDlgItemMessage( hDlg, IDC_OLDINTERMISSION, BM_SETCHECK, BST_CHECKED, 0 );
	else
		SendDlgItemMessage( hDlg, IDC_OLDINTERMISSION, BM_SETCHECK, BST_UNCHECKED, 0 );

	if ( compatflags & COMPATF_DISABLESTEALTHMONSTERS )
		SendDlgItemMessage( hDlg, IDC_DISABLESTEALTHMONSTERS, BM_SETCHECK, BST_CHECKED, 0 );
	else
		SendDlgItemMessage( hDlg, IDC_DISABLESTEALTHMONSTERS, BM_SETCHECK, BST_UNCHECKED, 0 );
/*
	if ( compatflags & COMPATF_DISABLECOOPERATIVEBACKPACKS )
		SendDlgItemMessage( hDlg, IDC_DISABLECOOPERATIVEBACKPACKS, BM_SETCHECK, BST_CHECKED, 0 );
	else
		SendDlgItemMessage( hDlg, IDC_DISABLECOOPERATIVEBACKPACKS, BM_SETCHECK, BST_UNCHECKED, 0 );
*/

	// Store current values for dmflags/dmflags2/compatflags.
	Val = dmflags.GetGenericRep( CVAR_Int );
	g_ulStoredDMFlags = Val.Int;
	Val = dmflags2.GetGenericRep( CVAR_Int );
	g_ulStoredDMFlags2 = Val.Int;
	Val = compatflags.GetGenericRep( CVAR_Int );
	g_ulStoredCompatflags = Val.Int;
}

//*****************************************************************************
//
void SERVERCONSOLE_UpdateDMFlagsDisplay( HWND hDlg )
{
	char	szString[32];
	ULONG	ulDMFlags = 0;
	ULONG	ulDMFlags2 = 0;
	ULONG	ulCompatflags = 0;

	// DMFLAGS
	if ( SendDlgItemMessage( hDlg, IDC_FORCE_FALLINGZD, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags |= DF_FORCE_FALLINGZD;
	if ( SendDlgItemMessage( hDlg, IDC_FORCE_FALLINGHX, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags |= DF_FORCE_FALLINGHX;
	if ( SendDlgItemMessage( hDlg, IDC_WEAPONS_STAY, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags |= DF_WEAPONS_STAY;
	if ( SendDlgItemMessage( hDlg, IDC_NO_ITEMS, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags |= DF_NO_ITEMS;
	if ( SendDlgItemMessage( hDlg, IDC_NO_HEALTH, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags |= DF_NO_HEALTH;
	if ( SendDlgItemMessage( hDlg, IDC_NO_ARMOR, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags |= DF_NO_ARMOR;
	if ( SendDlgItemMessage( hDlg, IDC_SPAWN_FARTHEST, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags |= DF_SPAWN_FARTHEST;
	if ( SendDlgItemMessage( hDlg, IDC_SAME_LEVEL, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags |= DF_SAME_LEVEL;
	if ( SendDlgItemMessage( hDlg, IDC_FORCE_RESPAWN, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags |= DF_FORCE_RESPAWN;
	if ( SendDlgItemMessage( hDlg, IDC_NO_EXIT, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags |= DF_NO_EXIT;
	if ( SendDlgItemMessage( hDlg, IDC_INFINITE_AMMO, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags |= DF_INFINITE_AMMO;
	if ( SendDlgItemMessage( hDlg, IDC_NO_MONSTERS, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags |= DF_NO_MONSTERS;
	if ( SendDlgItemMessage( hDlg, IDC_MONSTERS_RESPAWN, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags |= DF_MONSTERS_RESPAWN;
	if ( SendDlgItemMessage( hDlg, IDC_ITEMS_RESPAWN, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags |= DF_ITEMS_RESPAWN;
	if ( SendDlgItemMessage( hDlg, IDC_RESPAWN_SUPER, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags |= DF_RESPAWN_SUPER;
	if ( SendDlgItemMessage( hDlg, IDC_FAST_MONSTERS, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags |= DF_FAST_MONSTERS;
	if ( SendDlgItemMessage( hDlg, IDC_NO_JUMP, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags |= DF_NO_JUMP;
	if ( SendDlgItemMessage( hDlg, IDC_NO_FREELOOK, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags |= DF_NO_FREELOOK;
	if ( SendDlgItemMessage( hDlg, IDC_NO_FOV, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags |= DF_NO_FOV;
	if ( SendDlgItemMessage( hDlg, IDC_NO_COOP_MP_WEAPON_SPAWN, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags |= DF_NO_COOP_WEAPON_SPAWN;
	if ( SendDlgItemMessage( hDlg, IDC_DISALLOW_CROUCH, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags |= DF_NO_CROUCH ;
	if ( SendDlgItemMessage( hDlg, IDC_COOP_LOSE_INVENTORY, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags |= DF_COOP_LOSE_INVENTORY;
	if ( SendDlgItemMessage( hDlg, IDC_KEEP_KEYS, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags |= DF_COOP_LOSE_KEYS;
	if ( SendDlgItemMessage( hDlg, IDC_KEEP_WEAPONS, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags |= DF_COOP_LOSE_WEAPONS;
	if ( SendDlgItemMessage( hDlg, IDC_COOP_LOSE_ARMOR, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags |= DF_COOP_LOSE_ARMOR;
	if ( SendDlgItemMessage( hDlg, IDC_COOP_LOSE_POWERUPS, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags |= DF_COOP_LOSE_POWERUPS;
	if ( SendDlgItemMessage( hDlg, IDC_COOP_LOSE_AMMO, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags |= DF_COOP_LOSE_AMMO;
	if ( SendDlgItemMessage( hDlg, IDC_COOP_HALVE_AMMO, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags |= DF_COOP_HALVE_AMMO;

	sprintf( szString, "dmflags: %ld", ulDMFlags );
	SetDlgItemText( hDlg, IDC_DMFLAGS, szString );

	// DMFLAGS2
	if ( SendDlgItemMessage( hDlg, IDC_YES_WEAPONDROP, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags2 |= DF2_YES_WEAPONDROP;
	if ( SendDlgItemMessage( hDlg, IDC_NO_RUNES, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags2 |= DF2_NO_RUNES;
	if ( SendDlgItemMessage( hDlg, IDC_INSTANT_RETURN, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags2 |= DF2_INSTANT_RETURN;
	if ( SendDlgItemMessage( hDlg, IDC_NO_TEAM_SWITCH, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags2 |= DF2_NO_TEAM_SWITCH;
	if ( SendDlgItemMessage( hDlg, IDC_NO_TEAM_SELECT, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags2 |= DF2_NO_TEAM_SELECT;
	if ( SendDlgItemMessage( hDlg, IDC_YES_DOUBLEAMMO, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags2 |= DF2_YES_DOUBLEAMMO;
	if ( SendDlgItemMessage( hDlg, IDC_YES_DEGENERATION, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags2 |= DF2_YES_DEGENERATION;
	if ( SendDlgItemMessage( hDlg, IDC_YES_FREEAIMBFG, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags2 |= DF2_YES_FREEAIMBFG;
	if ( SendDlgItemMessage( hDlg, IDC_BARRELS_RESPAWN, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags2 |= DF2_BARRELS_RESPAWN;
	if ( SendDlgItemMessage( hDlg, IDC_NO_RESPAWN_INVUL, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags2 |= DF2_NO_RESPAWN_INVUL;
	if ( SendDlgItemMessage( hDlg, IDC_SHOTGUN_START, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags2 |= DF2_COOP_SHOTGUNSTART;
	if ( SendDlgItemMessage( hDlg, IDC_SAME_SPAWN_SPOT, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags2 |= DF2_SAME_SPAWN_SPOT;

	sprintf( szString, "dmflags2: %ld", ulDMFlags2 );
	SetDlgItemText( hDlg, IDC_DMFLAGS2, szString );

	// COMPATFLAGS
	if ( SendDlgItemMessage( hDlg, IDC_SHORTTEX, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulCompatflags |= COMPATF_SHORTTEX;
	if ( SendDlgItemMessage( hDlg, IDC_STAIRINDEX, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulCompatflags |= COMPATF_STAIRINDEX;
	if ( SendDlgItemMessage( hDlg, IDC_LIMITPAIN, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulCompatflags |= COMPATF_LIMITPAIN;
	if ( SendDlgItemMessage( hDlg, IDC_SILENTPICKUP, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulCompatflags |= COMPATF_SILENTPICKUP;
	if ( SendDlgItemMessage( hDlg, IDC_NO_PASSMOBJ, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulCompatflags |= COMPATF_NO_PASSMOBJ;
	if ( SendDlgItemMessage( hDlg, IDC_MAGICSILENCE, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulCompatflags |= COMPATF_MAGICSILENCE;
	if ( SendDlgItemMessage( hDlg, IDC_WALLRUN, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulCompatflags |= COMPATF_WALLRUN;
	if ( SendDlgItemMessage( hDlg, IDC_NOTOSSDROPS, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulCompatflags |= COMPATF_NOTOSSDROPS;
	if ( SendDlgItemMessage( hDlg, IDC_USEBLOCKING, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulCompatflags |= COMPATF_USEBLOCKING;
	if ( SendDlgItemMessage( hDlg, IDC_NODOORLIGHT, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulCompatflags |= COMPATF_NODOORLIGHT;
	if ( SendDlgItemMessage( hDlg, IDC_LIMITED_AIRMOVEMENT, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulCompatflags |= COMPATF_LIMITED_AIRMOVEMENT;
	if ( SendDlgItemMessage( hDlg, IDC_PLASMA_BUMP_BUG, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulCompatflags |= COMPATF_PLASMA_BUMP_BUG;
	if ( SendDlgItemMessage( hDlg, IDC_INSTANTRESPAWN, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulCompatflags |= COMPATF_INSTANTRESPAWN;
	if ( SendDlgItemMessage( hDlg, IDC_DISABLETAUNTS, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulCompatflags |= COMPATF_DISABLETAUNTS;
	if ( SendDlgItemMessage( hDlg, IDC_ORIGINALSOUNDCURVE, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulCompatflags |= COMPATF_ORIGINALSOUNDCURVE;
	if ( SendDlgItemMessage( hDlg, IDC_OLDINTERMISSION, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulCompatflags |= COMPATF_OLDINTERMISSION;
	if ( SendDlgItemMessage( hDlg, IDC_DISABLESTEALTHMONSTERS, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulCompatflags |= COMPATF_DISABLESTEALTHMONSTERS;
//	if ( SendDlgItemMessage( hDlg, IDC_DISABLECOOPERATIVEBACKPACKS, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
//		ulCompatflags |= COMPATF_DISABLECOOPERATIVEBACKPACKS;

	sprintf( szString, "compatflags: %ld", ulCompatflags );
	SetDlgItemText( hDlg, IDC_COMPATFLAGS, szString );
}

//*****************************************************************************
//
void SERVERCONSOLE_UpdateDMFlags( HWND hDlg )
{
	char	szString[32];
	ULONG	ulDMFlags = 0;
	ULONG	ulDMFlags2 = 0;
	ULONG	ulCompatflags = 0;

	// DMFLAGS
	if ( SendDlgItemMessage( hDlg, IDC_FORCE_FALLINGZD, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags |= DF_FORCE_FALLINGZD;
	if ( SendDlgItemMessage( hDlg, IDC_FORCE_FALLINGHX, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags |= DF_FORCE_FALLINGHX;
	if ( SendDlgItemMessage( hDlg, IDC_WEAPONS_STAY, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags |= DF_WEAPONS_STAY;
	if ( SendDlgItemMessage( hDlg, IDC_NO_ITEMS, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags |= DF_NO_ITEMS;
	if ( SendDlgItemMessage( hDlg, IDC_NO_HEALTH, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags |= DF_NO_HEALTH;
	if ( SendDlgItemMessage( hDlg, IDC_NO_ARMOR, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags |= DF_NO_ARMOR;
	if ( SendDlgItemMessage( hDlg, IDC_SPAWN_FARTHEST, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags |= DF_SPAWN_FARTHEST;
	if ( SendDlgItemMessage( hDlg, IDC_SAME_LEVEL, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags |= DF_SAME_LEVEL;
	if ( SendDlgItemMessage( hDlg, IDC_FORCE_RESPAWN, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags |= DF_FORCE_RESPAWN;
	if ( SendDlgItemMessage( hDlg, IDC_NO_EXIT, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags |= DF_NO_EXIT;
	if ( SendDlgItemMessage( hDlg, IDC_INFINITE_AMMO, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags |= DF_INFINITE_AMMO;
	if ( SendDlgItemMessage( hDlg, IDC_NO_MONSTERS, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags |= DF_NO_MONSTERS;
	if ( SendDlgItemMessage( hDlg, IDC_MONSTERS_RESPAWN, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags |= DF_MONSTERS_RESPAWN;
	if ( SendDlgItemMessage( hDlg, IDC_ITEMS_RESPAWN, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags |= DF_ITEMS_RESPAWN;
	if ( SendDlgItemMessage( hDlg, IDC_RESPAWN_SUPER, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags |= DF_RESPAWN_SUPER;
	if ( SendDlgItemMessage( hDlg, IDC_FAST_MONSTERS, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags |= DF_FAST_MONSTERS;
	if ( SendDlgItemMessage( hDlg, IDC_NO_JUMP, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags |= DF_NO_JUMP;
	if ( SendDlgItemMessage( hDlg, IDC_NO_FREELOOK, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags |= DF_NO_FREELOOK;
	if ( SendDlgItemMessage( hDlg, IDC_NO_FOV, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags |= DF_NO_FOV;
	if ( SendDlgItemMessage( hDlg, IDC_NO_COOP_MP_WEAPON_SPAWN, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags |= DF_NO_COOP_WEAPON_SPAWN;
	if ( SendDlgItemMessage( hDlg, IDC_DISALLOW_CROUCH, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags |= DF_NO_CROUCH;
	if ( SendDlgItemMessage( hDlg, IDC_COOP_LOSE_INVENTORY, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags |= DF_COOP_LOSE_INVENTORY;
	if ( SendDlgItemMessage( hDlg, IDC_KEEP_KEYS, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags |= DF_COOP_LOSE_KEYS;
	if ( SendDlgItemMessage( hDlg, IDC_KEEP_WEAPONS, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags |= DF_COOP_LOSE_WEAPONS;
	if ( SendDlgItemMessage( hDlg, IDC_COOP_LOSE_ARMOR, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags |= DF_COOP_LOSE_ARMOR;
	if ( SendDlgItemMessage( hDlg, IDC_COOP_LOSE_POWERUPS, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags |= DF_COOP_LOSE_POWERUPS;
	if ( SendDlgItemMessage( hDlg, IDC_COOP_LOSE_AMMO, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags |= DF_COOP_LOSE_AMMO;
	if ( SendDlgItemMessage( hDlg, IDC_COOP_HALVE_AMMO, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags |= DF_COOP_HALVE_AMMO;

	// If the DMFlags have changed, send the update.
	if ( g_ulStoredDMFlags != ulDMFlags )
	{
		sprintf( szString, "dmflags %ld", ulDMFlags );
		SERVER_AddCommand( szString );
	}

	// DMFLAGS2
	if ( SendDlgItemMessage( hDlg, IDC_YES_WEAPONDROP, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags2 |= DF2_YES_WEAPONDROP;
	if ( SendDlgItemMessage( hDlg, IDC_NO_RUNES, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags2 |= DF2_NO_RUNES;
	if ( SendDlgItemMessage( hDlg, IDC_INSTANT_RETURN, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags2 |= DF2_INSTANT_RETURN;
	if ( SendDlgItemMessage( hDlg, IDC_NO_TEAM_SWITCH, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags2 |= DF2_NO_TEAM_SWITCH;
	if ( SendDlgItemMessage( hDlg, IDC_NO_TEAM_SELECT, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags2 |= DF2_NO_TEAM_SELECT;
	if ( SendDlgItemMessage( hDlg, IDC_YES_DOUBLEAMMO, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags2 |= DF2_YES_DOUBLEAMMO;
	if ( SendDlgItemMessage( hDlg, IDC_YES_DEGENERATION, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags2 |= DF2_YES_DEGENERATION;
	if ( SendDlgItemMessage( hDlg, IDC_YES_FREEAIMBFG, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags2 |= DF2_YES_FREEAIMBFG;
	if ( SendDlgItemMessage( hDlg, IDC_BARRELS_RESPAWN, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags2 |= DF2_BARRELS_RESPAWN;
	if ( SendDlgItemMessage( hDlg, IDC_NO_RESPAWN_INVUL, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags2 |= DF2_NO_RESPAWN_INVUL;
	if ( SendDlgItemMessage( hDlg, IDC_SHOTGUN_START, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags2 |= DF2_COOP_SHOTGUNSTART;
	if ( SendDlgItemMessage( hDlg, IDC_SAME_SPAWN_SPOT, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulDMFlags2 |= DF2_SAME_SPAWN_SPOT;

	// If the DMFlags2 have changed, send the update.
	if ( g_ulStoredDMFlags2 != ulDMFlags2 )
	{
		sprintf( szString, "dmflags2 %ld", ulDMFlags2 );
		SERVER_AddCommand( szString );
	}

	// COMPATFLAGS
	if ( SendDlgItemMessage( hDlg, IDC_SHORTTEX, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulCompatflags |= COMPATF_SHORTTEX;
	if ( SendDlgItemMessage( hDlg, IDC_STAIRINDEX, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulCompatflags |= COMPATF_STAIRINDEX;
	if ( SendDlgItemMessage( hDlg, IDC_LIMITPAIN, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulCompatflags |= COMPATF_LIMITPAIN;
	if ( SendDlgItemMessage( hDlg, IDC_SILENTPICKUP, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulCompatflags |= COMPATF_SILENTPICKUP;
	if ( SendDlgItemMessage( hDlg, IDC_NO_PASSMOBJ, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulCompatflags |= COMPATF_NO_PASSMOBJ;
	if ( SendDlgItemMessage( hDlg, IDC_MAGICSILENCE, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulCompatflags |= COMPATF_MAGICSILENCE;
	if ( SendDlgItemMessage( hDlg, IDC_WALLRUN, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulCompatflags |= COMPATF_WALLRUN;
	if ( SendDlgItemMessage( hDlg, IDC_NOTOSSDROPS, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulCompatflags |= COMPATF_NOTOSSDROPS;
	if ( SendDlgItemMessage( hDlg, IDC_USEBLOCKING, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulCompatflags |= COMPATF_USEBLOCKING;
	if ( SendDlgItemMessage( hDlg, IDC_NODOORLIGHT, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulCompatflags |= COMPATF_NODOORLIGHT;
	if ( SendDlgItemMessage( hDlg, IDC_LIMITED_AIRMOVEMENT, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulCompatflags |= COMPATF_LIMITED_AIRMOVEMENT;
	if ( SendDlgItemMessage( hDlg, IDC_PLASMA_BUMP_BUG, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulCompatflags |= COMPATF_PLASMA_BUMP_BUG;
	if ( SendDlgItemMessage( hDlg, IDC_INSTANTRESPAWN, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulCompatflags |= COMPATF_INSTANTRESPAWN;
	if ( SendDlgItemMessage( hDlg, IDC_DISABLETAUNTS, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulCompatflags |= COMPATF_DISABLETAUNTS;
	if ( SendDlgItemMessage( hDlg, IDC_ORIGINALSOUNDCURVE, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulCompatflags |= COMPATF_ORIGINALSOUNDCURVE;
	if ( SendDlgItemMessage( hDlg, IDC_OLDINTERMISSION, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulCompatflags |= COMPATF_OLDINTERMISSION;
	if ( SendDlgItemMessage( hDlg, IDC_DISABLESTEALTHMONSTERS, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulCompatflags |= COMPATF_DISABLESTEALTHMONSTERS;
//	if ( SendDlgItemMessage( hDlg, IDC_DISABLECOOPERATIVEBACKPACKS, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
//		ulCompatflags |= COMPATF_DISABLECOOPERATIVEBACKPACKS;

	// If the compatflags have changed, send the update.
	if ( g_ulStoredCompatflags != ulCompatflags )
	{
		sprintf( szString, "compatflags %ld", ulCompatflags );
		SERVER_AddCommand( szString );
	}
}

//*****************************************************************************
//
void SERVERCONSOLE_InitializeLMSSettingsDisplay( HWND hDlg )
{
	UCVarValue	Val;

	// LMSALLOWEDWEAPONS
	if ( lmsallowedweapons & LMS_AWF_CHAINSAW )
		SendDlgItemMessage( hDlg, IDC_LMS_ALLOWCHAINSAW, BM_SETCHECK, BST_CHECKED, 0 );
	else
		SendDlgItemMessage( hDlg, IDC_LMS_ALLOWCHAINSAW, BM_SETCHECK, BST_UNCHECKED, 0 );

	if ( lmsallowedweapons & LMS_AWF_PISTOL )
		SendDlgItemMessage( hDlg, IDC_LMS_ALLOWPISTOL, BM_SETCHECK, BST_CHECKED, 0 );
	else
		SendDlgItemMessage( hDlg, IDC_LMS_ALLOWPISTOL, BM_SETCHECK, BST_UNCHECKED, 0 );

	if ( lmsallowedweapons & LMS_AWF_SHOTGUN )
		SendDlgItemMessage( hDlg, IDC_LMS_ALLOWSHOTGUN, BM_SETCHECK, BST_CHECKED, 0 );
	else
		SendDlgItemMessage( hDlg, IDC_LMS_ALLOWSHOTGUN, BM_SETCHECK, BST_UNCHECKED, 0 );

	if ( lmsallowedweapons & LMS_AWF_SSG )
		SendDlgItemMessage( hDlg, IDC_LMS_ALLOWSSG, BM_SETCHECK, BST_CHECKED, 0 );
	else
		SendDlgItemMessage( hDlg, IDC_LMS_ALLOWSSG, BM_SETCHECK, BST_UNCHECKED, 0 );

	if ( lmsallowedweapons & LMS_AWF_CHAINGUN )
		SendDlgItemMessage( hDlg, IDC_LMS_ALLOWCHAINGUN, BM_SETCHECK, BST_CHECKED, 0 );
	else
		SendDlgItemMessage( hDlg, IDC_LMS_ALLOWCHAINGUN, BM_SETCHECK, BST_UNCHECKED, 0 );

	if ( lmsallowedweapons & LMS_AWF_MINIGUN )
		SendDlgItemMessage( hDlg, IDC_LMS_ALLOWMINIGUN, BM_SETCHECK, BST_CHECKED, 0 );
	else
		SendDlgItemMessage( hDlg, IDC_LMS_ALLOWMINIGUN, BM_SETCHECK, BST_UNCHECKED, 0 );

	if ( lmsallowedweapons & LMS_AWF_ROCKETLAUNCHER )
		SendDlgItemMessage( hDlg, IDC_LMS_ALLOWROCKETLAUNCHER, BM_SETCHECK, BST_CHECKED, 0 );
	else
		SendDlgItemMessage( hDlg, IDC_LMS_ALLOWROCKETLAUNCHER, BM_SETCHECK, BST_UNCHECKED, 0 );

	if ( lmsallowedweapons & LMS_AWF_GRENADELAUNCHER )
		SendDlgItemMessage( hDlg, IDC_LMS_ALLOWGRENADELAUNCHER, BM_SETCHECK, BST_CHECKED, 0 );
	else
		SendDlgItemMessage( hDlg, IDC_LMS_ALLOWGRENADELAUNCHER, BM_SETCHECK, BST_UNCHECKED, 0 );

	if ( lmsallowedweapons & LMS_AWF_PLASMA )
		SendDlgItemMessage( hDlg, IDC_LMS_ALLOWPLASMA, BM_SETCHECK, BST_CHECKED, 0 );
	else
		SendDlgItemMessage( hDlg, IDC_LMS_ALLOWPLASMA, BM_SETCHECK, BST_UNCHECKED, 0 );

	if ( lmsallowedweapons & LMS_AWF_RAILGUN )
		SendDlgItemMessage( hDlg, IDC_LMS_ALLOWRAILGUN, BM_SETCHECK, BST_CHECKED, 0 );
	else
		SendDlgItemMessage( hDlg, IDC_LMS_ALLOWRAILGUN, BM_SETCHECK, BST_UNCHECKED, 0 );

	// LMSSPECTATORSETTINGS
	if ( lmsspectatorsettings & LMS_SPF_VIEW )
		SendDlgItemMessage( hDlg, IDC_LMS_SPECTATORVIEW, BM_SETCHECK, BST_CHECKED, 0 );
	else
		SendDlgItemMessage( hDlg, IDC_LMS_SPECTATORVIEW, BM_SETCHECK, BST_UNCHECKED, 0 );

	if ( lmsspectatorsettings & LMS_SPF_CHAT )
		SendDlgItemMessage( hDlg, IDC_LMS_SPECTATORTALK, BM_SETCHECK, BST_CHECKED, 0 );
	else
		SendDlgItemMessage( hDlg, IDC_LMS_SPECTATORTALK, BM_SETCHECK, BST_UNCHECKED, 0 );

	// Store current values for lmsallowedweapons/lmsspectatorsettings.
	Val = lmsallowedweapons.GetGenericRep( CVAR_Int );
	g_ulStoredLMSAllowedWeapons = Val.Int;
	Val = lmsspectatorsettings.GetGenericRep( CVAR_Int );
	g_ulStoredLMSSpectatorSettings = Val.Int;
}

//*****************************************************************************
//
void SERVERCONSOLE_UpdateLMSSettingsDisplay( HWND hDlg )
{
	char	szString[32];
	ULONG	ulLMSAllowedWeapons = 0;
	ULONG	ulLMSSpectatorSettings = 0;

	// LMSALLOWEDWEAPONS
	if ( SendDlgItemMessage( hDlg, IDC_LMS_ALLOWCHAINSAW, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulLMSAllowedWeapons |= LMS_AWF_CHAINSAW;
	if ( SendDlgItemMessage( hDlg, IDC_LMS_ALLOWPISTOL, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulLMSAllowedWeapons |= LMS_AWF_PISTOL;
	if ( SendDlgItemMessage( hDlg, IDC_LMS_ALLOWSHOTGUN, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulLMSAllowedWeapons |= LMS_AWF_SHOTGUN;
	if ( SendDlgItemMessage( hDlg, IDC_LMS_ALLOWSSG, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulLMSAllowedWeapons |= LMS_AWF_SSG;
	if ( SendDlgItemMessage( hDlg, IDC_LMS_ALLOWCHAINGUN, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulLMSAllowedWeapons |= LMS_AWF_CHAINGUN;
	if ( SendDlgItemMessage( hDlg, IDC_LMS_ALLOWMINIGUN, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulLMSAllowedWeapons |= LMS_AWF_MINIGUN;
	if ( SendDlgItemMessage( hDlg, IDC_LMS_ALLOWROCKETLAUNCHER, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulLMSAllowedWeapons |= LMS_AWF_ROCKETLAUNCHER;
	if ( SendDlgItemMessage( hDlg, IDC_LMS_ALLOWGRENADELAUNCHER, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulLMSAllowedWeapons |= LMS_AWF_GRENADELAUNCHER;
	if ( SendDlgItemMessage( hDlg, IDC_LMS_ALLOWPLASMA, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulLMSAllowedWeapons |= LMS_AWF_PLASMA;
	if ( SendDlgItemMessage( hDlg, IDC_LMS_ALLOWRAILGUN, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulLMSAllowedWeapons |= LMS_AWF_RAILGUN;

	sprintf( szString, "lmsallowedweapons: %ld", ulLMSAllowedWeapons );
	SetDlgItemText( hDlg, IDC_LMSALLOWEDWEAPONS, szString );

	// LMSSPECTATORSETTINGS
	if ( SendDlgItemMessage( hDlg, IDC_LMS_SPECTATORVIEW, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulLMSSpectatorSettings |= LMS_SPF_VIEW;
	if ( SendDlgItemMessage( hDlg, IDC_LMS_SPECTATORTALK, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulLMSSpectatorSettings |= LMS_SPF_CHAT;

	sprintf( szString, "lmsspectatorsettings: %ld", ulLMSSpectatorSettings );
	SetDlgItemText( hDlg, IDC_LMSSPECTATORSETTINGS, szString );
}

//*****************************************************************************
//
void SERVERCONSOLE_UpdateLMSSettings( HWND hDlg )
{
	char	szString[32];
	ULONG	ulLMSAllowedWeapons = 0;
	ULONG	ulLMSSpectatorSettings = 0;

	// LMSALLOWEDWEAPONS
	if ( SendDlgItemMessage( hDlg, IDC_LMS_ALLOWCHAINSAW, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulLMSAllowedWeapons |= LMS_AWF_CHAINSAW;
	if ( SendDlgItemMessage( hDlg, IDC_LMS_ALLOWPISTOL, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulLMSAllowedWeapons |= LMS_AWF_PISTOL;
	if ( SendDlgItemMessage( hDlg, IDC_LMS_ALLOWSHOTGUN, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulLMSAllowedWeapons |= LMS_AWF_SHOTGUN;
	if ( SendDlgItemMessage( hDlg, IDC_LMS_ALLOWSSG, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulLMSAllowedWeapons |= LMS_AWF_SSG;
	if ( SendDlgItemMessage( hDlg, IDC_LMS_ALLOWCHAINGUN, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulLMSAllowedWeapons |= LMS_AWF_CHAINGUN;
	if ( SendDlgItemMessage( hDlg, IDC_LMS_ALLOWMINIGUN, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulLMSAllowedWeapons |= LMS_AWF_MINIGUN;
	if ( SendDlgItemMessage( hDlg, IDC_LMS_ALLOWROCKETLAUNCHER, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulLMSAllowedWeapons |= LMS_AWF_ROCKETLAUNCHER;
	if ( SendDlgItemMessage( hDlg, IDC_LMS_ALLOWGRENADELAUNCHER, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulLMSAllowedWeapons |= LMS_AWF_GRENADELAUNCHER;
	if ( SendDlgItemMessage( hDlg, IDC_LMS_ALLOWPLASMA, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulLMSAllowedWeapons |= LMS_AWF_PLASMA;
	if ( SendDlgItemMessage( hDlg, IDC_LMS_ALLOWRAILGUN, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulLMSAllowedWeapons |= LMS_AWF_RAILGUN;

	// If lmsallowedweapons has changed, send the update.
	if ( g_ulStoredLMSAllowedWeapons != ulLMSAllowedWeapons )
	{
		sprintf( szString, "lmsallowedweapons %ld", ulLMSAllowedWeapons );
		SERVER_AddCommand( szString );
	}

	// LMSSPECTATORSETTINGS
	if ( SendDlgItemMessage( hDlg, IDC_LMS_SPECTATORVIEW, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulLMSSpectatorSettings |= LMS_SPF_VIEW;
	if ( SendDlgItemMessage( hDlg, IDC_LMS_SPECTATORTALK, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		ulLMSSpectatorSettings |= LMS_SPF_CHAT;

	// If lmsspectatorsettings has changed, send the update.
	if ( g_ulStoredLMSSpectatorSettings != ulLMSSpectatorSettings )
	{
		sprintf( szString, "lmsspectatorsettings %ld", ulLMSSpectatorSettings );
		SERVER_AddCommand( szString );
	}
}

//*****************************************************************************
//
void SERVERCONSOLE_InitializeGeneralSettingsDisplay( HWND hDlg )
{
	UCVarValue	Val;

	// SERVER GROUP
	Val = sv_hostname.GetGenericRep( CVAR_String );
	SetDlgItemText( hDlg, IDC_SERVERNAME, Val.String );
	
	Val = sv_website.GetGenericRep( CVAR_String );
	SetDlgItemText( hDlg, IDC_WADURL, Val.String );
	
	Val = sv_hostemail.GetGenericRep( CVAR_String );
	SetDlgItemText( hDlg, IDC_EMAIL, Val.String );

	if ( sv_updatemaster )
		SendDlgItemMessage( hDlg, IDC_UPDATEMASTER, BM_SETCHECK, BST_CHECKED, 0 );
	if ( sv_broadcast )
		SendDlgItemMessage( hDlg, IDC_BROADCAST, BM_SETCHECK, BST_CHECKED, 0 );
	if ( sv_showlauncherqueries )
		SendDlgItemMessage( hDlg, IDC_SHOWLAUNCHERQUERIES, BM_SETCHECK, BST_CHECKED, 0 );

	Val = sv_masterip.GetGenericRep( CVAR_String );
	SetDlgItemText( hDlg, IDC_MASTERIP, Val.String );

	Val = sv_motd.GetGenericRep( CVAR_String );
	// Turn "\n" into carriage returns.
	{
		char	szBuffer[512];
		char	szInputString[512];
		char	*psz;
		char	*pszString;
		char	c;

		strncpy( szInputString, Val.String, 512 );
		szInputString[512-1] = 0;

		// Nifty little trick to turn "\n" into '\n', while mantaining the "\c" color codes.
		V_ColorizeString( szInputString );
		V_UnColorizeString( szInputString, 256 );

		pszString = szInputString;

		// Incoming lines need a carriage return.
		psz = szBuffer;

		while ( 1 )
		{
			c = *pszString++;
			if ( c == '\0' )
			{
				*psz = c;
				break;
			}
			if ( c == '\n' )
				*psz++ = '\r';
			*psz++ = c;
		}

		SetDlgItemText( hDlg, IDC_MOTD, szBuffer );
	}

	// GAMEPLAY GROUP
	Val = fraglimit.GetGenericRep( CVAR_String );
	SetDlgItemText( hDlg, IDC_FRAGLIMIT, Val.String );
	g_ulStoredFraglimit = atoi( Val.String );

	Val = timelimit.GetGenericRep( CVAR_String );
	SetDlgItemText( hDlg, IDC_TIMELIMIT, Val.String );
	g_ulStoredTimelimit = atoi( Val.String );

	Val = pointlimit.GetGenericRep( CVAR_String );
	SetDlgItemText( hDlg, IDC_POINTLIMIT, Val.String );
	g_ulStoredPointlimit = atoi( Val.String );
	
	Val = duellimit.GetGenericRep( CVAR_String );
	SetDlgItemText( hDlg, IDC_DUELLIMIT, Val.String );
	g_ulStoredDuellimit = atoi( Val.String );

	Val = winlimit.GetGenericRep( CVAR_String );
	SetDlgItemText( hDlg, IDC_WINLIMIT, Val.String );
	g_ulStoredWinlimit = atoi( Val.String );

	SendDlgItemMessage( hDlg, IDC_GAMEPLAYMODE, CB_INSERTSTRING, -1, (WPARAM)(LPSTR)"Cooperative" );
	SendDlgItemMessage( hDlg, IDC_GAMEPLAYMODE, CB_INSERTSTRING, -1, (WPARAM)(LPSTR)"Survival Cooperative" );
	SendDlgItemMessage( hDlg, IDC_GAMEPLAYMODE, CB_INSERTSTRING, -1, (WPARAM)(LPSTR)"Invasion" );
	SendDlgItemMessage( hDlg, IDC_GAMEPLAYMODE, CB_INSERTSTRING, -1, (WPARAM)(LPSTR)"Deathmatch (free for all)" );
	SendDlgItemMessage( hDlg, IDC_GAMEPLAYMODE, CB_INSERTSTRING, -1, (WPARAM)(LPSTR)"Teamplay DM" );
	SendDlgItemMessage( hDlg, IDC_GAMEPLAYMODE, CB_INSERTSTRING, -1, (WPARAM)(LPSTR)"Duel DM" );
	SendDlgItemMessage( hDlg, IDC_GAMEPLAYMODE, CB_INSERTSTRING, -1, (WPARAM)(LPSTR)"Terminator DM" );
	SendDlgItemMessage( hDlg, IDC_GAMEPLAYMODE, CB_INSERTSTRING, -1, (WPARAM)(LPSTR)"Last Man Standing DM" );
	SendDlgItemMessage( hDlg, IDC_GAMEPLAYMODE, CB_INSERTSTRING, -1, (WPARAM)(LPSTR)"Team Last Man Standing" );
	SendDlgItemMessage( hDlg, IDC_GAMEPLAYMODE, CB_INSERTSTRING, -1, (WPARAM)(LPSTR)"Possession DM" );
	SendDlgItemMessage( hDlg, IDC_GAMEPLAYMODE, CB_INSERTSTRING, -1, (WPARAM)(LPSTR)"Team Possession" );
	SendDlgItemMessage( hDlg, IDC_GAMEPLAYMODE, CB_INSERTSTRING, -1, (WPARAM)(LPSTR)"Teamgame" );
	SendDlgItemMessage( hDlg, IDC_GAMEPLAYMODE, CB_INSERTSTRING, -1, (WPARAM)(LPSTR)"Capture the Flag" );
	SendDlgItemMessage( hDlg, IDC_GAMEPLAYMODE, CB_INSERTSTRING, -1, (WPARAM)(LPSTR)"One Flag CTF" );
	SendDlgItemMessage( hDlg, IDC_GAMEPLAYMODE, CB_INSERTSTRING, -1, (WPARAM)(LPSTR)"Skulltag" );

	if ( skulltag )
		SendDlgItemMessage( hDlg, IDC_GAMEPLAYMODE, CB_SETCURSEL, 14, 0 );
	else if ( oneflagctf )
		SendDlgItemMessage( hDlg, IDC_GAMEPLAYMODE, CB_SETCURSEL, 13, 0 );
	else if ( ctf )
		SendDlgItemMessage( hDlg, IDC_GAMEPLAYMODE, CB_SETCURSEL, 12, 0 );
	else if ( teamgame )
		SendDlgItemMessage( hDlg, IDC_GAMEPLAYMODE, CB_SETCURSEL, 11, 0 );
	else if ( teampossession )
		SendDlgItemMessage( hDlg, IDC_GAMEPLAYMODE, CB_SETCURSEL, 10, 0 );
	else if ( possession )
		SendDlgItemMessage( hDlg, IDC_GAMEPLAYMODE, CB_SETCURSEL, 9, 0 );
	else if ( teamlms )
		SendDlgItemMessage( hDlg, IDC_GAMEPLAYMODE, CB_SETCURSEL, 8, 0 );
	else if ( lastmanstanding )
		SendDlgItemMessage( hDlg, IDC_GAMEPLAYMODE, CB_SETCURSEL, 7, 0 );
	else if ( terminator )
		SendDlgItemMessage( hDlg, IDC_GAMEPLAYMODE, CB_SETCURSEL, 6, 0 );
	else if ( duel )
		SendDlgItemMessage( hDlg, IDC_GAMEPLAYMODE, CB_SETCURSEL, 5, 0 );
	else if ( teamplay )
		SendDlgItemMessage( hDlg, IDC_GAMEPLAYMODE, CB_SETCURSEL, 4, 0 );
	else if ( deathmatch )
		SendDlgItemMessage( hDlg, IDC_GAMEPLAYMODE, CB_SETCURSEL, 3, 0 );
	else if ( invasion )
		SendDlgItemMessage( hDlg, IDC_GAMEPLAYMODE, CB_SETCURSEL, 2, 0 );
	else if ( survival )
		SendDlgItemMessage( hDlg, IDC_GAMEPLAYMODE, CB_SETCURSEL, 1, 0 );
	else
		SendDlgItemMessage( hDlg, IDC_GAMEPLAYMODE, CB_SETCURSEL, 0, 0 );

	SendDlgItemMessage( hDlg, IDC_SKILL, CB_INSERTSTRING, -1, (WPARAM)(LPSTR)"I'm too young to die." );
	SendDlgItemMessage( hDlg, IDC_SKILL, CB_INSERTSTRING, -1, (WPARAM)(LPSTR)"Hey, not too rough." );
	SendDlgItemMessage( hDlg, IDC_SKILL, CB_INSERTSTRING, -1, (WPARAM)(LPSTR)"Hurt me plenty." );
	SendDlgItemMessage( hDlg, IDC_SKILL, CB_INSERTSTRING, -1, (WPARAM)(LPSTR)"Ultra-violence." );
	SendDlgItemMessage( hDlg, IDC_SKILL, CB_INSERTSTRING, -1, (WPARAM)(LPSTR)"Nightmare!" );

	switch ( gameskill )
	{
	case 0:
	case 1:
	case 2:
	case 3:
	case 4:

		Val = gameskill.GetGenericRep( CVAR_Int );
		SendDlgItemMessage( hDlg, IDC_SKILL, CB_SETCURSEL, Val.Int, 0 );
		break;
	default:

		SendDlgItemMessage( hDlg, IDC_SKILL, CB_SETCURSEL, 0, 0 );
		break;
	}

	SendDlgItemMessage( hDlg, IDC_BOTSKILL, CB_INSERTSTRING, -1, (WPARAM)(LPSTR)"I want my mommy!" );
	SendDlgItemMessage( hDlg, IDC_BOTSKILL, CB_INSERTSTRING, -1, (WPARAM)(LPSTR)"I'm allergic to pain." );
	SendDlgItemMessage( hDlg, IDC_BOTSKILL, CB_INSERTSTRING, -1, (WPARAM)(LPSTR)"Bring it on." );
	SendDlgItemMessage( hDlg, IDC_BOTSKILL, CB_INSERTSTRING, -1, (WPARAM)(LPSTR)"I thrive off pain." );
	SendDlgItemMessage( hDlg, IDC_BOTSKILL, CB_INSERTSTRING, -1, (WPARAM)(LPSTR)"Nightmare!" );

	switch ( botskill )
	{
	case 0:
	case 1:
	case 2:
	case 3:
	case 4:

		Val = botskill.GetGenericRep( CVAR_Int );
		SendDlgItemMessage( hDlg, IDC_BOTSKILL, CB_SETCURSEL, Val.Int, 0 );
		break;
	default:

		SendDlgItemMessage( hDlg, IDC_BOTSKILL, CB_SETCURSEL, 0, 0 );
		break;
	}

	// ACCESS GROUP
	Val = sv_password.GetGenericRep( CVAR_String );
	SetDlgItemText( hDlg, IDC_PASSWORD, Val.String );
	if ( sv_forcepassword )
		SendDlgItemMessage( hDlg, IDC_REQUIREPW, BM_SETCHECK, BST_CHECKED, 0 );

	Val = sv_joinpassword.GetGenericRep( CVAR_String );
	SetDlgItemText( hDlg, IDC_JOINPASSWORD, Val.String );
	if ( sv_forcejoinpassword )
		SendDlgItemMessage( hDlg, IDC_REQUIREJOINPW, BM_SETCHECK, BST_CHECKED, 0 );

	Val = sv_rconpassword.GetGenericRep( CVAR_String );
	SetDlgItemText( hDlg, IDC_RCONPASSWORD, Val.String );

	Val = sv_maxplayers.GetGenericRep( CVAR_String );
	SetDlgItemText( hDlg, IDC_MAXPLAYERS, Val.String );
	
	Val = sv_maxclients.GetGenericRep( CVAR_String );
	SetDlgItemText( hDlg, IDC_MAXCLIENTS, Val.String );
}

//*****************************************************************************
//
bool SERVERCONSOLE_IsRestartNeeded( HWND hDlg )
{
	switch ( SendDlgItemMessage( hDlg, IDC_GAMEPLAYMODE, CB_GETCURSEL, 0, 0 ))
	{
	// Cooperative
	case 0:

		if ( cooperative != true )
			return ( true );
		break;
	// Survival cooperative
	case 1:

		if ( survival != true )
			return ( true );
		break;
	// Invasion
	case 2:

		if ( invasion != true )
			return ( true );
		break;
	// DM FFA
	case 3:

		if ( deathmatch != true )
			return ( true );
		break;
	// DM teamplay
	case 4:

		if ( teamplay != true )
			return ( true );
		break;
	// DM duel
	case 5:

		if ( duel != true )
			return ( true );
		break;
	// DM terminator
	case 6:

		if ( terminator != true )
			return ( true );
		break;
	// DM LMS
	case 7:

		if ( lastmanstanding != true )
			return ( true );
		break;
	// Team LMS
	case 8:

		if ( teamlms != true )
			return ( true );
		break;
	// DM possession
	case 9:

		if ( possession != true )
			return ( true );
		break;
	// Team possession
	case 10:

		if ( teampossession != true )
			return ( true );
		break;
	// Teamgame
	case 11:

		if ( teamgame != true )
			return ( true );
		break;
	// CTF
	case 12:

		if ( ctf != true )
			return ( true );
		break;
	// One flag CTF
	case 13:

		if ( oneflagctf != true )
			return ( true );
		break;
	// Skulltag
	case 14:

		if ( skulltag != true )
			return ( true );
		break;
	}

	if ( SendDlgItemMessage( hDlg, IDC_SKILL, CB_GETCURSEL, 0, 0 ) != gameskill )
		return ( true );

	if ( SendDlgItemMessage( hDlg, IDC_BOTSKILL, CB_GETCURSEL, 0, 0 ) != botskill )
		return ( true );

	return ( false );
}

//*****************************************************************************
//
void SERVERCONSOLE_UpdateGeneralSettings( HWND hDlg )
{
	char	szBuffer[1024];
	char	szString[1024+64];

	// SERVER
	GetDlgItemText( hDlg, IDC_SERVERNAME, szBuffer, 1024 );
	sv_hostname = szBuffer;
	SERVERCONSOLE_UpdateTitleString( szBuffer );

	GetDlgItemText( hDlg, IDC_WADURL, szBuffer, 1024 );
	sv_website = szBuffer;

	GetDlgItemText( hDlg, IDC_EMAIL, szBuffer, 1024 );
	sv_hostemail = szBuffer;

	sv_updatemaster = !!SendDlgItemMessage( hDlg, IDC_UPDATEMASTER, BM_GETCHECK, 0, 0 );
	sv_broadcast = !!SendDlgItemMessage( hDlg, IDC_BROADCAST, BM_GETCHECK, 0, 0 );
	sv_showlauncherqueries = !!SendDlgItemMessage( hDlg, IDC_SHOWLAUNCHERQUERIES, BM_GETCHECK, 0, 0 );
		
	GetDlgItemText( hDlg, IDC_MASTERIP, szBuffer, 1024 );
	sv_masterip = szBuffer;

	GetDlgItemText( hDlg, IDC_MOTD, szBuffer, 1024 );
	{
		char	*psz;
		char	*pszString;
		char	c;

		psz = szBuffer;
		pszString = szString;
		while ( 1 )
		{
			c = *psz++;
			if ( c == 0 )
			{
				*pszString = c;
				break;
			}
			else if ( c == '\r' )
			{
				*pszString++ = '\\';
				*pszString++ = 'n';
				psz++;
			}
			else
				*pszString++ = c;
		}

		sv_motd = szString;
	}

	// GAMEPLAY
	GetDlgItemText( hDlg, IDC_FRAGLIMIT, szBuffer, 1024 );
	if ( atoi( szBuffer ) != (LONG)g_ulStoredFraglimit )
	{
		sprintf( szString, "fraglimit %s", szBuffer );
		SERVER_AddCommand( szString );
	}

	GetDlgItemText( hDlg, IDC_TIMELIMIT, szBuffer, 1024 );
	if ( atoi( szBuffer ) != (LONG)g_ulStoredTimelimit )
	{
		sprintf( szString, "timelimit %s", szBuffer );
		SERVER_AddCommand( szString );
	}

	GetDlgItemText( hDlg, IDC_POINTLIMIT, szBuffer, 1024 );
	if ( atoi( szBuffer ) != (LONG)g_ulStoredPointlimit )
	{
		sprintf( szString, "pointlimit %s", szBuffer );
		SERVER_AddCommand( szString );
	}

	GetDlgItemText( hDlg, IDC_DUELLIMIT, szBuffer, 1024 );
	if ( atoi( szBuffer ) != (LONG)g_ulStoredDuellimit )
	{
		sprintf( szString, "duellimit %s", szBuffer );
		SERVER_AddCommand( szString );
	}

	GetDlgItemText( hDlg, IDC_WINLIMIT, szBuffer, 1024 );
	if ( atoi( szBuffer ) != (LONG)g_ulStoredWinlimit )
	{
		sprintf( szString, "winlimit %s", szBuffer );
		SERVER_AddCommand( szString );
	}

	switch ( SendDlgItemMessage( hDlg, IDC_GAMEPLAYMODE, CB_GETCURSEL, 0, 0 ))
	{
	// Cooperative
	case 0:

		if ( cooperative != true )
		{
			sprintf( szString, "cooperative true" );
			SERVER_AddCommand( szString );
		}
		if ( survival != false )
		{
			sprintf( szString, "survival false" );
			SERVER_AddCommand( szString );
		}
		if ( invasion != false )
		{
			sprintf( szString, "invasion false" );
			SERVER_AddCommand( szString );
		}
		break;
	// Coop survival
	case 1:

		if ( survival != true )
		{
			sprintf( szString, "survival true" );
			SERVER_AddCommand( szString );
		}
		break;
	// Invasion
	case 2:

		if ( invasion != true )
		{
			sprintf( szString, "invasion true" );
			SERVER_AddCommand( szString );
		}
		break;
	// DM FFA
	case 3:

		if ( deathmatch != true )
		{
			sprintf( szString, "deathmatch true" );
			SERVER_AddCommand( szString );
		}
		if ( teamplay != false )
		{
			sprintf( szString, "teamplay false" );
			SERVER_AddCommand( szString );
		}
		if ( duel != false )
		{
			sprintf( szString, "duel false" );
			SERVER_AddCommand( szString );
		}
		if ( terminator != false )
		{
			sprintf( szString, "terminator false" );
			SERVER_AddCommand( szString );
		}
		if ( lastmanstanding != false )
		{
			sprintf( szString, "lastmanstanding false" );
			SERVER_AddCommand( szString );
		}
		if ( teamlms != false )
		{
			sprintf( szString, "teamlms false" );
			SERVER_AddCommand( szString );
		}
		if ( possession != false )
		{
			sprintf( szString, "possession false" );
			SERVER_AddCommand( szString );
		}
		if ( teampossession != false )
		{
			sprintf( szString, "teampossession false" );
			SERVER_AddCommand( szString );
		}
		break;
	// DM teamplay
	case 4:

		if ( teamplay != true )
		{
			sprintf( szString, "teamplay true" );
			SERVER_AddCommand( szString );
		}
		break;
	// DM duel
	case 5:

		if ( duel != true )
		{
			sprintf( szString, "duel true" );
			SERVER_AddCommand( szString );
		}
		break;
	// DM terminator
	case 6:

		if ( terminator != true )
		{
			sprintf( szString, "terminator true" );
			SERVER_AddCommand( szString );
		}
		break;
	// DM LMS
	case 7:

		if ( lastmanstanding != true )
		{
			sprintf( szString, "lastmanstanding true" );
			SERVER_AddCommand( szString );
		}
		break;
	// Team LMS
	case 8:

		if ( teamlms != true )
		{
			sprintf( szString, "teamlms true" );
			SERVER_AddCommand( szString );
		}
		break;
	// DM possession
	case 9:

		if ( possession != true )
		{
			sprintf( szString, "possession true" );
			SERVER_AddCommand( szString );
		}
		break;
	// Team possession
	case 10:

		if ( teampossession != true )
		{
			sprintf( szString, "teampossession true" );
			SERVER_AddCommand( szString );
		}
		break;
	// Teamgame
	case 11:

		if ( teamgame != true )
		{
			sprintf( szString, "teamgame true" );
			SERVER_AddCommand( szString );
		}
		if ( ctf != false )
		{
			sprintf( szString, "ctf false" );
			SERVER_AddCommand( szString );
		}
		if ( oneflagctf != false )
		{
			sprintf( szString, "oneflagctf false" );
			SERVER_AddCommand( szString );
		}
		if ( skulltag != false )
		{
			sprintf( szString, "skulltag false" );
			SERVER_AddCommand( szString );
		}
		break;
	// CTF
	case 12:

		if ( ctf != true )
		{
			sprintf( szString, "ctf true" );
			SERVER_AddCommand( szString );
		}
		break;
	// One flag CTF
	case 13:

		if ( oneflagctf != true )
		{
			sprintf( szString, "oneflagctf true" );
			SERVER_AddCommand( szString );
		}
		break;
	// Skulltag
	case 14:

		if ( skulltag != true )
		{
			sprintf( szString, "skulltag true" );
			SERVER_AddCommand( szString );
		}
		break;
	}

	if ( SendDlgItemMessage( hDlg, IDC_SKILL, CB_GETCURSEL, 0, 0 ) != gameskill )
	{
		sprintf( szString, "skill %ld", SendDlgItemMessage( hDlg, IDC_SKILL, CB_GETCURSEL, 0, 0 ));
		SERVER_AddCommand( szString );
	}

	if ( SendDlgItemMessage( hDlg, IDC_BOTSKILL, CB_GETCURSEL, 0, 0 ) != botskill )
	{
		sprintf( szString, "botskill %ld", SendDlgItemMessage( hDlg, IDC_BOTSKILL, CB_GETCURSEL, 0, 0 ));
		SERVER_AddCommand( szString );
	}

	GetDlgItemText( hDlg, IDC_PASSWORD, szBuffer, 1024 );
	sv_password = szBuffer;
	sv_forcepassword = !!SendDlgItemMessage( hDlg, IDC_REQUIREPW, BM_GETCHECK, 0, 0 );

	GetDlgItemText( hDlg, IDC_JOINPASSWORD, szBuffer, 1024 );
	sv_joinpassword = szBuffer;
	sv_forcejoinpassword = !!SendDlgItemMessage( hDlg, IDC_REQUIREJOINPW, BM_GETCHECK, 0, 0 );

	GetDlgItemText( hDlg, IDC_RCONPASSWORD, szBuffer, 1024 );
	sv_rconpassword = szBuffer;

	GetDlgItemText( hDlg, IDC_MAXCLIENTS, szBuffer, 1024 );
	sv_maxclients = atoi( szBuffer );

	GetDlgItemText( hDlg, IDC_MAXPLAYERS, szBuffer, 1024 );
	sv_maxplayers = atoi( szBuffer );

	// Update our 'public/private/LAN' section in the statusbar.
	SERVERCONSOLE_UpdateBroadcasting( );

}

//*****************************************************************************
//
void SERVERCONSOLE_UpdateOperatingSystem( char *pszString )
{
	sprintf( g_szOperatingSystem, pszString );
}

//*****************************************************************************
//
void SERVERCONSOLE_UpdateCPUSpeed( char *pszString )
{
	sprintf( g_szCPUSpeed, pszString );
}

//*****************************************************************************
//
void SERVERCONSOLE_UpdateVendor( char *pszString )
{
	sprintf( g_szVendor, pszString );
}

//*****************************************************************************
//
void SERVERCONSOLE_Print( char *pszString )
{
	char	szBuffer[SERVERCONSOLE_TEXTLENGTH];
	char	szInputString[SERVERCONSOLE_TEXTLENGTH];
	char	*psz;
	char	c;
	bool	bScroll = false;
	bool	bRecycled = false;

	V_StripColors( pszString );

	if (( sv_timestamp ) && ( gamestate == GS_LEVEL ))
	{
		time_t			CurrentTime;
		struct	tm		*pTimeInfo;

		time( &CurrentTime );
		pTimeInfo = localtime( &CurrentTime );

		switch ( sv_timestampformat )
		{
		// HH:MM:SS
		case 0:

			sprintf( szInputString, "[%02d:%02d:%02d] ", pTimeInfo->tm_hour, pTimeInfo->tm_min, pTimeInfo->tm_sec );
			break;
		// HH:MM:SSAM/PM
		case 1:

			// It's AM if the hour in the day is less than 12.
			if ( pTimeInfo->tm_hour < 12 )
				sprintf( szInputString, "[%02d:%02d:%02d AM] ", ( pTimeInfo->tm_hour == 0 ) ? 12 : pTimeInfo->tm_hour, pTimeInfo->tm_min, pTimeInfo->tm_sec );
			else
				sprintf( szInputString, "[%02d:%02d:%02d PM] ", ( pTimeInfo->tm_hour == 12 ) ? 12 : pTimeInfo->tm_hour % 12, pTimeInfo->tm_min, pTimeInfo->tm_sec );
			break;
		// HH:MM:SS am/pm
		case 2:

			// It's AM if the hour in the day is less than 12.
			if ( pTimeInfo->tm_hour < 12 )
				sprintf( szInputString, "[%02d:%02d:%02d am] ", ( pTimeInfo->tm_hour == 0 ) ? 12 : pTimeInfo->tm_hour, pTimeInfo->tm_min, pTimeInfo->tm_sec );
			else
				sprintf( szInputString, "[%02d:%02d:%02d pm] ", ( pTimeInfo->tm_hour == 12 ) ? 12 : pTimeInfo->tm_hour % 12, pTimeInfo->tm_min, pTimeInfo->tm_sec );
			break;
		// HH:MM
		case 3:

			sprintf( szInputString, "[%02d:%02d] ", pTimeInfo->tm_hour, pTimeInfo->tm_min );
			break;
		// HH:MMAM/PM
		case 4:

			// It's AM if the hour in the day is less than 12.
			if ( pTimeInfo->tm_hour < 12 )
				sprintf( szInputString, "[%02d:%02d AM] ", ( pTimeInfo->tm_hour == 0 ) ? 12 : pTimeInfo->tm_hour, pTimeInfo->tm_min );
			else
				sprintf( szInputString, "[%02d:%02d PM] ", ( pTimeInfo->tm_hour == 12 ) ? 12 : pTimeInfo->tm_hour % 12, pTimeInfo->tm_min );
			break;
		// HH:MM am/pm
		case 5:

			// It's AM if the hour in the day is less than 12.
			if ( pTimeInfo->tm_hour < 12 )
				sprintf( szInputString, "[%02d:%02d am] ", ( pTimeInfo->tm_hour == 0 ) ? 12 : pTimeInfo->tm_hour, pTimeInfo->tm_min );
			else
				sprintf( szInputString, "[%02d:%02d pm] ", ( pTimeInfo->tm_hour == 12 ) ? 12 : pTimeInfo->tm_hour % 12, pTimeInfo->tm_min );
			break;
		default:

			sprintf( szInputString, "[%02d:%02d:%02d] ", pTimeInfo->tm_hour, pTimeInfo->tm_min, pTimeInfo->tm_sec );
			break;
		}

		// Incoming lines need a carriage return.
		psz = szInputString + strlen( szInputString );
	}
	else
	{
		// Incoming lines need a carriage return.
		psz = szInputString;
	}

	// Check where the scrollbars are.
	LONG	lVisibleLine;
	LONG	lLineDiff;

	lVisibleLine = SendDlgItemMessage( g_hDlg, IDC_CONSOLEBOX, EM_GETFIRSTVISIBLELINE, 0, 0 );
	lLineDiff = g_ulNumLines - lVisibleLine;
	bScroll = ( lLineDiff <= 8);

	while ( 1 )
	{
		c = *pszString++;
		if ( c == '\0' )
		{
			*psz = c;
			break;
		}
		if ( c == '\n' )
		{
			*psz++ = '\r';
			g_ulNumLines++;
		}
		*psz++ = c;
	}

	if ( GetDlgItemText( g_hDlg, IDC_CONSOLEBOX, szBuffer, sizeof( szBuffer )))
	{
		LONG	lDifference;
		char	szConsoleBuffer[SERVERCONSOLE_TEXTLENGTH];

		// If the amount of text added to the buffer will cause a buffer overflow, 
		// shuffle the text upwards.
		psz = szBuffer;
		if (( lDifference = ( (LONG)strlen( szBuffer ) + (LONG)strlen( szInputString ) - SERVERCONSOLE_TEXTLENGTH )) >= 0 )
		{
			bRecycled = true;
			while ( 1 )
			{
				psz++;
				lDifference--;
				if ( *psz == 0 )
					break;
				if ( lDifference < 0 )
				{
					while ( 1 )
					{
						if ( *psz == 0 )
						{
							psz++;
							break;
						}
						else if ( *psz == '\r' )
						{
							psz += 2;
							break;
						}
						psz++;
					}
					break;
				}
			}
		}

		sprintf( szConsoleBuffer, "%s%s", psz, szInputString );
		SetDlgItemText( g_hDlg, IDC_CONSOLEBOX, szConsoleBuffer );

		// If the user has scrolled all the way down, autoscroll.
		if ( bScroll )
			SendDlgItemMessage( g_hDlg, IDC_CONSOLEBOX, EM_LINESCROLL, 0, g_ulNumLines );

		// If they've scrolled up but we've trimmed the text, don't autoscroll and compensate. 
		else if( bRecycled && ( lVisibleLine > 0 ) )
			SendDlgItemMessage( g_hDlg, IDC_CONSOLEBOX, EM_LINESCROLL, 0, lVisibleLine - 1 );

		// If they've scrolled up, don't autoscroll.
		else
			SendDlgItemMessage( g_hDlg, IDC_CONSOLEBOX, EM_LINESCROLL, 0, lVisibleLine );
	}
}

#endif	// GUI_SERVER_CONSOLE
