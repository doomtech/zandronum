//
//-----------------------------------------------------------------------------
//
// Copyright (C) 2009 Christoph Oelckers
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// As an exception to the GPL this code may be used in GZDoom
// derivatives under the following conditions:
//
// 1. The license of these files is not changed
// 2. Full source of the derived program is disclosed
//
//
// ----------------------------------------------------------------------------
//
// GL texture handling
//

#include "gl/gl_include.h"
#include "gl/gl_intern.h"
#include "textures/textures.h"
#include "textures/bitmap.h"
#include "w_wad.h"
#include "c_cvars.h"
#include "i_system.h"
#include "gl/new_renderer/textures/gl2_bitmap.h"
#include "gl/new_renderer/textures/gl2_texture.h"
#include "gl/new_renderer/textures/gl2_hwtexture.h"
#include "gl/common/glc_texture.h"
#include "gl/common/glc_translate.h"


unsigned char *gl_CreateUpsampledTextureBuffer ( const FTexture *inputTexture, unsigned char *inputBuffer, const int inWidth, const int inHeight, int &outWidth, int &outHeight, bool hasAlpha );


namespace GLRendererNew
{

	//----------------------------------------------------------------------------
	//
	//
	//
	//----------------------------------------------------------------------------

	FGLTexture::FGLTexture()
	{
		mGameTexture = NULL;
		mHWTexture = NULL;
		mOwner = NULL;
		mUsingHires = false;
		mCanUseHires = true;
		mHasColorKey = false;
		HiresTextureLump = -1;
		HiresTexture = NULL;
		mIsTransparent = -1;
	}


	//----------------------------------------------------------------------------
	//
	//
	//
	//----------------------------------------------------------------------------

	FGLTexture::~FGLTexture()
	{
		if (mHWTexture != NULL) delete mHWTexture;
		if (HiresTexture != NULL) delete HiresTexture;
	}

	void FGLTexture::Flush()
	{
		if (mHWTexture != NULL) delete mHWTexture;
		mHWTexture = NULL;
	}

	//----------------------------------------------------------------------------
	//
	//
	//
	//----------------------------------------------------------------------------

	bool FGLTexture::Create(FTexture *tex, bool asSprite, int translation)
	{
		if (mGameTexture == NULL)
		{
			mGameTexture = tex;
			mAsSprite = asSprite;
			mTranslation = translation;
			return true;
		}
		else return false;
	}

