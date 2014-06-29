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
// DESCRIPTION:
//	Internally used data structures for virtually everything,
//	 key definitions, lots of other stuff.
//
//-----------------------------------------------------------------------------

#ifndef __DOOMDEF_H__
#define __DOOMDEF_H__

#include <stdio.h>
#include <string.h>

//
// Global parameters/defines.
//

// Game mode handling - identify IWAD version
//	to handle IWAD dependend animations etc.
typedef enum
{
	shareware,		// DOOM 1 shareware, E1, M9
	registered,		// DOOM 1 registered, E3, M27
	commercial,		// DOOM 2 retail, E1 M34
	// DOOM 2 german edition not handled
	retail,			// DOOM 1 retail, E4, M36
	undetermined	// Well, no IWAD found.
  
} GameMode_t;


// If rangecheck is undefined, most parameter validation debugging code
// will not be compiled
#ifndef NORANGECHECKING
#ifndef RANGECHECK
#define RANGECHECK
#endif
#endif

// The maximum number of players, multiplayer/networking.
// [BB] Changed to 64.
#define MAXPLAYERS		64

// State updates, number of tics / second.
#define TICRATE 		35

// [Spleen] The amount of ticks of old positions to store for unlagged support
#define UNLAGGEDTICS	35

// [BB] The amount of tics stored for the client player prediction.
#define CLIENT_PREDICTION_TICS					70

// The current state of the game: whether we are
// playing, gazing at the intermission screen,
// the game final animation, or a demo. 
typedef enum
{
	GS_LEVEL,
	GS_INTERMISSION,
	GS_FINALE,
	GS_DEMOSCREEN,
	GS_FULLCONSOLE,		// [RH]	Fullscreen console
	GS_HIDECONSOLE,		// [RH] The menu just did something that should hide fs console
	GS_STARTUP,			// [RH] Console is fullscreen, and game is just starting
	GS_TITLELEVEL,		// [RH] A combination of GS_LEVEL and GS_DEMOSCREEN

	GS_FORCEWIPE = -1,
	GS_FORCEWIPEFADE = -2
} gamestate_t;

extern	gamestate_t 	gamestate;

// wipegamestate can be set to -1
//	to force a wipe on the next draw
extern gamestate_t wipegamestate;


typedef float skill_t;

/*
enum ESkillLevels
{
	sk_baby,
	sk_easy,
	sk_medium,
	sk_hard,
	sk_nightmare
};
*/



#define TELEFOGHEIGHT			(gameinfo.telefogheight)

//
// DOOM keyboard definition. Everything below 0x100 matches
// a DirectInput key code.
//
#define KEY_RIGHTARROW			0xcd	// DIK_RIGHT
#define KEY_LEFTARROW			0xcb	// DIK_LEFT
#define KEY_UPARROW 			0xc8	// DIK_UP
#define KEY_DOWNARROW			0xd0	// DIK_DOWN
#define KEY_ESCAPE				0x01	// DIK_ESCAPE
#define KEY_ENTER				0x1c	// DIK_RETURN
#define KEY_SPACE				0x39	// DIK_SPACE
#define KEY_TAB 				0x0f	// DIK_TAB
#define KEY_F1					0x3b	// DIK_F1
#define KEY_F2					0x3c	// DIK_F2
#define KEY_F3					0x3d	// DIK_F3
#define KEY_F4					0x3e	// DIK_F4
#define KEY_F5					0x3f	// DIK_F5
#define KEY_F6					0x40	// DIK_F6
#define KEY_F7					0x41	// DIK_F7
#define KEY_F8					0x42	// DIK_F8
#define KEY_F9					0x43	// DIK_F9
#define KEY_F10 				0x44	// DIK_F10
#define KEY_F11 				0x57	// DIK_F11
#define KEY_F12 				0x58	// DIK_F12

#define KEY_BACKSPACE			0x0e	// DIK_BACK
#define KEY_PAUSE				0xff

#define KEY_EQUALS				0x0d	// DIK_EQUALS
#define KEY_MINUS				0x0c	// DIK_MINUS

#define KEY_LSHIFT				0x2A	// DIK_LSHIFT
#define KEY_LCTRL				0x1d	// DIK_LCONTROL
#define KEY_LALT				0x38	// DIK_LMENU

#define	KEY_RSHIFT				KEY_LSHIFT
#define KEY_RCTRL				KEY_LCTRL
#define KEY_RALT				KEY_LALT

