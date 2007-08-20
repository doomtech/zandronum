//-----------------------------------------------------------------------------
//
// Skulltag Statistics Reporter Source
// Copyright (C) 2005 Brad Carney
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
// Date created:  4/4/05
//
//
// Filename: main.cpp
//
// Description: 
//
//-----------------------------------------------------------------------------
#include <windows.h>
#include <commctrl.h>
#define _CRT_SECURE_NO_DEPRECATE
#include <math.h>
#include <time.h>

#define USE_WINDOWS_DWORD

#include <setupapi.h>
#include <uxtheme.h>
#include <shellapi.h>
#include <mmsystem.h>

#include "main.h"
#include "network.h"
#include "resource.h"

// Look pretty under XP and Vista.
#pragma comment(linker, "\"/manifestdependency:type='Win32' ""name='Microsoft.Windows.Common-Controls' ""version='6.0.0.0' ""processorArchitecture='*' ""publicKeyToken='6595b64144ccf1df' ""language='*'\"")

//*****************************************************************************
//	VARIABLES

// Thread handle for the main thread that gathers stats.
HANDLE					g_hThread;

// Message buffer we write our commands to.
static	sizebuf_t		g_ServerBuffer;

// Message buffer we write our commands to.
static	sizebuf_t		g_MasterServerBuffer;

// Address of the master server.
static	netadr_t		g_AddressMasterServer;

// List of all the current servers.
static	SERVERINFO_t	g_ServerList[MAX_NUM_SERVERS];

// Number of lines present in the text box.
static	ULONG			g_ulNumLines = 0;

// Global handle for the main dialog box.
static	HWND			g_hDlg = NULL;

static	float			g_fAverageNumPlayers = 0.0f;
static	float			g_fAverageNumServers = 0.0f;

static	LONG			g_lNumPlayersThisQuery = 0;
static	LONG			g_lNumServersThisQuery = 0;

static	LONG			g_lTotalNumPlayers = 0;
static	LONG			g_lTotalNumServers = 0;

static	LONG			g_lMaxNumPlayers = 0;
static	LONG			g_lMaxNumServers = 0;

static	LONG			g_lNumQueries = 0;

static	UPDATETIME_t	g_NextQueryTime;
static	UPDATETIME_t	g_NextExportTime;
static	UPDATETIME_t	g_NextQueryRetryTime;

static	NOTIFYICONDATA	g_NotifyIconData;
static	HICON			g_hSmallIcon = NULL;
static	HINSTANCE		g_hInst;
static	bool			g_bSmallIconClicked = false;
//static	LONG			g_lQueryTicks = 0;
//static	LONG			g_lExportTicks = 0;
//static	LONG			g_lQueryRetryTicks = 0;

// Info for the master server.
static	SERVERINFO_t	g_MasterServerInfo;

static	FILE			*g_pPartialStatsFile;

//*****************************************************************************
//	PROTOTYPES

static	void		main_MainLoop( void );
static	void		main_QueryMasterServer( void );
static	void		main_ExportStats( void );
static	void		main_GetServerList( void );
static	void		main_QueryAllServers( void );
static	void		main_DeactivateAllServers( void );
static	void		main_ParseServerQuery( void );
static	LONG		main_GetNewServerID( void );
static	void		main_QueryServer( ULONG ulServer );
static	LONG		main_GetListIDByAddress( netadr_t Address );
static	void		main_ClearServerList( void );
static	void		main_CalculateNextQueryTime( UPDATETIME_t *pInfo );
static	void		main_CalculateNextExportTime( UPDATETIME_t *pInfo );
static	void		main_CalculateNextRetryTime( UPDATETIME_t *pInfo );
static	bool		main_NeedRetry( void );
static	void		main_ParsePartialStatsFile( void );

//*****************************************************************************
//	FUNCTIONS

int WINAPI WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd )
{
	// This never returns.
	g_hInst = hInstance;
	DialogBox( hInstance, MAKEINTRESOURCE( IDD_MAINDIALOG ), NULL, MAIN_MainDialogBoxCallback );
	

	return ( 0 );
}

