/*
** gl_wall.cpp
** Wall rendering preparation
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
#include "p_local.h"
#include "p_lnspec.h"
#include "a_sharedglobal.h"
#include "g_level.h"
#include "templates.h"
#include "gl/gl_struct.h"
#include "gl/gl_renderstruct.h"
#include "gl/gl_portal.h"
#include "gl/gl_lights.h"
#include "gl/gl_glow.h"
#include "gl/gl_data.h"
#include "gl/gl_texture.h"
#include "gl/gl_basic.h"
#include "gl/gl_functions.h"
#include "vectors.h"
#include "r_sky.h"

UniqueList<GLSkyInfo> UniqueSkies;
UniqueList<GLHorizonInfo> UniqueHorizons;
UniqueList<GLSectorStackInfo> UniqueStacks;
UniqueList<secplane_t> UniquePlaneMirrors;


CVAR(Bool,gl_mirrors,true,0)	// This is for debugging only!
CVAR(Bool,gl_mirror_envmap, true, CVAR_GLOBALCONFIG|CVAR_ARCHIVE)
CVAR(Bool, gl_render_segs, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR(Bool, gl_seamless, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR(Bool, gl_fakecontrast, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG|CVAR_NOINITCALL)

CUSTOM_CVAR(Bool, gl_render_precise, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	gl_render_segs=self;
	gl_seamless=self;
}



//==========================================================================
//
// 
//
//==========================================================================
void GLWall::PutWall(bool translucent)
{
	GLPortal * portal;
	int list;

	static char passflag[]={
		0,		//RENDERWALL_NONE,             
		1,		//RENDERWALL_TOP,              // unmasked
		1,		//RENDERWALL_M1S,              // unmasked
		2,		//RENDERWALL_M2S,              // depends on render and texture settings
		1,		//RENDERWALL_BOTTOM,           // unmasked
		4,		//RENDERWALL_SKYDOME,          // special
		3,		//RENDERWALL_FOGBOUNDARY,      // translucent
		4,		//RENDERWALL_HORIZON,          // special
		4,		//RENDERWALL_SKYBOX,           // special
		4,		//RENDERWALL_SECTORSTACK,      // special
		4,		//RENDERWALL_PLANEMIRROR,      // special
		4,		//RENDERWALL_MIRROR,           // special
		1,		//RENDERWALL_MIRRORSURFACE,    // needs special handling
		2,		//RENDERWALL_M2SNF,            // depends on render and texture settings, no fog
		2,		//RENDERWALL_M2SFOG,            // depends on render and texture settings, no fog
		3,		//RENDERWALL_COLOR,            // translucent
		2,		//RENDERWALL_FFBLOCK           // depends on render and texture settings
		4,		//RENDERWALL_COLORLAYER        // color layer needs special handling
	};
	
	if (gltexture && gltexture->GetTransparent())
	{
		translucent = true;
	}

	if (gl_fixedcolormap) 
	{
		// light planes don't get drawn with fullbright rendering
		if (!gltexture && passflag[type]!=4) return;

		Colormap.GetFixedColormap();
	}

	gl_CheckGlowing(this);

	if (translucent) // translucent walls
	{
		viewdistance = P_AproxDistance( ((seg->linedef->v1->x+seg->linedef->v2->x)>>1) - viewx,
											((seg->linedef->v1->y+seg->linedef->v2->y)>>1) - viewy);
		gl_drawinfo->drawlists[GLDL_TRANSLUCENT].AddWall(this);
	}
	else if (passflag[type]!=4)	// non-translucent walls
	{
		static DrawListType list_indices[2][2][2]={
			{ { GLDL_PLAIN, GLDL_FOG      }, { GLDL_MASKED,      GLDL_FOGMASKED      } },
			{ { GLDL_LIGHT, GLDL_LIGHTFOG }, { GLDL_LIGHTMASKED, GLDL_LIGHTFOGMASKED } }
		};

		bool masked;
		bool light = gl_forcemultipass;

		if (!gl_fixedcolormap)
		{
			if (gl_lights)
			{
				if (!seg->bPolySeg)
				{
					light = (seg->sidedef != NULL && seg->sidedef->lighthead[0] != NULL);
				}
				else if (sub)
				{
					light = sub->lighthead[0] != NULL;
				}
			}
		}
		else 
		{
			flags&=~GLWF_FOGGY;
		}

		masked = passflag[type]==1? false : (light && type!=RENDERWALL_FFBLOCK) || (gltexture && gltexture->tex->bMasked);

		list = list_indices[light][masked][!!(flags&GLWF_FOGGY)];
		if (list == GLDL_LIGHT)
		{
			if (gltexture->tex->gl_info.Brightmap && gl_brightmap_shader) list = GLDL_LIGHTBRIGHT;
			if (flags & GLWF_GLOW) list = GLDL_LIGHTBRIGHT;
		}
		gl_drawinfo->drawlists[list].AddWall(this);

	}
	else switch (type)
	{
	case RENDERWALL_COLORLAYER:
		gl_drawinfo->drawlists[GLDL_TRANSLUCENT].AddWall(this);
		break;

	// portals don't go into the draw list.
	// Instead they are added to the portal manager
	case RENDERWALL_HORIZON:
		horizon=UniqueHorizons.Get(horizon);
		portal=GLPortal::FindPortal(horizon);
		if (!portal) portal=new GLHorizonPortal(horizon);
		portal->AddLine(this);
		break;

	case RENDERWALL_SKYBOX:
		portal=GLPortal::FindPortal(skybox);
		if (!portal) portal=new GLSkyboxPortal(skybox);
		portal->AddLine(this);
		break;

	case RENDERWALL_SECTORSTACK:
		stack=UniqueStacks.Get(stack);	// map all stacks with the same displacement together
		portal=GLPortal::FindPortal(stack);
		if (!portal) portal=new GLSectorStackPortal(stack);
		portal->AddLine(this);
		break;

	case RENDERWALL_PLANEMIRROR:
		if (GLPortal::PlaneMirrorMode * planemirror->c <=0)
		{
			planemirror=UniquePlaneMirrors.Get(planemirror);
			portal=GLPortal::FindPortal(planemirror);
			if (!portal) portal=new GLPlaneMirrorPortal(planemirror);
			portal->AddLine(this);
		}
		break;

	case RENDERWALL_MIRROR:
		portal=GLPortal::FindPortal(seg->linedef);
		if (!portal) portal=new GLMirrorPortal(seg->linedef);
		portal->AddLine(this);
		if (gl_mirror_envmap) 
		{
			// draw a reflective layer over the mirror
			type=RENDERWALL_MIRRORSURFACE;
			gl_drawinfo->drawlists[GLDL_TRANSLUCENTBORDER].AddWall(this);
		}
		break;

	case RENDERWALL_SKY:
		sky=UniqueSkies.Get(sky);
		portal=GLPortal::FindPortal(sky);
		if (!portal) portal=new GLSkyPortal(sky);
		portal->AddLine(this);
		break;
	}
}

//==========================================================================
//
//	Sets 3D-floor lighting info
//
//==========================================================================

void GLWall::Put3DWall(lightlist_t * lightlist, bool translucent)
{
	bool fadewall = (!translucent && lightlist->caster && (lightlist->caster->flags&FF_FADEWALLS) && 
		!gl_isBlack((lightlist->extra_colormap)->Fade)) && gl_isBlack(Colormap.FadeColor);

	lightlevel=*lightlist->p_lightlevel;
	// relative light won't get changed here. It is constant across the entire wall.

	Colormap.CopyLightColor(lightlist->extra_colormap);
	if (fadewall) lightlevel=255;
	PutWall(translucent);

	if (fadewall)
	{
		FGLTexture * tex = gltexture;
		type = RENDERWALL_COLORLAYER;
		gltexture = NULL;
		Colormap.LightColor = (lightlist->extra_colormap)->Fade;
		alpha = (255-(*lightlist->p_lightlevel))/255.f*1.f;
		if (alpha>0.f) PutWall(true);

		type = RENDERWALL_FFBLOCK;
		alpha = 1.0;
		gltexture = tex;
	}
}

//==========================================================================
//
//  Splits a wall vertically if a 3D-floor
//	creates different lighting across the wall
//
//==========================================================================

void GLWall::SplitWall(sector_t * frontsector, bool translucent)
{
	GLWall copyWall1,copyWall2;
	fixed_t lightbottomleft;
	fixed_t lightbottomright;
	float maplightbottomleft;
	float maplightbottomright;
	unsigned int i;
	TArray<lightlist_t> & lightlist=frontsector->e->XFloor.lightlist;

	if (glseg.x1==glseg.x2 && glseg.y1==glseg.y2)
	{
		return;
	}

#ifdef _MSC_VER
#ifdef _DEBUG
	if (seg->linedef-lines==1)
		__asm nop
#endif
#endif

	if (lightlist.Size()>1)
	{
		// It's too bad that I still have to do some of the calculations here
		// in fixed point. The conversion introduces a needless inaccuracy here.

		#ifndef FLOAT_ENGINE
			fixed_t x1 = TO_MAP(glseg.x1);
			fixed_t y1 = TO_MAP(glseg.y1);
			fixed_t x2 = TO_MAP(glseg.x2);
			fixed_t y2 = TO_MAP(glseg.y2);
		#endif

		for(i=0;i<lightlist.Size()-1;i++)
		{
			#ifndef FLOAT_ENGINE
				if (i<lightlist.Size()-1) 
				{
					lightbottomleft = lightlist[i+1].plane.ZatPoint(x1,y1);
					lightbottomright= lightlist[i+1].plane.ZatPoint(x2,y2);
				}
				else 
				{
					lightbottomright = lightbottomleft = -32000*FRACUNIT;
				}

				maplightbottomleft=TO_GL(lightbottomleft);
				maplightbottomright=TO_GL(lightbottomright);
			#else
				if (i<lightlist.Size()-1) 
				{
					maplightbottomleft = lightlist[i+1].plane.ZatPoint(glseg.x1,glseg.y1);
					maplightbottomright= lightlist[i+1].plane.ZatPoint(glseg.x2,glseg.y2);
				}
				else 
				{
					maplightbottomright = maplightbottomleft = -32000;
				}
			#endif

			// The light is completely above the wall!
			if (maplightbottomleft>=ztop[0] && maplightbottomright>=ztop[1])
			{
				continue;
			}

			// check for an intersection with the upper plane
			if ((maplightbottomleft<ztop[0] && maplightbottomright>ztop[1]) ||
				(maplightbottomleft>ztop[0] && maplightbottomright<ztop[1]))
			{
				float clen = MAX<float>(fabsf(glseg.x2-glseg.x1), fabsf(glseg.y2-glseg.y2));

				float dch=ztop[1]-ztop[0];
				float dfh=maplightbottomright-maplightbottomleft;
				float coeff= (ztop[0]-maplightbottomleft)/(dfh-dch);
				
				// check for inaccuracies - let's be a little generous here!
				if (coeff*clen<.1f)
				{
					maplightbottomleft=ztop[0];
				}
				else if (coeff*clen>clen-.1f)
				{
					maplightbottomright=ztop[1];
				}
				else
				{
					// split the wall in 2 at the intersection and recursively split both halves
					copyWall1=copyWall2=*this;

					copyWall1.glseg.x2 = copyWall2.glseg.x1 = glseg.x1 + coeff * (glseg.x2-glseg.x1);
					copyWall1.glseg.y2 = copyWall2.glseg.y1 = glseg.y1 + coeff * (glseg.y2-glseg.y1);
					copyWall1.ztop[1] = copyWall2.ztop[0] = ztop[0] + coeff * (ztop[1]-ztop[0]);
					copyWall1.zbottom[1] = copyWall2.zbottom[0] = zbottom[0] + coeff * (zbottom[1]-zbottom[0]);
					copyWall1.glseg.fracright = copyWall2.glseg.fracleft = glseg.fracleft + coeff * (glseg.fracright-glseg.fracleft);
					copyWall1.uprgt.u = copyWall2.uplft.u = uplft.u + coeff * (uprgt.u-uplft.u);
					copyWall1.uprgt.v = copyWall2.uplft.v = uplft.v + coeff * (uprgt.v-uplft.v);
					copyWall1.lorgt.u = copyWall2.lolft.u = lolft.u + coeff * (lorgt.u-lolft.u);
					copyWall1.lorgt.v = copyWall2.lolft.v = lolft.v + coeff * (lorgt.v-lolft.v);

					copyWall1.SplitWall(frontsector, translucent);
					copyWall2.SplitWall(frontsector, translucent);
					return;
				}
			}

			// check for an intersection with the lower plane
			if ((maplightbottomleft<zbottom[0] && maplightbottomright>zbottom[1]) ||
				(maplightbottomleft>zbottom[0] && maplightbottomright<zbottom[1]))
			{
				float clen = MAX<float>(fabsf(glseg.x2-glseg.x1), fabsf(glseg.y2-glseg.y2));

				float dch=zbottom[1]-zbottom[0];
				float dfh=maplightbottomright-maplightbottomleft;
				float coeff= (zbottom[0]-maplightbottomleft)/(dfh-dch);

				// check for inaccuracies - let's be a little generous here because there's
				// some conversions between floats and fixed_t's involved
				if (coeff*clen<.1f)
				{
					maplightbottomleft=zbottom[0];
				}
				else if (coeff*clen>clen-.1f)
				{
					maplightbottomright=zbottom[1];
				}
				else
				{
					// split the wall in 2 at the intersection and recursively split both halves
					copyWall1=copyWall2=*this;

					copyWall1.glseg.x2 = copyWall2.glseg.x1 = glseg.x1 + coeff * (glseg.x2-glseg.x1);
					copyWall1.glseg.y2 = copyWall2.glseg.y1 = glseg.y1 + coeff * (glseg.y2-glseg.y1);
					copyWall1.ztop[1] = copyWall2.ztop[0] = ztop[0] + coeff * (ztop[1]-ztop[0]);
					copyWall1.zbottom[1] = copyWall2.zbottom[0] = zbottom[0] + coeff * (zbottom[1]-zbottom[0]);
					copyWall1.glseg.fracright = copyWall2.glseg.fracleft = glseg.fracleft + coeff * (glseg.fracright-glseg.fracleft);
					copyWall1.uprgt.u = copyWall2.uplft.u = uplft.u + coeff * (uprgt.u-uplft.u);
					copyWall1.uprgt.v = copyWall2.uplft.v = uplft.v + coeff * (uprgt.v-uplft.v);
					copyWall1.lorgt.u = copyWall2.lolft.u = lolft.u + coeff * (lorgt.u-lolft.u);
					copyWall1.lorgt.v = copyWall2.lolft.v = lolft.v + coeff * (lorgt.v-lolft.v);

					copyWall1.SplitWall(frontsector, translucent);
					copyWall2.SplitWall(frontsector, translucent);
					return;
				}
			}

			// 3D floor is completely within this light
			if (maplightbottomleft<=zbottom[0] && maplightbottomright<=zbottom[1])
			{
				// These values must not be destroyed!
				int ll=lightlevel;
				FColormap lc=Colormap;

				Put3DWall(&lightlist[i], translucent);

				lightlevel=ll;
				Colormap=lc;
				return;
			}

			if (maplightbottomleft<=ztop[0] && maplightbottomright<=ztop[1] &&
				(maplightbottomleft!=ztop[0] || maplightbottomright!=ztop[1]))
			{
				copyWall1=*this;

				ztop[0]=copyWall1.zbottom[0]=maplightbottomleft;
				ztop[1]=copyWall1.zbottom[1]=maplightbottomright;
				uplft.v=copyWall1.lolft.v=copyWall1.uplft.v+ 
					(maplightbottomleft-copyWall1.ztop[0])*(copyWall1.lolft.v-copyWall1.uplft.v)/(zbottom[0]-copyWall1.ztop[0]);
				uprgt.v=copyWall1.lorgt.v=copyWall1.uprgt.v+ 
					(maplightbottomright-copyWall1.ztop[1])*(copyWall1.lorgt.v-copyWall1.uprgt.v)/(zbottom[1]-copyWall1.ztop[1]);
				copyWall1.Put3DWall(&lightlist[i], translucent);
			}
			if (ztop[0]==zbottom[0] && ztop[1]==zbottom[1]) return;
		}
	}

	// These values must not be destroyed!
	int ll=lightlevel;
	FColormap lc=Colormap;

	Put3DWall(&lightlist[lightlist.Size()-1], translucent);

	lightlevel=ll;
	Colormap=lc;
}


//==========================================================================
//
// 
//
//==========================================================================
bool GLWall::DoHorizon(seg_t * seg,sector_t * fs, vertex_t * v1,vertex_t * v2)
{
	GLHorizonInfo hi;
	lightlist_t * light;

	// ZDoom doesn't support slopes in a horizon sector so I won't either!
	ztop[1]=ztop[0]=TO_GL(fs->GetPlaneTexZ(sector_t::ceiling));
	zbottom[1]=zbottom[0]=TO_GL(fs->GetPlaneTexZ(sector_t::floor));

	if (viewz<fs->GetPlaneTexZ(sector_t::ceiling))
	{
		if (viewz>fs->GetPlaneTexZ(sector_t::floor))
			zbottom[1]=zbottom[0]=TO_GL(viewz);

		if (fs->GetTexture(sector_t::ceiling)==skyflatnum)
		{
			SkyTexture(fs->sky, fs->CeilingSkyBox, true);
		}
		else
		{
			type=RENDERWALL_HORIZON;
			hi.plane.GetFromSector(fs, true);
			hi.lightlevel=GetCeilingLight(fs);
			hi.colormap=fs->ColorMap;

			if (fs->e->XFloor.ffloors.Size())
			{
				light = P_GetPlaneLight(fs, &fs->ceilingplane, true);

				if(!(fs->GetFlags(sector_t::ceiling)&SECF_ABSLIGHTING)) hi.lightlevel = *light->p_lightlevel;
				hi.colormap.LightColor = (light->extra_colormap)->Color;
			}

			if (gl_fixedcolormap) hi.colormap.GetFixedColormap();
			horizon=&hi;
			PutWall(0);
		}
		ztop[1]=ztop[0]=zbottom[0];
	}

	if (viewz>fs->GetPlaneTexZ(sector_t::floor))
	{
		zbottom[1]=zbottom[0]=TO_GL(fs->GetPlaneTexZ(sector_t::floor));
		if (fs->GetTexture(sector_t::floor)==skyflatnum)
		{
			SkyTexture(fs->sky, fs->FloorSkyBox, false);
		}
		else
		{
			type=RENDERWALL_HORIZON;
			hi.plane.GetFromSector(fs, false);
			hi.lightlevel=GetFloorLight(fs);
			hi.colormap=fs->ColorMap;

			if (fs->e->XFloor.ffloors.Size())
			{
				light = P_GetPlaneLight(fs, &fs->floorplane, false);

				if(!(fs->GetFlags(sector_t::floor)&SECF_ABSLIGHTING)) hi.lightlevel = *light->p_lightlevel;
				hi.colormap.LightColor = (light->extra_colormap)->Color;
			}

			if (gl_fixedcolormap) hi.colormap.GetFixedColormap();
			horizon=&hi;
			PutWall(0);
		}
	}
	return true;
}

//==========================================================================
//
// 
//
//==========================================================================
bool GLWall::SetWallCoordinates(seg_t * seg, int texturetop,
								int topleft,int topright, int bottomleft,int bottomright, int t_ofs)
{
	//
	//
	// set up texture coordinate stuff
	//
	// 
	const WorldTextureInfo * wti;
	float l_ul;
	float texlength;

	if (gltexture) 
	{
		float length = seg->sidedef? seg->sidedef->TexelLength: Dist2(glseg.x1, glseg.y1, glseg.x2, glseg.y2);

		wti=gltexture->GetWorldTextureInfo();
		l_ul=wti->FixToTexU(gltexture->TextureOffset(t_ofs));
		texlength = wti->FloatToTexU(length);
	}
	else 
	{
		wti=NULL;
		l_ul=0;
		texlength=0;
	}


	//
	//
	// set up coordinates for the left side of the polygon
	//
	// check left side for intersections
	if (topleft>=bottomleft)
	{
		// normal case
		ztop[0]=TO_GL(topleft);
		zbottom[0]=TO_GL(bottomleft);

		if (wti)
		{
			//uplft.u=lolft.u=l_ul;
			uplft.v=wti->FixToTexV(-topleft+texturetop);
			lolft.v=wti->FixToTexV(-bottomleft+texturetop);
		}
	}
	else
	{
		// ceiling below floor - clip to the visible part of the wall
		fixed_t dch=topright-topleft;
		fixed_t dfh=bottomright-bottomleft;
		fixed_t coeff=FixedDiv(bottomleft-topleft, dch-dfh);

		fixed_t inter_y=topleft+FixedMul(coeff,dch);
											 
		float inter_x= TO_GL(coeff);

		glseg.x1=glseg.x1+inter_x*(glseg.x2-glseg.x1);
		glseg.y1=glseg.y1+inter_x*(glseg.y2-glseg.y1);
		glseg.fracleft = inter_x;

		zbottom[0]=ztop[0]=TO_GL(inter_y);	

		if (wti)
		{
			//uplft.u=lolft.u=l_ul+wti->FloatToTexU(inter_x*length);
			lolft.v=uplft.v=wti->FixToTexV(-inter_y+texturetop);
		}
	}

	//
	//
	// set up coordinates for the right side of the polygon
	//
	// check left side for intersections
	if (topright>=bottomright)
	{
		// normal case
		ztop[1]=TO_GL(topright)		;
		zbottom[1]=TO_GL(bottomright)		;

		if (wti)
		{
			//uprgt.u=lorgt.u=l_ul+wti->FloatToTexU(length);
			uprgt.v=wti->FixToTexV(-topright+texturetop);
			lorgt.v=wti->FixToTexV(-bottomright+texturetop);
		}
	}
	else
	{
		// ceiling below floor - clip to the visible part of the wall
		fixed_t dch=topright-topleft;
		fixed_t dfh=bottomright-bottomleft;
		fixed_t coeff=FixedDiv(bottomleft-topleft, dch-dfh);

		fixed_t inter_y=topleft+FixedMul(coeff,dch);
											 
		float inter_x= TO_GL(coeff);

		glseg.x2=glseg.x1+inter_x*(glseg.x2-glseg.x1);
		glseg.y2=glseg.y1+inter_x*(glseg.y2-glseg.y1);
		glseg.fracright = inter_x;

		zbottom[1]=ztop[1]=TO_GL(inter_y);
		if (wti)
		{
			//uprgt.u=lorgt.u=l_ul+wti->FloatToTexU(inter_x*length);
			lorgt.v=uprgt.v=wti->FixToTexV(-inter_y+texturetop);
		}
	}

	uplft.u = lolft.u = l_ul + texlength * glseg.fracleft;
	uprgt.u = lorgt.u = l_ul + texlength * glseg.fracright;


	if (gltexture && gltexture->tex->bHasCanvas && flags&GLT_CLAMPY)
	{
		// Camera textures are upside down so we have to shift the y-coordinate
		// from [-1..0] to [0..1] when using texture clamping!

		uplft.v+=1.f;
		uprgt.v+=1.f;
		lolft.v+=1.f;
		lorgt.v+=1.f;
	}
	return true;
}

//==========================================================================
//
//  Handle one sided walls, upper and lower texture
//
//==========================================================================
void GLWall::DoTexture(int _type,seg_t * seg,int peg,
					   int ceilingrefheight,int floorrefheight,
					   int topleft,int topright,
					   int bottomleft,int bottomright,
					   int v_offset)
{
	if (topleft<=bottomleft && topright<=bottomright) return;

	// The Vertex values can be destroyed in this function and must be restored aferward!
	GLSeg glsave=glseg;
	int lh=ceilingrefheight-floorrefheight;
	int texpos;

	switch (_type)
	{
	case RENDERWALL_TOP:
		texpos = side_t::top;
		break;
	case RENDERWALL_BOTTOM:
		texpos = side_t::bottom;
		break;
	default:
		texpos = side_t::mid;
		break;
	}

	type = (seg->linedef->special == Line_Mirror && _type == RENDERWALL_M1S && 
		!(gl.flags & RFL_NOSTENCIL) && gl_mirrors) ? RENDERWALL_MIRROR : _type;

	ceilingrefheight+= 	gltexture->RowOffset(seg->sidedef->GetTextureYOffset(texpos))+
						(peg ? (gltexture->TextureHeight(FGLTexture::GLUSE_TEXTURE)<<FRACBITS)-lh-v_offset:0);

	if (!SetWallCoordinates(seg, ceilingrefheight, topleft, topright, bottomleft, bottomright, 
							seg->sidedef->GetTextureXOffset(texpos))) return;

	// Add this wall to the render list
	sector_t * sec = sub? sub->sector : seg->frontsector;
	if (sec->e->XFloor.lightlist.Size()==0 || gl_fixedcolormap) PutWall(false);
	else SplitWall(sec, false);
	glseg=glsave;
}


//==========================================================================
//
// 
//
//==========================================================================
void GLWall::DoMidTexture(seg_t * seg, bool drawfogboundary, 
						  sector_t * realfront, sector_t * realback,
						  fixed_t fch1, fixed_t fch2, fixed_t ffh1, fixed_t ffh2,
						  fixed_t bch1, fixed_t bch2, fixed_t bfh1, fixed_t bfh2)
								
{
	fixed_t topleft,bottomleft,topright,bottomright;
	GLSeg glsave=glseg;
	fixed_t texturetop, texturebottom;

	//
	//
	// Get the base coordinates for the texture
	//
	//
	if (gltexture)
	{
		// Align the texture to the ORIGINAL sector's height!!
		// At this point slopes don't matter because they don't affect the texture's z-position

		fixed_t rowoffset=gltexture->RowOffset(seg->sidedef->GetTextureYOffset(side_t::mid));
		if ( (seg->linedef->flags & ML_DONTPEGBOTTOM) >0)
		{
			texturebottom = MAX(realfront->GetPlaneTexZ(sector_t::floor),realback->GetPlaneTexZ(sector_t::floor))+rowoffset;
			texturetop=texturebottom+(gltexture->TextureHeight(FGLTexture::GLUSE_TEXTURE)<<FRACBITS);
		}
		else
		{
			texturetop = MIN(realfront->GetPlaneTexZ(sector_t::ceiling),realback->GetPlaneTexZ(sector_t::ceiling))+rowoffset;
			texturebottom=texturetop-(gltexture->TextureHeight(FGLTexture::GLUSE_TEXTURE)<<FRACBITS);
		}
	}
	else texturetop=texturebottom=0;

	//
	//
	// Depending on missing textures and possible plane intersections
	// decide which planes to use for the polygon
	//
	//
	if (realfront!=realback || 
		drawfogboundary || (seg->linedef->flags&ML_WRAP_MIDTEX) ||
		(realfront->heightsec && !(realfront->heightsec->MoreFlags & SECF_IGNOREHEIGHTSEC)))
	{
		//
		//
		// Set up the top
		//
		//
		FTexture * tex = TexMan(seg->sidedef->GetTexture(side_t::top));
		if (!tex || tex->UseType==FTexture::TEX_Null)
		{
			// texture is missing - use the higher plane
			topleft = MAX(bch1,fch1);
			topright = MAX(bch2,fch2);
		}
		else if ((bch1>fch1 || bch2>fch2) && 
				 (seg->frontsector->GetTexture(sector_t::ceiling)!=skyflatnum || seg->backsector->GetTexture(sector_t::ceiling)==skyflatnum)) 
				 // (!((bch1<=fch1 && bch2<=fch2) || (bch1>=fch1 && bch2>=fch2)))
		{
			// Use the higher plane and let the geometry clip the extruding part
			topleft = bch1;
			topright = bch2;
		}
		else
		{
			// But not if there can be visual artifacts.
			topleft = MIN(bch1,fch1);
			topright = MIN(bch2,fch2);
		}
		
		//
		//
		// Set up the bottom
		//
		//
		tex = TexMan(seg->sidedef->GetTexture(side_t::bottom));
		if (!tex || tex->UseType==FTexture::TEX_Null)
		{
			// texture is missing - use the lower plane
			bottomleft = MIN(bfh1,ffh1);
			bottomright = MIN(bfh2,ffh2);
		}
		else if (bfh1<ffh1 || bfh2<ffh2) // (!((bfh1<=ffh1 && bfh2<=ffh2) || (bfh1>=ffh1 && bfh2>=ffh2)))
		{
			// the floor planes intersect. Use the backsector's floor for drawing so that
			// drawing the front sector's plane clips the polygon automatically.
			bottomleft = bfh1;
			bottomright = bfh2;
		}
		else
		{
			// normal case - use the higher plane
			bottomleft = MAX(bfh1,ffh1);
			bottomright = MAX(bfh2,ffh2);
		}
		
		//
		//
		// if we don't need a fog sheet let's clip away some unnecessary parts of the polygon
		//
		//
		if (!drawfogboundary && !(seg->linedef->flags&ML_WRAP_MIDTEX))
		{
			if (texturetop<topleft && texturetop<topright) topleft=topright=texturetop;
			if (texturebottom>bottomleft && texturebottom>bottomright) bottomleft=bottomright=texturebottom;
		}
	}
	else
	{
		//
		//
		// if both sides of the line are in the same sector and the sector
		// doesn't have a Transfer_Heights special don't clip to the planes
		// Clipping to the planes is not necessary and can even produce
		// unwanted side effects.
		//
		//
		topleft=topright=texturetop;
		bottomleft=bottomright=texturebottom;
	}

	// nothing visible - skip the rest
	if (topleft<=bottomleft && topright<=bottomright) return;


	//
	//
	// set up texture coordinate stuff
	//
	// 
	fixed_t t_ofs = seg->sidedef->GetTextureXOffset(side_t::mid);

	if (gltexture)
	{
		// First adjust the texture offset so that the left edge of the linedef is inside the range [0..1].
		fixed_t texwidth = gltexture->TextureAdjustWidth(FGLTexture::GLUSE_TEXTURE)<<FRACBITS;
		
		t_ofs%=texwidth;
		if (t_ofs<-texwidth) t_ofs+=texwidth;	// shift negative results of % into positive range


		// Now check whether the linedef is completely within the texture range of [0..1].
		// If so we should use horizontal texture clamping to prevent filtering artifacts
		// at the edges.

		fixed_t textureoffset=gltexture->TextureOffset(t_ofs);
		int righttex=(textureoffset>>FRACBITS)+seg->sidedef->TexelLength;
		
		if ((textureoffset==0 && righttex<=gltexture->TextureWidth(FGLTexture::GLUSE_TEXTURE)) || 
			(textureoffset>=0 && righttex==gltexture->TextureWidth(FGLTexture::GLUSE_TEXTURE)))
		{
			flags|=GLT_CLAMPX;
		}
		else
		{
			flags&=~GLT_CLAMPX;
		}
		if (!(seg->linedef->flags&ML_WRAP_MIDTEX))
		{
			flags|=GLT_CLAMPY;
		}
	}
	SetWallCoordinates(seg, texturetop, topleft, topright, bottomleft, bottomright, t_ofs);

	//
	//
	// draw fog sheet if required
	//
	// 
	if (drawfogboundary)
	{
		type=RENDERWALL_FOGBOUNDARY;
		PutWall(true);
		if (!gltexture) return;
		type=RENDERWALL_M2SNF;
	}
	else type=RENDERWALL_M2S;

	//
	//
	// set up alpha blending
	//
	// 
	if (seg->linedef->Alpha)// && seg->linedef->special!=Line_Fogsheet)
	{
		bool translucent = false;

		switch (seg->linedef->flags& ML_ADDTRANS)//TRANSBITS)
		{
		case 0:
			RenderStyle=STYLE_Translucent;
			alpha=FIXED2FLOAT(seg->linedef->Alpha);
			translucent = seg->linedef->Alpha < FRACUNIT || (gltexture && gltexture->GetTransparent());
			break;

		case ML_ADDTRANS:
			RenderStyle=STYLE_Add;
			alpha=FIXED2FLOAT(seg->linedef->Alpha);
			translucent=true;
			break;
		}

		//
		//
		// for textures with large empty areas only the visible parts are drawn.
		// If these textures come too close to the camera they severely affect performance
		// if stacked closely together
		// Recognizing vertical gaps is rather simple and well worth the effort.
		//
		//
		int v=gltexture->GetAreaCount();
		if (v>0 && !drawfogboundary && !(seg->linedef->flags&ML_WRAP_MIDTEX))
		{
			// split the poly!
			GLWall split;
			int i,t=0;
			GL_RECT * splitrect=gltexture->GetAreas();
			float v_factor=(zbottom[0]-ztop[0])/(lolft.v-uplft.v);
			// only split the vertical area of the polygon that does not contain slopes!
			float splittopv = MAX(uplft.v, uprgt.v);
			float splitbotv = MIN(lolft.v, lorgt.v);

			// this is split vertically into sections.
			for(i=0;i<v;i++)
			{
				// the current segment is below the bottom line of the splittable area
				// (iow. the whole wall has been done)
				if (splitrect[i].top>=splitbotv) break;

				float splitbot=splitrect[i].top+splitrect[i].height;

				// the current segment is above the top line of the splittable area
				if (splitbot<=splittopv) continue;

				split=*this;

				// the top line of the current segment is inside the splittable area
				// use the splitrect's top as top of this segment
				// if not use the top of the remaining polygon
				if (splitrect[i].top>splittopv)
				{
					split.ztop[0]=split.ztop[1]= ztop[0]+v_factor*(splitrect[i].top-uplft.v);
					split.uplft.v=split.uprgt.v=splitrect[i].top;
				}
				// the bottom line of the current segment is inside the splittable area
				// use the splitrect's bottom as bottom of this segment
				// if not use the bottom of the remaining polygon
				if (splitbot<splitbotv)
				{
					split.zbottom[0]=split.zbottom[1]=ztop[0]+v_factor*(splitbot-uplft.v);
					split.lolft.v=split.lorgt.v=splitbot;
				}
				//
				//
				// Draw the stuff
				//
				//
				if (realfront->e->XFloor.lightlist.Size()==0 || gl_fixedcolormap) split.PutWall(translucent);
				else split.SplitWall(realfront, translucent);

				t=1;
			}
			render_texsplit+=t;
		}
		else
		{
			//
			//
			// Draw the stuff without splitting
			//
			//
			if (realfront->e->XFloor.lightlist.Size()==0 || gl_fixedcolormap) PutWall(translucent);
			else SplitWall(realfront, translucent);
		}
		alpha=1.0f;
	}
	// restore some values that have been altered in this function!
	glseg=glsave;
	flags&=~(GLT_CLAMPX|GLT_CLAMPY);
}


//==========================================================================
//
// 
//
//==========================================================================
void GLWall::BuildFFBlock(seg_t * seg, F3DFloor * rover,
						  fixed_t ff_topleft, fixed_t ff_topright, 
						  fixed_t ff_bottomleft, fixed_t ff_bottomright)
{
	side_t * mastersd=&sides[rover->master->sidenum[0]];
	int to;
	lightlist_t * light;
	bool translucent;
	byte savelight=lightlevel;
	FColormap savecolor=Colormap;
	float ul;
	float texlength;

	if (rover->flags&FF_FOG)
	{
		if (!gl_fixedcolormap)
		{
			// this may not yet be done!
			light=P_GetPlaneLight(rover->target, rover->top.plane,true);
			Colormap.Clear();
			Colormap.LightColor=(light->extra_colormap)->Fade;
			// the fog plane defines the light level, not the front sector!
			lightlevel=*light->p_lightlevel;
			gltexture=NULL;
			type=RENDERWALL_FFBLOCK;
		}
		else return;
	}
	else
	{
		FTextureID texno;
		
		if (rover->flags&FF_UPPERTEXTURE) texno = seg->sidedef->GetTexture(side_t::top);
		else if (rover->flags&FF_LOWERTEXTURE) texno = seg->sidedef->GetTexture(side_t::bottom);
		else texno = mastersd->GetTexture(side_t::mid);
			
		gltexture = FGLTexture::ValidateTexture(texno);

		if (!gltexture) return;
		const WorldTextureInfo * wti=gltexture->GetWorldTextureInfo();
		if (!wti) return;

		
		to=(rover->flags&(FF_UPPERTEXTURE|FF_LOWERTEXTURE))? 
			0:gltexture->TextureOffset(mastersd->GetTextureXOffset(side_t::mid));
		ul=wti->FixToTexU(to+gltexture->TextureOffset(seg->sidedef->GetTextureXOffset(side_t::mid)));
		texlength = wti->FloatToTexU(seg->sidedef->TexelLength);

		uplft.u = lolft.u = ul + texlength * glseg.fracleft;
		uprgt.u = lorgt.u = ul + texlength * glseg.fracright;
		
		fixed_t rowoffset=gltexture->RowOffset(seg->sidedef->GetTextureYOffset(side_t::mid));
		to=(rover->flags&(FF_UPPERTEXTURE|FF_LOWERTEXTURE))? 
			0:gltexture->RowOffset(mastersd->GetTextureYOffset(side_t::mid));
		
		uplft.v=wti->FixToTexV(to+rowoffset+*rover->top.texheight-ff_topleft);
		uprgt.v=wti->FixToTexV(to+rowoffset+*rover->top.texheight-ff_topright);
		lolft.v=wti->FixToTexV(to+rowoffset+*rover->top.texheight-ff_bottomleft);
		lorgt.v=wti->FixToTexV(to+rowoffset+*rover->top.texheight-ff_bottomright);
		type=RENDERWALL_FFBLOCK;
	}

	ztop[0]=TO_GL(ff_topleft);
	ztop[1]=TO_GL(ff_topright);
	zbottom[0]=TO_GL(ff_bottomleft);//-0.001f;
	zbottom[1]=TO_GL(ff_bottomright);

	if (rover->flags&(FF_TRANSLUCENT|FF_ADDITIVETRANS|FF_FOG))
	{
		alpha=rover->alpha/255.0f;
		RenderStyle = (rover->flags&FF_ADDITIVETRANS)? STYLE_Add : STYLE_Translucent;
		translucent=true;
		type=gltexture? RENDERWALL_M2S:RENDERWALL_COLOR;
	}
	else 
	{
		alpha=1.0f;
		RenderStyle=STYLE_Normal;
		translucent=false;
	}
	
	sector_t * sec = sub? sub->sector : seg->frontsector;
	if (sec->e->XFloor.lightlist.Size()==0 || gl_fixedcolormap) PutWall(translucent);
	else SplitWall(sec, translucent);
	alpha=1.0f;
	lightlevel = savelight;
	Colormap = savecolor;
}

//==========================================================================
//
// 
//
//==========================================================================
void GLWall::InverseFloors(seg_t * seg, sector_t * frontsector,
						   fixed_t topleft, fixed_t topright, 
						   fixed_t bottomleft, fixed_t bottomright)
{
	TArray<F3DFloor *> & frontffloors=frontsector->e->XFloor.ffloors;

	for(unsigned int i=0;i<frontffloors.Size();i++)
	{
		F3DFloor * rover=frontffloors[i];
		if (!(rover->flags&FF_EXISTS)) continue;
		if (!(rover->flags&FF_RENDERSIDES)) continue;
		if (!(rover->flags&(FF_INVERTSIDES|FF_ALLSIDES))) continue;

		fixed_t ff_topleft;
		fixed_t ff_topright;
		fixed_t ff_bottomleft;
		fixed_t ff_bottomright;

		if (rover->top.plane->a | rover->top.plane->b)
		{
			ff_topleft=rover->top.plane->ZatPoint(vertexes[0]);
			ff_topright=rover->top.plane->ZatPoint(vertexes[1]);
		}
		else
		{
			ff_topleft = ff_topright = *rover->top.texheight;
		}

		if (rover->bottom.plane->a | rover->bottom.plane->b)
		{
			ff_bottomleft=rover->bottom.plane->ZatPoint(vertexes[0]);
			ff_bottomright=rover->bottom.plane->ZatPoint(vertexes[1]);
		}
		else
		{
			ff_bottomleft = ff_bottomright = *rover->bottom.texheight;
		}

		// above ceiling
		if (ff_bottomleft>topleft && ff_bottomright>topright) continue;

		if (ff_topleft>topleft && ff_topright>topright)
		{
			// the new section overlaps with the previous one - clip it!
			ff_topleft=topleft;
			ff_topright=topright;
		}
		if (ff_bottomleft<bottomleft && ff_bottomright<bottomright)
		{
			ff_bottomleft=bottomleft;
			ff_bottomright=bottomright;
		}
		if (ff_topleft<ff_bottomleft || ff_topright<ff_bottomright) continue;

		BuildFFBlock(seg, rover, ff_topleft, ff_topright, ff_bottomleft, ff_bottomright);
		topleft=ff_bottomleft;
		topright=ff_bottomright;

		if (topleft<=bottomleft && topright<=bottomright) return;
	}

}

//==========================================================================
//
// 
//
//==========================================================================
void GLWall::ClipFFloors(seg_t * seg, F3DFloor * ffloor, sector_t * frontsector,
						 fixed_t topleft, fixed_t topright, 
						 fixed_t bottomleft, fixed_t bottomright)
{
	TArray<F3DFloor *> & frontffloors=frontsector->e->XFloor.ffloors;

	int flags=ffloor->flags&FF_SWIMMABLE|FF_TRANSLUCENT;

	for(unsigned int i=0;i<frontffloors.Size();i++)
	{
		F3DFloor * rover=frontffloors[i];
		if (!(rover->flags&FF_EXISTS)) continue;
		if (!(rover->flags&FF_RENDERSIDES)) continue;
		if ((rover->flags&flags)!=flags) continue;

		fixed_t ff_topleft;
		fixed_t ff_topright;
		fixed_t ff_bottomleft;
		fixed_t ff_bottomright;

		if (rover->top.plane->a | rover->top.plane->b)
		{
			ff_topleft=rover->top.plane->ZatPoint(vertexes[0]);
			ff_topright=rover->top.plane->ZatPoint(vertexes[1]);
		}
		else
		{
			ff_topleft = ff_topright = *rover->top.texheight;
		}

		// we are completely below the bottom so unless there are some
		// (unsupported) intersections there won't be any more floors that
		// could clip this one.
		if (ff_topleft<bottomleft && ff_topright<bottomright) goto done;

		if (rover->bottom.plane->a | rover->bottom.plane->b)
		{
			ff_bottomleft=rover->bottom.plane->ZatPoint(vertexes[0]);
			ff_bottomright=rover->bottom.plane->ZatPoint(vertexes[1]);
		}
		else
		{
			ff_bottomleft = ff_bottomright = *rover->bottom.texheight;
		}
		// above top line
		if (ff_bottomleft>topleft && ff_bottomright>topright) continue;

		// overlapping the top line
		if (ff_topleft>=topleft && ff_topright>=topright)
		{
			// overlapping with the entire range
			if (ff_bottomleft<=bottomleft && ff_bottomright<=bottomright) return;
			else if (ff_bottomleft>bottomleft && ff_bottomright>bottomright)
			{
				topleft=ff_bottomleft;
				topright=ff_bottomright;
			}
			else
			{
				// an intersecting case.
				// proper handling requires splitting but
				// I don't need this right now.
			}
		}
		else if (ff_topleft<=topleft && ff_topright<=topright)
		{
			BuildFFBlock(seg, ffloor, topleft, topright, ff_topleft, ff_topright);
			if (ff_bottomleft<=bottomleft && ff_bottomright<=bottomright) return;
			topleft=ff_bottomleft;
			topright=ff_bottomright;
		}
		else
		{
			// an intersecting case.
			// proper handling requires splitting but
			// I don't need this right now.
		}
	}

done:
	// if the program reaches here there is one block left to draw
	BuildFFBlock(seg, ffloor, topleft, topright, bottomleft, bottomright);
}

//==========================================================================
//
// 
//
//==========================================================================
void GLWall::DoFFloorBlocks(seg_t * seg,sector_t * frontsector,sector_t * backsector,
							fixed_t fch1, fixed_t fch2, fixed_t ffh1, fixed_t ffh2,
							fixed_t bch1, fixed_t bch2, fixed_t bfh1, fixed_t bfh2)

{
	TArray<F3DFloor *> & backffloors=backsector->e->XFloor.ffloors;
	fixed_t topleft, topright, bottomleft, bottomright;
	bool renderedsomething=false;

	// if the ceilings intersect use the backsector's height because this sector's ceiling will
	// obstruct the redundant parts.

	if (fch1<bch1 && fch2<bch2) 
	{
		topleft=fch1;
		topright=fch2;
	}
	else
	{
		topleft=bch1;
		topright=bch2;
	}

	if (ffh1>bfh1 && ffh2>bfh2) 
	{
		bottomleft=ffh1;
		bottomright=ffh2;
	}
	else
	{
		bottomleft=bfh1;
		bottomright=bfh2;
	}

	for(unsigned int i=0;i<backffloors.Size();i++)
	{
		F3DFloor * rover=backffloors[i];
		if (!(rover->flags&FF_EXISTS)) continue;
		if (!(rover->flags&FF_RENDERSIDES) || (rover->flags&FF_INVERTSIDES)) continue;

		fixed_t ff_topleft;
		fixed_t ff_topright;
		fixed_t ff_bottomleft;
		fixed_t ff_bottomright;

		if (rover->top.plane->a | rover->top.plane->b)
		{
			ff_topleft=rover->top.plane->ZatPoint(vertexes[0]);
			ff_topright=rover->top.plane->ZatPoint(vertexes[1]);
		}
		else
		{
			ff_topleft = ff_topright = *rover->top.texheight;
		}

		if (rover->bottom.plane->a | rover->bottom.plane->b)
		{
			ff_bottomleft=rover->bottom.plane->ZatPoint(vertexes[0]);
			ff_bottomright=rover->bottom.plane->ZatPoint(vertexes[1]);
		}
		else
		{
			ff_bottomleft = ff_bottomright = *rover->bottom.texheight;
		}


		// completely above ceiling
		if (ff_bottomleft>topleft && ff_bottomright>topright && !renderedsomething) continue;

		if (ff_topleft>topleft && ff_topright>topright)
		{
			// the new section overlaps with the previous one - clip it!
			ff_topleft=topleft;
			ff_topright=topright;
		}

		// do all inverse floors above the current one it there is a gap between the
		// last 3D floor and this one.
		if (topleft>ff_topleft && topright>ff_topright)
			InverseFloors(seg, frontsector, topleft, topright, ff_topleft, ff_topright);

		// if translucent or liquid clip away adjoining parts of the same type of FFloors on the other side
		if (rover->flags&(FF_SWIMMABLE|FF_TRANSLUCENT))
			ClipFFloors(seg, rover, frontsector, ff_topleft, ff_topright, ff_bottomleft, ff_bottomright);
		else
			BuildFFBlock(seg, rover, ff_topleft, ff_topright, ff_bottomleft, ff_bottomright);

		topleft=ff_bottomleft;
		topright=ff_bottomright;
		renderedsomething=true;
		if (topleft<=bottomleft && topright<=bottomright) return;
	}

	// draw all inverse floors below the lowest one
	if (frontsector->e->XFloor.ffloors.Size() > 0)
	{
		if (topleft>bottomleft || topright>bottomright)
			InverseFloors(seg, frontsector, topleft, topright, bottomleft, bottomright);
	}
}



//==========================================================================
//
// 
//
//==========================================================================
void GLWall::Process(seg_t *seg, sector_t * frontsector, sector_t * backsector, subsector_t * polysub, bool render_segs)
{
	vertex_t * v1, * v2;
	fixed_t fch1;
	fixed_t ffh1;
	fixed_t fch2;
	fixed_t ffh2;
	sector_t * realfront;
	sector_t * realback;

#ifdef _MSC_VER
#ifdef _DEBUG
	if (seg->linedef-lines==308)
		__asm nop
#endif
#endif

	this->seg=seg;
	this->sub=polysub ? polysub : seg->Subsector;

	if (polysub && seg->backsector)
	{
		// Textures on 2-sided polyobjects are aligned to the actual seg's sectors!
		realfront = seg->frontsector;
		realback = seg->backsector;
	}
	else
	{
		// Need these for aligning the textures!
		realfront = &sectors[frontsector->sectornum];
		realback = backsector? &sectors[backsector->sectornum] : NULL;
	}

	if (seg->sidedef == &sides[seg->linedef->sidenum[0]])
	{
		v1=seg->linedef->v1;
		v2=seg->linedef->v2;
	}
	else
	{
		v1=seg->linedef->v2;
		v2=seg->linedef->v1;
	}
	glseg.fracleft=0;
	glseg.fracright=1;

	if (render_segs)
	{
		if (abs(v1->x-v2->x) > abs(v1->y-v2->y))
		{
			glseg.fracleft = float(seg->v1->x - v1->x)/float(v2->x-v1->x);
			glseg.fracright = float(seg->v2->x - v1->x)/float(v2->x-v1->x);
		}
		else
		{
			glseg.fracleft = float(seg->v1->y - v1->y)/float(v2->y-v1->y);
			glseg.fracright = float(seg->v2->y - v1->y)/float(v2->y-v1->y);
		}
		v1=seg->v1;
		v2=seg->v2;
	}
	if (gl_seamless)
	{
		gl_RecalcVertexHeights(v1);
		gl_RecalcVertexHeights(v2);
	}

	vertexes[0]=v1;
	vertexes[1]=v2;

	glseg.x1= TO_GL(v1->x);
	glseg.y1= TO_GL(v1->y);
	glseg.x2= TO_GL(v2->x);
	glseg.y2= TO_GL(v2->y);
	Colormap=frontsector->ColorMap;
	flags = (!gl_isBlack(Colormap.FadeColor) || level.flags&LEVEL_HASFADETABLE)? GLWF_FOGGY : 0;



	lightlevel = seg->sidedef->GetLightLevel(true, frontsector->lightlevel);

	if (lightlevel<255 && gl_fakecontrast && !(flags&GLWF_FOGGY))
	{
		// In GL it's preferable to use the relative light for fake contrast instead of
		// altering the base light level which is also used to set fog density.
		rellight = seg->sidedef->GetLightLevel(false, frontsector->lightlevel) - lightlevel;
	}
	else rellight=0;

	alpha=1.0f;
	RenderStyle=STYLE_Normal;
	gltexture=NULL;

	topflat=frontsector->GetTexture(sector_t::ceiling);	// for glowing textures. These must be saved because
	bottomflat=frontsector->GetTexture(sector_t::floor);	// the sector passed here might be a temporary copy.

	// Save a little time (up to 0.3 ms per frame ;) )
	if (frontsector->floorplane.a | frontsector->floorplane.b)
	{
		ffh1=frontsector->floorplane.ZatPoint(v1); 
		ffh2=frontsector->floorplane.ZatPoint(v2); 
		zfloor[0]=TO_GL(ffh1);
		zfloor[1]=TO_GL(ffh2);
	}
	else
	{
		ffh1 = ffh2 = frontsector->GetPlaneTexZ(sector_t::floor); 
		zfloor[0] = zfloor[1] = TO_GL(ffh2);
	}

	if (frontsector->ceilingplane.a | frontsector->ceilingplane.b)
	{
		fch1=frontsector->ceilingplane.ZatPoint(v1);
		fch2=frontsector->ceilingplane.ZatPoint(v2);
		zceil[0]= TO_GL(fch1);
		zceil[1]= TO_GL(fch2);
	}
	else
	{
		fch1 = fch2 = frontsector->GetPlaneTexZ(sector_t::ceiling);
		zceil[0] = zceil[1] = TO_GL(fch2);
	}


	if (seg->linedef->special==Line_Horizon && !(gl.flags&RFL_NOSTENCIL))
	{
		SkyNormal(frontsector,v1,v2);
		DoHorizon(seg,frontsector, v1,v2);
		return;
	}

	//return;
	if (!backsector || !(seg->linedef->flags&ML_TWOSIDED)) // one sided
	{
		// sector's sky
		SkyNormal(frontsector,v1,v2);
		
		// normal texture
		gltexture=FGLTexture::ValidateTexture(seg->sidedef->GetTexture(side_t::mid));
		if (!gltexture) return;

		DoTexture(RENDERWALL_M1S,seg,(seg->linedef->flags & ML_DONTPEGBOTTOM)>0,
						  realfront->GetPlaneTexZ(sector_t::ceiling),realfront->GetPlaneTexZ(sector_t::floor),	// must come from the original!
						  fch1,fch2,ffh1,ffh2,0);
	}
	else // two sided
	{

		fixed_t bch1;
		fixed_t bch2;
		fixed_t bfh1;
		fixed_t bfh2;

		if (backsector->floorplane.a | backsector->floorplane.b)
		{
			bfh1=backsector->floorplane.ZatPoint(v1); 
			bfh2=backsector->floorplane.ZatPoint(v2); 
		}
		else
		{
			bfh1 = bfh2 = backsector->GetPlaneTexZ(sector_t::floor); 
		}

		if (backsector->ceilingplane.a | backsector->ceilingplane.b)
		{
			bch1=backsector->ceilingplane.ZatPoint(v1);
			bch2=backsector->ceilingplane.ZatPoint(v2);
		}
		else
		{
			bch1 = bch2 = backsector->GetPlaneTexZ(sector_t::ceiling);
		}

		SkyTop(seg,frontsector,backsector,v1,v2);
		SkyBottom(seg,frontsector,backsector,v1,v2);
		
		// upper texture
		if (frontsector->GetTexture(sector_t::ceiling)!=skyflatnum || backsector->GetTexture(sector_t::ceiling)!=skyflatnum)
		{
			fixed_t bch1a=bch1, bch2a=bch2;
			if (frontsector->GetTexture(sector_t::floor)!=skyflatnum || backsector->GetTexture(sector_t::floor)!=skyflatnum)
			{
				// the back sector's floor obstructs part of this wall				
				if (ffh1>bch1 && ffh2>bch2) 
				{
					bch2a=ffh2;
					bch1a=ffh1;
				}
			}

			if (bch1a<fch1 || bch2a<fch2)
			{
				gltexture=FGLTexture::ValidateTexture(seg->sidedef->GetTexture(side_t::top));
				if (gltexture) 
				{
					DoTexture(RENDERWALL_TOP,seg,(seg->linedef->flags & (ML_DONTPEGTOP))==0,
						realfront->GetPlaneTexZ(sector_t::ceiling),realback->GetPlaneTexZ(sector_t::ceiling),
						fch1,fch2,bch1a,bch2a,0);
				}
				else if ((frontsector->ceilingplane.a | frontsector->ceilingplane.b | 
						 backsector->ceilingplane.a | backsector->ceilingplane.b) && 
						frontsector->GetTexture(sector_t::ceiling)!=skyflatnum &&
						backsector->GetTexture(sector_t::ceiling)!=skyflatnum)
				{
					gltexture=FGLTexture::ValidateTexture(frontsector->GetTexture(sector_t::ceiling));
					if (gltexture)
					{
						DoTexture(RENDERWALL_TOP,seg,(seg->linedef->flags & (ML_DONTPEGTOP))==0,
							realfront->GetPlaneTexZ(sector_t::ceiling),realback->GetPlaneTexZ(sector_t::ceiling),
							fch1,fch2,bch1a,bch2a,0);
					}
				}
				else
				{
					gl_drawinfo->AddUpperMissingTexture(seg, bch1a);
				}
			}
		}


		/* mid texture */

		// in all other cases this might create more problems than it solves.
		bool drawfogboundary=((frontsector->ColorMap->Fade&0xffffff)!=0 && 
							(backsector->ColorMap->Fade&0xffffff)==0 &&
							!gl_fixedcolormap &&
							(frontsector->GetTexture(sector_t::ceiling)!=skyflatnum || backsector->GetTexture(sector_t::ceiling)!=skyflatnum));

		gltexture=FGLTexture::ValidateTexture(seg->sidedef->GetTexture(side_t::mid));
		if (gltexture || drawfogboundary)
		{
			DoMidTexture(seg, drawfogboundary, realfront, realback, fch1, fch2, ffh1, ffh2, bch1, bch2, bfh1, bfh2);
		}

		if (backsector->e->XFloor.ffloors.Size() || frontsector->e->XFloor.ffloors.Size()) 
		{
			DoFFloorBlocks(seg,frontsector,backsector, fch1, fch2, ffh1, ffh2, bch1, bch2, bfh1, bfh2);
		}
		
		if (1)//!((frontsector->GetTexture(sector_t::floor)==skyflatnum) && (backsector->GetTexture(sector_t::floor)==skyflatnum)))
		{
			/* bottom texture */
			if (frontsector->GetTexture(sector_t::ceiling)!=skyflatnum || backsector->GetTexture(sector_t::ceiling)!=skyflatnum)
			{
				// the back sector's ceiling obstructs part of this wall				
				if (fch1<bfh1 && fch2<bfh2)
				{
					bfh1=fch1;
					bfh2=fch2;
				}
			}

			if (bfh1>ffh1 || bfh2>ffh2)
			{
				gltexture=FGLTexture::ValidateTexture(seg->sidedef->GetTexture(side_t::bottom));
				if (gltexture) 
				{
					DoTexture(RENDERWALL_BOTTOM,seg,(seg->linedef->flags & ML_DONTPEGBOTTOM)>0,
						realback->GetPlaneTexZ(sector_t::floor),realfront->GetPlaneTexZ(sector_t::floor),
						bfh1,bfh2,ffh1,ffh2,
						frontsector->GetTexture(sector_t::ceiling)==skyflatnum && backsector->GetTexture(sector_t::ceiling)==skyflatnum ?
							realfront->GetPlaneTexZ(sector_t::floor)-realback->GetPlaneTexZ(sector_t::ceiling) : 
							realfront->GetPlaneTexZ(sector_t::floor)-realfront->GetPlaneTexZ(sector_t::ceiling));
				}
				else if ((frontsector->floorplane.a | frontsector->floorplane.b | 
						backsector->floorplane.a | backsector->floorplane.b) && 
						frontsector->GetTexture(sector_t::floor)!=skyflatnum &&
						backsector->GetTexture(sector_t::floor)!=skyflatnum)
				{
					// render it anyway with the sector's floor texture. With a background sky
					// there are ugly holes otherwise and slopes are simply not precise enough
					// to mach in any case.
					gltexture=FGLTexture::ValidateTexture(frontsector->GetTexture(sector_t::floor));
					if (gltexture)
					{
						DoTexture(RENDERWALL_BOTTOM,seg,(seg->linedef->flags & ML_DONTPEGBOTTOM)>0,
							realback->GetPlaneTexZ(sector_t::floor),realfront->GetPlaneTexZ(sector_t::floor),
							bfh1,bfh2,ffh1,ffh2, realfront->GetPlaneTexZ(sector_t::floor)-realfront->GetPlaneTexZ(sector_t::ceiling));
					}
				}
				else if (backsector->GetTexture(sector_t::floor)!=skyflatnum)
				{
					gl_drawinfo->AddLowerMissingTexture(seg, bfh1);
				}
			}
		}
	}
}

