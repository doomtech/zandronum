#ifndef __WIN32GLIFACE_H__
#define __WIN32GLIFACE_H__

/*
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <gl/gl.h>

#include "gl/glext.h"
#include "gl/wglext.h"
*/

#include "win32iface.h"
#include "hardware.h"
#include "v_video.h"
#include "tarray.h"

extern IVideo *Video;


extern BOOL AppActive;

EXTERN_CVAR (Float, dimamount)
EXTERN_CVAR (Color, dimcolor)

EXTERN_CVAR(Int, vid_defwidth);
EXTERN_CVAR(Int, vid_defheight);
EXTERN_CVAR(Int, vid_renderer);

extern HINSTANCE g_hInst;
extern HWND Window;
extern IVideo *Video;



class Win32GLVideo : public IVideo
{
public:
	Win32GLVideo(int parm);
	virtual ~Win32GLVideo();

	EDisplayType GetDisplayType () { return DISPLAY_Both; }
	void SetWindowedScale (float scale);
	void StartModeIterator (int bits, bool fs);
	bool NextMode (int *width, int *height, bool *letterbox);
	bool GoFullscreen(bool yes);
	DFrameBuffer *CreateFrameBuffer (int width, int height, bool fs, DFrameBuffer *old);
	virtual bool SetResolution (int width, int height, int bits);

protected:
	struct ModeInfo
	{
		ModeInfo (int inX, int inY, int inBits, int inRealY, int inRefresh)
			: next (NULL),
			width (inX),
			height (inY),
			bits (inBits),
			refreshHz (inRefresh),
			realheight (inRealY)
		{}
		ModeInfo *next;
		int width, height, bits, refreshHz, realheight;
	} *m_Modes;

	ModeInfo *m_IteratorMode;
	int m_IteratorBits;
	bool m_IteratorFS;
	bool m_IsFullscreen;
	int m_trueHeight;
	int m_DisplayWidth, m_DisplayHeight, m_DisplayBits, m_DisplayHz;
	HMODULE hmRender;

	void MakeModesList();
	void AddMode(int x, int y, int bits, int baseHeight, int refreshHz);
	void FreeModes();
public:
	int GetTrueHeight() { return m_trueHeight; }

};



class Win32GLFrameBuffer : public BaseWinFB
{
	DECLARE_CLASS(Win32GLFrameBuffer, BaseWinFB)

public:
	Win32GLFrameBuffer() {}
	Win32GLFrameBuffer(int width, int height, int bits, int refreshHz, bool fullscreen);
	virtual ~Win32GLFrameBuffer();

	// unused but must be defined
	virtual void Blank ();
	virtual bool PaintToWindow ();
	virtual HRESULT GetHR();

	virtual bool CreateResources ();
	virtual void ReleaseResources ();

	void SetVSync (bool vsync);
	void NewRefreshRate ();


	int GetTrueHeight() { return static_cast<Win32GLVideo*>(Video)->GetTrueHeight(); }

	bool Lock(bool buffered);
	bool Lock ();
	void Unlock();
	bool IsLocked ();


	bool IsFullscreen();
	void PaletteChanged();
	int QueryNewPalette();

	void InitializeState();

protected:

	bool CanUpdate();
	void SetGammaTable(WORD * tbl);

	float m_Gamma, m_Brightness, m_Contrast;
	WORD m_origGamma[768];
	BOOL m_supportsGamma;
	bool m_Fullscreen;
	int m_Width, m_Height, m_Bits, m_RefreshHz;
	int m_Lock;

	friend class Win32GLVideo;

};

#endif //__WIN32GLIFACE_H__
