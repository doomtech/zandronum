/*
#include "actor.h"
#include "gi.h"
#include "m_random.h"
#include "s_sound.h"
#include "d_player.h"
#include "a_action.h"
#include "a_pickups.h"
#include "p_local.h"
#include "a_sharedglobal.h"
#include "p_enemy.h"
#include "d_event.h"
#include "gstrings.h"
#include "thingdef/thingdef.h"
*/

static FRandom pr_snoutattack ("SnoutAttack");
static FRandom pr_pigattack ("PigAttack");
static FRandom pr_pigplayerthink ("PigPlayerThink");

extern void AdjustPlayerAngle (AActor *, AActor *);

// Pig player ---------------------------------------------------------------

class APigPlayer : public APlayerPawn
{
	DECLARE_CLASS (APigPlayer, APlayerPawn)
public:
	void MorphPlayerThink ();
};

IMPLEMENT_CLASS (APigPlayer)

void APigPlayer::MorphPlayerThink ()
{
	if (player->morphTics & 15)
	{
		return;
	}
	if(!(velx | vely) && pr_pigplayerthink() < 64)
	{ // Snout sniff
		if (player->ReadyWeapon != NULL)
		{
			P_SetPsprite(player, ps_weapon, player->ReadyWeapon->FindState("Grunt"));
		}
		S_Sound (this, CHAN_VOICE, "PigActive1", 1, ATTN_NORM); // snort
		return;
	}
	if (pr_pigplayerthink() < 48)
	{
		S_Sound (this, CHAN_VOICE, "PigActive", 1, ATTN_NORM);
	}
}

//============================================================================
//
// A_SnoutAttack
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_SnoutAttack)
{
	angle_t angle;
	int damage;
	int slope;
	player_t *player;
	AActor *puff;
	AActor *linetarget;

	if (NULL == (player = self->player))
	{
		return;
	}

	damage = 3+(pr_snoutattack()&3);
	angle = player->mo->angle;
	slope = P_AimLineAttack(player->mo, angle, MELEERANGE, &linetarget);
	puff = P_LineAttack(player->mo, angle, MELEERANGE, slope, damage, NAME_Melee, "SnoutPuff", true, &linetarget);
	S_Sound(player->mo, CHAN_VOICE, "PigActive", 1, ATTN_NORM);

	// [Dusk] clients aren't properly aware of linetarget, thus they stop here.
	if (NETWORK_InClientMode()) return;

	if(linetarget)
	{
		AdjustPlayerAngle(player->mo, linetarget);

		// [Dusk] update angle
		if (NETWORK_GetState() == NETSTATE_SERVER)
			SERVERCOMMANDS_MoveThing(player->mo, CM_ANGLE);

		if(puff != NULL)
		{ // Bit something
			S_Sound(player->mo, CHAN_VOICE, "PigAttack", 1, ATTN_NORM);

			// [Dusk] tell clients of the attack sound
			if (NETWORK_GetState() == NETSTATE_SERVER)
				SERVERCOMMANDS_SoundActor (player->mo, CHAN_VOICE, "PigAttack", 1, ATTN_NORM);
		}
	}
}

//============================================================================
//
// A_PigPain
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_PigPain)
{
	CALL_ACTION(A_Pain, self);
	if (self->z <= self->floorz)
	{
		self->velz = FRACUNIT*7/2;
	}
}
