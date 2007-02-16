//-----------------------------------------------------------------------------
//
// Skulltag Source
// Copyright (C) 2004-2005 Brad Carney
// Copyright (C) 2007-2012 Skulltag Development Team
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
// 3. Neither the name of the Skulltag Development Team nor the names of its
//    contributors may be used to endorse or promote products derived from this
//    software without specific prior written permission.
// 4. Redistributions in any form must be accompanied by information on how to
//    obtain complete source code for the software and any accompanying
//    software that uses the software. The source code must either be included
//    in the distribution or be available for no more than the cost of
//    distribution plus a nominal fee, and must be freely redistributable
//    under reasonable conditions. For an executable file, complete source
//    code means the source code for all modules it contains. It does not
//    include source code for modules or files that typically accompany the
//    major components of the operating system on which the executable file
//    runs.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// Date created:  2/4/05
//
//
// Filename: botpath.cpp
//
// Description: 
//
//-----------------------------------------------------------------------------

#include "botpath.h"
#include "deathmatch.h"
#include "doomstat.h"
#include "gi.h"
#include "m_bbox.h"
#include "p_lnspec.h"
#include "p_local.h"
#include "r_defs.h"
#include "r_main.h"

//*****************************************************************************
//	VARIABLES

static	AActor				*g_pBlockingActor;
static	line_t				*g_pBlockingLine;
static	AActor				*g_pPathActor;
static	fixed_t				g_PathSectorCeilingZ;
static	fixed_t				g_PathSectorFloorZ;
static	sector_t			*g_pPathSector;
static	fixed_t				g_BoundingBox[4];
static	fixed_t				g_PathX;
static	fixed_t				g_PathY;
static	sector_t			*g_pDoorSector;

static	fixed_t				g_OpenTop;
static	fixed_t				g_OpenBottom;
static	fixed_t				g_OpenRange;
static	fixed_t				g_LowFloor;
static	sector_t			*g_pOpenTopSector;
static	sector_t			*g_pOpenBottomSector;

// Keep track of any lines that are crossed, but don't process them until the move
// is proven valid.
static	TArray<line_t *>	g_pPathLineArray;

//*****************************************************************************
//	PROTOTYPES

static	bool		botpath_CheckThing( AActor *pThing );
static	bool		botpath_CheckLine( line_t *pLine );

//*****************************************************************************
//	FUNCTIONS

