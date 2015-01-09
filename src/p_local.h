// Emacs style mode select	 -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// DESCRIPTION:
//		Play functions, animation, global header.
//
//-----------------------------------------------------------------------------


#ifndef __P_LOCAL__
#define __P_LOCAL__

#ifndef __R_LOCAL__
#include "r_local.h"
#endif

#include "a_morph.h"

#include <stdlib.h>

#define STEEPSLOPE		46342	// [RH] Minimum floorplane.c value for walking

#define BONUSADD		6

// mapblocks are used to check movement
// against lines and things
#define MAPBLOCKUNITS	128
#define MAPBLOCKSIZE	(MAPBLOCKUNITS*FRACUNIT)
#define MAPBLOCKSHIFT	(FRACBITS+7)
#define MAPBMASK		(MAPBLOCKSIZE-1)
#define MAPBTOFRAC		(MAPBLOCKSHIFT-FRACBITS)


// MAXRADIUS is for precalculated sector block boxes
// the spider demon is larger,
// but we do not have any moving sectors nearby
#define MAXRADIUS		0/*32*FRACUNIT*/

//#define GRAVITY 		FRACUNIT
#define MAXMOVE 		(30*FRACUNIT)

#define TALKRANGE		(128*FRACUNIT)
#define USERANGE		(64*FRACUNIT)
#define MELEERANGE		(64*FRACUNIT)
#define MISSILERANGE	(32*64*FRACUNIT)
#define PLAYERMISSILERANGE	(8192*FRACUNIT)	// [RH] New MISSILERANGE for players

// follow a player exlusively for 3 seconds
#define BASETHRESHOLD	100


//
// P_PSPR
//
void P_SetupPsprites (player_t* curplayer);
void P_MovePsprites (player_t* curplayer);
void P_DropWeapon (player_t* player);


//
// P_USER
//
void	P_FallingDamage (AActor *ent);
bool	PLAYER_Responder( event_t *pEvent );
void	P_PlayerThink (player_t *player, ticcmd_t *pCmd = NULL );
/*
void	P_PredictPlayer (player_t *player);
void	P_UnPredictPlayer ();
*/

//
// P_MOBJ
//

#define ONFLOORZ		FIXED_MIN
#define ONCEILINGZ		FIXED_MAX
#define FLOATRANDZ		(FIXED_MAX-1)

extern fixed_t FloatBobOffsets[64];

struct FMapThing;

APlayerPawn *P_SpawnPlayer (FMapThing* mthing, bool bClientUpdate, player_t *p, bool tempplayer=false);


void P_ThrustMobj (AActor *mo, angle_t angle, fixed_t move);
int P_FaceMobj (AActor *source, AActor *target, angle_t *delta);
bool P_SeekerMissile (AActor *actor, angle_t thresh, angle_t turnMax, bool precise = false);

enum EPuffFlags
{
	PF_HITTHING = 1,
	PF_MELEERANGE = 2,
	PF_TEMPORARY = 4
};

// [BC] Added bTellClientToSpawn.
AActor *P_SpawnPuff (AActor *source, const PClass *pufftype, fixed_t x, fixed_t y, fixed_t z, angle_t dir, int updown, int flags = 0, bool bTellClientToSpawn = true);
void	P_SpawnBlood (fixed_t x, fixed_t y, fixed_t z, angle_t dir, int damage, AActor *originator);
void	P_BloodSplatter (fixed_t x, fixed_t y, fixed_t z, AActor *originator);
void	P_BloodSplatter2 (fixed_t x, fixed_t y, fixed_t z, AActor *originator);
void	P_RipperBlood (AActor *mo, AActor *bleeder);
int		P_GetThingFloorType (AActor *thing);
void	P_ExplodeMissile (AActor *missile, line_t *explodeline, AActor *target);

