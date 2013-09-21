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
#include "gl/system/gl_system.h"
#include "c_cvars.h"
#include "v_video.h"
#include "name.h"
#include "w_wad.h"
#include "i_system.h"
#include "doomerrors.h"
#include "v_palette.h"

#include "gl/system/gl_cvars.h"
#include "gl/shaders/gl_shader.h"
#include "gl/textures/gl_material.h"

// these will only have an effect on SM3 cards.
// For SM4 they are always on and for SM2 always off
CVAR(Bool, gl_warp_shader, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG|CVAR_NOINITCALL)
CVAR(Bool, gl_fog_shader, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG|CVAR_NOINITCALL)
CVAR(Bool, gl_colormap_shader, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG|CVAR_NOINITCALL)
CVAR(Bool, gl_brightmap_shader, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG|CVAR_NOINITCALL)
CVAR(Bool, gl_glow_shader, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG|CVAR_NOINITCALL)


extern long gl_frameMS;
extern PalEntry gl_CurrentFogColor;

bool gl_fogenabled;
bool gl_textureenabled;
bool gl_glowenabled;
int gl_texturemode;
int gl_brightmapenabled;
static float gl_lightfactor;
static float gl_lightdist;
static float gl_camerapos[3];
static int gl_effectstate;
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
	int colormapstart_index;
	int colormaprange_index;

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

	bool Load(const char * name, const char * vert_prog_lump, const char * fragprog, const char * fragprog2, const char *defines);

	void SetFogEnabled(int on)
	{
		if (on != currentfogenabled)
		{
			currentfogenabled = on;
			gl.Uniform1i(fogenabled_index, on); 
		}
	}

	void SetTextureMode(int mode)
	{
		if (mode != currenttexturemode)
		{
			currenttexturemode = mode;
			gl.Uniform1i(texturemode_index, mode); 
		}
	}

	void SetLightDist(float dist)
	{
		if (dist != currentlightdist)
		{
			currentlightdist = dist;
			gl.Uniform1f(lightdist_index, dist);
		}
	}

	void SetLightFactor(float fac)
	{
		if (fac != currentlightfactor)
		{
			currentlightfactor = fac;
			gl.Uniform1f(lightfactor_index, fac);
		}
	}

	void SetColormapColor(float r, float g, float b, float r1, float g1, float b1)
	{
		//if (fac != currentlightfactor)
		{
			//currentlightfactor = fac;
			gl.Uniform4f(colormapstart_index, r,g,b,0);
			gl.Uniform4f(colormaprange_index, r1-r,g1-g,b1-b,0);
		}
	}

	void SetCameraPos(float x, float y, float z)
	{
		gl.Uniform3f(camerapos_index, x, y, z); 
	}

	void SetGlowParams(float *topcolors, float topheight, float *bottomcolors, float bottomheight)
	{
		gl.Uniform4f(glowtopcolor_index, topcolors[0], topcolors[1], topcolors[2], topheight);
		gl.Uniform4f(glowbottomcolor_index, bottomcolors[0], bottomcolors[1], bottomcolors[2], bottomheight);
	}

	void SetGlowPosition(float topdist, float bottomdist)
	{
		gl.VertexAttrib1f(glowtopdist_index, topdist);
		gl.VertexAttrib1f(glowbottomdist_index, bottomdist);
	}

	bool Bind(float Speed);

};


static FShader *gl_activeShader;

//==========================================================================
//
//
//
//==========================================================================

