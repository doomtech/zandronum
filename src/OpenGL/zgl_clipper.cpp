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
#include "zgl_main.h"
#include "p_lnspec.h"
#include "r_defs.h"
#include "r_sky.h"
#include "r_state.h"
#include "m_bbox.h"
#include "templates.h"


#include <vector>
#include <algorithm>
using namespace std;
using std::vector;


//#define CLIP_USE_PLANE_CLASS


class ClipNode
{
public:
   ClipNode();
   bool Contains(angle_t left, angle_t right);
   bool Contains(angle_t angle);
   bool ContainedBy(angle_t left, angle_t right);
   void Merge(ClipNode &clipNode);
   void Merge(angle_t left, angle_t right);
   void Set(angle_t left, angle_t right);
   angle_t Left() { return m_left; }
   angle_t Right() { return m_right; }
   static bool RangeExists(ClipNode &node);
   static bool RangeEqual(ClipNode &node);
protected:
   angle_t m_left, m_right;
};


extern TextureList textureList;
extern seg_t *segs;
extern int skyflatnum;
extern bool DrawingDeferredLines;
extern GLdouble viewMatrix[16];
extern GLdouble projMatrix[16];
extern GLint viewport[4];

vector<ClipNode> Clipper;
#ifdef CLIP_USE_PLANE_CLASS
 Plane frustum[4];
#else
 float frustum[6][4];
#endif
ClipNode Checker;

EXTERN_CVAR (Bool, gl_nobsp)


ClipNode::ClipNode()
{
   m_left = m_right = 0;
}


bool ClipNode::Contains(angle_t angle)
{
   return (m_left <= angle && m_right >= angle);
}


bool ClipNode::Contains(angle_t left, angle_t right)
{
   return (m_left <= left && m_right >= right);
}


bool ClipNode::ContainedBy(angle_t left, angle_t right)
{
   return (left <= m_left && right >= m_right);
}


void ClipNode::Merge(angle_t left, angle_t right)
{
   m_left = MIN<angle_t>(m_left, left);
   m_left = MAX<angle_t>(m_right, right);
}


void ClipNode::Merge(ClipNode &clipNode)
{
   Merge(clipNode.Left(), clipNode.Right());
}


void ClipNode::Set(angle_t left, angle_t right)
{
   m_left = left;
   m_right = right;
}


bool ClipNode::RangeExists(ClipNode &node)
{
   return Checker.Contains(node.Left(), node.Right());
}


bool ClipNode::RangeEqual(ClipNode &node)
{
   return (Checker.Left() == node.Left() && Checker.Right() == node.Right());
}


void CL_Init()
{
   CL_ClearClipper();
}


void CL_Shutdown()
{
   CL_ClearClipper();
}


void CL_ClearClipper()
{
   Clipper.erase(Clipper.begin(), Clipper.end());
}


void CL_PrintClipper()
{
   vector<ClipNode>::iterator it;

   for (it = Clipper.begin(); it != Clipper.end(); ++it)
   {
      Printf("%.2f %.2f\n", ANGLE_TO_FLOAT(it->Left()), ANGLE_TO_FLOAT(it->Right()));
   }

   Printf("\n");
}


void CL_AddRange(angle_t startAngle, angle_t endAngle)
{
   vector<ClipNode>::iterator it, temp, where;
   ClipNode tempNode;

   tempNode.Set(startAngle, endAngle);

   // check if the new range is already occluded
   for (it = Clipper.begin(); it != Clipper.end(); ++it)
   {
      if (it->Contains(startAngle, endAngle)) return;
   }

   // remove any ranges covered by new range
   Checker.Set(startAngle, endAngle);
   where = std::remove_if(Clipper.begin(), Clipper.end(), ClipNode::RangeExists);
   Clipper.erase(where, Clipper.end());

   // check for overlapping start of new range with end of existing one (and possibly merge into next range as well)
   for (it = Clipper.begin(); it != Clipper.end(); ++it)
   {
      if (it->Contains(startAngle))
      {
         temp = it + 1;
         if (temp != Clipper.end())
         {
            if (temp->Contains(endAngle))
            {
               it->Set(it->Left(), temp->Right());
               Checker.Set(temp->Left(), temp->Right());
               where = std::remove_if(Clipper.begin(), Clipper.end(), ClipNode::RangeEqual);
               Clipper.erase(where, Clipper.end());
               return;
            }
         }

         it->Set(it->Left(), endAngle);
         return;
      }
      else if (it->Contains(endAngle))
      {
         it->Merge(startAngle, endAngle);
         it->Set(startAngle, it->Right());
         return;
      }
      else if (it->Left() > endAngle)
      {
         Clipper.insert(it, tempNode);
         return;
      }
   }

   Clipper.push_back(tempNode);
}


