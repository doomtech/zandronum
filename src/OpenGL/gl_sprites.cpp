/*
** gl_sprites.cpp
**
** Functions for rendering sprites.
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
#include <list>
#define USE_WINDOWS_DWORD
#include "OpenGLVideo.h"

#include "g_level.h"
#include "w_wad.h"
#include "gl_main.h"

typedef enum
{
   TYPE_ACTOR,
   TYPE_SEG,
   NUM_TYPES,
} DEFERREDTYPE;

typedef struct
{
   float x, y, z;
} VERTEX3D;


using std::list;


#define DEG2RAD( a ) ((a * (float)M_PI) / 180.f)
#define MIN(a, b) ((a < b) ? a : b)
#define MAX(a, b) ((a > b) ? a : b)


CVAR (Int, gl_billboard_mode, 2, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR (Bool, gl_sprite_clip_to_floor, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR (Bool, gl_sprite_sharp_edges, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
EXTERN_CVAR (Bool, gl_depthfog)
EXTERN_CVAR (Float, transsouls)

extern player_t *Player;
extern TextureList textureList;
extern GLdouble viewMatrix[16];
extern int playerlight;
extern int viewpitch;
extern int numTris;
extern bool DrawingDeferredLines;
extern bool InMirror;
extern bool CollectSpecials;


class SPRITELIST
{
public:
   char type;
   union {
      AActor *actor;
      seg_t *seg;
   } object;
   fixed_t dist;
   bool operator== (const SPRITELIST &other)
   {
      if (type == other.type && dist == other.dist)
      {
         switch (type)
         {
         case TYPE_ACTOR:
            return object.actor == other.object.actor;
         case TYPE_SEG:
            return object.seg == other.object.seg;
         default:
            return false;
         }
      }
      return false;
   }
};

TArray<SPRITELIST> visibleSprites;
VERTEX3D rightVec, upVec;


VERTEX3D GL_VectorRotateX(VERTEX3D vec, float angle)
{
   //vec.y += sinf(angle);
   //vec.z += cosf(angle);

   return vec;
}


VERTEX3D GL_VectorRotateY(VERTEX3D vec, float angle)
{
   //vec.x += sinf(angle);
   //vec.z += cosf(angle);

   return vec;
}


VERTEX3D GL_VectorMult(VERTEX3D vec, float scalar)
{
   vec.x *= scalar;
   vec.y *= scalar;
   vec.z *= scalar;

   return vec;
}


VERTEX3D GL_VectorAdd(VERTEX3D vec1, VERTEX3D vec2)
{
   vec1.x += vec2.x;
   vec1.y += vec2.y;
   vec1.z += vec2.z;

   return vec1;
}


void GL_CalculateCameraVectors()
{
   float yaw, pitch;

   yaw = (270.f - ANGLE_TO_FLOAT(viewangle)) * M_PI / 180.f;
   pitch = ANGLE_TO_FLOAT(viewpitch) * M_PI / 180.f;

   rightVec.x = 1;
   rightVec.y = 0;
   rightVec.z = 0;

   upVec.x = 0;
   upVec.y = 1;
   upVec.z = 0;

   rightVec = GL_VectorRotateX(rightVec, pitch);
   rightVec = GL_VectorRotateY(rightVec, yaw);

   upVec = GL_VectorRotateX(rightVec, pitch);
   upVec = GL_VectorRotateY(rightVec, yaw);
}


void GL_DrawBillboard(AActor *thing, float x, float y, float z, float w, float h, float offX, float offZ, float xMult, int lump)
{
   float tx, ty;
   sector_t *sec = thing->Sector;
   fixed_t btmFix;
   float btm, diff, a;

   btmFix = sec->floorplane.ZatPoint(thing->x, thing->y);
   if (sec->heightsec)
   {
      btmFix = MIN(btmFix, sec->heightsec->floorplane.ZatPoint(thing->x, thing->y));
   }
   btm = btmFix * MAP_SCALE;

   textureList.GetCorner(&tx, &ty);
   a = 360.f - (270.f - (viewangle>>ANGLETOFINESHIFT) * 360.f / FINEANGLES);

   glPushMatrix();

   x += offX;
   z += offZ;

   diff = y - btm;
   if (diff >= 0 || !gl_sprite_clip_to_floor) diff = 0;

   glTranslated(x, y - diff, z);
   glRotatef(a, 0.f, 1.f, 0.f);

   if (xMult < 0.0)
   {
      glBegin(GL_TRIANGLE_FAN);
         glTexCoord2f(tx, ty);
         glVertex3f(-w, 0.f, 0.f);
         glTexCoord2f(0.f, ty);
         glVertex3f(w, 0.f, 0.f);
         glTexCoord2f(0.f, 0.f);
         glVertex3f(w, h, 0.f);
         glTexCoord2f(tx, 0.f);
         glVertex3f(-w, h, 0.f);
      glEnd();
   }
   else
   {
      glBegin(GL_TRIANGLE_FAN);
         glTexCoord2f(0.f, ty);
         glVertex3f(-w, 0.f, 0.f);
         glTexCoord2f(tx, ty);
         glVertex3f(w, 0.f, 0.f);
         glTexCoord2f(tx, 0.f);
         glVertex3f(w, h, 0.f);
         glTexCoord2f(0.f, 0.f);
         glVertex3f(-w, h, 0.f);
      glEnd();
   }

   numTris += 2;

   glPopMatrix();
}


void GL_DrawBillboard2(AActor *thing, float x, float y, float z, float w, float h, float offX, float offZ, float xMult, int lump)
{
   float rx, ry, rz, ux, uy, uz;
   float cx, cy, cz;
   float tx, ty;
   sector_t *sec = thing->Sector;
   float btm, diff;
   fixed_t btmFix;

   btmFix = sec->floorplane.ZatPoint(thing->x, thing->y);
   if (sec->heightsec)
   {
      btmFix = MIN(btmFix, sec->heightsec->floorplane.ZatPoint(thing->x, thing->y));
   }
   btm = btmFix * MAP_SCALE;

   x += offX;
   z += offZ;

   textureList.GetCorner(&tx, &ty);

   h = h / 2;
   y += h;

   rx = viewMatrix[0]; ry = viewMatrix[4]; rz = viewMatrix[8];
   ux = viewMatrix[1]; uy = viewMatrix[5]; uz = viewMatrix[9];

   cy = y + (uy * -h) + (ry * w);
   diff = cy - btm;
   if (diff >= 0 || !gl_sprite_clip_to_floor) diff = 0;

   if (InMirror)
   {
      xMult *= -1;
   }

   glCullFace(GL_BACK);

   if (xMult < 0.0)
   {
      glBegin(GL_TRIANGLE_FAN);
         cx = x + (ux * -h) + (rx * -w);
         cy = y + (uy * -h) + (ry * -w);
         cz = z + (uz * -h) + (rz * -w);
         glTexCoord2f(tx, ty);
         glVertex3f(cx, cy - diff, cz);

         cx = x + (ux * -h) + (rx * w);
         cy = y + (uy * -h) + (ry * w);
         cz = z + (uz * -h) + (rz * w);
         glTexCoord2f(0.f, ty);
         glVertex3f(cx, cy - diff, cz);

         cx = x + (ux * h) + (rx * w);
         cy = y + (uy * h) + (ry * w);
         cz = z + (uz * h) + (rz * w);
         glTexCoord2f(0.f, 0.f);
         glVertex3f(cx, cy - diff, cz);

         cx = x + (ux * h) + (rx * -w);
         cy = y + (uy * h) + (ry * -w);
         cz = z + (uz * h) + (rz * -w);
         glTexCoord2f(tx, 0.f);
         glVertex3f(cx, cy - diff, cz);
      glEnd();
   }
   else
   {
      glBegin(GL_TRIANGLE_FAN);
         cx = x + (ux * -h) + (rx * -w);
         cy = y + (uy * -h) + (ry * -w);
         cz = z + (uz * -h) + (rz * -w);
         glTexCoord2f(0.f, ty);
         glVertex3f(cx, cy - diff, cz);

         cx = x + (ux * -h) + (rx * w);
         cy = y + (uy * -h) + (ry * w);
         cz = z + (uz * -h) + (rz * w);
         glTexCoord2f(tx, ty);
         glVertex3f(cx, cy - diff, cz);

         cx = x + (ux * h) + (rx * w);
         cy = y + (uy * h) + (ry * w);
         cz = z + (uz * h) + (rz * w);
         glTexCoord2f(tx, 0.f);
         glVertex3f(cx, cy - diff, cz);

         cx = x + (ux * h) + (rx * -w);
         cy = y + (uy * h) + (ry * -w);
         cz = z + (uz * h) + (rz * -w);
         glTexCoord2f(0.f, 0.f);
         glVertex3f(cx, cy - diff, cz);
      glEnd();
   }

   numTris += 2;

   if (InMirror)
   {
      glCullFace(GL_FRONT);
   }
}


void GL_DrawThing(player_t *player, AActor *thing, float offX, float offZ)
{
   sector_t *sector, tempsec;
   spritedef_t *sprdef;
   spriteframe_t *sprframe;
   PalEntry *p;
   int frameIndex, width, height;
   float x, y, z, ca, xMult, color, w, h, ow, oh;
   fixed_t fx, fy, fz;
   FTexture *tex;
   int picnum;

   if ((thing->renderflags & RF_INVISIBLE) || thing->RenderStyle == STYLE_None || (thing->RenderStyle >= STYLE_Translucent && thing->alpha == 0))
	{
      return;
	}

   sector = R_FakeFlat(thing->floorsector, &tempsec, NULL, NULL, false);

   if (thing->renderflags & RF_FULLBRIGHT || Player->fixedcolormap)
   {
      if (!sector->ColorMap->Fade)
      {
         glDisable(GL_FOG);
      }
      color = 1.f;
   }
   else
   {
      if (gl_depthfog && !Player->fixedcolormap)
      {
         glEnable(GL_FOG);
      }

      color = (float)(sector->lightlevel + (playerlight << 4)) / LIGHTLEVELMAX;
      //color = color * 0.95f;
   }

   if (sector->ColorMap->Fade)
   {
      GL_SetupFog(sector->lightlevel, sector->ColorMap->Fade.r, sector->ColorMap->Fade.g, sector->ColorMap->Fade.b);
   }
   else
   {
      GL_SetupFog(sector->lightlevel, ((level.fadeto & 0xFF0000) >> 16), ((level.fadeto & 0x00FF00) >> 8), level.fadeto & 0x0000FF);
   }

   p = &sector->ColorMap->Color;

   if (thing->flags & MF_SHADOW)
   {
      ca = 0.2f;
   }
   else
   {
      ca = thing->alpha / 65535.f;
   }

   switch (thing->RenderStyle)
   {
   case STYLE_Normal:
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      ca = 1.f;
      break;

   case STYLE_Shaded:
   case STYLE_Translucent:
   case STYLE_OptFuzzy:
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      break;

   case STYLE_SoulTrans:
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      ca = transsouls;
      break;

   case STYLE_Add:
      glBlendFunc(GL_SRC_ALPHA, GL_ONE);
      break;

   default:
      break;
   }

   // [RH] Interpolate the sprite's position to make it look smooth
	fx = thing->PrevX + FixedMul (r_TicFrac, thing->x - thing->PrevX);
	fy = thing->PrevY + FixedMul (r_TicFrac, thing->y - thing->PrevY);
	fz = thing->PrevZ + FixedMul (r_TicFrac, thing->z - thing->PrevZ);

   x = fx * MAP_SCALE;
   y = fz * MAP_SCALE;
   z = fy * MAP_SCALE;

   sprdef = &sprites[thing->sprite];
   frameIndex = thing->frame;
   sprframe = &SpriteFrames[sprdef->spriteframes + frameIndex];
   xMult = 1.f;

   if (thing->picnum != 0xFFFF)
	{
		picnum = thing->picnum;

		tex = TexMan(picnum);
		if (tex->UseType == FTexture::TEX_Null)
		{
			return;
		}
		xMult = 1.f;

		if (thing->picnum != 0xFFFF)
		{
			if (tex->Rotations != 0xFFFF)
			{
				// choose a different rotation based on player view
				spriteframe_t *sprframe = &SpriteFrames[tex->Rotations];
				angle_t ang = R_PointToAngle (fx, fy);
				angle_t rot = (ang - thing->angle + (angle_t)(ANGLE_45/2)*9) >> 28;
				picnum = sprframe->Texture[rot];
				if (sprframe->Flip & (1 << rot)) xMult = -1.f;
				tex = TexMan[picnum];	// Do not animate the rotation
			}
		}
	}
	else
	{
		// decide which texture to use for the sprite
#ifdef RANGECHECK
		if ((unsigned)thing->sprite >= (unsigned)sprites.Size ())
		{
			DPrintf ("R_ProjectSprite: invalid sprite number %i\n", thing->sprite);
			return;
		}
#endif
		spritedef_t *sprdef = &sprites[thing->sprite];
		if (thing->frame >= sprdef->numframes)
		{
			// If there are no frames at all for this sprite, don't draw it.
			return;
		}
		else
		{
			//picnum = SpriteFrames[sprdef->spriteframes + thing->frame].Texture[0];
			// choose a different rotation based on player view
			spriteframe_t *sprframe = &SpriteFrames[sprdef->spriteframes + thing->frame];
			angle_t ang = R_PointToAngle (fx, fy);
			angle_t rot = (ang - thing->angle + (angle_t)(ANGLE_45/2)*9) >> 28;
			picnum = sprframe->Texture[rot];
			if (sprframe->Flip & (1 << rot)) xMult = -1.f;
			tex = TexMan[picnum];	// Do not animate the rotation
		}
	}

   if (thing->Translation)
   {
      textureList.SetTranslation(thing->Translation);
   }
   else
   {
      // if using a sector color, don't use the colormap
      if (!sector->ColorMap->Fade)
      {
         textureList.SetTranslation(thing->Sector->ColorMap->Maps);
      }
   }
   textureList.AllowScale(false);
   textureList.BindTexture(tex);
   textureList.AllowScale(true);
   textureList.SetTranslation((byte *)NULL);

   width = tex->GetWidth();
   height = tex->GetHeight();

   ow = (width / 2.f) / MAP_COEFF;
   oh = height / MAP_COEFF;

   w = ow * (thing->xscale / 63.f);
   h = oh * (thing->yscale / 63.f);
   y -= h;

   y += (tex->TopOffset * (thing->yscale / 63.f)) / MAP_COEFF;
   offX = (offX * (tex->LeftOffset / MAP_COEFF * (thing->xscale / 63.f))) - (offX * w);
   offZ = (offZ * (tex->LeftOffset / MAP_COEFF * (thing->xscale / 63.f))) - (offZ * w);

   glColor4f(color * byte2float[p->r], color * byte2float[p->g], color * byte2float[p->b], ca);

   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

   switch (gl_billboard_mode)
   {
   case 3:
   case 2:
      GL_DrawBillboard2(thing, -x, y, z, w , h, offX, offZ, xMult, picnum);
      break;
   default:
      GL_DrawBillboard(thing, -x, y, z, w, h, offX, offZ, xMult, picnum);
      break;
   }

   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

   if (gl_depthfog && !Player->fixedcolormap)
   {
      glEnable(GL_FOG);
   }

   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}


void GL_ClearSprites()
{
   visibleSprites.Clear();
}


extern bool InMirror;

void GL_AddSeg(seg_t *seg)
{
   SPRITELIST sprite;
   fixed_t dist;
   vertex_t vert;

   if (DrawingDeferredLines || CollectSpecials)
   {
      return;
   }

   vert.x = (seg->v1->x + seg->v2->x) / 2;
   vert.y = (seg->v1->y + seg->v2->y) / 2;

   if ((vert.x == viewx) && (vert.y == viewy))
   {
      dist = 0;
   }
   else
   {
      dist = MIN(R_PointToDist2(seg->v1->x, seg->v1->y), R_PointToDist2(seg->v2->x, seg->v2->y));
   }

   sprite.object.seg = seg;
   sprite.type = TYPE_SEG;
   sprite.dist = dist;
   visibleSprites.Push(sprite);
}


void GL_AddSprite(AActor *actor)
{
   SPRITELIST sprite;
   fixed_t dist;
   bool drawChase;

   if (DrawingDeferredLines || CollectSpecials)
   {
      return;
   }

   drawChase = (Player->cheats & CF_CHASECAM) || (Player->health <= 0);

   if (actor->player && camera->player && !drawChase && !InMirror)
   {
      if (actor->player == camera->player)
      {
         return;
      }
   }

   if ((actor->x == viewx) && (actor->y == viewy))
   {
      dist = 0;
   }
   else
   {
      dist = R_PointToDist2(actor->x, actor->y);
   }

   sprite.object.actor = actor;
   sprite.type = TYPE_ACTOR;
   sprite.dist = dist;
   visibleSprites.Push(sprite);
}


void GL_spriteQsort(SPRITELIST *left, SPRITELIST *right)
{
	SPRITELIST *leftbound, *rightbound, pivot, temp;

   pivot = left[(right - left) / (2 * sizeof(SPRITELIST))];
   leftbound = left;
   rightbound = right;
   for (;;)
   {
      while (leftbound->dist < pivot.dist)
      {
         leftbound++;
      }
      while (rightbound->dist > pivot.dist)
      {
         rightbound--;
      }
      while (!(leftbound == rightbound) && (leftbound->dist == rightbound->dist))
      {
         leftbound++;
      }
      if (leftbound == rightbound)
      {
         break;
      }
      temp = *leftbound;
      *leftbound = *rightbound;
      *rightbound = temp;
   }
   if (left < --leftbound)
   {
      GL_spriteQsort(left, leftbound);
   }
   if (right > ++rightbound)
   {
      GL_spriteQsort(rightbound, right);
   }
}


void GL_DrawSprites()
{
   sector_t tempSec, *sec;
   AActor *thing;
   SPRITELIST *node, *sprites;
   int numSprites, i;
   float lastAlpha = 0.f, alpha, offX, offZ, ang;

   if (!DrawingDeferredLines)
   {
      return;
   }

   numSprites = visibleSprites.Size();
   if (numSprites == 0) return;

   sprites = new SPRITELIST[numSprites];
   for (i = 0; i < numSprites; i++)
   {
      sprites[i] = visibleSprites[i];
   }
   GL_spriteQsort(sprites, sprites + numSprites - 1);

   glEnable(GL_ALPHA_TEST);

   ang = 90.f - ANGLE_TO_FLOAT(viewangle);
   offX = cosf(DEG2RAD(ang));
   offZ = sinf(DEG2RAD(ang));

   for (i = numSprites - 1; i >= 0; i--)
   {
      node = sprites + i;
      switch (node->type)
      {
      case TYPE_ACTOR:
         thing = node->object.actor;
         alpha = MAX((thing->alpha / 65535.f) - 0.5f, 0.f);
         if (alpha != lastAlpha && gl_sprite_sharp_edges)
         {
            glAlphaFunc(GL_GREATER, alpha);
            lastAlpha = alpha;
         }
         GL_DrawThing(Player, thing, offX, offZ);
         break;
      case TYPE_SEG:
         sec = R_FakeFlat(node->object.seg->frontsector, &tempSec, NULL, NULL, false);
         alpha = MAX(byte2float[node->object.seg->linedef->alpha] - 0.5f, 0.f);
         if (alpha != lastAlpha && gl_sprite_sharp_edges)
         {
            glAlphaFunc(GL_GREATER, alpha);
            lastAlpha = alpha;
         }
         GL_DrawWall(node->object.seg, sec, false, false);
         break;
      }
   }

   delete[] sprites;

   if (gl_sprite_sharp_edges) glAlphaFunc(GL_GREATER, 0.0);
   glDisable(GL_ALPHA_TEST);
}
