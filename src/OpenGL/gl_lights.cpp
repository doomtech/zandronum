
#include <windows.h>
#include <gl/gl.h>
#include <gl/glu.h>
#include <string>

#define USE_WINDOWS_DWORD
#include "a_doomglobal.h"
#include "c_cvars.h"
#include "c_dispatch.h"
#include "m_random.h"
#include "gl_lights.h"
#include "gl_main.h"
#include "sc_man.h"
#include "templates.h"
#include "w_wad.h"
#include "gi.h"

using namespace std;


// all lights use this format:
// arg1: red
// arg2: green
// arg3: blue
// arg4: intensity
// arg5: radius or target if ASpotLight

// ASpotTarget is different, though, since it contains more information about the spotlight shape
// arg1: cone width (angle, 0..255)
// arg2: 
// arg3:
// arg4:
// arg5:


CVAR (Bool, gl_lights_debug, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);
CVAR (Bool, gl_lights, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);
CVAR (Bool, gl_lights_multiply, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);
CVAR (Float, gl_lights_intensity, 1.0f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);
CVAR (Float, gl_lights_size, 1.0f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);
EXTERN_CVAR (Bool, gl_depthfog);
EXTERN_CVAR (Bool, gl_texture);
EXTERN_CVAR (Bool, gl_wireframe);

extern player_t *Player;


//class SubsectorList
//{
//public:
//   subsector_t *Subsec;
//   SubsectorList *Next;
//};


class FLightAlias
{
public:
   FLightAlias();
   FLightAlias(const char *fakeName, const char *realName);
   ~FLightAlias();
   const char *AliasName() { return m_FakeName; }
   const char *RealName() { return m_RealName; }
protected:
   char *m_FakeName, *m_RealName;
};


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
   char *m_ActorName, *m_FrameName, *m_AssocLight;
};


class FLightDefaults
{
public:
   FLightDefaults(const char *name, const char *className);
   ~FLightDefaults();
   ADynamicLight *CreateLight(AActor *target, FState *targetState);
   const char *GetName() { return m_Name; }
   void SetAngle(angle_t angle) { m_Angle = angle; }
   void SetArg(int arg, byte val) { m_Args[arg] = val; }
   void SetOffset(fixed_t x, fixed_t y, fixed_t z) { m_X = x; m_Y = y; m_Z = z; }
   void SetSubtractive(bool subtract) { m_subtractive = subtract; }
   void SetHalo(bool halo) { m_halo = halo; }
protected:
   char *m_Name, *m_Class;
   unsigned char m_Args[5];
   angle_t m_Angle;
   fixed_t m_X, m_Y, m_Z;
   bool m_subtractive, m_halo;
};


TArray<FLightAssociation *> LightAssociations;
TArray<FLightDefaults *> LightDefaults;
TArray<FLightAlias *> LightAliases; // pretty much just for dehacked objects
TArray<TArray<ADynamicLight *> *> SubsecLights;
TArray<ADynamicLight *> HaloLights;
FRandom randLight;


FLightAlias::FLightAlias()
{
   m_FakeName = NULL;
   m_RealName = NULL;
}


FLightAlias::FLightAlias(const char *fakeName, const char *realName)
{
   m_FakeName = new char[strlen(fakeName) + 1];
   memcpy(m_FakeName, fakeName, strlen(fakeName) + 1);

   m_RealName = new char[strlen(realName)];
   memcpy(m_RealName, realName + 1, strlen(realName));
}


FLightAlias::~FLightAlias()
{
   if (m_FakeName) delete [] m_FakeName;
   if (m_RealName) delete [] m_RealName;
}


FLightAssociation::FLightAssociation()
{
   m_ActorName = NULL;
   m_FrameName = NULL;
   m_AssocLight = NULL;
}


FLightAssociation::FLightAssociation(const char *actorName, const char *frameName, const char *lightName)
{
   m_ActorName = new char[strlen(actorName) + 1];
   m_FrameName = new char[strlen(frameName) + 1];
   m_AssocLight = new char[strlen(lightName) + 1];

   sprintf(m_ActorName, "%s", actorName);
   sprintf(m_FrameName, "%s", frameName);
   sprintf(m_AssocLight, "%s", lightName);
}


FLightAssociation::~FLightAssociation()
{
   if (m_ActorName) delete [] m_ActorName;
   if (m_FrameName) delete [] m_FrameName;
   if (m_AssocLight) delete [] m_AssocLight;
}


void FLightAssociation::ReplaceActorName(const char *newName)
{
   if (m_ActorName) delete [] m_ActorName;

   m_ActorName = new char[strlen(newName) + 1];
   sprintf(m_ActorName, "%s", newName);
}


void FLightAssociation::ReplaceLightName(const char *newName)
{
   if (m_AssocLight) delete [] m_AssocLight;

   m_AssocLight = new char[strlen(newName) + 1];
   sprintf(m_AssocLight, "%s", newName);
}


FLightDefaults::FLightDefaults(const char *name, const char *className)
{
   m_Name = new char[strlen(name) + 1];
   sprintf(m_Name, "%s", name);

   m_Class = new char[strlen(className) + 1];
   sprintf(m_Class, "%s", className);

   m_X = m_Y = m_Z = 0;
   memset(m_Args, 0, 5);

   m_subtractive = false;
   m_halo = false;
}


