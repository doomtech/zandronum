
#ifndef __GL_SHADERS_H__
#define __GL_SHADERS_H__

extern bool gl_fogenabled;
extern bool gl_textureenabled;
extern bool gl_glowenabled;
extern bool gl_lightsenabled;
extern int gl_texturemode;
extern int gl_brightmapenabled;
extern bool gl_shaderactive;

//==========================================================================
//
// set brightness map and glowstatus
// Change will only take effect when the texture is rebound!
//
//==========================================================================

inline void gl_EnableBrightmap(bool on)
{
	gl_brightmapenabled = on;
}

inline void gl_EnableGlow(bool on)
{
	gl_glowenabled = on;
}

inline void gl_EnableLights(bool on)
{
	gl_lightsenabled = on;
}

bool gl_BrightmapsActive();
bool gl_GlowActive();
bool gl_ExtFogActive();

void gl_ApplyShader();
void gl_DisableShader();
void gl_ClearShaders();
void gl_InitShaders();

void gl_EnableShader(bool on);

void gl_SetTextureMode(int which);
void gl_EnableFog(bool on);
void gl_SetShaderLight(float level, float factor);
void gl_SetCamera(float x, float y, float z);

void gl_SetGlowParams(float *topcolors, float topheight, float *bottomcolors, float bottomheight);
void gl_SetGlowPosition(float topdist, float bottomdist);

void gl_SetLightRange(int first, int last, int forceadd);

int gl_SetupShader(bool cameratexture, int &shaderindex, int &cm, float warptime);


#endif

