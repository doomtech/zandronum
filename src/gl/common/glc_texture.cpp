/*
** Global texture data
**
**---------------------------------------------------------------------------
** Copyright 2004-2005 Christoph Oelckers
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
#include "c_cvars.h"
#include "w_wad.h"
#include "r_data.h"
#include "templates.h"
#include "colormatcher.h"
#include "r_translate.h"
#include "c_dispatch.h"
#include "win32gliface.h"
#include "v_palette.h"
#include "gl/common/glc_renderer.h"
#include "gl/common/glc_texture.h"

// Needed to destroy the old renderer's texture pointer 
#include "gl/old_renderer/gl1_texture.h"



//==========================================================================
//
// Texture CVARs
//
//==========================================================================
CUSTOM_CVAR(Float,gl_texture_filter_anisotropic,1.0f,CVAR_ARCHIVE|CVAR_GLOBALCONFIG|CVAR_NOINITCALL)
{
	if (GLRenderer != NULL) GLRenderer->FlushTextures();
}

CCMD(gl_flush)
{
	if (GLRenderer != NULL) GLRenderer->FlushTextures();
}

CUSTOM_CVAR(Int, gl_texture_filter, 0, CVAR_ARCHIVE|CVAR_GLOBALCONFIG|CVAR_NOINITCALL)
{
	if (self < 0 || self>4) self=4;
	if (GLRenderer != NULL) GLRenderer->FlushTextures();
}

CUSTOM_CVAR(Int, gl_texture_format, 0, CVAR_ARCHIVE|CVAR_GLOBALCONFIG|CVAR_NOINITCALL)
{
	// [BB] The number of available texture modes depends on the GPU capabilities.
	// RFL_TEXTURE_COMPRESSION gives us one additional mode and RFL_TEXTURE_COMPRESSION_S3TC
	// another three.
	int numOfAvailableTextureFormat = 4;
	if ( gl.flags & RFL_TEXTURE_COMPRESSION && gl.flags & RFL_TEXTURE_COMPRESSION_S3TC )
		numOfAvailableTextureFormat = 8;
	else if ( gl.flags & RFL_TEXTURE_COMPRESSION )
		numOfAvailableTextureFormat = 5;
	if (self < 0 || self > numOfAvailableTextureFormat-1) self=0;
	GLRenderer->FlushTextures();
}

CUSTOM_CVAR(Bool, gl_texture_usehires, true, CVAR_ARCHIVE|CVAR_NOINITCALL)
{
	if (GLRenderer != NULL) GLRenderer->FlushTextures();
}

CVAR(Bool, gl_precache, false, CVAR_ARCHIVE)
CVAR(Bool, gl_clamp_per_texture, false,  CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

TexFilter_s TexFilter[]={
	{GL_NEAREST,					GL_NEAREST,		false},
	{GL_NEAREST_MIPMAP_NEAREST,		GL_NEAREST,		true},
	{GL_LINEAR,						GL_LINEAR,		false},
	{GL_LINEAR_MIPMAP_NEAREST,		GL_LINEAR,		true},
	{GL_LINEAR_MIPMAP_LINEAR,		GL_LINEAR,		true},
};

int TexFormat[]={
	GL_RGBA8,
	GL_RGB5_A1,
	GL_RGBA4,
	GL_RGBA2,
	// [BB] Added compressed texture formats.
	GL_COMPRESSED_RGBA_ARB,
	GL_COMPRESSED_RGBA_S3TC_DXT1_EXT,
	GL_COMPRESSED_RGBA_S3TC_DXT3_EXT,
	GL_COMPRESSED_RGBA_S3TC_DXT5_EXT,
};



bool HasGlobalBrightmap;
FRemapTable GlobalBrightmap;

//===========================================================================
// 
// Examines the colormap to see if some of the colors have to be
// considered fullbright all the time.
//
//===========================================================================

void gl_GenerateGlobalBrightmapFromColormap()
{
	int lump = Wads.CheckNumForName("COLORMAP");
	if (lump == -1) lump = Wads.CheckNumForName("COLORMAP", ns_colormaps);
	if (lump == -1) return;
	FMemLump cmap = Wads.ReadLump(lump);
	FMemLump palette = Wads.ReadLump("PLAYPAL");
	const unsigned char *cmapdata = (const unsigned char *)cmap.GetMem();
	const unsigned char *paldata = (const unsigned char *)palette.GetMem();

	const int black = 0;
	const int white = ColorMatcher.Pick(255,255,255);


	GlobalBrightmap.MakeIdentity();
	memset(GlobalBrightmap.Remap, white, 256);
	for(int i=0;i<256;i++) GlobalBrightmap.Palette[i]=PalEntry(255,255,255,255);
	for(int j=0;j<32;j++)
	{
		for(int i=0;i<256;i++)
		{
			// the palette comparison should be for ==0 but that gives false positives with Heretic
			// and Hexen.
			if (cmapdata[i+j*256]!=i || (paldata[3*i]<10 && paldata[3*i+1]<10 && paldata[3*i+2]<10))
			{
				GlobalBrightmap.Remap[i]=black;
				GlobalBrightmap.Palette[i]=PalEntry(0,0,0);
			}
		}
	}
	for(int i=0;i<256;i++)
	{
		HasGlobalBrightmap |= GlobalBrightmap.Remap[i] == white;
		if (GlobalBrightmap.Remap[i] == white) DPrintf("Marked color %d as fullbright\n",i);
	}
}

//===========================================================================
//
// averageColor
//  input is RGBA8 pixel format.
//	The resulting RGB color can be scaled uniformly so that the highest 
//	component becomes one.
//
//===========================================================================
PalEntry averageColor(const DWORD *data, int size, fixed_t maxout_factor)
{
	int				i;
	unsigned int	r, g, b;



	// First clear them.
	r = g = b = 0;
	if (size==0) 
	{
		return PalEntry(255,255,255);
	}
	for(i = 0; i < size; i++)
	{
		r += BPART(data[i]);
		g += GPART(data[i]);
		b += RPART(data[i]);
	}

	r = r/size;
	g = g/size;
	b = b/size;

	int maxv=MAX(MAX(r,g),b);

	if(maxv && maxout_factor)
	{
		maxout_factor = FixedMul(maxout_factor, 255);
		r = Scale(r, maxout_factor, maxv);
		g = Scale(g, maxout_factor, maxv);
		b = Scale(b, maxout_factor, maxv);
	}
	return PalEntry(r,g,b);
}


//===========================================================================
//
// Camera texture rendering
//
//===========================================================================

void FCanvasTexture::RenderGLView (AActor *viewpoint, int fov)
{
	GLRenderer->RenderTextureView(this, viewpoint, fov);
	bNeedsUpdate = false;
	bDidUpdate = true;
	bFirstUpdate = false;
}


//==========================================================================
//
// GL status data for a texture
//
//==========================================================================

FTexture::MiscGLInfo::MiscGLInfo() throw()
{
	bGlowing = false;
	GlowColor = 0;
	GlowHeight = 128;
	bSkybox = false;
	FloorSkyColor = 0;
	CeilingSkyColor = 0;
	bFullbright = false;
	bSkyColorDone = false;
	bBrightmapChecked = false;
	bBrightmap = false;
	bBrightmapDisablesFullbright = false;

	GLTexture = NULL;
	Brightmap = NULL;
}

FTexture::MiscGLInfo::~MiscGLInfo()
{
	if (GLTexture != NULL) delete GLTexture;
	GLTexture = NULL;
	if (Brightmap != NULL) delete Brightmap;
	Brightmap = NULL;
}

//==========================================================================
//
// Precaches a GL texture
//
//==========================================================================

void FTexture::PrecacheGL()
{
	if (gl_precache)
	{
		GLRenderer->PrecacheTexture(this);
	}
}

//==========================================================================
//
// Precaches a GL texture
//
//==========================================================================

void FTexture::UncacheGL()
{
	GLRenderer->UncacheTexture(this);
}

//==========================================================================
//
// Calculates glow color for a texture
//
//==========================================================================

void FTexture::GetGlowColor(float *data)
{
	if (gl_info.bGlowing && gl_info.GlowColor == 0)
	{
		int w, h;
		unsigned char *buffer = GLRenderer->GetTextureBuffer(this, w, h);

		if (buffer)
		{
			gl_info.GlowColor = averageColor((DWORD *) buffer, w*h, 6*FRACUNIT/10);
			delete buffer;
		}

		// Black glow equals nothing so switch glowing off
		if (gl_info.GlowColor == 0) gl_info.bGlowing = false;
	}
	data[0]=gl_info.GlowColor.r/255.0f;
	data[1]=gl_info.GlowColor.g/255.0f;
	data[2]=gl_info.GlowColor.b/255.0f;
}

//===========================================================================
//
// fake brightness maps
// These are generated for textures affected by a colormap with
// fullbright entries.
// These textures are only used internally by the GL renderer so
// all code for software rendering support is missing
//
//===========================================================================

FBrightmapTexture::FBrightmapTexture (FTexture *source)
{
	memcpy(Name, source->Name, 9);
	SourcePic = source;
	CopySize(source);
	bNoDecals = source->bNoDecals;
	Rotations = source->Rotations;
	UseType = source->UseType;
	gl_info.bBrightmap = true;
}

FBrightmapTexture::~FBrightmapTexture ()
{
	Unload();
}

const BYTE *FBrightmapTexture::GetColumn (unsigned int column, const Span **spans_out)
{
	// not needed
	return NULL;
}

const BYTE *FBrightmapTexture::GetPixels ()
{
	// not needed
	return NULL;
}

void FBrightmapTexture::Unload ()
{
}

int FBrightmapTexture::CopyTrueColorPixels(FBitmap *bmp, int x, int y, int rotate, FCopyInfo *inf)
{
	SourcePic->CopyTrueColorTranslated(bmp, x, y, rotate, &GlobalBrightmap);
	return 0;
}


//==========================================================================
//
// Parses a brightmap definition
//
//==========================================================================

void gl_ParseBrightmap(FScanner &sc, int deflump)
{
	int type = FTexture::TEX_Any;
	bool disable_fullbright=false;
	bool thiswad = false;
	bool iwad = false;
	int maplump = -1;
	FString maplumpname;

	sc.MustGetString();
	if (sc.Compare("texture")) type = FTexture::TEX_Wall;
	else if (sc.Compare("flat")) type = FTexture::TEX_Flat;
	else if (sc.Compare("sprite")) type = FTexture::TEX_Sprite;
	else sc.UnGet();

	sc.MustGetString();
	FTextureID no = TexMan.CheckForTexture(sc.String, type);
	FTexture *tex = TexMan[no];

	sc.MustGetToken('{');
	while (!sc.CheckToken('}'))
	{
		sc.MustGetString();
		if (sc.Compare("disablefullbright"))
		{
			// This can also be used without a brightness map to disable
			// fullbright in rotations that only use brightness maps on
			// other angles.
			disable_fullbright = true;
		}
		else if (sc.Compare("thiswad"))
		{
			// only affects textures defined in the WAD containing the definition file.
			thiswad = true;
		}
		else if (sc.Compare ("iwad"))
		{
			// only affects textures defined in the IWAD.
			iwad = true;
		}
		else if (sc.Compare ("map"))
		{
			sc.MustGetString();

			if (maplump >= 0)
			{
				Printf("Multiple brightmap definitions in texture %s\n", tex? tex->Name : "(null)");
			}

			maplump = Wads.CheckNumForFullName(sc.String, true);

			if (maplump==-1) 
				Printf("Brightmap '%s' not found in texture '%s'\n", sc.String, tex? tex->Name : "(null)");

			maplumpname = sc.String;
		}
	}
	if (!tex)
	{
		return;
	}
	if (thiswad || iwad)
	{
		bool useme = false;
		int lumpnum = tex->GetSourceLump();

		if (lumpnum != -1)
		{
			if (iwad && Wads.GetLumpFile(lumpnum) <= FWadCollection::IWAD_FILENUM) useme = true;
			if (thiswad && Wads.GetLumpFile(lumpnum) == deflump) useme = true;
		}
		if (!useme) return;
	}

	if (maplump != -1)
	{
		FTexture *brightmap = FTexture::CreateTexture(maplump, tex->UseType);
		if (!brightmap)
		{
			Printf("Unable to create texture from '%s' in brightmap definition for '%s'\n", 
				maplumpname.GetChars(), tex->Name);
			return;
		}
		if (tex->gl_info.Brightmap != NULL)
		{
			// If there is already a brightmap assigned replace it
			delete tex->gl_info.Brightmap;
		}
		tex->gl_info.Brightmap = brightmap;
		brightmap->gl_info.bBrightmap = true;
	}	
	tex->gl_info.bBrightmapDisablesFullbright = disable_fullbright;
}

