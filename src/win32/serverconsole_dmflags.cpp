//------------------------------------------------------------------------------------------------------------------------------------------------------
//
// Skulltag Source
// Copyright (C) 2009 Rivecoder
// Copyright (C) 2007-2012 Skulltag Development Team
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
// 3. Neither the name of the Skulltag Development Team nor the names of its
//    contributors may be used to endorse or promote products derived from this
//    software without specific prior written permission.
// 4. Redistributions in any form must be accompanied by information on how to
//    obtain complete source code for the software and any accompanying
//    software that uses the software. The source code must either be included
//    in the distribution or be available for no more than the cost of
//    distribution plus a nominal fee, and must be freely redistributable
//    under reasonable conditions. For an executable file, complete source
//    code means the source code for all modules it contains. It does not
//    include source code for modules or files that typically accompany the
//    major components of the operating system on which the executable file
//    runs.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// Date created: 9/18/09
//
//
// Filename: serverconsole_dmflags.ccpp
//
// Description: Win32 code for the server's "DMFlags" dialog.
//
//----------------------------------------------------------------------------------------------------------------------------------------------------

#include <windows.h>
#include <commctrl.h>
#include <uxtheme.h>
#define USE_WINDOWS_DWORD
#include "i_system.h"
#include "network.h"
#include "callvote.h"
#include "v_text.h"
#include "m_menu.h"
#include "c_dispatch.h"
#include "cmdlib.h"
#include "cooperative.h"
#include "deathmatch.h"
#include "duel.h"
#include "gamemode.h"
#include "lastmanstanding.h"
#include "maprotation.h"
#include "network.h"
#include "resource.h"
#include "dmflags/resource.h"
#include "serverconsole.h"
#include "serverconsole_dmflags.h"
#include "sv_ban.h"
#include "sv_main.h"
#include "team.h"
#include "v_text.h"
#include "version.h"
#include "network.h"
#include "survival.h"
#ifdef	GUI_SERVER_CONSOLE

//--------------------------------------------------------------------------------------------------------------------------------------------------
//-- EXTERNALS -------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------

// Global instance of Skulltag.
extern	HINSTANCE		g_hInst;

// References to the names defined in m_options.cpp (to reduce code duplication).
extern	value_t			DF_Crouch[3];
extern	value_t			DF_Jump[3];

//--------------------------------------------------------------------------------------------------------------------------------------------------
//-- VARIABLES -------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------

// References to the dialog.
static	HWND			g_hDlg = NULL;

// Stored values of cvars (only send updates them if they change!)
static	ULONG			g_ulCompatFlags;
static	ULONG			g_ulCompatFlags2;
static	ULONG			g_ulDMFlags;
static	ULONG			g_ulDMFlags2;

