#include "gl_pch.h"

#include "win32iface.h"
#include "win32gliface.h"
#include "gl/gl_basic.h"
#include "gl/gl_intern.h"
#include "gl/gl_struct.h"
#include "gl/gl_texture.h"
#include "gl/gl_functions.h"
#include "templates.h"
#include "version.h"
#include "c_console.h"
//#include "gl_defs.h"

CUSTOM_CVAR(Int, gl_vid_multisample, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL )
{
	Printf("This won't take effect until "GAMENAME" is restarted.\n");
}

RenderContext gl;


CVAR(Bool, gl_vid_allowsoftware, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);
CVAR(Bool, gl_vid_compatibility, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);
CVAR(Int, gl_vid_refreshHz, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);

Win32GLVideo::Win32GLVideo(int parm) : m_Modes(NULL), m_IsFullscreen(false)
{
	I_SetWndProc();
	m_DisplayWidth = vid_defwidth;
	m_DisplayHeight = vid_defheight;
	m_DisplayBits = gl_vid_compatibility? 16:32;
	m_DisplayHz = 60;
	MakeModesList();
	GetContext(gl);
}

Win32GLVideo::~Win32GLVideo()
{
	FreeModes();
	FGLTexture::FlushAll();
	gl.SetFullscreen(0,0,0,0);
}

void Win32GLVideo::SetWindowedScale(float scale)
{
}

void Win32GLVideo::MakeModesList()
{
	ModeInfo *pMode, *nextmode;
	DEVMODE dm;
	int mode = 0;

	memset(&dm, 0, sizeof(DEVMODE));
	dm.dmSize = sizeof(DEVMODE);

	while (EnumDisplaySettings(NULL, mode, &dm))
	{
		this->AddMode(dm.dmPelsWidth, dm.dmPelsHeight, dm.dmBitsPerPel, dm.dmPelsHeight, dm.dmDisplayFrequency);
		++mode;
	}

	for (pMode = m_Modes; pMode != NULL; pMode = nextmode)
	{
		nextmode = pMode->next;
		if (pMode->realheight == pMode->height && pMode->height * 4/3 == pMode->width)
		{
			if (pMode->width >= 360)
			{
				AddMode (pMode->width, pMode->width * 9/16, pMode->bits, pMode->height, pMode->refreshHz);
			}
			if (pMode->width > 640)
			{
				AddMode (pMode->width, pMode->width * 10/16, pMode->bits, pMode->height, pMode->refreshHz);
			}
		}
	}
}

void Win32GLVideo::StartModeIterator(int bits, bool fs)
{
	m_IteratorMode = m_Modes;
	// I think it's better to ignore the game-side settings of bit depth.
	// The GL renderer will always default to 32 bits, except in compatibility mode
	m_IteratorBits = gl_vid_compatibility? 16:32;	
	m_IteratorFS = fs;
}

bool Win32GLVideo::NextMode(int *width, int *height, bool *letterbox)
{
	if (m_IteratorMode)
	{
		while (m_IteratorMode && m_IteratorMode->bits != m_IteratorBits)
		{
			m_IteratorMode = m_IteratorMode->next;
		}

		if (m_IteratorMode)
		{
			*width = m_IteratorMode->width;
			*height = m_IteratorMode->height;
			if (letterbox != NULL) *letterbox = m_IteratorMode->realheight != m_IteratorMode->height;
			m_IteratorMode = m_IteratorMode->next;
			return true;
		}
	}

	return false;
}

void Win32GLVideo::AddMode(int x, int y, int bits, int baseHeight, int refreshHz)
{
	ModeInfo **probep = &m_Modes;
	ModeInfo *probe = m_Modes;

	// This mode may have been already added to the list because it is
	// enumerated multiple times at different refresh rates. If it's
	// not present, add it to the right spot in the list; otherwise, do nothing.
	// Modes are sorted first by width, then by height, then by depth. In each
	// case the order is ascending.
	for (; probe != 0; probep = &probe->next, probe = probe->next)
	{
		if (probe->width != x)		continue;
		// Width is equal
		if (probe->height != y)		continue;
		// Width is equal
		if (probe->realheight != baseHeight)	continue;
		// Height is equal
		if (probe->bits != bits)	continue;
		// Bits is equal
		if (probe->refreshHz > refreshHz) continue;
		probe->refreshHz = refreshHz;
		return;
	}

	*probep = new ModeInfo (x, y, bits, baseHeight, refreshHz);
	(*probep)->next = probe;
}

void Win32GLVideo::FreeModes()
{
	ModeInfo *mode = m_Modes;

	while (mode)
	{
		ModeInfo *tempmode = mode;
		mode = mode->next;
		delete tempmode;
	}

	m_Modes = NULL;
}

