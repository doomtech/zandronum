#include "actor.h"
#include "info.h"
#include "p_enemy.h"
#include "p_local.h"
#include "a_doomglobal.h"
#include "a_action.h"
#include "thingdef.h"

void A_PainAttack (AActor *);
void A_PainDie (AActor *);

void A_SkullAttack (AActor *self);

//
// A_PainShootSkull
// Spawn a lost soul and launch it at the target
//
void A_PainShootSkull (AActor *self, angle_t angle)
{
	fixed_t x, y, z;
	
	AActor *other;
	angle_t an;
	int prestep;

	// [BC] Spawning of additional lost souls is server-side.
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
		return;

	const PClass *spawntype = NULL;

	if (self->DamageType==NAME_Massacre) return;

	int index=CheckIndex(1, NULL);
	if (index>=0) 
	{
		spawntype = PClass::FindClass((ENamedName)StateParameters[index]);
	}
	if (spawntype == NULL) spawntype = PClass::FindClass("LostSoul");

	// [RH] check to make sure it's not too close to the ceiling
	if (self->z + self->height + 8*FRACUNIT > self->ceilingz)
	{
		if (self->flags & MF_FLOAT)
		{
			self->momz -= 2*FRACUNIT;
			self->flags |= MF_INFLOAT;
			self->flags4 |= MF4_VFRICTION;
		}
		return;
	}

	// [RH] make this optional
	if (i_compatflags & COMPATF_LIMITPAIN)
	{
		// count total number of skulls currently on the level
		// if there are already 20 skulls on the level, don't spit another one
		int count = 20;
		FThinkerIterator iterator (spawntype);
		DThinker *othink;

		while ( (othink = iterator.Next ()) )
		{
			if (--count == 0)
				return;
		}
	}

	// okay, there's room for another one
	an = angle >> ANGLETOFINESHIFT;
	
	prestep = 4*FRACUNIT +
		3*(self->radius + GetDefaultByType(spawntype)->radius)/2;
	
	x = self->x + FixedMul (prestep, finecosine[an]);
	y = self->y + FixedMul (prestep, finesine[an]);
	z = self->z + 8*FRACUNIT;
				
	// Check whether the Lost Soul is being fired through a 1-sided	// phares
	// wall or an impassible line, or a "monsters can't cross" line.//   |
	// If it is, then we don't allow the spawn.						//   V

	if (Check_Sides (self, x, y))
	{
		return;
	}

	other = Spawn (spawntype, x, y, z, ALLOW_REPLACE);

	// [BC] If we're the server, tell clients to spawn the actor.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SpawnThing( other );

	// Check to see if the new Lost Soul's z value is above the
	// ceiling of its new sector, or below the floor. If so, kill it.

	if ((other->z >
         (other->Sector->ceilingplane.ZatPoint (other->x, other->y) - other->height)) ||
        (other->z < other->Sector->floorplane.ZatPoint (other->x, other->y)))
	{
		// kill it immediately
		P_DamageMobj (other, self, self, 1000000, NAME_None);		//   ^
		return;														//   |
	}																// phares

	// Check for movements.

	if (!P_CheckPosition (other, other->x, other->y))
	{
		// kill it immediately
		P_DamageMobj (other, self, self, 1000000, NAME_None);		
		return;
	}

	// [RH] Lost souls hate the same things as their pain elementals
	other->CopyFriendliness (self, true);

	A_SkullAttack (other);
}


//
// A_PainAttack
// Spawn a lost soul and launch it at the target
// 
void A_PainAttack (AActor *self)
{
	if (!self->target)
		return;

	A_FaceTarget (self);
	A_PainShootSkull (self, self->angle);
}

void A_DualPainAttack (AActor *self)
{
	if (!self->target)
		return;

	A_FaceTarget (self);
	A_PainShootSkull (self, self->angle + ANG45);
	A_PainShootSkull (self, self->angle - ANG45);
}

void A_PainDie (AActor *self)
{
	if (self->target != NULL && self->IsFriend (self->target))
	{ // And I thought you were my friend!
		self->flags &= ~MF_FRIENDLY;
	}
	A_NoBlocking (self);
	A_PainShootSkull (self, self->angle + ANG90);
	A_PainShootSkull (self, self->angle + ANG180);
	A_PainShootSkull (self, self->angle + ANG270);
}
