

#include "gl_main.h"
#include "gl_lights.h"
#include "gl_shaders.h"

#include "sc_man.h"
#include "w_wad.h"

#include "gi.h"
#include "templates.h"


// these are the core types available in the *DEFS lump
static const char *CoreKeywords[]=
{
   "pointlight",
   "pulselight",
   "flickerlight",
   "flickerlight2",
   "sectorlight",
   "object",
   "clearlights",
   "shader",
   "clearshaders",
   NULL
};


enum
{
   LIGHT_POINT,
   LIGHT_PULSE,
   LIGHT_FLICKER,
   LIGHT_FLICKER2,
   LIGHT_SECTOR,
   LIGHT_OBJECT,
   LIGHT_CLEAR,
   TAG_SHADER,
   TAG_CLEARSHADERS
};


int ScriptDepth;


void GL_ParseDefs()
{
   int workingLump, lastLump, type;
   char *defsLump;
   int texIndex, shaderClass, numShaders;
   unsigned int i;
   FShader *shader;
   FTexture *tex;

   switch (gameinfo.gametype)
   {
   case GAME_Heretic:
      defsLump = "HTICDEFS";
      break;
   case GAME_Hexen:
      defsLump = "HEXNDEFS";
      break;
   case GAME_Strife:
      defsLump = "STRFDEFS";
      break;
   default:
      defsLump = "DOOMDEFS";
      break;
   }

   lastLump = 0;
   while ((workingLump = Wads.FindLump(defsLump, &lastLump)) != -1)
   {
      SC_OpenLumpNum(workingLump, defsLump);
      while (SC_GetString())
      {
         if ((type = SC_MatchString(CoreKeywords)) == -1)
         {
            SC_ScriptError("Error parsing defs.  Unknown tag: %s.\n", (const char **)&sc_String);
            break;
         }
         else
         {
            ScriptDepth = 0;
            switch (type)
            {
            case LIGHT_POINT:
               GL_ParsePointLight();
               break;
            case LIGHT_PULSE:
               GL_ParsePulseLight();
               break;
            case LIGHT_FLICKER:
               GL_ParseFlickerLight();
               break;
            case LIGHT_FLICKER2:
               GL_ParseFlickerLight2();
               break;
            case LIGHT_SECTOR:
               GL_ParseSectorLight();
               break;
            case LIGHT_OBJECT:
               GL_ParseObject();
               break;
            case LIGHT_CLEAR:
               GL_ReleaseLights();
               break;
            case TAG_SHADER:
               GL_ParseShader();
               break;
            case TAG_CLEARSHADERS:
               GL_ReleaseShaders();
               break;
            }
         }
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

   Printf(" (parsed %d shader(s)).\n", numShaders);
}

