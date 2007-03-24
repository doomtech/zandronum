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
#include "glext.h"

#include "zgl_lights.h"
#include "zgl_main.h"
#include "d_player.h"
#include "p_local.h"
#include "p_lnspec.h"
#include "templates.h"

#include "zgl_shaders.h"

#include "gi.h"

extern PFNGLGENBUFFERSARBPROC glGenBuffersARB;
extern PFNGLBINDBUFFERARBPROC glBindBufferARB;
extern PFNGLBUFFERDATAARBPROC glBufferDataARB;
extern PFNGLDELETEBUFFERSARBPROC glDeleteBuffersARB;
extern PFNGLBLENDEQUATIONEXTPROC glBlendEquationEXT;


#define MAX(a, b) ((a > b) ? a : b)
#define MIN(a, b) ((a < b) ? a : b)


EXTERN_CVAR(Bool, gl_blend_animations)
EXTERN_CVAR(Bool, gl_texture)
EXTERN_CVAR(Bool, gl_wireframe)
EXTERN_CVAR(Bool, gl_depthfog)
EXTERN_CVAR (Bool, r_fogboundary)
CVAR(Bool, gl_texture_multitexture, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Bool, gl_lights_checkside, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)


extern level_locals_t level;
extern float *verts, *texCoords;
extern int numTris;
extern bool MaskSkybox, DrawingDeferredLines;
extern player_t *Player;
extern TArray<TArray<ADynamicLight *> *> SubsecLights;

extern PFNGLLOCKARRAYSEXTPROC glLockArraysEXT;
extern PFNGLUNLOCKARRAYSEXTPROC glUnlockArraysEXT;
extern PFNGLMULTITEXCOORD1FARBPROC glMultiTexCoord1fARB;
extern PFNGLMULTITEXCOORD2FARBPROC glMultiTexCoord2fARB;
extern PFNGLMULTITEXCOORD3FARBPROC glMultiTexCoord3fARB;
extern PFNGLMULTITEXCOORD4FARBPROC glMultiTexCoord4fARB;
extern PFNGLACTIVETEXTUREARBPROC glActiveTextureARB;
extern PFNGLCLIENTACTIVETEXTUREARBPROC glClientActiveTextureARB;


TArray<RList> renderList;
unsigned int maxList;
unsigned int frameStartMS;

unsigned int VertexArraySize;
float *TexCoordArray, *VertexArray;


class RListNode
{
public:
   RListNode();
   ~RListNode();
   void AddPoly(gl_poly_t *poly);
   RListNode *left, *right, *next;
   gl_poly_t *poly;
};


RListNode::RListNode()
{
   left = right = next = NULL;
   poly = NULL;
}


RListNode::~RListNode()
{
   if (next)
   {
      delete next;
      next = NULL;
   }
   if (left)
   {
      delete left;
      left = NULL;
   }
   if (right)
   {
      delete right;
      right = NULL;
   }
}


void RListNode::AddPoly(gl_poly_t *p)
{
   if (p->lightLevel == poly->lightLevel)
   {
      // add to list
      RListNode *node;
      if (next)
      {
         node = new RListNode();
         node->poly = p;
         node->next = next;
         next = node;
      }
      else
      {
         next = new RListNode();
         next->poly = p;
      }
   }
   else if (p->lightLevel < poly->lightLevel)
   {
      // add left
      if (left)
      {
         left->AddPoly(p);
      }
      else
      {
         left = new RListNode();
         left->poly = p;
      }
   }
   else
   {
      // add right
      if (right)
      {
         right->AddPoly(p);
      }
      else
      {
         right = new RListNode();
         right->poly = p;
      }
   }
}


gl_poly_t::gl_poly_t()
{
   numPts = 0;
   subsectorIndex = 0;
   rotationX = 0.f;
   rotationY = 0.f;
}

gl_poly_t::~gl_poly_t()
{
}


void GL_BindArrays(gl_poly_t *poly)
{
}


void GL_UnbindArrays(gl_poly_t *poly)
{
}


