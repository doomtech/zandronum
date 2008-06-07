
/*
** gl_missinglines.cpp
** This mess is only needed because ZDBSP likes to throw out lines 
** out of the BSP if they overlap
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
#include "gl/gl_include.h"
#include "p_local.h"
#include "gl/gl_struct.h"
#include "gl/gl_renderstruct.h"
#include "gl/gl_glow.h"
#include "gl/gl_data.h"
#include "gl/gl_basic.h"
#include "gl/gl_functions.h"
#include "gl/gl_geometric.h"

static seg_t * compareseg;
int firstmissingseg;

static int STACK_ARGS segcmp(const void * a, const void * b)
{
	seg_t * seg1 = *((seg_t**)a);
	seg_t * seg2 = *((seg_t**)b);

	return
		P_AproxDistance(seg2->v1->x - compareseg->v1->x, seg2->v1->y - compareseg->v1->y) -
		P_AproxDistance(seg1->v1->x - compareseg->v1->x, seg1->v1->y - compareseg->v1->y);

	//return (seg2->v1 - compareseg->v1).LengthSquared() - (seg1->v1 - compareseg->v1).LengthSquared();
}

//==========================================================================
//
// Collect all sidedefs which are not entirely covered by segs
//
// (Sigh! Why do I have to compensate for ZDBSP's shortcomings...)
//
//==========================================================================

void gl_CollectMissingLines()
{
	firstmissingseg=numsegs;

	TArray<seg_t> MissingSides;
	TArray<seg_t*> * linesegs = new TArray<seg_t*>[numsides];

	float * added_seglen = new float[numsides];

	memset(added_seglen, 0, sizeof(float)*numsides);
	for(int i=0;i<numsegs;i++)
	{
		seg_t * seg = &segs[i];

		if (seg->sidedef!=NULL)
		{
			// collect all the segs and calculate the length they occupy on their sidedef

			//added_seglen[seg->linedef-lines + side]+= (*seg->v2 - *seg->v1).Length();
#ifdef _MSC_VER
			added_seglen[seg->sidedef - sides]+= (Vector(seg->v2)-Vector(seg->v1)).Length();
#else
			Vector vec1(Vector(seg->v1));
			Vector vec2(Vector(seg->v2));
			Vector tmpVec = vec2-vec1;
			added_seglen[seg->sidedef - sides]+= tmpVec.Length();
#endif
			linesegs[seg->sidedef - sides].Push(seg);
		}
	}
	MissingSides.Clear();
	for(int i=0;i<numsides;i++)
	{
		side_t * side =&sides[i];
		line_t * line = &lines[side->linenum];

		//float linelen = (*line->v2 - *line->v1).Length();
#ifdef _MSC_VER
		float linelen = (Vector(line->v2)-Vector(line->v1)).Length();
#else
		Vector vec1(Vector(line->v1));
		Vector vec2(Vector(line->v2));
		Vector tmpVec = (vec2-vec1);
		float linelen = tmpVec.Length();
#endif

		if (added_seglen[i] < linelen -1)
		{
			Printf("Sidedef %d (linedef %d) incomplete (Length = %f of %f)\n",
				i,side->linenum, added_seglen[i], linelen);

			// create a seg for the sidedef
			seg_t seg;
			seg.sidedef = side;
			seg.linedef = line;
			if (side == &sides[line->sidenum[0]])
			{
				seg.v1 = line->v1;
				seg.v2 = line->v2;
				seg.frontsector = line->frontsector;
				seg.backsector = line->backsector;
			}
			else
			{
				seg.v2 = line->v1;
				seg.v1 = line->v2;
				seg.backsector = line->frontsector;
				seg.frontsector = line->backsector;
			}
			seg.Subsector = NULL;
			seg.PartnerSeg=NULL;
			seg.bPolySeg=false;

			if (linesegs[i].Size()!=0)
			{
				// There are already segs for this line so
				// we have to fill in the gaps. To make polyobject
				// spawning possible the sidedef has do be properly
				// split into segs that span its entire length
				compareseg = &seg;

				// Sort the segs so inserting new ones becomes easier
				qsort(&linesegs[i][0], linesegs[i].Size(), sizeof(seg_t*), segcmp);

				for(unsigned int j=0;j<linesegs[i].Size();j++)
				{
					if (linesegs[i][j]->v1 != seg.v1)
					{
						seg_t newseg = seg;
						newseg.v2 = linesegs[i][j]->v1;
						MissingSides.Push(newseg);
					}
					seg.v1 = linesegs[i][j]->v2;
				}
			}
			if (seg.v1!=seg.v2)
			{
				MissingSides.Push(seg);
			}
		}
	}

	if (MissingSides.Size()) 
	{
		// Now we have to add the newly created segs to the segs array so
		// that the polyobject spawn code has access to them.
		seg_t * newsegs = new seg_t[numsegs + MissingSides.Size()];

		memcpy(newsegs, segs, sizeof(seg_t) * numsegs);
		memcpy(newsegs+numsegs, &MissingSides[0], sizeof(seg_t)*MissingSides.Size());
		for(int i = 0; i < numsegs; i++)
		{
			if (newsegs[i].PartnerSeg && newsegs[i].PartnerSeg>=segs && newsegs[i].PartnerSeg<segs+numsegs) 
				newsegs[i].PartnerSeg = newsegs + (newsegs[i].PartnerSeg - segs);
			else newsegs[i].PartnerSeg=NULL;
		}
		numsegs += MissingSides.Size();
		delete [] segs;
		segs=newsegs;

		Printf("%d missing segs counted\n", MissingSides.Size());
	}
	delete [] linesegs;
	delete [] added_seglen;
}

//==========================================================================
//
// Render those lines
//
//==========================================================================

void gl_RenderMissingLines()
{
	for(int i=firstmissingseg;i<numsegs;i++)
	{
		seg_t * seg = &segs[i];

		// This line has already been processed
		if (seg->linedef->validcount==validcount) continue;

		// Don't draw lines facing away from the viewer
		divline_t dl = { seg->v1->x, seg->v1->y, seg->v2->x-seg->v1->x, seg->v2->y-seg->v1->y };
		if (P_PointOnDivlineSide(viewx, viewy, &dl)) continue;

		// Unfortunately there is no simple means to exclude lines here so we have
		// to draw them all. 
		sector_t ffakesec, bfakesec;

		sector_t * sector = gl_FakeFlat(seg->frontsector, &ffakesec, false);
		sector_t * backsector;

		if (seg->frontsector == seg->backsector) backsector=sector;
		else if (!seg->backsector) backsector=NULL;
		else backsector = gl_FakeFlat(seg->backsector, &bfakesec, true);

		GLWall wall;

		SetupWall.Start(true);

		wall.Process(seg, sector, backsector, NULL);
		rendered_lines++;

		SetupWall.Stop(true);
	}
}
