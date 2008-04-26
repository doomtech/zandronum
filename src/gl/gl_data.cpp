#include "gl_pch.h"

/*
** gl_data.cpp
** Maintenance data for GL renderer (mostly to handle rendering hacks)
**
**---------------------------------------------------------------------------
** Copyright 2005 Christoph Oelckers
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

#include "i_system.h"
#include "p_local.h"
#include "p_lnspec.h"
#include "c_dispatch.h"
#include "gl/gl_lights.h"
#include "gl/gl_glow.h"
#include "gl/gl_data.h"
#include "gl/gl_models.h"
#include "gl/gl_renderstruct.h"
#include "gl/gl_functions.h"
// [BC]
#include "network.h"
#include "sv_commands.h"

// [BC] Moved to p_lnspec.cpp.
/*
// Normally this would be better placed in p_lnspec.cpp.
// But I have accidentally overwritten that file several times
// so I'd rather place it here.
static int LS_Sector_SetPlaneReflection (line_t *ln, AActor *it, bool backSide,
	int arg0, int arg1, int arg2, int arg3, int arg4)
{
// Sector_SetPlaneReflection (tag, floor, ceiling)
	int secnum = -1;

	while ((secnum = P_FindSectorFromTag (arg0, secnum)) >= 0)
	{
		sector_t * s = &sectors[secnum];
		if (s->floorplane.a==0 && s->floorplane.b==0) s->floor_reflect = arg1/255.f;
		if (s->ceilingplane.a==0 && s->ceilingplane.b==0) sectors[secnum].ceiling_reflect = arg2/255.f;

		// [BC] If we're the server, tell clients that this sector's reflection is being altered.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetSectorReflection( secnum );
	}

	return true;
}
*/

//==========================================================================
//
// Portal identifier lists
//
//==========================================================================

TArray<GLVertex> gl_vertices(1024);

extern bool gl_disabled;

// A simple means so that I don't have to link to the debug stuff when I don't need it!
void (*gl_DebugHook)();


//==========================================================================
//
// prepare subsectors for GL rendering
//
//==========================================================================

CVAR(Int,forceglnodes, 0, CVAR_GLOBALCONFIG)	// only for testing - don't save!


inline void M_ClearBox (fixed_t *box)
{
	box[BOXTOP] = box[BOXRIGHT] = INT_MIN;
	box[BOXBOTTOM] = box[BOXLEFT] = INT_MAX;
}

inline void M_AddToBox(fixed_t* box,fixed_t x,fixed_t y)
{
	if (x<box[BOXLEFT]) box[BOXLEFT] = x;
	if (x>box[BOXRIGHT]) box[BOXRIGHT] = x;
	if (y<box[BOXBOTTOM]) box[BOXBOTTOM] = y;
	if (y>box[BOXTOP]) box[BOXTOP] = y;
}

static void SpreadHackedFlag(subsector_t * sub)
{
	// The subsector pointer hasn't been set yet!
	for(int i=0;i<sub->numlines;i++)
	{
		seg_t * seg = &segs[sub->firstline+i];

		if (seg->PartnerSeg)
		{
			subsector_t * sub2 = seg->PartnerSeg->Subsector;

			if (!(sub2->hacked&1) && sub2->render_sector == sub->render_sector)
			{
				sub2->hacked|=1;
				SpreadHackedFlag (sub2);
			}
		}
	}
}


static bool PointOnLine (int x, int y, int x1, int y1, int dx, int dy)
{
	const double SIDE_EPSILON = 6.5536;

	// For most cases, a simple dot product is enough.
	double d_dx = double(dx);
	double d_dy = double(dy);
	double d_x = double(x);
	double d_y = double(y);
	double d_x1 = double(x1);
	double d_y1 = double(y1);

	double s_num = (d_y1-d_y)*d_dx - (d_x1-d_x)*d_dy;

	if (fabs(s_num) < 17179869184.0)	// 4<<32
	{
		// Either the point is very near the line, or the segment defining
		// the line is very short: Do a more expensive test to determine
		// just how far from the line the point is.
		double l = sqrt(d_dx*d_dx+d_dy*d_dy);
		double dist = fabs(s_num)/l;
		if (dist < SIDE_EPSILON)
		{
			return true;
		}
	}
	return false;
}

