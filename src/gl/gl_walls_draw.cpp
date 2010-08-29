/*
** gl_walls_draw.cpp
** Wall rendering
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
#include "r_blend.h"
#include "gl/gl_struct.h"
#include "gl/gl_renderstruct.h"
#include "gl/gl_portal.h"
#include "gl/gl_lights.h"
#include "gl/gl_glow.h"
#include "gl/gl_data.h"
#include "gl/gl_texture.h"
#include "gl/gl_basic.h"
#include "gl/gl_functions.h"
#include "gl/gl_shader.h"

EXTERN_CVAR(Bool, gl_seamless)

//==========================================================================
//
// Sets up the texture coordinates for one light to be rendered
//
//==========================================================================
bool GLWall::PrepareLight(texcoord * tcs, ADynamicLight * light)
{
	float vtx[]={glseg.x1,zbottom[0],glseg.y1, glseg.x1,ztop[0],glseg.y1, glseg.x2,ztop[1],glseg.y2, glseg.x2,zbottom[1],glseg.y2};
	Plane p;
	Vector nearPt, up, right;
	float scale;

	p.Init(vtx,4);

	if (!p.ValidNormal()) 
	{
		return false;
	}

	if (!gl_SetupLight(p, light, nearPt, up, right, scale, Colormap.LightColor.a, true, !!(flags&GLWF_FOGGY))) 
	{
		return false;
	}

	Vector t1;
	int outcnt[4]={0,0,0,0};

	for(int i=0;i<4;i++)
	{
		t1.Set(&vtx[i*3]);
		Vector nearToVert = t1 - nearPt;
		tcs[i].u = (nearToVert.Dot(right) * scale) + 0.5f;
		tcs[i].v = (nearToVert.Dot(up) * scale) + 0.5f;

		// quick check whether the light touches this polygon
		if (tcs[i].u<0) outcnt[0]++;
		if (tcs[i].u>1) outcnt[1]++;
		if (tcs[i].v<0) outcnt[2]++;
		if (tcs[i].v>1) outcnt[3]++;

	}
	// The light doesn't touch this polygon
	if (outcnt[0]==4 || outcnt[1]==4 || outcnt[2]==4 || outcnt[3]==4) return false;

	draw_dlight++;
	return true;
}

//==========================================================================
//
// General purpose wall rendering function
// with the exception of walls lit by glowing flats 
// everything goes through here
//
// Tests have shown that precalculating this data
// doesn't give any noticable performance improvements
//
//==========================================================================

void GLWall::RenderWall(int textured, float * color2, ADynamicLight * light)
{
	texcoord tcs[4];
	bool glowing;

	if (!light)
	{
		tcs[0]=lolft;
		tcs[1]=uplft;
		tcs[2]=uprgt;
		tcs[3]=lorgt;
		glowing = !!(flags&GLWF_GLOW) && (textured & 2);
	}
	else
	{
		if (!PrepareLight(tcs, light)) return;
		glowing = false;
	}

	if (!(flags & GLWF_NOSHADER)) gl_ApplyShader();
	else gl_DisableShader();

	if (glowing)
	{
		// must be done after gl_ApplyShader!
		gl_SetGlowParams(topglowcolor, topglowheight, bottomglowcolor, bottomglowheight);
	}

	// the rest of the code is identical for textured rendering and lights

	gl.Begin(GL_TRIANGLE_FAN);


	// lower left corner
	if (glowing) gl_SetGlowPosition(zceil[0] - zbottom[0], zbottom[0] - zfloor[0]);
	if (textured&1) gl.TexCoord2f(tcs[0].u,tcs[0].v);
	gl.Vertex3f(glseg.x1,zbottom[0],glseg.y1);

	if (gl_seamless && glseg.fracleft==0) gl_SplitLeftEdge(this, tcs, glowing);

	// upper left corner
	if (glowing) gl_SetGlowPosition(zceil[0] - ztop[0], ztop[0] - zfloor[0]);
	if (textured&1) gl.TexCoord2f(tcs[1].u,tcs[1].v);
	gl.Vertex3f(glseg.x1,ztop[0],glseg.y1);

	// color for right side
	if (color2) gl.Color4fv(color2);

	// upper right corner
	if (glowing) gl_SetGlowPosition(zceil[1] - ztop[1], ztop[1] - zfloor[1]);
	if (textured&1) gl.TexCoord2f(tcs[2].u,tcs[2].v);
	gl.Vertex3f(glseg.x2,ztop[1],glseg.y2);

	if (gl_seamless && glseg.fracright==1) gl_SplitRightEdge(this, tcs, glowing);

	// lower right corner
	if (glowing) gl_SetGlowPosition(zceil[1] - zbottom[1], zbottom[1] - zfloor[1]);
	if (textured&1) gl.TexCoord2f(tcs[3].u,tcs[3].v); 
	gl.Vertex3f(glseg.x2,zbottom[1],glseg.y2);

	gl.End();

	vertexcount+=4;

}

//==========================================================================
//
// 
//
//==========================================================================

void GLWall::RenderFogBoundary()
{
	// [BB] Take care of gl_fogmode and DF2_FORCE_GL_DEFAULTS.
	OVERRIDE_FOGMODE_IF_NECESSARY

	if (gl_fogmode)
	{
		float fogdensity=gl_GetFogDensity(lightlevel, Colormap.FadeColor);

		float xcamera=TO_GL(viewx);
		float ycamera=TO_GL(viewy);

		float dist1=Dist2(xcamera,ycamera, glseg.x1,glseg.y1);
		float dist2=Dist2(xcamera,ycamera, glseg.x2,glseg.y2);


		// these values were determined by trial and error and are scale dependent!
		float fogd1=(0.95f-exp(-fogdensity*dist1/62500.f)) * 1.05f;
		float fogd2=(0.95f-exp(-fogdensity*dist2/62500.f)) * 1.05f;

		gl_ModifyColor(Colormap.FadeColor.r, Colormap.FadeColor.g, Colormap.FadeColor.b, Colormap.LightColor.a);
		float fc[4]={Colormap.FadeColor.r/255.0f,Colormap.FadeColor.g/255.0f,Colormap.FadeColor.b/255.0f,fogd2};

		gl_EnableTexture(false);
		gl_EnableFog(false);
		gl.AlphaFunc(GL_GREATER,0);
		gl.DepthFunc(GL_LEQUAL);
		gl.Color4f(fc[0],fc[1],fc[2], fogd1);

		flags &= ~GLWF_GLOW;
		RenderWall(0,fc);

		gl.DepthFunc(GL_LESS);
		gl_EnableFog(true);
		gl.AlphaFunc(GL_GEQUAL,0.5f);
		gl_EnableTexture(true);
	}
}


//==========================================================================
//
// 
//
//==========================================================================
void GLWall::RenderMirrorSurface()
{
	if (mirrortexture == NULL) return;

	// Use sphere mapping for this
	gl.Enable(GL_TEXTURE_GEN_T);
	gl.Enable(GL_TEXTURE_GEN_S);
	gl.TexGeni(GL_S,GL_TEXTURE_GEN_MODE,GL_SPHERE_MAP);
	gl.TexGeni(GL_T,GL_TEXTURE_GEN_MODE,GL_SPHERE_MAP);

	gl_SetColor(lightlevel, 0, &Colormap ,0.1f);
	gl.BlendFunc(GL_SRC_ALPHA,GL_ONE);
	gl.AlphaFunc(GL_GREATER,0);
	gl.DepthFunc(GL_LEQUAL);
	gl_SetFog(lightlevel, extralight*gl_weaponlight, &Colormap, true);

	FGLTexture * pat=FGLTexture::ValidateTexture(mirrortexture);
	pat->BindPatch(Colormap.LightColor.a, 0);

	flags &= ~GLWF_GLOW;
	flags |= GLWF_NOSHADER;
	RenderWall(0,NULL);

	gl.Disable(GL_TEXTURE_GEN_T);
	gl.Disable(GL_TEXTURE_GEN_S);

	// Restore the defaults for the translucent pass
	gl.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	gl.AlphaFunc(GL_GEQUAL,0.5f);
	gl.DepthFunc(GL_LESS);

	// This is drawn in the translucent pass which is done after the decal pass
	// As a result the decals have to be drawn here.
	if (seg->sidedef->AttachedDecals)
	{
		gl.Enable(GL_POLYGON_OFFSET_FILL);
		gl.PolygonOffset(-1.0f, -128.0f);
		gl.DepthMask(false);
		DoDrawDecals(seg->sidedef->AttachedDecals, seg);
		gl.DepthMask(true);
		gl.PolygonOffset(0.0f, 0.0f);
		gl.Disable(GL_POLYGON_OFFSET_FILL);
		gl.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
}


//==========================================================================
//
// 
//
//==========================================================================

void GLWall::RenderTranslucentWall()
{
	bool transparent = gltexture? gltexture->GetTransparent() : false;
	
	// currently the only modes possible are solid, additive or translucent
	// and until that changes I won't fix this code for the new blending modes!
	bool isadditive = RenderStyle == STYLE_Add;

	if (!transparent) gl.AlphaFunc(GL_GEQUAL,0.5f*fabs(alpha));
	else gl.Disable(GL_ALPHA_TEST);
	if (isadditive) gl.BlendFunc(GL_SRC_ALPHA,GL_ONE);

	int extra;
	if (gltexture) 
	{
		if (flags&GLWF_FOGGY) gl_EnableBrightmap(false);
		gl_EnableGlow(!!(flags & GLWF_GLOW));
		gltexture->Bind(Colormap.LightColor.a, flags, 0);
		extra = (extralight * gl_weaponlight);
	}
	else 
	{
		gl_EnableTexture(false);
		extra = 0;
	}

	gl_SetColor(lightlevel, extra, &Colormap, fabsf(alpha));
	if (type!=RENDERWALL_M2SNF) gl_SetFog(lightlevel, extra, &Colormap, isadditive);
	else gl_SetFog(255, 0, NULL, false);

	RenderWall(1,NULL);

	// restore default settings
	if (isadditive) gl.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	if (!transparent) gl.Enable(GL_ALPHA_TEST);

	if (!gltexture)	
	{
		gl_EnableTexture(true);
	}
	gl_EnableBrightmap(true);
	gl_EnableGlow(false);
}

//==========================================================================
//
// 
//
//==========================================================================
void GLWall::Draw(int pass)
{
	FLightNode * node;
	int rel;

#ifdef _MSC_VER
#ifdef _DEBUG
	if (seg->linedef-lines==879)
		__asm nop
#endif
#endif


	// This allows mid textures to be drawn on lines that might overlap a sky wall
	if ((flags&GLWF_SKYHACK && type==RENDERWALL_M2S) || type == RENDERWALL_COLORLAYER)
	{
		if (pass != GLPASS_DECALS)
		{
			gl.Enable(GL_POLYGON_OFFSET_FILL);
			gl.PolygonOffset(-1.0f, -128.0f);
		}
	}

	switch (pass)
	{
	case GLPASS_BASE:			// Base pass for non-masked polygons (all opaque geometry)
	case GLPASS_BASE_MASKED:	// Base pass for masked polygons (2sided mid-textures and transparent 3D floors)
	case GLPASS_PLAIN:			// Single-pass rendering

		rel = rellight + (extralight * gl_weaponlight);
		gl_SetColor(lightlevel, rel, &Colormap,1.0f);
		if (!(flags&GLWF_FOGGY) || pass == GLPASS_PLAIN) 
		{
			if (type!=RENDERWALL_M2SNF) gl_SetFog(lightlevel, rel, &Colormap, false);
			else gl_SetFog(255, 0, NULL, false);
		}
		gl_EnableGlow(!!(flags & GLWF_GLOW));

		// fall through
	case GLPASS_TEXTURE:		// modulated texture
		if (pass != GLPASS_BASE)
		{
			gltexture->Bind(Colormap.LightColor.a, flags, 0);
		}
		RenderWall((pass!=GLPASS_BASE) + 2*(pass!=GLPASS_TEXTURE), NULL);
		gl_EnableGlow(false);
		break;

	case GLPASS_LIGHT:
	case GLPASS_LIGHT_ADDITIVE:
		// black fog is diminishing light and should affect lights less than the rest!
		if (!(flags&GLWF_FOGGY)) gl_SetFog((255+lightlevel)>>1, 0, NULL, false);
		else gl_SetFog(lightlevel, 0, &Colormap, true);	

		if (!seg->bPolySeg)
		{
			// Iterate through all dynamic lights which touch this wall and render them
			if (seg->sidedef)
			{
				node = seg->sidedef->lighthead[pass==GLPASS_LIGHT_ADDITIVE];
			}
			else node = NULL;
		}
		else if (sub)
		{
			// To avoid constant rechecking for polyobjects use the subsector's lightlist instead
			node = sub->lighthead[pass==GLPASS_LIGHT_ADDITIVE];
		}
		else node = NULL;
		while (node)
		{
			if (!(node->lightsource->flags2&MF2_DORMANT))
			{
				iter_dlight++;
				RenderWall(1, NULL, node->lightsource);
			}
			node = node->nextLight;
		}
		break;

	case GLPASS_DECALS:
	case GLPASS_DECALS_NOFOG:
		if (seg->sidedef && seg->sidedef->AttachedDecals)
		{
			if (pass==GLPASS_DECALS) 
			{
				gl_SetFog(lightlevel, rellight + extralight*gl_weaponlight, &Colormap, false);
			}
			DoDrawDecals(seg->sidedef->AttachedDecals, seg);
		}
		break;

	case GLPASS_TRANSLUCENT:
		switch (type)
		{
		case RENDERWALL_MIRRORSURFACE:
			RenderMirrorSurface();
			break;

		case RENDERWALL_FOGBOUNDARY:
			RenderFogBoundary();
			break;

		default:
			RenderTranslucentWall();
			break;
		}
	}

	if ((flags&GLWF_SKYHACK && type==RENDERWALL_M2S) || type == RENDERWALL_COLORLAYER)
	{
		if (pass!=GLPASS_DECALS)
		{
			gl.Disable(GL_POLYGON_OFFSET_FILL);
			gl.PolygonOffset(0, 0);
		}
	}
}