//*****************************************************************************
//
BOOL CALLBACK MAIN_MainDialogBoxCallback( HWND hDlg, UINT Message, WPARAM wParam, LPARAM lParam )
{
	char			szString[16];
	time_t			CurrentTime;
	struct	tm		*pTimeInfo;

	switch ( Message )
	{
	case WM_CLOSE:

		//if ( MessageBox( hDlg, "Are you sure you want to quit?", MAIN_TITLESTRING, MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2 ) == IDYES )
		{
			EndDialog( hDlg, -1 );
			CloseHandle( g_hThread );

			if (( g_pPartialStatsFile = fopen( "partialstats.txt", "w" )) != NULL )
			{
				time( &CurrentTime );
				pTimeInfo = localtime( &CurrentTime );

				sprintf( szString, "%02d\n", pTimeInfo->tm_mon );
				fputs( szString, g_pPartialStatsFile );

				sprintf( szString, "%02d\n", pTimeInfo->tm_mday );
				fputs( szString, g_pPartialStatsFile );

				sprintf( szString, "%d\n", g_lTotalNumPlayers );
				fputs( szString, g_pPartialStatsFile );

				sprintf( szString, "%d\n", g_lTotalNumServers );
				fputs( szString, g_pPartialStatsFile );

				sprintf( szString, "%d\n", g_lMaxNumPlayers );
				fputs( szString, g_pPartialStatsFile );

				sprintf( szString, "%d\n", g_lMaxNumServers );
				fputs( szString, g_pPartialStatsFile );

				sprintf( szString, "%d\n", g_lNumQueries );
				fputs( szString, g_pPartialStatsFile );

				fclose( g_pPartialStatsFile );
			}
			exit( 0 );
		}
		break;
	case WM_DESTROY:

		Shell_NotifyIcon( NIM_DELETE, &g_NotifyIconData );
		PostQuitMessage( 0 );
		break;
	case WM_SYSCOMMAND:

		if ( wParam == SC_MINIMIZE )
		{
			RECT			DesktopRect;
			RECT			ThisWindowRect;
			ANIMATIONINFO	AnimationInfo;
			NOTIFYICONDATA	NotifyIconData;
			char			szString[64];

			AnimationInfo.cbSize = sizeof( AnimationInfo );
			SystemParametersInfo( SPI_GETANIMATION, sizeof( AnimationInfo ), &AnimationInfo, 0 );
			NotifyIconData.uFlags = NIF_TIP;

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
			
			sprintf( szString, "Hewwo!" );
			lstrcpy( NotifyIconData.szTip, szString );

			Shell_NotifyIcon( NIM_ADD, &NotifyIconData );
			break;
		}

		DefWindowProc( hDlg, Message, wParam, lParam );
		break;
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

				sprintf( szString, "Weeee" );
				lstrcpy( g_NotifyIconData.szTip, szString );

				Shell_NotifyIcon( NIM_DELETE, &NotifyIconData );
				g_bSmallIconClicked = false;
			}
			return ( TRUE );
		default:

			break;
		}
		return ( FALSE );
	case WM_INITDIALOG:
		{
			char		szString[256];

			g_hDlg = hDlg;

			// Initialize the server console text.
			sprintf( szString, "--== SKULLTAG STAT REPORTER ==--" );
			SetDlgItemText( hDlg, IDC_CONSOLEBOX, szString );
			Printf( "\n\n" );

			// Load the icons.
			g_hSmallIcon = (HICON)LoadImage( g_hInst,
					MAKEINTRESOURCE( IDI_ICON ),
					IMAGE_ICON,
					16,
					16,
					LR_SHARED );

			SendMessage( hDlg, WM_SETICON, (WPARAM)ICON_SMALL, (LPARAM)g_hSmallIcon );
			SendMessage( hDlg, WM_SETICON, (WPARAM)ICON_BIG, (LPARAM)LoadIcon( g_hInst, MAKEINTRESOURCE( IDI_ICON )));


			g_hThread = CreateThread( NULL, 0, MAIN_RunMainThread, 0, 0, 0 );
		}
		break;
	default:

		return ( FALSE );
	}

	return ( TRUE );
}

//*****************************************************************************
//
DWORD WINAPI MAIN_RunMainThread( LPVOID )
{
	// Set the local port.
	NETWORK_SetLocalPort( DEFAULT_STATS_PORT );

	// Initialize the network system.
	NETWORK_Initialize( );

	// Setup our master server message buffer.
	NETWORK_InitBuffer( &g_MasterServerBuffer, MAX_UDP_PACKET );
	NETWORK_ClearBuffer( &g_MasterServerBuffer );

	// Initialize the message buffer we send messages to individual servers in.
	NETWORK_InitBuffer( &g_ServerBuffer, MAX_UDP_PACKET );
	NETWORK_ClearBuffer( &g_ServerBuffer );

	NETWORK_StringToAddress( "skulltag.kicks-ass.net", &g_AddressMasterServer );
	g_AddressMasterServer.port = htons( 15300 );

	// Run the main loop of the program. This never returns.
	main_MainLoop( );

	return ( 0 );
}

//*****************************************************************************
//
void /*STACK_ARGS*/ Printf( const char *pszString, ... )
{
	va_list		ArgPtr;

	va_start( ArgPtr, pszString );
	VPrintf( pszString, ArgPtr );
	va_end( ArgPtr );
}

//*****************************************************************************
//
void VPrintf( const char *pszString, va_list Parms )
{
	char	szOutLine[8192];

	vsprintf( szOutLine, pszString, Parms );
	MAIN_Print( szOutLine );
}

//*****************************************************************************
//
void MAIN_Print( char *pszString )
{
	char	szBuffer[SERVERCONSOLE_TEXTLENGTH];
	char	szInputString[SERVERCONSOLE_TEXTLENGTH];
	char	*psz;
	char	c;
	bool	bScroll = false;

	if ( 1 )
	{
		time_t			CurrentTime;
		struct	tm		*pTimeInfo;

		time( &CurrentTime );
		pTimeInfo = localtime( &CurrentTime );

		// It's AM if the hour in the day is less than 12.
		if ( pTimeInfo->tm_hour < 12 )
			sprintf( szInputString, "[%02d:%02d:%02d am] ", ( pTimeInfo->tm_hour == 0 ) ? 12 : pTimeInfo->tm_hour, pTimeInfo->tm_min, pTimeInfo->tm_sec );
		else
			sprintf( szInputString, "[%02d:%02d:%02d pm] ", ( pTimeInfo->tm_hour == 12 ) ? 12 : pTimeInfo->tm_hour % 12, pTimeInfo->tm_min, pTimeInfo->tm_sec );

		// Incoming lines need a carriage return.
		psz = szInputString + strlen( szInputString );
	}
	else
	{
		// Incoming lines need a carriage return.
		psz = szInputString;
	}

	// If 
	{
		LONG	lLineDiff;

		lLineDiff = g_ulNumLines - SendDlgItemMessage( g_hDlg, IDC_CONSOLEBOX, EM_GETFIRSTVISIBLELINE, 0, 0 );
		bScroll = true;//(( lLineDiff <= 6 ) || g_bScrollConsoleOnNewline );
	}

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
		if (( lDifference = ( strlen( szBuffer ) + strlen( szInputString ) - SERVERCONSOLE_TEXTLENGTH )) >= 0 )
		{
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

		if ( bScroll )
			SendDlgItemMessage( g_hDlg, IDC_CONSOLEBOX, EM_LINESCROLL, 0, g_ulNumLines );
	}
}

