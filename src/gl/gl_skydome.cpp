/*
** gl_sky.cpp
**
** Draws the sky.  Loosely based on the JDoom sky and the ZDoomGL 0.66.2 sky.
**
**---------------------------------------------------------------------------
** Copyright 2003 Tim Stump
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
** 4. Full disclosure of the entire project's source code, except for third
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
#include "doomtype.h"
#include "g_level.h"
#include "sc_man.h"
#include "w_wad.h"
#include "gl/gl_portal.h"
#include "gl/gl_texture.h"
#include "gl/gl_functions.h"
#include "gl/gl_intern.h"
#include "gl/gl_shader.h"


//-----------------------------------------------------------------------------
//
// This is not a real texture but will be added to the texture manager
// so that it can be handled like any other sky.
//
//-----------------------------------------------------------------------------

class FSkyBox : public FTexture
{
public:

	FTexture * faces[6];
	bool fliptop;

	FSkyBox() 
	{ 
		faces[0]=faces[1]=faces[2]=faces[3]=faces[4]=faces[5]=NULL; 
		UseType=TEX_Override;
		gl_info.bSkybox = true;
		fliptop = false;
	}
	~FSkyBox()
	{
	}

	// If something attempts to use this as a texture just pass the information of the first face.
	virtual const BYTE *GetColumn (unsigned int column, const Span **spans_out)
	{
		if (faces[0]) return faces[0]->GetColumn(column, spans_out);
		return NULL;
	}
	virtual const BYTE *GetPixels ()
	{
		if (faces[0]) return faces[0]->GetPixels();
		return NULL;
	}
	virtual int CopyTrueColorPixels(FBitmap *bmp, int x, int y, int rotate, FCopyInfo *inf)
	{
		if (faces[0]) return faces[0]->CopyTrueColorPixels(bmp, x, y, rotate, inf);
		return 0;
	}
	bool UseBasePalette() { return false; }	// not really but here it's not important.

	void SetSize()
	{
		if (faces[0]) 
		{
			Width=faces[0]->GetWidth();
			Height=faces[0]->GetHeight();
			CalcBitSize();
		}
	}

	virtual void Unload () 
	{
		for(int i=0;i<6;i++) if (faces[i]) faces[i]->Unload();
	}

	void PrecacheGL()
	{
		for(int i=0;i<6;i++) if (faces[i]) faces[i]->PrecacheGL();
	}
};

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void gl_ParseSkybox(FScanner &sc)
{
	int facecount=0;

	sc.MustGetString();

	FSkyBox * sb = new FSkyBox;
	uppercopy(sb->Name, sc.String);
	sb->Name[8]=0;
	if (sc.CheckString("fliptop"))
	{
		sb->fliptop = true;
	}
	sc.MustGetStringName("{");
	while (!sc.CheckString("}"))
	{
		sc.MustGetString();
		if (facecount<6) 
		{
			sb->faces[facecount] = TexMan[TexMan.GetTexture(sc.String, FTexture::TEX_Wall, FTextureManager::TEXMAN_TryAny|FTextureManager::TEXMAN_Overridable)];
		}
		facecount++;
	}
	if (facecount != 3 && facecount != 6)
	{
		sc.ScriptError("%s: Skybox definition requires either 3 or 6 faces", sb->Name);
	}
	sb->SetSize();
	TexMan.AddTexture(sb);
}

//-----------------------------------------------------------------------------
//
// Shamelessly lifted from Doomsday (written by Jaakko Keränen)
// also shamelessly lifted from ZDoomGL! ;)
//
//-----------------------------------------------------------------------------

CVAR (Int, gl_sky_detail, 16, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
EXTERN_CVAR (Bool, r_stretchsky)

extern int skyfog;

//===========================================================================
// 
//	Gets the average color of a texture for use as a sky cap color
//
//===========================================================================
static PalEntry SkyCapColor(FTextureID texno, bool bottom)
{
	PalEntry col;

	FTexture *tex = TexMan[texno];
	if (!tex) return 0;

	if (!tex->gl_info.bSkyColorDone)
	{
		tex->gl_info.bSkyColorDone = true;

		FGLTexture * gltex = FGLTexture::ValidateTexture(tex);
		if (tex)
		{
			int w;
			int h;
			unsigned char * buffer = gltex->CreateTexBuffer(FGLTexture::GLUSE_TEXTURE, CM_DEFAULT, 0, w, h);

			if (buffer)
			{
				tex->gl_info.CeilingSkyColor = averageColor((DWORD *) buffer, w * MIN(30, h), 0);
				if (h>30)
				{
					tex->gl_info.FloorSkyColor = averageColor(((DWORD *) buffer)+(h-30)*w, w * 30, 0);
				}
				else tex->gl_info.FloorSkyColor = tex->gl_info.CeilingSkyColor;
				delete buffer;
			}
		}
	}
	return bottom? tex->gl_info.FloorSkyColor : tex->gl_info.CeilingSkyColor;
}




// The texture offset to be applied to the texture coordinates in SkyVertex().
static angle_t maxSideAngle = ANGLE_180 / 3;
static float texoff;
static int rows, columns;	
static fixed_t scale = 10000 << FRACBITS;
static bool yflip;
static int texw;
static float yMult, yAdd;
static bool foglayer;
static bool secondlayer;
static float R,G,B;

#define SKYHEMI_UPPER		0x1
#define SKYHEMI_LOWER		0x2
#define SKYHEMI_JUST_CAP	0x4	// Just draw the top or bottom cap.


//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

static void SkyVertex(int r, int c)
{
	angle_t topAngle= (angle_t)(c / (float)columns * ANGLE_MAX);
	angle_t sideAngle = maxSideAngle * (rows - r) / rows;
	fixed_t height = finesine[sideAngle>>ANGLETOFINESHIFT];
	fixed_t realRadius = FixedMul(scale, finecosine[sideAngle>>ANGLETOFINESHIFT]);
	fixed_t x = FixedMul(realRadius, finecosine[topAngle>>ANGLETOFINESHIFT]);
	fixed_t y = (!yflip) ? FixedMul(scale, height) : FixedMul(scale, height) * -1;
	fixed_t z = FixedMul(realRadius, finesine[topAngle>>ANGLETOFINESHIFT]);
	float fx, fy, fz;
	float color = r * 1.f / rows;
	float u, v;
	float timesRepeat;
	
	timesRepeat = (short)(4 * (256.f / texw));
	if (timesRepeat == 0.f) timesRepeat = 1.f;
	
	if (!foglayer)
	{
		gl_SetColor(255, 0, NULL, r==0? 0.0f : 1.0f);
		
		// And the texture coordinates.
		if(!yflip)	// Flipped Y is for the lower hemisphere.
		{
			u = (-timesRepeat * c / (float)columns) ;//* yMult;
			v = (r / (float)rows) * 1.f * yMult + yAdd;
		}
		else
		{
			u = (-timesRepeat * c / (float)columns) ;//* yMult;
			v = ((rows-r)/(float)rows) * 1.f * yMult + yAdd;
		}
		
		
		gl.TexCoord2f(u, v);
	}
	// And finally the vertex.
	fx =-TO_GL(x);	// Doom mirrors the sky vertically!
	fy = TO_GL(y);
	fz = TO_GL(z);
	gl.Vertex3f(fx, fy - 1.f, fz);
}


//-----------------------------------------------------------------------------
//
// Hemi is Upper or Lower. Zero is not acceptable.
// The current texture is used. SKYHEMI_NO_TOPCAP can be used.
//
//-----------------------------------------------------------------------------

static void RenderSkyHemisphere(int hemi)
{
	int r, c;
	
	if (hemi & SKYHEMI_LOWER)
	{
		yflip = true;
	}
	else
	{
		yflip = false;
	}
	
	// The top row (row 0) is the one that's faded out.
	// There must be at least 4 columns. The preferable number
	// is 4n, where n is 1, 2, 3... There should be at least
	// two rows because the first one is always faded.
	rows = 4;
	
	if (hemi & SKYHEMI_JUST_CAP)
	{
		return;
	}

	gl_DisableShader();

	// Draw the cap as one solid color polygon
	if (!foglayer)
	{
		columns = 4 * (gl_sky_detail > 0 ? gl_sky_detail : 1);
		foglayer=true;
		gl_EnableTexture(false);


		if (!secondlayer)
		{
			gl.Color3f(R, G ,B);
			gl.Begin(GL_TRIANGLE_FAN);
			for(c = 0; c < columns; c++)
			{
				SkyVertex(1, c);
			}
			gl.End();
		}

		gl_EnableTexture(true);
		foglayer=false;
		gl_ApplyShader();
	}
	else
	{
		columns=4;	// no need to do more!
		gl.Begin(GL_TRIANGLE_FAN);
		for(c = 0; c < columns; c++)
		{
			SkyVertex(0, c);
		}
		gl.End();
	}
	
	// The total number of triangles per hemisphere can be calculated
	// as follows: rows * columns * 2 + 2 (for the top cap).
	for(r = 0; r < rows; r++)
	{
		if (yflip)
		{
			gl.Begin(GL_TRIANGLE_STRIP);
            SkyVertex(r + 1, 0);
			SkyVertex(r, 0);
			for(c = 1; c <= columns; c++)
			{
				SkyVertex(r + 1, c);
				SkyVertex(r, c);
			}
			gl.End();
		}
		else
		{
			gl.Begin(GL_TRIANGLE_STRIP);
            SkyVertex(r, 0);
			SkyVertex(r + 1, 0);
			for(c = 1; c <= columns; c++)
			{
				SkyVertex(r, c);
				SkyVertex(r + 1, c);
			}
			gl.End();
		}
	}
}


//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

static void RenderDome(FTextureID texno, FGLTexture * tex, float x_offset, float y_offset, int CM_Index)
{
	int texh;

	bool skystretch = (r_stretchsky && !(level.flags & LEVEL_FORCENOSKYSTRETCH));


	if (tex)
	{
		tex->Bind(CM_Index, 0, 0);
		texw = tex->TextureWidth(FGLTexture::GLUSE_TEXTURE);
		texh = tex->TextureHeight(FGLTexture::GLUSE_TEXTURE);

		if (texh>190 && skystretch) texh=190;

		gl.Rotatef(-180.0f+x_offset, 0.f, 1.f, 0.f);

		yAdd = y_offset/texh;

		if (texh<=180) // && skystretch)
		{
			yMult=1.0f;
			if (!skystretch)
				gl.Scalef(1.f, texh/180.f, 1.f);
		}
		else
		{
			yMult= 180.0f/texh;
		}
	}

	if (tex && !secondlayer) 
	{
		PalEntry pe = SkyCapColor(texno, false);
		if (CM_Index!=CM_DEFAULT) ModifyPalette(&pe, &pe, CM_Index, 1);

		R=pe.r/255.0f;
		G=pe.g/255.0f;
		B=pe.b/255.0f;

		if (gl_fixedcolormap)
		{
			float rr, gg, bb;

			gl_GetLightColor(255, 0, NULL, &rr, &gg, &bb);
			R*=rr;
			G*=gg;
			B*=bb;
		}
	}

	RenderSkyHemisphere(SKYHEMI_UPPER);

	if(tex)
	{
		yAdd = y_offset/texh;

		if (texh<=180)
		{
			yMult=1.0f;
		}
		else
		{
			yAdd+=180.0f/texh;
		}
	}

	if (tex && !secondlayer) 
	{
		PalEntry pe = SkyCapColor(texno, true);
		if (CM_Index!=CM_DEFAULT) ModifyPalette(&pe, &pe, CM_Index, 1);
		R=pe.r/255.0f;
		G=pe.g/255.0f;
		B=pe.b/255.0f;

		if (fixedcolormap)
		{
			float rr,gg,bb;

			gl_GetLightColor(255, 0, NULL, &rr, &gg, &bb);
			R*=rr;
			G*=gg;
			B*=bb;
		}
	}

	RenderSkyHemisphere(SKYHEMI_LOWER);

	if (tex)
	{
		gl.Rotatef(180.0f-x_offset, 0, 1, 0);
		gl.Scalef(1.f, 1.f, 1.f);
	}
}


//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

static void RenderBox(FTextureID texno, FGLTexture * gltex, float x_offset, int CM_Index)
{
	FSkyBox * sb = static_cast<FSkyBox*>(gltex->tex);
	int faces;
	FGLTexture * tex;

	gl.Rotatef(-180.0f+x_offset, glset.skyrotatevector.X, glset.skyrotatevector.Z, glset.skyrotatevector.Y);
	gl.Color3f(R, G ,B);

	if (sb->faces[5]) 
	{
		faces=4;

		// north
		tex = FGLTexture::ValidateTexture(sb->faces[0]);
		tex->BindPatch(CM_Index, 0);
		gl_ApplyShader();
		gl.Begin(GL_TRIANGLE_FAN);
		gl.TexCoord2f(0, 0);
		gl.Vertex3f(128.f, 128.f, -128.f);
		gl.TexCoord2f(1, 0);
		gl.Vertex3f(-128.f, 128.f, -128.f);
		gl.TexCoord2f(1, 1);
		gl.Vertex3f(-128.f, -128.f, -128.f);
		gl.TexCoord2f(0, 1);
		gl.Vertex3f(128.f, -128.f, -128.f);
		gl.End();

		// east
		tex = FGLTexture::ValidateTexture(sb->faces[1]);
		tex->BindPatch(CM_Index, 0);
		gl_ApplyShader();
		gl.Begin(GL_TRIANGLE_FAN);
		gl.TexCoord2f(0, 0);
		gl.Vertex3f(-128.f, 128.f, -128.f);
		gl.TexCoord2f(1, 0);
		gl.Vertex3f(-128.f, 128.f, 128.f);
		gl.TexCoord2f(1, 1);
		gl.Vertex3f(-128.f, -128.f, 128.f);
		gl.TexCoord2f(0, 1);
		gl.Vertex3f(-128.f, -128.f, -128.f);
		gl.End();

		// south
		tex = FGLTexture::ValidateTexture(sb->faces[2]);
		tex->BindPatch(CM_Index, 0);
		gl_ApplyShader();
		gl.Begin(GL_TRIANGLE_FAN);
		gl.TexCoord2f(0, 0);
		gl.Vertex3f(-128.f, 128.f, 128.f);
		gl.TexCoord2f(1, 0);
		gl.Vertex3f(128.f, 128.f, 128.f);
		gl.TexCoord2f(1, 1);
		gl.Vertex3f(128.f, -128.f, 128.f);
		gl.TexCoord2f(0, 1);
		gl.Vertex3f(-128.f, -128.f, 128.f);
		gl.End();

		// west
		tex = FGLTexture::ValidateTexture(sb->faces[3]);
		tex->BindPatch(CM_Index, 0);
		gl_ApplyShader();
		gl.Begin(GL_TRIANGLE_FAN);
		gl.TexCoord2f(0, 0);
		gl.Vertex3f(128.f, 128.f, 128.f);
		gl.TexCoord2f(1, 0);
		gl.Vertex3f(128.f, 128.f, -128.f);
		gl.TexCoord2f(1, 1);
		gl.Vertex3f(128.f, -128.f, -128.f);
		gl.TexCoord2f(0, 1);
		gl.Vertex3f(128.f, -128.f, 128.f);
		gl.End();
	}
	else 
	{
		faces=1;
		// all 4 sides
		tex = FGLTexture::ValidateTexture(sb->faces[0]);
		tex->BindPatch(CM_Index, 0);

		gl_ApplyShader();
		gl.Begin(GL_TRIANGLE_FAN);
		gl.TexCoord2f(0, 0);
		gl.Vertex3f(128.f, 128.f, -128.f);
		gl.TexCoord2f(.25f, 0);
		gl.Vertex3f(-128.f, 128.f, -128.f);
		gl.TexCoord2f(.25f, 1);
		gl.Vertex3f(-128.f, -128.f, -128.f);
		gl.TexCoord2f(0, 1);
		gl.Vertex3f(128.f, -128.f, -128.f);
		gl.End();

		// east
		gl.Begin(GL_TRIANGLE_FAN);
		gl.TexCoord2f(.25f, 0);
		gl.Vertex3f(-128.f, 128.f, -128.f);
		gl.TexCoord2f(.5f, 0);
		gl.Vertex3f(-128.f, 128.f, 128.f);
		gl.TexCoord2f(.5f, 1);
		gl.Vertex3f(-128.f, -128.f, 128.f);
		gl.TexCoord2f(.25f, 1);
		gl.Vertex3f(-128.f, -128.f, -128.f);
		gl.End();

		// south
		gl.Begin(GL_TRIANGLE_FAN);
		gl.TexCoord2f(.5f, 0);
		gl.Vertex3f(-128.f, 128.f, 128.f);
		gl.TexCoord2f(.75f, 0);
		gl.Vertex3f(128.f, 128.f, 128.f);
		gl.TexCoord2f(.75f, 1);
		gl.Vertex3f(128.f, -128.f, 128.f);
		gl.TexCoord2f(.5f, 1);
		gl.Vertex3f(-128.f, -128.f, 128.f);
		gl.End();

		// west
		gl.Begin(GL_TRIANGLE_FAN);
		gl.TexCoord2f(.75f, 0);
		gl.Vertex3f(128.f, 128.f, 128.f);
		gl.TexCoord2f(1, 0);
		gl.Vertex3f(128.f, 128.f, -128.f);
		gl.TexCoord2f(1, 1);
		gl.Vertex3f(128.f, -128.f, -128.f);
		gl.TexCoord2f(.75f, 1);
		gl.Vertex3f(128.f, -128.f, 128.f);
		gl.End();
	}

	// top
	tex = FGLTexture::ValidateTexture(sb->faces[faces]);
	tex->BindPatch(CM_Index, 0);
	gl_ApplyShader();
	gl.Begin(GL_TRIANGLE_FAN);
	if (!sb->fliptop)
	{
		gl.TexCoord2f(0, 0);
		gl.Vertex3f(128.f, 128.f, -128.f);
		gl.TexCoord2f(1, 0);
		gl.Vertex3f(-128.f, 128.f, -128.f);
		gl.TexCoord2f(1, 1);
		gl.Vertex3f(-128.f, 128.f, 128.f);
		gl.TexCoord2f(0, 1);
		gl.Vertex3f(128.f, 128.f, 128.f);
	}
	else
	{
		gl.TexCoord2f(0, 0);
		gl.Vertex3f(128.f, 128.f, 128.f);
		gl.TexCoord2f(1, 0);
		gl.Vertex3f(-128.f, 128.f, 128.f);
		gl.TexCoord2f(1, 1);
		gl.Vertex3f(-128.f, 128.f, -128.f);
		gl.TexCoord2f(0, 1);
		gl.Vertex3f(128.f, 128.f, -128.f);
	}
	gl.End();


	// bottom
	tex = FGLTexture::ValidateTexture(sb->faces[faces+1]);
	tex->BindPatch(CM_Index, 0);
	gl_ApplyShader();
	gl.Begin(GL_TRIANGLE_FAN);
	gl.TexCoord2f(0, 0);
	gl.Vertex3f(128.f, -128.f, -128.f);
	gl.TexCoord2f(1, 0);
	gl.Vertex3f(-128.f, -128.f, -128.f);
	gl.TexCoord2f(1, 1);
	gl.Vertex3f(-128.f, -128.f, 128.f);
	gl.TexCoord2f(0, 1);
	gl.Vertex3f(128.f, -128.f, 128.f);
	gl.End();


}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------
void GLSkyPortal::DrawContents()
{
	bool drawBoth = false;
	int CM_Index;
	PalEntry FadeColor(0,0,0,0);

	if (gl_fixedcolormap) 
	{
		CM_Index=gl_fixedcolormap<CM_LIMIT? gl_fixedcolormap:CM_DEFAULT;
	}
	else 
	{
		CM_Index=CM_DEFAULT;
		FadeColor=origin->fadecolor;
	}

	gl_EnableFog(false);
	gl.Disable(GL_ALPHA_TEST);
	gl.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	gl.PushMatrix();
	gl_SetupView(0, 0, 0, viewangle, !!(MirrorFlag&1), !!(PlaneMirrorFlag&1), true);

	if (origin->texture[0] && origin->texture[0]->tex->gl_info.bSkybox)
	{
		if (fixedcolormap)
		{						
			float rr,gg,bb;

			gl_GetLightColor(255, 0, NULL, &rr, &gg, &bb);
			R=rr;
			G=gg;
			B=bb;
		}
		else R=G=B=1.f;

		RenderBox(origin->skytexno1, origin->texture[0], origin->x_offset[0], CM_Index);
		gl.Enable(GL_ALPHA_TEST);
	}
	else
	{
		if (origin->texture[0]==origin->texture[1] && origin->doublesky) origin->doublesky=false;	

		gl.Translatef(0.f, -1000.f, 0.f);

		if (origin->texture[0])
		{
			gl_SetTextureMode(TM_OPAQUE);
			RenderDome(origin->skytexno1, origin->texture[0], origin->x_offset[0], origin->y_offset, CM_Index);
			gl_SetTextureMode(TM_MODULATE);
		}
		
		gl.Enable(GL_ALPHA_TEST);
		gl.AlphaFunc(GL_GEQUAL,0.05f);
		
		if (origin->doublesky && origin->texture[1])
		{
			secondlayer=true;
			RenderDome(FNullTextureID(), origin->texture[1], origin->x_offset[1], origin->y_offset, CM_Index);
			secondlayer=false;
		}

		if (skyfog>0 && (FadeColor.r ||FadeColor.g || FadeColor.b))
		{
			gl_EnableTexture(false);
			foglayer=true;
			gl.Color4f(FadeColor.r/255.0f,FadeColor.g/255.0f,FadeColor.b/255.0f,skyfog/255.0f);
			RenderDome(FNullTextureID(), NULL, 0, 0, CM_DEFAULT);
			gl_EnableTexture(true);
			foglayer=false;
		}
	}
	gl.PopMatrix();
}

