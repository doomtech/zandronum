
#ifndef __GL_LIGHTS_H__
#define __GL_LIGHTS_H__


#include "actor.h"
#include "gl_shaders.h"


enum
{
   LIGHT_RED = 0,
   LIGHT_GREEN = 1,
   LIGHT_BLUE = 2,
   LIGHT_INTENSITY = 3,
   LIGHT_SECONDARY_INTENSITY = 4,
   LIGHT_SCALE = 3
};


//
// Base class
//

class ADynamicLight : public AActor
{
   DECLARE_STATELESS_ACTOR (ADynamicLight, AActor)
public:
   virtual void Tick();
   virtual bool IsSpotlight() { return false; }
   virtual void Serialize(FArchive &arc);
   byte GetRed() { return args[LIGHT_RED]; }
   byte GetGreen() { return args[LIGHT_GREEN]; }
   byte GetBlue() { return args[LIGHT_BLUE]; }
   virtual float GetIntensity() { return args[LIGHT_INTENSITY] * 1.f; }
   float GetRadius();
   ADynamicLight *lnext;
   virtual void BeginPlay();
   void PostBeginPlay();
   void Activate(AActor *activator);
   void Deactivate(AActor *activator);
   void SetOwned(bool owned) { m_owned = owned; }
   void SetOffset(fixed_t x, fixed_t y, fixed_t z);
   void UpdateLocation();
   bool IsOwned() { return m_owned; }
   virtual bool IsSubtractive() { return false; }
   angle_t AngleIncrements() { return ANGLE_1; }
   FState *targetState;
   unsigned int validcount;
   bool halo;
protected:
   bool m_enabled, m_owned;
   fixed_t m_offX, m_offY, m_offZ;
};


//
// Point Lights
//

class APointLight : public ADynamicLight
{
	DECLARE_STATELESS_ACTOR (APointLight, ADynamicLight)
public:
   virtual void PostBeginPlay();
};

class APointLightPulse : public APointLight
{
   DECLARE_STATELESS_ACTOR (APointLightPulse, APointLight)
public:
   void Serialize(FArchive &arc);
   void PostBeginPlay();
   virtual void Tick();
   virtual float GetIntensity() { return m_currentIntensity; }
protected:
   float m_currentIntensity;
   unsigned int m_lastUpdate;
   FCycler m_cycler;
};

class APointLightFlicker : public APointLight
{
   DECLARE_STATELESS_ACTOR (APointLightFlicker, APointLight)
public:
   void PostBeginPlay();
   virtual void Tick();
   virtual float GetIntensity() { return m_currentIntensity; }
protected:
   float m_currentIntensity;
};

class APointLightFlickerRandom : public APointLight
{
   DECLARE_STATELESS_ACTOR (APointLightFlickerRandom, APointLight)
public:
   void PostBeginPlay();
   virtual void Tick();
   virtual float GetIntensity() { return m_currentIntensity; }
protected:
   float m_currentIntensity;
   unsigned int m_tickCount;
};

class ASectorPointLight : public APointLight
{
   DECLARE_STATELESS_ACTOR (ASectorPointLight, APointLight)
public:
   virtual float GetIntensity();
};


// subtractive variants
class APointLightSubtractive : public APointLight
{
	DECLARE_STATELESS_ACTOR (APointLightSubtractive, APointLight)
public:
   virtual bool IsSubtractive() { return true; }
};

class APointLightPulseSubtractive : public APointLightPulse
{
   DECLARE_STATELESS_ACTOR (APointLightPulseSubtractive, APointLightPulse)
public:
   virtual bool IsSubtractive() { return true; }
};

class APointLightFlickerSubtractive : public APointLightFlicker
{
   DECLARE_STATELESS_ACTOR (APointLightFlickerSubtractive, APointLightFlicker)
public:
   virtual bool IsSubtractive() { return true; }
};

class APointLightFlickerRandomSubtractive : public APointLightFlickerRandom
{
   DECLARE_STATELESS_ACTOR (APointLightFlickerRandomSubtractive, APointLightFlickerRandom)
public:
   virtual bool IsSubtractive() { return true; }
};

class ASectorPointLightSubtractive : public ASectorPointLight
{
   DECLARE_STATELESS_ACTOR (ASectorPointLightSubtractive, ASectorPointLight)
public:
   virtual bool IsSubtractive() { return true; }
};

class AVavoomLight : public APointLight
{
   DECLARE_STATELESS_ACTOR (AVavoomLight, APointLight)
public:
   void PostBeginPlay();
   virtual float GetIntensity() { return args[LIGHT_INTENSITY] * 4.f; }
};


//
// Spot lights
//

class ASpotLight : public ADynamicLight
{
	DECLARE_STATELESS_ACTOR (ASpotLight, ADynamicLight)
public:
   virtual bool IsSpotlight() { return true; }
   void PostBeginPlay();
};

class ASpotTarget : public ADynamicLight
{
	DECLARE_STATELESS_ACTOR (ASpotTarget, ADynamicLight)
public:
   void PostBeginPlay();
};


typedef struct
{
   APointLight *Light;
   AActor *Owner;
} LightObject;


//
// Light helper methods
//

void GL_InitLights();
void GL_ReleaseLights();
void GL_CollectLights();
void GL_GetLightForPoint(float x, float y, float z, float *r, float *g, float *b);
void GL_CheckActorLights(AActor *actor);
int GL_GetIntensityForPoint(float x, float y, float z);
void GL_AddLightAlias(const char *fakeName, const char *realName);
void GL_DestroyActorLights(AActor *actor);
void GL_PrepareSubsectorLights();
void GL_LinkLights();
void GL_DrawHalos();

void GL_ParsePointLight();
void GL_ParsePulseLight();
void GL_ParseFlickerLight();
void GL_ParseFlickerLight2();
void GL_ParseSectorLight();
void GL_ParseObject();


#endif