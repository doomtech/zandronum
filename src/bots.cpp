//-----------------------------------------------------------------------------
//
// Skulltag Source
// Copyright (C) 2002 Brad Carney
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
//
//
// Filename: bots.cpp
//
// Description: Contains bot functions
//
//-----------------------------------------------------------------------------

#include "astar.h"
#include "bots.h"
#include "botcommands.h"
#include "c_cvars.h"
#include "c_dispatch.h"
#include "campaign.h"
#include "cmdlib.h"
#include "configfile.h"
#include "deathmatch.h"
#include "doomdef.h"
#include "doomstat.h"
#include "d_netinf.h"
#include "g_game.h"
#include "gi.h"
#include "i_system.h"
#include "joinqueue.h"
#include "m_random.h"
#include "network.h"
#include "p_lnspec.h"
#include "p_local.h"
#include "r_data.h"
#include "r_main.h"
#include "r_sky.h"
#include "s_sound.h"
#include "sc_man.h"
#include "scoreboard.h"
#include "st_stuff.h"
#include "stats.h"
#include "sv_commands.h"
#include "team.h"
#include "vectors.h"
#include "version.h"
#include "v_video.h"
#include "w_wad.h"
#include "p_trace.h"
#include "sbar.h"

//*****************************************************************************
//	VARIABLES

static	FRandom		BotSpawn( "BotSpawn" );
static	FRandom		BotRemove( "BotRemove" );
static	FRandom		g_RandomBotAimSeed( "RandomBotAimSeed" );
static	BOTSPAWN_t	g_BotSpawn[MAXPLAYERS];
static	BOTINFO_t	*g_BotInfo[MAX_BOTINFO];
static	BOTINFO_t	g_HardcodedBotInfo[NUM_HARDCODED_BOTS];
static	cycle_t		g_BotCycles;
static	bool		g_bBotIsInitialized[MAXPLAYERS];
static	LONG		g_lLastHeader;
static	char		*g_pszDataHeaders[NUM_DATAHEADERS] =
{
	"DH_COMMAND",
	"DH_STATEIDX",
	"DH_STATENAME",
	"DH_ONENTER",
	"DH_MAINLOOP",
	"DH_ONEXIT",
	"DH_EVENT",
	"DH_ENDONENTER",
	"DH_ENDMAINLOOP",
	"DH_ENDONEXIT",
	"DH_ENDEVENT",
	"DH_IFGOTO",
	"DH_IFNOTGOTO",
	"DH_GOTO",
	"DH_ORLOGICAL",
	"DH_ANDLOGICAL",
	"DH_ORBITWISE",
	"DH_EORBITWISE",
	"DH_ANDBITWISE",
	"DH_EQUALS",
	"DH_NOTEQUALS",
	"DH_LESSTHAN",
	"DH_LESSTHANEQUALS",
	"DH_GREATERTHAN",
	"DH_GREATERTHANEQUALS",
	"DH_NEGATELOGICAL",
	"DH_LSHIFT",
	"DH_RSHIFT",
	"DH_ADD",
	"DH_SUBTRACT",
	"DH_UNARYMINUS",
	"DH_MULTIPLY",
	"DH_DIVIDE",
	"DH_MODULUS",
	"DH_PUSHNUMBER",
	"DH_PUSHSTRINGINDEX",
	"DH_PUSHGLOBALVAR",
	"DH_PUSHLOCALVAR",
	"DH_DROPSTACKPOSITION",
	"DH_SCRIPTVARLIST",
	"DH_STRINGLIST",
	"DH_INCGLOBALVAR",
	"DH_DECGLOBALVAR",
	"DH_ASSIGNGLOBALVAR",
	"DH_ADDGLOBALVAR",
	"DH_SUBGLOBALVAR",
	"DH_MULGLOBALVAR",
	"DH_DIVGLOBALVAR",
	"DH_MODGLOBALVAR",
	"DH_INCLOCALVAR",
	"DH_DECLOCALVAR",
	"DH_ASSIGNLOCALVAR",
	"DH_ADDLOCALVAR",
	"DH_SUBLOCALVAR",
	"DH_MULLOCALVAR",
	"DH_DIVLOCALVAR",
	"DH_MODLOCALVAR",
	"DH_CASEGOTO",
	"DH_DROP",
	"DH_INCGLOBALARRAY",
	"DH_DECGLOBALARRAY",
	"DH_ASSIGNGLOBALARRAY",
	"DH_ADDGLOBALARRAY",
	"DH_SUBGLOBALARRAY",
	"DH_MULGLOBALARRAY",
	"DH_DIVGLOBALARRAY",
	"DH_MODGLOBALARRAY",
	"DH_PUSHGLOBALARRAY",
	"DH_SWAP",
	"DH_DUP",
	"DH_ARRAYSET",
};

//*****************************************************************************
static	LONG	g_lExpectedStackChange[NUM_DATAHEADERS] =
{
	0,		// DH_COMMAND
	0,		// DH_STATEIDX
	0,		// DH_STATENAME
	0,		// DH_ONENTER
	0,		// DH_MAINLOOP
	0,		// DH_ONEXIT
	0,		// DH_EVENT
	0,		// DH_ENDONENTER
	0,		// DH_ENDMAINLOOP
	0,		// DH_ENDONEXIT
	0,		// DH_ENDEVENT
	-1,		// DH_IFGOTO
	-1,		// DH_IFNOTGOTO
	0,		// DH_GOTO
	-1,		// DH_ORLOGICAL
	-1,		// DH_ANDLOGICAL
	-1,		// DH_ORBITWISE
	-1,		// DH_EORBITWISE
	-1,		// DH_ANDBITWISE
	-1,		// DH_EQUALS
	-1,		// DH_NOTEQUALS
	-1,		// DH_LESSTHAN
	-1,		// DH_LESSTHANEQUALS
	-1,		// DH_GREATERTHAN
	-1,		// DH_GREATERTHANEQUALS
	0,		// DH_NEGATELOGICAL
	-1,		// DH_LSHIFT
	-1,		// DH_RSHIFT
	-1,		// DH_ADD
	-1,		// DH_SUBTRACT
	0,		// DH_UNARYMINUS
	-1,		// DH_MULTIPLY
	-1,		// DH_DIVIDE
	-1,		// DH_MODULUS
	1,		// DH_PUSHNUMBER
	1,		// DH_PUSHSTRINGINDEX
	1,		// DH_PUSHGLOBALVAR
	1,		// DH_PUSHLOCALVAR
	-1,		// DH_DROPSTACKPOSITION
	0,		// DH_SCRIPTVARLIST
	0,		// DH_STRINGLIST
	0,		// DH_INCGLOBALVAR
	0,		// DH_DECGLOBALVAR
	-1,		// DH_ASSIGNGLOBALVAR
	-1,		// DH_ADDGLOBALVAR
	-1,		// DH_SUBGLOBALVAR
	-1,		// DH_MULGLOBALVAR
	-1,		// DH_DIVGLOBALVAR
	-1,		// DH_MODGLOBALVAR
	0,		// DH_INCLOCALVAR
	0,		// DH_DECLOCALVAR
	-1,		// DH_ASSIGNLOCALVAR
	-1,		// DH_ADDLOCALVAR
	-1,		// DH_SUBLOCALVAR
	-1,		// DH_MULLOCALVAR
	-1,		// DH_DIVLOCALVAR
	-1,		// DH_MODLOCALVAR
	0,		// DH_CASEGOTO
	-1,		// DH_DROP
	-1,		// DH_INCGLOBALARRAY
	-1,		// DH_DECGLOBALARRAY
	-2,		// DH_ASSIGNGLOBALARRAY
	-2,		// DH_ADDGLOBALARRAY
	-2,		// DH_SUBGLOBALARRAY
	-2,		// DH_MULGLOBALARRAY
	-2,		// DH_DIVGLOBALARRAY
	-2,		// DH_MODGLOBALARRAY
	0,		// DH_PUSHGLOBALARRAY
	0,		// DH_SWAP
	1,		// DH_DUP
	-3,		// DH_ARRAYSET
};

//*****************************************************************************
static	char	*g_pszEventNames[NUM_BOTEVENTS] =
{
	"BOTEVENT_KILLED_BYENEMY",
	"BOTEVENT_KILLED_BYPLAYER",
	"BOTEVENT_KILLED_BYSELF",
	"BOTEVENT_KILLED_BYENVIORNMENT",
	"BOTEVENT_REACHED_GOAL",
	"BOTEVENT_GOAL_REMOVED",
	"BOTEVENT_DAMAGEDBY_PLAYER",
	"BOTEVENT_PLAYER_SAY",
	"BOTEVENT_ENEMY_KILLED",
	"BOTEVENT_RESPAWNED",
	"BOTEVENT_INTERMISSION",
	"BOTEVENT_NEWMAP",
	"BOTEVENT_ENEMY_USEDFIST",
	"BOTEVENT_ENEMY_USEDCHAINSAW",
	"BOTEVENT_ENEMY_FIREDPISTOL",
	"BOTEVENT_ENEMY_FIREDSHOTGUN",
	"BOTEVENT_ENEMY_FIREDSSG",
	"BOTEVENT_ENEMY_FIREDCHAINGUN",
	"BOTEVENT_ENEMY_FIREDMINIGUN",
	"BOTEVENT_ENEMY_FIREDROCKET",
	"BOTEVENT_ENEMY_FIREDGRENADE",
	"BOTEVENT_ENEMY_FIREDRAILGUN",
	"BOTEVENT_ENEMY_FIREDPLASMA",
	"BOTEVENT_ENEMY_FIREDBFG",
	"BOTEVENT_ENEMY_FIREDBFG10K",
	"BOTEVENT_PLAYER_USEDFIST",
	"BOTEVENT_PLAYER_USEDCHAINSAW",
	"BOTEVENT_PLAYER_FIREDPISTOL",
	"BOTEVENT_PLAYER_FIREDSHOTGUN",
	"BOTEVENT_PLAYER_FIREDSSG",
	"BOTEVENT_PLAYER_FIREDCHAINGUN",
	"BOTEVENT_PLAYER_FIREDMINIGUN",
	"BOTEVENT_PLAYER_FIREDROCKET",
	"BOTEVENT_PLAYER_FIREDGRENADE",
	"BOTEVENT_PLAYER_FIREDRAILGUN",
	"BOTEVENT_PLAYER_FIREDPLASMA",
	"BOTEVENT_PLAYER_FIREDBFG",
	"BOTEVENT_PLAYER_FIREDBFG10K",
	"BOTEVENT_USEDFIST",
	"BOTEVENT_USEDCHAINSAW",
	"BOTEVENT_FIREDPISTOL",
	"BOTEVENT_FIREDSHOTGUN",
	"BOTEVENT_FIREDSSG",
	"BOTEVENT_FIREDCHAINGUN",
	"BOTEVENT_FIREDMINIGUN",
	"BOTEVENT_FIREDROCKET",
	"BOTEVENT_FIREDGRENADE",
	"BOTEVENT_FIREDRAILGUN",
	"BOTEVENT_FIREDPLASMA",
	"BOTEVENT_FIREDBFG",
	"BOTEVENT_FIREDBFG10K",
	"BOTEVENT_PLAYER_JOINEDGAME",
	"BOTEVENT_JOINEDGAME",
	"BOTEVENT_DUEL_STARTINGCOUNTDOWN",
	"BOTEVENT_DUEL_FIGHT",
	"BOTEVENT_DUEL_WINSEQUENCE",
	"BOTEVENT_SPECTATING",
	"BOTEVENT_LMS_STARTINGCOUNTDOWN",
	"BOTEVENT_LMS_FIGHT",
	"BOTEVENT_LMS_WINSEQUENCE",
	"BOTEVENT_WEAPONCHANGE",
	"BOTEVENT_ENEMY_BFGEXPLODE",
	"BOTEVENT_PLAYER_BFGEXPLODE",
	"BOTEVENT_BFGEXPLODE",
	"BOTEVENT_RECEIVEDMEDAL",
};

//*****************************************************************************
//	CONSOLE VARIABLES

CVAR( Bool, bot_allowchat, true, CVAR_ARCHIVE );
CVAR( Int, botdebug_statechanges, 0, CVAR_ARCHIVE );
CVAR( Int, botdebug_states, 0, CVAR_ARCHIVE );
CVAR( Int, botdebug_commands, 0, CVAR_ARCHIVE );
CVAR( Int, botdebug_obstructiontest, 0, CVAR_ARCHIVE );
CVAR( Int, botdebug_walktest, 0, CVAR_ARCHIVE );
CVAR( Int, botdebug_dataheaders, 0, CVAR_ARCHIVE );
CVAR( Int, botdebug_showstackpushes, 0, CVAR_ARCHIVE );
CVAR( Int, botdebug_showgoal, 0, CVAR_ARCHIVE );
CVAR( Int, botdebug_showcosts, 0, CVAR_ARCHIVE );
CVAR( Int, botdebug_showevents, 0, CVAR_ARCHIVE );
CVAR( Float, botdebug_maxsearchnodes, 1024.0, CVAR_ARCHIVE );
CVAR( Float, botdebug_maxgiveupnodes, 512.0, CVAR_ARCHIVE );
CVAR( Float, botdebug_maxroamgiveupnodes, 4096.0, CVAR_ARCHIVE );

//*****************************************************************************
//
CUSTOM_CVAR( Int, botskill, 2, CVAR_SERVERINFO | CVAR_LATCH )
{
	if ( self < 0 )
		self = 0;
	if ( self > 4 )
		self = 4;
}

//*****************************************************************************
//
CUSTOM_CVAR( Int, botdebug_shownodes, 0, CVAR_ARCHIVE )
{
	if ( self < 0 )
		self = 0;

	if ( self == 0 )
		ASTAR_ClearVisualizations( );
}

//*****************************************************************************
//	PROTOTYPES

void	SERVERCONSOLE_AddNewPlayer( LONG lPlayer );
void	SERVERCONSOLE_RemovePlayer( LONG lPlayer );

// Ugh.
EXTERN_CVAR( Bool, sv_cheats );

//*****************************************************************************
//	FUNCTIONS

void BOTS_Construct( void )
{
	ULONG	ulIdx;

	// Set up all the hardcoded bot info.

	/**************** HUMANS *****************/

	// BOT_CHUBBS	(9/30)
	sprintf( g_HardcodedBotInfo[BOT_CHUBBS].szName, "Chubbs" );
	g_HardcodedBotInfo[BOT_CHUBBS].Accuracy = BOTSKILL_LOW;
	g_HardcodedBotInfo[BOT_CHUBBS].Intellect = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_CHUBBS].Evade = BOTSKILL_LOW;
	g_HardcodedBotInfo[BOT_CHUBBS].Anticipation = BOTSKILL_POOR;
	g_HardcodedBotInfo[BOT_CHUBBS].ReactionTime = BOTSKILL_LOW;
	g_HardcodedBotInfo[BOT_CHUBBS].Perception = BOTSKILL_LOW;
	sprintf( g_HardcodedBotInfo[BOT_CHUBBS].szFavoriteWeapon, "supershotgun" );
	sprintf( g_HardcodedBotInfo[BOT_CHUBBS].szColor, "00 bf 00" );
	sprintf( g_HardcodedBotInfo[BOT_CHUBBS].szGender, "male" );
	sprintf( g_HardcodedBotInfo[BOT_CHUBBS].szSkinName, "big fat doomguy" );
	sprintf( g_HardcodedBotInfo[BOT_CHUBBS].szClassName, "fighter" );
	g_HardcodedBotInfo[BOT_CHUBBS].ulRailgunColor = RAILCOLOR_BLUE;
	g_HardcodedBotInfo[BOT_CHUBBS].ulChatFrequency = 75;
	g_HardcodedBotInfo[BOT_CHUBBS].bRevealed = true;
	g_HardcodedBotInfo[BOT_CHUBBS].bRevealedByDefault = true;
	sprintf( g_HardcodedBotInfo[BOT_CHUBBS].szScriptName, "FATBOT" );
	g_HardcodedBotInfo[BOT_CHUBBS].szChatFile[0] = 0;
	sprintf( g_HardcodedBotInfo[BOT_CHUBBS].szChatLump, "bots/chatfiles/chubbs.txt" );

	// BOT_DEIMOS	(13/30)
	sprintf( g_HardcodedBotInfo[BOT_DEIMOS].szName, "Deimos" );
	g_HardcodedBotInfo[BOT_DEIMOS].Accuracy = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_DEIMOS].Intellect = BOTSKILL_HIGH;
	g_HardcodedBotInfo[BOT_DEIMOS].Evade = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_DEIMOS].Anticipation = BOTSKILL_HIGH;
	g_HardcodedBotInfo[BOT_DEIMOS].ReactionTime = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_DEIMOS].Perception = BOTSKILL_MEDIUM;
	sprintf( g_HardcodedBotInfo[BOT_DEIMOS].szFavoriteWeapon, "supershotgun" );
	sprintf( g_HardcodedBotInfo[BOT_DEIMOS].szColor, "5f bf c0" );
	sprintf( g_HardcodedBotInfo[BOT_DEIMOS].szGender, "male" );
	sprintf( g_HardcodedBotInfo[BOT_DEIMOS].szSkinName, "" );
	sprintf( g_HardcodedBotInfo[BOT_DEIMOS].szClassName, "cleric" );
	g_HardcodedBotInfo[BOT_DEIMOS].ulRailgunColor = RAILCOLOR_WHITE;
	g_HardcodedBotInfo[BOT_DEIMOS].ulChatFrequency = 33;
	g_HardcodedBotInfo[BOT_DEIMOS].bRevealed = true;
	g_HardcodedBotInfo[BOT_DEIMOS].bRevealedByDefault = true;
//	sprintf( g_HardcodedBotInfo[BOT_DEIMOS].szScriptName, "SOLDRBOT" );
	sprintf( g_HardcodedBotInfo[BOT_DEIMOS].szScriptName, "HUMANBOT" );
	g_HardcodedBotInfo[BOT_DEIMOS].szChatFile[0] = 0;
	sprintf( g_HardcodedBotInfo[BOT_DEIMOS].szChatLump, "bots/chatfiles/soldier.txt" );

	// BOT_ALDEBARAN	(13/30)
	sprintf( g_HardcodedBotInfo[BOT_ALDEBARAN].szName, "Aldebaran" );
	g_HardcodedBotInfo[BOT_ALDEBARAN].Accuracy = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_ALDEBARAN].Intellect = BOTSKILL_HIGH;
	g_HardcodedBotInfo[BOT_ALDEBARAN].Evade = BOTSKILL_HIGH;
	g_HardcodedBotInfo[BOT_ALDEBARAN].Anticipation = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_ALDEBARAN].ReactionTime = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_ALDEBARAN].Perception = BOTSKILL_MEDIUM;
	sprintf( g_HardcodedBotInfo[BOT_ALDEBARAN].szFavoriteWeapon, "rocketlauncher" );
	sprintf( g_HardcodedBotInfo[BOT_ALDEBARAN].szColor, "ff 60 00" );
	sprintf( g_HardcodedBotInfo[BOT_ALDEBARAN].szGender, "male" );
	sprintf( g_HardcodedBotInfo[BOT_ALDEBARAN].szSkinName, "phobos" );
	sprintf( g_HardcodedBotInfo[BOT_ALDEBARAN].szClassName, "cleric" );
	g_HardcodedBotInfo[BOT_ALDEBARAN].ulRailgunColor = RAILCOLOR_ORANGE;
	g_HardcodedBotInfo[BOT_ALDEBARAN].ulChatFrequency = 33;
	g_HardcodedBotInfo[BOT_ALDEBARAN].bRevealed = true;
	g_HardcodedBotInfo[BOT_ALDEBARAN].bRevealedByDefault = true;
//	sprintf( g_HardcodedBotInfo[BOT_ALDEBARAN].szScriptName, "SOLDRBOT" );
	sprintf( g_HardcodedBotInfo[BOT_ALDEBARAN].szScriptName, "HUMANBOT" );
	g_HardcodedBotInfo[BOT_ALDEBARAN].szChatFile[0] = 0;
	sprintf( g_HardcodedBotInfo[BOT_ALDEBARAN].szChatLump, "bots/chatfiles/soldier.txt" );

	// BOT_CRASH	(6/30)
	sprintf( g_HardcodedBotInfo[BOT_CRASH].szName, "Crash" );
	g_HardcodedBotInfo[BOT_CRASH].Accuracy = BOTSKILL_LOW;
	g_HardcodedBotInfo[BOT_CRASH].Intellect = BOTSKILL_LOW;
	g_HardcodedBotInfo[BOT_CRASH].Evade = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_CRASH].Anticipation = BOTSKILL_LOW;
	g_HardcodedBotInfo[BOT_CRASH].ReactionTime = BOTSKILL_LOW;
	g_HardcodedBotInfo[BOT_CRASH].Perception = BOTSKILL_LOW;
	sprintf( g_HardcodedBotInfo[BOT_CRASH].szFavoriteWeapon, "grenadelauncher" );
	sprintf( g_HardcodedBotInfo[BOT_CRASH].szColor, "00 00 bf" );
	sprintf( g_HardcodedBotInfo[BOT_CRASH].szGender, "female" );
	sprintf( g_HardcodedBotInfo[BOT_CRASH].szSkinName, "crash" );
	sprintf( g_HardcodedBotInfo[BOT_CRASH].szClassName, "mage" );
	g_HardcodedBotInfo[BOT_CRASH].ulRailgunColor = RAILCOLOR_PURPLE;
	g_HardcodedBotInfo[BOT_CRASH].ulChatFrequency = 90;
	g_HardcodedBotInfo[BOT_CRASH].bRevealed = true;
	g_HardcodedBotInfo[BOT_CRASH].bRevealedByDefault = true;
	sprintf( g_HardcodedBotInfo[BOT_CRASH].szScriptName, "CRASHBOT" );
	g_HardcodedBotInfo[BOT_CRASH].szChatFile[0] = 0;
	sprintf( g_HardcodedBotInfo[BOT_CRASH].szChatLump, "bots/chatfiles/crash.txt" );

	// BOT_PROCYON	(16/30)
	sprintf( g_HardcodedBotInfo[BOT_PROCYON].szName, "Procyon" );
	g_HardcodedBotInfo[BOT_PROCYON].Accuracy = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_PROCYON].Intellect = BOTSKILL_HIGH;
	g_HardcodedBotInfo[BOT_PROCYON].Evade = BOTSKILL_HIGH;
	g_HardcodedBotInfo[BOT_PROCYON].Anticipation = BOTSKILL_EXCELLENT;
	g_HardcodedBotInfo[BOT_PROCYON].ReactionTime = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_PROCYON].Perception = BOTSKILL_MEDIUM;
	sprintf( g_HardcodedBotInfo[BOT_PROCYON].szFavoriteWeapon, "minigun" );
	sprintf( g_HardcodedBotInfo[BOT_PROCYON].szColor, "9f 6f 3f" );
	sprintf( g_HardcodedBotInfo[BOT_PROCYON].szGender, "male" );
	sprintf( g_HardcodedBotInfo[BOT_PROCYON].szSkinName, "procyon" );
	sprintf( g_HardcodedBotInfo[BOT_PROCYON].szClassName, "cleric" );
	g_HardcodedBotInfo[BOT_PROCYON].ulRailgunColor = RAILCOLOR_RED;
	g_HardcodedBotInfo[BOT_PROCYON].ulChatFrequency = 33;
	g_HardcodedBotInfo[BOT_PROCYON].bRevealed = true;
	g_HardcodedBotInfo[BOT_PROCYON].bRevealedByDefault = true;
//	sprintf( g_HardcodedBotInfo[BOT_PROCYON].szScriptName, "SOLDRBOT" );
	sprintf( g_HardcodedBotInfo[BOT_PROCYON].szScriptName, "HUMANBOT" );
	g_HardcodedBotInfo[BOT_PROCYON].szChatFile[0] = 0;
	sprintf( g_HardcodedBotInfo[BOT_PROCYON].szChatLump, "bots/chatfiles/soldier.txt" );

	// BOT_SIRIUS	(17/25)
	sprintf( g_HardcodedBotInfo[BOT_SIRIUS].szName, "Sirius" );
	g_HardcodedBotInfo[BOT_SIRIUS].Accuracy = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_SIRIUS].Intellect = BOTSKILL_EXCELLENT;
	g_HardcodedBotInfo[BOT_SIRIUS].Evade = BOTSKILL_EXCELLENT;
	g_HardcodedBotInfo[BOT_SIRIUS].Anticipation = BOTSKILL_EXCELLENT;
	g_HardcodedBotInfo[BOT_SIRIUS].ReactionTime = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_SIRIUS].Perception = BOTSKILL_MEDIUM;
	sprintf( g_HardcodedBotInfo[BOT_SIRIUS].szFavoriteWeapon, "rocketlauncher" );
	sprintf( g_HardcodedBotInfo[BOT_SIRIUS].szColor, "90 90 ff" );
	sprintf( g_HardcodedBotInfo[BOT_SIRIUS].szGender, "male" );
	sprintf( g_HardcodedBotInfo[BOT_SIRIUS].szSkinName, "Base II" );
	sprintf( g_HardcodedBotInfo[BOT_SIRIUS].szClassName, "cleric" );
	g_HardcodedBotInfo[BOT_SIRIUS].ulRailgunColor = RAILCOLOR_RED;
	g_HardcodedBotInfo[BOT_SIRIUS].ulChatFrequency = 33;
	g_HardcodedBotInfo[BOT_SIRIUS].bRevealed = true;
	g_HardcodedBotInfo[BOT_SIRIUS].bRevealedByDefault = true;
//	sprintf( g_HardcodedBotInfo[BOT_SIRIUS].szScriptName, "SOLDRBOT" );
	sprintf( g_HardcodedBotInfo[BOT_SIRIUS].szScriptName, "HUMANBOT" );
	g_HardcodedBotInfo[BOT_SIRIUS].szChatFile[0] = 0;
	sprintf( g_HardcodedBotInfo[BOT_SIRIUS].szChatLump, "bots/chatfiles/soldier.txt" );

	// BOT_RIGEL	(18/30)
	sprintf( g_HardcodedBotInfo[BOT_RIGEL].szName, "Rigel" );
	g_HardcodedBotInfo[BOT_RIGEL].Accuracy = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_RIGEL].Intellect = BOTSKILL_EXCELLENT;
	g_HardcodedBotInfo[BOT_RIGEL].Evade = BOTSKILL_HIGH;
	g_HardcodedBotInfo[BOT_RIGEL].Anticipation = BOTSKILL_EXCELLENT;
	g_HardcodedBotInfo[BOT_RIGEL].ReactionTime = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_RIGEL].Perception = BOTSKILL_HIGH;
	sprintf( g_HardcodedBotInfo[BOT_RIGEL].szFavoriteWeapon, "rocketlauncher" );
	sprintf( g_HardcodedBotInfo[BOT_RIGEL].szColor, "00 00 ff" );
	sprintf( g_HardcodedBotInfo[BOT_RIGEL].szGender, "male" );
	sprintf( g_HardcodedBotInfo[BOT_RIGEL].szSkinName, "Base II" );
	sprintf( g_HardcodedBotInfo[BOT_RIGEL].szClassName, "cleric" );
	g_HardcodedBotInfo[BOT_RIGEL].ulRailgunColor = RAILCOLOR_RED;
	g_HardcodedBotInfo[BOT_RIGEL].ulChatFrequency = 33;
	g_HardcodedBotInfo[BOT_RIGEL].bRevealed = true;
	g_HardcodedBotInfo[BOT_RIGEL].bRevealedByDefault = true;
//	sprintf( g_HardcodedBotInfo[BOT_RIGEL].szScriptName, "SOLDRBOT" );
	sprintf( g_HardcodedBotInfo[BOT_RIGEL].szScriptName, "HUMANBOT" );
	g_HardcodedBotInfo[BOT_RIGEL].szChatFile[0] = 0;
	sprintf( g_HardcodedBotInfo[BOT_RIGEL].szChatLump, "bots/chatfiles/soldier.txt" );

	// BOT_SEENAS	(14/30)
	sprintf( g_HardcodedBotInfo[BOT_SEENAS].szName, "Seenas" );
	g_HardcodedBotInfo[BOT_SEENAS].Accuracy = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_SEENAS].Intellect = BOTSKILL_HIGH;
	g_HardcodedBotInfo[BOT_SEENAS].Evade = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_SEENAS].Anticipation = BOTSKILL_HIGH;
	g_HardcodedBotInfo[BOT_SEENAS].ReactionTime = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_SEENAS].Perception = BOTSKILL_MEDIUM;
	sprintf( g_HardcodedBotInfo[BOT_SEENAS].szFavoriteWeapon, "rocketlauncher" );
	sprintf( g_HardcodedBotInfo[BOT_SEENAS].szColor, "b0 af 4f" );
	sprintf( g_HardcodedBotInfo[BOT_SEENAS].szGender, "male" );
	sprintf( g_HardcodedBotInfo[BOT_SEENAS].szSkinName, "seenas" );
	sprintf( g_HardcodedBotInfo[BOT_SEENAS].szClassName, "cleric" );
	g_HardcodedBotInfo[BOT_SEENAS].ulRailgunColor = RAILCOLOR_RED;
	g_HardcodedBotInfo[BOT_SEENAS].ulChatFrequency = 50;
	g_HardcodedBotInfo[BOT_SEENAS].bRevealed = true;
	g_HardcodedBotInfo[BOT_SEENAS].bRevealedByDefault = true;
//	sprintf( g_HardcodedBotInfo[BOT_SEENAS].szScriptName, "SOLDRBOT" );
	sprintf( g_HardcodedBotInfo[BOT_SEENAS].szScriptName, "HUMANBOT" );
	g_HardcodedBotInfo[BOT_SEENAS].szChatFile[0] = 0;
	sprintf( g_HardcodedBotInfo[BOT_SEENAS].szChatLump, "bots/chatfiles/soldier.txt" );

	// BOT_SYNAS	(18/30)
	sprintf( g_HardcodedBotInfo[BOT_SYNAS].szName, "Synas" );
	g_HardcodedBotInfo[BOT_SYNAS].Accuracy = BOTSKILL_HIGH;
	g_HardcodedBotInfo[BOT_SYNAS].Intellect = BOTSKILL_EXCELLENT;
	g_HardcodedBotInfo[BOT_SYNAS].Evade = BOTSKILL_EXCELLENT;
	g_HardcodedBotInfo[BOT_SYNAS].Anticipation = BOTSKILL_EXCELLENT;
	g_HardcodedBotInfo[BOT_SYNAS].ReactionTime = BOTSKILL_HIGH;
	g_HardcodedBotInfo[BOT_SYNAS].Perception = BOTSKILL_HIGH;
	sprintf( g_HardcodedBotInfo[BOT_SYNAS].szFavoriteWeapon, "grenadelauncher" );
	sprintf( g_HardcodedBotInfo[BOT_SYNAS].szColor, "8f 00 00" );
	sprintf( g_HardcodedBotInfo[BOT_SYNAS].szGender, "male" );
	sprintf( g_HardcodedBotInfo[BOT_SYNAS].szSkinName, "synas" );
	sprintf( g_HardcodedBotInfo[BOT_SYNAS].szClassName, "fighter" );
	g_HardcodedBotInfo[BOT_SYNAS].ulRailgunColor = RAILCOLOR_RED;
	g_HardcodedBotInfo[BOT_SYNAS].ulChatFrequency = 33;
	g_HardcodedBotInfo[BOT_SYNAS].bRevealed = true;
	g_HardcodedBotInfo[BOT_SYNAS].bRevealedByDefault = true;
