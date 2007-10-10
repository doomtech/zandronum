#include "gl_pch.h"

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
#include "sbar.h"
#include "gl/gl_struct.h"
#include "gl/gl_glow.h"
#include "gl/gl_texture.h"
#include "gl/gl_functions.h"
#include "gl/gl_intern.h"
#include "gl/gl_shader.h"

EXTERN_CVAR (Bool, r_drawplayersprites)
EXTERN_CVAR(Bool, gl_nocoloredspritelighting)

//==========================================================================
//
// R_DrawPSprite
//
//==========================================================================

static void DrawPSprite (player_t * player,pspdef_t *psp,fixed_t sx, fixed_t sy, int cm_index)
{
	float			fU2,fV2;
	fixed_t			tx;
	int				x1,y1,x2,y2;
	float			scale;
	fixed_t			scalex;
	fixed_t			texturemid;
	
	// decide which patch to use
	int lump=abs(gl_GetSpriteFrame(psp->state->sprite.index, psp->state->GetFrame(), 0));
	if (lump==INVALID_SPRITE) return;

	FGLTexture * tex=FGLTexture::ValidateTexture(lump, false);
	if (!tex) return;

	const PatchTextureInfo * pti = tex->BindPatch(cm_index);
	if (!pti) return;

	int vw = realviewwidth;
	int vh = realviewheight;

	// calculate edges of the shape
	scalex=FRACUNIT * vw / 320 * BaseRatioSizes[WidescreenRatio][3] / 48;

	tx = sx - ((160 + tex->GetScaledLeftOffset())<<FRACBITS);
	x1 = (FixedMul(tx, scalex)>>FRACBITS) + (vw>>1);
	if (x1 > vw)	return; // off the right side
	x1+=viewwindowx;

	tx +=  tex->TextureWidth() << FRACBITS;
	x2 = (FixedMul(tx, scalex)>>FRACBITS) + (vw>>1);
	if (x2 < 0) return; // off the left side
	x2+=viewwindowx;

	// killough 12/98: fix psprite positioning problem
	texturemid = (100<<FRACBITS) - (sy-(tex->GetScaledTopOffset()<<FRACBITS));

	AWeapon * wi=player->ReadyWeapon;
	if (wi && wi->YAdjust && screenblocks>=11) texturemid -= wi->YAdjust;

	scale = ((SCREENHEIGHT*vw)/SCREENWIDTH) / 200.0f;    
	y1=viewwindowy+(vh>>1)-(int)(((float)texturemid/(float)FRACUNIT)*scale);
	y2=y1+(int)((float)tex->TextureHeight()*scale)+1;

	fU2=pti->GetUR();
	fV2=pti->GetVB();

	gl.Begin(GL_TRIANGLE_STRIP);
	gl.TexCoord2f(0  , 0  ); gl.Vertex2f(x1,y1);
	gl.TexCoord2f(0  , fV2); gl.Vertex2f(x1,y2);
	gl.TexCoord2f(fU2, 0  ); gl.Vertex2f(x2,y1);
	gl.TexCoord2f(fU2, fV2); gl.Vertex2f(x2,y2);
	gl.End();
}

//==========================================================================
//
// R_DrawPlayerSprites
//
//==========================================================================

