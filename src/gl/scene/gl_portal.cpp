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

#include "gl/system/gl_system.h"
#include "p_local.h"
#include "vectors.h"
#include "c_dispatch.h"
#include "doomstat.h"
#include "a_sharedglobal.h"

#include "gl/system/gl_framebuffer.h"
#include "gl/system/gl_cvars.h"
#include "gl/renderer/gl_lightdata.h"
#include "gl/renderer/gl_renderstate.h"
#include "gl/dynlights/gl_glow.h"
#include "gl/data/gl_data.h"
#include "gl/scene/gl_clipper.h"
#include "gl/scene/gl_drawinfo.h"
#include "gl/scene/gl_portal.h"
#include "gl/shaders/gl_shader.h"
#include "gl/textures/gl_material.h"
#include "gl/utility/gl_clock.h"
#include "gl/utility/gl_templates.h"
#include "gl/utility/gl_geometric.h"

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//
//
// General portal handling code
//
//
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

EXTERN_CVAR(Bool, gl_portals)
EXTERN_CVAR(Bool, gl_noquery)
EXTERN_CVAR(Int, r_mirror_recursions)

TArray<GLPortal *> GLPortal::portals;
int GLPortal::recursion;
int GLPortal::MirrorFlag;
int GLPortal::PlaneMirrorFlag;
int GLPortal::renderdepth;
int GLPortal::PlaneMirrorMode;
GLuint GLPortal::QueryObject;

bool	 GLPortal::inupperstack;
bool	 GLPortal::inlowerstack;
bool	 GLPortal::inskybox;

UniqueList<GLSkyInfo> UniqueSkies;
UniqueList<GLHorizonInfo> UniqueHorizons;
UniqueList<GLSectorStackInfo> UniqueStacks;
UniqueList<secplane_t> UniquePlaneMirrors;



//==========================================================================
//
//
//
//==========================================================================

void GLPortal::BeginScene()
{
	UniqueSkies.Clear();
	UniqueHorizons.Clear();
	UniqueStacks.Clear();
	UniquePlaneMirrors.Clear();
}