//	sprintf( g_HardcodedBotInfo[BOT_SYNAS].szScriptName, "SOLDRBOT" );
	sprintf( g_HardcodedBotInfo[BOT_SYNAS].szScriptName, "HUMANBOT" );
	g_HardcodedBotInfo[BOT_SYNAS].szChatFile[0] = 0;
	sprintf( g_HardcodedBotInfo[BOT_SYNAS].szChatLump, "bots/chatfiles/soldier.txt" );

	// BOT_CYGNUS	(19/30)
	sprintf( g_HardcodedBotInfo[BOT_CYGNUS].szName, "Cygnus" );
	g_HardcodedBotInfo[BOT_CYGNUS].Accuracy = BOTSKILL_SUPREME;
	g_HardcodedBotInfo[BOT_CYGNUS].Intellect = BOTSKILL_SUPREME;
	g_HardcodedBotInfo[BOT_CYGNUS].Evade = BOTSKILL_SUPREME;
	g_HardcodedBotInfo[BOT_CYGNUS].Anticipation = BOTSKILL_SUPREME;
	g_HardcodedBotInfo[BOT_CYGNUS].ReactionTime = BOTSKILL_POOR;
	g_HardcodedBotInfo[BOT_CYGNUS].Perception = BOTSKILL_POOR;
	sprintf( g_HardcodedBotInfo[BOT_CYGNUS].szFavoriteWeapon, "supershotgun" );
	sprintf( g_HardcodedBotInfo[BOT_CYGNUS].szColor, "c0 bf 6f" );
	sprintf( g_HardcodedBotInfo[BOT_CYGNUS].szGender, "male" );
	sprintf( g_HardcodedBotInfo[BOT_CYGNUS].szSkinName, "cygnus" );
	sprintf( g_HardcodedBotInfo[BOT_CYGNUS].szClassName, "fighter" );
	g_HardcodedBotInfo[BOT_CYGNUS].ulRailgunColor = RAILCOLOR_SILVER;
	g_HardcodedBotInfo[BOT_CYGNUS].ulChatFrequency = 15;
	g_HardcodedBotInfo[BOT_CYGNUS].bRevealed = true;
	g_HardcodedBotInfo[BOT_CYGNUS].bRevealedByDefault = true;
//	sprintf( g_HardcodedBotInfo[BOT_CYGNUS].szScriptName, "CYGNSBOT" );
	sprintf( g_HardcodedBotInfo[BOT_CYGNUS].szScriptName, "HUMANBOT" );
	g_HardcodedBotInfo[BOT_CYGNUS].szChatFile[0] = 0;
	sprintf( g_HardcodedBotInfo[BOT_CYGNUS].szChatLump, "bots/chatfiles/cygnus.txt" );

	/**************** ROBOTS *****************/

	// BOT_ALPHUS	(18/30)
	sprintf( g_HardcodedBotInfo[BOT_ALPHUS].szName, "Alphus" );
	g_HardcodedBotInfo[BOT_ALPHUS].Accuracy = BOTSKILL_HIGH;
	g_HardcodedBotInfo[BOT_ALPHUS].Intellect = BOTSKILL_EXCELLENT;
	g_HardcodedBotInfo[BOT_ALPHUS].Evade = BOTSKILL_EXCELLENT;
	g_HardcodedBotInfo[BOT_ALPHUS].Anticipation = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_ALPHUS].ReactionTime = BOTSKILL_VERYPOOR;
	g_HardcodedBotInfo[BOT_ALPHUS].Perception = BOTSKILL_VERYPOOR;
	sprintf( g_HardcodedBotInfo[BOT_ALPHUS].szFavoriteWeapon, "supershotgun" );
	sprintf( g_HardcodedBotInfo[BOT_ALPHUS].szColor, "c0 bf 6f" );
	sprintf( g_HardcodedBotInfo[BOT_ALPHUS].szGender, "male" );
	sprintf( g_HardcodedBotInfo[BOT_ALPHUS].szSkinName, "gamma" );
	sprintf( g_HardcodedBotInfo[BOT_ALPHUS].szClassName, "fighter" );
	g_HardcodedBotInfo[BOT_ALPHUS].ulRailgunColor = RAILCOLOR_SILVER;
	g_HardcodedBotInfo[BOT_ALPHUS].ulChatFrequency = 20;
	g_HardcodedBotInfo[BOT_ALPHUS].bRevealed = true;
	g_HardcodedBotInfo[BOT_ALPHUS].bRevealedByDefault = true;
//	sprintf( g_HardcodedBotInfo[BOT_ALPHUS].szScriptName, "ROBOBOT" );
	sprintf( g_HardcodedBotInfo[BOT_ALPHUS].szScriptName, "DFULTBOT" );
	g_HardcodedBotInfo[BOT_ALPHUS].szChatFile[0] = 0;
	sprintf( g_HardcodedBotInfo[BOT_ALPHUS].szChatLump, "bots/chatfiles/robot.txt" );

	// BOT_PROTOS
	sprintf( g_HardcodedBotInfo[BOT_PROTOS].szName, "Protos" );
	g_HardcodedBotInfo[BOT_PROTOS].Accuracy = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_PROTOS].Intellect = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_PROTOS].Evade = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_PROTOS].Anticipation = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_PROTOS].ReactionTime = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_PROTOS].Perception = BOTSKILL_MEDIUM;
	sprintf( g_HardcodedBotInfo[BOT_PROTOS].szFavoriteWeapon, "supershotgun" );
	sprintf( g_HardcodedBotInfo[BOT_PROTOS].szColor, "00 00 00" );
	sprintf( g_HardcodedBotInfo[BOT_PROTOS].szGender, "cyborg" );
	sprintf( g_HardcodedBotInfo[BOT_PROTOS].szSkinName, "" );
	sprintf( g_HardcodedBotInfo[BOT_PROTOS].szClassName, "fighter" );
	g_HardcodedBotInfo[BOT_PROTOS].ulRailgunColor = RAILCOLOR_YELLOW;
	g_HardcodedBotInfo[BOT_PROTOS].ulChatFrequency = 20;
	g_HardcodedBotInfo[BOT_PROTOS].bRevealed = true;
	g_HardcodedBotInfo[BOT_PROTOS].bRevealedByDefault = true;
//	sprintf( g_HardcodedBotInfo[BOT_PROTOS].szScriptName, "ROBOBOT" );
	sprintf( g_HardcodedBotInfo[BOT_PROTOS].szScriptName, "DFULTBOT" );
	g_HardcodedBotInfo[BOT_PROTOS].szChatFile[0] = 0;
	sprintf( g_HardcodedBotInfo[BOT_PROTOS].szChatLump, "bots/chatfiles/robot.txt" );

	// BOT_BETUS	(18/30)
	sprintf( g_HardcodedBotInfo[BOT_BETUS].szName, "Betus" );
	g_HardcodedBotInfo[BOT_BETUS].Accuracy = BOTSKILL_HIGH;
	g_HardcodedBotInfo[BOT_BETUS].Intellect = BOTSKILL_EXCELLENT;
	g_HardcodedBotInfo[BOT_BETUS].Evade = BOTSKILL_EXCELLENT;
	g_HardcodedBotInfo[BOT_BETUS].Anticipation = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_BETUS].ReactionTime = BOTSKILL_VERYPOOR;
	g_HardcodedBotInfo[BOT_BETUS].Perception = BOTSKILL_VERYPOOR;
	sprintf( g_HardcodedBotInfo[BOT_BETUS].szFavoriteWeapon, "supershotgun" );
	sprintf( g_HardcodedBotInfo[BOT_BETUS].szColor, "c0 bf 6f" );
	sprintf( g_HardcodedBotInfo[BOT_BETUS].szGender, "male" );
	sprintf( g_HardcodedBotInfo[BOT_BETUS].szSkinName, "gamma" );
	sprintf( g_HardcodedBotInfo[BOT_BETUS].szClassName, "fighter" );
	g_HardcodedBotInfo[BOT_BETUS].ulRailgunColor = RAILCOLOR_SILVER;
	g_HardcodedBotInfo[BOT_BETUS].ulChatFrequency = 20;
	g_HardcodedBotInfo[BOT_BETUS].bRevealed = true;
	g_HardcodedBotInfo[BOT_BETUS].bRevealedByDefault = true;
//	sprintf( g_HardcodedBotInfo[BOT_BETUS].szScriptName, "ROBOBOT" );
	sprintf( g_HardcodedBotInfo[BOT_BETUS].szScriptName, "DFULTBOT" );
	g_HardcodedBotInfo[BOT_BETUS].szChatFile[0] = 0;
	sprintf( g_HardcodedBotInfo[BOT_BETUS].szChatLump, "bots/chatfiles/robot.txt" );

	// BOT_SCYON
	sprintf( g_HardcodedBotInfo[BOT_SCYON].szName, "Scyon" );
	g_HardcodedBotInfo[BOT_SCYON].Accuracy = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_SCYON].Intellect = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_SCYON].Evade = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_SCYON].Anticipation = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_SCYON].ReactionTime = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_SCYON].Perception = BOTSKILL_MEDIUM;
	sprintf( g_HardcodedBotInfo[BOT_SCYON].szFavoriteWeapon, "rocketlauncher" );
	sprintf( g_HardcodedBotInfo[BOT_SCYON].szColor, "00 00 00" );
	sprintf( g_HardcodedBotInfo[BOT_SCYON].szGender, "cyborg" );
	sprintf( g_HardcodedBotInfo[BOT_SCYON].szSkinName, "" );
	sprintf( g_HardcodedBotInfo[BOT_SCYON].szClassName, "random" );
	g_HardcodedBotInfo[BOT_SCYON].ulRailgunColor = RAILCOLOR_RED;
	g_HardcodedBotInfo[BOT_SCYON].ulChatFrequency = 20;
	g_HardcodedBotInfo[BOT_SCYON].bRevealed = true;
	g_HardcodedBotInfo[BOT_SCYON].bRevealedByDefault = true;
//	sprintf( g_HardcodedBotInfo[BOT_SCYON].szScriptName, "ROBOBOT" );
	sprintf( g_HardcodedBotInfo[BOT_SCYON].szScriptName, "DFULTBOT" );
	g_HardcodedBotInfo[BOT_SCYON].szChatFile[0] = 0;
	sprintf( g_HardcodedBotInfo[BOT_SCYON].szChatLump, "bots/chatfiles/robot.txt" );

	// BOT_GAMMA	(18/30)
	sprintf( g_HardcodedBotInfo[BOT_GAMMA].szName, "Gamma" );
	g_HardcodedBotInfo[BOT_GAMMA].Accuracy = BOTSKILL_HIGH;
	g_HardcodedBotInfo[BOT_GAMMA].Intellect = BOTSKILL_EXCELLENT;
	g_HardcodedBotInfo[BOT_GAMMA].Evade = BOTSKILL_EXCELLENT;
	g_HardcodedBotInfo[BOT_GAMMA].Anticipation = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_GAMMA].ReactionTime = BOTSKILL_VERYPOOR;
	g_HardcodedBotInfo[BOT_GAMMA].Perception = BOTSKILL_VERYPOOR;
	sprintf( g_HardcodedBotInfo[BOT_GAMMA].szFavoriteWeapon, "supershotgun" );
	sprintf( g_HardcodedBotInfo[BOT_GAMMA].szColor, "c0 bf 6f" );
	sprintf( g_HardcodedBotInfo[BOT_GAMMA].szGender, "male" );
	sprintf( g_HardcodedBotInfo[BOT_GAMMA].szSkinName, "gamma" );
	sprintf( g_HardcodedBotInfo[BOT_GAMMA].szClassName, "fighter" );
	g_HardcodedBotInfo[BOT_GAMMA].ulRailgunColor = RAILCOLOR_SILVER;
	g_HardcodedBotInfo[BOT_GAMMA].ulChatFrequency = 20;
	g_HardcodedBotInfo[BOT_GAMMA].bRevealed = true;
	g_HardcodedBotInfo[BOT_GAMMA].bRevealedByDefault = true;
//	sprintf( g_HardcodedBotInfo[BOT_GAMMA].szScriptName, "ROBOBOT" );
	sprintf( g_HardcodedBotInfo[BOT_GAMMA].szScriptName, "DFULTBOT" );
	g_HardcodedBotInfo[BOT_GAMMA].szChatFile[0] = 0;
	sprintf( g_HardcodedBotInfo[BOT_GAMMA].szChatLump, "bots/chatfiles/robot.txt" );

	// BOT_SCYTHE
	sprintf( g_HardcodedBotInfo[BOT_SCYTHE].szName, "Scythe" );
	g_HardcodedBotInfo[BOT_SCYTHE].Accuracy = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_SCYTHE].Intellect = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_SCYTHE].Evade = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_SCYTHE].Anticipation = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_SCYTHE].ReactionTime = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_SCYTHE].Perception = BOTSKILL_MEDIUM;
	sprintf( g_HardcodedBotInfo[BOT_SCYTHE].szFavoriteWeapon, "grenadelauncher" );
	sprintf( g_HardcodedBotInfo[BOT_SCYTHE].szColor, "00 00 00" );
	sprintf( g_HardcodedBotInfo[BOT_SCYTHE].szGender, "female" );
	sprintf( g_HardcodedBotInfo[BOT_SCYTHE].szSkinName, "" );
	sprintf( g_HardcodedBotInfo[BOT_SCYTHE].szClassName, "random" );
	g_HardcodedBotInfo[BOT_SCYTHE].ulRailgunColor = RAILCOLOR_BLUE;
	g_HardcodedBotInfo[BOT_SCYTHE].ulChatFrequency = 20;
	g_HardcodedBotInfo[BOT_SCYTHE].bRevealed = true;
	g_HardcodedBotInfo[BOT_SCYTHE].bRevealedByDefault = true;
//	sprintf( g_HardcodedBotInfo[BOT_SCYTHE].szScriptName, "ROBOBOT" );
	sprintf( g_HardcodedBotInfo[BOT_SCYTHE].szScriptName, "DFULTBOT" );
	g_HardcodedBotInfo[BOT_SCYTHE].szChatFile[0] = 0;
	sprintf( g_HardcodedBotInfo[BOT_SCYTHE].szChatLump, "bots/chatfiles/robot.txt" );

	// BOT_ELECTRA
	sprintf( g_HardcodedBotInfo[BOT_ELECTRA].szName, "Electra" );
	g_HardcodedBotInfo[BOT_ELECTRA].Accuracy = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_ELECTRA].Intellect = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_ELECTRA].Evade = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_ELECTRA].Anticipation = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_ELECTRA].ReactionTime = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_ELECTRA].Perception = BOTSKILL_MEDIUM;
	sprintf( g_HardcodedBotInfo[BOT_ELECTRA].szFavoriteWeapon, "railgun" );
	sprintf( g_HardcodedBotInfo[BOT_ELECTRA].szColor, "df 00 df" );
	sprintf( g_HardcodedBotInfo[BOT_ELECTRA].szGender, "female" );
	sprintf( g_HardcodedBotInfo[BOT_ELECTRA].szSkinName, "" );
	sprintf( g_HardcodedBotInfo[BOT_ELECTRA].szClassName, "mage" );
	g_HardcodedBotInfo[BOT_ELECTRA].ulRailgunColor = RAILCOLOR_PURPLE;
	g_HardcodedBotInfo[BOT_ELECTRA].ulChatFrequency = 20;
	g_HardcodedBotInfo[BOT_ELECTRA].bRevealed = true;
	g_HardcodedBotInfo[BOT_ELECTRA].bRevealedByDefault = true;
//	sprintf( g_HardcodedBotInfo[BOT_ELECTRA].szScriptName, "ROBOBOT" );
	sprintf( g_HardcodedBotInfo[BOT_ELECTRA].szScriptName, "DFULTBOT" );
	g_HardcodedBotInfo[BOT_ELECTRA].szChatFile[0] = 0;
	sprintf( g_HardcodedBotInfo[BOT_ELECTRA].szChatLump, "bots/chatfiles/robot.txt" );

	// BOT_OMICRON
	sprintf( g_HardcodedBotInfo[BOT_OMICRON].szName, "Omicron" );
	g_HardcodedBotInfo[BOT_OMICRON].Accuracy = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_OMICRON].Intellect = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_OMICRON].Evade = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_OMICRON].Anticipation = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_OMICRON].ReactionTime = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_OMICRON].Perception = BOTSKILL_MEDIUM;
	sprintf( g_HardcodedBotInfo[BOT_OMICRON].szFavoriteWeapon, "grenadelauncher" );
	sprintf( g_HardcodedBotInfo[BOT_OMICRON].szColor, "00 00 00" );
	sprintf( g_HardcodedBotInfo[BOT_OMICRON].szGender, "female" );
	sprintf( g_HardcodedBotInfo[BOT_OMICRON].szSkinName, "" );
	sprintf( g_HardcodedBotInfo[BOT_OMICRON].szClassName, "random" );
	g_HardcodedBotInfo[BOT_OMICRON].ulRailgunColor = RAILCOLOR_BLUE;
	g_HardcodedBotInfo[BOT_OMICRON].ulChatFrequency = 20;
	g_HardcodedBotInfo[BOT_OMICRON].bRevealed = true;
	g_HardcodedBotInfo[BOT_OMICRON].bRevealedByDefault = true;
//	sprintf( g_HardcodedBotInfo[BOT_OMICRON].szScriptName, "ROBOBOT" );
	sprintf( g_HardcodedBotInfo[BOT_OMICRON].szScriptName, "DFULTBOT" );
	g_HardcodedBotInfo[BOT_OMICRON].szChatFile[0] = 0;
	sprintf( g_HardcodedBotInfo[BOT_OMICRON].szChatLump, "bots/chatfiles/robot.txt" );

	// BOT_CRYON
	sprintf( g_HardcodedBotInfo[BOT_CRYON].szName, "Cryon" );
	g_HardcodedBotInfo[BOT_CRYON].Accuracy = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_CRYON].Intellect = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_CRYON].Evade = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_CRYON].Anticipation = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_CRYON].ReactionTime = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_CRYON].Perception = BOTSKILL_MEDIUM;
	sprintf( g_HardcodedBotInfo[BOT_CRYON].szFavoriteWeapon, "railgun" );
	sprintf( g_HardcodedBotInfo[BOT_CRYON].szColor, "00 00 00" );
	sprintf( g_HardcodedBotInfo[BOT_CRYON].szGender, "cyborg" );
	sprintf( g_HardcodedBotInfo[BOT_CRYON].szSkinName, "" );
	sprintf( g_HardcodedBotInfo[BOT_CRYON].szClassName, "fighter" );
	g_HardcodedBotInfo[BOT_CRYON].ulRailgunColor = RAILCOLOR_BLACK;
	g_HardcodedBotInfo[BOT_CRYON].ulChatFrequency = 20;
	g_HardcodedBotInfo[BOT_CRYON].bRevealed = true;
	g_HardcodedBotInfo[BOT_CRYON].bRevealedByDefault = true;
//	sprintf( g_HardcodedBotInfo[BOT_CRYON].szScriptName, "ROBOBOT" );
	sprintf( g_HardcodedBotInfo[BOT_CRYON].szScriptName, "DFULTBOT" );
	g_HardcodedBotInfo[BOT_CRYON].szChatFile[0] = 0;
	sprintf( g_HardcodedBotInfo[BOT_CRYON].szChatLump, "bots/chatfiles/robot.txt" );

	// BOT_OMEGA
	sprintf( g_HardcodedBotInfo[BOT_OMEGA].szName, "Omega" );
	g_HardcodedBotInfo[BOT_OMEGA].Accuracy = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_OMEGA].Intellect = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_OMEGA].Evade = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_OMEGA].Anticipation = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_OMEGA].ReactionTime = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_OMEGA].Perception = BOTSKILL_MEDIUM;
	sprintf( g_HardcodedBotInfo[BOT_OMEGA].szFavoriteWeapon, "railgun" );
	sprintf( g_HardcodedBotInfo[BOT_OMEGA].szColor, "00 00 00" );
	sprintf( g_HardcodedBotInfo[BOT_OMEGA].szGender, "cyborg" );
	sprintf( g_HardcodedBotInfo[BOT_OMEGA].szSkinName, "" );
	sprintf( g_HardcodedBotInfo[BOT_OMEGA].szClassName, "fighter" );
	g_HardcodedBotInfo[BOT_OMEGA].ulRailgunColor = RAILCOLOR_BLACK;
	g_HardcodedBotInfo[BOT_OMEGA].ulChatFrequency = 20;
	g_HardcodedBotInfo[BOT_OMEGA].bRevealed = true;
	g_HardcodedBotInfo[BOT_OMEGA].bRevealedByDefault = true;
//	sprintf( g_HardcodedBotInfo[BOT_OMEGA].szScriptName, "ROBOBOT" );
	sprintf( g_HardcodedBotInfo[BOT_OMEGA].szScriptName, "DFULTBOT" );
	g_HardcodedBotInfo[BOT_OMEGA].szChatFile[0] = 0;
	sprintf( g_HardcodedBotInfo[BOT_OMEGA].szChatLump, "bots/chatfiles/robot.txt" );

	/**************** INSECT BOSSES *****************/

	// BOT_PREY		(18/30)
	sprintf( g_HardcodedBotInfo[BOT_PREY].szName, "Prey" );
	g_HardcodedBotInfo[BOT_PREY].Accuracy = BOTSKILL_LOW;
	g_HardcodedBotInfo[BOT_PREY].Intellect = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_PREY].Evade = BOTSKILL_SUPREME;
	g_HardcodedBotInfo[BOT_PREY].Anticipation = BOTSKILL_LOW;
	g_HardcodedBotInfo[BOT_PREY].ReactionTime = BOTSKILL_EXCELLENT;
	g_HardcodedBotInfo[BOT_PREY].Perception = BOTSKILL_EXCELLENT;
	sprintf( g_HardcodedBotInfo[BOT_PREY].szFavoriteWeapon, "railgun" );
	sprintf( g_HardcodedBotInfo[BOT_PREY].szColor, "00 5f 00" );
	sprintf( g_HardcodedBotInfo[BOT_PREY].szGender, "cyborg" );
	sprintf( g_HardcodedBotInfo[BOT_PREY].szSkinName, "" );
	sprintf( g_HardcodedBotInfo[BOT_PREY].szClassName, "mage" );
	g_HardcodedBotInfo[BOT_PREY].ulRailgunColor = RAILCOLOR_GREEN;
	g_HardcodedBotInfo[BOT_PREY].ulChatFrequency = 50;
	g_HardcodedBotInfo[BOT_PREY].bRevealed = true;
	g_HardcodedBotInfo[BOT_PREY].bRevealedByDefault = true;
//	sprintf( g_HardcodedBotInfo[BOT_PREY].szScriptName, "INSCTBOT" );
	sprintf( g_HardcodedBotInfo[BOT_PREY].szScriptName, "DFULTBOT" );
	g_HardcodedBotInfo[BOT_PREY].szChatFile[0] = 0;
	sprintf( g_HardcodedBotInfo[BOT_PREY].szChatLump, "bots/chatfiles/insect.txt" );

	// BOT_MANEK	(21/30)
	sprintf( g_HardcodedBotInfo[BOT_MANEK].szName, "Manek" );
	g_HardcodedBotInfo[BOT_MANEK].Accuracy = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_MANEK].Intellect = BOTSKILL_SUPREME;
	g_HardcodedBotInfo[BOT_MANEK].Evade = BOTSKILL_EXCELLENT;
	g_HardcodedBotInfo[BOT_MANEK].Anticipation = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_MANEK].ReactionTime = BOTSKILL_HIGH;
	g_HardcodedBotInfo[BOT_MANEK].Perception = BOTSKILL_HIGH;
	sprintf( g_HardcodedBotInfo[BOT_MANEK].szFavoriteWeapon, "railgun" );
	sprintf( g_HardcodedBotInfo[BOT_MANEK].szColor, "5f 00 00" );
	sprintf( g_HardcodedBotInfo[BOT_MANEK].szGender, "cyborg" );
	sprintf( g_HardcodedBotInfo[BOT_MANEK].szSkinName, "" );
	sprintf( g_HardcodedBotInfo[BOT_MANEK].szClassName, "mage" );
	g_HardcodedBotInfo[BOT_MANEK].ulRailgunColor = RAILCOLOR_GREEN;
	g_HardcodedBotInfo[BOT_MANEK].ulChatFrequency = 50;
	g_HardcodedBotInfo[BOT_MANEK].bRevealed = true;
	g_HardcodedBotInfo[BOT_MANEK].bRevealedByDefault = true;
//	sprintf( g_HardcodedBotInfo[BOT_MANEK].szScriptName, "INSCTBOT" );
	sprintf( g_HardcodedBotInfo[BOT_MANEK].szScriptName, "DFULTBOT" );
	g_HardcodedBotInfo[BOT_MANEK].szChatFile[0] = 0;
	sprintf( g_HardcodedBotInfo[BOT_MANEK].szChatLump, "bots/chatfiles/insect.txt" );

	/**************** DEMONS *****************/

	// BOT_LINGUICA		(19/25)
	sprintf( g_HardcodedBotInfo[BOT_LINGUICA].szName, "Linguica" );
	g_HardcodedBotInfo[BOT_LINGUICA].Accuracy = BOTSKILL_EXCELLENT;
	g_HardcodedBotInfo[BOT_LINGUICA].Intellect = BOTSKILL_SUPREME;
	g_HardcodedBotInfo[BOT_LINGUICA].Evade = BOTSKILL_EXCELLENT;
	g_HardcodedBotInfo[BOT_LINGUICA].Anticipation = BOTSKILL_HIGH;
	g_HardcodedBotInfo[BOT_LINGUICA].ReactionTime = BOTSKILL_HIGH;
	g_HardcodedBotInfo[BOT_LINGUICA].Perception = BOTSKILL_HIGH;
	sprintf( g_HardcodedBotInfo[BOT_LINGUICA].szFavoriteWeapon, "plasmarifle" );
	sprintf( g_HardcodedBotInfo[BOT_LINGUICA].szColor, "ff 00 00" );
	sprintf( g_HardcodedBotInfo[BOT_LINGUICA].szGender, "male" );
	sprintf( g_HardcodedBotInfo[BOT_LINGUICA].szSkinName, "hissy hogger" );
	sprintf( g_HardcodedBotInfo[BOT_LINGUICA].szClassName, "random" );
	g_HardcodedBotInfo[BOT_LINGUICA].ulRailgunColor = RAILCOLOR_GREEN;
	g_HardcodedBotInfo[BOT_LINGUICA].ulChatFrequency = 75;
	g_HardcodedBotInfo[BOT_LINGUICA].bRevealed = true;
	g_HardcodedBotInfo[BOT_LINGUICA].bRevealedByDefault = true;
	sprintf( g_HardcodedBotInfo[BOT_LINGUICA].szScriptName, "SAUSGBOT" );
	g_HardcodedBotInfo[BOT_LINGUICA].szChatFile[0] = 0;
	sprintf( g_HardcodedBotInfo[BOT_LINGUICA].szChatLump, "bots/chatfiles/linguica.txt" );

	// BOT_TORRENT
	sprintf( g_HardcodedBotInfo[BOT_TORRENT].szName, "Torrent" );
	g_HardcodedBotInfo[BOT_TORRENT].Accuracy = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_TORRENT].Intellect = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_TORRENT].Evade = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_TORRENT].Anticipation = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_TORRENT].ReactionTime = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_TORRENT].Perception = BOTSKILL_MEDIUM;
	sprintf( g_HardcodedBotInfo[BOT_TORRENT].szFavoriteWeapon, "railgun" );
	sprintf( g_HardcodedBotInfo[BOT_TORRENT].szColor, "00 00 00" );
	sprintf( g_HardcodedBotInfo[BOT_TORRENT].szGender, "cyborg" );
	sprintf( g_HardcodedBotInfo[BOT_TORRENT].szSkinName, "" );
	sprintf( g_HardcodedBotInfo[BOT_TORRENT].szClassName, "fighter" );
	g_HardcodedBotInfo[BOT_TORRENT].ulRailgunColor = RAILCOLOR_RED;
	g_HardcodedBotInfo[BOT_TORRENT].ulChatFrequency = 10;
	g_HardcodedBotInfo[BOT_TORRENT].bRevealed = true;
	g_HardcodedBotInfo[BOT_TORRENT].bRevealedByDefault = true;
//	sprintf( g_HardcodedBotInfo[BOT_TORRENT].szScriptName, "DEMONBOT" );
	sprintf( g_HardcodedBotInfo[BOT_TORRENT].szScriptName, "DFULTBOT" );
	g_HardcodedBotInfo[BOT_TORRENT].szChatFile[0] = 0;
	sprintf( g_HardcodedBotInfo[BOT_TORRENT].szChatLump, "bots/chatfiles/demon.txt" );

	// BOT_CATACLYSM
	sprintf( g_HardcodedBotInfo[BOT_CATACLYSM].szName, "Cataclysm" );
	g_HardcodedBotInfo[BOT_CATACLYSM].Accuracy = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_CATACLYSM].Intellect = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_CATACLYSM].Evade = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_CATACLYSM].Anticipation = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_CATACLYSM].ReactionTime = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_CATACLYSM].Perception = BOTSKILL_MEDIUM;
	sprintf( g_HardcodedBotInfo[BOT_CATACLYSM].szFavoriteWeapon, "railgun" );
	sprintf( g_HardcodedBotInfo[BOT_CATACLYSM].szColor, "00 00 00" );
	sprintf( g_HardcodedBotInfo[BOT_CATACLYSM].szGender, "cyborg" );
	sprintf( g_HardcodedBotInfo[BOT_CATACLYSM].szSkinName, "" );
	sprintf( g_HardcodedBotInfo[BOT_CATACLYSM].szClassName, "fighter" );
	g_HardcodedBotInfo[BOT_CATACLYSM].ulRailgunColor = RAILCOLOR_RED;
	g_HardcodedBotInfo[BOT_CATACLYSM].ulChatFrequency = 10;
	g_HardcodedBotInfo[BOT_CATACLYSM].bRevealed = true;
	g_HardcodedBotInfo[BOT_CATACLYSM].bRevealedByDefault = true;
//	sprintf( g_HardcodedBotInfo[BOT_CATACLYSM].szScriptName, "DEMONBOT" );
	sprintf( g_HardcodedBotInfo[BOT_CATACLYSM].szScriptName, "DFULTBOT" );
	g_HardcodedBotInfo[BOT_CATACLYSM].szChatFile[0] = 0;
	sprintf( g_HardcodedBotInfo[BOT_CATACLYSM].szChatLump, "bots/chatfiles/demon.txt" );

	// BOT_VEX
	sprintf( g_HardcodedBotInfo[BOT_VEX].szName, "Vex" );
	g_HardcodedBotInfo[BOT_VEX].Accuracy = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_VEX].Intellect = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_VEX].Evade = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_VEX].Anticipation = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_VEX].ReactionTime = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_VEX].Perception = BOTSKILL_MEDIUM;
	sprintf( g_HardcodedBotInfo[BOT_VEX].szFavoriteWeapon, "railgun" );
	sprintf( g_HardcodedBotInfo[BOT_VEX].szColor, "00 00 00" );
	sprintf( g_HardcodedBotInfo[BOT_VEX].szGender, "cyborg" );
	sprintf( g_HardcodedBotInfo[BOT_VEX].szSkinName, "" );
	sprintf( g_HardcodedBotInfo[BOT_VEX].szClassName, "fighter" );
	g_HardcodedBotInfo[BOT_VEX].ulRailgunColor = RAILCOLOR_RED;
	g_HardcodedBotInfo[BOT_VEX].ulChatFrequency = 10;
	g_HardcodedBotInfo[BOT_VEX].bRevealed = true;
	g_HardcodedBotInfo[BOT_VEX].bRevealedByDefault = true;
