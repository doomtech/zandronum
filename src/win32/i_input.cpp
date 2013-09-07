/*
** i_input.cpp
** Handles input from keyboard, mouse, and joystick
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
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

// DI3 only supports up to 4 mouse buttons, and I want the joystick to
// be read using DirectInput instead of winmm.

#define DIRECTINPUT_VERSION 0x800
#if !defined(_WIN32_WINNT) || (_WIN32_WINNT < 0x0501)
#define _WIN32_WINNT 0x0501			// Support the mouse wheel and session notification.
#endif

#define WIN32_LEAN_AND_MEAN
#define __BYTEBOOL__
#ifndef __GNUC__
#define INITGUID
#endif
#include <windows.h>
#include <mmsystem.h>
#include <dbt.h>
#include <dinput.h>

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#ifdef _MSC_VER
#pragma warning(disable:4244)
#endif

#define USE_WINDOWS_DWORD
#include "network.h"
#include "chat.h"
#include "cl_main.h"

// Compensate for w32api's lack
#ifndef GET_XBUTTON_WPARAM
#define GET_XBUTTON_WPARAM(wParam) (HIWORD(wParam))
#endif
#ifndef WM_WTSSESSION_CHANGE
#define WM_WTSSESSION_CHANGE 0x02B1
#define WTS_CONSOLE_CONNECT 1
#define WTS_CONSOLE_DISCONNECT 2
#define WTS_SESSION_LOCK 7
#define WTS_SESSION_UNLOCK 8
#endif
#ifndef SetClassLongPtr
#define SetClassLongPtr SetClassLong
#endif
#ifndef PBT_APMSUSPEND
// w32api does not #define the PBT_ macros in winuser.h like the PSDK does
#include <pbt.h>
#endif

#define USE_WINDOWS_DWORD
#include "c_dispatch.h"
#include "doomtype.h"
#include "doomdef.h"
#include "doomstat.h"
#include "m_argv.h"
#include "i_input.h"
#include "v_video.h"
#include "i_sound.h"
#include "m_menu.h"
#include "g_game.h"
#include "d_main.h"
#include "d_gui.h"
#include "c_console.h"
#include "c_cvars.h"
#include "i_system.h"
#include "s_sound.h"
#include "m_misc.h"
#include "gameconfigfile.h"
#include "win32iface.h"
#include "templates.h"
#include "cmdlib.h"
#include "d_event.h"
#include "v_text.h"

#define DINPUT_BUFFERSIZE	32


class FInputDevice
{
public:
	virtual bool GetDevice() = 0;
	virtual void ProcessInput() = 0;
	virtual bool WndProcHook(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, LRESULT *result);
};

class FMouse : public FInputDevice
{
public:
	FMouse();

	virtual void Grab() = 0;
	virtual void Ungrab() = 0;

protected:
	void PostMouseMove(int x, int y);
	void WheelMoved(int wheelmove);
	void PostButtonEvent(int button, bool down);
	void ClearButtonState();

	int LastX, LastY;	// for m_filter
	WORD ButtonState;	// bit mask of current button states (1=down, 0=up)
};

#ifdef _DEBUG
#define INGAME_PRIORITY_CLASS	NORMAL_PRIORITY_CLASS
#else
//#define INGAME_PRIORITY_CLASS	HIGH_PRIORITY_CLASS
#define INGAME_PRIORITY_CLASS	NORMAL_PRIORITY_CLASS
#endif

BOOL DI_InitJoy (void);

extern HINSTANCE g_hInst;
extern DWORD SessionID;

extern void ShowEAXEditor ();
extern bool SpawnEAXWindow;

static HMODULE DInputDLL;

static void KeyRead ();
static BOOL I_StartupKeyboard ();
static void I_StartupMouse ();
static void CenterMouse_Win32 (LONG curx, LONG cury);
static void DI_Acquire (LPDIRECTINPUTDEVICE8 mouse);
static void DI_Unacquire (LPDIRECTINPUTDEVICE8 mouse);
static void SetCursorState (bool visible);
static HRESULT InitJoystick ();

static bool GUICapture;
static bool NativeMouse;

bool VidResizing;

extern bool SpawnEAXWindow;
extern BOOL vidactive;
extern HWND Window, ConWindow;
extern HWND EAXEditWindow;

extern void UpdateJoystickMenu ();
extern menu_t JoystickMenu;

EXTERN_CVAR (String, language)
EXTERN_CVAR (Bool, lookstrafe)


typedef enum { none, win32, dinput } mousemode_t;
static mousemode_t mousemode = none;

extern BOOL paused;
static bool HaveFocus = false;
static bool noidle = false;

static LPDIRECTINPUT8			g_pdi;
static LPDIRECTINPUT			g_pdi3;

static LPDIRECTINPUTDEVICE8		g_pJoy;

// These can also be earlier IDirectInputDevice interfaces.
// Since IDirectInputDevice8 just added new methods to it
// without rearranging the old ones, I just maintain one
// pointer for each device instead of two.

static LPDIRECTINPUTDEVICE8		g_pKey;

FMouse *Mouse;

HCURSOR TheArrowCursor, TheInvisibleCursor;

TArray<GUIDName> JoystickNames;

static DIDEVCAPS JoystickCaps;

float JoyAxes[6];
static int JoyActive;
static BYTE JoyButtons[128];
static BYTE JoyPOV[4];
static BYTE JoyAxisMap[8];
static float JoyAxisThresholds[8];
char *JoyAxisNames[8];
static const size_t Axes[8] =
{
	myoffsetof(DIJOYSTATE2,lX),
	myoffsetof(DIJOYSTATE2,lY),
	myoffsetof(DIJOYSTATE2,lZ),
	myoffsetof(DIJOYSTATE2,lRx),
	myoffsetof(DIJOYSTATE2,lRy),
	myoffsetof(DIJOYSTATE2,lRz),
	myoffsetof(DIJOYSTATE2,rglSlider[0]),
	myoffsetof(DIJOYSTATE2,rglSlider[1])
};
static const BYTE POVButtons[9] = { 0x01, 0x03, 0x02, 0x06, 0x04, 0x0C, 0x08, 0x09, 0x00 };

BOOL AppActive = TRUE;
int SessionState = 0;

CVAR (Bool,  use_mouse,				true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Bool,  m_noprescale,			false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Bool,	 m_filter,				false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

CVAR (Bool,  use_joystick,			false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

CUSTOM_CVAR (GUID, joy_guid,		NULL, CVAR_ARCHIVE|CVAR_GLOBALCONFIG|CVAR_NOINITCALL)
{
	if (g_pJoy != NULL)
	{
		DIDEVICEINSTANCE inst = { sizeof(DIDEVICEINSTANCE), };

		if (SUCCEEDED(g_pJoy->GetDeviceInfo (&inst)) && self != inst.guidInstance)
		{
			DI_InitJoy ();
			UpdateJoystickMenu ();
		}
	}
	else
	{
		DI_InitJoy ();
		UpdateJoystickMenu ();
	}
}

// [BB] Allows to keep the sound turned on, when the client is not the active app.
CVAR (Bool, cl_soundwhennotactive, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

static void MapAxis (FIntCVar &var, int num)
{
	if (var < JOYAXIS_NONE || var > JOYAXIS_UP)
	{
		var = JOYAXIS_NONE;
	}
	else
	{
		JoyAxisMap[num] = var;
	}
}

CUSTOM_CVAR (Int, joy_xaxis,	JOYAXIS_YAW,	 CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	MapAxis (self, 0);
}
CUSTOM_CVAR (Int, joy_yaxis,	JOYAXIS_FORWARD, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	MapAxis (self, 1);
}
CUSTOM_CVAR (Int, joy_zaxis,	JOYAXIS_SIDE,	 CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	MapAxis (self, 2);
}
CUSTOM_CVAR (Int, joy_xrot,		JOYAXIS_NONE,	 CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	MapAxis (self, 3);
}
CUSTOM_CVAR (Int, joy_yrot,		JOYAXIS_NONE,	 CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	MapAxis (self, 4);
}
CUSTOM_CVAR (Int, joy_zrot,		JOYAXIS_PITCH,	 CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	MapAxis (self, 5);
}
CUSTOM_CVAR (Int, joy_slider,	JOYAXIS_NONE,	 CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	MapAxis (self, 6);
}
CUSTOM_CVAR (Int, joy_dial,		JOYAXIS_NONE,	 CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	MapAxis (self, 7);
}

CUSTOM_CVAR (Float, joy_xthreshold,		0.15f,	CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	JoyAxisThresholds[0] = clamp (self * 256.f, 0.f, 256.f);
}
CUSTOM_CVAR (Float, joy_ythreshold,		0.15f,	CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	JoyAxisThresholds[1] = clamp (self * 256.f, 0.f, 256.f);
}
CUSTOM_CVAR (Float, joy_zthreshold,		0.15f,	CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	JoyAxisThresholds[2] = clamp (self * 256.f, 0.f, 256.f);
}
CUSTOM_CVAR (Float, joy_xrotthreshold,	0.15f,	CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	JoyAxisThresholds[3] = clamp (self * 256.f, 0.f, 256.f);
}
CUSTOM_CVAR (Float, joy_yrotthreshold,	0.15f,	CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	JoyAxisThresholds[4] = clamp (self * 256.f, 0.f, 256.f);
}
CUSTOM_CVAR (Float, joy_zrotthreshold,	0.15f,	CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	JoyAxisThresholds[5] = clamp (self * 256.f, 0.f, 256.f);
}
CUSTOM_CVAR (Float, joy_sliderthreshold,	0.f,	CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	JoyAxisThresholds[6] = clamp (self * 256.f, 0.f, 256.f);
}
CUSTOM_CVAR (Float, joy_dialthreshold,	0.f,	CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	JoyAxisThresholds[7] = clamp (self * 256.f, 0.f, 256.f);
}

CVAR (Float, joy_speedmultiplier,1.f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Float, joy_yawspeed,		-1.f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Float, joy_pitchspeed,	-.75f,CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Float, joy_forwardspeed,	-1.f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Float, joy_sidespeed,		 1.f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Float, joy_upspeed,		-1.f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

// set this to false to make keypad-enter a usable separate key!
CVAR (Bool, k_mergekeys, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Bool, k_allowfullscreentoggle, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

static FBaseCVar * const JoyConfigVars[] =
{
	&joy_xaxis, &joy_yaxis, &joy_zaxis, &joy_xrot, &joy_yrot, &joy_zrot, &joy_slider, &joy_dial,
	&joy_xthreshold, &joy_ythreshold, &joy_zthreshold, &joy_xrotthreshold, &joy_yrotthreshold, &joy_zrotthreshold, &joy_sliderthreshold, &joy_dialthreshold,
	&joy_speedmultiplier, &joy_yawspeed, &joy_pitchspeed, &joy_forwardspeed, &joy_sidespeed,
	&joy_upspeed
};

CUSTOM_CVAR (Int, in_mouse, 0, CVAR_ARCHIVE|CVAR_GLOBALCONFIG|CVAR_NOINITCALL)
{
	if (self < 0)
	{
		self = 0;
	}
	else if (self > 2)
	{
		self = 2;
	}
	else if (g_pdi == NULL && g_pdi3 == NULL)
	{
		return;
	}
	else
	{
		I_StartupMouse();
	}
}

static BYTE KeyState[256];
static BYTE DIKState[2][NUM_KEYS];
static int KeysReadCount;
static int ActiveDIKState;

// Convert DIK_* code to ASCII using Qwerty keymap
static const BYTE Convert [256] =
{
  //  0    1    2    3    4    5    6    7    8    9    A    B    C    D    E    F
	  0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=',   8,   9, // 0
	'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']',  13,   0, 'a', 's', // 1
	'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',  39, '`',   0,'\\', 'z', 'x', 'c', 'v', // 2
	'b', 'n', 'm', ',', '.', '/',   0, '*',   0, ' ',   0,   0,   0,   0,   0,   0, // 3
	  0,   0,   0,   0,   0,   0,   0, '7', '8', '9', '-', '4', '5', '6', '+', '1', // 4
	'2', '3', '0', '.',   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, // 5
	  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, // 6
	  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, // 7

	  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, '=',   0,   0, // 8
	  0, '@', ':', '_',   0,   0,   0,   0,   0,   0,   0,   0,  13,   0,   0,   0, // 9
	  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, // A
	  0,   0,   0, ',',   0, '/',   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, // B
	  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, // C
	  0,   0,   0,   0,   0,   0,   0,   0

};

static void FlushDIKState (int low=0, int high=NUM_KEYS-1)
{
	int i;
	event_t event;
	BYTE *state = DIKState[ActiveDIKState];

	memset (&event, 0, sizeof(event));
	event.type = EV_KeyUp;
	for (i = low; i <= high; ++i)
	{
		if (state[i])
		{
			state[i] = 0;
			event.data1 = i;
			event.data2 = i < 256 ? Convert[i] : 0;
			D_PostEvent (&event);
		}
	}
}

static void I_CheckGUICapture ()
{
	bool wantCapt;

	if (menuactive == MENU_Off)
	{
		wantCapt = ConsoleState == c_down || ConsoleState == c_falling || CHAT_GetChatMode();
	}
	else
	{
		wantCapt = (menuactive == MENU_On || menuactive == MENU_OnNoPause);
	}

	if (wantCapt != GUICapture)
	{
		GUICapture = wantCapt;
		if (wantCapt)
		{
			FlushDIKState ();
		}
	}
}

CUSTOM_CVAR(Int, mouse_capturemode, 1, CVAR_GLOBALCONFIG|CVAR_ARCHIVE)
{
	if (self < 0) self = 0;
	else if (self > 2) self = 2;
}

static bool inGame()
{
	switch (mouse_capturemode)
	{
	default:
	case 0:
		return gamestate == GS_LEVEL;
	case 1:
		return gamestate == GS_LEVEL || gamestate == GS_INTERMISSION || gamestate == GS_FINALE;
	case 2:
		return true;
	}
}

void I_CheckNativeMouse(bool preferNative)
{
	bool wantNative = !HaveFocus ||
		((!screen || !screen->IsFullscreen()) && 
		(!inGame() || GUICapture || paused || preferNative || !use_mouse));

	//Printf ("%d %d %d\n", wantNative, preferNative, NativeMouse);

	if (wantNative != NativeMouse)
	{
		if (wantNative)
		{
			Mouse->Ungrab();
		}
		else
		{
			Mouse->Grab();
		}
		NativeMouse = wantNative;
	}
}

LRESULT CALLBACK WndProc (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (Mouse != NULL)
	{
		LRESULT result;

		if (Mouse->WndProcHook(hWnd, message, wParam, lParam, &result))
		{
			return result;
		}
	}
	event_t event;

	memset (&event, 0, sizeof(event));

	switch (message)
	{
	case WM_DESTROY:
		SetPriorityClass (GetCurrentProcess(), NORMAL_PRIORITY_CLASS);
		//PostQuitMessage (0);
		exit (0);
		break;

	case WM_HOTKEY:
		break;

	case WM_PAINT:
		if (screen != NULL && 0)
		{
			static_cast<BaseWinFB *> (screen)->PaintToWindow ();
		}
		return DefWindowProc (hWnd, message, wParam, lParam);

	case WM_SETTINGCHANGE:
		// If regional settings were changed, reget preferred languages
		if (wParam == 0 && lParam != 0 && strcmp ((const char *)lParam, "intl") == 0)
		{
			language.Callback ();
		}
		return 0;

	case WM_KILLFOCUS:
		if (g_pKey) g_pKey->Unacquire ();
		
		FlushDIKState ();
		HaveFocus = false;
		I_CheckNativeMouse (true);	// Make sure mouse gets released right away
		break;

	case WM_SETFOCUS:
		if (g_pKey)
		{
			g_pKey->Acquire();
		}
		HaveFocus = true;
		I_CheckNativeMouse (false);
		break;

	case WM_SIZE:
		InvalidateRect (Window, NULL, FALSE);
		break;

	// Being forced to separate my keyboard input handler into
	// two pieces like this really stinks. (IMHO)
	case WM_KEYDOWN:
		// When the EAX editor is open, pressing Ctrl+Tab will switch to it
		if (EAXEditWindow != 0 && wParam == VK_TAB && !(lParam & 0x40000000) &&
			(GetKeyState (VK_CONTROL) & 0x8000))
		{
			SetForegroundWindow (EAXEditWindow);
			return 0;
		}
		// Intentional fall-through
	case WM_KEYUP:
		GetKeyboardState (KeyState);
		if (GUICapture)
		{
			event.type = EV_GUI_Event;
			if (message == WM_KEYUP)
			{
				event.subtype = EV_GUI_KeyUp;
			}
			else
			{
				event.subtype = (lParam & 0x40000000) ? EV_GUI_KeyRepeat : EV_GUI_KeyDown;
			}
			event.data3 = ((KeyState[VK_SHIFT]&128) ? GKM_SHIFT : 0) |
						  ((KeyState[VK_CONTROL]&128) ? GKM_CTRL : 0) |
						  ((KeyState[VK_MENU]&128) ? GKM_ALT : 0);
			if ( (event.data1 = MapVirtualKey (wParam, 2)) )
			{
				ToAscii (wParam, (lParam >> 16) & 255, KeyState, (LPWORD)&event.data2, 0);
				D_PostEvent (&event);
			}
			else
			{
				switch (wParam)
				{
				case VK_PRIOR:	event.data1 = GK_PGUP;		break;
				case VK_NEXT:	event.data1 = GK_PGDN;		break;
				case VK_END:	event.data1 = GK_END;		break;
				case VK_HOME:	event.data1 = GK_HOME;		break;
				case VK_LEFT:	event.data1 = GK_LEFT;		break;
				case VK_RIGHT:	event.data1 = GK_RIGHT;		break;
				case VK_UP:		event.data1 = GK_UP;		break;
				case VK_DOWN:	event.data1 = GK_DOWN;		break;
				case VK_DELETE:	event.data1 = GK_DEL;		break;
				case VK_ESCAPE:	event.data1 = GK_ESCAPE;	break;
				case VK_F1:		event.data1 = GK_F1;		break;
				case VK_F2:		event.data1 = GK_F2;		break;
				case VK_F3:		event.data1 = GK_F3;		break;
				case VK_F4:		event.data1 = GK_F4;		break;
				case VK_F5:		event.data1 = GK_F5;		break;
				case VK_F6:		event.data1 = GK_F6;		break;
				case VK_F7:		event.data1 = GK_F7;		break;
				case VK_F8:		event.data1 = GK_F8;		break;
				case VK_F9:		event.data1 = GK_F9;		break;
				case VK_F10:	event.data1 = GK_F10;		break;
				case VK_F11:	event.data1 = GK_F11;		break;
				case VK_F12:	event.data1 = GK_F12;		break;
				}
				if (event.data1 != 0)
				{
					event.data2 = event.data1;
					D_PostEvent (&event);
				}
			}
		}
		else
		{
			if (message == WM_KEYUP)
			{
				event.type = EV_KeyUp;
			}
			else
			{
				if (lParam & 0x40000000)
				{
					return 0;
				}
				else
				{
					event.type = EV_KeyDown;
				}
			}

			switch (wParam)
			{
				case VK_PAUSE:
					event.data1 = KEY_PAUSE;
					break;
				case VK_TAB:
					event.data1 = DIK_TAB;
					event.data2 = '\t';
					break;
				case VK_NUMLOCK:
					event.data1 = DIK_NUMLOCK;
					break;
			}
			if (event.data1)
			{
				DIKState[ActiveDIKState][event.data1] = (event.type == EV_KeyDown);
				D_PostEvent (&event);
			}
		}
		break;

	case WM_CHAR:
		if (GUICapture && wParam >= ' ')	// only send displayable characters
		{
			event.type = EV_GUI_Event;
			event.subtype = EV_GUI_Char;
			event.data1 = wParam;
			D_PostEvent (&event);
		}
		break;

	case WM_SYSCHAR:
		if (GUICapture && wParam >= '0' && wParam <= '9')	// make chat macros work
		{
			event.type = EV_GUI_Event;
			event.subtype = EV_GUI_Char;
			event.data1 = wParam;
			event.data2 = 1;
			D_PostEvent (&event);
		}
		if (wParam == '\r' && k_allowfullscreentoggle)
		{
			ToggleFullscreen = !ToggleFullscreen;
		}
		break;

	case WM_SYSCOMMAND:
		{
			WPARAM cmdType = wParam & 0xfff0;

			// Prevent activation of the window menu with Alt-Space
			if (cmdType != SC_KEYMENU)
				return DefWindowProc (hWnd, message, wParam, lParam);
		}
		break;

	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP:
		if (GUICapture)
		{
			event.type = EV_GUI_Event;
			if (message >= WM_LBUTTONDOWN && message <= WM_LBUTTONDBLCLK)
			{
				event.subtype = message - WM_LBUTTONDOWN + EV_GUI_LButtonDown;
			}
			else if (message >= WM_RBUTTONDOWN && message <= WM_RBUTTONDBLCLK)
			{
				event.subtype = message - WM_RBUTTONDOWN + EV_GUI_RButtonDown;
			}
			else if (message >= WM_MBUTTONDOWN && message <= WM_MBUTTONDBLCLK)
			{
				event.subtype = message - WM_MBUTTONDOWN + EV_GUI_MButtonDown;
			}
			D_PostEvent (&event);
		}
		break;

	case WM_DISPLAYCHANGE:
		if (SpawnEAXWindow)
		{
			SpawnEAXWindow = false;
			ShowEAXEditor ();
		}
		break;

	case WM_GETMINMAXINFO:
		if (screen && !VidResizing)
		{
			LPMINMAXINFO mmi = (LPMINMAXINFO)lParam;
			mmi->ptMinTrackSize.x = SCREENWIDTH + GetSystemMetrics (SM_CXSIZEFRAME) * 2;
			mmi->ptMinTrackSize.y = SCREENHEIGHT + GetSystemMetrics (SM_CYSIZEFRAME) * 2 +
									GetSystemMetrics (SM_CYCAPTION);
			return 0;
		}
		break;

	case WM_ACTIVATEAPP:
		AppActive = wParam;
		if (wParam)
		{
			SetPriorityClass (GetCurrentProcess (), INGAME_PRIORITY_CLASS);
		}
		else if (!noidle && NETWORK_GetState( ) != NETSTATE_CLIENT )
		{
			SetPriorityClass (GetCurrentProcess (), IDLE_PRIORITY_CLASS);
		}
		if ( ( NETWORK_GetState( ) != NETSTATE_CLIENT ) || ( cl_soundwhennotactive == false ) )
			S_SetSoundPaused (wParam);
		break;

	case WM_WTSSESSION_CHANGE:
	case WM_POWERBROADCAST:
		{
			int oldstate = SessionState;

			if (message == WM_WTSSESSION_CHANGE && lParam == (LPARAM)SessionID)
			{
#ifdef _DEBUG
				OutputDebugString ("SessionID matched\n");
#endif
				// When using fast user switching, XP will lock a session before
				// disconnecting it, and the session will be unlocked before reconnecting it.
				// For our purposes, video output will only happen when the session is
				// both unlocked and connected (that is, SessionState is 0).
#ifndef __WINE__
				switch (wParam)
				{
				case WTS_SESSION_LOCK:
					SessionState |= 1;
					break;
				case WTS_SESSION_UNLOCK:
					SessionState &= ~1;
					break;
				case WTS_CONSOLE_DISCONNECT:
					SessionState |= 2;
					//I_MovieDisableSound ();
					break;
				case WTS_CONSOLE_CONNECT:
					SessionState &= ~2;
					//I_MovieResumeSound ();
					break;
				}
#endif
			}
			else if (message == WM_POWERBROADCAST)
			{
				switch (wParam)
				{
				case PBT_APMSUSPEND:
					SessionState |= 4;
					break;
				case PBT_APMRESUMESUSPEND:
					SessionState &= ~4;
					break;
				}
			}

			if (GSnd != NULL)
			{
#if 0
				// Do we actually need this here?
				if (!oldstate && SessionState)
				{
					GSnd->SuspendSound ();
				}
				else if (oldstate && !SessionState)
				{
					GSnd->MovieResumeSound ();
				}
#endif
			}
#ifdef _DEBUG
			char foo[256];
			mysnprintf (foo, countof(foo), "Session Change: %ld %d\n", lParam, wParam);
			OutputDebugString (foo);
#endif
		}
		break;

	case WM_DEVICECHANGE:
#ifndef __WINE__
		if (wParam == DBT_DEVNODES_CHANGED ||
			wParam == DBT_DEVICEARRIVAL ||
			wParam == DBT_CONFIGCHANGED)
#else
		if (0)
#endif
		{
			unsigned int i;
			TArray<GUID> oldjoys;

			for (i = 0; i < JoystickNames.Size(); ++i)
			{
				oldjoys.Push (JoystickNames[i].ID);
			}

			DI_EnumJoy ();

			// If a new joystick was added and the joystick menu is open,
			// switch to it.
			if (menuactive != MENU_Off && CurrentMenu == &JoystickMenu)
			{
				for (i = 0; i < JoystickNames.Size(); ++i)
				{
					bool wasListed = false;

					for (unsigned int j = 0; j < oldjoys.Size(); ++j)
					{
						if (oldjoys[j] == JoystickNames[i].ID)
						{
							wasListed = true;
							break;
						}
					}
					if (!wasListed)
					{
						joy_guid = JoystickNames[i].ID;
						break;
					}
				}
			}

			// If the current joystick was removed,
			// try to switch to a different one.
			if (g_pJoy != NULL)
			{
				DIDEVICEINSTANCE inst = { sizeof(DIDEVICEINSTANCE), };

				if (SUCCEEDED(g_pJoy->GetDeviceInfo (&inst)))
				{
					for (i = 0; i < JoystickNames.Size(); ++i)
					{
						if (JoystickNames[i].ID == inst.guidInstance)
						{
							break;
						}
					}
					if (i == JoystickNames.Size ())
					{
						DI_InitJoy ();
					}
				}
			}
			else
			{
				DI_InitJoy ();
			}
			UpdateJoystickMenu ();
		}
		break;

	case WM_PALETTECHANGED:
		if ((HWND)wParam == Window)
			break;
		if (screen != NULL)
		{
			screen->PaletteChanged ();
		}
		return DefWindowProc (hWnd, message, wParam, lParam);

	case WM_QUERYNEWPALETTE:
		if (screen != NULL)
		{
			return screen->QueryNewPalette ();
		}
		return DefWindowProc (hWnd, message, wParam, lParam);

	case WM_ERASEBKGND:
		return true;

	default:
		return DefWindowProc (hWnd, message, wParam, lParam);
	}

	return 0;
}

/****** Joystick stuff ******/

