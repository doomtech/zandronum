#ifndef NO_GL

// HEADER FILES ------------------------------------------------------------

#include <iostream>

#include "doomtype.h"

#include "templates.h"
#include "i_system.h"
#include "i_video.h"
#include "v_video.h"
#include "v_pfx.h"
#include "stats.h"
#include "version.h"
#include "c_console.h"

#include "sdlglvideo.h"
#include "gl/gl_pch.h"
#include "r_defs.h"
#include "gl/gl_functions.h"
#include "gl/gl_struct.h"
#include "gl/gl_intern.h"
#include "gl/gl_basic.h"
#include "gl/gl_texture.h"
#include "gl/gl_shader.h"
#include "gl/gl_framebuffer.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

IMPLEMENT_ABSTRACT_CLASS(SDLGLFB)

struct MiniModeInfo
{
	WORD Width, Height;
};

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern IVideo *Video;
// extern int vid_renderer;

EXTERN_CVAR (Float, Gamma)
EXTERN_CVAR (Int, vid_displaybits)
EXTERN_CVAR (Int, vid_renderer)


// PUBLIC DATA DEFINITIONS -------------------------------------------------

CUSTOM_CVAR(Int, gl_vid_multisample, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL )
{
	Printf("This won't take effect until "GAMENAME" is restarted.\n");
}

RenderContext gl;

