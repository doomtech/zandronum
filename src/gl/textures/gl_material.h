
#ifndef __GL_TEXTURE_H
#define __GL_TEXTURE_H

#include "gl/textures/gl_hwtexture.h"
#include "gl/renderer/gl_colormap.h"
#include "r_data.h"
#include "i_system.h"

EXTERN_CVAR(Bool, gl_precache)

struct FRemapTable;
class FTextureShader;


// Two intermediate classes which wrap the low level textures.
// These ones are returned by the Bind* functions to ensure
// that the coordinate functions aren't used without the texture
// being initialized.
// Unfortunately it is necessary to maintain 2 of these.
// On older graphics cards which don't support non-power of 2 textures
// these are not interchangable so if a texture happens to be used
// both as sprite and texture there need to be different versions.

class WorldTextureInfo
{
	friend class FMaterial;
protected:
	const FHardwareTexture * gltexture;
	float scalex;
	float scaley;

public:

	float GetU(float upix) const { return gltexture->GetU(upix*scalex); }
	float GetV(float vpix) const { return gltexture->GetV(vpix*scaley); }
		  
	float FloatToTexU(float v) const { return gltexture->FloatToTexU(v*scalex); }
	float FloatToTexV(float v) const { return gltexture->FloatToTexV(v*scaley); }
};

class PatchTextureInfo
{
	friend class FMaterial;
protected:
	const FHardwareTexture * glpatch;
	float SpriteU[2], SpriteV[2];

public:
	float GetUL() const { return glpatch->GetUL(); }
	float GetVT() const { return glpatch->GetVT(); }
	float GetUR() const { return glpatch->GetUR(); }
	float GetVB() const { return glpatch->GetVB(); }
	float GetU(float upix) const { return glpatch->GetU(upix); }
	float GetV(float vpix) const { return glpatch->GetV(vpix); }

	float GetSpriteUL() const { return SpriteU[0]; }
	float GetSpriteVT() const { return SpriteV[0]; }
	float GetSpriteUR() const { return SpriteU[1]; }
	float GetSpriteVB() const { return SpriteV[1]; }
};


//===========================================================================
// 
// this is the texture maintenance class for OpenGL. 
//
//===========================================================================
class FMaterial;

enum ETexUse
{
	GLUSE_PATCH,
	GLUSE_TEXTURE,
	GLUSE_SPRITE,
};


class FGLTexture //: protected WorldTextureInfo, protected PatchTextureInfo
{
	friend class FMaterial;
public:
	FTexture * tex;
	FTexture * hirestexture;
	char bIsTransparent;
	int HiresLump;

private:
	FHardwareTexture *gltexture[5];
	FHardwareTexture *glpatch;

	int currentwarp;
	int currentwarptime;

	bool bHasColorkey;		// only for hires
	bool bExpand;
	float AlphaThreshold;

	unsigned char * LoadHiresTexture(FTexture *hirescheck, int *width, int *height, int cm);
	BYTE *WarpBuffer(BYTE *buffer, int Width, int Height, int warp);

	FHardwareTexture *CreateTexture(int clampmode);
	//bool CreateTexture();
	bool CreatePatch();

	const FHardwareTexture *Bind(int texunit, int cm, int clamp, int translation, FTexture *hirescheck, int warp);
	const FHardwareTexture *BindPatch(int texunit, int cm, int translation, int warp);

public:
	FGLTexture(FTexture * tx, bool expandpatches);
	~FGLTexture();

	unsigned char * CreateTexBuffer(int cm, int translation, int & w, int & h, bool expand, FTexture *hirescheck, int warp);

	void Clean(bool all);
	int Dump(int i);

};

//===========================================================================
// 
// this is the material class for OpenGL. 
//
//===========================================================================

class FMaterial
{
	struct FTextureLayer
	{
		FTexture *texture;
		bool animated;
	};

	static TArray<FMaterial *> mMaterials;
	static int mMaxBound;

	FGLTexture *mBaseLayer;	
	TArray<FTextureLayer> mTextureLayers;
	int mShaderIndex;

