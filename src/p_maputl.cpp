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
//		Movement/collision utility functions,
//		as used by function in p_map.c. 
//		BLOCKMAP Iterator functions,
//		and some PIT_* functions to use for iteration.
//
//-----------------------------------------------------------------------------



#include <stdlib.h>


#include "m_bbox.h"

#include "doomdef.h"
#include "doomstat.h"
#include "p_local.h"

// State.
#include "r_state.h"
#include "templates.h"
#include "vectors.h"

static AActor *RoughBlockCheck (AActor *mo, int index);


//==========================================================================
//
// P_AproxDistance
//
// Gives an estimation of distance (not exact)
// 
//==========================================================================

fixed_t P_AproxDistance (fixed_t dx, fixed_t dy)
{
	dx = abs(dx);
	dy = abs(dy);
	return (dx < dy) ? dx+dy-(dx>>1) : dx+dy-(dy>>1);
}

//==========================================================================
//
// P_BoxOnLineSide
//
// Considers the line to be infinite
// Returns side 0 or 1, -1 if box crosses the line.
//
//==========================================================================

int P_BoxOnLineSide (const fixed_t *tmbox, const line_t *ld)
{
	int p1;
	int p2;
		
	switch (ld->slopetype)
	{
	case ST_HORIZONTAL:
		p1 = tmbox[BOXTOP] > ld->v1->y;
		p2 = tmbox[BOXBOTTOM] > ld->v1->y;
		if (ld->dx < 0)
		{
			p1 ^= 1;
			p2 ^= 1;
		}
		break;
		
	case ST_VERTICAL:
		p1 = tmbox[BOXRIGHT] < ld->v1->x;
		p2 = tmbox[BOXLEFT] < ld->v1->x;
		if (ld->dy < 0)
		{
			p1 ^= 1;
			p2 ^= 1;
		}
		break;
		
	case ST_POSITIVE:
		p1 = P_PointOnLineSide (tmbox[BOXLEFT], tmbox[BOXTOP], ld);
		p2 = P_PointOnLineSide (tmbox[BOXRIGHT], tmbox[BOXBOTTOM], ld);
		break;
		
	case ST_NEGATIVE:
	default:	// Just to assure GCC that p1 and p2 really do get initialized
		p1 = P_PointOnLineSide (tmbox[BOXRIGHT], tmbox[BOXTOP], ld);
		p2 = P_PointOnLineSide (tmbox[BOXLEFT], tmbox[BOXBOTTOM], ld);
		break;
	}

	return (p1 == p2) ? p1 : -1;
}

//==========================================================================
//
// P_InterceptVector
//
// Returns the fractional intercept point along the first divline.
// This is only called by the addthings and addlines traversers.
//
//==========================================================================

fixed_t P_InterceptVector (const divline_t *v2, const divline_t *v1)
{
#if 1	// [RH] Use 64 bit ints, so long divlines don't overflow

	SQWORD den = ((SQWORD)v1->dy*v2->dx - (SQWORD)v1->dx*v2->dy) >> FRACBITS;
	if (den == 0)
		return 0;		// parallel
	SQWORD num = ((SQWORD)(v1->x - v2->x)*v1->dy + (SQWORD)(v2->y - v1->y)*v1->dx);
	return (fixed_t)(num / den);

#elif 1	// This is the original Doom version

	fixed_t 	frac;
	fixed_t 	num;
	fixed_t 	den;

	den = FixedMul (v1->dy>>8,v2->dx) - FixedMul(v1->dx>>8,v2->dy);

	if (den == 0)
		return 0;
	//	I_Error ("P_InterceptVector: parallel");
	
	num =
		FixedMul ( (v1->x - v2->x)>>8 ,v1->dy )
		+FixedMul ( (v2->y - v1->y)>>8, v1->dx );

	frac = FixedDiv (num , den);

	return frac;

#else	// UNUSED, float debug.

	float frac;
	float num;
	float den;
	float v1x = (float)v1->x/FRACUNIT;
	float v1y = (float)v1->y/FRACUNIT;
	float v1dx = (float)v1->dx/FRACUNIT;
	float v1dy = (float)v1->dy/FRACUNIT;
	float v2x = (float)v2->x/FRACUNIT;
	float v2y = (float)v2->y/FRACUNIT;
	float v2dx = (float)v2->dx/FRACUNIT;
	float v2dy = (float)v2->dy/FRACUNIT;
		
	den = v1dy*v2dx - v1dx*v2dy;

	if (den == 0)
		return 0;		// parallel
	
	num = (v1x - v2x)*v1dy + (v2y - v1y)*v1dx;
	frac = num / den;

	return frac*FRACUNIT;
#endif
}

//==========================================================================
//
// P_LineOpening
//
// Sets opentop and openbottom to the window
// through a two sided line.
// OPTIMIZE: keep this precalculated
//
//==========================================================================

