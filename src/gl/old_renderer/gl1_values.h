#ifndef __GL_VALUES
#define __GL_VALUES

#include "doomtype.h"

enum GLDrawItemType
{
	GLDIT_WALL,
	GLDIT_FLAT,
	GLDIT_SPRITE,
	GLDIT_POLY,
};

enum DrawListType
{
	// These are organized so that the various multipass rendering modes
	// have to be set as few times as possible
	GLDL_LIGHT,	
	GLDL_LIGHTBRIGHT,
	GLDL_LIGHTMASKED,
	GLDL_LIGHTFOG,
	GLDL_LIGHTFOGMASKED,

	GLDL_PLAIN,
	GLDL_MASKED,
	GLDL_FOG,
	GLDL_FOGMASKED,

	GLDL_TRANSLUCENT,
	GLDL_TRANSLUCENTBORDER,

	GLDL_TYPES,

	GLDL_FIRSTLIGHT = GLDL_LIGHT,
	GLDL_LASTLIGHT = GLDL_LIGHTFOGMASKED,
	GLDL_FIRSTNOLIGHT = GLDL_PLAIN,
	GLDL_LASTNOLIGHT = GLDL_FOGMASKED,
};

enum Drawpasses
{
	GLPASS_BASE,		// Draws the untextured surface only
	GLPASS_BASE_MASKED,	// Draws an untextured surface that is masked by the texture
	GLPASS_PLAIN,		// Draws a texture that isn't affected by dynamic lights with sector light settings
	GLPASS_LIGHT,		// Draws dynamic lights
	GLPASS_LIGHT_ADDITIVE,	// Draws additive dynamic lights
	GLPASS_TEXTURE,		// Draws the texture to be modulated with the light information on the base surface
	GLPASS_DECALS,		// Draws a decal
	GLPASS_DECALS_NOFOG,// Draws a decal without setting the fog (used for passes that need a fog layer)
	GLPASS_TRANSLUCENT,	// Draws translucent objects
};

enum WallTypes
{
	RENDERWALL_NONE,
	RENDERWALL_TOP,
	RENDERWALL_M1S,
	RENDERWALL_M2S,
	RENDERWALL_BOTTOM,
	RENDERWALL_SKY,
	RENDERWALL_FOGBOUNDARY,
	RENDERWALL_HORIZON,
	RENDERWALL_SKYBOX,
	RENDERWALL_SECTORSTACK,
	RENDERWALL_PLANEMIRROR,
	RENDERWALL_MIRROR,
	RENDERWALL_MIRRORSURFACE,
	RENDERWALL_M2SNF,
	RENDERWALL_M2SFOG,
	RENDERWALL_COLOR,
	RENDERWALL_FFBLOCK,
	RENDERWALL_COLORLAYER,
	// Insert new types at the end!
};


enum EColorManipulation
{

	CM_INVALID=-1,
	CM_DEFAULT=0,					// untranslated
	CM_DESAT0=CM_DEFAULT,
	CM_DESAT1,					// minimum desaturation
	CM_DESAT31=CM_DESAT1+30,	// maximum desaturation = grayscale
	CM_FIRSTSPECIALCOLORMAP,		// first special fixed colormap

	// special internal values
	CM_GRAY = 0x1000000,		// a simple grayscale map for colorizing blood splats
	CM_ICE	= 0x1000001,		// The bluish ice translation for frozen corpses
	CM_LITE	= 0x1000002,		// special values to handle these items without excessive hacking
	CM_SHADE= 0x1000003,		// alpha channel texture
	CM_TORCH= 0x1000010,		// These are not real color manipulations
};

#include "gl/common/glc_convert.h"
#endif
