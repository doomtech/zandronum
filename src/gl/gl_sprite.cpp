/*
** gl_sprite.cpp
** Sprite/Particle rendering
**
**---------------------------------------------------------------------------
** Copyright 2002-2005 Christoph Oelckers
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
#include "r_translate.h"
#include "g_level.h"
#include "doomstat.h"
#include "gl/gl_struct.h"
#include "gl/gl_renderstruct.h"
#include "gl/gl_lights.h"
#include "gl/gl_glow.h"
#include "gl/gl_texture.h"
#include "gl/gl_functions.h"
#include "gl/gl_portal.h"
#include "gl/gl_models.h"
#include "gl/gl_shader.h"
#include "r_sky.h"
// [BB] New #includes.
#include "gamemode.h"

CVAR(Bool, gl_usecolorblending, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR(Bool, gl_sprite_blend, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG);
CVAR(Int, gl_spriteclip, 1, CVAR_ARCHIVE)
CVAR(Int, gl_particles_style, 2, CVAR_ARCHIVE | CVAR_GLOBALCONFIG) // 0 = square, 1 = round, 2 = smooth
CVAR(Int, gl_billboard_mode, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

extern bool r_showviewer;
EXTERN_CVAR (Float, transsouls)

const BYTE SF_FRAMEMASK  = 0x1f;

//==========================================================================
//
// 
//
//==========================================================================
void GLSprite::Draw(int pass)
{
	bool additivefog = false;
	int rel = extralight*gl_weaponlight;

	if (pass!=GLPASS_PLAIN && pass!=GLPASS_TRANSLUCENT) return;

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

		// Brightmaps will only be used when doing regular drawing ops and having no fog
		if (!gl_isBlack(Colormap.FadeColor) || level.flags&LEVEL_HASFADETABLE || 
			RenderStyle.BlendOp != STYLEOP_Add)
		{
			gl_EnableBrightmap(false);
		}

		gl_SetRenderStyle(RenderStyle, false, 
			// The rest of the needed checks are done inside gl_SetRenderStyle
			trans > 1.f - FLT_EPSILON && gl_usecolorblending && actor && fullbright &&
			gltexture && !gltexture->GetTransparent());

		if (hw_styleflags == STYLEHW_NoAlphaTest)
		{
			gl.Disable(GL_ALPHA_TEST);
		}
		else
		{
			gl.AlphaFunc(GL_GEQUAL,trans/2.f);
		}

		if (RenderStyle.BlendOp == STYLEOP_Fuzz)
		{
			float fuzzalpha=0.44f;
			float minalpha=0.1f;

			// fog + fuzz don't work well without some fiddling with the alpha value!
			if (!gl_isBlack(Colormap.FadeColor))
			{
				float xcamera=TO_GL(viewx);
				float ycamera=TO_GL(viewy);

				float dist=Dist2(xcamera,ycamera, x,y);

				if (!Colormap.FadeColor.a) Colormap.FadeColor.a=clamp<int>(255-lightlevel,60,255);

				// this value was determined by trial and error and is scale dependent!
				float factor=0.05f+exp(-Colormap.FadeColor.a*dist/62500.f);
				fuzzalpha*=factor;
				minalpha*=factor;
			}

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
		if (actor)
		{
			gl_SetSpriteLighting(RenderStyle, actor, lightlevel, rel, &Colormap, ThingColor, 
								 trans, fullbright, false);
		}
		else if (particle)
		{
			if (gl_light_particles)
			{
				gl_SetSpriteLight(particle, lightlevel, rel, &Colormap, trans, ThingColor);
			}
			else 
			{
				gl_SetColor(lightlevel, rel, &Colormap, trans, ThingColor);
			}
		}
		else return;
	}

	if (gl_isBlack(Colormap.FadeColor)) foglevel=lightlevel;

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

	gl_SetFog(foglevel, rel, &Colormap, additivefog);

	if (gltexture) gltexture->BindPatch(Colormap.LightColor.a,translation);
	else if (!modelframe) gl_EnableTexture(false);

	if (!modelframe)
	{
		// [BB] Billboard stuff
		const bool drawWithXYBillboard = ( !(actor && actor->renderflags & RF_FORCEYBILLBOARD)
		                                   && players[consoleplayer].camera
		                                   && (gl_billboard_mode == 1 || (actor && actor->renderflags & RF_FORCEXYBILLBOARD )) );
		gl_ApplyShader();
		gl.Begin(GL_TRIANGLE_STRIP);
		if ( drawWithXYBillboard )
		{
			// Rotate the sprite about the vector starting at the center of the sprite
			// triangle strip and with direction orthogonal to where the player is looking
			// in the x/y plane.
			float xcenter = (x1+x2)*0.5;
			float ycenter = (y1+y2)*0.5;
			float zcenter = (z1+z2)*0.5;
			float angleRad = ANGLE_TO_RAD(players[consoleplayer].camera->angle);
			
			Matrix3x4 mat;
			mat.MakeIdentity();
			mat.Translate( xcenter, zcenter, ycenter);
			mat.Rotate(-sin(angleRad), 0, cos(angleRad),-ANGLE_TO_FLOAT(players[consoleplayer].camera->pitch));
			mat.Translate( -xcenter, -zcenter, -ycenter);
			Vector v1 = mat * Vector(x1,z1,y1);
			Vector v2 = mat * Vector(x2,z1,y2);
			Vector v3 = mat * Vector(x1,z2,y1);
			Vector v4 = mat * Vector(x2,z2,y2);

			if (gltexture)
			{
				gl.TexCoord2f(ul, vt); gl.Vertex3fv(&v1[0]);
				gl.TexCoord2f(ur, vt); gl.Vertex3fv(&v2[0]);
				gl.TexCoord2f(ul, vb); gl.Vertex3fv(&v3[0]);
				gl.TexCoord2f(ur, vb); gl.Vertex3fv(&v4[0]);
			}
			else	// Particle
			{
				gl.Vertex3fv(&v1[0]);
				gl.Vertex3fv(&v2[0]);
				gl.Vertex3fv(&v3[0]);
				gl.Vertex3fv(&v4[0]);
			}

		}
		else
		{
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
		}
		gl.End();
	}
	else
	{
		gl_RenderModel(this, Colormap.LightColor.a);
	}

	if (pass==GLPASS_TRANSLUCENT)
	{
		gl_EnableBrightmap(true);
		gl.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		gl.BlendEquation(GL_FUNC_ADD);
		gl_SetTextureMode(TM_MODULATE);

		// [BB] Restore the alpha test after drawing a smooth particle.
		if (hw_styleflags == STYLEHW_NoAlphaTest)
		{
			gl.Enable(GL_ALPHA_TEST);
		}
		else
		{
			gl.AlphaFunc(GL_GEQUAL,0.5f);
		}

		if (!gl_sprite_blend && hw_styleflags != STYLEHW_Solid && actor && !(actor->momx|actor->momy))
		{
			gl.Disable(GL_POLYGON_OFFSET_FILL);
			gl.PolygonOffset(0, 0);
		}
	}

	if (!gltexture) gl_EnableTexture(true);
}


//==========================================================================
//
// 
//
//==========================================================================
inline void GLSprite::PutSprite(bool translucent)
{
	int list;
	// [BB] Allow models to be drawn in the GLDL_TRANSLUCENT pass.
	if ( translucent || (!modelframe && gl_sprite_blend))
	{
		list = GLDL_TRANSLUCENT;
	}
	else if ((!gl_isBlack (Colormap.FadeColor) || level.flags&LEVEL_HASFADETABLE))
	{
		list = GLDL_FOGMASKED;
	}
	else
	{
		list = GLDL_MASKED;
	}
	gl_drawinfo->drawlists[list].AddSprite(this);
}

//==========================================================================
//
// 
//
//==========================================================================
void GLSprite::SplitSprite(sector_t * frontsector, bool translucent)
{
	GLSprite copySprite;
	fixed_t lightbottom;
	float maplightbottom;
	unsigned int i;
	bool put=false;
	TArray<lightlist_t> & lightlist=frontsector->e->XFloor.lightlist;

	//y1+=y;
	//y2+=y;
	//y=0;
	for(i=0;i<lightlist.Size();i++)
	{
		// Particles don't go through here so we can safely assume that actor is not NULL
		if (i<lightlist.Size()-1) lightbottom=lightlist[i+1].plane.ZatPoint(actor->x,actor->y);
		else lightbottom=frontsector->floorplane.ZatPoint(actor->x,actor->y);

		//maplighttop=TO_GL(lightlist[i].height);
		maplightbottom=TO_GL(lightbottom);
		if (maplightbottom<z2) maplightbottom=z2;

		if (maplightbottom<z1)
		{
			copySprite=*this;
			copySprite.lightlevel=*lightlist[i].p_lightlevel;
			copySprite.Colormap.CopyLightColor(lightlist[i].extra_colormap);

			if (glset.nocoloredspritelighting)
			{
				int v = (copySprite.Colormap.LightColor.r + copySprite.Colormap.LightColor.g + copySprite.Colormap.LightColor.b )/3;
				copySprite.Colormap.LightColor.r=
				copySprite.Colormap.LightColor.g=
				copySprite.Colormap.LightColor.b=(255+v+v)/3;
			}

			if (!gl_isWhite(ThingColor))
			{
				copySprite.Colormap.LightColor.r=(copySprite.Colormap.LightColor.r*ThingColor.r)>>8;
				copySprite.Colormap.LightColor.g=(copySprite.Colormap.LightColor.g*ThingColor.g)>>8;
				copySprite.Colormap.LightColor.b=(copySprite.Colormap.LightColor.b*ThingColor.b)>>8;
			}

			z1=copySprite.z2=maplightbottom;
			vt=copySprite.vb=copySprite.vt+ 
				(maplightbottom-copySprite.z1)*(copySprite.vb-copySprite.vt)/(z2-copySprite.z1);
			copySprite.PutSprite(translucent);
			put=true;
		}
	}
	//if (y1<y2) PutSprite(translucent);
}


void GLSprite::SetSpriteColor(sector_t *sector, fixed_t center_y)
{
	fixed_t lightbottom;
	float maplightbottom;
	unsigned int i;
	TArray<lightlist_t> & lightlist=actor->Sector->e->XFloor.lightlist;

	for(i=0;i<lightlist.Size();i++)
	{
		// Particles don't go through here so we can safely assume that actor is not NULL
		if (i<lightlist.Size()-1) lightbottom=lightlist[i+1].plane.ZatPoint(actor->x,actor->y);
		else lightbottom=sector->floorplane.ZatPoint(actor->x,actor->y);

		//maplighttop=TO_GL(lightlist[i].height);
		maplightbottom=TO_GL(lightbottom);
		if (maplightbottom<z2) maplightbottom=z2;

		if (maplightbottom<center_y)
		{
			lightlevel=*lightlist[i].p_lightlevel;
			Colormap.CopyLightColor(lightlist[i].extra_colormap);

			if (glset.nocoloredspritelighting)
			{
				int v = (Colormap.LightColor.r + Colormap.LightColor.g + Colormap.LightColor.b )/3;
				Colormap.LightColor.r=
				Colormap.LightColor.g=
				Colormap.LightColor.b=(255+v+v)/3;
			}

			if (!gl_isWhite(ThingColor))
			{
				Colormap.LightColor.r=(Colormap.LightColor.r*ThingColor.r)>>8;
				Colormap.LightColor.g=(Colormap.LightColor.g*ThingColor.g)>>8;
				Colormap.LightColor.b=(Colormap.LightColor.b*ThingColor.b)>>8;
			}
			return;
		}
	}
}


//==========================================================================
//
// 
//
//==========================================================================
void GLSprite::Process(AActor* thing,sector_t * sector)
{
	sector_t rs;
	sector_t * rendersector;
	// don't draw the thing that's used as camera (for viewshifts during quakes!)
	if (thing==viewactor) return;

 	// invisible things
	if (thing->sprite==0) return;

	if (thing->renderflags & RF_INVISIBLE || !thing->RenderStyle.IsVisible(thing->alpha)) 
	{
		if (!(thing->flags & MF_STEALTH) || !gl_fixedcolormap || !gl_enhanced_nightvision)
			return; 
	}

	// [BB] If the actor is supposed to be invisible to the player, skip it here.
	if ( GAMEMODE_IsActorVisibleToConsoleplayersCamera( thing ) == false )
		return;

	// [RH] Interpolate the sprite's position to make it look smooth
	fixed_t thingx = thing->PrevX + FixedMul (r_TicFrac, thing->x - thing->PrevX);
	fixed_t thingy = thing->PrevY + FixedMul (r_TicFrac, thing->y - thing->PrevY);
	fixed_t thingz = thing->PrevZ + FixedMul (r_TicFrac, thing->z - thing->PrevZ);

	// too close to the camera. This doesn't look good if it is a sprite.
	if (P_AproxDistance(thingx-viewx, thingy-viewy)<2*FRACUNIT)
	{
		// exclude vertically moving objects from this check.
		if (!(thing->momx==0 && thing->momy==0 && thing->momz!=0))
		{
			if (!gl_FindModelFrame(RUNTIME_TYPE(thing), thing->sprite, thing->frame /*, thing->state*/))
			{
				return;
			}
		}
	}

	// don't draw first frame of a player missile
	if (thing->flags&MF_MISSILE && thing->target==viewactor && viewactor != NULL)
	{
		if (P_AproxDistance(thingx-viewx, thingy-viewy) < thing->Speed ) return;
	}

	if (GLPortal::mirrorline)
	{
		// this thing is behind the mirror!
		if (P_PointOnLineSide(thingx, thingy, GLPortal::mirrorline)) return;
	}


	player_t *player=&players[consoleplayer];
	GL_RECT r;

	if (sector->sectornum!=thing->Sector->sectornum)
	{
		rendersector=gl_FakeFlat(thing->Sector, &rs, false);
	}
	else
	{
		rendersector=sector;
	}
	

	x = TO_GL(thingx);
	z = TO_GL(thingz-thing->floorclip);
	y = TO_GL(thingy);
	
	modelframe = gl_FindModelFrame(RUNTIME_TYPE(thing), thing->sprite, thing->frame /*, thing->state*/);
	if (!modelframe)
	{
		angle_t ang = R_PointToAngle(thingx, thingy);

		bool mirror;
		FTextureID patch = gl_GetSpriteFrame(thing->sprite, thing->frame, -1, ang - thing->angle, &mirror);
		if (!patch.isValid()) return;
		gltexture=FGLTexture::ValidateTexture(patch, false);
		if (!gltexture) return;

		const PatchTextureInfo * pti = gltexture->GetPatchTextureInfo();
	
		vt=pti->GetVT();
		vb=pti->GetVB();
		gltexture->GetRect(&r, FGLTexture::GLUSE_PATCH);
		if (mirror)
		{
			r.left=-r.width-r.left;	// mirror the sprite's x-offset
			ul=pti->GetUL();
			ur=pti->GetUR();
		}
		else
		{
			ul=pti->GetUR();
			ur=pti->GetUL();
		}
		r.Scale(TO_GL(thing->scaleX),TO_GL(thing->scaleY));

		float rightfac=-r.left;
		float leftfac=rightfac-r.width;

		z1=z-r.top;
		z2=z1-r.height;

		// Tests show that this doesn't look good for many decorations and corpses
		if (gl_spriteclip>0)
		{
			if (((thing->player || thing->flags3&MF3_ISMONSTER || thing->IsKindOf(RUNTIME_CLASS(AInventory))) && 
				(thing->flags&MF_ICECORPSE || !(thing->flags&MF_CORPSE))) || gl_spriteclip==2)
			{
				float btm=1000000.0f;
				extsector_t::xfloor &x = thing->Sector->e->XFloor;

				if (x.ffloors.Size())
				{
					for(unsigned int i=0;i<x.ffloors.Size();i++)
					{
						F3DFloor * ff=x.ffloors[i];
						fixed_t floorh=ff->top.plane->ZatPoint(thingx, thingy);
						if (floorh==thing->floorz) 
						{
							btm=TO_GL(floorh);
							break;
						}
					}
				}
				else if (thing->Sector->heightsec && !(thing->Sector->heightsec->MoreFlags & SECF_IGNOREHEIGHTSEC))
				{
					if (thing->flags2&MF2_ONMOBJ && thing->floorz==
						thing->Sector->heightsec->floorplane.ZatPoint(thingx, thingy))
					{
						btm=TO_GL(thing->floorz);
					}
				}
				if (btm==1000000.0f) 
					btm= TO_GL(thing->Sector->floorplane.ZatPoint(thingx, thingy)-thing->floorclip);

				float diff = z2 - btm;
				if (diff >= 0 /*|| !gl_sprite_clip_to_floor*/) diff = 0;
				if (diff <=-10)	// such a large displacement can't be correct! 
				{
					// for living monsters standing on the floor allow a little more.
					if (!(thing->flags3&MF3_ISMONSTER) || (thing->flags&MF_NOGRAVITY) || (thing->flags&MF_CORPSE) || diff<-18)
					{
						diff=0;
					}
				}
				z2-=diff;
				z1-=diff;
			}
		}

		x1=x-viewvecY*leftfac;
		x2=x-viewvecY*rightfac;
		y1=y+viewvecX*leftfac;
		y2=y+viewvecX*rightfac;

		scale = fabs(viewvecX * (thing->x-viewx) + viewvecY * (thing->y-viewy));
	}
	else gltexture=NULL;

	// light calculation

	bool enhancedvision=false;

	// allow disabling of the fullbright flag by a brightmap definition
	// (e.g. to do the gun flashes of Doom's zombies correctly.
	fullbright =
		(!gl_brightmap_shader || !gltexture || !gltexture->tex->gl_info.bBrightmapDisablesFullbright) &&
		 (thing->renderflags & RF_FULLBRIGHT);

	lightlevel=fullbright? 255 : rendersector->GetTexture(sector_t::ceiling) == skyflatnum ? 
			GetCeilingLight(rendersector) : GetFloorLight(rendersector); //rendersector->lightlevel;
	foglevel = rendersector->lightlevel;

	lightlevel = (byte)gl_CheckSpriteGlow(rendersector->GetTexture(sector_t::floor), lightlevel, thingz-thing->floorz);

	// colormap stuff is a little more complicated here...
	if (gl_fixedcolormap) 
	{
		enhancedvision=true;
		Colormap.GetFixedColormap();

		if (gl_fixedcolormap==CM_LITE)
		{
			if (gl_enhanced_nightvision &&
				(thing->IsKindOf(RUNTIME_CLASS(AInventory)) || thing->flags3&MF3_ISMONSTER || thing->flags&MF_MISSILE || thing->flags&MF_CORPSE))
			{
				Colormap.LightColor.a=CM_INVERT;
			}
		}
	}
	else 
	{
		Colormap=rendersector->ColorMap;
		if (fullbright)
		{
			if (rendersector == &sectors[rendersector->sectornum] || in_area != area_below)	
				// under water areas keep their color for fullbright objects
			{
				// Only make the light white but keep everything else (fog, desaturation and Boom colormap.)
				Colormap.LightColor.r=
				Colormap.LightColor.g=
				Colormap.LightColor.b=0xff;
			}
		}
		else if (glset.nocoloredspritelighting)
		{
			int v = (Colormap.LightColor.r /* * 77 */ + Colormap.LightColor.g /**143 */ + Colormap.LightColor.b /**35*/)/3;//255;
			Colormap.LightColor.r=
			Colormap.LightColor.g=
			Colormap.LightColor.b=(255+v+v)/3;
		}
		// [BB] This makes sure that actors, which have lFixedColormap set, are renderes accordingly.
		// For example a player using a doom sphere is rendered red for the other players.
		if ( thing->lFixedColormap )
		{
			switch ( thing->lFixedColormap )
			{
			case REDCOLORMAP:

				Colormap.LightColor.a = CM_REDMAP;
				break;
			case GREENCOLORMAP:

				Colormap.LightColor.a = CM_GREENMAP;
				break;
			case GOLDCOLORMAP:

				Colormap.LightColor.a = CM_GOLDMAP;
				break;
			case NUMCOLORMAPS:

				Colormap.LightColor.a = CM_INVERT;
				break;
			default:

				break;
			}
		}
	}

	translation=thing->Translation;

	// Since it is easy to get the blood color's RGB value
	// there is no need to create multiple textures for this.
	if (GetTranslationType(translation) == TRANSLATION_Blood)
	{
		if (Colormap.LightColor.a < CM_INVERT || Colormap.LightColor.a > CM_GREENMAP)
		{
			extern TArray<PalEntry> BloodTranslationColors;

			ThingColor = BloodTranslationColors[GetTranslationIndex(translation)];
			ThingColor.a=0;
			// This is to apply desaturation to the color
			gl_ModifyColor(ThingColor.r, ThingColor.g, ThingColor.b, Colormap.LightColor.a);
			translation = TRANSLATION(TRANSLATION_Standard, 8);
		}
		else 
		{
			// Blood color must be disabled when using any monochrome colormap
			ThingColor = 0xffffff;
			translation = 0;
		}
	}
	else ThingColor=0xffffff;

	RenderStyle = thing->RenderStyle;
	RenderStyle.CheckFuzz();
	trans = TO_GL(thing->alpha);
	hw_styleflags = STYLEHW_Normal;

	if (RenderStyle.Flags & STYLEF_TransSoulsAlpha)
	{
		trans = transsouls;
	}
	else if (RenderStyle.Flags & STYLEF_Alpha1)
	{
		trans = 1.f;
	}

	if (trans >= 1.f-FLT_EPSILON && RenderStyle.BlendOp != STYLEOP_Fuzz && (
			(RenderStyle.SrcAlpha == STYLEALPHA_One && RenderStyle.DestAlpha == STYLEALPHA_Zero) ||
			(RenderStyle.SrcAlpha == STYLEALPHA_Src && RenderStyle.DestAlpha == STYLEALPHA_InvSrc)
			))
	{
		// This is a non-translucent sprite (i.e. STYLE_Normal or equivalent)
		trans=1.f;
		
		hw_styleflags = STYLEHW_Solid;
	}
	if (gltexture && gltexture->GetTransparent())
	{
		hw_styleflags = STYLEHW_NoAlphaTest;
	}

	if (enhancedvision && gl_enhanced_nightvision)
	{
		if (RenderStyle.BlendOp == STYLEOP_Fuzz)
		{
			// enhanced vision makes them more visible!
			trans=0.5f;
			RenderStyle = STYLE_Translucent;
		}
		else if (thing->flags & MF_STEALTH)	
		{
			// enhanced vision overcomes stealth!
			if (trans < 0.5f) trans = 0.5f;
		}
	}

	if (trans==0.0f) return;

	// end of light calculation

	actor=thing;
	index = gl_spriteindex++;
	particle=NULL;
	
	const bool drawWithXYBillboard = ( !(actor->renderflags & RF_FORCEYBILLBOARD)
									   && players[consoleplayer].camera
									   && (gl_billboard_mode == 1 || actor->renderflags & RF_FORCEXYBILLBOARD ) );


	if (drawWithXYBillboard || modelframe)
	{
		if (!gl_fixedcolormap && !fullbright) SetSpriteColor(actor->Sector, actor->y + (actor->height>>1));
		PutSprite(hw_styleflags != STYLEHW_Solid);
	}
	else if (thing->Sector->e->XFloor.lightlist.Size()==0 || gl_fixedcolormap || fullbright) 
	{
		PutSprite(hw_styleflags != STYLEHW_Solid);
	}
	else
	{
		SplitSprite(thing->Sector, hw_styleflags != STYLEHW_Solid);
	}
	rendered_sprites++;
}


