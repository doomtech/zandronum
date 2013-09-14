#ifndef __GL_WALL_H
#define __GL_WALL_H
//==========================================================================
//
// One wall segment in the draw list
//
//==========================================================================
namespace GLRendererOld
{

	struct GLHorizonInfo;

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

	void CheckGlowing();
	void PutWall(bool translucent);

	bool PrepareLight(texcoord * tcs, ADynamicLight * light);
	void RenderWall(int textured, float * color2, ADynamicLight * light=NULL);
	void RenderGlowingPoly(int textured, ADynamicLight * light=NULL);
	int Intersection(FloatRect * rc,GLWall * result);

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

	void SplitLeftEdge(texcoord * tcs, bool glow);
	void SplitRightEdge(texcoord * tcs, bool glow);

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


}
#endif
