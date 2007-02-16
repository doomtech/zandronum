// Emacs style mode select   -*- C++ -*- 
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


#ifndef __I_VIDEO_H__
#define __I_VIDEO_H__

#include "doomtype.h"
#include "v_video.h"


// [RH] Set the display mode
DFrameBuffer *I_SetMode (int &width, int &height, DFrameBuffer *old);

// Pause a bit.
// [RH] Despite the name, it apparently never waited for the VBL, even in
// the original DOS version (if the Heretic/Hexen source is any indicator).
void I_WaitVBL(int count);

bool I_CheckResolution (int width, int height, int bpp);
void I_ClosestResolution (int *width, int *height, int bits);

void I_StartModeIterator (int bits);
bool I_NextMode (int *width, int *height, bool *fullscreen);

DCanvas *I_NewStaticCanvas (int width, int height);

enum EDisplayType
{
	DISPLAY_WindowOnly,
	DISPLAY_FullscreenOnly,
	DISPLAY_Both
};

EDisplayType I_DisplayType ();

#endif // __I_VIDEO_H__
