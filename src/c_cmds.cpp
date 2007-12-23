/*
** c_cmds.cpp
** Miscellaneous console commands.
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
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
** It might be a good idea to move these into files that they are more
** closely related to, but right now, I am too lazy to do that.
*/

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

#include "version.h"
#include "c_console.h"
#include "c_dispatch.h"

#include "i_system.h"

#include "doomstat.h"
#include "gstrings.h"
#include "s_sound.h"
#include "g_game.h"
#include "g_level.h"
#include "w_wad.h"
#include "g_level.h"
#include "gi.h"
#include "r_defs.h"
#include "d_player.h"
#include "r_main.h"
#include "templates.h"
#include "p_local.h"
#include "r_sky.h"
#include "p_setup.h"
// [BC] New #includes.
#include "deathmatch.h"
#include "cl_demo.h"
#include "cl_main.h"
#include "network.h"
#include "p_local.h"
#include "p_acs.h"
#include "team.h"
#include "maprotation.h"
#include "cl_commands.h"

extern FILE *Logfile;
extern bool insave;

// [BC] Store the name of the file we're currently logging to.
extern char g_szLogFilename[256];

CVAR (Bool, sv_cheats, false, CVAR_SERVERINFO | CVAR_LATCH)
CVAR (Bool, sv_logfilenametimestamp, true, CVAR_ARCHIVE)

CCMD (toggleconsole)
{
	C_ToggleConsole();
}

bool CheckCheatmode ()
{
	if ((( G_SkillProperty( SKILLP_DisableCheats )) ||
		( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )) ||
		( NETWORK_GetState( ) == NETSTATE_SERVER )) &&
		( sv_cheats == false ))
	{
		Printf ("sv_cheats must be true to enable this command.\n");
		return true;
	}
	else
	{
		return false;
	}
}

CCMD (quit)
{
	// [BC] This function may not be used by ConsoleCommand.
	if ( ACS_IsCalledFromConsoleCommand( ))
		return;

	if (!insave) exit (0);
}

CCMD (exit)
{
	// [BC] This function may not be used by ConsoleCommand.
	if ( ACS_IsCalledFromConsoleCommand( ))
		return;

	if (!insave) exit (0);
}

/*
==================
Cmd_God

Sets client to godmode

argv(0) god
==================
*/
CCMD (god)
{
	if (CheckCheatmode ())
		return;

	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
		CLIENTCOMMANDS_GenericCheat( CHT_GOD );
	else
	{
		Net_WriteByte (DEM_GENERICCHEAT);
		Net_WriteByte (CHT_GOD);
	}
}

CCMD (iddqd)
{
	if (CheckCheatmode ())
		return;

	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
		CLIENTCOMMANDS_GenericCheat( CHT_IDDQD );
	else
	{
		Net_WriteByte (DEM_GENERICCHEAT);
		Net_WriteByte (CHT_IDDQD);
	}
}

// [BC] Allow users to execute the "idfa" cheat from the console.
CCMD( idfa )
{
	if ( CheckCheatmode( ))
		return;

	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
		CLIENTCOMMANDS_GenericCheat( CHT_IDFA );
	else
	{
		Net_WriteByte (DEM_GENERICCHEAT);
		Net_WriteByte (CHT_IDFA);
	}
}

// [BC] Allow users to execute the "idkfa" cheat from the console.
CCMD( idkfa )
{
	if ( CheckCheatmode( ))
		return;

	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
		CLIENTCOMMANDS_GenericCheat( CHT_IDKFA );
	else
	{
		Net_WriteByte (DEM_GENERICCHEAT);
		Net_WriteByte (CHT_IDKFA);
	}
}

// [BC] Allow users to execute the "idchoppers" cheat from the console.
CCMD( idchoppers )
{
	if ( CheckCheatmode( ))
		return;

	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
		CLIENTCOMMANDS_GenericCheat( CHT_CHAINSAW );
	else
	{
		Net_WriteByte (DEM_GENERICCHEAT);
		Net_WriteByte (CHT_CHAINSAW);
	}
}

