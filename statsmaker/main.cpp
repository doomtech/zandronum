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

#define _CRT_SECURE_NO_DEPRECATE
#include "..\src\networkheaders.h"
#include "..\src\networkshared.h"
#include "main.h"
#include "network.h"

#include <windows.h>
#include <commctrl.h>
#include <math.h>
#include <time.h>

#define USE_WINDOWS_DWORD

#include <setupapi.h>
#include <uxtheme.h>
#include <shellapi.h>
#include <mmsystem.h>

#include "resource.h"

// Include the protocol for various ports.
#include "protocol_skulltag.h"
#include "protocol_zdaemon.h"

// Look pretty under XP and Vista.
#pragma comment(linker, "\"/manifestdependency:type='Win32' ""name='Microsoft.Windows.Common-Controls' ""version='6.0.0.0' ""processorArchitecture='*' ""publicKeyToken='6595b64144ccf1df' ""language='*'\"")

//*****************************************************************************
//	VARIABLES

// Thread handle for the main thread that gathers stats.
static	HANDLE					g_hThread;

// Number of lines present in the text box.
static	ULONG					g_ulNumLines = 0;

// Global handle for the main dialog box.
static	HWND					g_hDlg = NULL;

// Data for each of the port we're gathering stats for.
static	PORTINFO_s				g_PortInfo[NUM_PORTS];

static	UPDATETIME_s			g_NextQueryTime;
static	UPDATETIME_s			g_NextExportTime;
static	UPDATETIME_s			g_NextQueryRetryTime;

static	NOTIFYICONDATA			g_NotifyIconData;
static	HICON					g_hSmallIcon = NULL;
static	HINSTANCE				g_hInst;
static	bool					g_bSmallIconClicked = false;

static	FILE					*g_pPartialStatsFile;

//*****************************************************************************
//	PROTOTYPES

