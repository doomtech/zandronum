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
#include "templates.h"
#include "w_wad.h"
#include "zgl_main.h"

typedef enum
{
   TYPE_ACTOR,
   TYPE_SEG,
   TYPE_PARTICLE,
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


CVAR (Int, gl_billboard_mode, 1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR (Bool, gl_particles_additive, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR (Bool, gl_particles_grow, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR (Int, gl_particles_style, 2, CVAR_ARCHIVE | CVAR_GLOBALCONFIG) // 0 = square, 1 = round, 2 = smooth
CVAR (Bool, gl_particles_usepoints, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR (Bool, gl_sprite_clip_to_floor, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR (Bool, gl_sprite_sharp_edges, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR (Bool, gl_light_sprites, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR (Bool, gl_lights_halos, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
EXTERN_CVAR (Bool, gl_depthfog)
EXTERN_CVAR (Float, transsouls)
EXTERN_CVAR (Bool, r_drawfuzz)

extern PFNGLPROGRAMLOCALPARAMETER4FARBPROC glProgramLocalParameter4fARB;
extern PFNGLBLENDEQUATIONEXTPROC glBlendEquationEXT;

extern player_t *Player;
extern TextureList textureList;
extern GLdouble viewMatrix[16];
extern int playerlight;
extern int viewpitch;
extern int numTris;
extern bool DrawingDeferredLines;
extern bool InMirror;
extern bool CollectSpecials;
extern TArray<ADynamicLight *> HaloLights;


class SPRITELIST
{
public:
   SPRITELIST() { object.actor = NULL; }
   DEFERREDTYPE type;
   union {
      AActor *actor;
      seg_t *seg;
      Particle *particle;
   } object;
   float dist;
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
         case TYPE_PARTICLE:
            return object.particle == other.object.particle;
         default:
            return false;
         }
      }
      return false;
   }
};

TArray<SPRITELIST> visibleSprites;


void GL_CalculateCameraVectors()
{
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

   diff += FIX2FLT(thing->floorclip);

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

   rx = static_cast<float>(viewMatrix[0]); ry = static_cast<float>(viewMatrix[4]); rz = static_cast<float>(viewMatrix[8]);
   ux = static_cast<float>(viewMatrix[1]); uy = static_cast<float>(viewMatrix[5]); uz = static_cast<float>(viewMatrix[9]);

   cy = y + (uy * -h) + (ry * w);
   diff = cy - btm;
   if (diff >= 0 || !gl_sprite_clip_to_floor) diff = 0;

   diff += FIX2FLT(thing->floorclip);

   if (InMirror)
   {
      xMult *= -1;
   }

   glPushAttrib(GL_POLYGON_BIT);
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

   glPopAttrib();
}


void GL_DrawBillboard3(AActor *thing, float x, float y, float z, float w, float h, float offX, float offZ, float xMult, int lump)
{
}


void GL_DrawThing(player_t *player, AActor *thing, float offX, float offZ)
{
   sector_t *sector, tempsec;
   spritedef_t *sprdef;
   spriteframe_t *sprframe;
   PalEntry *p;
   int frameIndex, width, height, lightLevel;
   float x, y, z, ca, xMult, color, w, h, ow, oh, r, g, b;
   fixed_t fx, fy, fz;
   FTexture *tex;
   int picnum;
   ADynamicLight *light;

   //if (gl_lights_halos)
   if (false)
   {
      for (unsigned int i = 0; i < thing->Lights.Size(); i++)
      {
         light = static_cast<ADynamicLight *>(thing->Lights[i]);
         while (light)
         {
            if (light->halo && light->GetRadius() > 0.f) HaloLights.Push(light);
            light = light->lnext;
         }
      }
   }

   if ((thing->renderflags & RF_INVISIBLE) || thing->RenderStyle == STYLE_None || (thing->RenderStyle >= STYLE_Translucent && thing->alpha == 0))
	{
      return;
	}

   sector = GL_FakeFlat(thing->Sector, &tempsec, NULL, NULL, false);

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
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      break;

   case STYLE_OptFuzzy:
      if (r_drawfuzz)
      {
         //glBlendFunc(GL_SRC_ALPHA_SATURATE, GL_ONE_MINUS_SRC_ALPHA);
         //ca = 0.3f;
         glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
         textureList.LoadAlpha(true);
         if (glBlendEquationEXT)
         {
            glBlendEquationEXT(GL_FUNC_REVERSE_SUBTRACT);
            ca = 0.4f;
         }
         else
         {
            ca = 0.75f;
         }
      }
      else
      {
         glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      }
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
	else
	{
		// decide which texture to use for the sprite
		spritedef_t *sprdef = &sprites[thing->sprite];
		if (thing->frame >= sprdef->numframes)
		{
			// If there are no frames at all for this sprite, don't draw it.
			return;
		}
		else
		{
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

   if ((thing->renderflags & RF_FULLBRIGHT) || Player->fixedcolormap)
   {
      if (!sector->ColorMap->Fade)
      {
         glDisable(GL_FOG);
      }
      lightLevel = (int)LIGHTLEVELMAX;
   }
   else
   {
      if (gl_depthfog && !Player->fixedcolormap)
      {
         glEnable(GL_FOG);
         // setup fog first, before taking dynamic lights into account
         if (sector->ColorMap->Fade)
         {
            GL_SetupFog(sector->lightlevel, sector->ColorMap->Fade.r, sector->ColorMap->Fade.g, sector->ColorMap->Fade.b);
         }
         else
         {
            //GL_SetupFog(sector->lightlevel, ((level.fadeto & 0xFF0000) >> 16), ((level.fadeto & 0x00FF00) >> 8), level.fadeto & 0x0000FF);
            GL_SetupFog(sector->lightlevel, static_cast<BYTE>(RPART(level.fadeto)), static_cast<BYTE>(GPART(level.fadeto)), static_cast<BYTE>(BPART(level.fadeto)));
         }
      }
      lightLevel = sector->lightlevel;
   }

   if (gl_light_sprites) lightLevel += GL_GetIntensityForPoint(-x, y, z);
   lightLevel = clamp<int>(lightLevel, 0, 255);
   color = lightLevel / LIGHTLEVELMAX;

   r = color * byte2float[p->r];
   g = color * byte2float[p->g];
   b = color * byte2float[p->b];
   // thing->height
   if (gl_light_sprites) GL_GetLightForPoint(-x, y, z, &r, &g, &b);
   glColor4f(r, g, b, ca);

   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

   switch (gl_billboard_mode)
   {
   case 3:
      if (static_cast<OpenGLFrameBuffer *>(screen)->SupportsVertexPrograms())
      {
         //GL_DrawBillboard3(thing, -x, y, z, w , h, offX, offZ, xMult, picnum);
         GL_DrawBillboard2(thing, -x, y, z, w , h, offX, offZ, xMult, picnum);
      }
      else
      {
         GL_DrawBillboard2(thing, -x, y, z, w , h, offX, offZ, xMult, picnum);
      }
      break;
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
      //glDisable(GL_FOG);
   }

   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   textureList.LoadAlpha(false);
   if (glBlendEquationEXT) glBlendEquationEXT(GL_FUNC_ADD);
}


void GL_DrawParticle(float x, float y, float z, PalEntry *p, int size)
{
   float rx, ry, rz, ux, uy, uz;
   float cx, cy, cz;
   float tx, ty;
   float w, h;
   float angle;
   float grow;

   grow = 1 - byte2float[p->a];
   grow = (3 * grow) + 1;

   if (gl_particles_style == 0)
	   w = h = size * 0.18f;
   else if (gl_particles_style == 1)
   {
	   if (gl_particles_grow)
		   w = h = (size * 0.1f) * grow;
	   else
		   w = h = size * 0.2f;
   }
   else
   {
	   if (gl_particles_grow)
		   w = h = (size * 0.25f) * grow;
	   else
		   w = h = size * 0.5f;
   }

   rx = static_cast<float>(viewMatrix[0]); ry = static_cast<float>(viewMatrix[4]); rz = static_cast<float>(viewMatrix[8]);
   ux = static_cast<float>(viewMatrix[1]); uy = static_cast<float>(viewMatrix[5]); uz = static_cast<float>(viewMatrix[9]);
   textureList.GetCorner(&tx, &ty);

   angle = byte2float[p->a] * 720.f;

   glColor4f(byte2float[p->r], byte2float[p->g], byte2float[p->b], byte2float[p->a]);

   glPushAttrib(GL_POLYGON_BIT);
   glCullFace(GL_BACK);

   if (gl_particles_usepoints && static_cast<OpenGLFrameBuffer *>(screen)->SupportsPointSprites())
   {
      glEnable(GL_POINT_SPRITE_ARB);
      //glPointSize(size * 256.f);
      glBegin(GL_POINTS);
         glVertex3f(x, y, z);
      glEnd();
      glDisable(GL_POINT_SPRITE_ARB);
   }
   else
   {
      glBegin(GL_TRIANGLE_FAN);
         cx = x + (ux * -h) + (rx * -w);
         cy = y + (uy * -h) + (ry * -w);
         cz = z + (uz * -h) + (rz * -w);
         glTexCoord2f(0.0, ty);
         glVertex3f(cx, cy, cz);

         cx = x + (ux * -h) + (rx * w);
         cy = y + (uy * -h) + (ry * w);
         cz = z + (uz * -h) + (rz * w);
         glTexCoord2f(tx, ty);
         glVertex3f(cx, cy, cz);

         cx = x + (ux * h) + (rx * w);
         cy = y + (uy * h) + (ry * w);
         cz = z + (uz * h) + (rz * w);
         glTexCoord2f(tx, 0.0);
         glVertex3f(cx, cy, cz);

         cx = x + (ux * h) + (rx * -w);
         cy = y + (uy * h) + (ry * -w);
         cz = z + (uz * h) + (rz * -w);
         glTexCoord2f(0.0, 0.0);
         glVertex3f(cx, cy, cz);
      glEnd();
   }

   glPopAttrib();
}


void GL_DrawParticle(Particle *p)
{
   PalEntry pe;

   GL_SetupFog(p->lightLevel, p->fogR, p->fogG, p->fogB);

   if (gl_particles_additive)
   {
      glBlendFunc(GL_SRC_ALPHA, GL_ONE);
   }
   else
   {
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   }

   pe.r = p->r;
   pe.g = p->g;
   pe.b = p->b;
   pe.a = p->a;
   
   if (gl_particles_style == 0)
      textureList.BindTexture("GLPART3");
   if (gl_particles_style == 1)
      textureList.BindTexture("GLPART2");
   if (gl_particles_style == 2)
      textureList.BindTexture("GLPART");

   GL_DrawParticle(p->x, p->y, p->z, &pe, p->size);

   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}


void GL_ClearSprites()
{
   int numSprites = visibleSprites.Size();

   for (int i = numSprites - 1; i >= 0; i--)
   {
      if (visibleSprites[i].type == TYPE_PARTICLE)
      {
         delete visibleSprites[i].object.particle;
      }
   }

   visibleSprites.Clear();
}



extern bool InMirror;

void GL_AddSeg(seg_t *seg)
{
   SPRITELIST sprite;
   fixed_t dist;

   if (DrawingDeferredLines || CollectSpecials)
   {
      return;
   }

   dist = MIN(R_PointToDist(seg->v1->x, seg->v1->y), R_PointToDist(seg->v2->x, seg->v2->y));

   sprite.object.seg = seg;
   sprite.type = TYPE_SEG;
   sprite.dist = dist * MAP_SCALE;
   visibleSprites.Push(sprite);
}


void GL_AddSprite(AActor *actor)
{
   SPRITELIST sprite;
   float dist;
   bool drawChase;
   float x, y, z;
   ZGL_Vector v1, v2;

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

   x = -viewx * MAP_SCALE;
   y = viewz * MAP_SCALE;
   z = viewy * MAP_SCALE;
   v1.Set(x, y, z);

   x = -actor->x * MAP_SCALE;
   y = actor->z * MAP_SCALE;
   z = actor->y * MAP_SCALE;
   v2.Set(x, y, z);

   dist = (v1 - v2).Length();

   sprite.object.actor = actor;
   sprite.type = TYPE_ACTOR;
   sprite.dist = dist;
   visibleSprites.Push(sprite);

   GL_CheckActorLights(actor);
}


void GL_AddParticle(Particle *p)
{
   SPRITELIST sprite;
   float x, y, z, dist;
   ZGL_Vector v1, v2;

   x = -viewx * MAP_SCALE;
   y = viewz * MAP_SCALE;
   z = viewy * MAP_SCALE;
   v1.Set(x, y, z);
   v2.Set(p->x, p->y, p->z);

   dist = (v1 - v2).Length();

   sprite.object.particle = p;
   sprite.type = TYPE_PARTICLE;
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
   float alpha, offX, offZ, ang;
   bool useVertexPrograms = static_cast<OpenGLFrameBuffer *>(screen)->SupportsVertexPrograms();

   if (!DrawingDeferredLines)
   {
      return;
   }

   HaloLights.Clear();

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

   if (useVertexPrograms && gl_billboard_mode == 3)
   {
      GL_BindVertexProgram("BILLBOARD");
      GL_CalculateCameraVectors();
   }

   for (i = numSprites - 1; i >= 0; i--)
   {
      RL_SetupMode(RL_RESET);
      node = sprites + i;
      switch (node->type)
      {
      case TYPE_ACTOR:
         thing = node->object.actor;
         if (gl_sprite_sharp_edges)
         {
            alpha = MAX((thing->alpha / 65535.f) - 0.5f, 0.f);
         }
         else
         {
            alpha = 0.f;
         }
         glAlphaFunc(GL_GREATER, alpha);
         glDepthMask(GL_TRUE);
         GL_DrawThing(Player, thing, offX, offZ);
         break;
      case TYPE_SEG:
         sec = GL_FakeFlat(node->object.seg->frontsector, &tempSec, NULL, NULL, false);
         alpha = MAX(byte2float[node->object.seg->linedef->alpha] - 0.5f, 0.f);
         glAlphaFunc(GL_GREATER, alpha);
         glDepthMask(GL_TRUE);
         GL_DrawWall(node->object.seg, sec, false);
         break;
      case TYPE_PARTICLE:
         glDepthMask(GL_FALSE);
         glAlphaFunc(GL_GREATER, 0.f);
         GL_DrawParticle(node->object.particle);
         break;
      }
   }

   delete[] sprites;

   if (gl_sprite_sharp_edges) glAlphaFunc(GL_GREATER, 0.0);
   glDisable(GL_ALPHA_TEST);
   glDepthMask(GL_TRUE);
}