	WorldTextureInfo wti;
	PatchTextureInfo pti;
	short LeftOffset[3];
	short TopOffset[3];
	short Width[3];
	short Height[3];
	short RenderWidth[2];
	short RenderHeight[2];
	fixed_t tempScaleX, tempScaleY;



	void SetupShader(int shaderindex, int &cm);
	FGLTexture * ValidateSysTexture(FTexture * tex, bool expand);
	bool TrimBorders(int *rect);

public:
	FTexture *tex;
	
	FMaterial(FTexture *tex, bool forceexpand);
	~FMaterial();
	void Precache();
	bool isMasked() const
	{
		return !!mBaseLayer->tex->bMasked;
	}

	const WorldTextureInfo * Bind(int cm, int clamp=0, int translation=0);
	const PatchTextureInfo * BindPatch(int cm, int translation=0);

	const WorldTextureInfo * GetWorldTextureInfo();// { return mBaseLayer->GetWorldTextureInfo(); }
	const PatchTextureInfo * GetPatchTextureInfo();// { return mBaseLayer->GetPatchTextureInfo(); }

	unsigned char * CreateTexBuffer(int cm, int translation, int & w, int & h, bool expand = false, bool allowhires=true)
	{
		return mBaseLayer->CreateTexBuffer(cm, translation, w, h, expand, allowhires? tex:NULL, 0);
	}

	void Clean(bool f)
	{
		mBaseLayer->Clean(f);
	}

	void BindToFrameBuffer();
	// Patch drawing utilities

	void GetRect(FloatRect *r, ETexUse i) const;

	void SetWallScaling(fixed_t x, fixed_t y);

	// This is scaled size in integer units as needed by walls and flats
	int TextureHeight(ETexUse i) const { return RenderHeight[i]; }
	int TextureWidth(ETexUse i) const { return RenderWidth[i]; }

	int GetAreas(FloatRect **pAreas) const;

	fixed_t TextureOffset(fixed_t textureoffset) const;
	float TextureOffset(float textureoffset) const;

	fixed_t RowOffset(fixed_t rowoffset) const;
	float RowOffset(float rowoffset) const;

	// Returns the size for which texture offset coordinates are used.
	fixed_t TextureAdjustWidth(ETexUse i) const;

	int GetWidth(ETexUse i) const
	{
		return Width[i];
	}

	int GetHeight(ETexUse i) const
	{
		return Height[i];
	}

	int GetLeftOffset(ETexUse i) const
	{
		return LeftOffset[i];
	}

	int GetTopOffset(ETexUse i) const
	{
		return TopOffset[i];
	}

	int GetScaledLeftOffset(ETexUse i) const
	{
		return DivScale16(LeftOffset[i], tex->xScale);
	}

	int GetScaledTopOffset(ETexUse i) const
	{
		return DivScale16(TopOffset[i], tex->yScale);
	}

	float GetScaledLeftOffsetFloat(ETexUse i) const
	{
		return LeftOffset[i] / FIXED2FLOAT(tex->xScale);
	}

	float GetScaledTopOffsetFloat(ETexUse i) const
	{
		return TopOffset[i] / FIXED2FLOAT(tex->yScale);
	}

	// This is scaled size in floating point as needed by sprites
	float GetScaledWidthFloat(ETexUse i) const
	{
		return Width[i] / FIXED2FLOAT(tex->xScale);
	}

	float GetScaledHeightFloat(ETexUse i) const
	{
		return Height[i] / FIXED2FLOAT(tex->yScale);
	}

	bool GetTransparent()
	{
		if (mBaseLayer->bIsTransparent == -1) 
		{
			if (tex->UseType==FTexture::TEX_Sprite) BindPatch(CM_DEFAULT, 0);
			else Bind (CM_DEFAULT, 0, 0);
		}
		return !!mBaseLayer->bIsTransparent;
	}

	static void DeleteAll();
	static void FlushAll();
	static FMaterial *ValidateTexture(FTexture * tex);
	static FMaterial *ValidateTexture(FTextureID no, bool trans);

};

#endif