//==================================================================================
// [RC] This big map of fun ties all of the DMFlags to their respective checkboxes.
//==================================================================================
#define NUMBER_OF_FLAGS 81
static	FLAGMAPPING_t	g_Flags[NUMBER_OF_FLAGS] = 
{
//	{ DF_NO_ITEMS,						IDC_NO_ITEMS,					&g_ulDMFlags, },
	{ DF_NO_HEALTH,						IDC_NO_HEALTH1,					&g_ulDMFlags, },	
	{ DF_WEAPONS_STAY,					IDC_WEAPONS_STAY,				&g_ulDMFlags, },		
	{ DF_SPAWN_FARTHEST,				IDC_SPAWN_FARTHEST,				&g_ulDMFlags, },	
	{ DF_FORCE_RESPAWN,					IDC_FORCE_RESPAWN,				&g_ulDMFlags, },	
	{ DF_NO_ARMOR,						IDC_NO_ARMOR,					&g_ulDMFlags, },
	{ DF_INFINITE_AMMO,					IDC_INFINITE_AMMO,				&g_ulDMFlags, },
	{ DF_NO_MONSTERS,					IDC_NO_MONSTERS,				&g_ulDMFlags, },
	{ DF_MONSTERS_RESPAWN,				IDC_MONSTERS_RESPAWN,			&g_ulDMFlags, },
	{ DF_ITEMS_RESPAWN,					IDC_ITEMS_RESPAWN,				&g_ulDMFlags, },
	{ DF_FAST_MONSTERS,					IDC_FAST_MONSTERS,				&g_ulDMFlags, },	
	{ DF_NO_FREELOOK,					IDC_NO_FREELOOK,				&g_ulDMFlags, },
	{ DF_RESPAWN_SUPER,					IDC_RESPAWN_SUPER,				&g_ulDMFlags, },
	{ DF_NO_FOV,						IDC_NO_FOV,						&g_ulDMFlags, },
	{ DF_NO_COOP_WEAPON_SPAWN,			IDC_NO_COOP_MP_WEAPON_SPAWN,	&g_ulDMFlags, },
	{ DF_COOP_LOSE_INVENTORY,			IDC_COOP_LOSE_INVENTORY,		&g_ulDMFlags, },
	{ DF_COOP_LOSE_KEYS,				IDC_LOSE_KEYS,					&g_ulDMFlags, },
	{ DF_COOP_LOSE_WEAPONS,				IDC_LOSE_WEAPONS,				&g_ulDMFlags, },
	{ DF_COOP_LOSE_ARMOR,				IDC_COOP_LOSE_ARMOR,			&g_ulDMFlags, },
	{ DF_COOP_LOSE_POWERUPS,			IDC_COOP_LOSE_POWERUPS,			&g_ulDMFlags, },
	{ DF_COOP_LOSE_AMMO,				IDC_COOP_LOSE_AMMO,				&g_ulDMFlags, },
	{ DF_COOP_HALVE_AMMO,				IDC_COOP_HALVE_AMMO,			&g_ulDMFlags, },
	{ DF2_YES_WEAPONDROP,				IDC_YES_WEAPONDROP,				&g_ulDMFlags2, },
	{ DF2_NO_RUNES,						IDC_NO_RUNES,					&g_ulDMFlags2, },
	{ DF2_INSTANT_RETURN,				IDC_INSTANT_RETURN,				&g_ulDMFlags2, },
	{ DF2_NO_TEAM_SWITCH,				IDC_NO_TEAM_SWITCH,				&g_ulDMFlags2, },
	{ DF2_NO_TEAM_SELECT,				IDC_NO_TEAM_SELECT,				&g_ulDMFlags2, },
	{ DF2_YES_DOUBLEAMMO,				IDC_YES_DOUBLEAMMO,				&g_ulDMFlags2, },
	{ DF2_YES_DEGENERATION,				IDC_YES_DEGENERATION,			&g_ulDMFlags2, },
	{ DF2_YES_FREEAIMBFG,				IDC_YES_FREEAIMBFG,				&g_ulDMFlags2, },
	{ DF2_BARRELS_RESPAWN,				IDC_BARRELS_RESPAWN,			&g_ulDMFlags2, },
	{ DF2_NO_RESPAWN_INVUL,				IDC_NO_RESPAWN_INVUL,			&g_ulDMFlags2, },
	{ DF2_COOP_SHOTGUNSTART,			IDC_SHOTGUN_START,				&g_ulDMFlags2, },
	{ DF2_SAME_SPAWN_SPOT,				IDC_SAME_SPAWN_SPOT,			&g_ulDMFlags2, },
	{ DF2_YES_KEEP_TEAMS,				IDC_DF2_YES_KEEP_TEAMS,			&g_ulDMFlags2, },
	{ DF2_YES_KEEPFRAGS,				IDC_DF2_YES_KEEP_FRAGS,			&g_ulDMFlags2, },
	{ DF2_NO_RESPAWN,					IDC_DF2_NO_RESPAWN,				&g_ulDMFlags2, },
	{ DF2_YES_LOSEFRAG,					IDC_DF2_YES_LOSEFRAG,			&g_ulDMFlags2, },
	{ DF2_INFINITE_INVENTORY,			IDC_DF2_INFINITE_INVENTORY,		&g_ulDMFlags2, },
	{ DF2_KILL_MONSTERS,				IDC_DF2_KILL_MONSTERS,			&g_ulDMFlags2, },
	{ DF2_NO_AUTOMAP,					IDC_DF2_NO_AUTOMAP,				&g_ulDMFlags2, },
	{ DF2_NO_AUTOMAP_ALLIES,			IDC_DF2_NO_AUTOMAP_ALLIES,		&g_ulDMFlags2, },
	{ DF2_DISALLOW_SPYING,				IDC_DF2_DISALLOW_SPYING,		&g_ulDMFlags2, },
	{ DF2_CHASECAM,						IDC_DF2_CHASECAM,				&g_ulDMFlags2, },
	{ DF2_NOSUICIDE,					IDC_DF2_NOSUICIDE,				&g_ulDMFlags2, },
	{ DF2_NOAUTOAIM,					IDC_DF2_NOAUTOAIM,				&g_ulDMFlags2, },
	{ DF2_FORCE_GL_DEFAULTS,			IDC_DF2_FORCE_GL_DEFAULTS,		&g_ulDMFlags2, },
	{ DF2_NO_ROCKET_JUMPING,			IDC_DF2_NO_ROCKET_JUMPING,		&g_ulDMFlags2, },
	{ DF2_AWARD_DAMAGE_INSTEAD_KILLS,	IDC_DF2_AWARD_DAMAGE_INSTEAD_KILLS,	&g_ulDMFlags2, },
	{ DF2_FORCE_ALPHA,					IDC_DF2_FORCE_ALPHA,			&g_ulDMFlags2, },
	{ COMPATF_SHORTTEX,					IDC_SHORTTEX,					&g_ulCompatFlags, },
	{ COMPATF_STAIRINDEX,				IDC_STAIRINDEX,					&g_ulCompatFlags, },
	{ COMPATF_LIMITPAIN,				IDC_LIMITPAIN,					&g_ulCompatFlags, },
	{ COMPATF_SILENTPICKUP,				IDC_SILENTPICKUP,				&g_ulCompatFlags, },	
	{ COMPATF_NO_PASSMOBJ,				IDC_NO_PASSMOBJ,				&g_ulCompatFlags, },
	{ COMPATF_MAGICSILENCE,				IDC_MAGICSILENCE,				&g_ulCompatFlags, },
	{ COMPATF_WALLRUN,					IDC_WALLRUN,					&g_ulCompatFlags, },
	{ COMPATF_NOTOSSDROPS,				IDC_NOTOSSDROPS,				&g_ulCompatFlags, },
	{ COMPATF_USEBLOCKING,				IDC_USEBLOCKING,				&g_ulCompatFlags, },
	{ COMPATF_NODOORLIGHT,				IDC_NODOORLIGHT,				&g_ulCompatFlags, },
	{ COMPATF_RAVENSCROLL,				IDC_COMPATF_RAVENSCROLL,		&g_ulCompatFlags, },
	{ COMPATF_SOUNDTARGET,				IDC_COMPATF_SOUNDTARGET,		&g_ulCompatFlags, },
	{ COMPATF_DEHHEALTH,				IDC_COMPATF_DEHHEALTH,			&g_ulCompatFlags, },
	{ COMPATF_TRACE,					IDC_COMPATF_TRACE,				&g_ulCompatFlags, },
	{ COMPATF_DROPOFF,					IDC_COMPATF_DROPOFF,			&g_ulCompatFlags, },
	{ COMPATF_BOOMSCROLL,				IDC_COMPATF_BOOMSCROLL,			&g_ulCompatFlags, },
	{ COMPATF_INVISIBILITY,				IDC_COMPATF_INVISIBILITY,		&g_ulCompatFlags, },
	{ COMPATF_SILENT_INSTANT_FLOORS,	IDC_COMPATF_SILENT_INSTANT_FLOORS,		&g_ulCompatFlags, },
	{ COMPATF_SECTORSOUNDS,				IDC_COMPATF_SECTORSOUNDS,		&g_ulCompatFlags, },
	{ COMPATF_MISSILECLIP,				IDC_COMPATF_MISSILECLIP,		&g_ulCompatFlags, },
	{ COMPATF_CROSSDROPOFF,				IDC_COMPATF_CROSSDROPOFF,		&g_ulCompatFlags, },
	{ COMPATF_LIMITED_AIRMOVEMENT,		IDC_LIMITED_AIRMOVEMENT,		&g_ulCompatFlags, },
	{ COMPATF_PLASMA_BUMP_BUG,			IDC_PLASMA_BUMP_BUG,			&g_ulCompatFlags, },
	{ COMPATF_INSTANTRESPAWN,			IDC_INSTANTRESPAWN,				&g_ulCompatFlags, },
	{ COMPATF_DISABLETAUNTS,			IDC_DISABLETAUNTS,				&g_ulCompatFlags, },
	{ COMPATF_ORIGINALSOUNDCURVE,		IDC_ORIGINALSOUNDCURVE,			&g_ulCompatFlags, },
	{ COMPATF_OLDINTERMISSION,			IDC_OLDINTERMISSION,			&g_ulCompatFlags, },
	{ COMPATF_DISABLESTEALTHMONSTERS,	IDC_DISABLESTEALTHMONSTERS,		&g_ulCompatFlags, },	
	{ COMPATF_OLDRADIUSDMG,				IDC_COMPATF_OLDRADIUSDMG,		&g_ulCompatFlags, },
	{ COMPATF_NO_CROSSHAIR,				IDC_COMPATF_NO_CROSSHAIR,		&g_ulCompatFlags, },
	{ COMPATF_OLD_WEAPON_SWITCH,		IDC_COMPATF_OLD_WEAPON_SWITCH,	&g_ulCompatFlags, },
	{ COMPATF2_NETSCRIPTS_ARE_CLIENTSIDE,IDC_COMPATF2_NETSCRIPTS_ARE_CLIENTSIDE, &g_ulCompatFlags2, },
};

