
#ifndef __GL_SHADERS_H__
#define __GL_SHADERS_H__

EXTERN_CVAR(Bool, gl_glsl_renderer)

extern bool gl_fogenabled;
extern bool gl_textureenabled;
extern int gl_texturemode;


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
	void Bind(int cm);
	static void Unbind();
	static void Rebind();

};


#endif

