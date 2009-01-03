//-----------------------------------------------------------------------------
//
// Skulltag Source
// Copyright (C) 2008 Benjamin Berkels
// Copyright (C) 2007-2012 Skulltag Development Team
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
// 3. Neither the name of the Skulltag Development Team nor the names of its
//    contributors may be used to endorse or promote products derived from this
//    software without specific prior written permission.
// 4. Redistributions in any form must be accompanied by information on how to
//    obtain complete source code for the software and any accompanying
//    software that uses the software. The source code must either be included
//    in the distribution or be available for no more than the cost of
//    distribution plus a nominal fee, and must be freely redistributable
//    under reasonable conditions. For an executable file, complete source
//    code means the source code for all modules it contains. It does not
//    include source code for modules or files that typically accompany the
//    major components of the operating system on which the executable file
//    runs.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
//
//
// Filename: gl_hqresize.h 
//
// Description: Contains high quality upsampling functions.
// So far Scale2x/3x/4x as described in http://scale2x.sourceforge.net/
// are implemented.
//
//-----------------------------------------------------------------------------

#include "gl_hqresize.h"
#include "c_cvars.h"

CUSTOM_CVAR(Int, gl_texture_hqresize, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	FGLTexture::FlushAll();
}

CUSTOM_CVAR(Int, gl_texture_hqresize_maxinputsize, 512, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	FGLTexture::FlushAll();
}

CUSTOM_CVAR(Int, gl_texture_hqresize_target, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	FGLTexture::FlushAll();
}

void scale2x ( uint32* inputBuffer, uint32* outputBuffer, int inWidth, int inHeight )
{
	const int width = 2* inWidth;
	const int height = 2 * inHeight;

	for ( int i = 0; i < inWidth; ++i )
	{
		const int iMinus = (i > 0) ? (i-1) : 0;
		const int iPlus = (i < inWidth - 1 ) ? (i+1) : i;
		for ( int j = 0; j < inHeight; ++j )
		{
			const int jMinus = (j > 0) ? (j-1) : 0;
			const int jPlus = (j < inHeight - 1 ) ? (j+1) : j;
			const uint32 A = inputBuffer[ iMinus +inWidth*jMinus];
			const uint32 B = inputBuffer[ iMinus +inWidth*j    ];
			const uint32 C = inputBuffer[ iMinus +inWidth*jPlus];
			const uint32 D = inputBuffer[ i     +inWidth*jMinus];
			const uint32 E = inputBuffer[ i     +inWidth*j    ];
			const uint32 F = inputBuffer[ i     +inWidth*jPlus];
			const uint32 G = inputBuffer[ iPlus +inWidth*jMinus];
			const uint32 H = inputBuffer[ iPlus +inWidth*j    ];
			const uint32 I = inputBuffer[ iPlus +inWidth*jPlus];
			if (B != H && D != F) {
				outputBuffer[2*i   + width*2*j    ] = D == B ? D : E;
				outputBuffer[2*i   + width*(2*j+1)] = B == F ? F : E;
				outputBuffer[2*i+1 + width*2*j    ] = D == H ? D : E;
				outputBuffer[2*i+1 + width*(2*j+1)] = H == F ? F : E;
			} else {
				outputBuffer[2*i   + width*2*j    ] = E;
				outputBuffer[2*i   + width*(2*j+1)] = E;
				outputBuffer[2*i+1 + width*2*j    ] = E;
				outputBuffer[2*i+1 + width*(2*j+1)] = E;
			}
		}
	}
}

