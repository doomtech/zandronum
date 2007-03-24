/*
** gl_shaders.cpp
** Routines parsing/managing texture shaders.
**
**---------------------------------------------------------------------------
** Copyright 2003 Timothy Stump
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
#include "glext.h"

#define USE_WINDOWS_DWORD
#include "c_cvars.h"
#include "c_dispatch.h"
#include "zgl_main.h"
#include "sc_man.h"
#include "w_wad.h"
#include "v_text.h"

#include "gi.h"
#include "templates.h"


#if 0
 #define LOG1(msg, i) { FILE *f = fopen("shaders.log", "a");  fprintf(f, msg, i);  fclose(f); }
#else
 #define LOG1
#endif

#define MAX(a, b) (a > b ? a : b)
#define MIN(a, b) (a < b ? a : b)

extern PFNGLGENPROGRAMSARBPROC glGenProgramsARB;
extern PFNGLDELETEPROGRAMSARBPROC glDeleteProgramsARB;
extern PFNGLBINDPROGRAMARBPROC glBindProgramARB;
extern PFNGLPROGRAMSTRINGARBPROC glProgramStringARB;
extern PFNGLPROGRAMENVPARAMETER4FARBPROC glProgramEnvParameter4fARB;
extern PFNGLMULTITEXCOORD2FARBPROC glMultiTexCoord2fARB;


EXTERN_CVAR(Int, vid_renderer)
CVAR(Bool, gl_texture_useshaders, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

TArray<FShader *> Shaders[NUM_ShaderClasses];
TArray<FShader *> ShaderLookup[NUM_ShaderClasses];
TArray<VertexProgram *> VertexPrograms;
int ShaderDepth;

extern int viewpitch;


FShaderLayer::FShaderLayer()
{
   sprintf(name, "");

   animate = false;
   emissive = false;
   blendFuncSrc = GL_SRC_ALPHA;
   blendFuncDst = GL_ONE_MINUS_SRC_ALPHA;
   offsetX = 0.f;
   offsetY = 0.f;
   centerX = 0.0f;
   centerY = 0.0f;
   rotate = 0.f;
   rotation = 0.f;
   adjustX.SetParams(0.f, 0.f, 0.f);
   adjustY.SetParams(0.f, 0.f, 0.f);
   scaleX.SetParams(1.f, 1.f, 0.f);
   scaleY.SetParams(1.f, 1.f, 0.f);
   alpha.SetParams(1.f, 1.f, 0.f);
   r.SetParams(1.f, 1.f, 0.f);
   g.SetParams(1.f, 1.f, 0.f);
   b.SetParams(1.f, 1.f, 0.f);
   flags = 0;
   layerMask = NULL;
   texgen = SHADER_TexGen_None;
   warp = false;
}


FShaderLayer::FShaderLayer(const FShaderLayer &layer)
{
   sprintf(name, "%s", layer.name);

   animate = layer.animate;
   emissive = layer.emissive;
   adjustX = layer.adjustX;
   adjustY = layer.adjustY;
   blendFuncSrc = layer.blendFuncSrc;
   blendFuncDst = layer.blendFuncDst;
   offsetX = layer.offsetX;
   offsetY = layer.offsetY;
   centerX = layer.centerX;
   centerY = layer.centerX;
   rotate = layer.rotate;
   rotation = layer.rotation;
   scaleX = layer.scaleX;
   scaleY = layer.scaleY;
   vectorX = layer.vectorX;
   vectorY = layer.vectorY;
   alpha = layer.alpha;
   r = layer.r;
   g = layer.g;
   b = layer.b;
   flags = layer.flags;
   if (layer.layerMask)
   {
      layerMask = new FShaderLayer(*(layer.layerMask));
   }
   else
   {
      layerMask = NULL;
   }
   texgen = layer.texgen;
   warp = layer.warp;
}


FShaderLayer::~FShaderLayer()
{
   if (layerMask)
   {
      delete layerMask;
      layerMask = NULL;
   }
}


void FShaderLayer::Update(float diff)
{
   r.Update(diff);
   g.Update(diff);
   b.Update(diff);
   alpha.Update(diff);
   vectorY.Update(diff);
   vectorX.Update(diff);
   scaleX.Update(diff);
   scaleY.Update(diff);
   adjustX.Update(diff);
   adjustY.Update(diff);

   offsetX += vectorX * diff;
   if (offsetX >= 1.f) offsetX -= 1.f;
   if (offsetX < 0.f) offsetX += 1.f;

   offsetY += vectorY * diff;
   if (offsetY >= 1.f) offsetY -= 1.f;
   if (offsetY < 0.f) offsetY += 1.f;

   rotation += rotate * diff;
   if (rotation > 360.f) rotation -= 360.f;
   if (rotation < 0.f) rotation += 360.f;
}


FShader::FShader()
{
   layers.Clear();
   lastUpdate = 0;
   sprintf(name, "");
   sprintf(detailTex, "");
   compatibility = SHADER_Generic;
}


FShader::FShader(const FShader &shader)
{
   unsigned int i;

   for (i = 0; i < shader.layers.Size(); i++)
   {
      layers.Push(new FShaderLayer(*shader.layers[i]));
   }

   lastUpdate = shader.lastUpdate;

   sprintf(name, "%s", shader.name);
   sprintf(detailTex, "%s", shader.detailTex);
   compatibility = shader.compatibility;
}


FShader::~FShader()
{
   unsigned int i;

   for (i = 0; i < layers.Size(); i++)
   {
      delete layers[i];
   }

   layers.Clear();
}


bool FShader::operator== (const FShader &shader)
{
   if (shader.compatibility == compatibility)
   {
      if (strcmp(shader.name, name) != 0)
      {
         return false;
      }
   }
   else
   {
      return false;
   }

   return true;
}

VertexProgram::VertexProgram()
{
   name = NULL;
}


VertexProgram::~VertexProgram()
{
   delete[] name;
}


int GL_ShaderIndexForShader(FShader *shader)
{
   unsigned int i;

   for (i = 0; i < Shaders[shader->compatibility].Size(); i++)
   {
      if ((*Shaders[shader->compatibility][i]) == (*shader))
      {
         return i;
      }
   }

   return -1;
}


void GL_CheckShaderDepth()
{
   char *tmp;

   tmp = strstr(sc_String, "{");
   while (tmp)
   {
      ShaderDepth++;
      tmp++;
      tmp = strstr(tmp, "{");
   }

   tmp = strstr(sc_String, "}");
   while (tmp)
   {
      ShaderDepth--;
      tmp++;
      tmp = strstr(tmp, "}");
   }
}


unsigned int GL_GetBlendFromString(char *string)
{
   if (stricmp("GL_ZERO", string) == 0)
   {
      return GL_ZERO;
   }
   else if (stricmp("GL_ONE", string) == 0)
   {
      return GL_ONE;
   }
   else if (stricmp("GL_DST_COLOR", string) == 0)
   {
      return GL_DST_COLOR;
   }
   else if (stricmp("GL_ONE_MINUS_DST_COLOR", string) == 0)
   {
      return GL_ONE_MINUS_DST_COLOR;
   }
   else if (stricmp("GL_SRC_ALPHA", string) == 0)
   {
      return GL_SRC_ALPHA;
   }
   else if (stricmp("GL_ONE_MINUS_SRC_ALPHA", string) == 0)
   {
      return GL_ONE_MINUS_SRC_ALPHA;
   }
   else if (stricmp("GL_DST_ALPHA", string) == 0)
   {
      return GL_DST_ALPHA;
   }
   else if (stricmp("GL_ONE_MINUS_DST_ALPHA", string) == 0)
   {
      return GL_ONE_MINUS_DST_ALPHA;
   }
   else if (stricmp("GL_SRC_ALPHA_SATURATE", string) == 0)
   {
      return GL_SRC_ALPHA_SATURATE;
   }
   else if (stricmp("GL_SRC_COLOR", string) == 0)
   {
      return GL_SRC_COLOR;
   }
   else if (stricmp("GL_ONE_MINUS_SRC_COLOR", string) == 0)
   {
      return GL_ONE_MINUS_SRC_COLOR;
   }
   return GL_ONE;
}

static const char *LayerTags[]=
{
   "alpha",
   "animate",
   "blendFunc",
   "color",
   "center",
   "emissive",
   "mask",
   "offset",
   "offsetFunc",
   "rotate",
   "rotation",
   "scale",
   "scaleFunc",
   "texgen",
   "vector",
   "vectorFunc",
   "warp",
   NULL
};

enum {
   LAYERTAG_ALPHA,
   LAYERTAG_ANIMATE,
   LAYERTAG_BLENDFUNC,
   LAYERTAG_COLOR,
   LAYERTAG_CENTER,
   LAYERTAG_EMISSIVE,
   LAYERTAG_MASK,
   LAYERTAG_OFFSET,
   LAYERTAG_OFFSETFUNC,
   LAYERTAG_ROTATE,
   LAYERTAG_ROTATION,
   LAYERTAG_SCALE,
   LAYERTAG_SCALEFUNC,
   LAYERTAG_TEXGEN,
   LAYERTAG_VECTOR,
   LAYERTAG_VECTORFUNC,
   LAYERTAG_WARP
};

static const char *CycleTags[]=
{
   "linear",
   "sin",
   "cos",
   "sawtooth",
   "square",
   NULL
};


bool GL_ParseLayer(FShader *shader)
{
   unsigned int i;
   float start, end, cycle, r1, r2, g1, g2, b1, b2;
   FShaderLayer sink;
   FShader *tempShader;
   int type, incomingDepth;
   bool overrideBlend = false;

   incomingDepth = ShaderDepth;

   if (SC_GetString())
   {
      sprintf(sink.name, "%s", sc_String);
      for (i = 0; i < strlen(sc_String); i++)
      {
         sink.name[i] = toupper(sink.name[i]);
      }
      SC_GetString();
      GL_CheckShaderDepth();
      while (ShaderDepth > incomingDepth)
      {
         if (SC_GetString() == false)
         {
            ShaderDepth = incomingDepth;
            Printf("Layer parse error: %s\n", sink.name);
            return false;
         }

         if ((type = SC_MatchString(LayerTags)) != -1)
         {
            switch (type)
            {
            case LAYERTAG_ALPHA:
               SC_GetString();
               if (SC_Compare("cycle"))
               {
                  sink.alpha.ShouldCycle(true);
                  SC_GetString();
                  switch (SC_MatchString(CycleTags))
                  {
                  case CYCLE_Sin:
                     sink.alpha.SetCycleType(CYCLE_Sin);
                     break;
                  case CYCLE_Cos:
                     sink.alpha.SetCycleType(CYCLE_Cos);
                     break;
                  case CYCLE_SawTooth:
                     sink.alpha.SetCycleType(CYCLE_SawTooth);
                     break;
                  case CYCLE_Square:
                     sink.alpha.SetCycleType(CYCLE_Square);
                     break;
                  case CYCLE_Linear:
                     sink.alpha.SetCycleType(CYCLE_Linear);
                     break;
                  default:
                     // cycle type missing, back script up and default to CYCLE_Linear
                     sink.alpha.SetCycleType(CYCLE_Linear);
                     SC_UnGet();
                     break;
                  }
                  SC_GetFloat();
                  start = sc_Float;
                  SC_GetFloat();
                  end = sc_Float;
                  SC_GetFloat();
                  cycle = sc_Float;

                  sink.alpha.SetParams(start, end, cycle);
               }
               else
               {
                  start = static_cast<float>(atof(sc_String));
                  sink.alpha.SetParams(start, start, 0.f);
               }
               break;
            case LAYERTAG_ANIMATE:
               SC_GetString();
               sink.animate = SC_Compare("true") == TRUE;
               break;
            case LAYERTAG_BLENDFUNC:
               SC_GetString();
               sink.blendFuncSrc = GL_GetBlendFromString(sc_String);
               SC_GetString();
               sink.blendFuncDst = GL_GetBlendFromString(sc_String);
               overrideBlend = true;
               break;
            case LAYERTAG_COLOR:
               SC_GetString();
               if (SC_Compare("cycle"))
               {
                  sink.r.ShouldCycle(true);
                  sink.g.ShouldCycle(true);
                  sink.b.ShouldCycle(true);
                  // get color1
                  SC_GetString();

                  switch (SC_MatchString(CycleTags))
                  {
                  case CYCLE_Sin:
                     sink.r.SetCycleType(CYCLE_Sin);
                     sink.g.SetCycleType(CYCLE_Sin);
                     sink.b.SetCycleType(CYCLE_Sin);
                     break;
                  case CYCLE_Cos:
                     sink.r.SetCycleType(CYCLE_Cos);
                     sink.g.SetCycleType(CYCLE_Cos);
                     sink.b.SetCycleType(CYCLE_Cos);
                     break;
                  case CYCLE_SawTooth:
                     sink.r.SetCycleType(CYCLE_SawTooth);
                     sink.g.SetCycleType(CYCLE_SawTooth);
                     sink.b.SetCycleType(CYCLE_SawTooth);
                     break;
                  case CYCLE_Square:
                     sink.r.SetCycleType(CYCLE_Square);
                     sink.g.SetCycleType(CYCLE_Square);
                     sink.b.SetCycleType(CYCLE_Square);
                     break;
                  case CYCLE_Linear:
                     sink.r.SetCycleType(CYCLE_Linear);
                     sink.g.SetCycleType(CYCLE_Linear);
                     sink.b.SetCycleType(CYCLE_Linear);
                     break;
                  default:
                     // cycle type missing, back script up and default to CYCLE_Linear
                     sink.r.SetCycleType(CYCLE_Linear);
                     sink.g.SetCycleType(CYCLE_Linear);
                     sink.b.SetCycleType(CYCLE_Linear);
                     SC_UnGet();
                     break;
                  }

                  SC_GetFloat();
                  r1 = sc_Float;
                  SC_GetFloat();
                  g1 = sc_Float;
                  SC_GetFloat();
                  b1 = sc_Float;

                  // get color2
                  SC_GetFloat();
                  r2 = sc_Float;
                  SC_GetFloat();
                  g2 = sc_Float;
                  SC_GetFloat();
                  b2 = sc_Float;

                  // get cycle time
                  SC_GetFloat();
                  cycle = sc_Float;

                  sink.r.SetParams(r1, r2, cycle);
                  sink.g.SetParams(g1, g2, cycle);
                  sink.b.SetParams(b1, b2, cycle);
               }
               else
               {
                  r1 = static_cast<float>(atof(sc_String));
                  SC_GetFloat();
                  g1 = sc_Float;
                  SC_GetFloat();
                  b1 = sc_Float;

                  sink.r.SetParams(r1, r1, 0.f);
                  sink.g.SetParams(g1, g1, 0.f);
                  sink.b.SetParams(b1, b1, 0.f);
               }
               break;
            case LAYERTAG_CENTER:
               SC_GetFloat();
               sink.centerX = sc_Float;
               SC_GetFloat();
               sink.centerY = sc_Float;
               break;
            case LAYERTAG_EMISSIVE:
               SC_GetString();
               sink.emissive = SC_Compare("true") == TRUE;
               break;
            case LAYERTAG_OFFSET:
               SC_GetString();
               if (SC_Compare("cycle"))
               {
                  sink.adjustX.ShouldCycle(true);
                  sink.adjustY.ShouldCycle(true);

                  SC_GetFloat();
                  r1 = sc_Float;
                  SC_GetFloat();
                  r2 = sc_Float;

                  SC_GetFloat();
                  g1 = sc_Float;
                  SC_GetFloat();
                  g2 = sc_Float;

                  SC_GetFloat();
                  cycle = sc_Float;

                  sink.offsetX = r1;
                  sink.offsetY = r2;

                  sink.adjustX.SetParams(0.f, g1 - r1, cycle);
                  sink.adjustY.SetParams(0.f, g2 - r2, cycle);
               }
               else
               {
                  SC_UnGet();
                  SC_GetFloat();
                  sink.offsetX = sc_Float;
                  SC_GetFloat();
                  sink.offsetY = sc_Float;
               }
               break;
            case LAYERTAG_OFFSETFUNC:
               SC_GetString();
               switch (SC_MatchString(CycleTags))
               {
               case CYCLE_Sin:
                  sink.adjustX.SetCycleType(CYCLE_Sin);
                  break;
               case CYCLE_Cos:
                  sink.adjustX.SetCycleType(CYCLE_Cos);
                  break;
               case CYCLE_SawTooth:
                  sink.adjustX.SetCycleType(CYCLE_SawTooth);
                  break;
               case CYCLE_Square:
                  sink.adjustX.SetCycleType(CYCLE_Square);
                  break;
               case CYCLE_Linear:
               default:
                  sink.adjustX.SetCycleType(CYCLE_Linear);
                  break;
               }
               SC_GetString();
               switch (SC_MatchString(CycleTags))
               {
               case CYCLE_Sin:
                  sink.adjustY.SetCycleType(CYCLE_Sin);
                  break;
               case CYCLE_Cos:
                  sink.adjustY.SetCycleType(CYCLE_Cos);
                  break;
               case CYCLE_SawTooth:
                  sink.adjustY.SetCycleType(CYCLE_SawTooth);
                  break;
               case CYCLE_Square:
                  sink.adjustY.SetCycleType(CYCLE_Square);
                  break;
               case CYCLE_Linear:
               default:
                  sink.adjustY.SetCycleType(CYCLE_Linear);
                  break;
               }
               break;
            case LAYERTAG_MASK:
               tempShader = new FShader();
               GL_ParseLayer(tempShader);
               sink.layerMask = new FShaderLayer(*tempShader->layers[0]);
               delete tempShader;
               break;
            case LAYERTAG_ROTATE:
               SC_GetFloat();
               sink.rotate = sc_Float;
               break;
            case LAYERTAG_ROTATION:
               SC_GetFloat();
               sink.rotation = sc_Float;
               break;
            case LAYERTAG_SCALE:
               SC_GetString();
               if (SC_Compare("cycle"))
               {
                  sink.scaleX.ShouldCycle(true);
                  sink.scaleY.ShouldCycle(true);

                  SC_GetFloat();
                  r1 = sc_Float;
                  SC_GetFloat();
                  r2 = sc_Float;

                  SC_GetFloat();
                  g1 = sc_Float;
                  SC_GetFloat();
                  g2 = sc_Float;

                  SC_GetFloat();
                  cycle = sc_Float;

                  sink.scaleX.SetParams(r1, g1, cycle);
                  sink.scaleY.SetParams(r2, g2, cycle);
               }
               else
               {
                  SC_UnGet();
                  SC_GetFloat();
                  sink.scaleX.SetParams(sc_Float, sc_Float, 0.f);
                  SC_GetFloat();
                  sink.scaleY.SetParams(sc_Float, sc_Float, 0.f);
               }
               break;
            case LAYERTAG_SCALEFUNC:
               SC_GetString();
               switch (SC_MatchString(CycleTags))
               {
               case CYCLE_Sin:
                  sink.scaleX.SetCycleType(CYCLE_Sin);
                  break;
               case CYCLE_Cos:
                  sink.scaleX.SetCycleType(CYCLE_Cos);
                  break;
               case CYCLE_SawTooth:
                  sink.scaleX.SetCycleType(CYCLE_SawTooth);
                  break;
               case CYCLE_Square:
                  sink.scaleX.SetCycleType(CYCLE_Square);
                  break;
               case CYCLE_Linear:
               default:
                  sink.scaleX.SetCycleType(CYCLE_Linear);
                  break;
               }
               SC_GetString();
               switch (SC_MatchString(CycleTags))
               {
               case CYCLE_Sin:
                  sink.scaleY.SetCycleType(CYCLE_Sin);
                  break;
               case CYCLE_Cos:
                  sink.scaleY.SetCycleType(CYCLE_Cos);
                  break;
               case CYCLE_SawTooth:
                  sink.scaleY.SetCycleType(CYCLE_SawTooth);
                  break;
               case CYCLE_Square:
                  sink.scaleY.SetCycleType(CYCLE_Square);
                  break;
               case CYCLE_Linear:
               default:
                  sink.scaleY.SetCycleType(CYCLE_Linear);
                  break;
               }
               break;
            case LAYERTAG_TEXGEN:
               SC_GetString();
               if (SC_Compare("sphere"))
               {
                  sink.texgen = SHADER_TexGen_Sphere;
               }
               else
               {
                  sink.texgen = SHADER_TexGen_None;
               }
               break;
            case LAYERTAG_VECTOR:
               SC_GetString();
               if (SC_Compare("cycle"))
               {
                  sink.vectorX.ShouldCycle(true);
                  sink.vectorY.ShouldCycle(true);

                  SC_GetFloat();
                  r1 = sc_Float;
                  SC_GetFloat();
                  g1 = sc_Float;
                  SC_GetFloat();
                  r2 = sc_Float;
                  SC_GetFloat();
                  g2 = sc_Float;
                  SC_GetFloat();
                  cycle = sc_Float;

                  sink.vectorX.SetParams(r1, r2, cycle);
                  sink.vectorY.SetParams(g1, g2, cycle);
               }
               else
               {
                  start = static_cast<float>(atof(sc_String));
                  sink.vectorX.SetParams(start, start, 0.f);
                  SC_GetFloat();
                  start = sc_Float;
                  sink.vectorY.SetParams(start, start, 0.f);
               }
               break;
            case LAYERTAG_VECTORFUNC:
               SC_GetString();
               switch (SC_MatchString(CycleTags))
               {
               case CYCLE_Sin:
                  sink.vectorX.SetCycleType(CYCLE_Sin);
                  break;
               case CYCLE_Cos:
                  sink.vectorX.SetCycleType(CYCLE_Cos);
                  break;
               case CYCLE_SawTooth:
                  sink.vectorX.SetCycleType(CYCLE_SawTooth);
                  break;
               case CYCLE_Square:
                  sink.vectorX.SetCycleType(CYCLE_Square);
                  break;
               case CYCLE_Linear:
               default:
                  sink.vectorX.SetCycleType(CYCLE_Linear);
                  break;
               }
               SC_GetString();
               switch (SC_MatchString(CycleTags))
               {
               case CYCLE_Sin:
                  sink.vectorY.SetCycleType(CYCLE_Sin);
                  break;
               case CYCLE_Cos:
                  sink.vectorY.SetCycleType(CYCLE_Cos);
                  break;
               case CYCLE_SawTooth:
                  sink.vectorY.SetCycleType(CYCLE_SawTooth);
                  break;
               case CYCLE_Square:
                  sink.vectorY.SetCycleType(CYCLE_Square);
                  break;
               case CYCLE_Linear:
               default:
                  sink.vectorY.SetCycleType(CYCLE_Linear);
                  break;
               }
               break;
            case LAYERTAG_WARP:
               SC_GetString();
               sink.warp = SC_Compare("true") == TRUE;
               break;
            default:
               break;
            }
         }
         else
         {
            GL_CheckShaderDepth();
         }
      }

      shader->layers.Push(new FShaderLayer(sink));

      return true;
   }

   return false;
}


static const char *ShaderTags[]=
{
   "compatibility",
   "detailTex",
   "layer",
   NULL
};

enum {
   SHADERTAG_COMPATIBILITY,
   SHADERTAG_DETAILTEX,
   SHADERTAG_LAYER
};


bool GL_ParseShader()
{
   unsigned int i;
   int temp, type;
   FShader sink;
   TArray<char *> names;
   char *name;

   ShaderDepth = 0;

   if (SC_GetString())
   {
      do
      {
         name = new char[strlen(sc_String) + 1];
         sprintf(name, "%s", sc_String);
         for (i = 0; i < strlen(sc_String); i++)
         {
            name[i] = toupper(name[i]);
         }
         names.Push(name);
         SC_GetString();
      } while (strstr(sc_String, "{") == NULL);

      GL_CheckShaderDepth();

      while (ShaderDepth > 0)
      {
         if (SC_GetString() == false)
         {
            ShaderDepth = 0;
            Printf("Shader parse error: %s\n", sink.name);
            return false;
         }

         if ((type = SC_MatchString(ShaderTags)) != -1)
         {
            switch (type)
            {
            case SHADERTAG_COMPATIBILITY:
               SC_GetString();
               if (SC_Compare("hardware"))
               {
                  sink.compatibility = SHADER_Hardware;
               }
               else if (SC_Compare("software"))
               {
                  sink.compatibility = SHADER_Software;
               }
               else
               {
                  sink.compatibility = SHADER_Generic;
               }
               break;
            case SHADERTAG_DETAILTEX:
               SC_GetString();
               sprintf(sink.detailTex, "%s", sc_String);
               break;
            case SHADERTAG_LAYER:
               GL_ParseLayer(&sink);
               break;
            default:
               break;
            }
         }
         else
         {
            GL_CheckShaderDepth();
         }
      }

      for (i = 0; i < names.Size(); i++)
      {
         sprintf(sink.name, "%s", names[i]);
         temp = GL_ShaderIndexForShader(&sink);
         if (temp != -1)
         {
            // overwrite existing shader
            delete Shaders[sink.compatibility][i];
            Shaders[sink.compatibility][i] = new FShader(sink);
         }
         else
         {
            // add new shader
            Shaders[sink.compatibility].Push(new FShader(sink));
         }
         delete[] names[i];
      }
      names.Clear();

      return true;
   }

   return false;
}


void GL_ReleaseShaders()
{
   int shaderClass;
   unsigned int i;

   for (shaderClass = 0; shaderClass < NUM_ShaderClasses; shaderClass++)
   {
      for (i = 0; i < Shaders[shaderClass].Size(); i++)
      {
         delete Shaders[shaderClass][i];
      }
      Shaders[shaderClass].Clear();
   }
}


void GL_UpdateShader(FShader *shader)
{
   unsigned int i;
   FShaderLayer *layer;
   float diff = (frameStartMS - shader->lastUpdate) / 1000.f;

   if (shader->lastUpdate != 0 && !paused /*&& !bglobal.freeze*/)
   {
      for (i = 0; i < shader->layers.Size(); i++)
      {
         layer = shader->layers[i];
         layer->Update(diff);
         if (layer->layerMask) layer->layerMask->Update(diff);
      }
   }

   shader->lastUpdate = frameStartMS;
}


