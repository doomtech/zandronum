
#ifndef __GL_SHADERS_H__
#define __GL_SHADERS_H__

EXTERN_CVAR(Bool, gl_glsl_renderer)

extern bool gl_fogenabled;
extern bool gl_textureenabled;
extern int gl_texturemode;
extern int gl_brightmapenabled;

struct FShaderContainer;

class GLShader
{
	FName Name;
	FShaderContainer *container;
	// additional parameters

	static GLShader * lastshader;
	static int lastcm;

public:

	static void Initialize();
	static void Clear();
	static GLShader *Find(const char * shn);
	void Bind(int cm, bool brightmap, float Speed);
	static void Unbind();
	static void Rebind();

};


//==========================================================================
//
// set brightness map status
// Change will only take effect when the texture is rebound!
//
//==========================================================================

inline void gl_EnableBrightmap(bool on)
{
	gl_brightmapenabled = on;
}



#endif

