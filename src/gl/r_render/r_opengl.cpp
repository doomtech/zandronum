/*
** r_opengl.cpp
**
** OpenGL system interface
**
**---------------------------------------------------------------------------
** Copyright 2005 Tim Stump
** Copyright 2005 Christoph Oelckers
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
** 4. Full disclosure of the entire project's source code, except for third
**    party libraries is mandatory. (NOTE: This clause is non-negotiable!)
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/
#include "gl/gl_include.h"
#include "tarray.h"
#include "doomtype.h"
#include "gl/gl_intern.h"

#ifdef _WIN32 // [BB] Detect some kinds of glBegin hooking.
char myGlBeginCharArray[4] = {0,0,0,0};
#endif

#ifndef unix
static void CollectExtensions(HDC);
#else
#include <SDL.h>
#define wglGetProcAddress(x) (*SDL_GL_GetProcAddress)(x)
#endif
static void APIENTRY glBlendEquationDummy (GLenum mode);


#ifndef unix
PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB; // = (PFNWGLCHOOSEPIXELFORMATARBPROC)wglGetProcAddress("wglChoosePixelFormatARB");
#endif

static class DeletingStringArray : public TArray<char*>
{
public:
	~DeletingStringArray()
	{
		for(unsigned i=0;i<Size();i++)
		{
			if ((*this)[i]!=NULL) delete (*this)[i];
		}
	}
} m_Extensions;
#ifndef unix
HWND m_Window;
HDC m_hDC;
HGLRC m_hRC;
#endif

#define gl pgl

RenderContext * gl;

int occlusion_type=0;


//==========================================================================
//
// 
//
//==========================================================================

#ifndef unix
static HWND InitDummy()
{
	HMODULE g_hInst = GetModuleHandle(NULL);
	HWND dummy;
	//Create a rect structure for the size/position of the window
	RECT windowRect;
	windowRect.left = 0;
	windowRect.right = 64;
	windowRect.top = 0;
	windowRect.bottom = 64;

	//Window class structure
	WNDCLASS wc;

	//Fill in window class struct
	wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	wc.lpfnWndProc = (WNDPROC) DefWindowProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = g_hInst;
	wc.hIcon = LoadIcon(NULL, IDI_WINLOGO);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = NULL;
	wc.lpszMenuName = NULL;
	wc.lpszClassName = "GZDoomOpenGLDummyWindow";

	//Register window class
	if(!RegisterClass(&wc))
	{
		return 0;
	}

	//Set window style & extended style
	DWORD style, exStyle;
	exStyle = WS_EX_CLIENTEDGE;
	style = WS_SYSMENU | WS_BORDER | WS_CAPTION;// | WS_VISIBLE;

	//Adjust the window size so that client area is the size requested
	AdjustWindowRectEx(&windowRect, style, false, exStyle);

	//Create Window
	if(!(dummy = CreateWindowEx(exStyle,
		"GZDoomOpenGLDummyWindow",
		"GZDOOM",
		WS_CLIPSIBLINGS | WS_CLIPCHILDREN | style,
		0, 0,
		windowRect.right-windowRect.left,
		windowRect.bottom-windowRect.top,
		NULL, NULL,
		g_hInst,
		NULL)))
	{
		UnregisterClass("GZDoomOpenGLDummyWindow", g_hInst);
		return 0;
	}
	ShowWindow(dummy, SW_HIDE);

	return dummy;
}
#endif

//==========================================================================
//
// 
//
//==========================================================================

#ifndef unix
static void ShutdownDummy(HWND dummy)
{
	DestroyWindow(dummy);
	UnregisterClass("OpenGL", GetModuleHandle(NULL));
}
#endif


//==========================================================================
//
// 
//
//==========================================================================

#ifndef unix
static bool ReadInitExtensions()
{
	HDC hDC;
	HGLRC hRC;
	HWND dummy;

	PIXELFORMATDESCRIPTOR pfd = {
		sizeof(PIXELFORMATDESCRIPTOR),
			1,
			PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
			PFD_TYPE_RGBA,
			32, // color depth
			0, 0, 0, 0, 0, 0,
			0,
			0,
			0,
			0, 0, 0, 0,
			16, // z depth
			0, // stencil buffer
			0,
			PFD_MAIN_PLANE,
			0,
			0, 0, 0
	};

	int pixelFormat;

	// we have to create a dummy window to init stuff from or the full init stuff fails
	dummy = InitDummy();

	hDC = GetDC(dummy);
	pixelFormat = ChoosePixelFormat(hDC, &pfd);
	DescribePixelFormat(hDC, pixelFormat, sizeof(pfd), &pfd);

	SetPixelFormat(hDC, pixelFormat, &pfd);

	hRC = wglCreateContext(hDC);
	wglMakeCurrent(hDC, hRC);

	CollectExtensions(hDC);

	wglChoosePixelFormatARB = (PFNWGLCHOOSEPIXELFORMATARBPROC)wglGetProcAddress("wglChoosePixelFormatARB");

	/*
	if (wglChoosePixelFormatARB == NULL)
	{
	Printf("R_OPENGL: Couldn't find wglChoosePixelFormatARB.\n");
	wglMakeCurrent(NULL, NULL);
	wglDeleteContext(hRC);
	ReleaseDC(dummy, hDC);
	ShutdownDummy(dummy);
	return false;
	}
	*/

	// any extra stuff here?

	wglMakeCurrent(NULL, NULL);
	wglDeleteContext(hRC);
	ReleaseDC(dummy, hDC);
	ShutdownDummy(dummy);

	return true;
}
#endif

