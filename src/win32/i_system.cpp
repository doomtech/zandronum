/*
** i_system.cpp
** Timers, pre-console output, IWAD selection, and misc system routines.
**
**---------------------------------------------------------------------------
** Copyright 1998-2009 Randy Heit
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

// HEADER FILES ------------------------------------------------------------

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
#include "stats.h"
// [BC] New #includes.
#include "announcer.h"
#include "network.h"
#include "cl_demo.h"
#include "cl_main.h"
#include "campaign.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

extern void CheckCPUID(CPUInfo *cpu);
extern void LayoutMainWindow(HWND hWnd, HWND pane);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void CalculateCPUSpeed();
static void I_SelectTimer();

static int I_GetTimePolled(bool saveMS);
static int I_WaitForTicPolled(int prevtic);
static void I_FreezeTimePolled(bool frozen);
static int I_GetTimeEventDriven(bool saveMS);
static int I_WaitForTicEvent(int prevtic);
static void I_FreezeTimeEventDriven(bool frozen);
static void CALLBACK TimerTicked(UINT id, UINT msg, DWORD_PTR user, DWORD_PTR dw1, DWORD_PTR dw2);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

EXTERN_CVAR(String, language);
EXTERN_CVAR (Bool, queryiwad);

// Used on welcome/IWAD screen.
EXTERN_CVAR (Int, vid_renderer)
EXTERN_CVAR (Bool, fullscreen)
EXTERN_CVAR (Bool, gl_vid_compatibility)


extern HWND Window, ConWindow, GameTitleWindow;
extern HANDLE StdOut;
extern bool FancyStdOut;
extern HINSTANCE g_hInst;
extern FILE *Logfile;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

CVAR (String, queryiwad_key, "shift", CVAR_GLOBALCONFIG|CVAR_ARCHIVE);

double PerfToSec, PerfToMillisec;
UINT TimerPeriod;
UINT TimerEventID;
UINT MillisecondsPerTic;
HANDLE NewTicArrived;
uint32 LanguageIDs[4];

const IWADInfo *DoomStartupInfo;

// [K6/BB]
extern FString g_VersionWithOS;

int (*I_GetTime) (bool saveMS);
int (*I_WaitForTic) (int);
void (*I_FreezeTime) (bool frozen);

os_t OSPlatform;
bool gameisdead;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static ticcmd_t emptycmd;
static bool HasExited;

static DWORD basetime = 0;
// These are for the polled timer.
static DWORD TicStart;
static DWORD TicNext;
static int TicFrozen;

// These are for the event-driven timer.
static int tics;
static DWORD ted_start, ted_next;

static WadStuff *WadList;
static int NumWads;
static int DefaultWad;

// CODE --------------------------------------------------------------------

//==========================================================================
//
// I_Tactile
//
// Doom calls it when you take damage, so presumably it could be converted
// to something compatible with force feedback.
//
//==========================================================================

void I_Tactile(int on, int off, int total)
{
  // UNUSED.
  on = off = total = 0;
}

//==========================================================================
//
// I_BaseTiccmd
//
// Returns an empty ticcmd. I have no idea why this should be system-
// specific.
//
//==========================================================================

ticcmd_t *I_BaseTiccmd()
{
	return &emptycmd;
}

// Stubs that select the timer to use and then call into it ----------------

//==========================================================================
//
// I_GetTimeSelect
//
//==========================================================================

static int I_GetTimeSelect(bool saveMS)
{
	I_SelectTimer();
	return I_GetTime(saveMS);
}

//==========================================================================
//
// I_WaitForTicSelect
//
//==========================================================================

static int I_WaitForTicSelect(int prevtic)
{
	I_SelectTimer();
	return I_WaitForTic(prevtic);
}

//==========================================================================
//
// I_SelectTimer
//
// Tries to create a timer event for efficent CPU use when the FPS is
// capped. Failing that, it sets things up for a polling timer instead.
//
//==========================================================================

static void I_SelectTimer()
{
	assert(basetime == 0);

	// Use a timer event if possible.
	NewTicArrived = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (NewTicArrived)
	{
		UINT delay;
		const char *cmdDelay;

		cmdDelay = Args->CheckValue("-timerdelay");
		delay = 0;
		if (cmdDelay != 0)
		{
			delay = atoi(cmdDelay);
		}
		if (delay == 0)
		{
			delay = 1000/TICRATE;
		}
		MillisecondsPerTic = delay;
		TimerEventID = timeSetEvent(delay, 0, TimerTicked, 0, TIME_PERIODIC);
	}
	// Get the current time as the basetime.
	basetime = timeGetTime();
	// Set timer functions.
	if (TimerEventID != 0)
	{
		I_GetTime = I_GetTimeEventDriven;
		I_WaitForTic = I_WaitForTicEvent;
		I_FreezeTime = I_FreezeTimeEventDriven;
	}
	else
	{
		I_GetTime = I_GetTimePolled;
		I_WaitForTic = I_WaitForTicPolled;
		I_FreezeTime = I_FreezeTimePolled;
	}
}

//==========================================================================
//
// I_MSTime
//
// Returns the current time in milliseconds, where 0 is the first call
// to I_GetTime or I_WaitForTic.
//
//==========================================================================

unsigned int I_MSTime()
{
	assert(basetime != 0);
	return timeGetTime() - basetime;
}

//==========================================================================
//
// I_FPSTime
//
// Returns the current system time in milliseconds. This is used by the FPS
// meter of DFrameBuffer::DrawRateStuff(). Since the screen can display
// before the play simulation is ready to begin, this needs to be
// separate from I_MSTime().
//
//==========================================================================

unsigned int I_FPSTime()
{
	return timeGetTime();
}

//==========================================================================
//
// I_GetTimePolled
//
// Returns the current time in tics. If saveMS is true, then calls to
// I_GetTimeFrac() will use this tic as 0 and the next tic as 1.
//
//==========================================================================

static int I_GetTimePolled(bool saveMS)
{
	DWORD tm;

	if (TicFrozen != 0)
	{
		return TicFrozen;
	}

	tm = timeGetTime();
	if (basetime == 0)
	{
		basetime = tm;
	}
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

//==========================================================================
//
// I_WaitForTicPolled
//
// Busy waits until the current tic is greater than prevtic. Time must not
// be frozen.
//
//==========================================================================

static int I_WaitForTicPolled(int prevtic)
{
	int time;

	assert(TicFrozen == 0);
	while ((time = I_GetTimePolled(false)) <= prevtic)
	{ }

	return time;
}

//==========================================================================
//
// I_FreezeTimePolled
//
// Freeze/unfreeze the timer.
//
//==========================================================================

static void I_FreezeTimePolled(bool frozen)
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

//==========================================================================
//
// I_GetTimeEventDriven
//
// Returns the current tick counter. This is incremented asynchronously as
// the timer event fires.
//
//==========================================================================

static int I_GetTimeEventDriven(bool saveMS)
{
	if (saveMS)
	{
		TicStart = ted_start;
		TicNext = ted_next;
	}
	return tics;
}

//==========================================================================
//
// I_WaitForTicEvent
//
// Waits on the timer event as long as the current tic is not later than
// prevtic.
//
//==========================================================================

static int I_WaitForTicEvent(int prevtic)
{
	assert(!TicFrozen);
	while (prevtic >= tics)
	{
		WaitForSingleObject(NewTicArrived, 1000/TICRATE);
	}
	return tics;
}

//==========================================================================
//
// I_FreezeTimeEventDriven
//
// Freeze/unfreeze the ticker.
//
//==========================================================================

static void I_FreezeTimeEventDriven(bool frozen)
{
	TicFrozen = frozen;
}

//==========================================================================
//
// TimerTicked
//
// Advance the tick count and signal the NewTicArrived event.
//
//==========================================================================

static void CALLBACK TimerTicked(UINT id, UINT msg, DWORD_PTR user, DWORD_PTR dw1, DWORD_PTR dw2)
{
	if (!TicFrozen)
	{
		tics++;
	}
	ted_start = timeGetTime ();
	ted_next = ted_start + MillisecondsPerTic;
	SetEvent(NewTicArrived);
}

//==========================================================================
//
// I_GetTimeFrac
//
// Returns the fractional amount of a tic passed since the most recently
// saved tic.
//
//==========================================================================

fixed_t I_GetTimeFrac(uint32 *ms)
{
	DWORD now = timeGetTime();
	if (ms != NULL)
	{
		*ms = TicNext;
	}
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

//==========================================================================
//
// I_WaitVBL
//
// I_WaitVBL is never used to actually synchronize to the vertical blank.
// Instead, it's used for delay purposes. Doom used a 70 Hz display mode,
// so that's what we use to determine how long to wait for.
//
//==========================================================================

void I_WaitVBL(int count)
{
	Sleep(1000 * count / 70);
}

//==========================================================================
//
// I_DetectOS
//
// Determine which version of Windows the game is running on.
//
//==========================================================================

void I_DetectOS(void)
{
	OSVERSIONINFOEX info;
	const char *osname;

	info.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
	if (!GetVersionEx((OSVERSIONINFO *)&info))
	{
		// Retry with the older OSVERSIONINFO structure.
		info.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
		GetVersionEx((OSVERSIONINFO *)&info);
	}

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
		else if (info.dwMajorVersion == 6)
		{
			if (info.dwMinorVersion == 0)
			{
				if (info.wProductType == VER_NT_WORKSTATION)
				{
					osname = "Vista";
				}
				else
				{
					osname = "Server 2008";
				}
			}
			else if (info.dwMinorVersion == 1)
			{
				if (info.wProductType == VER_NT_WORKSTATION)
				{
					osname = "7";
				}
				else
				{
					osname = "Server 2008 R2";
				}
			}
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
		// [K6/BB]
		g_VersionWithOS.Format ( "%s on Windows %s (%lu.%lu.%lu)", GetVersionStringRev(), osname,
				info.dwMajorVersion, info.dwMinorVersion,
				info.dwBuildNumber & 0xffff);
	}
	else
	{
		Printf ("OS: Windows %s (NT %lu.%lu) Build %lu\n    %s\n",
				osname,
				info.dwMajorVersion, info.dwMinorVersion,
				info.dwBuildNumber, info.szCSDVersion);
		// [K6/BB]
		g_VersionWithOS.Format ( "%s on Windows %s (%lu.%lu.%lu)", GetVersionStringRev(), osname,
				info.dwMajorVersion, info.dwMinorVersion,
				info.dwBuildNumber);
	}

	if (OSPlatform == os_unknown)
	{
		Printf ("(Assuming Windows 95)\n");
		OSPlatform = os_Win95;
	}
}

//==========================================================================
//
// SubsetLanguageIDs
//
// Helper function for SetLanguageIDs.
//
//==========================================================================

static void SubsetLanguageIDs(LCID id, LCTYPE type, int idx)
{
	char buf[8];
	LCID langid;
	char *idp;

	if (!GetLocaleInfo(id, type, buf, 8))
		return;
	langid = MAKELCID(strtoul(buf, NULL, 16), SORT_DEFAULT);
	if (!GetLocaleInfo(langid, LOCALE_SABBREVLANGNAME, buf, 8))
		return;
	idp = (char *)(&LanguageIDs[idx]);
	memset (idp, 0, 4);
	idp[0] = tolower(buf[0]);
	idp[1] = tolower(buf[1]);
	idp[2] = tolower(buf[2]);
	idp[3] = 0;
}

//==========================================================================
//
// SetLanguageIDs
//
//==========================================================================

void SetLanguageIDs()
{
	size_t langlen = strlen(language);

	if (langlen < 2 || langlen > 3)
	{
		memset(LanguageIDs, 0, sizeof(LanguageIDs));
		SubsetLanguageIDs(LOCALE_USER_DEFAULT, LOCALE_ILANGUAGE, 0);
		SubsetLanguageIDs(LOCALE_USER_DEFAULT, LOCALE_IDEFAULTLANGUAGE, 1);
		SubsetLanguageIDs(LOCALE_SYSTEM_DEFAULT, LOCALE_ILANGUAGE, 2);
		SubsetLanguageIDs(LOCALE_SYSTEM_DEFAULT, LOCALE_IDEFAULTLANGUAGE, 3);
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

//==========================================================================
//
// CalculateCPUSpeed
//
// Make a decent guess at how much time elapses between TSC steps. This can
// vary over runtime depending on power management settings, so should not
// be used anywhere that truely accurate timing actually matters.
//
//==========================================================================

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

//==========================================================================
//
// I_Init
//
//==========================================================================

void I_Init()
{
	CheckCPUID(&CPU);
	CalculateCPUSpeed();
	DumpCPUInfo(&CPU);

	// [BB] CalculateCPUSpeed() messes with the priority, so I think it's a good place
	// to set the server priority afterwards.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		SetPriorityClass(GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS);

		// [BB] The server doesn't call I_SelectTimer, so we have to set basetime manually.
		if (!basetime)
			basetime = timeGetTime();
	}

	I_GetTime = I_GetTimeSelect;
	I_WaitForTic = I_WaitForTicSelect;

	atterm (I_ShutdownSound);
	I_InitSound ();
}

//==========================================================================
//
// I_Quit
//
//==========================================================================

void I_Quit()
{
	HasExited = true;		/* Prevent infinitely recursive exits -- killough */

	if (TimerEventID != 0)
	{
		timeKillEvent(TimerEventID);
	}
	if (NewTicArrived != NULL)
	{
		CloseHandle(NewTicArrived);
	}
	timeEndPeriod(TimerPeriod);
	if (demorecording)
	{
		G_CheckDemoStatus();
	}

	// [BC] Support for client-side demos.
	if ( CLIENTDEMO_IsRecording( ))
		CLIENTDEMO_FinishRecording( );
}


