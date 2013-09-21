/*
** gl_light.cpp
** Light level / fog management / dynamic lights
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
#include "c_dispatch.h"
#include "p_local.h"
#include "vectors.h"
#include "gl/gl_struct.h"
#include "gl/gl_lights.h"
#include "gl/common/glc_renderer.h"
#include "gl/old_renderer/gl1_renderer.h"
#include "gl/gl_functions.h"
#include "gl/old_renderer/gl1_shader.h"
#include "g_level.h"
#include "gl/common/glc_convert.h"

#include "gl/data/gl_data.h"
#include "gl/scene/gl_drawinfo.h"
#include "gl/scene/gl_portal.h"
#include "gl/textures/gl_material.h"


EXTERN_CVAR (Float, transsouls)

//==========================================================================
//
// Light related CVARs
//
//==========================================================================
EXTERN_CVAR (Float, gl_light_ambient);


CUSTOM_CVAR (Bool, gl_lights, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (self) gl_RecreateAllAttachedLights();
	else gl_DeleteAllAttachedLights();
}

CVAR(Int, gl_weaponlight, 8, CVAR_ARCHIVE);
CVAR(Bool,gl_enhanced_nightvision,true,CVAR_ARCHIVE)

CVAR (Bool, gl_attachedlights, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);
CVAR (Bool, gl_lights_checkside, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);
CVAR (Float, gl_lights_intensity, 1.0f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);
CVAR (Float, gl_lights_size, 1.0f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);
CVAR (Bool, gl_light_sprites, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);
CVAR (Bool, gl_light_particles, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);
CUSTOM_CVAR (Bool, gl_lights_additive, false,  CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	TThinkerIterator<ADynamicLight> it;
	ADynamicLight * mo;

	// Relink the lights if they switch lists
	while ((mo=it.Next()))
	{
		if (!(mo->flags&MF4_ADDITIVE))
		{
			mo->UnlinkLight();
			mo->LinkLight();
		}
	}
}

CUSTOM_CVAR(Int,gl_fogmode,1,CVAR_ARCHIVE|CVAR_NOINITCALL)
{
	if (self>2) self=2;
	if (self<0) self=0;
	if (self == 2 && !gl_ExtFogActive()) self = 1;	// mode 2 requires SM4
}

CUSTOM_CVAR(Int, gl_lightmode, 3 ,CVAR_ARCHIVE|CVAR_NOINITCALL)
{
	if (self>4) self=4;
	if (self<0) self=0;
	if (self == 2 && !gl_ExtFogActive()) self = 3;	// mode 2 requires SM4

	// [BB] Enforce Doom lighting if requested by the dmflags.
	if ( dmflags2 & DF2_FORCE_GL_DEFAULTS )
		glset.lightmode = 3;
	else
		glset.lightmode = self;
}

// [BB]
int gl_GetLightMode () {
	return ( dmflags2 & DF2_FORCE_GL_DEFAULTS ) ? 3 : gl_lightmode;
}


//==========================================================================
//
// Get current light color
//
//==========================================================================
void gl_GetLightColor(int lightlevel, int rellight, const FColormap * cm, float * pred, float * pgreen, float * pblue, bool weapon)
{
	// [BB] This construction purposely overrides the CVAR gl_light_ambient with a local variable of the same name.
	// This allows to implement DF2_FORCE_GL_DEFAULTS without any further changes in this function.
	const float gl_light_ambient_CVAR_value = gl_light_ambient;
	const float gl_light_ambient = ( dmflags2 & DF2_FORCE_GL_DEFAULTS ) ? 20.f : gl_light_ambient_CVAR_value;

	float & r=*pred,& g=*pgreen,& b=*pblue;
	int torch=0;

	if (gl_fixedcolormap) 
	{
		if (gl_fixedcolormap==CM_LITE)
		{
			if (gl_enhanced_nightvision) r=0.375f, g=1.0f, b=0.375f;
			else r=g=b=1.0f;
		}
		else if (gl_fixedcolormap>=CM_TORCH)
		{
			int flicker=gl_fixedcolormap-CM_TORCH;
			r=(0.8f+(7-flicker)/70.0f);
			if (r>1.0f) r=1.0f;
			b=g=r;
			if (gl_enhanced_nightvision) b*=0.75f;
		}
		else r=g=b=1.0f;
		return;
	}

	PalEntry lightcolor = cm? cm->LightColor : PalEntry(255,255,255);
	int blendfactor = cm? cm->blendfactor : 0;

	lightlevel = gl_CalcLightLevel(lightlevel, rellight, weapon);
	PalEntry pe = gl_CalcLightColor(lightlevel, lightcolor, blendfactor);
	r = pe.r/255.f;
	g = pe.g/255.f;
	b = pe.b/255.f;
}

//==========================================================================
//
// set current light color
//
//==========================================================================
void gl_SetColor(int light, int rellight, const FColormap * cm, float *red, float *green, float *blue, PalEntry ThingColor, bool weapon)
{ 
	float r,g,b;
	gl_GetLightColor(light, rellight, cm, &r, &g, &b, weapon);

	*red = r * ThingColor.r/255.0f;
	*green = g * ThingColor.g/255.0f;
	*blue = b * ThingColor.b/255.0f;
}

//==========================================================================
//
// set current light color
//
//==========================================================================
void gl_SetColor(int light, int rellight, const FColormap * cm, float alpha, PalEntry ThingColor, bool weapon)
{ 
	float r,g,b;
	gl_GetLightColor(light, rellight, cm, &r, &g, &b, weapon);
	gl.Color4f(r * ThingColor.r/255.0f, g * ThingColor.g/255.0f, b * ThingColor.b/255.0f, alpha);
}


PalEntry gl_CurrentFogColor=-1;
float gl_CurrentFogDensity=-1;

//==========================================================================
//
//
//
//==========================================================================

void gl_InitFog()
{
	gl_CurrentFogColor=-1;
	gl_CurrentFogDensity=-1;
	gl_EnableFog(true);
	gl_EnableFog(false);
	gl.Hint(GL_FOG_HINT, GL_FASTEST);
	gl.Fogi(GL_FOG_MODE, GL_EXP);

}


//==========================================================================
//
// Sets the fog for the current polygon
//
//==========================================================================

void gl_SetFog(int lightlevel, int rellight, const FColormap *cmap, bool isadditive)
{
	// [BB] This construction purposely overrides the CVAR gl_light_ambient with a local variable of the same name.
	// This allows to implement DF2_FORCE_GL_DEFAULTS without any further changes in this function.
	const float gl_light_ambient_CVAR_value = gl_light_ambient;
	const float gl_light_ambient = ( dmflags2 & DF2_FORCE_GL_DEFAULTS ) ? 20.f : gl_light_ambient_CVAR_value;
	// [BB] Take care of gl_fogmode and DF2_FORCE_GL_DEFAULTS.
	OVERRIDE_FOGMODE_IF_NECESSARY

	PalEntry fogcolor;
	float fogdensity;

	if (level.flags&LEVEL_HASFADETABLE)
	{
		fogdensity=70;
		fogcolor=0x808080;
	}
	else if (cmap != NULL && gl_fixedcolormap == 0)
	{
		fogcolor = cmap->FadeColor;
		fogdensity = gl_GetFogDensity(lightlevel, fogcolor);
		fogcolor.a=0;
	}
	else
	{
		fogcolor = 0;
		fogdensity = 0;
	}

	// Make fog a little denser when inside a skybox
	if (GLPortal::inskybox) fogdensity+=fogdensity/2;


	// no fog in enhanced vision modes!
	if (fogdensity==0 || gl_fogmode == 0)
	{
		gl_CurrentFogColor=-1;
		gl_CurrentFogDensity=-1;
		gl_EnableFog(false);
	}
	else
	{
		if (glset.lightmode == 2 && fogcolor == 0)
		{
			float light = gl_CalcLightLevel(lightlevel, rellight, false);
			gl_SetShaderLight(light, lightlevel);
		}
		else
		{
			gl_SetShaderLight(1.f, 1.f);
		}

		// For additive rendering using the regular fog color here would mean applying it twice
		// so always use black
		if (isadditive)
		{
			fogcolor=0;
		}
		// Handle desaturation
		gl_ModifyColor(fogcolor.r, fogcolor.g, fogcolor.b, cmap->colormap);

		gl_EnableFog((int)fogcolor!=-1);
		if (fogcolor!=gl_CurrentFogColor)
		{
			if ((int)fogcolor!=-1)
			{
				GLfloat FogColor[4]={fogcolor.r/255.0f,fogcolor.g/255.0f,fogcolor.b/255.0f,0.0f};
				gl.Fogfv(GL_FOG_COLOR, FogColor);
			}
			gl_CurrentFogColor=fogcolor;
		}
		if (fogdensity!=gl_CurrentFogDensity)
		{
			gl.Fogf(GL_FOG_DENSITY, fogdensity/64000.f);
			gl_CurrentFogDensity=fogdensity;
		}
	}
}

//==========================================================================
//
// Sets up the parameters to render one dynamic light onto one plane
//
//==========================================================================
bool gl_SetupLight(Plane & p, ADynamicLight * light, Vector & nearPt, Vector & up, Vector & right, 
				   float & scale, int desaturation, bool checkside, bool forceadditive)
{
	Vector fn, pos;

    float x = TO_GL(light->x);
	float y = TO_GL(light->y);
	float z = TO_GL(light->z);
	
	float dist = fabsf(p.DistToPoint(x, z, y));
	float radius = (light->GetRadius() * gl_lights_size);
	
	if (radius <= 0.f) return false;
	if (dist > radius) return false;
	if (checkside && gl_lights_checkside && p.PointOnSide(x, z, y))
	{
		return false;
	}

	scale = 1.0f / ((2.f * radius) - dist);

	// project light position onto plane (find closest point on plane)


	pos.Set(x,z,y);
	fn=p.Normal();
	fn.GetRightUp(right, up);

#ifdef _MSC_VER
	nearPt = pos + fn * dist;
#else
	Vector tmpVec = fn * dist;
	nearPt = pos + tmpVec;
#endif

	float cs = 1.0f - (dist / radius);
	if (gl_lights_additive || light->flags4&MF4_ADDITIVE || forceadditive) cs*=0.2f;	// otherwise the light gets too strong.
	float r = light->GetRed() / 255.0f * cs * gl_lights_intensity;
	float g = light->GetGreen() / 255.0f * cs * gl_lights_intensity;
	float b = light->GetBlue() / 255.0f * cs * gl_lights_intensity;

	if (light->IsSubtractive())
	{
		Vector v;
		
		gl.BlendEquation(GL_FUNC_REVERSE_SUBTRACT);
		v.Set(r, g, b);
		r = v.Length() - r;
		g = v.Length() - g;
		b = v.Length() - b;
	}
	else
	{
		gl.BlendEquation(GL_FUNC_ADD);
	}
	if (desaturation>0)
	{
		float gray=(r*77 + g*143 + b*37)/257;

		r= (r*(32-desaturation)+ gray*desaturation)/32;
		g= (g*(32-desaturation)+ gray*desaturation)/32;
		b= (b*(32-desaturation)+ gray*desaturation)/32;
	}
	gl.Color3f(r,g,b);
	return true;
}


//==========================================================================
//
//
//
//==========================================================================

bool gl_SetupLightTexture()
{

	if (GLRenderer->gllight == NULL) return false;
	FMaterial * pat = FMaterial::ValidateTexture(GLRenderer->gllight);
	pat->BindPatch(CM_DEFAULT, 0);
	return true;
}


//==========================================================================
//
//
//
//==========================================================================

inline fixed_t P_AproxDistance3(fixed_t dx, fixed_t dy, fixed_t dz)
{
	return P_AproxDistance(P_AproxDistance(dx,dy),dz);
}

//==========================================================================
//
// Sets the light for a sprite - takes dynamic lights into account
//
//==========================================================================
void gl_GetSpriteLight(AActor *self, fixed_t x, fixed_t y, fixed_t z, subsector_t * subsec, int desaturation, float * out)
{
	ADynamicLight *light;
	float frac, lr, lg, lb;
	float radius;
	
	out[0]=out[1]=out[2]=0;

	for(int j=0;j<2;j++)
	{
		// Go through moth light lists
		FLightNode * node = subsec->lighthead[j];
		while (node)
		{
			light=node->lightsource;
			if (!(light->flags2&MF2_DORMANT) &&
				(!(light->flags4&MF4_DONTLIGHTSELF) || light->target != self))
			{
				float dist = FVector3( TO_GL(x - light->x), TO_GL(y - light->y), TO_GL(z - light->z) ).Length();
				radius = light->GetRadius() * gl_lights_size;
				
				if (dist < radius)
				{
					frac = 1.0f - (dist / radius);
					
					if (frac > 0)
					{
						lr = light->GetRed() / 255.0f * gl_lights_intensity;
						lg = light->GetGreen() / 255.0f * gl_lights_intensity;
						lb = light->GetBlue() / 255.0f * gl_lights_intensity;
						if (light->IsSubtractive())
						{
							float bright = FVector3(lr, lg, lb).Length();
							FVector3 lightColor(lr, lg, lb);
							lr = (bright - lr) * -1;
							lg = (bright - lg) * -1;
							lb = (bright - lb) * -1;
						}
						
						out[0] += lr * frac;
						out[1] += lg * frac;
						out[2] += lb * frac;
					}
				}
			}
			node = node->nextLight;
		}
	}

	// Desaturate dynamic lighting if applicable
	if (desaturation>0 && desaturation<=CM_DESAT31)
	{
		float gray=(out[0]*77 + out[1]*143 + out[2]*37)/257;

		out[0]= (out[0]*(31-desaturation)+ gray*desaturation)/31;
		out[1]= (out[1]*(31-desaturation)+ gray*desaturation)/31;
		out[2]= (out[2]*(31-desaturation)+ gray*desaturation)/31;
	}
}



static void gl_SetSpriteLight(AActor *self, fixed_t x, fixed_t y, fixed_t z, subsector_t * subsec, 
                              int lightlevel, int rellight, FColormap * cm, float alpha, 
							  PalEntry ThingColor, bool weapon)
{
	float r,g,b;
	float result[3];
	gl_GetSpriteLight(self, x, y, z, subsec, cm? cm->colormap : 0, result);
	gl_GetLightColor(lightlevel, rellight, cm, &r, &g, &b, weapon);
	// Note: Due to subtractive lights the values can easily become negative so we have to clamp both
	// at the low and top end of the range!
	r = clamp<float>(result[0]+r, 0, 1.0f) * ThingColor.r/255.f;
	g = clamp<float>(result[1]+g, 0, 1.0f) * ThingColor.g/255.f;
	b = clamp<float>(result[2]+b, 0, 1.0f) * ThingColor.b/255.f;
	gl.Color4f(r, g, b, alpha);
}

void gl_SetSpriteLight(AActor * thing, int lightlevel, int rellight, FColormap * cm,
					   float alpha, PalEntry ThingColor, bool weapon)
{ 
	subsector_t * subsec = thing->subsector;

	gl_SetSpriteLight(thing, thing->x, thing->y, thing->z+(thing->height>>1), subsec, 
					  lightlevel, rellight, cm, alpha, ThingColor, weapon);
}

void gl_SetSpriteLight(particle_t * thing, int lightlevel, int rellight, FColormap *cm, float alpha, PalEntry ThingColor)
{ 
	gl_SetSpriteLight(NULL, thing->x, thing->y, thing->z, thing->subsector, lightlevel, rellight, 
					  cm, alpha, ThingColor, false);
}

//==========================================================================
//
// Modifies the color values depending on the render style
//
//==========================================================================

void gl_GetSpriteLighting(FRenderStyle style, AActor *thing, FColormap *cm, PalEntry &ThingColor)
{
	if (style.Flags & STYLEF_RedIsAlpha)
	{
		cm->colormap = CM_SHADE;
	}
	if (style.Flags & STYLEF_ColorIsFixed)
	{
		if (style.Flags & STYLEF_InvertSource)
		{
			ThingColor = PalEntry(thing->fillcolor).InverseColor();
		}
		else
		{
			ThingColor = thing->fillcolor;
		}
	}

	// This doesn't work like in the software renderer.
	if (style.Flags & STYLEF_InvertSource)
	{
		int gray = (cm->LightColor.r*77 + cm->LightColor.r*143 + cm->LightColor.r*36)>>8;
		cm->LightColor.r = cm->LightColor.g = cm->LightColor.b = gray;
	}
}


//==========================================================================
//
// Sets render state to draw the given render style
//
//==========================================================================

void gl_SetSpriteLighting(FRenderStyle style, AActor *thing, int lightlevel, int rellight, FColormap *cm, 
						  PalEntry ThingColor, float alpha, bool fullbright, bool weapon)
{
	FColormap internal_cm;

	if (style.Flags & STYLEF_RedIsAlpha)
	{
		cm->colormap = CM_SHADE;
	}
	if (style.Flags & STYLEF_ColorIsFixed)
	{
		if (style.Flags & STYLEF_InvertSource)
		{
			ThingColor = PalEntry(thing->fillcolor).InverseColor();
		}
		else
		{
			ThingColor = thing->fillcolor;
		}
		gl_ModifyColor(ThingColor.r, ThingColor.g, ThingColor.b, cm->colormap);
	}

	// This doesn't work like in the software renderer.
	if (style.Flags & STYLEF_InvertSource)
	{
		internal_cm = *cm;
		cm = &internal_cm;

		int gray = (internal_cm.LightColor.r*77 + internal_cm.LightColor.r*143 + internal_cm.LightColor.r*36)>>8;
		cm->LightColor.r = cm->LightColor.g = cm->LightColor.b = gray;
	}

	if (style.BlendOp == STYLEOP_Fuzz)
	{
		gl.Color4f(0.2f * ThingColor.r / 255.f, 0.2f * ThingColor.g / 255.f, 
					0.2f * ThingColor.b / 255.f, (alpha = 0.33f));
	}
	else
	{
		if (gl_light_sprites && gl_lights && GLRenderer->mLightCount && !fullbright)
		{
			gl_SetSpriteLight(thing, lightlevel, rellight, cm, alpha, ThingColor, weapon);
		}
		else
		{
			gl_SetColor(lightlevel, rellight, cm, alpha, ThingColor, weapon);
		}
	}
	gl.AlphaFunc(GL_GEQUAL,alpha/2.f);
}

//==========================================================================
//
// Modifies a color according to a specified colormap
//
//==========================================================================

void gl_ModifyColor(BYTE & red, BYTE & green, BYTE & blue, int cm)
{
	int gray = (red*77 + green*143 + blue*36)>>8;
	if (cm >= CM_FIRSTSPECIALCOLORMAP && cm < CM_FIRSTSPECIALCOLORMAP + SpecialColormaps.Size())
	{
		PalEntry pe = SpecialColormaps[cm - CM_FIRSTSPECIALCOLORMAP].GrayscaleToColor[gray];
		red = pe.r;
		green = pe.g;
		blue = pe.b;
	}
	else if (cm >= CM_DESAT1 && cm <= CM_DESAT31)
	{
		gl_Desaturate(gray, red, green, blue, red, green, blue, cm - CM_DESAT0);
	}
}