void GL_UpdateShaders()
{
   int shaderClass;
   unsigned int i;

   for (shaderClass = 0; shaderClass < NUM_ShaderClasses; shaderClass++)
   {
      for (i = 0; i < Shaders[shaderClass].Size(); i++)
      {
         GL_UpdateShader(Shaders[shaderClass][i]);
      }
   }
}


void GL_FakeUpdateShaders()
{
   int shaderClass;
   unsigned int i;

   for (shaderClass = 0; shaderClass < NUM_ShaderClasses; shaderClass++)
   {
      for (i = 0; i < Shaders[shaderClass].Size(); i++)
      {
         Shaders[shaderClass][i]->lastUpdate = frameStartMS;
      }
   }
}


FShader *GL_ShaderForTexture(FTexture *tex)
{
   FShader *shader = NULL;

   if (!gl_texture_useshaders)
   {
      return NULL;
   }

   if (stricmp(tex->Name, "") == 0)
   {
      return NULL;
   }

   if (TexMan.GetTexture(tex->Name, tex->UseType) >= (int)(ShaderLookup[SHADER_Generic].Size()))
   {
      return NULL;
   }

   if (OPENGL_GetCurrentRenderer( ) != RENDERER_OPENGL)
   {
      shader = ShaderLookup[SHADER_Software][TexMan.GetTexture(tex->Name, tex->UseType)];
   }
   else
   {
      shader = ShaderLookup[SHADER_Hardware][TexMan.GetTexture(tex->Name, tex->UseType)];
   }

   if (!shader)
   {
      shader = ShaderLookup[SHADER_Generic][TexMan.GetTexture(tex->Name, tex->UseType)];
   }

   return shader;
}