//*****************************************************************************
//*****************************************************************************
//
static void main_MainLoop( void )
{
	LONG			lTime;
	LONG			lOldTime;
	LONG			lFirstTime;
	time_t			CurrentTime;
	struct	tm		*pTimeInfo;
	LONG			lCommand;
	ULONG			ulIdx;

	time( &CurrentTime );
	pTimeInfo = localtime( &CurrentTime );

	g_NextQueryTime.lMonth = pTimeInfo->tm_mon;
	g_NextQueryTime.lDay = pTimeInfo->tm_mday;
	g_NextQueryTime.lHour = pTimeInfo->tm_hour;
	g_NextQueryTime.lMinute = pTimeInfo->tm_min;
	main_CalculateNextQueryTime( &g_NextQueryTime );

	g_NextExportTime.lMonth = pTimeInfo->tm_mon;
	g_NextExportTime.lDay = pTimeInfo->tm_mday;
	g_NextExportTime.lHour = 0;
	g_NextExportTime.lMinute = 0;
	main_CalculateNextExportTime( &g_NextExportTime );

	lFirstTime = timeGetTime( );
	lOldTime = lFirstTime;

	// See if there's a partial data file. If there is, parse it. If it matches today's date,
	// then pick up from there.
	Printf( "Checking for partial stats file...\n" );
	if (( g_pPartialStatsFile = fopen( "partialstats.txt", "r" )) != NULL )
		main_ParsePartialStatsFile( );

	while ( 1 )
	{
		while ( 1 )
		{
			lTime = timeGetTime( );
			if (( lTime - lOldTime ) < ( 1000 / DESIRED_FRAMERATE ))
				Sleep( 1 );
			else
				break;
		}

		lOldTime = timeGetTime( );

		time( &CurrentTime );
		pTimeInfo = localtime( &CurrentTime );

		// At the end of the day, export stats to a text file.
		if (( g_NextExportTime.lDay == pTimeInfo->tm_mday ) &&
			( g_NextExportTime.lHour == pTimeInfo->tm_hour ) &&
			( g_NextExportTime.lMinute == pTimeInfo->tm_min ))
		{
			Printf( "Exporting daily stats...\n" );
			main_ExportStats( );

			g_lTotalNumPlayers = 0;
			g_lTotalNumServers = 0;

			g_lNumQueries = 0;

			g_lMaxNumPlayers = 0;
			g_lMaxNumServers = 0;

			// Calculate the next time to export stats.
			main_CalculateNextExportTime( &g_NextExportTime );
		}

		// If it's time to query the servers, get the list of servers from the master server.
		if (( g_NextQueryTime.lDay == pTimeInfo->tm_mday ) &&
			( g_NextQueryTime.lHour == pTimeInfo->tm_hour ) &&
			( g_NextQueryTime.lMinute == pTimeInfo->tm_min ))
		{
			// Clear out the existing server list.
			main_ClearServerList( );

			Printf( "Querying master server...\n" );
			main_QueryMasterServer( );

			// Calculate the next time to query all servers.
			main_CalculateNextQueryTime( &g_NextQueryTime );

			g_NextQueryRetryTime.lSecond = pTimeInfo->tm_sec;
			g_NextQueryRetryTime.lMinute = pTimeInfo->tm_min;
			g_NextQueryRetryTime.lHour = pTimeInfo->tm_hour;

			main_CalculateNextRetryTime( &g_NextQueryRetryTime );
		}

		// If it's time to requery ignored servers, do that now.
		if (( main_NeedRetry( )) &&
			( g_NextQueryRetryTime.lHour == pTimeInfo->tm_hour ) &&
			( g_NextQueryRetryTime.lMinute == pTimeInfo->tm_min ) &&
			( g_NextQueryRetryTime.lSecond == pTimeInfo->tm_sec ))
		{
			if ( g_MasterServerInfo.ulActiveState == AS_WAITINGFORREPLY )
			{
				Printf( "Retrying master server...\n" );
				main_QueryMasterServer( );
			}
			else
			{
				Printf( "Retrying servers...\n" );
				for ( ulIdx = 0; ulIdx < MAX_NUM_SERVERS; ulIdx++ )
				{
					if ( g_ServerList[ulIdx].ulActiveState == AS_WAITINGFORREPLY ) //||
//						( g_ServerList[ulIdx].ulActiveState == AS_IGNORED ))
					{
						main_QueryServer( ulIdx );
					}
				}
			}

			g_NextQueryRetryTime.lSecond = pTimeInfo->tm_sec;
			g_NextQueryRetryTime.lMinute = pTimeInfo->tm_min;
			g_NextQueryRetryTime.lHour = pTimeInfo->tm_hour;

			main_CalculateNextRetryTime( &g_NextQueryRetryTime );
		}

		// If we've received packets from somewhere, parse them.
		while ( NETWORK_GetPackets( ))
		{
			if ( NETWORK_CompareAddress( g_AddressFrom, g_AddressMasterServer ))
			{
				LONG	lCommand;

				lCommand = NETWORK_ReadLong( );
				switch ( lCommand )
				{
				case MSC_BEGINSERVERLIST:

					g_lNumQueries++;

					g_lNumPlayersThisQuery = 0;
					g_lNumServersThisQuery = 0;

					// Clear out the server list.
					main_DeactivateAllServers( );

					// Get the list of servers.
					Printf( "Receiving server list...\n" );
					main_GetServerList( );

					// Now, query all the servers on the list.
					main_QueryAllServers( );

					g_NextQueryRetryTime.lSecond = pTimeInfo->tm_sec;
					g_NextQueryRetryTime.lMinute = pTimeInfo->tm_min;
					g_NextQueryRetryTime.lHour = pTimeInfo->tm_hour;

					main_CalculateNextRetryTime( &g_NextQueryRetryTime );
					break;
				}
			}
			else
			{
				LONG	lServer;

				lCommand = NETWORK_ReadLong( );
				switch ( lCommand )
				{
				case SERVER_LAUNCHER_CHALLENGE:

					main_ParseServerQuery( );
					break;
				case SERVER_LAUNCHER_IGNORING:

//					lServer = main_GetListIDByAddress( g_AddressFrom );
//					if ( lServer != -1 )
//						g_ServerList[lServer].ulActiveState = AS_IGNORED;

					break;
				case SERVER_LAUNCHER_BANNED:

					lServer = main_GetListIDByAddress( g_AddressFrom );
					if ( lServer != -1 )
						g_ServerList[lServer].ulActiveState = AS_BANNED;

					break;
				}
			}
		}
	}
}

