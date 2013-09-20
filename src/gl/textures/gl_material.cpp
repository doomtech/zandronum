/*
** gltexture.cpp
** The texture classes for hardware rendering
** (Even though they are named 'gl' there is nothing hardware dependent
**  in this file. That is all encapsulated in the FHardwareTexture class.)
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
#include "w_wad.h"
#include "m_png.h"
#include "r_draw.h"
#include "sbar.h"
#include "gi.h"
#include "cmdlib.h"
#include "stats.h"
#include "r_main.h"
#include "templates.h"
#include "sc_man.h"
#include "r_translate.h"
#include "colormatcher.h"

#include "gl/gl_struct.h"
#include "gl/common/glc_data.h"
#include "gl/gl_intern.h"
#include "gl/gl_framebuffer.h"
#include "gl/old_renderer/gl1_renderer.h"
#include "gl/gl_functions.h"
#include "gl/old_renderer/gl1_shader.h"
#include "gl/common/glc_translate.h"

#include "gl/textures/gl_texture.h"
#include "gl/textures/gl_bitmap.h"
#include "gl/textures/gl_material.h"


EXTERN_CVAR(Bool, gl_render_precise)
EXTERN_CVAR(Int, gl_lightmode)
EXTERN_CVAR(Bool, gl_precache)
EXTERN_CVAR(Bool, gl_texture_usehires)

//===========================================================================
//
// The GL texture maintenance class
//
//===========================================================================

//===========================================================================
//
// Constructor
//
//===========================================================================
FGLTexture::FGLTexture(FTexture * tx, bool expandpatches)
{
	assert(tx->gl_info.SystemTexture == NULL);
	tex = tx;

	glpatch=NULL;
	gltexture=NULL;

	HiresLump=-1;
	hirestexture = NULL;

	currentwarp = 0;

	bHasColorkey = false;

	tempScaleX = tempScaleY = FRACUNIT;

	for (int i=GLUSE_PATCH; i<=GLUSE_TEXTURE; i++)
	{
		Width[i] = tex->GetWidth();
		Height[i] = tex->GetHeight();
		LeftOffset[i] = tex->LeftOffset;
		TopOffset[i] = tex->TopOffset;
		RenderWidth[i] = tex->GetScaledWidth();
		RenderHeight[i] = tex->GetScaledHeight();
	}

	scalex = tex->xScale/(float)FRACUNIT;
	scaley = tex->yScale/(float)FRACUNIT;

	// a little adjustment to make sprites look better with texture filtering:
	// create a 1 pixel wide empty frame around them.
	if (expandpatches)
	{
		RenderWidth[GLUSE_PATCH]+=2;
		RenderHeight[GLUSE_PATCH]+=2;
		Width[GLUSE_PATCH]+=2;
		Height[GLUSE_PATCH]+=2;
		LeftOffset[GLUSE_PATCH]+=1;
		TopOffset[GLUSE_PATCH]+=1;
	}

	bIsTransparent = -1;

	if (tex->bHasCanvas) scaley=-scaley;
	tex->gl_info.SystemTexture = this;
}

//===========================================================================
//
// Destructor
//
//===========================================================================

FGLTexture::~FGLTexture()
{
	//if (tex != NULL && tex->gl_info.SystemTexture == this) tex->gl_info.SystemTexture = NULL;
	Clean(true);
	if (hirestexture) delete hirestexture;
}

//==========================================================================
//
// Checks for the presence of a hires texture replacement and loads it
//
//==========================================================================
unsigned char *FGLTexture::LoadHiresTexture(int *width, int *height, int cm)
{
	if (HiresLump==-1) 
	{
		bHasColorkey = false;
		HiresLump = CheckDDPK3(tex);
		if (HiresLump < 0) HiresLump = CheckExternalFile(tex, bHasColorkey);

		if (HiresLump >=0) 
		{
			hirestexture = FTexture::CreateTexture(HiresLump, FTexture::TEX_Any);
		}
	}
	if (hirestexture != NULL)
	{
		int w=hirestexture->GetWidth();
		int h=hirestexture->GetHeight();

		unsigned char * buffer=new unsigned char[w*(h+1)*4];
		memset(buffer, 0, w * (h+1) * 4);

		FGLBitmap bmp(buffer, w*4, w, h);
		bmp.SetTranslationInfo(cm);

		
		int trans = hirestexture->CopyTrueColorPixels(&bmp, 0, 0);
		tex->CheckTrans(buffer, w*h, trans);
		bIsTransparent = tex->gl_info.mIsTransparent;

		if (bHasColorkey)
		{
			// This is a crappy Doomsday color keyed image
			// We have to remove the key manually. :(
			DWORD * dwdata=(DWORD*)buffer;
			for (int i=(w*h);i>0;i--)
			{
				if (dwdata[i]==0xffffff00 || dwdata[i]==0xffff00ff) dwdata[i]=0;
			}
		}
		*width = w;
		*height = h;
		return buffer;
	}
	return NULL;
}

//===========================================================================
// 
//	Deletes all allocated resources
//
//===========================================================================

void FGLTexture::Clean(bool all)
{
	WorldTextureInfo::Clean(all);
	PatchTextureInfo::Clean(all);
}

//===========================================================================
//
// FGLTex::WarpBuffer
//
//===========================================================================

BYTE *FGLTexture::WarpBuffer(BYTE *buffer, int Width, int Height, int warp)
{
	if (Width > 256 || Height > 256) return buffer;

	DWORD *in = (DWORD*)buffer;
	DWORD *out = (DWORD*)new BYTE[4*Width*Height];
	float Speed = static_cast<FWarpTexture*>(tex)->GetSpeed();

	static_cast<FWarpTexture*>(tex)->GenTime = r_FrameTime;

	static DWORD linebuffer[256];	// anything larger will bring down performance so it is excluded above.
	DWORD timebase = DWORD(r_FrameTime*Speed*23/28);
	int xsize = Width;
	int ysize = Height;
	int xmask = xsize - 1;
	int ymask = ysize - 1;
	int ds_xbits;
	int i,x;

	if (warp == 1)
	{
		for(ds_xbits=-1,i=Width; i; i>>=1, ds_xbits++);

		for (x = xsize-1; x >= 0; x--)
		{
			int yt, yf = (finesine[(timebase+(x+17)*128)&FINEMASK]>>13) & ymask;
			const DWORD *source = in + x;
			DWORD *dest = out + x;
			for (yt = ysize; yt; yt--, yf = (yf+1)&ymask, dest += xsize)
			{
				*dest = *(source+(yf<<ds_xbits));
			}
		}
		timebase = DWORD(r_FrameTime*Speed*32/28);
		int y;
		for (y = ysize-1; y >= 0; y--)
		{
			int xt, xf = (finesine[(timebase+y*128)&FINEMASK]>>13) & xmask;
			DWORD *source = out + (y<<ds_xbits);
			DWORD *dest = linebuffer;
			for (xt = xsize; xt; xt--, xf = (xf+1)&xmask)
			{
				*dest++ = *(source+xf);
			}
			memcpy (out+y*xsize, linebuffer, xsize*sizeof(DWORD));
		}
	}
	else
	{
		int ybits;
		for(ybits=-1,i=ysize; i; i>>=1, ybits++);

		DWORD timebase = (r_FrameTime * Speed * 40 / 28);
		for (x = xsize-1; x >= 0; x--)
		{
			for (int y = ysize-1; y >= 0; y--)
			{
				int xt = (x + 128
					+ ((finesine[(y*128 + timebase*5 + 900) & 8191]*2)>>FRACBITS)
					+ ((finesine[(x*256 + timebase*4 + 300) & 8191]*2)>>FRACBITS)) & xmask;
				int yt = (y + 128
					+ ((finesine[(y*128 + timebase*3 + 700) & 8191]*2)>>FRACBITS)
					+ ((finesine[(x*256 + timebase*4 + 1200) & 8191]*2)>>FRACBITS)) & ymask;
				const DWORD *source = in + (xt << ybits) + yt;
				DWORD *dest = out + (x << ybits) + y;
				*dest = *source;
			}
		}
	}
	delete [] buffer;
	return (BYTE*)out;
}

//===========================================================================
// 
//	Initializes the buffer for the texture data
//
//===========================================================================

unsigned char * FGLTexture::CreateTexBuffer(ETexUse use, int _cm, int translation, int & w, int & h, bool allowhires, int warp)
{
	unsigned char * buffer;
	intptr_t cm = _cm;
	int W, H;


	// Textures that are already scaled in the texture lump will not get replaced
	// by hires textures
	if (gl_texture_usehires && allowhires && scalex==1.f && scaley==1.f)
	{
		buffer = LoadHiresTexture (&w, &h, _cm);
		if (buffer)
		{
			return buffer;
		}
	}

	W = w = Width[use];
	H = h = Height[use];


	buffer=new unsigned char[W*(H+1)*4];
	memset(buffer, 0, W * (H+1) * 4);

	FGLBitmap bmp(buffer, W*4, W, H);
	bmp.SetTranslationInfo(cm, translation);

	if (tex->bComplex)
	{
		FBitmap imgCreate;

		// The texture contains special processing so it must be composited using with the
		// base bitmap class and then be converted as a whole.
		if (imgCreate.Create(W, H))
		{
			memset(imgCreate.GetPixels(), 0, W * H * 4);
			int trans = 
				tex->CopyTrueColorPixels(&imgCreate, LeftOffset[use] - tex->LeftOffset, TopOffset[use] - tex->TopOffset);
			bmp.CopyPixelDataRGB(0, 0, imgCreate.GetPixels(), W, H, 4, W * 4, 0, CF_BGRA);
			tex->CheckTrans(buffer, W*H, trans);
			bIsTransparent = tex->gl_info.mIsTransparent;
		}
	}
	else if (translation<=0)
	{
		int trans = 
			tex->CopyTrueColorPixels(&bmp, LeftOffset[use] - tex->LeftOffset, TopOffset[use] - tex->TopOffset);

		tex->CheckTrans(buffer, W*H, trans);
		bIsTransparent = tex->gl_info.mIsTransparent;
	}
	else
	{
		// When using translations everything must be mapped to the base palette.
		// Since FTexture's method is doing exactly that by calling GetPixels let's use that here
		// to do all the dirty work for us. ;)
		tex->FTexture::CopyTrueColorPixels(&bmp, LeftOffset[use] - tex->LeftOffset, TopOffset[use] - tex->TopOffset);
		bIsTransparent = 0;
	}

	// [BB] Potentially upsample the buffer. Note: Even is the buffer is not upsampled,
	// w, h are set to the width and height of the buffer.
	// Also don't upsample warped textures.
	if ( bIsTransparent != 1 && warp==0 )
	{
		// [BB] Potentially upsample the buffer.
		buffer = gl_CreateUpsampledTextureBuffer ( tex, buffer, W, H, w, h, ( bIsTransparent == 1 ) || ( cm == CM_SHADE ) );
	}

	if (warp != 0)
	{
		buffer = WarpBuffer(buffer, w, h, warp);
		currentwarp = warp;
	}

	return buffer;
}


//===========================================================================
// 
//	Gets texture coordinate info for world (wall/flat) textures
//  The wrapper class is there to provide a set of coordinate
//  functions to access the texture
//
//===========================================================================
const WorldTextureInfo * FGLTexture::GetWorldTextureInfo()
{

	if (tex->UseType==FTexture::TEX_Null) return NULL;		// Cannot register a NULL texture!
	if (!gltexture) gltexture=new FHardwareTexture(Width[GLUSE_TEXTURE], Height[GLUSE_TEXTURE], true, true);
	if (gltexture) return (WorldTextureInfo*)this; 	
	return NULL;
}

//===========================================================================
// 
//	Gets texture coordinate info for sprites
//  The wrapper class is there to provide a set of coordinate
//  functions to access the texture
//
//===========================================================================
const PatchTextureInfo * FGLTexture::GetPatchTextureInfo()
{
	if (tex->UseType==FTexture::TEX_Null) return NULL;		// Cannot register a NULL texture!
	if (!glpatch) 
	{
		glpatch=new FHardwareTexture(Width[GLUSE_PATCH], Height[GLUSE_PATCH], false, false);
	}
	if (glpatch) return (PatchTextureInfo*)this; 	
	return NULL;
}

//===========================================================================
// 
//	Binds a texture to the renderer
//
//===========================================================================

const WorldTextureInfo * FGLTexture::Bind(int texunit, int cm, int clampmode, int translation, int warp)
{
	int usebright = false;

	if (translation <= 0) translation = -translation;
	else if (translation == TRANSLATION(TRANSLATION_Standard, 8)) translation = CM_GRAY;
	else if (translation == TRANSLATION(TRANSLATION_Standard, 7)) translation = CM_ICE;
	else translation = GLTranslationPalette::GetInternalTranslation(translation);

	if (GetWorldTextureInfo())
	{
		if (warp != 0 || currentwarp != warp)
		{
			// must recreate the texture
			Clean(true);
			GetWorldTextureInfo();
		}

		// Bind it to the system.
		if (!gltexture->Bind(texunit, cm, translation, clampmode))
		{
			
			int w,h;

			// Create this texture
			unsigned char * buffer = NULL;
			
			if (!tex->bHasCanvas)
			{
				buffer = CreateTexBuffer(GLUSE_TEXTURE, cm, translation, w, h, true, warp);
				tex->ProcessData(buffer, w, h, false);
			}
			if (!gltexture->CreateTexture(buffer, w, h, true, texunit, cm, translation)) 
			{
				// could not create texture
				delete buffer;
				return NULL;
			}
			delete buffer;
		}
		gltexture->SetTextureClamp(clampmode);

		if (tex->bHasCanvas) static_cast<FCanvasTexture*>(tex)->NeedUpdate();
		return (WorldTextureInfo*)this; 
	}
	return NULL;
}

//===========================================================================
// 
//	Binds a sprite to the renderer
//
//===========================================================================
const PatchTextureInfo * FGLTexture::BindPatch(int texunit, int cm, int translation, int warp)
{
	bool usebright = false;
	int transparm = translation;

	if (translation <= 0) translation = -translation;
	else if (translation == TRANSLATION(TRANSLATION_Standard, 8)) translation = CM_GRAY;
	else if (translation == TRANSLATION(TRANSLATION_Standard, 7)) translation = CM_ICE;
	else translation = GLTranslationPalette::GetInternalTranslation(translation);

	if (GetPatchTextureInfo())
	{
		if (warp != 0 || currentwarp != warp)
		{
			// must recreate the texture
			Clean(true);
			GetPatchTextureInfo();
		}

		// Bind it to the system. 
		if (!glpatch->Bind(texunit, cm, translation, -1))
		{
			int w, h;

			// Create this texture
			unsigned char * buffer = CreateTexBuffer(GLUSE_PATCH, cm, translation, w, h, false, warp);
			tex->ProcessData(buffer, w, h, true);
			if (!glpatch->CreateTexture(buffer, w, h, false, texunit, cm, translation)) 
			{
				// could not create texture
				delete buffer;
				return NULL;
			}
			delete buffer;
		}
		if (gl_render_precise)
		{
			gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		}
		return (PatchTextureInfo*)this; 	
	}
	return NULL;
}


//===========================================================================
//
//
//
//===========================================================================
FGLTexture * FMaterial::ValidateSysTexture(FTexture * tex, bool expand)
{
	if (tex	&& tex->UseType!=FTexture::TEX_Null)
	{
		FGLTexture *gltex = tex->gl_info.SystemTexture;
		if (gltex == NULL) 
		{
			gltex = new FGLTexture(tex, expand);
		}
		return gltex;
	}
	return NULL;
}

//===========================================================================
//
// Constructor
//
//===========================================================================
TArray<FMaterial *> FMaterial::mMaterials;

FMaterial::FMaterial(FTexture * tx)
{
	assert(tx->gl_info.Material == NULL);

	bool expanded = tx->UseType == FTexture::TEX_Sprite || 
					tx->UseType == FTexture::TEX_SkinSprite || 
					tx->UseType == FTexture::TEX_Decal;

	tex = tx;

	// FIXME: Should let the texture create the FGLTexture object.
	FGLTexture *gltex = ValidateSysTexture(tx, expanded);

	mTextures.Push(gltex);

	// set default shader. Can be warp or brightmap
	mShaderIndex = tx->bWarped;
	if (!tx->bWarped && gl.shadermodel > 2) 
	{
		tx->CreateDefaultBrightmap();
		if (tx->gl_info.Brightmap != NULL)
		{
			gltex = ValidateSysTexture(tx->gl_info.Brightmap, expanded);
			mTextures.Push(gltex);
			mShaderIndex = 3;
		}
	}
	mTextures.ShrinkToFit();
	mMaxBound = -1;
	mMaterials.Push(this);
	tex->gl_info.Material = this;
}

//===========================================================================
//
// Destructor
//
//===========================================================================

FMaterial::~FMaterial()
{
	//if (tex != NULL && tex->gl_info.Material == this) tex->gl_info.Material = NULL;
	for(unsigned i=0;i<mMaterials.Size();i++)
	{
		if (mMaterials[i]==this) 
		{
			mMaterials.Delete(i);
			break;
		}
	}

}

//===========================================================================
// 
//	Binds a texture to the renderer
//
//===========================================================================

const WorldTextureInfo * FMaterial::Bind(int cm, int clampmode, int translation)
{
	int usebright = false;
	int shaderindex = mShaderIndex;
	int maxbound = 0;

	int softwarewarp = gl_SetupShader(tex->bHasCanvas, shaderindex, cm, tex->bWarped? static_cast<FWarpTexture*>(tex)->GetSpeed() : 1.f);

	const WorldTextureInfo *inf = mTextures[0]->Bind(0, cm, clampmode, translation, softwarewarp);
	if (inf != NULL && shaderindex > 0)
	{
		for(unsigned i=0;i<mTextures.Size();i++)
		{
			mTextures[i]->Bind(i, CM_DEFAULT, clampmode, 0, false);
			maxbound = i;
		}
	}
	// unbind everything from the last texture that's still active
	for(int i=maxbound+1; i<=mMaxBound;i++)
	{
		FHardwareTexture::Unbind(i);
		mMaxBound = maxbound;
	}
	return inf;
}


//===========================================================================
// 
//	Binds a texture to the renderer
//
//===========================================================================

const PatchTextureInfo * FMaterial::BindPatch(int cm, int translation)
{
	int usebright = false;
	int shaderindex = mShaderIndex;
	int maxbound = 0;

	int softwarewarp = gl_SetupShader(tex->bHasCanvas, shaderindex, cm, tex->bWarped? static_cast<FWarpTexture*>(tex)->GetSpeed() : 1.f);

	const PatchTextureInfo *inf = mTextures[0]->BindPatch(0, cm, translation, softwarewarp);
	if (inf != NULL && shaderindex > 0)
	{
		for(unsigned i=0;i<mTextures.Size();i++)
		{
			mTextures[i]->BindPatch(i, CM_DEFAULT, 0, softwarewarp);
			maxbound = i;
		}
	}
	// unbind everything from the last texture that's still active
	for(int i=maxbound+1; i<=mMaxBound;i++)
	{
		FHardwareTexture::Unbind(i);
		mMaxBound = maxbound;
	}
	return inf;
}


//===========================================================================
//
// Untilities
//
//===========================================================================

void FMaterial::SetWallScaling(fixed_t x, fixed_t y)
{
	if (x != mTextures[0]->tempScaleX)
	{
		fixed_t scale_x = FixedMul(x, tex->xScale);
		int foo = (mTextures[0]->Width[GLUSE_TEXTURE] << 17) / scale_x; 
		mTextures[0]->RenderWidth[GLUSE_TEXTURE] = (foo >> 1) + (foo & 1); 
		mTextures[0]->scalex = scale_x/(float)FRACUNIT;
		mTextures[0]->tempScaleX = x;
	}
	if (y != mTextures[0]->tempScaleY)
	{
		fixed_t scale_y = FixedMul(y, tex->yScale);
		int foo = (mTextures[0]->Height[GLUSE_TEXTURE] << 17) / scale_y; 
		mTextures[0]->RenderHeight[GLUSE_TEXTURE] = (foo >> 1) + (foo & 1); 
		mTextures[0]->scaley = scale_y/(float)FRACUNIT;
		mTextures[0]->tempScaleY = y;
	}
}

//===========================================================================
//
//
//
//===========================================================================

fixed_t FMaterial::RowOffset(fixed_t rowoffset) const
{
	if (mTextures[0]->tempScaleX == FRACUNIT)
	{
		if (mTextures[0]->scaley==1.f || tex->bWorldPanning) return rowoffset;
		else return quickertoint(rowoffset/mTextures[0]->scaley);
	}
	else
	{
		if (tex->bWorldPanning) return FixedDiv(rowoffset, mTextures[0]->tempScaleY);
		else return quickertoint(rowoffset/mTextures[0]->scaley);
	}
}

//===========================================================================
//
//
//
//===========================================================================

fixed_t FMaterial::TextureOffset(fixed_t textureoffset) const
{
	if (mTextures[0]->tempScaleX == FRACUNIT)
	{
		if (mTextures[0]->scalex==1.f || tex->bWorldPanning) return textureoffset;
		else return quickertoint(textureoffset/mTextures[0]->scalex);
	}
	else
	{
		if (tex->bWorldPanning) return FixedDiv(textureoffset, mTextures[0]->tempScaleX);
		else return quickertoint(textureoffset/mTextures[0]->scalex);
	}
}


//===========================================================================
//
// Returns the size for which texture offset coordinates are used.
//
//===========================================================================

fixed_t FMaterial::TextureAdjustWidth(ETexUse i) const
{
	if (tex->bWorldPanning) 
	{
		if (i == GLUSE_PATCH || mTextures[0]->tempScaleX == FRACUNIT) return mTextures[0]->RenderWidth[i];
		else return FixedDiv(mTextures[0]->Width[i], mTextures[0]->tempScaleX);
	}
	else return mTextures[0]->Width[i];
}


//===========================================================================
//
// GetRect
//
//===========================================================================

void FMaterial::GetRect(FloatRect * r, ETexUse i) const
{
	r->left=-(float)GetScaledLeftOffset(i);
	r->top=-(float)GetScaledTopOffset(i);
	r->width=(float)TextureWidth(i);
	r->height=(float)TextureHeight(i);
}


//==========================================================================
//
// Gets a texture from the texture manager and checks its validity for
// GL rendering. 
//
//==========================================================================

FMaterial * FMaterial::ValidateTexture(FTexture * tex)
{
	if (tex	&& tex->UseType!=FTexture::TEX_Null)
	{
		FMaterial *gltex = tex->gl_info.Material;
		if (gltex == NULL) 
		{
			gltex = new FMaterial(tex);
		}
		return gltex;
	}
	return NULL;
}

FMaterial * FMaterial::ValidateTexture(FTextureID no, bool translate)
{
	return ValidateTexture(translate? TexMan(no) : TexMan[no]);
}


//==========================================================================
//
// Flushes all hardware dependent data
//
//==========================================================================

void FMaterial::FlushAll()
{
	for(int i=mMaterials.Size()-1;i>=0;i--)
	{
		mMaterials[i]->Clean(true);
	}
}

//==========================================================================
//
// Deletes all hardware dependent data
//
//==========================================================================

void FMaterial::DeleteAll()
{
	for(int i=mMaterials.Size()-1;i>=0;i--)
	{
		mMaterials[i]->tex->gl_info.Material = NULL;
		delete mMaterials[i];
	}
	mMaterials.Clear();
	mMaterials.ShrinkToFit();
}