bool BOTPATH_IsPositionBlocked( AActor *pActor, fixed_t DestX, fixed_t DestY )
{
	static	TArray<AActor *>	pPathActorArray;

	int xl, xh;
	int yl, yh;
	int bx, by;
	subsector_t		*pNewSector;
	AActor			*pThingBlocker;

	g_pPathActor = pActor;

	g_PathX = DestX;
	g_PathY = DestY;

	g_BoundingBox[BOXTOP] = DestY + pActor->radius;
	g_BoundingBox[BOXBOTTOM] = DestY - pActor->radius;
	g_BoundingBox[BOXRIGHT] = DestX + pActor->radius;
	g_BoundingBox[BOXLEFT] = DestX - pActor->radius;

	pNewSector = R_PointInSubsector( DestX, DestY );
	
	// The base floor / ceiling is from the subsector that contains the point.
	// Any contacted lines the step closer together will adjust them.
	g_PathSectorFloorZ = pNewSector->sector->floorplane.ZatPoint( DestX, DestY );
	g_PathSectorCeilingZ = pNewSector->sector->ceilingplane.ZatPoint( DestX, DestY );
	g_pPathSector = pNewSector->sector;

	validcount++;
	g_pPathLineArray.Clear ();
	pPathActorArray.Clear ();

	// Check things first, possibly picking things up.
	// The bounding box is extended by MAXRADIUS
	// because DActors are grouped into mapblocks
	// based on their origin point, and can overlap
	// into adjacent blocks by up to MAXRADIUS units.
	xl = ( g_BoundingBox[BOXLEFT] - bmaporgx - MAXRADIUS ) >> MAPBLOCKSHIFT;
	xh = ( g_BoundingBox[BOXRIGHT] - bmaporgx + MAXRADIUS ) >> MAPBLOCKSHIFT;
	yl = ( g_BoundingBox[BOXBOTTOM] - bmaporgy - MAXRADIUS ) >> MAPBLOCKSHIFT;
	yh = ( g_BoundingBox[BOXTOP] - bmaporgy + MAXRADIUS ) >> MAPBLOCKSHIFT;

	g_pBlockingActor = NULL;
	pThingBlocker = NULL;
/*
	if (thing->player)
	{ // [RH] Fake taller height to catch stepping up into things.
		thing->height = realheight + gameinfo.StepHeight;
	}
*/
	for ( bx = xl; bx <= xh; bx++ )
	{
		for ( by = yl; by <= yh; by++ )
		{
			AActor	*pRobin = NULL;

			do
			{
				if ( P_BlockThingsIterator( bx, by, botpath_CheckThing, pPathActorArray, pRobin ) == false )
				{
					// If we're blocked off, and not by a thing, we must have struck a wall.
					if ( g_pBlockingActor == NULL )
						return ( false );

					// [RH] If a thing can be stepped up on, we need to continue checking
					// other things in the blocks and see if we hit something that is
					// definitely blocking. Otherwise, we need to check the lines, or we
					// could end up stuck inside a wall.
					if (( g_pBlockingActor->player == NULL ) && (( g_pBlockingActor->z + g_pBlockingActor->height - pActor->z ) <= gameinfo.StepHeight ))
					{
						if (( pThingBlocker == NULL ) || ( g_pBlockingActor->z > pThingBlocker->z ))
							pThingBlocker = g_pBlockingActor;

						pRobin = g_pBlockingActor;
						g_pBlockingActor = NULL;
					}
					else if ((( pActor->z + pActor->height ) - g_pBlockingActor->z ) <= gameinfo.StepHeight )
					{
						if ( pThingBlocker )
						{
							// There is something to step up on. Return this thing as
							// the blocker so that we don't step up.
							return ( false );
						}

						// Nothing is blocking us, but this actor potentially could
						// if there is something else to step on.
						pRobin = g_pBlockingActor;
						g_pBlockingActor = NULL;
					}
					// Definitely blocking.
					else
						return ( false );
				}
				else
					pRobin = NULL;

			} while ( pRobin );
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

	g_pBlockingActor = NULL;
//	if (( g_PathSectorCeilingZ - g_PathSectorFloorZ ) < pActor->height )
//		return false;

	xl = ( g_BoundingBox[BOXLEFT] - bmaporgx ) >> MAPBLOCKSHIFT;
	xh = ( g_BoundingBox[BOXRIGHT] - bmaporgx ) >> MAPBLOCKSHIFT;
	yl = ( g_BoundingBox[BOXBOTTOM] - bmaporgy ) >> MAPBLOCKSHIFT;
	yh = ( g_BoundingBox[BOXTOP] - bmaporgy ) >> MAPBLOCKSHIFT;

	for ( bx = xl ; bx <= xh ; bx++ )
		for ( by = yl ; by <= yh ; by++ )
			if ( P_BlockLinesIterator( bx, by, botpath_CheckLine ) == false )
				return ( false );

	return (( g_pBlockingActor = pThingBlocker ) == NULL );
}

//*****************************************************************************
//
ULONG BOTPATH_TryWalk( AActor *pActor, fixed_t StartX, fixed_t StartY, fixed_t StartZ, fixed_t DestX, fixed_t DestY )
{
	ULONG		ulFlags;
	line_t		*pLine;
	LONG		lLineSide;
	LONG		lOldLineSide;
	LONG		lNumSteps;
	LONG		lCurrentStep;
	LONG		lHeightChange;
	fixed_t		XDistance;
	fixed_t		YDistance;
	fixed_t		AbsXDistance;
	fixed_t		AbsYDistance;
	fixed_t		OneStepDeltaX;
	fixed_t		OneStepDeltaY;
	fixed_t		X;
	fixed_t		Y;
	bool		bPotentialDoor;
//	bool		bObstructingIfNotDoor;

	ulFlags = 0;
	g_pDoorSector = NULL;
	bPotentialDoor = false;

	XDistance = DestX - StartX;
	YDistance = DestY - StartY;
	AbsXDistance = abs( XDistance );
	AbsYDistance = abs( YDistance );

	lNumSteps = 1;

	// If more distance is being covered in the X direction...
	if ( AbsXDistance > AbsYDistance )
	{
		if ( AbsXDistance > MAXMOVE )
			lNumSteps = 1 + ( AbsXDistance / MAXMOVE );
	}
	else
	{
		if ( AbsYDistance > MAXMOVE )
			lNumSteps = 1 + ( AbsYDistance / MAXMOVE );
	}

	lCurrentStep = 1;
	OneStepDeltaX = XDistance / lNumSteps;
	OneStepDeltaY = YDistance / lNumSteps;

	do
	{
		X = StartX + Scale( XDistance, lCurrentStep, lNumSteps );
		Y = StartY + Scale( YDistance, lCurrentStep, lNumSteps );

//		Printf( "(%d) %d/%d: (%d, %d) (%d, %d) (%d, %d) (%d, %d)\n", level.time, lCurrentStep, lNumSteps, Scale( XDistance, lCurrentStep, lNumSteps ) / FRACUNIT, Scale( YDistance, lCurrentStep, lNumSteps ) / FRACUNIT, StartX / FRACUNIT, StartY / FRACUNIT, DestX / FRACUNIT, DestY / FRACUNIT, X / FRACUNIT, Y / FRACUNIT );

		// Check to see if the position is blocked.
		if ( BOTPATH_IsPositionBlocked( pActor, X, Y ) == false )
		{
			// If we didn't strike a wall, then we've struck a line!
			if ( g_pBlockingActor == NULL )
			{
				ulFlags |= BOTPATH_OBSTRUCTED;
				return ( ulFlags );
			}

			if ((( g_pBlockingActor->z + g_pBlockingActor->height ) - StartZ ) > 0 )
			{
				if ((( g_pBlockingActor->z + g_pBlockingActor->height ) - StartZ ) <= gameinfo.StepHeight )
					ulFlags |= BOTPATH_STAIRS;
				else if ((( g_pBlockingActor->z + g_pBlockingActor->height ) - StartZ ) <= 36 * FRACUNIT )
					ulFlags |= BOTPATH_JUMPABLELEDGE;
				else if (( g_pBlockingActor->player == NULL ) && ((( g_pBlockingActor->z + g_pBlockingActor->height ) - StartZ ) <= 60 * FRACUNIT ))
					ulFlags |= BOTPATH_JUMPABLELEDGE;
				else
				{
					ulFlags |= BOTPATH_OBSTRUCTED;
					return ( ulFlags );
				}
			}
		/*
			// If the top of the object is too high to climb over, the path is obstructed (although couldn't you jump)?
			if ((( g_pBlockingActor->z + g_pBlockingActor->height ) - StartZ ) > gameinfo.StepHeight )
			{
				ulFlags |= BOTPATH_OBSTRUCTED;
				return ( ulFlags );
			}
		*/
		/*
			if (
				((( g_pBlockingActor->z + g_pBlockingActor->height ) - StartZ ) > gameinfo.StepHeight ) ||
				(( g_pBlockingActor->Sector->ceilingplane.ZatPoint( StartX, StartY ) - ( g_pBlockingActor->z + g_pBlockingActor->height ))) < pActor->height ) ||
				(( g_PathSectorCeilingZ - ( g_pBlockingActor->z + g_pBlockingActor->height )) < pActor->height )))
			{
				ulFlags |= BOTPATH_OBSTRUCTED;
				return ( ulFlags );
			}
		*/
			// If for some reason we're not allowed to pass over this object, flag the path as being obstructed.
			if ((( pActor->flags2 & MF2_PASSMOBJ ) == false ) || ( i_compatflags & COMPATF_NO_PASSMOBJ ))
			{
				ulFlags |= BOTPATH_OBSTRUCTED;
				return ( ulFlags );
			}
		}

		// If we can't fit into this sector because the ceiling is too low, or the sector's 
		// ceiling is below our top, potentially flag the path as being obstructed.
//		bObstructingIfNotDoor = false;
		if ((( g_PathSectorCeilingZ - g_PathSectorFloorZ ) < pActor->height ) || (( g_PathSectorCeilingZ - pActor->z ) < pActor->height ))
		{
			LONG	lIdx;

			// Check to see if this sector is actually a closed door. If the door has a linedef
			// attached to it that opens it, flag the path as having a door.
			for ( lIdx = 0; lIdx < g_pPathSector->linecount; lIdx++ )
			{
				if (( g_pPathSector->lines[lIdx]->special == Door_Open ) || ( g_pPathSector->lines[lIdx]->special == Door_Raise ))
				{
					bPotentialDoor = true;
					break;
				}
			}

			if ( bPotentialDoor == false )
			{
				ulFlags |= BOTPATH_OBSTRUCTED;
				return ( ulFlags );
			}
		}

		lHeightChange = g_PathSectorFloorZ - pActor->z;
		if ( lHeightChange > 0 )
		{
			if ( lHeightChange <= gameinfo.StepHeight )
				ulFlags |= BOTPATH_STAIRS;
			else if ( lHeightChange <= ( 60 * FRACUNIT ))
				ulFlags |= BOTPATH_JUMPABLELEDGE;
			else
			{
				ulFlags |= BOTPATH_OBSTRUCTED;
				return ( ulFlags );
			}
		}

		/*
		// If the step height is bigger than the allow set height, flag the path as being obstructed
		// (but shouldn't it be jumpable?)
		if (( g_PathSectorFloorZ - pActor->z ) > gameinfo.StepHeight )
		{
			ulFlags |= BOTPATH_OBSTRUCTED;
			return ( ulFlags );
		}
		*/
		// NO ARG! This should be fine.
		/*
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
		*/
		/*
		// killough 3/15/98: Allow certain objects to drop off
		if (!dropoff && !(thing->flags & (MF_DROPOFF|MF_FLOAT|MF_MISSILE)))
		{
			fixed_t floorz = tmfloorz;
			// [RH] If the thing is standing on something, use its current z as the floorz.
			// This is so that it does not walk off of things onto a drop off.
			if (thing->flags2 & MF2_ONMOBJ)
			{
				floorz = MAX(thing->z, tmfloorz);
			}
			if (floorz - tmdropoffz > gameinfo.StepHeight &&
				!(thing->flags2 & MF2_BLASTED))
			{ // Can't move over a dropoff unless it's been blasted
				thing->z = oldz;
				return false;
			}
		}
		*/
		// Check to see if any lines were crossed.
		while ( g_pPathLineArray.Pop( pLine ))
		{
			// Check to see if the line was crossed.
			lLineSide = P_PointOnLineSide( DestX, DestY, pLine );
			lOldLineSide = P_PointOnLineSide( StartX, StartY, pLine );
			if ( lLineSide != lOldLineSide )
			{
				sector_t		*pFrontSector;
				sector_t		*pBackSector;

				if ( lOldLineSide == 0 )
				{
					pFrontSector = pLine->frontsector;
					pBackSector = pLine->backsector;
				}
				else
				{
					pFrontSector = pLine->backsector;
					pBackSector = pLine->frontsector;
				}

				lHeightChange = pBackSector->floorplane.ZatPoint( pLine->v1 ) - pFrontSector->floorplane.ZatPoint( pLine->v1 );
				if ( lHeightChange > 0 )
				{
					if ( lHeightChange <= gameinfo.StepHeight )
						ulFlags |= BOTPATH_STAIRS;
					else if ( lHeightChange <= ( 60 * FRACUNIT ))
						ulFlags |= BOTPATH_JUMPABLELEDGE;
					else
					{
						ulFlags |= BOTPATH_OBSTRUCTED;
						return ( ulFlags );
					}
				}
				else
				{
					if ( lHeightChange <= -( 64 * FRACUNIT ))
						ulFlags |= BOTPATH_DROPOFF;
				}

				switch ( pBackSector->special )
				{
				case dDamage_End:
				case dDamage_Hellslime:
				case dDamage_SuperHellslime:
				case dLight_Strobe_Hurt:
				case dDamage_Nukage:
				case dDamage_LavaWimpy:
				case dScroll_EastLavaDamage:
				case dDamage_LavaHefty:

					ulFlags |= BOTPATH_DAMAGINGSECTOR;
					break;
				default:

					break;
				}

				if ( pBackSector->damage > 0 )
					ulFlags |= BOTPATH_DAMAGINGSECTOR;

				switch ( pLine->special )
				{
				case Teleport:
				case Teleport_NoFog:
				case Teleport_Line:

					ulFlags |= BOTPATH_TELEPORT;
					break;
				case Door_Raise:
				case Door_Open:

					if ( bPotentialDoor )
					{
						if ( pFrontSector->ceilingplane.ZatPoint( pLine->v1 ) > pBackSector->ceilingplane.ZatPoint( pLine->v1 ))
							g_pDoorSector = pBackSector;
						else
							g_pDoorSector = pFrontSector;

						ulFlags |= BOTPATH_DOOR;
					}
					break;
				default:

					break;
				}
			}
		}
	} while ( ++lCurrentStep <= lNumSteps );

//	if (( bObstructingIfNotDoor ) && (( ulFlags & BOTPATH_DOOR ) == false ))
//		ulFlags |= BOTPATH_OBSTRUCTED;

	return ( ulFlags );
}

//*****************************************************************************
//
void BOTPATH_LineOpening( line_t *pLine, fixed_t X, fixed_t Y, fixed_t RefX, fixed_t RefY )
{
	bool		bUseFront;
	sector_t	*pFrontSector;
	sector_t	*pBackSector;
	fixed_t		FrontCeiling;
	fixed_t		FrontFloor;
	fixed_t		BackCeiling;
	fixed_t		BackFloor;

	if ( pLine->sidenum[1] == NO_INDEX )
	{
		// Single sided line.
		g_OpenRange = 0;
	}

	pFrontSector = pLine->frontsector;
	pBackSector = pLine->backsector;

	FrontCeiling	= pFrontSector->ceilingplane.ZatPoint( X, Y );
	FrontFloor		= pFrontSector->floorplane.ZatPoint( X, Y );
	BackCeiling		= pBackSector->ceilingplane.ZatPoint( X, Y );
	BackFloor		= pBackSector->floorplane.ZatPoint( X, Y );

	if ( FrontCeiling < BackCeiling )
	{
		g_OpenTop = FrontCeiling;
		g_pOpenTopSector = pFrontSector;
	}
	else
	{
		g_OpenTop = BackCeiling;
		g_pOpenTopSector = pBackSector;
	}

	// [RH] fudge a bit for actors that are moving across lines
	// bordering a slope/non-slope that meet on the floor. Note
	// that imprecisions in the plane equation mean there is a
	// good chance that even if a slope and non-slope look like
	// they line up, they won't be perfectly aligned.
	if (( RefX == FIXED_MIN ) || ( abs( FrontFloor - BackFloor ) > 256 ))
		bUseFront = ( FrontFloor > BackFloor );
	else
	{
		if (( pFrontSector->floorplane.a | pFrontSector->floorplane.b ) == 0 )
			bUseFront = true;
		else if (( pBackSector->floorplane.a | pFrontSector->floorplane.b ) == 0 )
			bUseFront = false;
		else
			bUseFront = !P_PointOnLineSide( RefX, RefY, pLine );
	}

	if ( bUseFront )
	{
		g_OpenBottom = FrontFloor;
		g_pOpenBottomSector = pFrontSector;
		g_LowFloor = BackFloor;
	}
	else
	{
		g_OpenBottom = BackFloor;
		g_pOpenBottomSector = pBackSector;
		g_LowFloor = FrontFloor;
	}

	g_OpenRange = g_OpenTop - g_OpenBottom;
}

//*****************************************************************************
//
sector_t *BOTPATH_GetDoorSector( void )
{
	return ( g_pDoorSector );
}

//*****************************************************************************
//*****************************************************************************
//
static bool botpath_CheckThing( AActor *pThing )
{
	fixed_t blockdist;
				
	// Don't clip against ourselves.
	if ( pThing == g_pPathActor )
		return ( true );

	// Thing cannot block.
	if (( pThing->flags & MF_SOLID ) == false )
		return ( true );

	blockdist = pThing->radius + g_pPathActor->radius;
	if (( abs( pThing->x - g_PathX ) >= blockdist ) || ( abs( pThing->y - g_PathY ) >= blockdist ))
	{
		// Didn't come into contact with the thing.
		return ( true );
	}

	g_pBlockingActor = pThing;

	// Check if the item is above or below the thing.
	if (( i_compatflags & COMPATF_NO_PASSMOBJ ) == false )
	{
		if (( g_pPathActor->z >= ( pThing->z + pThing->height )) || (( g_pPathActor->z + g_pPathActor->height ) <= pThing->z ))
			return ( true );
	}

	// If this object is the bot's enemy, don't allow it to block.
	if (( g_pPathActor->player ) && ( g_pPathActor->player->pSkullBot ) && ( g_pPathActor->player->pSkullBot->m_ulPathType == BOTPATHTYPE_ENEMYPOSITION ) && ( pThing == players[g_pPathActor->player->pSkullBot->m_ulPlayerEnemy].mo ))
		return ( true );

	// Didn't pass the culling. I guess it's blocking!
	return ( false );
}

//*****************************************************************************
//
static bool botpath_CheckLine( line_t *pLine )
{
	// Lines don't intersect at all.
	if (( g_BoundingBox[BOXRIGHT] <= pLine->bbox[BOXLEFT] ) ||
		( g_BoundingBox[BOXLEFT] >= pLine->bbox[BOXRIGHT] ) ||
		( g_BoundingBox[BOXTOP] <= pLine->bbox[BOXBOTTOM] ) ||
		( g_BoundingBox[BOXBOTTOM] >= pLine->bbox[BOXTOP] ))
	{
		return ( true );
	}

	if ( P_BoxOnLineSide( g_BoundingBox, pLine ) != -1 )
		return ( true );

	// A line has been hit.

	// This is a one-sided line. It must be a blocking line.
	if (( pLine->backsector == NULL ) || ( pLine->flags & (ML_BLOCKING|ML_BLOCKEVERYTHING|ML_BLOCKPLAYERS) ))
	{
		g_pBlockingLine = pLine;
		return ( false );
	}
/*
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
*/
	fixed_t sx, sy;

	// set openrange, opentop, openbottom
	if ((( pLine->frontsector->floorplane.a | pLine->frontsector->floorplane.b ) |
		 ( pLine->backsector->floorplane.a | pLine->backsector->floorplane.b ) |
		 ( pLine->frontsector->ceilingplane.a | pLine->frontsector->ceilingplane.b ) |
		 ( pLine->backsector->ceilingplane.a | pLine->backsector->ceilingplane.b )) == 0 )
	{
		BOTPATH_LineOpening( pLine, sx = g_PathX, sy = g_PathY, g_PathX, g_PathY );
	}
	else
	{
		// Find the point on the line closest to the actor's center, and use
		// that to calculate openings.
		float dx = (float)pLine->dx;
		float dy = (float)pLine->dy;
		fixed_t r = (fixed_t)(((float)(g_PathX - pLine->v1->x) * dx +
				 			   (float)(g_PathY - pLine->v1->y) * dy) /
							  (dx*dx + dy*dy) * 16777216.f);

		if (r <= 0)
		{
			BOTPATH_LineOpening( pLine, sx = pLine->v1->x, sy = pLine->v1->y, g_PathX, g_PathY );
		}
		else if (r >= (1<<24))
		{
			BOTPATH_LineOpening( pLine, sx = pLine->v2->x, sy = pLine->v2->y, g_pPathActor->x, g_pPathActor->y );
		}
		else
		{
			BOTPATH_LineOpening( pLine, sx=pLine->v1->x + MulScale24 (r, pLine->dx),
				sy=pLine->v1->y + MulScale24 (r, pLine->dy), g_PathX, g_PathY );
		}
	}

	// Adjust floor / ceiling heights.
	if ( g_OpenTop < g_PathSectorCeilingZ )
	{
		LONG	lIdx;

		// Check to see if this sector is actually a closed door. If the door has a linedef
		// attached to it that opens it, flag the path as having a door.
		for ( lIdx = 0; lIdx < g_pOpenTopSector->linecount; lIdx++ )
		{
			if (( g_pOpenTopSector->lines[lIdx]->special == Door_Open ) || ( g_pOpenTopSector->lines[lIdx]->special == Door_Raise ))
			{
				// Add this line to the list, so we can decide what to do with it later.
				g_pPathLineArray.Push( pLine );
				return ( true );
			}
		}

		g_PathSectorCeilingZ = g_OpenTop;
		g_pBlockingLine = pLine;
	}

	if ( g_OpenBottom > g_PathSectorFloorZ )
	{
		g_PathSectorFloorZ = g_OpenBottom;
		g_pBlockingLine = pLine;
	}

	// Add this line to the list, so we can decide what to do with it later.
	g_pPathLineArray.Push( pLine );
	return ( true );
}
