//-----------------------------------------------------------------------------
//
// Skulltag Source
// Copyright (C) 2005 Brad Carney
// Copyright (C) 2007-2012 Skulltag Development Team
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
// 3. Neither the name of the Skulltag Development Team nor the names of its
//    contributors may be used to endorse or promote products derived from this
//    software without specific prior written permission.
// 4. Redistributions in any form must be accompanied by information on how to
//    obtain complete source code for the software and any accompanying
//    software that uses the software. The source code must either be included
//    in the distribution or be available for no more than the cost of
//    distribution plus a nominal fee, and must be freely redistributable
//    under reasonable conditions. For an executable file, complete source
//    code means the source code for all modules it contains. It does not
//    include source code for modules or files that typically accompany the
//    major components of the operating system on which the executable file
//    runs.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// Created:  10/21/05
//
//
// Filename: gl_main.cpp
//
// Description: 
//
//-----------------------------------------------------------------------------

#include <windows.h>
#include <gl/gl.h>
#include <gl/glu.h>
#include <il/il.h>

#define USE_WINDOWS_DWORD
#include "OpenGLVideo.h"

#include "gl_main.h"
#include "gl_texturelist.h"

#include "am_map.h"
#include "a_sharedglobal.h"
#include "doomdata.h"
#include "c_cvars.h"
#include "c_console.h"
#include "c_dispatch.h"
#include "g_level.h"
#include "m_fixed.h"
#include "m_bbox.h"
#include "p_local.h"
#include "p_lnspec.h"
#include "r_defs.h"
#include "r_plane.h"
#include "r_sky.h"
#include "r_things.h"
#include "sbar.h"
#include "tables.h"
#include "templates.h"
#include "w_wad.h"

#include "gi.h"

extern gameinfo_t gameinfo;
extern level_locals_t level;
extern DFrameBuffer *screen;

extern int BaseBlendR, BaseBlendG, BaseBlendB;
extern float BaseBlendA;

extern int numsectors;
extern sector_t *sectors;

extern visplane_t 				*floorplane;
extern visplane_t 				*ceilingplane;

extern int numnodes;
extern node_t *nodes;

extern int 			numlines;
extern line_t* 		lines;

extern int 			numsegs;
extern seg_t*			segs;
extern drawseg_t *drawsegs;

extern TArray<size_t> InterestingDrawsegs;

extern int 		skyflatnum;

extern particle_t		*Particles;
extern FPalette GPalette;

extern TArray<WORD> ParticlesInSubsec;

extern vissprite_t **spritesorter;
extern int spritesortersize;
extern int vsprcount;

extern bool InMirror;
extern FAnimDef **animLookup;

extern int viewpitch;

extern bool ShouldDrawSky;


int playerlight;
int numTris;

TArray<seg_t *> *mirrorLines;
TArray <sector_t *> visibleSectors;
TArray <ASkyViewpoint *> skyBoxes;
player_t *Player;
bool MaskSkybox;
bool DrawingDeferredLines;
bool InSkybox;
bool CollectSpecials;
GLdouble viewMatrix[16];
GLdouble projMatrix[16];

int totalCoords;
gl_poly_t *gl_polys;
TArray<bool> sectorMoving;
TArray<bool> sectorWasMoving;;

extern TextureList textureList;
TArray<subsector_t *> *visibleSubsectors;
byte *glpvs = NULL;
subsector_t *PlayerSubsector;



EXTERN_CVAR (Int, gl_billboard_mode)
EXTERN_CVAR (Int, gl_vid_stencilbits)
EXTERN_CVAR (Int, gl_vid_bitdepth)

CCMD (gl_list_extensions)
{
   char *extensions, *tmp, extension[80];
   int length;
   extensions = (char *)glGetString(GL_EXTENSIONS);

   while (tmp = strstr(extensions, " "))
   {
      length = tmp - extensions;
      strncpy(extension, extensions, length);
      extension[length] = '\0';
      extensions += length + 1;
      Printf("%s\n", extension);
   }
}


bool lockArrays;
extern bool useMultitexture;

void R_SetupFrame (AActor *actor);

//*****************************************************************************
//	VARIABLES

// What is the current renderer we're using?
static	RENDERER_e			g_CurrentRenderer = RENDERER_SOFTWARE;

//*****************************************************************************
//	CONSOLE VARIABLES

CVAR( Bool, gl_wireframe, false, CVAR_SERVERINFO )
CVAR( Bool, gl_blend_animations, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG )
CVAR( Bool, gl_texture, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG )
CVAR( Bool, gl_depthfog, true, CVAR_SERVERINFO | CVAR_ARCHIVE )
CVAR( Float, gl_depthfog_multiplier, 0.60f, CVAR_SERVERINFO | CVAR_ARCHIVE )
CVAR( Bool, gl_weapon, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG )
CVAR( Bool, gl_draw_sky, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG )
CVAR( Bool, gl_draw_skyboxes, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG )
CVAR( Bool, gl_particles_additive, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG )
CVAR( Bool, gl_particles_grow, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG )
CVAR( Int, gl_mirror_recursions, 4, CVAR_ARCHIVE | CVAR_GLOBALCONFIG )
CVAR( Bool, gl_mirror_envmap, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG )
CVAR( Bool, gl_nobsp, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG )

//*****************************************************************************
//	FUNCTIONS

void OPENGL_Construct( void )
{
	// Nothing to do here yet!
}

//*****************************************************************************
//
RENDERER_e OPENGL_GetCurrentRenderer( void )
{
	return ( g_CurrentRenderer );
}

//*****************************************************************************
//
void OPENGL_SetCurrentRenderer( RENDERER_e Renderer )
{
	g_CurrentRenderer = Renderer;
}


void GL_SetSubsectorArray(TArray<subsector_t *> *array)
{
   visibleSubsectors = array;
}


void GL_SetMirrorList(TArray<seg_t *> *mirrors)
{
   mirrorLines = mirrors;
}


TArray<subsector_t *> *GL_GetSubsectorArray()
{
   return visibleSubsectors;
}


TArray<seg_t *> *GL_GetMirrorList()
{
   return mirrorLines;
}


bool GL_UseStencilBuffer()
{
   return (gl_vid_stencilbits > 0 && ((OpenGLFrameBuffer *)screen)->GetBitdepth() > 16);
}