//==========================================================================
//
//
//
//==========================================================================
void GLPortal::ClearScreen()
{
	bool multi = !!gl.IsEnabled(GL_MULTISAMPLE);
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
	if (multi) gl.Enable(GL_MULTISAMPLE);
	gl_RenderState.Set2DMode(false);
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

bool GLPortal::Start(bool usestencil, bool doquery)
{
	rendered_portals++;
	PortalAll.Clock();
	if (usestencil)
	{
		if (!gl_portals) 
		{
			PortalAll.Unclock();
			return false;
		}
	
		// Create stencil 
		gl.StencilFunc(GL_EQUAL,recursion,~0);		// create stencil
		gl.StencilOp(GL_KEEP,GL_KEEP,GL_INCR);		// increment stencil of valid pixels
		gl.ColorMask(0,0,0,0);						// don't write to the graphics buffer
		gl_RenderState.EnableTexture(false);
		gl.Color3f(1,1,1);
		gl.DepthFunc(GL_LESS);
		gl_RenderState.Apply(true);

		if (NeedDepthBuffer())
		{
			gl.DepthMask(false);							// don't write to Z-buffer!
			if (!NeedDepthBuffer()) doquery = false;		// too much overhead and nothing to gain.
			else if (gl_noquery) doquery = false;
			
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
			gl_RenderState.EnableTexture(true);
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
					PortalAll.Unclock();
					return false;
				}
			}
			FDrawInfo::StartDrawInfo();
		}
		else
		{
			// No z-buffer is needed therefore we can skip all the complicated stuff that is involved
			// No occlusion queries will be done here. For these portals the overhead is far greater
			// than the benefit.
			// Note: We must draw the stencil with z-write enabled here because there is no second pass!

			gl.DepthMask(true);
			DrawPortalStencil();
			gl.StencilFunc(GL_EQUAL,recursion+1,~0);		// draw sky into stencil
			gl.StencilOp(GL_KEEP,GL_KEEP,GL_KEEP);		// this stage doesn't modify the stencil
			gl_RenderState.EnableTexture(true);
			gl.ColorMask(1,1,1,1);
			gl.Disable(GL_DEPTH_TEST);
			gl.DepthMask(false);							// don't write to Z-buffer!
		}
		recursion++;


	}
	else
	{
		if (NeedDepthBuffer())
		{
			FDrawInfo::StartDrawInfo();
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

	// save viewpoint
	savedviewx=viewx;
	savedviewy=viewy;
	savedviewz=viewz;
	savedviewactor=GLRenderer->mViewActor;
	savedviewangle=viewangle;
	savedviewarea=in_area;
	PortalAll.Unclock();
	return true;
}


inline void GLPortal::ClearClipper()
{
	fixed_t angleOffset = viewangle - savedviewangle;

	clipper.Clear();

	static int call=0;

	// Set the clipper to the minimal visible area
	clipper.SafeAddClipRange(0,0xffffffff);
	for(unsigned int i=0;i<lines.Size();i++)
	{
		angle_t startAngle = R_PointToAnglePrecise(savedviewx, savedviewy, 
												FLOAT2FIXED(lines[i].glseg.x2), FLOAT2FIXED(lines[i].glseg.y2));

		angle_t endAngle = R_PointToAnglePrecise(savedviewx, savedviewy, 
												FLOAT2FIXED(lines[i].glseg.x1), FLOAT2FIXED(lines[i].glseg.y1));

		if (startAngle-endAngle>0) 
		{
			clipper.SafeRemoveClipRangeRealAngles(startAngle + angleOffset, endAngle + angleOffset);
		}
	}

	// and finally clip it to the visible area
	angle_t a1 = GLRenderer->FrustumAngle();
	if (a1<ANGLE_180) clipper.SafeAddClipRangeRealAngles(viewangle+a1, viewangle-a1);

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
		if (needdepth) FDrawInfo::EndDrawInfo();

		// Restore the old view
		viewx=savedviewx;
		viewy=savedviewy;
		viewz=savedviewz;
		viewangle=savedviewangle;
		GLRenderer->mViewActor=savedviewactor;
		in_area=savedviewarea;
		GLRenderer->SetupView(viewx, viewy, viewz, viewangle, !!(MirrorFlag&1), !!(PlaneMirrorFlag&1));

		gl.Color4f(1,1,1,1);
		gl.ColorMask(0,0,0,0);						// no graphics
		gl.Color3f(1,1,1);
		gl_RenderState.EnableTexture(false);
		gl_RenderState.Apply(true);

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


		gl_RenderState.EnableTexture(true);
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
			FDrawInfo::EndDrawInfo();
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
		GLRenderer->mViewActor=savedviewactor;
		in_area=savedviewarea;
		GLRenderer->SetupView(viewx, viewy, viewz, viewangle, !!(MirrorFlag&1), !!(PlaneMirrorFlag&1));

		// This draws a valid z-buffer into the stencil's contents to ensure it
		// doesn't get overwritten by the level's geometry.

		gl.Color4f(1,1,1,1);
		gl.DepthFunc(GL_LEQUAL);
		gl.DepthRange(0,1);
		gl.ColorMask(0,0,0,0);						// no graphics
		gl_RenderState.EnableTexture(false);
		DrawPortalStencil();
		gl_RenderState.EnableTexture(true);
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
	}
	renderdepth++;
}


//-----------------------------------------------------------------------------
//
// Portal info
//
//-----------------------------------------------------------------------------

static bool gl_portalinfo;

CCMD(gl_portalinfo)
{
	gl_portalinfo = true;
}

FString indent;

//-----------------------------------------------------------------------------
//
// EndFrame
//
//-----------------------------------------------------------------------------

