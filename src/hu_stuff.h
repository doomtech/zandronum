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
// DESCRIPTION:  Head up display
//
//-----------------------------------------------------------------------------

#ifndef __HU_STUFF_H__
#define __HU_STUFF_H__

struct event_t;
class player_t;

//
// Globally visible constants.
//
#define HU_FONTSTART	BYTE('!')		// the first font characters
#define HU_FONTEND		BYTE('�')		// the last font characters

// Calculate # of glyphs in font.
#define HU_FONTSIZE		(HU_FONTEND - HU_FONTSTART + 1)
/*
//
// Chat routines
//

// [RH] Draw deathmatch scores

void HU_DrawScores (player_t *me);
*/
#endif
