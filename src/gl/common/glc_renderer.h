#ifndef __GL_RENDERER_H
#define __GL_RENDERER_H

#include "r_defs.h"
#include "v_video.h"
struct particle_t;
class FCanvasTexture;

enum SectorRenderFlags
{
	// This is used to avoid creating too many drawinfos
	SSRF_RENDERFLOOR=1,
	SSRF_RENDERCEILING=2,
	SSRF_RENDER3DPLANES=4,
	SSRF_RENDERALL=7,
	SSRF_PROCESSED=8,
};


class GLRendererBase
{
public:

	line_t * mirrorline;

	GLRendererBase() 
	{
		mirrorline = NULL;
	}
	~GLRendererBase() {}

	virtual void Initialize() = 0;
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
	virtual void SetupLevel() = 0;
	virtual void CleanLevelData() = 0;

	virtual void SetFixedColormap (player_t *player) = 0;
	virtual void WriteSavePic (player_t *player, FILE *file, int width, int height) = 0;
	virtual void RenderView (player_t* player) = 0;

};

// Global functions. Make them members of GLRendererBase later?
FTextureID gl_GetSpriteFrame(unsigned sprite, int frame, int rot, angle_t angle, bool *mirror);
void gl_RenderBSPNode (void *node);
bool gl_CheckClip(side_t * sidedef, sector_t * frontsector, sector_t * backsector);
void gl_CheckViewArea(vertex_t *v1, vertex_t *v2, sector_t *frontsector, sector_t *backsector);
sector_t * gl_FakeFlat(sector_t * sec, sector_t * dest, bool back);

void gl_GetRenderStyle(FRenderStyle style, bool drawopaque, bool allowcolorblending,
					   int *tm, int *sb, int *db, int *be);


typedef enum
{
        area_normal,
        area_below,
        area_above,
		area_default
} area_t;

extern area_t			in_area;

struct TexFilter_s
{
	int minfilter;
	int magfilter;
	bool mipmapping;
} ;



extern GLRendererBase *GLRenderer;

#endif