void GL_InitPolygon(gl_poly_t *poly)
{
   int index, i;

   if (poly->numPts == 0) return;

   if (!poly->initialized)
   {
      poly->vertices = VertexArray + (poly->arrayIndex * 3);
      poly->texCoords = TexCoordArray + (poly->arrayIndex * 2);

      index = 0;
      for (i = 1; i < poly->numPts - 1; i++)
      {
         poly->indices[index++] = poly->arrayIndex;
         poly->indices[index++] = poly->arrayIndex + i;
         poly->indices[index++] = poly->arrayIndex + i + 1;
      }
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

   if (sector->lastUpdate > poly->lastUpdate)
   {
      return true;
   }

   if ((sector->heightsec) && (sector->heightsec->lastUpdate > poly->lastUpdate))
   {
      return true;
   }

   return false;
}


bool GL_ShouldRecalcSeg(seg_t *seg, gl_poly_t *poly, sector_t *frontSector, sector_t *backSector)
{
   unsigned int textureChanged = seg->linedef->textureChanged;

   if (seg->bPolySeg) return true;
   if (textureChanged > poly->lastUpdate) return true;
   if (GL_ShouldRecalcPoly(poly, frontSector)) return true;
   if (backSector && GL_ShouldRecalcPoly(poly, backSector)) return true;

   return false;
}


void GL_RecalcPolyPlane(gl_poly_t *poly)
{
   poly->plane.Init(poly->vertices, poly->numPts);
   //poly->plane.Init(poly->vertices + 0, poly->vertices + 3, poly->vertices + 6);
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

   vert1 = seg->v1;
   vert2 = seg->v2;
   dist = seg->length;

   backSector = GL_FakeFlat(seg->backsector, &ts2, NULL, NULL, false);

   // texscale: 8 = 1.0, 16 = 2.0, 4 = 0.5
   txScale = tex->ScaleX ? tex->ScaleX / 8.f : 1.f;
   tyScale = tex->ScaleY ? tex->ScaleY / 8.f : 1.f;

   xOffset = seg->offset * txScale * INV_FRACUNIT;

   v1[0] = v3[0] = -vert1->x * MAP_SCALE;
   v1[2] = v3[2] = vert1->y * MAP_SCALE;
   v2[0] = v4[0] = -vert2->x * MAP_SCALE;
   v2[2] = v4[2] = vert2->y * MAP_SCALE;

   v1[1] = static_cast<float>(backSector->ceilingplane.ZatPoint(vert1));
   v2[1] = static_cast<float>(backSector->ceilingplane.ZatPoint(vert2));
   v3[1] = static_cast<float>(frontSector->ceilingplane.ZatPoint(vert1));
   v4[1] = static_cast<float>(frontSector->ceilingplane.ZatPoint(vert2));

   ll.x = xOffset / (tex->GetWidth() * 1.f);
   ul.x = ll.x;
   lr.x = ll.x + (dist / (tex->GetWidth() * 1.f) * txScale);
   ur.x = lr.x;

   ul.y = ur.y = 1.f;
   ll.y = lr.y = 0.f;

   if (tex)
   {
      texHeight = tex->GetHeight() / tyScale;
#if 0
      lowerHeight = backSector->ceilingtexz * INV_FRACUNIT;
      upperHeight = frontSector->ceilingtexz * INV_FRACUNIT;
#else
      lowerHeight = seg->backsector->ceilingtexz * INV_FRACUNIT;
      upperHeight = seg->frontsector->ceilingtexz * INV_FRACUNIT;
#endif
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

   GL_RecalcPolyPlane(poly);
   poly->lastUpdate = MAX<int>(frontSector->lastUpdate, backSector->lastUpdate);
}


void GL_RecalcMidWall(seg_t *seg, sector_t *frontSector, gl_poly_t *poly)
{
   float height1, height2, v1[5], v2[5], v3[5], v4[5], txScale, tyScale;
   float yOffset1, yOffset2, yOffset, xOffset;
   float upperHeight, lowerHeight;
   float tmpOffset = seg->sidedef->rowoffset * INV_FRACUNIT;
   long offset = 0;
   float texHeight, texWidth;
   FTexture *tex = NULL;
   texcoord_t ll, ul, lr, ur;
   sector_t *backSector, ts2;
   vertex_t *vert1, *vert2;

   if (!poly->initialized) GL_InitPolygon(poly);

   if (seg->sidedef->midtexture)
   {
      tex = TexMan(seg->sidedef->midtexture);
   }
   else
   {
      tex = NULL;
   }

   vert1 = seg->v1;
   vert2 = seg->v2;

   backSector = GL_FakeFlat(seg->backsector, &ts2, NULL, NULL, false);

   v1[0] = -vert1->x * MAP_SCALE;
   v1[2] = vert1->y * MAP_SCALE;
   v2[0] = -vert2->x * MAP_SCALE;
   v2[2] = vert2->y * MAP_SCALE;
   v1[1] = static_cast<float>(frontSector->floorplane.ZatPoint(seg->v1));
   v2[1] = static_cast<float>(frontSector->floorplane.ZatPoint(seg->v2));

   if ((seg->linedef->special == Line_Mirror && seg->backsector == NULL) || tex == NULL)
   {
      if (backSector)
      {
         height1 = static_cast<float>(backSector->ceilingplane.ZatPoint(vert1));
         height2 = static_cast<float>(backSector->ceilingplane.ZatPoint(vert2));
      }
      else
      {
         height1 = static_cast<float>(frontSector->ceilingplane.ZatPoint(vert1));
         height2 = static_cast<float>(frontSector->ceilingplane.ZatPoint(vert2));
      }
   }
   else
   {
      yOffset = 0.f;
      tmpOffset = 0.f;
      offset = seg->sidedef->rowoffset;
      yOffset1 = yOffset2 = yOffset;

      // texscale: 8 = 1.0, 16 = 2.0, 4 = 0.5
      txScale = tex->ScaleX ? tex->ScaleX / 8.f : 1.f;
      tyScale = tex->ScaleY ? tex->ScaleY / 8.f : 1.f;

      xOffset = seg->offset * INV_FRACUNIT;
   
      texHeight = tex->GetHeight() / tyScale;
      texWidth = tex->GetWidth() / txScale;
      textureList.GetTexture(seg->sidedef->midtexture, true);

      ll.x = xOffset / texWidth;
      ul.x = ll.x;
      lr.x = ll.x + (seg->length / texWidth);
      ur.x = lr.x;

      if (backSector)
      {
         height1 = static_cast<float>(MIN(frontSector->ceilingplane.ZatPoint(vert1), backSector->ceilingplane.ZatPoint(vert1)));
         height2 = static_cast<float>(MIN(frontSector->ceilingplane.ZatPoint(vert2), backSector->ceilingplane.ZatPoint(vert2)));
      }
      else
      {
         height1 = static_cast<float>(frontSector->ceilingplane.ZatPoint(vert1));
         height2 = static_cast<float>(frontSector->ceilingplane.ZatPoint(vert2));
      }

      if (backSector && (textureList.IsTransparent() || seg->linedef->flags & ML_TWOSIDED))
      {
#if 0
         fixed_t rbceil = backSector->ceilingtexz;
         fixed_t rbfloor = backSector->floortexz;
         fixed_t rfceil = frontSector->ceilingtexz;
         fixed_t rffloor = frontSector->floortexz;
#else
         fixed_t rbceil = seg->backsector->ceilingtexz;
         fixed_t rbfloor = seg->backsector->floortexz;
         fixed_t rfceil = seg->frontsector->ceilingtexz;
         fixed_t rffloor = seg->frontsector->floortexz;
#endif

         yOffset = static_cast<float>(seg->sidedef->rowoffset);
         if (!tex->bWorldPanning)
         {
            yOffset /= tyScale;
         }

         height1 = static_cast<float>(MIN(rbceil, rfceil));
         v1[1] = static_cast<float>(MAX(rbfloor, rffloor));

         if (seg->linedef->flags & ML_DONTPEGBOTTOM)
         {
            v1[1] += yOffset;
            height1 = v1[1] + (texHeight * FRACUNIT);
         }
         else
         {
            height1 += yOffset;
            v1[1] = height1 - (texHeight * FRACUNIT);
         }

         height2 = height1;
         v2[1] = v1[1];
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
            polyBottom = openBottom + (seg->sidedef->rowoffset * INV_FRACUNIT / tyScale);
            polyTop = polyBottom + texHeight;

            upperHeight = MIN(openTop, polyTop);
            lowerHeight = MAX(openBottom, polyBottom);

            yOffset = lowerHeight + texHeight - upperHeight + polyBottom - lowerHeight;
         }
         else
         {
            polyTop = openTop + (seg->sidedef->rowoffset * INV_FRACUNIT / tyScale);
            polyBottom = polyTop - texHeight;

            yOffset = polyTop - upperHeight;
         }
      }
      else
      {
         lowerHeight = frontSector->floortexz * INV_FRACUNIT;
         upperHeight = frontSector->ceilingtexz * INV_FRACUNIT;

         if (seg->linedef->flags & ML_DONTPEGBOTTOM)
         {
            yOffset = lowerHeight + texHeight - upperHeight;
         }
      }

      ll.y = lr.y = yOffset / texHeight;
      ul.y = ur.y = ll.y + ((upperHeight - lowerHeight) / texHeight);

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

   GL_RecalcPolyPlane(poly);
   if (backSector)
   {
      poly->lastUpdate = MAX<int>(frontSector->lastUpdate, backSector->lastUpdate);
   }
   else
   {
      poly->lastUpdate = frontSector->lastUpdate;
   }
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

   vert1 = seg->v1;
   vert2 = seg->v2;
   dist = seg->length;

   backSector = GL_FakeFlat(seg->backsector, &ts2, NULL, NULL, false);

   h1 = (backSector->floorplane.ZatPoint(vert1) - frontSector->floorplane.ZatPoint(vert1));
   h2 = (backSector->floorplane.ZatPoint(vert2) - frontSector->floorplane.ZatPoint(vert2));

   v1[0] = v3[0] = -vert1->x * MAP_SCALE;
   v1[2] = v3[2] = vert1->y * MAP_SCALE;
   v2[0] = v4[0] = -vert2->x * MAP_SCALE;
   v2[2] = v4[2] = vert2->y * MAP_SCALE;

   v1[1] = static_cast<float>(frontSector->floorplane.ZatPoint(vert1));
   v2[1] = static_cast<float>(frontSector->floorplane.ZatPoint(vert2));
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
#if 0
      lowerHeight = frontSector->floortexz * INV_FRACUNIT;
      upperHeight = backSector->floortexz * INV_FRACUNIT;
#else
      lowerHeight = seg->frontsector->floortexz * INV_FRACUNIT;
      upperHeight = seg->backsector->floortexz * INV_FRACUNIT;
#endif
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

   GL_RecalcPolyPlane(poly);
   poly->lastUpdate = MAX<int>(frontSector->lastUpdate, backSector->lastUpdate);
}


void GL_RecalcSeg(seg_t *seg, sector_t *controlSector)
{
   __w64 int index;
   bool fogBoundary = false;
   sector_t *backSector, ts;
   gl_poly_t *poly;

   index = numsubsectors * 2;
   index += (seg - segs) * 3;

   backSector = GL_FakeFlat(seg->backsector, &ts, NULL, NULL, false);
   if (backSector)
   {
      fogBoundary = IsFogBoundary(controlSector, backSector);
   }

   if (seg->linedef && seg->sidedef)
   {
      poly = gl_polys + index;

      if (seg->backsector && seg->sidedef->toptexture && GL_ShouldRecalcSeg(seg, poly, controlSector, backSector))
      {
         GL_RecalcUpperWall(seg, controlSector, poly);
      }

      poly++;

      if ((seg->sidedef->midtexture || fogBoundary) && GL_ShouldRecalcSeg(seg, poly, controlSector, backSector))
      {
         GL_RecalcMidWall(seg, controlSector, poly);
      }

      poly++;

      if (seg->backsector && seg->sidedef->bottomtexture && GL_ShouldRecalcSeg(seg, poly, controlSector, backSector))
      {
         GL_RecalcLowerWall(seg, controlSector, poly);
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

         if (seg->linedef)
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

   // plane is already calculated for the floor/ceiling, so just convert it to a format we can use
   poly1->plane.Set(sector->floorplane);
   poly2->plane.Set(sector->ceilingplane);

   poly1->lastUpdate = sector->lastUpdate;
   poly2->lastUpdate = sector->lastUpdate;

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


void GL_SetupForPoly(gl_poly_t *poly)
{
   GL_BindArrays(poly);
}


void GL_FinishPoly(gl_poly_t *poly)
{
   GL_UnbindArrays(poly);
}


void GL_RenderPoly(gl_poly_t *poly, bool deferred)
{
   FAnimDef *anim = NULL;
   float alpha;
   unsigned short tex1, tex2, curFrame;

   if (!poly->initialized) return;

   GL_SetupForPoly(poly);

   if (MaskSkybox)
   {
      glColor4f(1.f, 1.f, 1.f, 1.f);
      glDisable(GL_TEXTURE_2D);
      glDrawArrays(GL_TRIANGLE_FAN, 0, poly->numPts);
      glEnable(GL_TEXTURE_2D);
   }
   else
   {
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

      if (anim && gl_texture_multitexture)
      {
         curFrame = anim->CurFrame;

         if (0)//(anim->bUniqueFrames)
         {
            tex1 = anim->Frames[curFrame].FramePic;
         }
         else
         {
            tex1 = anim->BasePic + curFrame;
         }

         tex2 = GL_NextAnimFrame(anim);

         alpha = 0;//(anim->Countdown - (r_TicFrac * INV_FRACUNIT)) / (anim->StartCount * 1.f);

         // render in 1 pass
         float colors[4];
         colors[3] = alpha; // don't care about the other values (unused)

         // first interpolate between the two textures
         // use the alpha value from the texture environment for the blend amount
         // this keeps the blending seperate from the geometry alpha (so we can blend semi-transparent walls)
         textureList.SelectTexUnit(GL_TEXTURE0_ARB);
         glEnable(GL_TEXTURE_2D);
         // each texture unit has its own texture matrix, so set texture offset/rotation up for each
         glMatrixMode(GL_TEXTURE);
         glLoadIdentity();
         glScalef(poly->scaleX, poly->scaleY, 1.f);
         glTranslatef(poly->offX, poly->offY, 0.f);
         glRotatef(poly->rot, 0.f, 0.f, 1.f);
         glMatrixMode(GL_MODELVIEW);
         textureList.BindTexture(tex1, false);
         glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, colors);
         glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_EXT);
         glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_EXT, GL_INTERPOLATE_EXT); // interpolation = A*C + B*(1-C)
         glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_EXT, GL_TEXTURE0_ARB); // argA
         glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB_EXT, GL_SRC_COLOR);
         glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_EXT, GL_TEXTURE1_ARB); // argB
         glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB_EXT, GL_SRC_COLOR);
         glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE2_RGB_EXT, GL_CONSTANT_EXT); // argC (GL_CONSTANT_EXT = value from tex env color)
         glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_RGB_EXT, GL_SRC_ALPHA);

         // blend the resulting texture with the actual geometry color/alpha
         textureList.SelectTexUnit(GL_TEXTURE1_ARB);
         glEnable(GL_TEXTURE_2D);
         // each texture unit has its own texture matrix, so set texture offset/rotation up for each
         glMatrixMode(GL_TEXTURE);
         glLoadIdentity();
         glScalef(poly->scaleX, poly->scaleY, 1.f);
         glTranslatef(poly->offX, poly->offY, 0.f);
         glRotatef(poly->rot, 0.f, 0.f, 1.f);
         glMatrixMode(GL_MODELVIEW);
         textureList.BindTexture(tex2, false);
         glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_EXT);
         glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_EXT, GL_MODULATE); // modulate = A*B
         glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_EXT, GL_PREVIOUS_EXT); // argA (PREVIOUS = value from previous tex unit (interpolation)
         glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB_EXT, GL_SRC_COLOR);
         glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_EXT, GL_PRIMARY_COLOR_EXT); // argB (PRIMARY_COLOR = value from glColor*
         glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB_EXT, GL_SRC_COLOR);

         if (deferred)
         {
            glColor4f(poly->r, poly->g, poly->b, poly->a);
         }
         else
         {
            glColor4f(1.f, 1.f, 1.f, poly->a);
         }
         glDrawArrays(GL_TRIANGLE_FAN, poly->arrayIndex, poly->numPts);

         // clean up afterwards, making sure to set TEXTURE0 back to the default setting
         textureList.SelectTexUnit(GL_TEXTURE1_ARB);
         glDisable(GL_TEXTURE_2D);
         textureList.SelectTexUnit(GL_TEXTURE0_ARB);
         glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
         glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_EXT, GL_TEXTURE);
         glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_EXT, GL_PREVIOUS_EXT);
         glEnable(GL_BLEND);
         glEnable(GL_ALPHA_TEST);
      }
      else
      {
         glMatrixMode(GL_TEXTURE);
         glLoadIdentity();
         glScalef(poly->scaleX, poly->scaleY, 1.f);
         //glTranslatef(-poly->rotationX, -poly->rotationY, 0.f);
         glRotatef(poly->rot, 0.f, 0.f, 1.f);
         //glTranslatef(poly->rotationX, poly->rotationY, 0.f);
         glTranslatef(poly->offX, poly->offY, 0.f);

         glMatrixMode(GL_MODELVIEW);
         if (deferred)
         {
            glColor4f(poly->r, poly->g, poly->b, poly->a);
         }
         else
         {
            glColor4f(1.f, 1.f, 1.f, poly->a);
         }

         glDrawArrays(GL_TRIANGLE_FAN, poly->arrayIndex, poly->numPts);

         glEnable(GL_BLEND);
         glEnable(GL_ALPHA_TEST);
      }

      glMatrixMode(GL_TEXTURE);
      glLoadIdentity();
      glMatrixMode(GL_MODELVIEW);
   }

   numTris += poly->numPts - 2;

   GL_FinishPoly(poly);

   RL_SetupMode(RL_TEXTURED);
}


