#include "gl_pch.h"
/*
** gltexture.cpp
** The texture classes for hardware rendering
** (Even though they are named 'gl' there is nothing hardware dependent
**  in this file. That is all encapsulated in the GLTexture class.)
**
**---------------------------------------------------------------------------
** Copyright 2004-2005 Christoph Oelckers
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

#include "w_wad.h"
#include "m_png.h"
#include "r_draw.h"
#include "sbar.h"
#include "gi.h"
#include "cmdlib.h"
#include "stats.h"
#include "templates.h"
#include "sc_man.h"

#include "gl/gl_struct.h"
#include "gl/gl_framebuffer.h"
#include "gl/gl_texture.h"
#include "gl/gl_functions.h"
#include "gl/gl_shader.h"
#include "gl/gl_translate.h"

CUSTOM_CVAR(Bool, gl_warp_shader, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG|CVAR_NOINITCALL)
{
	if (self && !(gl.flags & RFL_GLSL)) self=0;
}

CUSTOM_CVAR(Bool, gl_colormap_shader, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG|CVAR_NOINITCALL)
{
	if (self && !(gl.flags & RFL_GLSL)) self=0;
}

CUSTOM_CVAR(Bool, gl_brightmap_shader, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG|CVAR_NOINITCALL)
{
	if (self && !(gl.flags & RFL_GLSL)) self=0;
}

// Only for testing for now. This isn't working fully yet.
CUSTOM_CVAR(Bool, gl_glsl_renderer, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG|CVAR_NOINITCALL)
{
	if (!(gl.flags & RFL_GLSL)) 
	{
		if (self) self=0;
	}
	else
	{
		GLShader::Unbind();
		FGLTexture::FlushAll();
	}
}

CUSTOM_CVAR(Bool, gl_texture_usehires, true, CVAR_ARCHIVE|CVAR_NOINITCALL)
{
	FGLTexture::FlushAll();
}

EXTERN_CVAR(Bool, gl_render_precise)

CVAR(Bool, gl_precache, false, CVAR_ARCHIVE)

static char GlobalBrightmap[256];
static bool HasGlobalBrightmap;

//===========================================================================
// 
// Examines the colormap to see if some of the colors have to be
// considered fullbright all the time.
//
//===========================================================================

void gl_GenerateGlobalBrightmapFromColormap()
{
	FMemLump cmap = Wads.ReadLump("COLORMAP");
	FMemLump palette = Wads.ReadLump("PLAYPAL");
	const unsigned char *cmapdata = (const unsigned char *)cmap.GetMem();
	const unsigned char *paldata = (const unsigned char *)palette.GetMem();

	memset(GlobalBrightmap, 255, 256);
	for(int j=0;j<32;j++)
	{
		for(int i=0;i<256;i++)
		{
			// the palette comparison should be for ==0 but that gives false positives with Heretic
			// and Hexen.
			if (cmapdata[i+j*256]!=i || (paldata[3*i]<10 && paldata[3*i+1]<10 && paldata[3*i+2]<10))
			{
				GlobalBrightmap[i]=0;
			}
		}
	}
	for(int i=0;i<256;i++)
	{
		HasGlobalBrightmap|=!!GlobalBrightmap[i];
		//if (GlobalBrightmap[i]) Printf("Marked color %d as fullbright\n",i);
	}
}

static const BYTE IcePalette[16][3] =
{
	{  10,  8, 18 },
	{  15, 15, 26 },
	{  20, 16, 36 },
	{  30, 26, 46 },
	{  40, 36, 57 },
	{  50, 46, 67 },
	{  59, 57, 78 },
	{  69, 67, 88 },
	{  79, 77, 99 },
	{  89, 87,109 },
	{  99, 97,120 },
	{ 109,107,130 },
	{ 118,118,141 },
	{ 128,128,151 },
	{ 138,138,162 },
	{ 148,148,172 }
};

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
	int fac;

	switch(cm)
	{
	case CM_DEFAULT:
		for(i=0;i<count;i++)
		{
			pout[0]=T::R(pin);
			pout[1]=T::G(pin);
			pout[2]=T::B(pin);
			pout[3]=T::A(pin);
			pout+=4;
			pin+=step;
		}
		break;

	case CM_INVERT:
		// Doom's inverted invulnerability map
		for(i=0;i<count;i++)
		{
			gl_InverseMap(T::Gray(pin), pout[0], pout[1], pout[2]);
			pout[3] = T::A(pin);
			pout+=4;
			pin+=step;
		}
		break;

	case CM_GOLDMAP:
		// Heretic's golden invulnerability map
		for(i=0;i<count;i++)
		{
			gl_GoldMap(T::Gray(pin), pout[0], pout[1], pout[2]);
			pout[3] = T::A(pin);
			pout+=4;
			pin+=step;
		}
		break;

	case CM_REDMAP:
		// Skulltag's red Doomsphere map
		for(i=0;i<count;i++)
		{
			gl_RedMap(T::Gray(pin), pout[0], pout[1], pout[2]);
			pout[3] = T::A(pin);
			pout+=4;
			pin+=step;
		}
		break;

	case CM_GREENMAP:
		// Skulltags's Guardsphere map
		for(i=0;i<count;i++)
		{
			gl_GreenMap(T::Gray(pin), pout[0], pout[1], pout[2]);
			pout[3] = T::A(pin);
			pout+=4;
			pin+=step;
		}
		break;

	case CM_GRAY:
		// this is used for colorization of blood.
		// To get the best results the brightness is taken from 
		// the most intense component and not averaged because that would be too dark.
		for(i=0;i<count;i++)
		{
			pout[0] = pout[1] = pout[2] = MAX(MAX(T::R(pin), T::G(pin)), T::B(pin));
			pout[3] = T::A(pin);
			pout+=4;
			pin+=step;
		}
		break;

	case CM_ICE:
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
		break;

	case CM_SHADE:
		// Alpha shade uses the red channel for true color pics
		for(i=0;i<count;i++)
		{
			pout[0] = pout[1] =	pout[2] = 255;
			pout[3] = T::R(pin);
			pout+=4;
			pin+=step;
		}
		break;
	
	default:
		if (cm<=CM_DESAT31)
		{
			// Desaturated light settings.
			fac=cm-CM_DESAT0;
			for(i=0;i<count;i++)
			{
				gl_Desaturate(T::Gray(pin), T::R(pin), T::G(pin), T::B(pin), pout[0], pout[1], pout[2], fac);
				pout[3] = T::A(pin);
				pout+=4;
				pin+=step;
			}
		}
		break;
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
// This excludes all the cases that force downconversion to the
// base palette because they wouldn't be used anyway.
//
//===========================================================================
void OpenGLFrameBuffer::CopyPixelDataRGB(BYTE * buffer, int texpitch, int texheight, int originx, int originy,
										const BYTE * patch, int srcwidth, int srcheight, int step_x, int step_y,
										int ct)
{
	if (ClipCopyPixelRect(texpitch>>2, texheight, originx, originy, patch, srcwidth, srcheight, step_x, step_y))
	{
		buffer+=4*originx + texpitch*originy;
		for (int y=0;y<srcheight;y++)
		{
			copyfuncs[ct](&buffer[y*texpitch], &patch[y*step_y], cm, srcwidth, step_x);
		}
	}
}

//===========================================================================
// 
// Creates one of the special palette translations for the given palette
//
//===========================================================================
void ModifyPalette(PalEntry * pout, PalEntry * pin, int cm, int count)
{
	int i;
	int fac;

	switch(cm)
	{
	case CM_DEFAULT:
		if (pin != pout)
			memcpy(pout, pin, count * sizeof(PalEntry));
		break;

	case CM_INVERT:
		// Doom's inverted invulnerability map
		for(i=0;i<count;i++)
		{
			int gray = (pin[i].r*77 + pin[i].g*143 + pin[i].b*37) >> 8;
			gl_InverseMap(gray, pout[i].r, pout[i].g, pout[i].b);
		}
		break;

	case CM_GOLDMAP:
		// Heretic's golden invulnerability map
		for(i=0;i<count;i++)
		{
			int gray = (pin[i].r*77 + pin[i].g*143 + pin[i].b*37) >> 8;
			gl_GoldMap(gray, pout[i].r, pout[i].g, pout[i].b);
		}
		break;

	case CM_REDMAP:
		// Skulltag's red Doomsphere map
		for(i=0;i<count;i++)
		{
			int gray = (pin[i].r*77 + pin[i].g*143 + pin[i].b*37) >> 8;
			gl_RedMap(gray, pout[i].r, pout[i].g, pout[i].b);
		}
		break;

	case CM_GREENMAP:
		// Skulltags's Guardsphere map
		for(i=0;i<count;i++)
		{
			int gray = (pin[i].r*77 + pin[i].g*143 + pin[i].b*37) >> 8;
			gl_GreenMap(gray, pout[i].r, pout[i].g, pout[i].b);
		}
		break;

	case CM_GRAY:
		// this is used for colorization of blood.
		// To get the best results the brightness is taken from 
		// the most intense component and not averaged because that would be too dark.
		for(i=0;i<count;i++)
		{
			pout[i].r = pout[i].g = pout[i].b = max(max(pin[i].r, pin[i].g), pin[i].b);
			pout[i].a = pin[i].a;
		}
		break;

	case CM_ICE:
		// Create the ice translation table, based on Hexen's.
		// Since this is done in True Color the purplish tint is fully preserved - even in Doom!
		for(i=0;i<count;i++)
		{
			int gray=(pin[i].r*77 + pin[i].g*143 + pin[i].b*37)>>12;

			pout[i].r = IcePalette[gray][0];
			pout[i].g = IcePalette[gray][1];
			pout[i].b = IcePalette[gray][2];
			pout[i].a = pin[i].a;
		}
		break;
	
	default:
		// Boom colormaps.
		if (cm>=CM_FIRSTCOLORMAP && cm<CM_FIRSTCOLORMAP+numfakecmaps)
		{
			if (count<=256)	// This does not work for raw image data because it assumes
							// the use of the base palette!
			{
				// CreateTexBuffer has already taken care of needed palette mapping so this
				// buffer is guaranteed to be in the base palette.
				byte * cmapp = &realcolormaps [NUMCOLORMAPS*256*(cm - CM_FIRSTCOLORMAP)];

				for(i=0;i<count;i++)
				{
					pout[i].r = GPalette.BaseColors[*cmapp].r;
					pout[i].g = GPalette.BaseColors[*cmapp].g;
					pout[i].b = GPalette.BaseColors[*cmapp].b;
					pout[i].a = pin[i].a;
					cmapp++;
				}
			}
			else if (pin != pout)
			{
				// Boom colormaps cannot be applied to hires texture replacements.
				// For those you have to set the colormap usage to  'blend'.
				memcpy(pout, pin, count * sizeof(PalEntry));
			}
		}
		else if (cm<=CM_DESAT31)
		{
			// Desaturated light settings.
			fac=cm-CM_DESAT0;
			for(i=0;i<count;i++)
			{
				int gray=(pin[i].r*77 + pin[i].g*143 + pin[i].b*36)>>8;
				gl_Desaturate(gray, pin[i].r, pin[i].g, pin[i].b, pout[i].r, pout[i].g, pout[i].b, fac);
				pout[i].a = pin[i].a;
			}
		}
		else if (pin!=pout)
		{
			memcpy(pout, pin, count * sizeof(PalEntry));
		}
		break;
	}
}


//===========================================================================
//
// Paletted to True Color texture copy function
//
//===========================================================================
void OpenGLFrameBuffer::CopyPixelData(BYTE * buffer, int texpitch, int texheight, int originx, int originy,
										const BYTE * patch, int srcwidth, int srcheight, 
										int step_x, int step_y, PalEntry * palette)
{
	PalEntry penew[256];

	int x,y,pos,i;

	if (ClipCopyPixelRect(texpitch>>2, texheight, originx, originy, patch, srcwidth, srcheight, step_x, step_y))
	{
		buffer+=4*originx + texpitch*originy;

		// CM_SHADE is an alpha map with 0==transparent and 1==opaque
		if (cm == CM_SHADE) 
		{
			for(int i=0;i<256;i++) 
			{
				if (palette[i].a!=255)
					penew[i]=PalEntry(255-i,255,255,255);
				else
					penew[i]=0xffffffff;	// If the palette contains transparent colors keep them.
			}
		}
		else if (cm == CM_BRIGHTMAP) 
		{
			if (palette == GetPalette() && translation >= 0)
			{
				for(int i=0;i<256;i++) 
				{
					if (GlobalBrightmap[i] && palette[i].a==0)
					{
						penew[i] = PalEntry(0,255,255,255);
					}
					else
					{
						penew[i] = PalEntry(255,0,0,0);
					}
				}
			}
			else
			{
				for(int i=0;i<256;i++) 
					penew[i] = PalEntry(palette[i].a,0,0,0);
			}
		}
		else
		{
			// apply any translation. 
			// The ice and blood color translations are done directly
			// because that yields better results.
			switch(translation)
			{
			case CM_GRAY:
				ModifyPalette(penew, palette, CM_GRAY, 256);
				break;

			case CM_ICE:
				ModifyPalette(penew, palette, CM_ICE, 256);
				break;

			default:
			{
				PalEntry *ptrans = GLTranslationPalette::GetPalette(translation);
				if (ptrans)
				{
					for(i = 0; i < 256; i++)
					{
						penew[i] = (ptrans[i]&0xffffff) | (palette[i]&0xff000000);
					}
					break;
				}
			}

			case 0:
				memcpy(penew, palette, 256*sizeof(PalEntry));
				break;
			}
			if (cm!=0)
			{
				// Apply color modifications like invulnerability, desaturation and Boom colormaps
				ModifyPalette(penew, penew, cm, 256);
			}
		}
		// Now penew contains the actual palette that is to be used for creating the image.

		// convert the image according to the translated palette.
		// Please note that the alpha of the passed palette is inverted. This is
		// so that the base palette can be used without constantly changing it.
		// This can also handle full PNG translucency.
		for (y=0;y<srcheight;y++)
		{
			pos=(y*texpitch);
			for (x=0;x<srcwidth;x++,pos+=4)
			{
				int v=(unsigned char)patch[y*step_y+x*step_x];
				if (penew[v].a==0)
				{
					buffer[pos]=penew[v].r;
					buffer[pos+1]=penew[v].g;
					buffer[pos+2]=penew[v].b;
					buffer[pos+3]=255-penew[v].a;
				}
				else if (penew[v].a!=255)
				{
					buffer[pos  ] = (buffer[pos  ] * penew[v].a + penew[v].r * (1-penew[v].a)) / 255;
					buffer[pos+1] = (buffer[pos+1] * penew[v].a + penew[v].g * (1-penew[v].a)) / 255;
					buffer[pos+2] = (buffer[pos+2] * penew[v].a + penew[v].b * (1-penew[v].a)) / 255;
					buffer[pos+3] = clamp<int>(buffer[pos+3] + (( 255-buffer[pos+3]) * (255-penew[v].a))/255, 0, 255);
				}
			}
		}
	}
}


//===========================================================================
//
// FMultipatchTexture::UseBasePalette
//
// returns true if all patches in the texture use the unmodified base
// palette.
//
//===========================================================================

bool FMultiPatchTexture::UseBasePalette() 
{ 
	for(int i=0;i<NumParts;i++)
	{
		if (Parts[i].Texture->UseBasePalette()) return true;
	}
	return false;
}

//===========================================================================
//
// FWarpTexture::CopyTrueColorPixels
//
// Since the base texture can be anything the warping must be done in
// true color
//
//===========================================================================

int FWarpTexture::CopyTrueColorPixels(BYTE * buffer, int buf_pitch, int buf_height, int xx, int yy)
{
	if (gl_warp_shader || gl_glsl_renderer)
	{
		return SourcePic->CopyTrueColorPixels(buffer, buf_pitch, buf_height, xx, yy);
	}

	unsigned long * in=new unsigned long[Width*Height];
	unsigned long * out;
	bool direct;
	int buf_width = buf_pitch>>2;
	
	FGLTexture *gltex = FGLTexture::ValidateTexture(this);
	gltex->createWarped = true;
	if (Width == buf_width && Height == buf_height && xx==0 && yy==0)
	{
		out = (unsigned long*)buffer;
		direct=true;
	}
	else
	{
		out = new unsigned long[Width*Height];
		direct=false;
	}

	GenTime = r_FrameTime;
	if (SourcePic->bMasked) memset(in, 0, Width*Height*sizeof(long));
	int ret = SourcePic->CopyTrueColorPixels((BYTE*)in, Width<<2, Height, 0, 0);

	static unsigned long linebuffer[4096];	// that's the maximum texture size for most graphics cards!
	int timebase = r_FrameTime*23/28;
	int xsize = Width;
	int ysize = Height;
	int xmask = xsize - 1;
	int ymask = ysize - 1;
	int ds_xbits;
	int i,x;

	for(ds_xbits=-1,i=Width; i; i>>=1, ds_xbits++);

	for (x = xsize-1; x >= 0; x--)
	{
		int yt, yf = (finesine[(timebase+(x+17)*128)&FINEMASK]>>13) & ymask;
		const unsigned long *source = in + x;
		unsigned long *dest = out + x;
		for (yt = ysize; yt; yt--, yf = (yf+1)&ymask, dest += xsize)
		{
			*dest = *(source+(yf<<ds_xbits));
		}
	}
	timebase = r_FrameTime*32/28;
	int y;
	for (y = ysize-1; y >= 0; y--)
	{
		int xt, xf = (finesine[(timebase+y*128)&FINEMASK]>>13) & xmask;
		unsigned long *source = out + (y<<ds_xbits);
		unsigned long *dest = linebuffer;
		for (xt = xsize; xt; xt--, xf = (xf+1)&xmask)
		{
			*dest++ = *(source+xf);
		}
		memcpy (out+y*xsize, linebuffer, xsize*sizeof(unsigned long));
	}

	if (!direct)
	{
		// Negative offsets cannot occur here.
		if (xx<0) xx=0;
		if (yy<0) yy=0;

		unsigned long * targ = ((unsigned long*)buffer) + xx + yy*buf_width;
		int linelen=MIN<int>(Width, buf_width-xx);
		int linecount=MIN<int>(Height, buf_height-yy);

		for(i=0;i<linecount;i++)
		{
			memcpy(targ, &out[Width*i], linelen*sizeof(unsigned long));
			targ+=buf_width;
		}
		delete [] out;
	}
	delete [] in;
	GenTime=r_FrameTime;
	return ret;
}

//===========================================================================
//
// FWarpTexture::CopyTrueColorPixels
//
// Since the base texture can be anything the warping must be done in
// true color
//
//===========================================================================

int FWarp2Texture::CopyTrueColorPixels(BYTE * buffer, int buf_pitch, int buf_height, int xx, int yy)
{
	if (gl_warp_shader || gl_glsl_renderer)
	{
		return SourcePic->CopyTrueColorPixels(buffer, buf_pitch, buf_height, xx, yy);
	}

	unsigned long * in=new unsigned long[Width*Height];
	unsigned long * out;
	bool direct;
	int buf_width = buf_pitch>>2;
	
	FGLTexture *gltex = FGLTexture::ValidateTexture(this);
	gltex->createWarped = true;
	if (Width == buf_width && Height == buf_height && xx==0 && yy==0)
	{
		out = (unsigned long*)buffer;
		direct=true;
	}
	else
	{
		out = new unsigned long[Width*Height];
		direct=false;
	}

	GenTime = r_FrameTime;
	if (SourcePic->bMasked) memset(in, 0, Width*Height*sizeof(long));
	int ret = SourcePic->CopyTrueColorPixels((BYTE*)in, Width<<2, Height, 0, 0);

	int xsize = Width;
	int ysize = Height;
	int xmask = xsize - 1;
	int ymask = ysize - 1;
	int ybits;
	int x, y;
	int i;

	for(ybits=-1,i=ysize; i; i>>=1, ybits++);

	DWORD timebase = r_FrameTime * 40 / 28;
	for (x = xsize-1; x >= 0; x--)
	{
		for (y = ysize-1; y >= 0; y--)
		{
			int xt = (x + 128
				+ ((finesine[(y*128 + timebase*5 + 900) & 8191]*2)>>FRACBITS)
				+ ((finesine[(x*256 + timebase*4 + 300) & 8191]*2)>>FRACBITS)) & xmask;
			int yt = (y + 128
				+ ((finesine[(y*128 + timebase*3 + 700) & 8191]*2)>>FRACBITS)
				+ ((finesine[(x*256 + timebase*4 + 1200) & 8191]*2)>>FRACBITS)) & ymask;
			const unsigned long *source = in + (xt << ybits) + yt;
			unsigned long *dest = out + (x << ybits) + y;
			*dest = *source;
		}
	}

	if (!direct)
	{
		// Negative offsets cannot occur here.
		if (xx<0) xx=0;
		if (yy<0) yy=0;

		unsigned long * targ = ((unsigned long*)buffer) + xx + yy*buf_width;
		int linelen=MIN<int>(Width, buf_width-xx);
		int linecount=MIN<int>(Height, buf_height-yy);

		for(i=0;i<linecount;i++)
		{
			memcpy(targ, &out[Width*i], linelen*sizeof(unsigned long));
			targ+=buf_width;
		}
		delete [] out;
	}
	delete [] in;
	GenTime=r_FrameTime;
	return ret;
}


//===========================================================================
//
// Camera texture rendering
//
//===========================================================================

void FCanvasTexture::RenderGLView (AActor *viewpoint, int fov)
{
	gl_RenderTextureView(this, viewpoint, fov);
	bNeedsUpdate = false;
	bDidUpdate = true;
	bFirstUpdate = false;
}


//==========================================================================
//
// Precaches a GL texture
//
//==========================================================================

void FTexture::PrecacheGL()
{
	if (gl_precache)
	{
		FGLTexture * gltex = FGLTexture::ValidateTexture(this);
		if (gltex) 
		{
			if (UseType==FTexture::TEX_Sprite) 
			{
				gltex->BindPatch(CM_DEFAULT);
			}
			else 
			{
				gltex->Bind (CM_DEFAULT);
			}
		}
	}
}

//==========================================================================
//
// Precaches a GL texture
//
//==========================================================================

void FTexture::UncacheGL()
{
	if (Native)
	{
		FGLTexture * gltex = FGLTexture::ValidateTexture(this);
		if (gltex) gltex->Clean(true); 
	}
}

//===========================================================================
//
// fake brightness maps
// These are generated for textures affected by a colormap with
// fullbright entries.
// These textures are only used internally by the GL renderer so
// all code for software rendering support is missing
//
//===========================================================================

FBrightmapTexture::FBrightmapTexture (FTexture *source)
{
	SourcePic = source;
	CopySize(source);
	bNoDecals = source->bNoDecals;
	Rotations = source->Rotations;
	UseType = source->UseType;
}

FBrightmapTexture::~FBrightmapTexture ()
{
	Unload();
}

const BYTE *FBrightmapTexture::GetColumn (unsigned int column, const Span **spans_out)
{
	// not needed
	return NULL;
}

const BYTE *FBrightmapTexture::GetPixels ()
{
	// not needed
	return NULL;
}

void FBrightmapTexture::Unload ()
{
}

int FBrightmapTexture::CopyTrueColorPixels(BYTE * buffer, int buf_width, int buf_height, int x, int y)
{
	SetTranslationInfo(CM_BRIGHTMAP, 0);
	SourcePic->CopyTrueColorPixels(buffer, buf_width, buf_height, x, y);
	return 0;
}

//===========================================================================
//
// The GL texture maintenance class
//
//===========================================================================
TArray<FGLTexture *> * FGLTexture::gltextures;

//===========================================================================
//
// Constructor
//
//===========================================================================
FGLTexture::FGLTexture(FTexture * tx)
{
	tex = tx;

	glpatch=NULL;
	gltexture=NULL;

	Shader = NULL;

	HiresLump=-1;
	hirestexture = NULL;

	areacount = 0;
	areas = NULL;
	createWarped = false;

	bHasColorkey = false;

	Width = tex->GetWidth();
	Height = tex->GetHeight();
	LeftOffset = tex->LeftOffset;
	TopOffset = tex->TopOffset;
	RenderWidth = tex->GetScaledWidth();
	RenderHeight = tex->GetScaledHeight();

	scalex = tex->xScale/(float)FRACUNIT;
	scaley = tex->yScale/(float)FRACUNIT;


	// a little adjustment to make sprites look better with texture filtering:
	// create a 1 pixel wide empty frame around them.
	if (tex->UseType == FTexture::TEX_Sprite || 
		tex->UseType == FTexture::TEX_SkinSprite || 
		tex->UseType == FTexture::TEX_Decal)
	{
		RenderWidth+=2;
		RenderHeight+=2;
		Width+=2;
		Height+=2;
		LeftOffset+=1;
		TopOffset+=1;
	}

	if ((gl.flags & RFL_GLSL) && tx->UseBasePalette() && HasGlobalBrightmap &&
		tx->UseType != FTexture::TEX_Autopage && tx->UseType != FTexture::TEX_Decal &&
		tx->UseType != FTexture::TEX_MiscPatch && tx->UseType != FTexture::TEX_FontChar &&
		tex->bm_info.Brightmap == NULL
		) 
	{
		tex->bm_info.Brightmap = new FBrightmapTexture(tx);
		tex->bm_info.Brightmap->bm_info.bIsBrightmap=-1;
	}
	bIsTransparent = -1;

	if (tex->bHasCanvas) scaley=-scaley;

	if (!gltextures) 
		gltextures = new TArray<FGLTexture *>;
	index = gltextures->Push(this);

}

//===========================================================================
//
// Destructor
//
//===========================================================================

FGLTexture::~FGLTexture()
{
	Clean(true);
	if (areas) delete [] areas;
	if (hirestexture) delete hirestexture;

	for(int i=0;i<gltextures->Size();i++)
	{
		if ((*gltextures)[i]==this) 
		{
			gltextures->Delete(i);
			break;
		}
	}
}

//===========================================================================
//
// Update
//
//===========================================================================

bool FGLTexture::Update ()
{
	return false;
}

//===========================================================================
//
// GetRect
//
//===========================================================================
void FGLTexture::GetRect(GL_RECT * r) const
{
	r->left=-(float)GetScaledLeftOffset();
	r->top=-(float)GetScaledTopOffset();
	r->width=(float)TextureWidth();
	r->height=(float)TextureHeight();
}

//===========================================================================
// 
//	Finds gaps in the texture which can be skipped by the renderer
//  This was mainly added to speed up one area in E4M6 of 007LTSD
//
//===========================================================================
bool FGLTexture::FindHoles(const unsigned char * buffer, int w, int h)
{
	const unsigned char * li;
	int y,x;
	int startdraw,lendraw;
	int gaps[5][2];
	int gapc=0;


	// already done!
	if (areacount) return false;
	if (tex->UseType==FTexture::TEX_Flat) return false;	// flats don't have transparent parts
	areacount=-1;	//whatever happens next, it shouldn't be done twice!

	// large textures are excluded for performance reasons
	if (h>256) return false;	

	startdraw=-1;
	lendraw=0;
	for(y=0;y<h;y++)
	{
		li=buffer+w*y*4+3;

		for(x=0;x<w;x++,li+=4)
		{
			if (*li!=0) break;
		}

		if (x!=w)
		{
			// non - transparent
			if (startdraw==-1) 
			{
				startdraw=y;
				// merge transparent gaps of less than 16 pixels into the last drawing block
				if (gapc && y<=gaps[gapc-1][0]+gaps[gapc-1][1]+16)
				{
					gapc--;
					startdraw=gaps[gapc][0];
					lendraw=y-startdraw;
				}
				if (gapc==4) return false;	// too many splits - this isn't worth it
			}
			lendraw++;
		}
		else if (startdraw!=-1)
		{
			if (lendraw==1) lendraw=2;
			gaps[gapc][0]=startdraw;
			gaps[gapc][1]=lendraw;
			gapc++;

			startdraw=-1;
			lendraw=0;
		}
	}
	if (startdraw!=-1)
	{
		gaps[gapc][0]=startdraw;
		gaps[gapc][1]=lendraw;
		gapc++;
	}
	if (startdraw==0 && lendraw==h) return false;	// nothing saved so don't create a split list

	GL_RECT * rcs=new GL_RECT[gapc];

	for(x=0;x<gapc;x++)
	{
		// gaps are stored as texture (u/v) coordinates
		rcs[x].width=rcs[x].left=-1.0f;
		rcs[x].top=(float)gaps[x][0]/(float)h;
		rcs[x].height=(float)gaps[x][1]/(float)h;
	}
	areas=rcs;
	areacount=gapc;

	return false;
}


//===========================================================================
// 
// smooth the edges of transparent fields in the texture
// returns false when nothing is manipulated to save the work on further
// levels

// 28/10/2003: major optimization: This function was far too pedantic.
// taking the value of one of the neighboring pixels is fully sufficient
//
//===========================================================================
#define CHKPIX(ofs) \
	(l1[(ofs)*4+3]==255 ? (( ((long*)l1)[0] = ((long*)l1)[ofs]&0xffffff), trans=true ) : false)

bool FGLTexture::SmoothEdges(unsigned char * buffer,int w, int h, bool clampsides)
{
	int x,y;
	bool trans=buffer[3]==0; // If I set this to false here the code won't detect textures 
							 // that only contain transparent pixels.
	unsigned char * l1;

	if (h<=1 || w<=1) return false;	// makes (a) no sense and (b) doesn't work with this code!

	l1=buffer;


	if (l1[3]==0 && !CHKPIX(1)) CHKPIX(w);
	l1+=4;
	for(x=1;x<w-1;x++, l1+=4)
	{
		if (l1[3]==0 &&	!CHKPIX(-1) && !CHKPIX(1)) CHKPIX(w);
	}
	if (l1[3]==0 && !CHKPIX(-1)) CHKPIX(w);
	l1+=4;

	for(y=1;y<h-1;y++)
	{
		if (l1[3]==0 && !CHKPIX(-w) && !CHKPIX(1)) CHKPIX(w);
		l1+=4;
		for(x=1;x<w-1;x++, l1+=4)
		{
			if (l1[3]==0 &&	!CHKPIX(-w) && !CHKPIX(-1) && !CHKPIX(1)) CHKPIX(w);
		}
		if (l1[3]==0 && !CHKPIX(-w) && !CHKPIX(-1)) CHKPIX(w);
		l1+=4;
	}

	if (l1[3]==0 && !CHKPIX(-w)) CHKPIX(1);
	l1+=4;
	for(x=1;x<w-1;x++, l1+=4)
	{
		if (l1[3]==0 &&	!CHKPIX(-w) && !CHKPIX(-1)) CHKPIX(1);
	}
	if (l1[3]==0 && !CHKPIX(-w)) CHKPIX(-1);

	return trans;
}


//===========================================================================
// 
// Post-process the texture data after the buffer has been created
//
//===========================================================================
bool FGLTexture::ProcessData(unsigned char * buffer, int w, int h, int cm, bool ispatch)
{
	if (tex->bm_info.bIsBrightmap==-1)
	{
		DWORD * dwbuf = (DWORD*)buffer;
		for(int i=0;i<w*h;i++)
		{
			if ((dwbuf[i]&0xffffff) != 0)
			{
				tex->bm_info.bIsBrightmap = 1;
				break;
			}
		}
		if (tex->bm_info.bIsBrightmap == -1) 
		{
			tex->bm_info.bIsBrightmap = 0;
			return false;
		}
	}
	else if (tex->bMasked && !tex->bm_info.bIsBrightmap) 
	{
		tex->bMasked=SmoothEdges(buffer, w, h, ispatch);
		if (tex->bMasked && !ispatch) FindHoles(buffer, w, h);
	}
	return true;
}


//===========================================================================
// 
//  Checks for transparent pixels if there is no simpler means to get
//  this information
//
//===========================================================================
void FGLTexture::CheckTrans(unsigned char * buffer, int size, int trans)
{
	if (bIsTransparent == -1) 
	{
		bIsTransparent = trans;
		if (trans == -1)
		{
			DWORD * dwbuf = (DWORD*)buffer;
			if (bIsTransparent == -1) for(int i=0;i<size;i++)
			{
				DWORD alpha = dwbuf[i]>>24;

				if (alpha != 0xff && alpha != 0)
				{
					bIsTransparent = 1;
					return;
				}
			}
		}
		bIsTransparent = 0;
	}
}

//===========================================================================
// 
//	Deletes all allocated resources
//
//===========================================================================

void FGLTexture::Clean(bool all)
{
	WorldTextureInfo::Clean(all);
	PatchTextureInfo::Clean(all);
	createWarped = false;
}

//===========================================================================
// 
//	Initializes the buffer for the texture data
//
//===========================================================================

unsigned char * FGLTexture::CreateTexBuffer(int _cm, int translation, int & w, int & h, bool allowhires)
{
	unsigned char * buffer;
	intptr_t cm = _cm;

	SetTranslationInfo(cm, translation);

	// Textures that are already scaled in the texture lump will not get replaced
	// by hires textures
	if (gl_texture_usehires && allowhires && scalex==1.f && scaley==1.f)
	{
		buffer = LoadHiresTexture (&w, &h);
		if (buffer)
		{
			return buffer;
		}
	}

	w=Width;
	h=Height;

	buffer=new unsigned char[Width*(Height+1)*4];
	memset(buffer, 0, Width * (Height+1) * 4);

	if (translation<=0)
	{
		int trans = 
			tex->CopyTrueColorPixels(buffer, GetWidth()<<2, GetHeight(), GetLeftOffset() - tex->LeftOffset, 
				GetTopOffset() - tex->TopOffset);

		CheckTrans(buffer, w*h, trans);

	}
	else
	{
		// When using translations everything must be mapped to the base palette.
		// Since FTexture's method is doing exactly that by calling GetPixels let's use that here
		// to do all the dirty work for us. ;)
		tex->FTexture::CopyTrueColorPixels(buffer, GetWidth()<<2, GetHeight(), GetLeftOffset() - tex->LeftOffset, 
				GetTopOffset() - tex->TopOffset);
	}

	return buffer;
}

//===========================================================================
// 
//	Gets texture coordinate info for world (wall/flat) textures
//  The wrapper class is there to provide a set of coordinate
//  functions to access the texture
//
//===========================================================================
const WorldTextureInfo * FGLTexture::GetWorldTextureInfo()
{

	if (tex->UseType==FTexture::TEX_Null) return NULL;		// Cannot register a NULL texture!
	if (!gltexture) gltexture=new GLTexture(Width, Height, true, true);
	if (gltexture) return (WorldTextureInfo*)this; 	
	return NULL;
}

//===========================================================================
// 
//	Gets texture coordinate info for sprites
//  The wrapper class is there to provide a set of coordinate
//  functions to access the texture
//
//===========================================================================
const PatchTextureInfo * FGLTexture::GetPatchTextureInfo()
{
	if (tex->UseType==FTexture::TEX_Null) return NULL;		// Cannot register a NULL texture!
	if (!glpatch) 
	{
		glpatch=new GLTexture(Width, Height, false, false);
	}
	if (glpatch) return (PatchTextureInfo*)this; 	
	return NULL;
}

//===========================================================================
// 
//	Binds a texture to the renderer
//
//===========================================================================

const WorldTextureInfo * FGLTexture::Bind(int texunit, int cm, int clampmode, int translation)
{
	bool usebright = false;

	translation = GLTranslationPalette::GetInternalTranslation(translation);

	if (GetWorldTextureInfo())
	{
		if (texunit == 0)
		{
			FTexture *brightmap = tex->bm_info.Brightmap;

			if (brightmap && (gl_glsl_renderer || gl_brightmap_shader) && translation >= 0 &&
				cm >= CM_DEFAULT && cm <= CM_DESAT31 && gl_brightmapenabled)
			{
				FGLTexture *bmgltex = FGLTexture::ValidateTexture(brightmap);
				bmgltex->Bind(1, CM_DEFAULT, clampmode, 0);
				if (brightmap->bm_info.bIsBrightmap == 0)
				{
					delete brightmap;
					tex->bm_info.Brightmap = NULL;
				}
				else usebright = true;
			}

			if (!gl_glsl_renderer)
			{
				if (gl.flags & RFL_GLSL)
				{
					if (createWarped && gl_warp_shader && tex->bWarped!=0)
					{
						Clean(true);
						GetWorldTextureInfo();
					}
						
					if ((gl_warp_shader && tex->bWarped!=0) || 
						(usebright) ||
						((tex->bHasCanvas || gl_colormap_shader) && cm!=CM_DEFAULT && /*!(cm>=CM_DESAT1 && cm<=CM_DESAT31) &&*/  cm!=CM_SHADE && gl_texturemode != TM_MASK))
					{
						Shader->Bind(cm, usebright);
						if (cm != CM_SHADE) cm = CM_DEFAULT;
					}
					else
					{
						GLShader::Unbind();
					}
				}

				if (tex->bWarped == 0 || !gl_warp_shader)
				{
					// If this is a warped texture that needs updating
					// delete all system textures created for this
					if (tex->CheckModified() && !tex->bHasCanvas && HiresLump<0 && HiresLump!=-2)
					{
						gltexture->Clean(true);
					}
				}
			}
			else
			{
				Shader->Bind(cm, usebright);
				if (cm != CM_SHADE) cm = CM_DEFAULT;
			}
		}

		// Bind it to the system.
		// clamping in x-direction may cause problems when rendering segs
		if (!gltexture->Bind(texunit, cm, translation, gl_render_precise? clampmode&GLT_CLAMPY : clampmode))
		{
			int w,h;

			// Create this texture
			unsigned char * buffer = CreateTexBuffer(cm, translation, w, h);

			ProcessData(buffer, w, h, cm, false);
			if (!gltexture->CreateTexture(buffer, w, h, true, texunit, cm, translation)) 
			{
				// could not create texture
				delete buffer;
				return NULL;
			}
			delete buffer;
		}
		if (texunit == 0) gltexture->SetTextureClamp(gl_render_precise? clampmode&GLT_CLAMPY : clampmode);

		if (tex->bHasCanvas) static_cast<FCanvasTexture*>(tex)->NeedUpdate();
		return (WorldTextureInfo*)this; 
	}
	return NULL;
}