//==========================================================================
//
// 
//
//==========================================================================

#ifndef unix
static void CollectExtensions(HDC m_hDC)
#else
static void CollectExtensions()
#endif
{
	const char *supported = NULL;
	char *extensions, *extension;
#ifndef unix
	PROC wglGetExtString = wglGetProcAddress("wglGetExtensionsStringARB");

	if (wglGetExtString)
	{
		supported = ((char*(__stdcall*)(HDC))wglGetExtString)(m_hDC);
	}

	if (supported)
	{
		extensions = new char[strlen(supported) + 1];
		strcpy(extensions, supported);

		extension = strtok(extensions, " ");
		while(extension)
		{
			m_Extensions.Push(new char[strlen(extension) + 1]);
			strcpy(m_Extensions[m_Extensions.Size() - 1], extension);
			extension = strtok(NULL, " ");
		}

		delete [] extensions;
	}
#endif

	supported = (char *)glGetString(GL_EXTENSIONS);

	if (supported)
	{
		extensions = new char[strlen(supported) + 1];
		strcpy(extensions, supported);

		extension = strtok(extensions, " ");
		while(extension)
		{
			m_Extensions.Push(new char[strlen(extension) + 1]);
			strcpy(m_Extensions[m_Extensions.Size() - 1], extension);
			extension = strtok(NULL, " ");
		}

		delete [] extensions;
	}
}

//==========================================================================
//
// 
//
//==========================================================================

static bool CheckExtension(const char *ext)
{
	for (unsigned int i = 0; i < m_Extensions.Size(); ++i)
	{
		if (strcmp(ext, m_Extensions[i]) == 0) return true;
	}

	return false;
}


//==========================================================================
//
// These 2 functions use a different set arguments than the ARB equivalents
//
//==========================================================================
static PFNGLBEGINOCCLUSIONQUERYNVPROC glBeginOcclusionQueryNV;
static PFNGLENDOCCLUSIONQUERYNVPROC glEndOcclusionQueryNV;

static void APIENTRY BeginOcclusionQuery(GLenum type, GLuint id)
{
	if (glBeginOcclusionQueryNV && type==GL_SAMPLES_PASSED_ARB) 
	{
		glBeginOcclusionQueryNV(id);
	}
		
}

static void APIENTRY EndOcclusionQuery(GLenum type)
{
	if (glEndOcclusionQueryNV && type==GL_SAMPLES_PASSED_ARB) 
	{
		glEndOcclusionQueryNV();
	}
}

//==========================================================================
//
// 
//
//==========================================================================

