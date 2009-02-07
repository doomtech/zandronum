/*
** gl_sky.cpp
** Sky preparation code.
**
**---------------------------------------------------------------------------
** Copyright 2002-2005 Christoph Oelckers
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
#include "a_sharedglobal.h"
#include "g_level.h"
#include "r_sky.h"
#include "gl/gl_renderstruct.h"
#include "gl/gl_functions.h"
#include "gl/gl_data.h"
#include "gl/gl_portal.h"
#include "gl/gl_texture.h"

EXTERN_CVAR(Bool, gl_notexturefill);
CVAR(Bool,gl_noskyboxes, false, 0)
extern int skyfog;
extern float gl_sky1pos, gl_sky2pos;

enum
{
	NoSkyDraw = 89
};


//==========================================================================
//
//  Calculate mirrorplane
//
//==========================================================================
void GLWall::MirrorPlane(secplane_t * plane, bool ceiling)
{
	if (!(gl.flags&RFL_NOSTENCIL))
	{
		if (ceiling && viewz >= plane->ZatPoint(viewx, viewy)) return;
		if (!ceiling && viewz <= plane->ZatPoint(viewx, viewy)) return;
		type=RENDERWALL_PLANEMIRROR;
		planemirror=plane;
		PutWall(0);
	}
}

//==========================================================================
//
//  Calculate sky texture
//
//==========================================================================
void GLWall::SkyTexture(int sky1,ASkyViewpoint * skyboxx, bool ceiling)
{
	// JUSTHIT is used as an indicator that a skybox is in use.
	// This is to avoid recursion
	if (!gl_noskyboxes && !(gl.flags&RFL_NOSTENCIL) && skyboxx && viewactor!=skyboxx && !(skyboxx->flags&MF_JUSTHIT))
	{
		if (!skyboxx->Mate) 
		{
			type=RENDERWALL_SKYBOX;
			skybox=skyboxx;
		}
		else 
		{
			static GLSectorStackInfo stackinfo;
			if (ceiling && GLPortal::inlowerstack) return;
			if (!ceiling && GLPortal::inupperstack) return;
			type=RENDERWALL_SECTORSTACK;
			stackinfo.deltax = skyboxx->Mate->x - skyboxx->x;
			stackinfo.deltay = skyboxx->Mate->y - skyboxx->y;
			stackinfo.deltaz = 0;
			stackinfo.isupper= ceiling;
			stack=&stackinfo;
		}
	}
	else
	{
		if (skyboxx && skyboxx->Mate) return;

		// VC's optimizer totally screws up if this is made local...
		static GLSkyInfo skyinfo;

		memset(&skyinfo, 0, sizeof(skyinfo));
		if ((sky1 & PL_SKYFLAT) && (sky1 & (PL_SKYFLAT-1)) && !(gl.flags&RFL_NOSTENCIL))
		{
			const line_t *l = &lines[(sky1&(PL_SKYFLAT-1))-1];
			const side_t *s = &sides[l->sidenum[0]];
			int pos;
			
			if (level.flags & LEVEL_SWAPSKIES && s->GetTexture(side_t::bottom).isValid())
			{
				pos = side_t::bottom;
			}
			else
			{
				pos = side_t::top;
			}

			FTextureID texno = s->GetTexture(pos);
			skyinfo.texture[0] = FGLTexture::ValidateTexture(texno);
			if (!skyinfo.texture[0] || skyinfo.texture[0]->tex->UseType == FTexture::TEX_Null) goto normalsky;
			skyinfo.skytexno1 = texno;
			skyinfo.x_offset[0] = ANGLE_TO_FLOAT(s->GetTextureXOffset(pos));
			skyinfo.y_offset = TO_GL(s->GetTextureYOffset(pos));
			skyinfo.mirrored = !l->args[2];
		}
		else
		{
		normalsky:
			if (level.flags&LEVEL_DOUBLESKY)
			{
				skyinfo.texture[1]=FGLTexture::ValidateTexture(sky1texture);
				if (!skyinfo.texture[1]) return;
				skyinfo.x_offset[1] = gl_sky1pos;
				skyinfo.doublesky = true;
			}
			
			if ((level.flags&LEVEL_SWAPSKIES || (sky1==PL_SKYFLAT && !(gl.flags&RFL_NOSTENCIL)) || (level.flags&LEVEL_DOUBLESKY)) &&
				sky2texture!=sky1texture)	// If both skies are equal use the scroll offset of the first!
			{
				skyinfo.texture[0]=FGLTexture::ValidateTexture(sky2texture);
				skyinfo.skytexno1=sky2texture;
				skyinfo.x_offset[0] = gl_sky2pos;
			}
			else
			{
				skyinfo.texture[0]=FGLTexture::ValidateTexture(sky1texture);
				skyinfo.skytexno1=sky1texture;
				skyinfo.x_offset[0] = gl_sky1pos;
			}
			if (!skyinfo.texture[0]) return;

		}
		if (skyfog>0) 
		{
			skyinfo.fadecolor=Colormap.FadeColor;
			skyinfo.fadecolor.a=0;
		}
		else skyinfo.fadecolor=0;

		type=RENDERWALL_SKY;
		sky = &skyinfo;
	}
	PutWall(0);
}


//==========================================================================
//
//  Skies on one sided walls
//
//==========================================================================
void GLWall::SkyNormal(sector_t * fs,vertex_t * v1,vertex_t * v2)
{
	bool ceilingsky = fs->GetTexture(sector_t::ceiling)==skyflatnum || (fs->CeilingSkyBox && fs->CeilingSkyBox->bAlways);
	if (ceilingsky || fs->ceiling_reflect)
	{
		ztop[0]=ztop[1]=32768.0f;
		zbottom[0]=zceil[0];
		zbottom[1]=zceil[1];
		if (ceilingsky)	SkyTexture(fs->sky,fs->CeilingSkyBox, true);
		else MirrorPlane(&fs->ceilingplane, true);
	}
	bool floorsky = fs->GetTexture(sector_t::floor)==skyflatnum || (fs->FloorSkyBox && fs->FloorSkyBox->bAlways);
	if (floorsky || fs->floor_reflect)
	{
		ztop[0]=zfloor[0];
		ztop[1]=zfloor[1];
		zbottom[0]=zbottom[1]=-32768.0f;
		if (floorsky) SkyTexture(fs->sky,fs->FloorSkyBox, false);
		else MirrorPlane(&fs->floorplane, false);
	}
}


//==========================================================================
//
//  Upper Skies on two sided walls
//
//==========================================================================
void GLWall::SkyTop(seg_t * seg,sector_t * fs,sector_t * bs,vertex_t * v1,vertex_t * v2)
{
	if (fs->GetTexture(sector_t::ceiling)==skyflatnum)
	{
		if ((bs->special&0xff) == NoSkyDraw) return;
		if (bs->GetTexture(sector_t::ceiling)==skyflatnum) 
		{
			// if the back sector is closed the sky must be drawn!
			if (bs->ceilingplane.ZatPoint(v1) > bs->floorplane.ZatPoint(v1) ||
				bs->ceilingplane.ZatPoint(v2) > bs->floorplane.ZatPoint(v2) || bs->transdoor)
					return;

			// one more check for some ugly transparent door hacks
			if (bs->floorplane.a==0 && bs->floorplane.b==0 && fs->floorplane.a==0 && fs->floorplane.b==0 &&
				bs->GetPlaneTexZ(sector_t::floor)==fs->GetPlaneTexZ(sector_t::floor)+FRACUNIT)
			{
				FTexture * tex = TexMan(seg->sidedef->GetTexture(side_t::bottom));
				if (!tex || tex->UseType==FTexture::TEX_Null) return;
			}
		}

		ztop[0]=ztop[1]=32768.0f;

		FTexture * tex = TexMan(seg->sidedef->GetTexture(side_t::top));
		if (/*tex && tex->UseType!=FTexture::TEX_Null &&*/ bs->GetTexture(sector_t::ceiling) != skyflatnum)

		{
			zbottom[0]=zceil[0];
			zbottom[1]=zceil[1];
		}
		else
		{
			zbottom[0]=TO_GL(bs->ceilingplane.ZatPoint(v1));
			zbottom[1]=TO_GL(bs->ceilingplane.ZatPoint(v2));
			flags|=GLWF_SKYHACK;	// mid textures on such lines need special treatment!
		}

		SkyTexture(fs->sky,fs->CeilingSkyBox, true);
	}
	else 
	{
		bool ceilingsky = (fs->CeilingSkyBox && fs->CeilingSkyBox->bAlways && fs->CeilingSkyBox!=bs->CeilingSkyBox); 
		if (ceilingsky || fs->ceiling_reflect)
		{
			// stacked sectors
			fixed_t fsc1=fs->ceilingplane.ZatPoint(v1);
			fixed_t fsc2=fs->ceilingplane.ZatPoint(v2);

			ztop[0]=ztop[1]=32768.0f;
			zbottom[0]=TO_GL(fsc1);
			zbottom[1]=TO_GL(fsc2);
			if (ceilingsky)	SkyTexture(fs->sky,fs->CeilingSkyBox, true);
			else MirrorPlane(&fs->ceilingplane, true);
		}
	}
}


