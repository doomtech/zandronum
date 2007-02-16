/*
** texture.cpp
** The base texture class
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
#include "w_wad.h"
#include "r_data.h"
#include "templates.h"

typedef bool (*CheckFunc)(FileReader & file);
typedef FTexture * (*CreateFunc)(FileReader & file, int lumpnum);

struct TexCreateInfo
{
	CheckFunc Check;
	CreateFunc Create;
	int usetype;
};

BYTE FTexture::GrayMap[256];

void FTexture::InitGrayMap()
{
	for (int i = 0; i < 256; ++i)
	{
		GrayMap[i] = ColorMatcher.Pick (i, i, i);
	}
}

// Examines the lump contents to decide what type of texture to create,
// and creates the texture.
FTexture * FTexture::CreateTexture (int lumpnum, int usetype)
{
	static TexCreateInfo CreateInfo[]={
		{ FIMGZTexture::Check,		FIMGZTexture::Create,		TEX_Any },
		{ FPNGTexture::Check,		FPNGTexture::Create,		TEX_Any },
		{ FJPEGTexture::Check,		FJPEGTexture::Create,		TEX_Any },
		{ FDDSTexture::Check,		FDDSTexture::Create,		TEX_Any },
		{ FPCXTexture::Check,		FPCXTexture::Create,		TEX_Any },
		{ FTGATexture::Check,		FTGATexture::Create,		TEX_Any },
		{ FRawPageTexture::Check,	FRawPageTexture::Create,	TEX_MiscPatch },
		{ FFlatTexture::Check,		FFlatTexture::Create,		TEX_Flat },
		{ FPatchTexture::Check,		FPatchTexture::Create,		TEX_Any },
		{ FAutomapTexture::Check,	FAutomapTexture::Create,	TEX_Autopage },
	};


	FWadLump data = Wads.OpenLumpNum (lumpnum);

	for(size_t i = 0; i < countof(CreateInfo); i++)
	{
		if ((CreateInfo[i].usetype == usetype || CreateInfo[i].usetype == TEX_Any) &&
			CreateInfo[i].Check(data))
		{
			FTexture * tex = CreateInfo[i].Create(data, lumpnum);
			if (tex != NULL) 
			{
				tex->UseType = usetype;
				if (usetype == FTexture::TEX_Flat) 
				{
					int w = tex->GetWidth();
					int h = tex->GetHeight();

					// Auto-scale flats with dimensions 128x128 and 256x256
					if (w==128 && h==128) 
					{
						tex->ScaleX = tex->ScaleY = 16;
						tex->bWorldPanning = true;
					}
					else if (w==256 && h==256) 
					{
						tex->ScaleX = tex->ScaleY = 32;
						tex->bWorldPanning = true;
					}
				}
				return tex;
			}
		}
	}
	return NULL;
}

FTexture::FTexture ()
: LeftOffset(0), TopOffset(0),
  WidthBits(0), HeightBits(0), ScaleX(8), ScaleY(8),
  UseType(TEX_Any), bNoDecals(false), bNoRemap0(false), bWorldPanning(false),
  bMasked(true), bAlphaTexture(false), bHasCanvas(false), bWarped(0), bIsPatch(false),
  Rotations(0xFFFF), Width(0), Height(0), WidthMask(0)
{
	*Name=0;
}

FTexture::~FTexture ()
{
}

bool FTexture::CheckModified ()
{
	return false;
}

void FTexture::SetFrontSkyLayer ()
{
	bNoRemap0 = true;
}

void FTexture::CalcBitSize ()
{
	// WidthBits is rounded down, and HeightBits is rounded up
	int i;

	for (i = 0; (1 << i) < Width; ++i)
	{ }

	WidthBits = i;

	// Having WidthBits that would allow for columns past the end of the
	// texture is not allowed, even if it means the entire texture is
	// not drawn.
	if (Width < (1 << WidthBits))
	{
		WidthBits--;
	}
	WidthMask = (1 << WidthBits) - 1;

	// The minimum height is 2, because we cannot shift right 32 bits.
	for (i = 1; (1 << i) < Height; ++i)
	{ }

	HeightBits = i;
}

FTexture::Span **FTexture::CreateSpans (const BYTE *pixels) const
{
	Span **spans, *span;

	if (!bMasked)
	{ // Texture does not have holes, so it can use a simpler span structure
		spans = (Span **)M_Malloc (sizeof(Span*)*Width + sizeof(Span)*2);
		span = (Span *)&spans[Width];
		for (int x = 0; x < Width; ++x)
		{
			spans[x] = span;
		}
		span[0].Length = Height;
		span[0].TopOffset = 0;
		span[1].Length = 0;
		span[1].TopOffset = 0;
	}
	else
	{ // Texture might have holes, so build a complete span structure
		int numcols = Width;
		int numrows = Height;
		int numspans = numcols;	// One span to terminate each column
		const BYTE *data_p;
		bool newspan;
		int x, y;

		data_p = pixels;

		// Count the number of spans in this texture
		for (x = numcols; x > 0; --x)
		{
			newspan = true;
			for (y = numrows; y > 0; --y)
			{
				if (*data_p++ == 0)
				{
					if (!newspan)
					{
						newspan = true;
					}
				}
				else if (newspan)
				{
					newspan = false;
					numspans++;
				}
			}
		}

		// Allocate space for the spans
		spans = (Span **)M_Malloc (sizeof(Span*)*numcols + sizeof(Span)*numspans);

		// Fill in the spans
		for (x = 0, span = (Span *)&spans[numcols], data_p = pixels; x < numcols; ++x)
		{
			newspan = true;
			spans[x] = span;
			for (y = 0; y < numrows; ++y)
			{
				if (*data_p++ == 0)
				{
					if (!newspan)
					{
						newspan = true;
						span++;
					}
				}
				else
				{
					if (newspan)
					{
						newspan = false;
						span->TopOffset = y;
						span->Length = 1;
					}
					else
					{
						span->Length++;
					}
				}
			}
			if (!newspan)
			{
				span++;
			}
			span->TopOffset = 0;
			span->Length = 0;
			span++;
		}
	}
	return spans;
}

void FTexture::FreeSpans (Span **spans) const
{
	free (spans);
}

void FTexture::CopyToBlock (BYTE *dest, int dwidth, int dheight, int xpos, int ypos, const BYTE *translation)
{
	int x1 = xpos, x2 = x1 + GetWidth(), xo = -x1;

	if (x1 < 0)
	{
		x1 = 0;
	}
	if (x2 > dwidth)
	{
		x2 = dwidth;
	}
	for (; x1 < x2; ++x1)
	{
		const BYTE *data;
		const Span *span;
		BYTE *outtop = &dest[dheight * x1];

		data = GetColumn (x1 + xo, &span);

		while (span->Length != 0)
		{
			int len = span->Length;
			int y = ypos + span->TopOffset;
			int adv = span->TopOffset;

			if (y < 0)
			{
				adv -= y;
				len += y;
				y = 0;
			}
			if (y + len > dheight)
			{
				len = dheight - y;
			}
			if (len > 0)
			{
				if (translation == NULL)
				{
					memcpy (outtop + y, data + adv, len);
				}
				else
				{
					for (int j = 0; j < len; ++j)
					{
						outtop[y+j] = translation[data[adv+j]];
					}
				}
			}
			span++;
		}
	}
}

// Converts a texture between row-major and column-major format
// by flipping it about the X=Y axis.

void FTexture::FlipSquareBlock (BYTE *block, int x, int y)
{
	int i, j;

	if (x != y) return;

	for (i = 0; i < x; ++i)
	{
		BYTE *corner = block + x*i + i;
		int count = x - i;
		if (count & 1)
		{
			count--;
			swap<BYTE> (corner[count], corner[count*x]);
		}
		for (j = 0; j < count; j += 2)
		{
			swap<BYTE> (corner[j], corner[j*x]);
			swap<BYTE> (corner[j+1], corner[(j+1)*x]);
		}
	}
}

void FTexture::FlipSquareBlockRemap (BYTE *block, int x, int y, const BYTE *remap)
{
	int i, j;
	BYTE t;

	if (x != y) return;

	for (i = 0; i < x; ++i)
	{
		BYTE *corner = block + x*i + i;
		int count = x - i;
		if (count & 1)
		{
			count--;
			t = remap[corner[count]];
			corner[count] = remap[corner[count*x]];
			corner[count*x] = t;
		}
		for (j = 0; j < count; j += 2)
		{
			t = remap[corner[j]];
			corner[j] = remap[corner[j*x]];
			corner[j*x] = t;
			t = remap[corner[j+1]];
			corner[j+1] = remap[corner[(j+1)*x]];
			corner[(j+1)*x] = t;
		}
	}
}

void FTexture::FlipNonSquareBlock (BYTE *dst, const BYTE *src, int x, int y, int srcpitch)
{
	int i, j;

	for (i = 0; i < x; ++i)
	{
		for (j = 0; j < y; ++j)
		{
			dst[i*y+j] = src[i+j*srcpitch];
		}
	}
}

void FTexture::FlipNonSquareBlockRemap (BYTE *dst, const BYTE *src, int x, int y, const BYTE *remap)
{
	int i, j;

	for (i = 0; i < x; ++i)
	{
		for (j = 0; j < y; ++j)
		{
			dst[i*y+j] = remap[src[i+j*x]];
		}
	}
}

FDummyTexture::FDummyTexture ()
{
	Width = 64;
	Height = 64;
	HeightBits = 6;
	WidthBits = 6;
	WidthMask = 63;
	Name[0] = 0;
	UseType = TEX_Null;
}

void FDummyTexture::Unload ()
{
}

void FDummyTexture::SetSize (int width, int height)
{
	Width = width;
	Height = height;
	CalcBitSize ();
}

// This must never be called
const BYTE *FDummyTexture::GetColumn (unsigned int column, const Span **spans_out)
{
	return NULL;
}

// And this also must never be called
const BYTE *FDummyTexture::GetPixels ()
{
	return NULL;
}