const WorldTextureInfo * FGLTexture::Bind(int cm, int clampmode, int translation)
{
	return Bind(0, cm, clampmode, translation);
}
//===========================================================================
// 
//	Binds a sprite to the renderer
//
//===========================================================================
const PatchTextureInfo * FGLTexture::BindPatch(int texunit, int cm, int translation)
{
	bool usebright = false;
	int transparm = translation;

	translation = GLTranslationPalette::GetInternalTranslation(translation);

	if (GetPatchTextureInfo())
	{
		if (texunit == 0)
		{
			FTexture *brightmap = tex->bm_info.Brightmap;

			if (brightmap && (gl_glsl_renderer || gl_brightmap_shader) && translation >= 0 && 
				transparm >= 0 && cm >= CM_DEFAULT && cm <= CM_DESAT31 && gl_brightmapenabled)
			{
				FGLTexture *bmgltex = FGLTexture::ValidateTexture(brightmap);
				bmgltex->BindPatch(1, CM_DEFAULT, 0);
				if (brightmap->bm_info.bIsBrightmap == 0)
				{
					delete brightmap;
					tex->bm_info.Brightmap = NULL;
				}
				else 
				{
					usebright = true;
				}
			}

			if (!gl_glsl_renderer)
			{
				if (gl.flags & RFL_GLSL)
				{
					if (createWarped && gl_warp_shader && tex->bWarped!=0)
					{
						Clean(true);
						GetPatchTextureInfo();
					}

					if ((gl_warp_shader && tex->bWarped!=0) || 
						(usebright) ||
						((tex->bHasCanvas || gl_colormap_shader) && cm!=CM_DEFAULT && /*!(cm>=CM_DESAT1 && cm<=CM_DESAT31) &&*/ cm!=CM_SHADE && gl_texturemode != TM_MASK))
					{
						Shader->Bind(cm, usebright);
						if (cm != CM_SHADE) cm = CM_DEFAULT;
					}
					else
					{
						GLShader::Unbind();
					}
				}

				if (tex->bWarped == 0 || !gl_warp_shader)
				{
					// If this is a warped texture that needs updating
					// delete all system textures created for this
					if (tex->CheckModified() && !tex->bHasCanvas && HiresLump<0 && HiresLump!=-2)
					{
						glpatch->Clean(true);
					}
				}
			}
			else
			{
				Shader->Bind(cm, usebright);
				if (cm != CM_SHADE) cm = CM_DEFAULT;
			}
		}

		// Bind it to the system. For multitexturing this
		// should be the only thing that needs adjusting
		if (!glpatch->Bind(texunit, cm, translation, -1))
		{
			int w, h;

			// Create this texture
			unsigned char * buffer = CreateTexBuffer(cm, translation, w, h, false);
			ProcessData(buffer, w, h, cm, true);
			if (!glpatch->CreateTexture(buffer, w, h, false, texunit, cm, translation)) 
			{
				// could not create texture
				delete buffer;
				return NULL;
			}
			delete buffer;
		}
		if (gl_render_precise)
		{
			gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		}
		return (PatchTextureInfo*)this; 	
	}
	return NULL;
}

