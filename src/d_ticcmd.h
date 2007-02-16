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
//		System specific interface stuff.
//
//-----------------------------------------------------------------------------


#ifndef __D_TICCMD_H__
#define __D_TICCMD_H__

#include "doomtype.h"
#include "d_protocol.h"

// The data sampled per tick (single player)
// and transmitted to other peers (multiplayer).
// Mainly movements/button commands per game tick,
// plus a checksum for internal state consistency.
struct ticcmd_t
{
	usercmd_t	ucmd;
/*
	char		forwardmove;	// *2048 for move
	char		sidemove;		// *2048 for move
	short		angleturn;		// <<16 for angle delta
*/
	SWORD		consistancy;	// checks for net game
};


inline FArchive &operator<< (FArchive &arc, ticcmd_t &cmd)
{
	return arc << cmd.consistancy << cmd.ucmd;
}

#endif	// __D_TICCMD_H__
