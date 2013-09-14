#ifndef __GL_STRUCT
#define __GL_STRUCT

#include "doomtype.h"
#include "v_palette.h"
#include "tarray.h"
#include "gl/old_renderer/gl1_values.h"
#include "gl/old_renderer/gl1_texture.h"
#include "textures/textures.h"

struct vertex_t;
struct line_t;
struct side_t;
struct seg_t;
struct subsector_t;
struct sector_t;
struct FGLSection;

namespace GLRendererOld
{
extern DWORD gl_fixedcolormap;
}

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


  // for internal use
struct FColormap
{
	PalEntry		LightColor;		// a is saturation (0 full, 31=b/w, other=custom colormap)
	PalEntry		FadeColor;		// a is fadedensity>>1
	int				blendfactor;

	void Clear()
	{
		LightColor=0xffffff;
		FadeColor=0;
		blendfactor=0;
	}

	void ClearColor()
	{
		LightColor.r=LightColor.g=LightColor.b=0xff;
		blendfactor=0;
	}


	void GetFixedColormap()
	{
		Clear();
		LightColor.a = GLRendererOld::gl_fixedcolormap<CM_LIMIT? GLRendererOld::gl_fixedcolormap:CM_DEFAULT;
	}

	FColormap & operator=(FDynamicColormap * from)
	{
		LightColor = from->Color;
		LightColor.a = from->Desaturate>>3;
		FadeColor = from->Fade;
		blendfactor = from->Color.a;
		return * this;
	}

	void CopyLightColor(FDynamicColormap * from)
	{
		LightColor = from->Color;
		LightColor.a = from->Desaturate>>3;
		blendfactor = from->Color.a;
	}
};

struct GLVertex
{
	float x,z,y;	// world coordinates
	float u,v;		// texture coordinates
	vertex_t * vt;	// real vertex
};

typedef struct
{
	float x1,x2;
	float y1,y2;
	float fracleft, fracright;	// fractional offset of the 2 vertices on the linedef
} GLSeg;


struct texcoord
{
	float u,v;
};


struct GLSkyInfo
{
	float x_offset[2];
	float y_offset;		// doubleskies don't have a y-offset
	GLRendererOld::FGLTexture * texture[2];
	FTextureID skytexno1;
	bool mirrored;
	bool doublesky;
	PalEntry fadecolor;	// if this isn't made part of the dome things will become more complicated when sky fog is used.

	bool operator==(const GLSkyInfo & inf)
	{
		return !memcmp(this, &inf, sizeof(*this));
	}
	bool operator!=(const GLSkyInfo & inf)
	{
		return !!memcmp(this, &inf, sizeof(*this));
	}
};

struct GLSectorStackInfo
{
	fixed_t deltax;
	fixed_t deltay;
	fixed_t deltaz;
	bool isupper;	
};


extern TArray<GLVertex> gl_vertices;

#endif