bool Win32GLVideo::GoFullscreen(bool yes)
{
	m_IsFullscreen = yes;

	m_trueHeight = m_DisplayHeight;
	for (ModeInfo *mode = m_Modes; mode != NULL; mode = mode->next)
	{
		if (mode->width == m_DisplayWidth && mode->height == m_DisplayHeight)
		{
			m_trueHeight = mode->realheight;
			break;
		}
	}

	if (yes)
	{
		gl.SetFullscreen(m_DisplayWidth, m_trueHeight, m_DisplayBits, m_DisplayHz);
	}
	else
	{
		gl.SetFullscreen(0,0,0,0);
	}
	return yes;
}


DFrameBuffer *Win32GLVideo::CreateFrameBuffer(int width, int height, bool fs, DFrameBuffer *old)
{
	Win32GLFrameBuffer *fb;

	m_DisplayWidth = width;
	m_DisplayHeight = height;
	m_DisplayBits = gl_vid_compatibility? 16:32;
	m_DisplayHz = 60;

	if (gl_vid_refreshHz == 0)
	{
		for (ModeInfo *mode = m_Modes; mode != NULL; mode = mode->next)
		{
			if (mode->width == m_DisplayWidth && mode->height == m_DisplayHeight && mode->bits == m_DisplayBits)
			{
				m_DisplayHz = MAX<int>(m_DisplayHz, mode->refreshHz);
			}
		}
	}
	else
	{
		m_DisplayHz = gl_vid_refreshHz;
	}

	if (old != NULL)
	{ // Reuse the old framebuffer if its attributes are the same
		fb = static_cast<Win32GLFrameBuffer *> (old);
		if (fb->m_Width == m_DisplayWidth &&
			fb->m_Height == m_DisplayHeight &&
			fb->m_Bits == m_DisplayBits &&
			fb->m_RefreshHz == m_DisplayHz &&
			fb->m_Fullscreen == fs)
		{
			return old;
		}
		//old->GetFlash(flashColor, flashAmount);
		delete old;
	}

	fb = new Win32GLFrameBuffer(m_DisplayWidth, m_DisplayHeight, m_DisplayBits, m_DisplayHz, fs);

	return fb;
}


static void CALLBACK DoPrintText(const char * out)
{
	PrintString(PRINT_HIGH, out);
}

IMPLEMENT_CLASS(Win32GLFrameBuffer)

Win32GLFrameBuffer::Win32GLFrameBuffer(int width, int height, int bits, int refreshHz, bool fullscreen) : BaseWinFB(width, height) 
{
	static int localmultisample=-1;

	if (localmultisample<0) localmultisample=gl_vid_multisample;

	m_Width = width;
	m_Height = height;
	m_Bits = bits;
	m_RefreshHz = refreshHz;
	m_Fullscreen = fullscreen;
	m_Lock=0;

	memcpy (SourcePalette, GPalette.BaseColors, sizeof(PalEntry)*256);
	UpdatePalette ();

	RECT r;
	LONG style, exStyle;

	static_cast<Win32GLVideo *>(Video)->GoFullscreen(fullscreen);

	ShowWindow (Window, SW_SHOW);
	GetWindowRect(Window, &r);
	style = WS_VISIBLE | WS_CLIPSIBLINGS;
	exStyle = 0;

	if (fullscreen)
	{
		style |= WS_POPUP;
		MoveWindow(Window, 0, 0, width, static_cast<Win32GLVideo *>(Video)->GetTrueHeight(), FALSE);
	}
	else
	{
		style |= WS_OVERLAPPEDWINDOW;
		exStyle |= WS_EX_WINDOWEDGE;
		MoveWindow(Window, r.left, r.top, width + (GetSystemMetrics(SM_CXSIZEFRAME) * 2), height + (GetSystemMetrics(SM_CYSIZEFRAME) * 2) + GetSystemMetrics(SM_CYCAPTION), FALSE);
	}

	SetWindowLong(Window, GWL_STYLE, style);
	SetWindowLong(Window, GWL_EXSTYLE, exStyle);
	SetWindowPos(Window, 0, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
	I_RestoreWindowedPos();

	if (!gl.InitHardware(Window, gl_vid_allowsoftware, gl_vid_compatibility, localmultisample, DoPrintText))
	{
		vid_renderer = 0;
		return;
	}

	m_supportsGamma = gl.GetGammaRamp((void *)m_origGamma);
	DoSetGamma();

	InitializeState();
}

Win32GLFrameBuffer::~Win32GLFrameBuffer()
{
	I_SaveWindowedPos();
	gl_ClearShaders();
	if (m_supportsGamma) 
	{
		gl.SetGammaRamp((void *)m_origGamma);
	}
	gl.Shutdown();
}


void Win32GLFrameBuffer::InitializeState()
{
	static bool first=true;

	gl.LoadExtensions();
	if (first)
	{
		first=false;
		// [BB] For some reason this crashes, if compiled with MinGW and optimization. Has to be investigated.
#ifdef _MSC_VER
		gl.PrintStartupLog(DoPrintText);
#endif

		if (gl.flags&RFL_NPOT_TEXTURE)
		{
			Printf("Support for non power 2 textures enabled.\n");
		}
		if (gl.flags&RFL_OCCLUSION_QUERY)
		{
			Printf("Occlusion query enabled.\n");
		}
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
	//GL::SetPerspective(90.f, GetWidth() * 1.f / GetHeight(), 0.f, 1000.f);
	*/

	gl.Viewport(0, (GetTrueHeight()-GetHeight())/2, GetWidth(), GetHeight()); 

	gl_InitShaders();
	gl_InitFog();
	Set2DMode();

	if (gl_vertices.Size())
	{
		gl.ArrayPointer(&gl_vertices[0], sizeof(GLVertex));
	}
}

void Win32GLFrameBuffer::Update()
{
	if (!AppActive) return;
	/*
	if (m_Lock>1)
	{
		m_UpdatePending=true;
		return;
	}
	*/

	Set2DMode();

	DrawRateStuff();

	if (GetTrueHeight() != GetHeight())
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

	}

	Finish.Reset();
	Finish.Start();
	gl.Finish();
	Finish.Stop();
	gl.SwapBuffers();
	Unlock();
}

//===========================================================================
//
// DoSetGamma
//
// (Unfortunately Windows has some safety precautions that block gamma ramps
//  that are considered too extreme. As a result this doesn't work flawlessly)
//
//===========================================================================

void Win32GLFrameBuffer::DoSetGamma()
{
	WORD gammaTable[768];

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

			gammaTable[i] = gammaTable[i + 256] = gammaTable[i + 512] = (WORD)clamp<double>(val*256, 0, 0xffff);
		}
		gl.SetGammaRamp((void*)gammaTable);
	}
}

