/*
** gl_framebuffer.cpp
** Implementation of the non-hardware specific parts of the
** OpenGL frame buffer
**
**---------------------------------------------------------------------------
** Copyright 2000-2007 Christoph Oelckers
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

#include "gl/gl_include.h"
#include "gl/common/glc_clock.h"
#include "gl/common/glc_texture.h"
#include "files.h"
#include "m_swap.h"
#include "r_draw.h"
#include "v_video.h"
#include "r_main.h"
#include "m_png.h"
#include "m_crc32.h"
#include "gl/common/glc_templates.h"
#include "gl/common/glc_data.h"
#include "gl/gl_struct.h"
#include "gl/gl_intern.h"
#include "gl/old_renderer/gl1_texture.h"
#include "gl/gl_functions.h"
#include "gl/old_renderer/gl1_shader.h"
#include "gl/gl_framebuffer.h"
#include "gl/common/glc_translate.h"
#include "vectors.h"
#include "gl/old_renderer/gl1_drawinfo.h"
// [BB] Added include.
#ifdef _MSC_VER
#include "../hqnx/hqnx.h"
#endif

IMPLEMENT_CLASS(OpenGLFrameBuffer)
EXTERN_CVAR (Float, vid_brightness)
EXTERN_CVAR (Float, vid_contrast)

void gl_SetupMenu();

GLRendererBase *GLRenderer;

using namespace GLRendererOld;

//==========================================================================
//
//
//
//==========================================================================

OpenGLFrameBuffer::OpenGLFrameBuffer(int width, int height, int bits, int refreshHz, bool fullscreen) : 
	Super(width, height, bits, refreshHz, fullscreen) 
{
	GLRenderer = new GL1Renderer;
	memcpy (SourcePalette, GPalette.BaseColors, sizeof(PalEntry)*256);
	UpdatePalette ();
	ScreenshotBuffer = NULL;
	LastCamera = NULL;

	DoSetGamma();

	InitializeState();
	gl_GenerateGlobalBrightmapFromColormap();
	gl_InitSpecialTextures();

	// [BB] Backported from GZDoom revision 660.
	Accel2D = true;

#ifdef _MSC_VER
	// [BB] Necessary for the hqnx resizing.
	InitLUTs();
#endif
}

OpenGLFrameBuffer::~OpenGLFrameBuffer()
{
	gl_FreeSpecialTextures();
	// all native textures must be completely removed before destroying the frame buffer
	FGLTexture::DeleteAll();
	gl_ClearShaders();
	delete GLRenderer;
	GLRenderer = NULL;
}

//==========================================================================
//
// Initializes the GL renderer
//
//==========================================================================

void OpenGLFrameBuffer::InitializeState()
{
	static bool first=true;

	gl.LoadExtensions();
	Super::InitializeState();
	gl_SetupMenu();
	if (first)
	{
		first=false;
		// [BB] For some reason this crashes, if compiled with MinGW and optimization. Has to be investigated.
#ifdef _MSC_VER
		gl.PrintStartupLog();
#endif

		if (gl.flags&RFL_NPOT_TEXTURE)
		{
			Printf("Support for non power 2 textures enabled.\n");
		}
		if (gl.flags&RFL_OCCLUSION_QUERY)
		{
			Printf("Occlusion query enabled.\n");
		}
	}
	gl.ClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	gl.ClearDepth(1.0f);
	gl.DepthFunc(GL_LESS);
	gl.ShadeModel(GL_SMOOTH);

	gl.Enable(GL_DITHER);
	gl.Enable(GL_ALPHA_TEST);
	gl.Disable(GL_CULL_FACE);
	gl.Disable(GL_POLYGON_OFFSET_FILL);
	gl.Enable(GL_POLYGON_OFFSET_LINE);
	gl.Enable(GL_BLEND);
	gl.Enable(GL_DEPTH_CLAMP_NV);
	gl.Disable(GL_DEPTH_TEST);
	gl.Enable(GL_TEXTURE_2D);
	gl.Disable(GL_LINE_SMOOTH);
	gl.AlphaFunc(GL_GEQUAL,0.5f);
	gl.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	gl.Hint(GL_LINE_SMOOTH_HINT, GL_NICEST);
	gl.Hint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
	gl.Hint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	gl.Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	/*
	gl.MatrixMode(GL_PROJECTION);
	gl.LoadIdentity();
	gl.Ortho(0.0, GetWidth() * 1.0, 0.0, GetHeight() * 1.0, -1.0, 1.0);

	gl.MatrixMode(GL_MODELVIEW);
	gl.LoadIdentity();
	//GL::SetPerspective(90.f, GetWidth() * 1.f / GetHeight(), 0.f, 1000.f);
	*/

	gl.Viewport(0, (GetTrueHeight()-GetHeight())/2, GetWidth(), GetHeight()); 

	Begin2D(false);
	GLRenderer->Initialize();
}