static void APIENTRY LoadExtensions()
{
#ifdef unix
	CollectExtensions();
#endif

	// This loads any function pointers and flags that require a vaild render context to
	// initialize properly

	gl->vendorstring=(char*)glGetString(GL_VENDOR);

	// First try the regular function
	gl->BlendEquation = (PFNGLBLENDEQUATIONPROC)wglGetProcAddress("glBlendEquation");
	// If that fails try the EXT version
	if (!gl->BlendEquation) gl->BlendEquation = (PFNGLBLENDEQUATIONPROC)wglGetProcAddress("glBlendEquationEXT");
	// If that fails use a no-op dummy
	if (!gl->BlendEquation) gl->BlendEquation = glBlendEquationDummy;

	if (CheckExtension("GL_ARB_texture_non_power_of_two")) gl->flags|=RFL_NPOT_TEXTURE;
	if (CheckExtension("GL_NV_texture_env_combine4")) gl->flags|=RFL_TEX_ENV_COMBINE4_NV;
	if (CheckExtension("GL_ATI_texture_env_combine3")) gl->flags|=RFL_TEX_ENV_COMBINE4_NV;
	if (CheckExtension("GL_ARB_texture_non_power_of_two")) gl->flags|=RFL_NPOT_TEXTURE;

#ifndef unix
	PFNWGLSWAPINTERVALEXTPROC vs = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");
	if (vs) gl->SetVSync = vs;
#endif

	glGetIntegerv(GL_MAX_TEXTURE_SIZE,&gl->max_texturesize);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	
	if (CheckExtension("GL_ARB_fragment_program"))
	{
		gl->GenProgramsARB = (PFNGLGENPROGRAMSARBPROC)wglGetProcAddress("glGenProgramsARB");
		gl->BindProgramARB = (PFNGLBINDPROGRAMARBPROC)wglGetProcAddress("glBindProgramARB");
		gl->ProgramStringARB = (PFNGLPROGRAMSTRINGARBPROC)wglGetProcAddress("glProgramStringARB");
		gl->DeleteProgramsARB = (PFNGLDELETEPROGRAMSARBPROC) wglGetProcAddress("glDeleteProgramsARB");
		gl->IsProgramARB = (PFNGLISPROGRAMARBPROC)wglGetProcAddress("glIsProgramARB");
		gl->flags|=RFL_FRAGMENT_PROGRAM;
	}
	if (CheckExtension ("GL_ARB_shader_objects") &&
		CheckExtension ("GL_ARB_vertex_shader") &&
		CheckExtension ("GL_ARB_fragment_shader") &&
		CheckExtension ("GL_ARB_shading_language_100"))
	{
		gl->DeleteObjectARB = (PFNGLDELETEOBJECTARBPROC)wglGetProcAddress("glDeleteObjectARB");
		gl->GetHandleARB = (PFNGLGETHANDLEARBPROC)wglGetProcAddress("glGetHandleARB");
		gl->DetachObjectARB = (PFNGLDETACHOBJECTARBPROC)wglGetProcAddress("glDetachObjectARB");
		gl->CreateShaderObjectARB = (PFNGLCREATESHADEROBJECTARBPROC)wglGetProcAddress("glCreateShaderObjectARB");
		gl->ShaderSourceARB = (PFNGLSHADERSOURCEARBPROC)wglGetProcAddress("glShaderSourceARB");
		gl->CompileShaderARB = (PFNGLCOMPILESHADERARBPROC)wglGetProcAddress("glCompileShaderARB");
		gl->CreateProgramObjectARB = (PFNGLCREATEPROGRAMOBJECTARBPROC)wglGetProcAddress("glCreateProgramObjectARB");
		gl->AttachObjectARB = (PFNGLATTACHOBJECTARBPROC)wglGetProcAddress("glAttachObjectARB");
		gl->LinkProgramARB = (PFNGLLINKPROGRAMARBPROC)wglGetProcAddress("glLinkProgramARB");
		gl->UseProgramObjectARB = (PFNGLUSEPROGRAMOBJECTARBPROC)wglGetProcAddress("glUseProgramObjectARB");
		gl->ValidateProgramARB = (PFNGLVALIDATEPROGRAMARBPROC)wglGetProcAddress("glValidateProgramARB");

		gl->VertexAttrib1fARB = (PFNGLVERTEXATTRIB1FARBPROC)wglGetProcAddress("glVertexAttrib1fARB");
		gl->VertexAttrib4fARB = (PFNGLVERTEXATTRIB4FARBPROC)wglGetProcAddress("glVertexAttrib4fARB");
		gl->GetAttribLocationARB = (PFNGLGETATTRIBLOCATIONARBPROC)wglGetProcAddress("glGetAttribLocationARB");


		gl->Uniform1fARB = (PFNGLUNIFORM1FARBPROC)wglGetProcAddress("glUniform1fARB");
		gl->Uniform2fARB = (PFNGLUNIFORM2FARBPROC)wglGetProcAddress("glUniform2fARB");
		gl->Uniform3fARB = (PFNGLUNIFORM3FARBPROC)wglGetProcAddress("glUniform3fARB");
		gl->Uniform4fARB = (PFNGLUNIFORM4FARBPROC)wglGetProcAddress("glUniform4fARB");
		gl->Uniform1iARB = (PFNGLUNIFORM1IARBPROC)wglGetProcAddress("glUniform1iARB");
		gl->Uniform2iARB = (PFNGLUNIFORM2IARBPROC)wglGetProcAddress("glUniform2iARB");
		gl->Uniform3iARB = (PFNGLUNIFORM3IARBPROC)wglGetProcAddress("glUniform3iARB");
		gl->Uniform4iARB = (PFNGLUNIFORM4IARBPROC)wglGetProcAddress("glUniform4iARB");
		gl->Uniform1fvARB = (PFNGLUNIFORM1FVARBPROC)wglGetProcAddress("glUniform1fvARB");
		gl->Uniform2fvARB = (PFNGLUNIFORM2FVARBPROC)wglGetProcAddress("glUniform2fvARB");
		gl->Uniform3fvARB = (PFNGLUNIFORM3FVARBPROC)wglGetProcAddress("glUniform3fvARB");
		gl->Uniform4fvARB = (PFNGLUNIFORM4FVARBPROC)wglGetProcAddress("glUniform4fvARB");
		gl->Uniform1ivARB = (PFNGLUNIFORM1IVARBPROC)wglGetProcAddress("glUniform1ivARB");
		gl->Uniform2ivARB = (PFNGLUNIFORM2IVARBPROC)wglGetProcAddress("glUniform2ivARB");
		gl->Uniform3ivARB = (PFNGLUNIFORM3IVARBPROC)wglGetProcAddress("glUniform3ivARB");
		gl->Uniform4ivARB = (PFNGLUNIFORM4IVARBPROC)wglGetProcAddress("glUniform4ivARB");
		
		gl->UniformMatrix2fvARB = (PFNGLUNIFORMMATRIX2FVARBPROC)wglGetProcAddress("glUniformMatrix2fvARB");
		gl->UniformMatrix3fvARB = (PFNGLUNIFORMMATRIX3FVARBPROC)wglGetProcAddress("glUniformMatrix3fvARB");
		gl->UniformMatrix4fvARB = (PFNGLUNIFORMMATRIX4FVARBPROC)wglGetProcAddress("glUniformMatrix4fvARB");
		
		gl->GetObjectParameterfvARB = (PFNGLGETOBJECTPARAMETERFVARBPROC)wglGetProcAddress("glGetObjectParameterfvARB");
		gl->GetObjectParameterivARB = (PFNGLGETOBJECTPARAMETERIVARBPROC)wglGetProcAddress("glGetObjectParameterivARB");
		gl->GetInfoLogARB = (PFNGLGETINFOLOGARBPROC)wglGetProcAddress("glGetInfoLogARB");
		gl->GetAttachedObjectsARB = (PFNGLGETATTACHEDOBJECTSARBPROC)wglGetProcAddress("glGetAttachedObjectsARB");
		gl->GetUniformLocationARB = (PFNGLGETUNIFORMLOCATIONARBPROC)wglGetProcAddress("glGetUniformLocationARB");
		gl->GetActiveUniformARB = (PFNGLGETACTIVEUNIFORMARBPROC)wglGetProcAddress("glGetActiveUniformARB");
		gl->GetUniformfvARB = (PFNGLGETUNIFORMFVARBPROC)wglGetProcAddress("glGetUniformfvARB");
		gl->GetUniformivARB = (PFNGLGETUNIFORMIVARBPROC)wglGetProcAddress("glGetUniformivARB");
		gl->GetShaderSourceARB = (PFNGLGETSHADERSOURCEARBPROC)wglGetProcAddress("glGetShaderSourceARB");

		gl->flags|=RFL_GLSL;
	}

	if (CheckExtension("GL_ARB_occlusion_query"))
	{
        gl->GenQueries         = (PFNGLGENQUERIESARBPROC)wglGetProcAddress("glGenQueriesARB");
        gl->DeleteQueries      = (PFNGLDELETEQUERIESARBPROC)wglGetProcAddress("glDeleteQueriesARB");
        gl->GetQueryObjectuiv  = (PFNGLGETQUERYOBJECTUIVARBPROC)wglGetProcAddress("glGetQueryObjectuivARB");
        gl->BeginQuery         = (PFNGLBEGINQUERYARBPROC)wglGetProcAddress("glBeginQueryARB");
        gl->EndQuery           = (PFNGLENDQUERYARBPROC)wglGetProcAddress("glEndQueryARB");
		gl->flags|=RFL_OCCLUSION_QUERY;
	}
	else if (CheckExtension("GL_NV_occlusion_query"))
	{
        gl->GenQueries             = (PFNGLGENOCCLUSIONQUERIESNVPROC)wglGetProcAddress("glGenOcclusionQueriesNV");
        gl->DeleteQueries          = (PFNGLDELETEOCCLUSIONQUERIESNVPROC)wglGetProcAddress("glDeleteOcclusionQueriesNV");
        gl->GetQueryObjectuiv      = (PFNGLGETOCCLUSIONQUERYUIVNVPROC)wglGetProcAddress("glGetOcclusionQueryuivNV");
        glBeginOcclusionQueryNV    = (PFNGLBEGINOCCLUSIONQUERYNVPROC)wglGetProcAddress("glBeginOcclusionQueryNV");
        glEndOcclusionQueryNV      = (PFNGLENDOCCLUSIONQUERYNVPROC)wglGetProcAddress("glEndOcclusionQueryNV");
        gl->BeginQuery             = BeginOcclusionQuery;
        gl->EndQuery               = EndOcclusionQuery;
		gl->flags|=RFL_OCCLUSION_QUERY;
	}

	// [BB] Check for the extensions that are necessary for on the fly texture compression.
	if (CheckExtension("GL_ARB_texture_compression"))
	{
		gl->flags|=RFL_TEXTURE_COMPRESSION;
	}
	if (CheckExtension("GL_EXT_texture_compression_s3tc"))
	{
		gl->flags|=RFL_TEXTURE_COMPRESSION_S3TC;
	}

	gl->ActiveTexture = (PFNGLACTIVETEXTUREPROC)wglGetProcAddress("glActiveTextureARB");
	gl->MultiTexCoord2f = (PFNGLMULTITEXCOORD2FPROC) wglGetProcAddress("glMultiTexCoord2fARB");
	gl->MultiTexCoord2fv = (PFNGLMULTITEXCOORD2FVPROC) wglGetProcAddress("glMultiTexCoord2fvARB");
}