void DI_JoyCheck ()
{
	float mul;
	event_t event;
	HRESULT hr;
	DIJOYSTATE2 js;
	int i;
	BYTE pov;

	if (g_pJoy == NULL)
	{
		return;
	}

	hr = g_pJoy->Poll ();
	if (FAILED(hr))
	{
		do
		{
			hr = g_pJoy->Acquire ();
		}
		while (hr == DIERR_INPUTLOST);
		if (FAILED(hr))
			return;
	}

	hr = g_pJoy->GetDeviceState (sizeof(DIJOYSTATE2), &js);
	if (FAILED(hr))
		return;

	mul = joy_speedmultiplier;
	if (Button_Speed.bDown)
	{
		mul *= 0.5f;
	}

	for (i = 0; i < 6; ++i)
	{
		JoyAxes[i] = 0.f;
	}

	for (i = 0; i < 8; ++i)
	{
		int vaxis = JoyAxisMap[i];

		if (vaxis != JOYAXIS_NONE)
		{
			if (vaxis == JOYAXIS_YAW && (Button_Strafe.bDown ||
				(Button_Mlook.bDown && lookstrafe)))
			{
				vaxis = JOYAXIS_SIDE;
			}
			else if (vaxis == JOYAXIS_FORWARD && Button_Mlook.bDown)
			{
				vaxis = JOYAXIS_PITCH;
			}

			float axisval = *((LONG *)((BYTE *)&js + Axes[i]));
			if (fabsf(axisval) > JoyAxisThresholds[i])
			{
				if (axisval > 0.f)
				{
					axisval -= JoyAxisThresholds[i];
				}
				else
				{
					axisval += JoyAxisThresholds[i];
				}
				JoyAxes[vaxis] += axisval * mul * 256.f / (256.f - JoyAxisThresholds[i]);
			}
		}
	}

	JoyAxes[JOYAXIS_YAW] *= joy_yawspeed;
	JoyAxes[JOYAXIS_PITCH] *= joy_pitchspeed;
	JoyAxes[JOYAXIS_FORWARD] *= joy_forwardspeed;
	JoyAxes[JOYAXIS_SIDE] *= joy_sidespeed;
	JoyAxes[JOYAXIS_UP] *= joy_upspeed;

	event.data2 = event.data3 = 0;

	// Send button up/down events

	for (i = 0; i < 128; ++i)
	{
		if ((js.rgbButtons[i] ^ JoyButtons[i]) & 0x80)
		{
			event.data1 = KEY_FIRSTJOYBUTTON + i;
			if (JoyButtons[i])
			{
				event.type = EV_KeyUp;
				JoyButtons[i] = 0;
			}
			else
			{
				event.type = EV_KeyDown;
				JoyButtons[i] = 0x80;
			}
			D_PostEvent (&event);
		}
	}

	for (i = 0; i < 4; ++i)
	{
		if (LOWORD(js.rgdwPOV[i]) == 0xFFFF)
		{
			pov = 8;
		}
		else
		{
			pov = ((js.rgdwPOV[i] + 2250) % 36000) / 4500;
		}
		pov = POVButtons[pov];
		for (int j = 0; j < 4; ++j)
		{
			BYTE mask = 1 << j;

			if ((JoyPOV[i] ^ pov) & mask)
			{
				event.data1 = KEY_JOYPOV1_UP + i*4 + j;
				event.type = (pov & mask) ? EV_KeyDown : EV_KeyUp;
				D_PostEvent (&event);
			}
		}
		JoyPOV[i] = pov;
	}
}

