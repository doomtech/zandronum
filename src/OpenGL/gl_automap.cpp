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
#include "gl_main.h"
#include "r_defs.h"

CVAR(Bool, gl_automap_dukestyle, false, CVAR_ARCHIVE)
CVAR(Float, gl_automap_transparency, 0.85f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);
EXTERN_CVAR (Int, am_rotate);
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

   sec = R_FakeFlat(subSec->sector, &ts, &floorLight, &ceilingLight, false);
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
   // make sure the subsector has been calculated...
   if (!poly->initialized) GL_RecalcSubsector(subSec, sec);
   textureList.BindTexture(subSec->sector->floorpic, true);

   glColor4f(color * byte2float[p->r], color * byte2float[p->g], color * byte2float[p->b], alpha);

   glBegin(GL_TRIANGLE_FAN);
      index = 0;
      for (i = subSec->numlines - 1; i >= 0; i--)
      {
         seg = &segs[subSec->firstline + i];
         x = ((seg->v2->x >> FRACTOMAPBITS) + 0) * automapScale * MAP_SCALE;
         y = ((seg->v2->y >> FRACTOMAPBITS) + 0) * automapScale * MAP_SCALE;
         tx = poly->texCoords[(index * 2) + 0];
         ty = poly->texCoords[(index * 2) + 1];
         glTexCoord2f(tx, ty);
         glVertex2f(x, y);
         index++;
      }
      x = ((seg->v1->x >> FRACTOMAPBITS) + 0) * automapScale * MAP_SCALE;
      y = ((seg->v1->y >> FRACTOMAPBITS) + 0) * automapScale * MAP_SCALE;
      tx = poly->texCoords[(index * 2) + 0];
      ty = poly->texCoords[(index * 2) + 1];
      glTexCoord2f(tx, ty);
      glVertex2f(x, y);
   glEnd();
}

void GL_DrawDukeAutomap()
{
   subsector_t *subSec;
   float yaw, x, y, stOffset;
   int i;

   automapScale = scale_mtof * (1.f / 0.018f * 2.f) * INV_FRACUNIT;
   automapScale *= 2.3f;

   glDisable(GL_DEPTH_TEST);

   if (viewactive)
   {
      glDisable(GL_TEXTURE_2D);
      glColor4f(0.f, 0.f, 0.f, 0.5f);
      glBegin(GL_QUADS);
         glVertex2i(0, screen->GetHeight());
         glVertex2i(0, 0);
         glVertex2i(screen->GetWidth(), 0);
         glVertex2i(screen->GetWidth(), screen->GetHeight());
      glEnd();
      glEnable(GL_TEXTURE_2D);

      glBlendFunc(GL_SRC_ALPHA, GL_ONE);
   }

   if (screenblocks < 11 || !viewactive)
   {
      stOffset = 16.f;
      if (st_scale)
      {
         stOffset = screen->GetHeight() / 200.f * stOffset;
      }
   }
   else
   {
      stOffset = 0;
   }

   //Printf("%d %d\n", m_x >> FRACBITS, m_y >> FRACBITS);
   x = (m_x + m_x2) * automapScale * (INV_FRACUNIT / 2);
   y = (m_y + m_y2) * automapScale * (INV_FRACUNIT / 2);

   glTranslatef((screen->GetWidth() / 2.f), ((screen->GetHeight() / 2.f) + stOffset), 0.f);

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

   GL_Set2DMode();
}


void GL_DrawLine(int x1, int y1, int x2, int y2, int color)
{
   PalEntry *p;

   p = &GPalette.BaseColors[color];

   glDisable(GL_TEXTURE_2D);
   glColor3ub(p->r, p->g, p->b);
   glBegin(GL_LINES);
      glVertex2i(x1, screen->GetHeight() - y1);
      glVertex2i(x2, screen->GetHeight() - y2);
   glEnd();
   glEnable(GL_TEXTURE_2D);
}
