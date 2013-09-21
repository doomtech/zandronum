/*
** gl_missingtexture.cpp
** Handles missing upper and lower textures and self referencing sector hacks
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

#include "a_sharedglobal.h"
#include "r_main.h"
#include "r_sky.h"
#include "g_level.h"


#include "gl/renderer/gl_renderer.h"
#include "gl/data/gl_data.h"
#include "gl/dynlights/gl_glow.h"
#include "gl/scene/gl_drawinfo.h"
#include "gl/utility/gl_clock.h"
#include "gl/utility/gl_templates.h"

int GetFloorLight (const sector_t *sec);
int GetCeilingLight (const sector_t *sec);


// This is for debugging maps.
CVAR(Bool, gl_notexturefill, false, 0);


FreeList<gl_subsectorrendernode> SSR_List;

// profiling data
static int totalupper, totallower;
static int lowershcount, uppershcount;
static glcycle_t totalms, showtotalms;
static glcycle_t totalssms;
static sector_t fakesec;

void FDrawInfo::ClearBuffers()
{
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
}
//==========================================================================
//
// Adds a subsector plane to a sector's render list
//
//==========================================================================

void FDrawInfo::AddOtherFloorPlane(int sector, gl_subsectorrendernode * node)
{
	int oldcnt = otherfloorplanes.Size();

	if (oldcnt<=sector)
	{
		otherfloorplanes.Resize(sector+1);
		for(int i=oldcnt;i<=sector;i++) otherfloorplanes[i]=NULL;
	}
	node->next = otherfloorplanes[sector];
	otherfloorplanes[sector] = node;
}

void FDrawInfo::AddOtherCeilingPlane(int sector, gl_subsectorrendernode * node)
{
	int oldcnt = otherceilingplanes.Size();

	if (oldcnt<=sector)
	{
		otherceilingplanes.Resize(sector+1);
		for(int i=oldcnt;i<=sector;i++) otherceilingplanes[i]=NULL;
	}
	node->next = otherceilingplanes[sector];
	otherceilingplanes[sector] = node;
}


//==========================================================================
//
// Collects all sectors that might need a fake ceiling
//
//==========================================================================
void FDrawInfo::AddUpperMissingTexture(seg_t * seg, fixed_t backheight)
{
	if (!seg->backsector) return;

	totalms.Clock();
	MissingTextureInfo mti = {};
	MissingSegInfo msi;

	if (!seg->Subsector) 
	{
		msi.MTI_Index = -1;
		msi.seg=seg;
		MissingLowerSegs.Push(msi);
		return;
	}

	subsector_t * sub = seg->Subsector;

	if (sub->render_sector != sub->sector || seg->frontsector != sub->sector) 
	{
		totalms.Unclock();
		return;
	}

	for(unsigned int i=0;i<MissingUpperTextures.Size();i++)
	{
		if (MissingUpperTextures[i].sub == sub)
		{
			// Use the lowest adjoining height to draw a fake ceiling if necessary
			if (backheight < MissingUpperTextures[i].planez) 
			{
				MissingUpperTextures[i].planez = backheight;
				MissingUpperTextures[i].seg = seg;
			}

			msi.MTI_Index = i;
			msi.seg=seg;
			MissingUpperSegs.Push(msi);

			totalms.Unclock();
			return;
		}
	}
	mti.seg=seg;
	mti.sub=sub;
	mti.planez=backheight;
	msi.MTI_Index = MissingUpperTextures.Push(mti);
	msi.seg=seg;
	MissingUpperSegs.Push(msi);
	totalms.Unclock();
}

//==========================================================================
//
// Collects all sectors that might need a fake floor
//
//==========================================================================
void FDrawInfo::AddLowerMissingTexture(seg_t * seg, fixed_t backheight)
{
	if (!seg->backsector) return;
	if (seg->backsector->transdoor)
	{
		if (seg->backsector->transdoorheight == seg->backsector->GetPlaneTexZ(sector_t::floor)) return;
	}

	totalms.Clock();
	MissingTextureInfo mti = {};
	MissingSegInfo msi;

	if (!seg->Subsector) 
	{
		msi.MTI_Index = -1;
		msi.seg=seg;
		MissingLowerSegs.Push(msi);
		return;
	}

	subsector_t * sub = seg->Subsector;

	if (sub->render_sector != sub->sector || seg->frontsector != sub->sector) 
	{
		totalms.Unclock();
		return;
	}

	// Ignore FF_FIX's because they are designed to abuse missing textures
	if (seg->backsector->e->XFloor.ffloors.Size() && seg->backsector->e->XFloor.ffloors[0]->flags&FF_FIX)
	{
		totalms.Unclock();
		return;
	}

	for(unsigned int i=0;i<MissingLowerTextures.Size();i++)
	{
		if (MissingLowerTextures[i].sub == sub)
		{
			// Use the highest adjoining height to draw a fake floor if necessary
			if (backheight > MissingLowerTextures[i].planez) 
			{
				MissingLowerTextures[i].planez = backheight;
				MissingLowerTextures[i].seg = seg;
			}

			msi.MTI_Index = i;
			msi.seg=seg;
			MissingLowerSegs.Push(msi);

			totalms.Unclock();
			return;
		}
	}
	mti.seg=seg;
	mti.sub = sub;
	mti.planez=backheight;
	msi.MTI_Index = MissingLowerTextures.Push(mti);
	msi.seg=seg;
	MissingLowerSegs.Push(msi);
	totalms.Unclock();
}


//==========================================================================
//
// 
//
//==========================================================================
bool FDrawInfo::DoOneSectorUpper(subsector_t * subsec, fixed_t planez)
{
	// Is there a one-sided wall in this sector?
	// Do this first to avoid unnecessary recursion
	for(DWORD i=0; i< subsec->numlines; i++)
	{
		if (segs[subsec->firstline + i].backsector == NULL) return false;
		if (segs[subsec->firstline + i].PartnerSeg == NULL) return false;
	}

	for(DWORD i=0; i< subsec->numlines; i++)
	{
		seg_t * seg = &segs[subsec->firstline + i];
		subsector_t * backsub = seg->PartnerSeg->Subsector;

		// already checked?
		if (backsub->validcount == validcount) continue;	
		backsub->validcount=validcount;

		if (seg->frontsector != seg->backsector && seg->linedef)
		{
			// Note: if this is a real line between sectors
			// we can be sure that render_sector is the real sector!

			sector_t * sec = gl_FakeFlat(seg->backsector, &fakesec, true);

			// Don't bother with slopes
			if (sec->ceilingplane.a!=0 || sec->ceilingplane.b!=0)  return false;

			// Is the neighboring ceiling lower than the desired height?
			if (sec->GetPlaneTexZ(sector_t::ceiling)<planez) 
			{
				// todo: check for missing textures.
				return false;
			}

			// This is an exact height match which means we don't have to do any further checks for this sector
			if (sec->GetPlaneTexZ(sector_t::ceiling)==planez) 
			{
				// If there's a texture abort
				FTexture * tex = TexMan[seg->sidedef->GetTexture(side_t::top)];
				if (!tex || tex->UseType==FTexture::TEX_Null) continue;
				else return false;
			}
		}
		if (!DoOneSectorUpper(backsub, planez)) return false;
	}
	// all checked ok. This subsector is part of the current fake plane

	HandledSubsectors.Push(subsec);
	return 1;
}

//==========================================================================
//
// 
//
//==========================================================================
bool FDrawInfo::DoOneSectorLower(subsector_t * subsec, fixed_t planez)
{
	// Is there a one-sided wall in this subsector?
	// Do this first to avoid unnecessary recursion
	for(DWORD i=0; i< subsec->numlines; i++)
	{
		if (segs[subsec->firstline + i].backsector == NULL) return false;
		if (segs[subsec->firstline + i].PartnerSeg == NULL) return false;
	}

	for(DWORD i=0; i< subsec->numlines; i++)
	{
		seg_t * seg = &segs[subsec->firstline + i];
		subsector_t * backsub = seg->PartnerSeg->Subsector;

		// already checked?
		if (backsub->validcount == validcount) continue;	
		backsub->validcount=validcount;

		if (seg->frontsector != seg->backsector && seg->linedef)
		{
			// Note: if this is a real line between sectors
			// we can be sure that render_sector is the real sector!

			sector_t * sec = gl_FakeFlat(seg->backsector, &fakesec, true);

			// Don't bother with slopes
			if (sec->floorplane.a!=0 || sec->floorplane.b!=0)  return false;

			// Is the neighboring floor higher than the desired height?
			if (sec->GetPlaneTexZ(sector_t::floor)>planez) 
			{
				// todo: check for missing textures.
				return false;
			}

			// This is an exact height match which means we don't have to do any further checks for this sector
			if (sec->GetPlaneTexZ(sector_t::floor)==planez) 
			{
				// If there's a texture abort
				FTexture * tex = TexMan[seg->sidedef->GetTexture(side_t::bottom)];
				if (!tex || tex->UseType==FTexture::TEX_Null) continue;
				else return false;
			}
		}
		if (!DoOneSectorLower(backsub, planez)) return false;
	}
	// all checked ok. This sector is part of the current fake plane

	HandledSubsectors.Push(subsec);
	return 1;
}


//==========================================================================
//
//
//
//==========================================================================
bool FDrawInfo::DoFakeBridge(subsector_t * subsec, fixed_t planez)
{
	// Is there a one-sided wall in this sector?
	// Do this first to avoid unnecessary recursion
	for(DWORD i=0; i< subsec->numlines; i++)
	{
		if (segs[subsec->firstline + i].backsector == NULL) return false;
		if (segs[subsec->firstline + i].PartnerSeg == NULL) return false;
	}

	for(DWORD i=0; i< subsec->numlines; i++)
	{
		seg_t * seg = &segs[subsec->firstline + i];
		subsector_t * backsub = seg->PartnerSeg->Subsector;

		// already checked?
		if (backsub->validcount == validcount) continue;	
		backsub->validcount=validcount;

		if (seg->frontsector != seg->backsector && seg->linedef)
		{
			// Note: if this is a real line between sectors
			// we can be sure that render_sector is the real sector!

			sector_t * sec = gl_FakeFlat(seg->backsector, &fakesec, true);

			// Don't bother with slopes
			if (sec->floorplane.a!=0 || sec->floorplane.b!=0)  return false;

			// Is the neighboring floor higher than the desired height?
			if (sec->GetPlaneTexZ(sector_t::floor)<planez) 
			{
				// todo: check for missing textures.
				return false;
			}

			// This is an exact height match which means we don't have to do any further checks for this sector
			// No texture checks though! 
			if (sec->GetPlaneTexZ(sector_t::floor)==planez) continue;
		}
		if (!DoFakeBridge(backsub, planez)) return false;
	}
	// all checked ok. This sector is part of the current fake plane

	HandledSubsectors.Push(subsec);
	return 1;
}

//==========================================================================
//
//
//
//==========================================================================
bool FDrawInfo::DoFakeCeilingBridge(subsector_t * subsec, fixed_t planez)
{
	// Is there a one-sided wall in this sector?
	// Do this first to avoid unnecessary recursion
	for(DWORD i=0; i< subsec->numlines; i++)
	{
		if (segs[subsec->firstline + i].backsector == NULL) return false;
		if (segs[subsec->firstline + i].PartnerSeg == NULL) return false;
	}

	for(DWORD i=0; i< subsec->numlines; i++)
	{
		seg_t * seg = &segs[subsec->firstline + i];
		subsector_t * backsub = seg->PartnerSeg->Subsector;

		// already checked?
		if (backsub->validcount == validcount) continue;	
		backsub->validcount=validcount;

		if (seg->frontsector != seg->backsector && seg->linedef)
		{
			// Note: if this is a real line between sectors
			// we can be sure that render_sector is the real sector!

			sector_t * sec = gl_FakeFlat(seg->backsector, &fakesec, true);

			// Don't bother with slopes
			if (sec->ceilingplane.a!=0 || sec->ceilingplane.b!=0)  return false;

			// Is the neighboring ceiling higher than the desired height?
			if (sec->GetPlaneTexZ(sector_t::ceiling)>planez) 
			{
				// todo: check for missing textures.
				return false;
			}

			// This is an exact height match which means we don't have to do any further checks for this sector
			// No texture checks though! 
			if (sec->GetPlaneTexZ(sector_t::ceiling)==planez) continue;
		}
		if (!DoFakeCeilingBridge(backsub, planez)) return false;
	}
	// all checked ok. This sector is part of the current fake plane

	HandledSubsectors.Push(subsec);
	return 1;
}


//==========================================================================
//
// Draws the fake planes
//
//==========================================================================
void FDrawInfo::HandleMissingTextures()
{
	sector_t fake;
	totalms.Clock();
	totalupper=MissingUpperTextures.Size();
	totallower=MissingLowerTextures.Size();

	for(unsigned int i=0;i<MissingUpperTextures.Size();i++)
	{
		if (!MissingUpperTextures[i].seg) continue;
		HandledSubsectors.Clear();
		validcount++;

		if (MissingUpperTextures[i].planez > viewz) 
		{
			// close the hole only if all neighboring sectors are an exact height match
			// Otherwise just fill in the missing textures.
			MissingUpperTextures[i].sub->validcount=validcount;
			if (DoOneSectorUpper(MissingUpperTextures[i].sub, MissingUpperTextures[i].planez))
			{
				sector_t * sec = MissingUpperTextures[i].seg->backsector;
				// The mere fact that this seg has been added to the list means that the back sector
				// will be rendered so we can safely assume that it is already in the render list

				for(unsigned int j=0;j<HandledSubsectors.Size();j++)
				{				
					gl_subsectorrendernode * node = SSR_List.GetNew();
					node->sub = HandledSubsectors[j];

					AddOtherCeilingPlane(sec->sectornum, node);
				}

				if (HandledSubsectors.Size()!=1)
				{
					// mark all subsectors in the missing list that got processed by this
					for(unsigned int j=0;j<HandledSubsectors.Size();j++)
					{
						for(unsigned int k=0;k<MissingUpperTextures.Size();k++)
						{
							if (MissingUpperTextures[k].sub==HandledSubsectors[j])
							{
								MissingUpperTextures[k].seg=NULL;
							}
						}
					}
				}
				else MissingUpperTextures[i].seg=NULL;
				continue;
			}
		}

		if (!MissingUpperTextures[i].seg->PartnerSeg) continue;
		if (!MissingUpperTextures[i].seg->PartnerSeg->Subsector) continue;
		validcount++;
		HandledSubsectors.Clear();

		{
			// It isn't a hole. Now check whether it might be a fake bridge
			sector_t * fakesector = gl_FakeFlat(MissingUpperTextures[i].seg->frontsector, &fake, false);
			fixed_t planez = fakesector->GetPlaneTexZ(sector_t::ceiling);

			MissingUpperTextures[i].seg->PartnerSeg->Subsector->validcount=validcount;
			if (DoFakeCeilingBridge(MissingUpperTextures[i].seg->PartnerSeg->Subsector, planez))
			{
				// The mere fact that this seg has been added to the list means that the back sector
				// will be rendered so we can safely assume that it is already in the render list

				for(unsigned int j=0;j<HandledSubsectors.Size();j++)
				{				
					gl_subsectorrendernode * node = SSR_List.GetNew();
					node->sub = HandledSubsectors[j];
					AddOtherCeilingPlane(fakesector->sectornum, node);
				}
			}
			continue;
		}
	}

	for(unsigned int i=0;i<MissingLowerTextures.Size();i++)
	{
		if (!MissingLowerTextures[i].seg) continue;
		HandledSubsectors.Clear();
		validcount++;

		if (MissingLowerTextures[i].planez < viewz) 
		{
			// close the hole only if all neighboring sectors are an exact height match
			// Otherwise just fill in the missing textures.
			MissingLowerTextures[i].sub->validcount=validcount;
			if (DoOneSectorLower(MissingLowerTextures[i].sub, MissingLowerTextures[i].planez))
			{
				sector_t * sec = MissingLowerTextures[i].seg->backsector;
				// The mere fact that this seg has been added to the list means that the back sector
				// will be rendered so we can safely assume that it is already in the render list

				for(unsigned int j=0;j<HandledSubsectors.Size();j++)
				{				
					gl_subsectorrendernode * node = SSR_List.GetNew();
					node->sub = HandledSubsectors[j];
					AddOtherFloorPlane(sec->sectornum, node);
				}

				if (HandledSubsectors.Size()!=1)
				{
					// mark all subsectors in the missing list that got processed by this
					for(unsigned int j=0;j<HandledSubsectors.Size();j++)
					{
						for(unsigned int k=0;k<MissingLowerTextures.Size();k++)
						{
							if (MissingLowerTextures[k].sub==HandledSubsectors[j])
							{
								MissingLowerTextures[k].seg=NULL;
							}
						}
					}
				}
				else MissingLowerTextures[i].seg=NULL;
				continue;
			}
		}

		if (!MissingLowerTextures[i].seg->PartnerSeg) continue;
		if (!MissingLowerTextures[i].seg->PartnerSeg->Subsector) continue;
		validcount++;
		HandledSubsectors.Clear();

		{
			// It isn't a hole. Now check whether it might be a fake bridge
			sector_t * fakesector = gl_FakeFlat(MissingLowerTextures[i].seg->frontsector, &fake, false);
			fixed_t planez = fakesector->GetPlaneTexZ(sector_t::floor);

			MissingLowerTextures[i].seg->PartnerSeg->Subsector->validcount=validcount;
			if (DoFakeBridge(MissingLowerTextures[i].seg->PartnerSeg->Subsector, planez))
			{
				// The mere fact that this seg has been added to the list means that the back sector
				// will be rendered so we can safely assume that it is already in the render list

				for(unsigned int j=0;j<HandledSubsectors.Size();j++)
				{				
					gl_subsectorrendernode * node = SSR_List.GetNew();
					node->sub = HandledSubsectors[j];
					AddOtherFloorPlane(fakesector->sectornum, node);
				}
			}
			continue;
		}
	}

	totalms.Unclock();
	showtotalms=totalms;
	totalms.Reset();
}


//==========================================================================
//
//
//
//==========================================================================

void FDrawInfo::DrawUnhandledMissingTextures()
{
	validcount++;
	for(int i=MissingUpperSegs.Size()-1; i>=0; i--)
	{
		int index = MissingUpperSegs[i].MTI_Index;
		if (index>=0 && MissingUpperTextures[index].seg==NULL) continue;

		seg_t * seg = MissingUpperSegs[i].seg;

		// already done!
		if (seg->linedef->validcount==validcount) continue;		// already done
		seg->linedef->validcount=validcount;
		if (seg->frontsector->GetPlaneTexZ(sector_t::ceiling) < viewz) continue;	// out of sight
		//if (seg->frontsector->ceilingpic==skyflatnum) continue;

		// FIXME: The check for degenerate subsectors should be more precise
		if (seg->PartnerSeg && seg->PartnerSeg->Subsector->degenerate) continue;
		if (seg->backsector->transdoor) continue;
		if (seg->backsector->GetTexture(sector_t::ceiling)==skyflatnum) continue;
		if (seg->backsector->CeilingSkyBox && seg->backsector->CeilingSkyBox->bAlways) continue;

		if (!gl_notexturefill) FloodUpperGap(seg);
	}

	validcount++;
	for(int i=MissingLowerSegs.Size()-1; i>=0; i--)
	{
		int index = MissingLowerSegs[i].MTI_Index;
		if (index>=0 && MissingLowerTextures[index].seg==NULL) continue;

		seg_t * seg = MissingLowerSegs[i].seg;

		// already done!
		if (seg->linedef->validcount==validcount) continue;		// already done
		seg->linedef->validcount=validcount;
		if (!(sectorrenderflags[seg->backsector->sectornum] & SSRF_RENDERFLOOR)) continue;
		if (seg->frontsector->GetPlaneTexZ(sector_t::floor) > viewz) continue;	// out of sight
		if (seg->backsector->transdoor) continue;
		if (seg->backsector->GetTexture(sector_t::floor)==skyflatnum) continue;
		if (seg->backsector->FloorSkyBox && seg->backsector->FloorSkyBox->bAlways) continue;

		if (!gl_notexturefill) FloodLowerGap(seg);
	}
	MissingUpperTextures.Clear();
	MissingLowerTextures.Clear();
	MissingUpperSegs.Clear();
	MissingLowerSegs.Clear();

}

ADD_STAT(missingtextures)
{
	FString out;
	out.Format("Missing textures: %d upper, %d lower, %.3f ms\n", 
		totalupper, totallower, showtotalms.TimeMS());
	return out;
}


//==========================================================================
//
// Multi-sector deep water hacks
//
//==========================================================================

void FDrawInfo::AddHackedSubsector(subsector_t * sub)
{
	if (!(level.flags & LEVEL_HEXENFORMAT))
	{
		SubsectorHackInfo sh={sub, 0};
		SubsectorHacks.Push (sh);
	}
}

//==========================================================================
//
// Finds a subsector whose plane can be used for rendering
//
//==========================================================================

bool FDrawInfo::CheckAnchorFloor(subsector_t * sub)
{
	// This subsector has a one sided wall and can be used.
	if (sub->hacked==3) return true;
	if (sub->degenerate) return false;

	for(DWORD j=0;j<sub->numlines;j++)
	{
		seg_t * seg = &segs[sub->firstline+j];
		if (!seg->PartnerSeg) return true;

		subsector_t * backsub = seg->PartnerSeg->Subsector;

		// Find a linedef with a different visplane on the other side.
		if (!backsub->degenerate && seg->linedef && 
			(sub->render_sector != backsub->render_sector && sub->sector != backsub->sector))
		{
			// I'm ignoring slopes, scaling and rotation here. The likelihood of ZDoom maps
			// using such crap hacks is simply too small
			if (sub->render_sector->GetTexture(sector_t::floor)==backsub->render_sector->GetTexture(sector_t::floor) &&
				sub->render_sector->GetPlaneTexZ(sector_t::floor)==backsub->render_sector->GetPlaneTexZ(sector_t::floor) &&
				GetFloorLight(sub->render_sector)==GetFloorLight(backsub->render_sector))
			{
				continue;
			}
			// This means we found an adjoining subsector that clearly would go into another
			// visplane. That means that this subsector can be used as an anchor.
			return true;
		}
	}
	return false;
}

//==========================================================================
//
// Collect connected subsectors that have to be rendered with the same plane
//
//==========================================================================
static bool inview;
static subsector_t * viewsubsector;
static TArray<seg_t *> lowersegs;

bool FDrawInfo::CollectSubsectorsFloor(subsector_t * sub, sector_t * anchor)
{

	// mark it checked
	sub->validcount=validcount;


	// We must collect any subsector that either is connected to this one with a miniseg
	// or has the same visplane.
	// We must not collect any subsector that  has the anchor's visplane!
	if (!sub->degenerate) 
	{
		// Is not being rendered so don't bother.
		if (!(ss_renderflags[DWORD(sub-subsectors)]&SSRF_PROCESSED)) return true;

		if (sub->render_sector->GetTexture(sector_t::floor) != anchor->GetTexture(sector_t::floor) ||
			sub->render_sector->GetPlaneTexZ(sector_t::floor)!=anchor->GetPlaneTexZ(sector_t::floor) ||
			GetFloorLight(sub->render_sector)!=GetFloorLight(anchor)) 
		{
			if (sub==viewsubsector && viewz<anchor->GetPlaneTexZ(sector_t::floor)) inview=true;
			HandledSubsectors.Push (sub);
		}
	}

	// We can assume that all segs in this subsector are connected to a subsector that has
	// to be checked as well
	for(DWORD j=0;j<sub->numlines;j++)
	{
		seg_t * seg = &segs[sub->firstline+j];
		if (seg->PartnerSeg)
		{
			subsector_t * backsub = seg->PartnerSeg->Subsector;

			// could be an anchor itself.
			if (!CheckAnchorFloor (backsub)) // must not be an anchor itself!
			{
				if (backsub->validcount!=validcount) 
				{
					if (!CollectSubsectorsFloor (backsub, anchor)) return false;
				}
			}
			else if (sub->render_sector == backsub->render_sector)
			{
				// Any anchor not within the original anchor's visplane terminates the processing.
				if (sub->render_sector->GetTexture(sector_t::floor)!=anchor->GetTexture(sector_t::floor) ||
					sub->render_sector->GetPlaneTexZ(sector_t::floor)!=anchor->GetPlaneTexZ(sector_t::floor) ||
					GetFloorLight(sub->render_sector)!=GetFloorLight(anchor)) 
				{
					return false;
				}
			}
			if (!seg->linedef || (seg->frontsector==seg->backsector && sub->render_sector!=backsub->render_sector))
				lowersegs.Push(seg);
		}
	}
	return true;
}

//==========================================================================
//
// Finds a subsector whose plane can be used for rendering
//
//==========================================================================

bool FDrawInfo::CheckAnchorCeiling(subsector_t * sub)
{
	// This subsector has a one sided wall and can be used.
	if (sub->hacked==3) return true;
	if (sub->degenerate) return false;

	for(DWORD j=0;j<sub->numlines;j++)
	{
		seg_t * seg = &segs[sub->firstline+j];
		if (!seg->PartnerSeg) return true;

		subsector_t * backsub = seg->PartnerSeg->Subsector;

		// Find a linedef with a different visplane on the other side.
		if (!backsub->degenerate && seg->linedef && 
			(sub->render_sector != backsub->render_sector && sub->sector != backsub->sector))
		{
			// I'm ignoring slopes, scaling and rotation here. The likelihood of ZDoom maps
			// using such crap hacks is simply too small
			if (sub->render_sector->GetTexture(sector_t::ceiling)==backsub->render_sector->GetTexture(sector_t::ceiling) &&
				sub->render_sector->GetPlaneTexZ(sector_t::ceiling)==backsub->render_sector->GetPlaneTexZ(sector_t::ceiling) &&
				GetCeilingLight(sub->render_sector)==GetCeilingLight(backsub->render_sector))
			{
				continue;
			}
			// This means we found an adjoining subsector that clearly would go into another
			// visplane. That means that this subsector can be used as an anchor.
			return true;
		}
	}
	return false;
}

//==========================================================================
//
// Collect connected subsectors that have to be rendered with the same plane
//
//==========================================================================

bool FDrawInfo::CollectSubsectorsCeiling(subsector_t * sub, sector_t * anchor)
{
	// mark it checked
	sub->validcount=validcount;


	// We must collect any subsector that either is connected to this one with a miniseg
	// or has the same visplane.
	// We must not collect any subsector that  has the anchor's visplane!
	if (!sub->degenerate) 
	{
		// Is not being rendererd so don't bother.
		if (!(ss_renderflags[DWORD(sub-subsectors)]&SSRF_PROCESSED)) return true;

		if (sub->render_sector->GetTexture(sector_t::ceiling)!=anchor->GetTexture(sector_t::ceiling) ||
			sub->render_sector->GetPlaneTexZ(sector_t::ceiling)!=anchor->GetPlaneTexZ(sector_t::ceiling) ||
			GetCeilingLight(sub->render_sector)!=GetCeilingLight(anchor)) 
		{
			HandledSubsectors.Push (sub);
		}
	}

	// We can assume that all segs in this subsector are connected to a subsector that has
	// to be checked as well
	for(DWORD j=0;j<sub->numlines;j++)
	{
		seg_t * seg = &segs[sub->firstline+j];
		if (seg->PartnerSeg)
		{
			subsector_t * backsub = seg->PartnerSeg->Subsector;

			// could be an anchor itself.
			if (!CheckAnchorCeiling (backsub)) // must not be an anchor itself!
			{
				if (backsub->validcount!=validcount) 
				{
					if (!CollectSubsectorsCeiling (backsub, anchor)) return false;
				}
			}
			else if (sub->render_sector == backsub->render_sector)
			{
				// Any anchor not within the original anchor's visplane terminates the processing.
				if (sub->render_sector->GetTexture(sector_t::ceiling)!=anchor->GetTexture(sector_t::ceiling) ||
					sub->render_sector->GetPlaneTexZ(sector_t::ceiling)!=anchor->GetPlaneTexZ(sector_t::ceiling) ||
					GetCeilingLight(sub->render_sector)!=GetCeilingLight(anchor)) 
				{
					return false;
				}
			}
		}
	}
	return true;
}

//==========================================================================
//
// Process the subsectors that have been marked as hacked
//
//==========================================================================

void FDrawInfo::HandleHackedSubsectors()
{
	lowershcount=uppershcount=0;
	totalssms.Reset();
	totalssms.Clock();

	viewsubsector = R_PointInSubsector(viewx, viewy);

	// Each subsector may only be processed once in this loop!
	validcount++;
	for(unsigned int i=0;i<SubsectorHacks.Size();i++)
	{
		subsector_t * sub = SubsectorHacks[i].sub;
		if (sub->validcount!=validcount && CheckAnchorFloor(sub))
		{
			// Now collect everything that is connected with this subsector.
			HandledSubsectors.Clear();
			inview=false;
			lowersegs.Clear();
			if (CollectSubsectorsFloor(sub, sub->render_sector))
			{
				for(unsigned int j=0;j<HandledSubsectors.Size();j++)
				{				
					gl_subsectorrendernode * node = SSR_List.GetNew();

					node->sub = HandledSubsectors[j];
					AddOtherFloorPlane(sub->render_sector->sectornum, node);
				}
				if (inview) for(unsigned int j=0;j<lowersegs.Size();j++)
				{
					seg_t * seg=lowersegs[j];
					GLRenderer->ProcessLowerMiniseg (seg, seg->Subsector->render_sector, seg->PartnerSeg->Subsector->render_sector);
				}
				lowershcount+=HandledSubsectors.Size();
			}
		}
	}

	// Each subsector may only be processed once in this loop!
	validcount++;
	for(unsigned int i=0;i<SubsectorHacks.Size();i++)
	{
		subsector_t * sub = SubsectorHacks[i].sub;
		if (sub->validcount!=validcount && CheckAnchorCeiling(sub))
		{
			// Now collect everything that is connected with this subsector.
			HandledSubsectors.Clear();
			if (CollectSubsectorsCeiling(sub, sub->render_sector))
			{
				for(unsigned int j=0;j<HandledSubsectors.Size();j++)
				{				
					gl_subsectorrendernode * node = SSR_List.GetNew();

					node->sub = HandledSubsectors[j];
					AddOtherCeilingPlane(sub->render_sector->sectornum, node);
				}
				uppershcount+=HandledSubsectors.Size();
			}
		}
	}


	SubsectorHacks.Clear();
	totalssms.Unclock();
}

ADD_STAT(sectorhacks)
{
	FString out;
	out.Format("sectorhacks = %.3f ms, %d upper, %d lower\n", totalssms.TimeMS(), uppershcount, lowershcount);
	return out;
}


//==========================================================================
//
// This merges visplanes that lie inside a sector stack together
// to avoid rendering these unneeded flats
//
//==========================================================================

void FDrawInfo::AddFloorStack(subsector_t * sub)
{
	FloorStacks.Push(sub);
}

void FDrawInfo::AddCeilingStack(subsector_t * sub)
{
	CeilingStacks.Push(sub);
}

//==========================================================================
//
//
//
//==========================================================================

void FDrawInfo::CollectSectorStacksCeiling(subsector_t * sub, sector_t * anchor)
{
	// mark it checked
	sub->validcount=validcount;

	// Has a sector stack or skybox itself!
	if (sub->render_sector->CeilingSkyBox && sub->render_sector->CeilingSkyBox->bAlways) return;

	// Don't bother processing unrendered subsectors
	if (sub->numlines>2 && !(ss_renderflags[DWORD(sub-subsectors)]&SSRF_PROCESSED)) return;

	// Must be the exact same visplane
	sector_t * me = sub->render_sector;
	if (me->GetTexture(sector_t::ceiling) != anchor->GetTexture(sector_t::ceiling) ||
		me->ceilingplane != anchor->ceilingplane ||
		GetCeilingLight(me) != GetCeilingLight(anchor) ||
		me->ColorMap != anchor->ColorMap ||
		me->GetXOffset(sector_t::ceiling) != anchor->GetXOffset(sector_t::ceiling) || 
		me->GetYOffset(sector_t::ceiling) != anchor->GetYOffset(sector_t::ceiling) || 
		me->GetXScale(sector_t::ceiling) != anchor->GetXScale(sector_t::ceiling) || 
		me->GetYScale(sector_t::ceiling) != anchor->GetYScale(sector_t::ceiling) || 
		me->GetAngle(sector_t::ceiling) != anchor->GetAngle(sector_t::ceiling))
	{
		// different visplane so it can't belong to this stack
		return;
	}

	HandledSubsectors.Push (sub);

	for(DWORD j=0;j<sub->numlines;j++)
	{
		seg_t * seg = &segs[sub->firstline+j];
		if (seg->PartnerSeg)
		{
			subsector_t * backsub = seg->PartnerSeg->Subsector;

			if (backsub->validcount!=validcount) CollectSectorStacksCeiling (backsub, anchor);
		}
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FDrawInfo::CollectSectorStacksFloor(subsector_t * sub, sector_t * anchor)
{
	// mark it checked
	sub->validcount=validcount;

	// Has a sector stack or skybox itself!
	if (sub->render_sector->FloorSkyBox && sub->render_sector->FloorSkyBox->bAlways) return;

	// Don't bother processing unrendered subsectors
	if (sub->numlines>2 && !(ss_renderflags[DWORD(sub-subsectors)]&SSRF_PROCESSED)) return;

	// Must be the exact same visplane
	sector_t * me = sub->render_sector;
	if (me->GetTexture(sector_t::floor) != anchor->GetTexture(sector_t::floor) ||
		me->floorplane != anchor->floorplane ||
		GetFloorLight(me) != GetFloorLight(anchor) ||
		me->ColorMap != anchor->ColorMap ||
		me->GetXOffset(sector_t::floor) != anchor->GetXOffset(sector_t::floor) || 
		me->GetYOffset(sector_t::floor) != anchor->GetYOffset(sector_t::floor) || 
		me->GetXScale(sector_t::floor) != anchor->GetXScale(sector_t::floor) || 
		me->GetYScale(sector_t::floor) != anchor->GetYScale(sector_t::floor) || 
		me->GetAngle(sector_t::floor) != anchor->GetAngle(sector_t::floor))
	{
		// different visplane so it can't belong to this stack
		return;
	}

	HandledSubsectors.Push (sub);

	for(DWORD j=0;j<sub->numlines;j++)
	{
		seg_t * seg = &segs[sub->firstline+j];
		if (seg->PartnerSeg)
		{
			subsector_t * backsub = seg->PartnerSeg->Subsector;

			if (backsub->validcount!=validcount) CollectSectorStacksFloor (backsub, anchor);
		}
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FDrawInfo::ProcessSectorStacks()
{
	unsigned int i;

	validcount++;
	for (i=0;i<CeilingStacks.Size (); i++)
	{
		subsector_t * sub = CeilingStacks[i];

		HandledSubsectors.Clear();
		for(DWORD j=0;j<sub->numlines;j++)
		{
			seg_t * seg = &segs[sub->firstline+j];
			if (seg->PartnerSeg)
			{
				subsector_t * backsub = seg->PartnerSeg->Subsector;

				if (backsub->validcount!=validcount) CollectSectorStacksCeiling (backsub, sub->render_sector);
			}
		}

		for(unsigned int j=0;j<HandledSubsectors.Size();j++)
		{				
			ss_renderflags[DWORD(HandledSubsectors[j]-subsectors)] &= ~SSRF_RENDERCEILING;

			if (sub->render_sector->CeilingSkyBox->PlaneAlpha!=0)
			{
				gl_subsectorrendernode * node = SSR_List.GetNew();
				node->sub = HandledSubsectors[j];
				AddOtherCeilingPlane(sub->render_sector->sectornum, node);
			}
		}
	}

	validcount++;
	for (i=0;i<FloorStacks.Size (); i++)
	{
		subsector_t * sub = FloorStacks[i];

		HandledSubsectors.Clear();
		for(DWORD j=0;j<sub->numlines;j++)
		{
			seg_t * seg = &segs[sub->firstline+j];
			if (seg->PartnerSeg)
			{
				subsector_t	* backsub = seg->PartnerSeg->Subsector;

				if (backsub->validcount!=validcount) CollectSectorStacksFloor (backsub, sub->render_sector);
			}
		}

		for(unsigned int j=0;j<HandledSubsectors.Size();j++)
		{				
			//Printf("%d: ss %d, sec %d\n", j, HandledSubsectors[j]-subsectors, HandledSubsectors[j]->render_sector->sectornum);
			ss_renderflags[DWORD(HandledSubsectors[j]-subsectors)] &= ~SSRF_RENDERFLOOR;

			if (sub->render_sector->FloorSkyBox->PlaneAlpha!=0)
			{
				gl_subsectorrendernode * node = SSR_List.GetNew();
				node->sub = HandledSubsectors[j];
				AddOtherFloorPlane(sub->render_sector->sectornum, node);
			}
		}
	}

	FloorStacks.Clear();
	CeilingStacks.Clear();
}