//==========================================================================
//
// 
//
//==========================================================================

void GLSprite::ProcessParticle (particle_t *particle, sector_t *sector)//, int shade, int fakeside)
{
	if (GLPortal::mirrorline)
	{
		// this particle is  behind the mirror!
		if (P_PointOnLineSide(particle->x, particle->y, GLPortal::mirrorline)) return;
	}

	player_t *player=&players[consoleplayer];
	
	if (particle->trans==0) return;
	lightlevel = sector->lightlevel;


	if (gl_fixedcolormap) 
	{
		Colormap.GetFixedColormap();
	}
	else
	{
		TArray<lightlist_t> & lightlist=sector->e->XFloor.lightlist;
		int lightbottom;

		Colormap = sector->ColorMap;
		for(unsigned int i=0;i<lightlist.Size();i++)
		{
			if (i<lightlist.Size()-1) lightbottom = lightlist[i+1].plane.ZatPoint(particle->x,particle->y);
			else lightbottom = sector->floorplane.ZatPoint(particle->x,particle->y);

			if (lightbottom < particle->y)
			{
				lightlevel = *lightlist[i].p_lightlevel;
				Colormap.LightColor = (lightlist[i].extra_colormap)->Color;
				break;
			}
		}
	}

	trans=particle->trans/255.0f;
	RenderStyle = STYLE_Translucent;

	ThingColor = GPalette.BaseColors[particle->color];
	ThingColor.a=0;

	modelframe=NULL;
	gltexture=NULL;

	// [BB] Load the texture for round or smooth particles
	if (gl_particles_style)
	{
		FTexture *lump = NULL;
		if (gl_particles_style == 1)
		{
			lump = glpart2;
		}
		else if (gl_particles_style == 2)
		{
			lump = glpart;
		}

		if (lump != NULL)
		{
			gltexture=FGLTexture::ValidateTexture(lump);
			translation = 0;
			const PatchTextureInfo * pti = gltexture->GetPatchTextureInfo();

			vt=0.0f;
			vb=pti->GetVB();
			GL_RECT r;
			gltexture->GetRect(&r, FGLTexture::GLUSE_PATCH);
			ul=pti->GetUR();
			ur=0.0f;
		}
	}

	x= TO_GL(particle->x);
	y= TO_GL(particle->y);
	z= TO_GL(particle->z);
	
	float scalefac=particle->size/4.0f;
	// [BB] The smooth particles are smaller than the other ones. Compensate for this here.
	if (gl_particles_style==2)
		scalefac *= 1.7;

	x1=x+viewvecY*scalefac;
	x2=x-viewvecY*scalefac;
	y1=y-viewvecX*scalefac;
	y2=y+viewvecX*scalefac;
	z1=z-scalefac;
	z2=z+scalefac;
	scale = fabs(viewvecX * (particle->x-viewx) + viewvecY * (particle->y-viewy));
	actor=NULL;
	this->particle=particle;
	
	// [BB] Translucent particles have to be rendered without the alpha test.
	if (gl_particles_style != 2 && trans>=1.0f-FLT_EPSILON) hw_styleflags = STYLEHW_Solid;
	else hw_styleflags = STYLEHW_NoAlphaTest;

	PutSprite(hw_styleflags != STYLEHW_Solid);
	rendered_sprites++;
}



