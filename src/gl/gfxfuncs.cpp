#include "gl_pch.h"
/*
** gfxfuncs.cpp
** True color graphics manipulation
**
**---------------------------------------------------------------------------
** Copyright 2003-2005 Christoph Oelckers
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
#include "r_main.h"
#include "m_swap.h"
#include "m_png.h"
#include "m_crc32.h"

EXTERN_CVAR(Float, png_gamma)
EXTERN_CVAR (Float, Gamma)

struct IHDR
{
	DWORD		Width;
	DWORD		Height;
	BYTE		BitDepth;
	BYTE		ColorType;
	BYTE		Compression;
	BYTE		Filter;
	BYTE		Interlace;
};

static inline void MakeChunk (void *where, DWORD type, size_t len)
{
	BYTE *const data = (BYTE *)where;
	*(DWORD *)(data - 8) = BigLong ((unsigned int)len);
	*(DWORD *)(data - 4) = type;
	*(DWORD *)(data + len) = BigLong ((unsigned int)CalcCRC32 (data-4, (unsigned int)(len+4)));
}

//===========================================================================
// 
//	Takes a screenshot
//
//===========================================================================
void gl_ScreenShot (const char* fname)
{
	BYTE work[8 +				// signature
			  12+2*4+5 +		// IHDR
			  12+4];			// gAMA


	int w=SCREENWIDTH;
	int h=SCREENHEIGHT;
	int p=3*w;

	byte * scr2= (byte *)M_Malloc(w * h * 3);
	byte * scr = (byte *)M_Malloc(w * h * 3);
	gl.ReadPixels(0,0,w,h,GL_RGB,GL_UNSIGNED_BYTE,scr2);

	for(int i=0;i<h;i++)
	{
		memcpy(scr + p * (h-1-i), scr2 + p*i, p);
	}
	free(scr2);
	

	DWORD *const sig = (DWORD *)&work[0];
	IHDR *const ihdr = (IHDR *)&work[8 + 8];
	DWORD *const gama = (DWORD *)((BYTE *)ihdr + 2*4+5 + 12);

	sig[0] = MAKE_ID(137,'P','N','G');
	sig[1] = MAKE_ID(13,10,26,10);

	ihdr->Width = BigLong (w);
	ihdr->Height = BigLong (h);
	ihdr->BitDepth = 8;
	ihdr->ColorType = 2;
	ihdr->Compression = 0;
	ihdr->Filter = 0;
	ihdr->Interlace = 0;
	MakeChunk (ihdr, MAKE_ID('I','H','D','R'), 2*4+5);

	// Assume a display exponent of 2.2 (100000/2.2 ~= 45454.5)
	*gama = BigLong (int (45454.5f * (png_gamma == 0.f ? Gamma : png_gamma)));
	MakeChunk (gama, MAKE_ID('g','A','M','A'), 4);

	FILE * file = fopen(fname, "wb");
	if (file != NULL)
	{
		if (fwrite (work, 1, sizeof(work), file) == sizeof(work))
		{
			if (M_SaveBitmap(scr, w*3, h, w*3, file))
			{
				M_FinishPNG(file);
			}
		}
	}

	fclose(file);
	free(scr);
}

//===========================================================================
//
// averageColor
//  input is RGBA8 pixel format.
//	The resulting RGB color can be scaled uniformly so that the highest 
//	component becomes one.
//
//===========================================================================
PalEntry averageColor(const unsigned long *data, int size, bool maxout)
{
	int				i;
	unsigned int	r, g, b;



	// First clear them.
	r = g = b = 0;
	if (size==0) 
	{
		return PalEntry(255,255,255);
	}
	for(i = 0; i < size; i++)
	{
		r += BPART(data[i]);
		g += GPART(data[i]);
		b += RPART(data[i]);
	}

	r = r/size;
	g = g/size;
	b = b/size;

	int maxv=max(max(r,g),b);

	if(maxv && maxout)
	{
		r *= 255.0f / maxv;
		g *= 255.0f / maxv;
		b *= 255.0f / maxv;
	}
	return PalEntry(r,g,b);
}



