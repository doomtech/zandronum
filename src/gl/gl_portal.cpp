/*
** gl_portal.cpp
**   Generalized portal maintenance classes for skyboxes, horizons etc.
**   Requires a stencil buffer!
**
**---------------------------------------------------------------------------
** Copyright 2004-2005 Christoph Oelckers
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
** 4. When not used as part of GZDoom or a GZDoom derivative, this code will be
**    covered by the terms of the GNU Lesser General Public License as published
**    by the Free Software Foundation; either version 2.1 of the License, or (at
**    your option) any later version.
** 5. Full disclosure of the entire project's source code, except for third
**    party libraries is mandatory. (NOTE: This clause is non-negotiable!)
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

#include "gl/gl_include.h"
#include "p_local.h"
#include "vectors.h"
#include "doomstat.h"
#include "gl/gl_struct.h"
#include "gl/gl_portal.h"
#include "gl/gl_clipper.h"
#include "gl/gl_glow.h"
#include "gl/gl_texture.h"
#include "gl/gl_functions.h"
#include "gl/gl_intern.h"
#include "gl/gl_basic.h"
#include "gl/gl_data.h"
#include "gl/gl_geometric.h"
#include "gl/gl_shader.h"

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//
//
// General portal handling code
//
//
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

CVAR(Bool, gl_portals, true, 0)

CUSTOM_CVAR(Int, r_mirror_recursions,4,CVAR_GLOBALCONFIG|CVAR_ARCHIVE)
{
	if (self<0) self=0;
	if (self>10) self=10;
}
bool gl_plane_reflection_i;	// This is needed in a header that cannot include the CVAR stuff...
CUSTOM_CVAR(Bool, gl_plane_reflection, true, CVAR_GLOBALCONFIG|CVAR_ARCHIVE)
{
	gl_plane_reflection_i = self;
}
TArray<GLPortal *> GLPortal::portals;
int GLPortal::recursion;
int GLPortal::MirrorFlag;
int GLPortal::PlaneMirrorFlag;
int GLPortal::renderdepth;
int GLPortal::PlaneMirrorMode;
GLuint GLPortal::QueryObject;

line_t * GLPortal::mirrorline;
bool	 GLPortal::inupperstack;
bool	 GLPortal::inlowerstack;
bool	 GLPortal::inskybox;

//==========================================================================
//
//
//
//==========================================================================
void GLPortal::ClearScreen()
{
	gl.MatrixMode(GL_MODELVIEW);
	gl.PushMatrix();
	gl.MatrixMode(GL_PROJECTION);
	gl.PushMatrix();
	screen->Begin2D(false);
	screen->Dim(0, 1.f, 0, 0, SCREENWIDTH, SCREENHEIGHT);
	gl.Enable(GL_DEPTH_TEST);
	gl.MatrixMode(GL_PROJECTION);
	gl.PopMatrix();
	gl.MatrixMode(GL_MODELVIEW);
	gl.PopMatrix();
}


//-----------------------------------------------------------------------------
//
// DrawPortalStencil
//
//-----------------------------------------------------------------------------
void GLPortal::DrawPortalStencil()
{
	for(unsigned int i=0;i<lines.Size();i++)
	{
		lines[i].RenderWall(0, NULL);

	}

	if (NeedCap() && lines.Size() > 1)
	{
		// Cap the stencil at the top and bottom 
		// (cheap ass version)
		gl.Begin(GL_TRIANGLE_FAN);
		gl.Vertex3f(-32767.0f,32767.0f,-32767.0f);
		gl.Vertex3f(-32767.0f,32767.0f, 32767.0f);
		gl.Vertex3f( 32767.0f,32767.0f, 32767.0f);
		gl.Vertex3f( 32767.0f,32767.0f,-32767.0f);
		gl.End();
		gl.Begin(GL_TRIANGLE_FAN);
		gl.Vertex3f(-32767.0f,-32767.0f,-32767.0f);
		gl.Vertex3f(-32767.0f,-32767.0f, 32767.0f);
		gl.Vertex3f( 32767.0f,-32767.0f, 32767.0f);
		gl.Vertex3f( 32767.0f,-32767.0f,-32767.0f);
		gl.End();
	}
}



//-----------------------------------------------------------------------------
//
// Start
//
//-----------------------------------------------------------------------------
CVAR(Bool, gl_noquery, false, 0)

bool GLPortal::Start(bool usestencil, bool doquery)
{
	PortalAll.Clock();
	if (usestencil)
	{
		if (!gl_portals) return false;
	
		// Create stencil 
		gl.StencilFunc(GL_EQUAL,recursion,~0);		// create stencil
		gl.StencilOp(GL_KEEP,GL_KEEP,GL_INCR);		// increment stencil of valid pixels
		gl.ColorMask(0,0,0,0);						// don't write to the graphics buffer
		gl.DepthMask(false);							// don't write to Z-buffer!
		gl_EnableTexture(false);
		gl.Color3f(1,1,1);
		gl.DepthFunc(GL_LESS);
		gl_DisableShader();

		if (NeedDepthBuffer())
		{
			if (gl_noquery) doquery = false;
			
			// If occlusion query is supported let's use it to avoid rendering portals that aren't visible
			if (doquery && gl.flags&RFL_OCCLUSION_QUERY)
			{
				if (!QueryObject) gl.GenQueries(1, &QueryObject);
				if (QueryObject) 
				{
					gl.BeginQuery(GL_SAMPLES_PASSED_ARB, QueryObject);
				}
				else doquery = false;	// some kind of error happened
					
			}

			DrawPortalStencil();

			if (doquery && gl.flags&RFL_OCCLUSION_QUERY)
			{
				gl.EndQuery(GL_SAMPLES_PASSED_ARB);
			}

			// Clear Z-buffer
			gl.StencilFunc(GL_EQUAL,recursion+1,~0);		// draw sky into stencil
			gl.StencilOp(GL_KEEP,GL_KEEP,GL_KEEP);		// this stage doesn't modify the stencil
			gl.DepthMask(true);							// enable z-buffer again
			gl.DepthRange(1,1);
			gl.DepthFunc(GL_ALWAYS);
			DrawPortalStencil();

			// set normal drawing mode
			gl_EnableTexture(true);
			gl.DepthFunc(GL_LESS);
			gl.ColorMask(1,1,1,1);
			gl.DepthRange(0,1);

			if (doquery && gl.flags&RFL_OCCLUSION_QUERY)
			{
				GLuint sampleCount;

				gl.GetQueryObjectuiv(QueryObject, GL_QUERY_RESULT_ARB, &sampleCount);

				if (sampleCount==0) 	// not visible
				{
					// restore default stencil op.
					gl.StencilOp(GL_KEEP,GL_KEEP,GL_KEEP);
					gl.StencilFunc(GL_EQUAL,recursion,~0);		// draw sky into stencil
					return false;
				}
			}
			GLDrawInfo::StartDrawInfo(NULL);
		}
		else
		{
			// No z-buffer is needed therefore we can skip all the complicated stuff that is involved
			// No occlusion queries will be done here. For these portals the overhead is far greater
			// than the benefit.

			DrawPortalStencil();
			gl.StencilFunc(GL_EQUAL,recursion+1,~0);		// draw sky into stencil
			gl.StencilOp(GL_KEEP,GL_KEEP,GL_KEEP);		// this stage doesn't modify the stencil
			gl_EnableTexture(true);
			gl.ColorMask(1,1,1,1);
			gl.Disable(GL_DEPTH_TEST);
		}
		recursion++;


	}
	else
	{
		if (NeedDepthBuffer())
		{
			GLDrawInfo::StartDrawInfo(NULL);
		}
		else
		{
			gl.DepthMask(false);
			gl.Disable(GL_DEPTH_TEST);
		}
	}
	// The clip plane from the previous portal must be deactivated for this one.
	clipsave = gl.IsEnabled(GL_CLIP_PLANE0+renderdepth-1);
	if (clipsave) gl.Disable(GL_CLIP_PLANE0+renderdepth-1);


	PortalAll.Unclock();

	// save viewpoint
	savedviewx=viewx;
	savedviewy=viewy;
	savedviewz=viewz;
	savedviewactor=viewactor;
	savedviewangle=viewangle;
	savedviewarea=in_area;
	mirrorline=NULL;
	return true;
}


inline void GLPortal::ClearClipper()
{
	fixed_t angleOffset = viewangle - savedviewangle;

	clipper.Clear();

	static int call=0;

	// Set the clipper to the minimal visible area
	clipper.AddClipRange(0,0xffffffff);
	for(unsigned int i=0;i<lines.Size();i++)
	{
		angle_t startAngle = R_PointToAngle2(savedviewx, savedviewy, 
												TO_MAP(lines[i].glseg.x2), TO_MAP(lines[i].glseg.y2));

		angle_t endAngle = R_PointToAngle2(savedviewx, savedviewy, 
												TO_MAP(lines[i].glseg.x1), TO_MAP(lines[i].glseg.y1));

		if (startAngle-endAngle>0) 
		{
			clipper.SafeRemoveClipRange(startAngle + angleOffset, endAngle + angleOffset);
		}
	}

	// and finally clip it to the visible area
	angle_t a1 = gl_FrustumAngle();
	if (a1<ANGLE_180) clipper.SafeAddClipRange(viewangle+a1, viewangle-a1);

}

//-----------------------------------------------------------------------------
//
// End
//
//-----------------------------------------------------------------------------
void GLPortal::End(bool usestencil)
{
	bool needdepth = NeedDepthBuffer();

	PortalAll.Clock();
	if (clipsave) gl.Enable (GL_CLIP_PLANE0+renderdepth-1);
	if (usestencil)
	{
		if (needdepth) GLDrawInfo::EndDrawInfo();

		// Restore the old view
		viewx=savedviewx;
		viewy=savedviewy;
		viewz=savedviewz;
		viewangle=savedviewangle;
		viewactor=savedviewactor;
		in_area=savedviewarea;
		gl_SetupView(viewx, viewy, viewz, viewangle, !!(MirrorFlag&1), !!(PlaneMirrorFlag&1));

		gl.Color4f(1,1,1,1);
		gl.ColorMask(0,0,0,0);						// no graphics
		gl.Color3f(1,1,1);
		gl_EnableTexture(false);
		gl_DisableShader();

		if (needdepth) 
		{
			// first step: reset the depth buffer to max. depth
			gl.DepthRange(1,1);							// always
			gl.DepthFunc(GL_ALWAYS);						// write the farthest depth value
			DrawPortalStencil();
		}
		else
		{
			gl.Enable(GL_DEPTH_TEST);
		}
		
		// second step: restore the depth buffer to the previous values and reset the stencil
		gl.DepthFunc(GL_LEQUAL);
		gl.DepthRange(0,1);
		gl.StencilOp(GL_KEEP,GL_KEEP,GL_DECR);
		gl.StencilFunc(GL_EQUAL,recursion,~0);		// draw sky into stencil
		DrawPortalStencil();
		gl.DepthFunc(GL_LESS);


		gl_EnableTexture(true);
		gl.ColorMask(1,1,1,1);
		recursion--;

		// restore old stencil op.
		gl.StencilOp(GL_KEEP,GL_KEEP,GL_KEEP);
		gl.StencilFunc(GL_EQUAL,recursion,~0);		// draw sky into stencil
	}
	else
	{
		if (needdepth) 
		{
			GLDrawInfo::EndDrawInfo();
			gl.Clear(GL_DEPTH_BUFFER_BIT);
		}
		else
		{
			gl.Enable(GL_DEPTH_TEST);
			gl.DepthMask(true);
		}
		// Restore the old view
		viewx=savedviewx;
		viewy=savedviewy;
		viewz=savedviewz;
		viewangle=savedviewangle;
		viewactor=savedviewactor;
		in_area=savedviewarea;
		gl_SetupView(viewx, viewy, viewz, viewangle, !!(MirrorFlag&1), !!(PlaneMirrorFlag&1));

		// This draws a valid z-buffer into the stencil's contents to ensure it
		// doesn't get overwritten by the level's geometry.

		gl.Color4f(1,1,1,1);
		gl.DepthFunc(GL_LEQUAL);
		gl.DepthRange(0,1);
		gl.ColorMask(0,0,0,0);						// no graphics
		gl_EnableTexture(false);
		DrawPortalStencil();
		gl_EnableTexture(true);
		gl.ColorMask(1,1,1,1);
		gl.DepthFunc(GL_LESS);
	}
	PortalAll.Unclock();
}


//-----------------------------------------------------------------------------
//
// StartFrame
//
//-----------------------------------------------------------------------------
void GLPortal::StartFrame()
{
	GLPortal * p=NULL;
	portals.Push(p);
	if (renderdepth==0)
	{
		inskybox=inupperstack=inlowerstack=false;
		mirrorline=NULL;
	}
	renderdepth++;
}


//-----------------------------------------------------------------------------
//
// EndFrame
//
//-----------------------------------------------------------------------------
void GLPortal::EndFrame()
{
	GLPortal * p;

	// Only use occlusion query if there are more than 2 portals. 
	// Otherwise there's too much overhead.
	bool usequery = portals.Size() > 2;

	while (portals.Pop(p) && p)
	{
		p->RenderPortal(true, usequery);
		delete p;
	}
	renderdepth--;
}


//-----------------------------------------------------------------------------
//
// Renders one sky portal without a stencil.
// In more complex scenes using a stencil for skies can severly stall
// the GPU and there's rarely more than one sky visible at a time.
//
//-----------------------------------------------------------------------------
bool GLPortal::RenderFirstSkyPortal(int recursion)
{
	GLPortal * p;
	GLPortal * best = NULL;
	unsigned bestindex=0;

	// Find the one with the highest amount of lines.
	// Normally this is also the one that saves the largest amount
	// of time by drawing it before the scene itself.
	for(unsigned i=portals.Size()-1;i>=0 && portals[i]!=NULL;i--)
	{
		p=portals[i];
		if (p->IsSky())
		{
			// Cannot clear the depth buffer inside a portal recursion
			if (recursion && p->NeedDepthBuffer()) continue;

			if (!best || p->lines.Size()>best->lines.Size())
			{
				best=p;
				bestindex=i;
			}
		}
	}

	if (best)
	{
		best->RenderPortal(false, false);
		portals.Delete(bestindex);

		delete best;
		return true;
	}
	return false;
}


//-----------------------------------------------------------------------------
//
// FindPortal
//
//-----------------------------------------------------------------------------
GLPortal * GLPortal::FindPortal(const void * src)
{
	int i=portals.Size()-1;

	while (i>=0 && portals[i] && portals[i]->GetSource()!=src) i--;
	return i>=0? portals[i]:NULL;
}



//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//
//
// Skybox Portal
//
//
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
//
// GLSkyboxPortal::DrawContents
//
//-----------------------------------------------------------------------------
static int skyboxrecursion=0;
void GLSkyboxPortal::DrawContents()
{
	int old_pm=PlaneMirrorMode;
	int saved_extralight = extralight;

	if (skyboxrecursion>=3)
	{
		ClearScreen();
		return;
	}

	skyboxrecursion++;
	origin->flags|=MF_JUSTHIT;
	extralight = 0;

	PlaneMirrorMode=0;

	gl.Disable(GL_DEPTH_CLAMP_NV);

	viewx = origin->x;
	viewy = origin->y;
	viewz = origin->z;

	// Don't let the viewpoint be too close to a floor or ceiling!
	fixed_t floorh = origin->Sector->floorplane.ZatPoint(origin->x, origin->y);
	fixed_t ceilh = origin->Sector->ceilingplane.ZatPoint(origin->x, origin->y);
	if (viewz<floorh+4*FRACUNIT) viewz=floorh+4*FRACUNIT;
	if (viewz>ceilh-4*FRACUNIT) viewz=ceilh-4*FRACUNIT;

	viewangle += origin->angle;

	viewactor = origin;

	validcount++;
	inskybox=true;
	gl_SetupView(viewx, viewy, viewz, viewangle, !!(MirrorFlag&1), !!(PlaneMirrorFlag&1));
	gl_SetViewArea();
	ClearClipper();
	gl_DrawScene();
	origin->flags&=~MF_JUSTHIT;
	inskybox=false;
	gl.Enable(GL_DEPTH_CLAMP_NV);
	skyboxrecursion--;

	PlaneMirrorMode=old_pm;
	extralight = saved_extralight;
}


//-----------------------------------------------------------------------------
//
// GLSectorStackPortal::DrawContents
//
//-----------------------------------------------------------------------------
void GLSectorStackPortal::DrawContents()
{
	viewx -= origin->deltax;
	viewy -= origin->deltay;
	viewz -= origin->deltaz;
	viewactor = NULL;

	validcount++;

	// avoid recursions!
	if (origin->isupper) inupperstack=true;
	else inlowerstack=true;

	gl_SetupView(viewx, viewy, viewz, viewangle, !!(MirrorFlag&1), !!(PlaneMirrorFlag&1));
	ClearClipper();
	gl_DrawScene();
}


//-----------------------------------------------------------------------------
//
// GLPlaneMirrorPortal::DrawContents
//
//-----------------------------------------------------------------------------
void GLPlaneMirrorPortal::DrawContents()
{
	if (renderdepth>r_mirror_recursions) 
	{
		ClearScreen();
		return;
	}

	int old_pm=PlaneMirrorMode;

	fixed_t planez = origin->ZatPoint(viewx, viewy);
	viewz = 2*planez - viewz;
	viewactor = NULL;
	PlaneMirrorMode = ksgn(origin->c);

	validcount++;

	PlaneMirrorFlag++;
	gl_SetupView(viewx, viewy, viewz, viewangle, !!(MirrorFlag&1), !!(PlaneMirrorFlag&1));
	ClearClipper();

	gl.Enable(GL_CLIP_PLANE0+renderdepth);
	// This only works properly for non-sloped planes so don't bother with the math.
	//double d[4]={origin->a/65536., origin->c/65536., origin->b/65536., TO_GL(origin->d)};
	double d[4]={0, PlaneMirrorMode, 0, TO_GL(origin->d)};
	gl.ClipPlane(GL_CLIP_PLANE0+renderdepth, d);

	gl_DrawScene();
	gl.Disable(GL_CLIP_PLANE0+renderdepth);
	PlaneMirrorFlag--;
	PlaneMirrorMode=old_pm;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//
//
// Mirror Portal
//
//
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
//
// R_EnterMirror
//
//-----------------------------------------------------------------------------
void GLMirrorPortal::DrawContents()
{
	if (renderdepth>r_mirror_recursions) 
	{
		ClearScreen();
		return;
	}

	mirrorline=linedef;
	angle_t startang = viewangle;
	fixed_t startx = viewx;
	fixed_t starty = viewy;

	vertex_t *v1 = mirrorline->v1;
	vertex_t *v2 = mirrorline->v2;

	// Reflect the current view behind the mirror.
	if (mirrorline->dx == 0)
	{ 
		// vertical mirror
		viewx = v1->x - startx + v1->x;

		// Compensation for reendering inaccuracies
		if (startx<v1->x)  viewx -= FRACUNIT/2;
		else viewx += FRACUNIT/2;
	}
	else if (mirrorline->dy == 0)
	{ 
		// horizontal mirror
		viewy = v1->y - starty + v1->y;

		// Compensation for reendering inaccuracies
		if (starty<v1->y)  viewy -= FRACUNIT/2;
		else viewy += FRACUNIT/2;
	}
	else
	{ 
		// any mirror--use floats to avoid integer overflow

		float dx = TO_GL(v2->x - v1->x);
		float dy = TO_GL(v2->y - v1->y);
		float x1 = TO_GL(v1->x);
		float y1 = TO_GL(v1->y);
		float x = TO_GL(startx);
		float y = TO_GL(starty);

		// the above two cases catch len == 0
		float r = ((x - x1)*dx + (y - y1)*dy) / (dx*dx + dy*dy);

		viewx = TO_MAP((x1 + r * dx)*2 - x);
		viewy = TO_MAP((y1 + r * dy)*2 - y);

		// Compensation for reendering inaccuracies
		FVector2 v(-dx, dy);
		v.MakeUnit();

		viewx+= TO_MAP(v[1] * renderdepth / 2);
		viewy+= TO_MAP(v[0] * renderdepth / 2);
	}
	viewangle = 2*R_PointToAngle2 (mirrorline->v1->x, mirrorline->v1->y,
										mirrorline->v2->x, mirrorline->v2->y) - startang;

	viewactor = NULL;

	validcount++;

	MirrorFlag++;
	gl_SetupView(viewx, viewy, viewz, viewangle, !!(MirrorFlag&1), !!(PlaneMirrorFlag&1));

	clipper.Clear();

	angle_t af = gl_FrustumAngle();
	if (af<ANGLE_180) clipper.SafeAddClipRange(viewangle+af, viewangle-af);

	// [BB] Spleen found out that the caching of the view angles doesn't work for mirrors
	// (kills performance and causes rendering defects).
	//angle_t a2 = mirrorline->v1->GetViewAngle();
	//angle_t a1 = mirrorline->v2->GetViewAngle();
	angle_t a2=R_PointToAngle(mirrorline->v1->x, mirrorline->v1->y);
	angle_t a1=R_PointToAngle(mirrorline->v2->x, mirrorline->v2->y);
	clipper.SafeAddClipRange(a1,a2);

	gl_DrawScene();

	MirrorFlag--;
}




//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//
//
// Horizon Portal
//
// This simply draws the area in medium sized squares. Drawing it as a whole
// polygon creates visible inaccuracies.
//
// Originally I tried to minimize the amount of data to be drawn but there
// are 2 problems with it:
//
// 1. Setting this up completely negates any performance gains.
// 2. It doesn't work with a 360° field of view (as when you are looking up.)
//
//
// So the brute force mechanism is just as good.
//
//
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//
// GLHorizonPortal::DrawContents
//
//-----------------------------------------------------------------------------
void GLHorizonPortal::DrawContents()
{
	PortalAll.Clock();

	GLSectorPlane * sp=&origin->plane;
	FGLTexture * gltexture;
	PalEntry color;
	float z;
	player_t * player=&players[consoleplayer];

	gltexture=FGLTexture::ValidateTexture(sp->texture);
	if (!gltexture) 
	{
		ClearScreen();
		return;
	}


	z=TO_GL(sp->texheight);


	if (gltexture && gltexture->tex->isFullbright())
	{
		// glowing textures are always drawn full bright without color
		gl_SetColor(255, 0, NULL, 1.f);
		gl_SetFog(255, 0, &origin->colormap, false);
	}
	else 
	{
		int rel = extralight * gl_weaponlight;
		gl_SetColor(origin->lightlevel, rel, &origin->colormap, 1.0f);
		gl_SetFog(origin->lightlevel, rel, &origin->colormap, false);
	}


	gltexture->Bind(origin->colormap.LightColor.a);
	gl_SetPlaneTextureRotation(sp, gltexture);

	gl.Disable(GL_ALPHA_TEST);
	gl.BlendFunc(GL_ONE,GL_ZERO);
	gl_ApplyShader();


	float vx=TO_GL(viewx);
	float vy=TO_GL(viewy);

	// Draw to some far away boundary
	for(float x=-32768+vx; x<32768+vx; x+=4096)
	{
		for(float y=-32768+vy; y<32768+vy;y+=4096)
		{
			gl.Begin(GL_TRIANGLE_FAN);

			gl.TexCoord2f(x/64, -y/64);
			gl.Vertex3f(x, z, y);

			gl.TexCoord2f(x/64 + 64, -y/64);
			gl.Vertex3f(x + 4096, z, y);

			gl.TexCoord2f(x/64 + 64, -y/64 - 64);
			gl.Vertex3f(x + 4096, z, y + 4096);

			gl.TexCoord2f(x/64, -y/64 - 64);
			gl.Vertex3f(x, z, y + 4096);

			gl.End();

		}
	}

	float vz=TO_GL(viewz);
	float tz=(z-vz);///64.0f;

	// fill the gap between the polygon and the true horizon
	// Since I can't draw into infinity there can always be a
	// small gap

	gl.Begin(GL_TRIANGLE_STRIP);

	gl.TexCoord2f(512.f, 0);
	gl.Vertex3f(-32768+vx, z, -32768+vy);
	gl.TexCoord2f(512.f, tz);
	gl.Vertex3f(-32768+vx, vz, -32768+vy);

	gl.TexCoord2f(-512.f, 0);
	gl.Vertex3f(-32768+vx, z,  32768+vy);
	gl.TexCoord2f(-512.f, tz);
	gl.Vertex3f(-32768+vx, vz,  32768+vy);

	gl.TexCoord2f(512.f, 0);
	gl.Vertex3f( 32768+vx, z,  32768+vy);
	gl.TexCoord2f(512.f, tz);
	gl.Vertex3f( 32768+vx, vz,  32768+vy);

	gl.TexCoord2f(-512.f, 0);
	gl.Vertex3f( 32768+vx, z, -32768+vy);
	gl.TexCoord2f(-512.f, tz);
	gl.Vertex3f( 32768+vx, vz, -32768+vy);

	gl.TexCoord2f(512.f, 0);
	gl.Vertex3f(-32768+vx, z, -32768+vy);
	gl.TexCoord2f(512.f, tz);
	gl.Vertex3f(-32768+vx, vz, -32768+vy);

	gl.End();

	gl.MatrixMode(GL_TEXTURE);
	gl.PopMatrix();
	gl.MatrixMode(GL_MODELVIEW);

	PortalAll.Unclock();

}

