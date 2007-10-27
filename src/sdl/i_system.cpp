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
//
//-----------------------------------------------------------------------------


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fnmatch.h>
#include <unistd.h>

#include <stdarg.h>
#include <sys/types.h>
#include <sys/time.h>
#ifndef __FreeBSD__
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#endif

#include "networkheaders.h"

#include "doomerrors.h"
#include <math.h>

#include "SDL.h"
#include "doomtype.h"
#include "version.h"
#include "doomdef.h"
#include "cmdlib.h"
#include "m_argv.h"
#include "m_misc.h"
#include "i_video.h"
#include "i_sound.h"
#include "i_music.h"

#include "d_main.h"
#include "d_net.h"
#include "g_game.h"
#include "i_system.h"
#include "c_dispatch.h"
#include "templates.h"

#include "stats.h"
#include "hardware.h"
#include "zstring.h"
#include "gameconfigfile.h"

#include "cl_demo.h"
#include "cl_main.h"

EXTERN_CVAR (String, language)

#ifdef USEASM
extern "C" void STACK_ARGS CheckMMX (CPUInfo *cpu);
#endif

extern "C"
{
	double		SecondsPerCycle = 1e-8;
	double		CyclesPerSecond = 1e8;
	CPUInfo		CPU;
}

extern bool GtkAvailable;

void CalculateCPUSpeed ();

DWORD LanguageIDs[4] =
{
	MAKE_ID ('e','n','u',0),
	MAKE_ID ('e','n','u',0),
	MAKE_ID ('e','n','u',0),
	MAKE_ID ('e','n','u',0)
};
	
int (*I_GetTime) (bool saveMS);
int (*I_WaitForTic) (int);

void I_Tactile (int on, int off, int total)
{
    // UNUSED.
    on = off = total = 0;
}

ticcmd_t emptycmd;
ticcmd_t *I_BaseTiccmd(void)
{
    return &emptycmd;
}

void I_BeginRead(void)
{
}

void I_EndRead(void)
{
}

// [RH] Returns time in milliseconds
unsigned int I_MSTime (void)
{
	return SDL_GetTicks ();
}

static DWORD TicStart;
static DWORD TicNext;

//
// I_GetTime
// returns time in 1/35th second tics
//
int I_GetTimePolled (bool saveMS)
{
	DWORD tm = SDL_GetTicks ();

	if (saveMS)
	{
		TicStart = tm;
		TicNext = Scale ((Scale (tm, TICRATE, 1000) + 1), 1000, TICRATE);
	}
	return Scale (tm, TICRATE, 1000);
}

int I_WaitForTicPolled (int prevtic)
{
    int time;

    while ((time = I_GetTimePolled(false)) <= prevtic)
		;

    return time;
}

// Returns the fractional amount of a tic passed since the most recent tic
fixed_t I_GetTimeFrac (uint32 *ms)
{
	DWORD now = SDL_GetTicks ();
	if (ms) *ms = TicNext;
	DWORD step = TicNext - TicStart;
	if (step == 0)
	{
		return FRACUNIT;
	}
	else
	{
		fixed_t frac = clamp<fixed_t> ((now - TicStart)*FRACUNIT/step, 0, FRACUNIT);
		return frac;
	}
}

void I_WaitVBL (int count)
{
    // I_WaitVBL is never used to actually synchronize to the
    // vertical blank. Instead, it's used for delay purposes.
    usleep (1000000 * count / 70);
}

//
// SetLanguageIDs
//
void SetLanguageIDs ()
{
}

