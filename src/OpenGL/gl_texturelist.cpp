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
#include "gl_texturelist.h"
#include "gl_main.h"
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
TArray<FDummyTexture> DefinedTextures;
FTexture *RootTexture = NULL;
TArray<FTexture *> TrackedTextures;

extern level_locals_t level;
extern GameMission_t	gamemission;
extern FColorMatcher ColorMatcher;

int TextureMode;


EXTERN_CVAR (Float, Gamma)
EXTERN_CVAR (Bool, gl_wireframe)
EXTERN_CVAR (Bool, gl_texture)

CVAR(Bool, gl_texture_anisotropic, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Float, gl_texture_anisotropy_degree, 2.0f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

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
      lumpNum = Wads.CheckNumForName(remap->newName);
      Printf("%s -> %s (%s)\n", remap->origName, remap->newName, lumpNum != -1 ? "ok" : "not found");
   }
}


extern FPalette GPalette;
void InitLUTs();


inline int CeilPow2(int num)
{
	int cumul;
	for(cumul=1; num > cumul; cumul <<= 1);
	return cumul;
}


inline int FloorPow2(int num)
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


CVAR (Bool, gl_sprite_precache, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

void GL_PrecacheAnimdef(FAnimDef *anim, float priority)
{
   unsigned short texNum, i;

   for (i = 0; i < anim->NumFrames; i++)
   {
      if (0)//anim->bUniqueFrames)
      {
         texNum = anim->Frames[i].FramePic;
      }
      else
      {
         texNum = anim->BasePic + i;
      }
      textureList.GetTexture(TexMan[texNum]);
      glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_PRIORITY, priority);
   }
}

void GL_PrecacheShader(FShader *shader, float priority)
{
   FShaderLayer *layer;
   unsigned int i;

   for (i = 0; i < shader->layers.Size(); i++)
   {
      layer = shader->layers[i];
      textureList.GetTexture(TexMan[layer->name]);
      glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_PRIORITY, priority);
   }
}

void GL_PrecacheTextures()
{
   unsigned int *hitlist, maxHit;
	BYTE *spritelist;
	int i, totalCount, count;
   float priority;

	textureList.Clear();
   totalCount = 0;

   //Printf("ZGL: Precaching textures...\n");

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

   count = 0;
   C_InitTicker ("Precaching Textures", totalCount);

   maxHit = 0;
   for (i = 0; i < TexMan.NumTextures(); i++)
   {
      maxHit = MAX<unsigned int>(maxHit, hitlist[i]);
   }

#if 1
   // Sky texture is always present.
	// Note that F_SKY1 is the name used to
	//	indicate a sky floor/ceiling as a flat,
	//	while the sky texture is stored like
	//	a wall texture, with an episode dependant
	//	name.
	if (sky1texture >= 0)
	{
      if (hitlist[sky1texture] == 0) totalCount++;
		hitlist[sky1texture] = maxHit;
	}
	if (sky2texture >= 0)
	{
      if (hitlist[sky2texture] == 0) totalCount++;
		hitlist[sky2texture] = maxHit;
	}
#endif

	for (i = 0; i < TexMan.NumTextures(); i++)
	{
		if (hitlist[i])
		{
			FTexture *tex = TexMan[i];
			if (tex != NULL)
			{
            textureList.AllowScale(tex->UseType != FTexture::TEX_Sprite);
            textureList.GetTexture(tex);
            priority = hitlist[i] * 1.f / maxHit;
            //priority = 0.f;
            //glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_PRIORITY, priority);
            FAnimDef *anim = GL_GetAnimForLump(i);
            if (anim != NULL)
            {
               GL_PrecacheAnimdef(anim, priority);
            }
            FShader *shader = GL_ShaderForTexture(tex);
            if (shader)
            {
               GL_PrecacheShader(shader, priority);
            }
			}
         C_SetTicker (++count, true);
		}
	}
   textureList.AllowScale(true);
   C_InitTicker (NULL, 0);

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
   Next = NULL;
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


void FTexture::Init()
{
}


void FTexture::ClearGLTextures()
{
   unsigned int i, j, size;
   FTexture *tex;

   for (i = 0; i < TrackedTextures.Size(); i++)
   {
      tex = TrackedTextures[i];
      size = tex->glData.Size();
/*
      for (j = 0; j < size; j++)
      {
         delete (tex->glData[j]);
      }
*/
      tex->glData.Clear();
   }

   TrackedTextures.Clear();
}


void FTexture::TrackTexture(FTexture *tex)
{
   FTexture *first = RootTexture;

   if (tex->glData.Size()) return; // already tracked

   TrackedTextures.Push(tex);
}


FTextureGLData::FTextureGLData()
{
   glTex = 0;
   translation = NULL;
   isAlpha = false;
   isTransparent = false;
   cx = cy = 1.f;
}


FTextureGLData::~FTextureGLData()
{
   if (glTex != 0) glDeleteTextures(1, &glTex);
}


TextureList::TextureList()
{
   TrackedTextures.Clear();
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

      screen->CalcGamma(Gamma, m_gammaTable);

      glGetIntegerv(GL_MAX_TEXTURE_SIZE, &m_maxTextureSize);
      Printf( PRINT_OPENGL, "Max texture size %d\n", m_maxTextureSize);

      m_loadAsAlpha = false;
      m_translation = NULL;
      m_translationShort = 0;
      m_savegameTex = 0;
      m_allowScale = true;

      m_initialized = true;
   }
}