void GL_DrawShader(FShader *shader, int x, int y, int width, int height)
{
   FShaderLayer *layer, *layerMask;

   for (unsigned int i = 0; i < shader->layers.Size(); ++i)
   {
      layer = shader->layers[i];
      layerMask = layer->layerMask;

      if (i == 0)
      {
         glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      }
      else
      {
         glBlendFunc(layer->blendFuncSrc, layer->blendFuncDst);
      }

      glColor4f(layer->r, layer->g, layer->b, layer->alpha);

      switch (layer->texgen)
      {
      case SHADER_TexGen_Sphere:
         glEnable(GL_TEXTURE_GEN_S);
         glEnable(GL_TEXTURE_GEN_T);
         break;
      default:
         glDisable(GL_TEXTURE_GEN_S);
         glDisable(GL_TEXTURE_GEN_T);
         break;
      }

      textureList.BindTexture(layer->name);
      if (layerMask)
      {
         glBegin(GL_TRIANGLE_STRIP);
            glMultiTexCoord2fARB(GL_TEXTURE0_ARB, 0.f, 0.f);
            glVertex2i(x, y);
            glMultiTexCoord2fARB(GL_TEXTURE0_ARB, 0.f, 1.f);
            glVertex2i(x, y - height);
            glMultiTexCoord2fARB(GL_TEXTURE0_ARB, 1.f, 0.f);
            glVertex2i(x + width, y);
            glMultiTexCoord2fARB(GL_TEXTURE0_ARB, 1.f, 1.f);
            glVertex2i(x + width, y - height);
         glEnd();
      }
      else
      {
         glBegin(GL_TRIANGLE_STRIP);
            glTexCoord2f(0.f, 0.f);
            glVertex2i(x, y);
            glTexCoord2f(0.f, 1.f);
            glVertex2i(x, y - height);
            glTexCoord2f(1.f, 0.f);
            glVertex2i(x + width, y);
            glTexCoord2f(1.f, 1.f);
            glVertex2i(x + width, y - height);
         glEnd();
      }
   }

   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   glDisable(GL_TEXTURE_GEN_S);
   glDisable(GL_TEXTURE_GEN_T);
}