//==========================================================================
//
// 
//
//==========================================================================

static void APIENTRY PrintStartupLog()
{
	Printf ("GL_VENDOR: %s\n", glGetString(GL_VENDOR));
	Printf ("GL_RENDERER: %s\n", glGetString(GL_RENDERER));
	Printf ("GL_VERSION: %s\n", glGetString(GL_VERSION));
	Printf ("GL_EXTENSIONS: %s\n", glGetString(GL_EXTENSIONS));
}

//==========================================================================
//
// 
//
//==========================================================================

static bool SetupPixelFormat(bool allowsoftware, bool nostencil, int multisample)
{
#ifndef unix
	int colorDepth;
	HDC deskDC;
	int attributes[26];
	int pixelFormat;
	unsigned int numFormats;
	float attribsFloat[] = {0.0f, 0.0f};
#endif
	int stencil;
	
#ifndef unix
	deskDC = GetDC(GetDesktopWindow());
	colorDepth = GetDeviceCaps(deskDC, BITSPIXEL);
	ReleaseDC(GetDesktopWindow(), deskDC);

	/*
	if (!nostencil && colorDepth < 32)
	{
		Printf("R_OPENGL: Desktop not in 32 bit mode!\n");
		return false;
	}
	*/
#endif

	if (!nostencil)
	{
#ifndef unix
		for (stencil=1;stencil>=0;stencil--)
		{
			if (wglChoosePixelFormatARB && stencil)
			{
				attributes[0]	=	WGL_RED_BITS_ARB; //bits
				attributes[1]	=	8;
				attributes[2]	=	WGL_GREEN_BITS_ARB; //bits
				attributes[3]	=	8;
				attributes[4]	=	WGL_BLUE_BITS_ARB; //bits
				attributes[5]	=	8;
				attributes[6]	=	WGL_ALPHA_BITS_ARB;
				attributes[7]	=	8;
				attributes[8]	=	WGL_DEPTH_BITS_ARB;
				attributes[9]	=	24;
				attributes[10]	=	WGL_STENCIL_BITS_ARB;
				attributes[11]	=	8;
			
				attributes[12]	=	WGL_DRAW_TO_WINDOW_ARB;	//required to be true
				attributes[13]	=	true;
				attributes[14]	=	WGL_SUPPORT_OPENGL_ARB;
				attributes[15]	=	true;
				attributes[16]	=	WGL_DOUBLE_BUFFER_ARB;
				attributes[17]	=	true;
			
				attributes[18]	=	WGL_ACCELERATION_ARB;	//required to be FULL_ACCELERATION_ARB
				if (allowsoftware)
				{
					attributes[19]	=	WGL_NO_ACCELERATION_ARB;
				}
				else
				{
					attributes[19]	=	WGL_FULL_ACCELERATION_ARB;
				}
			
				if (multisample > 0)
				{
					attributes[20]	=	WGL_SAMPLE_BUFFERS_ARB;
					attributes[21]	=	true;
					attributes[22]	=	WGL_SAMPLES_ARB;
					attributes[23]	=	multisample;
				}
				else
				{
					attributes[20]	=	0;
					attributes[21]	=	0;
					attributes[22]	=	0;
					attributes[23]	=	0;
				}
			
				attributes[24]	=	0;
				attributes[25]	=	0;
			
				if (!wglChoosePixelFormatARB(m_hDC, attributes, attribsFloat, 1, &pixelFormat, &numFormats))
				{
					Printf("R_OPENGL: Couldn't choose pixel format. Retrying in compatibility mode\n");
					goto oldmethod;
				}
			
				if (numFormats == 0)
				{
					Printf("R_OPENGL: No valid pixel formats found. Retrying in compatibility mode\n");
					goto oldmethod;
				}

				break;
			}
			else
			{
			oldmethod:
				// If wglChoosePixelFormatARB is not found we have to do it the old fashioned way.
				static PIXELFORMATDESCRIPTOR pfd = {
					sizeof(PIXELFORMATDESCRIPTOR),
						1,
						PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
						PFD_TYPE_RGBA,
						32, // color depth
						0, 0, 0, 0, 0, 0,
						0,
						0,
						0,
						0, 0, 0, 0,
						32, // z depth
						stencil*8, // stencil buffer
						0,
						PFD_MAIN_PLANE,
						0,
						0, 0, 0
				};

				pixelFormat = ChoosePixelFormat(m_hDC, &pfd);
				DescribePixelFormat(m_hDC, pixelFormat, sizeof(pfd), &pfd);

				if (pfd.dwFlags & PFD_GENERIC_FORMAT)
				{
					if (!allowsoftware)
					{
						if (stencil==0)
						{
							// not accelerated!
							Printf("R_OPENGL: OpenGL driver not accelerated!  Falling back to software renderer.\n");
							return false;
						}
						else
						{
							Printf("R_OPENGL: OpenGL driver not accelerated! Retrying in compatibility mode\n");
							continue;
						}
					}
				}
				break;
			}
		}
#else
		stencil=1;
		SDL_GL_SetAttribute( SDL_GL_RED_SIZE,  8 );
		SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE,  8 );
		SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE,  8 );
		SDL_GL_SetAttribute( SDL_GL_ALPHA_SIZE,  8 );
		SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE,  24 );
		SDL_GL_SetAttribute( SDL_GL_STENCIL_SIZE,  8 );
