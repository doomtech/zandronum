/*
** a_hatetarget.cpp
** Something for monsters to hate and shoot at
**
**---------------------------------------------------------------------------
** Copyright 2003-2006 Randy Heit
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
#include "m_fixed.h"

// Hate Target --------------------------------------------------------------

class AHateTarget : public AActor
{
	DECLARE_CLASS (AHateTarget, AActor)
public:
	void BeginPlay ();
	angle_t AngleIncrements (void);
	int TakeSpecialDamage (AActor *inflictor, AActor *source, int damage, FName damagetype);
};

IMPLEMENT_CLASS (AHateTarget)

void AHateTarget::BeginPlay ()
{
	Super::BeginPlay ();
	if (angle != 0)
	{	// Each degree translates into 10 units of health
		health = Scale (angle >> 1, 1800, 0x40000000);
		// Round to the nearest 10 because of inaccuracy above
		health = (health + 5) / 10 * 10;
	}
	else
	{
		special2 = 1;
		health = 1000001;
	}
}

int AHateTarget::TakeSpecialDamage (AActor *inflictor, AActor *source, int damage, FName damagetype)
{
	if (special2 != 0)
	{
		return 0;
	}
	else
	{
		return damage;
	}
}

angle_t AHateTarget::AngleIncrements (void)
{
	return ANGLE_1;
}
