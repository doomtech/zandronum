/*
#include "actor.h"
#include "m_random.h"
#include "a_action.h"
#include "p_local.h"
#include "p_enemy.h"
#include "s_sound.h"
#include "a_strifeglobal.h"
#include "f_finale.h"
#include "thingdef/thingdef.h"
#include "g_level.h"
#include "doomstat.h"
*/
static FRandom pr_prog ("Programmer");

// The Programmer level ending thing ----------------------------------------
// [RH] I took some liberties to make this "cooler" than it was in Strife.

class AProgLevelEnder : public AInventory
{
	DECLARE_CLASS (AProgLevelEnder, AInventory)
public:
	void Tick ();
	PalEntry GetBlend ();
};

IMPLEMENT_CLASS (AProgLevelEnder)

//============================================================================
//
// AProgLevelEnder :: Tick
//
// Fade to black, end the level, then unfade.
//
//============================================================================

void AProgLevelEnder::Tick ()
{
	if (special2 == 0)
	{ // fade out over .66 second
		special1 += 255 / (TICRATE*2/3);
		if (++special1 >= 255)
		{
			special1 = 255;
			special2 = 1;
			G_ExitLevel (0, false);
		}
	}
	else
	{ // fade in over two seconds
		special1 -= 255 / (TICRATE*2);
		if (special1 <= 0)
		{
			Destroy ();
		}
	}
}

//============================================================================
//
// AProgLevelEnder :: GetBlend
//
//============================================================================

PalEntry AProgLevelEnder::GetBlend ()
{
	return PalEntry ((BYTE)special1, 0, 0, 0);
}

//============================================================================
//
// A_ProgrammerMelee
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_ProgrammerMelee)
{
	int damage;

	if (self->target == NULL)
		return;

	A_FaceTarget (self);

	if (!self->CheckMeleeRange ())
		return;

	S_Sound (self, CHAN_WEAPON, "programmer/clank", 1, ATTN_NORM);

	damage = ((pr_prog() % 10) + 1) * 6;
	P_DamageMobj (self->target, self, self, damage, NAME_Melee);
	P_TraceBleed (damage, self->target, self);
}

//============================================================================
//
// A_SpotLightning
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_SpotLightning)
{
	AActor *spot;

	// [CW] Clients may not do this.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) || ( CLIENTDEMO_IsPlaying( )))
		return;

	if (self->target == NULL)
		return;

	spot = Spawn("SpectralLightningSpot", self->target->x, self->target->y, self->target->floorz, ALLOW_REPLACE);
	if (spot != NULL)
	{
		spot->threshold = 25;
		spot->target = self;
		spot->health = -2;
		spot->tracer = self->target;

		// [CW] Tell clients to spawn the actor.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SpawnThing( spot );
	}
}

//============================================================================
//
// A_SpawnProgrammerBase
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_SpawnProgrammerBase)
{
	AActor *foo = Spawn("ProgrammerBase", self->x, self->y, self->z + 24*FRACUNIT, ALLOW_REPLACE);
	if (foo != NULL)
	{
		foo->angle = self->angle + ANGLE_180 + (pr_prog.Random2() << 22);
		foo->velx = FixedMul (foo->Speed, finecosine[foo->angle >> ANGLETOFINESHIFT]);
		foo->vely = FixedMul (foo->Speed, finesine[foo->angle >> ANGLETOFINESHIFT]);
		foo->velz = pr_prog() << 9;
	}
}

//============================================================================
//
// A_ProgrammerDeath
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_ProgrammerDeath)
{
	if (!CheckBossDeath (self))
		return;

	for (int i = 0; i < MAXPLAYERS; ++i)
	{
		if (playeringame[i] && players[i].health > 0)
		{
			players[i].mo->GiveInventoryType (RUNTIME_CLASS(AProgLevelEnder));
			break;
		}
	}
	// the sky change scripts are now done as special actions in MAPINFO
	CALL_ACTION(A_BossDeath, self);
}
