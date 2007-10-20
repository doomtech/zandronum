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
// $Log:$
//
// DESCRIPTION:
//		Movement, collision handling.
//		Shooting and aiming.
//
//-----------------------------------------------------------------------------

#include <stdlib.h>
#include <math.h>

#include "templates.h"
#include "vectors.h"

#include "m_alloc.h"
#include "m_bbox.h"
#include "m_random.h"
#include "i_system.h"

#include "doomdef.h"
#include "p_local.h"
#include "p_lnspec.h"
#include "p_effect.h"
#include "p_terrain.h"
#include "p_trace.h"

#include "s_sound.h"
#include "decallib.h"

// State.
#include "doomstat.h"
#include "r_state.h"

#include "gi.h"

#include "a_sharedglobal.h"
#include "a_doomglobal.h"
#include "p_conversation.h"
#include "deathmatch.h"
#include "team.h"
#include "network.h"
#include "g_game.h"
#include "cooperative.h"
#include "sv_commands.h"
#include "cl_demo.h"
#include "cl_main.h"
#include "gamemode.h"

#define WATER_SINK_FACTOR		3
#define WATER_SINK_SMALL_FACTOR	4
#define WATER_SINK_SPEED		(FRACUNIT/2)
#define WATER_JUMP_SPEED		(FRACUNIT*7/2)

#define USE_PUZZLE_ITEM_SPECIAL 129


CVAR (Bool, cl_bloodsplats, true, CVAR_ARCHIVE)
CVAR (Int, sv_smartaim, 0, CVAR_ARCHIVE|CVAR_SERVERINFO)

static void CheckForPushSpecial (line_t *line, int side, AActor *mobj);
static void SpawnShootDecal (AActor *t1, const FTraceResults &trace);
static void SpawnDeepSplash (AActor *t1, const FTraceResults &trace, AActor *puff,
							 fixed_t vx, fixed_t vy, fixed_t vz);

fixed_t 		tmbbox[4];
AActor		   *tmthing;
static int 		tmflags;
static fixed_t	tmx;
static fixed_t	tmy;
static fixed_t	tmz;	// [RH] Needed for third dimension of teleporters
static fixed_t	pe_x;	// Pain Elemental position for Lost Soul checks	// phares
static fixed_t	pe_y;	// Pain Elemental position for Lost Soul checks	// phares
static fixed_t	ls_x;	// Lost Soul position for Lost Soul checks		// phares
static fixed_t	ls_y;	// Lost Soul position for Lost Soul checks		// phares

static FRandom pr_tracebleed ("TraceBleed");
static FRandom pr_checkthing ("CheckThing");
static FRandom pr_lineattack ("LineAttack");
static FRandom pr_crunch ("DoCrunch");

// If "floatok" true, move would be ok
// if within "tmfloorz - tmceilingz".
bool 			floatok;

fixed_t 		tmfloorz;
fixed_t 		tmceilingz;
fixed_t 		tmdropoffz;
int				tmfloorpic;
sector_t		*tmfloorsector;
int				tmceilingpic;
sector_t		*tmceilingsector;

static fixed_t	tmfbbox[4];
static AActor	*tmfthing;
fixed_t			tmffloorz;
fixed_t			tmfceilingz;
fixed_t			tmfdropoffz;
fixed_t			tmffloorpic;
sector_t		*tmffloorsector;
fixed_t			tmfceilingpic;
sector_t		*tmfceilingsector;

//Added by MC: So bot will know what kind of sector it's entering.
sector_t*		tmsector;

// keep track of the line that lowers the ceiling,
// so missiles don't explode against sky hack walls
line_t* 		ceilingline;

line_t			*BlockingLine;

static int		tmunstuck;     /* killough 8/1/98: whether to allow unsticking */

// keep track of special lines as they are hit,
// but don't process them until the move is proven valid
TArray<line_t *> spechit;

AActor *onmobj; // generic global onmobj...used for landing on pods/players
AActor *BlockingMobj;

// Temporary holder for thing_sectorlist threads
msecnode_t* sector_list = NULL;		// phares 3/16/98

extern sector_t *openbottomsec;
extern sector_t *opentopsec;

bool DoRipping;
AActor *LastRipped;

//==========================================================================
//
// PIT_FindFloorCeiling
//
//==========================================================================

static bool PIT_FindFloorCeiling (line_t *ld)
{
	if (tmfbbox[BOXRIGHT] <= ld->bbox[BOXLEFT]
		|| tmfbbox[BOXLEFT] >= ld->bbox[BOXRIGHT]
		|| tmfbbox[BOXTOP] <= ld->bbox[BOXBOTTOM]
		|| tmfbbox[BOXBOTTOM] >= ld->bbox[BOXTOP] )
		return true;

	if (P_BoxOnLineSide (tmfbbox, ld) != -1)
		return true;

	// A line has been hit
	
	if (!ld->backsector)
	{ // One sided line
		return true;
	}

	fixed_t sx, sy;

	// set openrange, opentop, openbottom
	if ((((ld->frontsector->floorplane.a | ld->frontsector->floorplane.b) |
		 (ld->backsector->floorplane.a | ld->backsector->floorplane.b) |
		 (ld->frontsector->ceilingplane.a | ld->frontsector->ceilingplane.b) |
		 (ld->backsector->ceilingplane.a | ld->backsector->ceilingplane.b)) == 0)
		 && ld->backsector->e->ffloors.Size()==0 && ld->frontsector->e->ffloors.Size()==0)
	{
		P_LineOpening (tmfthing, ld, sx=tmx, sy=tmy, tmx, tmy);
	}
	else
	{ // Find the point on the line closest to the actor's center, and use
	  // that to calculate openings
		float dx = (float)ld->dx;
		float dy = (float)ld->dy;
		fixed_t r = (fixed_t)(((float)(tmx - ld->v1->x) * dx +
				 			   (float)(tmy - ld->v1->y) * dy) /
							  (dx*dx + dy*dy) * 16777216.f);
		if (r <= 0)
		{
			P_LineOpening (tmfthing, ld, sx=ld->v1->x, sy=ld->v1->y, tmx, tmy);
		}
		else if (r >= (1<<24))
		{
			P_LineOpening (tmfthing, ld, sx=ld->v2->x, sy=ld->v2->y, tmfthing->x, tmfthing->y);
		}
		else
		{
			P_LineOpening (tmfthing, ld, sx=ld->v1->x + MulScale24 (r, ld->dx),
				sy=ld->v1->y + MulScale24 (r, ld->dy), tmx, tmy);
		}
	}

	// adjust floor / ceiling heights
	if (opentop < tmfceilingz)
	{
		tmfceilingz = opentop;
		BlockingLine = ld;
	}

	if (openbottom > tmffloorz)
	{
		tmffloorz = openbottom;
		tmffloorsector = openbottomsec;
		BlockingLine = ld;
	}

	if (lowfloor < tmfdropoffz)
		tmfdropoffz = lowfloor;
	
	return true;
}

//==========================================================================
//
// P_FindFloorCeiling
//
//==========================================================================

void P_FindFloorCeiling (AActor *actor)
{
	int	xl,xh,yl,yh,bx,by;
	fixed_t x, y;
	sector_t *sec;

	x = actor->x;
	y = actor->y;

	tmfbbox[BOXTOP] = y + actor->radius;
	tmfbbox[BOXBOTTOM] = y - actor->radius;
	tmfbbox[BOXRIGHT] = x + actor->radius;
	tmfbbox[BOXLEFT] = x - actor->radius;

	sec = R_PointInSubsector (x, y)->sector;
	tmffloorz = tmfdropoffz = sec->floorplane.ZatPoint (x, y);
	tmfceilingz = sec->ceilingplane.ZatPoint (x, y);
	tmffloorpic = sec->floorpic;
	tmffloorsector = sec;
	tmfceilingpic = sec->ceilingpic;
	tmfceilingsector = sec;
	tmfthing = actor;
	validcount++;

	xl = (tmfbbox[BOXLEFT] - bmaporgx) >> MAPBLOCKSHIFT;
	xh = (tmfbbox[BOXRIGHT] - bmaporgx) >> MAPBLOCKSHIFT;
	yl = (tmfbbox[BOXBOTTOM] - bmaporgy) >> MAPBLOCKSHIFT;
	yh = (tmfbbox[BOXTOP] - bmaporgy) >> MAPBLOCKSHIFT;

	for (bx = xl; bx <= xh; bx++)
		for (by = yl; by <= yh; by++)
			if (!P_BlockLinesIterator (bx, by, PIT_FindFloorCeiling))
				return;
}

//
// TELEPORT MOVE
// 

//
// PIT_StompThing
//
static bool StompAlwaysFrags;

bool PIT_StompThing (AActor *thing)
{
	fixed_t blockdist;

	// [BC] Don't allow spectators to telefrag/be telefragged.
	if ((( thing->player ) && ( thing->player->bSpectating )) || (( tmthing->player ) && ( tmthing->player->bSpectating )))
		return ( true );

	if (!(thing->flags & MF_SHOOTABLE))
		return true;

	// don't clip against self
	if (thing == tmthing)
		return true;

	blockdist = thing->radius + tmthing->radius;
	
	if (abs(thing->x - tmx) >= blockdist || abs(thing->y - tmy) >= blockdist)
	{
		// didn't hit it
		return true;
	}

	// [RH] Z-Check
	// But not if not MF2_PASSMOBJ or MF3_DONTOVERLAP are set!
	// Otherwise those things would get stuck inside each other.
	if ((tmthing->flags2 & MF2_PASSMOBJ || thing->flags4 & MF4_ACTLIKEBRIDGE) && !(i_compatflags & COMPATF_NO_PASSMOBJ))
	{
		if (!(thing->flags3 & tmthing->flags3 & MF3_DONTOVERLAP))
		{
			if (tmz > thing->z + thing->height)
				return true;        // overhead
			if (tmz+tmthing->height < thing->z)
				return true;        // underneath
		}
	}

	// monsters don't stomp things except on boss level
	// [RH] Some Heretic/Hexen monsters can telestomp
	if (StompAlwaysFrags || (tmthing->flags2 & MF2_TELESTOMP))
	{
		// [BC] Damage is never done client-side.
		if (( NETWORK_GetState( ) != NETSTATE_CLIENT ) && ( CLIENTDEMO_IsPlaying( ) == false ))
			P_DamageMobj (thing, tmthing, tmthing, 1000000, NAME_Telefrag);
		return true;
	}
	return false;
}

//
// P_TeleportMove
//
// [RH] Added telefrag parameter: When true, anything in the spawn spot
//		will always be telefragged, and the move will be successful.
//		Added z parameter. Originally, the thing's z was set *after* the
//		move was made, so the height checking I added for 1.13 could
//		potentially erroneously indicate the move was okay if the thing
//		was being teleported between two non-overlapping height ranges.
bool P_TeleportMove (AActor *thing, fixed_t x, fixed_t y, fixed_t z, bool telefrag)
{
	static TArray<AActor *> telebt;

	int 				xl;
	int 				xh;
	int 				yl;
	int 				yh;
	int 				bx;
	int 				by;
	
	subsector_t*		newsubsec;
	
	// kill anything occupying the position
	tmthing = thing;
	tmflags = thing->flags;
		
	tmx = x;
	tmy = y;
	tmz = z;
		
	tmfbbox[BOXTOP] = tmbbox[BOXTOP] = y + tmthing->radius;
	tmfbbox[BOXBOTTOM] = tmbbox[BOXBOTTOM] = y - tmthing->radius;
	tmfbbox[BOXRIGHT] = tmbbox[BOXRIGHT] = x + tmthing->radius;
	tmfbbox[BOXLEFT] = tmbbox[BOXLEFT] = x - tmthing->radius;

	newsubsec = R_PointInSubsector (x,y);
	ceilingline = NULL;
	
	// The base floor/ceiling is from the subsector that contains the point.
	// Any contacted lines the step closer together will adjust them.
	tmffloorz = tmfdropoffz = newsubsec->sector->floorplane.ZatPoint (x, y);
	tmfceilingz = newsubsec->sector->ceilingplane.ZatPoint (x, y);
	tmffloorpic = newsubsec->sector->floorpic;
	tmffloorsector = newsubsec->sector;
	tmfceilingpic = newsubsec->sector->ceilingpic;
	tmfceilingsector = newsubsec->sector;
	tmfthing = tmthing;
					
	validcount++;
	spechit.Clear ();
	telebt.Clear();

	StompAlwaysFrags = tmthing->player || (level.flags & LEVEL_MONSTERSTELEFRAG) || telefrag;

	// stomp on any things contacted
	xl = (tmbbox[BOXLEFT] - bmaporgx - MAXRADIUS)>>MAPBLOCKSHIFT;
	xh = (tmbbox[BOXRIGHT] - bmaporgx + MAXRADIUS)>>MAPBLOCKSHIFT;
	yl = (tmbbox[BOXBOTTOM] - bmaporgy - MAXRADIUS)>>MAPBLOCKSHIFT;
	yh = (tmbbox[BOXTOP] - bmaporgy + MAXRADIUS)>>MAPBLOCKSHIFT;

	for (bx=xl ; bx<=xh ; bx++)
	{
		for (by=yl ; by<=yh ; by++)
		{
			if (!P_BlockLinesIterator(bx,by,PIT_FindFloorCeiling))
			{
				return false;
			}
		}
	}

	fixed_t savefloorz = tmffloorz;
	fixed_t saveceilingz = tmfceilingz;
	sector_t *savesector = tmffloorsector;
	int savepic = tmffloorpic;
	sector_t *savecsector = tmffloorsector;
	int savecpic = tmffloorpic;
	fixed_t savedropoff = tmfdropoffz;

	for (bx=xl ; bx<=xh ; bx++)
	{
		for (by=yl ; by<=yh ; by++)
		{
			if (!P_BlockThingsIterator(bx,by,PIT_StompThing,telebt))
			{
				return false;
			}
		}
	}
	
	// the move is ok, so link the thing into its new position
	thing->SetOrigin (x, y, z);
	thing->floorz = savefloorz;
	thing->ceilingz = saveceilingz;
	thing->floorsector = savesector;
	thing->floorpic = savepic;
	thing->ceilingsector = savecsector;
	thing->ceilingpic = savecpic;
	thing->dropoffz = savedropoff;        // killough 11/98

	if (thing->flags2 & MF2_FLOORCLIP)
	{
		thing->AdjustFloorClip ();
	}

	if (thing == players[consoleplayer].camera)
	{
		R_ResetViewInterpolation ();
	}

	thing->PrevX = x;
	thing->PrevY = y;
	thing->PrevZ = z;

	return true;
}

//
// [RH] P_PlayerStartStomp
//
// Like P_TeleportMove, but it doesn't move anything, and only monsters and other
// players get telefragged.
//
bool PIT_StompThing2 (AActor *thing)
{
	fixed_t blockdist;

	if (!(thing->flags & MF_SHOOTABLE))
		return true;

	// don't clip against self, and don't kill your own voodoo dolls
	if (thing == tmthing ||
		(thing->player == tmthing->player && thing->player != NULL))
		return true;

	// only kill monsters and other players
	if (thing->player == NULL && !(thing->flags3 & MF3_ISMONSTER))
		return true;

	blockdist = thing->radius + tmthing->radius;
	
	if (abs(thing->x - tmthing->x) >= blockdist || abs(thing->y - tmthing->y) >= blockdist)
	{
		// didn't hit it
		return true;
	}

	if (tmthing->z > thing->z + thing->height)
		return true;        // overhead
	if (tmthing->z + tmthing->height < thing->z)
		return true;        // underneath

	P_DamageMobj (thing, tmthing, tmthing, 1000000, NAME_SpawnTelefrag);
	return true;
}

void P_PlayerStartStomp (AActor *actor)
{
	static TArray<AActor *> telebt;

	int 				xl;
	int 				xh;
	int 				yl;
	int 				yh;
	int 				bx;
	int 				by;
	
	tmthing = actor;
	tmflags = actor->flags;
		
	tmbbox[BOXTOP] = actor->y + actor->radius;
	tmbbox[BOXBOTTOM] = actor->y - actor->radius;
	tmbbox[BOXRIGHT] = actor->x + actor->radius;
	tmbbox[BOXLEFT] = actor->x - actor->radius;

	telebt.Clear();

	// stomp on any things contacted
	xl = (tmbbox[BOXLEFT] - bmaporgx - MAXRADIUS)>>MAPBLOCKSHIFT;
	xh = (tmbbox[BOXRIGHT] - bmaporgx + MAXRADIUS)>>MAPBLOCKSHIFT;
	yl = (tmbbox[BOXBOTTOM] - bmaporgy - MAXRADIUS)>>MAPBLOCKSHIFT;
	yh = (tmbbox[BOXTOP] - bmaporgy + MAXRADIUS)>>MAPBLOCKSHIFT;

	for (bx = xl; bx <= xh; bx++)
	{
		for (by = yl; by <= yh; by++)
		{
			P_BlockThingsIterator (bx, by, PIT_StompThing2, telebt);
		}
	}
}

inline fixed_t secfriction (const sector_t *sec)
{
	fixed_t friction = Terrains[TerrainTypes[sec->floorpic]].Friction;
	return friction != 0 ? friction : sec->friction;
}

inline fixed_t secmovefac (const sector_t *sec)
{
	fixed_t movefactor = Terrains[TerrainTypes[sec->floorpic]].MoveFactor;
	return movefactor != 0 ? movefactor : sec->movefactor;
}

//
// killough 8/28/98:
//
// P_GetFriction()
//
// Returns the friction associated with a particular mobj.

int P_GetFriction (const AActor *mo, int *frictionfactor)
{
	int friction = ORIG_FRICTION;
	int movefactor = ORIG_FRICTION_FACTOR;
	const msecnode_t *m;
	const sector_t *sec;

	if (mo->flags2 & MF2_FLY && mo->flags & MF_NOGRAVITY)
	{
		friction = FRICTION_FLY;
	}
	else if (!(mo->flags & MF_NOGRAVITY) && mo->waterlevel > 1 ||
		(mo->waterlevel == 1 && mo->z > mo->floorz + 6*FRACUNIT))
	{
		friction = secfriction (mo->Sector);
		movefactor = secmovefac (mo->Sector) >> 1;
	}
	else if (var_friction && !(mo->flags & (MF_NOCLIP|MF_NOGRAVITY)))
	{	// When the object is straddling sectors with the same
		// floor height that have different frictions, use the lowest
		// friction value (muddy has precedence over icy).

		for (m = mo->touching_sectorlist; m; m = m->m_tnext)
		{
			sec = m->m_sector;

			// 3D floors must be checked, too!
			for(unsigned i=0;i<sec->e->ffloors.Size();i++)
			{
				F3DFloor * rover=sec->e->ffloors[i];
				if (!(rover->flags&FF_EXISTS)) continue;
				if(!(rover->flags & FF_SOLID)) continue;

				// Player must be on top of the floor to be affected...
				if(mo->z != rover->top.plane->ZatPoint(mo->x,mo->y)) continue;
				fixed_t newfriction=secfriction(rover->model);
				if (newfriction<friction)
				{
					friction = newfriction;
					movefactor = secmovefac(rover->model);
				}
			}

			if (!(sec->special & FRICTION_MASK) &&
				Terrains[TerrainTypes[sec->floorpic]].Friction == 0)
			{
				continue;
			}
			if ((secfriction(sec) < friction || friction == ORIG_FRICTION) &&
				(mo->z <= sec->floorplane.ZatPoint (mo->x, mo->y) ||
				(sec->heightsec && !(sec->heightsec->MoreFlags & SECF_IGNOREHEIGHTSEC) &&
				mo->z <= sec->heightsec->floorplane.ZatPoint (mo->x, mo->y))))
			{
				friction = secfriction (sec);
				movefactor = secmovefac (sec);
			}
		}
	}
  
	if (frictionfactor)
		*frictionfactor = movefactor;

	return friction;
}

// phares 3/19/98
// P_GetMoveFactor() returns the value by which the x,y
// movements are multiplied to add to player movement.
//
// killough 8/28/98: rewritten

int P_GetMoveFactor (const AActor *mo, int *frictionp)
{
	int movefactor, friction;

	// If the floor is icy or muddy, it's harder to get moving. This is where
	// the different friction factors are applied to 'trying to move'. In
	// p_mobj.c, the friction factors are applied as you coast and slow down.

	if ((friction = P_GetFriction(mo, &movefactor)) < ORIG_FRICTION)
	{
		// phares 3/11/98: you start off slowly, then increase as
		// you get better footing

		int momentum = P_AproxDistance(mo->momx,mo->momy);

		if (momentum > MORE_FRICTION_MOMENTUM<<2)
			movefactor <<= 3;
		else if (momentum > MORE_FRICTION_MOMENTUM<<1)
			movefactor <<= 2;
		else if (momentum > MORE_FRICTION_MOMENTUM)
			movefactor <<= 1;
	}

	if (frictionp)
		*frictionp = friction;

	return movefactor;
}

//
// MOVEMENT ITERATOR FUNCTIONS
//

//																	// phares
// PIT_CrossLine													//   |
// Checks to see if a PE->LS trajectory line crosses a blocking		//   V
// line. Returns false if it does.
//
// tmbbox holds the bounding box of the trajectory. If that box
// does not touch the bounding box of the line in question,
// then the trajectory is not blocked. If the PE is on one side
// of the line and the LS is on the other side, then the
// trajectory is blocked.
//
// Currently this assumes an infinite line, which is not quite
// correct. A more correct solution would be to check for an
// intersection of the trajectory and the line, but that takes
// longer and probably really isn't worth the effort.
//

static // killough 3/26/98: make static
bool PIT_CrossLine (line_t* ld)
{
	if (!(ld->flags & ML_TWOSIDED) ||
		// [BC] New blocking flag that blocks players.
		(ld->flags & (ML_BLOCKING|ML_BLOCKMONSTERS|ML_BLOCKEVERYTHING|ML_BLOCK_PLAYERS)))
		if (!(tmbbox[BOXLEFT]   > ld->bbox[BOXRIGHT]  ||
			  tmbbox[BOXRIGHT]  < ld->bbox[BOXLEFT]   ||
			  tmbbox[BOXTOP]    < ld->bbox[BOXBOTTOM] ||
			  tmbbox[BOXBOTTOM] > ld->bbox[BOXTOP]))
			if (P_PointOnLineSide(pe_x,pe_y,ld) != P_PointOnLineSide(ls_x,ls_y,ld))
				return(false);  // line blocks trajectory				//   ^
	return(true); // line doesn't block trajectory					//   |
}																	// phares

// [BC] ugh old code for people who just have to have doom2.exe style movement.
/* killough 8/1/98: used to test intersection between thing and line
 * assuming NO movement occurs -- used to avoid sticky situations.
 */

static int untouched(line_t *ld)
{
  fixed_t x, y, tmbbox[4];
  return 
    (tmbbox[BOXRIGHT] = (x=tmthing->x)+tmthing->radius) <= ld->bbox[BOXLEFT] ||
    (tmbbox[BOXLEFT] = x-tmthing->radius) >= ld->bbox[BOXRIGHT] ||
    (tmbbox[BOXTOP] = (y=tmthing->y)+tmthing->radius) <= ld->bbox[BOXBOTTOM] ||
    (tmbbox[BOXBOTTOM] = y-tmthing->radius) >= ld->bbox[BOXTOP] ||
    P_BoxOnLineSide(tmbbox, ld) != -1;
}

//
// PIT_CheckLine
// Adjusts tmfloorz and tmceilingz as lines are contacted
//

