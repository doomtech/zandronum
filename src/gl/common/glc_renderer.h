#ifndef __GL_RENDERER_H
#define __GL_RENDERER_H

#include "r_defs.h"
#include "v_video.h"
#include "vectors.h"

struct particle_t;
class FCanvasTexture;
class FVertexBuffer;

enum SectorRenderFlags
{
	// This is used to avoid creating too many drawinfos
	SSRF_RENDERFLOOR=1,
	SSRF_RENDERCEILING=2,
	SSRF_RENDER3DPLANES=4,
	SSRF_RENDERALL=7,
	SSRF_PROCESSED=8,
};

struct GL_IRECT
{
	int left,top;
	int width,height;


	void Offset(int xofs,int yofs)
	{
		left+=xofs;
		top+=yofs;
	}
};


class GLRendererBase
{
public:

	line_t * mirrorline;
	int mMirrorCount;
	int mPlaneMirrorCount;
	bool mLightCount;
	float mCurrentFoV;
	AActor *mViewActor;

	float mSky1Pos, mSky2Pos;

	FRotator mAngles;
	FVector2 mViewVector;
	FVector3 mCameraPos;

	FVertexBuffer *mVBO;


	GLRendererBase() 
	{
		mirrorline = NULL;
		mMirrorCount = 0;
		mPlaneMirrorCount = 0;
		mLightCount = 0;
		mAngles = FRotator(0,0,0);
		mViewVector = FVector2(0,0);
		mCameraPos = FVector3(0,0,0);
		mVBO = NULL;
	}
	virtual ~GLRendererBase() ;

	angle_t FrustumAngle();
	void SetViewArea();
	void SetViewport(GL_IRECT *bounds);
	sector_t *RenderViewpoint (AActor * camera, GL_IRECT * bounds, float fov, float ratio, float fovratio, bool mainview);
	void RenderView(player_t *player);
	void SetCameraPos(fixed_t viewx, fixed_t viewy, fixed_t viewz, angle_t viewangle);
	void SetCameraPos(const FVector3 &viewvec, angle_t viewangle);

	virtual void Initialize();
	virtual void SetPaused() = 0;
	virtual void UnsetPaused() = 0;

	virtual void Begin2D() = 0;
	virtual void ClearBorders() = 0;
	virtual void DrawTexture(FTexture *img, DCanvas::DrawParms &parms) = 0;
	virtual void DrawLine(int x1, int y1, int x2, int y2, int palcolor, uint32 color) = 0;
	virtual void DrawPixel(int x1, int y1, int palcolor, uint32 color) = 0;
	virtual void Dim(PalEntry color, float damount, int x1, int y1, int w, int h) = 0;
	virtual void FlatFill (int left, int top, int right, int bottom, FTexture *src, bool local_origin) = 0;
	virtual void Clear(int left, int top, int right, int bottom, int palcolor, uint32 color) = 0;

	virtual void ProcessLowerMiniseg(seg_t *seg, sector_t * frontsector, sector_t * backsector) = 0;
	virtual void ProcessWall(seg_t *, sector_t *, sector_t *, subsector_t *) = 0;
	virtual void ProcessSprite(AActor *thing, sector_t *sector) = 0;
	virtual void ProcessParticle(particle_t *part, sector_t *sector) = 0;
	virtual void ProcessSector(sector_t *fakesector, subsector_t *sub) = 0;
	virtual void FlushTextures() = 0;
	virtual void RenderTextureView (FCanvasTexture *self, AActor *viewpoint, int fov) = 0;
	virtual void PrecacheTexture(FTexture *tex) = 0;
	virtual void UncacheTexture(FTexture *tex) = 0;
	virtual unsigned char *GetTextureBuffer(FTexture *tex, int &w, int &h) = 0;
	void SetupLevel();

	virtual void SetFixedColormap (player_t *player) = 0;
	virtual void WriteSavePic (player_t *player, FILE *file, int width, int height) = 0;
	virtual void EndDrawScene(sector_t * viewsector) = 0;
	virtual void Flush() {}

	virtual void SetProjection(float fov, float ratio, float fovratio) = 0;
	virtual void SetViewMatrix(bool mirror, bool planemirror) = 0;
	virtual void ProcessScene() = 0;

};

// Global functions. Make them members of GLRendererBase later?
FTextureID gl_GetSpriteFrame(unsigned sprite, int frame, int rot, angle_t angle, bool *mirror);
void gl_RenderBSPNode (void *node);
bool gl_CheckClip(side_t * sidedef, sector_t * frontsector, sector_t * backsector);
void gl_CheckViewArea(vertex_t *v1, vertex_t *v2, sector_t *frontsector, sector_t *backsector);

typedef enum
{
        area_normal,
        area_below,
        area_above,
		area_default
} area_t;

extern area_t			in_area;


sector_t * gl_FakeFlat(sector_t * sec, sector_t * dest, area_t in_area, bool back);
inline sector_t * gl_FakeFlat(sector_t * sec, sector_t * dest, bool back)
{
	return gl_FakeFlat(sec, dest, in_area, back);
}

void gl_GetRenderStyle(FRenderStyle style, bool drawopaque, bool allowcolorblending,
					   int *tm, int *sb, int *db, int *be);
void gl_SetFogParams(int _fogdensity, PalEntry _outsidefogcolor, int _outsidefogdensity, int _skyfog);
int gl_CalcLightLevel(int lightlevel, int rellight, bool weapon);
PalEntry gl_CalcLightColor(int lightlevel, PalEntry pe, int blendfactor);
float gl_GetFogDensity(int lightlevel, PalEntry fogcolor);


struct TexFilter_s
{
	int minfilter;
	int magfilter;
	bool mipmapping;
} ;


extern GLRendererBase *GLRenderer;

#endif