//*****************************************************************************
//
static void main_QueryMasterServer( void )
{
	// Clear out the buffer, and write out launcher challenge.
	NETWORK_ClearBuffer( &g_MasterServerBuffer );
	NETWORK_WriteLong( &g_MasterServerBuffer, LAUNCHER_CHALLENGE );

	// Send the master server our packet.
	NETWORK_LaunchPacket( g_MasterServerBuffer, g_AddressMasterServer, true );

	g_MasterServerInfo.ulActiveState = AS_WAITINGFORREPLY;
}

//*****************************************************************************
//
static void main_ExportStats( void )
{
	FILE			*pFile;
//	LONG			lIdx;
	time_t			CurrentTime;
	struct	tm		*pTimeInfo;
	tm				TimeInfo;

	time( &CurrentTime );
	pTimeInfo = localtime( &CurrentTime );

	TimeInfo = *pTimeInfo;
	TimeInfo.tm_mday--;
	if ( TimeInfo.tm_mday < 1 )
	{
		TimeInfo.tm_mon--;
		if ( TimeInfo.tm_mon < 0 )
			TimeInfo.tm_mon = 11;
		switch ( TimeInfo.tm_mon )
		{
		// 31 days in January.
		case 0:

			TimeInfo.tm_mday = 31;
			break;
		// 28 days in February.
		case 1:

			TimeInfo.tm_mday = 28;
			break;
		// 31 days in March.
		case 2:

			TimeInfo.tm_mday = 31;
			break;
		// 30 days in April.
		case 3:

			TimeInfo.tm_mday = 30;
			break;
		// 31 days in May.
		case 4:

			TimeInfo.tm_mday = 31;
			break;
		// 30 days in June.
		case 5:

			TimeInfo.tm_mday = 30;
			break;
		// 31 days in July.
		case 6:

			TimeInfo.tm_mday = 31;
			break;
		// 31 days in August.
		case 7:

			TimeInfo.tm_mday = 31;
			break;
		// 30 days in September.
		case 8:

			TimeInfo.tm_mday = 30;
			break;
		// 31 days in October.
		case 9:

			TimeInfo.tm_mday = 31;
			break;
		// 30 days in November.
		case 10:

			TimeInfo.tm_mday = 30;
			break;
		// 31 days in December.
		case 11:

			TimeInfo.tm_mday = 31;
			break;
		default:

			break;
		}
	}

	TimeInfo.tm_wday--;
	if ( TimeInfo.tm_wday < 0 )
		TimeInfo.tm_wday = 6;

	if ( pFile = fopen( "stats.txt", "a" ))
	{
		char	szOutString[512];
/*
		switch ( TimeInfo.tm_wday )
		{
		case 0:

			sprintf( szOutString, "STATISTICS FOR SUNDAY %02d/%02d/%04d\n", TimeInfo.tm_mon + 1, TimeInfo.tm_mday, TimeInfo.tm_year + 1900 );
			break;
		case 1:

			sprintf( szOutString, "STATISTICS FOR MONDAY %02d/%02d/%04d\n", TimeInfo.tm_mon + 1, TimeInfo.tm_mday, TimeInfo.tm_year + 1900 );
			break;
		case 2:

			sprintf( szOutString, "STATISTICS FOR TUESDAY %02d/%02d/%04d\n", TimeInfo.tm_mon + 1, TimeInfo.tm_mday, TimeInfo.tm_year + 1900 );
			break;
		case 3:

			sprintf( szOutString, "STATISTICS FOR WEDNESDAY %02d/%02d/%04d\n", TimeInfo.tm_mon + 1, TimeInfo.tm_mday, TimeInfo.tm_year + 1900 );
			break;
		case 4:

			sprintf( szOutString, "STATISTICS FOR THURSDAY %02d/%02d/%04d\n", TimeInfo.tm_mon + 1, TimeInfo.tm_mday, TimeInfo.tm_year + 1900 );
			break;
		case 5:

			sprintf( szOutString, "STATISTICS FOR FRIDAY %02d/%02d/%04d\n", TimeInfo.tm_mon + 1, TimeInfo.tm_mday, TimeInfo.tm_year + 1900 );
			break;
		case 6:

			sprintf( szOutString, "STATISTICS FOR SATURDAY %02d/%02d/%04d\n", TimeInfo.tm_mon + 1, TimeInfo.tm_mday, TimeInfo.tm_year + 1900 );
			break;
		default:

			sprintf( szOutString, "STATISTICS FOR <UNKNOWN DAY OF WEEK> %02d/%02d/%04d\n", TimeInfo.tm_mon + 1, TimeInfo.tm_mday, TimeInfo.tm_year + 1900 );
			break;
		}

		fputs( szOutString, pFile );

		for ( lIdx = 0; lIdx < ( (LONG)strlen( szOutString ) - 1 ); lIdx++ )
			fputs( "-", pFile );

		sprintf( szOutString, "\n\n" );
		fputs( szOutString, pFile );

		sprintf( szOutString, "Number of queries performed:\t%d\n", g_lNumQueries );
		fputs( szOutString, pFile );

		sprintf( szOutString, "Average number of players:\t%5.2f\n", g_fAverageNumPlayers );
		fputs( szOutString, pFile );

		sprintf( szOutString, "Average number of servers:\t%5.2f\n\n", g_fAverageNumServers );
		fputs( szOutString, pFile );

		sprintf( szOutString, "Maximum number of players:\t%d\n", g_lMaxNumPlayers );
		fputs( szOutString, pFile );

		sprintf( szOutString, "Maximum number of servers:\t%d\n\n", g_lMaxNumServers );
		fputs( szOutString, pFile );
*/
		sprintf( szOutString, "%02d/%02d/%02d\t%5.2f\t%5.2f\t%d\t%d\t%d\n", TimeInfo.tm_mon + 1, TimeInfo.tm_mday, ( TimeInfo.tm_year + 1900 ) % 100, g_fAverageNumPlayers, g_fAverageNumServers, g_lMaxNumPlayers, g_lMaxNumServers, g_lNumQueries );
		fputs( szOutString, pFile );

		fclose( pFile );
	}
}

