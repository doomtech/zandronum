#ifndef __GL_MATERIAL_H
#define __GL_MATERIAL_H

class FTexture;
#include "tarray.h"
#include "textures/textures.h"

namespace GLRendererNew
{

	class FShader;
	class FGLTexture;



struct FISize
{
	int w,h;
};

struct FFSize
{
	float w,h;
};

struct FIPoint
{
	int x, y;
};

struct FFPoint
{
	float x, y;
};



class FMaterial
{
	struct ShaderParameter
	{
		int mParamType;
		FName mName;
		int mIndex;
		union
		{
			int mIntParam[4];
			float mFloatParam[4];
		};
	};

	TArray<FGLTexture *> mLayers;
	FShader *mShader;
	TArray<ShaderParameter> mParams;

	FISize mSizeTexels;
	FISize mSizeUnits;

	FIPoint mOffsetTexels;
	FIPoint mOffsetUnits;

	float mAlphaThreshold;
	float mSpeed;

	FFPoint mDefaultScale;
	FFPoint mTempScale;

	static int Scale (int val, int scale) ;

public:
	FMaterial(FTexture *tex, bool asSprite, int translation);
	FMaterial();
	~FMaterial();

	void SetTempScale(float scalex, float scaley);

	float GetU(int pos)
	{
		return (float)pos / mSizeUnits.w;
	}

	float GetV(int pos)
	{
		return (float)pos / mSizeUnits.h;
	}

	int GetWidth() const
	{
		return mSizeTexels.w;
	}

	int GetHeight() const
	{
		return mSizeTexels.h;
	}

	int GetScaledWidth() const
	{
		return mSizeUnits.w;
	}

	int GetScaledHeight() const
	{
		return mSizeUnits.h;
	}

	int GetLeftOffset() const
	{
		return mOffsetTexels.x;
	}

	int GetTopOffset() const
	{
		return mOffsetTexels.y;
	}

	int GetScaledLeftOffset() const
	{
		return mOffsetUnits.x;
	}

	int GetScaledTopOffset() const
	{
		return mOffsetUnits.y;
	}

	float RowOffset(float ofs) const;
	float TextureOffset(float ofs) const;

	bool FindHoles(const unsigned char * buffer, int w, int h);
	void CheckTransparent(const unsigned char * buffer, int w, int h);
	void Bind(float *colormap, int texturemode, float desaturation, int clamp);



};


struct MaterialKey
{
	int keyval;

	MaterialKey() {}
	MaterialKey(bool assprite, int trans)
	{
		keyval = (trans<<1) + assprite;
	}
};


struct MaterialHashTraits
{
	// Returns the hash value for a key.
	hash_t Hash(const MaterialKey &key) { return (hash_t)key.keyval; }

	// Compares two keys, returning zero if they are the same.
	int Compare(const MaterialKey &left, const MaterialKey &right) 
	{ 
		return left.keyval != right.keyval;
	}
};


typedef TMap<MaterialKey, FMaterial *, MaterialHashTraits> FMaterialMap;


class FMaterialContainer : public FGLTextureBase
{
	FTexture *mTexture;
	FMaterial *mMatWorld;
	FMaterial *mMatPatch;
	FMaterialMap *mMatOthers;

public:
	FMaterialContainer(FTexture *tex);
	~FMaterialContainer();
	FMaterial *GetMaterial(bool asSprite, int translation);
};


}
#endif