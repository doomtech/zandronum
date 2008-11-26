#include "actor.h"
#include "info.h"
#include "m_random.h"
#include "p_local.h"
#include "p_enemy.h"
#include "s_sound.h"
#include "a_doomglobal.h"
#include "statnums.h"
#include "thingdef/thingdef.h"
// [BB] New #includes.
#include "cl_demo.h"
#include "deathmatch.h"
#include "team.h"

void A_Fire (AActor *);		// from m_archvile.cpp

void A_BrainAwake (AActor *);
void A_BrainPain (AActor *);
void A_BrainScream (AActor *);
void A_BrainExplode (AActor *);
void A_BrainDie (AActor *);
void A_BrainSpit (AActor *);
void A_SpawnFly (AActor *);
void A_SpawnSound (AActor *);

static FRandom pr_brainscream ("BrainScream");
static FRandom pr_brainexplode ("BrainExplode");
static FRandom pr_spawnfly ("SpawnFly");

class ABossTarget : public AActor
{
	DECLARE_STATELESS_ACTOR (ABossTarget, AActor)
public:
	void BeginPlay ();
};

class DBrainState : public DThinker
{
	DECLARE_CLASS (DBrainState, DThinker)
public:
	DBrainState ()
		: DThinker (STAT_BOSSTARGET),
		  Targets (STAT_BOSSTARGET),
		  SerialTarget (NULL),
		  Easy (false)
	{}
	void Serialize (FArchive &arc);
	ABossTarget *GetTarget ();
protected:
	TThinkerIterator<ABossTarget> Targets;
	ABossTarget *SerialTarget;
	bool Easy;
};

FState ABossBrain::States[] =
{
#define S_BRAINEXPLODE 0
	S_BRIGHT (MISL, 'B',   10, NULL 			, &States[S_BRAINEXPLODE+1]),
	S_BRIGHT (MISL, 'C',   10, NULL 			, &States[S_BRAINEXPLODE+2]),
	S_BRIGHT (MISL, 'D',   10, A_BrainExplode	, NULL),

#define S_BRAIN (S_BRAINEXPLODE+3)
	S_NORMAL (BBRN, 'A',   -1, NULL 			, NULL),

#define S_BRAIN_PAIN (S_BRAIN+1)
	S_NORMAL (BBRN, 'B',   36, A_BrainPain		, &States[S_BRAIN]),

#define S_BRAIN_DIE (S_BRAIN_PAIN+1)
	S_NORMAL (BBRN, 'A',  100, A_BrainScream	, &States[S_BRAIN_DIE+1]),
	S_NORMAL (BBRN, 'A',   10, NULL 			, &States[S_BRAIN_DIE+2]),
	S_NORMAL (BBRN, 'A',   10, NULL 			, &States[S_BRAIN_DIE+3]),
	S_NORMAL (BBRN, 'A',   -1, A_BrainDie		, NULL)
};

IMPLEMENT_ACTOR (ABossBrain, Doom, 88, 0)
	PROP_SpawnHealth (250)
	//PROP_HeightFixed (86)		// don't do this; it messes up some non-id levels
	PROP_MassLong (10000000)
	PROP_PainChance (255)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE)
	PROP_Flags4 (MF4_NOICEDEATH)
	PROP_Flags5 (MF5_OLDRADIUSDMG)

	PROP_SpawnState (S_BRAIN)
	PROP_PainState (S_BRAIN_PAIN)
	PROP_DeathState (S_BRAIN_DIE)

	PROP_PainSound ("brain/pain")
	PROP_DeathSound ("brain/death")
END_DEFAULTS

class ABossEye : public AActor
{
	DECLARE_ACTOR (ABossEye, AActor)
};

FState ABossEye::States[] =
{
#define S_BRAINEYE 0
	S_NORMAL (SSWV, 'A',   10, A_Look						, &States[S_BRAINEYE]),

#define S_BRAINEYESEE (S_BRAINEYE+1)
	S_NORMAL (SSWV, 'A',  181, A_BrainAwake 				, &States[S_BRAINEYESEE+1]),
	S_NORMAL (SSWV, 'A',  150, A_BrainSpit					, &States[S_BRAINEYESEE+1])
};