//*****************************************************************************
//
static void main_GetServerList( void )
{
	ULONG	ulServer;

	g_MasterServerInfo.ulActiveState = AS_ACTIVE;

	while ( NETWORK_ReadByte( ) != MSC_ENDSERVERLIST )
	{
		// Receiving information about a new server.
		ulServer = main_GetNewServerID( );
		if ( ulServer == -1 )
		{
			Printf( "main_GetServerList: Server limit exceeded (>=%d servers)", MAX_NUM_SERVERS );
			return;
		}

		// This server is now active.
		g_ServerList[ulServer].ulActiveState = AS_WAITINGFORREPLY;

		// Read in address information.
		g_ServerList[ulServer].Address.ip[0] = NETWORK_ReadByte( );
		g_ServerList[ulServer].Address.ip[1] = NETWORK_ReadByte( );
		g_ServerList[ulServer].Address.ip[2] = NETWORK_ReadByte( );
		g_ServerList[ulServer].Address.ip[3] = NETWORK_ReadByte( );
		g_ServerList[ulServer].Address.port = htons( NETWORK_ReadShort( ));
	}
}

//*****************************************************************************
//
static void main_QueryAllServers( void )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAX_NUM_SERVERS; ulIdx++ )
	{
		if ( g_ServerList[ulIdx].ulActiveState == AS_WAITINGFORREPLY )
			main_QueryServer( ulIdx );
	}
}

//*****************************************************************************
//
static void main_DeactivateAllServers( void )
{
	ULONG	ulIdx;

	g_MasterServerInfo.ulActiveState = AS_WAITINGFORREPLY;
	for ( ulIdx = 0; ulIdx < MAX_NUM_SERVERS; ulIdx++ )
	{
//		if ( g_ServerList[ulIdx].ulActiveState != AS_WAITINGFORREPLY )
			g_ServerList[ulIdx].ulActiveState = AS_INACTIVE;
	}
}

