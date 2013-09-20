#ifndef __GL_DRAWINFO_H
#define __GL_DRAWINFO_H

#include "gl/old_renderer/gl1_renderstruct.h"
#include "gl/common/glc_renderhacks.h"
#include "gl/old_renderer/gl1_wall.h"


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
// The draw info for one viewpoint
//
//==========================================================================

class GLDrawInfo : public FDrawInfo
{
	friend class GLFlat;


	struct wallseg
	{
		float x1, y1, z1, x2, y2, z2;
	};

	bool temporary;

	GLDrawInfo * next;

	void StartScene();
	void SetupFloodStencil(wallseg * ws);
	void ClearFloodStencil(wallseg * ws);
	void DrawFloodedPlane(wallseg * ws, float planez, sector_t * sec, bool ceiling);
	void FloodUpperGap(seg_t * seg);
	void FloodLowerGap(seg_t * seg);


public:

	GLDrawList drawlists[GLDL_TYPES];

	static void StartDrawInfo(GLDrawInfo * hi);
	static void EndDrawInfo();

};

extern GLDrawInfo * gl_drawinfo;

#endif