#define KEY_INS 				0xd2	// DIK_INSERT
#define KEY_DEL 				0xd3	// DIK_DELETE
#define KEY_END 				0xcf	// DIK_END
#define KEY_HOME				0xc7	// DIK_HOME
#define KEY_PGUP				0xc9	// DIK_PRIOR
#define KEY_PGDN				0xd1	// DIK_NEXT

#define KEY_MOUSE1				0x100
#define KEY_MOUSE2				0x101
#define KEY_MOUSE3				0x102
#define KEY_MOUSE4				0x103
#define KEY_MOUSE5				0x104
#define KEY_MOUSE6				0x105
#define KEY_MOUSE7				0x106
#define KEY_MOUSE8				0x107

#define KEY_FIRSTJOYBUTTON		0x108
#define KEY_LASTJOYBUTTON		0x187
#define KEY_JOYPOV1_UP			0x188
#define KEY_JOYPOV1_RIGHT		0x189
#define KEY_JOYPOV1_DOWN		0x18a
#define KEY_JOYPOV1_LEFT		0x18b
#define KEY_JOYPOV2_UP			0x18c
#define KEY_JOYPOV3_UP			0x190
#define KEY_JOYPOV4_UP			0x194

#define KEY_MWHEELUP			0x198
#define KEY_MWHEELDOWN			0x199

#define NUM_KEYS				0x19A

#define JOYAXIS_NONE			0
#define JOYAXIS_YAW				1
#define JOYAXIS_PITCH			2
#define JOYAXIS_FORWARD			3
#define JOYAXIS_SIDE			4
#define JOYAXIS_UP				5
//#define JOYAXIS_ROLL			6		// Ha ha. No roll for you.

// [RH] dmflags bits (based on Q2's)
// [RC] NOTE: If adding a flag, be sure to add a stub in serverconsole_dmflags.cpp.
enum
{
	DF_NO_HEALTH			= 1 << 0,	// Do not spawn health items (DM)
	DF_NO_ITEMS				= 1 << 1,	// Do not spawn powerups (DM) - [RC] Currently not implemented (no easy way to find if it's an object, like AArtifact).
	DF_WEAPONS_STAY			= 1 << 2,	// Leave weapons around after pickup (DM)
	DF_FORCE_FALLINGZD		= 1 << 3,	// Falling too far hurts (old ZDoom style)
	DF_FORCE_FALLINGHX		= 2 << 3,	// Falling too far hurts (Hexen style)
	DF_FORCE_FALLINGST		= 3 << 3,	// Falling too far hurts (Strife style)
//							  1 << 5	-- this space left blank --
	DF_SAME_LEVEL			= 1 << 6,	// Stay on the same map when someone exits (DM)
	DF_SPAWN_FARTHEST		= 1 << 7,	// Spawn players as far as possible from other players (DM)
	DF_FORCE_RESPAWN		= 1 << 8,	// Automatically respawn dead players after respawn_time is up (DM)
	DF_NO_ARMOR				= 1 << 9,	// Do not spawn armor (DM)
	DF_NO_EXIT				= 1 << 10,	// Kill anyone who tries to exit the level (DM)
	DF_INFINITE_AMMO		= 1 << 11,	// Don't use up ammo when firing
	DF_NO_MONSTERS			= 1 << 12,	// Don't spawn monsters (replaces -nomonsters parm)
	DF_MONSTERS_RESPAWN		= 1 << 13,	// Monsters respawn sometime after their death (replaces -respawn parm)
	DF_ITEMS_RESPAWN		= 1 << 14,	// Items other than invuln. and invis. respawn
	DF_FAST_MONSTERS		= 1 << 15,	// Monsters are fast (replaces -fast parm)
	DF_NO_JUMP				= 1 << 16,	// Don't allow jumping
	// [BB] I don't want to change the dmflag numbers compared to 97D.
	DF_YES_JUMP				= 1 << 29,
	DF_NO_FREELOOK			= 1 << 17,	// Don't allow freelook
	DF_RESPAWN_SUPER		= 1 << 18,	// Respawn invulnerability and invisibility
	DF_NO_FOV				= 1 << 19,	// Only let the arbitrator set FOV (for all players)
	DF_NO_COOP_WEAPON_SPAWN	= 1 << 20,	// Don't spawn multiplayer weapons in coop games
	DF_NO_CROUCH			= 1 << 21,	// Don't allow crouching
	// [BB] I don't want to change the dmflag numbers compared to 97D.
	DF_YES_CROUCH			= 1 << 30,	//
	DF_COOP_LOSE_INVENTORY	= 1 << 22,	// Lose all your old inventory when respawning in coop
	DF_COOP_LOSE_KEYS		= 1 << 23,	// Lose keys when respawning in coop
	DF_COOP_LOSE_WEAPONS	= 1 << 24,	// Lose weapons when respawning in coop
	DF_COOP_LOSE_ARMOR		= 1 << 25,	// Lose armor when respawning in coop
	DF_COOP_LOSE_POWERUPS	= 1 << 26,	// Lose powerups when respawning in coop
	DF_COOP_LOSE_AMMO		= 1 << 27,	// Lose ammo when respawning in coop
	DF_COOP_HALVE_AMMO		= 1 << 28,	// Lose half your ammo when respawning in coop (but not less than the normal starting amount)
};