void GLPortal::EndFrame()
{
	GLPortal * p;

	if (gl_portalinfo)
	{
		Printf("%s%d portals, depth = %d\n%s{\n", indent.GetChars(), portals.Size(), renderdepth, indent.GetChars());
		indent += "  ";
	}

	// Only use occlusion query if there are more than 2 portals. 
	// Otherwise there's too much overhead.
	// (And don't forget to consider the separating NULL pointers!)
	bool usequery = portals.Size() > 2 + renderdepth;

	while (portals.Pop(p) && p)
	{
		if (gl_portalinfo) 
		{
			Printf("%sProcessing %s, depth = %d, query = %d\n", indent.GetChars(), p->GetName(), renderdepth, usequery);
		}

		p->RenderPortal(true, usequery);
		delete p;
	}
	renderdepth--;

	if (gl_portalinfo)
	{
		indent.Truncate(long(indent.Len()-2));
		Printf("%s}\n", indent.GetChars());
		if (portals.Size() == 0) gl_portalinfo = false;
	}
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

	viewx = origin->PrevX + FixedMul(r_TicFrac, origin->x - origin->PrevX);
	viewy = origin->PrevY + FixedMul(r_TicFrac, origin->y - origin->PrevY);
	viewz = origin->PrevZ + FixedMul(r_TicFrac, origin->z - origin->PrevZ);
	viewangle += origin->PrevAngle + FixedMul(r_TicFrac, origin->angle - origin->PrevAngle);

	// Don't let the viewpoint be too close to a floor or ceiling!
	fixed_t floorh = origin->Sector->floorplane.ZatPoint(origin->x, origin->y);
	fixed_t ceilh = origin->Sector->ceilingplane.ZatPoint(origin->x, origin->y);
	if (viewz<floorh+4*FRACUNIT) viewz=floorh+4*FRACUNIT;
	if (viewz>ceilh-4*FRACUNIT) viewz=ceilh-4*FRACUNIT;


	GLRenderer->mViewActor = origin;

	validcount++;
	inskybox=true;
	GLRenderer->SetupView(viewx, viewy, viewz, viewangle, !!(MirrorFlag&1), !!(PlaneMirrorFlag&1));
	GLRenderer->SetViewArea();
	ClearClipper();
	GLRenderer->DrawScene();
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
	FPortal *portal = &::portals[origin->origin->special1];
	portal->UpdateClipAngles();

	viewx += origin->origin->x - origin->origin->Mate->x;
	viewy += origin->origin->y - origin->origin->Mate->y;
	GLRenderer->mViewActor = NULL;
	GLRenderer->mCurrentPortal = this;


	validcount++;

	// avoid recursions!
	if (origin->isupper) inupperstack=true;
	else inlowerstack=true;

	GLRenderer->SetupView(viewx, viewy, viewz, viewangle, !!(MirrorFlag&1), !!(PlaneMirrorFlag&1));
	ClearClipper();
	GLRenderer->DrawScene();
}

//-----------------------------------------------------------------------------
//
// GLSectorStackPortal::ClipSeg
//
//-----------------------------------------------------------------------------
int GLSectorStackPortal::ClipSeg(seg_t *seg) 
{ 
	FPortal *portal = &::portals[origin->origin->special1];
	angle_t *angles = &portal->ClipAngles[0];
	unsigned numpoints = portal->ClipAngles.Size()-1;
	angle_t clipangle = seg->v1->GetClipAngle();
	unsigned i, j;
	int relation;

	// Check the front side of the portal. Anything in front of the shape must be discarded by the clipper.
	for(i=0;i<numpoints; i++)
	{
		if (angles[i+1] - clipangle <= ANGLE_180 && angles[i] - clipangle > ANGLE_180 && angles[i+1] - angles[i] < ANGLE_180)
		{
			relation = DMulScale32(seg->v1->y - portal->Shape[i]->y - portal->yDisplacement, portal->Shape[i+1]->x - portal->Shape[i]->x,
				portal->Shape[i]->x - seg->v1->x + portal->xDisplacement, portal->Shape[i+1]->y - portal->Shape[i]->y);
			if (relation > 0) 
			{
				return PClip_InFront;
			}
			else if (relation == 0)
			{
				// If this vertex is on the boundary we need to check the second one, too. The line may be partially inside the shape
				// but outside the portal. We can use the same boundary line for this
				relation = DMulScale32(seg->v2->y - portal->Shape[i]->y - portal->yDisplacement, portal->Shape[i+1]->x - portal->Shape[i]->x,
					portal->Shape[i]->x - seg->v2->x + portal->xDisplacement, portal->Shape[i+1]->y - portal->Shape[i]->y);
				if (relation >= 0) return PClip_InFront;
			}
			return PClip_Inside;
		}
	}
	
	// The first vertex did not yield any useful result. Check the second one.
	clipangle = seg->v2->GetClipAngle();
	for(j=0;j<numpoints; j++)
	{
		if (angles[j+1] - clipangle <= ANGLE_180 && angles[j] - clipangle > ANGLE_180 && angles[j+1] - angles[j] < ANGLE_180)
		{
			relation = DMulScale32(seg->v2->y - portal->Shape[j]->y - portal->yDisplacement, portal->Shape[j+1]->x - portal->Shape[j]->x,
				portal->Shape[j]->x - seg->v1->x + portal->xDisplacement, portal->Shape[j+1]->y - portal->Shape[j]->y);
			if (relation > 0) 
			{
				return PClip_InFront;
			}
			else if (relation == 0)
			{
				// If this vertex is on the boundary we need to check the second one, too. The line may be partially inside the shape
				// but outside the portal. We can use the same boundary line for this
				relation = DMulScale32(seg->v2->y - portal->Shape[j]->y - portal->yDisplacement, portal->Shape[j+1]->x - portal->Shape[j]->x,
					portal->Shape[j]->x - seg->v2->x + portal->xDisplacement, portal->Shape[j+1]->y - portal->Shape[j]->y);
				if (relation >= 0) return PClip_InFront;
			}
			return PClip_Inside;
		}
	}
	return PClip_Inside;	// The viewpoint is inside the portal

#if 0
	// Check backside of portal
	for(i=0;i<numpoints; i++)
	{
		if (angles[i+1] - clipangle > ANGLE_180 && angles[i] - clipangle <= ANGLE_180)
		{
			relation1 = DMulScale32(seg->v1->y - portal->Shape[i]->y - portal->yDisplacement, portal->Shape[i+1]->x - portal->Shape[i]->x,
				portal->Shape[i]->x - seg->v1->x + portal->xDisplacement, portal->Shape[i+1]->y - portal->Shape[i]->y);
			if (relation1 < 0) return PClip_Inside;
			// If this vertex is inside we need to check the second one, too. The line may be partially inside the shape
			// but outside the portal.
			break;
		}
	}

	clipangle = seg->v2->GetClipAngle();
	for(j=0;j<numpoints; j++)
	{
		if (angles[j+1] - clipangle > ANGLE_180 && angles[j] - clipangle <= ANGLE_180)
		{
			relation2 = DMulScale32(seg->v2->y - portal->Shape[j]->y - portal->yDisplacement, portal->Shape[j+1]->x - portal->Shape[j]->x,
				portal->Shape[j]->x - seg->v2->x + portal->xDisplacement, portal->Shape[j+1]->y - portal->Shape[j]->y);
			if (relation2 < 0) return PClip_Inside;
			break;
		}
	}
	return PClip_Behind;
#endif
}


