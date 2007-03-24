
#ifndef __GL_TEXTURELIST_H__
#define __GL_TEXTURELIST_H__


#define CR(x) ((x & 0x00ff0000) >> 16)
#define CG(x) ((x & 0x0000ff00) >> 8)
#define CB(x) (x & 0x000000ff)
#define GEN_RGB(r, g, b) ((r << 16) + (g << 8) + b)


#include "r_defs.h"
#include "r_data.h"
#include "tarray.h"


typedef unsigned char BYTE;

typedef struct
{
   char origName[9];
   char newName[9];
} remap_tex_t;


extern TArray<remap_tex_t> RemappedTextures;


int CeilPow2(int num);
void GL_ParseRemappedTextures();


class FDefinedTexture : public FTexture
{
public:
   FDefinedTexture(const char *name, int width, int height);
   ~FDefinedTexture();
   void SetSize(int width, int height);
   const BYTE *GetColumn(unsigned int column, const Span **spans_out) { return NULL; }
   const BYTE *GetPixels() { return NULL; }
   void Unload();
   void Force32Bit(bool force32) { m_32bit = force32; }
   bool Is32bit() { return m_32bit; }
protected:
   bool m_32bit;
};


class TextureListNode
{
public:
   TextureListNode();
   ~TextureListNode();
   TextureListNode *GetTexture(FTexture *tex, BYTE *trans);
   void AddTexture(TextureListNode *texNode);

   FTexture *Tex;
   BYTE *translation;
   unsigned long averageColor;
   bool isTransparent;
   bool isSky;
   bool isAlpha;
   float cx, cy;
   TextureListNode *left;
   TextureListNode *right;
};


class TextureList
{
public:
   TextureList();
   ~TextureList();
   void Init();
   void ShutDown();
   void Purge();
   void Clear();
   void SetupTexparams();
   void UnbindTexture();
   void BindTexture(int textureID, bool translate);
   void BindTexture(const char *texName);
   void BindTexture(FTexture *tex);
   void BindSavegameTexture(int index);
   void GetTexture(int textureID, bool translate);
   void GetTexture(FTexture *tex);
   void BindGLTexture(int texId);
   void LoadAlpha(bool la) { m_loadAsAlpha = la; }
   void LoadSaturate(bool ls) { m_loadSaturate = ls; }
   void GetAverageColor(float *r, float *g, float *b);
   void SetTranslation(const BYTE *trans);
   void SetTranslation(unsigned short trans);
   void SetSavegameTexture(int index, BYTE *img, int width, int height);
   void UpdateForTranslation(BYTE *trans);
   void GrabFB();
   void AllowScale(bool as) { m_allowScale = as; }
   void AllowMipMap(bool mipmap) { m_allowMipMap = mipmap; }
   void GetCorner(float *x, float *y);
   void ForceHQ(int mode) { m_forceHQ = mode; }
   int GetTextureMode();
   int GetTextureModeMag();
   int NumTexUnits() { return m_numTexUnits; }
   unsigned int GetGLTexture();
   bool IsTransparent();
   bool IsLoadingAlpha();
   bool SelectTexUnit(int unit);
   float GetTopYOffset();
   float GetBottomYOffset();
   BYTE *ReduceFramebufferToPalette(int width, int height, bool fullScreen = false);

protected:
   void adjustColor(BYTE *img, int width, int height);
   void colorOutlines(BYTE *img, int width, int height);
   void updateTexture(FTexture *tex);
   void deleteSavegameTexture(int index);
   void addLuminanceNoise(BYTE *buffer, int width, int height);
   int checkExternalFile(char *fileName, BYTE useType, bool isPatch);
   int getTextureFormat(FTexture *tex);
   unsigned long calcAverageColor(BYTE *buffer, int width, int height);
   BYTE *loadExternalFile(char *fileName, BYTE useType, int *width, int *height, bool gameSpecific, bool isPatch);
   BYTE *loadFile(char *fileName, int *width, int *height);
   BYTE *loadFromLump(char *lumpName, int *width, int *height);
   BYTE *scaleImage(BYTE *buffer, int *width, int *height, bool highQuality);
   BYTE *hqresize(BYTE *buffer, int *width, int *height);
   unsigned int loadTexture(FTexture *tex);

   BYTE m_gammaTable[256], *m_translation;
   bool m_initialized, m_supportsAnisotropic, m_supportsS3TC, m_supportsPT, m_loadAsAlpha;
   bool m_loadAsSky, m_loadSaturate, m_allowScale, m_supportsNonPower2, m_allowMipMap;
   int m_maxTextureSize, m_forceHQ, m_numTexUnits, m_currentTexUnit;
   unsigned int *m_lastBoundTexture;
   unsigned short m_translationShort;
   unsigned int m_savegameTex[2];
   FTextureGLData *m_workingData;
};


#endif