/*
** buildtexture.cpp
** Handling Build textures
**
**---------------------------------------------------------------------------
** Copyright 2004-2006 Randy Heit
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
**
*/

#include "doomtype.h"
#include "files.h"
#include "r_local.h"
#include "w_wad.h"
#include "templates.h"
#include "cmdlib.h"

static TArray<BYTE *> BuildTileFiles;

FBuildTexture::FBuildTexture (int tilenum, const BYTE *pixels, int width, int height, int left, int top)
: Pixels (pixels), Spans (NULL)
{
	Width = width;
	Height = height;
	LeftOffset = left;
	TopOffset = top;
	CalcBitSize ();
	sprintf (Name, "BTIL%04d", tilenum);
	UseType = TEX_Build;
}

FBuildTexture::~FBuildTexture ()
{
	if (Spans != NULL)
	{
		FreeSpans (Spans);
		Spans = NULL;
	}
}

void FBuildTexture::Unload ()
{
	// Nothing to do, since the pixels are accessed from memory-mapped files directly
}

const BYTE *FBuildTexture::GetPixels ()
{
	return Pixels;
}

const BYTE *FBuildTexture::GetColumn (unsigned int column, const Span **spans_out)
{
	if (column >= Width)
	{
		if (WidthMask + 1 == Width)
		{
			column &= WidthMask;
		}
		else
		{
			column %= Width;
		}
	}
	if (spans_out != NULL)
	{
		if (Spans == NULL)
		{
			Spans = CreateSpans (Pixels);
		}
		*spans_out = Spans[column];
	}
	return Pixels + column*Height;
}



static void AddTiles (void *tiles)
{
//	int numtiles = LittleLong(((DWORD *)tiles)[1]);	// This value is not reliable
	int tilestart = LittleLong(((DWORD *)tiles)[2]);
	int tileend = LittleLong(((DWORD *)tiles)[3]);
	const WORD *tilesizx = &((const WORD *)tiles)[8];
	const WORD *tilesizy = &tilesizx[tileend - tilestart + 1];
	const DWORD *picanm = (const DWORD *)&tilesizy[tileend - tilestart + 1];
	BYTE *tiledata = (BYTE *)&picanm[tileend - tilestart + 1];

	for (int i = tilestart; i <= tileend; ++i)
	{
		int pic = i - tilestart;
		int width = LittleShort(tilesizx[pic]);
		int height = LittleShort(tilesizy[pic]);
		DWORD anm = LittleLong(picanm[pic]);
		int xoffs = (SBYTE)((anm >> 8) & 255) + width/2;
		int yoffs = (SBYTE)((anm >> 16) & 255) + height/2;
		int size = width*height;
		int texnum;
		FTexture *tex;

		if (width <= 0 || height <= 0) continue;

		tex = new FBuildTexture (i, tiledata, width, height, xoffs, yoffs);
		texnum = TexMan.AddTexture (tex);
		while (size > 0)
		{
			*tiledata = 255 - *tiledata;
			tiledata++;
			size--;
		}

		if ((picanm[pic] & 63) && (picanm[pic] & 192))
		{
			int type, speed;

			switch (picanm[pic] & 192)
			{
			case 64:	type = 2;	break;
			case 128:	type = 0;	break;
			case 192:	type = 1;	break;
			default:    type = 0;   break;  // Won't happen, but GCC bugs me if I don't put this here.
			}

			speed = (anm >> 24) & 15;
			speed = MAX (1, (1 << speed) * 1000 / 120);	// Convert from 120 Hz to 1000 Hz.

			R_AddSimpleAnim (texnum, picanm[pic] & 63, type, speed);
		}

		// Blood's rotation types:
		// 0 - Single
		// 1 - 5 Full
		// 2 - 8 Full
		// 3 - Bounce (looks no different from Single; seems to signal bouncy sprites)
		// 4 - 5 Half (not used in game)
		// 5 - 3 Flat (not used in game)
		// 6 - Voxel
		// 7 - Spin Voxel

		int rotType = (anm >> 28) & 7;
		if (rotType == 1)
		{
			spriteframe_t rot;
			rot.Texture[0] = texnum;
			rot.Texture[1] = texnum;
			for (int j = 1; j < 4; ++j)
			{
				rot.Texture[j*2] = texnum + j;
				rot.Texture[j*2+1] = texnum + j;
				rot.Texture[16-j*2] = texnum + j;
				rot.Texture[17-j*2] = texnum + j;
			}
			rot.Texture[8] = texnum + 4;
			rot.Texture[9] = texnum + 4;
			rot.Flip = 0x00FC;
			tex->Rotations = SpriteFrames.Push (rot);
		}
		else if (rotType == 2)
		{
			spriteframe_t rot;
			rot.Texture[0] = texnum;
			rot.Texture[1] = texnum;
			for (int j = 1; j < 8; ++j)
			{
				rot.Texture[16-j*2] = texnum + j;
				rot.Texture[17-j*2] = texnum + j;
			}
			rot.Flip = 0;
			tex->Rotations = SpriteFrames.Push (rot);
		}
	}
}

//
// R_InitBuildTiles
//
// [RH] Support Build tiles!
//

void R_InitBuildTiles ()
{
	int numartfiles = 0;
	char artfile[] = "tilesXXX.art";
	int lumpnum;

	lumpnum = Wads.CheckNumForFullName ("blood.pal");
	if (lumpnum >= 0)
	{
		// Blood's tiles are external resources. (Why did they do it like that?)
		FString rffpath = Wads.GetWadFullName (Wads.GetLumpFile (lumpnum));
		int slashat = rffpath.LastIndexOf ('/');
		if (slashat >= 0)
		{
			rffpath.Truncate (slashat + 1);
		}
		else
		{
			rffpath += '/';
		}

		for (; numartfiles < 1000; numartfiles++)
		{
			artfile[5] = numartfiles / 100 + '0';
			artfile[6] = numartfiles / 10 % 10 + '0';
			artfile[7] = numartfiles % 10 + '0';

			FString artpath = rffpath;
			artpath += artfile;

			FILE *f = fopen (artpath, "rb");
			if (f == NULL)
			{
				break;
			}

			size_t len = Q_filelength (f);
			BYTE *art = new BYTE[len];
			if (fread (art, 1, len, f) != len || LittleLong(*(DWORD *)art) != 1)
			{
				delete[] art;
			}
			else
			{
				BuildTileFiles.Push (art);
				AddTiles (art);
			}
			fclose (f);
		}
	}

	for (; numartfiles < 1000; numartfiles++)
	{
		artfile[5] = numartfiles / 100 + '0';
		artfile[6] = numartfiles / 10 % 10 + '0';
		artfile[7] = numartfiles % 10 + '0';
		lumpnum = Wads.CheckNumForFullName (artfile);
		if (lumpnum < 0)
		{
			break;
		}

		BYTE *art = new BYTE[Wads.LumpLength (lumpnum)];
		Wads.ReadLump (lumpnum, art);

		if (LittleLong(*(DWORD *)art) != 1)
		{
			delete[] art;
		}
		else
		{
			BuildTileFiles.Push (art);
			AddTiles (art);
		}
	}
}

void R_DeinitBuildTiles ()
{
	for (unsigned int i = 0; i < BuildTileFiles.Size(); ++i)
	{
		delete[] BuildTileFiles[i];
	}
	BuildTileFiles.Clear();
}

