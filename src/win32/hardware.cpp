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

EXTERN_CVAR (Bool, ticker)
EXTERN_CVAR (Bool, fullscreen)
EXTERN_CVAR (Float, vid_winscale)


#include "gl/win32gliface.h"
#include "gl/gl_texture.h"

bool ForceWindowed;

IVideo *Video;
//static IKeyboard *Keyboard;
//static IMouse *Mouse;
//static IJoystick *Joystick;


extern int NewWidth, NewHeight, NewBits, DisplayBits;
bool V_DoModeSetup (int width, int height, int bits);
void I_RestartRenderer();
void RebuildAllLights();
int currentrenderer=1;
bool changerenderer;
bool gl_disabled;
EXTERN_CVAR(Bool, gl_nogl)

// [ZDoomGL]
CUSTOM_CVAR (Int, vid_renderer, 1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	// 0: Software renderer
	// 1: OpenGL renderer

	if (gl_disabled) 
	{
		return;
	}

	if (self != currentrenderer)
	{
		switch (self)
		{
		case 0:
			Printf("Switching to software renderer...\n");
			break;
		case 1:
			Printf("Switching to OpenGL renderer...\n");
			break;
		default:
			Printf("Unknown renderer (%d).  Falling back to software renderer...\n", int(vid_renderer));
			self = 0; // make sure to actually switch to the software renderer
			break;
		}
		//changerenderer = true;
		Printf("You must restart GZDoom to switch the rendere\n");
	}
}

CCMD (vid_restart)
{
	if (!gl_disabled) changerenderer = true;
}

void I_CheckRestartRenderer()
{
	if (gl_disabled) return;
	
	while (changerenderer)
	{
		currentrenderer = vid_renderer;
		I_RestartRenderer();
		if (currentrenderer == vid_renderer) changerenderer = false;
	}
}

void I_RestartRenderer()
{
	FFont *font;
	int bits;
	
	FGLTexture::FlushAll();
	font = screen->Font;
	I_ShutdownGraphics();
	RebuildAllLights();	// Build the lightmaps for all colormaps. If the hardware renderer is active 
						// this time consuming step is skipped.
	
	changerenderer=false;
	if (gl_disabled) currentrenderer=0;
	if (currentrenderer==1) Video = new Win32GLVideo(0);
	else Video = new Win32Video (0);
	if (Video == NULL) I_FatalError ("Failed to initialize display");
	
	if (currentrenderer==0) bits=8;
	else bits=32;
	
	V_DoModeSetup(NewWidth, NewHeight, bits);
	screen->SetFont(font);
}

void I_ShutdownGraphics ()
{
	if (screen)
		delete screen, screen = NULL;
	if (Video)
		delete Video, Video = NULL;
}

void I_InitGraphics ()
{
	UCVarValue val;

	gl_disabled = gl_nogl;
	val.Bool = !!Args.CheckParm ("-devparm");
	ticker.SetGenericRepDefault (val, CVAR_Bool);

	if (gl_disabled) currentrenderer=0;
	else currentrenderer = vid_renderer;
	if (currentrenderer==1) Video = new Win32GLVideo(0);
	else Video = new Win32Video (0);

	if (Video == NULL)
		I_FatalError ("Failed to initialize display");
	
	atterm (I_ShutdownGraphics);
	
	Video->SetWindowedScale (vid_winscale);
}

/** Remaining code is common to Win32 and Linux **/

// VIDEO WRAPPERS ---------------------------------------------------------

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

	//* Right now, CreateFrameBuffer cannot return NULL
	if (res == NULL)
	{
		I_FatalError ("Mode %dx%d is unavailable\n", width, height);
	}
	//*/
	return res;
}

bool I_CheckResolution (int width, int height, int bits)
{
	int twidth, theight;

	Video->StartModeIterator (bits, screen ? screen->IsFullscreen() : fullscreen);
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

	for (iteration = 0; iteration < 2; iteration++)
	{
		Video->StartModeIterator (bits, screen ? screen->IsFullscreen() : fullscreen);
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

extern int NewWidth, NewHeight, NewBits, DisplayBits;

CUSTOM_CVAR (Bool, fullscreen, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG|CVAR_NOINITCALL)
{
	NewWidth = screen->GetWidth();
	NewHeight = screen->GetHeight();
	NewBits = DisplayBits;
	setmodeneeded = true;
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
		//setmodeneeded = true;	// This CVAR doesn't do anything and only causes problems!
	}
}

CCMD (vid_listmodes)
{
	static const char *ratios[5] = { "", " - 16:9", " - 16:10", "", " - 5:4" };
	int width, height, bits;
	bool letterbox;

	if (Video == NULL)
	{
		return;
	}

	for (bits = 1; bits <= 32; bits++)
	{
		Video->StartModeIterator (bits, screen->IsFullscreen());
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

