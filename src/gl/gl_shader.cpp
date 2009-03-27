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
** 5. Full disclosure of the entire project's source code, except for third
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
#include "gl/gl_intern.h"
#include "gl/gl_values.h"
#include "c_cvars.h"
#include "v_video.h"
#include "name.h"
#include "w_wad.h"
#include "gl_shader.h"
#include "i_system.h"
#include "doomerrors.h"

CUSTOM_CVAR(Bool, gl_warp_shader, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG|CVAR_NOINITCALL)
{
	if (self && !(gl.flags & RFL_GLSL)) self=0;
}

CUSTOM_CVAR(Bool, gl_fog_shader, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG|CVAR_NOINITCALL)
{
	if (self && !(gl.flags & RFL_GLSL)) self=0;
}

CUSTOM_CVAR(Bool, gl_colormap_shader, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG|CVAR_NOINITCALL)
{
	if (self && !(gl.flags & RFL_GLSL)) self=0;
}

CUSTOM_CVAR(Bool, gl_brightmap_shader, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG|CVAR_NOINITCALL)
{
	if (self && !(gl.flags & RFL_GLSL)) self=0;
}

CUSTOM_CVAR(Bool, gl_glow_shader, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG|CVAR_NOINITCALL)
{
	if (self && !(gl.flags & RFL_GLSL)) self=0;
}


extern long gl_frameMS;

bool gl_fogenabled;
bool gl_textureenabled;
bool gl_glowenabled;
int gl_texturemode;
int gl_brightmapenabled;
static float gl_lightfactor;
static float gl_lightdist;
static float gl_camerapos[3];
static int gl_warpstate;
static int gl_colormapstate;
static bool gl_brightmapstate;
static float gl_warptime;

class FShader
{
	friend class GLShader;

	GLhandleARB hShader;
	GLhandleARB hVertProg;
	GLhandleARB hFragProg;

	int timer_index;
	int desaturation_index;
	int fogenabled_index;
	int texturemode_index;
	int camerapos_index;
	int lightfactor_index;
	int lightdist_index;

	int glowbottomcolor_index;
	int glowtopcolor_index;

	int glowbottomdist_index;
	int glowtopdist_index;

	int currentfogenabled;
	int currenttexturemode;
	float currentlightfactor;
	float currentlightdist;

public:
	FShader()
	{
		hShader = hVertProg = hFragProg = NULL;
		currentfogenabled = currenttexturemode = 0;
		currentlightfactor = currentlightdist = 0.0f;
	}

	~FShader();

	bool Load(const char * name, const char * vertprog, const char * fragprog);

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

	void SetLightDist(float dist)
	{
		if (dist != currentlightdist)
		{
			currentlightdist = dist;
			gl.Uniform1fARB(lightdist_index, dist);
		}
	}

	void SetLightFactor(float fac)
	{
		if (fac != currentlightfactor)
		{
			currentlightfactor = fac;
			gl.Uniform1fARB(lightfactor_index, fac);
		}
	}

	void SetCameraPos(float x, float y, float z)
	{
		gl.Uniform3fARB(camerapos_index, x, y, z); 
	}

	void SetGlowParams(float *topcolors, float topheight, float *bottomcolors, float bottomheight)
	{
		gl.Uniform4fARB(glowtopcolor_index, topcolors[0], topcolors[1], topcolors[2], topheight);
		gl.Uniform4fARB(glowbottomcolor_index, bottomcolors[0], bottomcolors[1], bottomcolors[2], bottomheight);
	}

	void SetGlowPosition(float topdist, float bottomdist)
	{
		gl.VertexAttrib1fARB(glowtopdist_index, topdist);
		gl.VertexAttrib1fARB(glowbottomdist_index, bottomdist);
	}

	bool Bind(float Speed);

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
		camerapos_index = gl.GetUniformLocationARB(hShader, "camerapos");
		lightdist_index = gl.GetUniformLocationARB(hShader, "lightdist");
		lightfactor_index = gl.GetUniformLocationARB(hShader, "lightfactor");

