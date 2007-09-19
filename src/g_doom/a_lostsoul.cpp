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

	if (!self->target)
		return;
				
	dest = self->target;		
	self->flags |= MF_SKULLFLY;

	S_SoundID (self, CHAN_VOICE, self->AttackSound, 1, ATTN_NORM);

	// [BC] If we're the server, tell clients play this sound.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SoundIDActor( self, CHAN_VOICE, self->AttackSound, 127, ATTN_NORM );

	A_FaceTarget (self);
	an = self->angle >> ANGLETOFINESHIFT;
	self->momx = FixedMul (SKULLSPEED, finecosine[an]);
	self->momy = FixedMul (SKULLSPEED, finesine[an]);
	dist = P_AproxDistance (dest->x - self->x, dest->y - self->y);
	dist = dist / SKULLSPEED;
	
	if (dist < 1)
		dist = 1;
	self->momz = (dest->z+(dest->height>>1) - self->z) / dist;
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
