/*
** glc_renderer.cpp
** Common renderer routines 
**
**---------------------------------------------------------------------------
** Copyright 2008 Christoph Oelckers
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
#include "gl/gl_framebuffer.h"
#include "r_local.h"
#include "i_system.h"
#include "r_main.h"
#include "g_level.h"
#include "templates.h"
#include "r_interpolate.h"
#include "gl/common/glc_renderer.h"
#include "gl/common/glc_clock.h"
#include "gl/common/glc_dynlight.h"
#include "gl/common/glc_convert.h"
#include "gl/common/glc_clipper.h"

// [BB] Clients may not alter gl_nearclip.
CUSTOM_CVAR(Int,gl_nearclip,5,CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	// [BB] Limit CVAR turbo on clients to 100.
	if ( ( NETWORK_GetState( ) == NETSTATE_CLIENT ) && ( self != 5 ) )
		self = 5;
}

EXTERN_CVAR (Int, screenblocks)
EXTERN_CVAR (Bool, cl_capfps)
EXTERN_CVAR (Bool, r_deathcamera)

void R_SetupFrame (AActor * camera);
extern int viewpitch;

area_t			in_area;

//-----------------------------------------------------------------------------
//
// R_FrustumAngle
//
//-----------------------------------------------------------------------------
angle_t GLRendererBase::FrustumAngle()
{
	float tilt= fabs(mAngles.Pitch);

	// If the pitch is larger than this you can look all around at a FOV of 90�
	if (tilt>46.0f) return 0xffffffff;

	// ok, this is a gross hack that barely works...
	// but at least it doesn't overestimate too much...
	double floatangle=2.0+(45.0+((tilt/1.9)))*mCurrentFoV*48.0/BaseRatioSizes[WidescreenRatio][3]/90.0;
	angle_t a1 = ANGLE_1*toint(floatangle);
	if (a1>=ANGLE_180) return 0xffffffff;
	return a1;
}

//-----------------------------------------------------------------------------
//
// Sets the area the camera is in
//
//-----------------------------------------------------------------------------
void GLRendererBase::SetViewArea()
{
	// The render_sector is better suited to represent the current position in GL
	viewsector = R_PointInSubsector(viewx, viewy)->render_sector;

	// keep the view within the render sector's floor and ceiling
	fixed_t theZ = viewsector->ceilingplane.ZatPoint (viewx, viewy) - 4*FRACUNIT;
	if (viewz > theZ)
	{
		viewz = theZ;
	}

	theZ = viewsector->floorplane.ZatPoint (viewx, viewy) + 4*FRACUNIT;
	if (viewz < theZ)
	{
		viewz = theZ;
	}

	// Get the heightsec state from the render sector, not the current one!
	if (viewsector->heightsec && !(viewsector->heightsec->MoreFlags & SECF_IGNOREHEIGHTSEC))
	{
		in_area = viewz<=viewsector->heightsec->floorplane.ZatPoint(viewx,viewy) ? area_below :
				   (viewz>viewsector->heightsec->ceilingplane.ZatPoint(viewx,viewy) &&
				   !(viewsector->heightsec->MoreFlags&SECF_FAKEFLOORONLY)) ? area_above:area_normal;
	}
	else
	{
		in_area=area_default;	// depends on exposed lower sectors
	}
}

//-----------------------------------------------------------------------------
//
// sets 3D viewport and initial state
//
//-----------------------------------------------------------------------------

void GLRendererBase::SetViewport(GL_IRECT *bounds)
{
	if (!bounds)
	{
		int height, width;

		// Special handling so the view with a visible status bar displays properly

		if (screenblocks >= 10)
		{
			height = SCREENHEIGHT;
			width  = SCREENWIDTH;
		}
		else
		{
			height = (screenblocks*SCREENHEIGHT/10) & ~7;
			width = (screenblocks*SCREENWIDTH/10);
		}

		int trueheight = static_cast<OpenGLFrameBuffer*>(screen)->GetTrueHeight();	// ugh...
		int bars = (trueheight-screen->GetHeight())/2; 

		int vw = viewwidth;
		int vh = viewheight;
		gl.Viewport(viewwindowx, trueheight-bars-(height+viewwindowy-((height-vh)/2)), vw, height);
		gl.Scissor(viewwindowx, trueheight-bars-(vh+viewwindowy), vw, vh);
	}
	else
	{
		gl.Viewport(bounds->left, bounds->top, bounds->width, bounds->height);
		gl.Scissor(bounds->left, bounds->top, bounds->width, bounds->height);
	}
	gl.Enable(GL_SCISSOR_TEST);
	
	#ifdef _DEBUG
		gl.ClearColor(0.0f, 0.0f, 0.0f, 0.0f); 
		gl.Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	#else
		gl.Clear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	#endif

	gl.Enable(GL_MULTISAMPLE);
	gl.Enable(GL_DEPTH_TEST);
	gl.Enable(GL_STENCIL_TEST);
	gl.StencilFunc(GL_ALWAYS,0,~0);	// default stencil
	gl.StencilOp(GL_KEEP,GL_KEEP,GL_REPLACE);
}

//-----------------------------------------------------------------------------
//
// Setup the camera position
//
//-----------------------------------------------------------------------------

void GLRendererBase::SetCameraPos(fixed_t viewx, fixed_t viewy, fixed_t viewz, angle_t viewangle)
{
	float fviewangle=(float)(viewangle>>ANGLETOFINESHIFT)*360.0f/FINEANGLES;

	GLRenderer->mAngles.Yaw = 270.0f-fviewangle;
	GLRenderer->mViewVector = FVector2(cos(DEG2RAD(fviewangle)), sin(DEG2RAD(fviewangle)));
	GLRenderer->mCameraPos = FVector3(TO_GL(viewx), TO_GL(viewy), TO_GL(viewz));
}
	
//-----------------------------------------------------------------------------
//
// Renders one viewpoint in a scene
//
//-----------------------------------------------------------------------------

sector_t * GLRendererBase::RenderViewpoint (AActor * camera, GL_IRECT * bounds, float fov, float ratio, float fovratio, bool mainview)
{       
	TThinkerIterator<ADynamicLight> it(STAT_DLIGHT);

	// Check if there's some lights. If not some code can be skipped.
	mLightCount = (it.Next()!=NULL);

	sector_t * retval;
	R_SetupFrame (camera);
	SetViewArea();
	mAngles.Pitch = clamp<float>((float)((double)(int)(viewpitch))/ANGLE_1, -90, 90);

	// Scroll the sky
	mSky1Pos = (float)fmod(gl_frameMS * level.skyspeed1, 1024.f) * 90.f/256.f;
	mSky2Pos = (float)fmod(gl_frameMS * level.skyspeed2, 1024.f) * 90.f/256.f;



	// [BB] consoleplayer should be able to toggle the chase cam.
	if (camera->player && /*camera->player-players==consoleplayer &&*/
		((/*camera->player->*/players[consoleplayer].cheats & CF_CHASECAM) || (r_deathcamera && camera->health <= 0)) && camera==camera->player->mo)
	{
		mViewActor=NULL;
	}
	else
	{
		mViewActor=camera;
	}

	retval = viewsector;

	SetViewport(bounds);
	mCurrentFoV = fov;
	SetProjection(fov, ratio, fovratio);	// switch to perspective mode and set up clipper
	SetCameraPos(viewx, viewy, viewz, viewangle);
	SetViewMatrix(false, false);

	clipper.Clear();
	angle_t a1 = GLRenderer->FrustumAngle();
	clipper.SafeAddClipRange(viewangle+a1, viewangle-a1);

	ProcessScene();

	interpolator.RestoreInterpolations ();
	return retval;
}


