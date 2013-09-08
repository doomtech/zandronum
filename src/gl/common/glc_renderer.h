#ifndef __GL_RENDERER_H
#define __GL_RENDERER_H

#include "r_defs.h"
struct particle_t;
class FCanvasTexture;

class GLRendererBase
{
public:

	line_t * mirrorline;

	GLRendererBase() 
	{
		mirrorline = NULL;
	}
	~GLRendererBase() {}

	virtual void ProcessWall(seg_t *, sector_t *, sector_t *, subsector_t *) = 0;
	virtual void ProcessSprite(AActor *thing, sector_t *sector) = 0;
	virtual void ProcessParticle(particle_t *part, sector_t *sector) = 0;
	virtual void ProcessSector(sector_t *fakesector, subsector_t *sub) = 0;
	virtual void FlushTextures() = 0;
	virtual void RenderTextureView (FCanvasTexture *self, AActor *viewpoint, int fov) = 0;
	virtual void PrecacheTexture(FTexture *tex) = 0;
	virtual void UncacheTexture(FTexture *tex) = 0;
	virtual unsigned char *GetTextureBuffer(FTexture *tex, int &w, int &h) = 0;
};

// Global functions. Make them members of GLRendererBase later?
FTextureID gl_GetSpriteFrame(unsigned sprite, int frame, int rot, angle_t angle, bool *mirror);
void gl_RenderBSPNode (void *node);
bool gl_CheckClip(side_t * sidedef, sector_t * frontsector, sector_t * backsector);
void gl_CheckViewArea(vertex_t *v1, vertex_t *v2, sector_t *frontsector, sector_t *backsector);
sector_t * gl_FakeFlat(sector_t * sec, sector_t * dest, bool back);

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