FLightDefaults::~FLightDefaults()
{
   if (m_Name) delete [] m_Name;
   if (m_Class) delete [] m_Class;
}


ADynamicLight *FLightDefaults::CreateLight(AActor *target, FState *targetState)
{
   const PClass *type;
   char typeName[128];
   ADynamicLight *light = NULL;

   if (m_subtractive)
   {
      sprintf(typeName, "%sSubtractive", m_Class);
   }
   else
   {
      sprintf(typeName, "%s", m_Class);
   }

   type = PClass::FindClass(typeName);
   if (type)
   {
      light = static_cast<ADynamicLight *>(AActor::StaticSpawn(type, target->x, target->y, target->z, NO_REPLACE));
      light->angle = this->m_Angle;
      light->SetOffset(m_X, m_Y, m_Z);
      light->lnext = NULL;
      light->target = target;
      light->targetState = targetState;
      light->halo = m_halo;
      //memcpy(light->args, this->m_Args, 5);
      for (int a = 0; a < 3; a++) light->args[a] = clamp<int>((int)(m_Args[a] * gl_lights_intensity), 0, 255);
      light->args[LIGHT_INTENSITY] = clamp<int>((int)(m_Args[LIGHT_INTENSITY] * gl_lights_size), 0, 255);
      light->args[LIGHT_SECONDARY_INTENSITY] = clamp<int>((int)(m_Args[LIGHT_SECONDARY_INTENSITY] * gl_lights_size), 0, 255);
   }

   return light;
}


IMPLEMENT_STATELESS_ACTOR (ADynamicLight, Any, -1, 0)
   PROP_HeightFixed (0)
   PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY)
   PROP_RenderFlags (RF_INVISIBLE)
END_DEFAULTS


float ADynamicLight::GetRadius()
{
   bool enable = m_enabled;

   if (enable && target)
   {
      if (target->state != targetState)
      {
         enable = false;
      }
   }

   return (enable ? GetIntensity() * 2.f : 0.f);
}

void ADynamicLight::Serialize(FArchive &arc)
{
   Super::Serialize (arc);
	arc << m_enabled;
   arc << m_owned;

   if (arc.IsLoading())
   {
      // when loading lights, if owned flag to be removed (owned with no target)...
      target = NULL;
   }
}


void ADynamicLight::BeginPlay()
{
   Super::BeginPlay();
   lnext = NULL;
}


void ADynamicLight::PostBeginPlay()
{
   Super::PostBeginPlay();

   if (this->target)
   {
      m_owned = true;
   }

   if (!(SpawnFlags & MTF_DORMANT))
   {
      Activate (NULL);
   }
}


void ADynamicLight::Activate(AActor *activator)
{
   m_enabled = true;

   Super::Activate(activator);
}


void ADynamicLight::Deactivate(AActor *activator)
{
   m_enabled = false;

   Super::Deactivate(activator);
}


void ADynamicLight::Tick()
{
   Super::Tick();

   if (m_owned)
   {
      if (!target || !target->state)
      {
         this->Destroy();
         return;
      }

      x = target->x + m_offX;
      y = target->y + m_offZ;
      z = target->z + m_offY;
      Sector = target->Sector;
   }
}


void ADynamicLight::SetOffset(fixed_t x, fixed_t y, fixed_t z)
{
   m_offX = x;
   m_offY = y;
   m_offZ = z;
}


void ADynamicLight::UpdateLocation()
{
   if (target)
   {
      x = target->x + m_offX;
      y = target->y + m_offY;
      z = target->z + m_offZ;
   }
}


//
// APointLight
//

IMPLEMENT_STATELESS_ACTOR (APointLight, Any, 9800, 0)
END_DEFAULTS

void APointLight::PostBeginPlay ()
{
   Super::PostBeginPlay ();
}


//
// APointLightPulse
//

IMPLEMENT_STATELESS_ACTOR (APointLightPulse, Any, 9801, 0)
END_DEFAULTS

void APointLightPulse::Serialize(FArchive &arc)
{
   float pulseTime;

   Super::Serialize(arc);

   if (arc.IsLoading())
   {
      pulseTime = ANGLE_TO_FLOAT(this->angle) / TICRATE;
      m_lastUpdate = frameStartMS;
      m_cycler.SetParams(args[LIGHT_SECONDARY_INTENSITY], args[LIGHT_INTENSITY], pulseTime);
      m_cycler.ShouldCycle(true);
      m_cycler.SetCycleType(CYCLE_Sin);
      m_currentIntensity = m_cycler.GetVal();
   }
}

void APointLightPulse::PostBeginPlay()
{
   Super::PostBeginPlay();

   float pulseTime = ANGLE_TO_FLOAT(this->angle) / TICRATE;

   m_lastUpdate = frameStartMS;
   m_cycler.SetParams(args[LIGHT_SECONDARY_INTENSITY], args[LIGHT_INTENSITY], pulseTime);
   m_cycler.ShouldCycle(true);
   m_cycler.SetCycleType(CYCLE_Sin);
   m_currentIntensity = (byte)m_cycler.GetVal();
}

