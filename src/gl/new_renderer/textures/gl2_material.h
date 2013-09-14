#ifndef __GL_MATERIAL_H
#define __GL_MATERIAL_H

class FTexture;

namespace GLRendererNew
{

	class FShader;
	class FGLTexture;

struct FRect
{
	float left,top;
	float width,height;


	void Offset(float xofs,float yofs)
	{
		left+=xofs;
		top+=yofs;
	}
	void Scale(float xfac,float yfac)
	{
		left*=xfac;
		width*=xfac;
		top*=yfac;
		height*=yfac;
	}
};


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

	FFPoint mDefaultScale;
	FFPoint mTempScale;

	TArray<FRect> mAreas;	// for optimizing mid texture drawing.
	bool mIsTransparent;

	static int Scale (int val, int scale) ;

public:
	FMaterial(FTexture *tex, bool asSprite, int translation);
	~FMaterial();

	void SetTempScale(float scalex, float scaley);



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
	void CreateDefaultBrightmap();


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


class FMaterialContainer
{
	FTexture *mTexture;
	FMaterial *mMatWorld;
	FMaterial *mMatPatch;
	FMaterialMap *mMatOthers;

public:
	FMaterial *GetWorldMaterial(int translation);
	FMaterial *GetPatchMaterial(int translation);
};


}
#endif