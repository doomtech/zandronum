
#ifndef __GLC_DATA_H
#define __GLC_DATA_H

#include "doomtype.h"

struct GLRenderSettings
{

	SBYTE lightmode;
	bool nocoloredspritelighting;
	bool notexturefill;

	SBYTE map_lightmode;
	SBYTE map_nocoloredspritelighting;
	SBYTE map_notexturefill;

	FVector3 skyrotatevector;
	FVector3 skyrotatevector2;

};

extern GLRenderSettings glset;

#include "r_defs.h"

void gl_RecalcVertexHeights(vertex_t * v);
FTextureID gl_GetSpriteFrame(unsigned sprite, int frame, int rot, angle_t ang, bool *mirror);

class AStackPoint;

struct FPortal
{
	TArray<vertex_t *> Shape;	// forms the smallest convex outline around the portal area
	TArray<angle_t> ClipAngles;
	fixed_t xDisplacement;
	fixed_t yDisplacement;
	int plane;
	AStackPoint *origin;

	int PointOnShapeLineSide(fixed_t x, fixed_t y, int shapeindex);
	void AddVertexToShape(vertex_t *vertex);
	void AddSectorToPortal(sector_t *sector);
	void UpdateClipAngles();
};


extern TArray<FPortal> portals;


#endif
