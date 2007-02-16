/*
** gl_walls.cpp
**
** Functions for rendering segs and decals.
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
#include <list>

#define USE_WINDOWS_DWORD
#include "gl_main.h"
#include "gl_texturelist.h"

#include "a_sharedglobal.h"
#include "p_lnspec.h"
#include "r_draw.h"
#include "r_sky.h"
#include "templates.h"
#include "w_wad.h"
#include "gi.h"

#define MAX(a,b) ((a) >= (b) ? (a) : (b))

using std::list;


EXTERN_CVAR (Bool, gl_depthfog)
EXTERN_CVAR (Bool, gl_wireframe)
EXTERN_CVAR (Bool, gl_texture)
EXTERN_CVAR (Bool, r_fogboundary)
EXTERN_CVAR (Bool, gl_blend_animations)
EXTERN_CVAR (Int, ty)
EXTERN_CVAR (Int, tx)
CVAR (Bool, gl_draw_decals, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR (Bool, gl_show_walltype, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR (Bool, gl_mask_walls, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)


extern TextureList textureList;
extern bool DrawingDeferredLines;
extern bool CollectSpecials;
extern bool InSkybox;
extern bool InMirror;
extern bool MaskSkybox;
extern player_t *Player;
extern int playerlight;
extern int numTris;
extern list<seg_t *> mirrorLines;
extern TArray<spritedef_t> sprites;
extern TArray<spriteframe_t> SpriteFrames;

vertex_t *vert1, *vert2;
TArray<decal_data_t> DecalList;


void GL_RenderWallGeometry(float *v1, float *v2, float *v3, float *v4)
{
   glBegin(GL_TRIANGLE_FAN);
      glTexCoord2f(v3[3], v3[4]);
      glVertex3fv(v3);
      glTexCoord2f(v1[3], v1[4]);
      glVertex3fv(v1);
      glTexCoord2f(v2[3], v2[4]);
      glVertex3fv(v2);
      glTexCoord2f(v4[3], v4[4]);
      glVertex3fv(v4);
   glEnd();

   numTris += 2;
}


bool GL_SkyWall(seg_t *seg)
{
   side_t *front;

   front = seg->sidedef;

   if (seg->linedef->special == Line_Horizon)
   {
      return true;
   }

   if (seg->backsector)
   {
      if (front->toptexture == 0 && front->midtexture == 0 && front->bottomtexture == 0)
      {
         if (seg->frontsector->ceilingpic == skyflatnum && seg->backsector->ceilingpic == skyflatnum)
         {
            return true;
         }
      }
   }
   else
   {
      if (front->toptexture == 0 && front->midtexture == 0 && front->bottomtexture == 0)
      {
         if (seg->frontsector->ceilingpic == skyflatnum)
         {
            return true;
         }
      }
   }

   return false;
}

bool GL_ShouldDrawWall(seg_t *seg)
{
   byte special = seg->linedef->special;

   if (GL_SkyWall(seg))
   {
      return true;
   }

   switch (special)
   {
   case Line_Horizon:
      return false;
   default:
      break;
   }

   return true;
}


void GL_AddDecal(ADecal *decal, seg_t *seg)
{
   DecalList.Resize(DecalList.Size() + 1);
   DecalList[DecalList.Size() - 1].decal = decal;
   DecalList[DecalList.Size() - 1].seg = seg;
}


void GL_DrawDecals()
{
   sector_t *frontSector, *backSector, fs, bs;
   seg_t *seg;
   unsigned int i;

   glPolygonOffset(-1.0f, -1.0f);
   glDepthMask(GL_FALSE);

   for (i = 0; i < DecalList.Size(); i++)
   {
      seg = DecalList[i].seg;
      frontSector = R_FakeFlat(seg->frontsector, &fs, NULL, NULL, false);
      if (seg->backsector)
      {
         backSector = R_FakeFlat(seg->backsector, &bs, NULL, NULL, false);
      }
      else
      {
         backSector = NULL;
      }
      GL_DrawDecal(DecalList[i].decal, seg, frontSector, backSector);
   }

   glPolygonOffset(0.f, 0.f);
   glDepthMask(GL_TRUE);

   DecalList.Clear();
}

void GL_DrawDecal(ADecal *actor, seg_t *seg, sector_t *frontSector, sector_t *backSector)
{
   FTexture *tex;
   fixed_t zpos;
   fixed_t ownerAngle;
   PalEntry *p;
   float x, y, z;
   float r, g, b, a, color;
   float cx, cy;
   float angle;
   float width, height;
   float left, right, top, bottom, dist;
   int decalTile;
   bool flipx, loadAlpha;
   texcoord_t t1, t2, t3, t4;

   if (actor->renderflags & RF_INVISIBLE)
   {
      return;
   }

   switch (actor->renderflags & RF_RELMASK)
	{
	default:
		zpos = actor->z;
		break;
	case RF_RELUPPER:
		if (seg->linedef->flags & ML_DONTPEGTOP)
		{
			zpos = actor->z + frontSector->ceilingtexz;
		}
		else
		{
			zpos = actor->z + backSector->ceilingtexz;
		}
		break;
	case RF_RELLOWER:
		if (seg->linedef->flags & ML_DONTPEGBOTTOM)
		{
			zpos = actor->z + frontSector->ceilingtexz;
		}
		else
		{
			zpos = actor->z + backSector->floortexz;
		}
		break;
	case RF_RELMID:
		if (seg->linedef->flags & ML_DONTPEGBOTTOM)
		{
			zpos = actor->z + frontSector->floortexz;
		}
		else
		{
			zpos = actor->z + frontSector->ceilingtexz;
		}
	}

   if (actor->picnum != 0xffff)
	{
		decalTile = actor->picnum;
		flipx = actor->renderflags & RF_XFLIP;
	}
	else
	{
      decalTile = SpriteFrames[sprites[actor->sprite].spriteframes + actor->frame].Texture[0];
		//flipx = SpriteFrames[sprites[actor->sprite].spriteframes + actor->frame].flip & 1;
      flipx = 0;
	}

   y = zpos * MAP_SCALE;
   x = -actor->x * MAP_SCALE;
   z = actor->y * MAP_SCALE;
   ownerAngle = R_PointToAngle2(seg->v1->x, seg->v1->y, seg->v2->x, seg->v2->y);
   //ownerAngle = actor->angle;
   angle = (ownerAngle / (ANGLE_MAX * 1.f) * 360.f);

   if (actor->renderflags & RF_FULLBRIGHT)
   {
      glDisable(GL_FOG);
      color = 1.f;
   }
   else
   {
      color = byte2float[MIN<int>(frontSector->lightlevel + (playerlight << 4), 255)];
   }

   p = &GPalette.BaseColors[frontSector->ColorMap->Maps[APART(actor->alphacolor)]];
   r = byte2float[p->r];
   g = byte2float[p->g];
   b = byte2float[p->b];

   p = &frontSector->ColorMap->Color;
   r = r * byte2float[p->r] * color;
   g = g * byte2float[p->g] * color;
   b = b * byte2float[p->b] * color;

   tex = TexMan(decalTile);
   width = (tex->GetWidth() / 2.f) * (actor->xscale / 63.f) / MAP_COEFF;
   height = tex->GetHeight() * (actor->yscale / 63.f) / MAP_COEFF;

   top = (tex->TopOffset * (actor->yscale / 63.f)) / MAP_COEFF;
   bottom = top - height;
   left = -width;
   right = width;

   a = actor->alpha * INV_FRACUNIT;

   if (actor->RenderStyle == STYLE_Shaded)
   {
      loadAlpha = true;
   }
   else
   {
      loadAlpha = false;
      r = 1.f;
      g = 1.f;
      b = 1.f;
   }

   textureList.LoadAlpha(loadAlpha);
   textureList.AllowScale(false);
   textureList.BindTexture(decalTile, true);
   textureList.AllowScale(true);
   textureList.LoadAlpha(false);
   textureList.GetCorner(&cx, &cy);

   t1.x = 0.f; t1.y = cy;
   t2.x = 0.f; t2.y = 0.f;
   t3.x = cx;  t3.y = 0.f;
   t4.x = cx;  t4.y = cy;

   glPushMatrix();
   glTranslatef(x, y, z);
   glRotatef(angle, 0.f, 1.f, 0.f);
   glColor4f(r, g, b, a);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

#if 0
   vertex_t *testvert;
   testvert = seg->v1;
   left = 0;
   glColor4f(0, 0, 0, 0);
   dist = R_PointToDist3(actor->x, actor->y, testvert->x, testvert->y) * INV_FRACUNIT;
   if (width > dist)
   {
      glColor3f(1, 0, 0);
   }
#endif

   if (flipx)
   {
      glBegin(GL_TRIANGLE_FAN);
         glTexCoord2f(t1.x, t1.y);
         glVertex3f(left, bottom, 0);
         glTexCoord2f(t2.x, t2.y);
         glVertex3f(left, top, 0);
         glTexCoord2f(t3.x, t3.y);
         glVertex3f(right, top, 0);
         glTexCoord2f(t4.x, t4.y);
         glVertex3f(right, bottom, 0);
      glEnd();
   }
   else
   {
      glBegin(GL_TRIANGLE_FAN);
         glTexCoord2f(t4.x, t1.y);
         glVertex3f(left, bottom, 0);
         glTexCoord2f(t3.x, t2.y);
         glVertex3f(left, top, 0);
         glTexCoord2f(t2.x, t3.y);
         glVertex3f(right, top, 0);
         glTexCoord2f(t1.x, t4.y);
         glVertex3f(right, bottom, 0);
      glEnd();
   }

   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
   glPopMatrix();

   numTris += 2;

   if (gl_depthfog && !Player->fixedcolormap)
   {
      glEnable(GL_FOG);
   }
}


void GL_DrawLineHorizon(seg_t *seg, sector_t *sector)
{
   float v1[5], v2[5], v3[5], v4[5];
   int i;

   // calculate seg coordinates
   v1[0] = -seg->v1->x * MAP_SCALE;
   v1[1] = sector->ceilingplane.ZatPoint(seg->v1) * MAP_SCALE;
   v1[2] = seg->v1->y * MAP_SCALE;
   v2[0] = -seg->v2->x * MAP_SCALE;
   v2[1] = sector->ceilingplane.ZatPoint(seg->v2) * MAP_SCALE;
   v2[2] = seg->v2->y * MAP_SCALE;
   v1[3] = v2[3] = 0.f;
   v1[4] = v2[4] = 1.f;
   // extrude seg ends from center of sector to far plane
   memcpy(v3, v1, sizeof(float) * 5);
   memcpy(v4, v2, sizeof(float) * 5);
   // calc texcoords for 4 points

#if 0
   if (sector->ceilingpic)
   {
      textureList.BindTexture(sector->ceilingpic, true);
      glBegin(GL_TRIANGLE_FAN);
         for (i = 0; i < 4; i++)
         {
            glTexCoord2f(v1[3], v1[4]);
            glVertex3f(v1[0], v1[1], v1[2]);
            glTexCoord2f(v2[3], v2[4]);
            glVertex3f(v2[0], v2[1], v2[2]);
            glTexCoord2f(v3[3], v3[4]);
            glVertex3f(v3[0], v3[1], v3[2]);
            glTexCoord2f(v4[3], v4[4]);
            glVertex3f(v4[0], v4[1], v4[2]);
         }
      glEnd();
   }
#endif
}


void GL_GetOffsetsForSeg(seg_t *seg, float *offX, float *offY, wallpart_t part)
{
   float x, y, sx, sy;
   FTexture *tex;

   x = seg->sidedef->textureoffset * INV_FRACUNIT;
   y = seg->sidedef->rowoffset * INV_FRACUNIT;

   switch (part)
   {
   case WT_TOP:
      tex = TexMan(seg->sidedef->toptexture);
      break;
   case WT_MID:
      tex = TexMan(seg->sidedef->midtexture);
      if (seg->backsector && (seg->linedef->flags & ML_TWOSIDED))
      {
         y = 0.f;
      }
      break;
   case WT_BTM:
      tex = TexMan(seg->sidedef->bottomtexture);
      break;
   }

   *offX = x / tex->GetWidth();
   *offY = y / tex->GetHeight();
}


// stretch the wall vertically and draw to depth buffer only
void GL_MaskUpperWall(seg_t *seg, gl_poly_t *poly)
{
   float v1[3], v2[3], v3[3], v4[3];
   int vertSize = 3 * sizeof(float);

   if (DrawingDeferredLines || !gl_mask_walls || seg->bPolySeg)
   {
      // deferred lines don't affect this at all
      return;
   }

   memcpy(v1, poly->vertices, vertSize);
   memcpy(v2, poly->vertices + 3, vertSize);
   memcpy(v3, poly->vertices + 6, vertSize);
   memcpy(v4, poly->vertices + 9, vertSize);

   v2[1] = v1[1];
   v3[1] = v4[1];
   v1[1] = v4[1] = 10000.f;

   glDisable(GL_TEXTURE_2D);
   glColor4f(1.f, 1.f, 1.f, 1.f);
   glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
   glBegin(GL_TRIANGLE_FAN);
      glVertex3fv(v1);
      glVertex3fv(v2);
      glVertex3fv(v3);
      glVertex3fv(v4);
   glEnd();
   if (gl_texture && !gl_wireframe) glEnable(GL_TEXTURE_2D);
   glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
}


void GL_MaskLowerWall(seg_t *seg, gl_poly_t *poly)
{
   float v1[3], v2[3], v3[3], v4[3];
   int vertSize = 3 * sizeof(float);

   if (DrawingDeferredLines || !gl_mask_walls || seg->bPolySeg)
   {
      // deferred lines don't affect this at all
      return;
   }

   memcpy(v1, poly->vertices, vertSize);
   memcpy(v2, poly->vertices + 3, vertSize);
   memcpy(v3, poly->vertices + 6, vertSize);
   memcpy(v4, poly->vertices + 9, vertSize);

   v1[1] = v2[1];
   v4[1] = v3[1];
   v2[1] = v3[1] = -10000.f;

   glDisable(GL_TEXTURE_2D);
   glColor4f(1.f, 1.f, 1.f, 1.f);
   glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
   glBegin(GL_TRIANGLE_FAN);
      glVertex3fv(v1);
      glVertex3fv(v2);
      glVertex3fv(v3);
      glVertex3fv(v4);
   glEnd();
   if (gl_texture && !gl_wireframe) glEnable(GL_TEXTURE_2D);
   glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
}


void GL_DrawWall(seg_t *seg, sector_t *frontSector, bool isPoly, bool fogBoundary)
{
   int index;
   bool deferLine = false, validTexture;
   bool isMirror;
   float offX, offY, r, g, b;
   PalEntry p;
   gl_poly_t *poly;
   FTexture *tex;
   sector_t *backSector, ts;
   int lightLevel;

   if (seg->linedef == NULL || seg->sidedef == NULL)
   {
      return;
   }

   backSector = R_FakeFlat(seg->backsector, &ts, NULL, NULL, false);
   if (backSector)
   {
      fogBoundary = IsFogBoundary(frontSector, backSector);
      fogBoundary = false;
   }
   else
   {
      fogBoundary = false;
   }

   index = (numsubsectors * 2);
   index += seg->index * 3;

   if (!GL_ShouldDrawWall(seg))
   {
      return;
   }

   if (DrawingDeferredLines && MaskSkybox)
   {
      return;
   }

   if (seg->linedef->alpha < 255 && seg->backsector)
   {
      deferLine = true;
   }

   if (fogBoundary && !DrawingDeferredLines)
   {
      deferLine = true;
   }

   if (seg->sidedef->midtexture)
   {
      textureList.GetTexture(seg->sidedef->midtexture, true);
      if (seg->backsector != NULL && textureList.IsTransparent())
      {
         deferLine = true;
      }
   }

   if (deferLine && !DrawingDeferredLines && !CollectSpecials && !MaskSkybox)
   {
      GL_AddSeg(seg);
      fogBoundary = false;
   }

   p = frontSector->ColorMap->Color;
   if (seg->backsector)
   {
      p.a = seg->linedef->alpha;
   }
   else
   {
      // solid geometry is never translucent!
      p.a = 0xff;
   }

   if (Player->fixedcolormap)
   {
      glDisable(GL_FOG);
   }
   else
   {
      if (frontSector->ColorMap->Fade)
      {
         GL_SetupFog(frontSector->lightlevel, frontSector->ColorMap->Fade.r, frontSector->ColorMap->Fade.g, frontSector->ColorMap->Fade.b);
      }
      else
      {
         GL_SetupFog(frontSector->lightlevel, ((level.fadeto & 0xFF0000) >> 16), ((level.fadeto & 0x00FF00) >> 8), level.fadeto & 0x0000FF);
      }
   }

   foggy = level.fadeto || frontSector->ColorMap->Fade;
   GL_GetSectorColor(frontSector, &r, &g, &b, seg->sidedef->GetLightLevel(foggy, frontSector->lightlevel));
   lightLevel = clamp<int>(seg->sidedef->GetLightLevel(foggy, frontSector->lightlevel), 0, 255);

   isMirror = (seg->linedef->special == Line_Mirror && seg->backsector == NULL);

   if (MaskSkybox)
   {
      float v1[3], v2[3], v3[3], v4[3];

      if (seg->frontsector->CeilingSkyBox && GL_SkyWall(seg))
      {
         glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
         glStencilFunc(GL_ALWAYS, seg->frontsector->CeilingSkyBox->refmask, ~0);
         glColor3f(1.f, 1.f, 1.f);
         v1[1] = frontSector->ceilingplane.ZatPoint(seg->v1) * MAP_SCALE;
         v2[1] = frontSector->ceilingplane.ZatPoint(seg->v2) * MAP_SCALE;
         v3[1] = frontSector->floorplane.ZatPoint(seg->v1) * MAP_SCALE;
         v4[1] = frontSector->floorplane.ZatPoint(seg->v2) * MAP_SCALE;
         v1[0] = v3[0] = -seg->v1->x * MAP_SCALE;
         v1[2] = v3[2] = seg->v1->y * MAP_SCALE;
         v2[0] = v4[0] = -seg->v2->x * MAP_SCALE;
         v2[2] = v4[2] = seg->v2->y * MAP_SCALE;
         textureList.BindGLTexture(0);
         glBegin(GL_TRIANGLE_FAN);
            glVertex3fv(v1);
            glVertex3fv(v3);
            glVertex3fv(v4);
            glVertex3fv(v2);
         glEnd();
         glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
      }
   }
   else if (!CollectSpecials && !GL_SkyWall(seg))
   {
      if (p.r == 0xff && p.g == 0xff && p.b == 0xff)
      {
         textureList.SetTranslation(seg->frontsector->ColorMap->Maps);
      }

      if (seg->sidedef->toptexture == 0)
      {
         validTexture = false;
      }
      else
      {
         tex = TexMan(seg->sidedef->toptexture);
         validTexture = stricmp(tex->Name, "") != 0;
      }

      if (seg->backsector && validTexture && !DrawingDeferredLines && !isMirror)
      {
         if (seg->backsector->ceilingpic != skyflatnum || seg->frontsector->ceilingpic != skyflatnum)
         {
            GL_GetOffsetsForSeg(seg, &offX, &offY, WT_TOP);

            poly = &gl_polys[index + 0];
            textureList.GetTexture(seg->sidedef->toptexture, true);

            // set up the potentially continuously changing poly stuff here
            poly->isFogBoundary = false;
            poly->offX = offX;
            poly->offY = offY;
            poly->r = r;
            poly->g = g;
            poly->b = b;
            poly->a = 1.f;
            poly->scaleX = 1.f;
            poly->scaleY = 1.f;
            poly->rot = 0.f;
            poly->lightLevel = lightLevel;
            poly->doomTex = seg->sidedef->toptexture;
            if (p.r == 0xff && p.g == 0xff && p.b == 0xff)
            {
               poly->translation = seg->frontsector->ColorMap->Maps;
            }
            else
            {
               poly->translation = NULL;
            }

            if (frontSector->ColorMap->Fade)
            {
               poly->fogR = frontSector->ColorMap->Fade.r;
               poly->fogG = frontSector->ColorMap->Fade.g;
               poly->fogB = frontSector->ColorMap->Fade.b;
            }
            else
            {
               poly->fogR = (level.fadeto & 0xFF0000) >> 16;
               poly->fogG = (level.fadeto & 0x00FF00) >> 8;
               poly->fogB = level.fadeto & 0x0000FF;
            }

            RL_AddPoly(textureList.GetGLTexture(), index + 0);

            if (frontSector->ceilingpic == skyflatnum) GL_MaskUpperWall(seg, poly);
         }
      }

      if (seg->sidedef->midtexture == 0)
      {
         validTexture = false;
      }
      else
      {
         tex = TexMan(seg->sidedef->midtexture);
         validTexture = tex->UseType != FTexture::TEX_Null;
      }
      if ((validTexture && (deferLine == DrawingDeferredLines) && !(seg->linedef->special == Line_Horizon)) || isMirror || fogBoundary)
      {
         GL_GetOffsetsForSeg(seg, &offX, &offY, WT_MID);

         poly = &gl_polys[index + 1];
         if (isMirror)
         {
            p.a = 255 / 10; // 10% opacity
            textureList.BindTexture(TexMan["ENVTEX"]);
            glEnable(GL_TEXTURE_GEN_S);
            glEnable(GL_TEXTURE_GEN_T);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
         }
         else
         {
            textureList.GetTexture(seg->sidedef->midtexture, true);
         }

         // set up the potentially continuously changing poly stuff here
         if (fogBoundary)
         {
            poly->isFogBoundary = fogBoundary;
            if (backSector->ColorMap->Fade)
            {
               poly->fbR = backSector->ColorMap->Fade.r;
               poly->fbG = backSector->ColorMap->Fade.g;
               poly->fbB = backSector->ColorMap->Fade.b;
               poly->fbLightLevel = backSector->lightlevel;
            }
            else
            {
               poly->fbR = frontSector->ColorMap->Fade.r;
               poly->fbG = frontSector->ColorMap->Fade.g;
               poly->fbB = frontSector->ColorMap->Fade.b;
               poly->fbLightLevel = frontSector->lightlevel;
            }
         }
         else
         {
            poly->isFogBoundary = false;
         }
         poly->offX = offX;
         poly->offY = offY;
         poly->r = r;
         poly->g = g;
         poly->b = b;
         poly->a = byte2float[p.a];
         poly->scaleX = 1.f;
         poly->scaleY = 1.f;
         poly->rot = 0.f;
         poly->lightLevel = lightLevel;
         poly->doomTex = seg->sidedef->midtexture;
         if (p.r == 0xff && p.g == 0xff && p.b == 0xff)
         {
            poly->translation = seg->frontsector->ColorMap->Maps;
         }
         else
         {
            poly->translation = NULL;
         }

         if (frontSector->ColorMap->Fade)
         {
            poly->fogR = frontSector->ColorMap->Fade.r;
            poly->fogG = frontSector->ColorMap->Fade.g;
            poly->fogB = frontSector->ColorMap->Fade.b;
         }
         else
         {
            poly->fogR = (level.fadeto & 0xFF0000) >> 16;
            poly->fogG = (level.fadeto & 0x00FF00) >> 8;
            poly->fogB = level.fadeto & 0x0000FF;
         }

         if (p.a < 0xff)
         {
            glDepthMask(GL_FALSE);
         }

         if (DrawingDeferredLines || isMirror)
         {
            FShader *shader = NULL;

            if (!isMirror)
            {
               textureList.BindTexture(seg->sidedef->midtexture, true);
               shader = GL_ShaderForTexture(TexMan[seg->sidedef->midtexture]);
            }

            if (shader)
            {
               GL_RenderPolyWithShader(poly, shader);
            }
            else
            {
               GL_RenderPoly(poly);
            }
         }
         else
         {
            RL_AddPoly(textureList.GetGLTexture(), index + 1);
            if (!seg->backsector)
            {
               if (frontSector->ceilingpic == skyflatnum && !seg->sidedef->toptexture) GL_MaskUpperWall(seg, poly);
               if (frontSector->floorpic == skyflatnum && !seg->sidedef->bottomtexture) GL_MaskLowerWall(seg, poly);
            }
         }

         if (p.a < 0xff)
         {
            glDepthMask(GL_TRUE);
         }

         if (isMirror)
         {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDisable(GL_TEXTURE_GEN_S);
            glDisable(GL_TEXTURE_GEN_T);
         }
      }

      if (seg->sidedef->bottomtexture == 0)
      {
         validTexture = false;
      }
      else
      {
         tex = TexMan(seg->sidedef->bottomtexture);
         validTexture = stricmp(tex->Name, "") != 0;
      }
      if (seg->backsector && validTexture && !DrawingDeferredLines && !isMirror)
      {
         if (seg->backsector->floorpic != skyflatnum || seg->frontsector->floorpic != skyflatnum)
         {
            GL_GetOffsetsForSeg(seg, &offX, &offY, WT_BTM);

            poly = &gl_polys[index + 2];
            textureList.GetTexture(seg->sidedef->bottomtexture, true);

            // set up the potentially continuously changing poly stuff here
            poly->isFogBoundary = false;
            poly->offX = offX;
            poly->offY = offY;
            poly->r = r;
            poly->g = g;
            poly->b = b;
            poly->a = 1.f;
            poly->scaleX = 1.f;
            poly->scaleY = 1.f;
            poly->rot = 0.f;
            poly->lightLevel = lightLevel;
            poly->doomTex = seg->sidedef->bottomtexture;
            if (p.r == 0xff && p.g == 0xff && p.b == 0xff)
            {
               poly->translation = seg->frontsector->ColorMap->Maps;
            }
            else
            {
               poly->translation = NULL;
            }

            if (frontSector->ColorMap->Fade)
            {
               poly->fogR = frontSector->ColorMap->Fade.r;
               poly->fogG = frontSector->ColorMap->Fade.g;
               poly->fogB = frontSector->ColorMap->Fade.b;
            }
            else
            {
               poly->fogR = (level.fadeto & 0xFF0000) >> 16;
               poly->fogG = (level.fadeto & 0x00FF00) >> 8;
               poly->fogB = level.fadeto & 0x0000FF;
            }

            RL_AddPoly(textureList.GetGLTexture(), index + 2);

            if (frontSector->floorpic == skyflatnum) GL_MaskLowerWall(seg, poly);
         }
      }

      textureList.SetTranslation((byte *)NULL);
      if (gl_draw_decals && seg->linedef->special != Line_Mirror && !CollectSpecials && DrawingDeferredLines == deferLine)
      {
/*
         ADecal *decal = seg->sidedef->BoundActors;
         while (decal)
         {
            GL_AddDecal(decal, seg);
            decal = (ADecal *)decal->snext;
         }
*/
      }
   }
}