		glowbottomcolor_index = gl.GetUniformLocationARB(hShader, "bottomglowcolor");
		glowbottomdist_index = gl.GetAttribLocationARB(hShader, "bottomdistance");
		glowtopcolor_index = gl.GetUniformLocationARB(hShader, "topglowcolor");
		glowtopdist_index = gl.GetAttribLocationARB(hShader, "topdistance");

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

bool FShader::Bind(float Speed)
{
	if (gl_activeShader!=this)
	{
		gl.UseProgramObjectARB(hShader);
		gl_activeShader=this;
	}
	if (timer_index >=0 && Speed > 0.f) gl.Uniform1fARB(timer_index, gl_frameMS*Speed/1000.f);
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
	FString res;

	// can be empty.
	if (gettexel)
	{
		int lump1 = Wads.GetNumForFullName(gettexel);
		FMemLump data1 = Wads.ReadLump(lump1);
		res << (char*)data1.GetMem() << '\n';
	}

	int lump2 = Wads.GetNumForFullName(lighting);
	int lump3 = Wads.GetNumForFullName(main);

	FMemLump data2 = Wads.ReadLump(lump2);
	FMemLump data3 = Wads.ReadLump(lump3);

	res << (char*)data2.GetMem() << '\n' << (char*)data3.GetMem();
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
		const char * VertexShader;
		const char * lightpixelfunc;
	};

	static Lighting default_cm[]={
		{ "Standard",	"shaders/main_nofog.vp",	"shaders/light/light_norm.fp"		},
		{ "Inverse",	"shaders/main_nofog.vp",	"shaders/light/light_inverse.fp"	},
		{ "Gold",		"shaders/main_nofog.vp",	"shaders/light/light_gold.fp"		},
		{ "Red",		"shaders/main_nofog.vp",	"shaders/light/light_red.fp"		},
		{ "Green",		"shaders/main_nofog.vp",	"shaders/light/light_green.fp"		},
	};

