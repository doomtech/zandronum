#if 0
/*
** gl_shaders.cpp
** Routines parsing/managing texture shaders.
**
**---------------------------------------------------------------------------
** Copyright 2003 Timothy Stump
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
#include "c_cvars.h"
#include "c_dispatch.h"
#include "gl/gl_swshaders.h"
#include "sc_man.h"
#include "w_wad.h"
#include "v_text.h"
#include "zstring.h"

#include "gi.h"
#include "templates.h"


#if 0
#define LOG1(msg, i) { FILE *f = fopen("shaders.log", "a");  fprintf(f, msg, i);  fclose(f); }
#else
#define LOG1
#endif

#define MAX(a, b) (a > b ? a : b)
#define MIN(a, b) (a < b ? a : b)

//extern PFNGLPROGRAMENVPARAMETER4FARBPROC glProgramEnvParameter4fARB;
extern PFNGLMULTITEXCOORD2FARBPROC glMultiTexCoord2fARB;


CVAR(Bool, gl_texture_useshaders, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

TArray<FSWShader *> Shaders;
int ShaderDepth;

extern int viewpitch;


FShaderLayer::FShaderLayer()
{
	tex = NULL;
	additive=false;
	animate = false;
	emissive = false;
	blendFuncSrc = GL_SRC_ALPHA;
	blendFuncDst = GL_ONE_MINUS_SRC_ALPHA;
	offsetX = 0.f;
	offsetY = 0.f;
	centerX = 0.0f;
	centerY = 0.0f;
	rotate = 0.f;
	rotation = 0.f;
	adjustX.SetParams(0.f, 0.f, 0.f);
	adjustY.SetParams(0.f, 0.f, 0.f);
	scaleX.SetParams(1.f, 1.f, 0.f);
	scaleY.SetParams(1.f, 1.f, 0.f);
	alpha.SetParams(1.f, 1.f, 0.f);
	r.SetParams(1.f, 1.f, 0.f);
	g.SetParams(1.f, 1.f, 0.f);
	b.SetParams(1.f, 1.f, 0.f);
	flags = 0;
	layerMask = NULL;
	texgen = SHADER_TexGen_None;
	warp = false;
}


FShaderLayer::FShaderLayer(const FShaderLayer &layer)
{
	tex = layer.tex;
	additive = layer.additive;

	animate = layer.animate;
	emissive = layer.emissive;
	adjustX = layer.adjustX;
	adjustY = layer.adjustY;
	blendFuncSrc = layer.blendFuncSrc;
	blendFuncDst = layer.blendFuncDst;
	offsetX = layer.offsetX;
	offsetY = layer.offsetY;
	centerX = layer.centerX;
	centerY = layer.centerX;
	rotate = layer.rotate;
	rotation = layer.rotation;
	scaleX = layer.scaleX;
	scaleY = layer.scaleY;
	vectorX = layer.vectorX;
	vectorY = layer.vectorY;
	alpha = layer.alpha;
	r = layer.r;
	g = layer.g;
	b = layer.b;
	flags = layer.flags;
	if (layer.layerMask)
	{
		layerMask = new FShaderLayer(*(layer.layerMask));
	}
	else
	{
		layerMask = NULL;
	}
	texgen = layer.texgen;
	warp = layer.warp;
}


FShaderLayer::~FShaderLayer()
{
	if (layerMask)
	{
		delete layerMask;
		layerMask = NULL;
	}
}


void FShaderLayer::Update(float diff)
{
	r.Update(diff);
	g.Update(diff);
	b.Update(diff);
	alpha.Update(diff);
	vectorY.Update(diff);
	vectorX.Update(diff);
	scaleX.Update(diff);
	scaleY.Update(diff);
	adjustX.Update(diff);
	adjustY.Update(diff);

	offsetX += vectorX * diff;
	if (offsetX >= 1.f) offsetX -= 1.f;
	if (offsetX < 0.f) offsetX += 1.f;

	offsetY += vectorY * diff;
	if (offsetY >= 1.f) offsetY -= 1.f;
	if (offsetY < 0.f) offsetY += 1.f;

	rotation += rotate * diff;
	if (rotation > 360.f) rotation -= 360.f;
	if (rotation < 0.f) rotation += 360.f;
}


FSWShader::FSWShader()
{
	layers.Clear();
	lastUpdate = 0;
	tex = NULL;
	detailTex = NULL;
}


FSWShader::FSWShader(const FSWShader &shader)
{
	unsigned int i;

	for (i = 0; i < shader.layers.Size(); i++)
	{
		layers.Push(new FShaderLayer(*shader.layers[i]));
	}

	lastUpdate = shader.lastUpdate;

	tex = shader.tex;
	detailTex = shader.detailTex;
}


FSWShader::~FSWShader()
{
	unsigned int i;

	for (i = 0; i < layers.Size(); i++)
	{
		delete layers[i];
	}

	layers.Clear();
}


bool FSWShader::operator== (const FSWShader &shader)
{
	return tex == shader.tex;
}


int GL_ShaderIndexForShader(FSWShader *shader)
{
	unsigned int i;

	for (i = 0; i < Shaders.Size(); i++)
	{
		if ((*Shaders[i]) == (*shader))
		{
			return i;
		}
	}

	return -1;
}


void GL_CheckShaderDepth()
{
	char *tmp;

	tmp = strstr(sc_String, "{");
	while (tmp)
	{
		ShaderDepth++;
		tmp++;
		tmp = strstr(tmp, "{");
	}

	tmp = strstr(sc_String, "}");
	while (tmp)
	{
		ShaderDepth--;
		tmp++;
		tmp = strstr(tmp, "}");
	}
}


unsigned int GL_GetBlendFromString(char *string, unsigned int * opposite = NULL)
{
	if (stricmp("GL_ZERO", string) == 0)
	{
		if (opposite) *opposite = GL_ONE;
		return GL_ZERO;
	}
	else if (stricmp("GL_ONE", string) == 0)
	{
		if (opposite) *opposite = GL_ZERO;
		return GL_ONE;
	}
	else if (stricmp("GL_DST_COLOR", string) == 0)
	{
		if (opposite) *opposite = GL_ONE_MINUS_DST_COLOR;
		return GL_DST_COLOR;
	}
	else if (stricmp("GL_ONE_MINUS_DST_COLOR", string) == 0)
	{
		if (opposite) *opposite = GL_DST_COLOR;
		return GL_ONE_MINUS_DST_COLOR;
	}
	else if (stricmp("GL_SRC_ALPHA", string) == 0)
	{
		if (opposite) *opposite = GL_ONE_MINUS_SRC_ALPHA;
		return GL_SRC_ALPHA;
	}
	else if (stricmp("GL_ONE_MINUS_SRC_ALPHA", string) == 0)
	{
		if (opposite) *opposite = GL_SRC_ALPHA;
		return GL_ONE_MINUS_SRC_ALPHA;
	}
	/*
	else if (stricmp("GL_DST_ALPHA", string) == 0)
	{
	return GL_DST_ALPHA;
	}
	else if (stricmp("GL_ONE_MINUS_DST_ALPHA", string) == 0)
	{
	return GL_ONE_MINUS_DST_ALPHA;
	}
	else if (stricmp("GL_SRC_ALPHA_SATURATE", string) == 0)
	{
	return GL_SRC_ALPHA_SATURATE;
	}
	*/
	else if (stricmp("GL_SRC_COLOR", string) == 0)
	{
		if (opposite) *opposite = GL_ONE_MINUS_SRC_COLOR;
		return GL_SRC_COLOR;
	}
	else if (stricmp("GL_ONE_MINUS_SRC_COLOR", string) == 0)
	{
		if (opposite) *opposite = GL_ONE_MINUS_SRC_COLOR;
		return GL_ONE_MINUS_SRC_COLOR;
	}
	if (opposite) *opposite = GL_ZERO;
	return GL_ONE;
}

