#include "gl_pch.h"
/*
** gl_draw.cpp
** 2D drawing functions
**
**---------------------------------------------------------------------------
** Copyright 2000-2005 Christoph Oelckers
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

#include "files.h"
#include "m_swap.h"
#include "r_draw.h"
#include "v_video.h"
#include "gl/gl_struct.h"
#include "gl/gl_texture.h"
#include "gl/gl_functions.h"
#include "win32iface.h"
#include "gl/win32gliface.h"

//==========================================================================
//
//
//
//==========================================================================
void gl_DrawTexture(FTexInfo *texInfo)
{
	float x, y, w, h;
	float ox, oy, cx, cy, r, g, b;
	
	x = texInfo->x;
	y = texInfo->y;
	w = texInfo->width;
	// add one since it looks like the software functions are inclusive of the entire height (0..n) while
	// the GL functions are exclusive (0..n-1)
	h = texInfo->height;
	
	FGLTexture * gltex = FGLTexture::ValidateTexture(texInfo->tex);

	const PatchTextureInfo * pti;
	
	if (!texInfo->tex->bHasCanvas)
	{
		if (!texInfo->loadAlpha) 
		{
			int translationindex;

			if (texInfo->tex->UseType == FTexture::TEX_FontChar)
			{
				// Try to get the true color mapping from the paletted mapping which is being passed here
				//
				// Too bad that there is no decent way to get the index directly so the only way to get it
				// is to analyze the table's contents.
				byte * index0 = texInfo->font->GetColorTranslation(CR_BRICK);
				byte * index1 = texInfo->font->GetColorTranslation(CR_TAN);
				translationindex = (texInfo->translation - index0) / (index1 - index0);
				if (translationindex<0 || translationindex>=NUM_TEXT_COLORS) 
				{
					translationindex=CR_UNTRANSLATED;
				}

				// now get the corrseponding True Color table from the font.
				byte * tctstart = index0 + (NUM_TEXT_COLORS * (index1-index0));
				texInfo->translation = tctstart + 3 * (index1-index0) * translationindex;
			}
			else
			{
				// Aside from fonts these are the only ones being used by ZDoom and they are better passed by ID.
				//
				// If ZDoom changes its use of translation tables this has to be adjusted for it!
				//
				if (texInfo->translation >= translationtables[TRANSLATION_Players] &&
					texInfo->translation <  translationtables[TRANSLATION_Players] + (MAXPLAYERS+1)*256)
				{
					int in = texInfo->translation - translationtables[TRANSLATION_Players];
					translationindex = TRANSLATION(TRANSLATION_Players, in);
				}
				else
				{
					translationindex=0;
				}
				texInfo->translation=NULL;
			}
			pti = gltex->BindPatch(CM_DEFAULT, translationindex, texInfo->translation);
		}
		else 
		{
			// This is an alpha texture
			pti = gltex->BindPatch(CM_SHADE, 0);
		}

		if (!pti) return;

		cx = pti->GetUR();
		cy = pti->GetVB();
	}
	else
	{
		gltex->Bind(CM_DEFAULT);
		cx=1.f;
		cy=-1.f;
	}
	ox = oy = 0.f;
	
	if (texInfo->flipX)
	{
		float temp = ox;
		ox = cx;
		cx = temp;
	}
	
	// also take into account texInfo->windowLeft and texInfo->windowRight
	// just ignore for now...
	if (texInfo->windowLeft || texInfo->windowRight != texInfo->tex->GetScaledWidth()) return;
	
	if (texInfo->fillColor != -1)
	{
		r = RPART(texInfo->fillColor)/255.0f;
		g = GPART(texInfo->fillColor)/255.0f;
		b = BPART(texInfo->fillColor)/255.0f;
	}
	else
	{
		r = g = b = 1.f;
	}
	
	// scissor test doesn't use the current viewport for the coordinates, so use real screen coordinates
	int btm = (SCREENHEIGHT - screen->GetHeight()) / 2;
	btm = SCREENHEIGHT - btm;

	gl.Enable(GL_SCISSOR_TEST);
	int space = (static_cast<Win32GLFrameBuffer*>(screen)->GetTrueHeight()-screen->GetHeight())/2;	// ugh...
	gl.Scissor(texInfo->clipLeft, btm - texInfo->clipBottom+space, texInfo->clipRight - texInfo->clipLeft, texInfo->clipBottom - texInfo->clipTop);
	
	if (!texInfo->masked) gl.SetTextureMode(TM_OPAQUE);
	gl.Color4f(r, g, b, texInfo->alpha);
	
	gl.Disable(GL_ALPHA_TEST);
	gl.Begin(GL_TRIANGLE_STRIP);
	gl.TexCoord2f(ox, oy);
	gl.Vertex2i(x, y);
	gl.TexCoord2f(ox, cy);
	gl.Vertex2i(x, y + h);
	gl.TexCoord2f(cx, oy);
	gl.Vertex2i(x + w, y);
	gl.TexCoord2f(cx, cy);
	gl.Vertex2i(x + w, y + h);
	gl.End();
	gl.Enable(GL_ALPHA_TEST);
	
	gl.Scissor(0, 0, screen->GetWidth(), screen->GetHeight());
	gl.Disable(GL_SCISSOR_TEST);
	if (!texInfo->masked) gl.SetTextureMode(TM_MODULATE);
}


//==========================================================================
//
// Draws a byte buffer
//
//==========================================================================
void gl_DrawBuffer(byte * sbuffer, int width, int height, int x, int y, int dx, int dy, PalEntry * palette)
{
	if (palette==NULL) palette=GPalette.BaseColors;

	byte *buffer = new byte[width * height * 4 + width * 4];

	for (int yy = 0; yy < height; yy++)
	{
		for (int xx = 0; xx < width; xx++)
		{
			int index = xx + (yy * width);
			PalEntry p = palette[sbuffer[index]];
			buffer[(index * 4) + 0] = p.r;
			buffer[(index * 4) + 1] = p.g;
			buffer[(index * 4) + 2] = p.b;
			buffer[(index * 4) + 3] = 255;
		}
	}

	GLTexture * gltex = new GLTexture(width, height, false, false);
	gltex->CreateTexture(buffer, width, height, false, CM_DEFAULT);
	delete[] buffer;

	gl.Begin(GL_TRIANGLE_STRIP);
	gl.TexCoord2f(0, 0);
	gl.Vertex2i(x, y);
	gl.TexCoord2f(0, gltex->GetVB());
	gl.Vertex2i(x, y+dy);
	gl.TexCoord2f(gltex->GetUR(), 0);
	gl.Vertex2i(x+dx, y);
	gl.TexCoord2f(gltex->GetUR(), gltex->GetVB());
	gl.Vertex2i(x+dx, y+dy);
	gl.End();
	gl.Flush();
	delete gltex;
}

//==========================================================================
//
// Draws a canvas
//
//==========================================================================
void gl_DrawCanvas(DCanvas * canvas, int x, int y, int dx, int dy, PalEntry * palette)
{
	canvas->Lock();
	gl_DrawBuffer(canvas->GetBuffer(), canvas->GetWidth(), canvas->GetHeight(), x, y, dx, dy, palette);
	canvas->Unlock();
}

//==========================================================================
//
// Draws the savegame picture with its original palette
//
//==========================================================================
void gl_DrawSavePic(DCanvas * canvas, const char * Filename, int x, int y, int dx, int dy)
{
	static const char * cmp_filename=NULL;
	static BYTE * cmp_buffer=NULL;
	static union
	{
		DWORD palette[256];
		BYTE pngpal[256][3];
	};
	DWORD len,id;

	canvas->Lock();

	// I hope this is sufficient!
	if (cmp_filename!=Filename || canvas->GetBuffer()!=cmp_buffer)
	{
		cmp_filename = Filename;
		cmp_buffer = canvas->GetBuffer();
		// Read the palette from the savegame.
		FILE * file = fopen (Filename, "rb");
		if (file)
		{
			FileReader fr(file);

			fr.Seek (33, SEEK_SET);

			fr >> len >> id;
			while (id != MAKE_ID('I','D','A','T') && id != MAKE_ID('I','E','N','D'))
			{
				len = BigLong((unsigned int)len);
				if (id == MAKE_ID('P','L','T','E'))
				{
					int PaletteSize = MIN<int> (len / 3, 256);
					fr.Read (pngpal, PaletteSize * 3);
					if (PaletteSize * 3 != (int)len)
					{
						fr.Seek (len - PaletteSize * 3, SEEK_CUR);
					}
					for (int i = PaletteSize - 1; i >= 0; --i)
					{
						palette[i] = MAKERGB(pngpal[i][0], pngpal[i][1], pngpal[i][2]);
					}
				}
				else
				{
					fr.Seek (len, SEEK_CUR);
				}
				fr >> len >> len;	// Skip CRC
				id = MAKE_ID('I','E','N','D');
				fr >> id;
			}
			fclose(file);
		}
		else memcpy(palette, GPalette.BaseColors, sizeof(palette));
	}
	gl_DrawBuffer(canvas->GetBuffer(), canvas->GetWidth(), canvas->GetHeight(), x, y, dx, dy, (PalEntry*)palette);
	canvas->Unlock();
}

//==========================================================================
//
//
//
//==========================================================================
void gl_DrawLine(int x1, int y1, int x2, int y2, int color)
{
	PalEntry p = color&0xff000000? color : GPalette.BaseColors[color];
	gl_EnableTexture(false);
	gl.Color3ub(p.r, p.g, p.b);
	gl.Begin(GL_LINES);
	gl.Vertex2i(x1, y1);
	gl.Vertex2i(x2, y2);
	gl.End();
	gl_EnableTexture(true);
}


//==========================================================================
//
//
//
//==========================================================================
void Win32GLFrameBuffer::Dim(PalEntry) const
{
	// Unlike in the software renderer the color is being ignored here because
	// view blending only affects the actual view with the GL renderer.
	Dim((DWORD)dimcolor , dimamount, 0, 0, GetWidth(), GetHeight());
}

void Win32GLFrameBuffer::Dim(PalEntry color, float damount, int x1, int y1, int w, int h) const
{
	float r, g, b;
	
	gl_EnableTexture(false);
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
}

//==========================================================================
//
//
//
//==========================================================================
void Win32GLFrameBuffer::FlatFill (int left, int top, int right, int bottom, FTexture *src)
{
	float fU1,fU2,fV1,fV2;

	FGLTexture *gltexture=FGLTexture::ValidateTexture(src);
	
	if (!gltexture) return;

	const WorldTextureInfo * wti = gltexture->Bind(CM_DEFAULT);
	if (!wti) return;
	
	fU1=wti->GetU(left);
	fV1=wti->GetV(top);
	fU2=wti->GetU(right);
	fV2=wti->GetV(bottom);
	gl.Begin(GL_TRIANGLE_STRIP);
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
void Win32GLFrameBuffer::Clear(int left, int top, int right, int bottom, int color) const
{
	int rt;
	int offY = 0;
	PalEntry p = GPalette.BaseColors[color];
	int width = right-left;
	int height= bottom-top;
	
	
	rt = screen->GetHeight() - top;
	
	int space = (static_cast<Win32GLFrameBuffer*>(screen)->GetTrueHeight()-screen->GetHeight())/2;	// ugh...
	rt += space;
	/*
	if (!m_windowed && (m_trueHeight != m_height))
	{
		offY = (m_trueHeight - m_height) / 2;
		rt += offY;
	}
	*/
	
	gl_SetColorMode(CM_DEFAULT);
	gl.Enable(GL_SCISSOR_TEST);
	gl.Scissor(left, rt - height, width, height);
	
	gl.ClearColor(p.r/255.0f, p.g/255.0f, p.b/255.0f, 0.f);
	gl.Clear(GL_COLOR_BUFFER_BIT);
	gl.ClearColor(0.f, 0.f, 0.f, 0.f);
	
	gl.Disable(GL_SCISSOR_TEST);
}
