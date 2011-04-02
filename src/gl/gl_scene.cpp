/*
** gl_scene.cpp
** manages the rendering of the player's view
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
#include "gi.h"
#include "m_png.h"
#include "st_stuff.h"
#include "dobject.h"
#include "doomstat.h"
#include "g_level.h"
#include "r_interpolate.h"
#include "gl/gl_struct.h"
#include "gl/gl_renderstruct.h"
#include "gl/gl_portal.h"
#include "gl/gl_clipper.h"
#include "gl/gl_lights.h"
#include "gl/gl_data.h"
#include "gl/gl_texture.h"
#include "gl/gl_basic.h"
#include "gl/gl_functions.h"
#include "gl/gl_shader.h"
#include "gl/gl_framebuffer.h"
#include "gl/gl_models.h"

//==========================================================================
//
//
//
//==========================================================================
CVAR(Bool, gl_texture, true, 0)
CVAR(Bool, gl_no_skyclear, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
// [BB] Clients may not alter gl_nearclip.
CUSTOM_CVAR(Int,gl_nearclip,5,CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	// [BB] Limit CVAR turbo on clients to 100.
	if ( ( NETWORK_GetState( ) == NETSTATE_CLIENT ) && ( self != 5 ) )
		self = 5;
}
CVAR(Float, gl_mask_threshold, 0.5f,CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR(Bool, gl_forcemultipass, false, 0)

void R_SetupFrame (AActor * camera);
extern int viewpitch;
int gl_anglecache;
 
int rendered_lines,rendered_flats,rendered_sprites,render_vertexsplit,render_texsplit,rendered_decals;
int iter_dlightf, iter_dlight, draw_dlight, draw_dlightf;
glcycle_t RenderWall,SetupWall,ClipWall;
glcycle_t RenderFlat,SetupFlat;
glcycle_t RenderSprite,SetupSprite;
glcycle_t All, Finish, PortalAll;
int vertexcount, flatvertices, flatprimitives;
int palette_brightness;
long gl_frameMS;
int gl_spriteindex;
float gl_sky1pos, gl_sky2pos;
int gl_lightcount;

glcycle_t ProcessAll;
glcycle_t RenderAll;

extern UniqueList<GLSkyInfo> UniqueSkies;
extern UniqueList<GLHorizonInfo> UniqueHorizons;
extern UniqueList<GLSectorStackInfo> UniqueStacks;
extern UniqueList<secplane_t> UniquePlaneMirrors;


EXTERN_CVAR (Bool, cl_capfps)
EXTERN_CVAR (Bool, r_deathcamera)
CVAR(Bool, gl_blendcolormaps, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)


float viewvecX,viewvecY;

float roll     = 0.0f;
float yaw      = 0.0f;
float pitch    = 0.0f;

DWORD			gl_fixedcolormap;
float			currentFoV;
AActor *		viewactor;
area_t			in_area;

//-----------------------------------------------------------------------------
//
// R_FrustumAngle
//
//-----------------------------------------------------------------------------
angle_t gl_FrustumAngle()
{
	float tilt= (float)fabs(((double)(int)(viewpitch))/ANGLE_1);
	if (tilt>90.0f) tilt=90.0f;

	// If the pitch is larger than this you can look all around at a FOV of 90°
	if (abs(viewpitch)>46*ANGLE_1) return 0xffffffff;


	// ok, this is a gross hack that barely works...
	// but at least it doesn't overestimate too much...
	double floatangle=2.0+(45.0+((tilt/1.9)))*currentFoV*48.0/BaseRatioSizes[WidescreenRatio][3]/90.0;
	angle_t a1 = ANGLE_1*toint(floatangle);
	if (a1>=ANGLE_180) return 0xffffffff;
	return a1;
}



//==========================================================================
//
// SV_AddBlend
// [RH] This is from Q2.
//
//==========================================================================
static void gl_AddBlend (float r, float g, float b, float a, float v_blend[4])
{
	float a2, a3;

	if (a <= 0)
		return;
	a2 = v_blend[3] + (1-v_blend[3])*a;	// new total alpha
	a3 = v_blend[3]/a2;		// fraction of color from old

	v_blend[0] = v_blend[0]*a3 + r*(1-a3);
	v_blend[1] = v_blend[1]*a3 + g*(1-a3);
	v_blend[2] = v_blend[2]*a3 + b*(1-a3);
	v_blend[3] = a2;
}




void gl_ResetViewport()
{
	int trueheight = static_cast<OpenGLFrameBuffer*>(screen)->GetTrueHeight();	// ugh...
	gl.Viewport(0, (trueheight-screen->GetHeight())/2, screen->GetWidth(), screen->GetHeight()); 
}

//-----------------------------------------------------------------------------
//
// gl_StartDrawScene
// sets 3D viewport and initializes hardware for 3D rendering
//
//-----------------------------------------------------------------------------
static void gl_StartDrawScene(GL_IRECT * bounds, float fov, float ratio, float fovratio)
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
	
	gl.MatrixMode(GL_PROJECTION);
	gl.LoadIdentity();

	float fovy = 2 * RAD2DEG(atan(tan(DEG2RAD(fov) / 2) / fovratio));
	gluPerspective(fovy, ratio, (float)gl_nearclip, 65536.f);

	// reset statistics counters
	render_texsplit=render_vertexsplit=rendered_lines=rendered_flats=rendered_sprites=rendered_decals = 0;
	iter_dlightf = iter_dlight = draw_dlight = draw_dlightf = 0;
	RenderWall.Reset();
	SetupWall.Reset();
	ClipWall.Reset();
	RenderFlat.Reset();
	SetupFlat.Reset();
	RenderSprite.Reset();
	SetupSprite.Reset();

	gl.EnableClientState(GL_TEXTURE_COORD_ARRAY);
	gl.EnableClientState(GL_VERTEX_ARRAY);

	UniqueSkies.Clear();
	UniqueHorizons.Clear();
	UniqueStacks.Clear();
	UniquePlaneMirrors.Clear();
	currentFoV=fov;
}



//-----------------------------------------------------------------------------
//
// gl_SetupView
// Setup the view rotation matrix for the given viewpoint
//
//-----------------------------------------------------------------------------
void gl_SetupView(fixed_t viewx, fixed_t viewy, fixed_t viewz, angle_t viewangle, bool mirror, bool planemirror, bool nosectorclear)
{
	float fviewangle=(float)(viewangle>>ANGLETOFINESHIFT)*360.0f/FINEANGLES;
	float xCamera,yCamera;
	float zCamera;
	
	yaw=270.0f-fviewangle;
	
	viewvecY= sin(DEG2RAD(fviewangle));
	viewvecX= cos(DEG2RAD(fviewangle));

	// Player coordinates
	xCamera=TO_GL(viewx);
	yCamera=TO_GL(viewy);
	zCamera=TO_GL(viewz);
	
	gl.MatrixMode(GL_MODELVIEW);
	gl.LoadIdentity();

	float mult = mirror? -1:1;
	float planemult = planemirror? -1:1;

	gl.Rotatef(roll,  0.0f, 0.0f, 1.0f);
	gl.Rotatef(pitch, 1.0f, 0.0f, 0.0f);
	gl.Rotatef(yaw,   0.0f, mult, 0.0f);
	gl.Translatef( xCamera*mult, -zCamera*planemult, -yCamera);
	gl.Scalef(-mult, planemult, 1);
}

//-----------------------------------------------------------------------------
//
// Sets the area the camera is in
//
//-----------------------------------------------------------------------------
void gl_SetViewArea()
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
// ProcessScene
//
// Processes the current scene and creates the draw lists
//
//-----------------------------------------------------------------------------

static void ProcessScene()
{
	// reset the portal manager
	GLPortal::StartFrame();

	ProcessAll.Clock();

	// clip the scene and fill the drawlists
	gl_anglecache++;	// Need to recalculate vertex angles for new scene
	gl_spriteindex=0;
	gl_RenderBSPNode (nodes + numnodes - 1);

	// And now the crappy hacks that have to be done to avoid rendering anomalies:

	gl_drawinfo->HandleMissingTextures();	// Missing upper/lower textures
	gl_drawinfo->HandleHackedSubsectors();	// open sector hacks for deep water
	gl_drawinfo->ProcessSectorStacks();		// merge visplanes of sector stacks

	ProcessAll.Unclock();
}

//-----------------------------------------------------------------------------
//
// RenderScene
//
// Draws the current draw lists for the non GLSL renderer
//
//-----------------------------------------------------------------------------

static void RenderScene(int recursion)
{
	RenderAll.Clock();

	if (!gl_no_skyclear) GLPortal::RenderFirstSkyPortal(recursion);

	gl_SetCamera(TO_GL(viewx), TO_GL(viewy), TO_GL(viewz));

	gl_EnableFog(true);
	gl.BlendFunc(GL_ONE,GL_ZERO);

	// First draw all single-pass stuff

	// Part 1: solid geometry. This is set up so that there are no transparent parts
	gl.DepthFunc(GL_LESS);


	gl.Disable(GL_ALPHA_TEST);

	gl.Disable(GL_POLYGON_OFFSET_FILL);	// just in case

	gl_EnableTexture(gl_texture);
	gl_EnableBrightmap(true);
	gl_drawinfo->drawlists[GLDL_PLAIN].Sort();
	gl_drawinfo->drawlists[GLDL_PLAIN].Draw(gl_texture? GLPASS_PLAIN : GLPASS_BASE);
	gl_EnableBrightmap(false);
	gl_drawinfo->drawlists[GLDL_FOG].Sort();
	gl_drawinfo->drawlists[GLDL_FOG].Draw(gl_texture? GLPASS_PLAIN : GLPASS_BASE);
	gl_drawinfo->drawlists[GLDL_LIGHTFOG].Sort();
	gl_drawinfo->drawlists[GLDL_LIGHTFOG].Draw(gl_texture? GLPASS_PLAIN : GLPASS_BASE);


	gl.Enable(GL_ALPHA_TEST);

	// Part 2: masked geometry. This is set up so that only pixels with alpha>0.5 will show
	if (!gl_texture) 
	{
		gl_EnableTexture(true);
		gl_SetTextureMode(TM_MASK);
	}
	gl.AlphaFunc(GL_GEQUAL,gl_mask_threshold);
	gl_EnableBrightmap(true);
	gl_drawinfo->drawlists[GLDL_MASKED].Sort();
	gl_drawinfo->drawlists[GLDL_MASKED].Draw(gl_texture? GLPASS_PLAIN : GLPASS_BASE_MASKED);
	gl_EnableBrightmap(false);
	gl_drawinfo->drawlists[GLDL_FOGMASKED].Sort();
	gl_drawinfo->drawlists[GLDL_FOGMASKED].Draw(gl_texture? GLPASS_PLAIN : GLPASS_BASE_MASKED);
	gl_drawinfo->drawlists[GLDL_LIGHTFOGMASKED].Sort();
	gl_drawinfo->drawlists[GLDL_LIGHTFOGMASKED].Draw(gl_texture? GLPASS_PLAIN : GLPASS_BASE_MASKED);

	// And now the multipass stuff

	// First pass: empty background with sector light only

	// Part 1: solid geometry. This is set up so that there are no transparent parts
	gl_EnableTexture(false);
	gl_drawinfo->drawlists[GLDL_LIGHT].Draw(GLPASS_BASE);
	gl_EnableTexture(true);

	// Part 2: masked geometry. This is set up so that only pixels with alpha>0.5 will show
	// This creates a blank surface that only fills the nontransparent parts of the texture
	gl_SetTextureMode(TM_MASK);
	gl_EnableBrightmap(true);
	gl_drawinfo->drawlists[GLDL_LIGHTBRIGHT].Draw(GLPASS_BASE_MASKED);
	gl_drawinfo->drawlists[GLDL_LIGHTMASKED].Draw(GLPASS_BASE_MASKED);
	gl_EnableBrightmap(false);
	gl_SetTextureMode(TM_MODULATE);


	// second pass: draw lights (on fogged surfaces they are added to the textures!)
	gl.DepthMask(false);
	if (gl_lights && gl_lightcount && !gl_fixedcolormap)
	{
		if (gl_SetupLightTexture())
		{
			gl.BlendFunc(GL_ONE, GL_ONE);
			gl.DepthFunc(GL_EQUAL);
			for(int i=GLDL_FIRSTLIGHT; i<=GLDL_LASTLIGHT; i++)
			{
				gl_drawinfo->drawlists[i].Draw(GLPASS_LIGHT);
			}
			gl.BlendEquation(GL_FUNC_ADD);
		}
		else gl_lights=false;
	}

	// third pass: modulated texture
	gl.Color3f(1.0f, 1.0f, 1.0f);
	gl.BlendFunc(GL_DST_COLOR, GL_ZERO);
	gl_EnableFog(false);
	gl.DepthFunc(GL_LEQUAL);
	if (gl_texture) 
	{
		gl.Disable(GL_ALPHA_TEST);
		gl_drawinfo->drawlists[GLDL_LIGHT].Sort();
		gl_drawinfo->drawlists[GLDL_LIGHT].Draw(GLPASS_TEXTURE);
		gl.Enable(GL_ALPHA_TEST);
		gl_drawinfo->drawlists[GLDL_LIGHTBRIGHT].Sort();
		gl_drawinfo->drawlists[GLDL_LIGHTBRIGHT].Draw(GLPASS_TEXTURE);
		gl_drawinfo->drawlists[GLDL_LIGHTMASKED].Sort();
		gl_drawinfo->drawlists[GLDL_LIGHTMASKED].Draw(GLPASS_TEXTURE);
	}

	// fourth pass: additive lights
	gl_EnableFog(true);
	if (gl_lights && gl_lightcount && !gl_fixedcolormap)
	{
		gl.BlendFunc(GL_ONE, GL_ONE);
		gl.DepthFunc(GL_EQUAL);
		if (gl_SetupLightTexture())
		{
			for(int i=0; i<GLDL_TRANSLUCENT; i++)
			{
				gl_drawinfo->drawlists[i].Draw(GLPASS_LIGHT_ADDITIVE);
			}
			gl.BlendEquation(GL_FUNC_ADD);
		}
		else gl_lights=false;
	}

	gl.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// Draw decals (not a real pass)
	gl.DepthFunc(GL_LEQUAL);
	gl.Enable(GL_POLYGON_OFFSET_FILL);
	gl.PolygonOffset(-1.0f, -128.0f);
	gl.DepthMask(false);

	for(int i=0; i<GLDL_TRANSLUCENT; i++)
	{
		gl_drawinfo->drawlists[i].Draw(GLPASS_DECALS);
	}

	gl_SetTextureMode(TM_MODULATE);

	gl.DepthMask(true);


	// Push bleeding floor/ceiling textures back a little in the z-buffer
	// so they don't interfere with overlapping mid textures.
	gl.PolygonOffset(1.0f, 128.0f);

	// flood all the gaps with the back sector's flat texture
	// This will always be drawn like GLDL_PLAIN or GLDL_FOG, depending on the fog settings
	gl_drawinfo->DrawUnhandledMissingTextures();

	gl.PolygonOffset(0.0f, 0.0f);
	gl.Disable(GL_POLYGON_OFFSET_FILL);

	RenderAll.Unclock();
}

//-----------------------------------------------------------------------------
//
// RenderTranslucent
//
// Draws the current draw lists for the non GLSL renderer
//
//-----------------------------------------------------------------------------

static void RenderTranslucent()
{
	RenderAll.Clock();

	gl.DepthMask(false);
	gl_SetCamera(TO_GL(viewx), TO_GL(viewy), TO_GL(viewz));

	// final pass: translucent stuff
	gl.AlphaFunc(GL_GEQUAL,0.5f);
	gl.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	gl_EnableBrightmap(true);
	gl_drawinfo->drawlists[GLDL_TRANSLUCENTBORDER].Draw(GLPASS_TRANSLUCENT);
	gl_drawinfo->drawlists[GLDL_TRANSLUCENT].DrawSorted();
	gl_EnableBrightmap(false);

	gl.DepthMask(true);

	gl.AlphaFunc(GL_GEQUAL,0.5f);
	RenderAll.Unclock();
}


//-----------------------------------------------------------------------------
//
// gl_drawscene - this function renders the scene from the current
// viewpoint, including mirrors and skyboxes and other portals
// It is assumed that the GLPortal::EndFrame returns with the 
// stencil, z-buffer and the projection matrix intact!
//
//-----------------------------------------------------------------------------

void gl_DrawScene()
{
	static int recursion=0;

	ProcessScene();

	RenderScene(recursion);

	// Handle all portals after rendering the opaque objects but before
	// doing all translucent stuff
	recursion++;
	GLPortal::EndFrame();
	recursion--;

	RenderTranslucent();
}


//==========================================================================
//
// Draws a blend over the entire view
//
// This mostly duplicates the code in shared_sbar.cpp
// When I was writing this the original was called too late so that I
// couldn't get the blend in time. However, since then I made some changes
// here that would get lost if I switched back so I won't do it.
//
//==========================================================================
// [BC] Blah. Not a great place to include this.
EXTERN_CVAR (Float,  blood_fade_scalar)
static void gl_DrawBlend(sector_t * viewsector)
{
	float blend[4]={0,0,0,0};
	int cnt;
	PalEntry blendv=0;
	float extra_red;
	float extra_green;
	float extra_blue;
	player_t * player=players[consoleplayer].camera->player;

	// [RH] Amount of red flash for up to 114 damage points. Calculated by hand
	//		using a logarithmic scale and my trusty HP48G.
	static const byte DamageToAlpha[114] =
	{
		  0,   8,  16,  23,  30,  36,  42,  47,  53,  58,  62,  67,  71,  75,  79,
		 83,  87,  90,  94,  97, 100, 103, 107, 109, 112, 115, 118, 120, 123, 125,
		128, 130, 133, 135, 137, 139, 141, 143, 145, 147, 149, 151, 153, 155, 157,
		159, 160, 162, 164, 165, 167, 169, 170, 172, 173, 175, 176, 178, 179, 181,
		182, 183, 185, 186, 187, 189, 190, 191, 192, 194, 195, 196, 197, 198, 200,
		201, 202, 203, 204, 205, 206, 207, 209, 210, 211, 212, 213, 214, 215, 216,
		217, 218, 219, 220, 221, 221, 222, 223, 224, 225, 226, 227, 228, 229, 229,
		230, 231, 232, 233, 234, 235, 235, 236, 237
	};

	// don't draw sector based blends when an invulnerability colormap is active
	if (!gl_fixedcolormap)
	{
		if (!viewsector->e->XFloor.ffloors.Size())
		{
			if (viewsector->heightsec && !(viewsector->MoreFlags&SECF_IGNOREHEIGHTSEC))
			{
				switch(in_area)
				{
				default:
				case area_normal: blendv=viewsector->heightsec->midmap; break;
				case area_above: blendv=viewsector->heightsec->topmap; break;
				case area_below: blendv=viewsector->heightsec->bottommap; break;
				}
			}
		}
		else
		{
			TArray<lightlist_t> & lightlist = viewsector->e->XFloor.lightlist;

			for(unsigned int i=0;i<lightlist.Size();i++)
			{
				fixed_t lightbottom;
				if (i<lightlist.Size()-1) 
					lightbottom=lightlist[i+1].plane.ZatPoint(viewx,viewy);
				else 
					lightbottom=viewsector->floorplane.ZatPoint(viewx,viewy);

				if (lightbottom<viewz && (!lightlist[i].caster || !(lightlist[i].caster->flags&FF_FADEWALLS)))
				{
					// 3d floor 'fog' is rendered as a blending value
					blendv=(lightlist[i].extra_colormap)->Fade;
					// If this is the same as the sector's it doesn't apply!
					if (blendv == viewsector->ColorMap->Fade) blendv=0;
					// a little hack to make this work for Legacy maps.
					if (blendv.a==0 && blendv!=0) blendv.a=128;
					break;
				}
			}
		}
	}

	if (/*gl_blendcolormaps &&*/ blendv.a==0)
	{
		blendv = R_BlendForColormap(blendv);
		if (blendv.a==255)
		{
			// The calculated average is too dark so brighten it according to the palettes's overall brightness
			int maxcol = MAX<int>(MAX<int>(palette_brightness, blendv.r), MAX<int>(blendv.g, blendv.b));
			blendv.r = blendv.r * 255 / maxcol;
			blendv.g = blendv.g * 255 / maxcol;
			blendv.b = blendv.b * 255 / maxcol;
		}
	}

	if (blendv.a==255)
	{

		extra_red = blendv.r / 255.0f;
		extra_green = blendv.g / 255.0f;
		extra_blue = blendv.b / 255.0f;

		// If this is a multiplicative blend do it separately and add the additive ones on top of it!
		blendv=0;

		// black multiplicative blends are ignored
		if (extra_red || extra_green || extra_blue)
		{
			gl.Disable(GL_ALPHA_TEST);
			gl_EnableTexture(false);
			gl.BlendFunc(GL_DST_COLOR,GL_ZERO);
			gl.Color4f(extra_red, extra_green, extra_blue, 1.0f);
			gl_DisableShader();
			gl.Begin(GL_TRIANGLE_STRIP);
			gl.Vertex2f( 0.0f, 0.0f);
			gl.Vertex2f( 0.0f, (float)SCREENHEIGHT);
			gl.Vertex2f( (float)SCREENWIDTH, 0.0f);
			gl.Vertex2f( (float)SCREENWIDTH, (float)SCREENHEIGHT);
			gl.End();
		}
	}
	else if (blendv.a)
	{
		gl_AddBlend (blendv.r / 255.f, blendv.g / 255.f, blendv.b / 255.f, blendv.a/255.0f,blend);
	}

	if (player)
	{
		AInventory * in;

		for(in=player->mo->Inventory;in;in=in->Inventory)
		{
			PalEntry color = in->GetBlend ();
			if (color.a != 0)
			{
				gl_AddBlend (color.r/255.f, color.g/255.f, color.b/255.f, color.a/255.f, blend);
			}
		}
		if (player->bonuscount)
		{
			cnt = player->bonuscount << 3;
			gl_AddBlend (0.8431f, 0.7333f, 0.2706f, cnt > 128 ? 0.5f : cnt / 256.f, blend);
		}
		
		// FIXME!
		cnt = DamageToAlpha[MIN (113, player->damagecount)];
		
		// [BC] Allow users to tone down the intensity of the blood on the screen.
		cnt = (int)( cnt * blood_fade_scalar );

		if (cnt)
		{
			if (cnt > 175) cnt = 175; // too strong and it gets too opaque
			
			gl_AddBlend (player->mo->RedDamageFade / 255, 
				player->mo->GreenDamageFade / 255, 
				player->mo->BlueDamageFade / 255, cnt / 255.f, blend);
		}
		
		if (player->poisoncount)
		{
			cnt = MIN (player->poisoncount, 64);
			gl_AddBlend (0.04f, 0.2571f, 0.f, cnt/93.2571428571f, blend);
		}
		else if (player->hazardcount)
		{
			cnt= MIN(player->hazardcount/8, 64);
			gl_AddBlend (0.04f, 0.2571f, 0.f, cnt/93.2571428571f, blend);
		}
		if (player->mo->flags&MF_ICECORPSE)
		{
			gl_AddBlend (0.25f, 0.25f, 0.853f, 0.4f, blend);
		}

		// translucency may not go below 50%!
		if (blend[3]>0.5f) blend[3]=0.5f;
	}
	
	if (players[consoleplayer].camera != NULL)
	{
		// except for fadeto effects
		player_t *player = (players[consoleplayer].camera->player != NULL) ? players[consoleplayer].camera->player : &players[consoleplayer];
		gl_AddBlend (player->BlendR, player->BlendG, player->BlendB, player->BlendA, blend);
	}

	if (blend[3]>0.0f)
	{
		gl.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		gl.Disable(GL_ALPHA_TEST);
		gl_EnableTexture(false);
		gl.Color4fv(blend);
		gl_DisableShader();
		gl.Begin(GL_TRIANGLE_STRIP);
		gl.Vertex2f( 0.0f, 0.0f);
		gl.Vertex2f( 0.0f, (float)SCREENHEIGHT);
		gl.Vertex2f( (float)SCREENWIDTH, 0.0f);
		gl.Vertex2f( (float)SCREENWIDTH, (float)SCREENHEIGHT);
		gl.End();
	}
}


