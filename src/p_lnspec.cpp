/*
** p_lnspec.cpp
** Handles line specials
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
** Each function returns true if it caused something to happen
** or false if it could not perform the desired action.
*/

#include "doomstat.h"
#include "p_local.h"
#include "p_lnspec.h"
#include "p_enemy.h"
#include "g_level.h"
#include "v_palette.h"
#include "tables.h"
#include "i_system.h"
#include "a_sharedglobal.h"
#include "a_lightning.h"
#include "statnums.h"
#include "s_sound.h"
#include "templates.h"
#include "a_keys.h"
#include "gi.h"
#include "m_random.h"
#include "p_conversation.h"
#include "a_strifeglobal.h"
// [BC] New #includes.
#include "cooperative.h"
#include "deathmatch.h"
#include "team.h"
#include "announcer.h"
#include "sv_commands.h"
#include "network.h"
#include "invasion.h"
#include "cl_demo.h"

#define FUNC(a) static int a (line_t *ln, AActor *it, bool backSide, \
	int arg0, int arg1, int arg2, int arg3, int arg4)

#define SPEED(a)		((a)*(FRACUNIT/8))
#define TICS(a)			(((a)*TICRATE)/35)
#define OCTICS(a)		(((a)*TICRATE)/8)
#define	BYTEANGLE(a)	((angle_t)((a)<<24))

static FRandom pr_glass ("GlassBreak");


FUNC(LS_NOP)
{
	return false;
}

FUNC(LS_Polyobj_RotateLeft)
// Polyobj_RotateLeft (po, speed, angle)
{
	return EV_RotatePoly (ln, arg0, arg1, arg2, 1, false);
}

FUNC(LS_Polyobj_RotateRight)
// Polyobj_rotateRight (po, speed, angle)
{
	return EV_RotatePoly (ln, arg0, arg1, arg2, -1, false);
}

FUNC(LS_Polyobj_Move)
// Polyobj_Move (po, speed, angle, distance)
{
	return EV_MovePoly (ln, arg0, SPEED(arg1), BYTEANGLE(arg2), arg3 * FRACUNIT, false);
}

FUNC(LS_Polyobj_MoveTimes8)
// Polyobj_MoveTimes8 (po, speed, angle, distance)
{
	return EV_MovePoly (ln, arg0, SPEED(arg1), BYTEANGLE(arg2), arg3 * FRACUNIT * 8, false);
}

FUNC(LS_Polyobj_DoorSwing)
// Polyobj_DoorSwing (po, speed, angle, delay)
{
	return EV_OpenPolyDoor (ln, arg0, arg1, BYTEANGLE(arg2), arg3, 0, PODOOR_SWING);
}

FUNC(LS_Polyobj_DoorSlide)
// Polyobj_DoorSlide (po, speed, angle, distance, delay)
{
	return EV_OpenPolyDoor (ln, arg0, SPEED(arg1), BYTEANGLE(arg2), arg4, arg3*FRACUNIT, PODOOR_SLIDE);
}

FUNC(LS_Polyobj_OR_RotateLeft)
// Polyobj_OR_RotateLeft (po, speed, angle)
{
	return EV_RotatePoly (ln, arg0, arg1, arg2, 1, true);
}

FUNC(LS_Polyobj_OR_RotateRight)
// Polyobj_OR_RotateRight (po, speed, angle)
{
	return EV_RotatePoly (ln, arg0, arg1, arg2, -1, true);
}

FUNC(LS_Polyobj_OR_Move)
// Polyobj_OR_Move (po, speed, angle, distance)
{
	return EV_MovePoly (ln, arg0, SPEED(arg1), BYTEANGLE(arg2), arg3 * FRACUNIT, true);
}

FUNC(LS_Polyobj_OR_MoveTimes8)
// Polyobj_OR_MoveTimes8 (po, speed, angle, distance)
{
	return EV_MovePoly (ln, arg0, SPEED(arg1), BYTEANGLE(arg2), arg3 * FRACUNIT * 8, true);
}

FUNC(LS_Door_Close)
// Door_Close (tag, speed, lighttag)
{
	return EV_DoDoor (DDoor::doorClose, ln, it, arg0, SPEED(arg1), 0, 0, arg2);
}

FUNC(LS_Door_Open)
// Door_Open (tag, speed, lighttag)
{
	return EV_DoDoor (DDoor::doorOpen, ln, it, arg0, SPEED(arg1), 0, 0, arg2);
}

FUNC(LS_Door_Raise)
// Door_Raise (tag, speed, delay, lighttag)
{
	return EV_DoDoor (DDoor::doorRaise, ln, it, arg0, SPEED(arg1), TICS(arg2), 0, arg3);
}

FUNC(LS_Door_LockedRaise)
// Door_LockedRaise (tag, speed, delay, lock, lighttag)
{
	return EV_DoDoor (arg2 ? DDoor::doorRaise : DDoor::doorOpen, ln, it,
					  arg0, SPEED(arg1), TICS(arg2), arg3, arg4);
}

FUNC(LS_Door_CloseWaitOpen)
// Door_CloseWaitOpen (tag, speed, delay, lighttag)
{
	return EV_DoDoor (DDoor::doorCloseWaitOpen, ln, it, arg0, SPEED(arg1), OCTICS(arg2), 0, arg3);
}

FUNC(LS_Door_Animated)
// Door_Animated (tag, speed, delay)
{
	return EV_SlidingDoor (ln, it, arg0, arg1, arg2);
}

FUNC(LS_Generic_Door)
// Generic_Door (tag, speed, kind, delay, lock)
{
	int tag, lightTag;
	DDoor::EVlDoor type;

	switch (arg2 & 127)
	{
		case 0: type = DDoor::doorRaise;			break;
		case 1: type = DDoor::doorOpen;				break;
		case 2: type = DDoor::doorCloseWaitOpen;	break;
		case 3: type = DDoor::doorClose;			break;
		default: return false;
	}
	if (arg2 & 128)
	{
		// New for 2.0.58: Finally support BOOM's local door light effect
		tag = 0;
		lightTag = arg0;
	}
	else
	{
		tag = arg0;
		lightTag = 0;
	}
	return EV_DoDoor (type, ln, it, tag, SPEED(arg1), OCTICS(arg3), arg4, lightTag);
}

FUNC(LS_Floor_LowerByValue)
// Floor_LowerByValue (tag, speed, height)
{
	return EV_DoFloor (DFloor::floorLowerByValue, ln, arg0, SPEED(arg1), FRACUNIT*arg2, 0, 0);
}

FUNC(LS_Floor_LowerToLowest)
// Floor_LowerToLowest (tag, speed)
{
	return EV_DoFloor (DFloor::floorLowerToLowest, ln, arg0, SPEED(arg1), 0, 0, 0);
}

FUNC(LS_Floor_LowerToHighest)
// Floor_LowerToHighest (tag, speed, adjust)
{
	return EV_DoFloor (DFloor::floorLowerToHighest, ln, arg0, SPEED(arg1), (arg2-128)*FRACUNIT, 0, 0);
}

FUNC(LS_Floor_LowerToNearest)
// Floor_LowerToNearest (tag, speed)
{
	return EV_DoFloor (DFloor::floorLowerToNearest, ln, arg0, SPEED(arg1), 0, 0, 0);
}

FUNC(LS_Floor_RaiseByValue)
// Floor_RaiseByValue (tag, speed, height)
{
	return EV_DoFloor (DFloor::floorRaiseByValue, ln, arg0, SPEED(arg1), FRACUNIT*arg2, 0, 0);
}

FUNC(LS_Floor_RaiseToHighest)
// Floor_RaiseToHighest (tag, speed)
{
	return EV_DoFloor (DFloor::floorRaiseToHighest, ln, arg0, SPEED(arg1), 0, 0, 0);
}

FUNC(LS_Floor_RaiseToNearest)
// Floor_RaiseToNearest (tag, speed)
{
	return EV_DoFloor (DFloor::floorRaiseToNearest, ln, arg0, SPEED(arg1), 0, 0, 0);
}

FUNC(LS_Floor_RaiseAndCrush)
// Floor_RaiseAndCrush (tag, speed, crush)
{
	return EV_DoFloor (DFloor::floorRaiseAndCrush, ln, arg0, SPEED(arg1), 0, arg2, 0);
}

FUNC(LS_Floor_RaiseByValueTimes8)
// FLoor_RaiseByValueTimes8 (tag, speed, height)
{
	return EV_DoFloor (DFloor::floorRaiseByValue, ln, arg0, SPEED(arg1), FRACUNIT*arg2*8, 0, 0);
}

FUNC(LS_Floor_LowerByValueTimes8)
// Floor_LowerByValueTimes8 (tag, speed, height)
{
	return EV_DoFloor (DFloor::floorLowerByValue, ln, arg0, SPEED(arg1), FRACUNIT*arg2*8, 0, 0);
}

FUNC(LS_Floor_CrushStop)
// Floor_CrushStop (tag)
{
	return EV_FloorCrushStop (arg0);
}

FUNC(LS_Floor_LowerInstant)
// Floor_LowerInstant (tag, unused, height)
{
	return EV_DoFloor (DFloor::floorLowerInstant, ln, arg0, 0, arg2*FRACUNIT*8, 0, 0);
}

FUNC(LS_Floor_RaiseInstant)
// Floor_RaiseInstant (tag, unused, height)
{
	return EV_DoFloor (DFloor::floorRaiseInstant, ln, arg0, 0, arg2*FRACUNIT*8, 0, 0);
}

FUNC(LS_Floor_MoveToValueTimes8)
// Floor_MoveToValueTimes8 (tag, speed, height, negative)
{
	return EV_DoFloor (DFloor::floorMoveToValue, ln, arg0, SPEED(arg1),
					   arg2*FRACUNIT*8*(arg3?-1:1), 0, 0);
}

FUNC(LS_Floor_RaiseToLowestCeiling)
// Floor_RaiseToLowestCeiling (tag, speed)
{
	return EV_DoFloor (DFloor::floorRaiseToLowestCeiling, ln, arg0, SPEED(arg1), 0, 0, 0);
}

FUNC(LS_Floor_RaiseByTexture)
// Floor_RaiseByTexture (tag, speed)
{
	return EV_DoFloor (DFloor::floorRaiseByTexture, ln, arg0, SPEED(arg1), 0, 0, 0);
}

FUNC(LS_Floor_RaiseByValueTxTy)
// Floor_RaiseByValueTxTy (tag, speed, height)
{
	return EV_DoFloor (DFloor::floorRaiseAndChange, ln, arg0, SPEED(arg1), arg2*FRACUNIT, 0, 0);
}

FUNC(LS_Floor_LowerToLowestTxTy)
// Floor_LowerToLowestTxTy (tag, speed)
{
	return EV_DoFloor (DFloor::floorLowerAndChange, ln, arg0, SPEED(arg1), arg2*FRACUNIT, 0, 0);
}

FUNC(LS_Floor_Waggle)
// Floor_Waggle (tag, amplitude, frequency, delay, time)
{
	return EV_StartWaggle (arg0, arg1, arg2, arg3, arg4, false);
}

FUNC(LS_Ceiling_Waggle)
// Ceiling_Waggle (tag, amplitude, frequency, delay, time)
{
	return EV_StartWaggle (arg0, arg1, arg2, arg3, arg4, true);
}

FUNC(LS_Floor_TransferTrigger)
// Floor_TransferTrigger (tag)
{
	return EV_DoChange (ln, trigChangeOnly, arg0);
}

FUNC(LS_Floor_TransferNumeric)
// Floor_TransferNumeric (tag)
{
	return EV_DoChange (ln, numChangeOnly, arg0);
}

FUNC(LS_Floor_Donut)
// Floor_Donut (pillartag, pillarspeed, slimespeed)
{
	return EV_DoDonut (arg0, SPEED(arg1), SPEED(arg2));
}