static // killough 3/26/98: make static
bool PIT_CheckLine (line_t *ld)
{
	bool rail = false;

	if (tmbbox[BOXRIGHT] <= ld->bbox[BOXLEFT]
		|| tmbbox[BOXLEFT] >= ld->bbox[BOXRIGHT]
		|| tmbbox[BOXTOP] <= ld->bbox[BOXBOTTOM]
		|| tmbbox[BOXBOTTOM] >= ld->bbox[BOXTOP] )
		return true;

	if (P_BoxOnLineSide (tmbbox, ld) != -1)
		return true;

	// A line has been hit
/*
=
= The moving thing's destination position will cross the given line.
= If this should not be allowed, return false.
= If the line is special, keep track of it to process later if the move
=       is proven ok.  NOTE: specials are NOT sorted by order, so two special lines
=       that are only 8 pixels apart could be crossed in either order.
*/
	
	if (!ld->backsector)
	{ // One sided line
		if (tmthing->flags2 & MF2_BLASTED)
		{
			P_DamageMobj (tmthing, NULL, NULL, tmthing->Mass >> 5, NAME_Melee);
		}
		BlockingLine = ld;
		CheckForPushSpecial (ld, 0, tmthing);
		return false;
	}

	if (!(tmthing->flags & MF_MISSILE) || (ld->flags & ML_BLOCKEVERYTHING))
	{
		if (ld->flags & ML_RAILING)
		{
			rail = true;
		}
		else if ((ld->flags & (ML_BLOCKING|ML_BLOCKEVERYTHING)) || 	// explicitly blocking everything
			(!(tmthing->flags3 & MF3_NOBLOCKMONST) && (ld->flags & ML_BLOCKMONSTERS)) || // block monsters only
			// [BC] New blocking flag that blocks players.
			(( tmthing->player ) && ( ld->flags & ML_BLOCK_PLAYERS )) ||
			((ld->flags & ML_BLOCK_FLOATERS) && (tmthing->flags & MF_FLOAT)))	// block floaters
		{
			if (tmthing->flags2 & MF2_BLASTED)
			{
				P_DamageMobj (tmthing, NULL, NULL, tmthing->Mass >> 5, NAME_Melee);
			}
			BlockingLine = ld;
			CheckForPushSpecial (ld, 0, tmthing);
			return false;
		}
	}

	// [RH] Steep sectors count as dropoffs (unless already in one)
	if (!(tmthing->flags & MF_DROPOFF) &&
		!(tmthing->flags & (MF_NOGRAVITY|MF_NOCLIP)))
	{
		if (ld->frontsector->floorplane.c < STEEPSLOPE ||
			ld->backsector->floorplane.c < STEEPSLOPE)
		{
			const msecnode_t *node = tmthing->touching_sectorlist;
			bool allow = false;
			int count = 0;
			while (node != NULL)
			{
				count++;
				if (node->m_sector->floorplane.c < STEEPSLOPE)
				{
					allow = true;
					break;
				}
				node = node->m_tnext;
			}
			if (!allow)
			{
				return false;
			}
		}
	}

	fixed_t sx, sy;

	// set openrange, opentop, openbottom
	if ((((ld->frontsector->floorplane.a | ld->frontsector->floorplane.b) |
		 (ld->backsector->floorplane.a | ld->backsector->floorplane.b) |
		 (ld->frontsector->ceilingplane.a | ld->frontsector->ceilingplane.b) |
		 (ld->backsector->ceilingplane.a | ld->backsector->ceilingplane.b)) == 0)
		 && ld->backsector->e->ffloors.Size()==0 && ld->frontsector->e->ffloors.Size()==0)
	{
		P_LineOpening (tmthing, ld, sx=tmx, sy=tmy, tmx, tmy);
	}
	else
	{ // Find the point on the line closest to the actor's center, and use
	  // that to calculate openings
		float dx = (float)ld->dx;
		float dy = (float)ld->dy;
		fixed_t r = (fixed_t)(((float)(tmx - ld->v1->x) * dx +
				 			   (float)(tmy - ld->v1->y) * dy) /
							  (dx*dx + dy*dy) * 16777216.f);
/*		Printf ("%d:%d: %d  (%d %d %d %d)  (%d %d %d %d)\n", level.time, ld-lines, r,
			ld->frontsector->floorplane.a,
			ld->frontsector->floorplane.b,
			ld->frontsector->floorplane.c,
			ld->frontsector->floorplane.ic,
			ld->backsector->floorplane.a,
			ld->backsector->floorplane.b,
			ld->backsector->floorplane.c,
			ld->backsector->floorplane.ic);*/
		if (r <= 0)
		{
			P_LineOpening (tmthing, ld, ld->v1->x, ld->v1->y, tmx, tmy);
		}
		else if (r >= (1<<24))
		{
			P_LineOpening (tmthing, ld, ld->v2->x, ld->v2->y, tmthing->x, tmthing->y);
		}
		else
		{
			P_LineOpening (tmthing, ld, ld->v1->x + MulScale24 (r, ld->dx),
				ld->v1->y + MulScale24 (r, ld->dy), tmx, tmy);
		}

		// the floorplane on both sides is identical with the current one
		// so don't mess around with the z-position
		if (ld->frontsector->floorplane==ld->backsector->floorplane &&
			ld->frontsector->floorplane==tmthing->Sector->floorplane &&
			!ld->frontsector->e->ffloors.Size() && !ld->backsector->e->ffloors.Size())
		{
			openbottom=INT_MIN;
		}
		/*	Printf ("    %d %d %d\n", sx, sy, openbottom);*/
	}

	if (rail &&
		// Eww! Gross! This check means the rail only exists if you stand on the
		// high side of the rail. So if you're walking on the low side of the rail,
		// it's possible to get stuck in the rail until you jump out. Unfortunately,
		// there is an area on Strife MAP04 that requires this behavior. Still, it's
		// better than Strife's handling of rails, which lets you jump into rails
		// from either side. How long until somebody reports this as a bug and I'm
		// forced to say, "It's not a bug. It's a feature?" Ugh.
		(gameinfo.gametype != GAME_Strife ||
		 level.flags & LEVEL_HEXENFORMAT ||
		 openbottom == tmthing->Sector->floorplane.ZatPoint (sx, sy)))
	{
		openbottom += 32*FRACUNIT;
	}

	// adjust floor / ceiling heights
	if (opentop < tmceilingz)
	{
		tmceilingz = opentop;
		tmceilingsector = opentopsec;
		tmceilingpic = opentopsec->ceilingpic;
		ceilingline = ld;
		BlockingLine = ld;
	}

	if (openbottom > tmfloorz)
	{
		tmfloorz = openbottom;
		tmfloorsector = openbottomsec;
		tmfloorpic = openbottomsec->floorpic;
		BlockingLine = ld;
	}

	if (lowfloor < tmdropoffz)
		tmdropoffz = lowfloor;
	
	// if contacted a special line, add it to the list
	if (ld->special)
	{
		spechit.Push (ld);
	}

	return true;
}

// [BC] ugh old code for people who just have to have doom2.exe style movement.
static bool PIT_OldCheckLine(line_t *ld) // killough 3/26/98: make static
{
  if (tmbbox[BOXRIGHT] <= ld->bbox[BOXLEFT]
   || tmbbox[BOXLEFT] >= ld->bbox[BOXRIGHT]
   || tmbbox[BOXTOP] <= ld->bbox[BOXBOTTOM]
   || tmbbox[BOXBOTTOM] >= ld->bbox[BOXTOP] )
    return true; // didn't hit it

  if (P_BoxOnLineSide(tmbbox, ld) != -1)
    return true; // didn't hit it

  // A line has been hit

  // The moving thing's destination position will cross the given line.
  // If this should not be allowed, return false.
  // If the line is special, keep track of it
  // to process later if the move is proven ok.
  // NOTE: specials are NOT sorted by order,
  // so two special lines that are only 8 pixels apart
  // could be crossed in either order.

  // killough 7/24/98: allow player to move out of 1s wall, to prevent sticking
  if (!ld->backsector) // one sided line
    {
      BlockingLine = ld;
      return tmunstuck && !untouched(ld) &&
	FixedMul(tmx-tmthing->x,ld->dy) > FixedMul(tmy-tmthing->y,ld->dx);
    }

  // killough 8/10/98: allow bouncing objects to pass through as missiles
  if (!(tmthing->flags & (MF_MISSILE/* | MF_BOUNCES*/)))
    {
      if (ld->flags & ML_BLOCKING)           // explicitly blocking everything
	return tmunstuck && !untouched(ld);  // killough 8/1/98: allow escape

      // killough 8/9/98: monster-blockers don't affect friends
      if (!(/*tmthing->flags & MF_FRIEND ||*/ tmthing->player)
	  && ld->flags & ML_BLOCKMONSTERS)
	return false; // block monsters only
    }

  // set openrange, opentop, openbottom
  // these define a 'window' from one sector to another across this line

  P_OldLineOpening (ld, tmthing->x, tmthing->y);

  // adjust floor & ceiling heights

  if (opentop < tmceilingz)
    {
      tmceilingz = opentop;
      ceilingline = ld;
      BlockingLine = ld;
    }

  if (openbottom > tmfloorz)
    {
      tmfloorz = openbottom;
		tmfloorsector = openbottomsec;
      //floorline = ld;          // killough 8/1/98: remember floor linedef
      BlockingLine = ld;
    }

  if (lowfloor < tmdropoffz)
    tmdropoffz = lowfloor;

	// if contacted a special line, add it to the list
	if (ld->special)
	{
		spechit.Push (ld);
	}

  return true;
}

//==========================================================================
//
// PIT_CheckThing
//
//==========================================================================

static AActor *stepthing;

bool PIT_CheckThing (AActor *thing)
{
	fixed_t topz;
	fixed_t blockdist;
	bool 	solid;
	int 	damage;
				
	// don't clip against self
	if (thing == tmthing)
		return true;

	if (!(thing->flags & (MF_SOLID|MF_SPECIAL|MF_SHOOTABLE)) )
		return true;	// can't hit thing

	blockdist = thing->radius + tmthing->radius;
	if (abs(thing->x - tmx) >= blockdist || abs(thing->y - tmy) >= blockdist)
	{
		// didn't hit thing
		return true;
	}
	BlockingMobj = thing;
	topz = thing->z + thing->height;
	if (!(i_compatflags & COMPATF_NO_PASSMOBJ) && !(tmthing->flags & (MF_FLOAT|MF_MISSILE|MF_SKULLFLY|MF_NOGRAVITY)) &&
		(thing->flags & MF_SOLID) && (thing->flags4 & MF4_ACTLIKEBRIDGE))
	{
		// [RH] Let monsters walk on actors as well as floors
		if ((tmthing->flags3 & MF3_ISMONSTER) &&
			topz >= tmfloorz && topz <= tmthing->z + tmthing->MaxStepHeight)
		{
			// The commented-out if is an attempt to prevent monsters from walking off a
			// thing further than they would walk off a ledge. I can't think of an easy
			// way to do this, so I restrict them to only walking on bridges instead.
			// Uncommenting the if here makes it almost impossible for them to walk on
			// anything, bridge or otherwise.
//			if (abs(thing->x - tmx) <= thing->radius &&
//				abs(thing->y - tmy) <= thing->radius)
			{
				stepthing = thing;
				tmfloorz = topz;
			}
		}
	}
	// [RH] If the other thing is a bridge, then treat the moving thing as if it had MF2_PASSMOBJ, so
	// you can use a scrolling floor to move scenery items underneath a bridge.
	if ((tmthing->flags2 & MF2_PASSMOBJ || thing->flags4 & MF4_ACTLIKEBRIDGE) && !(i_compatflags & COMPATF_NO_PASSMOBJ))
	{ // check if a mobj passed over/under another object
		if (tmthing->flags3 & thing->flags3 & MF3_DONTOVERLAP)
		{ // Some things prefer not to overlap each other, if possible
			return false;
		}
		if ((tmthing->z >= topz) || (tmthing->z + tmthing->height <= thing->z))
		{
			return true;
		}
	}
	// Check for skulls slamming into things
	if (tmthing->flags & MF_SKULLFLY)
	{
		bool res = tmthing->Slam (BlockingMobj);
		BlockingMobj = NULL;
		return res;
	}
	// Check for blasted thing running into another
	if ((tmthing->flags2 & MF2_BLASTED) && (thing->flags & MF_SHOOTABLE))
	{
		if (!(thing->flags2 & MF2_BOSS) && (thing->flags3 & MF3_ISMONSTER))
		{
			thing->momx += tmthing->momx;
			thing->momy += tmthing->momy;
			if ((thing->momx + thing->momy) > 3*FRACUNIT)
			{
				damage = (tmthing->Mass / 100) + 1;
				P_DamageMobj (thing, tmthing, tmthing, damage, tmthing->DamageType);
				P_TraceBleed (damage, thing, tmthing);
				damage = (thing->Mass / 100) + 1;
				P_DamageMobj (tmthing, thing, thing, damage >> 2, tmthing->DamageType);
				P_TraceBleed (damage, tmthing, thing);
			}
			return false;
		}
	}
	// Check for missile
	if (tmthing->flags & MF_MISSILE)
	{
		// Server decides collision.
//		if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) || ( CLIENTDEMO_IsPlaying( )))
//			return ( true );

		// Check for a non-shootable mobj
		if (thing->flags2 & MF2_NONSHOOTABLE)
		{
			return true;
		}
		// Check for passing through a ghost
		if ((thing->flags3 & MF3_GHOST) && (tmthing->flags2 & MF2_THRUGHOST))
		{
			return true;
		}
		// Check if it went over / under
		if (tmthing->z > thing->z + thing->height)
		{ // Over thing
			return true;
		}
		if (tmthing->z+tmthing->height < thing->z)
		{ // Under thing
			return true;
		}

		if (tmthing->flags2 & MF2_BOUNCE2 && (( tmthing->flags & MF_MISSILE ) == false ))
		{
			if (tmthing->Damage == 0)
			{
				return (tmthing->target == thing || !(thing->flags & MF_SOLID));
			}
		}

		switch (tmthing->SpecialMissileHit (thing))
		{
		case 0:		return false;
		case 1:		return true;
		default:	break;
		}

		// [RH] Extend DeHacked infighting to allow for monsters
		// to never fight each other

		// [Graf Zahl] Why do I have the feeling that this didn't really work anymore now
		// that ZDoom supports friendly monsters?
		

		if (tmthing->target != NULL)
		{
			if (thing == tmthing->target)
			{ // Don't missile self
				return true;
			}

			// players are never subject to infighting settings and are always allowed
			// to harm / be harmed by anything.
			if (!thing->player && !tmthing->target->player)
			{
				int infight;
				if (level.flags & LEVEL_TOTALINFIGHTING) infight=1;
				else if (level.flags & LEVEL_NOINFIGHTING) infight=-1;
				else infight = infighting;
				
				// [BC] No infighting during invasion mode.
				if (infight < 0 || invasion)
				{
					// -1: Monsters cannot hurt each other, but make exceptions for
					//     friendliness and hate status.
					if (tmthing->target->flags & MF_SHOOTABLE)
					{
						if (!(thing->flags3 & MF3_ISMONSTER))
						{
							return false;	// Question: Should monsters be allowed to shoot barrels in this mode?
											// The old code does not.
						}

						// Monsters that are clearly hostile can always hurt each other
						if (!thing->IsHostile (tmthing->target))
						{
							// The same if the shooter hates the target
							if (thing->tid == 0 || tmthing->target->TIDtoHate != thing->tid)
							{
								return false;
							}
						}
					}
				}
				else if (infight == 0)
				{
					//  0: Monsters cannot hurt same species except 
					//     cases where they are clearly supposed to do that
					if (thing->IsFriend (tmthing->target))
					{
						// Friends never harm each other
						return false;
					}
					if (thing->TIDtoHate != 0 && thing->TIDtoHate == tmthing->target->TIDtoHate)
					{
						// [RH] Don't hurt monsters that hate the same thing as you do
						return false;
					}
					if (thing->GetSpecies() == tmthing->target->GetSpecies())
					{
						// Don't hurt same species or any relative -
						// but only if the target isn't one's hostile.
						if (!thing->IsHostile (tmthing->target))
						{
							// Allow hurting monsters the shooter hates.
							if (thing->tid == 0 || tmthing->target->TIDtoHate != thing->tid)
							{
								return false;
							}
						}
					}
				}
				// else if (infight==1) any shot hurts anything - no further tests
			}
		}
		if (!(thing->flags & MF_SHOOTABLE))
		{ // Didn't do any damage
			return !(thing->flags & MF_SOLID);
		}
		if ((thing->flags4 & MF4_SPECTRAL) && !(tmthing->flags4 & MF4_SPECTRAL))
		{
			return true;
		}
		if (DoRipping)
		{
			if (LastRipped != thing)
			{
				LastRipped = thing;
				if (!(thing->flags & MF_NOBLOOD) &&
					!(thing->flags2 & MF2_REFLECTIVE) &&
					!(tmthing->flags3 & MF3_BLOODLESSIMPACT) &&
					!(thing->flags2 & (MF2_INVULNERABLE|MF2_DORMANT)))
				{ // Ok to spawn blood
					P_RipperBlood (tmthing, thing);
				}
				S_Sound (tmthing, CHAN_BODY, "misc/ripslop", 1, ATTN_IDLE);
				damage = tmthing->GetMissileDamage (3, 2);
				// [BB] The server handles the damage of RIPPER weapons.
				if (( NETWORK_GetState( ) != NETSTATE_CLIENT ) && ( CLIENTDEMO_IsPlaying( ) == false ))
					P_DamageMobj (thing, tmthing, tmthing->target, damage, tmthing->DamageType);
				if (!(tmthing->flags3 & MF3_BLOODLESSIMPACT))
				{
					P_TraceBleed (damage, thing, tmthing);
				}
				if (thing->flags2 & MF2_PUSHABLE
					&& !(tmthing->flags2 & MF2_CANNOTPUSH))
				{ // Push thing
					thing->momx += tmthing->momx>>2;
					thing->momy += tmthing->momy>>2;
				}
			}
			spechit.Clear ();
			return true;
		}
		// Do damage
		damage = tmthing->GetMissileDamage ((tmthing->flags4 & MF4_STRIFEDAMAGE) ? 3 : 7, 1);
		if (( NETWORK_GetState( ) != NETSTATE_CLIENT ) && ( CLIENTDEMO_IsPlaying( ) == false ))
		{
			if (damage > 0)
			{
				if (( tmthing->target ) &&
					( tmthing->target->player ) &&
					( thing->player ) &&
					( thing->IsTeammate( tmthing->target ) == false ))
				{
					tmthing->target->player->bStruckPlayer = true;
				}

				P_DamageMobj (thing, tmthing, tmthing->target, damage, tmthing->DamageType);
				if ((tmthing->flags5 & MF5_BLOODSPLATTER) &&
					!(thing->flags & MF_NOBLOOD) &&
					!(thing->flags2 & MF2_REFLECTIVE) &&
					!(thing->flags2 & (MF2_INVULNERABLE|MF2_DORMANT)) &&
					!(tmthing->flags3 & MF3_BLOODLESSIMPACT) &&
					(pr_checkthing() < 192))
				{
					P_BloodSplatter (tmthing->x, tmthing->y, tmthing->z, thing);
				}
				if (!(tmthing->flags3 & MF3_BLOODLESSIMPACT))
				{
					P_TraceBleed (damage, thing, tmthing);
				}
			}
			else if (damage < 0)
			{
				P_GiveBody (thing, -damage);
			}
		}
		return false;		// don't traverse any more
	}
	// [BC] Don't push things in client mode.
	if (thing->flags2 & MF2_PUSHABLE && !(tmthing->flags2 & MF2_CANNOTPUSH) &&
		( NETWORK_GetState( ) != NETSTATE_CLIENT ) &&
		( CLIENTDEMO_IsPlaying( ) == false ))// &&
		//(tmthing->player == NULL || !(tmthing->player->cheats & CF_PREDICTING)))
	{ // Push thing
		thing->momx += tmthing->momx >> 2;
		thing->momy += tmthing->momy >> 2;

		// [BC] If we're the server, tell clients to update the thing's position and
		// momentum.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_MoveThingExact( thing, CM_X|CM_Y|CM_Z|CM_MOMX|CM_MOMY|CM_MOMZ );
	}
	solid = (thing->flags & MF_SOLID) &&
			!(thing->flags & MF_NOCLIP) &&
			(tmthing->flags & MF_SOLID);
	// Check for special pickup
	if ((thing->flags & MF_SPECIAL) && (tmflags & MF_PICKUP)
		// [RH] The next condition is to compensate for the extra height
		// that gets added by P_CheckPosition() so that you cannot pick
		// up things that are above your true height.
		&& thing->z < tmthing->z + tmthing->height - tmthing->MaxStepHeight)
	{ // Can be picked up by tmthing
	
		// Server decides what items are touched.
		if (( NETWORK_GetState( ) != NETSTATE_CLIENT ) &&
			( CLIENTDEMO_IsPlaying( ) == false ))
		{
			P_TouchSpecialThing (thing, tmthing);	// can remove thing
		}
	}

	// If this object has the bumpspecial flag, try to activate the item's special.
	if (( NETWORK_GetState( ) != NETSTATE_CLIENT ) &&
		( CLIENTDEMO_IsPlaying( ) == false ) &&
		( solid ) &&
		( thing->ulSTFlags & STFL_BUMPSPECIAL ))
	{
		if (( TEAM_GetSimpleSTMode( )) && ( tmthing->player ) && ( tmthing->player->bOnTeam ))
		{
			if (( thing->ulSTFlags & STFL_SCOREPILLAR ) &&
				( tmthing->FindInventory( TEAM_GetFlagItem( !tmthing->player->ulTeam ))) &&
				( thing->args[0] == NUM_TEAMS || tmthing->player->ulTeam == thing->args[0] ) &&
				( thing->args[1] > 0 ))
			{
				if (( tmthing->player->ulTeam == TEAM_BLUE ) ? ( TEAM_GetBlueSkullTaken( ) == false ) : ( TEAM_GetRedSkullTaken( ) == false ))
					TEAM_ScoreSkulltagPoint( tmthing->player, thing->args[1], thing );
				else
					TEAM_DisplayNeedToReturnSkullMessage( tmthing->player );
			}
		}

		LineSpecials[thing->special]( NULL, tmthing, false, thing->args[0],
			thing->args[1], thing->args[2], thing->args[3], thing->args[4] );
	}

	// killough 3/16/98: Allow non-solid moving objects to move through solid
	// ones, by allowing the moving thing (tmthing) to move if it's non-solid,
	// despite another solid thing being in the way.
	// killough 4/11/98: Treat no-clipping things as not blocking

	return !solid;

	// return !(thing->flags & MF_SOLID);	// old code -- killough
}

//*****************************************************************************
//
// [BC] ugh old code for people who just have to have doom2.exe style movement.
static bool PIT_OldCheckThing(AActor *thing) // killough 3/26/98: make static
{
  fixed_t blockdist;
//  int damage;

  // killough 11/98: add touchy things
  if (!(thing->flags & (MF_SOLID|MF_SPECIAL|MF_SHOOTABLE)))
    return true;

  blockdist = thing->radius + tmthing->radius;

  if (abs(thing->x - tmx) >= blockdist || abs(thing->y - tmy) >= blockdist)
    return true; // didn't hit it

  // killough 11/98:
  //
  // This test has less information content (it's almost always false), so it
  // should not be moved up to first, as it adds more overhead than it removes.

  // don't clip against self

  if (thing == tmthing)
    return true;

  /* killough 11/98:
   *
   * TOUCHY flag, for mines or other objects which die on contact with solids.
   * If a solid object of a different type comes in contact with a touchy
   * thing, and the touchy thing is not the sole one moving relative to fixed
   * surroundings such as walls, then the touchy thing dies immediately.
   */
/*
  if (thing->flags & MF_TOUCHY &&                  // touchy object
      tmthing->flags & MF_SOLID &&                 // solid object touches it
      thing->health > 0 &&                         // touchy object is alive
      (thing->intflags & MIF_ARMED ||              // Thing is an armed mine
       sentient(thing)) &&                         // ... or a sentient thing
      (thing->type != tmthing->type ||             // only different species
       thing->type == MT_PLAYER) &&                // ... or different players
      thing->z + thing->height >= tmthing->z &&    // touches vertically
      tmthing->z + tmthing->height >= thing->z &&
      (thing->type ^ MT_PAIN) |                    // PEs and lost souls
      (tmthing->type ^ MT_SKULL) &&                // are considered same
      (thing->type ^ MT_SKULL) |                   // (but Barons & Knights
      (tmthing->type ^ MT_PAIN))                   // are intentionally not)
    {
      P_DamageMobj(thing, NULL, NULL, thing->health);  // kill object
      return true;
    }
*/
  // check for skulls slamming into things
/*
  if (tmthing->flags & MF_SKULLFLY)
    {
      // A flying skull is smacking something.
      // Determine damage amount, and the skull comes to a dead stop.

      int damage = ((M_Random()%8)+1)*tmthing->info->damage;

      P_DamageMobj (thing, tmthing, tmthing, damage);

      tmthing->flags &= ~MF_SKULLFLY;
      tmthing->momx = tmthing->momy = tmthing->momz = 0;

      P_SetMobjState (tmthing, tmthing->info->spawnstate);

      return false;   // stop moving
    }
*/
  // missiles can hit other things
  // killough 8/10/98: bouncing non-solid things can hit other things too
/*
  if (tmthing->flags & MF_MISSILE || (tmthing->flags & MF_BOUNCES &&
				      !(tmthing->flags & MF_SOLID)))
    {
      // see if it went over / under

      if (tmthing->z > thing->z + thing->height)
	return true;    // overhead

      if (tmthing->z+tmthing->height < thing->z)
	return true;    // underneath

      if (tmthing->target && (tmthing->target->type == thing->type ||
	  (tmthing->target->type == MT_KNIGHT && thing->type == MT_BRUISER)||
	  (tmthing->target->type == MT_BRUISER && thing->type == MT_KNIGHT)))
      {
	if (thing == tmthing->target)
	  return true;                // Don't hit same species as originator.
	else
	  if (thing->type != MT_PLAYER)	// Explode, but do no damage.
	    return false;	        // Let players missile other players.
      }
      
      // killough 8/10/98: if moving thing is not a missile, no damage
      // is inflicted, and momentum is reduced if object hit is solid.

      if (!(tmthing->flags & MF_MISSILE)) {
	if (!(thing->flags & MF_SOLID)) {
	    return true;
	} else {
	    tmthing->momx = -tmthing->momx;
	    tmthing->momy = -tmthing->momy;
	    if (!(tmthing->flags & MF_NOGRAVITY))
	      {
		tmthing->momx >>= 2;
		tmthing->momy >>= 2;
	      }
	    return false;
	}
      }

      if (!(thing->flags & MF_SHOOTABLE))
	return !(thing->flags & MF_SOLID); // didn't do any damage

      // damage / explode

      damage = ((P_Random(pr_damage)%8)+1)*tmthing->info->damage;
      P_DamageMobj (thing, tmthing, tmthing->target, damage);

      // don't traverse any more
      return false;
    }
*/
  // check for special pickup

	if ((thing->flags & MF_SPECIAL) && (tmflags & MF_PICKUP))
	{ // Can be picked up by tmthing

		// [BC] Server decides what items are touched.
		if (( NETWORK_GetState( ) != NETSTATE_CLIENT ) &&
			( CLIENTDEMO_IsPlaying( ) == false ))
		{
			P_TouchSpecialThing (thing, tmthing);	// can remove thing
		}
	}

  // killough 3/16/98: Allow non-solid moving objects to move through solid
  // ones, by allowing the moving thing (tmthing) to move if it's non-solid,
  // despite another solid thing being in the way.
  // killough 4/11/98: Treat no-clipping things as not blocking

  return !((thing->flags & MF_SOLID && !(thing->flags & MF_NOCLIP))
           && (tmthing->flags & MF_SOLID || (1)));

  // return !(thing->flags & MF_SOLID);   // old code -- killough
}

// This routine checks for Lost Souls trying to be spawned		// phares
// across 1-sided lines, impassible lines, or "monsters can't	//   |
// cross" lines. Draw an imaginary line between the PE			//   V
// and the new Lost Soul spawn spot. If that line crosses
// a 'blocking' line, then disallow the spawn. Only search
// lines in the blocks of the blockmap where the bounding box
// of the trajectory line resides. Then check bounding box
// of the trajectory vs. the bounding box of each blocking
// line to see if the trajectory and the blocking line cross.
// Then check the PE and LS to see if they're on different
// sides of the blocking line. If so, return true, otherwise
// false.

