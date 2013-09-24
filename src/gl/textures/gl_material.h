
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
	FHardwareTexture * gltexture;
	float scalex;
	float scaley;

	void Clean(bool all)
	{
		if (gltexture) 
		{
			if (!all) gltexture->Clean(false);
			else
			{
				delete gltexture;
				gltexture=NULL;
			}
		}
	}

public:

	float GetU(float upix) const { return gltexture->GetU(upix*scalex); }
	float GetV(float vpix) const { return gltexture->GetV(vpix*scaley); }
		  
	float FloatToTexU(float v) const { return gltexture->FloatToTexU(v*scalex); }
	float FixToTexU(int v) const { return gltexture->FixToTexU(v)*scalex; }
	float FixToTexV(int v) const { return gltexture->FixToTexV(v)*scaley; }
};

class PatchTextureInfo
{
protected:
	FHardwareTexture * glpatch;

	void Clean(bool all)
	{
		if (glpatch) 
		{
			if (!all) glpatch->Clean(false);
			else
			{
				delete glpatch;
				glpatch=NULL;
			}
		}
	}

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


class FGLTexture : protected WorldTextureInfo, protected PatchTextureInfo
{
	friend class FMaterial;
public:
	FTexture * tex;
	FTexture * hirestexture;
	char bIsTransparent;
	int HiresLump;

private:
	int currentwarp;

	bool bHasColorkey;		// only for hires

	short LeftOffset[2];
	short TopOffset[2];
	short Width[2];
	short Height[2];
	short RenderWidth[2];
	short RenderHeight[2];
	float AlphaThreshold;
	fixed_t tempScaleX, tempScaleY;

	unsigned char * LoadHiresTexture(int *width, int *height, int cm);

	BYTE *WarpBuffer(BYTE *buffer, int Width, int Height, int warp);

	const WorldTextureInfo * GetWorldTextureInfo();
	const PatchTextureInfo * GetPatchTextureInfo();

	const WorldTextureInfo * Bind(int texunit, int cm, int clamp, int translation, int warp);
	const PatchTextureInfo * BindPatch(int texunit, int cm, int translation, int warp);

public:
	FGLTexture(FTexture * tx, bool expandpatches);
	~FGLTexture();

	unsigned char * CreateTexBuffer(ETexUse use, int cm, int translation, int & w, int & h, bool allowhires, int warp);

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

	void SetupShader(int shaderindex, int &cm);
	FGLTexture * ValidateSysTexture(FTexture * tex, bool expand);

public:
	FTexture *tex;
	
	FMaterial(FTexture *tex, bool forceexpand);
	~FMaterial();

	const WorldTextureInfo * Bind(int cm, int clamp=0, int translation=0);
	const PatchTextureInfo * BindPatch(int cm, int translation=0);

	const WorldTextureInfo * GetWorldTextureInfo() { return mBaseLayer->GetWorldTextureInfo(); }
	const PatchTextureInfo * GetPatchTextureInfo() { return mBaseLayer->GetPatchTextureInfo(); }

	unsigned char * CreateTexBuffer(ETexUse use, int cm, int translation, int & w, int & h, bool allowhires=true)
	{
		return mBaseLayer->CreateTexBuffer(use, cm, translation, w, h, allowhires, 0);
	}

	void Clean(bool f)
	{
		mBaseLayer->Clean(f);
	}

	void BindToFrameBuffer();
	// Patch drawing utilities

	void GetRect(FloatRect *r, ETexUse i) const;

	void SetWallScaling(fixed_t x, fixed_t y);

	int TextureHeight(ETexUse i) const { return mBaseLayer->RenderHeight[i]; }
	int TextureWidth(ETexUse i) const { return mBaseLayer->RenderWidth[i]; }

	int GetAreaCount() const { return tex->gl_info.areacount; }
	FloatRect *GetAreas() const { return tex->gl_info.areas; }

	fixed_t RowOffset(fixed_t rowoffset) const;
	fixed_t TextureOffset(fixed_t textureoffset) const;

	// Returns the size for which texture offset coordinates are used.
	fixed_t TextureAdjustWidth(ETexUse i) const;

	int GetWidth(ETexUse i) const
	{
		return mBaseLayer->Width[i];
	}

	int GetHeight(ETexUse i) const
	{
		return mBaseLayer->Height[i];
	}

	int GetLeftOffset(ETexUse i) const
	{
		return mBaseLayer->LeftOffset[i];
	}

	int GetTopOffset(ETexUse i) const
	{
		return mBaseLayer->TopOffset[i];
	}

	int GetScaledLeftOffset(ETexUse i) const
	{
		return DivScale16(mBaseLayer->LeftOffset[i], tex->xScale);
	}

	int GetScaledTopOffset(ETexUse i) const
	{
		return DivScale16(mBaseLayer->TopOffset[i], tex->yScale);
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
