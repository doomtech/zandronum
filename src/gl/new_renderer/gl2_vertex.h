#ifndef __GL2_VERTEX_H
#define __GL2_VERTEX_H

#include "tarray.h"

namespace GLRendererNew
{
class FMaterial;

struct FVertex3D
{
	float x,y,z;				// coordinates
	float u,v;					// texture coordinates
	unsigned char r,g,b,a;		// light color
	unsigned char fr,fg,fb,fd;	// fog color
	unsigned char tr,tg,tb,td;	// ceiling glow
	unsigned char br,bg,bb,bd;	// floor glow
	float lighton;
	float lightfogdensity;
	float lightfactor;
	float lightdist;
	float glowdisttop;
	float glowdistbottom;
};

struct FVertex2D
{
	float x,y;
	float u,v;
	unsigned char r,g,b,a;
};

class FVertexBuffer
{
	static unsigned int mLastBound;	// Crappy GL state machine. :(
protected:
	unsigned int mBufferId;
	void *mMap;

	FVertexBuffer();
	void BindBuffer();
public:
	~FVertexBuffer();
	void Map();
	bool Unmap();
};

class FVertexBuffer2D : public FVertexBuffer
{
	int mMaxSize;
public:
	FVertexBuffer2D(int size);
	bool Bind();

	int GetSize() const
	{
		return mMaxSize;
	}

	void ChangeSize(int newsize)
	{
		mMaxSize = newsize;
	}

	FVertex2D *GetVertexPointer(int vt) const
	{
		return ((FVertex2D*)mMap)+vt;
	}
};

struct FPrimitive2D
{
	int mPrimitiveType;
	int mVertexStart;
	int mVertexCount;

	FMaterial *mMaterial;
	int mTextureMode;

	int mSrcBlend;
	int mDstBlend;
	int mBlendEquation;

	bool mUseScissor;
	int mScissor[4];

	void Draw();
};

class FPrimitiveBuffer2D
{
	enum
	{
		BUFFER_INCREMENT = 1000,
		BUFFER_MAXIMUM = 20000
	};

	TArray<FPrimitive2D> mPrimitives;
	int mCurrentVertexIndex;
	int mCurrentVertexBufferSize;
	FVertexBuffer2D *mVertexBuffer;

public:
	FPrimitiveBuffer2D();
	~FPrimitiveBuffer2D();
	int NewPrimitive(int vertexcount, FPrimitive2D *&primptr, FVertex2D *&vertptr);
	void Flush();
};

}


#endif