AActor *P_SpawnMissile (AActor* source, AActor* dest, const PClass *type, AActor* owner = NULL);
AActor *P_SpawnMissileZ (AActor* source, fixed_t z, AActor* dest, const PClass *type);
AActor *P_SpawnMissileXYZ (fixed_t x, fixed_t y, fixed_t z, AActor *source, AActor *dest, const PClass *type, bool checkspawn = true, AActor *owner = NULL);
AActor *P_SpawnMissileAngle (AActor *source, const PClass *type, angle_t angle, fixed_t velz);
AActor *P_SpawnMissileAngleSpeed (AActor *source, const PClass *type, angle_t angle, fixed_t velz, fixed_t speed);
AActor *P_SpawnMissileAngleZ (AActor *source, fixed_t z, const PClass *type, angle_t angle, fixed_t velz);
AActor *P_SpawnMissileAngleZSpeed (AActor *source, fixed_t z, const PClass *type, angle_t angle, fixed_t velz, fixed_t speed, AActor *owner=NULL, bool checkspawn = true);
AActor *P_SpawnMissileZAimed (AActor *source, fixed_t z, AActor *dest, const PClass *type);

AActor *P_SpawnPlayerMissile (AActor* source, const PClass *type);
AActor *P_SpawnPlayerMissile (AActor *source, const PClass *type, angle_t angle, bool bSpawnSound = true );
AActor *P_SpawnPlayerMissile (AActor *source, fixed_t x, fixed_t y, fixed_t z, const PClass *type, angle_t angle, 
							  AActor **pLineTarget = NULL, AActor **MissileActor = NULL, bool nofreeaim = false, bool bSpawnSound = true, bool bSpawnOnClient = true);

void P_CheckFakeFloorTriggers (AActor *mo, fixed_t oldz, bool oldz_has_viewheight=false);

//
// [RH] P_THINGS
//
#define MAX_SPAWNABLES	(256)
extern const PClass *SpawnableThings[MAX_SPAWNABLES];

bool	P_Thing_Spawn (int tid, AActor *source, int type, angle_t angle, bool fog, int newtid);
bool	P_Thing_Projectile (int tid, AActor *source, int type, const char * type_name, angle_t angle,
			fixed_t speed, fixed_t vspeed, int dest, AActor *forcedest, int gravity, int newtid,
			bool leadTarget);
bool	P_MoveThing(AActor *source, fixed_t x, fixed_t y, fixed_t z, bool fog);
bool	P_Thing_Move (int tid, AActor *source, int mapspot, bool fog);
int		P_Thing_Damage (int tid, AActor *whofor0, int amount, FName type);
void	P_Thing_SetVelocity(AActor *actor, fixed_t vx, fixed_t vy, fixed_t vz, bool add, bool setbob);
void P_RemoveThing(AActor * actor);
bool P_Thing_Raise(AActor *thing, bool bIgnorePositionCheck = false); // [BB] Added bIgnorePositionCheck.

//
// P_ENEMY
//
void	P_NoiseAlert (AActor* target, AActor* emmiter, bool splash);


//
// P_MAPUTL
//
struct divline_t
{
	fixed_t 	x;
	fixed_t 	y;
	fixed_t 	dx;
	fixed_t 	dy;
	
};

struct intercept_t
{
	fixed_t 	frac;			// along trace line
	bool	 	isaline;
	bool		done;
	union {
		AActor *thing;
		line_t *line;
	} d;
};

typedef bool (*traverser_t) (intercept_t *in);

fixed_t P_AproxDistance (fixed_t dx, fixed_t dy);

//==========================================================================
//
// P_PointOnLineSide
//
// Returns 0 (front/on) or 1 (back)
// [RH] inlined, stripped down, and made more precise
//
//==========================================================================

inline int P_PointOnLineSide (fixed_t x, fixed_t y, const line_t *line)
{
	return DMulScale32 (y-line->v1->y, line->dx, line->v1->x-x, line->dy) > 0;
}

//==========================================================================
//
// P_PointOnDivlineSide
//
// Same as P_PointOnLineSide except it uses divlines
// [RH] inlined, stripped down, and made more precise
//
//==========================================================================

inline int P_PointOnDivlineSide (fixed_t x, fixed_t y, const divline_t *line)
{
	return DMulScale32 (y-line->y, line->dx, line->x-x, line->dy) > 0;
}

//==========================================================================
//
// P_MakeDivline
//
//==========================================================================

inline void P_MakeDivline (const line_t *li, divline_t *dl)
{
	dl->x = li->v1->x;
	dl->y = li->v1->y;
	dl->dx = li->dx;
	dl->dy = li->dy;
}

fixed_t P_InterceptVector (const divline_t *v2, const divline_t *v1);