const PatchTextureInfo * FGLTexture::BindPatch(int cm, int translation)
{
	return BindPatch(0, cm, translation);
}

//==========================================================================
//
// Flushes all hardware dependent data
//
//==========================================================================

void FGLTexture::FlushAll()
{
	if (gltextures)
	{
		for(int i=gltextures->Size()-1;i>=0;i--)
		{
			(*gltextures)[i]->Clean(true);
		}
	}
}

//==========================================================================
//
// Gets a texture from the texture manager and checks its validity for
// GL rendering. 
//
//==========================================================================

FGLTexture * FGLTexture::ValidateTexture(FTexture * tex)
{
	if (tex	&& tex->UseType!=FTexture::TEX_Null)
	{
		FGLTexture *gltex = static_cast<FGLTexture*>(tex->GetNative(false));

		if (gltex)
		{
			// This can't be put in the constructor because many
			// textures are created before the GL renderer is operable
			if ((gl.flags & RFL_GLSL) && !gltex->Shader) switch(tex->bWarped)
			{
			default:
				gltex->Shader = GLShader::Find("Default");
				break;
			case 1:
				gltex->Shader = GLShader::Find("Warp 1");
				break;
			case 2:
				gltex->Shader = GLShader::Find("Warp 2");
				break;
			}
		}
		return gltex;
	}
	return NULL;
}