void APointLightPulse::Tick()
{
   float diff = (frameStartMS - m_lastUpdate) / 1000.f;

   Super::Tick();

   m_lastUpdate = frameStartMS;
   if (m_enabled)
   {
      m_cycler.Update(diff);
      m_currentIntensity = m_cycler.GetVal();
   }
}


//
// APointLightFlicker
//

IMPLEMENT_STATELESS_ACTOR (APointLightFlicker, Any, 9802, 0)
END_DEFAULTS

void APointLightFlicker::PostBeginPlay()
{
   Super::PostBeginPlay();

   m_currentIntensity = args[LIGHT_INTENSITY];
}

void APointLightFlicker::Tick()
{
   byte rnd = randLight();
   float pct = this->angle * 1.f / ANGLE_MAX;

   Super::Tick();

   if (rnd >= pct * 255)
   {
      m_currentIntensity = args[LIGHT_SECONDARY_INTENSITY];
   }
   else
   {
      m_currentIntensity = args[LIGHT_INTENSITY];
   }
}


//
// ASectorPointLight
//

IMPLEMENT_STATELESS_ACTOR (ASectorPointLight, Any, 9803, 0)
END_DEFAULTS

float ASectorPointLight::GetIntensity()
{
   float intensity;
   float scale = args[LIGHT_SCALE] / 8.f;

   if (scale == 0.f) scale = 1.f;

   intensity = Sector->lightlevel * scale;
   intensity = clamp<float>(intensity, 0.f, 255.f);

   return intensity;
}


//
// APointLightFlickerRandom
//

IMPLEMENT_STATELESS_ACTOR (APointLightFlickerRandom, Any, 9804, 0)
END_DEFAULTS

void APointLightFlickerRandom::PostBeginPlay()
{
   Super::PostBeginPlay();

   m_currentIntensity = args[LIGHT_INTENSITY];
   m_tickCount = 0;
}

void APointLightFlickerRandom::Tick()
{
   byte flickerRange = args[LIGHT_SECONDARY_INTENSITY] - args[LIGHT_INTENSITY];
   float amt = randLight() / 255.f;

   Super::Tick();

   m_tickCount++;

   if (m_tickCount > (angle / ANGLE_1))
   {
      m_currentIntensity = args[LIGHT_INTENSITY] + (amt * flickerRange);
      m_tickCount = 0;
   }
}


//
// ASpotLight
//

IMPLEMENT_STATELESS_ACTOR (ASpotLight, Any, 9850, 0)
END_DEFAULTS

void ASpotLight::PostBeginPlay ()
{
   Super::PostBeginPlay ();
}


//
// ASpotTarget
//

IMPLEMENT_STATELESS_ACTOR (ASpotTarget, Any, 9851, 0)
END_DEFAULTS

void ASpotTarget::PostBeginPlay ()
{
   Super::PostBeginPlay ();
}


IMPLEMENT_STATELESS_ACTOR (APointLightSubtractive, Any, 9820, 0)
END_DEFAULTS

IMPLEMENT_STATELESS_ACTOR (APointLightPulseSubtractive, Any, 9821, 0)
END_DEFAULTS

IMPLEMENT_STATELESS_ACTOR (APointLightFlickerSubtractive, Any, 9822, 0)
END_DEFAULTS

IMPLEMENT_STATELESS_ACTOR (ASectorPointLightSubtractive, Any, 9823, 0)
END_DEFAULTS

IMPLEMENT_STATELESS_ACTOR (APointLightFlickerRandomSubtractive, Any, 9824, 0)
END_DEFAULTS

IMPLEMENT_STATELESS_ACTOR (AVavoomLight, Any, 9825, 0)
END_DEFAULTS

void AVavoomLight::PostBeginPlay ()
{
   Super::PostBeginPlay ();

   // since Vavoom lights z values are absolute, translate them to the height off the floor
   if (this->Sector) this->z -= this->Sector->floorplane.ZatPoint(this->x, this->y);
}

//
// end light classes
//


ADynamicLight *GL_CreateLight(const char *lightName, AActor *target, FState *targetState)
{
   unsigned int i;

   for (i = 0; i < LightDefaults.Size(); i++)
   {
      if (strcmp(LightDefaults[i]->GetName(), lightName) == 0)
      {
         return LightDefaults[i]->CreateLight(target, targetState);
      }
   }

   return NULL;
}


FLightAlias *GL_GetLightAlias(const char *name)
{
   for (unsigned int i = 0; i < LightAliases.Size(); i++)
   {
      if (strcmp(LightAliases[i]->AliasName(), name) == 0)
      {
         return LightAliases[i];
      }
   }

   return NULL;
}