//		SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER,  1 );
		if (multisample > 0) {
			SDL_GL_SetAttribute( SDL_GL_MULTISAMPLEBUFFERS, 1 );
			SDL_GL_SetAttribute( SDL_GL_MULTISAMPLESAMPLES, multisample );
		}
#endif
	}
	else
	{
		// Use the cheapest mode available and let's hope the driver can handle this...
		stencil=0;

#ifndef unix
		// If wglChoosePixelFormatARB is not found we have to do it the old fashioned way.
		static PIXELFORMATDESCRIPTOR pfd = {
			sizeof(PIXELFORMATDESCRIPTOR),
				1,
				PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
				PFD_TYPE_RGBA,
				16, // color depth
				0, 0, 0, 0, 0, 0,
				0,
				0,
				0,
				0, 0, 0, 0,
				16, // z depth
				0, // stencil buffer
				0,
				PFD_MAIN_PLANE,
				0,
				0, 0, 0
		};

		pixelFormat = ChoosePixelFormat(m_hDC, &pfd);
		DescribePixelFormat(m_hDC, pixelFormat, sizeof(pfd), &pfd);

		if (pfd.dwFlags & PFD_GENERIC_FORMAT)
		{
			if (!allowsoftware)
			{
				Printf("R_OPENGL: OpenGL driver not accelerated! Falling back to software renderer.\n");
				return false;
			}
		}
#else
	SDL_GL_SetAttribute( SDL_GL_RED_SIZE,  4 );
	SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE,  4 );
	SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE,  4 );
	SDL_GL_SetAttribute( SDL_GL_ALPHA_SIZE,  4 );
	SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE,  16 );
	//SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER,  1 )*/