bool SetJoystickSection (bool create)
{
	DIDEVICEINSTANCE inst = { sizeof(DIDEVICEINSTANCE), };
	char section[80] = "Joystick.";

	if (g_pJoy != NULL && SUCCEEDED(g_pJoy->GetDeviceInfo (&inst)))
	{
		FormatGUID (section + 9, countof(section) - 9, inst.guidInstance);
		strcpy (section + 9 + 38, ".Axes");
		return GameConfig->SetSection (section, create);
	}
	else
	{
		return false;
	}
}

void LoadJoystickConfig ()
{
	if (SetJoystickSection (false))
	{
		for (size_t i = 0; i < countof(JoyConfigVars); ++i)
		{
			const char *val = GameConfig->GetValueForKey (JoyConfigVars[i]->GetName());
			UCVarValue cval;

			if (val != NULL)
			{
				cval.String = const_cast<char *>(val);
				JoyConfigVars[i]->SetGenericRep (cval, CVAR_String);
			}
		}
	}
}

void SaveJoystickConfig ()
{
	if (SetJoystickSection (true))
	{
		GameConfig->ClearCurrentSection ();
		for (size_t i = 0; i < countof(JoyConfigVars); ++i)
		{
			UCVarValue cval = JoyConfigVars[i]->GetGenericRep (CVAR_String);
			GameConfig->SetValueForKey (JoyConfigVars[i]->GetName(), cval.String);
		}
	}
}

