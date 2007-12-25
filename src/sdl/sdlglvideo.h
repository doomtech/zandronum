#include "hardware.h"
#include "v_video.h"
#include <SDL.h>
#include "gl/gl_pch.h"

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
private:
	int IteratorMode;
	int IteratorBits;
	bool IteratorFS;
};
class SDLGLFB : public DFrameBuffer
{
	DECLARE_CLASS(SDLGLFB, DFrameBuffer)
public:
	SDLGLFB (int width, int height, bool fullscreen);
	~SDLGLFB ();

	void ForceBuffering (bool force);
	bool Lock(bool buffered);
	bool Lock ();
	void Unlock();
	bool IsLocked ();

	bool IsValid ();
	void Update ();
	PalEntry *GetPalette ();
	void GetFlashedPalette (PalEntry pal[256]);
	void UpdatePalette ();
	bool SetGamma (float gamma);
	bool SetFlash (PalEntry rgb, int amount);
	void GetFlash (PalEntry &rgb, int &amount);
	int GetPageCount ();
	bool IsFullscreen ();

	friend class SDLGLVideo;

//[C]
	int GetTrueHeight() { return GetHeight();}
	void Set2DMode();
	void Dim(PalEntry) const;
	void Dim(PalEntry, float, int, int, int, int) const;
	void FlatFill(int, int, int, int, FTexture*);
	void Clear(int, int, int, int, int, uint32) const;
	bool SetBrightness(float bright);
	bool SetContrast(float contrast);
	void DoSetGamma();
	void InitializeState();
private:
	PalEntry SourcePalette[256];
	BYTE GammaTable[3][256];
	bool UpdatePending;
	
	SDL_Surface *Screen;
	
	void UpdateColors ();

	SDLGLFB () {}

	int m_Lock;
	Uint16 m_origGamma[3][256];
	BOOL m_supportsGamma;
};
