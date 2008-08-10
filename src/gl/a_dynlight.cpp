#include "gl_pch.h"

/*
** a_dynlight.cpp
** Implements actors representing dynamic lights (hardware independent)
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

#include "templates.h"
#include "m_random.h"
#include "r_main.h"
#include "p_local.h"
#include "c_dispatch.h"
#include "gl/gl_lights.h"
#include "gl/gl_data.h"
#include "gl/gl_basic.h"
#include "gl/gl_functions.h"
#include "gl/gl_values.h"

EXTERN_CVAR (Float, gl_lights_size);
EXTERN_CVAR (Bool, gl_lights_additive);

//==========================================================================
//
// Actor classes
//
// For flexibility all functionality has been packed into a single class
// which is controlled by flags
//
//==========================================================================
IMPLEMENT_STATELESS_ACTOR (ADynamicLight, Any, -1, 0)
   PROP_HeightFixed (0)
   PROP_Radius(FRACUNIT/10)
   PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY)
   PROP_Flags4(MF4_FIXMAPTHINGPOS)	// don't let lights lie directly on a linedef.
   PROP_RenderFlags (RF_INVISIBLE)
   // [BB] The server doesn't handle dynamic lights, so allow the clients to spawn them.
   PROP_FlagsNetwork( NETFL_ALLOWCLIENTSPAWN )
END_DEFAULTS

IMPLEMENT_STATELESS_ACTOR (APointLight, Any, 9800, 0)
END_DEFAULTS

void APointLight::BeginPlay ()
{
	Super::BeginPlay();
	lighttype = PointLight;
}

IMPLEMENT_STATELESS_ACTOR (APointLightPulse, Any, 9801, 0)
END_DEFAULTS

void APointLightPulse::BeginPlay ()
{
	Super::BeginPlay();
	lighttype = PulseLight;
}

IMPLEMENT_STATELESS_ACTOR (APointLightFlicker, Any, 9802, 0)
END_DEFAULTS

void APointLightFlicker::BeginPlay ()
{
	Super::BeginPlay();
	lighttype = FlickerLight;
}

IMPLEMENT_STATELESS_ACTOR (ASectorPointLight, Any, 9803, 0)
END_DEFAULTS

void ASectorPointLight::BeginPlay ()
{
	Super::BeginPlay();
	lighttype = SectorLight;
}

IMPLEMENT_STATELESS_ACTOR (APointLightFlickerRandom, Any, 9804, 0)
END_DEFAULTS

void APointLightFlickerRandom::BeginPlay ()
{
	Super::BeginPlay();
	lighttype=RandomFlickerLight;
}


//IMPLEMENT_STATELESS_ACTOR (ASpotLight, Any, 9850, 0)
//END_DEFAULTS

//IMPLEMENT_STATELESS_ACTOR (ASpotTarget, Any, 9851, 0)
//END_DEFAULTS

IMPLEMENT_STATELESS_ACTOR (APointLightAdditive, Any, 9810, 0)
   PROP_Flags4Set (MF4_ADDITIVE)
END_DEFAULTS

IMPLEMENT_STATELESS_ACTOR (APointLightPulseAdditive, Any, 9811, 0)
   PROP_Flags4Set (MF4_ADDITIVE)
END_DEFAULTS

IMPLEMENT_STATELESS_ACTOR (APointLightFlickerAdditive, Any, 9812, 0)
   PROP_Flags4Set (MF4_ADDITIVE)
END_DEFAULTS

IMPLEMENT_STATELESS_ACTOR (ASectorPointLightAdditive, Any, 9813, 0)
   PROP_Flags4Set (MF4_ADDITIVE)
END_DEFAULTS

IMPLEMENT_STATELESS_ACTOR (APointLightFlickerRandomAdditive, Any, 9814, 0)
   PROP_Flags4Set (MF4_ADDITIVE)
END_DEFAULTS

IMPLEMENT_STATELESS_ACTOR (APointLightSubtractive, Any, 9820, 0)
   PROP_Flags4Set (MF4_SUBTRACTIVE)
END_DEFAULTS

IMPLEMENT_STATELESS_ACTOR (APointLightPulseSubtractive, Any, 9821, 0)
   PROP_Flags4Set (MF4_SUBTRACTIVE)
END_DEFAULTS

IMPLEMENT_STATELESS_ACTOR (APointLightFlickerSubtractive, Any, 9822, 0)
   PROP_Flags4Set (MF4_SUBTRACTIVE)
END_DEFAULTS

IMPLEMENT_STATELESS_ACTOR (ASectorPointLightSubtractive, Any, 9823, 0)
   PROP_Flags4Set (MF4_SUBTRACTIVE)
END_DEFAULTS

IMPLEMENT_STATELESS_ACTOR (APointLightFlickerRandomSubtractive, Any, 9824, 0)
   PROP_Flags4Set (MF4_SUBTRACTIVE)
END_DEFAULTS


IMPLEMENT_STATELESS_ACTOR (AVavoomLight, Any, 9825, 0)
END_DEFAULTS

void AVavoomLight::BeginPlay ()
{
	// This must not call Super::BeginPlay!
	ChangeStatNum(STAT_DLIGHT);
	if (Sector) z -= Sector->floorplane.ZatPoint(x, y);
	lighttype = PointLight;
}

IMPLEMENT_STATELESS_ACTOR (AVavoomLightWhite, Any, 1502, 0)
END_DEFAULTS

void AVavoomLightWhite::BeginPlay ()
{
	byte l_args[5];
	memcpy(l_args, args, 5);
	memset(args, 0, 5);
	m_intensity[0] = l_args[0]*4;
	args[LIGHT_RED] = 128;
	args[LIGHT_GREEN] = 128;
	args[LIGHT_BLUE] = 128;

	Super::BeginPlay();
}

IMPLEMENT_STATELESS_ACTOR (AVavoomLightColor, Any, 1503, 0)
END_DEFAULTS

void AVavoomLightColor::BeginPlay ()
{
	int l_args[5];
	memcpy(l_args, args, sizeof(l_args));
	memset(args, 0, 5);
	m_intensity[0] = l_args[0]*4;
	args[LIGHT_RED] = l_args[1] >> 1;
	args[LIGHT_GREEN] = l_args[2] >> 1;
	args[LIGHT_BLUE] = l_args[3] >> 1;

	Super::BeginPlay();
}

static FRandom randLight;

//==========================================================================
//
// Base class
//
//==========================================================================

//==========================================================================
//
//
//
//==========================================================================
void ADynamicLight::Serialize(FArchive &arc)
{
	Super::Serialize (arc);
	arc << lightflags << lighttype;
	arc << m_tickCount << m_currentIntensity;
	arc << m_intensity[0] << m_intensity[1];

	if (lighttype == PulseLight) arc << m_lastUpdate << m_cycler;
	if (arc.IsLoading()) LinkLight();

}


//==========================================================================
//
//
//
//==========================================================================
void ADynamicLight::BeginPlay()
{
	Super::BeginPlay();
	ChangeStatNum(STAT_DLIGHT);

	m_intensity[0] = args[LIGHT_INTENSITY];
	m_intensity[1] = args[LIGHT_SECONDARY_INTENSITY];
	if (args[LIGHT_RED]==0 && args[LIGHT_GREEN]==0 && args[LIGHT_BLUE]==0)
	{
		// Hackhack! This allows to predefine lights in DECORATE ;)
		PalEntry pe = GetClass()->Meta.GetMetaInt(AMETA_BloodColor);
		args[LIGHT_RED] = pe.r;
		args[LIGHT_GREEN] = pe.g;
		args[LIGHT_BLUE] = pe.b;
		m_intensity[0] = (byte)(radius>>FRACBITS);
	}
}

//==========================================================================
//
//
//
//==========================================================================
void ADynamicLight::PostBeginPlay()
{
	Super::PostBeginPlay();
	
	if (!(SpawnFlags & MTF_DORMANT))
	{
		Activate (NULL);
	}

	subsector = R_PointInSubsector2(x,y);
}


//==========================================================================
//
//
//
//==========================================================================
void ADynamicLight::Activate(AActor *activator)
{
	//Super::Activate(activator);
	flags2&=~MF2_DORMANT;	

	m_currentIntensity = m_intensity[0];
	m_tickCount = 0;

	if (lighttype == PulseLight)
	{
		float pulseTime = ANGLE_TO_FLOAT(this->angle) / TICRATE;
		
		m_lastUpdate = level.maptime;
		m_cycler.SetParams(m_intensity[1], m_intensity[0], pulseTime);
		m_cycler.ShouldCycle(true);
		m_cycler.SetCycleType(CYCLE_Sin);
		m_currentIntensity = (byte)m_cycler.GetVal();
	}
}


//==========================================================================
//
//
//
//==========================================================================
void ADynamicLight::Deactivate(AActor *activator)
{
	//Super::Deactivate(activator);
	flags2|=MF2_DORMANT;	
}


//==========================================================================
//
//
//
//==========================================================================
void ADynamicLight::Tick()
{

	if (IsOwned())
	{
		if (!target || !target->state)
		{
			this->Destroy();
			return;
		}
	}

	// Don't bother if the light won't be shown
	if (!IsActive()) return;

	// I am doing this with a type field so that I can dynamically alter the type of light
	// without having to create or maintain multiple objects.
	switch(lighttype)
	{
	case PulseLight:
	{
		float diff = (level.maptime - m_lastUpdate) / (float)TICRATE;
		
		m_lastUpdate = level.maptime;
		m_cycler.Update(diff);
		m_currentIntensity = m_cycler.GetVal();
		break;
	}

	case FlickerLight:
	{
		byte rnd = randLight();
		float pct = ANGLE_TO_FLOAT(angle)/360.f;
		
		m_currentIntensity = m_intensity[rnd >= pct * 255];
		break;
	}

	case RandomFlickerLight:
	{
		int flickerRange = m_intensity[1] - m_intensity[0];
		float amt = randLight() / 255.f;
		
		m_tickCount++;
		
		if (m_tickCount > ANGLE_TO_FLOAT(angle))
		{
			m_currentIntensity = m_intensity[0] + (amt * flickerRange);
			m_tickCount = 0;
		}
		break;
	}

	case SectorLight:
	{
		float intensity;
		float scale = args[LIGHT_SCALE] / 8.f;
		
		if (scale == 0.f) scale = 1.f;
		
		intensity = Sector->lightlevel * scale;
		intensity = clamp<float>(intensity, 0.f, 255.f);
		
		m_currentIntensity = intensity;
		break;
	}

	case PointLight:
		m_currentIntensity = m_intensity[0];
		break;
	}

	UpdateLocation();
}




//==========================================================================
//
//
//
//==========================================================================
void ADynamicLight::UpdateLocation()
{
	fixed_t oldx=x;
	fixed_t oldy=y;
	fixed_t oldradius=radius;
	float intensity;

	if (IsActive())
	{
		if (target)
		{
			PrevX = x = target->x + m_offX;
			PrevY = y = target->y + m_offZ;
			PrevZ = z = target->z + m_offY;
			subsector = R_PointInSubsector2(x, y);
			Sector = subsector->sector;
		}


		// The radius being used here is always the maximum possible with the
		// current settings. This avoids constant relinking of flickering lights

		if (lighttype == FlickerLight || lighttype == RandomFlickerLight) 
		{
			intensity = m_intensity[1];
		}
		else
		{
			intensity = m_currentIntensity;
		}
		radius = FROM_MAP(intensity * 2.0f * gl_lights_size);

		if (x!=oldx || y!=oldy || radius!=oldradius) 
		{
			//Update the light lists
			LinkLight();
		}
	}
}



//=============================================================================
//
// These have been copied from the secnode code and modified for the light links
//
// P_AddSecnode() searches the current list to see if this sector is
// already there. If not, it adds a sector node at the head of the list of
// sectors this object appears in. This is called when creating a list of
// nodes that will get linked in later. Returns a pointer to the new node.
//
//=============================================================================
static FreeList<FLightNode> freelist;

FLightNode * AddLightNode(FLightNode ** thread, void * linkto, ADynamicLight * light, FLightNode *& nextnode)
{
	FLightNode * node;

	node = nextnode;
	while (node)
    {
		if (node->targ==linkto)   // Already have a node for this sector?
		{
			node->lightsource = light; // Yes. Setting m_thing says 'keep it'.
			return(nextnode);
		}
		node = node->nextTarget;
    }

	// Couldn't find an existing node for this sector. Add one at the head
	// of the list.
	
	node = freelist.GetNew();
	
	node->targ = linkto;
	node->lightsource = light; 

	node->prevTarget = &nextnode; 
	node->nextTarget = nextnode;

	if (nextnode) nextnode->prevTarget = &node->nextTarget;
	
	// Add new node at head of sector thread starting at s->touching_thinglist
	
	node->prevLight = thread;  	
	node->nextLight = *thread; 
	if (node->nextLight) node->nextLight->prevLight=&node->nextLight;
	*thread = node;
	return(node);
}


//=============================================================================
//
// P_DelSecnode() deletes a sector node from the list of
// sectors this object appears in. Returns a pointer to the next node
// on the linked list, or NULL.
//
//=============================================================================

static FLightNode * DeleteLightNode(FLightNode * node)
{
	FLightNode * tn;  // next node on thing thread
	
	if (node)
    {
		
		*node->prevTarget = node->nextTarget;
		if (node->nextTarget) node->nextTarget->prevTarget=node->prevTarget;

		*node->prevLight = node->nextLight;
		if (node->nextLight) node->nextLight->prevLight=node->prevLight;
		
		// Return this node to the freelist
		tn=node->nextTarget;
		freelist.Release(node);
		return(tn);
    }
	return(NULL);
}                             // phares 3/13/98



//==========================================================================
//
// Gets the light's distance to a line
//
//==========================================================================

float ADynamicLight::DistToSeg(seg_t *seg)
{
   float u, px, py;

   float seg_dx = TO_MAP(seg->v2->x - seg->v1->x);
   float seg_dy = TO_MAP(seg->v2->y - seg->v1->y);
   float seg_length_sq = seg_dx * seg_dx + seg_dy * seg_dy;

   u = (  TO_MAP(x - seg->v1->x) * seg_dx + TO_MAP(y - seg->v1->y) * seg_dy) / seg_length_sq;
   if (u < 0.f) u = 0.f; // clamp the test point to the line segment
   if (u > 1.f) u = 1.f;

   px = TO_MAP(seg->v1->x) + (u * seg_dx);
   py = TO_MAP(seg->v1->y) + (u * seg_dy);

   px -= TO_MAP(x);
   py -= TO_MAP(y);

   return (px*px) + (py*py);
}


//==========================================================================
//
// Collect all touched sidedefs and subsectors
// to sidedefs and sector parts.
//
//==========================================================================

void ADynamicLight::CollectWithinRadius(subsector_t *subSec, float radius)
{
	if (!subSec) return;

	bool additive = (flags4&MF4_ADDITIVE) || gl_lights_additive;

	subSec->validcount = ::validcount;

	touching_subsectors = AddLightNode(&subSec->lighthead[additive], subSec, this, touching_subsectors);

	for (unsigned int i = 0; i < subSec->numlines; i++)
	{
		seg_t * seg = segs + subSec->firstline + i;

		if (seg->sidedef && seg->linedef && seg->linedef->validcount!=::validcount)
		{
			// light is in front of the seg
			if (DMulScale32 (y-seg->v1->y, seg->v2->x-seg->v1->x, seg->v1->x-x, seg->v2->y-seg->v1->y) <=0)
			{
				seg->linedef->validcount=validcount;
				touching_sides = AddLightNode(&seg->sidedef->lighthead[additive], 
											  seg->sidedef, this, touching_sides);
			}
		}

		if (seg->PartnerSeg && seg->PartnerSeg->Subsector->validcount!=::validcount)
		{
			// check distance from x/y to seg and if within radius add PartnerSeg->Subsector (lather/rinse/repeat)
			if (DistToSeg(seg) <= radius)
			{
				CollectWithinRadius(seg->PartnerSeg->Subsector, radius);
			}
		}
	}
}

//==========================================================================
//
// Link the light into the world
//
//==========================================================================

void ADynamicLight::LinkLight()
{
	// mark the old light nodes
	FLightNode * node;
	
	node = touching_sides;
	while (node)
    {
		node->lightsource = NULL;
		node = node->nextTarget;
    }
	node = touching_subsectors;
	while (node)
    {
		node->lightsource = NULL;
		node = node->nextTarget;
    }

	if (radius>0)
	{
		// passing in radius*radius allows us to do a distance check without any calls to sqrtf
		::validcount++;
		subsector_t * subSec = R_PointInSubsector2(x, y);
		if (subSec)
		{
			float fradius = TO_MAP(radius);
			CollectWithinRadius(subSec, fradius*fradius);
		}
	}
		
	// Now delete any nodes that won't be used. These are the ones where
	// m_thing is still NULL.
	
	node = touching_sides;
	while (node)
	{
		if (node->lightsource == NULL)
		{
			node = DeleteLightNode(node);
		}
		else
			node = node->nextTarget;
	}

	node = touching_subsectors;
	while (node)
	{
		if (node->lightsource == NULL)
		{
			node = DeleteLightNode(node);
		}
		else
			node = node->nextTarget;
	}
}


//==========================================================================
//
// Deletes the link lists
//
//==========================================================================
void ADynamicLight::UnlinkLight ()

{
	while (touching_sides) touching_sides = DeleteLightNode(touching_sides);
	while (touching_subsectors) touching_subsectors = DeleteLightNode(touching_subsectors);
}

void ADynamicLight::Destroy()
{
	UnlinkLight();
	Super::Destroy();
}



CCMD(listlights)
{
	int walls, sectors;
	int allwalls=0, allsectors=0;
	int i=0;
	ADynamicLight * dl;
	TThinkerIterator<ADynamicLight> it;

	while (dl=it.Next())
	{
		walls=0;
		sectors=0;
		Printf("%s at (%f, %f, %f), color = 0x%02x%02x%02x, radius = %f ",
			dl->target? dl->target->GetClass()->TypeName.GetChars() : dl->GetClass()->TypeName.GetChars(),
			TO_MAP(dl->x), TO_MAP(dl->y), TO_MAP(dl->z), dl->args[LIGHT_RED], 
			dl->args[LIGHT_GREEN], dl->args[LIGHT_BLUE], TO_MAP(dl->radius));
		i++;

		if (dl->target)
		{
			int spr = gl_GetSpriteFrame(dl->target->sprite, dl->target->frame, 0, 0);
			Printf(", frame = %s ", TexMan[spr]->Name);
		}


		FLightNode * node;

		node=dl->touching_sides;

		while (node)
		{
			walls++;
			allwalls++;
			node = node->nextTarget;
		}

		node=dl->touching_subsectors;

		while (node)
		{
			allsectors++;
			sectors++;
			node = node->nextTarget;
		}

		Printf("- %d walls, %d subsectors\n", walls, sectors);

	}
	Printf("%i dynamic lights, %d walls, %d subsectors\n\n\n", i, allwalls, allsectors);
}