fixed_t opentop;
fixed_t openbottom;
fixed_t openrange;
fixed_t lowfloor;
extern int tmfloorpic;
sector_t *openbottomsec;
sector_t *opentopsec;

void P_LineOpening (const line_t *linedef, fixed_t x, fixed_t y, fixed_t refx, fixed_t refy)
{
	sector_t *front, *back;
	fixed_t fc, ff, bc, bf;

	if (linedef->sidenum[1] == NO_SIDE)
	{
		// single sided line
		openrange = 0;
		return;
	}

	front = linedef->frontsector;
	back = linedef->backsector;

	fc = front->ceilingplane.ZatPoint (x, y);
	ff = front->floorplane.ZatPoint (x, y);
	bc = back->ceilingplane.ZatPoint (x, y);
	bf = back->floorplane.ZatPoint (x, y);

	/*Printf ("]]]]]] %d %d\n", ff, bf);*/

	opentopsec = fc < bc? front : back;
	opentop = fc < bc ? fc : bc;

	bool usefront;

	// [RH] fudge a bit for actors that are moving across lines
	// bordering a slope/non-slope that meet on the floor. Note
	// that imprecisions in the plane equation mean there is a
	// good chance that even if a slope and non-slope look like
	// they line up, they won't be perfectly aligned.
	if (refx == FIXED_MIN ||
		abs (ff-bf) > 256)
	{
		usefront = (ff > bf);
	}
	else
	{
		if ((front->floorplane.a | front->floorplane.b) == 0)
			usefront = true;
		else if ((back->floorplane.a | front->floorplane.b) == 0)
			usefront = false;
		else
			usefront = !P_PointOnLineSide (refx, refy, linedef);
	}

	if (usefront)
	{
		openbottom = ff;
		openbottomsec = front;
		lowfloor = bf;
		//tmfloorpic = front->floorpic;
	}
	else
	{
		openbottom = bf;
		openbottomsec = back;
		lowfloor = ff;
		//tmfloorpic = back->floorpic;
	}

	openrange = opentop - openbottom;
}

// [BC] ugh old code for people who just have to have doom2.exe style movement.
void P_OldLineOpening(const line_t *linedef, fixed_t x, fixed_t y)
{
	sector_t *front, *back;

	if (linedef->sidenum[1] == -1)      // single sided line
	{
		openrange = 0;
		return;
	}

	front = linedef->frontsector;
	back = linedef->backsector;
  
	if (front->ceilingplane.ZatPoint( x, y ) < back->ceilingplane.ZatPoint( x, y ))
		opentop = front->ceilingplane.ZatPoint( x, y );
	else
		opentop = back->ceilingplane.ZatPoint( x, y );

	if (front->floorplane.ZatPoint( x, y ) < back->floorplane.ZatPoint( x, y ))
	{
		openbottom = front->floorplane.ZatPoint( x, y );
		lowfloor = back->floorplane.ZatPoint( x, y );
	}
	else
	{
		openbottom = back->floorplane.ZatPoint( x, y );
		lowfloor = front->floorplane.ZatPoint( x, y );
	}
	openrange = opentop - openbottom;
}


//
// THING POSITION SETTING
//

//
// P_UnsetThingPosition
// Unlinks a thing from block map and sectors.
// On each position change, BLOCKMAP and other
// lookups maintaining lists of things inside
// these structures need to be updated.
//
void AActor::UnlinkFromWorld ()
{
	sector_list = NULL;
	if (!(flags & MF_NOSECTOR))
	{
		// invisible things don't need to be in sector list
		// unlink from subsector

		// killough 8/11/98: simpler scheme using pointers-to-pointers for prev
		// pointers, allows head node pointers to be treated like everything else
		AActor **prev = sprev;
		AActor  *next = snext;
		if ((*prev = next))  // unlink from sector list
			next->sprev = prev;
		snext = NULL;
		sprev = (AActor **)(size_t)0xBeefCafe;	// Woo! Bug-catching value!

		// phares 3/14/98
		//
		// Save the sector list pointed to by touching_sectorlist.
		// In P_SetThingPosition, we'll keep any nodes that represent
		// sectors the Thing still touches. We'll add new ones then, and
		// delete any nodes for sectors the Thing has vacated. Then we'll
		// put it back into touching_sectorlist. It's done this way to
		// avoid a lot of deleting/creating for nodes, when most of the
		// time you just get back what you deleted anyway.
		//
		// If this Thing is being removed entirely, then the calling
		// routine will clear out the nodes in sector_list.

		sector_list = touching_sectorlist;
		touching_sectorlist = NULL; //to be restored by P_SetThingPosition
	}
		
	if (!(flags & MF_NOBLOCKMAP))
	{
		// [RH] Unlink from all blocks this actor uses
		FBlockNode *block = this->BlockNode;

		while (block != NULL)
		{
			if (block->NextActor != NULL)
			{
				block->NextActor->PrevActor = block->PrevActor;
			}
			*(block->PrevActor) = block->NextActor;
			FBlockNode *next = block->NextBlock;
			block->Release ();
			block = next;
		}
		BlockNode = NULL;
	}
}


