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
		mIsTransparent = -1;
		HiresTextureLump = -1;
		HiresTexture = NULL;
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
			CheckTrans(buffer, w*h, trans);

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
			W+=2;
			H+=2;
			xofs++;
			yofs++;
		}
		w=W;
		h=H;


		buffer=new unsigned char[W*(H+1)*4];
		memset(buffer, 0, W * (H+1) * 4);

		FGLBitmap bmp(buffer, W*4, W, H, translation == -1);

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
				CheckTrans(buffer, W*H, trans);
			}
		}
		else if (translation<=0)
		{
			int trans = mGameTexture->CopyTrueColorPixels(&bmp, xofs, yofs);
			CheckTrans(buffer, W*H, trans);
		}
		else
		{
			FRemapTable remap;
			memset(&remap, 0, sizeof(remap));
			remap.Palette = GLTranslationPalette::GetPalette(translation);
			mGameTexture->CopyTrueColorTranslated(&bmp, xofs, yofs, 0, &remap);

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

	//----------------------------------------------------------------------------
	//
	//
	//
	//----------------------------------------------------------------------------

	void FGLTexture::CheckTrans(unsigned char * buffer, int size, int trans)
	{
		if (mIsTransparent == -1) 
		{
			mIsTransparent = trans;
			if (trans == -1)
			{
				DWORD * dwbuf = (DWORD*)buffer;
				if (mIsTransparent == -1) for(int i=0;i<size;i++)
				{
					DWORD alpha = dwbuf[i]>>24;

					if (alpha != 0xff && alpha != 0)
					{
						mIsTransparent = 1;
						return;
					}
				}
			}
			mIsTransparent = 0;
		}
	}

	//===========================================================================
	// 
	// smooth the edges of transparent fields in the texture
	//
	//===========================================================================
	#ifdef WORDS_BIGENDIAN
	#define MSB 0
	#define SOME_MASK 0xffffff00
	#else
	#define MSB 3
	#define SOME_MASK 0x00ffffff
	#endif

	#define CHKPIX(ofs) (l1[(ofs)*4+MSB]==255 ? (( ((DWORD*)l1)[0] = ((DWORD*)l1)[ofs]&SOME_MASK), trans=true ) : false)

	bool FGLTexture::SmoothEdges(unsigned char * buffer,int w, int h)
	{
		int x,y;
		bool trans=buffer[MSB]==0; // If I set this to false here the code won't detect textures 
		// that only contain transparent pixels.
		unsigned char * l1;

		if (h<=1 || w<=1) return false;  // makes (a) no sense and (b) doesn't work with this code!

		l1=buffer;


		if (l1[MSB]==0 && !CHKPIX(1)) CHKPIX(w);
		l1+=4;
		for(x=1;x<w-1;x++, l1+=4)
		{
			if (l1[MSB]==0 &&  !CHKPIX(-1) && !CHKPIX(1)) CHKPIX(w);
		}
		if (l1[MSB]==0 && !CHKPIX(-1)) CHKPIX(w);
		l1+=4;

		for(y=1;y<h-1;y++)
		{
			if (l1[MSB]==0 && !CHKPIX(-w) && !CHKPIX(1)) CHKPIX(w);
			l1+=4;
			for(x=1;x<w-1;x++, l1+=4)
			{
				if (l1[MSB]==0 &&  !CHKPIX(-w) && !CHKPIX(-1) && !CHKPIX(1)) CHKPIX(w);
			}
			if (l1[MSB]==0 && !CHKPIX(-w) && !CHKPIX(-1)) CHKPIX(w);
			l1+=4;
		}

		if (l1[MSB]==0 && !CHKPIX(-w)) CHKPIX(1);
		l1+=4;
		for(x=1;x<w-1;x++, l1+=4)
		{
			if (l1[MSB]==0 &&  !CHKPIX(-w) && !CHKPIX(-1)) CHKPIX(1);
		}
		if (l1[MSB]==0 && !CHKPIX(-w)) CHKPIX(-1);

		return trans;
	}

	//===========================================================================
	// 
	// Post-process the texture data after the buffer has been created
	//
	//===========================================================================

	bool FGLTexture::ProcessData(unsigned char * buffer, int w, int h)
	{
		if (mGameTexture->bMasked && !mGameTexture->gl_info.bBrightmap) 
		{
			mGameTexture->bMasked=SmoothEdges(buffer, w, h);
			//if (tex->bMasked && !ispatch) FindHoles(buffer, w, h);
		}
		return true;
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

	FGLTexture *FGLTextureManager::FindTexture(FTexture *gametex, bool asSprite, int translation)
	{
		TexManKey key(gametex, asSprite, translation);
		FGLTexture *&texaddr = mGLTextures[key];
		if (texaddr != NULL) return texaddr;

		texaddr = new FGLTexture;
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