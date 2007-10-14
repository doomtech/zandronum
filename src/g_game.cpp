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
// DESCRIPTION:  none
//
//-----------------------------------------------------------------------------



#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <time.h>

#include "templates.h"
#include "version.h"
#include "m_alloc.h"
#include "doomdef.h" 
#include "doomstat.h"
#include "d_protocol.h"
#include "d_netinf.h"
#include "f_finale.h"
#include "m_argv.h"
#include "m_misc.h"
#include "m_menu.h"
#include "m_random.h"
#include "m_crc32.h"
#include "i_system.h"
#include "p_setup.h"
#include "p_saveg.h"
#include "p_effect.h"
#include "p_tick.h"
#include "d_main.h"
#include "wi_stuff.h"
#include "hu_stuff.h"
#include "st_stuff.h"
#include "am_map.h"
#include "c_console.h"
#include "c_cvars.h"
#include "c_bind.h"
#include "c_dispatch.h"
#include "v_video.h"
#include "w_wad.h"
#include "p_local.h" 
#include "s_sound.h"
#include "gstrings.h"
#include "r_data.h"
#include "r_main.h"
#include "r_sky.h"
#include "r_draw.h"
#include "g_game.h"
#include "g_level.h"
#include "sbar.h"
#include "m_swap.h"
#include "m_png.h"
#include "gi.h"
#include "a_keys.h"
#include "a_artifacts.h"
#include "chat.h"
#include "deathmatch.h"
#include "duel.h"
#include "team.h"
#include "a_doomglobal.h"
#include "sv_commands.h"
#include "medal.h"
#include "cl_demo.h"
#include "cl_main.h"
#include "cl_statistics.h"
#include "browser.h"
#include "lastmanstanding.h"
#include "campaign.h"
#include "callvote.h"
#include "cooperative.h"
#include "invasion.h"
#include "scoreboard.h"
#include "survival.h"
#include "announcer.h"
#include "p_acs.h"
#include "cl_commands.h"
#include "possession.h"
#include "statnums.h"

#include "gl/gl_lights.h"

#include <zlib.h>

#include "g_hub.h"

static FRandom pr_dmspawn ("DMSpawn");

const int SAVEPICWIDTH = 216;
const int SAVEPICHEIGHT = 162;

bool	G_CheckDemoStatus (void);
void	G_ReadDemoTiccmd (ticcmd_t *cmd, int player);
void	G_WriteDemoTiccmd (ticcmd_t *cmd, int player, int buf);
void	G_PlayerReborn (int player);

void	G_DoReborn (int playernum, bool freshbot);

void	G_DoNewGame (void);
void	G_DoLoadGame (void);
void	G_DoPlayDemo (void);
void	G_DoCompleted (void);
void	G_DoVictory (void);
void	G_DoWorldDone (void);
void	G_DoSaveGame (bool okForQuicksave);
void	G_DoAutoSave ();

FIntCVar gameskill ("skill", 2, CVAR_SERVERINFO|CVAR_LATCH);
CVAR (Bool, chasedemo, false, 0);
CVAR (Bool, storesavepic, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

gameaction_t	gameaction;
gamestate_t 	gamestate = GS_STARTUP;
int 			respawnmonsters;

int 			paused;
bool 			sendpause;				// send a pause event next tic 
bool			sendsave;				// send a save event next tic 
bool			sendturn180;			// [RH] send a 180 degree turn next tic
bool 			usergame;				// ok to save / end game
bool			insave;					// Game is saving - used to block exit commands

bool			timingdemo; 			// if true, exit with report on completion 
bool 			nodrawers;				// for comparative timing purposes 
bool 			noblit; 				// for comparative timing purposes 

bool	 		viewactive;

player_t		players[MAXPLAYERS];
bool			playeringame[MAXPLAYERS];
DWORD			playerswiping;

int 			consoleplayer;			// player taking events
int 			gametic;

CVAR(Bool, demo_compress, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG);
char			demoname[256];
bool 			demorecording;
bool 			demoplayback;
bool 			netdemo;
bool			demonew;				// [RH] Only used around G_InitNew for demos
int				demover;
BYTE*			demobuffer;
BYTE*			demo_p;
BYTE*			democompspot;
BYTE*			demobodyspot;
size_t			maxdemosize;
BYTE*			zdemformend;			// end of FORM ZDEM chunk
BYTE*			zdembodyend;			// end of ZDEM BODY chunk
bool 			singledemo; 			// quit after playing a demo from cmdline 
 
bool 			precache = true;		// if true, load all graphics at start 
 
wbstartstruct_t wminfo; 				// parms for world map / intermission 
 
short			consistancy[MAXPLAYERS][BACKUPTICS];
 
BYTE*			savebuffer;
 
 
#define MAXPLMOVE				(forwardmove[1]) 
 
#define TURBOTHRESHOLD	12800

float	 		normforwardmove[2] = {0x19, 0x32};		// [RH] For setting turbo from console
float	 		normsidemove[2] = {0x18, 0x28};			// [RH] Ditto

fixed_t			forwardmove[2], sidemove[2];
fixed_t 		angleturn[4] = {640, 1280, 320, 320};		// + slow turn
fixed_t			flyspeed[2] = {1*256, 3*256};
int				lookspeed[2] = {450, 512};

#define SLOWTURNTICS	6 

CVAR (Bool,		cl_run,			false,	CVAR_GLOBALCONFIG|CVAR_ARCHIVE)		// Always run?
CVAR (Bool,		invertmouse,	false,	CVAR_GLOBALCONFIG|CVAR_ARCHIVE)		// Invert mouse look down/up?
CVAR (Bool,		freelook,		false,	CVAR_GLOBALCONFIG|CVAR_ARCHIVE)		// Always mlook?
CVAR (Bool,		lookstrafe,		false,	CVAR_GLOBALCONFIG|CVAR_ARCHIVE)		// Always strafe with mouse?
CVAR (Float,	m_pitch,		1.f,	CVAR_GLOBALCONFIG|CVAR_ARCHIVE)		// Mouse speeds
CVAR (Float,	m_yaw,			1.f,	CVAR_GLOBALCONFIG|CVAR_ARCHIVE)
CVAR (Float,	m_forward,		1.f,	CVAR_GLOBALCONFIG|CVAR_ARCHIVE)
CVAR (Float,	m_side,			2.f,	CVAR_GLOBALCONFIG|CVAR_ARCHIVE)
 
int 			turnheld;								// for accelerative turning 
 
// mouse values are used once 
int 			mousex;
int 			mousey; 		

FString			savegamefile;
char			savedescription[SAVESTRINGSIZE];

// [RH] Name of screenshot file to generate (usually NULL)
FString			shotfile;

AActor* 		bodyque[BODYQUESIZE]; 
int 			bodyqueslot; 

void R_ExecuteSetViewSize (void);

FString savename;
FString BackupSaveName;

bool SendLand;
const AInventory *SendItemUse, *SendItemDrop;

// [BC] How many ticks of the end level delay remain?
static	ULONG	g_ulEndLevelDelay = 0;

// [BC] How many ticks until we announce various messages?
static	ULONG	g_ulLevelIntroTicks = 0;

EXTERN_CVAR (Int, team)

// [RH] Allow turbo setting anytime during game
CUSTOM_CVAR (Float, turbo, 100.f, 0)
{
	if (self < 10.f)
	{
		self = 10.f;
	}
	else if (self > 256.f)
	{
		self = 256.f;
	}
	else
	{
		float scale = self * 0.01f;

		forwardmove[0] = (int)(normforwardmove[0]*scale);
		forwardmove[1] = (int)(normforwardmove[1]*scale);
		sidemove[0] = (int)(normsidemove[0]*scale);
		sidemove[1] = (int)(normsidemove[1]*scale);
	}
}

CCMD (turnspeeds)
{
	if (argv.argc() == 1)
	{
		Printf ("Current turn speeds: %d %d %d %d\n", angleturn[0],
			angleturn[1], angleturn[2], angleturn[3]);
	}
	else
	{
		int i;

		for (i = 1; i <= 4 && i < argv.argc(); ++i)
		{
			angleturn[i-1] = atoi (argv[i]);
		}
		if (i <= 2)
		{
			angleturn[1] = angleturn[0] * 2;
		}
		if (i <= 3)
		{
			angleturn[2] = angleturn[0] / 2;
		}
		if (i <= 4)
		{
			angleturn[3] = angleturn[2];
		}
	}
}

CCMD (slot)
{
	if (argv.argc() > 1)
	{
		int slot = atoi (argv[1]);

		if (slot < NUM_WEAPON_SLOTS)
		{
			SendItemUse = LocalWeapons.Slots[slot].PickWeapon (&players[consoleplayer]);

			// [BC] This can be NULL if we're a spectator.
			if ( SendItemUse == NULL )
				return;

			// [BC] If we're the client, switch to this weapon right now, since the whole
			// DEM_, etc. network code isn't ever executed.
			if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) &&
				( CLIENT_GetConnectionState( ) == CTS_ACTIVE ))
			{
				players[consoleplayer].mo->UseInventory( (AInventory *)SendItemUse );
			}
		}
	}
}

CCMD (centerview)
{
	// [BC] Only write this byte if we're recording a demo. Otherwise, just do it!
	if ( demorecording )
		Net_WriteByte (DEM_CENTERVIEW);
	else
	{
		if ( players[consoleplayer].mo )
			players[consoleplayer].mo->pitch = 0;

		if ( CLIENTDEMO_IsRecording( ))
			CLIENTDEMO_WriteLocalCommand( CLD_CENTERVIEW, NULL );
	}
}

CCMD(crouch)
{
	Net_WriteByte(DEM_CROUCH);
}

CCMD (land)
{
	SendLand = true;
}

CCMD (pause)
{
	sendpause = true;
}

CCMD (turn180)
{
	sendturn180 = true;
}

// [BC] New cvar that shows the name of the weapon we're cycling to.
CVAR( Bool, cl_showweapnameoncycle, true, CVAR_ARCHIVE )

CCMD (weapnext)
{
	SendItemUse = PickNextWeapon (&players[consoleplayer]);

	// [BC] This can be NULL if we're a spectator.
	if ( SendItemUse == NULL )
		return;

	// [BC] If we're the client, switch to this weapon right now, since the whole
	// DEM_, etc. network code isn't ever executed.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) &&
		( CLIENT_GetConnectionState( ) == CTS_ACTIVE ))
	{
		players[consoleplayer].mo->UseInventory( (AInventory *)SendItemUse );
	}

	// [BC] Option to display the name of the weapon being cycled to.
	if ( cl_showweapnameoncycle )
	{
		char				szString[64];
		DHUDMessageFadeOut	*pMsg;
		
		// Build the string and text color;
		sprintf( szString, "%s", SendItemUse->GetClass( )->TypeName.GetChars( ));
		// [RC] Set the font
		screen->SetFont( SmallFont );
		pMsg = new DHUDMessageFadeOut( szString,
			1.5f,
			gameinfo.gametype == GAME_Doom ? 0.96f : 0.95f,
			0,
			0,
			CR_GOLD,
			2.f,
			0.35f );

		StatusBar->AttachMessage( pMsg, MAKE_ID( 'P', 'N', 'A', 'M' ));
	}
}

CCMD (weapprev)
{
	SendItemUse = PickPrevWeapon (&players[consoleplayer]);

	// [BC] This can be NULL if we're a spectator.
	if ( SendItemUse == NULL )
		return;

	// [BC] If we're the client, switch to this weapon right now, since the whole
	// DEM_, etc. network code isn't ever executed.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) &&
		( CLIENT_GetConnectionState( ) == CTS_ACTIVE ))
	{
		players[consoleplayer].mo->UseInventory( (AInventory *)SendItemUse );
	}

	// [BC] Option to display the name of the weapon being cycled to.
	if ( cl_showweapnameoncycle )
	{
		char				szString[64];
		DHUDMessageFadeOut	*pMsg;
		
		// Build the string and text color;
		sprintf( szString, "%s", SendItemUse->GetClass( )->TypeName.GetChars( ));
		// [RC] Set the font
		screen->SetFont( SmallFont );

		pMsg = new DHUDMessageFadeOut( szString,
			1.5f,
			gameinfo.gametype == GAME_Doom ? 0.96f : 0.95f,
			0,
			0,
			CR_GOLD,
			2.f,
			0.35f );

		StatusBar->AttachMessage( pMsg, MAKE_ID( 'P', 'N', 'A', 'M' ));
	}
}

CCMD (invnext)
{
	AInventory *next;

	if (who == NULL)
		return;

	if (who->InvSel != NULL)
	{
		if ((next = who->InvSel->NextInv()) != NULL)
		{
			who->InvSel = next;
		}
		else
		{
			// Select the first item in the inventory
			if (!(who->Inventory->ItemFlags & IF_INVBAR))
			{
				who->InvSel = who->Inventory->NextInv();
			}
			else
			{
				who->InvSel = who->Inventory;
			}
		}
	}
	who->player->inventorytics = 5*TICRATE;
}

CCMD (invprev)
{
	AInventory *item, *newitem;

	if (who == NULL)
		return;

	if (who->InvSel != NULL)
	{
		if ((item = who->InvSel->PrevInv()) != NULL)
		{
			who->InvSel = item;
		}
		else
		{
			// Select the last item in the inventory
			item = who->InvSel;
			while ((newitem = item->NextInv()) != NULL)
			{
				item = newitem;
			}
			who->InvSel = item;
		}
	}
	who->player->inventorytics = 5*TICRATE;
}

CCMD (invuseall)
{
	// [BB] If we are a client, we have to bypass the way ZDoom handles the item usage.
	if( NETWORK_GetState( ) == NETSTATE_CLIENT )
		CLIENTCOMMANDS_RequestInventoryUseAll();
	else
		SendItemUse = (const AInventory *)1;
}

CCMD (invuse)
{
	// [BB] If we are a client, we have to bypass the way ZDoom handles the item usage.
	if( NETWORK_GetState( ) == NETSTATE_CLIENT )
	{
		AInventory *item = players[consoleplayer].mo->InvSel;
		CLIENTCOMMANDS_RequestInventoryUse( item );
		players[consoleplayer].inventorytics = 0;
	}
	else
	{
		if (players[consoleplayer].inventorytics == 0 || gameinfo.gametype == GAME_Strife)
		{
			SendItemUse = players[consoleplayer].mo->InvSel;
		}
		players[consoleplayer].inventorytics = 0;
	}
}

CCMD (use)
{
	// [BB] If we are a client, we have to bypass the way ZDoom handles the item usage.
	if( NETWORK_GetState( ) == NETSTATE_CLIENT )
	{
		if (argv.argc() > 1)
		{
			AInventory *item = players[consoleplayer].mo->FindInventory (PClass::FindClass (argv[1]));
			CLIENTCOMMANDS_RequestInventoryUse( item );
		}
	}
	else
	{
		if (argv.argc() > 1 && who != NULL)
		{
			SendItemUse = who->FindInventory (PClass::FindClass (argv[1]));
		}
	}
}

CCMD (invdrop)
{
	SendItemDrop = players[consoleplayer].mo->InvSel;
}

CCMD (drop)
{
	if (argv.argc() > 1 && who != NULL)
	{
		SendItemDrop = who->FindInventory (PClass::FindClass (argv[1]));
	}
}

CCMD (useflechette)
{ // Select from one of arti_poisonbag1-3, whichever the player has
	static const ENamedName bagnames[3] =
	{
		NAME_ArtiPoisonBag1,
		NAME_ArtiPoisonBag2,
		NAME_ArtiPoisonBag3
	};
	int i, j;

	if (who == NULL)
		return;

	if (who->IsKindOf (PClass::FindClass (NAME_ClericPlayer)))
		i = 0;
	else if (who->IsKindOf (PClass::FindClass (NAME_MagePlayer)))
		i = 1;
	else
		i = 2;

	for (j = 0; j < 3; ++j)
	{
		AInventory *item;
		if ( (item = who->FindInventory (bagnames[(i+j)%3])) )
		{
			SendItemUse = item;
			break;
		}
	}
}

CCMD (select)
{
	if (argv.argc() > 1)
	{
		AInventory *item = who->FindInventory (PClass::FindClass (argv[1]));
		if (item != NULL)
		{
			who->InvSel = item;
		}
	}
	who->player->inventorytics = 5*TICRATE;
}