struct FLineOpening
{
	fixed_t			top;
	fixed_t			bottom;
	fixed_t			range;
	fixed_t			lowfloor;
	sector_t		*bottomsec;
	sector_t		*topsec;
	FTextureID		ceilingpic;
	FTextureID		floorpic;
	bool			touchmidtex;
};

void	P_LineOpening (FLineOpening &open, AActor *thing, const line_t *linedef, fixed_t x, fixed_t y, fixed_t refx=FIXED_MIN, fixed_t refy=0);
//void	P_OldLineOpening (const line_t *linedef, fixed_t x, fixed_t y);

class FBoundingBox;
struct polyblock_t;

class FBlockLinesIterator
{
	int minx, maxx;
	int miny, maxy;

	int curx, cury;
	polyblock_t *polyLink;
	int polyIndex;
	int *list;

	void StartBlock(int x, int y);

public:
	FBlockLinesIterator(int minx, int miny, int maxx, int maxy, bool keepvalidcount = false);
	FBlockLinesIterator(const FBoundingBox &box);
	line_t *Next();
	void Reset() { StartBlock(minx, miny); }
};

class FBlockThingsIterator
{
	int minx, maxx;
	int miny, maxy;

	int curx, cury;

	FBlockNode *block;

	int Buckets[32];

	struct HashEntry
	{
		AActor *Actor;
		int Next;
	};
	HashEntry FixedHash[10];
	int NumFixedHash;
	TArray<HashEntry> DynHash;

	HashEntry *GetHashEntry(int i) { return i < (int)countof(FixedHash) ? &FixedHash[i] : &DynHash[i - countof(FixedHash)]; }

	void StartBlock(int x, int y);
	void SwitchBlock(int x, int y);
	void ClearHash();

	// The following is only for use in the path traverser 
	// and therefore declared private.
	FBlockThingsIterator();

	friend class FPathTraverse;

public:
	FBlockThingsIterator(int minx, int miny, int maxx, int maxy);
	FBlockThingsIterator(const FBoundingBox &box);
	AActor *Next(bool centeronly = false);
	void Reset() { StartBlock(minx, miny); }
};

class FPathTraverse
{
	static TArray<intercept_t> intercepts;

	divline_t trace;
	unsigned int intercept_index;
	unsigned int intercept_count;
	fixed_t maxfrac;
	unsigned int count;

	void AddLineIntercepts(int bx, int by);
	void AddThingIntercepts(int bx, int by, FBlockThingsIterator &it, bool compatible);
public:

	intercept_t *Next();

	FPathTraverse(fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2, int flags);
	~FPathTraverse();
	const divline_t &Trace() const { return trace; }
};


#define PT_ADDLINES 	1
#define PT_ADDTHINGS	2
#define PT_COMPATIBLE	4

AActor *P_BlockmapSearch (AActor *mo, int distance, AActor *(*check)(AActor*, int, void *), void *params = NULL);
AActor *P_RoughMonsterSearch (AActor *mo, int distance);

//
// P_MAP
//

struct FCheckPosition
{
	// in
	AActor			*thing;
	fixed_t			x;
	fixed_t			y;
	fixed_t			z;

	// out
	sector_t		*sector;
	fixed_t			floorz;
	fixed_t			ceilingz;
	fixed_t			dropoffz;
	FTextureID		floorpic;
	sector_t		*floorsector;
	FTextureID		ceilingpic;
	sector_t		*ceilingsector;
	bool			touchmidtex;
	bool			floatok;
	bool			FromPMove;
	line_t			*ceilingline;
	AActor			*stepthing;
	// [RH] These are used by PIT_CheckThing and P_XYMovement to apply
	// ripping damage once per tic instead of once per move.
	bool			DoRipping;
	AActor			*LastRipped;
	int				PushTime;

	FCheckPosition(bool rip=false)
	{
		DoRipping = rip;
		LastRipped = NULL;
		PushTime = 0;
		FromPMove = false;
	}
};



// If "floatok" true, move would be ok
// if within "tmfloorz - tmceilingz".
extern msecnode_t		*sector_list;		// phares 3/16/98

extern TArray<line_t *> spechit;


