#include "actor.h"
#include "info.h"
#include "p_local.h"
#include "s_sound.h"
#include "p_enemy.h"
#include "a_doomglobal.h"
#include "gstrings.h"
#include "a_action.h"

//
// PIT_VileCheck
// Detect a corpse that could be raised.
//
void A_StartFire (AActor *);
void A_FireCrackle (AActor *);
void A_Fire (AActor *);

class AArchvileFire : public AActor
{
	DECLARE_ACTOR (AArchvileFire, AActor)
};

FState AArchvileFire::States[] =
{
	S_BRIGHT (FIRE, 'A',	2, A_StartFire					, &States[1]),
	S_BRIGHT (FIRE, 'B',	2, A_Fire						, &States[2]),
	S_BRIGHT (FIRE, 'A',	2, A_Fire						, &States[3]),
	S_BRIGHT (FIRE, 'B',	2, A_Fire						, &States[4]),
	S_BRIGHT (FIRE, 'C',	2, A_FireCrackle				, &States[5]),
	S_BRIGHT (FIRE, 'B',	2, A_Fire						, &States[6]),
	S_BRIGHT (FIRE, 'C',	2, A_Fire						, &States[7]),
	S_BRIGHT (FIRE, 'B',	2, A_Fire						, &States[8]),
	S_BRIGHT (FIRE, 'C',	2, A_Fire						, &States[9]),
	S_BRIGHT (FIRE, 'D',	2, A_Fire						, &States[10]),
	S_BRIGHT (FIRE, 'C',	2, A_Fire						, &States[11]),
	S_BRIGHT (FIRE, 'D',	2, A_Fire						, &States[12]),
	S_BRIGHT (FIRE, 'C',	2, A_Fire						, &States[13]),
	S_BRIGHT (FIRE, 'D',	2, A_Fire						, &States[14]),
	S_BRIGHT (FIRE, 'E',	2, A_Fire						, &States[15]),
	S_BRIGHT (FIRE, 'D',	2, A_Fire						, &States[16]),
	S_BRIGHT (FIRE, 'E',	2, A_Fire						, &States[17]),
	S_BRIGHT (FIRE, 'D',	2, A_Fire						, &States[18]),
	S_BRIGHT (FIRE, 'E',	2, A_FireCrackle				, &States[19]),
	S_BRIGHT (FIRE, 'F',	2, A_Fire						, &States[20]),
	S_BRIGHT (FIRE, 'E',	2, A_Fire						, &States[21]),
	S_BRIGHT (FIRE, 'F',	2, A_Fire						, &States[22]),
	S_BRIGHT (FIRE, 'E',	2, A_Fire						, &States[23]),
	S_BRIGHT (FIRE, 'F',	2, A_Fire						, &States[24]),
	S_BRIGHT (FIRE, 'G',	2, A_Fire						, &States[25]),
	S_BRIGHT (FIRE, 'H',	2, A_Fire						, &States[26]),
	S_BRIGHT (FIRE, 'G',	2, A_Fire						, &States[27]),
	S_BRIGHT (FIRE, 'H',	2, A_Fire						, &States[28]),
	S_BRIGHT (FIRE, 'G',	2, A_Fire						, &States[29]),
	S_BRIGHT (FIRE, 'H',	2, A_Fire						, NULL)
};

IMPLEMENT_ACTOR (AArchvileFire, Doom, -1, 98)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY)
	PROP_Flags2 (MF2_MCROSS|MF2_PASSMOBJ|MF2_PUSHWALL)
	PROP_RenderStyle (STYLE_Add)

	PROP_SpawnState (0)
END_DEFAULTS

//
// A_VileStart
//
void A_VileStart (AActor *self)
{
	S_Sound (self, CHAN_VOICE, "vile/start", 1, ATTN_NORM);
}


//
// A_Fire
// Keep fire in front of player unless out of sight
//
void A_StartFire (AActor *self)
{
	S_Sound (self, CHAN_BODY, "vile/firestrt", 1, ATTN_NORM);
	A_Fire (self);
}

void A_FireCrackle (AActor *self)
{
	S_Sound (self, CHAN_BODY, "vile/firecrkl", 1, ATTN_NORM);
	A_Fire (self);
}

void A_Fire (AActor *self)
{
	AActor *dest;
	angle_t an;
				
	// [BC] Fire movement is server-side.
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
		return;

	dest = self->tracer;
	if (!dest)
		return;
				
	// don't move it if the vile lost sight
	if (!P_CheckSight (self->target, dest, 0) )
		return;

	an = dest->angle >> ANGLETOFINESHIFT;

	self->SetOrigin (dest->x + FixedMul (24*FRACUNIT, finecosine[an]),
					 dest->y + FixedMul (24*FRACUNIT, finesine[an]),
					 dest->z);

	// [BC] Tell clients of the fire update.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_MoveThingExact( self, CM_X|CM_Y|CM_Z );
}



//
// A_VileTarget
// Spawn the hellfire
//
void A_VileTarget (AActor *actor)
{
	AActor *fog;
		
	// [BC] Fire movement is server-side.
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
		return;

	if (!actor->target)
		return;

	A_FaceTarget (actor);

	fog = Spawn ("ArchvileFire", actor->target->x, actor->target->x,
		actor->target->z, ALLOW_REPLACE);
	
	// [BC] If we're the server, tell clients to spawn the thing.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SpawnThing( fog );

	actor->tracer = fog;
	fog->target = actor;
	fog->tracer = actor->target;
	A_Fire (fog);
}




//
// A_VileAttack
//
void A_VileAttack (AActor *actor)
{		
	AActor *fire;
	int an;
		
	if (!actor->target)
		return;
	
	A_FaceTarget (actor);

	if (!P_CheckSight (actor, actor->target, 0) )
		return;

	S_Sound (actor, CHAN_WEAPON, "vile/stop", 1, ATTN_NORM);
	P_DamageMobj (actor->target, actor, actor, 20, NAME_None);
	P_TraceBleed (20, actor->target);
	actor->target->momz = 1000 * FRACUNIT / actor->target->Mass;
		
	// [BC] Tell clients to play the arch-vile sound on their end.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SoundActor( actor, CHAN_WEAPON, "vile/stop", 127, ATTN_NORM );

	an = actor->angle >> ANGLETOFINESHIFT;

	fire = actor->tracer;

	if (!fire)
		return;
				
	// move the fire between the vile and the player
	fire->SetOrigin (actor->target->x + FixedMul (24*FRACUNIT, finecosine[an]),
					 actor->target->y + FixedMul (24*FRACUNIT, finesine[an]),
					 actor->target->z);
	
	P_RadiusAttack (fire, actor, 70, 70, NAME_Fire, false);
}