//
// G_BuildTiccmd
// Builds a ticcmd from all of the available inputs
// or reads it from the demo buffer.
// If recording a demo, write it out
//
void G_BuildTiccmd (ticcmd_t *cmd)
{
	int 		strafe;
	int 		speed;
	int 		forward;
	int 		side;
	int			fly;

	ticcmd_t	*base;

	base = I_BaseTiccmd (); 			// empty, or external driver
	*cmd = *base;

	cmd->consistancy = consistancy[consoleplayer][(maketic/ticdup)%BACKUPTICS];

	strafe = Button_Strafe.bDown;
	speed = Button_Speed.bDown ^ (int)cl_run;

	forward = side = fly = 0;

	// [RH] only use two stage accelerative turning on the keyboard
	//		and not the joystick, since we treat the joystick as
	//		the analog device it is.
	if (Button_Left.bDown || Button_Right.bDown)
		turnheld += ticdup;
	else
		turnheld = 0;

	// let movement keys cancel each other out
	if (strafe)
	{
		if (Button_Right.bDown)
			side += sidemove[speed];
		if (Button_Left.bDown)
			side -= sidemove[speed];
	}
	else
	{
		int tspeed = speed;

		if (turnheld < SLOWTURNTICS)
			tspeed *= 2;		// slow turn
		
		if (Button_Right.bDown)
		{
			G_AddViewAngle (angleturn[tspeed]);
			LocalKeyboardTurner = true;
		}
		if (Button_Left.bDown)
		{
			G_AddViewAngle (-angleturn[tspeed]);
			LocalKeyboardTurner = true;
		}
	}

	if (Button_LookUp.bDown)
		G_AddViewPitch (lookspeed[speed]);
	if (Button_LookDown.bDown)
		G_AddViewPitch (-lookspeed[speed]);

	if (Button_MoveUp.bDown)
		fly += flyspeed[speed];
	if (Button_MoveDown.bDown)
		fly -= flyspeed[speed];

	if (Button_Klook.bDown)
	{
		if (Button_Forward.bDown)
			G_AddViewPitch (lookspeed[speed]);
		if (Button_Back.bDown)
			G_AddViewPitch (-lookspeed[speed]);
	}
	else
	{
		if (Button_Forward.bDown)
			forward += forwardmove[speed];
		if (Button_Back.bDown)
			forward -= forwardmove[speed];
	}

	if (Button_MoveRight.bDown)
		side += sidemove[speed];
	if (Button_MoveLeft.bDown)
		side -= sidemove[speed];

	// buttons
	if (Button_Attack.bDown)
		cmd->ucmd.buttons |= BT_ATTACK;

	if (Button_AltAttack.bDown)
		cmd->ucmd.buttons |= BT_ALTATTACK;

	if (Button_Use.bDown)
		cmd->ucmd.buttons |= BT_USE;

	if (Button_Jump.bDown)
		cmd->ucmd.buttons |= BT_JUMP;

	if (Button_Crouch.bDown)
		cmd->ucmd.buttons |= BT_DUCK;

	// [RH] Scale joystick moves to full range of allowed speeds
	if (JoyAxes[JOYAXIS_PITCH] != 0)
	{
		G_AddViewPitch (int((JoyAxes[JOYAXIS_PITCH] * 2048) / 256));
		LocalKeyboardTurner = true;
	}
	if (JoyAxes[JOYAXIS_YAW] != 0)
	{
		G_AddViewAngle (int((-1280 * JoyAxes[JOYAXIS_YAW]) / 256));
		LocalKeyboardTurner = true;
	}

	side += int((MAXPLMOVE * JoyAxes[JOYAXIS_SIDE]) / 256);
	forward += int((JoyAxes[JOYAXIS_FORWARD] * MAXPLMOVE) / 256);
	fly += int(JoyAxes[JOYAXIS_UP] * 8);

	if (!Button_Mlook.bDown && !freelook)
	{
		forward += (int)((float)mousey * m_forward);
	}

	cmd->ucmd.pitch = LocalViewPitch >> 16;

	if (SendLand)
	{
		SendLand = false;
		fly = -32768;
	}

	if (strafe || lookstrafe)
		side += (int)((float)mousex * m_side);

	mousex = mousey = 0;

	if (forward > MAXPLMOVE)
		forward = MAXPLMOVE;
	else if (forward < -MAXPLMOVE)
		forward = -MAXPLMOVE;
	if (side > MAXPLMOVE)
		side = MAXPLMOVE;
	else if (side < -MAXPLMOVE)
		side = -MAXPLMOVE;

	cmd->ucmd.forwardmove += forward;
	cmd->ucmd.sidemove += side;
	cmd->ucmd.yaw = LocalViewAngle >> 16;
	cmd->ucmd.upmove = fly;
	LocalViewAngle = 0;
	LocalViewPitch = 0;

	// special buttons
	if (sendturn180)
	{
		sendturn180 = false;
		cmd->ucmd.buttons |= BT_TURN180;
	}
	if (sendpause)
	{
		sendpause = false;
		Net_WriteByte (DEM_PAUSE);
	}
	if (sendsave)
	{
		sendsave = false;
		Net_WriteByte (DEM_SAVEGAME);
		Net_WriteString (savegamefile);
		Net_WriteString (savedescription);
		savegamefile = "";
	}
	if (SendItemUse == (const AInventory *)1)
	{
		Net_WriteByte (DEM_INVUSEALL);
		SendItemUse = NULL;
	}
	else if (SendItemUse != NULL)
	{
		Net_WriteByte (DEM_INVUSE);
		Net_WriteLong (SendItemUse->InventoryID);
		SendItemUse = NULL;
	}
	if (SendItemDrop != NULL)
	{
		Net_WriteByte (DEM_INVDROP);
		Net_WriteLong (SendItemDrop->InventoryID);
		SendItemDrop = NULL;
	}

	cmd->ucmd.forwardmove <<= 8;
	cmd->ucmd.sidemove <<= 8;
}

//[Graf Zahl] This really helps if the mouse update rate can't be increased!
CVAR (Bool,		smooth_mouse,	false,	CVAR_GLOBALCONFIG|CVAR_ARCHIVE)

void G_AddViewPitch (int look)
{
	if (gamestate == GS_TITLELEVEL)
	{
		return;
	}
	look <<= 16;
	if (dmflags & DF_NO_FREELOOK)
	{
		LocalViewPitch = 0;
	}
	else if (look > 0)
	{
		// Avoid overflowing
		if (LocalViewPitch + look <= LocalViewPitch)
		{
			LocalViewPitch = 0x78000000;
		}
		else
		{
			LocalViewPitch = MIN(LocalViewPitch + look, 0x78000000);
		}
	}
	else if (look < 0)
	{
		// Avoid overflowing
		if (LocalViewPitch + look >= LocalViewPitch)
		{
			LocalViewPitch = -0x78000000;
		}
		else
		{
			LocalViewPitch = MAX(LocalViewPitch + look, -0x78000000);
		}
	}
	if (look != 0)
	{
		LocalKeyboardTurner = smooth_mouse;
	}
}

void G_AddViewAngle (int yaw)
{
	if (gamestate == GS_TITLELEVEL)
	{
		return;
	}
	LocalViewAngle -= yaw << 16;
	if (yaw != 0)
	{
		LocalKeyboardTurner = smooth_mouse;
	}
}

EXTERN_CVAR( Bool, sv_cheats );

// [RH] Spy mode has been separated into two console commands.
//		One goes forward; the other goes backward.
// [BC] Prototype for new function.
static void FinishChangeSpy( int pnum );
static void ChangeSpy (bool forward)
{
	// If you're not in a level, then you can't spy.
	if (gamestate != GS_LEVEL)
	{
		return;
	}

	// If not viewing through a player, return your eyes to your own head.
	if (!players[consoleplayer].camera || players[consoleplayer].camera->player == NULL)
	{
		players[consoleplayer].camera = players[consoleplayer].mo;
		return;
	}

	// [BC] Check a wide array of conditions to see if this is legal.
	if (( demoplayback == false ) &&
		( CLIENTDEMO_IsPlaying( ) == false ) &&
		( players[consoleplayer].bSpectating == false ) &&
		( NETWORK_GetState( ) == NETSTATE_CLIENT ) &&
		( teamgame == false ) &&
		( teamlms == false ) &&
		( teamplay == false ) &&
		( teampossession == false ) &&
		((( deathmatch == false ) && ( teamgame == false )) == false ) &&
		( sv_cheats == false ))
	{
		return;
	}

	// Otherwise, cycle to the next player.
	int pnum = players[consoleplayer].camera->player - players;
	int step = forward ? 1 : -1;

	// [BC] Special conditions for team LMS.
	if (( teamlms ) && (( lmsspectatorsettings & LMS_SPF_VIEW ) == false ))
	{
		// If this player is a true spectator (aka not on a team), don't allow him to change spy.
		if ( PLAYER_IsTrueSpectator( &players[consoleplayer] ))
		{
			players[consoleplayer].camera = players[consoleplayer].mo;
			FinishChangeSpy( consoleplayer );
			return;
		}

		// Break if the player isn't on a team.
		if ( players[consoleplayer].bOnTeam == false )
		{
			players[consoleplayer].camera = players[consoleplayer].mo;
			FinishChangeSpy( consoleplayer );
			return;
		}

		// Loop through all the players, and stop when we find one that's on our team.
		do
		{
			pnum += step;
			pnum &= MAXPLAYERS-1;

			// Skip players not in the game.
			if ( playeringame[pnum] == false )
				continue;

			// Skip other spectators.
			if ( players[pnum].bSpectating )
				continue;

			// Skip players not on our team.
			if (( players[pnum].bOnTeam == false ) || ( players[pnum].ulTeam != players[consoleplayer].ulTeam ))
				continue;

			break;
		} while ( pnum != consoleplayer );

		players[consoleplayer].camera = players[pnum].mo;
		FinishChangeSpy( pnum );
		return;
	}

	// [BC] Don't allow spynext in LMS when the spectator settings forbid it.
	if (( lastmanstanding ) && (( lmsspectatorsettings & LMS_SPF_VIEW ) == false ))
	{
		players[consoleplayer].camera = players[consoleplayer].mo;
		FinishChangeSpy( consoleplayer );
		return;
	}

	// [BC] Always allow spectator spying.
	if ( players[consoleplayer].bSpectating )
	{
		// Loop through all the players, and stop when we find one.
		do
		{
			pnum += step;
			pnum &= MAXPLAYERS-1;

			// Skip players not in the game.
			if ( playeringame[pnum] == false )
				continue;

			// Skip other spectators.
			if ( players[pnum].bSpectating )
				continue;

			break;
		} while ( pnum != consoleplayer );

		FinishChangeSpy( pnum );
		return;
	}

	// [BC] Allow view switch to players on our team.
	if ( GAMEMODE_GetFlags( GAMEMODE_GetCurrentMode( )) & GMF_PLAYERSONTEAMS )
	{
		// Break if the player isn't on a team.
		if ( players[consoleplayer].bOnTeam == false )
			return;

		// Loop through all the players, and stop when we find one that's on our team.
		do
		{
			pnum += step;
			pnum &= MAXPLAYERS - 1;

			// Skip players not in the game.
			if ( playeringame[pnum] == false )
				continue;

			// Skip other spectators.
			if ( players[pnum].bSpectating )
				continue;

			// Skip players not on our team.
			if (( players[pnum].bOnTeam == false ) || ( players[pnum].ulTeam != players[consoleplayer].ulTeam ))
				continue;

			break;
		} while ( pnum != consoleplayer );
	}
	// Deathmatch and co-op.
	else
	{
		// Loop through all the players, and stop when we find one that's on our team.
		while ( 1 )
		{
			pnum += step;
			pnum &= MAXPLAYERS-1;

			if ( playeringame[pnum] )
				break;
		}
	}

	// [BC] When we're all done, put the camera in the display player's body, etc.
	FinishChangeSpy( pnum );
}

// [BC] Split this out of ChangeSpy() so it can be called from within that function.
static void FinishChangeSpy( int pnum )
{
	players[consoleplayer].camera = players[pnum].mo;
	S_UpdateSounds(players[consoleplayer].camera);
	StatusBar->AttachToPlayer (&players[pnum]);
	// [BC] We really no longer need to do this since we have a message
	// that says "FOLLOWING - xxx" on the status bar.
/*
	if (demoplayback || ( NETWORK_GetState( ) != NETSTATE_SINGLE ))
	{
		StatusBar->ShowPlayerName ();
	}
*/

	// [BC] If we're a client, tell the server that we're switching our displayplayer.
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
		CLIENTCOMMANDS_ChangeDisplayPlayer( pnum );

	// [BC] Also, refresh the HUD since the display player is changing.
	SCOREBOARD_RefreshHUD( );
}

CCMD (spynext)
{
	// allow spy mode changes even during the demo
	ChangeSpy (true);
}

CCMD (spyprev)
{
	// allow spy mode changes even during the demo
	ChangeSpy (false);
}


//
// G_Responder
// Get info needed to make ticcmd_ts for the players.
//
bool G_Responder (event_t *ev)
{
	// any other key pops up menu if in demos
	// [RH] But only if the key isn't bound to a "special" command
	// [BC] Support for client-side demos.
	if (gameaction == ga_nothing && 
		(demoplayback || CLIENTDEMO_IsPlaying( ) || gamestate == GS_DEMOSCREEN || gamestate == GS_TITLELEVEL))
	{
		const char *cmd = C_GetBinding (ev->data1);

		if (ev->type == EV_KeyDown)
		{

			if (!cmd || (
				strnicmp (cmd, "menu_", 5) &&
				stricmp (cmd, "toggleconsole") &&
				stricmp (cmd, "sizeup") &&
				stricmp (cmd, "sizedown") &&
				stricmp (cmd, "togglemap") &&
				stricmp (cmd, "spynext") &&
				stricmp (cmd, "spyprev") &&
				stricmp (cmd, "chase") &&
				stricmp (cmd, "+showscores") &&
				// [BC]
				stricmp (cmd, "+showmedals") &&
				stricmp (cmd, "bumpgamma") &&
				stricmp (cmd, "screenshot")))
			{
				M_StartControlPanel (true);
				return true;
			}
			else
			{
				return C_DoKey (ev);
			}
		}
		if (cmd && cmd[0] == '+')
			return C_DoKey (ev);

		return false;
	}

	// Handle chat input at the level and intermission screens.
	if ( gamestate == GS_LEVEL || gamestate == GS_INTERMISSION )
	{
		if ( CHAT_Input( ev )) 
			return ( true );
	}

	if (gamestate == GS_LEVEL)
	{
		// Player function ate it.
		if ( PLAYER_Responder( ev ))
			return ( true );

		if (ST_Responder (ev))
			return true;		// status window ate it
		if (!viewactive && AM_Responder (ev))
			return true;		// automap ate it
	}
	else if (gamestate == GS_FINALE)
	{
		if (F_Responder (ev))
			return true;		// finale ate the event
	}

	switch (ev->type)
	{
	case EV_KeyDown:
		if (C_DoKey (ev))
			return true;
		break;

	case EV_KeyUp:
		C_DoKey (ev);
		break;

	// [RH] mouse buttons are sent as key up/down events
	case EV_Mouse: 
		mousex = (int)(ev->x * mouse_sensitivity);
		mousey = (int)(ev->y * mouse_sensitivity);
		break;
	}

	// [RH] If the view is active, give the automap a chance at
	// the events *last* so that any bound keys get precedence.

	if (gamestate == GS_LEVEL && viewactive)
		return AM_Responder (ev);

	return (ev->type == EV_KeyDown ||
			ev->type == EV_Mouse);
}

#ifdef	_DEBUG
CVAR( Bool, cl_emulatepacketloss, false, 0 )
#endif


//
// G_Ticker
// Make ticcmd_ts for the players.
//
extern FTexture *Page;


