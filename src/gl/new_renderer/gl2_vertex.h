#ifndef __GL2_VERTEX_H
#define __GL2_VERTEX_H

#include "tarray.h"

namespace GLRendererNew
{
class FMaterial;

struct FVertex3D
{
	enum {
		FOG_BLACK = 0,
		FOG_COLOR = 255,
		FOG_NONE = 128
	};

	float x,z,y;				// coordinates (note that y and z are switched!)
	float u,v;					// texture coordinates (in world coordinates!)
	unsigned char r,g,b,a;		// light color
	unsigned char fr,fg,fb,fon;	// fog color
	unsigned char tr,tg,tb,td;	// ceiling glow
	unsigned char br,bg,bb,bd;	// floor glow
	float fogdensity;			// fog density
	float lightfactor;			// brightening factor for camera light
	float lightdist;			// brightening distance for camera light
	float glowdisttop;			// distance from glowing ceiling plane
	float glowdistbottom;		// distance from glowing floor plane

	void SetLighting(int lightlevel, FDynamicColormap *cm, int extralight, bool additive, FTexture *tex);
};

struct FPrimitive3D
{
	int mPrimitiveType;
	int mVertexStart;
	int mVertexCount;

	FMaterial *mMaterial;
	int mClamp;
	int mTextureMode;
	int mDesaturation;
	float mAlphaThreshold;
	bool mTranslucent;

	int mSrcBlend;
	int mDstBlend;
	int mBlendEquation;

	void Draw();
	void SetRenderStyle(FRenderStyle style, bool opaque, bool allowcolorblending = false);
};



struct FVertex2D
{
	float x,y;
	float u,v;
	unsigned char r,g,b,a;

	void set(float x, float y, float u, float v, int r, int g, int b, int a)
	{
		this->x = x;
		this->y = y;
		this->u = u;
		this->v = v;
		this->r = (unsigned char)r;
		this->g = (unsigned char)g;
		this->b = (unsigned char)b;
		this->a = (unsigned char)a;
	}

	void set(int x, int y, float u, float v, int r, int g, int b, int a)
	{
		this->x = (float)x;
		this->y = (float)y;
		this->u = u;
		this->v = v;
		this->r = (unsigned char)r;
		this->g = (unsigned char)g;
		this->b = (unsigned char)b;
		this->a = (unsigned char)a;
	}

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
	float mAlphaThreshold;

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
		BUFFER_START = 25000,	// This is what the full console initially needs so it's a good start.
		BUFFER_INCREMENT = 5000,
		BUFFER_MAXIMUM = 200000
	};

	TArray<FPrimitive2D> mPrimitives;
	int mCurrentVertexIndex;
	int mCurrentVertexBufferSize;
	FVertexBuffer2D *mVertexBuffer;

public:
	FPrimitiveBuffer2D();
	~FPrimitiveBuffer2D();
	int NewPrimitive(int vertexcount, FPrimitive2D *&primptr, FVertex2D *&vertptr);
	bool CheckPrimitive(int type, int newvertexcount, FVertex2D *&vertptr);
	void Flush();
};

}


#endif