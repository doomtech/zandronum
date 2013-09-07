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
// $Log:$
//
// DESCRIPTION:
//
//-----------------------------------------------------------------------------


#include <stdlib.h>
#include <stdio.h>
#include <io.h>
#include <direct.h>
#include <string.h>
//#include <process.h>
#include <time.h>

#include <stdarg.h>
#include <sys/types.h>
#include <sys/timeb.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <richedit.h>
#include <wincrypt.h>
// [BB] New #includes.
#include <shellapi.h>
#include <shlobj.h>

#ifdef _MSC_VER
#pragma warning(disable:4244)
#endif

#define USE_WINDOWS_DWORD
#include "hardware.h"
#include "doomerrors.h"
#include <math.h>

#include "doomtype.h"
#include "version.h"
#include "doomdef.h"
#include "cmdlib.h"
#include "m_argv.h"
#include "m_misc.h"
#include "i_video.h"
#include "i_sound.h"
#include "i_music.h"
#include "resource.h"
#include "x86.h"
#include "stats.h"

#include "d_main.h"
#include "d_net.h"
#include "g_game.h"
#include "i_input.h"
#include "i_system.h"
#include "c_dispatch.h"
#include "templates.h"
#include "gameconfigfile.h"
#include "v_font.h"
#include "g_level.h"
#include "doomstat.h"
#include "v_palette.h"
// [BC] New #includes.
#include "announcer.h"
#include "network.h"
#include "cl_demo.h"
#include "cl_main.h"
#include "campaign.h"
#include "stats.h"

EXTERN_CVAR (String, language)

extern void CheckCPUID(CPUInfo *cpu);

// Used on welcome/IWAD screen.
EXTERN_CVAR (Int, vid_renderer)
EXTERN_CVAR (Bool, fullscreen)
EXTERN_CVAR (Bool, gl_vid_compatibility)
EXTERN_CVAR (Bool, gl_nogl)


extern HWND Window, ConWindow, GameTitleWindow;
extern HANDLE StdOut;
extern bool FancyStdOut;
extern HINSTANCE g_hInst;

double PerfToSec, PerfToMillisec;

UINT TimerPeriod;
UINT TimerEventID;
UINT MillisecondsPerTic;
HANDLE NewTicArrived;
uint32 LanguageIDs[4];
void CalculateCPUSpeed ();

const IWADInfo *DoomStartupInfo;

int (*I_GetTime) (bool saveMS);
int (*I_WaitForTic) (int);
void (*I_FreezeTime) (bool frozen);

os_t OSPlatform;

void I_Tactile (int on, int off, int total)
{
  // UNUSED.
  on = off = total = 0;
}

ticcmd_t emptycmd;
ticcmd_t *I_BaseTiccmd(void)
{
	return &emptycmd;
}

static DWORD basetime = 0;

// [RH] Returns time in milliseconds
unsigned int I_MSTime (void)
{
	DWORD tm;

	tm = timeGetTime();
	if (!basetime)
		basetime = tm;

	return tm - basetime;
}

static DWORD TicStart;
static DWORD TicNext;
static int TicFrozen;

//
// I_GetTime
// returns time in 1/35th second tics
//
int I_GetTimePolled (bool saveMS)
{
	DWORD tm;

	if (TicFrozen != 0)
	{
		return TicFrozen;
	}

	tm = timeGetTime();
	if (!basetime)
		basetime = tm;

	if (saveMS)
	{
		TicStart = tm;
		TicNext = (tm * TICRATE / 1000 + 1) * 1000 / TICRATE;
	}

	return ((tm-basetime)*TICRATE)/1000;
}

float I_GetTimeFloat( void )
{
	DWORD tm;

	tm = timeGetTime();
	if (!basetime)
		basetime = tm;

	return (( (float)tm - (float)basetime ) * (float)TICRATE ) / 1000.0f;
}

/*
int I_GetMSElapsed( void )
{
	DWORD tm;

	tm = timeGetTime();
	if (!basetime)
		basetime = tm;

	return ( tm - basetime );
}
*/

void I_Sleep( int iMS )
{
	Sleep( iMS );
}

int I_WaitForTicPolled (int prevtic)
{
	int time;

	assert(TicFrozen == 0);
	while ((time = I_GetTimePolled(false)) <= prevtic)
		;

	return time;
}

void I_FreezeTimePolled (bool frozen)
{
	if (frozen)
	{
		assert(TicFrozen == 0);
		TicFrozen = I_GetTimePolled(false);
	}
	else
	{
		assert(TicFrozen != 0);
		int froze = TicFrozen;
		TicFrozen = 0;
		int now = I_GetTimePolled(false);
		basetime += (now - froze) * 1000 / TICRATE;
	}
}


static int tics;
static DWORD ted_start, ted_next;

int I_GetTimeEventDriven (bool saveMS)
{
	if (saveMS)
	{
		TicStart = ted_start;
		TicNext = ted_next;
	}
	return tics;
}

