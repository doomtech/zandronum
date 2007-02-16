/*
** pngtexture.cpp
** Texture class for PNG images
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
#include "m_png.h"


bool FPNGTexture::Check(FileReader & file)
{
	DWORD id;
	file.Seek(0, SEEK_SET);
	file.Read(&id, 4);
	return (id == MAKE_ID(137,'P','N','G'));
}

FTexture *FPNGTexture::Create(FileReader & data, int lumpnum)
{
	union
	{
		DWORD dw;
		WORD w[2];
		BYTE b[4];
	} first4bytes;

	DWORD width, height;
	BYTE bitdepth, colortype, compression, filter, interlace;

	// This is most likely a PNG, but make sure. (Note that if the
	// first 4 bytes match, but later bytes don't, we assume it's
	// a corrupt PNG.)
	data.Seek(4, SEEK_SET);
	data.Read (first4bytes.b, 4);
	if (first4bytes.dw != MAKE_ID(13,10,26,10))		return NULL;
	data.Read (first4bytes.b, 4);
	if (first4bytes.dw != MAKE_ID(0,0,0,13))		return NULL;
	data.Read (first4bytes.b, 4);
	if (first4bytes.dw != MAKE_ID('I','H','D','R'))	return NULL;

	// The PNG looks valid so far. Check the IHDR to make sure it's a
	// type of PNG we support.
	data >> width >> height
		 >> bitdepth >> colortype >> compression >> filter >> interlace;

	if (compression != 0 || filter != 0 || interlace > 1)
	{
		return NULL;
	}
	if (!((1 << colortype) & 0x5D))
	{
		return NULL;
	}
	if (!((1 << bitdepth) & 0x116))
	{
		return NULL;
	}

	// Just for completeness, make sure the PNG has something more than an
	// IHDR.
	data.Seek (4, SEEK_CUR);
	data.Read (first4bytes.b, 4);
	if (first4bytes.dw == 0)
	{
		data.Read (first4bytes.b, 4);
		if (first4bytes.dw == MAKE_ID('I','E','N','D'))
		{
			return NULL;
		}
	}

	return new FPNGTexture (data, lumpnum, BigLong((int)width), BigLong((int)height),
		bitdepth, colortype, interlace);
}

FPNGTexture::FPNGTexture (FileReader &lump, int lumpnum, int width, int height,
						  BYTE depth, BYTE colortype, BYTE interlace)
: SourceLump(lumpnum), Pixels(0), Spans(0),
  BitDepth(depth), ColorType(colortype), Interlace(interlace),
  PaletteMap(0), PaletteSize(0), StartOfIDAT(0)
{
	union
	{
		DWORD palette[256];
		BYTE pngpal[256][3];
	};
	BYTE trans[256];
	bool havetRNS = false;
	DWORD len, id;
	int i;

	Wads.GetLumpName (Name, lumpnum);
	Name[8] = 0;

	UseType = TEX_MiscPatch;
	LeftOffset = 0;
	TopOffset = 0;
	bMasked = false;

	Width = width;
	Height = height;
	CalcBitSize ();

	memset (trans, 255, 256);

	// Parse pre-IDAT chunks. I skip the CRCs. Is that bad?
	lump.Seek (33, SEEK_SET);

	lump >> len >> id;
	while (id != MAKE_ID('I','D','A','T') && id != MAKE_ID('I','E','N','D'))
	{
		len = BigLong((unsigned int)len);
		switch (id)
		{
		default:
			lump.Seek (len, SEEK_CUR);
			break;

		case MAKE_ID('g','r','A','b'):
			// This is like GRAB found in an ILBM, except coordinates use 4 bytes
			{
				DWORD hotx, hoty;

				lump >> hotx >> hoty;
				LeftOffset = BigLong((int)hotx);
				TopOffset = BigLong((int)hoty);
			}
			break;

		case MAKE_ID('P','L','T','E'):
			PaletteSize = MIN<int> (len / 3, 256);
			lump.Read (pngpal, PaletteSize * 3);
			if (PaletteSize * 3 != (int)len)
			{
				lump.Seek (len - PaletteSize * 3, SEEK_CUR);
			}
			for (i = PaletteSize - 1; i >= 0; --i)
			{
				palette[i] = MAKERGB(pngpal[i][0], pngpal[i][1], pngpal[i][2]);
			}
			break;

		case MAKE_ID('t','R','N','S'):
			lump.Read (trans, len);
			havetRNS = true;
			break;

		case MAKE_ID('a','l','P','h'):
			bAlphaTexture = true;
			bMasked = true;
			break;
		}
		lump >> len >> len;	// Skip CRC
		id = MAKE_ID('I','E','N','D');
		lump >> id;
	}
	StartOfIDAT = lump.Tell() - 8;

	switch (colortype)
	{
	case 4:		// Grayscale + Alpha
		bMasked = true;
		// intentional fall-through

	case 0:		// Grayscale
		if (!bAlphaTexture)
		{
			if (colortype == 0 && havetRNS && trans[0] != 0)
			{
				bMasked = true;
				PaletteSize = 256;
				PaletteMap = new BYTE[256];
				memcpy (PaletteMap, GrayMap, 256);
				PaletteMap[trans[0]] = 0;
			}
			else
			{
				PaletteMap = GrayMap;
			}
		}
		break;

	case 3:		// Paletted
		PaletteMap = new BYTE[PaletteSize];
		GPalette.MakeRemap (palette, PaletteMap, trans, PaletteSize);
		for (i = 0; i < PaletteSize; ++i)
		{
			if (trans[i] == 0)
			{
				bMasked = true;
				PaletteMap[i] = 0;
			}
		}
		break;

	case 6:		// RGB + Alpha
		bMasked = true;
		break;
	}
}

FPNGTexture::~FPNGTexture ()
{
	Unload ();
	if (Spans != NULL)
	{
		FreeSpans (Spans);
		Spans = NULL;
	}
	if (PaletteMap != NULL && PaletteMap != GrayMap)
	{
		delete[] PaletteMap;
		PaletteMap = NULL;
	}
}

void FPNGTexture::Unload ()
{
	if (Pixels != NULL)
	{
		delete[] Pixels;
		Pixels = NULL;
	}
}

const BYTE *FPNGTexture::GetColumn (unsigned int column, const Span **spans_out)
{
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

const BYTE *FPNGTexture::GetPixels ()
{
	if (Pixels == NULL)
	{
		MakeTexture ();
	}
	return Pixels;
}

void FPNGTexture::MakeTexture ()
{
	FWadLump lump = Wads.OpenLumpNum (SourceLump);

	Pixels = new BYTE[Width*Height];
	if (StartOfIDAT == 0)
	{
		memset (Pixels, 0x99, Width*Height);
	}
	else
	{
		DWORD len, id;
		lump.Seek (StartOfIDAT, SEEK_SET);
		lump >> len >> id;

		if (ColorType == 0 || ColorType == 3)	/* Grayscale and paletted */
		{
			M_ReadIDAT (&lump, Pixels, Width, Height, Width, BitDepth, ColorType, Interlace, BigLong((unsigned int)len));

			if (Width == Height)
			{
				if (PaletteMap != NULL)
				{
					FlipSquareBlockRemap (Pixels, Width, Height, PaletteMap);
				}
				else
				{
					FlipSquareBlock (Pixels, Width, Height);
				}
			}
			else
			{
				BYTE *newpix = new BYTE[Width*Height];
				if (PaletteMap != NULL)
				{
					FlipNonSquareBlockRemap (newpix, Pixels, Width, Height, PaletteMap);
				}
				else
				{
					FlipNonSquareBlock (newpix, Pixels, Width, Height, Width);
				}
				BYTE *oldpix = Pixels;
				Pixels = newpix;
				delete[] oldpix;
			}
		}
		else		/* RGB and/or Alpha present */
		{
			int bytesPerPixel = ColorType == 2 ? 3 : ColorType == 4 ? 2 : 4;
			BYTE *tempix = new BYTE[Width * Height * bytesPerPixel];
			BYTE *in, *out;
			int x, y, pitch, backstep;

			M_ReadIDAT (&lump, tempix, Width, Height, Width*bytesPerPixel, BitDepth, ColorType, Interlace, BigLong((unsigned int)len));
			in = tempix;
			out = Pixels;

			// Convert from source format to paletted, column-major.
			// Formats with alpha maps are reduced to only 1 bit of alpha.
			switch (ColorType)
			{
			case 2:		// RGB
				pitch = Width * 3;
				backstep = Height * pitch - 3;
				for (x = Width; x > 0; --x)
				{
					for (y = Height; y > 0; --y)
					{
						*out++ = RGB32k[in[0]>>3][in[1]>>3][in[2]>>3];
						in += pitch;
					}
					in -= backstep;
				}
				break;

			case 4:		// Grayscale + Alpha
				pitch = Width * 2;
				backstep = Height * pitch - 2;
				if (PaletteMap != NULL)
				{
					for (x = Width; x > 0; --x)
					{
						for (y = Height; y > 0; --y)
						{
							*out++ = in[1] < 128 ? 0 : PaletteMap[in[0]];
							in += pitch;
						}
						in -= backstep;
					}
				}
				else
				{
					for (x = Width; x > 0; --x)
					{
						for (y = Height; y > 0; --y)
						{
							*out++ = in[1] < 128 ? 0 : in[0];
							in += pitch;
						}
						in -= backstep;
					}
				}
				break;

			case 6:		// RGB + Alpha
				pitch = Width * 4;
				backstep = Height * pitch - 4;
				for (x = Width; x > 0; --x)
				{
					for (y = Height; y > 0; --y)
					{
						*out++ = in[3] < 128 ? 0 : RGB32k[in[0]>>3][in[1]>>3][in[2]>>3];
						in += pitch;
					}
					in -= backstep;
				}
				break;
			}
			delete[] tempix;
		}
	}
	if (Spans == NULL)
	{
		Spans = CreateSpans (Pixels);
	}
}