void GL_RenderPolyWithShader(gl_poly_t *poly, FShader *shader, bool deferred)
{
   unsigned int i, numLayers;
   FShaderLayer *layer;
   FTexture *rootTex, *tex;
   float scaleX, scaleY, r, g, b, a;

   //if (deferred) GL_SetupFog(poly->lightLevel, 0, 0, 0);
   if (deferred) GL_SetupFog(poly->lightLevel, poly->fogR, poly->fogG, poly->fogB);

   if (!shader)
   {
      GL_RenderPoly(poly, deferred);
      return;
   }

   GL_SetupForPoly(poly);

   rootTex = TexMan[shader->name];

   numLayers = shader->layers.Size();
   for (i = 0; i < numLayers; i++)
   {
      layer = shader->layers[i];

      if (layer->alpha == 0.f) continue;

      tex = layer->animate ? TexMan(layer->name) : TexMan[layer->name];

      switch (layer->texgen)
      {
      case SHADER_TexGen_Sphere:
         glEnable(GL_TEXTURE_GEN_S);
         glEnable(GL_TEXTURE_GEN_T);
         scaleX = 2.f;
         scaleY = 2.f;
         break;
      default:
         glDisable(GL_TEXTURE_GEN_S);
         glDisable(GL_TEXTURE_GEN_T);
         scaleX = rootTex->GetWidth() * 1.f / tex->GetWidth();
         scaleY = rootTex->GetHeight() * 1.f / tex->GetHeight();
         scaleX = scaleX * poly->scaleX * layer->scaleX;
         scaleY = scaleY * poly->scaleY * layer->scaleY;
         break;
      }

      glMatrixMode(GL_TEXTURE);

      glLoadIdentity();
      glTranslatef(-poly->rotationX, -poly->rotationY, 0.f);
      glRotatef(layer->rotation, 0.f, 0.f, 1.f);
      glTranslatef(poly->rotationX, poly->rotationY, 0.f);
      glScalef(scaleX, scaleY, 0.f);
      glTranslatef(poly->offX + ((layer->offsetX + layer->adjustX) / layer->scaleX), poly->offY + ((layer->offsetY + layer->adjustY) / layer->scaleY), 0.f);
      glRotatef(poly->rot, 0.f, 0.f, 1.f);
      glMatrixMode(GL_MODELVIEW);

      // base layer on solid geometry is always blended with lights
      if (!i && (poly->a == 1.f || !deferred))
      {
         glBlendFunc(GL_DST_COLOR, GL_ZERO);
      }
      else
      {
         glBlendFunc(layer->blendFuncSrc, layer->blendFuncDst);
      }

      if ((i || deferred) && !layer->emissive)
      {
         r = layer->r * poly->r;
         g = layer->g * poly->g;
         b = layer->b * poly->b;
         a = layer->alpha * poly->a;
      }
      else
      {
         r = layer->r;
         g = layer->g;
         b = layer->b;
         a = layer->alpha * poly->a;
      }

      glColor4f(r, g, b, a);

      if (layer->layerMask)
      {
         textureList.SelectTexUnit(GL_TEXTURE0_ARB);
         textureList.BindTexture(tex);
         glEnable(GL_TEXTURE_2D);

         glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_EXT);
         glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_EXT, GL_MODULATE); // modulate = A*B
         glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_EXT, GL_TEXTURE0_ARB); // argA
         glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB_EXT, GL_SRC_COLOR);
         glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_EXT, GL_TEXTURE1_ARB); // argB
         glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB_EXT, GL_SRC_COLOR);

         // blend the resulting texture with the actual geometry color/alpha
         textureList.SelectTexUnit(GL_TEXTURE1_ARB);
         tex = layer->layerMask->animate ? TexMan(layer->layerMask->name) : TexMan[layer->layerMask->name];

         // be sure to calculate the mask scale the same as the layer scale
         scaleX = rootTex->GetWidth() * 1.f / tex->GetWidth();
         scaleY = rootTex->GetHeight() * 1.f / tex->GetHeight();
         scaleX = scaleX * poly->scaleX * layer->layerMask->scaleX;
         scaleY = scaleY * poly->scaleY * layer->layerMask->scaleY;

         textureList.BindTexture(tex);
         glEnable(GL_TEXTURE_2D);

         glMatrixMode(GL_TEXTURE);
         glLoadIdentity();
         glTranslatef(-poly->rotationX, -poly->rotationY, 0.f);
         glRotatef(layer->layerMask->rotation, 0.f, 0.f, 1.f);
         glTranslatef(poly->rotationX, poly->rotationY, 0.f);
         glScalef(scaleX, scaleY, 0.f);
         glTranslatef(poly->offX + layer->layerMask->offsetX + layer->layerMask->adjustX, poly->offY + layer->layerMask->offsetY + layer->layerMask->adjustY, 0.f);
         glRotatef(poly->rot, 0.f, 0.f, 1.f);
         glMatrixMode(GL_MODELVIEW);

         glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_EXT);
         glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_EXT, GL_MODULATE); // modulate = A*B
         glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_EXT, GL_PREVIOUS_EXT);
         glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB_EXT, GL_SRC_COLOR);
         glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_EXT, GL_PRIMARY_COLOR_EXT); // argB (PRIMARY_COLOR = value from glColor*)
         glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_EXT, GL_PRIMARY_COLOR_EXT); // argB (PRIMARY_COLOR = value from glColor*)
         glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB_EXT, GL_SRC_COLOR);
         glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_ALPHA_EXT, GL_SRC_COLOR);
      }
      else
      {
         textureList.BindTexture(tex);
      }

      glDrawArrays(GL_TRIANGLE_FAN, poly->arrayIndex, poly->numPts);

      if (layer->layerMask)
      {
         textureList.SelectTexUnit(GL_TEXTURE1_ARB);
         glDisable(GL_TEXTURE_2D);
         // make sure to change things back to the defaults on the primary texture unit
         textureList.SelectTexUnit(GL_TEXTURE0_ARB);
         glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
         glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_EXT, GL_PREVIOUS_EXT);
      }

      if (!i) // this only happens on the first pass
      {
         glAlphaFunc(GL_GREATER, 0.f); // always draw the whole layer (masking is handled on the first pass)
      }
   }

   glMatrixMode(GL_TEXTURE);
   glLoadIdentity();
   glMatrixMode(GL_MODELVIEW);
   glDisable(GL_TEXTURE_GEN_S);
   glDisable(GL_TEXTURE_GEN_T);

   glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

   GL_FinishPoly(poly);

   RL_SetupMode(RL_TEXTURED);
}