bool FShader::Load(const char * name, const char * vert_prog_lump, const char * frag_prog_lump, const char * proc_prog_lump, const char * defines)
{
	static char buffer[10000];

	if (gl.shadermodel > 0)
	{
		int vp_lump = Wads.CheckNumForFullName(vert_prog_lump);
		if (vp_lump == -1) I_Error("Unable to load '%s'", vert_prog_lump);
		FMemLump vp_data = Wads.ReadLump(vp_lump);

		int fp_lump = Wads.CheckNumForFullName(frag_prog_lump);
		if (fp_lump == -1) I_Error("Unable to load '%s'", frag_prog_lump);
		FMemLump fp_data = Wads.ReadLump(fp_lump);

		int pp_lump = Wads.CheckNumForFullName(proc_prog_lump);
		if (pp_lump == -1) I_Error("Unable to load '%s'", proc_prog_lump);
		FMemLump pp_data = Wads.ReadLump(pp_lump);

		FString fp_comb;
		if (gl.shadermodel < 4) fp_comb = "#define NO_SM4\n";
		// This uses GetChars on the strings to get rid of terminating 0 characters.
		fp_comb << defines << fp_data.GetString().GetChars() << "\n" << pp_data.GetString().GetChars();

		hVertProg = gl.CreateShaderObject(GL_VERTEX_SHADER);
		hFragProg = gl.CreateShaderObject(GL_FRAGMENT_SHADER);	


		int vp_size = (int)vp_data.GetSize();
		int fp_size = (int)fp_comb.Len();

		const char *vp_ptr = vp_data.GetString().GetChars();
		const char *fp_ptr = fp_comb.GetChars();

		gl.ShaderSource(hVertProg, 1, &vp_ptr, &vp_size);
		gl.ShaderSource(hFragProg, 1, &fp_ptr, &fp_size);

		gl.CompileShader(hVertProg);
		gl.CompileShader(hFragProg);

		hShader = gl.CreateProgramObject();

		gl.AttachObject(hShader, hVertProg);
		gl.AttachObject(hShader, hFragProg);

		gl.LinkProgram(hShader);
	
		gl.GetInfoLog(hShader, 10000, NULL, buffer);
		if (*buffer) 
		{
			Printf("Init Shader '%s':\n%s\n", name, buffer);
		}
		int linked;
		gl.GetObjectParameteriv(hShader, GL_LINK_STATUS, &linked);
		timer_index = gl.GetUniformLocation(hShader, "timer");
		desaturation_index = gl.GetUniformLocation(hShader, "desaturation_factor");
		fogenabled_index = gl.GetUniformLocation(hShader, "fogenabled");
		texturemode_index = gl.GetUniformLocation(hShader, "texturemode");
		camerapos_index = gl.GetUniformLocation(hShader, "camerapos");
		lightdist_index = gl.GetUniformLocation(hShader, "lightdist");
		lightfactor_index = gl.GetUniformLocation(hShader, "lightfactor");
		colormapstart_index = gl.GetUniformLocation(hShader, "colormapstart");
		colormaprange_index = gl.GetUniformLocation(hShader, "colormaprange");

		glowbottomcolor_index = gl.GetUniformLocation(hShader, "bottomglowcolor");
		glowbottomdist_index = gl.GetAttribLocation(hShader, "bottomdistance");
		glowtopcolor_index = gl.GetUniformLocation(hShader, "topglowcolor");
		glowtopdist_index = gl.GetAttribLocation(hShader, "topdistance");

		int texture2_index = gl.GetUniformLocation(hShader, "texture2");

		if (texture2_index > 0)
		{
			gl.UseProgramObject(hShader);
			gl.Uniform1i(texture2_index, 1);
			gl.UseProgramObject(0);
		}

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
	gl.DeleteObject(hShader);
	gl.DeleteObject(hVertProg);
	gl.DeleteObject(hFragProg);
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
		gl.UseProgramObject(hShader);
		gl_activeShader=this;
	}
	if (timer_index >=0 && Speed > 0.f) gl.Uniform1f(timer_index, gl_frameMS*Speed/1000.f);
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

	enum { NUM_SHADERS = 4 };

	FShader *shader[NUM_SHADERS];
	FShader *shader_cm;	// the shader for fullscreen colormaps

public:
	FShaderContainer(const char *ShaderName, const char *ShaderPath);
	~FShaderContainer();
	
};

//==========================================================================
//
//
//
//==========================================================================