bool GL_CheckExtension(const char *ext)
{
	const char *supported = NULL;
	int extLen = strlen(ext);

	PROC wglGetExtString = wglGetProcAddress("wglGetExtensionsStringARB");
	if (wglGetExtString)
	{
		supported = ((char*(__stdcall*)(HDC))wglGetExtString)(wglGetCurrentDC());
	}

	if (supported)
	{
		for (const char *p = supported;;p++)
		{
			p = strstr(p, ext);
			if (!p) break;

			if ((p == supported || p[-1] == ' ') && (p[extLen] == '\0' || p[extLen] == ' '))
			{
				return true;
			}
		}
	}

	supported = (char *)glGetString(GL_EXTENSIONS);
	if (supported)
	{
		for (const char *p = supported;;p++)
		{
			p = strstr(p, ext);
			if (!p) return false;

			if ((p == supported || p[-1] == ' ') && (p[extLen] == '\0' || p[extLen] == ' '))
			{
				return true;
			}
		}
	}

	return false;
}


void GL_Set2DMode()
{
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0.0, screen->GetWidth() * 1.0, 0.0, screen->GetHeight() * 1.0, -1.0, 1.0);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

   glEnable(GL_TEXTURE_2D);
	glDisable(GL_DEPTH_TEST);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}


void GL_SetupFog(byte lightlevel, byte r, byte g, byte b)
{
   float fog[4];
   float amt;

   if (gl_depthfog)
   {
      amt = 1.0f - byte2float[lightlevel];

      fog[0] = byte2float[r];
      fog[1] = byte2float[g];
      fog[2] = byte2float[b];
      fog[3] = 1.0f;

      amt = ((amt * amt) / 100.f) * gl_depthfog_multiplier * MAP_COEFF;

      glFogfv(GL_FOG_COLOR, fog);
      glFogf(GL_FOG_DENSITY, amt);
   }
}


void GL_GetSectorColor(sector_t *sector, float *r, float *g, float *b, int lightLevel)
{
   PalEntry *p = &sector->ColorMap->Color;
   float color;

   if (Player->fixedcolormap)
   {
      color = 1.f;
   }
   else
   {
      if (InSkybox)
      {
         color = lightLevel / LIGHTLEVELMAX;
      }
      else
      {
         color = (lightLevel + (playerlight << 4)) / LIGHTLEVELMAX;
      }
   }

   color = clamp<float>(color, 0.f, 1.f);

   *r = byte2float[p->r] * color;
   *g = byte2float[p->g] * color;
   *b = byte2float[p->b] * color;
}