//	sprintf( g_HardcodedBotInfo[BOT_VEX].szScriptName, "DEMONBOT" );
	sprintf( g_HardcodedBotInfo[BOT_VEX].szScriptName, "DFULTBOT" );
	g_HardcodedBotInfo[BOT_VEX].szChatFile[0] = 0;
	sprintf( g_HardcodedBotInfo[BOT_VEX].szChatLump, "bots/chatfiles/demon.txt" );

	// BOT_OBELISK
	sprintf( g_HardcodedBotInfo[BOT_OBELISK].szName, "Obelisk" );
	g_HardcodedBotInfo[BOT_OBELISK].Accuracy = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_OBELISK].Intellect = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_OBELISK].Evade = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_OBELISK].Anticipation = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_OBELISK].ReactionTime = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_OBELISK].Perception = BOTSKILL_MEDIUM;
	sprintf( g_HardcodedBotInfo[BOT_OBELISK].szFavoriteWeapon, "railgun" );
	sprintf( g_HardcodedBotInfo[BOT_OBELISK].szColor, "00 00 00" );
	sprintf( g_HardcodedBotInfo[BOT_OBELISK].szGender, "cyborg" );
	sprintf( g_HardcodedBotInfo[BOT_OBELISK].szSkinName, "" );
	sprintf( g_HardcodedBotInfo[BOT_OBELISK].szClassName, "fighter" );
	g_HardcodedBotInfo[BOT_OBELISK].ulRailgunColor = RAILCOLOR_RED;
	g_HardcodedBotInfo[BOT_OBELISK].ulChatFrequency = 10;
	g_HardcodedBotInfo[BOT_OBELISK].bRevealed = true;
	g_HardcodedBotInfo[BOT_OBELISK].bRevealedByDefault = true;
//	sprintf( g_HardcodedBotInfo[BOT_OBELISK].szScriptName, "DEMONBOT" );
	sprintf( g_HardcodedBotInfo[BOT_OBELISK].szScriptName, "DFULTBOT" );
	g_HardcodedBotInfo[BOT_OBELISK].szChatFile[0] = 0;
	sprintf( g_HardcodedBotInfo[BOT_OBELISK].szChatLump, "bots/chatfiles/demon.txt" );

	// BOT_DAEMOS
	sprintf( g_HardcodedBotInfo[BOT_DAEMOS].szName, "Daemos" );
	g_HardcodedBotInfo[BOT_DAEMOS].Accuracy = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_DAEMOS].Intellect = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_DAEMOS].Evade = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_DAEMOS].Anticipation = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_DAEMOS].ReactionTime = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_DAEMOS].Perception = BOTSKILL_MEDIUM;
	sprintf( g_HardcodedBotInfo[BOT_DAEMOS].szFavoriteWeapon, "supershotgun" );
	sprintf( g_HardcodedBotInfo[BOT_DAEMOS].szColor, "5f bf c0" );
	sprintf( g_HardcodedBotInfo[BOT_DAEMOS].szGender, "male" );
	sprintf( g_HardcodedBotInfo[BOT_DAEMOS].szSkinName, "" );
	sprintf( g_HardcodedBotInfo[BOT_DAEMOS].szClassName, "cleric" );
	g_HardcodedBotInfo[BOT_DAEMOS].ulRailgunColor = RAILCOLOR_BLACK;
	g_HardcodedBotInfo[BOT_DAEMOS].ulChatFrequency = 10;
	g_HardcodedBotInfo[BOT_DAEMOS].bRevealed = true;
	g_HardcodedBotInfo[BOT_DAEMOS].bRevealedByDefault = true;
//	sprintf( g_HardcodedBotInfo[BOT_DAEMOS].szScriptName, "DEMONBOT" );
	sprintf( g_HardcodedBotInfo[BOT_DAEMOS].szScriptName, "DFULTBOT" );
	g_HardcodedBotInfo[BOT_DAEMOS].szChatFile[0] = 0;
	sprintf( g_HardcodedBotInfo[BOT_DAEMOS].szChatLump, "bots/chatfiles/demon.txt" );

	// BOT_MAABUS	(23/25)
	sprintf( g_HardcodedBotInfo[BOT_MAABUS].szName, "Maabus" );
	g_HardcodedBotInfo[BOT_MAABUS].Accuracy = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_MAABUS].Intellect = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_MAABUS].Evade = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_MAABUS].Anticipation = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_MAABUS].ReactionTime = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_MAABUS].Perception = BOTSKILL_MEDIUM;
	sprintf( g_HardcodedBotInfo[BOT_MAABUS].szFavoriteWeapon, "railgun" );
	sprintf( g_HardcodedBotInfo[BOT_MAABUS].szColor, "bf bf 00" );
	sprintf( g_HardcodedBotInfo[BOT_MAABUS].szGender, "male" );
	sprintf( g_HardcodedBotInfo[BOT_MAABUS].szSkinName, "" );
	sprintf( g_HardcodedBotInfo[BOT_MAABUS].szClassName, "random" );
	g_HardcodedBotInfo[BOT_MAABUS].ulRailgunColor = RAILCOLOR_GOLD;
	g_HardcodedBotInfo[BOT_MAABUS].ulChatFrequency = 10;
	g_HardcodedBotInfo[BOT_MAABUS].bRevealed = true;
	g_HardcodedBotInfo[BOT_MAABUS].bRevealedByDefault = true;
//	sprintf( g_HardcodedBotInfo[BOT_MAABUS].szScriptName, "DEMONBOT" );
	sprintf( g_HardcodedBotInfo[BOT_MAABUS].szScriptName, "DFULTBOT" );
	g_HardcodedBotInfo[BOT_MAABUS].szChatFile[0] = 0;
	sprintf( g_HardcodedBotInfo[BOT_MAABUS].szChatLump, "bots/chatfiles/demon.txt" );

	// BOT_SLYOR	(23/25)
	sprintf( g_HardcodedBotInfo[BOT_SLYOR].szName, "Slyor" );
	g_HardcodedBotInfo[BOT_SLYOR].Accuracy = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_SLYOR].Intellect = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_SLYOR].Evade = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_SLYOR].Anticipation = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_SLYOR].ReactionTime = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_SLYOR].Perception = BOTSKILL_MEDIUM;
	sprintf( g_HardcodedBotInfo[BOT_SLYOR].szFavoriteWeapon, "railgun" );
	sprintf( g_HardcodedBotInfo[BOT_SLYOR].szColor, "8f 8f 00" );
	sprintf( g_HardcodedBotInfo[BOT_SLYOR].szGender, "male" );
	sprintf( g_HardcodedBotInfo[BOT_SLYOR].szSkinName, "slyor" );
	sprintf( g_HardcodedBotInfo[BOT_SLYOR].szClassName, "random" );
	g_HardcodedBotInfo[BOT_SLYOR].ulRailgunColor = RAILCOLOR_GOLD;
	g_HardcodedBotInfo[BOT_SLYOR].ulChatFrequency = 10;
	g_HardcodedBotInfo[BOT_SLYOR].bRevealed = true;
	g_HardcodedBotInfo[BOT_SLYOR].bRevealedByDefault = true;
//	sprintf( g_HardcodedBotInfo[BOT_SLYOR].szScriptName, "DEMONBOT" );
	sprintf( g_HardcodedBotInfo[BOT_SLYOR].szScriptName, "DFULTBOT" );
	g_HardcodedBotInfo[BOT_SLYOR].szChatFile[0] = 0;
	sprintf( g_HardcodedBotInfo[BOT_SLYOR].szChatLump, "bots/chatfiles/demon.txt" );

	/**************** FINAL BOSSES *****************/

	// BOT_ORION	(25/30)
	sprintf( g_HardcodedBotInfo[BOT_ORION].szName, "Orion" );
	g_HardcodedBotInfo[BOT_ORION].Accuracy = BOTSKILL_SUPREME;
	g_HardcodedBotInfo[BOT_ORION].Intellect = BOTSKILL_SUPREME;
	g_HardcodedBotInfo[BOT_ORION].Evade = BOTSKILL_EXCELLENT;
	g_HardcodedBotInfo[BOT_ORION].Anticipation = BOTSKILL_SUPREME;
	g_HardcodedBotInfo[BOT_ORION].ReactionTime = BOTSKILL_HIGH;
	g_HardcodedBotInfo[BOT_ORION].Perception = BOTSKILL_HIGH;
	sprintf( g_HardcodedBotInfo[BOT_ORION].szFavoriteWeapon, "railgun" );
	sprintf( g_HardcodedBotInfo[BOT_ORION].szColor, "bf bf 00" );
	sprintf( g_HardcodedBotInfo[BOT_ORION].szGender, "male" );
	sprintf( g_HardcodedBotInfo[BOT_ORION].szSkinName, "orion" );
	sprintf( g_HardcodedBotInfo[BOT_ORION].szClassName, "fighter" );
	g_HardcodedBotInfo[BOT_ORION].ulRailgunColor = RAILCOLOR_GOLD;
	g_HardcodedBotInfo[BOT_ORION].ulChatFrequency = 33;
	g_HardcodedBotInfo[BOT_ORION].bRevealed = true;
	g_HardcodedBotInfo[BOT_ORION].bRevealedByDefault = true;
//	sprintf( g_HardcodedBotInfo[BOT_ORION].szScriptName, "OREOBOT" );
	sprintf( g_HardcodedBotInfo[BOT_ORION].szScriptName, "HUMANBOT" );
	g_HardcodedBotInfo[BOT_ORION].szChatFile[0] = 0;
	sprintf( g_HardcodedBotInfo[BOT_ORION].szChatLump, "bots/chatfiles/orion.txt" );

	// BOT_ULTIMUS	(30/30)
	sprintf( g_HardcodedBotInfo[BOT_ULTIMUS].szName, "Ultimus" );
	g_HardcodedBotInfo[BOT_ULTIMUS].Accuracy = BOTSKILL_PERFECT;
	g_HardcodedBotInfo[BOT_ULTIMUS].Intellect = BOTSKILL_PERFECT;
	g_HardcodedBotInfo[BOT_ULTIMUS].Evade = BOTSKILL_PERFECT;
	g_HardcodedBotInfo[BOT_ULTIMUS].Anticipation = BOTSKILL_PERFECT;
	g_HardcodedBotInfo[BOT_ULTIMUS].ReactionTime = BOTSKILL_PERFECT;
	g_HardcodedBotInfo[BOT_ULTIMUS].Perception = BOTSKILL_PERFECT;
	sprintf( g_HardcodedBotInfo[BOT_ULTIMUS].szFavoriteWeapon, "railgun" );
	sprintf( g_HardcodedBotInfo[BOT_ULTIMUS].szColor, "c0 8f 60" );
	sprintf( g_HardcodedBotInfo[BOT_ULTIMUS].szGender, "cyborg" );
	sprintf( g_HardcodedBotInfo[BOT_ULTIMUS].szSkinName, "ultimus" );
	sprintf( g_HardcodedBotInfo[BOT_ULTIMUS].szClassName, "fighter" );
	g_HardcodedBotInfo[BOT_ULTIMUS].ulRailgunColor = RAILCOLOR_BLACK;
	g_HardcodedBotInfo[BOT_ULTIMUS].ulChatFrequency = 25;
	g_HardcodedBotInfo[BOT_ULTIMUS].bRevealed = true;
	g_HardcodedBotInfo[BOT_ULTIMUS].bRevealedByDefault = true;
//	sprintf( g_HardcodedBotInfo[BOT_ULTIMUS].szScriptName, "ULTIBOT" );
	sprintf( g_HardcodedBotInfo[BOT_ULTIMUS].szScriptName, "DFULTBOT" );
	g_HardcodedBotInfo[BOT_ULTIMUS].szChatFile[0] = 0;
	sprintf( g_HardcodedBotInfo[BOT_ULTIMUS].szChatLump, "bots/chatfiles/ultimus.txt" );

	/**************** EXTRA BOTS *****************/

	// BOT_ROMERO
	sprintf( g_HardcodedBotInfo[BOT_ROMERO].szName, "Romero" );
	g_HardcodedBotInfo[BOT_ROMERO].Accuracy = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_ROMERO].Intellect = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_ROMERO].Evade = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_ROMERO].Anticipation = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_ROMERO].ReactionTime = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_ROMERO].Perception = BOTSKILL_MEDIUM;
	sprintf( g_HardcodedBotInfo[BOT_ROMERO].szFavoriteWeapon, "chaingun" );
	sprintf( g_HardcodedBotInfo[BOT_ROMERO].szColor, "7f 80 3f" );
	sprintf( g_HardcodedBotInfo[BOT_ROMERO].szGender, "male" );
	sprintf( g_HardcodedBotInfo[BOT_ROMERO].szSkinName, "john romero" );
	sprintf( g_HardcodedBotInfo[BOT_ROMERO].szClassName, "mage" );
	g_HardcodedBotInfo[BOT_ROMERO].ulRailgunColor = RAILCOLOR_RAINBOW;
	g_HardcodedBotInfo[BOT_ROMERO].ulChatFrequency = 50;
	g_HardcodedBotInfo[BOT_ROMERO].bRevealed = false;
	g_HardcodedBotInfo[BOT_ROMERO].bRevealedByDefault = false;
//	sprintf( g_HardcodedBotInfo[BOT_ROMERO].szScriptName, "POGOBOT" );
	sprintf( g_HardcodedBotInfo[BOT_ROMERO].szScriptName, "HUMANBOT" );
	g_HardcodedBotInfo[BOT_ROMERO].szChatFile[0] = 0;
	sprintf( g_HardcodedBotInfo[BOT_ROMERO].szChatLump, "bots/chatfiles/romero.txt" );

	// BOT_H4X0R
	sprintf( g_HardcodedBotInfo[BOT_H4X0R].szName, "h4x0r" );
	g_HardcodedBotInfo[BOT_H4X0R].Accuracy = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_H4X0R].Intellect = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_H4X0R].Evade = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_H4X0R].Anticipation = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_H4X0R].ReactionTime = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_H4X0R].Perception = BOTSKILL_MEDIUM;
	sprintf( g_HardcodedBotInfo[BOT_H4X0R].szFavoriteWeapon, "railgun" );
	sprintf( g_HardcodedBotInfo[BOT_H4X0R].szColor, "bf bf 60" );
	sprintf( g_HardcodedBotInfo[BOT_H4X0R].szGender, "male" );
	sprintf( g_HardcodedBotInfo[BOT_H4X0R].szSkinName, "" );
	sprintf( g_HardcodedBotInfo[BOT_H4X0R].szClassName, "random" );
	g_HardcodedBotInfo[BOT_H4X0R].ulRailgunColor = RAILCOLOR_PURPLE;
	g_HardcodedBotInfo[BOT_H4X0R].ulChatFrequency = 75;
	g_HardcodedBotInfo[BOT_H4X0R].bRevealed = true;
	g_HardcodedBotInfo[BOT_H4X0R].bRevealedByDefault = true;
//	sprintf( g_HardcodedBotInfo[BOT_H4X0R].szScriptName, "HAXORBOT" );
	sprintf( g_HardcodedBotInfo[BOT_H4X0R].szScriptName, "HUMANBOT" );
	g_HardcodedBotInfo[BOT_H4X0R].szChatFile[0] = 0;
	sprintf( g_HardcodedBotInfo[BOT_H4X0R].szChatLump, "bots/chatfiles/1337.txt" );

	// BOT_FRAD
	sprintf( g_HardcodedBotInfo[BOT_FRAD].szName, "frad" );
	g_HardcodedBotInfo[BOT_FRAD].Accuracy = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_FRAD].Intellect = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_FRAD].Evade = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_FRAD].Anticipation = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_FRAD].ReactionTime = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_FRAD].Perception = BOTSKILL_MEDIUM;
	sprintf( g_HardcodedBotInfo[BOT_FRAD].szFavoriteWeapon, "railgun" );
	sprintf( g_HardcodedBotInfo[BOT_FRAD].szColor, "00 00 00" );
	sprintf( g_HardcodedBotInfo[BOT_FRAD].szGender, "male" );
	sprintf( g_HardcodedBotInfo[BOT_FRAD].szSkinName, "" );
	sprintf( g_HardcodedBotInfo[BOT_FRAD].szClassName, "random" );
	g_HardcodedBotInfo[BOT_FRAD].ulRailgunColor = RAILCOLOR_RED;
	g_HardcodedBotInfo[BOT_FRAD].ulChatFrequency = 70;
	g_HardcodedBotInfo[BOT_FRAD].bRevealed = false;
	g_HardcodedBotInfo[BOT_FRAD].bRevealedByDefault = false;
//	sprintf( g_HardcodedBotInfo[BOT_FRAD].szScriptName, "FRADBOT" );
	sprintf( g_HardcodedBotInfo[BOT_FRAD].szScriptName, "HUMANBOT" );
	g_HardcodedBotInfo[BOT_FRAD].szChatFile[0] = 0;
	sprintf( g_HardcodedBotInfo[BOT_FRAD].szChatLump, "bots/chatfiles/frad.txt" );

	// BOT_MEWSE
	sprintf( g_HardcodedBotInfo[BOT_MEWSE].szName, "mewse" );
	g_HardcodedBotInfo[BOT_MEWSE].Accuracy = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_MEWSE].Intellect = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_MEWSE].Evade = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_MEWSE].Anticipation = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_MEWSE].ReactionTime = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_MEWSE].Perception = BOTSKILL_MEDIUM;
	sprintf( g_HardcodedBotInfo[BOT_MEWSE].szFavoriteWeapon, "railgun" );
	sprintf( g_HardcodedBotInfo[BOT_MEWSE].szColor, "00 00 00" );
	sprintf( g_HardcodedBotInfo[BOT_MEWSE].szGender, "male" );
	sprintf( g_HardcodedBotInfo[BOT_MEWSE].szSkinName, "" );
	sprintf( g_HardcodedBotInfo[BOT_MEWSE].szClassName, "random" );
	g_HardcodedBotInfo[BOT_MEWSE].ulRailgunColor = RAILCOLOR_RAINBOW;
	g_HardcodedBotInfo[BOT_MEWSE].ulChatFrequency = 75;
	g_HardcodedBotInfo[BOT_MEWSE].bRevealed = false;
	g_HardcodedBotInfo[BOT_MEWSE].bRevealedByDefault = false;
//	sprintf( g_HardcodedBotInfo[BOT_MEWSE].szScriptName, "MEWSEBOT" );
	sprintf( g_HardcodedBotInfo[BOT_MEWSE].szScriptName, "HUMANBOT" );
	g_HardcodedBotInfo[BOT_MEWSE].szChatFile[0] = 0;
	sprintf( g_HardcodedBotInfo[BOT_MEWSE].szChatLump, "bots/chatfiles/mewse.txt" );

	// BOT_HISSY
	sprintf( g_HardcodedBotInfo[BOT_HISSY].szName, "Hissy" );
	g_HardcodedBotInfo[BOT_HISSY].Accuracy = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_HISSY].Intellect = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_HISSY].Evade = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_HISSY].Anticipation = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_HISSY].ReactionTime = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_HISSY].Perception = BOTSKILL_MEDIUM;
	sprintf( g_HardcodedBotInfo[BOT_HISSY].szFavoriteWeapon, "railgun" );
	sprintf( g_HardcodedBotInfo[BOT_HISSY].szColor, "00 ff 00" );
	sprintf( g_HardcodedBotInfo[BOT_HISSY].szGender, "male" );
	sprintf( g_HardcodedBotInfo[BOT_HISSY].szSkinName, "hissy" );
	sprintf( g_HardcodedBotInfo[BOT_HISSY].szClassName, "mage" );
	g_HardcodedBotInfo[BOT_HISSY].ulRailgunColor = RAILCOLOR_RAINBOW;
	g_HardcodedBotInfo[BOT_HISSY].ulChatFrequency = 65;
	g_HardcodedBotInfo[BOT_HISSY].bRevealed = true;
	g_HardcodedBotInfo[BOT_HISSY].bRevealedByDefault = true;
//	sprintf( g_HardcodedBotInfo[BOT_HISSY].szScriptName, "HISSYBOT" );
	sprintf( g_HardcodedBotInfo[BOT_HISSY].szScriptName, "HUMANBOT" );
	g_HardcodedBotInfo[BOT_HISSY].szChatFile[0] = 0;
	sprintf( g_HardcodedBotInfo[BOT_HISSY].szChatLump, "bots/chatfiles/hissy.txt" );

	// BOT_MASSMOUTH
	sprintf( g_HardcodedBotInfo[BOT_MASSMOUTH].szName, "Massmouth" );
	g_HardcodedBotInfo[BOT_MASSMOUTH].Accuracy = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_MASSMOUTH].Intellect = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_MASSMOUTH].Evade = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_MASSMOUTH].Anticipation = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_MASSMOUTH].ReactionTime = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_MASSMOUTH].Perception = BOTSKILL_MEDIUM;
	sprintf( g_HardcodedBotInfo[BOT_MASSMOUTH].szFavoriteWeapon, "chaingun" );
	sprintf( g_HardcodedBotInfo[BOT_MASSMOUTH].szColor, "00 b0 00" );
	sprintf( g_HardcodedBotInfo[BOT_MASSMOUTH].szGender, "male" );
	sprintf( g_HardcodedBotInfo[BOT_MASSMOUTH].szSkinName, "Massmouth 2" );
	sprintf( g_HardcodedBotInfo[BOT_MASSMOUTH].szClassName, "random" );
	g_HardcodedBotInfo[BOT_MASSMOUTH].ulRailgunColor = RAILCOLOR_WHITE;
	g_HardcodedBotInfo[BOT_MASSMOUTH].ulChatFrequency = 65;
	g_HardcodedBotInfo[BOT_MASSMOUTH].bRevealed = true;
	g_HardcodedBotInfo[BOT_MASSMOUTH].bRevealedByDefault = true;
//	sprintf( g_HardcodedBotInfo[BOT_MASSMOUTH].szScriptName, "MOUTHBOT" );
	sprintf( g_HardcodedBotInfo[BOT_MASSMOUTH].szScriptName, "HUMANBOT" );
	g_HardcodedBotInfo[BOT_MASSMOUTH].szChatFile[0] = 0;
	sprintf( g_HardcodedBotInfo[BOT_MASSMOUTH].szChatLump, "bots/chatfiles/massmouth.txt" );

	// BOT_DOOMCRATE
	sprintf( g_HardcodedBotInfo[BOT_DOOMCRATE].szName, "Doomcrate" );
	g_HardcodedBotInfo[BOT_DOOMCRATE].Accuracy = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_DOOMCRATE].Intellect = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_DOOMCRATE].Evade = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_DOOMCRATE].Anticipation = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_DOOMCRATE].ReactionTime = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_DOOMCRATE].Perception = BOTSKILL_MEDIUM;
	sprintf( g_HardcodedBotInfo[BOT_DOOMCRATE].szFavoriteWeapon, "chaingun" );
	sprintf( g_HardcodedBotInfo[BOT_DOOMCRATE].szColor, "ef c0 80" );
	sprintf( g_HardcodedBotInfo[BOT_DOOMCRATE].szGender, "male" );
	sprintf( g_HardcodedBotInfo[BOT_DOOMCRATE].szSkinName, "doomcrate" );
	sprintf( g_HardcodedBotInfo[BOT_DOOMCRATE].szClassName, "random" );
	g_HardcodedBotInfo[BOT_DOOMCRATE].ulRailgunColor = RAILCOLOR_SILVER;
	g_HardcodedBotInfo[BOT_DOOMCRATE].ulChatFrequency = 10;
	g_HardcodedBotInfo[BOT_DOOMCRATE].bRevealed = true;
	g_HardcodedBotInfo[BOT_DOOMCRATE].bRevealedByDefault = true;
//	sprintf( g_HardcodedBotInfo[BOT_DOOMCRATE].szScriptName, "BOXBOT" );
	sprintf( g_HardcodedBotInfo[BOT_DOOMCRATE].szScriptName, "DFULTBOT" );
	g_HardcodedBotInfo[BOT_DOOMCRATE].szChatFile[0] = 0;
	sprintf( g_HardcodedBotInfo[BOT_DOOMCRATE].szChatLump, "bots/chatfiles/doomcrate.txt" );

	// BOT_ZOMBIEMAN
	sprintf( g_HardcodedBotInfo[BOT_ZOMBIEMAN].szName, "Zombieman" );
	g_HardcodedBotInfo[BOT_ZOMBIEMAN].Accuracy = BOTSKILL_LOW;
	g_HardcodedBotInfo[BOT_ZOMBIEMAN].Intellect = BOTSKILL_LOW;
	g_HardcodedBotInfo[BOT_ZOMBIEMAN].Evade = BOTSKILL_LOW;
	g_HardcodedBotInfo[BOT_ZOMBIEMAN].Anticipation = BOTSKILL_LOW;
	g_HardcodedBotInfo[BOT_ZOMBIEMAN].ReactionTime = BOTSKILL_LOW;
	g_HardcodedBotInfo[BOT_ZOMBIEMAN].Perception = BOTSKILL_LOW;
	sprintf( g_HardcodedBotInfo[BOT_ZOMBIEMAN].szFavoriteWeapon, "chaingun" );
	sprintf( g_HardcodedBotInfo[BOT_ZOMBIEMAN].szColor, "bf 8f 5f" );
	sprintf( g_HardcodedBotInfo[BOT_ZOMBIEMAN].szGender, "male" );
	sprintf( g_HardcodedBotInfo[BOT_ZOMBIEMAN].szSkinName, "zombieman" );
	sprintf( g_HardcodedBotInfo[BOT_ZOMBIEMAN].szClassName, "random" );
	g_HardcodedBotInfo[BOT_ZOMBIEMAN].ulRailgunColor = RAILCOLOR_BLUE;
	g_HardcodedBotInfo[BOT_ZOMBIEMAN].ulChatFrequency = 0;
	g_HardcodedBotInfo[BOT_ZOMBIEMAN].bRevealed = true;
	g_HardcodedBotInfo[BOT_ZOMBIEMAN].bRevealedByDefault = true;
//	sprintf( g_HardcodedBotInfo[BOT_ZOMBIEMAN].szScriptName, "ZMBIEBOT" );
	sprintf( g_HardcodedBotInfo[BOT_ZOMBIEMAN].szScriptName, "DFULTBOT" );
	g_HardcodedBotInfo[BOT_ZOMBIEMAN].szChatFile[0] = 0;
	g_HardcodedBotInfo[BOT_ZOMBIEMAN].szChatLump[0] = 0;

	// BOT_SHOTGUNGUY
	sprintf( g_HardcodedBotInfo[BOT_SHOTGUNGUY].szName, "Shotgunner" );
	g_HardcodedBotInfo[BOT_SHOTGUNGUY].Accuracy = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_SHOTGUNGUY].Intellect = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_SHOTGUNGUY].Evade = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_SHOTGUNGUY].Anticipation = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_SHOTGUNGUY].ReactionTime = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_SHOTGUNGUY].Perception = BOTSKILL_MEDIUM;
	sprintf( g_HardcodedBotInfo[BOT_SHOTGUNGUY].szFavoriteWeapon, "shotgun" );
	sprintf( g_HardcodedBotInfo[BOT_SHOTGUNGUY].szColor, "df 20 20" );
	sprintf( g_HardcodedBotInfo[BOT_SHOTGUNGUY].szGender, "male" );
	sprintf( g_HardcodedBotInfo[BOT_SHOTGUNGUY].szSkinName, "shotgun guy" );
	sprintf( g_HardcodedBotInfo[BOT_SHOTGUNGUY].szClassName, "fighter" );
	g_HardcodedBotInfo[BOT_SHOTGUNGUY].ulRailgunColor = RAILCOLOR_BLUE;
	g_HardcodedBotInfo[BOT_SHOTGUNGUY].ulChatFrequency = 0;
	g_HardcodedBotInfo[BOT_SHOTGUNGUY].bRevealed = true;
	g_HardcodedBotInfo[BOT_SHOTGUNGUY].bRevealedByDefault = true;
//	sprintf( g_HardcodedBotInfo[BOT_SHOTGUNGUY].szScriptName, "ZMBIEBOT" );
	sprintf( g_HardcodedBotInfo[BOT_SHOTGUNGUY].szScriptName, "DFULTBOT" );
	g_HardcodedBotInfo[BOT_SHOTGUNGUY].szChatFile[0] = 0;
	g_HardcodedBotInfo[BOT_SHOTGUNGUY].szChatLump[0] = 0;

	// BOT_BIGCHAINGUNGUY
	sprintf( g_HardcodedBotInfo[BOT_BIGCHAINGUNGUY].szName, "Chaingunner" );
	g_HardcodedBotInfo[BOT_BIGCHAINGUNGUY].Accuracy = BOTSKILL_HIGH;
	g_HardcodedBotInfo[BOT_BIGCHAINGUNGUY].Intellect = BOTSKILL_HIGH;
	g_HardcodedBotInfo[BOT_BIGCHAINGUNGUY].Evade = BOTSKILL_HIGH;
	g_HardcodedBotInfo[BOT_BIGCHAINGUNGUY].Anticipation = BOTSKILL_HIGH;
	g_HardcodedBotInfo[BOT_BIGCHAINGUNGUY].ReactionTime = BOTSKILL_HIGH;
	g_HardcodedBotInfo[BOT_BIGCHAINGUNGUY].Perception = BOTSKILL_HIGH;
	sprintf( g_HardcodedBotInfo[BOT_BIGCHAINGUNGUY].szFavoriteWeapon, "chaingun" );
	sprintf( g_HardcodedBotInfo[BOT_BIGCHAINGUNGUY].szColor, "ef 00 00" );
	sprintf( g_HardcodedBotInfo[BOT_BIGCHAINGUNGUY].szGender, "male" );
	sprintf( g_HardcodedBotInfo[BOT_BIGCHAINGUNGUY].szSkinName, "big chaingun guy" );
	sprintf( g_HardcodedBotInfo[BOT_BIGCHAINGUNGUY].szClassName, "fighter" );
	g_HardcodedBotInfo[BOT_BIGCHAINGUNGUY].ulRailgunColor = RAILCOLOR_BLUE;
	g_HardcodedBotInfo[BOT_BIGCHAINGUNGUY].ulChatFrequency = 0;
	g_HardcodedBotInfo[BOT_BIGCHAINGUNGUY].bRevealed = true;
	g_HardcodedBotInfo[BOT_BIGCHAINGUNGUY].bRevealedByDefault = true;