FShaderContainer::FShaderContainer(const char *ShaderName, const char *ShaderPath)
{
	const char * shaderdefines[] = {
		"#define NO_GLOW\n#define NO_DESATURATE\n",
		"#define NO_DESATURATE\n",
		"#define NO_GLOW\n",
		"\n"
	};

	const char * shaderdesc[] = {
		"::default",
		"::glow",
		"::desaturate",
		"::glow+desaturate"
	};

	FString name;

	name << ShaderName << "::colormap";

	try
	{
		shader_cm = new FShader;
		if (!shader_cm->Load(name, "shaders/glsl/main_colormap.vp", "shaders/glsl/main_colormap.fp", ShaderPath, "\n"))
		{
			delete shader_cm;
			shader_cm = NULL;
		}
	}
	catch(CRecoverableError &err)
	{
		shader_cm = NULL;
		Printf("Unable to load shader %s:\n%s\n", name.GetChars(), err.GetMessage());
		I_Error("");
	}

	if (gl.shadermodel > 2)
	{
		for(int i = 0;i < NUM_SHADERS; i++)
		{
			FString name;

			name << ShaderName << shaderdesc[i];

			try
			{
				shader[i] = new FShader;
				if (!shader[i]->Load(name, "shaders/glsl/main.vp", "shaders/glsl/main.fp", ShaderPath, shaderdefines[i]))
				{
					delete shader[i];
					shader[i] = NULL;
				}
			}
			catch(CRecoverableError &err)
			{
				shader[i] = NULL;
				Printf("Unable to load shader %s:\n%s\n", name.GetChars(), err.GetMessage());
				I_Error("");
			}
		}
	}
	else memset(shader, 0, sizeof(shader));
}

