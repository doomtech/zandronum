/*
** gl_data.cpp
** Maintenance data for GL renderer (mostly to handle rendering hacks)
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

#include "gl/system/gl_system.h"

#include "doomtype.h"
#include "colormatcher.h"
#include "r_translate.h"
#include "i_system.h"
#include "p_local.h"
#include "p_lnspec.h"
#include "c_dispatch.h"
#include "r_sky.h"
#include "sc_man.h"
#include "w_wad.h"
#include "gi.h"
#include "g_level.h"

#include "gl/renderer/gl_renderer.h"
#include "gl/renderer/gl_lightdata.h"
#include "gl/data/gl_data.h"
#include "gl/dynlights/gl_dynlight.h"
#include "gl/dynlights/gl_glow.h"
#include "gl/models/gl_models.h"
#include "gl/utility/gl_clock.h"
#include "gl/shaders/gl_shader.h"
#include "gl/gl_functions.h"
// [BC]
#include "network.h"
#include "sv_commands.h"

GLRenderSettings glset;
long gl_frameMS;
long gl_frameCount;

EXTERN_CVAR(Int, gl_lightmode)

CUSTOM_CVAR(Float, maxviewpitch, 90.f, CVAR_ARCHIVE|CVAR_SERVERINFO)
{
	if (self>90.f) self=90.f;
	else if (self<-90.f) self=-90.f;
}


CUSTOM_CVAR(Bool, gl_nocoloredspritelighting, false, 0)
{
	glset.nocoloredspritelighting = self;
}

void gl_CreateSections();

//-----------------------------------------------------------------------------
//
// Adjust sprite offsets for GL rendering (IWAD resources only)
//
//-----------------------------------------------------------------------------

void AdjustSpriteOffsets()
{
	static bool done=false;
	char name[30];

	if (done) return;
	done=true;

	mysnprintf(name, countof(name), "sprofs/%s.sprofs", GameNames[gameinfo.gametype]);
	int lump = Wads.CheckNumForFullName(name);
	if (lump>=0)
	{
		FScanner sc;
		sc.OpenLumpNum(lump);
		GLRenderer->FlushTextures();
		while (sc.GetString())
		{
			int x,y;
			FTextureID texno = TexMan.CheckForTexture(sc.String, FTexture::TEX_Sprite);
			sc.GetNumber();
			x=sc.Number;
			sc.GetNumber();
			y=sc.Number;

			if (texno.isValid())
			{
				FTexture * tex = TexMan[texno];

				int lumpnum = tex->GetSourceLump();
				// We only want to change texture offsets for sprites in the IWAD!
				if (lumpnum >= 0 && lumpnum < Wads.GetNumLumps())
				{
					int wadno = Wads.GetLumpFile(lumpnum);
					if (wadno==FWadCollection::IWAD_FILENUM)
					{
						tex->LeftOffset=x;
						tex->TopOffset=y;
						tex->KillNative();
					}
				}
			}
		}
	}
}

// [BC] Moved to p_lnspec.cpp.
/*
// Normally this would be better placed in p_lnspec.cpp.
// But I have accidentally overwritten that file several times
// so I'd rather place it here.
static int LS_Sector_SetPlaneReflection (line_t *ln, AActor *it, bool backSide,
	int arg0, int arg1, int arg2, int arg3, int arg4)
{
// Sector_SetPlaneReflection (tag, floor, ceiling)
	int secnum = -1;

	while ((secnum = P_FindSectorFromTag (arg0, secnum)) >= 0)
	{
		sector_t * s = &sectors[secnum];
		if (s->floorplane.a==0 && s->floorplane.b==0) s->floor_reflect = arg1/255.f;
		if (s->ceilingplane.a==0 && s->ceilingplane.b==0) sectors[secnum].ceiling_reflect = arg2/255.f;

		// [BC] If we're the server, tell clients that this sector's reflection is being altered.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetSectorReflection( secnum );
	}

	return true;
}
*/

//==========================================================================
//
// Portal identifier lists
//
//==========================================================================

extern bool gl_disabled;


//==========================================================================
//
// MAPINFO stuff
//
//==========================================================================