	static Lighting default_light[]={
		{ "Standard",	"shaders/main.vp",			"shaders/light/light_eyefog.fp"		},
		{ "Brightmap",	"shaders/main.vp",			"shaders/light/light_brightmap.fp"	},
		{ "Glow",		"shaders/main_glow.vp",		"shaders/light/light_glow.fp"		},
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
			const char *main_fp = ShaderPath? "shaders/main.fp" : "shaders/main_notex.fp";
			FString frag = CombineFragmentShader(ShaderPath, default_cm[i].lightpixelfunc, main_fp);

			int vlump = Wads.GetNumForFullName(default_cm[i].VertexShader);
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

	for(int i=0;i<3;i++) for(int j=0;j<2;j++)
	{
		FString name;

		name << ShaderName << "::" << default_light[i].LightingName;
		if (j) name << "::desat";

		try
		{
			const char *main_fp = ShaderPath? main_fp2[j] : "shaders/main_notex.fp";
			FString frag = CombineFragmentShader(ShaderPath, default_light[i].lightpixelfunc, main_fp);

			int vlump = Wads.GetNumForFullName(default_light[i].VertexShader);
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

	for(int i=0;i<3;i++) for(int j=0;j<2;j++)
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
		{"No Texture", NULL },
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

	for(unsigned int i=0;i<AllContainers.Size();i++)
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

class GLShader
{
	FName Name;
	FShaderContainer *container;

public:

	static void Initialize();
	static void Clear();
	static GLShader *Find(const char * shn);
	static GLShader *Find(int warp);
	void Bind(int cm, int lightmode, float Speed);
	static void Unbind();

};

static TArray<GLShader *> AllShaders;

void GLShader::Initialize()
{
	if (gl.flags & RFL_GLSL)
	{
		for(int i=0;i<4;i++)
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
		for(unsigned int i=0;i<AllContainers.Size();i++)
		{
			delete AllContainers[i];
		}
		for(unsigned int i=0;i<AllShaders.Size();i++)
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

	for(unsigned int i=0;i<AllShaders.Size();i++)
	{
		if (AllContainers[i]->Name == sfn)
		{
			return AllShaders[i];
		}
	}
	return NULL;
}

GLShader *GLShader::Find(int warp)
{
	// indices 0-2 match the warping modes, 3 is the no texture shader
	if (warp < AllShaders.Size())
	{
		return AllShaders[warp];
	}
	return NULL;
}


void GLShader::Bind(int cm, int lightmode, float Speed)
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
			sh->Bind(Speed);
		break;

	default:
		bool desat = cm>=CM_DESAT1 && cm<=CM_DESAT31;
		sh = container->shader_light[lightmode][desat];
		// [BB] If there was a problem when loading the shader, sh is NULL here.
		if( sh )
		{
			sh->Bind(Speed);
			if (desat)
			{
				gl.Uniform1fARB(sh->desaturation_index, 1.f-float(cm-CM_DESAT0)/(CM_DESAT31-CM_DESAT0));
			}
		}
		break;
	}
}

void GLShader::Unbind()
{
	if ((gl.flags & RFL_GLSL) && gl_activeShader != NULL)
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
	if (on)
	{
		if (!gl_textureenabled) 
		{
			gl.Enable(GL_TEXTURE_2D);
		}
	}
	else 
	{
		if (gl_textureenabled) 
		{
			gl.Disable(GL_TEXTURE_2D);
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
}


//==========================================================================
//
// set texture mode
//
//==========================================================================

void gl_SetTextureMode(int which)
{
	if (which != gl_texturemode) gl.SetTextureMode(which);
	gl_texturemode = which;
}


//==========================================================================
//
// Lighting stuff 
//
//==========================================================================

void gl_SetShaderLight(float level, float olight)
{
#if 1 //ndef _DEBUG
	const float MAXDIST = 256.f;
	const float THRESHOLD = 96.f;
	const float FACTOR = 0.75f;
#else
	const float MAXDIST = 256.f;
	const float THRESHOLD = 96.f;
	const float FACTOR = 2.75f;
#endif


		
	if (olight < THRESHOLD)
	{
		gl_lightdist = olight * MAXDIST / THRESHOLD;
		olight = THRESHOLD;
	}
	else gl_lightdist = MAXDIST;

	gl_lightfactor = (olight/level);
	gl_lightfactor = 1.f + (gl_lightfactor - 1.f) * FACTOR;
}

//==========================================================================
//
// Sets camera position
//
//==========================================================================

void gl_SetCamera(float x, float y, float z)
{
	gl_camerapos[0] = x;
	gl_camerapos[1] = z;
	gl_camerapos[2] = y;
}

//==========================================================================
//
// Glow stuff
//
//==========================================================================


void gl_SetGlowParams(float *topcolors, float topheight, float *bottomcolors, float bottomheight)
{
	if (gl_activeShader) gl_activeShader->SetGlowParams(topcolors, topheight, bottomcolors, bottomheight);
}

void gl_SetGlowPosition(float topdist, float bottomdist)
{
	if (gl_activeShader) gl_activeShader->SetGlowPosition(topdist, bottomdist);
}

//==========================================================================
//
// Set texture shader info
//
//==========================================================================

void gl_SetTextureShader(int warped, int cm, bool usebright, float warptime)
{
	gl_warpstate = warped;
	gl_colormapstate = cm;
	gl_brightmapstate = usebright;
	gl_warptime = warptime;
}

//==========================================================================
//
// Apply shader settings
//
//==========================================================================
CVAR(Bool, gl_no_shaders, 0, 0) // For shader debugging.

void gl_ApplyShader()
{
	if (gl.flags & RFL_GLSL && !gl_no_shaders)
	{
		if (
			(gl_fogenabled && (gl_fogmode == 2 || gl_fog_shader) && gl_fogmode != 0) || // fog requires a shader
			(gl_textureenabled && (gl_warpstate != 0 || gl_brightmapstate || gl_colormapstate)) ||		// warp or brightmap
			(gl_glowenabled)		// glow requires a shader
			)
		{
			// we need a shader
			int index = gl_textureenabled? gl_warpstate : 3;
			GLShader *shd = GLShader::Find(index);

			if (shd != NULL)
			{
				int lightmode = gl_glowenabled? 2 : gl_brightmapstate? 1:0;
				shd->Bind(gl_colormapstate, lightmode, gl_warptime);

				if (gl_activeShader)
				{
					gl_activeShader->SetTextureMode(gl_texturemode);
					gl_activeShader->SetFogEnabled(gl_fogenabled? gl_fogmode : 0);
					gl_activeShader->SetCameraPos(gl_camerapos[0], gl_camerapos[1], gl_camerapos[2]);
					gl_activeShader->SetLightFactor(gl_lightfactor);
					gl_activeShader->SetLightDist(gl_lightdist);
				}
			}
		}
		else
		{
			GLShader::Unbind();
		}
	}
}

void gl_InitShaders()
{
	GLShader::Initialize();
}

void gl_DisableShader()
{
	GLShader::Unbind();
}

void gl_ClearShaders()
{
	GLShader::Clear();
}
