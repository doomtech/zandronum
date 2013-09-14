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
// Main renderer class
//

#include "gl/gl_include.h"
#include "gl/gl_intern.h"
#include "textures/textures.h"
#include "textures/bitmap.h"
#include "w_wad.h"
#include "c_cvars.h"
#include "i_system.h"
#include "v_palette.h"
#include "gl/new_renderer/gl2_renderer.h"
#include "gl/new_renderer/gl2_vertex.h"
#include "gl/new_renderer/textures/gl2_material.h"
#include "gl/new_renderer/textures/gl2_texture.h"
#include "gl/new_renderer/textures/gl2_shader.h"
#include "gl/common/glc_texture.h"
#include "gl/common/glc_translate.h"



namespace GLRendererNew
{

GL2Renderer *GLRenderer2;

//===========================================================================
// 
// Destroy renderer
//
//===========================================================================

GL2Renderer::~GL2Renderer()
{
	for(unsigned i=0;i<mMaterials.Size();i++)
	{
		delete mMaterials[i];
	}
	mMaterials.Clear();
	if (mShaders != NULL) delete mShaders;
	if (mTextures != NULL) delete mTextures;
	if (mRender2D != NULL) delete mRender2D;
}

//===========================================================================
// 
// Initialize renderer
//
//===========================================================================

void GL2Renderer::Initialize()
{
	mShaders = new FShaderContainer;
	mTextures = new FGLTextureManager;
	mRender2D = new FPrimitiveBuffer2D;
}

//===========================================================================
// 
// Pause renderer
//
//===========================================================================

void GL2Renderer::SetPaused()
{
	mShaders->SetActiveShader(NULL);
	gl.SetTextureMode(TM_MODULATE);
}

//===========================================================================
// 
// Unpause renderer
//
//===========================================================================

void GL2Renderer::UnsetPaused()
{
	gl.SetTextureMode(TM_MODULATE);
}

//===========================================================================
// 
//
//
//===========================================================================

void GL2Renderer::ProcessWall(seg_t *seg, sector_t *sector, sector_t *backsector, subsector_t *polysub)
{
}

//===========================================================================
// 
//
//
//===========================================================================

void GL2Renderer::ProcessLowerMiniseg(seg_t *seg, sector_t * frontsector, sector_t * backsector)
{
}

//===========================================================================
// 
//
//
//===========================================================================

void GL2Renderer::ProcessSprite(AActor *thing, sector_t *sector)
{
}

//===========================================================================
// 
//
//
//===========================================================================

void GL2Renderer::ProcessParticle(particle_t *part, sector_t *sector)
{
}

//===========================================================================
// 
//
//
//===========================================================================

void GL2Renderer::ProcessSector(sector_t *fakesector, subsector_t *sub)
{
}

//===========================================================================
// 
//
//
//===========================================================================

void GL2Renderer::FlushTextures()
{
	if (mTextures != NULL) mTextures->FlushAllTextures();
}

//===========================================================================
// 
//
//
//===========================================================================

void GL2Renderer::RenderTextureView (FCanvasTexture *self, AActor *viewpoint, int fov)
{
}

//===========================================================================
// 
//
//
//===========================================================================

void GL2Renderer::PrecacheTexture(FTexture *tex)
{
}

//===========================================================================
// 
//
//
//===========================================================================

void GL2Renderer::UncacheTexture(FTexture *tex)
{
}

//===========================================================================
// 
//
//
//===========================================================================

unsigned char *GL2Renderer::GetTextureBuffer(FTexture *tex, int &w, int &h)
{
	return NULL;
}

//===========================================================================
// 
//
//
//===========================================================================

void GL2Renderer::SetupLevel()
{
}

//===========================================================================
// 
//
//
//===========================================================================

void GL2Renderer::CleanLevelData()
{
}

//===========================================================================
// 
//
//
//===========================================================================

void GL2Renderer::ClearBorders()
{
}

//===========================================================================
// 
//
//
//===========================================================================

void GL2Renderer::Begin2D()
{
}

//==========================================================================
//
// Draws a texture
//
//==========================================================================

void GL2Renderer::DrawTexture(FTexture *img, DCanvas::DrawParms &parms)
{
	float x = FIXED2FLOAT(parms.x - Scale (parms.left, parms.destwidth, parms.texwidth));
	float y = FIXED2FLOAT(parms.y - Scale (parms.top, parms.destheight, parms.texheight));
	float w = FIXED2FLOAT(parms.destwidth);
	float h = FIXED2FLOAT(parms.destheight);
	float ox, oy, cx, cy;
	
	int r, g, b;
	int light = 255;
	int translation = 0;

	if (parms.colorOverlay)
	{
		// Right now there's only black. Should be implemented properly later
		light = 255 - APART(parms.colorOverlay);
	}

	if (!img->bHasCanvas)
	{
		if (!parms.alphaChannel) 
		{
			translation = 0;
			if (parms.remap != NULL)
			{
				translation = GLTranslationPalette::GetIndex(parms.remap);
			}
		}
		else 
		{
			// This is an alpha texture
			translation = TRANSLATION_SHADE;
		}

		ox = oy = 0.f;
		cx = cy = 1.f;
	}
	else
	{
		translation = 0;
		cx=1.f;
		cy=-1.f;
		ox = oy = 0.f;
	}
	
	FMaterial *mat = GetMaterial(img, true, translation);

	if (parms.flipX)
	{
		ox = 1.f;
		cx = 0.f;
	}
	
	// also take into account texInfo->windowLeft and texInfo->windowRight
	// just ignore for now...
	if (parms.windowleft || parms.windowright != img->GetScaledWidth()) return;
	

	FPrimitive2D *prim;
	FVertex2D *vert;

	int vtindex = mRender2D->NewPrimitive(4, prim, vert);
	if (vtindex >= 0)
	{
		prim->mVertexStart = vtindex;
		prim->mVertexCount = 4;

		prim->mScissor[0] = parms.lclip;
		prim->mScissor[1] = parms.uclip;
		prim->mScissor[2] = parms.rclip;
		prim->mScissor[3] = parms.dclip;

		prim->mUseScissor = (parms.lclip > 0 || parms.uclip > 0 || 
							parms.rclip < screen->GetWidth() || parms.dclip < screen->GetHeight());
	
		gl_GetRenderStyle(parms.style, !parms.masked, false, 
					&prim->mTextureMode, &prim->mSrcBlend, &prim->mDstBlend, &prim->mBlendEquation);

		prim->mPrimitiveType = GL_TRIANGLE_STRIP;

		vert[0].x = x;      vert[0].y = y;
		vert[0].u = ox;     vert[0].v = oy;

		vert[1].x = x;      vert[1].y = y + h;
		vert[1].u = ox;     vert[1].v = cy;

		vert[2].x = x + w;  vert[2].y = y;
		vert[2].u = cx;     vert[2].v = oy;

		vert[3].x = x + w;  vert[3].y = y + h;
		vert[3].u = cx;     vert[3].v = cy;

		if (parms.style.Flags & STYLEF_ColorIsFixed)
		{
			r = RPART(parms.fillcolor);
			g = GPART(parms.fillcolor);
			b = BPART(parms.fillcolor);
		}
		else
		{
			r = g = b = light;
		}
		int a = Scale(parms.alpha, 255, FRACUNIT);

		for(int i=0;i<4;i++)
		{
			vert[i].r = (unsigned char)r;
			vert[i].g = (unsigned char)g;
			vert[i].b = (unsigned char)b;
			vert[i].a = (unsigned char)a;
		}
	}
}

//==========================================================================
//
//
//
//==========================================================================
void GL2Renderer::DrawLine(int x1, int y1, int x2, int y2, int palcolor, uint32 color)
{
#if 0
	PalEntry p = color? (PalEntry)color : GPalette.BaseColors[color];
	gl_EnableTexture(false);
	gl_DisableShader();
	gl.Color3ub(p.r, p.g, p.b);
	gl.Begin(GL_LINES);
	gl.Vertex2i(x1, y1);
	gl.Vertex2i(x2, y2);
	gl.End();
	gl_EnableTexture(true);
#endif
}

//==========================================================================
//
//
//
//==========================================================================
void GL2Renderer::DrawPixel(int x1, int y1, int palcolor, uint32 color)
{
#if 0
	PalEntry p = color? (PalEntry)color : GPalette.BaseColors[color];
	gl_EnableTexture(false);
	gl_DisableShader();
	gl.Color3ub(p.r, p.g, p.b);
	gl.Begin(GL_POINTS);
	gl.Vertex2i(x1, y1);
	gl.End();
	gl_EnableTexture(true);
#endif
}

//===========================================================================
// 
//
//
//===========================================================================

void GL2Renderer::Dim(PalEntry color, float damount, int x1, int y1, int w, int h)
{
#if 0
	float r, g, b;
	
	gl_EnableTexture(false);
	gl_DisableShader();
	gl.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	gl.AlphaFunc(GL_GREATER,0);
	
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
	
	gl_EnableTexture(true);
#endif
}

//==========================================================================
//
//
//
//==========================================================================
void GL2Renderer::FlatFill (int left, int top, int right, int bottom, FTexture *src, bool local_origin)
{
#if 0
	float fU1,fU2,fV1,fV2;

	FGLTexture *gltexture=FGLTexture::ValidateTexture(src);
	
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
	gl_ApplyShader();
	gl.Begin(GL_TRIANGLE_STRIP);
	gl.TexCoord2f(fU1, fV1); gl.Vertex2f(left, top);
	gl.TexCoord2f(fU1, fV2); gl.Vertex2f(left, bottom);
	gl.TexCoord2f(fU2, fV1); gl.Vertex2f(right, top);
	gl.TexCoord2f(fU2, fV2); gl.Vertex2f(right, bottom);
	gl.End();
#endif
}

//==========================================================================
//
//
//
//==========================================================================
void GL2Renderer::Clear(int left, int top, int right, int bottom, int palcolor, uint32 color)
{
#if 0
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
	
	gl_DisableShader();

	gl.Enable(GL_SCISSOR_TEST);
	gl.Scissor(left, rt - height, width, height);
	
	gl.ClearColor(p.r/255.0f, p.g/255.0f, p.b/255.0f, 0.f);
	gl.Clear(GL_COLOR_BUFFER_BIT);
	gl.ClearColor(0.f, 0.f, 0.f, 0.f);
	
	gl.Disable(GL_SCISSOR_TEST);
#endif
}

//-----------------------------------------------------------------------------
//
// gl_SetFixedColormap
//
//-----------------------------------------------------------------------------

void GL2Renderer::SetFixedColormap (player_t *player)
{
}

//===========================================================================
//
// Render the view to a savegame picture
//
//===========================================================================

void GL2Renderer::WriteSavePic (player_t *player, FILE *file, int width, int height)
{
}

//-----------------------------------------------------------------------------
//
// R_RenderPlayerView - the main rendering function
//
//-----------------------------------------------------------------------------

void GL2Renderer::RenderView (player_t* player)
{       
}

//-----------------------------------------------------------------------------
//
// gets the GL texture for a specific texture
//
//-----------------------------------------------------------------------------

FGLTexture *GL2Renderer::GetGLTexture(FTexture *tex, bool asSprite, int translation)
{
	return mTextures->GetTexture(tex, asSprite, translation);
}

//-----------------------------------------------------------------------------
//
// gets the material for a specific texture
//
//-----------------------------------------------------------------------------

FMaterial *GL2Renderer::GetMaterial(FTexture *tex, bool asSprite, int translation)
{
	FMaterialContainer *matc = static_cast<FMaterialContainer*>(tex->gl_info.RenderTexture);

	if (matc == NULL)
	{
		tex->gl_info.RenderTexture = matc = new FMaterialContainer(tex);
		mMaterials.Push(matc);
	}
	return matc->GetMaterial(asSprite, translation);

}

FMaterial *GL2Renderer::GetMaterial(FTextureID no, bool animtrans, bool asSprite, int translation)
{
	return GetMaterial(animtrans? TexMan(no) : TexMan[no], asSprite, translation);
}

}