static	void					main_MainLoop( void );
static	void					main_ExportStats( void );
static	void					main_ClearServerList( void );
static	void					main_CalculateNextQueryTime( UPDATETIME_s *pInfo );
static	void					main_CalculateNextExportTime( UPDATETIME_s *pInfo );
static	void					main_CalculateNextRetryTime( UPDATETIME_s *pInfo );
static	bool					main_NeedRetry( void );
static	SERVERINFO_s			*main_FindServerByAddress( NETADDRESS_s Address );
static	void					main_ParsePartialStatsFile( void );
static	bool					main_NeedToDecode( void );

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
//	char			szString[16];
//	time_t			CurrentTime;
//	struct	tm		*pTimeInfo;

	switch ( Message )
	{
	case WM_CLOSE:

		if ( MessageBox( hDlg, "Are you sure you want to quit?", MAIN_TITLESTRING, MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2 ) == IDYES )
		{
			EndDialog( hDlg, -1 );
			CloseHandle( g_hThread );
/*
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
*/
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
	NETWORK_Construct( DEFAULT_STATS_PORT, false );

	// Setup the protocol modules.
	SKULLTAG_Construct( );
	ZDAEMON_Construct( );

	// Set up the master server info for Skulltag.
	NETWORK_StringToAddress( "skulltag.kicks-ass.net", &g_PortInfo[PORT_SKULLTAG].MasterServerInfo.Address );
	g_PortInfo[PORT_SKULLTAG].MasterServerInfo.Address.usPort = htons( 15300 );

	g_PortInfo[PORT_SKULLTAG].pvQueryMasterServer = SKULLTAG_QueryMasterServer;
	g_PortInfo[PORT_SKULLTAG].pvParseMasterServerResponse = SKULLTAG_ParseMasterServerResponse;
	g_PortInfo[PORT_SKULLTAG].pvQueryServer = SKULLTAG_QueryServer;
	g_PortInfo[PORT_SKULLTAG].pvParseServerResponse = SKULLTAG_ParseServerResponse;

	g_PortInfo[PORT_SKULLTAG].bHuffman = true;
	sprintf( g_PortInfo[PORT_SKULLTAG].szName, "Skulltag" );

	// Set up the master server info for ZDaemon.
	NETWORK_StringToAddress( "zdaemon.ath.cx", &g_PortInfo[PORT_ZDAEMON].MasterServerInfo.Address );
	g_PortInfo[PORT_ZDAEMON].MasterServerInfo.Address.usPort = htons( 15300 );

	g_PortInfo[PORT_ZDAEMON].pvQueryMasterServer = ZDAEMON_QueryMasterServer;
	g_PortInfo[PORT_ZDAEMON].pvParseMasterServerResponse = ZDAEMON_ParseMasterServerResponse;
	g_PortInfo[PORT_ZDAEMON].pvQueryServer = ZDAEMON_QueryServer;
	g_PortInfo[PORT_ZDAEMON].pvParseServerResponse = ZDAEMON_ParseServerResponse;

	g_PortInfo[PORT_ZDAEMON].bHuffman = false;
	sprintf( g_PortInfo[PORT_ZDAEMON].szName, "ZDaemon" );

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
//	LONG			lCommand;
	ULONG			ulIdx;
	ULONG			ulIdx2;
	BYTESTREAM_s	*pByteStream;
	LONG			lTotalNumPlayers;
	LONG			lTotalNumServers;
	float			fAverageNumPlayers;
	float			fAverageNumServers;
	char			szString[64];
	SERVERINFO_s	*pServer;

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

			for ( ulIdx = 0; ulIdx < NUM_PORTS; ulIdx++ )
			{
				g_PortInfo[ulIdx].aQueryInfo.Clear( );
				g_PortInfo[ulIdx].aServerInfo.Clear( );

				g_PortInfo[ulIdx].lMaxNumPlayers = 0;
				g_PortInfo[ulIdx].lMaxNumServers = 0;
			}

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

			Printf( "Querying master servers...\n" );
			for ( ulIdx = 0; ulIdx < NUM_PORTS; ulIdx++ )
			{
				g_PortInfo[ulIdx].pvQueryMasterServer( );
				g_PortInfo[ulIdx].MasterServerInfo.ulActiveState = AS_WAITINGFORREPLY;
			}

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
			for ( ulIdx = 0; ulIdx < NUM_PORTS; ulIdx++ )
			{
				if ( g_PortInfo[ulIdx].MasterServerInfo.ulActiveState == AS_WAITINGFORREPLY )
				{
					Printf( "Retrying %s master server...\n", g_PortInfo[ulIdx].szName );
					g_PortInfo[ulIdx].pvQueryMasterServer( );
				}
				else
				{
					Printf( "Retrying %s server(s)...\n", g_PortInfo[ulIdx].szName );
					for ( ulIdx2 = 0; ulIdx2 < g_PortInfo[ulIdx].aServerInfo.Size( ); ulIdx2++ )
					{
						if ( g_PortInfo[ulIdx].aServerInfo[ulIdx2].ulActiveState == AS_WAITINGFORREPLY )
						{
							g_PortInfo[ulIdx].pvQueryServer( &g_PortInfo[ulIdx].aServerInfo[ulIdx2] );
						}
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
			// If this is a Skulltag server, decode the packet using Huffman compression.
			if ( main_NeedToDecode( ))
				NETWORK_DecodePacket( );

			// Set up our byte stream.
			pByteStream = &NETWORK_GetNetworkMessageBuffer( )->ByteStream;
			pByteStream->pbStream = NETWORK_GetNetworkMessageBuffer( )->pbData;
			pByteStream->pbStreamEnd = pByteStream->pbStream + NETWORK_GetNetworkMessageBuffer( )->ulCurrentSize;

			for ( ulIdx = 0; ulIdx < NUM_PORTS; ulIdx++ )
			{
				if ( NETWORK_CompareAddress( NETWORK_GetFromAddress( ), g_PortInfo[ulIdx].MasterServerInfo.Address, false ))
				{
					if ( g_PortInfo[ulIdx].pvParseMasterServerResponse( pByteStream, g_PortInfo[ulIdx].aServerInfo, g_PortInfo[ulIdx].aQueryInfo ))
					{
						// We got a response.
						g_PortInfo[ulIdx].MasterServerInfo.ulActiveState = AS_ACTIVE;

						g_NextQueryRetryTime.lSecond = pTimeInfo->tm_sec;
						g_NextQueryRetryTime.lMinute = pTimeInfo->tm_min;
						g_NextQueryRetryTime.lHour = pTimeInfo->tm_hour;

						main_CalculateNextRetryTime( &g_NextQueryRetryTime );
						break;
					}
				}
				else
				{
					pServer = main_FindServerByAddress( NETWORK_GetFromAddress( ));

					if (( pServer ) &&
						( g_PortInfo[ulIdx].pvParseServerResponse( pByteStream, pServer, g_PortInfo[ulIdx].aQueryInfo )))
					{
						if ( g_PortInfo[ulIdx].aQueryInfo[g_PortInfo[ulIdx].aQueryInfo.Size( ) - 1].lNumPlayers > g_PortInfo[ulIdx].lMaxNumPlayers )
							g_PortInfo[ulIdx].lMaxNumPlayers = g_PortInfo[ulIdx].aQueryInfo[g_PortInfo[ulIdx].aQueryInfo.Size( ) - 1].lNumPlayers;
						if ( g_PortInfo[ulIdx].aQueryInfo[g_PortInfo[ulIdx].aQueryInfo.Size( ) - 1].lNumServers > g_PortInfo[ulIdx].lMaxNumServers )
							g_PortInfo[ulIdx].lMaxNumServers = g_PortInfo[ulIdx].aQueryInfo[g_PortInfo[ulIdx].aQueryInfo.Size( ) - 1].lNumServers;

						lTotalNumPlayers = 0;
						lTotalNumServers = 0;
						for ( ulIdx2 = 0; ulIdx2 < g_PortInfo[ulIdx].aQueryInfo.Size( ); ulIdx2++ )
						{
							lTotalNumPlayers += g_PortInfo[ulIdx].aQueryInfo[ulIdx2].lNumPlayers;
							lTotalNumServers += g_PortInfo[ulIdx].aQueryInfo[ulIdx2].lNumServers;
						}
						
						fAverageNumPlayers = (float)lTotalNumPlayers / (float)g_PortInfo[ulIdx].aQueryInfo.Size( );
						fAverageNumServers = (float)lTotalNumServers / (float)g_PortInfo[ulIdx].aQueryInfo.Size( );

						sprintf( szString, "Average players: %3.2f", fAverageNumPlayers );
						SetDlgItemText( g_hDlg, IDC_AVERAGEPLAYERS, szString );

						sprintf( szString, "Average servers: %3.2f", fAverageNumServers );
						SetDlgItemText( g_hDlg, IDC_AVERAGESERVERS, szString );

						sprintf( szString, "Num queries: %d", g_PortInfo[ulIdx].aQueryInfo.Size( ));
						SetDlgItemText( g_hDlg, IDC_NUMQUERIES, szString );

						sprintf( szString, "Max. players: %d", g_PortInfo[ulIdx].lMaxNumPlayers );
						SetDlgItemText( g_hDlg, IDC_MAXPLAYERS, szString );

						sprintf( szString, "Max. servers: %d", g_PortInfo[ulIdx].lMaxNumServers );
						SetDlgItemText( g_hDlg, IDC_MAXSERVERS, szString );
					}

					break;
				}
			}
		}
	}
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
	ULONG			ulIdx;
	ULONG			ulIdx2;
	float			fAverageNumPlayers;
	float			fAverageNumServers;
	LONG			lTotalNumPlayers;
	LONG			lTotalNumServers;

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

		// Write the date.
		sprintf( szOutString, "%02d/%02d/%02d\t", TimeInfo.tm_mon + 1, TimeInfo.tm_mday, ( TimeInfo.tm_year + 1900 ) % 100 );

		// Write the stats for each port.
		for ( ulIdx = 0; ulIdx < NUM_PORTS; ulIdx++ )
		{
			lTotalNumPlayers = 0;
			lTotalNumServers = 0;
			for ( ulIdx2 = 0; ulIdx2 < g_PortInfo[ulIdx].aQueryInfo.Size( ); ulIdx2++ )
			{
				lTotalNumPlayers += g_PortInfo[ulIdx].aQueryInfo[ulIdx2].lNumPlayers;
				lTotalNumServers += g_PortInfo[ulIdx].aQueryInfo[ulIdx2].lNumServers;
			}

			fAverageNumPlayers = (float)lTotalNumPlayers / (float)g_PortInfo[ulIdx].aQueryInfo.Size( );
			fAverageNumServers = (float)lTotalNumServers / (float)g_PortInfo[ulIdx].aQueryInfo.Size( );

			sprintf( szOutString, "%s%5.2f\t%5.2f\t%d\t%d\t%d\t", szOutString, fAverageNumPlayers, fAverageNumServers, g_PortInfo[ulIdx].lMaxNumPlayers, g_PortInfo[ulIdx].lMaxNumServers, g_PortInfo[ulIdx].aQueryInfo.Size( ) - 1 );
		}

		strcat( szOutString, "\n" );

		fputs( szOutString, pFile );
		fclose( pFile );
	}
}

//*****************************************************************************
//
static void main_ClearServerList( void )
{
	ULONG	ulIdx;
	ULONG	ulIdx2;

	for ( ulIdx = 0; ulIdx < NUM_PORTS; ulIdx++ )
	{
		g_PortInfo[ulIdx].MasterServerInfo.ulActiveState = AS_INACTIVE;
		for ( ulIdx2 = 0; ulIdx2 < g_PortInfo[ulIdx].aServerInfo.Size( ); ulIdx2++ )
		{
			g_PortInfo[ulIdx].aServerInfo[ulIdx2].ulActiveState = AS_INACTIVE;

			g_PortInfo[ulIdx].aServerInfo[ulIdx2].Address.abIP[0] = 0;
			g_PortInfo[ulIdx].aServerInfo[ulIdx2].Address.abIP[1] = 0;
			g_PortInfo[ulIdx].aServerInfo[ulIdx2].Address.abIP[2] = 0;
			g_PortInfo[ulIdx].aServerInfo[ulIdx2].Address.abIP[3] = 0;
			g_PortInfo[ulIdx].aServerInfo[ulIdx2].Address.usPort = 0;
		}
	}
}

//*****************************************************************************
//
static void main_CalculateNextQueryTime( UPDATETIME_s *pInfo )
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
static void main_CalculateNextExportTime( UPDATETIME_s *pInfo )
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
static void main_CalculateNextRetryTime( UPDATETIME_s *pInfo )
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
	ULONG	ulIdx2;

	for ( ulIdx = 0; ulIdx < NUM_PORTS; ulIdx++ )
	{
		// Check to see if any of the master servers haven't responded.
		if ( g_PortInfo[ulIdx].MasterServerInfo.ulActiveState == AS_WAITINGFORREPLY )
			return ( true );

		// If the master server has responded, check to see if we're still waiting for a
		// respone from any of the servers.
		for ( ulIdx2 = 0; ulIdx2 < g_PortInfo[ulIdx].aServerInfo.Size( ); ulIdx2++ )
		{
			if ( g_PortInfo[ulIdx].aServerInfo[ulIdx2].ulActiveState == AS_WAITINGFORREPLY )
				return ( true );
		}
	}

	return ( false );
}

