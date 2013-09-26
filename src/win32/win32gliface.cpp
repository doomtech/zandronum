#include "gl/system/gl_system.h"

#include "win32iface.h"
#include "win32gliface.h"
//#include "gl/gl_intern.h"
#include "templates.h"
#include "version.h"
#include "c_console.h"
#include "hardware.h"
#include "v_video.h"
#include "i_input.h"
#include "i_system.h"
#include "doomstat.h"
//#include "gl_defs.h"

#include "gl/renderer/gl_renderer.h"
#include "gl/system/gl_framebuffer.h"
#include "gl/shaders/gl_shader.h"
#include "gl/utility/gl_templates.h"

void gl_CalculateCPUSpeed();
extern int NewWidth, NewHeight, NewBits, DisplayBits;

CUSTOM_CVAR(Int, gl_vid_multisample, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL )
{
	Printf("This won't take effect until "GAMENAME" is restarted.\n");
}

RenderContext gl;


CVAR(Bool, gl_vid_allowsoftware, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);
EXTERN_CVAR(Bool, gl_vid_compatibility)
EXTERN_CVAR(Int, vid_refreshrate)

Win32GLVideo::Win32GLVideo(int parm) : m_Modes(NULL), m_IsFullscreen(false)
{
	#ifdef _WIN32
		 if (CPU.bRDTSC) gl_CalculateCPUSpeed();
	#endif
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
	if (GLRenderer != NULL) GLRenderer->FlushTextures();
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

	if (vid_refreshrate == 0)
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
		m_DisplayHz = vid_refreshrate;
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

	fb = new OpenGLFrameBuffer(m_DisplayWidth, m_DisplayHeight, m_DisplayBits, m_DisplayHz, fs);

	return fb;
}

bool Win32GLVideo::SetResolution (int width, int height, int bits)
{
	if (GLRenderer != NULL) GLRenderer->FlushTextures();
	I_ShutdownGraphics();
	
	Video = new Win32GLVideo(0);
	if (Video == NULL) I_FatalError ("Failed to initialize display");
	
	bits=32;
	
	V_DoModeSetup(width, height, bits);
	return true;	// We must return true because the old video context no longer exists.
}



IMPLEMENT_ABSTRACT_CLASS(Win32GLFrameBuffer)

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

	if (!gl.InitHardware(Window, gl_vid_allowsoftware, gl_vid_compatibility, localmultisample))
	{
		vid_renderer = 0;
		return;
	}

	HDC hDC = GetDC(Window);
	m_supportsGamma = !!GetDeviceGammaRamp(hDC, (void *)m_origGamma);
	ReleaseDC(Window, hDC);
}

Win32GLFrameBuffer::~Win32GLFrameBuffer()
{
	if (m_supportsGamma) 
	{
		HDC hDC = GetDC(Window);
		SetDeviceGammaRamp(hDC, (void *)m_origGamma);
		ReleaseDC(Window, hDC);
	}
	I_SaveWindowedPos();

	gl.SetFullscreen(0,0,0,0);

	ShowWindow (Window, SW_SHOW);
	SetWindowLong(Window, GWL_STYLE, WS_VISIBLE | WS_CLIPSIBLINGS | WS_OVERLAPPEDWINDOW);
	SetWindowLong(Window, GWL_EXSTYLE, WS_EX_WINDOWEDGE);
	SetWindowPos(Window, 0, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
	I_GetEvent();

	gl.Shutdown();
}


void Win32GLFrameBuffer::InitializeState()
{
}

bool Win32GLFrameBuffer::CanUpdate()
{
	if (!AppActive) return false;
	return true;
}

void Win32GLFrameBuffer::SetGammaTable(WORD *tbl)
{
	HDC hDC = GetDC(Window);
	SetDeviceGammaRamp(hDC, (void *)tbl);
	ReleaseDC(Window, hDC);
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

void Win32GLFrameBuffer::SetVSync (bool vsync)
{
	if (gl.SetVSync!=NULL) gl.SetVSync(vsync);
}

void Win32GLFrameBuffer::NewRefreshRate ()
{
	if (m_Fullscreen)
	{
		setmodeneeded = true;
		NewWidth = screen->GetWidth();
		NewHeight = screen->GetHeight();
		NewBits = DisplayBits;
	}
}

