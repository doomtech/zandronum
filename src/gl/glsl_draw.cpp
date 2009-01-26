
#include "gl/gl_include.h"
#include "p_local.h"
#include "p_lnspec.h"
#include "a_sharedglobal.h"
#include "r_blend.h"
#include "gl/gl_struct.h"
#include "gl/gl_renderstruct.h"
#include "gl/gl_portal.h"
#include "gl/gl_lights.h"
#include "gl/gl_glow.h"
#include "gl/gl_data.h"
#include "gl/gl_texture.h"
#include "gl/gl_basic.h"
#include "gl/gl_functions.h"
#include "gl/gl_shader.h"
#include "gl/glsl_state.h"

EXTERN_CVAR(Bool, gl_seamless)
EXTERN_CVAR(Bool, gl_usecolorblending)
EXTERN_CVAR(Bool, gl_sprite_blend)
EXTERN_CVAR(Bool, gl_nocoloredspritelighting)
EXTERN_CVAR(Int, gl_spriteclip)
EXTERN_CVAR(Int, gl_particles_style)
EXTERN_CVAR(Int, gl_billboard_mode)


//==========================================================================
//
// General purpose wall rendering function
// with the exception of walls lit by glowing flats 
// everything goes through here
//
// Tests have shown that precalculating this data
// doesn't give any noticable performance improvements
//
//==========================================================================

void GLWall::RenderWallGLSL()
{
	texcoord tcs[4];

	tcs[0]=lolft;
	tcs[1]=uplft;
	tcs[2]=uprgt;
	tcs[3]=lorgt;

	if (type==RENDERWALL_M2SFOG) glsl->EnableFogBoundary(true);

	glsl->Apply();
	gl.Begin(GL_TRIANGLE_FAN);

	// lower left corner
	gl.TexCoord2f(tcs[0].u,tcs[0].v);
	gl.Vertex3f(glseg.x1,zbottom[0],glseg.y1);

	if (gl_seamless && glseg.fracleft==0) gl_SplitLeftEdge(this, tcs);

	// upper left corner
	gl.TexCoord2f(tcs[1].u,tcs[1].v);
	gl.Vertex3f(glseg.x1,ztop[0],glseg.y1);

	// upper right corner
	gl.TexCoord2f(tcs[2].u,tcs[2].v);
	gl.Vertex3f(glseg.x2,ztop[1],glseg.y2);

	if (gl_seamless && glseg.fracright==1) gl_SplitRightEdge(this, tcs);

	// lower right corner
	gl.TexCoord2f(tcs[3].u,tcs[3].v); 
	gl.Vertex3f(glseg.x2,zbottom[1],glseg.y2);

	gl.End();

	vertexcount+=4;
	if (type==RENDERWALL_M2SFOG) glsl->EnableFogBoundary(false);
}


//==========================================================================
//
// 
//
//==========================================================================

