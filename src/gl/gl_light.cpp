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
#include "gl/gl_system.h"
#include "gl/gl_struct.h"
#include "gl/gl_lights.h"
#include "gl/gl_data.h"
#include "gl/gl_texture.h"
#include "gl/gl_functions.h"
#include "gl/gl_portal.h"
#include "g_level.h"


EXTERN_CVAR (Float, transsouls)

//==========================================================================
//
// Light related CVARs
//
//==========================================================================

CUSTOM_CVAR (Bool, gl_lights, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (self) gl_RecreateAllAttachedLights();
	else gl_DeleteAllAttachedLights();
}

CVAR(Int, gl_weaponlight, 8, CVAR_ARCHIVE);
CVAR(Bool,gl_enhanced_nightvision,true,CVAR_ARCHIVE)

CVAR (Bool, gl_attachedlights, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);
CVAR (Bool, gl_bulletlight, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);
CVAR (Bool, gl_lights_checkside, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);
CVAR (Float, gl_lights_intensity, 1.0f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);
CVAR (Float, gl_lights_size, 1.0f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);
CVAR (Bool, gl_light_sprites, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);
CVAR (Bool, gl_light_particles, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);
CVAR (Float, gl_light_ambient, 20.f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);
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
	if (self == 2 && !(gl.flags & RFL_GLSL)) self = 1;	// mode 2 requires GLSL
}