static void PrepareSectorData()
{
	int 				i;
	int 				j;
	FBoundingBox		bbox;
	size_t				/*ii,*/ jj;
	TArray<subsector_t *> undetermined;
	subsector_t *		ss;

	// The GL node builder produces screwed output when two-sided walls overlap with one-sides ones!
	for(i=0;i<numsegs;i++)
	{
		int partner= segs[i].PartnerSeg-segs;

		if (partner<0 || partner>=numsegs || &segs[partner]!=segs[i].PartnerSeg)
		{
			segs[i].PartnerSeg=NULL;
		}

		// glbsp creates such incorrect references for Strife.
		if (segs[i].linedef && segs[i].PartnerSeg && !segs[i].PartnerSeg->linedef)
		{
			segs[i].PartnerSeg = segs[i].PartnerSeg->PartnerSeg = NULL;
		}
	}

	for(i=0;i<numsegs;i++)
	{
		if (segs[i].PartnerSeg && segs[i].PartnerSeg->PartnerSeg!=&segs[i])
		{
			//Printf("Warning: seg %d (sector %d)'s partner seg is incorrect!\n", i, segs[i].frontsector-sectors);
			segs[i].PartnerSeg=NULL;
		}
	}

	// look up sector number for each subsector
	for (i = 0; i < numsubsectors; i++)
	{
		// For rendering pick the sector from the first seg that is a sector boundary
		// this takes care of self-referencing sectors
		ss = &subsectors[i];
		seg_t *seg = &segs[ss->firstline];

		// Check for one-dimensional subsectors. These aren't rendered and should not be
		// subject to filling areas with bleeding flats
		ss->degenerate=true;
		for(jj=2; jj<ss->numlines; jj++)
		{
			if (!PointOnLine(seg[jj].v1->x, seg[jj].v1->y, seg->v1->x, seg->v1->y, seg->v2->x-seg->v1->x, seg->v2->y-seg->v1->y))
			{
				// Not on the same line
				ss->degenerate=false;
				break;
			}
		}

		seg = &segs[ss->firstline];
		M_ClearBox(ss->bbox);
		for(jj=0; jj<ss->numlines; jj++)
		{
			M_AddToBox(ss->bbox,seg->v1->x, seg->v1->y);
			seg++;
		}

		if (forceglnodes<2)
		{
			seg_t * seg = &segs[ss->firstline];
			for(j=0; j<ss->numlines; j++)
			{
				if(seg->sidedef && (!seg->PartnerSeg || seg->sidedef->sector!=seg->PartnerSeg->sidedef->sector))
				{
					ss->render_sector = seg->sidedef->sector;
					break;
				}
				seg++;
			}
			if(ss->render_sector == NULL) 
			{
				undetermined.Push(ss);
			}
		}
		else ss->render_sector=ss->sector;
	}

	// assign a vaild render sector to all subsectors which haven't been processed yet.
	while (undetermined.Size())
	{
		bool deleted=false;
		for(i=undetermined.Size()-1;i>=0;i--)
		{
			ss=undetermined[i];
			seg_t * seg = &segs[ss->firstline];
			
			for(j=0; j<ss->numlines; j++)
			{
				if (seg->PartnerSeg && seg->PartnerSeg->Subsector)
				{
					sector_t * backsec = seg->PartnerSeg->Subsector->render_sector;
					if (backsec)
					{
						ss->render_sector=backsec;
						undetermined.Delete(i);
						deleted=1;
						break;
					}
				}
				seg++;
			}
		}
		if (!deleted && undetermined.Size()) 
		{
			// This only happens when a subsector is off the map.
			// Don't bother and just assign the real sector for rendering
			for(i=undetermined.Size()-1;i>=0;i--)
			{
				ss=undetermined[i];
				ss->render_sector=ss->sector;
			}
			break;
		}
	}

	// now group the subsectors by sector
	subsector_t ** subsectorbuffer = new subsector_t * [numsubsectors];

	for(i=0, ss=subsectors; i<numsubsectors; i++, ss++)
	{
		ss->render_sector->subsectorcount++;
	}

	for (i=0; i<numsectors; i++) 
	{
		sectors[i].subsectors = subsectorbuffer;
		subsectorbuffer += sectors[i].subsectorcount;
		sectors[i].subsectorcount = 0;
	}
	
	for(i=0, ss = subsectors; i<numsubsectors; i++, ss++)
	{
		ss->render_sector->subsectors[ss->render_sector->subsectorcount++]=ss;
	}

	// marks all malformed subsectors so rendering tricks using them can be handled more easily
	for (i = 0; i < numsubsectors; i++)
	{
		if (subsectors[i].sector == subsectors[i].render_sector)
		{
			seg_t * seg = &segs[subsectors[i].firstline];
			for(int j=0;j<subsectors[i].numlines;j++)
			{
				if (!(subsectors[i].hacked&1) && seg[j].linedef==0 && 
						seg[j].PartnerSeg!=NULL && 
						subsectors[i].render_sector != seg[j].PartnerSeg->Subsector->render_sector)
				{
					//Printf("Found hack: %d,%d %d,%d\n", seg[j].v1->x>>16, seg[j].v1->y>>16, seg[j].v2->x>>16, seg[j].v2->y>>16);
					subsectors[i].hacked|=1;
					SpreadHackedFlag(&subsectors[i]);
				}
				if (seg[j].PartnerSeg==NULL) subsectors[i].hacked|=2;	// used for quick termination checks
			}
		}
	}
}