void GL_RenderPolyBase(gl_poly_t *poly)
{
   GL_SetupForPoly(poly);

   glMatrixMode(GL_TEXTURE);
   glLoadIdentity();
   glScalef(poly->scaleX, poly->scaleY, 1.f);
   glTranslatef(poly->offX, poly->offY, 0.f);
   glRotatef(poly->rot, 0.f, 0.f, 1.f);
   glMatrixMode(GL_MODELVIEW);

   // this is the base color pass, so the fog is done here, too
   GL_SetupFog(poly->lightLevel, 0, 0, 0);
   glColor3f(poly->r, poly->g, poly->b);
   //glColor3f(1.f, 1.f, 1.f);
   glDrawArrays(GL_TRIANGLE_FAN, poly->arrayIndex, poly->numPts);

   GL_FinishPoly(poly);
}


void GL_RenderPolyDepth(gl_poly_t *poly, FShaderLayer *layer)
{
   GL_SetupForPoly(poly);

   // have to take into account the texture offset for the mask texture here, as well
   glMatrixMode(GL_TEXTURE);
   glLoadIdentity();
   if (layer)
   {
      glTranslatef(layer->offsetX + layer->adjustX, layer->offsetY + layer->adjustY, 0.f);
   }
   glScalef(poly->scaleX, poly->scaleY, 1.f);
   glTranslatef(poly->offX, poly->offY, 0.f);
   glRotatef(poly->rot, 0.f, 0.f, 1.f);
   glMatrixMode(GL_MODELVIEW);

   glDrawArrays(GL_TRIANGLE_FAN, poly->arrayIndex, poly->numPts);

   GL_FinishPoly(poly);

   glMatrixMode(GL_TEXTURE);
   glLoadIdentity();
   glMatrixMode(GL_MODELVIEW);
}