//*****************************************************************************
//
static void main_ParseServerQuery( void )
{
	LONG		lGameType = GAMETYPE_COOPERATIVE;
	LONG		lNumPWADs;
	LONG		lNumPlayers;
	ULONG		ulIdx;
	ULONG		ulBits;
	LONG		lServer;
	char		szString[256];

	lServer = main_GetListIDByAddress( g_AddressFrom );
	if (( lServer == -1 ) || ( g_ServerList[lServer].ulActiveState != AS_WAITINGFORREPLY ))
	{
		while ( NETWORK_ReadByte( ) != -1 )
			;

		return;
	}

	// This server is now active.
	g_ServerList[lServer].ulActiveState = AS_ACTIVE;

	// Read in the time we sent to the server.
	NETWORK_ReadLong( );

	// Read in the version.
	sprintf( g_ServerList[lServer].szVersion, NETWORK_ReadString( ));
	if ( strnicmp( g_ServerList[lServer].szVersion, "0.97", 4 ) != 0 )
	{
		while ( NETWORK_ReadByte( ) != -1 )
			;

		return;
	}

	// Read in the bits.
	ulBits = NETWORK_ReadLong( );

	// Read the server name.
	if ( ulBits & SQF_NAME )
		sprintf( g_ServerList[lServer].szHostName, NETWORK_ReadString( ));

	// Read the website URL.
	if ( ulBits & SQF_URL )
		NETWORK_ReadString( );

	// Read the host's e-mail address.
	if ( ulBits & SQF_EMAIL )
		NETWORK_ReadString( );

	// Read the map name.
	if ( ulBits & SQF_MAPNAME )
		NETWORK_ReadString( );

	// Read the maximum number of clients.
	if ( ulBits & SQF_MAXCLIENTS )
		NETWORK_ReadByte( );

	// Maximum slots.
	if ( ulBits & SQF_MAXPLAYERS )
		NETWORK_ReadByte( );

	// Read in the PWAD information.
	if ( ulBits & SQF_PWADS )
	{
		lNumPWADs = NETWORK_ReadByte( );
		for ( ulIdx = 0; ulIdx < (ULONG)lNumPWADs; ulIdx++ )
			NETWORK_ReadString( );
	}

	// Read the game type.
	if ( ulBits & SQF_GAMETYPE )
	{
		g_ServerList[lServer].lGameType = NETWORK_ReadByte( );
		NETWORK_ReadByte( );
		NETWORK_ReadByte( );
	}

	// Game name.
	if ( ulBits & SQF_GAMENAME )
		NETWORK_ReadString( );

	// Read in the IWAD name.
	if ( ulBits & SQF_IWAD )
		NETWORK_ReadString( );

	// Force password.
	if ( ulBits & SQF_FORCEPASSWORD )
		NETWORK_ReadByte( );

	// Force join password.
	if ( ulBits & SQF_FORCEJOINPASSWORD )
		NETWORK_ReadByte( );

	// Game skill.
	if ( ulBits & SQF_GAMESKILL )
		NETWORK_ReadByte( );

	// Bot skill.
	if ( ulBits & SQF_BOTSKILL )
		NETWORK_ReadByte( );

	// DMFlags, DMFlags2, compatflags.
	if ( ulBits & SQF_DMFLAGS )
	{
		NETWORK_ReadLong( );
		NETWORK_ReadLong( );
		NETWORK_ReadLong( );
	}

	// Fraglimit, timelimit, duellimit, pointlimit, winlimit.
	if ( ulBits & SQF_LIMITS )
	{
		NETWORK_ReadShort( );
		if ( NETWORK_ReadShort( ))
		{
			// Time left.
			NETWORK_ReadShort( );
		}

		NETWORK_ReadShort( );
		NETWORK_ReadShort( );
		NETWORK_ReadShort( );
	}

		// Team damage scale.
	if ( ulBits & SQF_TEAMDAMAGE )
		NETWORK_ReadFloat( );

	if ( ulBits & SQF_TEAMSCORES )
	{
		// Blue score.
		NETWORK_ReadShort( );

		// Red score.
		NETWORK_ReadShort( );
	}

	// Read in the number of players.
	if ( ulBits & SQF_NUMPLAYERS )
	{
		g_ServerList[lServer].lNumPlayers = NETWORK_ReadByte( );
		lNumPlayers = g_ServerList[lServer].lNumPlayers;

		if ( ulBits & SQF_PLAYERDATA )
		{
			for ( ulIdx = 0; ulIdx < (ULONG)g_ServerList[lServer].lNumPlayers; ulIdx++ )
			{
				// Read in this player's name.
				NETWORK_ReadString( );

				// Read in "fragcount" (could be frags, points, etc.)
				NETWORK_ReadShort( );

				// Read in the player's ping.
				NETWORK_ReadShort( );

				// Read in whether or not the player is spectating.
				NETWORK_ReadByte( );

				// Read in whether or not the player is a bot.
				if ( NETWORK_ReadByte( ))
					lNumPlayers--;

				if (( g_ServerList[lServer].lGameType == GAMETYPE_TEAMPLAY ) ||
					( g_ServerList[lServer].lGameType == GAMETYPE_TEAMLMS ) ||
					( g_ServerList[lServer].lGameType == GAMETYPE_TEAMPOSSESSION ) ||
					( g_ServerList[lServer].lGameType == GAMETYPE_SKULLTAG ) ||
					( g_ServerList[lServer].lGameType == GAMETYPE_CTF ) ||
					( g_ServerList[lServer].lGameType == GAMETYPE_ONEFLAGCTF ))
				{
					// Team.
					NETWORK_ReadByte( );
				}

				// Time.
				NETWORK_ReadByte( );
			}
		}
	}

	g_lNumPlayersThisQuery += lNumPlayers;
	g_lTotalNumPlayers += lNumPlayers;

	g_lNumServersThisQuery++;
	g_lTotalNumServers++;

	if ( g_lNumPlayersThisQuery > g_lMaxNumPlayers )
		g_lMaxNumPlayers = g_lNumPlayersThisQuery;
	if ( g_lNumServersThisQuery > g_lMaxNumServers )
		g_lMaxNumServers = g_lNumServersThisQuery;

	g_fAverageNumPlayers = (float)g_lTotalNumPlayers / (float)g_lNumQueries;
	g_fAverageNumServers = (float)g_lTotalNumServers / (float)g_lNumQueries;

	sprintf( szString, "Average players: %3.2f", g_fAverageNumPlayers );
	SetDlgItemText( g_hDlg, IDC_AVERAGEPLAYERS, szString );

	sprintf( szString, "Average servers: %3.2f", g_fAverageNumServers );
	SetDlgItemText( g_hDlg, IDC_AVERAGESERVERS, szString );

	sprintf( szString, "Num queries: %d", g_lNumQueries );
	SetDlgItemText( g_hDlg, IDC_NUMQUERIES, szString );

	sprintf( szString, "Max. players: %d", g_lMaxNumPlayers );
	SetDlgItemText( g_hDlg, IDC_MAXPLAYERS, szString );

	sprintf( szString, "Max. servers: %d", g_lMaxNumServers );
	SetDlgItemText( g_hDlg, IDC_MAXSERVERS, szString );
}

//*****************************************************************************
//
static LONG main_GetNewServerID( void )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAX_NUM_SERVERS; ulIdx++ )
	{
		if ( g_ServerList[ulIdx].ulActiveState == AS_INACTIVE )
			return ( ulIdx );
	}

	return ( -1 );
}

//*****************************************************************************
//
static void main_QueryServer( ULONG ulServer )
{
	// Clear out the buffer, and write out launcher challenge.
	NETWORK_ClearBuffer( &g_ServerBuffer );
	NETWORK_WriteLong( &g_ServerBuffer, LAUNCHER_CHALLENGE );
	NETWORK_WriteLong( &g_ServerBuffer, SQF_NUMPLAYERS|SQF_PLAYERDATA );
	NETWORK_WriteLong( &g_ServerBuffer, 0 );

	// Send the server our packet.
	NETWORK_LaunchPacket( g_ServerBuffer, g_ServerList[ulServer].Address, true );
}

//*****************************************************************************
//
static LONG main_GetListIDByAddress( netadr_t Address )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAX_NUM_SERVERS; ulIdx++ )
	{
		if ( NETWORK_CompareAddress( g_ServerList[ulIdx].Address, Address ))
			return ( ulIdx );
	}

	return ( -1 );
}

//*****************************************************************************
//
static void main_ClearServerList( void )
{
	ULONG	ulIdx;

	g_MasterServerInfo.ulActiveState = AS_INACTIVE;
	for ( ulIdx = 0; ulIdx < MAX_NUM_SERVERS; ulIdx++ )
	{
		g_ServerList[ulIdx].ulActiveState = AS_INACTIVE;

		g_ServerList[ulIdx].Address.ip[0] = 0;
		g_ServerList[ulIdx].Address.ip[1] = 0;
		g_ServerList[ulIdx].Address.ip[2] = 0;
		g_ServerList[ulIdx].Address.ip[3] = 0;
		g_ServerList[ulIdx].Address.port = 0;
	}
}

