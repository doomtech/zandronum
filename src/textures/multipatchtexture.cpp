/*
** multipatchtexture.cpp
** Texture class for standard Doom multipatch textures
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
#include "r_data.h"
#include "w_wad.h"
#include "i_system.h"
#include "gi.h"

// On the Alpha, accessing the shorts directly if they aren't aligned on a
// 4-byte boundary causes unaligned access warnings. Why it does this at
// all and only while initing the textures is beyond me.

#ifdef ALPHA
#define SAFESHORT(s)	((short)(((BYTE *)&(s))[0] + ((BYTE *)&(s))[1] * 256))
#else
#define SAFESHORT(s)	LittleShort(s)
#endif



FMultiPatchTexture::FMultiPatchTexture (const void *texdef, FPatchLookup *patchlookup, int maxpatchnum, bool strife)
: Pixels (0), Spans(0), Parts(0), bRedirect(false)
{
	union
	{
		const maptexture_t			*d;
		const strifemaptexture_t	*s;
	}
	mtexture;

	union
	{
		const mappatch_t			*d;
		const strifemappatch_t		*s;
	}
	mpatch;

	int i;

	mtexture.d = (const maptexture_t *)texdef;

	if (strife)
	{
		NumParts = SAFESHORT(mtexture.s->patchcount);
	}
	else
	{
		NumParts = SAFESHORT(mtexture.d->patchcount);
	}

	if (NumParts <= 0)
	{
		I_FatalError ("Bad texture directory");
	}

	UseType = FTexture::TEX_Wall;
	Parts = new TexPart[NumParts];
	Width = SAFESHORT(mtexture.d->width);
	Height = SAFESHORT(mtexture.d->height);
	strncpy (Name, (const char *)mtexture.d->name, 8);
	Name[8] = 0;

	CalcBitSize ();

	// [RH] Special for beta 29: Values of 0 will use the tx/ty cvars
	// to determine scaling instead of defaulting to 8. I will likely
	// remove this once I finish the betas, because by then, users
	// should be able to actually create scaled textures.
	// 10-June-2003: It's still here long after beta 29. Heh.

	ScaleX = mtexture.d->ScaleX ? mtexture.d->ScaleX : 0;
	ScaleY = mtexture.d->ScaleY ? mtexture.d->ScaleY : 0;

	if (mtexture.d->Flags & MAPTEXF_WORLDPANNING)
	{
		bWorldPanning = true;
	}

	if (strife)
	{
		mpatch.s = &mtexture.s->patches[0];
	}
	else
	{
		mpatch.d = &mtexture.d->patches[0];
	}

	for (i = 0; i < NumParts; ++i)
	{
		if (unsigned(LittleShort(mpatch.d->patch)) >= unsigned(maxpatchnum))
		{
			I_FatalError ("Bad PNAMES and/or texture directory:\n\nPNAMES has %d entries, but\n%s wants to use entry %d.",
				maxpatchnum, Name, LittleShort(mpatch.d->patch)+1);
		}
		Parts[i].OriginX = LittleShort(mpatch.d->originx);
		Parts[i].OriginY = LittleShort(mpatch.d->originy);
		Parts[i].Texture = patchlookup[LittleShort(mpatch.d->patch)].Texture;
		if (Parts[i].Texture == NULL)
		{
			Printf ("Unknown patch %s in texture %s\n", patchlookup[LittleShort(mpatch.d->patch)].Name, Name);
			NumParts--;
			i--;
		}
		if (strife)
			mpatch.s++;
		else
			mpatch.d++;
	}
	if (NumParts == 0)
	{
		Printf ("Texture %s is left without any patches\n", Name);
	}

	CheckForHacks ();

	// If this texture is just a wrapper around a single patch, we can simply
	// forward GetPixels() and GetColumn() calls to that patch.
	if (NumParts == 1)
	{
		if (Parts->OriginX == 0 && Parts->OriginY == 0 &&
			Parts->Texture->GetWidth() == Width &&
			Parts->Texture->GetHeight() == Height)
		{
			bRedirect = true;
		}
	}
}

FMultiPatchTexture::~FMultiPatchTexture ()
{
	Unload ();
	if (Parts != NULL)
	{
		delete[] Parts;
		Parts = NULL;
	}
	if (Spans != NULL)
	{
		FreeSpans (Spans);
		Spans = NULL;
	}
}

void FMultiPatchTexture::SetFrontSkyLayer ()
{
	for (int i = 0; i < NumParts; ++i)
	{
		Parts[i].Texture->SetFrontSkyLayer ();
	}
	bNoRemap0 = true;
}

void FMultiPatchTexture::Unload ()
{
	if (Pixels != NULL)
	{
		delete[] Pixels;
		Pixels = NULL;
	}
}

const BYTE *FMultiPatchTexture::GetPixels ()
{
	if (bRedirect)
	{
		return Parts->Texture->GetPixels ();
	}
	if (Pixels == NULL)
	{
		MakeTexture ();
	}
	return Pixels;
}

const BYTE *FMultiPatchTexture::GetColumn (unsigned int column, const Span **spans_out)
{
	if (bRedirect)
	{
		return Parts->Texture->GetColumn (column, spans_out);
	}
	if (Pixels == NULL)
	{
		MakeTexture ();
	}
	if ((unsigned)column >= (unsigned)Width)
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
		*spans_out = Spans[column];
	}
	return Pixels + column*Height;
}

void FMultiPatchTexture::MakeTexture ()
{
	// Add a little extra space at the end if the texture's height is not
	// a power of 2, in case somebody accidentally makes it repeat vertically.
	int numpix = Width * Height + (1 << HeightBits) - Height;

	Pixels = new BYTE[numpix];
	memset (Pixels, 0, numpix);

	for (int i = 0; i < NumParts; ++i)
	{
		Parts[i].Texture->CopyToBlock (Pixels, Width, Height,
			Parts[i].OriginX, Parts[i].OriginY);
	}

	if (Spans == NULL)
	{
		Spans = CreateSpans (Pixels);
	}
}

void FMultiPatchTexture::CheckForHacks ()
{
	if (NumParts <= 0)
	{
		return;
	}

	// Heretic sky textures are marked as only 128 pixels tall,
	// even though they are really 200 pixels tall.
	if (gameinfo.gametype == GAME_Heretic &&
		Name[0] == 'S' &&
		Name[1] == 'K' &&
		Name[2] == 'Y' &&
		Name[4] == 0 &&
		Name[3] >= '1' &&
		Name[3] <= '3' &&
		Height == 128)
	{
		Height = 200;
		HeightBits = 8;
		return;
	}

	// The Doom E1 sky has its patch's y offset at -8 instead of 0.
	if (gameinfo.gametype == GAME_Doom &&
		!(gameinfo.flags & GI_MAPxx) &&
		NumParts == 1 &&
		Height == 128 &&
		Parts->OriginY == -8 &&
		Name[0] == 'S' &&
		Name[1] == 'K' &&
		Name[2] == 'Y' &&
		Name[3] == '1' &&
		Name[4] == 0)
	{
		Parts->OriginY = 0;
		return;
	}

	// BIGDOOR7 in Doom also has patches at y offset -4 instead of 0.
	if (gameinfo.gametype == GAME_Doom &&
		!(gameinfo.flags & GI_MAPxx) &&
		NumParts == 2 &&
		Height == 128 &&
		Parts[0].OriginY == -4 &&
		Parts[1].OriginY == -4 &&
		Name[0] == 'B' &&
		Name[1] == 'I' &&
		Name[2] == 'G' &&
		Name[3] == 'D' &&
		Name[4] == 'O' &&
		Name[5] == 'O' &&
		Name[6] == 'R' &&
		Name[7] == '7')
	{
		Parts[0].OriginY = 0;
		Parts[1].OriginY = 0;
		return;
	}

	// [RH] Some wads (I forget which!) have single-patch textures 256
	// pixels tall that have patch lengths recorded as 0. I can't think of
	// any good reason for them to do this, and since I didn't make note
	// of which wad made me hack in support for them, the hack is gone
	// because I've added support for DeePsea's true tall patches.
	//
	// Okay, I found a wad with crap patches: Pleiades.wad's sky patches almost
	// fit this description and are a big mess, but they're not single patch!
	if (Height == 256)
	{
		int i;

		// All patches must be at the top of the texture for this fix
		for (i = 0; i < NumParts; ++i)
		{
			if (Parts[i].OriginX != 0)
			{
				break;
			}
		}

		if (i == NumParts)
		{
			// This really must check whether the texture in question is
			// actually an FPatchTexture before casting the pointer.
			for (i = 0; i < NumParts; ++i) if (Parts[i].Texture->bIsPatch)
			{
				FPatchTexture *tex = (FPatchTexture *)Parts[i].Texture;
				// Check if this patch is likely to be a problem.
				// It must be 256 pixels tall, and all its columns must have exactly
				// one post, where each post has a supposed length of 0.
				FMemLump lump = Wads.ReadLump (tex->SourceLump);
				const patch_t *realpatch = (patch_t *)lump.GetMem();
				const DWORD *cofs = realpatch->columnofs;
				int x, x2 = LittleShort(realpatch->width);

				if (LittleShort(realpatch->height) == 256)
				{
					for (x = 0; x < x2; ++x)
					{
						const column_t *col = (column_t*)((BYTE*)realpatch+LittleLong(cofs[x]));
						if (col->topdelta != 0 || col->length != 0)
						{
							break;	// It's not bad!
						}
						col = (column_t *)((BYTE *)col + 256 + 4);
						if (col->topdelta != 0xFF)
						{
							break;	// More than one post in a column!
						}
					}
					if (x == x2)
					{ // If all the columns were checked, it needs fixing.
						tex->HackHack (Height);
					}
				}
			}
		}
	}
}

void FTextureManager::AddTexturesLump (const void *lumpdata, int lumpsize, int patcheslump, int firstdup, bool texture1)
{
	FPatchLookup *patchlookup;
	int i, j;
	DWORD numpatches;

	if (firstdup == 0)
	{
		firstdup = (int)Textures.Size();
	}

	{
		FWadLump pnames = Wads.OpenLumpNum (patcheslump);

		pnames >> numpatches;

		// Check whether the amount of names reported is correct.
		if (numpatches < 0)
		{
			Printf("Corrupt PNAMES lump found (negative amount of entries reported)");
			return;
		}

		// Check whether the amount of names reported is correct.
		int lumplength = Wads.LumpLength(patcheslump);
		if (numpatches > DWORD((lumplength-4)/8))
		{
			Printf("PNAMES lump is shorter than required (%u entries reported but only %d bytes (%d entries) long\n",
				numpatches, lumplength, (lumplength-4)/8);
			// Truncate but continue reading. Who knows how many such lumps exist?
			numpatches = (lumplength-4)/8;
		}

		// Catalog the patches these textures use so we know which
		// textures they represent.
		patchlookup = (FPatchLookup *)alloca (numpatches * sizeof(*patchlookup));

		for (DWORD i = 0; i < numpatches; ++i)
		{
			pnames.Read (patchlookup[i].Name, 8);
			patchlookup[i].Name[8] = 0;

			j = CheckForTexture (patchlookup[i].Name, FTexture::TEX_WallPatch);
			if (j >= 0)
			{
				patchlookup[i].Texture = Textures[j].Texture;
			}
			else
			{
				// Shareware Doom has the same PNAMES lump as the registered
				// Doom, so printing warnings for patches that don't really
				// exist isn't such a good idea.
				//Printf ("Patch %s not found.\n", patchlookup[i].Name);
				patchlookup[i].Texture = NULL;
			}
		}
	}

	bool isStrife = false;
	const DWORD *maptex, *directory;
	DWORD maxoff;
	int numtextures;
	DWORD offset = 0;   // Shut up, GCC!

	maptex = (const DWORD *)lumpdata;
	numtextures = LittleLong(*maptex);
	maxoff = lumpsize;

	if (maxoff < DWORD(numtextures+1)*4)
	{
		Printf ("Texture directory is too short");
		return;
	}

	// Scan the texture lump to decide if it contains Doom or Strife textures
	for (i = 0, directory = maptex+1; i < numtextures; ++i)
	{
		offset = LittleLong(directory[i]);
		if (offset > maxoff)
		{
			Printf ("Bad texture directory");
			return;
		}

		maptexture_t *tex = (maptexture_t *)((BYTE *)maptex + offset);

		// There is bizzarely a Doom editing tool that writes to the
		// first two elements of columndirectory, so I can't check those.
		if (SAFESHORT(tex->patchcount) <= 0 ||
			tex->columndirectory[2] != 0 ||
			tex->columndirectory[3] != 0)
		{
			isStrife = true;
			break;
		}
	}


	// Textures defined earlier in the lump take precedence over those defined later,
	// but later TEXTUREx lumps take precedence over earlier ones.
	for (i = 1, directory = maptex; i <= numtextures; ++i)
	{
		if (i == 1 && texture1)
		{
			// The very first texture is just a dummy. Copy its dimensions to texture 0.
			// It still needs to be created in case someone uses it by name.
			offset = LittleLong(directory[1]);
			const maptexture_t *tex = (const maptexture_t *)((const BYTE *)maptex + offset);
			FDummyTexture *tex0 = static_cast<FDummyTexture *>(Textures[0].Texture);
			tex0->SetSize (SAFESHORT(tex->width), SAFESHORT(tex->height));
		}

		offset = LittleLong(directory[i]);
		if (offset > maxoff)
		{
			Printf ("Bad texture directory");
			return;
		}

		// If this texture was defined already in this lump, skip it
		// This could cause problems with animations that use the same name for intermediate
		// textures. Should I be worried?
		for (j = (int)Textures.Size() - 1; j >= firstdup; --j)
		{
			if (strnicmp (Textures[j].Texture->Name, (const char *)maptex + offset, 8) == 0)
				break;
		}
		if (j + 1 == firstdup)
		{
			FTexture *tex = new FMultiPatchTexture ((const BYTE *)maptex + offset, patchlookup, numpatches, isStrife);
			if (i == 1 && texture1)
			{
				tex->UseType = FTexture::TEX_Null;
			}
			TexMan.AddTexture (tex);
		}
	}
}


void FTextureManager::AddTexturesLumps (int lump1, int lump2, int patcheslump)
{
	int firstdup = (int)Textures.Size();

	if (lump1 >= 0)
	{
		FMemLump texdir = Wads.ReadLump (lump1);
		AddTexturesLump (texdir.GetMem(), Wads.LumpLength (lump1), patcheslump, firstdup, true);
	}
	if (lump2 >= 0)
	{
		FMemLump texdir = Wads.ReadLump (lump2);
		AddTexturesLump (texdir.GetMem(), Wads.LumpLength (lump2), patcheslump, firstdup, false);
	}
}

