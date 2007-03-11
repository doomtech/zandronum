/*
** gl_geom.cpp
**
** Functions for updating the vertex arrays.
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

#define USE_WINDOWS_DWORD
#include "OpenGLVideo.h"
#include "Glext.h"

#include "gl_main.h"
#include "d_player.h"
#include "p_local.h"
#include "p_lnspec.h"
#include "templates.h"

#include "gi.h"

extern PFNGLBLENDEQUATIONEXTPROC glBlendEquationEXT;

#define MAX(a, b) ((a > b) ? a : b)
#define MIN(a, b) ((a < b) ? a : b)


EXTERN_CVAR(Bool, gl_blend_animations)
EXTERN_CVAR(Bool, gl_texture)
EXTERN_CVAR(Bool, gl_wireframe)
EXTERN_CVAR(Bool, gl_depthfog)
EXTERN_CVAR (Bool, r_fogboundary)
CVAR(Bool, gl_texture_multitexture, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)


extern level_locals_t level;
extern float *verts, *texCoords;
extern int numTris;
extern bool MaskSkybox, DrawingDeferredLines;
extern player_t *Player;


TArray<RList> renderList;
unsigned int maxList;
unsigned int frameStartMS;


gl_poly_t::gl_poly_t()
{
   rotationX = 0.f;
   rotationY = 0.f;
}

gl_poly_t::~gl_poly_t()
{
}


void GL_BindArrays(gl_poly_t *poly)
{
      glVertexPointer(3, GL_FLOAT, 0, poly->vertices);
         glTexCoordPointer(2, GL_FLOAT, 0, poly->texCoords);
}


void GL_UnbindArrays(gl_poly_t *poly)
{
}


void GL_InitPolygon(gl_poly_t *poly)
{
   if (poly->vertices == NULL)
   {
      poly->vertices = new float[poly->numPts * 3];
      poly->texCoords = new float[poly->numPts * 2];
   }
   poly->initialized = true;
}


bool GL_ShouldRecalcPoly(gl_poly_t *poly, sector_t *sector)
{
   if (!poly->initialized) return true;

   if (sector->heightsec && !(sector->heightsec->MoreFlags & SECF_IGNOREHEIGHTSEC))
   {
      return true;
   }

   if (poly->lastUpdate < frameStartMS)
   {
      if (sectorMoving[sector - sectors] && poly->lastUpdate < frameStartMS)
      {
         return true;
      }

      if ((sector->heightsec) && (sectorMoving[sector->heightsec - sectors]))
      {
         return true;
      }
   }

   return false;
}


bool GL_ShouldRecalcSeg(gl_poly_t *poly, seg_t *seg)
{
   if (GL_ShouldRecalcPoly(poly, seg->frontsector) || (seg->backsector && GL_ShouldRecalcPoly(poly, seg->backsector)))
   {
      return true;
   }

   return false;
}


bool GL_SegTextureChanged(seg_t *seg)
{
   gl_poly_t *poly;
   int index;
   unsigned int textureChanged;

   textureChanged = seg->linedef->textureChanged;

   index = numsubsectors * 2;
   index += (seg - segs) * 3;

   poly = gl_polys + index;
   if (textureChanged > poly->lastUpdate) return true;
   poly++;
   if (textureChanged > poly->lastUpdate) return true;
   poly++;
   if (textureChanged > poly->lastUpdate) return true;

   return false;
}


bool GL_SegGeometryChanged(seg_t *seg)
{
   if (sectorMoving[seg->frontsector - sectors]) return true;
   if (seg->backsector && sectorMoving[seg->backsector - sectors]) return true;

   return false;
}


void GL_RecalcUpperWall(seg_t *seg, sector_t *frontSector, gl_poly_t *poly)
{
   FTexture *tex = TexMan(seg->sidedef->toptexture);
   float txScale, tyScale, yOffset, xOffset, dist;
   float v1[5], v2[5], v3[5], v4[5], texHeight;
   float upperHeight, lowerHeight;
   texcoord_t ll, ul, lr, ur;
   sector_t *backSector, ts2;
   vertex_t *vert1, *vert2;

   if (!poly->initialized) GL_InitPolygon(poly);
   poly->lastUpdate = frameStartMS;
   poly->initialized = true;

   vert1 = seg->v1;
   vert2 = seg->v2;
   dist = seg->length;

   backSector = R_FakeFlat(seg->backsector, &ts2, NULL, NULL, false);

   // texscale: 8 = 1.0, 16 = 2.0, 4 = 0.5
   txScale = tex->ScaleX ? tex->ScaleX / 8.f : 1.f;
   tyScale = tex->ScaleY ? tex->ScaleY / 8.f : 1.f;

   xOffset = seg->offset * txScale * INV_FRACUNIT;

   v1[0] = v3[0] = -vert1->x * MAP_SCALE;
   v1[2] = v3[2] = vert1->y * MAP_SCALE;
   v2[0] = v4[0] = -vert2->x * MAP_SCALE;
   v2[2] = v4[2] = vert2->y * MAP_SCALE;

   v1[1] = (float)backSector->ceilingplane.ZatPoint(vert1);
   v2[1] = (float)backSector->ceilingplane.ZatPoint(vert2);
   v3[1] = (float)frontSector->ceilingplane.ZatPoint(vert1);
   v4[1] = (float)frontSector->ceilingplane.ZatPoint(vert2);

   ll.x = xOffset / (tex->GetWidth() * 1.f);
   ul.x = ll.x;
   lr.x = ll.x + (dist / (tex->GetWidth() * 1.f) * txScale);
   ur.x = lr.x;

   ul.y = ur.y = 1.f;
   ll.y = lr.y = 0.f;

   if (tex)
   {
      texHeight = tex->GetHeight() / tyScale;

      lowerHeight = backSector->ceilingtexz * INV_FRACUNIT;
      upperHeight = frontSector->ceilingtexz * INV_FRACUNIT;

      if (seg->linedef->flags & ML_DONTPEGTOP)
      {
         yOffset = 0.f;
      }
      else
      {
         yOffset = lowerHeight + texHeight - upperHeight;
      }

      ul.y = ur.y = (yOffset + upperHeight - lowerHeight) / texHeight;
      ll.y = lr.y = yOffset / texHeight;

      // adjust for slopes
      ul.y -= ((v1[1] * INV_FRACUNIT) - lowerHeight) / texHeight;
      ur.y -= ((v2[1] * INV_FRACUNIT) - lowerHeight) / texHeight;
      ll.y += (upperHeight - (v3[1] * INV_FRACUNIT)) / texHeight;
      lr.y += (upperHeight - (v4[1] * INV_FRACUNIT)) / texHeight;
   }

   v1[1] *= MAP_SCALE;
   v2[1] *= MAP_SCALE;
   v3[1] *= MAP_SCALE;
   v4[1] *= MAP_SCALE;

   v1[3] = ul.x; v1[4] = ul.y;
   v2[3] = ur.x; v2[4] = ur.y;
   v3[3] = ll.x; v3[4] = ll.y;
   v4[3] = lr.x; v4[4] = lr.y;

   memcpy(poly->vertices + (0 * 3), v3, sizeof(float) * 3);
   memcpy(poly->vertices + (1 * 3), v1, sizeof(float) * 3);
   memcpy(poly->vertices + (2 * 3), v2, sizeof(float) * 3);
   memcpy(poly->vertices + (3 * 3), v4, sizeof(float) * 3);

   memcpy(poly->texCoords + (0 * 2), v3 + 3, sizeof(float) * 2);
   memcpy(poly->texCoords + (1 * 2), v1 + 3, sizeof(float) * 2);
   memcpy(poly->texCoords + (2 * 2), v2 + 3, sizeof(float) * 2);
   memcpy(poly->texCoords + (3 * 2), v4 + 3, sizeof(float) * 2);
}


void GL_RecalcMidWall(seg_t *seg, sector_t *frontSector, gl_poly_t *poly)
{
   float height1, height2, v1[5], v2[5], v3[5], v4[5], txScale, tyScale;
   float yOffset1, yOffset2, yOffset, xOffset;
   float upperHeight, lowerHeight;
   float tmpOffset = seg->sidedef->rowoffset * INV_FRACUNIT;
   long offset = 0;
   int texHeight;
   FTexture *tex = NULL;
   texcoord_t ll, ul, lr, ur;
   sector_t *backSector, ts2;
   vertex_t *vert1, *vert2;

   if (!poly->initialized) GL_InitPolygon(poly);
   poly->lastUpdate = frameStartMS;
   poly->initialized = true;

   tex = TexMan(seg->sidedef->midtexture);

   vert1 = seg->v1;
   vert2 = seg->v2;

   backSector = R_FakeFlat(seg->backsector, &ts2, NULL, NULL, false);

   v1[0] = -vert1->x * MAP_SCALE;
   v1[2] = vert1->y * MAP_SCALE;
   v2[0] = -vert2->x * MAP_SCALE;
   v2[2] = vert2->y * MAP_SCALE;
   v1[1] = (float)frontSector->floorplane.ZatPoint(seg->v1);
   v2[1] = (float)frontSector->floorplane.ZatPoint(seg->v2);

   if ((seg->linedef->special == Line_Mirror && seg->backsector == NULL) || tex == NULL)
   {
      if (backSector)
      {
         height1 = (float)backSector->ceilingplane.ZatPoint(vert1);
         height2 = (float)backSector->ceilingplane.ZatPoint(vert2);
      }
      else
      {
         height1 = (float)frontSector->ceilingplane.ZatPoint(vert1);
         height2 = (float)frontSector->ceilingplane.ZatPoint(vert2);
      }
   }
   else
   {
      //yOffset = seg->sidedef->rowoffset * INV_FRACUNIT;
      yOffset = 0.f;
      tmpOffset = 0.f;
      offset = seg->sidedef->rowoffset;
      yOffset1 = yOffset2 = yOffset;

      // texscale: 8 = 1.0, 16 = 2.0, 4 = 0.5
      txScale = tex->ScaleX ? tex->ScaleX / 8.f : 1.f;
      tyScale = tex->ScaleY ? tex->ScaleY / 8.f : 1.f;

      xOffset = (seg->offset * txScale) * INV_FRACUNIT;
   
      texHeight = (int)(tex->GetHeight() / tyScale);
      textureList.GetTexture(seg->sidedef->midtexture, true);

      ll.x = xOffset / tex->GetWidth();
      ul.x = ll.x;
      lr.x = ll.x + (seg->length / tex->GetWidth() * txScale);
      ur.x = lr.x;

      if (backSector)
      {
         height1 = (float)MIN(frontSector->ceilingplane.ZatPoint(vert1), backSector->ceilingplane.ZatPoint(vert1));
         height2 = (float)MIN(frontSector->ceilingplane.ZatPoint(vert2), backSector->ceilingplane.ZatPoint(vert2));
      }
      else
      {
         height1 = (float)frontSector->ceilingplane.ZatPoint(vert1);
         height2 = (float)frontSector->ceilingplane.ZatPoint(vert2);
      }

      if (backSector && (textureList.IsTransparent() || seg->linedef->flags & ML_TWOSIDED))
      {
         int h = texHeight;

#if 0
         if (seg->linedef->flags & ML_DONTPEGBOTTOM)
         {
            if (backSector)
            {
               v1[1] = MAX (frontSector->floortexz, backSector->floortexz) + offset;
               v2[1] = MAX (frontSector->floortexz, backSector->floortexz) + offset;
            }
            else
            {
               v1[1] = frontSector->floortexz + offset;
               v2[1] = frontSector->floortexz + offset;
            }
         }
         else
         {
            if (backSector)
            {
               v1[1] = MIN (frontSector->ceilingtexz, backSector->ceilingtexz) + offset - (h << FRACBITS);
               v2[1] = MIN (frontSector->ceilingtexz, backSector->ceilingtexz) + offset - (h << FRACBITS);
            }
            else
            {
               v1[1] = frontSector->ceilingtexz + offset - (h << FRACBITS);
               v2[1] = frontSector->ceilingtexz + offset - (h << FRACBITS);
            }
         }

         height1 = (h << FRACBITS) + v1[1];
         height2 = (h << FRACBITS) + v2[1];
#else
         float rbceil = (float)backSector->ceilingtexz,
				rbfloor = (float)backSector->floortexz,
				rfceil = (float)frontSector->ceilingtexz,
				rffloor = (float)frontSector->floortexz;
			float gaptop, gapbottom;

         yOffset = (float)seg->sidedef->rowoffset;
         height1 = gaptop = MIN(rbceil, rfceil);
         v1[1] = gapbottom = MAX(rbfloor, rffloor);

         if (seg->linedef->flags & ML_DONTPEGBOTTOM)
         {
            v1[1] += yOffset;
            height1 = v1[1] + (tex->GetHeight() * FRACUNIT);
         }
         else
         {
            height1 += yOffset;
            v1[1] = height1 - (tex->GetHeight() * FRACUNIT);
         }

         height2 = height1;
         v2[1] = v1[1];
#endif
      }
   }

   // make sure to clip the geometry to the back sector...
   if (backSector)
   {
      if (seg->sidedef->bottomtexture)
      {
         v1[1] = MAX(v1[1], backSector->floorplane.ZatPoint(seg->v1));
         v2[1] = MAX(v2[1], backSector->floorplane.ZatPoint(seg->v2));
      }
      if (seg->sidedef->toptexture)
      {
         height1 = MIN(height1, backSector->ceilingplane.ZatPoint(seg->v1));
         height2 = MIN(height2, backSector->ceilingplane.ZatPoint(seg->v2));
      }
   }

   memcpy(v3, v1, sizeof(float) * 3);
   memcpy(v4, v2, sizeof(float) * 3);

   ul.y = ur.y = 1.f;
   ll.y = lr.y = 0.f;

   if (tex)
   {
      yOffset = 0.f;

      // code borrowed from Legacy ;)
      // damn pegged/unpegged/two sided lines!
      if (backSector)
      {
         float worldHigh = backSector->ceilingtexz * INV_FRACUNIT;
         float worldLow = backSector->floortexz * INV_FRACUNIT;
         float polyBottom, polyTop;

         lowerHeight = frontSector->floortexz * INV_FRACUNIT;
         upperHeight = frontSector->ceilingtexz * INV_FRACUNIT;

         float openTop = upperHeight < worldHigh ? upperHeight : worldHigh;
         float openBottom = lowerHeight > worldLow ? lowerHeight : worldLow;

         if (seg->linedef->flags & ML_DONTPEGBOTTOM)
         {
            polyBottom = openBottom + seg->sidedef->rowoffset * INV_FRACUNIT;
            polyTop = polyBottom + texHeight;

            upperHeight = MIN(openTop, polyTop);
            lowerHeight = MAX(openBottom, polyBottom);

            yOffset = lowerHeight + texHeight - upperHeight + polyBottom - lowerHeight;
         }
         else
         {
            polyTop = openTop + seg->sidedef->rowoffset * INV_FRACUNIT;
            polyBottom = polyTop - texHeight;

            yOffset = polyTop - upperHeight;
         }
      }
      else
      {
         lowerHeight = frontSector->floortexz * INV_FRACUNIT;
         upperHeight = frontSector->ceilingtexz * INV_FRACUNIT;
         yOffset = 0.f;

         if (seg->linedef->flags & ML_DONTPEGBOTTOM)
         {
            yOffset += lowerHeight + texHeight - upperHeight;
         }
      }

      ll.y = lr.y = yOffset / texHeight;
      ul.y = ur.y = ll.y + ((upperHeight - lowerHeight) / texHeight);

      // scale the coordinates
      //ul.y *= tyScale;
      //ur.y *= tyScale;
      //ll.y *= tyScale;
      //lr.y *= tyScale;

      // clip to slopes
      ul.y -= ((v1[1] * INV_FRACUNIT) - lowerHeight) / texHeight;
      ur.y -= ((v2[1] * INV_FRACUNIT) - lowerHeight) / texHeight;
      ll.y += (upperHeight - (height1 * INV_FRACUNIT)) / texHeight;
      lr.y += (upperHeight - (height2 * INV_FRACUNIT)) / texHeight;
   }

   v1[1] *= MAP_SCALE;
   v2[1] *= MAP_SCALE;
   v3[1] = height1 * MAP_SCALE;
   v4[1] = height2 * MAP_SCALE;

   v1[3] = ul.x; v1[4] = ul.y;
   v2[3] = ur.x; v2[4] = ur.y;
   v3[3] = ll.x; v3[4] = ll.y;
   v4[3] = lr.x; v4[4] = lr.y;

   memcpy(poly->vertices + (0 * 3), v3, sizeof(float) * 3);
   memcpy(poly->vertices + (1 * 3), v1, sizeof(float) * 3);
   memcpy(poly->vertices + (2 * 3), v2, sizeof(float) * 3);
   memcpy(poly->vertices + (3 * 3), v4, sizeof(float) * 3);

   memcpy(poly->texCoords + (0 * 2), v3 + 3, sizeof(float) * 2);
   memcpy(poly->texCoords + (1 * 2), v1 + 3, sizeof(float) * 2);
   memcpy(poly->texCoords + (2 * 2), v2 + 3, sizeof(float) * 2);
   memcpy(poly->texCoords + (3 * 2), v4 + 3, sizeof(float) * 2);
}


void GL_RecalcLowerWall(seg_t *seg, sector_t *frontSector, gl_poly_t *poly)
{
   FTexture *tex = TexMan(seg->sidedef->bottomtexture);
   float txScale, tyScale, yOffset, xOffset, dist, texHeight;
   float v1[5], v2[5], v3[5], v4[5];
   float upperHeight, lowerHeight;
   texcoord_t ll, ul, lr, ur;
   sector_t *backSector, ts2;
   vertex_t *vert1, *vert2;
   fixed_t h1, h2;

   if (!poly->initialized) GL_InitPolygon(poly);
   poly->lastUpdate = frameStartMS;
   poly->initialized = true;

   vert1 = seg->v1;
   vert2 = seg->v2;
   dist = seg->length;

   backSector = R_FakeFlat(seg->backsector, &ts2, NULL, NULL, false);

   h1 = (backSector->floorplane.ZatPoint(vert1) - frontSector->floorplane.ZatPoint(vert1));
   h2 = (backSector->floorplane.ZatPoint(vert2) - frontSector->floorplane.ZatPoint(vert2));

   v1[0] = v3[0] = -vert1->x * MAP_SCALE;
   v1[2] = v3[2] = vert1->y * MAP_SCALE;
   v2[0] = v4[0] = -vert2->x * MAP_SCALE;
   v2[2] = v4[2] = vert2->y * MAP_SCALE;

   v1[1] = (float)frontSector->floorplane.ZatPoint(vert1);
   v2[1] = (float)frontSector->floorplane.ZatPoint(vert2);
   v3[1] = v1[1] + h1;
   v4[1] = v2[1] + h2;

   // texscale: 8 = 1.0, 16 = 2.0, 4 = 0.5
   txScale = tex->ScaleX ? tex->ScaleX / 8.f : 1.f;
   tyScale = tex->ScaleY ? tex->ScaleY / 8.f : 1.f;

   xOffset = (seg->offset * txScale) * INV_FRACUNIT;

   ll.x = xOffset / tex->GetWidth();
   ul.x = ll.x;
   lr.x = ll.x + (dist / tex->GetWidth() * txScale);
   ur.x = lr.x;

   ul.y = ur.y = 1.f;
   ll.y = lr.y = 0.f;

   if (tex)
   {
      texHeight = tex->GetHeight() / tyScale;

      lowerHeight = frontSector->floortexz * INV_FRACUNIT;
      upperHeight = backSector->floortexz * INV_FRACUNIT;

      if (seg->linedef->flags & ML_DONTPEGBOTTOM)
      {
         yOffset = (frontSector->ceilingtexz - backSector->floortexz) * INV_FRACUNIT;
      }
      else
      {
         yOffset = 0.f;
      }

      ul.y = ur.y = (yOffset + upperHeight - lowerHeight) / texHeight;
      ll.y = lr.y = yOffset / texHeight;

      // scale the coordinates
      //ul.y *= tyScale;
      //ur.y *= tyScale;
      //ll.y *= tyScale;
      //lr.y *= tyScale;

      // adjust for slopes
      ul.y -= ((v1[1] * INV_FRACUNIT) - lowerHeight) / texHeight;
      ur.y -= ((v2[1] * INV_FRACUNIT) - lowerHeight) / texHeight;
      ll.y += (upperHeight - (v3[1] * INV_FRACUNIT)) / texHeight;
      lr.y += (upperHeight - (v4[1] * INV_FRACUNIT)) / texHeight;
   }

   v1[1] *= MAP_SCALE;
   v2[1] *= MAP_SCALE;
   v3[1] *= MAP_SCALE;
   v4[1] *= MAP_SCALE;

   v1[3] = ul.x; v1[4] = ul.y;
   v2[3] = ur.x; v2[4] = ur.y;
   v3[3] = ll.x; v3[4] = ll.y;
   v4[3] = lr.x; v4[4] = lr.y;

   memcpy(poly->vertices + (0 * 3), v3, sizeof(float) * 3);
   memcpy(poly->vertices + (1 * 3), v1, sizeof(float) * 3);
   memcpy(poly->vertices + (2 * 3), v2, sizeof(float) * 3);
   memcpy(poly->vertices + (3 * 3), v4, sizeof(float) * 3);

   memcpy(poly->texCoords + (0 * 2), v3 + 3, sizeof(float) * 2);
   memcpy(poly->texCoords + (1 * 2), v1 + 3, sizeof(float) * 2);
   memcpy(poly->texCoords + (2 * 2), v2 + 3, sizeof(float) * 2);
   memcpy(poly->texCoords + (3 * 2), v4 + 3, sizeof(float) * 2);
}


void GL_RecalcSeg(seg_t *seg, sector_t *controlSector)
{
   int index;
   bool fogBoundary = false;
   sector_t *backSector, ts;

   index = numsubsectors * 2;
   index += (seg - segs) * 3;

   backSector = R_FakeFlat(seg->backsector, &ts, NULL, NULL, false);
   if (backSector)
   {
      fogBoundary = IsFogBoundary(controlSector, backSector);
   }

   if (seg->linedef && seg->sidedef)
   {
      if (seg->backsector && seg->sidedef->toptexture)
      {
         GL_RecalcUpperWall(seg, controlSector, gl_polys + index);
      }

      if (seg->sidedef->midtexture || fogBoundary)
      {
         GL_RecalcMidWall(seg, controlSector, gl_polys + (index + 1));
      }

      if (seg->backsector && seg->sidedef->bottomtexture)
      {
         GL_RecalcLowerWall(seg, controlSector, gl_polys + (index + 2));
      }
   }
}


void GL_RecalcSubsector(subsector_t *subSec, sector_t *sector)
{
   seg_t *seg;
   int polyIndex = subSec->index * 2;
   unsigned long i, vindex, tindex, firstLine, maxLine;
   gl_poly_t *poly1, *poly2;
   float sox, soy, fsx, fsy, csx, csy;
   FTexture *ctex, *ftex;
   bool recalc;

   firstLine = subSec->firstline;
   maxLine = firstLine + subSec->numlines;
   poly1 = &gl_polys[polyIndex];
   poly2 = &gl_polys[polyIndex + 1];

   recalc = GL_ShouldRecalcPoly(poly1, sector) || GL_ShouldRecalcPoly(poly2, sector);

   if (!recalc)
   {
      // just check the segs for changed textures
      for (i = firstLine; i < maxLine; i++)
      {
         seg = segs + i;

         if (seg->linedef && (GL_SegGeometryChanged(seg) || GL_SegTextureChanged(seg)))
         {
            GL_RecalcSeg(seg, sector);
         }
      }

      if (subSec->poly)
      {
         maxLine = subSec->poly->numsegs;
         for (i = 0; i < maxLine; i++)
         {
            seg = subSec->poly->segs[i];
            GL_RecalcSeg(seg, sector); // always recalc polyobjects for now
         }
      }

      return;
   }

   // recalc all the geometry in the subsector since the either the floor or ceiling sector has moved

   if (!poly1->initialized) GL_InitPolygon(poly1);
   if (!poly2->initialized) GL_InitPolygon(poly2);

   poly1->lastUpdate = frameStartMS;
   poly2->lastUpdate = frameStartMS;

   // sector offsets so rotations happen around sector center
   sox = -subSec->sector->CenterX * MAP_SCALE;
   soy = subSec->sector->CenterY * MAP_SCALE;

   ctex = TexMan(sector->ceilingpic);
   ftex = TexMan(sector->floorpic);
   fsx = fsy = csx = csy = 1.f;

   if (ftex->ScaleX) fsx = 8.f / ftex->ScaleX;
   if (ftex->ScaleY) fsy = 8.f / ftex->ScaleY;
   if (ctex->ScaleX) csx = 8.f / ctex->ScaleX;
   if (ctex->ScaleY) csy = 8.f / ctex->ScaleY;

   for (i = firstLine; i < maxLine; i++)
   {
      seg = segs + i;

      vindex = (maxLine - 1 - i) * 3;
      tindex = (maxLine - 1 - i) * 2;

      poly1->vertices[vindex + 0] = -seg->v2->x * MAP_SCALE;
      poly1->vertices[vindex + 1] = sector->floorplane.ZatPoint(seg->v2) * MAP_SCALE;
      poly1->vertices[vindex + 2] = seg->v2->y * MAP_SCALE;
      poly1->texCoords[tindex + 0] = -poly1->vertices[vindex + 0] / (ftex->GetWidth() * fsx) * MAP_COEFF;
      poly1->texCoords[tindex + 1] = -poly1->vertices[vindex + 2] / (ftex->GetHeight() * fsy) * MAP_COEFF;
      poly1->rotationX = sox / (ftex->GetWidth() * fsx) * MAP_COEFF;
      poly1->rotationY = soy / (ftex->GetHeight() * fsy) * MAP_COEFF;

      vindex = (i - firstLine) * 3;
      tindex = (i - firstLine) * 2;

      poly2->vertices[vindex + 0] = -seg->v2->x * MAP_SCALE;
      poly2->vertices[vindex + 1] = sector->ceilingplane.ZatPoint(seg->v2) * MAP_SCALE;
      poly2->vertices[vindex + 2] = seg->v2->y * MAP_SCALE;
      poly2->texCoords[tindex + 0] = -poly2->vertices[vindex + 0] / (ctex->GetWidth() * csx) * MAP_COEFF;
      poly2->texCoords[tindex + 1] = -poly2->vertices[vindex + 2] / (ctex->GetHeight() * csy) * MAP_COEFF;
      poly2->rotationX = sox / (ctex->GetWidth() * csx) * MAP_COEFF;
      poly2->rotationY = soy / (ctex->GetHeight() * csy) * MAP_COEFF;

      if (seg->linedef) GL_RecalcSeg(seg, sector);

      // update the subsector bounding box for the frustum checks
      // also add check for highest/lowest point for the surrounding lines...
      if (i == firstLine)
      {
         subSec->bbox[0][0] = poly2->vertices[vindex + 0];
         subSec->bbox[1][0] = poly2->vertices[vindex + 0];
         subSec->bbox[0][2] = poly2->vertices[vindex + 2];
         subSec->bbox[1][2] = poly2->vertices[vindex + 2];
         subSec->bbox[0][1] = poly1->vertices[((maxLine - 1 - i) * 3) + 1];
         subSec->bbox[1][1] = poly2->vertices[vindex + 1];
      }
      else
      {
         subSec->bbox[0][0] = MIN<float>(poly2->vertices[vindex + 0], subSec->bbox[0][0]);
         subSec->bbox[1][0] = MAX<float>(poly2->vertices[vindex + 0], subSec->bbox[1][0]);
         subSec->bbox[0][2] = MIN<float>(poly2->vertices[vindex + 2], subSec->bbox[0][2]);
         subSec->bbox[1][2] = MAX<float>(poly2->vertices[vindex + 2], subSec->bbox[1][2]);
         subSec->bbox[0][1] = MIN<float>(poly1->vertices[((maxLine - 1 - i) * 3) + 1], subSec->bbox[0][1]);
         subSec->bbox[1][1] = MAX<float>(poly2->vertices[vindex + 1], subSec->bbox[1][1]);
      }
   }

   if (subSec->poly)
   {
      maxLine = subSec->poly->numsegs;
      for (i = 0; i < maxLine; i++)
      {
         seg = subSec->poly->segs[i];
         if (seg->linedef) GL_RecalcSeg(seg, sector);
      }
   }
}


void GL_RenderPolyWithShader(gl_poly_t *poly, FShader *shader)
{
   unsigned int i, numLayers;
   FShaderLayer *layer;
   FTexture *tex;
   float scaleX, scaleY;

   GL_BindArrays(poly);

   GL_SetupFog(poly->lightLevel, poly->fogR, poly->fogG, poly->fogB);

   // non-masked, so disable transparency
   if (!DrawingDeferredLines && poly->a == 1.f)
   {
      glDisable(GL_BLEND);
   }

   numLayers = shader->layers.Size();
   for (i = 0; i < numLayers; i++)
   {
      layer = shader->layers[i];
      tex = TexMan[layer->name];

      scaleX = TexMan[shader->name]->GetWidth() * 1.f / TexMan[layer->name]->GetWidth();
      scaleY = TexMan[shader->name]->GetHeight() * 1.f / TexMan[layer->name]->GetHeight();

      scaleX = scaleX * poly->scaleX * layer->scaleX;
      scaleY = scaleY * poly->scaleY * layer->scaleY;

      glMatrixMode(GL_TEXTURE);

      glLoadIdentity();
      glScalef(scaleX, scaleY, 1.f);
      glTranslatef(layer->offsetX + poly->offX, layer->offsetY + poly->offY, 0.f);
      // layer rotations happen around the center of the sector
      glTranslatef(-poly->rotationX + layer->centerX, -poly->rotationY + layer->centerY, 0.f);
      glRotatef(layer->rotation, 0.f, 0.f, 1.f);
      glTranslatef(poly->rotationX - layer->centerX, poly->rotationY - layer->centerY, 0.f);
      glRotatef(poly->rot, 0.f, 0.f, 1.f);

      glMatrixMode(GL_MODELVIEW);

      textureList.BindTexture(TexMan[layer->name]);

      glBlendFunc(layer->blendFuncSrc, layer->blendFuncDst);
      // don't use fog on additive layers if not the first layer
      if (i && layer->blendFuncDst == GL_ONE)
      {
         glDisable(GL_FOG);
      }

      switch (layer->texGen)
      {
      case SHADER_TexGen_Sphere:
         glEnable(GL_TEXTURE_GEN_S);
         glEnable(GL_TEXTURE_GEN_T);
         break;
      default:
         glDisable(GL_TEXTURE_GEN_S);
         glDisable(GL_TEXTURE_GEN_T);
         break;
      }

      glColor4f(poly->r * layer->r.GetVal(), poly->g * layer->g.GetVal(), poly->b * layer->b.GetVal(), poly->a * layer->alpha.GetVal());
      glDrawArrays(GL_TRIANGLE_FAN, 0, poly->numPts);

      if (!i) // this only happens on the first pass
      {
         glDepthFunc(GL_EQUAL); // only draw if it matches up with first layer
         glDepthMask(GL_FALSE); // z values already written!
         glAlphaFunc(GL_GREATER, 0.f); // always draw the whole layer (masking is handled on the first pass)
         glEnable(GL_BLEND);
      }
      if (gl_depthfog && !Player->fixedcolormap)
      {
         glEnable(GL_FOG);
      }
   }

   glMatrixMode(GL_TEXTURE);
   glLoadIdentity();
   glMatrixMode(GL_MODELVIEW);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   glDisable(GL_TEXTURE_GEN_S);
   glDisable(GL_TEXTURE_GEN_T);
   glDepthFunc(GL_LEQUAL);
   glDepthMask(GL_TRUE);

   GL_UnbindArrays(poly);
}


void GL_RenderPoly(gl_poly_t *poly)
{
   FAnimDef *anim = NULL;
//   float alpha;
  // unsigned short tex1, tex2, curFrame;

   if (!poly->initialized) return;

   GL_BindArrays(poly);

   if (MaskSkybox)
   {
      glColor4f(1.f, 1.f, 1.f, 1.f);
      textureList.BindGLTexture(0);
      glDrawArrays(GL_TRIANGLE_FAN, 0, poly->numPts);
   }
   else
   {
      GL_SetupFog(poly->lightLevel, poly->fogR, poly->fogG, poly->fogB);

      // non-masked, so disable transparency
      if (!DrawingDeferredLines && poly->a == 1.f)
      {
         glDisable(GL_BLEND);
      }

      if (poly->doomTex > 0)
      {
         if (gl_blend_animations && gl_texture && !gl_wireframe && !textureList.IsTransparent())
         {
            anim = GL_GetAnimForLump(poly->doomTex);
         }
      }
      else
      {
         return;
      }

		glMatrixMode(GL_TEXTURE);
		glLoadIdentity();
		glScalef(poly->scaleX, poly->scaleY, 1.f);
		glTranslatef(poly->offX, poly->offY, 0.f);
		glRotatef(poly->rot, 0.f, 0.f, 1.f);
		glMatrixMode(GL_MODELVIEW);
		glColor4f(poly->r, poly->g, poly->b, poly->a);
		glDrawArrays(GL_TRIANGLE_FAN, 0, poly->numPts);

		glEnable(GL_BLEND);

      glMatrixMode(GL_TEXTURE);
      glLoadIdentity();
      glMatrixMode(GL_MODELVIEW);
   }

   numTris += poly->numPts - 2;

   if (r_fogboundary && poly->isFogBoundary)
   {
#if 0
      // draw poly again with depth-dependent alpha edges
      // note that fog boundaries are always walls, so they're always quads
      float a1, a2;
      fixed_t x, y, dist;
      int i;

      glDisable(GL_TEXTURE_2D);
      glAlphaFunc(GL_GREATER, 0.f);

      x = (fixed_t)(-poly->vertices[3] * MAP_SCALE);
      y = (fixed_t)(poly->vertices[5] * MAP_SCALE);
      dist = R_PointToDist2(x, y);
      a1 = dist / 384.f * INV_FRACUNIT;

      x = (fixed_t)(-poly->vertices[6] * MAP_SCALE);
      y = (fixed_t)(poly->vertices[8] * MAP_SCALE);
      dist = R_PointToDist2(x, y);
      a2 = dist / 384.f * INV_FRACUNIT;

      a1 *= 0.75f * byte2float[poly->fbLightLevel];
      a2 *= 0.75f * byte2float[poly->fbLightLevel];

      //a1 = 0.f;
      //a2 = 1.f;

      glColor4f(byte2float[poly->fbR] * byte2float[poly->fbLightLevel], byte2float[poly->fbG] * byte2float[poly->fbLightLevel], byte2float[poly->fbB] * byte2float[poly->fbLightLevel], a1 * a1);
      glBegin(GL_TRIANGLE_FAN);
      for (i = 0; i < poly->numPts; i++)
      {
         if (i == poly->numPts / 2)
         {
            glColor4f(byte2float[poly->fbR] * byte2float[poly->fbLightLevel], byte2float[poly->fbG] * byte2float[poly->fbLightLevel], byte2float[poly->fbB] * byte2float[poly->fbLightLevel], a2 * a2);
         }
         glVertex3fv(&poly->vertices[i * 3]);
      }
      glEnd();

      glEnable(GL_TEXTURE_2D);
#endif
   }

   // all this really does is potentially unlocks the vertex arrays
   GL_UnbindArrays(poly);
}


// add a poly index to the render list
//  note that the render list is a dynamic array of dynamic arrays
//  the first array is keyed with the OpenGL texture object
//  the sub-array is keyed with the polygon index
//  these are only ever resized up and only the count is reset each frame (they are completely reset on a map change, though)
void RL_AddPoly(GLuint tex, int polyIndex)
{
   RList *rl;
   int amt;
   unsigned int lastSize, i;

   //return;

   if ((tex + 1) >= renderList.Size())
   {
      lastSize = renderList.Size();
      amt = (tex + 1) - renderList.Size();
      renderList.Reserve(amt);
      for (i = lastSize; i < lastSize + amt; i++)
      {
         rl = &renderList[i];
         rl->numPolys = 0;
         rl->polys.Init();
      }
   }

   rl = &renderList[tex];
   if (rl->numPolys == rl->polys.Size())
   {
      rl->polys.Reserve(1);
   }
   rl->polys[rl->numPolys] = polyIndex;
   rl->numPolys++;
}


// completely clear the render list (destroy all the arrays)
void RL_Delete()
{
   RL_Clear();
   renderList.Clear();
}


// only reset the polygon count for the lists
void RL_Clear()
{
   unsigned int i, size;

   size = renderList.Size();

   for (i = 0; i < size; i++)
   {
      renderList[i].numPolys = 0;
   }
}


void RL_SetupMode(int mode)
{
   switch (mode)
   {
   case RL_DEPTH:
   case RL_DEFERRED_DEPTH:
      if (glBlendEquationEXT) glBlendEquationEXT(GL_FUNC_ADD);
      glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
      glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
      glEnable(GL_TEXTURE_2D);
      glEnable(GL_ALPHA_TEST);
      glDisable(GL_TEXTURE_GEN_S);
      glDisable(GL_TEXTURE_GEN_T);
      glDepthFunc(GL_LEQUAL);
      glDepthMask(GL_TRUE);
      glDisable(GL_BLEND);
      glColor3f(1.f, 1.f, 1.f);
     break;
   case RL_BASE:
      if (glBlendEquationEXT) glBlendEquationEXT(GL_FUNC_ADD);
      if (gl_wireframe)
      {
         glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
         glDepthFunc(GL_ALWAYS);
      }
      else
      {
         glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
         glDepthFunc(GL_EQUAL);
         //glDepthFunc(GL_LEQUAL);
      }
      glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
      glEnable(GL_TEXTURE_2D);
      glEnable(GL_ALPHA_TEST);
      glDisable(GL_TEXTURE_GEN_S);
      glDisable(GL_TEXTURE_GEN_T);
      glEnable(GL_BLEND);
      glBlendFunc(GL_ONE, GL_ZERO);
      glDepthMask(GL_FALSE);
      if (gl_depthfog && !Player->fixedcolormap)
      {
         glEnable(GL_FOG);
      }
      else
      {
         glDisable(GL_FOG);
      }
      glDisable(GL_TEXTURE_2D);
      glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_EXT);
      glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_EXT, GL_MODULATE); // modulate = A*B
      glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_EXT, GL_TEXTURE1_ARB); // argA
      glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB_EXT, GL_SRC_COLOR);
      glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_EXT, GL_TEXTURE0_ARB); // argB
      glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_ALPHA_EXT, GL_SRC_ALPHA);
     break;
   case RL_TEXTURED:
   case RL_TEXTURED_CLAMPED:
      if (glBlendEquationEXT) glBlendEquationEXT(GL_FUNC_ADD);
      glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
      glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
      glEnable(GL_TEXTURE_2D);
      glEnable(GL_ALPHA_TEST);
      glDisable(GL_TEXTURE_GEN_S);
      glDisable(GL_TEXTURE_GEN_T);
      glEnable(GL_BLEND);
      glBlendFunc(GL_DST_COLOR, GL_ZERO);
      glDepthFunc(GL_EQUAL);
      glDepthMask(GL_FALSE);
     break;
   case RL_LIGHTS:
      if (glBlendEquationEXT) glBlendEquationEXT(GL_FUNC_ADD);
      glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
      glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
      glEnable(GL_TEXTURE_2D);
      glDisable(GL_TEXTURE_GEN_S);
      glDisable(GL_TEXTURE_GEN_T);
      glDisable(GL_ALPHA_TEST);
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE);
      glDepthFunc(GL_EQUAL);
      glDepthMask(GL_FALSE);
     break;
   case RL_FOG:
   case RL_FOG_BOUNDARY:
      if (glBlendEquationEXT) glBlendEquationEXT(GL_FUNC_ADD);
      glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
      glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
      glDisable(GL_ALPHA_TEST);
      glDisable(GL_TEXTURE_2D);
      glDisable(GL_TEXTURE_GEN_S);
      glDisable(GL_TEXTURE_GEN_T);
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE);
      glDepthFunc(GL_EQUAL);
      glDepthMask(GL_FALSE);
      glEnable(GL_FOG);
     break;
   case RL_RESET:
      if (glBlendEquationEXT) glBlendEquationEXT(GL_FUNC_ADD);
      glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
      glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
      if (gl_wireframe)
      {
         glDisable(GL_TEXTURE_2D);
      }
      else
      {
         glEnable(GL_TEXTURE_2D);
      }
      glDisable(GL_TEXTURE_GEN_S);
      glDisable(GL_TEXTURE_GEN_T);
      glEnable(GL_DEPTH_TEST);
      glEnable(GL_BLEND);
      glEnable(GL_ALPHA_TEST);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glDepthFunc(GL_LEQUAL);
      glDepthMask(GL_TRUE);
      glAlphaFunc(GL_GREATER, 0.f);
      if (gl_depthfog && !Player->fixedcolormap)
      {
         glEnable(GL_FOG);
      }
      else
      {
         glDisable(GL_FOG);
      }
     break;
   case RL_MASK:
      if (glBlendEquationEXT) glBlendEquationEXT(GL_FUNC_ADD);
      glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
      glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
      glDisable(GL_TEXTURE_2D);
      glDisable(GL_ALPHA_TEST);
      glDisable(GL_TEXTURE_GEN_S);
      glDisable(GL_TEXTURE_GEN_T);
      glDepthFunc(GL_LEQUAL);
      glDepthMask(GL_TRUE);
     break;
   case RL_DEFERRED:
      if (glBlendEquationEXT) glBlendEquationEXT(GL_FUNC_ADD);
      glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
      glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
      if (gl_wireframe)
      {
         glDisable(GL_TEXTURE_2D);
      }
      else
      {
         glEnable(GL_TEXTURE_2D);
      }
      glDisable(GL_TEXTURE_GEN_S);
      glDisable(GL_TEXTURE_GEN_T);
      glEnable(GL_BLEND);
      glEnable(GL_ALPHA_TEST);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glDepthFunc(GL_LEQUAL);
      glDepthMask(GL_TRUE);
      if (gl_depthfog && !Player->fixedcolormap)
      {
         glEnable(GL_FOG);
      }
      else
      {
         glDisable(GL_FOG);
      }
     break;
   default:
     break;
   }
}

// loop through the renderlist and render all polys
void RL_RenderList()
{
   unsigned int i, j, numLists, numPolys;
   RList *rl;
   gl_poly_t *poly;
   FShader *shader;

   numLists = renderList.Size();
   for (i = 0; i < numLists; i++)
   {
      rl = &renderList[i];
      numPolys = renderList[i].numPolys;
      if (numPolys > 0)
      {
         // manually set up first poly to bind texture (so textures can be updated)
         poly = &gl_polys[renderList[i].polys[0]];
         // hmm, should translations be applied to shaders??
         textureList.SetTranslation(poly->translation);

         if ((poly->doomTex >= 0) && !MaskSkybox)
         {
            shader = GL_ShaderForTexture(TexMan[poly->doomTex]);
         }
         else
         {
            shader = NULL;
         }

         if (shader)
         {
            GL_RenderPolyWithShader(poly, shader);
            for (j = 1; j < numPolys; j++)
            {
               poly = &gl_polys[renderList[i].polys[j]];
               GL_RenderPolyWithShader(poly, shader);
            }
         }
         else
         {
            // only bind the texture if no shader is found
            textureList.BindTexture(poly->doomTex, true);
            GL_RenderPoly(poly);
            for (j = 1; j < numPolys; j++)
            {
               poly = &gl_polys[renderList[i].polys[j]];
               GL_RenderPoly(poly);
            }
         }
      }
   }

   textureList.SetTranslation((byte *)NULL);

   glMatrixMode(GL_TEXTURE);
   glLoadIdentity();
   glMatrixMode(GL_MODELVIEW);
}
