#ifndef __GL_STRUCT
#define __GL_STRUCT

#include "doomtype.h"
#include "v_palette.h"
#include "tarray.h"
#include "gl_values.h"
#include "textures/textures.h"

struct vertex_t;
struct line_t;
struct side_t;
struct seg_t;
struct subsector_t;
struct sector_t;
struct FGLSection;

extern DWORD gl_fixedcolormap;
class FGLTexture;

struct GL_RECT
{
	float left,top;
	float width,height;


	void Offset(float xofs,float yofs)
	{
		left+=xofs;
		top+=yofs;
	}
	void Scale(float xfac,float yfac)
	{
		left*=xfac;
		width*=xfac;
		top*=yfac;
		height*=yfac;
	}
	void Scale(fixed_t xfac,fixed_t yfac)
	{
		Scale(xfac/(float)FRACUNIT,yfac/(float)FRACUNIT);
	}
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
		LightColor.a = gl_fixedcolormap<CM_LIMIT? gl_fixedcolormap:CM_DEFAULT;
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
	FGLTexture * texture[2];
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

struct FGLSectionLine
{
	vertex_t *start;
	vertex_t *end;
	side_t *sidedef;
	line_t *linedef;
	seg_t *refseg;			// we need to reference at least one seg for each line.
	subsector_t *polysub;	// If this is part of a polyobject we need a reference to the containing subsector
	int otherside;
};

struct FGLSectionLoop
{
	int startline;
	int numlines;

	FGLSectionLine *GetLine(int no);
};

struct FGLSection
{
	sector_t *sector;
	TArray<subsector_t *> subsectors;
	int startloop;
	int numloops;

	FGLSectionLoop *GetLoop(int no);
};

extern TArray<FGLSectionLine> SectionLines;
extern TArray<FGLSectionLoop> SectionLoops;
extern TArray<FGLSection> Sections;

inline FGLSectionLine *FGLSectionLoop::GetLine(int no)
{
	return &SectionLines[startline + no];
}

inline FGLSectionLoop *FGLSection::GetLoop(int no)
{
	return &SectionLoops[startloop + no];
}

#endif