CCMD (notarget)
{
	if (CheckCheatmode ())
		return;

	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
		CLIENTCOMMANDS_GenericCheat( CHT_NOTARGET );
	else
	{
		Net_WriteByte (DEM_GENERICCHEAT);
		Net_WriteByte (CHT_NOTARGET);
	}
}

CCMD (fly)
{
	if (CheckCheatmode ())
		return;

	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
		CLIENTCOMMANDS_GenericCheat( CHT_FLY );
	else
	{
		Net_WriteByte (DEM_GENERICCHEAT);
		Net_WriteByte (CHT_FLY);
	}
}

/*
==================
Cmd_Noclip

argv(0) noclip
==================
*/
CCMD (noclip)
{
	if (CheckCheatmode ())
		return;

	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
		CLIENTCOMMANDS_GenericCheat( CHT_NOCLIP );
	else
	{
		Net_WriteByte (DEM_GENERICCHEAT);
		Net_WriteByte (CHT_NOCLIP);
	}
}

CCMD (powerup)
{
	if (CheckCheatmode ())
		return;

	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
		CLIENTCOMMANDS_GenericCheat( CHT_POWER );
	else
	{
		Net_WriteByte (DEM_GENERICCHEAT);
		Net_WriteByte (CHT_POWER);
	}
}

CCMD (morphme)
{
	if (CheckCheatmode ())
		return;

	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
		CLIENTCOMMANDS_GenericCheat( CHT_MORPH );
	else
	{
		if (argv.argc() == 1)
		{
			Net_WriteByte (DEM_GENERICCHEAT);
			Net_WriteByte (CHT_MORPH);
		}
		else
		{
			Net_WriteByte (DEM_MORPHEX);
			Net_WriteString (argv[1]);
		}
	}
}

CCMD (anubis)
{
	if (CheckCheatmode ())
		return;

	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
		CLIENTCOMMANDS_GenericCheat( CHT_ANUBIS );
	else
	{
		Net_WriteByte (DEM_GENERICCHEAT);
		Net_WriteByte (CHT_ANUBIS);
	}
}

// [GRB]
// [BC] I don't think so.
/*
CCMD (resurrect)
{
	if (CheckCheatmode ())
		return;

	Net_WriteByte (DEM_GENERICCHEAT);
	Net_WriteByte (CHT_RESSURECT);
}
*/
EXTERN_CVAR (Bool, chasedemo)

CCMD (chase)
{
	// [BC] Support for client-side demos.
	if (demoplayback || ( CLIENTDEMO_IsPlaying( )))
	{
		int i;

		if (chasedemo)
		{
			chasedemo = false;
			for (i = 0; i < MAXPLAYERS; i++)
				players[i].cheats &= ~CF_CHASECAM;
		}
		else
		{
			chasedemo = true;
			for (i = 0; i < MAXPLAYERS; i++)
				players[i].cheats |= CF_CHASECAM;
		}
		R_ResetViewInterpolation ();
	}
	else
	{
		// [BC] Disallow chasecam by default in teamgame as well.
		if (gamestate == GS_LEVEL && ( deathmatch || teamgame ) && CheckCheatmode ())
			return;

		if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
			CLIENTCOMMANDS_GenericCheat( CHT_CHASECAM );
		else
		{
			Net_WriteByte (DEM_GENERICCHEAT);
			Net_WriteByte (CHT_CHASECAM);
		}
	}
}

CCMD (idclev)
{
	if ((CheckCheatmode ()) || ( NETWORK_GetState( ) == NETSTATE_CLIENT ))
		return;

	if ((argv.argc() > 1) && (*(argv[1] + 2) == 0) && *(argv[1] + 1) && *argv[1])
	{
		int epsd, map;
		char buf[2];
		char *mapname;

		buf[0] = argv[1][0] - '0';
		buf[1] = argv[1][1] - '0';

		if (gameinfo.flags & GI_MAPxx)
		{
			epsd = 1;
			map = buf[0]*10 + buf[1];
		}
		else
		{
			epsd = buf[0];
			map = buf[1];
		}

		// Catch invalid maps.
		mapname = CalcMapName (epsd, map);

		MapData * mapd = P_OpenMapData(mapname);
		if (mapd == NULL)
			return;

		// So be it.
		delete mapd;
		Printf ("%s\n", GStrings("STSTR_CLEV"));
      	G_DeferedInitNew (mapname);
		players[0].health = 0;		// Force reset
	}
}