static const char *LayerTags[]=
{
	"alpha",
		"animate",
		"blendFunc",
		"color",
		"center",
		"emissive",
		"mask",
		"offset",
		"offsetFunc",
		"rotate",
		"rotation",
		"scale",
		"scaleFunc",
		"texgen",
		"vector",
		"vectorFunc",
		"warp",
		NULL
};

enum {
	LAYERTAG_ALPHA,
	LAYERTAG_ANIMATE,
	LAYERTAG_BLENDFUNC,
	LAYERTAG_COLOR,
	LAYERTAG_CENTER,
	LAYERTAG_EMISSIVE,
	LAYERTAG_MASK,
	LAYERTAG_OFFSET,
	LAYERTAG_OFFSETFUNC,
	LAYERTAG_ROTATE,
	LAYERTAG_ROTATION,
	LAYERTAG_SCALE,
	LAYERTAG_SCALEFUNC,
	LAYERTAG_TEXGEN,
	LAYERTAG_VECTOR,
	LAYERTAG_VECTORFUNC,
	LAYERTAG_WARP
};

static const char *CycleTags[]=
{
	"linear",
		"sin",
		"cos",
		"sawtooth",
		"square",
		NULL
};


bool GL_ParseLayer(FSWShader *shader)
{
	float start, end, cycle, r1, r2, g1, g2, b1, b2;
	FShaderLayer sink;
	FSWShader *tempShader;
	int type, incomingDepth;
	bool overrideBlend = false;

	incomingDepth = ShaderDepth;

	if (SC_GetString())
	{
		int texno = TexMan.CheckForTexture(sc_String, FTexture::TEX_Wall, FTextureManager::TEXMAN_Overridable|FTextureManager::TEXMAN_TryAny);
		if (texno==-1)
		{
			Printf("Texture %s not found\n", sc_String);
			return false;
		}
		sink.tex = TexMan[texno];
		SC_GetString();
		GL_CheckShaderDepth();
		while (ShaderDepth > incomingDepth)
		{
			if (SC_GetString() == false)
			{
				ShaderDepth = incomingDepth;
				Printf("Layer parse error: %s\n", sink.tex->Name);
				return false;
			}

			if ((type = SC_MatchString(LayerTags)) != -1)
			{
				switch (type)
				{
				case LAYERTAG_ALPHA:
					SC_GetString();
					if (SC_Compare("cycle"))
					{
						sink.alpha.ShouldCycle(true);
						SC_GetString();
						switch (SC_MatchString(CycleTags))
						{
						case CYCLE_Sin:
							sink.alpha.SetCycleType(CYCLE_Sin);
							break;
						case CYCLE_Cos:
							sink.alpha.SetCycleType(CYCLE_Cos);
							break;
						case CYCLE_SawTooth:
							sink.alpha.SetCycleType(CYCLE_SawTooth);
							break;
						case CYCLE_Square:
							sink.alpha.SetCycleType(CYCLE_Square);
							break;
						case CYCLE_Linear:
							sink.alpha.SetCycleType(CYCLE_Linear);
							break;
						default:
							// cycle type missing, back script up and default to CYCLE_Linear
							sink.alpha.SetCycleType(CYCLE_Linear);
							SC_UnGet();
							break;
						}
						SC_GetFloat();
						start = sc_Float;
						SC_GetFloat();
						end = sc_Float;
						SC_GetFloat();
						cycle = sc_Float;

						sink.alpha.SetParams(start, end, cycle);
					}
					else
					{
						start = static_cast<float>(atof(sc_String));
						sink.alpha.SetParams(start, start, 0.f);
					}
					break;
				case LAYERTAG_ANIMATE:
					SC_GetString();
					sink.animate = SC_Compare("true") == TRUE;
					break;
				case LAYERTAG_BLENDFUNC:
					SC_GetString();
					sink.blendFuncSrc = GL_GetBlendFromString(sc_String, &sink.blendFuncDst);
					//SC_GetString();
					//sink.blendFuncDst = GL_GetBlendFromString(sc_String);
					overrideBlend = true;
					break;
				case LAYERTAG_COLOR:
					SC_GetString();
					if (SC_Compare("cycle"))
					{
						sink.r.ShouldCycle(true);
						sink.g.ShouldCycle(true);
						sink.b.ShouldCycle(true);
						// get color1
						SC_GetString();

						switch (SC_MatchString(CycleTags))
						{
						case CYCLE_Sin:
							sink.r.SetCycleType(CYCLE_Sin);
							sink.g.SetCycleType(CYCLE_Sin);
							sink.b.SetCycleType(CYCLE_Sin);
							break;
						case CYCLE_Cos:
							sink.r.SetCycleType(CYCLE_Cos);
							sink.g.SetCycleType(CYCLE_Cos);
							sink.b.SetCycleType(CYCLE_Cos);
							break;
						case CYCLE_SawTooth:
							sink.r.SetCycleType(CYCLE_SawTooth);
							sink.g.SetCycleType(CYCLE_SawTooth);
							sink.b.SetCycleType(CYCLE_SawTooth);
							break;
						case CYCLE_Square:
							sink.r.SetCycleType(CYCLE_Square);
							sink.g.SetCycleType(CYCLE_Square);
							sink.b.SetCycleType(CYCLE_Square);
							break;
						case CYCLE_Linear:
							sink.r.SetCycleType(CYCLE_Linear);
							sink.g.SetCycleType(CYCLE_Linear);
							sink.b.SetCycleType(CYCLE_Linear);
							break;
						default:
							// cycle type missing, back script up and default to CYCLE_Linear
							sink.r.SetCycleType(CYCLE_Linear);
							sink.g.SetCycleType(CYCLE_Linear);
							sink.b.SetCycleType(CYCLE_Linear);
							SC_UnGet();
							break;
						}

						SC_GetFloat();
						r1 = sc_Float;
						SC_GetFloat();
						g1 = sc_Float;
						SC_GetFloat();
						b1 = sc_Float;

						// get color2
						SC_GetFloat();
						r2 = sc_Float;
						SC_GetFloat();
						g2 = sc_Float;
						SC_GetFloat();
						b2 = sc_Float;

						// get cycle time
						SC_GetFloat();
						cycle = sc_Float;

						sink.r.SetParams(r1, r2, cycle);
						sink.g.SetParams(g1, g2, cycle);
						sink.b.SetParams(b1, b2, cycle);
					}
					else
					{
						r1 = static_cast<float>(atof(sc_String));
						SC_GetFloat();
						g1 = sc_Float;
						SC_GetFloat();
						b1 = sc_Float;

						sink.r.SetParams(r1, r1, 0.f);
						sink.g.SetParams(g1, g1, 0.f);
						sink.b.SetParams(b1, b1, 0.f);
					}
					break;
				case LAYERTAG_CENTER:
					SC_GetFloat();
					sink.centerX = sc_Float;
					SC_GetFloat();
					sink.centerY = sc_Float;
					break;
				case LAYERTAG_EMISSIVE:
					SC_GetString();
					sink.emissive = SC_Compare("true") == TRUE;
					break;
				case LAYERTAG_OFFSET:
					SC_GetString();
					if (SC_Compare("cycle"))
					{
						sink.adjustX.ShouldCycle(true);
						sink.adjustY.ShouldCycle(true);

						SC_GetFloat();
						r1 = sc_Float;
						SC_GetFloat();
						r2 = sc_Float;

						SC_GetFloat();
						g1 = sc_Float;
						SC_GetFloat();
						g2 = sc_Float;

						SC_GetFloat();
						cycle = sc_Float;

						sink.offsetX = r1;
						sink.offsetY = r2;

						sink.adjustX.SetParams(0.f, g1 - r1, cycle);
						sink.adjustY.SetParams(0.f, g2 - r2, cycle);
					}
					else
					{
						SC_UnGet();
						SC_GetFloat();
						sink.offsetX = sc_Float;
						SC_GetFloat();
						sink.offsetY = sc_Float;
					}
					break;
				case LAYERTAG_OFFSETFUNC:
					SC_GetString();
					switch (SC_MatchString(CycleTags))
					{
					case CYCLE_Sin:
						sink.adjustX.SetCycleType(CYCLE_Sin);
						break;
					case CYCLE_Cos:
						sink.adjustX.SetCycleType(CYCLE_Cos);
						break;
					case CYCLE_SawTooth:
						sink.adjustX.SetCycleType(CYCLE_SawTooth);
						break;
					case CYCLE_Square:
						sink.adjustX.SetCycleType(CYCLE_Square);
						break;
					case CYCLE_Linear:
					default:
						sink.adjustX.SetCycleType(CYCLE_Linear);
						break;
					}
					SC_GetString();
					switch (SC_MatchString(CycleTags))
					{
					case CYCLE_Sin:
						sink.adjustY.SetCycleType(CYCLE_Sin);
						break;
					case CYCLE_Cos:
						sink.adjustY.SetCycleType(CYCLE_Cos);
						break;
					case CYCLE_SawTooth:
						sink.adjustY.SetCycleType(CYCLE_SawTooth);
						break;
					case CYCLE_Square:
						sink.adjustY.SetCycleType(CYCLE_Square);
						break;
					case CYCLE_Linear:
					default:
						sink.adjustY.SetCycleType(CYCLE_Linear);
						break;
					}
					break;
				case LAYERTAG_MASK:
					tempShader = new FSWShader();
					GL_ParseLayer(tempShader);
					sink.layerMask = new FShaderLayer(*tempShader->layers[0]);
					delete tempShader;
					break;
				case LAYERTAG_ROTATE:
					SC_GetFloat();
					sink.rotate = sc_Float;
					break;
				case LAYERTAG_ROTATION:
					SC_GetFloat();
					sink.rotation = sc_Float;
					break;
				case LAYERTAG_SCALE:
					SC_GetString();
					if (SC_Compare("cycle"))
					{
						sink.scaleX.ShouldCycle(true);
						sink.scaleY.ShouldCycle(true);

						SC_GetFloat();
						r1 = sc_Float;
						SC_GetFloat();
						r2 = sc_Float;

						SC_GetFloat();
						g1 = sc_Float;
						SC_GetFloat();
						g2 = sc_Float;

						SC_GetFloat();
						cycle = sc_Float;

						sink.scaleX.SetParams(r1, g1, cycle);
						sink.scaleY.SetParams(r2, g2, cycle);
					}
					else
					{
						SC_UnGet();
						SC_GetFloat();
						sink.scaleX.SetParams(sc_Float, sc_Float, 0.f);
						SC_GetFloat();
						sink.scaleY.SetParams(sc_Float, sc_Float, 0.f);
					}
					break;
				case LAYERTAG_SCALEFUNC:
					SC_GetString();
					switch (SC_MatchString(CycleTags))
					{
					case CYCLE_Sin:
						sink.scaleX.SetCycleType(CYCLE_Sin);
						break;
					case CYCLE_Cos:
						sink.scaleX.SetCycleType(CYCLE_Cos);
						break;
					case CYCLE_SawTooth:
						sink.scaleX.SetCycleType(CYCLE_SawTooth);
						break;
					case CYCLE_Square:
						sink.scaleX.SetCycleType(CYCLE_Square);
						break;
					case CYCLE_Linear:
					default:
						sink.scaleX.SetCycleType(CYCLE_Linear);
						break;
					}
					SC_GetString();
					switch (SC_MatchString(CycleTags))
					{
					case CYCLE_Sin:
						sink.scaleY.SetCycleType(CYCLE_Sin);
						break;
					case CYCLE_Cos:
						sink.scaleY.SetCycleType(CYCLE_Cos);
						break;
					case CYCLE_SawTooth:
						sink.scaleY.SetCycleType(CYCLE_SawTooth);
						break;
					case CYCLE_Square:
						sink.scaleY.SetCycleType(CYCLE_Square);
						break;
					case CYCLE_Linear:
					default:
						sink.scaleY.SetCycleType(CYCLE_Linear);
						break;
					}
					break;
				case LAYERTAG_TEXGEN:
					SC_GetString();
					if (SC_Compare("sphere"))
					{
						sink.texgen = SHADER_TexGen_Sphere;
					}
					else
					{
						sink.texgen = SHADER_TexGen_None;
					}
					break;
				case LAYERTAG_VECTOR:
					SC_GetString();
					if (SC_Compare("cycle"))
					{
						sink.vectorX.ShouldCycle(true);
						sink.vectorY.ShouldCycle(true);

						SC_GetFloat();
						r1 = sc_Float;
						SC_GetFloat();
						g1 = sc_Float;
						SC_GetFloat();
						r2 = sc_Float;
						SC_GetFloat();
						g2 = sc_Float;
						SC_GetFloat();
						cycle = sc_Float;

						sink.vectorX.SetParams(r1, r2, cycle);
						sink.vectorY.SetParams(g1, g2, cycle);
					}
					else
					{
						start = static_cast<float>(atof(sc_String));
						sink.vectorX.SetParams(start, start, 0.f);
						SC_GetFloat();
						start = sc_Float;
						sink.vectorY.SetParams(start, start, 0.f);
					}
					break;
				case LAYERTAG_VECTORFUNC:
					SC_GetString();
					switch (SC_MatchString(CycleTags))
					{
					case CYCLE_Sin:
						sink.vectorX.SetCycleType(CYCLE_Sin);
						break;
					case CYCLE_Cos:
						sink.vectorX.SetCycleType(CYCLE_Cos);
						break;
					case CYCLE_SawTooth:
						sink.vectorX.SetCycleType(CYCLE_SawTooth);
						break;
					case CYCLE_Square:
						sink.vectorX.SetCycleType(CYCLE_Square);
						break;
					case CYCLE_Linear:
					default:
						sink.vectorX.SetCycleType(CYCLE_Linear);
						break;
					}
					SC_GetString();
					switch (SC_MatchString(CycleTags))
					{
					case CYCLE_Sin:
						sink.vectorY.SetCycleType(CYCLE_Sin);
						break;
					case CYCLE_Cos:
						sink.vectorY.SetCycleType(CYCLE_Cos);
						break;
					case CYCLE_SawTooth:
						sink.vectorY.SetCycleType(CYCLE_SawTooth);
						break;
					case CYCLE_Square:
						sink.vectorY.SetCycleType(CYCLE_Square);
						break;
					case CYCLE_Linear:
					default:
						sink.vectorY.SetCycleType(CYCLE_Linear);
						break;
					}
					break;
				case LAYERTAG_WARP:
					SC_GetString();
					sink.warp = SC_Compare("true") == TRUE;
					break;
				default:
					break;
				}
			}
			else
			{
				GL_CheckShaderDepth();
			}
		}

		shader->layers.Push(new FShaderLayer(sink));

		return true;
	}

	return false;
}