//	sprintf( g_HardcodedBotInfo[BOT_BIGCHAINGUNGUY].szScriptName, "ZMBIEBOT" );
	sprintf( g_HardcodedBotInfo[BOT_BIGCHAINGUNGUY].szScriptName, "DFULTBOT" );
	g_HardcodedBotInfo[BOT_BIGCHAINGUNGUY].szChatFile[0] = 0;
	g_HardcodedBotInfo[BOT_BIGCHAINGUNGUY].szChatLump[0] = 0;

	// BOT_SUPERSHOTGUNNER
	sprintf( g_HardcodedBotInfo[BOT_SUPERSHOTGUNNER].szName, "Super Shotgunner" );
	g_HardcodedBotInfo[BOT_SUPERSHOTGUNNER].Accuracy = BOTSKILL_EXCELLENT;
	g_HardcodedBotInfo[BOT_SUPERSHOTGUNNER].Intellect = BOTSKILL_EXCELLENT;
	g_HardcodedBotInfo[BOT_SUPERSHOTGUNNER].Evade = BOTSKILL_EXCELLENT;
	g_HardcodedBotInfo[BOT_SUPERSHOTGUNNER].Anticipation = BOTSKILL_EXCELLENT;
	g_HardcodedBotInfo[BOT_SUPERSHOTGUNNER].ReactionTime = BOTSKILL_EXCELLENT;
	g_HardcodedBotInfo[BOT_SUPERSHOTGUNNER].Perception = BOTSKILL_EXCELLENT;
	sprintf( g_HardcodedBotInfo[BOT_SUPERSHOTGUNNER].szFavoriteWeapon, "supershotgun" );
	sprintf( g_HardcodedBotInfo[BOT_SUPERSHOTGUNNER].szColor, "00 ff 00" );
	sprintf( g_HardcodedBotInfo[BOT_SUPERSHOTGUNNER].szGender, "male" );
	sprintf( g_HardcodedBotInfo[BOT_SUPERSHOTGUNNER].szSkinName, "super shotgun guy" );
	sprintf( g_HardcodedBotInfo[BOT_SUPERSHOTGUNNER].szClassName, "fighter" );
	g_HardcodedBotInfo[BOT_SUPERSHOTGUNNER].ulRailgunColor = RAILCOLOR_GREEN;
	g_HardcodedBotInfo[BOT_SUPERSHOTGUNNER].ulChatFrequency = 0;
	g_HardcodedBotInfo[BOT_SUPERSHOTGUNNER].bRevealed = true;
	g_HardcodedBotInfo[BOT_SUPERSHOTGUNNER].bRevealedByDefault = true;
//	sprintf( g_HardcodedBotInfo[BOT_SUPERSHOTGUNNER].szScriptName, "ZMBIEBOT" );
	sprintf( g_HardcodedBotInfo[BOT_SUPERSHOTGUNNER].szScriptName, "DFULTBOT" );
	g_HardcodedBotInfo[BOT_SUPERSHOTGUNNER].szChatFile[0] = 0;
	g_HardcodedBotInfo[BOT_SUPERSHOTGUNNER].szChatLump[0] = 0;

	// BOT_SSNAZI	(12/30)
	sprintf( g_HardcodedBotInfo[BOT_SSNAZI].szName, "SS Nazi" );
	g_HardcodedBotInfo[BOT_SSNAZI].Accuracy = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_SSNAZI].Intellect = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_SSNAZI].Evade = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_SSNAZI].Anticipation = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_SSNAZI].ReactionTime = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_SSNAZI].Perception = BOTSKILL_MEDIUM;
	sprintf( g_HardcodedBotInfo[BOT_SSNAZI].szFavoriteWeapon, "shotgun" );
	sprintf( g_HardcodedBotInfo[BOT_SSNAZI].szColor, "00 00 ff" );
	sprintf( g_HardcodedBotInfo[BOT_SSNAZI].szGender, "male" );
	sprintf( g_HardcodedBotInfo[BOT_SSNAZI].szSkinName, "SS Nazi" );
	sprintf( g_HardcodedBotInfo[BOT_SSNAZI].szClassName, "fighter" );
	g_HardcodedBotInfo[BOT_SSNAZI].ulRailgunColor = RAILCOLOR_BLUE;
	g_HardcodedBotInfo[BOT_SSNAZI].ulChatFrequency = 0;
	g_HardcodedBotInfo[BOT_SSNAZI].bRevealed = true;
	g_HardcodedBotInfo[BOT_SSNAZI].bRevealedByDefault = true;
//	sprintf( g_HardcodedBotInfo[BOT_SSNAZI].szScriptName, "ENEMYBOT" );
	sprintf( g_HardcodedBotInfo[BOT_SSNAZI].szScriptName, "DFULTBOT" );
	g_HardcodedBotInfo[BOT_SSNAZI].szChatFile[0] = 0;
	g_HardcodedBotInfo[BOT_SSNAZI].szChatLump[0] = 0;

	// BOT_NAZIGUARD	(12/30)
	sprintf( g_HardcodedBotInfo[BOT_NAZIGUARD].szName, "Nazi Guard" );
	g_HardcodedBotInfo[BOT_NAZIGUARD].Accuracy = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_NAZIGUARD].Intellect = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_NAZIGUARD].Evade = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_NAZIGUARD].Anticipation = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_NAZIGUARD].ReactionTime = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_NAZIGUARD].Perception = BOTSKILL_MEDIUM;
	sprintf( g_HardcodedBotInfo[BOT_NAZIGUARD].szFavoriteWeapon, "shotgun" );
	sprintf( g_HardcodedBotInfo[BOT_NAZIGUARD].szColor, "b0 b0 b0" );
	sprintf( g_HardcodedBotInfo[BOT_NAZIGUARD].szGender, "male" );
	sprintf( g_HardcodedBotInfo[BOT_NAZIGUARD].szSkinName, "Nazi Guard" );
	sprintf( g_HardcodedBotInfo[BOT_NAZIGUARD].szClassName, "fighter" );
	g_HardcodedBotInfo[BOT_NAZIGUARD].ulRailgunColor = RAILCOLOR_BLUE;
	g_HardcodedBotInfo[BOT_NAZIGUARD].ulChatFrequency = 0;
	g_HardcodedBotInfo[BOT_NAZIGUARD].bRevealed = true;
	g_HardcodedBotInfo[BOT_NAZIGUARD].bRevealedByDefault = true;
//	sprintf( g_HardcodedBotInfo[BOT_NAZIGUARD].szScriptName, "ENEMYBOT" );
	sprintf( g_HardcodedBotInfo[BOT_NAZIGUARD].szScriptName, "DFULTBOT" );
	g_HardcodedBotInfo[BOT_NAZIGUARD].szChatFile[0] = 0;
	g_HardcodedBotInfo[BOT_NAZIGUARD].szChatLump[0] = 0;

	// BOT_NAZICHAINGUNNER	(18/30)
	sprintf( g_HardcodedBotInfo[BOT_NAZICHAINGUNNER].szName, "Nazi Chaingunner" );
	g_HardcodedBotInfo[BOT_NAZICHAINGUNNER].Accuracy = BOTSKILL_HIGH;
	g_HardcodedBotInfo[BOT_NAZICHAINGUNNER].Intellect = BOTSKILL_HIGH;
	g_HardcodedBotInfo[BOT_NAZICHAINGUNNER].Evade = BOTSKILL_HIGH;
	g_HardcodedBotInfo[BOT_NAZICHAINGUNNER].Anticipation = BOTSKILL_HIGH;
	g_HardcodedBotInfo[BOT_NAZICHAINGUNNER].ReactionTime = BOTSKILL_HIGH;
	g_HardcodedBotInfo[BOT_NAZICHAINGUNNER].Perception = BOTSKILL_HIGH;
	sprintf( g_HardcodedBotInfo[BOT_NAZICHAINGUNNER].szFavoriteWeapon, "shotgun" );
	sprintf( g_HardcodedBotInfo[BOT_NAZICHAINGUNNER].szColor, "00 00 ef" );
	sprintf( g_HardcodedBotInfo[BOT_NAZICHAINGUNNER].szGender, "male" );
	sprintf( g_HardcodedBotInfo[BOT_NAZICHAINGUNNER].szSkinName, "Nazi Chaingunner" );
	sprintf( g_HardcodedBotInfo[BOT_NAZICHAINGUNNER].szClassName, "fighter" );
	g_HardcodedBotInfo[BOT_NAZICHAINGUNNER].ulRailgunColor = RAILCOLOR_BLUE;
	g_HardcodedBotInfo[BOT_NAZICHAINGUNNER].ulChatFrequency = 0;
	g_HardcodedBotInfo[BOT_NAZICHAINGUNNER].bRevealed = true;
	g_HardcodedBotInfo[BOT_NAZICHAINGUNNER].bRevealedByDefault = true;
//	sprintf( g_HardcodedBotInfo[BOT_NAZICHAINGUNNER].szScriptName, "ENEMYBOT" );
	sprintf( g_HardcodedBotInfo[BOT_NAZICHAINGUNNER].szScriptName, "DFULTBOT" );
	g_HardcodedBotInfo[BOT_NAZICHAINGUNNER].szChatFile[0] = 0;
	g_HardcodedBotInfo[BOT_NAZICHAINGUNNER].szChatLump[0] = 0;

	// BOT_STRIFEGUY	(18/30)
	sprintf( g_HardcodedBotInfo[BOT_STRIFEGUY].szName, "Strife Guy" );
	g_HardcodedBotInfo[BOT_STRIFEGUY].Accuracy = BOTSKILL_HIGH;
	g_HardcodedBotInfo[BOT_STRIFEGUY].Intellect = BOTSKILL_HIGH;
	g_HardcodedBotInfo[BOT_STRIFEGUY].Evade = BOTSKILL_HIGH;
	g_HardcodedBotInfo[BOT_STRIFEGUY].Anticipation = BOTSKILL_HIGH;
	g_HardcodedBotInfo[BOT_STRIFEGUY].ReactionTime = BOTSKILL_HIGH;
	g_HardcodedBotInfo[BOT_STRIFEGUY].Perception = BOTSKILL_HIGH;
	sprintf( g_HardcodedBotInfo[BOT_STRIFEGUY].szFavoriteWeapon, "shotgun" );
	sprintf( g_HardcodedBotInfo[BOT_STRIFEGUY].szColor, "bf 8f 5f" );
	sprintf( g_HardcodedBotInfo[BOT_STRIFEGUY].szGender, "male" );
	sprintf( g_HardcodedBotInfo[BOT_STRIFEGUY].szSkinName, "Strife Guy" );
	sprintf( g_HardcodedBotInfo[BOT_STRIFEGUY].szClassName, "fighter" );
	g_HardcodedBotInfo[BOT_STRIFEGUY].ulRailgunColor = RAILCOLOR_BLUE;
	g_HardcodedBotInfo[BOT_STRIFEGUY].ulChatFrequency = 0;
	g_HardcodedBotInfo[BOT_STRIFEGUY].bRevealed = true;
	g_HardcodedBotInfo[BOT_STRIFEGUY].bRevealedByDefault = true;
//	sprintf( g_HardcodedBotInfo[BOT_STRIFEGUY].szScriptName, "ENEMYBOT" );
	sprintf( g_HardcodedBotInfo[BOT_STRIFEGUY].szScriptName, "DFULTBOT" );
	g_HardcodedBotInfo[BOT_STRIFEGUY].szChatFile[0] = 0;
	g_HardcodedBotInfo[BOT_STRIFEGUY].szChatLump[0] = 0;

	// BOT_XXENEMYXX
	sprintf( g_HardcodedBotInfo[BOT_XXENEMYXX].szName, "xxEnemyxx" );
	g_HardcodedBotInfo[BOT_XXENEMYXX].Accuracy = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_XXENEMYXX].Intellect = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_XXENEMYXX].Evade = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_XXENEMYXX].Anticipation = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_XXENEMYXX].ReactionTime = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_XXENEMYXX].Perception = BOTSKILL_MEDIUM;
	sprintf( g_HardcodedBotInfo[BOT_XXENEMYXX].szFavoriteWeapon, "chaingun" );
	sprintf( g_HardcodedBotInfo[BOT_XXENEMYXX].szColor, "7f 80 3f" );
	sprintf( g_HardcodedBotInfo[BOT_XXENEMYXX].szGender, "male" );
	sprintf( g_HardcodedBotInfo[BOT_XXENEMYXX].szSkinName, "" );
	sprintf( g_HardcodedBotInfo[BOT_XXENEMYXX].szClassName, "random" );
	g_HardcodedBotInfo[BOT_XXENEMYXX].ulRailgunColor = RAILCOLOR_RED;
	g_HardcodedBotInfo[BOT_XXENEMYXX].ulChatFrequency = 80;
	g_HardcodedBotInfo[BOT_XXENEMYXX].bRevealed = false;
	g_HardcodedBotInfo[BOT_XXENEMYXX].bRevealedByDefault = false;
//	sprintf( g_HardcodedBotInfo[BOT_XXENEMYXX].szScriptName, "ENEMYBOT" );
	sprintf( g_HardcodedBotInfo[BOT_XXENEMYXX].szScriptName, "HUMANBOT" );
	g_HardcodedBotInfo[BOT_XXENEMYXX].szChatFile[0] = 0;
	sprintf( g_HardcodedBotInfo[BOT_XXENEMYXX].szChatLump, "bots/chatfiles/xxenemyxx.txt" );

	// BOT_FIFFY
	sprintf( g_HardcodedBotInfo[BOT_FIFFY].szName, "King REoL" );
	g_HardcodedBotInfo[BOT_FIFFY].Accuracy = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_FIFFY].Intellect = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_FIFFY].Evade = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_FIFFY].Anticipation = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_FIFFY].ReactionTime = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_FIFFY].Perception = BOTSKILL_MEDIUM;
	sprintf( g_HardcodedBotInfo[BOT_FIFFY].szFavoriteWeapon, "chaingun" );
	sprintf( g_HardcodedBotInfo[BOT_FIFFY].szColor, "7f 80 3f" );
	sprintf( g_HardcodedBotInfo[BOT_FIFFY].szGender, "male" );
	sprintf( g_HardcodedBotInfo[BOT_FIFFY].szSkinName, "" );
	sprintf( g_HardcodedBotInfo[BOT_FIFFY].szClassName, "mage" );
	g_HardcodedBotInfo[BOT_FIFFY].ulRailgunColor = RAILCOLOR_PURPLE;
	g_HardcodedBotInfo[BOT_FIFFY].ulChatFrequency = 65;
	g_HardcodedBotInfo[BOT_FIFFY].bRevealed = false;
	g_HardcodedBotInfo[BOT_FIFFY].bRevealedByDefault = false;
//	sprintf( g_HardcodedBotInfo[BOT_FIFFY].szScriptName, "REOLBOT" );
	sprintf( g_HardcodedBotInfo[BOT_FIFFY].szScriptName, "HUMANBOT" );
	g_HardcodedBotInfo[BOT_FIFFY].szChatFile[0] = 0;
	sprintf( g_HardcodedBotInfo[BOT_FIFFY].szChatLump, "bots/chatfiles/reol.txt" );

	// BOT_MEEPY
	sprintf( g_HardcodedBotInfo[BOT_MEEPY].szName, "Meepy" );
	g_HardcodedBotInfo[BOT_MEEPY].Accuracy = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_MEEPY].Intellect = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_MEEPY].Evade = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_MEEPY].Anticipation = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_MEEPY].ReactionTime = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_MEEPY].Perception = BOTSKILL_MEDIUM;
	sprintf( g_HardcodedBotInfo[BOT_MEEPY].szFavoriteWeapon, "chaingun" );
	sprintf( g_HardcodedBotInfo[BOT_MEEPY].szColor, "7f 80 3f" );
	sprintf( g_HardcodedBotInfo[BOT_MEEPY].szGender, "male" );
	sprintf( g_HardcodedBotInfo[BOT_MEEPY].szSkinName, "meepy" );
	sprintf( g_HardcodedBotInfo[BOT_MEEPY].szClassName, "mage" );
	g_HardcodedBotInfo[BOT_MEEPY].ulRailgunColor = RAILCOLOR_BLUE;
	g_HardcodedBotInfo[BOT_MEEPY].ulChatFrequency = 33;
	g_HardcodedBotInfo[BOT_MEEPY].bRevealed = false;
	g_HardcodedBotInfo[BOT_MEEPY].bRevealedByDefault = false;
//	sprintf( g_HardcodedBotInfo[BOT_MEEPY].szScriptName, "MEEPYBOT" );
	sprintf( g_HardcodedBotInfo[BOT_MEEPY].szScriptName, "HUMANBOT" );
	g_HardcodedBotInfo[BOT_MEEPY].szChatFile[0] = 0;
	sprintf( g_HardcodedBotInfo[BOT_MEEPY].szChatLump, "bots/chatfiles/meepy.txt" );

	// BOT_CHEXMAN
	sprintf( g_HardcodedBotInfo[BOT_CHEXMAN].szName, "Chexman" );
	g_HardcodedBotInfo[BOT_CHEXMAN].Accuracy = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_CHEXMAN].Intellect = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_CHEXMAN].Evade = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_CHEXMAN].Anticipation = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_CHEXMAN].ReactionTime = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_CHEXMAN].Perception = BOTSKILL_MEDIUM;
	sprintf( g_HardcodedBotInfo[BOT_CHEXMAN].szFavoriteWeapon, "chaingun" );
	sprintf( g_HardcodedBotInfo[BOT_CHEXMAN].szColor, "7f 80 3f" );
	sprintf( g_HardcodedBotInfo[BOT_CHEXMAN].szGender, "male" );
	sprintf( g_HardcodedBotInfo[BOT_CHEXMAN].szSkinName, "chexman" );
	sprintf( g_HardcodedBotInfo[BOT_CHEXMAN].szClassName, "cleric" );
	g_HardcodedBotInfo[BOT_CHEXMAN].ulRailgunColor = RAILCOLOR_YELLOW;
	g_HardcodedBotInfo[BOT_CHEXMAN].ulChatFrequency = 33;
	g_HardcodedBotInfo[BOT_CHEXMAN].bRevealed = false;
	g_HardcodedBotInfo[BOT_CHEXMAN].bRevealedByDefault = false;
//	sprintf( g_HardcodedBotInfo[BOT_CHEXMAN].szScriptName, "CHEXBOT" );
	sprintf( g_HardcodedBotInfo[BOT_CHEXMAN].szScriptName, "DFULTBOT" );
	g_HardcodedBotInfo[BOT_CHEXMAN].szChatFile[0] = 0;
	sprintf( g_HardcodedBotInfo[BOT_CHEXMAN].szChatLump, "bots/chatfiles/chexman.txt" );

	// BOT_DAISY
	sprintf( g_HardcodedBotInfo[BOT_DAISY].szName, "Daisy" );
	g_HardcodedBotInfo[BOT_DAISY].Accuracy = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_DAISY].Intellect = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_DAISY].Evade = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_DAISY].Anticipation = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_DAISY].ReactionTime = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_DAISY].Perception = BOTSKILL_MEDIUM;
	sprintf( g_HardcodedBotInfo[BOT_DAISY].szFavoriteWeapon, "chaingun" );
	sprintf( g_HardcodedBotInfo[BOT_DAISY].szColor, "7f 80 3f" );
	sprintf( g_HardcodedBotInfo[BOT_DAISY].szGender, "male" );
	sprintf( g_HardcodedBotInfo[BOT_DAISY].szSkinName, "daisy" );
	sprintf( g_HardcodedBotInfo[BOT_DAISY].szClassName, "cleric" );
	g_HardcodedBotInfo[BOT_DAISY].ulRailgunColor = RAILCOLOR_RAINBOW;
	g_HardcodedBotInfo[BOT_DAISY].ulChatFrequency = 50;
	g_HardcodedBotInfo[BOT_DAISY].bRevealed = false;
	g_HardcodedBotInfo[BOT_DAISY].bRevealedByDefault = false;
	sprintf( g_HardcodedBotInfo[BOT_DAISY].szScriptName, "DFULTBOT" );
	g_HardcodedBotInfo[BOT_DAISY].szChatFile[0] = 0;
	sprintf( g_HardcodedBotInfo[BOT_DAISY].szChatLump, "bots/chatfiles/daisy.txt" );

	// BOT_LINMENGJU
	sprintf( g_HardcodedBotInfo[BOT_LINMENGJU].szName, "\\cgL\\ciI\\cfN \\cdM\\cfe\\chn\\cgg\\ccj\\ciu" );
	g_HardcodedBotInfo[BOT_LINMENGJU].Accuracy = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_LINMENGJU].Intellect = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_LINMENGJU].Evade = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_LINMENGJU].Anticipation = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_LINMENGJU].ReactionTime = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_LINMENGJU].Perception = BOTSKILL_MEDIUM;
	sprintf( g_HardcodedBotInfo[BOT_LINMENGJU].szFavoriteWeapon, "shotgun" );
	sprintf( g_HardcodedBotInfo[BOT_LINMENGJU].szColor, "8f 5f 2f" );
	sprintf( g_HardcodedBotInfo[BOT_LINMENGJU].szGender, "male" );
	sprintf( g_HardcodedBotInfo[BOT_LINMENGJU].szSkinName, "base" );
	sprintf( g_HardcodedBotInfo[BOT_LINMENGJU].szClassName, "cleric" );
	g_HardcodedBotInfo[BOT_LINMENGJU].ulRailgunColor = RAILCOLOR_RED;
	g_HardcodedBotInfo[BOT_LINMENGJU].ulChatFrequency = 80;
	g_HardcodedBotInfo[BOT_LINMENGJU].bRevealed = false;
	g_HardcodedBotInfo[BOT_LINMENGJU].bRevealedByDefault = false;
	sprintf( g_HardcodedBotInfo[BOT_LINMENGJU].szScriptName, "HUMANBOT" );
	g_HardcodedBotInfo[BOT_LINMENGJU].szChatFile[0] = 0;
	sprintf( g_HardcodedBotInfo[BOT_LINMENGJU].szChatLump, "bots/chatfiles/fob.txt" );

	// BOT_GOLDENFATTIE
	sprintf( g_HardcodedBotInfo[BOT_GOLDENFATTIE].szName, "Golden Fattie" );
	g_HardcodedBotInfo[BOT_GOLDENFATTIE].Accuracy = BOTSKILL_SUPREME;
	g_HardcodedBotInfo[BOT_GOLDENFATTIE].Intellect = BOTSKILL_SUPREME;
	g_HardcodedBotInfo[BOT_GOLDENFATTIE].Evade = BOTSKILL_SUPREME;
	g_HardcodedBotInfo[BOT_GOLDENFATTIE].Anticipation = BOTSKILL_SUPREME;
	g_HardcodedBotInfo[BOT_GOLDENFATTIE].ReactionTime = BOTSKILL_SUPREME;
	g_HardcodedBotInfo[BOT_GOLDENFATTIE].Perception = BOTSKILL_SUPREME;
	sprintf( g_HardcodedBotInfo[BOT_GOLDENFATTIE].szFavoriteWeapon, "minigun" );
	sprintf( g_HardcodedBotInfo[BOT_GOLDENFATTIE].szColor, "a0 a0 00" );
	sprintf( g_HardcodedBotInfo[BOT_GOLDENFATTIE].szGender, "male" );
	sprintf( g_HardcodedBotInfo[BOT_GOLDENFATTIE].szSkinName, "big fat doomguy" );
	sprintf( g_HardcodedBotInfo[BOT_GOLDENFATTIE].szClassName, "cleric" );
	g_HardcodedBotInfo[BOT_GOLDENFATTIE].ulRailgunColor = RAILCOLOR_GOLD;
	g_HardcodedBotInfo[BOT_GOLDENFATTIE].ulChatFrequency = 33;
	g_HardcodedBotInfo[BOT_GOLDENFATTIE].bRevealed = false;
	g_HardcodedBotInfo[BOT_GOLDENFATTIE].bRevealedByDefault = false;
	sprintf( g_HardcodedBotInfo[BOT_GOLDENFATTIE].szScriptName, "FATBOT" );
//	sprintf( g_HardcodedBotInfo[BOT_GOLDENFATTIE].szScriptName, "DFULTBOT" );
	g_HardcodedBotInfo[BOT_GOLDENFATTIE].szChatFile[0] = 0;
	sprintf( g_HardcodedBotInfo[BOT_GOLDENFATTIE].szChatLump, "bots/chatfiles/chubbs.txt" );

	// BOT_QUOTEBOT
	sprintf( g_HardcodedBotInfo[BOT_QUOTEBOT].szName, "Quotebot" );
	g_HardcodedBotInfo[BOT_QUOTEBOT].Accuracy = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_QUOTEBOT].Intellect = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_QUOTEBOT].Evade = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_QUOTEBOT].Anticipation = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_QUOTEBOT].ReactionTime = BOTSKILL_MEDIUM;
	g_HardcodedBotInfo[BOT_QUOTEBOT].Perception = BOTSKILL_MEDIUM;
	sprintf( g_HardcodedBotInfo[BOT_QUOTEBOT].szFavoriteWeapon, "chaingun" );
	sprintf( g_HardcodedBotInfo[BOT_QUOTEBOT].szColor, "ef c0 80" );
	sprintf( g_HardcodedBotInfo[BOT_QUOTEBOT].szGender, "male" );
	sprintf( g_HardcodedBotInfo[BOT_QUOTEBOT].szSkinName, "doomcrate" );
	sprintf( g_HardcodedBotInfo[BOT_QUOTEBOT].szClassName, "random" );
	g_HardcodedBotInfo[BOT_QUOTEBOT].ulRailgunColor = RAILCOLOR_BLUE;
	g_HardcodedBotInfo[BOT_QUOTEBOT].ulChatFrequency = 95;
	g_HardcodedBotInfo[BOT_QUOTEBOT].bRevealed = false;
	g_HardcodedBotInfo[BOT_QUOTEBOT].bRevealedByDefault = false;
//	sprintf( g_HardcodedBotInfo[BOT_QUOTEBOT].szScriptName, "QUOTEBOT" );
	sprintf( g_HardcodedBotInfo[BOT_QUOTEBOT].szScriptName, "DFULTBOT" );
	g_HardcodedBotInfo[BOT_QUOTEBOT].szChatFile[0] = 0;
	sprintf( g_HardcodedBotInfo[BOT_QUOTEBOT].szChatLump, "bots/chatfiles/quotebot.txt" );

	// Initialize all botinfo pointers.
	for ( ulIdx = 0; ulIdx < MAX_BOTINFO; ulIdx++ )
	{
		// We have a bunch of bots that are hardcoded.
		if ( ulIdx < NUM_HARDCODED_BOTS )
			g_BotInfo[ulIdx] = &g_HardcodedBotInfo[ulIdx];
		else
			g_BotInfo[ulIdx] = NULL;
	}

	// Initialize the botspawn table.
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		g_BotSpawn[ulIdx].szName[0] = 0;
		g_BotSpawn[ulIdx].szTeam[0] = 0;
		g_BotSpawn[ulIdx].ulTick = 0;
	}

	// Initialize the "is initialized" table.
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
		g_bBotIsInitialized[ulIdx] = false;

	// Call BOTS_Destruct() when Skulltag closes.
	atterm( BOTS_Destruct );
}

//*****************************************************************************
//
void BOTS_Tick( void )
{
	ULONG	ulIdx;

	// First, handle any bots waiting to be spawned in skirmish games.
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( g_BotSpawn[ulIdx].ulTick )
		{
			// It's time to spawn our bot.
			if ( --g_BotSpawn[ulIdx].ulTick == 0 )
			{
				ULONG		ulPlayerIdx = BOTS_FindFreePlayerSlot( );
				CSkullBot	*pBot = NULL;

				if ( ulPlayerIdx == MAXPLAYERS )
				{
					Printf( "The maximum number of players/bots has been reached.\n" );
					break;
				}

				// Verify that the bot's name matches one of the existing bots.
				if ( BOTS_IsValidName( g_BotSpawn[ulIdx].szName ))
					pBot = new CSkullBot( g_BotSpawn[ulIdx].szName, g_BotSpawn[ulIdx].szTeam, BOTS_FindFreePlayerSlot( ));
				else
				{
					Printf( PRINT_HIGH, "Invalid bot name: %s\n", g_BotSpawn[ulIdx].szName );
					break;
				}

				g_BotSpawn[ulIdx].szName[0] = 0;
				g_BotSpawn[ulIdx].szTeam[0] = 0;
			}
		}
	}
}

//*****************************************************************************
//
void BOTS_Destruct( void )
{
	ULONG	ulIdx;

	// First, go through and free all additional botinfo's.
	for ( ulIdx = 0; ulIdx < MAX_BOTINFO; ulIdx++ )
	{
		if ( ulIdx < NUM_HARDCODED_BOTS )
			continue;

		if ( g_BotInfo[ulIdx] != NULL )
		{
			free( g_BotInfo[ulIdx] );
			g_BotInfo[ulIdx] = NULL;
		}
	}
}

//*****************************************************************************
//
bool BOTS_AddBotInfo( BOTINFO_t *pBotInfo )
{
	ULONG	ulIdx;

	// First, find a free slot to add the botinfo.
	for ( ulIdx = 0; ulIdx < MAX_BOTINFO; ulIdx++ )
	{
		if ( g_BotInfo[ulIdx] != NULL )
			continue;

		// Allocate some memory for this new block.
		g_BotInfo[ulIdx] = (BOTINFO_t *)malloc( sizeof( BOTINFO_t ));

		// Now copy all the data we passed in into this block.
		g_BotInfo[ulIdx]->bRevealed						= pBotInfo->bRevealed;
		g_BotInfo[ulIdx]->bRevealedByDefault			= pBotInfo->bRevealedByDefault;
		g_BotInfo[ulIdx]->Accuracy						= pBotInfo->Accuracy;
		g_BotInfo[ulIdx]->Anticipation					= pBotInfo->Anticipation;
		g_BotInfo[ulIdx]->Evade							= pBotInfo->Evade;
		g_BotInfo[ulIdx]->Intellect						= pBotInfo->Intellect;
		g_BotInfo[ulIdx]->ulRailgunColor				= pBotInfo->ulRailgunColor;
		g_BotInfo[ulIdx]->ulChatFrequency				= pBotInfo->ulChatFrequency;
		g_BotInfo[ulIdx]->ReactionTime					= pBotInfo->ReactionTime;
		g_BotInfo[ulIdx]->Perception					= pBotInfo->Perception;
		sprintf( g_BotInfo[ulIdx]->szFavoriteWeapon,	"%s", pBotInfo->szFavoriteWeapon );
		sprintf( g_BotInfo[ulIdx]->szClassName,			"%s", pBotInfo->szClassName );
		sprintf( g_BotInfo[ulIdx]->szColor,				"%s", pBotInfo->szColor );
		sprintf( g_BotInfo[ulIdx]->szGender,			"%s", pBotInfo->szGender );
		sprintf( g_BotInfo[ulIdx]->szSkinName,			"%s", pBotInfo->szSkinName );
		sprintf( g_BotInfo[ulIdx]->szName,				"%s", pBotInfo->szName );
		sprintf( g_BotInfo[ulIdx]->szScriptName,		"%s", pBotInfo->szScriptName );
		sprintf( g_BotInfo[ulIdx]->szChatFile,			"%s", pBotInfo->szChatFile );
		sprintf( g_BotInfo[ulIdx]->szChatLump,			"%s", pBotInfo->szChatLump );

		return ( true );
	}

	return ( false );
}