CCMD (hxvisit)
{
	if (CheckCheatmode ())
		return;

	if ((argv.argc() > 1) && (*(argv[1] + 2) == 0) && *(argv[1] + 1) && *argv[1])
	{
		char mapname[9];

		sprintf (mapname, "&wt@%c%c", argv[1][0], argv[1][1]);

		if (CheckWarpTransMap (mapname, false))
		{
			// Just because it's in MAPINFO doesn't mean it's in the wad.

			MapData * map = P_OpenMapData(mapname);
			if (map != NULL)
			{
				// So be it.
				delete map;
				Printf ("%s\n", GStrings("STSTR_CLEV"));
      			G_DeferedInitNew (mapname);
				return;
			}
		}
		Printf ("No such map found\n");
	}
}

CCMD (changemap)
{
	if ( level.info == NULL )
	{
		Printf( "You can't use changemap, when not in a level.\n" );
		return;
	}

	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
	{
		Printf( "Only the server can change the map.\n" );
		return;
	}

	if (argv.argc() > 1)
	{
		MapData * map = P_OpenMapData(argv[1]);
		if (map == NULL)
		{
			Printf ("No map %s\n", argv[1]);
		}
		else
		{
			delete map;
			// Fuck that DEM shit!
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			{
				strncpy( level.nextmap, argv[1], 8 );

				level.flags |= LEVEL_CHANGEMAPCHEAT;

				G_ExitLevel( 0, false );
			}
			else
			{
				if (argv.argc() > 2)
				{
					Net_WriteByte (DEM_CHANGEMAP2);
					Net_WriteByte (atoi(argv[2]));
				}
				else
				{
					Net_WriteByte (DEM_CHANGEMAP);
				}
				Net_WriteString (argv[1]);
			}
		}
	}
	else
	{
		Printf ("Usage: changemap <map name> [position]\n");
	}
}

CCMD( nextmap )
{
	if ( level.info == NULL )
	{
		Printf( "You can't use nextmap, when not in a level.\n" );
		return;
	}

	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
	{
		Printf( "Only the server can change the map.\n" );
		return;
	}

	// Fuck that DEM shit!
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		G_ExitLevel( 0, false );
	}
	else
	{
		if (argv.argc() > 1)
		{
			Net_WriteByte (DEM_CHANGEMAP2);
			Net_WriteByte (atoi(argv[1]));
		}
		else
		{
			Net_WriteByte (DEM_CHANGEMAP);
		}
		Net_WriteString( level.nextmap );
	}
}

CCMD (give)
{
	if (CheckCheatmode () || argv.argc() < 2)
		return;

	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
	{
		if ( argv.argc( ) > 2 )
			CLIENTCOMMANDS_GiveCheat( argv[1], clamp( atoi( argv[2]), 1, 255 ));
		else
			CLIENTCOMMANDS_GiveCheat( argv[1], 0 );
	}
	else
	{
		Net_WriteByte (DEM_GIVECHEAT);
		Net_WriteString (argv[1]);
		if (argv.argc() > 2)
			Net_WriteWord (clamp (atoi (argv[2]), 1, 32767));
		else
			Net_WriteWord (0);
	}
}

CCMD (gameversion)
{
	Printf ("%s : " __DATE__ "\n", DOTVERSIONSTR);
}

CCMD (print)
{
	if (argv.argc() != 2)
	{
		Printf ("print <name>: Print a string from the string table\n");
		return;
	}
	const char *str = GStrings[argv[1]];
	if (str == NULL)
	{
		Printf ("%s unknown\n", argv[1]);
	}
	else
	{
		Printf ("%s\n", str);
	}
}

