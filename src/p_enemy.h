#ifndef __P_ENEMY_H__
#define __P_ENEMY_H__

#include "thingdef/thingdef.h"

struct sector_t;
class AActor;
class AInventory;
struct PClass;


enum dirtype_t
{
	DI_EAST,
	DI_NORTHEAST,
	DI_NORTH,
	DI_NORTHWEST,
	DI_WEST,
	DI_SOUTHWEST,
	DI_SOUTH,
	DI_SOUTHEAST,
	DI_NODIR,
	NUMDIRS
};

extern fixed_t xspeed[8], yspeed[8];

enum LO_Flags
{
	LOF_NOSIGHTCHECK = 1,
	LOF_NOSOUNDCHECK = 2,
	LOF_DONTCHASEGOAL = 4,
	LOF_NOSEESOUND = 8,
	LOF_FULLVOLSEESOUND = 16,
};

struct FLookExParams
{
	angle_t fov;
	fixed_t mindist;
	fixed_t maxdist;
	fixed_t maxheardist;
	int flags;
	FState *seestate;
};

void P_RecursiveSound (sector_t *sec, AActor *soundtarget, bool splash, int soundblocks);
bool P_HitFriend (AActor *self);
void P_NoiseAlert (AActor *target, AActor *emmiter, bool splash=false);
bool P_CheckMeleeRange2 (AActor *actor);
bool P_Move (AActor *actor);
bool P_TryWalk (AActor *actor);
void P_NewChaseDir (AActor *actor);
AInventory *P_DropItem (AActor *source, const PClass *type, int special, int chance);
void P_TossItem (AActor *item);
bool P_LookForPlayers (AActor *actor, INTBOOL allaround, FLookExParams *params);
void A_Weave(AActor *self, int xyspeed, int zspeed, fixed_t xydist, fixed_t zdist);

DECLARE_ACTION(A_Look)
DECLARE_ACTION(A_Wander)
DECLARE_ACTION(A_BossDeath)
DECLARE_ACTION(A_Pain)
DECLARE_ACTION(A_MonsterRail)
DECLARE_ACTION(A_NoBlocking)
DECLARE_ACTION(A_Scream)
DECLARE_ACTION(A_FreezeDeath)
DECLARE_ACTION(A_FreezeDeathChunks)
DECLARE_ACTION(A_BossDeath)

void A_Chase(AActor *self);
void A_FaceTarget (AActor *actor);

bool A_RaiseMobj (AActor *, fixed_t speed);
bool A_SinkMobj (AActor *, fixed_t speed);

bool CheckBossDeath (AActor *);
int P_Massacre ();
bool P_CheckMissileRange (AActor *actor);

#define SKULLSPEED (20*FRACUNIT)
void A_SkullAttack(AActor *self, fixed_t speed);

#endif //__P_ENEMY_H__