bool Check_Sides(AActor* actor, int x, int y)
{
	int bx,by,xl,xh,yl,yh;

	pe_x = actor->x;
	pe_y = actor->y;
	ls_x = x;
	ls_y = y;

	// Here is the bounding box of the trajectory

	tmbbox[BOXLEFT]   = pe_x < x ? pe_x : x;
	tmbbox[BOXRIGHT]  = pe_x > x ? pe_x : x;
	tmbbox[BOXTOP]    = pe_y > y ? pe_y : y;
	tmbbox[BOXBOTTOM] = pe_y < y ? pe_y : y;

	// Determine which blocks to look in for blocking lines

	xl = (tmbbox[BOXLEFT]   - bmaporgx)>>MAPBLOCKSHIFT;
	xh = (tmbbox[BOXRIGHT]  - bmaporgx)>>MAPBLOCKSHIFT;
	yl = (tmbbox[BOXBOTTOM] - bmaporgy)>>MAPBLOCKSHIFT;
	yh = (tmbbox[BOXTOP]    - bmaporgy)>>MAPBLOCKSHIFT;

	// xl->xh, yl->yh determine the mapblock set to search

	validcount++; // prevents checking same line twice
	for (bx = xl ; bx <= xh ; bx++)
		for (by = yl ; by <= yh ; by++)
		if (!P_BlockLinesIterator(bx,by,PIT_CrossLine))
			return true;										//   ^
	return(false);												//   |
}																// phares

//*****************************************************************************
//
// [BC] ugh old code for people who just have to have doom2.exe style movement.
bool P_OldCheckPosition (AActor *thing, fixed_t x, fixed_t y)
{
  	static TArray<AActor *> checkpbt;

  int xl, xh, yl, yh, bx, by;
  subsector_t*  newsubsec;

  tmthing = thing;

  tmx = x;
  tmy = y;

  tmbbox[BOXTOP] = y + tmthing->radius;
  tmbbox[BOXBOTTOM] = y - tmthing->radius;
  tmbbox[BOXRIGHT] = x + tmthing->radius;
  tmbbox[BOXLEFT] = x - tmthing->radius;

  newsubsec = R_PointInSubsector (x,y);
  /*floorline = */ceilingline = BlockingLine = NULL; // killough 8/1/98

  // Whether object can get out of a sticky situation:
  tmunstuck = thing->player &&          /* only players */
    thing->player->mo == thing &&       /* not voodoo dolls */
    /*mbf_features*/0; /* not under old demos */

  // The base floor / ceiling is from the subsector
  // that contains the point.
  // Any contacted lines the step closer together
  // will adjust them.

	tmfloorz = tmdropoffz = newsubsec->sector->floorplane.ZatPoint (x, y);
	tmceilingz = newsubsec->sector->ceilingplane.ZatPoint (x, y);
	validcount++;
	spechit.Clear( );
	checkpbt.Clear( );

  if ( tmthing->flags & MF_NOCLIP )
    return true;

  // Check things first, possibly picking things up.
  // The bounding box is extended by MAXRADIUS
  // because mobj_ts are grouped into mapblocks
  // based on their origin point, and can overlap
  // into adjacent blocks by up to MAXRADIUS units.

  xl = (tmbbox[BOXLEFT] - bmaporgx - MAXRADIUS)>>MAPBLOCKSHIFT;
  xh = (tmbbox[BOXRIGHT] - bmaporgx + MAXRADIUS)>>MAPBLOCKSHIFT;
  yl = (tmbbox[BOXBOTTOM] - bmaporgy - MAXRADIUS)>>MAPBLOCKSHIFT;
  yh = (tmbbox[BOXTOP] - bmaporgy + MAXRADIUS)>>MAPBLOCKSHIFT;


  for (bx=xl ; bx<=xh ; bx++)
    for (by=yl ; by<=yh ; by++)
      if (!P_BlockThingsIterator(bx,by,PIT_OldCheckThing,checkpbt))
        return false;

  // check lines

  xl = (tmbbox[BOXLEFT] - bmaporgx)>>MAPBLOCKSHIFT;
  xh = (tmbbox[BOXRIGHT] - bmaporgx)>>MAPBLOCKSHIFT;
  yl = (tmbbox[BOXBOTTOM] - bmaporgy)>>MAPBLOCKSHIFT;
  yh = (tmbbox[BOXTOP] - bmaporgy)>>MAPBLOCKSHIFT;

  for (bx=xl ; bx<=xh ; bx++)
    for (by=yl ; by<=yh ; by++)
      if (!P_BlockLinesIterator (bx,by,PIT_OldCheckLine))
        return false; // doesn't fit

  return true;
}

//---------------------------------------------------------------------------
//
// PIT_CheckOnmobjZ
//
//---------------------------------------------------------------------------

bool PIT_CheckOnmobjZ (AActor *thing)
{
	if (!(thing->flags & MF_SOLID))
	{ // Can't hit thing
		return true;
	}
	if (thing->flags & (MF_SPECIAL|MF_NOCLIP|MF_CORPSE))
	{ // [RH] Corpses and specials and noclippers don't block moves
		return true;
	}
	if (!(thing->flags4 & MF4_ACTLIKEBRIDGE) && (tmthing->flags & MF_SPECIAL))
	{ // [RH] Only bridges block pickup items
		return true;
	}
	if (thing == tmthing)
	{ // Don't clip against self
		return true;
	}
	if (tmthing->z > thing->z+thing->height)
	{ // over thing
		return true;
	}
	else if (tmthing->z+tmthing->height <= thing->z)
	{ // under thing
		return true;
	}
	else if (!tmflags && onmobj != NULL && thing->z + thing->height < onmobj->z + onmobj->height)
	{ // something higher is in the way
		return true;
	}
	fixed_t blockdist = thing->radius+tmthing->radius;
	if (abs(thing->x-tmx) >= blockdist || abs(thing->y-tmy) >= blockdist)
	{ // Didn't hit thing
		return true;
	}
	onmobj = thing;
	return !tmflags;
}

/*
===============================================================================

						MOVEMENT CLIPPING

===============================================================================
*/

//----------------------------------------------------------------------------
//
// FUNC P_TestMobjLocation
//
// Returns true if the mobj is not blocked by anything at its current
// location, otherwise returns false.
//
//----------------------------------------------------------------------------

bool P_TestMobjLocation (AActor *mobj)
{
	int flags;

	flags = mobj->flags;
	mobj->flags &= ~MF_PICKUP;
	if (P_CheckPosition(mobj, mobj->x, mobj->y))
	{ // XY is ok, now check Z
		mobj->flags = flags;
		fixed_t z = mobj->z;
		if (mobj->flags2 & MF2_FLOATBOB)
		{
			z -= FloatBobOffsets[(mobj->FloatBobPhase + level.maptime - 1) & 63];
		}
		if ((z < mobj->floorz) || (z + mobj->height > mobj->ceilingz))
		{ // Bad Z
			return false;
		}
		return true;
	}
	mobj->flags = flags;
	return false;
}

//
// P_CheckPosition
// This is purely informative, nothing is modified
// (except things picked up and missile damage applied).
// 
// in:
//	a AActor (can be valid or invalid)
//	a position to be checked
//	 (doesn't need to be related to the AActor->x,y)
//
// during:
//	special things are touched if MF_PICKUP
//	early out on solid lines?
//
// out:
//	newsubsec
//	floorz
//	ceilingz
//	tmdropoffz = the lowest point contacted (monsters won't move to a dropoff)
//	speciallines[]
//	numspeciallines
//  AActor *BlockingMobj = pointer to thing that blocked position (NULL if not
//   blocked, or blocked by a line).
bool P_CheckPosition (AActor *thing, fixed_t x, fixed_t y)
{
	static TArray<AActor *> checkpbt;

	int xl, xh;
	int yl, yh;
	int bx, by;
	subsector_t *newsubsec;
	AActor *thingblocker;
	AActor *fakedblocker;
	fixed_t realheight = thing->height;

	tmthing = thing;
	tmflags = thing->flags;

	tmx = x;
	tmy = y;

	tmbbox[BOXTOP] = y + thing->radius;
	tmbbox[BOXBOTTOM] = y - thing->radius;
	tmbbox[BOXRIGHT] = x + thing->radius;
	tmbbox[BOXLEFT] = x - thing->radius;

	newsubsec = R_PointInSubsector (x,y);
	ceilingline = BlockingLine = NULL;
	
// The base floor / ceiling is from the subsector that contains the point.
// Any contacted lines the step closer together will adjust them.
	tmfloorz = tmdropoffz = newsubsec->sector->floorplane.ZatPoint (x, y);
	tmceilingz = newsubsec->sector->ceilingplane.ZatPoint (x, y);
	tmfloorpic = newsubsec->sector->floorpic;
	tmfloorsector = newsubsec->sector;
	tmceilingpic = newsubsec->sector->ceilingpic;
	tmceilingsector = newsubsec->sector;

	//Added by MC: Fill the tmsector.
	tmsector = newsubsec->sector;
	
	//Check 3D floors
	if(tmsector->e->ffloors.Size())
	{
		F3DFloor*  rover;
		fixed_t    delta1;
		fixed_t    delta2;
		int        thingtop = thing->z + thing->height;
		
		for(unsigned i=0;i<tmsector->e->ffloors.Size();i++)
		{
			rover=tmsector->e->ffloors[i];
			if(!(rover->flags & FF_SOLID) || !(rover->flags & FF_EXISTS)) continue;

			fixed_t ff_bottom=rover->bottom.plane->ZatPoint(x, y);
			fixed_t ff_top=rover->top.plane->ZatPoint(x, y);
			
			delta1 = thing->z - (ff_bottom + ((ff_top-ff_bottom)/2));
			delta2 = thingtop - (ff_bottom + ((ff_top-ff_bottom)/2));
			if(ff_top > tmfloorz && abs(delta1) < abs(delta2)) 
			{
				tmfloorz = tmdropoffz = ff_top;
				tmfloorpic = *rover->top.texture;
			}
			if(ff_bottom < tmceilingz && abs(delta1) >= abs(delta2)) 
			{
				tmceilingz = ff_bottom;
				tmceilingpic = *rover->bottom.texture;
			}
		}
	}
	
	validcount++;
	spechit.Clear ();
	checkpbt.Clear ();

	if ((tmflags & MF_NOCLIP) && !(tmflags & MF_SKULLFLY))
		return true;
	
	// Check things first, possibly picking things up.
	// The bounding box is extended by MAXRADIUS
	// because DActors are grouped into mapblocks
	// based on their origin point, and can overlap
	// into adjacent blocks by up to MAXRADIUS units.
	xl = (tmbbox[BOXLEFT] - bmaporgx - MAXRADIUS)>>MAPBLOCKSHIFT;
	xh = (tmbbox[BOXRIGHT] - bmaporgx + MAXRADIUS)>>MAPBLOCKSHIFT;
	yl = (tmbbox[BOXBOTTOM] - bmaporgy - MAXRADIUS)>>MAPBLOCKSHIFT;
	yh = (tmbbox[BOXTOP] - bmaporgy + MAXRADIUS)>>MAPBLOCKSHIFT;

	BlockingMobj = NULL;
	thingblocker = NULL;
	fakedblocker = NULL;
	if (thing->player)
	{ // [RH] Fake taller height to catch stepping up into things.
		thing->height = realheight + thing->MaxStepHeight;
	}

	stepthing = NULL;
	for (bx = xl; bx <= xh; bx++)
	{
		for (by = yl; by <= yh; by++)
		{
			AActor *robin = NULL;
			do
			{
				if (!P_BlockThingsIterator (bx, by, PIT_CheckThing, checkpbt, robin))
				{ // [RH] If a thing can be stepped up on, we need to continue checking
				  // other things in the blocks and see if we hit something that is
				  // definitely blocking. Otherwise, we need to check the lines, or we
				  // could end up stuck inside a wall.
					if (BlockingMobj == NULL || (i_compatflags & COMPATF_NO_PASSMOBJ))
					{ // Thing slammed into something; don't let it move now.
						thing->height = realheight;
						return false;
					}
					else if (!BlockingMobj->player && !(thing->flags & (MF_FLOAT|MF_MISSILE|MF_SKULLFLY)) &&
						BlockingMobj->z+BlockingMobj->height-thing->z <= thing->MaxStepHeight)
					{
						if (thingblocker == NULL ||
							BlockingMobj->z > thingblocker->z)
						{
							thingblocker = BlockingMobj;
						}
						robin = BlockingMobj;
						BlockingMobj = NULL;
					}
					else if (thing->player &&
						thing->z + thing->height - BlockingMobj->z <= thing->MaxStepHeight)
					{
						if (thingblocker)
						{ // There is something to step up on. Return this thing as
						  // the blocker so that we don't step up.
							thing->height = realheight;
							return false;
						}
						// Nothing is blocking us, but this actor potentially could
						// if there is something else to step on.
						fakedblocker = BlockingMobj;
						robin = BlockingMobj;
						BlockingMobj = NULL;
					}
					else
					{ // Definitely blocking
						thing->height = realheight;
						return false;
					}
				}
				else
				{
					robin = NULL;
				}
			} while (robin);
		}
	}

	// check lines

	// [RH] We need to increment validcount again, because a function above may
	// have already set some lines to equal the current validcount.
	//
	// Specifically, when DehackedPickup spawns a new item in its TryPickup()
	// function, that new actor will set the lines around it to match validcount
	// when it links itself into the world. If we just leave validcount alone,
	// that will give the player the freedom to walk through walls at will near
	// a pickup they cannot get, because their validcount will prevent them from
	// being considered for collision with the player.
	validcount++;

	BlockingMobj = NULL;
	thing->height = realheight;
	if (tmflags & MF_NOCLIP)
		return (BlockingMobj = thingblocker) == NULL;
	xl = (tmbbox[BOXLEFT] - bmaporgx)>>MAPBLOCKSHIFT;
	xh = (tmbbox[BOXRIGHT] - bmaporgx)>>MAPBLOCKSHIFT;
	yl = (tmbbox[BOXBOTTOM] - bmaporgy)>>MAPBLOCKSHIFT;
	yh = (tmbbox[BOXTOP] - bmaporgy)>>MAPBLOCKSHIFT;

	fixed_t thingdropoffz = tmfloorz;
	//bool onthing = (thingdropoffz != tmdropoffz);
	tmfloorz = tmdropoffz;

	for (bx=xl ; bx<=xh ; bx++)
		for (by=yl ; by<=yh ; by++)
			if (!P_BlockLinesIterator (bx,by,PIT_CheckLine))
				return false;

	if (tmceilingz - tmfloorz < thing->height)
		return false;

	if (stepthing != NULL)
	{
		tmdropoffz = thingdropoffz;
	}

	return (BlockingMobj = thingblocker) == NULL;
}

//=============================================================================
//
// P_CheckOnmobj(AActor *thing)
//
//				Checks if the new Z position is legal
//=============================================================================

AActor *P_CheckOnmobj (AActor *thing)
{
	fixed_t oldz;
	bool good;

	oldz = thing->z;
	P_FakeZMovement (thing);
	good = P_TestMobjZ (thing, false);
	thing->z = oldz;

	return good ? NULL : onmobj;
}

//=============================================================================
//
// P_TestMobjZ
//
//=============================================================================

bool P_TestMobjZ (AActor *actor, bool quick)
{
	static TArray<AActor *> mobjzbt;

	int	xl,xh,yl,yh,bx,by;
	fixed_t x, y;

	onmobj = NULL;
	if (actor->flags & MF_NOCLIP)
		return true;

	tmx = x = actor->x;
	tmy = y = actor->y;
	tmthing = actor;
	tmflags = quick;

	tmbbox[BOXTOP] = y + actor->radius;
	tmbbox[BOXBOTTOM] = y - actor->radius;
	tmbbox[BOXRIGHT] = x + actor->radius;
	tmbbox[BOXLEFT] = x - actor->radius;
//
// the bounding box is extended by MAXRADIUS because actors are grouped
// into mapblocks based on their origin point, and can overlap into adjacent
// blocks by up to MAXRADIUS units
//
	xl = (tmbbox[BOXLEFT] - bmaporgx - MAXRADIUS)>>MAPBLOCKSHIFT;
	xh = (tmbbox[BOXRIGHT] - bmaporgx + MAXRADIUS)>>MAPBLOCKSHIFT;
	yl = (tmbbox[BOXBOTTOM] - bmaporgy - MAXRADIUS)>>MAPBLOCKSHIFT;
	yh = (tmbbox[BOXTOP] - bmaporgy + MAXRADIUS)>>MAPBLOCKSHIFT;

	mobjzbt.Clear();

	for (bx = xl; bx <= xh; bx++)
		for (by = yl; by <= yh; by++)
			if (!P_BlockThingsIterator (bx, by, PIT_CheckOnmobjZ, mobjzbt))
				return false;

	return onmobj == NULL;
}

//=============================================================================
//
// P_FakeZMovement
//
//				Fake the zmovement so that we can check if a move is legal
//=============================================================================

void P_FakeZMovement (AActor *mo)
{
//
// adjust height
//
	mo->z += mo->momz;
	if ((mo->flags&MF_FLOAT) && mo->target)
	{ // float down towards target if too close
		if (!(mo->flags & MF_SKULLFLY) && !(mo->flags & MF_INFLOAT))
		{
			fixed_t dist = P_AproxDistance (mo->x - mo->target->x, mo->y - mo->target->y);
			fixed_t delta = (mo->target->z + (mo->height>>1)) - mo->z;
			if (delta < 0 && dist < -(delta*3))
				mo->z -= mo->FloatSpeed;
			else if (delta > 0 && dist < (delta*3))
				mo->z += mo->FloatSpeed;
		}
	}
	if (mo->player && mo->flags&MF_NOGRAVITY && (mo->z > mo->floorz))
	{
		mo->z += finesine[(FINEANGLES/80*level.maptime)&FINEMASK]/8;
	}

//
// clip movement
//
	if (mo->z <= mo->floorz)
	{ // hit the floor
		mo->z = mo->floorz;
	}

	if (mo->z + mo->height > mo->ceilingz)
	{ // hit the ceiling
		mo->z = mo->ceilingz - mo->height;
	}
}

//===========================================================================
//
// CheckForPushSpecial
//
//===========================================================================

static void CheckForPushSpecial (line_t *line, int side, AActor *mobj)
{
	if (line->special)
	{
		// [BC] In a netgame, since mobj->target is never valid, we must go this route to avoid a crash.
		if ((mobj->flags2 & MF2_PUSHWALL) || ( NETWORK_GetState( ) == NETSTATE_CLIENT ) || ( CLIENTDEMO_IsPlaying( )))
		{
			P_ActivateLine (line, mobj, side, SPAC_PUSH);
		}
		else if (mobj->flags2 & MF2_IMPACT)
		{
			if ((level.flags & LEVEL_MISSILESACTIVATEIMPACT) ||
				!(mobj->flags & MF_MISSILE) ||
				(mobj->target == NULL))
			{
				P_ActivateLine (line, mobj, side, SPAC_IMPACT);
			}
			else
			{
				P_ActivateLine (line, mobj->target, side, SPAC_IMPACT);
			}
		}	
	}
}

//
// P_TryMove
// Attempt to move to a new position,
// crossing special lines unless MF_TELEPORT is set.
//
bool P_TryMove (AActor *thing, fixed_t x, fixed_t y,
				bool dropoff, // killough 3/15/98: allow dropoff as option
				const secplane_t * onfloor) // [RH] Let P_TryMove keep the thing on the floor
{
	fixed_t 	oldx;
	fixed_t 	oldy;
	fixed_t		oldz;
	int 		side;
	int 		oldside;
	line_t* 	ld;
	sector_t*	oldsec = thing->Sector;	// [RH] for sector actions
	sector_t*	newsec;

	floatok = false;
	oldz = thing->z;
	if (onfloor)
	{
		thing->z = onfloor->ZatPoint (x, y);
	}
	if (!P_CheckPosition (thing, x, y))
	{
		// Solid wall or thing
		if (!BlockingMobj || BlockingMobj->player || !thing->player)
		{
			goto pushline;
		}
		else
		{
			if (BlockingMobj->player || !thing->player)
			{
				goto pushline;
			}
			else if (BlockingMobj->z+BlockingMobj->height-thing->z 
				> thing->MaxStepHeight
				|| (BlockingMobj->Sector->ceilingplane.ZatPoint (x, y)
				- (BlockingMobj->z+BlockingMobj->height) < thing->height)
				|| (tmceilingz-(BlockingMobj->z+BlockingMobj->height) 
				< thing->height))
			{
				goto pushline;
			}
		}
		if (!(tmthing->flags2 & MF2_PASSMOBJ) || (i_compatflags & COMPATF_NO_PASSMOBJ))
		{
			thing->z = oldz;
			return false;
		}
	}

	if (onfloor && tmfloorsector == thing->floorsector)
	{
		thing->z = tmfloorz;
	}
	if (!(thing->flags & MF_NOCLIP))
	{
		if (tmceilingz - tmfloorz < thing->height)
		{
			goto pushline;		// doesn't fit
		}

		floatok = true;
		
		if (!(thing->flags & MF_TELEPORT)
			&& tmceilingz - thing->z < thing->height
			&& !(thing->flags3 & MF3_CEILINGHUGGER)
			&& (!(thing->flags2 & MF2_FLY) || !(thing->flags & MF_NOGRAVITY)))
		{
			goto pushline;		// mobj must lower itself to fit
		}
		if (thing->flags2 & MF2_FLY && thing->flags & MF_NOGRAVITY)
		{
#if 1
			if (thing->z+thing->height > tmceilingz)
				goto pushline;
#else
			// When flying, slide up or down blocking lines until the actor
			// is not blocked.
			if (thing->z+thing->height > tmceilingz)
			{
				thing->momz = -8*FRACUNIT;
				goto pushline;
			}
			else if (thing->z < tmfloorz && tmfloorz-tmdropoffz > thing->MaxDropOffHeight)
			{
				thing->momz = 8*FRACUNIT;
				goto pushline;
			}
#endif
		}
		if (!(thing->flags & MF_TELEPORT) && !(thing->flags3 & MF3_FLOORHUGGER))
		{
			if (tmfloorz-thing->z > thing->MaxStepHeight)
			{ // too big a step up
				goto pushline;
			}
			else if ((thing->flags & MF_MISSILE) && tmfloorz > thing->z)
			{ // [RH] Don't let normal missiles climb steps
				goto pushline;
			}
			else if (thing->z < tmfloorz)
			{ // [RH] Check to make sure there's nothing in the way for the step up
				fixed_t savedz = thing->z;
				bool good;
				thing->z = tmfloorz;
				good = P_TestMobjZ (thing);
				thing->z = savedz;
				if (!good)
				{
					goto pushline;
				}
			}
		}

		// killough 3/15/98: Allow certain objects to drop off
		if ((!dropoff && !(thing->flags & (MF_DROPOFF|MF_FLOAT|MF_MISSILE))) || (thing->flags5&MF5_NODROPOFF))
		{
			if (!(thing->flags5&MF5_AVOIDINGDROPOFF))
			{
				fixed_t floorz = tmfloorz;
				// [RH] If the thing is standing on something, use its current z as the floorz.
				// This is so that it does not walk off of things onto a drop off.
				if (thing->flags2 & MF2_ONMOBJ)
				{
					floorz = MAX(thing->z, tmfloorz);
				}

				if (floorz - tmdropoffz > thing->MaxDropOffHeight &&
					!(thing->flags2 & MF2_BLASTED))
				{ // Can't move over a dropoff unless it's been blasted
					thing->z = oldz;
					return false;
				}
			}
			else
			{
				// special logic to move a monster off a dropoff
				// this intentionally does not check for standing on things.
				if (thing->floorz - tmfloorz > thing->MaxDropOffHeight ||
					thing->dropoffz - tmdropoffz > thing->MaxDropOffHeight) return false;
			}
		}
		if (thing->flags2 & MF2_CANTLEAVEFLOORPIC
			&& (tmfloorpic != thing->Sector->floorpic
				|| tmfloorz - thing->z != 0))
		{ // must stay within a sector of a certain floor type
			thing->z = oldz;
			return false;
		}
	}

	// [RH] Check status of eyes against fake floor/ceiling in case
	// it slopes or the player's eyes are bobbing in and out.

	bool oldAboveFakeFloor, oldAboveFakeCeiling;
	fixed_t viewheight;
	
	viewheight = thing->player ? thing->player->viewheight : thing->height / 2;
	oldAboveFakeFloor = oldAboveFakeCeiling = false;	// pacify GCC

	if (oldsec->heightsec)
	{
		fixed_t eyez = oldz + viewheight;

		oldAboveFakeFloor = eyez > oldsec->heightsec->floorplane.ZatPoint (thing->x, thing->y);
		oldAboveFakeCeiling = eyez > oldsec->heightsec->ceilingplane.ZatPoint (thing->x, thing->y);
	}

	// the move is ok, so link the thing into its new position
	thing->UnlinkFromWorld ();

	oldx = thing->x;
	oldy = thing->y;
	thing->floorz = tmfloorz;
	thing->ceilingz = tmceilingz;
	thing->dropoffz = tmdropoffz;		// killough 11/98: keep track of dropoffs
	thing->floorpic = tmfloorpic;
	thing->floorsector = tmfloorsector;
	thing->ceilingpic = tmceilingpic;
	thing->ceilingsector = tmceilingsector;
	thing->x = x;
	thing->y = y;

	// [BC] Flag this thing as having moved.
	thing->ulSTFlags |= STFL_POSITIONCHANGED;

	thing->LinkToWorld ();

	if (thing->flags2 & MF2_FLOORCLIP)
	{
		thing->AdjustFloorClip ();
	}
/*
	// [RH] Don't activate anything if just predicting
	if (thing->player && (thing->player->cheats & CF_PREDICTING))
	{
		return true;
	}
*/
	// if any special lines were hit, do the effect
	if (!(thing->flags & (MF_TELEPORT|MF_NOCLIP)))
	{
		while (spechit.Pop (ld))
		{
			// see if the line was crossed
			side = P_PointOnLineSide (thing->x, thing->y, ld);
			oldside = P_PointOnLineSide (oldx, oldy, ld);
			if (side != oldside && ld->special)
			{
				// Don't activate specials if the thing is a spectating player.
				if ( thing->player && thing->player->bSpectating )
				{
					// Although teleport specials are okay.
					if (ld->special == Teleport ||
				     ld->special == Teleport_NoFog ||
					 ld->special == Teleport_Line)
					{ 
						P_ActivateLine (ld, thing, oldside, SPAC_CROSS); 
					}
				}
				else if (thing->player)
				{
					P_ActivateLine (ld, thing, oldside, SPAC_CROSS);
				}
				else if (thing->flags2 & MF2_MCROSS)
				{
					P_ActivateLine (ld, thing, oldside, SPAC_MCROSS);
				}
				else if (thing->flags2 & MF2_PCROSS)
				{
					P_ActivateLine (ld, thing, oldside, SPAC_PCROSS);
				}
				else if ((ld->special == Teleport ||
						  ld->special == Teleport_NoFog ||
						  ld->special == Teleport_Line))
				{	// [RH] Just a little hack for BOOM compatibility
					P_ActivateLine (ld, thing, oldside, SPAC_MCROSS);
				}
				else
				{
					// I don't think allowing non-monsters to activate
					// monster-allowed lines will hurt Hexen compatibility.
					P_ActivateLine (ld, thing, oldside, SPAC_OTHERCROSS);
				}
			}
		}
	}

	// [RH] Check for crossing fake floor/ceiling
	newsec = thing->Sector;
	if (newsec->heightsec && oldsec->heightsec && newsec->SecActTarget)
	{
		const sector_t *hs = newsec->heightsec;
		fixed_t eyez = thing->z + viewheight;
		fixed_t fakez = hs->floorplane.ZatPoint (x, y);

		if (!oldAboveFakeFloor && eyez > fakez)
		{ // View went above fake floor
			newsec->SecActTarget->TriggerAction (thing, SECSPAC_EyesSurface);
		}
		else if (oldAboveFakeFloor && eyez <= fakez)
		{ // View went below fake floor
			newsec->SecActTarget->TriggerAction (thing, SECSPAC_EyesDive);
		}

		if (!(hs->MoreFlags & SECF_FAKEFLOORONLY))
		{
			fakez = hs->ceilingplane.ZatPoint (x, y);
			if (!oldAboveFakeCeiling && eyez > fakez)
			{ // View went above fake ceiling
				newsec->SecActTarget->TriggerAction (thing, SECSPAC_EyesAboveC);
			}
			else if (oldAboveFakeCeiling && eyez <= fakez)
			{ // View went below fake ceiling
				newsec->SecActTarget->TriggerAction (thing, SECSPAC_EyesBelowC);
			}
		}
	}

	// [RH] If changing sectors, trigger transitions
	if (oldsec != newsec)
	{
		if (oldsec->SecActTarget)
		{
			oldsec->SecActTarget->TriggerAction (thing, SECSPAC_Exit);
		}
		if (newsec->SecActTarget)
		{
			int act = SECSPAC_Enter;
			if (thing->z <= newsec->floorplane.ZatPoint (thing->x, thing->y))
			{
				act |= SECSPAC_HitFloor;
			}
			if (thing->z + thing->height >= newsec->ceilingplane.ZatPoint (thing->x, thing->y))
			{
				act |= SECSPAC_HitCeiling;
			}
			if (newsec->heightsec &&
				thing->z == newsec->heightsec->floorplane.ZatPoint (thing->x, thing->y))
			{
				act |= SECSPAC_HitFakeFloor;
			}
			newsec->SecActTarget->TriggerAction (thing, act);
		}
	}
	return true;

pushline:
/*
	// [RH] Don't activate anything if just predicting
	if (thing->player && (thing->player->cheats & CF_PREDICTING))
	{
		return false;
	}
*/
	thing->z = oldz;
	if (!(thing->flags&(MF_TELEPORT|MF_NOCLIP)))
	{
		int numSpecHitTemp;

		if (tmthing->flags2 & MF2_BLASTED)
		{
			P_DamageMobj (tmthing, NULL, NULL, tmthing->Mass >> 5, NAME_Melee);
		}
		numSpecHitTemp = (int)spechit.Size ();
		while (numSpecHitTemp > 0)
		{
			// see which lines were pushed
			ld = spechit[--numSpecHitTemp];
			side = P_PointOnLineSide (thing->x, thing->y, ld);
			CheckForPushSpecial (ld, side, thing);
		}
	}
	return false;
}

