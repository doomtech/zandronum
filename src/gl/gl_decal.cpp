/*
** gl_decal.cpp
** OpenGL decal rendering code
**
**---------------------------------------------------------------------------
** Copyright 2003-2005 Christoph Oelckers
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

#include "doomdata.h"
#include "gl/gl_include.h"
#include "a_sharedglobal.h"
#include "gl/gl_renderstruct.h"
#include "gl/gl_texture.h"
#include "gl/gl_functions.h"
#include "gl/gl_intern.h"

struct DecalVertex
{
	float x,y,z;
	float u,v;
};

//==========================================================================
//
//
//
//==========================================================================
void GLWall::DrawDecal(DBaseDecal *actor, seg_t *seg, sector_t *frontSector, sector_t *backSector)
{
	line_t * line=seg->linedef;
	side_t * side=seg->sidedef;
	int i;
	fixed_t zpos;
	int light;
	int rel;
	float a;
	bool flipx, flipy, loadAlpha;
	DecalVertex dv[4];
	FTextureID decalTile;
	

	if (actor->RenderFlags & RF_INVISIBLE) return;
	if (type==RENDERWALL_FFBLOCK && gltexture->tex->bMasked) return;	// No decals on 3D floors with transparent textures.

	//if (actor->sprite != 0xffff)
	{
		decalTile = actor->PicNum;
		flipx = !!(actor->RenderFlags & RF_XFLIP);
		flipy = !!(actor->RenderFlags & RF_YFLIP);
	}
	/*
	else
	{
	decalTile = SpriteFrames[sprites[actor->sprite].spriteframes + actor->frame].lump[0];
	flipx = SpriteFrames[sprites[actor->sprite].spriteframes + actor->frame].flip & 1;
	}
	*/


	FGLTexture * tex=FGLTexture::ValidateTexture(decalTile, false);
	if (!tex) return;
	
	switch (actor->RenderFlags & RF_RELMASK)
	{
	default:
		// No valid decal can have this type. If one is encountered anyway
		// it is in some way invalid.
		return;
		//zpos = actor->z;
		//break;

	case RF_RELUPPER:
		if (type!=RENDERWALL_TOP) return;
		if (line->flags & ML_DONTPEGTOP)
		{
			zpos = actor->Z + frontSector->GetPlaneTexZ(sector_t::ceiling);
		}
		else
		{
			zpos = actor->Z + backSector->GetPlaneTexZ(sector_t::ceiling);
		}
		break;
	case RF_RELLOWER:
		if (type!=RENDERWALL_BOTTOM) return;
		if (line->flags & ML_DONTPEGBOTTOM)
		{
			zpos = actor->Z + frontSector->GetPlaneTexZ(sector_t::ceiling);
		}
		else
		{
			zpos = actor->Z + backSector->GetPlaneTexZ(sector_t::floor);
		}
		break;
	case RF_RELMID:
		if (type==RENDERWALL_TOP || type==RENDERWALL_BOTTOM) return;
		if (line->flags & ML_DONTPEGBOTTOM)
		{
			zpos = actor->Z + frontSector->GetPlaneTexZ(sector_t::floor);
		}
		else
		{
			zpos = actor->Z + frontSector->GetPlaneTexZ(sector_t::ceiling);
		}
	}
	
	if (actor->RenderFlags & RF_FULLBRIGHT)
	{
		light = 255;
		rel = 0;
	}
	else
	{
		light = lightlevel;
		rel = rellight + extralight * gl_weaponlight;
	}
	
	int r = RPART(actor->AlphaColor);
	int g = GPART(actor->AlphaColor);
	int b = BPART(actor->AlphaColor);
	FColormap p = Colormap;
	
	if (glset.nocoloredspritelighting)
	{
		int v = (Colormap.LightColor.r * 77 + Colormap.LightColor.g*143 + Colormap.LightColor.b*35)/255;
		p.LightColor = PalEntry(p.LightColor.a, v, v, v);
	}
	
	float red, green, blue;
	
	if (actor->RenderStyle.Flags & STYLEF_RedIsAlpha)
	{
		loadAlpha = true;
		p.LightColor.a=CM_SHADE;

		gl_GetLightColor(light, rel, &p, &red, &green, &blue);
		
		if (gl_lights && gl_lightcount && !gl_fixedcolormap && gl_light_sprites)
		{
			float result[3];
			fixed_t x, y;
			actor->GetXY(seg->sidedef, x, y);
			gl_GetSpriteLight(NULL, x, y, zpos, seg->Subsector, Colormap.LightColor.a-CM_DESAT0, result);
			red = clamp<float>(result[0]+red, 0, 1.0f);
			green = clamp<float>(result[1]+green, 0, 1.0f);
			blue = clamp<float>(result[2]+blue, 0, 1.0f);
		}

		BYTE R = quickertoint(r * red);
		BYTE G = quickertoint(g * green);
		BYTE B = quickertoint(b * blue);

		gl_ModifyColor(R,G,B, Colormap.LightColor.a);

		red = R/255.f;
		green = G/255.f;
		blue = B/255.f;
	}	
	else
	{
		loadAlpha = false;
		
		red = 1.f;
		green = 1.f;
		blue = 1.f;
	}
	
	
	a = TO_GL(actor->Alpha);
	
	// now clip the decal to the actual polygon
	float decalwidth = tex->TextureWidth(FGLTexture::GLUSE_PATCH)  * TO_GL(actor->ScaleX);
	float decalheight= tex->TextureHeight(FGLTexture::GLUSE_PATCH) * TO_GL(actor->ScaleY);
	float decallefto = tex->GetLeftOffset(FGLTexture::GLUSE_PATCH) * TO_GL(actor->ScaleX);
	float decaltopo  = tex->GetTopOffset(FGLTexture::GLUSE_PATCH)  * TO_GL(actor->ScaleY);

	
	float leftedge = glseg.fracleft * side->TexelLength;
	float linelength = glseg.fracright * side->TexelLength - leftedge;

	// texel index of the decal's left edge
	float decalpixpos = (float)side->TexelLength * actor->LeftDistance / (1<<30) - (flipx? decalwidth-decallefto : decallefto) - leftedge;

	float left,right;
	float lefttex,righttex;

	// decal is off the left edge
	if (decalpixpos < 0)
	{
		left = 0;
		lefttex = -decalpixpos;
	}
	else
	{
		left = decalpixpos;
		lefttex = 0;
	}
	
	// decal is off the right edge
	if (decalpixpos + decalwidth > linelength)
	{
		right = linelength;
		righttex = right - decalpixpos;
	}
	else
	{
		right = decalpixpos + decalwidth;
		righttex = decalwidth;
	}
	if (right<=left) return;	// nothing to draw

	// one texture unit on the wall as vector
	float vx=(glseg.x2-glseg.x1)/linelength;
	float vy=(glseg.y2-glseg.y1)/linelength;
		
	dv[1].x=dv[0].x=glseg.x1+vx*left;
	dv[1].y=dv[0].y=glseg.y1+vy*left;

	dv[3].x=dv[2].x=glseg.x1+vx*right;
	dv[3].y=dv[2].y=glseg.y1+vy*right;
		
	zpos+= FRACUNIT*(flipy? decalheight-decaltopo : decaltopo);

	const PatchTextureInfo * pti=tex->BindPatch(p.LightColor.a, actor->Translation);

	dv[1].z=dv[2].z = TO_GL(zpos);
	dv[0].z=dv[3].z = dv[1].z - decalheight;
	dv[1].v=dv[2].v=pti->GetVT();

	dv[1].u=dv[0].u=pti->GetU(lefttex / TO_GL(actor->ScaleX));
	dv[3].u=dv[2].u=pti->GetU(righttex / TO_GL(actor->ScaleX));
	dv[0].v=dv[3].v=pti->GetVB();


	// now clip to the top plane
	float vzt=(ztop[1]-ztop[0])/linelength;
	float topleft=this->ztop[0]+vzt*left;
	float topright=this->ztop[0]+vzt*right;

	// completely below the wall
	if (topleft<dv[0].z && topright<dv[3].z) 
		return;

	if (topleft<dv[1].z || topright<dv[2].z)
	{
		// decal has to be clipped at the top
		// let texture clamping handle all extreme cases
		dv[1].v=(dv[1].z-topleft)/(dv[1].z-dv[0].z)*dv[0].v;
		dv[2].v=(dv[2].z-topright)/(dv[2].z-dv[3].z)*dv[3].v;
		dv[1].z=topleft;
		dv[2].z=topright;
	}

	// now clip to the bottom plane
	float vzb=(zbottom[1]-zbottom[0])/linelength;
	float bottomleft=this->zbottom[0]+vzb*left;
	float bottomright=this->zbottom[0]+vzb*right;

	// completely above the wall
	if (bottomleft>dv[1].z && bottomright>dv[2].z) 
		return;

	if (bottomleft>dv[0].z || bottomright>dv[3].z)
	{
		// decal has to be clipped at the bottom
		// let texture clamping handle all extreme cases
		dv[0].v=(dv[1].z-bottomleft)/(dv[1].z-dv[0].z)*(dv[0].v-dv[1].v) + dv[1].v;
		dv[3].v=(dv[2].z-bottomright)/(dv[2].z-dv[3].z)*(dv[3].v-dv[2].v) + dv[2].v;
		dv[0].z=bottomleft;
		dv[3].z=bottomright;
	}


	if (flipx)
	{
		float ur=pti->GetUR();
		for(i=0;i<4;i++) dv[i].u=ur-dv[i].u;
	}
	if (flipy)
	{
		float vb=pti->GetVB();
		for(i=0;i<4;i++) dv[i].v=vb-dv[i].v;
	}
	// fog is set once per wall in the calling function and not per decal!

	if (loadAlpha)
	{
		gl.Color4f(red, green, blue, a);
	}
	else
	{
		gl_SetColor(light, rel, &p, a);
	}

	gl_SetRenderStyle(actor->RenderStyle, false, false);

	// If srcalpha is one it looks better with a higher alpha threshold
	if (actor->RenderStyle.SrcAlpha == STYLEALPHA_One) gl.AlphaFunc(GL_GEQUAL, 0.5f);
	else gl.AlphaFunc(GL_GREATER, 0.f);

	gl_ApplyShader();
	gl.Begin(GL_TRIANGLE_FAN);
	for(i=0;i<4;i++)
	{
		gl.TexCoord2f(dv[i].u,dv[i].v);
		gl.Vertex3f(dv[i].x,dv[i].z,dv[i].y);
	}
	gl.End();
	rendered_decals++;
}

//==========================================================================
//
//
//
//==========================================================================
void GLWall::DoDrawDecals(DBaseDecal * decal, seg_t * seg)
{
	while (decal)
	{
		// the sectors are only used for their texture origin coordinates
		// so we don't need the fake sectors for deep water etc.
		// As this is a completely split wall fragment no further splits are
		// necessary for the decal.
		sector_t * frontsector;

		// for 3d-floor segments use the model sector as reference
		if ((decal->RenderFlags&RF_CLIPMASK)==RF_CLIPMID) frontsector=decal->Sector;
		else frontsector=seg->frontsector;

		DrawDecal(decal,seg,frontsector,seg->backsector);
		decal = decal->WallNext;
	}
}


