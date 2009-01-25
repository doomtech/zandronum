/*
** gl_glow.cpp
** Glowing flats like Doomsday
** (consider this deprecated due to problems with slope handling)
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

#include "gl/gl_include.h"
#include "w_wad.h"
#include "sc_man.h"

#include "gl/gl_texture.h"
#include "gl/gl_glow.h"
#include "gl/gl_functions.h"
#include "gl/gl_intern.h"
#include "gl/gl_renderstruct.h"

CVAR(Int, wallglowheight, 128, CVAR_ARCHIVE)
CVAR(Float, wallglowfactor, 0.6f, CVAR_ARCHIVE)

//===========================================================================
// 
//	Reads glow definitions from GLDEFS
//
//===========================================================================
void gl_InitGlow(FScanner &sc)
{
	sc.MustGetStringName("{");
	while (!sc.CheckString("}"))
	{
		sc.MustGetString();
		if (sc.Compare("FLATS"))
		{
			sc.MustGetStringName("{");
			while (!sc.CheckString("}"))
			{
				sc.MustGetString();
				FTextureID flump=TexMan.CheckForTexture(sc.String, FTexture::TEX_Flat,FTextureManager::TEXMAN_TryAny);
				FTexture *tex = TexMan[flump];
				if (tex) tex->gl_info.bGlowing = true;
			}
		}

		if (sc.Compare("WALLS"))
		{
			sc.MustGetStringName("{");
			while (!sc.CheckString("}"))
			{
				sc.MustGetString();
				FTextureID flump=TexMan.CheckForTexture(sc.String, FTexture::TEX_Wall,FTextureManager::TEXMAN_TryAny);
				FTexture *tex = TexMan[flump];
				if (tex) tex->gl_info.bGlowing = true;
			}
		}
	}
}


//===========================================================================
// 
//	Gets the average color of a texture for use as a glow color
//
//===========================================================================
void gl_GetGlowColor(FTextureID texno, float * data)
{
	FTexture *tex = TexMan[texno];
	if (tex && tex->gl_info.bGlowing && tex->gl_info.GlowColor == 0)
	{
		FGLTexture * gltex = FGLTexture::ValidateTexture(tex);
		if (gltex)
		{
			int w, h;
			unsigned char * buffer = gltex->CreateTexBuffer(FGLTexture::GLUSE_TEXTURE, CM_DEFAULT, 0, w, h);

			if (buffer)
			{
				tex->gl_info.GlowColor = averageColor((DWORD *) buffer, w*h, true);
				delete buffer;
			}
		}
		if (tex->gl_info.GlowColor == 0) tex->gl_info.bGlowing = false;
	}

	data[0]=tex->gl_info.GlowColor.r/255.0f;
	data[1]=tex->gl_info.GlowColor.g/255.0f;
	data[2]=tex->gl_info.GlowColor.b/255.0f;
}


//==========================================================================
//
// Does this texture emit a glow?
//
//==========================================================================
bool gl_isGlowingTexture(FTextureID texno)
{
	FTexture *tex = TexMan[texno];
	if (tex) return tex->IsGlowing();
	else return false;
}

//==========================================================================
//
// Checks whether a wall should glow
//
//==========================================================================
void gl_CheckGlowing(GLWall * wall)
{
	if ((gl_isGlowingTexture(wall->topflat) || gl_isGlowingTexture(wall->bottomflat)) && 
		!gl_isFullbright(wall->Colormap.LightColor, wall->lightlevel))
	{
		wall->flags |= GLWall::GLWF_GLOW;
	}
}

//==========================================================================
//
// Checks whether a sprite should be affected by a glow
//
//==========================================================================
int gl_CheckSpriteGlow(FTextureID floorpic, int lightlevel, fixed_t floordiff)
{
	if (gl_isGlowingTexture(floorpic))
	{
		if (floordiff<wallglowheight*FRACUNIT)
		{
			int maxlight=(255+lightlevel)>>1;
			fixed_t lightfrac = floordiff / wallglowheight;
			if (lightfrac<0) lightfrac=0;
			lightlevel= (lightfrac*lightlevel + maxlight*(FRACUNIT-lightfrac))>>FRACBITS;
		}
	}
	return lightlevel;
}

//==========================================================================
//
// Renders a polygon with glow from floor or ceiling
// this only works for non-sloped walls!
//
//==========================================================================
void GLWall::RenderGlowingPoly(int textured, ADynamicLight * light)
{
	float renderbottom=zbottom[0];
	float rendertop;
	float glowheight=wallglowheight;
	float glowbottomtop=zfloor[0]+glowheight;
	float glowtopbottom=zceil[0]-glowheight;
	bool glowbot=zbottom[0]<glowbottomtop && gl_isGlowingTexture(bottomflat);
	bool glowtop=ztop[0]>glowtopbottom && gl_isGlowingTexture(topflat);
	float color_o[4];
	float color_b[4];
	float color_top[4];
	float glowc_b[3];
	float glowc_t[3];
	int i;


	texcoord tcs[4];

	if (!light)
	{
		tcs[0]=lolft;
		tcs[1]=uplft;
		tcs[2]=uprgt;
		tcs[3]=lorgt;

		if (textured&2)
		{
			gl_GetLightColor(lightlevel, rellight + (extralight*gl_weaponlight), &Colormap,
							 &color_o[0],&color_o[1],&color_o[2]);
			color_o[3]=alpha;
			memcpy(color_b,color_o,sizeof(color_b));
			if (glowbot)
			{
				gl_GetGlowColor(bottomflat, glowc_b);
				for (i=0;i<3;i++) color_b[i]+=glowc_b[i]*wallglowfactor*(glowbottomtop-renderbottom)/glowheight;
			}
			if (glowtop)
			{
				gl_GetGlowColor(topflat, glowc_t);
				if (glowtopbottom<renderbottom)
					for(i=0;i<3;i++) color_b[i]+=glowc_t[i]*wallglowfactor*(renderbottom-glowtopbottom)/glowheight;
			}
			for(i=0;i<3;i++) if (color_b[i]>1.0f) color_b[i]=1.0f;
		}
	}
	else
	{
		textured&=1;
		if (!PrepareLight(tcs, light)) return;
	}


	float polyh=ztop[0]-zbottom[0];
	float fact=polyh? (tcs[1].v-tcs[0].v)/polyh:0;

	// The wall is being split into 1-3 gradients for upper glow, lower glow and center part (either nothing or overlap)
	while (renderbottom<ztop[0])
	{
		rendertop=ztop[0];
		if (glowbot && glowbottomtop>renderbottom && glowbottomtop<rendertop) rendertop=glowbottomtop;
		if (glowtop && glowtopbottom>renderbottom && glowtopbottom<rendertop) rendertop=glowtopbottom;

		if (textured&2)
		{
			memcpy(color_top,color_o,sizeof(color_top));
			if (glowbot && rendertop<glowbottomtop)
				for (i=0;i<3;i++) color_top[i]+=glowc_b[i]*wallglowfactor*(glowbottomtop-rendertop)/glowheight;

			if (glowtop && glowtopbottom<rendertop)
				for(i=0;i<3;i++) color_top[i]+=glowc_t[i]*wallglowfactor*(rendertop-glowtopbottom)/glowheight;
			
			for(i=0;i<3;i++) if (color_top[i]>1.0f) color_top[i]=1.0f;

		}

		gl.Begin(GL_TRIANGLE_FAN);


		// lower left corner
		if (textured&2) gl.Color4fv(color_b);
		if (textured&1) gl.TexCoord2f(tcs[1].u,fact*(renderbottom-ztop[0])+tcs[1].v);
		gl.Vertex3f(glseg.x1,renderbottom,glseg.y1);

		// upper left corner
		if (textured&2) gl.Color4fv(color_top);
		if (textured&1) gl.TexCoord2f(tcs[1].u,fact*(rendertop-ztop[0])+tcs[1].v);
		gl.Vertex3f(glseg.x1,rendertop,glseg.y1);

		// upper right corner
		if (textured&1) gl.TexCoord2f(tcs[2].u,fact*(rendertop-ztop[1])+tcs[2].v);
		gl.Vertex3f(glseg.x2,rendertop,glseg.y2);

		// lower right corner
		if (textured&2) gl.Color4fv(color_b);
		if (textured&1) gl.TexCoord2f(tcs[2].u,fact*(renderbottom-ztop[1])+tcs[2].v);
		gl.Vertex3f(glseg.x2,renderbottom,glseg.y2);
		gl.End();

		renderbottom=rendertop;
		memcpy(color_b,color_top,sizeof(color_b));
	}
}


