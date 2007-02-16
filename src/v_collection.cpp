/*
** v_collection.cpp
** Holds a collection of images
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
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
*/

#include "v_collection.h"
#include "v_font.h"
#include "v_video.h"
#include "m_swap.h"
#include "w_wad.h"

FImageCollection::FImageCollection ()
: NumImages (0), ImageMap (0)
{
}

FImageCollection::FImageCollection (const char **patchNames, int numPatches)
{
	Init (patchNames, numPatches);
}

FImageCollection::~FImageCollection ()
{
	Uninit ();
}

void FImageCollection::Init (const char **patchNames, int numPatches, int namespc)
{
	NumImages = numPatches;
	ImageMap = new int[numPatches];

	for (int i = 0; i < numPatches; ++i)
	{
		int picnum = TexMan.AddPatch (patchNames[i], namespc);

		if (picnum == -1 && namespc != ns_sprites)
		{
			picnum = TexMan.AddPatch (patchNames[i], ns_sprites);
		}
		ImageMap[i] = picnum;
	}
}

void FImageCollection::Uninit ()
{
	if (ImageMap != NULL)
	{
		delete[] ImageMap;
		ImageMap = NULL;
	}
	NumImages = 0;
}

FTexture *FImageCollection::operator[] (int index) const
{
	if ((unsigned int)index >= (unsigned int)NumImages)
	{
		return NULL;
	}
	return ImageMap[index] < 0 ? NULL : TexMan[ImageMap[index]];
}