//==========================================================================
//
// I_FatalError
//
// Throw an error that will end the game.
//
//==========================================================================

void STACK_ARGS I_FatalError(const char *error, ...)
{
	static BOOL alreadyThrown = false;
	gameisdead = true;

	if (!alreadyThrown)		// ignore all but the first message -- killough
	{
		alreadyThrown = true;
		char errortext[MAX_ERRORTEXT];
		va_list argptr;
		va_start(argptr, error);
		myvsnprintf(errortext, MAX_ERRORTEXT, error, argptr);
		va_end(argptr);

		// Record error to log (if logging)
		if (Logfile)
		{
			fprintf(Logfile, "\n**** DIED WITH FATAL ERROR:\n%s\n", errortext);
			fflush(Logfile);
		}

		// [BB] Tell the server we're leaving the game.
		if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
			CLIENT_QuitNetworkGame( NULL );

		throw CFatalError(errortext);
	}

	if (!HasExited)		// If it hasn't exited yet, exit now -- killough
	{
		HasExited = 1;	// Prevent infinitely recursive exits -- killough
		exit(-1);
	}
}

//==========================================================================
//
// I_Error
//
// Throw an error that will send us to the console if we are far enough
// along in the startup process.
//
//==========================================================================

void STACK_ARGS I_Error(const char *error, ...)
{
	va_list argptr;
	char errortext[MAX_ERRORTEXT];

	va_start(argptr, error);
	myvsnprintf(errortext, MAX_ERRORTEXT, error, argptr);
	va_end(argptr);

	// [BB] Tell the server we're leaving the game.
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
		CLIENT_QuitNetworkGame( NULL );

	throw CRecoverableError(errortext);
}

