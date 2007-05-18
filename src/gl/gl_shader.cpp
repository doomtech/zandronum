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


FShader * activeShader;

FShader * shader_Null;
FShader * shader_InverseMap;
FShader * shader_GoldMap;
FShader * shader_RedMap;
FShader * shader_GreenMap;
FShader * shader_Warp1;
FShader * shader_Warp1_NoFog;
FShader * shader_Warp2;
FShader * shader_Warp2_NoFog;

static FName warpinfo_name;

static bool gl_fogenabled;
static bool gl_textureenabled;


//==========================================================================
//
//
//
//==========================================================================

bool FShader::Load(const char * name, const char * vp_path, const char * fp_path)
{
	static char buffer[10000];

	if (gl.flags & RFL_GLSL)
	{
		hVertProg = gl.CreateShaderObjectARB(GL_VERTEX_SHADER_ARB);
		hFragProg = gl.CreateShaderObjectARB(GL_FRAGMENT_SHADER_ARB);	

		int vp_lump = Wads.CheckNumForFullName(vp_path);
		int fp_lump = Wads.CheckNumForFullName(fp_path);

		if (vp_lump<0 || fp_lump<0)
		{
			Printf(PRINT_HIGH, "Source for shader %s not found\n", name);
			return false;
		}

		FMemLump vp_mem = Wads.ReadLump(vp_lump);
		FMemLump fp_mem = Wads.ReadLump(fp_lump);

		int vp_size = Wads.LumpLength(vp_lump);
		int fp_size = Wads.LumpLength(fp_lump);

		const GLcharARB * vp_memp = (const GLcharARB*)vp_mem.GetMem();
		const GLcharARB * fp_memp = (const GLcharARB*)fp_mem.GetMem();

		gl.ShaderSourceARB(hVertProg, 1, &vp_memp, &vp_size);
		gl.ShaderSourceARB(hFragProg, 1, &fp_memp, &fp_size);

		gl.CompileShaderARB(hVertProg);
		gl.CompileShaderARB(hFragProg);

		hShader = gl.CreateProgramObjectARB();

		gl.AttachObjectARB(hShader, hVertProg);
		gl.AttachObjectARB(hShader, hFragProg);

		gl.LinkProgramARB(hShader);
	
		gl.GetInfoLogARB(hShader, 10000, NULL, buffer);
		if (*buffer) 
		{
			Printf("Init Shader '%s': %s\n", name, buffer);
		}
		int linked;
		gl.GetObjectParameterivARB(hShader, GL_OBJECT_LINK_STATUS_ARB, &linked);
		return !!linked;
	}
	return false;
}


//==========================================================================
//
//
//
//==========================================================================

void FShader::AddParameter(const char * pname)
{
	FName nm = FName(pname);

	for(int i=0;i<params.Size();i++)
	{
		if (param_names[i]==nm) return;
	}
	param_names.Push(nm);
	params.Push(gl.GetAttribLocationARB(hShader, pname));
}

//==========================================================================
//
//
//
//==========================================================================

