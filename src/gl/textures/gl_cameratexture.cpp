/*
** GL camera texture handling
**
**---------------------------------------------------------------------------
** Copyright 2009 Christoph Oelckers
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
#include "gl/system/gl_cvars.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/textures/gl_material.h"

#include "c_cvars.h"
CVAR(Bool, gl_usefb, false , CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
//===========================================================================
//
// Camera texture rendering
//
//===========================================================================
extern TexFilter_s TexFilter[];

void FCanvasTexture::RenderGLView (AActor *Viewpoint, int FOV)
{
	FMaterial * gltex = FMaterial::ValidateTexture(this);

	int width = gltex->TextureWidth(GLUSE_TEXTURE);
	int height = gltex->TextureHeight(GLUSE_TEXTURE);

	gl_fixedcolormap=CM_DEFAULT;

	bool usefb;

	if (gl.flags & RFL_FRAMEBUFFER)
	{
		usefb = gl_usefb || width > screen->GetWidth() || height > screen->GetHeight();
	}
	else usefb = false;


	if (!usefb)
	{
		gl.Flush();
	}
	else
	{
#if defined(_WIN32) && (defined(_MSC_VER) || defined(__INTEL_COMPILER))
		__try
#endif
		{
			GLRenderer->StartOffscreen();
			gltex->BindToFrameBuffer();
		}
#if defined(_WIN32) && (defined(_MSC_VER) || defined(__INTEL_COMPILER))
		__except(1)
		{
			usefb = false;
			gl_usefb = false;
			GLRenderer->EndOffscreen();
			gl.Flush();
		}
#endif
	}

	GL_IRECT bounds;
	bounds.left=bounds.top=0;
	bounds.width=FHardwareTexture::GetTexDimension(gltex->GetWidth(GLUSE_TEXTURE));
	bounds.height=FHardwareTexture::GetTexDimension(gltex->GetHeight(GLUSE_TEXTURE));

	GLRenderer->RenderViewpoint(Viewpoint, &bounds, FOV, (float)width/height, (float)width/height, false, false);

	if (!usefb)
	{
		gl.Flush();
		gltex->Bind(CM_DEFAULT, 0, 0);
		gl.CopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, bounds.width, bounds.height);
	}
	else
	{
		GLRenderer->EndOffscreen();
	}

	gltex->Bind(CM_DEFAULT, 0, 0);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, TexFilter[gl_texture_filter].magfilter);
	bNeedsUpdate = false;
	bDidUpdate = true;
	bFirstUpdate = false;
}