void G_Ticker ()
{
	int i;
	gamestate_t	oldgamestate;
	int			buf;
	ticcmd_t*	cmd;
	LONG		lSize;
#ifdef	_DEBUG
	static	ULONG	s_ulEmulatingPacketLoss;
#endif
	// Client's don't spawn players until instructed by the server.
	if (( NETWORK_GetState( ) != NETSTATE_CLIENT ) &&
		( CLIENTDEMO_IsPlaying( ) == false ))
	{
		// do player reborns if needed
		for (i = 0; i < MAXPLAYERS; i++)
		{
			// [BC] PST_REBORNNOINVENTORY, PST_ENTERNOINVENTORY
			if (playeringame[i] &&
				(players[i].playerstate == PST_REBORN ||
				players[i].playerstate == PST_REBORNNOINVENTORY ||
				players[i].playerstate == PST_ENTER ||
				players[i].playerstate == PST_ENTERNOINVENTORY))
			{
				G_DoReborn (i, false);
			}
		}
	}
	// [BC] Tick the client and client statistics modules.
	else if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
	{
		CLIENT_Tick( );
		CLIENTSTATISTICS_Tick( );
	}

	if (ToggleFullscreen)
	{
		static char toggle_fullscreen[] = "toggle fullscreen";
		ToggleFullscreen = false;
		AddCommandString (toggle_fullscreen);
	}

	// do things to change the game state
	oldgamestate = gamestate;
	while (gameaction != ga_nothing)
	{
		if (gameaction == ga_newgame2)
		{
			gameaction = ga_newgame;
			break;
		}
		switch (gameaction)
		{
		case ga_loadlevel:
			G_DoLoadLevel (-1, false);
			break;
		case ga_newgame2:	// Silence GCC (see above)
		case ga_newgame:
			G_DoNewGame ();
			break;
		case ga_loadgame:
			G_DoLoadGame ();
			break;
		case ga_savegame:
			G_DoSaveGame (true);
			break;
		case ga_autosave:
			G_DoAutoSave ();
			break;
		case ga_playdemo:
			G_DoPlayDemo ();
			break;
		case ga_completed:
			G_DoCompleted ();
			break;
		case ga_slideshow:
			F_StartSlideshow ();
			break;
		case ga_worlddone:
			G_DoWorldDone ();
			break;
		case ga_screenshot:
			M_ScreenShot (shotfile);
			shotfile = "";
			gameaction = ga_nothing;
			break;
		case ga_fullconsole:
			C_FullConsole ();
			gameaction = ga_nothing;
			break;
		case ga_togglemap:
			AM_ToggleMap ();
			gameaction = ga_nothing;
			break;
		case ga_nothing:
			break;
		}
		C_AdjustBottom ();
	}

	if (oldgamestate != gamestate)
	{
		if (oldgamestate == GS_DEMOSCREEN && Page != NULL)
		{
			Page->Unload();
			Page = NULL;
		}
		else if (oldgamestate == GS_FINALE)
		{
			F_EndFinale ();
		}
	}

	// get commands, check consistancy, and build new consistancy check
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
		buf = gametic % MAXSAVETICS;
	else
		buf = (gametic/ticdup)%BACKUPTICS;

	// If we're the client, attempt to read in packets from the server.
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
	{
		cmd = &players[consoleplayer].cmd;
//		memcpy( cmd, &netcmds[consoleplayer][buf], sizeof( ticcmd_t ));
		memcpy( cmd, &netcmds[0][buf], sizeof( ticcmd_t ));
	}

	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
	{
		while (( lSize = NETWORK_GetPackets( )) > 0 )
		{
			UCVarValue		Val;
			NETADDRESS_s	MasterAddress;
			char			*pszMasterPort;
			BYTESTREAM_s	*pByteStream;

			Val = cl_masterip.GetGenericRep( CVAR_String );
			NETWORK_StringToAddress( Val.String, &MasterAddress );

			// Allow the user to specify which port the master server is on.
			pszMasterPort = Args.CheckValue( "-masterport" );
			if ( pszMasterPort )
				MasterAddress.usPort = NETWORK_ntohs( atoi( pszMasterPort ));
			else 
				MasterAddress.usPort = NETWORK_ntohs( DEFAULT_MASTER_PORT );

			pByteStream = &NETWORK_GetNetworkMessageBuffer( )->ByteStream;

			// If we're a client and receiving a message from the server...
			if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) &&
				( NETWORK_CompareAddress( NETWORK_GetFromAddress( ), CLIENT_GetServerAddress( ), false )))
			{
				// Statistics.
				CLIENTSTATISTICS_AddToBytesReceived( lSize );

#ifdef	_DEBUG
				// Emulate packet loss for debugging.
				if (( cl_emulatepacketloss ) && gamestate == GS_LEVEL )
				{
					if ( s_ulEmulatingPacketLoss )
					{
						--s_ulEmulatingPacketLoss;
						break;
					}

					// Should activate once every two/three seconds or so.
					if ( M_Random( ) < 4 )
					{
						// This should give a range anywhere from 1 to 128 ticks.
						s_ulEmulatingPacketLoss = ( M_Random( ) / 4 );
					}

//					if (( M_Random( ) < 170 ) && gamestate == GS_LEVEL )
//						break;
				}
#endif
				// We've gotten a packet! Now figure out what it's saying.
				if ( CLIENT_ReadPacketHeader( pByteStream ))
				{
					// If we're recording a demo, write the contents of this packet.
					if ( CLIENTDEMO_IsRecording( ))
						CLIENTDEMO_WritePacket( pByteStream );

					CLIENT_ParsePacket( pByteStream, false );
				}
				else
				{
					while ( CLIENT_GetNextPacket( ))
					{
						pByteStream = &NETWORK_GetNetworkMessageBuffer( )->ByteStream;

						// If we're recording a demo, write the contents of this packet.
						if ( CLIENTDEMO_IsRecording( ))
							CLIENTDEMO_WritePacket( pByteStream );

						// Parse this packet.
						CLIENT_ParsePacket( pByteStream, true );

						// Don't continue parsing if we've exited the network game.
						if ( CLIENT_GetConnectionState( ) == CTS_DISCONNECTED )
							break;
					}
				}

				// We've heard from the server; don't time out!
				CLIENT_SetLastServerTick( gametic );
				CLIENT_SetServerLagging( false );
			}
			else
			{
				char			*pszPrefix1 = "127.0.0.1";
				char			*pszPrefix2 = "10.";
				char			*pszPrefix3 = "172.16.";
				char			*pszPrefix4 = "192.168.";
				char			*pszAddressBuf;
				NETADDRESS_s	AddressFrom;
				LONG			lCommand;

				pszAddressBuf = NETWORK_AddressToString( NETWORK_GetFromAddress( ));

				// Skulltag is receiving a message from something on the LAN.
				if (( strncmp( pszAddressBuf, pszPrefix1, 9 ) == 0 ) || 
					( strncmp( pszAddressBuf, pszPrefix2, 3 ) == 0 ) ||
					( strncmp( pszAddressBuf, pszPrefix3, 7 ) == 0 ) ||
					( strncmp( pszAddressBuf, pszPrefix4, 8 ) == 0 ))
				{
					AddressFrom = MasterAddress;

					// Keep the same port as the from address.
					AddressFrom.usPort = NETWORK_GetFromAddress( ).usPort;
				}
				else
					AddressFrom = NETWORK_GetFromAddress( );

				// If we're receiving info from the master server...
				if ( NETWORK_CompareAddress( AddressFrom, MasterAddress, false ))
				{
					lCommand = NETWORK_ReadLong( pByteStream );
					switch ( lCommand )
					{
					case MSC_BEGINSERVERLIST:

						// Get the list of servers.
						BROWSER_GetServerList( pByteStream );

						// Now, query all the servers on the list.
						BROWSER_QueryAllServers( );

						// Finally, clear the server list. Server slots will be reactivated when
						// they come in.
						BROWSER_DeactivateAllServers( );
						break;
					default:

						Printf( "Unknown command from master server: %d\n", lCommand );
						break;
					}
				}
				// Perhaps it's a message from a server we're queried.
				else
				{
					lCommand = NETWORK_ReadLong( pByteStream );
					if ( lCommand == SERVER_LAUNCHER_CHALLENGE )
						BROWSER_ParseServerQuery( pByteStream, false );
					else if ( lCommand == SERVER_LAUNCHER_IGNORING )
						Printf( "WARNING! Please wait a full 10 seconds before refreshing the server list.\n" );
//					else
//						Printf( "Unknown network message from %s.\n", NETWORK_AddressToString( g_AddressFrom ));
				}
			}
		}

		while (( lSize = NETWORK_GetLANPackets( )) > 0 )
		{
			LONG			lCommand;
			BYTESTREAM_s	*pByteStream;

			pByteStream = &NETWORK_GetNetworkMessageBuffer( )->ByteStream;

			lCommand = NETWORK_ReadLong( pByteStream );
			if ( lCommand == SERVER_LAUNCHER_CHALLENGE )
				BROWSER_ParseServerQuery( pByteStream, true );
		}

		// Now that we're done parsing the multiple packets the server has sent our way, check
		// to see if any packets are missing.
		if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) && ( CLIENT_GetConnectionState( ) >= CTS_REQUESTINGSNAPSHOT ))
			CLIENT_CheckForMissingPackets( );
	}

	// If we're playing back a demo, read packets and ticcmds now.
	if ( CLIENTDEMO_IsPlaying( ))
		CLIENTDEMO_ReadPacket( );

	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
	{
		// Check if either us or the server is lagging.
		CLIENT_WaitForServer( );

		// If all is good, send our commands.
		if ( CLIENT_GetServerLagging( ) == false )
			CLIENT_SendCmd( );

		// If we're recording a demo, write the player's commands.
		if ( CLIENTDEMO_IsRecording( ))
			CLIENTDEMO_WriteTiccmd( &players[consoleplayer].cmd );
	}

	if (( NETWORK_GetState( ) != NETSTATE_CLIENT ) &&
		( NETWORK_GetState( ) != NETSTATE_SERVER ) &&
		( CLIENTDEMO_IsPlaying( ) == false ))
	{
		// [RH] Include some random seeds and player stuff in the consistancy
		// check, not just the player's x position like BOOM.
		DWORD rngsum = FRandom::StaticSumSeeds ();

		for (i = 0; i < MAXPLAYERS; i++)
		{
			if (playeringame[i])
			{
				ticcmd_t *cmd = &players[i].cmd;
				ticcmd_t *newcmd = &netcmds[i][buf];

				if ((gametic % ticdup) == 0)
				{
					RunNetSpecs (i, buf);
				}
				if (demorecording)
				{
					G_WriteDemoTiccmd (newcmd, i, buf);
				}
				// If the user alt-tabbed away, paused gets set to -1. In this case,
				// we do not want to read more demo commands until paused is no
				// longer negative.
				if (demoplayback && paused >= 0)
				{
					G_ReadDemoTiccmd (cmd, i);
				}
				else
				{
					memcpy (cmd, newcmd, sizeof(ticcmd_t));
				}

				// check for turbo cheats
				if (cmd->ucmd.forwardmove > TURBOTHRESHOLD &&
					!(gametic&31) && ((gametic>>5)&(MAXPLAYERS-1)) == i )
				{
					Printf ("%s is turbo!\n", players[i].userinfo.netname);
				}
			}
		}
	}

	// Check if the players are moving faster than they should.
	if (( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( sv_cheats == false ))
	{
		for ( i = 0; i < MAXPLAYERS; i++ )
		{
			if ( playeringame[i] )
			{
				LONG		lMaxThreshold;
				ticcmd_t	*cmd = &players[i].cmd;

				lMaxThreshold = TURBOTHRESHOLD;
				if ( players[i].cheats & CF_SPEED25 )
					lMaxThreshold *= 2;

				// [BB] In case the speed of the player is increased by 50 percent, lMaxThreshold is multiplied by 4,
				// exactly like it was done with CF_SPEED before.
				if ( (players[i].mo) && (players[i].mo->Inventory) && (players[i].mo->Inventory->GetSpeedFactor() > FRACUNIT) )
				{
					float floatSpeedFactor = static_cast<float>(players[i].mo->Inventory->GetSpeedFactor())/static_cast<float>(FRACUNIT);
					lMaxThreshold *= (6.*floatSpeedFactor-5.);
				}

				// Check for turbo cheats.
				if (( cmd->ucmd.forwardmove > lMaxThreshold ) ||
					( cmd->ucmd.forwardmove < -lMaxThreshold ) ||
					( cmd->ucmd.sidemove > lMaxThreshold ) ||
					( cmd->ucmd.sidemove < -lMaxThreshold ))
				{
					// If the player was moving faster than he was allowed to, kick him!
					SERVER_KickPlayer( i, "Speed exceeds that allowed (turbo cheat)." );
				}
			}
		}
	}

	// do main actions
	switch (gamestate)
	{
	case GS_LEVEL:

		// [BC] Tick these modules, but only if the ticker should run.
		if (( paused == false ) &&
			( P_CheckTickerPaused( ) == false ))
		{
			// Update some general bot stuff.
			BOTS_Tick( );

			// Tick the duel module.
			DUEL_Tick( );

			// Tick the LMS module.
			LASTMANSTANDING_Tick( );

			// Tick the possession module.
			POSSESSION_Tick( );

			// Tick the survival module.
			SURVIVAL_Tick( );

			// Tick the invasion module.
			INVASION_Tick( );

			// Reset the bot cycles counter before we tick their logic.
			BOTS_ResetCyclesCounter( );

			// Tick the callvote module.
			CALLVOTE_Tick( );
		}

		P_Ticker ();
		AM_Ticker ();

		// Tick the medal system.
		MEDAL_Tick( );

		// Play "Welcome" sounds for teamgame modes.
		if ( g_ulLevelIntroTicks < TICRATE )
		{
			g_ulLevelIntroTicks++;
			if ( g_ulLevelIntroTicks == TICRATE )
			{
				if ( oneflagctf )
					ANNOUNCER_PlayEntry( cl_announcer, "WelcomeToOneFlagCTF" );
				else if ( ctf )
					ANNOUNCER_PlayEntry( cl_announcer, "WelcomeToCTF" );
				else if ( skulltag )
					ANNOUNCER_PlayEntry( cl_announcer, "WelcomeToST" );
			}
		}

		// Apply end level delay.
		if (( g_ulEndLevelDelay ) &&
			( NETWORK_GetState( ) != NETSTATE_CLIENT ) &&
			( CLIENTDEMO_IsPlaying( ) == false ))
		{
			if ( --g_ulEndLevelDelay == 0 )
			{
				// Tell the clients about the expired end level delay.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_SetGameEndLevelDelay( g_ulEndLevelDelay );

				// If we're in a duel, set up the next duel.
				if ( duel )
				{
					// If the player must win all duels, and lost this one, then he's DONE!
					if (( DUEL_GetLoser( ) == consoleplayer ) && ( CAMPAIGN_InCampaign( )) && ( CAMPAIGN_GetCampaignInfo( level.mapname )->bMustWinAllDuels ))
					{
						// Tell the player he loses!
						Printf( "You lose!\n" );

						// End the level.
						G_ExitLevel( 0, false );

						// When the level loads, start the next duel.
						DUEL_SetStartNextDuelOnLevelLoad( true );
					}
					// If we've reached the duel limit, exit the level.
					else if (( duellimit > 0 ) && ( DUEL_GetNumDuels( ) >= duellimit ))
					{
						if ( NETWORK_GetState( ) == NETSTATE_SERVER )
							SERVER_Printf( PRINT_HIGH, "Duellimit hit.\n" );
						else
							Printf( "Duellimit hit.\n" );
						G_ExitLevel( 0, false );

						// When the level loads, start the next duel.
						DUEL_SetStartNextDuelOnLevelLoad( true );
					}
					else
					{
						// Send the loser back to the spectators! Doing so will automatically set up
						// the next duel.
						DUEL_SendLoserToSpectators( );
					}
				}
				else if ( lastmanstanding || teamlms )
				{
					bool	bLimitHit = false;

					if ( winlimit > 0 )
					{
						if ( lastmanstanding )
						{
							ULONG	ulIdx;
							
							for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
							{
								if (( playeringame[ulIdx] == false ) || ( PLAYER_IsTrueSpectator( &players[ulIdx] )))
									continue;

								if ( players[ulIdx].ulWins >= winlimit )
								{
									bLimitHit = true;
									break;
								}
							}
						}
						else
						{
							if (( TEAM_GetWinCount( TEAM_BLUE ) >= winlimit ) || ( TEAM_GetWinCount( TEAM_RED ) >= winlimit ))
								bLimitHit = true;
						}

						if ( bLimitHit )
						{
							if ( NETWORK_GetState( ) == NETSTATE_SERVER )
								SERVER_Printf( PRINT_HIGH, "Winlimit hit.\n" );
							else
								Printf( "Winlimit hit.\n" );
							G_ExitLevel( 0, false );

							// When the level loads, start the next match.
//							LASTMANSTANDING_SetStartNextMatchOnLevelLoad( true );
						}
						else
						{
							LASTMANSTANDING_SetState( LMSS_WAITINGFORPLAYERS );
							LASTMANSTANDING_Tick( );
						}
					}
					else
					{
						LASTMANSTANDING_SetState( LMSS_WAITINGFORPLAYERS );
						LASTMANSTANDING_Tick( );
					}
				}
				else if ( possession || teampossession )
				{
					bool	bLimitHit = false;

					if ( pointlimit > 0 )
					{
						if ( possession )
						{
							ULONG	ulIdx;
							
							for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
							{
								if (( playeringame[ulIdx] == false ) || ( PLAYER_IsTrueSpectator( &players[ulIdx] )))
									continue;

								if ( players[ulIdx].lPointCount >= pointlimit )
								{
									bLimitHit = true;
									break;
								}
							}
						}
						else
						{
							if (( TEAM_GetScore( TEAM_BLUE ) >= pointlimit ) || ( TEAM_GetScore( TEAM_RED ) >= pointlimit ))
								bLimitHit = true;
						}

						if ( bLimitHit )
						{
							if ( NETWORK_GetState( ) == NETSTATE_SERVER )
								SERVER_Printf( PRINT_HIGH, "Winlimit hit.\n" );
							else
								Printf( "Winlimit hit.\n" );
							G_ExitLevel( 0, false );
						}
						else
						{
							POSSESSION_SetState( PSNS_PRENEXTROUNDCOUNTDOWN );
							POSSESSION_Tick( );
						}
					}
					else
					{
						POSSESSION_SetState( PSNS_PRENEXTROUNDCOUNTDOWN );
						POSSESSION_Tick( );
					}
				}
				else if ( survival )
				{
					SURVIVAL_SetState( SURVS_WAITINGFORPLAYERS );
					SURVIVAL_Tick( );
				}
				else
					G_ExitLevel( 0, false );
			}
		}

		break;

	case GS_TITLELEVEL:
		P_Ticker ();
		break;

	case GS_INTERMISSION:

		// Make sure bots are ticked during intermission.
		{
			ULONG	ulIdx;

			// Reset the bot cycles counter before we tick their logic.
			BOTS_ResetCyclesCounter( );

			for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
			{
				if ( playeringame[ulIdx] == false )
					continue;

				if ( players[ulIdx].pSkullBot )
					players[ulIdx].pSkullBot->Tick( );
			}
		}

		WI_Ticker ();
		break;

	case GS_FINALE:
		F_Ticker ();
		break;

	case GS_DEMOSCREEN:
		D_PageTicker ();
		break;

	case GS_STARTUP:
		if (gameaction == ga_nothing)
		{
			gamestate = GS_FULLCONSOLE;
			gameaction = ga_fullconsole;
		}
		break;

	default:
		break;
	}

	// [BC] If any data has accumulated in our packet, send it out now.
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
		CLIENT_EndTick( );
}


//
// PLAYER STRUCTURE FUNCTIONS
// also see P_SpawnPlayer in P_Mobj
//

//
// G_PlayerFinishLevel
// Called when a player completes a level.
//
void G_PlayerFinishLevel (int player, EFinishLevelType mode, bool resetinventory)
{
	AInventory *item, *next;
	player_t *p;

	p = &players[player];

	// Strip all current powers, unless moving in a hub and the power is okay to keep.
	item = p->mo->Inventory;
	while (item != NULL)
	{
		next = item->Inventory;
		if (item->IsKindOf (RUNTIME_CLASS(APowerup)))
		{
			if (deathmatch || mode != FINISH_SameHub || !(item->ItemFlags & IF_HUBPOWER))
			{
				item->Destroy ();
			}
		}
		item = next;
	}
	if (p->ReadyWeapon != NULL &&
		p->ReadyWeapon->WeaponFlags&WIF_POWERED_UP &&
		p->PendingWeapon == p->ReadyWeapon->SisterWeapon)
	{
		// Unselect powered up weapons if the unpowered counterpart is pending
		p->ReadyWeapon=p->PendingWeapon;
	}
	p->mo->flags &= ~MF_SHADOW; 		// cancel invisibility
	p->mo->RenderStyle = STYLE_Normal;
	p->mo->alpha = FRACUNIT;
	p->extralight = 0;					// cancel gun flashes
	p->fixedcolormap = 0;				// cancel ir goggles
	p->damagecount = 0; 				// no palette changes
	p->bonuscount = 0;
	p->poisoncount = 0;
	p->inventorytics = 0;

	if (mode != FINISH_SameHub)
	{
		// Take away flight and keys (and anything else with IF_INTERHUBSTRIP set)
		item = p->mo->Inventory;
		while (item != NULL)
		{
			next = item->Inventory;
			if (item->ItemFlags & IF_INTERHUBSTRIP)
			{
				item->Destroy ();
			}
			item = next;
		}
	}

	if (mode == FINISH_NoHub && !(level.flags & LEVEL_KEEPFULLINVENTORY))
	{ // Reduce all owned (visible) inventory to 1 item each
		for (item = p->mo->Inventory; item != NULL; item = item->Inventory)
		{
			if (item->ItemFlags & IF_INVBAR)
			{
				item->Amount = 1;
			}
		}
	}

	if (p->morphTics)
	{ // Undo morph
		P_UndoPlayerMorph (p, true);
	}

	// [BC] Reset a bunch of other Skulltag stuff.
	p->ulConsecutiveHits = 0;
	p->ulConsecutiveRailgunHits = 0;
	p->ulDeathsWithoutFrag = 0;
	p->ulFragsWithoutDeath = 0;
	p->ulLastExcellentTick = 0;
	p->ulLastFragTick = 0;
	p->ulLastBFGFragTick = 0;
	if ( p->pIcon )
		p->pIcon->Destroy( );

	// Clears the entire inventory and gives back the defaults for starting a game
	if (resetinventory)
	{
		AInventory *inv = p->mo->Inventory;

		while (inv != NULL)
		{
			AInventory *next = inv->Inventory;
			if (!(inv->ItemFlags & IF_UNDROPPABLE))
			{
				inv->Destroy ();
			}
			else if (inv->GetClass() == RUNTIME_CLASS(AHexenArmor))
			{
				AHexenArmor *harmor = static_cast<AHexenArmor *> (inv);
				harmor->Slots[3] = harmor->Slots[2] = harmor->Slots[1] = harmor->Slots[0] = 0;
			}
			inv = next;
		}
		p->ReadyWeapon = NULL;
		p->PendingWeapon = WP_NOCHANGE;
		p->psprites[ps_weapon].state = NULL;
		p->psprites[ps_flash].state = NULL;
		p->mo->GiveDefaultInventory();
	}
}

