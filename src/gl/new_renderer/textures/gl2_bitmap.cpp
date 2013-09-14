//
//-----------------------------------------------------------------------------
//
// Copyright (C) 2009 Christoph Oelckers
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// As an exception to the GPL this code may be used in GZDoom
// derivatives under the following conditions:
//
// 1. The license of these files is not changed
// 2. Full source of the derived program is disclosed
//
//
// ----------------------------------------------------------------------------
//
// Bitmap class for texture creation
//

#include "gl/new_renderer/textures/gl2_bitmap.h"
#include "r_translate.h"

namespace GLRendererNew
{

//===========================================================================
// 
// multi-format pixel copy with colormap application
// requires one of the previously defined conversion classes to work
//
//===========================================================================
template<class T>
void iCopyColors(unsigned char * pout, const unsigned char * pin, int cm, int count, int step)
{
	int i;

	if (cm)
	{
		// Create the ice translation table, based on Hexen's.
		// Since this is done in True Color the purplish tint is fully preserved - even in Doom!
		for(i=0;i<count;i++)
		{
			int gray = T::Gray(pin)>>4;

			pout[0] = IcePalette[gray][0];
			pout[1] = IcePalette[gray][1];
			pout[2] = IcePalette[gray][2];
			pout[3] = 255;
			pout+=4;
			pin+=step;
		}
	}
	else
	{
		for(i=0;i<count;i++)
		{
			pout[0]=T::R(pin);
			pout[1]=T::G(pin);
			pout[2]=T::B(pin);
			pout[3]=T::A(pin);
			pout+=4;
			pin+=step;
		}
	}
}

typedef void (*CopyFunc)(unsigned char * pout, const unsigned char * pin, int cm, int count, int step);

static CopyFunc copyfuncs[]={
	iCopyColors<cRGB>,
	iCopyColors<cRGBA>,
	iCopyColors<cIA>,
	iCopyColors<cCMYK>,
	iCopyColors<cBGR>,
	iCopyColors<cBGRA>,
	iCopyColors<cI16>,
	iCopyColors<cRGB555>,
	iCopyColors<cPalEntry>
};

//===========================================================================
//
// True Color texture copy function
//
//===========================================================================
void FGLBitmap::CopyPixelDataRGB(int originx, int originy,
								const BYTE * patch, int srcwidth, int srcheight, int step_x, int step_y,
								int rotate, int ct, FCopyInfo *inf)
{
	if (ClipCopyPixelRect(Width, Height, originx, originy, patch, srcwidth, srcheight, step_x, step_y, rotate))
	{
		BYTE *buffer = GetPixels() + 4*originx + Pitch*originy;
		for (int y=0;y<srcheight;y++)
		{
			copyfuncs[ct](&buffer[y*Pitch], &patch[y*step_y], cm, srcwidth, step_x);
		}
	}
}

//===========================================================================
//
// Paletted to True Color texture copy function
//
//===========================================================================
void FGLBitmap::CopyPixelData(int originx, int originy, const BYTE * patch, int srcwidth, int srcheight, 
										int step_x, int step_y, int rotate, PalEntry * palette, FCopyInfo *inf)
{
	PalEntry penew[256];

	int x,y,pos,i;

	if (ClipCopyPixelRect(Width, Height, originx, originy, patch, srcwidth, srcheight, step_x, step_y, rotate))
	{
		BYTE *buffer = GetPixels() + 4*originx + Pitch*originy;

		if (cm != 0)
		{
			for(i=0;i<256;i++)
			{
				int gray=(palette[i].r*77 + palette[i].g*143 + palette[i].b*37)>>12;

				penew[i].r = IcePalette[gray][0];
				penew[i].g = IcePalette[gray][1];
				penew[i].b = IcePalette[gray][2];
				penew[i].a = palette[i].a;
			}
			palette = penew;
		}

		for (y=0;y<srcheight;y++)
		{
			pos=(y*Pitch);
			for (x=0;x<srcwidth;x++,pos+=4)
			{
				int v=(unsigned char)patch[y*step_y+x*step_x];
				if (palette[v].a!=0)
				{
					buffer[pos]   = palette[v].r;
					buffer[pos+1] = palette[v].g;
					buffer[pos+2] = palette[v].b;
					buffer[pos+3] = palette[v].a;
				}
			}
		}
	}
}




}
