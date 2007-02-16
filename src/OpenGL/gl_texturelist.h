
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
extern TArray<FDummyTexture> DefinedTextures;


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
   unsigned int glTex;
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
   void BindTexture(int textureID, bool translate);
   void BindTexture(const char *texName);
   void BindTexture(FTexture *tex);
   void BindSavegameTexture();
   void GetTexture(int textureID, bool translate);
   void GetTexture(FTexture *tex);
   void BindGLTexture(int texId);
   void LoadAlpha(bool la);
   void GetAverageColor(float *r, float *g, float *b);
   void SetTranslation(const BYTE *trans);
   void SetTranslation(unsigned short trans);
   void SetSavegameTexture(BYTE *img, int width, int height);
   void UpdateForTranslation(BYTE *trans);
   void GrabFB();
   void AllowScale(bool as);
   void GetCorner(float *x, float *y);
   int GetTextureMode();
   int GetTextureModeMag();
   unsigned int GetGLTexture();
   bool IsTransparent();
   bool IsLoadingAlpha();
   BYTE *ReduceFramebufferToPalette(int width, int height);

protected:
   void colorOutlines(BYTE *img, int width, int height);
   void updateTexture(FTexture *tex);
   void deleteSavegameTexture();
   void addLuminanceNoise(BYTE *buffer, int width, int height);
   int checkExternalFile(char *fileName, bool isPatch);
   int getTextureFormat(FTexture *tex);
   unsigned long calcAverageColor(BYTE *buffer, int width, int height);
   BYTE *loadExternalFile(char *fileName, int *width, int *height, bool gameSpecific, bool isPatch);
   BYTE *loadFile(char *fileName, int *width, int *height);
   BYTE *loadFromLump(char *lumpName, int *width, int *height);
   BYTE *scaleImage(BYTE *buffer, int *width, int *height, bool highQuality);
   BYTE *hqresize(BYTE *buffer, int *width, int *height);
   unsigned int loadTexture(FTexture *tex);

   BYTE m_gammaTable[256], *m_translation;
   bool m_initialized, m_loadAsAlpha, m_loadAsSky, m_allowScale;
   int m_maxTextureSize, m_lastBoundTexture;
   unsigned short m_translationShort;
   unsigned int m_savegameTex;
   FTextureGLData *m_workingData;
};


#endif