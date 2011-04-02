/*
** gl_weapon.cpp
** Weapon sprite drawing
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
#include "sbar.h"
#include "doomstat.h"
#include "gl/gl_struct.h"
#include "gl/gl_glow.h"
#include "gl/gl_texture.h"
#include "gl/gl_functions.h"
#include "gl/gl_intern.h"
#include "gl/gl_shader.h"
#include "gl/gl_models.h"

EXTERN_CVAR (Bool, r_drawplayersprites)
EXTERN_CVAR(Float, transsouls)

//==========================================================================
//
// R_DrawPSprite
//
//==========================================================================

static void DrawPSprite (player_t * player,pspdef_t *psp,fixed_t sx, fixed_t sy, int cm_index, bool hudModelStep)
{
	float			fU1,fV1;
	float			fU2,fV2;
	fixed_t			tx;
	int				x1,y1,x2,y2;
	float			scale;
	fixed_t			scalex;
	fixed_t			texturemid;
	
	// [BB] In the HUD model step we just render the model and break out. 
	if ( hudModelStep )
	{
		gl_RenderHUDModel( psp, sx, sy, cm_index );
		return;
	}

	// decide which patch to use
	bool mirror;
	FTextureID lump = gl_GetSpriteFrame(psp->state->sprite, psp->state->GetFrame(), 0, 0, &mirror);
	if (!lump.isValid()) return;

	FGLTexture * tex=FGLTexture::ValidateTexture(lump, false);
	if (!tex) return;

	const PatchTextureInfo * pti = tex->BindPatch(cm_index, 0);
	if (!pti) return;

	int vw = viewwidth;
	int vh = viewheight;

	// calculate edges of the shape
	scalex=FRACUNIT * vw / 320 * BaseRatioSizes[WidescreenRatio][3] / 48;

	tx = sx - ((160 + tex->GetScaledLeftOffset(FGLTexture::GLUSE_PATCH))<<FRACBITS);
	x1 = (FixedMul(tx, scalex)>>FRACBITS) + (vw>>1);
	if (x1 > vw)	return; // off the right side
	x1+=viewwindowx;

	tx +=  tex->TextureWidth(FGLTexture::GLUSE_PATCH) << FRACBITS;
	x2 = (FixedMul(tx, scalex)>>FRACBITS) + (vw>>1);
	if (x2 < 0) return; // off the left side
	x2+=viewwindowx;

	// killough 12/98: fix psprite positioning problem
	texturemid = (100<<FRACBITS) - (sy-(tex->GetScaledTopOffset(FGLTexture::GLUSE_PATCH)<<FRACBITS));

	AWeapon * wi=player->ReadyWeapon;
	if (wi && wi->YAdjust && screenblocks>=11) texturemid -= wi->YAdjust;

	scale = ((SCREENHEIGHT*vw)/SCREENWIDTH) / 200.0f;    
	y1=viewwindowy+(vh>>1)-(int)(((float)texturemid/(float)FRACUNIT)*scale);
	y2=y1+(int)((float)tex->TextureHeight(FGLTexture::GLUSE_PATCH)*scale)+1;

	if (!mirror)
	{
		fU1=pti->GetUL();
		fV1=pti->GetVT();
		fU2=pti->GetUR();
		fV2=pti->GetVB();
	}
	else
	{
		fU2=pti->GetUL();
		fV1=pti->GetVT();
		fU1=pti->GetUR();
		fV2=pti->GetVB();
	}

	gl_ApplyShader();
	gl.Begin(GL_TRIANGLE_STRIP);
	gl.TexCoord2f(fU1, fV1); gl.Vertex2f(x1,y1);
	gl.TexCoord2f(fU1, fV2); gl.Vertex2f(x1,y2);
	gl.TexCoord2f(fU2, fV1); gl.Vertex2f(x2,y1);
	gl.TexCoord2f(fU2, fV2); gl.Vertex2f(x2,y2);
	gl.End();
}

//==========================================================================
//
// R_DrawPlayerSprites
//
//==========================================================================

void gl_DrawPlayerSprites(sector_t * viewsector, bool hudModelStep)
{
	bool statebright[2] = {false, false};
	unsigned int i;
	pspdef_t *psp;
	int lightlevel=0;
	fixed_t ofsx, ofsy;
	FColormap cm;
	sector_t * fakesec, fs;
	AActor * playermo=players[consoleplayer].camera;
	player_t * player=playermo->player;
	
	if(!player || playermo->renderflags&RF_INVISIBLE || !r_drawplayersprites ||
		viewactor!=playermo || playermo->RenderStyle.BlendOp == STYLEOP_None) return;

	P_BobWeapon (player, &player->psprites[ps_weapon], &ofsx, &ofsy);

	// check for fullbright
	if (player->fixedcolormap==0)
	{
		for (i=0, psp=player->psprites; i<=ps_flash; i++,psp++)
			if (psp->state != NULL) statebright[i] = !!psp->state->GetFullbright();
	}

	if (gl_fixedcolormap) 
	{
		lightlevel=255;
		cm.GetFixedColormap();
		statebright[0] = statebright[1] = true;
	}
	else
	{
		fakesec    = gl_FakeFlat(viewsector, &fs, false);

		// calculate light level for weapon sprites
		lightlevel = fakesec->lightlevel;

		lightlevel = gl_CheckSpriteGlow(viewsector->GetTexture(sector_t::floor), lightlevel, playermo->z-playermo->floorz);

		// calculate colormap for weapon sprites
		if (viewsector->e->XFloor.ffloors.Size() && !glset.nocoloredspritelighting)
		{
			TArray<lightlist_t> & lightlist = viewsector->e->XFloor.lightlist;
			for(i=0;i<lightlist.Size();i++)
			{
				int lightbottom;

				if (i<lightlist.Size()-1) 
				{
					lightbottom=lightlist[i+1].plane.ZatPoint(viewx,viewy);
				}
				else 
				{
					lightbottom=viewsector->floorplane.ZatPoint(viewx,viewy);
				}

				if (lightbottom<player->viewz) 
				{
					cm = lightlist[i].extra_colormap;
					lightlevel = *lightlist[i].p_lightlevel;
					break;
				}
			}
		}
		else 
		{
			cm=fakesec->ColorMap;
			if (glset.nocoloredspritelighting) cm.ClearColor();
		}
	}

	PalEntry ThingColor = playermo->fillcolor;
	vissprite_t vis;

	vis.RenderStyle=playermo->RenderStyle;
	vis.alpha=playermo->alpha;
	if (playermo->Inventory) playermo->Inventory->AlterWeaponSprite(&vis);

	// Set the render parameters
	vis.RenderStyle.CheckFuzz();
	gl_SetRenderStyle(vis.RenderStyle, false, false);

	float trans;
	if (vis.RenderStyle.Flags & STYLEF_TransSoulsAlpha)
	{
		trans = transsouls;
	}
	else if (vis.RenderStyle.Flags & STYLEF_Alpha1)
	{
		trans = 1.f;
	}
	else
	{
		trans = TO_GL(vis.alpha);
	}

	// now draw the different layers of the weapon
	gl_EnableBrightmap(true);
	for (i=0, psp=player->psprites; i<=ps_flash; i++,psp++)
	{
		if (psp->state) 
		{
			// set the lighting parameters (only calls glColor and glAlphaFunc)
			gl_SetSpriteLighting(vis.RenderStyle, playermo, statebright[i]? 255 : lightlevel, 
				0, &cm, 0xffffff, trans, statebright[i], true);
			DrawPSprite (player,psp,psp->sx+ofsx, psp->sy+ofsy, cm.LightColor.a, hudModelStep);
		}
	}
	gl_EnableBrightmap(false);
}

//==========================================================================
//
// R_DrawPlayerSprites
//
//==========================================================================

void gl_DrawTargeterSprites()
{
	int i;
	pspdef_t *psp;
	AActor * playermo=players[consoleplayer].camera;
	player_t * player=playermo->player;
	
	if(!player || playermo->renderflags&RF_INVISIBLE || !r_drawplayersprites ||
		viewactor!=playermo) return;

	gl_EnableBrightmap(false);
	gl.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	gl.AlphaFunc(GL_GEQUAL,0.5f);
	gl.BlendEquation(GL_FUNC_ADD);
	gl.Color3f(1.0f,1.0f,1.0f);
	gl_SetTextureMode(TM_MODULATE);

	// The Targeter's sprites are always drawn normally.
	for (i=ps_targetcenter, psp = &player->psprites[ps_targetcenter]; i<NUMPSPRITES; i++,psp++)
		if (psp->state) DrawPSprite (player,psp,psp->sx, psp->sy, CM_DEFAULT, false);
}

