/*
** gl_flat.cpp
** Flat rendering
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
#include "a_sharedglobal.h"
#include "gl/gl_renderstruct.h"
#include "gl/gl_functions.h"
#include "gl/gl_lights.h"
#include "gl/gl_glow.h"
#include "gl/gl_data.h"
#include "gl/gl_texture.h"
#include "gl/gl_intern.h"
#include "gl/gl_basic.h"
#include "gl/gl_shader.h"
#include "r_sky.h"
#include "g_level.h"
#include "doomstat.h"


EXTERN_CVAR (Bool, gl_lights_checkside);

//==========================================================================
//
// Sets the texture matrix according to the plane's texture positioning
// information
//
//==========================================================================

void gl_SetPlaneTextureRotation(const GLSectorPlane * secplane, FGLTexture * gltexture)
{
	float uoffs=TO_GL(secplane->xoffs)/gltexture->TextureWidth(FGLTexture::GLUSE_TEXTURE);
	float voffs=TO_GL(secplane->yoffs)/gltexture->TextureHeight(FGLTexture::GLUSE_TEXTURE);

	float xscale1=TO_GL(secplane->xscale);
	float yscale1=TO_GL(secplane->yscale);

	float xscale2=64.f/gltexture->TextureWidth(FGLTexture::GLUSE_TEXTURE);
	float yscale2=64.f/gltexture->TextureHeight(FGLTexture::GLUSE_TEXTURE);

	float angle=-ANGLE_TO_FLOAT(secplane->angle);

	gl.MatrixMode(GL_TEXTURE);

	gl.PushMatrix();
	gl.Scalef(xscale1 ,yscale1,1.0f);
	gl.Translatef(uoffs,voffs,0.0f);
	gl.Scalef(xscale2 ,yscale2,1.0f);
	gl.Rotatef(angle,0.0f,0.0f,1.0f);
}


//==========================================================================
//
// Flats 
//
//==========================================================================

void GLFlat::DrawSubsectorLights(subsector_t * sub, int pass)
{
	Plane p;
	Vector nearPt, up, right;
	float scale;
	int k;

	FLightNode * node = sub->lighthead[pass==GLPASS_LIGHT_ADDITIVE];
	gl_DisableShader();
	while (node)
	{
		ADynamicLight * light = node->lightsource;
		
		if (light->flags2&MF2_DORMANT)
		{
			node=node->nextLight;
			continue;
		}

		// we must do the side check here because gl_SetupLight needs the correct plane orientation
		// which we don't have for Legacy-style 3D-floors
		fixed_t planeh = plane.plane.ZatPoint(light->x, light->y);
		if (gl_lights_checkside && ((planeh<light->z && ceiling) || (planeh>light->z && !ceiling)))
		{
			node=node->nextLight;
			continue;
		}

		p.Set(plane.plane);
		if (!gl_SetupLight(p, light, nearPt, up, right, scale, Colormap.LightColor.a, false, foggy)) 
		{
			node=node->nextLight;
			continue;
		}
		draw_dlightf++;

		// Set the plane
		if (plane.plane.a || plane.plane.b) for(k = 0; k < sub->numvertices; k++)
		{
			// Unfortunately the rendering inaccuracies prohibit any kind of plane translation
			// This must be done on a per-vertex basis.
			gl_vertices[sub->firstvertex + k].z =
				TO_GL(plane.plane.ZatPoint(gl_vertices[sub->firstvertex + k].vt));
		}
		else for(k = 0; k < sub->numvertices; k++)
		{
			gl_vertices[sub->firstvertex + k].z = z;
		}

		// Render the light
		gl.Begin(GL_TRIANGLE_FAN);
		for(k = 0; k < sub->numvertices; k++)
		{
			Vector t1;
			GLVertex * vt = &gl_vertices[sub->firstvertex + k];

			t1.Set(vt->x, vt->z, vt->y);
			Vector nearToVert = t1 - nearPt;
			gl.TexCoord2f( (nearToVert.Dot(right) * scale) + 0.5f, (nearToVert.Dot(up) * scale) + 0.5f);
			gl.Vertex3f(vt->x, vt->z, vt->y);
		}
		gl.End();
		node = node->nextLight;
	}
}

//==========================================================================
//
//
//
//==========================================================================
CVAR(Bool, gl_usearrays, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)


void GLFlat::DrawSubsector(subsector_t * sub)
{
	int k;
	int v;

	// Set the plane
	if (plane.plane.a || plane.plane.b) for(k = 0, v = sub->firstvertex; k < sub->numvertices; k++, v++)
	{
		// Unfortunately the rendering inaccuracies prohibit any kind of plane translation
		// This must be done on a per-vertex basis.
		gl_vertices[v].z = TO_GL(plane.plane.ZatPoint(gl_vertices[v].vt));
	}
	else for(k = 0, v = sub->firstvertex; k < sub->numvertices; k++, v++)
	{
		// Quicker way for non-sloped sectors
		gl_vertices[v].z = z;
	}
	if (gl_usearrays)
	{
		gl.DrawArrays(GL_TRIANGLE_FAN, sub->firstvertex, sub->numvertices);
	}
	else
	{
		gl.Begin(GL_TRIANGLE_FAN);
		for(k = 0, v = sub->firstvertex; k < sub->numvertices; k++, v++)
		{
			gl.TexCoord2f(gl_vertices[v].u, gl_vertices[v].v);
			gl.Vertex3f(gl_vertices[v].x, gl_vertices[v].z, gl_vertices[v].y);
		}
		gl.End();
	}

	flatvertices += sub->numvertices;
	flatprimitives++;
}

//==========================================================================
//
//
//
//==========================================================================

void GLFlat::DrawSubsectors(bool istrans)
{
	gl_ApplyShader();
	if (sub)
	{
		// This represents a single subsector
		DrawSubsector(sub);
	}
	else
	{
		// Draw the subsectors belonging to this sector
		for (int i=0; i<sector->subsectorcount; i++)
		{
			subsector_t * sub = sector->subsectors[i];

			// This is just a quick hack to make translucent 3D floors and portals work.
			if (gl_drawinfo->ss_renderflags[sub-subsectors]&renderflags || istrans)
			{
				DrawSubsector(sub);
			}
		}

		// Draw the subsectors assigned to it due to missing textures
		if (!(renderflags&SSRF_RENDER3DPLANES))
		{
			gl_subsectorrendernode * node = (renderflags&SSRF_RENDERFLOOR)?
				gl_drawinfo->GetOtherFloorPlanes(sector->sectornum) :
				gl_drawinfo->GetOtherCeilingPlanes(sector->sectornum);

			while (node)
			{
				DrawSubsector(node->sub);
				node = node->next;
			}
		}
	}
}

//==========================================================================
//
//
//
//==========================================================================
void GLFlat::Draw(int pass)
{
	int i;
	int rel = extralight*gl_weaponlight;

#ifdef _MSC_VER
#ifdef _DEBUG
	if (sector->sectornum == 249 || sector->sectornum == 287)
		__asm nop
#endif
#endif


	switch (pass)
	{
	case GLPASS_BASE:
		gl_SetColor(lightlevel, rel, &Colormap,1.0f);
		if (!foggy) gl_SetFog(lightlevel, rel, &Colormap, false);
		DrawSubsectors(false);
		break;

	case GLPASS_BASE_MASKED:
	case GLPASS_PLAIN:			// Single-pass rendering
		gl_SetColor(lightlevel, rel, &Colormap,1.0f);
		if (!foggy || pass == GLPASS_PLAIN) 
			gl_SetFog(lightlevel, rel, &Colormap, false);
		// fall through
	case GLPASS_TEXTURE:
		gltexture->Bind(Colormap.LightColor.a);
		gl_SetPlaneTextureRotation(&plane, gltexture);
		DrawSubsectors(false);
		gl.PopMatrix();
		break;

	case GLPASS_LIGHT:
	case GLPASS_LIGHT_ADDITIVE:

		if (!foggy)	gl_SetFog((255+lightlevel)>>1, 0, &Colormap, false);
		else gl_SetFog(lightlevel, 0, &Colormap, true);	

		if (sub)
		{
			DrawSubsectorLights(sub, pass);
		}
		else
		{
			// Draw the subsectors belonging to this sector
			for (i=0; i<sector->subsectorcount; i++)
			{
				subsector_t * sub = sector->subsectors[i];

				if (gl_drawinfo->ss_renderflags[sub-subsectors]&renderflags)
				{
					DrawSubsectorLights(sub, pass);
				}
			}

			// Draw the subsectors assigned to it due to missing textures
			if (!(renderflags&SSRF_RENDER3DPLANES))
			{
				gl_subsectorrendernode * node = (renderflags&SSRF_RENDERFLOOR)?
					gl_drawinfo->GetOtherFloorPlanes(sector->sectornum) :
					gl_drawinfo->GetOtherCeilingPlanes(sector->sectornum);

				while (node)
				{
					DrawSubsectorLights(node->sub, pass);
					node = node->next;
				}
			}
		}
		break;

	case GLPASS_TRANSLUCENT:
		if (renderstyle==STYLE_Add) gl.BlendFunc(GL_SRC_ALPHA, GL_ONE);
		gl_SetColor(lightlevel, rel, &Colormap, alpha);
		gl_SetFog(lightlevel, rel, &Colormap, false);
		gl.AlphaFunc(GL_GEQUAL,0.5f*(alpha));
		if (!gltexture)	gl_EnableTexture(false);

		else 
		{
			if (foggy) gl_EnableBrightmap(false);
			gltexture->Bind(Colormap.LightColor.a);
			gl_SetPlaneTextureRotation(&plane, gltexture);
		}

		DrawSubsectors(true);

		gl_EnableBrightmap(true);
		if (!gltexture)	gl_EnableTexture(true);
		else gl.PopMatrix();
		if (renderstyle==STYLE_Add) gl.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		break;
	}
}



//==========================================================================
//
// GLFlat::PutFlat
//
// Checks texture, lighting and translucency settings and puts this
// plane in the appropriate render list.
//
//==========================================================================
inline void GLFlat::PutFlat(bool fog)
{
	int list;

	if (gl_fixedcolormap) 
	{
		Colormap.GetFixedColormap();
	}
	if ((gl.flags&RFL_NOSTENCIL) && !(renderflags&SSRF_RENDER3DPLANES))
	{
		renderstyle=STYLE_Translucent;
		alpha=1.f;
	}
	if (renderstyle!=STYLE_Translucent || alpha < 1.f - FLT_EPSILON || fog)
	{
		int list = (renderflags&SSRF_RENDER3DPLANES) ? GLDL_TRANSLUCENT : GLDL_TRANSLUCENTBORDER;
		gl_drawinfo->drawlists[list].AddFlat (this);
	}
	else if (gltexture != NULL)
	{
		static DrawListType list_indices[2][2][2]={
			{ { GLDL_PLAIN, GLDL_FOG      }, { GLDL_MASKED,      GLDL_FOGMASKED      } },
			{ { GLDL_LIGHT, GLDL_LIGHTFOG }, { GLDL_LIGHTMASKED, GLDL_LIGHTFOGMASKED } }
		};

		bool light = gl_forcemultipass;
		bool masked = gltexture->tex->bMasked && ((renderflags&SSRF_RENDER3DPLANES) || stack);

		if (!gl_fixedcolormap)
		{
			foggy = !gl_isBlack (Colormap.FadeColor) || level.flags&LEVEL_HASFADETABLE;

			if (gl_lights && gl_lightcount)	// Are lights touching this sector?
			{
				for(int i=0;i<sector->subsectorcount;i++) if (sector->subsectors[i]->lighthead[0] != NULL)
				{
					light=true;
				}
			}
		}
		else foggy = false;

		list = list_indices[light][masked][foggy];
		if (list == GLDL_LIGHT && gltexture->tex->gl_info.Brightmap && gl_brightmap_shader) list = GLDL_LIGHTBRIGHT;

		gl_drawinfo->drawlists[list].AddFlat (this);
	}
}

//==========================================================================
//
// This draws one flat 
// The passed sector does not indicate the area which is rendered. 
// It is only used as source for the plane data.
// The whichplane boolean indicates if the flat is a floor(false) or a ceiling(true)
//
//==========================================================================

void GLFlat::Process(sector_t * sector, bool whichplane, bool fog)
{
	plane.GetFromSector(sector, whichplane);

	if (!fog)
	{
		if (plane.texture==skyflatnum) return;

		gltexture=FGLTexture::ValidateTexture(plane.texture);
		if (!gltexture) return;
		if (gltexture->tex->isFullbright()) 
		{
			Colormap.LightColor.r = Colormap.LightColor.g = Colormap.LightColor.b = 0xff;
			lightlevel=255;
		}
	}
	else 
	{
		gltexture=NULL;
		lightlevel=abs(lightlevel);
	}

	// get height from vplane
	z=TO_GL(plane.texheight);

	if (!whichplane && sector->transdoor) z -= 1;
	
	PutFlat(fog);
	rendered_flats++;
}

//==========================================================================
//
// Process a sector's flats for rendering
//
//==========================================================================
#define CenterSpot(sec) (vertex_t*)&(sec)->soundorg[0]

void GLFlat::ProcessSector(sector_t * frontsector, subsector_t * sub)
{
	lightlist_t * light;

#ifdef _MSC_VER
#ifdef _DEBUG
	if (frontsector==NULL)
	{
		__asm int 3
	}
	if (frontsector->sectornum==6)
	{
		__asm nop
	}
#endif
#endif

	// Get the real sector for this one.
	sector=&sectors[frontsector->sectornum];	
	extsector_t::xfloor &x = sector->e->XFloor;
	this->sub=NULL;

	gl_drawinfo->ss_renderflags[sub-subsectors]|=SSRF_PROCESSED;
	if (sub->hacked&1) gl_drawinfo->AddHackedSubsector(sub);
	if (sub->degenerate) return;

	byte * srf = &gl_drawinfo->sectorrenderflags[sector->sectornum];

	//
	//
	//
	// do floors
	//
	//
	//
	if (frontsector->floorplane.ZatPoint(viewx, viewy) <= viewz)
	{
		gl_drawinfo->ss_renderflags[sub-subsectors]|=SSRF_RENDERFLOOR;

		// process the original floor first.
		if (frontsector->FloorSkyBox && frontsector->FloorSkyBox->bAlways) gl_drawinfo->AddFloorStack(sub);

		if (!((*srf)&SSRF_RENDERFLOOR))
		{
			(*srf) |= SSRF_RENDERFLOOR;

			lightlevel = GetFloorLight(frontsector);
			Colormap=frontsector->ColorMap;
			stack = frontsector->FloorSkyBox && frontsector->FloorSkyBox->bAlways;
			alpha= stack ? frontsector->FloorSkyBox->PlaneAlpha/65536.0f : 1.0f-frontsector->floor_reflect;

			ceiling=false;
			renderflags=SSRF_RENDERFLOOR;

			if (x.ffloors.Size())
			{
				light = P_GetPlaneLight(sector, &frontsector->floorplane, false);
				if (!(sector->GetFlags(sector_t::floor)&SECF_ABSLIGHTING) || light!=&x.lightlist[0])	
					lightlevel = *light->p_lightlevel;

				Colormap.CopyLightColor(light->extra_colormap);
			}
			renderstyle = STYLE_Translucent;
			if (alpha!=0.0f) Process(frontsector, false, false);
		}
	}
	
	//
	//
	//
	// do ceilings
	//
	//
	//
	if (frontsector->ceilingplane.ZatPoint(viewx, viewy) >= viewz)
	{
		gl_drawinfo->ss_renderflags[sub-subsectors]|=SSRF_RENDERCEILING;

		// process the original ceiling first.
		if (frontsector->CeilingSkyBox && frontsector->CeilingSkyBox->bAlways) gl_drawinfo->AddCeilingStack(sub);

		if (!((*srf)&SSRF_RENDERCEILING))
		{
			(*srf) |= SSRF_RENDERCEILING;

			lightlevel = GetCeilingLight(frontsector);
			Colormap=frontsector->ColorMap;
			stack = frontsector->CeilingSkyBox && frontsector->CeilingSkyBox->bAlways;
			alpha=stack ? frontsector->CeilingSkyBox->PlaneAlpha/65536.0f : 1.0f-frontsector->ceiling_reflect;

			ceiling=true;
			renderflags=SSRF_RENDERCEILING;

			if (x.ffloors.Size())
			{
				light = P_GetPlaneLight(sector, &sector->ceilingplane, true);

				if(!(sector->GetFlags(sector_t::ceiling)&SECF_ABSLIGHTING)) lightlevel = *light->p_lightlevel;
				Colormap.CopyLightColor(light->extra_colormap);
			}
			renderstyle = STYLE_Translucent;
			if (alpha!=0.0f) Process(frontsector, true, false);
		}
	}

	//
	//
	//
	// do 3D floors
	//
	//
	//

	stack=false;
	if (x.ffloors.Size())
	{
		player_t * player=players[consoleplayer].camera->player;

		// do the plane setup only once and just mark all subsectors that have to be processed
		gl_drawinfo->ss_renderflags[sub-subsectors]|=SSRF_RENDER3DPLANES;
		renderflags=SSRF_RENDER3DPLANES;
		if (!((*srf)&SSRF_RENDER3DPLANES))
		{
			(*srf) |= SSRF_RENDER3DPLANES;
			// 3d-floors must not overlap!
			fixed_t lastceilingheight=sector->CenterCeiling();	// render only in the range of the
			fixed_t lastfloorheight=sector->CenterFloor();		// current sector part (if applicable)
			F3DFloor * rover;	
			int k;
			
			// floors are ordered now top to bottom so scanning the list for the best match
			// is no longer necessary.

			ceiling=true;
			for(k=0;k<x.ffloors.Size();k++)
			{
				rover=x.ffloors[k];
				
				if ((rover->flags&(FF_EXISTS|FF_RENDERPLANES))==(FF_EXISTS|FF_RENDERPLANES))
				{
					if (rover->flags&FF_FOG && gl_fixedcolormap) continue;
					if (rover->flags&(FF_INVERTPLANES|FF_BOTHPLANES))
					{
						fixed_t ff_top=rover->top.plane->ZatPoint(CenterSpot(sector));
						if (ff_top<lastceilingheight)
						{
							if (viewz<=rover->top.plane->ZatPoint(viewx, viewy))
							{
								// FF_FOG requires an inverted logic where to get the light from
								light=P_GetPlaneLight(sector, rover->top.plane,!!(rover->flags&FF_FOG));
								lightlevel=*light->p_lightlevel;
								
								if (rover->flags&FF_FOG) Colormap.LightColor = (light->extra_colormap)->Fade;
								else Colormap.CopyLightColor(light->extra_colormap);

								Colormap.FadeColor=frontsector->ColorMap->Fade;

								alpha=rover->alpha/255.0f;
								renderstyle = rover->flags&FF_ADDITIVETRANS? STYLE_Add : STYLE_Translucent;
								Process(rover->top.model, rover->top.isceiling, !!(rover->flags&FF_FOG));
							}
							lastceilingheight=ff_top;
						}
					}
					if (!(rover->flags&FF_INVERTPLANES))
					{
						fixed_t ff_bottom=rover->bottom.plane->ZatPoint(CenterSpot(sector));
						if (ff_bottom<lastceilingheight)
						{
							if (viewz<=rover->bottom.plane->ZatPoint(viewx, viewy))
							{
								light=P_GetPlaneLight(sector, rover->bottom.plane,!(rover->flags&FF_FOG));
								lightlevel=*light->p_lightlevel;

								if (rover->flags&FF_FOG) Colormap.LightColor = (light->extra_colormap)->Fade;
								else Colormap.CopyLightColor(light->extra_colormap);

								Colormap.FadeColor=frontsector->ColorMap->Fade;

								alpha=rover->alpha/255.0f;
								renderstyle = rover->flags&FF_ADDITIVETRANS? STYLE_Add : STYLE_Translucent;
								Process(rover->bottom.model, rover->bottom.isceiling, !!(rover->flags&FF_FOG));
							}
							lastceilingheight=ff_bottom;
							if (rover->alpha<255) lastceilingheight++;
						}
					}
				}
			}
				  
			ceiling=false;
			for(k=x.ffloors.Size()-1;k>=0;k--)
			{
				rover=x.ffloors[k];
				
				if ((rover->flags&(FF_EXISTS|FF_RENDERPLANES))==(FF_EXISTS|FF_RENDERPLANES))
				{
					if (rover->flags&FF_FOG && gl_fixedcolormap) continue;
					if (rover->flags&(FF_INVERTPLANES|FF_BOTHPLANES))
					{
						fixed_t ff_bottom=rover->bottom.plane->ZatPoint(CenterSpot(sector));
						if (ff_bottom>lastfloorheight || (rover->flags&FF_FIX))
						{
							if (viewz>=rover->bottom.plane->ZatPoint(viewx, viewy))
							{
								if (rover->flags&FF_FIX)
								{
									lightlevel = rover->model->lightlevel;
									Colormap = rover->model->ColorMap;
								}
								else
								{
									light=P_GetPlaneLight(sector, rover->bottom.plane,!(rover->flags&FF_FOG));
									lightlevel=*light->p_lightlevel;

									if (rover->flags&FF_FOG) Colormap.LightColor = (light->extra_colormap)->Fade;
									else Colormap.CopyLightColor(light->extra_colormap);

									Colormap.FadeColor=frontsector->ColorMap->Fade;
								}

								alpha=rover->alpha/255.0f;
								renderstyle = rover->flags&FF_ADDITIVETRANS? STYLE_Add : STYLE_Translucent;
								Process(rover->bottom.model, rover->bottom.isceiling, !!(rover->flags&FF_FOG));
							}
							lastfloorheight=ff_bottom;
						}
					}
					if (!(rover->flags&FF_INVERTPLANES))
					{
						fixed_t ff_top=rover->top.plane->ZatPoint(CenterSpot(sector));
						if (ff_top>lastfloorheight)
						{
							if (viewz>=rover->top.plane->ZatPoint(viewx, viewy))
							{
								light=P_GetPlaneLight(sector, rover->top.plane,!!(rover->flags&FF_FOG));
								lightlevel=*light->p_lightlevel;

								if (rover->flags&FF_FOG) Colormap.LightColor = (light->extra_colormap)->Fade;
								else Colormap.CopyLightColor(light->extra_colormap);

								Colormap.FadeColor=frontsector->ColorMap->Fade;

								alpha=rover->alpha/255.0f;
								renderstyle = rover->flags&FF_ADDITIVETRANS? STYLE_Add : STYLE_Translucent;
								Process(rover->top.model, rover->top.isceiling, !!(rover->flags&FF_FOG));
							}
							lastfloorheight=ff_top;
							if (rover->alpha<255) lastfloorheight--;
						}
					}
				}
			}
		}
	}
}