//-----------------------------------------------------------------------------
//
// GLSectorStackPortal::ClipPoint
//
//-----------------------------------------------------------------------------
int GLSectorStackPortal::ClipPoint(fixed_t x, fixed_t y) 
{ 
	FPortal *portal = &::portals[origin->origin->special1];
	angle_t *angles = &portal->ClipAngles[0];
	unsigned numpoints = portal->ClipAngles.Size()-1;
	angle_t clipangle = R_PointToPseudoAngle(viewx, viewy, x, y);
	unsigned i;

	for(i=0;i<numpoints; i++)
	{
		if (clipangle >= angles[i] && clipangle < angles[i+1])
		{
			int relation = DMulScale32(y - portal->Shape[i]->y, portal->Shape[i+1]->x - portal->Shape[i]->x,
				portal->Shape[i]->x - x, portal->Shape[i+1]->y - portal->Shape[i]->y);
			if (relation > 0) return PClip_InFront;
			return PClip_Inside;
		}
	}
	return PClip_Inside;
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
	GLRenderer->mViewActor = NULL;
	PlaneMirrorMode = ksgn(origin->c);

	validcount++;

	PlaneMirrorFlag++;
	GLRenderer->SetupView(viewx, viewy, viewz, viewangle, !!(MirrorFlag&1), !!(PlaneMirrorFlag&1));
	ClearClipper();

	gl.Enable(GL_CLIP_PLANE0+renderdepth);
	// This only works properly for non-sloped planes so don't bother with the math.
	//double d[4]={origin->a/65536., origin->c/65536., origin->b/65536., FIXED2FLOAT(origin->d)};
	double d[4]={0, PlaneMirrorMode, 0, FIXED2FLOAT(origin->d)};
	gl.ClipPlane(GL_CLIP_PLANE0+renderdepth, d);

	GLRenderer->DrawScene();
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

	GLRenderer->mCurrentPortal = this;
	angle_t startang = viewangle;
	fixed_t startx = viewx;
	fixed_t starty = viewy;

	vertex_t *v1 = linedef->v1;
	vertex_t *v2 = linedef->v2;

	// Reflect the current view behind the mirror.
	if (linedef->dx == 0)
	{ 
		// vertical mirror
		viewx = v1->x - startx + v1->x;

		// Compensation for reendering inaccuracies
		if (startx<v1->x)  viewx -= FRACUNIT/2;
		else viewx += FRACUNIT/2;
	}
	else if (linedef->dy == 0)
	{ 
		// horizontal mirror
		viewy = v1->y - starty + v1->y;

		// Compensation for reendering inaccuracies
		if (starty<v1->y)  viewy -= FRACUNIT/2;
		else viewy += FRACUNIT/2;
	}
	else
	{ 
		// any mirror--use floats to avoid integer overflow. 
		// Use doubles to avoid losing precision which is very important here.

		double dx = FIXED2FLOAT(v2->x - v1->x);
		double dy = FIXED2FLOAT(v2->y - v1->y);
		double x1 = FIXED2FLOAT(v1->x);
		double y1 = FIXED2FLOAT(v1->y);
		double x = FIXED2FLOAT(startx);
		double y = FIXED2FLOAT(starty);

		// the above two cases catch len == 0
		double r = ((x - x1)*dx + (y - y1)*dy) / (dx*dx + dy*dy);

		viewx = FLOAT2FIXED((x1 + r * dx)*2 - x);
		viewy = FLOAT2FIXED((y1 + r * dy)*2 - y);

		// Compensation for reendering inaccuracies
		FVector2 v(-dx, dy);
		v.MakeUnit();

		viewx+= FLOAT2FIXED(v[1] * renderdepth / 2);
		viewy+= FLOAT2FIXED(v[0] * renderdepth / 2);
	}
	// we cannot afford any imprecisions caused by R_PointToAngle2 here. They'd be visible as seams around the mirror.
	viewangle = 2*R_PointToAnglePrecise (linedef->v1->x, linedef->v1->y,
										linedef->v2->x, linedef->v2->y) - startang;

	GLRenderer->mViewActor = NULL;

	validcount++;

	MirrorFlag++;
	GLRenderer->SetupView(viewx, viewy, viewz, viewangle, !!(MirrorFlag&1), !!(PlaneMirrorFlag&1));

	clipper.Clear();

	angle_t af = GLRenderer->FrustumAngle();
	if (af<ANGLE_180) clipper.SafeAddClipRangeRealAngles(viewangle+af, viewangle-af);

	// [BB] Spleen found out that the caching of the view angles doesn't work for mirrors
	// (kills performance and causes rendering defects).
	//angle_t a2 = linedef->v1->GetClipAngle();
	//angle_t a1 = linedef->v2->GetClipAngle();
	angle_t a2=R_PointToAngle(linedef->v1->x, linedef->v1->y);
	angle_t a1=R_PointToAngle(linedef->v2->x, linedef->v2->y);
	clipper.SafeAddClipRange(a1,a2);

	GLRenderer->DrawScene();

	MirrorFlag--;
}