//*****************************************************************************
//
void BOTS_ParseBotInfo( void )
{
	LONG		lCurLump;
	LONG		lLastLump = 0;
	char		szKey[64];
	char		szValue[128];

	// Search through all loaded wads for a lump called "BOTINFO".
	while (( lCurLump = Wads.FindLump( "BOTINFO", (int *)&lLastLump )) != -1 )
	{
		// Make pszBotInfo point to the raw data (which should be a text file) in the BOTINFO lump.
		SC_OpenLumpNum( lCurLump, "BOTINFO" );

		// Begin parsing that text. COM_Parse will create a token (com_token), and
		// pszBotInfo will skip past the token.
		while ( SC_GetString( ))
		{
			BOTINFO_t	BotInfo;

			// Initialize our botinfo variable.
			BotInfo.bRevealed					= true;
			BotInfo.bRevealedByDefault			= true;
			BotInfo.Accuracy					= BOTSKILL_MEDIUM;
			BotInfo.Anticipation				= BOTSKILL_MEDIUM;
			BotInfo.Evade						= BOTSKILL_MEDIUM;
			BotInfo.Intellect					= BOTSKILL_MEDIUM;
			BotInfo.ulRailgunColor				= 0;
			BotInfo.ulChatFrequency				= 50;
			BotInfo.ReactionTime				= BOTSKILL_MEDIUM;
			BotInfo.Perception					= BOTSKILL_MEDIUM;
			sprintf( BotInfo.szFavoriteWeapon, 	"pistol" );
			sprintf( BotInfo.szClassName,		"random" );
			sprintf( BotInfo.szColor,			"00 00 00" );
			sprintf( BotInfo.szGender,			"male" );
			sprintf( BotInfo.szName,			"UNNAMED BOT" );
			sprintf( BotInfo.szScriptName,		"" );
			sprintf( BotInfo.szSkinName,		"base" );
			sprintf( BotInfo.szChatFile,		"" );
			sprintf( BotInfo.szChatLump,		"" );

			while ( sc_String[0] != '{' )
				SC_GetString( );

			// We've encountered a starting bracket. Now continue to parse until we hit an end bracket.
			while ( sc_String[0] != '}' )
			{
				// The current token should be our key. (key = value) If it's an end bracket, break.
				SC_GetString( );
				sprintf( szKey, sc_String );
				if ( sc_String[0] == '}' )
					break;

				// The following key must be an = sign. If not, the user made an error!
				SC_GetString( );
				if ( stricmp( sc_String, "=" ) != 0 )
					I_Error( "BOTS_ParseBotInfo: Missing \"=\" in BOTINFO lump for field \"%s\"!", szKey );

				// The last token should be our value.
				SC_GetString( );
				sprintf( szValue, sc_String );

				// Now try to match our key with a valid bot info field.
				if ( stricmp( szKey, "name" ) == 0 )
				{
					sprintf( BotInfo.szName, szValue );
				}
				else if ( stricmp( szKey, "accuracy" ) == 0 )
				{
					switch ( atoi( szValue ))
					{
					case 0:
					case 1:
					case 2:
					case 3:
					case 4:

						BotInfo.Accuracy = (BOTSKILL_e)( atoi( szValue ) + (LONG)BOTSKILL_LOW );
						break;
					default:

						I_Error( "BOTS_ParseBotInfo: Expected value from 0-4 for field \"accuracy\"!" );
						break;
					}
				}
				else if (( stricmp( szKey, "intelect" ) == 0 ) || ( stricmp( szKey, "intellect" ) == 0 ))
				{
					switch ( atoi( szValue ))
					{
					case 0:
					case 1:
					case 2:
					case 3:
					case 4:

						BotInfo.Intellect = (BOTSKILL_e)( atoi( szValue ) + (LONG)BOTSKILL_LOW );
						break;
					default:

						I_Error( "BOTS_ParseBotInfo: Expected value from 0-4 for field \"intellect\"!" );
						break;
					}
				}
				else if ( stricmp( szKey, "evade" ) == 0 )
				{
					switch ( atoi( szValue ))
					{
					case 0:
					case 1:
					case 2:
					case 3:
					case 4:

						BotInfo.Evade = (BOTSKILL_e)( atoi( szValue ) + (LONG)BOTSKILL_LOW );
						break;
					default:

						I_Error( "BOTS_ParseBotInfo: Expected value from 0-4 for field \"evade\"!" );
						break;
					}
				}
				else if ( stricmp( szKey, "anticipation" ) == 0 )
				{
					switch ( atoi( szValue ))
					{
					case 0:
					case 1:
					case 2:
					case 3:
					case 4:

						BotInfo.Anticipation = (BOTSKILL_e)( atoi( szValue ) + (LONG)BOTSKILL_LOW );
						break;
					default:

						I_Error( "BOTS_ParseBotInfo: Expected value from 0-4 for field \"anticipation\"!" );
						break;
					}
				}
				else if ( stricmp( szKey, "reactiontime" ) == 0 )
				{
					switch ( atoi( szValue ))
					{
					case 0:
					case 1:
					case 2:
					case 3:
					case 4:

						BotInfo.ReactionTime = (BOTSKILL_e)( atoi( szValue ) + (LONG)BOTSKILL_LOW );
						break;
					default:

						I_Error( "BOTS_ParseBotInfo: Expected value from 0-4 for field \"reactiontime\"!" );
						break;
					}
				}
				else if ( stricmp( szKey, "perception" ) == 0 )
				{
					switch ( atoi( szValue ))
					{
					case 0:
					case 1:
					case 2:
					case 3:
					case 4:

						BotInfo.Perception = (BOTSKILL_e)( atoi( szValue ) + (LONG)BOTSKILL_LOW );
						break;
					default:

						I_Error( "BOTS_ParseBotInfo: Expected value from 0-4 for field \"perception\"!" );
						break;
					}
				}
				else if ( stricmp( szKey, "favoriteweapon" ) == 0 )
				{
					sprintf( BotInfo.szFavoriteWeapon, szValue );
				}
				else if ( stricmp( szKey, "class" ) == 0 )
				{
					sprintf( BotInfo.szClassName, szValue );
				}
				else if ( stricmp( szKey, "color" ) == 0 )
				{
					sprintf( BotInfo.szColor, szValue );
				}
				else if ( stricmp( szKey, "gender" ) == 0 )
				{
					sprintf( BotInfo.szGender, szValue );
				}
				else if ( stricmp( szKey, "skin" ) == 0 )
				{
					sprintf( BotInfo.szSkinName, szValue );
				}
				else if ( stricmp( szKey, "railcolor" ) == 0 )
				{
					if ( stricmp( szValue, "blue" ) == 0 )
						BotInfo.ulRailgunColor = RAILCOLOR_BLUE;
					else if ( stricmp( szValue, "red" ) == 0 )
						BotInfo.ulRailgunColor = RAILCOLOR_RED;
					else if ( stricmp( szValue, "yellow" ) == 0 )
						BotInfo.ulRailgunColor = RAILCOLOR_YELLOW;
					else if ( stricmp( szValue, "black" ) == 0 )
						BotInfo.ulRailgunColor = RAILCOLOR_BLACK;
					else if ( stricmp( szValue, "silver" ) == 0 )
						BotInfo.ulRailgunColor = RAILCOLOR_SILVER;
					else if ( stricmp( szValue, "gold" ) == 0 )
						BotInfo.ulRailgunColor = RAILCOLOR_GOLD;
					else if ( stricmp( szValue, "green" ) == 0 )
						BotInfo.ulRailgunColor = RAILCOLOR_GREEN;
					else if ( stricmp( szValue, "white" ) == 0 )
						BotInfo.ulRailgunColor = RAILCOLOR_WHITE;
					else if ( stricmp( szValue, "purple" ) == 0 )
						BotInfo.ulRailgunColor = RAILCOLOR_PURPLE;
					else if ( stricmp( szValue, "orange" ) == 0 )
						BotInfo.ulRailgunColor = RAILCOLOR_ORANGE;
					else if ( stricmp( szValue, "rainbow" ) == 0 )
						BotInfo.ulRailgunColor = RAILCOLOR_RAINBOW;
					else
						BotInfo.ulRailgunColor = atoi( szValue );
				}
				else if ( stricmp( szKey, "chatfrequency" ) == 0 )
				{
					BotInfo.ulChatFrequency = atoi( szValue );
					if (( BotInfo.ulChatFrequency < 0 ) || ( BotInfo.ulChatFrequency > 100 ))
						I_Error( "BOTS_ParseBotInfo: Expected value from 0-100 for field \"chatfrequency\"!" );
				}
				else if ( stricmp( szKey, "revealed" ) == 0 )
				{
					if ( stricmp( szValue, "true" ) == 0 )
						BotInfo.bRevealed = true;
					else if ( stricmp( szValue, "false" ) == 0 )
						BotInfo.bRevealed = false;
					else
						BotInfo.bRevealed = !!atoi( szValue );

					BotInfo.bRevealedByDefault = BotInfo.bRevealed;
				}
				else if ( stricmp( szKey, "script" ) == 0 )
				{
					if ( strlen( szValue ) > 8 )
						I_Error( "BOTS_ParseBotInfo: Value for BOTINFO property \"script\" (\"%s\") cannot exceed 8 characters!", szValue );

					sprintf( BotInfo.szScriptName, szValue );
				}
				else if ( stricmp( szKey, "chatfile" ) == 0 )
				{
					sprintf( BotInfo.szChatFile, szValue );
				}
				else if ( stricmp( szKey, "chatlump" ) == 0 )
				{
					if ( strlen( szValue ) > 8 )
						I_Error( "BOTS_ParseBotInfo: Value for BOTINFO property \"chatlump\" (\"%s\") cannot exceed 8 characters!", szValue );

					sprintf( BotInfo.szChatLump, szValue );
				}
				else
					I_Error( "BOTS_ParseBotInfo: Unknown BOTINFO property, \"%s\"!", szKey );
			}

			// Finally, add our completed botinfo.
			BOTS_AddBotInfo( &BotInfo );
		}
	}
}

//*****************************************************************************
//
bool BOTS_IsValidName( char *pszName )
{
	if ( pszName == NULL )
		return ( false );
	else
	{
		ULONG	ulIdx;
		char	szName[64];

		// Search through the botinfo. If the given name matches one of the names, return true.
		for ( ulIdx = 0; ulIdx < MAX_BOTINFO; ulIdx++ )
		{
			if ( g_BotInfo[ulIdx] == NULL )
				continue;

			sprintf( szName, g_BotInfo[ulIdx]->szName );
			V_ColorizeString( szName );
			V_RemoveColorCodes( szName );
			if ( stricmp( szName, pszName ) == 0 )
				return ( true );
		}

		// Didn't find anything.
		return ( false );
	}
}

//*****************************************************************************
//
ULONG BOTS_FindFreePlayerSlot( void )
{
	ULONG	ulIdx;

	// Don't allow us to add bots past the max. clients limit.
	if (( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( SERVER_CalcNumPlayers( ) >= sv_maxclients ))
		return ( MAXPLAYERS );

	// Loop through all the player slots. If it's an inactive slot, break.
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( playeringame[ulIdx] == false )
			break;
	}

	return ( ulIdx );
}

//*****************************************************************************
//
void BOTS_RemoveBot( ULONG ulPlayerIdx, bool bExitMsg )
{
	ULONG	ulIdx;

	if ( ulPlayerIdx >= MAXPLAYERS )
		return;

	if ( players[ulPlayerIdx].pSkullBot == false )
		return;

	if ( bExitMsg )
	{
		if ( NETWORK_GetState( ) != NETSTATE_SERVER )
			Printf( PRINT_HIGH, "%s \\c-left the game.\n", players[ulPlayerIdx].userinfo.netname );
		else
			SERVER_Printf( PRINT_HIGH, "%s \\c-left the game.\n", players[ulPlayerIdx].userinfo.netname );
	}

	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		// Remove the bot from the game.
		SERVERCOMMANDS_DisconnectPlayer( ulPlayerIdx, ulPlayerIdx, SVCF_SKIPTHISCLIENT );

		// Update the scoreboard.
		SERVERCONSOLE_RemovePlayer( ulPlayerIdx );
	}

	// If he's carrying an important item like a flag, etc., make sure he, 
	// drops it before he leaves.
	if ( players[ulPlayerIdx].mo )
		players[ulPlayerIdx].mo->DropImportantItems( true );

	// If this bot was eligible to get an assist, cancel that.
	if ( TEAM_GetAssistPlayer( TEAM_BLUE ) == ulPlayerIdx )
		TEAM_SetAssistPlayer( TEAM_BLUE, MAXPLAYERS );
	if ( TEAM_GetAssistPlayer( TEAM_RED ) == ulPlayerIdx )
		TEAM_SetAssistPlayer( TEAM_RED, MAXPLAYERS );

	playeringame[ulPlayerIdx] = false;

	if ( g_bBotIsInitialized[ulPlayerIdx] )
		players[ulPlayerIdx].pSkullBot->PreDelete( );
/*
	else
	{
		if ( players[ulPlayerIdx].pSkullBot->m_bHasScript )
		{
			players[ulPlayerIdx].pSkullBot->m_bHasScript = false;
//			players[ulPlayerIdx].pSkullBot->m_ScriptData.RawData.CloseOnDestruct = false;
		}
	}
*/
	delete ( players[ulPlayerIdx].pSkullBot );
	players[ulPlayerIdx].pSkullBot = NULL;
	players[ulPlayerIdx].bIsBot = false;

	// Tell the join queue module that a player has left the game.
	if (( PLAYER_IsTrueSpectator( &players[ulPlayerIdx] ) == false ) && ( gameaction != ga_worlddone ))
		JOINQUEUE_PlayerLeftGame( true );

	// If this bot was the enemy of another bot, tell the bot.
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if (( playeringame[ulIdx] == false ) || ( players[ulIdx].pSkullBot == NULL ) || ( g_bBotIsInitialized[ulIdx] == false ))
			continue;

		if ( players[ulIdx].pSkullBot->m_ulPlayerEnemy == ulPlayerIdx )
			players[ulIdx].pSkullBot->m_ulPlayerEnemy = MAXPLAYERS;
	}

	// Refresh the HUD since the number of players in the game is potentially changing.
	SCOREBOARD_RefreshHUD( );
}

//*****************************************************************************
//
void BOTS_RemoveAllBots( bool bExitMsg )
{
	ULONG	ulIdx;

	// Loop through all the players and delete all the bots.
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( playeringame[ulIdx] && players[ulIdx].pSkullBot && g_bBotIsInitialized[ulIdx] )
			BOTS_RemoveBot( ulIdx, bExitMsg );
	}
}

//*****************************************************************************
//
void BOTS_ResetCyclesCounter( void )
{
	g_BotCycles = 0;
}

//*****************************************************************************
//
// TEEEEEEEEEEEEEMP
bool PTR_AimTraverse (intercept_t* in);

bool BOTS_IsPathObstructed( fixed_t Distance, AActor *pSource )
{
	angle_t			Angle;
	angle_t			Pitch;
	angle_t			Meat;
	FTraceResults	TraceResults;
	fixed_t			vx, vy, vz, sz;

	Meat = pSource->angle;
	Angle = /*pSource->angle*/Meat >> ANGLETOFINESHIFT;
	Pitch = 0;//pSource->pitch >> ANGLETOFINESHIFT;

	vx = FixedMul( finecosine[Pitch], finecosine[Angle] );
	vy = FixedMul( finecosine[Pitch], finesine[Angle] );
//	vx = FixedMul( pSource->x, Distance );
//	vy = FixedMul( pSource->y, Distance );
	vz = finesine[Pitch];

	sz = pSource->z - pSource->floorclip + pSource->height;// + (fixed_t)(chase_height * FRACUNIT);

//	if ( P_PathTraverse( CurPos.x, CurPos.y, DestPos.x, DestPos.y, PT_ADDLINES|PT_ADDTHINGS, PTR_AimTraverse ) == false )
//	return ( P_PathTraverse( pSource->x, pSource->y, vx, vy, PT_ADDLINES|PT_ADDTHINGS, PTR_AimTraverse ) == false );
	if ( Trace( pSource->x,	// Source X
				pSource->y,		// Source Y
				pSource->z + gameinfo.StepHeight,//sz,				// Source Z
				pSource->Sector,// Source sector
				vx,
				vy,
				vz,
				Distance * FRACUNIT,	// Maximum search distance
				MF_SOLID|MF_SHOOTABLE,		// Actor mask (ignore actors without this flag)
				0,				// Wall mask
				pSource,		// Actor to ignore
				TraceResults	// Callback
				))
	{
		/*
		if ( TraceResults.Line )
		{
			Printf( "Line\n---Flags: " );
			if ( TraceResults.Line->flags & ML_BLOCKING )
				Printf( "BLOCKING " );
			if ( TraceResults.Line->flags & ML_BLOCKMONSTERS )
				Printf( "BLOCKMONSTERS " );
			if ( TraceResults.Line->flags & ML_TWOSIDED )
				Printf( "TWOSIDED " );
			if ( TraceResults.Line->flags & ML_DONTPEGTOP )
				Printf( "UPPERUNPEG " );
			if ( TraceResults.Line->flags & ML_DONTPEGBOTTOM )
				Printf( "LOWERUNPEG " );
			if ( TraceResults.Line->flags & ML_SECRET )
				Printf( "SECRET " );
			if ( TraceResults.Line->flags & ML_SOUNDBLOCK )
				Printf( "SOUNDBLOCK " );
			if ( TraceResults.Line->flags & ML_DONTDRAW )
				Printf( "INVIS " );
			if ( TraceResults.Line->flags & ML_MAPPED )
				Printf( "ONMAP " );
			if ( TraceResults.Line->flags & ML_REPEAT_SPECIAL )
				Printf( "REPEATABLE " );
			if ( TraceResults.Line->flags & ML_MONSTERSCANACTIVATE )
				Printf( "MONSTERACTIVATE " );
			if ( TraceResults.Line->flags & ML_PASSUSE_BOOM )
				Printf( "PASSUSE " );
			if ( TraceResults.Line->flags & ML_BLOCKEVERYTHING )
				Printf( "BLOCKALL " );
			Printf( "\n" );
		}
		*/
		return ( true );
	}
	
	return ( false );
}

//*****************************************************************************
//
bool BOTS_IsVisible( AActor *pActor1, AActor *pActor2 )
{
	angle_t	Angle;

	// Check if we have a line of sight to this object.
	if ( P_CheckSight( pActor1, pActor2, 2 ) == false )
		return ( false );

	Angle = R_PointToAngle2( pActor1->x,
					  pActor1->y, 
					  pActor2->x,
					  pActor2->y );

	Angle -= pActor1->angle;

	// If the object within our view range, tell the bot.
	return (( Angle <= ANG45 ) || ( Angle >= ((ULONG)ANGLE_1 * 315 )));
}

//*****************************************************************************
//
void BOTS_ArchiveRevealedBotsAndSkins( FConfigFile *f )
{
	ULONG	ulIdx;
	char	szString[64];

	for ( ulIdx = 0; ulIdx < MAX_BOTINFO; ulIdx++ )
	{
		if ( g_BotInfo[ulIdx] == NULL )
			continue;

		// If this bot isn't revealed, or isn't revealed by default, don't archive it.
		if (( g_BotInfo[ulIdx]->bRevealed == false ) || ( g_BotInfo[ulIdx]->bRevealedByDefault ))
			continue;

		sprintf( szString, "\"%s\"", g_BotInfo[ulIdx]->szName );

		V_ColorizeString( szString );
		V_RemoveColorCodes( szString );
		f->SetValueForKey( szString, "1" );
	}

	for ( ulIdx = 0; ulIdx < (ULONG)numskins; ulIdx++ )
	{
		// If this bot isn't revealed, or isn't revealed by default, don't archive it.
		if (( skins[ulIdx].bRevealed == false ) || ( skins[ulIdx].bRevealedByDefault ))
			continue;

		sprintf( szString, "\"%s\"", skins[ulIdx].name );

		V_ColorizeString( szString );
		V_RemoveColorCodes( szString );
		f->SetValueForKey( szString, "1" );
	}
}

//*****************************************************************************
//
void BOTS_RestoreRevealedBotsAndSkins( FConfigFile &config )
{
	char		szBuffer[64];
	ULONG		ulIdx;
	const char	*pszKey;
	const char	*pszValue;

	while ( config.NextInSection ( pszKey, pszValue ))
	{
		for ( ulIdx = 0; ulIdx < MAX_BOTINFO; ulIdx++ )
		{
			if ( g_BotInfo[ulIdx] == NULL )
				continue;

			if ( g_BotInfo[ulIdx]->bRevealed )
				continue;

			sprintf( szBuffer, g_BotInfo[ulIdx]->szName );
			V_ColorizeString( szBuffer );
			V_RemoveColorCodes( szBuffer );

			if ( strnicmp( pszKey + 1, szBuffer, strlen( szBuffer )) != 0 )
				continue;

			g_BotInfo[ulIdx]->bRevealed = true;
		}

		for ( ulIdx = 0; ulIdx < (ULONG)numskins; ulIdx++ )
		{
			if ( skins[ulIdx].bRevealed )
				continue;

			sprintf( szBuffer, skins[ulIdx].name );
			V_ColorizeString( szBuffer );
			V_RemoveColorCodes( szBuffer );

			if ( strnicmp( pszKey + 1, szBuffer, strlen( szBuffer )) != 0 )
				continue;

			skins[ulIdx].bRevealed = true;
		}
	}
}

//*****************************************************************************
//
bool BOTS_IsBotInitialized( ULONG ulBot )
{
	return ( g_bBotIsInitialized[ulBot] );
}

//*****************************************************************************
//
BOTSKILL_e BOTS_AdjustSkill( CSkullBot *pBot, BOTSKILL_e Skill )
{
	LONG	lSkill;

	lSkill = (LONG)Skill;
	switch ( botskill )
	{
	// In easy mode, take two points off the skill level.
	case 0:

		lSkill -= 2;
		break;
	// In somewhat easy, take off one point.
	case 1:

		lSkill -= 1;
		break;
	// In somewhat hard, add one point.
	case 3:

		lSkill += 1;
		break;
	// In nightmare, add two points.
	case 4:

		lSkill += 2;
		break;
	}

	if ( pBot->m_bSkillIncrease )
		lSkill++;
	if ( pBot->m_bSkillDecrease )
		lSkill--;

	if ( lSkill < BOTSKILL_VERYPOOR )
		lSkill = (LONG)BOTSKILL_VERYPOOR;
	else if ( lSkill >= NUM_BOT_SKILLS )
		lSkill = NUM_BOT_SKILLS - 1;

	return ( (BOTSKILL_e)lSkill );
}

//*****************************************************************************
//
void BOTS_PostWeaponFiredEvent( ULONG ulPlayer, BOTEVENT_e EventIfSelf, BOTEVENT_e EventIfEnemy, BOTEVENT_e EventIfPlayer )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if (( playeringame[ulIdx] == false ) || ( players[ulIdx].pSkullBot == false ))
			continue;

		if ( players[ulIdx].pSkullBot->m_ulPlayerEnemy == ulPlayer )
			players[ulIdx].pSkullBot->PostEvent( EventIfEnemy );
		else if ( ulPlayer == ulIdx )
			players[ulIdx].pSkullBot->PostEvent( EventIfSelf );
		else
			players[ulIdx].pSkullBot->PostEvent( EventIfPlayer );
	}
}

//*****************************************************************************
//*****************************************************************************
//
ULONG BOTINFO_GetNumBotInfos( void )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAX_BOTINFO; ulIdx++ )
	{
		if ( g_BotInfo[ulIdx] == NULL )
			return ( ulIdx );
	}

	return ( ulIdx );
}

//*****************************************************************************
//
char *BOTINFO_GetName( ULONG ulIdx )
{
	if (( ulIdx >= MAX_BOTINFO ) || ( g_BotInfo[ulIdx] == NULL ))
		return ( NULL );

	return ( g_BotInfo[ulIdx]->szName );
}

//*****************************************************************************
//
BOTSKILL_e BOTINFO_GetAccuracy( ULONG ulIdx )
{
	if (( ulIdx >= MAX_BOTINFO ) || ( g_BotInfo[ulIdx] == NULL ))
		return ( BOTSKILL_VERYPOOR );

	return ( g_BotInfo[ulIdx]->Accuracy );
}

//*****************************************************************************
//
BOTSKILL_e BOTINFO_GetIntellect( ULONG ulIdx )
{
	if (( ulIdx >= MAX_BOTINFO ) || ( g_BotInfo[ulIdx] == NULL ))
		return ( BOTSKILL_VERYPOOR );

	return ( g_BotInfo[ulIdx]->Intellect );
}

//*****************************************************************************
//
BOTSKILL_e BOTINFO_GetEvade( ULONG ulIdx )
{
	if (( ulIdx >= MAX_BOTINFO ) || ( g_BotInfo[ulIdx] == NULL ))
		return ( BOTSKILL_VERYPOOR );

	return ( g_BotInfo[ulIdx]->Evade );
}

//*****************************************************************************
//
BOTSKILL_e BOTINFO_GetAnticipation( ULONG ulIdx )
{
	if (( ulIdx >= MAX_BOTINFO ) || ( g_BotInfo[ulIdx] == NULL ))
		return ( BOTSKILL_VERYPOOR );

	return ( g_BotInfo[ulIdx]->Anticipation );
}

//*****************************************************************************
//
BOTSKILL_e BOTINFO_GetReactionTime( ULONG ulIdx )
{
	if (( ulIdx >= MAX_BOTINFO ) || ( g_BotInfo[ulIdx] == NULL ))
		return ( BOTSKILL_VERYPOOR );

	return ( g_BotInfo[ulIdx]->ReactionTime );
}

//*****************************************************************************
//
BOTSKILL_e BOTINFO_GetPerception( ULONG ulIdx )
{
	if (( ulIdx >= MAX_BOTINFO ) || ( g_BotInfo[ulIdx] == NULL ))
		return ( BOTSKILL_VERYPOOR );

	return ( g_BotInfo[ulIdx]->Perception );
}

//*****************************************************************************
//
char *BOTINFO_GetFavoriteWeapon( ULONG ulIdx )
{
	if (( ulIdx >= MAX_BOTINFO ) || ( g_BotInfo[ulIdx] == NULL ))
		return ( NULL );

	return ( g_BotInfo[ulIdx]->szFavoriteWeapon );
}

//*****************************************************************************
//
char *BOTINFO_GetColor( ULONG ulIdx )
{
	if (( ulIdx >= MAX_BOTINFO ) || ( g_BotInfo[ulIdx] == NULL ))
		return ( NULL );

	return ( g_BotInfo[ulIdx]->szColor );
}

//*****************************************************************************
//
char *BOTINFO_GetGender( ULONG ulIdx )
{
	if (( ulIdx >= MAX_BOTINFO ) || ( g_BotInfo[ulIdx] == NULL ))
		return ( NULL );

	return ( g_BotInfo[ulIdx]->szGender );
}

//*****************************************************************************
//
char *BOTINFO_GetClass( ULONG ulIdx )
{
	if (( ulIdx >= MAX_BOTINFO ) || ( g_BotInfo[ulIdx] == NULL ))
		return ( NULL );

	return ( g_BotInfo[ulIdx]->szClassName );
}

//*****************************************************************************
//
char *BOTINFO_GetSkin( ULONG ulIdx )
{
	if (( ulIdx >= MAX_BOTINFO ) || ( g_BotInfo[ulIdx] == NULL ))
		return ( NULL );

	return ( g_BotInfo[ulIdx]->szSkinName );
}

//*****************************************************************************
//
ULONG BOTINFO_GetRailgunColor( ULONG ulIdx )
{
	if (( ulIdx >= MAX_BOTINFO ) || ( g_BotInfo[ulIdx] == NULL ))
		return ( -1 );

	return ( g_BotInfo[ulIdx]->ulRailgunColor );
}

//*****************************************************************************
//
ULONG BOTINFO_GetChatFrequency( ULONG ulIdx )
{
	if (( ulIdx >= MAX_BOTINFO ) || ( g_BotInfo[ulIdx] == NULL ))
		return ( -1 );

	return ( g_BotInfo[ulIdx]->ulChatFrequency );
}

//*****************************************************************************
//
bool BOTINFO_GetRevealed( ULONG ulIdx )
{
	if (( ulIdx >= MAX_BOTINFO ) || ( g_BotInfo[ulIdx] == NULL ))
		return ( false );

	return ( g_BotInfo[ulIdx]->bRevealed );
}

//*****************************************************************************
//
char *BOTINFO_GetChatFile( ULONG ulIdx )
{
	if (( ulIdx >= MAX_BOTINFO ) || ( g_BotInfo[ulIdx] == NULL ))
		return ( NULL );

	return ( g_BotInfo[ulIdx]->szChatFile );
}

//*****************************************************************************
//
char *BOTINFO_GetChatLump( ULONG ulIdx )
{
	if (( ulIdx >= MAX_BOTINFO ) || ( g_BotInfo[ulIdx] == NULL ))
		return ( NULL );

	return ( g_BotInfo[ulIdx]->szChatLump );
}

//*****************************************************************************
//*****************************************************************************
//
void BOTSPAWN_AddToTable( char *pszBotName, char *pszBotTeam )
{
	ULONG	ulIdx;
	ULONG	ulTick;

	if (( pszBotName == NULL ) || ( pszBotName[0] == 0 ))
		return;

	ulTick = TICRATE;
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( g_BotSpawn[ulIdx].szName[0] )
		{
			ulTick += TICRATE;
			continue;
		}

		sprintf( g_BotSpawn[ulIdx].szName, pszBotName );
		if (( pszBotTeam == NULL ) || ( pszBotTeam[0] == 0 ))
			g_BotSpawn[ulIdx].szTeam[0] = 0;
		else
			sprintf( g_BotSpawn[ulIdx].szTeam, pszBotTeam );
		g_BotSpawn[ulIdx].ulTick = ulTick;
		break;
	}
}

//*****************************************************************************
//
void BOTSPAWN_ClearTable( void )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		g_BotSpawn[ulIdx].szName[0] = 0;
		g_BotSpawn[ulIdx].szTeam[0] = 0;
		g_BotSpawn[ulIdx].ulTick = 0;
	}
}

//*****************************************************************************
//
char *BOTSPAWN_GetName( ULONG ulIdx )
{
	if ( ulIdx >= MAXPLAYERS )
		return ( NULL );

	return ( g_BotSpawn[ulIdx].szName );
}

//*****************************************************************************
//
void BOTSPAWN_SetName( ULONG ulIdx, char *pszName )
{
	if ( ulIdx >= MAXPLAYERS )
		return;

	if ( pszName == NULL )
		g_BotSpawn[ulIdx].szName[0] = 0;
	else
		sprintf( g_BotSpawn[ulIdx].szName, pszName );
}

//*****************************************************************************
//
char *BOTSPAWN_GetTeam( ULONG ulIdx )
{
	if ( ulIdx >= MAXPLAYERS )
		return ( NULL );

	return ( g_BotSpawn[ulIdx].szTeam );
}

//*****************************************************************************
//
void BOTSPAWN_SetTeam( ULONG ulIdx, char *pszTeam )
{
	if ( ulIdx >= MAXPLAYERS )
		return;

	sprintf( g_BotSpawn[ulIdx].szTeam, pszTeam );
}

//*****************************************************************************
//
ULONG BOTSPAWN_GetTicks( ULONG ulIdx )
{
	if ( ulIdx >= MAXPLAYERS )
		return ( NULL );

	return ( g_BotSpawn[ulIdx].ulTick );
}