int I_WaitForTicEvent (int prevtic)
{
	assert(!TicFrozen);
	while (prevtic >= tics)
	{
		WaitForSingleObject(NewTicArrived, 1000/TICRATE);
	}

	return tics;
}

void CALLBACK TimerTicked (UINT id, UINT msg, DWORD_PTR user, DWORD_PTR dw1, DWORD_PTR dw2)
{
	if (!TicFrozen)
	{
		tics++;
	}
	ted_start = timeGetTime ();
	ted_next = ted_start + MillisecondsPerTic;
	SetEvent (NewTicArrived);
}

void I_FreezeTimeEventDriven(bool frozen)
{
	TicFrozen = frozen;
}

// Returns the fractional amount of a tic passed since the most recent tic
fixed_t I_GetTimeFrac (uint32 *ms)
{
	DWORD now = timeGetTime();
	if (ms) *ms = TicNext;
	DWORD step = TicNext - TicStart;
	if (step == 0)
	{
		return FRACUNIT;
	}
	else
	{
		fixed_t frac = clamp<fixed_t> ((now - TicStart)*FRACUNIT/step, 0, FRACUNIT);
		return frac;
	}
}

void I_WaitVBL (int count)
{
	// I_WaitVBL is never used to actually synchronize to the
	// vertical blank. Instead, it's used for delay purposes.
	Sleep (1000 * count / 70);
}

// [RH] Detect the OS the game is running under
void I_DetectOS (void)
{
	OSVERSIONINFO info;
	const char *osname;

	info.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	GetVersionEx (&info);

	switch (info.dwPlatformId)
	{
	case VER_PLATFORM_WIN32_WINDOWS:
		OSPlatform = os_Win95;
		if (info.dwMinorVersion < 10)
		{
			osname = "95";
		}
		else if (info.dwMinorVersion < 90)
		{
			osname = "98";
		}
		else
		{
			osname = "Me";
		}
		break;

	case VER_PLATFORM_WIN32_NT:
		OSPlatform = info.dwMajorVersion < 5 ? os_WinNT4 : os_Win2k;
		osname = "NT";
		if (info.dwMajorVersion == 5)
		{
			if (info.dwMinorVersion == 0)
			{
				osname = "2000";
			}
			if (info.dwMinorVersion == 1)
			{
				osname = "XP";
			}
			else if (info.dwMinorVersion == 2)
			{
				osname = "Server 2003";
			}
		}
		else if (info.dwMajorVersion == 6 && info.dwMinorVersion == 0)
		{
			osname = "Vista";
		}
		break;

	default:
		OSPlatform = os_unknown;
		osname = "Unknown OS";
		break;
	}

	if (OSPlatform == os_Win95)
	{
		Printf ("OS: Windows %s %lu.%lu.%lu %s\n",
				osname,
				info.dwMajorVersion, info.dwMinorVersion,
				info.dwBuildNumber & 0xffff, info.szCSDVersion);
	}
	else
	{
		Printf ("OS: Windows %s %lu.%lu (Build %lu)\n    %s\n",
				osname,
				info.dwMajorVersion, info.dwMinorVersion,
				info.dwBuildNumber, info.szCSDVersion);
	}

	if (OSPlatform == os_unknown)
	{
		Printf ("(Assuming Windows 95)\n");
		OSPlatform = os_Win95;
	}
}

//
// SubsetLanguageIDs
//
static void SubsetLanguageIDs (LCID id, LCTYPE type, int idx)
{
	char buf[8];
	LCID langid;
	char *idp;

	if (!GetLocaleInfo (id, type, buf, 8))
		return;
	langid = MAKELCID (strtoul(buf, NULL, 16), SORT_DEFAULT);
	if (!GetLocaleInfo (langid, LOCALE_SABBREVLANGNAME, buf, 8))
		return;
	idp = (char *)(&LanguageIDs[idx]);
	memset (idp, 0, 4);
	idp[0] = tolower(buf[0]);
	idp[1] = tolower(buf[1]);
	idp[2] = tolower(buf[2]);
	idp[3] = 0;
}

//
// SetLanguageIDs
//
void SetLanguageIDs ()
{
	size_t langlen = strlen (language);

	if (langlen < 2 || langlen > 3)
	{
		memset (LanguageIDs, 0, sizeof(LanguageIDs));
		SubsetLanguageIDs (LOCALE_USER_DEFAULT, LOCALE_ILANGUAGE, 0);
		SubsetLanguageIDs (LOCALE_USER_DEFAULT, LOCALE_IDEFAULTLANGUAGE, 1);
		SubsetLanguageIDs (LOCALE_SYSTEM_DEFAULT, LOCALE_ILANGUAGE, 2);
		SubsetLanguageIDs (LOCALE_SYSTEM_DEFAULT, LOCALE_IDEFAULTLANGUAGE, 3);
	}
	else
	{
		DWORD lang = 0;

		((BYTE *)&lang)[0] = (language)[0];
		((BYTE *)&lang)[1] = (language)[1];
		((BYTE *)&lang)[2] = (language)[2];
		LanguageIDs[0] = lang;
		LanguageIDs[1] = lang;
		LanguageIDs[2] = lang;
		LanguageIDs[3] = lang;
	}
}