//================================================
// [RC] And here we define all of the drop downs.
//================================================

//*****************************************************************************
// [RC] We're using the m_menu.h definition because we're going to re-use all that code. Falling damage just isn't defined there yet.
value_t DF_FallingDamage[4] = {
	{ 0, "None" },
	{ DF_FORCE_FALLINGZD, "Old (ZDoom)" },
	{ DF_FORCE_FALLINGHX, "Hexen" },
	{ DF_FORCE_FALLINGST, "Strife" }
};

//*****************************************************************************
//
value_t DF_LevelExit[3] = {
	{ 0, "Continue to the next map" },
	{ DF_SAME_LEVEL, "Restart the current level" },
	{ DF_NO_EXIT, "Kill the player" },
};

//*****************************************************************************
//
#define NUMBER_OF_DROPDOWNS 4
static MULTIFLAG_t g_MultiFlags[NUMBER_OF_DROPDOWNS] = 
{
	{ DF_FallingDamage, 4, IDC_FALLINGDAMAGE, &g_ulDMFlags, },
	{ DF_Crouch, 3, IDC_DMF_CROUCHING, &g_ulDMFlags, },
	{ DF_Jump, 3, IDC_DMF_JUMPING, &g_ulDMFlags, },
	{ DF_LevelExit, 3, IDC_DMF_EXIT1, &g_ulDMFlags, },

};

