#include "r_main.h"
#include "r_plane.h"
#include "r_draw.h"
#include "r_things.h"

#include "zgl_main.h"


extern viewarea_t ViewArea;
extern viewarea_t InArea;


int GetFloorLight (const sector_t *sec);
int GetCeilingLight (const sector_t *sec);


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
sector_t * GL_FakeFlat(sector_t * sec, sector_t * dest, int *floorlightlevel, int *ceilinglightlevel, bool back)
{
#if 1
   return R_FakeFlat(sec, dest, floorlightlevel, ceilinglightlevel, back);
#else
   if (!sec) return NULL;

   // [RH] allow per-plane lighting
	if (floorlightlevel != NULL)
	{
		*floorlightlevel = GetFloorLight (sec);
	}

	if (ceilinglightlevel != NULL)
	{
		*ceilinglightlevel = GetCeilingLight (sec);
	}

	if (!sec->heightsec || (sec->heightsec->MoreFlags & SECF_IGNOREHEIGHTSEC) || sec->heightsec == sec) return sec;

	if (InArea == AREA_ABOVE)
	{
		if (sec->heightsec->MoreFlags&SECF_FAKEFLOORONLY || sec->ceilingpic==skyflatnum) InArea = AREA_NORMAL;
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
			if (InArea == AREA_BELOW)
			{
				dest->ColorMap = s->ColorMap;
				if (!(s->MoreFlags & SECF_NOFAKELIGHT))
				{
					dest->lightlevel  = s->lightlevel;
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

	if (InArea == AREA_BELOW)
	{
		dest->ColorMap = s->ColorMap;
		dest->floortexz = sec->floortexz;
		dest->ceilingtexz = s->floortexz;
		dest->floorplane=sec->floorplane;
		dest->ceilingplane=s->floorplane;
		//dest->ceilingplane.FlipVert();

		if (!back)
		{
			dest->floorpic = diffTex ? sec->floorpic : s->floorpic;
			dest->floor_xoffs		= s->floor_xoffs;
			dest->floor_yoffs		= s->floor_yoffs;
			dest->floor_xscale		= s->floor_xscale;
			dest->floor_yscale		= s->floor_yscale;
			dest->floor_angle		= s->floor_angle;
			dest->base_floor_angle	= s->base_floor_angle;
			dest->base_floor_yoffs	= s->base_floor_yoffs;

			dest->ceilingplane		= s->floorplane;
			
			if (s->ceilingpic == skyflatnum) 
			{
				dest->ceilingpic    = dest->floorpic;
				//dest->floorplane			= dest->ceilingplane;
				//dest->floorplane.FlipVert ();
				//dest->floorplane.ChangeHeight (+1);
				dest->ceilingpic			= dest->floorpic;
				dest->ceiling_xoffs			= dest->floor_xoffs;
				dest->ceiling_yoffs			= dest->floor_yoffs;
				dest->ceiling_xscale		= dest->floor_xscale;
				dest->ceiling_yscale		= dest->floor_yscale;
				dest->ceiling_angle			= dest->floor_angle;
				dest->base_ceiling_angle	= dest->base_floor_angle;
				dest->base_ceiling_yoffs	= dest->base_floor_yoffs;

			} 
			else 
			{
				dest->ceilingpic			= diffTex ? s->floorpic : s->ceilingpic;
				dest->ceiling_xoffs			= s->ceiling_xoffs;
				dest->ceiling_yoffs			= s->ceiling_yoffs;
				dest->ceiling_xscale		= s->ceiling_xscale;
				dest->ceiling_yscale		= s->ceiling_yscale;
				dest->ceiling_angle			= s->ceiling_angle;
				dest->base_ceiling_angle	= s->base_ceiling_angle;
				dest->base_ceiling_yoffs	= s->base_ceiling_yoffs;
			}
			
			if (!(s->MoreFlags & SECF_NOFAKELIGHT))
			{
				dest->lightlevel  = s->lightlevel;
			}
		}
	}
	else if (InArea == AREA_ABOVE)
	{
		dest->ColorMap = s->ColorMap;
		dest->ceilingtexz = sec->ceilingtexz;
		dest->floortexz   = s->ceilingtexz;
		dest->ceilingplane= sec->ceilingplane;
		dest->floorplane = s->ceilingplane;
		//dest->floorplane.FlipVert();

		if (!back)
		{
			dest->ceilingpic = diffTex ? sec->ceilingpic : s->ceilingpic;
			dest->floorpic			= s->ceilingpic;
			dest->floor_xoffs		= dest->ceiling_xoffs		= s->ceiling_xoffs;
			dest->floor_yoffs		= dest->ceiling_yoffs		= s->ceiling_yoffs;
			dest->floor_xscale		= dest->ceiling_xscale		= s->ceiling_xscale;
			dest->floor_yscale		= dest->ceiling_yscale		= s->ceiling_yscale;
			dest->floor_angle		= dest->ceiling_angle		= s->ceiling_angle;
			dest->base_floor_angle	= dest->base_ceiling_angle	= s->base_ceiling_angle;
			dest->base_floor_yoffs	= dest->base_ceiling_yoffs	= s->base_ceiling_yoffs;
			
			if (s->floorpic != skyflatnum)
			{
				dest->ceilingplane	= sec->ceilingplane;
				dest->floorpic		= s->floorpic;
				dest->floor_xoffs	= s->floor_xoffs;
				dest->floor_yoffs	= s->floor_yoffs;
				dest->floor_xscale	= s->floor_xscale;
				dest->floor_yscale	= s->floor_yscale;
				dest->floor_angle	= s->floor_angle;
			}
			
			if (!(s->MoreFlags & SECF_NOFAKELIGHT))
			{
				dest->lightlevel  = s->lightlevel;
			}
		}
	}
//	dest->slopeflag= ((dest->floorplane.a | dest->floorplane.b) ? 1:0) +
//					 ((dest->ceilingplane.a | dest->ceilingplane.b) ? 2:0);
	return dest;
#endif
}
