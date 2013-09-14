#ifndef __GL2_VERTEX_H
#define __GL2_VERTEX_H

#include "tarray.h"

namespace GLRendererNew
{

struct GLVertex3D
{
	float x,y,z;				// coordinates
	float u,v;					// texture coordinates
	unsigned char r,g,b,a;		// light color
	unsigned char fr,fg,fb,fd;	// fog color
	unsigned char tr,tg,tb,td;	// ceiling glow
	unsigned char br,bg,bb,bd;	// floor glow
	float lightfogdensity;
	float lightfactor;
	float glowdisttop;
	float glowdistbottom;
};

struct GLVertex2D
{
	float x,y;
	float u,v;
	unsigned char r,g,b,a;
};

struct GLPrimitive2D
{
	int mPrimitiveType;
	int mTextureMode;
	int mSrcBlend;
	int mDstBlend;
	int mBlendEquation;
	FMaterial *mMaterial;
	bool mUseScissor;
	int mScissor[4];
	int mVertexStart;
};


extern TArray<GLVertex3D> Vertices3D;
extern TArray<GLVertex2D> Vertices2D;
extern TArray<GLPrimitive2D> Primitives2D;

}


#endif