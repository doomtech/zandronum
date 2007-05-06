/*
** p_3dfloor.cpp
**
** 3D-floor handling (and a little other stuff as well. ;))
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

#include "templates.h"
#include "p_local.h"
#include "p_lnspec.h"
#include "w_wad.h"
#include "sc_man.h"
#include "gl/gl_lights.h"

#define ML_3DMIDTEX		0x4000

enum
{
	// crappy hack to make certain WADs look better
	sec_outside = 87,
};




//==========================================================================
//
//  3D Floors
//
//==========================================================================

//==========================================================================
//
// Add one 3D floor to the sector
//
//==========================================================================
static void P_Add3DFloor(sector_t* sec, sector_t* sec2, line_t* master, int flags,int transluc)
{
	F3DFloor*      ffloor;
	unsigned  i;
		
	for(i = 0; i < sec2->e->attached.Size(); i++) if(sec2->e->attached[i] == sec) return;
	sec2->e->attached.Push(sec);
	
	//Add the floor
	ffloor = new F3DFloor;
	ffloor->top.model = ffloor->bottom.model = ffloor->model = sec2;
	ffloor->target = sec;
	
	if (!(flags&FF_THINFLOOR)) 
	{
		ffloor->bottom.plane = &sec2->floorplane;
		ffloor->bottom.texture = &sec2->floorpic;
		ffloor->bottom.texheight = &sec2->floortexz;
		ffloor->bottom.isceiling = false;
	}
	else 
	{
		ffloor->bottom.plane = &sec2->ceilingplane;
		ffloor->bottom.texture = &sec2->ceilingpic;
		ffloor->bottom.texheight = &sec2->ceilingtexz;
		ffloor->bottom.isceiling = true;
	}
	
	if (!(flags&FF_FIX))
	{
		ffloor->top.plane = &sec2->ceilingplane;
		ffloor->top.texture = &sec2->ceilingpic;
		ffloor->top.texheight = &sec2->ceilingtexz;
		ffloor->toplightlevel = &sec2->lightlevel;
		ffloor->top.isceiling = true;
	}
	else	// FF_FIX is a special case to patch rendering holes
	{
		ffloor->top.plane = &sec->floorplane;
		ffloor->top.texture = &sec2->floorpic;
		ffloor->top.texheight = &sec2->floortexz;
		ffloor->toplightlevel = &sec->lightlevel;
		ffloor->top.isceiling = false;
		ffloor->top.model = sec;
	}

	// Hacks for Vavoom's idiotic implementation
	if (flags&FF_INVERTSECTOR)
	{
		// switch the planes
		F3DFloor::planeref sp = ffloor->top;

		ffloor->top=ffloor->bottom;
		ffloor->bottom=sp;

		if (flags&FF_SWIMMABLE)
		{
			// Vavoom floods the lower part if it is swimmable.
			// fortunately this plane won't be rendered - otherwise this wouldn't work...
			ffloor->bottom.plane=&sec->floorplane;
			ffloor->bottom.model=sec;
		}
	}

	ffloor->flags  = flags;
	ffloor->master = master;
	ffloor->alpha  = transluc;

	// The engine cannot handle sloped translucent floors. Sorry
	if (ffloor->top.plane->a || ffloor->top.plane->b || ffloor->bottom.plane->a || ffloor->bottom.plane->b)
	{
		ffloor->alpha = FRACUNIT;
	}

	sec->e->ffloors.Push(ffloor);
}

//==========================================================================
//
// Creates all 3D floors defined by one linedef
//
//==========================================================================
static int P_Set3DFloor(line_t * line, int param,int param2, int alpha)
{
	int s,i;
	int flags;
	int tag=line->args[0];
    sector_t * sec = line->frontsector, * ss;

    for (s=-1; (s = P_FindSectorFromTag(tag,s)) >= 0;)
	{
		ss=&sectors[s];

		if (param==0)
		{
			flags=FF_EXISTS|FF_RENDERALL|FF_SOLID|FF_INVERTSECTOR;
			for (i=0;i<sec->linecount;i++)
			{
				line_t * l=sec->lines[i];

				alpha=255;
				if (l->special==Sector_SetContents && l->frontsector==sec)
				{
					alpha=clamp<int>(l->args[1], 0, 100);
					if (l->args[2]&1) flags&=~FF_SOLID;
					if (alpha!=100) flags|=FF_TRANSLUCENT;//|FF_BOTHPLANES|FF_ALLSIDES;
					if (l->args[0]) 
					{
						// Yes, Vavoom's 3D-floor definitions suck!
						static DWORD vavoomcolors[]={
							0, 0x101080, 0x801010, 0x108010, 0x287020, 0xf0f010};
						flags|=FF_SWIMMABLE|FF_BOTHPLANES|FF_ALLSIDES;

						l->frontsector->ColorMap = GetSpecialLights (l->frontsector->ColorMap->Color, 
																	 vavoomcolors[l->args[0]], 
																	 l->frontsector->ColorMap->Desaturate);
					}
					alpha=(alpha*255)/100;
					break;
				}
			}
		}
		else if (param==4)
		{
			flags=FF_EXISTS|FF_RENDERPLANES|FF_INVERTPLANES|FF_NOSHADE|FF_FIX|FF_NOSHADE;
			alpha=255;
		}
		else 
		{
			static const int defflags[]= {0, 
										  FF_SOLID, 
										  FF_SWIMMABLE|FF_BOTHPLANES|FF_ALLSIDES, 
										  0, 0, 
										  FF_SOLID|FF_BOTHPLANES|FF_ALLSIDES, 
										  FF_SWIMMABLE|FF_BOTHPLANES|FF_ALLSIDES, 
										  FF_BOTHPLANES|FF_ALLSIDES};

			flags = defflags[param&7] | FF_EXISTS|FF_RENDERALL;

			if (param2&1) flags|=FF_NOSHADE;
			if (param2&2) flags|=FF_DOUBLESHADOW;
			if (param2&4) flags|=FF_FOG;
			if (param2&8) flags|=FF_THINFLOOR;
			if (param2&16) flags|=FF_UPPERTEXTURE;
			if (param2&32) flags|=FF_LOWERTEXTURE;
			if (param2&64) flags|=FF_ADDITIVETRANS|FF_TRANSLUCENT;
			if (param2&512) flags|=FF_FADEWALLS;
			if (sides[line->sidenum[0]].toptexture<0 && alpha<255)
			{
				alpha=clamp(-sides[line->sidenum[0]].toptexture, 0, 255);
			}
			if (alpha==0) flags&=~(FF_RENDERALL|FF_BOTHPLANES|FF_ALLSIDES);
			else if (alpha!=255) flags|=FF_TRANSLUCENT;
										 
		}
		P_Add3DFloor(ss, sec, line, flags, alpha);
	}
	// To be 100% safe this should be done even if the alpha by texture value isn't used.
	if (sides[line->sidenum[0]].toptexture<0) sides[line->sidenum[0]].toptexture=0;
	return 1;
}

//==========================================================================
//
// P_PlayerOnSpecial3DFloor
// Checks to see if a player is standing on or is inside a 3D floor (water)
// and applies any specials..
//
//==========================================================================

void P_PlayerOnSpecial3DFloor(player_t* player)
{
	sector_t * sector = player->mo->Sector;

	for(unsigned i=0;i<sector->e->ffloors.Size();i++)
	{
		F3DFloor* rover=sector->e->ffloors[i];

		if (!(rover->flags & FF_EXISTS)) continue;
		if (rover->flags & FF_FIX) continue;

		// Check the 3D floor's type...
		if(rover->flags & FF_SOLID)
		{
			// Player must be on top of the floor to be affected...
			if(player->mo->z != rover->top.plane->ZatPoint(player->mo->x, player->mo->y)) continue;
		}
		else
		{
			//Water and DEATH FOG!!! heh
			if (player->mo->z > rover->top.plane->ZatPoint(player->mo->x, player->mo->y) || 
				(player->mo->z + player->mo->height) < rover->bottom.plane->ZatPoint(player->mo->x, player->mo->y))
				continue;
		}
		
		if (rover->model->special || rover->model->damage) P_PlayerInSpecialSector(player, rover->model);
		break;
	}
}

//==========================================================================
//
// P_CheckFor3DFloorHit
// Checks whether the player's feet touch a solid 3D floor in the sector
//
//==========================================================================
bool P_CheckFor3DFloorHit(AActor * mo)
{
	sector_t * sector = mo->Sector;

	//if ((mo->player && (mo->player->cheats & CF_PREDICTING))) return false;

	for(unsigned i=0;i<sector->e->ffloors.Size();i++)
	{
		F3DFloor* rover=sector->e->ffloors[i];

		if (!(rover->flags & FF_EXISTS)) continue;

		if(rover->flags & FF_SOLID && rover->model->SecActTarget)
		{
			if(mo->z == rover->top.plane->ZatPoint(mo->x, mo->y)) 
			{
				rover->model->SecActTarget->TriggerAction (mo, SECSPAC_HitFloor);
				return true;
			}
		}
	}
	return false;
}

//==========================================================================
//
// P_CheckFor3DCeilingHit
// Checks whether the player's head touches a solid 3D floor in the sector
//
//==========================================================================
bool P_CheckFor3DCeilingHit(AActor * mo)
{
	sector_t * sector = mo->Sector;

	//if ((mo->player && (mo->player->cheats & CF_PREDICTING))) return false;

	for(unsigned i=0;i<sector->e->ffloors.Size();i++)
	{
		F3DFloor* rover=sector->e->ffloors[i];

		if (!(rover->flags & FF_EXISTS)) continue;

		if(rover->flags & FF_SOLID && rover->model->SecActTarget)
		{
			if(mo->z+mo->height == rover->bottom.plane->ZatPoint(mo->x, mo->y)) 
			{
				rover->model->SecActTarget->TriggerAction (mo, SECSPAC_HitCeiling);
				return true;
			}
		}
	}
	return false;
}

//==========================================================================
//
// P_Recalculate3DFloors
//
// This function sorts the ffloors by height and creates the lightlists 
// that the given sector uses to light floors/ceilings/walls according to the 3D floors.
//
//==========================================================================
#define CenterSpot(sec) (vertex_t*)&(sec)->soundorg[0]

void P_Recalculate3DFloors(sector_t * sector)
{
	F3DFloor *		rover;
	F3DFloor *		pick;
	unsigned		pickindex;
	F3DFloor *		clipped=NULL;
	fixed_t			clipped_top;
	fixed_t			clipped_bottom;
	fixed_t			maxheight, minheight;
	unsigned		i, j;
	lightlist_t newlight;

	TArray<F3DFloor*> & ffloors=sector->e->ffloors;
	TArray<lightlist_t> & lightlist = sector->e->lightlist;

	// Sort the floors top to bottom for quicker access here and later
	// Translucent and swimmable floors are split if they overlap with solid ones.
	if (ffloors.Size()>1)
	{
		TArray<F3DFloor*> oldlist;
		
		oldlist = ffloors;
		ffloors.Clear();

		// first delete the old dynamic stuff
		for(i=0;i<oldlist.Size();i++)
		{
			F3DFloor * rover=oldlist[i];

			if (rover->flags&FF_DYNAMIC)
			{
				delete rover;
				oldlist.Delete(i);
				i--;
				continue;
			}
			if (rover->flags&FF_CLIPPED)
			{
				rover->flags&=~FF_CLIPPED;
				rover->flags|=FF_EXISTS;
			}
		}

		while (oldlist.Size())
		{
			pick=oldlist[0];
			fixed_t height=pick->top.plane->ZatPoint(CenterSpot(sector));

			// find highest starting ffloor - intersections are not supported!
			pickindex=0;
			for (j=1;j<oldlist.Size();j++)
			{
				fixed_t h2=oldlist[j]->top.plane->ZatPoint(CenterSpot(sector));

				if (h2>height)
				{
					pick=oldlist[j];
					pickindex=j;
					height=h2;
				}
			}

			oldlist.Delete(pickindex);

			if (pick->flags&(FF_SWIMMABLE|FF_TRANSLUCENT) && pick->flags&FF_EXISTS)
			{
				clipped=pick;
				clipped_top=height;
				clipped_bottom=pick->bottom.plane->ZatPoint(CenterSpot(sector));
				ffloors.Push(pick);
			}
			else if (clipped && clipped_bottom<height)
			{
				// translucent floor above must be clipped to this one!
				F3DFloor * dyn=new F3DFloor;
				*dyn=*clipped;
				clipped->flags|=FF_CLIPPED;
				clipped->flags&=~FF_EXISTS;
				dyn->flags|=FF_DYNAMIC;
				dyn->bottom=pick->top;
				ffloors.Push(dyn);
				ffloors.Push(pick);

				fixed_t pick_bottom=pick->bottom.plane->ZatPoint(CenterSpot(sector));

				if (pick_bottom<=clipped_bottom)
				{
					clipped=NULL;
				}
				else
				{
					// the translucent part extends below the clipper
					dyn=new F3DFloor;
					*dyn=*clipped;
					dyn->flags|=FF_DYNAMIC|FF_EXISTS;
					dyn->top=pick->bottom;
					ffloors.Push(dyn);
				}
			}
			else
			{
				clipped=NULL;
				ffloors.Push(pick);
			}

		}
	}

	// having the floors sorted makes this routine significantly simpler
	// Only some overlapping cases with FF_DOUBLESHADOW might create anomalies
	// but these are clearly undefined.
	if(ffloors.Size())
	{
		lightlist.Resize(1);
		lightlist[0].plane = sector->ceilingplane;
		lightlist[0].p_lightlevel = &sector->lightlevel;
		lightlist[0].caster = NULL;
		lightlist[0].p_extra_colormap = &sector->ColorMap;
		lightlist[0].flags = 0;
		
		maxheight = sector->CenterCeiling();
		minheight = sector->CenterFloor();
		for(i = 0; i < ffloors.Size(); i++)
		{
			rover=ffloors[i];

			if ( !(rover->flags & FF_EXISTS) || rover->flags & FF_NOSHADE )
				continue;
				
			fixed_t ff_top=rover->top.plane->ZatPoint(CenterSpot(sector));
			if (ff_top < minheight) break;	// reached the floor
			if (ff_top < maxheight)
			{
				newlight.plane = *rover->top.plane;
				newlight.p_lightlevel = rover->toplightlevel;
				newlight.caster = rover;
				newlight.p_extra_colormap=&rover->model->ColorMap;
				newlight.flags = rover->flags;
				lightlist.Push(newlight);
			}
			else if (i==0)
			{
				fixed_t ff_bottom=rover->bottom.plane->ZatPoint(CenterSpot(sector));
				if (ff_bottom<maxheight)
				{
					// this segment begins over the ceiling and extends beyond it
					lightlist[0].p_lightlevel = rover->toplightlevel;
					lightlist[0].caster = rover;
					lightlist[0].p_extra_colormap=&rover->model->ColorMap;
					lightlist[0].flags = rover->flags;
				}
			}
			if (rover->flags&FF_DOUBLESHADOW)
			{
				fixed_t ff_bottom=rover->bottom.plane->ZatPoint(CenterSpot(sector));
				if(ff_bottom < maxheight && ff_bottom>minheight)
				{
					newlight.caster = rover;
					newlight.plane = *rover->bottom.plane;
					if (lightlist.Size()>1)
					{
						newlight.p_lightlevel = lightlist[lightlist.Size()-2].p_lightlevel;
						newlight.p_extra_colormap = lightlist[lightlist.Size()-2].p_extra_colormap;
					}
					else
					{
						newlight.p_lightlevel = &sector->lightlevel;
						newlight.p_extra_colormap = &sector->ColorMap;
					}
					newlight.flags = rover->flags;
					lightlist.Push(newlight);
				}
			}
		}
	}
}


void P_RecalculateAttached3DFloors(sector_t * sec)
{
	for(unsigned int i=0; i<sec->e->attached.Size(); i++)
	{
		P_Recalculate3DFloors(sec->e->attached[i]);
	}
	P_Recalculate3DFloors(sec);
}

//==========================================================================
//
//
//
//==========================================================================
lightlist_t * P_GetPlaneLight(sector_t * sector, secplane_t * plane, bool underside)
{
	unsigned   i;

	fixed_t planeheight=plane->ZatPoint(CenterSpot(sector));
	if(underside) planeheight--;
	
	for(i = 1; i < sector->e->lightlist.Size(); i++)
		if (sector->e->lightlist[i].plane.ZatPoint(CenterSpot(sector)) <= planeheight) 
			return &sector->e->lightlist[i - 1];
		
	return &sector->e->lightlist[sector->e->lightlist.Size() - 1];
}



//============================================================================
//
// GetMidTexturePosition
//
//============================================================================
bool P_GetMidTexturePosition(const line_t * line, int sideno, fixed_t * ptextop, fixed_t * ptexbot)
{
	side_t *side = &sides[line->sidenum[sideno]];

	if (line->sidenum[0]==NO_SIDE || line->sidenum[1]==NO_SIDE || !side->midtexture) return false;
	
	FTexture * tex= TexMan(side->midtexture);

	fixed_t rowoffset=side->rowoffset;
	fixed_t textureheight=tex->GetHeight()<<FRACBITS;
	if (tex->ScaleY != 8 && tex->ScaleY != 0 && !tex->bWorldPanning)
	{
		rowoffset = DivScale3(rowoffset, tex->ScaleY);
		textureheight = DivScale3(textureheight, tex->ScaleY);
	}


	if(line->flags & ML_DONTPEGBOTTOM)
	{
		*ptexbot = rowoffset +
				   MAX<fixed_t>(line->frontsector->floortexz, line->backsector->floortexz);

		*ptextop = *ptexbot + textureheight;
	}
	else
	{
		*ptextop = rowoffset +
				   MIN<fixed_t>(line->frontsector->ceilingtexz, line->backsector->ceilingtexz);
		
		*ptexbot = *ptextop - textureheight;
	}
	return true;
}



//==========================================================================
//
// Extended P_LineOpening
//
//==========================================================================
extern int tmfloorpic;

void P_LineOpening (AActor * thing,const line_t *linedef, fixed_t x, fixed_t y, fixed_t refx, fixed_t refy)
{

	P_LineOpening(linedef, x, y, refx, refy);
	if (openrange<=0) return;

    if(thing)
    {
		sector_t *front, *back;

		fixed_t thingbot, thingtop;
		fixed_t tt, tb;
		
		thingbot = thing->z;
		thingtop = thingbot + thing->height;

		front = linedef->frontsector;
		back = linedef->backsector;

		// Check for 3D-floors in the sector (mostly identical to what Legacy does here)
		if(front->e->ffloors.Size() || back->e->ffloors.Size())
		{
			F3DFloor*      rover;
			unsigned i;

			fixed_t    lowestceiling = opentop;
			fixed_t    highestfloor = openbottom;
			fixed_t    lowestfloor = lowfloor;
			fixed_t    delta1;
			fixed_t    delta2;
			int highestfloorpic = tmfloorpic;
			
			thingtop = thing->z + thing->height;
			
			// Check for frontsector's 3D-floors
			for(i=0;i<front->e->ffloors.Size();i++)
			{
				rover=front->e->ffloors[i];

				if (!(rover->flags&FF_EXISTS)) continue;
				if (!(rover->flags & FF_SOLID)) continue;

				fixed_t ff_bottom=rover->bottom.plane->ZatPoint(x, y);
				fixed_t ff_top=rover->top.plane->ZatPoint(x, y);
				
				delta1 = abs(thing->z - ((ff_bottom + ff_top) / 2));
				delta2 = abs(thingtop - ((ff_bottom + ff_top) / 2));
				if(ff_bottom < lowestceiling && delta1 >= delta2) lowestceiling = ff_bottom;
				
				if(ff_top > highestfloor && delta1 < delta2) highestfloor = ff_top;
				else if(ff_top > lowestfloor && delta1 < delta2) lowestfloor = ff_top;
			}
			
			// Check for backsector's 3D-floors
			for(i=0;i<back->e->ffloors.Size();i++)
			{
				rover=back->e->ffloors[i];

				if (!(rover->flags&FF_EXISTS)) continue;
				if (!(rover->flags & FF_SOLID)) continue;
				
				fixed_t ff_bottom=rover->bottom.plane->ZatPoint(x, y);
				fixed_t ff_top=rover->top.plane->ZatPoint(x, y);
				
				delta1 = abs(thing->z - ((ff_bottom + ff_top) / 2));
				delta2 = abs(thingtop - ((ff_bottom + ff_top) / 2));
				if(ff_bottom < lowestceiling && delta1 >= delta2) lowestceiling = ff_bottom;
				
				if(ff_top > highestfloor && delta1 < delta2)
				{
					highestfloor = ff_top;
					highestfloorpic = *rover->top.texture;
				}
				else if(ff_top > lowestfloor && delta1 < delta2) lowestfloor = ff_top;
			}
			
			if(highestfloor > openbottom)
			{
				openbottom = highestfloor;
				tmfloorpic = highestfloorpic;
			}
			
			if(lowestceiling < opentop) opentop = lowestceiling;
			
			if(lowestfloor > lowfloor) lowfloor = lowestfloor;
		}

		// [BB] This makes the translucent lines in D2INV1 block missiles, which they should not.
		// I have no idea why ML_3DMIDTEX is set for them. Should be investigated.
/*
		// some rudimentary 3DMidtex handling
		if((linedef->flags & ML_3DMIDTEX) && 
			P_GetMidTexturePosition(linedef, 0, &tt, &tb))
		{
			if (thing->z + (thing->height/2) < (tt + tb)/2)
			{
				if(tb < opentop) opentop = tb;
			}
			else
			{
				if(tt > openbottom) openbottom = tt;
			}
		}
*/
    }

	openrange = opentop - openbottom;
}


