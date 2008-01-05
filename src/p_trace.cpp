/*
** p_trace.cpp
** Generalized trace function, like most 3D games have
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#include "p_trace.h"
#include "p_local.h"
#include "i_system.h"

static fixed_t StartZ;
static fixed_t Vx, Vy, Vz;
static DWORD ActorMask, WallMask;
static AActor *IgnoreThis;
static FTraceResults *Results;
static sector_t *CurSector;
static fixed_t MaxDist;
static fixed_t EnterDist;
static bool (*TraceCallback)(FTraceResults &res);
static DWORD TraceFlags;

// These are required for 3D-floor checking
// to create a fake sector with a floor 
// or ceiling plane coming from a 3D-floor
static sector_t DummySector[2];	
static int sectorsel;		

static bool PTR_TraceIterator (intercept_t *);
static bool CheckSectorPlane (const sector_t *sector, bool checkFloor, divline_t & trace);
static bool EditTraceResult (DWORD flags, FTraceResults &res);

bool Trace (fixed_t x, fixed_t y, fixed_t z, sector_t *sector,
			fixed_t vx, fixed_t vy, fixed_t vz, fixed_t maxDist,
			DWORD actorMask, DWORD wallMask, AActor *ignore,
			FTraceResults &res,
			DWORD flags, bool (*callback)(FTraceResults &res))
{
	// Under extreme circumstances ADecal::DoTrace can call this from inside a linespecial call!
	static bool recursion;
	if (recursion) return false;
	recursion=true;

	int ptflags;

	ptflags = actorMask ? PT_ADDLINES|PT_ADDTHINGS : PT_ADDLINES;

	StartZ = z;
	Vx = vx;
	Vy = vy;
	Vz = vz;
	ActorMask = actorMask;
	WallMask = wallMask;
	IgnoreThis = ignore;
	CurSector = sector;
	MaxDist = maxDist;
	EnterDist = 0;
	TraceCallback = callback;
	TraceFlags = flags;
	res.CrossedWater = NULL;
	Results = &res;

	res.HitType = TRACE_HitNone;

	// Do a 3D floor check in the starting sector
	res.ffloor=NULL;
	sectorsel=0;

	if (sector->e->ffloors.Size())
	{
		memcpy(&DummySector[0],sector,sizeof(sector_t));
		CurSector=sector=&DummySector[0];
		sectorsel=1;
		fixed_t bf = sector->floorplane.ZatPoint (x, y);
		fixed_t bc = sector->ceilingplane.ZatPoint (x, y);

		for(unsigned int i=0;i<sector->e->ffloors.Size();i++)
		{
			F3DFloor * rover=sector->e->ffloors[i];

			if (rover->flags&FF_SOLID && rover->flags&FF_EXISTS)
			{
				fixed_t ff_bottom=rover->bottom.plane->ZatPoint(x, y);
				fixed_t ff_top=rover->top.plane->ZatPoint(x, y);
				// clip to the part of the sector we are in
				if (z>ff_top)
				{
					// above
					if (bf<ff_top)
					{
						sector->floorplane=*rover->top.plane;
						sector->floorpic=*rover->top.texture;
						bf=ff_top;
					}
				}
				else if (z<ff_bottom)
				{
					//below
					if (bc>ff_bottom)
					{
						sector->ceilingplane=*rover->bottom.plane;
						sector->ceilingpic=*rover->bottom.texture;
						bc=ff_bottom;
					}
				}
			}
		}
	}

	// check for overflows and clip if necessary
	SQWORD xd= (SQWORD)x + ( ( SQWORD(vx) * SQWORD(maxDist) )>>16);

	if (xd>SQWORD(32767)*FRACUNIT)
	{
		maxDist=FixedDiv(FIXED_MAX-x,vx);
	}
	else if (xd<-SQWORD(32767)*FRACUNIT)
	{
		maxDist=FixedDiv(FIXED_MIN-x,vx);
	}


	SQWORD yd= (SQWORD)y + ( ( SQWORD(vy) * SQWORD(maxDist) )>>16);

	if (yd>SQWORD(32767)*FRACUNIT)
	{
		maxDist=FixedDiv(FIXED_MAX-y,vy);
	}
	else if (yd<-SQWORD(32767)*FRACUNIT)
	{
		maxDist=FixedDiv(FIXED_MIN-y,vy);
	}

	// recalculate the trace's end points for robustness
	if (P_PathTraverse (x, y, x + FixedMul (vx, maxDist), y + FixedMul (vy, maxDist),
		ptflags, PTR_TraceIterator))
	{ // check for intersection with floor/ceiling
		res.Sector = CurSector;
		divline_t trace;

		trace.x=x;
		trace.y=y;

		if (CheckSectorPlane (CurSector, true, trace))
		{
			res.HitType = TRACE_HitFloor;
			if (res.CrossedWater == NULL &&
				CurSector->heightsec != NULL &&
				CurSector->heightsec->floorplane.ZatPoint (res.X, res.Y) >= res.Z)
			{
				res.CrossedWater = CurSector;
			}
		}
		else if (CheckSectorPlane (CurSector, false, trace))
		{
			res.HitType = TRACE_HitCeiling;
		}
	}

	recursion=false;
	if (res.HitType != TRACE_HitNone)
	{
		if (flags)
		{
			return EditTraceResult (flags, res);
		}
		else
		{
			return true;
		}
	}
	else
	{
		res.HitType = TRACE_HitNone;
		res.X = x + FixedMul (vx, maxDist);
		res.Y = y + FixedMul (vy, maxDist);
		res.Z = z + FixedMul (vz, maxDist);
		res.Distance = maxDist;
		res.Fraction = FRACUNIT;
		return false;
	}
}

static bool PTR_TraceIterator (intercept_t *in)
{
	fixed_t hitx, hity, hitz;
	fixed_t dist;

	if (in->isaline)
	{
		int lineside;
		sector_t *entersector;

		dist = FixedMul (MaxDist, in->frac);
		hitx = trace.x + FixedMul (Vx, dist);
		hity = trace.y + FixedMul (Vy, dist);
		hitz = StartZ + FixedMul (Vz, dist);

		fixed_t ff, fc, bf = 0, bc = 0;

		// CurSector may be a copy so we must compare the sector number, not the index!
		if (in->d.line->frontsector->sectornum == CurSector->sectornum)
		{
			lineside = 0;
		}
		else if (in->d.line->backsector && in->d.line->backsector->sectornum == CurSector->sectornum)
		{
			lineside = 1;
		}
		else
		{ // Dammit. Why does Doom have to allow non-closed sectors?
			if (in->d.line->backsector == NULL)
			{
				lineside = 0;
				CurSector = in->d.line->frontsector;
			}
			else
			{
				lineside = P_PointOnLineSide (trace.x, trace.y, in->d.line);
				CurSector = lineside ? in->d.line->backsector : in->d.line->frontsector;
			}
		}

		if (!(in->d.line->flags & ML_TWOSIDED))
		{
			entersector = NULL;
		}
		else
		{
			entersector = (lineside == 0) ? in->d.line->backsector : in->d.line->frontsector;
			
			// For backwards compatibility: Ignore lines with the same sector on both sides.
			// This is the way Doom.exe did it and some WADs (e.g. Alien Vendetta MAP15 need it.
			if (i_compatflags & COMPATF_TRACE && in->d.line->backsector == in->d.line->frontsector)
			{
				return true;
			}
		}

		ff = CurSector->floorplane.ZatPoint (hitx, hity);
		fc = CurSector->ceilingplane.ZatPoint (hitx, hity);

		if (entersector != NULL)
		{
			bf = entersector->floorplane.ZatPoint (hitx, hity);
			bc = entersector->ceilingplane.ZatPoint (hitx, hity);
		}

		if (Results->CrossedWater == NULL &&
			CurSector->heightsec &&
			!(CurSector->MoreFlags & SECF_IGNOREHEIGHTSEC) &&
			//CurSector->heightsec->waterzone &&
			hitz <= CurSector->heightsec->floorplane.ZatPoint (hitx, hity))
		{
			// hit crossed a water plane
			Results->CrossedWater = CurSector;
		}

		if (hitz <= ff)
		{	// hit floor in front of wall
			Results->HitType = TRACE_HitFloor;
		}
		else if (hitz >= fc)
		{ 	// hit ceiling in front of wall
			Results->HitType = TRACE_HitCeiling;
		}
		else if (entersector == NULL ||
			hitz <= bf || hitz >= bc ||
			in->d.line->flags & WallMask)
		{	// hit the wall
			
			Results->HitType = TRACE_HitWall;
			Results->Tier =
				entersector == NULL ? TIER_Middle :
				hitz <= bf ? TIER_Lower :
				hitz >= bc ? TIER_Upper : TIER_Middle;
			if (TraceFlags & TRACE_Impact)
			{
				P_ActivateLine (in->d.line, IgnoreThis, lineside, SPAC_IMPACT);
			}
		}
		else
		{ 	// made it past the wall
			// check for 3D floors first
			if (entersector->e->ffloors.Size())
			{
				memcpy(&DummySector[sectorsel],entersector,sizeof(sector_t));
				entersector=&DummySector[sectorsel];
				sectorsel^=1;

				for(unsigned int i=0;i<entersector->e->ffloors.Size();i++)
				{
					F3DFloor * rover=entersector->e->ffloors[i];

					if (rover->flags&FF_SOLID && rover->flags&FF_EXISTS)
					{
						fixed_t ff_bottom=rover->bottom.plane->ZatPoint(hitx, hity);
						fixed_t ff_top=rover->top.plane->ZatPoint(hitx, hity);

						// clip to the part of the sector we are in
						if (hitz>ff_top)
						{
							// above
							if (bf<ff_top)
							{
								entersector->floorplane=*rover->top.plane;
								entersector->floorpic=*rover->top.texture;
								bf=ff_top;
							}
						}
						else if (hitz<ff_bottom)
						{
							//below
							if (bc>ff_bottom)
							{
								entersector->ceilingplane=*rover->bottom.plane;
								entersector->ceilingpic=*rover->bottom.texture;
								bc=ff_bottom;
							}
						}
						else
						{
							//hit the edge - equivalent to hitting the wall
							Results->HitType = TRACE_HitWall;
							Results->Tier = TIER_FFloor;
							Results->ffloor = rover;
							if ((TraceFlags & TRACE_Impact) && in->d.line->special)
							{
								P_ActivateLine (in->d.line, IgnoreThis, lineside, SPAC_IMPACT);
							}
							goto cont;
						}
					}
				}
			}



			Results->HitType = TRACE_HitNone;
			if (TraceFlags & TRACE_PCross)
			{
				P_ActivateLine (in->d.line, IgnoreThis, lineside, SPAC_PCROSS);
			}
			if (TraceFlags & TRACE_Impact)
			{ // This is incorrect for "impact", but Hexen did this, so
			  // we need to as well, for compatibility
				P_ActivateLine (in->d.line, IgnoreThis, lineside, SPAC_IMPACT);
			}
		}
cont:

		if (Results->HitType != TRACE_HitNone)
		{
			// We hit something, so figure out where exactly
			Results->Sector = CurSector;

			if (Results->HitType != TRACE_HitWall &&
				!CheckSectorPlane (CurSector, Results->HitType == TRACE_HitFloor, trace))
			{ // trace is parallel to the plane (or right on it)
				if (entersector == NULL)
				{
					Results->HitType = TRACE_HitWall;
					Results->Tier = TIER_Middle;
				}
				else
				{
					if (hitz <= bf || hitz >= bc)
					{
						Results->HitType = TRACE_HitWall;
						Results->Tier =
							hitz <= bf ? TIER_Lower :
							hitz >= bc ? TIER_Upper : TIER_Middle;
					}
					else
					{
						Results->HitType = TRACE_HitNone;
					}
				}
				if (Results->HitType == TRACE_HitWall && TraceFlags & TRACE_Impact)
				{
					P_ActivateLine(in->d.line, IgnoreThis, lineside, SPAC_IMPACT);
				}
			}

			if (Results->HitType == TRACE_HitWall)
			{
				Results->X = hitx;
				Results->Y = hity;
				Results->Z = hitz;
				Results->Distance = dist;
				Results->Fraction = in->frac;
				Results->Line = in->d.line;
				Results->Side = lineside;
			}
		}

		if (Results->HitType == TRACE_HitNone)
		{
			CurSector = entersector;
			EnterDist = dist;
			return true;
		}

		if (TraceCallback != NULL)
		{
			return TraceCallback (*Results);
		}
		else
		{
			return false;
		}
	}

	// Encountered an actor
	if (!(in->d.thing->flags & ActorMask) ||
		in->d.thing == IgnoreThis)
	{
		return true;
	}

	dist = FixedMul (MaxDist, in->frac);
	hitx = trace.x + FixedMul (Vx, dist);
	hity = trace.y + FixedMul (Vy, dist);
	hitz = StartZ + FixedMul (Vz, dist);

	if (hitz > in->d.thing->z + in->d.thing->height)
	{ // trace enters above actor
		if (Vz >= 0) return true;      // Going up: can't hit
		
		// Does it hit the top of the actor?
		dist = FixedDiv(in->d.thing->z + in->d.thing->height - StartZ, Vz);

		if (dist > MaxDist) return true;
		in->frac = FixedDiv(dist, MaxDist);

		hitx = trace.x + FixedMul (Vx, dist);
		hity = trace.y + FixedMul (Vy, dist);
		hitz = StartZ + FixedMul (Vz, dist);

		// calculated coordinate is outside the actor's bounding box
		if (abs(hitx - in->d.thing->x) > in->d.thing->radius ||
			abs(hity - in->d.thing->y) > in->d.thing->radius) return true;
	}
	else if (hitz < in->d.thing->z)
	{ // trace enters below actor
		if (Vz <= 0) return true;      // Going down: can't hit
		
		// Does it hit the bottom of the actor?
		dist = FixedDiv(in->d.thing->z - StartZ, Vz);
		if (dist > MaxDist) return true;
		in->frac = FixedDiv(dist, MaxDist);

		hitx = trace.x + FixedMul (Vx, dist);
		hity = trace.y + FixedMul (Vy, dist);
		hitz = StartZ + FixedMul (Vz, dist);

		// calculated coordinate is outside the actor's bounding box
		if (abs(hitx - in->d.thing->x) > in->d.thing->radius ||
			abs(hity - in->d.thing->y) > in->d.thing->radius) return true;
	}

	// check for fake floors first
	if (CurSector->e->ffloors.Size())
	{
		fixed_t ff_floor=CurSector->floorplane.ZatPoint(hitx, hity);
		fixed_t ff_ceiling=CurSector->ceilingplane.ZatPoint(hitx, hity);

		if (hitz>ff_ceiling)	// actor is hit above the current ceiling
		{
			Results->HitType=TRACE_HitCeiling;
		}
		else if (hitz<ff_floor)	// actor is hit below the current floor
		{
			Results->HitType=TRACE_HitFloor;
		}
		else goto cont1;

		// the trace hit a 3D-floor before the thing.
		// Calculate an intersection and abort.
		Results->Sector = CurSector;
		if (!CheckSectorPlane(CurSector, Results->HitType==TRACE_HitFloor, trace))
		{
			Results->HitType=TRACE_HitNone;
		}
		if (TraceCallback != NULL)
		{
			return TraceCallback (*Results);
		}
		else
		{
			return false;
		}
	}


cont1:

	Results->HitType = TRACE_HitActor;
	Results->X = hitx;
	Results->Y = hity;
	Results->Z = hitz;
	Results->Distance = dist;
	Results->Fraction = in->frac;
	Results->Actor = in->d.thing;

	if (TraceCallback != NULL)
	{
		return TraceCallback (*Results);
	}
	else
	{
		return false;
	}
}

static bool CheckSectorPlane (const sector_t *sector, bool checkFloor, divline_t & trace)
{
	secplane_t plane;

	if (checkFloor)
	{
		plane = sector->floorplane;
	}
	else
	{
		plane = sector->ceilingplane;
	}

	fixed_t den = TMulScale16 (plane.a, Vx, plane.b, Vy, plane.c, Vz);

	if (den != 0)
	{
		fixed_t num = TMulScale16 (plane.a, trace.x,
								   plane.b, trace.y,
								   plane.c, StartZ) + plane.d;

		fixed_t hitdist = FixedDiv (-num, den);

		if (hitdist > EnterDist && hitdist < MaxDist)
		{
			Results->X = trace.x + FixedMul (Vx, hitdist);
			Results->Y = trace.y + FixedMul (Vy, hitdist);
			Results->Z = StartZ + FixedMul (Vz, hitdist);
			Results->Distance = hitdist;
			Results->Fraction = FixedDiv (hitdist, MaxDist);
			return true;
		}
	}
	return false;
}

static bool EditTraceResult (DWORD flags, FTraceResults &res)
{
	if (flags & TRACE_NoSky)
	{ // Throw away sky hits
		if (res.HitType == TRACE_HitFloor)
		{
			if (res.Sector->floorpic == skyflatnum)
			{
				res.HitType = TRACE_HitNone;
				return false;
			}
		}
		else if (res.HitType == TRACE_HitCeiling)
		{
			if (res.Sector->ceilingpic == skyflatnum)
			{
				res.HitType = TRACE_HitNone;
				return false;
			}
		}
		else if (res.HitType == TRACE_HitWall)
		{
			if (res.Tier == TIER_Upper &&
				res.Line->frontsector->ceilingpic == skyflatnum &&
				res.Line->backsector->ceilingpic == skyflatnum)
			{
				res.HitType = TRACE_HitNone;
				return false;
			}
		}
	}
	return true;
}