//*****************************************************************************
//
static SERVERINFO_s *main_FindServerByAddress( NETADDRESS_s Address )
{
	ULONG	ulIdx;
	ULONG	ulIdx2;

	for ( ulIdx = 0; ulIdx < NUM_PORTS; ulIdx++ )
	{
		for ( ulIdx2 = 0; ulIdx2 < g_PortInfo[ulIdx].aServerInfo.Size( ); ulIdx2++ )
		{
			if ( NETWORK_CompareAddress( Address, g_PortInfo[ulIdx].aServerInfo[ulIdx2].Address, false ))
				return ( &g_PortInfo[ulIdx].aServerInfo[ulIdx2] );
		}
	}
	
	return ( NULL );
}

//*****************************************************************************
//
static void main_ParsePartialStatsFile( void )
{
/*
	char			szString[256];
	LONG			lPosition;
	char			cChar;
	time_t			CurrentTime;
	struct	tm		*pTimeInfo;
*/
/*
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
*/
}

//*****************************************************************************
//
static bool main_NeedToDecode( void )
{
	ULONG	ulIdx;
	ULONG	ulIdx2;

	for ( ulIdx = 0; ulIdx < NUM_PORTS; ulIdx++ )
	{
		if ( NETWORK_CompareAddress( NETWORK_GetFromAddress( ), g_PortInfo[ulIdx].MasterServerInfo.Address, false ))
			return ( g_PortInfo[ulIdx].bHuffman );

		for ( ulIdx2 = 0; ulIdx2 < g_PortInfo[ulIdx].aServerInfo.Size( ); ulIdx2++ )
		{
			if ( NETWORK_CompareAddress( NETWORK_GetFromAddress( ), g_PortInfo[ulIdx].aServerInfo[ulIdx2].Address, false ))
				return ( g_PortInfo[ulIdx].bHuffman );
		}
	}

	return ( false );
}