//*****************************************************************************
//
// [BC] ugh old code for people who just have to have doom2.exe style movement.
bool P_OldTryMove (AActor *thing, fixed_t x, fixed_t y,
				bool dropoff, // killough 3/15/98: allow dropoff as option
				bool onfloor) // [RH] Let P_TryMove keep the thing on the floor
{
	fixed_t		oldx, oldy;
	line_t* 	ld;
	int 		side;
	int 		oldside;

//  felldown = 
	floatok = false;               // killough 11/98

//	if (!P_OldCheckPosition (thing, x, y))
	if (!P_CheckPosition (thing, x, y))
		return false;   // solid wall or thing

	if ( !(thing->flags & MF_NOCLIP) )
	{
		// killough 7/26/98: reformatted slightly
		// killough 8/1/98: Possibly allow escape if otherwise stuck

		if (tmceilingz - tmfloorz < thing->height ||     // doesn't fit
			// mobj must lower to fit
			(floatok = true, !(thing->flags & MF_TELEPORT) &&
			tmceilingz - thing->z < thing->height) ||
			// too big a step up
			(!(thing->flags & MF_TELEPORT) && 
			tmfloorz - thing->z > 24*FRACUNIT))
		{	
			return tmunstuck 
				&& !(ceilingline && untouched(ceilingline));
				/*&& !(  floorline && untouched(  floorline));*/
		}

		/* killough 3/15/98: Allow certain objects to drop off
		* killough 7/24/98, 8/1/98: 
		* Prevent monsters from getting stuck hanging off ledges
		* killough 10/98: Allow dropoffs in controlled circumstances
		* killough 11/98: Improve symmetry of clipping on stairs
		*/

		if (!(thing->flags & (MF_DROPOFF|MF_FLOAT)))
		{
//			if (comp[comp_dropoff])
			if ( 1 )
			{
				if ((0/*compatibility*/ || !dropoff) && (tmfloorz - tmdropoffz > 24*FRACUNIT))
					return false;                      // don't stand over a dropoff
			}
			else
			{
				// [BB] dropoff is a bool, it can't be equal to 2
				if (!dropoff || (/*dropoff==2 &&*/ // large jump down (e.g. dogs)
					(tmfloorz-tmdropoffz > 128*FRACUNIT || 
					!thing->target || thing->target->z >tmdropoffz)))
				{
					if (/*!monkeys ||*/ /*!mbf_features*/ 1 ?
						( tmfloorz - tmdropoffz > 24*FRACUNIT ) :
						thing->floorz  - tmfloorz > 24*FRACUNIT ||
						thing->dropoffz - tmdropoffz > 24*FRACUNIT)
					{
						return false;
					}
				}
				else
				{ /* dropoff allowed -- check for whether it fell more than 24 */
//					felldown = !(thing->flags & MF_NOGRAVITY) &&
//					thing->z - tmfloorz > 24*FRACUNIT;
				}
			}

			if (thing->flags2 & MF2_BOUNCE2 &&    // killough 8/13/98
			!(thing->flags & (MF_MISSILE|MF_NOGRAVITY)) &&
			/*!sentient(thing) &&*/ tmfloorz - thing->z > 16*FRACUNIT)
				return false; // too big a step up for bouncers under gravity
/*
			// killough 11/98: prevent falling objects from going up too many steps
			if (thing->intflags & MIF_FALLING && tmfloorz - thing->z >
				FixedMul(thing->momx,thing->momx)+FixedMul(thing->momy,thing->momy))
			{
				return false;
			}
*/
		}
	}

  // the move is ok,
  // so unlink from the old position and link into the new position

	thing->UnlinkFromWorld( );

	oldx = thing->x;
	oldy = thing->y;
	thing->floorz = tmfloorz;
	thing->ceilingz = tmceilingz;
	thing->dropoffz = tmdropoffz;      // killough 11/98: keep track of dropoffs
	thing->x = x;
	thing->y = y;

	thing->LinkToWorld( );

  // if any special lines were hit, do the effect

	// if any special lines were hit, do the effect
	if (!(thing->flags & (MF_TELEPORT|MF_NOCLIP)))
	{
		while (spechit.Pop (ld))
		{
			// see if the line was crossed
			side = P_PointOnLineSide (thing->x, thing->y, ld);
			oldside = P_PointOnLineSide (oldx, oldy, ld);
			if (side != oldside && ld->special)
			{
				// Don't activate specials if the thing is a spectating player.
				if ( thing->player && thing->player->bSpectating )
				{
					// Although teleport specials are okay.
					if (ld->special == Teleport ||
				     ld->special == Teleport_NoFog ||
					 ld->special == Teleport_Line)
					{ 
						P_ActivateLine (ld, thing, oldside, SPAC_CROSS); 
					}
				}
				else if (thing->player)
				{
					P_ActivateLine (ld, thing, oldside, SPAC_CROSS);
				}
				else if (thing->flags2 & MF2_MCROSS)
				{
					P_ActivateLine (ld, thing, oldside, SPAC_MCROSS);
				}
				else if (thing->flags2 & MF2_PCROSS)
				{
					P_ActivateLine (ld, thing, oldside, SPAC_PCROSS);
				}
				else if ((ld->special == Teleport ||
						  ld->special == Teleport_NoFog ||
						  ld->special == Teleport_Line))
				{	// [RH] Just a little hack for BOOM compatibility
					P_ActivateLine (ld, thing, oldside, SPAC_MCROSS);
				}
			}
		}
	}

	return true;
}

//
// SLIDE MOVE
// Allows the player to slide along any angled walls.
//
fixed_t 		bestslidefrac;
fixed_t 		secondslidefrac;

line_t* 		bestslideline;
line_t* 		secondslideline;

AActor* 		slidemo;

fixed_t 		tmxmove;
fixed_t 		tmymove;

extern bool		onground;


//
// P_HitSlideLine
// Adjusts the xmove / ymove
// so that the next move will slide along the wall.
// If the floor is icy, then you can bounce off a wall.				// phares
//
void P_HitSlideLine (line_t* ld)
{
	int 	side;

	angle_t lineangle;
	angle_t moveangle;
	angle_t deltaangle;
	
	fixed_t movelen;
	bool	icyfloor;	// is floor icy?							// phares
																	//   |
	// Under icy conditions, if the angle of approach to the wall	//   V
	// is more than 45 degrees, then you'll bounce and lose half
	// your momentum. If less than 45 degrees, you'll slide along
	// the wall. 45 is arbitrary and is believable.

	// Check for the special cases of horz or vert walls.

	// killough 10/98: only bounce if hit hard (prevents wobbling)
	icyfloor = 
		(P_AproxDistance(tmxmove, tmymove) > 4*FRACUNIT) &&
		var_friction &&  // killough 8/28/98: calc friction on demand
		slidemo->z <= slidemo->floorz &&
		P_GetFriction (slidemo, NULL) > ORIG_FRICTION;

	if (ld->slopetype == ST_HORIZONTAL)
	{
		if (icyfloor && (abs(tmymove) > abs(tmxmove)))
		{
			tmxmove /= 2; // absorb half the momentum
			tmymove = -tmymove/2;
			if (slidemo->player && ( slidemo->player->bSpectating == false ) && slidemo->health > 0)// && !(slidemo->player->cheats & CF_PREDICTING))
			{
				S_Sound (slidemo, CHAN_VOICE, "*grunt", 1, ATTN_IDLE); // oooff!

				// [BC] Tell clients of the "oof" sound.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_SoundActor( slidemo, CHAN_VOICE, "*grunt", 1, ATTN_IDLE );
			}
		}
		else
			tmymove = 0; // no more movement in the Y direction
		return;
	}

	if (ld->slopetype == ST_VERTICAL)
	{
		if (icyfloor && (abs(tmxmove) > abs(tmymove)))
		{
			tmxmove = -tmxmove/2; // absorb half the momentum
			tmymove /= 2;
			if (slidemo->player && ( slidemo->player->bSpectating == false ) && slidemo->health > 0)// && !(slidemo->player->cheats & CF_PREDICTING))
			{
				// [BC] Tell clients of the "oof" sound.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_SoundActor( slidemo, CHAN_VOICE, "*grunt", 1, ATTN_IDLE );

				S_Sound (slidemo, CHAN_VOICE, "*grunt", 1, ATTN_IDLE); // oooff!//   ^
			}
		}																		//   |
		else																	// phares
			tmxmove = 0; // no more movement in the X direction
		return;
	}

	// The wall is angled. Bounce if the angle of approach is		// phares
	// less than 45 degrees.										// phares

	side = P_PointOnLineSide (slidemo->x, slidemo->y, ld);

	lineangle = R_PointToAngle2 (0,0, ld->dx, ld->dy);

	if (side == 1)
		lineangle += ANG180;

	moveangle = R_PointToAngle2 (0,0, tmxmove, tmymove);

	moveangle += 10;		// prevents sudden path reversal due to	// phares
							// rounding error						//   |
	deltaangle = moveangle-lineangle;								//   V
	movelen = P_AproxDistance (tmxmove, tmymove);
	if (icyfloor && (deltaangle > ANG45) && (deltaangle < ANG90+ANG45))
	{
		moveangle = lineangle - deltaangle;
		movelen /= 2; // absorb
		if (slidemo->player && ( slidemo->player->bSpectating == false ) && slidemo->health > 0)// && !(slidemo->player->cheats & CF_PREDICTING))
		{
			S_Sound (slidemo, CHAN_VOICE, "*grunt", 1, ATTN_IDLE); // oooff!
		}
		moveangle >>= ANGLETOFINESHIFT;
		tmxmove = FixedMul (movelen, finecosine[moveangle]);
		tmymove = FixedMul (movelen, finesine[moveangle]);
	}																//   ^
	else															//   |
	{																// phares
#if 0
		fixed_t newlen;
	
		if (deltaangle > ANG180)
			deltaangle += ANG180;
		//	I_Error ("SlideLine: ang>ANG180");

		lineangle >>= ANGLETOFINESHIFT;
		deltaangle >>= ANGLETOFINESHIFT;

		newlen = FixedMul (movelen, finecosine[deltaangle]);

		tmxmove = FixedMul (newlen, finecosine[lineangle]);
		tmymove = FixedMul (newlen, finesine[lineangle]);
#else
		divline_t dll, dlv;
		fixed_t inter1, inter2, inter3;

		P_MakeDivline (ld, &dll);

		dlv.x = slidemo->x;
		dlv.y = slidemo->y;
		dlv.dx = dll.dy;
		dlv.dy = -dll.dx;

		inter1 = P_InterceptVector(&dll, &dlv);

		dlv.dx = tmxmove;
		dlv.dy = tmymove;
		inter2 = P_InterceptVector (&dll, &dlv);
		inter3 = P_InterceptVector (&dlv, &dll);

		if (inter3 != 0)
		{
			tmxmove = Scale (inter2-inter1, dll.dx, inter3);
			tmymove = Scale (inter2-inter1, dll.dy, inter3);
		}
		else
		{
			tmxmove = tmymove = 0;
		}
#endif
	}																// phares
}

// [BC] ugh old code for people who just have to have doom2.exe style movement.
static void P_OldHitSlideLine(line_t *ld)
  {
  int     side;
  angle_t lineangle;
  angle_t moveangle;
  angle_t deltaangle;
  fixed_t movelen;
  fixed_t newlen;
  bool icyfloor;  // is floor icy?                               // phares
                                                                    //   |
  // Under icy conditions, if the angle of approach to the wall     //   V
  // is more than 45 degrees, then you'll bounce and lose half
  // your momentum. If less than 45 degrees, you'll slide along
  // the wall. 45 is arbitrary and is believable.

  // Check for the special cases of horz or vert walls.

  /* killough 10/98: only bounce if hit hard (prevents wobbling)
   * cph - DEMOSYNC - should only affect players in Boom demos? */
  icyfloor = 0;/*
    (mbf_features ? 
     P_AproxDistance(tmxmove, tmymove) > 4*FRACUNIT : !compatibility) &&
    variable_friction &&  // killough 8/28/98: calc friction on demand
    slidemo->z <= slidemo->floorz &&
    P_GetFriction(slidemo, NULL) > ORIG_FRICTION;
*/
  if (ld->slopetype == ST_HORIZONTAL)
    {
    if (icyfloor && (abs(tmymove) > abs(tmxmove)))
      {
      tmxmove /= 2; // absorb half the momentum
      tmymove = -tmymove/2;
      //S_StartSound(slidemo,sfx_oof); // oooff!
      }
    else
      tmymove = 0; // no more movement in the Y direction
    return;
    }

  if (ld->slopetype == ST_VERTICAL)
    {
    if (icyfloor && (abs(tmxmove) > abs(tmymove)))
      {
      tmxmove = -tmxmove/2; // absorb half the momentum
      tmymove /= 2;
      //S_StartSound(slidemo,sfx_oof); // oooff!                      //   ^
      }                                                             //   |
    else                                                            // phares
      tmxmove = 0; // no more movement in the X direction
    return;
    }

  // The wall is angled. Bounce if the angle of approach is         // phares
  // less than 45 degrees.                                          // phares

  side = P_PointOnLineSide (slidemo->x, slidemo->y, ld);

  lineangle = R_PointToAngle2 (0,0, ld->dx, ld->dy);
  if (side == 1)
    lineangle += ANG180;
  moveangle = R_PointToAngle2 (0,0, tmxmove, tmymove);

  // killough 3/2/98:
  // The moveangle+=10 breaks v1.9 demo compatibility in
  // some demos, so it needs demo_compatibility switch.

  if (0)//!demo_compatibility)
    moveangle += 10; // prevents sudden path reversal due to        // phares
                     // rounding error                              //   |
  deltaangle = moveangle-lineangle;                                 //   V
  movelen = P_AproxDistance (tmxmove, tmymove);
  if (icyfloor && (deltaangle > ANG45) && (deltaangle < ANG90+ANG45))
    {
    moveangle = lineangle - deltaangle;
    movelen /= 2; // absorb
    //S_StartSound(slidemo,sfx_oof); // oooff!
    moveangle >>= ANGLETOFINESHIFT;
    tmxmove = FixedMul (movelen, finecosine[moveangle]);
    tmymove = FixedMul (movelen, finesine[moveangle]);
    }                                                               //   ^
  else                                                              //   |
    {                                                               // phares
    if (deltaangle > ANG180)
      deltaangle += ANG180;

    //  I_Error ("SlideLine: ang>ANG180");

    lineangle >>= ANGLETOFINESHIFT;
    deltaangle >>= ANGLETOFINESHIFT;
    newlen = FixedMul (movelen, finecosine[deltaangle]);
    tmxmove = FixedMul (newlen, finecosine[lineangle]);
    tmymove = FixedMul (newlen, finesine[lineangle]);
    }                                                               // phares
  }


//
// PTR_SlideTraverse
//
bool PTR_SlideTraverse (intercept_t* in)
{
	line_t* 	li;
		
	if (!in->isaline)
		I_Error ("PTR_SlideTraverse: not a line?");
				
	li = in->d.line;
	
	if ( !(li->flags & ML_TWOSIDED) )
	{
		if (P_PointOnLineSide (slidemo->x, slidemo->y, li))
		{
			// don't hit the back side
			return true;				
		}
		goto isblocking;
	}
	if (li->flags & (ML_BLOCKING|ML_BLOCKEVERYTHING))
	{
		goto isblocking;
	}

	// set openrange, opentop, openbottom
	P_LineOpening (slidemo, li, trace.x + FixedMul (trace.dx, in->frac),
		trace.y + FixedMul (trace.dy, in->frac));
	
	if (openrange < slidemo->height)
		goto isblocking;				// doesn't fit
				
	if (opentop - slidemo->z < slidemo->height)
		goto isblocking;				// mobj is too high

	if (openbottom - slidemo->z > slidemo->MaxStepHeight)
	{
		goto isblocking;				// too big a step up
	}
	else if (slidemo->z < openbottom)
	{ // [RH] Check to make sure there's nothing in the way for the step up
		fixed_t savedz = slidemo->z;
		slidemo->z = openbottom;
		bool good = P_TestMobjZ (slidemo);
		slidemo->z = savedz;
		if (!good)
		{
			goto isblocking;
		}
	}

	// this line doesn't block movement
	return true;				
		
	// the line does block movement,
	// see if it is closer than best so far
  isblocking:			
	if (in->frac < bestslidefrac)
	{
		secondslidefrac = bestslidefrac;
		secondslideline = bestslideline;
		bestslidefrac = in->frac;
		bestslideline = li;
	}
		
	return false;		// stop
}

// [BC] ugh old code for people who just have to have doom2.exe style movement.
static bool PTR_OldSlideTraverse(intercept_t *in)
  {
  line_t* li;

  if (!in->isaline)
    I_Error ("PTR_SlideTraverse: not a line?");

  li = in->d.line;

  if ( ! (li->flags & ML_TWOSIDED) )
    {
    if (P_PointOnLineSide (slidemo->x, slidemo->y, li))
      return true; // don't hit the back side
    goto isblocking;
    }

  // set openrange, opentop, openbottom.
  // These define a 'window' from one sector to another across a line

//  P_LineOpening (li);
	P_LineOpening (li, trace.x + FixedMul (trace.dx, in->frac),
		trace.y + FixedMul (trace.dy, in->frac));	// set openrange, opentop, openbottom

  if (openrange < slidemo->height)
    goto isblocking;  // doesn't fit

  if (opentop - slidemo->z < slidemo->height)
    goto isblocking;  // mobj is too high

  if (openbottom - slidemo->z > 24*FRACUNIT )
    goto isblocking;  // too big a step up

  // this line doesn't block movement

  return true;

  // the line does block movement,
  // see if it is closer than best so far

isblocking:

  if (in->frac < bestslidefrac)
    {
    secondslidefrac = bestslidefrac;
    secondslideline = bestslideline;
    bestslidefrac = in->frac;
    bestslideline = li;
    }

  return false; // stop
  }


//
// P_SlideMove
//
// The momx / momy move is bad, so try to slide along a wall.
//
// Find the first line hit, move flush to it, and slide along it
//
// This is a kludgy mess.
//
void P_SlideMove (AActor *mo, fixed_t tryx, fixed_t tryy, int numsteps)
{
	fixed_t leadx, leady;
	fixed_t trailx, traily;
	fixed_t newx, newy;
	fixed_t xmove, ymove;
	const secplane_t * walkplane;
	int hitcount;

	slidemo = mo;
	hitcount = 3;
	
  retry:
	if (!--hitcount)
		goto stairstep; 		// don't loop forever
	
	// trace along the three leading corners
	if (tryx > 0)
	{
		leadx = mo->x + mo->radius;
		trailx = mo->x - mo->radius;
	}
	else
	{
		leadx = mo->x - mo->radius;
		trailx = mo->x + mo->radius;
	}

	if (tryy > 0)
	{
		leady = mo->y + mo->radius;
		traily = mo->y - mo->radius;
	}
	else
	{
		leady = mo->y - mo->radius;
		traily = mo->y + mo->radius;
	}

	bestslidefrac = FRACUNIT+1;
		
	P_PathTraverse (leadx, leady, leadx+tryx, leady+tryy, PT_ADDLINES, PTR_SlideTraverse);
	P_PathTraverse (trailx, leady, trailx+tryx, leady+tryy, PT_ADDLINES, PTR_SlideTraverse);
	P_PathTraverse (leadx, traily, leadx+tryx, traily+tryy, PT_ADDLINES, PTR_SlideTraverse);
	
	// move up to the wall
	if (bestslidefrac == FRACUNIT+1)
	{
		// the move must have hit the middle, so stairstep
	  stairstep:
		// killough 3/15/98: Allow objects to drop off ledges
		xmove = 0, ymove = tryy;
		walkplane = P_CheckSlopeWalk (mo, xmove, ymove);
		if (!P_TryMove (mo, mo->x + xmove, mo->y + ymove, true, walkplane))
		{
			xmove = tryx, ymove = 0;
			walkplane = P_CheckSlopeWalk (mo, xmove, ymove);
			P_TryMove (mo, mo->x + xmove, mo->y + ymove, true, walkplane);
		}
		return;
	}

	// fudge a bit to make sure it doesn't hit
	bestslidefrac -= FRACUNIT/32;
	if (bestslidefrac > 0)
	{
		newx = FixedMul (tryx, bestslidefrac);
		newy = FixedMul (tryy, bestslidefrac);
		
		// killough 3/15/98: Allow objects to drop off ledges
		if (!P_TryMove (mo, mo->x+newx, mo->y+newy, true))
			goto stairstep;
	}

	// Now continue along the wall.
	bestslidefrac = FRACUNIT - (bestslidefrac + FRACUNIT/32);	// remainder
	if (bestslidefrac > FRACUNIT)
		bestslidefrac = FRACUNIT;
	else if (bestslidefrac <= 0)
		return;

	tryx = tmxmove = FixedMul (tryx, bestslidefrac);
	tryy = tmymove = FixedMul (tryy, bestslidefrac);

	P_HitSlideLine (bestslideline); 	// clip the moves

	mo->momx = tmxmove * numsteps;
	mo->momy = tmymove * numsteps;

	// killough 10/98: affect the bobbing the same way (but not voodoo dolls)
	if (mo->player && mo->player->mo == mo)
	{
		if (abs(mo->player->momx) > abs(mo->momx))
			mo->player->momx = mo->momx;
		if (abs(mo->player->momy) > abs(mo->momy))
			mo->player->momy = mo->momy;
	}

	walkplane = P_CheckSlopeWalk (mo, tmxmove, tmymove);

	// killough 3/15/98: Allow objects to drop off ledges
	if (!P_TryMove (mo, mo->x+tmxmove, mo->y+tmymove, true, walkplane))
	{
		goto retry;
	}
}