//*****************************************************************************
//
void BOTSPAWN_SetTicks( ULONG ulIdx, ULONG ulTicks )
{
	if ( ulIdx >= MAXPLAYERS )
		return;

	g_BotSpawn[ulIdx].ulTick = ulTicks;
}

//*****************************************************************************
//*****************************************************************************
//
CSkullBot::CSkullBot( char *pszName, char *pszTeamName, ULONG ulPlayerNum )
{
	ULONG	ulIdx;

	g_bBotIsInitialized[ulPlayerNum] = false;

	// First, initialize all variables.
	m_posTarget.x = 0;
	m_posTarget.y = 0;
	m_posTarget.z = 0;
	m_pPlayer = NULL;
	m_ulBotInfoIdx = MAX_BOTINFO;
	m_bHasScript = false;
	m_lForwardMove = 0;
	m_lSideMove = 0;
	m_bForwardMovePersist = false;
	m_bSideMovePersist = false;
	m_ulLastSeenPlayer = MAXPLAYERS;
	m_ulLastPlayerDamagedBy = MAXPLAYERS;
	m_ulPlayerKilledBy = MAXPLAYERS;
	m_ulPlayerKilled = MAXPLAYERS;
	m_lButtons = 0;
	m_bAimAtEnemy = false;
	m_ulAimAtEnemyDelay = 0;
	m_AngleDelta = 0;
	m_AngleOffBy = 0;
	m_AngleDesired = 0;
	m_bTurnLeft = false;
	m_pGoalActor = NULL;
	m_ulPlayerEnemy = MAXPLAYERS;
	m_ulPathType = BOTPATHTYPE_NONE;
	m_PathGoalPos.x = 0;
	m_PathGoalPos.y = 0;
	m_PathGoalPos.z = 0;
	for ( ulIdx = 0; ulIdx < MAX_REACTION_TIME; ulIdx++ )
	{
		m_EnemyPosition[ulIdx].x = 0;
		m_EnemyPosition[ulIdx].y = 0;
		m_EnemyPosition[ulIdx].z = 0;
	}
	m_ulLastEnemyPositionTick = 0;
	m_bSkillIncrease = false;
	m_bSkillDecrease = false;
	m_ulLastMedalReceived = NUM_MEDALS;
	m_lQueueHead = 0;
	m_lQueueTail = 0;
	for ( ulIdx = 0; ulIdx < MAX_STORED_EVENTS; ulIdx++ )
	{
		m_StoredEventQueue[ulIdx].Event = NUM_BOTEVENTS;
		m_StoredEventQueue[ulIdx].lTick = -1;
	}

	// We've asked to spawn a certain bot by name. Search through the bot database and see
	// if there's a bot with the name we've asked for.
	if ( pszName )
	{
		char	szName[64];

		for ( ulIdx = 0; ulIdx < MAX_BOTINFO; ulIdx++ )
		{
			if ( g_BotInfo[ulIdx] != NULL )
			{
				sprintf( szName, g_BotInfo[ulIdx]->szName );
				V_ColorizeString( szName );
				V_RemoveColorCodes( szName );
				if ( stricmp( szName, pszName ) == 0 )
				{
					// If the bot was hidden, reveal it!
					if ( g_BotInfo[ulIdx]->bRevealed == false )
					{
						Printf( "Hidden bot \"%s\\c-\" has now been revealed!\n", g_BotInfo[ulIdx]->szName );
						g_BotInfo[ulIdx]->bRevealed = true;
					}

					m_ulBotInfoIdx = ulIdx;
					break;
				}
			}
		}
		// We've already handled the "what if there's no match" exception.
	}
	// If the user doesn't input a name, randomly select an available bot.
	else
	{
		ULONG	ulRandom;

		while ( m_ulBotInfoIdx == MAX_BOTINFO )
		{
			ulRandom = ( BotSpawn( ) % MAX_BOTINFO );
			if ( g_BotInfo[ulRandom] != NULL )
			{
				if ( g_BotInfo[ulRandom]->bRevealed )
					m_ulBotInfoIdx = ulRandom;
			}
		}
	}

	// Link the bot to the player.
	m_pPlayer = &players[ulPlayerNum];
	m_pPlayer->pSkullBot = this;
	m_pPlayer->bIsBot = true;
	m_pPlayer->bSpectating = false;

	// Update the playeringame slot.
	playeringame[ulPlayerNum] = true;

	// Setup the player's userinfo based on the bot's botinfo.
	sprintf( m_pPlayer->userinfo.netname, "%s", g_BotInfo[m_ulBotInfoIdx]->szName );
	V_ColorizeString( m_pPlayer->userinfo.netname );
	m_pPlayer->userinfo.color = V_GetColorFromString( NULL, g_BotInfo[m_ulBotInfoIdx]->szColor );
	if ( gameinfo.gametype != GAME_Hexen )
	{
		if ( g_BotInfo[m_ulBotInfoIdx]->szSkinName )
		{
			LONG	lSkin;

			lSkin = R_FindSkin( g_BotInfo[m_ulBotInfoIdx]->szSkinName, 0 );
			m_pPlayer->userinfo.skin = lSkin;

			// If the skin was hidden, reveal it!
			if ( skins[lSkin].bRevealed == false )
			{
				Printf( "Hidden skin \"%s\\c-\" has now been revealed!\n", skins[lSkin].name );
				skins[lSkin].bRevealed = true;
			}
		}
	}
	else
	{
		if ( g_BotInfo[m_ulBotInfoIdx]->szClassName )
		{
			// See if the given class name matches one in the global list.
			for ( ulIdx = 0; ulIdx < PlayerClasses.Size( ); ulIdx++ )
			{
				if ( stricmp( g_BotInfo[m_ulBotInfoIdx]->szClassName, PlayerClasses[ulIdx].Type->TypeName.GetChars( )) == 0 )
				{
					m_pPlayer->userinfo.PlayerClass = ( ulIdx - 1 );
					break;
				}
			}
		}
	}
	m_pPlayer->userinfo.lRailgunTrailColor = g_BotInfo[m_ulBotInfoIdx]->ulRailgunColor;
	m_pPlayer->userinfo.gender = D_GenderToInt( g_BotInfo[m_ulBotInfoIdx]->szGender );
	if ( pszTeamName )
	{
		// If we're in teamgame mode, put the bot on red or blue.
		if ( teamplay || teamgame || teamlms || teampossession )
		{
			// Pick a team for our bot
			if ( !stricmp( pszTeamName, TEAM_GetName( TEAM_BLUE )))
				PLAYER_SetTeam( m_pPlayer, TEAM_BLUE, true );
			else if ( !stricmp( pszTeamName, TEAM_GetName( TEAM_RED )))
				PLAYER_SetTeam( m_pPlayer, TEAM_RED, true );
			else
				PLAYER_SetTeam( m_pPlayer, TEAM_ChooseBestTeamForPlayer( ), true );
		}
	}
	else
	{
		// In certain modes, the bot NEEDS to be placed on a team, or else he will constantly
		// respawn.
		if ( teamplay || teamlms || teampossession )
			PLAYER_SetTeam( m_pPlayer, TEAM_ChooseBestTeamForPlayer( ), true );
	}

	// For now, bots always switch weapons on pickup.
	m_pPlayer->userinfo.switchonpickup = 2;
	m_pPlayer->userinfo.StillBob = 0;
	m_pPlayer->userinfo.MoveBob = 0.25;

	// If we've added the bot to a single player game, enable "fake multiplayer" mode.
	if ( NETWORK_GetState( ) == NETSTATE_SINGLE )
		NETWORK_SetState( NETSTATE_SINGLE_MULTIPLAYER );

	m_pPlayer->playerstate = PST_ENTER;
	m_pPlayer->fragcount = 0;
	m_pPlayer->killcount = 0;	
	m_pPlayer->ulDeathsWithoutFrag = 0;
	m_pPlayer->ulConsecutiveHits = 0;
	m_pPlayer->ulConsecutiveRailgunHits = 0;
	m_pPlayer->ulDeathCount = 0;
	m_pPlayer->ulFragsWithoutDeath = 0;
	m_pPlayer->ulLastExcellentTick = 0;
	m_pPlayer->ulLastFragTick = 0;
	m_pPlayer->ulLastBFGFragTick = 0;
	m_pPlayer->ulRailgunShots = 0;
	m_pPlayer->ulTime = 0;

	// Load the bot's script. If he doesn't have a script, inform the user.
	if ( g_BotInfo[m_ulBotInfoIdx]->szScriptName[0] )
	{
		ULONG		ulIdx;
		ULONG		ulIdx2;

		// This bot now has a script. Now, initialize all the script data.
		m_bHasScript = true;

		// Open the lump name specified.
		m_ScriptData.RawData = Wads.OpenLumpName( g_BotInfo[m_ulBotInfoIdx]->szScriptName );
		
		m_ScriptData.lScriptPos = 0;
		m_ScriptData.bExitingState = false;
		m_ScriptData.bInOnExit = false;
		m_ScriptData.bInEvent = false;
		m_ScriptData.lNextState = -1;
		for ( ulIdx = 0; ulIdx < MAX_NUM_STATES; ulIdx++ )
		{
			m_ScriptData.StatePositions[ulIdx].lPos = -1;
			m_ScriptData.StatePositions[ulIdx].lOnEnterPos = -1;
			m_ScriptData.StatePositions[ulIdx].lMainloopPos = -1;
			m_ScriptData.StatePositions[ulIdx].lOnExitPos = -1;
		}

		m_ScriptData.lNumGlobalEvents = 0;
		for ( ulIdx = 0; ulIdx < MAX_NUM_GLOBAL_EVENTS; ulIdx++ )
		{
			m_ScriptData.GlobalEventPositions[ulIdx].lPos = -1;
			m_ScriptData.GlobalEventPositions[ulIdx].Event = (BOTEVENT_e)-1;
		}

		for ( ulIdx = 0; ulIdx < MAX_NUM_STATES; ulIdx++ )
		{
			m_ScriptData.lNumEvents[ulIdx] = 0;

			for ( ulIdx2 = 0; ulIdx2 < MAX_NUM_EVENTS; ulIdx2++ )
			{
				m_ScriptData.EventPositions[ulIdx][ulIdx2].lPos = -1;
				m_ScriptData.EventPositions[ulIdx][ulIdx2].Event = (BOTEVENT_e)-1;
			}
		}

		for ( ulIdx = 0; ulIdx < MAX_SCRIPT_VARIABLES; ulIdx++ )
		{
			m_ScriptData.alScriptVariables[ulIdx] = 0;
		}

		for ( ulIdx = 0; ulIdx < MAX_NUM_STATES; ulIdx++ )
		{
			for ( ulIdx2 = 0; ulIdx2 < MAX_STATE_VARIABLES; ulIdx2++ )
				m_ScriptData.alStateVariables[ulIdx][ulIdx2] = 0;
		}

		m_ScriptData.lCurrentStateIdx = -1;
		m_ScriptData.ulScriptDelay = 0;
		m_ScriptData.ulScriptEventDelay = 0;

		// Parse through the script to determine where the various states and events are located.
		this->GetStatePositions( );

		// Now that we've gotten the state positions, we can set the script position to either
		// the onenter or mainloop position of the spawn state.
		for ( ulIdx = 0; ulIdx < MAX_NUM_STATES; ulIdx++ )
		{
			if ( stricmp( m_ScriptData.szStateName[ulIdx], "statespawn" ) == 0 )
			{
				if ( m_ScriptData.StatePositions[ulIdx].lOnEnterPos != -1 )
					m_ScriptData.lScriptPos = m_ScriptData.StatePositions[ulIdx].lOnEnterPos;
				else
					m_ScriptData.lScriptPos = m_ScriptData.StatePositions[ulIdx].lMainloopPos;

				m_ScriptData.lCurrentStateIdx = ulIdx;
				break;
			}
		}

		if ( ulIdx == MAX_NUM_STATES )
			I_Error( "Could not find spawn state in bot %s's script!", m_pPlayer->userinfo.netname );

		m_ScriptData.lStackPosition = 0;
		m_ScriptData.lStringStackPosition = 0;

		for ( ulIdx = 0; ulIdx < BOTSCRIPT_STACK_SIZE; ulIdx++ )
		{
			m_ScriptData.alStack[ulIdx] = 0;
			m_ScriptData.aszStringStack[ulIdx][0] = 0;
		}
	}
	else
	{
		Printf( "%s does not have a script specified. %s will not do anything.\n", players[ulPlayerNum].userinfo.netname, players[ulPlayerNum].userinfo.gender == GENDER_MALE ? "He" : players[ulPlayerNum].userinfo.gender == GENDER_MALE ? "She" : "It" );
	}

	// Check and see if this bot should spawn as a spectator.
	m_pPlayer->bSpectating = PLAYER_ShouldSpawnAsSpectator( m_pPlayer );

	// Spawn the bot at its appropriate team start.
	if ( teamgame )
	{
		if ( players[ulPlayerNum].bOnTeam )
			G_TeamgameSpawnPlayer( ulPlayerNum, players[ulPlayerNum].ulTeam, true );
		else
			G_TemporaryTeamSpawnPlayer( ulPlayerNum, true );
	}
	// If deathmatch, just spawn at a random spot.
	else if ( deathmatch )
		G_DeathMatchSpawnPlayer( ulPlayerNum, true );
	// Otherwise, just spawn at their normal player start.
	else
		G_CooperativeSpawnPlayer( ulPlayerNum, true );

	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
		Printf( "%s \\c-entered the game.\n", players[ulPlayerNum].userinfo.netname );
	else
	{
		// Let the other players know that this bot has entered the game.
		SERVER_Printf( PRINT_HIGH, "%s \\c-entered the game.\n", players[ulPlayerNum].userinfo.netname );

		// Add this player to the scoreboard.
		SERVERCONSOLE_AddNewPlayer( ulPlayerNum );

		// Now send out the player's userinfo out to other players.
		SERVERCOMMANDS_SetPlayerUserInfo( ulPlayerNum, USERINFO_ALL );
	}

	g_bBotIsInitialized[ulPlayerNum] = true;

	BOTCMD_SetLastJoinedPlayer( GetPlayer( )->userinfo.netname );

	// Tell the bots that a new players has joined the game!
	{
		ULONG	ulIdx;

		for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
		{
			if ( playeringame[ulIdx] == false )
				continue;

			// Don't tell the bot that joined that it joined the game.
			if ( ulIdx == ulPlayerNum )
				continue;

			if ( players[ulIdx].pSkullBot )
				players[ulIdx].pSkullBot->PostEvent( BOTEVENT_PLAYER_JOINEDGAME );
		}
	}

	// If this bot spawned as a spectator, let him know.
	if ( m_pPlayer->bSpectating )
		PostEvent( BOTEVENT_SPECTATING );

	// Refresh the HUD since a new player is now here (this affects the number of players in the game).
	SCOREBOARD_RefreshHUD( );
}

//*****************************************************************************
//
CSkullBot::~CSkullBot( )
{
}

//*****************************************************************************
//
void CSkullBot::Tick( void )
{
	ticcmd_t	*cmd = &m_pPlayer->cmd;
	AActor		*mo = m_pPlayer->mo;

	clock( g_BotCycles );

	// Don't execute bot logic during demos, or if the console player is a client.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) || demoplayback )
		return;

	// Reset the bots keypresses.
	memset( cmd, 0, sizeof( ticcmd_t ));

	// Bot does not have a script. Nothing to do.
	if ( m_bHasScript == false )
		return;

	// Don't run their script if the game is frozen.
	if ( GAME_GetFreezeMode( ))
		return;

	// Check to see if there's any events that need to be executed.
	while ( m_lQueueHead != m_lQueueTail )
	{
		if ( gametic >= ( m_StoredEventQueue[m_lQueueHead].lTick ))
			DeleteEventFromQueue( );
		else
			break;
	}

	// Is the bot's script is currently paused?
	if ( m_ScriptData.bInEvent )
	{
		if ( m_ScriptData.ulScriptEventDelay )
		{
			m_ScriptData.ulScriptEventDelay--;

			this->EndTick( );
			return;
		}
	}
	else if ( m_ScriptData.ulScriptDelay )
	{
		m_ScriptData.ulScriptDelay--;

		this->EndTick( );
		return;
	}

	// Parse the bots's script. This executes the bot's logic.
	this->ParseScript( );

	this->EndTick( );
}

//*****************************************************************************
//
void CSkullBot::EndTick( void )
{
	if ( m_bForwardMovePersist )
		m_pPlayer->cmd.ucmd.forwardmove = m_lForwardMove << 8;
	if ( m_bSideMovePersist )
		m_pPlayer->cmd.ucmd.sidemove = m_lSideMove << 8;
	m_pPlayer->cmd.ucmd.buttons |= m_lButtons;

	unclock( g_BotCycles );

	if ( botdebug_states && ( m_pPlayer->mo->CheckLocalView( consoleplayer )) && ( NETWORK_GetState( ) != NETSTATE_SERVER ))
		Printf( "%s: %s\n", m_pPlayer->userinfo.netname, m_ScriptData.szStateName[m_ScriptData.lCurrentStateIdx] );
}

//*****************************************************************************
//
void CSkullBot::PostEvent( BOTEVENT_e Event )
{
//	LONG		lBuffer;
	ULONG		ulIdx;
	BOTSKILL_e	Skill;

	// No script to post the event to.
	if ( m_bHasScript == false )
		return;

//	if ( botdebug_showevents )
//		Printf( "%s: %s\n", GetPlayer( )->userinfo.netname, g_pszEventNames[Event] );

	Skill = BOTS_AdjustSkill( this, BOTINFO_GetReactionTime( m_ulBotInfoIdx ));
	switch ( Skill )
	{
	case BOTSKILL_VERYPOOR:

		AddEventToQueue( Event, gametic + 25 );
		return;
	case BOTSKILL_POOR:

		AddEventToQueue( Event, gametic + 19 );
		return;
	case BOTSKILL_LOW:

		AddEventToQueue( Event, gametic + 14 );
		return;
	case BOTSKILL_MEDIUM:

		AddEventToQueue( Event, gametic + 10 );
		return;
	case BOTSKILL_HIGH:

		AddEventToQueue( Event, gametic + 7 );
		return;
	case BOTSKILL_EXCELLENT:

		AddEventToQueue( Event, gametic + 5 );
		return;
	case BOTSKILL_SUPREME:

		AddEventToQueue( Event, gametic + 3 );
		return;
	case BOTSKILL_GODLIKE:

		AddEventToQueue( Event, gametic + 1 );
		return;
	// A bot with perfect reaction time can instantly react to events.
	case BOTSKILL_PERFECT:

		break;
	}

	// First, scan global events.
	for ( ulIdx = 0; ulIdx < m_ScriptData.lNumGlobalEvents; ulIdx++ )
	{
		// Found a matching event.
		if ( m_ScriptData.GlobalEventPositions[ulIdx].Event == Event )
		{
			if ( m_ScriptData.bInEvent == false )
			{
				// Save the current position in the script, since we will go back to it if we do not change
				// states.
				m_ScriptData.lSavedScriptPos = m_ScriptData.lScriptPos;

				m_ScriptData.bInEvent = true;
			}

			m_ScriptData.lScriptPos = m_ScriptData.GlobalEventPositions[ulIdx].lPos;
			m_ScriptData.RawData.Seek( m_ScriptData.lScriptPos, SEEK_SET );

			// Parse after the event.
			m_ScriptData.ulScriptEventDelay = 0;
			ParseScript( );
			return;
		}
	}

	// If the event wasn't found in the global events, scan state events.
	for ( ulIdx = 0; ulIdx < MAX_NUM_EVENTS; ulIdx++ )
	{
		// Found a matching event.
		if ( m_ScriptData.EventPositions[m_ScriptData.lCurrentStateIdx][ulIdx].Event == Event )
		{
			if ( m_ScriptData.bInEvent == false )
			{
				// Save the current position in the script, since we will go back to it if we do not change
				// states.
				m_ScriptData.lSavedScriptPos = m_ScriptData.lScriptPos;

				m_ScriptData.bInEvent = true;
			}

			m_ScriptData.lScriptPos = m_ScriptData.EventPositions[m_ScriptData.lCurrentStateIdx][ulIdx].lPos;
			m_ScriptData.RawData.Seek( m_ScriptData.lScriptPos, SEEK_SET );

			// Parse after the event.
			m_ScriptData.ulScriptEventDelay = 0;
			ParseScript( );
			return;
		}
	}
}

