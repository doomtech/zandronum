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

#include "gl/gl_values.h"
#include "gl/gl_struct.h"

struct F3DFloor;
struct model_t;
class FGLTexture;


int GetFloorLight (const sector_t *sec);
int GetCeilingLight (const sector_t *sec);


struct GLDrawList;
class ADynamicLight;


enum HWRenderStyle
{
	STYLEHW_Normal,			// default
	STYLEHW_Solid,			// drawn solid (needs special treatment for sprites)
	STYLEHW_NoAlphaTest,	// disable alpha test
};

struct GLRenderSettings
{

	SBYTE lightmode;
	bool nocoloredspritelighting;

	SBYTE map_lightmode;
	SBYTE map_nocoloredspritelighting;

	FVector3 skyrotatevector;

};

extern GLRenderSettings glset;

//==========================================================================
//
// One sector plane, still in fixed point
//
//==========================================================================

struct GLSectorPlane
{
	FTextureID texture;
	secplane_t plane;
	fixed_t texheight;
	fixed_t xoffs,  yoffs;
	fixed_t	xscale, yscale;
	angle_t	angle;

	void GetFromSector(sector_t * sec, bool ceiling)
	{
		xoffs = sec->GetXOffset(ceiling);
		yoffs = sec->GetYOffset(ceiling);
		xscale = sec->GetXScale(ceiling);
		yscale = sec->GetYScale(ceiling);
		angle = sec->GetAngle(ceiling);
		if (ceiling)
		{
			texture = sec->GetTexture(sector_t::ceiling);
			plane = sec->ceilingplane;
			texheight = plane.d;
		}
		else
		{
			texture = sec->GetTexture(sector_t::floor);
			plane = sec->floorplane;
			texheight = -plane.d;
		}
	}
};

struct GLHorizonInfo
{
	GLSectorPlane plane;
	int lightlevel;
	FColormap colormap;
};

//==========================================================================
//
// One wall segment in the draw list
//
//==========================================================================

class GLWall
{
public:

	enum
	{
		//GLWF_CLAMPX=1, use GLT_* for these!
		//GLWF_CLAMPY=2,
		GLWF_SKYHACK=4,
		GLWF_FOGGY=8,
		GLWF_GLOW=16,		// illuminated by glowing flats
		GLWF_NOSHADER=32,	// cannot be drawn with shaders.
	};

	friend struct GLDrawList;
	friend class GLPortal;

	GLSeg glseg;
	vertex_t * vertexes[2];				// required for polygon splitting
	float ztop[2],zbottom[2];
	texcoord uplft, uprgt, lolft, lorgt;
	float alpha;
	FGLTexture *gltexture;


	FColormap Colormap;
	ERenderStyle RenderStyle;
	
	fixed_t viewdistance;

	BYTE lightlevel;
	BYTE type;
	BYTE flags;
	short rellight;

	float topglowcolor[3];
	float bottomglowcolor[3];
	float topglowheight;
	float bottomglowheight;

	union
	{
		// it's either one of them but never more!
		AActor * skybox;			// for skyboxes
		GLSkyInfo * sky;			// for normal sky
		GLHorizonInfo * horizon;	// for horizon information
		GLSectorStackInfo * stack;	// for sector stacks
		secplane_t * planemirror;	// for plane mirrors
	};


	FTextureID topflat,bottomflat;

	// these are not the same as ytop and ybottom!!!
	float zceil[2];
	float zfloor[2];

public:
	seg_t * seg;			// this gives the easiest access to all other structs involved
	subsector_t * sub;		// For polyobjects
private:

	void PutWall(bool translucent);

	bool PrepareLight(texcoord * tcs, ADynamicLight * light);
	void RenderWall(int textured, float * color2, ADynamicLight * light=NULL);
	void RenderGlowingPoly(int textured, ADynamicLight * light=NULL);
	int Intersection(GL_RECT * rc,GLWall * result);

	void FloodPlane(int pass);

	void MirrorPlane(secplane_t * plane, bool ceiling);
	void SkyTexture(int sky1,ASkyViewpoint * skyboxx, bool ceiling);

	void SkyNormal(sector_t * fs,vertex_t * v1,vertex_t * v2);
	void SkyTop(seg_t * seg,sector_t * fs,sector_t * bs,vertex_t * v1,vertex_t * v2);
	void SkyBottom(seg_t * seg,sector_t * fs,sector_t * bs,vertex_t * v1,vertex_t * v2);
	void Put3DWall(lightlist_t * lightlist, bool translucent);
	void SplitWall(sector_t * frontsector, bool translucent);
	void LightPass();
	void SetHorizon(vertex_t * ul, vertex_t * ur, vertex_t * ll, vertex_t * lr);
	bool DoHorizon(seg_t * seg,sector_t * fs, vertex_t * v1,vertex_t * v2);

	bool SetWallCoordinates(seg_t * seg, int ceilingrefheight,
							int topleft,int topright, int bottomleft,int bottomright, int texoffset);

	void DoTexture(int type,seg_t * seg,int peg,
						   int ceilingrefheight,int floorrefheight,
						   int CeilingHeightstart,int CeilingHeightend,
						   int FloorHeightstart,int FloorHeightend,
						   int v_offset);