bool Win32GLFrameBuffer::SetGamma(float gamma)
{
	DoSetGamma();
	return true;
}

bool Win32GLFrameBuffer::SetBrightness(float bright)
{
	DoSetGamma();
	return true;
}

bool Win32GLFrameBuffer::SetContrast(float contrast)
{
	DoSetGamma();
	return true;
}


HRESULT Win32GLFrameBuffer::GetHR() 
{ 
	return 0; 
}

void Win32GLFrameBuffer::Blank () 
{
}

bool Win32GLFrameBuffer::PaintToWindow () 
{ 
	return false; 
}

bool Win32GLFrameBuffer::CreateResources () 
{ 
	return false; 
}

void Win32GLFrameBuffer::ReleaseResources () 
{
}

bool Win32GLFrameBuffer::Lock(bool buffered)
{
	m_Lock++;
	Buffer = MemBuffer;
	return true;
}

bool Win32GLFrameBuffer::Lock () 
{ 	
	return Lock(false); 
}

void Win32GLFrameBuffer::Unlock () 	
{ 
	m_Lock--;
}

bool Win32GLFrameBuffer::IsLocked () 
{ 
	return m_Lock>0;// true;
}

PalEntry *Win32GLFrameBuffer::GetPalette()
{
	return SourcePalette;
}

void Win32GLFrameBuffer::GetFlashedPalette(PalEntry palette[256])
{
	memcpy(palette, SourcePalette, 256*sizeof(PalEntry));
}

void Win32GLFrameBuffer::UpdatePalette()
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

bool Win32GLFrameBuffer::SetFlash(PalEntry rgb, int amount)
{
	return true;
}

void Win32GLFrameBuffer::GetFlash(PalEntry &rgb, int &amount)
{
}

int Win32GLFrameBuffer::GetPageCount()
{
	return 1;
}

bool Win32GLFrameBuffer::IsFullscreen()
{
	return m_Fullscreen;
}

void Win32GLFrameBuffer::PaletteChanged()
{
}

int Win32GLFrameBuffer::QueryNewPalette()
{
	return 0;
}

//==========================================================================
//
//
//
//==========================================================================
void Win32GLFrameBuffer::Set2DMode()
{
	gl_SetColorMode(CM_DEFAULT);	// no colormap translations in 3D mode!
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
}

//==========================================================================
//
//
//
//==========================================================================

void Win32GLFrameBuffer::SetVSync (bool vsync)
{
	if (gl.SetVSync!=NULL) gl.SetVSync(vsync);
}

//==========================================================================
//
//
//
//==========================================================================
void gl_ClearScreen()
{
	gl.MatrixMode(GL_MODELVIEW);
	gl.PushMatrix();
	gl.MatrixMode(GL_PROJECTION);
	gl.PushMatrix();
	static_cast<Win32GLFrameBuffer*>(screen)->Set2DMode();
	screen->Dim(0, 1.f, 0, 0, SCREENWIDTH, SCREENHEIGHT);
	gl.Enable(GL_DEPTH_TEST);
	gl.MatrixMode(GL_PROJECTION);
	gl.PopMatrix();
	gl.MatrixMode(GL_MODELVIEW);
	gl.PopMatrix();
}