FUNC(LS_Generic_Floor)
// Generic_Floor (tag, speed, height, target, change/model/direct/crush)
{
	DFloor::EFloor type;

	if (arg4 & 8)
	{
		switch (arg3)
		{
			case 1: type = DFloor::floorRaiseToHighest;			break;
			case 2: type = DFloor::floorRaiseToLowest;			break;
			case 3: type = DFloor::floorRaiseToNearest;			break;
			case 4: type = DFloor::floorRaiseToLowestCeiling;	break;
			case 5: type = DFloor::floorRaiseToCeiling;			break;
			case 6: type = DFloor::floorRaiseByTexture;			break;
			default:type = DFloor::floorRaiseByValue;			break;
		}
	}
	else
	{
		switch (arg3)
		{
			case 1: type = DFloor::floorLowerToHighest;			break;
			case 2: type = DFloor::floorLowerToLowest;			break;
			case 3: type = DFloor::floorLowerToNearest;			break;
			case 4: type = DFloor::floorLowerToLowestCeiling;	break;
			case 5: type = DFloor::floorLowerToCeiling;			break;
			case 6: type = DFloor::floorLowerByTexture;			break;
			default:type = DFloor::floorLowerByValue;			break;
		}
	}

	return EV_DoFloor (type, ln, arg0, SPEED(arg1), arg2*FRACUNIT,
					   (arg4 & 16) ? 20 : -1, arg4 & 7);
					   
}

FUNC(LS_Stairs_BuildDown)
// Stair_BuildDown (tag, speed, height, delay, reset)
{
	return EV_BuildStairs (arg0, DFloor::buildDown, ln,
						   arg2 * FRACUNIT, SPEED(arg1), TICS(arg3), arg4, 0, 1);
}

FUNC(LS_Stairs_BuildUp)
// Stairs_BuildUp (tag, speed, height, delay, reset)
{
	return EV_BuildStairs (arg0, DFloor::buildUp, ln,
						   arg2 * FRACUNIT, SPEED(arg1), TICS(arg3), arg4, 0, 1);
}

FUNC(LS_Stairs_BuildDownSync)
// Stairs_BuildDownSync (tag, speed, height, reset)
{
	return EV_BuildStairs (arg0, DFloor::buildDown, ln,
						   arg2 * FRACUNIT, SPEED(arg1), 0, arg3, 0, 2);
}

FUNC(LS_Stairs_BuildUpSync)
// Stairs_BuildUpSync (tag, speed, height, reset)
{
	return EV_BuildStairs (arg0, DFloor::buildUp, ln,
						   arg2 * FRACUNIT, SPEED(arg1), 0, arg3, 0, 2);
}

FUNC(LS_Stairs_BuildUpDoom)
// Stairs_BuildUpDoom (tag, speed, height, delay, reset)
{
	return EV_BuildStairs (arg0, DFloor::buildUp, ln,
						   arg2 * FRACUNIT, SPEED(arg1), TICS(arg3), arg4, 0, 0);
}

FUNC(LS_Generic_Stairs)
// Generic_Stairs (tag, speed, step, dir/igntxt, reset)
{
	DFloor::EStair type = (arg3 & 1) ? DFloor::buildUp : DFloor::buildDown;
	bool res = EV_BuildStairs (arg0, type, ln,
							   arg2 * FRACUNIT, SPEED(arg1), 0, arg4, arg3 & 2, 0);

	if (res && ln && (ln->flags & ML_REPEAT_SPECIAL) && ln->special == Generic_Stairs)
		// Toggle direction of next activation of repeatable stairs
		ln->args[3] ^= 1;

	return res;
}

FUNC(LS_Pillar_Build)
// Pillar_Build (tag, speed, height)
{
	return EV_DoPillar (DPillar::pillarBuild, arg0, SPEED(arg1), arg2*FRACUNIT, 0, -1);
}

FUNC(LS_Pillar_BuildAndCrush)
// Pillar_BuildAndCrush (tag, speed, height, crush)
{
	return EV_DoPillar (DPillar::pillarBuild, arg0, SPEED(arg1), arg2*FRACUNIT, 0, arg3);
}

FUNC(LS_Pillar_Open)
// Pillar_Open (tag, speed, f_height, c_height)
{
	return EV_DoPillar (DPillar::pillarOpen, arg0, SPEED(arg1), arg2*FRACUNIT, arg3*FRACUNIT, -1);
}

FUNC(LS_Ceiling_LowerByValue)
// Ceiling_LowerByValue (tag, speed, height)
{
	return EV_DoCeiling (DCeiling::ceilLowerByValue, ln, arg0, SPEED(arg1), 0, arg2*FRACUNIT, -1, 0, 0);
}

FUNC(LS_Ceiling_RaiseByValue)
// Ceiling_RaiseByValue (tag, speed, height)
{
	return EV_DoCeiling (DCeiling::ceilRaiseByValue, ln, arg0, SPEED(arg1), 0, arg2*FRACUNIT, -1, 0, 0);
}

FUNC(LS_Ceiling_LowerByValueTimes8)
// Ceiling_LowerByValueTimes8 (tag, speed, height)
{
	return EV_DoCeiling (DCeiling::ceilLowerByValue, ln, arg0, SPEED(arg1), 0, arg2*FRACUNIT*8, -1, 0, 0);
}

FUNC(LS_Ceiling_RaiseByValueTimes8)
// Ceiling_RaiseByValueTimes8 (tag, speed, height)
{
	return EV_DoCeiling (DCeiling::ceilRaiseByValue, ln, arg0, SPEED(arg1), 0, arg2*FRACUNIT*8, -1, 0, 0);
}

FUNC(LS_Ceiling_CrushAndRaise)
// Ceiling_CrushAndRaise (tag, speed, crush)
{
	return EV_DoCeiling (DCeiling::ceilCrushAndRaise, ln, arg0, SPEED(arg1), SPEED(arg1)/2, 0, arg2, 0, 0);
}

FUNC(LS_Ceiling_LowerAndCrush)
// Ceiling_LowerAndCrush (tag, speed, crush)
{
	return EV_DoCeiling (DCeiling::ceilLowerAndCrush, ln, arg0, SPEED(arg1), SPEED(arg1), 0, arg2, 0, 0);
}

FUNC(LS_Ceiling_CrushStop)
// Ceiling_CrushStop (tag)
{
	return EV_CeilingCrushStop (arg0);
}

FUNC(LS_Ceiling_CrushRaiseAndStay)
// Ceiling_CrushRaiseAndStay (tag, speed, crush)
{
	return EV_DoCeiling (DCeiling::ceilCrushRaiseAndStay, ln, arg0, SPEED(arg1), SPEED(arg1)/2, 0, arg2, 0, 0);
}

FUNC(LS_Ceiling_MoveToValueTimes8)
// Ceiling_MoveToValueTimes8 (tag, speed, height, negative)
{
	return EV_DoCeiling (DCeiling::ceilMoveToValue, ln, arg0, SPEED(arg1), 0,
						 arg2*FRACUNIT*8*((arg3) ? -1 : 1), -1, 0, 0);
}

FUNC(LS_Ceiling_LowerToHighestFloor)
// Ceiling_LowerToHighestFloor (tag, speed)
{
	return EV_DoCeiling (DCeiling::ceilLowerToHighestFloor, ln, arg0, SPEED(arg1), 0, 0, -1, 0, 0);
}

FUNC(LS_Ceiling_LowerInstant)
// Ceiling_LowerInstant (tag, unused, height)
{
	return EV_DoCeiling (DCeiling::ceilLowerInstant, ln, arg0, 0, 0, arg2*FRACUNIT*8, -1, 0, 0);
}

FUNC(LS_Ceiling_RaiseInstant)
// Ceiling_RaiseInstant (tag, unused, height)
{
	return EV_DoCeiling (DCeiling::ceilRaiseInstant, ln, arg0, 0, 0, arg2*FRACUNIT*8, -1, 0, 0);
}

FUNC(LS_Ceiling_CrushRaiseAndStayA)
// Ceiling_CrushRaiseAndStayA (tag, dnspeed, upspeed, damage)
{
	return EV_DoCeiling (DCeiling::ceilCrushRaiseAndStay, ln, arg0, SPEED(arg1), SPEED(arg2), 0, arg3, 0, 0);
}

FUNC(LS_Ceiling_CrushRaiseAndStaySilA)
// Ceiling_CrushRaiseAndStaySilA (tag, dnspeed, upspeed, damage)
{
	return EV_DoCeiling (DCeiling::ceilCrushRaiseAndStay, ln, arg0, SPEED(arg1), SPEED(arg2), 0, arg3, 1, 0);
}

FUNC(LS_Ceiling_CrushAndRaiseA)
// Ceiling_CrushAndRaiseA (tag, dnspeed, upspeed, damage)
{
	return EV_DoCeiling (DCeiling::ceilCrushAndRaise, ln, arg0, SPEED(arg1), SPEED(arg2), 0, arg3, 0, 0);
}

FUNC(LS_Ceiling_CrushAndRaiseSilentA)
// Ceiling_CrushAndRaiseSilentA (tag, dnspeed, upspeed, damage)
{
	return EV_DoCeiling (DCeiling::ceilCrushAndRaise, ln, arg0, SPEED(arg1), SPEED(arg2), 0, arg3, 1, 0);
}

FUNC(LS_Ceiling_RaiseToNearest)
// Ceiling_RaiseToNearest (tag, speed)
{
	return EV_DoCeiling (DCeiling::ceilRaiseToNearest, ln, arg0, SPEED(arg1), 0, 0, -1, 0, 0);
}

FUNC(LS_Ceiling_LowerToLowest)
// Ceiling_LowerToLowest (tag, speed)
{
	return EV_DoCeiling (DCeiling::ceilLowerToLowest, ln, arg0, SPEED(arg1), 0, 0, -1, 0, 0);
}

FUNC(LS_Ceiling_LowerToFloor)
// Ceiling_LowerToFloor (tag, speed)
{
	return EV_DoCeiling (DCeiling::ceilLowerToFloor, ln, arg0, SPEED(arg1), 0, 0, -1, 0, 0);
}

FUNC(LS_Generic_Ceiling)
// Generic_Ceiling (tag, speed, height, target, change/model/direct/crush)
{
	DCeiling::ECeiling type;

	if (arg4 & 8) {
		switch (arg3) {
			case 1:  type = DCeiling::ceilRaiseToHighest;		break;
			case 2:  type = DCeiling::ceilRaiseToLowest;		break;
			case 3:  type = DCeiling::ceilRaiseToNearest;		break;
			case 4:  type = DCeiling::ceilRaiseToHighestFloor;	break;
			case 5:  type = DCeiling::ceilRaiseToFloor;			break;
			case 6:  type = DCeiling::ceilRaiseByTexture;		break;
			default: type = DCeiling::ceilRaiseByValue;			break;
		}
	} else {
		switch (arg3) {
			case 1:  type = DCeiling::ceilLowerToHighest;		break;
			case 2:  type = DCeiling::ceilLowerToLowest;		break;
			case 3:  type = DCeiling::ceilLowerToNearest;		break;
			case 4:  type = DCeiling::ceilLowerToHighestFloor;	break;
			case 5:  type = DCeiling::ceilLowerToFloor;			break;
			case 6:  type = DCeiling::ceilLowerByTexture;		break;
			default: type = DCeiling::ceilLowerByValue;			break;
		}
	}

	return EV_DoCeiling (type, ln, arg0, SPEED(arg1), SPEED(arg1), arg2*FRACUNIT,
						 (arg4 & 16) ? 20 : -1, 0, arg4 & 7);
	return false;
}

FUNC(LS_Generic_Crusher)
// Generic_Crusher (tag, dnspeed, upspeed, silent, damage)
{
	return EV_DoCeiling (DCeiling::ceilCrushAndRaise, ln, arg0, SPEED(arg1),
						 SPEED(arg2), 0, arg4, arg3 ? 2 : 0, 0);
}

FUNC(LS_Plat_PerpetualRaise)
// Plat_PerpetualRaise (tag, speed, delay)
{
	return EV_DoPlat (arg0, ln, DPlat::platPerpetualRaise, 0, SPEED(arg1), TICS(arg2), 8, 0);
}

FUNC(LS_Plat_PerpetualRaiseLip)
// Plat_PerpetualRaiseLip (tag, speed, delay, lip)
{
	return EV_DoPlat (arg0, ln, DPlat::platPerpetualRaise, 0, SPEED(arg1), TICS(arg2), arg3, 0);
}

FUNC(LS_Plat_Stop)
// Plat_Stop (tag)
{
	EV_StopPlat (arg0);
	return true;
}

FUNC(LS_Plat_DownWaitUpStay)
// Plat_DownWaitUpStay (tag, speed, delay)
{
	return EV_DoPlat (arg0, ln, DPlat::platDownWaitUpStay, 0, SPEED(arg1), TICS(arg2), 8, 0);
}