BOOL CALLBACK EnumJoysticksCallback (LPCDIDEVICEINSTANCE lpddi, LPVOID pvRef)
{
	GUIDName name;

	JoyActive++;
	name.ID = lpddi->guidInstance;
	name.Name = copystring (lpddi->tszInstanceName);
	JoystickNames.Push (name);
	return DIENUM_CONTINUE;
}

void DI_EnumJoy ()
{
	unsigned int i;

	for (i = 0; i < JoystickNames.Size(); ++i)
	{
		delete[] JoystickNames[i].Name;
	}

	JoyActive = 0;
	JoystickNames.Clear ();

	if (g_pdi != NULL && !Args->CheckParm ("-nojoy"))
	{
		g_pdi->EnumDevices (DI8DEVCLASS_GAMECTRL, EnumJoysticksCallback, NULL, DIEDFL_ALLDEVICES);
	}
}

BOOL DI_InitJoy (void)
{
	HRESULT hr;
	unsigned int i;

	if (g_pdi == NULL)
	{
		return TRUE;
	}

	if (g_pJoy != NULL)
	{
		SaveJoystickConfig ();
		g_pJoy->Release ();
		g_pJoy = NULL;
	}

	if (JoystickNames.Size() == 0)
	{
		return TRUE;
	}

	// Try to obtain the joystick specified by joy_guid
	for (i = 0; i < JoystickNames.Size(); ++i)
	{
		if (JoystickNames[i].ID == joy_guid)
		{
			hr = g_pdi->CreateDevice (JoystickNames[i].ID, &g_pJoy, NULL);
			if (FAILED(hr))
			{
				i = JoystickNames.Size();
			}
			break;
		}
	}

	// If the preferred joystick could not be obtained, grab the first
	// one available.
	if (i == JoystickNames.Size())
	{
		for (i = 0; i <= JoystickNames.Size(); ++i)
		{
			hr = g_pdi->CreateDevice (JoystickNames[i].ID, &g_pJoy, NULL);
			if (SUCCEEDED(hr))
			{
				break;
			}
		}
	}

	if (i == JoystickNames.Size())
	{
		JoyActive = 0;
		return TRUE;
	}

	if (FAILED (InitJoystick ()))
	{
		JoyActive = 0;
		g_pJoy->Release ();
		g_pJoy = NULL;
	}
	else
	{
		LoadJoystickConfig ();
		joy_guid = JoystickNames[i].ID;
	}

	return TRUE;
}

