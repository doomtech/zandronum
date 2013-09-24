/*
** gltexture.cpp
** Low level OpenGL texture handling. These classes are also
** containers for the various translations a texture can have.
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

#include "gl/system/gl_system.h"
#include "templates.h"
#include "r_draw.h"
#include "m_crc32.h"
#include "c_cvars.h"
#include "c_dispatch.h"
#include "v_palette.h"

#include "gl/system/gl_cvars.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/textures/gl_material.h"


extern TexFilter_s TexFilter[];
extern int TexFormat[];
EXTERN_CVAR(Bool, gl_clamp_per_texture)


//===========================================================================
// 
//	Static texture data
//
//===========================================================================
unsigned int FHardwareTexture::lastbound[FHardwareTexture::MAX_TEXTURES];

//===========================================================================
// 
// STATIC - Gets the maximum size of hardware textures
//
//===========================================================================
int FHardwareTexture::GetTexDimension(int value)
{
	if (value > gl.max_texturesize) return gl.max_texturesize;
	if (gl.flags&RFL_NPOT_TEXTURE) return value;

	int i=1;
	while (i<value) i+=i;
	return i;
}

//===========================================================================
// 
//	Loads the texture image into the hardware
//
// NOTE: For some strange reason I was unable to find the source buffer
// should be one line higher than the actual texture. I got extremely
// strange crashes deep inside the GL driver when I didn't do it!
//
//===========================================================================
void FHardwareTexture::LoadImage(unsigned char * buffer,int w, int h, unsigned int & glTexID,int wrapparam, bool alphatexture, int texunit)
{
	int rh,rw;
	int texformat=TexFormat[gl_texture_format];
	bool deletebuffer=false;
	bool use_mipmapping = TexFilter[gl_texture_filter].mipmapping;

	if (alphatexture) texformat=GL_ALPHA8;
	if (glTexID==0) gl.GenTextures(1,&glTexID);
	gl.BindTexture(GL_TEXTURE_2D, glTexID);
	lastbound[texunit]=glTexID;

	if (!buffer)
	{
		w=texwidth;
		h=abs(texheight);
		rw = GetTexDimension (w);
		rh = GetTexDimension (h);

		// The texture must at least be initialized if no data is present.
		mipmap=false;
		gl.TexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, false);
		buffer=(unsigned char *)calloc(4,rw * (rh+1));
		deletebuffer=true;
		//texheight=-h;	
	}
	else
	{
		rw = GetTexDimension (w);
		rh = GetTexDimension (h);

		if (gl_vid_compatibility)
		{
			mipmap=false;
		}
		gl.TexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, (mipmap && use_mipmapping));

		if (rw == w && rh == h)
		{
			scalexfac=scaleyfac=1.f;
		}
		else if (wrapparam==GL_REPEAT || rw < w || rh < h)
		{
			// The image must be scaled to fit the texture
			unsigned char * scaledbuffer=(unsigned char *)calloc(4,rw * (rh+1));
			if (scaledbuffer)
			{
				gluScaleImage(GL_RGBA,w, h,GL_UNSIGNED_BYTE,buffer, rw, rh, GL_UNSIGNED_BYTE,scaledbuffer);
				deletebuffer=true;
				buffer=scaledbuffer;
			}
			scalexfac=scaleyfac=1.f;
		}
		else
		{
			// The image must be copied to a larger buffer
			unsigned char * scaledbuffer=(unsigned char *)calloc(4,rw * (rh+1));
			if (scaledbuffer)
			{
				for(int y=0;y<h;y++)
				{
					memcpy(scaledbuffer + rw * y * 4, buffer + w * y * 4, w * 4);
					// duplicate the last row to eliminate texture filtering artifacts on borders!
					if (rw>w) 
						memcpy(	scaledbuffer + rw * y * 4 + w * 4,
						scaledbuffer + rw * y * 4 + w * 4 -4, 4);
				}
				// also duplicate the last line for the same reason!
				memcpy(	scaledbuffer + rw * h * 4, 	scaledbuffer + rw * (h-1) * 4, w*4 + 4);
				
				deletebuffer=true;
				buffer=scaledbuffer;
				scalexfac = (float)w / rw;
				scaleyfac = (float)h / rh;
			}
		}
	}
	gl.TexImage2D(GL_TEXTURE_2D, 0, texformat, rw, rh, 0, GL_RGBA, GL_UNSIGNED_BYTE, buffer);

	if (deletebuffer) free(buffer);

	if (mipmap && use_mipmapping)
	{
		gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, TexFilter[gl_texture_filter].minfilter);
		if (gl_texture_filter_anisotropic)
		{
			gl.TexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, gl_texture_filter_anisotropic);
		}
	}
	else
	{
		gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, TexFilter[gl_texture_filter].magfilter);
	}

	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapparam==GL_CLAMP? GL_CLAMP_TO_EDGE : GL_REPEAT);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapparam==GL_CLAMP? GL_CLAMP_TO_EDGE : GL_REPEAT);
	clampmode = wrapparam==GL_CLAMP? GLT_CLAMPX|GLT_CLAMPY : 0;

	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, TexFilter[gl_texture_filter].magfilter);
}


//===========================================================================
// 
//	Creates a texture
//
//===========================================================================
FHardwareTexture::FHardwareTexture(int _width, int _height, bool _mipmap, bool wrap) 
{
	mipmap=_mipmap;
	texwidth=_width;
	texheight=_height;

	if (wrap || (gl.flags&RFL_NPOT_TEXTURE))
	{
		scaleyfac=scalexfac=1.f;
	}
	else
	{
		scalexfac=MIN<float>(1.f,(float)texwidth/FHardwareTexture::GetTexDimension(texwidth));
		scaleyfac=MIN<float>(1.f,(float)texheight/FHardwareTexture::GetTexDimension(texheight));
	}

	int cm_arraysize = CM_FIRSTSPECIALCOLORMAP + SpecialColormaps.Size();
	glTexID = new unsigned[cm_arraysize];
	memset(glTexID,0,sizeof(unsigned int)*cm_arraysize);
	clampmode=0;
	glDepthID = 0;
}


//===========================================================================
// 
//	Deletes a texture id and unbinds it from the texture units
//
//===========================================================================
void FHardwareTexture::DeleteTexture(unsigned int texid)
{
	if (texid != 0) 
	{
		for(int i = 0; i < MAX_TEXTURES; i++)
		{
			if (lastbound[i] == texid)
			{
				lastbound[i] = 0;
			}
		}
		gl.DeleteTextures(1, &texid);
	}
}

//===========================================================================
// 
//	Frees all associated resources
//
//===========================================================================
void FHardwareTexture::Clean(bool all)
{
	int cm_arraysize = CM_FIRSTSPECIALCOLORMAP + SpecialColormaps.Size();

	if (all)
	{
		for (int i=0;i<cm_arraysize;i++)
		{
			DeleteTexture(glTexID[i]);
		}
		//gl.DeleteTextures(cm_arraysize,glTexID);
		memset(glTexID,0,sizeof(unsigned int)*cm_arraysize);
	}
	else
	{
		for (int i=1;i<cm_arraysize;i++)
		{
			DeleteTexture(glTexID[i]);
		}
		//gl.DeleteTextures(cm_arraysize-1,glTexID+1);
		memset(glTexID+1,0,sizeof(unsigned int)*(cm_arraysize-1));
	}
	for(unsigned int i=0;i<glTexID_Translated.Size();i++)
	{
		DeleteTexture(glTexID_Translated[i].glTexID);
	}
	glTexID_Translated.Clear();
	if (glDepthID != 0) gl.DeleteRenderbuffers(1, &glDepthID);
}

//===========================================================================
// 
//	Destroys the texture
//
//===========================================================================
FHardwareTexture::~FHardwareTexture() 
{ 
	Clean(true); 
	delete [] glTexID;
}


//===========================================================================
// 
//	Gets a texture ID address and validates all required data
//
//===========================================================================

unsigned * FHardwareTexture::GetTexID(int cm, int translation)
{
	if (cm < 0 || cm >= CM_FIRSTSPECIALCOLORMAP + SpecialColormaps.Size()) cm=CM_DEFAULT;

	if (translation==0)
	{
		return &glTexID[cm];
	}

	// normally there aren't more than very few different 
	// translations here so this isn't performance critical.
	for(unsigned int i=0;i<glTexID_Translated.Size();i++)
	{
		if (glTexID_Translated[i].cm == cm &&
			glTexID_Translated[i].translation == translation)
		{
			return &glTexID_Translated[i].glTexID;
		}
	}

	int add = glTexID_Translated.Reserve(1);
	glTexID_Translated[add].cm=cm;
	glTexID_Translated[add].translation=translation;
	glTexID_Translated[add].glTexID=0;
	return &glTexID_Translated[add].glTexID;
}

//===========================================================================
// 
//	Binds this patch
//
//===========================================================================
unsigned int FHardwareTexture::Bind(int texunit, int cm,int translation)
{
	unsigned int * pTexID=GetTexID(cm, translation);

	if (*pTexID!=0)
	{
		if (lastbound[texunit]==*pTexID) return *pTexID;
		lastbound[texunit]=*pTexID;
		if (texunit != 0) gl.ActiveTexture(GL_TEXTURE0+texunit);
		gl.BindTexture(GL_TEXTURE_2D, *pTexID);
		if (texunit != 0) gl.ActiveTexture(GL_TEXTURE0);
		return *pTexID;
	}
	return 0;
}


void FHardwareTexture::Unbind(int texunit)
{
	if (lastbound[texunit] != 0)
	{
		if (texunit != 0) gl.ActiveTexture(GL_TEXTURE0+texunit);
		gl.BindTexture(GL_TEXTURE_2D, 0);
		if (texunit != 0) gl.ActiveTexture(GL_TEXTURE0);
		lastbound[texunit] = 0;
	}
}

void FHardwareTexture::UnbindAll()
{
	for(int texunit = 0; texunit < 16; texunit++)
	{
		Unbind(texunit);
	}
}

//===========================================================================
// 
//	Creates a depth buffer for this texture
//
//===========================================================================

int FHardwareTexture::GetDepthBuffer()
{
	if (gl.flags & RFL_FRAMEBUFFER)
	{
		if (glDepthID == 0)
		{
			gl.GenRenderbuffers(1, &glDepthID);
			gl.BindRenderbuffer(GL_RENDERBUFFER, glDepthID);
			gl.RenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 
				GetTexDimension(texwidth), GetTexDimension(texheight));
			gl.BindRenderbuffer(GL_RENDERBUFFER, 0);
		}
		return glDepthID;
	}
	return 0;
}


//===========================================================================
// 
//	Binds this texture's surfaces to the current framrbuffer
//
//===========================================================================

void FHardwareTexture::BindToFrameBuffer()
{
	if (gl.flags & RFL_FRAMEBUFFER)
	{
		gl.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, glTexID[0], 0);
		gl.FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, GetDepthBuffer()); 
	}
}


//===========================================================================
// 
//	(re-)creates the texture
//
//===========================================================================
unsigned int FHardwareTexture::CreateTexture(unsigned char * buffer, int w, int h, bool wrap, int texunit,
									  int cm, int translation)
{
	if (cm < 0 || cm >= CM_FIRSTSPECIALCOLORMAP + SpecialColormaps.Size()) cm=CM_DEFAULT;

	unsigned int * pTexID=GetTexID(cm, translation);

	if (texunit != 0) gl.ActiveTexture(GL_TEXTURE0+texunit);
	LoadImage(buffer, w, h, *pTexID, wrap? GL_REPEAT:GL_CLAMP, cm==CM_SHADE, texunit);
	if (texunit != 0) gl.ActiveTexture(GL_TEXTURE0);
	return *pTexID;
}