IMPLEMENT_ACTOR (ABossEye, Doom, 89, 0)
	PROP_HeightFixed (32)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOSECTOR)

	PROP_SpawnState (S_BRAINEYE)
	PROP_SeeState (S_BRAINEYESEE)
END_DEFAULTS

IMPLEMENT_STATELESS_ACTOR (ABossTarget, Doom, 87, 0)
	PROP_HeightFixed (32)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOSECTOR)
END_DEFAULTS

void ABossTarget::BeginPlay ()
{
	Super::BeginPlay ();
	ChangeStatNum (STAT_BOSSTARGET);
}

class ASpawnShot : public AActor
{
	DECLARE_ACTOR (ASpawnShot, AActor)
};

FState ASpawnShot::States[] =
{
	S_BRIGHT (BOSF, 'A',	3, A_SpawnSound 				, &States[1]),
	S_BRIGHT (BOSF, 'B',	3, A_SpawnFly					, &States[2]),
	S_BRIGHT (BOSF, 'C',	3, A_SpawnFly					, &States[3]),
	S_BRIGHT (BOSF, 'D',	3, A_SpawnFly					, &States[0])
};

IMPLEMENT_ACTOR (ASpawnShot, Doom, -1, 0)
	PROP_RadiusFixed (6)
	PROP_HeightFixed (32)
	PROP_SpeedFixed (10)
	PROP_Damage (3)
	PROP_Flags (MF_NOBLOCKMAP|MF_MISSILE|MF_DROPOFF|MF_NOGRAVITY|MF_NOCLIP)
	PROP_Flags2 (MF2_NOTELEPORT)
	PROP_Flags4 (MF4_RANDOMIZE)

	PROP_SpawnState (0)

	PROP_SeeSound ("brain/spit")
	PROP_DeathSound ("brain/cubeboom")
END_DEFAULTS

class ASpawnFire : public AActor
{
	DECLARE_ACTOR (ASpawnFire, AActor)
};

FState ASpawnFire::States[] =
{
	S_BRIGHT (FIRE, 'A',	4, A_Fire						, &States[1]),
	S_BRIGHT (FIRE, 'B',	4, A_Fire						, &States[2]),
	S_BRIGHT (FIRE, 'C',	4, A_Fire						, &States[3]),
	S_BRIGHT (FIRE, 'D',	4, A_Fire						, &States[4]),
	S_BRIGHT (FIRE, 'E',	4, A_Fire						, &States[5]),
	S_BRIGHT (FIRE, 'F',	4, A_Fire						, &States[6]),
	S_BRIGHT (FIRE, 'G',	4, A_Fire						, &States[7]),
	S_BRIGHT (FIRE, 'H',	4, A_Fire						, NULL)
};

IMPLEMENT_ACTOR (ASpawnFire, Doom, -1, 0)
	PROP_HeightFixed (78)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY)
	PROP_RenderStyle (STYLE_Add)

	PROP_SpawnState (0)
END_DEFAULTS

void A_BrainAwake (AActor *self)
{
	// killough 3/26/98: only generates sound now
	S_Sound (self, CHAN_VOICE, "brain/sight", 1, ATTN_NONE);
}

void A_BrainPain (AActor *self)
{
	S_Sound (self, CHAN_VOICE, "brain/pain", 1, ATTN_NONE);
}

static void BrainishExplosion (fixed_t x, fixed_t y, fixed_t z)
{
	AActor *boom = Spawn("Rocket", x, y, z, NO_REPLACE);
	if (boom != NULL)
	{
		boom->DeathSound = "misc/brainexplode";
		boom->momz = pr_brainscream() << 9;
		boom->SetState (&ABossBrain::States[S_BRAINEXPLODE]);
		boom->effects = 0;
		boom->Damage = 0;	// disables collision detection which is not wanted here
		boom->tics -= pr_brainscream() & 7;
		if (boom->tics < 1)
			boom->tics = 1;
	}
}

void A_BrainScream (AActor *self)
{
	fixed_t x;
		
	for (x = self->x - 196*FRACUNIT; x < self->x + 320*FRACUNIT; x += 8*FRACUNIT)
	{
		BrainishExplosion (x, self->y - 320*FRACUNIT,
			128 + (pr_brainscream() << (FRACBITS + 1)));
	}
	S_Sound (self, CHAN_VOICE, "brain/death", 1, ATTN_NONE);
}