BOOL CALLBACK EnumAxesCallback (LPCDIDEVICEOBJECTINSTANCE lpddoi, LPVOID pvRef)
{
	DIPROPRANGE diprg =
	{
		{
			sizeof (DIPROPRANGE),
			sizeof (DIPROPHEADER),
			lpddoi->dwType,
			DIPH_BYID
		},
		-256,
		+256
	};
	if (lpddoi->wUsagePage == 1)
	{
		if (lpddoi->wUsage >= 0x30 && lpddoi->wUsage <= 0x37)
		{
			JoyAxisNames[lpddoi->wUsage-0x30] = copystring (lpddoi->tszName);
		}
	}
	if (FAILED(g_pJoy->SetProperty (DIPROP_RANGE, &diprg.diph)))
	{
		return DIENUM_STOP;
	}
	else
	{
		return DIENUM_CONTINUE;
	}
}

static HRESULT InitJoystick ()
{
	HRESULT hr;

	memset (JoyPOV, 9, sizeof(JoyPOV));
	for (int i = 0; i < 8; ++i)
	{
		if (JoyAxisNames[i])
		{
			delete[] JoyAxisNames[i];
			JoyAxisNames[i] = NULL;
		}
	}

	hr = g_pJoy->SetDataFormat (&c_dfDIJoystick2);
	if (FAILED(hr))
	{
		Printf (PRINT_BOLD, "Could not set joystick data format.\n");
		return hr;
	}

	hr = g_pJoy->SetCooperativeLevel (Window, DISCL_EXCLUSIVE|DISCL_FOREGROUND);
	if (FAILED(hr))
	{
		Printf (PRINT_BOLD, "Could not set joystick cooperative level.\n");
		return hr;
	}

	JoystickCaps.dwSize = sizeof(JoystickCaps);
	hr = g_pJoy->GetCapabilities (&JoystickCaps);
	if (FAILED(hr))
	{
		Printf (PRINT_BOLD, "Could not query joystick capabilities.\n");
		return hr;
	}

	hr = g_pJoy->EnumObjects (EnumAxesCallback, NULL, DIDFT_AXIS);
	if (FAILED(hr))
	{
		Printf (PRINT_BOLD, "Could not set joystick axes ranges.\n");
		return hr;
	}

	g_pJoy->Acquire ();

	return S_OK;
}

/****** Stuff from Andy Bay's mymouse.c ******/

/****************************************************************************
 *
 *		DIInit
 *
 *		Initialize the DirectInput variables.
 *
 ****************************************************************************/


// [RH] Used to obtain DirectInput access to the mouse.
//		(Preferred for Win95, but buggy under NT 4.)

bool I_InitInput (void *hwnd)
{
	HRESULT hr;

	Printf ("I_InitInput\n");
	atterm (I_ShutdownInput);

	NativeMouse = true;

	noidle = !!Args->CheckParm ("-noidle");
	g_pdi = NULL;
	g_pdi3 = NULL;

	// Try for DirectInput 8 first, then DirectInput 3 for NT 4's benefit.

	DInputDLL = LoadLibrary("dinput8.dll");
	if (DInputDLL != NULL)
	{
		typedef HRESULT (WINAPI *blah)(HINSTANCE, DWORD, REFIID, LPVOID *, LPUNKNOWN);
		blah di8c = (blah)GetProcAddress(DInputDLL, "DirectInput8Create");
		if (di8c != NULL)
		{
			hr = di8c(g_hInst, DIRECTINPUT_VERSION, IID_IDirectInput8A, (void **)&g_pdi, NULL);
			if (FAILED(hr))
			{
				Printf(TEXTCOLOR_ORANGE "DirectInput8Create failed: %08lx", hr);
				g_pdi = NULL;	// Just to be sure DirectInput8Create didn't change it
			}
		}
		else
		{
			Printf(TEXTCOLOR_ORANGE "Could not find DirectInput8Create in dinput8.dll\n");
		}
	}

	if (g_pdi == NULL)
	{
		if (DInputDLL != NULL)
		{
			FreeLibrary(DInputDLL);
		}
		DInputDLL = LoadLibrary ("dinput.dll");
		if (DInputDLL == NULL)
		{
			I_FatalError ("Could not load dinput.dll: %08lx", GetLastError());
		}

		typedef HRESULT (WINAPI *blah)(HINSTANCE, DWORD, LPDIRECTINPUT*, LPUNKNOWN);
		blah dic = (blah)GetProcAddress (DInputDLL, "DirectInputCreateA");

		if (dic == NULL)
		{
			I_FatalError ("dinput.dll is corrupt");
		}

		hr = dic (g_hInst, 0x0300, &g_pdi3, NULL);
		if (FAILED(hr))
		{
			I_FatalError ("DirectInputCreate failed: %08lx", hr);
		}
	}

	Printf ("I_StartupMouse\n");
	I_StartupMouse();

	Printf ("I_StartupJoystick\n");
	DI_EnumJoy ();
	DI_InitJoy ();

	Printf ("I_StartupKeyboard\n");
	I_StartupKeyboard();

	return TRUE;
}