//==========================================================================
//
// Some processing for transparent door hacks
//
//==========================================================================
static void PrepareTransparentDoors(sector_t * sector)
{
	bool solidwall=false;
	int notextures=0;
	int nobtextures=0;
	int selfref=0;
	int i;
	sector_t * nextsec=NULL;

#ifdef _MSC_VER
#ifdef _DEBUG
	if (sector-sectors==590)
	{
		__asm nop
	}
#endif
#endif

	P_Recalculate3DFloors(sector);
	if (sector->subsectorcount==0) return;

	sector->transdoorheight=sector->floortexz;
	sector->transdoor= !(sector->e->ffloors.Size() || sector->heightsec || sector->floorplane.a || sector->floorplane.b);

	if (sector->transdoor)
	{
		for (i=0; i<sector->linecount; i++)
		{
			if (sector->lines[i]->frontsector==sector->lines[i]->backsector) 
			{
				selfref++;
				continue;
			}

			sector_t * sec=getNextSector(sector->lines[i], sector);
			if (sec==NULL) 
			{
				solidwall=true;
				continue;
			}
			else
			{
				nextsec=sec;

				int side=sides[sector->lines[i]->sidenum[0]].sector==sec;

				if (sector->floortexz!=sec->floortexz+FRACUNIT) 
				{
					sector->transdoor=false;
					return;
				}
				if (sides[sector->lines[i]->sidenum[1-side]].toptexture==0) notextures++;
				if (sides[sector->lines[i]->sidenum[1-side]].bottomtexture==0) nobtextures++;
			}
		}
		if (selfref+notextures==sector->linecount || sector->ceilingpic==skyflatnum)
		{
			sector->transdoor=false;
			return;
		}

		// This is a crude attempt to fix an incorrect transparent door effect I found in some
		// WolfenDoom maps but considering the amount of code required to handle it I left it in.
		// Do this only if the sector only contains one-sided walls or ones with no lower texture.
		if (solidwall)
		{
			if (solidwall+nobtextures+selfref==sector->linecount && nextsec)
			{
				sector->heightsec=nextsec;
				sector->heightsec->MoreFlags=0;
			}
			sector->transdoor=false;
		}
	}
}



//=============================================================================
//
//
//
//=============================================================================
side_t* getNextSide(sector_t * sec, line_t* line)
{
	if (sec==line->frontsector)
	{
		if (sec==line->backsector) return NULL;	
		if (line->sidenum[1]!=NO_SIDE) return &sides[line->sidenum[1]];
	}
	else
	{
		if (line->sidenum[0]!=NO_SIDE) return &sides[line->sidenum[0]];
	}
	return NULL;
}

//==========================================================================
//
// Initialize the level data for the GL renderer
//
//==========================================================================

