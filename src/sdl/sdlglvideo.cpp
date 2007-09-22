
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
#include "gl/gl_functions.h"
#include "gl/gl_struct.h"
#include "gl/gl_intern.h"
#include "gl/gl_basic.h"
#include "gl/gl_shader.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

IMPLEMENT_CLASS(SDLGLFB)

struct MiniModeInfo
{
	WORD Width, Height;
};

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern IVideo *Video;

EXTERN_CVAR (Float, Gamma)
EXTERN_CVAR (Int, vid_displaybits)
EXTERN_CVAR(Int, vid_renderer);


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
	{ 1400, 1050 },
	{ 1600, 900 },	// 16:9
	{ 1600, 1000 },	// 16:10
	{ 1600, 1200 },
};

// CODE --------------------------------------------------------------------

void DoPrintText(const char * out)
{
	PrintString(PRINT_HIGH, out);
}

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
//	FGLTexture::FlushAll();
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
	
	SDLGLFB *fb = new SDLGLFB (width, height, fullscreen);
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
		}

		++retry;
		fb = static_cast<SDLGLFB *>(CreateFrameBuffer (width, height, fullscreen, NULL));
	}

//	fb->SetFlash (flashColor, flashAmount);
	m_trueHeight = fb->Height;
	return fb;
}

void SDLGLVideo::SetWindowedScale (float scale)
{
}

// FrameBuffer implementation -----------------------------------------------

SDLGLFB::SDLGLFB (int width, int height, bool fullscreen)
	: DFrameBuffer (width, height)
{
	static int localmultisample=-1;

	if (localmultisample<0) localmultisample=gl_vid_multisample;

	int i;
	
	m_Lock=0;

	UpdatePending = false;
	
	if (!gl.InitHardware(gl_vid_allowsoftware, gl_vid_compatibility, localmultisample, DoPrintText))
	{
		vid_renderer = 0;
		return;
	}

	Screen = SDL_SetVideoMode (width, height, vid_displaybits,
		SDL_HWSURFACE|SDL_HWPALETTE|SDL_OPENGL | SDL_GL_DOUBLEBUFFER|SDL_ANYFORMAT|
		(fullscreen ? SDL_FULLSCREEN : 0));

	if (Screen == NULL)
		return;


/*	for (i = 0; i < 256; i++)
	{
		GammaTable[0][i] = GammaTable[1][i] = GammaTable[2][i] = i;
	}
	if (Screen->format->palette == NULL)
	{
		NotPaletted = true;
		GPfx.SetFormat (Screen->format->BitsPerPixel,
			Screen->format->Rmask,
			Screen->format->Gmask,
			Screen->format->Bmask);
	}*/
	memcpy (SourcePalette, GPalette.BaseColors, sizeof(PalEntry)*256);
	UpdatePalette ();

	m_supportsGamma = gl.GetGammaRamp(m_origGamma[0], m_origGamma[1], m_origGamma[2]);
	printf("m_supportsGamma: %i", m_supportsGamma);
	DoSetGamma();

	gl.LoadExtensions();
	gl.PrintStartupLog(DoPrintText);

	int value = 0;
	SDL_GL_GetAttribute( SDL_GL_STENCIL_SIZE, &value );
	if (!value) {
		Printf("Failed to use stencil buffer!\n");	//[C] is it needed to retry without stencil?
		gl.flags|=RFL_NOSTENCIL;
	}

	if (gl.flags&RFL_NPOT_TEXTURE)
	{
		Printf("Support for non power 2 textures enabled.\n");
	}
	if (gl.flags&RFL_OCCLUSION_QUERY)
	{
		Printf("Occlusion query enabled.\n");
	}

	gl.ClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	gl.ClearDepth(1.0f);
	gl.DepthFunc(GL_LESS);
	gl.ShadeModel(GL_SMOOTH);

	gl.Enable(GL_DITHER);
	gl.Enable(GL_ALPHA_TEST);
	gl.Disable(GL_CULL_FACE);
	gl.Disable(GL_POLYGON_OFFSET_FILL);
	gl.Enable(GL_POLYGON_OFFSET_LINE);
	gl.Enable(GL_BLEND);
	gl.Enable(GL_DEPTH_CLAMP_NV);
	gl.Disable(GL_DEPTH_TEST);
	gl.Enable(GL_TEXTURE_2D);
	gl.Disable(GL_LINE_SMOOTH);
	gl.AlphaFunc(GL_GEQUAL,0.5f);
	gl.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	gl.Hint(GL_LINE_SMOOTH_HINT, GL_NICEST);
	gl.Hint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
	gl.Hint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	gl.Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

/*	
	gl.MatrixMode(GL_PROJECTION);
	gl.LoadIdentity();
	gl.Ortho(0.0, GetWidth() * 1.0, 0.0, GetHeight() * 1.0, -1.0, 1.0);

	gl.MatrixMode(GL_MODELVIEW);
	gl.LoadIdentity();
//	GL::SetPerspective(90.f, GetWidth() * 1.f / GetHeight(), 0.f, 1000.f);
*/	

	gl.Viewport(0, 0/*(GetTrueHeight()-GetHeight())/2*/, GetWidth(), GetHeight()); 

	GLShader::Initialize();
	gl_InitFog();
	Set2DMode();

	if (gl_vertices.Size())
	{
		gl.ArrayPointer(&gl_vertices[0], sizeof(GLVertex));
	}
}

