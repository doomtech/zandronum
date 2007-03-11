
#include "gl_particles.h"


IMPLEMENT_STATELESS_ACTOR (AParticleSystem, Any, 9875, 0)
   PROP_HeightFixed (0)
   PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY)
   PROP_RenderFlags (RF_INVISIBLE)
END_DEFAULTS


AParticleSystem::~AParticleSystem()
{
}
