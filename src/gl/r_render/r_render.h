#ifndef R_RENDER
#define R_RENDER

enum RenderFlags
{
	RFL_NPOT_TEXTURE=1,
	RFL_NOSTENCIL=2,
	RFL_FRAGMENT_PROGRAM=4,
	RFL_GLSL=8,
	RFL_OCCLUSION_QUERY=16,
	RFL_TEX_ENV_COMBINE4_NV=32,
	RFL_TEX_ENV_COMBINE3_ATI=64,
	// [BB] Added texture compression flags.
	RFL_TEXTURE_COMPRESSION=128,
	RFL_TEXTURE_COMPRESSION_S3TC=256,
};

enum TexMode
{
	TMF_MASKBIT = 1,
	TMF_OPAQUEBIT = 2,
	TMF_INVERTBIT = 4,

	TM_MODULATE = 0,
	TM_MASK = TMF_MASKBIT,
	TM_OPAQUE = TMF_OPAQUEBIT,
	TM_INVERT = TMF_INVERTBIT,
	//TM_INVERTMASK = TMF_MASKBIT | TMF_INVERTBIT
	TM_INVERTOPAQUE = TMF_INVERTBIT | TMF_OPAQUEBIT,
	TM_COLOROVERLAY=8,
	TM_BRIGHTMAP=16,
	TM_BRIGHTMAP_TEXTURED=24,
};

struct RenderContext
{
	unsigned int flags;
	int max_texturesize;
	char * vendorstring;

	void (APIENTRY * LoadExtensions) ();
	void (APIENTRY * SetTextureMode) (int type);
	void (APIENTRY * ArrayPointer) (void * data, int stride);
	void (APIENTRY * PrintStartupLog) ();
	BOOL (APIENTRY * SetVSync) (int on);
#ifndef unix
	bool (APIENTRY * InitHardware) (HWND, bool allowsoftware, bool nostencil, int multisample);
	void (APIENTRY * Shutdown) ();
#else
	bool (APIENTRY * InitHardware) (bool allowsoftware, bool nostencil, int multisample);
#endif
	void (APIENTRY * SwapBuffers) ();
#ifndef unix
	void (APIENTRY * SetGammaRamp) (void * ramp);
	bool (APIENTRY * GetGammaRamp) (void * ramp);
#else
	void (APIENTRY * SetGammaRamp) (Uint16 *redtable, Uint16 *greentable, Uint16 *bluetable);
	bool (APIENTRY * GetGammaRamp) (Uint16 *redtable, Uint16 *greentable, Uint16 *bluetable);
#endif
	bool (APIENTRY * SetFullscreen) (int w, int h, int bits, int hz);


	void (APIENTRY * Begin) (GLenum mode);
	void (APIENTRY * End) (void);
	void (APIENTRY * DrawArrays) (GLenum mode, GLint first, GLsizei count);

	void (APIENTRY * TexCoord2f) (GLfloat s, GLfloat t);
	void (APIENTRY * TexCoord2fv) (const GLfloat *v);

	void (APIENTRY * Vertex2f) (GLfloat x, GLfloat y);
	void (APIENTRY * Vertex2i) (GLint x, GLint y);
	void (APIENTRY * Vertex3f) (GLfloat x, GLfloat y, GLfloat z);
	void (APIENTRY * Vertex3d) (GLdouble x, GLdouble y, GLdouble z);
	void (APIENTRY * Vertex3fv) (const GLfloat *v);