	void DoMidTexture(seg_t * seg, bool drawfogboundary,
					  sector_t * realfront, sector_t * realback,
					  fixed_t fch1, fixed_t fch2, fixed_t ffh1, fixed_t ffh2,
					  fixed_t bch1, fixed_t bch2, fixed_t bfh1, fixed_t bfh2);

	void BuildFFBlock(seg_t * seg, F3DFloor * rover,
					  fixed_t ff_topleft, fixed_t ff_topright, 
					  fixed_t ff_bottomleft, fixed_t ff_bottomright);
	void InverseFloors(seg_t * seg, sector_t * frontsector,
					   fixed_t topleft, fixed_t topright, 
					   fixed_t bottomleft, fixed_t bottomright);
	void ClipFFloors(seg_t * seg, F3DFloor * ffloor, sector_t * frontsector,
					fixed_t topleft, fixed_t topright, 
					fixed_t bottomleft, fixed_t bottomright);
	void DoFFloorBlocks(seg_t * seg, sector_t * frontsector, sector_t * backsector,
					  fixed_t fch1, fixed_t fch2, fixed_t ffh1, fixed_t ffh2,
					  fixed_t bch1, fixed_t bch2, fixed_t bfh1, fixed_t bfh2);

	void DrawDecal(DBaseDecal *actor, seg_t *seg, sector_t *frontSector, sector_t *backSector);
	void DoDrawDecals(DBaseDecal * decal, seg_t * seg);
	void ProcessOneDecal(seg_t *seg, DBaseDecal * decal, float leftxfrac,float rightxfrac);
	void ProcessDecals(seg_t *seg, float leftxfrac,float rightxfrac);

	void RenderFogBoundary();
	void RenderMirrorSurface();
	void RenderTranslucentWall();

public:

	void Process(seg_t *seg, sector_t * frontsector, sector_t * backsector, subsector_t * polysub, bool render_segs);
	void ProcessLowerMiniseg(seg_t *seg, sector_t * frontsector, sector_t * backsector);
	void Draw(int pass);

	float PointOnSide(float x,float y)
	{
		return -((y-glseg.y1)*(glseg.x2-glseg.x1)-(x-glseg.x1)*(glseg.y2-glseg.y1));
	}

	// Lines start-end and fdiv must intersect.
	double CalcIntersectionVertex(GLWall * w2)
	{
		float ax = glseg.x1, ay=glseg.y1;
		float bx = glseg.x2, by=glseg.y2;
		float cx = w2->glseg.x1, cy=w2->glseg.y1;
		float dx = w2->glseg.x2, dy=w2->glseg.y2;
		return ((ay-cy)*(dx-cx)-(ax-cx)*(dy-cy)) / ((bx-ax)*(dy-cy)-(by-ay)*(dx-cx));
	}

};

//==========================================================================
//
// One flat plane in the draw list
//
//==========================================================================

struct FSpriteModelFrame;

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

struct particle_t;

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
	double CalcIntersectionVertex(GLWall * w2)
	{
		float ax = x1, ay=y1;
		float bx = x2, by=y2;
		float cx = w2->glseg.x1, cy=w2->glseg.y1;
		float dx = w2->glseg.x2, dy=w2->glseg.y2;
		return ((ay-cy)*(dx-cx)-(ax-cx)*(dy-cy)) / ((bx-ax)*(dy-cy)-(by-ay)*(dx-cx));
	}

};


//==========================================================================
//
// Intermediate struct to link one draw item into a draw list
//
// unfortunately this struct must not contain pointers because
// the arrays may be reallocated!
//
//==========================================================================

struct GLDrawItem
{
	GLDrawItemType rendertype;
	int index;

	GLDrawItem(GLDrawItemType _rendertype,int _index) : rendertype(_rendertype),index(_index) {}
};

struct SortNode
{
	int itemindex;
	SortNode * parent;
	SortNode * next;		// unsorted successor
	SortNode * left;		// left side of this node
	SortNode * equal;		// equal to this node
	SortNode * right;		// right side of this node


	void UnlinkFromChain();
	void Link(SortNode * hook);
	void AddToEqual(SortNode * newnode);
	void AddToLeft (SortNode * newnode);
	void AddToRight(SortNode * newnode);
};

//==========================================================================
//
// One draw list. This contains all info for one type of rendering data
//
//==========================================================================

struct GLDrawList
{
//private:
	TArray<GLWall> walls;
	TArray<GLFlat> flats;
	TArray<GLSprite> sprites;
	TArray<GLDrawItem> drawitems;
	int SortNodeStart;
	SortNode * sorted;

public:
	GLDrawList()
	{
		next=NULL;
		SortNodeStart=-1;
		sorted=NULL;
	}

	~GLDrawList()
	{
		Reset();
	}

	void AddWall(GLWall * wall);
	void AddFlat(GLFlat * flat);
	void AddSprite(GLSprite * sprite);
	void Reset();
	void Sort();