ADynamicLight *GL_LightsForState(AActor *actor, FState *state)
{
   ADynamicLight *lights, *tmpLight, *light;
   spritedef_t *sprdef;
   spriteframe_t *sprframe;
   unsigned int i, j;
   FTexture *tex;
   const PClass *type;
   FLightAlias *alias;

   lights = tmpLight = NULL;

   sprdef = &sprites[state->sprite.index];
   sprframe = &SpriteFrames[sprdef->spriteframes + state->GetFrame()];
   type = RUNTIME_TYPE(actor);
   alias = GL_GetLightAlias(type->TypeName);
   if (alias)
   {
      type = PClass::FindClass(alias->RealName());
   }

   for (i = 0; i < LightAssociations.Size(); i++)
   {
      if (stricmp(type->TypeName, LightAssociations[i]->ActorName()) == 0)
      {
         for (j = 0; j < 16; j++)
         {
            tex = TexMan[sprframe->Texture[j]];
            if (strstr(tex->Name, LightAssociations[i]->FrameName()) == tex->Name)
            {
               light = GL_CreateLight(LightAssociations[i]->Light(), actor, state);
               if (light)
               {
                  if (!lights)
                  {
                     lights = light;
                     tmpLight = light;
                  }
                  else
                  {
                     tmpLight->lnext = light;
                     tmpLight = tmpLight->lnext;
                  }
               }
               else
               {
                  Printf("Unknown light: %s\n", LightAssociations[i]->Light());
               }
               break;
            }
         }
      }
   }

   return lights;
}


void GL_CheckActorLights(AActor *actor)
{
   int i, numStates;
   FState *state;
   const PClass *type;
   FLightAlias *alias;

   // lights don't have attached lights...
   if (actor->IsKindOf(PClass::FindClass("DynamicLight"))) return;

   if (actor->IsKindOf(PClass::FindClass("Actor")))
   {
      type = RUNTIME_TYPE(actor);
	  if( type != NULL ){
		alias = GL_GetLightAlias(type->TypeName);
		if (alias)
		{
			type = PClass::FindClass(alias->RealName());
		}
		if( type != NULL ){
			numStates = 0;
			// walk back up the heirarchy to find the parent with the most states
			// usually this is a pretty short trip (1 or 2 parents)
			while ( (type != NULL) && (stricmp("AActor", type->TypeName) != 0) )
			{
					if( type->ActorInfo )
						numStates = MAX<int>(numStates, type->ActorInfo->NumOwnedStates);
					type = type->ParentClass;
			}
		}
	  }
   }
   else
   {
      return;
   }

   if (actor->Lights.Size() != numStates)
   {
      actor->Lights.Resize(numStates);
      type = RUNTIME_TYPE(actor);
      alias = GL_GetLightAlias(type->TypeName);
      if (alias)
      {
         type = PClass::FindClass(alias->RealName());
      }
      for (i = 0; i < numStates; i++)
      {
         // make sure to read the states from the parent classes, as well
         while (i >= type->ActorInfo->NumOwnedStates)
         {
            type = type->ParentClass;
         }
         state = &type->ActorInfo->OwnedStates[i];
         actor->Lights[i] = GL_LightsForState(actor, state);
      }
   }
}


void GL_GetLightForPoint(float x, float y, float z, float *r, float *g, float *b)
{
   Vector p, l, t, lightColor;
   APointLight *light;
   float frac, dist, tr, tg, tb, lr, lg, lb;
   fixed_t fx, fy;
   subsector_t *subSec;

   fx = (fixed_t)(-x * FRACUNIT);
   fy = (fixed_t)(z * FRACUNIT);
   subSec = R_PointInSubsector(fx, fy);
   if (!subSec) return;

   p.Set(x, y, z);

   tr = tg = tb = 0.f;

   for (unsigned int i = 0; i < SubsecLights[subSec->index]->Size(); i++)
   {
      light = static_cast<APointLight *>(SubsecLights[subSec->index]->Item(i));

      l.Set(-light->x * MAP_SCALE, light->z * MAP_SCALE, light->y * MAP_SCALE);
      t = l - p;
      dist = t.Length();
      if (dist > light->GetRadius() * MAP_COEFF) continue;

      frac = 1.f - (dist / (light->GetRadius() * MAP_COEFF));

      lightColor.Set(byte2float[light->GetRed()], byte2float[light->GetGreen()], byte2float[light->GetBlue()]);
      //lightColor.Normalize(); // don't bother saturating the light color
      if (light->IsSubtractive())
      {
         lr = (lightColor.Length() - lightColor.X()) * -1;
         lg = (lightColor.Length() - lightColor.Y()) * -1;
         lb = (lightColor.Length() - lightColor.Z()) * -1;
      }
      else
      {
         lr = lightColor.X();
         lg = lightColor.Y();
         lb = lightColor.Z();
      }

      tr += lr * frac;
      tg += lg * frac;
      tb += lb * frac;
   }

   *r += tr;
   *g += tg;
   *b += tb;

   *r = clamp<float>(*r, 0.f, 1.f);
   *g = clamp<float>(*g, 0.f, 1.f);
   *b = clamp<float>(*b, 0.f, 1.f);
}


int GL_GetIntensityForPoint(float x, float y, float z)
{
   Vector p, l, t;
   APointLight *light;
   float frac, dist, intensity;
   fixed_t fx, fy;
   subsector_t *subSec;

   fx = (fixed_t)(-x * FRACUNIT);
   fy = (fixed_t)(z * FRACUNIT);
   subSec = R_PointInSubsector(fx, fy);
   if (!subSec) return 0;

   intensity = 0.f;
   p.Set(x, y, z);

   for (unsigned int i = 0; i < SubsecLights[subSec->index]->Size(); i++)
   {
      light = static_cast<APointLight *>(SubsecLights[subSec->index]->Item(i));

      l.Set(-light->x * MAP_SCALE, light->z * MAP_SCALE, light->y * MAP_SCALE);
      t = l - p;
      dist = t.Length();
      if (dist > light->GetRadius() * MAP_COEFF) continue;

      frac = 1.f - (dist / (light->GetRadius() * MAP_COEFF));
      intensity += frac * 255;
   }

   return clamp<int>((int)intensity, 0, 255);
}


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
   "halo",
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
   LIGHTTAG_HALO
};


