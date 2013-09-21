
#ifndef __GL_TEXTURE_H
#define __GL_TEXTURE_H

#include "gl/textures/gl_hwtexture.h"
#include "gl/renderer/gl_colormap.h"
#include "r_data.h"
#include "i_system.h"

EXTERN_CVAR(Bool, gl_precache)

struct FRemapTable;


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
	static TArray<FMaterial *> mMaterials;
	TArray<FGLTexture *> mTextures;
	int mShaderIndex;
	int mMaxBound;

	void SetupShader(int shaderindex, int &cm);
	FGLTexture * ValidateSysTexture(FTexture * tex, bool expand);

public:
	FTexture *tex;
	
	FMaterial(FTexture *tex);
	~FMaterial();

	const WorldTextureInfo * Bind(int cm, int clamp=0, int translation=0);
	const PatchTextureInfo * BindPatch(int cm, int translation=0);

	const WorldTextureInfo * GetWorldTextureInfo() { return mTextures[0]->GetWorldTextureInfo(); }
	const PatchTextureInfo * GetPatchTextureInfo() { return mTextures[0]->GetPatchTextureInfo(); }

	unsigned char * CreateTexBuffer(ETexUse use, int cm, int translation, int & w, int & h, bool allowhires=true)
	{
		return mTextures[0]->CreateTexBuffer(use, cm, translation, w, h, allowhires, 0);
	}

	void Clean(bool f)
	{
		for(unsigned i=0;i<mTextures.Size();i++)
		{
			mTextures[i]->Clean(f);
		}
	}

	// Patch drawing utilities

	void GetRect(FloatRect *r, ETexUse i) const;

	void SetWallScaling(fixed_t x, fixed_t y);

	int TextureHeight(ETexUse i) const { return mTextures[0]->RenderHeight[i]; }
	int TextureWidth(ETexUse i) const { return mTextures[0]->RenderWidth[i]; }

	int GetAreaCount() const { return tex->gl_info.areacount; }
	FloatRect *GetAreas() const { return tex->gl_info.areas; }

	fixed_t RowOffset(fixed_t rowoffset) const;
	fixed_t TextureOffset(fixed_t textureoffset) const;

	// Returns the size for which texture offset coordinates are used.
	fixed_t TextureAdjustWidth(ETexUse i) const;

	int GetWidth(ETexUse i) const
	{
		return mTextures[0]->Width[i];
	}

	int GetHeight(ETexUse i) const
	{
		return mTextures[0]->Height[i];
	}

	int GetLeftOffset(ETexUse i) const
	{
		return mTextures[0]->LeftOffset[i];
	}

	int GetTopOffset(ETexUse i) const
	{
		return mTextures[0]->TopOffset[i];
	}

	int GetScaledLeftOffset(ETexUse i) const
	{
		return DivScale16(mTextures[0]->LeftOffset[i], tex->xScale);
	}

	int GetScaledTopOffset(ETexUse i) const
	{
		return DivScale16(mTextures[0]->TopOffset[i], tex->yScale);
	}

	bool GetTransparent()
	{
		if (mTextures[0]->bIsTransparent == -1) 
		{
			if (tex->UseType==FTexture::TEX_Sprite) BindPatch(CM_DEFAULT, 0);
			else Bind (CM_DEFAULT, 0, 0);
		}
		return !!mTextures[0]->bIsTransparent;
	}

	static void DeleteAll();
	static void FlushAll();
	static FMaterial *ValidateTexture(FTexture * tex);
	static FMaterial *ValidateTexture(FTextureID no, bool trans);

};


void gl_EnableTexture(bool on);

#endif