FUNC(LS_Plat_DownWaitUpStayLip)
// Plat_DownWaitUpStayLip (tag, speed, delay, lip, floor-sound?)
{
	return EV_DoPlat (arg0, ln,
		arg4 ? DPlat::platDownWaitUpStayStone : DPlat::platDownWaitUpStay,
		0, SPEED(arg1), TICS(arg2), arg3, 0);
}

FUNC(LS_Plat_DownByValue)
// Plat_DownByValue (tag, speed, delay, height)
{
	return EV_DoPlat (arg0, ln, DPlat::platDownByValue, FRACUNIT*arg3*8, SPEED(arg1), TICS(arg2), 0, 0);
}

FUNC(LS_Plat_UpByValue)
// Plat_UpByValue (tag, speed, delay, height)
{
	return EV_DoPlat (arg0, ln, DPlat::platUpByValue, FRACUNIT*arg3*8, SPEED(arg1), TICS(arg2), 0, 0);
}

FUNC(LS_Plat_UpWaitDownStay)
// Plat_UpWaitDownStay (tag, speed, delay)
{
	return EV_DoPlat (arg0, ln, DPlat::platUpWaitDownStay, 0, SPEED(arg1), TICS(arg2), 0, 0);
}

FUNC(LS_Plat_UpNearestWaitDownStay)
// Plat_UpNearestWaitDownStay (tag, speed, delay)
{
	return EV_DoPlat (arg0, ln, DPlat::platUpNearestWaitDownStay, 0, SPEED(arg1), TICS(arg2), 0, 0);
}

FUNC(LS_Plat_RaiseAndStayTx0)
// Plat_RaiseAndStayTx0 (tag, speed)
{
	return EV_DoPlat (arg0, ln, DPlat::platRaiseAndStay, 0, SPEED(arg1), 0, 0, 1);
}

FUNC(LS_Plat_UpByValueStayTx)
// Plat_UpByValueStayTx (tag, speed, height)
{
	return EV_DoPlat (arg0, ln, DPlat::platUpByValueStay, FRACUNIT*arg2*8, SPEED(arg1), 0, 0, 2);
}

FUNC(LS_Plat_ToggleCeiling)
// Plat_ToggleCeiling (tag)
{
	return EV_DoPlat (arg0, ln, DPlat::platToggle, 0, 0, 0, 0, 0);
}

FUNC(LS_Generic_Lift)
// Generic_Lift (tag, speed, delay, target, height)
{
	DPlat::EPlatType type;

	switch (arg3)
	{
		case 1:
			type = DPlat::platDownWaitUpStay;
			break;
		case 2:
			type = DPlat::platDownToNearestFloor;
			break;
		case 3:
			type = DPlat::platDownToLowestCeiling;
			break;
		case 4:
			type = DPlat::platPerpetualRaise;
			break;
		default:
			type = DPlat::platUpByValue;
			break;
	}

	return EV_DoPlat (arg0, ln, type, arg4*8*FRACUNIT, SPEED(arg1), OCTICS(arg2), 0, 0);
}

FUNC(LS_Exit_Normal)
// Exit_Normal (position)
{
	if (CheckIfExitIsGood (it))
	{
		G_ExitLevel (arg0, false);
		return true;
	}
	return false;
}

FUNC(LS_Exit_Secret)
// Exit_Secret (position)
{
	if (CheckIfExitIsGood (it))
	{
		G_SecretExitLevel (arg0);
		return true;
	}
	return false;
}

FUNC(LS_Teleport_NewMap)
// Teleport_NewMap (map, position, keepFacing?)
{
	if (backSide == 0 || gameinfo.gametype == GAME_Strife)
	{
		level_info_t *info = FindLevelByNum (arg0);

		if (info && CheckIfExitIsGood (it))
		{
			strncpy (level.nextmap, info->mapname, 8);
			G_ExitLevel (arg1, !!arg2);
			return true;
		}
	}
	return false;
}

FUNC(LS_Teleport)
// Teleport (tid, sectortag, bNoSourceFog)
{
	return EV_Teleport (arg0, arg1, ln, backSide, it, true, !arg2, false);
}

FUNC(LS_Teleport_NoFog)
// Teleport_NoFog (tid, useang, sectortag)
{
	return EV_Teleport (arg0, arg2, ln, backSide, it, false, false, !arg1);
}

FUNC(LS_Teleport_ZombieChanger)
// Teleport_ZombieChanger (tid, sectortag)
{
	// This is practically useless outside of Strife, but oh well.
	if (it != NULL)
	{
		EV_Teleport (arg0, arg1, ln, backSide, it, false, false, false);
		it->SetState (it->PainState);

		// [BC] If we're the server, tell clients to put this thing in its pain state.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetThingState( it, STATE_PAIN );

		return true;
	}
	return false;
}

FUNC(LS_TeleportOther)
// TeleportOther (other_tid, dest_tid, fog?)
{
	return EV_TeleportOther (arg0, arg1, arg2?true:false);
}

FUNC(LS_TeleportGroup)
// TeleportGroup (group_tid, source_tid, dest_tid, move_source?, fog?)
{
	return EV_TeleportGroup (arg0, it, arg1, arg2, arg3?true:false, arg4?true:false);
}

FUNC(LS_TeleportInSector)
// TeleportInSector (tag, source_tid, dest_tid, bFog, group_tid)
{
	return EV_TeleportSector (arg0, arg1, arg2, arg3?true:false, arg4);
}

FUNC(LS_Teleport_EndGame)
// Teleport_EndGame ()
{
	if (!backSide && CheckIfExitIsGood (it))
	{
		G_SetForEndGame (level.nextmap);
		G_ExitLevel (0, false);
		return true;
	}
	return false;
}

FUNC(LS_Teleport_Line)
// Teleport_Line (thisid, destid, reversed)
{
	return EV_SilentLineTeleport (ln, backSide, it, arg1, arg2);
}

static void ThrustThingHelper (AActor *it, angle_t angle, int force, INTBOOL nolimit);
FUNC(LS_ThrustThing)
// ThrustThing (angle, force, nolimit, tid)
{
	if (arg3 != 0)
	{
		FActorIterator iterator (arg3);
		while ((it = iterator.Next()) != NULL)
		{
			ThrustThingHelper (it, BYTEANGLE(arg0), arg1, arg2);
		}
		return true;
	}
	else if (it)
	{
		ThrustThingHelper (it, BYTEANGLE(arg0), arg1, arg2);
		return true;
	}
	return false;
}

static void ThrustThingHelper (AActor *it, angle_t angle, int force, INTBOOL nolimit)
{
	angle >>= ANGLETOFINESHIFT;
	it->momx += force * finecosine[angle];
	it->momy += force * finesine[angle];
	if (!nolimit)
	{
		it->momx = clamp<fixed_t> (it->momx, -MAXMOVE, MAXMOVE);
		it->momy = clamp<fixed_t> (it->momy, -MAXMOVE, MAXMOVE);
	}
}

FUNC(LS_ThrustThingZ)	// [BC]
// ThrustThingZ (tid, zthrust, down/up, set)
{
	AActor *victim;
	fixed_t thrust = arg1*FRACUNIT/4;

	// [BC] Up is default
	if (arg2)
		thrust = -thrust;

	if (arg0 != 0)
	{
		FActorIterator iterator (arg0);

		while ( (victim = iterator.Next ()) )
		{
			if (!arg3)
				victim->momz = thrust;
			else
				victim->momz += thrust;
		}
		return true;
	}
	else if (it)
	{
		if (!arg3)
			it->momz = thrust;
		else
			it->momz += thrust;
		return true;
	}
	return false;
}

FUNC(LS_Thing_SetSpecial)	// [BC]
// Thing_SetSpecial (tid, special, arg1, arg2, arg3)
// [RH] Use the SetThingSpecial ACS command instead.
// It can set all args and not just the first three.
{
	if (arg0 != 0)
	{
		AActor *actor;
		FActorIterator iterator (arg0);

		while ( (actor = iterator.Next ()) )
		{
			actor->special = arg1;
			actor->args[0] = arg2;
			actor->args[1] = arg3;
			actor->args[2] = arg4;
		}
	}
	return true;
}

FUNC(LS_Thing_ChangeTID)
// Thing_ChangeTID (oldtid, newtid)
{
	if (arg0 == 0)
	{
		if (it != NULL && !(it->ObjectFlags & OF_MassDestruction))
		{
			it->RemoveFromHash ();
			it->tid = arg1;
			it->AddToHash ();
		}
	}
	else
	{
		FActorIterator iterator (arg0);
		AActor *actor, *next;

		next = iterator.Next ();
		while (next != NULL)
		{
			actor = next;
			next = iterator.Next ();

			if (!(actor->ObjectFlags & OF_MassDestruction))
			{
				actor->RemoveFromHash ();
				actor->tid = arg1;
				actor->AddToHash ();
			}
		}
	}
	return true;
}

FUNC(LS_DamageThing)
// DamageThing (damage, mod)
{
	if (it)
	{
		if (arg0 < 0)
		{ // Negative damages mean healing
			if (it->player)
			{
				P_GiveBody (it, -arg0);
			}
			else
			{
				it->health -= arg0;
				if (it->GetDefault()->health < it->health)
					it->health = it->GetDefault()->health;
			}
		}
		else if (arg0 > 0)
		{
			P_DamageMobj (it, NULL, NULL, arg0, arg1);
		}
		else
		{ // If zero damage, guarantee a kill
			P_DamageMobj (it, NULL, NULL, 1000000, arg1);
		}
	}

	return it ? true : false;
}

FUNC(LS_HealThing)
// HealThing (amount, max)
{
	if (it)
	{
		int max = arg1;

		if (max == 0 || it->player == NULL)
		{
			// [BC] If it's a regular player, we can still apply the player's max. health
			// bonus to his maximum allowed health.
			if ( it->player )
			{
				// [BC] Apply the prosperity power.
				if ( it->player->Powers & PW_PROSPERITY )
					max = deh.MaxSoulsphere + 50;
				else
					max = it->GetDefault()->health + it->player->lMaxHealthBonus;
			}
			else
				max = it->GetDefault()->health;
		}
		else if (max == 1)
		{
			// [BC] Include the player's max. health bonus in the max. allowed health.
			max = deh.MaxSoulsphere + it->player->lMaxHealthBonus;
		}

		// If health is already above max, do nothing
		if (it->health < max)
		{
			it->health += arg0;
			if (it->health > max && max > 0)
			{
				it->health = max;
			}
			if (it->player)
			{
				it->player->health = it->health;

				// [BC] If we're the server, send out the health change.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_SetPlayerHealth( ULONG( it->player - players ));
			}
		}
	}

	return it ? true : false;
}

FUNC(LS_Thing_Activate)
// Thing_Activate (tid)
{
	if (arg0 != 0)
	{
		AActor *actor;
		FActorIterator iterator (arg0);
		int count = 0;

		actor = iterator.Next ();
		while (actor)
		{
			// Actor might remove itself as part of activation, so get next
			// one before activating it.
			AActor *temp = iterator.Next ();
			actor->Activate (it);
			actor = temp;
			count++;
		}

		return count != 0;
	}
	else if (it != NULL)
	{
		it->Activate(it);
		return true;
	}
	return false;
}

FUNC(LS_Thing_Deactivate)
// Thing_Deactivate (tid)
{
	if (arg0 != 0)
	{
		AActor *actor;
		FActorIterator iterator (arg0);
		int count = 0;
	
		actor = iterator.Next ();
		while (actor)
		{
			// Actor might removes itself as part of deactivation, so get next
			// one before we activate it.
			AActor *temp = iterator.Next ();
			actor->Deactivate (it);
			actor = temp;
			count++;
		}
	
		return count != 0;
	}
	else if (it != NULL)
	{
		it->Deactivate(it);
		return true;
	}
	return false;
}

static void RemoveThing(AActor * actor)
{
	// Don't remove live players.
	if (actor->player == NULL || actor != actor->player->mo)
	{
		// [BC] DESTROY!
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_DestroyThing( actor );

		// be friendly to the level statistics. ;)
		if (actor->CountsAsKill() && actor->health > 0) level.total_monsters--;
		if (actor->flags&MF_COUNTITEM) level.total_items--;
		actor->Destroy ();
	}
}

FUNC(LS_Thing_Remove)
// Thing_Remove (tid)
{
	if (arg0 != 0)
	{
		FActorIterator iterator (arg0);
		AActor *actor;

		actor = iterator.Next ();
		while (actor)
		{
			AActor *temp = iterator.Next ();

			RemoveThing(actor);
			actor = temp;
		}
	}
	else if (it != NULL)
	{
		RemoveThing(it);
	}

	return true;
}