bool	P_TestMobjLocation (AActor *mobj);
bool	P_TestMobjZ (AActor *mobj, bool quick=true, AActor **pOnmobj = NULL);
bool	P_CheckPosition (AActor *thing, fixed_t x, fixed_t y, FCheckPosition &tm);
bool	P_CheckPosition (AActor *thing, fixed_t x, fixed_t y);
//bool	P_OldCheckPosition (AActor *thing, fixed_t x, fixed_t y);
AActor	*P_CheckOnmobj (AActor *thing);
void	P_FakeZMovement (AActor *mo);
bool	P_TryMove (AActor* thing, fixed_t x, fixed_t y, int dropoff, const secplane_t * onfloor, FCheckPosition &tm);
bool	P_TryMove (AActor* thing, fixed_t x, fixed_t y, int dropoff, const secplane_t * onfloor = NULL);
bool	P_OldTryMove (AActor* thing, fixed_t x, fixed_t y, bool dropoff, bool onfloor = false);
bool	P_CheckMove(AActor *thing, fixed_t x, fixed_t y);
void	P_ApplyTorque(AActor *mo);
bool	P_TeleportMove (AActor* thing, fixed_t x, fixed_t y, fixed_t z, bool telefrag);	// [RH] Added z and telefrag parameters
void	P_PlayerStartStomp (AActor *actor);		// [RH] Stomp on things for a newly spawned player
void	P_SlideMove (AActor* mo, fixed_t tryx, fixed_t tryy, int numsteps);
void	P_OldSlideMove (AActor* mo);
bool	P_BounceWall (AActor *mo);
bool	P_BounceActor (AActor *mo, AActor * BlockingMobj);
bool	P_CheckSight (const AActor* t1, const AActor* t2, int flags=0);

enum ESightFlags
{
	SF_IGNOREVISIBILITY=1,
	SF_SEEPASTSHOOTABLELINES=2,
	SF_SEEPASTBLOCKEVERYTHING=4,
	SF_IGNOREWATERBOUNDARY=8
};

void	P_ResetSightCounters (bool full);
void	P_ResetSpawnCounters( void ); // [BC]
bool	P_TalkFacing (AActor *player);
void	P_UseLines (player_t* player);
//void	P_UseItems (player_t* player);
bool	P_UsePuzzleItem (AActor *actor, int itemType);
void	P_FindFloorCeiling (AActor *actor, bool onlymidtex = false);

bool	P_ChangeSector (sector_t* sector, int crunch, int amt, int floorOrCeil, bool isreset);

fixed_t P_AimLineAttack (AActor *t1, angle_t angle, fixed_t distance, AActor **pLineTarget = NULL, fixed_t vrange=0, int flags = 0, AActor *target=NULL);

enum
{
	ALF_FORCENOSMART = 1,
	ALF_CHECK3D = 2,
	ALF_CHECKNONSHOOTABLE = 4,
	ALF_CHECKCONVERSATION = 8,
};

AActor *P_LineAttack (AActor *t1, angle_t angle, fixed_t distance, int pitch, int damage, FName damageType, const PClass *pufftype, bool ismelee = false, AActor **victim = NULL);
AActor *P_LineAttack (AActor *t1, angle_t angle, fixed_t distance, int pitch, int damage, FName damageType, FName pufftype, bool ismelee = false, AActor **victim = NULL);
void	P_TraceBleed (int damage, fixed_t x, fixed_t y, fixed_t z, AActor *target, angle_t angle, int pitch);
void	P_TraceBleed (int damage, AActor *target, angle_t angle, int pitch);
void	P_TraceBleed (int damage, AActor *target, AActor *missile);		// missile version
void	P_TraceBleed (int damage, AActor *target);		// random direction version
void	P_RailAttack (AActor *source, int damage, int offset, int color1 = 0, int color2 = 0, float maxdiff = 0, bool silent = false, const PClass *puff = NULL, bool pierce = true, angle_t angleoffset = 0, angle_t pitchoffset = 0);	// [RH] Shoot a railgun
void	P_RailAttackWithPossibleSpread(AActor *source, int damage, int offset, int color1 = 0, int color2 = 0, float maxdiff = 0, bool silent = false, const PClass *puff = NULL, bool pierce = true, angle_t angleoffset = 0, angle_t pitchoffset = 0);	// [BB] Shoot a railgun with spread applied if necessary
bool	P_HitFloor (AActor *thing);
bool	P_HitWater (AActor *thing, sector_t *sec, fixed_t splashx = FIXED_MIN, fixed_t splashy = FIXED_MIN, fixed_t splashz=FIXED_MIN, bool checkabove = false, bool alert = true);
void	P_CheckSplash(AActor *self, fixed_t distance);
bool	P_CheckMissileSpawn (AActor *missile, bool bExplode = true);
void	P_PlaySpawnSound(AActor *missile, AActor *spawner);