void CL_NormalizePlane(float *p)
{
   float mag;

   mag = 1.f / sqrtf(p[0] * p[0] + p[1] * p[1] + p[2] * p[2]);
   p[0] *= mag;
   p[1] *= mag;
   p[2] *= mag;
   p[3] *= mag;
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

#ifdef CLIP_USE_PLANE_CLASS
   // extract right plane
   frustum[0].Init(clip[3] - clip[0], clip[7] - clip[4], clip[11] - clip[8], clip[15] - clip[12]);
   // extract left plane
   frustum[1].Init(clip[3] + clip[0], clip[7] + clip[4], clip[11] + clip[8], clip[15] + clip[12]);
   // extract top plane
   frustum[2].Init(clip[3] - clip[1], clip[7] - clip[5], clip[11] - clip[9], clip[15] - clip[13]);
   // extract bottom plane
   frustum[3].Init(clip[3] + clip[1], clip[7] + clip[5], clip[11] + clip[9], clip[15] + clip[13]);
   // extract far plane
   frustum[4].Init(clip[3] - clip[2], clip[7] - clip[6], clip[11] - clip[10], clip[15] - clip[14]);
   // extract near plane
   frustum[5].Init(clip[3] + clip[2], clip[7] + clip[6], clip[11] + clip[10], clip[15] + clip[14]);
#else
   /* Extract the numbers for the RIGHT plane */
   frustum[0][0] = clip[ 3] - clip[ 0];
   frustum[0][1] = clip[ 7] - clip[ 4];
   frustum[0][2] = clip[11] - clip[ 8];
   frustum[0][3] = clip[15] - clip[12];

   CL_NormalizePlane(frustum[0]);

   /* Extract the numbers for the LEFT plane */
   frustum[1][0] = clip[ 3] + clip[ 0];
   frustum[1][1] = clip[ 7] + clip[ 4];
   frustum[1][2] = clip[11] + clip[ 8];
   frustum[1][3] = clip[15] + clip[12];

   CL_NormalizePlane(frustum[1]);

   /* Extract the BOTTOM plane */
   frustum[2][0] = clip[ 3] + clip[ 1];
   frustum[2][1] = clip[ 7] + clip[ 5];
   frustum[2][2] = clip[11] + clip[ 9];
   frustum[2][3] = clip[15] + clip[13];

   CL_NormalizePlane(frustum[2]);

   /* Extract the TOP plane */
   frustum[3][0] = clip[ 3] - clip[ 1];
   frustum[3][1] = clip[ 7] - clip[ 5];
   frustum[3][2] = clip[11] - clip[ 9];
   frustum[3][3] = clip[15] - clip[13];

   CL_NormalizePlane(frustum[3]);

   /* Extract the FAR plane */
   frustum[4][0] = clip[ 3] - clip[ 2];
   frustum[4][1] = clip[ 7] - clip[ 6];
   frustum[4][2] = clip[11] - clip[10];
   frustum[4][3] = clip[15] - clip[14];

   CL_NormalizePlane(frustum[4]);

   /* Extract the NEAR plane */
   frustum[5][0] = clip[ 3] + clip[ 2];
   frustum[5][1] = clip[ 7] + clip[ 6];
   frustum[5][2] = clip[11] + clip[10];
   frustum[5][3] = clip[15] + clip[14];

   CL_NormalizePlane(frustum[5]);
#endif
}


bool CL_IsRangeVisible(angle_t startAngle, angle_t endAngle)
{
   vector<ClipNode>::iterator it;

   for (it = Clipper.begin(); it != Clipper.end(); ++it)
   {
      if (it->Contains(startAngle, endAngle)) return false;
   }

	return true;
}


