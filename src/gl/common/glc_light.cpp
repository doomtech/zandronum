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
// [BB] Necessary to make GZDoom revision 486 compile.
#include "gl/data/gl_data.h"
#include "c_dispatch.h"
#include "p_local.h"

static float distfogtable[2][256];	// light to fog conversion table for black fog
static int fogdensity;
static PalEntry outsidefogcolor;
static int outsidefogdensity;
int skyfog;

CVAR (Float, gl_light_ambient, 20.f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);


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
// Sets render state to draw the given render style
// includes: Texture mode, blend equation and blend mode
//
//==========================================================================

void gl_GetRenderStyle(FRenderStyle style, bool drawopaque, bool allowcolorblending,
					   int *tm, int *sb, int *db, int *be)
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

	*tm = texturemode;
	*be = blendequation;
	*sb = srcblend;
	*db = dstblend;
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
// Get current light level
//
//==========================================================================

int gl_CalcLightLevel(int lightlevel, int rellight, bool weapon)
{
	int light;

	if (glset.lightmode&2 && lightlevel<192) 
	{
		light = (192.f - (192-lightlevel)* (weapon? 1.5f : 1.95f));
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
	return clamp(quickertoint(light+rellight), 0, 255);
}

//==========================================================================
//
// Get current light color
//
//==========================================================================

PalEntry gl_CalcLightColor(int light, PalEntry pe, int blendfactor)
{
	int r,g,b;

	if (blendfactor == 0)
	{
		r = pe.r * light / 255;
		g = pe.g * light / 255;
		b = pe.b * light / 255;
	}
	else
	{
		int mixlight = light * (255 - blendfactor);

		r = (mixlight + pe.r * blendfactor) / 255;
		g = (mixlight + pe.g * blendfactor) / 255;
		b = (mixlight + pe.b * blendfactor) / 255;
	}
	return PalEntry(BYTE(r), BYTE(g), BYTE(b));
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

	if (glset.lightmode&4)
	{
		// uses approximations of Legacy's default settings.
		density = fogdensity? fogdensity : 18;
	}
	else if ((fogcolor.d & 0xffffff) == 0)
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

