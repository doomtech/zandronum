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
#include "glext.h"

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

#ifndef M_PI
 #define M_PI 3.14159265358979323846	// matches value in gcc v2 math.h
#endif

#define DEG2RAD( a ) (( a * M_PI ) / 180.0)
#define INVERSECOLORMAP		32


extern PFNGLLOCKARRAYSEXTPROC glLockArraysEXT;
extern PFNGLUNLOCKARRAYSEXTPROC glUnlockArraysEXT;
extern PFNGLMULTITEXCOORD1FARBPROC glMultiTexCoord1fARB;
extern PFNGLMULTITEXCOORD2FARBPROC glMultiTexCoord2fARB;
extern PFNGLMULTITEXCOORD3FARBPROC glMultiTexCoord3fARB;
extern PFNGLMULTITEXCOORD4FARBPROC glMultiTexCoord4fARB;
extern PFNGLACTIVETEXTUREARBPROC glActiveTextureARB;
extern PFNGLCLIENTACTIVETEXTUREARBPROC glClientActiveTextureARB;


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

extern int ST_Y;


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
GLint viewport[4];
GLdouble viewMatrix[16];
GLdouble projMatrix[16];
float FogStartLookup[256];

int totalCoords;
gl_poly_t *gl_polys;
TArray<bool> sectorMoving;

extern TextureList textureList;
TArray<subsector_t *> *visibleSubsectors;
BYTE *glpvs = NULL;
subsector_t *PlayerSubsector;
viewarea_t ViewArea, InArea;


