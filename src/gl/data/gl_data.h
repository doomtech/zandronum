
#ifndef __GLC_DATA_H
#define __GLC_DATA_H

#include "doomtype.h"

struct GLRenderSettings
{

	SBYTE lightmode;
	bool nocoloredspritelighting;

	SBYTE map_lightmode;
	SBYTE map_nocoloredspritelighting;

	FVector3 skyrotatevector;

};

extern GLRenderSettings glset;

#include "r_defs.h"

void gl_RecalcVertexHeights(vertex_t * v);
FTextureID gl_GetSpriteFrame(unsigned sprite, int frame, int rot, angle_t ang, bool *mirror);

#endif