void CalculateCPUSpeed()
{
	LARGE_INTEGER freq;

	QueryPerformanceFrequency (&freq);

	if (freq.QuadPart != 0 && CPU.bRDTSC)
	{
		LARGE_INTEGER count1, count2;
		cycle_t ClockCalibration;
		DWORD min_diff;

		ClockCalibration.Reset();

		// Count cycles for at least 55 milliseconds.
        // The performance counter may be very low resolution compared to CPU
        // speeds today, so the longer we count, the more accurate our estimate.
        // On the other hand, we don't want to count too long, because we don't
        // want the user to notice us spend time here, since most users will
        // probably never use the performance statistics.
        min_diff = freq.LowPart * 11 / 200;

		// Minimize the chance of task switching during the testing by going very
		// high priority. This is another reason to avoid timing for too long.
		SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS);
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

		// Make sure we start timing on a counter boundary.
		QueryPerformanceCounter(&count1);
		do { QueryPerformanceCounter(&count2); } while (count1.QuadPart == count2.QuadPart);

		// Do the timing loop.
		ClockCalibration.Clock();
		do { QueryPerformanceCounter(&count1); } while ((count1.QuadPart - count2.QuadPart) < min_diff);
		ClockCalibration.Unclock();

		SetPriorityClass(GetCurrentProcess(), NORMAL_PRIORITY_CLASS);
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);

		PerfToSec = double(count1.QuadPart - count2.QuadPart) / (double(ClockCalibration.GetRawCounter()) * freq.QuadPart);
		PerfToMillisec = PerfToSec * 1000.0;
	}

	Printf ("CPU Speed: %.0f MHz\n", 0.001 / PerfToMillisec);
}

//
// I_Init
//

void I_Init (void)
{
	CheckCPUID(&CPU);
	CalculateCPUSpeed();
	DumpCPUInfo(&CPU);

	// [BB] CalculateCPUSpeed() messes with the priority, so I think it's a good place
	// to set the server priority afterwards.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SetPriorityClass(GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS);

	// Use a timer event if possible
	NewTicArrived = CreateEvent (NULL, FALSE, FALSE, NULL);
	if (NewTicArrived)
	{
		UINT delay;
		char *cmdDelay;

		cmdDelay = Args->CheckValue ("-timerdelay");
		delay = 0;
		if (cmdDelay != 0)
		{
			delay = atoi (cmdDelay);
		}
		if (delay == 0)
		{
			delay = 1000/TICRATE;
		}
#ifndef __WINE__
		TimerEventID = timeSetEvent
			(
				delay,
				0,
				TimerTicked,
				0,
				TIME_PERIODIC
			);
#endif
		MillisecondsPerTic = delay;
	}
	if (TimerEventID != 0)
	{
		I_GetTime = I_GetTimeEventDriven;
		I_WaitForTic = I_WaitForTicEvent;
		I_FreezeTime = I_FreezeTimeEventDriven;
	}
	else
	{	// If no timer event, busy-loop with timeGetTime
		I_GetTime = I_GetTimePolled;
		I_WaitForTic = I_WaitForTicPolled;
		I_FreezeTime = I_FreezeTimePolled;
	}

	atterm (I_ShutdownSound);
	I_InitSound ();
}

//
// I_Quit
//
static int has_exited;

void I_Quit (void)
{
	has_exited = 1;		/* Prevent infinitely recursive exits -- killough */

	if (TimerEventID)
		timeKillEvent (TimerEventID);
	if (NewTicArrived)
		CloseHandle (NewTicArrived);

	timeEndPeriod (TimerPeriod);

	if (demorecording)
		G_CheckDemoStatus();
	// [BC] Support for client-side demos.
	if ( CLIENTDEMO_IsRecording( ))
		CLIENTDEMO_FinishRecording( );
}


//
// I_Error
//
extern FILE *Logfile;
bool gameisdead;

void STACK_ARGS I_FatalError (const char *error, ...)
{
	static BOOL alreadyThrown = false;
	gameisdead = true;

	if (!alreadyThrown)		// ignore all but the first message -- killough
	{
		alreadyThrown = true;
		char errortext[MAX_ERRORTEXT];
		va_list argptr;
		va_start (argptr, error);
		myvsnprintf (errortext, MAX_ERRORTEXT, error, argptr);
		va_end (argptr);

		// Record error to log (if logging)
		if (Logfile)
		{
			fprintf (Logfile, "\n**** DIED WITH FATAL ERROR:\n%s\n", errortext);
			fflush (Logfile);
		}

		// [BB] Tell the server we're leaving the game.
		if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
			CLIENT_QuitNetworkGame( NULL );

		throw CFatalError (errortext);
	}

	if (!has_exited)	// If it hasn't exited yet, exit now -- killough
	{
		has_exited = 1;	// Prevent infinitely recursive exits -- killough
		exit(-1);
	}
}