	//==========================================================================
	//
	// Checks for the presence of a hires texture replacement and loads it
	//
	//==========================================================================
	unsigned char *FGLTexture::LoadHiresTexture(int *width, int *height)
	{
		int HiresLump = HiresTextureLump;
		if (HiresLump==-1) 
		{
			mHasColorKey = false;
			HiresLump = HiresTextureLump = CheckDDPK3(mGameTexture);
			if (HiresLump < 0) HiresLump = CheckExternalFile(mGameTexture, mHasColorKey);
		}

		if (HiresLump >=0 && HiresTexture == NULL) 
		{
			HiresTexture = FTexture::CreateTexture(HiresLump, FTexture::TEX_Any);
		}

		if (HiresTexture != NULL)
		{
			int w = HiresTexture->GetWidth();
			int h = HiresTexture->GetHeight();

			unsigned char * buffer=new unsigned char[w*(h+1)*4];
			memset(buffer, 0, w * (h+1) * 4);

			FGLBitmap bmp(buffer, w*4, w, h);
			
			int trans = HiresTexture->CopyTrueColorPixels(&bmp, 0, 0);
			mGameTexture->CheckTrans(buffer, w*h, trans);

			if (mHasColorKey)
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

	//----------------------------------------------------------------------------
	//
	//
	//
	//----------------------------------------------------------------------------

	unsigned char * FGLTexture::CreateTexBuffer(int translation, int &w, int &h)
	{
		unsigned char * buffer;
		int W, H;
		int xofs, yofs;


		// Textures that are already scaled in the texture lump will not get replaced
		// by hires textures
		if (gl_texture_usehires && mCanUseHires && translation == 0)
		{
			buffer = LoadHiresTexture (&w, &h);
			if (buffer)
			{
				return buffer;
			}
		}

		W = mGameTexture->GetWidth();
		H = mGameTexture->GetHeight();
		xofs = yofs = 0;

		if (mAsSprite)
		{
			if (mGameTexture->UseType == FTexture::TEX_Sprite || 
				mGameTexture->UseType == FTexture::TEX_SkinSprite || 
				mGameTexture->UseType == FTexture::TEX_Decal)
			{
				W+=2;
				H+=2;
				xofs++;
				yofs++;
			}
		}
		w=W;
		h=H;


		buffer=new unsigned char[W*(H+1)*4];
		memset(buffer, 0, W * (H+1) * 4);

		FGLBitmap bmp(buffer, W*4, W, H, translation == TRANSLATION_ICE);

		if (mGameTexture->bComplex)
		{
			FBitmap imgCreate;

			// The texture contains special processing so it must be composited by using the
			// base bitmap class and then be converted as a whole.
			if (imgCreate.Create(W, H))
			{
				memset(imgCreate.GetPixels(), 0, W * H * 4);
				int trans = mGameTexture->CopyTrueColorPixels(&imgCreate, xofs, yofs);
				bmp.CopyPixelDataRGB(0, 0, imgCreate.GetPixels(), W, H, 4, W * 4, 0, CF_BGRA);
				mGameTexture->CheckTrans(buffer, W*H, trans);
				mIsTransparent = mGameTexture->gl_info.mIsTransparent;
			}
		}
		else if (translation<=0)
		{
			int trans = mGameTexture->CopyTrueColorPixels(&bmp, xofs, yofs);
			mGameTexture->CheckTrans(buffer, W*H, trans);
			mIsTransparent = mGameTexture->gl_info.mIsTransparent;
		}
		else
		{
			FRemapTable remap;
			memset(&remap, 0, sizeof(remap));
			remap.Palette = GLTranslationPalette::GetPalette(translation);
			mGameTexture->CopyTrueColorTranslated(&bmp, xofs, yofs, -1, -1, 0, &remap);

			// mapping to the base palette will destroy all translucency information so we don't need
			// to check thisourselves.
			mIsTransparent = 0;
		}
		if (!mIsTransparent)
		{
			// [BB] Potentially upsample the buffer.
			buffer = gl_CreateUpsampledTextureBuffer (mGameTexture, buffer, W, H, w, h, !!mIsTransparent );
		}

		return buffer;
	}

	//===========================================================================
	// 
	// 
	//
	//===========================================================================

	bool FGLTexture::CreateHardwareTexture(unsigned char * buffer, int w, int h, int format)
	{
		bool mipmap = !mAsSprite && mGameTexture->UseType != FTexture::TEX_FontChar;
		mHWTexture = new FHardwareTexture;
		return mHWTexture->Create(buffer, w, h, mipmap, format);
	}

	//===========================================================================
	// 
	// 
	//
	//===========================================================================

	void FGLTexture::Bind(int texunit, int clamp)
	{
		int w, h;

		if (mHWTexture == NULL)
		{
			unsigned char *buffer = CreateTexBuffer(mTranslation, w, h);
			if (buffer == NULL) return;
			mGameTexture->ProcessData(buffer, w, h, mAsSprite);
			CreateHardwareTexture(buffer, w, h, -1);
			delete [] buffer;
		}
		mHWTexture->Bind(texunit);
		//if (clamp != -1) mHWTexture->SetClamp(clamp);
	}

	//===========================================================================
	// 
	// 
	//
	//===========================================================================

	FGLTexture *FGLTextureManager::GetTexture(FTexture *gametex, bool asSprite, int translation)
	{
		TexManKey key(gametex, asSprite, translation);
		FGLTexture **ptexaddr = mGLTextures.CheckKey(key);
		if (ptexaddr != NULL) return *ptexaddr;

		FGLTexture *texaddr = new FGLTexture;
		if (!texaddr->Create(gametex, asSprite, translation))
		{
			delete texaddr;
			texaddr = NULL;
		}
		mGLTextures[key] = texaddr;
		return texaddr;
	}

	//===========================================================================
	// 
	// 
	//
	//===========================================================================

	void FGLTextureManager::FlushAllTextures()
	{
		FGLTextureMap::Iterator it(mGLTextures);
		FGLTextureMap::Pair *p;

		while (it.NextPair(p))
		{
			p->Value->Flush();
		}
	}

	//===========================================================================
	// 
	// 
	//
	//===========================================================================

	void FGLTextureManager::DestroyAllTextures()
	{
		FGLTextureMap::Iterator it(mGLTextures);
		FGLTextureMap::Pair *p;

		while (it.NextPair(p))
		{
			delete p->Value;
		}
		mGLTextures.Clear();
	}

}