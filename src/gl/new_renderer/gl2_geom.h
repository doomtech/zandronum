
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

struct FSectorPlaneObject : public FRenderObject
{
	sector_t *mSector;
	secplane_t mPlane;
	bool mUpside;
	bool mLightEffect;		// do not render when a fullbright effect is on
	int mPrimitiveIndex;
	int mPlaneType;

	bool isVisible(const FVector3 &viewpoint, bool upside)	// upside must be passed for optimization
	{
		float planez = mPlane.ZatPoint(viewpoint.X, viewpoint.Y);
		if (!upside) return planez > viewpoint.Z;
		else return planez < viewpoint.Z;
	}
};

struct FSectorAreaData
{
	bool valid;
	FSectorPlaneObject mCeiling;
	FSectorPlaneObject mFloor;
	TArray<FSectorPlaneObject> m3DFloorsC;
	TArray<FSectorPlaneObject> m3DFloorsF;
};

struct FSectorRenderData
{
	sector_t *mSector;
	FSectorAreaData mAreas[3];
	TArray<FVertex3D> mVertices;
	TArray<FPrimitive3D> mPrimitives;

	void CreatePrimitives(GLDrawInfo *di, FSectorPlaneObject *plane);

	void CreateDynamicPrimitive(FSectorPlaneObject *plane, FVertex3D *verts, subsector_t *sub);

	void CreatePlane(FSectorPlaneObject *plane,
					 int in_area, sector_t *sec, GLSectorPlane &splane, 
					 int lightlevel, FDynamicColormap *cm, 
					 float alpha, bool additive, bool upside, bool opaque);

	void Init(int sector);
	void Validate(area_t in_area);
	void Process(subsector_t *sub, area_t in_area);
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