void TextureList::ShutDown()
{
   if (m_initialized)
   {
      this->Clear();
      this->deleteSavegameTexture();

      ilShutDown();

      m_initialized = false;
   }
}


void TextureList::Clear()
{
   //Printf("ZGL: Clearing textures...\n");

   FTexture::ClearGLTextures();

   m_translation = NULL;
   m_translationShort = 0;
}


void TextureList::colorOutlines(byte *img, int width, int height)
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


int TextureList::checkExternalFile(char *fileName, bool isPatch)
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
      sprintf(texType, "textures");
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


byte *TextureList::loadExternalFile(char *fileName, int *width, int *height, bool gameSpecific, bool isPatch)
{
   char fn[80], texType[16], checkName[32];
   byte *buffer = NULL;

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
      sprintf(texType, "textures");
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


byte *TextureList::loadFile(char *fileName, int *width, int *height)
{
   byte *buffer = NULL;
   byte *data;
   ILuint imgID;
   int x, y, index;

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
   buffer = new byte[*width * *height * 4];
   data = ilGetData();

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

   ilDeleteImages(1, &imgID);

   return buffer;
}


byte *TextureList::loadFromLump(char *lumpName, int *width, int *height)
{
   byte *buffer = NULL, *data;
   ILuint imgID;
   void *lumpData;
   int lumpNum, x, y, index;
   FMemLump memLump;

   lumpNum = Wads.CheckNumForName(lumpName);
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
   buffer = new byte[*width * *height * 4];
   data = ilGetData();

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
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GetTextureMode());
}


void TextureList::Purge()
{
   this->Clear();
   screen->CalcGamma(Gamma, m_gammaTable);
   GL_PrecacheTextures();
}


void TextureList::BindGLTexture(int texId)
{
   if (m_lastBoundTexture != texId)
   {
      glBindTexture(GL_TEXTURE_2D, texId);
      m_lastBoundTexture = texId;
   }
}