//==========================================================================
//
// Updates the screen
//
//==========================================================================

void OpenGLFrameBuffer::Update()
{
	if (!CanUpdate()) return;

	Begin2D(false);

	DrawRateStuff();

	if (GetTrueHeight() != GetHeight())
	{
		// Letterbox time! Draw black top and bottom borders.
		int borderHeight = (GetTrueHeight() - GetHeight()) / 2;

		gl.Viewport(0, 0, GetWidth(), GetTrueHeight());
		gl.MatrixMode(GL_PROJECTION);
		gl.LoadIdentity();
		gl.Ortho(0.0, GetWidth() * 1.0, 0.0, GetTrueHeight(), -1.0, 1.0);
		gl.MatrixMode(GL_MODELVIEW);
		gl.Color3f(0.f, 0.f, 0.f);
		gl_EnableTexture(false);
		gl_DisableShader();

		gl.Begin(GL_QUADS);
		// upper quad
		gl.Vertex2i(0, borderHeight);
		gl.Vertex2i(0, 0);
		gl.Vertex2i(GetWidth(), 0);
		gl.Vertex2i(GetWidth(), borderHeight);
		gl.End();

		gl.Begin(GL_QUADS);
		// lower quad
		gl.Vertex2i(0, GetTrueHeight());
		gl.Vertex2i(0, GetTrueHeight() - borderHeight);
		gl.Vertex2i(GetWidth(), GetTrueHeight() - borderHeight);
		gl.Vertex2i(GetWidth(), GetTrueHeight());
		gl.End();

		gl_EnableTexture(true);

		Begin2D(false);
		gl.Viewport(0, (GetTrueHeight() - GetHeight()) / 2, GetWidth(), GetHeight()); 

	}

	Finish.Reset();
	Finish.Clock();
	gl.Finish();
	Finish.Unclock();
	gl.SwapBuffers();
	Unlock();
}

//===========================================================================
//
// DoSetGamma
//
// (Unfortunately Windows has some safety precautions that block gamma ramps
//  that are considered too extreme. As a result this doesn't work flawlessly)
//
//===========================================================================

void OpenGLFrameBuffer::DoSetGamma()
{
	WORD gammaTable[768];

	if (m_supportsGamma)
	{
		// This formula is taken from Doomsday
		float gamma = clamp<float>(Gamma, 0.1f, 4.f);
		float contrast = clamp<float>(vid_contrast, 0.1f, 3.f);
		float bright = clamp<float>(vid_brightness, -0.8f, 0.8f);

		double invgamma = 1 / gamma;
		double norm = pow(255., invgamma - 1);

		for (int i = 0; i < 256; i++)
		{
			double val = i * contrast - (contrast - 1) * 127;
			if(gamma != 1) val = pow(val, invgamma) / norm;
			val += bright * 128;

			gammaTable[i] = gammaTable[i + 256] = gammaTable[i + 512] = (WORD)clamp<double>(val*256, 0, 0xffff);
		}
		SetGammaTable(gammaTable);
	}
}

bool OpenGLFrameBuffer::SetGamma(float gamma)
{
	DoSetGamma();
	return true;
}

bool OpenGLFrameBuffer::SetBrightness(float bright)
{
	DoSetGamma();
	return true;
}

bool OpenGLFrameBuffer::SetContrast(float contrast)
{
	DoSetGamma();
	return true;
}

bool OpenGLFrameBuffer::UsesColormap() const
{
	// The GL renderer has no use for colormaps so let's
	// not create them and save us some time.
	return false;
}

//===========================================================================
//
//
//===========================================================================

void OpenGLFrameBuffer::UpdatePalette()
{
	int rr=0,gg=0,bb=0;
	for(int x=0;x<256;x++)
	{
		rr+=GPalette.BaseColors[x].r;
		gg+=GPalette.BaseColors[x].g;
		bb+=GPalette.BaseColors[x].b;
	}
	rr>>=8;
	gg>>=8;
	bb>>=8;

	palette_brightness = (rr*77 + gg*143 + bb*35)/255;
}

void OpenGLFrameBuffer::GetFlashedPalette (PalEntry pal[256])
{
	memcpy(pal, SourcePalette, 256*sizeof(PalEntry));
}

PalEntry *OpenGLFrameBuffer::GetPalette ()
{
	return SourcePalette;
}

bool OpenGLFrameBuffer::SetFlash(PalEntry rgb, int amount)
{
	Flash = PalEntry(amount, rgb.r, rgb.g, rgb.b);
	return true;
}

void OpenGLFrameBuffer::GetFlash(PalEntry &rgb, int &amount)
{
	rgb = Flash;
	rgb.a=0;
	amount = Flash.a;
}