void GL_DrawFloorCeiling(subsector_t *subSec, sector_t *sector, int floorlightlevel, int ceilinglightlevel)
{
   angle_t angle;
   PalEntry *p;
   int i;
   float xOffset, yOffset, xScale, yScale, r, g, b;
   gl_poly_t *poly;
   seg_t *seg;
   FTexture *tex;

   if (subSec->isPoly)
   {
      return;
   }

   p = &sector->ColorMap->Color;

   if (!MaskSkybox)
   {
      if (sector->floorpic != skyflatnum)
      {
         tex = TexMan(sector->floorpic);

         if (Player->fixedcolormap)
         {
            glDisable(GL_FOG);
         }

         GL_GetSectorColor(sector, &r, &g, &b, floorlightlevel);

         angle = sector->base_floor_angle + sector->floor_angle;
         xOffset = sector->floor_xoffs / (tex->GetWidth() * 1.f * FRACUNIT);
         yOffset = (sector->floor_yoffs + sector->base_floor_yoffs) / (tex->GetHeight() * 1.f * FRACUNIT);
         xScale = sector->floor_xscale * INV_FRACUNIT;
         yScale = sector->floor_yscale * INV_FRACUNIT;

         poly = &gl_polys[(subSec->index * 2) + 0];
         poly->isFogBoundary = false;
         poly->offX = xOffset;
         poly->offY = yOffset;
         poly->scaleX = xScale;
         poly->scaleY = yScale;
         poly->r = r;
         poly->g = g;
         poly->b = b;
         poly->a = 1.f;
         poly->rot = ANGLE_TO_FLOAT(angle) * -1.f;
         poly->lightLevel = floorlightlevel;
         poly->doomTex = sector->floorpic;
         if (p->r == 0xff && p->g == 0xff && p->b == 0xff)
         {
            poly->translation = sector->ColorMap->Maps;
         }
         else
         {
            poly->translation = NULL;
         }

         if (sector->ColorMap->Fade)
         {
            poly->fogR = sector->ColorMap->Fade.r;
            poly->fogG = sector->ColorMap->Fade.g;
            poly->fogB = sector->ColorMap->Fade.b;
         }
         else
         {
            poly->fogR = (level.fadeto & 0xFF0000) >> 16;
            poly->fogG = (level.fadeto & 0x00FF00) >> 8;
            poly->fogB = level.fadeto & 0x0000FF;
         }

         textureList.GetTexture(sector->floorpic, true);
         RL_AddPoly(textureList.GetGLTexture(), (subSec->index * 2) + 0);
      }
   }
   else
   {
      if (subSec->sector->FloorSkyBox && sector->floorpic == skyflatnum)
      {
         glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
         textureList.BindGLTexture(0);
         glStencilFunc(GL_ALWAYS, subSec->sector->FloorSkyBox->refmask, ~0);
         glColor3f(1.f, 1.f, 1.f);

         glBegin(GL_TRIANGLE_FAN);
            for (i = subSec->numlines - 1; i >= 0; i--)
            {
               seg = &segs[subSec->firstline + i];
               glVertex3f(-seg->v2->x * MAP_SCALE, sector->floorplane.ZatPoint(seg->v2) * MAP_SCALE, seg->v2->y * MAP_SCALE);
            }
            glVertex3f(-seg->v1->x * MAP_SCALE, sector->floorplane.ZatPoint(seg->v1) * MAP_SCALE, seg->v1->y * MAP_SCALE);
         glEnd();

         glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
      }
   }

   if (!MaskSkybox)
   {
      if (sector->ceilingpic != skyflatnum)
      {
         tex = TexMan(sector->ceilingpic);

         if (Player->fixedcolormap)
         {
            glDisable(GL_FOG);
         }

         GL_GetSectorColor(sector, &r, &g, &b, ceilinglightlevel);

         angle = sector->base_ceiling_angle + sector->ceiling_angle;
         xOffset = sector->ceiling_xoffs / (tex->GetWidth() * 1.f * FRACUNIT);
         yOffset = (sector->ceiling_yoffs + sector->base_ceiling_yoffs) / (tex->GetHeight() * 1.f * FRACUNIT);
         xScale = sector->ceiling_xscale * INV_FRACUNIT;
         yScale = sector->ceiling_yscale * INV_FRACUNIT;

         poly = &gl_polys[(subSec->index * 2) + 1];
         poly->isFogBoundary = false;
         poly->offX = xOffset;
         poly->offY = yOffset;
         poly->scaleX = xScale;
         poly->scaleY = yScale;
         poly->r = r;
         poly->g = g;
         poly->b = b;
         poly->a = 1.f;
         poly->rot = ANGLE_TO_FLOAT(angle) * -1.f;
         poly->lightLevel = ceilinglightlevel;
         poly->doomTex = sector->ceilingpic;
         if (p->r == 0xff && p->g == 0xff && p->b == 0xff)
         {
            poly->translation = sector->ColorMap->Maps;
         }
         else
         {
            poly->translation = NULL;
         }

         if (sector->ColorMap->Fade)
         {
            poly->fogR = sector->ColorMap->Fade.r;
            poly->fogG = sector->ColorMap->Fade.g;
            poly->fogB = sector->ColorMap->Fade.b;
         }
         else
         {
            poly->fogR = (level.fadeto & 0xFF0000) >> 16;
            poly->fogG = (level.fadeto & 0x00FF00) >> 8;
            poly->fogB = level.fadeto & 0x0000FF;
         }

         textureList.GetTexture(sector->ceilingpic, true);
         RL_AddPoly(textureList.GetGLTexture(), (subSec->index * 2) + 1);
      }
   }
   else
   {
      if (subSec->sector->CeilingSkyBox && sector->ceilingpic == skyflatnum)
      {
         glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
         textureList.BindGLTexture(0);
         glStencilFunc(GL_ALWAYS, subSec->sector->CeilingSkyBox->refmask, ~0);
         glColor3f(1.f, 1.f, 1.f);

         glBegin(GL_TRIANGLE_FAN);
            for (i = 0; i < subSec->numlines; i++)
            {
               seg = &segs[subSec->firstline + i];
               glVertex3f(-seg->v1->x * MAP_SCALE, sector->ceilingplane.ZatPoint(seg->v1) * MAP_SCALE, seg->v1->y * MAP_SCALE);
            }
            glVertex3f(-seg->v2->x * MAP_SCALE, sector->ceilingplane.ZatPoint(seg->v2) * MAP_SCALE, seg->v2->y * MAP_SCALE);
         glEnd();

         glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
      }
   }

   textureList.SetTranslation((byte *)NULL);

   glMatrixMode(GL_TEXTURE);
   glLoadIdentity();
   glMatrixMode(GL_MODELVIEW);
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

   if (gl_particles_grow)
   {
      w = h = (size * 0.25f) * grow;
   }
   else
   {
      w = h = size * 0.5f;
   }

   rx = viewMatrix[0]; ry = viewMatrix[4]; rz = viewMatrix[8];
   ux = viewMatrix[1]; uy = viewMatrix[5]; uz = viewMatrix[9];
   textureList.GetCorner(&tx, &ty);

   angle = byte2float[p->a] * 720.f;

   glColor4f(byte2float[p->r], byte2float[p->g], byte2float[p->b], byte2float[p->a]);

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


void GL_DrawParticles(int index)
{
   particle_t *p;
   int c;
   float r, g, b, ticFrac;
   PalEntry pe;
   float x, y, z;
   sector_t *sec, tempsec;

   sec = subsectors[index].sector;
   sec = R_FakeFlat(sec, &tempsec, NULL, NULL, false);

   if (sec->ColorMap->Fade)
   {
      GL_SetupFog(sec->lightlevel, sec->ColorMap->Fade.r, sec->ColorMap->Fade.g, sec->ColorMap->Fade.b);
   }
   else
   {
      GL_SetupFog(sec->lightlevel, ((level.fadeto & 0xFF0000) >> 16), ((level.fadeto & 0x00FF00) >> 8), level.fadeto & 0x0000FF);
   }

   if (gl_particles_additive)
   {
      glBlendFunc(GL_SRC_ALPHA, GL_ONE);
   }
   else
   {
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   }

   glDepthMask(GL_FALSE);

   textureList.BindTexture("GLPART");

   ticFrac = r_TicFrac * INV_FRACUNIT;

   for (WORD i = ParticlesInSubsec[index]; i != NO_PARTICLE; i = Particles[i].snext)
	{
      p = &Particles[i];

      c = p->color;
      r = byte2float[GPalette.BaseColors[c].r] * byte2float[sec->ColorMap->Color.r];
      g = byte2float[GPalette.BaseColors[c].g] * byte2float[sec->ColorMap->Color.g];
      b = byte2float[GPalette.BaseColors[c].b] * byte2float[sec->ColorMap->Color.b];

      x = (p->x + (p->velx * ticFrac)) * MAP_SCALE;
      y = (p->z + (p->velz * ticFrac)) * MAP_SCALE;
      z = (p->y + (p->vely * ticFrac)) * MAP_SCALE;

      pe.r = (byte)(r * 255);
      pe.g = (byte)(g * 255);
      pe.b = (byte)(b * 255);
      pe.a = (byte)(p->trans - (p->fade * ticFrac));

      GL_DrawParticle(-x, y, z, &pe, p->size);
	}

   glDepthMask(GL_TRUE);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}


bool GL_SegFacingDir(fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2)
{
#if 0
   float nx, ny;
   float vvx, vvy;

   nx = (y1 - y2) * INV_FRACUNIT;
   ny = (x2 - x1) * INV_FRACUNIT;
   vvx = (x1 - viewx) * INV_FRACUNIT;
   vvy = (y1 - viewy) * INV_FRACUNIT;

   if ((nx * vvx) + (ny * vvy) > 0)
   {
      return true;
   }
   else
   {
	   return false;
   }
#else
   fixed_t nx, ny, vvx, vvy, res;

   nx = (y1 - y2) >> 8;
   ny = (x2 - x1) >> 8;
   vvx = (x1 - viewx) >> 8;
   vvy = (y1 - viewy) >> 8;

   res = FixedMul(nx, vvx) + FixedMul(ny, vvy);

   if (res > 0)
   {
      return true;
   }
   else
   {
      return false;
   }
#endif
}


void GL_RenderPolyobj(subsector_t *subSec, sector_t *sector)
{
   seg_t *seg;
   int i, numSegs;

   numSegs = subSec->poly->numsegs;

   for (i = 0; i < numSegs; i++)
   {
      seg = subSec->poly->segs[i];

      if (GL_SegFacingDir(seg->v1->x, seg->v1->y, seg->v2->x, seg->v2->y))
      {
         GL_DrawWall(seg, sector, true, false);
      }
   }
}


bool GL_RenderSubsector(subsector_t *subSec)
{
   int i, numLines, firstLine;
   seg_t *seg;
   AActor *actor;
   int floorlightlevel, ceilinglightlevel;
   sector_t tempSec, *sector;
   line_t *line;

   if (subSec->isPoly) return false;

   sector = R_FakeFlat(subSec->sector, &tempSec, &floorlightlevel, &ceilinglightlevel, false);

   if (!CollectSpecials)
   {
      if (!InSkybox)
      {
         subSec->isMapped = true;
      }

      numLines = subSec->numlines;
      firstLine = subSec->firstline;

      for (i = firstLine; i < firstLine + numLines; i++)
      {
         seg = segs + i;
         line = seg->linedef;

         if (seg->linedef)
         {
            if (!seg->bPolySeg && GL_SegFacingDir(seg->v1->x, seg->v1->y, seg->v2->x, seg->v2->y))
            {
               if (seg->linedef->special == Line_Mirror && seg->backsector == NULL)
               {
                  if (gl_mirror_envmap && !MaskSkybox)
                  {
                     seg->linedef->flags |= ML_MAPPED;
                     GL_DrawWall(seg, sector, false, false);
                  }
               }
               else
               {
                  seg->linedef->flags |= ML_MAPPED;
                  GL_DrawWall(seg, sector, false, false);
               }
            }
         }
      }

      if (subSec->poly)
      {
         GL_RenderPolyobj(subSec, sector);
      }

      if (subSec->validcount == validcount && !MaskSkybox && !CollectSpecials && !DrawingDeferredLines)
      {
         if (subSec->sector->validcount < validcount)
         {
            actor = subSec->sector->thinglist;
            while (actor)
            {
               GL_AddSprite(actor);
               actor = actor->snext;
            }
            subSec->sector->validcount = validcount;
         }
      }
   }

   GL_DrawFloorCeiling(subSec, sector, floorlightlevel, ceilinglightlevel);

   return true;
}


void GL_DrawVisibleSubsectors()
{
   subsector_t *subSec;
   TArray<subsector_t *> reverseSubsectors;
   int i, size;

   CL_ClearClipper();
   RL_Clear();

   size = visibleSubsectors->Size();
   validcount++;

   //for (i = size - 1; i >= 0; i--)
   for (i = 0; i < size; i++)
   {
      subSec = visibleSubsectors->Item(i);
      subSec->validcount = validcount;
      if (GL_RenderSubsector(subSec))
      {
         reverseSubsectors.Push(subSec);
      }
   }

   RL_RenderList();
   RL_Clear();
   GL_DrawDecals();

   DrawingDeferredLines = true;
   GL_DrawSprites();
   DrawingDeferredLines = false;
   GL_ClearSprites();
   GL_DrawDecals();

   glCullFace(GL_BACK);
   for (i = 0; i < reverseSubsectors.Size(); i++)
   {
      subSec = reverseSubsectors[i];
      GL_DrawParticles(subSec->index);
   }

   if (InMirror)
   {
      glCullFace(GL_FRONT);
   }

   visibleSubsectors->Clear();
}


void GL_AddVisibleSubsector(subsector_t *subSec)
{
   int i, firstLine, numLines;
   seg_t *seg;
   sector_t tempSec, *sector;

   // completely ignore subsectors used to hold polyobjects
   if (subSec->isPoly) return;

   sector = R_FakeFlat(subSec->sector, &tempSec, NULL, NULL, false);
   GL_RecalcSubsector(subSec, sector);

   if (CL_CheckSubsector(subSec, sector)) // check if the subsector has been clipped away
   {
      if (CL_SubsectorInFrustum(subSec, sector))
      {
         if (sector->ceilingpic == skyflatnum || sector->floorpic == skyflatnum)
         {
            ShouldDrawSky = true;
         }

         firstLine = subSec->firstline;
         numLines = subSec->numlines + firstLine;
         for (i = firstLine; i < numLines; i++)
         {
            seg = segs + i;

            if (seg->linedef)
            {
               if (!seg->backsector && !seg->sidedef->midtexture)
               {
                  ShouldDrawSky = true;
               }
               if (!seg->bPolySeg && GL_SegFacingDir(seg->v1->x, seg->v1->y, seg->v2->x, seg->v2->y) && CL_CheckSegRange(seg))
               {
                  if (seg->linedef->special == Line_Mirror && seg->backsector == NULL)
                  {
                     mirrorLines->Push(seg);
                  }
               }
            }
         }

         if (sector->CeilingSkyBox && !InMirror && !InSkybox && sector->ceilingpic == skyflatnum)
         {
            if (subSec->sector->CeilingSkyBox->refmask == 0)
            {
               subSec->sector->CeilingSkyBox->refmask = skyBoxes.Size() + 1;
               skyBoxes.Push(subSec->sector->CeilingSkyBox);
            }
         }

         if (sector->FloorSkyBox && !InMirror && !InSkybox && sector->floorpic == skyflatnum)
         {
            if (subSec->sector->FloorSkyBox->refmask == 0)
            {
               subSec->sector->FloorSkyBox->refmask = skyBoxes.Size() + 1;
               skyBoxes.Push(subSec->sector->FloorSkyBox);
            }
         }

         visibleSubsectors->Push(subSec);
      }

      GL_ClipSubsector(subSec, sector);
   }
}


//
// hey, no more BSP required!
//
void GL_AddVisibleSubsectors(angle_t left, angle_t right)
{
   int i;
   bool clipperFull = false;

   // sort all subsectors by distance to viewer
   GL_SortSubsectors();

   // collect subsectors in frustum, clipping each as we check it
   // because subsectors are sorted, we know they are in front-to-back order, so it's safe to clip each one as we get it
   // stop if we reach the end, or if the clipper is full
   i = 0;
   CL_SafeAddClipRange(right, left);
   while (!CL_ClipperFull() && i < numsubsectors)
   {
      GL_AddVisibleSubsector(sortedsubsectors[i].subSec);
      i++;
   }
   CL_ClearClipper();
}


void GL_RenderBSPNode(void *node, angle_t left, angle_t right)
{
   int side;
   node_t *bsp;

   if (CL_ClipperFull())
   {
      return;
   }

   if (numnodes == 0)
	{
		//GL_AddVisibleSubsector (subsectors);
      // no nodes, so use non-bsp renderer :)
      GL_AddVisibleSubsectors(left, right);
	}
   else
   {
      while (!((size_t)node & 1))  // Keep going until found a subsector
	   {
		   bsp = (node_t *)node;

		   // Decide which side the view point is on.
		   side = R_PointOnSide(viewx, viewy, bsp);

		   // Recursively divide front space (toward the viewer).
		   GL_RenderBSPNode(bsp->children[side], left, right);

		   // Possibly divide back space (away from the viewer).
		   side ^= 1;
		   node = bsp->children[side];
	   }
	   GL_AddVisibleSubsector((subsector_t *)((BYTE *)node - 1));
   }
}


void GL_RenderSkybox(ASkyViewpoint *skyBox)
{
   long oldx, oldy, oldz, angle, oldpitch;
   float yaw, pitch, x, y, z;
   TArray<subsector_t *> subSecs, *prevSubSecs;
   angle_t a1, a2;

   oldx = viewx; oldy = viewy; oldz = viewz; angle = viewangle; oldpitch = viewpitch;

   viewx = skyBox->x;  viewy = skyBox->y;  viewz = skyBox->z;
   viewangle = angle + skyBox->angle;
   viewpitch = oldpitch + skyBox->pitch;

   PlayerSubsector = R_PointInSubsector(viewx, viewy);

   glClear(GL_DEPTH_BUFFER_BIT);

   glPushMatrix();
   glLoadIdentity();

   yaw = 270.f - ANGLE_TO_FLOAT(viewangle);
   pitch = ANGLE_TO_FLOAT(viewpitch);

   x = viewx * MAP_SCALE;
   y = viewy * MAP_SCALE;
   z = viewz * MAP_SCALE;

   glRotatef(pitch, 1, 0, 0);
   glRotatef(yaw, 0, 1, 0);
   glTranslatef(x, -z, -y);

   CL_CalcFrustumPlanes();

   prevSubSecs = GL_GetSubsectorArray();
   GL_SetSubsectorArray(&subSecs);

   CL_ClearClipper();

   a1 = CL_FrustumAngle(FRUSTUM_LEFT);
   a2 = CL_FrustumAngle(FRUSTUM_RIGHT);
   CL_SafeAddClipRange(a2, a1);

   if (gl_nobsp)
   {
      GL_AddVisibleSubsectors(a1, a2);
   }
   else
   {
      GL_RenderBSPNode(nodes + numnodes - 1, a1, a2);
   }
   GL_DrawVisibleSubsectors();
   CL_ClearClipper();

   glPopMatrix();

   GL_SetSubsectorArray(prevSubSecs);

   viewx = oldx; viewy = oldy; viewz = oldz;
   viewangle = angle;
   viewpitch = oldpitch;
}


#define FIXED2FLOAT(f)			((float)(f) * INV_FRACUNIT)
#define FLOAT2FIXED(f)			(fixed_t)((f) * (float)FRACUNIT)

bool InMirror;

void GL_RenderMirror(seg_t *seg)
{
   angle_t startang = viewangle, a1, a2;
	fixed_t startx = viewx;
	fixed_t starty = viewy;
   float x, y, z, yaw, pitch;
   static int count = 0;
   TArray<subsector_t *> subSecs, *prevSubSecs;
   TArray<seg_t *> mirrors, *prevMirrors;

   if (gl_mirror_recursions == -1)
   {
      return;
   }

   vertex_t *v1 = seg->v1;

	// Reflect the current view behind the mirror.
	if (seg->linedef->dx == 0)
	{ // vertical mirror
		viewx = v1->x - startx + v1->x;
	}
	else if (seg->linedef->dy == 0)
	{ // horizontal mirror
		viewy = v1->y - starty + v1->y;
	}
	else
	{ // any mirror--use floats to avoid integer overflow
		vertex_t *v2 = seg->v2;

		float dx = FIXED2FLOAT(v2->x - v1->x);
		float dy = FIXED2FLOAT(v2->y - v1->y);
		float x1 = FIXED2FLOAT(v1->x);
		float y1 = FIXED2FLOAT(v1->y);
		float x = FIXED2FLOAT(startx);
		float y = FIXED2FLOAT(starty);

		// the above two cases catch len == 0
		float r = ((x - x1)*dx + (y - y1)*dy) / (dx*dx + dy*dy);

		viewx = FLOAT2FIXED((x1 + r * dx)*2 - x);
		viewy = FLOAT2FIXED((y1 + r * dy)*2 - y);
	}

	viewangle = 2 * R_PointToAngle2 (seg->v1->x, seg->v1->y, seg->v2->x, seg->v2->y) - startang;

   glPushMatrix();
   glLoadIdentity();

   yaw = 270.f - ANGLE_TO_FLOAT(viewangle);
   pitch = ANGLE_TO_FLOAT(viewpitch);

   x = viewx * MAP_SCALE;
   y = viewy * MAP_SCALE;
   z = viewz * MAP_SCALE;

   //PlayerSubsector = R_PointInSubsector(viewx, viewy);
   // use the subsector the mirror seg is in for PVS checking
   PlayerSubsector = seg->Subsector;

   glRotatef(pitch, 1.0, 0.0, 0.0);

   if (count & 1)
   {
      glRotatef(yaw, 0.0, 1.0, 0.0);
      glTranslatef(x, -z, -y);
      glScalef(1, 1, 1);
      glCullFace(GL_BACK);
   }
   else
   {
      glRotatef(yaw, 0.0, -1.0, 0.0);
      glTranslatef(-x, -z, -y);
      glScalef(-1, 1, 1);
      glCullFace(GL_FRONT);
   }

   CL_CalcFrustumPlanes();
   skyBoxes.Clear();
   prevMirrors = GL_GetMirrorList();
   GL_SetMirrorList(&mirrors);
   prevSubSecs = GL_GetSubsectorArray();
   GL_SetSubsectorArray(&subSecs);

   count++;

   GL_ClearSprites();

   CL_ClearClipper();
   CL_AddSegRange(seg);

   a1 = CL_FrustumAngle(FRUSTUM_LEFT);
   a2 = CL_FrustumAngle(FRUSTUM_RIGHT);
   CL_SafeAddClipRange(a2, a1);

   if (gl_nobsp)
   {
      GL_AddVisibleSubsectors(a1, a2);
   }
   else
   {
      GL_RenderBSPNode(nodes + numnodes - 1, a1, a2);
   }

   if (gl_mirror_recursions >= count && count <= 30)
   {
      seg_t *seg;
      for (int i = 0; i < mirrors.Size(); i++)
      {
         seg = mirrors[i];
         GL_RenderMirror(seg);
      }
      mirrors.Clear();
   }

   if (count & 1)
   {
      glCullFace(GL_FRONT);
   }
   else
   {
      glCullFace(GL_BACK);
   }

   CL_CalcFrustumPlanes();

   CL_ClearClipper();
   CL_AddSegRange(seg);
   GL_DrawVisibleSubsectors();
   GL_ClearSprites();
   CL_ClearClipper();

   subSecs.Clear();
   GL_SetSubsectorArray(prevSubSecs);
   GL_SetMirrorList(prevMirrors);

   count--;

   glCullFace(GL_FRONT);

   glPopMatrix();

   viewangle = startang;
	viewx = startx;
	viewy = starty;
}


void GL_DrawMirrors()
{
   seg_t *seg;
   int i;
   InMirror = true;

   for (i = 0; i < mirrorLines->Size(); i++)
   {
      seg = mirrorLines->Item(i);
    GL_RenderMirror(seg);
   }
   mirrorLines->Clear();

   InMirror = false;
   glCullFace(GL_BACK);
   CL_CalcFrustumPlanes();
}


void GL_DrawSkyboxes()
{
   ASkyViewpoint *skyBox;
   int i;
   bool useStencil = GL_UseStencilBuffer();

   if (gl_draw_skyboxes)
   {
      if (useStencil)
      {
         glEnable(GL_STENCIL_TEST);
         glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
      }

      InSkybox = true;
      for (i = 0; i < skyBoxes.Size(); i++)
      {
         skyBox = skyBoxes[i];
         if (useStencil)
         {
            glStencilFunc(GL_EQUAL, skyBox->refmask, ~0);
         }
         GL_RenderSkybox(skyBox);
         skyBox->refmask = 0;
      }
      skyBoxes.Clear();
      InSkybox = false;

      if (useStencil)
      {
         glDisable(GL_STENCIL_TEST);
      }

      CL_CalcFrustumPlanes();
   }
}


void GL_DrawScene(player_t *player)
{
   TArray<seg_t *> mirrors;
   TArray<subsector_t *> subSecs;
   int i, numSubSecs;
   static int lastGametic = gametic;
   float yaw, pitch;
   GLbitfield mask;
   angle_t a1, a2;

   playerlight = player->extralight;

   if (gl_depthfog && !Player->fixedcolormap)
   {
      glEnable(GL_FOG);
   }
   else
   {
      glDisable(GL_FOG);
   }

   Player = player;

   if (gl_wireframe || !gl_texture)
   {
      glDisable(GL_TEXTURE_2D);
   }

   CL_CalcFrustumPlanes();
   GL_ClearSprites();
   RL_Clear();
   skyBoxes.Clear();
   mirrors.Clear();
   numTris = 0;

   GL_SetSubsectorArray(&subSecs);
   GL_SetMirrorList(&mirrors);

   CL_ClearClipper();
   a1 = CL_FrustumAngle(FRUSTUM_LEFT);
   a2 = CL_FrustumAngle(FRUSTUM_RIGHT);
   CL_SafeAddClipRange(a2, a1);

   if (gl_nobsp)
   {
      GL_AddVisibleSubsectors(a1, a2);
   }
   else
   {
      GL_RenderBSPNode(nodes + numnodes - 1, a1, a2);
   }
   CL_ClearClipper();

   numSubSecs = subSecs.Size();

   mask = GL_DEPTH_BUFFER_BIT;
   if ((gl_draw_sky && ShouldDrawSky) || gl_wireframe)
   {
      mask |= GL_COLOR_BUFFER_BIT;
   }
   if (GL_UseStencilBuffer())
   {
      mask |= GL_STENCIL_BUFFER_BIT;
   }
   glClear(mask);

   if (gl_draw_sky && !gl_wireframe && ShouldDrawSky)
   {
      glPushMatrix();
      glLoadIdentity();

      yaw = 270.f - ANGLE_TO_FLOAT(viewangle);
      pitch = ANGLE_TO_FLOAT(viewpitch);

      glRotatef(pitch, 1.f, 0.f, 0.f);
      glRotatef(yaw, 0.f, 1.f, 0.f);
      GL_DrawSky();
      glPopMatrix();
   }

   NetUpdate ();

   if (skyBoxes.Size() && gl_draw_skyboxes)
   {
      CL_ClearClipper();

      if (GL_UseStencilBuffer())
      {
         glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
         textureList.BindGLTexture(0);
         glEnable(GL_STENCIL_TEST);
         glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
         MaskSkybox = true;
         for (i = 0; i < numSubSecs; i++)
         {
            GL_RenderSubsector(subSecs[i]);
         }
         MaskSkybox = false;
         CL_ClearClipper();
         glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
      }

      GL_DrawSkyboxes();

      glClear(GL_DEPTH_BUFFER_BIT);
   }

   if (mirrors.Size() > 0)
   {
      GL_DrawMirrors();
      mirrors.Clear();
   }

   GL_DrawVisibleSubsectors();

   NetUpdate ();

   if (gl_depthfog)
   {
      glDisable(GL_FOG);
   }

   RL_Clear();
}

EXTERN_CVAR( Bool, r_drawplayersprites );
void GL_DrawPlayerWeapon(player_t *player)
{
   pspdef_t *psp;
   spritedef_t *sprdef;
   spriteframe_t *sprframe;
   sector_t *sector, tempsec;
   short lump;
   int i;
   float r, g, b, a, color;
   float x, y;
   PalEntry *p;
   FTexture *tex;
   fixed_t ofsx, ofsy;
   AInventory	*pInventory;

	if (!r_drawplayersprites ||
		!camera->player ||
		(players[consoleplayer].cheats & CF_CHASECAM))
		return;

   if (camera->player != NULL)
	{
      //GL_Set2DMode();

      sector = R_FakeFlat(camera->Sector, &tempsec, NULL, NULL, false);
      color = (float)(sector->lightlevel + (playerlight << 4)) / LIGHTLEVELMAX;
      p = &sector->ColorMap->Color;
      r = color * byte2float[p->r];
      g = color * byte2float[p->g];
      b = color * byte2float[p->b];

	  if ( camera->RenderStyle == STYLE_Translucent )
		  a = (float)camera->alpha / (float)FRACUNIT;
	  else
		  a = 1.0f;
/*
	  pInventory = player->mo->FindInventory( PClass::FindClass( "BlurSphere" ));
	  if ( pInventory == NULL )
		  pInventory = player->mo->FindInventory( PClass::FindClass( "BlurSphere" ));
      if ( pInventory )
	   {
		   APowerup		*pPowerup;

		   pPowerup = static_cast<APowerup *>( pInventory );
		   if (( pPowerup->EffectTics < 4*32) && !( pPowerup->EffectTics & 8))
         {
            a = 1.0;
         }
         else
         {
		      a = 0.2;
         }
	   }
      else
      {
         a = 1.0;
      }
*/
      glColor4f(r, g, b, a);
/*
      FWeaponInfo *weapon;
		if (camera->player->powers[pw_weaponlevel2])
      {
			weapon = wpnlev2info[camera->player->readyweapon];
      }
		else
      {
			weapon = wpnlev1info[camera->player->readyweapon];
      }
*/
      if (p->r == 0xff && p->g == 0xff && p->b == 0xff)
      {
         textureList.SetTranslation(camera->Sector->ColorMap->Maps);
      }

      P_BobWeapon (camera->player, &camera->player->psprites[ps_weapon], &ofsx, &ofsy);

		// add all active psprites
		for (i = 0, psp = camera->player->psprites; i < NUMPSPRITES; i++, psp++)
		{
			if (psp->state)
         {
            sprdef = &sprites[psp->state->sprite.index];
            sprframe = &SpriteFrames[sprdef->spriteframes + psp->state->GetFrame()];
            lump = sprframe->Texture[0];

            tex = TexMan[lump];
            y = ((psp->sy + ofsy) * INV_FRACUNIT) + 0.f;
			if (camera->player->ReadyWeapon)
            {
				y += FixedMul(StatusBar->GetDisplacement(), camera->player->ReadyWeapon->YAdjust) * INV_FRACUNIT;
            }
            x = (psp->sx + ofsx) * INV_FRACUNIT;

            GL_DrawWeaponTexture(tex, x, y);
         }
		}
      textureList.SetTranslation((byte *)NULL);
	}
}


void GL_DrawFlash()
{
   PalEntry p;
   int amt;
   float r, g, b, a;

   //GL_Set2DMode();

   ((OpenGLFrameBuffer *)screen)->GetFlash(p, amt);

   if (amt == 0)
   {
      return;
   }

   glDisable(GL_DEPTH_TEST);
   glDisable(GL_TEXTURE_2D);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

   r = byte2float[p.r];
   g = byte2float[p.g];
   b = byte2float[p.b];
   a = byte2float[amt];
   glColor4d(r, g, b, a);

   glBegin(GL_TRIANGLE_STRIP);
      glVertex2d(0.0, screen->GetHeight());
      glVertex2d(0.0, 0.0);
      glVertex2d(screen->GetWidth(), screen->GetHeight());
      glVertex2d(screen->GetWidth(), 0.0);
   glEnd();

   glEnable(GL_TEXTURE_2D);
   glEnable(GL_DEPTH_TEST);
}


void GL_DrawBlends(player_t *player)
{
   GL_DrawFlash();

   if (player->fixedcolormap == NUMCOLORMAPS)
   {
      glDisable(GL_DEPTH_TEST);
      glDisable(GL_TEXTURE_2D);
      glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ONE_MINUS_SRC_COLOR);

      glColor4d(1, 1, 1, 1);

      glBegin(GL_TRIANGLE_STRIP);
         glVertex2d(0.0, screen->GetHeight());
         glVertex2d(0.0, 0.0);
         glVertex2d(screen->GetWidth(), screen->GetHeight());
         glVertex2d(screen->GetWidth(), 0.0);
      glEnd();

      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glEnable(GL_TEXTURE_2D);
      glEnable(GL_DEPTH_TEST);
   }
}