//
// I_Init
//
void I_Init (void)
{
#ifndef USEASM
    memset (&CPU, 0, sizeof(CPU));
#else
	CheckMMX (&CPU);
	CalculateCPUSpeed ();
	
	// Why does Intel right-justify this string?
	char *f = CPU.CPUString, *t = f;
	
	while (*f == ' ')
	{
		++f;
	}
	if (f != t)
	{
		while (*f != 0)
		{
			*t++ = *f++;
		}
		// [BB] We don't copy the terminating zero of f to t, so we still have to terminate t.
		*t = 0;
	}
#endif
	if (CPU.VendorID[0])
	{
		Printf ("CPU Vendor ID: %s\n", CPU.VendorID);
		if (CPU.CPUString[0])
		{
			Printf ("  Name: %s\n", CPU.CPUString);
		}
		if (CPU.bIsAMD)
		{
			Printf ("  Family %d (%d), Model %d, Stepping %d\n",
				CPU.Family, CPU.AMDFamily, CPU.AMDModel, CPU.AMDStepping);
		}
		else
		{
			Printf ("  Family %d, Model %d, Stepping %d\n",
				CPU.Family, CPU.Model, CPU.Stepping);
		}
		Printf ("  Features:");
		if (CPU.bMMX)		Printf (" MMX");
		if (CPU.bMMXPlus)	Printf (" MMX+");
		if (CPU.bSSE)		Printf (" SSE");
		if (CPU.bSSE2)		Printf (" SSE2");
		if (CPU.bSSE3)		Printf (" SSE3");
		if (CPU.b3DNow)		Printf (" 3DNow!");
		if (CPU.b3DNowPlus)	Printf (" 3DNow!+");
		Printf ("\n");
	}

	I_GetTime = I_GetTimePolled;
	I_WaitForTic = I_WaitForTicPolled;
    I_InitSound ();
}

void CalculateCPUSpeed ()
{
	timeval start, stop, now;
	cycle_t ClockCycles;
	DWORD usec;

	if (CPU.bRDTSC)
	{
		ClockCycles = 0;
		clock (ClockCycles);
		gettimeofday (&start, NULL);
	
		// Count cycles for at least 100 milliseconds.
		// We don't have the same accuracy we can get with the Win32
		// performance counters, so we have to time longer.
		stop.tv_usec = start.tv_usec + 100000;
		stop.tv_sec = start.tv_sec;
		if (stop.tv_usec >= 1000000)
		{
			stop.tv_usec -= 1000000;
			stop.tv_sec += 1;
		}
		do
		{
			gettimeofday (&now, NULL);
		} while (timercmp (&now, &stop, <));
	
		unclock (ClockCycles);
		gettimeofday (&now, NULL);
		usec = now.tv_usec - start.tv_usec;

		CyclesPerSecond = (double)ClockCycles * 1e6 / (double)usec;
		SecondsPerCycle = 1.0 / CyclesPerSecond;
	}
	Printf (PRINT_HIGH, "CPU Speed: ~%f MHz\n", CyclesPerSecond / 1e6);
}

//
// I_Quit
//
static int has_exited;

void I_Quit (void)
{
	has_exited = 1;		/* Prevent infinitely recursive exits -- killough */

	if (demorecording)
		G_CheckDemoStatus();
	// [BC] Support for client-side demos.
	if ( CLIENTDEMO_IsRecording( ))
		CLIENTDEMO_FinishRecording( );
	G_ClearSnapshots ();
}


//
// I_Error
//
extern FILE *Logfile;
bool gameisdead;

void STACK_ARGS I_FatalError (const char *error, ...)
{
    static bool alreadyThrown = false;
    gameisdead = true;

    if (!alreadyThrown)		// ignore all but the first message -- killough
    {
		alreadyThrown = true;
		char errortext[MAX_ERRORTEXT];
		int index;
		va_list argptr;
		va_start (argptr, error);
		index = vsprintf (errortext, error, argptr);
		va_end (argptr);

		// Record error to log (if logging)
		if (Logfile)
			fprintf (Logfile, "\n**** DIED WITH FATAL ERROR:\n%s\n", errortext);

		// [BB] Tell the server we're leaving the game.
		if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
			CLIENT_QuitNetworkGame( NULL );

//		throw CFatalError (errortext);
		fprintf (stderr, "%s\n", errortext);
		exit (-1);
    }

    if (!has_exited)	// If it hasn't exited yet, exit now -- killough
    {
		has_exited = 1;	// Prevent infinitely recursive exits -- killough
		exit(-1);
    }
}

void STACK_ARGS I_Error (const char *error, ...)
{
	va_list argptr;
	char errortext[MAX_ERRORTEXT];

	va_start (argptr, error);
	vsprintf (errortext, error, argptr);
	va_end (argptr);

	// [BB] Tell the server we're leaving the game.
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
		CLIENT_QuitNetworkGame( NULL );

	throw CRecoverableError (errortext);
}

void I_SetIWADInfo (const IWADInfo *info)
{
}

void I_PrintStr (const char *cp)
{
	fputs (cp, stdout);
	fflush (stdout);
}

#ifndef __FreeBSD__
// GtkTreeViews eats return keys. I want this to be like a Windows listbox
// where pressing Return can still activate the default button.
gint AllowDefault(GtkWidget *widget, GdkEventKey *event, gpointer func_data)
{
	if (event->type == GDK_KEY_PRESS && event->keyval == GDK_Return)
	{
		gtk_window_activate_default (GTK_WINDOW(func_data));
	}
	return FALSE;
}