//-----------------------------------------------------------------------------
//
// gl_SetupView
// Draws player sprites and color blend
//
//-----------------------------------------------------------------------------
void gl_EndDrawScene(sector_t * viewsector)
{
	extern void gl_DrawPlayerSprites (sector_t *, bool);
	extern void gl_DrawTargeterSprites();

	// [BB] HUD models need to be rendered here. Make sure that
	// gl_DrawPlayerSprites is only called once. Either to draw
	// HUD models or to draw the weapon sprites.
	const bool renderHUDModel = gl_IsHUDModelForPlayerAvailable( players[consoleplayer].camera->player );
	if ( renderHUDModel )
	{
		// [BB] The HUD model should be drawn over everything else already drawn.
		gl.Clear(GL_DEPTH_BUFFER_BIT);
		gl_DrawPlayerSprites (viewsector, true);
	}

	gl.DisableClientState(GL_TEXTURE_COORD_ARRAY);
	gl.DisableClientState(GL_VERTEX_ARRAY);

	gl.Disable(GL_STENCIL_TEST);
	gl.Disable(GL_POLYGON_SMOOTH);

	gl_EnableFog(false);
	screen->Begin2D(false);

	gl_ResetViewport();
	// [BB] Only draw the sprites if we didn't render a HUD model before.
	if ( renderHUDModel == false )
	{
		gl_DrawPlayerSprites (viewsector, false);
	}
	gl_DrawTargeterSprites();
	gl_DrawBlend(viewsector);

	// Restore standard rendering state
	gl.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	gl.Color3f(1.0f,1.0f,1.0f);
	gl_EnableTexture(true);
	gl.Enable(GL_ALPHA_TEST);
	gl.Disable(GL_SCISSOR_TEST);
}