CUSTOM_CVAR(Int, gl_lightmode, 3 ,CVAR_ARCHIVE|CVAR_NOINITCALL)
{
	if (self>4) self=4;
	if (self<0) self=0;
	if (self == 2 && !(gl.flags & RFL_GLSL)) self = 3;	// mode 2 requires GLSL

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

static float distfogtable[2][256];	// light to fog conversion table for black fog

static int fogdensity;
static PalEntry outsidefogcolor;
static int outsidefogdensity;
int skyfog;

//==========================================================================
//
// Sets up the fog tables
//
//==========================================================================

CUSTOM_CVAR (Int, gl_distfog, 70, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	for (int i=0;i<256;i++)
	{

		if (i<164)
		{
			distfogtable[0][i]= (gl_distfog>>1) + (gl_distfog)*(164-i)/164;
		}
		else if (i<230)
		{											    
			distfogtable[0][i]= (gl_distfog>>1) - (gl_distfog>>1)*(i-164)/(230-164);
		}
		else distfogtable[0][i]=0;

		if (i<128)
		{
			distfogtable[1][i]= 6.f + (gl_distfog>>1) + (gl_distfog)*(128-i)/48;
		}
		else if (i<216)
		{											    
			distfogtable[1][i]= (216.f-i) / ((216.f-128.f)) * gl_distfog / 10;
		}
		else distfogtable[1][i]=0;
	}
}

//==========================================================================
//
// Set fog parameters for the level
//
//==========================================================================
void gl_SetFogParams(int _fogdensity, PalEntry _outsidefogcolor, int _outsidefogdensity, int _skyfog)
{
	fogdensity=_fogdensity;
	outsidefogcolor=_outsidefogcolor;
	outsidefogdensity=_outsidefogdensity? _outsidefogdensity : _fogdensity? _fogdensity:70;
	skyfog=_skyfog;

	outsidefogdensity>>=1;
	fogdensity>>=1;
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

	float light;

	if (glset.lightmode&2 && lightlevel<192) 
	{
		if (!weapon)
		{
			light = (192.f - (192-lightlevel)*1.95f);
		}
		else
		{
			// For weapons make it a little brighter so that they remain visible
			light = (192.f - (192-lightlevel)*1.5f);
		}
	}
	else
	{
		light=lightlevel;
	}

	if (light<gl_light_ambient) 
	{
		light=gl_light_ambient;
		if (rellight<0) rellight>>=1;
	}
	light = clamp(light+rellight, 0.f, 255.f);

	if (cm!=NULL)
	{
		if (cm->blendfactor==0)
		{
			r=cm->LightColor.r * light / 255.0f / 255.f;
			g=cm->LightColor.g * light / 255.0f / 255.f;
			b=cm->LightColor.b * light / 255.0f / 255.f;
		}
		else
		{
			float mixlight = light * (255 - cm->blendfactor);

			r = (mixlight + cm->LightColor.r * cm->blendfactor) / 255.f / 255.f;
			g = (mixlight + cm->LightColor.g * cm->blendfactor) / 255.f / 255.f;
			b = (mixlight + cm->LightColor.b * cm->blendfactor) / 255.f / 255.f;
		}
	}
	else r=g=b=light/255.f;
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

//==========================================================================
//
// calculates the current fog density
//
//	Rules for fog:
//
//  1. If bit 4 of gl_lightmode is set always use the level's fog density. 
//     This is what Legacy's GL render does.
//	2. black fog means no fog and always uses the distfogtable based on the level's fog density setting
//	3. If outside fog is defined and the current fog color is the same as the outside fog
//	   the engine always uses the outside fog density to make the fog uniform across the level.
//	   If the outside fog's density is undefined it uses the level's fog density and if that is
//	   not defined it uses a default of 70.
//	4. If a global fog density is specified it is being used for all fog on the level
//	5. If none of the above apply fog density is based on the light level as for the software renderer.
//
//==========================================================================

float gl_GetFogDensity(int lightlevel, PalEntry fogcolor)
{
	float density;

	if (gl_fixedcolormap) 
	{
		return 0;
	}
	if (glset.lightmode&4)
	{
		// uses approximations of Legacy's default settings.
		density = fogdensity? fogdensity : 18;
	}
	else if (gl_isBlack(fogcolor))
	{
		// case 1: black fog
		density=distfogtable[glset.lightmode!=0][lightlevel];
	}
	else if (outsidefogcolor.a!=0xff && 
			fogcolor.r==outsidefogcolor.r && 
			fogcolor.g==outsidefogcolor.g &&
			fogcolor.b==outsidefogcolor.b) 
	{
		// case 2. outsidefogdensity has already been set as needed
		density=outsidefogdensity;
	}
	else  if (fogdensity!=0)
	{
		// case 3: level has fog density set
		density=fogdensity;
	}
	else 
	{
		// case 4: use light level
		density=clamp<int>(255-lightlevel,30,255);
	}
	return density;
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
	else if (cmap != NULL)
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
		if ((glset.lightmode == 2 && gl_fog_shader) && fogcolor == 0)
		{
			float light;

			if (lightlevel<192) 
			{
				light = (192.f - (192-lightlevel)*1.95f);
			}
			else
			{
				light=lightlevel;
			}

			if (light<gl_light_ambient) 
			{
				light=gl_light_ambient;
				if (rellight<0) rellight>>=1;
			}
			light = clamp(light+rellight, 0.f, 255.f);

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
		gl_ModifyColor(fogcolor.r, fogcolor.g, fogcolor.b, cmap->LightColor.a);

		gl_EnableFog(fogcolor!=-1);
		if (fogcolor!=gl_CurrentFogColor)
		{
			if (fogcolor!=-1)
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

	if (gllight == NULL) return false;
	FGLTexture * pat = FGLTexture::ValidateTexture(gllight);
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
	gl_GetSpriteLight(self, x, y, z, subsec, cm? cm->LightColor.a : 0, result);
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
// Sets render state to draw the given render style
// includes: Texture mode, blend equation and blend mode
//
//==========================================================================
void gl_SetRenderStyle(FRenderStyle style, bool drawopaque, bool allowcolorblending)
{
	static int blendstyles[] = { GL_ZERO, GL_ONE, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA };
	static int renderops[] = { 0, GL_FUNC_ADD, GL_FUNC_SUBTRACT, GL_FUNC_REVERSE_SUBTRACT, -1, -1, -1, -1};

	int srcblend = blendstyles[style.SrcAlpha&3];
	int dstblend = blendstyles[style.DestAlpha&3];
	int blendequation = renderops[style.BlendOp&7];
	int texturemode = drawopaque? TM_OPAQUE : TM_MODULATE;

	if (style.Flags & STYLEF_ColorIsFixed)
	{
		texturemode = TM_MASK;
	}
	else if (style.Flags & STYLEF_InvertSource)
	{
		texturemode = drawopaque? TM_INVERTOPAQUE : TM_INVERT;
	}

	if (blendequation == -1)
	{
		srcblend = GL_DST_COLOR;
		dstblend = GL_ONE_MINUS_SRC_ALPHA;
		blendequation = GL_FUNC_ADD;
	}

	if (allowcolorblending && srcblend == GL_SRC_ALPHA && dstblend == GL_ONE && blendequation == GL_FUNC_ADD)
	{
		srcblend = GL_SRC_COLOR;
	}

	gl.BlendEquation(blendequation);
	gl.BlendFunc(srcblend, dstblend);
	gl_SetTextureMode(texturemode);
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
		cm->LightColor.a = CM_SHADE;
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
		cm->LightColor.a = CM_SHADE;
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
		gl_ModifyColor(ThingColor.r, ThingColor.g, ThingColor.b, cm->LightColor.a);
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
		if (gl_light_sprites && gl_lights && gl_lightcount && !fullbright)
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
// For testing sky fog sheets
//
//==========================================================================
CCMD(skyfog)
{
	if (argv.argc()>1)
	{
		skyfog=strtol(argv[1],NULL,0);
	}
}