//
// G_PlayerReborn
// Called after a player dies
// almost everything is cleared and initialized
//
void G_PlayerReborn (int player)
{
	player_t*	p;
	int			fragcount;	// [RH] Cumulative frags
	int 		killcount;
	int 		itemcount;
	int 		secretcount;
	int			chasecam;
	BYTE		currclass;
	userinfo_t  userinfo;	// [RH] Save userinfo
	APlayerPawn *actor;
	const PClass *cls;
	FString		log;
	bool		bOnTeam;
	bool		bSpectating;
	bool		bDeadSpectator;
	ULONG		ulTeam;
	LONG		lPointCount;
	ULONG		ulDeathCount;
	ULONG		ulConsecutiveHits;
	ULONG		ulConsecutiveRailgunHits;
	ULONG		ulDeathsWithoutFrag;
	ULONG		ulMedalCount[NUM_MEDALS];
	CSkullBot	*pSkullBot;
	ULONG		ulPing;
	ULONG		ulWins;
	ULONG		ulTime;
	LONG		lCheats;

	p = &players[player];

	fragcount = p->fragcount;
	killcount = p->killcount;
	itemcount = p->itemcount;
	secretcount = p->secretcount;
	currclass = p->CurrentPlayerClass;
	memcpy (&userinfo, &p->userinfo, sizeof(userinfo));
	actor = p->mo;
	cls = p->cls;
	log = p->LogText;
	chasecam = p->cheats & CF_CHASECAM;

	bOnTeam = p->bOnTeam;
	bSpectating = p->bSpectating;
	bDeadSpectator = p->bDeadSpectator;
	ulTeam = p->ulTeam;
	lPointCount = p->lPointCount;
	ulDeathCount = p->ulDeathCount;
	ulConsecutiveHits = p->ulConsecutiveHits;
	ulConsecutiveRailgunHits = p->ulConsecutiveRailgunHits;
	ulDeathsWithoutFrag = p->ulDeathsWithoutFrag;
	memcpy( &ulMedalCount, &p->ulMedalCount, sizeof( ulMedalCount ));
	pSkullBot = p->pSkullBot;
	ulPing = p->ulPing;
	ulWins = p->ulWins;
	ulTime = p->ulTime;
	lCheats = p->cheats;

	// Reset player structure to its defaults
	p->~player_t();
	::new(p) player_t;

	p->fragcount = fragcount;
	p->killcount = killcount;
	p->itemcount = itemcount;
	p->secretcount = secretcount;
	p->CurrentPlayerClass = currclass;
	memcpy (&p->userinfo, &userinfo, sizeof(userinfo));
	p->mo = actor;
	p->cls = cls;
	p->LogText = log;
	p->cheats |= chasecam;

	p->oldbuttons = 255, p->attackdown = true;	// don't do anything immediately

	p->bOnTeam = bOnTeam;
	p->bSpectating = bSpectating;
	p->bDeadSpectator = bDeadSpectator;
	p->ulTeam = ulTeam;
	p->lPointCount = lPointCount;
	p->ulDeathCount = ulDeathCount;
	p->ulConsecutiveHits = ulConsecutiveHits;
	p->ulConsecutiveRailgunHits = ulConsecutiveRailgunHits;
	p->ulDeathsWithoutFrag = ulDeathsWithoutFrag;
	memcpy( &p->ulMedalCount, &ulMedalCount, sizeof( ulMedalCount ));
	p->pSkullBot = pSkullBot;
	p->ulPing = ulPing;
	p->ulWins = ulWins;
	p->ulTime = ulTime;
	if ( lCheats & CF_FREEZE )
		p->cheats |= CF_FREEZE;

	p->bIsBot = p->pSkullBot ? true : false;

	p->playerstate = PST_LIVE;

	if (gamestate != GS_TITLELEVEL)
	{
		actor->GiveDefaultInventory ();
		p->ReadyWeapon = p->PendingWeapon;
	}

	// [BC] Apply temporary invulnerability when respawned.
	if (( NETWORK_GetState( ) != NETSTATE_CLIENT ) &&
		( CLIENTDEMO_IsPlaying( ) == false ) &&
		(( dmflags2 & DF2_NO_RESPAWN_INVUL ) == false ) &&
		( deathmatch || teamgame || alwaysapplydmflags ) &&
		( p->bSpectating == false ))
	{
		APowerup *invul = static_cast<APowerup*>(actor->GiveInventoryType (RUNTIME_CLASS(APowerInvulnerable)));
		invul->EffectTics = 2*TICRATE;
		invul->BlendColor = 0;				// don't mess with the view
/*
		AInventory	*pInventory;

		// If we're the server, send the powerup update to clients.
		pInventory = actor->FindInventory( RUNTIME_CLASS( APowerInvulnerable ));
		if (( pInventory ) && ( NETWORK_GetState( ) == NETSTATE_SERVER ))
			SERVERCOMMANDS_GiveInventory( p - players, pInventory );
*/
		// Apply respawn invulnerability effect.
		switch ( cl_respawninvuleffect )
		{
		case 1:

			actor->RenderStyle = STYLE_Translucent;
			actor->effects |= FX_VISIBILITYFLICKER;
			break;
		case 2:

			actor->effects |= FX_RESPAWNINVUL;	// [RH] special effect
			break;
		}
	}
}

//
// G_CheckSpot	
// Returns false if the player cannot be respawned
// at the given mapthing2_t spot  
// because something is occupying it 
//

bool G_CheckSpot (int playernum, mapthing2_t *mthing)
{
	fixed_t x;
	fixed_t y;
	fixed_t z, oldz;
	int i;

	x = mthing->x << FRACBITS;
	y = mthing->y << FRACBITS;
	z = mthing->z << FRACBITS;

	z += R_PointInSubsector (x, y)->sector->floorplane.ZatPoint (x, y);

	if (!players[playernum].mo)
	{ // first spawn of level, before corpses
		for (i = 0; i < playernum; i++)
			if (players[i].mo && players[i].mo->x == x && players[i].mo->y == y)
				return false;
		return true;
	}

	oldz = players[playernum].mo->z;	// [RH] Need to save corpse's z-height
	players[playernum].mo->z = z;		// [RH] Checks are now full 3-D

	// killough 4/2/98: fix bug where P_CheckPosition() uses a non-solid
	// corpse to detect collisions with other players in DM starts
	//
	// Old code:
	// if (!P_CheckPosition (players[playernum].mo, x, y))
	//    return false;

	players[playernum].mo->flags |=  MF_SOLID;
	i = P_CheckPosition(players[playernum].mo, x, y);
	players[playernum].mo->flags &= ~MF_SOLID;
	players[playernum].mo->z = oldz;	// [RH] Restore corpse's height
	if (!i)
		return false;

	return true;
}


//
// G_DeathMatchSpawnPlayer 
// Spawns a player at one of the random death match spots 
// called at level load and each death 
//

// [RH] Returns the distance of the closest player to the given mapthing2_t.
static fixed_t PlayersRangeFromSpot (mapthing2_t *spot)
{
	fixed_t closest = INT_MAX;
	fixed_t distance;
	int i;

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i] || !players[i].mo || players[i].health <= 0)
			continue;

		distance = P_AproxDistance (players[i].mo->x - spot->x * FRACUNIT,
									players[i].mo->y - spot->y * FRACUNIT);

		if (distance < closest)
			closest = distance;
	}

	return closest;
}

// Returns the average distance this spot is from all the enemies of ulPlayer.
static fixed_t TeamLMSPlayersRangeFromSpot( ULONG ulPlayer, mapthing2_t *spot )
{
	ULONG	ulNumSpots;
	fixed_t	distance = INT_MAX;
	int i;

	ulNumSpots = 0;
	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i] || !players[i].mo || players[i].health <= 0)
			continue;

		// Ignore players on our team.
		if (( players[ulPlayer].bOnTeam ) && ( players[i].bOnTeam ) && ( players[ulPlayer].ulTeam == players[i].ulTeam ))
			continue;

		ulNumSpots++;
		distance += P_AproxDistance (players[i].mo->x - spot->x * FRACUNIT,
									players[i].mo->y - spot->y * FRACUNIT);
	}

	if ( ulNumSpots )
		return ( distance / ulNumSpots );
	else
		return ( distance );
}

// [RH] Select the deathmatch spawn spot farthest from everyone.
static mapthing2_t *SelectFarthestDeathmatchSpot( ULONG ulPlayer, size_t selections )
{
	fixed_t bestdistance = 0;
	mapthing2_t *bestspot = NULL;
	unsigned int i;

	for (i = 0; i < selections; i++)
	{
		fixed_t distance = PlayersRangeFromSpot (&deathmatchstarts[i]);

		// Did not find a spot.
		if ( distance == INT_MAX )
			continue;

		if ( G_CheckSpot( ulPlayer, &deathmatchstarts[i] ) == false )
			continue;

		if (distance > bestdistance)
		{
			bestdistance = distance;
			bestspot = &deathmatchstarts[i];
		}
	}

	return bestspot;
}


// Try to find a deathmatch spawn spot farthest from our enemies.
static mapthing2_t *SelectBestTeamLMSSpot( ULONG ulPlayer, size_t selections )
{
	ULONG		ulIdx;
	fixed_t		Distance;
	fixed_t		BestDistance;
	mapthing2_t	*pBestSpot;

	pBestSpot = NULL;
	BestDistance = 0;
	for ( ulIdx = 0; ulIdx < selections; ulIdx++ )
	{
		Distance = TeamLMSPlayersRangeFromSpot( ulPlayer, &deathmatchstarts[ulIdx] );

		// Did not find a spot.
		if ( Distance == INT_MAX )
			continue;

		if ( G_CheckSpot( ulPlayer, &deathmatchstarts[ulIdx] ) == false )
			continue;

		if ( Distance > BestDistance )
		{
			BestDistance = Distance;
			pBestSpot = &deathmatchstarts[ulIdx];
		}
	}

	return ( pBestSpot );
}

// [RH] Select a deathmatch spawn spot at random (original mechanism)
static mapthing2_t *SelectRandomDeathmatchSpot (int playernum, unsigned int selections)
{
	unsigned int i, j;

	for (j = 0; j < 20; j++)
	{
		i = pr_dmspawn() % selections;
		if (G_CheckSpot (playernum, &deathmatchstarts[i]) )
		{
			return &deathmatchstarts[i];
		}
	}

	// [RH] return a spot anyway, since we allow telefragging when a player spawns
	return &deathmatchstarts[i];
}
// [RC] Select a possession start
static mapthing2_t *SelectRandomPossessionSpot (int playernum, unsigned int selections)
{
	unsigned int i, j;

	for (j = 0; j < 20; j++)
	{

		i = pr_dmspawn() % selections;

		if (G_CheckSpot (playernum, &PossessionStarts[i]) )
		{
			return &PossessionStarts[i];
		}
	}
		
	// [RC] All spots are occupied (though they really shouldn't)
	// This will give the player the sphere at the start
	return &PossessionStarts[i];
}

// [RC] Select a terminator start
static mapthing2_t *SelectRandomTerminatorSpot (int playernum, unsigned int selections)
{
	unsigned int i, j;

	for (j = 0; j < 20; j++)
	{

		i = pr_dmspawn() % selections;

		if (G_CheckSpot (playernum, &TerminatorStarts[i]) )
		{
			return &TerminatorStarts[i];
		}
	}
		
	// [RC] All spots are occupied (though they really shouldn't)
	// This will give the player the sphere at the start
	return &TerminatorStarts[i];
}


// Select a temporary team spawn spot at random.
static mapthing2_t *SelectTemporaryTeamSpot( USHORT usPlayer, ULONG ulNumSelections )
{
	ULONG	ulNumAttempts;
	ULONG	ulSelection;

	// Try up to 20 times to find a valid spot.
	for ( ulNumAttempts = 0; ulNumAttempts < 20; ulNumAttempts++ )
	{
		ulSelection = ( pr_dmspawn( ) % ulNumSelections );
		if ( G_CheckSpot( usPlayer, &TemporaryTeamStarts[ulSelection] ))
			return ( &TemporaryTeamStarts[ulSelection] );
	}

	// Return a spot anyway, since we allow telefragging when a player spawns.
	return ( &TemporaryTeamStarts[ulSelection] );
}

// Select a blue team spawn spot at random.
static mapthing2_t *SelectRandomBlueTeamSpot( USHORT usPlayer, ULONG ulNumSelections )
{
	ULONG	ulNumAttempts;
	ULONG	ulSelection;

	// Try up to 20 times to find a valid spot.
	for ( ulNumAttempts = 0; ulNumAttempts < 20; ulNumAttempts++ )
	{
		ulSelection = ( pr_dmspawn( ) % ulNumSelections );
		if ( G_CheckSpot( usPlayer, &BlueTeamStarts[ulSelection] ))
			return ( &BlueTeamStarts[ulSelection] );
	}

	// Return a spot anyway, since we allow telefragging when a player spawns.
	return ( &BlueTeamStarts[ulSelection] );
}

// Select a blue team spawn spot at random.
static mapthing2_t *SelectRandomRedTeamSpot( USHORT usPlayer, ULONG ulNumSelections )
{
	ULONG	ulNumAttempts;
	ULONG	ulSelection;

	// Try up to 20 times to find a valid spot.
	for ( ulNumAttempts = 0; ulNumAttempts < 20; ulNumAttempts++ )
	{
		ulSelection = ( pr_dmspawn( ) % ulNumSelections );
		if ( G_CheckSpot( usPlayer, &RedTeamStarts[ulSelection] ))
			return ( &RedTeamStarts[ulSelection] );
	}

	// Return a spot anyway, since we allow telefragging when a player spawns.
	return ( &RedTeamStarts[ulSelection] );
}

// Select a cooperative spawn spot at random.
static mapthing2_t *SelectRandomCooperativeSpot( ULONG ulPlayer, ULONG ulNumSelections )
{
	ULONG		ulNumAttempts;
	ULONG		ulSelection;
	ULONG		ulIdx;

	// Try up to 20 times to find a valid spot.
	for ( ulNumAttempts = 0; ulNumAttempts < 20; ulNumAttempts++ )
	{
		ulIdx = 0;
		while (( playerstarts[ulIdx].type == 0 ) && ( ulIdx < MAXPLAYERS ))
			ulIdx++;

		ulSelection = ( pr_dmspawn( ) % ulNumSelections );
		while ( ulSelection > 0 )
		{
			ulSelection--;
			while (( playerstarts[ulIdx].type == 0 ) && ( ulIdx < MAXPLAYERS ))
				ulIdx++;
		}

		if ( G_CheckSpot( ulPlayer, &playerstarts[ulIdx] ))
			return ( &playerstarts[ulIdx] );
	}

	// Return a spot anyway, since we allow telefragging when a player spawns.
	return ( &playerstarts[ulIdx] );
}

void G_DeathMatchSpawnPlayer( int playernum, bool bClientUpdate )
{
	unsigned int selections;
	mapthing2_t *spot;

	selections = deathmatchstarts.Size ();
	// [RH] We can get by with just 1 deathmatch start
	if (selections < 1)
		I_Error( "No deathmatch starts!" );

	if ( teamlms && ( players[playernum].bOnTeam ))
	{
		// If we didn't find a valid spot, just pick one at random.
		if (( spot = SelectBestTeamLMSSpot( playernum, selections )) == NULL )
			spot = SelectRandomDeathmatchSpot( playernum, selections );
	}
	else if ( dmflags & DF_SPAWN_FARTHEST )
	{
		// If we didn't find a valid spot, just pick one at random.
		if (( spot = SelectFarthestDeathmatchSpot( playernum, selections )) == NULL )
			spot = SelectRandomDeathmatchSpot( playernum, selections );
	}
	else
		spot = SelectRandomDeathmatchSpot (playernum, selections);

	if ( spot == NULL )
		I_Error( "Could not find a valid deathmatch spot! (this should not happen)" );

	P_SpawnPlayer( spot, bClientUpdate, &players[playernum] );
}

void G_TemporaryTeamSpawnPlayer( ULONG ulPlayer, bool bClientUpdate )
{
	ULONG		ulNumSelections;
	mapthing2_t	*pSpot;

	ulNumSelections = TemporaryTeamStarts.Size( );

	// If there aren't any temporary starts, just spawn them at a random team location.
	if ( ulNumSelections < 1 )
	{
		bool	bCanUseRedStarts = false;
		bool	bCanUseBlueStarts = false;
		LONG	lTeam;

		bCanUseRedStarts = RedTeamStarts.Size( ) > 0;
		bCanUseBlueStarts = BlueTeamStarts.Size( ) > 0;

		if ( bCanUseRedStarts && bCanUseBlueStarts )
		{
			FRandom	TeamSeed( "TeamSeed" );

			lTeam = TeamSeed.Random( ) % 2;
		}
		else if ( bCanUseRedStarts )
			lTeam = TEAM_RED;
		else if ( bCanUseBlueStarts )
			lTeam = TEAM_BLUE;
		else
			lTeam = NUM_TEAMS;

		if ( lTeam == NUM_TEAMS )
			I_Error( "No teamgame starts!" );

		G_TeamgameSpawnPlayer( ulPlayer, lTeam, bClientUpdate );
		return;
	}

	// SelectTemporaryTeamSpot should always return a valid spot. If not, we have a problem.
	pSpot = SelectTemporaryTeamSpot( ulPlayer, ulNumSelections );

	// ANAMOLOUS HAPPENING!!!
	if ( pSpot == NULL )
		I_Error( "Could not find a valid temporary spot! (this should not happen)" );

	P_SpawnPlayer( pSpot, bClientUpdate, &players[ulPlayer] );
}

void G_TeamgameSpawnPlayer( ULONG ulPlayer, ULONG ulTeam, bool bClientUpdate )
{
	ULONG		ulNumSelections;
	mapthing2_t	*pSpot;

	if ( ulTeam == TEAM_BLUE )
	{
		ulNumSelections = BlueTeamStarts.Size( );
		if ( ulNumSelections < 1 )
			I_Error( "No blue team starts!" );

		// SelectBlueTeamSpot should always return a valid spot. If not, we have a problem.
		pSpot = SelectRandomBlueTeamSpot( ulPlayer, ulNumSelections );
	}
	else
	{
		ulNumSelections = RedTeamStarts.Size( );
		if ( ulNumSelections < 1 )
			I_Error( "No red team starts!" );

		// SelectRedTeamSpot should always return a valid spot. If not, we have a problem.
		pSpot = SelectRandomRedTeamSpot( ulPlayer, ulNumSelections );
	}

	// ANAMOLOUS HAPPENING!!!
	if ( pSpot == NULL )
		I_Error( "Could not find a valid temporary spot! (this should not happen)" );

	P_SpawnPlayer( pSpot, bClientUpdate, &players[ulPlayer] );
}

void G_CooperativeSpawnPlayer( ULONG ulPlayer, bool bClientUpdate, bool bTempPlayer )
{
	ULONG		ulNumSpots;
	ULONG		ulIdx;
	mapthing2_t	*pSpot;

	// If there's a valid start for this player, spawn him there.
	if (( playerstarts[ulPlayer].type != 0 ) && ( G_CheckSpot( ulPlayer, &playerstarts[ulPlayer] )))
	{
		P_SpawnPlayer( &playerstarts[ulPlayer], bClientUpdate, NULL, bTempPlayer );
		return;
	}

	ulNumSpots = 0;
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( playerstarts[ulIdx].type != 0 )
			ulNumSpots++;
	}

	if ( ulNumSpots < 1 )
		I_Error( "No cooperative starts!" );

	// Now, try to find a valid cooperative start.
	pSpot = SelectRandomCooperativeSpot( ulPlayer, ulNumSpots );

	// ANAMOLOUS HAPPENING!!!
	if ( pSpot == NULL )
		I_Error( "Could not find a valid deathmatch spot! (this should not happen)" );

	P_SpawnPlayer( pSpot, bClientUpdate, &players[ulPlayer], bTempPlayer );
}