//
// P_SetThingPosition
// Links a thing into both a block and a subsector based on it's x y.
// Sets thing->sector properly
//
void AActor::LinkToWorld (bool buggy)
{
	// link into subsector
	sector_t *sec;

	if (!buggy || numnodes == 0)
	{
		sec = R_PointInSubsector (x, y)->sector;
	}
	else
	{
		sec = LinkToWorldForMapThing ();
	}

	LinkToWorld (sec);
}

void AActor::LinkToWorld (sector_t *sec)
{
	if (sec == NULL)
	{
		LinkToWorld ();
		return;
	}
	Sector = sec;
	subsector = R_PointInSubsector2(x, y);	// this is from the rendering nodes, not the gameplay nodes!

	if ( !(flags & MF_NOSECTOR) )
	{
		// invisible things don't go into the sector links
		// killough 8/11/98: simpler scheme using pointer-to-pointer prev
		// pointers, allows head nodes to be treated like everything else

		AActor **link = &sec->thinglist;
		AActor *next = *link;
		if ((snext = next))
			next->sprev = &snext;
		sprev = link;
		*link = this;

		// phares 3/16/98
		//
		// If sector_list isn't NULL, it has a collection of sector
		// nodes that were just removed from this Thing.

		// Collect the sectors the object will live in by looking at
		// the existing sector_list and adding new nodes and deleting
		// obsolete ones.

		// When a node is deleted, its sector links (the links starting
		// at sector_t->touching_thinglist) are broken. When a node is
		// added, new sector links are created.
		P_CreateSecNodeList (this, x, y);
		touching_sectorlist = sector_list;	// Attach to thing
		sector_list = NULL;		// clear for next time
    }

	
	// link into blockmap (inert things don't need to be in the blockmap)
	if ( !(flags & MF_NOBLOCKMAP) )
	{
		int x1 = (x - radius - bmaporgx)>>MAPBLOCKSHIFT;
		int x2 = (x + radius - bmaporgx)>>MAPBLOCKSHIFT;
		int y1 = (y - radius - bmaporgy)>>MAPBLOCKSHIFT;
		int y2 = (y + radius - bmaporgy)>>MAPBLOCKSHIFT;

		if (x1 >= bmapwidth || x2 < 0 || y1 >= bmapheight || y2 < 0)
		{ // thing is off the map
			BlockNode = NULL;
		}
		else
        { // [RH] Link into every block this actor touches, not just the center one
			FBlockNode **alink = &this->BlockNode;
			x1 = MAX (0, x1);
			y1 = MAX (0, y1);
			x2 = MIN (bmapwidth - 1, x2);
			y2 = MIN (bmapheight - 1, y2);
			for (int y = y1; y <= y2; ++y)
			{
				for (int x = x1; x <= x2; ++x)
				{
					FBlockNode **link = &blocklinks[y*bmapwidth + x];
					FBlockNode *node = FBlockNode::Create (this, x, y);

					// Link in to block
					if ((node->NextActor = *link) != NULL)
					{
						(*link)->PrevActor = &node->NextActor;
					}
					node->PrevActor = link;
					*link = node;

					// Link in to actor
					node->PrevBlock = alink;
					node->NextBlock = NULL;
					(*alink) = node;
					alink = &node->NextBlock;
				}
			}
		}
	}
}

//
// [RH] LinkToWorldForMapThing
//
// Emulate buggy PointOnLineSide and fix actors that lie on
// lines to compensate for some IWAD maps.
//
static int R_PointOnSideSlow (fixed_t x, fixed_t y, node_t *node)
{
	// [RH] This might have been faster than two multiplies and an
	// add on a 386/486, but it certainly isn't on anything newer than that.
	fixed_t	dx;
	fixed_t	dy;
	fixed_t	left;
	fixed_t	right;

	if (!node->dx)
	{
		if (x <= node->x)
			return node->dy > 0;

		return node->dy < 0;
	}
	if (!node->dy)
	{
		if (y <= node->y)
			return node->dx < 0;

		return node->dx > 0;
	}

	dx = (x - node->x);
	dy = (y - node->y);

	// Try to quickly decide by looking at sign bits.
	if ( (node->dy ^ node->dx ^ dx ^ dy)&0x80000000 )
	{
		if  ( (node->dy ^ dx) & 0x80000000 )
		{
			// (left is negative)
			return 1;
		}
		return 0;
	}

	left = FixedMul ( node->dy>>FRACBITS , dx );
	right = FixedMul ( dy , node->dx>>FRACBITS );

	if (right < left)
	{
		// front side
		return 0;
	}
	// back side
	return 1;
}

