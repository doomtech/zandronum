#ifndef __GL_FUNCT
#define __GL_FUNCT

#include "doomtype.h"
#include "templates.h"

class FArchive;

class AActor;
class FTexture;
class FFont;
struct GLSectorPlane;
struct sector_t;
class player_s;
struct GL_IRECT;
class FGLTexture;
class FCanvasTexture;
class GLWall;
struct texcoord;
struct MapData;
extern DWORD gl_fixedcolormap;

// gl_data.cpp

void gl_CheckNodes(MapData * map);
bool gl_LoadGLNodes(MapData * map);
void gl_InitData();
void gl_CleanLevelData();
void gl_PreprocessLevel(void);

// Light + color

void gl_CalcLightTable(int usegamma);

extern float lighttable[256];
inline int gl_CalcLightLevel(int lightlevel) 
{
	return clamp<int>(lightlevel,0, 255);
}

inline bool gl_isBlack(PalEntry color)
{
	return color.r + color.g + color.b == 0;
}

inline bool gl_isWhite(PalEntry color)
{
	return color.r + color.g + color.b == 3*0xff;
}

inline bool gl_isFullbright(PalEntry color, int lightlevel)
{
	return gl_fixedcolormap || (gl_isWhite(color) && lightlevel==255);
}

struct FColormap;
void gl_GetLightColor(int lightlevel, int rellight, const FColormap * cm, float * pred, float * pgreen, float * pblue, bool weapon=false);
void gl_SetColor(int light, int rellight, const FColormap * cm, float alpha, PalEntry ThingColor = 0xffffff, bool weapon=false);

void gl_GetSpriteLight(fixed_t x, fixed_t y, fixed_t z, subsector_t * subsec, int desaturation, float * out);
void gl_SetSpriteLight(AActor * thing, int lightlevel, int rellight, FColormap * cm, float alpha, PalEntry ThingColor = 0xffffff, bool weapon=false);

struct particle_t;
void gl_SetSpriteLight(particle_t * thing, int lightlevel, int rellight, FColormap *cm, float alpha, PalEntry ThingColor = 0xffffff);

void gl_InitFog();
void gl_SetFogParams(int _fogdensity, PalEntry _outsidefogcolor, int _outsidefogdensity, int _skyfog);
float gl_GetFogDensity(int lightlevel, PalEntry fogcolor);
void gl_SetFog(int lightlevel, PalEntry pe, int renderstyle);

// textures + sprites

void gl_SetPlaneTextureRotation(const GLSectorPlane * secplane, FGLTexture * gltexture);

void gl_ClearShaders();
void gl_InitShaders();
void gl_EnableShader(bool on);

void gl_EnableFog(bool on);
void gl_SetCamera(float x, float y, float z);
void gl_SetFogLight(int lightlevel);
bool gl_SetColorMode(int cm, bool force=false);
bool gl_SetShaderForWarp(int type, float time);
void gl_EnableTexture(bool on);


int gl_GetSpriteFrame(unsigned sprite, int frame, int rot, angle_t angle=0);


#ifndef max
#define max(a,b) ((a)>(b)?(a):(b))
#endif
#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#endif

// Render

void gl_RenderBSPNode (void *node);
sector_t * gl_FakeFlat(sector_t * sec, sector_t * dest, bool back);

#define INVALID_SPRITE 0xffffff

// Data

void SaveGFX(const char * fn, unsigned char * buffer, int w, int h);
PalEntry averageColor(const unsigned long *data, int size, bool maxout);
void gl_ScreenShot(const char *filename);
void gl_EnableShader(bool on);
bool gl_SetShader(int cmap);
void gl_Init(int width, int height);

void gl_DrawLine(int x0, int y0, int x1, int y1, int BaseColor);

struct FTexInfo
{
   FTexture *tex;
   FFont * font;
   float x;
   float y;
   float width;
   float height;
   int clipLeft, clipRight, clipTop, clipBottom;
   int windowLeft, windowRight;
   const BYTE *translation;
   bool loadAlpha, flipX, masked;
   float alpha;
   int fillColor;
};

void gl_DrawTexture(FTexInfo *texInfo);
void gl_DrawBuffer(BYTE * buffer, int width, int height, int x, int y, int dx, int dy, PalEntry * palette);
void gl_DrawCanvas(DCanvas * canvas, int x, int y, int dx, int dy, PalEntry * palette);
void gl_DrawSavePic(DCanvas * canvas, const char * Filename, int x, int y, int dx, int dy);


// Scene

void gl_ClearScreen();
void gl_SetupView(fixed_t viewx, fixed_t viewy, fixed_t viewz, angle_t viewangle, bool mirror, bool planemirror, bool nosectorclear=false);
void gl_SetViewArea();
void gl_DrawScene();
void gl_EndDrawScene();
sector_t * gl_RenderView (AActor * camera, GL_IRECT * bounds, float fov, float ratio, bool mainview);
void gl_RenderPlayerView(player_s *player);   // Called by G_Drawer.
void gl_RenderViewToCanvas(DCanvas * pic, int x, int y, int width, int height);
void gl_RenderTextureView(FCanvasTexture *Texture, AActor * Viewpoint, int FOV);
angle_t gl_FrustumAngle();

void gl_LinkLights();


// ZDBSP shittiness compensation
void gl_CollectMissingLines();
void gl_RenderMissingLines();


void gl_SetActorLights(AActor *);
void gl_DeleteAllAttachedLights();
void gl_RecreateAllAttachedLights();
void gl_ParseDefs();


void gl_SplitLeftEdge(GLWall * wall, texcoord * tcs);
void gl_SplitRightEdge(GLWall * wall, texcoord * tcs);
void gl_RecalcVertexHeights(vertex_t * v);
void gl_InitVertexData();
void gl_CleanVertexData();

inline float Dist2(float x1,float y1,float x2,float y2)
{
	return sqrtf((x1-x2)*(x1-x2)+(y1-y2)*(y1-y2));
}


void I_RestartRenderer();
void I_CheckRestartRenderer();

EXTERN_CVAR(Int, screenblocks)

#endif
