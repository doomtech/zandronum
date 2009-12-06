/*
** gl_dynlight.cpp
** Light definitions for actors.
**
**---------------------------------------------------------------------------
** Copyright 2003 Timothy Stump
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
#include "c_cvars.h"
#include "c_dispatch.h"
#include "m_random.h"
#include "sc_man.h"
#include "templates.h"
#include "w_wad.h"
#include "gi.h"
#include "r_state.h"
#include "stats.h"
#include "zstring.h"
#include "d_dehacked.h"

#include "gl/gl_functions.h"
#include "gl/gl_lights.h"
#include "gl/gl_texture.h"

EXTERN_CVAR (Float, gl_lights_intensity);
EXTERN_CVAR (Float, gl_lights_size);
int ScriptDepth;
void gl_ParseSkybox(FScanner &sc);
void gl_InitGlow(FScanner &sc);
void gl_ParseBrightmap(FScanner &sc, int);

//==========================================================================
//
// Dehacked aliasing
//
//==========================================================================

inline const PClass * GetRealType(const PClass * ti)
{
	FActorInfo *rep = ti->ActorInfo->GetReplacement();
	if (rep != ti->ActorInfo && rep != NULL && rep->Class->IsDescendantOf(RUNTIME_CLASS(ADehackedPickup)))
	{
		return rep->Class;
	}
	return ti;
}



//==========================================================================
//
// Light associations
//
//==========================================================================
class FLightAssociation
{
public:
   FLightAssociation();
   FLightAssociation(const char *actorName, const char *frameName, const char *lightName);
   ~FLightAssociation();
   const char *ActorName() { return m_ActorName; }
   const char *FrameName() { return m_FrameName; }
   const char *Light() { return m_AssocLight; }
   void ReplaceActorName(const char *newName);
   void ReplaceLightName(const char *newName);
protected:
   FString m_ActorName, m_FrameName, m_AssocLight;
};

TArray<FLightAssociation *> LightAssociations;


FLightAssociation::FLightAssociation()
{
}


FLightAssociation::FLightAssociation(const char *actorName, const char *frameName, const char *lightName)
{
	m_ActorName.Format("%s", actorName);
	m_FrameName.Format("%s", frameName);
	m_AssocLight.Format("%s", lightName);
}


FLightAssociation::~FLightAssociation()
{
}


void FLightAssociation::ReplaceActorName(const char *newName)
{
	m_ActorName.Format("%s", newName);
}


void FLightAssociation::ReplaceLightName(const char *newName)
{
	m_AssocLight.Format("%s", newName);
}



//==========================================================================
//
// Light definitions
//
//==========================================================================
class FLightDefaults
{
public:
   FLightDefaults(const char *name, ELightType type);
   ~FLightDefaults();

   void ApplyProperties(ADynamicLight * light) const;
   const char *GetName() const { return m_Name; }
   void SetAngle(angle_t angle) { m_Angle = angle; }
   void SetArg(int arg, byte val) { m_Args[arg] = val; }
   byte GetArg(int arg) { return m_Args[arg]; }
   void SetOffset(float* ft) { m_X = FLOAT2FIXED(ft[0]); m_Y = FLOAT2FIXED(ft[1]); m_Z = FLOAT2FIXED(ft[2]); }
   void SetSubtractive(bool subtract) { m_subtractive = subtract; }
   void SetAdditive(bool add) { m_additive = add; }
   void SetDontLightSelf(bool add) { m_dontlightself = add; }
   void SetHalo(bool halo) { m_halo = halo; }
protected:
   FString m_Name;
   unsigned char m_Args[5];
   angle_t m_Angle;
   fixed_t m_X, m_Y, m_Z;
   ELightType m_type;
   bool m_subtractive, m_additive, m_halo, m_dontlightself;
};

TArray<FLightDefaults *> LightDefaults;

FLightDefaults::FLightDefaults(const char *name, ELightType type)
{
	m_Name = name;
	m_type = type;

	m_X = m_Y = m_Z = 0;
	memset(m_Args, 0, 5);

	m_subtractive = false;
	m_additive = false;
	m_halo = false;
	m_dontlightself = false;
}

FLightDefaults::~FLightDefaults()
{
}

void FLightDefaults::ApplyProperties(ADynamicLight * light) const
{
	light->lighttype = m_type;
	light->angle = m_Angle;
	light->SetOffset(m_X, m_Y, m_Z);
	light->halo = m_halo;
	for (int a = 0; a < 3; a++) light->args[a] = clamp<int>((int)(m_Args[a] * gl_lights_intensity), 0, 255);
	light->m_intensity[0] = m_Args[LIGHT_INTENSITY] * gl_lights_size;
	light->m_intensity[1] = m_Args[LIGHT_SECONDARY_INTENSITY] * gl_lights_size;
	light->flags4&=~(MF4_ADDITIVE|MF4_SUBTRACTIVE|MF4_DONTLIGHTSELF);
	if (m_subtractive) light->flags4|=MF4_SUBTRACTIVE;
	if (m_additive) light->flags4|=MF4_ADDITIVE;
	if (m_dontlightself) light->flags4|=MF4_DONTLIGHTSELF;
}


//==========================================================================
//
// light definition file parser
//
//==========================================================================


static const char *LightTags[]=
{
   "color",
   "size",
   "secondarySize",
   "offset",
   "chance",
   "interval",
   "scale",
   "frame",
   "light",
   "{",
   "}",
   "subtractive",
   "additive",
   "halo",
   "dontlightself",
   NULL
};


enum {
   LIGHTTAG_COLOR,
   LIGHTTAG_SIZE,
   LIGHTTAG_SECSIZE,
   LIGHTTAG_OFFSET,
   LIGHTTAG_CHANCE,
   LIGHTTAG_INTERVAL,
   LIGHTTAG_SCALE,
   LIGHTTAG_FRAME,
   LIGHTTAG_LIGHT,
   LIGHTTAG_OPENBRACE,
   LIGHTTAG_CLOSEBRACE,
   LIGHTTAG_SUBTRACTIVE,
   LIGHTTAG_ADDITIVE,
   LIGHTTAG_HALO,
   LIGHTTAG_DONTLIGHTSELF,
};


extern int ScriptDepth;


inline float gl_ParseFloat(FScanner &sc)
{
   sc.GetFloat();

   return sc.Float;
}


inline int gl_ParseInt(FScanner &sc)
{
   sc.GetNumber();

   return sc.Number;
}


inline char *gl_ParseString(FScanner &sc)
{
   sc.GetString();

   return sc.String;
}


void gl_ParseTriple(FScanner &sc, float floatVal[3])
{
   for (int i = 0; i < 3; i++)
   {
      floatVal[i] = gl_ParseFloat(sc);
   }
}


void gl_AddLightDefaults(FLightDefaults *defaults)
{
   FLightDefaults *temp;
   unsigned int i;

   // remove duplicates
   for (i = 0; i < LightDefaults.Size(); i++)
   {
      temp = LightDefaults[i];
      if (strcmp(temp->GetName(), defaults->GetName()) == 0)
      {
         delete temp;
         LightDefaults.Delete(i);
         break;
      }
   }

   LightDefaults.Push(defaults);
}


// parse thing 9800
void gl_ParsePointLight(FScanner &sc)
{
	int type;
	float floatTriple[3];
	int intVal;
	FString name;
	FLightDefaults *defaults;

	// get name
	sc.GetString();
	name = sc.String;

	// check for opening brace
	sc.GetString();
	if (sc.Compare("{"))
	{
		defaults = new FLightDefaults(name.GetChars(), PointLight);
		ScriptDepth++;
		while (ScriptDepth)
		{
			sc.GetString();
			type = sc.MatchString(LightTags);
			switch (type)
			{
			case LIGHTTAG_OPENBRACE:
				ScriptDepth++;
				break;
			case LIGHTTAG_CLOSEBRACE:
				ScriptDepth--;
				break;
			case LIGHTTAG_COLOR:
				gl_ParseTriple(sc, floatTriple);
				defaults->SetArg(LIGHT_RED, clamp<int>((int)(floatTriple[0] * 255), 0, 255));
				defaults->SetArg(LIGHT_GREEN, clamp<int>((int)(floatTriple[1] * 255), 0, 255));
				defaults->SetArg(LIGHT_BLUE, clamp<int>((int)(floatTriple[2] * 255), 0, 255));
				break;
			case LIGHTTAG_OFFSET:
				gl_ParseTriple(sc, floatTriple);
				defaults->SetOffset(floatTriple);
				break;
			case LIGHTTAG_SIZE:
				intVal = clamp<int>(gl_ParseInt(sc), 0, 255);
				defaults->SetArg(LIGHT_INTENSITY, intVal);
				break;
			case LIGHTTAG_SUBTRACTIVE:
				defaults->SetSubtractive(gl_ParseInt(sc) != 0);
				break;
			case LIGHTTAG_ADDITIVE:
				defaults->SetAdditive(gl_ParseInt(sc) != 0);
				break;
			case LIGHTTAG_HALO:
				defaults->SetHalo(gl_ParseInt(sc) != 0);
				break;
			case LIGHTTAG_DONTLIGHTSELF:
				defaults->SetDontLightSelf(gl_ParseInt(sc) != 0);
				break;
			default:
				sc.ScriptError("Unknown tag: %s\n", sc.String);
			}
		}
		gl_AddLightDefaults(defaults);
	}
	else
	{
		sc.ScriptError("Expected '{'.\n");
	}
}


void gl_ParsePulseLight(FScanner &sc)
{
	int type;
	float floatVal, floatTriple[3];
	int intVal;
	FString name;
	FLightDefaults *defaults;

	// get name
	sc.GetString();
	name = sc.String;

	// check for opening brace
	sc.GetString();
	if (sc.Compare("{"))
	{
		defaults = new FLightDefaults(name.GetChars(), PulseLight);
		ScriptDepth++;
		while (ScriptDepth)
		{
			sc.GetString();
			type = sc.MatchString(LightTags);
			switch (type)
			{
			case LIGHTTAG_OPENBRACE:
				ScriptDepth++;
				break;
			case LIGHTTAG_CLOSEBRACE:
				ScriptDepth--;
				break;
			case LIGHTTAG_COLOR:
				gl_ParseTriple(sc, floatTriple);
				defaults->SetArg(LIGHT_RED, clamp<int>((int)(floatTriple[0] * 255), 0, 255));
				defaults->SetArg(LIGHT_GREEN, clamp<int>((int)(floatTriple[1] * 255), 0, 255));
				defaults->SetArg(LIGHT_BLUE, clamp<int>((int)(floatTriple[2] * 255), 0, 255));
				break;
			case LIGHTTAG_OFFSET:
				gl_ParseTriple(sc, floatTriple);
				defaults->SetOffset(floatTriple);
				break;
			case LIGHTTAG_SIZE:
				intVal = clamp<int>(gl_ParseInt(sc), 0, 255);
				defaults->SetArg(LIGHT_INTENSITY, intVal);
				break;
			case LIGHTTAG_SECSIZE:
				intVal = clamp<int>(gl_ParseInt(sc), 0, 255);
				defaults->SetArg(LIGHT_SECONDARY_INTENSITY, intVal);
				break;
			case LIGHTTAG_INTERVAL:
				floatVal = gl_ParseFloat(sc);
				defaults->SetAngle(FLOAT_TO_ANGLE(floatVal * TICRATE));
				break;
			case LIGHTTAG_SUBTRACTIVE:
				defaults->SetSubtractive(gl_ParseInt(sc) != 0);
				break;
			case LIGHTTAG_HALO:
				defaults->SetHalo(gl_ParseInt(sc) != 0);
				break;
			case LIGHTTAG_DONTLIGHTSELF:
				defaults->SetDontLightSelf(gl_ParseInt(sc) != 0);
				break;
			default:
				sc.ScriptError("Unknown tag: %s\n", sc.String);
			}
		}
		gl_AddLightDefaults(defaults);
	}
	else
	{
		sc.ScriptError("Expected '{'.\n");
	}
}


void gl_ParseFlickerLight(FScanner &sc)
{
	int type;
	float floatVal, floatTriple[3];
	int intVal;
	FString name;
	FLightDefaults *defaults;

	// get name
	sc.GetString();
	name = sc.String;

	// check for opening brace
	sc.GetString();
	if (sc.Compare("{"))
	{
		defaults = new FLightDefaults(name.GetChars(), FlickerLight);
		ScriptDepth++;
		while (ScriptDepth)
		{
			sc.GetString();
			type = sc.MatchString(LightTags);
			switch (type)
			{
			case LIGHTTAG_OPENBRACE:
				ScriptDepth++;
				break;
			case LIGHTTAG_CLOSEBRACE:
				ScriptDepth--;
				break;
			case LIGHTTAG_COLOR:
				gl_ParseTriple(sc, floatTriple);
				defaults->SetArg(LIGHT_RED, clamp<int>((int)(floatTriple[0] * 255), 0, 255));
				defaults->SetArg(LIGHT_GREEN, clamp<int>((int)(floatTriple[1] * 255), 0, 255));
				defaults->SetArg(LIGHT_BLUE, clamp<int>((int)(floatTriple[2] * 255), 0, 255));
				break;
			case LIGHTTAG_OFFSET:
				gl_ParseTriple(sc, floatTriple);
				defaults->SetOffset(floatTriple);
				break;
			case LIGHTTAG_SIZE:
				intVal = clamp<int>(gl_ParseInt(sc), 0, 255);
				defaults->SetArg(LIGHT_INTENSITY, intVal);
				break;
			case LIGHTTAG_SECSIZE:
				intVal = clamp<int>(gl_ParseInt(sc), 0, 255);
				defaults->SetArg(LIGHT_SECONDARY_INTENSITY, intVal);
				break;
			case LIGHTTAG_CHANCE:
				floatVal = gl_ParseFloat(sc);
				defaults->SetAngle((angle_t)(floatVal * ANGLE_MAX));
				break;
			case LIGHTTAG_SUBTRACTIVE:
				defaults->SetSubtractive(gl_ParseInt(sc) != 0);
				break;
			case LIGHTTAG_HALO:
				defaults->SetHalo(gl_ParseInt(sc) != 0);
				break;
			case LIGHTTAG_DONTLIGHTSELF:
				defaults->SetDontLightSelf(gl_ParseInt(sc) != 0);
				break;
			default:
				sc.ScriptError("Unknown tag: %s\n", sc.String);
			}
		}
		gl_AddLightDefaults(defaults);
	}
	else
	{
		sc.ScriptError("Expected '{'.\n");
	}
}


void gl_ParseFlickerLight2(FScanner &sc)
{
	int type;
	float floatVal, floatTriple[3];
	int intVal;
	FString name;
	FLightDefaults *defaults;

	// get name
	sc.GetString();
	name = sc.String;

	// check for opening brace
	sc.GetString();
	if (sc.Compare("{"))
	{
		defaults = new FLightDefaults(name.GetChars(), RandomFlickerLight);
		ScriptDepth++;
		while (ScriptDepth)
		{
			sc.GetString();
			type = sc.MatchString(LightTags);
			switch (type)
			{
			case LIGHTTAG_OPENBRACE:
				ScriptDepth++;
				break;
			case LIGHTTAG_CLOSEBRACE:
				ScriptDepth--;
				break;
			case LIGHTTAG_COLOR:
				gl_ParseTriple(sc, floatTriple);
				defaults->SetArg(LIGHT_RED, clamp<int>((int)(floatTriple[0] * 255), 0, 255));
				defaults->SetArg(LIGHT_GREEN, clamp<int>((int)(floatTriple[1] * 255), 0, 255));
				defaults->SetArg(LIGHT_BLUE, clamp<int>((int)(floatTriple[2] * 255), 0, 255));
				break;
			case LIGHTTAG_OFFSET:
				gl_ParseTriple(sc, floatTriple);
				defaults->SetOffset(floatTriple);
				break;
			case LIGHTTAG_SIZE:
				intVal = clamp<int>(gl_ParseInt(sc), 0, 255);
				defaults->SetArg(LIGHT_INTENSITY, intVal);
				break;
			case LIGHTTAG_SECSIZE:
				intVal = clamp<int>(gl_ParseInt(sc), 0, 255);
				defaults->SetArg(LIGHT_SECONDARY_INTENSITY, intVal);
				break;
			case LIGHTTAG_INTERVAL:
				floatVal = gl_ParseFloat(sc);
				defaults->SetAngle((angle_t)(floatVal * ANGLE_MAX));
				break;
			case LIGHTTAG_SUBTRACTIVE:
				defaults->SetSubtractive(gl_ParseInt(sc) != 0);
				break;
			case LIGHTTAG_HALO:
				defaults->SetHalo(gl_ParseInt(sc) != 0);
				break;
			case LIGHTTAG_DONTLIGHTSELF:
				defaults->SetDontLightSelf(gl_ParseInt(sc) != 0);
				break;
			default:
				sc.ScriptError("Unknown tag: %s\n", sc.String);
			}
		}
		if (defaults->GetArg(LIGHT_SECONDARY_INTENSITY) < defaults->GetArg(LIGHT_INTENSITY))
		{
			byte v = defaults->GetArg(LIGHT_SECONDARY_INTENSITY);
			defaults->SetArg(LIGHT_SECONDARY_INTENSITY, defaults->GetArg(LIGHT_INTENSITY));
			defaults->SetArg(LIGHT_INTENSITY, v);
		}
		gl_AddLightDefaults(defaults);
	}
	else
	{
		sc.ScriptError("Expected '{'.\n");
	}
}


void gl_ParseSectorLight(FScanner &sc)
{
	int type;
	float floatVal;
	float floatTriple[3];
	FString name;
	FLightDefaults *defaults;

	// get name
	sc.GetString();
	name = sc.String;

	// check for opening brace
	sc.GetString();
	if (sc.Compare("{"))
	{
		defaults = new FLightDefaults(name.GetChars(), SectorLight);
		ScriptDepth++;
		while (ScriptDepth)
		{
			sc.GetString();
			type = sc.MatchString(LightTags);
			switch (type)
			{
			case LIGHTTAG_OPENBRACE:
				ScriptDepth++;
				break;
			case LIGHTTAG_CLOSEBRACE:
				ScriptDepth--;
				break;
			case LIGHTTAG_COLOR:
				gl_ParseTriple(sc, floatTriple);
				defaults->SetArg(LIGHT_RED, clamp<int>((int)(floatTriple[0] * 255), 0, 255));
				defaults->SetArg(LIGHT_GREEN, clamp<int>((int)(floatTriple[1] * 255), 0, 255));
				defaults->SetArg(LIGHT_BLUE, clamp<int>((int)(floatTriple[2] * 255), 0, 255));
				break;
			case LIGHTTAG_OFFSET:
				gl_ParseTriple(sc, floatTriple);
				defaults->SetOffset(floatTriple);
				break;
			case LIGHTTAG_SCALE:
				floatVal = gl_ParseFloat(sc);
				defaults->SetArg(LIGHT_SCALE, (byte)(floatVal * 255));
				break;
			case LIGHTTAG_SUBTRACTIVE:
				defaults->SetSubtractive(gl_ParseInt(sc) != 0);
				break;
			case LIGHTTAG_HALO:
				defaults->SetHalo(gl_ParseInt(sc) != 0);
				break;
			case LIGHTTAG_DONTLIGHTSELF:
				defaults->SetDontLightSelf(gl_ParseInt(sc) != 0);
				break;
			default:
				sc.ScriptError("Unknown tag: %s\n", sc.String);
			}
		}
		gl_AddLightDefaults(defaults);
	}
	else
	{
		sc.ScriptError("Expected '{'.\n");
	}
}


void gl_AddLightAssociation(FLightAssociation *assoc)
{
	FLightAssociation *temp;
	unsigned int i;

	for (i = 0; i < LightAssociations.Size(); i++)
	{
		temp = LightAssociations[i];
		if (stricmp(temp->ActorName(), assoc->ActorName()) == 0)
		{
			if (strcmp(temp->FrameName(), assoc->FrameName()) == 0)
			{
				temp->ReplaceLightName(assoc->Light());
				return;
			}
		}
	}

	LightAssociations.Push(assoc);
}


void gl_ParseFrame(FScanner &sc, FString name)
{
	int type, startDepth;
	FString frameName;

	// get name
	sc.GetString();
	if (strlen(sc.String) > 8)
	{
		sc.ScriptError("Name longer than 8 characters: %s\n", sc.String);
	}
	frameName = sc.String;

	startDepth = ScriptDepth;

	// check for opening brace
	sc.GetString();
	if (sc.Compare("{"))
	{
		ScriptDepth++;
		while (ScriptDepth > startDepth)
		{
			sc.GetString();
			type = sc.MatchString(LightTags);
			switch (type)
			{
			case LIGHTTAG_OPENBRACE:
				ScriptDepth++;
				break;
			case LIGHTTAG_CLOSEBRACE:
				ScriptDepth--;
				break;
			case LIGHTTAG_LIGHT:
				gl_ParseString(sc);
				// [BB] The server just ignores all light associations.
				if ( NETWORK_GetState( ) != NETSTATE_SERVER )
					gl_AddLightAssociation(new FLightAssociation(name.GetChars(), frameName.GetChars(), sc.String));
				break;
			default:
				sc.ScriptError("Unknown tag: %s\n", sc.String);
			}
		}
	}
	else
	{
		sc.ScriptError("Expected '{'.\n");
	}
}


void gl_ParseObject(FScanner &sc)
{
	int type;
	FString name;

	// get name
	sc.GetString();
	name = sc.String;

	// check for opening brace
	sc.GetString();
	if (sc.Compare("{"))
	{
		ScriptDepth++;
		while (ScriptDepth)
		{
			sc.GetString();
			type = sc.MatchString(LightTags);
			switch (type)
			{
			case LIGHTTAG_OPENBRACE:
				ScriptDepth++;
				break;
			case LIGHTTAG_CLOSEBRACE:
				ScriptDepth--;
				break;
			case LIGHTTAG_FRAME:
				gl_ParseFrame(sc, name);
				break;
			default:
				sc.ScriptError("Unknown tag: %s\n", sc.String);
			}
		}
	}
	else
	{
		sc.ScriptError("Expected '{'.\n");
	}
}


void gl_ReleaseLights()
{
   unsigned int i;

   for (i = 0; i < LightAssociations.Size(); i++)
   {
      delete LightAssociations[i];
   }

   for (i = 0; i < LightDefaults.Size(); i++)
   {
      delete LightDefaults[i];
   }

   LightAssociations.Clear();
   LightDefaults.Clear();
}


void gl_ReplaceLightAssociations(const char *oldName, const char *newName)
{
   unsigned int i;

   //Printf("%s -> %s\n", oldName, newName);

   for (i = 0; i < LightAssociations.Size(); i++)
   {
      if (strcmp(LightAssociations[i]->ActorName(), oldName) == 0)
      {
         LightAssociations[i]->ReplaceActorName(newName);
      }
   }
}


// these are the core types available in the *DEFS lump
static const char *CoreKeywords[]=
{
   "pointlight",
   "pulselight",
   "flickerlight",
   "flickerlight2",
   "sectorlight",
   "object",
   "clearlights",
   "shader",
   "clearshaders",
   "skybox",
   "glow",
   "brightmap",
   "disable_fullbright",
   "#include",
   NULL
};


enum
{
   LIGHT_POINT,
   LIGHT_PULSE,
   LIGHT_FLICKER,
   LIGHT_FLICKER2,
   LIGHT_SECTOR,
   LIGHT_OBJECT,
   LIGHT_CLEAR,
   TAG_SHADER,
   TAG_CLEARSHADERS,
   TAG_SKYBOX,
   TAG_GLOW,
   TAG_BRIGHTMAP,
   TAG_DISABLE_FB,
   TAG_INCLUDE,
};


//==========================================================================
//
// This is only here so any shader definition for ZDoomGL can be skipped
// There is no functionality for this stuff!
//
//==========================================================================
bool gl_ParseShader(FScanner &sc)
{
	int  ShaderDepth = 0;

	if (sc.GetString())
	{
		char *tmp;

		tmp = strstr(sc.String, "{");
		while (tmp)
		{
			ShaderDepth++;
			tmp++;
			tmp = strstr(tmp, "{");
		}

		tmp = strstr(sc.String, "}");
		while (tmp)
		{
			ShaderDepth--;
			tmp++;
			tmp = strstr(tmp, "}");
		}

		if (ShaderDepth == 0) return true;
	}
	return false;
}

//==========================================================================
//
// Light associations per actor class
//
// Turn this inefficient mess into something that can be used at run time.
//
//==========================================================================

class FInternalLightAssociation
{
public:
	FInternalLightAssociation(FLightAssociation * asso);
	int Sprite() const { return m_sprite; }
	int Frame() const { return m_frame; }
	const FLightDefaults *Light() const { return m_AssocLight; }
protected:
	int m_sprite;
	int m_frame;
	FLightDefaults * m_AssocLight;
};


FInternalLightAssociation::FInternalLightAssociation(FLightAssociation * asso)
{

	m_AssocLight=NULL;
	for(unsigned int i=0;i<LightDefaults.Size();i++)
	{
		if (!strcmp(asso->Light(), LightDefaults[i]->GetName()))
		{
			m_AssocLight = LightDefaults[i];
			break;
		}
	}

	m_sprite=-1;
	m_frame = -1;
	for (unsigned i = 0; i < sprites.Size (); ++i)
	{
		if (strncmp (sprites[i].name, asso->FrameName(), 4) == 0)
		{
			m_sprite = (int)i;
			break;
		}
	}

	// Only handle lights for full frames. 
	// I won't bother with special lights for single rotations
	// because there is no decent use for them!
	if (strlen(asso->FrameName())==5 || asso->FrameName()[5]=='0')
	{
		m_frame = toupper(asso->FrameName()[4])-'A';
	}
}


inline TArray<FInternalLightAssociation *> * gl_GetActorLights(AActor * actor)
{
	return (TArray<FInternalLightAssociation *>*)actor->lightassociations;
}

void gl_InitializeActorLights()
{
	for(unsigned int i=0;i<LightAssociations.Size();i++)
	{
		const PClass * ti = PClass::FindClass(LightAssociations[i]->ActorName());
		if (ti)
		{
			ti = GetRealType(ti);
			AActor * defaults = GetDefaultByType(ti);
			if (defaults)
			{
				FInternalLightAssociation * iasso = new FInternalLightAssociation(LightAssociations[i]);

				if (!defaults->lightassociations)
				{
					defaults->lightassociations = new TArray<FInternalLightAssociation*>;
				}
				TArray<FInternalLightAssociation *> * lights = gl_GetActorLights(defaults);
				if (iasso->Light()==NULL)
				{
					// The definition was not valid.
					delete iasso;
				}
				else
				{
					lights->Push(iasso);
				}
			}
		}
	}
}


//==========================================================================
//
// per-state light adjustment
//
//==========================================================================

void gl_SetActorLights(AActor *actor)
{
	TArray<FInternalLightAssociation *> * l = gl_GetActorLights(actor);

	All.Clock();
	if (l && currentrenderer==1)
	{
		TArray<FInternalLightAssociation *> & LightAssociations=*l;
		ADynamicLight *lights, *tmpLight, *light;
		unsigned int i;
		unsigned int count;

		int sprite = actor->state->sprite;
		int frame = actor->state->GetFrame();

		lights = tmpLight = NULL;

		count=0;
		
		for (i = 0; i < LightAssociations.Size(); i++)
		{
			if (LightAssociations[i]->Sprite() == sprite &&
				(LightAssociations[i]->Frame()==frame || LightAssociations[i]->Frame()==-1))
			{
				// I'm skipping the single rotations because that really doesn't make sense!
				if (count < actor->dynamiclights.Size()) 
				{
					light = barrier_cast<ADynamicLight*>(actor->dynamiclights[count]);
					assert(light != NULL);
				}
				else
				{
					light = Spawn<ADynamicLight>(actor->x, actor->y, actor->z, NO_REPLACE);
					light->target = actor;
					light->owned = true;
					actor->dynamiclights.Push(light);
				}
				light->flags2&=~MF2_DORMANT;
				LightAssociations[i]->Light()->ApplyProperties(light);
				count++;
			}
		}
		for(;count<actor->dynamiclights.Size();count++)
		{
			actor->dynamiclights[count]->flags2|=MF2_DORMANT;
			memset(actor->dynamiclights[count]->args, 0, sizeof(actor->args));
		}
		// Store the current sprite in the first light to avoid 
		// redundant reassignments of the light properties.
		if (actor->dynamiclights.Size())
		{
			actor->dynamiclights[0]->special1=(int)actor->sprite;
			actor->dynamiclights[0]->special2=(int)actor->frame;
		}
	}
	All.Unclock();
}

//==========================================================================
//
// This is called before saving the game
//
//==========================================================================

void gl_DeleteAllAttachedLights()
{
	TThinkerIterator<AActor> it;
	AActor * a;
	ADynamicLight * l;

	while ((a=it.Next())) 
	{
		a->dynamiclights.Clear();
	}

	TThinkerIterator<ADynamicLight> it2;

	l=it2.Next();
	while (l) 
	{
		ADynamicLight * ll = it2.Next();
		if (l->owned) l->Destroy();
		l=ll;
	}


}

void gl_RecreateAllAttachedLights()
{
	TThinkerIterator<AActor> it;
	AActor * a;

	while ((a=it.Next())) 
	{
		gl_SetActorLights(a);
	}
}


//==========================================================================
// The actual light def parsing code is there.
// DoParseDefs is no longer called directly by ParseDefs, now it's called
// by LoadDynLightDefs, which wasn't simply integrated into ParseDefs
// because of the way the code needs to load two out of five lumps.
//==========================================================================
void gl_DoParseDefs(FScanner &sc, int workingLump)
{
	int recursion=0;
	int lump, type;

	// Get actor class name.
	while (true)
	{
		sc.SavePos();
		if (!sc.GetToken ())
		{
			return;
		}
		type = sc.MatchString(CoreKeywords);
		switch (type)
		{
		case TAG_INCLUDE:
			{
				sc.MustGetString();
				// This is not using sc.Open because it can print a more useful error message when done here
				lump = Wads.CheckNumForFullName(sc.String);
				// Try a normal WAD name lookup only if it's a proper name without path
				// separator and not longer than 8 characters.
				if (lump==-1 && strlen(sc.String) <= 8 && !strchr(sc.String, '/'))
					lump = Wads.CheckNumForName(sc.String);
				if (lump==-1)
					sc.ScriptError("Lump '%s' not found", sc.String);

				FScanner newscanner(lump);
				gl_DoParseDefs(newscanner, lump);
				break;
			}
		case LIGHT_POINT:
			gl_ParsePointLight(sc);
			break;
		case LIGHT_PULSE:
			gl_ParsePulseLight(sc);
			break;
		case LIGHT_FLICKER:
			gl_ParseFlickerLight(sc);
			break;
		case LIGHT_FLICKER2:
			gl_ParseFlickerLight2(sc);
			break;
		case LIGHT_SECTOR:
			gl_ParseSectorLight(sc);
			break;
		case LIGHT_OBJECT:
			gl_ParseObject(sc);
			break;
		case LIGHT_CLEAR:
			gl_ReleaseLights();
			break;
		case TAG_SHADER:
			gl_ParseShader(sc);
			break;
		case TAG_CLEARSHADERS:
			break;
		case TAG_SKYBOX:
			gl_ParseSkybox(sc);
			break;
		case TAG_GLOW:
			gl_InitGlow(sc);
			break;
		case TAG_BRIGHTMAP:
			gl_ParseBrightmap(sc, workingLump);
			break;
		case TAG_DISABLE_FB:
			{
				/* not implemented.
				sc.MustGetString();
				const PClass *cls = PClass::FindClass(sc.String);
				if (cls) GetDefaultByType(cls)->renderflags |= RF_NEVERFULLBRIGHT;
				*/
			}
			break;
		default:
			sc.ScriptError("Error parsing defs.  Unknown tag: %s.\n", sc.String);
			break;
		}
	}
}

void gl_LoadDynLightDefs(const char * defsLump)
{
	int workingLump, lastLump;

	lastLump = 0;
	while ((workingLump = Wads.FindLump(defsLump, &lastLump)) != -1)
	{
		FScanner sc(workingLump);
		gl_DoParseDefs(sc, workingLump);
	}
}


void gl_ParseDefs()
{
	const char *defsLump;

	atterm( gl_ReleaseLights ); 
	switch (gameinfo.gametype)
	{
	case GAME_Heretic:
		defsLump = "HTICDEFS";
		break;
	case GAME_Hexen:
		defsLump = "HEXNDEFS";
		break;
	case GAME_Strife:
		defsLump = "STRFDEFS";
		break;
	default:
		defsLump = "DOOMDEFS";
		break;
	}
	gl_LoadDynLightDefs(defsLump);
	gl_LoadDynLightDefs("GLDEFS");
	gl_InitializeActorLights();
}

