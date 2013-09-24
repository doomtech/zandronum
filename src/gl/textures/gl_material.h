
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

public:
	float GetUL() const { return glpatch->GetUL(); }
	float GetVT() const { return glpatch->GetVT(); }
	float GetUR() const { return glpatch->GetUR(); }
	float GetVB() const { return glpatch->GetVB(); }
	float GetU(float upix) const { return glpatch->GetU(upix); }
	float GetV(float vpix) const { return glpatch->GetV(vpix); }
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

	bool bHasColorkey;		// only for hires
	bool bExpand;
	float AlphaThreshold;

	unsigned char * LoadHiresTexture(int *width, int *height, int cm);
	BYTE *WarpBuffer(BYTE *buffer, int Width, int Height, int warp);

	FHardwareTexture *CreateTexture(int clampmode);
	//bool CreateTexture();
	bool CreatePatch();

	const FHardwareTexture *Bind(int texunit, int cm, int clamp, int translation, bool allowhires, int warp);
	const FHardwareTexture *BindPatch(int texunit, int cm, int translation, int warp);

public:
	FGLTexture(FTexture * tx, bool expandpatches);
	~FGLTexture();

	unsigned char * CreateTexBuffer(int cm, int translation, int & w, int & h, bool expand, bool allowhires, int warp);

	void Clean(bool all);

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
	short LeftOffset[2];
	short TopOffset[2];
	short Width[2];
	short Height[2];
	short RenderWidth[2];
	short RenderHeight[2];
	fixed_t tempScaleX, tempScaleY;



	void SetupShader(int shaderindex, int &cm);
	FGLTexture * ValidateSysTexture(FTexture * tex, bool expand);

public:
	FTexture *tex;
	
	FMaterial(FTexture *tex, bool forceexpand);
	~FMaterial();
	void Precache();

	const WorldTextureInfo * Bind(int cm, int clamp=0, int translation=0);
	const PatchTextureInfo * BindPatch(int cm, int translation=0);

	const WorldTextureInfo * GetWorldTextureInfo();// { return mBaseLayer->GetWorldTextureInfo(); }
	const PatchTextureInfo * GetPatchTextureInfo();// { return mBaseLayer->GetPatchTextureInfo(); }

	unsigned char * CreateTexBuffer(int cm, int translation, int & w, int & h, bool expand = false, bool allowhires=true)
	{
		return mBaseLayer->CreateTexBuffer(cm, translation, w, h, expand, allowhires, 0);
	}

	void Clean(bool f)
	{
		mBaseLayer->Clean(f);
	}

	void BindToFrameBuffer();
	// Patch drawing utilities

	void GetRect(FloatRect *r, ETexUse i) const;

	void SetWallScaling(fixed_t x, fixed_t y);

	int TextureHeight(ETexUse i) const { return RenderHeight[i]; }
	int TextureWidth(ETexUse i) const { return RenderWidth[i]; }

	int GetAreaCount() const { return tex->gl_info.areacount; }
	FloatRect *GetAreas() const { return tex->gl_info.areas; }

	fixed_t RowOffset(fixed_t rowoffset) const;
	fixed_t TextureOffset(fixed_t textureoffset) const;

	float FMaterial::RowOffset(float rowoffset) const;

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