#endif
	}
	if (stencil==0)
	{
		gl->flags|=RFL_NOSTENCIL;
	}

#ifndef unix
	if (!SetPixelFormat(m_hDC, pixelFormat, NULL))
	{
		Printf("R_OPENGL: Couldn't set pixel format.\n");
		return false;
	}
#endif

	return true;
}


//==========================================================================
//
// 
//
//==========================================================================

#ifndef unix
static bool APIENTRY InitHardware (HWND Window, bool allowsoftware, bool nostencil, int multisample)
#else
bool APIENTRY InitHardware (bool allowsoftware, bool nostencil, int multisample)
#endif
{
#ifndef unix
	m_Window=Window;
	m_hDC = GetDC(Window);
#endif

	if (!SetupPixelFormat(allowsoftware, nostencil, multisample))
	{
		Printf ("R_OPENGL: Reverting to software mode...\n");
		return false;
	}

#ifndef unix
	m_hRC = wglCreateContext(m_hDC);

	if (m_hRC == NULL)
	{
		Printf ("R_OPENGL: Couldn't create render context. Reverting to software mode...\n");
		return false;
	}

	wglMakeCurrent(m_hDC, m_hRC);
#endif
	return true;
}


//==========================================================================
//
// 
//
//==========================================================================

#ifndef unix
static void APIENTRY Shutdown()
{
	if (m_hRC)
	{
		wglMakeCurrent(0, 0);
		wglDeleteContext(m_hRC);
	}
	if (m_hDC) ReleaseDC(m_Window, m_hDC);
}
#endif