FUNC(LS_Thing_Destroy)
// Thing_Destroy (tid, extreme)
{
	if (arg0 == 0)
	{
		P_Massacre ();
	}
	else
	{
		FActorIterator iterator (arg0);
		AActor *actor;

		actor = iterator.Next ();
		while (actor)
		{
			AActor *temp = iterator.Next ();
			if (actor->flags & MF_SHOOTABLE)
				P_DamageMobj (actor, NULL, it, arg1 ? 1000000 : actor->health, MOD_UNKNOWN);
			actor = temp;
		}
	}
	return true;
}

FUNC(LS_Thing_Damage)
// Thing_Damage (tid, amount, MOD)
{
	FActorIterator iterator (arg0);
	AActor *actor;

	// [BB] Needed to make Thing_Damage work with tid == 0.
	actor = (arg0 == 0 ? it : iterator.Next());
	while (actor)
	{
		AActor *next = iterator.Next ();
		if (actor->flags & MF_SHOOTABLE)
		{
			if (arg1 > 0)
			{
				P_DamageMobj (actor, NULL, it, arg1, arg2);
			}
			else if (actor->health < actor->GetDefault()->health)
			{
				actor->health -= arg1;
				if (actor->health > actor->GetDefault()->health)
				{
					actor->health = actor->GetDefault()->health;
				}
				if (actor->player != NULL)
				{
					actor->player->health = actor->health;
				}
			}
		}
		actor = next;
	}
	return true;
}

FUNC(LS_Thing_Projectile)
// Thing_Projectile (tid, type, angle, speed, vspeed)
{
	return P_Thing_Projectile (arg0, it, arg1, NULL, BYTEANGLE(arg2), arg3<<(FRACBITS-3),
		arg4<<(FRACBITS-3), 0, NULL, 0, 0, false);
}

FUNC(LS_Thing_ProjectileGravity)
// Thing_ProjectileGravity (tid, type, angle, speed, vspeed)
{
	return P_Thing_Projectile (arg0, it, arg1, NULL, BYTEANGLE(arg2), arg3<<(FRACBITS-3),
		arg4<<(FRACBITS-3), 0, NULL, 1, 0, false);
}

FUNC(LS_Thing_Hate)
// Thing_Hate (hater, hatee, group/"xray"?)
{
	FActorIterator haterIt (arg0);
	AActor *hater, *hatee = NULL;
	FActorIterator hateeIt (arg1);
	bool nothingToHate = false;

	if (arg1 != 0)
	{
		while ((hatee = hateeIt.Next ()))
		{
			if (hatee->flags & MF_SHOOTABLE	&&	// can't hate nonshootable things
				hatee->health > 0 &&			// can't hate dead things
				!(hatee->flags2 & MF2_DORMANT))	// can't target dormant things
			{
				break;
			}
		}
		if (hatee == NULL)
		{ // Nothing to hate
			nothingToHate = true;
		}
	}

	if (arg0 == 0)
	{
		if (it != NULL && it->player != NULL)
		{
			// Players cannot have their attitudes set
			return false;
		}
		else
		{
			hater = it;
		}
	}
	else
	{
		while ((hater = haterIt.Next ()))
		{
			if (hater->health > 0 && hater->flags & MF_SHOOTABLE)
			{
				break;
			}
		}
	}
	while (hater != NULL)
	{
		// Can't hate if can't attack.
		if (hater->SeeState != NULL)
		{
			// If hating a group of things, record the TID and NULL
			// the target (if its TID doesn't match). A_Look will
			// find an appropriate thing to go chase after.
			if (arg2 != 0)
			{
				hater->TIDtoHate = arg1;
				hater->LastLook.Actor = NULL;

				// If the TID to hate is 0, then don't forget the target and
				// lastenemy fields.
				if (arg1 != 0)
				{
					if (hater->target != NULL && hater->target->tid != arg1)
					{
						hater->target = NULL;
					}
					if (hater->lastenemy != NULL && hater->lastenemy->tid != arg1)
					{
						hater->lastenemy = NULL;
					}
				}
			}
			// Hate types for arg2:
			//
			// 0 - Just hate one specific actor
			// 1 - Hate actors with given TID and attack players when shot
			// 2 - Same as 1, but will go after enemies without seeing them first
			// 3 - Hunt actors with given TID and also players
			// 4 - Same as 3, but will go after monsters without seeing them first
			// 5 - Hate actors with given TID and ignore player attacks
			// 6 - Same as 5, but will go after enemies without seeing them first

			// Note here: If you use Thing_Hate (tid, 0, 2), you can make
			// a monster go after a player without seeing him first.
			if (arg2 == 2 || arg2 == 4 || arg2 == 6)
			{
				hater->flags3 |= MF3_NOSIGHTCHECK;
			}
			else
			{
				hater->flags3 &= ~MF3_NOSIGHTCHECK;
			}
			if (arg2 == 3 || arg2 == 4)
			{
				hater->flags3 |= MF3_HUNTPLAYERS;
			}
			else
			{
				hater->flags3 &= ~MF3_HUNTPLAYERS;
			}
			if (arg2 == 5 || arg2 == 6)
			{
				hater->flags4 |= MF4_NOHATEPLAYERS;
			}
			else
			{
				hater->flags4 &= ~MF4_NOHATEPLAYERS;
			}

			if (arg1 == 0)
			{
				hatee = it;
			}
			else if (nothingToHate)
			{
				hatee = NULL;
			}
			else if (arg2 != 0)
			{
				do
				{
					hatee = hateeIt.Next ();
				}
				while ( hatee == NULL ||
						hatee == hater ||					// can't hate self
						!(hatee->flags & MF_SHOOTABLE) ||	// can't hate nonshootable things
						hatee->health <= 0 ||				// can't hate dead things
						(hatee->flags & MF2_DORMANT));		// can't target dormant things
			}

			if (hatee != NULL && hatee != hater && (arg2 == 0 || (hater->goal != NULL && hater->target != hater->goal)))
			{
				if (hater->target)
				{
					hater->lastenemy = hater->target;
				}
				hater->target = hatee;
				if (!(hater->flags2 & MF2_DORMANT))
				{
					hater->SetState (hater->SeeState);
				}
			}
		}
		if (arg0 != 0)
		{
			while ((hater = haterIt.Next ()))
			{
				if (hater->health > 0 && hater->flags & MF_SHOOTABLE)
				{
					break;
				}
			}
		}
		else
		{
			hater = NULL;
		}
	}
	return true;
}

FUNC(LS_Thing_ProjectileAimed)
// Thing_ProjectileAimed (tid, type, speed, target, newtid)
{
	return P_Thing_Projectile (arg0, it, arg1, NULL, 0, arg2<<(FRACBITS-3), 0, arg3, it, 0, arg4, false);
}

FUNC(LS_Thing_ProjectileIntercept)
// Thing_ProjectileIntercept (tid, type, speed, target, newtid)
{
	return P_Thing_Projectile (arg0, it, arg1, NULL, 0, arg2<<(FRACBITS-3), 0, arg3, it, 0, arg4, true);
}

// [BC] added newtid for next two
FUNC(LS_Thing_Spawn)
// Thing_Spawn (tid, type, angle, newtid)
{
	return P_Thing_Spawn (arg0, it, arg1, BYTEANGLE(arg2), true, arg3);
}

FUNC(LS_Thing_SpawnNoFog)
// Thing_SpawnNoFog (tid, type, angle, newtid)
{
	return P_Thing_Spawn (arg0, it, arg1, BYTEANGLE(arg2), false, arg3);
}

FUNC(LS_Thing_SpawnFacing)
// Thing_SpawnFacing (tid, type, nofog, newtid)
{
	return P_Thing_Spawn (arg0, it, arg1, ANGLE_MAX, arg2 ? false : true, arg3);
}

// [BC] No longer static so clients can call this function.
/*static*/ bool DoThingRaise(AActor *thing)
{
	if (thing == NULL)
		return false;	// not valid

	if (!(thing->flags & MF_CORPSE) )
		return true;	// not a corpse
	
	if (thing->tics != -1)
		return true;	// not lying still yet
	
	if (thing->RaiseState == NULL)
		return true;	// monster doesn't have a raise state
	
	AActor *info = thing->GetDefault ();

	thing->momx = thing->momy = 0;

	// [RH] Check against real height and radius
	fixed_t oldheight = thing->height;
	fixed_t oldradius = thing->radius;
	int oldflags = thing->flags;

	thing->flags |= MF_SOLID;
	thing->height = info->height;	// [RH] Use real height
	thing->radius = info->radius;	// [RH] Use real radius
	if (!P_CheckPosition (thing, thing->x, thing->y))
	{
		thing->flags = oldflags;
		thing->radius = oldradius;
		thing->height = oldheight;
		return false;
	}

	S_Sound (thing, CHAN_BODY, "vile/raise", 1, ATTN_IDLE);
	
	thing->SetState (info->RaiseState);
	thing->flags = info->flags;
	thing->flags2 = info->flags2;
	thing->flags3 = info->flags3;
	thing->flags4 = info->flags4;
	// [BC] Apply new ST flags as well.
	thing->flags5 = info->flags5;
	thing->ulSTFlags = info->ulSTFlags;
	thing->ulNetworkFlags = info->ulNetworkFlags;
	thing->health = info->health;
	thing->target = NULL;
	thing->lastenemy = NULL;

	// [RH] If it's a monster, it gets to count as another kill
	if (thing->CountsAsKill())
	{
		level.total_monsters++;

		// [BC] Update invasion's HUD.
		if (( invasion ) && ( NETWORK_GetState( ) != NETSTATE_CLIENT ) && ( CLIENTDEMO_IsPlaying( ) == false ))
		{
			INVASION_SetNumMonstersLeft( INVASION_GetNumMonstersLeft( ) + 1 );

			// [BC] If we're the server, tell the client how many monsters are left.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetInvasionNumMonstersLeft( );
		}
	}

	// [BC] If we're the server, tell clients to put the thing into its raise state.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SetThingState( thing, STATE_RAISE );

	return true;
}

FUNC(LS_Thing_Raise)
// Thing_Raise(tid)
{
	AActor * target;
	bool ok = false;

	if (arg0==0)
	{
		ok = DoThingRaise (it);
	}
	else
	{
		TActorIterator<AActor> iterator (arg0);

		while ( (target = iterator.Next ()) )
		{
			ok |= DoThingRaise(target);
		}
	}
	return ok;
}

FUNC(LS_Thing_SetGoal)
// Thing_SetGoal (tid, goal, delay, chasegoal)
{
	TActorIterator<AActor> selfiterator (arg0);
	TActorIterator<APatrolPoint> goaliterator (arg1);
	AActor *self;
	APatrolPoint *goal = goaliterator.Next ();
	bool ok = false;

	while ( (self = selfiterator.Next ()) )
	{
		ok = true;
		if (self->flags & MF_SHOOTABLE)
		{
			self->goal = goal;
			if (arg3 == 0) self->flags5 &=~ MF5_CHASEGOAL;
			else self->flags5 |= MF5_CHASEGOAL;
			if (self->target == NULL)
			{
				self->reactiontime = arg2 * TICRATE;
			}
		}
	}

	return ok;
}

FUNC(LS_Thing_Move)		// [BC]
// Thing_Move (tid, mapspot, nofog)
{
	return P_Thing_Move (arg0, it, arg1, arg2 ? false : true);
}

FUNC(LS_Thing_SetTranslation)
// Thing_SetTranslation (tid, range)
{
	TActorIterator<AActor> iterator (arg0);
	WORD range;
	AActor *target;
	bool ok = false;

	if (arg1 == -1 && it != NULL)
	{
		range = it->Translation;
	}
	else if (arg1 >= 1 && arg1 < MAX_ACS_TRANSLATIONS)
	{
		range = (TRANSLATION_LevelScripted<<8)|(arg1-1);
	}
	else
	{
		range = 0;
	}

	while ( (target = iterator.Next ()) )
	{
		ok = true;
		target->Translation = range;

		// [BC] If we're the server, tell clients to set this thing's translation.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetThingTranslation( target );
	}

	return ok;
}