void STACK_ARGS I_Error (const char *error, ...)
{
	va_list argptr;
	char errortext[MAX_ERRORTEXT];

	va_start (argptr, error);
	myvsnprintf (errortext, MAX_ERRORTEXT, error, argptr);
	va_end (argptr);

	// [BB] Tell the server we're leaving the game.
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
		CLIENT_QuitNetworkGame( NULL );

	throw CRecoverableError (errortext);
}

extern void LayoutMainWindow (HWND hWnd, HWND pane);

void I_SetIWADInfo (const IWADInfo *info)
{
	DoomStartupInfo = info;

	// Make the startup banner show itself
	LayoutMainWindow (Window, NULL);
}

void I_PrintStr (const char *cp)
{
	if (ConWindow == NULL && StdOut == NULL)
		return;

	HWND edit = ConWindow;
	char buf[256];
	int bpos = 0;
	CHARRANGE selection;
	CHARRANGE endselection;
	LONG lines_before = 0, lines_after;
	CHARFORMAT format;

	if (edit != NULL)
	{
		// Store the current selection and set it to the end so we can append text.
		SendMessage (edit, EM_EXGETSEL, 0, (LPARAM)&selection);
		endselection.cpMax = endselection.cpMin = GetWindowTextLength (edit);
		SendMessage (edit, EM_EXSETSEL, 0, (LPARAM)&endselection);

		// GetWindowTextLength and EM_EXSETSEL can disagree on where the end of
		// the text is. Find out what EM_EXSETSEL thought it was and use that later.
		SendMessage (edit, EM_EXGETSEL, 0, (LPARAM)&endselection);

		// Remember how many lines there were before we added text.
		lines_before = SendMessage (edit, EM_GETLINECOUNT, 0, 0);
	}

	while (*cp != 0)
	{
		// 28 is the escape code for a color change.
		if ((*cp == 28 && bpos != 0) || bpos == 255)
		{
			buf[bpos] = 0;
			if (edit != NULL)
			{
				SendMessage (edit, EM_REPLACESEL, FALSE, (LPARAM)buf);
			}
			if (StdOut != NULL)
			{
				DWORD bytes_written;
				WriteFile(StdOut, buf, bpos, &bytes_written, NULL);
			}
			bpos = 0;
		}
		if (*cp != 28)
		{
			buf[bpos++] = *cp++;
		}
		else
		{
			const BYTE *color_id = (const BYTE *)cp + 1;
			EColorRange range = V_ParseFontColor (color_id, CR_UNTRANSLATED, CR_YELLOW);
			cp = (const char *)color_id;

			if (range != CR_UNDEFINED)
			{
				// Change the color of future text added to the control.
				PalEntry color = V_LogColorFromColorRange (range);
				if (StdOut != NULL && FancyStdOut)
				{
					// Unfortunately, we are pretty limited here: There are only
					// eight basic colors, and each comes in a dark and a bright
					// variety.
					float h, s, v, r, g, b;
					WORD attrib = 0;

					RGBtoHSV(color.r / 255.0, color.g / 255.0, color.b / 255.0, &h, &s, &v);
					if (s != 0)
					{ // color
						HSVtoRGB(&r, &g, &b, h, 1, 1);
						if (r == 1)  attrib  = FOREGROUND_RED;
						if (g == 1)  attrib |= FOREGROUND_GREEN;
						if (b == 1)  attrib |= FOREGROUND_BLUE;
						if (v > 0.6) attrib |= FOREGROUND_INTENSITY;
					}
					else
					{ // gray
						     if (v < 0.33) attrib = FOREGROUND_INTENSITY;
						else if (v < 0.90) attrib = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
						else			   attrib = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
					}
					SetConsoleTextAttribute(StdOut, attrib);
				}
				if (edit != NULL)
				{
					// GDI uses BGR colors, but color is RGB, so swap the R and the B.
					swap (color.r, color.b);
					// Change the color.
					format.cbSize = sizeof(format);
					format.dwMask = CFM_COLOR;
					format.dwEffects = 0;
					format.crTextColor = color;
					SendMessage (edit, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&format);
				}
			}
		}
	}
	if (bpos != 0)
	{
		buf[bpos] = 0;
		if (edit != NULL)
		{
			SendMessage (edit, EM_REPLACESEL, FALSE, (LPARAM)buf); 
		}
		if (StdOut != NULL)
		{
			DWORD bytes_written;
			WriteFile(StdOut, buf, bpos, &bytes_written, NULL);
		}
	}

	if (edit != NULL)
	{
		// If the old selection was at the end of the text, keep it at the end and
		// scroll. Don't scroll if the selection is anywhere else.
		if (selection.cpMin == endselection.cpMin && selection.cpMax == endselection.cpMax)
		{
			selection.cpMax = selection.cpMin = GetWindowTextLength (edit);
			lines_after = SendMessage (edit, EM_GETLINECOUNT, 0, 0);
			if (lines_after > lines_before)
			{
				SendMessage (edit, EM_LINESCROLL, 0, lines_after - lines_before);
			}
		}
		// Restore the previous selection.
		SendMessage (edit, EM_EXSETSEL, 0, (LPARAM)&selection);
		// Give the edit control a chance to redraw itself.
		I_GetEvent ();
	}
	if (StdOut != NULL && FancyStdOut)
	{ // Set text back to gray, in case it was changed.
		SetConsoleTextAttribute(StdOut, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
	}
}

EXTERN_CVAR (Bool, queryiwad);
CVAR (String, queryiwad_key, "shift", CVAR_GLOBALCONFIG|CVAR_ARCHIVE);
static WadStuff *WadList;
static int NumWads;
static int DefaultWad;

static void SetQueryIWad (HWND dialog)
{
	HWND checkbox = GetDlgItem (dialog, IDC_DONTASKIWAD);
	int state = SendMessage (checkbox, BM_GETCHECK, 0, 0);
	bool query = (state != BST_CHECKED);
	queryiwad = query;
}

BOOL CALLBACK IWADBoxCallback (HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	HWND ctrl;
	int i;

	switch (message)
	{
	case WM_INITDIALOG:
		char	szString[256];

		// Set up our version string.
		sprintf(szString, "Version %s.", DOTVERSIONSTR_REV);
		SetDlgItemText (hDlg, IDC_WELCOME_VERSION, szString);

		// Check the current video settings.
		// [BB] Take into account that gl_nogl overrides vid_renderer.
		SendDlgItemMessage( hDlg, ( ( gl_nogl == false ) && vid_renderer ) ? IDC_WELCOME_OPENGL : IDC_WELCOME_SOFTWARE, BM_SETCHECK, BST_CHECKED, 0 );
		SendDlgItemMessage( hDlg, IDC_WELCOME_FULLSCREEN, BM_SETCHECK, fullscreen ? BST_CHECKED : BST_UNCHECKED, 0 );
		SendDlgItemMessage( hDlg, IDC_WELCOME_COMPAT, BM_SETCHECK, gl_vid_compatibility ? BST_CHECKED : BST_UNCHECKED, 0 );

		// Set the state of the "Don't ask me again" checkbox.
		SendDlgItemMessage ( hDlg, IDC_DONTASKIWAD, BM_SETCHECK, queryiwad ? BST_UNCHECKED : BST_CHECKED, 0);
				
		// Populate the list with all the IWADs found.
		ctrl = GetDlgItem (hDlg, IDC_IWADLIST);
		for (i = 0; i < NumWads; i++)
		{
			FString work;
			const char *filepart = strrchr (WadList[i].Path, '/');
			if (filepart == NULL)
				filepart = WadList[i].Path;
			else
				filepart++;
			work.Format ("%s (%s)", IWADInfos[WadList[i].Type].Name, filepart);
			SendMessage (ctrl, LB_ADDSTRING, 0, (LPARAM)work.GetChars());
			SendMessage (ctrl, LB_SETITEMDATA, i, (LPARAM)i);
		}

		// Select the current IWAD in the list.
		SendMessage (ctrl, LB_SETCURSEL, DefaultWad, 0);
		SetFocus (ctrl);

		// Make sure the dialog is in front. If SHIFT was pressed to force it visible, then the main window will normally be on top.
		SetForegroundWindow (hDlg);
		break;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDCANCEL)
		{
			EndDialog (hDlg, -1);
		}
		else if(LOWORD(wParam) == IDC_DONTASKIWAD)
		{
			char	szString[256];
			char	szKey[6];

			// Determine the curent setting.
			I_GetWelcomeScreenKeyString( szKey );

			// If checked, show the label indicating which key to press.
			if(SendDlgItemMessage( hDlg, IDC_DONTASKIWAD, BM_GETCHECK, 0, 0 ) == BST_CHECKED)
				sprintf(szString, "Hold <%s> on startup to show this screen.", szKey);
			else
				strcpy(szString, "");	// Hide it.

			SetDlgItemText (hDlg, IDC_WELCOME_SHIFTLABEL, szString);
		}
		else if(LOWORD(wParam) == IDC_WELCOME_COMPAT)
		{
			// Tell the user about this setting.
			if(SendDlgItemMessage( hDlg, IDC_WELCOME_COMPAT, BM_GETCHECK, 0, 0 ) == BST_CHECKED)
				strcpy(szString, "This setting should only be used if you're having problems.");
			else
				strcpy(szString, "");	// Hide it.

			SetDlgItemText (hDlg, IDC_WELCOME_SHIFTLABEL, szString);
		}


		else if (LOWORD(wParam) == IDOK
			/* [RC] No longer the primary control. > || (LOWORD(wParam) == IDC_IWADLIST && HIWORD(wParam) == LBN_DBLCLK)<*/)
		{
			SetQueryIWad (hDlg);
			vid_renderer = SendDlgItemMessage( hDlg, IDC_WELCOME_OPENGL, BM_GETCHECK, 0, 0 ) == BST_CHECKED;
			// [BB] If the users wants OpenGL, we need to make sure that gl_nogl is false.
			if ( ( vid_renderer == 1 ) && ( gl_nogl == true ) )
				gl_nogl = false;
			gl_vid_compatibility = SendDlgItemMessage( hDlg, IDC_WELCOME_COMPAT, BM_GETCHECK, 0, 0 ) == BST_CHECKED;
			fullscreen = SendDlgItemMessage( hDlg, IDC_WELCOME_FULLSCREEN, BM_GETCHECK, 0, 0 ) == BST_CHECKED;
			ctrl = GetDlgItem (hDlg, IDC_IWADLIST);
			EndDialog (hDlg, SendMessage (ctrl, LB_GETCURSEL, 0, 0));
		}
		break;
	}
	return FALSE;
}

