/*
** a_fountain.cpp
** Actors that make particle fountains
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
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

#include "actor.h"
#include "info.h"
#include "p_effect.h"
#include "network.h"

class AParticleFountain : public AActor
{
	DECLARE_STATELESS_ACTOR (AParticleFountain, AActor)
public:
	void PostBeginPlay ();
	void Activate (AActor *activator);
	void Deactivate (AActor *activator);
};

IMPLEMENT_STATELESS_ACTOR (AParticleFountain, Any, -1, 0)
	PROP_HeightFixed (0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY)
	PROP_RenderFlags (RF_INVISIBLE)
END_DEFAULTS

#define FOUNTAIN(color,ednum) \
	class A##color##ParticleFountain : public AParticleFountain { \
		DECLARE_STATELESS_ACTOR (A##color##ParticleFountain, AParticleFountain) }; \
	IMPLEMENT_STATELESS_ACTOR (A##color##ParticleFountain, Any, ednum, 0) \
		PROP_SpawnHealth (ednum-9026) \
	END_DEFAULTS

FOUNTAIN (Red, 9027);
FOUNTAIN (Green, 9028);
FOUNTAIN (Blue, 9029);
FOUNTAIN (Yellow, 9030);
FOUNTAIN (Purple, 9031);
FOUNTAIN (Black, 9032);
FOUNTAIN (White, 9033);

void AParticleFountain::PostBeginPlay ()
{
	Super::PostBeginPlay ();
	if (!(SpawnFlags & MTF_DORMANT))
		Activate (NULL);
}

void AParticleFountain::Activate (AActor *activator)
{
	Super::Activate (activator);
	effects &= ~FX_FOUNTAINMASK;
	effects |= health << FX_FOUNTAINSHIFT;
}

void AParticleFountain::Deactivate (AActor *activator)
{
	Super::Deactivate (activator);
	effects &= ~FX_FOUNTAINMASK;

	// [BC] In client mode, mark this as being dormant so that when PostBeginPlay() is
	// called, it doesn't try to activate the fountain.
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
		SpawnFlags |= MTF_DORMANT;
}

