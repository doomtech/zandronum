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
#define TO_MAP(v) ((float)(v)/FRACUNIT)

#include <windows.h>
#include <gl/gl.h>
#include <gl/glu.h>
#include <list>

#define USE_WINDOWS_DWORD
#include "zgl_main.h"
#include "zgl_texturelist.h"
#include "glext.h"

#include "a_sharedglobal.h"
#include "p_lnspec.h"
#include "r_draw.h"
#include "r_sky.h"
#include "templates.h"
#include "w_wad.h"
#include "gi.h"

#define MAX(a,b) ((a) >= (b) ? (a) : (b))

#define INVERSECOLORMAP 32

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
CVAR (Bool, gl_decals_subtractive, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)


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

extern PFNGLBLENDEQUATIONEXTPROC glBlendEquationEXT;

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
   BYTE special = seg->linedef->special;

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


void GL_AddDecal(DBaseDecal *decal, seg_t *seg)
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
      frontSector = GL_FakeFlat(seg->frontsector, &fs, NULL, NULL, false);
      backSector = GL_FakeFlat(seg->backsector, &bs, NULL, NULL, false);
      GL_DrawDecal(DecalList[i].decal, seg, frontSector, backSector);
   }

   if (glBlendEquationEXT) glBlendEquationEXT(GL_FUNC_ADD);
   glPolygonOffset(0.f, 0.f);
   glDepthMask(GL_TRUE);

   DecalList.Clear();
}


