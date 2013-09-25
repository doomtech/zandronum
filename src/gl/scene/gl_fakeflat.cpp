/*
** gl_renderutil.cpp
** Utility functions for the renderer
**
**---------------------------------------------------------------------------
** Copyright 2001-2008 Christoph Oelckers
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

#include "p_lnspec.h"
#include "p_local.h"
#include "a_sharedglobal.h"
#include "r_sky.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/scene/gl_clipper.h"


//==========================================================================
//
// Check whether the player can look beyond this line
//
//==========================================================================
CVAR(Bool, gltest_slopeopt, false, 0)

bool gl_CheckClip(side_t * sidedef, sector_t * frontsector, sector_t * backsector)
{
	line_t *linedef = sidedef->linedef;
	fixed_t bs_floorheight1;
	fixed_t bs_floorheight2;
	fixed_t bs_ceilingheight1;
	fixed_t bs_ceilingheight2;
	fixed_t fs_floorheight1;
	fixed_t fs_floorheight2;
	fixed_t fs_ceilingheight1;
	fixed_t fs_ceilingheight2;

	// Mirrors and horizons always block the view
	//if (linedef->special==Line_Mirror || linedef->special==Line_Horizon) return true;

	// Lines with stacked sectors must never block!

	if (backsector->CeilingSkyBox && backsector->CeilingSkyBox->bAlways) return false;
	if (backsector->FloorSkyBox && backsector->FloorSkyBox->bAlways) return false;
	if (frontsector->CeilingSkyBox && frontsector->CeilingSkyBox->bAlways) return false;
	if (frontsector->FloorSkyBox && frontsector->FloorSkyBox->bAlways) return false;

	// on large levels this distinction can save some time
	// That's a lot of avoided multiplications if there's a lot to see!

	if (frontsector->ceilingplane.a | frontsector->ceilingplane.b)
	{
		fs_ceilingheight1=frontsector->ceilingplane.ZatPoint(linedef->v1);
		fs_ceilingheight2=frontsector->ceilingplane.ZatPoint(linedef->v2);
	}
	else
	{
		fs_ceilingheight2=fs_ceilingheight1=frontsector->ceilingplane.d;
	}

	if (frontsector->floorplane.a | frontsector->floorplane.b)
	{
		fs_floorheight1=frontsector->floorplane.ZatPoint(linedef->v1);
		fs_floorheight2=frontsector->floorplane.ZatPoint(linedef->v2);
	}
	else
	{
		fs_floorheight2=fs_floorheight1=-frontsector->floorplane.d;
	}
	
	if (backsector->ceilingplane.a | backsector->ceilingplane.b)
	{
		bs_ceilingheight1=backsector->ceilingplane.ZatPoint(linedef->v1);
		bs_ceilingheight2=backsector->ceilingplane.ZatPoint(linedef->v2);
	}
	else
	{
		bs_ceilingheight2=bs_ceilingheight1=backsector->ceilingplane.d;
	}

	if (backsector->floorplane.a | backsector->floorplane.b)
	{
		bs_floorheight1=backsector->floorplane.ZatPoint(linedef->v1);
		bs_floorheight2=backsector->floorplane.ZatPoint(linedef->v2);
	}
	else
	{
		bs_floorheight2=bs_floorheight1=-backsector->floorplane.d;
	}

	// now check for closed sectors!
	if (bs_ceilingheight1<=fs_floorheight1 && bs_ceilingheight2<=fs_floorheight2) 
	{
		FTexture * tex = TexMan(sidedef->GetTexture(side_t::top));
		if (!tex || tex->UseType==FTexture::TEX_Null) return false;
		if (backsector->GetTexture(sector_t::ceiling)==skyflatnum && 
			frontsector->GetTexture(sector_t::ceiling)==skyflatnum) return false;
		return true;
	}

	if (fs_ceilingheight1<=bs_floorheight1 && fs_ceilingheight2<=bs_floorheight2) 
	{
		FTexture * tex = TexMan(sidedef->GetTexture(side_t::bottom));
		if (!tex || tex->UseType==FTexture::TEX_Null) return false;

		// properly render skies (consider door "open" if both floors are sky):
		if (backsector->GetTexture(sector_t::ceiling)==skyflatnum && 
			frontsector->GetTexture(sector_t::ceiling)==skyflatnum) return false;
		return true;
	}

	if (bs_ceilingheight1<=bs_floorheight1 && bs_ceilingheight2<=bs_floorheight2)
	{
		// preserve a kind of transparent door/lift special effect:
		if (bs_ceilingheight1 < fs_ceilingheight1 || bs_ceilingheight2 < fs_ceilingheight2) 
		{
			FTexture * tex = TexMan(sidedef->GetTexture(side_t::top));
			if (!tex || tex->UseType==FTexture::TEX_Null) return false;
		}
		if (bs_floorheight1 > fs_floorheight1 || bs_floorheight2 > fs_floorheight2)
		{
			FTexture * tex = TexMan(sidedef->GetTexture(side_t::bottom));
			if (!tex || tex->UseType==FTexture::TEX_Null) return false;
		}
		if (backsector->GetTexture(sector_t::ceiling)==skyflatnum && 
			frontsector->GetTexture(sector_t::ceiling)==skyflatnum) return false;
		if (backsector->GetTexture(sector_t::floor)==skyflatnum && frontsector->GetTexture(sector_t::floor)
			==skyflatnum) return false;
		return true;
	}

	return false;
}

//==========================================================================
//
// check for levels with exposed lower areas
//
//==========================================================================

void gl_CheckViewArea(vertex_t *v1, vertex_t *v2, sector_t *frontsector, sector_t *backsector)
{
	if (in_area==area_default && 
		(backsector->heightsec && !(backsector->heightsec->MoreFlags & SECF_IGNOREHEIGHTSEC)) &&
		(!frontsector->heightsec || frontsector->heightsec->MoreFlags & SECF_IGNOREHEIGHTSEC))
	{
		sector_t * s = backsector->heightsec;

		fixed_t cz1 = frontsector->ceilingplane.ZatPoint(v1);
		fixed_t cz2 = frontsector->ceilingplane.ZatPoint(v2);
		fixed_t fz1 = s->floorplane.ZatPoint(v1);
		fixed_t fz2 = s->floorplane.ZatPoint(v2);

		if (cz1<=fz1 && cz2<=fz2) 
			in_area=area_below;
		else 
			in_area=area_normal;
	}
}


//==========================================================================
//
//
//
//==========================================================================

static bool CopyPlaneIfValid (secplane_t *dest, const secplane_t *source, const secplane_t *opp)
{
	bool copy = false;

	// If the planes do not have matching slopes, then always copy them
	// because clipping would require creating new sectors.
	if (source->a != dest->a || source->b != dest->b || source->c != dest->c)
	{
		copy = true;
	}
	else if (opp->a != -dest->a || opp->b != -dest->b || opp->c != -dest->c)
	{
		if (source->d < dest->d)
		{
			copy = true;
		}
	}
	else if (source->d < dest->d && source->d > -opp->d)
	{
		copy = true;
	}

	if (copy)
	{
		*dest = *source;
	}

	return copy;
}




//==========================================================================
//
// This is mostly like R_FakeFlat but with a few alterations necessitated
// by hardware rendering
//
//==========================================================================
sector_t * gl_FakeFlat(sector_t * sec, sector_t * dest, area_t in_area, bool back)
{
	if (!sec->heightsec || sec->heightsec->MoreFlags & SECF_IGNOREHEIGHTSEC || sec->heightsec==sec) return sec;

#ifdef _MSC_VER
#ifdef _DEBUG
	if (sec-sectors==560)
	{
		__asm nop
	}
#endif
#endif

	if (in_area==area_above)
	{
		if (sec->heightsec->MoreFlags&SECF_FAKEFLOORONLY || sec->GetTexture(sector_t::ceiling)==skyflatnum) in_area=area_normal;
	}

	int diffTex = (sec->heightsec->MoreFlags & SECF_CLIPFAKEPLANES);
	sector_t * s = sec->heightsec;
		  
	*dest=*sec;
	// Replace floor and ceiling height with control sector's heights.
	if (diffTex)
	{
		if (CopyPlaneIfValid (&dest->floorplane, &s->floorplane, &sec->ceilingplane))
		{
			dest->SetTexture(sector_t::floor, s->GetTexture(sector_t::floor), false);
			dest->SetPlaneTexZ(sector_t::floor, s->GetPlaneTexZ(sector_t::floor));
			dest->vboindex[sector_t::floor] = sec->vboindex[sector_t::vbo_fakefloor];
			dest->vboheight[sector_t::floor] = s->vboheight[sector_t::floor];
		}
		else if (s->MoreFlags & SECF_FAKEFLOORONLY)
		{
			if (in_area==area_below)
			{
				dest->ColorMap=s->ColorMap;
				if (!(s->MoreFlags & SECF_NOFAKELIGHT))
				{
					dest->lightlevel  = s->lightlevel;
					dest->SetPlaneLight(sector_t::floor, s->GetPlaneLight(sector_t::floor));
					dest->SetPlaneLight(sector_t::ceiling, s->GetPlaneLight(sector_t::ceiling));
					dest->ChangeFlags(sector_t::floor, -1, s->GetFlags(sector_t::floor));
					dest->ChangeFlags(sector_t::ceiling, -1, s->GetFlags(sector_t::ceiling));
				}
				return dest;
			}
			return sec;
		}
	}
	else
	{
		dest->SetPlaneTexZ(sector_t::floor, s->GetPlaneTexZ(sector_t::floor));
		dest->floorplane   = s->floorplane;

		dest->vboindex[sector_t::floor] = sec->vboindex[sector_t::vbo_fakefloor];
		dest->vboheight[sector_t::floor] = s->vboheight[sector_t::floor];
	}

	if (!(s->MoreFlags&SECF_FAKEFLOORONLY))
	{
		if (diffTex)
		{
			if (CopyPlaneIfValid (&dest->ceilingplane, &s->ceilingplane, &sec->floorplane))
			{
				dest->SetTexture(sector_t::ceiling, s->GetTexture(sector_t::ceiling), false);
				dest->SetPlaneTexZ(sector_t::ceiling, s->GetPlaneTexZ(sector_t::ceiling));
				dest->vboindex[sector_t::ceiling] = sec->vboindex[sector_t::vbo_fakeceiling];
				dest->vboheight[sector_t::ceiling] = s->vboheight[sector_t::ceiling];
			}
		}
		else
		{
			dest->ceilingplane  = s->ceilingplane;
			dest->SetPlaneTexZ(sector_t::ceiling, s->GetPlaneTexZ(sector_t::ceiling));
			dest->vboindex[sector_t::ceiling] = sec->vboindex[sector_t::vbo_fakeceiling];
			dest->vboheight[sector_t::ceiling] = s->vboheight[sector_t::ceiling];
		}
	}

	if (in_area==area_below)
	{
		dest->ColorMap=s->ColorMap;
		dest->SetPlaneTexZ(sector_t::floor, sec->GetPlaneTexZ(sector_t::floor));
		dest->SetPlaneTexZ(sector_t::ceiling, s->GetPlaneTexZ(sector_t::floor));
		dest->floorplane=sec->floorplane;
		dest->ceilingplane=s->floorplane;
		dest->ceilingplane.FlipVert();

		dest->vboindex[sector_t::floor] = sec->vboindex[sector_t::floor];
		dest->vboheight[sector_t::floor] = sec->vboheight[sector_t::floor];

		dest->vboindex[sector_t::ceiling] = sec->vboindex[sector_t::vbo_fakefloor];
		dest->vboheight[sector_t::ceiling] = s->vboheight[sector_t::floor];

		if (!back)
		{
			dest->SetTexture(sector_t::floor, diffTex ? sec->GetTexture(sector_t::floor) : s->GetTexture(sector_t::floor), false);
			dest->planes[sector_t::floor].xform = s->planes[sector_t::floor].xform;

			//dest->ceilingplane		= s->floorplane;
			
			if (s->GetTexture(sector_t::ceiling) == skyflatnum) 
			{
				dest->SetTexture(sector_t::ceiling, dest->GetTexture(sector_t::floor), false);
				//dest->floorplane			= dest->ceilingplane;
				//dest->floorplane.FlipVert ();
				//dest->floorplane.ChangeHeight (+1);
				dest->planes[sector_t::ceiling].xform = dest->planes[sector_t::floor].xform;

			} 
			else 
			{
				dest->SetTexture(sector_t::ceiling, diffTex ? s->GetTexture(sector_t::floor) : s->GetTexture(sector_t::ceiling), false);
				dest->planes[sector_t::ceiling].xform = s->planes[sector_t::ceiling].xform;
			}
			
			if (!(s->MoreFlags & SECF_NOFAKELIGHT))
			{
				dest->lightlevel  = s->lightlevel;
				dest->SetPlaneLight(sector_t::floor, s->GetPlaneLight(sector_t::floor));
				dest->SetPlaneLight(sector_t::ceiling, s->GetPlaneLight(sector_t::ceiling));
				dest->ChangeFlags(sector_t::floor, -1, s->GetFlags(sector_t::floor));
				dest->ChangeFlags(sector_t::ceiling, -1, s->GetFlags(sector_t::ceiling));
			}
		}
	}
	else if (in_area==area_above)
	{
		dest->ColorMap=s->ColorMap;
		dest->SetPlaneTexZ(sector_t::ceiling, sec->GetPlaneTexZ(sector_t::ceiling));
		dest->SetPlaneTexZ(sector_t::floor, s->GetPlaneTexZ(sector_t::ceiling));
		dest->ceilingplane= sec->ceilingplane;
		dest->floorplane = s->ceilingplane;
		dest->floorplane.FlipVert();

		dest->vboindex[sector_t::floor] = sec->vboindex[sector_t::vbo_fakeceiling];
		dest->vboheight[sector_t::floor] = s->vboheight[sector_t::ceiling];

		dest->vboindex[sector_t::ceiling] = sec->vboindex[sector_t::ceiling];
		dest->vboheight[sector_t::ceiling] = sec->vboheight[sector_t::ceiling];

		if (!back)
		{
			dest->SetTexture(sector_t::ceiling, diffTex ? sec->GetTexture(sector_t::ceiling) : s->GetTexture(sector_t::ceiling), false);
			dest->SetTexture(sector_t::floor, s->GetTexture(sector_t::ceiling), false);
			dest->planes[sector_t::ceiling].xform = dest->planes[sector_t::floor].xform = s->planes[sector_t::ceiling].xform;
			
			if (s->GetTexture(sector_t::floor) != skyflatnum)
			{
				dest->SetTexture(sector_t::floor, s->GetTexture(sector_t::floor), false);
				dest->planes[sector_t::floor].xform = s->planes[sector_t::floor].xform;
			}
			
			if (!(s->MoreFlags & SECF_NOFAKELIGHT))
			{
				dest->lightlevel  = s->lightlevel;
				dest->SetPlaneLight(sector_t::floor, s->GetPlaneLight(sector_t::floor));
				dest->SetPlaneLight(sector_t::ceiling, s->GetPlaneLight(sector_t::ceiling));
				dest->ChangeFlags(sector_t::floor, -1, s->GetFlags(sector_t::floor));
				dest->ChangeFlags(sector_t::ceiling, -1, s->GetFlags(sector_t::ceiling));
			}
		}
	}
	return dest;
}


