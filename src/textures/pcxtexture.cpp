/*
** pcxtexture.cpp
** Texture class for PCX images
**
**---------------------------------------------------------------------------
** Copyright 2005 David HENRY
** Copyright 2006 Christoph Oelckers
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


bool FPCXTexture::Check(FileReader & file)
{
	PCXHeader hdr;

	file.Seek(0, SEEK_SET);
	file.Read(&hdr, sizeof(hdr));

#ifdef WORDS_BIGENDIAN
	hdr.xmin = LittleShort(hdr.xmin);
	hdr.xmax = LittleShort(hdr.xmax);
	hdr.bytesPerScanLine = LittleShort(hdr.bytesPerScanLine);
#endif

	if (hdr.manufacturer != 10 || hdr.encoding != 1) return false;
	if (hdr.version != 0 && hdr.version != 2 && hdr.version != 3 && hdr.version != 4 && hdr.version != 5) return false;
	if (hdr.bitsPerPixel != 1 && hdr.bitsPerPixel != 8) return false; 
	if (hdr.bitsPerPixel == 1 && hdr.numColorPlanes !=1 && hdr.numColorPlanes != 4) return false;
	if (hdr.bitsPerPixel == 8 && hdr.bytesPerScanLine != ((hdr.xmax - hdr.xmin + 2)&~1)) return false;

	for (int i = 0; i < 54; i++) 
	{
		if (hdr.padding[i] != 0) return false;
	}
	return true;
}

FTexture * FPCXTexture::Create(FileReader & file, int lumpnum)
{
	PCXHeader hdr;

	file.Seek(0, SEEK_SET);
	file.Read(&hdr, sizeof(hdr));

	return new FPCXTexture(lumpnum, hdr);
}

FPCXTexture::FPCXTexture(int lumpnum, PCXHeader & hdr)
: SourceLump(lumpnum), Pixels(0)
{
	Wads.GetLumpName (Name, lumpnum);
	Name[8] = 0;
	bMasked = false;
	Width = LittleShort(hdr.xmax) - LittleShort(hdr.xmin) + 1;
	Height = LittleShort(hdr.ymax) - LittleShort(hdr.ymin) + 1;
	CalcBitSize();

	DummySpans[0].TopOffset = 0;
	DummySpans[0].Length = Height;
	DummySpans[1].TopOffset = 0;
	DummySpans[1].Length = 0;
}

FPCXTexture::~FPCXTexture ()
{
	Unload ();
}


void FPCXTexture::Unload ()
{
	if (Pixels != NULL)
	{
		delete[] Pixels;
		Pixels = NULL;
	}
}

const BYTE *FPCXTexture::GetColumn (unsigned int column, const Span **spans_out)
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
		*spans_out = DummySpans;
	}
	return Pixels + column*Height;
}

const BYTE *FPCXTexture::GetPixels ()
{
	if (Pixels == NULL)
	{
		MakeTexture ();
	}
	return Pixels;
}

void FPCXTexture::ReadPCX1bit (BYTE *dst, FileReader & lump, PCXHeader *hdr)
{
	int y, i, bytes;
	int rle_count = 0;
	BYTE rle_value = 0;

	BYTE * srcp = new BYTE[lump.GetLength() - sizeof(PCXHeader)];
	lump.Read(srcp, lump.GetLength() - sizeof(PCXHeader));
	BYTE * src = srcp;

	for (y = 0; y < Height; ++y)
	{
		BYTE * ptr = &dst[y * Width];

		bytes = hdr->bytesPerScanLine;

		while (bytes--)
		{
			if (rle_count == 0)
			{
				if ( (rle_value = *src++) < 0xc0)
				{
					rle_count = 1;
				}
				else
				{
					rle_count = rle_value - 0xc0;
					rle_value = *src++;
				}
			}

			rle_count--;

			for (i = 7; i >= 0; --i, ptr ++)
			{
				*ptr = ((rle_value & (1 << i)) > 0);
			}
		}
	}
	delete [] srcp;
}


void FPCXTexture::ReadPCX4bits (BYTE *dst, FileReader & lump, PCXHeader *hdr)
{
	int rle_count = 0, rle_value = 0;
	int x, y, c;
	int bytes;
	BYTE * line = new BYTE[hdr->bytesPerScanLine];
	BYTE * colorIndex = new BYTE[Width];

	BYTE * srcp = new BYTE[lump.GetLength() - sizeof(PCXHeader)];
	lump.Read(srcp, lump.GetLength() - sizeof(PCXHeader));
	BYTE * src = srcp;

	for (y = 0; y < Height; ++y)
	{
		BYTE * ptr = &dst[y * Width];
		memset (ptr, 0, Width * sizeof (BYTE));

		for (c = 0; c < 4; ++c)
		{
			BYTE * pLine = line;

			bytes = hdr->bytesPerScanLine;

			while (bytes--)
			{
				if (rle_count == 0)
				{
					if ( (rle_value = *src++) < 0xc0)
					{
						rle_count = 1;
					}
					else
					{
						rle_count = rle_value - 0xc0;
						rle_value = *src++;
					}
				}

				rle_count--;
				*(pLine++) = rle_value;
			}

			/* compute line's color indexes */
			for (x = 0; x < Width; ++x)
			{
				if (line[x / 8] & (128 >> (x % 8)))
					ptr[x] += (1 << c);
			}
		}
	}

	/* release memory */
	delete [] colorIndex;
	delete [] line;
	delete [] srcp;
}


