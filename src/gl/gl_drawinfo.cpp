/*
** gl_drawinfo.cpp
** Implements the draw info structure which contains most of the
** data in a scene and the draw lists - including a very thorough BSP 
** style sorting algorithm for translucent objects.
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
** 4. When not used as part of GZDoom or a GZDoom derivative, this code will be
**    covered by the terms of the GNU Lesser General Public License as published
**    by the Free Software Foundation; either version 2.1 of the License, or (at
**    your option) any later version.
** 5. Full disclosure of the entire project's source code, except for third
**    party libraries is mandatory. (NOTE: This clause is non-negotiable!)
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
#include "gl/gl_include.h"
#include "gl/gl_renderstruct.h"
#include "gl/gl_texture.h"
#include "gl/gl_intern.h"
#include "gl/gl_basic.h"

GLDrawInfo * gl_drawinfo;
extern FreeList<gl_subsectorrendernode> SSR_List;

//==========================================================================
//
//
//
//==========================================================================
class StaticSortNodeArray : public TDeletingArray<SortNode*>
{
	unsigned usecount;
public:
	unsigned Size() { return usecount; }
	void Clear() { usecount=0; }
	void Release(int start) { usecount=start; }
	SortNode * GetNew();
};


SortNode * StaticSortNodeArray::GetNew()
{
	if (usecount==TArray<SortNode*>::Size())
	{
		Push(new SortNode);
	}
	return operator[](usecount++);
}


static StaticSortNodeArray SortNodes;

//==========================================================================
//
//
//
//==========================================================================
void GLDrawList::Reset()
{
	if (sorted) SortNodes.Release(SortNodeStart);
	sorted=NULL;
	walls.Clear();
	flats.Clear();
	sprites.Clear();
	drawitems.Clear();
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Translucent polygon sorting - uses a BSP algorithm with an additional 'equal' branch


//==========================================================================
//
//
//
//==========================================================================
inline void SortNode::UnlinkFromChain()
{
	if (parent) parent->next=next;
	if (next) next->parent=parent;
	parent=next=NULL;
}

//==========================================================================
//
//
//
//==========================================================================
inline void SortNode::Link(SortNode * hook)
{
	if (hook)
	{
		parent=hook->parent;
		hook->parent=this;
	}
	next=hook;
	if (parent) parent->next=this;
}

//==========================================================================
//
//
//
//==========================================================================
inline void SortNode::AddToEqual(SortNode *child)
{
	child->UnlinkFromChain();
	child->equal=equal;
	equal=child;			
}

//==========================================================================
//
//
//
//==========================================================================
inline void SortNode::AddToLeft(SortNode * child)
{
	child->UnlinkFromChain();
	child->Link(left);
	left=child;
}

//==========================================================================
//
//
//
//==========================================================================
inline void SortNode::AddToRight(SortNode * child)
{
	child->UnlinkFromChain();
	child->Link(right);
	right=child;
}


//==========================================================================
//
//
//
//==========================================================================
void GLDrawList::MakeSortList()
{
	SortNode * p, * n, * c;
	unsigned i;

	SortNodeStart=SortNodes.Size();
	p=NULL;
	n=SortNodes.GetNew();
	for(i=0;i<drawitems.Size();i++)
	{
		n->itemindex=(int)i;
		n->left=n->equal=n->right=NULL;
		n->parent=p;
		p=n;
		if (i!=drawitems.Size()-1)
		{
			c=SortNodes.GetNew();
			n->next=c;
			n=c;
		}
		else
		{
			n->next=NULL;
		}
	}
}


//==========================================================================
//
//
//
//==========================================================================
SortNode * GLDrawList::FindSortPlane(SortNode * head)
{
	while (head->next && drawitems[head->itemindex].rendertype!=GLDIT_FLAT) 
		head=head->next;
	if (drawitems[head->itemindex].rendertype==GLDIT_FLAT) return head;
	return NULL;
}


//==========================================================================
//
//
//
//==========================================================================
SortNode * GLDrawList::FindSortWall(SortNode * head)
{
	fixed_t farthest=INT_MIN;
	fixed_t nearest=INT_MAX;
	SortNode * best=NULL;
	SortNode * node=head;
	fixed_t bestdist=INT_MAX;

	while (node)
	{
		GLDrawItem * it=&drawitems[node->itemindex];
		if (it->rendertype==GLDIT_WALL)
		{
			fixed_t d=walls[it->index].viewdistance;
			if (d>farthest) farthest=d;
			if (d<nearest) nearest=d;
		}
		node=node->next;
	}
	if (farthest==INT_MIN) return NULL;
	node=head;
	farthest=(farthest+nearest)>>1;
	while (node)
	{
		GLDrawItem * it=&drawitems[node->itemindex];
		if (it->rendertype==GLDIT_WALL)
		{
			fixed_t di=abs(walls[it->index].viewdistance-farthest);
			if (!best || di<bestdist)
			{
				best=node;
				bestdist=di;
			}
		}
		node=node->next;
	}
	return best;
}

//==========================================================================
//
// Note: sloped planes are a huge problem...
//
//==========================================================================
void GLDrawList::SortPlaneIntoPlane(SortNode * head,SortNode * sort)
{
	GLFlat * fh=&flats[drawitems[head->itemindex].index];
	GLFlat * fs=&flats[drawitems[sort->itemindex].index];

	if (fh->z==fs->z) 
		head->AddToEqual(sort);
	else if ( (fh->z<fs->z && fh->ceiling) || (fh->z>fs->z && !fh->ceiling)) 
		head->AddToLeft(sort);
	else 
		head->AddToRight(sort);
}


//==========================================================================
//
//
//
//==========================================================================
void GLDrawList::SortWallIntoPlane(SortNode * head,SortNode * sort)
{
	GLFlat * fh=&flats[drawitems[head->itemindex].index];
	GLWall * ws=&walls[drawitems[sort->itemindex].index];
	GLWall * ws1;

	bool ceiling = fh->z > TO_GL(viewz);


	if (ws->ztop[0]>fh->z && ws->zbottom[0]<fh->z)
	{
		// We have to split this wall!

		// WARNING: NEVER EVER push a member of an array onto the array itself.
		// Bad things will happen if the memory must be reallocated!
		GLWall w=*ws;
		AddWall(&w);

		ws1=&walls[walls.Size()-1];
		ws=&walls[drawitems[sort->itemindex].index];	// may have been reallocated!
		float newtexv = ws->uplft.v + ((ws->lolft.v - ws->uplft.v) / (ws->zbottom[0] - ws->ztop[0])) * (fh->z - ws->ztop[0]);

		// I make the very big assumption here that translucent walls in sloped sectors
		// and 3D-floors never coexist in the same level. If that were the case this
		// code would become extremely more complicated.
		if (!ceiling)
		{
			ws->ztop[1] = ws1->zbottom[1] = ws->ztop[0] = ws1->zbottom[0] = fh->z;
			ws->uprgt.v = ws1->lorgt.v = ws->uplft.v = ws1->lolft.v = newtexv;
		}
		else
		{
			ws1->ztop[1] = ws->zbottom[1] = ws1->ztop[0] = ws->zbottom[0] = fh->z;
			ws1->uplft.v = ws->lolft.v = ws1->uprgt.v = ws->lorgt.v=newtexv;
		}

		SortNode * sort2=SortNodes.GetNew();
		memset(sort2,0,sizeof(SortNode));
		sort2->itemindex=drawitems.Size()-1;

		head->AddToLeft(sort);
		head->AddToRight(sort2);
	}
	else if ((ws->zbottom[0]<fh->z && !ceiling) || (ws->ztop[0]>fh->z && ceiling))	// completely on the left side
	{
		head->AddToLeft(sort);
	}
	else
	{
		head->AddToRight(sort);
	}

}

//==========================================================================
//
//
//
//==========================================================================
void GLDrawList::SortSpriteIntoPlane(SortNode * head,SortNode * sort)
{
	GLFlat * fh=&flats[drawitems[head->itemindex].index];
	GLSprite * ss=&sprites[drawitems[sort->itemindex].index];
	GLSprite * ss1;

	bool ceiling = fh->z > TO_GL(viewz);

	if (ss->z1>fh->z && ss->z2<fh->z)
	{
		// We have to split this sprite!
		GLSprite s=*ss;
		AddSprite(&s);
		ss1=&sprites[sprites.Size()-1];
		ss=&sprites[drawitems[sort->itemindex].index];	// may have been reallocated!
		float newtexv=ss->vt + ((ss->vb-ss->vt)/(ss->z2-ss->z1))*(fh->z-ss->z1);

		if (!ceiling)
		{
			ss->z1=ss1->z2=fh->z;
			ss->vt=ss1->vb=newtexv;
		}
		else
		{
			ss1->z1=ss->z2=fh->z;
			ss1->vt=ss->vb=newtexv;
		}

		SortNode * sort2=SortNodes.GetNew();
		memset(sort2,0,sizeof(SortNode));
		sort2->itemindex=drawitems.Size()-1;

		head->AddToLeft(sort);
		head->AddToRight(sort2);
	}
	else if ((ss->z2<fh->z && !ceiling) || (ss->z1>fh->z && ceiling))	// completely on the left side
	{
		head->AddToLeft(sort);
	}
	else
	{
		head->AddToRight(sort);
	}

}


//==========================================================================
//
//
//
//==========================================================================
#define MIN_EQ (0.0005f)

void GLDrawList::SortWallIntoWall(SortNode * head,SortNode * sort)
{
	GLWall * wh=&walls[drawitems[head->itemindex].index];
	GLWall * ws=&walls[drawitems[sort->itemindex].index];
	GLWall * ws1;
	float v1=wh->PointOnSide(ws->glseg.x1,ws->glseg.y1);
	float v2=wh->PointOnSide(ws->glseg.x2,ws->glseg.y2);

	if (fabs(v1)<MIN_EQ && fabs(v2)<MIN_EQ) 
	{
		if (ws->type==RENDERWALL_FOGBOUNDARY && wh->type!=RENDERWALL_FOGBOUNDARY) 
		{
			head->AddToRight(sort);
		}
		else if (ws->type!=RENDERWALL_FOGBOUNDARY && wh->type==RENDERWALL_FOGBOUNDARY) 
		{
			head->AddToLeft(sort);
		}
		else 
		{
			head->AddToEqual(sort);
		}
	}
	else if (v1<MIN_EQ && v2<MIN_EQ) 
	{
		head->AddToLeft(sort);
	}
	else if (v1>-MIN_EQ && v2>-MIN_EQ) 
	{
		head->AddToRight(sort);
	}
	else
	{
		double r=ws->CalcIntersectionVertex(wh);

		float ix=(float)(ws->glseg.x1+r*(ws->glseg.x2-ws->glseg.x1));
		float iy=(float)(ws->glseg.y1+r*(ws->glseg.y2-ws->glseg.y1));
		float iu=(float)(ws->uplft.u + r * (ws->uprgt.u - ws->uplft.u));
		float izt=(float)(ws->ztop[0]+r*(ws->ztop[1]-ws->ztop[0]));
		float izb=(float)(ws->zbottom[0]+r*(ws->zbottom[1]-ws->zbottom[0]));

		GLWall w=*ws;
		AddWall(&w);
		ws1=&walls[walls.Size()-1];
		ws=&walls[drawitems[sort->itemindex].index];	// may have been reallocated!

		ws1->glseg.x1=ws->glseg.x2=ix;
		ws1->glseg.y1=ws->glseg.y2=iy;
		ws1->ztop[0]=ws->ztop[1]=izt;
		ws1->zbottom[0]=ws->zbottom[1]=izb;
		ws1->lolft.u = ws1->uplft.u = ws->lorgt.u = ws->uprgt.u = iu;

		SortNode * sort2=SortNodes.GetNew();
		memset(sort2,0,sizeof(SortNode));
		sort2->itemindex=drawitems.Size()-1;

		if (v1>0)
		{
			head->AddToLeft(sort2);
			head->AddToRight(sort);
		}
		else
		{
			head->AddToLeft(sort);
			head->AddToRight(sort2);
		}
	}
}


//==========================================================================
//
// 
//
//==========================================================================
void GLDrawList::SortSpriteIntoWall(SortNode * head,SortNode * sort)
{
	GLWall * wh=&walls[drawitems[head->itemindex].index];
	GLSprite * ss=&sprites[drawitems[sort->itemindex].index];
	GLSprite * ss1;

	float v1 = wh->PointOnSide(ss->x1, ss->y1);
	float v2 = wh->PointOnSide(ss->x2, ss->y2);

	if (fabs(v1)<MIN_EQ && fabs(v2)<MIN_EQ) 
	{
		if (wh->type==RENDERWALL_FOGBOUNDARY) 
		{
			head->AddToLeft(sort);
		}
		else 
		{
			head->AddToEqual(sort);
		}
	}
	else if (v1<MIN_EQ && v2<MIN_EQ) 
	{
		head->AddToLeft(sort);
	}
	else if (v1>-MIN_EQ && v2>-MIN_EQ) 
	{
		head->AddToRight(sort);
	}
	else
	{
		double r=ss->CalcIntersectionVertex(wh);

		float ix=(float)(ss->x1 + r * (ss->x2-ss->x1));
		float iy=(float)(ss->y1 + r * (ss->y2-ss->y1));
		float iu=(float)(ss->ul + r * (ss->ur-ss->ul));

		GLSprite s=*ss;
		AddSprite(&s);
		ss1=&sprites[sprites.Size()-1];
		ss=&sprites[drawitems[sort->itemindex].index];	// may have been reallocated!

		ss1->x1=ss->x2=ix;
		ss1->y1=ss->y2=iy;
		ss1->ul=ss->ur=iu;

		SortNode * sort2=SortNodes.GetNew();
		memset(sort2,0,sizeof(SortNode));
		sort2->itemindex=drawitems.Size()-1;

		if (v1>0)
		{
			head->AddToLeft(sort2);
			head->AddToRight(sort);
		}
		else
		{
			head->AddToLeft(sort);
			head->AddToRight(sort2);
		}
	}
}


//==========================================================================
//
//
//
//==========================================================================
CVAR(Bool, gl_spritesortback, false, 0)

inline int GLDrawList::CompareSprites(SortNode * a,SortNode * b)
{
	GLSprite * s1=&sprites[drawitems[a->itemindex].index];
	GLSprite * s2=&sprites[drawitems[b->itemindex].index];

	float res = s2->scale-s1->scale;
	if (fabs(res)>FLT_EPSILON) return res;
	return gl_spritesortback? s1->index-s2->index : s2->index-s1->index;
}

//==========================================================================
//
//
//
//==========================================================================
static GLDrawList * gd;
int __cdecl CompareSprite(const void * a,const void * b)
{
	return gd->CompareSprites(*(SortNode**)a,*(SortNode**)b);
}

//==========================================================================
//
//
//
//==========================================================================
SortNode * GLDrawList::SortSpriteList(SortNode * head)
{
	SortNode * n;
	int count;
	unsigned i;

	static TArray<SortNode*> sortspritelist;

	SortNode * parent=head->parent;

	sortspritelist.Clear();
	for(count=0,n=head;n;n=n->next) sortspritelist.Push(n);
	gd=this;
	qsort(&sortspritelist[0],sortspritelist.Size(),sizeof(SortNode *),CompareSprite);
	for(i=0;i<sortspritelist.Size();i++)
	{
		sortspritelist[i]->next=NULL;
		if (parent) parent->equal=sortspritelist[i];
		parent=sortspritelist[i];
	}
	return sortspritelist[0];
}

//==========================================================================
//
//
//
//==========================================================================
SortNode * GLDrawList::DoSort(SortNode * head)
{
	SortNode * node, * sn, * next;

	sn=FindSortPlane(head);
	if (sn)
	{
		if (sn==head) head=head->next;
		sn->UnlinkFromChain();
		node=head;
		head=sn;
		while (node)
		{
			next=node->next;
			switch(drawitems[node->itemindex].rendertype)
			{
			case GLDIT_FLAT:
				SortPlaneIntoPlane(head,node);
				break;

			case GLDIT_WALL:
				SortWallIntoPlane(head,node);
				break;

			case GLDIT_SPRITE:
				SortSpriteIntoPlane(head,node);
				break;
			case GLDIT_POLY: break;
			}
			node=next;
		}
	}
	else
	{
		sn=FindSortWall(head);
		if (sn)
		{
			if (sn==head) head=head->next;
			sn->UnlinkFromChain();
			node=head;
			head=sn;
			while (node)
			{
				next=node->next;
				switch(drawitems[node->itemindex].rendertype)
				{
				case GLDIT_WALL:
					SortWallIntoWall(head,node);
					break;

				case GLDIT_SPRITE:
					SortSpriteIntoWall(head,node);
					break;
				case GLDIT_POLY: break;
				case GLDIT_FLAT: break;
				}
				node=next;
			}
		}
		else 
		{
			return SortSpriteList(head);
		}
	}
	if (head->left) head->left=DoSort(head->left);
	if (head->right) head->right=DoSort(head->right);
	return sn;
}


//==========================================================================
//
//
//
//==========================================================================
void GLDrawList::DoDraw(int pass, int i)
{
	switch(drawitems[i].rendertype)
	{
	case GLDIT_FLAT:
		{
			GLFlat * f=&flats[drawitems[i].index];
			RenderFlat.Clock();
			f->Draw(pass);
			RenderFlat.Unclock();
		}
		break;

	case GLDIT_WALL:
		{
			GLWall * w=&walls[drawitems[i].index];
			RenderWall.Clock();
			w->Draw(pass);
			RenderWall.Unclock();
		}
		break;

	case GLDIT_SPRITE:
		{
			GLSprite * s=&sprites[drawitems[i].index];
			RenderSprite.Clock();
			s->Draw(pass);
			RenderSprite.Unclock();
		}
		break;
	case GLDIT_POLY: break;
	}
}

//==========================================================================
//
//
//
//==========================================================================
void GLDrawList::DoDrawSorted(SortNode * head)
{
	do
	{
		if (head->left) 
		{
			DoDrawSorted(head->left);
		}
		DoDraw(GLPASS_TRANSLUCENT, head->itemindex);
		if (head->equal)
		{
			SortNode * ehead=head->equal;
			while (ehead)
			{
				DoDraw(GLPASS_TRANSLUCENT, ehead->itemindex);
				ehead=ehead->equal;
			}
		}
	}
	while ((head=head->right));
}

//==========================================================================
//
//
//
//==========================================================================
void GLDrawList::DrawSorted()
{
	if (drawitems.Size()==0) return;

	if (!sorted)
	{
		MakeSortList();
		sorted=DoSort(SortNodes[SortNodeStart]);
	}
	DoDrawSorted(sorted);
}

//==========================================================================
//
//
//
//==========================================================================
void GLDrawList::Draw(int pass)
{
	for(unsigned i=0;i<drawitems.Size();i++)
	{
		DoDraw(pass, i);
	}
}

//==========================================================================
//
// Sorting the drawitems first by texture and then by light level.
//
//==========================================================================
static GLDrawList * sortinfo;

static int __cdecl dicmp (const void *a, const void *b)
{
	const GLDrawItem * di[2];
	FGLTexture * tx[2];
	int lights[2];
	//colormap_t cm[2];
	di[0]=(const GLDrawItem *)a;
	di[1]=(const GLDrawItem *)b;

	for(int i=0;i<2;i++)
	{
		switch(di[i]->rendertype)
		{
		case GLDIT_FLAT:
		{
			GLFlat * f=&sortinfo->flats[di[i]->index];
			tx[i]=f->gltexture;
			lights[i]=f->lightlevel;
		}
		break;

		case GLDIT_WALL:
		{
			GLWall * w=&sortinfo->walls[di[i]->index];
			tx[i]=w->gltexture;
			lights[i]=w->lightlevel;
		}
		break;

		case GLDIT_SPRITE:
		{
			GLSprite * s=&sortinfo->sprites[di[i]->index];
			tx[i]=s->gltexture;
			lights[i]=s->lightlevel;
		}
		break;
		case GLDIT_POLY: break;
		}
	}
	if (lights[0]!=lights[1]) return lights[0]-lights[1];
	if (tx[0]!=tx[1]) return tx[0]-tx[1];
	/*if (lights[0]!=lights[1])*/ return lights[0]-lights[1];
}


