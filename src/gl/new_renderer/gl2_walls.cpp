//
//-----------------------------------------------------------------------------
//
// Copyright (C) 2009 Christoph Oelckers
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// As an exception to the GPL this code may be used in GZDoom
// derivatives under the following conditions:
//
// 1. The license of these files is not changed
// 2. Full source of the derived program is disclosed
//
//
// ----------------------------------------------------------------------------
//
// Flat processing / rendering
//

#include "gl/gl_include.h"
#include "r_main.h"
#include "r_defs.h"
#include "r_state.h"
#include "v_palette.h"
#include "a_sharedglobal.h"
#include "r_sky.h"
#include "g_level.h"
#include "v_video.h"
#include "p_lnspec.h"
#include "doomdata.h"
#include "gl/common/glc_geometric.h"
#include "gl/common/glc_convert.h"
#include "gl/common/glc_data.h"
#include "gl/new_renderer/gl2_renderer.h"
#include "gl/new_renderer/gl2_geom.h"
#include "gl/new_renderer/textures/gl2_material.h"

#include "gl/gl_functions.h"

EXTERN_CVAR(Bool, gl_fakecontrast)

namespace GLRendererNew
{

void FWallRenderData::CreatePrimitives(GLDrawInfo *di, FWallObject *plane)
{
}

void FWallRenderData::Init(int side)
{
}

//==========================================================================
//
// 
//
//==========================================================================

void FWallRenderData::Validate(seg_t *seg, sector_t *fs, sector_t *bs, subsector_t *polysub, area_t in_area)
{
	vertex_t * v1, * v2;
	sector_t *realfront;
	sector_t *realback;
	FWallData wd;
	FMaterial *mat = NULL;

	if (mSide->dirty)
	{
		mAreas[0].valid =
		mAreas[1].valid =
		mAreas[2].valid =
		mSide->dirty = false;
	}

	FWallAreaData *area = &mAreas[in_area];
	if (!area->valid)
	{
		mSub=polysub ? polysub : seg->Subsector;

		if (polysub && seg->backsector != NULL)
		{
			// Textures on 2-sided polyobjects are aligned to the actual seg's sectors.
			realfront = seg->frontsector;
			realback = seg->backsector;
		}
		else
		{
			// Need these for aligning the textures.
			realfront = &sectors[fs->sectornum];
			realback = bs? &sectors[bs->sectornum] : NULL;
		}

		if (seg->sidedef == seg->linedef->sidedef[0])
		{
			v1=seg->linedef->v1;
			v2=seg->linedef->v2;
		}
		else
		{
			v1=seg->linedef->v2;
			v2=seg->linedef->v1;
		}
		if (v1->dirty) gl_RecalcVertexHeights(v1);
		if (v2->dirty) gl_RecalcVertexHeights(v2);

	}

	wd.v[0] = v1;
	wd.v[1] = v2;

	wd.foggy = (!gl_isBlack(fs->ColorMap->Fade) || level.flags&LEVEL_HASFADETABLE);

	wd.lightlevel = seg->sidedef->GetLightLevel(true, fs->lightlevel);

	if (wd.lightlevel<255 && gl_fakecontrast && !wd.foggy)
	{
		// In GL it's preferable to use the relative light for fake contrast instead of
		// altering the base light level which is also used to set fog density.
		wd.rellight = seg->sidedef->GetLightLevel(false, fs->lightlevel) - wd.lightlevel;
	}
	else wd.rellight=0;

	wd.topflat = fs->GetTexture(sector_t::ceiling);	// for glowing textures. These must be saved because
	wd.bottomflat = fs->GetTexture(sector_t::floor);	// the sector passed here might be a temporary copy.

	// Save a little time (up to 0.3 ms per frame ;) )
	if (fs->floorplane.a | fs->floorplane.b)
	{
		wd.ffh1 = fs->floorplane.ZatPoint(v1); 
		wd.ffh2 = fs->floorplane.ZatPoint(v2); 
	}
	else
	{
		wd.ffh1 = wd.ffh2 = fs->GetPlaneTexZ(sector_t::floor); 
	}

	if (fs->ceilingplane.a | fs->ceilingplane.b)
	{
		wd.fch1 = fs->ceilingplane.ZatPoint(v1);
		wd.fch2 = fs->ceilingplane.ZatPoint(v2);
	}
	else
	{
		wd.fch1 = wd.fch2 = fs->GetPlaneTexZ(sector_t::ceiling);
	}

	if (seg->linedef->special==Line_Horizon)
	{
		//CreateNormalSky(fs,v1,v2);
		//CreateHorizon(seg,fs, v1,v2);
		return;
	}

	if (!bs || !(seg->linedef->flags&ML_TWOSIDED)) // one sided
	{
		// sector's sky
		//CreateNormalSky(fs,v1,v2);
		
		// normal texture
		FMaterial *mat = GLRenderer2->GetMaterial(seg->sidedef->GetTexture(side_t::mid), true, false, 0);
		if (mat != NULL)
		{
			/*
			CreateWall(seg,(seg->linedef->flags & ML_DONTPEGBOTTOM) > 0,
					  realfront->GetPlaneTexZ(sector_t::ceiling),
					  realfront->GetPlaneTexZ(sector_t::floor),	// must come from the original!
					  wd.fch1, wd.fch2, wd.ffh1, wd.ffh2,0);
			*/
		}
	}
	else // two sided
	{
		if (bs->floorplane.a | bs->floorplane.b)
		{
			wd.bfh1 = bs->floorplane.ZatPoint(v1); 
			wd.bfh2 = bs->floorplane.ZatPoint(v2); 
		}
		else
		{
			wd.bfh1 = wd.bfh2 = bs->GetPlaneTexZ(sector_t::floor); 
		}

		if (bs->ceilingplane.a | bs->ceilingplane.b)
		{
			wd.bch1 = bs->ceilingplane.ZatPoint(v1);
			wd.bch2 = bs->ceilingplane.ZatPoint(v2);
		}
		else
		{
			wd.bch1 = wd.bch2 = bs->GetPlaneTexZ(sector_t::ceiling);
		}

		//CreateSkyTop(seg,fs,bs,v1,v2);
		//CreateSkyBottom(seg,fs,bs,v1,v2);

		// upper texture
		if (fs->GetTexture(sector_t::ceiling)!=skyflatnum || bs->GetTexture(sector_t::ceiling)!=skyflatnum)
		{
			fixed_t bch1a = wd.bch1, bch2a = wd.bch2;
	
			if (fs->GetTexture(sector_t::floor)!=skyflatnum || bs->GetTexture(sector_t::floor)!=skyflatnum)
			{
				// the back sector's floor obstructs part of this wall				
				if (wd.ffh1 > wd.bch1 && wd.ffh2 > wd.bch2) 
				{
					bch2a = wd.ffh2;
					bch1a = wd.ffh1;
				}
			}

			if (bch1a < wd.fch1 || bch2a < wd.fch2)
			{
				mat = GLRenderer2->GetMaterial(seg->sidedef->GetTexture(side_t::top), true, false, 0);
	
				if (mat != NULL) 
				{
					/*
					CreateWall(seg,(seg->linedef->flags & (ML_DONTPEGTOP))==0,
						realfront->GetPlaneTexZ(sector_t::ceiling),
						realback->GetPlaneTexZ(sector_t::ceiling),
						wd.fch1,wd.fch2,bch1a,bch2a,0);
					*/
				}
				else if ((fs->ceilingplane.a | fs->ceilingplane.b | 
						 bs->ceilingplane.a | bs->ceilingplane.b) && 
						fs->GetTexture(sector_t::ceiling) != skyflatnum &&
						bs->GetTexture(sector_t::ceiling) != skyflatnum)
				{
					// missinh upper texture in a sloped sector - full with ceiling.
					mat = GLRenderer2->GetMaterial(fs->GetTexture(sector_t::ceiling), true, false, 0);
					if (mat != NULL)
					{
						/*
						CreateWall(seg,(seg->linedef->flags & (ML_DONTPEGTOP))==0,
							realfront->GetPlaneTexZ(sector_t::ceiling),
							realback->GetPlaneTexZ(sector_t::ceiling),
							wd.fch1,wd.fch2,bch1a,bch2a,0);
						*/
					}
				}
				else
				{
					//CreateUpperMissingTexture(seg, bch1a);
				}
			}
		}

		/* mid texture */

		// in all other cases this might create more problems than it solves.
		bool drawfogboundary=((fs->ColorMap->Fade&0xffffff)!=0 && 
							(bs->ColorMap->Fade&0xffffff)==0 &&
							(fs->GetTexture(sector_t::ceiling)!=skyflatnum || bs->GetTexture(sector_t::ceiling)!=skyflatnum));

		mat = GLRenderer2->GetMaterial(seg->sidedef->GetTexture(side_t::mid), true, false, 0);

		if (mat != NULL || drawfogboundary)
		{
			//CreateMidTexture(seg, drawfogboundary, realfront, realback, fch1, fch2, ffh1, ffh2, bch1, bch2, bfh1, bfh2);
		}

		if (bs->e->XFloor.ffloors.Size() || fs->e->XFloor.ffloors.Size()) 
		{
			//CreateFFloorBlocks(seg,fs,bs, fch1, fch2, ffh1, ffh2, bch1, bch2, bfh1, bfh2);
		}
		
		/* bottom texture */
		if (fs->GetTexture(sector_t::ceiling)!=skyflatnum || bs->GetTexture(sector_t::ceiling)!=skyflatnum)
		{
			// the back sector's ceiling obstructs part of this wall				
			if (wd.fch1 < wd.bfh1 && wd.fch2 < wd.bfh2)
			{
				wd.bfh1 = wd.fch1;
				wd.bfh2 = wd.fch2;
			}
		}

		if (wd.bfh1 > wd.ffh1 || wd.bfh2 > wd.ffh2)
		{
			mat = GLRenderer2->GetMaterial(seg->sidedef->GetTexture(side_t::bottom), true, false, 0);
			if (mat != NULL) 
			{
				/*
				CreateWall(seg,(seg->linedef->flags & ML_DONTPEGBOTTOM)>0,
					realback->GetPlaneTexZ(sector_t::floor),realfront->GetPlaneTexZ(sector_t::floor),
					wd.bfh1,wd.bfh2,wd.ffh1,wd.ffh2,
					fs->GetTexture(sector_t::ceiling)==skyflatnum && bs->GetTexture(sector_t::ceiling)==skyflatnum ?
						realfront->GetPlaneTexZ(sector_t::floor)-realback->GetPlaneTexZ(sector_t::ceiling) : 
						realfront->GetPlaneTexZ(sector_t::floor)-realfront->GetPlaneTexZ(sector_t::ceiling));
				*/
			}
			else if ((fs->floorplane.a | fs->floorplane.b | 
					bs->floorplane.a | bs->floorplane.b) && 
					fs->GetTexture(sector_t::floor) != skyflatnum &&
					bs->GetTexture(sector_t::floor) != skyflatnum)
			{
				// render it anyway with the sector's floor texture. With a background sky
				// there are ugly holes otherwise and slopes are simply not precise enough
				// to mach in any case.
				mat = GLRenderer2->GetMaterial(fs->GetTexture(sector_t::floor), true, false, 0);
				if (mat != NULL) 
				{
					/*
					CreateWall(seg,(seg->linedef->flags & ML_DONTPEGBOTTOM)>0,
						realback->GetPlaneTexZ(sector_t::floor),realfront->GetPlaneTexZ(sector_t::floor),
						wd.bfh1,wd.bfh2,wd.ffh1,wd.ffh2, realfront->GetPlaneTexZ(sector_t::floor)-realfront->GetPlaneTexZ(sector_t::ceiling));
					*/
				}
			}
			else if (bs->GetTexture(sector_t::floor)!=skyflatnum)
			{
				//CreateLowerMissingTexture(seg, bfh1);
			}
		}
	}
}

//===========================================================================
// 
//
//
//===========================================================================

void FWallRenderData::Process(seg_t *seg, sector_t *fs, sector_t *bs, subsector_t *polysub, area_t in_area)
{
	/*
	GLDrawInfo *di = GLRenderer2->mCurrentDrawInfo;
	side_t *sd = seg->sidedef;

	if (in_area >= area_above) in_area = area_normal;

	Validate(seg, fs, bs, polysub, in_area);

	FWallAreaData *area = &mAreas[in_area];

	if (area->mUpper.mPrimitiveIndex >= 0) di->AddObject(&area->mUpper);
	else if (area->mUpper.mPrimitiveIndex == -2) di->AddUpperMissingTexture(seg, area->mUpper.mPrimitiveCount);
	if (area->mMiddle.mPrimitiveIndex >= 0) di->AddObject(&area->mMiddle);
	if (area->mLower.mPrimitiveIndex >= 0) di->AddObject(&area->mLower);
	else if (area->mLower.mPrimitiveIndex == -2) di->AddUpperMissingTexture(seg, area->mLower.mPrimitiveCount);
	
	for(unsigned k = 0; k < area->m3DFloorParts.Size(); k++)
	{
		di->AddObject(&area->m3DFloorParts[k]);
	}

	if (area->mFogBoundary.mPrimitiveIndex >= 0) di->AddObject(&area->mFogBoundary);
	*/
}



}