extern unsigned int frameStartMS;
unsigned int I_MSTime (void);

void GL_SetPerspective(float fovY, float aspect, float zNear, float zFar)
{
   float fH, fW;

   // note that the doom fov is horizontal fov, so adjust accordingly
   fovY = clamp<float>(fovY, 1.f, 140.f) / aspect;
   fH = tanf(fovY / 180.f * M_PI) * zNear / 2.f;
   fW = fH * aspect;

   glFrustum(-fW, fW, -fH, fH, zNear, zFar);
}

void GL_RenderPlayerView(player_t *player, void (*lengthyCallback)())
{
   float yaw, pitch;
   float x, y, z, r, g, b;

   Player = player;
   InMirror = false;
   ShouldDrawSky = false;

   frameStartMS = I_MSTime ();

   R_SetupFrame(player->mo);
   R_FindParticleSubsectors();
   GL_UpdateShaders();

   PlayerSubsector = R_PointInSubsector(viewx, viewy);

   glMatrixMode(GL_PROJECTION);
   glLoadIdentity();
   GL_SetPerspective(player->FOV * 0.915f, 1.6f, 1.f, 16384.f);
   glMatrixMode(GL_MODELVIEW);

   glEnable(GL_DEPTH_TEST);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

   if (gl_wireframe || !gl_texture)
   {
      glDisable(GL_TEXTURE_2D);
   }
   else
   {
      glEnable(GL_TEXTURE_2D);
   }

   if (gl_wireframe)
   {
      glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
      r = g = b = 0.f;
   }
   else
   {
      glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
      GL_GetSkyColor(&r, &g, &b);
   }

   glClearColor(r, g, b, 1.f);
   glLoadIdentity();

   yaw = 270.f - ANGLE_TO_FLOAT(viewangle);
   pitch = ANGLE_TO_FLOAT(viewpitch);

   //yaw = 270.f - (viewangle * 1.f / (1 << ANGLETOFINESHIFT) * 360.f / FINEANGLES);
   //pitch = viewpitch * 1.f / (1 << ANGLETOFINESHIFT) * 360.f / FINEANGLES;

   x = viewx * MAP_SCALE;
   y = viewy * MAP_SCALE;
   z = viewz * MAP_SCALE;

   glRotatef(pitch, 1.f, 0.f, 0.f);
   glRotatef(yaw, 0.f, 1.f, 0.f);
   glTranslatef(x, -z, -y);

   if (lengthyCallback) lengthyCallback();
   GL_DrawScene(player);
   if (lengthyCallback) lengthyCallback();

   if (gl_wireframe)
   {
      glEnable(GL_TEXTURE_2D);
      glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
   }

   GL_Set2DMode();

   if (gl_weapon)
   {
      GL_DrawPlayerWeapon(player);
   }

   GL_DrawBlends(player);

   restoreinterpolations ();
}


