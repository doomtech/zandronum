/*
** gl_clipper.cpp
**
** Handles visibility checks and frustrum culling.
** Loosely based on the JDoom clipper.
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
#include "p_lnspec.h"
#include "r_defs.h"
#include "r_sky.h"
#include "r_state.h"
#include "m_bbox.h"
#include "templates.h"


class clipnode_t
{
public:
   clipnode_t *prev, *next;
   angle_t start, end;
   clipnode_t()
   {
      start = end = 0;
      next = prev = NULL;
   }
   clipnode_t(const clipnode_t &other)
   {
      start = other.start;
      end = other.end;
      next = other.next;
      prev = other.prev;
   }
   ~clipnode_t()
   {
      if (next) delete next;
   }
   bool operator== (const clipnode_t &other)
   {
      return other.start == start && other.end == end;
   }
   bool contains(angle_t startAngle, angle_t endAngle)
   {
      return (start <= startAngle && end >= endAngle);
   }
};

extern TextureList textureList;
extern seg_t *segs;
extern int skyflatnum;
extern bool DrawingDeferredLines;
extern GLdouble viewMatrix[16];
extern GLdouble projMatrix[16];

 clipnode_t	*clipnodes;		// The list of clipnodes.
 clipnode_t	*cliphead;		// The head node.

float frustum[6][4];

EXTERN_CVAR (Bool, gl_nobsp)


void CL_Init()
{
   cliphead = NULL;
   clipnodes = NULL;
}


void CL_Shutdown()
{
   CL_ClearClipper();
}


void CL_CalcFrustumPlanes()
{
   float clip[16];

   glGetDoublev(GL_PROJECTION_MATRIX, projMatrix);
   glGetDoublev(GL_MODELVIEW_MATRIX, viewMatrix);

   clip[ 0] = (float)(viewMatrix[ 0] * projMatrix[ 0] + viewMatrix[ 1] * projMatrix[ 4] + viewMatrix[ 2] * projMatrix[ 8] + viewMatrix[ 3] * projMatrix[12]);
   clip[ 1] = (float)(viewMatrix[ 0] * projMatrix[ 1] + viewMatrix[ 1] * projMatrix[ 5] + viewMatrix[ 2] * projMatrix[ 9] + viewMatrix[ 3] * projMatrix[13]);
   clip[ 2] = (float)(viewMatrix[ 0] * projMatrix[ 2] + viewMatrix[ 1] * projMatrix[ 6] + viewMatrix[ 2] * projMatrix[10] + viewMatrix[ 3] * projMatrix[14]);
   clip[ 3] = (float)(viewMatrix[ 0] * projMatrix[ 3] + viewMatrix[ 1] * projMatrix[ 7] + viewMatrix[ 2] * projMatrix[11] + viewMatrix[ 3] * projMatrix[15]);

   clip[ 4] = (float)(viewMatrix[ 4] * projMatrix[ 0] + viewMatrix[ 5] * projMatrix[ 4] + viewMatrix[ 6] * projMatrix[ 8] + viewMatrix[ 7] * projMatrix[12]);
   clip[ 5] = (float)(viewMatrix[ 4] * projMatrix[ 1] + viewMatrix[ 5] * projMatrix[ 5] + viewMatrix[ 6] * projMatrix[ 9] + viewMatrix[ 7] * projMatrix[13]);
   clip[ 6] = (float)(viewMatrix[ 4] * projMatrix[ 2] + viewMatrix[ 5] * projMatrix[ 6] + viewMatrix[ 6] * projMatrix[10] + viewMatrix[ 7] * projMatrix[14]);
   clip[ 7] = (float)(viewMatrix[ 4] * projMatrix[ 3] + viewMatrix[ 5] * projMatrix[ 7] + viewMatrix[ 6] * projMatrix[11] + viewMatrix[ 7] * projMatrix[15]);

   clip[ 8] = (float)(viewMatrix[ 8] * projMatrix[ 0] + viewMatrix[ 9] * projMatrix[ 4] + viewMatrix[10] * projMatrix[ 8] + viewMatrix[11] * projMatrix[12]);
   clip[ 9] = (float)(viewMatrix[ 8] * projMatrix[ 1] + viewMatrix[ 9] * projMatrix[ 5] + viewMatrix[10] * projMatrix[ 9] + viewMatrix[11] * projMatrix[13]);
   clip[10] = (float)(viewMatrix[ 8] * projMatrix[ 2] + viewMatrix[ 9] * projMatrix[ 6] + viewMatrix[10] * projMatrix[10] + viewMatrix[11] * projMatrix[14]);
   clip[11] = (float)(viewMatrix[ 8] * projMatrix[ 3] + viewMatrix[ 9] * projMatrix[ 7] + viewMatrix[10] * projMatrix[11] + viewMatrix[11] * projMatrix[15]);

   clip[12] = (float)(viewMatrix[12] * projMatrix[ 0] + viewMatrix[13] * projMatrix[ 4] + viewMatrix[14] * projMatrix[ 8] + viewMatrix[15] * projMatrix[12]);
   clip[13] = (float)(viewMatrix[12] * projMatrix[ 1] + viewMatrix[13] * projMatrix[ 5] + viewMatrix[14] * projMatrix[ 9] + viewMatrix[15] * projMatrix[13]);
   clip[14] = (float)(viewMatrix[12] * projMatrix[ 2] + viewMatrix[13] * projMatrix[ 6] + viewMatrix[14] * projMatrix[10] + viewMatrix[15] * projMatrix[14]);
   clip[15] = (float)(viewMatrix[12] * projMatrix[ 3] + viewMatrix[13] * projMatrix[ 7] + viewMatrix[14] * projMatrix[11] + viewMatrix[15] * projMatrix[15]);

   /* Extract the numbers for the RIGHT plane */
   frustum[0][0] = clip[ 3] - clip[ 0];
   frustum[0][1] = clip[ 7] - clip[ 4];
   frustum[0][2] = clip[11] - clip[ 8];
   frustum[0][3] = clip[15] - clip[12];

   /* Extract the numbers for the LEFT plane */
   frustum[1][0] = clip[ 3] + clip[ 0];
   frustum[1][1] = clip[ 7] + clip[ 4];
   frustum[1][2] = clip[11] + clip[ 8];
   frustum[1][3] = clip[15] + clip[12];

   /* Extract the BOTTOM plane */
   frustum[2][0] = clip[ 3] + clip[ 1];
   frustum[2][1] = clip[ 7] + clip[ 5];
   frustum[2][2] = clip[11] + clip[ 9];
   frustum[2][3] = clip[15] + clip[13];

   /* Extract the TOP plane */
   frustum[3][0] = clip[ 3] - clip[ 1];
   frustum[3][1] = clip[ 7] - clip[ 5];
   frustum[3][2] = clip[11] - clip[ 9];
   frustum[3][3] = clip[15] - clip[13];

   /* Extract the FAR plane */
   frustum[4][0] = clip[ 3] - clip[ 2];
   frustum[4][1] = clip[ 7] - clip[ 6];
   frustum[4][2] = clip[11] - clip[10];
   frustum[4][3] = clip[15] - clip[14];

   /* Extract the NEAR plane */
   frustum[5][0] = clip[ 3] + clip[ 2];
   frustum[5][1] = clip[ 7] + clip[ 6];
   frustum[5][2] = clip[11] + clip[10];
   frustum[5][3] = clip[15] + clip[14];
}