int OpenGLFrameBuffer::GetPageCount()
{
	return 1;
}

//==========================================================================
//
// DFrameBuffer :: PrecacheTexture
//
//==========================================================================

void OpenGLFrameBuffer::PrecacheTexture(FTexture *tex, bool cache)
{
	if (tex != NULL)
	{
		if (cache)
		{
			tex->PrecacheGL();
		}
		else
		{
			tex->UncacheGL();
		}
	}
}


//==========================================================================
//
// DFrameBuffer :: CreatePalette
//
// Creates a native palette from a remap table, if supported.
//
//==========================================================================

FNativePalette *OpenGLFrameBuffer::CreatePalette(FRemapTable *remap)
{
	return GLTranslationPalette::CreatePalette(remap);
}

//==========================================================================
//
//
//
//==========================================================================
bool OpenGLFrameBuffer::Begin2D(bool)
{
	gl.MatrixMode(GL_MODELVIEW);
	gl.LoadIdentity();
	gl.MatrixMode(GL_PROJECTION);
	gl.LoadIdentity();
	gl.Ortho(
		(GLdouble) 0,
		(GLdouble) GetWidth(), 
		(GLdouble) GetHeight(), 
		(GLdouble) 0,
		(GLdouble) -1.0, 
		(GLdouble) 1.0 
		);
	gl.Disable(GL_DEPTH_TEST);
	gl.Disable(GL_MULTISAMPLE);
	gl_EnableFog(false);
	return true;
}

//==========================================================================
//
// Draws a texture
//
//==========================================================================

void STACK_ARGS OpenGLFrameBuffer::DrawTextureV(FTexture *img, int x0, int y0, uint32 tag, va_list tags)
{
	DrawParms parms;

	if (ParseDrawTextureTags(img, x0, y0, tag, tags, &parms, true))
	{
		if (GLRenderer != NULL) GLRenderer->DrawTexture(img, parms);
	}
}

//==========================================================================
//
//
//
//==========================================================================
void OpenGLFrameBuffer::DrawLine(int x1, int y1, int x2, int y2, int palcolor, uint32 color)
{
	if (GLRenderer != NULL) 
		GLRenderer->DrawLine(x1, y1, x2, y2, palcolor, color);
}

//==========================================================================
//
//
//
//==========================================================================
void OpenGLFrameBuffer::DrawPixel(int x1, int y1, int palcolor, uint32 color)
{
	if (GLRenderer != NULL) 
		GLRenderer->DrawPixel(x1, y1, palcolor, color);
}

//==========================================================================
//
//
//
//==========================================================================
void OpenGLFrameBuffer::Dim(PalEntry)
{
	// Unlike in the software renderer the color is being ignored here because
	// view blending only affects the actual view with the GL renderer.
	if (GLRenderer != NULL) 
		GLRenderer->Dim((DWORD)dimcolor , dimamount, 0, 0, GetWidth(), GetHeight());
}

void OpenGLFrameBuffer::Dim(PalEntry color, float damount, int x1, int y1, int w, int h)
{
	if (GLRenderer != NULL) 
		GLRenderer->Dim(color, damount, x1, y1, w, h);
}

//==========================================================================
//
//
//
//==========================================================================
void OpenGLFrameBuffer::FlatFill (int left, int top, int right, int bottom, FTexture *src, bool local_origin)
{

	if (GLRenderer != NULL) 
		GLRenderer->FlatFill(left, top, right, bottom, src, local_origin);
}

//==========================================================================
//
//
//
//==========================================================================
void OpenGLFrameBuffer::Clear(int left, int top, int right, int bottom, int palcolor, uint32 color)
{
	if (GLRenderer != NULL) 
		GLRenderer->Clear(left, top, right, bottom, palcolor, color);
}

//===========================================================================
// 
//	Takes a screenshot
//
//===========================================================================

void OpenGLFrameBuffer::GetScreenshotBuffer(const BYTE *&buffer, int &pitch, ESSType &color_type)
{
	int w = SCREENWIDTH;
	int h = SCREENHEIGHT;

	ReleaseScreenshotBuffer();
	ScreenshotBuffer = new BYTE[w * h * 3];

	gl.ReadPixels(0,(GetTrueHeight() - GetHeight()) / 2,w,h,GL_RGB,GL_UNSIGNED_BYTE,ScreenshotBuffer);
	pitch = -w*3;
	color_type = SS_RGB;
	buffer = ScreenshotBuffer + w * 3 * (h - 1);
}

//===========================================================================
// 
// Releases the screenshot buffer.
//
//===========================================================================

void OpenGLFrameBuffer::ReleaseScreenshotBuffer()
{
	if (ScreenshotBuffer != NULL) delete [] ScreenshotBuffer;
	ScreenshotBuffer = NULL;
}