//==========================================================================
//
// 
//
//==========================================================================
void GLWall::ProcessLowerMiniseg(seg_t *seg, sector_t * frontsector, sector_t * backsector)
{
	if (frontsector->GetTexture(sector_t::floor)==skyflatnum) return;

	fixed_t ffh = frontsector->GetPlaneTexZ(sector_t::floor); 
	fixed_t bfh = backsector->GetPlaneTexZ(sector_t::floor); 
	if (bfh>ffh)
	{
		this->seg=seg;
		this->sub=NULL;

		vertex_t * v1=seg->v1;
		vertex_t * v2=seg->v2;
		vertexes[0]=v1;
		vertexes[1]=v2;

		glseg.x1= TO_GL(v1->x);
		glseg.y1= TO_GL(v1->y);
		glseg.x2= TO_GL(v2->x);
		glseg.y2= TO_GL(v2->y);
		glseg.fracleft=0;
		glseg.fracright=1;

		flags = (!gl_isBlack(Colormap.FadeColor) || level.flags&LEVEL_HASFADETABLE)? GLWF_FOGGY : 0;

		// can't do fake contrast without a sidedef
		lightlevel = frontsector->lightlevel;
		rellight = 0;

		alpha=1.0f;
		RenderStyle=STYLE_Normal;
		Colormap=frontsector->ColorMap;

		topflat=frontsector->GetTexture(sector_t::ceiling);	// for glowing textures
		bottomflat=frontsector->GetTexture(sector_t::floor);

		zfloor[0] = zfloor[1] = TO_GL(ffh);

		gltexture=FGLTexture::ValidateTexture(frontsector->GetTexture(sector_t::floor));

		if (gltexture) 
		{
			type=RENDERWALL_BOTTOM;
			SetWallCoordinates(seg, bfh, bfh, bfh, ffh, ffh, 0);
			PutWall(false);
		}
	}
}