void GLDrawList::Sort()
{
	if (drawitems.Size()!=0)
	{
		sortinfo=this;
		qsort(&drawitems[0], drawitems.Size(), sizeof(drawitems[0]), dicmp);
	}
}

//==========================================================================
//
//
//
//==========================================================================
void GLDrawList::AddWall(GLWall * wall)
{
	drawitems.Push(GLDrawItem(GLDIT_WALL,walls.Push(*wall)));
}

//==========================================================================
//
//
//
//==========================================================================
void GLDrawList::AddFlat(GLFlat * flat)
{
	drawitems.Push(GLDrawItem(GLDIT_FLAT,flats.Push(*flat)));
}

//==========================================================================
//
//
//
//==========================================================================
void GLDrawList::AddSprite(GLSprite * sprite)
{	
	drawitems.Push(GLDrawItem(GLDIT_SPRITE,sprites.Push(*sprite)));
}


//==========================================================================
//
// Try to reuse the lists as often as possible
//
//==========================================================================

static FreeList<GLDrawInfo> di_list;

//==========================================================================
//
// Sets up a new drawinfo struct
//
//==========================================================================
void GLDrawInfo::StartDrawInfo(GLDrawInfo * di)
{
	if (!di)
	{
		di=di_list.GetNew();
		di->temporary=true;
	}
	di->StartScene();
}

