/*
** gl_sky.cpp
**
** Draws the sky.  Loosely based on the JDoom sky and the ZDoomGL 0.66.2 sky.
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
#include "d_player.h"
#include "g_level.h"
#include "r_sky.h"
#include "w_wad.h"


GLuint skyDL;
bool ShouldDrawSky;


//
// Shamelessly lifted from Doomsday (written by Jaakko Keränen)
//


EXTERN_CVAR(Bool, gl_depthfog)
CVAR (Int, gl_sky_detail, 5, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)


#define SKYHEMI_UPPER		0x1
#define SKYHEMI_LOWER		0x2
#define SKYHEMI_JUST_CAP	0x4	// Just draw the top or bottom cap.


extern TextureList textureList;
extern fixed_t sky1pos, sky1speed;
extern fixed_t sky2pos, sky2speed;
extern fixed_t viewx, viewy, viewz;
extern player_t *Player;


// The texture offset to be applied to the texture coordinates in SkyVertex().
static angle_t maxSideAngle = ANGLE_180 / 3;
static float texoff;
static int rows, columns;	
static fixed_t scale = 10000 << FRACBITS;
static bool yflip;
static int texw,texh;
static float tx, ty;
static float yMult;

#define SKYHEMI_UPPER		0x1
#define SKYHEMI_LOWER		0x2
#define SKYHEMI_JUST_CAP	0x4	// Just draw the top or bottom cap.


static void SkyVertex(int r, int c)
{
   angle_t topAngle= (angle_t)(c / (float)columns * ANGLE_MAX);
   angle_t sideAngle = maxSideAngle * (rows - r) / rows;
   fixed_t height = finesine[sideAngle>>ANGLETOFINESHIFT];
   fixed_t realRadius = FixedMul(scale, finecosine[sideAngle>>ANGLETOFINESHIFT]);
   fixed_t x = FixedMul(realRadius, finecosine[topAngle>>ANGLETOFINESHIFT]);
   fixed_t y = (!yflip) ? FixedMul(scale, height) : FixedMul(scale, height) * -1;
   fixed_t z = FixedMul(realRadius, finesine[topAngle>>ANGLETOFINESHIFT]);
   float fx, fy, fz;
   float color = r * 1.f / rows;
   float u, v;
   float timesRepeat;

   timesRepeat = (short)(4 * (256.f / texw));
   if (timesRepeat == 0.f) timesRepeat = 1.f;

   if (r == 0)
   {
      glColor4f(1.f, 1.f, 1.f, 0.f);
   }
   else
   {
      glColor4f(1.f, 1.f, 1.f, 1.f);
   }

	// And the texture coordinates.
	if(!yflip)	// Flipped Y is for the lower hemisphere.
   {
      u = (-timesRepeat * c / (float)columns) * yMult;
      v = (r / (float)rows) * 1.f * yMult;
   }
	else
   {
      u = (-timesRepeat * c / (float)columns) * yMult;
      v = ((rows-r)/(float)rows) * 1.f * yMult;
   }

   fx = x * INV_FRACUNIT;
   fy = y * INV_FRACUNIT;
   fz = z * INV_FRACUNIT;

   glTexCoord2f(u, v);
	// And finally the vertex.
	glVertex3f(fx, fy - 1.f, fz);
}


// Hemi is Upper or Lower. Zero is not acceptable.
// The current texture is used. SKYHEMI_NO_TOPCAP can be used.
void GL_RenderSkyHemisphere(int hemi)
{
	int r, c;

	if (hemi & SKYHEMI_LOWER)
   {
      yflip = true;
   }
   else
   {
      yflip = false;
   }

	// The top row (row 0) is the one that's faded out.
	// There must be at least 4 columns. The preferable number
	// is 4n, where n is 1, 2, 3... There should be at least
	// two rows because the first one is always faded.
	rows = 4;
   columns = 4 * (gl_sky_detail > 0 ? gl_sky_detail : 1);

   if (hemi & SKYHEMI_JUST_CAP)
   {
      return;
   }

	// The total number of triangles per hemisphere can be calculated
	// as follows: rows * columns * 2 + 2 (for the top cap).
	for(r = 0; r < rows; r++)
	{
      if (yflip)
      {
         glBegin(GL_TRIANGLE_STRIP);
            SkyVertex(r + 1, 0);
		      SkyVertex(r, 0);
		      for(c = 1; c <= columns; c++)
		      {
               SkyVertex(r + 1, c);
			      SkyVertex(r, c);
		      }
		   glEnd();
      }
      else
      {
		   glBegin(GL_TRIANGLE_STRIP);
            SkyVertex(r, 0);
		      SkyVertex(r + 1, 0);
		      for(c = 1; c <= columns; c++)
		      {
               SkyVertex(r, c);
			      SkyVertex(r + 1, c);
		      }
		   glEnd();
      }
	}
}


void GL_DrawSky()
{
   float angle;
   FTexture *tex;
   bool drawBoth = false;

   int sky1tex, sky2tex;
   int s1p, s2p;

   drawBoth = !((level.flags & LEVEL_SWAPSKIES) || (sky2texture == sky1texture));

	if ((level.flags & LEVEL_SWAPSKIES) || (sky2texture == sky1texture))
	{
		sky1tex = sky2texture;
      s1p = sky2pos;
		sky2tex = sky1texture;
      s2p = sky1pos;
	}
	else
	{
		sky1tex = sky1texture;
      s1p = sky1pos;
		sky2tex = sky2texture;
      s2p = sky2pos;
	}

   glDepthMask(GL_FALSE);
   glDisable(GL_DEPTH_TEST);
   glDisable(GL_FOG);
   glDisable(GL_ALPHA_TEST);

   glPushMatrix();

   glTranslatef(0.f, -1000.f, 0.f);

   angle = s2p * 1.f / (FRACUNIT * 2.f);
   glRotatef(-angle, 0.f, 1.f, 0.f);

   textureList.BindTexture(sky2tex, true);
   tex = TexMan(sky2tex);
   texw = tex->GetWidth();
   texh = tex->GetHeight();
   tx = ty = 1.f;
   yMult = 1.f;

   GL_RenderSkyHemisphere(SKYHEMI_UPPER);
   GL_RenderSkyHemisphere(SKYHEMI_LOWER);

   if (drawBoth)
   {
      angle = (s1p * 1.f / (FRACUNIT * 2.f)) - angle;
      glRotatef(-angle, 0.f, 1.f, 0.f);
      tex = TexMan(sky1tex);
      textureList.BindTexture(sky1tex, true);
      texw = tex->GetWidth();
      texh = tex->GetHeight();
      if (level.flags & LEVEL_DOUBLESKY && texh <= 128)
      {
         yMult = 2.f;
      }
      tx = ty = 1.f;
      GL_RenderSkyHemisphere(SKYHEMI_UPPER);
      GL_RenderSkyHemisphere(SKYHEMI_LOWER);
   }

   glPopMatrix();

   glEnable(GL_DEPTH_TEST);
   glEnable(GL_ALPHA_TEST);
   glDepthMask(GL_TRUE);
}


void GL_GetSkyColor(float *r, float *g, float *b)
{
   int skyTex;

   if ((level.flags & LEVEL_SWAPSKIES) || (sky2texture == sky1texture))
	{
		skyTex = sky1texture;
	}
	else
	{
		skyTex = sky2texture;
	}

   textureList.GetTexture(skyTex, true);
   textureList.GetAverageColor(r, g, b);
}



//
// FSkyDome stuff
//


FSkyDome::FSkyDome()
{
}


FSkyDome::~FSkyDome()
{
}


void FSkyDome::Draw()
{
}

