/*
** gl_renderstruct.h
** Defines the structures used to store data to be rendered
**
**---------------------------------------------------------------------------
** Copyright 2002-2005 Christoph Oelckers
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/


#ifndef __GL_RENDERSTRUCT_H
#define __GL_RENDERSTRUCT_H

#include "r_local.h"
#include "tarray.h"
#include "templates.h"

#include "gl/common/glc_structs.h"
#include "gl/old_renderer/gl1_values.h"
#include "gl/gl_struct.h"

struct F3DFloor;
struct model_t;
struct FSpriteModelFrame;
struct particle_t;
class ADynamicLight;

int GetFloorLight (const sector_t *sec);
int GetCeilingLight (const sector_t *sec);


namespace GLRendererOld
{
class FGLTexture;
struct GLDrawList;


enum HWRenderStyle
{
	STYLEHW_Normal,			// default
	STYLEHW_Solid,			// drawn solid (needs special treatment for sprites)
	STYLEHW_NoAlphaTest,	// disable alpha test
};

struct GLVertex
{
	float x,z,y;	// world coordinates
	float u,v;		// texture coordinates
	vertex_t * vt;	// real vertex
};

extern TArray<GLVertex> gl_vertices;

struct GLHorizonInfo
{
	GLSectorPlane plane;
	int lightlevel;
	FColormap colormap;
};


class GLWall;

//==========================================================================
//
// One flat plane in the draw list
//
//==========================================================================

class GLFlat
{
public:
	friend struct GLDrawList;

	sector_t * sector;
	subsector_t * sub;	// only used for translucent planes
	float z; // the z position of the flat (height)
	FGLTexture *gltexture;

	FColormap Colormap;	// light and fog
	ERenderStyle renderstyle;

	float alpha;
	GLSectorPlane plane;
	short lightlevel;
	bool stack;
	bool foggy;
	bool ceiling;
	BYTE renderflags;

	void DrawSubsector(subsector_t * sub);
	void DrawSubsectorLights(subsector_t * sub, int pass);
	void DrawSubsectors(bool istrans);

	void PutFlat(bool fog = false);
	void Process(sector_t * sector, bool whichplane, bool notexture);
	void ProcessSector(sector_t * frontsector, subsector_t * sub);
	void Draw(int pass);
};


//==========================================================================
//
// One sprite in the draw list
//
//==========================================================================


class GLSprite
{
public:
	friend struct GLDrawList;
	friend void Mod_RenderModel(GLSprite * spr, model_t * mdl, int framenumber);

	BYTE lightlevel;
	BYTE foglevel;
	BYTE hw_styleflags;
	bool fullbright;
	PalEntry ThingColor;	// thing's own color
	FColormap Colormap;
	FSpriteModelFrame * modelframe;
	FRenderStyle RenderStyle;

	int translation;
	int index;
	float scale;
	float x,y,z;	// needed for sorting!

	float ul,ur;
	float vt,vb;
	float x1,y1,z1;
	float x2,y2,z2;

	FGLTexture *gltexture;
	float trans;
	AActor * actor;
	particle_t * particle;

	void SplitSprite(sector_t * frontsector, bool translucent);
	void SetLowerParam();

public:

	void Draw(int pass);
	void PutSprite(bool translucent);
	void Process(AActor* thing,sector_t * sector);
	void ProcessParticle (particle_t *particle, sector_t *sector);//, int shade, int fakeside)
	void SetThingColor(PalEntry);
	void SetSpriteColor(sector_t *sector, fixed_t y);

	// Lines start-end and fdiv must intersect.
	double CalcIntersectionVertex(GLWall * w2);
};


}


#endif