struct FGLROptions : public FOptionalMapinfoData
{
	FGLROptions()
	{
		identifier = "gl_renderer";
		fogdensity = 0;
		outsidefogdensity = 0;
		skyfog = 0;
		lightmode = -1;
		nocoloredspritelighting = -1;
		skyrotatevector = FVector3(0,0,1);
	}
	virtual FOptionalMapinfoData *Clone() const
	{
		FGLROptions *newopt = new FGLROptions;
		newopt->identifier = identifier;
		newopt->fogdensity = fogdensity;
		newopt->outsidefogdensity = outsidefogdensity;
		newopt->skyfog = skyfog;
		newopt->lightmode = lightmode;
		newopt->nocoloredspritelighting = nocoloredspritelighting;
		newopt->skyrotatevector = skyrotatevector;
		return newopt;
	}
	int			fogdensity;
	int			outsidefogdensity;
	int			skyfog;
	int			lightmode;
	SBYTE		nocoloredspritelighting;
	FVector3	skyrotatevector;
	FVector3	skyrotatevector2;
};

DEFINE_MAP_OPTION(fogdensity, false)
{
	FGLROptions *opt = info->GetOptData<FGLROptions>("gl_renderer");
	parse.ParseAssign();
	parse.sc.MustGetNumber();
	opt->fogdensity = parse.sc.Number;
}

DEFINE_MAP_OPTION(outsidefogdensity, false)
{
	FGLROptions *opt = info->GetOptData<FGLROptions>("gl_renderer");
	parse.ParseAssign();
	parse.sc.MustGetNumber();
	opt->outsidefogdensity = parse.sc.Number;
}

DEFINE_MAP_OPTION(skyfog, false)
{
	FGLROptions *opt = info->GetOptData<FGLROptions>("gl_renderer");
	parse.ParseAssign();
	parse.sc.MustGetNumber();
	opt->skyfog = parse.sc.Number;
}

DEFINE_MAP_OPTION(lightmode, false)
{
	FGLROptions *opt = info->GetOptData<FGLROptions>("gl_renderer");
	parse.ParseAssign();
	parse.sc.MustGetNumber();
	opt->lightmode = BYTE(parse.sc.Number);
}

DEFINE_MAP_OPTION(nocoloredspritelighting, false)
{
	FGLROptions *opt = info->GetOptData<FGLROptions>("gl_renderer");
	if (parse.CheckAssign())
	{
		parse.sc.MustGetNumber();
		opt->nocoloredspritelighting = !!parse.sc.Number;
	}
	else
	{
		opt->nocoloredspritelighting = true;
	}
}

DEFINE_MAP_OPTION(skyrotate, false)
{
	FGLROptions *opt = info->GetOptData<FGLROptions>("gl_renderer");

	parse.ParseAssign();
	parse.sc.MustGetFloat();
	opt->skyrotatevector.X = (float)parse.sc.Float;
	if (parse.format_type == FMapInfoParser::FMT_New) parse.sc.MustGetStringName(","); 
	parse.sc.MustGetFloat();
	opt->skyrotatevector.Y = (float)parse.sc.Float;
	if (parse.format_type == FMapInfoParser::FMT_New) parse.sc.MustGetStringName(","); 
	parse.sc.MustGetFloat();
	opt->skyrotatevector.Z = (float)parse.sc.Float;
	opt->skyrotatevector.MakeUnit();
}

DEFINE_MAP_OPTION(skyrotate2, false)
{
	FGLROptions *opt = info->GetOptData<FGLROptions>("gl_renderer");

	parse.ParseAssign();
	parse.sc.MustGetFloat();
	opt->skyrotatevector2.X = (float)parse.sc.Float;
	if (parse.format_type == FMapInfoParser::FMT_New) parse.sc.MustGetStringName(","); 
	parse.sc.MustGetFloat();
	opt->skyrotatevector2.Y = (float)parse.sc.Float;
	if (parse.format_type == FMapInfoParser::FMT_New) parse.sc.MustGetStringName(","); 
	parse.sc.MustGetFloat();
	opt->skyrotatevector2.Z = (float)parse.sc.Float;
	opt->skyrotatevector2.MakeUnit();
}