//-----------------------------------------------------------------------------
//
// renders the view
//
//-----------------------------------------------------------------------------

void GLRendererBase::RenderView (player_t* player)
{
	AActor *&LastCamera = static_cast<OpenGLFrameBuffer*>(screen)->LastCamera;

	if (player->camera != LastCamera)
	{
		// If the camera changed don't interpolate
		// Otherwise there will be some not so nice effects.
		R_ResetViewInterpolation();
		LastCamera=player->camera;
	}

	// reset statistics counters
	All.Reset();
	All.Clock();
	PortalAll.Reset();
	RenderAll.Reset();
	ProcessAll.Reset();
	RenderWall.Reset();
	SetupWall.Reset();
	ClipWall.Reset();
	RenderFlat.Reset();
	SetupFlat.Reset();
	RenderSprite.Reset();
	SetupSprite.Reset();

	flatvertices=flatprimitives=vertexcount=0;
	render_texsplit=render_vertexsplit=rendered_lines=rendered_flats=rendered_sprites=rendered_decals = 0;

	// Get this before everything else
	if (cl_capfps || r_NoInterpolate) r_TicFrac = FRACUNIT;
	else r_TicFrac = I_GetTimeFrac (&r_FrameTime);
	gl_frameMS = I_MSTime();

	R_FindParticleSubsectors ();

	// prepare all camera textures that have been used in the last frame
	FCanvasTextureInfo::UpdateAll();


	// I stopped using BaseRatioSizes here because the information there wasn't well presented.
	#define RMUL (1.6f/1.333333f)
	static float ratios[]={RMUL*1.333333f, RMUL*1.777777f, RMUL*1.6f, RMUL*1.333333f, RMUL*1.2f};

	// now render the main view
	float fovratio;
	float ratio = ratios[WidescreenRatio];
	if (!(WidescreenRatio&4))
	{
		fovratio = 1.6f;
	}
	else
	{
		fovratio = ratio;
	}

	SetFixedColormap (player);
	RenderMainView(player, FieldOfView * 360.0f / FINEANGLES, ratio, fovratio);

	All.Unclock();
}