//
// G_QueueBody
//
static void G_QueueBody (AActor *body)
{
	// flush an old corpse if needed
	int modslot = bodyqueslot%BODYQUESIZE;

	// [BC] Skulltag has its own system.
	return;

	if (bodyqueslot >= BODYQUESIZE && bodyque[modslot] != NULL)
	{
		bodyque[modslot]->Destroy ();
	}
	bodyque[modslot] = body;

	// Copy the player's translation in case they respawn as something that uses
	// a different translation range.
	R_CopyTranslation (TRANSLATION(TRANSLATION_PlayerCorpses,modslot), body->Translation);
	body->Translation = TRANSLATION(TRANSLATION_PlayerCorpses,modslot);

	bodyqueslot++;
}

//
// G_DoReborn
//
void G_DoReborn (int playernum, bool freshbot)
{
//	int i;
	AActor	*pOldBody;

	// All of this is done remotely.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return;
	}
	else if ( NETWORK_GetState( ) == NETSTATE_SINGLE )
	{
		if (BackupSaveName.Len() > 0 && FileExists (BackupSaveName.GetChars()))
		{ // Load game from the last point it was saved
			savename = BackupSaveName;
			gameaction = ga_loadgame;
		}
		else
		{ // Reload the level from scratch
			bool indemo = demoplayback;
			BackupSaveName = "";
			G_InitNew (level.mapname, false);
			demoplayback = indemo;
//			gameaction = ga_loadlevel;
		}
	}
	else
	{
		// respawn at the start
//		int i;

		// first disassociate the corpse
		pOldBody = players[playernum].mo;
		if ( players[playernum].mo )
		{
			G_QueueBody (players[playernum].mo);
			players[playernum].mo->player = NULL;
		}

		// spawn at random spot if in death match
		if ( deathmatch )
		{
			G_DeathMatchSpawnPlayer( playernum, true );

			// If the player fired a missile before he died, update its target so he gets
			// credit if it kills someone.
			if ( pOldBody )
			{
				AActor						*pActor;
				TThinkerIterator<AActor>	Iterator;
				
				while ( pActor = Iterator.Next( ))
				{
					if ( pActor->target == pOldBody )
						pActor->target = players[playernum].mo;
				}
			}
			return;
		}

		// Spawn the player at their appropriate team start.
		if ( teamgame )
		{
			if ( players[playernum].bOnTeam )
				G_TeamgameSpawnPlayer( playernum, players[playernum].ulTeam, true );
			else
				G_TemporaryTeamSpawnPlayer( playernum, true );

			// If the player fired a missile before he died, update its target so he gets
			// credit if it kills someone.
			if ( pOldBody )
			{
				AActor						*pActor;
				TThinkerIterator<AActor>	Iterator;
				
				while ( pActor = Iterator.Next( ))
				{
					if ( pActor->target == pOldBody )
						pActor->target = players[playernum].mo;
				}
			}
			return;
		}

		G_CooperativeSpawnPlayer( playernum, true );

		// If the player fired a missile before he died, update its target so he gets
		// credit if it kills someone.
		if ( pOldBody )
		{
			AActor						*pActor;
			TThinkerIterator<AActor>	Iterator;
			
			while ( pActor = Iterator.Next( ))
			{
				if ( pActor->target == pOldBody )
					pActor->target = players[playernum].mo;
			}
		}
	}
}

// Determine is a level is a deathmatch, CTF, etc. level by items that are placed on it.
void GAME_CheckMode( void )
{
	ULONG						ulFlags = (ULONG)dmflags;
	ULONG						ulFlags2 = (ULONG)dmflags2;
	UCVarValue					Val;
	ULONG						ulIdx;
	ULONG						ulNumFlags;
	ULONG						ulNumSkulls;
	bool						bPlayerStarts = false;
	AActor						*pActor;
	AActor						*pNewSkull;
	cluster_info_t				*pCluster;
	TThinkerIterator<AActor>	iterator;

	// Clients can't change flags/modes!
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return;
	}

	// By default, we're in regular CTF/ST mode.
	TEAM_SetSimpleCTFMode( false );
	TEAM_SetSimpleSTMode( false );

	// Also, reset the team module.
	TEAM_Reset( );

	// First, we need to count the number of real single/coop player starts.
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( playerstarts[ulIdx].type > 0 )
		{
			bPlayerStarts = true;
			break;
		}
	}

	// We have deathmatch starts, but nothing else.
	if (( deathmatchstarts.Size( ) > 0 ) && 
		( TemporaryTeamStarts.Size( ) == 0 ) && ( BlueTeamStarts.Size( ) == 0 ) && ( RedTeamStarts.Size( ) == 0 ) &&
		 bPlayerStarts == false )
	{
		// Since we only have deathmatch starts, enable deathmatch, and disable teamgame/coop.
		Val.Bool = true;
		deathmatch.ForceSet( Val, CVAR_Bool );
		Val.Bool = false;
		teamgame.ForceSet( Val, CVAR_Bool );

		// Allow the server to use their own dmflags.
		if ( sv_defaultdmflags )
		{
			// Don't do "spawn farthest" for duels.
			if ( duel )
				ulFlags |= DF_WEAPONS_STAY | DF_ITEMS_RESPAWN | DF_NO_MONSTERS | DF_NO_CROUCH;
			else
				ulFlags |= DF_SPAWN_FARTHEST | DF_WEAPONS_STAY | DF_ITEMS_RESPAWN | DF_NO_MONSTERS | DF_NO_CROUCH;
			ulFlags2 |= DF2_YES_DOUBLEAMMO;
		}
	}

	// We have team starts, but nothing else.
	if ((( TemporaryTeamStarts.Size( ) > 0 ) || ( BlueTeamStarts.Size( ) > 0 ) || ( RedTeamStarts.Size( ) > 0 )) &&
		( deathmatchstarts.Size( ) == 0 ) &&
		bPlayerStarts == false )
	{
		// Since we only have teamgame starts, enable teamgame, and disable deathmatch/coop.
		Val.Bool = true;
		teamgame.ForceSet( Val, CVAR_Bool );
		Val.Bool = false;
		deathmatch.ForceSet( Val, CVAR_Bool );

		// Allow the server to use their own dmflags.
		if ( sv_defaultdmflags )
		{
			ulFlags |= DF_WEAPONS_STAY | DF_ITEMS_RESPAWN | DF_NO_MONSTERS | DF_NO_CROUCH;
			ulFlags2 |= DF2_YES_DOUBLEAMMO;
		}

		// Furthermore, we can determine between a ST and CTF level by whether or not
		// there are skulls or flags placed on the level.
//		if ( oneflagctf == false )
		{
			TThinkerIterator<AActor>	iterator;

			ulNumFlags = 0;
			ulNumSkulls = 0;
			while ( pActor = iterator.Next( ))
			{
				if ( pActor->IsKindOf( PClass::FindClass( "BlueSkull" )))
					ulNumSkulls++;

				if ( pActor->IsKindOf( PClass::FindClass( "RedSkull" )))
					ulNumSkulls++;

				if ( pActor->IsKindOf( PClass::FindClass( "BlueFlag" )))
					ulNumFlags++;

				if ( pActor->IsKindOf( PClass::FindClass( "RedFlag" )))
					ulNumFlags++;
			}

			// We found flags but no skulls. Set CTF mode.
			if ( ulNumFlags && ( ulNumSkulls == 0 ))
			{
				Val.Bool = true;

				if ( oneflagctf == false )
					ctf.ForceSet( Val, CVAR_Bool );
			}

			// We found skulls but no flags. Set Skulltag mode.
			else if ( ulNumSkulls && ( ulNumFlags == 0 ))
			{
				Val.Bool = true;

				skulltag.ForceSet( Val, CVAR_Bool );
			}
		}
	}

	// We have single player starts, but nothing else.
	if ( bPlayerStarts == true &&
		( TemporaryTeamStarts.Size( ) == 0 ) && ( BlueTeamStarts.Size( ) == 0 ) && ( RedTeamStarts.Size( ) == 0 ) &&
		( deathmatchstarts.Size( ) == 0 ))
	{
/*
		// Ugh, this is messy... tired and don't feel like explaning why this is needed...
		if (( NETWORK_GetState( ) != NETSTATE_SINGLE ) || deathmatch || teamgame )
		{
			for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
			{
				if ( playeringame[ulIdx] == false )
					continue;

				P_SpawnPlayer( &playerstarts[ulIdx], false, NULL );
			}
		}
*/
		// Since we only have single player starts, disable deathmatch and teamgame.
		Val.Bool = false;
		deathmatch.ForceSet( Val, CVAR_Bool );
		teamgame.ForceSet( Val, CVAR_Bool );

		// Allow the server to use their own dmflags.
		if ( sv_defaultdmflags )
		{
			ulFlags &= ~DF_WEAPONS_STAY | ~DF_ITEMS_RESPAWN | ~DF_NO_MONSTERS | ~DF_NO_CROUCH;
			ulFlags2 &= ~DF2_YES_DOUBLEAMMO;
		}

		// If invasion starts are present, load the map in invasion mode.
		if ( GenericInvasionStarts.Size( ) > 0 )
		{
			Val.Bool = true;
			invasion.ForceSet( Val, CVAR_Bool );
		}
		else
		{
			Val.Bool = false;
			invasion.ForceSet( Val, CVAR_Bool );
		}
	}

	if ( cooperative && ( GenericInvasionStarts.Size( ) == 0 ))
	{
		Val.Bool = false;
		invasion.ForceSet( Val, CVAR_Bool );
	}

	// Disallow survival mode in hubs.
	pCluster = FindClusterInfo( level.cluster );
	if (( cooperative ) &&
		( pCluster ) &&
		( pCluster->flags & CLUSTER_HUB ))
	{
		Val.Bool = false;
		survival.ForceSet( Val, CVAR_Bool );
	}

	// In a campaign, just use whatever dmflags are assigned.
	if ( CAMPAIGN_InCampaign( ) == false )
	{
		if ( dmflags != ulFlags )
			dmflags = ulFlags;
		if ( dmflags2 != ulFlags2 )
			dmflags2 = ulFlags2;
	}

	// If there aren't any pickup, blue return, or red return scripts, then use the
	// simplified, hardcoded version of the CTF or ST modes.
	if (( FBehavior::StaticCountTypedScripts( SCRIPT_Pickup ) == 0 ) ||
		( FBehavior::StaticCountTypedScripts( SCRIPT_BlueReturn ) == 0 ) ||
		( FBehavior::StaticCountTypedScripts( SCRIPT_RedReturn ) == 0 ))
	{
		if ( ctf || oneflagctf )
		{
			TEAM_SetSimpleCTFMode( true );

			while ( pActor = iterator.Next( ))
			{
				if ( pActor->IsKindOf( PClass::FindClass( "BlueFlag" )))
				{
					POS_t	Origin;

					Origin.x = pActor->x;
					Origin.y = pActor->y;
					Origin.z = pActor->z;

					TEAM_SetBlueFlagOrigin( Origin );
				}

				if ( pActor->IsKindOf( PClass::FindClass( "RedFlag" )))
				{
					POS_t	Origin;

					Origin.x = pActor->x;
					Origin.y = pActor->y;
					Origin.z = pActor->z;

					TEAM_SetRedFlagOrigin( Origin );
				}

				if ( pActor->IsKindOf( PClass::FindClass( "WhiteFlag" )))
				{
					POS_t	Origin;

					Origin.x = pActor->x;
					Origin.y = pActor->y;
					Origin.z = pActor->z;

					TEAM_SetWhiteFlagOrigin( Origin );
				}
			}
		}
		// We found skulls but no flags. Set Skulltag mode.
		else if ( skulltag )
		{
			TEAM_SetSimpleSTMode( true );

			while ( pActor = iterator.Next( ))
			{
				if ( pActor->IsKindOf( PClass::FindClass( "BlueSkull" )))
				{
					POS_t	Origin;

					// Replace this skull with skulltag mode's version of the skull.
					pNewSkull = Spawn( PClass::FindClass( "BlueSkullST" ), pActor->x, pActor->y, pActor->z, NO_REPLACE );
					if ( pNewSkull )
						pNewSkull->flags &= ~MF_DROPPED;

					Origin.x = pActor->x;
					Origin.y = pActor->y;
					Origin.z = pActor->z;

					TEAM_SetBlueSkullOrigin( Origin );
					pActor->Destroy( );
				}

				if ( pActor->IsKindOf( PClass::FindClass( "RedSkull" )))
				{
					POS_t	Origin;

					// Replace this skull with skulltag mode's version of the skull.
					pNewSkull = Spawn( PClass::FindClass( "RedSkullST" ), pActor->x, pActor->y, pActor->z, NO_REPLACE );
					if ( pNewSkull )
						pNewSkull->flags &= ~MF_DROPPED;

					Origin.x = pActor->x;
					Origin.y = pActor->y;
					Origin.z = pActor->z;

					TEAM_SetRedSkullOrigin( Origin );
					pActor->Destroy( );
				}
			}
		}
	}

	// Reset the status bar.
	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
	{
		delete ( StatusBar );

		if ( gamestate == GS_TITLELEVEL )
			StatusBar = new FBaseStatusBar( 0 );
		else if ( gameinfo.gametype == GAME_Doom )
			StatusBar = CreateDoomStatusBar( );
		else if ( gameinfo.gametype == GAME_Heretic )
			StatusBar = CreateHereticStatusBar( );
		else if ( gameinfo.gametype == GAME_Hexen )
			StatusBar = CreateHexenStatusBar( );
		else if ( gameinfo.gametype == GAME_Strife )
			StatusBar = CreateStrifeStatusBar( );
		else
			StatusBar = new FBaseStatusBar( 0 );

		StatusBar->AttachToPlayer( &players[consoleplayer] );
		StatusBar->NewGame( );
	}
}

//*****************************************************************************
//
bool GAME_ZPositionMatchesOriginal( AActor *pActor )
{
	fixed_t		Space;

	// Determine the Z position to spawn this actor in.
	if ( pActor->flags & MF_SPAWNCEILING )
		return ( pActor->z == ( pActor->Sector->ceilingplane.ZatPoint( pActor->x, pActor->y ) - pActor->height - ( pActor->SpawnPoint[2] << FRACBITS )));
	else if ( pActor->flags2 & MF2_SPAWNFLOAT )
	{
		Space = pActor->Sector->ceilingplane.ZatPoint( pActor->x, pActor->y ) - pActor->height - pActor->Sector->floorplane.ZatPoint( pActor->x, pActor->y );
		if ( Space > ( 48 * FRACUNIT ))
		{
			Space -= ( 40 * FRACUNIT );
			if (( pActor->z >= MulScale8( Space, 0 ) + ( pActor->Sector->floorplane.ZatPoint( pActor->x, pActor->y ) + 40 * FRACUNIT )) ||
				( pActor->z <= MulScale8( Space, INT_MAX ) + ( pActor->Sector->floorplane.ZatPoint( pActor->x, pActor->y ) + 40 * FRACUNIT )))
			{
				return ( true );
			}
			return ( false );
		}
		else
			return ( pActor->z == pActor->Sector->floorplane.ZatPoint( pActor->x, pActor->y ));
	}
	else if ( pActor->flags2 & MF2_FLOATBOB )
		return ( pActor->z == (( pActor->SpawnPoint[2] << FRACBITS ) + FloatBobOffsets[( pActor->FloatBobPhase + level.time ) & 63] ));
	else
		return ( pActor->z == ( pActor->Sector->floorplane.ZatPoint( pActor->x, pActor->y ) + ( pActor->SpawnPoint[2] << FRACBITS )));
}

//*****************************************************************************
//
bool GAME_DormantStatusMatchesOriginal( AActor *pActor )
{
	// For objects that have just spawned, assume that their dormant status is fine.
	if ( pActor->ObjectFlags & OF_JustSpawned )
		return ( true );

	if (( pActor->flags3 & MF3_ISMONSTER ) &&
		(( pActor->health > 0 ) || ( pActor->flags & MF_ICECORPSE )))
	{
		if ( pActor->flags2 & MF2_DORMANT )
			return !!( pActor->SpawnFlags & MTF_DORMANT );
		else
			return (( pActor->SpawnFlags & MTF_DORMANT ) == false );
	}

	if ( pActor->GetClass( )->IsDescendantOf( PClass::FindClass( "ParticleFountain" )))
	{
		if (( pActor->effects & ( pActor->health << FX_FOUNTAINSHIFT )) == false )
			return !!( pActor->SpawnFlags & MTF_DORMANT );
		else
			return (( pActor->SpawnFlags & MTF_DORMANT ) == false );
	}

	return ( true );
}

