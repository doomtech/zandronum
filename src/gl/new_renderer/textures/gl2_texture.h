#ifndef __GL_TEXTURE_H
#define __GL_TEXTURE_H

namespace GLRendererNew
{


	class FHardwareTexture;
	class FGLTextureManager;

class FGLTexture
{
	FTexture *mGameTexture;
	FHardwareTexture *mHWTexture;
	FGLTextureManager *mOwner;
	int mTranslation;
	bool mHasColorKey;
	bool mAsSprite;
	bool mUsingHires;
	bool mCanUseHires;
	char mIsTransparent;

	int HiresTextureLump;
	FTexture *HiresTexture;

	unsigned char *LoadHiresTexture(int *width, int *height);


	void CheckHires();
	

public:
	FGLTexture();
	~FGLTexture();
	bool Create(FTexture *tex, bool asSprite, int translation);
	void Bind(int textureunit, int clamp);
	bool CreateHardwareTexture(unsigned char * buffer, int w, int h, int format);
	void Flush();
	unsigned char * CreateTexBuffer(int translation, int &w, int &h);


};

struct TexManKey
{
	FTexture *tex;
	bool asSprite;
	int translation;

	TexManKey() {}
	TexManKey(FTexture *t, bool assprite, int trans)
	{
		tex = t;
		translation = trans;
		asSprite = assprite;
	}
};

struct TexManHashTraits
{
	// Returns the hash value for a key.
	hash_t Hash(const TexManKey &key) { return (hash_t)(intptr_t)key.tex; }

	// Compares two keys, returning zero if they are the same.
	int Compare(const TexManKey &left, const TexManKey &right) 
	{ 
		return left.tex != right.tex || left.translation != right.translation || left.asSprite != right.asSprite; 
	}
};

typedef TMap<TexManKey, FGLTexture *, TexManHashTraits> FGLTextureMap;

class FGLTextureManager
{
	FGLTextureMap mGLTextures;

public:
	~FGLTextureManager()
	{
		DestroyAllTextures();
	}
	FGLTexture *GetTexture(FTexture *gametex, bool asSprite, int translation);
	void FlushAllTextures();
	void DestroyAllTextures();
};

}
#endif