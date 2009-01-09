#include "templates.h"
#include "actor.h"
#include "info.h"
#include "m_random.h"
#include "s_sound.h"
#include "p_local.h"
#include "p_enemy.h"
#include "a_doomglobal.h"
#include "gi.h"
#include "gstrings.h"
#include "a_action.h"
#include "thingdef/thingdef.h"
// [BB] New #includes.
#include "cl_demo.h"

 FRandom pr_lost ("LostMissileRange");

//
// SkullAttack
// Fly at the player like a missile.
//
#define SKULLSPEED (20*FRACUNIT)

void A_SkullAttack (AActor *self)
{
	AActor *dest;
	angle_t an;
	int dist;
	int n;

	// [BC] This is handled server-side.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return;
	}

	if (!self->target)
		return;
				
	int index = CheckIndex (1, NULL);
	if (index >= 0) 
	{
		n = FLOAT2FIXED(EvalExpressionF (StateParameters[index], self));
		if (n == 0) n = SKULLSPEED;
	}
	else n = SKULLSPEED;

	dest = self->target;		
	self->flags |= MF_SKULLFLY;

	S_Sound (self, CHAN_VOICE, self->AttackSound, 1, ATTN_NORM);

	// [BC] If we're the server, tell clients play this sound.
	// [BB] And tell them of MF_SKULLFLY.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		SERVERCOMMANDS_SetThingFlags( self, FLAGSET_FLAGS );
		SERVERCOMMANDS_SoundActor( self, CHAN_VOICE, S_GetName( self->AttackSound ), 1, ATTN_NORM );
	}

	A_FaceTarget (self);
	an = self->angle >> ANGLETOFINESHIFT;
	self->momx = FixedMul (n, finecosine[an]);
	self->momy = FixedMul (n, finesine[an]);
	dist = P_AproxDistance (dest->x - self->x, dest->y - self->y);
	dist = dist / n;
	
	if (dist < 1)
		dist = 1;
	self->momz = (dest->z+(dest->height>>1) - self->z) / dist;

	// [BC] Update the lost soul's momentum.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_MoveThingExact( self, CM_X|CM_Y|CM_Z|CM_MOMX|CM_MOMY|CM_MOMZ );
}

//==========================================================================
//
// CVAR transsouls
//
// How translucent things drawn with STYLE_SoulTrans are. Normally, only
// Lost Souls have this render style, but a dehacked patch could give other
// things this style. Values less than 0.25 will automatically be set to
// 0.25 to ensure some degree of visibility. Likewise, values above 1.0 will
// be set to 1.0, because anything higher doesn't make sense.
//
//==========================================================================

CUSTOM_CVAR (Float, transsouls, 0.75f, CVAR_ARCHIVE)
{
	if (self < 0.25f)
	{
		self = 0.25f;
	}
	else if (self > 1.f)
	{
		self = 1.f;
	}
}