// [BC] More dmflags. w00p!
// [RC] NOTE: If adding a flag, be sure to add a stub in serverconsole_dmflags.cpp.
enum
{
//	DF2_YES_IMPALING		= 1 << 0,	// Player gets implaed on MF2_IMPALE items
	DF2_YES_WEAPONDROP		= 1 << 1,	// Drop current weapon upon death
	DF2_NO_RUNES			= 1 << 2,	// Don't spawn runes
	DF2_INSTANT_RETURN		= 1 << 3,	// Instantly return flags and skulls when player carrying it dies (ST/CTF)
	DF2_NO_TEAM_SWITCH		= 1 << 4,	// Do not allow players to switch teams in teamgames
	DF2_NO_TEAM_SELECT		= 1 << 5,	// Player is automatically placed on a team.
	DF2_YES_DOUBLEAMMO		= 1 << 6,	// Double amount of ammo that items give you like skill 1 and 5 do
	DF2_YES_DEGENERATION	= 1 << 7,	// Player slowly loses health when over 100% (Quake-style)
	DF2_YES_FREEAIMBFG		= 1 << 8,	// Allow BFG freeaiming in multiplayer games.
	DF2_BARRELS_RESPAWN		= 1 << 9,	// Barrels respawn (duh)
	DF2_NO_RESPAWN_INVUL	= 1 << 10,	// No respawn invulnerability.
	DF2_COOP_SHOTGUNSTART	= 1 << 11,	// All playres start with a shotgun when they respawn
	DF2_SAME_SPAWN_SPOT		= 1 << 12,	// Players respawn in the same place they died (co-op)

	// [BB] I don't want to change the dmflag numbers compared to 97D.
	DF2_YES_KEEP_TEAMS		= 1 << 13,	// Player keeps his team after a map change.

	DF2_YES_KEEPFRAGS		= 1 << 14,	// Don't clear frags after each level
	DF2_NO_RESPAWN			= 1 << 15,	// Player cannot respawn
	DF2_YES_LOSEFRAG		= 1 << 16,	// Lose a frag when killed. More incentive to try to not get yerself killed
	DF2_INFINITE_INVENTORY	= 1 << 17,	// Infinite inventory.
	DF2_KILL_MONSTERS		= 1 << 22,	// All monsters must be killed before the level exits.
	DF2_NO_AUTOMAP			= 1 << 23,	// Players are allowed to see the automap.
	DF2_NO_AUTOMAP_ALLIES	= 1 << 24,	// Allies can been seen on the automap.
	DF2_DISALLOW_SPYING		= 1 << 25,	// You can spy on your allies.
	DF2_CHASECAM			= 1 << 26,	// Players can use the chasecam cheat.
	DF2_NOSUICIDE			= 1 << 27,	// Players are allowed to suicide.
	DF2_NOAUTOAIM			= 1 << 28,	// Players cannot use autoaim.

	// [BB] Enforces some Gl rendering options to their default values.
	DF2_FORCE_GL_DEFAULTS		= 1 << 18,

	// [BB] P_RadiusAttack doesn't give players any z-momentum if the attack was made by a player. This essentially disables rocket jumping.
	DF2_NO_ROCKET_JUMPING		= 1 << 19,

	// [BB] Award actual damage dealt instead of kills.
	DF2_AWARD_DAMAGE_INSTEAD_KILLS		= 1 << 20,

	// [BB] Enforces clients to display alpha, i.e. render as if r_drawtrans == 1.
	DF2_FORCE_ALPHA		= 1 << 21,

	// [BB] Spawn map actors in coop as if the game was single player.
	DF2_COOP_SP_ACTOR_SPAWN		= 1 << 29,
};

// [BB] Even more dmflags...
enum
{
	// [BB] Enforces clients not to identify players, i.e. behave as if cl_identifytarget == 0.
	DF3_NO_IDENTIFY_TARGET		= 1 << 0,