//-----------------------------------------------------------------------------
//
// R_RenderView - renders one view - either the screen or a camera texture
//
//-----------------------------------------------------------------------------
static GLDrawInfo GlobalDrawInfo;

sector_t * gl_RenderView (AActor * camera, GL_IRECT * bounds, float fov, float ratio, float fovratio, bool mainview)
{       
	TThinkerIterator<ADynamicLight> it(STAT_DLIGHT);

	// Check if there's some lights. If not some code can be skipped.
	gl_lightcount = (it.Next()!=NULL);

	sector_t * retval;
	R_SetupFrame (camera);
	gl_SetViewArea();
	pitch = clamp<float>((float)((double)(int)(viewpitch))/ANGLE_1, -90, 90);

	// Scroll the sky
	gl_sky1pos = (float)fmod(gl_frameMS * level.skyspeed1, 1024.f) * 90.f/256.f;
	gl_sky2pos = (float)fmod(gl_frameMS * level.skyspeed2, 1024.f) * 90.f/256.f;

	retval = viewsector;


	// [BB] consoleplayer should be able to toggle the chase cam.
	if (camera->player && /*camera->player-players==consoleplayer &&*/
		((/*camera->player->*/players[consoleplayer].cheats & CF_CHASECAM) || (r_deathcamera && camera->health <= 0)) && camera==camera->player->mo)
	{
		viewactor=NULL;
	}
	else
	{
		viewactor=camera;
	}

	gl_StartDrawScene(bounds, fov, ratio, fovratio);	// switch to perspective mode and set up clipper

	GLDrawInfo::StartDrawInfo(&GlobalDrawInfo);
	gl_SetupView(viewx, viewy, viewz, viewangle, false, false);

	clipper.Clear();
	angle_t a1 = gl_FrustumAngle();
	clipper.SafeAddClipRange(viewangle+a1, viewangle-a1);

	gl_DrawScene();
	GLDrawInfo::EndDrawInfo();

	interpolator.RestoreInterpolations ();

	// [BC] Be need to clear the clipper once again to prevent a memory leak. It's not a
	// great solution, but it'll do for now.
	clipper.Clear( );

	return retval;
}