sector_t *AActor::LinkToWorldForMapThing ()
{
	// [GZDoom]
	node_t *node = gamenodes + numgamenodes - 1;
	//node_t *node = nodes + numnodes - 1;

	do
	{
		// Use original buggy point-on-side test when spawning
		// things at level load so that the map spots in the
		// emerald key room of Hexen MAP01 are spawned on the
		// window ledge instead of the blocking floor in front
		// of it. Why do I consider it buggy? Because a point
		// that lies directly on a line should always be
		// considered as "in front" of the line. The orientation
		// of the line should be irrelevant.
		node = (node_t *)node->children[R_PointOnSideSlow (x, y, node)];
	}
	while (!((size_t)node & 1));

	subsector_t *ssec = (subsector_t *)((BYTE *)node - 1);

	if (flags4 & MF4_FIXMAPTHINGPOS)
	{
		// If the thing is exactly on a line, move it into the subsector
		// slightly in order to resolve clipping issues in the renderer.
		// This check needs to use the blockmap, because an actor on a
		// one-sided line might go into a subsector behind the line, so
		// the line would not be included as one of its subsector's segs.

		int blockx = (x - bmaporgx) >> MAPBLOCKSHIFT;
		int blocky = (y - bmaporgy) >> MAPBLOCKSHIFT;

		if ((unsigned int)blockx < (unsigned int)bmapwidth &&
			(unsigned int)blocky < (unsigned int)bmapheight)
		{
			int *list;

			for (list = blockmaplump + blockmap[blocky*bmapwidth + blockx] + 1; *list != -1; ++list)
			{
				line_t *ldef = &lines[*list];

				if (ldef->frontsector == ldef->backsector)
				{ // Skip two-sided lines inside a single sector
					continue;
				}
				if (ldef->backsector != NULL)
				{
					if (ldef->frontsector->floorplane == ldef->backsector->floorplane &&
						ldef->frontsector->ceilingplane == ldef->backsector->ceilingplane)
					{ // Skip two-sided lines without any height difference on either side
						continue;
					}
				}

#if 1
				// Not inside the line's bounding box
				if (x + radius <= ldef->bbox[BOXLEFT]
					|| x - radius >= ldef->bbox[BOXRIGHT]
					|| y + radius <= ldef->bbox[BOXBOTTOM]
					|| y - radius >= ldef->bbox[BOXTOP] )
					continue;

				// Get the exact distance to the line
				divline_t dll, dlv;
				fixed_t linelen = (fixed_t)sqrt((double)ldef->dx*ldef->dx + (double)ldef->dy*ldef->dy);

				P_MakeDivline (ldef, &dll);

				dlv.x = x;
				dlv.y = y;
				dlv.dx = FixedDiv(dll.dy, linelen);
				dlv.dy = -FixedDiv(dll.dx, linelen);

				fixed_t distance = abs(P_InterceptVector(&dlv, &dll));

				if (distance < radius)
				{
					DPrintf ("%s at (%d,%d) lies on %s line %d, distance = %f\n",
						this->GetClass()->TypeName.GetChars(), x>>FRACBITS, y>>FRACBITS, 
						ldef->dx == 0? "vertical" :	ldef->dy == 0? "horizontal" : "diagonal",
						ldef-lines, FIXED2FLOAT(distance));
					angle_t finean = R_PointToAngle2 (0, 0, ldef->dx, ldef->dy);
					if (ldef->backsector != NULL && ldef->backsector == ssec->sector)
					{
						finean += ANGLE_90;
					}
					else
					{
						finean -= ANGLE_90;
					}
					finean >>= ANGLETOFINESHIFT;

					// Get the distance we have to move the object away from the wall
					distance = radius - distance;
					x += FixedMul(distance, finecosine[finean]);
					y += FixedMul(distance, finesine[finean]);
					ssec = R_PointInSubsector (x, y);
					break;
				}
#else
				if (DMulScale32 (y - ldef->v1->y, ldef->dx, ldef->v1->x - x, ldef->dy) == 0)
				{
					// It touches the infinite line; now make sure it touches the linedef
					SQWORD num, den;

					den = (SQWORD)ldef->dx*ldef->dx + (SQWORD)ldef->dy*ldef->dy;
					if (den != 0)
					{
						num = (SQWORD)(x-ldef->v1->x)*ldef->dx+(SQWORD)(y-ldef->v1->y)*ldef->dy;
						if (num >= 0 && num <= den)
						{
							DPrintf ("%s at (%d,%d) lies directly on %s line %d\n",
								this->GetClass()->TypeName.GetChars(), x>>FRACBITS, y>>FRACBITS, 
								ldef->dx == 0? "vertical" :	ldef->dy == 0? "horizontal" : "diagonal",
								ldef-lines);
							angle_t finean = R_PointToAngle2 (0, 0, ldef->dx, ldef->dy);
							if (ldef->backsector != NULL && ldef->backsector == ssec->sector)
							{
								finean += ANGLE_90;
							}
							else
							{
								finean -= ANGLE_90;
							}
							finean >>= ANGLETOFINESHIFT;
							x += finecosine[finean]) >> 2;
							y += finesine[finean]) >> 2;
							break;
						}
					}
				}
#endif
			}
		}
	}

	return ssec->sector;
}

