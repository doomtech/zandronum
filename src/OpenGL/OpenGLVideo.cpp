//-----------------------------------------------------------------------------
//
// Skulltag Source
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
// Date created:  10/22/03
//
//
// Filename: OpenGLVideo.cpp
//
// Description:
//
//-----------------------------------------------------------------------------

#define USE_WINDOWS_DWORD
#include "OpenGLVideo.h"
#include "doomtype.h"
#include "templates.h"
#include "i_input.h"
#include "i_system.h"
#include "i_video.h"
#include "v_video.h"
#include "v_pfx.h"
#include "stats.h"

#include "resource.h"

#include "gl_main.h"
#include "glext.h"
#include "wglext.h"


#define __BYTEBOOL__


// swap control
PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT = NULL;
PFNWGLGETSWAPINTERVALEXTPROC wglGetSwapIntervalEXT = NULL;
PFNGLLOCKARRAYSEXTPROC glLockArraysEXT = NULL;
PFNGLUNLOCKARRAYSEXTPROC glUnlockArraysEXT = NULL;
unsigned long currentRefresh, lastRefresh;

// multitexturing
PFNGLMULTITEXCOORD1FARBPROC glMultiTexCoord1fARB = NULL;
PFNGLMULTITEXCOORD2FARBPROC glMultiTexCoord2fARB = NULL;
PFNGLMULTITEXCOORD3FARBPROC glMultiTexCoord3fARB = NULL;
PFNGLMULTITEXCOORD4FARBPROC glMultiTexCoord4fARB = NULL;
PFNGLACTIVETEXTUREARBPROC glActiveTextureARB = NULL;
PFNGLCLIENTACTIVETEXTUREARBPROC glClientActiveTextureARB = NULL;
bool useMultitexture = false;

// vertex buffer objects
PFNGLGENBUFFERSARBPROC glGenBuffersARB = NULL;
PFNGLBINDBUFFERARBPROC glBindBufferARB = NULL;
PFNGLBUFFERDATAARBPROC glBufferDataARB = NULL;
PFNGLDELETEBUFFERSARBPROC glDeleteBuffersARB = NULL;

// vertex programs
PFNGLGENPROGRAMSARBPROC glGenProgramsARB = NULL;
PFNGLDELETEPROGRAMSARBPROC glDeleteProgramsARB = NULL;
PFNGLBINDPROGRAMARBPROC glBindProgramARB = NULL;
PFNGLPROGRAMSTRINGARBPROC glProgramStringARB = NULL;
PFNGLPROGRAMENVPARAMETER4FARBPROC glProgramEnvParameter4fARB = NULL;
PFNGLPROGRAMLOCALPARAMETER4FARBPROC glProgramLocalParameter4fARB = NULL;

// subtractive blending
PFNGLBLENDEQUATIONEXTPROC glBlendEquationEXT = NULL;

// point sprites
PFNGLPOINTPARAMETERFARBPROC glPointParameterfARB;
PFNGLPOINTPARAMETERFVARBPROC glPointParameterfvARB;


extern HWND Window;
extern HINSTANCE hInstance;
extern HCURSOR TheArrowCursor, TheInvisibleCursor;
extern IVideo *Video;
extern BOOL AppActive;
extern bool FullscreenReset;
extern bool VidResizing;
extern bool changerenderer;
extern int NewWidth, NewHeight, NewBits;
extern GLint viewport[4];

static bool FlipFlags;
void I_RestartRenderer();