//==========================================================================
//
// I_SetIWADInfo
//
//==========================================================================

void I_SetIWADInfo(const IWADInfo *info)
{
	DoomStartupInfo = info;

	// Make the startup banner show itself
	LayoutMainWindow(Window, NULL);
}

//==========================================================================
//
// ToEditControl
//
// Converts string to Unicode and inserts it into the control.
//
//==========================================================================

void ToEditControl(HWND edit, const char *buf, wchar_t *wbuf, int bpos)
{
	// Let's just do this ourself. It's not hard, and we can compensate for
	// special console characters at the same time.
#if 0
	MultiByteToWideChar(1252 /* Western */, 0, buf, bpos, wbuf, countof(wbuf));
	wbuf[bpos] = 0;
#else
	static wchar_t notlatin1[32] =		// code points 0x80-0x9F
	{
		0x20AC,		// Euro sign
		0x0081,		// Undefined
		0x201A,		// Single low-9 quotation mark
		0x0192,		// Latin small letter f with hook
		0x201E,		// Double low-9 quotation mark
		0x2026,		// Horizontal ellipsis
		0x2020,		// Dagger
		0x2021,		// Double dagger
		0x02C6,		// Modifier letter circumflex accent
		0x2030,		// Per mille sign
		0x0160,		// Latin capital letter S with caron
		0x2039,		// Single left-pointing angle quotation mark
		0x0152,		// Latin capital ligature OE
		0x008D,		// Undefined
		0x017D,		// Latin capital letter Z with caron
		0x008F,		// Undefined
		0x0090,		// Undefined
		0x2018,		// Left single quotation mark
		0x2019,		// Right single quotation mark
		0x201C,		// Left double quotation mark
		0x201D,		// Right double quotation mark
		0x2022,		// Bullet
		0x2013,		// En dash
		0x2014,		// Em dash
		0x02DC,		// Small tilde
		0x2122,		// Trade mark sign
		0x0161,		// Latin small letter s with caron
		0x203A,		// Single right-pointing angle quotation mark
		0x0153,		// Latin small ligature oe
		0x009D,		// Undefined
		0x017E,		// Latin small letter z with caron
		0x0178		// Latin capital letter Y with diaeresis
	};
	for (int i = 0; i <= bpos; ++i)
	{
		wchar_t code = (BYTE)buf[i];
		if (code >= 0x1D && code <= 0x1F)
		{ // The bar characters, most commonly used to indicate map changes
			code = 0x2550;	// Box Drawings Double Horizontal
		}
		else if (code >= 0x80 && code <= 0x9F)
		{
			code = notlatin1[code - 0x80];
		}
		wbuf[i] = code;
	}
#endif
	SendMessageW(edit, EM_REPLACESEL, FALSE, (LPARAM)wbuf); 
}

