#ifndef __GL2_RENDERER
#define __GL2_RENDERER

#include "tarray.h"
#include "gl/common/glc_renderer.h"
#include "gl/common/glc_renderhacks.h"
#include "gl/common/glc_templates.h"
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
struct GLDrawInfo;


class GL2Renderer : public GLRendererBase
{
public:
	FShaderContainer *mShaders;
	FGLTextureManager *mTextures;
	TArray<FMaterialContainer *> mMaterials;
	FPrimitiveBuffer2D *mRender2D;
	FMaterialContainer *mDefaultMaterial;
	FSkyDrawer *mSkyDrawer;
	GLDrawInfo *mGlobalDrawInfo;

	GLDrawInfo *mCurrentDrawInfo;

	TArray<FSectorRenderData> mSectorData;
	FreeList<GLDrawInfo> di_list;

	GL2Renderer() 
	{
		mShaders = NULL;
		mTextures = NULL;
		mRender2D = NULL;
		mDefaultMaterial = NULL;
		mSkyDrawer = NULL;
		mGlobalDrawInfo = NULL;
		mCurrentDrawInfo = NULL;
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
	void EndDrawScene(sector_t * viewsector);
	void SetupView(fixed_t viewx, fixed_t viewy, fixed_t viewz, angle_t viewangle);
	void ProcessScene();
	void Flush();

	void SetProjection(float fov, float ratio, float fovratio);
	void SetViewMatrix(bool mirror, bool planemirror);

	void CollectScene();
	void DrawScene();

	GLDrawInfo *StartDrawInfo(GLDrawInfo * di);
	void EndDrawInfo();

	// renderer internal functions
	FGLTexture *GetGLTexture(FTexture *tex, bool asSprite, int translation);
	FMaterial *GetMaterial(FTexture *tex, bool asSprite, int translation);
	FMaterial *GetMaterial(FTextureID texindex, bool animtrans, bool asSprite, int translation);
	FShader *GetShader(const char *name);
};

//==========================================================================
//
// The draw info for one viewpoint
//
//==========================================================================

struct GLDrawInfo : public FDrawInfo
{
	friend class GL2Renderer;

	FVector3 mViewpoint;

	bool temporary;

	GLDrawInfo * next;

	//void SetupFloodStencil(wallseg * ws) [;
	//void ClearFloodStencil(wallseg * ws);
	//void DrawFloodedPlane(wallseg * ws, float planez, sector_t * sec, bool ceiling);
	void FloodUpperGap(seg_t * seg) {}
	void FloodLowerGap(seg_t * seg) {}


public:
	TArray<FRenderObject*> mDrawLists[2];	// one for opaque, one for translucent
	void AddObject(FRenderObject *obj)
	{
		mDrawLists[obj->mAlpha].Push(obj);
	}

	GLDrawInfo()
	{
		temporary = false;
		next = NULL;
	}

	void StartScene()
	{
		FDrawInfo::StartScene();
		mDrawLists[0].Clear();
		mDrawLists[1].Clear();
	}

	void SetViewpoint(float x, float y, float z)
	{
		mViewpoint = FVector3(x,y,z);
	}


};


// same as GLRenderer but with another type because this will be needed throughout the
// new renderer.
extern GL2Renderer *GLRenderer2;
}

#endif
