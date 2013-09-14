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
// Material class
//

#include "gl/gl_include.h"
#include "gl/gl_intern.h"	// CVAR declarations only.
#include "r_translate.h"
#include "r_data.h"
#include "textures/textures.h"
#include "gl/common/glc_translate.h"
#include "gl/new_renderer/gl2_renderer.h"
#include "gl/new_renderer/textures/gl2_texture.h"
#include "gl/new_renderer/textures/gl2_material.h"
#include "gl/new_renderer/textures/gl2_shader.h"


namespace GLRendererNew
{

//===========================================================================
// 
//
//
//===========================================================================

int FMaterial::Scale (int val, int scale) 
{ 
	int foo = (val << 17) / scale; 
	return (foo >> 1) + (foo & 1); 
}


//===========================================================================
// 
//
//
//===========================================================================

FMaterial::FMaterial(FTexture *tex, bool asSprite, int translation)
{
	if (tex->gl_info.bBrightmapChecked == 0)
	{
		tex->CreateDefaultBrightmap();
	}

	mSpeed = 0;

	mSizeTexels.w = tex->GetWidth();
	mSizeTexels.h = tex->GetHeight();

	mSizeUnits.w = tex->GetScaledWidth();
	mSizeUnits.h = tex->GetScaledHeight();

	mOffsetTexels.x = tex->LeftOffset;
	mOffsetTexels.y = tex->TopOffset;

	mOffsetUnits.x = tex->GetScaledLeftOffset();
	mOffsetUnits.y = tex->GetScaledTopOffset();

	// a little adjustment to make sprites look better with texture filtering:
	// create a 1 pixel wide empty frame around them.
	if (tex->UseType == FTexture::TEX_Sprite || 
		tex->UseType == FTexture::TEX_SkinSprite || 
		tex->UseType == FTexture::TEX_Decal)
	{
		if (!tex->bWarped)
		{
			mSizeTexels.w += 2;
			mSizeTexels.h += 2;

			mSizeUnits.w = Scale(mSizeTexels.w, tex->xScale);
			mSizeUnits.h = Scale(mSizeTexels.h, tex->yScale);

			mOffsetTexels.x += 1;
			mOffsetTexels.y += 1;

			mOffsetUnits.x = Scale(mOffsetTexels.x, tex->xScale);
			mOffsetUnits.y = Scale(mOffsetTexels.y, tex->yScale);
		}
	}
	mTempScale.x = mDefaultScale.x = FIXED2FLOAT(tex->xScale);
	mTempScale.y = mDefaultScale.y = FIXED2FLOAT(tex->yScale);


	/* if (tex->materialdef)
	{
	}
	else
	*/
	{
		const char *shadername;
		FGLTexture *gltex = GLRenderer2->GetGLTexture(tex, asSprite, translation);
		mLayers.Push(gltex);	// The first layer is always the owning texture

		if (translation == TRANSLATION_INTENSITY)
		{
			shadername = "Intensity";
		}
		else if (translation == TRANSLATION_SHADE)
		{
			shadername = "AlphaShade";
		}
		if (tex->bWarped == 1)
		{
			shadername = "Warp1";
			mSpeed = static_cast<FWarpTexture*>(tex)->GetSpeed();
		}
		else if (tex->bWarped == 2)
		{
			shadername = "Warp2";
			mSpeed = static_cast<FWarpTexture*>(tex)->GetSpeed();
		}
		else if (tex->gl_info.Brightmap != NULL && translation != TRANSLATION_ICE)
		{
			// NOTE: No brightmaps for icy textures!
			gltex = GLRenderer2->GetGLTexture(tex->gl_info.Brightmap, asSprite, 0);
			// the brightmap goes into the second layer - and is *not* using the specified translation
			mLayers.Push(gltex);
			shadername = "Brightmap";
		}
		else
		{
			shadername = "Default";
		}
	}
	//mShader = GLRenderer2->GetShader(shadername);
	mLayers.ShrinkToFit();
}

//===========================================================================
// 
//
//
//===========================================================================

FMaterial::~FMaterial()
{
}


//===========================================================================
// 
//
//
//===========================================================================

void FMaterial::Bind(float *colormap, int texturemode, float desaturation, int clamp)
{
	mShader->Bind(colormap, texturemode, desaturation, mSpeed);
	for(unsigned i=0;i<mLayers.Size();i++)
	{
		mLayers[i]->Bind(i, clamp);
	}

}

//===========================================================================
// 
//
//
//===========================================================================

FMaterialContainer::FMaterialContainer(FTexture *tex)
{
	mTexture = tex;
	mMatWorld = NULL;
	mMatPatch = NULL;
	mMatOthers = NULL;
}

//===========================================================================
// 
//
//
//===========================================================================

FMaterialContainer::~FMaterialContainer()
{
	if (mMatWorld != NULL) delete mMatWorld;
	if (mMatPatch != NULL) delete mMatPatch;
	if (mMatOthers != NULL)
	{
		FMaterialMap::Iterator it(*mMatOthers);
		FMaterialMap::Pair *p;

		while (it.NextPair(p))
		{
			delete p->Value;
		}
		delete mMatOthers;
	}
}

//===========================================================================
// 
//
//
//===========================================================================

FMaterial *FMaterialContainer::GetMaterial(bool asSprite, int translation)
{
	FMaterial ** mat;
	if (translation == 0)
	{
		if (!asSprite)
		{
			mat = &mMatWorld;
		}
		else
		{
			mat = &mMatPatch;
		}
	}
	else
	{
		MaterialKey key(asSprite, translation);
		if (mMatOthers == NULL) mMatOthers = new FMaterialMap;
		mat = &(*mMatOthers)[key];
	}
	if (*mat == NULL)
	{
		*mat = new FMaterial(mTexture, asSprite, translation);
	}
	return *mat;
}



}