void GLWall::RenderTranslucentWallGLSL()
{
	bool transparent = gltexture? gltexture->GetTransparent() : false;
	
	// currently the only modes possible are solid, additive or translucent
	// and until that changes I won't fix this code for the new blending modes!
	bool additive = RenderStyle == STYLE_Add;

	if (!transparent) glsl->SetAlphaThreshold(0.5f*fabs(alpha));
	else glsl->SetAlphaThreshold(0);

	if (additive) glsl->SetBlend(GL_SRC_ALPHA,GL_ONE);


	if (gltexture) 
	{
		gltexture->Bind(Colormap.LightColor.a, flags);
		// prevent some ugly artifacts at the borders of fences etc.
		glsl->SetLight(lightlevel, (extralight * gl_weaponlight), &Colormap, fabsf(alpha), additive);
	}
	else 
	{
		glsl->EnableTexture(false);
		glsl->SetLight(lightlevel, 0, &Colormap, fabsf(alpha), additive);
	}

	RenderWallGLSL();

	// restore default settings
	if (additive) glsl->SetBlend(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glsl->EnableTexture(true);
}

//==========================================================================
//
// 
//
//==========================================================================

void GLWall::RenderFogBoundaryGLSL()
{
	glsl->SetAlphaThreshold(0);
	glsl->EnableFogBoundary(true);
	glsl->EnableTexture(false);
	glsl->SetLight(lightlevel, 0, &Colormap, 0, false);

	RenderWallGLSL();

	glsl->EnableTexture(true);
	glsl->EnableFogBoundary(false);
	glsl->EnableTexture(true);
}

//==========================================================================
//
// 
//
//==========================================================================
void GLWall::DrawGLSL(int pass)
{
#ifdef _MSC_VER
#ifdef _DEBUG
	if (seg->linedef-lines==879)
		__asm nop
#endif
#endif


	// This allows mid textures to be drawn on lines that might overlap a sky wall
	if ((flags&GLWF_SKYHACK && type==RENDERWALL_M2S) || type == RENDERWALL_COLORLAYER)
	{
		if (pass != GLPASS_DECALS)
		{
			gl.Enable(GL_POLYGON_OFFSET_FILL);
			gl.PolygonOffset(-1.0f, -128.0f);
		}
	}

	switch (pass)
	{
	case GLPASS_PLAIN:			// Single-pass rendering
		glsl->SetLight(lightlevel, rellight + (extralight * gl_weaponlight), &Colormap, 1.0f, false);
		gltexture->Bind(Colormap.LightColor.a, flags);
		RenderWallGLSL();
		break;

	case GLPASS_DECALS:
		if (seg->sidedef && seg->sidedef->AttachedDecals)
		{
			DoDrawDecals(seg->sidedef->AttachedDecals, seg);
		}
		break;

	case GLPASS_TRANSLUCENT:
		switch (type)
		{
		case RENDERWALL_MIRRORSURFACE:
			//RenderMirrorSurface();
			break;

		case RENDERWALL_FOGBOUNDARY:
			RenderFogBoundaryGLSL();
			break;

		default:
			RenderTranslucentWallGLSL();
			break;
		}
	}

	if ((flags&GLWF_SKYHACK && type==RENDERWALL_M2S) || type == RENDERWALL_COLORLAYER)
	{
		if (pass!=GLPASS_DECALS)
		{
			gl.Disable(GL_POLYGON_OFFSET_FILL);
			gl.PolygonOffset(0, 0);
		}
	}
}



//==========================================================================
//
// Flats 
//
//==========================================================================
/*
void GLFlat::DrawSubsectorLights(subsector_t * sub, int pass)
{
	Plane p;
	Vector nearPt, up, right;
	float scale;
	int k;

#ifdef DEBUG
	if (sub-subsectors==314)
	{
		__asm nop
	}
#endif

	FLightNode * node = sub->lighthead[pass==GLPASS_LIGHT_ADDITIVE];
	while (node)
	{
		ADynamicLight * light = node->lightsource;
		
		if (light->flags2&MF2_DORMANT)
		{
			node=node->nextLight;
			continue;
		}

		// we must do the side check here because gl_SetupLight needs the correct plane orientation
		// which we don't have for Legacy-style 3D-floors
		fixed_t planeh = plane.plane.ZatPoint(light->x, light->y);
		if (gl_lights_checkside && ((planeh<light->z && ceiling) || (planeh>light->z && !ceiling)))
		{
			node=node->nextLight;
			continue;
		}

		p.Set(plane.plane);
		if (!gl_SetupLight(p, light, nearPt, up, right, scale, Colormap.LightColor.a, false, foggy)) 
		{
			node=node->nextLight;
			continue;
		}
		draw_dlightf++;

		// Set the plane
		if (plane.plane.a || plane.plane.b) for(k = 0; k < sub->numvertices; k++)
		{
			// Unfortunately the rendering inaccuracies prohibit any kind of plane translation
			// This must be done on a per-vertex basis.
			gl_vertices[sub->firstvertex + k].z =
				TO_MAP(plane.plane.ZatPoint(gl_vertices[sub->firstvertex + k].vt));
		}
		else for(k = 0; k < sub->numvertices; k++)
		{
			gl_vertices[sub->firstvertex + k].z = z;
		}

		// Render the light
		gl.Begin(GL_TRIANGLE_FAN);
		for(k = 0; k < sub->numvertices; k++)
		{
			Vector t1;
			GLVertex * vt = &gl_vertices[sub->firstvertex + k];

			t1.Set(vt->x, vt->z, vt->y);
			Vector nearToVert = t1 - nearPt;
			gl.TexCoord2f( (nearToVert.Dot(right) * scale) + 0.5f, (nearToVert.Dot(up) * scale) + 0.5f);
			gl.Vertex3f(vt->x, vt->z, vt->y);
		}
		gl.End();
		node = node->nextLight;
	}
}
*/


//==========================================================================
//
//
//
//==========================================================================
void GLFlat::DrawGLSL(int pass)
{
	switch (pass)
	{
	case GLPASS_PLAIN:			// Single-pass rendering
		glsl->SetLight(lightlevel, extralight*gl_weaponlight, &Colormap,1.0f, false);
		gltexture->Bind(Colormap.LightColor.a);
		gl_SetPlaneTextureRotation(&plane, gltexture);
		DrawSubsectors(false);
		gl.PopMatrix();
		break;

	case GLPASS_TRANSLUCENT:
		if (renderstyle==STYLE_Add) glsl->SetBlend(GL_SRC_ALPHA, GL_ONE);
		glsl->SetLight(lightlevel, extralight*gl_weaponlight, &Colormap, alpha, renderstyle==STYLE_Add);
		glsl->SetAlphaThreshold(0.5f*(alpha));
		if (!gltexture)	glsl->EnableTexture(false);
		else 
		{
			gltexture->Bind(Colormap.LightColor.a);
			gl_SetPlaneTextureRotation(&plane, gltexture);
		}

		DrawSubsectors(true);

		if (!gltexture)	glsl->EnableTexture(true);
		else gl.PopMatrix();
		if (renderstyle==STYLE_Add) glsl->SetBlend(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		break;
	}
}

//==========================================================================
//
// 
//
//==========================================================================
void GLSprite::DrawGLSL(int pass)
{
	bool additivefog = false;

	if (pass==GLPASS_TRANSLUCENT)
	{
		// The translucent pass requires special setup for the various modes.

		if (!gl_sprite_blend && hw_styleflags != STYLEHW_Solid && actor && !(actor->momx|actor->momy))
		{
			// Draw translucent non-moving sprites with a slightly altered z-offset to avoid z-fighting 
			// when in the same position as a regular sprite.
			// (mostly added for KDiZD's lens flares.)
			
			gl.Enable(GL_POLYGON_OFFSET_FILL);
			gl.PolygonOffset(-1.0f, -64.0f);
		}

		gl_SetRenderStyle(RenderStyle, false, 
			// The rest of the needed checks are done inside gl_SetRenderStyle
			trans > 1.f - FLT_EPSILON && gl_usecolorblending && actor && (actor->renderflags & RF_FULLBRIGHT) &&
			gltexture && !gltexture->GetTransparent());

		glsl->SetAlphaThreshold(hw_styleflags == STYLEHW_NoAlphaTest? 0 : trans/2.f);

		if (RenderStyle.BlendOp == STYLEOP_Fuzz)
		{
			float fuzzalpha=0.44f;
			float minalpha=0.1f;

			// fog + fuzz don't work well without some fiddling with the alpha value!
			if (!gl_isBlack(Colormap.FadeColor))
			{
				float xcamera=TO_MAP(viewx);
				float ycamera=TO_MAP(viewy);

				float dist=Dist2(xcamera,ycamera, x,y);

				if (!Colormap.FadeColor.a) Colormap.FadeColor.a=clamp<int>(255-lightlevel,60,255);

				// this value was determined by trial and error and is scale dependent!
				float factor=0.05f+exp(-Colormap.FadeColor.a*dist/62500.f);
				fuzzalpha*=factor;
				minalpha*=factor;
			}
			glsl->SetAlphaThreshold(minalpha);

			gl.AlphaFunc(GL_GEQUAL,minalpha);
			gl.Color4f(0.2f,0.2f,0.2f,fuzzalpha);
			additivefog = true;
		}
		else if (RenderStyle.BlendOp == STYLEOP_Add && RenderStyle.DestAlpha == STYLEALPHA_One)
		{
			additivefog = true;
		}
	}
	if (RenderStyle.BlendOp!=STYLEOP_Fuzz)
	{
		float result[3]={0,0,0};
		if (gl_lights)
		{
			if (actor)
			{
				gl_GetSpriteLight(actor, actor->x, actor->y, actor->z+(actor->height>>1), 
					actor->subsector, 0, result);
			}
			else if (particle && gl_light_particles)
			{
				gl_GetSpriteLight(NULL, particle->x, particle->y, particle->z, 
					particle->subsector, 0, result);
			}
			else return;
		}
		glsl->SetAddLight(result);
	}

	if (RenderStyle.Flags & STYLEF_FadeToBlack) 
	{
		Colormap.FadeColor=0;
		additivefog = true;
	}

	if (RenderStyle.Flags & STYLEF_InvertOverlay) 
	{
		Colormap.FadeColor = Colormap.FadeColor.InverseColor();
		additivefog=false;
	}

	glsl->SetLight(lightlevel, extralight*gl_weaponlight, &Colormap, trans, additivefog, ThingColor);

	if (gltexture) gltexture->BindPatch(Colormap.LightColor.a,translation);
	else if (!modelframe) gl_EnableTexture(false);

	if (!modelframe)
	{
		// [BB] Billboard stuff
		const bool drawWithXYBillboard = ( !(actor && actor->renderflags & RF_FORCEYBILLBOARD)
		                                   && players[consoleplayer].camera
		                                   && (gl_billboard_mode == 1 || (actor && actor->renderflags & RF_FORCEXYBILLBOARD )) );
		if ( drawWithXYBillboard )
		{
			// Save the current view matrix.
			gl.MatrixMode(GL_MODELVIEW);
			gl.PushMatrix();
			// Rotate the sprite about the vector starting at the center of the sprite
			// triangle strip and with direction orthogonal to where the player is looking
			// in the x/y plane.
			float xcenter = (x1+x2)*0.5;
			float ycenter = (y1+y2)*0.5;
			float zcenter = (z1+z2)*0.5;
			float angleRad = ANGLE_TO_FLOAT(players[consoleplayer].camera->angle)/180.*M_PI;
			gl.Translatef( xcenter, zcenter, ycenter);
			gl.Rotatef(-ANGLE_TO_FLOAT(players[consoleplayer].camera->pitch), -sin(angleRad), 0, cos(angleRad));
			gl.Translatef( -xcenter, -zcenter, -ycenter);
		}
		glsl->Apply();
		gl.Begin(GL_TRIANGLE_STRIP);
		if (gltexture)
		{
			gl.TexCoord2f(ul, vt); gl.Vertex3f(x1, z1, y1);
			gl.TexCoord2f(ur, vt); gl.Vertex3f(x2, z1, y2);
			gl.TexCoord2f(ul, vb); gl.Vertex3f(x1, z2, y1);
			gl.TexCoord2f(ur, vb); gl.Vertex3f(x2, z2, y2);
		}
		else	// Particle
		{
			gl.Vertex3f(x1, z1, y1);
			gl.Vertex3f(x2, z1, y2);
			gl.Vertex3f(x1, z2, y1);
			gl.Vertex3f(x2, z2, y2);
		}
		gl.End();
		// [BB] Billboard stuff
		if ( drawWithXYBillboard )
		{
			// Restore the view matrix.
			gl.MatrixMode(GL_MODELVIEW);
			gl.PopMatrix();
		}
	}
	else
	{
		//gl_RenderModel(this, Colormap.LightColor.a);
	}

	if (pass==GLPASS_TRANSLUCENT)
	{
		glsl->SetBlend(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		gl.BlendEquation(GL_FUNC_ADD);
		glsl->SetTextureMode(TM_MODULATE);
		glsl->SetAlphaThreshold(0.5f);

		if (!gl_sprite_blend && hw_styleflags != STYLEHW_Solid && actor && !(actor->momx|actor->momy))
		{
			gl.Disable(GL_POLYGON_OFFSET_FILL);
			gl.PolygonOffset(0, 0);
		}
	}

	if (!gltexture) glsl->EnableTexture(true);
}