void InitGLRMapinfoData()
{
	FGLROptions *opt = level.info->GetOptData<FGLROptions>("gl_renderer", false);

	if (opt != NULL)
	{
		gl_SetFogParams(opt->fogdensity, level.info->outsidefog, opt->outsidefogdensity, opt->skyfog);
		glset.map_lightmode = opt->lightmode;
		glset.map_nocoloredspritelighting = opt->nocoloredspritelighting;
		glset.skyrotatevector = opt->skyrotatevector;
		if (!gl_ExtFogActive() && glset.map_lightmode ==2) glset.map_lightmode = 3;
	}
	else
	{
		gl_SetFogParams(0, level.info->outsidefog, 0, 0);
		glset.map_lightmode = -1;
		glset.map_nocoloredspritelighting = -1;
		glset.skyrotatevector = FVector3(0,0,1);
	}

	// [SP/BB] Don't access gl_lightmode directly.
	if (glset.map_lightmode < 0 || glset.map_lightmode > 4) glset.lightmode = gl_GetLightMode();
	else glset.lightmode = glset.map_lightmode;
	if (glset.map_nocoloredspritelighting == -1) glset.nocoloredspritelighting = gl_nocoloredspritelighting;
	else glset.nocoloredspritelighting = !!glset.map_nocoloredspritelighting;
}

CCMD(gl_resetmap)
{
	// [SP/BB] Don't access gl_lightmode directly.
	if (glset.map_lightmode < 0 || glset.map_lightmode > 4) glset.lightmode = gl_GetLightMode();
	else glset.lightmode = glset.map_lightmode;
	if (glset.map_nocoloredspritelighting == -1) glset.nocoloredspritelighting = gl_nocoloredspritelighting;
	else glset.nocoloredspritelighting = !!glset.map_nocoloredspritelighting;
}


//===========================================================================
//
//  Gets the texture index for a sprite frame
//
//===========================================================================
const BYTE SF_FRAMEMASK  = 0x1f;

FTextureID gl_GetSpriteFrame(unsigned sprite, int frame, int rot, angle_t ang, bool *mirror)
{
	frame&=SF_FRAMEMASK;
	spritedef_t *sprdef = &sprites[sprite];
	if (frame >= sprdef->numframes)
	{
		// If there are no frames at all for this sprite, don't draw it.
		return FNullTextureID();
	}
	else
	{
		//picnum = SpriteFrames[sprdef->spriteframes + thing->frame].Texture[0];
		// choose a different rotation based on player view
		spriteframe_t *sprframe = &SpriteFrames[sprdef->spriteframes + frame];
		if (rot==-1)
		{
			if (sprframe->Texture[0] == sprframe->Texture[1])
			{
				rot = (ang + (angle_t)(ANGLE_45/2)*9) >> 28;
			}
			else
			{
				rot = (ang + (angle_t)(ANGLE_45/2)*9-(angle_t)(ANGLE_180/16)) >> 28;
			}
		}
		if (mirror) *mirror = !!(sprframe->Flip&(1<<rot));
		return sprframe->Texture[rot];
	}
}


//==========================================================================
//
// Recalculate all heights affectting this vertex.
//
//==========================================================================
void gl_RecalcVertexHeights(vertex_t * v)
{
	int i,j,k;
	float height;

	v->numheights=0;
	for(i=0;i<v->numsectors;i++)
	{
		for(j=0;j<2;j++)
		{
			if (j==0) height=FIXED2FLOAT(v->sectors[i]->ceilingplane.ZatPoint(v));
			else height=FIXED2FLOAT(v->sectors[i]->floorplane.ZatPoint(v));

			for(k=0;k<v->numheights;k++)
			{
				if (height == v->heightlist[k]) break;
				if (height < v->heightlist[k])
				{
					memmove(&v->heightlist[k+1], &v->heightlist[k], sizeof(float) * (v->numheights-k));
					v->heightlist[k]=height;
					v->numheights++;
					break;
				}
			}
			if (k==v->numheights) v->heightlist[v->numheights++]=height;
		}
	}
	if (v->numheights<=2) v->numheights=0;	// is not in need of any special attention
	v->dirty = false;
}


