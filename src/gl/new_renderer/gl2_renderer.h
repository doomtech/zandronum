#ifndef __GL2_RENDERER
#define __GL2_RENDERER

#include "tarray.h"
#include "gl/common/glc_renderer.h"

namespace GLRendererNew
{
class FShaderContainer;
class FGLTextureManager;
class FMaterialContainer;
class FMaterial;
class FGLTexture;
class FPrimitiveBuffer2D;

class GL2Renderer : public GLRendererBase
{
public:
	FShaderContainer *mShaders;
	FGLTextureManager *mTextures;
	TArray<FMaterialContainer *> mMaterials;
	FPrimitiveBuffer2D *mRender2D;

	GL2Renderer() 
	{
		mShaders = NULL;
		mTextures = NULL;
		mRender2D = NULL;
	}
	~GL2Renderer();

	// renderer interface
	void Initialize();
	void SetPaused();
	void UnsetPaused();

	void Begin2D();
	void ClearBorders();
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

	void SetFixedColormap (player_t *player);
	void WriteSavePic (player_t *player, FILE *file, int width, int height);
	void RenderView (player_t* player);

	// renderer internal functions
	FGLTexture *GetGLTexture(FTexture *tex, bool asSprite, int translation);
	FMaterial *GetMaterial(FTexture *tex, bool asSprite, int translation);
	FMaterial *GetMaterial(FTextureID texindex, bool animtrans, bool asSprite, int translation);

};

// same as GLRenderer but with another type because this will be needed throughout the
// new renderer.
extern GL2Renderer *GLRenderer2;
}

#endif