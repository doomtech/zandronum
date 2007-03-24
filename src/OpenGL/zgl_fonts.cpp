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
#include "OpenGLVideo.h"
#include "zgl_main.h"
#include "zgl_texturelist.h"
#include "doomdef.h"
#include "r_local.h"
#include "p_spec.h"
#include "w_wad.h"
#include "templates.h"

#include "gi.h"

#include "v_text.h"

EXTERN_CVAR (Bool, hud_scale)

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
extern int CleanXfac, CleanYfac;
extern int NewWidth, NewHeight;


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


void GL_DrawWeaponTexture(FTexture *tex, float x1, float y1, float x2, float y2)
{
   float cx, cy;

   textureList.AllowScale(false);
   textureList.BindTexture(tex);
   textureList.AllowScale(true);
   textureList.GetCorner(&cx, &cy);

   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, textureList.GetTextureModeMag());
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, textureList.GetTextureModeMag());

   glBegin(GL_TRIANGLE_STRIP);
      glTexCoord2f(0.f, 0.f);
      glVertex2f(x1, y1);
      glTexCoord2f(0.f, cy);
      glVertex2f(x1, y2);
      glTexCoord2f(cx, 0.f);
      glVertex2f(x2, y1);
      glTexCoord2f(cx, cy);
      glVertex2f(x2, y2);
   glEnd();
}


void GL_DrawQuad(int left, int top, int right, int bottom)
{
   //GL_Set2DMode();

   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
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


void GL_DrawTexture(FTexInfo *texInfo)
{
   int x, y, w, h;
   float ox, oy, cx, cy, r, g, b;

   x = texInfo->x;
   y = screen->GetHeight() - texInfo->y;
   w = texInfo->width;
   // add one since it looks like the software functions are inclusive of the entire height (0..n) while
   // the GL functions are exclusive (0..n-1)
   h = texInfo->height;

   textureList.LoadAlpha(texInfo->loadAlpha);
   textureList.SetTranslation(texInfo->translation);
   textureList.AllowScale(false);

   textureList.BindTexture(texInfo->tex);

   textureList.SetTranslation((BYTE *)NULL);
   textureList.LoadAlpha(false);
   textureList.AllowScale(true);

   ox = oy = 0.f;
   textureList.GetCorner(&cx, &cy);

   if (texInfo->flipX)
   {
      float temp = ox;
      ox = cx;
      cx = temp;
   }

   // also take into account texInfo->windowLeft and texInfo->windowRight
   // just ignore for now...
   if (texInfo->windowLeft || texInfo->windowRight != texInfo->tex->GetWidth()) return;

   if (texInfo->fillColor != -1)
   {
      PalEntry *p = &screen->GetPalette()[texInfo->fillColor];
      r = byte2float[p->r];
      g = byte2float[p->g];
      b = byte2float[p->b];
   }
   else
   {
      r = g = b = 1.f;
   }

   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, textureList.GetTextureModeMag());
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, textureList.GetTextureModeMag());

   // scissor test doesn't use the current viewport for the coordinates, so use real screen coordinates
   int btm = (static_cast<OpenGLFrameBuffer *>(screen)->TrueHeight() - screen->GetHeight()) / 2;
   btm = static_cast<OpenGLFrameBuffer *>(screen)->TrueHeight() - btm;
   glEnable(GL_SCISSOR_TEST);
   glScissor(texInfo->clipLeft, btm - texInfo->clipBottom, texInfo->clipRight - texInfo->clipLeft, texInfo->clipBottom - texInfo->clipTop);

   glColor4f(r, g, b, texInfo->alpha);

   glDisable(GL_ALPHA_TEST);
   glBegin(GL_TRIANGLE_STRIP);
      glTexCoord2f(ox, oy);
      glVertex2i(x, y);
      glTexCoord2f(ox, cy);
      glVertex2i(x, y - h);
      glTexCoord2f(cx, oy);
      glVertex2i(x + w, y);
      glTexCoord2f(cx, cy);
      glVertex2i(x + w, y - h);
   glEnd();
   glEnable(GL_ALPHA_TEST);

   glScissor(0, 0, screen->GetWidth(), screen->GetHeight());
   glDisable(GL_SCISSOR_TEST);

   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
}