FUNC(LS_ACS_Execute)
// ACS_Execute (script, map, s_arg1, s_arg2, s_arg3)
{
	level_info_t *info;

	if ( (arg1 == 0) || !(info = FindLevelByNum (arg1)) )
		return P_StartScript (it, ln, arg0, level.mapname, backSide, arg2, arg3, arg4, false, false);
	else
		return P_StartScript (it, ln, arg0, info->mapname, backSide, arg2, arg3, arg4, false, false);
}

FUNC(LS_ACS_ExecuteAlways)
// ACS_ExecuteAlways (script, map, s_arg1, s_arg2, s_arg3)
{
	level_info_t *info;

	if ( (arg1 == 0) || !(info = FindLevelByNum (arg1)) )
		return P_StartScript (it, ln, arg0, level.mapname, backSide, arg2, arg3, arg4, true, false);
	else
		return P_StartScript (it, ln, arg0, info->mapname, backSide, arg2, arg3, arg4, true, false);
}

FUNC(LS_ACS_LockedExecute)
// ACS_LockedExecute (script, map, s_arg1, s_arg2, lock)
{
	if (arg4 && !P_CheckKeys (it, arg4, true))
		return false;
	else
		return LS_ACS_Execute (ln, it, backSide, arg0, arg1, arg2, arg3, 0);
}

FUNC(LS_ACS_LockedExecuteDoor)
// ACS_LockedExecuteDoor (script, map, s_arg1, s_arg2, lock)
{
	if (arg4 && !P_CheckKeys (it, arg4, false))
		return false;
	else
		return LS_ACS_Execute (ln, it, backSide, arg0, arg1, arg2, arg3, 0);
}

FUNC(LS_ACS_ExecuteWithResult)
// ACS_ExecuteWithResult (script, s_arg1, s_arg2, s_arg3)
{
	// This is like ACS_ExecuteAlways, except the script is always run on
	// the current map, and the return value is whatever the script sets
	// with SetResultValue.
	return P_StartScript (it, ln, arg0, level.mapname, backSide, arg1, arg2, arg3, true, true);
}

FUNC(LS_ACS_Suspend)
// ACS_Suspend (script, map)
{
	level_info_t *info;

	if ( (arg1 == 0) || !(info = FindLevelByNum (arg1)) )
		P_SuspendScript (arg0, level.mapname);
	else
		P_SuspendScript (arg0, info->mapname);

	return true;
}

FUNC(LS_ACS_Terminate)
// ACS_Terminate (script, map)
{
	level_info_t *info;

	if ( (arg1 == 0) || !(info = FindLevelByNum (arg1)) )
		P_TerminateScript (arg0, level.mapname);
	else
		P_TerminateScript (arg0, info->mapname);

	return true;
}

FUNC(LS_FloorAndCeiling_LowerByValue)
// FloorAndCeiling_LowerByValue (tag, speed, height)
{
	return EV_DoElevator (ln, DElevator::elevateLower, SPEED(arg1), arg2*FRACUNIT, arg0);
}

FUNC(LS_FloorAndCeiling_RaiseByValue)
// FloorAndCeiling_RaiseByValue (tag, speed, height)
{
	return EV_DoElevator (ln, DElevator::elevateRaise, SPEED(arg1), arg2*FRACUNIT, arg0);
}

FUNC(LS_FloorAndCeiling_LowerRaise)
// FloorAndCeiling_LowerRaise (tag, fspeed, cspeed)
{
	return EV_DoCeiling (DCeiling::ceilRaiseToHighest, ln, arg0, SPEED(arg2), 0, 0, 0, 0, 0) |
		   EV_DoFloor     (DFloor::floorLowerToLowest, ln, arg0, SPEED(arg1), 0, 0, 0);
}

FUNC(LS_Elevator_MoveToFloor)
// Elevator_MoveToFloor (tag, speed)
{
	return EV_DoElevator (ln, DElevator::elevateCurrent, SPEED(arg1), 0, arg0);
}

FUNC(LS_Elevator_RaiseToNearest)
// Elevator_RaiseToNearest (tag, speed)
{
	return EV_DoElevator (ln, DElevator::elevateUp, SPEED(arg1), 0, arg0);
}

FUNC(LS_Elevator_LowerToNearest)
// Elevator_LowerToNearest (tag, speed)
{
	return EV_DoElevator (ln, DElevator::elevateDown, SPEED(arg1), 0, arg0);
}

FUNC(LS_Light_ForceLightning)
// Light_ForceLightning (tag)
{
	P_ForceLightning ();
	return true;
}

FUNC(LS_Light_RaiseByValue)
// Light_RaiseByValue (tag, value)
{
	EV_LightChange (arg0, arg1);
	return true;
}

FUNC(LS_Light_LowerByValue)
// Light_LowerByValue (tag, value)
{
	EV_LightChange (arg0, -arg1);
	return true;
}

FUNC(LS_Light_ChangeToValue)
// Light_ChangeToValue (tag, value)
{
	EV_LightTurnOn (arg0, arg1);
	return true;
}

FUNC(LS_Light_Fade)
// Light_Fade (tag, value, tics);
{
	EV_StartLightFading (arg0, arg1, TICS(arg2));
	return true;
}

FUNC(LS_Light_Glow)
// Light_Glow (tag, upper, lower, tics)
{
	EV_StartLightGlowing (arg0, arg1, arg2, TICS(arg3));
	return true;
}

FUNC(LS_Light_Flicker)
// Light_Flicker (tag, upper, lower)
{
	EV_StartLightFlickering (arg0, arg1, arg2);
	return true;
}

FUNC(LS_Light_Strobe)
// Light_Strobe (tag, upper, lower, u-tics, l-tics)
{
	EV_StartLightStrobing (arg0, arg1, arg2, TICS(arg3), TICS(arg4));
	return true;
}

FUNC(LS_Light_StrobeDoom)
// Light_StrobeDoom (tag, u-tics, l-tics)
{
	EV_StartLightStrobing (arg0, TICS(arg1), TICS(arg2));
	return true;
}

FUNC(LS_Light_MinNeighbor)
// Light_MinNeighbor (tag)
{
	EV_TurnTagLightsOff (arg0);
	return true;
}

FUNC(LS_Light_MaxNeighbor)
// Light_MaxNeighbor (tag)
{
	EV_LightTurnOn (arg0, -1);
	return true;
}

FUNC(LS_Light_Stop)
// Light_Stop (tag)
{
	EV_StopLightEffect (arg0);
	return true;
}

FUNC(LS_Radius_Quake)
// Radius_Quake (intensity, duration, damrad, tremrad, tid)
{
	return P_StartQuake (it, arg4, arg0, arg1, arg2, arg3);
}

FUNC(LS_UsePuzzleItem)
// UsePuzzleItem (item, script)
{
	AInventory *item;

	if (!it) return false;

	// Check player's inventory for puzzle item
	for (item = it->Inventory; item != NULL; item = item->Inventory)
	{
		if (item->IsKindOf (RUNTIME_CLASS(APuzzleItem)))
		{
			if (static_cast<APuzzleItem*>(item)->PuzzleItemNumber == arg0)
			{
				if (it->UseInventory (item))
				{
					return true;
				}
				break;
			}
		}
	}

	// [RH] Say "hmm" if you don't have the puzzle item
	S_Sound (it, CHAN_VOICE, "*puzzfail", 1, ATTN_IDLE);
	return false;
}

FUNC(LS_Sector_ChangeSound)
// Sector_ChangeSound (tag, sound)
{
	int secNum;
	bool rtn;

	if (!arg0)
		return false;

	secNum = -1;
	rtn = false;
	while ((secNum = P_FindSectorFromTag (arg0,	secNum)) >= 0)
	{
		sectors[secNum].seqType = arg1;
		rtn = true;
	}
	return rtn;
}

// [BC] Start of new Skulltag linespecials.

FUNC( LS_Player_SetTeam )
// Player_SetTeam( blue/red )
{
	// Don't set teams on the client end.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) || ( CLIENTDEMO_IsPlaying( )))
		return ( false );

	// Break if we don't have a player.
	if ( !it || !it->player )
		return ( false );

	// Verify that we're setting a valid team.
	if ( arg0 > NUM_TEAMS )
		I_Error( "Tried to set player to bad team, %d\n", arg0 );

	// Set the player's team.
	PLAYER_SetTeam( it->player, arg0, false );
	return ( true );
}

void SERVERCONSOLE_UpdatePlayerInfo( LONG lPlayer, ULONG ulUpdateFlags );
void SERVERCONSOLE_UpdateScoreboard( void );
FUNC( LS_Team_Score )
// Team_Score (int howmuch, bool nogrin)
{
	// Scoring is not client side.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) || ( CLIENTDEMO_IsPlaying( )))
		return ( false );

	// Nothing to do if we're not in teamgame mode.
	if ( teamgame == false )
		return ( false );

	// Make sure a valid player is doing the scoring.
	if ( !it || !it->player || it->player->bOnTeam == false )
		return ( false );

	TEAM_SetScore( it->player->ulTeam, TEAM_GetScore( it->player->ulTeam ) + arg0, true );

	it->player->lPointCount += arg0;
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		SERVERCOMMANDS_SetPlayerPoints( ULONG( it->player - players ));

		// Also, update the scoreboard.
		SERVERCONSOLE_UpdatePlayerInfo( ULONG( it->player - players ), UDF_FRAGS );
		SERVERCONSOLE_UpdateScoreboard( );
	}

	return ( false );
}

FUNC( LS_Team_GivePoints )
// Team_GivePoints( int iTeam, int iHowMuch, bool bAnnounce )
{
	// Scoring is not client side.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) || ( CLIENTDEMO_IsPlaying( )))
		return ( false );

	// Nothing to do if we're not in teamgame mode.
	if ( teamgame == false )
		return ( false );

	// Make sure this is a valid team.
	if (( arg0 < 0 ) || ( arg0 >= NUM_TEAMS ))
		return ( false );

	// Give the point(s) to the team.
	TEAM_SetScore( arg0, TEAM_GetScore( arg0 ) + arg1, !!arg2 );

	if ( it && it->player && it->player->bOnTeam )
	{
		it->player->lPointCount += arg0;

		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		{
			SERVERCOMMANDS_SetPlayerPoints( ULONG( it->player - players ));

			// Also, update the scoreboard.
			SERVERCONSOLE_UpdatePlayerInfo( ULONG( it->player - players ), UDF_FRAGS );
			SERVERCONSOLE_UpdateScoreboard( );
		}
	}

	return ( false );
}

FUNC( LS_Teleport_NoStop )
// Teleport_NoStop (tid, sectortag, bNoSourceFog)
{
	return EV_Teleport( arg0, arg1, ln, backSide, it, true, !arg2, false, false );
}

// [BC] End of new Skulltag linespecials.

struct FThinkerCollection
{
	int RefNum;
	DThinker *Obj;
};

static TArray<FThinkerCollection> Collection;

void AdjustPusher (int tag, int magnitude, int angle, DPusher::EPusher type)
{
	// Find pushers already attached to the sector, and change their parameters.
	{
		TThinkerIterator<DPusher> iterator;
		FThinkerCollection collect;

		while ( (collect.Obj = iterator.Next ()) )
		{
			if ((collect.RefNum = ((DPusher *)collect.Obj)->CheckForSectorMatch (type, tag)) >= 0)
			{
				((DPusher *)collect.Obj)->ChangeValues (magnitude, angle);
				Collection.Push (collect);
			}
		}
	}

	size_t numcollected = Collection.Size ();
	int secnum = -1;

	// Now create pushers for any sectors that don't already have them.
	while ((secnum = P_FindSectorFromTag (tag, secnum)) >= 0)
	{
		unsigned int i;
		for (i = 0; i < numcollected; i++)
		{
			if (Collection[i].RefNum == sectors[secnum].tag)
				break;
		}
		if (i == numcollected)
		{
			new DPusher (type, NULL, magnitude, angle, NULL, secnum);
		}
	}
	Collection.Clear ();
}

FUNC(LS_Sector_SetWind)
// Sector_SetWind (tag, amount, angle)
{
	if (ln || arg3)
		return false;

	AdjustPusher (arg0, arg1, arg2, DPusher::p_wind);
	return true;
}

FUNC(LS_Sector_SetCurrent)
// Sector_SetCurrent (tag, amount, angle)
{
	if (ln || arg3)
		return false;

	AdjustPusher (arg0, arg1, arg2, DPusher::p_current);
	return true;
}

FUNC(LS_Sector_SetFriction)
// Sector_SetFriction (tag, amount)
{
	P_SetSectorFriction (arg0, arg1, true);
	return true;
}