//*****************************************************************************
//
// Ugh.
void P_LoadBehavior( MapData *pMap );
void DECAL_ClearDecals( void );
void GAME_ResetMap( void )
{
	ULONG							ulIdx;
	MapData							*pMap;
	level_info_t					*pLevelInfo;
	bool							bSendSkyUpdate;
//	sector_t						*pSector;
//	fixed_t							Space;
	AActor							*pActor;
	AActor							*pNewActor;
	AActor							*pActorInfo;
	fixed_t							X;
	fixed_t							Y;
	fixed_t							Z;
	TThinkerIterator<AActor>		ActorIterator;

	// Unload decals.
	DECAL_ClearDecals( );

	// This is all we do in client mode.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return;
	}

	for ( ulIdx = 0; ulIdx < (ULONG)numlines; ulIdx++ )
	{
		// Reset the line's special.
		lines[ulIdx].special = lines[ulIdx].SavedSpecial;
		lines[ulIdx].args[0] = lines[ulIdx].SavedArgs[0];
		lines[ulIdx].args[1] = lines[ulIdx].SavedArgs[1];
		lines[ulIdx].args[2] = lines[ulIdx].SavedArgs[2];
		lines[ulIdx].args[3] = lines[ulIdx].SavedArgs[3];
		lines[ulIdx].args[4] = lines[ulIdx].SavedArgs[4];

		// Also, restore any changed textures.
		if ( lines[ulIdx].ulTexChangeFlags != 0 )
		{
			if ( lines[ulIdx].sidenum[0] != 0xffff )
			{
				sides[lines[ulIdx].sidenum[0]].toptexture = sides[lines[ulIdx].sidenum[0]].SavedTopTexture;
				sides[lines[ulIdx].sidenum[0]].midtexture = sides[lines[ulIdx].sidenum[0]].SavedMidTexture;
				sides[lines[ulIdx].sidenum[0]].bottomtexture = sides[lines[ulIdx].sidenum[0]].SavedBottomTexture;
			}

			if ( lines[ulIdx].sidenum[1] != NO_SIDE )
			{
				sides[lines[ulIdx].sidenum[1]].toptexture = sides[lines[ulIdx].sidenum[1]].SavedTopTexture;
				sides[lines[ulIdx].sidenum[1]].midtexture = sides[lines[ulIdx].sidenum[1]].SavedMidTexture;
				sides[lines[ulIdx].sidenum[1]].bottomtexture = sides[lines[ulIdx].sidenum[1]].SavedBottomTexture;
			}

			// If we're the server, tell clients about this texture change.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetLineTexture( ulIdx );

			// Mark the texture as no being changed.
			lines[ulIdx].ulTexChangeFlags = 0;
		}

		// Restore the line's alpha if it changed.
		if ( lines[ulIdx].alpha != lines[ulIdx].SavedAlpha )
		{
			lines[ulIdx].alpha = lines[ulIdx].SavedAlpha;

			// If we're the server, tell clients about this alpha change.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetLineAlpha( ulIdx );
		}

		// Restore the line's blocking status.
		if ( lines[ulIdx].flags != lines[ulIdx].SavedFlags )
		{
			lines[ulIdx].flags = lines[ulIdx].SavedFlags;

			// If we're the server, tell clients about this blocking change.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetLineBlocking( ulIdx );
		}
	}

	// Restore sector heights, flat changes, light changes, etc.
	for ( ulIdx = 0; ulIdx < (ULONG)numsectors; ulIdx++ )
	{
		if ( sectors[ulIdx].bCeilingHeightChange )
		{
			sectors[ulIdx].ceilingplane = sectors[ulIdx].SavedCeilingPlane;
			sectors[ulIdx].ceilingtexz = sectors[ulIdx].SavedCeilingTexZ;
			sectors[ulIdx].bCeilingHeightChange = false;

			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetSectorCeilingPlane( ulIdx );
		}

		if ( sectors[ulIdx].bFloorHeightChange )
		{
			sectors[ulIdx].floorplane = sectors[ulIdx].SavedFloorPlane;
			sectors[ulIdx].floortexz = sectors[ulIdx].SavedFloorTexZ;
			sectors[ulIdx].bFloorHeightChange = false;

			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetSectorFloorPlane( ulIdx );
		}

		if ( sectors[ulIdx].bFlatChange )
		{
			sectors[ulIdx].floorpic = sectors[ulIdx].SavedFloorPic;
			sectors[ulIdx].ceilingpic = sectors[ulIdx].SavedCeilingPic;
			sectors[ulIdx].bFlatChange = false;

			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetSectorFlat( ulIdx );
		}

		if ( sectors[ulIdx].bLightChange )
		{
			sectors[ulIdx].lightlevel = sectors[ulIdx].SavedLightLevel;
			sectors[ulIdx].bLightChange = false;

			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetSectorLightLevel( ulIdx );
		}

		if ( sectors[ulIdx].ColorMap != sectors[ulIdx].SavedColorMap )
		{
			sectors[ulIdx].ColorMap = sectors[ulIdx].SavedColorMap;

			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			{
				SERVERCOMMANDS_SetSectorColor( ulIdx );
				SERVERCOMMANDS_SetSectorFade( ulIdx );
			}
		}

		if ( sectors[ulIdx].ColorMap != sectors[ulIdx].SavedColorMap )
		{
			sectors[ulIdx].ColorMap = sectors[ulIdx].SavedColorMap;

			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			{
				SERVERCOMMANDS_SetSectorColor( ulIdx );
				SERVERCOMMANDS_SetSectorFade( ulIdx );
			}
		}

		if (( sectors[ulIdx].SavedFloorXOffset != sectors[ulIdx].floor_xoffs ) ||
			( sectors[ulIdx].SavedFloorYOffset != sectors[ulIdx].floor_yoffs ) ||
			( sectors[ulIdx].SavedCeilingXOffset != sectors[ulIdx].ceiling_xoffs ) ||
			( sectors[ulIdx].SavedCeilingYOffset != sectors[ulIdx].ceiling_yoffs ))
		{
			sectors[ulIdx].floor_xoffs = sectors[ulIdx].SavedFloorXOffset;
			sectors[ulIdx].floor_yoffs = sectors[ulIdx].SavedFloorYOffset;
			sectors[ulIdx].ceiling_xoffs = sectors[ulIdx].SavedCeilingXOffset;
			sectors[ulIdx].ceiling_yoffs = sectors[ulIdx].SavedCeilingYOffset;

			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetSectorPanning( ulIdx );
		}

		if (( sectors[ulIdx].SavedFloorXScale != sectors[ulIdx].floor_xscale ) ||
			( sectors[ulIdx].SavedFloorYScale != sectors[ulIdx].floor_yscale ) ||
			( sectors[ulIdx].SavedCeilingXScale != sectors[ulIdx].ceiling_xscale ) ||
			( sectors[ulIdx].SavedCeilingYScale != sectors[ulIdx].ceiling_yscale ))
		{
			sectors[ulIdx].floor_xscale = sectors[ulIdx].SavedFloorXScale;
			sectors[ulIdx].floor_yscale = sectors[ulIdx].SavedFloorYScale;
			sectors[ulIdx].ceiling_xscale = sectors[ulIdx].SavedCeilingXScale;
			sectors[ulIdx].ceiling_yscale = sectors[ulIdx].SavedCeilingYScale;

			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetSectorScale( ulIdx );
		}

		if (( sectors[ulIdx].SavedFloorAngle != sectors[ulIdx].floor_angle ) ||
			( sectors[ulIdx].SavedCeilingAngle != sectors[ulIdx].ceiling_angle ))
		{
			sectors[ulIdx].floor_angle = sectors[ulIdx].SavedFloorAngle;
			sectors[ulIdx].ceiling_angle = sectors[ulIdx].SavedCeilingAngle;

			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetSectorRotation( ulIdx );
		}

		if (( sectors[ulIdx].SavedBaseFloorAngle != sectors[ulIdx].base_floor_angle ) ||
			( sectors[ulIdx].SavedBaseFloorYOffset != sectors[ulIdx].base_floor_yoffs ) ||
			( sectors[ulIdx].SavedBaseCeilingAngle != sectors[ulIdx].base_ceiling_angle ) ||
			( sectors[ulIdx].SavedBaseCeilingYOffset != sectors[ulIdx].base_ceiling_yoffs ))
		{
			sectors[ulIdx].base_floor_angle = sectors[ulIdx].SavedBaseFloorAngle;
			sectors[ulIdx].base_floor_yoffs = sectors[ulIdx].SavedBaseFloorYOffset;
			sectors[ulIdx].base_ceiling_angle = sectors[ulIdx].SavedBaseCeilingAngle;
			sectors[ulIdx].base_ceiling_yoffs = sectors[ulIdx].SavedBaseCeilingYOffset;

			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetSectorAngleYOffset( ulIdx );
		}

		if (( sectors[ulIdx].SavedFriction != sectors[ulIdx].friction ) ||
			( sectors[ulIdx].SavedMoveFactor != sectors[ulIdx].movefactor ))
		{
			sectors[ulIdx].friction = sectors[ulIdx].SavedFriction;
			sectors[ulIdx].movefactor = sectors[ulIdx].SavedMoveFactor;

			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetSectorFriction( ulIdx );
		}

		if (( sectors[ulIdx].SavedSpecial != sectors[ulIdx].special ) ||
			( sectors[ulIdx].SavedDamage != sectors[ulIdx].damage ) ||
			( sectors[ulIdx].SavedMOD != sectors[ulIdx].mod ))
		{
			sectors[ulIdx].special = sectors[ulIdx].SavedSpecial;
			sectors[ulIdx].damage = sectors[ulIdx].SavedDamage;
			sectors[ulIdx].mod = sectors[ulIdx].SavedMOD;

			// No client update necessary here.
		}

		if (( sectors[ulIdx].SavedCeilingReflect != sectors[ulIdx].ceiling_reflect ) ||
			( sectors[ulIdx].SavedFloorReflect != sectors[ulIdx].floor_reflect ))
		{
			sectors[ulIdx].ceiling_reflect = sectors[ulIdx].SavedCeilingReflect;
			sectors[ulIdx].floor_reflect = sectors[ulIdx].SavedFloorReflect;

			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetSectorReflection( ulIdx );
		}
	}

	// Reset the sky properties of the map.
	pLevelInfo = level.info;//FindLevelInfo( level.mapname );
	if ( pLevelInfo )
	{
		bSendSkyUpdate = false;
		if (( stricmp( level.skypic1, pLevelInfo->skypic1 ) != 0 ) ||
			( stricmp( level.skypic2, pLevelInfo->skypic2 ) != 0 ))
		{
			bSendSkyUpdate = true;
		}

		strncpy( level.skypic1, pLevelInfo->skypic1, 8 );
		strncpy( level.skypic2, pLevelInfo->skypic2, 8 );
		if ( level.skypic2[0] == 0 )
			strncpy( level.skypic2, level.skypic1, 8 );

		sky1texture = TexMan.GetTexture( level.skypic1, FTexture::TEX_Wall, FTextureManager::TEXMAN_Overridable );
		sky2texture = TexMan.GetTexture( level.skypic2, FTexture::TEX_Wall, FTextureManager::TEXMAN_Overridable );

		R_InitSkyMap( );

		// If we're the server, tell clients to update their sky.
		if (( bSendSkyUpdate ) && ( NETWORK_GetState( ) == NETSTATE_SERVER ))
			SERVERCOMMANDS_SetMapSky( );
	}

	// Reset the number of monsters killed,  items picked up, and found secrets on the level.
	level.killed_monsters = 0;
	level.found_items = 0;
	level.found_secrets = 0;
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		SERVERCOMMANDS_SetMapNumKilledMonsters( );
		SERVERCOMMANDS_SetMapNumFoundItems( );
		SERVERCOMMANDS_SetMapNumFoundSecrets( );
	}

	// Restart the map music.
	S_ChangeMusic( level.music, level.musicorder );
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		SERVER_SetMapMusic( level.music );
		SERVERCOMMANDS_SetMapMusic( SERVER_GetMapMusic( ));
	}

	// Reload the actors on this level.
	while (( pActor = ActorIterator.Next( )) != NULL )
	{
		// Don't reload players.
		if ( pActor->IsKindOf( RUNTIME_CLASS( APlayerPawn )))
			continue;

		// If this object belongs to someone's inventory, and it originally spawned on the
		// level, respawn the item in its original location, but don't take it out of the
		// player's inventory.
		if (( pActor->IsKindOf( RUNTIME_CLASS( AInventory ))) && 
			( static_cast<AInventory *>( pActor )->Owner ))
		{
			if ( pActor->ulSTFlags & STFL_LEVELSPAWNED )
			{
				// Get the default information for this actor, so we can determine how to
				// respawn it.
				pActorInfo = pActor->GetDefault( );

				// Spawn the new actor.
				X = pActor->SpawnPoint[0] << FRACBITS;
				Y = pActor->SpawnPoint[1] << FRACBITS;

				// Determine the Z point based on its flags.
				if ( pActorInfo->flags & MF_SPAWNCEILING )
					Z = ONCEILINGZ;
				else if ( pActorInfo->flags2 & MF2_SPAWNFLOAT )
					Z = FLOATRANDZ;
				else if ( pActorInfo->flags2 & MF2_FLOATBOB )
					Z = pActor->SpawnPoint[2] << FRACBITS;
				else
					Z = ONFLOORZ;

				pNewActor = Spawn( RUNTIME_TYPE( pActor ), X, Y, Z, NO_REPLACE );

				// Adjust the Z position after it's spawned.
				if ( Z == ONFLOORZ )
					pNewActor->z += pActor->SpawnPoint[2] << FRACBITS;
				else if ( Z == ONCEILINGZ )
					pNewActor->z -= pActor->SpawnPoint[2] << FRACBITS;
/*
				pSector = R_PointInSubsector( X, Y )->sector;
				pNewActor->z = pSector->floorplane.ZatPoint( X, Y );

				// Determine the Z position to spawn this actor in.
				if ( pActorInfo->flags & MF_SPAWNCEILING )
					pNewActor->z = pNewActor->ceilingz - pNewActor->height - ( pActor->SpawnPoint[2] << FRACBITS );
				else if ( pActorInfo->flags2 & MF2_SPAWNFLOAT )
				{
					Space = pNewActor->ceilingz - pNewActor->height - pNewActor->floorz;
					if ( Space > 48*FRACUNIT )
					{
						Space -= 40*FRACUNIT;
						pNewActor->z = (( Space * M_Random( )) >>8 ) + pNewActor->floorz + ( 40 * FRACUNIT );
					}
					else
					{
						pNewActor->z = pNewActor->floorz;
					}
				}
				else if ( pActorInfo->flags2 & MF2_FLOATBOB )
					pNewActor->z = pActor->SpawnPoint[2] << FRACBITS;
*/
				// Inherit attributes from the old actor.
				pNewActor->SpawnPoint[0] = pActor->SpawnPoint[0];
				pNewActor->SpawnPoint[1] = pActor->SpawnPoint[1];
				pNewActor->SpawnPoint[2] = pActor->SpawnPoint[2];
				pNewActor->SpawnAngle = pActor->SpawnAngle;
				pNewActor->SpawnFlags = pActor->SpawnFlags;
				pNewActor->angle = ANG45 * ( pActor->SpawnAngle / 45 );
				pNewActor->tid = pActor->tid;
				pNewActor->special = pActor->SavedSpecial;
				pNewActor->SavedSpecial = pActor->SavedSpecial;
				pNewActor->args[0] = pActor->args[0];
				pNewActor->args[1] = pActor->args[1];
				pNewActor->args[2] = pActor->args[2];
				pNewActor->args[3] = pActor->args[3];
				pNewActor->args[4] = pActor->args[4];
				pNewActor->AddToHash( );

				pNewActor->ulSTFlags |= STFL_LEVELSPAWNED;

				// Handle the spawn flags of the item.
				pNewActor->HandleSpawnFlags( );

				// Spawn the new actor.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_SpawnThing( pNewActor );
			}

			continue;
		}

		// Destroy any actor not present when the map loaded.
		if (( pActor->ulSTFlags & STFL_LEVELSPAWNED ) == false )
		{
			// If this is a monster, decrement the total number of monsters on the level.
			if ( pActor->CountsAsKill( ))
				level.total_monsters--;

			// If this is an item, decrement the total number of item on the level.
			if ( pActor->flags & MF_COUNTITEM )
				level.total_items--;

			// If we're the server, tell clients to delete the actor.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_DestroyThing( pActor );

			// [BB] Destroying lights here will result in crashes after the countdown
			// of a duel ends in skirmish. Has to be investigated.
			if( !pActor->IsKindOf( RUNTIME_CLASS( ADynamicLight ) ) )
				pActor->Destroy( );
			continue;
		}

		// Get the default information for this actor, so we can determine how to
		// respawn it.
		pActorInfo = pActor->GetDefault( );

		// This item appears to be untouched; no need to respawn it.
		if ((( pActor->ulSTFlags & STFL_POSITIONCHANGED ) == false ) &&
			( pActor->InSpawnState( )) &&
			( GAME_DormantStatusMatchesOriginal( pActor )) &&
			( pActor->health == pActorInfo->health ))
		{
			if ( pActor->special != pActor->SavedSpecial )
				pActor->special = pActor->SavedSpecial;

			continue;
		}

		// Spawn the new actor.
		X = pActor->SpawnPoint[0] << FRACBITS;
		Y = pActor->SpawnPoint[1] << FRACBITS;

		// Determine the Z point based on its flags.
		if ( pActorInfo->flags & MF_SPAWNCEILING )
			Z = ONCEILINGZ;
		else if ( pActorInfo->flags2 & MF2_SPAWNFLOAT )
			Z = FLOATRANDZ;
		else if ( pActorInfo->flags2 & MF2_FLOATBOB )
			Z = pActor->SpawnPoint[2] << FRACBITS;
		else
			Z = ONFLOORZ;

		pNewActor = Spawn( RUNTIME_TYPE( pActor ), X, Y, Z, NO_REPLACE );

		// [BB] This if fixes a server crash, if ambient sounds are currently playing
		// at the end of a countdown (DUEL start countdown for example).
		if( pNewActor != NULL ){
			// Adjust the Z position after it's spawned.
			if ( Z == ONFLOORZ )
				pNewActor->z += pActor->SpawnPoint[2] << FRACBITS;
			else if ( Z == ONCEILINGZ )
				pNewActor->z -= pActor->SpawnPoint[2] << FRACBITS;
/*
			pSector = R_PointInSubsector( X, Y )->sector;
			pNewActor->z = pSector->floorplane.ZatPoint( X, Y );

			// Determine the Z position to spawn this actor in.
			if ( pActorInfo->flags & MF_SPAWNCEILING )
				pNewActor->z = pNewActor->ceilingz - pNewActor->height - ( pActor->SpawnPoint[2] << FRACBITS );
			else if ( pActorInfo->flags2 & MF2_SPAWNFLOAT )
			{
				Space = pNewActor->ceilingz - pNewActor->height - pNewActor->floorz;
				if ( Space > 48*FRACUNIT )
				{
					Space -= 40*FRACUNIT;
					pNewActor->z = (( Space * M_Random( )) >>8 ) + pNewActor->floorz + ( 40 * FRACUNIT );
				}
				else
				{
					pNewActor->z = pNewActor->floorz;
				}
			}
			else if ( pActorInfo->flags2 & MF2_FLOATBOB )
				pNewActor->z = pActor->SpawnPoint[2] << FRACBITS;
*/
			// Inherit attributes from the old actor.
			pNewActor->SpawnPoint[0] = pActor->SpawnPoint[0];
			pNewActor->SpawnPoint[1] = pActor->SpawnPoint[1];
			pNewActor->SpawnPoint[2] = pActor->SpawnPoint[2];
			pNewActor->SpawnAngle = pActor->SpawnAngle;
			pNewActor->SpawnFlags = pActor->SpawnFlags;
			pNewActor->angle = ANG45 * ( pActor->SpawnAngle / 45 );
			pNewActor->tid = pActor->tid;
			pNewActor->special = pActor->SavedSpecial;
			pNewActor->SavedSpecial = pActor->SavedSpecial;
			pNewActor->args[0] = pActor->args[0];
			pNewActor->args[1] = pActor->args[1];
			pNewActor->args[2] = pActor->args[2];
			pNewActor->args[3] = pActor->args[3];
			pNewActor->args[4] = pActor->args[4];
			pNewActor->AddToHash( );

			// Just do this stuff for monsters.
			if ( pActor->flags & MF_COUNTKILL )
			{
				if ( pActor->SpawnFlags & MTF_AMBUSH )
					pNewActor->flags |= MF_AMBUSH;

				pNewActor->reactiontime = 18;

				pNewActor->TIDtoHate = pActor->TIDtoHate;
				pNewActor->LastLook = pActor->LastLook;
				pNewActor->flags3 |= pActor->flags3 & MF3_HUNTPLAYERS;
				pNewActor->flags4 |= pActor->flags4 & MF4_NOHATEPLAYERS;
			}

			pNewActor->flags &= ~MF_DROPPED;
			pNewActor->ulSTFlags |= STFL_LEVELSPAWNED;

			// Handle the spawn flags of the item.
			pNewActor->HandleSpawnFlags( );

			// If the old actor counts as a kill, remove it from the total monster count
			// since it's being deleted.
			if ( pActor->CountsAsKill( ))
				level.total_monsters--;

			// If the old actor counts as an item, remove it from the total item count
			// since it's being deleted.
			if ( pActor->flags & MF_COUNTITEM )
				level.total_items--;

			// Remove the old actor.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_DestroyThing( pActor );

			pActor->Destroy( );

			// Tell clients to spawn the new actor.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SpawnThing( pNewActor );
		}
	}

	// If we're the server, tell clients the new number of total items/monsters.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		SERVERCOMMANDS_SetMapNumTotalMonsters( );
		SERVERCOMMANDS_SetMapNumTotalItems( );
	}

	// Also, delete all floors, plats, etc. that are in progress.
	for ( ulIdx = 0; ulIdx < (ULONG)numsectors; ulIdx++ )
	{
		if ( sectors[ulIdx].ceilingdata )
		{
			// Stop the sound sequence (if any) associated with this sector.
			SN_StopSequence( &sectors[ulIdx] );

			sectors[ulIdx].ceilingdata->Destroy( );
			sectors[ulIdx].ceilingdata = NULL;
		}
		if ( sectors[ulIdx].floordata )
		{
			// Stop the sound sequence (if any) associated with this sector.
			SN_StopSequence( &sectors[ulIdx] );

			sectors[ulIdx].floordata->Destroy( );
			sectors[ulIdx].floordata = NULL;
		}
	}

	// If we're the server, tell clients to delete all their ceiling/floor movers.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_DestroyAllSectorMovers( );

	// Unload the ACS scripts so we can reload them.
	FBehavior::StaticUnloadModules( );
	delete ( DACSThinker::ActiveThinker );
	DACSThinker::ActiveThinker = NULL;

	// Open the current map and load its BEHAVIOR lump.
	pMap = P_OpenMapData( level.mapname );
	if ( pMap == NULL )
		I_Error( "GAME_ResetMap: Unable to open map '%s'\n", level.mapname );
	else if ( pMap->HasBehavior )
		P_LoadBehavior( pMap );

	// Run any default scripts that needed to be run.
	FBehavior::StaticLoadDefaultModules( );

	// Restart running any open scripts on this map, since we just destroyed them all!
	FBehavior::StaticStartTypedScripts( SCRIPT_Open, NULL, false );

	delete ( pMap );
}

