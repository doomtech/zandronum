#include "gl/gl_include.h"
#include "gl/gl_intern.h"
#include "p_local.h"


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
		if (sec->heightsec->MoreFlags&SECF_FAKEFLOORONLY || sec->ceilingpic==skyflatnum) in_area=area_normal;
	}

	int diffTex = (sec->heightsec->MoreFlags & SECF_CLIPFAKEPLANES);
	sector_t * s = sec->heightsec;
		  
	*dest=*sec;
	// Replace floor and ceiling height with control sector's heights.
	if (diffTex)
	{
		if (CopyPlaneIfValid (&dest->floorplane, &s->floorplane, &sec->ceilingplane))
		{
			dest->floorpic = s->floorpic;
			dest->floortexz   = s->floortexz;
		}
		else if (s->MoreFlags & SECF_FAKEFLOORONLY)
		{
			if (in_area==area_below)
			{
				dest->ColorMap=s->ColorMap;
				if (!(s->MoreFlags & SECF_NOFAKELIGHT))
				{
					dest->lightlevel  = s->lightlevel;
					dest->FloorLight = s->FloorLight;
					dest->CeilingLight = s->CeilingLight;
					dest->FloorFlags = s->FloorFlags;
					dest->CeilingFlags = s->CeilingFlags;
				}
				return dest;
			}
			return sec;
		}
	}
	else
	{
		dest->floortexz   = s->floortexz;
		dest->floorplane   = s->floorplane;
	}

	if (!(s->MoreFlags&SECF_FAKEFLOORONLY))
	{
		if (diffTex)
		{
			if (CopyPlaneIfValid (&dest->ceilingplane, &s->ceilingplane, &sec->floorplane))
			{
				dest->ceilingpic = s->ceilingpic;
				dest->ceilingtexz = s->ceilingtexz;
			}
		}
		else
		{
			dest->ceilingplane  = s->ceilingplane;
			dest->ceilingtexz = s->ceilingtexz;
		}
	}

	if (in_area==area_below)
	{
		dest->ColorMap=s->ColorMap;
		dest->floortexz = sec->floortexz;
		dest->ceilingtexz = s->floortexz;
		dest->floorplane=sec->floorplane;
		dest->ceilingplane=s->floorplane;
		dest->ceilingplane.FlipVert();

		if (!back)
		{
			dest->floorpic = diffTex ? sec->floorpic : s->floorpic;
			dest->planes[sector_t::floor].xform = s->planes[sector_t::floor].xform;

			dest->ceilingplane		= s->floorplane;
			
			if (s->ceilingpic == skyflatnum) 
			{
				dest->ceilingpic    = dest->floorpic;
				//dest->floorplane			= dest->ceilingplane;
				//dest->floorplane.FlipVert ();
				//dest->floorplane.ChangeHeight (+1);
				dest->ceilingpic			= dest->floorpic;
				dest->planes[sector_t::ceiling].xform = dest->planes[sector_t::floor].xform;

			} 
			else 
			{
				dest->ceilingpic			= diffTex ? s->floorpic : s->ceilingpic;
				dest->planes[sector_t::ceiling].xform = s->planes[sector_t::ceiling].xform;
			}
			
			if (!(s->MoreFlags & SECF_NOFAKELIGHT))
			{
				dest->lightlevel  = s->lightlevel;
				dest->FloorLight = s->FloorLight;
				dest->CeilingLight = s->CeilingLight;
				dest->FloorFlags = s->FloorFlags;
				dest->CeilingFlags = s->CeilingFlags;
			}
		}
	}
	else if (in_area==area_above)
	{
		dest->ColorMap=s->ColorMap;
		dest->ceilingtexz = sec->ceilingtexz;
		dest->floortexz   = s->ceilingtexz;
		dest->ceilingplane= sec->ceilingplane;
		dest->floorplane = s->ceilingplane;
		dest->floorplane.FlipVert();

		if (!back)
		{
			dest->ceilingpic = diffTex ? sec->ceilingpic : s->ceilingpic;
			dest->floorpic			= s->ceilingpic;
			dest->planes[sector_t::ceiling].xform = dest->planes[sector_t::floor].xform = s->planes[sector_t::ceiling].xform;
			
			if (s->floorpic != skyflatnum)
			{
				dest->ceilingplane	= sec->ceilingplane;
				dest->floorpic		= s->floorpic;
				dest->planes[sector_t::floor].xform = s->planes[sector_t::floor].xform;
			}
			
			if (!(s->MoreFlags & SECF_NOFAKELIGHT))
			{
				dest->lightlevel  = s->lightlevel;
				dest->FloorLight = s->FloorLight;
				dest->CeilingLight = s->CeilingLight;
				dest->FloorFlags = s->FloorFlags;
				dest->CeilingFlags = s->CeilingFlags;
			}
		}
	}
	return dest;
}


