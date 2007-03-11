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
#include "gl_main.h"
#include "sc_man.h"
#include "w_wad.h"
#include "v_text.h"


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
EXTERN_CVAR(Bool, gl_texture)
EXTERN_CVAR(Bool, gl_wireframe)
CVAR(Bool, gl_texture_useshaders, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

TArray<FShader *> Shaders[NUM_ShaderClasses];
TArray<FShader *> ShaderLookup[NUM_ShaderClasses];
TArray<VertexProgram *> VertexPrograms;
int ShaderDepth;
extern unsigned int frameStartMS;


FShaderLayer::FShaderLayer()
{
   blendFuncSrc = GL_SRC_ALPHA;
   blendFuncDst = GL_ONE_MINUS_SRC_ALPHA;
   offsetX = 0.f;
   offsetY = 0.f;
   centerX = 0.f;
   centerY = 0.f;
   rotate = 0.f;
   rotation = 0.f;
   scaleX = 1.f;
   scaleY = 1.f;
   texGen = SHADER_TexGen_None;
   alpha.SetParams(1.f, 1.f, 0.f);
   r.SetParams(1.f, 1.f, 0.f);
   g.SetParams(1.f, 1.f, 0.f);
   b.SetParams(1.f, 1.f, 0.f);
   flags = 0;
   sprintf(name, "");
}


FShaderLayer::FShaderLayer(const FShaderLayer &layer)
{
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
   texGen = layer.texGen;
   vectorX = layer.vectorX;
   vectorY = layer.vectorY;
   alpha = layer.alpha;
   r = layer.r;
   g = layer.g;
   b = layer.b;
   flags = layer.flags;
   sprintf(name, "%s", layer.name);
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


FCycler::FCycler()
{
   m_cycle = 0.f;
   m_cycleType = CYCLE_Linear;
   m_shouldCycle = false;
   m_start = m_current = 0.f;
   m_end = 0.f;
   m_increment = true;
}


void FCycler::SetParams(float start, float end, float cycle)
{
   m_cycle = cycle;
   m_time = 0.f;
   m_start = m_current = start;
   m_end = end;
   m_increment = true;
}


void FCycler::Update(float diff)
{
   float mult, angle;
   float step = m_end - m_start;

   if (!m_shouldCycle)
   {
      return;
   }

   m_time += diff;
   if (m_time >= m_cycle)
   {
      m_time = m_cycle;
   }

   mult = m_time / m_cycle;

   switch (m_cycleType)
   {
   case CYCLE_Linear:
      if (m_increment)
      {
         m_current = m_start + (step * mult);
      }
      else
      {
         m_current = m_end - (step * mult);
      }
      break;
   case CYCLE_Sin:
      angle = M_PI * 2.f * mult;
      mult = sinf(angle);
      mult = (mult + 1.f) / 2.f;
      m_current = m_start + (step * mult);
      break;
   case CYCLE_Cos:
      angle = M_PI * 2.f * mult;
      mult = cosf(angle);
      mult = (mult + 1.f) / 2.f;
      m_current = m_start + (step * mult);
      break;
   }

   if (m_time == m_cycle)
   {
      m_time = 0.f;
      m_increment = !m_increment;
   }
}


float FCycler::GetVal()
{
   return m_current;
}


void FCycler::ShouldCycle(bool sc)
{
   m_shouldCycle = sc;
}


void FCycler::SetCycleType(CycleType ct)
{
   m_cycleType = ct;
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


bool GL_ParseLayer(FShader *shader)
{
   unsigned int i;
   float start, end, cycle, r1, r2, g1, g2, b1, b2;
   FShaderLayer sink;

   if (SC_GetString())
   {
      sprintf(sink.name, "%s", sc_String);
      for (i = 0; i < strlen(sc_String); i++)
      {
         sink.name[i] = toupper(sink.name[i]);
      }
      SC_GetString();
      GL_CheckShaderDepth();
      while (ShaderDepth > 1)
      {
         if (SC_GetString() == false)
         {
            ShaderDepth = 1;
            Printf("Layer parse error: %s\n", sink.name);
            return false;
         }

         if (SC_Compare("texgen"))
         {
            SC_GetString();
            if (SC_Compare("sphere"))
            {
               sink.texGen = SHADER_TexGen_Sphere;
            }
            else
            {
               sink.texGen = SHADER_TexGen_None;
            }
         }
         else if (SC_Compare("vector"))
         {
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
         }
         else if (SC_Compare("vectorFunc"))
         {
            SC_GetString();
            if (SC_Compare("sin"))
            {
               sink.vectorX.SetCycleType(CYCLE_Sin);
            }
            else if (SC_Compare("cos"))
            {
               sink.vectorX.SetCycleType(CYCLE_Cos);
            }
            SC_GetString();
            if (SC_Compare("sin"))
            {
               sink.vectorY.SetCycleType(CYCLE_Sin);
            }
            else if (SC_Compare("cos"))
            {
               sink.vectorX.SetCycleType(CYCLE_Cos);
            }
         }
         else if (SC_Compare("blendFunc"))
         {
            SC_GetString();
            sink.blendFuncSrc = GL_GetBlendFromString(sc_String);
            SC_GetString();
            sink.blendFuncDst = GL_GetBlendFromString(sc_String);
         }
         else if (SC_Compare("scale"))
         {
            SC_GetFloat();
            sink.scaleX = 1.f / sc_Float;
            SC_GetFloat();
            sink.scaleY = 1.f / sc_Float;
         }
         else if (SC_Compare("warp"))
         {
            SC_GetString();
            if (SC_Compare("true"))
            {
               sink.warp = true;
            }
            else
            {
               sink.warp = false;
            }
         }
         else if (SC_Compare("offset"))
         {
            SC_GetFloat();
            sink.offsetX = sc_Float;
            SC_GetFloat();
            sink.offsetY = sc_Float;
         }
         else if (SC_Compare("rotation"))
         {
            SC_GetFloat();
            sink.rotation = sc_Float;
         }
         else if (SC_Compare("rotate"))
         {
            SC_GetFloat();
            sink.rotate = sc_Float;
         }
         else if (SC_Compare("center"))
         {
            SC_GetFloat();
            sink.centerX = sc_Float;
            SC_GetFloat();
            sink.centerY = sc_Float;
         }
         else if (SC_Compare("alpha"))
         {
            SC_GetString();
            if (SC_Compare("cycle"))
            {
               sink.alpha.ShouldCycle(true);
               SC_GetString();
               if (SC_Compare("sin"))
               {
                  sink.alpha.SetCycleType(CYCLE_Sin);
               }
               else if (SC_Compare("cos"))
               {
                  sink.alpha.SetCycleType(CYCLE_Cos);
               }
               else if (SC_Compare("linear"))
               {
                  sink.alpha.SetCycleType(CYCLE_Linear);
               }
               else
               {
                  // cycle type missing, back script up and default to CYCLE_Linear
                  SC_UnGet();
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
         }
         else if (SC_Compare("color"))
         {
            SC_GetString();
            if (SC_Compare("cycle"))
            {
               sink.r.ShouldCycle(true);
               sink.g.ShouldCycle(true);
               sink.b.ShouldCycle(true);
               // get color1
               SC_GetString();

               if (SC_Compare("sin"))
               {
                  sink.r.SetCycleType(CYCLE_Sin);
                  sink.g.SetCycleType(CYCLE_Sin);
                  sink.b.SetCycleType(CYCLE_Sin);
               }
               else if (SC_Compare("cos"))
               {
                  sink.r.SetCycleType(CYCLE_Cos);
                  sink.g.SetCycleType(CYCLE_Cos);
                  sink.b.SetCycleType(CYCLE_Cos);
               }
               else if (SC_Compare("linear"))
               {
                  sink.r.SetCycleType(CYCLE_Linear);
                  sink.g.SetCycleType(CYCLE_Linear);
                  sink.b.SetCycleType(CYCLE_Linear);
               }
               else
               {
                  // cycle type missing, back script up and default to CYCLE_Linear
                  SC_UnGet();
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
         }

         GL_CheckShaderDepth();
      }

      // base layer has to be opaque
      if (shader->layers.Size() == 0)
      {
         sink.alpha.SetParams(1.f, 1.f, 0.f);
      }

      shader->layers.Push(new FShaderLayer(sink));

      return true;
   }

   return false;
}


bool GL_ParseShader()
{
   unsigned int i;
   int temp;
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
         if (SC_Compare("detail"))
         {
            SC_GetString();
            sprintf(sink.detailTex, "%s", sc_String);
         }
         else if (SC_Compare("compatibility"))
         {
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
         }
         else if (SC_Compare("layer"))
         {
            GL_ParseLayer(&sink);
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


void GL_InitShaders()
{
   int shaderslump, lastLump = 0;
   int texIndex, shaderClass, numShaders;
   unsigned int i;
   FShader *shader;
   FTexture *tex;

   for (shaderClass = 0; shaderClass < NUM_ShaderClasses; shaderClass++)
   {
      Shaders[shaderClass].Clear();
   }

   while ((shaderslump = Wads.FindLump("SHADERS", &lastLump)) != -1)
   {
      SC_OpenLumpNum(shaderslump, "SHADERS");
      SC_GetString();
      while (SC_Compare ("shader"))
      {
         GL_ParseShader();
         SC_GetString();
      }
      SC_Close();
   }

   for (shaderClass = 0; shaderClass < NUM_ShaderClasses; shaderClass++)
   {
      Shaders[shaderClass].ShrinkToFit();
      ShaderLookup[shaderClass].Resize(TexMan.NumTextures());
   }

   for (texIndex = 0; texIndex < TexMan.NumTextures(); texIndex++)
   {
      for (shaderClass = 0; shaderClass < NUM_ShaderClasses; shaderClass++)
      {
         ShaderLookup[shaderClass][texIndex] = NULL;
      }
   }

   numShaders = 0;
   for (shaderClass = 0; shaderClass < NUM_ShaderClasses; shaderClass++)
   {
      numShaders += Shaders[shaderClass].Size();
      for (i = 0; i < Shaders[shaderClass].Size(); i++)
      {
         shader = Shaders[shaderClass][i];
         tex = TexMan[shader->name];
         texIndex = TexMan.GetTexture(tex->Name, tex->UseType);
         ShaderLookup[shaderClass][texIndex] = shader;
      }
   }

   Printf("Parsed %d shader(s).\n", numShaders);
}


void STACK_ARGS GL_ReleaseShaders()
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

   if (shader->lastUpdate != 0)
   {
      for (i = 0; i < shader->layers.Size(); i++)
      {
         layer = shader->layers[i];

         layer->r.Update(diff);
         layer->g.Update(diff);
         layer->b.Update(diff);
         layer->alpha.Update(diff);
         layer->vectorY.Update(diff);
         layer->vectorX.Update(diff);

         layer->offsetX += layer->vectorX.GetVal() * diff;
         if (layer->offsetX > 1.f) layer->offsetX -= 1.f;
         if (layer->offsetX < 0.f) layer->offsetX += 1.f;

         layer->offsetY += layer->vectorY.GetVal() * diff;
         if (layer->offsetY > 1.f) layer->offsetY -= 1.f;
         if (layer->offsetY < 0.f) layer->offsetY += 1.f;

         layer->rotation += layer->rotate * diff;
         if (layer->rotation > 360.f) layer->rotation -= 360.f;
         if (layer->rotation < 0.f) layer->rotation += 360.f;
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


FShader *GL_ShaderForTexture(FTexture *tex)
{
   FShader *shader = NULL;

   if (!gl_texture_useshaders || !gl_texture || gl_wireframe)
   {
      return NULL;
   }

   if (stricmp(tex->Name, "") == 0)
   {
      return NULL;
   }

   if (TexMan.GetTexture(tex->Name, tex->UseType) >= ShaderLookup[SHADER_Generic].Size())
   {
      return NULL;
   }

   if ( OPENGL_GetCurrentRenderer( ) == RENDERER_SOFTWARE )
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

   //if (shader) Printf("found shader %s.\n", tex->Name);

   return shader;
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
