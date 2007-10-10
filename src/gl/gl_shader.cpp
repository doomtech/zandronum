#include "gl_pch.h"
/*
** gl_shader.cpp
**
** GLSL shader handling
**
**---------------------------------------------------------------------------
** Copyright 2004-2005 Christoph Oelckers
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
** 4. When not used as part of GZDoom or a GZDoom derivative, this code will be
**    covered by the terms of the GNU Lesser General Public License as published
**    by the Free Software Foundation; either version 2.1 of the License, or (at
**    your option) any later version.
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
#include "gl/gl_values.h"
#include "c_cvars.h"
#include "v_video.h"
#include "name.h"
#include "w_wad.h"
#include "gl_shader.h"
#include "i_system.h"
#include "doomerrors.h"

extern long gl_frameMS;

bool gl_fogenabled;
bool gl_textureenabled;
int gl_texturemode;
int gl_brightmapenabled;

class FShader
{
	friend class GLShader;

	GLhandleARB hShader;
	GLhandleARB hVertProg;
	GLhandleARB hFragProg;

	TArray<int> attribs;
	TArray<int> attrib_names;
	TArray<int> uniforms;
	TArray<int> uniform_names;

	int timer_index;
	int desaturation_index;
	int fogenabled_index;
	int texturemode_index;

	int currentfogenabled;
	int currenttexturemode;

public:
	FShader()
	{
		hShader = hVertProg = hFragProg = NULL;
		currentfogenabled = currenttexturemode = 0;
	}

	~FShader();

	bool Load(const char * name, const char * vertprog, const char * fragprog);
	void AddAttrib(const char * pname);
	int GetAttribIndex(FName pname);
	void AddUniform(const char * pname);
	int GetUniformIndex(FName pname);

	void SetFogEnabled(int on)
	{
		if (on != currentfogenabled)
		{
			currentfogenabled = on;
			gl.Uniform1iARB(fogenabled_index, on); 
		}
	}

	void SetTextureMode(int mode)
	{
		if (mode != currenttexturemode)
		{
			currenttexturemode = mode;
			gl.Uniform1iARB(texturemode_index, mode); 
		}
	}
	bool Bind();

};


static FShader *gl_activeShader;

//==========================================================================
//
//
//
//==========================================================================

bool FShader::Load(const char * name, const char * vert_prog, const char * frag_prog)
{
	static char buffer[10000];

	if (gl.flags & RFL_GLSL)
	{
		hVertProg = gl.CreateShaderObjectARB(GL_VERTEX_SHADER_ARB);
		hFragProg = gl.CreateShaderObjectARB(GL_FRAGMENT_SHADER_ARB);	

		int vp_size = (int)strlen(vert_prog);
		int fp_size = (int)strlen(frag_prog);

		gl.ShaderSourceARB(hVertProg, 1, &vert_prog, &vp_size);
		gl.ShaderSourceARB(hFragProg, 1, &frag_prog, &fp_size);

		gl.CompileShaderARB(hVertProg);
		gl.CompileShaderARB(hFragProg);

		hShader = gl.CreateProgramObjectARB();

		gl.AttachObjectARB(hShader, hVertProg);
		gl.AttachObjectARB(hShader, hFragProg);

		gl.LinkProgramARB(hShader);
	
		gl.GetInfoLogARB(hShader, 10000, NULL, buffer);
		if (*buffer) 
		{
			Printf("Init Shader '%s':\n%s\n", name, buffer);
		}
		int linked;
		gl.GetObjectParameterivARB(hShader, GL_OBJECT_LINK_STATUS_ARB, &linked);
		timer_index = gl.GetUniformLocationARB(hShader, "timer");
		desaturation_index = gl.GetUniformLocationARB(hShader, "desaturation_factor");
		fogenabled_index = gl.GetUniformLocationARB(hShader, "fogenabled");
		texturemode_index = gl.GetUniformLocationARB(hShader, "texturemode");

		int brightmap_index = gl.GetUniformLocationARB(hShader, "brightmap");

		gl.UseProgramObjectARB(hShader);
		gl.Uniform1iARB(brightmap_index, 1);
		gl.UseProgramObjectARB(0);

		return !!linked;
	}
	return false;
}

//==========================================================================
//
//
//
//==========================================================================

FShader::~FShader()
{
	gl.DeleteObjectARB(hShader);
	gl.DeleteObjectARB(hVertProg);
	gl.DeleteObjectARB(hFragProg);
}


//==========================================================================
//
//
//
//==========================================================================

void FShader::AddAttrib(const char * pname)
{
	FName nm = FName(pname);

	for(int i=0;i<attribs.Size();i++)
	{
		if (attrib_names[i]==nm) return;
	}
	attrib_names.Push(nm);
	attribs.Push(gl.GetAttribLocationARB(hShader, pname));
}

//==========================================================================
//
//
//
//==========================================================================

int FShader::GetAttribIndex(FName pname)
{
	for(int i=0;i<attribs.Size();i++)
	{
		if (attrib_names[i]==pname) return attribs[i];
	}
	return -1;
}


//==========================================================================
//
//
//
//==========================================================================

void FShader::AddUniform(const char * pname)
{
	FName nm = FName(pname);

	for(int i=0;i<uniforms.Size();i++)
	{
		if (uniform_names[i]==nm) return;
	}
	uniform_names.Push(nm);
	uniforms.Push(gl.GetUniformLocationARB(hShader, pname));
}

//==========================================================================
//
//
//
//==========================================================================

int FShader::GetUniformIndex(FName pname)
{
	for(int i=0;i<uniforms.Size();i++)
	{
		if (uniform_names[i]==pname) return uniforms[i];
	}
	return -1;
}

//==========================================================================
//
//
//
//==========================================================================

bool FShader::Bind()
{
	if (gl_activeShader!=this)
	{
		gl.UseProgramObjectARB(hShader);
		SetTextureMode(gl_texturemode);
		SetFogEnabled(gl_fogenabled);
		gl_activeShader=this;
	}
	if (timer_index >=0) gl.Uniform1fARB(timer_index, gl_frameMS/1000.f);
	return true;
}



//==========================================================================
//
// This class contains the shaders for the different lighting modes
// that are required (e.g. special colormaps etc.)
//
//==========================================================================
struct FShaderContainer
{
	friend class GLShader;

	FName Name;
	FName TexFileName;

	FShader *shader_cm[6];

	// These are needed twice: Once for normal lighting, once for desaturation.
	FShader *shader_light[3][2];

	FString CombineFragmentShader(const char * gettexel, const char * lighting, const char * main);

public:
	FShaderContainer(const char *ShaderName, const char *ShaderPath);
	~FShaderContainer();
	
};

//==========================================================================
//
//
//
//==========================================================================

FString FShaderContainer::CombineFragmentShader(const char * gettexel, const char * lighting, const char * main)
{
	int lump1 = Wads.GetNumForFullName(gettexel);
	int lump2 = Wads.GetNumForFullName(lighting);
	int lump3 = Wads.GetNumForFullName(main);

	FMemLump data1 = Wads.ReadLump(lump1);
	FMemLump data2 = Wads.ReadLump(lump2);
	FMemLump data3 = Wads.ReadLump(lump3);

	FString res;

	res << (char*)data1.GetMem() << '\n' << (char*)data2.GetMem() << '\n' << (char*)data3.GetMem();
	return res;
}

//==========================================================================
//
//
//
//==========================================================================

FShaderContainer::FShaderContainer(const char *ShaderName, const char *ShaderPath)
{
	struct Lighting
	{
		const char * LightingName;
		const char * lightpixelfunc;
	};

	static Lighting default_cm[]={
		{ "Standard",	"shaders/light/light_norm.fp"			},
		{ "Inverse",	"shaders/light/light_inverse.fp"		},
		{ "Gold",		"shaders/light/light_gold.fp"			},
		{ "Red",		"shaders/light/light_red.fp"			},
		{ "Green",		"shaders/light/light_green.fp"			},
	};

	static Lighting default_light[]={
		{ "Standard",	"shaders/light/light_eyefog.fp"		},
		{ "Brightmap",	"shaders/light/light_brightmap.fp"	},
		//{ "Doom",		"shaders/light/light_doom.fp"		},
	};

	static const char * main_fp2[]={ "shaders/main.fp", "shaders/main_desat.fp" };

	Name = ShaderName;
	TexFileName = ShaderPath;

	for(int i=0;i<5;i++)
	{
		FString name;

		name << ShaderName << "::" << default_cm[i].LightingName;

		try
		{
			FString frag = CombineFragmentShader(ShaderPath, default_cm[i].lightpixelfunc, "shaders/main.fp");

			int vlump = Wads.GetNumForFullName("shaders/main_nofog.vp");
			FMemLump vdata = Wads.ReadLump(vlump);

			shader_cm[i] = new FShader;
			if (!shader_cm[i]->Load(name, (const char*)vdata.GetMem(), frag))
			{
				delete shader_cm[i];
				shader_cm[i]=NULL;
			}
		}
		catch(CRecoverableError &err)
		{
			shader_cm[i]=NULL;
			Printf("Unable to load shader %s:\n%s\n", name.GetChars(), err.GetMessage());
		}

	}

	for(int i=0;i<2;i++) for(int j=0;j<2;j++)
	{
		FString name;

		name << ShaderName << "::" << default_light[i].LightingName;
		if (j) name << "::desat";

		try
		{
			FString frag = CombineFragmentShader(ShaderPath, default_light[i].lightpixelfunc, main_fp2[j]);

			int vlump = Wads.GetNumForFullName("shaders/main.vp");
			FMemLump vdata = Wads.ReadLump(vlump);

			shader_light[i][j] = new FShader;
			if (!shader_light[i][j]->Load(name, (const char*)vdata.GetMem(), frag))
			{
				delete shader_light[i][j];
				shader_light[i][j]=NULL;
			}
		}
		catch(CRecoverableError &err)
		{
			shader_light[i][j]=NULL;
			Printf("Unable to load shader %s:\n%s\n", name.GetChars(), err.GetMessage());
		}
	}
}

//==========================================================================
//
//
//
//==========================================================================
FShaderContainer::~FShaderContainer()
{
	for(int i=0;i<5;i++)
	{
		if (shader_cm[i]!=NULL)
		{
			delete shader_cm[i];
			shader_cm[i] = NULL;
		}
	}

	for(int i=0;i<2;i++) for(int j=0;j<2;j++)
	{
		if (shader_light[i][j]!=NULL)
		{
			delete shader_light[i][j];
			shader_light[i][j] = NULL;
		}
	}
}

//==========================================================================
//
//
//
//==========================================================================
struct FDefaultShader 
{
	const char * ShaderName;
	const char * gettexelfunc;
};

static FDefaultShader defaultshaders[]=
	{	
		{"Default",	"shaders/tex/tex_norm.fp"},
		{"Warp 1",	"shaders/tex/tex_warp1.fp"},
		{"Warp 2",	"shaders/tex/tex_warp2.fp"},
		{NULL,NULL}
		
	};

static TArray<FShaderContainer *> AllContainers;

//==========================================================================
//
//
//
//==========================================================================

static FShaderContainer * AddShader(const char * name, const char * texfile)
{
	FShaderContainer *sh = new FShaderContainer(name, texfile);
	AllContainers.Push(sh);
	return sh;
}



static FShaderContainer * GetShader(const char * n,const char * fn)
{
	FName sfn = fn;

	for(int i=0;i<AllContainers.Size();i++)
	{
		if (AllContainers[i]->TexFileName == sfn)
		{
			return AllContainers[i];
		}
	}
	return AddShader(n, fn);
}


//==========================================================================
//
//
//
//==========================================================================

GLShader * GLShader::lastshader;
int GLShader::lastcm;

static TArray<GLShader *> AllShaders;

void GLShader::Initialize()
{
	if (gl.flags & RFL_GLSL)
	{
		for(int i=0;i<3;i++)
		{
			FShaderContainer * shc = AddShader(defaultshaders[i].ShaderName, defaultshaders[i].gettexelfunc);
			GLShader * shd = new GLShader;
			shd->container = shc;
			shd->Name = defaultshaders[i].ShaderName;
			AllShaders.Push(shd);
		}
	}
}

void GLShader::Clear()
{
	if (gl.flags & RFL_GLSL)
	{
		for(int i=0;i<AllContainers.Size();i++)
		{
			delete AllContainers[i];
		}
		for(int i=0;i<AllShaders.Size();i++)
		{
			delete AllShaders[i];
		}
		AllContainers.Clear();
		AllShaders.Clear();
	}
}

GLShader *GLShader::Find(const char * shn)
{
	FName sfn = shn;

	for(int i=0;i<AllShaders.Size();i++)
	{
		if (AllContainers[i]->Name == sfn)
		{
			return AllShaders[i];
		}
	}
	return NULL;
}


void GLShader::Bind(int cm, bool brightmap)
{
	FShader *sh=NULL;
	switch(cm)
	{
	case CM_INVERT:
	case CM_GOLDMAP:
	case CM_REDMAP:
	case CM_GREENMAP:
		// these are never used with any kind of lighting or fog
		sh = container->shader_cm[cm-CM_INVERT+1];
		// [BB] If there was a problem when loading the shader, sh is NULL here.
		if( sh )
			sh->Bind();
		break;

	default:

		bool desat = cm>=CM_DESAT1 && cm<=CM_DESAT31;
		sh = container->shader_light[brightmap][desat];
		// [BB] If there was a problem when loading the shader, sh is NULL here.
		if( sh )
		{
			sh->Bind();
			if (desat)
			{
				gl.Uniform1fARB(sh->desaturation_index, 1.f-float(cm-CM_DESAT0)/(CM_DESAT31-CM_DESAT0));
			}
		}
		break;
	}
	lastshader = this;
	lastcm = cm;
}

void GLShader::Rebind()
{
	//if (lastshader) lastshader->Bind(lastcm);
}

void GLShader::Unbind()
{
	if (gl.flags & RFL_GLSL)
	{
		gl.UseProgramObjectARB(0);
		gl_activeShader=NULL;
	}
}

//==========================================================================
//
// enable/disable texturing
//
//==========================================================================

void gl_EnableTexture(bool on)
{
	static FShader * shader_when_disabled = NULL;
	if (on)
	{
		if (!gl_textureenabled) 
		{
			gl.Enable(GL_TEXTURE_2D);
			if (shader_when_disabled != NULL)
			{
				shader_when_disabled->Bind();
			}
		}
	}
	else 
	{
		if (gl_textureenabled) 
		{
			gl.Disable(GL_TEXTURE_2D);
			shader_when_disabled = gl_activeShader;
			GLShader::Unbind();
		}
	}
	if (gl_textureenabled != on)
	{
		gl_textureenabled=on;
	}
}

//==========================================================================
//
// enable/disable fog
//
//==========================================================================

void gl_EnableFog(bool on)
{
	if (on) 
	{
		if (!gl_fogenabled) gl.Enable(GL_FOG);
	}
	else 
	{
		if (gl_fogenabled) gl.Disable(GL_FOG);
	}
	gl_fogenabled=on;
	if (gl_activeShader) gl_activeShader->SetFogEnabled(on);
}


//==========================================================================
//
// set texture mode
//
//==========================================================================

void gl_SetTextureMode(int which)
{
	if (which != gl_texturemode) gl.SetTextureMode(which);
	if (gl_activeShader) gl_activeShader->SetTextureMode(which);
	gl_texturemode = which;
}


//==========================================================================
//
// Lighting stuff (unused for now)
//
//==========================================================================

bool i_useshaders;

void gl_SetCamera(float x, float y, float z)
{
	/*
	if (i_useshaders)
	{
		gl.Uniform4fARB(u_camera, x, z, y, 0);
	}
	*/
}

