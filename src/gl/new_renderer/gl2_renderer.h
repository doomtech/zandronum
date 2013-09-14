#ifndef __GL2_RENDERER
#define __GL2_RENDERER

#include "tarray.h"
#include "gl/common/glc_renderer.h"
#include "gl/new_renderer/gl2_geom.h"

namespace GLRendererNew
{
class FShaderContainer;
class FGLTextureManager;
class FMaterialContainer;
class FMaterial;
class FShader;
class FGLTexture;
class FPrimitiveBuffer2D;
class FSkyDrawer;
struct FGLSubsectorData;


class GL2Renderer : public GLRendererBase
{
public:
	FShaderContainer *mShaders;
	FGLTextureManager *mTextures;
	TArray<FMaterialContainer *> mMaterials;
	FPrimitiveBuffer2D *mRender2D;
	FMaterialContainer *mDefaultMaterial;
	FSkyDrawer *mSkyDrawer;

	TArray<FGLSectorRenderData> mSectorData;

	GL2Renderer() 
	{
		mShaders = NULL;
		mTextures = NULL;
		mRender2D = NULL;
		mDefaultMaterial = NULL;
		mSkyDrawer = NULL;
	}
	~GL2Renderer();

	sector_t *RenderView (AActor * camera, GL_IRECT * bounds, float fov, float ratio, float fovratio, bool mainview);

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
	void RenderMainView (player_t *player, float fov, float ratio, float fovratio);
	void SetupView(fixed_t viewx, fixed_t viewy, fixed_t viewz, angle_t viewangle);
	void ProcessScene();
	void Flush();

	void InvalidateSector(sector_t *sec, int mode);
	void InvalidateSidedef(side_t *side, int mode);

	void SetProjection(float fov, float ratio, float fovratio);
	void SetViewMatrix(bool mirror, bool planemirror);

	// renderer internal functions
	FGLTexture *GetGLTexture(FTexture *tex, bool asSprite, int translation);
	FMaterial *GetMaterial(FTexture *tex, bool asSprite, int translation);
	FMaterial *GetMaterial(FTextureID texindex, bool animtrans, bool asSprite, int translation);
	FShader *GetShader(const char *name);
};

// same as GLRenderer but with another type because this will be needed throughout the
// new renderer.
extern GL2Renderer *GLRenderer2;
}

#endif