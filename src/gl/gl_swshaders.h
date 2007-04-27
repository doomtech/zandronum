
#ifndef __GL_SWSHADERS_H__
#define __GL_SWSHADERS_H__


#include "tarray.h"
#include "gl/gl_cycler.h"
#include "gl/gl_texture.h"



/*
enum
{
   SHADER_Generic = 0,
   SHADER_Hardware,
   SHADER_Software,
   NUM_ShaderClasses,
};
*/

enum
{
   SHADER_TexGen_None = 0,
   SHADER_TexGen_Sphere,
   NUM_TexGenTypes
};


typedef struct
{
   float time;
   float cycle;
} cycle_params_t;


class FShaderLayer
{
public:
   FShaderLayer();
   FShaderLayer(const FShaderLayer &layer);
   ~FShaderLayer();
   void Update(float diff);
   FTexture * tex;	// The actual texture/flat/whatever to use for the layer
   bool additive;
   bool warp;
   bool animate;
   bool emissive;
   float centerX, centerY;
   float rotation;
   float rotate;
   float offsetX, offsetY;
   FCycler adjustX, adjustY;
   FCycler vectorX, vectorY;
   FCycler scaleX, scaleY;
   FCycler alpha;
   FCycler r, g, b;
   unsigned int flags;
   unsigned int blendFuncSrc, blendFuncDst;
   unsigned char texgen;
   FShaderLayer *layerMask;
};


class FSWShader
{
public:
   FSWShader();
   FSWShader(const FSWShader &shader);
   ~FSWShader();
   bool operator== (const FSWShader &shader);
   FTexture * tex; // Texture to replace
   FTexture * detailTex; // Detail texture
   // int compatibility; // hardware/software/any shader
   TArray <FShaderLayer *> layers; // layers for shader
   unsigned int lastUpdate;
};


extern TArray<FSWShader *> Shaders;
//extern TArray<FSWShader *> ShaderLookup[NUM_ShaderClasses];

void GL_InitShaders();
void GL_ReleaseShaders();
void GL_UpdateShaders();
void GL_FakeUpdateShaders();
void GL_UpdateShader(FSWShader *shader);
void GL_DrawShaders();
//FSWShader *GL_ShaderForTexture(FTexture *tex);

bool GL_ParseShader();


#endif // __GL_SHADERS_H__