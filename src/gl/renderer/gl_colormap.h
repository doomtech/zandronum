#ifndef __GL_COLORMAP
#define __GL_COLORMAP

#include "doomtype.h"
#include "v_palette.h"

extern DWORD gl_fixedcolormap;


enum EColorManipulation
{

	CM_INVALID=-1,
	CM_DEFAULT=0,					// untranslated
	CM_DESAT0=CM_DEFAULT,
	CM_DESAT1,					// minimum desaturation
	CM_DESAT31=CM_DESAT1+30,	// maximum desaturation = grayscale
	CM_FIRSTSPECIALCOLORMAP,		// first special fixed colormap

	// special internal values for texture creation
	CM_GRAY = 0x1000000,		// a simple grayscale map for colorizing blood splats
	CM_ICE	= 0x1000001,		// The bluish ice translation for frozen corpses
	CM_SHADE= 0x1000002,		// alpha channel texture

	// These are not to be passed to the texture manager
	CM_LITE	= 0x2000000,		// special values to handle these items without excessive hacking
	CM_TORCH= 0x2000010,		// These are not real color manipulations
};

  // for internal use
struct FColormap
{
	PalEntry		LightColor;		// a is saturation (0 full, 31=b/w, other=custom colormap)
	PalEntry		FadeColor;		// a is fadedensity>>1
	int				colormap;
	int				blendfactor;

	void Clear()
	{
		LightColor=0xffffff;
		FadeColor=0;
		colormap = CM_DEFAULT;
		blendfactor=0;
	}

	void ClearColor()
	{
		LightColor.r=LightColor.g=LightColor.b=0xff;
		blendfactor=0;
	}


	void GetFixedColormap()
	{
		Clear();
		colormap = gl_fixedcolormap >= CM_LITE? CM_DEFAULT : gl_fixedcolormap;
	}

	FColormap & operator=(FDynamicColormap * from)
	{
		LightColor = from->Color;
		colormap = from->Desaturate>>3;
		FadeColor = from->Fade;
		blendfactor = from->Color.a;
		return * this;
	}

	void CopyLightColor(FDynamicColormap * from)
	{
		LightColor = from->Color;
		colormap = from->Desaturate>>3;
		blendfactor = from->Color.a;
	}
};



#endif