static void SetWallScroller (int id, int sidechoice, fixed_t dx, fixed_t dy)
{
	if ((dx | dy) == 0)
	{
		// Special case: Remove the scroller, because the deltas are both 0.
		TThinkerIterator<DScroller> iterator (STAT_SCROLLER);
		DScroller *scroller;

		while ( (scroller = iterator.Next ()) )
		{
			int wallnum = scroller->GetWallNum ();

			if (wallnum >= 0 && lines[sides[wallnum].linenum].id == id &&
				lines[sides[wallnum].linenum].sidenum[sidechoice] == (DWORD)wallnum)
			{
				scroller->Destroy ();
			}
		}
	}
	else
	{
		// Find scrollers already attached to the matching walls, and change
		// their rates.
		{
			TThinkerIterator<DScroller> iterator (STAT_SCROLLER);
			FThinkerCollection collect;

			while ( (collect.Obj = iterator.Next ()) )
			{
				if ((collect.RefNum = ((DScroller *)collect.Obj)->GetWallNum ()) != -1 &&
					lines[sides[collect.RefNum].linenum].id == id &&
					lines[sides[collect.RefNum].linenum].sidenum[sidechoice] == (DWORD)collect.RefNum)
				{
					((DScroller *)collect.Obj)->SetRate (dx, dy);
					Collection.Push (collect);
				}
			}
		}

		size_t numcollected = Collection.Size ();
		int linenum = -1;

		// Now create scrollers for any walls that don't already have them.
		while ((linenum = P_FindLineFromID (id, linenum)) >= 0)
		{
			unsigned int i;
			for (i = 0; i < numcollected; i++)
			{
				if ((DWORD)Collection[i].RefNum == lines[linenum].sidenum[sidechoice])
					break;
			}
			if (i == numcollected)
			{
				if (lines[linenum].sidenum[sidechoice] != NO_SIDE)
				{
					new DScroller (DScroller::sc_side, dx, dy, -1, lines[linenum].sidenum[sidechoice], 0);
				}
			}
		}
		Collection.Clear ();
	}
}

FUNC(LS_Scroll_Texture_Both)
// Scroll_Texture_Both (id, left, right, up, down)
{
	if (arg0 == 0)
		return false;

	fixed_t dx = (arg1 - arg2) * (FRACUNIT/64);
	fixed_t dy = (arg4 - arg3) * (FRACUNIT/64);
	int sidechoice;

	if (arg0 < 0)
	{
		sidechoice = 1;
		arg0 = -arg0;
	}
	else
	{
		sidechoice = 0;
	}

	SetWallScroller (arg0, sidechoice, dx, dy);

	return true;
}

/*static*/ void SetScroller (int tag, DScroller::EScrollType type, fixed_t dx, fixed_t dy)
{
	// [BB] The server has to tell the clients to call SetScroller.
	if( NETWORK_GetState() == NETSTATE_SERVER )
		SERVERCOMMANDS_SetScroller(type, dx, dy, tag);

	TThinkerIterator<DScroller> iterator (STAT_SCROLLER);
	DScroller *scroller;
	int i;

	// Check if there is already a scroller for this tag
	// If at least one sector with this tag is scrolling, then they all are.
	// If the deltas are both 0, we don't remove the scroller, because a
	// displacement/accelerative scroller might have been set up, and there's
	// no way to create one after the level is fully loaded.
	i = 0;
	while ( (scroller = iterator.Next ()) )
	{
		if (scroller->IsType (type))
		{
			if (sectors[scroller->GetAffectee ()].tag == tag)
			{
				i++;
				scroller->SetRate (dx, dy);
			}
		}
	}

	if (i > 0 || (dx|dy) == 0)
	{
		return;
	}

	// Need to create scrollers for the sector(s)
	for (i = -1; (i = P_FindSectorFromTag (tag, i)) >= 0; )
	{
		new DScroller (type, dx, dy, -1, i, 0);
	}
}

// NOTE: For the next two functions, x-move and y-move are
// 0-based, not 128-based as they are if they appear on lines.
// Note also that parameter ordering is different.

FUNC(LS_Scroll_Floor)
// Scroll_Floor (tag, x-move, y-move, s/c)
{
	fixed_t dx = arg1 * FRACUNIT/32;
	fixed_t dy = arg2 * FRACUNIT/32;

	if (arg3 == 0 || arg3 == 2)
	{
		SetScroller (arg0, DScroller::sc_floor, -dx, dy);
	}
	else
	{
		SetScroller (arg0, DScroller::sc_floor, 0, 0);
	}
	if (arg3 > 0)
	{
		SetScroller (arg0, DScroller::sc_carry, dx, dy);
	}
	else
	{
		SetScroller (arg0, DScroller::sc_carry, 0, 0);
	}
	return true;
}

FUNC(LS_Scroll_Ceiling)
// Scroll_Ceiling (tag, x-move, y-move, 0)
{
	fixed_t dx = arg1 * FRACUNIT/32;
	fixed_t dy = arg2 * FRACUNIT/32;

	SetScroller (arg0, DScroller::sc_ceiling, -dx, dy);
	return true;
}

FUNC(LS_PointPush_SetForce)
// PointPush_SetForce ()
{
	return false;
}

FUNC(LS_Sector_SetDamage)
// Sector_SetDamage (tag, amount, mod)
{
	int secnum = -1;
	while ((secnum = P_FindSectorFromTag (arg0, secnum)) >= 0) {
		sectors[secnum].damage = arg1;
		sectors[secnum].mod = arg2;
	}
	return true;
}

FUNC(LS_Sector_SetGravity)
// Sector_SetGravity (tag, intpart, fracpart)
{
	int secnum = -1;
	float gravity;

	if (arg2 > 99)
		arg2 = 99;
	gravity = (float)arg1 + (float)arg2 * 0.01f;

	while ((secnum = P_FindSectorFromTag (arg0, secnum)) >= 0)
	{
		sectors[secnum].gravity = gravity;

		// [BC] If we're the server, tell clients that this sector's gravity is being altered.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetSectorGravity( secnum );
	}

	return true;
}

FUNC(LS_Sector_SetColor)
// Sector_SetColor (tag, r, g, b, desaturate)
{
	int secnum = -1;
	PalEntry color = PalEntry (arg1, arg2, arg3);
	
	while ((secnum = P_FindSectorFromTag (arg0, secnum)) >= 0)
	{
		sectors[secnum].ColorMap = GetSpecialLights (color, sectors[secnum].ColorMap->Fade, arg4);

		// Tell clients about the sector color update.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetSectorColor( secnum );
	}

	return true;
}

FUNC(LS_Sector_SetFade)
// Sector_SetFade (tag, r, g, b)
{
	int secnum = -1;
	PalEntry fade = PalEntry (arg1, arg2, arg3);

	while ((secnum = P_FindSectorFromTag (arg0, secnum)) >= 0)
	{
		sectors[secnum].ColorMap = GetSpecialLights (sectors[secnum].ColorMap->Color, fade, sectors[secnum].ColorMap->Desaturate);

		// [BC] Tell clients about the sector fade update.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetSectorFade( secnum );
	}
	return true;
}

FUNC(LS_Sector_SetCeilingPanning)
// Sector_SetCeilingPanning (tag, x-int, x-frac, y-int, y-frac)
{
	int secnum = -1;
	fixed_t xofs = arg1 * FRACUNIT + arg2 * (FRACUNIT/100);
	fixed_t yofs = arg3 * FRACUNIT + arg4 * (FRACUNIT/100);

	while ((secnum = P_FindSectorFromTag (arg0, secnum)) >= 0)
	{
		sectors[secnum].ceiling_xoffs = xofs;
		sectors[secnum].ceiling_yoffs = yofs;

		// Tell clients about the floor panning update.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetSectorPanning( secnum );
	}
	return true;
}

FUNC(LS_Sector_SetFloorPanning)
// Sector_SetFloorPanning (tag, x-int, x-frac, y-int, y-frac)
{
	int secnum = -1;
	fixed_t xofs = arg1 * FRACUNIT + arg2 * (FRACUNIT/100);
	fixed_t yofs = arg3 * FRACUNIT + arg4 * (FRACUNIT/100);

	while ((secnum = P_FindSectorFromTag (arg0, secnum)) >= 0)
	{
		sectors[secnum].floor_xoffs = xofs;
		sectors[secnum].floor_yoffs = yofs;

		// Tell clients about the floor panning update.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetSectorPanning( secnum );
	}
	return true;
}

FUNC(LS_Sector_SetCeilingScale)
// Sector_SetCeilingScale (tag, x-int, x-frac, y-int, y-frac)
{
	int secnum = -1;
	fixed_t xscale = arg1 * FRACUNIT + arg2 * (FRACUNIT/100);
	fixed_t yscale = arg3 * FRACUNIT + arg4 * (FRACUNIT/100);

	if (xscale)
		xscale = FixedDiv (FRACUNIT, xscale);
	if (yscale)
		yscale = FixedDiv (FRACUNIT, yscale);

	while ((secnum = P_FindSectorFromTag (arg0, secnum)) >= 0)
	{
		if (xscale)
			sectors[secnum].ceiling_xscale = xscale;
		if (yscale)
			sectors[secnum].ceiling_yscale = yscale;

		// [BC] Tell clients about the scale update.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetSectorScale( secnum );
	}
	return true;
}

FUNC(LS_Sector_SetFloorScale2)
// Sector_SetFloorScale2 (tag, x-factor, y-factor)
{
	int secnum = -1;

	if (arg1)
		arg1 = FixedDiv (FRACUNIT, arg1);
	if (arg2)
		arg2 = FixedDiv (FRACUNIT, arg2);

	while ((secnum = P_FindSectorFromTag (arg0, secnum)) >= 0)
	{
		if (arg1)
			sectors[secnum].floor_xscale = arg1;
		if (arg2)
			sectors[secnum].floor_yscale = arg2;

		// [BC] Tell clients about the scale update.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetSectorScale( secnum );
	}
	return true;
}

FUNC(LS_Sector_SetCeilingScale2)
// Sector_SetFloorScale2 (tag, x-factor, y-factor)
{
	int secnum = -1;

	if (arg1)
		arg1 = FixedDiv (FRACUNIT, arg1);
	if (arg2)
		arg2 = FixedDiv (FRACUNIT, arg2);

	while ((secnum = P_FindSectorFromTag (arg0, secnum)) >= 0)
	{
		if (arg1)
			sectors[secnum].ceiling_xscale = arg1;
		if (arg2)
			sectors[secnum].ceiling_yscale = arg2;

		// [BC] Tell clients about the scale update.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetSectorScale( secnum );
	}
	return true;
}

FUNC(LS_Sector_SetFloorScale)
// Sector_SetFloorScale (tag, x-int, x-frac, y-int, y-frac)
{
	int secnum = -1;
	fixed_t xscale = arg1 * FRACUNIT + arg2 * (FRACUNIT/100);
	fixed_t yscale = arg3 * FRACUNIT + arg4 * (FRACUNIT/100);

	if (xscale)
		xscale = FixedDiv (FRACUNIT, xscale);
	if (yscale)
		yscale = FixedDiv (FRACUNIT, yscale);

	while ((secnum = P_FindSectorFromTag (arg0, secnum)) >= 0)
	{
		if (xscale)
			sectors[secnum].floor_xscale = xscale;
		if (yscale)
			sectors[secnum].floor_yscale = yscale;

		// [BC] Tell clients about the scale update.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetSectorScale( secnum );
	}
	return true;
}

FUNC(LS_Sector_SetRotation)
// Sector_SetRotation (tag, floor-angle, ceiling-angle)
{
	int secnum = -1;
	angle_t ceiling = arg2 * ANGLE_1;
	angle_t floor = arg1 * ANGLE_1;

	while ((secnum = P_FindSectorFromTag (arg0, secnum)) >= 0)
	{
		sectors[secnum].floor_angle = floor;
		sectors[secnum].ceiling_angle = ceiling;

		// [BC] Tell clients about the rotation update.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetSectorRotation( secnum );
	}
	return true;
}

FUNC(LS_Line_AlignCeiling)
// Line_AlignCeiling (lineid, side)
{
	int line = P_FindLineFromID (arg0, -1);
	bool ret = 0;

	if (line < 0)
		I_Error ("Sector_AlignCeiling: Lineid %d is undefined", arg0);
	do
	{
		ret |= R_AlignFlat (line, !!arg1, 1);
	} while ( (line = P_FindLineFromID (arg0, line)) >= 0);
	return ret;
}