void CL_ClearClipper()
{
   delete cliphead;
   cliphead = NULL;
}


bool CL_IsRangeVisible(angle_t startAngle, angle_t endAngle)
{
   clipnode_t *ci;
   ci = cliphead;
	
   while (ci != NULL)
   {
      if (ci->contains(startAngle, endAngle))
      {
			return false;
      }

      ci = ci->next;
   }

	return true;
}


clipnode_t *CL_NewRange(angle_t start, angle_t end)
{
   clipnode_t *node;

   node = new clipnode_t;

   node->start = start;
   node->end = end;

   return node;
}


void CL_RemoveRange(clipnode_t *range)
{
   if (range == cliphead)
   {
      cliphead = cliphead->next;
      if (cliphead) cliphead->prev = NULL;
   }
   else
   {
      if (range->prev) range->prev->next = range->next;
      if (range->next) range->next->prev = range->prev;
   }

   // make sure it doesn't delete the rest of the list!
   range->next = range->prev = NULL;

   delete range;
}


void CL_AddClipRange(angle_t start, angle_t end)
{
   clipnode_t *node, *temp, *prevNode;

   if (cliphead)
   {
      //check to see if range contains any old ranges
      node = cliphead;
      while (node != NULL)
      {
         if (node->contains(start, end))
         {
            return;
         }
         else if (start <= node->start && end >= node->end)
         {
            temp = node;
            node = node->next;
            CL_RemoveRange(temp);
         }
         else
         {
            node = node->next;
         }
      }

      //check to see if range overlaps a range (or possibly 2)
      node = cliphead;
      while (node != NULL)
      {
         if (start < node->start && end >= node->start)
         {
            node->start = start;
            return;
         }

         if (start <= node->end && end > node->end)
         {
            // check for possible merger
            if (node->next)
            {
               // merge two nodes
               if (end >= node->next->start)
               {
                  node->end = node->next->end;
                  CL_RemoveRange(node->next);
               }
               else
               {
                  node->end = end;
               }
            }
            else
            {
               node->end = end;
            }

            return;
         }

         node = node->next;
      }

      //just add range
      node = cliphead;
      prevNode = NULL;
      temp = CL_NewRange(start, end);

      while (node != NULL && end > node->start)
      {
         prevNode = node;
         node = node->next;
      }

      if (node == NULL)
      {
         temp->next = NULL;
         temp->prev = prevNode;
         if (prevNode) prevNode->next = temp;
         if (!cliphead) cliphead = temp;
      }
      else
      {
         if (node == cliphead)
         {
            temp->next = cliphead;
            cliphead->prev = temp;
            cliphead = temp;
         }
         else
         {
            temp->next = node;
            temp->prev = prevNode;
            prevNode->next = temp;
            node->prev = temp;
         }
      }
   }
   else
   {
      temp = CL_NewRange(start, end);
      cliphead = temp;
      return;
   }
}