void A_BrainExplode (AActor *self)
{
	fixed_t x = self->x + pr_brainexplode.Random2()*2048;
	fixed_t z = 128 + pr_brainexplode()*2*FRACUNIT;
	BrainishExplosion (x, self->y, z);
}

void A_BrainDie (AActor *self)
{
	// [RH] If noexit, then don't end the level.
	// [BC] teamgame
	if ((deathmatch || teamgame || alwaysapplydmflags) && (dmflags & DF_NO_EXIT))
		return;

	G_ExitLevel (0, false);
}

void A_BrainSpit (AActor *self)
{
	TThinkerIterator<DBrainState> iterator (STAT_BOSSTARGET);
	DBrainState *state;
	AActor *targ;
	AActor *spit;

	// [BC] Brain spitting is server-side.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return;
	}

	// shoot a cube at current target
	if (NULL == (state = iterator.Next ()))
	{
		state = new DBrainState;
	}
	targ = state->GetTarget ();

	if (targ != NULL)
	{
		const PClass *spawntype = NULL;
		int index = CheckIndex (1, NULL);
		if (index >= 0) spawntype = PClass::FindClass ((ENamedName)StateParameters[index]);
		if (spawntype == NULL) spawntype = RUNTIME_CLASS(ASpawnShot);

		// spawn brain missile
		spit = P_SpawnMissile (self, targ, spawntype);

		if (spit != NULL)
		{
			spit->target = targ;
			spit->master = self;
			// [RH] Do this correctly for any trajectory. Doom would divide by 0
			// if the target had the same y coordinate as the spitter.
			if ((spit->momx | spit->momy) == 0)
			{
				spit->reactiontime = 0;
			}
			else if (abs(spit->momy) > abs(spit->momx))
			{
				spit->reactiontime = (targ->y - self->y) / spit->momy;
			}
			else
			{
				spit->reactiontime = (targ->x - self->x) / spit->momx;
			}
			// [GZ] Calculates when the projectile will have reached destination
			spit->reactiontime += level.maptime;

			// [BC] If we're the server, tell clients to spawn the actor.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SpawnMissile( spit );
		}

		if (index >= 0)
		{
			S_Sound(self, CHAN_WEAPON, self->AttackSound, 1, ATTN_NONE);

			// [BC] If we're the server, tell clients create the sound.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SoundPoint( self->x, self->y, self->z, CHAN_WEAPON, self->AttackSound, 1, ATTN_NONE );
		}
		else
		{
			// compatibility fallback
			S_Sound (self, CHAN_WEAPON, "brain/spit", 1, ATTN_NONE);

			// [BC] If we're the server, tell clients create the sound.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SoundPoint( self->x, self->y, self->z, CHAN_WEAPON, "brain/spit", 1, ATTN_NONE );
		}
	}
}