int I_GetWelcomeScreenKeyCode( void )
{
	int vkey = 0;

	if (stricmp (queryiwad_key, "shift") == 0)
		vkey = VK_SHIFT;
	else if (stricmp (queryiwad_key, "control") == 0 || stricmp (queryiwad_key, "ctrl") == 0)
		vkey = VK_CONTROL;

	return vkey;
}


void I_GetWelcomeScreenKeyString( char *pszString )
{
	switch(I_GetWelcomeScreenKeyCode())
	{
		case VK_CONTROL:
			strcpy(pszString, "CTRL");
			break;
		default:
		case VK_SHIFT:
			strcpy(pszString, "SHIFT");
	}
}

//==========================================================================
// "No IWAD" setup dialog
//==========================================================================

static	HWND		g_hDlg_NoIWAD;
static	HWND		g_hDlg_NoIWAD_Welcome;
static	HWND		g_hDlg_NoIWAD_NoDoom;
static	HWND		g_hDlg_NoIWAD_Redirect;
static	HBRUSH		g_hWhiteBrush;

//==========================================================================
//
// I_ShowDirectoryBrowser
//
// Shows the Windows "select a directory" dialog.
// Returns whether successful; if so, copies the path into pszBuffer.
//
//==========================================================================

bool I_ShowDirectoryBrowser( HWND hDlg, char *pszBuffer, const char *pszDisplayTitle )
{
	BROWSEINFO   bi;
	ZeroMemory( &bi, sizeof( bi )); 
	TCHAR   szDisplayName[MAX_PATH]; 
	szDisplayName[0] = 0;  

	bi.hwndOwner        =   hDlg; 
	bi.pidlRoot         =   NULL; 
	bi.pszDisplayName   =   szDisplayName; 
	bi.lpszTitle        =   pszDisplayTitle; 
	bi.ulFlags          =   BIF_RETURNONLYFSDIRS;
	bi.lParam           =   NULL; 
	bi.iImage           =   0;  

	LPITEMIDLIST pidl = SHBrowseForFolder( &bi );
	if ( pidl && SHGetPathFromIDList( pidl, pszBuffer ))
		return true;
	else
		return false;
}