	// [BB] Apply lmsspectatorsettings in all game modes.
	DF3_ALWAYS_APPLY_LMS_SPECTATORSETTINGS		= 1 << 1,

	// [BB] Enforces clients not to draw coop info, i.e. behave as if cl_drawcoopinfo == 0.
	DF3_NO_COOP_INFO		= 1 << 2,

	// [Spleen] Don't use ping-based backwards reconciliation for player-fired hitscans and rails.
	DF3_NOUNLAGGED			= 1 << 3,

	// [BB] Handle player bodies as if they had MF6_THRUSPECIES.
	DF3_UNBLOCK_PLAYERS			= 1 << 4,

	// [BB] Enforces clients not to show medals, i.e. behave as if cl_medals == 0.
	DF3_NO_MEDALS			= 1 << 5,

	// [Dusk] Share keys between all players
	DF3_SHARE_KEYS			= 1 << 6,
};

// [RH] Compatibility flags.
// [RC] NOTE: If adding a flag, be sure to add a stub in serverconsole_dmflags.cpp.
enum
{
	COMPATF_SHORTTEX		= 1 << 0,	// Use Doom's shortest texture around behavior?
	COMPATF_STAIRINDEX		= 1 << 1,	// Don't fix loop index for stair building?
	COMPATF_LIMITPAIN		= 1 << 2,	// Pain elemental is limited to 20 lost souls?
	COMPATF_SILENTPICKUP	= 1 << 3,	// Pickups are only heard locally?
	COMPATF_NO_PASSMOBJ		= 1 << 4,	// Pretend every actor is infinitely tall?
	COMPATF_MAGICSILENCE	= 1 << 5,	// Limit actors to one sound at a time?
	COMPATF_WALLRUN			= 1 << 6,	// Enable buggier wall clipping so players can wallrun?
	COMPATF_NOTOSSDROPS		= 1 << 7,	// Spawn dropped items directly on the floor?
	COMPATF_USEBLOCKING		= 1 << 8,	// Any special line can block a use line
	COMPATF_NODOORLIGHT		= 1 << 9,	// Don't do the BOOM local door light effect
	COMPATF_RAVENSCROLL		= 1 << 10,	// Raven's scrollers use their original carrying speed
	COMPATF_SOUNDTARGET		= 1 << 11,	// Use sector based sound target code.
	COMPATF_DEHHEALTH		= 1 << 12,	// Limit deh.MaxHealth to the health bonus (as in Doom2.exe)
	COMPATF_TRACE			= 1 << 13,	// Trace ignores lines with the same sector on both sides
	COMPATF_DROPOFF			= 1 << 14,	// Monsters cannot move when hanging over a dropoff
	COMPATF_BOOMSCROLL		= 1 << 15,	// Scrolling sectors are additive like in Boom
	COMPATF_INVISIBILITY	= 1 << 16,	// Monsters can see semi-invisible players
	// [BB] Changed from 1 << 17 to 1<<27.
	COMPATF_SILENT_INSTANT_FLOORS = 1<<27,	// Instantly moving floors are not silent
	// [BB] Changed from 1 << 18 to 1<<28.
	COMPATF_SECTORSOUNDS	= 1 << 28,	// Sector sounds use original method for sound origin.
	// [BB] Changed from 1 << 19 to 1<<29.
	COMPATF_MISSILECLIP		= 1 << 29,	// Use original Doom heights for clipping against projectiles
	// [BB] Changed from 1 << 20 to 1<<30.
	COMPATF_CROSSDROPOFF	= 1 << 30,	// monsters can't be pushed over dropoffs

	// [BC] Start of new compatflags.

	// Limited movement in the air.
	COMPATF_LIMITED_AIRMOVEMENT	= 1 << 17,

	// Allow the map01 "plasma bump" bug.
	COMPATF_PLASMA_BUMP_BUG	= 1 << 18,

	// Allow instant respawn after death.
	COMPATF_INSTANTRESPAWN	= 1 << 19,

	// Taunting is disabled.
	COMPATF_DISABLETAUNTS	= 1 << 20,

	// Use doom2.exe's original sound curve.
	COMPATF_ORIGINALSOUNDCURVE	= 1 << 21,

	// Use doom2.exe's original intermission screens/music.
	COMPATF_OLDINTERMISSION		= 1 << 22,

	// Disable stealth monsters, since doom2.exe didn't have them.
	COMPATF_DISABLESTEALTHMONSTERS		= 1 << 23,

	// [BB] Always use the old radius damage code (infinite height)
	COMPATF_OLDRADIUSDMG		= 1 << 24,