void GLDrawInfo::StartScene()
{
	sectorrenderflags.Resize(numsectors);
	ss_renderflags.Resize(numsubsectors);
	memset(&sectorrenderflags[0], 0, numsectors*sizeof(sectorrenderflags[0]));
	memset(&ss_renderflags[0], 0, numsubsectors*sizeof(ss_renderflags[0]));


	for(unsigned int i=0;i< otherfloorplanes.Size();i++)
	{
		gl_subsectorrendernode * node = otherfloorplanes[i];
		while (node)
		{
			gl_subsectorrendernode * n = node;
			node = node->next;
			SSR_List.Release(n);
		}
	}
	otherfloorplanes.Clear();

	for(unsigned int i=0;i< otherceilingplanes.Size();i++)
	{
		gl_subsectorrendernode * node = otherceilingplanes[i];
		while (node)
		{
			gl_subsectorrendernode * n = node;
			node = node->next;
			SSR_List.Release(n);
		}
	}
	otherceilingplanes.Clear();

	// clear all the lists that might not have been cleared already
	MissingUpperTextures.Clear();
	MissingLowerTextures.Clear();
	MissingUpperSegs.Clear();
	MissingLowerSegs.Clear();
	SubsectorHacks.Clear();
	CeilingStacks.Clear();
	FloorStacks.Clear();
	HandledSubsectors.Clear();

	next=gl_drawinfo;
	gl_drawinfo=this;
	for(int i=0;i<GLDL_TYPES;i++) drawlists[i].Reset();
}

//==========================================================================
//
//
//
//==========================================================================
void GLDrawInfo::EndDrawInfo()
{
	GLDrawInfo * di = gl_drawinfo;

	for(int i=0;i<GLDL_TYPES;i++) di->drawlists[i].Reset();
	gl_drawinfo=di->next;

	if (di->temporary) di_list.Release(di);
}