void GL_DrawDecal(DBaseDecal *decal, seg_t *seg, sector_t *frontSector, sector_t *backSector)
{
#if 0
#if 1
   line_t *line = seg->linedef;
	side_t *side = seg->sidedef;
	int i;
	fixed_t zpos;
	int light;
	float a;
	bool flipx, flipy, loadAlpha;
	PalEntry *p;
	float dv[4][5];
	int decalTile;
   int index;
   gl_poly_t *poly;

   if (!seg->linedef) return;
	if (decal->RenderFlags & RF_INVISIBLE) return;

   index = (numsubsectors * 2);
   index += seg->index * 3;

	//if (actor->sprite != 0xffff)
	{
		decalTile = decal->PicNum;
		flipx = !!(decal->RenderFlags & RF_XFLIP);
		flipy = !!(decal->RenderFlags & RF_YFLIP);
	}

	switch (decal->RenderFlags & RF_RELMASK)
	{
	default:
		zpos = decal->Z;
		break;

	case RF_RELUPPER:
      poly = &gl_polys[index + 0];
		if (line->flags & ML_DONTPEGTOP)
		{
			zpos = decal->Z + frontSector->ceilingtexz;
		}
		else
		{
			zpos = decal->Z + backSector->ceilingtexz;
		}
		break;
	case RF_RELLOWER:
      poly = &gl_polys[index + 2];
		if (line->flags & ML_DONTPEGBOTTOM)
		{
			zpos = decal->Z + frontSector->ceilingtexz;
		}
		else
		{
			zpos = decal->Z + backSector->floortexz;
		}
		break;
	case RF_RELMID:
      poly = &gl_polys[index + 1];
		if (line->flags & ML_DONTPEGBOTTOM)
		{
			zpos = decal->Z + frontSector->floortexz;
		}
		else
		{
			zpos = decal->Z + frontSector->ceilingtexz;
		}
	}

   if (!poly->vertices) return;

	if (decal->RenderFlags & RF_FULLBRIGHT)
	{
		light=255;
		// I don't thik this is such a good idea...
		//glDisable(GL_FOG);	
	}
	else
	{
		light = MIN<int>(frontSector->lightlevel + (playerlight << 4), 255);
	}
	
	int r = RPART(decal->AlphaColor);
	int g = GPART(decal->AlphaColor);
	int b = BPART(decal->AlphaColor);
	
	float red, green, blue;

   p = &screen->GetPalette()[frontSector->ColorMap->Maps[APART(decal->AlphaColor)]];
   red = byte2float[p->r];
   green = byte2float[p->g];
   blue = byte2float[p->b];

   p = &frontSector->ColorMap->Color;
   red *= byte2float[p->r];
   green *= byte2float[p->g];
   blue *= byte2float[p->b];

	red = r * red / 255.f;
	green = g * green / 255.f;
	blue = b * blue / 255.f;
	a = decal->Alpha * INV_FRACUNIT;

	if (decal->RenderStyle == STYLE_Shaded)
	{
		loadAlpha = true;
		//p.a=CM_SHADE;
	}
	else
	{
		loadAlpha = false;
		red = 1.f;
		green = 1.f;
		blue = 1.f;
	}

   FTexture *tex = TexMan(decalTile);
   if (!tex) return;

   textureList.LoadAlpha(loadAlpha);
   textureList.SetTranslation(decal->Translation);
   textureList.BindTexture(tex);
   textureList.SetTranslation((BYTE *)NULL);
   textureList.LoadAlpha(false);
	
    // now clip the decal to the actual polygon - we do this in full texel coordinates
/*
    int decalwidth=(tex->GetWidth()*decal->ScaleX)>>6;
	int decalheight=(tex->GetHeight()*decal->ScaleX)>>6;
	int decallefto=(tex->LeftOffset*decal->ScaleX)>>6;
	int decaltopo=(tex->TopOffset*decal->ScaleX)>>6;

	// texel index of the decal's left edge
	//int decalpixpos = MulScale30(side->TexelLength, actor->floorclip) - (flipx? decalwidth-decallefto : decallefto);
	int decalpixpos = MulScale30(side->TexelLength, 0) - (flipx? decalwidth-decallefto : decallefto);
   
	int left,right;
    int lefttex,righttex;
*/

	float decalwidth = tex->GetWidth()  * TO_MAP(decal->ScaleX);
	float decalheight= tex->GetHeight() * TO_MAP(decal->ScaleY);
	float decallefto = tex->LeftOffset * TO_MAP(decal->ScaleX);
	float decaltopo  = tex->TopOffset  * TO_MAP(decal->ScaleY);
	

	// texel index of the decal's left edge
	float decalpixpos = (float)side->TexelLength * decal->LeftDistance / (1<<30) - (flipx? decalwidth-decallefto : decallefto);

	float left,right;
	float lefttex,righttex;

	// decal is off the left edge
	if (decalpixpos<0)
	{
		left=0;
		lefttex=-decalpixpos;
	}
	else
	{
		left=decalpixpos;
		lefttex=0;
	}
	
	// decal is off the right edge
	if (decalpixpos+decalwidth>side->TexelLength)
	{
		right=side->TexelLength;
		righttex=right-decalpixpos;
	}
	else
	{
		right=decalpixpos+decalwidth;
		righttex=decalwidth;
	}

	if (right<=left) return;	// nothing to draw

	glColor4f(red, green, blue, a);
    //glColor3f(1.f, 0.f, 0.f);

	switch(decal->RenderStyle)
	{
	case STYLE_Shaded:
	case STYLE_Translucent:
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glAlphaFunc(GL_GREATER,0.0f);
		break;

	case STYLE_Add:
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		glAlphaFunc(GL_GREATER,0.0f);
		break;

	case STYLE_Fuzzy:
		glBlendFunc(GL_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA);
		glAlphaFunc(GL_GREATER,0.0f);
		break;

	default:
		glBlendFunc(GL_ONE,GL_ZERO);	
		glAlphaFunc(GL_GEQUAL,0.5f);
		break;
	}

   GL_SetupFog(poly->lightLevel, poly->fogR, poly->fogG, poly->fogB);

   float fleft = left / MAP_COEFF;
	float fright = right / MAP_COEFF;

	float flength;

	flength = side->TexelLength / MAP_COEFF;

	// one texture unit on the wall as vector
   float vx = (poly->vertices[(2 * 3) + 0] - poly->vertices[(0 * 3) + 0]) / flength;
   float vz = (poly->vertices[(2 * 3) + 2] - poly->vertices[(0 * 3) + 2]) / flength;
	float fracleft=fleft/flength;
   	
	dv[1][0]=dv[0][0]=poly->vertices[(0 * 3) + 0]+vx*fleft;
	dv[1][2]=dv[0][2]=poly->vertices[(0 * 3) + 2]+vz*fleft;

	dv[3][0]=dv[2][0]=poly->vertices[(0 * 3) + 0]+vx*fright;
	dv[3][2]=dv[2][2]=poly->vertices[(0 * 3) + 2]+vz*fright;
   	
	zpos+= FRACUNIT*(flipy? decalheight-decaltopo : decaltopo);

	dv[1][1]=dv[2][1]=zpos * MAP_SCALE;
	dv[0][1]=dv[3][1]=(zpos-decalheight*FRACUNIT) * MAP_SCALE;
	dv[1][4]=dv[2][4]=0;


	//const PatchTextureInfo * pti=tex->BindPatch(p.a, decal->Translation);
   float u, v;
   textureList.GetCorner(&u, &v);
   dv[1][3]=dv[0][3]=((lefttex*64/decal->ScaleX) * 1.f / tex->GetWidth()) * 1.f;
	dv[3][3]=dv[2][3]=((righttex*64/decal->ScaleX) * 1.f / tex->GetWidth()) * u;
	dv[0][4]=dv[3][4] = v;

	// now clip to the top plane
   float topleft, topright;
   topleft = poly->vertices[(0 * 3) + 1];
   topright = poly->vertices[(3 * 3) + 1];

	// completely below the wall
	if (topleft<dv[0][1] && topright<dv[3][1]) return;

	if (topleft<dv[1][1] || topright<dv[2][1])
	{
		// decal has to be clipped at the top
		// let texture clamping handle all extreme cases
		dv[1][4]=(dv[1][1]-topleft)/(dv[1][1]-dv[0][1])*dv[0][4];
		dv[2][4]=(dv[2][1]-topright)/(dv[2][1]-dv[3][1])*dv[3][4];
		dv[1][1]=topleft;
		dv[2][1]=topright;
	}

	// now clip to the bottom plane
   float bottomleft, bottomright;
   bottomleft = poly->vertices[(1 * 3) + 1];
   bottomright = poly->vertices[(2 * 3) + 1];

	// completely above the wall
	if (bottomleft>dv[1][1] && bottomright>dv[2][1]) return;

	if (bottomleft>dv[0][1] || bottomright>dv[3][1])
	{
		// decal has to be clipped at the bottom
		// let texture clamping handle all extreme cases
		dv[0][4]=(dv[1][1]-bottomleft)/(dv[1][1]-dv[0][1])*(dv[0][4]-dv[1][4]) + dv[1][4];
		dv[3][4]=(dv[2][1]-bottomright)/(dv[2][1]-dv[3][1])*(dv[3][4]-dv[2][4]) + dv[2][4];

      //Printf("%.2f %.2f  %.2f %.2f\n", bottomleft, bottomright, topleft, topright);

		dv[0][1]=bottomleft;
		dv[3][1]=bottomright;
	}

	if (flipx)
	{
		float ur=0.f;
		for(i=0;i<4;i++) dv[i][3]=ur-dv[i][3];
	}
	if (flipy)
	{
		float vb=v;
		for(i=0;i<4;i++) dv[i][4]=vb-dv[i][4];
	}

	glBegin(GL_TRIANGLE_FAN);
	for(i=3;i>=0;--i)
	{
		glTexCoord2f(dv[i][3],dv[i][4]);
		glVertex3f(dv[i][0],dv[i][1],dv[i][2]);
	}
	glEnd();
#else
   FTexture *tex;
   fixed_t zpos;
   fixed_t ownerAngle;
   PalEntry *p;
   float x, y, z;
   float r, g, b, a, color;
   float cx, cy;
   float angle;
   float width, height;
   float left, right, top, bottom;
   int decalTile, index;
   bool flipx, loadAlpha;
   texcoord_t t1, t2, t3, t4;
   gl_poly_t *poly;
   Vector v;

   if (decal->RenderFlags & RF_INVISIBLE)
   {
      return;
   }

   index = (numsubsectors * 2);
   index += seg->index * 3;

   if (seg->sidedef->midtexture)
   {
      poly = &gl_polys[index + 1];
   }
   else if (seg->sidedef->toptexture)
   {
      poly = &gl_polys[index + 0];
   }
   else
   {
      poly = &gl_polys[index + 2];
   }

   switch (decal->RenderFlags & RF_RELMASK)
	{
	default:
		zpos = decal->Z;
		break;
	case RF_RELUPPER:
		if (seg->linedef->flags & ML_DONTPEGTOP)
		{
			zpos = decal->Z + frontSector->ceilingtexz;
		}
		else
		{
			zpos = decal->Z + backSector->ceilingtexz;
		}
		break;
	case RF_RELLOWER:
		if (seg->linedef->flags & ML_DONTPEGBOTTOM)
		{
			zpos = decal->Z + frontSector->ceilingtexz;
		}
		else
		{
			zpos = decal->Z + backSector->floortexz;
		}
		break;
	case RF_RELMID:
		if (seg->linedef->flags & ML_DONTPEGBOTTOM)
		{
			zpos = decal->Z + frontSector->floortexz;
		}
		else
		{
			zpos = decal->Z + frontSector->ceilingtexz;
		}
      break;
	}

   if (decal->PicNum != 0xffff)
	{
		decalTile = decal->PicNum;
		flipx = decal->RenderFlags & RF_XFLIP;
	}
	else
	{
		return;
	 // FIX!
      //decalTile = SpriteFrames[sprites[decal->sprite].spriteframes + decal->frame].Texture[0];
	  //flipx = SpriteFrames[sprites[decal->sprite].spriteframes + decal->frame].Flip & 1;
	}

	fixed_t decalx, decaly;
	decal->GetXY (seg->sidedef, decalx, decaly);
   y = zpos * MAP_SCALE;
   //x = -decal->x * MAP_SCALE;
   //z = decal->y * MAP_SCALE;
   x = -decalx * MAP_SCALE;
   z = decaly * MAP_SCALE;

   ownerAngle = R_PointToAngle2(seg->v1->x, seg->v1->y, seg->v2->x, seg->v2->y);
   angle = ANGLE_TO_FLOAT(ownerAngle);

   p = &screen->GetPalette()[frontSector->ColorMap->Maps[APART(decal->AlphaColor)]];
   r = byte2float[p->r];
   g = byte2float[p->g];
   b = byte2float[p->b];

   p = &frontSector->ColorMap->Color;
   r *= byte2float[p->r];
   g *= byte2float[p->g];
   b *= byte2float[p->b];

   if (decal->RenderFlags & RF_FULLBRIGHT)
   {
      glDisable(GL_FOG);
      color = 1.f;

      if (glBlendEquationEXT)
      {
         glBlendEquationEXT(GL_FUNC_ADD);
      }
   }
   else
   {
      GL_SetupFog(poly->lightLevel, poly->fogR, poly->fogG, poly->fogB);
      color = byte2float[poly->lightLevel];

      if (gl_decals_subtractive && glBlendEquationEXT)
      {
         glBlendEquationEXT(GL_FUNC_REVERSE_SUBTRACT);
         v.Set(r, g, b);
         r = (v.Length() - r) * 1.f;
         g = (v.Length() - g) * 1.f;
         b = (v.Length() - b) * 1.f;
      }
   }

   r *= color;
   g *= color;
   b *= color;

   tex = TexMan(decalTile);
   width = (tex->GetWidth() * 0.5f) * (decal->ScaleX / 63.f) / MAP_COEFF;
   height = tex->GetHeight() * (decal->ScaleY / 63.f) / MAP_COEFF;

   top = (tex->TopOffset * (decal->ScaleY / 63.f)) / MAP_COEFF;
   bottom = top - height;
   left = -width;
   right = width;

   a = decal->Alpha * INV_FRACUNIT;

   if (decal->RenderStyle == STYLE_Shaded)
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

   glColor4f(r, g, b, a);

   if (!flipx)
   {
      t1.x *= -1;
      t2.x *= -1;
      t3.x *= -1;
      t4.x *= -1;
   }

   glPushMatrix();
   glTranslatef(x, y, z);
   glRotatef(angle, 0.f, 1.f, 0.f);

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

   glPopMatrix();

   numTris += 2;

   if (gl_depthfog && !Player->fixedcolormap)
   {
      glEnable(GL_FOG);
      //glDisable(GL_FOG);
   }
#endif
#endif
}


