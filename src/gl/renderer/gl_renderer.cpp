/*
** gl1_renderer.cpp
** Renderer interface
**
**---------------------------------------------------------------------------
** Copyright 2008 Christoph Oelckers
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
#include "files.h"
#include "m_swap.h"
#include "r_draw.h"
#include "v_video.h"
#include "r_main.h"
#include "m_png.h"
#include "m_crc32.h"
#include "w_wad.h"
//#include "gl/gl_intern.h"
#include "gl/gl_functions.h"
#include "vectors.h"

#include "gl/system/gl_framebuffer.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/renderer/gl_lightdata.h"
#include "gl/renderer/gl_renderstate.h"
#include "gl/data/gl_data.h"
#include "gl/data/gl_vertexbuffer.h"
#include "gl/dynlights/gl_lightbuffer.h"
#include "gl/scene/gl_drawinfo.h"
#include "gl/shaders/gl_shader.h"
#include "gl/textures/gl_texture.h"
#include "gl/textures/gl_translate.h"
#include "gl/textures/gl_material.h"
#include "gl/utility/gl_clock.h"
#include "gl/utility/gl_templates.h"

//===========================================================================
// 
// Renderer interface
//
//===========================================================================

EXTERN_CVAR(Bool, gl_render_segs)

//-----------------------------------------------------------------------------
//
// Initialize
//
//-----------------------------------------------------------------------------

void FGLRenderer::Initialize()
{
	glpart2 = FTexture::CreateTexture(Wads.GetNumForFullName("glstuff/glpart2.png"), FTexture::TEX_MiscPatch);
	glpart = FTexture::CreateTexture(Wads.GetNumForFullName("glstuff/glpart.png"), FTexture::TEX_MiscPatch);
	mirrortexture = FTexture::CreateTexture(Wads.GetNumForFullName("glstuff/mirror.png"), FTexture::TEX_MiscPatch);
	gllight = FTexture::CreateTexture(Wads.GetNumForFullName("glstuff/gllight.png"), FTexture::TEX_MiscPatch);

	mVBO = new FVertexBuffer;
	mFBID = 0;
	SetupLevel();
	mShaderManager = new FShaderManager;
}

FGLRenderer::~FGLRenderer() 
{
	gl_DeleteAllAttachedLights();
	FMaterial::FlushAll();
	if (mShaderManager != NULL) delete mShaderManager;
	if (mVBO != NULL) delete mVBO;
	if (glpart2) delete glpart2;
	if (glpart) delete glpart;
	if (mirrortexture) delete mirrortexture;
	if (gllight) delete gllight;
	if (mFBID != 0) gl.DeleteFramebuffers(1, &mFBID);
}

//===========================================================================
// 
//
//
//===========================================================================

void FGLRenderer::SetupLevel()
{
	mVBO->CreateVBO();
}

void FGLRenderer::Begin2D()
{
	gl_RenderState.EnableFog(false);
	gl_RenderState.Set2DMode(true);
}

//===========================================================================
// 
//
//
//===========================================================================

void FGLRenderer::ProcessLowerMiniseg(seg_t *seg, sector_t * frontsector, sector_t * backsector)
{
	GLWall wall;
	wall.ProcessLowerMiniseg(seg, frontsector, backsector);
	rendered_lines++;
}

//===========================================================================
// 
//
//
//===========================================================================

void FGLRenderer::ProcessSprite(AActor *thing, sector_t *sector)
{
	GLSprite glsprite;
	glsprite.Process(thing, sector);
}

//===========================================================================
// 
//
//
//===========================================================================

void FGLRenderer::ProcessParticle(particle_t *part, sector_t *sector)
{
	GLSprite glsprite;
	glsprite.ProcessParticle(part, sector);//, 0, 0);
}

//===========================================================================
// 
//
//
//===========================================================================

void FGLRenderer::ProcessSector(sector_t *fakesector, subsector_t *sub)
{
	GLFlat glflat;
	glflat.ProcessSector(fakesector, sub);
}

//===========================================================================
// 
//
//
//===========================================================================

void FGLRenderer::FlushTextures()
{
	FMaterial::FlushAll();
}

//===========================================================================
// 
//
//
//===========================================================================

bool FGLRenderer::StartOffscreen()
{
	if (gl.flags & RFL_FRAMEBUFFER)
	{
		if (mFBID == 0) gl.GenFramebuffers(1, &mFBID);
		gl.BindFramebuffer(GL_FRAMEBUFFER, mFBID);
		return true;
	}
	return false;
}

//===========================================================================
// 
//
//
//===========================================================================

void FGLRenderer::EndOffscreen()
{
	if (gl.flags & RFL_FRAMEBUFFER)
	{
		gl.BindFramebuffer(GL_FRAMEBUFFER, 0); 
	}
}

//===========================================================================
// 
//
//
//===========================================================================

unsigned char *FGLRenderer::GetTextureBuffer(FTexture *tex, int &w, int &h)
{
	FMaterial * gltex = FMaterial::ValidateTexture(tex);
	if (gltex)
	{
		return gltex->CreateTexBuffer(CM_DEFAULT, 0, w, h);
	}
	return NULL;
}

//===========================================================================
// 
//
//
//===========================================================================

void FGLRenderer::ClearBorders()
{
	OpenGLFrameBuffer *glscreen = static_cast<OpenGLFrameBuffer*>(screen);

	// Letterbox time! Draw black top and bottom borders.
	int width = glscreen->GetWidth();
	int height = glscreen->GetHeight();
	int trueHeight = glscreen->GetTrueHeight();

	int borderHeight = (trueHeight - height) / 2;

	gl.Viewport(0, 0, width, trueHeight);
	gl.MatrixMode(GL_PROJECTION);
	gl.LoadIdentity();
	gl.Ortho(0.0, width * 1.0, 0.0, trueHeight, -1.0, 1.0);
	gl.MatrixMode(GL_MODELVIEW);
	gl.Color3f(0.f, 0.f, 0.f);
	gl_RenderState.Set2DMode(true);
	gl_RenderState.EnableTexture(false);
	gl_RenderState.Apply(true);

	gl.Begin(GL_QUADS);
	// upper quad
	gl.Vertex2i(0, borderHeight);
	gl.Vertex2i(0, 0);
	gl.Vertex2i(width, 0);
	gl.Vertex2i(width, borderHeight);

	// lower quad
	gl.Vertex2i(0, trueHeight);
	gl.Vertex2i(0, trueHeight - borderHeight);
	gl.Vertex2i(width, trueHeight - borderHeight);
	gl.Vertex2i(width, trueHeight);
	gl.End();

	gl_RenderState.EnableTexture(true);

	gl.Viewport(0, (trueHeight - height) / 2, width, height); 
}

//==========================================================================
//
// Draws a texture
//
//==========================================================================

void FGLRenderer::DrawTexture(FTexture *img, DCanvas::DrawParms &parms)
{
	double xscale = parms.destwidth / parms.texwidth;
	double yscale = parms.destheight / parms.texheight;
	double x = parms.x - parms.left * xscale;
	double y = parms.y - parms.top * yscale;
	double w = parms.destwidth;
	double h = parms.destheight;
	float u1, v1, u2, v2, r, g, b;
	float light = 1.f;

	FMaterial * gltex = FMaterial::ValidateTexture(img);

	const PatchTextureInfo * pti;

	if (parms.colorOverlay)
	{
		// Right now there's only black. Should be implemented properly later
		light = 1.f - APART(parms.colorOverlay)/255.f;
	}

	if (!img->bHasCanvas)
	{
		if (!parms.alphaChannel) 
		{
			int translation = 0;
			if (parms.remap != NULL)
			{
				GLTranslationPalette * pal = static_cast<GLTranslationPalette*>(parms.remap->GetNative());
				if (pal) translation = -pal->GetIndex();
			}
			pti = gltex->BindPatch(CM_DEFAULT, translation);
		}
		else 
		{
			// This is an alpha texture
			pti = gltex->BindPatch(CM_SHADE, 0);
		}

		if (!pti) return;

		u1 = pti->GetUL();
		v1 = pti->GetVT();
		u2 = pti->GetUR();
		v2 = pti->GetVB();
	}
	else
	{
		gltex->Bind(CM_DEFAULT, 0, 0);
		u2=1.f;
		v2=-1.f;
		u1 = v1 = 0.f;
		gl_RenderState.SetTextureMode(TM_OPAQUE);
	}
	
	if (parms.flipX)
	{
		float temp = u1;
		u1 = u2;
		u2 = temp;
	}
	

	if (parms.windowleft > 0 || parms.windowright < parms.texwidth)
	{
		x += parms.windowleft * xscale;
		w -= (parms.texwidth - parms.windowright + parms.windowleft) * xscale;

		u1 = float(u1 + parms.windowleft / parms.texwidth);
		u2 = float(u2 - (parms.texwidth - parms.windowright) / parms.texwidth);
	}

	if (parms.style.Flags & STYLEF_ColorIsFixed)
	{
		r = RPART(parms.fillcolor)/255.0f;
		g = GPART(parms.fillcolor)/255.0f;
		b = BPART(parms.fillcolor)/255.0f;
	}
	else
	{
		r = g = b = light;
	}
	
	// scissor test doesn't use the current viewport for the coordinates, so use real screen coordinates
	int btm = (SCREENHEIGHT - screen->GetHeight()) / 2;
	btm = SCREENHEIGHT - btm;

	gl.Enable(GL_SCISSOR_TEST);
	int space = (static_cast<OpenGLFrameBuffer*>(screen)->GetTrueHeight()-screen->GetHeight())/2;
	gl.Scissor(parms.lclip, btm - parms.dclip + space, parms.rclip - parms.lclip, parms.dclip - parms.uclip);
	
	gl_SetRenderStyle(parms.style, !parms.masked, false);
	if (img->bHasCanvas)
	{
		gl_RenderState.SetTextureMode(TM_OPAQUE);
	}

	gl.Color4f(r, g, b, FIXED2FLOAT(parms.alpha));
	
	gl_RenderState.EnableAlphaTest(false);
	gl_RenderState.Apply();
	gl.Begin(GL_TRIANGLE_STRIP);
	gl.TexCoord2f(u1, v1);
	glVertex2d(x, y);
	gl.TexCoord2f(u1, v2);
	glVertex2d(x, y + h);
	gl.TexCoord2f(u2, v1);
	glVertex2d(x + w, y);
	gl.TexCoord2f(u2, v2);
	glVertex2d(x + w, y + h);
	gl.End();
	gl_RenderState.EnableAlphaTest(true);
	
	gl.Scissor(0, 0, screen->GetWidth(), screen->GetHeight());
	gl.Disable(GL_SCISSOR_TEST);
	gl_RenderState.SetTextureMode(TM_MODULATE);
	gl_RenderState.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	gl_RenderState.BlendEquation(GL_FUNC_ADD);
}

//==========================================================================
//
//
//
//==========================================================================
void FGLRenderer::DrawLine(int x1, int y1, int x2, int y2, int palcolor, uint32 color)
{
	PalEntry p = color? (PalEntry)color : GPalette.BaseColors[palcolor];
	gl_RenderState.EnableTexture(false);
	gl_RenderState.Apply(true);
	gl.Color3ub(p.r, p.g, p.b);
	gl.Begin(GL_LINES);
	gl.Vertex2i(x1, y1);
	gl.Vertex2i(x2, y2);
	gl.End();
	gl_RenderState.EnableTexture(true);
}

//==========================================================================
//
//
//
//==========================================================================
void FGLRenderer::DrawPixel(int x1, int y1, int palcolor, uint32 color)
{
	PalEntry p = color? (PalEntry)color : GPalette.BaseColors[palcolor];
	gl_RenderState.EnableTexture(false);
	gl_RenderState.Apply(true);
	gl.Color3ub(p.r, p.g, p.b);
	gl.Begin(GL_POINTS);
	gl.Vertex2i(x1, y1);
	gl.End();
	gl_RenderState.EnableTexture(true);
}

//===========================================================================
// 
//
//
//===========================================================================

void FGLRenderer::Dim(PalEntry color, float damount, int x1, int y1, int w, int h)
{
	float r, g, b;
	
	gl_RenderState.EnableTexture(false);
	gl_RenderState.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	gl_RenderState.AlphaFunc(GL_GREATER,0);
	gl_RenderState.Apply(true);
	
	r = color.r/255.0f;
	g = color.g/255.0f;
	b = color.b/255.0f;
	
	gl.Begin(GL_TRIANGLE_FAN);
	gl.Color4f(r, g, b, damount);
	gl.Vertex2i(x1, y1);
	gl.Vertex2i(x1, y1 + h);
	gl.Vertex2i(x1 + w, y1 + h);
	gl.Vertex2i(x1 + w, y1);
	gl.End();
	
	gl_RenderState.EnableTexture(true);
}

//==========================================================================
//
//
//
//==========================================================================
void FGLRenderer::FlatFill (int left, int top, int right, int bottom, FTexture *src, bool local_origin)
{
	float fU1,fU2,fV1,fV2;

	FMaterial *gltexture=FMaterial::ValidateTexture(src);
	
	if (!gltexture) return;

	const WorldTextureInfo * wti = gltexture->Bind(CM_DEFAULT, 0, 0);
	if (!wti) return;
	
	if (!local_origin)
	{
		fU1=wti->GetU(left);
		fV1=wti->GetV(top);
		fU2=wti->GetU(right);
		fV2=wti->GetV(bottom);
	}
	else
	{		fU1=wti->GetU(0);
		fV1=wti->GetV(0);
		fU2=wti->GetU(right-left);
		fV2=wti->GetV(bottom-top);
	}
	gl_RenderState.Apply();
	gl.Begin(GL_TRIANGLE_STRIP);
	gl.Color4f(1, 1, 1, 1);
	gl.TexCoord2f(fU1, fV1); gl.Vertex2f(left, top);
	gl.TexCoord2f(fU1, fV2); gl.Vertex2f(left, bottom);
	gl.TexCoord2f(fU2, fV1); gl.Vertex2f(right, top);
	gl.TexCoord2f(fU2, fV2); gl.Vertex2f(right, bottom);
	gl.End();
}

//==========================================================================
//
//
//
//==========================================================================
void FGLRenderer::Clear(int left, int top, int right, int bottom, int palcolor, uint32 color)
{
	int rt;
	int offY = 0;
	PalEntry p = palcolor==-1? (PalEntry)color : GPalette.BaseColors[palcolor];
	int width = right-left;
	int height= bottom-top;
	
	
	rt = screen->GetHeight() - top;
	
	int space = (static_cast<OpenGLFrameBuffer*>(screen)->GetTrueHeight()-screen->GetHeight())/2;	// ugh...
	rt += space;
	/*
	if (!m_windowed && (m_trueHeight != m_height))
	{
		offY = (m_trueHeight - m_height) / 2;
		rt += offY;
	}
	*/
	
	gl.Enable(GL_SCISSOR_TEST);
	gl.Scissor(left, rt - height, width, height);
	
	gl.ClearColor(p.r/255.0f, p.g/255.0f, p.b/255.0f, 0.f);
	gl.Clear(GL_COLOR_BUFFER_BIT);
	gl.ClearColor(0.f, 0.f, 0.f, 0.f);
	
	gl.Disable(GL_SCISSOR_TEST);
}