void GL_DrawShaders()
{
   int curX, curY, shaderClass, maxY;
   FShader *shader;
   FTexture *tex;

   curX = 2;
   curY = screen->GetHeight() - 2;
   maxY = 0;

   glPushMatrix();
   glLoadIdentity();
   glColor3f(1.f, 1.f, 1.f);

   for (shaderClass = 0; shaderClass < NUM_ShaderClasses; shaderClass++)
   {
      for (unsigned int i = 0; i < Shaders[shaderClass].Size(); i++)
      {
         shader = Shaders[shaderClass][i];
         tex = TexMan[shader->name];

         if (curX + tex->GetWidth() > screen->GetWidth())
         {
            curX = 2;
            curY -= maxY + 2;
            maxY = 0;
         }

         GL_DrawShader(shader, curX, curY, tex->GetWidth(), tex->GetHeight());

         maxY = MAX<int>(maxY, tex->GetHeight());
         curX += tex->GetWidth() + 2;
      }
   }

   glPopMatrix();
}


VertexProgram *GL_ParseVertexProgram()
{
   VertexProgram *vp = NULL;
   FMemLump lumpData;
   char *vpString, *err;
   int vpLength;

   if (SC_GetString())
   {
      // get internal name of vertex program
      vp = new VertexProgram;
      vp->name = new char[strlen(sc_String) + 1];
      sprintf(vp->name, "%s", sc_String);

      // get lump vertex program is stored in
      SC_GetString();
      lumpData = Wads.ReadLump(sc_String);

      vpString = (char *)lumpData.GetMem();
      vpLength = Wads.LumpLength(Wads.GetNumForName(sc_String));

      glGenProgramsARB(1, &vp->programID);
      glBindProgramARB(GL_VERTEX_PROGRAM_ARB, vp->programID);
      glProgramStringARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, vpLength, vpString);

      err = (char *)glGetString(GL_PROGRAM_ERROR_STRING_ARB);
      if (strlen(err))
      {
         Printf("ZGL: error reading VP (%s):\n%s", vp->name, err);
      }
   }

   return vp;
}