//-----------------------------------------------------------------------------
//
// R_RenderTextureView - renders camera textures
//
//-----------------------------------------------------------------------------
void gl_RenderTextureView(FCanvasTexture *Texture, AActor * Viewpoint, int FOV)
{
	GL_IRECT bounds;
	FGLTexture * gltex = FGLTexture::ValidateTexture(Texture);

	int width = gltex->TextureWidth(FGLTexture::GLUSE_TEXTURE);
	int height = gltex->TextureHeight(FGLTexture::GLUSE_TEXTURE);

	gl_fixedcolormap=CM_DEFAULT;
	bounds.left=bounds.top=0;
	bounds.width=GLTexture::GetTexDimension(gltex->GetWidth(FGLTexture::GLUSE_TEXTURE));
	bounds.height=GLTexture::GetTexDimension(gltex->GetHeight(FGLTexture::GLUSE_TEXTURE));

	gl.Flush();
	gl_RenderView(Viewpoint, &bounds, FOV, (float)width/height, (float)width/height, false);
	gl.Flush();
	gltex->Bind(CM_DEFAULT, 0, 0);
	gl.CopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, bounds.width, bounds.height);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GLTexture::TexFilter[gl_texture_filter].magfilter);
}


//-----------------------------------------------------------------------------
//
// gl_SetFixedColormap
//
//-----------------------------------------------------------------------------