void gl_PreprocessLevel()
{
	int i,j;

	static bool modelsdone=false;

//	LineSpecials[159]=LS_Sector_SetPlaneReflection;

	if (!modelsdone)
	{
		modelsdone=true;
		gl_InitModels();
	}

	R_ResetViewInterpolation ();


	// Nasty: I can't rely upon the sidedef assignments because ZDBSP likes to screw them up
	// if the sidedefs are compressed and both sides are the same.
	for(i=0;i<numsegs;i++)
	{
		seg_t * seg=&segs[i];
		if (seg->backsector == seg->frontsector && seg->linedef)
		{
			fixed_t d1=P_AproxDistance(seg->v1->x-seg->linedef->v1->x,seg->v1->y-seg->linedef->v1->y);
			fixed_t d2=P_AproxDistance(seg->v2->x-seg->linedef->v1->x,seg->v2->y-seg->linedef->v1->y);

			if (d2<d1)	// backside
			{
				seg->sidedef = &sides[seg->linedef->sidenum[1]];
			}
			else	// front side
			{
				seg->sidedef = &sides[seg->linedef->sidenum[0]];
			}
		}
	}
	
	if (gl_disabled) return;

	PrepareSectorData();
	for(i=0;i<numsectors;i++) PrepareTransparentDoors(&sectors[i]);
	pitch=0.0f;

	gl_vertices.Resize(100);	
	gl_vertices.Clear();	

	// Create the flat vertex array
	for (i=0; i<numsubsectors; i++)
	{
		subsector_t * ssector = &subsectors[i];

		if (ssector->numlines<=2) continue;
			
		ssector->numvertices = ssector->numlines;
		ssector->firstvertex = gl_vertices.Size();

		for(j = 0;  j < ssector->numlines; j++)
		{
			seg_t * seg = &segs[ssector->firstline + j];
			vertex_t * vtx = seg->v1;
			GLVertex * vt=&gl_vertices[gl_vertices.Reserve(1)];

			vt->u =  TO_MAP(vtx->x)/64.0f;
			vt->v = -TO_MAP(vtx->y)/64.0f;
			vt->x =  TO_MAP(vtx->x);
			vt->y =  TO_MAP(vtx->y);
			vt->z = 0.0f;
			vt->vt = vtx;
		}
	}
	gl_CollectMissingLines();
	gl_InitVertexData();

	// This code can be called when the hardware renderer is inactive!
	if (currentrenderer!=0) gl.ArrayPointer(&gl_vertices[0], sizeof(GLVertex));

	if (gl_DebugHook) gl_DebugHook();
}


CCMD(dumpgeometry)
{
	for(int i=0;i<numsectors;i++)
	{
		sector_t * sector = &sectors[i];

		Printf("Sector %d\n",i);
		for(int j=0;j<sector->subsectorcount;j++)
		{
			subsector_t * sub = sector->subsectors[j];

			Printf("    Subsector %d - real sector = %d - %s\n", sub-subsectors, sub->sector->sectornum, sub->hacked&1? "hacked":"");
			for(int k=0;k<sub->numlines;k++)
			{
				seg_t * seg = &segs[sub->firstline+k];
				if (seg->linedef)
				{
				Printf("      (%4.4f, %4.4f), (%4.4f, %4.4f) - seg %d, linedef %d, side %d", 
					TO_MAP(seg->v1->x), TO_MAP(seg->v1->y), TO_MAP(seg->v2->x), TO_MAP(seg->v2->y),
					seg-segs, seg->linedef-lines, seg->sidedef!=&sides[seg->linedef->sidenum[0]]);
				}
				else
				{
					Printf("      (%4.4f, %4.4f), (%4.4f, %4.4f) - seg %d, miniseg", 
						TO_MAP(seg->v1->x), TO_MAP(seg->v1->y), TO_MAP(seg->v2->x), TO_MAP(seg->v2->y),
						seg-segs);
				}
				if (seg->PartnerSeg) 
				{
					subsector_t * sub2 = seg->PartnerSeg->Subsector;
					Printf(", back sector = %d, real back sector = %d", sub2->render_sector->sectornum, seg->PartnerSeg->frontsector->sectornum);
				}
				Printf("\n");
			}
		}
	}
}

//==========================================================================
//
// Cleans up all the GL data for the last level
//
//==========================================================================
void gl_CleanLevelData()
{
	// Dynamic lights must be destroyed before the sector information here is deleted!
	TThinkerIterator<ADynamicLight> it(STAT_DLIGHT);
	AActor * mo=it.Next();
	while (mo)
	{
		AActor * next = it.Next();
		mo->Destroy();
		mo=next;
	}

	if (sectors && sectors[0].subsectors) 
	{
		delete [] sectors[0].subsectors;
		sectors[0].subsectors=NULL;
	}
	if (gamenodes && gamenodes!=nodes)
	{
		delete [] gamenodes;
		gamenodes = NULL;
		numgamenodes = 0;
	}
	if (gamesubsectors && gamesubsectors!=subsectors)
	{
		delete [] gamesubsectors;
		gamesubsectors = NULL;
		numgamesubsectors = 0;
	}
	gl_CleanVertexData();
}


