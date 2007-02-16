/*
** hardware.cpp
** Somewhat OS-independant interface to the screen, mouse, keyboard, and stick
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

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define USE_WINDOWS_DWORD
#include "hardware.h"
#include "win32iface.h"
#include "i_video.h"
#include "i_system.h"
#include "c_console.h"
#include "c_cvars.h"
#include "c_dispatch.h"
#include "v_text.h"
#include "network.h"

// [ZDoomGL]
#include "OpenGLVideo.h"
#include "gl_main.h"

EXTERN_CVAR (Bool, ticker)
EXTERN_CVAR (Bool, fullscreen)
EXTERN_CVAR (Float, vid_winscale)

bool ForceWindowed;

IVideo *Video;
//static IKeyboard *Keyboard;
//static IMouse *Mouse;
//static IJoystick *Joystick;

// [ZDoomGL]
static bool initialized = false;
extern int NewWidth, NewHeight, NewBits, DisplayBits;
extern BOOL vidactive;
extern bool UsingGLNodes;
bool V_DoModeSetup (int width, int height, int bits);
bool changerenderer;

void I_RestartRenderer()
{
   FFont *font;

   font = screen->Font;
   I_ShutdownHardware();
   I_InitHardware();
   V_DoModeSetup(NewWidth, NewHeight, 8);
   screen->SetFont(font);

   if (( OPENGL_GetCurrentRenderer( ) == RENDERER_OPENGL ) && gamestate == GS_LEVEL)
   {
	   if ( UsingGLNodes == false )
		   P_LoadGLNodes( Wads.GetNumForName( level.mapname ));

	   GL_GenerateLevelGeometry();
   }

   if ( OPENGL_GetCurrentRenderer( ) == RENDERER_SOFTWARE )
   {
      C_GetConback(NewWidth, NewHeight);
   }
}

void I_ShutdownHardware ()
{
	if (screen)
		delete screen, screen = NULL;
	if (Video)
		delete Video, Video = NULL;
}

void I_InitHardware ()
{
	UCVarValue val;

	val.Bool = !!Args.CheckParm ("-devparm");
	ticker.SetGenericRepDefault (val, CVAR_Bool);
	
	if ( OPENGL_GetCurrentRenderer( ) == RENDERER_OPENGL )
		Video = new OpenGLVideo( );
	else
		Video = new Win32Video (0);

	if (Video == NULL)
		I_FatalError ("Failed to initialize display");

	if (!initialized) // [ZDoomGL]
   {
	   atterm (I_ShutdownHardware);
   }

	Video->SetWindowedScale (vid_winscale);

   initialized = true; // [ZDoomGL]
}

/** Remaining code is common to Win32 and Linux **/

// VIDEO WRAPPERS ---------------------------------------------------------

EDisplayType I_DisplayType ()
{
	return Video->GetDisplayType ();
}

DFrameBuffer *I_SetMode (int &width, int &height, DFrameBuffer *old)
{
	bool fs = false;
	switch (Video->GetDisplayType ())
	{
	case DISPLAY_WindowOnly:
		fs = false;
		break;
	case DISPLAY_FullscreenOnly:
		fs = true;
		break;
	case DISPLAY_Both:
		if (ForceWindowed)
		{
			fs = false;
		}
		else
		{
			fs = fullscreen;
		}
		break;
	}
	DFrameBuffer *res = Video->CreateFrameBuffer (width, height, fs, old);

	/* Right now, CreateFrameBuffer cannot return NULL
	if (res == NULL)
	{
		I_FatalError ("Mode %dx%d is unavailable\n", width, height);
	}
	*/
	return res;
}

bool I_CheckResolution (int width, int height, int bits)
{
	int twidth, theight;

	Video->FullscreenChanged (screen ? screen->IsFullscreen() : fullscreen);
	Video->StartModeIterator (bits);
	while (Video->NextMode (&twidth, &theight, NULL))
	{
		if (width == twidth && height == theight)
			return true;
	}
	return false;
}