//*****************************************************************************
//
// [BC] ugh old code for people who just have to have doom2.exe style movement.
void P_OldSlideMove (AActor *mo)
{
  int hitcount = 3;

  slidemo = mo; // the object that's sliding

  do 
    {
      fixed_t leadx, leady, trailx, traily;

      if (!--hitcount)
	goto stairstep;   // don't loop forever

      // trace along the three leading corners

      if (mo->momx > 0)
	leadx = mo->x + mo->radius, trailx = mo->x - mo->radius;
      else
	leadx = mo->x - mo->radius, trailx = mo->x + mo->radius;

      if (mo->momy > 0)
	leady = mo->y + mo->radius, traily = mo->y - mo->radius;
      else
	leady = mo->y - mo->radius, traily = mo->y + mo->radius;

      bestslidefrac = FRACUNIT+1;

      P_PathTraverse(leadx, leady, leadx+mo->momx, leady+mo->momy,
		     PT_ADDLINES, PTR_OldSlideTraverse);
      P_PathTraverse(trailx, leady, trailx+mo->momx, leady+mo->momy,
		     PT_ADDLINES, PTR_OldSlideTraverse);
      P_PathTraverse(leadx, traily, leadx+mo->momx, traily+mo->momy,
		     PT_ADDLINES, PTR_OldSlideTraverse);

      // move up to the wall

      if (bestslidefrac == FRACUNIT+1)
	{
	  // the move must have hit the middle, so stairstep

	stairstep:

	  /* killough 3/15/98: Allow objects to drop off ledges
	   *
	   * phares 5/4/98: kill momentum if you can't move at all
	   * This eliminates player bobbing if pressed against a wall
	   * while on ice.
	   *
	   * killough 10/98: keep buggy code around for old Boom demos
	   *
	   * cph 2000/09//23: buggy code was only in Boom v2.01
	   */

	  if (!P_OldTryMove(mo, mo->x, mo->y + mo->momy, true))
	    if (!P_OldTryMove(mo, mo->x + mo->momx, mo->y, true))
	      if (0)//compatibility_level == boom_201_compatibility)
		mo->momx = mo->momy = 0;

	  break;
	}

      // fudge a bit to make sure it doesn't hit
      
      if ((bestslidefrac -= 0x800) > 0)
	{
	  fixed_t newx = FixedMul(mo->momx, bestslidefrac);
	  fixed_t newy = FixedMul(mo->momy, bestslidefrac);

	  // killough 3/15/98: Allow objects to drop off ledges
	  
	  if (!P_OldTryMove(mo, mo->x+newx, mo->y+newy, true))
	    goto stairstep;
	}

      // Now continue along the wall.
      // First calculate remainder.

      bestslidefrac = FRACUNIT-(bestslidefrac+0x800);

      if (bestslidefrac > FRACUNIT)
	bestslidefrac = FRACUNIT;

      if (bestslidefrac <= 0)
	break;

      tmxmove = FixedMul(mo->momx, bestslidefrac);
      tmymove = FixedMul(mo->momy, bestslidefrac);

      P_OldHitSlideLine(bestslideline); // clip the moves

      mo->momx = tmxmove;
      mo->momy = tmymove;

      /* killough 10/98: affect the bobbing the same way (but not voodoo dolls)
       * cph - DEMOSYNC? */
      if (mo->player && mo->player->mo == mo)
	{
	  if (abs(mo->player->momx) > abs(tmxmove))
	    mo->player->momx = tmxmove;
	  if (abs(mo->player->momy) > abs(tmymove))
	    mo->player->momy = tmymove;
	}
    }  // killough 3/15/98: Allow objects to drop off ledges:
  while (!P_OldTryMove(mo, mo->x+tmxmove, mo->y+tmymove, true));
}

//============================================================================
//
// P_CheckSlopeWalk
//
//============================================================================

const secplane_t * P_CheckSlopeWalk (AActor *actor, fixed_t &xmove, fixed_t &ymove)
{
	static secplane_t copyplane;
	if (actor->flags & MF_NOGRAVITY)
	{
		return NULL;
	}

	const secplane_t *plane = &actor->floorsector->floorplane;
	fixed_t planezhere = plane->ZatPoint (actor->x, actor->y);

	for(unsigned int i=0;i<actor->floorsector->e->ffloors.Size();i++)
	{
		F3DFloor * rover= actor->floorsector->e->ffloors[i];
		if(!(rover->flags & FF_SOLID) || !(rover->flags & FF_EXISTS)) continue;

		fixed_t thisplanez = rover->top.plane->ZatPoint(actor->x, actor->y);

		if (thisplanez>planezhere && thisplanez<=actor->z + actor->MaxStepHeight)
		{
			copyplane = *rover->top.plane;
			if (copyplane.c<0) copyplane.FlipVert();
			plane = &copyplane;
			planezhere=thisplanez;
		}
	}
	if (actor->floorsector != actor->Sector)
	{
		for(unsigned int i=0;i<actor->Sector->e->ffloors.Size();i++)
		{
			F3DFloor * rover= actor->Sector->e->ffloors[i];
			if(!(rover->flags & FF_SOLID) || !(rover->flags & FF_EXISTS)) continue;

			fixed_t thisplanez = rover->top.plane->ZatPoint(actor->x, actor->y);

			if (thisplanez>planezhere && thisplanez<=actor->z + actor->MaxStepHeight)
			{
				copyplane = *rover->top.plane;
				if (copyplane.c<0) copyplane.FlipVert();
				plane = &copyplane;
				planezhere=thisplanez;
			}
		}
	}

	if (actor->floorsector != actor->Sector)
	{
		// this additional check prevents sliding on sloped dropoffs
		if (planezhere>actor->floorz+4*FRACUNIT)
			return NULL;
	}

	if (actor->z - planezhere > FRACUNIT)
	{ // not on floor
		return NULL;
	}

	if ((plane->a | plane->b) != 0)
	{
		fixed_t destx, desty;
		fixed_t t;

		destx = actor->x + xmove;
		desty = actor->y + ymove;
		t = TMulScale16 (plane->a, destx, plane->b, desty, plane->c, actor->z) + plane->d;
		if (t < 0)
		{ // Desired location is behind (below) the plane
		  // (i.e. Walking up the plane)
			if (plane->c < STEEPSLOPE)
			{ // Can't climb up slopes of ~45 degrees or more
				if (actor->flags & MF_NOCLIP)
				{
					return plane;
				}
				else
				{
					const msecnode_t *node;
					bool dopush = true;

					if (plane->c > STEEPSLOPE*2/3)
					{
						for (node = actor->touching_sectorlist; node; node = node->m_tnext)
						{
							const sector_t *sec = node->m_sector;
							if (sec->floorplane.c >= STEEPSLOPE)
							{
								if (sec->floorplane.ZatPoint (destx, desty) >= actor->z - actor->MaxStepHeight)
								{
									dopush = false;
									break;
								}
							}
						}
					}
					if (dopush)
					{
						xmove = actor->momx = plane->a * 2;
						ymove = actor->momy = plane->b * 2;
					}
					return NULL;
				}
			}
			// Slide the desired location along the plane's normal
			// so that it lies on the plane's surface
			destx -= FixedMul (plane->a, t);
			desty -= FixedMul (plane->b, t);
			xmove = destx - actor->x;
			ymove = desty - actor->y;
			return plane;
		}
		else if (t > 0)
		{ // Desired location is in front of (above) the plane
			if (planezhere == actor->z)
			{ // Actor's current spot is on/in the plane, so walk down it
			  // Same principle as walking up, except reversed
				destx += FixedMul (plane->a, t);
				desty += FixedMul (plane->b, t);
				xmove = destx - actor->x;
				ymove = desty - actor->y;
				return plane;//(plane->c >= STEEPSLOPE);
			}
		}
	}
	return NULL;
}

//============================================================================
//
// PTR_BounceTraverse
//
//============================================================================

bool PTR_BounceTraverse (intercept_t *in)
{
	line_t  *li;

	if (!in->isaline)
		I_Error ("PTR_BounceTraverse: not a line?");

	li = in->d.line;
	if (li->flags & ML_BLOCKEVERYTHING)
	{
		goto bounceblocking;
	}
	if (!(li->flags&ML_TWOSIDED))
	{
		if (P_PointOnLineSide (slidemo->x, slidemo->y, li))
			return true;			// don't hit the back side
		goto bounceblocking;
	}

	P_LineOpening (slidemo, li, trace.x + FixedMul (trace.dx, in->frac),
		trace.y + FixedMul (trace.dy, in->frac));	// set openrange, opentop, openbottom
	if (openrange < slidemo->height)
		goto bounceblocking;				// doesn't fit

	if (opentop - slidemo->z < slidemo->height)
		goto bounceblocking;				// mobj is too high
	return true;			// this line doesn't block movement

// the line does block movement, see if it is closer than best so far
bounceblocking:
	if (in->frac < bestslidefrac)
	{
		secondslidefrac = bestslidefrac;
		secondslideline = bestslideline;
		bestslidefrac = in->frac;
		bestslideline = li;
	}
	return false;   // stop
}

//============================================================================
//
// P_BounceWall
//
//============================================================================

bool P_BounceWall (AActor *mo)
{
	fixed_t         leadx, leady;
	int             side;
	angle_t         lineangle, moveangle, deltaangle;
	fixed_t         movelen;
	line_t			*line;

	if (!(mo->flags2 & MF2_BOUNCE2))
	{
		return false;
	}

	if (BlockingLine != NULL)
	{
		line = BlockingLine;
	}
	else
	{
		slidemo = mo;
//
// trace along the three leading corners
//
		if (mo->momx > 0)
		{
			leadx = mo->x+mo->radius;
		}
		else
		{
			leadx = mo->x-mo->radius;
		}
		if (mo->momy > 0)
		{
			leady = mo->y+mo->radius;
		}
		else
		{
			leady = mo->y-mo->radius;
		}
		bestslidefrac = FRACUNIT+1;
		if (P_PathTraverse(leadx, leady, leadx+mo->momx, leady+mo->momy,
			PT_ADDLINES, PTR_BounceTraverse))
		{ // Could not find a wall, so bounce off the floor/ceiling instead.
			fixed_t floordist = mo->z - mo->floorz;
			fixed_t ceildist = mo->ceilingz - mo->z;
			if (floordist <= ceildist)
			{
				mo->FloorBounceMissile (mo->Sector->floorplane);
				return true;
			}
			else
			{
				mo->FloorBounceMissile (mo->Sector->ceilingplane);
				return true;
			}
			/*
			else
			{
				return (mo->flags2 & MF2_BOUNCE2) != 0;
			}
			*/
		}
		line = bestslideline;
	}

	if (line->special == Line_Horizon)
	{
		mo->SeeSound = 0;	// it might make a sound otherwise
		mo->Destroy();
		return true;
	}

	side = P_PointOnLineSide (mo->x, mo->y, line);
	lineangle = R_PointToAngle2 (0, 0, line->dx, line->dy);
	if (side == 1)
	{
		lineangle += ANG180;
	}
	moveangle = R_PointToAngle2 (0, 0, mo->momx, mo->momy);
	deltaangle = (2*lineangle)-moveangle;
	mo->angle = deltaangle;

	lineangle >>= ANGLETOFINESHIFT;
	deltaangle >>= ANGLETOFINESHIFT;

	movelen = P_AproxDistance (mo->momx, mo->momy);
	movelen = (movelen * 192) >> 8; // friction

	fixed_t box[4];
	box[BOXTOP] = mo->y + mo->radius;
	box[BOXBOTTOM] = mo->y - mo->radius;
	box[BOXLEFT] = mo->x - mo->radius;
	box[BOXRIGHT] = mo->x + mo->radius;
	if (P_BoxOnLineSide (box, line) == -1)
	{
		mo->SetOrigin (mo->x + FixedMul(mo->radius,
			finecosine[deltaangle]), mo->y + FixedMul(mo->radius, finesine[deltaangle]), mo->z);;
	}
	if (movelen < FRACUNIT)
	{
		movelen = 2*FRACUNIT;
	}
	mo->momx = FixedMul(movelen, finecosine[deltaangle]);
	mo->momy = FixedMul(movelen, finesine[deltaangle]);
	return true;
}


//============================================================================
//
// Aiming
//
//============================================================================
AActor*			linetarget;		// who got hit (or NULL)
AActor*			shootthing;
fixed_t			shootz;			// Height if not aiming up or down
fixed_t			attackrange;
fixed_t			aimpitch;

struct aim_t
{


	fixed_t			toppitch, bottompitch;
	AActor *		thing_friend, * thing_other;
	angle_t			pitch_friend, pitch_other;
	bool			notsmart;
	sector_t *		lastsector;
	secplane_t *	lastfloorplane;
	secplane_t *	lastceilingplane;

	bool			crossedffloors;

};

aim_t aim;

//============================================================================
//
// AimTraverse3DFloors
//
//============================================================================
bool P_AimTraverse3DFloors(intercept_t * in)
{
	sector_t * nextsector;
	secplane_t * nexttopplane, * nextbottomplane;
	line_t * li=in->d.line;

	nextsector=NULL;
	nexttopplane=nextbottomplane=NULL;

	// [BB] In j-ecinvbug.wad I experienced li->backsector == NULL, which obviously leads to a crash.
	if( li->frontsector && li->backsector && (li->frontsector->e->ffloors.Size() || li->backsector->e->ffloors.Size()) )
	{
		int  frontflag;
		F3DFloor* rover;
		int    highpitch, lowpitch;

		fixed_t trX = trace.x + FixedMul (trace.dx, in->frac);
		fixed_t trY = trace.y + FixedMul (trace.dy, in->frac);
		fixed_t dist = FixedMul (attackrange, in->frac);

		
		int dir = aimpitch < 0 ? 1 : aimpitch > 0 ? -1 : 0;
		
		frontflag = P_PointOnLineSide(shootthing->x, shootthing->y, li);
		
		// 3D floor check. This is not 100% accurate but normally sufficient when
		// combined with a final sight check
		for(int i=1;i<=2;i++)
		{
			sector_t * s=i==1? li->frontsector:li->backsector;

			for(unsigned k=0;k<s->e->ffloors.Size();k++)
			{
				aim.crossedffloors=true;
				rover=s->e->ffloors[k];
			
				if(!(rover->flags & FF_SOLID) || !(rover->flags & FF_EXISTS)) continue;
				
				fixed_t ff_bottom=rover->bottom.plane->ZatPoint(trX, trY);
				fixed_t ff_top=rover->top.plane->ZatPoint(trX, trY);
				

				highpitch = -(int)R_PointToAngle2 (0, shootz, dist, ff_top);
				lowpitch = -(int)R_PointToAngle2 (0, shootz, dist, ff_bottom);

				if (highpitch<=aim.toppitch)
				{
					// blocks completely
					if (lowpitch>=aim.bottompitch) return false;	
					// blocks upper edge of view
					if (lowpitch>aim.toppitch) 
					{
						aim.toppitch=lowpitch;
						if (frontflag!=i-1)
						{
							nexttopplane=rover->bottom.plane;
						}
					}
				}
				else if (lowpitch>=aim.bottompitch)
				{
					// blocks lower edge of view
					if (highpitch<aim.bottompitch)  
					{
						aim.bottompitch=highpitch;
						if (frontflag!=i-1)
						{
							nextbottomplane=rover->top.plane;
						}
					}
				}
				// trace is leaving a sector with a 3d-floor

				if (frontflag==i-1)
				{
					if (s==aim.lastsector)
					{
						// upper slope intersects with this 3d-floor
						if (rover->bottom.plane==aim.lastceilingplane && lowpitch > aim.toppitch)
						{
							aim.toppitch=lowpitch;
						}
						// lower slope intersects with this 3d-floor
						if (rover->top.plane==aim.lastfloorplane && highpitch < aim.bottompitch)
						{
							aim.bottompitch=highpitch;
						}
					}
				}
				if (aim.toppitch >= aim.bottompitch) return false;		// stop
			}
		}
    }

	aim.lastsector=nextsector;
	aim.lastceilingplane=nexttopplane;
	aim.lastfloorplane=nextbottomplane;
	return true;
}

//============================================================================
//
// PTR_AimTraverse
// Sets linetaget and aimpitch when a target is aimed at.
//
//============================================================================

bool PTR_AimTraverse (intercept_t* in)
{
	fixed_t &			toppitch=aim.toppitch;
	fixed_t &			bottompitch=aim.bottompitch;

	line_t* 			li;
	AActor* 			th;
	fixed_t 			pitch;
	fixed_t 			thingtoppitch;
	fixed_t 			thingbottompitch;
	fixed_t 			dist;
	int					thingpitch;

	if (in->isaline) 
	{
		li = in->d.line;

		if ( !(li->flags & ML_TWOSIDED) || (li->flags & ML_BLOCKEVERYTHING) )
			return false;				// stop

		// Crosses a two sided line.
		// A two sided line will restrict the possible target ranges.
		P_LineOpening (li, trace.x + FixedMul (trace.dx, in->frac),
			trace.y + FixedMul (trace.dy, in->frac));

		if (openbottom >= opentop)
			return false;				// stop

		dist = FixedMul (attackrange, in->frac);

		pitch = -(int)R_PointToAngle2 (0, shootz, dist, openbottom);
		if (pitch < bottompitch)
			bottompitch = pitch;

		pitch = -(int)R_PointToAngle2 (0, shootz, dist, opentop);
		if (pitch > toppitch)
			toppitch = pitch;

		if (toppitch >= bottompitch)
			return false;				// stop
						
		return P_AimTraverse3DFloors(in);
	}

	// shoot a thing
	th = in->d.thing;
	if (th == shootthing)
		return true;					// can't shoot self

	if (!(th->flags&MF_SHOOTABLE))
		return true;					// corpse or something

	// check for physical attacks on a ghost
	if ((th->flags3 & MF3_GHOST) && 
		shootthing->player &&	// [RH] Be sure shootthing is a player
		shootthing->player->ReadyWeapon &&
		(shootthing->player->ReadyWeapon->flags2 & MF2_THRUGHOST))
	{
		return true;
	}
		
	dist = FixedMul (attackrange, in->frac);

	// we must do one last check whether the trace has crossed a 3D floor
	if (aim.lastsector==th->Sector && th->Sector->e->ffloors.Size())
	{
		if (aim.lastceilingplane)
		{
			fixed_t ff_top=aim.lastceilingplane->ZatPoint(th->x, th->y);
			fixed_t pitch = -(int)R_PointToAngle2 (0, shootz, dist, ff_top);
			// upper slope intersects with this 3d-floor
			if (pitch > toppitch)
			{
				toppitch=pitch;
			}
		}
		if (aim.lastfloorplane)
		{
			fixed_t ff_bottom=aim.lastfloorplane->ZatPoint(th->x, th->y);
			fixed_t pitch = -(int)R_PointToAngle2 (0, shootz, dist, ff_bottom);
			// lower slope intersects with this 3d-floor
			if (pitch < bottompitch)
			{
				bottompitch=pitch;
			}
		}
	}

	// check angles to see if the thing can be aimed at

	thingtoppitch = -(int)R_PointToAngle2 (0, shootz, dist, th->z + th->height);

	if (thingtoppitch > bottompitch)
		return true;					// shot over the thing

	thingbottompitch = -(int)R_PointToAngle2 (0, shootz, dist, th->z);

	if (thingbottompitch < toppitch)
		return true;					// shot under the thing
		
	if (aim.crossedffloors)
	{
		// if 3D floors were in the way do an extra visibility check for safety
		if (!P_CheckSight(shootthing, th, 1)) 
		{
			// the thing can't be seen so we can safely exclude its range from our aiming field
			if (thingtoppitch<toppitch) 
			{
				if (thingbottompitch>toppitch) toppitch=thingbottompitch;
			}
			else if (thingbottompitch>bottompitch)
			{
				if (thingtoppitch<bottompitch) bottompitch=thingtoppitch;
			}
			return toppitch<bottompitch;
		}
	}

	// this thing can be hit!
	if (thingtoppitch < toppitch)
		thingtoppitch = toppitch;

	if (thingbottompitch > bottompitch)
		thingbottompitch = bottompitch;
	
	thingpitch = thingtoppitch/2 + thingbottompitch/2;

	if (sv_smartaim && !aim.notsmart)
	{
		// try to be a little smarter about what to aim at!
		// In particular avoid autoaiming at friends amd barrels.
		if (th->IsFriend(shootthing))
		{
			if (sv_smartaim < 2)
			{
				// friends don't aim at friends (except players), at least not first
				aim.thing_friend=th;
				aim.pitch_friend=thingpitch;
			}
		}
		else if (!(th->flags3&MF3_ISMONSTER) )
		{
			if (sv_smartaim < 3)
			{
				// don't autoaim at barrels and other shootable stuff unless no monsters have been found
				aim.thing_other=th;
				aim.pitch_other=thingpitch;
			}
		}
		else
		{
			linetarget=th;
			aimpitch=thingpitch;
			return false;
		}
	}
	else
	{
		linetarget=th;
		aimpitch=thingpitch;
		return false;
	}
	return true;
}

//============================================================================
//
// P_AimLineAttack
//
//============================================================================
fixed_t P_AimLineAttack (AActor *t1, angle_t angle, fixed_t distance, fixed_t vrange, bool forcenosmart)
{
	fixed_t x2;
	fixed_t y2;

	angle >>= ANGLETOFINESHIFT;
	shootthing = t1;

	x2 = t1->x + (distance>>FRACBITS)*finecosine[angle];
	y2 = t1->y + (distance>>FRACBITS)*finesine[angle];
	shootz = t1->z + (t1->height>>1) - t1->floorclip;
	if (t1->player != NULL)
	{
		shootz += FixedMul (t1->player->mo->AttackZOffset, t1->player->crouchfactor);
	}
	else
	{
		shootz += 8*FRACUNIT;
	}

	// can't shoot outside view angles
	if (vrange == 0)
	{
		if (t1->player == NULL || dmflags & DF_NO_FREELOOK)
		{
			vrange = ANGLE_1*35;
		}
		else
		{
			// 35 degrees is approximately what Doom used. You cannot have a
			// vrange of 0 degrees, because then toppitch and bottompitch will
			// be equal, and PTR_AimTraverse will never find anything to shoot at
			// if it crosses a line.
			vrange = clamp (t1->player->userinfo.aimdist, ANGLE_1/2, ANGLE_1*35);
		}
	}
	aim.toppitch = t1->pitch - vrange;
	aim.bottompitch = t1->pitch + vrange;
	aim.notsmart = forcenosmart;

	attackrange = distance;
	linetarget = NULL;

	// for smart aiming
	aim.thing_friend=aim.thing_other=NULL;

	// Information for tracking crossed 3D floors
	aimpitch=t1->pitch;
	aim.crossedffloors=t1->Sector->e->ffloors.Size()!=0;
	aim.lastsector=t1->Sector;
	aim.lastfloorplane=aim.lastceilingplane=NULL;

	// set initial 3d-floor info
	for(unsigned i=0;i<t1->Sector->e->ffloors.Size();i++)
	{
		F3DFloor * rover=t1->Sector->e->ffloors[i];
		fixed_t bottomz=rover->bottom.plane->ZatPoint(t1->x, t1->y);

		if (bottomz>=t1->z+t1->height) aim.lastceilingplane=rover->bottom.plane;

		bottomz=rover->top.plane->ZatPoint(t1->x, t1->y);
		if (bottomz<=t1->z) aim.lastfloorplane=rover->top.plane;
	}

	P_PathTraverse (t1->x, t1->y, x2, y2, PT_ADDLINES|PT_ADDTHINGS, PTR_AimTraverse);

	if (!linetarget) 
	{
		if (aim.thing_other)
		{
			linetarget=aim.thing_other;
			aimpitch=aim.pitch_other;
		}
		else if (aim.thing_friend)
		{
			linetarget=aim.thing_friend;
			aimpitch=aim.pitch_friend;
		}
	}
	return linetarget ? aimpitch : t1->pitch;
}


/*
=================
=
= P_LineAttack
=
= if damage == 0, it is just a test trace that will leave linetarget set
=
=================
*/

static bool CheckForGhost (FTraceResults &res)
{
	if (res.HitType != TRACE_HitActor)
	{
		return false;
	}

	// check for physical attacks on a ghost
	if (res.Actor->flags3 & MF3_GHOST || res.Actor->flags4 & MF4_SPECTRAL)
	{
		res.HitType = TRACE_HitNone;
		return true;
	}

	return false;
}

static bool CheckForSpectral (FTraceResults &res)
{
	if (res.HitType != TRACE_HitActor)
	{
		return false;
	}

	// check for physical attacks on spectrals
	if (res.Actor->flags4 & MF4_SPECTRAL)
	{
		res.HitType = TRACE_HitNone;
		return true;
	}

	return false;
}

