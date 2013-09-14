#ifndef __GL1_RENDERER
#define __GL1_RENDERER

#include "gl/common/glc_renderer.h"
#include "gl/common/glc_structs.h"

struct GL_IRECT;

namespace GLRendererOld
{

class GL1Renderer : public GLRendererBase
{
	~GL1Renderer();

	void RenderTextureView(FCanvasTexture *Texture, AActor * Viewpoint, int FOV);

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
	void PrecacheTexture(FTexture *tex);
	void UncacheTexture(FTexture *tex);
	unsigned char *GetTextureBuffer(FTexture *tex, int &w, int &h);
	void SetupLevel();
	void CleanLevelData();

	void SetFixedColormap (player_t *player);
	void WriteSavePic (player_t *player, FILE *file, int width, int height);
	void RenderMainView (player_t *player, float fov, float ratio, float fovratio);
	void ProcessScene();

	void SetProjection(float fov, float ratio, float fovratio);
	void SetViewMatrix(bool mirror, bool planemirror);

};

// textures + sprites

extern DWORD gl_fixedcolormap;

class FGLTexture;


void gl_SetPlaneTextureRotation(const GLSectorPlane * secplane, FGLTexture * gltexture);
void gl_ClearShaders();
void gl_EnableShader(bool on);

void gl_SetTextureMode(int which);
void gl_EnableFog(bool on);
void gl_SetShaderLight(float level, float factor);
void gl_SetCamera(float x, float y, float z);

void gl_SetGlowParams(float *topcolors, float topheight, float *bottomcolors, float bottomheight);
void gl_SetGlowPosition(float topdist, float bottomdist);

void gl_SetTextureShader(int warped, int cm, bool usebright, float warptime);

void gl_ApplyShader();

void gl_RecalcVertexHeights(vertex_t * v);
void gl_InitVertexData();
void gl_CleanVertexData();

void gl_SetupView(fixed_t viewx, fixed_t viewy, fixed_t viewz, angle_t viewangle, bool mirror, bool planemirror);
void gl_DrawScene();
void gl_EndDrawScene();
sector_t * gl_RenderView (AActor * camera, GL_IRECT * bounds, float fov, float ratio, bool mainview);
void gl_SetRenderStyle(FRenderStyle style, bool drawopaque, bool allowcolorblending);

}

#endif