void OpenGLFrameBuffer::SetFixedColormap (player_t *player)
{
	gl_fixedcolormap=CM_DEFAULT;

	// check for special colormaps
	player_t * cplayer = player->camera->player;
	if (cplayer) 
	{
		if (cplayer->extralight<0)
		{
			gl_fixedcolormap=CM_INVERT;
			extralight=0;
		}
		else if (cplayer->fixedcolormap==INVERSECOLORMAP) gl_fixedcolormap=CM_INVERT;
		else if (cplayer->fixedcolormap==GOLDCOLORMAP) gl_fixedcolormap=CM_GOLDMAP;
		else if (cplayer->fixedcolormap==REDCOLORMAP) gl_fixedcolormap=CM_REDMAP;
		else if (cplayer->fixedcolormap==GREENCOLORMAP) gl_fixedcolormap=CM_GREENMAP;
		else if (cplayer->fixedcolormap!=0 && cplayer->fixedcolormap<NUMCOLORMAPS) 
		{
			for(AInventory * in = cplayer->mo->Inventory; in; in = in->Inventory)
			{
				PalEntry color = in->GetBlend ();

				// Need special handling for light amplifiers 
				if (in->IsA(RUNTIME_CLASS(APowerTorch)))
				{
					gl_fixedcolormap = cplayer->fixedcolormap + CM_TORCH;
				}
				else if (in->IsA(RUNTIME_CLASS(APowerLightAmp)))
				{
					gl_fixedcolormap = CM_LITE;
				}
			}
		}
	}
}