//==========================================================================
//
//
//
//==========================================================================
FShaderContainer::~FShaderContainer()
{
	delete shader_cm;
	for(int i = 0;i < NUM_SHADERS; i++)
	{
		if (shader[i] != NULL)
		{
			delete shader[i];
			shader[i] = NULL;
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
		{"Default",	"shaders/glsl/func_normal.fp"},
		{"Warp 1",	"shaders/glsl/func_warp1.fp"},
		{"Warp 2",	"shaders/glsl/func_warp2.fp"},
		{"Brightmap","shaders/glsl/func_brightmap.fp"},
		{"No Texture", "shaders/glsl/func_notexture.fp"},
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
	static GLShader *Find(unsigned int warp);
	void Bind(int cm, bool glowing, float Speed);
	static void Unbind();

};

static TArray<GLShader *> AllShaders;

void GLShader::Initialize()
{
	if (gl.shadermodel > 0)
	{
		for(int i=0;defaultshaders[i].ShaderName != NULL;i++)
		{
			FShaderContainer * shc = AddShader(defaultshaders[i].ShaderName, defaultshaders[i].gettexelfunc);
			GLShader * shd = new GLShader;
			shd->container = shc;
			shd->Name = defaultshaders[i].ShaderName;
			AllShaders.Push(shd);
			if (gl.shadermodel <= 2) break;	// SM2 will only initialize the default shader
		}
	}
}

void GLShader::Clear()
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

GLShader *GLShader::Find(unsigned int warp)
{
	// indices 0-2 match the warping modes, 3 is brightmap, 4 no texture, the following are custom
	if (warp < AllShaders.Size())
	{
		return AllShaders[warp];
	}
	return NULL;
}


void GLShader::Bind(int cm, bool glowing, float Speed)
{
	FShader *sh=NULL;

	if (cm >= CM_FIRSTSPECIALCOLORMAP && cm < CM_FIRSTSPECIALCOLORMAP + SpecialColormaps.Size())
	{
		// these are never used with any kind of lighting or fog
		sh = container->shader_cm;
		// [BB] If there was a problem when loading the shader, sh is NULL here.
		if( sh )
		{
			FSpecialColormap *map = &SpecialColormaps[cm - CM_FIRSTSPECIALCOLORMAP];
			sh->Bind(Speed);
			sh->SetColormapColor(map->ColorizeStart[0], map->ColorizeStart[1], map->ColorizeStart[2],
				map->ColorizeEnd[0], map->ColorizeEnd[1], map->ColorizeEnd[2]);
		}
	}
	else
	{
		bool desat = cm>=CM_DESAT1 && cm<=CM_DESAT31;
		sh = container->shader[glowing + 2*desat];
		// [BB] If there was a problem when loading the shader, sh is NULL here.
		if( sh )
		{
			sh->Bind(Speed);
			if (desat)
			{
				gl.Uniform1f(sh->desaturation_index, 1.f-float(cm-CM_DESAT0)/(CM_DESAT31-CM_DESAT0));
			}
		}
	}
}

void GLShader::Unbind()
{
	if (gl.shadermodel > 0 && gl_activeShader != NULL)
	{
		/* if (gl.shadermodel == 4)
		{
			// set a default shader here.
		}
		*/
		gl.UseProgramObject(0);
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
	gl_textureenabled=on;
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
	if (which != gl_texturemode && which != -1) gl.SetTextureMode(which);
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
	if (gl_lightfactor == 1.f) gl_lightdist = 0.f;	// save some code in the shader
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

int gl_SetupShader(bool cameratexture, int &shaderindex, int &cm, float warptime)
{
	bool usecmshader = false;
	int softwarewarp = 0;

	// fixme: move this check into shader class
	if (shaderindex == 3)
	{
		// Brightmap should not be used.
		if (!gl_brightmapenabled || cm >= CM_FIRSTSPECIALCOLORMAP)
		{
			shaderindex = 0;
		}
	}

	// selectively disable shader features depending on available feature set.
	switch (gl.shadermodel)
	{
	case 4:
		usecmshader = cm > CM_DEFAULT && cm < CM_FIRSTSPECIALCOLORMAP + SpecialColormaps.Size() && 
			gl_texturemode != TM_MASK;
		break;

	case 3:
		usecmshader = (cameratexture || gl_colormap_shader) && 
			cm > CM_DEFAULT && cm < CM_FIRSTSPECIALCOLORMAP + SpecialColormaps.Size() && 
			gl_texturemode != TM_MASK;

		if (!gl_brightmap_shader && shaderindex >= 3) 
		{
			shaderindex = 0;
		}
		else if (!gl_warp_shader && shaderindex < 3)
		{
			softwarewarp = shaderindex;
			shaderindex = 0;
		}
		break;

	case 2:
		usecmshader = !!(cameratexture);
		softwarewarp = shaderindex < 3? shaderindex : 0;
		shaderindex = 0;
		break;

	default:
		softwarewarp = shaderindex < 3? shaderindex : 0;
		shaderindex = 0;
		return softwarewarp;
	}

	gl_effectstate = shaderindex;
	gl_colormapstate = usecmshader? cm : CM_DEFAULT;
	if (usecmshader) cm = CM_DEFAULT;
	gl_warptime = warptime;
	return softwarewarp;
}


//==========================================================================
//
// Apply shader settings
//
//==========================================================================

void gl_ApplyShader()
{
	bool useshaders = false;

	switch (gl.shadermodel)
	{
	case 2:
		useshaders = (gl_textureenabled && gl_colormapstate != CM_DEFAULT);
		break;

	case 3:
		useshaders = (
			gl_effectstate != 0 ||	// special shaders
			(gl_fogenabled && (gl_fogmode == 2 || gl_fog_shader) && gl_fogmode != 0) || // fog requires a shader
			(gl_textureenabled && (gl_effectstate != 0 || gl_colormapstate)) ||		// colormap
			(gl_glowenabled)		// glow requires a shader
			);
		break;

	case 4:
		// useshaders = true;
		useshaders = (
			gl_effectstate != 0 ||	// special shaders
			(gl_fogenabled && gl_fogmode != 0) || // fog requires a shader
			(gl_textureenabled && gl_colormapstate) ||	// colormap
			(gl_glowenabled)		// glow requires a shader
			);
		break;

	default:
		break;
	}

	if (useshaders)
	{
		// we need a shader
		GLShader *shd = GLShader::Find(gl_textureenabled? gl_effectstate : 4);

		if (shd != NULL)
		{
			shd->Bind(gl_colormapstate, gl_glowenabled, gl_warptime);

			if (gl_activeShader)
			{
				gl_activeShader->SetTextureMode(gl_texturemode);

				int fogset = 0;
				if (gl_fogenabled)
				{
					if ((gl_CurrentFogColor & 0xffffff) == 0)
					{
						fogset = gl_fogmode;
					}
					else
					{
						fogset = -gl_fogmode;
					}
				}

				gl_activeShader->SetFogEnabled(fogset);
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

bool gl_BrightmapsActive()
{
	return gl.shadermodel == 4 || (gl.shadermodel == 3 && gl_brightmap_shader);
}

bool gl_GlowActive()
{
	return gl.shadermodel == 4 || (gl.shadermodel == 3 && gl_glow_shader);
}

bool gl_ExtFogActive()
{
	return gl.shadermodel == 4;
}