//==========================================================================
//
// Extended sector data
//
//==========================================================================

static extsector_t * extsectordata;


//==========================================================================
//
// Deletes the extended sector data
//
//==========================================================================

void P_CleanExtSectors()
{
	int i;
	
	if (extsectordata)
	{
		for (i=0; i<numsectors; i++)
		{
			for(unsigned j=0;j<extsectordata[i].ffloors.Size();j++) delete extsectordata[i].ffloors[j];

			// destroy embedded objects (TArrays)
			extsectordata[i].extsector_t::~extsector_t();
		}
		free(extsectordata);
		extsectordata = NULL;
	}
}


//==========================================================================
//
// Creates the extended sector data
//
//==========================================================================

void P_CreateExtSectors()
{
	int i;
	sector_t * sector;
	FDynamicColormap	*fogMap=NULL;

	// Extended properties - DON'T USE new HERE!
	extsectordata= (extsector_t *)calloc (numsectors,sizeof(extsector_t));

	for (i=0,sector=sectors;i<numsectors;i++,sector++)
	{
		sector->e=extsectordata+i;
		sector->sectornum = i;
		sector->floor_reflect = sector->ceiling_reflect = 0;

		// apply outside fog to all sectors specially marked
		// (mostly added to make Herian 2's maps 07 and 19 look better)
		if ((sector->special&0xff)==sec_outside)
		{
			if (level.outsidefog != 0xff000000)
			{
				if (fogMap == NULL)
					fogMap = GetSpecialLights (PalEntry (255,255,255), level.outsidefog, 0);
				sector->ColorMap = fogMap;
			}
		}
	}
}
//==========================================================================
//
// Spawns non-ZDoom specials
//
//==========================================================================
void P_SpawnSpecials2 (void)
{
	int i;
	line_t * line;

	for (i=0,line=lines;i<numlines;i++,line++)
	{
		switch(line->special)
		{
		case ExtraFloor_LightOnly:
			// Note: I am spawning both this and ZDoom's ExtraLight data
			// I don't want to mess with both at the same time during rendering
			// so inserting this into the 3D-floor table as well seemed to be
			// the best option.
			//
			// This does not yet handle case 0 properly!
			P_Set3DFloor(line, 3, line->args[1]&1? 514:512, 0);
			break;

		case Sector_Set3DFloor:
			if (line->args[1]&8)
			{
				line->id = line->args[4];
			}
			else
			{
				line->args[0]+=256*line->args[4];
				line->args[4]=0;
			}
			P_Set3DFloor(line, line->args[1]&~8, line->args[2], line->args[3]);
			break;

		default:
			continue;
		}
		line->special=0;
		line->args[0] = line->args[1] = line->args[2] = line->args[3] = line->args[4]==0;
	}

	//
	// Also do some tweaks to the spawned actors
	//
	TThinkerIterator<AActor> it;
	AActor * mo;

	while (mo=it.Next())
	{
		// For GL the default 25% is extremely low so I increase it.
		// (But this doesn't handle a renderer switch. Whatever.)
		if (mo->SpawnFlags & MTF_SHADOW)
		{
			if (currentrenderer==1) mo->alpha = TRANSLUC50;
		}
		
		// Don't count monsters in end-of-level sectors
		// In 99.9% of all occurences they are part of a trap
		// and not supposed to be killed.
		if (mo->flags & MF_COUNTKILL)
		{
			if (mo->Sector->special == dDamage_End)
			{
				level.total_monsters--;
				mo->flags&=~(MF_COUNTKILL);
			}
		}
	}
}



