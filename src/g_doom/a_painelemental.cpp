/*
#include "actor.h"
#include "info.h"
#include "p_enemy.h"
#include "p_local.h"
#include "a_action.h"
#include "templates.h"
#include "m_bbox.h"
#include "thingdef/thingdef.h"
#include "doomstat.h"
*/

DECLARE_ACTION(A_SkullAttack)

static const PClass *GetSpawnType(DECLARE_PARAMINFO)
{
	ACTION_PARAM_START(1);
	ACTION_PARAM_CLASS(spawntype, 0);

	if (spawntype == NULL) spawntype = PClass::FindClass("LostSoul");
	return spawntype;
}


#define SKULLSPEED (20*FRACUNIT)
void A_SkullAttack(AActor *self, fixed_t speed);

//
// A_PainShootSkull
// Spawn a lost soul and launch it at the target
//
void A_PainShootSkull (AActor *self, angle_t angle, const PClass *spawntype)
{
	fixed_t x, y, z;
	
	AActor *other;
	angle_t an;
	int prestep;

	// [BC] Spawning of additional lost souls is server-side.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return;
	}

	if (spawntype == NULL) return;

	if (self->DamageType==NAME_Massacre) return;

	// [RH] check to make sure it's not too close to the ceiling
	if (self->z + self->height + 8*FRACUNIT > self->ceilingz)
	{
		if (self->flags & MF_FLOAT)
		{
			self->velz -= 2*FRACUNIT;
			self->flags |= MF_INFLOAT;
			self->flags4 |= MF4_VFRICTION;
		}
		return;
	}

	// [RH] make this optional
	if (i_compatflags & COMPATF_LIMITPAIN)
	{
		// count total number of skulls currently on the level
		// if there are already 21 skulls on the level, don't spit another one
		int count = 21;
		FThinkerIterator iterator (spawntype);
		DThinker *othink;

		while ( (othink = iterator.Next ()) )
		{
			// [BB] Don't count actors hidden by HideOrDestroyIfSafe().
			AActor *actor = static_cast<AActor *> ( othink );
			if ( actor && ( actor->state == RUNTIME_CLASS ( AInventory )->ActorInfo->FindState("HideIndefinitely") ) )
				++count;

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

	FBoundingBox box(MIN(self->x, x), MIN(self->y, y), MAX(self->x, x), MAX(self->y, y));
	FBlockLinesIterator it(box);
	line_t *ld;

	while ((ld = it.Next()))
	{
		if (!(ld->flags & ML_TWOSIDED) ||
			(ld->flags & (ML_BLOCKING|ML_BLOCKMONSTERS|ML_BLOCKEVERYTHING)))
		{
			if (!(box.Left()   > ld->bbox[BOXRIGHT]  ||
				  box.Right()  < ld->bbox[BOXLEFT]   ||
				  box.Top()    < ld->bbox[BOXBOTTOM] ||
				  box.Bottom() > ld->bbox[BOXTOP]))
			{
				if (P_PointOnLineSide(self->x,self->y,ld) != P_PointOnLineSide(x,y,ld))
					return;  // line blocks trajectory				//   ^
			}
		}
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

	A_SkullAttack(other, SKULLSPEED);
}


//
// A_PainAttack
// Spawn a lost soul and launch it at the target
// 
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_PainAttack)
{
	if (!self->target)
		return;

	const PClass *spawntype = GetSpawnType(PUSH_PARAMINFO);
	A_FaceTarget (self);
	A_PainShootSkull (self, self->angle, spawntype);
}

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_DualPainAttack)
{
	if (!self->target)
		return;

	const PClass *spawntype = GetSpawnType(PUSH_PARAMINFO);
	A_FaceTarget (self);
	A_PainShootSkull (self, self->angle + ANG45, spawntype);
	A_PainShootSkull (self, self->angle - ANG45, spawntype);
}

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_PainDie)
{
	if (self->target != NULL && self->IsFriend (self->target))
	{ // And I thought you were my friend!
		self->flags &= ~MF_FRIENDLY;
	}
	const PClass *spawntype = GetSpawnType(PUSH_PARAMINFO);
	CALL_ACTION(A_NoBlocking, self);
	A_PainShootSkull (self, self->angle + ANG90, spawntype);
	A_PainShootSkull (self, self->angle + ANG180, spawntype);
	A_PainShootSkull (self, self->angle + ANG270, spawntype);
}