void GL_RenderPolyFogged(gl_poly_t *poly)
{
   if (poly->fogR == 0 && poly->fogG == 0 && poly->fogB == 0) return;

   GL_SetupForPoly(poly);
   GL_SetupFog(poly->lightLevel, poly->fogR, poly->fogG, poly->fogB);

   glColor4f(0.f, 0.f, 0.f, 1.f);
   glDrawArrays(GL_TRIANGLE_FAN, poly->arrayIndex, poly->numPts);

   GL_FinishPoly(poly);
}


void GL_RenderPolyFogBoundary(gl_poly_t *poly)
{
   //if (poly->fogR == 0 && poly->fogG == 0 && poly->fogB == 0) return;

   GL_SetupForPoly(poly);
   GL_SetupFog(poly->fbLightLevel, poly->fbR, poly->fbG, poly->fbB);

   glColor4f(0.f, 0.f, 0.f, 1.f);
   glDrawArrays(GL_TRIANGLE_FAN, poly->arrayIndex, poly->numPts);

   GL_FinishPoly(poly);
}


void GL_RenderPolyLit(gl_poly_t *poly)
{
   APointLight *light;
   fixed_t lx, ly, lz;
   float x, y, z, radius, dist, scale, cs, u, v, frac, fracRad, r, g, b, a;
   Vector pos, fn, nearPt, nearToVert, right, up, t1;

   fn = poly->plane.Normal();

   for (unsigned int lindex = 0; lindex < SubsecLights[poly->subsectorIndex]->Size(); lindex++)
   {
      light = static_cast<APointLight *>(SubsecLights[poly->subsectorIndex]->Item(lindex));

      lx = light->x;
      ly = light->y;
      lz = light->z;

      if (light->IsOwned())
      {
         // was just saved, so ignore the light
         if (light->target == NULL) continue;
         a = FIX2FLT(light->target->alpha);
      }
      else
      {
         a = 1.f;
      }

      x = -light->x * MAP_SCALE;
      y = light->z * MAP_SCALE;
      z = light->y * MAP_SCALE;
      pos.Set(x, y, z);
      frac = 1.f;

      // get dist from light origin to poly plane
      dist = fabsf(poly->plane.DistToPoint(x, y, z));
      radius = light->GetRadius() * MAP_COEFF;
      scale = 1.0f / ((2.f * radius) - dist);

      if (radius <= 0.f) continue;
      if (dist > radius) continue;

      // project light position onto plane (find closest point on plane)
      fn.GetRightUp(right, up);
      nearPt = pos.ProjectPlane(right, up);

      // check if nearPt is contained within the owner subsector bbox
      if (!GL_PointNearPoly(nearPt, poly, radius)) continue;

      // don't light the poly if the light is behind it
      if (gl_lights_checkside && !poly->plane.PointOnSide(x, y, z))
      {
         // if the light is close to the plane, allow a bit of light (1/4 of the actual intensity)
         fracRad = radius / (4.f * MAP_COEFF);
         if (fracRad < 1.f) fracRad = 1.f;
         if (dist < fracRad)
         {
            frac = 1.f - (dist / fracRad);
         }
         else
         {
            continue;
         }
      }

      cs = 1.0f - (dist / radius);
      cs *= frac;
      r = byte2float[light->GetRed()] * cs * a;
      g = byte2float[light->GetGreen()] * cs * a;
      b = byte2float[light->GetBlue()] * cs * a;

      // draw light
      if (light->IsSubtractive())
      {
         // ignore subtractive lights if subtractive blending not supported
         if (!glBlendEquationEXT) continue;

         Vector v;

         glBlendEquationEXT(GL_FUNC_REVERSE_SUBTRACT);
         v.Set(r, g, b);
         r = v.Length() - r;
         g = v.Length() - g;
         b = v.Length() - b;
      }
      else
      {
         if (glBlendEquationEXT) glBlendEquationEXT(GL_FUNC_ADD);
      }

      glColor3f(r, g, b);
      glBegin(GL_TRIANGLE_FAN);
         for (int pi = 0; pi < poly->numPts; pi++)
         {
            t1.Set(poly->vertices + (pi * 3));
            nearToVert = t1 - nearPt;
            u = (nearToVert.Dot(right) * scale) + 0.5f;
            v = (nearToVert.Dot(up) * scale) + 0.5f;
            glTexCoord2f(u, v);
            glVertex3f(t1.X(), t1.Y(), t1.Z());
         }
      glEnd();
   }

   if (glBlendEquationEXT) glBlendEquationEXT(GL_FUNC_ADD);
}


