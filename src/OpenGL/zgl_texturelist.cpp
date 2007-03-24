/*
** gl_texturelist.cpp
**
** Implementation of the TextureList class.  Wrapper class for loading
** and tracking textures, flats, and patches.
**
**---------------------------------------------------------------------------
** Copyright 2003 Tim Stump
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#include <windows.h>
#include <gl/gl.h>
#include <gl/glu.h>
#include <il/il.h>
#include <il/ilu.h>
#include <string.h>
#ifndef unix
#include <io.h>
#endif

#define USE_WINDOWS_DWORD
#include "OpenGLVideo.h"
#include "zgl_texturelist.h"
#include "zgl_main.h"
#include "Image.h"

#include "c_console.h"
#include "c_cvars.h"
#include "c_dispatch.h"
#include "i_system.h"
#include "r_defs.h"
#include "r_draw.h"
#include "r_sky.h"
#include "r_state.h"
#include "sc_man.h"
#include "templates.h"
#include "v_palette.h"
#include "v_font.h"
#include "v_video.h"
#include "w_wad.h"

#include "gi.h"


#if 0
   #define LOG(x) { FILE *f; f = fopen("texlist.log", "a"); fprintf(f, x); fflush(f); fclose(f); }
   #define LOG1(x, a1) { FILE *f; f = fopen("texlist.log", "a"); fprintf(f, x, a1); fflush(f); fclose(f); }
   #define LOG2(x, a1, a2) { FILE *f; f = fopen("texlist.log", "a"); fprintf(f, x, a1, a2); fflush(f); fclose(f); }
   #define LOG3(x, a1, a2, a3) { FILE *f; f = fopen("texlist.log", "a"); fprintf(f, x, a1, a2, a3); fflush(f); fclose(f); }
#else
   #define LOG(x)
   #define LOG1(x, a1)
   #define LOG2(x, a1, a2)
   #define LOG3(x, a1, a2, a3)
#endif


#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#define GL_GENERATE_MIPMAP_SGIS 0x8191
#define COMPRESSED_RGBA_S3TC_DXT1_EXT	0x83F1
#define COMPRESSED_RGBA_S3TC_DXT3_EXT	0x83F2
#define COMPRESSED_RGBA_S3TC_DXT5_EXT	0x83F3


TextureList textureList;
TArray<remap_tex_t> RemappedTextures;
TArray<FTexture *> TrackedTextures;

extern level_locals_t level;
extern GameMission_t	gamemission;
extern FColorMatcher ColorMatcher;
extern int ST_Y;

int TextureMode;


EXTERN_CVAR (Float, Gamma)
EXTERN_CVAR (Bool, gl_wireframe)
EXTERN_CVAR (Bool, gl_texture)

CVAR(Bool, gl_texture_anisotropic, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Float, gl_texture_anisotropy_degree, 2.0f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Bool, gl_sprite_precache, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

CUSTOM_CVAR(Int, gl_texture_camtex_format, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
   textureList.Purge();
}

CUSTOM_CVAR(Int, gl_texture_hqresize, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
   textureList.Purge();
}

CUSTOM_CVAR(String, gl_texture_mode, "GL_LINEAR_MIPMAP_LINEAR", CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
   TextureMode = textureList.GetTextureMode();
   textureList.Purge();
}

CUSTOM_CVAR(Bool, gl_texture_usehires, true, CVAR_ARCHIVE | CVAR_NOINITCALL)
{
   textureList.Purge();
}

CUSTOM_CVAR(Float, gl_texture_color_multiplier, 1.0f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
   textureList.Purge();
}

CUSTOM_CVAR(String, gl_texture_format, "GL_RGB5_A1", CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
   textureList.Purge();
}

CCMD (gl_purge_textures)
{
	textureList.Purge();
}

CCMD (gl_list_remaps)
{
   unsigned int i;
   int lumpNum;
   remap_tex_t *remap;

   Printf("Listing %d remapped textures.\n", RemappedTextures.Size());
   for (i = 0; i < RemappedTextures.Size(); i++)
   {
      remap = &RemappedTextures[i];
      //lumpNum = Wads.CheckNumForName(remap->newName);
      lumpNum = 0;
      lumpNum = Wads.FindLump(remap->newName, &lumpNum);
      Printf("%s -> %s (%s)\n", remap->origName, remap->newName, lumpNum != -1 ? "ok" : "not found");
   }
}


extern FPalette GPalette;
void InitLUTs();


int CeilPow2(int num)
{
	int cumul;
	for(cumul=1; num > cumul; cumul <<= 1);
	return cumul;
}


int FloorPow2(int num)
{
	int fl = CeilPow2(num);
	if(fl > num) fl >>= 1;
	return fl;
}


inline int RoundPow2(int num)
{
	int cp2 = CeilPow2(num), fp2 = FloorPow2(num);
	return (cp2-num >= num-fp2)? fp2 : cp2;
}


void GL_PrecacheAnimdef(FAnimDef *anim, float priority)
{
   unsigned short texNum, i;
   FTexture *tex;

   for (i = 0; i < anim->NumFrames; i++)
   {
      if (0)//(anim->bUniqueFrames)
      {
         texNum = anim->Frames[i].FramePic;
      }
      else
      {
         texNum = anim->BasePic + i;
      }
      tex = TexMan[texNum];
      if (tex && tex->UseType != FTexture::TEX_Null)
      {
         tex->glData.Clear();
         textureList.BindTexture(tex);
      }
   }
}

void GL_PrecacheShader(FShader *shader, float priority)
{
   FShaderLayer *layer;
   FTexture *tex;
   unsigned int i;

   for (i = 0; i < shader->layers.Size(); i++)
   {
      layer = shader->layers[i];
      tex = TexMan[layer->name];
      if (tex && tex->UseType != FTexture::TEX_Null)
      {
         tex->glData.Clear();
         textureList.GetTexture(tex);
      }
      //glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_PRIORITY, priority);
   }
}

void GL_PrecacheTextures()
{
   unsigned int *hitlist;
	BYTE *spritelist;
	int i, totalCount;

	textureList.Clear();
   totalCount = 0;

	hitlist = new unsigned int[TexMan.NumTextures()];
	
	// Precache textures (and sprites).
	memset(hitlist, 0, TexMan.NumTextures() * sizeof(unsigned int));

   if (gl_sprite_precache)
   {

      AActor *actor;
		TThinkerIterator<AActor> iterator;

      spritelist = new BYTE[sprites.Size()];
      memset (spritelist, 0, sprites.Size());

		while ( (actor = iterator.Next ()) )
			spritelist[actor->sprite] = 1;

	   for (i = (int)(sprites.Size () - 1); i >= 0; i--)
	   {
		   if (spritelist[i])
		   {
			   int j, k;
			   for (j = 0; j < sprites[i].numframes; j++)
			   {
				   const spriteframe_t *frame = &SpriteFrames[sprites[i].spriteframes + j];

				   for (k = 0; k < 16; k++)
				   {
					   int pic = frame->Texture[k];
					   if (pic != 0xFFFF)
					   {
                     if (hitlist[pic] == 0) totalCount++;
						   hitlist[pic]++;
					   }
				   }
			   }
		   }
	   }

      delete[] spritelist;
   }

   for (i = 0; i < numsectors; i++)
	{
      if (hitlist[sectors[i].floorpic] == 0) totalCount++;
      if (hitlist[sectors[i].ceilingpic] == 0) totalCount++;
      hitlist[sectors[i].floorpic]++;
      hitlist[sectors[i].ceilingpic]++;
	}

   for (i = 0; i < numsides; i++)
	{
      if (hitlist[sides[i].toptexture] == 0) totalCount++;
      if (hitlist[sides[i].midtexture] == 0) totalCount++;
      if (hitlist[sides[i].bottomtexture] == 0) totalCount++;

      hitlist[sides[i].toptexture]++;
      hitlist[sides[i].midtexture]++;
      hitlist[sides[i].bottomtexture]++;
	}

   // Sky texture is always present.
	// Note that F_SKY1 is the name used to
	//	indicate a sky floor/ceiling as a flat,
	//	while the sky texture is stored like
	//	a wall texture, with an episode dependant
	//	name.
	if (sky1texture >= 0)
	{
      if (hitlist[sky1texture] == 0) totalCount++;
		hitlist[sky1texture] = 1;
	}
	if (sky2texture >= 0)
	{
      if (hitlist[sky2texture] == 0) totalCount++;
		hitlist[sky2texture] = 1;
	}

   Printf("ZGL: Precaching textures...");
   // start at one to skip over the dummy texture (seems to cause crashes)...
	for (i = 1; i < TexMan.NumTextures(); i++)
	{
		if (hitlist[i])
		{
			FTexture *tex = TexMan[i];
			if (tex != NULL && tex->UseType != FTexture::TEX_Null)
			{
            tex->glData.Clear();
            textureList.AllowScale(tex->UseType != FTexture::TEX_Sprite);
            textureList.BindTexture(tex);
            FAnimDef *anim = GL_GetAnimForLump(i);
            if (anim != NULL)
            {
               GL_PrecacheAnimdef(anim, 0.f);
            }
            FShader *shader = GL_ShaderForTexture(tex);
            if (shader)
            {
               GL_PrecacheShader(shader, 0.f);
            }
			}
		}
	}
   textureList.AllowScale(true);
   Printf("done.\n");

	delete[] hitlist;
}


remap_tex_t *GL_GetRemapForName(const char *name)
{
   unsigned int i;

   for (i = 0; i < RemappedTextures.Size(); i++)
   {
      if (strcmp(RemappedTextures[i].origName, name) == 0)
      {
         //Printf("Found remap for %s (%s -> %s)\n", name, RemappedTextures[i].origName, RemappedTextures[i].newName);
         return &RemappedTextures[i];
      }
   }

   return NULL;
}


void GL_ParseRemappedTextures()
{
   int remapLump, lastLump;
   char src[9], dst[9];
   bool is32bit;
   int width, height;
   remap_tex_t *remap;
   FDefinedTexture *defTex;

   lastLump = 0;
   src[8] = '\0';
   dst[8] = '\0';

   while ((remapLump = Wads.FindLump("HIRESTEX", &lastLump)) != -1)
   {
      SC_OpenLumpNum(remapLump, "HIRESTEX");
      while (SC_GetString())
      {
         if (SC_Compare("remap")) // remap an existing texture
         {
            SC_GetString();
            memcpy(src, sc_String, 8);
            SC_GetString();
            memcpy(dst, sc_String, 8);

            remap = GL_GetRemapForName(src);
            if (remap == NULL)
            {
               RemappedTextures.Resize(RemappedTextures.Size() + 1);
               remap = &RemappedTextures[RemappedTextures.Size() - 1];
            }

            sprintf(remap->origName, "%s", src);
            sprintf(remap->newName, "%s", dst);
         }
         else if (SC_Compare("define")) // define a new "fake" texture
         {
            SC_GetString();
            memcpy(src, sc_String, 8);
            SC_GetString();
            is32bit = false;
            if (SC_Compare("force32bit"))
            {
               is32bit = true;
            }
            else
            {
               SC_UnGet();
            }
            SC_GetNumber();
            width = sc_Number;
            SC_GetNumber();
            height = sc_Number;

            if (TexMan.CheckForTexture(src, FTexture::TEX_Defined) < 0)
            {
               defTex = new FDefinedTexture(src, width, height);
               TexMan.AddTexture(defTex);
            }
            else
            {
               defTex = (FDefinedTexture *)TexMan[src];
               defTex->SetSize(width, height);
            }

            if (is32bit) defTex->Force32Bit(true);

            // add a remap to load the hires lump
            remap = GL_GetRemapForName(src);
            if (remap == NULL)
            {
               RemappedTextures.Resize(RemappedTextures.Size() + 1);
               remap = &RemappedTextures[RemappedTextures.Size() - 1];
            }
            sprintf(remap->origName, "%s", src);
            sprintf(remap->newName, "%s", src);
         }
         else
         {
         }
      }
      SC_Close();
   }
}


FDefinedTexture::FDefinedTexture(const char *name, int width, int height)
{
   sprintf(Name, "%s", name);
   Width = width;
   Height = height;
   UseType = FTexture::TEX_Defined;
   m_32bit = false;
}


FDefinedTexture::~FDefinedTexture()
{
}


void FDefinedTexture::Unload()
{
}


void FDefinedTexture::SetSize(int width, int height)
{
   this->Width = width;
   this->Height = height;
}


FTextureGLData::FTextureGLData()
{
   glTex = 0;
   translation = NULL;
   isAlpha = false;
   isTransparent = false;
   cx = cy = 1.f;
   topOffset = bottomOffset = 0.f;
}


FTextureGLData::~FTextureGLData()
{
   if (glTex != 0) glDeleteTextures(1, &glTex);
}


TextureList::TextureList()
{
}


TextureList::~TextureList()
{
   this->ShutDown();
}


void TextureList::Init()
{
   if (!m_initialized)
   {
      ilInit();
      iluInit();
      InitLUTs();

      TrackedTextures.Clear();

      glGetIntegerv(GL_MAX_TEXTURE_SIZE, &m_maxTextureSize);
      Printf("ZGL: Max texture size %d\n", m_maxTextureSize);

      glGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB, &m_numTexUnits);
      Printf("ZGL: Number of texel units %d\n", m_numTexUnits);
      m_lastBoundTexture = new unsigned int[m_numTexUnits];
      m_currentTexUnit = 0;
      for (int i = 0; i < m_numTexUnits; i++)
      {
         SelectTexUnit(GL_TEXTURE0_ARB + i);
         glEnable(GL_TEXTURE_2D);
         glBindTexture(GL_TEXTURE_2D, 0);
         m_lastBoundTexture[i] = 0;
         glDisable(GL_TEXTURE_2D);
      }

      SelectTexUnit(GL_TEXTURE0_ARB);

      m_supportsAnisotropic = GL_CheckExtension("GL_EXT_texture_filter_anisotropic");
      if (m_supportsAnisotropic)
      {
         Printf("ZGL: Enabling anisotropic texturing support.\n");
      }

      m_supportsS3TC = GL_CheckExtension("GL_EXT_texture_compression_s3tc");
      if (m_supportsS3TC)
      {
         Printf("ZGL: Enabling texture compression.\n");
      }

      m_supportsPT = GL_CheckExtension("GL_EXT_paletted_texture");
      if (m_supportsPT)
      {
         //Printf("ZGL: Enabling paletted textures.\n");
      }

      m_supportsNonPower2 = GL_CheckExtension("GL_ARB_texture_non_power_of_two");
      if (m_supportsNonPower2)
      {
         Printf("ZGL: supports non power 2 textures.\n");
      }

      m_loadSaturate = false;
      m_loadAsAlpha = false;
      m_translation = NULL;
      m_translationShort = 0;
      m_savegameTex[0] = 0;
      m_savegameTex[1] = 0;
      m_allowScale = true;
      m_allowMipMap = true;
      m_forceHQ = 0;

      m_initialized = true;
   }
}


void TextureList::ShutDown()
{
   if (m_initialized)
   {
      Clear();
      deleteSavegameTexture(0);
      deleteSavegameTexture(1);

      ilShutDown();

      delete [] m_lastBoundTexture;

      m_initialized = false;
   }
}


void TextureList::Clear()
{
   FTexture *tex;

   for (unsigned int i = 0; i < TrackedTextures.Size(); i++)
   {
      tex = TrackedTextures[i];
      for (unsigned int j = 0; j < tex->glData.Size(); j++)
      {
         delete tex->glData[j];
      }
      tex->glData.Clear();
   }

   TrackedTextures.Clear();

   m_translation = NULL;
   m_translationShort = 0;
}


void TextureList::colorOutlines(BYTE *img, int width, int height)
{
   int i, j, a, b, index, index2;

   for (j = 0; j < height; j++)
   {
      for (i = 0; i < width; i++)
      {
         index = (i + (j * width)) * 4;
         if (img[index + 3])
         {
            for (b = -1; b < 2; b++)
            {
               for (a = -1; a < 2; a++)
               {
                  if (i + a < 0 || i + a >= width || j + b < 0 || j + b >= height || (!a && !b)) continue;
                  index2 = ((i + a) + ((b + j) * width)) * 4;
                  if (!img[index2 + 3])
                  {
                     img[index2 + 0] = img[index + 0];
                     img[index2 + 1] = img[index + 1];
                     img[index2 + 2] = img[index + 2];
                  }
               }
            }
         }
      }
   }
}


int TextureList::checkExternalFile(char *fileName, BYTE useType, bool isPatch)
{
   char fn[80], texType[16], checkName[32];

   if (isPatch)
   {
      if (m_translationShort == 0)
      {
         sprintf(checkName, "%s", fileName);
      }
      else
      {
         sprintf(checkName, "%d_%d_%s", (m_translationShort & 0xff00) >> 8, m_translationShort & 0xff, fileName);
      }
      sprintf(texType, "patches");
   }
   else
   {
      sprintf(checkName, "%s", fileName);
      switch (useType)
      {
      case FTexture::TEX_Flat:
         sprintf(texType, "flats");
         break;
      default:
         sprintf(texType, "textures");
         break;
      }
   }

   switch (gameinfo.gametype)
   {
   case GAME_Doom:
      switch (gamemission)
      {
      case doom:
         sprintf(fn, "./%s/doom/doom1/%s", texType, checkName);
         if (_access(fn, 0) == 0) return 2;
         break;
      case doom2:
         sprintf(fn, "./%s/doom/doom2/%s", texType, checkName);
         if (_access(fn, 0) == 0) return 2;
         break;
      case pack_tnt:
         sprintf(fn, "./%s/doom/tnt/%s", texType, checkName);
         if (_access(fn, 0) == 0) return 2;
         break;
      case pack_plut:
         sprintf(fn, "./%s/doom/plut/%s", texType, checkName);
         if (_access(fn, 0) == 0) return 2;
         break;
      }
      sprintf(fn, "./%s/doom/%s", texType, checkName);
      if (_access(fn, 0) == 0) return 1;
      break;
   case GAME_Heretic:
      sprintf(fn, "./%s/heretic/%s", texType, checkName);
      if (_access(fn, 0) == 0) return 1;
      break;
   case GAME_Hexen:
      sprintf(fn, "./%s/hexen/%s", texType, checkName);
      if (_access(fn, 0) == 0) return 1;
      break;
   default:
      sprintf(fn, "./%s/%s", texType, checkName);
      if (_access(fn, 0) == 0) return 1;
   }

   return 0;
}


BYTE *TextureList::loadExternalFile(char *fileName, BYTE useType, int *width, int *height, bool gameSpecific, bool isPatch)
{
   char fn[80], texType[16], checkName[32];
   BYTE *buffer = NULL;

   if (isPatch)
   {
      if (m_translationShort == 0)
      {
         sprintf(checkName, "%s", fileName);
      }
      else
      {
         sprintf(checkName, "%d_%d_%s", (m_translationShort & 0xff00) >> 8, m_translationShort & 0xff, fileName);
      }
      sprintf(texType, "patches");
   }
   else
   {
      sprintf(checkName, "%s", fileName);
      switch (useType)
      {
      case FTexture::TEX_Flat:
         sprintf(texType, "flats");
         break;
      default:
         sprintf(texType, "textures");
         break;
      }
   }

   switch (gameinfo.gametype)
   {
   case GAME_Doom:
      if (gameSpecific)
      {
         switch (gamemission)
         {
         case doom:
            sprintf(fn, "./%s/doom/doom1/%s", texType, checkName);
            break;
         case doom2:
            sprintf(fn, "./%s/doom/doom2/%s", texType, checkName);
            break;
         case pack_tnt:
            sprintf(fn, "./%s/doom/tnt/%s", texType, checkName);
            break;
         case pack_plut:
            sprintf(fn, "./%s/doom/plut/%s", texType, checkName);
            break;
         default:
            sprintf(fn, "./%s/doom/%s", texType, checkName);
            break;
         }
      }
      else
      {
         sprintf(fn, "./%s/doom/%s", texType, checkName);
      }
      break;
   case GAME_Heretic:
      sprintf(fn, "./%s/heretic/%s", texType, checkName);
      break;
   case GAME_Hexen:
      sprintf(fn, "./%s/hexen/%s", texType, checkName);
      break;
   default:
      sprintf(fn, "./%s/%s", texType, checkName);
   }

   buffer = this->loadFile(fn, width, height);

   return buffer;
}


BYTE *TextureList::loadFile(char *fileName, int *width, int *height)
{
   byte *buffer = NULL;
   byte *data;
   ILuint imgID;
   int imgSize;

   ilGenImages(1, &imgID);
   ilBindImage(imgID);
   ilLoad(IL_TYPE_UNKNOWN, fileName);
   ilConvertImage(IL_RGBA, IL_UNSIGNED_BYTE);
   // for some reason, tga's load upside down
   if (strstr(fileName, ".tga") != NULL)
   {
      iluFlipImage();
   }

   *width = ilGetInteger(IL_IMAGE_WIDTH);
   *height = ilGetInteger(IL_IMAGE_HEIGHT);
   imgSize = *width * *height * 4;
   buffer = new byte[imgSize];
   data = ilGetData();
#if 1
   memcpy(buffer, data, imgSize);
#else
   for (y = 0; y < *height; y++)
   {
      for (x = 0; x < *width; x++)
      {
         index = x + (y * *width);
         buffer[(index * 4) + 0] = data[(index * 4) + 0];
         buffer[(index * 4) + 1] = data[(index * 4) + 1];
         buffer[(index * 4) + 2] = data[(index * 4) + 2];
         buffer[(index * 4) + 3] = data[(index * 4) + 3];
      }
   }
#endif
   ilDeleteImages(1, &imgID);

   return buffer;
}


BYTE *TextureList::loadFromLump(char *lumpName, int *width, int *height)
{
   BYTE *buffer = NULL, *data;
   ILuint imgID;
   void *lumpData;
   int lumpNum, tmpLump, lastLump, imgSize;
   FMemLump memLump;

   tmpLump = 0;
   while ((lumpNum = Wads.FindLump (lumpName, &tmpLump)) != -1)
   {
      lastLump = lumpNum;
   }
   lumpNum = lastLump;

   if (lumpNum == -1)
   {
      return NULL;
   }
   memLump = Wads.ReadLump(lumpNum);
   lumpData = memLump.GetMem();

   ilGenImages(1, &imgID);
   ilBindImage(imgID);

   ilLoadL(IL_TYPE_UNKNOWN, lumpData, Wads.LumpLength(lumpNum));

   ilConvertImage(IL_RGBA, IL_UNSIGNED_BYTE);

   *width = ilGetInteger(IL_IMAGE_WIDTH);
   *height = ilGetInteger(IL_IMAGE_HEIGHT);
   imgSize = *width * *height * 4;
   buffer = new BYTE[imgSize];
   data = ilGetData();
#if 1
   memcpy(buffer, data, imgSize);
#else
   for (y = 0; y < *height; y++)
   {
      for (x = 0; x < *width; x++)
      {
         index = x + (y * *width);
         buffer[(index * 4) + 0] = data[(index * 4) + 0];
         buffer[(index * 4) + 1] = data[(index * 4) + 1];
         buffer[(index * 4) + 2] = data[(index * 4) + 2];
         buffer[(index * 4) + 3] = data[(index * 4) + 3];
      }
   }
#endif
   ilDeleteImages(1, &imgID);

   return buffer;
}


int TextureList::getTextureFormat(FTexture *tex)
{
   if (m_loadAsAlpha)
   {
      return GL_ALPHA;
   }

   if (tex && tex->UseType == FTexture::TEX_Defined)
   {
      if (static_cast<FDefinedTexture *>(tex)->Is32bit())
      {
         return GL_RGBA8;
      }
   }

   if (tex && tex->CanvasTexture())
   {
      // only use 16 bit textures for camera textures
      // keeps the speed up
      switch (gl_texture_camtex_format)
      {
      case 1:
         return GL_LUMINANCE;
      default:
         return GL_RGB5_A1;
      }
   }

   if (strcmp(gl_texture_format, "GL_RGB") == 0)
      return GL_RGB;

   if (strcmp(gl_texture_format, "GL_RGBA2") == 0)
      return GL_RGBA2;

   if (strcmp(gl_texture_format, "GL_RGBA4") == 0)
      return GL_RGBA4;

   if (strcmp(gl_texture_format, "GL_RGBA8") == 0)
      return GL_RGBA8;

   if (strcmp(gl_texture_format, "GL_RGB5_A1") == 0)
      return GL_RGB5_A1;

   if (strcmp(gl_texture_format, "GL_LUMINANCE") == 0)
      return GL_LUMINANCE_ALPHA;

   if (strcmp(gl_texture_format, "GL_ALPHA") == 0)
      return GL_ALPHA;

   if (strcmp(gl_texture_format, "GL_INTENSITY") == 0)
      return GL_INTENSITY;

   if (m_supportsS3TC)
   {
      if (strcmp(gl_texture_format, "GL_S3TC") == 0)
      {
         return COMPRESSED_RGBA_S3TC_DXT3_EXT;
      }
   }

   return GL_RGB5_A1;
}


int TextureList::GetTextureMode()
{
   if (strcmp(gl_texture_mode, "GL_NEAREST") == 0)
      return GL_NEAREST;

   if (strcmp(gl_texture_mode, "GL_LINEAR") == 0)
      return GL_LINEAR;

   if (strcmp(gl_texture_mode, "GL_NEAREST_MIPMAP_NEAREST") == 0)
      return GL_NEAREST_MIPMAP_NEAREST;

   if (strcmp(gl_texture_mode, "GL_NEAREST_MIPMAP_LINEAR") == 0)
      return GL_NEAREST_MIPMAP_LINEAR;

   if (strcmp(gl_texture_mode, "GL_LINEAR_MIPMAP_NEAREST") == 0)
      return GL_LINEAR_MIPMAP_NEAREST;

   if (strcmp(gl_texture_mode, "GL_NEAREST_MIPMAP_LINEAR") == 0)
      return GL_NEAREST_MIPMAP_LINEAR;

   return GL_LINEAR_MIPMAP_LINEAR;
}


int TextureList::GetTextureModeMag()
{
   if (strcmp(gl_texture_mode, "GL_NEAREST") == 0)
      return GL_NEAREST;

   if (strcmp(gl_texture_mode, "GL_LINEAR") == 0)
      return GL_LINEAR;

   if (strcmp(gl_texture_mode, "GL_NEAREST_MIPMAP_NEAREST") == 0)
      return GL_NEAREST;

   if (strcmp(gl_texture_mode, "GL_NEAREST_MIPMAP_LINEAR") == 0)
      return GL_NEAREST;

   if (strcmp(gl_texture_mode, "GL_LINEAR_MIPMAP_NEAREST") == 0)
      return GL_LINEAR;

   if (strcmp(gl_texture_mode, "GL_NEAREST_MIPMAP_LINEAR") == 0)
      return GL_NEAREST;

   return GL_LINEAR;
}


void TextureList::SetupTexparams()
{
   //glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_PRIORITY, 0.0f);

   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GetTextureModeMag());

   if (m_allowMipMap)
   {
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GetTextureMode());
   }
   else
   {
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GetTextureModeMag());
   }

   if (m_supportsAnisotropic)
   {
      if (gl_texture_anisotropic)
      {
         gl_texture_anisotropy_degree = gl_texture_anisotropy_degree < 1.0f ? 1.0f : gl_texture_anisotropy_degree;
         glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, gl_texture_anisotropy_degree);
      }
      else
      {
         glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1.0f);
      }
   }
}


void TextureList::Purge()
{
   this->Clear();
   GL_PrecacheTextures();
}


void TextureList::BindGLTexture(int texId)
{
   if (m_lastBoundTexture[m_currentTexUnit] != texId)
   {
      glBindTexture(GL_TEXTURE_2D, texId);
      m_lastBoundTexture[m_currentTexUnit] = texId;
      SetupTexparams();
   }
}


BYTE *TextureList::hqresize(BYTE *buffer, int *width, int *height)
{
   BYTE *img;
   CImage hqimage;
   int w, h, type;

   w = *width;
   h = *height;

   hqimage.SetImage(buffer, w, h, 32);
   delete[] buffer;
   hqimage.Convert32To17();

   if (m_forceHQ)
   {
      type = m_forceHQ;
   }
   else
   {
      type = gl_texture_hqresize;
   }

   switch (type)
   {
   case 3:
      w *= 4;
      h *= 4;
      img = new BYTE[w * h * 4];
      hq4x_32((int*)hqimage.m_pBitmap, img, hqimage.m_Xres, hqimage.m_Yres, w * 4);
      break;
   case 2:
      w *= 3;
      h *= 3;
      img = new BYTE[w * h * 4];
      hq3x_32((int*)hqimage.m_pBitmap, img, hqimage.m_Xres, hqimage.m_Yres, w * 4);
      break;
   default:
      w *= 2;
      h *= 2;
      img = new BYTE[w * h * 4];
      hq2x_32((int*)hqimage.m_pBitmap, img, hqimage.m_Xres, hqimage.m_Yres, w * 4);
      break;
   }

   hqimage.Destroy();

   *width = w;
   *height = h;

   return img;
}


void TextureList::addLuminanceNoise(BYTE *buffer, int width, int height)
{
   // add some random luminance noise to the input image
   int x, y, index;
   BYTE r, g, b;
   float luminance;
   float plusMinus = 0.1f;

   for (y = 0; y < height; y++)
   {
      for (x = 0; x < width; x++)
      {
         index = (x + (y * width)) * 4;
         r = buffer[index + 0];
         g = buffer[index + 1];
         b = buffer[index + 2];
         luminance = plusMinus * (rand() * 1.f / RAND_MAX);
         luminance = (1.f - (plusMinus / 2.f)) + luminance;
         buffer[index + 0] = (BYTE)clamp<int>((int)(r * luminance), 0, 255);
         buffer[index + 1] = (BYTE)clamp<int>((int)(g * luminance), 0, 255);
         buffer[index + 2] = (BYTE)clamp<int>((int)(b * luminance), 0, 255);
      }
   }
}


BYTE *TextureList::scaleImage(BYTE *buffer, int *width, int *height, bool highQuality)
{
   int ow, oh, w, h, x, y, indexSrc, indexDst, sx, sy;
   float scaleX, scaleY;
   bool forceScale = false;
   BYTE *img;

   ow = *width;
   oh = *height;

   if (m_supportsNonPower2)
   //if (false)
   {
      w = ow;
      h = oh;
   }
   else
   {
      w = CeilPow2(ow);
      h = CeilPow2(oh);
   }

   if (w > m_maxTextureSize)
   {
      w = m_maxTextureSize;
      forceScale = true;
   }

   if (h > m_maxTextureSize)
   {
      h = m_maxTextureSize;
      forceScale = true;
   }

   if (w == ow && h == oh)
   {
      return buffer;
   }

   img = new BYTE[w * h * 4];
   memset(img, 0, w * h * 4);

   if (m_allowScale || forceScale)
   {
      if (highQuality)
      {
         gluScaleImage(GL_RGBA, ow, oh, GL_UNSIGNED_BYTE, buffer, w, h, GL_UNSIGNED_BYTE, img);
      }
      else
      {
         scaleX = ow * 1.f / w;
         scaleY = oh * 1.f / h;

         for (y = 0; y < h; y++)
         {
            for (x = 0; x < w; x++)
            {
               sx = (int)(x * scaleX);
               sy = (int)(y * scaleY);
               indexDst = (x + (y * w)) * 4;
               indexSrc = (sx + (sy * ow)) * 4;
               memcpy(img + indexDst, buffer + indexSrc, 4);
            }
         }
      }
   }
   else
   {
      for (y = 0; y < oh; y++)
      {
         indexDst = y * w * 4;
         indexSrc = y * ow * 4;
         memcpy(img + indexDst, buffer + indexSrc, ow * 4);
      }
   }

   *width = w;
   *height = h;

   delete[] buffer;

   return img;
}


unsigned long TextureList::calcAverageColor(BYTE *buffer, int width, int height)
{
   int x, y, index, countX, count;
   int cumulColorX[3], cumulColor[3];

   cumulColor[0] = cumulColor[1] = cumulColor[2] = 0;
   count = 0;

   for (y = 0; y < height / 3; y++)
   {
      cumulColorX[0] = cumulColorX[1] = cumulColorX[2] = 0;
      countX = 0;

      for (x = 0; x < width; x++)
      {
         index = (x + (y * width)) * 4;
         if (buffer[index + 3] > 0)
         {
            countX++;
            cumulColorX[0] += buffer[index + 0];
            cumulColorX[1] += buffer[index + 1];
            cumulColorX[2] += buffer[index + 2];
         }
      }

      if (countX != 0)
      {
         count++;
         cumulColor[0] += cumulColorX[0] / countX;
         cumulColor[1] += cumulColorX[1] / countX;
         cumulColor[2] += cumulColorX[2] / countX;
      }
   }

   if (count != 0)
   {
      return (unsigned long)(GEN_RGB(cumulColor[0] / count, cumulColor[1] / count, cumulColor[2] / count));
   }
   else
   {
      return 0;
   }
}


void TextureList::GetAverageColor(float *r, float *g, float *b)
{
   *r = byte2float[(m_workingData->averageColor & 0x00ff0000) >> 16];
   *g = byte2float[(m_workingData->averageColor & 0x0000ff00) >> 8];
   *b = byte2float[m_workingData->averageColor & 0x000000ff];
}


GLuint TextureList::loadTexture(FTexture *tex)
{
   const BYTE *img;
   BYTE *buffer, ci, transIndex;
   int neededSize;
   int x, y, indexSrc, indexDst, nw, nh, origW, origH, gs, blankRows;
   int imgSize;
   PalEntry *p;
   GLuint texID;
   GLint format;
   float c;
   char texName[32], fileName[32];
   bool imgLoaded = false; // this shows when a hires texture is loaded
   bool isPatch = false;
   remap_tex_t *remap;
   bool isTransparent;
   bool rowBlank;
   unsigned long averageColor;

   isPatch |= (tex->UseType == FTexture::TEX_Sprite);
   isPatch |= (tex->UseType == FTexture::TEX_MiscPatch);
   isPatch |= (tex->UseType == FTexture::TEX_FontChar);

   nw = tex->GetWidth();
   nh = tex->GetHeight();

   if (nw == 0 || nh == 0 || tex->UseType == FTexture::TEX_Null)
   {
      // use an invalid texture for, well, invalid textures!
      isTransparent = false;
      m_workingData->isAlpha = m_loadAsAlpha;
      m_workingData->translation = m_translation;
      return 0;
   }

   isTransparent = false;
   transIndex = 0x00;

   // remapped textures are not affected by the gl_texture_usehires cvar
   // they're in the wad for a reason!
   remap = GL_GetRemapForName(tex->Name);
   if (remap)
   {
      buffer = loadFromLump(remap->newName, &nw, &nh);
      if (buffer)
      {
         imgLoaded = true;
      }
      else
      {
         // remapped texture is missing, so use blank texture
         tex = TexMan[0];
      }
   }

   // remapped textures are dummy textures, so only check for no image after checking for remaps
   if (!imgLoaded && !tex->GetPixels()) return 0;

   if (gl_texture_usehires && !imgLoaded)
   {
      sprintf(texName, "%s", tex->Name);

      // ooh, jpg support :)
      sprintf(fileName, "%s.jpg", texName);
      if (!imgLoaded && (gs = checkExternalFile(fileName, tex->UseType, isPatch)))
      {
         buffer = loadExternalFile(fileName, tex->UseType, &nw, &nh, gs == 2, isPatch);
         imgLoaded = true;
      }
      sprintf(fileName, "%s.png", texName);
      if (!imgLoaded && (gs = checkExternalFile(fileName, tex->UseType, isPatch)))
      {
         buffer = loadExternalFile(fileName, tex->UseType, &nw, &nh, gs == 2, isPatch);
         imgLoaded = true;
      }
      sprintf(fileName, "%s.tga", texName);
      if (!imgLoaded && (gs = checkExternalFile(fileName, tex->UseType, isPatch)))
      {
         buffer = loadExternalFile(fileName, tex->UseType, &nw, &nh, gs == 2, isPatch);
         imgLoaded = true;
      }
   }

   imgSize = nw * nh;
   neededSize = imgSize * 4;

   if (!imgLoaded)
   {
      img = tex->GetPixels();
      buffer = new BYTE[neededSize];
      //memset(buffer, 0, neededSize);

      isTransparent = m_loadAsAlpha;

      if (img != NULL)
      {
         for (y = 0; y < nh; y++)
         {
            for (x = 0; x < nw; x++)
            {
               indexSrc = y + (x * nh);
               indexDst = (x + (y * nw)) * 4;
               ci = img[indexSrc];
               if (m_loadAsAlpha)
               {
                  buffer[indexDst + 0] = 0xff;
                  buffer[indexDst + 1] = 0xff;
                  buffer[indexDst + 2] = 0xff;
                  buffer[indexDst + 3] = ci;
               }
               else
               {
                  if (ci == transIndex && tex->UseType != FTexture::TEX_Flat)
                  {
                     buffer[indexDst + 3] = 0;
                     isTransparent = true;
                  }
                  else
                  {
                     buffer[indexDst + 3] = 255;
                  }

                  if (m_translation)
                  {
                     ci = m_translation[ci];
                  }

                  p = &screen->GetPalette()[ci];
                  if (static_cast<OpenGLFrameBuffer *>(screen)->SupportsGamma())
                  {
                     c = p->r * gl_texture_color_multiplier;
                     buffer[indexDst + 0] = (BYTE)clamp<int>(c, 0, 255);
                     c = p->g * gl_texture_color_multiplier;
                     buffer[indexDst + 1] = (BYTE)clamp<int>(c, 0, 255);
                     c = p->b * gl_texture_color_multiplier;
                     buffer[indexDst + 2] = (BYTE)clamp<int>(c, 0, 255);
                  }
                  else
                  {
                     c = static_cast<OpenGLFrameBuffer *>(screen)->GetGamma(p->r) * gl_texture_color_multiplier;
                     buffer[indexDst + 0] = (BYTE)clamp<int>(c, 0, 255);
                     c = static_cast<OpenGLFrameBuffer *>(screen)->GetGamma(p->g) * gl_texture_color_multiplier;
                     buffer[indexDst + 1] = (BYTE)clamp<int>(c, 0, 255);
                     c = static_cast<OpenGLFrameBuffer *>(screen)->GetGamma(p->b) * gl_texture_color_multiplier;
                     buffer[indexDst + 2] = (BYTE)clamp<int>(c, 0, 255);
                  }
               }
            }
         }
      }
      else
      {
         memset(buffer, 255, neededSize);
         isTransparent = false;
      }
   }
   else
   {
      if (isPatch)
      {
         // just mark hires patches as always transparent
         isTransparent = true;
      }
      else
      {
         for (indexSrc = 0; indexSrc < neededSize - 1; indexSrc += 4)
         {
            if (buffer[indexSrc + 3] != 0xff) isTransparent = true;
         }
      }
   }

   if (tex->CanvasTexture()) isTransparent = false;

   if (isTransparent)
   {
      blankRows = 0;
      for (y = 0; y < nh; y++)
      {
         rowBlank = true;
         for (x = 3; x < nw; x += 4)
         {
            if (buffer[((x + (y * nw)) * 4) + 3] == 0xff)
            {
               rowBlank = false;
            }
         }
         if (rowBlank)
         {
            blankRows++;
         }
         else
         {
            break;
         }
      }
      m_workingData->topOffset = blankRows / (nh * 1.f);

      blankRows = 0;
      for (y = nh - 1; y >= 0; y--)
      {
         rowBlank = true;
         for (x = 3; x < nw; x += 4)
         {
            if (buffer[((x + (y * nw)) * 4) + 3] == 0xff)
            {
               rowBlank = false;
            }
         }
         if (rowBlank)
         {
            blankRows++;
         }
         else
         {
            break;
         }
      }
      m_workingData->bottomOffset = blankRows / (nh * 1.f);
   }

   //isTransparent = tex->bMasked;

   if (m_loadSaturate) adjustColor(buffer, nw, nh);
   averageColor = calcAverageColor(buffer, nw, nh);
   if (!m_loadAsAlpha) colorOutlines(buffer, nw, nh);
   m_workingData->averageColor = averageColor;
   m_workingData->isTransparent = isTransparent;
   m_workingData->isAlpha = m_loadAsAlpha;
   m_workingData->translation = m_translation;

   // only resample non-hires images and non-alpha images
   if ((gl_texture_hqresize || m_forceHQ) && !m_loadAsAlpha && !imgLoaded)
   {
      buffer = hqresize(buffer, &nw, &nh);
      if (!isPatch) addLuminanceNoise(buffer, nw, nh);
   }

   origW = nw;
   origH = nh;

   buffer = scaleImage(buffer, &nw, &nh, true);

   if (!m_allowScale)
   {
      m_workingData->cx = origW * 1.f / nw;
      m_workingData->cy = origH * 1.f / nh;
   }

   glGenTextures(1, &texID);
   glBindTexture(GL_TEXTURE_2D, texID);
   //glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

   format = getTextureFormat(tex);

   glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, GL_TRUE);
   glTexImage2D(GL_TEXTURE_2D, 0, format, nw, nh, 0, GL_RGBA, GL_UNSIGNED_BYTE, buffer);

   delete[] buffer;

   return texID;
}


void TextureList::GetTexture(int textureID, bool translate)
{
   FTexture *tex;

   if (translate)
   {
      tex = TexMan(textureID);
   }
   else
   {
      tex = TexMan[textureID];
   }

   GetTexture(tex);
}


void TextureList::GetTexture(FTexture *tex)
{
   FTextureGLData *data = NULL;

   if (tex->UseType == FTexture::TEX_Null)
   {
      Printf("ZGL: binding null texture.\n");
   }

   // this basically just sets up the texture dimensions before grabbing the pixels.
   tex->GetWidth(); tex->GetHeight();

   for (unsigned int i = 0; i < tex->glData.Size(); i++)
   {
      if ((tex->glData[i]->translation == m_translation && tex->glData[i]->isAlpha == m_loadAsAlpha) || tex->CanvasTexture() || tex->UseType == FTexture::TEX_Null)
      {
         m_workingData = tex->glData[i];
         if (tex->CanvasTexture()) tex->GetPixels(); // flag the texture as dirty
         return;
      }
   }

   // no texture matching current translation found, so add the new texture
   if (tex->glData.Size() == 0) TrackedTextures.Push(tex);
   m_workingData = new FTextureGLData();
   m_workingData->glTex = loadTexture(tex);
   tex->glData.Push(m_workingData);
}


void TextureList::BindTexture(int textureID, bool translate)
{
   FTexture *tex;

   if (translate)
   {
      tex = TexMan(textureID);
   }
   else
   {
      tex = TexMan[textureID];
   }
#ifdef _DEBUG
   if ( tex == NULL )
   {
      Printf("ZGL: Trying to bind a NULL FTexture pointer. This should not happen!.\n");
	  return;
   }
#endif
   BindTexture(tex);
}


void TextureList::BindTexture(const char *texName)
{
   BindTexture(TexMan[texName]);
}


void TextureList::BindTexture(FTexture *tex)
{
   if (m_workingData)
   {
      UnbindTexture();
   }

   GetTexture(tex);

   BindGLTexture(m_workingData->glTex);

   if (tex->CheckModified())
   {
      updateTexture(tex);
   }

   //SetupTexparams();
}


void TextureList::UnbindTexture()
{
}


bool TextureList::IsTransparent()
{
   if (m_workingData != NULL)
   {
      return m_workingData->isTransparent;
   }
   else
   {
      return false;
   }
}


bool TextureList::IsLoadingAlpha()
{
   return m_loadAsAlpha;
}


void TextureList::SetTranslation(const BYTE *trans)
{
   m_translation = (BYTE *)trans;
   m_translationShort = 0;
}


void TextureList::SetTranslation(unsigned short trans)
{
   m_translationShort = trans;

   if (trans != 0)
   {
      m_translation = translationtables[(trans & 0xff00) >> 8] + (trans & 0x00ff) * 256;
   }
   else
   {
      m_translation = NULL;
   }
}


void TextureList::updateTexture(FTexture *tex)
{
   int x, y, width, height;
   int indexSrc, indexDst;
   const BYTE *img;
   BYTE *buffer, ci, transIndex;
   PalEntry *p;
   float c;
   bool isTransparent;

   if (tex->CanvasTexture()) return;

   isTransparent = false;
   transIndex = 0x00;

   img = tex->GetPixels();
   width = tex->GetWidth();
   height = tex->GetHeight();

   buffer = new BYTE[width * height * 4];

   for (y = 0; y < height; y++)
   {
      for (x = 0; x < width; x++)
      {
         indexSrc = y + (x * height);
         indexDst = (x + (y * width)) * 4;

         if (m_loadAsAlpha)
         {
            ci = img[indexSrc];
            buffer[indexDst + 0] = 0xff;
            buffer[indexDst + 1] = 0xff;
            buffer[indexDst + 2] = 0xff;
            buffer[indexDst + 3] = ci;
            isTransparent = true;
         }
         else
         {
            ci = img[indexSrc];

            if (ci != transIndex || tex->UseType == FTexture::TEX_Flat)
            {
               buffer[indexDst + 3] = 255;
            }
            else
            {
               buffer[indexDst + 3] = 0;
               isTransparent = true;
            }

            if (m_translation)
            {
               ci = m_translation[ci];
            }

            p = &screen->GetPalette()[ci];
            if (static_cast<OpenGLFrameBuffer *>(screen)->SupportsGamma())
            {
               c = p->r * gl_texture_color_multiplier;
               buffer[indexDst + 0] = (BYTE)clamp<int>(c, 0, 255);
               c = p->g * gl_texture_color_multiplier;
               buffer[indexDst + 1] = (BYTE)clamp<int>(c, 0, 255);
               c = p->b * gl_texture_color_multiplier;
               buffer[indexDst + 2] = (BYTE)clamp<int>(c, 0, 255);
            }
            else
            {
               c = static_cast<OpenGLFrameBuffer *>(screen)->GetGamma(p->r) * gl_texture_color_multiplier;
               buffer[indexDst + 0] = (BYTE)clamp<int>(c, 0, 255);
               c = static_cast<OpenGLFrameBuffer *>(screen)->GetGamma(p->g) * gl_texture_color_multiplier;
               buffer[indexDst + 1] = (BYTE)clamp<int>(c, 0, 255);
               c = static_cast<OpenGLFrameBuffer *>(screen)->GetGamma(p->b) * gl_texture_color_multiplier;
               buffer[indexDst + 2] = (BYTE)clamp<int>(c, 0, 255);
            }
         }
      }
   }

   if (m_loadSaturate) adjustColor(buffer, width, height);

   if (gl_texture_hqresize && !m_loadAsAlpha)
   {
      buffer = hqresize(buffer, &width, &height);
   }

   if (!m_loadAsAlpha) colorOutlines(buffer, width, height);
   this->AllowScale(false);
   buffer = scaleImage(buffer, &width, &height, false);
   this->AllowScale(true);

   glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, GL_TRUE);
   glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, buffer);

   delete [] buffer;
}


void TextureList::UpdateForTranslation(BYTE *trans)
{
   BYTE *oldTrans;
   FTexture *tex;

   oldTrans = m_translation;
   m_translation = trans;
   for (unsigned int j = 0; j < TrackedTextures.Size(); j++)
   {
      tex = TrackedTextures[j];
      for (unsigned int i = 0; i < tex->glData.Size(); i++)
      {
         if (tex->glData[i]->translation == m_translation)
         {
            BindGLTexture(tex->glData[i]->glTex);
            updateTexture(tex);
         }
      }
      //tex = tex->Next;
   }
   m_translation = oldTrans;
}


void TextureList::SetSavegameTexture(int texIndex, BYTE *img, int width, int height)
{
   int x, y, format, index;
   BYTE *buffer, ci;
   PalEntry *p;

   deleteSavegameTexture(texIndex);

   if (img)
   {
      buffer = new BYTE[width * height * 4];

      for (y = 0; y < height; y++)
      {
         for (x = 0; x < width; x++)
         {
            index = x + (y * width);
            ci = img[index];
            p = &screen->GetPalette()[ci];
            buffer[(index * 4) + 0] = p->r;
            buffer[(index * 4) + 1] = p->g;
            buffer[(index * 4) + 2] = p->b;
            buffer[(index * 4) + 3] = 255;
         }
      }

      buffer = scaleImage(buffer, &width, &height, false);

      glGenTextures(1, &m_savegameTex[texIndex]);
      BindGLTexture(m_savegameTex[texIndex]);

      format = getTextureFormat(NULL);

      glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, buffer);

      delete[] buffer;
   }
}


void TextureList::BindSavegameTexture(int index)
{
   BindGLTexture(m_savegameTex[index]);
}


void TextureList::deleteSavegameTexture(int index)
{
   if (m_savegameTex[index])
   {
      glDeleteTextures(1, &m_savegameTex[index]);
      m_savegameTex[index] = 0;
   }
}


void TextureList::GrabFB()
{
   BYTE *fb, *scaledfb;
   int width, height, format;

   width = 256;
   height = 256;

   fb = new BYTE[screen->GetWidth() * screen->GetHeight() * 4];
   scaledfb = new BYTE[width * height * 4];

   glReadPixels(0, 0, screen->GetWidth(), screen->GetHeight(), GL_RGBA, GL_UNSIGNED_BYTE, fb);
   gluScaleImage(GL_RGBA, screen->GetWidth(), screen->GetHeight(), GL_UNSIGNED_BYTE, fb, width, height, GL_UNSIGNED_BYTE, scaledfb);
   delete[] fb;

   BindSavegameTexture(0);
   glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
   format = getTextureFormat(NULL);
   glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaledfb);
   delete[] scaledfb;

   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GetTextureModeMag());
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GetTextureModeMag());
}


// note that this is only used for reading the savegame image (from GL_RenderViewToCanvas)
BYTE *TextureList::ReduceFramebufferToPalette(int width, int height, bool fullScreen)
{
   int x, y, indexSrc, indexDst, offset;
   int capWidth, capHeight, origX, origY;
   BYTE *img, *fb, *scaledfb;
   PalEntry p;

   if (fullScreen)
   {
      capWidth = SCREENWIDTH;
      capHeight = SCREENHEIGHT;
      origX = 0;
      origY = 0;
   }
   else
   {
      capWidth = realviewwidth;
      capHeight = realviewheight;
      origX = viewwindowx;
      origY = viewwindowy;
   }

   if ((realviewheight == screen->GetHeight()) || fullScreen)
   {
      offset = 0;
   }
   else
   {
      offset = screen->GetHeight() - ST_Y;
   }

   fb = new BYTE[capWidth * capHeight * 3];
   scaledfb = new BYTE[width * height * 3];
   img = new BYTE[width * height];

   static_cast<OpenGLFrameBuffer *>(screen)->ReadPixels(origX, origY + offset, capWidth, capHeight, fb);
   //glReadPixels(viewwindowx, viewwindowy + offset, realviewwidth, realviewheight, GL_RGB, GL_UNSIGNED_BYTE, fb);
   gluScaleImage(GL_RGB, capWidth, capHeight, GL_UNSIGNED_BYTE, fb, width, height, GL_UNSIGNED_BYTE, scaledfb);
   delete[] fb;

   for (y = 0; y < height; y++)
   {
      for (x = 0; x < width; x++)
      {
         indexSrc = x + (y * width);
         indexDst = x + ((height - (y + 1)) * width);
         p.r = scaledfb[(indexSrc * 3) + 0];
         p.g = scaledfb[(indexSrc * 3) + 1];
         p.b = scaledfb[(indexSrc * 3) + 2];
         img[indexDst] = ColorMatcher.Pick(p.r, p.g, p.b);
      }
   }

   delete[] scaledfb;

   return img;
}


unsigned int TextureList::GetGLTexture()
{
   if (m_workingData)
   {
      return m_workingData->glTex;
   }
   else
   {
      return 0;
   }
}


void TextureList::GetCorner(float *x, float *y)
{
   if (m_workingData)
   {
      *x = m_workingData->cx;
      *y = m_workingData->cy;
   }
   else
   {
      *x = 0.f;
      *y = 0.f;
   }
}

float TextureList::GetTopYOffset()
{
   if (m_workingData)
   {
      return m_workingData->topOffset;
   }
   else
   {
      return 0.f;
   }
}


float TextureList::GetBottomYOffset()
{
   if (m_workingData)
   {
      return m_workingData->bottomOffset;
   }
   else
   {
      return 0.f;
   }
}


void TextureList::adjustColor(BYTE *img, int width, int height)
{
   int index;
   BYTE c;

   for (int y = 0; y < height; y++)
   {
      for (int x = 0; x < width; x++)
      {
         index = (x + (y * width)) * 4;

         c = img[index + 0];
         c = c >= 128 ? 255 : c * 2;
         img[index + 0] = c;

         c = img[index + 1];
         c = c >= 128 ? 255 : c * 2;
         img[index + 1] = c;

         c = img[index + 2];
         c = c >= 128 ? 255 : c * 2;
         img[index + 2] = c;

         c = img[index + 3];
         c = c >= 128 ? 255 : c * 2;
         img[index + 3] = c;
      }
   }
}


extern PFNGLACTIVETEXTUREARBPROC glActiveTextureARB;

bool TextureList::SelectTexUnit(int unit)
{
   int selUnit = unit - GL_TEXTURE0_ARB;

   if (selUnit < m_numTexUnits)
   {
      m_currentTexUnit = selUnit;
      glActiveTextureARB(unit);
      return true;
   }

   return false;
}