//*****************************************************************************
//
static void main_CalculateNextQueryTime( UPDATETIME_t *pInfo )
{
	pInfo->lMinute++;
	while (( pInfo->lMinute % 5 ) != 0 )
		pInfo->lMinute++;

	if ( pInfo->lMinute >= 60 )
	{
		pInfo->lMinute = 0;
		pInfo->lHour++;
	}

	if ( pInfo->lHour >= 24 )
	{
		pInfo->lHour = 0;
		pInfo->lDay++;
		switch ( pInfo->lMonth )
		{
		// 31 days in January.
		case 0:

			if ( pInfo->lDay > 31 )
			{
				pInfo->lDay = 1;
				pInfo->lMonth++;
			}
			break;
		// 28 days in February.
		case 1:

			if ( pInfo->lDay > 28 )
			{
				pInfo->lDay = 1;
				pInfo->lMonth++;
			}
			break;
		// 31 days in March.
		case 2:

			if ( pInfo->lDay > 31 )
			{
				pInfo->lDay = 1;
				pInfo->lMonth++;
			}
			break;
		// 30 days in April.
		case 3:

			if ( pInfo->lDay > 30 )
			{
				pInfo->lDay = 1;
				pInfo->lMonth++;
			}
			break;
		// 31 days in May.
		case 4:

			if ( pInfo->lDay > 31 )
			{
				pInfo->lDay = 1;
				pInfo->lMonth++;
			}
			break;
		// 30 days in June.
		case 5:

			if ( pInfo->lDay > 30 )
			{
				pInfo->lDay = 1;
				pInfo->lMonth++;
			}
			break;
		// 31 days in July.
		case 6:

			if ( pInfo->lDay > 31 )
			{
				pInfo->lDay = 1;
				pInfo->lMonth++;
			}
			break;
		// 31 days in August.
		case 7:

			if ( pInfo->lDay > 31 )
			{
				pInfo->lDay = 1;
				pInfo->lMonth++;
			}
			break;
		// 30 days in September.
		case 8:

			if ( pInfo->lDay > 30 )
			{
				pInfo->lDay = 1;
				pInfo->lMonth++;
			}
			break;
		// 31 days in October.
		case 9:

			if ( pInfo->lDay > 31 )
			{
				pInfo->lDay = 1;
				pInfo->lMonth++;
			}
			break;
		// 30 days in November.
		case 10:

			if ( pInfo->lDay > 30 )
			{
				pInfo->lDay = 1;
				pInfo->lMonth++;
			}
			break;
		// 31 days in December.
		case 11:

			if ( pInfo->lDay > 31 )
			{
				pInfo->lDay = 1;
				pInfo->lMonth = 0;
			}
			break;
		default:

			break;
		}
	}
}

//*****************************************************************************
//
static void main_CalculateNextExportTime( UPDATETIME_t *pInfo )
{
	pInfo->lMinute = 0;
	pInfo->lHour = 0;

	pInfo->lDay++;
	switch ( pInfo->lMonth )
	{
	// 31 days in January.
	case 0:

		if ( pInfo->lDay > 31 )
		{
			pInfo->lDay = 1;
			pInfo->lMonth++;
		}
		break;
	// 28 days in February.
	case 1:

		if ( pInfo->lDay > 28 )
		{
			pInfo->lDay = 1;
			pInfo->lMonth++;
		}
		break;
	// 31 days in March.
	case 2:

		if ( pInfo->lDay > 31 )
		{
			pInfo->lDay = 1;
			pInfo->lMonth++;
		}
		break;
	// 30 days in April.
	case 3:

		if ( pInfo->lDay > 30 )
		{
			pInfo->lDay = 1;
			pInfo->lMonth++;
		}
		break;
	// 31 days in May.
	case 4:

		if ( pInfo->lDay > 31 )
		{
			pInfo->lDay = 1;
			pInfo->lMonth++;
		}
		break;
	// 30 days in June.
	case 5:

		if ( pInfo->lDay > 30 )
		{
			pInfo->lDay = 1;
			pInfo->lMonth++;
		}
		break;
	// 31 days in July.
	case 6:

		if ( pInfo->lDay > 31 )
		{
			pInfo->lDay = 1;
			pInfo->lMonth++;
		}
		break;
	// 31 days in August.
	case 7:

		if ( pInfo->lDay > 31 )
		{
			pInfo->lDay = 1;
			pInfo->lMonth++;
		}
		break;
	// 30 days in September.
	case 8:

		if ( pInfo->lDay > 30 )
		{
			pInfo->lDay = 1;
			pInfo->lMonth++;
		}
		break;
	// 31 days in October.
	case 9:

		if ( pInfo->lDay > 31 )
		{
			pInfo->lDay = 1;
			pInfo->lMonth++;
		}
		break;
	// 30 days in November.
	case 10:

		if ( pInfo->lDay > 30 )
		{
			pInfo->lDay = 1;
			pInfo->lMonth++;
		}
		break;
	// 31 days in December.
	case 11:

		if ( pInfo->lDay > 31 )
		{
			pInfo->lDay = 1;
			pInfo->lMonth = 0;
		}
		break;
	default:

		break;
	}
}

//*****************************************************************************
//
static void main_CalculateNextRetryTime( UPDATETIME_t *pInfo )
{
	pInfo->lSecond++;
	while (( pInfo->lSecond % 10 ) != 0 )
		pInfo->lSecond++;

	if ( pInfo->lSecond >= 60 )
	{
		pInfo->lSecond = 0;
		pInfo->lMinute++;
	}

	if ( pInfo->lMinute >= 60 )
	{
		pInfo->lMinute = 0;
		pInfo->lHour++;
	}

	if ( pInfo->lHour >= 24 )
		pInfo->lHour = 0;
}