int GLMirrorPortal::ClipSeg(seg_t *seg) 
{ 
	// this seg is completely behind the mirror!
	if (P_PointOnLineSide(seg->v1->x, seg->v1->y, linedef) &&
		P_PointOnLineSide(seg->v2->x, seg->v2->y, linedef)) 
	{
		return PClip_InFront;
	}
	return PClip_Inside; 
}

int GLMirrorPortal::ClipPoint(fixed_t x, fixed_t y) 
{ 
	if (P_PointOnLineSide(x, y, linedef)) 
	{
		return PClip_InFront;
	}
	return PClip_Inside; 
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
// 2. It doesn't work with a 360� field of view (as when you are looking up.)
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
	FMaterial * gltexture;
	PalEntry color;
	float z;
	player_t * player=&players[consoleplayer];

	gltexture=FMaterial::ValidateTexture(sp->texture, true);
	if (!gltexture) 
	{
		ClearScreen();
		PortalAll.Unclock();
		return;
	}


	z=FIXED2FLOAT(sp->texheight);


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


	gltexture->Bind(origin->colormap.colormap);

	gl_RenderState.EnableAlphaTest(false);
	gl_RenderState.BlendFunc(GL_ONE,GL_ZERO);
	gl_RenderState.Apply();


	bool pushed = gl_SetPlaneTextureRotation(sp, gltexture);

	float vx=FIXED2FLOAT(viewx);
	float vy=FIXED2FLOAT(viewy);

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

	float vz=FIXED2FLOAT(viewz);
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

	if (pushed)
	{
		gl.PopMatrix();
		gl.MatrixMode(GL_MODELVIEW);
	}

	PortalAll.Unclock();

}

const char *GLSkyPortal::GetName() { return "Sky"; }
const char *GLSkyboxPortal::GetName() { return "Skybox"; }
const char *GLSectorStackPortal::GetName() { return "Sectorstack"; }
const char *GLPlaneMirrorPortal::GetName() { return "Planemirror"; }
const char *GLMirrorPortal::GetName() { return "Mirror"; }
const char *GLHorizonPortal::GetName() { return "Horizon"; }