CVAR (Bool, gl_wireframe, false, CVAR_SERVERINFO)
CVAR (Bool, gl_blend_animations, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR (Bool, gl_texture, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR (Bool, gl_depthfog, true, CVAR_SERVERINFO | CVAR_ARCHIVE)
CVAR (Float, gl_depthfog_multiplier, 0.60f, CVAR_SERVERINFO | CVAR_ARCHIVE)
CVAR (Bool, gl_weapon, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR (Bool, gl_draw_sky, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR (Bool, gl_draw_skyboxes, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR (Int, gl_mirror_recursions, 4, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR (Bool, gl_mirror_envmap, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR (Bool, gl_nobsp, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR (Bool, gl_light_particles, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR (Bool, gl_texture_showshaders, false, 0)

CUSTOM_CVAR(Bool, gl_vid_stereoscopy, false, CVAR_NOINITCALL)
{
   if (self)
   {
      Printf(PRINT_BOLD, "LOL@CYB: NO STEREOSCOPY FOR YOU! :P\n");
      gl_vid_stereoscopy = false;
   }
}

EXTERN_CVAR (Int, gl_billboard_mode)
EXTERN_CVAR (Int, gl_vid_stencilbits)
EXTERN_CVAR (Int, gl_vid_bitdepth)
EXTERN_CVAR (Bool, gl_lights_multiply)
EXTERN_CVAR (Bool, st_scale)
EXTERN_CVAR (Bool, r_drawplayersprites)
EXTERN_CVAR (Int, r_detail)


CCMD (gl_list_extensions)
{
   char *extensions, *tmp, extension[80];
   int length;

   extensions = NULL;

   PROC wglGetExtString = wglGetProcAddress("wglGetExtensionsStringARB");
   if (wglGetExtString)
   {
      extensions = ((char*(__stdcall*)(HDC))wglGetExtString)(wglGetCurrentDC());
   }

   if (extensions)
   {
      while (tmp = strstr(extensions, " "))
      {
         length = tmp - extensions;
         strncpy(extension, extensions, length);
         extension[length] = '\0';
         extensions += length + 1;
         Printf("%s\n", extension);
      }
   }

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

void R_SetupFrame (AActor *actor);

extern PFNGLLOCKARRAYSEXTPROC glLockArraysEXT;
extern PFNGLUNLOCKARRAYSEXTPROC glUnlockArraysEXT;


//*****************************************************************************
//	VARIABLES

// What is the current renderer we're using?
static	RENDERER_e			g_CurrentRenderer = RENDERER_SOFTWARE;

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

void GL_Init()
{
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


void GL_Set3DMode()
{
   glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0.0, screen->GetWidth() * 1.0, 0.0, screen->GetHeight() * 1.0, -1000.0, 1000.0);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

   glEnable(GL_TEXTURE_2D);
	glDisable(GL_DEPTH_TEST);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}


extern float CurrentVisibility;

void GL_SetupFog(BYTE lightlevel, BYTE r, BYTE g, BYTE b)
{
   float fog[4];
   float amt;

   if (gl_depthfog)
   {
      fog[0] = byte2float[r];
      fog[1] = byte2float[g];
      fog[2] = byte2float[b];
      fog[3] = 1.f;

      amt = 1.0f - byte2float[lightlevel];
      amt = (amt * amt) * 0.01f * gl_depthfog_multiplier * MAP_COEFF;

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
         color = lightLevel * 1.f / LIGHTLEVELMAX;
      }
      else
      {
         color = (lightLevel + (playerlight << 4)) * 1.f / LIGHTLEVELMAX;
      }
   }

   color = clamp<float>(color, 0.f, 1.f);

   *r = byte2float[p->r] * color;
   *g = byte2float[p->g] * color;
   *b = byte2float[p->b] * color;
}


extern fixed_t r_FloorVisibility;
void GL_DrawFloorCeiling(subsector_t *subSec, sector_t *sector, int floorlightlevel, int ceilinglightlevel)
{
   angle_t angle;
   PalEntry *p;
   int i;
   float xOffset, yOffset, xScale, yScale, r, g, b;
   gl_poly_t *poly;
   seg_t *seg;
   FTexture *tex;

   //if (subSec->isPoly) return;

   p = &sector->ColorMap->Color;

   if (!MaskSkybox)
   {
      if (sector->floorpic != skyflatnum)
      {
         tex = TexMan(sector->floorpic);
         if (tex->UseType != FTexture::TEX_Null)
         {
            GL_GetSectorColor(sector, &r, &g, &b, floorlightlevel);
            //floorlightlevel = (int)(floorlightlevel * 0.875f);

            angle = sector->base_floor_angle + sector->floor_angle;
            xOffset = sector->floor_xoffs / (tex->GetWidth() * 1.f * FRACUNIT);
            yOffset = (sector->floor_yoffs + sector->base_floor_yoffs) / (tex->GetHeight() * 1.f * FRACUNIT);
            xScale = sector->floor_xscale * INV_FRACUNIT;
            yScale = sector->floor_yscale * INV_FRACUNIT;

            poly = &gl_polys[(subSec->index * 2) + 0];
            poly->offX = xOffset;
            poly->offY = yOffset;
            poly->scaleX = xScale;
            if (tex->CanvasTexture())
            {
               yScale *= -1;
            }
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
   }
   else
   {
      if (subSec->sector->FloorSkyBox && sector->floorpic == skyflatnum)
      {
         glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
         glDisable(GL_TEXTURE_2D);
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

         glEnable(GL_TEXTURE_2D);
         glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
      }
   }

   if (!MaskSkybox)
   {
      if (sector->ceilingpic != skyflatnum)
      {
         tex = TexMan(sector->ceilingpic);
         if (tex->UseType != FTexture::TEX_Null)
         {
            GL_GetSectorColor(sector, &r, &g, &b, ceilinglightlevel);
            //ceilinglightlevel = (int)(ceilinglightlevel * 0.875f);

            angle = sector->base_ceiling_angle + sector->ceiling_angle;
            xOffset = sector->ceiling_xoffs / (tex->GetWidth() * 1.f * FRACUNIT);
            yOffset = (sector->ceiling_yoffs + sector->base_ceiling_yoffs) / (tex->GetHeight() * 1.f * FRACUNIT);
            xScale = sector->ceiling_xscale * INV_FRACUNIT;
            yScale = sector->ceiling_yscale * INV_FRACUNIT;

            poly = &gl_polys[(subSec->index * 2) + 1];
            poly->offX = xOffset;
            poly->offY = yOffset;
            if (tex->CanvasTexture())
            {
               yScale *= -1;
            }
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
   }
   else
   {
      if (subSec->sector->CeilingSkyBox && sector->ceilingpic == skyflatnum)
      {
         glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
         glDisable(GL_TEXTURE_2D);
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

         glEnable(GL_TEXTURE_2D);
         glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
      }
   }

   textureList.SetTranslation((BYTE *)NULL);

   glMatrixMode(GL_TEXTURE);
   glLoadIdentity();
   glMatrixMode(GL_MODELVIEW);
}


void GL_CollectParticles(int index)
{
   particle_t *p;
   Particle *part;
   int c, lightLevel;
   float r, g, b, ticFrac;
   PalEntry pe;
   float x, y, z;
   sector_t *sec, tempsec;
   BYTE fogR, fogG, fogB;

   sec = subsectors[index].sector;
   sec = GL_FakeFlat(sec, &tempsec, NULL, NULL, false);

   if (sec->ColorMap->Fade)
   {
      fogR = sec->ColorMap->Fade.r;
      fogG = sec->ColorMap->Fade.g;
      fogB = sec->ColorMap->Fade.b;
   }
   else
   {
      fogR = (level.fadeto & 0xFF0000) >> 16;
      fogG = (level.fadeto & 0x00FF00) >> 8;
      fogB = level.fadeto & 0x0000FF;
   }

   ticFrac = r_TicFrac * INV_FRACUNIT;

   for (WORD i = ParticlesInSubsec[index]; i != NO_PARTICLE; i = Particles[i].snext)
	{
      p = &Particles[i];

      c = p->color;
      r = byte2float[screen->GetPalette()[c].r] * byte2float[sec->ColorMap->Color.r] * byte2float[sec->lightlevel];
      g = byte2float[screen->GetPalette()[c].g] * byte2float[sec->ColorMap->Color.g] * byte2float[sec->lightlevel];
      b = byte2float[screen->GetPalette()[c].b] * byte2float[sec->ColorMap->Color.b] * byte2float[sec->lightlevel];

      x = -(p->x + (p->velx * ticFrac)) * MAP_SCALE;
      y = (p->z + (p->velz * ticFrac)) * MAP_SCALE;
      z = (p->y + (p->vely * ticFrac)) * MAP_SCALE;

      if (gl_light_particles) GL_GetLightForPoint(x, y, z, &r, &g, &b);

      pe.r = (BYTE)(r * 255);
      pe.g = (BYTE)(g * 255);
      pe.b = (BYTE)(b * 255);
      pe.a = (BYTE)(p->trans - (p->fade * ticFrac));

      lightLevel = sec->lightlevel + GL_GetIntensityForPoint(x, y, z);
      lightLevel = clamp<int>(lightLevel, 0, 255);
      part = new Particle();
      part->r = (BYTE)(r * 255);
      part->g = (BYTE)(g * 255);
      part->b = (BYTE)(b * 255);
      part->a = (BYTE)(p->trans - (p->fade * ticFrac));
      part->x = x;
      part->y = y;
      part->z = z;
      part->lightLevel = lightLevel;
      part->size = p->size;
      part->fogR = fogR;
      part->fogG = fogG;
      part->fogB = fogB;
      GL_AddParticle(part);
	}
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
         GL_DrawWall(seg, sector, true);
      }
   }
}


bool GL_RenderSubsector(subsector_t *subSec)
{
   int i, numLines, firstLine;
   seg_t *seg;
   int floorlightlevel, ceilinglightlevel;
   sector_t tempSec, *sector;
   line_t *line;
   AActor *actor;

   sector = GL_FakeFlat(subSec->render_sector, &tempSec, &floorlightlevel, &ceilinglightlevel, false);

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
                     GL_DrawWall(seg, sector, false);
                  }
               }
               else
               {
                  seg->linedef->flags |= ML_MAPPED;
                  GL_DrawWall(seg, sector, false);
               }
            }
         }
      }

      if (subSec->poly)
      {
         GL_RenderPolyobj(subSec, sector);
      }
   }

   GL_DrawFloorCeiling(subSec, sector, floorlightlevel, ceilinglightlevel);

   return true;
}


void GL_DrawVisibleSubsectors()
{
   subsector_t *subSec;
   //TArray<subsector_t *> reverseSubsectors;
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
         //reverseSubsectors.Push(subSec);
         GL_CollectParticles(subSec->index);
      }
   }

   // passes go like this:
   // - depth pass
   // - color pass (with depthfog)
   // - lights (n passes)
   // - texture modulation pass
   // - sector fog pass (for colored fog)
   //GL_CollectLights();

   //if (glLockArraysEXT) glLockArraysEXT(0, VertexArraySize);

   RL_RenderList(RL_DEPTH);
   RL_RenderList(RL_BASE);
   if (!gl_wireframe)
   {
      if (gl_lights_multiply) RL_RenderList(RL_LIGHTS);
      if (gl_texture) RL_RenderList(RL_TEXTURED);
      if (!gl_lights_multiply) RL_RenderList(RL_LIGHTS);
   }
   if (gl_depthfog) RL_RenderList(RL_FOG);
   RL_Clear();
   RL_SetupMode(RL_RESET);

   //if (glUnlockArraysEXT) glUnlockArraysEXT();
#if 0
   for (i = 0; i < reverseSubsectors.Size(); i++)
   {
      subSec = reverseSubsectors[i];
      GL_CollectParticles(subSec->index);
   }
#endif
   GL_DrawDecals();
   DrawingDeferredLines = true;
   GL_DrawSprites();
   GL_DrawHalos();
   DrawingDeferredLines = false;
   GL_ClearSprites();

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

   sector = GL_FakeFlat(subSec->render_sector, &tempSec, NULL, NULL, false);
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

   if (CL_ClipperFull()) return;

   if (numnodes == 0)
	{
      GL_AddVisibleSubsector(subsectors);
      return;
	}

   while (!((size_t)node & 1))  // Keep going until found a subsector
	{
		bsp = (node_t *)node;

		// Decide which side the view point is on.
		side = R_PointOnSide(viewx, viewy, bsp);

   	// Recursively divide front space (toward the viewer).
	   GL_RenderBSPNode(bsp->children[side], left, right);

		// Possibly divide back space (away from the viewer).
		side ^= 1;

      if (!CL_CheckBBox (bsp->bbox[side])) return;

      node = bsp->children[side];
	}

	GL_AddVisibleSubsector((subsector_t *)((BYTE *)node - 1));
}


void GL_AddQueuedSubsector(subsector_t *ssec, sector_t *sector, int depthCount)
{
   if (!ssec) return;

   if (CL_SubsectorInFrustum(ssec, sector))
   {
      visibleSubsectors->Push(ssec);
   }
}


sector_t TempSector;
void GL_QueueSubsectors(subsector_t *ssec, int depthCount)
{
   register unsigned long i, firstLine, maxLine;
   sector_t *sector;
   seg_t *seg;

   if (!ssec) return;
   if (ssec->validcount == validcount) return; // already touched
   ssec->validcount = validcount;

   //Printf("%d\n", depthCount);

   sector = GL_FakeFlat(ssec->render_sector, &TempSector, NULL, NULL, false);
   GL_RecalcSubsector(ssec, sector);
   GL_AddQueuedSubsector(ssec, sector, depthCount);

   firstLine = ssec->firstline;
   maxLine = firstLine + ssec->numlines;
   seg = segs + firstLine;

   // note: function is recursive, so "sector" may not be valid after this point

   for (i = firstLine; i < maxLine; i++, seg++)
   {
      if (seg->PartnerSeg && seg->PartnerSeg->Subsector)
      {
         GL_QueueSubsectors(seg->PartnerSeg->Subsector, depthCount + 1);
      }
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

   a1 = CL_FrustumAngle();
   CL_SafeAddClipRange(viewangle + a1, viewangle - a1);

   if (gl_nobsp)
   {
      //GL_AddVisibleSubsectors(a1, a2);
      GL_QueueSubsectors(PlayerSubsector, 0);
      ShouldDrawSky = true;
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
   gl_poly_t *poly;
   double planeData[4];
   int index;

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

   index = numsubsectors * 2;
   index += (seg - segs) * 3;
   poly = gl_polys + (index + 1);

   planeData[0] = poly->plane.A() * -1;
   planeData[1] = poly->plane.B() * -1;
   planeData[2] = poly->plane.C() * -1;
   planeData[3] = poly->plane.D() * -1;

   glEnable(GL_CLIP_PLANE0);
   glClipPlane(GL_CLIP_PLANE0, planeData);

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

   a1 = CL_FrustumAngle();
   CL_SafeAddClipRange(viewangle + a1, viewangle - a1);

   if (gl_nobsp)
   {
      //GL_AddVisibleSubsectors(a1, a2);
      GL_QueueSubsectors(PlayerSubsector, 0);
      ShouldDrawSky = true;
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
   glDisable(GL_CLIP_PLANE0);

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


void GL_DrawScene()
{
   TArray<seg_t *> mirrors;
   static TArray<subsector_t *> subSecs;
   int i, numSubSecs;
   static int lastGametic = gametic;
   float yaw, pitch;
   angle_t a1, a2;
   GLbitfield mask;

   playerlight = Player->extralight;

   CL_CalcFrustumPlanes();
   GL_ClearSprites();
   RL_Clear();
   skyBoxes.Clear();
   mirrors.Clear();
   numTris = 0;

   GL_SetSubsectorArray(&subSecs);
   GL_SetMirrorList(&mirrors);

   CL_ClearClipper();
   a1 = CL_FrustumAngle();
   CL_SafeAddClipRange(viewangle + a1, viewangle - a1);

   if (gl_nobsp)
   {
      //GL_AddVisibleSubsectors(a1, a2);
      GL_QueueSubsectors(PlayerSubsector, 0);
      ShouldDrawSky = true;
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
         glDisable(GL_TEXTURE_2D);
         glEnable(GL_STENCIL_TEST);
         glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
         MaskSkybox = true;
         for (i = 0; i < numSubSecs; i++)
         {
            GL_RenderSubsector(subSecs[i]);
         }
         MaskSkybox = false;
         CL_ClearClipper();
         glEnable(GL_TEXTURE_2D);
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

   static_cast<OpenGLFrameBuffer *>(screen)->SetNumTris(numTris);

   RL_Clear();
}


extern FTexture *CrosshairImage;
void GL_DrawPlayerWeapon(player_t *player)
{
   int i;
   int lightnum, actualextralight;
   pspdef_t* psp;
   sector_t* sec;
   static sector_t tempsec;
   int floorlight, ceilinglight;
   fixed_t ofsx, ofsy;
   int centerY = screen->GetHeight() >> 1;
   int centerX = screen->GetWidth() >> 1;

   if (!camera->player || (player->cheats & CF_CHASECAM)) return;

   sec = GL_FakeFlat (camera->Sector, &tempsec, &floorlight, &ceilinglight, false);

	// [RH] set foggy flag
	foggy = (level.fadeto || sec->ColorMap->Fade || (level.flags & LEVEL_HASFADETABLE));
	actualextralight = foggy ? 0 : extralight << 4;

	// get light level
	lightnum = clamp<int>(((floorlight + ceilinglight) >> 1) + actualextralight, 0, 255);

   if (camera->player != NULL)
   {
      spritedef_t *sprdef;
      spriteframe_t *sprframe;
      bool flip;
      float x1, y1, x2, y2, a, r, g, b, x, y, z;
      float scaleX, scaleY;
      FTexture *tex;
      int picwidth, picheight;

      a = FIX2FLT(player->mo->alpha);
      switch (player->mo->RenderStyle)
      {
      case STYLE_None:
         return;
      case STYLE_Normal:
         glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
         break;
      case STYLE_Add:
         glBlendFunc(GL_SRC_ALPHA, GL_ONE);
         break;
      case STYLE_Shaded:
         glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
         textureList.LoadAlpha(true);
         break;
      default:
         glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
         break;
      }

      P_BobWeapon (camera->player, &camera->player->psprites[ps_weapon], &ofsx, &ofsy);
      for (i = 0, psp = camera->player->psprites; i < NUMPSPRITES; i++, psp++)
		{
         // [RH] Don't draw the targeter's crosshair if the player already has a crosshair set.
         if (psp->state && (i != ps_targetcenter || CrosshairImage == NULL))
         {
            if ((unsigned)psp->state->sprite.index >= (unsigned)sprites.Size ()) continue;

            sprdef = &sprites[psp->state->sprite.index];
            sprframe = &SpriteFrames[sprdef->spriteframes + psp->state->GetFrame()];
            tex = TexMan(sprframe->Texture[0]);
            picwidth = tex->GetWidth();
            picheight = tex->GetHeight();
            flip = sprframe->Flip & 1;

            x1 = FIX2FLT(psp->sx + ofsx) - (320/2) - tex->LeftOffset;
            y1 = FIX2FLT(psp->sy + ofsy) - tex->TopOffset;
#if 0
            if (camera->player && (realviewheight == screen->GetHeight() || (screen->GetWidth() > 320 && !st_scale)))
            {	// Adjust PSprite for fullscreen views
               AWeapon *weapon;
               if (camera->player != NULL)
               {
                  weapon = camera->player->ReadyWeapon;
               }
               if (i <= ps_flash)
               {
                  if (weapon != NULL && weapon->YAdjust != 0)
                  {
                     if (realviewheight == screen->GetHeight())
                     {
                        y1 -= FIX2FLT(weapon->YAdjust);
                     }
                     else
                     {
                        y1 -= FIX2FLT(FixedMul (StatusBar->GetDisplacement (), weapon->YAdjust));
                     }
                  }
                  y1 -= FIX2FLT(BaseRatioSizes[WidescreenRatio][2]);
               }
            }
#endif
            x2 = x1 + tex->GetWidth();
            y2 = y1 + tex->GetHeight();

            //scaleX = FIX2FLT(pspritexscale);
            //scaleY = FIX2FLT(pspriteyscale);
            scaleY = (screen->GetHeight() + GL_GetStatusBarOffset()) / 200.f;
            scaleX = screen->GetHeight() / 200.f;

            x1 *= scaleX;
            x2 *= scaleX;
            y1 *= scaleY;
            y2 *= scaleY;

            x1 += centerX;
            x2 += centerX;

            y1 = screen->GetHeight() - y1;
            y2 = screen->GetHeight() - y2;

            x = -viewx * MAP_SCALE;
            y = viewz * MAP_SCALE;
            z = viewy * MAP_SCALE;

            if (sec->ColorMap->Maps)
            {
               r = 1.f;
               g = 1.f;
               b = 1.f;
               textureList.SetTranslation(sec->ColorMap->Maps);
            }
            else
            {
               r = byte2float[sec->ColorMap->Color.r];
               g = byte2float[sec->ColorMap->Color.g];
               b = byte2float[sec->ColorMap->Color.b];
               textureList.SetTranslation((BYTE *)NULL);
            }

            r *= byte2float[lightnum];
            g *= byte2float[lightnum];
            b *= byte2float[lightnum];

            GL_GetLightForPoint(x, y, z, &r, &g, &b);

            glColor4f(r, g, b, a);
            GL_DrawWeaponTexture(tex, x1, y1, x2, y2);
            textureList.SetTranslation((BYTE *)NULL);
         }
         // [RH] Don't bob the targeter.
         if (i == ps_flash)
         {
            ofsx = ofsy = 0;
         }
      }
      textureList.LoadAlpha(false);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   }
}


void GL_DrawFlash()
{
   PalEntry p;
   int amt;
   float r, g, b, a;

   //GL_Set2DMode();

   ((OpenGLFrameBuffer *)screen)->GetFlash(&p, &amt);

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

   if (player->fixedcolormap == INVERSECOLORMAP)
   //if (player->Powers & PW_INVULNERABILITY)
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


void GL_RenderActorView(AActor *actor, float aspect, float fov)
{
   float yaw, pitch, aspectMul;
   float x, y, z, r, g, b;

   // correct the aspect ratio
   aspectMul = FIX2FLT(yaspectmul);
   // yaspectmul is affected by the detail doubling modes, so account for that
   switch (r_detail)
   {
   case 1:
      aspectMul *= 0.5f;
      break;
   case 2:
      aspectMul *= 2.f;
      break;
   default:
      break;
   }

   aspect *= aspectMul;

   R_FindParticleSubsectors();

   InMirror = false;
   ShouldDrawSky = false;

   R_SetupFrame(actor);

   PlayerSubsector = R_PointInSubsector(viewx, viewy);

   if (PlayerSubsector->render_sector->heightsec && !(PlayerSubsector->render_sector->heightsec->MoreFlags & SECF_IGNOREHEIGHTSEC))
   {
      InArea = ViewArea = viewz <= PlayerSubsector->render_sector->heightsec->floorplane.ZatPoint(viewx,viewy) ? AREA_BELOW :
					   (viewz > PlayerSubsector->render_sector->heightsec->ceilingplane.ZatPoint(viewx,viewy) &&
					   !(PlayerSubsector->render_sector->heightsec->MoreFlags&SECF_FAKEFLOORONLY)) ? AREA_ABOVE : AREA_NORMAL;
   }
   else
   {
      ViewArea = AREA_NORMAL;
      InArea = AREA_DEFAULT;
   }

   //ViewArea = InArea = AREA_ABOVE;

   glMatrixMode(GL_PROJECTION);
   glLoadIdentity();
   GL_SetPerspective(fov * 0.915f, aspect, 1.f, 32768.f);
   glMatrixMode(GL_MODELVIEW);

   RL_SetupMode(RL_RESET);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

   if (gl_wireframe)
   {
      r = g = b = 0.f;
   }
   else
   {
      GL_GetSkyColor(&r, &g, &b);
   }

   glClearColor(r, g, b, 0.f);
   glLoadIdentity();

   yaw = 270.f - ANGLE_TO_FLOAT(viewangle);
   pitch = ANGLE_TO_FLOAT(viewpitch);

   x = viewx * MAP_SCALE;
   y = viewy * MAP_SCALE;
   z = viewz * MAP_SCALE;

   glRotatef(pitch, 1.f, 0.f, 0.f);
   glRotatef(yaw, 0.f, 1.f, 0.f);
   glTranslatef(x, -z, -y);

   GL_DrawScene();

   restoreinterpolations ();
}

void GL_RenderPlayerView(player_t *player, void (*lengthyCallback)())
{
   // have to fudge the numbers to maintain an apparent aspect ratio of 1.6 for the whole screen
   // while only drawing in the current viewport
   int width = realviewwidth;
   int height = realviewheight;
   float aspect = width * 1.f / height;

   //Player = player;

   GL_ResetViewport();
   GL_SetupViewport();
   GL_RenderActorView(player->mo, aspect, player->FOV / (BaseRatioSizes[WidescreenRatio][3] / 48.f));

   GL_Set2DMode();

   if (gl_weapon)
   {
      GL_DrawPlayerWeapon(player);
   }

   GL_DrawBlends(player);

   GL_ResetViewport();

   if (gl_texture_showshaders) GL_DrawShaders();
}


void GL_ScreenShot(const char *filename)
{
   ILuint imgID;
	BYTE *screenGL = new BYTE[screen->GetWidth() * screen->GetHeight() * 3];

	if (!screenGL)
	{
		Printf("ZGL: No memory for screenshot\n");
	}
	else
	{
      static_cast<OpenGLFrameBuffer *>(screen)->ReadPixels(0, 0, screen->GetWidth(), screen->GetHeight(), screenGL);
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
   int i, j, arrayIndex, numPolys, pIndex;;
   subsector_t *subSec;
   seg_t *seg;
   float x, y;
   int numVerts;
   gl_poly_t *poly;

   //GL_UnloadLevelGeometry();

   ViewArea = AREA_NORMAL;
   InArea = AREA_DEFAULT;

   GL_SetupAnimTables();
   RL_Delete();

   // floor + ceiling for each subsector and top + mid + bottom for each seg
   delete[] gl_polys;
   numPolys = (numsubsectors * 2) + (numsegs * 3);
   gl_polys = new gl_poly_t[numPolys];

   sector_t *sec;
   arrayIndex = 0;
   totalCoords = 0;

   if (sortedsubsectors)
   {
      delete[] sortedsubsectors;
      sortedsubsectors = NULL;
   }
   sortedsubsectors = new VisibleSubsector[numsubsectors];

   numVerts = 0;
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

      poly = &gl_polys[(i * 2) + 0];
      poly->subsectorIndex = i;
      poly->numPts = subSec->numlines;
      poly->initialized = false;
      poly->lastUpdate = 0;
      poly->numIndices = 3 + ((subSec->numlines - 3) * 3);
      poly->indices = new unsigned int[poly->numIndices];
      poly->vertices = NULL;
      poly->texCoords = NULL;
      poly->arrayIndex = arrayIndex;

      arrayIndex += subSec->numlines;

      poly = &gl_polys[(i * 2) + 1];
      poly->subsectorIndex = i;
      poly->numPts = subSec->numlines;
      poly->initialized = false;
      poly->lastUpdate = 0;
      poly->numIndices = 3 + ((subSec->numlines - 3) * 3);
      poly->indices = new unsigned int[poly->numIndices];
      poly->vertices = NULL;
      poly->texCoords = NULL;
      poly->arrayIndex = arrayIndex;

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
         poly = &gl_polys[(numsubsectors * 2) + ((i * 3) + 0)];
         poly->numPts = 4;
         poly->initialized = false;
         poly->lastUpdate = 0;
         poly->numIndices = 6;
         poly->indices = new unsigned int[poly->numIndices];
         poly->vertices = NULL;
         poly->texCoords = NULL;
         poly->arrayIndex = arrayIndex;

         arrayIndex += 4;

         poly = &gl_polys[(numsubsectors * 2) + ((i * 3) + 1)];
         poly->numPts = 4;
         poly->initialized = false;
         poly->lastUpdate = 0;
         poly->numIndices = 6;
         poly->indices = new unsigned int[poly->numIndices];
         poly->vertices = NULL;
         poly->texCoords = NULL;
         poly->arrayIndex = arrayIndex;

         arrayIndex += 4;

         poly = &gl_polys[(numsubsectors * 2) + ((i * 3) + 2)];
         poly->numPts = 4;
         poly->initialized = false;
         poly->lastUpdate = 0;
         poly->numIndices = 6;
         poly->indices = new unsigned int[poly->numIndices];
         poly->vertices = NULL;
         poly->texCoords = NULL;
         poly->arrayIndex = arrayIndex;

         arrayIndex += 4;
      }
   }

   // set up the correct subsector indexes for segs and polyobject segs
   for (i = 0; i < numsubsectors; i++)
   {
      subSec = subsectors + i;
      for (j = 0; j < subSec->numlines; j++)
      {
         seg = segs + (subSec->firstline + j);
         pIndex = (numsubsectors * 2) + (seg->index * 3);
         if (seg->linedef)
         {
            poly = &gl_polys[pIndex + 0];
            poly->subsectorIndex = i;
            poly = &gl_polys[pIndex + 1];
            poly->subsectorIndex = i;
            poly = &gl_polys[pIndex + 2];
            poly->subsectorIndex = i;
         }
      }

      if (subSec->poly)
      {
         for (j = 0; j < subSec->poly->numsegs; j++)
         {
            seg = subSec->poly->segs[j];
            pIndex = (numsubsectors * 2) + (seg->index * 3);
            if (seg->linedef)
            {
               poly = &gl_polys[pIndex + 0];
               poly->subsectorIndex = i;
               poly = &gl_polys[pIndex + 1];
               poly->subsectorIndex = i;
               poly = &gl_polys[pIndex + 2];
               poly->subsectorIndex = i;
            }
         }
      }
   }

   totalCoords = arrayIndex;
   VertexArraySize = totalCoords;
   VertexArray = new float[totalCoords * 3];
   TexCoordArray = new float[totalCoords * 2];

   glVertexPointer(3, GL_FLOAT, 0, VertexArray);
   int numTexelUnits = textureList.NumTexUnits();
   if (numTexelUnits)
   {
      for (i = 0; i < numTexelUnits; i++)
      {
         glClientActiveTextureARB(GL_TEXTURE0_ARB + i);
         glTexCoordPointer(2, GL_FLOAT, 0, TexCoordArray);
      }
      glClientActiveTextureARB(GL_TEXTURE0_ARB);
   }

   GL_PrecacheTextures();

   for (i = 0; i < numsubsectors; i++)
   {
      subSec = &subsectors[i];

      subSec->isPoly = false;
      subSec->touched = false;
      for (j = 0; j < subSec->numlines; j++)
      {
         if (segs[subSec->firstline + j].bPolySeg)
         {
            subSec->isPoly = true;
            break;
         }
      }
   }

   GL_PrepareSubsectorLights();
}


void GL_UnloadLevelGeometry()
{
   int i, numPolys;

   delete[] TexCoordArray;
   delete[] VertexArray;

   numPolys = (numsubsectors * 2) + (numsegs * 3);
   for (i = 0; i < numPolys; i++)
   {
      if (gl_polys[i].indices) delete[] gl_polys[i].indices;
   }

   TexCoordArray = NULL;
   VertexArray = NULL;

   delete[] gl_polys;
   delete[] sortedsubsectors;
   if (animLookup) delete[] animLookup;
   textureList.Clear();
   RL_Delete();

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


void GL_SetColor(float r, float g, float b, float a)
{
   glColor4f(r, g, b, a);
}


void GL_RenderViewToCanvas(DCanvas *pic, int x, int y, int width, int height)
{
   BYTE *fb, *buff;

   GL_RenderPlayerView(Player, NULL);
   fb = textureList.ReduceFramebufferToPalette(width, height);
   buff = pic->GetBuffer();
   memcpy(buff, fb, width * height);
   delete[] fb;
}


void GL_ShutDown()
{
   GL_ReleaseShaders();
   GL_ReleaseLights();
}


int GL_GetStatusBarOffset()
{
   if (realviewheight == screen->GetHeight())
   {
      return 0;
   }
   else
   {
      return screen->GetHeight() - ST_Y;
   }
}


void GL_SetupViewport()
{
   static_cast<OpenGLFrameBuffer *>(screen)->SetViewport(viewwindowx, viewwindowy + GL_GetStatusBarOffset(), realviewwidth, realviewheight);
}


void GL_ResetViewport()
{
   static_cast<OpenGLFrameBuffer *>(screen)->SetViewport(0, 0, screen->GetWidth(), screen->GetHeight());
}
