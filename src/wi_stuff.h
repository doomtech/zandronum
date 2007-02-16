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
//	Intermission.
//
//-----------------------------------------------------------------------------

#ifndef __WI_STUFF__
#define __WI_STUFF__

#include "doomdef.h"

//
// INTERMISSION
// Structure passed e.g. to WI_Start(wb)
//
typedef struct wbplayerstruct_s
{
	bool		in;			// whether the player is in game
	
	// Player stats, kills, collected items etc.
	int			skills;
	int			sitems;
	int			ssecret;
	int			stime;
	int			frags[MAXPLAYERS];
	int			fragcount;	// [RH] Cumulative frags for this player

} wbplayerstruct_t;

typedef struct wbstartstruct_s
{
	int			finished_ep;
	int			next_ep;

	char		current[8];	// [RH] Name of map just finished
	char		next[8];	// next level, [RH] actual map name

	char		lname0[8];
	char		lname1[8];
	
	int			maxkills;
	int			maxitems;
	int			maxsecret;
	int			maxfrags;

	// the par time and sucktime
	int			partime;	// in tics
	int			sucktime;	// in minutes
	
	// total time for the entire current game
	int			totaltime;

	// index of this player in game
	int			pnum;	

	wbplayerstruct_s	plyr[MAXPLAYERS];
} wbstartstruct_t;

// Called by main loop, animate the intermission.
void WI_Ticker ();

// Called by main loop,
// draws the intermission directly into the screen buffer.
void WI_Drawer ();

// Setup for an intermission screen.
void WI_Start (wbstartstruct_t *wbstartstruct);

#endif
