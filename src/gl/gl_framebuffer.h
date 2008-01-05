#ifndef __GL_FRAMEBUFFER
#define __GL_FRAMEBUFFER

#ifdef _WIN32
#include "win32iface.h"
#include "win32gliface.h"

class GLTexture;
class FShader;

class OpenGLFrameBuffer : public Win32GLFrameBuffer
{
	typedef Win32GLFrameBuffer Super;
#else
#include "sdlglvideo.h"
class OpenGLFrameBuffer : public SDLGLFB
{
	typedef SDLGLFB Super;
#endif

	DECLARE_CLASS(OpenGLFrameBuffer, Win32GLFrameBuffer)

public:

	explicit OpenGLFrameBuffer() {}
	OpenGLFrameBuffer(int width, int height, int bits, int refreshHz, bool fullscreen) ;
	~OpenGLFrameBuffer();

	void InitializeState();
	void Update();

	// Color correction
	bool SetGamma (float gamma);
	bool SetBrightness(float bright);
	bool SetContrast(float contrast);
	void DoSetGamma();
	bool UsesColormap() const;

	void UpdatePalette();
	void GetFlashedPalette (PalEntry pal[256]);
	PalEntry *GetPalette ();
	bool SetFlash(PalEntry rgb, int amount);
	void GetFlash(PalEntry &rgb, int &amount);
	int GetPageCount();
	bool Begin2D(bool copy3d);

	// Saves canvas to a file
	bool CanWritePCX();
	void SaveFile(const char *filename, bool savepcx);

	// 2D drawing
	void STACK_ARGS DrawTextureV(FTexture *img, int x, int y, uint32 tag, va_list tags);
	void DrawLine(int x1, int y1, int x2, int y2, int palcolor, uint32 color);
	void DrawPixel(int x1, int y1, int palcolor, uint32 color);
	void Clear(int left, int top, int right, int bottom, int palcolor, uint32 color);
	void Dim(PalEntry color=0);
	void Dim (PalEntry color, float damount, int x1, int y1, int w, int h);
	void FlatFill (int left, int top, int right, int bottom, FTexture *src);

	// texture copy functions
	virtual void CopyPixelDataRGB(BYTE * buffer, int texwidth, int texheight, int originx, int originy,
					     const BYTE * patch, int pix_width, int pix_height, int step_x, int step_y,
						 int ct);

	virtual void CopyPixelData(BYTE * buffer, int texwidth, int texheight, int originx, int originy,
					  const BYTE * patch, int pix_width, int pix_height, 
					  int step_x, int step_y, PalEntry * palette);

	FNativeTexture *CreatePalette(FRemapTable *remap);


	void SetTranslationInfo(int _cm, int _trans=-1337)
	{
		if (_cm != -1) cm = _cm;
		if (_trans != -1337) translation = _trans;
	}

	bool WipeStartScreen(int type);
	void WipeEndScreen();
	bool WipeDo(int ticks);
	void WipeCleanup();


private:
	PalEntry Flash;
	int cm;
	int translation;
	PalEntry SourcePalette[256];

	class Wiper
	{
	public:
		virtual ~Wiper();
		virtual bool Run(int ticks, OpenGLFrameBuffer *fb) = 0;
	};

	class Wiper_Melt;			friend class Wiper_Melt;
	class Wiper_Burn;			friend class Wiper_Burn;
	class Wiper_Crossfade;		friend class Wiper_Crossfade;

	Wiper *ScreenWipe;
	GLTexture *wipestartscreen;
	GLTexture *wipeendscreen;
	bool GatheringWipeScreen;

};

inline void SetTranslationInfo(int _cm, int _trans=-1337)
{
	static_cast<OpenGLFrameBuffer*>(screen)->SetTranslationInfo(_cm,_trans);
}


#endif //__GL_FRAMEBUFFER