void CL_SafeAddClipRange(angle_t startAngle, angle_t endAngle)
{
   if (endAngle < startAngle)
   {
      CL_AddRange(startAngle, ANGLE_MAX);
      CL_AddRange(0, endAngle);
   }
   else
   {
      CL_AddRange(startAngle, endAngle);
   }
}


void CL_AddSegRange(seg_t *seg)
{
   angle_t a1, a2;

   a2 = R_PointToAngle(seg->v1->x, seg->v1->y);
   a1 = R_PointToAngle(seg->v2->x, seg->v2->y);

   CL_SafeAddClipRange(a1, a2);
}


bool CL_SafeCheckRange(angle_t startAngle, angle_t endAngle)
{
   if (endAngle < startAngle)
   {
      return (CL_IsRangeVisible(startAngle, ANGLE_MAX) || CL_IsRangeVisible(0, endAngle));
   }
   else
   {
      return CL_IsRangeVisible(startAngle, endAngle);
   }
}


bool CL_CheckSegRange(seg_t *seg)
{
   angle_t a1, a2;

   a2 = R_PointToAngle(seg->v1->x, seg->v1->y);
   a1 = R_PointToAngle(seg->v2->x, seg->v2->y);

   return CL_SafeCheckRange(a1, a2);
}


bool CL_BBoxInFrustum(float bbox[2][3])
{
   int p;

   for (p = 0; p < 6; p++)
   {
#ifdef CLIP_USE_PLANE_CLASS
      if (frustum[p].PointOnSide(bbox[0][0], bbox[0][1], bbox[0][2])) continue;
      if (frustum[p].PointOnSide(bbox[1][0], bbox[0][1], bbox[0][2])) continue;
      if (frustum[p].PointOnSide(bbox[0][0], bbox[1][1], bbox[0][2])) continue;
      if (frustum[p].PointOnSide(bbox[1][0], bbox[1][1], bbox[0][2])) continue;
      if (frustum[p].PointOnSide(bbox[0][0], bbox[0][1], bbox[1][2])) continue;
      if (frustum[p].PointOnSide(bbox[1][0], bbox[0][1], bbox[1][2])) continue;
      if (frustum[p].PointOnSide(bbox[0][0], bbox[1][1], bbox[1][2])) continue;
      if (frustum[p].PointOnSide(bbox[1][0], bbox[1][1], bbox[1][2])) continue;
#else
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
#endif
      return false;
   }

   return true;
}


bool CL_SphereInFrustum(float x, float y, float z, float radius)
{
   int p;
   float d;

   for (p = 0; p < 6; p++)
   {
      // d = A*px + B*py + C*pz + D
#ifdef CLIP_USE_PLANE_CLASS
      d = frustum[p].DistToPoint(x, y, z);
#else
      d = (frustum[p][0] * x) + (frustum[p][1] * y) + (frustum[p][2] * z) + frustum[p][3];
#endif
      if (d + radius < 0) return false;
   }

   return true;
}


bool CL_SubsectorInFrustum(subsector_t *ssec, sector_t *sector)
{
   return CL_BBoxInFrustum(ssec->bbox);
}