byte *TextureList::hqresize(byte *buffer, int *width, int *height)
{
   byte *img;
   CImage hqimage;
   int w, h;

   w = *width;
   h = *height;

   hqimage.SetImage(buffer, w, h, 32);
   delete[] buffer;
   hqimage.Convert32To17();

   switch (gl_texture_hqresize)
   {
   case 3:
      w *= 4;
      h *= 4;
      img = new byte[w * h * 4];
      hq4x_32((int*)hqimage.m_pBitmap, img, hqimage.m_Xres, hqimage.m_Yres, w * 4);
      break;
   case 2:
      w *= 3;
      h *= 3;
      img = new byte[w * h * 4];
      hq3x_32((int*)hqimage.m_pBitmap, img, hqimage.m_Xres, hqimage.m_Yres, w * 4);
      break;
   default:
      w *= 2;
      h *= 2;
      img = new byte[w * h * 4];
      hq2x_32((int*)hqimage.m_pBitmap, img, hqimage.m_Xres, hqimage.m_Yres, w * 4);
      break;
   }

   hqimage.Destroy();

   *width = w;
   *height = h;

   return img;
}


void TextureList::addLuminanceNoise(byte *buffer, int width, int height)
{
   // add some random luminance noise to the input image
   int x, y, index;
   byte r, g, b;
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
         buffer[index + 0] = (byte)clamp<int>((int)(r * luminance), 0, 255);
         buffer[index + 1] = (byte)clamp<int>((int)(g * luminance), 0, 255);
         buffer[index + 2] = (byte)clamp<int>((int)(b * luminance), 0, 255);
      }
   }
}


byte *TextureList::scaleImage(byte *buffer, int *width, int *height, bool highQuality)
{
   int ow, oh, w, h, x, y, indexSrc, indexDst, sx, sy;
   float scaleX, scaleY;
   bool forceScale = false;
   byte *img;

   ow = *width;
   oh = *height;

   w = CeilPow2(ow);
   h = CeilPow2(oh);

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

   img = new byte[w * h * 4];
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


unsigned long TextureList::calcAverageColor(byte *buffer, int width, int height)
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
   const byte *img;
   byte *buffer, ci, transIndex;
   int neededSize;
   int x, y, indexSrc, indexDst, nw, nh, origW, origH, gs;
   PalEntry *p;
   GLuint texID;
   GLint format;
   float c;
   char texName[32], fileName[32];
   bool imgLoaded = false; // this shows when a hires texture is loaded
   bool isPatch = false;
   remap_tex_t *remap;
   bool isTransparent;
   unsigned long averageColor;

   isPatch |= (tex->UseType == FTexture::TEX_Sprite);
   isPatch |= (tex->UseType == FTexture::TEX_MiscPatch);
   isPatch |= (tex->UseType == FTexture::TEX_FontChar);

   nw = tex->GetWidth();
   nh = tex->GetHeight();

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
      if (tex->UseType == FTexture::TEX_Flat)
      {
         sprintf(texName, "flat-%s", tex->Name);
      }
      else
      {
         sprintf(texName, "%s", tex->Name);
      }

      // ooh, jpg support :)
      sprintf(fileName, "%s.jpg", texName);
      if (!imgLoaded && (gs = checkExternalFile(fileName, isPatch)))
      {
         buffer = loadExternalFile(fileName, &nw, &nh, gs == 2, isPatch);
         imgLoaded = true;
      }
      sprintf(fileName, "%s.png", texName);
      if (!imgLoaded && (gs = checkExternalFile(fileName, isPatch)))
      {
         buffer = loadExternalFile(fileName, &nw, &nh, gs == 2, isPatch);
         imgLoaded = true;
      }
      sprintf(fileName, "%s.tga", texName);
      if (!imgLoaded && (gs = checkExternalFile(fileName, isPatch)))
      {
         buffer = loadExternalFile(fileName, &nw, &nh, gs == 2, isPatch);
         imgLoaded = true;
      }
   }

   if (!imgLoaded)
   {
      neededSize = nw * nh * 4;
      img = tex->GetPixels();
      buffer = new byte[neededSize];
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
               if (m_loadAsAlpha)
               {
                  ci = img[indexSrc];
                  buffer[indexDst + 0] = 0xff;
                  buffer[indexDst + 1] = 0xff;
                  buffer[indexDst + 2] = 0xff;
                  buffer[indexDst + 3] = ci;
               }
               else
               {
                  ci = img[indexSrc];

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

                  p = &GPalette.BaseColors[ci];
                  c = m_gammaTable[p->r] * gl_texture_color_multiplier;
                  buffer[indexDst + 0] = (byte)clamp<int>(c, 0, 255);
                  c = m_gammaTable[p->g] * gl_texture_color_multiplier;
                  buffer[indexDst + 1] = (byte)clamp<int>(c, 0, 255);
                  c = m_gammaTable[p->b] * gl_texture_color_multiplier;
                  buffer[indexDst + 2] = (byte)clamp<int>(c, 0, 255);
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
         for (y = 0; y < nh; y++)
         {
            for (x = 0; x < nw; x++)
            {
               if (buffer[((x + (y * nw)) * 4) + 3] < 0xff)
               {
                  isTransparent = true;
               }
            }
         }
      }
   }

   //isTransparent = tex->bMasked;

   averageColor = calcAverageColor(buffer, nw, nh);
   colorOutlines(buffer, nw, nh);
   m_workingData->averageColor = averageColor;
   m_workingData->isTransparent = isTransparent;
   m_workingData->isAlpha = m_loadAsAlpha;
   m_workingData->translation = m_translation;

   // only resample non-hires images and non-alpha images
   if (gl_texture_hqresize && !m_loadAsAlpha && !imgLoaded)
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

   gluBuild2DMipmaps(GL_TEXTURE_2D, format, nw, nh, GL_RGBA, GL_UNSIGNED_BYTE, buffer);

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
   unsigned int i, size;
   FTextureGLData *data = NULL;

   size = tex->glData.Size();

   // add it to the "in use" list
   if (size == 0) FTexture::TrackTexture(tex);

   for (i = 0; i < size; i++)
   {
      if (tex->glData[i]->translation == m_translation)
      {
         m_workingData = tex->glData[i];
         return;
      }
   }

   // no texture matching current translation found, so add the new texture
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

   BindTexture(tex);
}