//===========================================================================
//
//  Gets the texture index for a sprite frame
//
//===========================================================================
extern TArray<spritedef_t> sprites;
extern TArray<spriteframe_t> SpriteFrames;

FTextureID gl_GetSpriteFrame(unsigned sprite, int frame, int rot, angle_t ang, bool *mirror)
{
	frame&=SF_FRAMEMASK;
	spritedef_t *sprdef = &sprites[sprite];
	if (frame >= sprdef->numframes)
	{
		// If there are no frames at all for this sprite, don't draw it.
		return FNullTextureID();
	}
	else
	{
		//picnum = SpriteFrames[sprdef->spriteframes + thing->frame].Texture[0];
		// choose a different rotation based on player view
		spriteframe_t *sprframe = &SpriteFrames[sprdef->spriteframes + frame];
		if (rot==-1)
		{
			if (sprframe->Texture[0] == sprframe->Texture[1])
			{
				rot = (ang + (angle_t)(ANGLE_45/2)*9) >> 28;
			}
			else
			{
				rot = (ang + (angle_t)(ANGLE_45/2)*9-(angle_t)(ANGLE_180/16)) >> 28;
			}
		}
		if (mirror) *mirror = !!(sprframe->Flip&(1<<rot));
		return sprframe->Texture[rot];
	}
}