void GL_ScreenShot(const char *filename)
{
   ILuint imgID;
	byte *screenGL = new byte[screen->GetWidth() * screen->GetHeight() * 3];

	if (!screenGL)
	{
		Printf( PRINT_OPENGL, "No memory for screenshot\n");
	}
	else
	{
		glReadPixels(0, 0, screen->GetWidth(), screen->GetHeight(), GL_RGB, GL_UNSIGNED_BYTE, screenGL);
      ilGenImages(1, &imgID);
      ilBindImage(imgID);
      ilTexImage(screen->GetWidth(), screen->GetHeight(), 1, 3, IL_RGB, IL_UNSIGNED_BYTE, screenGL);
		delete[] screenGL;
      ilHint(IL_COMPRESSION_HINT, IL_USE_COMPRESSION);
      ilSave(IL_PNG, (char *)filename);
      ilDeleteImages(1, &imgID);
	}
}


void GL_GenerateLevelGeometry()
{
   int i, j, arrayIndex;
   subsector_t *subSec;
   seg_t *seg;
   float x, y;

   //GL_UnloadLevelGeometry();

   GL_SetupAnimTables();
   RL_Delete();

   // floor + ceiling for each subsector and top + mid + bottom for each seg
   delete[] gl_polys;
   gl_polys = new gl_poly_t[(numsubsectors * 2) + (numsegs * 3)];

   sector_t *sec;
   arrayIndex = 0;
   totalCoords = 0;

   if (sortedsubsectors)
   {
      delete[] sortedsubsectors;
      sortedsubsectors = NULL;
   }
   sortedsubsectors = new VisibleSubsector[numsubsectors];

   for (i = 0; i < numsubsectors; i++)
   {
      subSec = subsectors + i;
      sec = subSec->sector;

      if (subSec->poly)
      {
         for (j = subSec->poly->numsegs - 1; j >= 0; j--)
         {
            subSec->poly->segs[j]->bPolySeg = 1;
         }
      }

      subSec->index = i;
      sortedsubsectors[i].subSec = subSec;
      sortedsubsectors[i].dist = 0;

      gl_polys[(i * 2) + 0].numPts = subSec->numlines;
      gl_polys[(i * 2) + 0].initialized = false;
      gl_polys[(i * 2) + 0].lastUpdate = 0;
      gl_polys[(i * 2) + 0].vertices = NULL;
      gl_polys[(i * 2) + 0].texCoords = NULL;

      arrayIndex += subSec->numlines;

      gl_polys[(i * 2) + 1].numPts = subSec->numlines;
      gl_polys[(i * 2) + 1].initialized = false;
      gl_polys[(i * 2) + 1].lastUpdate = 0;
      gl_polys[(i * 2) + 1].vertices = NULL;
      gl_polys[(i * 2) + 1].texCoords = NULL;

      arrayIndex += subSec->numlines;
   }

   for (i = 0; i < numlines; i++)
   {
      lines[i].textureChanged = 0;
   }

   for (i = 0; i < numsegs; i++)
   {
      seg = &segs[i];
      seg->index = i;
      x = (seg->v2->x - seg->v1->x) * INV_FRACUNIT;
      y = (seg->v2->y - seg->v1->y) * INV_FRACUNIT;
      seg->length = sqrtf((x * x) + (y * y));

      // ignore minisegs!
      if (seg->linedef)
      {
         gl_polys[(numsubsectors * 2) + ((i * 3) + 0)].numPts = 4;
         gl_polys[(numsubsectors * 2) + ((i * 3) + 0)].initialized = false;
         gl_polys[(numsubsectors * 2) + ((i * 3) + 0)].lastUpdate = 0;
         gl_polys[(numsubsectors * 2) + ((i * 3) + 0)].vertices = NULL;
         gl_polys[(numsubsectors * 2) + ((i * 3) + 0)].texCoords = NULL;

         arrayIndex += 4;

         gl_polys[(numsubsectors * 2) + ((i * 3) + 1)].numPts = 4;
         gl_polys[(numsubsectors * 2) + ((i * 3) + 1)].initialized = false;
         gl_polys[(numsubsectors * 2) + ((i * 3) + 1)].lastUpdate = 0;
         gl_polys[(numsubsectors * 2) + ((i * 3) + 1)].vertices = NULL;
         gl_polys[(numsubsectors * 2) + ((i * 3) + 1)].texCoords = NULL;

         arrayIndex += 4;

         gl_polys[(numsubsectors * 2) + ((i * 3) + 2)].numPts = 4;
         gl_polys[(numsubsectors * 2) + ((i * 3) + 2)].initialized = false;
         gl_polys[(numsubsectors * 2) + ((i * 3) + 2)].lastUpdate = 0;
         gl_polys[(numsubsectors * 2) + ((i * 3) + 2)].vertices = NULL;
         gl_polys[(numsubsectors * 2) + ((i * 3) + 2)].texCoords = NULL;

         arrayIndex += 4;
      }
   }

   totalCoords = arrayIndex;

   GL_PrecacheTextures();
   GL_BuildSkyDisplayList();
   viewsector = R_PointInSubsector(viewx, viewy)->sector;

   for (i = 0; i < numsubsectors; i++)
   {
      subSec = &subsectors[i];

      subSec->isPoly = false;
      for (j = 0; j < subSec->numlines; j++)
      {
         if (segs[subSec->firstline + j].bPolySeg)
         {
            subSec->isPoly = true;
            break;
         }
      }
   }
}


void GL_UnloadLevelGeometry()
{
   int i, numPolys;

   numPolys = (numsubsectors * 2) + (numsegs * 3);

   for (i = 0; i < numPolys; i++)
   {
      delete[] gl_polys[i].vertices;
      delete[] gl_polys[i].texCoords;
      gl_polys[i].vertices = NULL;
      gl_polys[i].texCoords = NULL;
   }

   delete[] gl_polys;
   delete[] sortedsubsectors;
   sectorMoving.Clear();
   sectorWasMoving.Clear();
   if (animLookup) delete[] animLookup;
   textureList.Clear();
   RL_Delete();
   GL_DeleteSkyDisplayList();
   if (glpvs)
   {
      delete[] glpvs;
      glpvs = NULL;
   }
}


void GL_PurgeTextures()
{
   textureList.Purge();
}

void GL_RenderViewToCanvas(DCanvas *pic, int x, int y, int width, int height)
{
   byte *fb, *buff;

   GL_RenderPlayerView(Player, NULL);
   fb = textureList.ReduceFramebufferToPalette(width, height);
   buff = pic->GetBuffer();
   memcpy(buff, fb, width * height);
   delete[] fb;
}
