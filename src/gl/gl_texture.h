
#ifndef __GL_TEXTURE_H
#define __GL_TEXTURE_H

#include "gl/gltexture.h"
#include "r_data.h"
#include "i_system.h"
#include "textures/bitmap.h"

EXTERN_CVAR(Bool, gl_precache)
EXTERN_CVAR(Bool, gl_brightmap_shader)

struct GL_RECT;

void ModifyPalette(PalEntry * pout, PalEntry * pin, int cm, int count);
void CopyColorsRGBA(unsigned char * pout, const unsigned char * pin, int cm, int count, int step);

class FGLBitmap : public FBitmap
{
	int cm;
	int translation;
public:

	FGLBitmap() { cm = CM_DEFAULT; translation = 0; }
	FGLBitmap(BYTE *buffer, int pitch, int width, int height)
		: FBitmap(buffer, pitch, width, height)
	{ cm = CM_DEFAULT; translation = 0; }

	void SetTranslationInfo(int _cm, int _trans=-1337)
	{
		if (_cm != -1) cm = _cm;
		if (_trans != -1337) translation = _trans;

	}

	virtual void CopyPixelDataRGB(int originx, int originy, const BYTE *patch, int srcwidth, 
								int srcheight, int step_x, int step_y, int rotate, int ct, FCopyInfo *inf = NULL);
	virtual void CopyPixelData(int originx, int originy, const BYTE * patch, int srcwidth, int srcheight, 
								int step_x, int step_y, int rotate, PalEntry * palette, FCopyInfo *inf = NULL);
};


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
	GLTexture * gltexture;
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
	GLTexture * glpatch;

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
struct FRemapTable;

class FGLTexture : protected WorldTextureInfo, protected PatchTextureInfo
{
	friend void AdjustSpriteOffsets();

	static TArray<FGLTexture *> gltextures;
public:
	enum ETexUse
	{
		GLUSE_PATCH,
		GLUSE_TEXTURE,
	};

	FTexture * tex;
	FTexture * hirestexture;
	char bIsTransparent;
	bool createWarped;
	int HiresLump;

private:
	int index;

	signed char areacount;
	bool bHasColorkey;		// only for hires
	GL_RECT * areas;

	short LeftOffset[2];
	short TopOffset[2];
	short Width[2];
	short Height[2];
	short RenderWidth[2];
	short RenderHeight[2];
	float AlphaThreshold;

	bool FindHoles(const unsigned char * buffer, int w, int h);
	bool ProcessData(unsigned char * buffer, int w, int h, int cm, bool ispatch);
	void CheckTrans(unsigned char * buffer, int size, int trans);
	static bool SmoothEdges(unsigned char * buffer,int w, int h, bool clampsides);
	int CheckDDPK3();
	int CheckExternalFile(bool & hascolorkey);
	unsigned char * LoadHiresTexture(int *width, int *height, int cm);

	BYTE *WarpBuffer(BYTE *buffer, int Width, int Height, int warp);

	void CreateDefaultBrightmap();
	void CheckForAlpha(const unsigned char * buffer);
	void SetupShader(int clampmode, int warped, int &cm, int translation);

	const WorldTextureInfo * Bind(int texunit, int cm, int clamp, int translation);
	const PatchTextureInfo * BindPatch(int texunit, int cm, int translation);

public:
	FGLTexture(FTexture * tx);
	~FGLTexture();

	unsigned char * CreateTexBuffer(ETexUse use, int cm, int translation, int & w, int & h, bool allowhires=true);
	const WorldTextureInfo * Bind(int cm, int clamp=0, int translation=0);
	const PatchTextureInfo * BindPatch(int cm, int translation=0);

	const WorldTextureInfo * GetWorldTextureInfo();
	const PatchTextureInfo * GetPatchTextureInfo();

	void Clean(bool all);

	static void FlushAll();
	static void DeleteAll();
	static FGLTexture * ValidateTexture(FTexture * tex);
	static FGLTexture * ValidateTexture(FTextureID no, bool translate=true);

	// Patch drawing utilities

	void GetRect(GL_RECT * r, ETexUse i) const;

	int TextureHeight(ETexUse i) const { return RenderHeight[i]; }
	int TextureWidth(ETexUse i) const { return RenderWidth[i]; }

	int GetAreaCount() const { return areacount; }
	GL_RECT * GetAreas() const { return areas; }

	fixed_t RowOffset(fixed_t rowoffset) const
	{
		if (scaley==1.f || tex->bWorldPanning) return rowoffset;
		else return quickertoint(rowoffset/scaley);
	}

	fixed_t TextureOffset(fixed_t textureoffset) const
	{
		if (scalex==1.f || tex->bWorldPanning) return textureoffset;
		else return quickertoint(textureoffset/scalex);
	}

	// Returns the size for which texture offset coordinates are used.
	fixed_t TextureAdjustWidth(ETexUse i) const
	{
		if (tex->bWorldPanning) return RenderWidth[i];
		else return Width[i];
	}

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

	int GetIndex() const
	{
		return index;
	}

	bool GetTransparent()
	{
		if (bIsTransparent == -1) 
		{
			if (tex->UseType==FTexture::TEX_Sprite) BindPatch(CM_DEFAULT, 0, true);
			else Bind (CM_DEFAULT, 0, 0, true);
		}
		return !!bIsTransparent;
	}

};


class FBrightmapTexture : public FTexture
{
	friend class FGLTexture;

public:
	FBrightmapTexture (FTexture *source);
	~FBrightmapTexture ();

	const BYTE *GetColumn (unsigned int column, const Span **spans_out);
	const BYTE *GetPixels ();
	void Unload ();

	int CopyTrueColorPixels(FBitmap *bmp, int x, int y, int rotate, FCopyInfo *inf);
	bool UseBasePalette() { return false; }

protected:
	FTexture *SourcePic;
	//BYTE *Pixels;
	//Span **Spans;
};

void gl_EnableTexture(bool on);
void gl_GenerateGlobalBrightmapFromColormap();

#endif
