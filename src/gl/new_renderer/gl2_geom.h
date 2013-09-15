
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
	FMaterial *mMat;	// this is needed for sorting geometry on a higher level than single primitives
	bool mAlpha;
};


//==========================================================================
//
// Flats 
//
//==========================================================================

struct FSubsectorPrimitive
{
	int mSubsector;
	FPrimitive3D mPrimitive;
};

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
		if (upside) return mPlane.ZatPoint(viewpoint.X, viewpoint.Y) > viewpoint.Z;
		else return mPlane.ZatPoint(viewpoint.X, viewpoint.Y) < viewpoint.Z;
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
	TArray<FSubsectorPrimitive> mPrimitives;

	void CreatePlanePrimitives(GLDrawInfo *di, FSectorPlaneObject *plane, FPrimitiveBuffer3D *buffer);

	void CreateDynamicPrimitive(FSectorPlaneObject *plane, FVertex3D *modelvert, FVertex3D *verts, subsector_t *sub);

	void CreatePlane(FSectorPlaneObject *plane,
					 int in_area, sector_t *sec, GLSectorPlane &splane, 
					 int lightlevel, FDynamicColormap *cm, 
					 float alpha, bool additive, bool upside, bool opaque);

	void Init(int sector);
	void Validate(area_t in_area);
	void Process(subsector_t *sub, area_t in_area);
};



}


#endif