void TextureList::BindTexture(const char *texName)
{
   BindTexture(TexMan[texName]);
}


void TextureList::BindTexture(FTexture *tex)
{
   GetTexture(tex);
   BindGLTexture(m_workingData->glTex);

   if (tex->CheckModified())
   {
      updateTexture(tex);
   }

   SetupTexparams();
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


void TextureList::LoadAlpha(bool la)
{
   m_loadAsAlpha = la;
}


void TextureList::SetTranslation(const byte *trans)
{
   m_translation = (byte *)trans;
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
   const byte *img;
   byte *buffer, ci, transIndex;
   PalEntry *p;
   float c;
   bool isTransparent;

   isTransparent = false;
   transIndex = 0x00;

   img = tex->GetPixels();
   width = tex->GetWidth();
   height = tex->GetHeight();

   buffer = new byte[width * height * 4];

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

            p = &GPalette.BaseColors[ci];
            c = m_gammaTable[p->r] * gl_texture_color_multiplier;
            buffer[indexDst + 0] = (byte)clamp<int>(c, 0, 255);
            c = m_gammaTable[p->g] * gl_texture_color_multiplier;
            buffer[indexDst + 1] = (byte)clamp<int>(c, 0, 255);
            c = m_gammaTable[p->b] * gl_texture_color_multiplier;
            buffer[indexDst + 2] = (byte)clamp<int>(c, 0, 255);
         }
      }
   }

   // don't resample constantly changing textures?
   //  hmm, seems fast enough...
   if (gl_texture_hqresize && !m_loadAsAlpha)
   {
      buffer = hqresize(buffer, &width, &height);
   }

   colorOutlines(buffer, width, height);
   this->AllowScale(false);
   buffer = scaleImage(buffer, &width, &height, false);
   this->AllowScale(true);

   //m_workingData->isTransparent = isTransparent;

	gluBuild2DMipmaps(GL_TEXTURE_2D, getTextureFormat(tex), width, height, GL_RGBA, GL_UNSIGNED_BYTE, buffer);

   delete[] buffer;
}