void CL_SafeAddClipRange(angle_t startAngle, angle_t endAngle)
{
   if(startAngle > endAngle)
	{
		// The range has to added in two parts.
		CL_AddClipRange(startAngle, ANGLE_MAX);
		CL_AddClipRange(0, endAngle);
	}
	else
	{
		// Add the range as usual.
		CL_AddClipRange(startAngle, endAngle);
	}
}


void CL_AddSegRange(seg_t *seg)
{
   angle_t startAngle, endAngle;

   if (!seg->linedef) return;

   startAngle = R_PointToAngle(seg->v2->x, seg->v2->y);
   endAngle = R_PointToAngle(seg->v1->x, seg->v1->y);

   CL_SafeAddClipRange(startAngle, endAngle);
}


bool CL_SafeCheckRange(angle_t startAngle, angle_t endAngle)
{
   if(startAngle > endAngle)
	{
		return (CL_IsRangeVisible(startAngle, ANGLE_MAX) || CL_IsRangeVisible(0, endAngle));
	}

	return CL_IsRangeVisible(startAngle, endAngle);
}


bool CL_CheckSegRange(seg_t *seg)
{
   angle_t startAngle, endAngle;

   startAngle = R_PointToAngle(seg->v2->x, seg->v2->y);
   endAngle = R_PointToAngle(seg->v1->x, seg->v1->y);

   return CL_SafeCheckRange(startAngle, endAngle);
}


bool CL_BBoxInFrustum(float bbox[2][3])
{
   int p;

   for (p = 0; p < 6; p++)
   {
      if (frustum[p][0] * (bbox[0][0]) + frustum[p][1] * (bbox[0][1]) + frustum[p][2] * (bbox[0][2]) + frustum[p][3] > 0)
      {
         continue;
      }
      if (frustum[p][0] * (bbox[1][0]) + frustum[p][1] * (bbox[0][1]) + frustum[p][2] * (bbox[0][2]) + frustum[p][3] > 0)
      {
         continue;
      }
      if (frustum[p][0] * (bbox[0][0]) + frustum[p][1] * (bbox[1][1]) + frustum[p][2] * (bbox[0][2]) + frustum[p][3] > 0)
      {
         continue;
      }
      if (frustum[p][0] * (bbox[1][0]) + frustum[p][1] * (bbox[1][1]) + frustum[p][2] * (bbox[0][2]) + frustum[p][3] > 0)
      {
         continue;
      }
      if (frustum[p][0] * (bbox[0][0]) + frustum[p][1] * (bbox[0][1]) + frustum[p][2] * (bbox[1][2]) + frustum[p][3] > 0)
      {
         continue;
      }
      if (frustum[p][0] * (bbox[1][0]) + frustum[p][1] * (bbox[0][1]) + frustum[p][2] * (bbox[1][2]) + frustum[p][3] > 0)
      {
         continue;
      }
      if (frustum[p][0] * (bbox[0][0]) + frustum[p][1] * (bbox[1][1]) + frustum[p][2] * (bbox[1][2]) + frustum[p][3] > 0)
      {
         continue;
      }
      if (frustum[p][0] * (bbox[1][0]) + frustum[p][1] * (bbox[1][1]) + frustum[p][2] * (bbox[1][2]) + frustum[p][3] > 0)
      {
         continue;
      }
      return false;
   }

   return true;
}