void GL_ReadVertexPrograms()
{
   VertexProgram *vp;
   int progLump, lastLump;

   //Printf("ZGL: Reading vertex programs.\n");
   VertexPrograms.Clear();

   lastLump = 0;
   while ((progLump = Wads.FindLump("VERTPROG", &lastLump)) != -1)
   {
      SC_OpenLumpNum(progLump, "VERTPROG");
      while (vp = GL_ParseVertexProgram())
      {
         VertexPrograms.Push(vp);
      }
      SC_Close();
   }

   Printf("ZGL: Read %d vertex programs.\n", VertexPrograms.Size());
}


void GL_DeleteVertexPrograms()
{
   unsigned int i;

   for (i = 0; i < VertexPrograms.Size(); i++)
   {
      glDeleteProgramsARB(1, &VertexPrograms[i]->programID);
      delete VertexPrograms[i];
   }

   VertexPrograms.Clear();
}


void GL_BindVertexProgram(char *name)
{
   unsigned int i;

   for (i = 0; i < VertexPrograms.Size(); i++)
   {
      if (strcmp(name, VertexPrograms[i]->name) == 0)
      {
         glBindProgramARB(GL_VERTEX_PROGRAM_ARB, VertexPrograms[i]->programID);
         return;
      }
   }

   Printf("ZGL: Couldn't bind VP %s.\n", name);
   glBindProgramARB(GL_VERTEX_PROGRAM_ARB, 0);
}


CCMD(gl_list_shaders)
{
   unsigned int i;

   Printf(TEXTCOLOR_YELLOW "%d Generic Shaders.\n", Shaders[SHADER_Generic].Size());
   for (i = 0; i < Shaders[SHADER_Generic].Size(); i++)
   {
      Printf(" %s\n", Shaders[SHADER_Generic][i]->name);
   }
   Printf(TEXTCOLOR_YELLOW "%d Hardware Shaders.\n", Shaders[SHADER_Hardware].Size());
   for (i = 0; i < Shaders[SHADER_Hardware].Size(); i++)
   {
      Printf(" %s\n", Shaders[SHADER_Hardware][i]->name);
   }
   Printf(TEXTCOLOR_YELLOW "%d Software Shaders.\n", Shaders[SHADER_Software].Size());
   for (i = 0; i < Shaders[SHADER_Software].Size(); i++)
   {
      Printf(" %s\n", Shaders[SHADER_Software][i]->name);
   }
}