void InvalidateSector(sector_t *sec, int mode)
{
	if (GLRenderer != NULL) GLRenderer->InvalidateSector(sec, mode);
}

void InvalidateSidedef(side_t *side, int mode)
{
	if (GLRenderer != NULL) GLRenderer->InvalidateSidedef(side, mode);
}

//-----------------------------------------------------------------------------
//
// Rendering statistics
//
//-----------------------------------------------------------------------------
ADD_STAT(rendertimes)
{
	static FString buff;
	static int lasttime=0;
	int t=I_MSTime();
	if (t-lasttime>1000) 
	{
		buff.Format("W: Render=%2.2f, Setup=%2.2f, Clip=%2.2f - F: Render=%2.2f, Setup=%2.2f - S: Render=%2.2f, Setup=%2.2f - All=%2.2f, Render=%2.2f, Setup=%2.2f, Portal=%2.2f, Finish=%2.2f\n",
		RenderWall.TimeMS(), SetupWall.TimeMS(), ClipWall.TimeMS(), RenderFlat.TimeMS(), SetupFlat.TimeMS(),
		RenderSprite.TimeMS(), SetupSprite.TimeMS(), All.TimeMS() + Finish.TimeMS(), RenderAll.TimeMS(),
		ProcessAll.TimeMS(), PortalAll.TimeMS(), Finish.TimeMS());
		lasttime=t;
	}
	return buff;
}

ADD_STAT(renderstats)
{
	FString out;
	out.Format("Walls: %d (%d splits, %d t-splits, %d vertices), Flats: %d (%d primitives, %d vertices), Sprites: %d, Decals=%d\n", 
		rendered_lines, render_vertexsplit, render_texsplit, vertexcount, rendered_flats, flatprimitives, flatvertices, rendered_sprites,rendered_decals );
	return out;
}