//*****************************************************************************
//
void CSkullBot::ParseScript( void )
{
	bool	bStopParsing;
	LONG	lCommandHeader;
	LONG	lBuffer;
	LONG	lVariable;
	LONG	lNumOperations = 0;
//	LONG	lExpectedStackPosition;

	bStopParsing = false;
	while ( bStopParsing == false )
	{
		m_ScriptData.RawData.Seek( m_ScriptData.lScriptPos, SEEK_SET );
		m_ScriptData.RawData.Read( &lCommandHeader, sizeof( LONG ));
		m_ScriptData.lScriptPos += sizeof( LONG );

		if ( botdebug_dataheaders )
			Printf( "%s\n", g_pszDataHeaders[lCommandHeader] );

		if ( lNumOperations++ >= 8192 )
			I_Error( "ParseScript: Infinite loop detected in bot %s's script!", GetPlayer( )->userinfo.netname );

//		lExpectedStackPosition = m_ScriptData.lStackPosition + g_lExpectedStackChange[lCommandHeader];
//		g_lLastHeader = lCommandHeader;
		switch ( lCommandHeader )
		{
		// Looped back around to the beginning of the main loop.
		case DH_MAINLOOP:

			break;
		// We encountered a command.
		case DH_COMMAND:

			// Read in the command.
			m_ScriptData.RawData.Read( &lBuffer, sizeof( LONG ));
			m_ScriptData.lScriptPos += sizeof( LONG );

			if (( lBuffer < 0 ) || ( lBuffer >= NUM_BOTCMDS ))
				I_Error( "ParseScript: Unknown command %d, in state %s!", lBuffer, m_ScriptData.szStateName[m_ScriptData.lCurrentStateIdx] );
			else
				BOTCMD_RunCommand( (BOTCMD_e)lBuffer, this );

			if ( m_ScriptData.bExitingState && ( m_ScriptData.bInOnExit == false ))
			{
				// If this state has an onexit section, go to that.
				if ( m_ScriptData.StatePositions[m_ScriptData.lCurrentStateIdx].lOnExitPos != -1 )
				{
					m_ScriptData.RawData.Seek( m_ScriptData.StatePositions[m_ScriptData.lCurrentStateIdx].lOnExitPos, SEEK_SET );
					m_ScriptData.lScriptPos = m_ScriptData.StatePositions[m_ScriptData.lCurrentStateIdx].lOnExitPos;

					m_ScriptData.bInOnExit = true;
				}
				// Otherwise, move on to the next state.
				else if ( m_ScriptData.StatePositions[m_ScriptData.lNextState].lOnEnterPos != -1 )
				{
					m_ScriptData.RawData.Seek( m_ScriptData.StatePositions[m_ScriptData.lNextState].lOnEnterPos, SEEK_SET );
					m_ScriptData.lScriptPos = m_ScriptData.StatePositions[m_ScriptData.lNextState].lOnEnterPos;

					if ( botdebug_statechanges )
						Printf( "leaving %s, entering %s\n", m_ScriptData.szStateName[m_ScriptData.lCurrentStateIdx], m_ScriptData.szStateName[m_ScriptData.lNextState] );

					m_ScriptData.lCurrentStateIdx = m_ScriptData.lNextState;
					m_ScriptData.bExitingState = false;
					bStopParsing = true;
				}
				else if ( m_ScriptData.StatePositions[m_ScriptData.lNextState].lMainloopPos != -1 )
				{
					m_ScriptData.RawData.Seek( m_ScriptData.StatePositions[m_ScriptData.lNextState].lMainloopPos, SEEK_SET );
					m_ScriptData.lScriptPos = m_ScriptData.StatePositions[m_ScriptData.lNextState].lMainloopPos;

					if ( botdebug_statechanges )
						Printf( "leaving %s, entering %s\n", m_ScriptData.szStateName[m_ScriptData.lCurrentStateIdx], m_ScriptData.szStateName[m_ScriptData.lNextState] );

					m_ScriptData.lCurrentStateIdx = m_ScriptData.lNextState;
					m_ScriptData.bExitingState = false;
					bStopParsing = true;
				}
				else
					I_Error( "ParseScript: Couldn't find onenter or mainloop section in state %s!\n", m_ScriptData.szStateName[m_ScriptData.lNextState] );

				// If we're changing states, kill the script delay.
				m_ScriptData.ulScriptDelay = 0;
				m_ScriptData.ulScriptEventDelay = 0;
				m_ScriptData.bInEvent = false;
			}

			// If delay has been called, delay the script.
			if ( m_ScriptData.bInEvent )
			{
				if ( m_ScriptData.ulScriptEventDelay )
					bStopParsing = true;
			}
			else if ( m_ScriptData.ulScriptDelay )
				bStopParsing = true;

			break;
		case DH_ENDONENTER:

			// Read the next longword. It must be the start of the main loop.
			m_ScriptData.RawData.Read( &lBuffer, sizeof( LONG ));
			m_ScriptData.lScriptPos += sizeof( LONG );

			if ( lBuffer != DH_MAINLOOP )
				I_Error( "ParseSecton: Missing mainloop in state %s!", m_ScriptData.szStateName[m_ScriptData.lCurrentStateIdx] );

//			if ( m_ScriptData.lStackPosition != 0 )
//				I_Error( "ParseScript: Stack position not 0 at DH_ENDONENTER!" );
			break;
		case DH_ENDMAINLOOP:

			m_ScriptData.lScriptPos = m_ScriptData.StatePositions[m_ScriptData.lCurrentStateIdx].lMainloopPos;
			m_ScriptData.RawData.Seek( m_ScriptData.lScriptPos, SEEK_SET );
			bStopParsing = true;

//			if ( m_ScriptData.lStackPosition != 0 )
//				I_Error( "ParseScript: Stack position not 0 at DH_ENDMAINLOOP!" );
			break;
		case DH_ONEXIT:

			bStopParsing = true;
			break;
		case DH_ENDONEXIT:

			// Otherwise, move on to the next state.
			if ( m_ScriptData.StatePositions[m_ScriptData.lNextState].lOnEnterPos != -1 )
			{
				m_ScriptData.RawData.Seek( m_ScriptData.StatePositions[m_ScriptData.lNextState].lOnEnterPos, SEEK_SET );
				m_ScriptData.lScriptPos = m_ScriptData.StatePositions[m_ScriptData.lNextState].lOnEnterPos;
			}
			else if ( m_ScriptData.StatePositions[m_ScriptData.lNextState].lMainloopPos != -1 )
			{
				m_ScriptData.RawData.Seek( m_ScriptData.StatePositions[m_ScriptData.lNextState].lMainloopPos, SEEK_SET );
				m_ScriptData.lScriptPos = m_ScriptData.StatePositions[m_ScriptData.lNextState].lMainloopPos;
			}
			else
				I_Error( "ParseScript: Couldn't find onenter or mainloop section in state %s!\n", m_ScriptData.szStateName[m_ScriptData.lNextState] );

			if ( botdebug_statechanges )
				Printf( "leaving %s, entering %s\n", m_ScriptData.szStateName[m_ScriptData.lCurrentStateIdx], m_ScriptData.szStateName[m_ScriptData.lNextState] );

			m_ScriptData.lCurrentStateIdx = m_ScriptData.lNextState;
			m_ScriptData.bInOnExit = false;
			m_ScriptData.bExitingState = false;
			bStopParsing = true;

//			if ( m_ScriptData.lStackPosition != 0 )
//				I_Error( "ParseScript: Stack position not 0 at DH_ENDONEXIT!" );
			break;
		case DH_ENDEVENT:

			// If a changestate command wasn't called, simply go back to the main loop.
			m_ScriptData.lScriptPos = m_ScriptData.lSavedScriptPos;
			m_ScriptData.RawData.Seek( m_ScriptData.lScriptPos, SEEK_SET );

			bStopParsing = true;
			m_ScriptData.bInEvent = false;

//			if ( m_ScriptData.lStackPosition != 0 )
//				I_Error( "ParseScript: Stack position not 0 at DH_ENDEVENT!" );

			break;
		case DH_PUSHNUMBER:

			m_ScriptData.RawData.Read( &lBuffer, sizeof( LONG ));
			m_ScriptData.lScriptPos += sizeof( LONG );

			PushToStack( lBuffer );
			break;
		case DH_PUSHSTRINGINDEX:

			m_ScriptData.RawData.Read( &lBuffer, sizeof( LONG ));
			m_ScriptData.lScriptPos += sizeof( LONG );

			PushToStringStack( m_ScriptData.szStringList[lBuffer] );
			break;
		case DH_PUSHGLOBALVAR:

			m_ScriptData.RawData.Read( &lBuffer, sizeof( LONG ));
			m_ScriptData.lScriptPos += sizeof( LONG );

			PushToStack( m_ScriptData.alScriptVariables[lBuffer] );
			break;
		case DH_PUSHLOCALVAR:

			m_ScriptData.RawData.Read( &lBuffer, sizeof( LONG ));
			m_ScriptData.lScriptPos += sizeof( LONG );

			PushToStack( m_ScriptData.alStateVariables[m_ScriptData.lCurrentStateIdx][lBuffer] );
			break;
		case DH_IFGOTO:

			m_ScriptData.RawData.Read( &lBuffer, sizeof( LONG ));
			m_ScriptData.lScriptPos += sizeof( LONG );

			if ( m_ScriptData.alStack[m_ScriptData.lStackPosition - 1] )
			{
				m_ScriptData.lScriptPos = lBuffer;
				m_ScriptData.RawData.Seek( m_ScriptData.lScriptPos, SEEK_SET );
			}
			PopStack( );
			break;
		case DH_IFNOTGOTO:

			m_ScriptData.RawData.Read( &lBuffer, sizeof( LONG ));
			m_ScriptData.lScriptPos += sizeof( LONG );

			if ( m_ScriptData.alStack[m_ScriptData.lStackPosition - 1] == 0 )
			{
				m_ScriptData.lScriptPos = lBuffer;
				m_ScriptData.RawData.Seek( m_ScriptData.lScriptPos, SEEK_SET );
			}
			PopStack( );
			break;
		case DH_GOTO:

			m_ScriptData.RawData.Read( &lBuffer, sizeof( LONG ));
			m_ScriptData.lScriptPos += sizeof( LONG );

			m_ScriptData.lScriptPos = lBuffer;
			m_ScriptData.RawData.Seek( m_ScriptData.lScriptPos, SEEK_SET );
			break;
		case DH_DROPSTACKPOSITION:

			PopStack( );
			break;
		case DH_ORLOGICAL:

			m_ScriptData.alStack[m_ScriptData.lStackPosition - 2] = ( m_ScriptData.alStack[m_ScriptData.lStackPosition - 2] || m_ScriptData.alStack[m_ScriptData.lStackPosition - 1] );
			PopStack( );
			break;
		case DH_ANDLOGICAL:

			m_ScriptData.alStack[m_ScriptData.lStackPosition - 2] = ( m_ScriptData.alStack[m_ScriptData.lStackPosition - 2] && m_ScriptData.alStack[m_ScriptData.lStackPosition - 1] );
			PopStack( );
			break;
		case DH_ORBITWISE:

			m_ScriptData.alStack[m_ScriptData.lStackPosition - 2] = ( m_ScriptData.alStack[m_ScriptData.lStackPosition - 2] | m_ScriptData.alStack[m_ScriptData.lStackPosition - 1] );
			PopStack( );
			break;
		case DH_EORBITWISE:

			m_ScriptData.alStack[m_ScriptData.lStackPosition - 2] = ( m_ScriptData.alStack[m_ScriptData.lStackPosition - 2] ^ m_ScriptData.alStack[m_ScriptData.lStackPosition - 1] );
			PopStack( );
			break;
		case DH_ANDBITWISE:

			m_ScriptData.alStack[m_ScriptData.lStackPosition - 2] = ( m_ScriptData.alStack[m_ScriptData.lStackPosition - 2] & m_ScriptData.alStack[m_ScriptData.lStackPosition - 1] );
			PopStack( );
			break;
		case DH_EQUALS:

			m_ScriptData.alStack[m_ScriptData.lStackPosition - 2] = ( m_ScriptData.alStack[m_ScriptData.lStackPosition - 2] == m_ScriptData.alStack[m_ScriptData.lStackPosition - 1] );
			PopStack( );
			break;
		case DH_NOTEQUALS:

			m_ScriptData.alStack[m_ScriptData.lStackPosition - 2] = ( m_ScriptData.alStack[m_ScriptData.lStackPosition - 2] != m_ScriptData.alStack[m_ScriptData.lStackPosition - 1] );
			PopStack( );
			break;
		case DH_LESSTHAN:

			m_ScriptData.alStack[m_ScriptData.lStackPosition - 2] = ( m_ScriptData.alStack[m_ScriptData.lStackPosition - 2] < m_ScriptData.alStack[m_ScriptData.lStackPosition - 1] );
			PopStack( );
			break;
		case DH_LESSTHANEQUALS:

			m_ScriptData.alStack[m_ScriptData.lStackPosition - 2] = ( m_ScriptData.alStack[m_ScriptData.lStackPosition - 2] <= m_ScriptData.alStack[m_ScriptData.lStackPosition - 1] );
			PopStack( );
			break;
		case DH_GREATERTHAN:

			m_ScriptData.alStack[m_ScriptData.lStackPosition - 2] = ( m_ScriptData.alStack[m_ScriptData.lStackPosition - 2] > m_ScriptData.alStack[m_ScriptData.lStackPosition - 1] );
			PopStack( );
			break;
		case DH_GREATERTHANEQUALS:

			m_ScriptData.alStack[m_ScriptData.lStackPosition - 2] = ( m_ScriptData.alStack[m_ScriptData.lStackPosition - 2] >= m_ScriptData.alStack[m_ScriptData.lStackPosition - 1] );
			PopStack( );
			break;
		case DH_NEGATELOGICAL:

			m_ScriptData.alStack[m_ScriptData.lStackPosition - 1] = !m_ScriptData.alStack[m_ScriptData.lStackPosition - 1];
			break;
		case DH_LSHIFT:

			m_ScriptData.alStack[m_ScriptData.lStackPosition - 2] = ( m_ScriptData.alStack[m_ScriptData.lStackPosition - 2] << m_ScriptData.alStack[m_ScriptData.lStackPosition - 1] );
			PopStack( );
			break;
		case DH_RSHIFT:

			m_ScriptData.alStack[m_ScriptData.lStackPosition - 2] = ( m_ScriptData.alStack[m_ScriptData.lStackPosition - 2] >> m_ScriptData.alStack[m_ScriptData.lStackPosition - 1] );
			PopStack( );
			break;
		case DH_ADD:

			m_ScriptData.alStack[m_ScriptData.lStackPosition - 2] = ( m_ScriptData.alStack[m_ScriptData.lStackPosition - 2] + m_ScriptData.alStack[m_ScriptData.lStackPosition - 1] );
			PopStack( );
			break;
		case DH_SUBTRACT:

			m_ScriptData.alStack[m_ScriptData.lStackPosition - 2] = ( m_ScriptData.alStack[m_ScriptData.lStackPosition - 2] - m_ScriptData.alStack[m_ScriptData.lStackPosition - 1] );
			PopStack( );
			break;
		case DH_UNARYMINUS:

			m_ScriptData.alStack[m_ScriptData.lStackPosition - 1] = -m_ScriptData.alStack[m_ScriptData.lStackPosition - 1];
			break;
		case DH_MULTIPLY:

			m_ScriptData.alStack[m_ScriptData.lStackPosition - 2] = ( m_ScriptData.alStack[m_ScriptData.lStackPosition - 2] * m_ScriptData.alStack[m_ScriptData.lStackPosition - 1] );
			PopStack( );
			break;
		case DH_DIVIDE:

			if ( m_ScriptData.alStack[m_ScriptData.lStackPosition - 1] == 0 )
				I_Error( "ParseScript: Illegal divide by 0 in bot %s's script!", m_pPlayer->userinfo.netname );

			m_ScriptData.alStack[m_ScriptData.lStackPosition - 2] = ( m_ScriptData.alStack[m_ScriptData.lStackPosition - 2] / m_ScriptData.alStack[m_ScriptData.lStackPosition - 1] );
			PopStack( );
			break;
		case DH_MODULUS:

			m_ScriptData.alStack[m_ScriptData.lStackPosition - 2] = ( m_ScriptData.alStack[m_ScriptData.lStackPosition - 2] % m_ScriptData.alStack[m_ScriptData.lStackPosition - 1] );
			PopStack( );
			break;
		case DH_SCRIPTVARLIST:

			// This doesn't do much now.
			m_ScriptData.RawData.Read( &lBuffer, sizeof( LONG ));
			m_ScriptData.lScriptPos += sizeof( LONG );
			break;
		case DH_INCGLOBALVAR:

			m_ScriptData.RawData.Read( &lVariable, sizeof( LONG ));
			m_ScriptData.lScriptPos += sizeof( LONG );

			m_ScriptData.alScriptVariables[lVariable]++;
			break;
		case DH_DECGLOBALVAR:

			m_ScriptData.RawData.Read( &lVariable, sizeof( LONG ));
			m_ScriptData.lScriptPos += sizeof( LONG );

			m_ScriptData.alScriptVariables[lVariable]--;
			break;
		case DH_ASSIGNGLOBALVAR:

			m_ScriptData.RawData.Read( &lVariable, sizeof( LONG ));
			m_ScriptData.lScriptPos += sizeof( LONG );

			m_ScriptData.alScriptVariables[lVariable] = m_ScriptData.alStack[m_ScriptData.lStackPosition - 1];
			PopStack( );
			break;
		case DH_ADDGLOBALVAR:

			m_ScriptData.RawData.Read( &lVariable, sizeof( LONG ));
			m_ScriptData.lScriptPos += sizeof( LONG );

			m_ScriptData.alScriptVariables[lVariable] += m_ScriptData.alStack[m_ScriptData.lStackPosition - 1];
			PopStack( );
			break;
		case DH_SUBGLOBALVAR:

			m_ScriptData.RawData.Read( &lVariable, sizeof( LONG ));
			m_ScriptData.lScriptPos += sizeof( LONG );

			m_ScriptData.alScriptVariables[lVariable] -= m_ScriptData.alStack[m_ScriptData.lStackPosition - 1];
			PopStack( );
			break;
		case DH_MULGLOBALVAR:

			m_ScriptData.RawData.Read( &lVariable, sizeof( LONG ));
			m_ScriptData.lScriptPos += sizeof( LONG );

			m_ScriptData.alScriptVariables[lVariable] *= m_ScriptData.alStack[m_ScriptData.lStackPosition - 1];
			PopStack( );
			break;
		case DH_DIVGLOBALVAR:

			m_ScriptData.RawData.Read( &lVariable, sizeof( LONG ));
			m_ScriptData.lScriptPos += sizeof( LONG );

			if ( m_ScriptData.alStack[m_ScriptData.lStackPosition - 1] == 0 )
				I_Error( "ParseScript: Illegal divide by 0 occured while trying to divide global variable in bot %s's script!!", m_pPlayer->userinfo.netname );

			m_ScriptData.alScriptVariables[lVariable] /= m_ScriptData.alStack[m_ScriptData.lStackPosition - 1];
			PopStack( );
			break;
		case DH_MODGLOBALVAR:

			m_ScriptData.RawData.Read( &lVariable, sizeof( LONG ));
			m_ScriptData.lScriptPos += sizeof( LONG );

			m_ScriptData.alScriptVariables[lVariable] %= m_ScriptData.alStack[m_ScriptData.lStackPosition - 1];
			PopStack( );
			break;
		case DH_INCLOCALVAR:

			m_ScriptData.RawData.Read( &lVariable, sizeof( LONG ));
			m_ScriptData.lScriptPos += sizeof( LONG );

			m_ScriptData.alStateVariables[m_ScriptData.lCurrentStateIdx][lVariable]++;
			break;
		case DH_DECLOCALVAR:

			m_ScriptData.RawData.Read( &lVariable, sizeof( LONG ));
			m_ScriptData.lScriptPos += sizeof( LONG );

			m_ScriptData.alStateVariables[m_ScriptData.lCurrentStateIdx][lVariable]--;
			break;
		case DH_ASSIGNLOCALVAR:

			m_ScriptData.RawData.Read( &lVariable, sizeof( LONG ));
			m_ScriptData.lScriptPos += sizeof( LONG );

			m_ScriptData.alStateVariables[m_ScriptData.lCurrentStateIdx][lVariable] = m_ScriptData.alStack[m_ScriptData.lStackPosition - 1];
			PopStack( );
			break;
		case DH_ADDLOCALVAR:

			m_ScriptData.RawData.Read( &lVariable, sizeof( LONG ));
			m_ScriptData.lScriptPos += sizeof( LONG );

			m_ScriptData.alStateVariables[m_ScriptData.lCurrentStateIdx][lVariable] += m_ScriptData.alStack[m_ScriptData.lStackPosition - 1];
			PopStack( );
			break;
		case DH_SUBLOCALVAR:

			m_ScriptData.RawData.Read( &lVariable, sizeof( LONG ));
			m_ScriptData.lScriptPos += sizeof( LONG );

			m_ScriptData.alStateVariables[m_ScriptData.lCurrentStateIdx][lVariable] -= m_ScriptData.alStack[m_ScriptData.lStackPosition - 1];
			PopStack( );
			break;
		case DH_MULLOCALVAR:

			m_ScriptData.RawData.Read( &lVariable, sizeof( LONG ));
			m_ScriptData.lScriptPos += sizeof( LONG );

			m_ScriptData.alStateVariables[m_ScriptData.lCurrentStateIdx][lVariable] *= m_ScriptData.alStack[m_ScriptData.lStackPosition - 1];
			PopStack( );
			break;
		case DH_DIVLOCALVAR:

			m_ScriptData.RawData.Read( &lVariable, sizeof( LONG ));
			m_ScriptData.lScriptPos += sizeof( LONG );

			if ( m_ScriptData.alStack[m_ScriptData.lStackPosition - 1] == 0 )
				I_Error( "ParseScript: Illegal divide by 0 occured while trying to divide local variable in bot %s's script!", m_pPlayer->userinfo.netname );

			m_ScriptData.alStateVariables[m_ScriptData.lCurrentStateIdx][lVariable] /= m_ScriptData.alStack[m_ScriptData.lStackPosition - 1];
			PopStack( );
			break;
		case DH_MODLOCALVAR:

			m_ScriptData.RawData.Read( &lVariable, sizeof( LONG ));
			m_ScriptData.lScriptPos += sizeof( LONG );

			m_ScriptData.alStateVariables[m_ScriptData.lCurrentStateIdx][lVariable] %= m_ScriptData.alStack[m_ScriptData.lStackPosition - 1];
			PopStack( );
			break;
		case DH_CASEGOTO:

			m_ScriptData.RawData.Read( &lVariable, sizeof( LONG ));
			m_ScriptData.lScriptPos += sizeof( LONG );

			m_ScriptData.RawData.Read( &lBuffer, sizeof( LONG ));
			m_ScriptData.lScriptPos += sizeof( LONG );

			if ( m_ScriptData.alStack[m_ScriptData.lStackPosition - 1] == lVariable )
			{
				m_ScriptData.lScriptPos = lBuffer;
				m_ScriptData.RawData.Seek( m_ScriptData.lScriptPos, SEEK_SET );

				PopStack( );
			}
			break;
		case DH_DROP:

			PopStack( );
			break;
		case DH_INCGLOBALARRAY:

			{
				LONG	lIdx;

				lIdx = m_ScriptData.alStack[m_ScriptData.lStackPosition - 1];

				m_ScriptData.RawData.Read( &lVariable, sizeof( LONG ));
				m_ScriptData.lScriptPos += sizeof( LONG );

				m_ScriptData.alScriptArrays[lVariable][lIdx]++;
				PopStack( );
			}
			break;
		case DH_DECGLOBALARRAY:

			{
				LONG	lIdx;

				lIdx = m_ScriptData.alStack[m_ScriptData.lStackPosition - 1];

				m_ScriptData.RawData.Read( &lVariable, sizeof( LONG ));
				m_ScriptData.lScriptPos += sizeof( LONG );

				m_ScriptData.alScriptArrays[lVariable][lIdx]--;
				PopStack( );
			}
			break;
		case DH_ASSIGNGLOBALARRAY:

			m_ScriptData.RawData.Read( &lVariable, sizeof( LONG ));
			m_ScriptData.lScriptPos += sizeof( LONG );

			m_ScriptData.alScriptArrays[lVariable][m_ScriptData.alStack[m_ScriptData.lStackPosition - 2]] = m_ScriptData.alStack[m_ScriptData.lStackPosition - 1];
			PopStack( );
			PopStack( );
			break;
		case DH_ADDGLOBALARRAY:

			m_ScriptData.RawData.Read( &lVariable, sizeof( LONG ));
			m_ScriptData.lScriptPos += sizeof( LONG );

			m_ScriptData.alScriptArrays[lVariable][m_ScriptData.alStack[m_ScriptData.lStackPosition - 2]] += m_ScriptData.alStack[m_ScriptData.lStackPosition - 1];
			PopStack( );
			PopStack( );
			break;
		case DH_SUBGLOBALARRAY:

			m_ScriptData.RawData.Read( &lVariable, sizeof( LONG ));
			m_ScriptData.lScriptPos += sizeof( LONG );

			m_ScriptData.alScriptArrays[lVariable][m_ScriptData.alStack[m_ScriptData.lStackPosition - 2]] -= m_ScriptData.alStack[m_ScriptData.lStackPosition - 1];
			PopStack( );
			PopStack( );
			break;
		case DH_MULGLOBALARRAY:

			m_ScriptData.RawData.Read( &lVariable, sizeof( LONG ));
			m_ScriptData.lScriptPos += sizeof( LONG );

			m_ScriptData.alScriptArrays[lVariable][m_ScriptData.alStack[m_ScriptData.lStackPosition - 2]] *= m_ScriptData.alStack[m_ScriptData.lStackPosition - 1];
			PopStack( );
			PopStack( );
			break;
		case DH_DIVGLOBALARRAY:

			m_ScriptData.RawData.Read( &lVariable, sizeof( LONG ));
			m_ScriptData.lScriptPos += sizeof( LONG );

			if ( m_ScriptData.alStack[m_ScriptData.lStackPosition - 1] == 0 )
				I_Error( "ParseScript: Illegal divide by 0 occured while trying to divide array in bot %s's script!", m_pPlayer->userinfo.netname );

			m_ScriptData.alScriptArrays[lVariable][m_ScriptData.alStack[m_ScriptData.lStackPosition - 2]] /= m_ScriptData.alStack[m_ScriptData.lStackPosition - 1];
			PopStack( );
			PopStack( );
			break;
		case DH_MODGLOBALARRAY:

			m_ScriptData.RawData.Read( &lVariable, sizeof( LONG ));
			m_ScriptData.lScriptPos += sizeof( LONG );

			m_ScriptData.alScriptArrays[lVariable][m_ScriptData.alStack[m_ScriptData.lStackPosition - 2]] %= m_ScriptData.alStack[m_ScriptData.lStackPosition - 1];
			PopStack( );
			PopStack( );
			break;
		case DH_PUSHGLOBALARRAY:

			m_ScriptData.RawData.Read( &lVariable, sizeof( LONG ));
			m_ScriptData.lScriptPos += sizeof( LONG );

			m_ScriptData.alStack[m_ScriptData.lStackPosition - 1] = m_ScriptData.alScriptArrays[lVariable][m_ScriptData.alStack[m_ScriptData.lStackPosition - 1]];
			break;
		case DH_SWAP:

			{
				LONG	lTemp;

				lTemp = m_ScriptData.alStack[m_ScriptData.lStackPosition - 1];
				m_ScriptData.alStack[m_ScriptData.lStackPosition - 1] = m_ScriptData.alStack[m_ScriptData.lStackPosition - 2];
				m_ScriptData.alStack[m_ScriptData.lStackPosition - 2] = lTemp;
			}
			break;
		case DH_DUP:

			m_ScriptData.alStack[m_ScriptData.lStackPosition] = m_ScriptData.alStack[m_ScriptData.lStackPosition - 1];
			m_ScriptData.lStackPosition++;

			if ( botdebug_showstackpushes )
				Printf( "PushToStack: DH_DUP\n" );

			break;
		case DH_ARRAYSET:
			
			{
				LONG	lArray;
				LONG	lVal;
				LONG	lHighestVal;
//				LONG	lIdx;

				lArray = m_ScriptData.alStack[m_ScriptData.lStackPosition - 3];
				lVal = m_ScriptData.alStack[m_ScriptData.lStackPosition - 2];
				lHighestVal = m_ScriptData.alStack[m_ScriptData.lStackPosition - 1];

				if (( lArray < 0 ) || ( lArray >= MAX_SCRIPT_ARRAYS ))
					I_Error( "ParseScript: Invalid array index, %d, in command \"arrayset\"!", lArray );
				if (( lHighestVal < 0 ) || ( lHighestVal >= MAX_SCRIPTARRAY_SIZE ))
					I_Error( "ParseScript: Invalid array maximum index, %d, in command \"arrayset\"!", lHighestVal );

				memset( m_ScriptData.alScriptArrays[lArray], lVal, lHighestVal * sizeof( LONG ));
				PopStack( );
				PopStack( );
				PopStack( );
			}
			break;
		default:

			{
				char	szCommandHeader[32];

				itoa( lCommandHeader, szCommandHeader, 10 );
				I_Error( "ParseScript: Invalid command, %s, in state %s!", lCommandHeader < NUM_DATAHEADERS ? g_pszDataHeaders[lCommandHeader] : szCommandHeader, m_ScriptData.szStateName[m_ScriptData.lCurrentStateIdx] );
			}
			break;
		}
/*
		if (( lExpectedStackPosition != m_ScriptData.lStackPosition ) &&
			( lCommandHeader != DH_COMMAND ) &&
			( lCommandHeader != DH_CASEGOTO ))
		{
			I_Error( "ParseScript: Something's screwey about %s!", g_pszDataHeaders[lCommandHeader] );
		}
*/
	}
}

//*****************************************************************************
//
void CSkullBot::GetStatePositions( void )
{
	bool		bStopParsing;
	LONG		lCommandHeader;
	LONG		lLastCommandHeader;
	LONG		lBuffer;

	bStopParsing = false;
	lCommandHeader = -1;
	while ( bStopParsing == false )
	{
		lLastCommandHeader = lCommandHeader;

		// Hit the end of the file.
		m_ScriptData.RawData.Seek( m_ScriptData.lScriptPos, SEEK_SET );
		if ( m_ScriptData.RawData.Read( &lCommandHeader, sizeof( LONG )) < sizeof( LONG ))
			return;
		m_ScriptData.lScriptPos += sizeof( LONG );

		switch ( lCommandHeader )
		{
		case DH_COMMAND:

			// Read in the command.
			m_ScriptData.RawData.Read( &lBuffer, sizeof( LONG ));
			m_ScriptData.lScriptPos += sizeof( LONG );

			if (( lBuffer < 0 ) || ( lBuffer >= NUM_BOTCMDS ))
				I_Error( "GetStatePositions: Unknown command %d, in state %s!", lBuffer, m_ScriptData.szStateName[m_ScriptData.lCurrentStateIdx] );

			// Read in the argument list.
			m_ScriptData.RawData.Read( &lBuffer, sizeof( LONG ));
			m_ScriptData.lScriptPos += sizeof( LONG );
			break;
		case DH_ONENTER:

			if ( m_ScriptData.lCurrentStateIdx == -1 )
				I_Error( "GetStatePositions: Found onenter without state definition!" );

			m_ScriptData.StatePositions[m_ScriptData.lCurrentStateIdx].lOnEnterPos = m_ScriptData.lScriptPos;
			break;
		case DH_MAINLOOP:

			if ( m_ScriptData.lCurrentStateIdx == -1 )
				I_Error( "GetStatePositions: Found mainloop without state definition!" );

			m_ScriptData.StatePositions[m_ScriptData.lCurrentStateIdx].lMainloopPos = m_ScriptData.lScriptPos;
			break;
		case DH_ONEXIT:

			if ( m_ScriptData.lCurrentStateIdx == -1 )
				I_Error( "GetStatePositions: Found onexit without state definition!" );

			m_ScriptData.StatePositions[m_ScriptData.lCurrentStateIdx].lOnExitPos = m_ScriptData.lScriptPos;
			break;
		case DH_EVENT:

			// Read in the event.
			m_ScriptData.RawData.Read( &lBuffer, sizeof( LONG ));
			m_ScriptData.lScriptPos += sizeof( LONG );

			if ( m_ScriptData.lCurrentStateIdx == -1 )
			{
				if ( m_ScriptData.lNumGlobalEvents == MAX_NUM_GLOBAL_EVENTS )
					I_Error( "GetStatePositions: Too many global events in bot %s's script!", GetPlayer( )->userinfo.netname );

				m_ScriptData.GlobalEventPositions[m_ScriptData.lNumGlobalEvents].lPos = m_ScriptData.lScriptPos;
				m_ScriptData.GlobalEventPositions[m_ScriptData.lNumGlobalEvents].Event = (BOTEVENT_e)lBuffer;
				m_ScriptData.lNumGlobalEvents++;
			}
			else
			{
				if ( m_ScriptData.lNumEvents[m_ScriptData.lCurrentStateIdx] == MAX_NUM_EVENTS )
					I_Error( "GetStatePositions: Too many events in bot %s's state, %s!", GetPlayer( )->userinfo.netname, m_ScriptData.szStateName[m_ScriptData.lCurrentStateIdx] );

				m_ScriptData.EventPositions[m_ScriptData.lCurrentStateIdx][m_ScriptData.lNumEvents[m_ScriptData.lCurrentStateIdx]].lPos = m_ScriptData.lScriptPos;
				m_ScriptData.EventPositions[m_ScriptData.lCurrentStateIdx][m_ScriptData.lNumEvents[m_ScriptData.lCurrentStateIdx]].Event = (BOTEVENT_e)lBuffer;
				m_ScriptData.lNumEvents[m_ScriptData.lCurrentStateIdx]++;
			}
			break;
		case DH_ENDONENTER:

			break;
		case DH_ENDMAINLOOP:

			break;
		case DH_ENDONEXIT:

			break;
		case DH_ENDEVENT:

			break;
		case DH_STATENAME:

			{
				LONG	lNameLength;
				char	szStateName[256];

				// Read in the string length of the script name.
				m_ScriptData.RawData.Read( &lNameLength, sizeof( LONG ));
				m_ScriptData.lScriptPos += sizeof( LONG );

				// Now, read in the string name.
				m_ScriptData.RawData.Read( szStateName, lNameLength );
				m_ScriptData.lScriptPos += lNameLength;

				szStateName[lNameLength] = 0;

				// Read in the string length of the script name.
				m_ScriptData.RawData.Read( &lBuffer, sizeof( LONG ));
				m_ScriptData.lScriptPos += sizeof( LONG );

				if ( lBuffer != DH_STATEIDX )
					I_Error( "GetStatePositions: Expected state index after state, %s!", szStateName );

				m_ScriptData.RawData.Read( &m_ScriptData.lCurrentStateIdx, sizeof( LONG ));
				m_ScriptData.lScriptPos += sizeof( LONG );

				m_ScriptData.StatePositions[m_ScriptData.lCurrentStateIdx].lPos = m_ScriptData.lScriptPos;
				sprintf( m_ScriptData.szStateName[m_ScriptData.lCurrentStateIdx], szStateName );
			}

			// All done.
			break;
		case DH_STATEIDX:

			{
				m_ScriptData.RawData.Read( &m_ScriptData.lCurrentStateIdx, sizeof( LONG ));
				m_ScriptData.lScriptPos += sizeof( LONG );

				m_ScriptData.StatePositions[m_ScriptData.lCurrentStateIdx].lPos = m_ScriptData.lScriptPos;
			}

			break;
		case DH_PUSHNUMBER:

			m_ScriptData.RawData.Read( &lBuffer, sizeof( LONG ));
			m_ScriptData.lScriptPos += sizeof( LONG );
			break;
		case DH_PUSHSTRINGINDEX:

			m_ScriptData.RawData.Read( &lBuffer, sizeof( LONG ));
			m_ScriptData.lScriptPos += sizeof( LONG );
			break;
		case DH_PUSHGLOBALVAR:

			m_ScriptData.RawData.Read( &lBuffer, sizeof( LONG ));
			m_ScriptData.lScriptPos += sizeof( LONG );
			break;
		case DH_PUSHLOCALVAR:

			m_ScriptData.RawData.Read( &lBuffer, sizeof( LONG ));
			m_ScriptData.lScriptPos += sizeof( LONG );
			break;
		case DH_IFGOTO:

			m_ScriptData.RawData.Read( &lBuffer, sizeof( LONG ));
			m_ScriptData.lScriptPos += sizeof( LONG );
			break;
		case DH_IFNOTGOTO:

			m_ScriptData.RawData.Read( &lBuffer, sizeof( LONG ));
			m_ScriptData.lScriptPos += sizeof( LONG );
			break;
		case DH_GOTO:

			m_ScriptData.RawData.Read( &lBuffer, sizeof( LONG ));
			m_ScriptData.lScriptPos += sizeof( LONG );
			break;
		case DH_ORLOGICAL:
		case DH_ANDLOGICAL:
		case DH_ORBITWISE:
		case DH_EORBITWISE:
		case DH_ANDBITWISE:
		case DH_EQUALS:
		case DH_NOTEQUALS:
		case DH_DROPSTACKPOSITION:
		case DH_LESSTHAN:
		case DH_LESSTHANEQUALS:
		case DH_GREATERTHAN:
		case DH_GREATERTHANEQUALS:
		case DH_NEGATELOGICAL:
		case DH_LSHIFT:
		case DH_RSHIFT:
		case DH_ADD:
		case DH_SUBTRACT:
		case DH_UNARYMINUS:
		case DH_MULTIPLY:
		case DH_DIVIDE:
		case DH_MODULUS:

			break;
		case DH_SCRIPTVARLIST:

			m_ScriptData.RawData.Read( &lBuffer, sizeof( LONG ));
			m_ScriptData.lScriptPos += sizeof( LONG );
			break;
		case DH_STRINGLIST:

			{
				char	szString[MAX_STRING_LENGTH];
				LONG	lNumStrings;
				ULONG	ulIdx;

				m_ScriptData.RawData.Read( &lNumStrings, sizeof( LONG ));
				m_ScriptData.lScriptPos += sizeof( LONG );

				for ( ulIdx = 0; ulIdx < (ULONG)lNumStrings; ulIdx++ )
				{
					// This is the string length.
					m_ScriptData.RawData.Read( &lBuffer, sizeof( LONG ));
					m_ScriptData.lScriptPos += sizeof( LONG );

					m_ScriptData.RawData.Read( &szString, lBuffer );
					m_ScriptData.lScriptPos += lBuffer;

					szString[lBuffer] = 0;
					sprintf( m_ScriptData.szStringList[ulIdx], szString );
				}
			}
			break;
		case DH_INCGLOBALVAR:

			m_ScriptData.RawData.Read( &lBuffer, sizeof( LONG ));
			m_ScriptData.lScriptPos += sizeof( LONG );
			break;
		case DH_DECGLOBALVAR:

			m_ScriptData.RawData.Read( &lBuffer, sizeof( LONG ));
			m_ScriptData.lScriptPos += sizeof( LONG );
			break;
		case DH_ASSIGNGLOBALVAR:

			m_ScriptData.RawData.Read( &lBuffer, sizeof( LONG ));
			m_ScriptData.lScriptPos += sizeof( LONG );
			break;
		case DH_ADDGLOBALVAR:

			m_ScriptData.RawData.Read( &lBuffer, sizeof( LONG ));
			m_ScriptData.lScriptPos += sizeof( LONG );
			break;
		case DH_SUBGLOBALVAR:

			m_ScriptData.RawData.Read( &lBuffer, sizeof( LONG ));
			m_ScriptData.lScriptPos += sizeof( LONG );
			break;
		case DH_MULGLOBALVAR:

			m_ScriptData.RawData.Read( &lBuffer, sizeof( LONG ));
			m_ScriptData.lScriptPos += sizeof( LONG );
			break;
		case DH_DIVGLOBALVAR:

			m_ScriptData.RawData.Read( &lBuffer, sizeof( LONG ));
			m_ScriptData.lScriptPos += sizeof( LONG );
			break;
		case DH_MODGLOBALVAR:

			m_ScriptData.RawData.Read( &lBuffer, sizeof( LONG ));
			m_ScriptData.lScriptPos += sizeof( LONG );
			break;
		case DH_INCLOCALVAR:

			m_ScriptData.RawData.Read( &lBuffer, sizeof( LONG ));
			m_ScriptData.lScriptPos += sizeof( LONG );
			break;
		case DH_DECLOCALVAR:

			m_ScriptData.RawData.Read( &lBuffer, sizeof( LONG ));
			m_ScriptData.lScriptPos += sizeof( LONG );
			break;
		case DH_ASSIGNLOCALVAR:

			m_ScriptData.RawData.Read( &lBuffer, sizeof( LONG ));
			m_ScriptData.lScriptPos += sizeof( LONG );
			break;
		case DH_ADDLOCALVAR:

			m_ScriptData.RawData.Read( &lBuffer, sizeof( LONG ));
			m_ScriptData.lScriptPos += sizeof( LONG );
			break;
		case DH_SUBLOCALVAR:

			m_ScriptData.RawData.Read( &lBuffer, sizeof( LONG ));
			m_ScriptData.lScriptPos += sizeof( LONG );
			break;
		case DH_MULLOCALVAR:

			m_ScriptData.RawData.Read( &lBuffer, sizeof( LONG ));
			m_ScriptData.lScriptPos += sizeof( LONG );
			break;
		case DH_DIVLOCALVAR:

			m_ScriptData.RawData.Read( &lBuffer, sizeof( LONG ));
			m_ScriptData.lScriptPos += sizeof( LONG );
			break;
		case DH_MODLOCALVAR:

			m_ScriptData.RawData.Read( &lBuffer, sizeof( LONG ));
			m_ScriptData.lScriptPos += sizeof( LONG );
			break;
		case DH_CASEGOTO:

			m_ScriptData.RawData.Read( &lBuffer, sizeof( LONG ));
			m_ScriptData.lScriptPos += sizeof( LONG );

			m_ScriptData.RawData.Read( &lBuffer, sizeof( LONG ));
			m_ScriptData.lScriptPos += sizeof( LONG );
			break;
		case DH_DROP:

			break;
		case DH_INCGLOBALARRAY:

			m_ScriptData.RawData.Read( &lBuffer, sizeof( LONG ));
			m_ScriptData.lScriptPos += sizeof( LONG );
			break;
		case DH_DECGLOBALARRAY:

			m_ScriptData.RawData.Read( &lBuffer, sizeof( LONG ));
			m_ScriptData.lScriptPos += sizeof( LONG );
			break;
		case DH_ASSIGNGLOBALARRAY:

			m_ScriptData.RawData.Read( &lBuffer, sizeof( LONG ));
			m_ScriptData.lScriptPos += sizeof( LONG );
			break;
		case DH_ADDGLOBALARRAY:

			m_ScriptData.RawData.Read( &lBuffer, sizeof( LONG ));
			m_ScriptData.lScriptPos += sizeof( LONG );
			break;
		case DH_SUBGLOBALARRAY:

			m_ScriptData.RawData.Read( &lBuffer, sizeof( LONG ));
			m_ScriptData.lScriptPos += sizeof( LONG );
			break;
		case DH_MULGLOBALARRAY:

			m_ScriptData.RawData.Read( &lBuffer, sizeof( LONG ));
			m_ScriptData.lScriptPos += sizeof( LONG );
			break;
		case DH_DIVGLOBALARRAY:

			m_ScriptData.RawData.Read( &lBuffer, sizeof( LONG ));
			m_ScriptData.lScriptPos += sizeof( LONG );
			break;
		case DH_MODGLOBALARRAY:

			m_ScriptData.RawData.Read( &lBuffer, sizeof( LONG ));
			m_ScriptData.lScriptPos += sizeof( LONG );
			break;
		case DH_PUSHGLOBALARRAY:

			m_ScriptData.RawData.Read( &lBuffer, sizeof( LONG ));
			m_ScriptData.lScriptPos += sizeof( LONG );
			break;
		case DH_SWAP:

			break;
		case DH_ARRAYSET:

			break;
		default:

			I_Error( "GetStatePositions: Unknown header, %d in bot %s's script at position, %d! (Last known header: %d)", lCommandHeader, m_pPlayer->userinfo.netname, m_ScriptData.lScriptPos - sizeof( LONG ), lLastCommandHeader );
			break;
		}
	}
}