void FPCXTexture::ReadPCX8bits (BYTE *dst, FileReader & lump, PCXHeader *hdr)
{
	int rle_count = 0, rle_value = 0;
	int y, bytes;

	BYTE * srcp = new BYTE[lump.GetLength() - sizeof(PCXHeader)];
	lump.Read(srcp, lump.GetLength() - sizeof(PCXHeader));
	BYTE * src = srcp;

	for (y = 0; y < Height; ++y)
	{
		BYTE * ptr = &dst[y * Width];

		bytes = hdr->bytesPerScanLine;
		while (bytes--)
		{
			if (rle_count == 0)
			{
				if( (rle_value = *src++) < 0xc0)
				{
					rle_count = 1;
				}
				else
				{
					rle_count = rle_value - 0xc0;
					rle_value = *src++;
				}
			}

			rle_count--;
			*ptr++ = rle_value;
		}
	}
	delete [] srcp;
}


void FPCXTexture::ReadPCX24bits (BYTE *dst, FileReader & lump, PCXHeader *hdr, int planes)
{
	int rle_count = 0, rle_value = 0;
	int y, c;
	int bytes;

	BYTE * srcp = new BYTE[lump.GetLength() - sizeof(PCXHeader)];
	lump.Read(srcp, lump.GetLength() - sizeof(PCXHeader));
	BYTE * src = srcp;

	for (y = 0; y < Height; ++y)
	{
		/* for each color plane */
		for (c = 0; c < planes; ++c)
		{
			BYTE * ptr = &dst[y * Width * planes];
			bytes = hdr->bytesPerScanLine;

			while (bytes--)
			{
				if (rle_count == 0)
				{
					if( (rle_value = *src++) < 0xc0)
					{
						rle_count = 1;
					}
					else
					{
						rle_count = rle_value - 0xc0;
						rle_value = *src++;
					}
				}

				rle_count--;
				ptr[c] = (BYTE)rle_value;
				ptr += planes;
			}
		}
	}
	delete [] srcp;
}

void FPCXTexture::MakeTexture()
{
	BYTE PaletteMap[256];
	PCXHeader header;
	int bitcount;

	FWadLump lump = Wads.OpenLumpNum(SourceLump);

	lump.Read(&header, sizeof(header));

	bitcount = header.bitsPerPixel * header.numColorPlanes;
	Pixels = new BYTE[Width*Height];

	if (bitcount < 24)
	{
		if (bitcount < 8)
		{
			for (int i=0;i<16;i++)
			{
				PaletteMap[i] = ColorMatcher.Pick(header.palette[i*3],header.palette[i*3+1],header.palette[i*3+2]);
			}

			switch (bitcount)
			{
			default:
			case 1:
				ReadPCX1bit (Pixels, lump, &header);
				break;

			case 4:
				ReadPCX4bits (Pixels, lump, &header);
				break;
			}
		}
		else if (bitcount == 8)
		{
			BYTE c;
			lump.Seek(-769, SEEK_END);
			lump >> c;
			if (c !=0x0c) memcpy(PaletteMap, GrayMap, 256);	// Fallback for files without palette
			else for(int i=0;i<256;i++)
			{
				BYTE r,g,b;
				lump >> r >> g >> b;
				PaletteMap[i] = ColorMatcher.Pick(r,g,b);
			}
			lump.Seek(sizeof(header), SEEK_SET);
			ReadPCX8bits (Pixels, lump, &header);
		}
		if (Width == Height)
		{
			FlipSquareBlockRemap(Pixels, Width, Height, PaletteMap);
		}
		else
		{
			BYTE *newpix = new BYTE[Width*Height];
			FlipNonSquareBlockRemap (newpix, Pixels, Width, Height, PaletteMap);
			BYTE *oldpix = Pixels;
			Pixels = newpix;
			delete[] oldpix;
		}
	}
	else
	{
		BYTE * buffer = new BYTE[Width*Height * 3];
		BYTE * row = buffer;
		ReadPCX24bits (buffer, lump, &header, 3);
		for(int y=0; y<Height; y++)
		{
			for(int x=0; x < Width; x++)
			{
				Pixels[y+Height*x] = RGB32k[row[0]>>3][row[1]>>3][row[2]>>3];
				row+=3;
			}
		}
		delete [] buffer;
	}
}