void GL_GetOffsetsForSeg(seg_t *seg, float *offX, float *offY, wallpart_t part)
{
   float x, y;
   float scaleX, scaleY;
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

   if (tex->bWorldPanning)
   {
      scaleX = tex->ScaleX ? tex->ScaleX / 8.f : 1.f;
      scaleY = tex->ScaleY ? tex->ScaleY / 8.f : 1.f;
   }
   else
   {
      scaleX = 1.f;
      scaleY = 1.f;
   }

   *offX = (x / tex->GetWidth()) * scaleX;
   *offY = (y / tex->GetHeight()) * scaleY;
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


void GL_DrawWall(seg_t *seg, sector_t *frontSector, bool isPoly)
{
   int index;
   bool deferLine, validTexture;
   bool isMirror, fogBoundary;
   float offX, offY, r, g, b;
   PalEntry p;
   gl_poly_t *poly;
   FTexture *tex;
   FShader *shader;
   sector_t *backSector, ts;
   int lightLevel;

   if (seg->linedef == NULL || seg->sidedef == NULL)
   {
      return;
   }

   if (!GL_ShouldDrawWall(seg))
   {
      return;
   }

   if (DrawingDeferredLines && MaskSkybox)
   {
      return;
   }

   backSector = GL_FakeFlat(seg->backsector, &ts, NULL, NULL, false);

   if (backSector)
   {
      fogBoundary = IsFogBoundary(frontSector, backSector) && gl_depthfog;
   }
   else
   {
      fogBoundary = false;
   }

   index = (numsubsectors * 2);
   index += seg->index * 3;

   deferLine = false;
   if (seg->linedef->alpha < 255 && seg->backsector)
   {
      deferLine = true;
   }

   if (fogBoundary)
   {
      deferLine = true;
   }

   if (seg->sidedef->midtexture)
   {
      textureList.GetTexture(seg->sidedef->midtexture, true);
      if (seg->backsector && textureList.IsTransparent())
      {
         deferLine = true;
      }
   }

   if (!DrawingDeferredLines && deferLine && !CollectSpecials && !MaskSkybox)
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
      p.a = 255;
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
         glDisable(GL_TEXTURE_2D);
         glBegin(GL_TRIANGLE_FAN);
            glVertex3fv(v1);
            glVertex3fv(v3);
            glVertex3fv(v4);
            glVertex3fv(v2);
         glEnd();
         glEnable(GL_TEXTURE_2D);
         glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
      }
   }
   else if (!CollectSpecials && !GL_SkyWall(seg))
   {
      if (p.r == 0xff && p.g == 0xff && p.b == 0xff)
      {
         textureList.SetTranslation(seg->frontsector->ColorMap->Maps);
      }

      if (!seg->sidedef->toptexture)
      {
         validTexture = false;
      }
      else
      {
         tex = TexMan(seg->sidedef->toptexture);
         validTexture = (tex != NULL) && (tex->UseType != FTexture::TEX_Null) && (stricmp(tex->Name, "") != 0);
      }

      if (!DrawingDeferredLines && backSector && validTexture && !isMirror)
      {
         if (backSector->ceilingpic != skyflatnum || frontSector->ceilingpic != skyflatnum)
         {
            GL_GetOffsetsForSeg(seg, &offX, &offY, WT_TOP);

            poly = &gl_polys[index + 0];
            textureList.GetTexture(seg->sidedef->toptexture, true);

            // set up the potentially continuously changing poly stuff here
            poly->offX = offX;
            poly->offY = offY + textureList.GetTopYOffset();
            poly->r = r;
            poly->g = g;
            poly->b = b;
            poly->a = 1.f;
            poly->scaleX = 1.f;
            if (tex->CanvasTexture())
            {
               // these textures are upside down, so make sure to flip them
               poly->scaleY = -1.f;
            }
            else
            {
               poly->scaleY = 1.f;
            }
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
               poly->fogR = (BYTE)((level.fadeto & 0xFF0000) >> 16);
               poly->fogG = (BYTE)((level.fadeto & 0x00FF00) >> 8);
               poly->fogB = (BYTE)(level.fadeto & 0x0000FF);
            }

            RL_AddPoly(textureList.GetGLTexture(), index + 0);

            if (frontSector->ceilingpic == skyflatnum) GL_MaskUpperWall(seg, poly);
         }
      }

      if (!seg->sidedef->midtexture)
      {
         validTexture = false;
      }
      else
      {
         tex = TexMan(seg->sidedef->midtexture);
         validTexture = (tex != NULL) && (tex->UseType != FTexture::TEX_Null) && (stricmp(tex->Name, "") != 0);
      }
      if (((validTexture || fogBoundary) && (deferLine == DrawingDeferredLines) && !(seg->linedef->special == Line_Horizon)) || isMirror)
      {
         if (validTexture) GL_GetOffsetsForSeg(seg, &offX, &offY, WT_MID);

         poly = &gl_polys[index + 1];
         if (isMirror)
         {
            p.a = 25; // 10% opacity
         }
         else if (validTexture)
         {
            textureList.GetTexture(seg->sidedef->midtexture, true);
         }

         // set up the potentially continuously changing poly stuff here
         if (fogBoundary)
         {
            if (frontSector->ColorMap->Fade)
            {
               poly->fbR = frontSector->ColorMap->Fade.r;
               poly->fbG = frontSector->ColorMap->Fade.g;
               poly->fbB = frontSector->ColorMap->Fade.b;
               poly->fbLightLevel = frontSector->lightlevel;
            }
            else
            {
               poly->fbR = backSector->ColorMap->Fade.r;
               poly->fbG = backSector->ColorMap->Fade.g;
               poly->fbB = backSector->ColorMap->Fade.b;
               poly->fbLightLevel = backSector->lightlevel;
            }
         }

         poly->offX = offX;
         poly->offY = offY;
         poly->r = r;
         poly->g = g;
         poly->b = b;
         poly->a = byte2float[p.a];
         poly->scaleX = 1.f;
         if (validTexture && tex->CanvasTexture())
         {
            // these textures are upside down, so make sure to flip them
            poly->scaleY = -1.f;
         }
         else
         {
            poly->scaleY = 1.f;
         }
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
            poly->fogR = (BYTE)((level.fadeto & 0xFF0000) >> 16);
            poly->fogG = (BYTE)((level.fadeto & 0x00FF00) >> 8);
            poly->fogB = (BYTE)(level.fadeto & 0x0000FF);
         }

         if (DrawingDeferredLines || isMirror)
         {
            if (validTexture)
            {
               textureList.BindTexture(seg->sidedef->midtexture, true);
               shader = GL_ShaderForTexture(TexMan[seg->sidedef->midtexture]);
               if (!isMirror && poly->a == 1.f)
               {
                  // only non-translucent geometry gets lit
                  RL_RenderPoly(RL_DEPTH, poly);
                  glAlphaFunc(GL_GREATER, 0.f);
                  RL_RenderPoly(RL_BASE, poly);
                  RL_RenderPoly(RL_LIGHTS, poly);
                  if (!gl_wireframe && gl_texture)
                  {
                     RL_RenderPoly(RL_TEXTURED_CLAMPED, poly);
                  }
                  if (!fogBoundary && gl_depthfog) RL_RenderPoly(RL_FOG, poly);
               }
               else
               {
                  if (isMirror)
                  {
                     glEnable(GL_TEXTURE_GEN_S);
                     glEnable(GL_TEXTURE_GEN_T);
                     glBlendFunc(GL_SRC_ALPHA, GL_ONE);

                     textureList.BindTexture("ENVTEX");
                     GL_RenderPoly(poly, true);

                     glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                     glDisable(GL_TEXTURE_GEN_S);
                     glDisable(GL_TEXTURE_GEN_T);
                  }
                  else
                  {
                     if (!gl_wireframe && gl_texture)
                     {
                        //glAlphaFunc(GL_GREATER, 0.f);
                        RL_RenderPoly(RL_DEFERRED, poly);
                     }
                  }
               }
            }

            if (fogBoundary)
            {
               RL_RenderPoly(RL_MASK, poly);
               RL_RenderPoly(RL_FOG_BOUNDARY, poly);
            }

            RL_SetupMode(RL_RESET);
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
      }

      if (!seg->sidedef->bottomtexture)
      {
         validTexture = false;
      }
      else
      {
         tex = TexMan(seg->sidedef->bottomtexture);
         validTexture = (tex != NULL) && (tex->UseType != FTexture::TEX_Null) && (stricmp(tex->Name, "") != 0);
      }
      if (!DrawingDeferredLines && backSector && validTexture && !isMirror)
      {
         if (backSector->floorpic != skyflatnum || frontSector->floorpic != skyflatnum)
         {
            GL_GetOffsetsForSeg(seg, &offX, &offY, WT_BTM);

            poly = &gl_polys[index + 2];
            textureList.GetTexture(seg->sidedef->bottomtexture, true);

            // set up the potentially continuously changing poly stuff here
            poly->offX = offX;
            poly->offY = offY - textureList.GetBottomYOffset();
            poly->r = r;
            poly->g = g;
            poly->b = b;
            poly->a = 1.f;
            poly->scaleX = 1.f;
            if (tex->CanvasTexture())
            {
               // these textures are upside down, so make sure to flip them
               poly->scaleY = -1.f;
            }
            else
            {
               poly->scaleY = 1.f;
            }
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
               poly->fogR = (BYTE)((level.fadeto & 0xFF0000) >> 16);
               poly->fogG = (BYTE)((level.fadeto & 0x00FF00) >> 8);
               poly->fogB = (BYTE)(level.fadeto & 0x0000FF);
            }

            RL_AddPoly(textureList.GetGLTexture(), index + 2);

            if (frontSector->floorpic == skyflatnum) GL_MaskLowerWall(seg, poly);
         }
      }

      textureList.SetTranslation((BYTE *)NULL);
#if 0
		if (gl_draw_decals && seg->linedef->special != Line_Mirror && !CollectSpecials && DrawingDeferredLines == deferLine && !fogBoundary)
		{

		DBaseDecal *decal = seg->sidedef->AttachedDecals;
		while (decal)
			{
				GL_AddDecal(decal, seg);
				decal = decal->WallNext;
			}
		}
#endif
   }
}