// Free all input resources
void I_ShutdownInput ()
{
	if (g_pKey)
	{
		g_pKey->Unacquire ();
		g_pKey->Release ();
		g_pKey = NULL;
	}
	if (Mouse != NULL)
	{
		delete Mouse;
	}
	if (g_pJoy)
	{
		SaveJoystickConfig ();
		g_pJoy->Release ();
		g_pJoy = NULL;
	}
	if (g_pdi)
	{
		g_pdi->Release ();
		g_pdi = NULL;
	}
	if (g_pdi3)
	{
		g_pdi3->Release ();
		g_pdi3 = NULL;
	}
	if (DInputDLL != NULL)
	{
		FreeLibrary (DInputDLL);
		DInputDLL = NULL;
	}
}

static void SetCursorState(bool visible)
{
	HCURSOR usingCursor = visible ? TheArrowCursor : TheInvisibleCursor;
	SetClassLongPtr(Window, GCLP_HCURSOR, (LONG_PTR)usingCursor);
	if (HaveFocus)
	{
		SetCursor(usingCursor);
	}
}

// Initialize the keyboard
static BOOL I_StartupKeyboard (void)
{
	HRESULT hr;

	// Obtain an interface to the system key device.
	if (g_pdi3)
	{
		hr = g_pdi3->CreateDevice (GUID_SysKeyboard, (LPDIRECTINPUTDEVICE*)&g_pKey, NULL);
	}
	else
	{
		hr = g_pdi->CreateDevice (GUID_SysKeyboard, &g_pKey, NULL);
	}

	if (FAILED(hr))
	{
		I_FatalError ("Could not create keyboard device");
	}

	// Set the data format to "keyboard format".
	hr = g_pKey->SetDataFormat (&c_dfDIKeyboard);

	if (FAILED(hr))
	{
		I_FatalError ("Could not set keyboard data format");
	}

	// Set the cooperative level.
	hr = g_pKey->SetCooperativeLevel (Window, DISCL_FOREGROUND|DISCL_NONEXCLUSIVE);

	if (FAILED(hr))
	{
		I_FatalError("Could not set keyboard cooperative level");
	}

	g_pKey->Acquire ();
	return TRUE;
}

static void KeyRead ()
{
	HRESULT hr;
	event_t event;
	BYTE *fromState, *toState;
	int i;

	if (g_pKey == NULL)
	{
		return;
	}

	memset (&event, 0, sizeof(event));
	fromState = DIKState[ActiveDIKState];
	toState = DIKState[ActiveDIKState ^ 1];

	hr = g_pKey->GetDeviceState (256, toState);
	if (hr == DIERR_INPUTLOST || hr == DIERR_NOTACQUIRED)
	{
		hr = g_pKey->Acquire ();
		if (hr != DI_OK)
		{
			return;
		}
		hr = g_pKey->GetDeviceState (256, toState);
	}
	if (hr != DI_OK)
	{
		return;
	}

	// Successfully got the buffer
	KeysReadCount++;
	ActiveDIKState ^= 1;

	// Copy key states not handled here from the old to the new buffer
	memcpy (toState + 256, fromState + 256, NUM_KEYS - 256);
	toState[DIK_TAB] = fromState[DIK_TAB];
	toState[DIK_NUMLOCK] = fromState[DIK_NUMLOCK];

	if (k_mergekeys)
	{
		// "Merge" multiple keys that are considered to be the same.
		// Also clear out the alternate versions after merging.
		toState[DIK_RETURN]		|= toState[DIK_NUMPADENTER];
		toState[DIK_LMENU]		|= toState[DIK_RMENU];
		toState[DIK_LCONTROL]	|= toState[DIK_RCONTROL];
		toState[DIK_LSHIFT]		|= toState[DIK_RSHIFT];

		toState[DIK_NUMPADENTER] = 0;
		toState[DIK_RMENU]		 = 0;
		toState[DIK_RCONTROL]	 = 0;
		toState[DIK_RSHIFT]		 = 0;
	}

	// Now generate events for any keys that differ between the states
	if (!GUICapture)
	{
		for (i = 1; i < 256; i++)
		{
			if (toState[i] != fromState[i])
			{
				event.type = toState[i] ? EV_KeyDown : EV_KeyUp;
				event.data1 = i;
				event.data2 = Convert[i];
				event.data3 = (toState[DIK_LSHIFT] ? GKM_SHIFT : 0) |
							  (toState[DIK_LCONTROL] ? GKM_CTRL : 0) |
							  (toState[DIK_LMENU] ? GKM_ALT : 0);
				D_PostEvent (&event);
			}
		}
	}
}

void I_GetEvent ()
{
	MSG mess;

	// Briefly enter an alertable state so that if a secondary thread
	// crashed, we will execute the APC it sent now.
	SleepEx (0, TRUE);

	while (PeekMessage (&mess, NULL, 0, 0, PM_REMOVE))
	{
		if (mess.message == WM_QUIT)
			exit (mess.wParam);
		if (EAXEditWindow == 0 || !IsDialogMessage (EAXEditWindow, &mess))
		{
			TranslateMessage (&mess);
			DispatchMessage (&mess);
		}
	}

	KeyRead ();

	if (Mouse != NULL)
	{
		Mouse->ProcessInput();
	}
}


//
// I_StartTic
//
void I_StartTic ()
{
	ResetButtonTriggers ();
	I_CheckGUICapture ();
	I_CheckNativeMouse (false);
	I_GetEvent ();
}

//
// I_StartFrame
//
void I_StartFrame ()
{
	if (use_joystick)
	{
		DI_JoyCheck ();
	}
}

void I_PutInClipboard (const char *str)
{
	if (str == NULL || !OpenClipboard (Window))
		return;
	EmptyClipboard ();

	HGLOBAL cliphandle = GlobalAlloc (GMEM_DDESHARE, strlen (str) + 1);
	if (cliphandle != NULL)
	{
		char *ptr = (char *)GlobalLock (cliphandle);
		strcpy (ptr, str);
		GlobalUnlock (cliphandle);
		SetClipboardData (CF_TEXT, cliphandle);
	}
	CloseClipboard ();
}

FString I_GetFromClipboard (bool return_nothing)
{
	FString retstr;
	HGLOBAL cliphandle;
	char *clipstr;
	char *nlstr;

	if (return_nothing || !IsClipboardFormatAvailable (CF_TEXT) || !OpenClipboard (Window))
		return retstr;

	cliphandle = GetClipboardData (CF_TEXT);
	if (cliphandle != NULL)
	{
		clipstr = (char *)GlobalLock (cliphandle);
		if (clipstr != NULL)
		{
			// Convert CR-LF pairs to just LF while copying to the FString
			for (nlstr = clipstr; *nlstr != '\0'; ++nlstr)
			{
				if (nlstr[0] == '\r' && nlstr[1] == '\n')
				{
					nlstr++;
				}
				retstr += *nlstr;
			}
			GlobalUnlock (clipstr);
		}
	}

	CloseClipboard ();
	return retstr;
}

#include "i_movie.h"

CCMD (playmovie)
{
	if (argv.argc() != 2)
	{
		Printf ("Usage: playmovie <movie name>\n");
		return;
	}
	I_PlayMovie (argv[1]);
}

//==========================================================================
//
// FInputDevice :: WndProcHook
//
// Gives subclasses a chance to intercept window messages. 
//
//==========================================================================

bool FInputDevice::WndProcHook(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, LRESULT *result)
{
	return false;
}

//==========================================================================
//
// FMouse - Constructor
//
//==========================================================================

FMouse::FMouse()
{
	LastX = LastY = 0;
	ButtonState = 0;
}

//==========================================================================
//
// FMouse :: PostMouseMove
//
// Posts a mouse movement event, potentially averaging it with the previous
// movement. If there is no movement to post, then no event is generated.
//
//==========================================================================

void FMouse::PostMouseMove(int x, int y)
{
	event_t ev = { 0 };

	if (m_filter)
	{
		ev.x = (x + LastX) / 2;
		ev.y = (y + LastY) / 2;
	}
	else
	{
		ev.x = x;
		ev.y = y;
	}
	LastX = x;
	LastY = y;
	if (ev.x | ev.y)
	{
		ev.type = EV_Mouse;
		D_PostEvent(&ev);
	}
}