static const char *ShaderTags[]=
{
	"compatibility",
		"detailTex",
		"layer",
		NULL
};

enum {
	SHADERTAG_COMPATIBILITY,
	SHADERTAG_DETAILTEX,
	SHADERTAG_LAYER
};


bool GL_ParseShader()
{
	unsigned int i;
	int temp, type;
	FSWShader sink;
	TArray<FTexture *> names;
	string name;
	int texno;

	ShaderDepth = 0;

	if (SC_GetString())
	{
		do
		{
			int texno = TexMan.CheckForTexture(sc_String, FTexture::TEX_Wall, FTextureManager::TEXMAN_Overridable|FTextureManager::TEXMAN_TryAny);
			if (texno==-1)
			{
				Printf("Texture %s not found\n", sc_String);
				return false;
			}
			FTexture * tex = TexMan[texno];

			name = sc_String;
			names.Push(tex);
			SC_GetString();
		} 
		while (strstr(sc_String, "{") == NULL);

		GL_CheckShaderDepth();

		while (ShaderDepth > 0)
		{
			if (SC_GetString() == false)
			{
				ShaderDepth = 0;
				Printf("Shader parse error: %s\n", name.GetChars());
				return false;
			}

			if ((type = SC_MatchString(ShaderTags)) != -1)
			{
				switch (type)
				{
				case SHADERTAG_COMPATIBILITY:
					SC_GetString();
					break;
				case SHADERTAG_DETAILTEX:
					SC_GetString();
					texno = TexMan.CheckForTexture(sc_String, FTexture::TEX_Wall, FTextureManager::TEXMAN_Overridable|FTextureManager::TEXMAN_TryAny);
					if (texno==-1)
					{
						Printf("Texture %s not found\n", sc_String);
						sink.detailTex = NULL;
					}
					else
					{
						sink.detailTex = TexMan[texno];
					}
					break;
				case SHADERTAG_LAYER:
					GL_ParseLayer(&sink);
					break;
				default:
					break;
				}
			}
			else
			{
				GL_CheckShaderDepth();
			}
		}

		for (i = 0; i < names.Size(); i++) if (names[i])
		{
			sink.tex = names[i];
			temp = GL_ShaderIndexForShader(&sink);
			if (temp != -1)
			{
				// overwrite existing shader
				delete Shaders[i];
				Shaders[i] = new FSWShader(sink);
			}
			else
			{
				// add new shader
				Shaders.Push(new FSWShader(sink));
			}
		}
		names.Clear();

		return true;
	}

	return false;
}


