#include "actor.h"
#include "p_local.h"
#include "s_sound.h"
#include "p_enemy.h"
#include "a_doomglobal.h"
#include "a_action.h"

void A_CyberAttack (AActor *self)
{
	AActor	*pMissile;

	if (!self->target)
		return;
				
	A_FaceTarget (self);
	pMissile = P_SpawnMissile (self, self->target, PClass::FindClass("Rocket"));

	// [BC] If we're the server, tell clients to spawn the missile.
	if (( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( pMissile ))
		SERVERCOMMANDS_SpawnMissile( pMissile );
}

void A_Hoof (AActor *self)
{
	S_Sound (self, CHAN_BODY, "cyber/hoof", 1, ATTN_IDLE);
	A_Chase (self);
}