//==========================================================================
//
// I_PrintStr
//
// Send output to the list box shown during startup (and hidden during
// gameplay).
//
//==========================================================================

void I_PrintStr(const char *cp)
{
	if (ConWindow == NULL && StdOut == NULL)
		return;

	HWND edit = ConWindow;
	char buf[256];
	wchar_t wbuf[countof(buf)];
	int bpos = 0;
	CHARRANGE selection;
	CHARRANGE endselection;
	LONG lines_before = 0, lines_after;
	CHARFORMAT format;

	if (edit != NULL)
	{
		// Store the current selection and set it to the end so we can append text.
		SendMessage(edit, EM_EXGETSEL, 0, (LPARAM)&selection);
		endselection.cpMax = endselection.cpMin = GetWindowTextLength(edit);
		SendMessage(edit, EM_EXSETSEL, 0, (LPARAM)&endselection);

		// GetWindowTextLength and EM_EXSETSEL can disagree on where the end of
		// the text is. Find out what EM_EXSETSEL thought it was and use that later.
		SendMessage(edit, EM_EXGETSEL, 0, (LPARAM)&endselection);

		// Remember how many lines there were before we added text.
		lines_before = (LONG)SendMessage(edit, EM_GETLINECOUNT, 0, 0);
	}

	while (*cp != 0)
	{
		// 28 is the escape code for a color change.
		if ((*cp == 28 && bpos != 0) || bpos == 255)
		{
			buf[bpos] = 0;
			if (edit != NULL)
			{
				ToEditControl(edit, buf, wbuf, bpos);
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
			EColorRange range = V_ParseFontColor(color_id, CR_UNTRANSLATED, CR_YELLOW);
			cp = (const char *)color_id;

			if (range != CR_UNDEFINED)
			{
				// Change the color of future text added to the control.
				PalEntry color = V_LogColorFromColorRange(range);
				if (StdOut != NULL && FancyStdOut)
				{
					// Unfortunately, we are pretty limited here: There are only
					// eight basic colors, and each comes in a dark and a bright
					// variety.
					float h, s, v, r, g, b;
					WORD attrib = 0;

					RGBtoHSV(color.r / 255.f, color.g / 255.f, color.b / 255.f, &h, &s, &v);
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
					swapvalues(color.r, color.b);
					// Change the color.
					format.cbSize = sizeof(format);
					format.dwMask = CFM_COLOR;
					format.dwEffects = 0;
					format.crTextColor = color;
					SendMessage(edit, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&format);
				}
			}
		}
	}
	if (bpos != 0)
	{
		buf[bpos] = 0;
		if (edit != NULL)
		{
			ToEditControl(edit, buf, wbuf, bpos);
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
			lines_after = (LONG)SendMessage(edit, EM_GETLINECOUNT, 0, 0);
			if (lines_after > lines_before)
			{
				SendMessage(edit, EM_LINESCROLL, 0, lines_after - lines_before);
			}
		}
		// Restore the previous selection.
		SendMessage(edit, EM_EXSETSEL, 0, (LPARAM)&selection);
		// Give the edit control a chance to redraw itself.
		I_GetEvent();
	}
	if (StdOut != NULL && FancyStdOut)
	{ // Set text back to gray, in case it was changed.
		SetConsoleTextAttribute(StdOut, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
	}
}

//==========================================================================
//
// SetQueryIWAD
//
// The user had the "Don't ask again" box checked when they closed the
// IWAD selection dialog.
//
//==========================================================================

static void SetQueryIWad(HWND dialog)
{
	HWND checkbox = GetDlgItem(dialog, IDC_DONTASKIWAD);
	int state = (int)SendMessage(checkbox, BM_GETCHECK, 0, 0);
	bool query = (state != BST_CHECKED);
	queryiwad = query;
}

//==========================================================================
//
// IWADBoxCallback
//
// Dialog proc for the IWAD selector.
//
//==========================================================================

BOOL CALLBACK IWADBoxCallback(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	HWND ctrl;
	int i;

	switch (message)
	{
	case WM_INITDIALOG:
		char	szString[256];

		// Set up our version string.
		sprintf(szString, "Version %s.", GetVersionStringRev());
		SetDlgItemText (hDlg, IDC_WELCOME_VERSION, szString);

		// Check the current video settings.
		SendDlgItemMessage( hDlg, vid_renderer ? IDC_WELCOME_OPENGL : IDC_WELCOME_SOFTWARE, BM_SETCHECK, BST_CHECKED, 0 );
		SendDlgItemMessage( hDlg, IDC_WELCOME_FULLSCREEN, BM_SETCHECK, fullscreen ? BST_CHECKED : BST_UNCHECKED, 0 );
		SendDlgItemMessage( hDlg, IDC_WELCOME_COMPAT, BM_SETCHECK, gl_vid_compatibility ? BST_CHECKED : BST_UNCHECKED, 0 );

		// Set the state of the "Don't ask me again" checkbox.
		SendDlgItemMessage ( hDlg, IDC_DONTASKIWAD, BM_SETCHECK, queryiwad ? BST_UNCHECKED : BST_CHECKED, 0);
				
		// Populate the list with all the IWADs found.
		ctrl = GetDlgItem(hDlg, IDC_IWADLIST);
		for (i = 0; i < NumWads; i++)
		{
			FString work;
			const char *filepart = strrchr(WadList[i].Path, '/');
			if (filepart == NULL)
				filepart = WadList[i].Path;
			else
				filepart++;
			work.Format("%s (%s)", IWADInfos[WadList[i].Type].Name, filepart);
			SendMessage(ctrl, LB_ADDSTRING, 0, (LPARAM)work.GetChars());
			SendMessage(ctrl, LB_SETITEMDATA, i, (LPARAM)i);
		}

		// Select the current IWAD in the list.
		SendMessage(ctrl, LB_SETCURSEL, DefaultWad, 0);
		SetFocus(ctrl);

		// Make sure the dialog is in front. If SHIFT was pressed to force it visible, then the main window will normally be on top.
		SetForegroundWindow(hDlg);
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
			SetQueryIWad(hDlg);
			vid_renderer = SendDlgItemMessage( hDlg, IDC_WELCOME_OPENGL, BM_GETCHECK, 0, 0 ) == BST_CHECKED;
			gl_vid_compatibility = SendDlgItemMessage( hDlg, IDC_WELCOME_COMPAT, BM_GETCHECK, 0, 0 ) == BST_CHECKED;
			fullscreen = SendDlgItemMessage( hDlg, IDC_WELCOME_FULLSCREEN, BM_GETCHECK, 0, 0 ) == BST_CHECKED;
			ctrl = GetDlgItem (hDlg, IDC_IWADLIST);
			EndDialog(hDlg, SendMessage (ctrl, LB_GETCURSEL, 0, 0));
		}
		break;
	}
	return FALSE;
}

//==========================================================================
//
// I_PickIWad
//
// Open a dialog to pick the IWAD, if there is more than one found.
//
//==========================================================================

int I_GetWelcomeScreenKeyCode( void )
{
	int vkey = 0;

	if (stricmp(queryiwad_key, "shift") == 0)
		vkey = VK_SHIFT;
	else if (stricmp(queryiwad_key, "control") == 0 || stricmp (queryiwad_key, "ctrl") == 0)
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
				g_hDlg_NoIWAD_Welcome = CreateDialogParam( g_hInst, MAKEINTRESOURCE( IDD_NOIWADS_WELCOME ), hDlg, (DLGPROC)NoIWADBox_Welcome_Callback, NULL );
				g_hDlg_NoIWAD_NoDoom = CreateDialogParam( g_hInst, MAKEINTRESOURCE( IDD_NOIWADS_NODOOM ), hDlg, (DLGPROC)NoIWADBox_NoDoom_Callback, NULL );
				g_hDlg_NoIWAD_Redirect = CreateDialogParam( g_hInst, MAKEINTRESOURCE( IDD_NOIWADS_REDIRECT ), hDlg, (DLGPROC)NoIWADBox_Redirect_Callback, NULL );
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

		return (int)DialogBox(g_hInst, MAKEINTRESOURCE(IDD_IWADDIALOG),
			(HWND)Window, (DLGPROC)IWADBoxCallback);
	}
	return defaultiwad;
}

