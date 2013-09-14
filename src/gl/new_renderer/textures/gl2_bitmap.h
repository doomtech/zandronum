#ifndef __GL_BITMAP_H
#define __GL_BITMAP_H

#include "textures/bitmap.h"

namespace GLRendererNew
{

class FGLBitmap : public FBitmap
{
	int cm;
public:

	FGLBitmap() { cm = 0; }
	FGLBitmap(BYTE *buffer, int pitch, int width, int height, int _cm = 0)
		: FBitmap(buffer, pitch, width, height)
	{ cm = _cm; }

	virtual void CopyPixelDataRGB(int originx, int originy, const BYTE *patch, int srcwidth, 
								int srcheight, int step_x, int step_y, int rotate, int ct, FCopyInfo *inf = NULL);
	virtual void CopyPixelData(int originx, int originy, const BYTE * patch, int srcwidth, int srcheight, 
								int step_x, int step_y, int rotate, PalEntry * palette, FCopyInfo *inf = NULL);
};


}

#endif