CVAR(Bool, gl_vid_allowsoftware, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// Dummy screen sizes to pass when windowed
static MiniModeInfo WinModes[] =
{
	{ 320, 200 },
	{ 320, 240 },
	{ 400, 225 },	// 16:9
	{ 400, 300 },
	{ 480, 270 },	// 16:9
	{ 480, 360 },
	{ 512, 288 },	// 16:9
	{ 512, 384 },
	{ 640, 360 },	// 16:9
	{ 640, 400 },
	{ 640, 480 },
	{ 720, 480 },	// 16:10
	{ 720, 540 },
	{ 800, 450 },	// 16:9
	{ 800, 500 },	// 16:10
	{ 800, 600 },
	{ 848, 480 },	// 16:9
	{ 960, 600 },	// 16:10
	{ 960, 720 },
	{ 1024, 576 },	// 16:9
	{ 1024, 640 },	// 16:10
	{ 1024, 768 },
	{ 1088, 612 },	// 16:9
	{ 1152, 648 },	// 16:9
	{ 1152, 720 },	// 16:10
	{ 1152, 864 },
	{ 1280, 720 },	// 16:9
	{ 1280, 800 },	// 16:10
	{ 1280, 960 },
	{ 1360, 768 },	// 16:9
	{ 1400, 787 },	// 16:9
	{ 1400, 875 },	// 16:10
	{ 1440, 900 },
	{ 1400, 1050 },
	{ 1600, 900 },	// 16:9
	{ 1600, 1000 },	// 16:10
	{ 1600, 1200 },
	{ 1680, 1050 },
	{ 1920, 1080 },
	{ 1920, 1200 },
	{ 2054, 1536 }
};

// CODE --------------------------------------------------------------------

SDLGLVideo::SDLGLVideo (int parm)
{
	IteratorBits = 0;
	IteratorFS = false;
    if( SDL_Init( SDL_INIT_VIDEO ) < 0 ) {
        fprintf( stderr, "Video initialization failed: %s\n",
             SDL_GetError( ) );
    }
	GetContext(gl);
}

SDLGLVideo::~SDLGLVideo ()
{
	FGLTexture::FlushAll();
	SDL_Quit( );
}

void SDLGLVideo::StartModeIterator (int bits, bool fs)
{
	IteratorMode = 0;
	IteratorBits = bits;
	IteratorFS = fs;
}

bool SDLGLVideo::NextMode (int *width, int *height, bool *letterbox)
{
	if (IteratorBits != 8)
		return false;
	
	if (!IteratorFS)
	{
		if ((unsigned)IteratorMode < sizeof(WinModes)/sizeof(WinModes[0]))
		{
			*width = WinModes[IteratorMode].Width;
			*height = WinModes[IteratorMode].Height;
			++IteratorMode;
			return true;
		}
	}
	else
	{
		SDL_Rect **modes = SDL_ListModes (NULL, SDL_FULLSCREEN|SDL_HWSURFACE);
		if (modes != NULL && modes[IteratorMode] != NULL)
		{
			*width = modes[IteratorMode]->w;
			*height = modes[IteratorMode]->h;
			++IteratorMode;
			return true;
		}
	}
	return false;
}

DFrameBuffer *SDLGLVideo::CreateFrameBuffer (int width, int height, bool fullscreen, DFrameBuffer *old)
{
	static int retry = 0;
	static int owidth, oheight;
	
	PalEntry flashColor;
//	int flashAmount;

	if (old != NULL)
	{ // Reuse the old framebuffer if its attributes are the same
		SDLGLFB *fb = static_cast<SDLGLFB *> (old);
		if (fb->Width == width &&
			fb->Height == height)
		{
			bool fsnow = (fb->Screen->flags & SDL_FULLSCREEN) != 0;
	
			if (fsnow != fullscreen)
			{
				SDL_WM_ToggleFullScreen (fb->Screen);
			}
			return old;
		}
//		old->GetFlash (flashColor, flashAmount);
		delete old;
	}
	else
	{
		flashColor = 0;
//		flashAmount = 0;
	}
	
	SDLGLFB *fb = new OpenGLFrameBuffer (width, height, 32, 60, fullscreen);
	retry = 0;
	
	// If we could not create the framebuffer, try again with slightly
	// different parameters in this order:
	// 1. Try with the closest size
	// 2. Try in the opposite screen mode with the original size
	// 3. Try in the opposite screen mode with the closest size
	// This is a somewhat confusing mass of recursion here.

	while (fb == NULL || !fb->IsValid ())
	{
		if (fb != NULL)
		{
			delete fb;
		}

		switch (retry)
		{
		case 0:
			owidth = width;
			oheight = height;
		case 2:
			// Try a different resolution. Hopefully that will work.
			I_ClosestResolution (&width, &height, 8);
			break;

		case 1:
			// Try changing fullscreen mode. Maybe that will work.
			width = owidth;
			height = oheight;
			fullscreen = !fullscreen;
			break;

		default:
			// I give up!
			I_FatalError ("Could not create new screen (%d x %d)", owidth, oheight);

			fprintf( stderr, "!!! [SDLGLVideo::CreateFrameBuffer] Got beyond I_FatalError !!!" );
			return NULL;	//[C] actually this shouldn't be reached; probably should be replaced with an ASSERT
		}

		++retry;
		fb = static_cast<SDLGLFB *>(CreateFrameBuffer (width, height, fullscreen, NULL));
	}

//	fb->SetFlash (flashColor, flashAmount);
	return fb;
}

void SDLGLVideo::SetWindowedScale (float scale)
{
}

bool SDLGLVideo::SetResolution (int width, int height, int bits)
{
	// FIXME: Is it possible to do this without completely destroying the old
	// interface?
#ifndef NO_GL
	
	FGLTexture::FlushAll();
	I_ShutdownGraphics();

	Video = new SDLGLVideo(0);
	if (Video == NULL) I_FatalError ("Failed to initialize display");

#if (defined(WINDOWS)) || defined(WIN32)
	bits=32;
#else
	bits=24;
#endif
	
	V_DoModeSetup(width, height, bits);
#endif
	return true;	// We must return true because the old video context no longer exists.
}

// FrameBuffer implementation -----------------------------------------------

SDLGLFB::SDLGLFB (int width, int height, int, int, bool fullscreen)
	: DFrameBuffer (width, height)
{
	static int localmultisample=-1;

	if (localmultisample<0) localmultisample=gl_vid_multisample;

	int i;
	
	m_Lock=0;

	UpdatePending = false;
	
	if (!gl.InitHardware(gl_vid_allowsoftware, gl_vid_compatibility, localmultisample))
	{
		vid_renderer = 0;
		return;
	}

	Screen = SDL_SetVideoMode (width, height, vid_displaybits,
		SDL_HWSURFACE|SDL_HWPALETTE|SDL_OPENGL | SDL_GL_DOUBLEBUFFER|SDL_ANYFORMAT|
		(fullscreen ? SDL_FULLSCREEN : 0));

	if (Screen == NULL)
		return;

	m_supportsGamma = gl.GetGammaRamp(m_origGamma[0], m_origGamma[1], m_origGamma[2]);
}

SDLGLFB::~SDLGLFB ()
{
	if (m_supportsGamma) 
	{
		gl.SetGammaRamp(m_origGamma[0], m_origGamma[1], m_origGamma[2]);
	}
}

void SDLGLFB::InitializeState() 
{
	int value = 0;
	SDL_GL_GetAttribute( SDL_GL_STENCIL_SIZE, &value );
	if (!value) 
	{
		Printf("Failed to use stencil buffer!\n");	//[C] is it needed to recreate buffer in "cheapest mode"?
		gl.flags|=RFL_NOSTENCIL;
	}
}

bool SDLGLFB::CanUpdate ()
{
	if (m_Lock != 1)
	{
		if (m_Lock > 0)
		{
			UpdatePending = true;
			--m_Lock;
		}
		return false;
	}
	return true;
}

void SDLGLFB::SetGammaTable(WORD *tbl)
{
	gl.SetGammaRamp(&tbl[0], &tbl[256], &tbl[512]);
}

bool SDLGLFB::Lock(bool buffered)
{
	m_Lock++;
	Buffer = MemBuffer;
	return true;
}

bool SDLGLFB::Lock () 
{ 	
	return Lock(false); 
}

void SDLGLFB::Unlock () 	
{ 
	if (UpdatePending && m_Lock == 1)
	{
		Update ();
	}
	else if (--m_Lock <= 0)
	{
		m_Lock = 0;
	}
}

bool SDLGLFB::IsLocked () 
{ 
	return m_Lock>0;// true;
}

bool SDLGLFB::IsFullscreen ()
{
	return (Screen->flags & SDL_FULLSCREEN) != 0;
}


bool SDLGLFB::IsValid ()
{
	return DFrameBuffer::IsValid() && Screen != NULL;
}

void SDLGLFB::NewRefreshRate ()
{
}


#endif