extern int ScriptDepth;


float GL_ParseFloat()
{
   SC_GetFloat();

   return sc_Float;
}


int GL_ParseInt()
{
   SC_GetNumber();

   return sc_Number;
}


char *GL_ParseString()
{
   SC_GetString();

   return sc_String;
}


void GL_ParseTriple(float floatVal[3])
{
   for (int i = 0; i < 3; i++)
   {
      floatVal[i] = GL_ParseFloat();
   }
}


void GL_AddLightDefaults(FLightDefaults *defaults)
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
void GL_ParsePointLight()
{
   int type;
   float floatTriple[3];
   int intVal;
   std::string name;
   FLightDefaults *defaults;

   // get name
   SC_GetString();
   name = sc_String;

   // check for opening brace
   SC_GetString();
   if (SC_Compare("{"))
   {
      defaults = new FLightDefaults(name.c_str(), "PointLight");
      ScriptDepth++;
      while (ScriptDepth)
      {
         SC_GetString();
         if ((type = SC_MatchString(LightTags)) != -1)
         {
            switch (type)
            {
            case LIGHTTAG_OPENBRACE:
               ScriptDepth++;
               break;
            case LIGHTTAG_CLOSEBRACE:
               ScriptDepth--;
               break;
            case LIGHTTAG_COLOR:
               GL_ParseTriple(floatTriple);
               defaults->SetArg(LIGHT_RED, clamp<int>((int)(floatTriple[0] * 255), 0, 255));
               defaults->SetArg(LIGHT_GREEN, clamp<int>((int)(floatTriple[1] * 255), 0, 255));
               defaults->SetArg(LIGHT_BLUE, clamp<int>((int)(floatTriple[2] * 255), 0, 255));
               break;
            case LIGHTTAG_OFFSET:
               GL_ParseTriple(floatTriple);
               defaults->SetOffset((fixed_t)(floatTriple[0] * FRACUNIT), (fixed_t)(floatTriple[1] * FRACUNIT), (fixed_t)(floatTriple[2] * FRACUNIT));
               break;
            case LIGHTTAG_SIZE:
               intVal = clamp<int>(GL_ParseInt(), 0, 255);
               defaults->SetArg(LIGHT_INTENSITY, intVal);
               break;
            case LIGHTTAG_SUBTRACTIVE:
               defaults->SetSubtractive(GL_ParseInt() != 0);
               break;
            case LIGHTTAG_HALO:
               defaults->SetHalo(GL_ParseInt() != 0);
               break;
            }
         }
         else
         {
            SC_ScriptError("Unknown tag: %s\n", (const char **)&sc_String);
         }
      }
      GL_AddLightDefaults(defaults);
   }
   else
   {
      SC_ScriptError("Expected '{'.\n");
   }
}


void GL_ParsePulseLight()
{
   int type;
   float floatVal, floatTriple[3];
   int intVal;
   std::string name;
   FLightDefaults *defaults;

   // get name
   SC_GetString();
   name = sc_String;

   // check for opening brace
   SC_GetString();
   if (SC_Compare("{"))
   {
      defaults = new FLightDefaults(name.c_str(), "PointLightPulse");
      ScriptDepth++;
      while (ScriptDepth)
      {
         SC_GetString();
         if ((type = SC_MatchString(LightTags)) != -1)
         {
            switch (type)
            {
            case LIGHTTAG_OPENBRACE:
               ScriptDepth++;
               break;
            case LIGHTTAG_CLOSEBRACE:
               ScriptDepth--;
               break;
            case LIGHTTAG_COLOR:
               GL_ParseTriple(floatTriple);
               defaults->SetArg(LIGHT_RED, clamp<int>((int)(floatTriple[0] * 255), 0, 255));
               defaults->SetArg(LIGHT_GREEN, clamp<int>((int)(floatTriple[1] * 255), 0, 255));
               defaults->SetArg(LIGHT_BLUE, clamp<int>((int)(floatTriple[2] * 255), 0, 255));
               break;
            case LIGHTTAG_OFFSET:
               GL_ParseTriple(floatTriple);
               defaults->SetOffset((fixed_t)(floatTriple[0] * FRACUNIT), (fixed_t)(floatTriple[1] * FRACUNIT), (fixed_t)(floatTriple[2] * FRACUNIT));
               break;
            case LIGHTTAG_SIZE:
               intVal = clamp<int>(GL_ParseInt(), 0, 255);
               defaults->SetArg(LIGHT_INTENSITY, intVal);
               break;
            case LIGHTTAG_SECSIZE:
               intVal = clamp<int>(GL_ParseInt(), 0, 255);
               defaults->SetArg(LIGHT_SECONDARY_INTENSITY, intVal);
               break;
            case LIGHTTAG_INTERVAL:
               floatVal = GL_ParseFloat();
               defaults->SetAngle((angle_t)(floatVal * TICRATE * ANGLE_1));
               break;
            case LIGHTTAG_SUBTRACTIVE:
               defaults->SetSubtractive(GL_ParseInt() != 0);
               break;
            case LIGHTTAG_HALO:
               defaults->SetHalo(GL_ParseInt() != 0);
               break;
            }
         }
         else
         {
            SC_ScriptError("Unknown tag: %s\n", (const char **)&sc_String);
         }
      }
      GL_AddLightDefaults(defaults);
   }
   else
   {
      SC_ScriptError("Expected '{'.\n");
   }
}