void gl_DrawPlayerSprites(sector_t * viewsector)
{
	int i;
	pspdef_t *psp;
	bool fullbright=false;
	int lightlevel=0;
	fixed_t ofsx, ofsy;
	FColormap cm;
	sector_t * fakesec, fs;
	AActor * playermo=players[consoleplayer].camera;
	player_t * player=playermo->player;
	
	if(!player || playermo->renderflags&RF_INVISIBLE || !r_drawplayersprites ||
		viewactor!=playermo || playermo->RenderStyle==STYLE_None) return;

	// check for fullbright
	if (player->fixedcolormap==0)
	{
		for (i=0, psp=player->psprites; i<=ps_flash; i++,psp++)
			if (psp->state && psp->state->GetFullbright()) fullbright=true;
	}

	if (gl_fixedcolormap) 
	{
		lightlevel=255;
		cm.GetFixedColormap();
		fullbright=true;
	}
	else
	{
		fakesec    = gl_FakeFlat(viewsector, &fs, false);

		// calculate light level for weapon sprites
		lightlevel = fakesec->lightlevel;

		lightlevel = gl_CheckSpriteGlow(viewsector->floorpic, lightlevel, playermo->z-playermo->floorz);

		// calculate colormap for weapon sprites
		if (viewsector->e->ffloors.Size() && !gl_nocoloredspritelighting)
		{
			TArray<lightlist_t> & lightlist = viewsector->e->lightlist;
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
					cm = *lightlist[i].p_extra_colormap;
					lightlevel = *lightlist[i].p_lightlevel;
					break;
				}
			}
		}
		else 
		{
			cm=fakesec->ColorMap;
			if (gl_nocoloredspritelighting) cm.ClearColor();
		}
		if (!fullbright)
		{
			lightlevel = gl_CalcLightLevel(lightlevel);
		}
		else 
		{
			lightlevel=255;
			//if (area!=area_below) cm.ClearColor();
			// keep desaturation/colormap!
		}
	}

	
	vissprite_t vis;
	vis.RenderStyle=playermo->RenderStyle;
	vis.alpha=playermo->alpha;
	if (playermo->Inventory) playermo->Inventory->AlterWeaponSprite(&vis);

	// Set light and blend mode
	switch(vis.RenderStyle)
	{											
	case STYLE_OptFuzzy:
	case STYLE_Fuzzy:
		gl.BlendFunc(GL_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA);
		gl.AlphaFunc(GL_GEQUAL,0.1f);
		gl.Color4f(0.2f,0.2f,0.2f,0.33f);
		break;
	/*
	case STYLE_Subtract:
		gl.BlendEquation(GL_FUNC_REVERSE_SUBTRACT);
	*/
	case STYLE_Add:
	case STYLE_Translucent:
		if (vis.RenderStyle == STYLE_Add) gl.BlendFunc(GL_SRC_ALPHA,GL_ONE);
		gl.AlphaFunc(GL_GEQUAL, 0.5f * TO_MAP(abs(vis.alpha)));
		gl_SetColor(lightlevel, 0, &cm, TO_MAP(vis.alpha), (PalEntry)0xffffff, true);
		break;
	case STYLE_Normal:

		if (gl_light_sprites && gl_lights && !fullbright)
		{
			gl_SetSpriteLight(playermo, lightlevel, 0, &cm, 1.0, (PalEntry)0xffffff, true);
		}
		else
		{
			gl_SetColor(lightlevel, 0, &cm, 1.0f, (PalEntry)0xffffff, true);
		}
		break;

	}

	P_BobWeapon (player, &player->psprites[ps_weapon], &ofsx, &ofsy);

	// now draw the different layers of the weapon
	gl_EnableBrightmap(true);
	for (i=0, psp=player->psprites; i<=ps_flash; i++,psp++)
		if (psp->state) DrawPSprite (player,psp,psp->sx+ofsx, psp->sy+ofsy, cm.LightColor.a);
	gl_EnableBrightmap(false);

	// Restore default settings
	switch(vis.RenderStyle)
	{
	case STYLE_Stencil:
	case STYLE_TranslucentStencil:
	case STYLE_Fuzzy:
	case STYLE_Add:
	case STYLE_Translucent:
		gl.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		gl.AlphaFunc(GL_GEQUAL,0.5f);
		break;
	}
	gl.Color3f(1.0f,1.0f,1.0f);

	// The Targeter's sprites are always drawn normally!
	for (; i<NUMPSPRITES; i++,psp++)
		if (psp->state) DrawPSprite (player,psp,psp->sx, psp->sy, CM_DEFAULT);

}

