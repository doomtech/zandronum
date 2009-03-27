
#ifndef __GL_LIGHTS_H__
#define __GL_LIGHTS_H__

#include "gl/gl_geometric.h"
#include "gl/gl_intern.h"
#include "gl/gl_data.h"
#include "gl/gl_cycler.h"


EXTERN_CVAR(Bool, gl_lights)
EXTERN_CVAR(Bool, gl_attachedlights)
EXTERN_CVAR(Bool, gl_bulletlight)

class ADynamicLight;
class FArchive;



enum
{
   LIGHT_RED = 0,
   LIGHT_GREEN = 1,
   LIGHT_BLUE = 2,
   LIGHT_INTENSITY = 3,
   LIGHT_SECONDARY_INTENSITY = 4,
   LIGHT_SCALE = 3,
};

// This is as good as something new - and it can be set directly in the ActorInfo!
#define MF4_SUBTRACTIVE MF4_MISSILEEVENMORE
#define MF4_ADDITIVE MF4_MISSILEMORE
#define MF4_DONTLIGHTSELF MF4_SEESDAGGERS

enum ELightType
{
	PointLight,
	PulseLight,
	FlickerLight,
	RandomFlickerLight,
	SectorLight,
	SpotLight,
	ColorPulseLight,
	ColorFlickerLight, 
	RandomColorFlickerLight
};


struct FLightNode
{
	FLightNode ** prevTarget;
	FLightNode * nextTarget;
	FLightNode ** prevLight;
	FLightNode * nextLight;
	ADynamicLight * lightsource;
	union
	{
		side_t * targLine;
		subsector_t * targSubsector;
		void * targ;
	};
};


//
// Base class
//
// [CO] I merged everything together in this one class so that I don't have
// to create and re-create an excessive amount of objects
//
class FLightDefaults;

class ADynamicLight : public AActor
{
	DECLARE_CLASS (ADynamicLight, AActor)
public:
	virtual void Tick();
	void Serialize(FArchive &arc);
	BYTE GetRed() const { return args[LIGHT_RED]; }
	BYTE GetGreen() const { return args[LIGHT_GREEN]; }
	BYTE GetBlue() const { return args[LIGHT_BLUE]; }
	float GetIntensity() const { return m_currentIntensity; }
	float GetRadius() const { return (IsActive() ? GetIntensity() * 2.f : 0.f); }
	void LinkLight();
	void UnlinkLight();
	size_t PointerSubstitution (DObject *old, DObject *notOld);

	virtual void BeginPlay();
	void PostBeginPlay();
	void Destroy();
	void Activate(AActor *activator);
	void Deactivate(AActor *activator);
	void SetOffset(fixed_t x, fixed_t y, fixed_t z);
	void UpdateLocation();
	bool IsOwned() const { return owned; }
	bool IsActive() const { return !(flags2&MF2_DORMANT); }
	bool IsSubtractive() { return !!(flags4&MF4_SUBTRACTIVE); }
	bool IsAdditive() { return !!(flags4&MF4_ADDITIVE); }
	FState *targetState;
	FLightNode * touching_sides;
	FLightNode * touching_subsectors;

private:
	float DistToSeg(seg_t *seg);
	void CollectWithinRadius(subsector_t *subSec, float radius);

protected:
	fixed_t m_offX, m_offY, m_offZ;
	float m_currentIntensity;
	int m_tickCount;
	unsigned int m_lastUpdate;
	FCycler m_cycler;
	subsector_t * subsector;

public:
	int m_intensity[2];
	BYTE lightflags;
	BYTE lighttype;
	bool owned;
	bool halo;
	BYTE color2[3];

	// intermediate texture coordinate data
	// this is stored in the light object to avoid recalculating it
	// several times during rendering of a flat
	Vector nearPt, up, right;
	float scale;

};

class AVavoomLight : public ADynamicLight
{
   DECLARE_CLASS (AVavoomLight, ADynamicLight)
public:
   virtual void BeginPlay();
};

class AVavoomLightWhite : public AVavoomLight
{
   DECLARE_CLASS (AVavoomLightWhite, AVavoomLight)
public:
   virtual void BeginPlay();
};

class AVavoomLightColor : public AVavoomLight
{
   DECLARE_CLASS (AVavoomLightColor, AVavoomLight)
public:
	void BeginPlay();
};


enum
{
	STAT_DLIGHT=64
};


//
// Light helper methods
//


extern unsigned int frameStartMS;

bool gl_SetupLight(Plane & p, ADynamicLight * light, Vector & nearPt, Vector & up, Vector & right, float & scale, int desaturation, bool checkside=true, bool forceadditive=true);
bool gl_SetupLightTexture();
void gl_GetLightForThing(AActor * thing, float upper, float lower, float & r, float & g, float & b);

extern bool i_useshaders;

#endif