CCMD (exec)
{
	if (argv.argc() < 2)
		return;

	for (int i = 1; i < argv.argc(); ++i)
	{
		switch (C_ExecFile (argv[i], gamestate == GS_STARTUP))
		{
		case 1: Printf ("Could not open \"%s\"\n", argv[1]); break;
		case 2: Printf ("Error parsing \"%s\"\n", argv[1]); break;
		default: break;
		}
	}
}

CCMD (logfile)
{
	const char *timestr = myasctime ();

	// This function may not be used by ConsoleCommand.
	if ( ACS_IsCalledFromConsoleCommand( ))
		return;

	if (Logfile)
	{
		Printf ("Log stopped: %s\n", timestr);
		fclose (Logfile);
		Logfile = NULL;
	}

	if (argv.argc() >= 2)
	{
		// [BB] In case (sv_logfilenametimestamp == true) we append the current date/time to the logfile name
		char logfilename[256];

		time_t clock;
		struct tm *lt;
		time (&clock);
		lt = localtime (&clock);
		if (lt != NULL && sv_logfilenametimestamp )
			sprintf( logfilename, "%s__%d_%02d_%02d-%02d_%02d_%02d", argv[1], lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday, lt->tm_hour, lt->tm_min, lt->tm_sec);
		else
			sprintf( logfilename, "%s", argv[1]);

		if ( (Logfile = fopen (logfilename, "w")) )
		{
			sprintf( g_szLogFilename, argv[1] );
			Printf ("Log started: %s\n", timestr);
		}
		else
		{
			Printf ("Could not start log\n");
		}
	}
}

CCMD (puke)
{
	int argc = argv.argc();

	if (argc < 2 || argc > 5)
	{
		Printf (" puke <script> [arg1] [arg2] [arg3]\n");
	}
	else
	{
		int script = atoi (argv[1]);

		if (script == 0)
		{ // Script 0 is reserved for Strife support. It is not pukable.
			return;
		}
		int arg[3] = { 0, 0, 0 };
		int argn = MIN (argc - 2, 3), i;

		for (i = 0; i < argn; ++i)
		{
			arg[i] = atoi (argv[2+i]);
		}

		// [BB] The check if the client is allowed to execute the script
		// is done in P_StartScript, no need to check here.
		if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
			( NETWORK_GetState( ) == NETSTATE_SERVER ) ||
			( CLIENTDEMO_IsPlaying( )))
		{
			P_StartScript (players[consoleplayer].mo, NULL, script, level.mapname, false,
				arg[0], arg[1], arg[2], (script < 0), false, true);
		}
		else
		{
			if (script > 0)
			{
				Net_WriteByte (DEM_RUNSCRIPT);
				Net_WriteWord (script);
			}
			else
			{
				Net_WriteByte (DEM_RUNSCRIPT2);
				Net_WriteWord (-script);
			}
			Net_WriteByte (argn);
			for (i = 0; i < argn; ++i)
			{
				Net_WriteLong (arg[i]);
			}
		}
	}
}

CCMD (error)
{
	if (argv.argc() > 1)
	{
		char *textcopy = copystring (argv[1]);
		I_Error (textcopy);
	}
	else
	{
		Printf ("Usage: error <error text>\n");
	}
}

CCMD (error_fatal)
{
	if (argv.argc() > 1)
	{
		char *textcopy = copystring (argv[1]);
		I_FatalError (textcopy);
	}
	else
	{
		Printf ("Usage: error_fatal <error text>\n");
	}
}

