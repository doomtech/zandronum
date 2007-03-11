
#ifndef __GL_PARTICLES_H__
#define __GL_PARTICLES_H__

#include "actor.h"


class AParticleSystem : public AActor
{
   DECLARE_STATELESS_ACTOR (AParticleSystem, AActor)
public:
   virtual ~AParticleSystem();
protected:
   int m_MaxParticles;
};

#endif //__GL_PARTICLES_H__