void AActor::SetOrigin (fixed_t ix, fixed_t iy, fixed_t iz)
{
	UnlinkFromWorld ();
	x = ix;
	y = iy;
	z = iz;
	LinkToWorld ();
	floorz = Sector->floorplane.ZatPoint (ix, iy);
	ceilingz = Sector->ceilingplane.ZatPoint (ix, iy);
}

FBlockNode *FBlockNode::FreeBlocks = NULL;

FBlockNode *FBlockNode::Create (AActor *who, int x, int y)
{
	FBlockNode *block;

	if (FreeBlocks != NULL)
	{
		block = FreeBlocks;
		FreeBlocks = block->NextBlock;
	}
	else
	{
		block = new FBlockNode;
	}
	block->BlockIndex = x + y*bmapwidth;
	block->Me = who;
	block->NextActor = NULL;
	block->PrevActor = NULL;
	block->PrevBlock = NULL;
	block->NextBlock = NULL;
	return block;
}

void FBlockNode::Release ()
{
	NextBlock = FreeBlocks;
	FreeBlocks = this;
}

//
// BLOCK MAP ITERATORS
// For each line/thing in the given mapblock,
// call the passed PIT_* function.
// If the function returns false,
// exit with false without checking anything else.
//


//
// P_BlockLinesIterator
// The validcount flags are used to avoid checking lines
// that are marked in multiple mapblocks,
// so increment validcount before the first call
// to P_BlockLinesIterator, then make one or more calls
// to it.
//
extern polyblock_t **PolyBlockMap;

bool P_BlockLinesIterator (int x, int y, bool(*func)(line_t*))
{
	if (x<0 || y<0 || x>=bmapwidth || y>=bmapheight)
	{
		return true;
	}
	else
	{
		int	offset;
		int *list;

		/* [RH] Polyobj stuff from Hexen --> */
		polyblock_t *polyLink;

		offset = y*bmapwidth + x;
		if (PolyBlockMap)
		{
			polyLink = PolyBlockMap[offset];
			while (polyLink)
			{
				if (polyLink->polyobj && polyLink->polyobj->validcount != validcount)
				{
					int i;
					seg_t **tempSeg = polyLink->polyobj->segs;
					polyLink->polyobj->validcount = validcount;

					for (i = polyLink->polyobj->numsegs; i; i--, tempSeg++)
					{
						if ((*tempSeg)->linedef->validcount != validcount)
						{
							(*tempSeg)->linedef->validcount = validcount;
							if (!func ((*tempSeg)->linedef))
								return false;
						}
					}
				}
				polyLink = polyLink->next;
			}
		}
		/* <-- Polyobj stuff from Hexen */

		offset = *(blockmap + offset);

		// There is an extra entry at the beginning of every block.
		// Apparently, id had originally intended for it to be used
		// to keep track of things, but the final code does not do that.
		for (list = blockmaplump + offset + 1; *list != -1; list++)
		{
			line_t *ld = &lines[*list];

			if (ld->validcount != validcount)
			{
				ld->validcount = validcount;
					
				if ( !func(ld) )
					return false;
			}
		}
	}
	return true;		// everything was checked
}


//
// P_BlockThingsIterator
//

bool P_BlockThingsIterator (int x, int y, bool(*func)(AActor*), TArray<AActor *> &checkarray, AActor *actor)
{
	if ((unsigned int)x >= (unsigned int)bmapwidth ||
		(unsigned int)y >= (unsigned int)bmapheight)
	{
		return true;
	}
	else
	{
		FBlockNode *block;
		int index = y*bmapwidth + x;

		if (actor == NULL)
		{
			block = blocklinks[index];
		}
		else
		{
			block = actor->BlockNode;
			while (block != NULL && block->BlockIndex != index)
			{
				block = block->NextBlock;
			}
			if (block != NULL)
			{
				block = block->NextActor;
			}
		}
		while (block != NULL)
		{
			FBlockNode *next = block->NextActor;
			int i;

			// Don't recheck things that were already checked
			for (i = (int)checkarray.Size() - 1; i >= 0; --i)
			{
				if (checkarray[i] == block->Me)
				{
					break;
				}
			}
			if (i < 0)
			{
				checkarray.Push (block->Me);
				if (!func (block->Me))
				{
					return false;
				}
			}
			block = next;
		}
	}
	return true;
}