//==========================================================================
//
//  Lower Skies on two sided walls
//
//==========================================================================
void GLWall::SkyBottom(seg_t * seg,sector_t * fs,sector_t * bs,vertex_t * v1,vertex_t * v2)
{
	if (fs->GetTexture(sector_t::floor)==skyflatnum)
	{
		if ((bs->special&0xff) == NoSkyDraw) return;
		FTexture * tex = TexMan(seg->sidedef->GetTexture(side_t::bottom));
		
		// For lower skies the normal logic only applies to walls with no lower texture!
		if (tex->UseType==FTexture::TEX_Null)
		{
			if (bs->GetTexture(sector_t::floor)==skyflatnum)
			{
				// if the back sector is closed the sky must be drawn!
				if (bs->ceilingplane.ZatPoint(v1) > bs->floorplane.ZatPoint(v1) ||
					bs->ceilingplane.ZatPoint(v2) > bs->floorplane.ZatPoint(v2))
						return;

			}
			else
			{
				// Special hack for Vrack2b
				if (bs->floorplane.ZatPoint(viewx, viewy) > viewz) return;
			}
		}
		zbottom[0]=zbottom[1]=-32768.0f;

		if ((tex && tex->UseType!=FTexture::TEX_Null) || bs->GetTexture(sector_t::floor)!=skyflatnum)
		{
			ztop[0]=zfloor[0];
			ztop[1]=zfloor[1];
		}
		else
		{
			ztop[0]=TO_GL(bs->floorplane.ZatPoint(v1));
			ztop[1]=TO_GL(bs->floorplane.ZatPoint(v2));
			flags|=GLWF_SKYHACK;	// mid textures on such lines need special treatment!
		}

		SkyTexture(fs->sky,fs->FloorSkyBox, false);
	}
	else 
	{
		bool floorsky = (fs->FloorSkyBox && fs->FloorSkyBox->bAlways && fs->FloorSkyBox!=bs->FloorSkyBox);
		if (floorsky || fs->floor_reflect)
		{
			// stacked sectors
			fixed_t fsc1=fs->floorplane.ZatPoint(v1);
			fixed_t fsc2=fs->floorplane.ZatPoint(v2);

			zbottom[0]=zbottom[1]=-32768.0f;
			ztop[0]=TO_GL(fsc1);
			ztop[1]=TO_GL(fsc2);

			if (floorsky) SkyTexture(fs->sky,fs->FloorSkyBox, false);
			else MirrorPlane(&fs->floorplane, false);
		}
	}
}