void GL_ReleaseShaders()
{
	unsigned int i;

	for (i = 0; i < Shaders.Size(); i++)
	{
		delete Shaders[i];
	}
	Shaders.Clear();
}


void GL_UpdateShader(FSWShader *shader, unsigned int frameStartMS)
{
	unsigned int i;
	FShaderLayer *layer;
	float diff = (frameStartMS - shader->lastUpdate) / 1000.f;

	if (shader->lastUpdate != 0 && !paused && !bglobal.freeze)
	{
		for (i = 0; i < shader->layers.Size(); i++)
		{
			layer = shader->layers[i];
			layer->Update(diff);
			if (layer->layerMask) layer->layerMask->Update(diff);
		}
	}

	shader->lastUpdate = frameStartMS;
}


void GL_UpdateShaders()
{
	unsigned int i;
	unsigned int ms = I_MSTime();

	for (i = 0; i < Shaders.Size(); i++)
	{
		GL_UpdateShader(Shaders[i], ms);
	}
}


void GL_FakeUpdateShaders()
{
	unsigned int i;
	unsigned int ms = I_MSTime();

	for (i = 0; i < Shaders.Size(); i++)
	{
		Shaders[i]->lastUpdate = ms;
	}
}


void GL_DrawShader(FSWShader *shader, int x, int y, int width, int height)
{
	FShaderLayer *layer, *layerMask;

	for (unsigned int i = 0; i < shader->layers.Size(); ++i)
	{
		layer = shader->layers[i];
		layerMask = layer->layerMask;

		if (i == 0)
		{
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		}
		else
		{
			glBlendFunc(layer->blendFuncSrc, layer->blendFuncDst);
		}

		glColor4f(layer->r, layer->g, layer->b, layer->alpha);

		switch (layer->texgen)
		{
		case SHADER_TexGen_Sphere:
			glEnable(GL_TEXTURE_GEN_S);
			glEnable(GL_TEXTURE_GEN_T);
			break;
		default:
			glDisable(GL_TEXTURE_GEN_S);
			glDisable(GL_TEXTURE_GEN_T);
			break;
		}

		FGLTexture::ValidateTexture(layer->tex)->Bind(CM_DEFAULT);
		/*
		if (layerMask)
		{
			glBegin(GL_TRIANGLE_STRIP);
			glMultiTexCoord2fARB(GL_TEXTURE0_ARB, 0.f, 0.f);
			glVertex2i(x, y);
			glMultiTexCoord2fARB(GL_TEXTURE0_ARB, 0.f, 1.f);
			glVertex2i(x, y - height);
			glMultiTexCoord2fARB(GL_TEXTURE0_ARB, 1.f, 0.f);
			glVertex2i(x + width, y);
			glMultiTexCoord2fARB(GL_TEXTURE0_ARB, 1.f, 1.f);
			glVertex2i(x + width, y - height);
			glEnd();
		}
		else
		*/
		{
			glBegin(GL_TRIANGLE_STRIP);
			glTexCoord2f(0.f, 0.f);
			glVertex2i(x, y);
			glTexCoord2f(0.f, 1.f);
			glVertex2i(x, y - height);
			glTexCoord2f(1.f, 0.f);
			glVertex2i(x + width, y);
			glTexCoord2f(1.f, 1.f);
			glVertex2i(x + width, y - height);
			glEnd();
		}
	}

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_TEXTURE_GEN_S);
	glDisable(GL_TEXTURE_GEN_T);
}


void GL_DrawShaders()
{
	int curX, curY, maxY;
	FSWShader *shader;
	FTexture *tex;

	curX = 2;
	curY = screen->GetHeight() - 2;
	maxY = 0;

	glPushMatrix();
	glLoadIdentity();
	glColor3f(1.f, 1.f, 1.f);

	for (unsigned int i = 0; i < Shaders.Size(); i++)
	{
		shader = Shaders[i];
		tex = shader->tex;

		if (curX + tex->GetWidth() > screen->GetWidth())
		{
			curX = 2;
			curY -= maxY + 2;
			maxY = 0;
		}

		GL_DrawShader(shader, curX, curY, tex->GetWidth(), tex->GetHeight());

		maxY = MAX<int>(maxY, tex->GetHeight());
		curX += tex->GetWidth() + 2;
	}

	glPopMatrix();
}


CCMD(gl_list_shaders)
{
	unsigned int i;

	Printf(TEXTCOLOR_YELLOW "%d Shaders.\n", Shaders.Size());
	for (i = 0; i < Shaders.Size(); i++)
	{
		Printf(" %s\n", Shaders[i]->tex->Name);
	}
}
#endif