void A_SpawnFly (AActor *self)
{
	AActor *newmobj;
	AActor *fog = NULL;
	AActor *targ;
	int r;
		
	// [BC] Brain spitting is server-side.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return;
	}

	// [GZ] Should be more fiable than a countdown...
	if (self->reactiontime > level.maptime)
		return; // still flying
		
	targ = self->target;


	const PClass *spawntype = NULL;
	int index = CheckIndex (1, NULL);
		// First spawn teleport fire.
	if (index >= 0) 
	{
		spawntype = PClass::FindClass ((ENamedName)StateParameters[index]);
		if (spawntype != NULL) 
		{
			fog = Spawn (spawntype, targ->x, targ->y, targ->z, ALLOW_REPLACE);
			if (fog != NULL) S_Sound (fog, CHAN_BODY, fog->SeeSound, 1, ATTN_NORM);
		}
	}
	else
	{
		fog = Spawn<ASpawnFire> (targ->x, targ->y, targ->z, ALLOW_REPLACE);
		if (fog != NULL) S_Sound (fog, CHAN_BODY, "brain/spawn", 1, ATTN_NORM);
	}

	// [BC] If we're the server, spawn the fire, and tell clients to play the sound.
	if (( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( fog ))
	{
		SERVERCOMMANDS_SpawnThing( fog );
		SERVERCOMMANDS_SoundPoint( fog->x, fog->y, fog->z, CHAN_BODY, "brain/spawn", 1, ATTN_NORM );
	}

	FName SpawnName;

	if (self->master != NULL)
	{
		FDropItem *di;   // di will be our drop item list iterator
		FDropItem *drop; // while drop stays as the reference point.
		int n=0;

		drop = di = GetDropItems(self->master->GetClass());
		if (di != NULL)
		{
			while (di != NULL)
			{
				if (di->Name != NAME_None)
				{
					if (di->amount < 0) di->amount = 1; // default value is -1, we need a positive value.
					n += di->amount; // this is how we can weight the list.
					di = di->Next;
				}
			}
			di = drop;
			n = pr_spawnfly(n);
			while (n > 0)
			{
				if (di->Name != NAME_None)
				{
					n -= di->amount; // logically, none of the -1 values have survived by now.
					if (n > -1) di = di->Next; // If we get into the negatives, we've reached the end of the list.
				}
			}

			SpawnName = di->Name;
		}
	}
	if (SpawnName == NAME_None)
	{
		const char *type;
		// Randomly select monster to spawn.
		r = pr_spawnfly ();

		// Probability distribution (kind of :),
		// decreasing likelihood.
			 if (r < 50)  type = "DoomImp";
		else if (r < 90)  type = "Demon";
		else if (r < 120) type = "Spectre";
		else if (r < 130) type = "PainElemental";
		else if (r < 160) type = "Cacodemon";
		else if (r < 162) type = "Archvile";
		else if (r < 172) type = "Revenant";
		else if (r < 192) type = "Arachnotron";
		else if (r < 222) type = "Fatso";
		else if (r < 246) type = "HellKnight";
		else			  type = "BaronOfHell";

		SpawnName = type;
	}
	spawntype = PClass::FindClass(SpawnName);
	if (spawntype != NULL)
	{
		newmobj = Spawn (spawntype, targ->x, targ->y, targ->z, ALLOW_REPLACE);
		if (newmobj != NULL)
		{
			// Make the new monster hate what the boss eye hates
			AActor *eye = self->target;

			if (eye != NULL)
			{
				newmobj->CopyFriendliness (eye, false);
			}
			if (newmobj->SeeState != NULL && P_LookForPlayers (newmobj, true))
				newmobj->SetState (newmobj->SeeState);

			if (!(newmobj->ObjectFlags & OF_EuthanizeMe))
			{
				// telefrag anything in this spot
				P_TeleportMove (newmobj, newmobj->x, newmobj->y, newmobj->z, true);

				// [BC] If we're the server, tell clients to spawn the new monster.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				{
					SERVERCOMMANDS_SpawnThing( newmobj );
					if ( newmobj->state == newmobj->SeeState )
						SERVERCOMMANDS_SetThingState( newmobj, STATE_SEE );
				}
			}
		}
	}

	// [BC] Tell clients to destroy the cube.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_DestroyThing( self );

	// remove self (i.e., cube).
	self->Destroy ();
}

// travelling cube sound
void A_SpawnSound (AActor *self)	
{
	S_Sound (self, CHAN_BODY, "brain/cube", 1, ATTN_IDLE);
	A_SpawnFly (self);
}

// Each brain on the level shares a single global state
IMPLEMENT_CLASS (DBrainState)

ABossTarget *DBrainState::GetTarget ()
{
	Easy = !Easy;

	if (G_SkillProperty(SKILLP_EasyBossBrain) && !Easy)
		return NULL;

	ABossTarget *target;

	if (SerialTarget)
	{
		do
		{
			target = Targets.Next ();
		} while (target != NULL && target != SerialTarget);
		SerialTarget = NULL;
	}
	else
	{
		target = Targets.Next ();
	}
	return (target == NULL) ? Targets.Next () : target;
}

void DBrainState::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << Easy;
	if (arc.IsStoring ())
	{
		ABossTarget *target = Targets.Next ();
		arc << target;
	}
	else
	{
		arc << SerialTarget;
	}
}