//--------------------------------------------------------------------------------------------------------------------------------------------------
//-- PROTOTYPES ------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------

// (Uxtheme) Styles tabs in Windows XP and up. 
static	HRESULT			(__stdcall *pEnableThemeDialogTexture)( HWND hwnd, DWORD dwFlags );

static	void			flags_UpdateFlagsFromCheckboxes( void );
static	void			flags_UpdateValueLabels( void );
static	void			flags_UpdateCheckboxesFromFlags( void );
static	void			flags_ReadNewValue( HWND hDlg, int iControlID, ULONG &ulFlags );
static	BOOL			flags_HandleTabSwitch( HWND hDlg, LPNMHDR nmhdr );
static	void			flags_InsertTab( char *pszTitle, int iResource, HWND hDlg, TCITEM tcitem, HWND edit, RECT tabrect, RECT tcrect, int &index );
BOOL	CALLBACK		flags_GenericTabCallback( HWND hDlg, UINT Message, WPARAM wParam, LPARAM lParam );

//--------------------------------------------------------------------------------------------------------------------------------------------------
//-- FUNCTIONS -------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------

//==========================================================================
//
// DMFlagsCallback
//
// The callback for the main dialog itself.
//
//==========================================================================

BOOL CALLBACK SERVERCONSOLE_DMFlagsCallback( HWND hDlg, UINT Message, WPARAM wParam, LPARAM lParam )
{
	switch ( Message )
	{
	case WM_CLOSE:

		EndDialog( hDlg, -1 );
		break;
	case WM_INITDIALOG:
		{
			// Enable tab gradients on XP and later.
			HMODULE uxtheme = LoadLibrary ("uxtheme.dll");
			if ( uxtheme != NULL )
				pEnableThemeDialogTexture = (HRESULT (__stdcall *)(HWND,DWORD))GetProcAddress (uxtheme, "EnableThemeDialogTexture");

			g_hDlg = hDlg;
			g_ulDMFlags = dmflags;
			g_ulDMFlags2 = dmflags2;
			g_ulCompatFlags = compatflags;

			SendDlgItemMessage( hDlg, IDC_DMFLAGS_VALUE, EM_SETLIMITTEXT, 12, 0 );
			SendDlgItemMessage( hDlg, IDC_DMFLAGS2_VALUE, EM_SETLIMITTEXT, 12, 0 );
			SendDlgItemMessage( hDlg, IDC_COMPATFLAGS_VALUE, EM_SETLIMITTEXT, 12, 0 );
			SendDlgItemMessage( hDlg, IDC_COMPATFLAGS2_VALUE, EM_SETLIMITTEXT, 12, 0 );
			SendMessage( GetDlgItem( hDlg, IDC_TITLE ), WM_SETFONT, (WPARAM) CreateFont( 24, 0, 0, 0, 900, 0, 0, 0, 0, 0, 0, 0, 0, "Tahoma" ), (LPARAM) 1);

			// Set up the tab control.
			TCITEM		tcitem;
			tcitem.mask = TCIF_TEXT | TCIF_PARAM;
			HWND edit = GetDlgItem( hDlg, IDC_FLAGSTABCONTROL );
			RECT		tabrect, tcrect;
			GetWindowRect( edit, &tcrect );
			ScreenToClient( hDlg, (LPPOINT) &tcrect.left );
			ScreenToClient( hDlg, (LPPOINT) &tcrect.right );
			TabCtrl_GetItemRect( edit, 0, &tabrect );

			// Create the tabs.		
			int index = 0;
			flags_InsertTab( "General", IDD_DMFLAGS_GENERAL, hDlg, tcitem, edit, tabrect, tcrect, index );
			flags_InsertTab( "Players", IDD_DMFLAGS_PLAYERS, hDlg, tcitem, edit, tabrect, tcrect, index );
			flags_InsertTab( "Cooperative", IDD_DMFLAGS_COOP, hDlg, tcitem, edit, tabrect, tcrect, index );
			flags_InsertTab( "Deathmatch", IDD_DMFLAGS_DM, hDlg, tcitem, edit, tabrect, tcrect, index );			
			flags_InsertTab( "Compatibility", IDD_DMFLAGS_COMPAT, hDlg, tcitem, edit, tabrect, tcrect, index );
		}
		break;
	case WM_COMMAND:

		// User is typing in some new flags.
		if ( HIWORD ( wParam ) == EN_CHANGE )
		{
			switch ( LOWORD( wParam ))
			{
			case IDC_DMFLAGS_VALUE:

				flags_ReadNewValue( hDlg, LOWORD( wParam ), g_ulDMFlags );
				break;
			case IDC_DMFLAGS2_VALUE:

				flags_ReadNewValue( hDlg, LOWORD( wParam ), g_ulDMFlags2 );
				break;
			case IDC_COMPATFLAGS_VALUE:

				flags_ReadNewValue( hDlg, LOWORD( wParam ), g_ulCompatFlags );
				break;
			case IDC_COMPATFLAGS2_VALUE:

				flags_ReadNewValue( hDlg, LOWORD( wParam ), g_ulCompatFlags2 );
				break;
			}
		}
		else if ( HIWORD( wParam ) == EN_KILLFOCUS )
		{
			flags_UpdateFlagsFromCheckboxes( );
			flags_UpdateValueLabels( );
		}
		
		switch ( LOWORD( wParam ))
		{
		case IDOK:

			// Save the flags.
			if ( dmflags != g_ulDMFlags )
				dmflags = g_ulDMFlags;
			if ( dmflags2 != g_ulDMFlags2 )
				dmflags2 = g_ulDMFlags2;
			if ( compatflags != g_ulCompatFlags )
				compatflags = g_ulCompatFlags;
			if ( compatflags2 != g_ulCompatFlags2 )
				compatflags2 = g_ulCompatFlags2;

			EndDialog( hDlg, -1 );
			break;
		case IDCANCEL:

			EndDialog( hDlg, -1 );
			break;
		}
		break;
	case WM_NOTIFY:
		{
			LPNMHDR	nmhdr = (LPNMHDR) lParam;

			// User clicked the tab control.
			if ( nmhdr->idFrom == IDC_FLAGSTABCONTROL )
				return flags_HandleTabSwitch( hDlg, nmhdr );
		}
		break;
	default:

		return ( FALSE );
	}

	return ( TRUE );
}