void I_ClosestResolution (int *width, int *height, int bits)
{
	int twidth, theight;
	int cwidth = 0, cheight = 0;
	int iteration;
	DWORD closest = 4294967295u;

	Video->FullscreenChanged (screen ? screen->IsFullscreen() : fullscreen);
	for (iteration = 0; iteration < 2; iteration++)
	{
		Video->StartModeIterator (bits);
		while (Video->NextMode (&twidth, &theight, NULL))
		{
			if (twidth == *width && theight == *height)
				return;

			if (iteration == 0 && (twidth < *width || theight < *height))
				continue;

			DWORD dist = (twidth - *width) * (twidth - *width)
				+ (theight - *height) * (theight - *height);

			if (dist < closest)
			{
				closest = dist;
				cwidth = twidth;
				cheight = theight;
			}
		}
		if (closest != 4294967295u)
		{
			*width = cwidth;
			*height = cheight;
			return;
		}
	}
}	

void I_StartModeIterator (int bits)
{
	Video->StartModeIterator (bits);
}

bool I_NextMode (int *width, int *height, bool *letterbox)
{
	return Video->NextMode (width, height, letterbox);
}

DCanvas *I_NewStaticCanvas (int width, int height)
{
	return new DSimpleCanvas (width, height);
}

extern int NewWidth, NewHeight, NewBits, DisplayBits;

CUSTOM_CVAR (Bool, fullscreen, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		return;

	if (Video->FullscreenChanged (self))
	{
		NewWidth = screen->GetWidth();
		NewHeight = screen->GetHeight();
		NewBits = DisplayBits;
		setmodeneeded = true;
	}
}

CUSTOM_CVAR (Float, vid_winscale, 1.f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (self < 1.f)
	{
		self = 1.f;
	}
	else if (Video)
	{
		Video->SetWindowedScale (self);
		NewWidth = screen->GetWidth();
		NewHeight = screen->GetHeight();
		NewBits = DisplayBits;
		setmodeneeded = true;
	}
}

CCMD (vid_listmodes)
{
	static const char *ratios[5] = { "", " - 16:9", " - 16:10", "", " - 5:4" };
	int width, height, bits;
	bool letterbox;

	for (bits = 1; bits <= 32; bits++)
	{
		Video->StartModeIterator (bits);
		while (Video->NextMode (&width, &height, &letterbox))
		{
			bool thisMode = (width == DisplayWidth && height == DisplayHeight && bits == DisplayBits);
			int ratio = CheckRatio (width, height);
			Printf (thisMode ? PRINT_BOLD : PRINT_HIGH,
				"%s%4d x%5d x%3d%s%s\n",
				thisMode || !(ratio & 3) ? "" : TEXTCOLOR_GOLD,
				width, height, bits,
				ratios[ratio],
				thisMode || !letterbox ? "" : TEXTCOLOR_BROWN " LB"
				);
		}
	}
}

CCMD (vid_currentmode)
{
	Printf ("%dx%dx%d\n", DisplayWidth, DisplayHeight, DisplayBits);
}

CUSTOM_CVAR( Int, vid_renderer, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL )
{
	if ( self != OPENGL_GetCurrentRenderer( ))
	{
		switch (self)
		{
		case RENDERER_SOFTWARE:
        
			Printf( "Switching to software renderer...\n" );
			break;
		case RENDERER_OPENGL:
			
			Printf( "Switching to OpenGL renderer...\n" );
			break;
		default:
			
			Printf( "Unknown renderer: %d! Falling back to software renderer...\n", vid_renderer );
			
			// Set this back to the software renderer.
			self = RENDERER_SOFTWARE;
			break;
		}

		// This can be different if we were in software mode, and tried to change to an invalid
		// renderer.
		if ( self != OPENGL_GetCurrentRenderer( ))
			changerenderer = true;
   }
}

CCMD (vid_restart)
{
	changerenderer = true;
}
