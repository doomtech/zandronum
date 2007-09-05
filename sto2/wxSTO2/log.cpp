//-----------------------------------------------------------------------------
//
// Skulltag Online 2
// Copyright (C) 2007 Rivecoder
// Date created: 8/28/07
//
// Program-wide logging module.
//-----------------------------------------------------------------------------


#include "log.h"

void LOG_Add( const char *entry, ... )
{
	char entrytext[MAX_LOGENTRY];
	va_list argptr;
	va_start (argptr, entry);
	int index = vsprintf (entrytext, entry, argptr);
	va_end (argptr);

	// Remove linebreaks from log messages - wxLog adds them for us.
	if( (entrytext[strlen(entrytext) - 1] == '\n') || 
		(entrytext[strlen(entrytext) - 1] == '\r') )
			entrytext[strlen(entrytext) - 1] = 0;

	wxLogMessage( entrytext );
}