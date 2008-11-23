/*
** teaminfo.cpp
** Implementation of the TEAMINFO lump.
**
**---------------------------------------------------------------------------
** Copyright 2007 Christopher Westley
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
*/

// HEADER FILES ------------------------------------------------------------

#include "d_player.h"
#include "i_system.h"
#include "sc_man.h"
#include "teaminfo.h"
#include "v_palette.h"
#include "w_wad.h"

// [CW] New includes.
#include "team.h"
#include "version.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void TEAMINFO_Init ();
void TEAMINFO_ParseTeam ();

// [CW] See 'TEAM_CheckIfValid' in 'team.cpp'.

//bool TEAMINFO_IsValidTeam (int team);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

TArray <TEAMINFO> teams;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static const char *keywords_teaminfo [] = {
	"PLAYERCOLOR",
	"TEXTCOLOR",
	"LOGO",
	"FLAGITEM",
	"SKULLITEM",
	"RAILCOLOR",
	"PLAYERSTARTTHINGNUMBER",
	"SMALLFLAGHUDICON",
	"SMALLSKULLHUDICON",
	"LARGEFLAGHUDICON",
	"LARGESKULLHUDICON",
	"WINNERPIC",
	"LOSERPIC",
	"WINNERTHEME",
	"LOSERTHEME",
	"ALLOWCUSTOMPLAYERCOLOR",
	NULL
};

// CODE --------------------------------------------------------------------

//==========================================================================
//
// TEAMINFO_Init
//
//==========================================================================

void TEAMINFO_Init ()
{
	int lastlump = 0, lump;

	while ((lump = Wads.FindLump ("TEAMINFO", &lastlump)) != -1)
	{
		SC_OpenLumpNum (lump, "TEAMINFO");
		while (SC_GetString ())
		{
			if (SC_Compare("CLEARTEAMS"))
				teams.Clear ();
			else if (SC_Compare("TEAM"))
				TEAMINFO_ParseTeam ();
			else 
				SC_ScriptError ("Unknown command %s in TEAMINFO", sc_String);
		}
		SC_Close();
	}

	if (teams.Size () < 2)
		I_FatalError ("At least two teams must be defined in TEAMINFO");

	if ( teams.Size( ) > MAX_TEAMS )
		I_FatalError ( "Only %d teams can be defined in TEAMINFO", MAX_TEAMS );
}

//==========================================================================
//
// TEAMINFO_ParseTeam
//
//==========================================================================

void TEAMINFO_ParseTeam ()
{
	TEAMINFO team;

	// [BB] Initialize some values.
	team.bCustomPlayerColorAllowed = false;

	int i;
	char *color;

	SC_MustGetString ();
	team.Name = sc_String;

	SC_MustGetStringName("{");
	while (!SC_CheckString("}"))
	{
		SC_MustGetString();
		switch(i = SC_MatchString (keywords_teaminfo))
		{
		case 0:
			SC_MustGetString ();
			color = sc_String;
			team.lPlayerColor = V_GetColor (NULL, color);
			break;

		case 1:
			SC_MustGetString();
			team.TextColor = '[';
			team.TextColor += sc_String;
			team.TextColor += ']';
			break;

		case 2:
			SC_MustGetString( );
			// [CW] 'Logo' isn't supported by Skulltag.
			Printf( "WARNING: 'Logo' is not a supported TEAMINFO option in "GAMENAME".\n" );
			break;

		case 3:
			SC_MustGetString( );
			team.FlagItem = sc_String;
			break;

		case 4:
			SC_MustGetString( );
			team.SkullItem = sc_String;
			break;

		case 5:
			SC_MustGetString( );
			team.lRailColor = V_GetColorFromString( NULL, sc_String );
			break;

		case 6:
			SC_MustGetNumber( );
			team.ulPlayerStartThingNumber = sc_Number;
			break;

		case 7:
			SC_MustGetString( );
			team.SmallFlagHUDIcon = sc_String;
			break;

		case 8:
			SC_MustGetString( );
			team.SmallSkullHUDIcon = sc_String;
			break;

		case 9:
			SC_MustGetString( );
			team.LargeFlagHUDIcon = sc_String;
			break;

		case 10:
			SC_MustGetString( );
			team.LargeSkullHUDIcon = sc_String;
			break;

		case 11:
			SC_MustGetString( );
			team.WinnerPic = sc_String;
			break;

		case 12:
			SC_MustGetString( );
			team.LoserPic = sc_String;
			break;

		case 13:
			SC_MustGetString( );
			team.WinnerTheme = sc_String;
			break;

		case 14:
			SC_MustGetString( );
			team.LoserTheme = sc_String;
			break;

		case 15:
			team.bCustomPlayerColorAllowed = true;
			break;

		default:
			I_FatalError( "Unknown option '%s', on line %d in TEAMINFO.", sc_String, sc_Line );
			break;
		}
	}

	teams.Push (team);
}

/*

// [CW] See 'TEAM_CheckIfValid' in 'team.cpp'.

//==========================================================================
//
// TEAMINFO_IsValidTeam
//
//==========================================================================

bool TEAMINFO_IsValidTeam (int team)
{
	if (team < 0 || team >= (signed)teams.Size ())
	{
		return false;
	}

	return true;
}

// [CW] See 'TEAM_GetTextColor' in 'team.cpp'.

//==========================================================================
//
// TEAMINFO :: GetTextColor
//
//==========================================================================

int TEAMINFO::GetTextColor () const
{
	if (textcolor.IsEmpty())
	{
		return CR_UNTRANSLATED;
	}
	const BYTE *cp = (const BYTE *)textcolor.GetChars();
	int color = V_ParseFontColor(cp, 0, 0);
	if (color == CR_UNDEFINED)
	{
		Printf("Undefined color '%s' in definition of team %s\n", textcolor.GetChars (), name.GetChars ());
		color = CR_UNTRANSLATED;
	}
	return color;
}

*/
