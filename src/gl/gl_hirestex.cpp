
/*
** Hires texture management
**
**---------------------------------------------------------------------------
** Copyright 2005 Christoph Oelckers
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
#include "gl/gl_include.h"
#include "w_wad.h"
#include "m_png.h"
#include "r_draw.h"
#include "sbar.h"
#include "gi.h"
#include "cmdlib.h"
#include "sc_man.h"

#include "gl/gl_struct.h"
#include "gl/gl_texture.h"
#include "gl/gl_functions.h"

//==========================================================================
//
// Checks for the presence of a hires texture replacement in a Doomsday style PK3
//
//==========================================================================
int FGLTexture::CheckDDPK3()
{
	static const char * doom1texpath[]= {
		"data/jdoom/textures/doom/%s.%s", "data/jdoom/textures/doom-ult/%s.%s", "data/jdoom/textures/doom1/%s.%s", "data/jdoom/textures/%s.%s", NULL };

	static const char * doom2texpath[]= {
		"data/jdoom/textures/doom2/%s.%s", "data/jdoom/textures/%s.%s", NULL };

	static const char * pluttexpath[]= {
		"data/jdoom/textures/doom2-plut/%s.%s", "data/jdoom/textures/plutonia/%s.%s", "data/jdoom/textures/%s.%s", NULL };

	static const char * tnttexpath[]= {
		"data/jdoom/textures/doom2-tnt/%s.%s", "data/jdoom/textures/tnt/%s.%s", "data/jdoom/textures/%s.%s", NULL };

	static const char * heretictexpath[]= {
		"data/jheretic/textures/%s.%s", NULL };

	static const char * hexentexpath[]= {
		"data/jhexen/textures/%s.%s", NULL };

	static const char * strifetexpath[]= {
		"data/jstrife/textures/%s.%s", NULL };

	static const char * doomflatpath[]= {
		"data/jdoom/flats/%s.%s", NULL };

	static const char * hereticflatpath[]= {
		"data/jheretic/flats/%s.%s", NULL };

	static const char * hexenflatpath[]= {
		"data/jhexen/flats/%s.%s", NULL };

	static const char * strifeflatpath[]= {
		"data/jstrife/flats/%s.%s", NULL };


	FString checkName;
	const char ** checklist;
	BYTE useType=tex->UseType;

	if (useType==FTexture::TEX_SkinSprite || useType==FTexture::TEX_Decal || useType==FTexture::TEX_FontChar)
	{
		return -3;
	}

	bool ispatch = (useType==FTexture::TEX_MiscPatch || useType==FTexture::TEX_Sprite) ;

	// for patches this doesn't work yet
	if (ispatch) return -3;

	switch (gameinfo.gametype)
	{
	case GAME_Doom:
		switch (gamemission)
		{
		case doom:
			checklist = useType==FTexture::TEX_Flat? doomflatpath : doom1texpath;
			break;
		case doom2:
			checklist = useType==FTexture::TEX_Flat? doomflatpath : doom2texpath;
			break;
		case pack_tnt:
			checklist = useType==FTexture::TEX_Flat? doomflatpath : tnttexpath;
			break;
		case pack_plut:
			checklist = useType==FTexture::TEX_Flat? doomflatpath : pluttexpath;
			break;
		default:
			return -3;
		}
		break;

	case GAME_Heretic:
		checklist = useType==FTexture::TEX_Flat? hereticflatpath : heretictexpath;
		break;
	case GAME_Hexen:
		checklist = useType==FTexture::TEX_Flat? hexenflatpath : hexentexpath;
		break;
	case GAME_Strife:
		checklist = useType==FTexture::TEX_Flat? strifeflatpath : strifetexpath;
		break;
	default:
		return -3;
	}

	while (*checklist)
	{
		static const char * extensions[] = { "PNG", "JPG", "TGA", "PCX", NULL };

		for (const char ** extp=extensions; *extp; extp++)
		{
			checkName.Format(*checklist, tex->Name, *extp);
			int lumpnum = Wads.CheckNumForFullName(checkName);
			if (lumpnum >= 0) return lumpnum;
		}
		checklist++;
	}
	return -3;
}


//==========================================================================
//
// Checks for the presence of a hires texture replacement
//
//==========================================================================
int FGLTexture::CheckExternalFile(bool & hascolorkey)
{
	static const char * doom1texpath[]= {
		"%stextures/doom/doom1/%s.%s", "%stextures/doom/doom1/%s-ck.%s", 
			"%stextures/doom/%s.%s", "%stextures/doom/%s-ck.%s", "%stextures/%s.%s", "%stextures/%s-ck.%s", NULL
	};

	static const char * doom2texpath[]= {
		"%stextures/doom/doom2/%s.%s", "%stextures/doom/doom2/%s-ck.%s", 
			"%stextures/doom/%s.%s", "%stextures/doom/%s-ck.%s", "%stextures/%s.%s", "%stextures/%s-ck.%s", NULL
	};

	static const char * pluttexpath[]= {
		"%stextures/doom/plut/%s.%s", "%stextures/doom/plut/%s-ck.%s", 
		"%stextures/doom/doom2-plut/%s.%s", "%stextures/doom/doom2-plut/%s-ck.%s", 
			"%stextures/doom/%s.%s", "%stextures/doom/%s-ck.%s", "%stextures/%s.%s", "%stextures/%s-ck.%s", NULL
	};

	static const char * tnttexpath[]= {
		"%stextures/doom/tnt/%s.%s", "%stextures/doom/tnt/%s-ck.%s", 
		"%stextures/doom/doom2-tnt/%s.%s", "%stextures/doom/doom2-tnt/%s-ck.%s", 
			"%stextures/doom/%s.%s", "%stextures/doom/%s-ck.%s", "%stextures/%s.%s", "%stextures/%s-ck.%s", NULL
	};

	static const char * heretictexpath[]= {
		"%stextures/heretic/%s.%s", "%stextures/heretic/%s-ck.%s", "%stextures/%s.%s", "%stextures/%s-ck.%s", NULL
	};

	static const char * hexentexpath[]= {
		"%stextures/hexen/%s.%s", "%stextures/hexen/%s-ck.%s", "%stextures/%s.%s", "%stextures/%s-ck.%s", NULL
	};

	static const char * strifetexpath[]= {
		"%stextures/strife/%s.%s", "%stextures/strife/%s-ck.%s", "%stextures/%s.%s", "%stextures/%s-ck.%s", NULL
	};

	static const char * doom1flatpath[]= {
		"%sflats/doom/doom1/%s.%s", "%stextures/doom/doom1/flat-%s.%s", 
			"%sflats/doom/%s.%s", "%stextures/doom/flat-%s.%s", "%sflats/%s.%s", "%stextures/flat-%s.%s", NULL
	};

	static const char * doom2flatpath[]= {
		"%sflats/doom/doom2/%s.%s", "%stextures/doom/doom2/flat-%s.%s", 
			"%sflats/doom/%s.%s", "%stextures/doom/flat-%s.%s", "%sflats/%s.%s", "%stextures/flat-%s.%s", NULL
	};

	static const char * plutflatpath[]= {
		"%sflats/doom/plut/%s.%s", "%stextures/doom/plut/flat-%s.%s", 
		"%sflats/doom/doom2-plut/%s.%s", "%stextures/doom/doom2-plut/flat-%s.%s", 
			"%sflats/doom/%s.%s", "%stextures/doom/flat-%s.%s", "%sflats/%s.%s", "%stextures/flat-%s.%s", NULL
	};

	static const char * tntflatpath[]= {
		"%sflats/doom/tnt/%s.%s", "%stextures/doom/tnt/flat-%s.%s", 
		"%sflats/doom/doom2-tnt/%s.%s", "%stextures/doom/doom2-tnt/flat-%s.%s", 
			"%sflats/doom/%s.%s", "%stextures/doom/flat-%s.%s", "%sflats/%s.%s", "%stextures/flat-%s.%s", NULL
	};

	static const char * hereticflatpath[]= {
		"%sflats/heretic/%s.%s", "%stextures/heretic/flat-%s.%s", "%sflats/%s.%s", "%stextures/flat-%s.%s", NULL
	};

	static const char * hexenflatpath[]= {
		"%sflats/hexen/%s.%s", "%stextures/hexen/flat-%s.%s", "%sflats/%s.%s", "%stextures/flat-%s.%s", NULL
	};

	static const char * strifeflatpath[]= {
		"%sflats/strife/%s.%s", "%stextures/strife/flat-%s.%s", "%sflats/%s.%s", "%stextures/flat-%s.%s", NULL
	};

	static const char * doom1patchpath[]= {
		"%spatches/doom/doom1/%s.%s", "%spatches/doom/%s.%s", "%spatches/%s.%s", NULL
	};

	static const char * doom2patchpath[]= {
		"%spatches/doom/doom2/%s.%s", "%spatches/doom/%s.%s", "%spatches/%s.%s", NULL
	};

	static const char * plutpatchpath[]= {
		"%spatches/doom/plut/%s.%s", "%spatches/doom/%s.%s", "%spatches/%s.%s", NULL
	};

	static const char * tntpatchpath[]= {
		"%spatches/doom/tnt/%s.%s", "%spatches/doom/%s.%s", "%spatches/%s.%s", NULL
	};

	static const char * hereticpatchpath[]= {
		"%spatches/heretic/%s.%s", "%spatches/%s.%s", NULL
	};

	static const char * hexenpatchpath[]= {
		"%spatches/hexen/%s.%s", "%spatches/%s.%s", NULL
	};

	static const char * strifepatchpath[]= {
		"%spatches/strife/%s.%s", "%spatches/%s.%s", NULL
	};

	FString checkName;
	const char ** checklist;
	BYTE useType=tex->UseType;

	if (useType==FTexture::TEX_SkinSprite || useType==FTexture::TEX_Decal || useType==FTexture::TEX_FontChar)
	{
		return -3;
	}

	bool ispatch = (useType==FTexture::TEX_MiscPatch || useType==FTexture::TEX_Sprite) ;

	// for patches this doesn't work yet
	if (ispatch) return -3;

	switch (gameinfo.gametype)
	{
	case GAME_Doom:
		switch (gamemission)
		{
		case doom:
			checklist = ispatch ? doom1patchpath : useType==FTexture::TEX_Flat? doom1flatpath : doom1texpath;
			break;
		case doom2:
			checklist = ispatch ? doom2patchpath : useType==FTexture::TEX_Flat? doom2flatpath : doom2texpath;
			break;
		case pack_tnt:
			checklist = ispatch ? tntpatchpath : useType==FTexture::TEX_Flat? tntflatpath : tnttexpath;
			break;
		case pack_plut:
			checklist = ispatch ? plutpatchpath : useType==FTexture::TEX_Flat? plutflatpath : pluttexpath;
			break;
		default:
			return -3;
		}
		break;

	case GAME_Heretic:
		checklist = ispatch ? hereticpatchpath : useType==FTexture::TEX_Flat? hereticflatpath : heretictexpath;
		break;
	case GAME_Hexen:
		checklist = ispatch ? hexenpatchpath : useType==FTexture::TEX_Flat? hexenflatpath : hexentexpath;
		break;
	case GAME_Strife:
		checklist = ispatch ?strifepatchpath : useType==FTexture::TEX_Flat? strifeflatpath : strifetexpath;
		break;
	default:
		return -3;
	}

	while (*checklist)
	{
		static const char * extensions[] = { "PNG", "JPG", "TGA", "PCX", NULL };

		for (const char ** extp=extensions; *extp; extp++)
		{
			checkName.Format(*checklist, progdir, tex->Name, *extp);
			if (_access(checkName, 0) == 0) 
			{
				hascolorkey = !!strstr(checkName, "-ck.");
				return Wads.AddExternalFile(checkName);
			}
		}
		checklist++;
	}
	return -3;
}


//==========================================================================
//
// Checks for the presence of a hires texture replacement and loads it
//
//==========================================================================
unsigned char *FGLTexture::LoadHiresTexture(int *width, int *height)
{
	if (HiresLump==-1) 
	{
		bHasColorkey = false;
		HiresLump = CheckDDPK3();
		if (HiresLump < 0) HiresLump = CheckExternalFile(bHasColorkey);

		if (HiresLump >=0) 
		{
			hirestexture = FTexture::CreateTexture(HiresLump, FTexture::TEX_Any);
		}
	}
	if (hirestexture != NULL)
	{
		int w=hirestexture->GetWidth();
		int h=hirestexture->GetHeight();

		unsigned char * buffer=new unsigned char[w*(h+1)*4];
		memset(buffer, 0, w * (h+1) * 4);

		
		int trans = hirestexture->CopyTrueColorPixels(buffer, w<<2, h, 0, 0);
		CheckTrans(buffer, w*h, trans);

		if (bHasColorkey)
		{
			// This is a crappy Doomsday color keyed image
			// We have to remove the key manually. :(
			DWORD * dwdata=(DWORD*)buffer;
			for (int i=(w*h);i>0;i--)
			{
				if (dwdata[i]==0xffffff00 || dwdata[i]==0xffff00ff) dwdata[i]=0;
			}
		}
		*width = w;
		*height = h;
		return buffer;
	}
	return NULL;
}
