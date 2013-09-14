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
#include "textures/textures.h"
#include "gl/new_renderer/textures/gl2_texture.h"
#include "gl/new_renderer/textures/gl2_material.h"


namespace GLRendererNew
{

int FMaterial::Scale (int val, int scale) 
{ 
	int foo = (val << 17) / scale; 
	return (foo >> 1) + (foo & 1); 
}


FMaterial::FMaterial(FTexture *tex, bool asSprite, int translation)
{
	if (tex->gl_info.bBrightmapChecked == 0)
	{
		tex->CreateDefaultBrightmap();
	}

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


	//TArray<FGLTexture *> mLayers;
	//FShader *mShader;

	/* if (tex->materialdef)
	{
	}
	else
	*/
	{
		const char *shadername;
		//mLayers.Push(tex);	// The first layer is always the owning texture

		if (translation == TRANSLATION(TRANSLATION_Standard, 8))
		{
			shadername = "Intensity";
		}
		else if (translation == TRANSLATION(TRANSLATION_Standard, 9))	// Placeholder for alpha shade
		{
			shadername = "AlphaShade";
		}
		if (tex->bWarped == 1)
		{
			shadername = "Warp1";
		}
		else if (tex->bWarped == 2)
		{
			shadername = "Warp2";
		}
		else if (tex->gl_info.Brightmap != NULL)
		{
			//mLayers.Push(tex->gl_info.Brightmap);	// the brightmap goes into the second layer
			shadername = "Brightmap";
		}
		else
		{
			shadername = "Default";
		}
	}
}

FMaterial::~FMaterial()
{
}


}