void GL_ParseFlickerLight()
{
   int type;
   float floatVal, floatTriple[3];
   int intVal;
   std::string name;
   FLightDefaults *defaults;

   // get name
   SC_GetString();
   name = sc_String;

   // check for opening brace
   SC_GetString();
   if (SC_Compare("{"))
   {
      defaults = new FLightDefaults(name.c_str(), "PointLightFlicker");
      ScriptDepth++;
      while (ScriptDepth)
      {
         SC_GetString();
         if ((type = SC_MatchString(LightTags)) != -1)
         {
            switch (type)
            {
            case LIGHTTAG_OPENBRACE:
               ScriptDepth++;
               break;
            case LIGHTTAG_CLOSEBRACE:
               ScriptDepth--;
               break;
            case LIGHTTAG_COLOR:
               GL_ParseTriple(floatTriple);
               defaults->SetArg(LIGHT_RED, clamp<int>((int)(floatTriple[0] * 255), 0, 255));
               defaults->SetArg(LIGHT_GREEN, clamp<int>((int)(floatTriple[1] * 255), 0, 255));
               defaults->SetArg(LIGHT_BLUE, clamp<int>((int)(floatTriple[2] * 255), 0, 255));
               break;
            case LIGHTTAG_OFFSET:
               GL_ParseTriple(floatTriple);
               defaults->SetOffset((fixed_t)(floatTriple[0] * FRACUNIT), (fixed_t)(floatTriple[1] * FRACUNIT), (fixed_t)(floatTriple[2] * FRACUNIT));
               break;
            case LIGHTTAG_SIZE:
               intVal = clamp<int>(GL_ParseInt(), 0, 255);
               defaults->SetArg(LIGHT_INTENSITY, intVal);
               break;
            case LIGHTTAG_SECSIZE:
               intVal = clamp<int>(GL_ParseInt(), 0, 255);
               defaults->SetArg(LIGHT_SECONDARY_INTENSITY, intVal);
               break;
            case LIGHTTAG_CHANCE:
               floatVal = GL_ParseFloat();
               defaults->SetAngle((angle_t)(floatVal * ANGLE_MAX));
               break;
            case LIGHTTAG_SUBTRACTIVE:
               defaults->SetSubtractive(GL_ParseInt() != 0);
               break;
            case LIGHTTAG_HALO:
               defaults->SetHalo(GL_ParseInt() != 0);
               break;
            }
         }
         else
         {
            SC_ScriptError("Unknown tag: %s\n", (const char **)&sc_String);
         }
      }
      GL_AddLightDefaults(defaults);
   }
   else
   {
      SC_ScriptError("Expected '{'.\n");
   }
}


void GL_ParseFlickerLight2()
{
   int type;
   float floatVal, floatTriple[3];
   int intVal;
   std::string name;
   FLightDefaults *defaults;

   // get name
   SC_GetString();
   name = sc_String;

   // check for opening brace
   SC_GetString();
   if (SC_Compare("{"))
   {
      defaults = new FLightDefaults(name.c_str(), "PointLightFlickerRandom");
      ScriptDepth++;
      while (ScriptDepth)
      {
         SC_GetString();
         if ((type = SC_MatchString(LightTags)) != -1)
         {
            switch (type)
            {
            case LIGHTTAG_OPENBRACE:
               ScriptDepth++;
               break;
            case LIGHTTAG_CLOSEBRACE:
               ScriptDepth--;
               break;
            case LIGHTTAG_COLOR:
               GL_ParseTriple(floatTriple);
               defaults->SetArg(LIGHT_RED, clamp<int>((int)(floatTriple[0] * 255), 0, 255));
               defaults->SetArg(LIGHT_GREEN, clamp<int>((int)(floatTriple[1] * 255), 0, 255));
               defaults->SetArg(LIGHT_BLUE, clamp<int>((int)(floatTriple[2] * 255), 0, 255));
               break;
            case LIGHTTAG_OFFSET:
               GL_ParseTriple(floatTriple);
               defaults->SetOffset((fixed_t)(floatTriple[0] * FRACUNIT), (fixed_t)(floatTriple[1] * FRACUNIT), (fixed_t)(floatTriple[2] * FRACUNIT));
               break;
            case LIGHTTAG_SIZE:
               intVal = clamp<int>(GL_ParseInt(), 0, 255);
               defaults->SetArg(LIGHT_INTENSITY, intVal);
               break;
            case LIGHTTAG_SECSIZE:
               intVal = clamp<int>(GL_ParseInt(), 0, 255);
               defaults->SetArg(LIGHT_SECONDARY_INTENSITY, intVal);
               break;
            case LIGHTTAG_INTERVAL:
               floatVal = GL_ParseFloat();
               defaults->SetAngle((angle_t)(floatVal * ANGLE_MAX));
               break;
            case LIGHTTAG_SUBTRACTIVE:
               defaults->SetSubtractive(GL_ParseInt() != 0);
               break;
            case LIGHTTAG_HALO:
               defaults->SetHalo(GL_ParseInt() != 0);
               break;
            }
         }
         else
         {
            SC_ScriptError("Unknown tag: %s\n", (const char **)&sc_String);
         }
      }
      GL_AddLightDefaults(defaults);
   }
   else
   {
      SC_ScriptError("Expected '{'.\n");
   }
}


