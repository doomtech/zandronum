/*
** gfxfuncs.cpp
** True color graphics functions
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
#include "gl_values.h"
#include "gl_functions.h"
#include "r_main.h"
#include "m_swap.h"
#include "m_png.h"
#include "m_crc32.h"
#include "templates.h"
#include "v_palette.h"

//===========================================================================
//
// averageColor
//  input is RGBA8 pixel format.
//	The resulting RGB color can be scaled uniformly so that the highest 
//	component becomes one.
//
//===========================================================================
PalEntry averageColor(const DWORD *data, int size, fixed_t maxout_factor)
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

	int maxv=MAX(MAX(r,g),b);

	if(maxv && maxout_factor)
	{
		maxout_factor = FixedMul(maxout_factor, 255);
		r = Scale(r, maxout_factor, maxv);
		g = Scale(g, maxout_factor, maxv);
		b = Scale(b, maxout_factor, maxv);
	}
	return PalEntry(r,g,b);
}


//==========================================================================
//
// Modifies a color according to a specified colormap
//
//==========================================================================

void gl_ModifyColor(BYTE & red, BYTE & green, BYTE & blue, int cm)
{
	int gray = (red*77 + green*143 + blue*36)>>8;
	if (cm == CM_INVERT /* || cm == CM_LITE*/)
	{
		gl_InverseMap(gray, red, green, blue);
	}
	else if (cm == CM_GOLDMAP)
	{
		gl_GoldMap(gray, red, green, blue);
	}
	else if (cm == CM_REDMAP)
	{
		gl_RedMap(gray, red, green, blue);
	}
	else if (cm == CM_GREENMAP)
	{
		gl_GreenMap(gray, red, green, blue);
	}
	else if (cm >= CM_DESAT1 && cm <= CM_DESAT31)
	{
		gl_Desaturate(gray, red, green, blue, red, green, blue, cm - CM_DESAT0);
	}
}