void GL_RenderPolyMask(gl_poly_t *poly)
{
   GL_SetupForPoly(poly);

   glColor3f(0.f, 0.f, 0.f);
   glDrawArrays(GL_TRIANGLE_FAN, poly->arrayIndex, poly->numPts);

   GL_FinishPoly(poly);
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


void RL_FinishMode(int mode)
{
   switch (mode)
   {
   case RL_BASE:
      glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
      glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_EXT, GL_TEXTURE);
      glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_EXT, GL_PREVIOUS_EXT);
      glMatrixMode(GL_TEXTURE);
      glLoadIdentity();
      glMatrixMode(GL_MODELVIEW);
      glDisable(GL_FOG);
     break;
   case RL_DEFERRED:
      glDisable(GL_FOG);
     break;
   case RL_FOG:
   case RL_FOG_BOUNDARY:
      glDisable(GL_FOG);
     break;
   default:
     break;
   }
}


FShader *RL_PrepForPoly(int mode, gl_poly_t *poly)
{
   FShader *shader;

   shader = (poly->doomTex >= 0 && !MaskSkybox) ? GL_ShaderForTexture(TexMan[poly->doomTex]) : NULL;

   switch (mode)
   {
   case RL_DEPTH:
   case RL_DEFERRED_DEPTH:
      textureList.SetTranslation(poly->translation);
      if (shader)
      {
         if (shader->layers[0]->animate)
         {
            textureList.BindTexture(TexMan(shader->layers[0]->name));
         }
         else
         {
            textureList.BindTexture(TexMan[shader->layers[0]->name]);
         }
      }
      else
      {
         textureList.BindTexture(poly->doomTex, true);
      }
      textureList.SetTranslation((byte *)NULL);
      if (mode == RL_DEPTH)
      {
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
      }
      else
      {
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      }
      break;
   case RL_BASE:
      if (shader)
      {
         if (shader->layers[0]->animate)
         {
            textureList.BindTexture(TexMan(shader->layers[0]->name));
         }
         else
         {
            textureList.BindTexture(TexMan[shader->layers[0]->name]);
         }
      }
      else
      {
         textureList.BindTexture(poly->doomTex, true);
      }
      break;
   case RL_TEXTURED_CLAMPED:
   case RL_TEXTURED:
      textureList.SetTranslation(poly->translation);
      if (shader)
      {
         textureList.BindTexture(shader->name);
      }
      else
      {
         textureList.BindTexture(poly->doomTex, true);
      }
      textureList.SetTranslation((byte *)NULL);
      if (mode == RL_TEXTURED)
      {
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
      }
      else
      {
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      }
      break;
   case RL_LIGHTS:
      shader = NULL;
      // no mipmapping for the light texture (or it smears with the MIN filter)
      textureList.AllowMipMap(false);
      textureList.BindTexture("GLLIGHT");
      textureList.AllowMipMap(true);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      break;
   case RL_FOG:
   case RL_FOG_BOUNDARY:
      break;
   case RL_MASK:
      break;
   case RL_DEFERRED:
      textureList.SetTranslation(poly->translation);
      if (shader)
      {
         textureList.BindTexture(shader->name);
      }
      else
      {
         textureList.BindTexture(poly->doomTex, true);
      }
      textureList.SetTranslation((byte *)NULL);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      break;
   default:
      break;
   }

   return shader;
}


