/*
** a_sectoraction.cpp
** Actors that hold specials to be executed upon conditions in a sector
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

#include "r_defs.h"
#include "p_lnspec.h"

// The base class for sector actions ----------------------------------------

IMPLEMENT_STATELESS_ACTOR (ASectorAction, Any, -1, 0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY)
	PROP_Flags3 (MF3_DONTSPLASH)
END_DEFAULTS

void ASectorAction::Destroy ()
{
	// Remove ourself from this sector's list of actions
	AActor *probe = Sector->SecActTarget;
	union
	{
		AActor **act;
		ASectorAction **secact;
	} prev;
	prev.secact = &Sector->SecActTarget;

	while (probe && probe != this)
	{
		prev.act = &probe->tracer;
		probe = probe->tracer;
	}
	if (probe != NULL)
	{
		*prev.act = probe->tracer;
	}

	Super::Destroy ();
}

void ASectorAction::BeginPlay ()
{
	Super::BeginPlay ();

	// Add ourself to this sector's list of actions
	tracer = Sector->SecActTarget;
	Sector->SecActTarget = this;
}

void ASectorAction::Activate (AActor *source)
{
	flags2 &= ~MF2_DORMANT;	// Projectiles cannot trigger
}

void ASectorAction::Deactivate (AActor *source)
{
	flags2 |= MF2_DORMANT;	// Projectiles can trigger
}

bool ASectorAction::TriggerAction (AActor *triggerer, int activationType)
{
	if (tracer != NULL)
		return static_cast<ASectorAction *>(tracer)->TriggerAction (triggerer, activationType);
	else
		return false;
}

bool ASectorAction::CheckTrigger (AActor *triggerer) const
{
	if (special &&
		(triggerer->player ||
		 ((flags & MF_AMBUSH) && (triggerer->flags2 & MF2_MCROSS)) ||
		 ((flags2 & MF2_DORMANT) && (triggerer->flags2 & MF2_PCROSS))))
	{
		bool res = !!LineSpecials[special] (NULL, triggerer, false, args[0], args[1],
			args[2], args[3], args[4]);
		return res;
	}
	return false;
}

// Triggered when entering sector -------------------------------------------

class ASecActEnter : public ASectorAction
{
	DECLARE_STATELESS_ACTOR (ASecActEnter, ASectorAction)
public:
	bool TriggerAction (AActor *triggerer, int activationType);
};

IMPLEMENT_STATELESS_ACTOR (ASecActEnter, Any, 9998, 0)
END_DEFAULTS

bool ASecActEnter::TriggerAction (AActor *triggerer, int activationType)
{
	bool didit = (activationType & SECSPAC_Enter) ? CheckTrigger (triggerer) : false;
	return didit | Super::TriggerAction (triggerer, activationType);
}

// Triggered when leaving sector --------------------------------------------

class ASecActExit : public ASectorAction
{
	DECLARE_STATELESS_ACTOR (ASecActExit, ASectorAction)
public:
	bool TriggerAction (AActor *triggerer, int activationType);
};

IMPLEMENT_STATELESS_ACTOR (ASecActExit, Any, 9997, 0)
END_DEFAULTS

bool ASecActExit::TriggerAction (AActor *triggerer, int activationType)
{
	bool didit = (activationType & SECSPAC_Exit) ? CheckTrigger (triggerer) : false;
	return didit | Super::TriggerAction (triggerer, activationType);
}

// Triggered when hitting sector's floor ------------------------------------

class ASecActHitFloor : public ASectorAction
{
	DECLARE_STATELESS_ACTOR (ASecActHitFloor, ASectorAction)
public:
	bool TriggerAction (AActor *triggerer, int activationType);
};

// Skull Tag uses 9999 for a special that is triggered whenever
// the player is on the sector's floor. I think this is more useful.
IMPLEMENT_STATELESS_ACTOR (ASecActHitFloor, Any, 9999, 0)
END_DEFAULTS

bool ASecActHitFloor::TriggerAction (AActor *triggerer, int activationType)
{
	bool didit = (activationType & SECSPAC_HitFloor) ? CheckTrigger (triggerer) : false;
	return didit | Super::TriggerAction (triggerer, activationType);
}

// Triggered when hitting sector's ceiling ----------------------------------

class ASecActHitCeil : public ASectorAction
{
	DECLARE_STATELESS_ACTOR (ASecActHitCeil, ASectorAction)
public:
	bool TriggerAction (AActor *triggerer, int activationType);
};

IMPLEMENT_STATELESS_ACTOR (ASecActHitCeil, Any, 9996, 0)
END_DEFAULTS

bool ASecActHitCeil::TriggerAction (AActor *triggerer, int activationType)
{
	bool didit = (activationType & SECSPAC_HitCeiling) ? CheckTrigger (triggerer) : false;
	return didit | Super::TriggerAction (triggerer, activationType);
}

// Triggered when using inside sector ---------------------------------------

class ASecActUse : public ASectorAction
{
	DECLARE_STATELESS_ACTOR (ASecActUse, ASectorAction)
public:
	bool TriggerAction (AActor *triggerer, int activationType);
};

IMPLEMENT_STATELESS_ACTOR (ASecActUse, Any, 9995, 0)
END_DEFAULTS

bool ASecActUse::TriggerAction (AActor *triggerer, int activationType)
{
	bool didit = (activationType & SECSPAC_Use) ? CheckTrigger (triggerer) : false;
	return didit | Super::TriggerAction (triggerer, activationType);
}

// Triggered when using a sector's wall -------------------------------------

class ASecActUseWall : public ASectorAction
{
	DECLARE_STATELESS_ACTOR (ASecActUseWall, ASectorAction)
public:
	bool TriggerAction (AActor *triggerer, int activationType);
};

IMPLEMENT_STATELESS_ACTOR (ASecActUseWall, Any, 9994, 0)
END_DEFAULTS

bool ASecActUseWall::TriggerAction (AActor *triggerer, int activationType)
{
	bool didit = (activationType & SECSPAC_UseWall) ? CheckTrigger (triggerer) : false;
	return didit | Super::TriggerAction (triggerer, activationType);
}

// Triggered when eyes go below fake floor ----------------------------------

class ASecActEyesDive : public ASectorAction
{
	DECLARE_STATELESS_ACTOR (ASecActEyesDive, ASectorAction)
public:
	bool TriggerAction (AActor *triggerer, int activationType);
};

IMPLEMENT_STATELESS_ACTOR (ASecActEyesDive, Any, 9993, 0)
END_DEFAULTS

bool ASecActEyesDive::TriggerAction (AActor *triggerer, int activationType)
{
	bool didit = (activationType & SECSPAC_EyesDive) ? CheckTrigger (triggerer) : false;
	return didit | Super::TriggerAction (triggerer, activationType);
}

// Triggered when eyes go above fake floor ----------------------------------

class ASecActEyesSurface : public ASectorAction
{
	DECLARE_STATELESS_ACTOR (ASecActEyesSurface, ASectorAction)
public:
	bool TriggerAction (AActor *triggerer, int activationType);
};

IMPLEMENT_STATELESS_ACTOR (ASecActEyesSurface, Any, 9992, 0)
END_DEFAULTS

bool ASecActEyesSurface::TriggerAction (AActor *triggerer, int activationType)
{
	bool didit = (activationType & SECSPAC_EyesSurface) ? CheckTrigger (triggerer) : false;
	return didit | Super::TriggerAction (triggerer, activationType);
}

// Triggered when eyes go below fake floor ----------------------------------

class ASecActEyesBelowC : public ASectorAction
{
	DECLARE_STATELESS_ACTOR (ASecActEyesBelowC, ASectorAction)
public:
	bool TriggerAction (AActor *triggerer, int activationType);
};

IMPLEMENT_STATELESS_ACTOR (ASecActEyesBelowC, Any, 9983, 0)
END_DEFAULTS

bool ASecActEyesBelowC::TriggerAction (AActor *triggerer, int activationType)
{
	bool didit = (activationType & SECSPAC_EyesBelowC) ? CheckTrigger (triggerer) : false;
	return didit | Super::TriggerAction (triggerer, activationType);
}

// Triggered when eyes go above fake floor ----------------------------------

class ASecActEyesAboveC : public ASectorAction
{
	DECLARE_STATELESS_ACTOR (ASecActEyesAboveC, ASectorAction)
public:
	bool TriggerAction (AActor *triggerer, int activationType);
};

IMPLEMENT_STATELESS_ACTOR (ASecActEyesAboveC, Any, 9982, 0)
END_DEFAULTS

bool ASecActEyesAboveC::TriggerAction (AActor *triggerer, int activationType)
{
	bool didit = (activationType & SECSPAC_EyesAboveC) ? CheckTrigger (triggerer) : false;
	return didit | Super::TriggerAction (triggerer, activationType);
}

// Triggered when eyes go below fake floor ----------------------------------

class ASecActHitFakeFloor : public ASectorAction
{
	DECLARE_STATELESS_ACTOR (ASecActHitFakeFloor, ASectorAction)
public:
	bool TriggerAction (AActor *triggerer, int activationType);
};

IMPLEMENT_STATELESS_ACTOR (ASecActHitFakeFloor, Any, 9989, 0)
END_DEFAULTS

bool ASecActHitFakeFloor::TriggerAction (AActor *triggerer, int activationType)
{
	bool didit = (activationType & SECSPAC_HitFakeFloor) ? CheckTrigger (triggerer) : false;
	return didit | Super::TriggerAction (triggerer, activationType);
}
