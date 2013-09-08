#ifndef __GL_DRAWINFO_H
#define __GL_DRAWINFO_H

#include "gl/gl_renderstruct.h"
#include "gl/old_renderer/gl1_wall.h"

namespace GLRendererOld
{
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
}
#endif