//==========================================================================
//
// flags_UpdateFlagsFromCheckboxes
//
// Regenerates the flags' values from the checkboxes.
//
//==========================================================================

static void flags_UpdateFlagsFromCheckboxes( void )
{
	g_ulDMFlags = 0;
	g_ulDMFlags2 = 0;
	g_ulCompatFlags = 0;
	g_ulCompatFlags2 = 0;

	for ( unsigned int i = 0; i < NUMBER_OF_FLAGS; i++ )
	{
		if ( SendMessage( g_Flags[i].hObject, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
			*g_Flags[i].ulFlagVariable |= g_Flags[i].ulThisFlag;
	}

	// Update the drop-downs.
	for ( unsigned int i = 0; i < NUMBER_OF_DROPDOWNS; i++ )
	{
		int selectedIndex = SendMessage( g_MultiFlags[i].hObject, CB_GETCURSEL, 0, 0 );
		*g_MultiFlags[i].ulFlagVariable |= (int)( g_MultiFlags[i].dataPairings[selectedIndex].value );
	}
}

//==========================================================================
//
// flags_UpdateValueLabels
//
// Updates the numbers inside the "flags: ##" textboxes.
//
//==========================================================================

static void flags_UpdateValueLabels( void )
{
	FString fsLabel;

	fsLabel.Format( "%ld", g_ulDMFlags );
	SetDlgItemText( g_hDlg, IDC_DMFLAGS_VALUE, fsLabel.GetChars( ));

	fsLabel.Format( "%ld", g_ulDMFlags2 );
	SetDlgItemText( g_hDlg, IDC_DMFLAGS2_VALUE, fsLabel.GetChars( ));

	fsLabel.Format( "%ld", g_ulCompatFlags );
	SetDlgItemText( g_hDlg, IDC_COMPATFLAGS_VALUE, fsLabel.GetChars( ));

	fsLabel.Format( "%ld", g_ulCompatFlags2 );
	SetDlgItemText( g_hDlg, IDC_COMPATFLAGS2_VALUE, fsLabel.GetChars( ));
}

//==========================================================================
//
// flags_ReadNewValue
//
// Reads an entirely new flag from the "flags: ##" textboxes.
//
//==========================================================================

void flags_ReadNewValue( HWND hDlg, int iControlID, ULONG &ulFlags )
{
	char	szBuffer[1024];

	GetDlgItemText( hDlg, iControlID, szBuffer, 1024 );
	ulFlags = atoi( szBuffer );
	flags_UpdateCheckboxesFromFlags( );
}

//==========================================================================
//
// flags_UpdateCheckboxesFromFlags
//
// Resets the checkboxes' values from the flags.
//
//==========================================================================

static void flags_UpdateCheckboxesFromFlags( void )
{
	for ( unsigned int i = 0; i < NUMBER_OF_FLAGS; i++ )
		SendMessage( g_Flags[i].hObject, BM_SETCHECK, ( *g_Flags[i].ulFlagVariable & g_Flags[i].ulThisFlag ) ? BST_CHECKED : BST_UNCHECKED, 0 );

	// Update the drop-downs.
	for ( unsigned int i = 0; i < NUMBER_OF_DROPDOWNS; i++ )
	{
		// Go through the possible sub-values and find the one that's enabled.
		int largestIndex = 0, largestBit = 0;
		for ( int setting = 0; setting < g_MultiFlags[i].numPairings; setting++ )
		{
			int iBit = (int)(g_MultiFlags[i].dataPairings[setting].value);

			// This is equal to the largest fully-enabled subvalue. For example, falling damage is (8|16|24), so if 24 is on, we have to be careful not to choose 8.
			if ( (( *g_MultiFlags[i].ulFlagVariable & iBit ) == iBit ) && iBit > largestBit )
			{
				largestIndex = setting;
				largestBit = iBit;
			}
		}

		SendMessage( g_MultiFlags[i].hObject, CB_SETCURSEL, largestIndex, 0 );
	}
}

//==========================================================================
//
// InsertTab
//
// Helper method to insert a tab.
//
//==========================================================================

static void flags_InsertTab( char *pszTitle, int iResource, HWND hDlg, TCITEM tcitem, HWND edit, RECT tabrect, RECT tcrect, int &index )
{
	tcitem.pszText = pszTitle;
	tcitem.lParam = (LPARAM) CreateDialogParam( g_hInst, MAKEINTRESOURCE( iResource ), hDlg, flags_GenericTabCallback, (LPARAM) edit );
	TabCtrl_InsertItem( edit, index, &tcitem );
	SetWindowPos( (HWND) tcitem.lParam, HWND_TOP, tcrect.left + 3, tcrect.top + tabrect.bottom + 3,
		tcrect.right - tcrect.left - 8, tcrect.bottom - tcrect.top - tabrect.bottom - 8, 0 );
	index++;
}

//==========================================================================
//
// HandleTabSwitch
//
// Helper method to switch tabs properly.
//
//==========================================================================

static BOOL flags_HandleTabSwitch( HWND hDlg, LPNMHDR nmhdr )
{
	TCITEM		tcitem;
	HWND		edit;

	int i = TabCtrl_GetCurSel( nmhdr->hwndFrom );
	tcitem.mask = TCIF_PARAM;
	TabCtrl_GetItem( nmhdr->hwndFrom, i, &tcitem );
	edit = (HWND) tcitem.lParam;

	// The tab is right about to switch. Hide the current tab pane.
	if ( nmhdr->code == TCN_SELCHANGING )
	{
		ShowWindow( edit, SW_HIDE );
		SetWindowLongPtr( hDlg, DWLP_MSGRESULT, FALSE );
		return TRUE;
	}
	// The tab has just switched. Show the new tab pane.
	else if ( nmhdr->code == TCN_SELCHANGE )
	{
		ShowWindow ( edit, SW_SHOW );
		return TRUE;
	}
	else
		return FALSE;
}

//==========================================================================
//
// flags_GenericTabCallback
//
// Generic initializer for all the sub-tabs.
//
//==========================================================================

BOOL CALLBACK flags_GenericTabCallback( HWND hDlg, UINT Message, WPARAM wParam, LPARAM lParam )
{
	switch ( Message )
	{
	case WM_INITDIALOG:

		// Enable tab gradients on XP and later.
		if ( pEnableThemeDialogTexture != NULL )
			pEnableThemeDialogTexture ( hDlg, ETDT_ENABLETAB );

		// Go through and claim the checkboxes on this tab.
		for ( unsigned int i = 0; i < NUMBER_OF_FLAGS; i++ )
		{
			if ( GetDlgItem( hDlg, g_Flags[i].iControlID ) != NULL )
			{
				g_Flags[i].hObject = GetDlgItem( hDlg, g_Flags[i].iControlID );

				// Check to see if we need to disable the flag.
				EnableWindow( g_Flags[i].hObject, !g_Flags[i].bDisabled );
				if ( g_Flags[i].bDisabled )
					*g_Flags[i].ulFlagVariable &= ~g_Flags[i].ulThisFlag;
			}
		}

		// Init the drop-down boxes.
		for ( unsigned int i = 0; i < NUMBER_OF_DROPDOWNS; i++ )
		{
			if ( GetDlgItem( hDlg, g_MultiFlags[i].iControlID ) != NULL )
			{
				g_MultiFlags[i].hObject = GetDlgItem( hDlg, g_MultiFlags[i].iControlID );
				for ( int setting = 0; setting < g_MultiFlags[i].numPairings; setting++ )
					SendDlgItemMessage( hDlg, g_MultiFlags[i].iControlID, CB_INSERTSTRING, -1, (WPARAM)(LPSTR)g_MultiFlags[i].dataPairings[setting].name );
			}
		}

		flags_UpdateCheckboxesFromFlags( );
		flags_UpdateValueLabels( );
		break;
	case WM_COMMAND:

		// Check if user changed any of the checkboxes.
		for ( unsigned int i = 0; i < NUMBER_OF_FLAGS; i++ )
		{
			// They did! Update the flags.
			if ( g_Flags[i].iControlID == LOWORD( wParam ) )
			{
				flags_UpdateFlagsFromCheckboxes( );
				flags_UpdateValueLabels( );
				break;
			}
		}

		// Check if user changed any of the dropdowns.
		for ( unsigned int i = 0; i < NUMBER_OF_DROPDOWNS; i++ )
		{
			// They did! Update the flags.
			if ( g_MultiFlags[i].iControlID == LOWORD( wParam ) && HIWORD( wParam ) == CBN_SELCHANGE )
			{
				flags_UpdateFlagsFromCheckboxes( );
				flags_UpdateValueLabels( );
				break;
			}
		}
		break;
	}

	return FALSE;
}

#endif	// GUI_SERVER_CONSOLE