void P_LineAttack (AActor *t1, angle_t angle, fixed_t distance,
				   int pitch, int damage, FName damageType, const PClass *pufftype)
{
	// [BB] The only reason the client should try to execute P_LineAttack, is the online hitscan decal fix. 
	if ((( NETWORK_GetState( ) == NETSTATE_CLIENT ) || ( CLIENTDEMO_IsPlaying( ))) &&
		( cl_hitscandecalhack == false ))
	{
		return;
	}
	fixed_t vx, vy, vz, shootz;
	FTraceResults trace;
	angle_t srcangle = angle;
	int srcpitch = pitch;
	bool hitGhosts;
	bool killPuff = false;
	AActor *puff = NULL;

	angle >>= ANGLETOFINESHIFT;
	pitch = (angle_t)(pitch) >> ANGLETOFINESHIFT;

	vx = FixedMul (finecosine[pitch], finecosine[angle]);
	vy = FixedMul (finecosine[pitch], finesine[angle]);
	vz = -finesine[pitch];

	shootz = t1->z - t1->floorclip + (t1->height>>1);
	if (t1->player != NULL)
	{
		shootz += FixedMul (t1->player->mo->AttackZOffset, t1->player->crouchfactor);
	}
	else
	{
		shootz += 8*FRACUNIT;
	}
	attackrange = distance;
	aimpitch = pitch;

	hitGhosts = (t1->player != NULL &&
		t1->player->ReadyWeapon != NULL &&
		(t1->player->ReadyWeapon->flags2 & MF2_THRUGHOST));

	if (!Trace (t1->x, t1->y, shootz, t1->Sector, vx, vy, vz, distance,
		MF_SHOOTABLE, ML_BLOCKEVERYTHING, t1, trace,
		TRACE_NoSky|TRACE_Impact, hitGhosts ? CheckForGhost : CheckForSpectral))
	{ // hit nothing
		// [BB] No decal will be spawned, so the client stops here. 
		if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
			( CLIENTDEMO_IsPlaying( )))
		{
			return;
		}
		AActor *puffDefaults = GetDefaultByType (pufftype);
		if (puffDefaults->ActiveSound)
		{ // Play miss sound
			S_SoundID (t1, CHAN_WEAPON, puffDefaults->ActiveSound, 1, ATTN_NORM);

			// [BC] Play the hit sound to clients.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SoundActor( t1, CHAN_WEAPON, S_GetName( puffDefaults->ActiveSound ), 1, ATTN_NORM );
		}
		if (puffDefaults->flags3 & MF3_ALWAYSPUFF)
		{ // Spawn the puff anyway
			P_SpawnPuff (pufftype, trace.X, trace.Y, trace.Z, angle - ANG180, 2);
		}
		else
		{
			return;
		}
	}
	else
	{
		fixed_t hitx = 0, hity = 0, hitz = 0;

		if (trace.HitType != TRACE_HitActor)
		{
			// [BB] The client only spawns decals, no puffs.
			if (( NETWORK_GetState( ) != NETSTATE_CLIENT ) &&
				( CLIENTDEMO_IsPlaying( ) == false ))
			{
				// position a bit closer for puffs
				if (trace.HitType != TRACE_HitWall || trace.Line->special != Line_Horizon)
				{
					fixed_t closer = trace.Distance - 4*FRACUNIT;
					puff = P_SpawnPuff (pufftype, t1->x + FixedMul (vx, closer),
						t1->y + FixedMul (vy, closer),
						shootz + FixedMul (vz, closer), angle - ANG90, 0);
				}
			}

			// [RH] Spawn a decal
			if (trace.HitType == TRACE_HitWall && trace.Line->special != Line_Horizon)
			{
				SpawnShootDecal (t1, trace);
			}
			else if (puff != NULL &&
				trace.CrossedWater == NULL &&
				trace.Sector->heightsec == NULL &&
				trace.HitType == TRACE_HitFloor)
			{
				P_HitWater (puff, trace.Sector);
			}
			// [BB] Decal has been spawned, so the client stops here. 
			if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
				( CLIENTDEMO_IsPlaying( )))
			{
				return;
			}
		}
		else
		{
			// [BB] No decal will be spawned, so the client stops here. 
			if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
				( CLIENTDEMO_IsPlaying( )))
			{
				return;
			}

			bool bloodsplatter = (t1->flags5 & MF5_BLOODSPLATTER) ||
									(t1->player != NULL &&	t1->player->ReadyWeapon != NULL &&
										(t1->player->ReadyWeapon->WeaponFlags & WIF_AXEBLOOD));

			bool axeBlood = (t1->player != NULL &&
				t1->player->ReadyWeapon != NULL &&
				(t1->player->ReadyWeapon->WeaponFlags & WIF_AXEBLOOD));

			// Hit a thing, so it could be either a puff or blood
			hitx = t1->x + FixedMul (vx, trace.Distance);
			hity = t1->y + FixedMul (vy, trace.Distance);
			hitz = shootz + FixedMul (vz, trace.Distance);

			// Spawn bullet puffs or blood spots, depending on target type.
			AActor *puffDefaults = GetDefaultByType (pufftype);
			if ((puffDefaults->flags3 & MF3_PUFFONACTORS) ||
				(trace.Actor->flags & MF_NOBLOOD) ||
				(trace.Actor->flags2 & (MF2_INVULNERABLE|MF2_DORMANT)))
			{
				puff = P_SpawnPuff (pufftype, hitx, hity, hitz, angle - ANG180, 2, true);
			}
			if (!(GetDefaultByType(pufftype)->flags3&MF3_BLOODLESSIMPACT))
			{
				if (!bloodsplatter && !axeBlood &&
					!(trace.Actor->flags & MF_NOBLOOD) &&
					!(trace.Actor->flags2 & (MF2_INVULNERABLE|MF2_DORMANT)))
				{
					P_SpawnBlood (hitx, hity, hitz, angle - ANG180, damage, trace.Actor);
				}
	
				if (damage)
				{
					if (bloodsplatter || axeBlood)
					{
						if (!(trace.Actor->flags&MF_NOBLOOD) &&
							!(trace.Actor->flags2&(MF2_INVULNERABLE|MF2_DORMANT)))
						{
							if (axeBlood)
							{
								P_BloodSplatter2 (hitx, hity, hitz, trace.Actor);
							}
							if (pr_lineattack() < 192)
							{
								P_BloodSplatter (hitx, hity, hitz, trace.Actor);
							}
						}
					}

					// [BC] If the attacker is a player and struck another player, flag it.
					if (( trace.Actor->player ) && ( t1->player ) && ( t1->IsTeammate( trace.Actor ) == false ))
						t1->player->bStruckPlayer = true;

					// [RH] Stick blood to walls
					P_TraceBleed (damage, trace.X, trace.Y, trace.Z,
						trace.Actor, srcangle, srcpitch);
				}
			}
			if (damage) 
			{
				int flags = 0;
				// Allow MF5_PIERCEARMOR on a weapon as well.
				if (t1->player != NULL && t1->player->ReadyWeapon != NULL &&
					t1->player->ReadyWeapon->flags5 & MF5_PIERCEARMOR)
				{
					flags |= DMG_NO_ARMOR;
				}
			
				if (puff == NULL)
				{ 
					// Since the puff is the damage inflictor we need it here 
					// regardless of whether it is displayed or not.
					puff = P_SpawnPuff (pufftype, hitx, hity, hitz, angle - ANG180, 2, true, false);
					killPuff = true;
				}
				P_DamageMobj (trace.Actor, puff ? puff : t1, t1, damage, damageType, flags);
			}
		}
		if (trace.CrossedWater)
		{

			if (puff == NULL)
			{ // Spawn puff just to get a mass for the splash
				puff = P_SpawnPuff (pufftype, hitx, hity, hitz, angle - ANG180, 2, true, false);
				killPuff = true;
			}
			SpawnDeepSplash (t1, trace, puff, vx, vy, vz);
		}
	}
	if (killPuff && puff != NULL)
	{
		puff->Destroy();
	}
}

void P_LineAttack (AActor *t1, angle_t angle, fixed_t distance,
				   int pitch, int damage, FName damageType, FName pufftype)
{
	const PClass * type = PClass::FindClass(pufftype);
	if (type == NULL)
	{
		Printf("Attempt to spawn unknown actor type '%s'\n", pufftype.GetChars());
	}
	else
	{
		P_LineAttack(t1, angle, distance, pitch, damage, damageType, type);
	}
}

void P_TraceBleed (int damage, fixed_t x, fixed_t y, fixed_t z, AActor *actor, angle_t angle, int pitch)
{
	if (!cl_bloodsplats)
		return;

	const char *bloodType = "BloodSplat";
	int count;
	int noise;

	if ((actor->flags & MF_NOBLOOD) ||
		(actor->flags5 & MF5_NOBLOODDECALS) ||
		(actor->flags2 & (MF2_INVULNERABLE|MF2_DORMANT)) ||
		(actor->player && actor->player->cheats & CF_GODMODE))
	{
		return;
	}
	if (damage < 15)
	{	// For low damages, there is a chance to not spray blood at all
		if (damage <= 10)
		{
			if (pr_tracebleed() < 160)
			{
				return;
			}
		}
		count = 1;
		noise = 18;
	}
	else if (damage < 25)
	{
		count = 2;
		noise = 19;
	}
	else
	{	// For high damages, there is a chance to spray just one big glob of blood
		if (pr_tracebleed() < 24)
		{
			bloodType = "BloodSmear";
			count = 1;
			noise = 20;
		}
		else
		{
			count = 3;
			noise = 20;
		}
	}

	for (; count; --count)
	{
		FTraceResults bleedtrace;

		angle_t bleedang = (angle + ((pr_tracebleed()-128) << noise)) >> ANGLETOFINESHIFT;
		angle_t bleedpitch = (angle_t)(pitch + ((pr_tracebleed()-128) << noise)) >> ANGLETOFINESHIFT;
		fixed_t vx = FixedMul (finecosine[bleedpitch], finecosine[bleedang]);
		fixed_t vy = FixedMul (finecosine[bleedpitch], finesine[bleedang]);
		fixed_t vz = -finesine[bleedpitch];

		if (Trace (x, y, z, actor->Sector,
					vx, vy, vz, 172*FRACUNIT, 0, ML_BLOCKEVERYTHING, actor,
					bleedtrace, TRACE_NoSky))
		{
			if (bleedtrace.HitType == TRACE_HitWall)
			{
				PalEntry bloodcolor = (PalEntry)actor->GetClass()->Meta.GetMetaInt(AMETA_BloodColor);
				if (bloodcolor != 0)
				{
					bloodcolor.r>>=1;	// the full color is too bright for blood decals
					bloodcolor.g>>=1;
					bloodcolor.b>>=1;
					bloodcolor.a=1;
				}

				// [BC] Servers don't need to spawn decals.
				if ( NETWORK_GetState( ) != NETSTATE_SERVER )
				{
					DImpactDecal::StaticCreate (bloodType,
						bleedtrace.X, bleedtrace.Y, bleedtrace.Z,
						sides + bleedtrace.Line->sidenum[bleedtrace.Side],
						bleedtrace.ffloor,
						bloodcolor);
				}
			}
		}
	}
}

void P_TraceBleed (int damage, AActor *target, angle_t angle, int pitch)
{
	P_TraceBleed (damage, target->x, target->y, target->z + target->height/2,
		target, angle, pitch);
}

void P_TraceBleed (int damage, AActor *target, AActor *missile)
{
	int pitch;

	if (target == NULL || missile->flags3 & MF3_BLOODLESSIMPACT)
	{
		return;
	}

	if (missile->momz != 0)
	{
		double aim;

		aim = atan ((double)missile->momz / (double)P_AproxDistance (missile->x - target->x, missile->y - target->y));
		pitch = -(int)(aim * ANGLE_180/PI);
	}
	else
	{
		pitch = 0;
	}
	P_TraceBleed (damage, target->x, target->y, target->z + target->height/2,
		target, R_PointToAngle2 (missile->x, missile->y, target->x, target->y),
		pitch);
}

void P_TraceBleed (int damage, AActor *target)
{
	fixed_t one = pr_tracebleed() << 24;
	fixed_t two = (pr_tracebleed()-128) << 16;

	P_TraceBleed (damage, target->x, target->y, target->z + target->height/2,
		target, one, two);
}

//
// [RH] Rail gun stuffage
//
struct SRailHit
{
	AActor *HitActor;
	fixed_t Distance;
};
static TArray<SRailHit> RailHits (16);

static bool ProcessRailHit (FTraceResults &res)
{
	if (res.HitType != TRACE_HitActor)
	{
		return false;
	}

	// Invulnerable things completely block the shot
	if (res.Actor->flags2 & MF2_INVULNERABLE)
	{
		return false;
	}

	// Save this thing for damaging later, and continue the trace
	SRailHit newhit;
	newhit.HitActor = res.Actor;
	newhit.Distance = res.Distance - 10*FRACUNIT;	// put blood in front
	RailHits.Push (newhit);

	return true;
}

void P_RailAttack (AActor *source, int damage, int offset, int color1, int color2, float maxdiff, bool silent, FName puff)
{
	fixed_t vx, vy, vz;
	angle_t angle, pitch;
	fixed_t x1, y1;
	FVector3 start, end;
	FTraceResults trace;
	bool			bHitPlayer;

	pitch = (angle_t)(-source->pitch) >> ANGLETOFINESHIFT;
	angle = source->angle >> ANGLETOFINESHIFT;

	vx = FixedMul (finecosine[pitch], finecosine[angle]);
	vy = FixedMul (finecosine[pitch], finesine[angle]);
	vz = finesine[pitch];

	x1 = source->x;
	y1 = source->y;
	shootz = source->z - source->floorclip + (source->height >> 1) + 8*FRACUNIT;

	angle = (source->angle - ANG90) >> ANGLETOFINESHIFT;
	x1 += offset*finecosine[angle];
	y1 += offset*finesine[angle];

	RailHits.Clear ();
	start.X = FIXED2FLOAT(x1);
	start.Y = FIXED2FLOAT(y1);
	start.Z = FIXED2FLOAT(shootz);

	Trace (x1, y1, shootz, source->Sector, vx, vy, vz,
		8192*FRACUNIT, MF_SHOOTABLE, ML_BLOCKEVERYTHING, source, trace,
		TRACE_PCross|TRACE_Impact, ProcessRailHit);

	if (trace.HitType == TRACE_HitWall)
	{
		SpawnShootDecal (source, trace);
	}
	if (trace.HitType == TRACE_HitFloor &&
		trace.CrossedWater == NULL &&
		trace.Sector->heightsec == NULL)
	{
		fixed_t savex, savey, savez;
		fixed_t savefloor, saveceil, savedropoff;
		int savefloorpic;
		sector_t *savefloorsec;
		int saveceilingpic;
		sector_t *saveceilingsec;

		savex = source->x;
		savey = source->y;
		savez = source->z;
		savefloor = source->floorz;
		saveceil = source->ceilingz;
		savedropoff = source->dropoffz;
		savefloorpic = source->floorpic;
		savefloorsec = source->floorsector;
		saveceilingpic = source->ceilingpic;
		saveceilingsec = source->ceilingsector;

		source->SetOrigin (trace.X, trace.Y, trace.Z);
		P_HitWater (source, trace.Sector);
		source->SetOrigin (savex, savey, savez);

		source->floorz = savefloor;
		source->ceilingz = saveceil;
		source->dropoffz = savedropoff;
		source->floorpic = savefloorpic;
		source->floorsector = savefloorsec;
		source->ceilingpic = saveceilingpic;
		source->ceilingsector = saveceilingsec;
	}
	if (trace.CrossedWater)
	{
		AActor *thepuff = Spawn ("BulletPuff", 0, 0, 0, ALLOW_REPLACE);
		if (thepuff != NULL)
		{
			SpawnDeepSplash (source, trace, thepuff, vx, vy, vz);
			thepuff->Destroy ();
		}
	}

	// Now hurt anything the trace hit
	unsigned int i;

	// Initialize bHitPlayer.
	bHitPlayer = false;

	for (i = 0; i < RailHits.Size (); i++)
	{
		fixed_t x, y, z;

		x = x1 + FixedMul (RailHits[i].Distance, vx);
		y = y1 + FixedMul (RailHits[i].Distance, vy);
		z = shootz + FixedMul (RailHits[i].Distance, vz);

		if ((RailHits[i].HitActor->flags & MF_NOBLOOD) ||
			(RailHits[i].HitActor->flags2 & (MF2_DORMANT|MF2_INVULNERABLE)))
		{
			const PClass *puffclass = PClass::FindClass(puff);
			if (puffclass != NULL) P_SpawnPuff (puffclass, x, y, z, source->angle - ANG180, 1, true);
		}
		else
		{
			P_SpawnBlood (x, y, z, source->angle - ANG180, damage, RailHits[i].HitActor);
		}
		// Damage is server side.
		if (( NETWORK_GetState( ) != NETSTATE_CLIENT ) && ( CLIENTDEMO_IsPlaying( ) == false ))
			P_DamageMobj (RailHits[i].HitActor, source, source, damage, NAME_Railgun, DMG_NO_ARMOR);
		P_TraceBleed (damage, x, y, z, RailHits[i].HitActor, angle, pitch);

		if (( RailHits[i].HitActor->player ) && ( source->IsTeammate( RailHits[i].HitActor ) == false ))
		{
			bHitPlayer = true;
			if ( source->player )
			{
				source->player->ulConsecutiveRailgunHits++;

				// If the player has made 2 straight consecutive hits with the railgun, award a medal.
				if (( source->player->ulConsecutiveRailgunHits % 2 ) == 0 )
				{
					// If the player gets 4+ straight hits with the railgun, award a "Most Impressive" medal.
					if ( source->player->ulConsecutiveRailgunHits >= 4 )
					{
						if (( NETWORK_GetState( ) != NETSTATE_CLIENT ) && ( CLIENTDEMO_IsPlaying( ) == false ))
							MEDAL_GiveMedal( ULONG( source->player - players ), MEDAL_MOSTIMPRESSIVE );

						// Tell clients about the medal that been given.
						if ( NETWORK_GetState( ) == NETSTATE_SERVER )
							SERVERCOMMANDS_GivePlayerMedal( ULONG( source->player - players ), MEDAL_MOSTIMPRESSIVE );
					}
					// Otherwise, award an "Impressive" medal.
					else
					{
						if (( NETWORK_GetState( ) != NETSTATE_CLIENT ) && ( CLIENTDEMO_IsPlaying( ) == false ))
							MEDAL_GiveMedal( ULONG( source->player - players ), MEDAL_IMPRESSIVE );

						// Tell clients about the medal that been given.
						if ( NETWORK_GetState( ) == NETSTATE_SERVER )
							SERVERCOMMANDS_GivePlayerMedal( ULONG( source->player - players ), MEDAL_IMPRESSIVE );
					}
				}
			}
		}
	}

	if ( source->player )
	{
		// Player did not strike a player with his railgun. Reset consecutive hits to 0.
		if ( bHitPlayer == false )
			source->player->ulConsecutiveRailgunHits = 0;
	}

	end.X = FIXED2FLOAT(trace.X);
	end.Y = FIXED2FLOAT(trace.Y);
	end.Z = FIXED2FLOAT(trace.Z);
	P_DrawRailTrail (source, start, end, color1, color2, maxdiff, silent);

	// [BC] If we're the server, tell clients to create a railgun trail.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_WeaponRailgun( source, start, end, color1, color2, maxdiff, silent );
}