SDLGLFB::~SDLGLFB ()
{
//	I_SaveWindowedPos();
	GLShader::Clear();
	if (m_supportsGamma) 
	{
		gl.SetGammaRamp(m_origGamma[0], m_origGamma[1], m_origGamma[2]);
	}
//	gl.Shutdown();
}

bool SDLGLFB::IsValid ()
{
	return DFrameBuffer::IsValid() && Screen != NULL;
}

int SDLGLFB::GetPageCount ()
{
	return 1;
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

void SDLGLFB::Update ()
{
	if (m_Lock != 1)
	{
		if (m_Lock > 0)
		{
			UpdatePending = true;
			--m_Lock;
		}
		return;
	}

	Set2DMode();

	DrawRateStuff ();

/*	if (GetTrueHeight() != GetHeight())
	{
		// Letterbox time! Draw black top and bottom borders.
		int borderHeight = (GetTrueHeight() - GetHeight()) / 2;

		gl.Viewport(0, 0, GetWidth(), GetTrueHeight());
		gl.MatrixMode(GL_PROJECTION);
		gl.LoadIdentity();
		gl.Ortho(0.0, GetWidth() * 1.0, 0.0, GetTrueHeight(), -1.0, 1.0);
		gl.MatrixMode(GL_MODELVIEW);
		gl.Color3f(0.f, 0.f, 0.f);
		gl_EnableTexture(false);

		gl.Begin(GL_QUADS);
		// upper quad
		gl.Vertex2i(0, borderHeight);
		gl.Vertex2i(0, 0);
		gl.Vertex2i(GetWidth(), 0);
		gl.Vertex2i(GetWidth(), borderHeight);
		gl.End();

		gl.Begin(GL_QUADS);
		// lower quad
		gl.Vertex2i(0, GetTrueHeight());
		gl.Vertex2i(0, GetTrueHeight() - borderHeight);
		gl.Vertex2i(GetWidth(), GetTrueHeight() - borderHeight);
		gl.Vertex2i(GetWidth(), GetTrueHeight());
		gl.End();

		gl_EnableTexture(true);

		Set2DMode();
		gl.Viewport(0, (GetTrueHeight() - GetHeight()) / 2, GetWidth(), GetHeight()); 

	}*/

	Finish.Reset();
	Finish.Start();
	gl.Finish();
	Finish.Stop();
	gl.SwapBuffers();
	Unlock();
}

PalEntry *SDLGLFB::GetPalette ()
{
	return SourcePalette;
}

void SDLGLFB::UpdatePalette ()
{
	int rr=0,gg=0,bb=0;
	for(int x=0;x<256;x++)
	{
		rr+=GPalette.BaseColors[x].r;
		gg+=GPalette.BaseColors[x].g;
		bb+=GPalette.BaseColors[x].b;
	}
	rr>>=8;
	gg>>=8;
	bb>>=8;

	palette_brightness = (rr*77 + gg*143 + bb*35)/255;
}

void SDLGLFB::DoSetGamma()
{
	Uint16 gammaTable[3][256];

	if (m_supportsGamma)
	{
		// This formula is taken from Doomsday
		float gamma = clamp<float>(Gamma, 0.1f, 4.f);
		float contrast = clamp<float>(vid_contrast, 0.1f, 3.f);
		float bright = clamp<float>(vid_brightness, -0.8f, 0.8f);

		double invgamma = 1 / gamma;
		double norm = pow(255., invgamma - 1);

		for (int i = 0; i < 256; i++)
		{
			double val = i * contrast - (contrast - 1) * 127;
			if(gamma != 1) val = pow(val, invgamma) / norm;
			val += bright * 128;

			gammaTable[0][i] = gammaTable[1][i] = gammaTable[2][i] = (WORD)clamp<double>(val*256, 0, 0xffff);
		}
		gl.SetGammaRamp(gammaTable[0], gammaTable[1], gammaTable[2]);
	}
}
bool SDLGLFB::SetGamma (float gamma)
{
	DoSetGamma();
	return true;
}

bool SDLGLFB::SetBrightness(float bright)
{
	DoSetGamma();
	return true;
}

bool SDLGLFB::SetContrast(float contrast)
{
	DoSetGamma();
	return true;
}

bool SDLGLFB::SetFlash (PalEntry rgb, int amount)
{
	return true;
}

void SDLGLFB::GetFlash (PalEntry &rgb, int &amount)
{
}

void SDLGLFB::GetFlashedPalette (PalEntry pal[256])
{
	memcpy(pal, SourcePalette, 256*sizeof(PalEntry));
}

bool SDLGLFB::IsFullscreen ()
{
	return (Screen->flags & SDL_FULLSCREEN) != 0;
}

void SDLGLFB::Set2DMode()
{
	gl.MatrixMode(GL_MODELVIEW);
	gl.LoadIdentity();
	gl.MatrixMode(GL_PROJECTION);
	gl.LoadIdentity();
	gl.Ortho(
		(GLdouble) 0,
		(GLdouble) GetWidth(), 
		(GLdouble) GetHeight(), 
		(GLdouble) 0,
		(GLdouble) -1.0, 
		(GLdouble) 1.0 
		);
	gl.Disable(GL_DEPTH_TEST);
	gl.Disable(GL_MULTISAMPLE);
	gl_EnableFog(false);
	GLShader::Unbind();
}

void gl_ClearScreen()
{
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	static_cast<SDLGLFB*>(screen)->Set2DMode();
	screen->Dim(0, 1.f, 0, 0, SCREENWIDTH, SCREENHEIGHT);
	glEnable(GL_DEPTH_TEST);
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
}