int CL_SubsectorInFrustum(subsector_t *ssec, sector_t *sector)
{
   if (CL_BBoxInFrustum(ssec->bbox))
   {
      return 1;
   }
   else
   {
      return 0;
   }
}


bool CL_NodeInFrustum(node_t *bsp)
{
   float bbox[2][3];
   fixed_t x, z;

   // assume very tall geometry here
   bbox[0][1] = -150000.f;
   bbox[1][1] = 150000.f;

   x = MIN<fixed_t>(bsp->bbox[0][BOXLEFT], bsp->bbox[1][BOXLEFT]);
   z = MAX<fixed_t>(bsp->bbox[0][BOXTOP], bsp->bbox[1][BOXTOP]);
   bbox[0][0] = -x * MAP_SCALE;
   bbox[0][2] = z * MAP_SCALE;

   x = MAX<fixed_t>(bsp->bbox[0][BOXRIGHT], bsp->bbox[1][BOXRIGHT]);
   z = MIN<fixed_t>(bsp->bbox[0][BOXBOTTOM], bsp->bbox[1][BOXBOTTOM]);
   bbox[1][0] = -x * MAP_SCALE;
   bbox[1][2] = z * MAP_SCALE;

   return CL_BBoxInFrustum(bbox);
}


bool CL_NodeVisible(node_t *bsp)
{
   angle_t a1, a2;
   fixed_t x1, x2, y1, y2;

   return true;

   if (CL_NodeInFrustum(bsp))
   {
#if 0
      x1 = MIN<fixed_t>(bsp->bbox[0][BOXLEFT], bsp->bbox[1][BOXLEFT]);
      y2 = MIN<fixed_t>(bsp->bbox[0][BOXBOTTOM], bsp->bbox[1][BOXBOTTOM]);
      x2 = MAX<fixed_t>(bsp->bbox[0][BOXRIGHT], bsp->bbox[1][BOXRIGHT]);
      y1 = MAX<fixed_t>(bsp->bbox[0][BOXTOP], bsp->bbox[1][BOXTOP]);

      if (GL_SegFacingDir(x1, y1, x2, y1))
      {
         a1 = R_PointToAngle(x1, y1);
         a2 = R_PointToAngle(x2, y1);
         if (CL_SafeCheckRange(a1, a2))
         {
            return true;
         }
      }

      if (GL_SegFacingDir(x2, y1, x2, y2))
      {
         a1 = R_PointToAngle(x2, y1);
         a2 = R_PointToAngle(x2, y2);
         if (CL_SafeCheckRange(a1, a2))
         {
            return true;
         }
      }

      if (GL_SegFacingDir(x2, y2, x1, y2))
      {
         a1 = R_PointToAngle(x2, y2);
         a2 = R_PointToAngle(x1, y2);
         if (CL_SafeCheckRange(a1, a2))
         {
            return true;
         }
      }

      if (GL_SegFacingDir(x1, y2, x1, y1))
      {

         a1 = R_PointToAngle(x1, y2);
         a2 = R_PointToAngle(x1, y1);
         if (CL_SafeCheckRange(a1, a2))
         {
            return true;
         }
      }
      return false;
#else
      return true;
#endif
   }

   return false;
}


bool CL_CheckBBox(fixed_t *bspcoord)
{
   float bbox[2][3];

   // assume very tall geometry here
   bbox[0][1] = 150000.f;
   bbox[1][1] = -150000.f;

   bbox[0][0] = -bspcoord[BOXLEFT] * MAP_SCALE;
   bbox[0][2] = bspcoord[BOXTOP] * MAP_SCALE;
   bbox[1][0] = -bspcoord[BOXRIGHT] * MAP_SCALE;
   bbox[1][2] = bspcoord[BOXBOTTOM] * MAP_SCALE;

   return CL_BBoxInFrustum(bbox);
}