void scale3x ( uint32* inputBuffer, uint32* outputBuffer, int inWidth, int inHeight )
{
	const int width = 3* inWidth;
	const int height = 3 * inHeight;

	for ( int i = 0; i < inWidth; ++i )
	{
		const int iMinus = (i > 0) ? (i-1) : 0;
		const int iPlus = (i < inWidth - 1 ) ? (i+1) : i;
		for ( int j = 0; j < inHeight; ++j )
		{
			const int jMinus = (j > 0) ? (j-1) : 0;
			const int jPlus = (j < inHeight - 1 ) ? (j+1) : j;
			const uint32 A = inputBuffer[ iMinus +inWidth*jMinus];
			const uint32 B = inputBuffer[ iMinus +inWidth*j    ];
			const uint32 C = inputBuffer[ iMinus +inWidth*jPlus];
			const uint32 D = inputBuffer[ i     +inWidth*jMinus];
			const uint32 E = inputBuffer[ i     +inWidth*j    ];
			const uint32 F = inputBuffer[ i     +inWidth*jPlus];
			const uint32 G = inputBuffer[ iPlus +inWidth*jMinus];
			const uint32 H = inputBuffer[ iPlus +inWidth*j    ];
			const uint32 I = inputBuffer[ iPlus +inWidth*jPlus];
			if (B != H && D != F) {
				outputBuffer[3*i   + width*3*j    ] = D == B ? D : E;
				outputBuffer[3*i   + width*(3*j+1)] = (D == B && E != C) || (B == F && E != A) ? B : E;
				outputBuffer[3*i   + width*(3*j+2)] = B == F ? F : E;
				outputBuffer[3*i+1 + width*3*j    ] = (D == B && E != G) || (D == H && E != A) ? D : E;
				outputBuffer[3*i+1 + width*(3*j+1)] = E;
				outputBuffer[3*i+1 + width*(3*j+2)] = (B == F && E != I) || (H == F && E != C) ? F : E;
				outputBuffer[3*i+2 + width*3*j    ] = D == H ? D : E;
				outputBuffer[3*i+2 + width*(3*j+1)] = (D == H && E != I) || (H == F && E != G) ? H : E;
				outputBuffer[3*i+2 + width*(3*j+2)] = H == F ? F : E;
			} else {
				outputBuffer[3*i   + width*3*j    ] = E;
				outputBuffer[3*i   + width*(3*j+1)] = E;
				outputBuffer[3*i   + width*(3*j+2)] = E;
				outputBuffer[3*i+1 + width*3*j    ] = E;
				outputBuffer[3*i+1 + width*(3*j+1)] = E;
				outputBuffer[3*i+1 + width*(3*j+2)] = E;
				outputBuffer[3*i+2 + width*3*j    ] = E;
				outputBuffer[3*i+2 + width*(3*j+1)] = E;
				outputBuffer[3*i+2 + width*(3*j+2)] = E;
			}
		}
	}
}

void scale4x ( uint32* inputBuffer, uint32* outputBuffer, int inWidth, int inHeight )
{
	int width = 2* inWidth;
	int height = 2 * inHeight;
	uint32 * buffer2x = new uint32[width*height];

	scale2x ( reinterpret_cast<uint32*> ( inputBuffer ), reinterpret_cast<uint32*> ( buffer2x ), inWidth, inHeight );
	width *= 2;
	height *= 2;
	scale2x ( reinterpret_cast<uint32*> ( buffer2x ), reinterpret_cast<uint32*> ( outputBuffer ), 2*inWidth, 2*inHeight );
	delete[] buffer2x;
}


unsigned char *scaleNxHelper( void (*scaleNxFunction) ( uint32* , uint32* , int , int),
							  const int N,
							  unsigned char *inputBuffer,
							  const int inWidth,
							  const int inHeight,
							  int &outWidth,
							  int &outHeight )
{
	outWidth = N * inWidth;
	outHeight = N *inHeight;
	unsigned char * newBuffer = new unsigned char[outWidth*outHeight*4];

	scaleNxFunction ( reinterpret_cast<uint32*> ( inputBuffer ), reinterpret_cast<uint32*> ( newBuffer ), inWidth, inHeight );
	delete[] inputBuffer;
	return newBuffer;
}

//===========================================================================
// 
// [BB] Upsamples the texture in inputBuffer, frees inputBuffer and returns
//  the upsampled buffer.
//
//===========================================================================
unsigned char *gl_CreateUpsampledTextureBuffer ( const FGLTexture *inputGLTexture, unsigned char *inputBuffer, const int inWidth, const int inHeight, int &outWidth, int &outHeight )
{
	// [BB] Don't resample if the width or height of the input texture is bigger than gl_texture_hqresize_maxinputsize.
	if ( ( inWidth > gl_texture_hqresize_maxinputsize ) || ( inHeight > gl_texture_hqresize_maxinputsize ) )
		return inputBuffer;

	// [BB] The hqnx upsampling (not the scaleN one) destroys partial transparency, don't upsamle textures using it.
	if ( inputGLTexture->bIsTransparent == 1 )
		return inputBuffer;

	bool upsample = false;

	switch (gl_texture_hqresize_target)
	{
	case 0:
		{
			upsample = true;
			break;
		}
	case 1:
		{
			BYTE useType = inputGLTexture->tex->UseType;
			if ( ( useType == FTexture::TEX_Sprite )
				|| ( useType == FTexture::TEX_SkinSprite )
				|| ( useType == FTexture::TEX_FontChar ) )
				upsample = true;
		}
	}

	if ( inputBuffer && upsample )
	{
		outWidth = inWidth;
		outHeight = inHeight;
		int type = gl_texture_hqresize;
		switch (type)
		{
		case 1:
			return scaleNxHelper( &scale2x, 2, inputBuffer, inWidth, inHeight, outWidth, outHeight );
		case 2:
			return scaleNxHelper( &scale3x, 3, inputBuffer, inWidth, inHeight, outWidth, outHeight );
		case 3:
			return scaleNxHelper( &scale4x, 4, inputBuffer, inWidth, inHeight, outWidth, outHeight );
		}
	}
	return inputBuffer;
}