//==========================================================================
//
// FMouse :: WheelMoved
//
// Generates events for a wheel move. Events are generated for every
// WHEEL_DELTA units that the wheel has moved. In normal mode, each move
// generates both a key down and a key up event. In GUI mode, only one
// event is generated for each unit of movement.
//
//==========================================================================

void FMouse::WheelMoved(int wheelmove)
{
	event_t ev = { 0 };
	int dir;

	if (GUICapture)
	{
		ev.type = EV_GUI_Event;
		if (wheelmove < 0)
		{
			dir = WHEEL_DELTA;
			ev.subtype = EV_GUI_WheelDown;
		}
		else
		{
			dir = -WHEEL_DELTA;
			ev.subtype = EV_GUI_WheelUp;
		}
		ev.data3 = ((KeyState[VK_SHIFT]&128) ? GKM_SHIFT : 0) |
				   ((KeyState[VK_CONTROL]&128) ? GKM_CTRL : 0) |
				   ((KeyState[VK_MENU]&128) ? GKM_ALT : 0);
		while (abs(wheelmove) >= WHEEL_DELTA)
		{
			D_PostEvent(&ev);
			wheelmove += dir;
		}
	}
	else
	{
		if (wheelmove < 0)
		{
			dir = WHEEL_DELTA;
			ev.data1 = KEY_MWHEELDOWN;
		}
		else
		{
			dir = -WHEEL_DELTA;
			ev.data1 = KEY_MWHEELUP;
		}
		while (abs(wheelmove) >= WHEEL_DELTA)
		{
			ev.type = EV_KeyDown;
			D_PostEvent(&ev);
			ev.type = EV_KeyUp;
			D_PostEvent(&ev);
			wheelmove += dir;
		}
	}
}

//==========================================================================
//
// FMouse :: PostButtonEvent
//
// Posts a mouse button up/down event. Down events are always posted. Up
// events will only be sent if the button is currently marked as down.
//
//==========================================================================

void FMouse::PostButtonEvent(int button, bool down)
{
	event_t ev = { 0 };
	int mask = 1 << button;

	ev.data1 = KEY_MOUSE1 + button;
	if (down)
	{
		ButtonState |= mask;
		ev.type = EV_KeyDown;
		D_PostEvent(&ev);
	}
	else if (ButtonState & mask)
	{
		ButtonState &= ~mask;
		ev.type = EV_KeyUp;
		D_PostEvent(&ev);
	}
}

//==========================================================================
//
// FMouse :: ClearButtonState
//
// Sends key up events for all buttons that are currently down and marks
// them as up. Used when focus is lost and we can no longer track up events,
// so get them marked up right away.
//
//==========================================================================

void FMouse::ClearButtonState()
{
	if (ButtonState != 0)
	{
		int i, mask;
		event_t ev = { 0 };

		ev.type = EV_KeyUp;
		for (i = sizeof(ButtonState) * 8, mask = 1; i > 0; --i, mask <<= 1)
		{
			if (ButtonState & mask)
			{
				ev.data1 = KEY_MOUSE1 + (int)sizeof(ButtonState) * 8 - i;
				D_PostEvent(&ev);
			}
		}
		ButtonState = 0;
	}
}

class FDInputMouse : public FMouse
{
public:
	FDInputMouse();
	~FDInputMouse();
	
	bool GetDevice();
	void ProcessInput();
	void Grab();
	void Ungrab();

protected:
	LPDIRECTINPUTDEVICE8 Device;
};

//==========================================================================
//
// FDInputMouse - Constructor
//
//==========================================================================

FDInputMouse::FDInputMouse()
{
	Device = NULL;
}

//==========================================================================
//
// FDInputMouse - Destructor
//
//==========================================================================

FDInputMouse::~FDInputMouse()
{
	if (Device != NULL)
	{
		Device->Release();
		Device = NULL;
	}
}

//==========================================================================
//
// FDInputMouse :: GetDevice
//
// Create the device interface and initialize it.
//
//==========================================================================

bool FDInputMouse::GetDevice()
{
	HRESULT hr;

	if (g_pdi3)
	{ // DirectInput3 interface
		hr = g_pdi3->CreateDevice(GUID_SysMouse, (LPDIRECTINPUTDEVICE*)&Device, NULL);
	}
	else
	{ // DirectInput8 interface
		hr = g_pdi->CreateDevice(GUID_SysMouse, &Device, NULL);
	}
	if (FAILED(hr))
	{
		return false;
	}
	
	// How many buttons does this mouse have?
	DIDEVCAPS_DX3 caps = { sizeof(caps) };
	hr = Device->GetCapabilities((DIDEVCAPS *)&caps);
	// If that failed, just assume four buttons.
	if (FAILED(hr))
	{
		caps.dwButtons = 4;
	}
	// Now select the data format with enough buttons for this mouse.
	// (Can we use c_dfDIMouse2 with DirectInput3? If so, then we could just set
	// that unconditionally.)
	hr = Device->SetDataFormat(caps.dwButtons <= 4 ? &c_dfDIMouse : &c_dfDIMouse2);
	if (FAILED(hr))
	{
ufailit:
		Device->Release();
		Device = NULL;
		return false;
	}
	// Set cooperative level.
	hr = Device->SetCooperativeLevel(Window, DISCL_EXCLUSIVE | DISCL_FOREGROUND);
	if (FAILED(hr))
	{
		goto ufailit;
	}
	// Set buffer size so we can use buffered input.
	DIPROPDWORD prop;
	prop.diph.dwSize = sizeof(prop);
	prop.diph.dwHeaderSize = sizeof(prop.diph);
	prop.diph.dwObj = 0;
	prop.diph.dwHow = DIPH_DEVICE;
	prop.dwData = DINPUT_BUFFERSIZE;
	hr = Device->SetProperty(DIPROP_BUFFERSIZE, &prop.diph);
	if (FAILED(hr))
	{
		goto ufailit;
	}
	return true;
}

//==========================================================================
//
// FDInputMouse :: ProcessInput
//
// Posts any events that have accumulated since the previous call.
//
//==========================================================================

void FDInputMouse::ProcessInput()
{
	DIDEVICEOBJECTDATA od;
	DWORD dwElements;
	HRESULT hr;
	int count = 0;
	int dx, dy;

	dx = 0;
	dy = 0;

	if (!HaveFocus || NativeMouse)
		return;

	event_t ev = { 0 };
	for (;;)
	{
		DWORD cbObjectData = g_pdi3 ? sizeof(DIDEVICEOBJECTDATA_DX3) : sizeof(DIDEVICEOBJECTDATA);
		dwElements = 1;
		hr = Device->GetDeviceData(cbObjectData, &od, &dwElements, 0);
		if (hr == DIERR_INPUTLOST || hr == DIERR_NOTACQUIRED)
		{
			Grab();
			hr = Device->GetDeviceData(cbObjectData, &od, &dwElements, 0);
		}

		/* Unable to read data or no data available */
		if (FAILED(hr) || !dwElements)
			break;

		count++;

		/* Look at the element to see what happened */
		// GCC does not like putting the DIMOFS_ macros in case statements,
		// so use ifs instead.
		if (od.dwOfs == (DWORD)DIMOFS_X)
		{
			dx += od.dwData;
		}
		else if (od.dwOfs == (DWORD)DIMOFS_Y)
		{
			dy += od.dwData;
		}
		else if (od.dwOfs == (DWORD)DIMOFS_Z)
		{
			WheelMoved(od.dwData);
		}
		else if (od.dwOfs >= (DWORD)DIMOFS_BUTTON0 && od.dwOfs <= (DWORD)DIMOFS_BUTTON7)
		{
			/* [RH] Mouse button events mimic keydown/up events */
			if (!GUICapture)
			{
				PostButtonEvent(od.dwOfs - DIMOFS_BUTTON0, !!(od.dwData & 0x80));
			}
		}
	}
	PostMouseMove(m_noprescale ? dx : dx<<2, -dy);
}