FUNC(LS_Line_AlignFloor)
// Line_AlignFloor (lineid, side)
{
	int line = P_FindLineFromID (arg0, -1);
	bool ret = 0;

	if (line < 0)
		I_Error ("Sector_AlignFloor: Lineid %d is undefined", arg0);
	do
	{
		ret |= R_AlignFlat (line, !!arg1, 0);
	} while ( (line = P_FindLineFromID (arg0, line)) >= 0);
	return ret;
}

FUNC(LS_ChangeCamera)
// ChangeCamera (tid, who, revert?)
{
	AActor *camera;
	if (arg0 != 0)
	{
		FActorIterator iterator (arg0);
		camera = iterator.Next ();
	}
	else
	{
		camera = NULL;
	}

	if (!it || !it->player || arg1)
	{
		int i;

		for (i = 0; i < MAXPLAYERS; i++)
		{
			if (!playeringame[i])
				continue;

			if (camera)
			{
				players[i].camera = camera;
				if (arg2)
					players[i].cheats |= CF_REVERTPLEASE;

				// [BC] If we're the server, tell this player to change his camera.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_SetPlayerCamera( i, camera->lNetID, ( arg2 ) ? true : false );
			}
			else
			{
				players[i].camera = players[i].mo;
				players[i].cheats &= ~CF_REVERTPLEASE;

				// [BC] If we're the server, tell this player to change his camera.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_SetPlayerCamera( i, players[i].mo->lNetID, false );
			}
		}
	}
	else
	{
		if (camera)
		{
			it->player->camera = camera;
			if (arg2)
				it->player->cheats |= CF_REVERTPLEASE;

			// [BC] If we're the server, tell this player to change his camera.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetPlayerCamera( ULONG( it->player - players ), camera->lNetID, true );
		}
		else
		{
			it->player->camera = it;
			it->player->cheats &= ~CF_REVERTPLEASE;

			// [BC] If we're the server, tell this player to change his camera.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetPlayerCamera( ULONG( it->player - players ), it->lNetID, false );
		}
	}

	return true;
}

enum
{
	PROP_FROZEN,
	PROP_NOTARGET,
	PROP_INSTANTWEAPONSWITCH,
	PROP_FLY,
	PROP_TOTALLYFROZEN,

	PROP_INVULNERABILITY,
	PROP_STRENGTH,
	PROP_INVISIBILITY,
	PROP_RADIATIONSUIT,
	PROP_ALLMAP,
	PROP_INFRARED,
	PROP_WEAPONLEVEL2,
	PROP_FLIGHT,
	PROP_UNUSED1,
	PROP_UNUSED2,
	PROP_SPEED,
};

FUNC(LS_SetPlayerProperty)
// SetPlayerProperty (who, set, which)
{
	int mask = 0;

	if ((!it || !it->player) && !arg0)
		return false;

	// Add or remove a power
	if (arg2 >= PROP_INVULNERABILITY && arg2 <= PROP_SPEED)
	{
		static const PClass *powers[11] =
		{
			RUNTIME_CLASS(APowerInvulnerable),
			RUNTIME_CLASS(APowerStrength),
			RUNTIME_CLASS(APowerInvisibility),
			RUNTIME_CLASS(APowerIronFeet),
			NULL, // MapRevealer
			RUNTIME_CLASS(APowerLightAmp),
			RUNTIME_CLASS(APowerWeaponLevel2),
			RUNTIME_CLASS(APowerFlight),
			NULL,
			NULL,
			RUNTIME_CLASS(APowerSpeed)
		};
		int power = arg2 - PROP_INVULNERABILITY;

		if (power > 4 && powers[power] == NULL)
		{
			return false;
		}

		if (arg0 == 0)
		{
			if (arg1)
			{ // Give power to activator
				if (power != 4)
				{
					it->GiveInventoryType (powers[power]);
				}
				else if (it->player - players == consoleplayer)
				{
					level.flags |= LEVEL_ALLMAP;
				}
			}
			else
			{ // Take power from activator
				if (power != 4)
				{
					AInventory *item = it->FindInventory (powers[power]);
					if (item != NULL)
					{
						item->Destroy ();
					}
				}
				else if (it->player - players == consoleplayer)
				{
					level.flags &= ~LEVEL_ALLMAP;
				}
			}
		}
		else
		{
			int i;

			for (i = 0; i < MAXPLAYERS; i++)
			{
				if (!playeringame[i] || players[i].mo == NULL)
					continue;

				if (arg1)
				{ // Give power
					if (power != 4)
					{
						players[i].mo->GiveInventoryType (powers[power]);
					}
					else if (i == consoleplayer)
					{
						level.flags |= LEVEL_ALLMAP;
					}
				}
				else
				{ // Take power
					if (power != 4)
					{
						AInventory *item = players[i].mo->FindInventory (powers[power]);
						if (item != NULL)
						{
							item->Destroy ();
						}
					}
					else if (i == consoleplayer)
					{
						level.flags &= ~LEVEL_ALLMAP;
					}
				}
			}
		}
		return true;
	}

	// Set or clear a flag
	switch (arg2)
	{
	case PROP_FROZEN:
		mask = CF_FROZEN;
		break;
	case PROP_NOTARGET:
		mask = CF_NOTARGET;
		break;
	case PROP_INSTANTWEAPONSWITCH:
		mask = CF_INSTANTWEAPSWITCH;
		break;
	case PROP_FLY:
		mask = CF_FLY;
		break;
	case PROP_TOTALLYFROZEN:
		mask = CF_TOTALLYFROZEN;
		break;
	}

	if (arg0 == 0)
	{
		if (arg1)
		{
			it->player->cheats |= mask;
			if (arg2 == PROP_FLY)
			{
				it->flags2 |= MF2_FLY;
				it->flags |= MF_NOGRAVITY;
			}
		}
		else
		{
			it->player->cheats &= ~mask;
			if (arg2 == PROP_FLY)
			{
				it->flags2 &= ~MF2_FLY;
				it->flags &= ~MF_NOGRAVITY;
			}
		}
	}
	else
	{
		int i;

		for (i = 0; i < MAXPLAYERS; i++)
		{
			if (!playeringame[i])
				continue;

			if (arg1)
			{
				players[i].cheats |= mask;
				if (arg2 == PROP_FLY)
				{
					players[i].mo->flags2 |= MF2_FLY;
					players[i].mo->flags |= MF_NOGRAVITY;
				}
			}
			else
			{
				players[i].cheats &= ~mask;
				if (arg2 == PROP_FLY)
				{
					players[i].mo->flags2 &= ~MF2_FLY;
					players[i].mo->flags &= ~MF_NOGRAVITY;
				}
			}
		}
	}

	return !!mask;
}

FUNC(LS_TranslucentLine)
// TranslucentLine (id, amount, type)
{
	int linenum = -1;
	while ((linenum = P_FindLineFromID (arg0, linenum)) >= 0)
	{
		lines[linenum].alpha = arg1 & 255;

		// [BC] If we're the server, tell clients to adjust this line's alpha.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetLineAlpha( linenum );

		if (arg2 == 0)
		{
			sides[lines[linenum].sidenum[0]].Flags &= ~WALLF_ADDTRANS;
			if (lines[linenum].sidenum[1] != NO_SIDE)
			{
				sides[lines[linenum].sidenum[1]].Flags &= ~WALLF_ADDTRANS;
			}
		}
		else if (arg2 == 1)
		{
			sides[lines[linenum].sidenum[0]].Flags |= WALLF_ADDTRANS;
			if (lines[linenum].sidenum[1] != NO_SIDE)
			{
				sides[lines[linenum].sidenum[1]].Flags |= WALLF_ADDTRANS;
			}
		}
		else
		{
			Printf ("Unknown translucency type used with TranslucentLine\n");
		}
	}

	return true;
}

FUNC(LS_Autosave)
{
	if (gameaction != ga_savegame)
	{
		Net_WriteByte (DEM_CHECKAUTOSAVE);
	}
	return true;
}

FUNC(LS_ChangeSkill)
{
	if (arg0 < sk_baby || arg0 > sk_nightmare)
	{
		NextSkill = -1;
	}
	else
	{
		NextSkill = arg0;
	}
	return true;
}

FUNC(LS_NoiseAlert)
// NoiseAlert (TID of target, TID of emitter)
{
	AActor *target, *emitter;

	if (arg0 == 0)
	{
		target = it;
	}
	else
	{
		FActorIterator iter (arg0);
		target = iter.Next();
	}

	if (arg1 == 0)
	{
		emitter = it;
	}
	else if (arg1 == arg0)
	{
		emitter = target;
	}
	else
	{
		FActorIterator iter (arg1);
		emitter = iter.Next();
	}

	P_NoiseAlert (target, emitter);
	return true;
}

FUNC(LS_SendToCommunicator)
// SendToCommunicator (voc #, front-only, identify, nolog)
{
	// This obviously isn't going to work for co-op.
	if (arg1 && backSide)
		return false;

	if (it != NULL && it->player != NULL && it->FindInventory<ACommunicator>())
	{
		char name[32];
		sprintf (name, "svox/voc%d", arg0);

		if (!arg3)
		{
			it->player->SetLogNumber (arg0);
		}

		if (it->CheckLocalView (consoleplayer))
		{
			S_StopSound ((fixed_t *)NULL, CHAN_VOICE);
			S_Sound (CHAN_VOICE, name, 1, ATTN_NORM);
			if (arg2 == 0)
			{
				Printf (PRINT_CHAT, "Incoming Message\n");
			}
			else if (arg2 == 1)
			{
				Printf (PRINT_CHAT, "Incoming Message from BlackBird\n");
			}
		}
		return true;
	}
	return false;
}

FUNC(LS_ForceField)
// ForceField ()
{
	if (it != NULL)
	{
		P_DamageMobj (it, NULL, NULL, 16, MOD_UNKNOWN);
		P_ThrustMobj (it, it->angle + ANGLE_180, 0x7D000);
	}
	return true;
}

FUNC(LS_ClearForceField)
// ClearForceField (tag)
{
	int secnum = -1;
	bool rtn = false;

	while ((secnum = P_FindSectorFromTag (arg0, secnum)) >= 0)
	{
		sector_t *sec = &sectors[secnum];
		rtn = true;

		for (int i = 0; i < sec->linecount; ++i)
		{
			line_t *line = sec->lines[i];
			if (line->backsector != NULL && line->special == ForceField)
			{
				line->flags &= ~(ML_BLOCKING|ML_BLOCKEVERYTHING);
				line->special = 0;
				sides[line->sidenum[0]].midtexture = 0;
				sides[line->sidenum[1]].midtexture = 0;

				// [BC] Mark this line's texture change flags.
				line->ulTexChangeFlags |= TEXCHANGE_FRONTMEDIUM|TEXCHANGE_BACKMEDIUM;

				// [BC] If we're the server, tell clients that this line's textures and
				// blocking status have been altered.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				{
					SERVERCOMMANDS_SetLineTexture( ULONG( line - lines ));
					SERVERCOMMANDS_SetLineBlocking( ULONG( line - lines ));
				}
			}
		}
	}
	return rtn;
}

FUNC(LS_GlassBreak)
// GlassBreak (bNoJunk)
{
	bool switched;
	bool quest1, quest2;

	ln->flags &= ~(ML_BLOCKING|ML_BLOCKEVERYTHING);

	// [BC] If we're the server, update this line's blocking.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SetLineBlocking( ULONG( ln - lines ));

	switched = P_ChangeSwitchTexture (&sides[ln->sidenum[0]], false, 0, &quest1);
	ln->special = 0;
	if (ln->sidenum[1] != NO_SIDE)
	{
		switched |= P_ChangeSwitchTexture (&sides[ln->sidenum[1]], false, 0, &quest2);
	}
	else
	{
		quest2 = quest1;
	}
	if (switched)
	{
		if (!arg0)
		{ // Break some glass
			fixed_t x, y;
			AActor *glass;
			angle_t an;
			int speed;

			x = ln->v1->x + ln->dx/2;
			y = ln->v1->y + ln->dy/2;
			x += (ln->frontsector->soundorg[0] - x) / 5;
			y += (ln->frontsector->soundorg[1] - y) / 5;

			for (int i = 0; i < 7; ++i)
			{
				glass = Spawn("GlassJunk", x, y, ONFLOORZ, ALLOW_REPLACE);

				glass->z += 24 * FRACUNIT;
				glass->SetState (glass->SpawnState + (pr_glass() % glass->health));
				an = pr_glass() << (32-8);
				glass->angle = an;
				an >>= ANGLETOFINESHIFT;
				speed = pr_glass() & 3;
				glass->momx = finecosine[an] * speed;
				glass->momy = finesine[an] * speed;
				glass->momz = (pr_glass() & 7) << FRACBITS;
				// [RH] Let the shards stick around longer than they did in Strife.
				glass->tics += pr_glass();
			}
		}
		if (quest1 || quest2)
		{ // Up stats and signal this mission is complete
			if (it == NULL)
			{
				for (int i = 0; i < MAXPLAYERS; ++i)
				{
					if (playeringame[i])
					{
						it = players[i].mo;
						break;
					}
				}
			}
			if (it != NULL)
			{
				it->GiveInventoryType (QuestItemClasses[28]);
				it->GiveInventoryType (RUNTIME_CLASS(AUpgradeAccuracy));
				it->GiveInventoryType (RUNTIME_CLASS(AUpgradeStamina));
			}
		}
	}
	// We already changed the switch texture, so don't make the main code switch it back.
	return false;
}