int FShader::GetParameterIndex(FName pname)
{
	for(int i=0;i<params.Size();i++)
	{
		if (param_names[i]==pname) return params[i];
	}
	return -1;
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

bool FShader::Bind()
{
	if (activeShader!=this)
	{
		gl.UseProgramObjectARB(hShader);
		activeShader=this;
	}
	return true;
}

//==========================================================================
//
// Initializes the GLSL shaders used by the engine
//
//==========================================================================

void gl_ClearShaders()
{
	if (gl.flags & RFL_GLSL)
	{
		gl.UseProgramObjectARB(0);
		if (shader_Null) delete shader_Null;
		if (shader_InverseMap) delete shader_InverseMap;
		if (shader_GoldMap) delete shader_GoldMap;
		if (shader_Warp1) delete shader_Warp1;
		if (shader_Warp1_NoFog) delete shader_Warp1_NoFog;
		if (shader_Warp2) delete shader_Warp2;
		if (shader_Warp2_NoFog) delete shader_Warp2_NoFog;
	}
}

void gl_InitShaders() 
{
	if (gl.flags & RFL_GLSL)
	{
		shader_Null = new FShader;

		shader_InverseMap = new FShader;
		if (!shader_InverseMap->Load("Inverse", "shaders/inverse.vp", "shaders/inverse.fp"))
		{
			delete shader_InverseMap;
			shader_InverseMap=NULL;
		}

		shader_GoldMap = new FShader;
		if (!shader_GoldMap->Load("Gold", "shaders/gold.vp", "shaders/gold.fp"))
		{
			delete shader_GoldMap;
			shader_GoldMap=NULL;
		}

		shader_RedMap = new FShader;
		if (!shader_RedMap->Load("Red", "shaders/red.vp", "shaders/red.fp"))
		{
			delete shader_RedMap;
			shader_RedMap=NULL;
		}

		shader_GreenMap = new FShader;
		if (!shader_GreenMap->Load("Green", "shaders/green.vp", "shaders/green.fp"))
		{
			delete shader_GreenMap;
			shader_GreenMap=NULL;
		}

		shader_Warp1 = new FShader;
		if (!shader_Warp1->Load("Warp1", "shaders/warp1.vp", "shaders/warp1.fp"))
		{
			delete shader_Warp1;
			shader_Warp1=NULL;
		}
		else shader_Warp1->AddParameter("warpInfo");

		shader_Warp1_NoFog = new FShader;
		if (!shader_Warp1_NoFog->Load("Warp1NoFog", "shaders/warp1ovr.vp", "shaders/warp1ovr.fp"))
		{
			delete shader_Warp1_NoFog;
			shader_Warp1_NoFog=NULL;
		}
		else shader_Warp1_NoFog->AddParameter("warpInfo");

		shader_Warp2 = new FShader;
		if (!shader_Warp2->Load("Warp2", "shaders/warp2.vp", "shaders/warp2.fp"))
		{
			delete shader_Warp2;
			shader_Warp2=NULL;
		}
		else shader_Warp2->AddParameter("warpInfo");

		shader_Warp2_NoFog = new FShader;
		if (!shader_Warp2_NoFog->Load("Warp2NoFog", "shaders/warp2ovr.vp", "shaders/warp2ovr.fp"))
		{
			delete shader_Warp2_NoFog;
			shader_Warp2_NoFog=NULL;
		}
		else shader_Warp2_NoFog->AddParameter("warpInfo");

		warpinfo_name = FName("warpInfo");
	}
}


//==========================================================================
//
// Sets a shader for invulnerability colormaps
// Currently it is only used for camera textures.
//
//==========================================================================

bool gl_SetColorMode(int cm, bool force)
{
	if (gl.flags&RFL_GLSL)
	{
		// Only used for camera textures!
		static int lastcmap=-1;

		if (lastcmap==cm) return true;

		switch(force? cm : CM_INVALID)
		{
		default:
			cm=CM_DEFAULT;
		case CM_DEFAULT:
			shader_Null->Bind();
			break;

		case CM_INVERT:
			if (!shader_InverseMap) return false;
			shader_InverseMap->Bind();
			break;
		case CM_GOLDMAP:
			if (!shader_GoldMap) return false;
			shader_GoldMap->Bind();
			break;
		case CM_REDMAP:
			if (!shader_RedMap) return false;
			shader_RedMap->Bind();
			break;
		case CM_GREENMAP:
			if (!shader_GreenMap) return false;
			shader_GreenMap->Bind();
			break;
		}
		lastcmap=cm;
		return force;
	}
	return false;
}

//==========================================================================
//
// Sets a shader for warped textures
//
//==========================================================================

bool gl_SetShaderForWarp(int type, float time)
{
	FShader * shader;
	int parm;

	switch (type)
	{
	default:	// 0 means 'no warping shader'
		shader_Null->Bind();
		return true;

	case 1:

		if (gl_fogenabled) shader = shader_Warp1;
		else shader = shader_Warp1_NoFog;
		if (!shader) 
		{
			shader_Null->Bind();
			return false;
		}
		parm = shader->GetParameterIndex(warpinfo_name);
		gl.VertexAttrib4fARB(parm, time/2.f, 2.f, 0, 0);
		break;

	case 2:

		if (gl_fogenabled) shader = shader_Warp2;
		else shader = shader_Warp2_NoFog;
		if (!shader) 
		{
			shader_Null->Bind();
			return false;
		}
		parm = shader->GetParameterIndex(warpinfo_name);
		gl.VertexAttrib4fARB(parm, time, 1.f, time, 1.f);
		break;
	}

	shader->Bind();

	return true;
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
		if (!gl_textureenabled) gl.Enable(GL_TEXTURE_2D);
		gl_textureenabled=true;
	}
	else if (gl_textureenabled)
	{
		gl.Disable(GL_TEXTURE_2D);
		gl_SetColorMode(CM_DEFAULT, false);
		//gl_SetShaderForWarp(0,0);
		gl_textureenabled=false;
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
		if (!gl_fogenabled)
		{
			gl_fogenabled=true;
			gl.Enable(GL_FOG);
		}
	}
	else 
	{
		if (gl_fogenabled)
		{
			gl_fogenabled=false;
			gl.Disable(GL_FOG);
		}
	}
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

inline float gl_ShadeForLight(int lightLevel)
{
   return 2.f - (lightLevel + 12) / 128.f;
}


void gl_SetFogLight(int lightlevel)
{
	/*
	if (i_useshaders)
	{
		gl.VertexAttrib1fARB(a_shade, gl_ShadeForLight(lightlevel));
		gl.VertexAttrib1fARB(a_visibility, TO_MAP(r_WallVisibility) / 32.f); 
	}
	*/
}