//*****************************************************************************
//
void GAME_SpawnTerminatorArtifact( void )
{
	ULONG		ulIdx;
	AActor		*pTerminatorBall;
	mapthing2_t	*pSpot;

	// [RC] Spawn it at a Terminator start, or a deathmatch spot
	if(TerminatorStarts.Size() > 0) 	// Use the terminator starts, if the mapper added them
		pSpot = SelectRandomTerminatorSpot(0, TerminatorStarts.Size());
	else if(deathmatchstarts.Size() > 0) // Or use a deathmatch start, if one exists
		pSpot = SelectRandomDeathmatchSpot(0, deathmatchstarts.Size());
	else // Or return! Be that way!
		return;

	// Since G_CheckSpot() clears players' MF_SOLID flag for whatever reason, we have
	// to restore it manually here.
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if (( playeringame[ulIdx] ) && ( players[ulIdx].mo ))
			players[ulIdx].mo->flags |= MF_SOLID;
	}

	if ( pSpot == NULL )
		return;

	// Spawn the ball.
	pTerminatorBall = Spawn( PClass::FindClass( "Terminator" ), pSpot->x << FRACBITS, pSpot->y << FRACBITS, ONFLOORZ, NO_REPLACE );

	// If we're the server, tell clients to spawn the new ball.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SpawnThing( pTerminatorBall );
}

//*****************************************************************************
//
void GAME_SpawnPossessionArtifact( void )
{
	ULONG		ulIdx;
	AActor		*pPossessionStone;
	mapthing2_t	*pSpot;

	// [RC] Spawn it at a Possession start, or a deathmatch spot
	if(PossessionStarts.Size() > 0) 	// Did the mapper place possession starts? Use those
		pSpot = SelectRandomPossessionSpot(0, PossessionStarts.Size());
	else if(deathmatchstarts.Size() > 0) // Or use a deathmatch start, if one exists
		pSpot = SelectRandomDeathmatchSpot(0, deathmatchstarts.Size());
	else // Or return! Be that way!
		return;

	// Since G_CheckSpot() clears players' MF_SOLID flag for whatever reason, we have
	// to restore it manually here.
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if (( playeringame[ulIdx] ) && ( players[ulIdx].mo ))
			players[ulIdx].mo->flags |= MF_SOLID;
	}

	if ( pSpot == NULL )
		return;

	// Spawn the ball.
	pPossessionStone = Spawn( PClass::FindClass( "PossessionStone" ), pSpot->x << FRACBITS, pSpot->y << FRACBITS, ONFLOORZ, NO_REPLACE );

	// If we're the server, tell clients to spawn the possession stone.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SpawnThing( pPossessionStone );
}

//*****************************************************************************
//
void GAME_SetEndLevelDelay( ULONG ulTicks )
{
	g_ulEndLevelDelay = ulTicks;

	// Tell the clients about the end level delay.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SetGameEndLevelDelay( g_ulEndLevelDelay );
}

//*****************************************************************************
//
ULONG GAME_GetEndLevelDelay( void )
{
	return ( g_ulEndLevelDelay );
}

//*****************************************************************************
//
void GAME_SetLevelIntroTicks( USHORT usTicks )
{
	g_ulLevelIntroTicks = usTicks;
}

//*****************************************************************************
//
USHORT GAME_GetLevelIntroTicks( void )
{
	return ( g_ulLevelIntroTicks );
}

//*****************************************************************************
//
LONG GAME_CountLivingPlayers( void )
{
	ULONG	ulIdx;
	ULONG	ulNumLivingPlayers;

	ulNumLivingPlayers = 0;
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if (( playeringame[ulIdx] ) && ( players[ulIdx].bSpectating == false ) && ( players[ulIdx].health > 0 ))
			ulNumLivingPlayers++;
	}

	return ( ulNumLivingPlayers );
}

void G_ScreenShot (char *filename)
{
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		return;

	shotfile = filename;
	gameaction = ga_screenshot;
}





//
// G_InitFromSavegame
// Can be called by the startup code or the menu task.
//
void G_LoadGame (const char* name)
{
	if (name != NULL)
	{
		savename = name;
		gameaction = ga_loadgame;
	}
}

static bool CheckSingleWad (char *name, bool &printRequires, bool printwarn)
{
	if (name == NULL)
	{
		return true;
	}
	if (Wads.CheckIfWadLoaded (name) < 0)
	{
		if (printwarn)
		{
			if (!printRequires)
			{
				Printf ("This savegame needs these wads:\n%s", name);
			}
			else
			{
				Printf (", %s", name);
			}
		}
		printRequires = true;
		delete[] name;
		return false;
	}
	delete[] name;
	return true;
}

// Return false if not all the needed wads have been loaded.
bool G_CheckSaveGameWads (PNGHandle *png, bool printwarn)
{
	char *text;
	bool printRequires = false;

	text = M_GetPNGText (png, "Game WAD");
	CheckSingleWad (text, printRequires, printwarn);
	text = M_GetPNGText (png, "Map WAD");
	CheckSingleWad (text, printRequires, printwarn);

	if (printRequires)
	{
		if (printwarn)
		{
			Printf ("\n");
		}
		return false;
	}

	return true;
}

static void WriteVars (FILE *file, SDWORD *vars, size_t count, DWORD id)
{
	size_t i, j;

	for (i = 0; i < count; ++i)
	{
		if (vars[i] != 0)
			break;
	}
	if (i < count)
	{
		// Find last non-zero var. Anything beyond the last stored variable
		// will be zeroed at load time.
		for (j = count-1; j > i; --j)
		{
			if (vars[j] != 0)
				break;
		}
		FPNGChunkArchive arc (file, id);
		for (i = 0; i <= j; ++i)
		{
			DWORD var = vars[i];
			arc << var;
		}
	}
}

static void ReadVars (PNGHandle *png, SDWORD *vars, size_t count, DWORD id)
{
	size_t len = M_FindPNGChunk (png, id);
	size_t used = 0;

	if (len != 0)
	{
		DWORD var;
		size_t i;
		FPNGChunkArchive arc (png->File->GetFile(), id, len);
		used = len / 4;

		for (i = 0; i < used; ++i)
		{
			arc << var;
			vars[i] = var;
		}
		png->File->ResetFilePtr();
	}
	if (used < count)
	{
		memset (&vars[used], 0, (count-used)*4);
	}
}

static void WriteArrayVars (FILE *file, FWorldGlobalArray *vars, unsigned int count, DWORD id)
{
	unsigned int i, j;

	// Find the first non-empty array.
	for (i = 0; i < count; ++i)
	{
		if (vars[i].CountUsed() != 0)
			break;
	}
	if (i < count)
	{
		// Find last non-empty array. Anything beyond the last stored array
		// will be emptied at load time.
		for (j = count-1; j > i; --j)
		{
			if (vars[j].CountUsed() != 0)
				break;
		}
		FPNGChunkArchive arc (file, id);
		arc.WriteCount (i);
		arc.WriteCount (j);
		for (; i <= j; ++i)
		{
			const SDWORD *key;
			SDWORD *val;

			arc.WriteCount (vars[i].CountUsed());
			FWorldGlobalArrayIterator it(vars[i]);
			while (it.NextPair (key, val))
			{
				arc.WriteCount (*key);
				arc.WriteCount (*val);
			}
		}
	}
}

static void ReadArrayVars (PNGHandle *png, FWorldGlobalArray *vars, size_t count, DWORD id)
{
	size_t len = M_FindPNGChunk (png, id);
	unsigned int i, k;

	for (i = 0; i < count; ++i)
	{
		vars[i].Clear ();
	}

	if (len != 0)
	{
		DWORD max, size;
		FPNGChunkArchive arc (png->File->GetFile(), id, len);

		i = arc.ReadCount ();
		max = arc.ReadCount ();

		for (; i <= max; ++i)
		{
			size = arc.ReadCount ();
			for (k = 0; k < size; ++k)
			{
				SDWORD key, val;
				key = arc.ReadCount();
				val = arc.ReadCount();
				vars[i].Insert (key, val);
			}
		}
		png->File->ResetFilePtr();
	}
}

void G_DoLoadGame ()
{
	char sigcheck[16];
	char *text = NULL;
	char *map;

	gameaction = ga_nothing;

	FILE *stdfile = fopen (savename.GetChars(), "rb");
	if (stdfile == NULL)
	{
		Printf ("Could not read savegame '%s'\n", savename.GetChars());
		return;
	}

	PNGHandle *png = M_VerifyPNG (stdfile);
	if (png == NULL)
	{
		fclose (stdfile);
		Printf ("'%s' is not a valid (PNG) savegame\n", savename.GetChars());
		return;
	}

	SaveVersion = 0;

	// Check whether this savegame actually has been created by a compatible engine.
	// Since there are ZDoom derivates using the exact same savegame format but
	// with mutual incompatibilities this check simplifies things significantly.
	char *engine = M_GetPNGText (png, "Engine");
	if (engine == NULL || 0 != strcmp (engine, GAMESIG))
	{
		// Make a special case for the message printed for old savegames that don't
		// have this information.
		if (engine == NULL)
		{
			Printf ("Savegame is from an incompatible version\n");
		}
		else
		{
			Printf ("Savegame is from another ZDoom-based engine: %s\n", engine);
			delete[] engine;
		}
		delete png;
		fclose (stdfile);
		return;
	}
	if (engine != NULL)
	{
		delete[] engine;
	}

	if (!M_GetPNGText (png, "ZDoom Save Version", sigcheck, 16) ||
		0 != strncmp (sigcheck, SAVESIG, 9) ||		// ZDOOMSAVE is the first 9 chars
		(SaveVersion = atoi (sigcheck+9)) < MINSAVEVER)
	{
		Printf ("Savegame is from an incompatible version\n");
		delete png;
		fclose (stdfile);
		return;
	}

	if (!G_CheckSaveGameWads (png, true))
	{
		fclose (stdfile);
		return;
	}

	map = M_GetPNGText (png, "Current Map");
	if (map == NULL)
	{
		Printf ("Savegame is missing the current map\n");
		fclose (stdfile);
		return;
	}

	// Read intermission data for hubs
	G_ReadHubInfo(png);

	text = M_GetPNGText (png, "Important CVARs");
	if (text != NULL)
	{
		BYTE *vars_p = (BYTE *)text;
		C_ReadCVars (&vars_p);
		delete[] text;
	}

	// dearchive all the modifications
	if (M_FindPNGChunk (png, MAKE_ID('p','t','I','c')) == 8)
	{
		DWORD time[2];
		fread (&time, 8, 1, stdfile);
		time[0] = BigLong((unsigned int)time[0]);
		time[1] = BigLong((unsigned int)time[1]);
		level.time = Scale (time[1], TICRATE, time[0]);
	}
	else
	{ // No ptIc chunk so we don't know how long the user was playing
		level.time = 0;
	}

	G_ReadSnapshots (png);
	FRandom::StaticReadRNGState (png);
	P_ReadACSDefereds (png);

	// load a base level
	savegamerestore = true;		// Use the player actors in the savegame
	bool demoplaybacksave = demoplayback;
	G_InitNew (map, false);
	demoplayback = demoplaybacksave;
	delete[] map;
	savegamerestore = false;

	ReadVars (png, ACS_WorldVars, NUM_WORLDVARS, MAKE_ID('w','v','A','r'));
	ReadVars (png, ACS_GlobalVars, NUM_GLOBALVARS, MAKE_ID('g','v','A','r'));
	ReadArrayVars (png, ACS_WorldArrays, NUM_WORLDVARS, MAKE_ID('w','a','R','r'));
	ReadArrayVars (png, ACS_GlobalArrays, NUM_GLOBALVARS, MAKE_ID('g','a','R','r'));

	// [BC] Read the invasion state, etc.
	if ( invasion )
		INVASION_ReadSaveInfo( png );

	NextSkill = -1;
	if (M_FindPNGChunk (png, MAKE_ID('s','n','X','t')) == 1)
	{
		BYTE next;
		fread (&next, 1, 1, stdfile);
		NextSkill = next;
	}

	if (level.info->snapshot != NULL)
	{
		delete level.info->snapshot;
		level.info->snapshot = NULL;
	}

	BackupSaveName = savename;

	delete png;
	fclose (stdfile);
}


//
// G_SaveGame
// Called by the menu task.
// Description is a 24 byte text string
//
void G_SaveGame (const char *filename, const char *description)
{
	savegamefile = filename;
	strcpy (savedescription, description);
	sendsave = true;
}

FString G_BuildSaveName (const char *prefix, int slot)
{
	FString name;
	const char *leader;
	const char *slash = "";

	if (NULL != (leader = Args.CheckValue ("-savedir")))
	{
		size_t len = strlen (leader);
		if (leader[len-1] != '\\' && leader[len-1] != '/')
		{
			slash = "/";
		}
	}
#ifndef unix
	else if (Args.CheckParm ("-cdrom"))
	{
		leader = "c:/zdoomdat/";
	}
	else
	{
		leader = progdir;
	}
#else
	else
	{
		leader = "";
	}
#endif
	if (slot < 0)
	{
		name.Format ("%s%s%s", leader, slash, prefix);
	}
	else
	{
		name.Format ("%s%s%s%d.zds", leader, slash, prefix, slot);
	}
#ifdef unix
	if (leader[0] == 0)
	{
		name = GetUserFile (name);
	}
#endif
	return name;
}

