// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id: i_main.c,v 1.8 1998/05/15 00:34:03 killough Exp $
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
//
// DESCRIPTION:
//      Main program, simply calls D_DoomMain high level loop.
//
//-----------------------------------------------------------------------------

#include <SDL.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <new>
#include <sys/param.h>

#include "doomerrors.h"
#include "m_argv.h"
#include "d_main.h"
#include "i_system.h"
#include "i_video.h"
#include "c_console.h"
#include "errors.h"
#include "version.h"
#include "w_wad.h"

DArgs Args;

#define MAX_TERMS	32
void (*TermFuncs[MAX_TERMS]) ();
const char *TermNames[MAX_TERMS];
static int NumTerms;

void addterm (void (*func) (), const char *name)
{
	// Make sure this function wasn't already registered.
	for (int i = 0; i < NumTerms; ++i)
	{
		if (TermFuncs[i] == func)
		{
			return;
		}
	}
    if (NumTerms == MAX_TERMS)
	{
		func ();
		I_FatalError (
			"Too many exit functions registered.\n"
			"Increase MAX_TERMS in i_main.cpp");
	}
	TermNames[NumTerms] = name;
    TermFuncs[NumTerms++] = func;
}

void popterm ()
{
	if (NumTerms)
		NumTerms--;
}

void STACK_ARGS call_terms ()
{
    while (NumTerms > 0)
	{
//		printf ("term %d - %s\n", NumTerms, TermNames[NumTerms-1]);
		TermFuncs[--NumTerms] ();
	}
}

static void STACK_ARGS NewFailure ()
{
    I_FatalError ("Failed to allocate memory from system heap");
}

extern "C" int cc_install_handlers(int, int*, const char*, int(*)(char*, char*));
static int DoomSpecificInfo (char *buffer, char *end)
{
	const char *arg;
	int size = end-buffer;
	int i, p;

	SDL_Quit();

	p = 0;
	p += snprintf (buffer+p, size-p, GAMENAME" version " DOTVERSIONSTR " (" __DATE__ ")\n");
	p += snprintf (buffer+p, size-p, "\nCommand line:");
	for (i = 0; i < Args.NumArgs(); ++i)
	{
		p += snprintf (buffer+p, size-p, " %s", Args.GetArg(i));
	}
	p += snprintf (buffer+p, size-p, "\n");
	
	for (i = 0; (arg = Wads.GetWadName (i)) != NULL; ++i)
	{
		p += snprintf (buffer+p, size-p, "\nWad %d: %s", i, arg);
	}

	if (gamestate != GS_LEVEL && gamestate != GS_TITLELEVEL)
	{
		p += snprintf (buffer+p, size-p, "\n\nNot in a level.");
	}
	else
	{
		char name[9];

		strncpy (name, level.mapname, 8);
		name[8] = 0;
		p += snprintf (buffer+p, size-p, "\n\nCurrent map: %s", name);

		if (!viewactive)
		{
			buffer += snprintf (buffer+p, size-p, "\n\nView not active.");
		}
		else
		{
			p += snprintf (buffer+p, size-p, "\n\nviewx = %d", (int)viewx);
			p += snprintf (buffer+p, size-p, "\nviewy = %d", (int)viewy);
			p += snprintf (buffer+p, size-p, "\nviewz = %d", (int)viewz);
			p += snprintf (buffer+p, size-p, "\nviewangle = %x", (unsigned int)viewangle);
		}
	}
	buffer[p++] = '\n';
	buffer[p++] = '\0';

	return p;
}

int main (int argc, char **argv)
{
	printf(GAMENAME" v%s - SVN revision %s - SDL version\nCompiled on %s\n\n",
		DOTVERSIONSTR_NOREV,SVN_REVISION_STRING,__DATE__);

	{
		int s[4] = { SIGSEGV, SIGILL, SIGFPE, SIGBUS };
		cc_install_handlers(4, s, GAMENAMELOWERCASE"-crash.log", DoomSpecificInfo);
	}

	seteuid (getuid ());
    std::set_new_handler (NewFailure);

	Args.SetArgs (argc, argv);

	if ( Args.CheckParm( "-host" ))
	{
		if (SDL_Init (SDL_INIT_TIMER|SDL_INIT_NOPARACHUTE) == -1)
		{
			fprintf (stderr, "Could not initialize SDL:\n%s\n", SDL_GetError());
			return -1;
		}
	}
	else
	{
		if (SDL_Init (SDL_INIT_VIDEO|SDL_INIT_TIMER|SDL_INIT_NOPARACHUTE) == -1)
		{
			fprintf (stderr, "Could not initialize SDL:\n%s\n", SDL_GetError());
			return -1;
		}
	}

	atterm (SDL_Quit);

	SDL_WM_SetCaption (GAMESIG " " DOTVERSIONSTR " (" __DATE__ ")", NULL);
	
    try
    {

		/*
		  killough 1/98:

		  This fixes some problems with exit handling
		  during abnormal situations.

		  The old code called I_Quit() to end program,
		  while now I_Quit() is installed as an exit
		  handler and exit() is called to exit, either
		  normally or abnormally. Seg faults are caught
		  and the error handler is used, to prevent
		  being left in graphics mode or having very
		  loud SFX noise because the sound card is
		  left in an unstable state.
		*/

		atexit (call_terms);
		atterm (I_Quit);

		if (realpath (argv[0], progdir) == NULL)
			strcpy (progdir, argv[0]);
		char *slash = strrchr (progdir, '/');
		if (slash)
			*(slash + 1) = '\0';
		else
			progdir[0] = '.', progdir[1] = '/', progdir[2] = '\0';

		C_InitConsole (80*8, 25*8, false);
		D_DoomMain ();
    }
    catch (class CDoomError &error)
    {
		if (error.GetMessage ())
			fprintf (stderr, "%s\n", error.GetMessage ());
		exit (-1);
    }
    catch (...)
    {
		call_terms ();
		throw;
    }
    return 0;
}
