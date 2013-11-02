#ifndef __SDLGLVIDEO_H__
#define __SDLGLVIDEO_H__

#include "hardware.h"
#include "v_video.h"
#include <SDL.h>
#include "gl/system/gl_system.h"

EXTERN_CVAR (Float, dimamount)
EXTERN_CVAR (Color, dimcolor)

class SDLGLVideo : public IVideo
{
 public:
	SDLGLVideo (int parm);
	~SDLGLVideo ();

	EDisplayType GetDisplayType () { return DISPLAY_Both; }
	void SetWindowedScale (float scale);

	DFrameBuffer *CreateFrameBuffer (int width, int height, bool fs, DFrameBuffer *old);

	void StartModeIterator (int bits, bool fs);
	bool NextMode (int *width, int *height, bool *letterbox);
	bool SetResolution (int width, int height, int bits);

private:
	int IteratorMode;
	int IteratorBits;
	bool IteratorFS;
};
class SDLGLFB : public DFrameBuffer
{
	DECLARE_CLASS(SDLGLFB, DFrameBuffer)
public:
	// this must have the same parameters as the Windows version, even if they are not used!
	SDLGLFB (void *hMonitor, int width, int height, int, int, bool fullscreen); 
	~SDLGLFB ();

	void ForceBuffering (bool force);
	bool Lock(bool buffered);
	bool Lock ();
	void Unlock();
	bool IsLocked ();

	bool IsValid ();
	bool IsFullscreen ();

	void NewRefreshRate ();

	friend class SDLGLVideo;

//[C]
	int GetTrueHeight() { return GetHeight();}

protected:
	bool CanUpdate();
	void SetGammaTable(WORD *tbl);
	void InitializeState();

	SDLGLFB () {}
	BYTE GammaTable[3][256];
	bool UpdatePending;
	
	SDL_Surface *Screen;
	
	void UpdateColors ();

	int m_Lock;
	Uint16 m_origGamma[3][256];
	bool m_supportsGamma;
};
#endif
