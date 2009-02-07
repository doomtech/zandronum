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


enum SectorRenderFlags
{
	// This is used to avoid creating too many drawinfos
	SSRF_RENDERFLOOR=1,
	SSRF_RENDERCEILING=2,
	SSRF_RENDER3DPLANES=4,
	SSRF_RENDERALL=7,
	SSRF_PROCESSED=8,
};

enum EColorManipulation
{
	// special internal values
	CM_BRIGHTMAP=-4,			// Brightness map for colormap based bright colors
	CM_GRAY=-3,					// a simple grayscale map for colorizing blood splats
	CM_ICE=-2,					// The bluish ice translation for frozen corpses

	CM_INVALID=-1,
	CM_DEFAULT=0,					// untranslated
	CM_DESAT0=CM_DEFAULT,
	CM_DESAT1,					// minimum desaturation
	CM_DESAT31=CM_DESAT1+30,	// maximum desaturation = grayscale
	CM_INVERT,					// Doom's invulnerability colormap
	CM_GOLDMAP,					// Heretic's invulnerability colormap
	CM_REDMAP,					// Skulltag's Doomsphere colormap
	CM_GREENMAP,				// Skulltag's Guardsphere colormap
	CM_SHADE,					// alpha channel texture
	CM_LIMIT,					// Max. manipulation value for regular textures. Everything above is for special use.
	CM_FIRSTCOLORMAP=CM_LIMIT,	// Boom colormaps

	CM_LITE=246,				// special values to handle these items without excessive hacking
	CM_TORCH=247,				// These are not real color manipulations
};

#define TO_GL(v) ((float)(v)/FRACUNIT)
#define TO_MAP(f) (fixed_t)quickertoint((f)*FRACUNIT)
#define ANGLE_TO_FLOAT(ang) ((float)((ang) * 180. / ANGLE_180))
#define FLOAT_TO_ANGLE(ang) (angle_t)((ang) / 180. * ANGLE_180)

#define ANGLE_TO_RAD(ang) ((float)((ang) * M_PI / ANGLE_180))
#define RAD_TO_ANGLE(ang) (angle_t)((ang) / M_PI * ANGLE_180)

#define FLOAT_TO_RAD(ang) float((ang) * M_PI / 180.)
#define RAD_TO_FLOAT(ang) float((ang) / M_PI * 180.)

#endif