//
// INTERCEPT ROUTINES
//
TArray<intercept_t> intercepts (128);

divline_t		trace;
INTBOOL			earlyout;
int 			ptflags;

//
// PIT_AddLineIntercepts.
// Looks for lines in the given block
// that intercept the given trace
// to add to the intercepts list.
//
// A line is crossed if its endpoints
// are on opposite sides of the trace.
// Returns true if earlyout and a solid line hit.
//
bool PIT_AddLineIntercepts (line_t *ld)
{
	int 				s1;
	int 				s2;
	fixed_t 			frac;
	divline_t			dl;

	// avoid precision problems with two routines
	if ( trace.dx > FRACUNIT*16
		 || trace.dy > FRACUNIT*16
		 || trace.dx < -FRACUNIT*16
		 || trace.dy < -FRACUNIT*16)
	{
		s1 = P_PointOnDivlineSide (ld->v1->x, ld->v1->y, &trace);
		s2 = P_PointOnDivlineSide (ld->v2->x, ld->v2->y, &trace);
	}
	else
	{
		s1 = P_PointOnLineSide (trace.x, trace.y, ld);
		s2 = P_PointOnLineSide (trace.x+trace.dx, trace.y+trace.dy, ld);
	}
	
	if (s1 == s2)
		return true;	// line isn't crossed
	
	// hit the line
	P_MakeDivline (ld, &dl);
	frac = P_InterceptVector (&trace, &dl);

	if (frac < 0)
		return true;	// behind source
		
	// try to early out the check
	if (earlyout
		&& frac < FRACUNIT
		&& !ld->backsector)
	{
		return false;	// stop checking
	}
	

	intercept_t newintercept;

	newintercept.frac = frac;
	newintercept.isaline = true;
	newintercept.d.line = ld;
	intercepts.Push (newintercept);

	return true;		// continue
}



//
// PIT_AddThingIntercepts
//
bool PIT_AddThingIntercepts (AActor* thing)
{
	int numfronts = 0;
	divline_t line;
	int i;

	// [RH] Don't check a corner to corner crossection for hit.
	// Instead, check against the actual bounding box.

	// There's probably a smarter way to determine which two sides
	// of the thing face the trace than by trying all four sides...
	for (i = 0; i < 4; ++i)
	{
		switch (i)
		{
		case 0:		// Top edge
			line.x = thing->x + thing->radius;
			line.y = thing->y + thing->radius;
			line.dx = -thing->radius * 2;
			line.dy = 0;
			break;

		case 1:		// Right edge
			line.x = thing->x + thing->radius;
			line.y = thing->y - thing->radius;
			line.dx = 0;
			line.dy = thing->radius * 2;
			break;

		case 2:		// Bottom edge
			line.x = thing->x - thing->radius;
			line.y = thing->y - thing->radius;
			line.dx = thing->radius * 2;
			line.dy = 0;
			break;

		case 3:		// Left edge
			line.x = thing->x - thing->radius;
			line.y = thing->y + thing->radius;
			line.dx = 0;
			line.dy = thing->radius * -2;
			break;
		}
		// Check if this side is facing the trace origin
		if (P_PointOnDivlineSide (trace.x, trace.y, &line) == 0)
		{
			numfronts++;

			// If it is, see if the trace crosses it
			if (P_PointOnDivlineSide (line.x, line.y, &trace) !=
				P_PointOnDivlineSide (line.x + line.dx, line.y + line.dy, &trace))
			{
				// It's a hit
				fixed_t frac = P_InterceptVector (&trace, &line);
				if (frac < 0)
				{ // behind source
					return true;
				}

				intercept_t newintercept;
				newintercept.frac = frac;
				newintercept.isaline = false;
				newintercept.d.thing = thing;
				intercepts.Push (newintercept);
				return true;	// keep going
			}
		}
	}

	// If none of the sides was facing the trace, then the trace
	// must have started inside the box, so add it as an intercept.
	if (numfronts == 0)
	{
		intercept_t newintercept;
		newintercept.frac = 0;
		newintercept.isaline = false;
		newintercept.d.thing = thing;
		intercepts.Push (newintercept);
		return true;	// keep going
	}

	// Didn't hit it
	return true;
}


//
// P_TraverseIntercepts
// Returns true if the traverser function returns true
// for all lines.
// 
bool P_TraverseIntercepts (traverser_t func, fixed_t maxfrac)
{
	unsigned int count;
	fixed_t 	 dist;
	unsigned int scanpos;
	intercept_t *scan;
	intercept_t *in = NULL;

	count = intercepts.Size ();

	while (count--)
	{
		dist = FIXED_MAX;
		for (scanpos = 0; scanpos < intercepts.Size (); scanpos++)
		{
			scan = &intercepts[scanpos];
			if (scan->frac < dist)
			{
				dist = scan->frac;
				in = scan;
			}
		}
		
		if (dist > maxfrac || in == NULL)
			return true;		// checked everything in range			

		if (!func (in))
			return false;		// don't bother going farther

		in->frac = FIXED_MAX;
	}
		
	return true;				// everything was traversed
}