//==========================================================================
//
// Mark sectors dirty
//
//==========================================================================
int DirtyCount;

void sector_t::SetDirty(bool dolines, bool dovertices)
{
	Dirty.Clock();
	if (currentrenderer == 1 && this == &sectors[sectornum])
	{
		dirty = true;

		if (dirtyframe[0] != gl_frameCount)
		{
			DirtyCount++;
			dirtyframe[0] = gl_frameCount;
			for(unsigned i = 0; i < e->SectorDependencies.Size(); i++)
				e->SectorDependencies[i]->dirty = true;
		}

		if (dolines)
		{
			if (dirtyframe[1] != gl_frameCount)
			{
				dirtyframe[1] = gl_frameCount;

				for(unsigned i = 0; i < e->SideDependencies.Size(); i++)
					e->SideDependencies[i]->dirty = true;
			}
		}

		if (dovertices)
		{
			if (dirtyframe[2] != gl_frameCount)
			{
				dirtyframe[2] = gl_frameCount;

				for(unsigned i = 0; i < e->VertexDependencies.Size(); i++)
					e->VertexDependencies[i]->dirty = true;
			}
		}
	}
	Dirty.Unclock();
}


void gl_InitData()
{
	// [BB] This is set in p_lnspec.cpp
	//LineSpecials[159]=LS_Sector_SetPlaneReflection;
	gl_InitModels();
	AdjustSpriteOffsets();
}

//==========================================================================
//
// dumpgeometry
//
//==========================================================================

CCMD(dumpgeometry)
{
	for(int i=0;i<numsectors;i++)
	{
		sector_t * sector = &sectors[i];

		Printf("Sector %d\n",i);
		for(int j=0;j<sector->subsectorcount;j++)
		{
			subsector_t * sub = sector->subsectors[j];

			Printf("    Subsector %d - real sector = %d - %s\n", sub-subsectors, sub->sector->sectornum, sub->hacked&1? "hacked":"");
			for(DWORD k=0;k<sub->numlines;k++)
			{
				seg_t * seg = &segs[sub->firstline+k];
				if (seg->linedef)
				{
				Printf("      (%4.4f, %4.4f), (%4.4f, %4.4f) - seg %d, linedef %d, side %d", 
					FIXED2FLOAT(seg->v1->x), FIXED2FLOAT(seg->v1->y), FIXED2FLOAT(seg->v2->x), FIXED2FLOAT(seg->v2->y),
					seg-segs, seg->linedef-lines, seg->sidedef != seg->linedef->sidedef[0]);
				}
				else
				{
					Printf("      (%4.4f, %4.4f), (%4.4f, %4.4f) - seg %d, miniseg", 
						FIXED2FLOAT(seg->v1->x), FIXED2FLOAT(seg->v1->y), FIXED2FLOAT(seg->v2->x), FIXED2FLOAT(seg->v2->y),
						seg-segs);
				}
				if (seg->PartnerSeg) 
				{
					subsector_t * sub2 = seg->PartnerSeg->Subsector;
					Printf(", back sector = %d, real back sector = %d", sub2->render_sector->sectornum, seg->PartnerSeg->frontsector->sectornum);
				}
				Printf("\n");
			}
		}
	}
}

CCMD(dumpdependencies)
{
	for(int i=0;i<numsectors;i++)
	{
		Printf(PRINT_LOG, "Dependencies of sector %d\n", i);
		for(unsigned j = 0; j < sectors[i].e->VertexDependencies.Size(); j++)
		{
			Printf(PRINT_LOG,"\tVertex %d\n", int(sectors[i].e->VertexDependencies[j] - vertexes));
		}
		for(unsigned j = 0; j < sectors[i].e->SideDependencies.Size(); j++)
		{
			Printf(PRINT_LOG,"\tSide %d (Line %d)\n", int(sectors[i].e->SideDependencies[j] - sides), (sectors[i].e->SideDependencies[j]->linedef-lines));
		}
		for(unsigned j = 0; j < sectors[i].e->SectorDependencies.Size(); j++)
		{
			Printf(PRINT_LOG,"\tSector %d\n", int(sectors[i].e->SectorDependencies[j] - sectors));
		}
	}
}