void GL_ParseSectorLight()
{
   int type;
   float floatVal;
   float floatTriple[3];
   std::string name;
   FLightDefaults *defaults;

   // get name
   SC_GetString();
   name = sc_String;

   // check for opening brace
   SC_GetString();
   if (SC_Compare("{"))
   {
      defaults = new FLightDefaults(name.c_str(), "SectorPointLight");
      ScriptDepth++;
      while (ScriptDepth)
      {
         SC_GetString();
         if ((type = SC_MatchString(LightTags)) != -1)
         {
            switch (type)
            {
            case LIGHTTAG_OPENBRACE:
               ScriptDepth++;
               break;
            case LIGHTTAG_CLOSEBRACE:
               ScriptDepth--;
               break;
            case LIGHTTAG_COLOR:
               GL_ParseTriple(floatTriple);
               defaults->SetArg(LIGHT_RED, clamp<int>((int)(floatTriple[0] * 255), 0, 255));
               defaults->SetArg(LIGHT_GREEN, clamp<int>((int)(floatTriple[1] * 255), 0, 255));
               defaults->SetArg(LIGHT_BLUE, clamp<int>((int)(floatTriple[2] * 255), 0, 255));
               break;
            case LIGHTTAG_OFFSET:
               GL_ParseTriple(floatTriple);
               defaults->SetOffset((fixed_t)(floatTriple[0] * FRACUNIT), (fixed_t)(floatTriple[1] * FRACUNIT), (fixed_t)(floatTriple[2] * FRACUNIT));
               break;
            case LIGHTTAG_SCALE:
               floatVal = GL_ParseFloat();
               defaults->SetArg(LIGHT_SCALE, (byte)(floatVal * 255));
               break;
            case LIGHTTAG_SUBTRACTIVE:
               defaults->SetSubtractive(GL_ParseInt() != 0);
               break;
            case LIGHTTAG_HALO:
               defaults->SetHalo(GL_ParseInt() != 0);
               break;
            }
         }
         else
         {
            SC_ScriptError("Unknown tag: %s\n", (const char **)&sc_String);
         }
      }
      GL_AddLightDefaults(defaults);
   }
   else
   {
      SC_ScriptError("Expected '{'.\n");
   }
}


void GL_AddLightAssociation(FLightAssociation *assoc)
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


void GL_ParseFrame(std::string name)
{
   int type, startDepth;
   std::string frameName;

   // get name
   SC_GetString();
   if (strlen(sc_String) > 8)
   {
      SC_ScriptError("Name longer than 8 characters: %s\n", (const char **)&sc_String);
   }
   frameName = sc_String;

   startDepth = ScriptDepth;

   // check for opening brace
   SC_GetString();
   if (SC_Compare("{"))
   {
      ScriptDepth++;
      while (ScriptDepth > startDepth)
      {
         SC_GetString();
         if ((type = SC_MatchString(LightTags)) != -1)
         {
            switch (type)
            {
            case LIGHTTAG_OPENBRACE:
               ScriptDepth++;
               break;
            case LIGHTTAG_CLOSEBRACE:
               ScriptDepth--;
               break;
            case LIGHTTAG_LIGHT:
               GL_ParseString();
               GL_AddLightAssociation(new FLightAssociation(name.c_str(), frameName.c_str(), sc_String));
               break;
            }
         }
         else
         {
            SC_ScriptError("Unknown tag: %s\n", (const char **)&sc_String);
         }
      }
   }
   else
   {
      SC_ScriptError("Expected '{'.\n");
   }
}


void GL_ParseObject()
{
   int type;
   std::string name;

   // get name
   SC_GetString();
   name = "A"; // add the A to the start of the actor name
   name += sc_String;

   // check for opening brace
   SC_GetString();
   if (SC_Compare("{"))
   {
      ScriptDepth++;
      while (ScriptDepth)
      {
         SC_GetString();
         if ((type = SC_MatchString(LightTags)) != -1)
         {
            switch (type)
            {
            case LIGHTTAG_OPENBRACE:
               ScriptDepth++;
               break;
            case LIGHTTAG_CLOSEBRACE:
               ScriptDepth--;
               break;
            case LIGHTTAG_FRAME:
               GL_ParseFrame(name);
               break;
            }
         }
         else
         {
            SC_ScriptError("Unknown tag: %s\n", (const char **)&sc_String);
         }
      }
   }
   else
   {
      SC_ScriptError("Expected '{'.\n");
   }
}