//*****************************************************************************
//
BOOL CALLBACK NoIWADBox_Welcome_Callback (HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	// "I don't have Doom!" clicked. Advance to the next screen.
	if ( message == WM_COMMAND && LOWORD( wParam ) == IDC_NODOOM )
	{
		SetWindowPos( g_hDlg_NoIWAD_Welcome, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_HIDEWINDOW );			
		SetWindowPos( g_hDlg_NoIWAD_NoDoom, NULL, 0, 55, 450, 200, SWP_NOZORDER );
		return FALSE; 
	}
	// "Browse" button clicked.
	else if ( message == WM_COMMAND && LOWORD( wParam ) == IDC_BROWSE )
	{
		char szPathName[MAX_PATH]; 
		if ( !I_ShowDirectoryBrowser( hDlg, szPathName, "Please select the folder where Doom is installed.\nWe're looking for doom.wad or doom2.wad." ))
			return FALSE;

		// See if this directory contains any IWADs.
		if ( D_DoesDirectoryHaveIWADs( szPathName ))
		{
			// Add this directory to the user's profile, restart, and play!
			if ( GameConfig->SetSection ("IWADSearch.Directories"))
			{
				GameConfig->SetValueForKey ("Path", szPathName, true);
				GameConfig->WriteConfigFile();

				FString arguments = "";
				for ( int i = 1; i < Args->NumArgs(); i++ )
					arguments.AppendFormat( "%s ", Args->GetArg(i) );
				ShellExecute( hDlg, "open", Args->GetArg( 0 ), arguments.GetChars( ), NULL, SW_SHOW );

				exit( 0 );
			}
		}
		else
			MessageBox( hDlg, "No IWADs were found in that folder.\n\nPlease select the folder that contains your copy of doom.wad or doom2.wad.", "Couldn't find Doom", MB_ICONEXCLAMATION );
	}
	return FALSE;
}