CCMD (dir)
{
	FString dir;
	char curdir[256];
	const char *match;
	findstate_t c_file;
	void *file;

	if (!getcwd (curdir, countof(curdir)))
	{
		Printf ("Current path too long\n");
		return;
	}

	if (argv.argc() == 1 || chdir (argv[1]))
	{
		match = argv.argc() == 1 ? "./*" : argv[1];

		dir = ExtractFilePath (match);
		if (dir[0] != '\0')
		{
			match += dir.Len();
		}
		else
		{
			dir = "./";
		}
		if (match[0] == '\0')
		{
			match = "*";
		}

		if (chdir (dir))
		{
			Printf ("%s not found\n", dir.GetChars());
			return;
		}
	}
	else
	{
		match = "*";
		dir = argv[1];
		if (dir[dir.Len()-1] != '/')
		{
			dir += '/';
		}
	}

	if ( (file = I_FindFirst (match, &c_file)) == ((void *)(-1)))
		Printf ("Nothing matching %s%s\n", dir.GetChars(), match);
	else
	{
		Printf ("Listing of %s%s:\n", dir.GetChars(), match);
		do
		{
			if (I_FindAttr (&c_file) & FA_DIREC)
				Printf (PRINT_BOLD, "%s <dir>\n", I_FindName (&c_file));
			else
				Printf ("%s\n", I_FindName (&c_file));
		} while (I_FindNext (file, &c_file) == 0);
		I_FindClose (file);
	}

	chdir (curdir);
}

CCMD (fov)
{
	player_t *player = who ? who->player : &players[consoleplayer];

	if (argv.argc() != 2)
	{
		Printf ("fov is %g\n", player->DesiredFOV);
		return;
	}
	else if (dmflags & DF_NO_FOV)
	{
		if (consoleplayer == Net_Arbitrator)
		{
			Net_WriteByte (DEM_FOV);
		}
		else
		{
			Printf ("The arbitrator has disabled FOV changes.\n");
			return;
		}
	}
	else
	{
		// Just do this here in client games.
		if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
			player->DesiredFOV = clamp (atoi (argv[1]), 5, 179);

		Net_WriteByte (DEM_MYFOV);
	}
	Net_WriteByte (clamp (atoi (argv[1]), 5, 179));
}

//==========================================================================
//
// CCMD r_visibility
//
// Controls how quickly light ramps across a 1/z range. Set this, and it
// sets all the r_*Visibility variables (except r_SkyVisibilily, which is
// currently unused).
//
//==========================================================================

CCMD (r_visibility)
{
	if (argv.argc() < 2)
	{
		Printf ("Visibility is %g\n", R_GetVisibility());
	}
	else if ( NETWORK_GetState( ) != NETSTATE_CLIENT )
	{
		R_SetVisibility (atof (argv[1]));
	}
	else
	{
		Printf ("Visibility cannot be changed in net games.\n");
	}
}

//==========================================================================
//
// CCMD warp
//
// Warps to a specific location on a map
//
//==========================================================================

CCMD (warp)
{
	if (gamestate != GS_LEVEL)
	{
		Printf ("You can only warp inside a level.\n");
		return;
	}
	else if ( NETWORK_GetState( ) != NETSTATE_SINGLE )
	{
		Printf ("You cannot warp in a net game!\n");
		return;
	}
	if (argv.argc() != 3)
	{
		Printf ("Usage: warp <x> <y>\n");
	}
	else
	{
		P_TeleportMove (players[consoleplayer].mo, fixed_t(atof(argv[1])*65536.0), fixed_t(atof(argv[2])*65536.0), ONFLOORZ, true);
	}
}

//==========================================================================
//
// CCMD load
//
// Load a saved game.
//
//==========================================================================

CCMD (load)
{
    if (argv.argc() != 2)
	{
        Printf ("usage: load <filename>\n");
        return;
    }
	if (( NETWORK_GetState( ) != NETSTATE_SINGLE ) && ( NETWORK_GetState( ) != NETSTATE_SINGLE_MULTIPLAYER ))
	{
		Printf ("cannot load during a network game\n");
		return;
	}
	FString fname = argv[1];
	DefaultExtension (fname, ".zds");
    G_LoadGame (fname);
}

//==========================================================================
//
// CCMD save
//
// Save the current game.
//
//==========================================================================

