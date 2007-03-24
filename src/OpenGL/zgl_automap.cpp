/*
** gl_automap.cpp
** Routines for drawing a Duke3D style automap.
**
**---------------------------------------------------------------------------
** Copyright 2003 Timothy Stump
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

#define USE_WINDOWS_DWORD
#include "zgl_main.h"
#include "r_defs.h"

CVAR(Bool, gl_automap_dukestyle, false, CVAR_ARCHIVE)
CVAR(Float, gl_automap_transparency, 0.85f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);
CVAR(Bool, gl_automap_glow, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);
CVAR(Int, gl_automap_glowsize, 40, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);
EXTERN_CVAR(Int, am_rotate);
EXTERN_CVAR(Bool, st_scale);
EXTERN_CVAR(Int, screenblocks);
EXTERN_CVAR(Int, am_cheat);

#define MAPBITS 12
#define MAPUNIT (1<<MAPBITS)
#define FRACTOMAPBITS (FRACBITS-MAPBITS)

extern fixed_t scale_mtof;
extern fixed_t m_x2, m_y2;
extern fixed_t m_x, m_y;
extern fixed_t m_w, m_h;
extern int f_w;
extern int f_h;
extern int f_x;
extern int f_y;
float automapScale;


void GL_DrawMappedSubsector(subsector_t *subSec)
{
   int i, index, floorLight, ceilingLight;
   float x, y, tx, ty, color, alpha;
   seg_t *seg;
   sector_t *sec, ts;
   gl_poly_t *poly;
   PalEntry *p;

   if (subSec->isPoly) return;

   sec = GL_FakeFlat(subSec->render_sector, &ts, &floorLight, &ceilingLight, false);
   if (!sec) return;
   if (sec->floorpic == skyflatnum) return;

   if (viewactive)
   {
      alpha = gl_automap_transparency;
   }
   else
   {
      alpha = 1.f;
   }

   p = &sec->ColorMap->Color;
   color = floorLight / LIGHTLEVELMAX;

   poly = &gl_polys[(subSec->index * 2) + 0];
   if (!poly) return; // hmm, NULL poly, so somethings wrong...
   // make sure the subsector has been calculated...
   if (!poly->initialized)
   {
      GL_RecalcSubsector(subSec, sec);
      if (!poly->initialized) return;
   }
   textureList.SetTranslation(sec->ColorMap->Maps);
   textureList.BindTexture(sec->floorpic, true);
   textureList.SetTranslation((byte *)NULL);

   glColor4f(color * byte2float[p->r], color * byte2float[p->g], color * byte2float[p->b], alpha);

   glBegin(GL_TRIANGLE_FAN);
      index = 0;
      for (i = subSec->numlines - 1; i >= 0; i--)
      {
         seg = &segs[subSec->firstline + i];
         x = (seg->v2->x >> FRACTOMAPBITS) * automapScale * MAP_SCALE;
         y = (seg->v2->y >> FRACTOMAPBITS) * automapScale * MAP_SCALE;
         tx = poly->texCoords[(index * 2) + 0];
         ty = poly->texCoords[(index * 2) + 1];
         glTexCoord2f(tx, ty);
         glVertex2f(x, y);
         index++;
      }
      x = (seg->v1->x >> FRACTOMAPBITS) * automapScale * MAP_SCALE;
      y = (seg->v1->y >> FRACTOMAPBITS) * automapScale * MAP_SCALE;
      tx = poly->texCoords[(index * 2) + 0];
      ty = poly->texCoords[(index * 2) + 1];
      glTexCoord2f(tx, ty);
      glVertex2f(x, y);
   glEnd();
}

void GL_DrawDukeAutomap()
{
   subsector_t *subSec;
   float yaw, x, y;
   int i, sw, sh, stOffset;

   automapScale = scale_mtof * (1.f / 0.018f * 2.f) * INV_FRACUNIT;
   automapScale *= 2.3f;

   glDisable(GL_DEPTH_TEST);
   if (viewactive)
   {
      GL_SetupViewport();
      sw = realviewwidth;
      sh = realviewheight;
      glScalef(screen->GetWidth() * 1.f / sw, screen->GetHeight() * 1.f / sh, 1.f);
      stOffset = 0;
      glBlendFunc(GL_SRC_ALPHA, GL_ONE);
   }
   else
   {
      sw = screen->GetWidth();
      sh = screen->GetHeight();
      stOffset = GL_GetStatusBarOffset();
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   }

   x = (m_x + m_x2) * automapScale * (INV_FRACUNIT / 2);
   y = (m_y + m_y2) * automapScale * (INV_FRACUNIT / 2);

   glTranslatef(sw * 0.5f, (sh + stOffset) * 0.5f, 0.f);

   if (am_rotate)
   {
      yaw = 270.f - ANGLE_TO_FLOAT(camera->angle);
      yaw -= 180.f;
      glRotatef(yaw, 0.f, 0.f, 1.f);
   }

   glTranslatef(-x, -y, 0.f);

   for (i = 0; i < numsubsectors; i++)
   {
      subSec = subsectors + i;
      if (subSec->isMapped || am_cheat) GL_DrawMappedSubsector(subSec);
   }
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

   glEnable(GL_DEPTH_TEST);

   glScalef(1.f, 1.f, 1.f);
   GL_Set2DMode();
   GL_ResetViewport();
}


void GL_DrawLine(int x1, int y1, int x2, int y2, PalEntry *p)
{
   glDisable(GL_TEXTURE_2D);
   glColor3ub(p->r, p->g, p->b);
   glBegin(GL_LINES);
      glVertex2i(x1, screen->GetHeight() - y1);
      glVertex2i(x2, screen->GetHeight() - y2);
   glEnd();
   glEnable(GL_TEXTURE_2D);
}


void GL_DrawGlowLine(int x1, int y1, int x2, int y2, PalEntry *p)
{
   float unit[2], normal[2];
   float dx, dy, length;
   float thickness = gl_automap_glowsize * (scale_mtof * INV_FRACUNIT) * 2.5f + 3;

   y1 = screen->GetHeight() - y1;
   y2 = screen->GetHeight() - y2;

   dx = x2 * 1.f - x1;
   dy = y2 * 1.f - y1;

   length = sqrtf(dx*dx + dy*dy);
   if (length <= 0.f) return;

   unit[0] = dx / length;
   unit[1] = dy / length;
   normal[0] = unit[1];
   normal[1] = -unit[0];

   textureList.BindTexture("GLPART");
   glColor3ub(p->r, p->g, p->b);

   glBlendFunc(GL_SRC_ALPHA, GL_ONE);
   glBegin(GL_QUADS);
	   // Start of the line.
	   glTexCoord2f(0.f, 0.f);
	   glVertex2f(x1 - unit[0] * thickness + normal[0] * thickness, y1 - unit[1] * thickness + normal[1] * thickness);
	   glTexCoord2f(0.5f, 0.f);
	   glVertex2f(x1 + normal[0] * thickness, y1 + normal[1] * thickness);
	   glTexCoord2f(0.5f, 1.f);
	   glVertex2f(x1 - normal[0] * thickness, y1 - normal[1] * thickness);
	   glTexCoord2f(0.f, 1.f);
	   glVertex2f(x1 - unit[0] * thickness - normal[0] * thickness, y1 - unit[1] * thickness - normal[1] * thickness);

	   // The middle part of the line.
	   glTexCoord2f(0.5f, 0.f);
	   glVertex2f(x1 + normal[0] * thickness, y1 + normal[1] * thickness);
	   glVertex2f(x2 + normal[0] * thickness, y2 + normal[1] * thickness);
	   glTexCoord2f(0.5f, 1.f);
	   glVertex2f(x2 - normal[0] * thickness, y2 - normal[1] * thickness);
	   glVertex2f(x1 - normal[0] * thickness, y1 - normal[1] * thickness);

	   // End of the line.
	   glTexCoord2f(0.5f, 0.f);
	   glVertex2f(x2 + normal[0] * thickness, y2 + normal[1] * thickness);
	   glTexCoord2f(1.f, 0.f);
	   glVertex2f(x2 + unit[0] * thickness + normal[0] * thickness, y2 + unit[1] * thickness + normal[1] * thickness);
	   glTexCoord2f(1.f, 1.f);
	   glVertex2f(x2 + unit[0] * thickness - normal[0] * thickness, y2 + unit[1] * thickness - normal[1] * thickness);
	   glTexCoord2f(0.5f, 1.f);
	   glVertex2f(x2 - normal[0] * thickness, y2 - normal[1] * thickness);
   glEnd();

   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

