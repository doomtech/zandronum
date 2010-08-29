#ifndef __GL_FUNCT
#define __GL_FUNCT

#include "doomtype.h"
#include "templates.h"
#include "m_fixed.h"
#include "tables.h"
#include "textures/textures.h"

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
struct FColormap;
struct particle_t;
struct subsector_t;
struct vertex_t;
class DCanvas;
union FRenderStyle;
struct side_t;

extern DWORD gl_fixedcolormap;

// gl_data.cpp

void gl_CheckNodes(MapData * map);
bool gl_LoadGLNodes(MapData * map);
void gl_InitData();
void gl_CleanLevelData();
void gl_PreprocessLevel(void);
void gl_AddMapinfoParser();

// Light + color

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

void gl_GetLightColor(int lightlevel, int rellight, const FColormap * cm, float * pred, float * pgreen, float * pblue, bool weapon=false);
void gl_SetColor(int light, int rellight, const FColormap * cm, float alpha, PalEntry ThingColor = 0xffffff, bool weapon=false);
void gl_SetColor(int light, int rellight, const FColormap * cm, float *red, float *green, float *blue, PalEntry ThingColor=0xffffff, bool weapon=false);

void gl_GetSpriteLight(AActor *Self, fixed_t x, fixed_t y, fixed_t z, subsector_t * subsec, int desaturation, float * out);
void gl_SetSpriteLight(AActor * thing, int lightlevel, int rellight, FColormap * cm, float alpha, PalEntry ThingColor = 0xffffff, bool weapon=false);

void gl_GetSpriteLight(AActor * thing, int lightlevel, int rellight, FColormap * cm,
					   float *red, float *green, float *blue,
					   PalEntry ThingColor, bool weapon);

void gl_SetRenderStyle(FRenderStyle style, bool drawopaque, bool allowcolorblending);
void gl_SetSpriteLighting(FRenderStyle style, AActor *thing, int lightlevel, int rellight, FColormap *cm, 
						  PalEntry ThingColor, float alpha, bool fullbright, bool weapon);


void gl_SetSpriteLight(particle_t * thing, int lightlevel, int rellight, FColormap *cm, float alpha, PalEntry ThingColor = 0xffffff);

void gl_InitFog();
void gl_SetFogParams(int _fogdensity, PalEntry _outsidefogcolor, int _outsidefogdensity, int _skyfog);
float gl_GetFogDensity(int lightlevel, PalEntry fogcolor);
void gl_SetFog(int lightlevel, int rellight, const FColormap *cm, bool isadditive);

// [BB] Get value of gl_lightmode, respecting DF2_FORCE_GL_DEFAULTS.
int gl_GetLightMode ( );

// [BB] This construction purposely overrides the CVAR gl_fogmode with a local variable of the same name.
// This allows to implement DF2_FORCE_GL_DEFAULTS by only putting this define at the beginning of a function
// that uses gl_fogmode without any further changes in that function.
#define OVERRIDE_FOGMODE_IF_NECESSARY \
	const int gl_fogmode_CVAR_value = gl_fogmode; \
	const int gl_fogmode = ( ( dmflags2 & DF2_FORCE_GL_DEFAULTS ) && ( gl_fogmode_CVAR_value == 0 ) ) ? 1 : gl_fogmode_CVAR_value;

// textures + sprites

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

void gl_EnableTexture(bool on);



FTextureID gl_GetSpriteFrame(unsigned sprite, int frame, int rot, angle_t angle, bool *mirror);


// Render

void gl_RenderBSPNode (void *node);

bool gl_CheckClip(side_t * sidedef, sector_t * frontsector, sector_t * backsector);
void gl_CheckViewArea(vertex_t *v1, vertex_t *v2, sector_t *frontsector, sector_t *backsector);
sector_t * gl_FakeFlat(sector_t * sec, sector_t * dest, bool back);

#define INVALID_SPRITE 0xffffff


// Scene

void gl_SetupView(fixed_t viewx, fixed_t viewy, fixed_t viewz, angle_t viewangle, bool mirror, bool planemirror, bool nosectorclear=false);
void gl_SetViewArea();
void gl_DrawScene();
void gl_EndDrawScene();
sector_t * gl_RenderView (AActor * camera, GL_IRECT * bounds, float fov, float ratio, bool mainview);
void gl_RenderViewToCanvas(DCanvas * pic, int x, int y, int width, int height);
void gl_RenderTextureView(FCanvasTexture *Texture, AActor * Viewpoint, int FOV);
angle_t gl_FrustumAngle();

void gl_LinkLights();


void gl_SetActorLights(AActor *);
void gl_DeleteAllAttachedLights();
void gl_RecreateAllAttachedLights();
void gl_ParseDefs();


void gl_SplitLeftEdge(GLWall * wall, texcoord * tcs, bool glow);
void gl_SplitRightEdge(GLWall * wall, texcoord * tcs, bool glow);
void gl_RecalcVertexHeights(vertex_t * v);
void gl_InitVertexData();
void gl_CleanVertexData();

inline float Dist2(float x1,float y1,float x2,float y2)
{
	return sqrtf((x1-x2)*(x1-x2)+(y1-y2)*(y1-y2));
}


__forceinline void gl_InverseMap(int gray, BYTE & red, BYTE & green, BYTE & blue)
{
	red = green = blue = clamp<int>(255-gray,0,255);
}

__forceinline void gl_GoldMap(int gray, BYTE & red, BYTE & green, BYTE & blue)
{
	red=clamp<int>(gray+(gray>>1),0,255);
	green=clamp<int>(gray-(gray>>2),0,255);
	blue=0;
}

__forceinline void gl_RedMap(int gray, BYTE & red, BYTE & green, BYTE & blue)
{
	red=clamp<int>(gray+(gray>>1),0,255);
	green=0;
	blue=0;
}

__forceinline void gl_GreenMap(int gray, BYTE & red, BYTE & green, BYTE & blue)
{
	red=clamp<int>(gray+(gray>>1),0,255);
	green=clamp<int>(gray+(gray>>1),0,255);
	blue=gray;
}

__forceinline void gl_Desaturate(int gray, int ired, int igreen, int iblue, BYTE & red, BYTE & green, BYTE & blue, int fac)
{
	red = (ired*(31-fac) + gray*fac)/31;
	green = (igreen*(31-fac) + gray*fac)/31;
	blue = (iblue*(31-fac) + gray*fac)/31;
}

void gl_ModifyColor(BYTE & red, BYTE & green, BYTE & blue, int cm);
PalEntry averageColor(const DWORD *data, int size, fixed_t maxout);


extern int currentrenderer;

#endif