void GL_ReleaseLights()
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

   for (i = 0; i < LightAliases.Size(); i++)
   {
      delete LightAliases[i];
   }

   LightAssociations.Clear();
   LightDefaults.Clear();
   LightAliases.Clear();
}


void GL_ReplaceLightAssociations(const char *oldName, const char *newName)
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


void GL_AddLightAlias(const char *fakeName, const char *realName)
{
   FLightAlias *alias = GL_GetLightAlias(fakeName);

   if (!alias)
   {
      alias = new FLightAlias(fakeName, realName);
      LightAliases.Push(alias);
   }
}


void GL_PrepareSubsectorLights()
{
   unsigned int i;

   for (i = 0; i < SubsecLights.Size(); i++)
   {
      delete SubsecLights[i];
   }

   SubsecLights.Resize(numsubsectors);

   for (i = 0; i < SubsecLights.Size(); i++)
   {
      SubsecLights[i] = new TArray<ADynamicLight *>();
   }
}


float GL_DistToSeg(seg_t *seg, fixed_t x, fixed_t y)
{
   float u, px, py;

   u = ((FIX2FLT(x - seg->v1->x) * FIX2FLT(seg->v2->x - seg->v1->x)) + (FIX2FLT(y - seg->v1->y) * FIX2FLT(seg->v2->y - seg->v1->y))) / (seg->length * seg->length);
   if (u < 0.f) u = 0.f; // clamp the test point to the line segment
   if (u > 1.f) u = 1.f;

   px = FIX2FLT(seg->v1->x) + (u * FIX2FLT(seg->v2->x - seg->v1->x));
   py = FIX2FLT(seg->v1->y) + (u * FIX2FLT(seg->v2->y - seg->v1->y));

   px -= FIX2FLT(x);
   py -= FIX2FLT(y);

   return (px*px) + (py*py);
}


SubsectorList *GL_CollectWithinRadius(subsector_t *subSec, float radius, fixed_t x, fixed_t y)
{
   SubsectorList *list, *temp;
   seg_t *seg;

   if (!subSec) return NULL;

   subSec->touched = true;

   list = new SubsectorList();
   list->AddSubsector(subSec);

   for (unsigned int i = 0; i < subSec->numlines; i++)
   {
      seg = segs + subSec->firstline + i;
      if (seg->PartnerSeg && !seg->PartnerSeg->Subsector->touched)
      {
         // check distance from x/y to seg and if within radius add PartnerSeg->Subsector (lather/rinse/repeat)
         if (GL_DistToSeg(seg, x, y) <= radius)
         {
            temp = GL_CollectWithinRadius(seg->PartnerSeg->Subsector, radius, x, y);
            if (temp)
            {
               list->LinkList(temp->Head);
               temp->UnlinkList();
               delete temp;
            }
         }
      }
   }

   return list;
}


SubsectorList *GL_SubsectorsWithinRadius(float radius, fixed_t x, fixed_t y)
{
   SubsectorList *list;
   subsector_t *subSec;

   subSec = R_PointInSubsector(x, y);
   if (!subSec) return NULL;

   list = GL_CollectWithinRadius(subSec, radius, x, y);

   return list;
}


void GL_LinkLights()
{
   unsigned int i;
   FThinkerIterator *iter;
   ADynamicLight *light;
   int count = 0;
   float radius;
   SubsectorList *subsecList;
   SubsectorList::SubsectorListNode *node;

   for (i = 0; i < SubsecLights.Size(); i++)
   {
      SubsecLights[i]->Clear();
   }

   if (!gl_lights) return;

   iter = new FThinkerIterator(&ADynamicLight::_StaticType);
   light = static_cast<ADynamicLight *>(iter->Next());

   while (light)
   {
      if ((radius = light->GetRadius()) > 0.f)
      {
         // passing in radius*radius allows us to do a distance check without any calls to sqrtf
         subsecList = GL_SubsectorsWithinRadius(radius*radius, light->x, light->y);
         count = 0;
         node = subsecList->Head;
         while (node)
         {
            SubsecLights[node->Subsec->index]->Push(light);
            node->Subsec->touched = false;
            node = node->Next;
         }
         delete subsecList;
      }
      light = static_cast<ADynamicLight *>(iter->Next());
   }
}


void GL_DrawHalos()
{
   float x, y, z, size;
   ADynamicLight *light;

   glDisable(GL_DEPTH_TEST);
   glDisable(GL_TEXTURE_2D);
   glDisable(GL_FOG);

   for (unsigned int i = 0; i < HaloLights.Size(); i++)
   {
      light = HaloLights[i];
      size = light->GetIntensity();
      x = -light->x * MAP_SCALE;
      y = light->z * MAP_SCALE;
      z = light->y * MAP_SCALE;

      glColor4f(size, size, size, 0.5f);
      glPointSize(64.f);

      glBegin(GL_POINTS);
         glVertex3f(x, y, z);
      glEnd();
   }

   glEnable(GL_TEXTURE_2D);
   glEnable(GL_DEPTH_TEST);
}


CCMD(gl_list_objectlights)
{
   unsigned int i;

   for (i = 0; i < LightAssociations.Size(); i++)
   {
      Printf("%s/%s => %s\n", LightAssociations[i]->ActorName(), LightAssociations[i]->FrameName(), LightAssociations[i]->Light());
   }
}