//*****************************************************************************
//
BOOL CALLBACK NoIWADBox_NoDoom_Callback (HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	if ( message == WM_COMMAND && (( LOWORD( wParam ) == IDC_USESTEAM ) || ( LOWORD( wParam ) == IDC_USEFREEDOOM )))
	{
		if ( LOWORD( wParam ) == IDC_USESTEAM )
		{
			I_RunProgram( "http://" DOMAIN_NAME "/go/buydoom/" );
			SetDlgItemText( g_hDlg_NoIWAD_Redirect, IDC_REDIRECTING2, "After you've downloaded Doom 2, simply restart " GAMENAME "." );
		}
		else if ( LOWORD( wParam ) == IDC_USEFREEDOOM )
		{
			I_RunProgram( "http://" DOMAIN_NAME "/go/freedoom/" );
			SetDlgItemText( g_hDlg_NoIWAD_Redirect, IDC_REDIRECTING2, "After downloading, simply extract doom2.wad into the " GAMENAME " directory." );
			SetDlgItemText( g_hDlg_NoIWAD_Redirect, IDC_RESTART, "Done" );
		}

		// Advance to the redirect page.
		SendMessage( GetDlgItem( g_hDlg_NoIWAD_Redirect, IDC_REDIRECTING1 ), WM_SETFONT, (WPARAM) CreateFont( 13, 0, 0, 0, 600, 0, 0, 0, 0, 0, 0, 0, 0, "Tahoma" ), (LPARAM) 1 );
		SetWindowPos( g_hDlg_NoIWAD_NoDoom, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_HIDEWINDOW );			
		SetWindowPos( g_hDlg_NoIWAD_Redirect, NULL, 0, 55, 450, 200, SWP_NOZORDER );
	}
	return FALSE; 
}

//*****************************************************************************
//
BOOL CALLBACK NoIWADBox_Redirect_Callback (HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	// "Restart" button clicked.
	if ( message == WM_COMMAND && LOWORD( wParam ) == IDC_RESTART )
	{
		FString arguments = "";
		for ( int i = 1; i < Args->NumArgs(); i++ )
			arguments.AppendFormat( "%s ", Args->GetArg(i) );
		ShellExecute( hDlg, "open", Args->GetArg( 0 ), arguments.GetChars( ), NULL, SW_SHOW );
		exit( 0 );
	}
	return FALSE;
}

//*****************************************************************************
//
void main_PaintRectangle( HDC hDC, RECT *rect, COLORREF color )
{
	COLORREF oldcr = SetBkColor( hDC, color );
	ExtTextOut( hDC, 0, 0, ETO_OPAQUE, rect, "", 0, 0 );
	SetBkColor( hDC, oldcr );
}

//*****************************************************************************
//
BOOL CALLBACK NoIWADBoxCallback (HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_CLOSE:

		EndDialog( hDlg, -1 );
		break;
	case WM_INITDIALOG:
		{
			g_hDlg_NoIWAD = hDlg;

			// Format the top of the dialog.
			SendMessage( GetDlgItem( hDlg, IDC_STTITLE ), WM_SETFONT, (WPARAM) CreateFont( 19, 0, 0, 0, 600, 0, 0, 0, 0, 0, 0, 0, 0, "Tahoma" ), (LPARAM) 1 );
			LOGBRUSH LogBrush;
			LogBrush.lbStyle = BS_SOLID;
			LogBrush.lbColor = RGB( 255, 255, 255 );
			g_hWhiteBrush = CreateBrushIndirect( &LogBrush );

			// Create the child dialogs.
			if ( g_hDlg_NoIWAD_Welcome == NULL )
			{
				g_hDlg_NoIWAD_Welcome = CreateDialogParam( g_hInst, MAKEINTRESOURCE( IDD_NOIWADS_WELCOME ), hDlg, NoIWADBox_Welcome_Callback, NULL );
				g_hDlg_NoIWAD_NoDoom = CreateDialogParam( g_hInst, MAKEINTRESOURCE( IDD_NOIWADS_NODOOM ), hDlg, NoIWADBox_NoDoom_Callback, NULL );
				g_hDlg_NoIWAD_Redirect = CreateDialogParam( g_hInst, MAKEINTRESOURCE( IDD_NOIWADS_REDIRECT ), hDlg, NoIWADBox_Redirect_Callback, NULL );
				SetParent( g_hDlg_NoIWAD_Welcome, hDlg );
				SetParent( g_hDlg_NoIWAD_NoDoom, hDlg );
				SetParent( g_hDlg_NoIWAD_Redirect, hDlg );
				SetWindowPos( g_hDlg_NoIWAD_Welcome, NULL, 0, 55, 450, 250, SWP_NOZORDER );
			}
		}
		break;
	case WM_CTLCOLORSTATIC:

		if ( GetDlgCtrlID( (HWND)lParam ) == IDC_STTITLE || GetDlgCtrlID( (HWND)lParam ) == IDI_ICONST ) // Paint the title label's background white.
			return (LRESULT) g_hWhiteBrush;			
		else
			return NULL;
	case WM_PAINT:
		{
			// Paint the top of the form white.
			PAINTSTRUCT Ps;
			RECT r;
			r.left = 0;
			r.top = 0;
			r.bottom = 55;
			r.right = 500;
			main_PaintRectangle( BeginPaint(hDlg, &Ps), &r, RGB(255, 255, 255));
		}
	}
	return FALSE;
}