FUNC(LS_StartConversation)
// StartConversation (tid, facetalker)
{
	FActorIterator iterator (arg0);

	AActor *target = iterator.Next();

	// Nothing to talk to
	if (target == NULL)
	{
		return false;
	}
	
	// Only living players are allowed to start conversations
	if (it == NULL || it->player == NULL || it->player->mo != it || it->health<=0)
	{
		return false;
	}

	// Dead things can't talk.
	if (target->health <= 0)
	{
		return false;
	}
	// Fighting things don't talk either.
	if (target->flags4 & MF4_INCOMBAT)
	{
		return false;
	}
	if (target->Conversation != NULL)
	{
		// Give the NPC a chance to play a brief animation
		target->ConversationAnimation (0);
		P_StartConversation (target, it, !!arg1, true);
		return true;
	}
	return false;
}


lnSpecFunc LineSpecials[256] =
{
	LS_NOP,
	LS_NOP,		// Polyobj_StartLine,
	LS_Polyobj_RotateLeft,
	LS_Polyobj_RotateRight,
	LS_Polyobj_Move,
	LS_NOP,		// Polyobj_ExplicitLine
	LS_Polyobj_MoveTimes8,
	LS_Polyobj_DoorSwing,
	LS_Polyobj_DoorSlide,
	LS_NOP,		// Line_Horizon
	LS_Door_Close,
	LS_Door_Open,
	LS_Door_Raise,
	LS_Door_LockedRaise,
	LS_Door_Animated,
	LS_Autosave,
	LS_NOP,		// Transfer_WallLight
	LS_Thing_Raise,
	LS_StartConversation,
	LS_NOP,		// 19
	LS_Floor_LowerByValue,
	LS_Floor_LowerToLowest,
	LS_Floor_LowerToNearest,
	LS_Floor_RaiseByValue,
	LS_Floor_RaiseToHighest,
	LS_Floor_RaiseToNearest,
	LS_Stairs_BuildDown,
	LS_Stairs_BuildUp,
	LS_Floor_RaiseAndCrush,
	LS_Pillar_Build,
	LS_Pillar_Open,
	LS_Stairs_BuildDownSync,
	LS_Stairs_BuildUpSync,
	LS_ForceField,
	LS_ClearForceField,
	LS_Floor_RaiseByValueTimes8,
	LS_Floor_LowerByValueTimes8,
	LS_NOP,		// 37
	LS_Ceiling_Waggle,
	LS_Teleport_ZombieChanger,
	LS_Ceiling_LowerByValue,
	LS_Ceiling_RaiseByValue,
	LS_Ceiling_CrushAndRaise,
	LS_Ceiling_LowerAndCrush,
	LS_Ceiling_CrushStop,
	LS_Ceiling_CrushRaiseAndStay,
	LS_Floor_CrushStop,
	LS_NOP,		// 47
	LS_NOP,		// 48
	LS_GlassBreak,
	LS_NOP,		// 50: ExtraFloor_LightOnly
	LS_NOP,		// 51
	LS_NOP,		// 52
	LS_NOP,		// 53
	LS_NOP,		// 54
	LS_NOP,		// 55
	LS_NOP,		// 56
	LS_NOP,		// 57
	LS_NOP,		// 58
	LS_NOP,		// 59
	LS_Plat_PerpetualRaise,
	LS_Plat_Stop,
	LS_Plat_DownWaitUpStay,
	LS_Plat_DownByValue,
	LS_Plat_UpWaitDownStay,
	LS_Plat_UpByValue,
	LS_Floor_LowerInstant,
	LS_Floor_RaiseInstant,
	LS_Floor_MoveToValueTimes8,
	LS_Ceiling_MoveToValueTimes8,
	LS_Teleport,
	LS_Teleport_NoFog,
	LS_ThrustThing,
	LS_DamageThing,
	LS_Teleport_NewMap,
	LS_Teleport_EndGame,
	LS_TeleportOther,
	LS_TeleportGroup,
	LS_TeleportInSector,
	LS_NOP,		// 79
	LS_ACS_Execute,
	LS_ACS_Suspend,
	LS_ACS_Terminate,
	LS_ACS_LockedExecute,
	LS_ACS_ExecuteWithResult,
	LS_ACS_LockedExecuteDoor,
	LS_NOP,		// 86
	LS_NOP,		// 87
	LS_NOP,		// 88
	LS_NOP,		// 89
	LS_Polyobj_OR_RotateLeft,
	LS_Polyobj_OR_RotateRight,
	LS_Polyobj_OR_Move,
	LS_Polyobj_OR_MoveTimes8,
	LS_Pillar_BuildAndCrush,
	LS_FloorAndCeiling_LowerByValue,
	LS_FloorAndCeiling_RaiseByValue,
	LS_NOP,		// 97
	LS_NOP,		// 98
	LS_NOP,		// 99
	LS_NOP,		// Scroll_Texture_Left
	LS_NOP,		// Scroll_Texture_Right
	LS_NOP,		// Scroll_Texture_Up
	LS_NOP,		// Scroll_Texture_Down
	LS_NOP,		// 104
	LS_NOP,		// 105
	LS_NOP,		// 106
	LS_NOP,		// 107
	LS_NOP,		// 108
	LS_Light_ForceLightning,
	LS_Light_RaiseByValue,
	LS_Light_LowerByValue,
	LS_Light_ChangeToValue,
	LS_Light_Fade,
	LS_Light_Glow,
	LS_Light_Flicker,
	LS_Light_Strobe,
	LS_Light_Stop,
	LS_NOP,		// 118
	LS_Thing_Damage,
	LS_Radius_Quake,
	LS_NOP,		// Line_SetIdentification
	LS_NOP,		// Thing_SetGravity			// [BC] Start
	LS_NOP,		// Thing_ReverseGravity
	LS_NOP,		// Thing_RevertGravity
	LS_Thing_Move,
	LS_NOP,		// LS_Thing_SetFrame
	LS_Thing_SetSpecial,
	LS_ThrustThingZ,						// [BC] End
	LS_UsePuzzleItem,
	LS_Thing_Activate,
	LS_Thing_Deactivate,
	LS_Thing_Remove,
	LS_Thing_Destroy,
	LS_Thing_Projectile,
	LS_Thing_Spawn,
	LS_Thing_ProjectileGravity,
	LS_Thing_SpawnNoFog,
	LS_Floor_Waggle,
	LS_Thing_SpawnFacing,
	LS_Sector_ChangeSound,
	LS_NOP,		// 141 Music_Pause			// [BC] Start
	LS_NOP,		// 142 Music_Change
	LS_NOP,		// 143 Player_RemoveItem,
	LS_NOP,		// 144 Player_GiveItem,
	LS_Player_SetTeam,
	LS_NOP,		// 146 Player_SetLeader
	LS_NOP,		// 147 Team_InitFP
	LS_NOP,		// 148 TeleportAll
	LS_NOP,		// 149 TeleportAll_NoFog
	LS_NOP,		// 150 Team_GiveFP
	LS_NOP,		// 151 Team_UseFP
	LS_Team_Score,
	LS_Team_GivePoints,
	LS_Teleport_NoStop,
	LS_NOP,		// 155 Team_RemoveItem
	LS_NOP,		// 156 Team_GiveItem		// [BC] End
	LS_NOP,		// 157
	LS_NOP,		// 158
	LS_NOP,		// 159
	LS_NOP,		// 160
	LS_NOP,		// 161
	LS_NOP,		// 162
	LS_NOP,		// 163
	LS_NOP,		// 164
	LS_NOP,		// 165
	LS_NOP,		// 166
	LS_NOP,		// 167
	LS_NOP,		// 168
	LS_NOP,		// 169
	LS_Sector_SetCeilingScale2,
	LS_Sector_SetFloorScale2,
	LS_Plat_UpNearestWaitDownStay,
	LS_NoiseAlert,
	LS_SendToCommunicator,
	LS_Thing_ProjectileIntercept,
	LS_Thing_ChangeTID,
	LS_Thing_Hate,
	LS_Thing_ProjectileAimed,
	LS_ChangeSkill,
	LS_Thing_SetTranslation,
	LS_NOP,		// Plane_Align
	LS_NOP,		// Line_Mirror
	LS_Line_AlignCeiling,
	LS_Line_AlignFloor,
	LS_Sector_SetRotation,
	LS_Sector_SetCeilingPanning,
	LS_Sector_SetFloorPanning,
	LS_Sector_SetCeilingScale,
	LS_Sector_SetFloorScale,
	LS_NOP,		// Static_Init
	LS_SetPlayerProperty,
	LS_Ceiling_LowerToHighestFloor,
	LS_Ceiling_LowerInstant,
	LS_Ceiling_RaiseInstant,
	LS_Ceiling_CrushRaiseAndStayA,
	LS_Ceiling_CrushAndRaiseA,
	LS_Ceiling_CrushAndRaiseSilentA,
	LS_Ceiling_RaiseByValueTimes8,
	LS_Ceiling_LowerByValueTimes8,
	LS_Generic_Floor,
	LS_Generic_Ceiling,
	LS_Generic_Door,
	LS_Generic_Lift,
	LS_Generic_Stairs,
	LS_Generic_Crusher,
	LS_Plat_DownWaitUpStayLip,
	LS_Plat_PerpetualRaiseLip,
	LS_TranslucentLine,
	LS_NOP,		// Transfer_Heights
	LS_NOP,		// Transfer_FloorLight
	LS_NOP,		// Transfer_CeilingLight
	LS_Sector_SetColor,
	LS_Sector_SetFade,
	LS_Sector_SetDamage,
	LS_Teleport_Line,
	LS_Sector_SetGravity,
	LS_Stairs_BuildUpDoom,
	LS_Sector_SetWind,
	LS_Sector_SetFriction,
	LS_Sector_SetCurrent,
	LS_Scroll_Texture_Both,
	LS_NOP,		// Scroll_Texture_Model
	LS_Scroll_Floor,
	LS_Scroll_Ceiling,
	LS_NOP,		// Scroll_Texture_Offsets
	LS_ACS_ExecuteAlways,
	LS_PointPush_SetForce,
	LS_Plat_RaiseAndStayTx0,
	LS_Thing_SetGoal,
	LS_Plat_UpByValueStayTx,
	LS_Plat_ToggleCeiling,
	LS_Light_StrobeDoom,
	LS_Light_MinNeighbor,
	LS_Light_MaxNeighbor,
	LS_Floor_TransferTrigger,
	LS_Floor_TransferNumeric,
	LS_ChangeCamera,
	LS_Floor_RaiseToLowestCeiling,
	LS_Floor_RaiseByValueTxTy,
	LS_Floor_RaiseByTexture,
	LS_Floor_LowerToLowestTxTy,
	LS_Floor_LowerToHighest,
	LS_Exit_Normal,
	LS_Exit_Secret,
	LS_Elevator_RaiseToNearest,
	LS_Elevator_MoveToFloor,
	LS_Elevator_LowerToNearest,
	LS_HealThing,
	LS_Door_CloseWaitOpen,
	LS_Floor_Donut,
	LS_FloorAndCeiling_LowerRaise,
	LS_Ceiling_RaiseToNearest,
	LS_Ceiling_LowerToLowest,
	LS_Ceiling_LowerToFloor,
	LS_Ceiling_CrushRaiseAndStaySilA
};