//===========================================================================
//
// Render the view to a savegame picture
//
//===========================================================================

void OpenGLFrameBuffer::WriteSavePic (player_t *player, FILE *file, int width, int height)
{
	GL_IRECT bounds;

	bounds.left=0;
	bounds.top=0;
	bounds.width=width;
	bounds.height=height;
	gl.Flush();
	SetFixedColormap(player);
	sector_t *viewsector = gl_RenderView(players[consoleplayer].camera, &bounds, 
								FieldOfView * 360.0f / FINEANGLES, 1.6f, 1.6f, true);
	gl.Disable(GL_STENCIL_TEST);
	screen->Begin2D(false);
	gl_DrawBlend(viewsector);
	gl.Flush();

	byte * scr = (byte *)M_Malloc(width * height * 3);
	gl.ReadPixels(0,0,width, height,GL_RGB,GL_UNSIGNED_BYTE,scr);
	M_CreatePNG (file, scr + ((height-1) * width * 3), NULL, SS_RGB, width, height, -width*3);
	M_Free(scr);

	// [BC] In GZDoom, this is called every frame, regardless of whether or not
	// the view is active. In Skulltag, we don't so we have to call this here
	// to reset everything, such as the viewport, after rendering our view to
	// a canvas.
	gl_EndDrawScene( viewsector );
}