// Double-clicking an entry in the list is the same as pressing OK.
gint DoubleClickChecker(GtkWidget *widget, GdkEventButton *event, gpointer func_data)
{
	if (event->type == GDK_2BUTTON_PRESS)
	{
		*(int *)func_data = 1;
		gtk_main_quit();
	}
	return FALSE;
}

// When the user presses escape, that should be the same as canceling the dialog.
gint CheckEscape (GtkWidget *widget, GdkEventKey *event, gpointer func_data)
{
	if (event->type == GDK_KEY_PRESS && event->keyval == GDK_Escape)
	{
		gtk_main_quit();
	}
	return FALSE;
}

void ClickedOK(GtkButton *button, gpointer func_data)
{
	*(int *)func_data = 1;
	gtk_main_quit();
}

EXTERN_CVAR (Bool, queryiwad);

int I_PickIWad_Gtk (WadStuff *wads, int numwads, bool showwin, int defaultiwad)
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *bbox;
	GtkWidget *widget;
	GtkWidget *tree;
	GtkWidget *check;
	GtkListStore *store;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkTreeSelection *selection;
	GtkTreeIter iter, defiter;
	int close_style = 0;
	int i;

	// Create the dialog window.
	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title (GTK_WINDOW(window), GAMESIG " " DOTVERSIONSTR ": Select an IWAD to use");
	gtk_window_set_position (GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	gtk_container_set_border_width (GTK_CONTAINER(window), 10);
	g_signal_connect (window, "delete_event", G_CALLBACK(gtk_main_quit), NULL);
	g_signal_connect (window, "key_press_event", G_CALLBACK(CheckEscape), NULL);

	// Create the vbox container.
	vbox = gtk_vbox_new (FALSE, 10);
	gtk_container_add (GTK_CONTAINER(window), vbox);

	// Create the top label.
	widget = gtk_label_new ("ZDoom found more than one IWAD\nSelect from the list below to determine which one to use:");
	gtk_box_pack_start (GTK_BOX(vbox), widget, false, false, 0);
	gtk_misc_set_alignment (GTK_MISC(widget), 0, 0);

	// Create a list store with all the found IWADs.
	store = gtk_list_store_new (3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT);
	for (i = 0; i < numwads; ++i)
	{
		const char *filepart = strrchr (wads[i].Path, '/');
		if (filepart == NULL)
			filepart = wads[i].Path;
		else
			filepart++;
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,
			0, filepart,
			1, IWADInfos[wads[i].Type].Name,
			2, i,
			-1);
		if (i == defaultiwad)
		{
			defiter = iter;
		}
	}

	// Create the tree view control to show the list.
	tree = gtk_tree_view_new_with_model (GTK_TREE_MODEL(store));
	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes ("IWAD", renderer, "text", 0, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW(tree), column);
	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes ("Game", renderer, "text", 1, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW(tree), column);
	gtk_box_pack_start (GTK_BOX(vbox), GTK_WIDGET(tree), true, true, 0);
	g_signal_connect(G_OBJECT(tree), "button_press_event", G_CALLBACK(DoubleClickChecker), &close_style);
	g_signal_connect(G_OBJECT(tree), "key_press_event", G_CALLBACK(AllowDefault), window);

	// Select the default IWAD.
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW(tree));
	gtk_tree_selection_select_iter (selection, &defiter);

	// Create the hbox for the bottom row.
	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_end (GTK_BOX(vbox), hbox, false, false, 0);

	// Create the "Don't ask" checkbox.
	check = gtk_check_button_new_with_label ("Don't ask me this again");
	gtk_box_pack_start (GTK_BOX(hbox), check, false, false, 0);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(check), !showwin);

	// Create the OK/Cancel button box.
	bbox = gtk_hbutton_box_new ();
	gtk_button_box_set_layout (GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);
	gtk_box_set_spacing (GTK_BOX(bbox), 10);
	gtk_box_pack_end (GTK_BOX(hbox), bbox, false, false, 0);

	// Create the OK button.
	widget = gtk_button_new_from_stock (GTK_STOCK_OK);
	gtk_box_pack_start (GTK_BOX(bbox), widget, false, false, 0);
	GTK_WIDGET_SET_FLAGS (widget, GTK_CAN_DEFAULT);
	gtk_widget_grab_default (widget);
	g_signal_connect (widget, "clicked", G_CALLBACK(ClickedOK), &close_style);
	g_signal_connect (widget, "activate", G_CALLBACK(ClickedOK), &close_style);

	// Create the cancel button.
	widget = gtk_button_new_from_stock (GTK_STOCK_CANCEL);
	gtk_box_pack_start (GTK_BOX(bbox), widget, false, false, 0);
	g_signal_connect (widget, "clicked", G_CALLBACK(gtk_main_quit), &window);

	// Finally we can show everything.
	gtk_widget_show_all (window);

	gtk_main ();

	if (close_style == 1)
	{
		GtkTreeModel *model;
		GValue value = { 0, };
		
		// Find out which IWAD was selected.
		gtk_tree_selection_get_selected (selection, &model, &iter);
		gtk_tree_model_get_value (GTK_TREE_MODEL(model), &iter, 2, &value);
		i = g_value_get_int (&value);
		g_value_unset (&value);
		
		// Set state of queryiwad based on the checkbox.
		queryiwad = !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(check));
	}
	else
	{
		i = -1;
	}
	
	if (GTK_IS_WINDOW(window))
	{
		gtk_widget_destroy (window);
		// If we don't do this, then the X window might not actually disappear.
		while (g_main_context_iteration (NULL, FALSE)) {}
	}

	return i;
}
#endif