CCMD (save)
{
    if (argv.argc() < 2 || argv.argc() > 3)
	{
        Printf ("usage: save <filename> [description]\n");
        return;
    }
    if (!usergame)
	{
        Printf ("not in a saveable game\n");
        return;
    }
    if (gamestate != GS_LEVEL)
	{
        Printf ("not in a level\n");
        return;
    }
    if(players[consoleplayer].health <= 0 && ( NETWORK_GetState( ) == NETSTATE_SINGLE ))
    {
        Printf ("player is dead in a single-player game\n");
        return;
    }
    FString fname = argv[1];
	DefaultExtension (fname, ".zds");
	G_SaveGame (fname, argv.argc() > 2 ? argv[2] : argv[1]);
}

//==========================================================================
//
// CCMD wdir
//
// Lists the contents of a loaded wad file.
//
//==========================================================================

CCMD (wdir)
{
	if (argv.argc() != 2)
	{
		Printf ("usage: wdir <wadfile>\n");
		return;
	}
	int wadnum = Wads.CheckIfWadLoaded (argv[1]);
	if (wadnum < 0)
	{
		Printf ("%s must be loaded to view its directory.\n", argv[1]);
		return;
	}
	for (int i = 0; i < Wads.GetNumLumps(); ++i)
	{
		if (Wads.GetLumpFile(i) == wadnum)
		{
			Printf ("%s\n", Wads.GetLumpFullName(i));
		}
	}
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------
CCMD(linetarget)
{
	if (CheckCheatmode () || players[consoleplayer].mo == NULL) return;
	P_AimLineAttack(players[consoleplayer].mo,players[consoleplayer].mo->angle,MISSILERANGE, 0);
	if (linetarget)
	{
		Printf("Target=%s, Health=%d, Spawnhealth=%d\n",
			linetarget->GetClass()->TypeName.GetChars(),
			linetarget->health,
			linetarget->GetDefault()->health);
	}
	else Printf("No target found\n");
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------
CCMD(monster)
{
	AActor * mo;

	if (CheckCheatmode ()) return;
	TThinkerIterator<AActor> it;

	while ( (mo = it.Next()) )
	{
		if (mo->flags3&MF3_ISMONSTER && !(mo->flags&MF_CORPSE) && !(mo->flags&MF_FRIENDLY))
		{
			Printf ("%s at (%d,%d,%d)\n",
				mo->GetClass()->TypeName.GetChars(),
				mo->x >> FRACBITS, mo->y >> FRACBITS, mo->z >> FRACBITS);
		}
	}
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------
CCMD(items)
{
	AActor * mo;

	if (CheckCheatmode ()) return;
	TThinkerIterator<AActor> it;

	while ( (mo = it.Next()) )
	{
		if (mo->IsKindOf(RUNTIME_CLASS(AInventory)) && mo->flags&MF_SPECIAL)
		{
			Printf ("%s at (%d,%d,%d)\n",
				mo->GetClass()->TypeName.GetChars(),
				mo->x >> FRACBITS, mo->y >> FRACBITS, mo->z >> FRACBITS);
		}
	}
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------
CCMD(changesky)
{
	const char *sky1name;

	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) || ( NETWORK_GetState( ) == NETSTATE_SERVER ) || argv.argc()<2) return;

	sky1name = argv[1];
	if (sky1name[0] != 0)
	{
		strncpy (level.skypic1, sky1name, 8);
		sky1texture = TexMan.GetTexture (sky1name, FTexture::TEX_Wall, FTextureManager::TEXMAN_Overridable);
	}
	R_InitSkyMap ();
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------
CCMD(thaw)
{
	if (CheckCheatmode())
		return;

	Net_WriteByte (DEM_GENERICCHEAT);
	Net_WriteByte (CHT_CLEARFROZENPROPS);
}

//*****************************************************************************
//	Some console commands to emulate a multiplayer game while in single player mode.
CCMD( netgame )
{
	if ( NETWORK_GetState( ) != NETSTATE_SINGLE )
		Printf( "Multiplayer already enabled/emulated.\n" );
	else
	{
		NETWORK_SetState( NETSTATE_SINGLE_MULTIPLAYER );
		Printf( "Multiplayer emulation enabled.\n" );
	}
}

//*****************************************************************************
//
CCMD( multiplayer )
{
	C_DoCommand( "netgame", 0 );
}