// [RH] Position the chasecam
void	P_AimCamera (AActor *t1, fixed_t &x, fixed_t &y, fixed_t &z, sector_t *&sec);

// [RH] Means of death
void	P_RadiusAttack (AActor *spot, AActor *source, int damage, int distance, 
						FName damageType, bool hurtSelf, bool dodamage=true, int fulldamagedistance=0);

void	P_DelSector_List();
void	P_DelSeclist(msecnode_t *);							// phares 3/16/98
void	P_CreateSecNodeList(AActor*,fixed_t,fixed_t);		// phares 3/14/98
int		P_GetMoveFactor(const AActor *mo, int *frictionp);	// phares  3/6/98
int		P_GetFriction(const AActor *mo, int *frictionfactor);
bool	Check_Sides(AActor *, int, int);					// phares

// [RH] 
const secplane_t * P_CheckSlopeWalk (AActor *actor, fixed_t &xmove, fixed_t &ymove);

// [TP]
bool P_CheckUnblock ( AActor *pActor1, AActor *pActor2 );

//----------------------------------------------------------------------------------
//
// Added so that in the source there's a clear distinction between
// game engine and renderer specific calls.
// (For ZDoom itself this doesn't make any difference here but for GZDoom it does.)
//
//----------------------------------------------------------------------------------
subsector_t *P_PointInSubsector (fixed_t x, fixed_t y);
inline sector_t *P_PointInSector(fixed_t x, fixed_t y)
{
	return P_PointInSubsector(x,y)->sector;
}

//
// P_SETUP
//
extern BYTE*			rejectmatrix;	// for fast sight rejection
extern int*				blockmaplump;	// offsets in blockmap are from here
extern int*				blockmap;
extern int				bmapwidth;
extern int				bmapheight; 	// in mapblocks
extern fixed_t			bmaporgx;
extern fixed_t			bmaporgy;		// origin of block map
extern FBlockNode**		blocklinks; 	// for thing chains



//
// P_INTER
//
void P_TouchSpecialThing (AActor *special, AActor *toucher);
void P_DamageMobj (AActor *target, AActor *inflictor, AActor *source, int damage, FName mod, int flags=0);
void P_PoisonMobj (AActor *target, AActor *inflictor, AActor *source, int damage, int duration, int period);
bool P_GiveBody (AActor *actor, int num);
bool P_PoisonPlayer (player_t *player, AActor *poisoner, AActor *source, int poison);
void P_PoisonDamage (player_t *player, AActor *source, int damage, bool playPainSound);

enum EDmgFlags
{
	DMG_NO_ARMOR = 1,
	DMG_INFLICTOR_IS_PUFF = 2,
	DMG_THRUSTLESS = 4,
	DMG_FORCED = 8,
	DMG_NO_FACTOR = 16,
};


// ===== PO_MAN =====

typedef enum
{
	PODOOR_NONE,
	PODOOR_SLIDE,
	PODOOR_SWING,
} podoortype_t;

bool EV_RotatePoly (line_t *line, int polyNum, int speed, int byteAngle, int direction, bool overRide);
bool EV_MovePoly (line_t *line, int polyNum, int speed, angle_t angle, fixed_t dist, bool overRide);
bool EV_MovePolyTo (line_t *line, int polyNum, int speed, fixed_t x, fixed_t y, bool overRide);
bool EV_OpenPolyDoor (line_t *line, int polyNum, int speed, angle_t angle, int delay, int distance, podoortype_t type);
bool EV_StopPoly (int polyNum);


// [RH] Data structure for P_SpawnMapThing() to keep track
//		of polyobject-related things.
struct polyspawns_t
{
	polyspawns_t *next;
	fixed_t x;
	fixed_t y;
	short angle;
	short type;
};

enum
{
	PO_HEX_ANCHOR_TYPE = 3000,
	PO_HEX_SPAWN_TYPE,
	PO_HEX_SPAWNCRUSH_TYPE,