int I_PickIWad (WadStuff *wads, int numwads, bool showwin, int defaultiwad)
{
	int i;

	if (!showwin)
	{
		return defaultiwad;
	}
	
#ifndef __FreeBSD__
	if (GtkAvailable)
	{
		return I_PickIWad_Gtk (wads, numwads, showwin, defaultiwad);
	}
#endif
	
	printf ("Please select a game wad (or 0 to exit):\n");
	for (i = 0; i < numwads; ++i)
	{
		const char *filepart = strrchr (wads[i].Path, '/');
		if (filepart == NULL)
			filepart = wads[i].Path;
		else
			filepart++;
		printf ("%d. %s (%s)\n", i+1, IWADInfos[wads[i].Type].Name, filepart);
	}
	printf ("Which one? ");
	scanf ("%d", &i);
	if (i > numwads)
		return -1;
	return i-1;
}

bool I_WriteIniFailed ()
{
	printf ("The config file %s could not be saved:\n%s\n", GameConfig->GetPathName(), strerror(errno));
	return false;
	// return true to retry
}

static const char *pattern;

#ifdef __FreeBSD__
static int matchfile (struct dirent *ent)
#else
#ifdef OSF1
static int matchfile (struct dirent *ent)
#else
static int matchfile (const struct dirent *ent)
#endif
#endif
{
    return fnmatch (pattern, ent->d_name, FNM_NOESCAPE) == 0;
}

void *I_FindFirst (const char *filespec, findstate_t *fileinfo)
{
	FString dir;
	
	char *slash = strrchr (filespec, '/');
	if (slash)
	{
		pattern = slash+1;
		dir = FString(filespec, slash-filespec+1);
	}
	else
	{
		pattern = filespec;
		dir = ".";
	}

    fileinfo->current = 0;
    fileinfo->count = scandir (dir.GetChars(), &fileinfo->namelist,
							   matchfile, alphasort);
    if (fileinfo->count > 0)
    {
		return fileinfo;
    }
    return (void*)-1;
}

int I_FindNext (void *handle, findstate_t *fileinfo)
{
    findstate_t *state = (findstate_t *)handle;
    if (state->current < fileinfo->count)
    {
	    return ++state->current < fileinfo->count ? 0 : -1;
	}
	return -1;
}

int I_FindClose (void *handle)
{
	findstate_t *state = (findstate_t *)handle;
	if (handle != (void*)-1 && state->count > 0)
	{
		state->count = 0;
		free (state->namelist);
		state->namelist = NULL;
	}
	return 0;
}

int I_FindAttr (findstate_t *fileinfo)
{
	struct dirent *ent = fileinfo->namelist[fileinfo->current];

#ifdef OSF1
	return 0;	// I don't know how to detect dirs under OSF/1
#else
    return (ent->d_type == DT_DIR) ? FA_DIREC : 0;
#endif
}

// No clipboard support for Linux
void I_PutInClipboard (const char *str)
{
}

char *I_GetFromClipboard ()
{
	return NULL;
}
