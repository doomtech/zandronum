/*
** win32iface.h
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

#ifndef	__WIN32_IFACE_H__
#define	__WIN32_IFACE_H__

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ddraw.h>

#include "hardware.h"
#include "v_video.h"

class Win32Video : public IVideo
{
 public:
	Win32Video (int parm);
	~Win32Video ();

	EDisplayType GetDisplayType () { return DISPLAY_Both; }
	bool FullscreenChanged (bool fs);
	void SetWindowedScale (float scale);

	DFrameBuffer *CreateFrameBuffer (int width, int height, bool fs, DFrameBuffer *old);

	void StartModeIterator (int bits);
	bool NextMode (int *width, int *height, bool *letterbox);

	bool GoFullscreen (bool yes);
	void BlankForGDI ();

 private:
	struct ModeInfo
	{
		ModeInfo (int inX, int inY, int inBits, int inRealY)
			: next (NULL),
			  width (inX),
			  height (inY),
			  bits (inBits),
			  realheight (inRealY)
		{}

		ModeInfo *next;
		int width, height, bits;
		int realheight;
	} *m_Modes;

	ModeInfo *m_IteratorMode;
	int m_IteratorBits;
	bool m_IteratorFS;
	bool m_IsFullscreen;

	bool m_CalledCoInitialize;

	void AddMode (int x, int y, int bits, int baseHeight);
	void FreeModes ();

	void NewDDMode (int x, int y);
	static HRESULT WINAPI EnumDDModesCB (LPDDSURFACEDESC desc, void *modes);

	friend class DDrawFB;
};

class BaseWinFB : public DFrameBuffer
{
public:
	BaseWinFB (int width, int height) : DFrameBuffer (width, height), Windowed (true) {}

	bool IsFullscreen () { return !Windowed; }
	virtual void Blank () = 0;
	virtual bool PaintToWindow () = 0;

protected:
	virtual bool CreateResources () = 0;
	virtual void ReleaseResources () = 0;

	bool Windowed;

	friend int I_PlayMovie (const char *name);
	friend class Win32Video;
};

class DDrawFB : public BaseWinFB
{
public:
	DDrawFB (int width, int height, bool fullscreen);
	~DDrawFB ();

	bool IsValid ();
	bool Lock ();
	bool Lock (bool buffer);
	void Unlock ();
	void ForceBuffering (bool force);
	void Update ();
	PalEntry *GetPalette ();
	void GetFlashedPalette (PalEntry pal[256]);
	void UpdatePalette ();
	bool SetGamma (float gamma);
	bool SetFlash (PalEntry rgb, int amount);
	void GetFlash (PalEntry &rgb, int &amount);
	int GetPageCount ();
	int QueryNewPalette ();
	void PaletteChanged ();
	HRESULT GetHR () { return LastHR; }

	void Blank ();
	bool PaintToWindow ();
	void DoSpeedTest ();

private:
	enum LockSurfRes { NoGood, Good, GoodWasLost };
	
	bool CreateResources ();
	void ReleaseResources ();
	bool CreateSurfacesAttached ();
	bool CreateSurfacesComplex ();
	bool CreateBlitterSource ();
	LockSurfRes LockSurf (LPRECT lockrect, LPDIRECTDRAWSURFACE surf);
	void RebuildColorTable ();
	void MaybeCreatePalette ();
	bool AddBackBuf (LPDIRECTDRAWSURFACE *surface, int num);
	HRESULT AttemptRestore ();

	HRESULT LastHR;
	BYTE GammaTable[3][256];
	PalEntry SourcePalette[256];
	PALETTEENTRY PalEntries[256];

	LPDIRECTDRAWPALETTE Palette;
	LPDIRECTDRAWSURFACE PrimarySurf;
	LPDIRECTDRAWSURFACE BackSurf;
	LPDIRECTDRAWSURFACE BackSurf2;
	LPDIRECTDRAWSURFACE BlitSurf;
	LPDIRECTDRAWSURFACE LockingSurf;
	LPDIRECTDRAWCLIPPER Clipper;
	//IDirectDrawGammaControl *GammaControl;
	HPALETTE GDIPalette;
	BYTE *ClipRegion;
	DWORD ClipSize;
	PalEntry Flash;
	int FlashAmount;
	int BufferCount;
	int BufferPitch;
	int TrueHeight;
	float Gamma;

	bool NeedGammaUpdate;
	bool NeedPalUpdate;
	bool NeedResRecreate;
	bool MustBuffer;		// The screen is not 8-bit, or there is no backbuffer
	bool BufferingNow;		// Most recent Lock was buffered
	bool WasBuffering;		// Second most recent Lock was buffered
	bool Write8bit;
	bool UpdatePending;		// On final unlock, call Update()
	bool UseBlitter;		// Use blitter to copy from sys mem to video mem
	bool UsePfx;
};

#endif