#ifndef unix
static bool APIENTRY SetFullscreen(int w, int h, int bits, int hz)
{
	DEVMODE dm;

	if (w==0)
	{
		ChangeDisplaySettings(0, 0);
	}
	else
	{
		dm.dmSize = sizeof(DEVMODE);
		dm.dmPelsWidth = w;
		dm.dmPelsHeight = h;
		dm.dmBitsPerPel = bits;
		dm.dmDisplayFrequency = hz;
		dm.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL | DM_DISPLAYFREQUENCY;
		if (DISP_CHANGE_SUCCESSFUL != ChangeDisplaySettings(&dm, CDS_FULLSCREEN))
		{
			dm.dmFields &= ~DM_DISPLAYFREQUENCY;
			return DISP_CHANGE_SUCCESSFUL == ChangeDisplaySettings(&dm, CDS_FULLSCREEN);
		}
	}
	return true;
}
#endif
//==========================================================================
//
// 
//
//==========================================================================

static void APIENTRY iSwapBuffers()
{
#ifndef unix
	SwapBuffers(m_hDC);
#else
	SDL_GL_SwapBuffers ();
#endif
}

#ifndef unix
static void APIENTRY SetGammaRamp (void * ramp)
#else
static void APIENTRY SetGammaRamp (Uint16 *redtable, Uint16 *greentable, Uint16 *bluetable)
#endif
{
#ifndef unix
	SetDeviceGammaRamp(m_hDC, ramp);
#else
	SDL_SetGammaRamp(redtable, greentable, bluetable);
#endif
}

#ifndef unix
static bool APIENTRY GetGammaRamp (void * ramp)
#else
static bool APIENTRY GetGammaRamp (Uint16 *redtable, Uint16 *greentable, Uint16 *bluetable)
#endif
{
#ifndef unix
	return !!GetDeviceGammaRamp(m_hDC, ramp);
#else
	return (SDL_GetGammaRamp(redtable, greentable, bluetable) >= 0);
#endif
}

static BOOL APIENTRY SetVSync(int)
{
	// empty placeholder
	return false;
}

//==========================================================================
//
// 
//
//==========================================================================

static void APIENTRY glBlendEquationDummy (GLenum mode)
{
	// If this is not supported all non-existent modes are
	// made to draw nothing.
	if (mode == GL_FUNC_ADD)
	{
		glColorMask(true, true, true, true);
	}
	else
	{
		glColorMask(false, false, false, false);
	}
}

//==========================================================================
//
// 
//
//==========================================================================

static void APIENTRY SetTextureMode(int type)
{
	static float white[] = {1.f,1.f,1.f,1.f};

	if (gl_vid_compatibility)
	{
		type = TM_MODULATE;
	}
	if (type == TM_MASK)
	{
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
		glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_REPLACE);
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_PRIMARY_COLOR);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);

		glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE); 
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PRIMARY_COLOR);
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA, GL_TEXTURE0);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_ALPHA, GL_SRC_ALPHA);
	}
	else if (type == TM_OPAQUE)
	{
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
		glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE0);
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_PRIMARY_COLOR);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);

		glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE); 
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PRIMARY_COLOR);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
	}
	else if (type == TM_INVERT)
	{
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
		glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE0);
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_PRIMARY_COLOR);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_ONE_MINUS_SRC_COLOR);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);

		glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE); 
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PRIMARY_COLOR);
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA, GL_TEXTURE0);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_ALPHA, GL_SRC_ALPHA);
	}
	else if (type == TM_INVERTOPAQUE)
	{
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
		glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE0);
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_PRIMARY_COLOR);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_ONE_MINUS_SRC_COLOR);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);

		glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE); 
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PRIMARY_COLOR);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
	}
	else if (type == TM_COLOROVERLAY)
	{
		// Bah! Why can't ATI and NVidia create a common extension for this? :(
		if (gl->flags & RFL_TEX_ENV_COMBINE4_NV)
		{
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE4_NV);
			glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_ADD);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE0);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_PRIMARY_COLOR);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE2_RGB, GL_CONSTANT);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE3_RGB_NV, GL_ZERO);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_RGB, GL_SRC_COLOR);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND3_RGB_NV, GL_ONE_MINUS_SRC_COLOR);

			glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE); 
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PRIMARY_COLOR);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA, GL_TEXTURE0);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_ALPHA, GL_SRC_ALPHA);
		}
		else if (gl->flags & RFL_TEX_ENV_COMBINE3_ATI)
		{
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
			glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE_ADD_ATI);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE0);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_PRIMARY_COLOR);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE2_RGB, GL_CONSTANT);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_RGB, GL_SRC_COLOR);

			glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE); 
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PRIMARY_COLOR);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA, GL_TEXTURE0);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_ALPHA, GL_SRC_ALPHA);
		}
		else
		{
			// not supported
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		}

	}
	else if (type == TM_BRIGHTMAP || type == TM_BRIGHTMAP_TEXTURED)
	{
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
		glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_INTERPOLATE);
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_CONSTANT);
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_TEXTURE0);
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE2_RGB, GL_PRIMARY_COLOR);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_RGB, GL_SRC_COLOR);
		glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, white);

		glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE); 
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_TEXTURE0);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
		if (type == TM_BRIGHTMAP_TEXTURED)
		{
			gl->ActiveTexture(GL_TEXTURE1);

			gl->ActiveTexture(GL_TEXTURE0);
		}
	}
	else // if (type == TM_MODULATE)
	{
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	}
}
//==========================================================================
//
// 
//
//==========================================================================

