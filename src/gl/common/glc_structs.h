#ifndef __GLC_STRUCTS_H
#define __GLC_STRUCTS_H

#include "textures/textures.h"
//==========================================================================
//
// One sector plane, still in fixed point
//
//==========================================================================

struct GLSectorPlane
{
	FTextureID texture;
	secplane_t plane;
	fixed_t texheight;
	fixed_t xoffs,  yoffs;
	fixed_t	xscale, yscale;
	angle_t	angle;

	void GetFromSector(sector_t * sec, int ceiling)
	{
		xoffs = sec->GetXOffset(ceiling);
		yoffs = sec->GetYOffset(ceiling);
		xscale = sec->GetXScale(ceiling);
		yscale = sec->GetYScale(ceiling);
		angle = sec->GetAngle(ceiling);
		texture = sec->GetTexture(ceiling);
		plane = sec->GetSecPlane(ceiling);
		texheight = (ceiling == sector_t::ceiling)? plane.d : -plane.d;
	}
};



#endif