CVAR (Bool, gl_show_frameinfo, false, 0)
CVAR (Bool, gl_vid_allowsoftware, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR (Bool, gl_vid_hardwaregamma, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

CUSTOM_CVAR (Bool, gl_use_vertex_buffer_objects, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
   I_RestartRenderer();
}

EXTERN_CVAR (Int, gl_vid_bitdepth)
EXTERN_CVAR (Float, dimamount)
EXTERN_CVAR (Color, dimcolor)
EXTERN_CVAR (Bool, fullscreen)
EXTERN_CVAR (Bool, vid_fps)
EXTERN_CVAR (Int, vid_renderer)


void GL_EnableVsync(bool enable)
{
   if (wglSwapIntervalEXT)
   {
      if (enable)
      {
         Printf("ZGL: Enabling vsync.\n");
         wglSwapIntervalEXT(1);
      }
      else
      {
         Printf("ZGL: Disabling vsync.\n");
         wglSwapIntervalEXT(0);
      }
   }
}


CUSTOM_CVAR(Bool, gl_vid_vsync, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
   if (screen == NULL || wglSwapIntervalEXT == NULL || wglGetSwapIntervalEXT == NULL)
   {
      return;
   }

   if (((OpenGLFrameBuffer *)screen)->SwapControl())
   {
	   GL_EnableVsync(self);
   }
}

CUSTOM_CVAR(Bool, gl_line_smooth, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
#if 0
   if (self)
   {
      glEnable(GL_LINE_SMOOTH);
   }
   else
   {
      glDisable(GL_LINE_SMOOTH);
   }
#else
   glDisable(GL_LINE_SMOOTH);
#endif
}

CUSTOM_CVAR(Int, gl_vid_refresh, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if ( fullscreen && ( gamestate != GS_STARTUP ))
		I_RestartRenderer( );
}

CUSTOM_CVAR(Int, gl_vid_stencilbits, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
}


extern TextureList textureList;
extern float FogStartLookup[256];

//*****************************************************************************
//	PROTOTYPES

void	DoBlending( const PalEntry *from, PalEntry *to, int count, int r, int g, int b, int a );

//*****************************************************************************
//	FUNCTIONS

OpenGLVideo::OpenGLVideo( )
{
	ShowWindow(Window, SW_NORMAL);

	FogStartLookup[0] = 0.0001f;
	for (int i = 1; i < 256; i++)
	{
		FogStartLookup[i] = powf(65536.f, byte2float[i]);
		FogStartLookup[i] *= byte2float[i];
	}

	// Zero out the modes list, and then create it.
	m_pVideoModeList = NULL;
	this->MakeModesList( );

	// We've tried to creat the frame buffer 0 times.
	m_ulFBCreateAttemptCounter = 0;
}

//*****************************************************************************
//
OpenGLVideo::~OpenGLVideo( )
{
	// Free the modes list.
	this->FreeModesList( );

	// What does this do?
	ChangeDisplaySettings( 0, 0 );

	// No longer display the window.
	ShowWindow( Window, SW_HIDE );
}

//*****************************************************************************
//*****************************************************************************
//
void OpenGLVideo::SetWindowedScale( float fScale )
{
	// What is this function supposed to do?
}

//*****************************************************************************
//
DFrameBuffer *OpenGLVideo::CreateFrameBuffer( int iWidth, int iHeight, bool bFullScreen, DFrameBuffer *pOldFrameBuffer )
{
	OpenGLFrameBuffer	*pFrameBuffer;
	PalEntry			flashColor;
	int					flashAmount;
	static	int			s_iOriginalWidth;
	static	int			s_iOriginalHeight;

	// Are the attributes of the old frame buffer the same? If so, we can just reuse it.
	if ( pOldFrameBuffer != NULL )
	{
		pFrameBuffer = static_cast<OpenGLFrameBuffer *>( pOldFrameBuffer );
		if (( pFrameBuffer->m_ulDisplayWidth == (ULONG)iWidth ) &&
			( pFrameBuffer->m_ulDisplayHeight == (ULONG)iHeight ) &&
			( pFrameBuffer->m_ulDisplayBPP == (ULONG)gl_vid_bitdepth ) &&
			( pFrameBuffer->m_bFullScreen == bFullScreen ))
		{
			return ( pOldFrameBuffer );
		}

		// Why do we do this?
		pOldFrameBuffer->GetFlash( flashColor, flashAmount );

		// Delete the old frame buffer, since it's no longer used.
		delete ( pOldFrameBuffer );
	}
	else
	{
		flashColor = 0;
		flashAmount = 0;
	}

	// Set our internal width, height, and BPP.
	m_ulDisplayWidth = iWidth;
	m_ulDisplayHeight = iHeight;
	m_ulDisplayBPP = gl_vid_bitdepth;

	pFrameBuffer = new OpenGLFrameBuffer( m_ulDisplayWidth, m_ulDisplayHeight, bFullScreen );

	// If we could not create the framebuffer, try again with slightly
	// different parameters in this order:
	// 1. Try with the closest size
	// 2. Try in the opposite screen mode with the original size
	// 3. Try in the opposite screen mode with the closest size
	// This is a somewhat confusing mass of recursion here.
	while (( pFrameBuffer == NULL ) || ( pFrameBuffer->IsValid( ) == false ))
	{
		static HRESULT hr;

		// If the frame buffer was created, but isn't valid, figure out what went wrong.
		if ( pFrameBuffer != NULL )
		{
			if ( m_ulFBCreateAttemptCounter == 0 )
				hr = pFrameBuffer->GetHR( );

			delete ( pFrameBuffer );
		}

		// Determine what to do based on what attempt we're on.
		switch ( m_ulFBCreateAttemptCounter )
		{
		// If this is our first attempt at creating the frame buffer, save the original width
		// and height.
		case 0:

			s_iOriginalWidth = iWidth;
			s_iOriginalHeight = iHeight;
		case 2:

			// Try a different resolution. Hopefully that will work.
			I_ClosestResolution( &iWidth, &iHeight, m_ulDisplayBPP );
			break;
		case 1:

			// Try changing fullscreen mode. Maybe that will work.
			iWidth = s_iOriginalWidth;
			iHeight = s_iOriginalHeight;
			bFullScreen = !bFullScreen;
			break;
		default:

			// Oh well, couldn't create it.
			I_FatalError( "Could not create new screen (%d x %d): %08x", s_iOriginalWidth, s_iOriginalHeight, hr );
			break;
		}

		m_ulFBCreateAttemptCounter++;
		pFrameBuffer = static_cast<OpenGLFrameBuffer *>( CreateFrameBuffer( iWidth, iHeight, bFullScreen, NULL ));
	}

	// We successfully created the frame buffer, so reset this.
	m_ulFBCreateAttemptCounter = 0;

	if ( pFrameBuffer->IsFullscreen( ) != bFullScreen )
		Video->FullscreenChanged( !bFullScreen );

	// What does this do?
	pFrameBuffer->SetFlash( flashColor, flashAmount );

	// Return the newly created frame buffer.
	return ( pFrameBuffer );
}

//*****************************************************************************
//
void OpenGLVideo::StartModeIterator( int iBPP )
{
	m_pIteratorVideoMode = m_pVideoModeList;
	m_ulIteratorBPP = iBPP;
}

//*****************************************************************************
//
bool OpenGLVideo::FullscreenChanged( bool bFullScreen )
{
	return ( m_bFullScreen != bFullScreen );
}

//*****************************************************************************
//
int OpenGLVideo::GetModeCount( void )
{
	ULONG			ulCount;
	VIDEOMODEINFO_s	*pMode;

	ulCount = 0;
	pMode = m_pVideoModeList;
	while ( pMode )
	{
		ulCount++;
		pMode = pMode->pNext;
	}

	return ( ulCount );
}

//*****************************************************************************
//
bool OpenGLVideo::NextMode( int *piWidth, int *piHeight, bool *bLetterbox, ULONG *pulHZ )
{
	if ( m_pIteratorVideoMode )
	{
		// Search for the next mode that matches our BPP (it might be this one!)
		while (( m_pIteratorVideoMode ) && ( m_pIteratorVideoMode->ulBPP != m_ulIteratorBPP ))
		{
			m_pIteratorVideoMode = m_pIteratorVideoMode->pNext;
		}

		// If a mode matched our BPP, "return" it.
		if ( m_pIteratorVideoMode )
		{
			*piWidth = m_pIteratorVideoMode->ulWidth;
			*piHeight = m_pIteratorVideoMode->ulHeight;
			if (pulHZ != NULL)
				*pulHZ = m_pIteratorVideoMode->ulHz;
			if (bLetterbox != NULL)
				*bLetterbox = m_pIteratorVideoMode->ulRealHeight != m_pIteratorVideoMode->ulHeight;
			m_pIteratorVideoMode = m_pIteratorVideoMode->pNext;
			return ( true );
		}
	}
	return ( false );
}

bool OpenGLVideo::NextMode( int *piWidth, int *piHeight, bool *bLetterbox )
{
	return NextMode( piWidth, piHeight, bLetterbox, NULL );
}
//*****************************************************************************
//
void OpenGLVideo::BlankForGDI( void )
{
   static_cast<OpenGLFrameBuffer *>( screen )->Blank( );
}

//*****************************************************************************
//
bool OpenGLVideo::GoFullscreen( bool bYes )
{
	DEVMODE		DevMode;
	ULONG		ulHz;

	// Nothing to do if our fullscreen status already matches what we want to change to.
//	if ( m_bFullScreen == bYes )
//		return ( bYes );

	// If we don't want fullscreen mode, there's not a lot we have to do.
	if ( bYes == false )
	{
		// Go back to windowed mode.
		ChangeDisplaySettings( 0, 0 );

		m_bFullScreen = false;
		return ( m_bFullScreen );
	}

	// If we don't have a refresh rate set, attempt to get it from... somewhere...
	if ( gl_vid_refresh == 0 )
		ulHz = this->GetRefreshRate( m_ulDisplayWidth, m_ulDisplayHeight, m_ulDisplayBPP );
	else
		ulHz = gl_vid_refresh;

	// The refresh rate must be at least 60 hertz.
	if ( ulHz < 60 )
		ulHz = 60;

	Printf( PRINT_OPENGL, "Setting refresh rate of %d Hz.\n", ulHz );

	// Zero out the settings.
	memset( &DevMode, 0, sizeof( DEVMODE ));

	DevMode.dmSize = sizeof( DEVMODE );
	DevMode.dmPelsWidth = m_ulDisplayWidth;
	DevMode.dmPelsHeight = m_ulDisplayHeight;
	DevMode.dmBitsPerPel = m_ulDisplayBPP;
	DevMode.dmDisplayFrequency = ulHz;
	DevMode.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL | DM_DISPLAYFREQUENCY;

	ChangeDisplaySettings( &DevMode, CDS_FULLSCREEN );

	m_bFullScreen = true;
	return ( m_bFullScreen );
}

//*****************************************************************************
//
ULONG OpenGLVideo::GetRefreshRate( ULONG ulWidth, ULONG ulHeight, ULONG ulBPP )
{
	VIDEOMODEINFO_s		*pMode;

	pMode = m_pVideoModeList;
	while ( pMode )
	{
		if (( pMode->ulWidth == ulWidth ) &&
			( pMode->ulHeight == ulHeight ) &&
			( pMode->ulBPP == ulBPP ))
		{
			return ( pMode->ulHz );
		}

		pMode = pMode->pNext;
	}

	// If we didn't find anything valid, just return 60.
	return ( 60 );
}

//*****************************************************************************
//
void OpenGLVideo::MakeModesList( void )
{
	//VIDEOMODEINFO_s *pLastMode = (VIDEOMODEINFO_s *)&m_pVideoModeList;
	DEVMODE			DevMode;
	LONG			lMode;
	VIDEOMODEINFO_s *pMode, *pNextMode;

	memset(&DevMode, 0, sizeof(DEVMODE));
	DevMode.dmSize = sizeof(DEVMODE);

	lMode = 0;
	while (EnumDisplaySettings(NULL, lMode, &DevMode))
	{
#if 0
		FILE *f = fopen("modes.txt", "a");
		fprintf(f, "width: %4d height: %4d bits: %2d hz: %3d\n", DevMode.dmPelsWidth, DevMode.dmPelsHeight, DevMode.dmBitsPerPel, DevMode.dmDisplayFrequency);
		fclose(f);
#endif
		this->AddMode(DevMode.dmPelsWidth, DevMode.dmPelsHeight, DevMode.dmBitsPerPel, DevMode.dmPelsHeight, DevMode.dmDisplayFrequency);
		lMode++;
	}

	// Now add 16:9 and 16:10 resolutions you can use in a window or letterboxed
	for (pMode = m_pVideoModeList; pMode != NULL; pMode = pNextMode)
	{
		pNextMode = pMode->pNext;
		if (pMode->ulRealHeight == pMode->ulHeight && pMode->ulHeight * 4/3 == pMode->ulWidth)
		{
			if (pMode->ulWidth >= 360)
			{
				AddMode (pMode->ulWidth, pMode->ulWidth * 9/16, pMode->ulBPP, pMode->ulHeight, pMode->ulHz);
			}
			if (pMode->ulWidth > 640)
			{
				AddMode (pMode->ulWidth, pMode->ulWidth * 10/16, pMode->ulBPP, pMode->ulHeight, pMode->ulHz);
			}
		}
	}
}

//*****************************************************************************
//
void OpenGLVideo::AddMode( ULONG ulWidth, ULONG ulHeight, ULONG ulBPP, ULONG ulBaseHeight, ULONG ulHz )
{
	VIDEOMODEINFO_s **ppProbe = &m_pVideoModeList;
	VIDEOMODEINFO_s *pProbe = m_pVideoModeList;

	if (ulHz < 60) ulHz = 60;

	// This mode may have been already added to the list because it is
	// enumerated multiple times at different refresh rates. If it's
	// not present, add it to the right spot in the list; otherwise, do nothing.
	// Modes are sorted first by width, then by height, then by depth. In each
	// case the order is ascending.
	for (; pProbe != 0; ppProbe = &pProbe->pNext, pProbe = pProbe->pNext)
	{
		if (pProbe->ulWidth > ulWidth)		break;
		if (pProbe->ulWidth < ulWidth)		continue;
		// Width is equal
		if (pProbe->ulHeight > ulHeight)		break;
		if (pProbe->ulHeight < ulHeight)		continue;
		// Height is equal
		if (pProbe->ulBPP > ulBPP)		break;
		if (pProbe->ulBPP < ulBPP)		continue;
		// Bits is equal
      if (pProbe->ulHz < ulHz)
      {
         // adjust the refresh rate for the mode
         pProbe->ulHz = ulHz;
      }
		return;
	}

	// Allocate memory for the new video mode.
	*ppProbe = new VIDEOMODEINFO_s (ulWidth, ulHeight, ulBPP, ulBaseHeight);
	(*ppProbe)->ulHz = ulHz;
	(*ppProbe)->pNext = pProbe;
}

//*****************************************************************************
//
void OpenGLVideo::FreeModesList( void )
{
	VIDEOMODEINFO_s	*pMode;
	VIDEOMODEINFO_s	*pTempMode;

	// Go through the video mode list and delete all the video modes.
	pMode = m_pVideoModeList;
	while ( pMode )
	{
		pTempMode = pMode;
		pMode = pMode->pNext;
		delete ( pTempMode );
	}

	m_pVideoModeList = NULL;
}

//*****************************************************************************
//*****************************************************************************
//
OpenGLFrameBuffer::OpenGLFrameBuffer( ULONG ulWidth, ULONG ulHeight, bool bFullScreen ) : BaseWinFB( ulWidth, ulHeight )
{
  // TO-DO: Use the constructor from ZDoomGL 0.81 (at the moment deactivated by "if 0");
#if 1
   RECT r;
   LONG lStyle, lExStyle;
   int i;

   m_ulDisplayWidth = ulWidth;
   m_ulDisplayHeight = ulHeight;
   m_ulDisplayBPP = (int)gl_vid_bitdepth;
   m_lNumTris = 0;

   m_ulTrueHeight = ulHeight;
   if (fullscreen)
   {
      for (VIDEOMODEINFO_s *mode = static_cast<OpenGLVideo *>(Video)->m_pVideoModeList; mode != NULL; mode = mode->pNext)
      {
         if (mode->ulWidth == ulWidth && mode->ulHeight == ulHeight)
         {
            m_ulTrueHeight = mode->ulRealHeight;
            break;
         }
      }
   }

   m_bFullScreen = (static_cast<OpenGLVideo *>(Video)->GoFullscreen(bFullScreen));

   for (i = 0; i < 256; i++)
   {
      m_GammaTable[i] = i;
   }
   memcpy (m_SourcePalette, GPalette.BaseColors, sizeof(PalEntry)*256);

   GetWindowRect(Window, &r);

   lStyle = WS_VISIBLE | WS_CLIPSIBLINGS;
   //lExStyle = WS_EX_APPWINDOW;
   lExStyle = 0;

   if (bFullScreen)
   {
      lStyle |= WS_POPUP;
      MoveWindow(Window, 0, 0, ulWidth, m_ulTrueHeight, FALSE);
   }
   else
   {
      lStyle |= WS_OVERLAPPEDWINDOW;
      lExStyle |= WS_EX_WINDOWEDGE;
      MoveWindow(Window, r.left, r.top, ulWidth + (GetSystemMetrics(SM_CXSIZEFRAME) * 2), ulHeight + (GetSystemMetrics(SM_CYSIZEFRAME) * 2) + GetSystemMetrics(SM_CYCAPTION), FALSE);
   }

   SetWindowLong(Window, GWL_STYLE, lStyle);
   SetWindowLong(Window, GWL_EXSTYLE, lExStyle);
   SetWindowPos(Window, 0, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

   m_hDC = GetDC(Window);

   this->SetupPixelFormat();
   Printf("ZGL: Creating context for %d*%d*%d screen.\n", m_ulDisplayWidth, m_ulDisplayHeight, m_ulDisplayBPP);
   Printf("ZGL: Stencil depth is %d bpp.\n", (int)gl_vid_stencilbits);
   m_hRC = wglCreateContext(m_hDC);
   if (m_hRC == NULL)
   {
      I_FatalError("Could not create OpenGL RC.");
   }

   ResetContext();
   CL_Init();

   Printf("ZGL: Vendor: %s\n", glGetString(GL_VENDOR));
   Printf("ZGL: Renderer: %s\n", glGetString(GL_RENDERER));
   Printf("ZGL: Version: %s\n", glGetString(GL_VERSION));

   if (!this->loadRequiredExtensions())
   {
      vid_renderer = 0;
      Printf("Falling back to software renderer.\n");
      m_bSupportsVertexProgram = false;
      m_supportsFragmentProgram = false;
      m_supportsVBO = false;
      m_bSupportsSwapInterval = false;
      m_bSupportsBlendSubtract = false;
      m_bSupportsRenderTexture = false;
      m_bCompiledArrays = false;
      m_bSupportsPointSprites = false;
   }
   else
   {
      loadExtensions();
      textureList.Init();

      SetViewport(0, 0, ulWidth, ulHeight);
      glGetIntegerv(GL_VIEWPORT, viewport);

      if (m_bSupportsVertexProgram)
      {
         GL_ReadVertexPrograms();
      }

      InitializeState();

      glEnableClientState(GL_VERTEX_ARRAY);
      glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	  // FIX: Crashes for some reason if textureList.NumTexUnits() is used instead of 1
      for (i = 0; i < 1/*textureList.NumTexUnits()*/; i++)
      {
         glClientActiveTextureARB(GL_TEXTURE0_ARB + i);
         glEnableClientState(GL_TEXTURE_COORD_ARRAY);
      }
      glClientActiveTextureARB(GL_TEXTURE0_ARB);
   }

   m_bSupportsGamma = false;
   if (gl_vid_hardwaregamma)
   {
      if (GetDeviceGammaRamp(m_hDC, (void *)m_OrigGamma))
      {
         m_bSupportsGamma = true;
         Printf("ZGL: Using hardware gamma\n");
      }
   }
   CalcGamma(Gamma, m_GammaTable);

#else
	//	ULONG	ulIdx;
	ULONG	ulFullWidth;
	ULONG	ulFullHeight;
	int		i;

	// Print a nice little message. Maybe have this sooner?
	Printf( PRINT_OPENGL, "Initializing OpenGL...\n" );

	// Initialize the display width, height, and bits per pixel.
	m_ulDisplayWidth = ulWidth;
	m_ulDisplayHeight = ulHeight;
	m_ulDisplayBPP = (ULONG)gl_vid_bitdepth;
	m_lNumTris = 0;

	memcpy( m_SourcePalette, GPalette.BaseColors, sizeof( PalEntry ) * 256 );

	m_ulTrueHeight = ulHeight;
	if (bFullScreen)
	{
		for (VIDEOMODEINFO_s *pMode = static_cast<OpenGLVideo *>(Video)->m_pVideoModeList; pMode != NULL; pMode = pMode->pNext)
		{
			if (pMode->ulWidth == ulWidth && pMode->ulHeight == ulHeight)
			{
				m_ulTrueHeight = pMode->ulRealHeight;
				break;
			}
		}
	}


	// Attempt to go into windowed or fullscreen mode, depending on what's passed into
	// this function. Then, set the fullscreen member variable to the actual fullscreen status.
	m_bFullScreen = static_cast<OpenGLVideo *>( Video )->GoFullscreen( bFullScreen );

	// Configure our window based on whether or not we're in fullscreen mode.
	if ( m_bFullScreen )
	{
		// Show the window.
		ShowWindow( Window, SW_SHOW );

		// Move the window into place. I'm not sure why this is needed on top of the
		// "SetWindowPos" call, but without it, a little gray bar appears at the top of the
		// screen.
		MoveWindow( Window, 0, 0, m_ulDisplayWidth, m_ulDisplayHeight, FALSE );

		// Apply the window's style/extended style settings.
		SetWindowLongPtr( Window, GWL_STYLE, WS_VISIBLE );

		// Set the window position and size.
		SetWindowPos( Window,
			NULL,
			0,
			0,
			m_ulDisplayWidth,
			m_ulDisplayHeight,
			SWP_NOCOPYBITS | SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED
			);
	}
	else
	{
		// We need to find the full width/height of the window. This includes our resolution,
		// the borders, and caption.
		ulFullWidth = m_ulDisplayWidth + ( GetSystemMetrics( SM_CXSIZEFRAME ) * 2 );
		ulFullHeight = m_ulDisplayHeight + ( GetSystemMetrics( SM_CYSIZEFRAME ) * 2 ) + GetSystemMetrics( SM_CYCAPTION );

		// Set this for some reason.
		VidResizing = true;

		// Apply the window's style/extended style settings.
		SetWindowLongPtr( Window, GWL_STYLE, WS_VISIBLE | WS_OVERLAPPEDWINDOW );

		// Set the window position and size.
		SetWindowPos( Window,
			NULL,
			0,
			0,
			ulFullWidth,
			ulFullHeight,
			SWP_DRAWFRAME | SWP_NOCOPYBITS | SWP_NOMOVE | SWP_NOZORDER 
			);

		// Same as above.
		VidResizing = false;

		// Show the window.
		ShowWindow( Window, SW_SHOW );
	}

	// Grab the device context for this window.
	m_hDC = GetDC( Window );

	// Little GZDoom thing here.
	m_bSupportsGamma = GetGammaRamp( (void *)m_OriginalGamma );

	// Set the pixel format that everything is drawn in (24-bit, etc.).
	this->SetupPixelFormat( );

	// If this necessary?
	Printf( PRINT_OPENGL, "Creating context for %dx%dx%d screen.\n", m_ulDisplayWidth, m_ulDisplayHeight, m_ulDisplayBPP );
	Printf( PRINT_OPENGL, "Stencil depth is %d bpp.\n", (int)gl_vid_stencilbits );

	// Create the OpenGL rendering context. This links all the OpenGL calls to the device context.
	if (( m_hRC = wglCreateContext( m_hDC )) == NULL )
		I_FatalError( "Could not create OpenGL rendering context!" );
	else
		wglMakeCurrent( m_hDC, m_hRC );

	// Print out some nice information.
	Printf( PRINT_OPENGL, "Vendor: %s\n", glGetString( GL_VENDOR ));
	Printf( PRINT_OPENGL, "Renderer: %s\n", glGetString( GL_RENDERER ));
	Printf( PRINT_OPENGL, "OpenGL version: %s\n", glGetString( GL_VERSION ));

	// Initialize the texture list.
	textureList.Init( );

	// Set the OpenGL viewport. This is the area of the screen it renders to.
	glViewport( 0, 0, m_ulDisplayWidth, m_ulDisplayHeight );

	// What's this?
	m_maxTexelUnits = 1;

	// Now load some extensions.
   useMultitexture = false;
   m_useMultiTexture = false;//GL_CheckExtension("GL_EXT_texture_env_combine") && GL_CheckExtension("GL_ARB_multitexture");
   if (m_useMultiTexture)
   {
      glGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB, &m_maxTexelUnits);
      Printf("ZGL: Enabling multitexturing (%d texel units).\n", m_maxTexelUnits);

      glActiveTextureARB = (PFNGLACTIVETEXTUREARBPROC)wglGetProcAddress("glActiveTextureARB");
      glClientActiveTextureARB = (PFNGLCLIENTACTIVETEXTUREARBPROC)wglGetProcAddress("glClientActiveTextureARB");
      glMultiTexCoord2fARB = (PFNGLMULTITEXCOORD2FARBPROC)wglGetProcAddress("glMultiTexCoord2fARB");

      useMultitexture = true;
   }

   m_supportsFragmentProgram = GL_CheckExtension("GL_ARB_fragment_program");
   if (m_supportsFragmentProgram)
   {
      //Printf("ZGL: Enabling fragment program support.\n");
   }

   m_supportsVBO = false;GL_CheckExtension("GL_ARB_vertex_buffer_object");
   glGenBuffersARB = NULL;
   glBindBufferARB = NULL;
   glBufferDataARB = NULL;
   glDeleteBuffersARB = NULL;
   if (m_supportsVBO)
   {
      Printf("ZGL: enabling vertex buffers.\n");
      glGenBuffersARB = (PFNGLGENBUFFERSARBPROC) wglGetProcAddress("glGenBuffersARB");
      glBindBufferARB = (PFNGLBINDBUFFERARBPROC) wglGetProcAddress("glBindBufferARB");
      glBufferDataARB = (PFNGLBUFFERDATAARBPROC) wglGetProcAddress("glBufferDataARB");
      glDeleteBuffersARB = (PFNGLDELETEBUFFERSARBPROC) wglGetProcAddress("glDeleteBuffersARB");
   }
/*
   m_supportsSwapInterval = GL_CheckExtension("WGL_EXT_swap_control");
   if (m_supportsSwapInterval)
   {
      Printf("ZGL: Enabling vsync control.\n");
      wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");
      wglGetSwapIntervalEXT = (PFNWGLGETSWAPINTERVALEXTPROC)wglGetProcAddress("wglGetSwapIntervalEXT");
      GL_EnableVsync(gl_vid_vsync);
   }
*/

	// Set the background color of the window to black.
	glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );

	// Set up our depth testing.
	glEnable( GL_DEPTH_TEST );
	glClearDepth( 1.0f );
	glDepthFunc( GL_LEQUAL );

	// Blend colors, and smooth out lighting.
	glShadeModel( GL_SMOOTH );

	// I need to look up ALLLLLLL of it to see what it is!
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

   if (m_useMultiTexture)
   {
      for (i = 0; i < m_maxTexelUnits; i++)
      {
         glClientActiveTextureARB(GL_TEXTURE0_ARB + i);
         glEnableClientState(GL_TEXTURE_COORD_ARRAY);
      }
      glClientActiveTextureARB(GL_TEXTURE0_ARB);
   }

	glEnable(GL_DITHER);
	glDisable(GL_ALPHA_TEST);
	glEnable(GL_CULL_FACE);
	glEnable(GL_POLYGON_OFFSET_FILL);
	glEnable(GL_BLEND);

	if (gl_line_smooth)
		glEnable(GL_LINE_SMOOTH);
	else
		glDisable(GL_LINE_SMOOTH);

	glAlphaFunc(GL_GREATER, 0.0);

	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	glHint(GL_FOG_HINT, GL_NICEST);
	glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

	glFogf(GL_FOG_MODE, GL_EXP);

	glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
	glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	CL_Init();
#endif
}

OpenGLFrameBuffer::~OpenGLFrameBuffer()
{
   textureList.ShutDown();
   CL_Shutdown();

   wglMakeCurrent(NULL, NULL);
   wglDeleteContext(m_hRC);
   ReleaseDC(Window, m_hDC);
}

//*****************************************************************************
//
void OpenGLFrameBuffer::InitializeState()
{
   glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
   glClearDepth(1.0f);
   glDepthFunc(GL_LEQUAL);
   glShadeModel(GL_SMOOTH);

   glEnable(GL_DITHER);
   glEnable(GL_DEPTH_TEST);
   glDisable(GL_ALPHA_TEST);
   glDisable(GL_FOG); // make sure to initialize to "off"
   glEnable(GL_CULL_FACE);
   glEnable(GL_POLYGON_OFFSET_FILL);
   glEnable(GL_POLYGON_OFFSET_LINE);
   glEnable(GL_BLEND);
#if 0
   if (gl_line_smooth)
   {
      glEnable(GL_LINE_SMOOTH);
   }
   else
   {
      glDisable(GL_LINE_SMOOTH);
   }
#else
   glDisable(GL_LINE_SMOOTH);
#endif
   glAlphaFunc(GL_GREATER, 0.0);

   glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
   glHint(GL_FOG_HINT, GL_NICEST);
   glFogi(GL_FOG_MODE, GL_EXP);
   glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

   glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
   glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);

   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

//*****************************************************************************
//
bool OpenGLFrameBuffer::loadRequiredExtensions()
{
   static const char *requiredExtensions[] = {
      "GL_ARB_multitexture",
      "GL_EXT_texture_env_combine",
      "GL_SGIS_generate_mipmap",
      NULL
   };
   const char *ext = requiredExtensions[0];
   int i = 0;

   glActiveTextureARB = NULL;
   glClientActiveTextureARB = NULL;
   glMultiTexCoord2fARB = NULL;

   while (ext)
   {
      if (!GL_CheckExtension(ext))
      {
         Printf("ZGL ERROR: Required extension %s missing.\n", ext);
         return false;
      }
      ext = requiredExtensions[++i];
   }

   glActiveTextureARB = (PFNGLACTIVETEXTUREARBPROC)wglGetProcAddress("glActiveTextureARB");
   glClientActiveTextureARB = (PFNGLCLIENTACTIVETEXTUREARBPROC)wglGetProcAddress("glClientActiveTextureARB");
   glMultiTexCoord2fARB = (PFNGLMULTITEXCOORD2FARBPROC)wglGetProcAddress("glMultiTexCoord2fARB");

   if (!glActiveTextureARB || !glClientActiveTextureARB || !glMultiTexCoord2fARB)
   {
      Printf("ZGL ERROR: multitexture reported as supported, but functions not found.\n");
      return false;
   }

   return true;
}

//*****************************************************************************
//
void OpenGLFrameBuffer::loadExtensions()
{
   m_bSupportsVertexProgram = GL_CheckExtension("GL_ARB_vertex_program");
   if (m_bSupportsVertexProgram)
   {
      Printf("ZGL: Enabling vertex program support.\n");
      glGenProgramsARB = (PFNGLGENPROGRAMSARBPROC)wglGetProcAddress("glGenProgramsARB");
      glDeleteProgramsARB = (PFNGLDELETEPROGRAMSARBPROC)wglGetProcAddress("glDeleteProgramsARB");
      glProgramStringARB = (PFNGLPROGRAMSTRINGARBPROC)wglGetProcAddress("glProgramStringARB");
      glBindProgramARB = (PFNGLBINDPROGRAMARBPROC)wglGetProcAddress("glBindProgramARB");
      glProgramEnvParameter4fARB = (PFNGLPROGRAMENVPARAMETER4FARBPROC)wglGetProcAddress("glProgramEnvParameter4fARB");
      glProgramLocalParameter4fARB = (PFNGLPROGRAMLOCALPARAMETER4FARBPROC)wglGetProcAddress("glProgramLocalParameter4fARB");
   }

   m_supportsFragmentProgram = GL_CheckExtension("GL_ARB_fragment_program");
   if (m_supportsFragmentProgram)
   {
      //Printf("ZGL: Enabling fragment program support.\n");
   }

   m_supportsVBO = GL_CheckExtension("GL_ARB_vertex_buffer_object");
   if (m_supportsVBO)
   {
      Printf("ZGL: enabling vertex buffers.\n");
      glGenBuffersARB = (PFNGLGENBUFFERSARBPROC) wglGetProcAddress("glGenBuffersARB");
      glBindBufferARB = (PFNGLBINDBUFFERARBPROC) wglGetProcAddress("glBindBufferARB");
      glBufferDataARB = (PFNGLBUFFERDATAARBPROC) wglGetProcAddress("glBufferDataARB");
      glDeleteBuffersARB = (PFNGLDELETEBUFFERSARBPROC) wglGetProcAddress("glDeleteBuffersARB");
   }
   else
   {
      glGenBuffersARB = NULL;
      glBindBufferARB = NULL;
      glBufferDataARB = NULL;
      glDeleteBuffersARB = NULL;
   }

   m_bSupportsSwapInterval = GL_CheckExtension("WGL_EXT_swap_control");
   if (m_bSupportsSwapInterval)
   {
      Printf("ZGL: Enabling vsync control.\n");
      wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");
      wglGetSwapIntervalEXT = (PFNWGLGETSWAPINTERVALEXTPROC)wglGetProcAddress("wglGetSwapIntervalEXT");
      GL_EnableVsync(gl_vid_vsync);
   }
   else
   {
      wglSwapIntervalEXT = NULL;
      wglGetSwapIntervalEXT = NULL;
   }

   m_bSupportsBlendSubtract = GL_CheckExtension("GL_EXT_blend_subtract");
   if (m_bSupportsBlendSubtract)
   {
      Printf("ZGL: Enabling subtractive blending.\n");
      glBlendEquationEXT = (PFNGLBLENDEQUATIONEXTPROC)wglGetProcAddress("glBlendEquationEXT");
      glBlendEquationEXT(GL_FUNC_ADD);
   }
   else
   {
      glBlendEquationEXT = NULL;
   }

   m_bSupportsRenderTexture = GL_CheckExtension("WGL_ARB_render_texture") && GL_CheckExtension("WGL_ARB_pbuffer") && GL_CheckExtension("WGL_ARB_pixel_format");
   if (m_bSupportsRenderTexture)
   {
      //Printf("ZGL: Enabling render to texture.\n");
   }

   m_bSupportsPointSprites = GL_CheckExtension("GL_ARB_point_sprite") && GL_CheckExtension("GL_ARB_point_parameters");
   m_bSupportsPointSprites = false; // hmm, pointsprites don't quite look right (can't make them as big as normal particles)
   if (m_bSupportsPointSprites)
   {
      Printf("ZGL: Enabling point sprites.\n");
      glPointParameterfARB = (PFNGLPOINTPARAMETERFEXTPROC)wglGetProcAddress("glPointParameterfARB");
      glPointParameterfvARB = (PFNGLPOINTPARAMETERFVEXTPROC)wglGetProcAddress("glPointParameterfvARB");

      glTexEnvf(GL_POINT_SPRITE_ARB, GL_COORD_REPLACE_ARB, GL_TRUE);
      float quadratic[] =  { 1.0f, 0.0f, 0.01f };
      glPointParameterfvARB(GL_POINT_DISTANCE_ATTENUATION_ARB, quadratic);
      float maxSize = 0.0f;
      glGetFloatv(GL_POINT_SIZE_MAX_ARB, &maxSize);
      glPointSize(maxSize);
      glPointParameterfARB(GL_POINT_SIZE_MAX_ARB, maxSize);
      glPointParameterfARB(GL_POINT_SIZE_MIN_ARB, 1.0f);
   }
   else
   {
      glPointParameterfARB = NULL;
      glPointParameterfvARB = NULL;
   }

   m_bCompiledArrays = GL_CheckExtension("GL_EXT_compiled_vertex_array");
   if (m_bCompiledArrays)
   {
      glLockArraysEXT = (PFNGLLOCKARRAYSEXTPROC)wglGetProcAddress("glLockArraysEXT");
      glUnlockArraysEXT = (PFNGLUNLOCKARRAYSEXTPROC)wglGetProcAddress("glUnlockArraysEXT");
   }
   else
   {
      glLockArraysEXT = NULL;
      glUnlockArraysEXT = NULL;
   }
}

//*****************************************************************************
//
bool OpenGLFrameBuffer::IsFullscreen( )
{
	return ( m_bFullScreen );
}

//*****************************************************************************
//
void OpenGLFrameBuffer::Blank( )
{
	if ( m_bFullScreen )
	{
		// This is the code for the software renderer for this function.
/*
		DDBLTFX blitFX = { sizeof(blitFX) };

		blitFX.dwFillColor = 0;
		DDraw->FlipToGDISurface ();
		PrimarySurf->Blt (NULL, NULL, NULL, DDBLT_COLORFILL, &blitFX);
*/
	}
}

//*****************************************************************************
//
bool OpenGLFrameBuffer::PaintToWindow( )
{
	// I don't believe anything needs to be done here.
	return ( true );
}

//*****************************************************************************
//*****************************************************************************
//
void OpenGLFrameBuffer::StartFrame()
{
   //SwapBuffers(m_hDC);
}
//*****************************************************************************
//*****************************************************************************
//
void OpenGLFrameBuffer::Update( )
{
	// Draw the frames per second, etc.
	DrawRateStuff( );

	if (m_bFullScreen && (m_ulTrueHeight != m_ulDisplayHeight))
	{
		// Letterbox time! Draw black top and bottom borders.
		int borderHeight = (m_ulTrueHeight - m_ulDisplayHeight) / 2;

		glViewport(0, 0, m_ulDisplayWidth, m_ulTrueHeight);
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(0.0, m_ulDisplayWidth * 1.0, 0.0, m_ulTrueHeight * 1.0, -1.0, 1.0);
		glMatrixMode(GL_MODELVIEW);
		glColor3f(0.f, 0.f, 0.f);
		glDisable(GL_TEXTURE_2D);

		glBegin(GL_QUADS);
			// upper quad
			glVertex2i(0, borderHeight);
			glVertex2i(0, 0);
			glVertex2i(m_ulDisplayWidth, 0);
			glVertex2i(m_ulDisplayWidth, borderHeight);
		glEnd();

		glBegin(GL_QUADS);
			// lower quad
			glVertex2i(0, m_ulTrueHeight);
			glVertex2i(0, m_ulTrueHeight - borderHeight);
			glVertex2i(m_ulDisplayWidth, m_ulTrueHeight - borderHeight);
			glVertex2i(m_ulDisplayWidth, m_ulTrueHeight);
		glEnd();

		glEnable(GL_TEXTURE_2D);
		GL_Set2DMode();
		SetViewport(0, 0, m_ulDisplayWidth, m_ulDisplayHeight);
	}
	// Swap the buffers.
	SwapBuffers( m_hDC );
}

//*****************************************************************************
//
void OpenGLFrameBuffer::GetFlashedPalette( PalEntry aPal[256] )
{
	memcpy( aPal, m_SourcePalette, sizeof( PalEntry ) * 256 );
	if ( m_lFlashAmount )
		DoBlending( aPal, aPal, 256, m_Flash.r, m_Flash.g, m_Flash.b, m_lFlashAmount );
}

//*****************************************************************************
//
void OpenGLFrameBuffer::UpdatePalette( )
{
	m_bNeedPalUpdate = true;
}

//*****************************************************************************
//
void OpenGLFrameBuffer::GetFlash( PalEntry &pRGB, int &piAmount )
{
	pRGB = m_Flash;
	piAmount = m_lFlashAmount;
}

//*****************************************************************************
//
void OpenGLFrameBuffer::GetFlash( PalEntry *pRGP, int *piAmount )
{
	pRGP->r = m_Flash.r;
	pRGP->g = m_Flash.g;
	pRGP->b = m_Flash.b;
	*piAmount = m_lFlashAmount;
}

//*****************************************************************************
//
void OpenGLFrameBuffer::Dim() const
{
	PalEntry pe;
	float fA;

	pe.r = (BYTE)(RPART(DWORD(*dimcolor)));
	pe.g = (BYTE)(GPART(DWORD(*dimcolor)));
	pe.b = (BYTE)(BPART(DWORD(*dimcolor)));

	// Make sure the cvar dimamount is valid.
	if ( dimamount < 0.0f )
		dimamount = 0.0f;
	else if ( dimamount > 1.0f )
		dimamount = 1.0f;

	if ( dimamount == 0.0f )
		return;

	fA = (float)dimamount;

	Dim(pe, fA, 0, 0, this->GetWidth(), this->GetHeight());
}

//*****************************************************************************
//
void OpenGLFrameBuffer::Dim( PalEntry Color, float fDAmount, int iX1, int iY1, int iW, int iH ) const
{

	float	fR;
	float	fG;
	float	fB;
	float	fA;

	// Disable the use of textures and set our blend function.
	glDisable(GL_TEXTURE_2D);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	fR = byte2float[Color.r];
	fG = byte2float[Color.g];
	fB = byte2float[Color.b];
	fA = (float)dimamount;

	glBegin( GL_TRIANGLE_FAN );
		glColor4f( fR, fG, fB, 0.0f );
		glVertex2i(iX1, this->GetHeight() - iY1);
		glVertex2i(iX1, this->GetHeight() - (iY1 + iH));
		glVertex2i(iX1 + iW, this->GetHeight() - (iY1 + iH));
		glVertex2i(iX1 + iW, this->GetHeight() - iY1);
	glEnd();

	// Turn the use of textures back on.
	glEnable(GL_TEXTURE_2D);
}

//*****************************************************************************
//
void OpenGLFrameBuffer::Clear( int iLeft, int iTop, int iRight, int iBottom, int iColor ) const
{
	int iRt, iH;
	int iOffY = 0;
	PalEntry		*pPaletteEntry;

	pPaletteEntry = &GPalette.BaseColors[iColor];

	//GL_Set2DMode();

	iRt = m_ulDisplayHeight - iTop;
	iH = iBottom - iTop;

	if (m_bFullScreen && (m_ulTrueHeight != m_ulDisplayHeight))
	{
		iOffY = (m_ulTrueHeight - m_ulDisplayHeight) / 2;
		iRt += iOffY;
	}

	glEnable(GL_SCISSOR_TEST);
	glScissor(iLeft, iRt - iH, iRight - iLeft, iH);

	glClearColor(byte2float[pPaletteEntry->r], byte2float[pPaletteEntry->g], byte2float[pPaletteEntry->b], 0.f);
	glClear(GL_COLOR_BUFFER_BIT);
	glClearColor(0.f, 0.f, 0.f, 0.f);

	glDisable(GL_SCISSOR_TEST);
}

//*****************************************************************************
//
bool OpenGLFrameBuffer::Lock( )
{
	//FIXME
	return ( true );
}

//*****************************************************************************
//
bool OpenGLFrameBuffer::Lock( bool bBuffer )
{
	//FIXME
	Buffer = MemBuffer;
	return ( true );
}

//*****************************************************************************
//
bool OpenGLFrameBuffer::Relock( )
{
	//FIXME
	return ( true );
}

//*****************************************************************************
//
bool OpenGLFrameBuffer::SetGamma( float fGamma )
{
	WORD gammaTable[768];

	m_fGamma = fGamma;
	CalcGamma( fGamma, m_GammaTable);

	if (m_bSupportsGamma)
	{
		for (int i = 0; i < 256; i++)
		{
			gammaTable[i] = gammaTable[i + 256] = gammaTable[i + 512] = (WORD)(byte2float[m_GammaTable[i]] * 65535);
		}
		SetDeviceGammaRamp(m_hDC, (void*)gammaTable);
	}
	else
	{
		// avoid a double-purge when changing renderers
		if (changerenderer == false)
		{
			textureList.Purge();
		}
	}

	return true;
}

//*****************************************************************************
//
bool OpenGLFrameBuffer::GetGammaRamp( void *pv )
{
	return ( !!GetDeviceGammaRamp( m_hDC, pv ));
}

//*****************************************************************************
//
void OpenGLFrameBuffer::PaletteChanged( )
{
}

//*****************************************************************************
//
PalEntry *OpenGLFrameBuffer::GetPalette( )
{
	return ( m_SourcePalette );
}

//*****************************************************************************
//
void OpenGLFrameBuffer::SetupPixelFormat( )
{
   int zDepth = m_ulDisplayBPP;
   int colorDepth;
   int stencilBits;
   HDC deskDC;

   colorDepth = m_ulDisplayBPP;
   if (!fullscreen)
   {
      // not fullscreen, so match color depth to desktop depth
      deskDC = GetDC(GetDesktopWindow());
      colorDepth = GetDeviceCaps(deskDC, BITSPIXEL);
      ReleaseDC(GetDesktopWindow(), deskDC);
   }

   if (gl_vid_stencilbits && colorDepth > 16)
   {
      zDepth = 32;
      stencilBits = gl_vid_stencilbits;
   }
   else
   {
      zDepth = 16;
      stencilBits = 0;
   }

   PIXELFORMATDESCRIPTOR pfd = {
      sizeof(PIXELFORMATDESCRIPTOR),
      1,
      PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
      PFD_TYPE_RGBA,
      colorDepth, // color depth
      0, 0, 0, 0, 0, 0,
      0,
      0,
      0,
      0, 0, 0, 0,
      zDepth, // z depth
      stencilBits, // stencil buffer
      0,
      PFD_MAIN_PLANE,
      0,
      0, 0, 0
   };

   int pixelFormat;

   pixelFormat = ChoosePixelFormat(m_hDC, &pfd);
   DescribePixelFormat(m_hDC, pixelFormat, sizeof(pfd), &pfd);

   if (pfd.dwFlags & PFD_GENERIC_FORMAT)
   {
      if (!gl_vid_allowsoftware)
      {
         // not accelerated!
         vid_renderer = 0;
         Printf( PRINT_OPENGL, "OpenGL driver not accelerated!  Falling back to software renderer.\n");
      }
   }

   SetPixelFormat(m_hDC, pixelFormat, &pfd);
}






bool OpenGLFrameBuffer::SetFlash(PalEntry rgb, int amount)
{
	m_Flash = rgb;
	m_lFlashAmount = amount;

	m_bNeedPalUpdate = true;

	return true;
}



int OpenGLFrameBuffer::GetPageCount()
{
   return 1;
}


int OpenGLFrameBuffer::QueryNewPalette()
{
   //FIXME
   return 0;
}


HRESULT OpenGLFrameBuffer::GetHR()
{
   return m_lastHR;
}


void OpenGLFrameBuffer::loadLightInfo()
{
}

int OpenGLFrameBuffer::GetBitdepth()
{
   return m_ulDisplayBPP;
}

bool OpenGLFrameBuffer::UseVBO()
{
   return gl_use_vertex_buffer_objects && m_supportsVBO;
}

BYTE OpenGLFrameBuffer::GetGamma(BYTE input)
{
   return m_GammaTable[input];
}

void OpenGLFrameBuffer::SetViewport(int iX, int iY, int iWidth, int iHeight)
{
	int iOffY = 0;

	if (m_bFullScreen && (m_ulTrueHeight != m_ulDisplayHeight))
	{
		iOffY = (m_ulTrueHeight - m_ulDisplayHeight) / 2;
	}

	glViewport(iX, iY + iOffY, iWidth, iHeight);
}

void OpenGLFrameBuffer::SetNumTris(int iNumTris)
{
   //m_lNumTris = iNumTris;
}

void OpenGLFrameBuffer::ReadPixels(int iX, int iY, int iWidth, int iHeight, BYTE *img)
{
	int iOffY = 0;

	if (m_bFullScreen && (m_ulTrueHeight != m_ulDisplayHeight))
	{
		iOffY = (m_ulTrueHeight - m_ulDisplayHeight) / 2;
	}

	glReadPixels(iX, iY + iOffY, iWidth, iHeight, GL_RGB, GL_UNSIGNED_BYTE, img);
}

void OpenGLFrameBuffer::ResetContext()
{
	wglMakeCurrent(m_hDC, m_hRC);
}