static void APIENTRY ArrayPointer(void * data, int stride)
{
	glTexCoordPointer(2,GL_FLOAT, stride, (float*)data + 3);
	glVertexPointer(3,GL_FLOAT, stride, data);
}


//==========================================================================
//
// 
//
//==========================================================================

/*
extern "C"
{

__declspec(dllexport) 
*/
void APIENTRY GetContext(RenderContext & gl)
{
	::gl=&gl;

	gl.flags=0;

	gl.LoadExtensions = LoadExtensions;
	gl.SetTextureMode = SetTextureMode;
	gl.ArrayPointer = ArrayPointer;
	gl.PrintStartupLog = PrintStartupLog;
	gl.InitHardware = InitHardware;
#ifndef unix
	gl.Shutdown = Shutdown;
#endif
	gl.SwapBuffers = iSwapBuffers;
	gl.GetGammaRamp = GetGammaRamp;
	gl.SetGammaRamp = SetGammaRamp;
#ifndef unix
	gl.SetFullscreen = SetFullscreen;
#endif

	gl.Begin = glBegin;
	gl.End = glEnd;
	gl.DrawArrays = glDrawArrays;

	gl.TexCoord2f = glTexCoord2f;
	gl.TexCoord2fv = glTexCoord2fv;

	gl.Vertex2f = glVertex2f;
	gl.Vertex2i = glVertex2i;
	gl.Vertex3f = glVertex3f;
	gl.Vertex3fv = glVertex3fv;
	gl.Vertex3d = glVertex3d;

	gl.Color4f = glColor4f;
	gl.Color4fv = glColor4fv;
	gl.Color3f = glColor3f;
	gl.Color3ub = glColor3ub;

	gl.AlphaFunc =  glAlphaFunc;
	gl.BlendFunc = glBlendFunc;

	gl.ColorMask = glColorMask;

	gl.DepthFunc = glDepthFunc;
	gl.DepthMask = glDepthMask;
	gl.DepthRange = glDepthRange;

	gl.StencilFunc = glStencilFunc;
	gl.StencilMask = glStencilMask;
	gl.StencilOp = glStencilOp;

	gl.MatrixMode = glMatrixMode;
	gl.PushMatrix = glPushMatrix;
	gl.PopMatrix = glPopMatrix;
	gl.LoadIdentity = glLoadIdentity;
	gl.MultMatrixd = glMultMatrixd;
	gl.Translatef = glTranslatef;
	gl.Ortho = glOrtho;
	gl.Scalef = glScalef;
	gl.Rotatef = glRotatef;

	gl.Viewport = glViewport;
	gl.Scissor = glScissor;

	gl.Clear = glClear;
	gl.ClearColor = glClearColor;
	gl.ClearDepth = glClearDepth;
	gl.ShadeModel = glShadeModel;
	gl.Hint = glHint;

	gl.DisableClientState = glDisableClientState;
	gl.EnableClientState = glEnableClientState;

	gl.Fogf = glFogf;
	gl.Fogi = glFogi;
	gl.Fogfv = glFogfv;

	gl.Enable = glEnable;
	gl.IsEnabled = glIsEnabled;
	gl.Disable = glDisable;

	gl.TexGeni = glTexGeni;
	gl.DeleteTextures = glDeleteTextures;
	gl.GenTextures = glGenTextures;
	gl.BindTexture = glBindTexture;
	gl.TexImage2D = glTexImage2D;
	gl.TexParameterf = glTexParameterf;
	gl.TexParameteri = glTexParameteri;
	gl.CopyTexSubImage2D = glCopyTexSubImage2D;

	gl.ReadPixels = glReadPixels;
	gl.PolygonOffset = glPolygonOffset;
	gl.ClipPlane = glClipPlane;

	gl.Finish = glFinish;
	gl.Flush = glFlush;

	gl.BlendEquation = glBlendEquationDummy;
	gl.SetVSync = SetVSync;

#ifdef _WIN32 // [BB] Detect some kinds of glBegin hooking.
	for ( int i = 0; i < 4; ++i )
		myGlBeginCharArray[i] = reinterpret_cast<char *>(gl.Begin)[i];
#endif

#ifndef unix
	ReadInitExtensions();
	//GL is not yet inited in UNIX version, read them later in LoadExtensions
#endif

}



//} // extern "C"