void RL_RenderList(int mode)
{
   RList *rl;
   gl_poly_t *poly;
   FShader *shader;
   FShaderLayer *layer;
   int numLists, numPolys;

   RL_SetupMode(mode);

   numLists = renderList.Size();

   for (int i = 0; i < numLists; i++)
   {
      rl = &renderList[i];
      numPolys = rl->numPolys;
      if (numPolys)
      {
         poly = &gl_polys[rl->polys[0]];
         shader = RL_PrepForPoly(mode, poly);
         if (shader)
         {
            layer = shader->layers[0];
         }
         else
         {
            layer = NULL;
         }

         for (int j = 0; j < numPolys; j++)
         {
            poly = &gl_polys[renderList[i].polys[j]];

            if (poly->initialized)
            {
               switch (mode)
               {
               case RL_DEPTH:
               case RL_DEFERRED_DEPTH:
                  GL_RenderPolyDepth(poly, layer);
                  break;
               case RL_BASE:
                  GL_RenderPolyBase(poly);
                  break;
               case RL_TEXTURED:
               case RL_TEXTURED_CLAMPED:
                  GL_RenderPolyWithShader(poly, shader, false);
                  break;
               case RL_LIGHTS:
                  GL_RenderPolyLit(poly);
                  break;
               case RL_FOG:
                  GL_RenderPolyFogged(poly);
                  break;
               case RL_FOG_BOUNDARY:
                  GL_RenderPolyFogBoundary(poly);
                  break;
               case RL_MASK:
                  GL_RenderPolyMask(poly);
                  break;
               case RL_DEFERRED:
                  GL_RenderPolyWithShader(poly, shader, true);
                  break;
               default:
                  break;
               }
            }
         }
      }
   }

   glMatrixMode(GL_TEXTURE);
   glLoadIdentity();
   glMatrixMode(GL_MODELVIEW);

   textureList.SetTranslation((byte *)NULL);

   RL_FinishMode(mode);
}