void P_RailAttackWithPossibleSpread (AActor *source, int damage, int offset, int color1, int color2, float maxdiff, bool silent, FName puff)
{
	P_RailAttack (source, damage, offset, color1, color2, maxdiff, silent, puff );

	// [BB] Apply spread.
	if (NULL != source->player )
	{
		if ( source->player->cheats & CF_SPREAD )
		{
			fixed_t		SavedActorAngle;

			SavedActorAngle = source->angle;

			source->angle += ( ANGLE_45 / 3 );
			P_RailAttack (source, damage, offset, color1, color2, maxdiff, silent, puff );
			source->angle = SavedActorAngle;

			source->angle -= ( ANGLE_45 / 3 );
			P_RailAttack (source, damage, offset, color1, color2, maxdiff, silent, puff );
			source->angle = SavedActorAngle;
		}
	}
}
//
// [RH] P_AimCamera
//
CVAR (Float, chase_height, -8.f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Float, chase_dist, 90.f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
fixed_t CameraX, CameraY, CameraZ;
sector_t *CameraSector;

void P_AimCamera (AActor *t1)
{
	fixed_t distance = (fixed_t)(chase_dist * FRACUNIT);
	angle_t angle = (t1->angle - ANG180) >> ANGLETOFINESHIFT;
	angle_t pitch = (angle_t)(t1->pitch) >> ANGLETOFINESHIFT;
	FTraceResults trace;
	fixed_t vx, vy, vz, sz;

	vx = FixedMul (finecosine[pitch], finecosine[angle]);
	vy = FixedMul (finecosine[pitch], finesine[angle]);
	vz = finesine[pitch];

	sz = t1->z - t1->floorclip + t1->height + (fixed_t)(chase_height * FRACUNIT);

	if (Trace (t1->x, t1->y, sz, t1->Sector,
		vx, vy, vz, distance, 0, 0, NULL, trace) &&
		trace.Distance > 10*FRACUNIT)
	{
		// Position camera slightly in front of hit thing
		fixed_t dist = trace.Distance - 5*FRACUNIT;
		CameraX = t1->x + FixedMul (vx, dist);
		CameraY = t1->y + FixedMul (vy, dist);
		CameraZ = sz + FixedMul (vz, dist);
	}
	else
	{
		CameraX = trace.X;
		CameraY = trace.Y;
		CameraZ = trace.Z;
	}
	CameraSector = trace.Sector;
}


//
// USE LINES
//
AActor *usething;
bool foundline;

bool PTR_UseTraverse (intercept_t *in)
{
	// [RH] Check for things to talk with or use a puzzle item on
	if (!in->isaline)
	{
		if (usething==in->d.thing) return true;
		// Check thing

		// Check for puzzle item use or USESPECIAL flag
		if (in->d.thing->flags5 & MF5_USESPECIAL || in->d.thing->special == USE_PUZZLE_ITEM_SPECIAL)
		{
			if (LineSpecials[in->d.thing->special] (NULL, usething, false,
				in->d.thing->args[0], in->d.thing->args[1], in->d.thing->args[2],
				in->d.thing->args[3], in->d.thing->args[4]))
				return false;
		}
		// Dead things can't talk.
		if (in->d.thing->health <= 0)
		{
			return true;
		}
		// Fighting things don't talk either.
		if (in->d.thing->flags4 & MF4_INCOMBAT)
		{
			return true;
		}
		if (in->d.thing->Conversation != NULL)
		{
			// Give the NPC a chance to play a brief animation
			in->d.thing->ConversationAnimation (0);
			P_StartConversation (in->d.thing, usething, true, true);
			return false;
		}
		return true;
	}

	// [RH] The range passed to P_PathTraverse was doubled so that it could
	// find things up to 128 units away (for Strife), but it should still reject
	// lines further than 64 units away.
	if (in->frac > FRACUNIT/2)
	{
		P_LineOpening (in->d.line, trace.x + FixedMul (trace.dx, in->frac),
			trace.y + FixedMul (trace.dy, in->frac));
		return openrange>0;
	}

	if (in->d.line->special == 0 || (GET_SPAC(in->d.line->flags) != SPAC_USETHROUGH &&
		GET_SPAC(in->d.line->flags) != SPAC_USE))
	{
blocked:
		if (in->d.line->flags & ML_BLOCKEVERYTHING)
		{
			openrange = 0;
		}
		else
		{
			P_LineOpening (in->d.line, trace.x + FixedMul (trace.dx, in->frac),
				trace.y + FixedMul (trace.dy, in->frac));
		}
		if (openrange <= 0 ||
			(in->d.line->special != 0 && (i_compatflags & COMPATF_USEBLOCKING)))
		{
			// [RH] Give sector a chance to intercept the use

			sector_t * sec;

			sec = usething->Sector;

			// [BC] Just skip right over this in client mode so we don't try to trigger any
			// actions.
			if (( NETWORK_GetState( ) != NETSTATE_CLIENT ) && ( CLIENTDEMO_IsPlaying( ) == false ) && sec->SecActTarget && sec->SecActTarget->TriggerAction (usething, SECSPAC_Use))
			{
				return false;
			}

			sec = P_PointOnLineSide(usething->x, usething->y, in->d.line) == 0?
				in->d.line->frontsector : in->d.line->backsector;

			if (sec != NULL && sec->SecActTarget &&
				sec->SecActTarget->TriggerAction (usething, SECSPAC_UseWall))
			{
				return false;
			}

			if (usething->player)
			{
				// [BC] Tell clients of the "oof" sound.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_SoundActor( slidemo, CHAN_VOICE, "*grunt", 1, ATTN_IDLE, ULONG( usething->player - players ), SVCF_SKIPTHISCLIENT );

				S_Sound (usething, CHAN_VOICE, "*usefail", 1, ATTN_IDLE);
			}
			return false;		// can't use through a wall
		}
		foundline = true;
		return true;			// not a special line, but keep checking
	}
		
	if (P_PointOnLineSide (usething->x, usething->y, in->d.line) == 1)
		// [RH] continue traversal for two-sided lines
		//return in->d.line->backsector != NULL;		// don't use back side
		goto blocked;	// do a proper check for back sides of triggers
		
	P_ActivateLine (in->d.line, usething, 0, SPAC_USE);

	//WAS can't use more than one special line in a row
	//jff 3/21/98 NOW multiple use allowed with enabling line flag
	//[RH] And now I've changed it again. If the line is of type
	//	   SPAC_USE, then it eats the use. Everything else passes
	//	   it through, including SPAC_USETHROUGH.
	if (i_compatflags & COMPATF_USEBLOCKING)
	{
		return GET_SPAC(in->d.line->flags) == SPAC_USETHROUGH;
	}
	else
	{
		return GET_SPAC(in->d.line->flags) != SPAC_USE;
	}
}

// Returns false if a "oof" sound should be made because of a blocking
// linedef. Makes 2s middles which are impassable, as well as 2s uppers
// and lowers which block the player, cause the sound effect when the
// player tries to activate them. Specials are excluded, although it is
// assumed that all special linedefs within reach have been considered
// and rejected already (see P_UseLines).
//
// by Lee Killough
//

bool PTR_NoWayTraverse (intercept_t *in)
{
	line_t *ld = in->d.line;

	// [GrafZahl] de-obfuscated. Was I the only one who was unable to makes sense out of
	// this convoluted mess?
	if (ld->special) return true;
	if (ld->flags&(ML_BLOCKING|ML_BLOCKEVERYTHING)) return false;
	P_LineOpening(ld, trace.x+FixedMul(trace.dx, in->frac),trace.y+FixedMul(trace.dy, in->frac));
	return  openrange >0 && 
			openbottom <= usething->z + usething->MaxStepHeight &&
			opentop >= usething->z + usething->height;
}

//
// PTR_UsethingTraverse
//

bool PTR_UsethingTraverse (intercept_t* in)
{
	AActor	 			*pUseTarget;
	fixed_t 			thingtoppitch;
	fixed_t 			thingbottompitch;
	fixed_t 			dist;
	fixed_t				usez;

	usez = usething->z + (usething->height >> 1) + 8*FRACUNIT;

	// [BC] I'm not sure... will pUseTarget always be valid?			
	pUseTarget = in->d.thing;

	// Can't activate ourselves.
	if ( pUseTarget == usething )
		return ( true );

	// Don't try to activate non-solid objects.
	if (( pUseTarget->flags & MF_SOLID ) == false )
		return ( true );
	
	// check angles to see if the thing can be aimed at
	dist = FixedMul (USERANGE, in->frac);

	thingtoppitch = -(int)R_PointToAngle2( 0, usez, dist, pUseTarget->z + pUseTarget->height);

	// Too far above the object.
	if ( thingtoppitch > aim.bottompitch )
		return ( true );

	thingbottompitch = -(int)R_PointToAngle2( 0, usez, dist, pUseTarget->z );

	// Too far below the object.
	if ( thingbottompitch < aim.toppitch )
		return ( true );
	
	// this thing can be hit!
	if ( thingtoppitch < aim.toppitch )
		thingtoppitch = aim.toppitch;

	if ( thingbottompitch > aim.bottompitch )
		thingbottompitch = aim.bottompitch;

	aimpitch = thingtoppitch/2 + thingbottompitch/2;
	linetarget = pUseTarget;

	// Don't go any farther.
	return ( false );
/*
	// check angles to see if the thing can be aimed at
	dist = FixedMul (USERANGE, in->frac);
	thingtopslope = FixedDiv (th->z+th->height - usez , dist);

//	if (thingtopslope < bottomslope)
//		return true;

	thingbottomslope = FixedDiv (th->z - usez, dist);

//	if (thingbottomslope > topslope)
//		return true;

//	if (thingtopslope > topslope)
//		thingtopslope = topslope;
	
//	if (thingbottomslope < bottomslope)
//		thingbottomslope = bottomslope;

	linetarget = th;

	return false;
*/
}

/*
================
=
= P_UseLines
=
= Looks for special lines in front of the player to activate
================
*/

void P_UseLines (player_t *player)
{
	angle_t angle;
	fixed_t x1, y1, x2, y2;

	usething = player->mo;
	if ( usething == NULL )
		return;

	foundline = false;

	angle = player->mo->angle >> ANGLETOFINESHIFT;
	x1 = player->mo->x;
	y1 = player->mo->y;
	x2 = x1 + (USERANGE>>FRACBITS)*finecosine[angle]*2;
	y2 = y1 + (USERANGE>>FRACBITS)*finesine[angle]*2;

	// old code:
	//
	// P_PathTraverse ( x1, y1, x2, y2, PT_ADDLINES, PTR_UseTraverse );
	//
	// This added test makes the "oof" sound work on 2s lines -- killough:

	if (P_PathTraverse (x1, y1, x2, y2, PT_ADDLINES|PT_ADDTHINGS, PTR_UseTraverse))
	{ // [RH] Give sector a chance to eat the use
		sector_t *sec = usething->Sector;
		int spac = SECSPAC_Use;
		if (foundline)
			spac |= SECSPAC_UseWall;

		// [BC] Don't try to trigger sector actions in client mode.
		if ((( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
			( CLIENTDEMO_IsPlaying( )) ||
			!sec->SecActTarget ||
			 !sec->SecActTarget->TriggerAction (usething, spac)) &&
			!P_PathTraverse (x1, y1, x2, y2, PT_ADDLINES, PTR_NoWayTraverse))
		{
			S_Sound (usething, CHAN_VOICE, "*usefail", 1, ATTN_IDLE);

			// [BC] Tell clients of the "oof" sound.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SoundActor( usething, CHAN_VOICE, "*usefail", 1, ATTN_IDLE, ULONG( player - players ), SVCF_SKIPTHISCLIENT );
		}
	}
}

/*
================
=
= [BC] P_UseItems
=
= Looks for special items in front of the player to activate
================
*/

void P_UseItems( player_s *pPlayer )
{
	fixed_t 	x1;
	fixed_t 	y1;
	fixed_t 	x2;
	fixed_t 	y2;		
	fixed_t		vrange;

	usething = pPlayer->mo;
	if ( usething == NULL )
		return;

	x1 = usething->x;
	y1 = usething->y;
	x2 = x1 + ((USERANGE+8)>>FRACBITS)*finecosine[usething->angle >> ANGLETOFINESHIFT];
	y2 = y1 + ((USERANGE+8)>>FRACBITS)*finesine[usething->angle >> ANGLETOFINESHIFT];
	vrange = clamp( pPlayer->userinfo.aimdist, ANGLE_1/2, ANGLE_1*35 );
	aim.toppitch = usething->pitch - vrange;
	aim.bottompitch = usething->pitch + vrange;

	linetarget = NULL;

	P_PathTraverse( x1, y1, x2, y2, PT_ADDTHINGS, PTR_UsethingTraverse );

	if ( linetarget )
	{
		// Don't try to trigger sector actions in client mode.
		if (( NETWORK_GetState( ) != NETSTATE_CLIENT ) &&
			( CLIENTDEMO_IsPlaying( ) == false ) &&
			( linetarget->special ) &&
			( linetarget->ulSTFlags & STFL_USESPECIAL ))
		{
			LineSpecials[linetarget->special]( NULL, usething, false, linetarget->args[0],
									   linetarget->args[1], linetarget->args[2],
									   linetarget->args[3], linetarget->args[4] );
		}
		else
		{
			S_Sound( usething, CHAN_VOICE, "*usefail", 1, ATTN_IDLE );

			// Tell clients of the "oof" sound.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SoundActor( usething, CHAN_VOICE, "*usefail", 1, ATTN_IDLE, ULONG( pPlayer - players ), SVCF_SKIPTHISCLIENT );
		}
	}
}

/*
================
=
= P_PlayerScan
=
= Looks for other players directly in front of the player.
================
*/

player_s *P_PlayerScan( AActor *pSource )
{
	fixed_t vx, vy, vz, shootz;
	FTraceResults	trace;
	int				pitch;
	angle_t			angle;

	angle = pSource->angle >> ANGLETOFINESHIFT;
	pitch = (angle_t)( pSource->pitch ) >> ANGLETOFINESHIFT;

	vx = FixedMul (finecosine[pitch], finecosine[angle]);
	vy = FixedMul (finecosine[pitch], finesine[angle]);
	vz = -finesine[pitch];

	shootz = pSource->z - pSource->floorclip + (pSource->height>>1) + 8*FRACUNIT;

	if ( Trace( pSource->x,	// Actor x
		pSource->y, // Actor y
		shootz,	// Actor z
		pSource->Sector,
		vx,
		vy,
		vz,
		( 32 * 64 * FRACUNIT ) /* MISSILERANGE */,	// Maximum distance
		MF_SHOOTABLE,	// Actor mask
		ML_BLOCKEVERYTHING,	// Wall mask
		pSource,		// Actor to ignore
		trace,	// Result
		TRACE_NoSky,	// Trace flags
		NULL ) == false )	// Callback
	// Did not spot anything anything.
	{
		return ( NULL );
	}
	else
	{
		// Return NULL if we did not hit an actor.
		if ( trace.HitType != TRACE_HitActor )
			return ( NULL );

		// Return NULL if the actor we hit is not a player.
		if ( trace.Actor->player == NULL )
			return ( NULL );

		// Return the player we found.
		return ( trace.Actor->player );
	}
}

//==========================================================================
//
// PTR_PuzzleItemTraverse
//
//==========================================================================

static AActor *PuzzleItemUser;
static int PuzzleItemType;
static bool PuzzleActivated;

bool PTR_PuzzleItemTraverse (intercept_t *in)
{
	AActor *mobj;

	if (in->isaline)
	{ // Check line
		if (in->d.line->special != USE_PUZZLE_ITEM_SPECIAL)
		{
			P_LineOpening (in->d.line, trace.x + FixedMul (trace.dx, in->frac),
				trace.y + FixedMul (trace.dy, in->frac));
			if (openrange <= 0)
			{
				return false; // can't use through a wall
			}
			return true; // Continue searching
		}
		if (P_PointOnLineSide (PuzzleItemUser->x, PuzzleItemUser->y,
			in->d.line) == 1)
		{ // Don't use back sides
			return false;
		}
		if (PuzzleItemType != in->d.line->args[0])
		{ // Item type doesn't match
			return false;
		}
		P_StartScript (PuzzleItemUser, in->d.line, in->d.line->args[1], NULL, 0,
			in->d.line->args[2], in->d.line->args[3], in->d.line->args[4], true, false);
		in->d.line->special = 0;
		PuzzleActivated = true;
		return false; // Stop searching
	}
	// Check thing
	mobj = in->d.thing;
	if (mobj->special != USE_PUZZLE_ITEM_SPECIAL)
	{ // Wrong special
		return true;
	}
	if (PuzzleItemType != mobj->args[0])
	{ // Item type doesn't match
		return true;
	}
	P_StartScript (PuzzleItemUser, NULL, mobj->args[1], NULL, 0,
		mobj->args[2], mobj->args[3], mobj->args[4], true, false);
	mobj->special = 0;
	PuzzleActivated = true;
	return false; // Stop searching
}

//==========================================================================
//
// P_UsePuzzleItem
//
// Returns true if the puzzle item was used on a line or a thing.
//
//==========================================================================

bool P_UsePuzzleItem (AActor *actor, int itemType)
{
	int angle;
	fixed_t x1, y1, x2, y2;

	PuzzleItemType = itemType;
	PuzzleItemUser = actor;
	PuzzleActivated = false;
	angle = actor->angle>>ANGLETOFINESHIFT;
	x1 = actor->x;
	y1 = actor->y;
	x2 = x1+(USERANGE>>FRACBITS)*finecosine[angle];
	y2 = y1+(USERANGE>>FRACBITS)*finesine[angle];
	P_PathTraverse (x1, y1, x2, y2, PT_ADDLINES|PT_ADDTHINGS,
		PTR_PuzzleItemTraverse);
	return PuzzleActivated;
}

//
// RADIUS ATTACK
//
AActor* bombsource;
AActor* bombspot;
int 	bombdamage;
float	bombdamagefloat;
int		bombdistance;
float	bombdistancefloat;
bool	DamageSource;
FName	bombmod;
FVector3 bombvec;
bool	bombdodamage;

//=============================================================================
//
// PIT_RadiusAttack
//
// "bombsource" is the creature that caused the explosion at "bombspot".
// [RH] Now it knows about vertical distances and can thrust things vertically.
//=============================================================================

// [RH] Damage scale to apply to thing that shot the missile.
static float selfthrustscale;

CUSTOM_CVAR (Float, splashfactor, 1.f, CVAR_SERVERINFO)
{
	if (self <= 0.f)
		self = 1.f;
	else
		selfthrustscale = 1.f / self;
}

bool PIT_RadiusAttack (AActor *thing)
{
	if (!(thing->flags & MF_SHOOTABLE) )
		return true;

	// [BC] New weapon flag that allows radius damage on monsters that have protection
	// from radius damage.
	if (( bombsource == NULL ) || ( bombsource->player == NULL ) ||
		( bombsource->player->ReadyWeapon == NULL ) ||
		(( bombsource->player->ReadyWeapon->WeaponFlags & WIF_RADIUSDAMAGE_BOSSES ) == false ))
	{
		// Boss spider and cyborg and Heretic's ep >= 2 bosses
		// take no damage from concussion.
		if (thing->flags3 & MF3_NORADIUSDMG && !(bombspot->flags4 & MF4_FORCERADIUSDMG))
			return true;
	}

	if (!DamageSource && thing == bombsource)
	{ // don't damage the source of the explosion
		return true;
	}

	// a much needed option: monsters that fire explosive projectiles cannot 
	// be hurt by projectiles fired by a monster of the same type.
	// Controlled by the DONTHURTSPECIES flag.
	if (bombsource && 
		thing->GetClass() == bombsource->GetClass() && 
		!thing->player &&
		bombsource->flags4 & MF4_DONTHURTSPECIES
		) return true;

	// Barrels always use the original code, since this makes
	// them far too "active." BossBrains also use the old code
	// because some user levels require they have a height of 16,
	// which can make them near impossible to hit with the new code.
	if (!bombdodamage || !((bombspot->flags5 | thing->flags5) & MF5_OLDRADIUSDMG))
	{
		// [RH] New code. The bounding box only covers the
		// height of the thing and not the height of the map.
		float points;
		float len;
		fixed_t dx, dy;
		float boxradius;

		dx = abs (thing->x - bombspot->x);
		dy = abs (thing->y - bombspot->y);
		boxradius = float (thing->radius);

		// The damage pattern is square, not circular.
		len = float (dx > dy ? dx : dy);

		if (bombspot->z < thing->z || bombspot->z >= thing->z + thing->height)
		{
			float dz;

			if (bombspot->z > thing->z)
			{
				dz = float (bombspot->z - thing->z - thing->height);
			}
			else
			{
				dz = float (thing->z - bombspot->z);
			}
			if (len <= boxradius)
			{
				len = dz;
			}
			else
			{
				len -= boxradius;
				len = sqrtf (len*len + dz*dz);
			}
		}
		else
		{
			len -= boxradius;
			if (len < 0.f)
				len = 0.f;
		}
		len /= FRACUNIT;
		points = bombdamagefloat * (1.f - len * bombdistancefloat);
		if (thing == bombsource)
		{
			points = points * splashfactor;
		}
		points *= thing->GetClass()->Meta.GetMetaFixed(AMETA_RDFactor, FRACUNIT)/(float)FRACUNIT;

		if (points > 0.f && P_CheckSight (thing, bombspot, 1))
		{ // OK to damage; target is in direct path
			float momz;
			float thrust;
			int damage = (int)points;

			// [BC] Damage is server side.
			if (( NETWORK_GetState( ) != NETSTATE_CLIENT ) && ( CLIENTDEMO_IsPlaying( ) == false ))
			{
				if (bombdodamage) P_DamageMobj (thing, bombspot, bombsource, damage, bombmod);
				else if (thing->player == NULL) thing->flags2 |= MF2_BLASTED;
			}

			// [BC] If this explosion damaged a player, and the explosion originated from a
			// player, mark the source player as striking a player, to potentially reward an
			// accuracy medal.
			if (( bombsource ) &&
				( bombsource->player ) &&
				( thing != bombsource ) &&
				( thing->player ) &&
				( thing->IsTeammate( bombsource ) == false ))
			{
				bombsource->player->bStruckPlayer = true;
			}

			if (!(thing->flags & MF_ICECORPSE))
			{
				if (bombdodamage && !(bombspot->flags3 & MF3_BLOODLESSIMPACT)) P_TraceBleed (damage, thing, bombspot);

				if (!bombdodamage || !(bombspot->flags2 & MF2_NODMGTHRUST))
				{
					thrust = points * 0.5f / (float)thing->Mass;
					if (bombsource == thing)
					{
						thrust *= selfthrustscale;
					}
					momz = (float)(thing->z + (thing->height>>1) - bombspot->z) * thrust;
					if (bombsource != thing)
					{
						momz *= 0.5f;
					}
					else
					{
						momz *= 0.8f;
					}
					angle_t ang = R_PointToAngle2 (bombspot->x, bombspot->y, thing->x, thing->y) >> ANGLETOFINESHIFT;
					thing->momx += fixed_t (finecosine[ang] * thrust);
					thing->momy += fixed_t (finesine[ang] * thrust);
					if (bombdodamage) thing->momz += (fixed_t)momz;	// this really doesn't work well

					// [BC] If we're the server, update the thing's momentum.
					if ( NETWORK_GetState( ) == NETSTATE_SERVER )
						SERVERCOMMANDS_MoveThingExact( thing, CM_MOMX|CM_MOMY|CM_MOMZ );
				}
			}
		}
	}
	else
	{
		// [RH] Old code just for barrels
		fixed_t dx, dy, dist;

		dx = abs (thing->x - bombspot->x);
		dy = abs (thing->y - bombspot->y);

		dist = dx>dy ? dx : dy;
		dist = (dist - thing->radius) >> FRACBITS;

		if (dist < 0)
			dist = 0;

		if (dist >= bombdistance)
			return true;  // out of range

		if (P_CheckSight (thing, bombspot, 1))
		{ // OK to damage; target is in direct path
			int damage = Scale (bombdamage, bombdistance-dist, bombdistance);
			damage = (int)((float)damage * splashfactor);

			damage = Scale(damage, thing->GetClass()->Meta.GetMetaFixed(AMETA_RDFactor, FRACUNIT), FRACUNIT);
			if (damage > 0)
			{
				// Damage is server side.
				if (( NETWORK_GetState( ) != NETSTATE_CLIENT ) && ( CLIENTDEMO_IsPlaying( ) == false ))
					P_DamageMobj (thing, bombspot, bombsource, damage, bombmod);
				P_TraceBleed (damage, thing, bombspot);
			}
		}
	}
	return true;
}

//
// P_RadiusAttack
// Source is the creature that caused the explosion at spot.
//
void P_RadiusAttack (AActor *spot, AActor *source, int damage, int distance, FName damageType,
	bool hurtSource, bool dodamage)
{
	static TArray<AActor *> radbt;

	int x, y;
	int xl, xh, yl, yh;
	fixed_t dist;

	if (distance <= 0)
		return;

	dist = (distance + MAXRADIUS)<<FRACBITS;
	yh = (spot->y + dist - bmaporgy)>>MAPBLOCKSHIFT;
	yl = (spot->y - dist - bmaporgy)>>MAPBLOCKSHIFT;
	xh = (spot->x + dist - bmaporgx)>>MAPBLOCKSHIFT;
	xl = (spot->x - dist - bmaporgx)>>MAPBLOCKSHIFT;
	bombspot = spot;
	bombsource = source;
	bombdamage = damage;
	bombdistance = distance;
	bombdistancefloat = 1.f / (float)distance;
	DamageSource = hurtSource;
	bombdamagefloat = (float)damage;
	bombmod = damageType;
	bombdodamage = dodamage;
	bombvec.X = FIXED2FLOAT(spot->x);
	bombvec.Y = FIXED2FLOAT(spot->y);
	bombvec.Z = FIXED2FLOAT(spot->z);

	radbt.Clear();

	for (y = yl; y <= yh; y++)
		for (x = xl; x <= xh; x++)
			P_BlockThingsIterator (x, y, PIT_RadiusAttack, radbt);
}



//
// SECTOR HEIGHT CHANGING
// After modifying a sector's floor or ceiling height,
// call this routine to adjust the positions
// of all things that touch the sector.
//
// If anything doesn't fit anymore, true will be returned.
//
// [RH] If crushchange is non-negative, they will take the
//		specified amount of damage as they are being crushed.
//		If crushchange is negative, you should set the sector
//		height back the way it was and call P_ChangeSector()
//		again to undo the changes.
//		Note that this is very different from the original
//		true/false usage of crushchange! If you want regular
//		DOOM crushing behavior set crushchange to 10 or -1
//		if no crushing is desired.
//
static int moveamt;
int		crushchange;
static sector_t *movesec;
bool 	nofit;
TArray<AActor *> intersectors;

EXTERN_CVAR (Int, cl_bloodtype)

//=============================================================================
//
// P_AdjustFloorCeil
//
//=============================================================================

bool P_AdjustFloorCeil (AActor *thing)
{
	bool isgood = P_CheckPosition (thing, thing->x, thing->y);
	thing->floorz = tmfloorz;
	thing->ceilingz = tmceilingz;
	thing->dropoffz = tmdropoffz;		// killough 11/98: remember dropoffs
	thing->floorpic = tmfloorpic;
	thing->floorsector = tmfloorsector;
	thing->ceilingpic = tmceilingpic;
	thing->ceilingsector = tmceilingsector;
	return isgood;
}

//=============================================================================
//
// PIT_FindAboveIntersectors
//
//=============================================================================

bool PIT_FindAboveIntersectors (AActor *thing)
{
	if (!(thing->flags & MF_SOLID))
	{ // Can't hit thing
		return true;
	}
	if (thing->flags & (MF_CORPSE|MF_SPECIAL))
	{ // [RH] Corpses and specials don't block moves
		return true;
	}
	if (thing == tmthing)
	{ // Don't clip against self
		return true;
	}
	fixed_t blockdist = thing->radius+tmthing->radius;
	if (abs(thing->x-tmx) >= blockdist || abs(thing->y-tmy) >= blockdist)
	{ // Didn't hit thing
		return true;
	}
	if (thing->z >= tmthing->z &&
		thing->z <= tmthing->z + tmthing->height)
	{ // Thing intersects above the base
		intersectors.Push (thing);
	}
	return true;
}

//=============================================================================
//
// PIT_FindBelowIntersectors
//
//=============================================================================

bool PIT_FindBelowIntersectors (AActor *thing)
{
	if (!(thing->flags & MF_SOLID))
	{ // Can't hit thing
		return true;
	}
	if (thing->flags & (MF_CORPSE|MF_SPECIAL))
	{ // [RH] Corpses and specials don't block moves
		return true;
	}
	if (thing == tmthing)
	{ // Don't clip against self
		return true;
	}
	fixed_t blockdist = thing->radius+tmthing->radius;
	if (abs(thing->x-tmx) >= blockdist || abs(thing->y-tmy) >= blockdist)
	{ // Didn't hit thing
		return true;
	}
	if (thing->z + thing->height <= tmthing->z + tmthing->height &&
		thing->z + thing->height > tmthing->z)
	{ // Thing intersects below the base
		intersectors.Push (thing);
	}
	return true;
}

//=============================================================================
//
// P_FindAboveIntersectors
//
//=============================================================================

void P_FindAboveIntersectors (AActor *actor)
{
	static TArray<AActor *> abovebt;

	int	xl,xh,yl,yh,bx,by;
	fixed_t x, y;

	if (actor->flags & MF_NOCLIP)
		return;

	if (!(actor->flags & MF_SOLID))
		return;

	tmx = x = actor->x;
	tmy = y = actor->y;
	tmthing = actor;

	tmbbox[BOXTOP] = y + actor->radius;
	tmbbox[BOXBOTTOM] = y - actor->radius;
	tmbbox[BOXRIGHT] = x + actor->radius;
	tmbbox[BOXLEFT] = x - actor->radius;
//
// the bounding box is extended by MAXRADIUS because actors are grouped
// into mapblocks based on their origin point, and can overlap into adjacent
// blocks by up to MAXRADIUS units
//
	xl = (tmbbox[BOXLEFT] - bmaporgx - MAXRADIUS)>>MAPBLOCKSHIFT;
	xh = (tmbbox[BOXRIGHT] - bmaporgx + MAXRADIUS)>>MAPBLOCKSHIFT;
	yl = (tmbbox[BOXBOTTOM] - bmaporgy - MAXRADIUS)>>MAPBLOCKSHIFT;
	yh = (tmbbox[BOXTOP] - bmaporgy + MAXRADIUS)>>MAPBLOCKSHIFT;

	abovebt.Clear();

	for (bx = xl; bx <= xh; bx++)
		for (by = yl; by <= yh; by++)
			if (!P_BlockThingsIterator (bx, by, PIT_FindAboveIntersectors, abovebt))
				return;

	return;
}

//=============================================================================
//
// P_FindBelowIntersectors
//
//=============================================================================

void P_FindBelowIntersectors (AActor *actor)
{
	static TArray<AActor *> belowbt;

	int	xl,xh,yl,yh,bx,by;
	fixed_t x, y;

	if (actor->flags & MF_NOCLIP)
		return;

	if (!(actor->flags & MF_SOLID))
		return;

	tmx = x = actor->x;
	tmy = y = actor->y;
	tmthing = actor;

	tmbbox[BOXTOP] = y + actor->radius;
	tmbbox[BOXBOTTOM] = y - actor->radius;
	tmbbox[BOXRIGHT] = x + actor->radius;
	tmbbox[BOXLEFT] = x - actor->radius;
//
// the bounding box is extended by MAXRADIUS because actors are grouped
// into mapblocks based on their origin point, and can overlap into adjacent
// blocks by up to MAXRADIUS units
//
	xl = (tmbbox[BOXLEFT] - bmaporgx - MAXRADIUS)>>MAPBLOCKSHIFT;
	xh = (tmbbox[BOXRIGHT] - bmaporgx + MAXRADIUS)>>MAPBLOCKSHIFT;
	yl = (tmbbox[BOXBOTTOM] - bmaporgy - MAXRADIUS)>>MAPBLOCKSHIFT;
	yh = (tmbbox[BOXTOP] - bmaporgy + MAXRADIUS)>>MAPBLOCKSHIFT;

	belowbt.Clear();

	for (bx = xl; bx <= xh; bx++)
		for (by = yl; by <= yh; by++)
			if (!P_BlockThingsIterator (bx, by, PIT_FindBelowIntersectors, belowbt))
				return;

	return;
}

//=============================================================================
//
// P_DoCrunch
//
//=============================================================================

void P_DoCrunch (AActor *thing)
{
	ULONG	ulIdx;

	// [BC] Don't handle the respawning of crunched items on the client end.
	if (( NETWORK_GetState( ) != NETSTATE_CLIENT ) && ( CLIENTDEMO_IsPlaying( ) == false ))
	{
		// [BC] If this item is a flag belonging to a team, return it, and/or don't do shit!
		for ( ulIdx = 0; ulIdx < NUM_TEAMS; ulIdx++ )
		{
			if ( thing->GetClass( ) == TEAM_GetFlagItem( ulIdx ))
			{
				if ( thing->flags & MF_DROPPED )
					TEAM_ExecuteReturnRoutine( ulIdx, NULL );

				return;
			}
		}

		// [BC] Do the same thing, except for the white flag.
		if ( thing->GetClass( ) == PClass::FindClass( "WhiteFlag" ))
		{
			if ( thing->flags & MF_DROPPED )
				TEAM_ExecuteReturnRoutine( NUM_TEAMS, NULL );

			return;
		}

		// [BC] If this is the terminator or possession artifact, respawn them on the map.
		if (( terminator ) || ( possession ) || ( teampossession ))
		{
			if ( thing->GetClass( )->IsDescendantOf( RUNTIME_CLASS( APowerupGiver )))
			{
				if ( static_cast<APowerupGiver *>( thing )->PowerupType == RUNTIME_CLASS( APowerTerminatorArtifact ))
					GAME_SpawnTerminatorArtifact( );

				if ( static_cast<APowerupGiver *>( thing )->PowerupType == RUNTIME_CLASS( APowerPossessionArtifact ))
					GAME_SpawnPossessionArtifact( );
			}
		}
	}

	// crunch bodies to giblets
	if ((thing->flags & MF_CORPSE) &&
		!(thing->flags3 & MF3_DONTGIB) &&
		(thing->health <= 0))
	{
		FState * state = thing->FindState(NAME_Crush);
		if (state != NULL && !(thing->flags & MF_ICECORPSE))
		{
			// Clear MF_CORPSE so that this isn't done more than once
			thing->flags &= ~(MF_CORPSE|MF_SOLID);
			thing->height = thing->radius = 0;
			thing->SetState (state);
			return;
		}
		if (!(thing->flags & MF_NOBLOOD))
		{
			AActor *gib = Spawn ("RealGibs", thing->x, thing->y, thing->z, ALLOW_REPLACE);
			gib->RenderStyle = thing->RenderStyle;
			gib->alpha = thing->alpha;
			gib->height = 0;
			gib->radius = 0;
			S_Sound (thing, CHAN_BODY, "misc/fallingsplat", 1, ATTN_IDLE);

			PalEntry bloodcolor = (PalEntry)thing->GetClass()->Meta.GetMetaInt(AMETA_BloodColor);
			if (bloodcolor!=0) gib->Translation = TRANSLATION(TRANSLATION_Blood, bloodcolor.a);
		}
		if (thing->flags & MF_ICECORPSE)
		{
			thing->tics = 1;
			thing->momx = thing->momy = thing->momz = 0;

			// [BC] If we're the server, tell clients to update this thing's tics and
			// momentum.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			{
				SERVERCOMMANDS_SetThingTics( thing );
				SERVERCOMMANDS_MoveThing( thing, CM_MOMX|CM_MOMY|CM_MOMZ );
			}
		}
		else if (thing->player)
		{
			thing->flags |= MF_NOCLIP;
			thing->flags3 |= MF3_DONTGIB;
			thing->renderflags |= RF_INVISIBLE;
		}
		else
		{
			// [BC] If we're playing a game mode in which the map resets, and this is something
			// that is level spawned, don't destroy it. Instead, put it in a temporary invisibile
			// state.
			if (( GAMEMODE_GetFlags( GAMEMODE_GetCurrentMode( )) & GMF_MAPRESETS ) &&
				( thing->ulSTFlags & STFL_LEVELSPAWNED ) &&
				( NETWORK_GetState( ) != NETSTATE_CLIENT ) &&
				( CLIENTDEMO_IsPlaying( ) == false ))
			{
				thing->renderflags |= RF_INVISIBLE;
				thing->flags &= ~MF_SOLID;
				thing->SetState( &AInventory::States[17] );
				return;
			}

			thing->Destroy ();
		}
		return;		// keep checking
	}

	// crunch dropped items
	if (thing->flags & MF_DROPPED)
	{
		// [BC] If we're the server, tell clients to destroy this item.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_DestroyThing( thing );

		// [BC] Don't destroy items in client mode; the server will tell us to.
		if (( NETWORK_GetState( ) != NETSTATE_CLIENT ) && ( CLIENTDEMO_IsPlaying( ) == false ))
		{
			// [BC] If we're playing a game mode in which the map resets, and this is something
			// that is level spawned, don't destroy it. Instead, put it in a temporary invisibile
			// state.
			if (( GAMEMODE_GetFlags( GAMEMODE_GetCurrentMode( )) & GMF_MAPRESETS ) &&
				( thing->ulSTFlags & STFL_LEVELSPAWNED ) &&
				( NETWORK_GetState( ) != NETSTATE_CLIENT ) &&
				( CLIENTDEMO_IsPlaying( ) == false ))
			{
				thing->renderflags |= RF_INVISIBLE;
				thing->flags &= ~MF_SOLID;
				thing->SetState( &AInventory::States[17] );
				return;
			}

			thing->Destroy ();
		}
		return;		// keep checking
	}

	if (!(thing->flags & MF_SOLID) || (thing->flags & MF_NOCLIP))
	{
		return;
	}

	if (!(thing->flags & MF_SHOOTABLE))
	{
		return;		// assume it is bloody gibs or something
	}

	nofit = true;

	if ((crushchange > 0) && !(level.maptime & 3))
	{
		// [BC] Damage is done server-side.
		if (( NETWORK_GetState( ) != NETSTATE_CLIENT ) && ( CLIENTDEMO_IsPlaying( ) == false ))
			P_DamageMobj (thing, NULL, NULL, crushchange, NAME_Crush);

		// spray blood in a random direction
		if ((!(thing->flags&MF_NOBLOOD)) &&
			(!(thing->flags2&(MF2_INVULNERABLE|MF2_DORMANT))))
		{
			PalEntry bloodcolor = (PalEntry)thing->GetClass()->Meta.GetMetaInt(AMETA_BloodColor);
			const PClass *bloodcls = PClass::FindClass((ENamedName)thing->GetClass()->Meta.GetMetaInt(AMETA_BloodType, NAME_Blood));

			P_TraceBleed (crushchange, thing);
			if ( (bloodcls != NULL) && (( cl_bloodtype <= 1) || ( NETWORK_GetState( ) == NETSTATE_SERVER )) )
			{
				AActor *mo;

				mo = Spawn (bloodcls, thing->x, thing->y,
					thing->z + thing->height/2, ALLOW_REPLACE);

				mo->momx = pr_crunch.Random2 () << 12;
				mo->momy = pr_crunch.Random2 () << 12;
				if (bloodcolor != 0 && !(mo->flags2 & MF2_DONTTRANSLATE))
				{
					mo->Translation = TRANSLATION(TRANSLATION_Blood, bloodcolor.a);
				}
			}
			if (cl_bloodtype >= 1)
			{
				angle_t an;

				an = (M_Random () - 128) << 24;
				P_DrawSplash2 (32, thing->x, thing->y,
							   thing->z + thing->height/2, an, 2, bloodcolor);
			}
		}
	}

	// keep checking (crush other things)
	return;
}

//=============================================================================
//
// P_PushUp
//
// Returns 0 if thing fits, 1 if ceiling got in the way, or 2 if something
// above it didn't fit.
//=============================================================================

int P_PushUp (AActor *thing)
{
	unsigned int firstintersect = intersectors.Size ();
	unsigned int lastintersect;
	int mymass = thing->Mass;

	if (thing->z + thing->height > thing->ceilingz)
	{
		return 1;
	}
	P_FindAboveIntersectors (thing);
	lastintersect = intersectors.Size ();
	for (; firstintersect < lastintersect; firstintersect++)
	{
		AActor *intersect = intersectors[firstintersect];
		if (!(intersect->flags2 & MF2_PASSMOBJ) ||
			(!(intersect->flags3 & MF3_ISMONSTER) &&
			 intersect->Mass > mymass))
		{ // Can't push things more massive than ourself
			return 2;
		}
		fixed_t oldz = intersect->z;
		P_AdjustFloorCeil (intersect);
		intersect->z = thing->z + thing->height + 1;
		if (P_PushUp (intersect))
		{ // Move blocked
			P_DoCrunch (intersect);
			intersect->z = oldz;
			return 2;
		}
	}
	return 0;
}

//=============================================================================
//
// P_PushDown
//
// Returns 0 if thing fits, 1 if floor got in the way, or 2 if something
// below it didn't fit.
//=============================================================================

int P_PushDown (AActor *thing)
{
	unsigned int firstintersect = intersectors.Size ();
	unsigned int lastintersect;
	int mymass = thing->Mass;

	if (thing->z <= thing->floorz)
	{
		return 1;
	}
	P_FindBelowIntersectors (thing);
	lastintersect = intersectors.Size ();
	for (; firstintersect < lastintersect; firstintersect++)
	{
		AActor *intersect = intersectors[firstintersect];
		if (!(intersect->flags2 & MF2_PASSMOBJ) ||
			(!(intersect->flags3 & MF3_ISMONSTER) &&
			 intersect->Mass > mymass))
		{ // Can't push things more massive than ourself
			return 2;
		}
		fixed_t oldz = intersect->z;
		P_AdjustFloorCeil (intersect);
		if (oldz > thing->z - intersect->height)
		{ // Only push things down, not up.
			intersect->z = thing->z - intersect->height;
			if (P_PushDown (intersect))
			{ // Move blocked
				P_DoCrunch (intersect);
				intersect->z = oldz;
				return 2;
			}
		}
	}
	return 0;
}

//=============================================================================
//
// PIT_FloorDrop
//
//=============================================================================

void PIT_FloorDrop (AActor *thing)
{
	fixed_t oldfloorz = thing->floorz;

	P_AdjustFloorCeil (thing);

	if (thing->momz == 0 &&
		(!(thing->flags & MF_NOGRAVITY) ||
		 (thing->z == oldfloorz && !(thing->flags & MF_NOLIFTDROP))))
	{
		fixed_t oldz = thing->z;

		// If float bob, always stay the same approximate distance above
		// the floor; otherwise only move things standing on the floor,
		// and only do it if the drop is slow enough.
		if (thing->flags2 & MF2_FLOATBOB)
		{
			thing->z = thing->z - oldfloorz + thing->floorz;
			P_CheckFakeFloorTriggers (thing, oldz);
		}
		else if ((thing->flags & MF_NOGRAVITY) ||
			((!(level.flags & LEVEL_HEXENFORMAT) || moveamt < 9*FRACUNIT)
			 && thing->z - thing->floorz <= moveamt))
		{
			thing->z = thing->floorz;
			P_CheckFakeFloorTriggers (thing, oldz);
		}

		// [BC] Mark this thing as having moved.
		thing->ulSTFlags |= STFL_POSITIONCHANGED;
	}
}

//=============================================================================
//
// PIT_FloorRaise
//
//=============================================================================

void PIT_FloorRaise (AActor *thing)
{
	fixed_t oldfloorz = thing->floorz;

	P_AdjustFloorCeil (thing);

	// Move things intersecting the floor up
	if (thing->z <= thing->floorz ||
		(!(thing->flags & MF_NOGRAVITY) && (thing->flags2 & MF2_FLOATBOB)))
	{
		intersectors.Clear ();
		fixed_t oldz = thing->z;
		if (!(thing->flags2 & MF2_FLOATBOB))
		{
			thing->z = thing->floorz;
		}
		else
		{
			thing->z = thing->z - oldfloorz + thing->floorz;
		}

		// [BC] Mark this thing as having moved.
		thing->ulSTFlags |= STFL_POSITIONCHANGED;

		switch (P_PushUp (thing))
		{
		default:
			P_CheckFakeFloorTriggers (thing, oldz);
			break;
		case 1:
			P_DoCrunch (thing);
			P_CheckFakeFloorTriggers (thing, oldz);
			break;
		case 2:
			P_DoCrunch (thing);
			thing->z = oldz;
			break;
		}
	}
}

//=============================================================================
//
// PIT_CeilingLower
//
//=============================================================================

void PIT_CeilingLower (AActor *thing)
{
	bool onfloor;

	onfloor = thing->z <= thing->floorz;
	P_AdjustFloorCeil (thing);

	if (thing->z + thing->height > thing->ceilingz)
	{
		intersectors.Clear ();
		fixed_t oldz = thing->z;
		if (thing->ceilingz - thing->height >= thing->floorz)
		{
			thing->z = thing->ceilingz - thing->height;
		}
		else
		{
			thing->z = thing->floorz;
		}

		// [BC] Mark this thing as having moved.
		thing->ulSTFlags |= STFL_POSITIONCHANGED;

		switch (P_PushDown (thing))
		{
		case 2:
			// intentional fall-through
		case 1:
			if (onfloor)
				thing->z = thing->floorz;
			P_DoCrunch (thing);
			P_CheckFakeFloorTriggers (thing, oldz);
			break;
		default:
			P_CheckFakeFloorTriggers (thing, oldz);
			break;
		}
	}
}

//=============================================================================
//
// PIT_CeilingRaise
//
//=============================================================================

void PIT_CeilingRaise (AActor *thing)
{
	bool isgood = P_AdjustFloorCeil (thing);

	// For DOOM compatibility, only move things that are inside the floor.
	// (or something else?) Things marked as hanging from the ceiling will
	// stay where they are.
	if (thing->z < thing->floorz &&
		thing->z + thing->height >= thing->ceilingz - moveamt &&
		!(thing->flags & MF_NOLIFTDROP))
	{
		fixed_t oldz = thing->z;
		thing->z = thing->floorz;
		if (thing->z + thing->height > thing->ceilingz)
		{
			thing->z = thing->ceilingz - thing->height;
		}
		P_CheckFakeFloorTriggers (thing, oldz);

		// [BC] Mark this thing as having moved.
		thing->ulSTFlags |= STFL_POSITIONCHANGED;
	}
	else if ((thing->flags & MF2_PASSMOBJ) && !isgood && thing->z + thing->height < thing->ceilingz)
	{
		if (!P_TestMobjZ (thing) && onmobj->z <= thing->z)
		{
			thing->z = MIN (thing->ceilingz - thing->height,
							onmobj->z + onmobj->height);

			// [BC] Mark this thing as having moved.
			thing->ulSTFlags |= STFL_POSITIONCHANGED;
		}
	}
}

//=============================================================================
//
// P_ChangeSector	[RH] Was P_CheckSector in BOOM
//
// jff 3/19/98 added to just check monsters on the periphery
// of a moving sector instead of all in bounding box of the
// sector. Both more accurate and faster.
//
//=============================================================================

bool P_ChangeSector (sector_t *sector, int crunch, int amt, int floorOrCeil)
{
	void (*iterator)(AActor *);
	msecnode_t *n;

	nofit = false;
	crushchange = crunch;
	moveamt = abs (amt);
	movesec = sector;


	// Also process all sectors that have properties transferred from the
	// changed sector - for 3D-floors etc.
	if(sector->e->attached.Size())
	{
		unsigned       i;
		sector_t*      sec;


		// Use different functions for the four different types of sector movement.
		// for 3D-floors the meaning of floor and ceiling is inverted!!!
		if (floorOrCeil == 1)
		{ 
			iterator = (amt >= 0) ? PIT_FloorRaise : PIT_FloorDrop;
		}
		else
		{ 
			iterator = (amt >=0) ? PIT_CeilingRaise : PIT_CeilingLower;
		}

		for(i = 0; i < sector->e->attached.Size(); i ++)
		{
			sec = sector->e->attached[i];
			P_Recalculate3DFloors(sec);	// Must recalculate the 3d floor and light lists

			// no thing checks for attached sectors because of heightsec
			if (sec->heightsec==sector) continue;

			for (n=sec->touching_thinglist; n; n=n->m_snext) n->visited = false;
			do 
			{
				for (n=sec->touching_thinglist; n; n=n->m_snext)
				{
					if (!n->visited)
					{
						n->visited  = true;
						if (!(n->m_thing->flags&MF_NOBLOCKMAP)) iterator(n->m_thing);
						break;
					}
				}
			} 
			while (n);
		}
	}
	P_Recalculate3DFloors(sector);			// Must recalculate the 3d floor and light lists


	// [RH] Use different functions for the four different types of sector
	// movement. Also update the soundorg's z-coordinate for 3D sound.
	if (floorOrCeil == 0)
	{ // floor
		iterator = (amt < 0) ? PIT_FloorDrop : PIT_FloorRaise;
		sector->soundorg[2] = sector->floorplane.ZatPoint (sector->soundorg[0], sector->soundorg[1]);
	}
	else
	{ // ceiling
		iterator = (amt < 0) ? PIT_CeilingLower : PIT_CeilingRaise;
		sector->soundorg[2] = sector->ceilingplane.ZatPoint (sector->soundorg[0], sector->soundorg[1]);
	}

	// killough 4/4/98: scan list front-to-back until empty or exhausted,
	// restarting from beginning after each thing is processed. Avoids
	// crashes, and is sure to examine all things in the sector, and only
	// the things which are in the sector, until a steady-state is reached.
	// Things can arbitrarily be inserted and removed and it won't mess up.
	//
	// killough 4/7/98: simplified to avoid using complicated counter

	// Mark all things invalid

	for (n = sector->touching_thinglist; n; n = n->m_snext)
		n->visited = false;

	do
	{
		for (n = sector->touching_thinglist; n; n = n->m_snext)	// go through list
		{
			if (!n->visited)								// unprocessed thing found
			{
				n->visited = true; 							// mark thing as processed
				if (!(n->m_thing->flags & MF_NOBLOCKMAP))	//jff 4/7/98 don't do these
					iterator (n->m_thing);		 			// process it
				break;										// exit and start over
			}
		}
	} while (n);	// repeat from scratch until all things left are marked valid

	return nofit;
}

//=============================================================================
// phares 3/21/98
//
// Maintain a freelist of msecnode_t's to reduce memory allocs and frees.
//=============================================================================

msecnode_t *headsecnode = NULL;

//=============================================================================
//
// P_GetSecnode
//
// Retrieve a node from the freelist. The calling routine
// should make sure it sets all fields properly.
//
//=============================================================================

msecnode_t *P_GetSecnode()
{
	msecnode_t *node;

	if (headsecnode)
	{
		node = headsecnode;
		headsecnode = headsecnode->m_snext;
	}
	else
	{
		node = (msecnode_t *)M_Malloc (sizeof(*node));
	}
	return node;
}

//=============================================================================
//
// P_PutSecnode
//
// Returns a node to the freelist.
//
//=============================================================================

void P_PutSecnode (msecnode_t *node)
{
	node->m_snext = headsecnode;
	headsecnode = node;
}

//=============================================================================
// phares 3/16/98
//
// P_AddSecnode
//
// Searches the current list to see if this sector is
// already there. If not, it adds a sector node at the head of the list of
// sectors this object appears in. This is called when creating a list of
// nodes that will get linked in later. Returns a pointer to the new node.
//
//=============================================================================

msecnode_t *P_AddSecnode (sector_t *s, AActor *thing, msecnode_t *nextnode)
{
	msecnode_t *node;

	if (s == 0)
	{
		I_FatalError ("AddSecnode of 0 for %s\n", thing->_StaticType.TypeName.GetChars());
	}

	node = nextnode;
	while (node)
	{
		if (node->m_sector == s)	// Already have a node for this sector?
		{
			node->m_thing = thing;	// Yes. Setting m_thing says 'keep it'.
			return nextnode;
		}
		node = node->m_tnext;
	}

	// Couldn't find an existing node for this sector. Add one at the head
	// of the list.

	node = P_GetSecnode();

	// killough 4/4/98, 4/7/98: mark new nodes unvisited.
	node->visited = 0;

	node->m_sector = s; 			// sector
	node->m_thing  = thing; 		// mobj
	node->m_tprev  = NULL;			// prev node on Thing thread
	node->m_tnext  = nextnode;		// next node on Thing thread
	if (nextnode)
		nextnode->m_tprev = node;	// set back link on Thing

	// Add new node at head of sector thread starting at s->touching_thinglist

	node->m_sprev  = NULL;			// prev node on sector thread
	node->m_snext  = s->touching_thinglist; // next node on sector thread
	if (s->touching_thinglist)
		node->m_snext->m_sprev = node;
	s->touching_thinglist = node;
	return node;
}

//=============================================================================
//
// P_DelSecnode
//
// Deletes a sector node from the list of
// sectors this object appears in. Returns a pointer to the next node
// on the linked list, or NULL.
//
//=============================================================================

msecnode_t *P_DelSecnode (msecnode_t *node)
{
	msecnode_t* tp;  // prev node on thing thread
	msecnode_t* tn;  // next node on thing thread
	msecnode_t* sp;  // prev node on sector thread
	msecnode_t* sn;  // next node on sector thread

	if (node)
	{
		// Unlink from the Thing thread. The Thing thread begins at
		// sector_list and not from AActor->touching_sectorlist.

		tp = node->m_tprev;
		tn = node->m_tnext;
		if (tp)
			tp->m_tnext = tn;
		if (tn)
			tn->m_tprev = tp;

		// Unlink from the sector thread. This thread begins at
		// sector_t->touching_thinglist.

		sp = node->m_sprev;
		sn = node->m_snext;
		if (sp)
			sp->m_snext = sn;
		else
			node->m_sector->touching_thinglist = sn;
		if (sn)
			sn->m_sprev = sp;

		// Return this node to the freelist

		P_PutSecnode(node);
		return tn;
	}
	return NULL;
} 														// phares 3/13/98

//=============================================================================
//
// P_DelSector_List
//
// Deletes the sector_list and NULLs it.
//
//=============================================================================

void P_DelSector_List ()
{
	if (sector_list != NULL)
	{
		P_DelSeclist (sector_list);
		sector_list = NULL;
	}
}

//=============================================================================
//
// P_DelSeclist
//
// Delete an entire sector list
//
//=============================================================================

void P_DelSeclist (msecnode_t *node)
{
	while (node)
		node = P_DelSecnode (node);
}

//=============================================================================
// phares 3/14/98
//
// PIT_GetSectors
//
// Locates all the sectors the object is in by looking at the lines that
// cross through it. You have already decided that the object is allowed
// at this location, so don't bother with checking impassable or
// blocking lines.
//=============================================================================

bool PIT_GetSectors (line_t *ld)
{
	if (tmbbox[BOXRIGHT]	  <= ld->bbox[BOXLEFT]	 ||
			tmbbox[BOXLEFT]   >= ld->bbox[BOXRIGHT]  ||
			tmbbox[BOXTOP]	  <= ld->bbox[BOXBOTTOM] ||
			tmbbox[BOXBOTTOM] >= ld->bbox[BOXTOP])
		return true;

	if (P_BoxOnLineSide (tmbbox, ld) != -1)
		return true;

	// This line crosses through the object.

	// Collect the sector(s) from the line and add to the
	// sector_list you're examining. If the Thing ends up being
	// allowed to move to this position, then the sector_list
	// will be attached to the Thing's AActor at touching_sectorlist.

	sector_list = P_AddSecnode (ld->frontsector,tmthing,sector_list);

	// Don't assume all lines are 2-sided, since some Things
	// like MT_TFOG are allowed regardless of whether their radius takes
	// them beyond an impassable linedef.

	// killough 3/27/98, 4/4/98:
	// Use sidedefs instead of 2s flag to determine two-sidedness.

	if (ld->backsector)
		sector_list = P_AddSecnode(ld->backsector, tmthing, sector_list);

	return true;
}

//=============================================================================
// phares 3/14/98
//
// P_CreateSecNodeList
//
// Alters/creates the sector_list that shows what sectors the object resides in
//
//=============================================================================

void P_CreateSecNodeList (AActor *thing, fixed_t x, fixed_t y)
{
	int xl, xh, yl, yh, bx, by;
	msecnode_t *node;

	// [RH] Save old tm* values.
	AActor *thingsave = tmthing;
	int flagssave = tmflags;
	fixed_t xsave = tmx;
	fixed_t ysave = tmy;
	fixed_t bboxsave[4];

	memcpy (bboxsave, tmbbox, sizeof(bboxsave));

	// First, clear out the existing m_thing fields. As each node is
	// added or verified as needed, m_thing will be set properly. When
	// finished, delete all nodes where m_thing is still NULL. These
	// represent the sectors the Thing has vacated.

	node = sector_list;
	while (node)
	{
		node->m_thing = NULL;
		node = node->m_tnext;
	}

	tmthing = thing;
	tmflags = thing->flags;

	tmx = x;
	tmy = y;

	tmbbox[BOXTOP]	  = y + tmthing->radius;
	tmbbox[BOXBOTTOM] = y - tmthing->radius;
	tmbbox[BOXRIGHT]  = x + tmthing->radius;
	tmbbox[BOXLEFT]   = x - tmthing->radius;

	validcount++; // used to make sure we only process a line once

	xl = (tmbbox[BOXLEFT] - bmaporgx)>>MAPBLOCKSHIFT;
	xh = (tmbbox[BOXRIGHT] - bmaporgx)>>MAPBLOCKSHIFT;
	yl = (tmbbox[BOXBOTTOM] - bmaporgy)>>MAPBLOCKSHIFT;
	yh = (tmbbox[BOXTOP] - bmaporgy)>>MAPBLOCKSHIFT;

	for (bx = xl; bx <= xh; bx++)
		for (by = yl; by <= yh; by++)
			P_BlockLinesIterator (bx,by,PIT_GetSectors);

	// Add the sector of the (x,y) point to sector_list.

	sector_list = P_AddSecnode (thing->Sector, thing, sector_list);

	// Now delete any nodes that won't be used. These are the ones where
	// m_thing is still NULL.

	node = sector_list;
	while (node)
	{
		if (node->m_thing == NULL)
		{
			if (node == sector_list)
				sector_list = node->m_tnext;
			node = P_DelSecnode (node);
		}
		else
		{
			node = node->m_tnext;
		}
	}

	// [RH] Restore old tm* values.
	tmthing = thingsave;
	tmflags = flagssave;
	tmx = xsave;
	tmy = ysave;
	memcpy (tmbbox, bboxsave, sizeof(bboxsave));
}

void SpawnShootDecal (AActor *t1, const FTraceResults &trace)
{
	FDecalBase *decalbase = NULL;

	// [BC] Servers don't need to spawn decals.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		return;

	if (t1->player != NULL && t1->player->ReadyWeapon != NULL)
	{
		decalbase = t1->player->ReadyWeapon->GetDefault()->DecalGenerator;
	}
	else
	{
		decalbase = t1->DecalGenerator;
	}
	if (decalbase != NULL)
	{
		DImpactDecal::StaticCreate (decalbase->GetDecal (),
			trace.X, trace.Y, trace.Z, sides + trace.Line->sidenum[trace.Side], trace.ffloor);
	}
}

static void SpawnDeepSplash (AActor *t1, const FTraceResults &trace, AActor *puff,
	fixed_t vx, fixed_t vy, fixed_t vz)
{
	if (!trace.CrossedWater->heightsec) return;
	
	fixed_t num, den, hitdist;
	const secplane_t *plane = &trace.CrossedWater->heightsec->floorplane;

	den = TMulScale16 (plane->a, vx, plane->b, vy, plane->c, vz);
	if (den != 0)
	{
		num = TMulScale16 (plane->a, t1->x, plane->b, t1->y, plane->c, shootz) + plane->d;
		hitdist = FixedDiv (-num, den);

		if (hitdist >= 0 && hitdist <= trace.Distance)
		{
			fixed_t savex, savey, savez;

			// Move the puff onto the water surface, splash, then move it back.
			if (puff == NULL)
			{
				puff = t1;
			}

			savex = puff->x;
			savey = puff->y;
			savez = puff->z;
			puff->SetOrigin (t1->x+FixedMul (vx, hitdist),
							 t1->y+FixedMul (vy, hitdist),
							shootz+FixedMul (vz, hitdist));
			P_HitWater (puff, puff->Sector);
			puff->SetOrigin (savex, savey, savez);
		}
	}
}