//==========================================================================
//
// I_WriteIniFailed
//
// Display a message when the config failed to save.
//
//==========================================================================

bool I_WriteIniFailed()
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
	return MessageBox(Window, errortext.GetChars(), GAMENAME " configuration not saved", MB_ICONEXCLAMATION | MB_RETRYCANCEL) == IDRETRY;
}

//==========================================================================
//
// I_FindFirst
//
// Start a pattern matching sequence.
//
//==========================================================================

void *I_FindFirst(const char *filespec, findstate_t *fileinfo)
{
	return FindFirstFileA(filespec, (LPWIN32_FIND_DATAA)fileinfo);
}

//==========================================================================
//
// I_FindNext
//
// Return the next file in a pattern matching sequence.
//
//==========================================================================

int I_FindNext(void *handle, findstate_t *fileinfo)
{
	return !FindNextFileA((HANDLE)handle, (LPWIN32_FIND_DATAA)fileinfo);
}

//==========================================================================
//
// I_FindClose
//
// Finish a pattern matching sequence.
//
//==========================================================================

int I_FindClose(void *handle)
{
	return FindClose((HANDLE)handle);
}

//==========================================================================
//
// QueryPathKey
//
// Returns the value of a registry key into the output variable value.
//
//==========================================================================

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

//==========================================================================
//
// I_GetSteamPath
//
// Check the registry for the path to Steam, so that we can search for
// IWADs that were bought with Steam.
//
//==========================================================================

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

//==========================================================================
//
// I_MakeRNGSeed
//
// Returns a 32-bit random seed, preferably one with lots of entropy.
//
//==========================================================================

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