	// [RH] Thing numbers that don't conflict with Doom things
	PO_ANCHOR_TYPE = 9300,
	PO_SPAWN_TYPE,
	PO_SPAWNCRUSH_TYPE,
	PO_SPAWNHURT_TYPE
};

extern int po_NumPolyobjs;
extern polyspawns_t *polyspawns;	// [RH] list of polyobject things to spawn


void PO_Init ();
bool PO_Busy (int polyobj);
FPolyObj *PO_GetPolyobj(int polyNum);

//
// P_SPEC
//
#include "p_spec.h"

// [BB] Moved here from po_man.cpp
class DPolyAction : public DThinker
{
	DECLARE_CLASS (DPolyAction, DThinker)
	HAS_OBJECT_POINTERS
public:
	DPolyAction (int polyNum);
	void Serialize (FArchive &arc);
	void Destroy();
	void Stop();
	int GetSpeed() const { return m_Speed; }

	void StopInterpolation ();

	void	SetSpeed( LONG lSpeed );

	LONG	GetDist( void );
	void	SetDist( LONG lDist );

	LONG	GetPolyObj( void );

	virtual void UpdateToClient( ULONG ulClient ); // [WS] We need this here.
protected:
	DPolyAction ();
	int m_PolyObj;
	int m_Speed;
	int m_Dist;
	TObjPtr<DInterpolation> m_Interpolation;

	void SetInterpolation ();

	friend void ThrustMobj (AActor *actor, seg_t *seg, FPolyObj *po);
};

class DRotatePoly : public DPolyAction
{
	DECLARE_CLASS (DRotatePoly, DPolyAction)
public:
	DRotatePoly (int polyNum);
	void Tick ();
	void UpdateToClient( ULONG ulClient );

private:
	DRotatePoly ();

	friend bool EV_RotatePoly (line_t *line, int polyNum, int speed, int byteAngle, int direction, bool overRide);
};


class DMovePoly : public DPolyAction
{
	DECLARE_CLASS (DMovePoly, DPolyAction)
public:
	DMovePoly (int polyNum);
	void Serialize (FArchive &arc);
	void Tick ();
	virtual void UpdateToClient( ULONG ulClient ); // [WS] This needs to be virtual.

	LONG	GetAngle( void );
	void	SetAngle( LONG lAngle );

	LONG	GetXSpeed( void );
	void	SetXSpeed( LONG lSpeed );

	LONG	GetYSpeed( void );
	void	SetYSpeed( LONG lSpeed );
protected:
	DMovePoly ();
	int m_Angle;
	fixed_t m_xSpeed; // for sliding walls
	fixed_t m_ySpeed;

	friend bool EV_MovePoly (line_t *line, int polyNum, int speed, angle_t angle, fixed_t dist, bool overRide);
};

class DMovePolyTo : public DPolyAction
{
	DECLARE_CLASS(DMovePolyTo, DPolyAction)
public:
	DMovePolyTo(int polyNum);
	void Serialize(FArchive &arc);
	void Tick();
protected:
	DMovePolyTo();
	fixed_t m_xSpeed;
	fixed_t m_ySpeed;
	fixed_t m_xTarget;
	fixed_t m_yTarget;

	friend bool EV_MovePolyTo(line_t *line, int polyNum, int speed, int x, int y, bool overRide);
};


class DPolyDoor : public DMovePoly
{
	DECLARE_CLASS (DPolyDoor, DMovePoly)
public:
	DPolyDoor (int polyNum, podoortype_t type);
	void Serialize (FArchive &arc);
	void Tick ();
	void UpdateToClient( ULONG ulClient );

	LONG	GetDirection( void );
	void	SetDirection( LONG lDirection );

	LONG	GetTotalDist( void );
	void	SetTotalDist( LONG lDist );

	bool	GetClose( void );
	void	SetClose( bool bClose );

protected:
	int m_Direction;
	int m_TotalDist;
	int m_Tics;
	int m_WaitTics;
	podoortype_t m_Type;
	bool m_Close;

	friend bool EV_OpenPolyDoor (line_t *line, int polyNum, int speed, angle_t angle, int delay, int distance, podoortype_t type);
private:
	DPolyDoor ();
};

#endif	// __P_LOCAL__
