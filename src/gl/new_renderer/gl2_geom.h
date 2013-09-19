
#ifndef __GL2_GEOM_H
#define __GL2_GEOM_H


#include "gl/common/glc_structs.h"
#include "gl2_vertex.h"

struct FDynamicColormap;

namespace GLRendererNew
{
	struct GLDrawInfo;

struct FRenderObject
{
	enum EType
	{
		RO_FLAT,
		RO_WALL,
		RO_SPRITE
	};
	FMaterial *mMat;	// this is needed for sorting geometry on a higher level than single primitives
	EType mType;
	bool mAlpha;
};


//==========================================================================
//
// Flats 
//
//==========================================================================

struct FWallData
{
	vertex_t *v[2];
	fixed_t fch1, fch2, ffh1, ffh2;
	fixed_t bch1, bch2, bfh1, bfh2;
	FTextureID bottomflat, topflat;
	int lightlevel, rellight;
	bool foggy;

};

struct FWallObject : public FRenderObject
{
	seg_t *mSeg;
	side_t *mSide;
	int mTier;
	bool mPolyobj;
	bool mHackSeg;
	bool mFogBoundary;
	bool mLightEffect;		// do not render when a fullbright effect is on
	int mPrimitiveCount;
	int mPrimitiveIndex;

};

struct FWallAreaData
{
	bool valid;
	FWallObject mUpper;
	FWallObject mMiddle;
	FWallObject mLower;
	FWallObject mFogBoundary;	// this must be separate because it can overlay mMiddle
	TArray<FWallObject> m3DFloorParts;
};

struct FWallRenderData
{
	side_t *mSide;
	subsector_t *mSub;	// this is only for polyobjects to get the light lists
	FWallAreaData mAreas[3];
	TArray<FVertex3D> mVertices;
	TArray<FPrimitive3D> mPrimitives;

	void CreatePrimitives(GLDrawInfo *di, FWallObject *plane);
	void Init(int sector);
	void Validate(seg_t *seg, sector_t *fs, sector_t *bs, subsector_t *polysub, area_t in_area);
	void Process(seg_t *seg, sector_t *fs, sector_t *bs, subsector_t *polysub, area_t in_area);
};



}


#endif