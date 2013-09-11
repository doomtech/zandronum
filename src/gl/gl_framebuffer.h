#ifndef __GL_FRAMEBUFFER
#define __GL_FRAMEBUFFER

#ifdef _WIN32
#include "win32iface.h"
#include "win32gliface.h"

namespace GLRendererOld
{
	class GLTexture;
	class FShader;
}

class OpenGLFrameBuffer : public Win32GLFrameBuffer
{
	typedef Win32GLFrameBuffer Super;
	DECLARE_CLASS(OpenGLFrameBuffer, Win32GLFrameBuffer)
#else
#include "sdlglvideo.h"
class OpenGLFrameBuffer : public SDLGLFB
{
//	typedef SDLGLFB Super;	//[C]commented, DECLARE_CLASS defines this in linux
	DECLARE_CLASS(OpenGLFrameBuffer, SDLGLFB)
#endif


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

	// Retrieves a buffer containing image data for a screenshot.
	// Hint: Pitch can be negative for upside-down images, in which case buffer
	// points to the last row in the buffer, which will be the first row output.
	virtual void GetScreenshotBuffer(const BYTE *&buffer, int &pitch, ESSType &color_type);

	// Releases the screenshot buffer.
	virtual void ReleaseScreenshotBuffer();

	// 2D drawing
	void STACK_ARGS DrawTextureV(FTexture *img, int x, int y, uint32 tag, va_list tags);
	void DrawLine(int x1, int y1, int x2, int y2, int palcolor, uint32 color);
	void DrawPixel(int x1, int y1, int palcolor, uint32 color);
	void Clear(int left, int top, int right, int bottom, int palcolor, uint32 color);
	void Dim(PalEntry color=0);
	void Dim (PalEntry color, float damount, int x1, int y1, int w, int h);
	void FlatFill (int left, int top, int right, int bottom, FTexture *src, bool local_origin=false);

	void PrecacheTexture(FTexture *tex, bool cache);

	FNativePalette *CreatePalette(FRemapTable *remap);

	void RenderView (player_t* player);
	void WriteSavePic (player_t *player, FILE *file, int width, int height);

	bool WipeStartScreen(int type);
	void WipeEndScreen();
	bool WipeDo(int ticks);
	void WipeCleanup();


private:
	PalEntry Flash;

	// Texture creation info
	int cm;
	int translation;
	bool iscomplex;

	PalEntry SourcePalette[256];
	BYTE *ScreenshotBuffer;

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
	GLRendererOld::GLTexture *wipestartscreen;
	GLRendererOld::GLTexture *wipeendscreen;

	AActor * LastCamera;

	void SetFixedColormap (player_t *player);

};

#include "common/glc_renderer.h"

class GL1Renderer : public GLRendererBase
{
	void Initialize();
	void SetPaused();
	void UnsetPaused();

	void DrawTexture(FTexture *img, DCanvas::DrawParms &parms);
	void DrawLine(int x1, int y1, int x2, int y2, int palcolor, uint32 color);
	void DrawPixel(int x1, int y1, int palcolor, uint32 color);
	void Dim(PalEntry color, float damount, int x1, int y1, int w, int h);
	void FlatFill (int left, int top, int right, int bottom, FTexture *src, bool local_origin);
	void Clear(int left, int top, int right, int bottom, int palcolor, uint32 color);

	void ProcessLowerMiniseg(seg_t *seg, sector_t * frontsector, sector_t * backsector);
	void ProcessWall(seg_t *, sector_t *, sector_t *, subsector_t *);
	void ProcessSprite(AActor *thing, sector_t *sector);
	void ProcessParticle(particle_t *part, sector_t *sector);
	void ProcessSector(sector_t *fakesector, subsector_t *sub);
	void FlushTextures();
	void RenderTextureView (FCanvasTexture *self, AActor *viewpoint, int fov);
	void PrecacheTexture(FTexture *tex);
	void UncacheTexture(FTexture *tex);
	unsigned char *GetTextureBuffer(FTexture *tex, int &w, int &h);
	void SetupLevel();
	void CleanLevelData();
};

#endif //__GL_FRAMEBUFFER