//==========================================================================
//
// FDInputMouse :: Grab
//
//==========================================================================

void FDInputMouse::Grab()
{
	Device->Acquire();
	SetCursorState(NativeMouse);
}

//==========================================================================
//
// FDInputMouse :: Ungrab
//
//==========================================================================

void FDInputMouse::Ungrab()
{
	Device->Unacquire();
	SetCursorState(true);
	ClearButtonState();
}

class FWin32Mouse : public FMouse
{
public:
	FWin32Mouse();
	~FWin32Mouse();
	
	bool GetDevice();
	void ProcessInput();
	bool WndProcHook(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, LRESULT *result);
	void Grab();
	void Ungrab();

protected:
	void CenterMouse(int x, int y);

	POINT UngrabbedPointerPos;
	LONG PrevX, PrevY;
	bool Grabbed;
};

//==========================================================================
//
// FWin32Mouse - Constructor
//
//==========================================================================

FWin32Mouse::FWin32Mouse()
{
	GetCursorPos(&UngrabbedPointerPos);
	Grabbed = false;
}

//==========================================================================
//
// FWin32Mouse - Destructor
//
//==========================================================================

FWin32Mouse::~FWin32Mouse()
{
	Ungrab();
}

//==========================================================================
//
// FWin32Mouse :: GetDevice
//
// The Win32 mouse is always available, since it is the lowest common
// denominator. (Even if it's not connected, it is still considered as
// "available"; it just won't generate any events.)
//
//==========================================================================

bool FWin32Mouse::GetDevice()
{
	return true;
}

//==========================================================================
//
// FWin32Mouse :: ProcessInput
//
// Get current mouse position and post events if the mouse has moved from
// last time.
//
//==========================================================================

void FWin32Mouse::ProcessInput()
{
	POINT pt;
	int x, y;

	if (!HaveFocus || !Grabbed || !GetCursorPos(&pt))
	{
		return;
	}

	x = pt.x - PrevX;
	y = PrevY - pt.y;

	if (!m_noprescale)
	{
		x *= 3;
		y *= 2;
	}
	if (x | y)
	{
		CenterMouse(pt.x, pt.y);
	}
	PostMouseMove(x, y);
}

//==========================================================================
//
// FWin32Mouse :: WndProcHook
//
// Intercepts mouse-related window messages if the mouse is grabbed.
//
//==========================================================================

bool FWin32Mouse::WndProcHook(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, LRESULT *result)
{
	if (!Grabbed)
	{
		return false;
	}

	if (message == WM_SIZE)
	{
		if (wParam == SIZE_MAXIMIZED || wParam == SIZE_RESTORED)
		{
			CenterMouse(-1, -1);
			*result = 0;
			return true;
		}
	}
	else if (message == WM_MOVE)
	{
		CenterMouse(-1, -1);
		*result = 0;
		return true;
	}
	else if (message == WM_SYSCOMMAND)
	{
		// Do not enter the window moving and sizing modes while grabbed,
		// because those require the mouse.
		wParam &= 0xFFF0;
		if (wParam == SC_MOVE || wParam == SC_SIZE)
		{
			*result = 0;
			return true;
		}
	}
	else if (message == WM_MOUSEWHEEL)
	{
		WheelMoved(HIWORD(wParam));
		return true;
	}
	else if (message >= WM_LBUTTONDOWN && message <= WM_MBUTTONUP)
	{
		int action = (message - WM_LBUTTONDOWN) % 3;
		int button = (message - WM_LBUTTONDOWN) / 3;

		if (action == 2)
		{ // double clicking we care not about.
			return false;
		}
		event_t ev = { 0 };
		ev.type = action ? EV_KeyUp : EV_KeyDown;
		ev.data1 = KEY_MOUSE1 + button;
		if (action)
		{
			ButtonState &= ~(1 << button);
		}
		else
		{
			ButtonState |= 1 << button;
		}
		D_PostEvent(&ev);
		return true;
	}
	else if (message >= WM_XBUTTONDOWN && message <= WM_XBUTTONUP)
	{
		// Microsoft's (lack of) documentation for the X buttons is unclear on whether
		// or not simultaneous pressing of multiple X buttons will ever be merged into
		// a single message. Winuser.h describes the button field as being filled with
		// flags, which suggests that it could merge them. My testing
		// indicates it does not, but I will assume it might in the future.
		WORD xbuttons = GET_XBUTTON_WPARAM(wParam);
		event_t ev = { 0 };

		ev.type = (message == WM_XBUTTONDOWN) ? EV_KeyDown : EV_KeyUp;

		// There are only two X buttons defined presently, so I extrapolate from
		// the current winuser.h values to support up to 8 mouse buttons.
		for (int i = 0; i < 5; ++i, xbuttons >>= 1)
		{
			if (xbuttons & 1)
			{
				ev.data1 = KEY_MOUSE4 + i;
				if (ev.type == EV_KeyDown)
				{
					ButtonState |= 1 << (i + 4);
				}
				else
				{
					ButtonState &= ~(1 << (i + 4));
				}
				D_PostEvent(&ev);
			}
		}
		*result = TRUE;
		return true;
	}
	return false;
}

//==========================================================================
//
// FWin32Mouse :: Grab
//
// Hides the mouse and locks it inside the window boundaries.
//
//==========================================================================

void FWin32Mouse::Grab()
{
	RECT rect;

	if (Grabbed)
	{
		return;
	}

	GetCursorPos(&UngrabbedPointerPos);
	ClipCursor(NULL);		// helps with Win95?
	GetClientRect(Window, &rect);

	// Reposition the rect so that it only covers the client area.
	ClientToScreen(Window, (LPPOINT)&rect.left);
	ClientToScreen(Window, (LPPOINT)&rect.right);

	ClipCursor(&rect);
	SetCursorState(false);
	CenterMouse(-1, -1);
	Grabbed = true;
}

//==========================================================================
//
// FWin32Mouse :: Ungrab
//
// Shows the mouse and lets it roam free.
//
//==========================================================================

void FWin32Mouse::Ungrab()
{
	if (!Grabbed)
	{
		return;
	}

	ClipCursor(NULL);
	SetCursorPos(UngrabbedPointerPos.x, UngrabbedPointerPos.y);
	SetCursorState(true);
	Grabbed = false;
	ClearButtonState();
}

//==========================================================================
//
// FWin32Mouse :: CenterMouse
//
// Moves the mouse to the center of the window, but only if the current
// position isn't already in the center.
//
//==========================================================================

void FWin32Mouse::CenterMouse(int curx, int cury)
{
	RECT rect;

	GetWindowRect (Window, &rect);

	int centx = (rect.left + rect.right) >> 1;
	int centy = (rect.top + rect.bottom) >> 1;

	// Reduce the number of WM_MOUSEMOVE messages that get sent
	// by only calling SetCursorPos when we really need to.
	if (centx != curx || centy != cury)
	{
		PrevX = centx;
		PrevY = centy;
		SetCursorPos (centx, centy);
	}
}

static void I_StartupMouse ()
{
	mousemode_t new_mousemode;

	if (in_mouse == 1 || (in_mouse == 0 && OSPlatform == os_WinNT4))
		new_mousemode = win32;
	else
		new_mousemode = dinput;

	if (new_mousemode != mousemode)
	{
		if (new_mousemode == dinput)
		{
			Mouse = new FDInputMouse();
		}
		else
		{
			Mouse = new FWin32Mouse();
		}
		if (!Mouse->GetDevice())
		{
			delete Mouse;
			if (new_mousemode == dinput)
			{
				new_mousemode = win32;
				Mouse = new FWin32Mouse();
				if (!Mouse->GetDevice())
				{
					Mouse = NULL;
					new_mousemode = none;
				}
			}
		}
		mousemode = new_mousemode;
	}
	HaveFocus = GetFocus() == Window;
}
