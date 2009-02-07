
#ifndef __GL_SHADERS_H__
#define __GL_SHADERS_H__

extern bool gl_fogenabled;
extern bool gl_textureenabled;
extern bool gl_glowenabled;
extern int gl_texturemode;
extern int gl_brightmapenabled;

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

void gl_ApplyShader();
void gl_DisableShader();
void gl_ClearShaders();
void gl_InitShaders();


#endif