	void (APIENTRY * Color4f) (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
	void (APIENTRY * Color4fv) (const GLfloat *v);
	void (APIENTRY * Color3f) (GLfloat red, GLfloat green, GLfloat blue);
	void (APIENTRY * Color3ub) (GLubyte red, GLubyte green, GLubyte blue);

	void (APIENTRY * AlphaFunc) (GLenum func, GLclampf ref);
	void (APIENTRY * BlendFunc) (GLenum sfactor, GLenum dfactor);
	void (APIENTRY * BlendEquation) (GLenum);
	void (APIENTRY * ColorMask) (GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);

	void (APIENTRY * DepthFunc) (GLenum func);
	void (APIENTRY * DepthMask) (GLboolean flag);
	void (APIENTRY * DepthRange) (GLclampd zNear, GLclampd zFar);

	void (APIENTRY * StencilFunc) (GLenum func, GLint ref, GLuint mask);
	void (APIENTRY * StencilMask) (GLuint mask);
	void (APIENTRY * StencilOp) (GLenum fail, GLenum zfail, GLenum zpass);

	void (APIENTRY * MatrixMode) (GLenum mode);
	void (APIENTRY * PushMatrix) (void);
	void (APIENTRY * PopMatrix) (void);
	void (APIENTRY * LoadIdentity) (void);
	void (APIENTRY * MultMatrixd) (const GLdouble *m);
	void (APIENTRY * Translatef) (GLfloat x, GLfloat y, GLfloat z);
	void (APIENTRY * Ortho) (GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar);
	void (APIENTRY * Scalef) (GLfloat x, GLfloat y, GLfloat z);
	void (APIENTRY * Rotatef) (GLfloat angle, GLfloat x, GLfloat y, GLfloat z);

	void (APIENTRY * Viewport) (GLint x, GLint y, GLsizei width, GLsizei height);
	void (APIENTRY * Scissor) (GLint x, GLint y, GLsizei width, GLsizei height);

	void (APIENTRY * Clear) (GLbitfield mask);
	void (APIENTRY * ClearColor) (GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
	void (APIENTRY * ClearDepth) (GLclampd depth);
	void (APIENTRY * ShadeModel) (GLenum mode);
	void (APIENTRY * Hint) (GLenum target, GLenum mode);

	void (APIENTRY * DisableClientState) (GLenum array);
	void (APIENTRY * EnableClientState) (GLenum array);

	void (APIENTRY * Fogf) (GLenum pname, GLfloat param);
	void (APIENTRY * Fogi) (GLenum pname, GLint param);
	void (APIENTRY * Fogfv) (GLenum pname, const GLfloat *params);

	void (APIENTRY * Enable) (GLenum cap);
	GLboolean (APIENTRY * IsEnabled) (GLenum cap);
	void (APIENTRY * Disable) (GLenum cap);

	void (APIENTRY * TexGeni) (GLenum coord, GLenum pname, GLint param);
	void (APIENTRY * DeleteTextures) (GLsizei n, const GLuint *textures);
	void (APIENTRY * GenTextures) (GLsizei n, GLuint *textures);
	void (APIENTRY * BindTexture) (GLenum target, GLuint texture);
	void (APIENTRY * TexImage2D) (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
	void (APIENTRY * TexParameterf) (GLenum target, GLenum pname, GLfloat param);
	void (APIENTRY * TexParameteri) (GLenum target, GLenum pname, GLint param);
	void (APIENTRY * CopyTexSubImage2D) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height);

	void (APIENTRY * ReadPixels) (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels);
	void (APIENTRY * PolygonOffset) (GLfloat factor, GLfloat units);
	void (APIENTRY * ClipPlane) (GLenum which, const GLdouble *);

	void (APIENTRY * Finish) (void);
	void (APIENTRY * Flush) (void);

	// ARB_FRAGMENT_PROGRAM
	PFNGLGENPROGRAMSARBPROC GenProgramsARB;
	PFNGLBINDPROGRAMARBPROC BindProgramARB;
	PFNGLPROGRAMSTRINGARBPROC ProgramStringARB;
	PFNGLDELETEPROGRAMSARBPROC DeleteProgramsARB;
	PFNGLISPROGRAMARBPROC IsProgramARB;

	// ARB_SHADER_OBJECTS
	PFNGLDELETEOBJECTARBPROC DeleteObjectARB;
	PFNGLGETHANDLEARBPROC GetHandleARB;
	PFNGLDETACHOBJECTARBPROC DetachObjectARB;
	PFNGLCREATESHADEROBJECTARBPROC CreateShaderObjectARB;
	PFNGLSHADERSOURCEARBPROC ShaderSourceARB;
	PFNGLCOMPILESHADERARBPROC CompileShaderARB;
	PFNGLCREATEPROGRAMOBJECTARBPROC CreateProgramObjectARB;
	PFNGLATTACHOBJECTARBPROC AttachObjectARB;
	PFNGLLINKPROGRAMARBPROC LinkProgramARB;
	PFNGLUSEPROGRAMOBJECTARBPROC UseProgramObjectARB;
	PFNGLVALIDATEPROGRAMARBPROC ValidateProgramARB;
	PFNGLVERTEXATTRIB1FARBPROC VertexAttrib1fARB;
	PFNGLVERTEXATTRIB4FARBPROC VertexAttrib4fARB;
	PFNGLGETATTRIBLOCATIONARBPROC GetAttribLocationARB;

	PFNGLUNIFORM1FARBPROC Uniform1fARB;
	PFNGLUNIFORM2FARBPROC Uniform2fARB;
	PFNGLUNIFORM3FARBPROC Uniform3fARB;
	PFNGLUNIFORM4FARBPROC Uniform4fARB;
	PFNGLUNIFORM1IARBPROC Uniform1iARB;
	PFNGLUNIFORM2IARBPROC Uniform2iARB;
	PFNGLUNIFORM3IARBPROC Uniform3iARB;
	PFNGLUNIFORM4IARBPROC Uniform4iARB;
	PFNGLUNIFORM1FVARBPROC Uniform1fvARB;
	PFNGLUNIFORM2FVARBPROC Uniform2fvARB;
	PFNGLUNIFORM3FVARBPROC Uniform3fvARB;
	PFNGLUNIFORM4FVARBPROC Uniform4fvARB;
	PFNGLUNIFORM1IVARBPROC Uniform1ivARB;
	PFNGLUNIFORM2IVARBPROC Uniform2ivARB;
	PFNGLUNIFORM3IVARBPROC Uniform3ivARB;
	PFNGLUNIFORM4IVARBPROC Uniform4ivARB;

	PFNGLUNIFORMMATRIX2FVARBPROC UniformMatrix2fvARB;
	PFNGLUNIFORMMATRIX3FVARBPROC UniformMatrix3fvARB;
	PFNGLUNIFORMMATRIX4FVARBPROC UniformMatrix4fvARB;

	PFNGLGETOBJECTPARAMETERFVARBPROC GetObjectParameterfvARB;
	PFNGLGETOBJECTPARAMETERIVARBPROC GetObjectParameterivARB;
	PFNGLGETINFOLOGARBPROC GetInfoLogARB;
	PFNGLGETATTACHEDOBJECTSARBPROC GetAttachedObjectsARB;
	PFNGLGETUNIFORMLOCATIONARBPROC GetUniformLocationARB;
	PFNGLGETACTIVEUNIFORMARBPROC GetActiveUniformARB;
	PFNGLGETUNIFORMFVARBPROC GetUniformfvARB;
	PFNGLGETUNIFORMIVARBPROC GetUniformivARB;
	PFNGLGETSHADERSOURCEARBPROC GetShaderSourceARB;
	
	PFNGLGENQUERIESARBPROC GenQueries;
	PFNGLDELETEQUERIESARBPROC DeleteQueries;
	PFNGLBEGINQUERYARBPROC BeginQuery;
	PFNGLENDQUERYARBPROC EndQuery;
	PFNGLGETQUERYOBJECTUIVARBPROC GetQueryObjectuiv;

	PFNGLACTIVETEXTUREPROC ActiveTexture;
	PFNGLMULTITEXCOORD2FPROC MultiTexCoord2f;
	PFNGLMULTITEXCOORD2FVPROC MultiTexCoord2fv;

};


typedef void (APIENTRY * GetContextProc)(RenderContext & gl);

void APIENTRY GetContext(RenderContext & gl);

#endif