bool CL_CheckSubsector(subsector_t *ssec, sector_t *sector)
{
   int result = 0;
   unsigned short i;
   unsigned long numLines, firstLine;
   seg_t *seg;
   byte *vis;

   numLines = ssec->numlines;
   firstLine = ssec->firstline;

   // check pvs first!
   if (glpvs && PlayerSubsector)
   {
      vis = glpvs + (((numsubsectors + 7) / 8) * PlayerSubsector->index);
      if (!(vis[ssec->index >> 3] & (1 << (ssec->index & 7))))
      {
         return false;
      }
   }

   // check subsector as normal
   for (i = 0, seg = segs + firstLine; i < numLines; i++, seg++)
   {
      if (GL_SegFacingDir(seg->v1->x, seg->v1->y, seg->v2->x, seg->v2->y) && CL_CheckSegRange(seg))
      {
         return true;
      }
   }

   return false;
}


bool CL_ShouldClipAgainstSeg(seg_t *seg, sector_t *frontSector, sector_t *backSector)
{
   fixed_t x, y;
   //
   // never clip minisegs
   //
   if (!seg->linedef)
   {
      return false;
   }

   //
   // make sure seg is facing viewer
   //
   if (!GL_SegFacingDir(seg->v1->x, seg->v1->y, seg->v2->x, seg->v2->y))
   {
      return false;
   }

   //
   // no backsector == solid wall
   //
   if (!backSector)
   {
      return true;
   }

   //
   // translucent lines shouldn't be clipped against
   //
   if (seg->linedef->alpha != 0xff)
   {
      return false;
   }

   //
   // check to see if the floor/ceiling planes are closed
   //
   if (backSector)
   {
      x = (seg->v1->x + seg->v2->x) >> 1;
      y = (seg->v1->y + seg->v2->y) >> 1;

      if (frontSector->floorplane.ZatPoint(x, y) >= backSector->ceilingplane.ZatPoint(x, y))
      {
         return true;
      }

      if (frontSector->ceilingplane.ZatPoint(x, y) <= backSector->floorplane.ZatPoint(x, y))
      {
         return true;
      }
   }

   //
   // check for hidden paths (like in Doom E1M1)
   //
#if 1
   if (seg->sidedef->midtexture && seg->PartnerSeg && seg->PartnerSeg->sidedef && !seg->PartnerSeg->sidedef->midtexture)
   {
      textureList.GetTexture(seg->sidedef->midtexture, true);
      if (!textureList.IsTransparent() && (seg->linedef->alpha == 255)) // && (seg->linedef->flags & ML_TWOSIDED == 0))
      {
         return true;
      }
   }
#endif

   return false;
}


extern int viewpitch;
extern player_t *Player;
//
// find the angle to the frustum points of the far plane
//
angle_t CL_FrustumAngle(int whichAngle)
{
   angle_t amt;
   float pct, rem, vp;

   vp = clamp<float>(fabsf(ANGLE_TO_FLOAT(viewpitch)) + (Player->FOV / 2), 0.f, 90.f);
   pct = vp / 90.f;
   //rem = (180.f - (Player->FOV)) * pct;
   //amt = (angle_t)((Player->FOV + rem) * ANGLE_1);
   amt = (angle_t)(180.f * pct * ANGLE_1);

   if (whichAngle == FRUSTUM_LEFT)
   {
      return viewangle - amt;
   }
   else
   {
      return viewangle + amt;
   }
}

//
// find the angle to the frustum points of the far plane
//
angle_t CL_FrustumAngle()
{
   angle_t amt;
   float pct, vp;

   vp = clamp<float>(fabsf(ANGLE_TO_FLOAT(viewpitch)) + (Player->FOV / 2), 0.f, 90.f);
   pct = vp / 90.f;
   amt = (angle_t)(180.f * pct * ANGLE_1);

   return amt;
}

bool CL_ClipperFull()
{
#if 0
   angle_t a1, a2;
   a1 = CL_FrustumAngle(FRUSTUM_LEFT);
   a2 = CL_FrustumAngle(FRUSTUM_RIGHT);
   return !CL_SafeCheckRange(a1, a2);
#else
   return (cliphead && cliphead->start == 0 && cliphead->end == ANGLE_MAX);
#endif
}