//
// P_PathTraverse
// Traces a line from x1,y1 to x2,y2,
// calling the traverser function for each.
// Returns true if the traverser function returns true
// for all lines.
//
bool P_PathTraverse (fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2, int flags, bool (*trav) (intercept_t *))
{
	static TArray<AActor *> pathbt;

	fixed_t 	xt1;
	fixed_t 	yt1;
	fixed_t 	xt2;
	fixed_t 	yt2;
	
	fixed_t 	xstep;
	fixed_t 	ystep;
	
	fixed_t 	partialx, partialy;
	
	fixed_t 	xintercept;
	fixed_t 	yintercept;
	
	int 		mapx;
	int 		mapy;
	
	int 		mapxstep;
	int 		mapystep;

	int 		count;
				
	earlyout = flags & PT_EARLYOUT;
				
	validcount++;
	intercepts.Clear ();
	pathbt.Clear ();
		
	if ( ((x1-bmaporgx)&(MAPBLOCKSIZE-1)) == 0)
		x1 += FRACUNIT; // don't side exactly on a line
	
	if ( ((y1-bmaporgy)&(MAPBLOCKSIZE-1)) == 0)
		y1 += FRACUNIT; // don't side exactly on a line

	trace.x = x1;
	trace.y = y1;
	trace.dx = x2 - x1;
	trace.dy = y2 - y1;

	x1 -= bmaporgx;
	y1 -= bmaporgy;
	xt1 = x1>>MAPBLOCKSHIFT;
	yt1 = y1>>MAPBLOCKSHIFT;

	x2 -= bmaporgx;
	y2 -= bmaporgy;
	xt2 = x2>>MAPBLOCKSHIFT;
	yt2 = y2>>MAPBLOCKSHIFT;

	if (xt2 > xt1)
	{
		mapxstep = 1;
		partialx = FRACUNIT - ((x1>>MAPBTOFRAC)&(FRACUNIT-1));
		ystep = FixedDiv (y2-y1,abs(x2-x1));
	}
	else if (xt2 < xt1)
	{
		mapxstep = -1;
		partialx = (x1>>MAPBTOFRAC)&(FRACUNIT-1);
		ystep = FixedDiv (y2-y1,abs(x2-x1));
	}
	else
	{
		mapxstep = 0;
		partialx = FRACUNIT;
		ystep = 256*FRACUNIT;
	}	
	yintercept = (y1>>MAPBTOFRAC) + FixedMul (partialx, ystep);

		
	if (yt2 > yt1)
	{
		mapystep = 1;
		partialy = FRACUNIT - ((y1>>MAPBTOFRAC)&(FRACUNIT-1));
		xstep = FixedDiv (x2-x1,abs(y2-y1));
	}
	else if (yt2 < yt1)
	{
		mapystep = -1;
		partialy = (y1>>MAPBTOFRAC)&(FRACUNIT-1);
		xstep = FixedDiv (x2-x1,abs(y2-y1));
	}
	else
	{
		mapystep = 0;
		partialy = FRACUNIT;
		xstep = 256*FRACUNIT;
	}	
	xintercept = (x1>>MAPBTOFRAC) + FixedMul (partialy, xstep);

	// [RH] Fix for traces that pass only through blockmap corners. In that case,
	// xintercept and yintercept can both be set ahead of mapx and mapy, so the
	// for loop would never advance anywhere.

	if (abs(xstep) == FRACUNIT && abs(ystep) == FRACUNIT)
	{
		if (ystep < 0)
		{
			partialx = FRACUNIT - partialx;
		}
		if (xstep < 0)
		{
			partialy = FRACUNIT - partialy;
		}
		if (partialx == partialy)
		{
			xintercept = xt1 << FRACBITS;
			yintercept = yt1 << FRACBITS;
		}
	}

	// Step through map blocks.
	// Count is present to prevent a round off error
	// from skipping the break statement.
	mapx = xt1;
	mapy = yt1;
		
	for (count = 0 ; count < 100 ; count++)
	{
		if (flags & PT_ADDLINES)
		{
			if (!P_BlockLinesIterator (mapx, mapy, PIT_AddLineIntercepts))
				return false;	// early out
		}
		
		if (flags & PT_ADDTHINGS)
		{
			if (!P_BlockThingsIterator (mapx, mapy, PIT_AddThingIntercepts, pathbt))
				return false;	// early out
		}
				
		if (mapx == xt2 && mapy == yt2)
		{
			break;
		}

		// [RH] Handle corner cases properly instead of pretending they don't exist.
		switch ((((yintercept >> FRACBITS) == mapy) << 1) | ((xintercept >> FRACBITS) == mapx))
		{
		case 0:		// neither xintercept nor yintercept match!
			count = 100;	// Stop traversing, because somebody screwed up.
			break;

		case 1:		// xintercept matches
			xintercept += xstep;
			mapy += mapystep;
			break;

		case 2:		// yintercept matches
			yintercept += ystep;
			mapx += mapxstep;
			break;

		case 3:		// xintercept and yintercept both match
			// The trace is exiting a block through its corner. Not only does the block
			// being entered need to be checked (which will happen when this loop
			// continues), but the other two blocks adjacent to the corner also need to
			// be checked.
			if (flags & PT_ADDLINES)
			{
				if (!P_BlockLinesIterator (mapx + mapxstep, mapy, PIT_AddLineIntercepts) ||
					!P_BlockLinesIterator (mapx, mapy + mapystep, PIT_AddLineIntercepts))
					return false;	// early out
			}
			
			if (flags & PT_ADDTHINGS)
			{
				if (!P_BlockThingsIterator (mapx + mapxstep, mapy, PIT_AddThingIntercepts, pathbt) ||
					!P_BlockThingsIterator (mapx, mapy + mapystep, PIT_AddThingIntercepts, pathbt))
					return false;	// early out
			}
			xintercept += xstep;
			yintercept += ystep;
			mapx += mapxstep;
			mapy += mapystep;
			break;
		}
	}
	// go through the sorted list
	return P_TraverseIntercepts ( trav, FRACUNIT );
}

