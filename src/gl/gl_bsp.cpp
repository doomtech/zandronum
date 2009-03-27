/*
** gl_bsp.cpp
** Main rendering loop / BSP traversal / visibility clipping
**
**---------------------------------------------------------------------------
** Copyright 2000-2005 Christoph Oelckers
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
#include "p_lnspec.h"
#include "p_local.h"
#include "a_sharedglobal.h"
#include "gl/gl_renderstruct.h"
#include "gl/gl_clipper.h"
#include "gl/gl_lights.h"
#include "gl/gl_data.h"
#include "gl/gl_portal.h"
#include "gl/gl_basic.h"
#include "gl/gl_functions.h"
#include "r_sky.h"


EXTERN_CVAR(Bool, gl_render_segs)

Clipper clipper;


//==========================================================================
//
// R_AddLine
// Clips the given segment
// and adds any visible pieces to the line list.
//
//==========================================================================

static void AddLine (seg_t *seg,sector_t * sector,subsector_t * polysub)
{
	angle_t startAngle, endAngle;
	sector_t * backsector = NULL;
	sector_t bs;

	ClipWall.Clock();
	if (GLPortal::mirrorline)
	{
		// this seg is completely behind the mirror!
		if (P_PointOnLineSide(seg->v1->x, seg->v1->y, GLPortal::mirrorline) &&
			P_PointOnLineSide(seg->v2->x, seg->v2->y, GLPortal::mirrorline)) 
		{
			ClipWall.Unclock();
			return;
		}
	}

	startAngle = seg->v2->GetViewAngle();
	endAngle = seg->v1->GetViewAngle();

	// Back side, i.e. backface culling	- read: endAngle >= startAngle!
	if (startAngle-endAngle<ANGLE_180 || !seg->linedef)  
	{
		ClipWall.Unclock();
		return;
	}

	if (!clipper.SafeCheckRange(startAngle, endAngle)) 
	{
		ClipWall.Unclock();
		return;
	}

	if (!seg->backsector)
	{
		clipper.SafeAddClipRange(startAngle, endAngle);
	}
	else if (!polysub)	// Two-sided polyobjects never obstruct the view
	{
		if (sector->sectornum == seg->backsector->sectornum)
		{
			FTexture * tex = TexMan(seg->sidedef->GetTexture(side_t::mid));
			if (!tex || tex->UseType==FTexture::TEX_Null) 
			{
				// nothing to do here!
				ClipWall.Unclock();
				seg->linedef->validcount=validcount;
				return;
			}
			backsector=sector;
		}
		else
		{
			// clipping checks are only needed when the backsector is not the same as the front sector
			gl_CheckViewArea(seg->v1, seg->v2, seg->frontsector, seg->backsector);

			backsector = gl_FakeFlat(seg->backsector, &bs, true);

			if (gl_CheckClip(seg->sidedef, sector, backsector))
			{
				clipper.SafeAddClipRange(startAngle, endAngle);
			}
		}
	}
	else 
	{
		// Backsector for polyobj segs is always the containing sector itself
		backsector = sector;
	}

	seg->linedef->flags |= ML_MAPPED;
	ClipWall.Unclock();

	if (!gl_render_segs)
	{
		// rendering per linedef as opposed per seg is significantly more efficient
		// so mark the linedef as rendered here and render it completely.
		if (seg->linedef->validcount!=validcount) seg->linedef->validcount=validcount;
		else return;
	}

	GLWall wall;

	SetupWall.Clock();

	wall.Process(seg, sector, backsector, polysub, gl_render_segs);
	rendered_lines++;

	SetupWall.Unclock();
}


//==========================================================================
//
//
//
//==========================================================================
static inline void AddLines(subsector_t * sub, sector_t * sector)
{
	if (sub->poly)
	{ // Render the polyobj in the subsector first
		int polyCount = sub->poly->numsegs;
		seg_t **polySeg = sub->poly->segs;
		while (polyCount--)
		{
			AddLine (*polySeg++, sector, sub);
		}
	}

	int count = sub->numlines;
	seg_t * line = &segs[sub->firstline];

	while (count--)
	{
		if (line->linedef)
		{
			if (!line->bPolySeg) AddLine (line, sector, NULL);
		}
		else
		{
		}
		line++;
	}
}


//==========================================================================
//
// R_RenderThings
//
//==========================================================================
static inline void RenderThings(subsector_t * sub, sector_t * sector)
{
	GLSprite glsprite;

	SetupSprite.Clock();
	sector_t * sec=sub->sector;
	// BSP is traversed by subsector.
	// A sector might have been split into several
	//	subsectors during BSP building.
	// Thus we check whether it was already added.
	if (sec->thinglist != NULL && sec->validcount != validcount)
	{
		// Well, now it will be done.
		sec->validcount = validcount;

		// Handle all things in sector.
		for (AActor * thing = sec->thinglist; thing; thing = thing->snext)
		{
			glsprite.Process(thing, sector);
		}
	}
	SetupSprite.Unclock();
}

//==========================================================================
//
// R_Subsector
// Determine floor/ceiling planes.
// Add sprites of things in sector.
// Draw one or more line segments.
//
//==========================================================================

static void DoSubsector(subsector_t * sub)
{
	int i;
	sector_t * sector;
	sector_t * fakesector;
	sector_t fake;
	GLFlat glflat;
	GLSprite glsprite;
	
	// check for visibility of this entire subsector! This requires GL nodes!
	if (!clipper.CheckBox(sub->bbox)) return;

#ifdef _MSC_VER
#ifdef _DEBUG
	if (sub->sector-sectors==931)
	{
		__asm nop
	}
#endif
#endif

	sector=sub->sector;
	if (!sector) return;

	sector->MoreFlags |= SECF_DRAWN;
	fakesector=gl_FakeFlat(sector, &fake, false);

	// [RH] Add particles
	//int shade = LIGHT2SHADE((floorlightlevel + ceilinglightlevel)/2 + r_actualextralight);
	for (i = ParticlesInSubsec[sub-subsectors]; i != NO_PARTICLE; i = Particles[i].snext)
	{
		glsprite.ProcessParticle(Particles + i, fakesector);//, 0, 0);
	}
	AddLines(sub, fakesector);
	RenderThings(sub, fakesector);

	// Subsectors with only 2 lines cannot have any area!
	if (sub->numlines>2 || (sub->hacked&1)) 
	{
		// Exclude the case when it tries to render a sector with a heightsec
		// but undetermined heightsec state. This can only happen if the
		// subsector is obstructed but not excluded due to a large bounding box.
		// Due to the way a BSP works such a subsector can never be visible
		if (!sector->heightsec || sector->heightsec->MoreFlags & SECF_IGNOREHEIGHTSEC || in_area!=area_default)
		{
			if (sector != sub->render_sector)
			{
				sector = sub->render_sector;
				// the planes of this subsector are faked to belong to another sector
				// This means we need the heightsec parts and light info of the render sector, not the actual one!
				fakesector = gl_FakeFlat(sector, &fake, false);
			}
			SetupFlat.Clock();
			glflat.ProcessSector(fakesector, sub);
			SetupFlat.Unclock();
		}
	}
}




//==========================================================================
//
// RenderBSPNode
// Renders all subsectors below a given node,
//  traversing subtree recursively.
// Just call with BSP root.
//
//==========================================================================

void gl_RenderBSPNode (void *node)
{
	if (numnodes == 0)
	{
		DoSubsector (subsectors);
		return;
	}
	while (!((size_t)node & 1))  // Keep going until found a subsector
	{
		node_t *bsp = (node_t *)node;

		// Decide which side the view point is on.
		int side = R_PointOnSide(viewx, viewy, bsp);

		// Recursively divide front space (toward the viewer).
		gl_RenderBSPNode (bsp->children[side]);

		// Possibly divide back space (away from the viewer).
		side ^= 1;
		if (!clipper.CheckBox(bsp->bbox[side]))
		{
			return;
		}

		node = bsp->children[side];
	}
	DoSubsector ((subsector_t *)((BYTE *)node - 1));
}