// this renders a single polygon
void RL_RenderPoly(int mode, gl_poly_t *poly)
{
   FShader *shader;
   FShaderLayer *layer;

   RL_SetupMode(mode);
   shader = RL_PrepForPoly(mode, poly);
   if (shader)
   {
      layer = shader->layers[0];
   }
   else
   {
      layer = NULL;
   }

   if (poly->initialized)
   {
      switch (mode)
      {
      case RL_DEPTH:
      case RL_DEFERRED_DEPTH:
         GL_RenderPolyDepth(poly, layer);
         break;
      case RL_BASE:
         GL_RenderPolyBase(poly);
         break;
      case RL_TEXTURED:
      case RL_TEXTURED_CLAMPED:
         if (!gl_wireframe && gl_texture) GL_RenderPolyWithShader(poly, shader, false);
         break;
      case RL_LIGHTS:
         if (!gl_wireframe) GL_RenderPolyLit(poly);
         break;
      case RL_FOG:
         if (!gl_wireframe) GL_RenderPolyFogged(poly);
         break;
      case RL_FOG_BOUNDARY:
         if (!gl_wireframe) GL_RenderPolyFogBoundary(poly);
         break;
      case RL_MASK:
         GL_RenderPolyMask(poly);
         break;
      case RL_DEFERRED:
         if (!gl_wireframe && gl_texture) GL_RenderPolyWithShader(poly, shader, true);
         break;
      default:
         break;
      }
   }

   RL_FinishMode(mode);
}
