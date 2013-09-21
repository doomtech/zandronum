
#ifndef __GL_LIGHTS_H__
#define __GL_LIGHTS_H__

#include "gl/gl_intern.h"

#include "gl/data/gl_data.h"
#include "gl/dynlights/gl_dynlight.h"
#include "gl/utility/gl_geometric.h"

//
// Light helper methods
//


extern unsigned int frameStartMS;

class AActor;
class FTexture;
class FFont;
struct sector_t;
class player_s;
struct GL_IRECT;
class FCanvasTexture;
struct texcoord;
struct MapData;
struct FColormap;
struct particle_t;
struct subsector_t;
struct vertex_t;
class DCanvas;
union FRenderStyle;
struct side_t;


// Light + color

void gl_GetLightColor(int lightlevel, int rellight, const FColormap * cm, float * pred, float * pgreen, float * pblue, bool weapon=false);
void gl_SetColor(int light, int rellight, const FColormap * cm, float alpha, PalEntry ThingColor = 0xffffff, bool weapon=false);
void gl_SetColor(int light, int rellight, const FColormap * cm, float *red, float *green, float *blue, PalEntry ThingColor=0xffffff, bool weapon=false);

void gl_GetSpriteLight(AActor *Self, fixed_t x, fixed_t y, fixed_t z, subsector_t * subsec, int desaturation, float * out);
void gl_SetSpriteLight(AActor * thing, int lightlevel, int rellight, FColormap * cm, float alpha, PalEntry ThingColor = 0xffffff, bool weapon=false);

void gl_GetSpriteLight(AActor * thing, int lightlevel, int rellight, FColormap * cm,
					   float *red, float *green, float *blue,
					   PalEntry ThingColor, bool weapon);

void gl_SetSpriteLighting(FRenderStyle style, AActor *thing, int lightlevel, int rellight, FColormap *cm, 
						  PalEntry ThingColor, float alpha, bool fullbright, bool weapon);


void gl_SetSpriteLight(particle_t * thing, int lightlevel, int rellight, FColormap *cm, float alpha, PalEntry ThingColor = 0xffffff);

void gl_SetFog(int lightlevel, int rellight, const FColormap *cm, bool isadditive);

inline bool gl_isBlack(PalEntry color)
{
	return color.r + color.g + color.b == 0;
}

inline bool gl_isWhite(PalEntry color)
{
	return color.r + color.g + color.b == 3*0xff;
}

extern DWORD gl_fixedcolormap;

inline bool gl_isFullbright(PalEntry color, int lightlevel)
{
	return gl_fixedcolormap || (gl_isWhite(color) && lightlevel==255);
}


bool gl_SetupLight(Plane & p, ADynamicLight * light, Vector & nearPt, Vector & up, Vector & right, float & scale, int desaturation, bool checkside=true, bool forceadditive=true);
bool gl_SetupLightTexture();
void gl_GetLightForThing(AActor * thing, float upper, float lower, float & r, float & g, float & b);

__forceinline void gl_Desaturate(int gray, int ired, int igreen, int iblue, BYTE & red, BYTE & green, BYTE & blue, int fac)
{
	red = (ired*(31-fac) + gray*fac)/31;
	green = (igreen*(31-fac) + gray*fac)/31;
	blue = (iblue*(31-fac) + gray*fac)/31;
}

void gl_ModifyColor(BYTE & red, BYTE & green, BYTE & blue, int cm);



#endif