//-----------------------------------------------------------------------------
//
// R_RenderPlayerView - the main rendering function
//
//-----------------------------------------------------------------------------

#ifdef _WIN32 // [BB] Detect some kinds of glBegin hooking.
extern char myGlBeginCharArray[4];
int crashoutTic = 0;
#endif

void OpenGLFrameBuffer::RenderView (player_t* player)
{       
#ifdef _WIN32 // [BB] Detect some kinds of glBegin hooking.
	// [BB] Continuously make this check, otherwise a hack could bypass the check by activating
	// and deactivating itself at the right time interval.
	{
		if ( strncmp(reinterpret_cast<char *>(gl.Begin), myGlBeginCharArray, 4) )
		{
			I_FatalError ( "OpenGL malfunction encountered.\n" );
		}
		else
		{
			// [BB] Most GL wallhacks deactivate GL_DEPTH_TEST by manipulating glBegin.
			// Here we try check if this is done.
			GLboolean oldValue;
			glGetBooleanv ( GL_DEPTH_TEST, &oldValue );
			gl.Enable ( GL_DEPTH_TEST );
			gl.Begin( GL_TRIANGLE_STRIP );
			gl.End();
			GLboolean valueTrue;
			glGetBooleanv ( GL_DEPTH_TEST, &valueTrue );

			// [BB] Also check if glGetBooleanv simply always returns true.
			gl.Disable ( GL_DEPTH_TEST );
			GLboolean valueFalse;
			glGetBooleanv ( GL_DEPTH_TEST, &valueFalse );

			if ( ( valueTrue == false ) || ( valueFalse == true ) )
			{
				I_FatalError ( "OpenGL malfunction encountered.\n" );
			}

			if ( oldValue )
				gl.Enable ( GL_DEPTH_TEST );
			else
				gl.Disable ( GL_DEPTH_TEST );
		}
	}
#endif

	if (player->camera != LastCamera)
	{
		// If the camera changed don't interpolate
		// Otherwise there will be some not so nice effects.
		R_ResetViewInterpolation();
		LastCamera=player->camera;
	}

	//Printf("Starting scene\n");
	All.Reset();
	All.Clock();
	PortalAll.Reset();
	RenderAll.Reset();
	ProcessAll.Reset();
	flatvertices=flatprimitives=vertexcount=0;

	// Get this before everything else
	if (cl_capfps || r_NoInterpolate) r_TicFrac = FRACUNIT;
	else r_TicFrac = I_GetTimeFrac (&r_FrameTime);
	gl_frameMS = I_MSTime();

	R_FindParticleSubsectors ();

	// prepare all camera textures that have been used in the last frame
	FCanvasTextureInfo::UpdateAll();

	SetFixedColormap (player);

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

	sector_t * viewsector = gl_RenderView(player->camera, NULL, FieldOfView * 360.0f / FINEANGLES, ratio, fovratio, true);
	gl_EndDrawScene(viewsector);

	All.Unclock();
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

ADD_STAT(lightstats)
{
	FString out;
	out.Format("DLight - Walls: %d processed, %d rendered - Flats: %d processed, %d rendered\n", 
		iter_dlight, draw_dlight, iter_dlightf, draw_dlightf );
	return out;
}