//===========================================================================
//
// P_RoughMonsterSearch
//
// Searches though the surrounding mapblocks for monsters/players
//		distance is in MAPBLOCKUNITS
//===========================================================================

AActor *P_RoughMonsterSearch (AActor *mo, int distance)
{
	return P_BlockmapSearch (mo, distance, RoughBlockCheck);
}

AActor *P_BlockmapSearch (AActor *mo, int distance, AActor *(*check)(AActor*, int))
{
	int blockX;
	int blockY;
	int startX, startY;
	int blockIndex;
	int firstStop;
	int secondStop;
	int thirdStop;
	int finalStop;
	int count;
	AActor *target;

	startX = (mo->x-bmaporgx)>>MAPBLOCKSHIFT;
	startY = (mo->y-bmaporgy)>>MAPBLOCKSHIFT;
	validcount++;
	
	if (startX >= 0 && startX < bmapwidth && startY >= 0 && startY < bmapheight)
	{
		if ( (target = check (mo, startY*bmapwidth+startX)) )
		{ // found a target right away
			return target;
		}
	}
	for (count = 1; count <= distance; count++)
	{
		blockX = clamp (startX-count, 0, bmapwidth-1);
		blockY = clamp (startY-count, 0, bmapheight-1);

		blockIndex = blockY*bmapwidth+blockX;
		firstStop = startX+count;
		if (firstStop < 0)
		{
			continue;
		}
		if (firstStop >= bmapwidth)
		{
			firstStop = bmapwidth-1;
		}
		secondStop = startY+count;
		if (secondStop < 0)
		{
			continue;
		}
		if (secondStop >= bmapheight)
		{
			secondStop = bmapheight-1;
		}
		thirdStop = secondStop*bmapwidth+blockX;
		secondStop = secondStop*bmapwidth+firstStop;
		firstStop += blockY*bmapwidth;
		finalStop = blockIndex;		

		// Trace the first block section (along the top)
		for (; blockIndex <= firstStop; blockIndex++)
		{
			if ( (target = check (mo, blockIndex)) )
			{
				return target;
			}
		}
		// Trace the second block section (right edge)
		for (blockIndex--; blockIndex <= secondStop; blockIndex += bmapwidth)
		{
			if ( (target = check (mo, blockIndex)) )
			{
				return target;
			}
		}		
		// Trace the third block section (bottom edge)
		for (blockIndex -= bmapwidth; blockIndex >= thirdStop; blockIndex--)
		{
			if ( (target = check (mo, blockIndex)) )
			{
				return target;
			}
		}
		// Trace the final block section (left edge)
		for (blockIndex++; blockIndex > finalStop; blockIndex -= bmapwidth)
		{
			if ( (target = check (mo, blockIndex)) )
			{
				return target;
			}
		}
	}
	return NULL;	
}

//===========================================================================
//
// RoughBlockCheck
//
//===========================================================================

static AActor *RoughBlockCheck (AActor *mo, int index)
{
	FBlockNode *link;

	for (link = blocklinks[index]; link != NULL; link = link->NextActor)
	{
		if (link->Me != mo)
		{
			if (mo->IsOkayToAttack (link->Me))
			{
				return link->Me;
			}
		}
	}
	return NULL;
}
