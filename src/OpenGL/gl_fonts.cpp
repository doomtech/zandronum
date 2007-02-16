/*
** gl_fonts.cpp
**
** Methods for drawing patches.  I should probably rename this file...
**
**---------------------------------------------------------------------------
** Copyright 2003 Tim Stump
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

#include <windows.h>
#include <gl/gl.h>
#include <gl/glu.h>
#define USE_WINDOWS_DWORD
#include "gl_main.h"
#include "gl_texturelist.h"
#include "doomdef.h"
#include "r_local.h"
#include "p_spec.h"
#include "w_wad.h"
#include "templates.h"

#include "gi.h"

#include "v_text.h"

#if 0
   #define LOG(x) { FILE *f; f = fopen("fonts.log", "a"); fprintf(f, x); fflush(f); fclose(f); }
   #define LOG1(x, a1) { FILE *f; f = fopen("fonts.log", "a"); fprintf(f, x, a1); fflush(f); fclose(f); }
   #define LOG2(x, a1, a2) { FILE *f; f = fopen("fonts.log", "a"); fprintf(f, x, a1, a2); fflush(f); fclose(f); }
   #define LOG3(x, a1, a2, a3) { FILE *f; f = fopen("fonts.log", "a"); fprintf(f, x, a1, a2, a3); fflush(f); fclose(f); }
#else
   #define LOG(x)
   #define LOG1(x, a1)
   #define LOG2(x, a1, a2)
   #define LOG3(x, a1, a2, a3)
#endif


//#define MIN(a, b) ((a < b) ? a : b)


extern TextureList textureList;
extern float CleanXfac, CleanYfac;
extern int NewWidth, NewHeight;

EXTERN_CVAR (Bool, hud_scale)

void GL_vDrawConBackG(FTexture *pic, int width, int height)
{
   int sheight;
   float cx, cy, ty;

   sheight = screen->GetHeight();
   textureList.BindTexture(pic);
   textureList.GetCorner(&cx, &cy);

   ty = 1.f - (height / (sheight * 1.f));

   glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	if (gamestate != GS_FULLCONSOLE && gamestate != GS_STARTUP && gamestate != GS_INTERMISSION)
	{
      glColor4f(1.f, 1.f, 1.f, (1.f - ty) * 1.25f);
	}
   else
   {
      glColor4f(0.5f, 0.5f, 0.5f, 1.f);
   }

	glBegin(GL_TRIANGLE_STRIP);
	   glTexCoord2f(0.f, ty);
      glVertex2i(0, sheight);
	   glTexCoord2f(0.f, cy);
      glVertex2i(0, sheight - height);
     	glTexCoord2f(cx, ty);
      glVertex2i(width, sheight);
      glTexCoord2f(cx, cy);
      glVertex2i(width, sheight - height);
  	glEnd();
}


void GL_DrawTexture(FTexture *tex, float x, float y)
{
   textureList.BindTexture(tex);
   float rx, ry, rw, rh;

   //GL_Set2DMode();

   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

   x = x - tex->LeftOffset;
   y = y - tex->TopOffset;

   rx = ((x - 160) * FRACUNIT * CleanXfac + (screen->GetWidth() * (FRACUNIT / 2))) * INV_FRACUNIT;
   ry = ((y - 100) * FRACUNIT * CleanYfac + (screen->GetHeight() * (FRACUNIT / 2))) * INV_FRACUNIT;
   ry = screen->GetHeight() - ry;

   rw = (tex->GetWidth() * CleanXfac * FRACUNIT) * INV_FRACUNIT;
   rh = (tex->GetHeight() * CleanYfac * FRACUNIT) * INV_FRACUNIT;

   glColor3f(1.f, 1.f, 1.f);
   glBegin(GL_TRIANGLE_STRIP);
      glTexCoord2f(0.f, 0.f);
      glVertex2f(rx, ry);
      glTexCoord2f(0.f, 1.f);
      glVertex2f(rx, ry - rh);
      glTexCoord2f(1.f, 0.f);
      glVertex2f(rx + rw, ry);
      glTexCoord2f(1.f, 1.f);
      glVertex2f(rx + rw, ry - rh);
   glEnd();

   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
}


void GL_DrawWeaponTexture(FTexture *tex, float x, float y)
{
   float rx, ry, rw, rh, cx, cy;

   textureList.AllowScale(false);
   textureList.BindTexture(tex);
   textureList.AllowScale(true);
   textureList.GetCorner(&cx, &cy);

   //GL_Set2DMode();

   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, textureList.GetTextureModeMag());
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, textureList.GetTextureModeMag());

   x = x - tex->LeftOffset;
   y = y - tex->TopOffset;

   rx = x / 320.f * screen->GetWidth();
   ry = y / 200.f * screen->GetHeight();
   ry = screen->GetHeight() - ry;

   rw = tex->GetWidth() / 320.f * screen->GetWidth();
   rh = tex->GetHeight() / 200.f * screen->GetHeight();

   glBegin(GL_TRIANGLE_STRIP);
      glTexCoord2f(0.f, 0.f);
      glVertex2f(rx, ry);
      glTexCoord2f(0.f, cy);
      glVertex2f(rx, ry - rh);
      glTexCoord2f(cx, 0.f);
      glVertex2f(rx + rw, ry);
      glTexCoord2f(cx, cy);
      glVertex2f(rx + rw, ry - rh);
   glEnd();

   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
}


void GL_DrawTextureNotClean(FTexture *tex, float x, float y)
{
   glColor3f(1.f, 1.f, 1.f);
   GL_DrawWeaponTexture(tex, x, y);
}


void GL_DrawTextureTiled(FTexture *tex, int left, int top, int right, int bottom)
{
   textureList.BindTexture(tex);
   float rx, ry, rw, rh, tx1, ty1, tx2, ty2;

   //GL_Set2DMode();

   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

   rx = left * 1.f;
   ry = screen->GetHeight() - (top * 1.f);

   rw = right * 1.f;
   rh = screen->GetHeight() - (bottom * 1.f);

   tx1 = rx / (tex->GetWidth() * 1.f);
   ty1 = top / (tex->GetHeight() * 1.f);
   tx2 = rw / (tex->GetHeight() * 1.f);
   ty2 = bottom / (tex->GetHeight() * 1.f);

   glColor3f(1.f, 1.f, 1.f);
   glBegin(GL_TRIANGLE_STRIP);
      glTexCoord2f(tx1, ty1);
      glVertex2f(rx, ry);
      glTexCoord2f(tx1, ty2);
      glVertex2f(rx, rh);
      glTexCoord2f(tx2, ty1);
      glVertex2f(rw, ry);
      glTexCoord2f(tx2, ty2);
      glVertex2f(rw, rh);
   glEnd();
}


void GL_DrawQuad(int left, int top, int right, int bottom)
{
   //GL_Set2DMode();

   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

   top = screen->GetHeight() - top;
   bottom = screen->GetHeight() - bottom;

   glColor3f(1.f, 1.f, 1.f);
   glBegin(GL_TRIANGLE_FAN);
      glTexCoord2f(0.f, 0.f);
      glVertex2i(left, top);
      glTexCoord2f(0.f, 1.f);
      glVertex2i(left, bottom);
      glTexCoord2f(1.f, 1.f);
      glVertex2i(right, bottom);
      glTexCoord2f(1.f, 0.f);
      glVertex2i(right, top);
   glEnd();
}


void STACK_ARGS GL_DrawTextureVA(FTexture *img, int x0, int y0, uint32 tags_first, ...)
{
	FTexture::Span unmaskedSpan[2];
	const FTexture::Span **spanptr, *spans;
	static BYTE identitymap[256];
	static short bottomclipper[MAXWIDTH], topclipper[MAXWIDTH];
	va_list tags;
	uint32 tag;
	INTBOOL boolval;
	int intval;
	// [BC] Potentially flag the texture as being text so we can handle it differently.
	bool	bIsText = false;

	if (img == NULL || img->UseType == FTexture::TEX_Null)
	{
		return;
	}

	int texwidth = img->GetScaledWidth();
	int texheight = img->GetScaledHeight();

	int windowleft = 0;
	int windowright = texwidth;
	int dclip = screen->GetHeight();
	int uclip = 0;
	int lclip = 0;
	int rclip = screen->GetWidth();
	int destwidth = windowright << FRACBITS;
	int destheight = texheight << FRACBITS;
	int top = img->GetScaledTopOffset();
	int left = img->GetScaledLeftOffset();
	fixed_t alpha = FRACUNIT;
	int fillColor = -1;
	const BYTE *translation = NULL;
	INTBOOL alphaChannel = false;
	INTBOOL flipX = false;
	fixed_t shadowAlpha = 0;
	int shadowColor = 0;
	int virtWidth = screen->GetWidth();
	int virtHeight = screen->GetHeight();
	INTBOOL keepratio = false;
	ERenderStyle style = STYLE_Count;

	// [BC] What's this used for?
	float r, g, b, a, tx, ty;
	r = g = b = a = 1.0f;

	x0 <<= FRACBITS;
	y0 <<= FRACBITS;

	spanptr = &spans;

	// Parse the tag list for attributes
	va_start (tags, tags_first);
	tag = tags_first;

	while (tag != TAG_DONE)
	{
		va_list *more_p;
		uint32 data;

		switch (tag)
		{
		case TAG_IGNORE:
		default:
			data = va_arg (tags, uint32);
			break;

		case TAG_MORE:
			more_p = va_arg (tags, va_list *);
			va_end (tags);
#ifdef __GNUC__
			__va_copy (tags, *more_p);
#else
			tags = *more_p;
#endif
			break;

		case DTA_DestWidth:
			destwidth = va_arg (tags, int) << FRACBITS;
			break;

		case DTA_DestHeight:
			destheight = va_arg (tags, int) << FRACBITS;
			break;

		case DTA_Clean:
			boolval = va_arg (tags, INTBOOL);
			if (boolval)
			{
				x0 = (LONG)((x0 - 160*FRACUNIT) * CleanXfac + (screen->GetWidth( ) * (FRACUNIT/2)));
				y0 = (LONG)((y0 - 100*FRACUNIT) * CleanYfac + (screen->GetHeight( ) * (FRACUNIT/2)));
				destwidth = (LONG)( texwidth * CleanXfac * FRACUNIT );

				// [BC] Text looks horrible when it's y-stretched.
				if ( bIsText )
					destheight = texheight * (int)CleanYfac * FRACUNIT;
				else
					destheight = (LONG)( texheight * CleanYfac * FRACUNIT );
			}
			break;

		case DTA_CleanNoMove:
			boolval = va_arg (tags, INTBOOL);
			if (boolval)
			{
				if ( bIsText )
					destwidth = texwidth * (int)CleanXfac * FRACUNIT;
				else
					destwidth = (LONG)( texwidth * CleanXfac * FRACUNIT );

				// [BC] Text looks horrible when it's y-stretched.
				if ( bIsText )
					destheight = texheight * (int)CleanYfac * FRACUNIT;
				else
					destheight = (LONG)( texheight * CleanYfac * FRACUNIT );
			}
			break;

		case DTA_320x200:
			boolval = va_arg (tags, INTBOOL);
			if (boolval)
			{
				virtWidth = 320;
				virtHeight = 200;
			}
			break;

		case DTA_HUDRules:
			{
				bool xright = x0 < 0;
				bool ybot = y0 < 0;
				intval = va_arg (tags, int);

				if (hud_scale)
				{
					x0 *= CleanXfac;
					if (intval == HUD_HorizCenter)
						x0 += screen->GetWidth() * FRACUNIT / 2;
					else if (xright)
						x0 = screen->GetWidth() * FRACUNIT + x0;
					y0 *= CleanYfac;
					if (ybot)
						y0 = screen->GetHeight() * FRACUNIT + y0;
					destwidth = texwidth * CleanXfac * FRACUNIT;
					destheight = texheight * CleanYfac * FRACUNIT;
				}
				else
				{
					if (intval == HUD_HorizCenter)
						x0 += screen->GetWidth() * FRACUNIT / 2;
					else if (xright)
						x0 = screen->GetWidth() * FRACUNIT + x0;
					if (ybot)
						y0 = screen->GetHeight() * FRACUNIT + y0;
				}
			}
			break;

		case DTA_VirtualWidth:
			virtWidth = va_arg (tags, int);
			break;
			
		case DTA_VirtualHeight:
			virtHeight = va_arg (tags, int);
			break;

		case DTA_Alpha:
			alpha = MIN<fixed_t> (FRACUNIT, va_arg (tags, fixed_t));
			break;

		case DTA_AlphaChannel:
			alphaChannel = va_arg (tags, INTBOOL);
			break;

		case DTA_FillColor:
			fillColor = va_arg (tags, int);
			break;

		case DTA_Translation:
			translation = va_arg (tags, const BYTE *);
			break;

		case DTA_FlipX:
			flipX = va_arg (tags, INTBOOL);
			break;

		case DTA_TopOffset:
			top = va_arg (tags, int);
			break;

		case DTA_LeftOffset:
			left = va_arg (tags, int);
			break;

		case DTA_CenterOffset:
			if (va_arg (tags, int))
			{
				left = texwidth / 2;
				top = texheight / 2;
			}
			break;

		case DTA_CenterBottomOffset:
			if (va_arg (tags, int))
			{
				left = texwidth / 2;
				top = texheight;
			}
			break;

		case DTA_WindowLeft:
			windowleft = va_arg (tags, int);
			break;

		case DTA_WindowRight:
			windowright = va_arg (tags, int);
			break;

		case DTA_ClipTop:
			uclip = va_arg (tags, int);
			if (uclip < 0)
			{
				uclip = 0;
			}
			break;

		case DTA_ClipBottom:
			dclip = va_arg (tags, int);
			if (dclip > screen->GetHeight())
			{
				dclip = screen->GetHeight();
			}
			break;

		case DTA_ClipLeft:
			lclip = va_arg (tags, int);
			if (lclip < 0)
			{
				lclip = 0;
			}
			break;

		case DTA_ClipRight:
			rclip = va_arg (tags, int);
			if (rclip > screen->GetWidth())
			{
				rclip = screen->GetWidth();
			}
			break;

		case DTA_ShadowAlpha:
			shadowAlpha = MIN<fixed_t> (FRACUNIT, va_arg (tags, fixed_t));
			break;

		case DTA_ShadowColor:
			shadowColor = va_arg (tags, int);
			break;

		case DTA_Shadow:
			boolval = va_arg (tags, INTBOOL);
			if (boolval)
			{
				shadowAlpha = FRACUNIT/2;
				shadowColor = 0;
			}
			else
			{
				shadowAlpha = 0;
			}
			break;

		case DTA_Masked:
			boolval = va_arg (tags, INTBOOL);
			if (boolval)
			{
				spanptr = &spans;
			}
			else
			{
				spanptr = NULL;
			}
			break;

		case DTA_KeepRatio:
			keepratio = va_arg (tags, INTBOOL);
			break;

		case DTA_RenderStyle:
			style = ERenderStyle(va_arg (tags, int));
			break;

		// [BC] Is what we're drawing text? If so, handle it differently.
		case DTA_IsText:

			bIsText = !!va_arg( tags, INTBOOL );

			// Don't apply these text rules to the big font.
			if ( screen->Font == BigFont )
				bIsText = false;
			break;
		}
		tag = va_arg (tags, uint32);
	}
	va_end (tags);

	if (virtWidth != screen->GetWidth() || virtHeight != screen->GetHeight())
	{
		int myratio = CheckRatio (screen->GetWidth(), screen->GetHeight());
		if (myratio != 0 && !keepratio)
		{ // The target surface is not 4:3, so expand the specified
		  // virtual size to avoid undesired stretching of the image.
		  // Does not handle non-4:3 virtual sizes. I'll worry about
		  // those if somebody expresses a desire to use them.
			x0 = Scale (screen->GetWidth()*960, x0-virtWidth*FRACUNIT/2, virtWidth*BaseRatioSizes[myratio][0]) + screen->GetWidth()*FRACUNIT/2;
			y0 = Scale (screen->GetHeight(), y0, virtHeight);
			destwidth = FixedDiv (screen->GetWidth()*960 * (destwidth>>FRACBITS), virtWidth*BaseRatioSizes[myratio][0]);
			destheight = FixedDiv (screen->GetHeight() * (destheight>>FRACBITS), virtHeight);
		}
		else
		{
			x0 = Scale (screen->GetWidth(), x0, virtWidth);
			y0 = Scale (screen->GetHeight(), y0, virtHeight);
			destwidth = FixedDiv (screen->GetWidth() * (destwidth>>FRACBITS), virtWidth);
			destheight = FixedDiv (screen->GetHeight() * (destheight>>FRACBITS), virtHeight);
		}
	}

	if (destwidth <= 0 || destheight <= 0)
	{
		return;
	}

   textureList.SetTranslation(translation);
   textureList.LoadAlpha(alphaChannel == 1);
   textureList.AllowScale(false);

   textureList.BindTexture(img);

   textureList.AllowScale(true);
   textureList.LoadAlpha(false);
   textureList.SetTranslation((byte *)NULL);

   textureList.GetCorner(&tx, &ty);

   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, textureList.GetTextureModeMag());
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, textureList.GetTextureModeMag());
   glDisable(GL_ALPHA_TEST);

   x0 -= Scale (left, destwidth, texwidth);
   y0 -= Scale (top, destheight, texheight);

   x0 /= FRACUNIT;
   y0 /= FRACUNIT;
   destwidth /= FRACUNIT;
   destheight /= FRACUNIT;

   y0 = screen->GetHeight() - y0;

   if (fillColor != -1)
   {
      PalEntry *p = &GPalette.BaseColors[fillColor];
      r = byte2float[p->r];
      g = byte2float[p->g];
      b = byte2float[p->b];
   }

   a = alpha * INV_FRACUNIT;

   glColor4f(r, g, b, a);

   if (flipX)
   {
      glBegin(GL_TRIANGLE_STRIP);
         glTexCoord2f(tx, 0.f);
         glVertex2i(x0, y0);
         glTexCoord2f(tx, ty);
         glVertex2i(x0, y0 - destheight);
         glTexCoord2f(0.f, 0.f);
         glVertex2i(x0 + destwidth, y0);
         glTexCoord2f(0.f, ty);
         glVertex2i(x0 + destwidth, y0 - destheight);
      glEnd();
   }
   else
   {
      glBegin(GL_TRIANGLE_STRIP);
         glTexCoord2f(0.f, 0.f);
         glVertex2i(x0, y0);
         glTexCoord2f(0.f, ty);
         glVertex2i(x0, y0 - destheight);
         glTexCoord2f(tx, 0.f);
         glVertex2i(x0 + destwidth, y0);
         glTexCoord2f(tx, ty);
         glVertex2i(x0 + destwidth, y0 - destheight);
      glEnd();
   }

   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
   glEnable(GL_ALPHA_TEST);
}