FGLTexture * FGLTexture::ValidateTexture(int no, bool translate)
{
	return FGLTexture::ValidateTexture(translate? TexMan(no) : TexMan[no]);
}


//==========================================================================
//
// Parses a brightmap definition
//
//==========================================================================

void gl_ParseBrightmap(FScanner &sc, int deflump)
{
	int type = FTexture::TEX_Any;
	bool disable_fullbright=false;
	bool thiswad = false;
	bool iwad = false;
	int maplump = -1;
	FString maplumpname;

	sc.MustGetString();
	if (sc.Compare("texture")) type = FTexture::TEX_Wall;
	else if (sc.Compare("flat")) type = FTexture::TEX_Flat;
	else if (sc.Compare("sprite")) type = FTexture::TEX_Sprite;
	else sc.UnGet();

	sc.MustGetString();
	int no = TexMan.CheckForTexture(sc.String, type);
	FTexture *tex = TexMan[no];

	sc.MustGetToken('{');
	while (!sc.CheckToken('}'))
	{
		sc.MustGetString();
		if (sc.Compare("disablefullbright"))
		{
			// This can also be used without a brightness map to disable
			// fullbright in rotations that only use brightness maps on
			// other angles.
			disable_fullbright = true;
		}
		else if (sc.Compare("thiswad"))
		{
			// only affects textures defined in the WAD containing the definition file.
			thiswad = true;
		}
		else if (sc.Compare ("iwad"))
		{
			// only affects textures defined in the IWAD.
			iwad = true;
		}
		else if (sc.Compare ("map"))
		{
			sc.MustGetString();

			if (maplump >= 0)
			{
				Printf("Multiple brightmap definitions in texture %s\n", tex? tex->Name : "(null)");
			}

			maplump = Wads.CheckNumForFullName(sc.String);

			// Try a normal WAD name lookup only if it's a proper name without path separator and
			// not longer than 8 characters.
			if (maplump==-1 && strlen(sc.String) <= 8 && !strchr(sc.String, '/')) 
				maplump = Wads.CheckNumForName(sc.String);

			if (maplump==-1) 
				Printf("Brightmap '%s' not found in texture '%s'\n", sc.String, tex? tex->Name : "(null)");

			maplumpname = sc.String;
		}
	}
	if (!tex)
	{
		return;
	}
	if (thiswad || iwad)
	{
		bool useme = false;
		int lumpnum = tex->GetSourceLump();

		if (lumpnum != -1)
		{
			if (iwad && Wads.GetLumpFile(lumpnum) <= FWadCollection::IWAD_FILENUM) useme = true;
			if (thiswad && Wads.GetLumpFile(lumpnum) == deflump) useme = true;
		}
		if (!useme) return;
	}

	if (maplump != -1)
	{
		FTexture *brightmap = FTexture::CreateTexture(maplump, tex->UseType);
		if (!brightmap)
		{
			Printf("Unable to create texture from '%s' in brightmap definition for '%s'\n", 
				maplumpname.GetChars(), tex->Name);
			return;
		}
		tex->bm_info.Brightmap = brightmap;
		brightmap->bm_info.bIsBrightmap = true;
	}	
	tex->bm_info.bBrightmapDisablesFullbright = disable_fullbright;
}