CVAR (Int, autosavenum, 0, CVAR_NOSET|CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Int, disableautosave, 0, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CUSTOM_CVAR (Int, autosavecount, 4, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (self < 0)
		self = 0;
	if (self > 20)
		self = 20;
}

extern void P_CalcHeight (player_t *);

void G_DoAutoSave ()
{
	// Keep up to four autosaves at a time
	UCVarValue num;
	const char *readableTime;
	int count = autosavecount != 0 ? autosavecount : 1;
	
	num.Int = (autosavenum + 1) % count;
	autosavenum.ForceSet (num, CVAR_Int);

	savegamefile = G_BuildSaveName ("auto", num.Int);

	readableTime = myasctime ();
	strcpy (savedescription, "Autosave ");
	strncpy (savedescription+9, readableTime+4, 12);
	savedescription[9+12] = 0;

	G_DoSaveGame (false);
}


static void PutSaveWads (FILE *file)
{
	const char *name;

	// Name of IWAD
	name = Wads.GetWadName (FWadCollection::IWAD_FILENUM);
	M_AppendPNGText (file, "Game WAD", name);

	// Name of wad the map resides in
	if (Wads.GetLumpFile (level.lumpnum) > 1)
	{
		name = Wads.GetWadName (Wads.GetLumpFile (level.lumpnum));
		M_AppendPNGText (file, "Map WAD", name);
	}
}

static void PutSaveComment (FILE *file)
{
	char comment[256];
	const char *readableTime;
	WORD len;
	int levelTime;

	// Get the current date and time
	readableTime = myasctime ();

	strncpy (comment, readableTime, 10);
	strncpy (comment+10, readableTime+19, 5);
	strncpy (comment+15, readableTime+10, 9);
	comment[24] = 0;

	M_AppendPNGText (file, "Creation Time", comment);

	// Get level name
	//strcpy (comment, level.level_name);
	sprintf(comment, "%s - %s", level.mapname, level.level_name);
	len = (WORD)strlen (comment);
	comment[len] = '\n';

	// Append elapsed time
	levelTime = level.time / TICRATE;
	sprintf (comment+len+1, "time: %02d:%02d:%02d",
		levelTime/3600, (levelTime%3600)/60, levelTime%60);
	comment[len+16] = 0;

	// Write out the comment
	M_AppendPNGText (file, "Comment", comment);
}

static void PutSavePic (FILE *file, int width, int height)
{
	if (width <= 0 || height <= 0 || !storesavepic)
	{
		M_CreateDummyPNG (file);
	}
	else
	{
		DCanvas *pic = new DSimpleCanvas (width, height);
		PalEntry palette[256];

		// Take a snapshot of the player's view
		pic->Lock ();
		P_CheckPlayerSprites();
		R_RenderViewToCanvas (players[consoleplayer].mo, pic, 0, 0, width, height);
		screen->GetFlashedPalette (palette);
		M_CreatePNG (file, pic, palette);
		pic->Unlock ();
		delete pic;
	}
}

void G_DoSaveGame (bool okForQuicksave)
{
	if (demoplayback)
	{
		savegamefile = G_BuildSaveName ("demosave.zds", -1);
	}

	insave = true;
	G_SnapshotLevel ();

	FILE *stdfile = fopen (savegamefile.GetChars(), "wb");

	if (stdfile == NULL)
	{
		Printf ("Could not create savegame '%s'\n", savegamefile.GetChars());
		savegamefile = "";
		gameaction = ga_nothing;
		insave = false;
		return;
	}

	SaveVersion = SAVEVER;
	PutSavePic (stdfile, SAVEPICWIDTH, SAVEPICHEIGHT);
	M_AppendPNGText (stdfile, "Software", "ZDoom " DOTVERSIONSTR);
	M_AppendPNGText (stdfile, "Engine", GAMESIG);
	M_AppendPNGText (stdfile, "ZDoom Save Version", SAVESIG);
	M_AppendPNGText (stdfile, "Title", savedescription);
	M_AppendPNGText (stdfile, "Current Map", level.mapname);
	PutSaveWads (stdfile);
	PutSaveComment (stdfile);

	// Intermission stats for hubs
	G_WriteHubInfo(stdfile);

	{
		BYTE vars[4096], *vars_p;
		vars_p = vars;
		C_WriteCVars (&vars_p, CVAR_SERVERINFO);
		*vars_p = 0;
		M_AppendPNGText (stdfile, "Important CVARs", (char *)vars);
	}

	if (level.time != 0 || level.maptime != 0)
	{
		DWORD time[2] = { BigLong(TICRATE), BigLong(level.time) };
		M_AppendPNGChunk (stdfile, MAKE_ID('p','t','I','c'), (BYTE *)&time, 8);
	}

	G_WriteSnapshots (stdfile);
	FRandom::StaticWriteRNGState (stdfile);
	P_WriteACSDefereds (stdfile);

	WriteVars (stdfile, ACS_WorldVars, NUM_WORLDVARS, MAKE_ID('w','v','A','r'));
	WriteVars (stdfile, ACS_GlobalVars, NUM_GLOBALVARS, MAKE_ID('g','v','A','r'));
	WriteArrayVars (stdfile, ACS_WorldArrays, NUM_WORLDVARS, MAKE_ID('w','a','R','r'));
	WriteArrayVars (stdfile, ACS_GlobalArrays, NUM_GLOBALVARS, MAKE_ID('g','a','R','r'));

	// [BC] Write the invasion state, etc.
	if ( invasion )
		INVASION_WriteSaveInfo( stdfile );

	if (NextSkill != -1)
	{
		BYTE next = NextSkill;
		M_AppendPNGChunk (stdfile, MAKE_ID('s','n','X','t'), &next, 1);
	}

	M_NotifyNewSave (savegamefile.GetChars(), savedescription, okForQuicksave);
	gameaction = ga_nothing;
	savedescription[0] = 0;

	M_FinishPNG (stdfile);
	fclose (stdfile);

	// Check whether the file is ok.
	bool success = false;
	stdfile = fopen (savegamefile.GetChars(), "rb");
	if (stdfile != NULL)
	{
		PNGHandle * pngh = M_VerifyPNG(stdfile);
		if (pngh != NULL)
		{
			success=true;
			delete pngh;
		}
		fclose(stdfile);
	}
	if (success) Printf ("%s\n", GStrings("GGSAVED"));
	else Printf(PRINT_HIGH, "Save failed\n");

	BackupSaveName = savegamefile;
	savegamefile = "";

	// We don't need the snapshot any longer.
	if (level.info->snapshot != NULL)
	{
		delete level.info->snapshot;
		level.info->snapshot = NULL;
	}
		
	insave = false;
}




//
// DEMO RECORDING
//

void G_ReadDemoTiccmd (ticcmd_t *cmd, int player)
{
	int id = DEM_BAD;

	while (id != DEM_USERCMD && id != DEM_EMPTYUSERCMD)
	{
		if (!demorecording && demo_p >= zdembodyend)
		{
			// nothing left in the BODY chunk, so end playback.
			G_CheckDemoStatus ();
			break;
		}

		id = ReadByte (&demo_p);

		switch (id)
		{
		case DEM_STOP:
			// end of demo stream
			G_CheckDemoStatus ();
			break;

		case DEM_USERCMD:
			UnpackUserCmd (&cmd->ucmd, &cmd->ucmd, &demo_p);
			break;

		case DEM_EMPTYUSERCMD:
			// leave cmd->ucmd unchanged
			break;

		case DEM_DROPPLAYER:
			{
				BYTE i = ReadByte (&demo_p);
				if (i < MAXPLAYERS)
				{
					playeringame[i] = false;
					playerswiping &= ~(1 << i);
				}
			}
			break;

		default:
			Net_DoCommand (id, &demo_p, player);
			break;
		}
	}
} 

bool stoprecording;

CCMD (stop)
{
	stoprecording = true;
}

extern BYTE *lenspot;

void G_WriteDemoTiccmd (ticcmd_t *cmd, int player, int buf)
{
	BYTE *specdata;
	int speclen;

	// Not in multiplayer.
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
		return;

	if (stoprecording)
	{ // use "stop" console command to end demo recording
		G_CheckDemoStatus ();
		if ( NETWORK_GetState( ) != NETSTATE_CLIENT )
		{
			gameaction = ga_fullconsole;
		}
		return;
	}

	// [RH] Write any special "ticcmds" for this player to the demo
	if ((specdata = NetSpecs[player][buf].GetData (&speclen)) && gametic % ticdup == 0)
	{
		memcpy (demo_p, specdata, speclen);
		demo_p += speclen;
		NetSpecs[player][buf].SetData (NULL, 0);
	}

	// [RH] Now write out a "normal" ticcmd.
	WriteUserCmdMessage (&cmd->ucmd, &players[player].cmd.ucmd, &demo_p);

	// [RH] Bigger safety margin
	if (demo_p > demobuffer + maxdemosize - 64)
	{
		ptrdiff_t pos = demo_p - demobuffer;
		ptrdiff_t spot = lenspot - demobuffer;
		ptrdiff_t comp = democompspot - demobuffer;
		ptrdiff_t body = demobodyspot - demobuffer;
		// [RH] Allocate more space for the demo
		maxdemosize += 0x20000;
		demobuffer = (BYTE *)M_Realloc (demobuffer, maxdemosize);
		demo_p = demobuffer + pos;
		lenspot = demobuffer + spot;
		democompspot = demobuffer + comp;
		demobodyspot = demobuffer + body;
	}
}



//
// G_RecordDemo
//
void G_RecordDemo (char* name)
{
	char *v;

	usergame = false;
	strcpy (demoname, name);
	FixPathSeperator (demoname);
	DefaultExtension (demoname, ".lmp");
	v = Args.CheckValue ("-maxdemo");
	maxdemosize = 0x20000;
	demobuffer = (BYTE *)M_Malloc (maxdemosize);

	demorecording = true; 
}


// [RH] Demos are now saved as IFF FORMs. I've also removed support
//		for earlier ZDEMs since I didn't want to bother supporting
//		something that probably wasn't used much (if at all).

void G_BeginRecording (const char *startmap)
{
	int i;

	if (startmap == NULL)
	{
		startmap = level.mapname;
	}
	demo_p = demobuffer;

	WriteLong (FORM_ID, &demo_p);			// Write FORM ID
	demo_p += 4;							// Leave space for len
	WriteLong (ZDEM_ID, &demo_p);			// Write ZDEM ID

	// Write header chunk
	StartChunk (ZDHD_ID, &demo_p);
	WriteWord (DEMOGAMEVERSION, &demo_p);			// Write ZDoom version
	*demo_p++ = 2;							// Write minimum version needed to use this demo.
	*demo_p++ = 3;							// (Useful?)
	for (i = 0; i < 8; i++)					// Write name of map demo was recorded on.
	{
		*demo_p++ = startmap[i];
	}
	WriteLong (rngseed, &demo_p);			// Write RNG seed
	*demo_p++ = consoleplayer;
	FinishChunk (&demo_p);

	// Write player info chunks
	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i])
		{
			StartChunk (UINF_ID, &demo_p);
			WriteByte ((BYTE)i, &demo_p);
			D_WriteUserInfoStrings (i, &demo_p);
			FinishChunk (&demo_p);
		}
	}

	// It is possible to start a "multiplayer" game with only one player,
	// so checking the number of players when playing back the demo is not
	// enough.
	if ( NETWORK_GetState( ) != NETSTATE_SINGLE )
	{
		StartChunk (NETD_ID, &demo_p);
		FinishChunk (&demo_p);
	}

	// Write cvars chunk
	StartChunk (VARS_ID, &demo_p);
	C_WriteCVars (&demo_p, CVAR_SERVERINFO|CVAR_DEMOSAVE);
	FinishChunk (&demo_p);

	// Indicate body is compressed
	StartChunk (COMP_ID, &demo_p);
	democompspot = demo_p;
	WriteLong (0, &demo_p);
	FinishChunk (&demo_p);

	// Begin BODY chunk
	StartChunk (BODY_ID, &demo_p);
	demobodyspot = demo_p;
}


//
// G_PlayDemo
//

char defdemoname[128];

void G_DeferedPlayDemo (char *name)
{
	strncpy (defdemoname, name, 127);
	gameaction = ga_playdemo;
}

CCMD (playdemo)
{
	if (argv.argc() > 1)
	{
		G_DeferedPlayDemo (argv[1]);
		singledemo = true;
	}
}

CCMD (timedemo)
{
	if (argv.argc() > 1)
	{
		G_TimeDemo (argv[1]);
		singledemo = true;
	}
}

// [RH] Process all the information in a FORM ZDEM
//		until a BODY chunk is entered.
bool G_ProcessIFFDemo (char *mapname)
{
	bool headerHit = false;
	bool bodyHit = false;
	int numPlayers = 0;
	int id, len, i;
	uLong uncompSize = 0;
	BYTE *nextchunk;

	demoplayback = true;

	for (i = 0; i < MAXPLAYERS; i++)
		playeringame[i] = 0;

	len = ReadLong (&demo_p);
	zdemformend = demo_p + len + (len & 1);

	// Check to make sure this is a ZDEM chunk file.
	// TODO: Support multiple FORM ZDEMs in a CAT. Might be useful.

	id = ReadLong (&demo_p);
	if (id != ZDEM_ID)
	{
		Printf ("Not a ZDoom demo file!\n");
		return true;
	}

	// Process all chunks until a BODY chunk is encountered.

	while (demo_p < zdemformend && !bodyHit)
	{
		id = ReadLong (&demo_p);
		len = ReadLong (&demo_p);
		nextchunk = demo_p + len + (len & 1);
		if (nextchunk > zdemformend)
		{
			Printf ("Demo is mangled!\n");
			return true;
		}

		switch (id)
		{
		case ZDHD_ID:
			headerHit = true;

			demover = ReadWord (&demo_p);	// ZDoom version demo was created with
			if (demover < MINDEMOVERSION)
			{
				Printf ("Demo requires an older version of ZDoom!\n");
				//return true;
			}
			if (ReadWord (&demo_p) > DEMOGAMEVERSION)	// Minimum ZDoom version
			{
				Printf ("Demo requires a newer version of ZDoom!\n");
				return true;
			}
			memcpy (mapname, demo_p, 8);	// Read map name
			mapname[8] = 0;
			demo_p += 8;
			rngseed = ReadLong (&demo_p);
			FRandom::StaticClearRandom ();
			consoleplayer = *demo_p++;
			break;

		case VARS_ID:
			C_ReadCVars (&demo_p);
			break;

		case UINF_ID:
			i = ReadByte (&demo_p);
			if (!playeringame[i])
			{
				playeringame[i] = 1;
				numPlayers++;
			}
			D_ReadUserInfoStrings (i, &demo_p, false);
			break;

		case NETD_ID:

			NETWORK_SetState( NETSTATE_SINGLE_MULTIPLAYER );
			break;

		case BODY_ID:
			bodyHit = true;
			zdembodyend = demo_p + len;
			break;

		case COMP_ID:
			uncompSize = ReadLong (&demo_p);
			break;
		}

		if (!bodyHit)
			demo_p = nextchunk;
	}

	if (!numPlayers)
	{
		Printf ("Demo has no players!\n");
		return true;
	}

	if (!bodyHit)
	{
		zdembodyend = NULL;
		Printf ("Demo has no BODY chunk!\n");
		return true;
	}

	if (numPlayers > 1)
		NETWORK_SetState( NETSTATE_SINGLE_MULTIPLAYER );

	if (uncompSize > 0)
	{
		BYTE *uncompressed = new BYTE[uncompSize];
		int r = uncompress (uncompressed, &uncompSize, demo_p, zdembodyend - demo_p);
		if (r != Z_OK)
		{
			Printf ("Could not decompress demo!\n");
			delete[] uncompressed;
			return true;
		}
		delete[] demobuffer;
		zdembodyend = uncompressed + uncompSize;
		demobuffer = demo_p = uncompressed;
	}

	return false;
}

void G_DoPlayDemo (void)
{
	char mapname[9];
	int demolump;
	// [BC] For saving the output of ReadLong().
	int	i;

	gameaction = ga_nothing;

	// [RH] Allow for demos not loaded as lumps
	demolump = Wads.CheckNumForName (defdemoname);
	if (demolump >= 0)
	{
		int demolen = Wads.LumpLength (demolump);
		demobuffer = new BYTE[demolen];
		Wads.ReadLump (demolump, demobuffer);
	}
	else
	{
		// [BC] First, see if a .cld demo with this name exists.
		FixPathSeperator (defdemoname);
		DefaultExtension (defdemoname, ".cld");
		if ( M_DoesFileExist( defdemoname ))
		{
			// Put the game in the full console.
			gameaction = ga_fullconsole;

			CLIENTDEMO_DoPlayDemo( defdemoname );
			return;
		}
		else
			defdemoname[strlen( defdemoname ) - 4] = 0;

		FixPathSeperator (defdemoname);
		DefaultExtension (defdemoname, ".lmp");
		M_ReadFile (defdemoname, &demobuffer);
	}
	demo_p = demobuffer;

	Printf ("Playing demo %s\n", defdemoname);

	C_BackupCVars ();		// [RH] Save cvars that might be affected by demo

	if (( i = ReadLong (&demo_p)) != FORM_ID)
	{
		const char *eek = "Cannot play non-ZDoom demos.\n(They would go out of sync badly.)\n";

		// [BC] Check if this is one of Skulltag's client-side demos.
		// [BC] THIS IS BROKEN NOW
		if ( i == CLD_DEMOSTART )
		{
			CLIENTDEMO_DoPlayDemo( defdemoname );
			return;
		}

		if (singledemo)
		{
			I_Error (eek);
		}
		else
		{
			Printf (PRINT_BOLD, eek);
			gameaction = ga_nothing;
		}
	}
	else if (G_ProcessIFFDemo (mapname))
	{
		gameaction = ga_nothing;
		demoplayback = false;
	}
	else
	{
		// don't spend a lot of time in loadlevel 
		precache = false;
		demonew = true;
		G_InitNew (mapname, false);
		C_HideConsole ();
		demonew = false;
		precache = true;

		usergame = false;
		demoplayback = true;
	}
}

//
// G_TimeDemo
//
void G_TimeDemo (char* name)
{
	nodrawers = !!Args.CheckParm ("-nodraw");
	noblit = !!Args.CheckParm ("-noblit");
	timingdemo = true;
	singletics = true;

	strncpy (defdemoname, name, 128);
	gameaction = ga_playdemo;
}


/*
===================
=
= G_CheckDemoStatus
=
= Called after a death or level completion to allow demos to be cleaned up
= Returns true if a new demo loop action will take place
===================
*/

bool G_CheckDemoStatus (void)
{
	// [BC] Support for client-side demos.
	if (!demorecording && ( CLIENTDEMO_IsRecording( ) == false ))
	{ // [RH] Restore the player's userinfo settings.
		D_SetupUserInfo();
	}

	if (demoplayback)
	{
		extern int starttime;
		int endtime = 0;

		if (timingdemo)
			endtime = I_GetTime (false) - starttime;

		C_RestoreCVars ();		// [RH] Restore cvars demo might have changed

		delete[] demobuffer;
		demoplayback = false;
		netdemo = false;
//		netgame = false;
//		multiplayer = false;
		singletics = false;
		{
			int i;

			for (i = 1; i < MAXPLAYERS; i++)
				playeringame[i] = 0;
		}
		consoleplayer = 0;
		players[0].camera = NULL;
		StatusBar->AttachToPlayer (&players[0]);

		if (singledemo || timingdemo)
		{
			if (timingdemo)
			{
				// Trying to get back to a stable state after timing a demo
				// seems to cause problems. I don't feel like fixing that
				// right now.
				I_FatalError ("timed %i gametics in %i realtics (%.1f fps)\n"
							  "(This is not really an error.)", gametic,
							  endtime, (float)gametic/(float)endtime*(float)TICRATE);
			}
			else
			{
				Printf ("Demo ended.\n");
			}
			gameaction = ga_fullconsole;
			timingdemo = false;
			return false;
		}
		else
		{
			D_AdvanceDemo (); 
		}

		return true; 
	}

	if (demorecording)
	{
		BYTE *formlen;

		WriteByte (DEM_STOP, &demo_p);

		if (demo_compress)
		{
			// Now that the entire BODY chunk has been created, replace it with
			// a compressed version. If the BODY successfully compresses, the
			// contents of the COMP chunk will be changed to indicate the
			// uncompressed size of the BODY.
			uLong len = demo_p - demobodyspot;
			uLong outlen = (len + len/100 + 12);
			Byte *compressed = new Byte[outlen];
			int r = compress2 (compressed, &outlen, demobodyspot, len, 9);
			if (r == Z_OK && outlen < len)
			{
				formlen = democompspot;
				WriteLong (len, &democompspot);
				memcpy (demobodyspot, compressed, outlen);
				demo_p = demobodyspot + outlen;
			}
			delete[] compressed;
		}
		FinishChunk (&demo_p);
		formlen = demobuffer + 4;
		WriteLong (demo_p - demobuffer - 8, &formlen);

		M_WriteFile (demoname, demobuffer, demo_p - demobuffer); 
		free (demobuffer); 
		demorecording = false;
		stoprecording = false;
		Printf ("Demo %s recorded\n", demoname); 
	}

	return false; 
}

// [BC] New console command that freezes all actors (except the player
// who activated the cheat).

//*****************************************************************************
//
CCMD( freeze )
{
	if (( NETWORK_GetState( ) == NETSTATE_SINGLE ) || ( NETWORK_GetState( ) == NETSTATE_SINGLE_MULTIPLAYER ))
	{
		// Toggle the freeze mode.
		if ( level.flags & LEVEL_FROZEN )
			level.flags &= ~LEVEL_FROZEN;
		else
			level.flags|= LEVEL_FROZEN;

		Printf( "Freeze mode %s\n", ( level.flags & LEVEL_FROZEN ) ? "ON" : "OFF" );

		if ( level.flags & LEVEL_FROZEN )
			players[consoleplayer].cheats |= CF_FREEZE;
		else
			players[consoleplayer].cheats &= ~CF_FREEZE;
	}
}