	void MakeSortList();
	SortNode * FindSortPlane(SortNode * head);
	SortNode * FindSortWall(SortNode * head);
	void SortPlaneIntoPlane(SortNode * head,SortNode * sort);
	void SortWallIntoPlane(SortNode * head,SortNode * sort);
	void SortSpriteIntoPlane(SortNode * head,SortNode * sort);
	void SortWallIntoWall(SortNode * head,SortNode * sort);
	void SortSpriteIntoWall(SortNode * head,SortNode * sort);
	int CompareSprites(SortNode * a,SortNode * b);
	SortNode * SortSpriteList(SortNode * head);
	SortNode * DoSort(SortNode * head);
	
	void DoDraw(int pass, int index);
	void DoDrawSorted(SortNode * node);
	void DrawSorted();
	void Draw(int pass);
	
	GLDrawList * next;
} ;


//==========================================================================
//
// these are used to link faked planes due to missing textures to a sector
//
//==========================================================================
struct gl_subsectorrendernode
{
	gl_subsectorrendernode *	next;
	subsector_t *				sub;
};

//==========================================================================
//
// The draw info for one viewpoint
//
//==========================================================================

class GLDrawInfo
{
	friend class GLFlat;

	struct MissingTextureInfo
	{
		seg_t * seg;
		subsector_t * sub;
		fixed_t planez;
		fixed_t planezfront;
	};

	struct MissingSegInfo
	{
		seg_t * seg;
		int MTI_Index;	// tells us which MissingTextureInfo represents this seg.
	};

	struct SubsectorHackInfo
	{
		subsector_t * sub;
		BYTE flags;
	};

	struct wallseg
	{
		float x1, y1, z1, x2, y2, z2;
	};

	bool temporary;

	TArray<BYTE> sectorrenderflags;
	TArray<BYTE> ss_renderflags;

	TArray<gl_subsectorrendernode*> otherfloorplanes;
	TArray<gl_subsectorrendernode*> otherceilingplanes;

	TArray<MissingTextureInfo> MissingUpperTextures;
	TArray<MissingTextureInfo> MissingLowerTextures;

	TArray<MissingSegInfo> MissingUpperSegs;
	TArray<MissingSegInfo> MissingLowerSegs;

	TArray<SubsectorHackInfo> SubsectorHacks;
	// collect all the segs

	TArray<subsector_t *> CeilingStacks;
	TArray<subsector_t *> FloorStacks;


	TArray<subsector_t *> HandledSubsectors;


	GLDrawInfo * next;

	void StartScene();
	bool DoOneSectorUpper(subsector_t * subsec, fixed_t planez);
	bool DoOneSectorLower(subsector_t * subsec, fixed_t planez);
	bool DoFakeBridge(subsector_t * subsec, fixed_t planez);
	bool DoFakeCeilingBridge(subsector_t * subsec, fixed_t planez);
	void SetupFloodStencil(wallseg * ws);
	void ClearFloodStencil(wallseg * ws);
	void DrawFloodedPlane(wallseg * ws, float planez, sector_t * sec, bool ceiling);
	void FloodUpperGap(seg_t * seg);
	void FloodLowerGap(seg_t * seg);
	bool CheckAnchorFloor(subsector_t * sub);
	bool CollectSubsectorsFloor(subsector_t * sub, sector_t * anchor);
	bool CheckAnchorCeiling(subsector_t * sub);
	bool CollectSubsectorsCeiling(subsector_t * sub, sector_t * anchor);
	void CollectSectorStacksCeiling(subsector_t * sub, sector_t * anchor);
	void CollectSectorStacksFloor(subsector_t * sub, sector_t * anchor);


public:

	GLDrawList drawlists[GLDL_TYPES];

	void AddUpperMissingTexture(seg_t * seg, fixed_t backheight);
	void AddLowerMissingTexture(seg_t * seg, fixed_t backheight);
	void HandleMissingTextures();
	void DrawUnhandledMissingTextures();
	void AddHackedSubsector(subsector_t * sub);
	void HandleHackedSubsectors();
	void AddFloorStack(subsector_t * sub);
	void AddCeilingStack(subsector_t * sub);
	void ProcessSectorStacks();

	void AddOtherFloorPlane(int sector, gl_subsectorrendernode * node);
	void AddOtherCeilingPlane(int sector, gl_subsectorrendernode * node);

	gl_subsectorrendernode * GetOtherFloorPlanes(unsigned int sector)
	{
		if (sector<otherfloorplanes.Size()) return otherfloorplanes[sector];
		else return NULL;
	}

	gl_subsectorrendernode * GetOtherCeilingPlanes(unsigned int sector)
	{
		if (sector<otherceilingplanes.Size()) return otherceilingplanes[sector];
		else return NULL;
	}

	static void StartDrawInfo(GLDrawInfo * hi);
	static void EndDrawInfo();

};

extern GLDrawInfo * gl_drawinfo;

extern int gl_anglecache;
inline angle_t vertex_t::GetViewAngle()
{
	return angletime == gl_anglecache? viewangle : (angletime = gl_anglecache, viewangle = R_PointToAngle2(viewx, viewy, x,y));
}




#endif