	// Disable cooperative backpacks.
	// [BB] We are running out of numbers, 1 << 24 is now used for COMPATF_OLDRADIUSDMG
//	COMPATF_DISABLECOOPERATIVEBACKPACKS	= 1 << 24,

	// [BB] Clients are not allowed to use a crosshair.
	COMPATF_NO_CROSSHAIR		= 1 << 25,

	// [BB] Clients use the vanilla Doom weapon on pickup behavior.
	COMPATF_OLD_WEAPON_SWITCH		= 1 << 26,
};

// [BB] More compatibility flags.
enum
{
	// [BB] Treat ACS scripts with the SCRIPTF_Net flag to be client side, i.e.
	// executed on the clients, but not on the server.
	COMPATF2_NETSCRIPTS_ARE_CLIENTSIDE		= 1 << 0,
	// [BB] Clients send ucmd.buttons as "long" instead of as "byte" in CLIENTCOMMANDS_ClientMove.
	// So far this is only necessary if the ACS function GetPlayerInput is used in a server side
	// script to check for buttons bigger than BT_ZOOM. Otherwise this information is completely
	// useless for the server and the additional net traffic to send it should be avoided.
	COMPATF2_CLIENTS_SEND_FULL_BUTTON_INFO		= 1 << 1,
	// [BB] Players are not allowed to use the land CCMD. Because of Skulltag's default amount
	// of air control, flying players can get a huge speed boast with the land CCMD. Disallowing
	// players to land, allows to keep the default air control most people are used to while not
	// giving flying players too much of an advantage.
	COMPATF2_NO_LAND						= 1 << 2,
	// [BB] Use Doom's random table instead of ZDoom's random number generator.
	COMPATF2_OLD_RANDOM_GENERATOR		= 1 << 3,
	// [BB] Add NOGRAVITY to actors named InvulnerabilitySphere, Soulsphere, Megasphere and BlurSphere
	// when spawned by the map.
	COMPATF2_NOGRAVITY_SPHERES		= 1 << 4,
	// [BB] When a player leaves the game, don't stop any scripts of that player that are still running.
	COMPATF2_DONT_STOP_PLAYER_SCRIPTS_ON_DISCONNECT		= 1 << 5,
	// [BB] Use the horizontal thrust of old ZDoom versions in P_RadiusAttack.
	COMPATF2_OLD_EXPLOSION_THRUST		= 1 << 6,
	// [BB] Use the P_TestMobjZ approach of old ZDoom versions where non-SOLID things (like flags) fall
	// through invisible bridges.
	COMPATF2_OLD_BRIDGE_DROPS		= 1 << 7,
	// [CK] Uses old ZDoom jump physics, it's a minor bug in the gravity code that causes gravity application in the wrong place
	COMPATF2_ZDOOM_123B33_JUMP_PHYSICS = 1 << 8,
	// [CK] You can't change weapons mid raise/lower in vanilla
	COMPATF2_FULL_WEAPON_LOWER = 1 << 9,
	// [Dusk] Clients run server-side scripts called from client-side scripts on their own
	COMPATF2_CLIENT_ACS_EXECUTE = 1 << 10,
};

// Emulate old bugs for select maps. These are not exposed by a cvar
// or mapinfo because we do not want new maps to use these bugs.
enum
{
	BCOMPATF_SETSLOPEOVERFLOW	= 1 << 0,	// SetSlope things can overflow
	BCOMPATF_RESETPLAYERSPEED	= 1 << 1,	// Set player speed to 1.0 when changing maps
	BCOMPATF_SPECHITOVERFLOW	= 1 << 2,	// Emulate spechit overflow (e.g. Strain MAP07)
};

// phares 3/20/98:
//
// Player friction is variable, based on controlling
// linedefs. More friction can create mud, sludge,
// magnetized floors, etc. Less friction can create ice.

#define MORE_FRICTION_MOMENTUM	15000	// mud factor based on momentum
#define ORIG_FRICTION			0xE800	// original value
#define ORIG_FRICTION_FACTOR	2048	// original value
#define FRICTION_LOW			0xf900
#define FRICTION_FLY			0xeb00


#define BLINKTHRESHOLD (4*32)

#ifndef __BIG_ENDIAN__
#define MAKE_ID(a,b,c,d)	((DWORD)((a)|((b)<<8)|((c)<<16)|((d)<<24)))
#else
#define MAKE_ID(a,b,c,d)	((DWORD)((d)|((c)<<8)|((b)<<16)|((a)<<24)))
#endif

#endif	// __DOOMDEF_H__
