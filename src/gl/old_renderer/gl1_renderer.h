#ifndef __GL1_RENDERER
#define __GL1_RENDERER

#include "gl/renderer/gl_renderer.h"

struct GL_IRECT;
struct GLSectorPlane;

// textures + sprites

extern DWORD gl_fixedcolormap;

class FMaterial;


void gl_SetPlaneTextureRotation(const GLSectorPlane * secplane, FMaterial * gltexture);
void gl_ClearShaders();
void gl_EnableShader(bool on);

void gl_SetTextureMode(int which);
void gl_EnableFog(bool on);
void gl_SetShaderLight(float level, float factor);
void gl_SetCamera(float x, float y, float z);

void gl_SetGlowParams(float *topcolors, float topheight, float *bottomcolors, float bottomheight);
void gl_SetGlowPosition(float topdist, float bottomdist);

int gl_SetupShader(bool cameratexture, int &shaderindex, int &cm, float warptime);
void gl_ApplyShader();

void gl_EndDrawScene();
sector_t * gl_RenderView (AActor * camera, GL_IRECT * bounds, float fov, float ratio, bool mainview);
void gl_SetRenderStyle(FRenderStyle style, bool drawopaque, bool allowcolorblending);

#endif