bool CL_CheckBBox(fixed_t *bspcoord)
{
   float bbox[2][3];

   // assume very tall geometry here
   bbox[0][0] = MIN<float>(-bspcoord[BOXLEFT] * MAP_SCALE, -bspcoord[BOXRIGHT] * MAP_SCALE);
   bbox[0][1] = -150000.f;
   bbox[0][2] = MIN<float>(bspcoord[BOXTOP] * MAP_SCALE, bspcoord[BOXBOTTOM] * MAP_SCALE);
   bbox[1][0] = MAX<float>(-bspcoord[BOXLEFT] * MAP_SCALE, -bspcoord[BOXRIGHT] * MAP_SCALE);
   bbox[1][1] = 150000.f;
   bbox[1][2] = MAX<float>(bspcoord[BOXTOP] * MAP_SCALE, bspcoord[BOXBOTTOM] * MAP_SCALE);

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
   fixed_t bs_floorheight1, bs_floorheight2, bs_ceilingheight1, bs_ceilingheight2;
	fixed_t fs_floorheight1, fs_floorheight2, fs_ceilingheight1, fs_ceilingheight2;

   if (!seg->linedef) return false; // miniseg!
   if (seg->length == 0.f) return false;
   if (!GL_SegFacingDir(seg->v1->x, seg->v1->y, seg->v2->x, seg->v2->y)) return false;

   if (seg->linedef->special == Line_Mirror || seg->linedef->special == Line_Horizon) return true;

   if (!backSector)
   {
      return true;
   }
   else if (!seg->bPolySeg)
   {
      //if (frontSector->floorplane.Tilted() || frontSector->ceilingplane.Tilted())
      if (true)
      {
         fs_floorheight1 = frontSector->floorplane.ZatPoint(seg->v1);
         fs_floorheight2 = frontSector->floorplane.ZatPoint(seg->v2);
         fs_ceilingheight1 = frontSector->ceilingplane.ZatPoint(seg->v1);
         fs_ceilingheight2 = frontSector->ceilingplane.ZatPoint(seg->v2);
      }
      else
      {
         fs_floorheight1 = fs_floorheight2 = frontSector->floortexz;
         fs_ceilingheight1 = fs_ceilingheight2 = frontSector->ceilingtexz;
      }

      //if (backSector->floorplane.Tilted() || backSector->ceilingplane.Tilted())
      if (true)
      {
         bs_floorheight1 = backSector->floorplane.ZatPoint(seg->v1);
         bs_floorheight2 = backSector->floorplane.ZatPoint(seg->v2);
         bs_ceilingheight1 = backSector->ceilingplane.ZatPoint(seg->v1);
         bs_ceilingheight2 = backSector->ceilingplane.ZatPoint(seg->v2);
      }
      else
      {
         bs_floorheight1 = bs_floorheight2 = backSector->floortexz;
         bs_ceilingheight1 = bs_ceilingheight2 = backSector->ceilingtexz;
      }

      // now check for closed sectors!
	   if (bs_ceilingheight1 <= fs_floorheight1 && bs_ceilingheight2 <= fs_floorheight2) 
	   {
		   //FTexture *tex = TexMan(seg->sidedef->toptexture);
		   //if (!tex || tex->UseType == FTexture::TEX_Null) return false;
		   //if (backSector->ceilingpic == skyflatnum && frontSector->ceilingpic == skyflatnum) return false;

		   return true;
	   }

	   if (fs_ceilingheight1 <= bs_floorheight1 && fs_ceilingheight2 <= bs_floorheight2) 
	   {
		   //FTexture *tex = TexMan(seg->sidedef->bottomtexture);
		   //if (!tex || tex->UseType == FTexture::TEX_Null) return false;
		   //if (backSector->ceilingpic == skyflatnum && frontSector->ceilingpic == skyflatnum) return false;

		   return true;
	   }

      if (bs_ceilingheight1<=bs_floorheight1 && bs_ceilingheight2<=bs_floorheight2)
	   {
		   // preserve a kind of transparent door/lift special effect:
		   if (bs_ceilingheight1 < fs_ceilingheight1 || bs_ceilingheight2 < fs_ceilingheight2) 
		   {
			   FTexture *tex = TexMan(seg->sidedef->toptexture);
			   if (!tex || tex->UseType == FTexture::TEX_Null) return false;
		   }
		   if (bs_floorheight1 > fs_floorheight1 || bs_floorheight2 > fs_floorheight2)
		   {
			   FTexture *tex = TexMan(seg->sidedef->bottomtexture);
			   if (!tex || tex->UseType == FTexture::TEX_Null) return false;
		   }

		   if (backSector->ceilingpic == skyflatnum && frontSector->ceilingpic == skyflatnum) return false;
		   if (backSector->floorpic == skyflatnum && frontSector->floorpic == skyflatnum) return false;

		   return true;
	   }
   }

   return false;
}


extern int viewpitch;
extern player_t *Player;
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
   if (Clipper.empty()) return false;
   return Clipper.front().Contains(0, ANGLE_MAX);
}

