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
// Hardware texture class
//

#include "gl/gl_include.h"
#include "gl/gl_intern.h"	// CVAR declarations only.
#include "textures/textures.h"
#include "gl/new_renderer/textures/gl2_hwtexture.h"



namespace GLRendererNew
{

	struct TexFilter_s
	{
		int minfilter;
		int magfilter;
		bool mipmapping;
	} ;

	static TexFilter_s TexFilter[]={
		{GL_NEAREST,					GL_NEAREST,		false},
		{GL_NEAREST_MIPMAP_NEAREST,		GL_NEAREST,		true},
		{GL_LINEAR,						GL_LINEAR,		false},
		{GL_LINEAR_MIPMAP_NEAREST,		GL_LINEAR,		true},
		{GL_LINEAR_MIPMAP_LINEAR,		GL_LINEAR,		true},
	};



	unsigned FHardwareTexture::LastBound[32];

	//----------------------------------------------------------------------------
	//
	//
	//
	//----------------------------------------------------------------------------

	FHardwareTexture::FHardwareTexture()
	{
		glGenTextures(1,&mTextureID);
	}


	//----------------------------------------------------------------------------
	//
	//
	//
	//----------------------------------------------------------------------------

	FHardwareTexture::~FHardwareTexture()
	{
		glDeleteTextures(1,&mTextureID);
	}


	//----------------------------------------------------------------------------
	//
	//
	//
	//----------------------------------------------------------------------------

	bool FHardwareTexture::Create(const unsigned char *buffer, int w, int h, bool mipmapped, int texformat)
	{
		if (texformat == -1) texformat = GL_RGBA;
		if (Bind(0))
		{
			glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, mipmapped);
			glTexImage2D(GL_TEXTURE_2D, 0, texformat, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, buffer);

			if (mipmapped)
			{
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, TexFilter[gl_texture_filter].minfilter);
				if (gl_texture_filter_anisotropic)
				{
					glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, gl_texture_filter_anisotropic);
				}
			}
			else
			{
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, TexFilter[gl_texture_filter].magfilter);
			}

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, TexFilter[gl_texture_filter].magfilter);
			return true;
		}
		else return false;
	}

	
	//----------------------------------------------------------------------------
	//
	//
	//
	//----------------------------------------------------------------------------

	bool FHardwareTexture::Bind(int texunit)
	{
		if (mTextureID != 0)
		{
			if (LastBound[texunit] == mTextureID) return true;
			LastBound[texunit] = mTextureID;
			if (texunit != 0) gl.ActiveTexture(GL_TEXTURE0+texunit);
			glBindTexture(GL_TEXTURE_2D, mTextureID);
			if (texunit != 0) gl.ActiveTexture(GL_TEXTURE0);
			return true;
		}
		else return false;
	}


}