//*****************************************************************************
//
void I_ShowNoIWADsScreen( void )
{
	DialogBox( g_hInst, MAKEINTRESOURCE(IDD_NOIWADS), NULL, (DLGPROC)NoIWADBoxCallback );
}

int I_PickIWad (WadStuff *wads, int numwads, bool showwin, int defaultiwad)
{
	int vkey = I_GetWelcomeScreenKeyCode();
	if (showwin || (vkey != 0 && GetAsyncKeyState(vkey)))
	{
		WadList = wads;
		NumWads = numwads;
		DefaultWad = defaultiwad;

		return DialogBox (g_hInst, MAKEINTRESOURCE(IDD_IWADDIALOG),
			(HWND)Window, (DLGPROC)IWADBoxCallback);
	}
	return defaultiwad;
}

bool I_WriteIniFailed ()
{
	char *lpMsgBuf;
	FString errortext;

	FormatMessageA (FORMAT_MESSAGE_ALLOCATE_BUFFER | 
					FORMAT_MESSAGE_FROM_SYSTEM | 
					FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		GetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
		(LPSTR)&lpMsgBuf,
		0,
		NULL 
	);
	errortext.Format ("The config file %s could not be written:\n%s", GameConfig->GetPathName(), lpMsgBuf);
	LocalFree (lpMsgBuf);
	return MessageBox (Window, errortext.GetChars(), GAMENAME " configuration not saved", MB_ICONEXCLAMATION | MB_RETRYCANCEL) == IDRETRY;
}

void *I_FindFirst (const char *filespec, findstate_t *fileinfo)
{
	return FindFirstFileA (filespec, (LPWIN32_FIND_DATAA)fileinfo);
}
int I_FindNext (void *handle, findstate_t *fileinfo)
{
	return !FindNextFileA ((HANDLE)handle, (LPWIN32_FIND_DATAA)fileinfo);
}

int I_FindClose (void *handle)
{
	return FindClose ((HANDLE)handle);
}

static bool QueryPathKey(HKEY key, const char *keypath, const char *valname, FString &value)
{
	HKEY steamkey;
	DWORD pathtype;
	DWORD pathlen;
	LONG res;

	if(ERROR_SUCCESS == RegOpenKeyEx(key, keypath, 0, KEY_QUERY_VALUE, &steamkey))
	{
		if (ERROR_SUCCESS == RegQueryValueEx(steamkey, valname, 0, &pathtype, NULL, &pathlen) &&
			pathtype == REG_SZ && pathlen != 0)
		{
			// Don't include terminating null in count
			char *chars = value.LockNewBuffer(pathlen - 1);
			res = RegQueryValueEx(steamkey, valname, 0, NULL, (LPBYTE)chars, &pathlen);
			value.UnlockBuffer();
			if (res != ERROR_SUCCESS)
			{
				value = "";
			}
		}
		RegCloseKey(steamkey);
	}
	return value.IsNotEmpty();
}

FString I_GetSteamPath()
{
	FString path;

	if (QueryPathKey(HKEY_CURRENT_USER, "Software\\Valve\\Steam", "SteamPath", path))
	{
		return path;
	}
	if (QueryPathKey(HKEY_LOCAL_MACHINE, "Software\\Valve\\Steam", "InstallPath", path))
	{
		return path;
	}
	path = "";
	return path;
}

// Return a random seed, preferably one with lots of entropy.
unsigned int I_MakeRNGSeed()
{
	unsigned int seed;

	// If RtlGenRandom is available, use that to avoid increasing the
	// working set by pulling in all of the crytographic API.
	HMODULE advapi = GetModuleHandle("advapi32.dll");
	if (advapi != NULL)
	{
		BOOLEAN (APIENTRY *RtlGenRandom)(void *, ULONG) =
			(BOOLEAN (APIENTRY *)(void *, ULONG))GetProcAddress(advapi, "SystemFunction036");
		if (RtlGenRandom != NULL)
		{
			if (RtlGenRandom(&seed, sizeof(seed)))
			{
				return seed;
			}
		}
	}

	// Use the full crytographic API to produce a seed. If that fails,
	// time() is used as a fallback.
	HCRYPTPROV prov;

	if (!CryptAcquireContext(&prov, NULL, MS_DEF_PROV, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
	{
		return (unsigned int)time(NULL);
	}
	if (!CryptGenRandom(prov, sizeof(seed), (BYTE *)&seed))
	{
		seed = (unsigned int)time(NULL);
	}
	CryptReleaseContext(prov, 0);
	return seed;
}

// [RC] Lunches the path given. This was encapsulated to make wrangling with #includes easier.
void I_RunProgram( const char *szPath )
{
	ShellExecute( NULL, "open", szPath, NULL, NULL, SW_SHOW );
}

