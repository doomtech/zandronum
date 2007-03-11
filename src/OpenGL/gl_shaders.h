
#ifndef __GL_SHADERS_H__
#define __GL_SHADERS_H__


#include "tarray.h"


typedef enum
{
   SHADER_TexGen_None,
   SHADER_TexGen_Sphere,
} shader_param_t;


typedef enum
{
   CYCLE_Linear,
   CYCLE_Sin,
   CYCLE_Cos
} CycleType;


enum
{
   SHADER_Generic = 0,
   SHADER_Hardware,
   SHADER_Software,
   NUM_ShaderClasses,
};


class FCycler
{
public:
   FCycler();
   void Update(float diff);
   void SetParams(float start, float end, float cycle);
   void ShouldCycle(bool sc);
   void SetCycleType(CycleType ct);
   float GetVal();
protected:
   float m_start, m_end, m_current;
   float m_time, m_cycle;
   bool m_increment, m_shouldCycle;
   CycleType m_cycleType;
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
   char name[9]; // name of the actual texture/flat/whatever to use for the layer
   shader_param_t texGen;
   bool warp;
   FCycler vectorX, vectorY;
   float offsetX, offsetY;
   float scaleX, scaleY;
   float centerX, centerY;
   float rotation;
   float rotate;
   FCycler alpha;
   FCycler r, g, b;
   unsigned int flags;
   unsigned int blendFuncSrc, blendFuncDst;
};


class FShader
{
public:
   FShader();
   FShader(const FShader &shader);
   ~FShader();
   bool operator== (const FShader &shader);
   char name[9]; // name of texture to replace
   char detailTex[9]; // name of detail texture
   int compatibility; // hardware/software/any shader
   TArray <FShaderLayer *> layers; // layers for shader
   unsigned int lastUpdate;
};


extern TArray<FShader *> Shaders[NUM_ShaderClasses];
extern TArray<FShader *> ShaderLookup[NUM_ShaderClasses];

void GL_InitShaders();
void STACK_ARGS GL_ReleaseShaders();
void GL_UpdateShaders();
FShader *GL_ShaderForTexture(FTexture *tex);

bool GL_ParseShader();


#endif // __GL_SHADERS_H__