void TextureList::UpdateForTranslation(byte *trans)
{
   byte *oldTrans;
   FTexture *tex = RootTexture;
   unsigned int i;

   oldTrans = m_translation;
   m_translation = trans;
   while (tex)
   {
      for (i = 0; i < tex->glData.Size(); i++)
      {
         if (tex->glData[i]->translation == m_translation)
         {
            BindGLTexture(tex->glData[i]->glTex);
            updateTexture(tex);
         }
      }
      tex = tex->Next;
   }
   m_translation = oldTrans;
}

#if 0
void TextureList::recursiveUpdateTranslation(TextureListNode *node, byte *trans)
{
   bool wasAlpha;

   if (node)
   {
      if (node->left)
      {
         recursiveUpdateTranslation(node->left, trans);
      }
      if (node->translation == trans)
      {
         wasAlpha = m_loadAsAlpha;

         m_loadAsAlpha = node->isAlpha;

         BindGLTexture(node->glTex);
         updateTexture(node->Tex);
         node->isTransparent = isTransparent;

         m_loadAsAlpha = wasAlpha;
      }
      if (node->right)
      {
         recursiveUpdateTranslation(node->right, trans);
      }
   }
}
#endif


void TextureList::SetSavegameTexture(byte *img, int width, int height)
{
   int x, y, format, index;
   byte *buffer, ci;
   PalEntry *p;

   deleteSavegameTexture();

   if (img)
   {
      buffer = new byte[width * height * 4];

      for (y = 0; y < height; y++)
      {
         for (x = 0; x < width; x++)
         {
            index = x + (y * width);
            ci = img[index];
            p = &GPalette.BaseColors[ci];
            buffer[(index * 4) + 0] = p->r;
            buffer[(index * 4) + 1] = p->g;
            buffer[(index * 4) + 2] = p->b;
            buffer[(index * 4) + 3] = 255;
         }
      }

      buffer = scaleImage(buffer, &width, &height, false);

      glGenTextures(1, &m_savegameTex);
      BindGLTexture(m_savegameTex);
      glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

      format = getTextureFormat(NULL);

      glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, buffer);

      delete[] buffer;
   }
}


void TextureList::BindSavegameTexture()
{
   BindGLTexture(m_savegameTex);
}


void TextureList::deleteSavegameTexture()
{
   if (m_savegameTex)
   {
      glDeleteTextures(1, &m_savegameTex);
      m_savegameTex = 0;
   }
}


void TextureList::GrabFB()
{
   byte *fb, *scaledfb;
   int width, height, format;

   width = 256;
   height = 256;

   fb = new byte[screen->GetWidth() * screen->GetHeight() * 4];
   scaledfb = new byte[width * height * 4];

   glReadPixels(0, 0, screen->GetWidth(), screen->GetHeight(), GL_RGBA, GL_UNSIGNED_BYTE, fb);
   gluScaleImage(GL_RGBA, screen->GetWidth(), screen->GetHeight(), GL_UNSIGNED_BYTE, fb, width, height, GL_UNSIGNED_BYTE, scaledfb);
   delete[] fb;

   BindSavegameTexture();
   glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
   format = getTextureFormat(NULL);
   glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaledfb);
   delete[] scaledfb;

   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GetTextureModeMag());
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GetTextureModeMag());
}


byte *TextureList::ReduceFramebufferToPalette(int width, int height)
{
   int x, y, indexSrc, indexDst;
   byte *img, *fb, *scaledfb;
   PalEntry p;

   fb = new byte[screen->GetWidth() * screen->GetHeight() * 3];
   scaledfb = new byte[width * height * 3];
   img = new byte[width * height];

   glReadPixels(0, 0, screen->GetWidth(), screen->GetHeight(), GL_RGB, GL_UNSIGNED_BYTE, fb);
   gluScaleImage(GL_RGB, screen->GetWidth(), screen->GetHeight(), GL_UNSIGNED_BYTE, fb, width, height, GL_UNSIGNED_BYTE, scaledfb);
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

void TextureList::AllowScale(bool as)
{
   m_allowScale = as;
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
