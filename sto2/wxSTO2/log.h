//-----------------------------------------------------------------------------
//
// Skulltag Online 2
// Copyright (C) 2007 Rivecoder
// Date created: 8/28/07
//
// Program-wide logging module.
//-----------------------------------------------------------------------------

#ifndef __LOG_H__
#define __LOG_H__

#define WIN32_LEAN_AND_MEAN
#include <wx/wxprec.h>
#include "../../src/networkheaders.h"
#include "../../src/networkshared.h"
#ifndef WX_PRECOMP
#	include <wx/wx.h>
#endif

//*****************************************************************************
//	DEFINES
#define MAX_LOGENTRY	1024
enum
{
	// 
	LOGERROR_MISC,
	LOGERROR_NETWORK,
	LOGERROR_HUFFMAN,

};

void LOG_Add( const char *entry, ... );

#endif