//*****************************************************************************
//
ULONG CSkullBot::GetScriptDelay( void )
{
	return ( m_ScriptData.ulScriptDelay );
}

//*****************************************************************************
//
void CSkullBot::SetScriptDelay( ULONG ulDelay )
{
	if ( ulDelay > 0 )
		m_ScriptData.ulScriptDelay = ulDelay;
}

//*****************************************************************************
//
ULONG CSkullBot::GetScriptEventDelay( void )
{
	return ( m_ScriptData.ulScriptEventDelay );
}

//*****************************************************************************
//
void CSkullBot::SetScriptEventDelay( ULONG ulDelay )
{
	if ( ulDelay > 0 )
		m_ScriptData.ulScriptEventDelay = ulDelay;
}

//*****************************************************************************
//
player_s *CSkullBot::GetPlayer( void )
{
	return ( m_pPlayer );
}

//*****************************************************************************
//
FWadLump *CSkullBot::GetRawScriptData( void )
{
	return ( &m_ScriptData.RawData );
}

//*****************************************************************************
//
LONG CSkullBot::GetScriptPosition( void )
{
	return ( m_ScriptData.lScriptPos );
}

//*****************************************************************************
//
void CSkullBot::SetScriptPosition( LONG lPosition )
{
	m_ScriptData.lScriptPos = lPosition;
}

//*****************************************************************************
//
void CSkullBot::IncrementScriptPosition( LONG lUnits )
{
	m_ScriptData.lScriptPos += lUnits;
}

//*****************************************************************************
//
void CSkullBot::SetExitingState( bool bExit )
{
	m_ScriptData.bExitingState = bExit;
}

//*****************************************************************************
//
void CSkullBot::SetNextState( LONG lNextState )
{
	m_ScriptData.lNextState = lNextState;
}

//*****************************************************************************
//
void CSkullBot::PushToStack( LONG lValue )
{
	m_ScriptData.alStack[m_ScriptData.lStackPosition++] = lValue;
	
	if ( botdebug_showstackpushes )
		Printf( "%s: PushToStack: %d\n", m_pPlayer->userinfo.netname, m_ScriptData.lStackPosition );

	if ( m_ScriptData.lStackPosition >= BOTSCRIPT_STACK_SIZE )
		I_Error( "PushToStack: Stack size exceeded in bot %s's script!", m_pPlayer->userinfo.netname );
}

//*****************************************************************************
//
void CSkullBot::PopStack( void )
{
	m_ScriptData.lStackPosition--;
	
	if ( botdebug_showstackpushes )
		Printf( "%s: PopStack: %d\n", m_pPlayer->userinfo.netname, m_ScriptData.lStackPosition );

	if ( m_ScriptData.lStackPosition < 0 )
		I_Error( "PopStack: Bot stack position went below 0 in bot %s's script!", m_pPlayer->userinfo.netname );
}

//*****************************************************************************
//
void CSkullBot::PushToStringStack( char *pszString )
{
	sprintf( m_ScriptData.aszStringStack[m_ScriptData.lStringStackPosition++], pszString );
	
	if ( botdebug_showstackpushes )
		Printf( "PushToStringStack: %d\n", m_ScriptData.lStringStackPosition );

	if ( m_ScriptData.lStringStackPosition >= BOTSCRIPT_STACK_SIZE )
		I_Error( "PushToStringStack: Stack size exceeded!" );
}

//*****************************************************************************
//
void CSkullBot::PopStringStack( void )
{
	m_ScriptData.lStringStackPosition--;
	
	if ( botdebug_showstackpushes )
		Printf( "PopStringStack: %d\n", m_ScriptData.lStackPosition );

	if ( m_ScriptData.lStringStackPosition < 0 )
		I_Error( "PopStringStack: Bot stack position went below 0!" );
}

//*****************************************************************************
//
void CSkullBot::PreDelete( void )
{
	// If this player is the displayplayer, revert the camera back to the console player's eyes.
	if (( m_pPlayer->mo->CheckLocalView( consoleplayer )) && ( NETWORK_GetState( ) != NETSTATE_SERVER ))
	{
		players[consoleplayer].camera = players[consoleplayer].mo;
		S_UpdateSounds( players[consoleplayer].camera );
		StatusBar->AttachToPlayer( &players[consoleplayer] );
	}

	// Remove the bot from the game.
	playeringame[( m_pPlayer - players )] = false;

	// Delete the actor attached to the player.
	if ( m_pPlayer->mo )
		m_pPlayer->mo->Destroy( );

	// Finally, fix some pointers.
	m_pPlayer->pSkullBot = NULL;
	m_pPlayer->mo = NULL;
	m_pPlayer = NULL;
}

//*****************************************************************************
//
void CSkullBot::HandleAiming( void )
{
	if (( m_bAimAtEnemy ) && ( m_ulPlayerEnemy != MAXPLAYERS ) && ( players[m_ulPlayerEnemy].mo ))
	{
		fixed_t	Distance;
		fixed_t	ShootZ;
		LONG	lTopPitch;
		LONG	lBottomPitch;
		POS_t	EnemyPos;

		// Get the current enemy position.
		EnemyPos = this->GetEnemyPosition( );
//		Printf( "Position delta:\nX: %d\nY: %d\nZ: %d\n",
//			( EnemyPos.x - players[m_ulPlayerEnemy].mo->x ) / FRACUNIT, 
//			( EnemyPos.y - players[m_ulPlayerEnemy].mo->y ) / FRACUNIT, 
//			( EnemyPos.z - players[m_ulPlayerEnemy].mo->z ) / FRACUNIT );

		m_pPlayer->mo->angle = R_PointToAngle2( m_pPlayer->mo->x,
											m_pPlayer->mo->y, 
											EnemyPos.x,
											EnemyPos.y );

		m_pPlayer->mo->angle += m_AngleDesired;
		m_AngleOffBy -= m_AngleDelta;
		if ( m_bTurnLeft )
			m_pPlayer->mo->angle -= m_AngleOffBy;
		else
			m_pPlayer->mo->angle += m_AngleOffBy;

		ShootZ = m_pPlayer->mo->z - m_pPlayer->mo->floorclip + ( m_pPlayer->mo->height >> 1 ) + ( 8 * FRACUNIT );
		Distance = P_AproxDistance( m_pPlayer->mo->x - EnemyPos.x, m_pPlayer->mo->y - EnemyPos.y );
//		m_pPlayer->mo->pitch = R_PointToAngle( Distance, ( EnemyPos.z + ( players[m_ulPlayerEnemy].mo->height / 2 )) - m_pPlayer->mo->z );
		lTopPitch = -(LONG)R_PointToAngle2( 0, ShootZ, Distance, EnemyPos.z + players[m_ulPlayerEnemy].mo->height );
		lBottomPitch = -(LONG)R_PointToAngle2( 0, ShootZ, Distance, EnemyPos.z );

		m_pPlayer->mo->pitch = ( lTopPitch / 2 ) + ( lBottomPitch / 2 );
/*
		{
			angle_t	Angle;
			angle_t	AngleDifference;

			Angle = R_PointToAngle2( m_pPlayer->mo->x,
												m_pPlayer->mo->y, 
												EnemyPos.x,
												EnemyPos.y );

			if ( Angle > m_pPlayer->mo->angle )
				AngleDifference = Angle - m_pPlayer->mo->angle;
			else
				AngleDifference = m_pPlayer->mo->angle - Angle;

			Printf( "Now offby: " );
			// AngleDistance is the absolute value of the difference in player's angle, and
			// the angle pointint to his enemy. WE DO NOT KNOW IF THE DIFFERENCE IS TO THE
			// LEFT OR RIGHT!
			if ( AngleDifference > ANGLE_180 )
			{
				Printf( "-" );
				AngleDifference = ( ANGLE_MAX - AngleDifference ) + 1;	// Add 1 because ANGLE_MAX is 1 short of 360
			}

			Printf( "%2.2f\n", (float)AngleDifference / ANGLE_1 );
			Printf( "Player's angle: %2.2f\n", (float)m_pPlayer->mo->angle / ANGLE_1 );
		}
*/
//		if (( m_ulPlayerEnemy != MAXPLAYERS ) && players[m_ulPlayerEnemy].mo )
		{
			// Still waiting to reaim at the enemy.
			if ( m_ulAimAtEnemyDelay == 0 )
			{
				angle_t		Angle;
				angle_t		AngleDifference;
				angle_t		AngleDesired;
				angle_t		AngleRange;
				angle_t		AngleRandom;
				angle_t		AngleFinal;

				// Get the exact angle between us and the enemy.
				Angle = R_PointToAngle2( m_pPlayer->mo->x,
										  m_pPlayer->mo->y, 
										  EnemyPos.x,
										  EnemyPos.y );

				// The greater the difference between the angle between ourselves and the enemy, and our
				// current angle, the more inaccurate it is likely to be.
				if ( Angle > m_pPlayer->mo->angle )
					AngleDifference = Angle - m_pPlayer->mo->angle;
				else
					AngleDifference = m_pPlayer->mo->angle - Angle;

				// AngleDistance is the absolute value of the difference in player's angle, and
				// the angle pointint to his enemy. WE DO NOT KNOW IF THE DIFFERENCE IS TO THE
				// LEFT OR RIGHT!
				if ( AngleDifference > ANGLE_180 )
					AngleDifference = ( ANGLE_MAX - AngleDifference ) + 1;	// Add 1 because ANGLE_MAX is 1 short of 360
/*
				Printf( "Angle to enemy: %2.2f\n", (float)( (float)Angle / ANGLE_1 ));
				Printf( "Current angle: %2.2f\n", (float)( (float)m_pPlayer->mo->angle / ANGLE_1 ));
				Printf( "Initially off by (absolute value): %2.2f\n", (float)( (float)AngleDifference / ANGLE_1 ));
*/
				// Select some random angle between 0 and 360 degrees. 
				AngleRandom = g_RandomBotAimSeed.Random2( 360 );
				while ( (LONG)AngleRandom < 0 )
					AngleRandom += 360;
				AngleRandom *= ANGLE_1;

				// Select the range of possible angles and how long it's going to be before
				// reaiming, based on the bot's accuracy skill level.
				switch ( BOTS_AdjustSkill( this, BOTINFO_GetAccuracy( m_ulBotInfoIdx )))
				{
				case BOTSKILL_VERYPOOR:

					AngleRange = ANGLE_1 * 90;
					m_ulAimAtEnemyDelay = 17;
					break;
				case BOTSKILL_POOR:

					AngleRange = ANGLE_1 * 75;
					m_ulAimAtEnemyDelay = 12;
					break;
				case BOTSKILL_LOW:

					AngleRange = ANGLE_1 * 60;
					m_ulAimAtEnemyDelay = 8;
					break;
				case BOTSKILL_MEDIUM:

					AngleRange = ANGLE_1 * 45;
					m_ulAimAtEnemyDelay = 6;
					break;
				case BOTSKILL_HIGH:

					AngleRange = ANGLE_1 * 35;
					m_ulAimAtEnemyDelay = 4;
					break;
				case BOTSKILL_EXCELLENT:

					AngleRange = ANGLE_1 * 25;
					m_ulAimAtEnemyDelay = 3;
					break;
				case BOTSKILL_SUPREME:

					AngleRange = ANGLE_1 * 15;
					m_ulAimAtEnemyDelay = 2;
					break;
				case BOTSKILL_GODLIKE:

					AngleRange = ANGLE_1 * 10;
					m_ulAimAtEnemyDelay = 1;
					break;
				case BOTSKILL_PERFECT:

					// Accuracy is perfect.
					m_pPlayer->mo->angle = Angle;
					m_ulAimAtEnemyDelay = 0;

					// Just return: nothing else to do.
					return;
				default:

					I_Error( "botcmd_BeginAimingAtEnemy: Unknown botskill level, %d!", BOTINFO_GetAccuracy( m_ulBotInfoIdx ));
					break;
				}

				// Now, get the desired angle based on the random angle and the range of angles.
				// If the difference in angles is more than the angle range, select an angle
				// within that range instead.
				if (( AngleDifference/* / 2*/ ) > AngleRange )
				{
//					Printf( "Using AngleDifference as maximum angle range\n" );

					// Disallow % 0.
					if ( AngleDifference == 0 )
						AngleDifference++;

					AngleDesired = ( AngleRandom % ( AngleDifference/* / 2*/ ));
				}
				else
				{
					AngleDesired = ( AngleRandom % AngleRange );
				}

//				Printf( "Desired angle offset: %2.2f\n", (float)( (float)AngleDesired / ANGLE_1 ));

				if ( g_RandomBotAimSeed( ) % 2 )
				{
//					Printf( "Randomly picked angle to the left...\n" );

					AngleFinal = Angle + AngleDesired;
					m_AngleDesired = AngleDesired;
				}
				// Otherwise, turn counter-clockwise.
				else
				{
//					Printf( "Randomly picked angle to the right...\n" );

					AngleFinal = Angle - AngleDesired;
					m_AngleDesired = ANGLE_MAX - AngleDesired;
				}

//				Printf( "AngleFinal: %2.2f\n", (float)AngleFinal / ANGLE_1 );
//				Printf( "Player's angle: %2.2f\n", (float)m_pPlayer->mo->angle / ANGLE_1 );

				// Now AngleDifference is going to be the differnce between our current angle
				// and the final angle!
				if ( AngleFinal > m_pPlayer->mo->angle )
				{
//					Printf( "We'll need to turn LEFT\n" );
					m_bTurnLeft = true;
					AngleDifference = AngleFinal - m_pPlayer->mo->angle;
				}
				else
				{
//					Printf( "We'll need to turn RIGHT\n" );
					m_bTurnLeft = false;
					AngleDifference = m_pPlayer->mo->angle - AngleFinal;
				}

//				Printf( "AngleDifference: %2.2f\n", (float)AngleDifference / ANGLE_1 );
				if ( AngleDifference > ANGLE_180 )
				{
					m_bTurnLeft = !m_bTurnLeft;
					AngleDifference = ( ANGLE_MAX - AngleDifference ) + 1;	// Add 1 because ANGLE_MAX is 1 short of 360
//					Printf( "Angle too large! New value: %2.2f (changing direction)\n", (float)Angle / ANGLE_1 );
				}

				// Our delta angle is the angles we turn each tick, so that at the end of our
				// turning, we've reached our desired angle.
				m_AngleOffBy = AngleDifference;
				m_AngleDelta = ( AngleDifference / ( m_ulAimAtEnemyDelay + 1 ));
//				Printf( "AngleDelta (%d / %d): %2.2f\n", AngleDifference / ANGLE_1, m_ulAimAtEnemyDelay + 1, (float)( (float)m_AngleDelta / ANGLE_1 ));
//				Printf( "All done!\n" );
			}
			else
			{
				m_ulAimAtEnemyDelay--;
			}
		}
	}
	else
		m_pPlayer->mo->pitch = 0;
}

//*****************************************************************************
//
POS_t CSkullBot::GetEnemyPosition( void )
{
	LONG		lTicks;
	POS_t		Pos1;
	POS_t		Pos2;
	POS_t		PosFinal;
	BOTSKILL_e	Skill;

	Skill = BOTS_AdjustSkill( this, BOTINFO_GetPerception( m_ulBotInfoIdx ));
	switch ( Skill )
	{
	case BOTSKILL_VERYPOOR:

		lTicks = 25;
		Pos1 = m_EnemyPosition[( gametic - lTicks ) % MAX_REACTION_TIME];
		Pos2 = m_EnemyPosition[( gametic - ( lTicks + 1 )) % MAX_REACTION_TIME];
		break;
	case BOTSKILL_POOR:

		lTicks = 19;
		Pos1 = m_EnemyPosition[( gametic - lTicks ) % MAX_REACTION_TIME];
		Pos2 = m_EnemyPosition[( gametic - ( lTicks + 1 )) % MAX_REACTION_TIME];
		break;
	case BOTSKILL_LOW:

		lTicks = 14;
		Pos1 = m_EnemyPosition[( gametic - lTicks ) % MAX_REACTION_TIME];
		Pos2 = m_EnemyPosition[( gametic - ( lTicks + 1 )) % MAX_REACTION_TIME];
		break;
	case BOTSKILL_MEDIUM:

		lTicks = 10;
		Pos1 = m_EnemyPosition[( gametic - lTicks ) % MAX_REACTION_TIME];
		Pos2 = m_EnemyPosition[( gametic - ( lTicks + 1 )) % MAX_REACTION_TIME];
		break;
	case BOTSKILL_HIGH:

		lTicks = 7;
		Pos1 = m_EnemyPosition[( gametic - lTicks ) % MAX_REACTION_TIME];
		Pos2 = m_EnemyPosition[( gametic - ( lTicks + 1 )) % MAX_REACTION_TIME];
		break;
	case BOTSKILL_EXCELLENT:

		lTicks = 5;
		Pos1 = m_EnemyPosition[( gametic - lTicks ) % MAX_REACTION_TIME];
		Pos2 = m_EnemyPosition[( gametic - ( lTicks + 1 )) % MAX_REACTION_TIME];
		break;
	case BOTSKILL_SUPREME:

		lTicks = 3;
		Pos1 = m_EnemyPosition[( gametic - lTicks ) % MAX_REACTION_TIME];
		Pos2 = m_EnemyPosition[( gametic - ( lTicks + 1 )) % MAX_REACTION_TIME];
		break;
	case BOTSKILL_GODLIKE:

		lTicks = 1;
		Pos1 = m_EnemyPosition[( gametic - lTicks ) % MAX_REACTION_TIME];
		Pos2 = m_EnemyPosition[( gametic - ( lTicks + 1 )) % MAX_REACTION_TIME];
		break;
	case BOTSKILL_PERFECT:

		return ( m_EnemyPosition[gametic % MAX_REACTION_TIME] );
	default:

		{
			POS_t	ZeroPos;
		
			I_Error( "GetEnemyPosition: Unknown bot skill level, %d!", (LONG)Skill );

			// To shut the compiler up...
			ZeroPos.x = ZeroPos.y = ZeroPos.z = 0;
			return ( ZeroPos );
		}
		break;
	}

	PosFinal.x = Pos1.x + (( Pos1.x - Pos2.x ) * lTicks );
	PosFinal.y = Pos1.y + (( Pos1.y - Pos2.y ) * lTicks );
	PosFinal.z = Pos1.z + (( Pos1.z - Pos2.z ) * lTicks );
	return ( PosFinal );
}

//*****************************************************************************
//
void CSkullBot::SetEnemyPosition( fixed_t X, fixed_t Y, fixed_t Z )
{
	m_EnemyPosition[gametic % MAX_REACTION_TIME].x = X;
	m_EnemyPosition[gametic % MAX_REACTION_TIME].y = Y;
	m_EnemyPosition[gametic % MAX_REACTION_TIME].z = Z;

	m_ulLastEnemyPositionTick = gametic;
	if ( m_ulPathType == BOTPATHTYPE_ENEMYPOSITION )
		m_ulPathType = BOTPATHTYPE_NONE;
}

//*****************************************************************************
//
POS_t CSkullBot::GetLastEnemyPosition( void )
{
	return ( m_EnemyPosition[m_ulLastEnemyPositionTick % MAX_REACTION_TIME] );
}

//*****************************************************************************
//
void CSkullBot::AddEventToQueue( BOTEVENT_e Event, LONG lTick )
{
	m_StoredEventQueue[m_lQueueTail].Event = Event;
	m_StoredEventQueue[m_lQueueTail++].lTick = lTick;
	m_lQueueTail = m_lQueueTail % MAX_STORED_EVENTS;

	if ( m_lQueueTail == m_lQueueHead )
		I_Error( "AddEventToQueue: Event queue size exceeded!" );
}

//*****************************************************************************
//
void CSkullBot::DeleteEventFromQueue( void )
{
	ULONG		ulIdx;
	BOTEVENT_e	Event;

	Event = m_StoredEventQueue[m_lQueueHead++].Event;
	m_lQueueHead = m_lQueueHead % MAX_STORED_EVENTS;

	if ( botdebug_showevents )
		Printf( "%s: %s\n", GetPlayer( )->userinfo.netname, g_pszEventNames[Event] );

	// First, scan global events.
	for ( ulIdx = 0; ulIdx < m_ScriptData.lNumGlobalEvents; ulIdx++ )
	{
		// Found a matching event.
		if ( m_ScriptData.GlobalEventPositions[ulIdx].Event == Event )
		{
			if ( m_ScriptData.bInEvent == false )
			{
				// Save the current position in the script, since we will go back to it if we do not change
				// states.
				m_ScriptData.lSavedScriptPos = m_ScriptData.lScriptPos;

				m_ScriptData.bInEvent = true;
			}

			m_ScriptData.lScriptPos = m_ScriptData.GlobalEventPositions[ulIdx].lPos;
			m_ScriptData.RawData.Seek( m_ScriptData.lScriptPos, SEEK_SET );

			// Parse after the event.
			m_ScriptData.ulScriptEventDelay = 0;
			ParseScript( );
			return;
		}
	}

	// If the event wasn't found in the global events, scan state events.
	for ( ulIdx = 0; ulIdx < MAX_NUM_EVENTS; ulIdx++ )
	{
		// Found a matching event.
		if ( m_ScriptData.EventPositions[m_ScriptData.lCurrentStateIdx][ulIdx].Event == Event )
		{
			if ( m_ScriptData.bInEvent == false )
			{
				// Save the current position in the script, since we will go back to it if we do not change
				// states.
				m_ScriptData.lSavedScriptPos = m_ScriptData.lScriptPos;

				m_ScriptData.bInEvent = true;
			}

			m_ScriptData.lScriptPos = m_ScriptData.EventPositions[m_ScriptData.lCurrentStateIdx][ulIdx].lPos;
			m_ScriptData.RawData.Seek( m_ScriptData.lScriptPos, SEEK_SET );

			// Parse after the event.
			m_ScriptData.ulScriptEventDelay = 0;
			ParseScript( );
			return;
		}
	}
}

//*****************************************************************************
//	CONSOLE COMMANDS

// Add a secret bot to the skirmish menu
CCMD( reveal )
{
	char	szBuffer[256];
	ULONG	ulIdx;

	if ( argv.argc( ) < 2 )
	{
		Printf( PRINT_HIGH, "reveal [botname]: reveals a secret bot\n" );
		return;
	}

	for ( ulIdx = 0; ulIdx < MAX_BOTINFO; ulIdx++ )
	{
		if ( g_BotInfo[ulIdx] == NULL )
			continue;

		if ( g_BotInfo[ulIdx]->bRevealed )
			continue;

		sprintf( szBuffer, g_BotInfo[ulIdx]->szName );
		V_ColorizeString( szBuffer );
		V_RemoveColorCodes( szBuffer );

		if ( stricmp( argv[1], szBuffer ) == 0 )
		{
			Printf( "Hidden bot \"%s\\c-\" has now been revealed!\n", g_BotInfo[ulIdx]->szName );
			g_BotInfo[ulIdx]->bRevealed = true;
		}
	}

	for ( ulIdx = 0; ulIdx < (ULONG)numskins; ulIdx++ )
	{
		if ( skins[ulIdx].bRevealed )
			continue;

		sprintf( szBuffer, skins[ulIdx].name );
		V_ColorizeString( szBuffer );
		V_RemoveColorCodes( szBuffer );

		if ( stricmp( argv[1], skins[ulIdx].name ) == 0 )
		{
			Printf( "Hidden skin \"%s\\c-\" has now been revealed!\n", skins[ulIdx].name );
			skins[ulIdx].bRevealed = true;
		}
	}
}

//*****************************************************************************
//
CCMD( addbot )
{
	CSkullBot	*pBot;
	ULONG		ulPlayerIdx;

	if ( gamestate != GS_LEVEL )
		return;

	// Don't allow adding of bots in campaign mode.
	if (( CAMPAIGN_InCampaign( )) && ( sv_cheats == false ))
	{
		Printf( "Bots cannot be added manually during a campaign!\n" );
		return;
	}

	// Don't allow bots in network mode, unless we're the host.
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
	{
		Printf( "Only the host can add bots!\n" );
		return;
	}

	if ( ASTAR_IsInitialized( ) == false )
	{
		if ( sv_disallowbots )
			Printf( "The bot pathing nodes have not been set up. Please set \"sv_disallowbots\" to \"false\" if you wish to use bots.\n" );
		else if ( level.flags & LEVEL_NOBOTNODES )
			Printf( "The bot pathing nodes have not been set up. This level has disabled the ability to do so.\n" );
		else
			Printf( "The bot pathing nodes have not been set up. Please reload the level if you wish to use bots.\n" );

		// Don't allow the bot to spawn.
		return;
	}

	// Break if we have the wrong amount of arguments.
	if ( argv.argc( ) > 3 )
	{
		Printf( "addbot [botname] [red/blue]: add a bot to the game\n" );
		return;
	}

	ulPlayerIdx = BOTS_FindFreePlayerSlot( );
	if ( ulPlayerIdx == MAXPLAYERS )
	{
		Printf( "The maximum number of players/bots has been reached.\n" );
		return;
	}

	switch ( argv.argc( ) )
	{
	case 1:

		// Allocate a new bot.
		pBot = new CSkullBot( NULL, NULL, BOTS_FindFreePlayerSlot( ));
		break;
	case 2:

		// Verify that the bot's name matches one of the existing bots.
		if ( BOTS_IsValidName( argv[1] ))
			pBot = new CSkullBot( argv[1], NULL, BOTS_FindFreePlayerSlot( ));
		else
		{
			Printf( "Invalid bot name: %s\n", argv[1] );
			return;
		}
		break;
	case 3:

		// Verify that the bot's name matches one of the existing bots.
		if ( BOTS_IsValidName( argv[1] ))
			pBot = new CSkullBot( argv[1], argv[2], BOTS_FindFreePlayerSlot( ));
		else
		{
			Printf( "Invalid bot name: %s\n", argv[1] );
			return;
		}
		break;
	}
}

//*****************************************************************************
//
CCMD( removebot )
{
	ULONG	ulIdx;
	char	szName[64];

	// Don't allow removing of bots in campaign mode.
	if (( CAMPAIGN_InCampaign( )) && ( sv_cheats == false ))
	{
		Printf( "Bots cannot be removed manually during a campaign!\n" );
		return;
	}

	// If we didn't input which bot to remove, remove a random one.
	if ( argv.argc( ) < 2 )
	{
		ULONG	ulRandom;
		bool	bBotInGame = false;

		// First, verify that there's a bot in the game.
		for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
		{
			if (( playeringame[ulIdx] ) && ( players[ulIdx].pSkullBot ))
			{
				bBotInGame = true;
				break;
			}
		}

		// If there isn't, break.
		if ( bBotInGame == false )
		{
			Printf( "No bots found.\n" );
			return;
		}

		// Now randomly select a bot to remove.
		do
		{
			ulRandom = ( BotRemove( ) % MAXPLAYERS );
		} while (( playeringame[ulRandom] == false ) || ( players[ulRandom].pSkullBot == NULL ));

		// Now that we've found a valid bot, remove it.
		BOTS_RemoveBot( ulRandom, true );
	}
	else
	{
		for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
		{
			if (( playeringame[ulIdx] == false ) || ( players[ulIdx].pSkullBot == NULL ))
				continue;

			sprintf( szName, players[ulIdx].userinfo.netname );
			V_RemoveColorCodes( szName );
			if ( stricmp( szName, argv[1] ) == 0 )
			{
				// Now that we've found a valid bot, remove it.
				BOTS_RemoveBot( ulIdx, true );

				// Nothing more to do.
				return;
			}
		}
	}
}

//*****************************************************************************
//
CCMD( removebots )
{
	// Don't allow removing of bots in campaign mode.
	if (( CAMPAIGN_InCampaign( )) && ( sv_cheats == false ))
	{
		Printf( "Bots cannot be removed manually during a campaign!\n" );
		return;
	}

	// Remove all the bots from the level. If less than 3 seconds have passed,
	// the scripts are probably clearing out all the bots from the level, so don't
	// display exit messages.
	if ( level.time > ( 3 * TICRATE ))
		BOTS_RemoveAllBots( true );
	else
		BOTS_RemoveAllBots( false );
}

//*****************************************************************************
//
CCMD( listbots )
{
	ULONG	ulIdx;
	ULONG	ulNumBots = 0;

	Printf( "Active bots:\n\n" );

	// Loop through all the players and count up the bots.
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( playeringame[ulIdx] && players[ulIdx].pSkullBot )
		{
			Printf( "%s\n", players[ulIdx].userinfo.netname );
			ulNumBots++;
		}
	}

	Printf( "\n%d bot%s.\n", ulNumBots, ulNumBots == 1 ? "" : "s" );
}

//*****************************************************************************
ADD_STAT( bots )
{
	FString	Out;

	Out.Format( "Bot cycles = %04.1f ms",
		(double)g_BotCycles * SecondsPerCycle * 1000
		);

	return ( Out );
}

