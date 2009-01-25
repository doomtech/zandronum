#include "gl/gl_include.h"
#include "gl/gl_intern.h"
#include "p_local.h"
#include "r_sky.h"


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
sector_t * gl_FakeFlat(sector_t * sec, sector_t * dest, bool back)
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
	}

	if (!(s->MoreFlags&SECF_FAKEFLOORONLY))
	{
		if (diffTex)
		{
			if (CopyPlaneIfValid (&dest->ceilingplane, &s->ceilingplane, &sec->floorplane))
			{
				dest->SetTexture(sector_t::ceiling, s->GetTexture(sector_t::ceiling), false);
				dest->SetPlaneTexZ(sector_t::ceiling, s->GetPlaneTexZ(sector_t::ceiling));
			}
		}
		else
		{
			dest->ceilingplane  = s->ceilingplane;
			dest->SetPlaneTexZ(sector_t::ceiling, s->GetPlaneTexZ(sector_t::ceiling));
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

		if (!back)
		{
			dest->SetTexture(sector_t::floor, diffTex ? sec->GetTexture(sector_t::floor) : s->GetTexture(sector_t::floor), false);
			dest->planes[sector_t::floor].xform = s->planes[sector_t::floor].xform;

			dest->ceilingplane		= s->floorplane;
			
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

		if (!back)
		{
			dest->SetTexture(sector_t::ceiling, diffTex ? sec->GetTexture(sector_t::ceiling) : s->GetTexture(sector_t::ceiling), false);
			dest->SetTexture(sector_t::floor, s->GetTexture(sector_t::ceiling), false);
			dest->planes[sector_t::ceiling].xform = dest->planes[sector_t::floor].xform = s->planes[sector_t::ceiling].xform;
			
			if (s->GetTexture(sector_t::floor) != skyflatnum)
			{
				dest->ceilingplane	= sec->ceilingplane;
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