//*****************************************************************************
//
static bool main_NeedRetry( void )
{
	ULONG	ulIdx;

	if ( g_MasterServerInfo.ulActiveState == AS_WAITINGFORREPLY )
		return ( true );

	for ( ulIdx = 0; ulIdx < MAX_NUM_SERVERS; ulIdx++ )
	{
		if ( g_ServerList[ulIdx].ulActiveState == AS_WAITINGFORREPLY )// ||
//			( g_ServerList[ulIdx].ulActiveState == AS_IGNORED ))
		{
			return ( true );
		}
	}

	return ( false );
}

//*****************************************************************************
//
static void main_ParsePartialStatsFile( void )
{
	char			szString[256];
	LONG			lPosition;
	char			cChar;
	time_t			CurrentTime;
	struct	tm		*pTimeInfo;

	time( &CurrentTime );
	pTimeInfo = localtime( &CurrentTime );
	
	Printf( "Parsing partial stats file...\n" );

	lPosition = 0;
	szString[0] = 0;

	// First, read the month.
	while ( 1 )
	{
		cChar = fgetc( g_pPartialStatsFile );
		if ( cChar == -1 )
		{
			Printf( "Premature end of file!\n" );
			return;
		}

		if (( cChar == '\n' ) || ( cChar == 'r' ))
		{
			if ( atoi( szString ) != pTimeInfo->tm_mon )
			{
				Printf( "Partial stats file of a different month. Ignoring...\n" );
				return;
			}
			break;
		}

		szString[lPosition++] = cChar;
		szString[lPosition] = 0;
	}

	lPosition = 0;
	szString[0] = 0;

	// Then read the day.
	while ( 1 )
	{
		cChar = fgetc( g_pPartialStatsFile );
		if ( cChar == -1 )
		{
			Printf( "Premature end of file!\n" );
			return;
		}

		if (( cChar == '\n' ) || ( cChar == 'r' ))
		{
			if ( atoi( szString ) != pTimeInfo->tm_mday )
			{
				Printf( "Partial stats file of a different day. Ignoring...\n" );
				return;
			}
			break;
		}

		szString[lPosition++] = cChar;
		szString[lPosition] = 0;
	}

	lPosition = 0;
	szString[0] = 0;

	// Then read the total number of players.
	while ( 1 )
	{
		cChar = fgetc( g_pPartialStatsFile );
		if ( cChar == -1 )
		{
			Printf( "Premature end of file!\n" );
			return;
		}

		if (( cChar == '\n' ) || ( cChar == 'r' ))
		{
			g_lTotalNumPlayers = atoi( szString );
			break;
		}

		szString[lPosition++] = cChar;
		szString[lPosition] = 0;
	}

	lPosition = 0;
	szString[0] = 0;

	while ( 1 )
	{
		cChar = fgetc( g_pPartialStatsFile );
		if ( cChar == -1 )
		{
			Printf( "Premature end of file!\n" );
			g_lTotalNumPlayers = 0;
			return;
		}

		if (( cChar == '\n' ) || ( cChar == 'r' ))
		{
			g_lTotalNumServers = atoi( szString );
			break;
		}

		szString[lPosition++] = cChar;
		szString[lPosition] = 0;
	}

	lPosition = 0;
	szString[0] = 0;

	while ( 1 )
	{
		cChar = fgetc( g_pPartialStatsFile );
		if ( cChar == -1 )
		{
			Printf( "Premature end of file!\n" );
			g_lTotalNumPlayers = 0;
			g_lTotalNumServers = 0;
			return;
		}

		if (( cChar == '\n' ) || ( cChar == 'r' ))
		{
			g_lMaxNumPlayers = atoi( szString );
			break;
		}

		szString[lPosition++] = cChar;
		szString[lPosition] = 0;
	}

	lPosition = 0;
	szString[0] = 0;

	while ( 1 )
	{
		cChar = fgetc( g_pPartialStatsFile );
		if ( cChar == -1 )
		{
			Printf( "Premature end of file!\n" );
			g_lTotalNumPlayers = 0;
			g_lTotalNumServers = 0;
			g_lMaxNumPlayers = 0;
			return;
		}

		if (( cChar == '\n' ) || ( cChar == 'r' ))
		{
			g_lMaxNumServers = atoi( szString );
			break;
		}

		szString[lPosition++] = cChar;
		szString[lPosition] = 0;
	}

	lPosition = 0;
	szString[0] = 0;

	while ( 1 )
	{
		cChar = fgetc( g_pPartialStatsFile );
		if ( cChar == -1 )
		{
			Printf( "Premature end of file!\n" );
			g_lTotalNumPlayers = 0;
			g_lTotalNumServers = 0;
			g_lMaxNumPlayers = 0;
			g_lMaxNumServers = 0;
			return;
		}

		if (( cChar == '\n' ) || ( cChar == 'r' ))
		{
			g_lNumQueries = atoi( szString );
			break;
		}

		szString[lPosition++] = cChar;
		szString[lPosition] = 0;
	}

	Printf( "Success!\n" );

	g_fAverageNumPlayers = (float)g_lTotalNumPlayers / (float)g_lNumQueries;
	g_fAverageNumServers = (float)g_lTotalNumServers / (float)g_lNumQueries;

	sprintf( szString, "Average players: %3.2f", g_fAverageNumPlayers );
	SetDlgItemText( g_hDlg, IDC_AVERAGEPLAYERS, szString );

	sprintf( szString, "Average servers: %3.2f", g_fAverageNumServers );
	SetDlgItemText( g_hDlg, IDC_AVERAGESERVERS, szString );

	sprintf( szString, "Num queries: %d", g_lNumQueries );
	SetDlgItemText( g_hDlg, IDC_NUMQUERIES, szString );

	sprintf( szString, "Max. players: %d", g_lMaxNumPlayers );
	SetDlgItemText( g_hDlg, IDC_MAXPLAYERS, szString );

	sprintf( szString, "Max. servers: %d", g_lMaxNumServers );
	SetDlgItemText( g_hDlg, IDC_MAXSERVERS, szString );
}
