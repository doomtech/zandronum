
#ifndef __GL_SHADERS_H__
#define __GL_SHADERS_H__


#include "tarray.h"


typedef enum
{
   CYCLE_Linear,
   CYCLE_Sin,
   CYCLE_Cos,
   CYCLE_SawTooth,
   CYCLE_Square
} CycleType;


enum
{
   SHADER_Generic = 0,
   SHADER_Hardware,
   SHADER_Software,
   NUM_ShaderClasses,
};

enum
{
   SHADER_TexGen_None = 0,
   SHADER_TexGen_Sphere,
   NUM_TexGenTypes
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
   inline operator float () const { return m_current; }
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
   ~FShaderLayer();
   void Update(float diff);
   char name[9]; // name of the actual texture/flat/whatever to use for the layer
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
void GL_ReleaseShaders();
void GL_UpdateShaders();
void GL_FakeUpdateShaders();
void GL_UpdateShader(FShader *shader);
void GL_DrawShaders();
FShader *GL_ShaderForTexture(FTexture *tex);

bool GL_ParseShader();


#endif // __GL_SHADERS_H__