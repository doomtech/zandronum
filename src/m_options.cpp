/*
** m_options.cpp
** New options menu code
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
** Sorry this got so convoluted. It was originally much cleaner until
** I started adding all sorts of gadgets to the menus. I might someday
** make a project of rewriting the entire menu system using Amiga-style
** taglists to describe each menu item. We'll see... (Probably not.)
*/
#include "templates.h"
#include "doomdef.h"
#include "gstrings.h"
#include <string.h>
#include "c_console.h"
#include "c_dispatch.h"
#include "c_bind.h"

#include "d_main.h"
#include "d_gui.h"

#include "i_system.h"
#include "i_video.h"

#include "i_music.h"
#include "i_input.h"

#include "v_video.h"
#include "v_text.h"
#include "w_wad.h"
#include "gi.h"

#include "r_local.h"
#include "v_palette.h"
#include "gameconfigfile.h"

#include "hu_stuff.h"

#include "g_game.h"

#include "m_argv.h"
#include "m_swap.h"

#include "s_sound.h"

#include "doomstat.h"

#include "m_misc.h"
#include "sc_man.h"
#include "cmdlib.h"
#include "d_event.h"

// Data.
#include "m_menu.h"

#include "announcer.h"
#include "cl_commands.h"
#include "cl_demo.h"
#include "cl_main.h"
#include "deathmatch.h"
#include "duel.h"
#include "team.h"
#include "network.h"
#include "scoreboard.h"
#include "version.h"
#include "browser.h"
#include "m_random.h"
#include "lastmanstanding.h"
#include "campaign.h"
#include "callvote.h"
#include "cooperative.h"
#include "invasion.h"
#include "chat.h"
#include "hardware.h"
#include "sbar.h"
#include "p_effect.h"
#include "win32/g15/g15.h"
#include "gl/gl_functions.h"
#include "team.h"
#include "gamemode.h"
#include "g_level.h"

// [ZZ] PWO header file
#include "g_shared/pwo.h"

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void R_GetPlayerTranslation (int color, FPlayerSkin *skin, BYTE *table);
void StartGLMenu (void);
void M_StartMessage (const char *string, void (*routine)(int), bool input);

EXTERN_CVAR(Int, vid_renderer)
EXTERN_CVAR(Bool, nomonsterinterpolation)
EXTERN_CVAR(Int, showendoom)
EXTERN_CVAR(Bool, hud_althud)
EXTERN_CVAR(Int, compatmode)

static value_t Renderers[] = {
	{ 0.0, "Software" },
	{ 1.0, "OpenGL" },
};
//EXTERN_CVAR(Bool, hud_althud)
extern bool gl_disabled;

//
// defaulted values
//
CVAR (Float, mouse_sensitivity, 1.f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

// Show messages has default, 0 = off, 1 = on
CVAR (Bool, show_messages, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Bool, show_obituaries, true, CVAR_ARCHIVE)
EXTERN_CVAR (Bool, longsavemessages)
EXTERN_CVAR (Bool, screenshot_quiet)

// [RC] Played when a chat message arrives. Values: off, default, Doom 1 (dstink), Doom 2 (dsradio).
CVAR (Int, chat_sound, 1, CVAR_ARCHIVE)

extern int	skullAnimCounter;

EXTERN_CVAR (String, name)
EXTERN_CVAR (Color, color)
EXTERN_CVAR (String, skin)
EXTERN_CVAR (String, gender)
EXTERN_CVAR (Int, railcolor)
EXTERN_CVAR (Int, handicap)
EXTERN_CVAR (Bool, cl_run)
EXTERN_CVAR (Bool, cl_identifytarget)
EXTERN_CVAR( Int, cl_connectiontype )
EXTERN_CVAR (Int, crosshair)
EXTERN_CVAR (Bool, freelook)
EXTERN_CVAR (Int, sv_smartaim)
EXTERN_CVAR (Int, am_colorset)
EXTERN_CVAR (String,	playerclass)

/*static*/	ULONG		g_ulPlayerSetupSkin;
/*static*/	ULONG		g_ulPlayerSetupColor;
/*static*/	LONG		g_lPlayerSetupClass;

CVAR( String, menu_name, "", 0 )
CVAR( Color, menu_color, 0x000000, 0 )
CVAR( String, menu_skin, "", 0 );
CVAR( String, menu_playerclass, "", 0 );
CVAR( Int, menu_gender, 0, 0 );
CVAR( Int, menu_railcolor, 0, 0 );
CVAR( Int, menu_handicap, 0, 0 );
CVAR( Float, menu_autoaim, 0.0f, 0 );

static void CalcIndent (menu_t *menu);

void M_ChangeMessages ();
void M_SizeDisplay (int diff);

int  M_StringHeight (char *string);

EColorRange LabelColor;
EColorRange ValueColor;
EColorRange MoreColor;

static bool CanScrollUp;
static bool CanScrollDown;
static int VisBottom;

static	LONG	g_lSavedColor;
static	bool	g_bSwitchColorBack;

value_t YesNo[2] = {
	{ 0.0, "No" },
	{ 1.0, "Yes" }
};

value_t NoYes[2] = {
	{ 0.0, "Yes" },
	{ 1.0, "No" }
};

value_t OnOff[2] = {
	{ 0.0, "Off" },
	{ 1.0, "On" }
};

value_t OffOn[2] = {
	{ 0.0, "On" },
	{ 1.0, "Off" }
};

value_t CompatModes[5] = {
	{ 0.0, "Default" },
	{ 1.0, "Doom" },
	{ 2.0, "Doom (strict)" },
	{ 3.0, "Boom" },
	{ 4.0, "ZDoom 2.0.63" }
};

value_t GenderVals[3] = {
	{ 0.0, "Male" },
	{ 1.0, "Female" },
	{ 2.0, "Other" },
};

value_t AutoaimVals[7] = {
	{ 0.0, "Never" },
	{ 1.0, "Very Low" },
	{ 2.0, "Low" },
	{ 3.0, "Medium" },
	{ 4.0, "High" },
	{ 5.0, "Very high" },
	{ 6.0, "Always" }
};

value_t TrailColorVals[11] = {
	{ 0.0, "Blue" },
	{ 1.0, "Red" },
	{ 2.0, "Yellow" },
	{ 3.0, "Black" },
	{ 4.0, "Silver" },
	{ 5.0, "Gold" },
	{ 6.0, "Green" },
	{ 7.0, "White" },
	{ 8.0, "Purple" },
	{ 9.0, "Orange" },
	{ 10.0, "Rainbow" }
};

value_t AllowSkinVals[3] = {
	{ 0.0, "No skins" },
	{ 1.0, "All skins" },
	{ 2.0, "No cheat skins" },
};

value_t ConnectionTypeVals[2] = {
	{ 0.0, "56k/ISDN" },
	{ 1.0, "DSL" },
};

value_t GameskillVals[5] = {
	{ 0.0, "I'm too young to die." },
	{ 1.0, "Hey, not too rough." },
	{ 2.0, "Hurt me plenty." },
	{ 3.0, "Ultra-Violence." },
	{ 4.0, "Nightmare!" }
};

value_t BotskillVals[5] = {
	{ 0.0, "I want my mommy!" },
	{ 1.0, "I'm allergic to pain." },
	{ 2.0, "Bring it on." },
	{ 3.0, "I thrive off pain." },
	{ 4.0, "Nightmare!" }
};

value_t ModifierVals[3] = {
	{ 0.0, "None" },
	{ 1.0, "Instagib" },
	{ 2.0, "Buckshot" },
};

// [CW] Add to this when bumping 'MAX_TEAMS'.
value_t TeamVals[MAX_TEAMS] = {
	{ 0.0, "Team 1" },
	{ 1.0, "Team 2" },
	{ 2.0, "Team 3" },
	{ 3.0, "Team 4" },
};

value_t GameModeVals[16] = {
	{ 0.0, "Cooperative" },
	{ 1.0, "Survival Cooperative" },
	{ 2.0, "Invasion" },
	{ 3.0, "Deathmatch" },
	{ 4.0, "Team Deathmatch" },
	{ 5.0, "Duel" },
	{ 6.0, "Terminator" },
	{ 7.0, "Last Man Standing" },
	{ 8.0, "Team Last Man Standing" },
	{ 9.0, "Possession" },
	{ 10.0, "Team Possession" },
	{ 11.0, "Teamgame (ACS)" },
	{ 12.0, "Capture the Flag" },
	{ 13.0, "One flag CTF" },
	{ 14.0, "Skulltag" },
	{ 15.0, "Domination" },
};

value_t ServerTypeVals[2] = {
	{ 0.0, "Internet" },
	{ 1.0, "Local" },
};

value_t ServerSortTypeVals[4] = {
	{ 0.0, "Ping" },
	{ 1.0, "Server Name" },
	{ 2.0, "Map Name" },
	{ 3.0, "Players" },
};

value_t ServerGameModeVals[17] = {
	{ 0.0, "Any mode" },
	{ 1.0, "Cooperative" },
	{ 2.0, "Survival cooperative" },
	{ 3.0, "Invasion" },
	{ 4.0, "Deathmatch (free for all)" },
	{ 5.0, "Teamplay deathmatch" },
	{ 6.0, "Duel deathmatch" },
	{ 7.0, "Terminator deathmatch" },
	{ 8.0, "Last man standing deathmatch" },
	{ 9.0, "Team last man standing" },
	{ 10.0, "Possession deathmatch" },
	{ 11.0, "Team possession" },
	{ 12.0, "Teamgame" },
	{ 13.0, "Capture the flag" },
	{ 14.0, "One flag CTF" },
	{ 15.0, "Skulltag" },
	{ 16.0, "Domination" },
};

value_t SwitchOnPickupVals[4] = {
	{ 0.0, "Never" },
	{ 1.0, "Only higher ranked" },
	{ 2.0, "Always" },
	// [ZZ] Added PWO item
	{ 3.0, "Use PWO" },
};

menu_t  *CurrentMenu;
int		CurrentItem;
static const char	   *OldMessage;
static itemtype OldType;

extern	IVideo	*Video;
static	bool	g_bStringInput = false;
static	char	g_szStringInputBuffer[64];
static	char	g_szWeaponPrefStringBuffer[20][64];

int flagsvar;
enum
{
	SHOW_DMFlags = 1,
	SHOW_DMFlags2 = 2,
	SHOW_CompatFlags = 4,
	SHOW_MenuDMFlags = 8,
	SHOW_MenuDMFlags2 = 16
};

/*=======================================
 *
 * Confirm Menu - Used by safemore
 *
 *=======================================*/
static void ActivateConfirm (const char *text, void (*func)());
static void ConfirmIsAGo ();

static menuitem_t ConfirmItems[] = {
	{ whitetext,NULL,									{NULL}, {0}, {0}, {0}, {NULL} },
	{ redtext,	"Do you really want to do this?",		{NULL}, {0}, {0}, {0}, {NULL} },
	{ redtext,	" ",									{NULL}, {0}, {0}, {0}, {NULL} },
	{ rightmore,"Yes",									{NULL}, {0}, {0}, {0}, {(value_t*)ConfirmIsAGo} },
	{ rightmore,"No",									{NULL}, {0}, {0}, {0}, {(value_t*)M_PopMenuStack} },
};

static menu_t ConfirmMenu = {
	"PLEASE CONFIRM",
	3,
	countof(ConfirmItems),
	140,
	ConfirmItems,
};

/*=======================================
 *
 * Options Menu
 *
 *=======================================*/

static void CustomizeControls (void);
static void GameplayOptions (void);
static void CompatibilityOptions (void);
static void VideoOptions (void);
static void SoundOptions (void);
static void MouseOptions (void);
static void JoystickOptions (void);
static void GoToConsole (void);
void M_PlayerSetup (void);
void M_SkulltagVersionDrawer( void );
void Reset2Defaults (void);
void Reset2Saved (void);

static void SetVidMode (void);

static menu_t MenuPWO = 
{
	"WEAPON SETUP",
	0,
	0,
	0,
	NULL,
};

void WeaponOptions()
{
	if(!PWO_MenuOptions) PWO_GenerateMenu();
	MenuPWO.items = PWO_MenuOptions;
	MenuPWO.numitems = PWO_MenuOptionsSz;

	M_SwitchMenu(&MenuPWO);
}

static menuitem_t OptionItems[] =
{
	{ more,		"Customize Controls",	{NULL},					{0.0}, {0.0},	{0.0}, {(value_t *)CustomizeControls} },
	{ more,		"Mouse options",		{NULL},					{0.0}, {0.0},	{0.0}, {(value_t *)MouseOptions} },
	{ more,		"Joystick options",		{NULL},					{0.0}, {0.0},	{0.0}, {(value_t *)JoystickOptions} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ more,		"Player setup",			{NULL},					{0.0}, {0.0},	{0.0}, {(value_t *)M_PlayerSetup} },
	{ more,		"Gameplay Options",		{NULL},					{0.0}, {0.0},	{0.0}, {(value_t *)GameplayOptions} },
	{ more,		"Compatibility Options",{NULL},					{0.0}, {0.0},	{0.0}, {(value_t *)CompatibilityOptions} },
	{ more,		"Sound Options",		{NULL},					{0.0}, {0.0},	{0.0}, {(value_t *)SoundOptions} },
	{ more,		"Display Options",		{NULL},					{0.0}, {0.0},	{0.0}, {(value_t *)VideoOptions} },
	{ more,		"Set video mode",		{NULL},					{0.0}, {0.0},	{0.0}, {(value_t *)SetVidMode} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ safemore,	"Reset to defaults",	{NULL},					{0.0}, {0.0},	{0.0}, {(value_t *)Reset2Defaults} },
	{ safemore,	"Reset to last saved",	{NULL},					{0.0}, {0.0},	{0.0}, {(value_t *)Reset2Saved} },
	{ more,		"Go to console",		{NULL},					{0.0}, {0.0},	{0.0}, {(value_t *)GoToConsole} },
};

menu_t OptionMenu =
{
	"OPTIONS",
	0,
	countof(OptionItems),
	0,
	OptionItems,
	0,
	0,
	0,
	M_SkulltagVersionDrawer,
	false,
	NULL,
};

/*=======================================
 *
 * Mouse Menu
 *
 *=======================================*/

EXTERN_CVAR (Bool, use_mouse)
EXTERN_CVAR (Bool, smooth_mouse)
EXTERN_CVAR (Float, m_forward)
EXTERN_CVAR (Float, m_pitch)
EXTERN_CVAR (Float, m_side)
EXTERN_CVAR (Float, m_yaw)
EXTERN_CVAR (Bool, invertmouse)
EXTERN_CVAR (Bool, lookspring)
EXTERN_CVAR (Bool, lookstrafe)
EXTERN_CVAR (Bool, m_noprescale)
// [BB] Added m_filter.
EXTERN_CVAR (Bool, m_filter)

static menuitem_t MouseItems[] =
{
	{ discrete,	"Enable mouse",			{&use_mouse},			{2.0}, {0.0},	{0.0}, {YesNo} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ slider,	"Overall sensitivity",	{&mouse_sensitivity},	{0.5}, {2.5},	{0.1}, {NULL} },
	{ discrete,	"Prescale mouse movement",{&m_noprescale},		{2.0}, {0.0},	{0.0}, {NoYes} },
	// [BB] smooth_mouse doesn't do anything in Skulltag (Carn deactivated it long ago), so link this to m_filter.
	{ discrete, "Smooth mouse movement",{&m_filter},			{2.0}, {0.0},	{0.0}, {YesNo} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ slider,	"Turning speed",		{&m_yaw},				{0.5}, {2.5},	{0.1}, {NULL} },
	{ slider,	"Mouselook speed",		{&m_pitch},				{0.5}, {2.5},	{0.1}, {NULL} },
	{ slider,	"Forward/Backward speed",{&m_forward},			{0.5}, {2.5},	{0.1}, {NULL} },
	{ slider,	"Strafing speed",		{&m_side},				{0.5}, {2.5},	{0.1}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ discrete, "Always Mouselook",		{&freelook},			{2.0}, {0.0},	{0.0}, {OnOff} },
	{ discrete, "Invert Mouse",			{&invertmouse},			{2.0}, {0.0},	{0.0}, {OnOff} },
	{ discrete, "Lookspring",			{&lookspring},			{2.0}, {0.0},	{0.0}, {OnOff} },
	{ discrete, "Lookstrafe",			{&lookstrafe},			{2.0}, {0.0},	{0.0}, {OnOff} },
};

menu_t MouseMenu =
{
	"MOUSE OPTIONS",
	0,
	countof(MouseItems),
	0,
	MouseItems,
};

/*=======================================
 *
 * Joystick Menu
 *
 *=======================================*/

EXTERN_CVAR (Bool, use_joystick)
EXTERN_CVAR (Float, joy_speedmultiplier)
EXTERN_CVAR (Int, joy_xaxis)
EXTERN_CVAR (Int, joy_yaxis)
EXTERN_CVAR (Int, joy_zaxis)
EXTERN_CVAR (Int, joy_xrot)
EXTERN_CVAR (Int, joy_yrot)
EXTERN_CVAR (Int, joy_zrot)
EXTERN_CVAR (Int, joy_slider)
EXTERN_CVAR (Int, joy_dial)
EXTERN_CVAR (Float, joy_xthreshold)
EXTERN_CVAR (Float, joy_ythreshold)
EXTERN_CVAR (Float, joy_zthreshold)
EXTERN_CVAR (Float, joy_xrotthreshold)
EXTERN_CVAR (Float, joy_yrotthreshold)
EXTERN_CVAR (Float, joy_zrotthreshold)
EXTERN_CVAR (Float, joy_sliderthreshold)
EXTERN_CVAR (Float, joy_dialthreshold)
EXTERN_CVAR (Float, joy_yawspeed)
EXTERN_CVAR (Float, joy_pitchspeed)
EXTERN_CVAR (Float, joy_forwardspeed)
EXTERN_CVAR (Float, joy_sidespeed)
EXTERN_CVAR (Float, joy_upspeed)
EXTERN_CVAR (GUID, joy_guid)

static value_t JoyAxisMapNames[6] =
{
	{ 0.0, "None" },
	{ 1.0, "Turning" },
	{ 2.0, "Looking Up/Down" },
	{ 3.0, "Moving Forward" },
	{ 4.0, "Strafing" },
	{ 5.0, "Moving Up/Down" }
};

static value_t Inversion[2] =
{
	{ 0.0, "Not Inverted" },
	{ 1.0, "Inverted" }
};

static menuitem_t JoystickItems[] =
{
	{ discrete,	"Enable joystick",		{&use_joystick},		{2.0}, {0.0},	{0.0}, {YesNo} },
	{ discrete_guid,"Active joystick",	{&joy_guid},			{0.0}, {0.0},	{0.0}, {NULL} },
	{ slider,	"Overall sensitivity",	{&joy_speedmultiplier},	{0.9}, {2.0},	{0.2}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ whitetext,"Axis Assignments",		{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },

	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
};

menu_t JoystickMenu =
{
	"JOYSTICK OPTIONS",
	0,
	countof(JoystickItems),
	0,
	JoystickItems,
};


/*=======================================
 *
 * Controls Menu
 *
 *=======================================*/

menuitem_t ControlsItems[] =
{
	{ redtext,"ENTER to change, BACKSPACE to clear", {NULL}, {0.0}, {0.0}, {0.0}, {NULL} },
	{ redtext,	" ",					{NULL},	{0.0}, {0.0}, {0.0}, {NULL} },
	{ whitetext,"Controls",				{NULL},	{0.0}, {0.0}, {0.0}, {NULL} },
	{ control,	"Fire",					{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"+attack"} },
	{ control,	"Secondary Fire",		{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"+altattack"} },
	{ control,	"Use / Open",			{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"+use"} },
	{ control,	"Move forward",			{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"+forward"} },
	{ control,	"Move backward",		{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"+back"} },
	{ control,	"Strafe left",			{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"+moveleft"} },
	{ control,	"Strafe right",			{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"+moveright"} },
	{ control,	"Turn left",			{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"+left"} },
	{ control,	"Turn right",			{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"+right"} },
	{ control,	"Jump",					{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"+jump"} },
	{ control,	"Crouch",				{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"+crouch"} },
	{ control,	"Crouch Toggle",		{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"crouch"} },
	{ control,	"Fly / Swim up",		{NULL},	{0.0}, {0.0}, {0.0}, {(value_t *)"+moveup"} },
	{ control,	"Fly / Swim down",		{NULL},	{0.0}, {0.0}, {0.0}, {(value_t *)"+movedown"} },
	{ control,	"Stop flying",			{NULL},	{0.0}, {0.0}, {0.0}, {(value_t *)"land"} },
	{ control,	"Mouse look",			{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"+mlook"} },
	{ control,	"Keyboard look",		{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"+klook"} },
	{ control,	"Look up",				{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"+lookup"} },
	{ control,	"Look down",			{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"+lookdown"} },
	{ control,	"Center view",			{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"centerview"} },
	{ control,	"Run",					{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"+speed"} },
	{ control,	"Strafe",				{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"+strafe"} },
	{ redtext,	" ",					{NULL},	{0.0}, {0.0}, {0.0}, {NULL} },
	{ whitetext,"Chat",					{NULL},	{0.0}, {0.0}, {0.0}, {NULL} },
	{ control,	"Say",					{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"say"} },
	{ control,	"Team say",				{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"say_team"} },
	{ redtext,	" ",					{NULL},	{0.0}, {0.0}, {0.0}, {NULL} },
	{ whitetext,"Weapons",				{NULL},	{0.0}, {0.0}, {0.0}, {NULL} },
	{ control,	"Next weapon",			{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"weapnext"} },
	{ control,	"Previous weapon",		{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"weapprev"} },
	{ redtext,	" ",					{NULL},	{0.0}, {0.0}, {0.0}, {NULL} },
	{ whitetext,"Inventory",			{NULL},	{0.0}, {0.0}, {0.0}, {NULL} },
	{ control,	"Activate item",		{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"invuse"} },
	{ control,	"Activate all items",	{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"invuseall"} },
	{ control,	"Next item",			{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"invnext"} },
	{ control,	"Previous item",		{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"invprev"} },
	{ control,	"Drop item",			{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"invdrop"} },
	{ control,	"Query item",			{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"invquery"} },
	{ control,	"Drop weapon",			{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"weapdrop"} },
	{ redtext,	" ",					{NULL},	{0.0}, {0.0}, {0.0}, {NULL} },
	{ whitetext,"Voting",				{NULL},	{0.0}, {0.0}, {0.0}, {NULL} },
	{ control,  "Vote yes",				{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"vote_yes"} },
	{ control,  "Vote no",				{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"vote_no"} },
	{ redtext,	" ",					{NULL},	{0.0}, {0.0}, {0.0}, {NULL} },
	{ whitetext,"Other",				{NULL},	{0.0}, {0.0}, {0.0}, {NULL} },
	{ control,	"Toggle automap",		{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"togglemap"} },
	{ control,	"Chasecam",				{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"chase"} },
	{ control,	"Add a bot",			{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"addbot"} },
	{ control,	"Coop spy",				{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"spynext"} },
	{ control,	"Screenshot",			{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"screenshot"} },
	{ control,	"Show scores",			{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"+showscores"} },
	{ control,	"Show medals",			{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"+showmedals"} },
	{ control,	"Spectate",				{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"spectate"} },
	{ control,	"Taunt",				{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"taunt"} },
	{ control,  "Open console",			{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"toggleconsole"} },
	{ redtext,	" ",					{NULL},	{0.0}, {0.0}, {0.0}, {NULL} },
	{ whitetext,"Strife Popup Screens",	{NULL},	{0.0}, {0.0}, {0.0}, {NULL} },
	{ control,	"Mission objectives",	{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"showpop 1"} },
	{ control,	"Keys list",			{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"showpop 2"} },
	{ control,	"Weapons/ammo/stats",	{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"showpop 3"} },
};

static TArray<menuitem_t> CustomControlsItems (0);

menu_t ControlsMenu =
{
	"CUSTOMIZE CONTROLS",
	3,
	countof(ControlsItems),
	0,
	ControlsItems,
	2,
};

/*=======================================
 *
 * Display Options Menu
 *
 *=======================================*/
static void StartMessagesMenu (void);
static void StartAutomapMenu (void);
static void StartHUDMenu (void);
static void InitCrosshairsList();
#ifdef G15_ENABLED
	static void StartG15Menu (void);
#endif
EXTERN_CVAR (Bool, st_scale)
EXTERN_CVAR (Bool, r_stretchsky)
EXTERN_CVAR (Int,  r_columnmethod)
EXTERN_CVAR (Bool, r_drawfuzz)
EXTERN_CVAR (Int,  cl_rockettrails)
EXTERN_CVAR (Int,  cl_pufftype)
EXTERN_CVAR (Int,  cl_bloodtype)
EXTERN_CVAR (Int,  wipetype)
EXTERN_CVAR (Bool, vid_palettehack)
EXTERN_CVAR (Bool, vid_attachedsurfaces)
EXTERN_CVAR (Int,  screenblocks)
EXTERN_CVAR (Int,  cl_grenadetrails)
EXTERN_CVAR (Float,  blood_fade_scalar)
EXTERN_CVAR (Bool, r_drawtrans)
EXTERN_CVAR (Bool, r_deathcamera)
EXTERN_CVAR (Bool, cl_capfps)


static TArray<valuestring_t> Crosshairs;

static value_t ColumnMethods[] = {
	{ 0.0, "Original" },
	{ 1.0, "Optimized" }
};

static value_t RocketTrailTypes[] = {
	{ 0.0, "Off" },
	{ 1.0, "Particles" },
	{ 2.0, "Sprites" },
	{ 3.0, "Sprites & Particles" }
};

static value_t BloodTypes[] = {
	{ 0.0, "Sprites" },
	{ 1.0, "Sprites & Particles" },
	{ 2.0, "Particles" }
};

static value_t PuffTypes[] = {
	{ 0.0, "Sprites" },
	{ 1.0, "Particles" }
};

static value_t Wipes[] = {
	{ 0.0, "None" },
	{ 1.0, "Melt" },
	{ 2.0, "Burn" },
	{ 3.0, "Crossfade" }
};

static value_t RespawnInvulEffectTypes[] = {
	{ 0.0, "None" },
	{ 1.0, "Skulltag" },
	{ 2.0, "ZDoom" },
};

static value_t Endoom[] = {
	{ 0.0, "Off" },
	{ 1.0, "On" },
	{ 2.0, "Only modified" }
};

static menuitem_t VideoItems[] = {
	{ more,		"Message Options",		{NULL},					{0.0}, {0.0},	{0.0}, {(value_t *)StartMessagesMenu} },
	{ more,     "OpenGL Options",		{NULL},					{0.0}, {0.0},	{0.0}, {(value_t *)StartGLMenu} },
	{ more,		"Automap Options",		{NULL},					{0.0}, {0.0},	{0.0}, {(value_t *)StartAutomapMenu} },
	{ more,		"HUD Options",			{NULL},					{0.0}, {0.0},	{0.0}, {(value_t *)StartHUDMenu} },

	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ slider,	"Gamma correction",		{&Gamma},			   	{0.1}, {3.0},	{0.1}, {NULL} },
	{ slider,	"Brightness",			{&vid_brightness},		{-0.8}, {0.8},	{0.05}, {NULL} },
	{ slider,	"Contrast",				{&vid_contrast},	   	{0.1}, {3.0},	{0.1}, {NULL} },
	{ slider,	"Blood brightness",		{&blood_fade_scalar},  	{0.0}, {1.0},	{0.05}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ discrete, "Cap framerate",		{&cl_capfps},			{4.0}, {0.0},	{0.0}, {YesNo} },
	{ discrete, "Screen wipe style",	{&wipetype},			{4.0}, {0.0},	{0.0}, {Wipes} },
	{ discrete, "Death camera",			{&r_deathcamera},		{4.0}, {0.0},	{0.0}, {YesNo} },
	{ discrete, "Use fuzz effect",		{&r_drawfuzz},			{2.0}, {0.0},	{0.0}, {YesNo} },
	{ discrete, "Respawn invul effect",	{&cl_respawninvuleffect},	{3.0}, {0.0},	{0.0}, {RespawnInvulEffectTypes} },
	{ discrete, "Rocket Trails",		{&cl_rockettrails},		{4.0}, {0.0},	{0.0}, {RocketTrailTypes} },
	{ discrete, "Grenade Trails",		{&cl_grenadetrails},		{2.0}, {0.0},	{0.0}, {OnOff} },
	{ discrete, "Blood Type",			{&cl_bloodtype},	   	{3.0}, {0.0},	{0.0}, {BloodTypes} },
	{ discrete, "Bullet Puff Type",		{&cl_pufftype},			{2.0}, {0.0},	{0.0}, {PuffTypes} },
	{ discrete, "Stretch short skies",	{&r_stretchsky},	   	{2.0}, {0.0},	{0.0}, {OnOff} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ discrete, "Column render mode",	{&r_columnmethod},		{2.0}, {0.0},	{0.0}, {ColumnMethods} },
	{ discrete, "Disable alpha",		{&r_drawtrans},			{2.0}, {0.0},	{0.0}, {NoYes} },
#ifdef _WIN32
	{ discrete,	"Show ENDOOM screen",	{&showendoom},			{3.0}, {0.0},	{0.0}, {Endoom} },
	{ discrete, "DirectDraw palette hack", {&vid_palettehack},	{2.0}, {0.0},	{0.0}, {OnOff} },
	{ discrete, "Use attached surfaces", {&vid_attachedsurfaces},{2.0}, {0.0},	{0.0}, {OnOff} },
#endif

};

// [BB] Moved crosshair selection to the HUD menu.
//#define CROSSHAIR_INDEX 9

menu_t VideoMenu =
{
	"DISPLAY OPTIONS",
	0,
	countof(VideoItems),
	0,
	VideoItems,
};

/*=======================================
 *
 * HUD Menu [RC]
 *
 *=======================================*/

EXTERN_CVAR (Bool, cl_onekey)
EXTERN_CVAR( Bool, cl_stfullscreenhud )

static value_t VoteScreenTypes[] = {
	{ 0, "Minimal" },
	{ 1, "Fullscreen" },
};

static value_t FullscreenHUDStyle[] = {
	{ 0, "Classic style" },
	{ 1, "New style" },
};


static menuitem_t HUDMenuItems[] = {
	#ifdef G15_ENABLED
		{ more,		"Logitech G15",				{NULL},					{0.0}, {0.0},	{0.0}, {(value_t *)StartG15Menu} },
	#endif
	{ slider,	"Screen size",				{&screenblocks},	   		{3.0}, {12.0},	{1.0}, {NULL} },
	{ discrete, "Voting display",			{&cl_showfullscreenvote},	{2.0}, {0.0},	{0.0}, {VoteScreenTypes} },
	{ discrete, "Fullscreen HUD",			{&cl_stfullscreenhud},		{2.0}, {0.0},	{0.0}, {FullscreenHUDStyle} },
	{ discrete, "Stretch status bar",		{&st_scale},				{2.0}, {0.0},	{0.0}, {OnOff} },
	{ redtext,	" ",						{NULL},						{0.0}, {0.0},	{0.0}, {NULL} },
	{ discretes,"Crosshair",				{&crosshair},			   	{8.0}, {0.0},	{0.0}, {NULL} },
	{ discrete, "Large frag messages",		{&cl_showlargefragmessages},{2.0}, {0.0},	{0.0}, {YesNo} },
//	{ discrete, "GZDoom HUD",				{&hud_althud},				{2.0}, {0.0},	{0.0}, {OnOff} },
	{ discrete, "One key display",			{&cl_onekey},				{2.0}, {0.0},	{0.0}, {YesNo} },
	{ discrete, "Draw coop info",		{&cl_drawcoopinfo},{2.0}, {0.0},	{0.0}, {YesNo} },


};

#ifdef G15_ENABLED
#define CROSSHAIR_INDEX 6
#else
#define CROSSHAIR_INDEX 5
#endif

menu_t HUDMenu =
{
	"HUD OPTIONS",
	0,
	countof(HUDMenuItems),
	0,
	HUDMenuItems,
};


/*=======================================
 *
 * G15 Menu [RC]
 *
 *=======================================*/
#ifdef G15_ENABLED
	EXTERN_CVAR (Bool, g15_enable)
	EXTERN_CVAR (Bool, g15_showlargefragmessages)

	static menuitem_t G15MenuItems[] = {
		{ discrete, "Enable LCD display",			{&g15_enable},	{2.0}, {0.0},	{0.0}, {YesNo} },
		{ discrete, "Large frag messages",			{&g15_showlargefragmessages},	{2.0}, {0.0},	{0.0}, {OnOff} },

	};

	menu_t G15Menu =
	{
		"G15 DISPLAY",
		0,
		countof(G15MenuItems),
		0,
		G15MenuItems,
	};

	void UpdateG15Menu ()
	{
		if ( !G15_IsDeviceConnected() )
			G15_TryConnect( );

		// Enable/disable the menu if the LCD is present.
		if ( !G15_IsDeviceConnected() )
		{
			G15MenuItems[0].type = redtext;
			G15MenuItems[0].label = "Logitech G15 not connected";
			G15Menu.numitems = 1;
		}
		else
		{
			G15MenuItems[0].type = discrete;
			G15MenuItems[0].label = "Enable LCD display";
			G15Menu.numitems = 2;
		}	
	}

	static void StartG15Menu (void)
	{
		UpdateG15Menu( );
		M_SwitchMenu (&G15Menu);
	}

#endif

/*=======================================
 *
 * Automap Menu
 *
 *=======================================*/
static void StartMapColorsMenu (void);

EXTERN_CVAR (Int, am_rotate)
EXTERN_CVAR (Int, am_overlay)
EXTERN_CVAR (Bool, am_showitems)
EXTERN_CVAR (Bool, am_showmonsters)
EXTERN_CVAR (Bool, am_showsecrets)
EXTERN_CVAR (Bool, am_showtime)
EXTERN_CVAR (Int, am_map_secrets)
EXTERN_CVAR (Bool, am_showtotaltime)
EXTERN_CVAR (Bool, am_drawmapback)

static value_t MapColorTypes[] = {
	{ 0, "Custom" },
	{ 1, "Traditional Doom" },
	{ 2, "Traditional Strife" },
	{ 3, "Traditional Raven" }
};

static value_t SecretTypes[] = {
	{ 0, "Never" },
	{ 1, "Only when found" },
	{ 2, "Always" },
};

static value_t RotateTypes[] = {
	{ 0, "Off" },
	{ 1, "On" },
	{ 2, "On for overlay only" }
};

static value_t OverlayTypes[] = {
	{ 0, "Off" },
	{ 1, "Overlay+Normal" },
	{ 2, "Overlay Only" }
};

static menuitem_t AutomapItems[] = {
	{ discrete, "Map color set",		{&am_colorset},			{4.0}, {0.0},	{0.0}, {MapColorTypes} },
	{ more,		"Set custom colors",	{NULL},					{0.0}, {0.0},	{0.0}, {(value_t*)StartMapColorsMenu} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ discrete, "Rotate automap",		{&am_rotate},		   	{3.0}, {0.0},	{0.0}, {RotateTypes} },
	{ discrete, "Overlay automap",		{&am_overlay},			{3.0}, {0.0},	{0.0}, {OverlayTypes} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ discrete, "Show item counts",		{&am_showitems},		{2.0}, {0.0},	{0.0}, {OnOff} },
	{ discrete, "Show monster counts",	{&am_showmonsters},		{2.0}, {0.0},	{0.0}, {OnOff} },
	{ discrete, "Show secret counts",	{&am_showsecrets},		{2.0}, {0.0},	{0.0}, {OnOff} },
	{ discrete, "Show time elapsed",	{&am_showtime},			{2.0}, {0.0},	{0.0}, {OnOff} },
	{ discrete, "Show total time elapsed",	{&am_showtotaltime},	{2.0}, {0.0},	{0.0}, {OnOff} },
	{ discrete, "Show secrets on map",		{&am_map_secrets},		{3.0}, {0.0},	{0.0}, {SecretTypes} },
	{ discrete, "Draw map background",	{&am_drawmapback},		{2.0}, {0.0},	{0.0}, {OnOff} },
};

menu_t AutomapMenu =
{
	"AUTOMAP OPTIONS",
	0,
	countof(AutomapItems),
	0,
	AutomapItems,
};

/*=======================================
 *
 * Map Colors Menu
 *
 *=======================================*/
static void DefaultCustomColors();

EXTERN_CVAR (Color, am_backcolor)
EXTERN_CVAR (Color, am_yourcolor)
EXTERN_CVAR (Color, am_wallcolor)
EXTERN_CVAR (Color, am_secretwallcolor)
EXTERN_CVAR (Color, am_tswallcolor)
EXTERN_CVAR (Color, am_fdwallcolor)
EXTERN_CVAR (Color, am_cdwallcolor)
EXTERN_CVAR (Color, am_thingcolor)
EXTERN_CVAR (Color, am_gridcolor)
EXTERN_CVAR (Color, am_xhaircolor)
EXTERN_CVAR (Color, am_notseencolor)
EXTERN_CVAR (Color, am_lockedcolor)
EXTERN_CVAR (Color, am_ovyourcolor)
EXTERN_CVAR (Color, am_ovwallcolor)
EXTERN_CVAR (Color, am_ovthingcolor)
EXTERN_CVAR (Color, am_ovotherwallscolor)
EXTERN_CVAR (Color, am_ovunseencolor)
EXTERN_CVAR (Color, am_ovtelecolor)
EXTERN_CVAR (Color, am_intralevelcolor)
EXTERN_CVAR (Color, am_interlevelcolor)
EXTERN_CVAR (Color, am_secretsectorcolor)
EXTERN_CVAR (Color, am_thingcolor_friend)
EXTERN_CVAR (Color, am_thingcolor_monster)
EXTERN_CVAR (Color, am_thingcolor_item)
EXTERN_CVAR (Color, am_ovthingcolor_friend)
EXTERN_CVAR (Color, am_ovthingcolor_monster)
EXTERN_CVAR (Color, am_ovthingcolor_item)

static menuitem_t MapColorsItems[] = {
	{ rsafemore,   "Restore default custom colors",				{NULL},					{0}, {0}, {0}, {(value_t*)DefaultCustomColors} },
	{ redtext,	   " ",											{NULL},					{0}, {0}, {0}, {0} },
	{ colorpicker, "Background",								{&am_backcolor},		{0}, {0}, {0}, {0} },
	{ colorpicker, "You",										{&am_yourcolor},		{0}, {0}, {0}, {0} },
	{ colorpicker, "1-sided walls",								{&am_wallcolor},		{0}, {0}, {0}, {0} },
	{ colorpicker, "2-sided walls with different floors",		{&am_fdwallcolor},		{0}, {0}, {0}, {0} },
	{ colorpicker, "2-sided walls with different ceilings",		{&am_cdwallcolor},		{0}, {0}, {0}, {0} },
	{ colorpicker, "Map grid",									{&am_gridcolor},		{0}, {0}, {0}, {0} },
	{ colorpicker, "Center point",								{&am_xhaircolor},		{0}, {0}, {0}, {0} },
	{ colorpicker, "Not-yet-seen walls",						{&am_notseencolor},		{0}, {0}, {0}, {0} },
	{ colorpicker, "Locked doors",								{&am_lockedcolor},		{0}, {0}, {0}, {0} },
	{ colorpicker, "Teleporter to the same map",				{&am_intralevelcolor},	{0}, {0}, {0}, {0} },
	{ colorpicker, "Teleporter to a different map",				{&am_interlevelcolor},	{0}, {0}, {0}, {0} },
	{ colorpicker, "Secret sector",								{&am_secretsectorcolor},	{0}, {0}, {0}, {0} },
	{ redtext,		" ",										{NULL},					{0}, {0}, {0}, {0} },
	{ colorpicker, "Invisible 2-sided walls (for cheat)",		{&am_tswallcolor},		{0}, {0}, {0}, {0} },
	{ colorpicker, "Secret walls (for cheat)",					{&am_secretwallcolor},	{0}, {0}, {0}, {0} },
	{ colorpicker, "Actors (for cheat)",						{&am_thingcolor},		{0}, {0}, {0}, {0} },
	{ colorpicker, "Monsters (for cheat)",						{&am_thingcolor_monster},		{0}, {0}, {0}, {0} },
	{ colorpicker, "Friends (for cheat)",						{&am_thingcolor_friend},		{0}, {0}, {0}, {0} },
	{ colorpicker, "Items (for cheat)",							{&am_thingcolor_item},			{0}, {0}, {0}, {0} },
	{ redtext,		" ",										{NULL},					{0}, {0}, {0}, {0} },
	{ colorpicker, "You (overlay)",								{&am_ovyourcolor},		{0}, {0}, {0}, {0} },
	{ colorpicker, "1-sided walls (overlay)",					{&am_ovwallcolor},		{0}, {0}, {0}, {0} },
	{ colorpicker, "2-sided walls (overlay)",					{&am_ovotherwallscolor},{0}, {0}, {0}, {0} },
	{ colorpicker, "Not-yet-seen walls (overlay)",				{&am_ovunseencolor},	{0}, {0}, {0}, {0} },
	{ colorpicker, "Teleporter (overlay)",						{&am_ovtelecolor},		{0}, {0}, {0}, {0} },
	{ colorpicker, "Actors (overlay) (for cheat)",				{&am_ovthingcolor},		{0}, {0}, {0}, {0} },
	{ colorpicker, "Monsters (overlay) (for cheat)",			{&am_ovthingcolor_monster},		{0}, {0}, {0}, {0} },
	{ colorpicker, "Friends (overlay) (for cheat)",				{&am_ovthingcolor_friend},		{0}, {0}, {0}, {0} },
	{ colorpicker, "Items (overlay) (for cheat)",				{&am_ovthingcolor_item},		{0}, {0}, {0}, {0} },
};

menu_t MapColorsMenu =
{
	"CUSTOMIZE MAP COLORS",
	0,
	countof(MapColorsItems),
	48,
	MapColorsItems,
};

/*=======================================
 *
 * Color Picker Sub-menu
 *
 *=======================================*/
static void StartColorPickerMenu (const char *colorname, FColorCVar *cvar);
static void ColorPickerReset ();
static int CurrColorIndex;
static int SelColorIndex;
static void UpdateSelColor (int index);


static menuitem_t ColorPickerItems[] = {
	{ redtext,		NULL,					{NULL},		{0},  {0},   {0},  {0} },
	{ redtext,		" ",					{NULL},		{0},  {0},   {0},  {0} },
	{ intslider,	"Red",					{NULL},		{0},  {255}, {15}, {0} },
	{ intslider,	"Green",				{NULL},		{0},  {255}, {15}, {0} },
	{ intslider,	"Blue",					{NULL},		{0},  {255}, {15}, {0} },
	{ redtext,		" ",					{NULL},		{0},  {0},   {0},  {0} },
	{ more,			"Undo changes",			{NULL},		{0},  {0},   {0},  {(value_t*)ColorPickerReset} },
	{ redtext,		" ",					{NULL},		{0},  {0},   {0},  {0} },
	{ palettegrid,	" ",					{NULL},		{0},  {0},   {0},  {0} },
	{ palettegrid,	" ",					{NULL},		{1},  {0},   {0},  {0} },
	{ palettegrid,	" ",					{NULL},		{2},  {0},   {0},  {0} },
	{ palettegrid,	" ",					{NULL},		{3},  {0},   {0},  {0} },
	{ palettegrid,	" ",					{NULL},		{4},  {0},   {0},  {0} },
	{ palettegrid,	" ",					{NULL},		{5},  {0},   {0},  {0} },
	{ palettegrid,	" ",					{NULL},		{6},  {0},   {0},  {0} },
	{ palettegrid,	" ",					{NULL},		{7},  {0},   {0},  {0} },
	{ palettegrid,	" ",					{NULL},		{8},  {0},   {0},  {0} },
	{ palettegrid,	" ",					{NULL},		{9},  {0},   {0},  {0} },
	{ palettegrid,	" ",					{NULL},		{10}, {0},   {0},  {0} },
	{ palettegrid,	" ",					{NULL},		{11}, {0},   {0},  {0} },
	{ palettegrid,	" ",					{NULL},		{12}, {0},   {0},  {0} },
	{ palettegrid,	" ",					{NULL},		{13}, {0},   {0},  {0} },
	{ palettegrid,	" ",					{NULL},		{14}, {0},   {0},  {0} },
	{ palettegrid,	" ",					{NULL},		{15}, {0},   {0},  {0} },
};

menu_t ColorPickerMenu =
{
	"SELECT COLOR",
	2,
	countof(ColorPickerItems),
	0,
	ColorPickerItems,
};

/*=======================================
 *
 * Messages Menu
 *
 *=======================================*/
void	SetupTextScalingMenu( void );

EXTERN_CVAR (Bool, con_scaletext)
EXTERN_CVAR (Bool, con_centernotify)
EXTERN_CVAR (Int,  msg0color)
EXTERN_CVAR (Int,  msg1color)
EXTERN_CVAR (Int,  msg2color)
EXTERN_CVAR (Int,  msg3color)
EXTERN_CVAR (Int,  msg4color)
EXTERN_CVAR (Int,  msgmidcolor)
EXTERN_CVAR (Int,  msglevel)
EXTERN_CVAR (Int, con_colorinmessages)

static value_t ScaleValues[] =
{
	{ 0.0, "Off" },
	{ 1.0, "On" },
	{ 2.0, "Double" }
};

static value_t TextColors[] =
{
	{ 0.0, "brick" },
	{ 1.0, "tan" },
	{ 2.0, "gray" },
	{ 3.0, "green" },
	{ 4.0, "brown" },
	{ 5.0, "gold" },
	{ 6.0, "red" },
	{ 7.0, "blue" },
	{ 8.0, "orange" },
	{ 9.0, "white" },
	{ 10.0, "yellow" },
	{ 11.0, "default" },
	{ 12.0, "black" },
	{ 13.0, "light blue" },
	{ 14.0, "cream" },
	{ 15.0, "olive" },
	{ 16.0, "dark green" },
	{ 17.0, "dark red" },
	{ 18.0, "dark brown" },
	{ 19.0, "purple" },
	{ 20.0, "dark gray" },
};

static value_t MessageLevels[] = {
	{ 0.0, "Item Pickup" },
	{ 1.0, "Obituaries" },
	{ 2.0, "Critical Messages" }
};

static value_t MessageColorLevels[] = {
	{ 0.0, "Off" },
	{ 1.0, "On" },
	{ 2.0, "Not in chat" }
};

static value_t ChatSounds[] = {
	{ 0.0, "Off" },
	{ 1.0, "Default" },
	{ 2.0, "Doom 1" },
	{ 3.0, "Doom 2" }
};


static menuitem_t MessagesItems[] = {
	{ more,		"Text scaling",			{NULL},					{0.0}, {0.0}, 	{0.0}, {(value_t *)SetupTextScalingMenu} },
	{ discrete, "Show messages",		{&show_messages},		{2.0}, {0.0},   {0.0}, {OnOff} },
	{ discrete, "Show obituaries",		{&show_obituaries},		{2.0}, {0.0},   {0.0}, {OnOff} },
	{ discrete, "Chat sound",			{&chat_sound},			{4.0}, {0.0},   {0.0}, {ChatSounds} },
	{ discrete, "Minimum message level",{&msglevel},		   	{3.0}, {0.0},   {0.0}, {MessageLevels} },
	{ discrete, "Center messages",		{&con_centernotify},	{2.0}, {0.0},	{0.0}, {OnOff} },
	{ discrete, "Color in messages",	{&con_colorinmessages},	{3.0}, {0.0},	{0.0}, {MessageColorLevels} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ whitetext, "Message Colors",		{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ cdiscrete, "Item Pickup",			{&msg0color},		   	{21.0}, {0.0},	{0.0}, {TextColors} },
	{ cdiscrete, "Obituaries",			{&msg1color},		   	{21.0}, {0.0},	{0.0}, {TextColors} },
	{ cdiscrete, "Critical Messages",	{&msg2color},		   	{21.0}, {0.0},	{0.0}, {TextColors} },
	{ cdiscrete, "Chat Messages",		{&msg3color},		   	{21.0}, {0.0},	{0.0}, {TextColors} },
	{ cdiscrete, "Team Messages",		{&msg4color},		   	{21.0}, {0.0},	{0.0}, {TextColors} },
	{ cdiscrete, "Centered Messages",	{&msgmidcolor},			{21.0}, {0.0},	{0.0}, {TextColors} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ discrete, "Screenshot messages",	{&screenshot_quiet},	{2.0}, {0.0},	{0.0}, {OffOn} },
	{ discrete, "Detailed save messages",{&longsavemessages},	{2.0}, {0.0},	{0.0}, {OnOff} },
};

menu_t MessagesMenu =
{
	"MESSAGES",
	0,
	countof(MessagesItems),
	0,
	MessagesItems,
};


/*=======================================
 *
 * Video Modes Menu
 *
 *=======================================*/

extern bool setmodeneeded;
extern int NewWidth, NewHeight, NewBits;
extern int DisplayBits;

int testingmode;		// Holds time to revert to old mode
int OldWidth, OldHeight, OldBits;

void M_FreeModesList ();
static void BuildModesList (int hiwidth, int hiheight, int hi_id);
static bool GetSelectedSize (int line, int *width, int *height);
static void SetModesMenu (int w, int h, int bits);

EXTERN_CVAR (Int, vid_defwidth)
EXTERN_CVAR (Int, vid_defheight)
EXTERN_CVAR (Int, vid_defbits)

static FIntCVar DummyDepthCvar (NULL, 0, 0);

EXTERN_CVAR (Bool, fullscreen)

static value_t Depths[22];

EXTERN_CVAR (Bool, vid_tft)		// Defined below
CUSTOM_CVAR (Int, menu_screenratios, 0, CVAR_ARCHIVE)
{
	// [BC] No need to do this in server mode.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		return;

	if (self < 0 || self > 4)
	{
		self = 3;
	}
	else if (self == 4 && !vid_tft)
	{
		self = 3;
	}
	else
	{
		BuildModesList (SCREENWIDTH, SCREENHEIGHT, DisplayBits);
	}
}

static value_t Ratios[] =
{
	{ 0.0, "4:3" },
	{ 1.0, "16:9" },
	{ 2.0, "16:10" },
	{ 3.0, "All" }
};
static value_t RatiosTFT[] =
{
	{ 0.0, "4:3" },
	{ 4.0, "5:4" },
	{ 1.0, "16:9" },
	{ 2.0, "16:10" },
	{ 3.0, "All" }
};

static char VMEnterText[] = "Press ENTER to set mode";
static char VMTestText[] = "T to test mode for 5 seconds";

static menuitem_t ModesItems[] = {
//	{ discrete, "Screen mode",			{&DummyDepthCvar},		{0.0}, {0.0},	{0.0}, {Depths} },
	{ discrete, "Aspect ratio",			{&menu_screenratios},	{4.0}, {0.0},	{0.0}, {Ratios} },
#ifndef NO_GL
	{ discrete,	"Renderer",				{&vid_renderer},		{2.0}, {0.0},	{0.0}, {Renderers} }, // [ZDoomGL]
#else
	// Keep array size the same
	{ redtext,	" ",					{NULL},					{0.0},	{0.0},	{0.0}, {NULL} },
#endif
	{ discrete, "Fullscreen",			{&fullscreen},			{2.0}, {0.0},	{0.0}, {YesNo} },
	{ discrete, "Enable 5:4 aspect ratio",{&vid_tft},			{2.0}, {0.0},	{0.0}, {YesNo} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ screenres,NULL,					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ screenres,NULL,					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ screenres,NULL,					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ screenres,NULL,					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ screenres,NULL,					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ screenres,NULL,					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ screenres,NULL,					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ screenres,NULL,					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ screenres,NULL,					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ screenres,NULL,					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
//	{ whitetext,"Note: Only 8 bpp modes are supported",{NULL},	{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,  VMEnterText,			{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,  VMTestText,				{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
};

#define VM_DEPTHITEM	0
#define VM_ASPECTITEM	0
// [BC] Edited due to addition of "Renderer" line.
#define VM_RESSTART		5
#define VM_ENTERLINE	15
#define VM_TESTLINE		17
// [BC] End of changes.

menu_t ModesMenu =
{
	"VIDEO MODE",
	2,
	countof(ModesItems),
	0,
	ModesItems,
};

CUSTOM_CVAR (Bool, vid_tft, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	// [BC] No need to do this in server mode.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		return;

	if (self)
	{
		ModesItems[VM_ASPECTITEM].b.numvalues = 5.f;
		ModesItems[VM_ASPECTITEM].e.values = RatiosTFT;
	}
	else
	{
		ModesItems[VM_ASPECTITEM].b.numvalues = 4.f;
		ModesItems[VM_ASPECTITEM].e.values = Ratios;
		if (menu_screenratios == 4)
		{
			menu_screenratios = 0;
		}
	}
}

/*=======================================
 *
 * Gameplay Options (dmflags) Menu
 *
 *=======================================*/

value_t SmartAim[4] = {
	{ 0.0, "Off" },
	{ 1.0, "On" },
	{ 2.0, "Never friends" },
	{ 3.0, "Only monsters" }
};

value_t DF_Jump[3] = {
	{ 0, "Default" },
	{ DF_NO_JUMP, "Off" },
	{ DF_YES_JUMP, "On" }
};

value_t DF_Crouch[3] = {
	{ 0, "Default" },
	{ DF_NO_CROUCH, "Off" },
	{ DF_YES_CROUCH, "On" }
};

static menuitem_t DMFlagsItems[] = {
	{ discrete, "Teamplay",				{&teamplay},	{2.0}, {0.0}, {0.0}, {OnOff} },
	{ slider,	"Team damage scalar",	{&teamdamage},	{0.0}, {1.0}, {0.05},{NULL} },
	{ redtext,	" ",					{NULL},			{0.0}, {0.0}, {0.0}, {NULL} },
	{ discrete, "Smart Autoaim",		{&sv_smartaim},	{4.0}, {0.0}, {0.0}, {SmartAim} },
	{ bitflag,	"Falling damage (old)",	{&dmflags},		{0}, {0}, {0}, {(value_t *)DF_FORCE_FALLINGZD} },
	{ bitflag,	"Falling damage (Hexen)",{&dmflags},	{0}, {0}, {0}, {(value_t *)DF_FORCE_FALLINGHX} },
	{ bitflag,	"Weapons stay (DM)",	{&dmflags},		{0}, {0}, {0}, {(value_t *)DF_WEAPONS_STAY} },
	{ bitflag,	"Allow powerups (DM)",	{&dmflags},		{1}, {0}, {0}, {(value_t *)DF_NO_ITEMS} },
	{ bitflag,	"Allow health (DM)",	{&dmflags},		{1}, {0}, {0}, {(value_t *)DF_NO_HEALTH} },
	{ bitflag,	"Allow armor (DM)",		{&dmflags},		{1}, {0}, {0}, {(value_t *)DF_NO_ARMOR} },
	{ bitflag,	"Spawn farthest (DM)",	{&dmflags},		{0}, {0}, {0}, {(value_t *)DF_SPAWN_FARTHEST} },
	{ bitflag,	"Same map (DM)",		{&dmflags},		{0}, {0}, {0}, {(value_t *)DF_SAME_LEVEL} },
	{ bitflag,	"Force respawn (DM)",	{&dmflags},		{0}, {0}, {0}, {(value_t *)DF_FORCE_RESPAWN} },
	{ bitflag,	"Allow exit (DM)",		{&dmflags},		{1}, {0}, {0}, {(value_t *)DF_NO_EXIT} },
	{ bitflag,	"Barrels respawn (DM)",	{&dmflags2},	{0}, {0}, {0}, {(value_t *)DF2_BARRELS_RESPAWN} },
	{ bitflag,	"No respawn protection (DM)",{&dmflags2},	{0}, {0}, {0}, {(value_t *)DF2_NO_RESPAWN_INVUL} },
	{ bitflag,	"Drop weapon",			{&dmflags2},	{0}, {0}, {0}, {(value_t *)DF2_YES_WEAPONDROP} },
	{ bitflag,	"Double ammo",			{&dmflags2},	{0}, {0}, {0}, {(value_t *)DF2_YES_DOUBLEAMMO} },
	{ bitflag,	"Infinite ammo",		{&dmflags},		{0}, {0}, {0}, {(value_t *)DF_INFINITE_AMMO} },
	{ bitflag,	"Infinite inventory",	{&dmflags2},	{0}, {0}, {0}, {(value_t *)DF2_INFINITE_INVENTORY} },
	{ bitflag,	"No monsters",			{&dmflags},		{0}, {0}, {0}, {(value_t *)DF_NO_MONSTERS} },
	{ bitflag,	"No monsters to exit",	{&dmflags2},	{0}, {0}, {0}, {(value_t *)DF2_KILL_MONSTERS} },
	{ bitflag,	"Monsters respawn",		{&dmflags},		{0}, {0}, {0}, {(value_t *)DF_MONSTERS_RESPAWN} },
	{ bitflag,	"No respawn",			{&dmflags2},	{0}, {0}, {0}, {(value_t *)DF2_NO_RESPAWN} },
	{ bitflag,	"Items respawn",		{&dmflags},		{0}, {0}, {0}, {(value_t *)DF_ITEMS_RESPAWN} },
	{ bitflag,	"Mega powerups respawn",{&dmflags},		{0}, {0}, {0}, {(value_t *)DF_RESPAWN_SUPER} },
	{ bitflag,	"Fast monsters",		{&dmflags},		{0}, {0}, {0}, {(value_t *)DF_FAST_MONSTERS} },
	{ bitflag,	"Degeneration",			{&dmflags2},	{0}, {0}, {0}, {(value_t *)DF2_YES_DEGENERATION} },
	{ bitflag,	"Allow Autoaim",		{&dmflags2},	{1}, {0}, {0}, {(value_t *)DF2_NOAUTOAIM} },
	{ bitflag,	"Disallow Suicide",		{&dmflags2},	{1}, {0}, {0}, {(value_t *)DF2_NOSUICIDE} },
	{ bitmask,	"Allow jump",			{&dmflags},		{3.0}, {DF_NO_JUMP|DF_YES_JUMP}, {0}, {DF_Jump} },
	{ bitmask,	"Allow crouch",			{&dmflags},		{3.0}, {DF_NO_CROUCH|DF_YES_CROUCH}, {0}, {DF_Crouch} },
	{ bitflag,	"Allow freelook",		{&dmflags},		{1}, {0}, {0}, {(value_t *)DF_NO_FREELOOK} },
	{ bitflag,	"Allow FOV",			{&dmflags},		{1}, {0}, {0}, {(value_t *)DF_NO_FOV} },
	{ bitflag,	"Allow BFG aiming",		{&dmflags2},	{0}, {0}, {0}, {(value_t *)DF2_YES_FREEAIMBFG} },
	{ bitflag,	"Allow automap",		{&dmflags2},	{1}, {0}, {0}, {(value_t *)DF2_NO_AUTOMAP} },
	{ bitflag,	"Automap allies",		{&dmflags2},	{1}, {0}, {0}, {(value_t *)DF2_NO_AUTOMAP_ALLIES} },
	{ bitflag,	"Allow spying",			{&dmflags2},	{1}, {0}, {0}, {(value_t *)DF2_DISALLOW_SPYING} },
	{ bitflag,	"Don't spawn runes (DM)",	{&dmflags2},	{0}, {0}, {0}, {(value_t *)DF2_NO_RUNES} },
	{ bitflag,	"Instant flag return (ST/CTF)",	{&dmflags2},	{0}, {0}, {0}, {(value_t *)DF2_INSTANT_RETURN} },
	{ bitflag,	"Server picks teams (ST/CTF)",	{&dmflags2},	{0}, {0}, {0}, {(value_t *)DF2_NO_TEAM_SELECT} },
	{ bitflag,	"Chasecam cheat",		{&dmflags2},	{0}, {0}, {0}, {(value_t *)DF2_CHASECAM} },

	{ bitflag,	"Lose frag if fragged",	{&dmflags2},	{0}, {0}, {0}, {(value_t *)DF2_YES_LOSEFRAG} },
	{ bitflag,	"Keep frags gained",	{&dmflags2},	{0}, {0}, {0}, {(value_t *)DF2_YES_KEEPFRAGS} },
	{ bitflag,	"No team switching",	{&dmflags2},	{0}, {0}, {0}, {(value_t *)DF2_NO_TEAM_SWITCH} },

	{ redtext,	" ",					{NULL},			{0}, {0}, {0}, {NULL} },
	{ whitetext,"Cooperative Settings",	{NULL},			{0}, {0}, {0}, {NULL} },
	{ bitflag,	"Spawn multi. weapons", {&dmflags},		{1}, {0}, {0}, {(value_t *)DF_NO_COOP_WEAPON_SPAWN} },
	{ bitflag,	"Lose entire inventory",{&dmflags},		{0}, {0}, {0}, {(value_t *)DF_COOP_LOSE_INVENTORY} },
	{ bitflag,	"Keep keys",			{&dmflags},		{1}, {0}, {0}, {(value_t *)DF_COOP_LOSE_KEYS} },
	{ bitflag,	"Keep weapons",			{&dmflags},		{1}, {0}, {0}, {(value_t *)DF_COOP_LOSE_WEAPONS} },
	{ bitflag,	"Keep armor",			{&dmflags},		{1}, {0}, {0}, {(value_t *)DF_COOP_LOSE_ARMOR} },
	{ bitflag,	"Keep powerups",		{&dmflags},		{1}, {0}, {0}, {(value_t *)DF_COOP_LOSE_POWERUPS} },
	{ bitflag,	"Keep ammo",			{&dmflags},		{1}, {0}, {0}, {(value_t *)DF_COOP_LOSE_AMMO} },
	{ bitflag,	"Lose half ammo",		{&dmflags},		{0}, {0}, {0}, {(value_t *)DF_COOP_HALVE_AMMO} },
	{ bitflag,	"Spawn where died",		{&dmflags2},	{0}, {0}, {0}, {(value_t *)DF2_SAME_SPAWN_SPOT} },
	{ bitflag,	"Start with shotgun",	{&dmflags2},	{0}, {0}, {0}, {(value_t *)DF2_COOP_SHOTGUNSTART} },
};

static menu_t DMFlagsMenu =
{
	"GAMEPLAY OPTIONS",
	0,
	countof(DMFlagsItems),
	0,
	DMFlagsItems,
};

/*=======================================
 *
 * Compatibility Options Menu
 *
 *=======================================*/

static menuitem_t CompatibilityItems[] = {
	{ discrete, "Compatibility mode",						{&compatmode},	{5.0}, {1.0},	{0.0}, {CompatModes} },
	{ redtext,	" ",					{NULL},			{0.0}, {0.0}, {0.0}, {NULL} },
	{ bitflag,	"Find shortest textures like Doom",			{&compatflags}, {0}, {0}, {0}, {(value_t *)COMPATF_SHORTTEX} },
	{ bitflag,	"Use buggier stair building",				{&compatflags}, {0}, {0}, {0}, {(value_t *)COMPATF_STAIRINDEX} },
	{ bitflag,	"Limit Pain Elementals' Lost Souls",		{&compatflags}, {0}, {0}, {0}, {(value_t *)COMPATF_LIMITPAIN} },
	{ bitflag,	"Don't let others hear your pickups",		{&compatflags}, {0}, {0}, {0}, {(value_t *)COMPATF_SILENTPICKUP} },
	{ bitflag,	"Actors are infinitely tall",				{&compatflags}, {0}, {0}, {0}, {(value_t *)COMPATF_NO_PASSMOBJ} },
	{ bitflag,	"Cripple sound for silent BFG trick",		{&compatflags}, {0}, {0}, {0}, {(value_t *)COMPATF_MAGICSILENCE} },
	{ bitflag,	"Enable wall running",						{&compatflags}, {0}, {0}, {0}, {(value_t *)COMPATF_WALLRUN} },
	{ bitflag,	"Spawn item drops on the floor",			{&compatflags}, {0}, {0}, {0}, {(value_t *)COMPATF_NOTOSSDROPS} },
	{ bitflag,  "All special lines can block <use>",		{&compatflags}, {0}, {0}, {0}, {(value_t *)COMPATF_USEBLOCKING} },
	{ bitflag,	"Disable BOOM door light effect",			{&compatflags}, {0}, {0}, {0}, {(value_t *)COMPATF_NODOORLIGHT} },
	{ bitflag,	"Raven scrollers use original speed",		{&compatflags}, {0}, {0}, {0}, {(value_t *)COMPATF_RAVENSCROLL} },
	{ bitflag,	"Use original sound target handling",		{&compatflags}, {0}, {0}, {0}, {(value_t *)COMPATF_SOUNDTARGET} },
	{ bitflag,	"DEH health settings like Doom2.exe",		{&compatflags}, {0}, {0}, {0}, {(value_t *)COMPATF_DEHHEALTH} },
	{ bitflag,	"Self ref. sectors don't block shots",		{&compatflags}, {0}, {0}, {0}, {(value_t *)COMPATF_TRACE} },
	{ bitflag,	"Monsters get stuck over dropoffs",			{&compatflags}, {0}, {0}, {0}, {(value_t *)COMPATF_DROPOFF} },
	{ bitflag,	"Monsters cannot cross dropoffs",			{&compatflags}, {0}, {0}, {0}, {(value_t *)COMPATF_CROSSDROPOFF} },
	{ bitflag,	"Monsters see invisible players",			{&compatflags}, {0}, {0}, {0}, {(value_t *)COMPATF_INVISIBILITY} },
	{ bitflag,	"Boom scrollers are additive",				{&compatflags}, {0}, {0}, {0}, {(value_t *)COMPATF_BOOMSCROLL} },
	{ bitflag,	"Inst. moving floors are not silent",		{&compatflags}, {0}, {0}, {0}, {(value_t *)COMPATF_SILENT_INSTANT_FLOORS} },
	{ bitflag,  "Sector sounds use center as source",		{&compatflags}, {0}, {0}, {0}, {(value_t *)COMPATF_SECTORSOUNDS} },
	{ bitflag,  "Use Doom heights for missile clipping",	{&compatflags}, {0}, {0}, {0}, {(value_t *)COMPATF_MISSILECLIP} },
	{ bitflag,	"Limited movement in the air",				{&compatflags}, {0}, {0}, {0}, {(value_t *)COMPATF_LIMITED_AIRMOVEMENT} },
	{ bitflag,	"Plasma bump bug",							{&compatflags}, {0}, {0}, {0}, {(value_t *)COMPATF_PLASMA_BUMP_BUG} },
	{ bitflag,	"Allow instant respawn",					{&compatflags}, {0}, {0}, {0}, {(value_t *)COMPATF_INSTANTRESPAWN} },
	{ bitflag,	"Disable taunts",							{&compatflags}, {0}, {0}, {0}, {(value_t *)COMPATF_DISABLETAUNTS} },
	{ bitflag,	"Original sound curve",						{&compatflags}, {0}, {0}, {0}, {(value_t *)COMPATF_ORIGINALSOUNDCURVE} },
	{ bitflag,	"Use old intermission screens/music",		{&compatflags}, {0}, {0}, {0}, {(value_t *)COMPATF_OLDINTERMISSION} },
	{ bitflag,	"Disable stealth monsters",					{&compatflags}, {0}, {0}, {0}, {(value_t *)COMPATF_DISABLESTEALTHMONSTERS} },
//	{ bitflag,	"Disable cooperative backpacks",			{&compatflags}, {0}, {0}, {0}, {(value_t *)COMPATF_DISABLECOOPERATIVEBACKPACKS} },

	{ discrete, "Interpolate monster movement",	{&nomonsterinterpolation},		{2.0}, {0.0},	{0.0}, {NoYes} },
};

static menu_t CompatibilityMenu =
{
	"COMPATIBILITY OPTIONS",
	0,
	countof(CompatibilityItems),
	0,
	CompatibilityItems,
};

/*=======================================
 *
 * Sound Options Menu
 *
 *=======================================*/

#ifdef _WIN32
EXTERN_CVAR (Float, snd_movievolume)
#endif
EXTERN_CVAR (Bool, snd_flipstereo)
EXTERN_CVAR (Bool, snd_pitched)
EXTERN_CVAR (String, snd_output_format)
EXTERN_CVAR (String, snd_speakermode)
EXTERN_CVAR (String, snd_resampler)
EXTERN_CVAR (String, snd_output)
EXTERN_CVAR (Int, snd_buffersize)
EXTERN_CVAR (Int, snd_buffercount)
EXTERN_CVAR (Int, snd_samplerate)
EXTERN_CVAR (Bool, snd_hrtf)
EXTERN_CVAR (Bool, snd_waterreverb)
EXTERN_CVAR (Float, snd_waterlp)
EXTERN_CVAR (Int, snd_mididevice)

static void MakeSoundChanges ();
static void AdvSoundOptions ();
static void ModReplayerOptions ();

static value_t SampleRates[] =
{
	{ 0.f,		"Default" },
	{ 4000.f,	"4000 Hz" },
	{ 8000.f,	"8000 Hz" },
	{ 11025.f,	"11025 Hz" },
	{ 22050.f,	"22050 Hz" },
	{ 32000.f,	"32000 Hz" },
	{ 44100.f,	"44100 Hz" },
	{ 48000.f,	"48000 Hz" }
};

static value_t BufferSizes[] =
{
	{    0.f, "Default" },
	{   64.f, "64 samples" },
	{  128.f, "128 samples" },
	{  256.f, "256 samples" },
	{  512.f, "512 samples" },
	{ 1024.f, "1024 samples" },
	{ 2048.f, "2048 samples" },
	{ 4096.f, "4096 samples" }
};

static value_t BufferCounts[] =
{
	{    0.f, "Default" },
	{    2.f, "2" },
	{    3.f, "3" },
	{    4.f, "4" },
	{    5.f, "5" },
	{    6.f, "6" },
	{    7.f, "7" },
	{    8.f, "8" },
	{    9.f, "9" },
	{   10.f, "10" },
	{   11.f, "11" },
	{   12.f, "12" }
};

static valueenum_t Outputs[] =
{
	{ "Default",		"Default" },
#if defined(_WIN32)
	{ "DirectSound",	"DirectSound" },
	{ "WASAPI",			"Vista WASAPI" },
	{ "ASIO",			"ASIO" },
	{ "WaveOut",		"WaveOut" },
	{ "OpenAL",			"OpenAL (very beta)" },
#elif defined(unix)
	{ "OSS",			"OSS" },
	{ "ALSA",			"ALSA" },
	{ "SDL",			"SDL" },
	{ "ESD",			"ESD" },
#elif defined(__APPLE__)
	{ "Sound Manager",	"Sound Manager" },
	{ "Core Audio",		"Core Audio" },
#endif
	{ "No sound",		"No sound" }
};

static valueenum_t OutputFormats[] =
{
	{ "PCM-8",		"8-bit" },
	{ "PCM-16",		"16-bit" },
	{ "PCM-24",		"24-bit" },
	{ "PCM-32",		"32-bit" },
	{ "PCM-Float",	"32-bit float" }
};

static valueenum_t SpeakerModes[] =
{
	{ "Auto",		"Auto" },
	{ "Mono",		"Mono" },
	{ "Stereo",		"Stereo" },
	{ "Prologic",	"Dolby Prologic Decoder" },
	{ "Quad",		"Quad" },
	{ "Surround",	"5 speakers" },
	{ "5.1",		"5.1 speakers" },
	{ "7.1",		"7.1 speakers" }
};

static valueenum_t Resamplers[] =
{
	{ "NoInterp",	"No interpolation" },
	{ "Linear",		"Linear" },
	{ "Cubic",		"Cubic" },
	{ "Spline",		"Spline" }
};

static menuitem_t SoundItems[] =
{
	{ slider,	"Sounds volume",		{&snd_sfxvolume},		{0.0}, {1.0},	{0.05}, {NULL} },
	{ slider,	"Music volume",			{&snd_musicvolume},		{0.0}, {1.0},	{0.05}, {NULL} },
	{ discrete, "MIDI device",			{&snd_mididevice},		{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ discrete, "Underwater reverb",	{&snd_waterreverb},		{2.0}, {0.0},	{0.0}, {OnOff} },
	{ slider,	"Underwater cutoff",	{&snd_waterlp},			{0.0}, {2000.0},{50.0}, {NULL} },
	{ discrete, "Randomize pitches",	{&snd_pitched},			{2.0}, {0.0},	{0.0}, {OnOff} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ more,		"Restart sound",		{NULL},					{0.0}, {0.0},	{0.0}, {(value_t *)MakeSoundChanges} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ ediscrete,"Output system",		{&snd_output},			{countof(Outputs)}, {0.0}, {0.0}, {(value_t *)Outputs} },
	{ ediscrete,"Output format",		{&snd_output_format},	{5.0}, {0.0},	{0.0}, {(value_t *)OutputFormats} },
	{ ediscrete,"Speaker mode",			{&snd_speakermode},		{8.0}, {0.0},	{0.0}, {(value_t *)SpeakerModes} },
	{ ediscrete,"Resampler",			{&snd_resampler},		{4.0}, {0.0},	{0.0}, {(value_t *)Resamplers} },
	{ discrete, "HRTF filter",			{&snd_hrtf},			{2.0}, {0.0},	{0.0}, {(value_t *)OnOff} },

	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ more,		"Advanced options",		{NULL},					{0.0}, {0.0},	{0.0}, {(value_t *)AdvSoundOptions} },
	{ more,		"Module replayer options", {NULL},				{0.0}, {0.0},	{0.0}, {(value_t *)ModReplayerOptions} },
};

static menu_t SoundMenu =
{
	"SOUND OPTIONS",
	0,
	countof(SoundItems),
	0,
	SoundItems,
};

#define MIDI_DEVICE_ITEM 2

/*=======================================
 *
 * Advanced Sound Options Menu
 *
 *=======================================*/

EXTERN_CVAR (Bool, opl_onechip)

static menuitem_t AdvSoundItems[] =
{
	{ discrete, "Sample rate",			{&snd_samplerate},		{8.0}, {0.0},	{0.0}, {SampleRates} },
	{ discrete, "Buffer size",			{&snd_buffersize},		{8.0}, {0.0},	{0.0}, {BufferSizes} },
	{ discrete, "Buffer count",			{&snd_buffercount},		{12.0}, {0.0},	{0.0}, {BufferCounts} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ whitetext,"OPL Synthesis",		{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ discrete, "Only emulate one OPL chip", {&opl_onechip},	{2.0}, {0.0},	{0.0}, {OnOff} },
};

static menu_t AdvSoundMenu =
{
	"ADVANCED SOUND OPTIONS",
	0,
	countof(AdvSoundItems),
	0,
	AdvSoundItems,
};

/*=======================================
 *
 * Module Replayer Options Menu
 *
 *=======================================*/

EXTERN_CVAR(Bool, mod_dumb)
EXTERN_CVAR(Int, mod_samplerate)
EXTERN_CVAR(Int, mod_volramp)
EXTERN_CVAR(Int, mod_interp)
EXTERN_CVAR(Bool, mod_autochip)
EXTERN_CVAR(Int, mod_autochip_size_force)
EXTERN_CVAR(Int, mod_autochip_size_scan)
EXTERN_CVAR(Int, mod_autochip_scan_threshold)

static value_t ModReplayers[] =
{
	{ 0.0, "FMOD" },
	{ 1.0, "foo_dumb" }
};

static value_t ModInterpolations[] =
{
	{ 0.0, "None" },
	{ 1.0, "Linear" },
	{ 2.0, "Cubic" }
};

static value_t ModVolumeRamps[] =
{
	{ 0.0, "None" },
	{ 1.0, "Logarithmic" },
	{ 2.0, "Linear" },
	{ 3.0, "XM=lin, else none" },
	{ 4.0, "XM=lin, else log" }
};

static menuitem_t ModReplayerItems[] =
{
	{ discrete, "Replayer engine",		{&mod_dumb},			{2.0}, {0.0},	{0.0}, {ModReplayers} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ discrete, "Sample rate",			{&mod_samplerate},		{8.0}, {0.0},	{0.0}, {SampleRates} },
	{ discrete, "Interpolation",		{&mod_interp},			{3.0}, {0.0},	{0.0}, {ModInterpolations} },
	{ discrete, "Volume ramping",		{&mod_volramp},			{5.0}, {0.0},	{0.0}, {ModVolumeRamps} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ discrete, "Chip-o-matic",			{&mod_autochip},		{2.0}, {0.0},	{0.0}, {OnOff} },
	// TODO if the menu system is ever rewritten: Provide a decent
	// mechanism to edit the chip-o-matic settings like you can with
	// the foo_dumb preferences in foobar2000.
};

static menu_t ModReplayerMenu =
{
	"MODULE REPLAYER OPTIONS",
	0,
	countof(ModReplayerItems),
	0,
	ModReplayerItems,
};


//===========================================================================
static void ActivateConfirm (const char *text, void (*func)())
{
	ConfirmItems[0].label = text;
	ConfirmItems[0].e.mfunc = func;
	ConfirmMenu.lastOn = 3;
	M_SwitchMenu (&ConfirmMenu);
}

//====================================================================================
//
// Player Selection Slider
// Used in the "kick player" and "ignore player" menu.
//
//====================================================================================

CVAR( Int, menu_playerslider_idx, 0, 0 );
static TArray<valuestring_t> AvailablePlayers;

//*****************************************************************************
//
bool playerslider_IsValidPlayer( LONG lPlayer, bool bAllowBots )
{
	if ( lPlayer >= MAXPLAYERS )
		return ( false );

	if ( !playeringame[lPlayer] )
		return ( false );

	if ( !bAllowBots && ( players[lPlayer].bIsBot ))
		return ( false );

	if ( lPlayer == consoleplayer )
		return ( false );

	return ( true );
}

//*****************************************************************************
//
void playerslider_BuildList( bool bIncludeBots )
{
	// Build the list of players.
	valuestring_t	value;
	AvailablePlayers.Clear();
	for ( ULONG ulIdx = 0; ulIdx < MAXPLAYERS; ++ulIdx )
	{
		if ( playerslider_IsValidPlayer( ulIdx, bIncludeBots ) )
		{
			value.value = static_cast<float> ( AvailablePlayers.Size() );
			value.name.Format( "%s", players[ulIdx].userinfo.netname );
			AvailablePlayers.Push( value );
		}
	}
	value.value = static_cast<float> ( AvailablePlayers.Size() );
}

//====================================================================================
//
// Multiplayer Vote-->Ignore Player Menu
//
//====================================================================================

CVAR( Int, menu_ignoreplayer_duration, 0, 0 );
CVAR( Int, menu_ignoreplayer_action, 0, 0 );

void ignoreplayermenu_Ignore( void );

//*****************************************************************************
//
value_t ignoreplayer_Durations[4] = {
	{ 0.0, "Indefinitely" },
	{ 10.0, "10 minutes" },
	{ 20.0, "20 minutes" },
	{ 30.0, "30 minutes" },
};

//*****************************************************************************
//
value_t ignoreplayer_Actions[2] = {
	{ 0.0, "Ignore" },
	{ 1.0, "Unignore" },
};

//*****************************************************************************
//
static menuitem_t ignoreplayermenu_Items[] =
{
	{ discretes,"Player",			{&menu_playerslider_idx},		{1.0}, {0.0},	{0.0}, {NULL} },
	{ discrete,	"Duration",			{&menu_ignoreplayer_duration},	{4.0}, {0.0},	{0.0}, {ignoreplayer_Durations} },
	{ redtext,	" ",				{NULL},							{0.0}, {0.0},	{0.0}, {NULL} },
	{ discrete,	"Action",			{&menu_ignoreplayer_action},	{2.0}, {0.0},	{0.0}, {ignoreplayer_Actions} },
	{ more,		"Execute!",			{NULL},							{0.0}, {0.0},	{0.0}, {(value_t *)ignoreplayermenu_Ignore} },
};

// [BB, RC] Line number of the "Player:" entry from ignoreplayermenu_Items. If the line number is changed, the value has to be adjusted.
#define IGNOREPLAYER_SLIDER_LOCATION 0

//*****************************************************************************
//
menu_t IgnorePlayerMenu = {
	"IGNORE A PLAYER",
	0,
	countof(ignoreplayermenu_Items),
	0,
	ignoreplayermenu_Items,
	0,
	0,
};

//*****************************************************************************
//
void ignoreplayermenu_Ignore( void )
{
	// Clean the name of color codes.
	FString Name = AvailablePlayers[menu_playerslider_idx].name.GetChars( );
	V_RemoveColorCodes( Name );
	V_EscapeBacklashes( Name );

	// Execute the command.
	char	szString[256];
	if ( menu_ignoreplayer_action == 0 )
		sprintf( szString, "ignore \"%s\" %d", Name.Left( 96 ).GetChars(), menu_ignoreplayer_duration.GetGenericRep( CVAR_Int ).Int );
	else
		sprintf( szString, "unignore \"%s\"", Name.Left( 96 ).GetChars());

	AddCommandString( szString );
	M_ClearMenus( );
}

//*****************************************************************************
//
void ignoreplayermenu_Show( void )
{
	if ( SERVER_CountPlayers( false ) < 2 )
	{
		M_ClearMenus( );
		M_StartMessage( "There is nobody else here to ignore!\n\npress any key.", NULL, false );
		return;
	}

	// Set up the player selection slider.
	playerslider_BuildList( false );
	ignoreplayermenu_Items[IGNOREPLAYER_SLIDER_LOCATION].b.numvalues = static_cast<float>(AvailablePlayers.Size());
	ignoreplayermenu_Items[IGNOREPLAYER_SLIDER_LOCATION].e.valuestrings = &AvailablePlayers[0];

	M_SwitchMenu( &IgnorePlayerMenu );
}

//====================================================================================
//
// Call Vote-->Kick Player Menu
//
//====================================================================================

CVAR( String, menu_votereason, "", 0 );

void kickplayermenu_Kick( void );

//*****************************************************************************
//
static menuitem_t kickplayermenu_Items[] =
{
	{ discretes,"Player",			{&menu_playerslider_idx},		   	{1.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",				{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ string,	"Reason for kicking:",{&menu_votereason},			{0.0}, {0.0},	{0.0}, {NULL} },
	{ more,		"Kick!",			{NULL},							{0.0}, {0.0},	{0.0}, {(value_t *)kickplayermenu_Kick} },
};

// [BB, RC] Line number of the "Player:" entry from kickplayermenu_Items. If the line number is changed, the value has to be adjusted.
#define KICKPLAYER_SLIDER_LOCATION 0

//*****************************************************************************
//
menu_t KickPlayerMenu = {
	"KICK A PLAYER",
	0,
	countof(kickplayermenu_Items),
	0,
	kickplayermenu_Items,
	0,
	0,
};

//*****************************************************************************
//
void kickplayermenu_Kick( void )
{
	// Clean the name of color codes.
	FString Name = AvailablePlayers[menu_playerslider_idx].name.GetChars( );
	V_RemoveColorCodes( Name );
	V_EscapeBacklashes( Name );

	FString Reason = menu_votereason.GetGenericRep( CVAR_String ).String;
	V_EscapeBacklashes( Reason );

	// Execute the command.
	char	szString[256];
	sprintf( szString, "callvote kick \"%s\" \"%s\"", Name.Left( 96 ).GetChars(), Reason.Left( 25 ).GetChars());
	AddCommandString( szString );
	M_ClearMenus( );
}

//*****************************************************************************
//
void kickplayermenu_Show( void )
{
	if ( SERVER_CountPlayers( false ) < 2 )
	{
		M_ClearMenus( );
		M_StartMessage( "There is nobody else here to kick!\n\npress any key.", NULL, false );
		return;
	}

	// Set up the player selection slider.
	playerslider_BuildList( false );
	kickplayermenu_Items[KICKPLAYER_SLIDER_LOCATION].b.numvalues = static_cast<float>(AvailablePlayers.Size());
	kickplayermenu_Items[KICKPLAYER_SLIDER_LOCATION].e.valuestrings = &AvailablePlayers[0];

	M_SwitchMenu( &KickPlayerMenu );
}

//====================================================================================
//
// Call Vote-->Map Menu
//
//====================================================================================

CVAR( String, menu_mapvotemap, "", 0 );
CVAR( Bool, menu_mapvoteintermission, false, 0 );

//*****************************************************************************
//
void mapvotemenu_Vote( void )
{
	// Sanitize the inputs.
	FString		Map = menu_mapvotemap.GetGenericRep( CVAR_String ).String;
	FString		Reason = menu_votereason.GetGenericRep( CVAR_String ).String;
	V_EscapeBacklashes( Map );
	V_EscapeBacklashes( Reason );

	if ( !Map.Len( ) )
	{
		Printf( "You didn't specify a map!\n" );
		return;
	}

	// Execute the command.
	char		szString[256];
	sprintf( szString, "callvote %s %s \"%s\"", ( menu_mapvoteintermission.GetGenericRep( CVAR_Bool ).Bool ? "changemap" : "map" ), Map.Left( 128 ).GetChars(), Reason.Left( 25 ).GetChars() );
	AddCommandString( szString );
	M_ClearMenus( );
}

//*****************************************************************************
//
static menuitem_t mapvotemenu_Items[] =
{
	{ string,	"Map",				{&menu_mapvotemap},				{0.0}, {0.0},	{0.0}, {NULL} },
	{ discrete, "Intermission",		{&menu_mapvoteintermission},	{2.0}, {0.0},	{0.0}, {YesNo} },	
	{ redtext,	" ",				{NULL},							{0.0}, {0.0},	{0.0}, {NULL} },
	{ string,	"Reason for change:",{&menu_votereason},			{0.0}, {0.0},	{0.0}, {NULL} },
	{ more,		"Vote!",			{NULL},							{0.0}, {0.0},	{0.0}, {(value_t *)mapvotemenu_Vote} },
};

//*****************************************************************************
//
menu_t MapVoteMenu = {
	"CHANGE MAP",
	0,
	countof(mapvotemenu_Items),
	0,
	mapvotemenu_Items,
	0,
	0,
};

//*****************************************************************************
//
void mapvotemenu_Show( void )
{
	M_SwitchMenu( &MapVoteMenu );
}

//====================================================================================
//
// Call Vote-->Limit Menu
//
//====================================================================================

//*****************************************************************************
//
value_t limitvote_Types[5] = {
	{ 0.0, "fraglimit" },
	{ 1.0, "timelimit" },
	{ 2.0, "winlimit" },
	{ 3.0, "duellimit" },
	{ 4.0, "pointlimit" }
};

//*****************************************************************************
//
CVAR( Int, menu_limitvote_type, 0, 0 );
CVAR( String, menu_limitvote_value, "", 0 );

//*****************************************************************************
//
void limitvotemenu_Vote( void )
{
	// Sanitize the inputs.
	int			iVoteType = menu_limitvote_type.GetGenericRep( CVAR_Int ).Int;
	int			iLimit =  atoi( menu_limitvote_value.GetGenericRep( CVAR_String ).String );
	FString		Reason = menu_votereason.GetGenericRep( CVAR_String ).String;
	V_EscapeBacklashes( Reason );

	if ( iVoteType >= 5 || iVoteType < 0 )
		return;

	// Execute the command.
	char		szString[512];
	sprintf( szString, "callvote %s %d \"%s\"", limitvote_Types[iVoteType].name, iLimit, Reason.Left( 25 ).GetChars() );
	AddCommandString( szString );
	M_ClearMenus( );
}

//*****************************************************************************
//
static menuitem_t limitvotemenu_Items[] =
{
	{ discrete,	"Type of limit",	{&menu_limitvote_type},			{5.0}, {0.0},	{0.0}, {limitvote_Types} },
	{ string,	"New value",		{&menu_limitvote_value},		{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",				{NULL},							{0.0}, {0.0},	{0.0}, {NULL} },
	{ string,	"Reason for change:",{&menu_votereason},			{0.0}, {0.0},	{0.0}, {NULL} },	
	{ more,		"Vote!",			{NULL},							{0.0}, {0.0},	{0.0}, {(value_t *)limitvotemenu_Vote} },
};

//*****************************************************************************
//
menu_t limitvoteMenu = {
	"CHANGE LIMIT",
	0,
	countof(limitvotemenu_Items),
	0,
	limitvotemenu_Items,
	0,
	0,
};

//*****************************************************************************
//
void limitvotemenu_Show( void )
{
	M_SwitchMenu( &limitvoteMenu );
}

//====================================================================================
//
// Call Vote Menu
//
//====================================================================================

static menuitem_t CallVoteItems[] =
{
	{ more,		"Kick a player",			{NULL},					{0.0}, {0.0},	{0.0}, {(value_t *)kickplayermenu_Show} },
	{ more,		"Change the map",			{NULL},					{0.0}, {0.0},	{0.0}, {(value_t *)mapvotemenu_Show} },
	{ more,		"Change a limit",			{NULL},					{0.0}, {0.0},	{0.0}, {(value_t *)limitvotemenu_Show} },
};

menu_t CallVoteMenu = {
	"CALL VOTE",
	0,
	countof(CallVoteItems),
	0,
	CallVoteItems,
	0,
	0,
};

/*=======================================
 *
 * Multiplayer Menu
 *
 *=======================================*/

void SendNewColor (int red, int green, int blue);
void M_PlayerSetup (void);
void M_AccountSetup( void );
void M_SetupPlayerSetupMenu( void );
void M_StartBrowserMenu( void );
void M_Spectate( void );
void M_CallVote( void );
void M_ChangeTeam( void );
void M_Skirmish( void );

static menuitem_t MultiplayerItems[] =
{
	{ more,		"Offline skirmish",				{NULL},					{0.0}, {0.0},	{0.0}, {(value_t *)M_Skirmish} }, // [RC] Clarification that Skirmish is not hosting
	{ more,		"Browse servers",		{NULL},					{0.0}, {0.0},	{0.0}, {(value_t *)M_StartBrowserMenu} },
	{ more,		"Player setup",			{NULL},					{0.0}, {0.0},	{0.0}, {(value_t *)M_PlayerSetup} },
//	{ more,		"Account setup",		{NULL},					{0.0}, {0.0},	{0.0}, {(value_t *)M_AccountSetup} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ more,		"Spectate",				{NULL},					{0.0}, {0.0},	{0.0}, {(value_t *)M_Spectate} },
	{ more,		"Switch teams",				{NULL},					{0.0}, {0.0}, {0.0}, {(value_t *)M_ChangeTeam} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ more,		"Call a vote",			{NULL},					{0.0}, {0.0},	{0.0}, {(value_t *)M_CallVote} },	
	{ more,		"Ignore a player",			{NULL},					{0.0}, {0.0},	{0.0}, {(value_t *)ignoreplayermenu_Show} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ discrete, "Allow skins",			{&cl_skins},			{3.0}, {0.0},	{0.0}, {AllowSkinVals} },
	{ discrete, "Allow taunts",			{&cl_taunts},			{2.0}, {0.0},	{0.0}, {OnOff} },
//	{ discrete, "Allow medals",			{&cl_medals},			{2.0}, {0.0},	{0.0}, {OnOff} },
//	{ discrete, "Allow icons",			{&cl_icons},			{2.0}, {0.0},	{0.0}, {OnOff} },
	{ discrete,	"Identify players",		{&cl_identifytarget},	{2.0}, {0.0}, {0.0}, {OnOff} },
	{ discrete,	"Start as spectator",	{&cl_startasspectator},	{2.0}, {0.0},	{0.0}, {YesNo} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ string,	"Server password",		{&cl_password},				{0.0}, {0.0},	{0.0}, {NULL} },
	{ string,	"Join password",		{&cl_joinpassword},		{0.0}, {0.0},	{0.0}, {NULL} },
	{ discrete, "Connection type",		{&cl_connectiontype},	{2.0}, {0.0}, {0.0}, {ConnectionTypeVals} },
	{ discrete, "Reset frags at join",	{&cl_dontrestorefrags},	{2.0}, {0.0},	{0.0}, {YesNo} },
};

menu_t MultiplayerMenu = {
	"MULTIPLAYER",
	0,
	countof(MultiplayerItems),
	0,
	MultiplayerItems,
	0,
	0,
	0,
	M_SkulltagVersionDrawer,
	false,
};

void M_Spectate( void )
{
	M_ClearMenus( );
	if ( gamestate == GS_LEVEL )
		C_DoCommand( "spectate" );
	else
		M_StartMessage( "You must be in a game to spectate.\n\npress any key.", NULL, false );
}

//*****************************************************************************
//
void M_CallVote( void )
{
	// Don't allow a vote unless the player is a client.
	if ( NETWORK_GetState( ) != NETSTATE_CLIENT )
	{
		M_ClearMenus( );
		M_StartMessage( "You must be in a multiplayer game to vote.\n\npress any key.", NULL, false );

		return;
	}

	M_SwitchMenu( &CallVoteMenu );
}

//*****************************************************************************
//
void M_SkulltagVersionDrawer( void )
{
	ULONG	ulTextHeight;
	ULONG	ulCurYPos;
	char	szString[256];

	ulCurYPos = 182;
	ulTextHeight = ( gameinfo.gametype == GAME_Doom ? 8 : 9 );

	sprintf( szString, "%s v%s", GAMENAME, DOTVERSIONSTR_REV );
	screen->DrawText( SmallFont, CR_WHITE, 160 - ( SmallFont->StringWidth( szString ) / 2 ), ulCurYPos, szString, DTA_Clean, true, TAG_DONE );

	ulCurYPos += ulTextHeight;

	sprintf( szString, "\"Ask your doctor if it's right for you.\"" );
	screen->DrawText( SmallFont, CR_WHITE, 160 - ( SmallFont->StringWidth( szString ) / 2 ), ulCurYPos, szString, DTA_Clean, true, TAG_DONE );
}

/*=======================================
 *
 * Browser Menu
 *
 *=======================================*/

static	LONG	g_lSelectedServer = -1;
static	int		g_iSortedServers[MAX_BROWSER_SERVERS];

void M_RefreshServers( void );
void M_GetServerInfo( void );
void M_CheckConnectToServer( void );
void M_VerifyJoinWithoutWad( int iChar );
void M_ConnectToServer( void );
void M_BuildServerList( void );
bool M_ScrollServerList( bool bUp );
LONG M_CalcLastSortedIndex( void );
bool M_ShouldShowServer( LONG lServer );
void M_BrowserMenuDrawer( void );
void M_StartInternalBrowse( void );

static	void			browsermenu_SortServers( ULONG ulSortType );
static	int	STACK_ARGS	browsermenu_PingCompareFunc( const void *arg1, const void *arg2 );
static	int	STACK_ARGS	browsermenu_ServerNameCompareFunc( const void *arg1, const void *arg2 );
static	int	STACK_ARGS	browsermenu_MapNameCompareFunc( const void *arg1, const void *arg2 );
static	int	STACK_ARGS	browsermenu_PlayersCompareFunc( const void *arg1, const void *arg2 );

#define	NUM_SERVER_SLOTS	8
#define	SERVER_SLOT_START	8

CVAR( Int, menu_browser_servers, 0, CVAR_ARCHIVE )
CVAR( Int, menu_browser_gametype, 0, CVAR_ARCHIVE )
CVAR( Int, menu_browser_sortby, 0, CVAR_ARCHIVE );
CVAR( Bool, menu_browser_showempty, true, CVAR_ARCHIVE );
CVAR( Bool, menu_browser_showfull, true, CVAR_ARCHIVE )

// [RC] In Windows, a menu to launch IDEse or the internal browser is shown. There currently isn't an IDEse for Linux.
#ifdef WIN32

	void M_StartIdeSe( void )
	{
		I_RunProgram( "idese.exe" );
		exit( 0 );
	}

	static menuitem_t BrowserTypeItems[] = 
	{
		{ more,		"IDEse",		{NULL},					{0.0}, {0.0},	{0.0}, {(value_t *)M_StartIdeSe} },
		{ more,		"Internal browser",			{NULL},					{0.0}, {0.0},	{0.0}, {(value_t *)M_StartInternalBrowse} },
	};

	menu_t BrowserTypeMenu = {
		"SELECT BROWSER",
		0,
		countof(BrowserTypeItems),
		0,
		BrowserTypeItems,
		0,
		0,
		0,
		0,
		false,
		NULL,
	};	

#endif

static menuitem_t BrowserItems[] =
{
	{ discrete, "Servers",				{&menu_browser_servers},		{2.0}, {0.0},	{0.0}, {ServerTypeVals} },
	{ discrete, "Gametype",				{&menu_browser_gametype},		{17.0}, {0.0},	{0.0}, {ServerGameModeVals} },
	{ discrete, "Sort by",				{&menu_browser_sortby},			{4.0}, {0.0},	{0.0}, {ServerSortTypeVals} },
	{ discrete, "Show empty",			{&menu_browser_showempty},		{2.0}, {0.0},	{0.0}, {YesNo} },
	{ discrete,	"Show full",			{&menu_browser_showfull},		{2.0}, {0.0},	{0.0}, {YesNo} },
	{ redtext,	" ",					{NULL},							{0.0}, {0.0},	{0.0}, {NULL} },
	{ browserheader," ",				{NULL},							{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},							{0.0}, {0.0},	{0.0}, {NULL} },
	{ browserslot, NULL,				{NULL},							{0.0}, {0.0},	{0.0}, {NULL} },
	{ browserslot, NULL,				{NULL},							{0.0}, {0.0},	{0.0}, {NULL} },
	{ browserslot, NULL,				{NULL},							{0.0}, {0.0},	{0.0}, {NULL} },
	{ browserslot, NULL,				{NULL},							{0.0}, {0.0},	{0.0}, {NULL} },
	{ browserslot, NULL,				{NULL},							{0.0}, {0.0},	{0.0}, {NULL} },
	{ browserslot, NULL,				{NULL},							{0.0}, {0.0},	{0.0}, {NULL} },
	{ browserslot, NULL,				{NULL},							{0.0}, {0.0},	{0.0}, {NULL} },
	{ browserslot, NULL,				{NULL},							{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},							{0.0}, {0.0},	{0.0}, {NULL} },
	{ more,		"Refresh",				{NULL},							{0.0}, {0.0},	{0.0}, {(value_t *)M_RefreshServers} },
	{ more,		"Get server info",		{NULL},							{0.0}, {0.0},	{0.0}, {(value_t *)M_GetServerInfo} },
	{ more,		"Join game!",			{NULL},							{0.0}, {0.0},	{0.0}, {(value_t *)M_CheckConnectToServer} },
};

menu_t BrowserMenu = {
	"SERVER BROWSER",
	0,
	countof(BrowserItems),
	0,
	BrowserItems,
	0,
	0,
	0,
	M_BrowserMenuDrawer,
	false,
	NULL,
};

void M_StartBrowserMenu( void )
{
	// Switch the menu.
	#ifdef WIN32
		M_SwitchMenu( &BrowserTypeMenu );
	#else
		M_StartInternalBrowse( );
	#endif
}

void M_StartInternalBrowse( void )
{
	g_lSelectedServer = -1;

	// First, clear the existing server list.
	BROWSER_ClearServerList( );

	// Then, query the master server.
	BROWSER_QueryMasterServer( );

	// Build the server list.
//	M_BuildServerList( );

	// Switch the menu.
	M_SwitchMenu( &BrowserMenu );
}

//*****************************************************************************
//
void M_BrowserMenuDrawer( void )
{
	LONG	lNumServers;
	char	szString[256];

	lNumServers = M_CalcLastSortedIndex( );
	sprintf( szString, "Currently showing %d servers", static_cast<int> (lNumServers) );
	screen->DrawText( SmallFont, CR_WHITE, 160 - ( SmallFont->StringWidth( szString ) / 2 ), 190, szString, DTA_Clean, true, TAG_DONE );
}

//*****************************************************************************
//
void M_RefreshServers( void )
{
	// Don't do anything if we're still waiting for a response from the master server.
	if ( BROWSER_WaitingForMasterResponse( ))
		return;

	g_lSelectedServer = -1;

	// First, clear the existing server list.
	BROWSER_ClearServerList( );

	// Then, query the master server.
	BROWSER_QueryMasterServer( );
}

//*****************************************************************************
//
char	g_szVerifyJoinString[128];
void M_CheckConnectToServer( void )
{
	if ( g_lSelectedServer != -1 && BROWSER_IsActive( g_lSelectedServer ))
	{
		ULONG	ulIdx;
		bool	bNeedToLoadWads = false;
		FString	WadList;
		FString	MissingWadList;

		// If the server uses a PWAD, make sure we have it loaded. If not, ask the user
		// if he'd like to join the server anyway.
		for ( ulIdx = 0; ulIdx < static_cast<unsigned> (BROWSER_GetNumPWADs( g_lSelectedServer )); ulIdx++ )
		{
			WadList += BROWSER_GetPWADName( g_lSelectedServer, ulIdx );

			if ( ulIdx + 1 < static_cast<unsigned> (BROWSER_GetNumPWADs( g_lSelectedServer )))
				WadList += " ";

			if ( Wads.CheckIfWadLoaded( BROWSER_GetPWADName( g_lSelectedServer, ulIdx )) == -1 )
			{
				// A needed wad hasn't bee loaded! Signify that we'll need to load wads.
				bNeedToLoadWads = true;
				// [BB] Save the wads we are missing so that we can print a meaningful error message.
				MissingWadList += BROWSER_GetPWADName( g_lSelectedServer, ulIdx );
				MissingWadList += "\n";
			}
		}

		// Clear away the menus.
		M_ClearMenus( );

		// Check if we need to load up PWADs before we join the server.
		if ( bNeedToLoadWads )
		{
			I_Error ( "Can't connect to \"%s\". You don't have the following wads loaded:\n%s\nNote: It is recommended to use Doomseeker or IDE to join servers.", BROWSER_GetHostName ( g_lSelectedServer ), MissingWadList.GetChars() );

			gamestate = GS_FULLCONSOLE;
			if ( 0 )//D_LoadWads( szWadList ))
			{
				// Everything checked out! Join the server.
				M_ConnectToServer( );
			}
		}
		else
		{
			// Everything checked out! Join the server.
			M_ConnectToServer( );
		}
	}
}

//*****************************************************************************
//
void M_VerifyJoinWithoutWad( int iChar )
{
	if ( iChar != 'y' )
		return;

	// User wants to join the server.
	M_ConnectToServer( );
}

//*****************************************************************************
//
void M_ConnectToServer( void )
{
	if ( g_lSelectedServer != -1 && BROWSER_IsActive( g_lSelectedServer ))
	{
		char	szString[128];

		// Build our command string.
		sprintf( szString, "connect %s", NETWORK_AddressToString( BROWSER_GetAddress( g_lSelectedServer )));

		// Do the equavilent of typing "connect <IP>" in the console.
		AddCommandString( szString );

		// Clear out the menus.
		M_ClearMenus( );
	}
}

//*****************************************************************************
//
void M_BuildServerList( void )
{
	ULONG	ulIdx;

	// First, sort the menus.
	browsermenu_SortServers( menu_browser_sortby );

	for ( ulIdx = SERVER_SLOT_START; ulIdx < ( SERVER_SLOT_START + NUM_SERVER_SLOTS ); ulIdx++ )
		BrowserItems[ulIdx].f.lServer = g_iSortedServers[ulIdx - SERVER_SLOT_START];
}

//*****************************************************************************
//
bool M_ScrollServerList( bool bUp )
{
	ULONG	ulIdx;

	if ( bUp )
	{
		if ( BrowserItems[SERVER_SLOT_START].f.lServer != g_iSortedServers[0] )
		{
			for ( ulIdx = SERVER_SLOT_START; ulIdx < ( SERVER_SLOT_START + NUM_SERVER_SLOTS ); ulIdx++ )
			{
				ULONG	ulSortedIdx;

				// Go through the sorted server list and attempt to match it with the
				// server in this menu slot. If it matches, change the menu server slot
				// to the previous sorted server slot.
				for ( ulSortedIdx = 1; ulSortedIdx < MAX_BROWSER_SERVERS; ulSortedIdx++ )
				{
					if ( BrowserItems[ulIdx].f.lServer == g_iSortedServers[ulSortedIdx] )
					{
						BrowserItems[ulIdx].f.lServer = g_iSortedServers[ulSortedIdx - 1];
						break;
					}
				}
			}

			// Scrolling took place.
			return ( true );
		}
		// No scrolling took place.
		else
			return ( false );
	}
	else
	{
		// No servers in the list.
		if ( M_CalcLastSortedIndex( ) == 0 )
			return ( false );

		if ( BrowserItems[SERVER_SLOT_START + NUM_SERVER_SLOTS].f.lServer != g_iSortedServers[M_CalcLastSortedIndex( ) - 1] )
		{
			for ( ulIdx = SERVER_SLOT_START; ulIdx < ( SERVER_SLOT_START + NUM_SERVER_SLOTS ); ulIdx++ )
			{
				ULONG	ulSortedIdx;

				// Go through the sorted server list and attempt to match it with the
				// server in this menu slot. If it matches, change the menu server slot
				// to the next sorted server slot.
				for ( ulSortedIdx = 0; ulSortedIdx < MAX_BROWSER_SERVERS - 1; ulSortedIdx++ )
				{
					if ( BrowserItems[ulIdx].f.lServer == g_iSortedServers[ulSortedIdx] )
					{
						BrowserItems[ulIdx].f.lServer = g_iSortedServers[ulSortedIdx + 1];
						break;
					}
				}
			}

			// Scrolling took place.
			return ( true );
		}
		// No scrolling took place.
		else
			return ( false );
	}
}

//*****************************************************************************
//
LONG M_CalcLastSortedIndex( void )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAX_BROWSER_SERVERS; ulIdx++ )
	{
		if ( M_ShouldShowServer( g_iSortedServers[ulIdx] ) == false )
			return ( ulIdx );
	}

	return ( ulIdx );
}

//*****************************************************************************
//
bool M_ShouldShowServer( LONG lServer )
{
	// Don't show inactive servers.
	if ( BROWSER_IsActive( lServer ) == false )
		return ( false );
/*
	// Don't show servers that don't have the same IWAD we do.
	if ( stricmp( SERVER_MASTER_GetIWADName( ), BROWSER_GetIWADName( lServer )) != 0 )
		return ( false );
*/
	// Don't show Internet servers if we are only showing LAN servers.
	if ( menu_browser_servers == 1 )
	{
		if ( BROWSER_IsLAN( lServer ) == false )
			return ( false );
	}

	// Don't show LAN servers if we are only showing Internet servers.
	if ( menu_browser_servers == 0 )
	{
		if ( BROWSER_IsLAN( lServer ) == true )
			return ( false );
	}

	// Don't show empty servers.
	if ( menu_browser_showempty == false )
	{
		if ( BROWSER_GetNumPlayers( lServer ) == 0 )
			return ( false );
	}

	// Don't show full servers.
	if ( menu_browser_showfull == false )
	{
		if ( BROWSER_GetNumPlayers( lServer ) ==  BROWSER_GetMaxClients( lServer ))
			return ( false );
	}

	// Don't show servers that have the gameplay mode we want.
	if ( menu_browser_gametype != 0 )
	{
		if ( BROWSER_GetGameMode( lServer ) != ( menu_browser_gametype - 1 ))
			return ( false );
	}

	return ( true );
}

//*****************************************************************************
//
static void browsermenu_SortServers( ULONG ulSortType )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAX_BROWSER_SERVERS; ulIdx++ )
		g_iSortedServers[ulIdx] = ulIdx;

	switch ( ulSortType )
	{
	// Ping.
	case 0:

		qsort( g_iSortedServers, MAX_BROWSER_SERVERS, sizeof( int ), browsermenu_PingCompareFunc );
		break;
	// Server name.
	case 1:

		qsort( g_iSortedServers, MAX_BROWSER_SERVERS, sizeof( int ), browsermenu_ServerNameCompareFunc );
		break;
	// Map name.
	case 2:

		qsort( g_iSortedServers, MAX_BROWSER_SERVERS, sizeof( int ), browsermenu_MapNameCompareFunc );
		break;
	// Players.
	case 3:

		qsort( g_iSortedServers, MAX_BROWSER_SERVERS, sizeof( int ), browsermenu_PlayersCompareFunc );
		break;
	}
}

//*****************************************************************************
//
static int STACK_ARGS browsermenu_PingCompareFunc( const void *arg1, const void *arg2 )
{
	if (( M_ShouldShowServer( *(int *)arg1 ) == false ) && ( M_ShouldShowServer( *(int *)arg2 ) == false ))
		return ( 0 );

	if ( M_ShouldShowServer( *(int *)arg1 ) == false )
		return ( 1 );

	if ( M_ShouldShowServer( *(int *)arg2 ) == false )
		return ( -1 );

	return ( BROWSER_GetPing( *(int *)arg1 ) - BROWSER_GetPing( *(int *)arg2 ));
}

//*****************************************************************************
//
static int STACK_ARGS browsermenu_ServerNameCompareFunc( const void *arg1, const void *arg2 )
{
	if (( M_ShouldShowServer( *(int *)arg1 ) == false ) && ( M_ShouldShowServer( *(int *)arg2 ) == false ))
		return ( 0 );

	if ( M_ShouldShowServer( *(int *)arg1 ) == false )
		return ( 1 );

	if ( M_ShouldShowServer( *(int *)arg2 ) == false )
		return ( -1 );

	return ( stricmp( BROWSER_GetHostName( *(int *)arg1 ), BROWSER_GetHostName( *(int *)arg2 )));
}

//*****************************************************************************
//
static int STACK_ARGS browsermenu_MapNameCompareFunc( const void *arg1, const void *arg2 )
{
	if (( M_ShouldShowServer( *(int *)arg1 ) == false ) && ( M_ShouldShowServer( *(int *)arg2 ) == false ))
		return ( 0 );

	if ( M_ShouldShowServer( *(int *)arg1 ) == false )
		return ( 1 );

	if ( M_ShouldShowServer( *(int *)arg2 ) == false )
		return ( -1 );

	return ( stricmp( BROWSER_GetMapname( *(int *)arg1 ), BROWSER_GetMapname( *(int *)arg2 )));
}

//*****************************************************************************
//
static int STACK_ARGS browsermenu_PlayersCompareFunc( const void *arg1, const void *arg2 )
{
	if (( M_ShouldShowServer( *(int *)arg1 ) == false ) && ( M_ShouldShowServer( *(int *)arg2 ) == false ))
		return ( 0 );

	if ( M_ShouldShowServer( *(int *)arg1 ) == false )
		return ( 1 );

	if ( M_ShouldShowServer( *(int *)arg2 ) == false )
		return ( -1 );

	return ( BROWSER_GetNumPlayers( *(int *)arg2 ) - BROWSER_GetNumPlayers( *(int *)arg1 ));
}

/*=======================================
 *
 * Server Info Menu
 *
 *=======================================*/

void M_ReturnToBrowserMenu( void );
void M_DrawServerInfo( void );

static menuitem_t ServerInfoItems[] =
{
	{ redtext,	" ",					{NULL},							{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},							{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},							{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},							{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},							{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},							{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},							{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},							{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},							{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},							{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},							{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},							{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},							{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},							{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},							{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},							{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},							{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},							{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},							{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},							{0.0}, {0.0},	{0.0}, {NULL} },
	{ more,		"Return to browser",	{NULL},							{0.0}, {0.0},	{0.0}, {(value_t *)M_ReturnToBrowserMenu} },
};

menu_t ServerInfoMenu = {
	"SERVER INFORMATION",
	20,
	countof(ServerInfoItems),
	0,
	ServerInfoItems,
	0,
	0,
	0,
	M_DrawServerInfo,
	false,
	NULL,
	MNF_ALIGNLEFT,
};

//*****************************************************************************
//
void M_ReturnToBrowserMenu( void )
{
	CurrentMenu->lastOn = CurrentItem;
	M_PopMenuStack( );
}

//*****************************************************************************
//
void M_DrawServerInfo( void )
{
	ULONG	ulIdx;
	ULONG	ulCurYPos;
	ULONG	ulTextHeight;
	char	szString[256];

	if ( g_lSelectedServer == -1 )
		return;

	ulCurYPos = 32;
	ulTextHeight = ( gameinfo.gametype == GAME_Doom ? 8 : 9 );

	sprintf( szString, "Name: \\cc%s", BROWSER_GetHostName( g_lSelectedServer ));
	V_ColorizeString( szString );
	screen->DrawText( SmallFont, CR_UNTRANSLATED, 16, ulCurYPos, szString, DTA_Clean, true, TAG_DONE );

	ulCurYPos += ulTextHeight;

	sprintf( szString, "IP: \\cc%s", NETWORK_AddressToString( BROWSER_GetAddress( g_lSelectedServer )));
	V_ColorizeString( szString );
	screen->DrawText( SmallFont, CR_UNTRANSLATED, 16, ulCurYPos, szString, DTA_Clean, true, TAG_DONE );

	ulCurYPos += ulTextHeight;

	sprintf( szString, "Map: \\cc%s", BROWSER_GetMapname( g_lSelectedServer ));
	V_ColorizeString( szString );
	screen->DrawText( SmallFont, CR_UNTRANSLATED, 16, ulCurYPos, szString, DTA_Clean, true, TAG_DONE );

	ulCurYPos += ulTextHeight;

	sprintf( szString, "Gametype: \\cc%s", GameModeVals[BROWSER_GetGameMode( g_lSelectedServer )].name );
	V_ColorizeString( szString );
	screen->DrawText( SmallFont, CR_UNTRANSLATED, 16, ulCurYPos, szString, DTA_Clean, true, TAG_DONE );

	ulCurYPos += ulTextHeight;

	sprintf( szString, "IWAD: \\cc%s", BROWSER_GetIWADName( g_lSelectedServer ));
	V_ColorizeString( szString );
	screen->DrawText( SmallFont, CR_UNTRANSLATED, 16, ulCurYPos, szString, DTA_Clean, true, TAG_DONE );

	ulCurYPos += ulTextHeight;

	sprintf( szString, "PWADs: \\cc%d", static_cast<int> (BROWSER_GetNumPWADs( g_lSelectedServer )));
	V_ColorizeString( szString );
	screen->DrawText( SmallFont, CR_UNTRANSLATED, 16, ulCurYPos, szString, DTA_Clean, true, TAG_DONE );

	ulCurYPos += ulTextHeight;

	for ( ulIdx = 0; ulIdx < static_cast<unsigned> (MIN( (int)BROWSER_GetNumPWADs( g_lSelectedServer ), 4 )); ulIdx++ )
	{
		sprintf( szString, "\\cc%s", BROWSER_GetPWADName( g_lSelectedServer, ulIdx ));
		V_ColorizeString( szString );
		screen->DrawText( SmallFont, CR_UNTRANSLATED, 32, ulCurYPos, szString, DTA_Clean, true, TAG_DONE );

		ulCurYPos += ulTextHeight;
	}

	sprintf( szString, "WAD URL: \\cc%s", BROWSER_GetWadURL( g_lSelectedServer ));
	V_ColorizeString( szString );
	screen->DrawText( SmallFont, CR_UNTRANSLATED, 16, ulCurYPos, szString, DTA_Clean, true, TAG_DONE );

	ulCurYPos += ulTextHeight;

	sprintf( szString, "Host e-mail: \\cc%s", BROWSER_GetEmailAddress( g_lSelectedServer ));
	V_ColorizeString( szString );
	screen->DrawText( SmallFont, CR_UNTRANSLATED, 16, ulCurYPos, szString, DTA_Clean, true, TAG_DONE );

	ulCurYPos += ulTextHeight;

	sprintf( szString, "Players: \\cc%d/%d", static_cast<int> (BROWSER_GetNumPlayers( g_lSelectedServer )), static_cast<int> (BROWSER_GetMaxClients( g_lSelectedServer )));
	V_ColorizeString( szString );
	screen->DrawText( SmallFont, CR_UNTRANSLATED, 16, ulCurYPos, szString, DTA_Clean, true, TAG_DONE );

	ulCurYPos += ulTextHeight;

	if ( BROWSER_GetNumPlayers( g_lSelectedServer ))
	{
		ulCurYPos += ulTextHeight;

		screen->DrawText( SmallFont, CR_UNTRANSLATED, 32, ulCurYPos, "NAME", DTA_Clean, true, TAG_DONE );
		screen->DrawText( SmallFont, CR_UNTRANSLATED, 192, ulCurYPos, "FRAGS", DTA_Clean, true, TAG_DONE );
		screen->DrawText( SmallFont, CR_UNTRANSLATED, 256, ulCurYPos, "PING", DTA_Clean, true, TAG_DONE );

		ulCurYPos += ( ulTextHeight * 2 );

		for ( ulIdx = 0; static_cast<signed> (ulIdx) < MIN( (int)BROWSER_GetNumPlayers( g_lSelectedServer ), 4 ); ulIdx++ )
		{
			sprintf( szString, "%s", BROWSER_GetPlayerName( g_lSelectedServer, ulIdx ));
			V_ColorizeString( szString );
			screen->DrawText( SmallFont, CR_GRAY, 32, ulCurYPos, szString, DTA_Clean, true, TAG_DONE );

			sprintf( szString, "%d", static_cast<int> (BROWSER_GetPlayerFragcount( g_lSelectedServer, ulIdx )));
			V_ColorizeString( szString );
			screen->DrawText( SmallFont, CR_GRAY, 192, ulCurYPos, szString, DTA_Clean, true, TAG_DONE );

			sprintf( szString, "%d", static_cast<int> (BROWSER_GetPlayerPing( g_lSelectedServer, ulIdx )));
			V_ColorizeString( szString );
			screen->DrawText( SmallFont, CR_GRAY, 256, ulCurYPos, szString, DTA_Clean, true, TAG_DONE );

			ulCurYPos += ulTextHeight;
		}
	}
}

//*****************************************************************************
//
void M_GetServerInfo( void )
{
	if ( g_lSelectedServer != -1 )
	{
		// Switch the menu.
		M_SwitchMenu( &ServerInfoMenu );
	}
}

/*=======================================
 *
 * Player setup Menu
 *
 *=======================================*/

void M_WeaponSetup( void );
void M_PlayerSetupDrawer( void );

void M_AcceptPlayerSetupChanges( void );
void M_UndoPlayerSetupChanges( void );
bool M_PlayerSetupItemsChanged( void );
void M_AcceptPlayerSetupChangesFromPrompt( int iChar );

extern menu_t PlayerSetupMenu;

extern FPlayerClass		*PlayerClass;
extern int				PlayerRotation;

// [RC] Moved switch team to the Multiplayer menu
void M_ChangeTeam( void )
{
	// Clear the menus, and send the changeteam command.
	M_ClearMenus( );
	static char changeteam[] = "changeteam";
	AddCommandString( changeteam );
}


void M_SetupPlayerSetupMenu( void )
{
	UCVarValue	Val;

	// Initialize all the menu variables.
	Val = name.GetGenericRep( CVAR_String );
	menu_name.SetGenericRep( Val, CVAR_String );

	Val = skin.GetGenericRep( CVAR_String );
	menu_skin.SetGenericRep( Val, CVAR_String );

	Val = playerclass.GetGenericRep( CVAR_String );
	menu_playerclass.SetGenericRep( Val, CVAR_String );

	menu_gender = D_GenderToInt( gender );

	menu_color = color;
	menu_handicap = handicap;
	menu_railcolor = railcolor;
	menu_autoaim = autoaim;

	// Initialize the skin, color, and class placeholder variables.
	g_ulPlayerSetupSkin = R_FindSkin( skin, players[consoleplayer].CurrentPlayerClass );
	g_ulPlayerSetupColor = players[consoleplayer].userinfo.color;
	g_lPlayerSetupClass = players[consoleplayer].userinfo.PlayerClass;
}

void M_AcceptPlayerSetupChanges( void )
{
	UCVarValue	Val;
	ULONG		ulUpdateFlags;
	
	// No need to do this if nothing's changed!
	if ( M_PlayerSetupItemsChanged( ) == false )
		return;

	ulUpdateFlags = 0;
	CLIENT_SetAllowSendingOfUserInfo( false );

	if ( stricmp( menu_name, name ) != 0 )
		ulUpdateFlags |= USERINFO_NAME;

	// [RC] Clean the name
	char	szPlayerName[64];
	sprintf( szPlayerName, "%s", menu_name.GetGenericRep( CVAR_String ).String );
	V_CleanPlayerName(szPlayerName);
	menu_name = szPlayerName;

	Val = menu_name.GetGenericRep( CVAR_String );
	name.SetGenericRep( Val, CVAR_String );

	if ( menu_color.GetGenericRep( CVAR_Int ).Int != color.GetGenericRep( CVAR_Int ).Int )
		ulUpdateFlags |= USERINFO_COLOR;
	color = menu_color;

	if ( stricmp( menu_skin, skin ) != 0 )
		ulUpdateFlags |= USERINFO_SKIN;
	Val = menu_skin.GetGenericRep( CVAR_String );
	skin.SetGenericRep( Val, CVAR_String );

	if ( stricmp( menu_playerclass, playerclass ) != 0 )
		ulUpdateFlags |= USERINFO_PLAYERCLASS;
	Val = menu_playerclass.GetGenericRep( CVAR_String );
	playerclass.SetGenericRep( Val, CVAR_String );

	if ( stricmp( GenderVals[menu_gender].name, gender ) != 0 )
		ulUpdateFlags |= USERINFO_GENDER;
	gender = GenderVals[menu_gender].name;

	if ( menu_handicap != handicap )
		ulUpdateFlags |= USERINFO_HANDICAP;
	handicap = menu_handicap;

	if ( menu_autoaim != autoaim )
		ulUpdateFlags |= USERINFO_AIMDISTANCE;
	autoaim = menu_autoaim;

	if ( menu_railcolor != railcolor )
		ulUpdateFlags |= USERINFO_RAILCOLOR;
	railcolor = menu_railcolor;

	CLIENT_SetAllowSendingOfUserInfo( true );

	// Send updated userinfo to the server.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) &&
		( CLIENT_GetConnectionState( ) >= CTS_REQUESTINGSNAPSHOT ) &&
		( ulUpdateFlags > 0 ))
	{
		CLIENTCOMMANDS_UserInfo( ulUpdateFlags );
	}
}

void M_UndoPlayerSetupChanges( void )
{
	M_SetupPlayerSetupMenu( );
}

bool M_PlayerSetupItemsChanged( void )
{
	if ( stricmp( menu_name, name ) != 0 )
		return ( true );

	if ( menu_color.GetGenericRep( CVAR_Int ).Int != color.GetGenericRep( CVAR_Int ).Int )
		return ( true );

	if ( stricmp( menu_skin, skin ) != 0 )
		return ( true );

	if ( stricmp( menu_playerclass, playerclass ) != 0 )
		return ( true );

	if ( stricmp( GenderVals[menu_gender].name, gender ) != 0 )
		return ( true );

	if ( menu_handicap != handicap )
		return ( true );

	if ( menu_autoaim != autoaim )
		return ( true );

	if ( menu_railcolor != railcolor )
		return ( true );

	return ( false );
}

void M_AcceptPlayerSetupChangesFromPrompt( int iChar )
{
	if ( iChar != 'y' )
		return;

	M_AcceptPlayerSetupChanges( );
}

/*=======================================
 *
 * Account setup Menu
 *
 *=======================================*/
/*
void M_NewAccount( void );

menuitem_t AccountSetupItems[] = {
//	{ redtext,	" ",						NULL,					0.0, 0.0, 0.0, NULL  },
//	{ redtext,	"Please enter your account information below",						NULL,					0.0, 0.0, 0.0, NULL  },
//	{ redtext,	" ",						NULL,					0.0, 0.0, 0.0, NULL  },
	{ string,	"Username",					&cl_accountname,		0.0, 0.0, 0.0, NULL  },
	{ pwstring,	"Password",					&cl_accountpassword,	0.0, 0.0, 0.0, NULL  },
	{ redtext,	" ",						NULL,					0.0, 0.0, 0.0, NULL  },
	{ discrete, "Auto-login",				&cl_autologin,			2.0, 0.0, 0.0, YesNo },
	{ discrete, "Auto-ghost",				&cl_autoghost,			2.0, 0.0, 0.0, YesNo },
	{ redtext,	" ",						NULL,					0.0, 0.0, 0.0, NULL  },
	{ more,		"Log in",					NULL,					0.0, 0.0, 0.0, {(value_t *)M_NewAccount} },
	{ more,		"New account",				NULL,					0.0, 0.0, 0.0, {(value_t *)M_NewAccount} },
};

menu_t AccountSetupMenu = {
	"ACCOUNT SETUP",
	0,
	countof(AccountSetupItems),
	0,
	AccountSetupItems,
	0,
	0,
	NULL,
	MNF_ALIGNLEFT,
};

void M_AccountSetup( void )
{
	M_SwitchMenu( &AccountSetupMenu );
}
*/
/*=======================================
 *
 * New account Menu
 *
 *=======================================*/
/*
menuitem_t NewAccountItems[] = {
	{ redtext,	"Please choose a username and password.",						NULL,					0.0, 0.0, 0.0, NULL  },
	{ redtext,	" ",						NULL,					0.0, 0.0, 0.0, NULL  },
	{ string,	"Username",					&cl_accountname,		0.0, 0.0, 0.0, NULL  },
	{ pwstring,	"Password",					&cl_accountpassword,	0.0, 0.0, 0.0, NULL  },
	{ redtext,	" ",						NULL,					0.0, 0.0, 0.0, NULL  },
	{ more,		"Create account!",			NULL,					0.0, 0.0, 0.0, {(value_t *)M_NewAccount} },
};

menu_t NewAccountMenu = {
	"NEW ACCOUNT",
	0,
	countof(NewAccountItems),
	0,
	NewAccountItems,
	0,
	0,
	NULL,
	MNF_ALIGNLEFT,
};

void M_NewAccount( void )
{
	if ( NewAccountMenu.lastOn == 0 )
		NewAccountMenu.lastOn = 2;

	M_SwitchMenu( &NewAccountMenu );
}
*/
/*=======================================
 *
 * Weapon setup Menu
 *
 *=======================================*/

EXTERN_CVAR (Int, switchonpickup)

//*****************************************************************************
//
void M_WeaponSetupMenuDrawer( void )
{
	/* [RC] Remove the outmoded text about pressing + and - to change the user's personal weapon order
	ULONG	ulTextHeight;
	ULONG	ulCurYPos;
	char	szString[256];

	ulCurYPos = 182;
	ulTextHeight = ( gameinfo.gametype == GAME_Doom ? 8 : 9 );

	sprintf( szString, "Use the + and - keys to" );
	screen->DrawText( CR_WHITE, 160 - ( SmallFont->StringWidth( szString ) / 2 ), ulCurYPos, szString, DTA_Clean, true, TAG_DONE );

	ulCurYPos += ulTextHeight;

	sprintf( szString, "change your personal weapon ranking" );
	screen->DrawText( CR_WHITE, 160 - ( SmallFont->StringWidth( szString ) / 2 ), ulCurYPos, szString, DTA_Clean, true, TAG_DONE );
	*/
}

static menuitem_t WeaponSetupItems[] = {
	{ discrete,	"Switch on pickup",			{&switchonpickup},		{4.0}, {0.0}, {0.0}, {SwitchOnPickupVals}  },
	{ discrete,	"Allow switch with no ammo",{&cl_noammoswitch},		{2.0}, {0.0}, {0.0}, {YesNo}  },
	{ discrete,	"Cycle with original order",{&cl_useoriginalweaponorder},{2.0}, {0.0}, {0.0}, {YesNo}  },
	{ redtext,	" ",						{NULL},					{0.0}, {0.0}, {0.0}, {NULL}  },
//	{ redtext,	"WEAPON PREFERENCES",		NULL,					0.0, 0.0, 0.0, NULL  },
	{ redtext,	" ",						{NULL},					{0.0}, {0.0}, {0.0}, {NULL}  },
	{ redtext,	" ",						{NULL},					{0.0}, {0.0}, {0.0}, {NULL}  },
	{ weaponslot,	" ",						{NULL},					{0.0}, {0.0}, {0.0}, {NULL}  },
	{ weaponslot,	" ",						{NULL},					{0.0}, {0.0}, {0.0}, {NULL}  },
	{ weaponslot,	" ",						{NULL},					{0.0}, {0.0}, {0.0}, {NULL}  },
	{ weaponslot,	" ",						{NULL},					{0.0}, {0.0}, {0.0}, {NULL}  },
	{ weaponslot,	" ",						{NULL},					{0.0}, {0.0}, {0.0}, {NULL}  },
	{ weaponslot,	" ",						{NULL},					{0.0}, {0.0}, {0.0}, {NULL}  },
	{ weaponslot,	" ",						{NULL},					{0.0}, {0.0}, {0.0}, {NULL}  },
	{ weaponslot,	" ",						{NULL},					{0.0}, {0.0}, {0.0}, {NULL}  },
	{ weaponslot,	" ",						{NULL},					{0.0}, {0.0}, {0.0}, {NULL}  },
	{ weaponslot,	" ",						{NULL},					{0.0}, {0.0}, {0.0}, {NULL}  },
	{ weaponslot,	" ",						{NULL},					{0.0}, {0.0}, {0.0}, {NULL}  },
	{ weaponslot,	" ",						{NULL},					{0.0}, {0.0}, {0.0}, {NULL}  },
	{ weaponslot,	" ",						{NULL},					{0.0}, {0.0}, {0.0}, {NULL}  },
};

menu_t WeaponSetupMenu = {
	"WEAPON SETUP",
	0,
	countof(WeaponSetupItems),
	0,
	WeaponSetupItems,
	NULL,
	0,
	0,
	M_WeaponSetupMenuDrawer,
	false,
	NULL,
	MNF_ALIGNLEFT,
};

//const char *GetWeaponPrefNameByRank( ULONG ulRank );
void M_RefreshWeaponSetupItems( void )
{
	ULONG		ulIdx;
	ULONG		ulSlotStart;
	menuitem_t	*pItem;

	// Populate the weapon setup menu with the current weapon preferences.
	ulSlotStart = 1;
	for ( ulIdx = 0; ulIdx < static_cast<unsigned> (CurrentMenu->numitems); ulIdx++ )
	{
		pItem = CurrentMenu->items + ulIdx;
		if ( pItem->type == weaponslot )
		{
			const char	*pszString;

			pszString = NULL;//GetWeaponPrefNameByRank( ulSlotStart );
			if ( pszString )
			{
				sprintf( g_szWeaponPrefStringBuffer[ulIdx], "Slot %d: \\cc%s", static_cast<unsigned int> (ulSlotStart), pszString );
				V_ColorizeString( g_szWeaponPrefStringBuffer[ulIdx] );
				pItem->label = g_szWeaponPrefStringBuffer[ulIdx];
			}
			else
			{
				g_szWeaponPrefStringBuffer[ulIdx][0] = 0;
				V_ColorizeString( g_szWeaponPrefStringBuffer[ulIdx] );
				pItem->label = g_szWeaponPrefStringBuffer[ulIdx];
			}

			ulSlotStart++;
		}
	}
}

void M_WeaponSetup (void)
{
	// [ZZ] Left here for some imaginary compatibility with existing code
	WeaponOptions();
	return;

	M_SwitchMenu( &WeaponSetupMenu );

	CurrentItem = 0;
	M_RefreshWeaponSetupItems( );
}

/*=======================================
 *
 * Skirmish Menu
 *
 *=======================================*/

static void SkirmishGameplayOptions( void );
void M_BotSetup( void );
void M_ClearBotSlotList( void );
void M_StartSkirmishGame( void );

CVAR( Int, menu_level, 0, CVAR_ARCHIVE )
CVAR( Int, menu_gamemode, 0, CVAR_ARCHIVE )
CVAR( Int, menu_timelimit, 0, CVAR_ARCHIVE );
CVAR( Int, menu_fraglimit, 0, CVAR_ARCHIVE );
CVAR( Int, menu_pointlimit, 0, CVAR_ARCHIVE );
CVAR( Int, menu_duellimit, 0, CVAR_ARCHIVE );
CVAR( Int, menu_winlimit, 0, CVAR_ARCHIVE );
CVAR( Int, menu_wavelimit, 0, CVAR_ARCHIVE );
CVAR( Int, menu_skill, 0, CVAR_ARCHIVE );
CVAR( Int, menu_botskill, 0, CVAR_ARCHIVE );
CVAR( Int, menu_botspawn0, -1, CVAR_ARCHIVE );
CVAR( Int, menu_botspawn1, -1, CVAR_ARCHIVE );
CVAR( Int, menu_botspawn2, -1, CVAR_ARCHIVE );
CVAR( Int, menu_botspawn3, -1, CVAR_ARCHIVE );
CVAR( Int, menu_botspawn4, -1, CVAR_ARCHIVE );
CVAR( Int, menu_botspawn5, -1, CVAR_ARCHIVE );
CVAR( Int, menu_botspawn6, -1, CVAR_ARCHIVE );
CVAR( Int, menu_botspawn7, -1, CVAR_ARCHIVE );
CVAR( Int, menu_botspawn8, -1, CVAR_ARCHIVE );
CVAR( Int, menu_botspawn9, -1, CVAR_ARCHIVE );
CVAR( Int, menu_botspawn10, -1, CVAR_ARCHIVE );
CVAR( Int, menu_botspawn11, -1, CVAR_ARCHIVE );
CVAR( Int, menu_botspawn12, -1, CVAR_ARCHIVE );
CVAR( Int, menu_botspawn13, -1, CVAR_ARCHIVE );
CVAR( Int, menu_botspawn14, -1, CVAR_ARCHIVE );
CVAR( Int, menu_botspawn15, -1, CVAR_ARCHIVE );
CVAR( Int, menu_teambotspawn0, -1, CVAR_ARCHIVE );
CVAR( Int, menu_teambotspawn1, -1, CVAR_ARCHIVE );
CVAR( Int, menu_teambotspawn2, -1, CVAR_ARCHIVE );
CVAR( Int, menu_teambotspawn3, -1, CVAR_ARCHIVE );
CVAR( Int, menu_teambotspawn4, -1, CVAR_ARCHIVE );
CVAR( Int, menu_teambotspawn5, -1, CVAR_ARCHIVE );
CVAR( Int, menu_teambotspawn6, -1, CVAR_ARCHIVE );
CVAR( Int, menu_teambotspawn7, -1, CVAR_ARCHIVE );
CVAR( Int, menu_teambotspawn8, -1, CVAR_ARCHIVE );
CVAR( Int, menu_teambotspawn9, -1, CVAR_ARCHIVE );
CVAR( Int, menu_teambotspawn10, -1, CVAR_ARCHIVE );
CVAR( Int, menu_teambotspawn11, -1, CVAR_ARCHIVE );
CVAR( Int, menu_teambotspawn12, -1, CVAR_ARCHIVE );
CVAR( Int, menu_teambotspawn13, -1, CVAR_ARCHIVE );
CVAR( Int, menu_teambotspawn14, -1, CVAR_ARCHIVE );
CVAR( Int, menu_teambotspawn15, -1, CVAR_ARCHIVE );
CVAR( Int, menu_teambotspawn16, -1, CVAR_ARCHIVE );
CVAR( Int, menu_teambotspawn17, -1, CVAR_ARCHIVE );
CVAR( Int, menu_teambotspawn18, -1, CVAR_ARCHIVE );
CVAR( Int, menu_teambotspawn19, -1, CVAR_ARCHIVE );
CVAR( Int, menu_dmflags, 20612, CVAR_ARCHIVE );
CVAR( Int, menu_dmflags2, 512, CVAR_ARCHIVE );
CVAR( Int, menu_modifier, 0, CVAR_ARCHIVE );

static menuitem_t SkirmishItems[] = {
	{ levelslot,"Level",					{&menu_level},			{0.0}, {0.0}, {0.0}, {NULL}  },
	{ discrete,	"Game mode",				{&menu_gamemode},		{16.0}, {0.0}, {0.0}, {GameModeVals} },
	{ discrete, "Modifier",					{&menu_modifier},		{3.0}, {0.0}, {0.0}, {ModifierVals} },
	{ redtext,	" ",						{NULL},					{0.0}, {0.0}, {0.0}, {NULL}  },
	{ number,	"Timelimit",				{&menu_timelimit},		{0.0}, {100.0}, {1.0}, {NULL}  },
	{ number,	"Fraglimit",				{&menu_fraglimit},		{0.0}, {100.0}, {1.0}, {NULL}  },
	{ number,	"Pointlimit",				{&menu_pointlimit},		{0.0}, {100.0}, {1.0}, {NULL}  },
	{ number,	"Duellimit",				{&menu_duellimit},		{0.0}, {100.0}, {1.0}, {NULL}  },
	{ number,	"Winlimit",					{&menu_winlimit},		{0.0}, {100.0}, {1.0}, {NULL}  },
	{ number,	"Wavelimit",				{&menu_wavelimit},		{0.0}, {10.0}, {1.0}, {NULL}  },
	{ redtext,	" ",						{NULL},					{0.0}, {0.0}, {0.0}, {NULL}  },
	{ discrete,	"Skill",					{&menu_skill},			{5.0}, {0.0}, {0.0}, {GameskillVals} },
	{ discrete,	"Botskill",					{&menu_botskill},		{5.0}, {0.0}, {0.0}, {BotskillVals} },
	{ redtext,	" ",						{NULL},					{0.0}, {0.0}, {0.0}, {NULL}  },
	{ more,		"Bot setup",				{NULL},					{0.0}, {0.0}, {0.0}, {(value_t *)M_BotSetup} },
	{ more,		"Gameplay options",			{NULL},					{0.0}, {0.0}, {0.0}, {(value_t *)SkirmishGameplayOptions} },
	{ redtext,	" ",						{NULL},					{0.0}, {0.0}, {0.0}, {NULL}  },
	{ more,		"Start game!",				{NULL},					{0.0}, {0.0}, {0.0}, {(value_t *)M_StartSkirmishGame} },
};

menu_t SkirmishMenu = {
	"SKIRMISH",
	0,
	countof(SkirmishItems),
	0,
	SkirmishItems,
	0,
	0,
	0,
	NULL,
	false,
	NULL,
	MNF_ALIGNLEFT,
};

void M_Skirmish( void )
{
	M_SwitchMenu( &SkirmishMenu );
}

void M_ClearBotSlotList( void )
{
	ULONG		ulIdx;
	FBaseCVar	*pVar;
	UCVarValue	Val;
	char		szCVarName[32];

	// Initialize bot spawn times.
	if ( GAMEMODE_GetFlags( static_cast<GAMEMODE_e>(menu_gamemode.GetGenericRep( CVAR_Int ).Int) ) & GMF_PLAYERSONTEAMS )
	{
		for ( ulIdx = 0; ulIdx < MAX_BOTTEAMSLOTS; ulIdx++ )
		{
			sprintf( szCVarName, "menu_teambotspawn%d", static_cast<unsigned int> (ulIdx) );
			pVar = FindCVar( szCVarName, NULL );

			Val = pVar->GetGenericRep( CVAR_Int );
			Val.Int = -1;
			pVar->SetGenericRep( Val, CVAR_Int );
		}
	}
	else
	{
		for ( ulIdx = 0; ulIdx < 16; ulIdx++ )
		{
			sprintf( szCVarName, "menu_botspawn%d", static_cast<unsigned int> (ulIdx) );
			pVar = FindCVar( szCVarName, NULL );

			Val = pVar->GetGenericRep( CVAR_Int );
			Val.Int = -1;
			pVar->SetGenericRep( Val, CVAR_Int );
		}
	}
}

void G_NewInit( void );
void M_StartSkirmishGame( void )
{
	char		szLevelName[8];
	ULONG		ulIdx;
	UCVarValue	Val;
	FBaseCVar	*pVar;
	char		szCVarName[32];
	char		szBuffer[256];

	// Setup the level name.
	sprintf( szLevelName, "%s", wadlevelinfos[menu_level].mapname );

	// Invalid level selected.
	if ( stricmp( szLevelName, "(null)" ) == 0 )
		return;

	// Go into single player mode.
	NETWORK_SetState( NETSTATE_SINGLE );

	// Disable campaign mode.
	CAMPAIGN_DisableCampaign( );

	Val = menu_skill.GetGenericRep( CVAR_Int );
	gameskill.ForceSet( Val, CVAR_Int );

	Val = menu_botskill.GetGenericRep( CVAR_Int );
	botskill.ForceSet( Val, CVAR_Int );

	Val = menu_timelimit.GetGenericRep( CVAR_Float );
	timelimit.ForceSet( Val, CVAR_Float );

	Val = menu_fraglimit.GetGenericRep( CVAR_Int );
	fraglimit.ForceSet( Val, CVAR_Int );

	Val = menu_pointlimit.GetGenericRep( CVAR_Int );
	pointlimit.ForceSet( Val, CVAR_Int );

	Val = menu_duellimit.GetGenericRep( CVAR_Int );
	duellimit.ForceSet( Val, CVAR_Int );

	Val = menu_winlimit.GetGenericRep( CVAR_Int );
	winlimit.ForceSet( Val, CVAR_Int );

	Val = menu_wavelimit.GetGenericRep( CVAR_Int );
	wavelimit.ForceSet( Val, CVAR_Int );

	Val = menu_dmflags.GetGenericRep( CVAR_Int );
	dmflags.ForceSet( Val, CVAR_Int );

	Val = menu_dmflags2.GetGenericRep( CVAR_Int );
	dmflags2.ForceSet( Val, CVAR_Int );

	GAMEMODE_SetCurrentMode( (GAMEMODE_e) menu_gamemode.GetGenericRep( CVAR_Int ).Int );
	GAMEMODE_SetModifier( (MODIFIER_e) menu_modifier.GetGenericRep( CVAR_Int ).Int );

	// [BB] In non-cooperative game modes we need to enable multiplayer emulation,
	// otherwise respawning and player class selection won't work properly.
	if ( !(GAMEMODE_GetFlags( GAMEMODE_GetCurrentMode( )) & GMF_COOPERATIVE) )
		NETWORK_SetState( NETSTATE_SINGLE_MULTIPLAYER );

	// Remove all the existing bots.
	BOTS_RemoveAllBots( false );

	// Potentially end playing demos.
	if ( demoplayback )
	{
		C_RestoreCVars( );
		demoplayback = false;
		D_SetupUserInfo( );
	}
	if ( CLIENTDEMO_IsPlaying( ))
	{
		CLIENTDEMO_SetPlaying( false );
		D_SetupUserInfo( );
	}

	// [BB] We may not call G_InitNew here, this causes crashes in the software renderer,
	// for example when going from D2IG03 to D2IG04 (both started from the skirmish menu).
	// Since G_InitNew calls BOTSPAWN_ClearTable() we have to call BOTSPAWN_BlockClearTable()
	// to protect the table. Not very elegant, but seems to work.
	//G_InitNew( szLevelName, false );
	G_DeferedInitNew( szLevelName );
	BOTSPAWN_ClearTable();
	BOTSPAWN_BlockClearTable();
	gamestate = gamestate == GS_FULLCONSOLE ? GS_HIDECONSOLE : gamestate;

	// Clear out the menus and load the level.
	M_ClearMenus( );

	// Initialize bot spawn times.
	if ( GAMEMODE_GetFlags( static_cast<GAMEMODE_e>(menu_gamemode.GetGenericRep( CVAR_Int ).Int) ) & GMF_PLAYERSONTEAMS )
	{
		for ( ulIdx = 0; ulIdx < MAX_BOTTEAMSLOTS; ulIdx++ )
		{
			sprintf( szCVarName, "menu_teambotspawn%d", static_cast<unsigned int> (ulIdx) );
			pVar = FindCVar( szCVarName, NULL );

			Val = pVar->GetGenericRep( CVAR_Int );
			if (( BOTINFO_GetRevealed( Val.Int )) && (  BOTINFO_GetName( Val.Int ) != NULL ))
			{
				sprintf( szBuffer, "%s", BOTINFO_GetName( Val.Int ));
				V_ColorizeString( szBuffer );
				V_RemoveColorCodes( szBuffer );

				// [CW] Add to this when bumping 'MAX_TEAMS'.
				if ( ulIdx >= 15 )
				{
					if ( teams.Size( ) >= 4 )
						BOTSPAWN_AddToTable( szBuffer, (char *)TEAM_GetName( 3 ));
				}
				else if ( ulIdx >= 10 )
				{
					if ( teams.Size( ) >= 3 )
						BOTSPAWN_AddToTable( szBuffer, (char *)TEAM_GetName( 2 ) );
				}
				else if ( ulIdx >= 5 )
				{
					if ( teams.Size( ) >= 2 )
						BOTSPAWN_AddToTable( szBuffer, (char *)TEAM_GetName( 1 ) );
				}
				else
				{
					BOTSPAWN_AddToTable( szBuffer, (char *)TEAM_GetName( 0 ) );
				}
			}
		}
	}
	else
	{
		for ( ulIdx = 0; ulIdx < 16; ulIdx++ )
		{
			sprintf( szCVarName, "menu_botspawn%d", static_cast<unsigned int> (ulIdx) );
			pVar = FindCVar( szCVarName, NULL );

			Val = pVar->GetGenericRep( CVAR_Int );
			if (( BOTINFO_GetRevealed( Val.Int )) && (  BOTINFO_GetName( Val.Int ) != NULL ))
			{
				sprintf( szBuffer, "%s", BOTINFO_GetName( Val.Int ));
				V_ColorizeString( szBuffer );
				V_RemoveColorCodes( szBuffer );
				BOTSPAWN_AddToTable( szBuffer, NULL );
			}
		}
	}
}

/*=======================================
 *
 * Skirmish Gameplay Options (dmflags) Menu
 *
 *=======================================*/

static menuitem_t SkirmishDMFlagsItems[] = {
	{ slider,	"Team damage scalar",	{&teamdamage},	{0.0}, {1.0}, {0.05},{NULL} },
	{ redtext,	" ",					{NULL},			{0.0}, {0.0}, {0.0}, {NULL} },
	{ bitflag,	"Falling damage (old)",	{&menu_dmflags},		{0}, {0}, {0}, {(value_t *)DF_FORCE_FALLINGZD} },
	{ bitflag,	"Falling damage (Hexen)",{&menu_dmflags},	{0}, {0}, {0}, {(value_t *)DF_FORCE_FALLINGHX} },
	{ bitflag,	"Weapons stay (DM)",	{&menu_dmflags},		{0}, {0}, {0}, {(value_t *)DF_WEAPONS_STAY} },
	{ bitflag,	"Allow powerups (DM)",	{&menu_dmflags},		{1}, {0}, {0}, {(value_t *)DF_NO_ITEMS} },
	{ bitflag,	"Allow health (DM)",	{&menu_dmflags},		{1}, {0}, {0}, {(value_t *)DF_NO_HEALTH} },
	{ bitflag,	"Allow armor (DM)",		{&menu_dmflags},		{1}, {0}, {0}, {(value_t *)DF_NO_ARMOR} },
	{ bitflag,	"Spawn farthest (DM)",	{&menu_dmflags},		{0}, {0}, {0}, {(value_t *)DF_SPAWN_FARTHEST} },
	{ bitflag,	"Same map (DM)",		{&menu_dmflags},		{0}, {0}, {0}, {(value_t *)DF_SAME_LEVEL} },
	{ bitflag,	"Force respawn (DM)",	{&menu_dmflags},		{0}, {0}, {0}, {(value_t *)DF_FORCE_RESPAWN} },
	{ bitflag,	"Allow exit (DM)",		{&menu_dmflags},		{1}, {0}, {0}, {(value_t *)DF_NO_EXIT} },
	{ bitflag,	"Infinite ammo",		{&menu_dmflags},		{0}, {0}, {0}, {(value_t *)DF_INFINITE_AMMO} },
	{ bitflag,	"No monsters",			{&menu_dmflags},		{0}, {0}, {0}, {(value_t *)DF_NO_MONSTERS} },
	{ bitflag,	"Monsters respawn",		{&menu_dmflags},		{0}, {0}, {0}, {(value_t *)DF_MONSTERS_RESPAWN} },
	{ bitflag,	"Items respawn",		{&menu_dmflags},		{0}, {0}, {0}, {(value_t *)DF_ITEMS_RESPAWN} },
	{ bitflag,	"Mega powerups respawn",{&menu_dmflags},		{0}, {0}, {0}, {(value_t *)DF_RESPAWN_SUPER} },
	{ bitflag,	"Fast monsters",		{&menu_dmflags},		{0}, {0}, {0}, {(value_t *)DF_FAST_MONSTERS} },
	{ bitflag,	"Allow jump",			{&menu_dmflags},		{1}, {0}, {0}, {(value_t *)DF_NO_JUMP} },
	{ bitflag,	"Allow freelook",		{&menu_dmflags},		{1}, {0}, {0}, {(value_t *)DF_NO_FREELOOK} },
	{ bitflag,	"Allow FOV",			{&menu_dmflags},		{1}, {0}, {0}, {(value_t *)DF_NO_FOV} },
	{ bitflag,	"Drop weapons (DM)",	{&menu_dmflags2},	{0}, {0}, {0}, {(value_t *)DF2_YES_WEAPONDROP} },
	{ bitflag,	"Don't spawn runes (DM)",	{&menu_dmflags2},	{0}, {0}, {0}, {(value_t *)DF2_NO_RUNES} },
	{ bitflag,	"Instant flag return (ST/CTF)",{&menu_dmflags2},	{0}, {0}, {0}, {(value_t *)DF2_INSTANT_RETURN} },
	{ bitflag,	"No team switching (ST/CTF)",	{&menu_dmflags2},	{0}, {0}, {0}, {(value_t *)DF2_NO_TEAM_SWITCH} },
	{ bitflag,	"Server picks teams (ST/CTF)",	{&menu_dmflags2},	{0}, {0}, {0}, {(value_t *)DF2_NO_TEAM_SELECT} },
	{ bitflag,	"Double ammo (DM)",		{&menu_dmflags2},	{0}, {0}, {0}, {(value_t *)DF2_YES_DOUBLEAMMO} },
	{ bitflag,	"Degeneration (DM)",	{&menu_dmflags2},	{0}, {0}, {0}, {(value_t *)DF2_YES_DEGENERATION} },
	{ bitflag,	"Allow BFG aiming",		{&menu_dmflags2},	{0}, {0}, {0}, {(value_t *)DF2_YES_FREEAIMBFG} },
	{ bitflag,	"Barrels respawn (DM)",	{&menu_dmflags2},	{0}, {0}, {0}, {(value_t *)DF2_BARRELS_RESPAWN} },
	{ bitflag,	"No respawn protection (DM)",{&menu_dmflags2},	{0}, {0}, {0}, {(value_t *)DF2_NO_RESPAWN_INVUL} },
	{ bitflag,	"Start with shotgun",{&menu_dmflags2},	{0}, {0}, {0}, {(value_t *)DF2_COOP_SHOTGUNSTART} },
	{ bitflag,	"Spawn where died (coop)",{&menu_dmflags2},	{0}, {0}, {0}, {(value_t *)DF2_SAME_SPAWN_SPOT} },
};

static menu_t SkirmishDMFlagsMenu =
{
	"GAMEPLAY OPTIONS",
	0,
	countof(SkirmishDMFlagsItems),
	0,
	SkirmishDMFlagsItems,
};

/*=======================================
 *
 * Bot Setup Menu
 *
 *=======================================*/

static menuitem_t BotSetupItems[] = {
	{ botslot,	"Slot 1:",					{NULL},					{0.0}, {0.0}, {0.0}, {NULL} },
	{ botslot,	"Slot 2:",					{NULL},					{0.0}, {0.0}, {0.0}, {NULL} },
	{ botslot,	"Slot 3:",					{NULL},					{0.0}, {0.0}, {0.0}, {NULL} },
	{ botslot,	"Slot 4:",					{NULL},					{0.0}, {0.0}, {0.0}, {NULL} },
	{ botslot,	"Slot 5:",					{NULL},					{0.0}, {0.0}, {0.0}, {NULL} },
	{ botslot,	"Slot 6:",					{NULL},					{0.0}, {0.0}, {0.0}, {NULL} },
	{ botslot,	"Slot 7:",					{NULL},					{0.0}, {0.0}, {0.0}, {NULL} },
	{ botslot,	"Slot 8:",					{NULL},					{0.0}, {0.0}, {0.0}, {NULL} },
	{ botslot,	"Slot 9:",					{NULL},					{0.0}, {0.0}, {0.0}, {NULL} },
	{ botslot,	"Slot 10:",					{NULL},					{0.0}, {0.0}, {0.0}, {NULL} },
	{ botslot,	"Slot 11:",					{NULL},					{0.0}, {0.0}, {0.0}, {NULL} },
	{ botslot,	"Slot 12:",					{NULL},					{0.0}, {0.0}, {0.0}, {NULL} },
	{ botslot,	"Slot 13:",					{NULL},					{0.0}, {0.0}, {0.0}, {NULL} },
	{ botslot,	"Slot 14:",					{NULL},					{0.0}, {0.0}, {0.0}, {NULL} },
	{ botslot,	"Slot 15:",					{NULL},					{0.0}, {0.0}, {0.0}, {NULL} },
	{ botslot,	"Slot 16:",					{NULL},					{0.0}, {0.0}, {0.0}, {NULL} },
	{ redtext,	" ",						{NULL},					{0.0}, {0.0}, {0.0}, {NULL}  },
	{ more,		"Clear list",				{NULL},					{0.0}, {0.0}, {0.0}, {(value_t *)M_ClearBotSlotList} },
	{ more,		"Start game!",				{NULL},					{0.0}, {0.0}, {0.0}, {(value_t *)M_StartSkirmishGame} },
};

menu_t BotSetupMenu = {
	"BOT SETUP",
	0,
	countof(BotSetupItems),
	0,
	BotSetupItems,
	0,
	0,
	0,
	NULL,
	false,
	NULL,
	MNF_ALIGNLEFT,
};

/*=======================================
 *
 * Team Bot Setup Menu
 *
 *=======================================*/

// [CW] Add to this when bumping 'MAX_TEAMS'.
static menuitem_t TeamBotSetupItems[] = {
	{ redtext,	"Team 1",					{NULL},					{0.0}, {0.0}, {0.0}, {NULL}  },
	{ redtext,	" ",						{NULL},					{0.0}, {0.0}, {0.0}, {NULL}  },
	{ botslot,	"Slot 1:",					{NULL},					{0.0}, {0.0}, {0.0}, {NULL} },
	{ botslot,	"Slot 2:",					{NULL},					{0.0}, {0.0}, {0.0}, {NULL} },
	{ botslot,	"Slot 3:",					{NULL},					{0.0}, {0.0}, {0.0}, {NULL} },
	{ botslot,	"Slot 4:",					{NULL},					{0.0}, {0.0}, {0.0}, {NULL} },
	{ botslot,	"Slot 5:",					{NULL},					{0.0}, {0.0}, {0.0}, {NULL} },
	{ redtext,	" ",						{NULL},					{0.0}, {0.0}, {0.0}, {NULL}  },
	{ redtext,	"Team 2",					{NULL},					{0.0}, {0.0}, {0.0}, {NULL}  },
	{ redtext,	" ",						{NULL},					{0.0}, {0.0}, {0.0}, {NULL}  },
	{ botslot,	"Slot 1:",					{NULL},					{0.0}, {0.0}, {0.0}, {NULL} },
	{ botslot,	"Slot 2:",					{NULL},					{0.0}, {0.0}, {0.0}, {NULL} },
	{ botslot,	"Slot 3:",					{NULL},					{0.0}, {0.0}, {0.0}, {NULL} },
	{ botslot,	"Slot 4:",					{NULL},					{0.0}, {0.0}, {0.0}, {NULL} },
	{ botslot,	"Slot 5:",					{NULL},					{0.0}, {0.0}, {0.0}, {NULL} },
	{ redtext,	" ",						{NULL},					{0.0}, {0.0}, {0.0}, {NULL}  },
	{ redtext,	"Team 3",					{NULL},					{0.0}, {0.0}, {0.0}, {NULL}  },
	{ redtext,	" ",						{NULL},					{0.0}, {0.0}, {0.0}, {NULL}  },
	{ botslot,	"Slot 1:",					{NULL},					{0.0}, {0.0}, {0.0}, {NULL} },
	{ botslot,	"Slot 2:",					{NULL},					{0.0}, {0.0}, {0.0}, {NULL} },
	{ botslot,	"Slot 3:",					{NULL},					{0.0}, {0.0}, {0.0}, {NULL} },
	{ botslot,	"Slot 4:",					{NULL},					{0.0}, {0.0}, {0.0}, {NULL} },
	{ botslot,	"Slot 5:",					{NULL},					{0.0}, {0.0}, {0.0}, {NULL} },
	{ redtext,	" ",						{NULL},					{0.0}, {0.0}, {0.0}, {NULL}  },
	{ redtext,	"Team 4",					{NULL},					{0.0}, {0.0}, {0.0}, {NULL}  },
	{ redtext,	" ",						{NULL},					{0.0}, {0.0}, {0.0}, {NULL}  },
	{ botslot,	"Slot 1:",					{NULL},					{0.0}, {0.0}, {0.0}, {NULL} },
	{ botslot,	"Slot 2:",					{NULL},					{0.0}, {0.0}, {0.0}, {NULL} },
	{ botslot,	"Slot 3:",					{NULL},					{0.0}, {0.0}, {0.0}, {NULL} },
	{ botslot,	"Slot 4:",					{NULL},					{0.0}, {0.0}, {0.0}, {NULL} },
	{ botslot,	"Slot 5:",					{NULL},					{0.0}, {0.0}, {0.0}, {NULL} },
	{ redtext,	" ",						{NULL},					{0.0}, {0.0}, {0.0}, {NULL}  },
	{ more,		"Clear list",				{NULL},					{0.0}, {0.0}, {0.0}, {(value_t *)M_ClearBotSlotList} },
	{ more,		"Start game!",				{NULL},					{0.0}, {0.0}, {0.0}, {(value_t *)M_StartSkirmishGame} },
};

menu_t TeamBotSetupMenu = {
	"BOT SETUP",
	2,
	countof(TeamBotSetupItems),
	0,
	TeamBotSetupItems,
	0,
	0,
	0,
	NULL,
	false,
	NULL,
	MNF_ALIGNLEFT,
};

void M_BotSetup( void )
{
	ULONG		ulIdx;
	FBaseCVar	*pVar;
	UCVarValue	Val;
	char		szCVarName[32];

	if ( GAMEMODE_GetFlags( static_cast<GAMEMODE_e>(menu_gamemode.GetGenericRep( CVAR_Int ).Int) ) & GMF_PLAYERSONTEAMS )
	{
		for ( ulIdx = 0; ulIdx < MAX_BOTTEAMSLOTS; ulIdx++ )
		{
			sprintf( szCVarName, "menu_teambotspawn%d", static_cast<unsigned int> (ulIdx) );
			pVar = FindCVar( szCVarName, NULL );

			Val = pVar->GetGenericRep( CVAR_Int );

			if ( BOTINFO_GetRevealed( Val.Int ) == false )
			{
				Val.Int = -1;
				pVar->SetGenericRep( Val, CVAR_Int );
			}
		}

		M_SwitchMenu( &TeamBotSetupMenu );
	}
	else
	{
		for ( ulIdx = 0; ulIdx < 16; ulIdx++ )
		{
			sprintf( szCVarName, "menu_botspawn%d", static_cast<unsigned int> (ulIdx) );
			pVar = FindCVar( szCVarName, NULL );

			Val = pVar->GetGenericRep( CVAR_Int );

			if ( BOTINFO_GetRevealed( Val.Int ) == false )
			{
				Val.Int = -1;
				pVar->SetGenericRep( Val, CVAR_Int );
			}
		}

		M_SwitchMenu( &BotSetupMenu );
	}
}

/*=======================================
 *
 * [BB] Player Class Selection Menu
 *
 *=======================================*/

CVAR( Int, menu_teamplayerclass, 0, 0 );

static TArray<valuestring_t> AvailablePlayerClasses;
static ULONG g_ulDesiredTeam = 0;

static	void	SelectClassAndJoinTeam( void );

static menuitem_t PlayerClassSelectionItems[] =
{
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ discretes,	"Class",				{&menu_teamplayerclass},		   		{1.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ more,		"Join game",			{NULL},					{0.0}, {0.0},	{0.0}, {(value_t *)SelectClassAndJoinTeam} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
};

// [BB] Line number of the "Class" entry from PlayerClassSelectionItems.
// If the line number is changed, the value has to be adjusted.
#define PLAYERCLASSES_INDEX 6

static void SelectClassAndJoinTeam( void )
{
	char command[1024];

	playerclass = AvailablePlayerClasses[menu_teamplayerclass].name.GetChars();
	// [BB] If the class selection menu is used in non-team games or the random team 
	// is chosen in a team game, g_ulDesiredTeam is teams.Size( ). Using "join" in both
	// cases should work fine, although in team games this will be more like
	// "team autoselect" instead of "team random". For gameplay purposes the former
	// is better anyway.
	if ( g_ulDesiredTeam == teams.Size( ) )
		sprintf ( command, "join" );
	else
		sprintf ( command, "team \"%s\"", TEAM_GetName( g_ulDesiredTeam ) );
	AddCommandString( command );
	M_ClearMenus( );
}

menu_t PlayerClassSelectionMenu =
{
	"Player Class Selection",
	8,
	countof(PlayerClassSelectionItems),
	0,
	PlayerClassSelectionItems,
	0,
	0,
	0,
	NULL,
	false,
	NULL,
	MNF_ALIGNLEFT,
};

static void InitAvailablePlayerClassList( ULONG ulTeam )
{
	valuestring_t value;
	g_ulDesiredTeam = ulTeam;
	AvailablePlayerClasses.Clear();
	for ( ULONG ulIdx = 0; ulIdx < PlayerClasses.Size(); ++ulIdx )
	{
		if ( TEAM_IsClassAllowedForTeam( ulIdx, ulTeam ) )
		{
			value.value = static_cast<float> ( AvailablePlayerClasses.Size() );
			value.name = PlayerClasses[ulIdx].Type->Meta.GetMetaString (APMETA_DisplayName);
			AvailablePlayerClasses.Push ( value );
		}
	}
	value.value = static_cast<float> ( AvailablePlayerClasses.Size() );
	static char random[] = "Random";
	value.name = random;
	AvailablePlayerClasses.Push ( value );
	PlayerClassSelectionItems[PLAYERCLASSES_INDEX].b.numvalues = static_cast<float>(AvailablePlayerClasses.Size());
	PlayerClassSelectionItems[PLAYERCLASSES_INDEX].e.valuestrings = &AvailablePlayerClasses[0];
}

/*=======================================
 *
 * Join Team Menu
 *
 *=======================================*/

CVAR( Int, menu_teamidxjointeammenu, 0, 0 );

static TArray<valuestring_t> AvailableTeams;

static	void	AutoSelectTeam( void );
static	void	JoinTeam( void );
static	void	JoinRandom( void );
static	void	ShowHelp( void );

static menuitem_t JoinTeamItems[] =
{
	{ redtext,	"Please select a team.",{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ more,		"Auto-select",			{NULL},					{0.0}, {0.0},	{0.0}, {(value_t *)AutoSelectTeam} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ discretes,	"Team",				{&menu_teamidxjointeammenu},		   		{1.0}, {0.0},	{0.0}, {NULL} },
	{ more,		"Join game",			{NULL},					{0.0}, {0.0},	{0.0}, {(value_t *)JoinTeam} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ more,		"How to play",			{NULL},					{0.0}, {0.0},	{0.0}, { (value_t *)ShowHelp} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
};

// [BB] Line number of the "Team" entry from JoinTeamItems.
// If the line number is changed, the value has to be adjusted.
#define PLAYERTEAMS_INDEX 7

static void AutoSelectTeam( void )
{
	static char autoselect[] = "team autoselect";
	AddCommandString( autoselect );
	M_ClearMenus( );
}

// [RC]  ...F1 DAMMIT!
static void ShowHelp( void )
{
	M_ClearMenus( );
	static char menu_help[] = "menu_help";
	AddCommandString( menu_help );
}

static void JoinTeam( void )
{
	// [BB] Get the name of the selected team, "random" in case "( menu_teamidxjointeammenu == ( AvailableTeams.Size() - 1 ) )"
	FString teamName = AvailableTeams[menu_teamidxjointeammenu].name.GetChars();
	V_RemoveColorCodes ( teamName );
	if( (PlayerClasses.Size() > 1) && ( static_cast<unsigned> (menu_teamidxjointeammenu) < ( AvailableTeams.Size() - 1 ) ) )
	{
		InitAvailablePlayerClassList( TEAM_GetTeamNumberByName ( teamName ) );
		M_SwitchMenu (&PlayerClassSelectionMenu);
	}
	else
	{
		char command[1024];
		sprintf ( command, "team \"%s\"", teamName.GetChars() );
		AddCommandString( command );
		M_ClearMenus( );
	}
}

menu_t JoinTeamMenu =
{
	"JOIN TEAM",
	5,
	countof(JoinTeamItems),
	0,
	JoinTeamItems,
	0,
	0,
	0,
	NULL,
	false,
	NULL,
	MNF_ALIGNLEFT,
};

static void InitAvailableTeamsList( )
{
	valuestring_t value;
	AvailableTeams.Clear();
	for ( ULONG ulIdx = 0; ulIdx < teams.Size(); ++ulIdx )
	{
		if ( TEAM_CheckIfValid( ulIdx ) )
		{
			value.value = static_cast<float> ( AvailableTeams.Size() );
			value.name.Format ( "\\c%c%s", V_GetColorChar( TEAM_GetTextColor( ulIdx ) ), TEAM_GetName ( ulIdx ) );
			V_ColorizeString ( value.name );
			AvailableTeams.Push ( value );
		}
	}
	value.value = static_cast<float> ( AvailableTeams.Size() );
	static char random[] = "Random";
	value.name = random;
	AvailableTeams.Push ( value );
	JoinTeamItems[PLAYERTEAMS_INDEX].b.numvalues = static_cast<float>(AvailableTeams.Size());
	JoinTeamItems[PLAYERTEAMS_INDEX].e.valuestrings = &AvailableTeams[0];
}

/*=======================================
 *
 * Join Game Menu [RC]
 *
 *=======================================*/

static	void	JoinGame( void );

static menuitem_t JoinItems[] =
{
	{ redtext,	"You are spectating.",	{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ more,		"Join game",			{NULL},					{0.0}, {0.0},	{0.0}, {(value_t *)JoinGame} },
	{ more,		"How to play",			{NULL},					{0.0}, {0.0},	{0.0}, { (value_t *)ShowHelp} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
};


// [RC] For the join menu
static void JoinGame( void )
{
	// [BB] If there are several classes to choose from, we just hijack PlayerClassSelectionMenu
	// that I originally intended for team based modes to respect the limitedtoteam property.
	if( PlayerClasses.Size() > 1 )
	{
		InitAvailablePlayerClassList( teams.Size( ) );
		M_SwitchMenu (&PlayerClassSelectionMenu);
	}
	else
	{
		static char join[] = "join";
		AddCommandString( join );
		M_ClearMenus( );
	}
}

menu_t JoinMenu =
{
	"JOIN Game",
	7,
	countof(JoinItems),
	0,
	JoinItems,
	0,
	0,
	0,
	NULL,
	false,
	NULL,
	MNF_CENTERED,
};

static value_t FilterModes[] =
{
	{ 1.0, "None" },
	{ 2.0, "Linear" },
   { 3.0, "None (mipmapped)" },
   { 4.0, "Bilinear" },
   { 5.0, "Trilinear" },
};

static value_t HqResizeModes[] =
{
   { 0.0, "Off" },
   { 1.0, "2X" },
   { 2.0, "3X" },
   { 3.0, "4X" },
};

static value_t ParticleStyles[] =
{
	{ 0.0, "Rectangle" },
	{ 1.0, "Round" },
	{ 2.0, "Smooth" },
};

static value_t TextureFormats[] =
{
	{ 1.0, "RGBA2" },
	{ 2.0, "RGB5_A1" },
	{ 3.0, "RGBA8" },
};

static value_t Anisotropy[] =
{
	{ 2.0, "2x" },
	{ 4.0, "4x" },
	{ 8.0, "8x" },
	{ 16.0, "16x" },
};

/*=======================================
 *
 * Text Scaling Menu
 *
 *=======================================*/

CVAR( Float, menu_textsizescalar, 0.0f, 0 )

void	TextScalingMenuDrawer( void );

static menuitem_t TextScalingMenuItems[] = {
	{ discrete,	"Enable text scaling",	{&con_scaletext},		{2.0},	{0.0},	{0.0},	{OnOff} },
	{ txslider,	"Text size scalar",		{&menu_textsizescalar},	{0.0},	{0.0},	{1.0},	{NULL} },
	{ mnnumber,	"Virtual width",		{&con_virtualwidth},	{0.0},	{0.0},	{0.0},	{NULL} },
	{ mnnumber,	"Virtual height",		{&con_virtualheight},	{0.0},	{0.0},	{0.0},	{NULL} },
	{ discrete,	"Use screen ratio",				{&con_scaletext_usescreenratio},				{2.0}, {0.0}, {0.0}, {OnOff} },
	{ redtext,	" ",					{NULL},					{0.0},	{0.0},	{0.0},	{NULL} },
	{ redtext,	" ",					{NULL},					{0.0},	{0.0},	{0.0},	{NULL} },
	{ whitetext,"SAMPLE TEXT",			{NULL},					{0.0},	{0.0},	{0.0},	{NULL} },
};

menu_t TextScalingMenu =
{
	"TEXT SCALING",
	0,
	countof(TextScalingMenuItems),
	0,
	TextScalingMenuItems,
	0,
	0,
	0,
	TextScalingMenuDrawer,
	false,
	NULL,
};

void SetupTextScalingMenu( void )
{
	int			iNumModes;
	int			iWidth;
	int			iHeight;
	UCVarValue	Val;
	bool		bLetterBox;

	iNumModes = 0;
	Video->StartModeIterator (8, true);
	while (Video->NextMode (&iWidth, &iHeight, &bLetterBox))
	{
		if (( iWidth <= con_virtualwidth ) && ( iHeight <= con_virtualheight ))
		{
			Val.Float = (float)iNumModes;
			menu_textsizescalar.SetGenericRep( Val, CVAR_Float );
		}

		iNumModes++;
	}

	TextScalingMenuItems[1].c.max = iNumModes - 1;
	M_SwitchMenu( &TextScalingMenu );
}

void TextScalingMenuDrawer( void )
{
	char		szString[128];
	UCVarValue	ValWidth;
	UCVarValue	ValHeight;
	float		fXScale;
	float		fYScale = 0.0f;
	bool		bScale;

	bScale = false;
	if (( con_virtualwidth > 0 ) && ( con_virtualheight > 0 ))
	{
		ValWidth = con_virtualwidth.GetGenericRep( CVAR_Int );
		ValHeight = con_virtualheight.GetGenericRep( CVAR_Int );

		fXScale =  (float)ValWidth.Int / 320.0f;
		fYScale =  (float)ValHeight.Int / 200.0f;

		bScale = true;
	}

	sprintf( szString, "This is clean text." );
	screen->DrawText( SmallFont, CR_WHITE,
		160 - ( SmallFont->StringWidth( szString ) / 2 ),
		96,
		szString,
		DTA_Clean, true,
		TAG_DONE );

	sprintf( szString, "This is unscaled text." );
	screen->DrawText( SmallFont, CR_WHITE,
		( (float)screen->GetWidth( ) / 320.0f ) * 160 - ( SmallFont->StringWidth( szString ) / 2 ),
		( (float)screen->GetHeight( ) / 200.0f ) * 112,
		szString,
		TAG_DONE );

	if ( bScale )
	{
		sprintf( szString, "This is scaled text.\n" );
		screen->DrawText( SmallFont, CR_WHITE,
			(LONG)(( ValWidth.Int / 2 ) - ( SmallFont->StringWidth( szString ) / 2 )),
			( 128 * fYScale ) - ( SmallFont->GetHeight( ) / 2 ),
			szString,
			DTA_VirtualWidth, ValWidth.Int,
			DTA_VirtualHeight, ValHeight.Int,
			TAG_DONE );
	}
}

static void ActivateConfirm (char *text, void (*func)())
{
	ConfirmItems[0].label = text;
	ConfirmItems[0].e.mfunc = func;
	ConfirmMenu.lastOn = 3;
	M_SwitchMenu (&ConfirmMenu);
}

static void ConfirmIsAGo ()
{
	M_PopMenuStack ();
	ConfirmItems[0].e.mfunc ();
}

//
//		Set some stuff up for the video modes menu
//
static BYTE BitTranslate[32];

void M_OptInit (void)
{
	if (gameinfo.gametype & GAME_DoomChex)
	{
		LabelColor = CR_UNTRANSLATED;
		ValueColor = CR_GRAY;
		MoreColor = CR_GRAY;
	}
	else if (gameinfo.gametype == GAME_Heretic)
	{
		LabelColor = CR_GREEN;
		ValueColor = CR_UNTRANSLATED;
		MoreColor = CR_UNTRANSLATED;
	}
	else // Hexen
	{
		LabelColor = CR_RED;
		ValueColor = CR_UNTRANSLATED;
		MoreColor = CR_UNTRANSLATED;
	}

	if (gl_disabled)
	{
		// If the GL system is permanently disabled change the GL menu items.
		VideoItems[1].label = "Enable OpenGL system";
		ModesItems[1].type = nochoice;
	}
}

void M_InitVideoModesMenu ()
{
	int dummy1, dummy2;
	size_t currval = 0;

	// Nothing here is needed for servers.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		return;

	M_RefreshModesList();

	for (unsigned int i = 1; i <= 32 && currval < countof(Depths); i++)
	{
		Video->StartModeIterator (i, screen->IsFullscreen());
		if (Video->NextMode (&dummy1, &dummy2, NULL))
		{
			/*
			Depths[currval].value = currval;
			mysnprintf (name, countof(name), "%d bit", i);
			Depths[currval].name = copystring (name);
			*/
			BitTranslate[currval++] = i;
		}
	}

	//ModesItems[VM_DEPTHITEM].b.min = (float)currval;

	switch (Video->GetDisplayType ())
	{
	case DISPLAY_FullscreenOnly:
		ModesItems[2].type = nochoice;
		ModesItems[2].b.min = 1.f;
		break;
	case DISPLAY_WindowOnly:
		ModesItems[2].type = nochoice;
		ModesItems[2].b.min = 0.f;
		break;
	default:
		break;
	}

	if (gameinfo.gametype == GAME_Doom)
	{
		LabelColor = CR_UNTRANSLATED;
		ValueColor = CR_GRAY;
		MoreColor = CR_GRAY;
	}
	else if (gameinfo.gametype == GAME_Heretic)
	{
		LabelColor = CR_GREEN;
		ValueColor = CR_UNTRANSLATED;
		MoreColor = CR_UNTRANSLATED;
	}
	else // Hexen
	{
		LabelColor = CR_RED;
		ValueColor = CR_UNTRANSLATED;
		MoreColor = CR_UNTRANSLATED;
	}

	g_bSwitchColorBack = false;
	g_lSavedColor = 0;
}

//
//		Toggle messages on/off
//
void M_ChangeMessages ()
{
	if (show_messages)
	{
		Printf (128, "%s\n", GStrings("MSGOFF"));
		show_messages = false;
	}
	else
	{
		Printf (128, "%s\n", GStrings("MSGON"));
		show_messages = true;
	}
}

CCMD (togglemessages)
{
	M_ChangeMessages ();
}

void M_SizeDisplay (int diff)
{
	// changing screenblocks automatically resizes the display
	screenblocks = screenblocks + diff;
}

CCMD (sizedown)
{
	M_SizeDisplay (-1);
	S_Sound (CHAN_VOICE | CHAN_UI, "menu/change", 1, ATTN_NONE);
}

CCMD (sizeup)
{
	M_SizeDisplay (1);
	S_Sound (CHAN_VOICE | CHAN_UI, "menu/change", 1, ATTN_NONE);
}

// Draws a string in the console font, scaled to the 8x8 cells
// used by the default console font.
static void M_DrawConText (int color, int x, int y, const char *str)
{
	int len = (int)strlen(str);

	x = int((x - 160) * (CleanXfac) + screen->GetWidth() / 2);
	y = int((y - 100) * (CleanYfac) + screen->GetHeight() / 2);
	screen->DrawText (ConFont, color, x, y, str,
		DTA_CellX, int(8 * CleanXfac),
		DTA_CellY, int(8 * CleanYfac),
		TAG_DONE);
}

void M_BuildKeyList (menuitem_t *item, int numitems)
{
	int i;

	for (i = 0; i < numitems; i++, item++)
	{
		if (item->type == control)
			C_GetKeysForCommand (item->e.command, &item->b.key1, &item->c.key2);
	}
}

static void CalcIndent (menu_t *menu)
{
	int i, widest = 0, thiswidth;
	menuitem_t *item;

	for (i = 0; i < menu->numitems; i++)
	{
		item = menu->items + i;
		if (item->type != whitetext && item->type != redtext && item->type != browserheader && item->type != browserslot && item->type != screenres &&
			(item->type != discrete || !item->c.discretecenter))
		{
			thiswidth = SmallFont->StringWidth (item->label);
			if (thiswidth > widest)
				widest = thiswidth;
		}
	}
	menu->indent = widest + 4;
}

void M_SwitchMenu (menu_t *menu)
{
	MenuStack[MenuStackDepth].menu.newmenu = menu;
	MenuStack[MenuStackDepth].isNewStyle = true;
	MenuStack[MenuStackDepth].drawSkull = false;
	MenuStackDepth++;

	CanScrollUp = false;
	CanScrollDown = false;
	CurrentMenu = menu;
	CurrentItem = menu->lastOn;

	if (!menu->indent)
	{
		CalcIndent (menu);
	}

	flagsvar = 0;
}

// [BC]
bool M_StartMultiplayerMenu (void)
{
	M_SwitchMenu (&MultiplayerMenu);
	return true;
}

bool M_StartOptionsMenu (void)
{
	M_SwitchMenu (&OptionMenu);
	return true;
}

// [RC] Handler for the space key. Shows the appropiate menu or message.
bool M_JoinMenu ( void )
{
	if (( GAMEMODE_GetFlags( GAMEMODE_GetCurrentMode( )) & GMF_DEADSPECTATORS ) && players[consoleplayer].bDeadSpectator )
	{
		Printf( "You cannot rejoin the game until the round is over!\n" );
		return ( true );
	}

	// ST/CTF/domination without a selection room, or another team game.
	if ( ( GAMEMODE_GetFlags( GAMEMODE_GetCurrentMode( )) & GMF_PLAYERSONTEAMS ) &&
		( !( GAMEMODE_GetFlags( GAMEMODE_GetCurrentMode( )) & GMF_TEAMGAME ) || ( TemporaryTeamStarts.Size( ) == 0 ) ) &&
		(( dmflags2 & DF2_NO_TEAM_SELECT ) == false ))
	{
		M_StartControlPanel( true );
		M_StartJoinTeamMenu( );
	}
	else
	{
		M_StartControlPanel( true );
		M_StartJoinMenu( );
	}

	return true;
}

bool M_StartJoinTeamMenu (void)
{
	OptionsActive = true;
	InitAvailableTeamsList ();
	M_SwitchMenu (&JoinTeamMenu);
	return true;
}



bool M_StartJoinMenu (void)
{
	OptionsActive = true;
	M_SwitchMenu (&JoinMenu);
	return true;
}


void M_DrawSlider (int x, int y, float min, float max, float cur)
{
	float range;

	range = max - min;

	if (cur > max)
		cur = max;
	else if (cur < min)
		cur = min;

	cur -= min;

	M_DrawConText(CR_WHITE, x, y, "\x10\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x12");
	// [BC] Prevent a divide by 0 crash.
	if ( range != 0 )
		M_DrawConText(CR_ORANGE, x + 5 + (int)((cur * 78.f) / range), y, "\x13");
}

int M_FindCurVal (float cur, value_t *values, int numvals)
{
	int v;

	for (v = 0; v < numvals; v++)
		if (values[v].value == cur)
			break;

	return v;
}

int M_FindCurVal (float cur, valuestring_t *values, int numvals)
{
	int v;

	for (v = 0; v < numvals; v++)
		if (values[v].value == cur)
			break;

	return v;
}

int M_FindCurGUID (const GUID &guid, GUIDName *values, int numvals)
{
	int v;

	for (v = 0; v < numvals; v++)
		if (memcmp (&values[v].ID, &guid, sizeof(GUID)) == 0)
			break;

	return v;
}

const char *M_FindCurVal(const char *cur, valueenum_t *values, int numvals)
{
	for (int v = 0; v < numvals; ++v)
	{
		if (stricmp(values[v].value, cur) == 0)
		{
			return values[v].name;
		}
	}
	return cur;
}

const char *M_FindPrevVal(const char *cur, valueenum_t *values, int numvals)
{
	for (int v = 0; v < numvals; ++v)
	{
		if (stricmp(values[v].value, cur) == 0)
		{
			return values[v == 0 ? numvals - 1 : v - 1].value;
		}
	}
	return values[0].value;
}

const char *M_FindNextVal(const char *cur, valueenum_t *values, int numvals)
{
	for (int v = 0; v < numvals; ++v)
	{
		if (stricmp(values[v].value, cur) == 0)
		{
			return values[v == numvals - 1 ? 0 : v + 1].value;
		}
	}
	return values[0].value;
}

void M_OptDrawer ()
{
	EColorRange color = CR_UNDEFINED;
	int y, width, i, x = 0, ytop, fontheight;
	menuitem_t *item;
	UCVarValue value;
	DWORD overlay;
	int labelofs;
	int indent;

	if (!CurrentMenu->DontDim)
	{
		screen->Dim ();
	}

	if (CurrentMenu->PreDraw !=  NULL)
	{
		CurrentMenu->PreDraw ();
	}

	if (CurrentMenu->y != 0)
	{
		y = CurrentMenu->y;
	}
	else
	{
		if (BigFont && CurrentMenu->texttitle)
		{
			screen->DrawText (BigFont, gameinfo.gametype & GAME_DoomChex ? CR_RED : CR_UNTRANSLATED,
				160-BigFont->StringWidth (CurrentMenu->texttitle)/2, 10,
				CurrentMenu->texttitle, DTA_Clean, true, TAG_DONE);
			y = 15 + BigFont->GetHeight ();
		}
		else
		{
			y = 15;
		}
	}
	if (gameinfo.gametype & GAME_Raven)
	{
		labelofs = 2;
		y -= 2;
		fontheight = 9;
	}
	else
	{
		labelofs = 0;
		fontheight = 8;
	}

	ytop = y + CurrentMenu->scrolltop * 8;

	for (i = 0; i < CurrentMenu->numitems && y <= 200 - SmallFont->GetHeight(); i++, y += fontheight)
	{
		if (i == CurrentMenu->scrolltop)
		{
			i += CurrentMenu->scrollpos;
		}

		item = CurrentMenu->items + i;
		overlay = 0;
		if (item->type == discrete && item->c.discretecenter)
		{
			indent = 160;
		}
		else
		{
			indent = CurrentMenu->indent;
		}

		if (item->type != screenres && item->type != browserslot)
		{
			width = SmallFont->StringWidth (item->label);
			switch (item->type)
			{
			case more:
			case safemore:
				// Align this menu to the left-hand side.
				if ( CurrentMenu->iFlags & MNF_ALIGNLEFT )
					x = 32;
				else if ( CurrentMenu->iFlags & MNF_CENTERED )
					x = 160 - ( SmallFont->StringWidth( item->label ) / 2 );
				else
					x = indent - width;
				color = MoreColor;
				break;

			case numberedmore:
			case rsafemore:
			case rightmore:
				x = indent + 14;
				color = item->type != rightmore ? CR_GREEN : MoreColor;
				break;

			case redtext:
				x = 160 - width / 2;
				color = LabelColor;
				break;

			case whitetext:
				x = 160 - width / 2;
				color = CR_GOLD;//ValueColor;
				break;

			case listelement:
				x = indent + 14;
				color = LabelColor;
				break;

			case colorpicker:
				x = indent + 14;
				color = MoreColor;
				break;

			case browserheader:

				screen->DrawText( SmallFont, CR_UNTRANSLATED, 16, y, "PING", DTA_Clean, true, TAG_DONE );
				screen->DrawText( SmallFont, CR_UNTRANSLATED, 48, y, "NAME", DTA_Clean, true, TAG_DONE );
				screen->DrawText( SmallFont, CR_UNTRANSLATED, /*128*/160, y, "MAP", DTA_Clean, true, TAG_DONE );
//				screen->DrawText( SmallFont, CR_UNTRANSLATED, 160, y, "WAD", DTA_Clean, true, TAG_DONE );
				screen->DrawText( SmallFont, CR_UNTRANSLATED, 224, y, "TYPE", DTA_Clean, true, TAG_DONE );
				screen->DrawText( SmallFont, CR_UNTRANSLATED, 272, y, "PLYRS", DTA_Clean, true, TAG_DONE );
				break;

			case discrete:
				if (item->d.graycheck != NULL && !(**item->d.graycheck))
				{
					overlay = MAKEARGB(128,0,0,0);
				}
				// Intentional fall-through

			default:

				// Align this menu to the left-hand side.
				if ( CurrentMenu->iFlags & MNF_ALIGNLEFT )
					x = 32;
				else if ( CurrentMenu->iFlags & MNF_CENTERED )
					x = 160 - ( SmallFont->StringWidth( item->label ) / 2 );
				else
					x = indent - width;

				color = (item->type == control && menuactive == MENU_WaitKey && i == CurrentItem)
					? CR_YELLOW : LabelColor;
				break;
			}
			screen->DrawText (SmallFont, color, x, y, item->label, DTA_Clean, true, DTA_ColorOverlay, overlay, TAG_DONE);

			// [BC/BB] Skulltag's alignment handling.
			if ( CurrentMenu->iFlags & MNF_ALIGNLEFT )
				x = 32 + width + 6;
			else
				x = indent + 14;

			switch (item->type)
			{
			case numberedmore:
				if (item->b.position != 0)
				{
					char tbuf[16];

					mysnprintf (tbuf, countof(tbuf), "%d.", item->b.position);
					x = indent - SmallFont->StringWidth (tbuf);
					screen->DrawText (SmallFont, CR_GREY, x, y, tbuf, DTA_Clean, true, TAG_DONE);
				}
				break;
			case bitmask:
			{
				int v, vals;

				value = item->a.cvar->GetGenericRep (CVAR_Int);
				value.Float = value.Int & int(item->c.max);
				vals = (int)item->b.numvalues;

				v = M_FindCurVal (value.Float, item->e.values, vals);

				if (v == vals)
				{
					screen->DrawText (SmallFont, ValueColor, indent + 14, y, "Unknown",
						DTA_Clean, true, TAG_DONE);
				}
				else
				{
					screen->DrawText (SmallFont, item->type == cdiscrete ? v : ValueColor,
						indent + 14, y, item->e.values[v].name,
						DTA_Clean, true, TAG_DONE);
				}

			}
			break;

			case discretes:
			case discrete:
			case cdiscrete:
			case inverter:
			{
				int v, vals;

				overlay = 0;

				// [BC] Hack for autoaim.
				if ( strcmp( item->label, "Autoaim" ) == 0 )
				{
					char	szLabel[32];

					sprintf( szLabel, "%s",	menu_autoaim == 0 ? "Never" :
						menu_autoaim <= 0.25 ? "Very Low" :
						menu_autoaim <= 0.5 ? "Low" :
						menu_autoaim <= 1 ? "Medium" :
						menu_autoaim <= 2 ? "High" :
						menu_autoaim <= 3 ? "Very High" : "Always");

					screen->DrawText( SmallFont, ValueColor, x, y, szLabel, DTA_Clean, true, TAG_DONE );
				}
				else
				{
					value = item->a.cvar->GetGenericRep (CVAR_Float);
					if (item->type == inverter)
					{
						value.Float = (value.Float < 0.f);
						vals = 2;
					}
					else
					{
						vals = (int)item->b.numvalues;
					}
					if (item->type != discretes)
					{
						v = M_FindCurVal (value.Float, item->e.values, vals);
					}
					else
					{
						v = M_FindCurVal (value.Float, item->e.valuestrings, vals);
					}
					if (item->type == discrete)
					{
						if (item->d.graycheck != NULL && !(**item->d.graycheck))
						{
							overlay = MAKEARGB(96,48,0,0);
						}
					}

					// [BB] To handle MNF_ALIGNLEFT, "x" is used instead of "indent + 14"
					if (v == vals)
					{
						screen->DrawText (SmallFont, ValueColor, x, y, "Unknown",
							DTA_Clean, true, DTA_ColorOverlay, overlay, TAG_DONE);
					}
					else
					{
						screen->DrawText (SmallFont, item->type == cdiscrete ? v : ValueColor,
							x, y,
							item->type != discretes ? item->e.values[v].name : item->e.valuestrings[v].name.GetChars(),
							DTA_Clean, true, DTA_ColorOverlay, overlay, TAG_DONE);
					}
				}
			}
			break;

			case ediscrete:
			{
				const char *v;

				value = item->a.cvar->GetGenericRep (CVAR_String);
				v = M_FindCurVal(value.String, item->e.enumvalues, (int)item->b.numvalues);
				screen->DrawText(SmallFont, ValueColor, indent + 14, y, v, DTA_Clean, true, TAG_DONE);
			}
			break;

			case discrete_guid:
			{
				int v, vals;

				vals = (int)item->b.numvalues;
				v = M_FindCurGUID (*(item->a.guidcvar), item->e.guidvalues, vals);

				if (v == vals)
				{
					UCVarValue val = item->a.guidcvar->GetGenericRep (CVAR_String);
					screen->DrawText (SmallFont, ValueColor, x, y, val.String, DTA_Clean, true, TAG_DONE);
				}
				else
				{
					screen->DrawText (SmallFont, ValueColor, x, y, item->e.guidvalues[v].Name,
						DTA_Clean, true, TAG_DONE);
				}

			}
			break;

			case nochoice:
				screen->DrawText (SmallFont, CR_GOLD, x, y,
					(item->e.values[(int)item->b.min]).name, DTA_Clean, true, TAG_DONE);
				break;

			case slider:

				// [BC] Hack for player color.
				if ( strcmp( item->label, "Red" ) == 0 )
				{
					USHORT	usX;

					usX = SmallFont->StringWidth( "Green" ) + 8 + 32;

					M_DrawSlider( usX, y, 0.0f, 255.0f, RPART( g_ulPlayerSetupColor ));
				}
				else if ( strcmp( item->label, "Green" ) == 0 )
				{
					USHORT	usX;

					usX = SmallFont->StringWidth( "Green" ) + 8 + 32;

					M_DrawSlider( usX, y, 0.0f, 255.0f, GPART( g_ulPlayerSetupColor ));
				}
				else if ( strcmp( item->label, "Blue" ) == 0 )
				{
					USHORT	usX;

					usX = SmallFont->StringWidth( "Green" ) + 8 + 32;

					M_DrawSlider( usX, y, 0.0f, 255.0f, BPART( g_ulPlayerSetupColor ));
				}
				// Default.
				else
				{
					value = item->a.cvar->GetGenericRep (CVAR_Float);
					M_DrawSlider (x, y + labelofs, item->b.min, item->c.max, value.Float);
				}
				break;

			case absslider:
				value = item->a.cvar->GetGenericRep (CVAR_Float);
				M_DrawSlider (indent + 14, y + labelofs, item->b.min, item->c.max, fabs(value.Float));
				break;

			case intslider:
				M_DrawSlider (indent + 14, y + labelofs, item->b.min, item->c.max, item->a.fval);
				break;

			case control:
			{
				char description[64];

				C_NameKeys (description, item->b.key1, item->c.key2);
				if (description[0])
				{
					M_DrawConText(CR_WHITE, indent + 14, y-1+labelofs, description);
				}
				else
				{
					screen->DrawText(SmallFont, CR_BLACK, indent + 14, y + labelofs, "---",
						DTA_Clean, true, TAG_DONE);
				}
			}
			break;

			case colorpicker:
			{
				int box_x, box_y;
				box_x = (indent - 35 - 160) * CleanXfac + screen->GetWidth()/2;
				box_y = (y - ((gameinfo.gametype & GAME_Raven) ? 99 : 100)) * CleanYfac + screen->GetHeight()/2;
				screen->Clear (box_x, box_y, box_x + 32*CleanXfac, box_y + (fontheight-1)*CleanYfac,
					item->a.colorcvar->GetIndex(), 0);
			}
			break;

			case palettegrid:
			{
				int box_x, box_y;
				int x1, p;
				const int w = fontheight*CleanXfac;
				const int h = fontheight*CleanYfac;

				box_y = (y - 98) * CleanYfac + screen->GetHeight()/2;
				p = 0;
				box_x = (indent - 32 - 160) * CleanXfac + screen->GetWidth()/2;
				for (x1 = 0, p = int(item->b.min * 16); x1 < 16; ++p, ++x1)
				{
					screen->Clear (box_x, box_y, box_x + w, box_y + h, p, 0);
					if (p == CurrColorIndex || (i == CurrentItem && x1 == SelColorIndex))
					{
						int r, g, b;
						DWORD col;
						double blinky;
						if (i == CurrentItem && x1 == SelColorIndex)
						{
							r = 255, g = 128, b = 0;
						}
						else
						{
							r = 200, g = 200, b = 255;
						}
						// Make sure the cursors stand out against similar colors
						// by pulsing them.
						blinky = fabs(sin(I_MSTime()/1000.0)) * 0.5 + 0.5;
						col = MAKEARGB(255,int(r*blinky),int(g*blinky),int(b*blinky));

						screen->Clear (box_x, box_y, box_x + w, box_y + 1, -1, col);
						screen->Clear (box_x, box_y + h-1, box_x + w, box_y + h, -1, col);
						screen->Clear (box_x, box_y, box_x + 1, box_y + h, -1, col);
						screen->Clear (box_x + w - 1, box_y, box_x + w, box_y + h, -1, col);
					}
					box_x += w;
				}
			}
			break;

			case bitflag:
			{
				value_t *value;
				const char *str;

				if (item->b.min)
					value = NoYes;
				else
					value = YesNo;

				if (item->a.cvar)
				{
					if ((*(item->a.intcvar)) & item->e.flagmask)
						str = value[1].name;
					else
						str = value[0].name;
				}
				else
				{
					str = "???";
				}

				screen->DrawText (SmallFont, ValueColor,
					x, y, str, DTA_Clean, true, TAG_DONE);
			}
			break;

			case string:

				// If we're currently inputting a string,
				// draw the temporary string and the cursor.
				if ( g_bStringInput && i == CurrentItem )
				{
					screen->DrawText( SmallFont, CR_GREY, x, y, g_szStringInputBuffer, DTA_Clean, true, TAG_DONE );

					// Draw the cursor.
					screen->DrawText( SmallFont, CR_GREY,
						x + SmallFont->StringWidth( g_szStringInputBuffer ),
						y,
						gameinfo.gametype == GAME_Doom ? "_" : "[", DTA_Clean, true, TAG_DONE );
				}
				else
					screen->DrawText( SmallFont, CR_GREY, x, y, *item->a.stringcvar, DTA_Clean, true, TAG_DONE );
				break;
			case pwstring:

				{
					ULONG	ulIdx;
					char	szPWString[64];

					// If we're currently inputting a string,
					// draw the temporary string and the cursor.
					if ( g_bStringInput && i == CurrentItem )
					{
						sprintf( szPWString, "%s", g_szStringInputBuffer );
						for ( ulIdx = 0; ulIdx < strlen( szPWString ); ulIdx++ )
							szPWString[ulIdx] = '*';

						screen->DrawText( SmallFont, CR_GREY, x, y, szPWString, DTA_Clean, true, TAG_DONE );

						// Draw the cursor.
						screen->DrawText( SmallFont, CR_GREY,
							x + SmallFont->StringWidth( szPWString ),
							y,
							gameinfo.gametype == GAME_Doom ? "_" : "[", DTA_Clean, true, TAG_DONE );
					}
					else
					{
						UCVarValue	Val;

						Val = item->a.stringcvar->GetGenericRep( CVAR_String );
						sprintf( szPWString, "%s", Val.String );
						for ( ulIdx = 0; ulIdx < strlen( szPWString ); ulIdx++ )
							szPWString[ulIdx] = '*';

						screen->DrawText( SmallFont, CR_GREY, x, y, szPWString, DTA_Clean, true, TAG_DONE );
					}
				}
				break;
			case number:
				{
					char	szString[16];
				
					sprintf( szString, "%d", item->a.cvar->GetGenericRep( CVAR_Int ).Int);
					screen->DrawText( SmallFont, CR_GREY, x, y, szString, DTA_Clean, true, TAG_DONE );
				}
				break;
			case skintype:

				screen->DrawText( SmallFont, CR_GREY, x, y, skins[g_ulPlayerSetupSkin].name, DTA_Clean, true, TAG_DONE );
				break;
			case classtype:

				screen->DrawText( SmallFont, CR_GREY, x, y, ( g_lPlayerSetupClass == -1 ) ? "random" : PlayerClasses[g_lPlayerSetupClass].Type->Meta.GetMetaString (APMETA_DisplayName), DTA_Clean, true, TAG_DONE );
				break;
			case botslot:

				{
					FBaseCVar	*pVar;
					UCVarValue	Val;
					char		szCVarName[32];
					char		szBuffer[256];

					if ( GAMEMODE_GetFlags( static_cast<GAMEMODE_e>(menu_gamemode.GetGenericRep( CVAR_Int ).Int) ) & GMF_PLAYERSONTEAMS )
					{
						// [CW] Add to this when bumping 'MAX_TEAMS'.
						switch( i )
						{
						case TEAM1_STARTMENUPOS:
						case TEAM1_STARTMENUPOS + 1:
						case TEAM1_STARTMENUPOS + 2:
						case TEAM1_STARTMENUPOS + 3:
						case TEAM1_STARTMENUPOS + 4:
							sprintf( szCVarName, "menu_teambotspawn%d", i - TEAM1_STARTMENUPOS );
							break;

						case TEAM2_STARTMENUPOS:
						case TEAM2_STARTMENUPOS + 1:
						case TEAM2_STARTMENUPOS + 2:
						case TEAM2_STARTMENUPOS + 3:
						case TEAM2_STARTMENUPOS + 4:
							sprintf( szCVarName, "menu_teambotspawn%d", i - TEAM2_STARTMENUPOS + 5 );
							break;

						case TEAM3_STARTMENUPOS:
						case TEAM3_STARTMENUPOS + 1:
						case TEAM3_STARTMENUPOS + 2:
						case TEAM3_STARTMENUPOS + 3:
						case TEAM3_STARTMENUPOS + 4:
							sprintf( szCVarName, "menu_teambotspawn%d", i - TEAM3_STARTMENUPOS + 2*5 );
							break;

						case TEAM4_STARTMENUPOS:
						case TEAM4_STARTMENUPOS + 1:
						case TEAM4_STARTMENUPOS + 2:
						case TEAM4_STARTMENUPOS + 3:
						case TEAM4_STARTMENUPOS + 4:
							sprintf( szCVarName, "menu_teambotspawn%d", i - TEAM4_STARTMENUPOS + 3*5 );
							break;

						default:
							break;
						}
					}
					else
						sprintf( szCVarName, "menu_botspawn%d", i );

					pVar = FindCVar( szCVarName, NULL );

					Val = pVar->GetGenericRep( CVAR_Int );
					if ( BOTINFO_GetName( Val.Int ) == NULL )
						screen->DrawText( SmallFont, CR_GREY, x, y, "-", DTA_Clean, true, TAG_DONE );
					else
					{
						sprintf( szBuffer, "%s", BOTINFO_GetName( Val.Int ));
						V_ColorizeString( szBuffer );
						screen->DrawText( SmallFont, CR_GREY, x, y, szBuffer, DTA_Clean, true, TAG_DONE );
					}
				}
				break;
			case announcer:

				if ( ANNOUNCER_GetName( *item->a.intcvar ) == NULL )
					screen->DrawText( SmallFont, CR_GREY, x, y, "NONE", DTA_Clean, true, TAG_DONE );
				else
					screen->DrawText( SmallFont, CR_GREY, x, y, ANNOUNCER_GetName( *item->a.intcvar ), DTA_Clean, true, TAG_DONE );
				break;
			case levelslot:
				{
					char	szMapName[64];

					if ( wadlevelinfos.Size( ) > 0 )
					{
						if ( *item->a.intcvar >= static_cast<signed> (wadlevelinfos.Size( )))
							*item->a.intcvar = 0;

						level_info_t *info = &wadlevelinfos[*item->a.intcvar];
						sprintf( szMapName, "%s - %s", wadlevelinfos[*item->a.intcvar].mapname, info ? info->LookupLevelName().GetChars() : "" );

						if ( stricmp( szMapName, "(null)" ) == 0 )
							screen->DrawText( SmallFont, CR_GREY, x, y, "UNKNOWN LEVEL", DTA_Clean, true, TAG_DONE );
						else
							screen->DrawText( SmallFont, CR_GREY, x, y, szMapName, DTA_Clean, true, TAG_DONE );
					}
				}
				break;

			case txslider:

				value = item->a.cvar->GetGenericRep (CVAR_Float);
				M_DrawSlider (x, y + labelofs, item->b.min, item->c.max, value.Float);
				break;
			case mnnumber:

				// If we're currently inputting a string,
				// draw the temporary string and the cursor.
				if ( g_bStringInput && i == CurrentItem )
				{
					screen->DrawText( SmallFont, CR_GREY, x, y, g_szStringInputBuffer, DTA_Clean, true, TAG_DONE );

					// Draw the cursor.
					screen->DrawText( SmallFont, CR_GREY,
						x + SmallFont->StringWidth( g_szStringInputBuffer ),
						y,
						gameinfo.gametype == GAME_Doom ? "_" : "[", DTA_Clean, true, TAG_DONE );
				}
				else
				{
					char	szString[16];
				
					sprintf( szString, "%d", item->a.cvar->GetGenericRep( CVAR_Int ).Int);
					screen->DrawText( SmallFont, CR_GREY, x, y, szString, DTA_Clean, true, TAG_DONE );
				}
				break;
			default:
				break;
			}

			if (item->type != palettegrid &&	// Palette grids draw their own cursor
				i == CurrentItem &&
				(skullAnimCounter < 6 || menuactive == MENU_WaitKey))
			{
				if ( CurrentMenu->iFlags & MNF_CENTERED )
					M_DrawConText(CR_RED, 160 - ( SmallFont->StringWidth( item->label ) / 2 ) - 11, y-1+labelofs, "\xd" );
				else if ( CurrentMenu->iFlags & MNF_ALIGNLEFT )
					M_DrawConText(CR_RED, 32 - /*p->width*/8 - 3, y-1+labelofs, "\xd" );
				else
					M_DrawConText(CR_RED, indent + 3, y-1+labelofs, "\xd");
			}
		}
		else if ( item->type == screenres )
		{
			char *str = NULL;

			for (x = 0; x < 3; x++)
			{
				switch (x)
				{
				case 0: str = item->b.res1; break;
				case 1: str = item->c.res2; break;
				case 2: str = item->d.res3; break;
				}
				if (str)
				{
					if (x == item->e.highlight)
 						color = CR_GOLD;	//ValueColor;
					else
						color = CR_BRICK;	//LabelColor;

					screen->DrawText (SmallFont, color, 104 * x + 20, y, str, DTA_Clean, true, TAG_DONE);
				}
			}

			if (i == CurrentItem && ((item->a.selmode != -1 && (skullAnimCounter < 6 || menuactive == MENU_WaitKey)) || testingmode))
			{
				M_DrawConText(CR_RED, item->a.selmode * 104 + 8, y-1 + labelofs, "\xd");
			}
		}
		else if ( item->type == browserslot )
		{
			LONG	lServer;
			char	szString[32];
			LONG	lColor;

			lServer = item->f.lServer;

			if ( M_ShouldShowServer( lServer ))
			{
				if ( lServer == g_lSelectedServer )
					lColor = CR_ORANGE;
				else
					lColor = CR_GRAY;

				// Draw ping.
				sprintf( szString, "%d", static_cast<int> (BROWSER_GetPing( lServer )));
				screen->DrawText( SmallFont, lColor, 16, y, szString, DTA_Clean, true, TAG_DONE );

				// Draw name.
				strncpy( szString, BROWSER_GetHostName( lServer ), 12 );
				szString[12] = 0;
				if ( strlen( BROWSER_GetHostName( lServer )) > 12 )
					sprintf( szString + strlen ( szString ), "..." );
				screen->DrawText( SmallFont, lColor, 48, y, szString, DTA_Clean, true, TAG_DONE );

				// Draw map.
				strncpy( szString, BROWSER_GetMapname( lServer ), 8 );
				screen->DrawText( SmallFont, lColor, /*128*/160, y, szString, DTA_Clean, true, TAG_DONE );
/*
				// Draw wad.
				if ( BROWSER_Get
				sprintf( szString, "%d", BROWSER_GetPing( lServer ));
				screen->DrawText( SmallFont, CR_GRAY, 160, y, "WAD", DTA_Clean, true, TAG_DONE );
*/
				// Draw gametype.
				strncpy( szString, GAMEMODE_GetShortName( BROWSER_GetGameMode( lServer )), 8 );
				screen->DrawText( SmallFont, lColor, 224, y, szString, DTA_Clean, true, TAG_DONE );

				// Draw players.
				sprintf( szString, "%d/%d", static_cast<int> (BROWSER_GetNumPlayers( lServer )), static_cast<int> (BROWSER_GetMaxClients( lServer )));
				screen->DrawText( SmallFont, lColor, 272, y, szString, DTA_Clean, true, TAG_DONE );

				// Draw the cursor.
				if (( i == CurrentItem ) && ( skullAnimCounter < 6 ))
					screen->DrawText( ConFont, CR_RED, 6, y-1 + labelofs, "\xd", DTA_Clean, true, TAG_DONE );

				// If the server in the highest browser slot is not the first server in the sorted server list, draw
				// an up scroll arrow. (If this happens, we must have scrolled down.)
				if (( i == SERVER_SLOT_START ) && ( lServer != g_iSortedServers[0] ) && ( BROWSER_CalcNumServers( )))
					screen->DrawText( ConFont, CR_ORANGE, 300, y-1 + labelofs - 4, "\x1a", DTA_Clean, true, TAG_DONE );

				// If the server in the highest browser slot is not the last server in the sorted server list, draw
				// a down scroll arrow.
				if (( i == ( SERVER_SLOT_START + NUM_SERVER_SLOTS - 1 )) && ( lServer != g_iSortedServers[M_CalcLastSortedIndex( ) - 1] ) && ( BROWSER_CalcNumServers( )))
					screen->DrawText( ConFont, CR_ORANGE, 300, y-1 + labelofs + 4, "\x1b", DTA_Clean, true, TAG_DONE );
			}
		}
	}

	CanScrollUp = (CurrentMenu->scrollpos > 0);
	CanScrollDown = (i < CurrentMenu->numitems);
	VisBottom = i - 1;

	if (CanScrollUp)
	{
		M_DrawConText(CR_ORANGE, 3, ytop + labelofs, "\x1a");
	}
	if (CanScrollDown)
	{
		M_DrawConText(CR_ORANGE, 3, y - 8 + labelofs, "\x1b");
	}

	if (flagsvar)
	{
		static const FIntCVar *const vars[5] = { &dmflags, &dmflags2, &compatflags, &menu_dmflags, &menu_dmflags2 };
		char flagsblah[256];
		char *fillptr = flagsblah;
		bool printed = false;

		for (int i = 0; i < 5; ++i)
		{
			if (flagsvar & (1 << i))
			{
				if (printed)
				{
					fillptr += mysnprintf (fillptr, countof(flagsblah) - (fillptr - flagsblah), "    ");
				}
				printed = true;

				// O NOES, HAX! :((((((((((
				if ( i == 3 )
					fillptr += mysnprintf (fillptr, countof(flagsblah) - (fillptr - flagsblah), "dmflags = %d", **vars[i]);
				else if ( i == 4 )
					fillptr += mysnprintf (fillptr, countof(flagsblah) - (fillptr - flagsblah), "dmflags2 = %d", **vars[i]);
				else
					fillptr += mysnprintf (fillptr, countof(flagsblah) - (fillptr - flagsblah), "%s = %d", vars[i]->GetName (), **vars[i]);
			}
		}
		screen->DrawText (SmallFont, ValueColor,
			160 - (SmallFont->StringWidth (flagsblah) >> 1), 0, flagsblah,
			DTA_Clean, true, TAG_DONE);
	}
}

//bool IncreaseWeaponPrefPosition( char *pszWeaponName, bool bDisplayMessage );
//bool DecreaseWeaponPrefPosition( char *pszWeaponName, bool bDisplayMessage );
void M_OptResponder (event_t *ev)
{
	menuitem_t *item;
	int ch = tolower (ev->data1);
	UCVarValue value;

	item = CurrentMenu->items + CurrentItem;

	if (menuactive == MENU_WaitKey && ev->type == EV_KeyDown)
	{
		if (ev->data1 != KEY_ESCAPE)
		{
			C_ChangeBinding (item->e.command, ev->data1);
			M_BuildKeyList (CurrentMenu->items, CurrentMenu->numitems);
		}
		menuactive = MENU_On;
		CurrentMenu->items[0].label = OldMessage;
		CurrentMenu->items[0].type = OldType;
		return;
	}

	// User is currently inputting a string. Handle input for that here.
	if ( g_bStringInput )
	{
		ULONG	ulLength;

		ulLength = (ULONG)strlen( g_szStringInputBuffer );

		if ( ev->subtype == EV_GUI_KeyDown || ev->subtype == EV_GUI_KeyRepeat )
		{
			if ( ch == '\r' )
			{
				UCVarValue	Val;

				V_ColorizeString( g_szStringInputBuffer );

				if ( item->type == mnnumber )
				{
					Val.Int = atoi( g_szStringInputBuffer );
					item->a.intcvar->ForceSet( Val, CVAR_Int );
				}
				else
				{
					Val.String = g_szStringInputBuffer;
					item->a.stringcvar->ForceSet( Val, CVAR_String );
				}

				g_bStringInput = false;
			}
			else if ( ch == GK_ESCAPE )
				g_bStringInput = false;
			else if ( ch == '\b' )
			{
				if ( ulLength )
					g_szStringInputBuffer[ulLength - 1] = '\0';
			}
			// Ctrl+C. 
			else if ( ev->data1 == 'C' && ( ev->data3 & GKM_CTRL ))
			{
				I_PutInClipboard((char *)g_szStringInputBuffer );
			}
			// Ctrl+V.
			else if ( ev->data1 == 'V' && ( ev->data3 & GKM_CTRL ))
			{
				FString clipString = I_GetFromClipboard (false);
				if (clipString.IsNotEmpty())
				{
					const char *clip = clipString.GetChars();
					// Only paste the first line.
					while (*clip != '\0')
					{
						if (*clip == '\n' || *clip == '\r' || *clip == '\b')
						{
							break;
						}
						ulLength = (ULONG)strlen( g_szStringInputBuffer );
						if ( ulLength < ( 64 - 2 ))
						{
							g_szStringInputBuffer[ulLength] = *clip++;
							g_szStringInputBuffer[ulLength + 1] = '\0';
						}
						// [BB] We still need to increment the pointer, otherwise the while loop never ends.
						else
							*clip++;
					}
				}
			}
		}
		else if ( ev->subtype == EV_GUI_Char )
		{
			if ( ulLength  < ( 64 - 2 ))
			{
				// Don't allow input of characters when entering a manual number.
				if (( item->type != mnnumber ) || ( ev->data1 >= '0' && ev->data1 <= '9' ))
				{
					g_szStringInputBuffer[ulLength] = ev->data1;
					g_szStringInputBuffer[ulLength + 1] = '\0';
				}
			}
		}
		return;
	}

	if (ev->subtype == EV_GUI_KeyRepeat)
	{
		if (ch != GK_LEFT && ch != GK_RIGHT && ch != GK_UP && ch != GK_DOWN)
		{
			return;
		}
	}
	else if (ev->subtype != EV_GUI_KeyDown)
	{
		return;
	}
	
	if (item->type == bitflag &&
		(ch == GK_LEFT || ch == GK_RIGHT || ch == '\r')
		&& (!demoplayback || ( CLIENTDEMO_IsPlaying( ) == false ) || CurrentMenu == &SkirmishDMFlagsMenu))	// [BC] Allow people to toggle dmflags at the skirmish dmflags menu.
	{
		*(item->a.intcvar) = (*(item->a.intcvar)) ^ item->e.flagmask;
		return;
	}

	switch (ch)
	{
	case GK_DOWN:
		if (CurrentMenu->numitems > 1)
		{
			int modecol;
			bool	bMoveCursor = true;

			if (item->type == screenres)
			{
				modecol = item->a.selmode;
				item->a.selmode = -1;
			}
			else
			{
				modecol = 0;
			}

			if (( CurrentMenu->items[CurrentItem].type == browserslot ) &&
				( CurrentItem == SERVER_SLOT_START + NUM_SERVER_SLOTS - 1 ) &&
				( CurrentMenu->items[CurrentItem].f.lServer != g_iSortedServers[M_CalcLastSortedIndex( ) - 1] ))
			{
				M_ScrollServerList( false );
				bMoveCursor = false;
			}

			if ( bMoveCursor )
			{
				do
				{
					CurrentItem++;
					if (CanScrollDown && CurrentItem == VisBottom)
					{
						CurrentMenu->scrollpos++;
						VisBottom++;
					}
					if (CurrentItem == CurrentMenu->numitems)
					{
						CurrentMenu->scrollpos = 0;
						CurrentItem = 0;
					}
				} while (CurrentMenu->items[CurrentItem].type == redtext ||
						 CurrentMenu->items[CurrentItem].type == whitetext ||
						 CurrentMenu->items[CurrentItem].type == browserheader ||
						 (( CurrentMenu->items[CurrentItem].type == weaponslot ) && ( strlen( CurrentMenu->items[CurrentItem].label ) == 0 )) ||
						 (CurrentMenu->items[CurrentItem].type == browserslot &&
						  M_ShouldShowServer( CurrentMenu->items[CurrentItem].f.lServer ) == false) ||
						 (CurrentMenu->items[CurrentItem].type == screenres &&
						  !CurrentMenu->items[CurrentItem].b.res1) ||
						 (CurrentMenu->items[CurrentItem].type == numberedmore &&
						  !CurrentMenu->items[CurrentItem].b.position));
			}

			if (CurrentMenu->items[CurrentItem].type == screenres)
			{
				item = &CurrentMenu->items[CurrentItem];
				while ((modecol == 2 && !item->d.res3) || (modecol == 1 && !item->c.res2))
				{
					modecol--;
				}
				CurrentMenu->items[CurrentItem].a.selmode = modecol;
			}

			S_Sound (CHAN_VOICE | CHAN_UI, "menu/cursor", 1, ATTN_NONE);
		}
		break;

	case GK_UP:
		if (CurrentMenu->numitems > 1)
		{
			int modecol;
			bool	bMoveCursor = true;

			if (item->type == screenres)
			{
				modecol = item->a.selmode;
				item->a.selmode = -1;
			}
			else
			{
				modecol = 0;
			}

			if (( CurrentMenu->items[CurrentItem].type == browserslot ) &&
				( CurrentItem == SERVER_SLOT_START ) &&
				( CurrentMenu->items[CurrentItem].f.lServer != g_iSortedServers[0] ))
			{
				M_ScrollServerList( true );
				bMoveCursor = false;
			}

			if ( bMoveCursor )
			{
				do
				{
					CurrentItem--;
					if (CurrentMenu->scrollpos > 0 &&
						CurrentItem == CurrentMenu->scrolltop + CurrentMenu->scrollpos)
					{
						CurrentMenu->scrollpos--;
					}
					if (CurrentItem < 0)
					{
						int maxitems, rowheight;

						// Figure out how many lines of text fit on the menu
						if (BigFont && CurrentMenu->texttitle)
						{
							maxitems = 15 + BigFont->GetHeight ();
						}
						else
						{
							maxitems = 15;
						}
						if (!(gameinfo.gametype & GAME_DoomChex))
						{
							maxitems -= 2;
							rowheight = 9;
						}
						else
						{
							rowheight = 8;
						}
						maxitems = (200 - SmallFont->GetHeight () - maxitems) / rowheight + 1;

						CurrentMenu->scrollpos = MAX (0,CurrentMenu->numitems - maxitems + CurrentMenu->scrolltop);
						CurrentItem = CurrentMenu->numitems - 1;
					}
				} while (CurrentMenu->items[CurrentItem].type == redtext ||
						 CurrentMenu->items[CurrentItem].type == whitetext ||
						 CurrentMenu->items[CurrentItem].type == browserheader ||
						 (( CurrentMenu->items[CurrentItem].type == weaponslot ) && ( strlen( CurrentMenu->items[CurrentItem].label ) == 0 )) ||
						 (CurrentMenu->items[CurrentItem].type == browserslot &&
						  M_ShouldShowServer( CurrentMenu->items[CurrentItem].f.lServer ) == false) ||
						 (CurrentMenu->items[CurrentItem].type == screenres &&
						  !CurrentMenu->items[CurrentItem].b.res1) ||
						 (CurrentMenu->items[CurrentItem].type == numberedmore &&
						  !CurrentMenu->items[CurrentItem].b.position));
			}

			if (CurrentMenu->items[CurrentItem].type == screenres)
				CurrentMenu->items[CurrentItem].a.selmode = modecol;

			S_Sound (CHAN_VOICE | CHAN_UI, "menu/cursor", 1, ATTN_NONE);
		}
		break;

	case GK_PGUP:
		if (CurrentMenu->scrollpos > 0)
		{
			CurrentMenu->scrollpos -= VisBottom - CurrentMenu->scrollpos - CurrentMenu->scrolltop;
			if (CurrentMenu->scrollpos < 0)
			{
				CurrentMenu->scrollpos = 0;
			}
			CurrentItem = CurrentMenu->scrolltop + CurrentMenu->scrollpos + 1;
			while (CurrentMenu->items[CurrentItem].type == redtext ||
				   CurrentMenu->items[CurrentItem].type == whitetext ||
				   CurrentMenu->items[CurrentItem].type == browserheader ||
				   (CurrentMenu->items[CurrentItem].type == screenres &&
					!CurrentMenu->items[CurrentItem].b.res1) ||
				   (CurrentMenu->items[CurrentItem].type == numberedmore &&
					!CurrentMenu->items[CurrentItem].b.position))
			{
				++CurrentItem;
			}
			S_Sound (CHAN_VOICE | CHAN_UI, "menu/cursor", 1, ATTN_NONE);
		}
		break;

	case GK_PGDN:
		if (CanScrollDown)
		{
			int pagesize = VisBottom - CurrentMenu->scrollpos - CurrentMenu->scrolltop;
			CurrentMenu->scrollpos += pagesize;
			if (CurrentMenu->scrollpos + CurrentMenu->scrolltop + pagesize > CurrentMenu->numitems)
			{
				CurrentMenu->scrollpos = CurrentMenu->numitems - CurrentMenu->scrolltop - pagesize;
			}
			CurrentItem = CurrentMenu->scrolltop + CurrentMenu->scrollpos + 1;
			while (CurrentMenu->items[CurrentItem].type == redtext ||
				   CurrentMenu->items[CurrentItem].type == whitetext ||
				   CurrentMenu->items[CurrentItem].type == browserheader ||
				   (CurrentMenu->items[CurrentItem].type == screenres &&
					!CurrentMenu->items[CurrentItem].b.res1) ||
				   (CurrentMenu->items[CurrentItem].type == numberedmore &&
					!CurrentMenu->items[CurrentItem].b.position))
			{
				++CurrentItem;
			}
			S_Sound (CHAN_VOICE | CHAN_UI, "menu/cursor", 1, ATTN_NONE);
		}
		break;

	case GK_LEFT:
		switch (item->type)
		{
			case slider:
			case absslider:
			case intslider:
				{
					// [BB] Awful hack for the player color handling. Was already here when I joined ST.
					if (!stricmp (item->label, "Red") && (item->type == slider))
					{
						int cr = g_ulPlayerSetupColor;
						int red = RPART(cr);

						red -= 16;
						if (red < 0)
							red = 0;
						SendNewColor (red, GPART(cr), BPART(cr));

						g_bSwitchColorBack = false;
					}
					else if (!stricmp (item->label, "Green") && (item->type == slider))
					{
						int cr = g_ulPlayerSetupColor;
						int green = GPART(cr);

						green -= 16;
						if (green < 0)
							green = 0;
						SendNewColor (RPART(cr), green, BPART(cr));

						g_bSwitchColorBack = false;
					}
					else if (!stricmp (item->label, "Blue") && (item->type == slider))
					{
						int cr = g_ulPlayerSetupColor;
						int blue = BPART(cr);

						blue -= 16;
						if (blue < 0)
							blue = 0;
						SendNewColor (RPART(cr), GPART(cr), blue);

						g_bSwitchColorBack = false;
					}
					else
					{
						UCVarValue newval;
						bool reversed;

						if (item->type == intslider)
							value.Float = item->a.fval;
						else
							value = item->a.cvar->GetGenericRep (CVAR_Float);
						reversed = item->type == absslider && value.Float < 0.f;
						newval.Float = (reversed ? -value.Float : value.Float) - item->d.step;

						if (newval.Float < item->b.min)
							newval.Float = item->b.min;
						else if (newval.Float > item->c.max)
							newval.Float = item->c.max;

						if (reversed)
						{
							newval.Float = -newval.Float;
						}

						if (item->type == intslider)
							item->a.fval = newval.Float;
						else if (item->e.cfunc)
							item->e.cfunc (item->a.cvar, newval.Float);
						else
							item->a.cvar->SetGenericRep (newval, CVAR_Float);
					}
				}
				S_Sound (CHAN_VOICE | CHAN_UI, "menu/change", 1, ATTN_NONE);
				break;

			case palettegrid:
				SelColorIndex = (SelColorIndex - 1) & 15;
				S_Sound (CHAN_VOICE | CHAN_UI, "menu/cursor", 1, ATTN_NONE);
				break;

			case discretes:
			case discrete:
			case cdiscrete:
				{
					// Hack for autoaim.
					if ( strcmp( item->label, "Autoaim" ) == 0 )
					{
						float ranges[] = { 0, 0.25, 0.5, 1, 2, 3, 5000 };
						float aim = menu_autoaim;
						int i;

						// Select a lower autoaim
						for (i = 6; i >= 1; i--)
						{
							if (aim >= ranges[i])
							{
								aim = ranges[i - 1];
								break;
							}
						}
	
						menu_autoaim = aim;
					}
					else
					{
						int cur;
						int numvals;

						numvals = (int)item->b.min;
						value = item->a.cvar->GetGenericRep (CVAR_Float);
					if (item->type != discretes)
					{
						cur = M_FindCurVal (value.Float, item->e.values, numvals);
					}
					else
					{
						cur = M_FindCurVal (value.Float, item->e.valuestrings, numvals);
					}
						if (--cur < 0)
							cur = numvals - 1;

						value.Float = item->type != discretes ? item->e.values[cur].value : item->e.valuestrings[cur].value;
						item->a.cvar->SetGenericRep (value, CVAR_Float);

						// Hack hack. Rebuild list of resolutions
						if (item->e.values == Depths)
							BuildModesList (SCREENWIDTH, SCREENHEIGHT, DisplayBits);

						// Hack for the browser menu. If we changed a setting, rebuild the list.
						if ( CurrentMenu == &BrowserMenu )
							M_BuildServerList( );
					}
				}
				S_Sound (CHAN_VOICE | CHAN_UI, "menu/change", 1, ATTN_NONE);
				break;
			case ediscrete:
				value = item->a.cvar->GetGenericRep(CVAR_String);
				value.String = const_cast<char *>(M_FindPrevVal(value.String, item->e.enumvalues, (int)item->b.numvalues));
				item->a.cvar->SetGenericRep(value, CVAR_String);
				S_Sound (CHAN_VOICE | CHAN_UI, "menu/change", 1, ATTN_NONE);
				break;

			case bitmask:
				{
					int cur;
					int numvals;
					int bmask = int(item->c.max);

					numvals = (int)item->b.min;
					value = item->a.cvar->GetGenericRep (CVAR_Int);
					
					cur = M_FindCurVal (value.Int & bmask, item->e.values, numvals);
					if (--cur < 0)
						cur = numvals - 1;

					value.Int = (value.Int & ~bmask) | int(item->e.values[cur].value);
					item->a.cvar->SetGenericRep (value, CVAR_Int);
				}
				S_Sound (CHAN_VOICE | CHAN_UI, "menu/change", 1, ATTN_NONE);
				break;
			case discrete_guid:
				{
					int cur;
					int numvals;

					numvals = (int)item->b.numvalues;
					cur = M_FindCurGUID (*(item->a.guidcvar), item->e.guidvalues, numvals);
					if (--cur < 0)
						cur = numvals - 1;

					*(item->a.guidcvar) = item->e.guidvalues[cur].ID;
				}
				S_Sound (CHAN_VOICE | CHAN_UI, "menu/change", 1, ATTN_NONE);
				break;

			case inverter:
				value = item->a.cvar->GetGenericRep (CVAR_Float);
				value.Float = -value.Float;
				item->a.cvar->SetGenericRep (value, CVAR_Float);
				S_Sound (CHAN_VOICE | CHAN_UI, "menu/change", 1, ATTN_NONE);
				break;

			case screenres:
				{
					int col;

					col = item->a.selmode - 1;
					if (col < 0)
					{
						if (CurrentItem > 0)
						{
							if (CurrentMenu->items[CurrentItem - 1].type == screenres)
							{
								item->a.selmode = -1;
								CurrentMenu->items[--CurrentItem].a.selmode = 2;
							}
						}
					}
					else
					{
						item->a.selmode = col;
					}
				}
				S_Sound (CHAN_VOICE | CHAN_UI, "menu/cursor", 1, ATTN_NONE);
				break;

			case number:

				{
					value = item->a.cvar->GetGenericRep( CVAR_Int );
					
					value.Int -= item->d.step;
					if ( value.Int < item->b.min )
						value.Int = item->c.max;

					item->a.cvar->SetGenericRep( value, CVAR_Int );
					S_Sound( CHAN_VOICE | CHAN_UI, "menu/change", 1, ATTN_NONE );
				}
				break;
			case skintype:

				if ((( GetDefaultByType( PlayerClass->Type )->flags4 & MF4_NOSKIN ) == false ) &&
					( g_lPlayerSetupClass != -1 ))
				{
					LONG	lSkin = g_ulPlayerSetupSkin;

					// Don't allow hidden skins to be selectable.
					do
					{
						lSkin--;
						if ( lSkin < 0 )
							lSkin = (int)skins.Size() - 1;

					} while (( skins[lSkin].bRevealed == false ) || ( PlayerClass->CheckSkin( lSkin ) == false ));

					g_ulPlayerSetupSkin = lSkin;
					cvar_set( "menu_skin", skins[lSkin].name );

					if ( skins[lSkin].szColor[0] != 0 )
					{
						if ( g_bSwitchColorBack == false )
						{
							UCVarValue	Val;

							Val = menu_color.GetGenericRep( CVAR_Int );
							g_lSavedColor = Val.Int;

							g_bSwitchColorBack = true;
						}
						
						cvar_set( "menu_color", skins[lSkin].szColor );
						g_ulPlayerSetupColor = V_GetColorFromString( NULL, skins[lSkin].szColor );
					}
					else if ( g_bSwitchColorBack )
					{
						UCVarValue	Val;

						Val.Int = g_lSavedColor;
						menu_color.SetGenericRep( Val, CVAR_Int );
						g_ulPlayerSetupColor = g_lSavedColor;

						g_bSwitchColorBack = false;
					}
				}
				S_Sound( CHAN_VOICE | CHAN_UI, "menu/change", 1, ATTN_NONE );
				break;
			case classtype:

				if ( PlayerClasses.Size( ) > 0 )
				{
					LONG	lClass = g_lPlayerSetupClass;

					lClass--;
					if ( lClass < -1 )
						lClass = PlayerClasses.Size() - 1;

					cvar_set( "menu_playerclass", ( lClass == -1 ) ? "random" : PlayerClasses[lClass].Type->Meta.GetMetaString (APMETA_DisplayName));

					g_lPlayerSetupClass = lClass;
				}
				S_Sound( CHAN_VOICE | CHAN_UI, "menu/change", 1, ATTN_NONE );
				break;
			case botslot:
				{
					LONG		lBotIdx = -1;
					FBaseCVar	*pVar;
					UCVarValue	Val;
					char		szCVarName[32];

					if ( GAMEMODE_GetFlags( static_cast<GAMEMODE_e>(menu_gamemode.GetGenericRep( CVAR_Int ).Int) ) & GMF_PLAYERSONTEAMS )
					{
						// [CW] Add to this when bumping 'MAX_TEAMS'.
						switch( CurrentItem )
						{
						case TEAM1_STARTMENUPOS:
						case TEAM1_STARTMENUPOS + 1:
						case TEAM1_STARTMENUPOS + 2:
						case TEAM1_STARTMENUPOS + 3:
						case TEAM1_STARTMENUPOS + 4:
							sprintf( szCVarName, "menu_teambotspawn%d", CurrentItem - TEAM1_STARTMENUPOS );
							break;

						case TEAM2_STARTMENUPOS:
						case TEAM2_STARTMENUPOS + 1:
						case TEAM2_STARTMENUPOS + 2:
						case TEAM2_STARTMENUPOS + 3:
						case TEAM2_STARTMENUPOS + 4:
							sprintf( szCVarName, "menu_teambotspawn%d", CurrentItem - TEAM2_STARTMENUPOS + 5 );
							break;

						case TEAM3_STARTMENUPOS:
						case TEAM3_STARTMENUPOS + 1:
						case TEAM3_STARTMENUPOS + 2:
						case TEAM3_STARTMENUPOS + 3:
						case TEAM3_STARTMENUPOS + 4:
							sprintf( szCVarName, "menu_teambotspawn%d", CurrentItem - TEAM3_STARTMENUPOS + 2*5 );
							break;

						case TEAM4_STARTMENUPOS:
						case TEAM4_STARTMENUPOS + 1:
						case TEAM4_STARTMENUPOS + 2:
						case TEAM4_STARTMENUPOS + 3:
						case TEAM4_STARTMENUPOS + 4:
							sprintf( szCVarName, "menu_teambotspawn%d", CurrentItem - TEAM4_STARTMENUPOS + 3*5 );
							break;

						default:
							break;
						}
					}
					else
						sprintf( szCVarName, "menu_botspawn%d", CurrentItem );

					pVar = FindCVar( szCVarName, NULL );

					Val = pVar->GetGenericRep( CVAR_Int );

					// Decrement the bot index. Skip over non-revealed bots.
					lBotIdx = Val.Int;
					do
					{
						lBotIdx--;

						if ( lBotIdx < -1 )
							lBotIdx = MAX_BOTINFO - 1;

					} while ( lBotIdx >= 0 && BOTINFO_GetRevealed( lBotIdx ) == false );

					// Finally, set the index in the botspawn slot.
					Val.Int = lBotIdx;
					pVar->SetGenericRep( Val, CVAR_Int );
				}
				S_Sound( CHAN_VOICE | CHAN_UI, "menu/change", 1, ATTN_NONE );
				break;
			case announcer:
				{
					LONG	lAnnouncerIdx;
					
					lAnnouncerIdx = *item->a.intcvar;

					lAnnouncerIdx--;
					if ( lAnnouncerIdx < -1 )
						lAnnouncerIdx = ANNOUNCER_GetNumProfiles( ) - 1;

					*(item->a.intcvar) = lAnnouncerIdx;
				}
				S_Sound( CHAN_VOICE | CHAN_UI, "menu/change", 1, ATTN_NONE );
				break;
			case levelslot:
				{
					SHORT	sLevelIdx;
					
					sLevelIdx = *item->a.intcvar;

					sLevelIdx--;
					if ( sLevelIdx < 0 )
						sLevelIdx = wadlevelinfos.Size( ) - 1;

					*(item->a.intcvar) = sLevelIdx;
				}
				S_Sound( CHAN_VOICE | CHAN_UI, "menu/change", 1, ATTN_NONE );
				break;

			case txslider:
				{
					UCVarValue	newval;
					int			iMode;
					int			iWidth;
					int			iHeight;
					bool		bLetterBox;

					value = item->a.cvar->GetGenericRep (CVAR_Float);
					newval.Float = value.Float - item->d.step;

					if (newval.Float < item->b.min)
						newval.Float = item->b.min;
					else if (newval.Float > item->c.max)
						newval.Float = item->c.max;

					iMode = 0;
					// [BB] check true
					Video->StartModeIterator (8, true);
					while (Video->NextMode (&iWidth, &iHeight, &bLetterBox))
					{
						if ( iMode == newval.Float )
						{
							con_virtualwidth = iWidth;
							con_virtualheight = iHeight;
							break;
						}

						iMode++;
					}

					if (item->e.cfunc)
						item->e.cfunc (item->a.cvar, newval.Float);
					else
						item->a.cvar->SetGenericRep (newval, CVAR_Float);
				}
				S_Sound (CHAN_VOICE | CHAN_UI, "menu/change", 1, ATTN_NONE);
				break;

			default:
				break;
		}
		break;

	case GK_RIGHT:
		switch (item->type)
		{
			case slider:
			case absslider:
			case intslider:
				{
					// [BB] Awful hack for the player color handling. Was already here when I joined ST.
					if (!stricmp (item->label, "Red") && (item->type == slider))
					{
						int cr = g_ulPlayerSetupColor;
						int red = RPART(cr);

						red += 16;
						if (red > 255)
							red = 255;
						SendNewColor (red, GPART(cr), BPART(cr));

						g_bSwitchColorBack = false;
					}
					else if (!stricmp (item->label, "Green") && (item->type == slider))
					{
						int cr = g_ulPlayerSetupColor;
						int green = GPART(cr);

						green += 16;
						if (green > 255)
							green = 255;
						SendNewColor (RPART(cr), green, BPART(cr));

						g_bSwitchColorBack = false;
					}
					else if (!stricmp (item->label, "Blue") && (item->type == slider))
					{
						int cr = g_ulPlayerSetupColor;
						int blue = BPART(cr);

						blue += 16;
						if (blue > 255)
							blue = 255;
						SendNewColor (RPART(cr), GPART(cr), blue);

						g_bSwitchColorBack = false;
					}
					else
					{
						UCVarValue newval;
						bool reversed;

						if (item->type == intslider)
							value.Float = item->a.fval;
						else
							value = item->a.cvar->GetGenericRep (CVAR_Float);
						reversed = item->type == absslider && value.Float < 0.f;
						newval.Float = (reversed ? -value.Float : value.Float) + item->d.step;

						if (newval.Float > item->c.max)
							newval.Float = item->c.max;
						else if (newval.Float < item->b.min)
							newval.Float = item->b.min;

						if (reversed)
						{
							newval.Float = -newval.Float;
						}

						if (item->type == intslider)
							item->a.fval = newval.Float;
						else if (item->e.cfunc)
							item->e.cfunc (item->a.cvar, newval.Float);
						else
							item->a.cvar->SetGenericRep (newval, CVAR_Float);
					}
				}
				S_Sound (CHAN_VOICE | CHAN_UI, "menu/change", 1, ATTN_NONE);
				break;

			case palettegrid:
				SelColorIndex = (SelColorIndex + 1) & 15;
				S_Sound (CHAN_VOICE | CHAN_UI, "menu/cursor", 1, ATTN_NONE);
				break;

			case discretes:
			case discrete:
			case cdiscrete:
				{
					// Hack for autoaim.
					if ( strcmp( item->label, "Autoaim" ) == 0 )
					{
						float ranges[] = { 0, 0.25, 0.5, 1, 2, 3, 5000 };
						float aim = menu_autoaim;
						int i;

						// Select a higher autoaim
						for (i = 5; i >= 0; i--)
						{
							if (aim >= ranges[i])
							{
								aim = ranges[i + 1];
								break;
							}
						}
	
						menu_autoaim = aim;
					}
					else
					{
						int cur;
						int numvals;

						numvals = (int)item->b.min;
						value = item->a.cvar->GetGenericRep (CVAR_Float);
					if (item->type != discretes)
					{
						cur = M_FindCurVal (value.Float, item->e.values, numvals);
					}
					else
					{
						cur = M_FindCurVal (value.Float, item->e.valuestrings, numvals);
					}
						if (++cur >= numvals)
							cur = 0;

						value.Float = item->type != discretes ? item->e.values[cur].value : item->e.valuestrings[cur].value;
						item->a.cvar->SetGenericRep (value, CVAR_Float);

						// Hack hack. Rebuild list of resolutions
						if (item->e.values == Depths)
							BuildModesList (SCREENWIDTH, SCREENHEIGHT, DisplayBits);

						// Hack for the browser menu. If we changed a setting, rebuild the list.
						if ( CurrentMenu == &BrowserMenu )
							M_BuildServerList( );
					}
				}
				S_Sound (CHAN_VOICE | CHAN_UI, "menu/change", 1, ATTN_NONE);
				break;
			case ediscrete:
				value = item->a.cvar->GetGenericRep(CVAR_String);
				value.String = const_cast<char *>(M_FindNextVal(value.String, item->e.enumvalues, (int)item->b.numvalues));
				item->a.cvar->SetGenericRep(value, CVAR_String);
				S_Sound (CHAN_VOICE | CHAN_UI, "menu/change", 1, ATTN_NONE);
				break;

			case bitmask:
				{
					int cur;
					int numvals;
					int bmask = int(item->c.max);

					numvals = (int)item->b.min;
					value = item->a.cvar->GetGenericRep (CVAR_Int);
					
					cur = M_FindCurVal (value.Int & bmask, item->e.values, numvals);
					if (++cur >= numvals)
						cur = 0;

					value.Int = (value.Int & ~bmask) | int(item->e.values[cur].value);
					item->a.cvar->SetGenericRep (value, CVAR_Int);
				}
				S_Sound (CHAN_VOICE | CHAN_UI, "menu/change", 1, ATTN_NONE);
				break;
			case discrete_guid:
				{
					int cur;
					int numvals;

					numvals = (int)item->b.numvalues;
					cur = M_FindCurGUID (*(item->a.guidcvar), item->e.guidvalues, numvals);
					if (++cur >= numvals)
						cur = 0;

					*(item->a.guidcvar) = item->e.guidvalues[cur].ID;
				}
				S_Sound (CHAN_VOICE | CHAN_UI, "menu/change", 1, ATTN_NONE);
				break;

			case inverter:
				value = item->a.cvar->GetGenericRep (CVAR_Float);
				value.Float = -value.Float;
				item->a.cvar->SetGenericRep (value, CVAR_Float);
				S_Sound (CHAN_VOICE | CHAN_UI, "menu/change", 1, ATTN_NONE);
				break;

			case screenres:
				{
					int col;

					col = item->a.selmode + 1;
					if ((col > 2) || (col == 2 && !item->d.res3) || (col == 1 && !item->c.res2))
					{
						if (CurrentMenu->numitems - 1 > CurrentItem)
						{
							if (CurrentMenu->items[CurrentItem + 1].type == screenres)
							{
								if (CurrentMenu->items[CurrentItem + 1].b.res1)
								{
									item->a.selmode = -1;
									CurrentMenu->items[++CurrentItem].a.selmode = 0;
								}
							}
						}
					}
					else
					{
						item->a.selmode = col;
					}
				}
				S_Sound (CHAN_VOICE | CHAN_UI, "menu/cursor", 1, ATTN_NONE);
				break;

			case number:

				{
					value = item->a.cvar->GetGenericRep( CVAR_Int );
					
					value.Int += item->d.step;
					if ( value.Int > item->c.max )
						value.Int = item->b.min;

					item->a.cvar->SetGenericRep( value, CVAR_Int );
					S_Sound( CHAN_VOICE | CHAN_UI, "menu/change", 1, ATTN_NONE );
				}
				break;
			case skintype:

				if ((( GetDefaultByType( PlayerClass->Type )->flags4 & MF4_NOSKIN ) == false ) &&
					( g_lPlayerSetupClass != -1 ))
				{
					LONG	lSkin = g_ulPlayerSetupSkin;

					// Don't allow hidden skins to be selectable.
					do
					{
						lSkin++;
						if ( lSkin >= (int)skins.Size() )
							lSkin = 0;

					} while (( skins[lSkin].bRevealed == false ) || ( PlayerClass->CheckSkin( lSkin ) == false ));

					g_ulPlayerSetupSkin = lSkin;
					cvar_set( "menu_skin", skins[lSkin].name );

					if ( skins[lSkin].szColor[0] != 0 )
					{
						if ( g_bSwitchColorBack == false )
						{
							UCVarValue	Val;

							Val = menu_color.GetGenericRep( CVAR_Int );
							g_lSavedColor = Val.Int;

							g_bSwitchColorBack = true;
						}
						
						cvar_set( "menu_color", skins[lSkin].szColor );
						g_ulPlayerSetupColor = V_GetColorFromString( NULL, skins[lSkin].szColor );
					}
					else if ( g_bSwitchColorBack )
					{
						UCVarValue	Val;

						Val.Int = g_lSavedColor;
						menu_color.SetGenericRep( Val, CVAR_Int );
						g_ulPlayerSetupColor = g_lSavedColor;

						g_bSwitchColorBack = false;
					}
				}
				S_Sound( CHAN_VOICE | CHAN_UI, "menu/change", 1, ATTN_NONE );
				break;
			case classtype:

				if ( PlayerClasses.Size( ) > 0 )
				{
					LONG	lClass = g_lPlayerSetupClass;

					lClass++;
					if ( lClass >= (LONG)PlayerClasses.Size() )
						lClass = -1;

					cvar_set( "menu_playerclass", ( lClass == -1 ) ? "random" : PlayerClasses[lClass].Type->Meta.GetMetaString (APMETA_DisplayName));

					g_lPlayerSetupClass = lClass;
				}
				S_Sound( CHAN_VOICE | CHAN_UI, "menu/change", 1, ATTN_NONE );
				break;
			case botslot:
				{
					LONG		lBotIdx = -1;
					FBaseCVar	*pVar;
					UCVarValue	Val;
					char		szCVarName[32];

					if ( GAMEMODE_GetFlags( static_cast<GAMEMODE_e>(menu_gamemode.GetGenericRep( CVAR_Int ).Int) ) & GMF_PLAYERSONTEAMS )
					{
						// [CW] Add to this when bumping 'MAX_TEAMS'.
						switch( CurrentItem )
						{
						case TEAM1_STARTMENUPOS:
						case TEAM1_STARTMENUPOS + 1:
						case TEAM1_STARTMENUPOS + 2:
						case TEAM1_STARTMENUPOS + 3:
						case TEAM1_STARTMENUPOS + 4:
							sprintf( szCVarName, "menu_teambotspawn%d", CurrentItem - TEAM1_STARTMENUPOS );
							break;

						case TEAM2_STARTMENUPOS:
						case TEAM2_STARTMENUPOS + 1:
						case TEAM2_STARTMENUPOS + 2:
						case TEAM2_STARTMENUPOS + 3:
						case TEAM2_STARTMENUPOS + 4:
							sprintf( szCVarName, "menu_teambotspawn%d", CurrentItem - TEAM2_STARTMENUPOS + 5 );
							break;

						case TEAM3_STARTMENUPOS:
						case TEAM3_STARTMENUPOS + 1:
						case TEAM3_STARTMENUPOS + 2:
						case TEAM3_STARTMENUPOS + 3:
						case TEAM3_STARTMENUPOS + 4:
							sprintf( szCVarName, "menu_teambotspawn%d", CurrentItem - TEAM3_STARTMENUPOS + 2*5 );
							break;

						case TEAM4_STARTMENUPOS:
						case TEAM4_STARTMENUPOS + 1:
						case TEAM4_STARTMENUPOS + 2:
						case TEAM4_STARTMENUPOS + 3:
						case TEAM4_STARTMENUPOS + 4:
							sprintf( szCVarName, "menu_teambotspawn%d", CurrentItem - TEAM4_STARTMENUPOS + 3*5 );
							break;

						default:
							break;
						}
					}
					else
						sprintf( szCVarName, "menu_botspawn%d", CurrentItem );

					pVar = FindCVar( szCVarName, NULL );

					Val = pVar->GetGenericRep( CVAR_Int );

					// Increment the bot index. Skip over non-revealed bots.
					lBotIdx = Val.Int;
					do
					{
						lBotIdx++;

						if ( lBotIdx >= MAX_BOTINFO )
							lBotIdx = -1;

					} while ( lBotIdx >= 0 && BOTINFO_GetRevealed( lBotIdx ) == false );

					// Finally, set the index in the botspawn slot.
					Val.Int = lBotIdx;
					pVar->SetGenericRep( Val, CVAR_Int );
				}
				S_Sound( CHAN_VOICE | CHAN_UI, "menu/change", 1, ATTN_NONE );
				break;
			case announcer:
				{
					LONG	lAnnouncerIdx;

					lAnnouncerIdx = *(item->a.intcvar);

					lAnnouncerIdx++;
					if ( lAnnouncerIdx >= static_cast<signed> (ANNOUNCER_GetNumProfiles( )))
						lAnnouncerIdx = -1;

					*(item->a.intcvar) = lAnnouncerIdx;
				}
				S_Sound( CHAN_VOICE | CHAN_UI, "menu/change", 1, ATTN_NONE );
				break;
			case levelslot:
				{
					SHORT	sLevelIdx;
						
					sLevelIdx = *(item->a.intcvar);

					sLevelIdx++;
					if ( sLevelIdx >= static_cast<signed> (wadlevelinfos.Size( )))
						sLevelIdx = 0;

					*(item->a.intcvar) = sLevelIdx;
				}
				S_Sound( CHAN_VOICE | CHAN_UI, "menu/change", 1, ATTN_NONE );
				break;

			case txslider:
				{
					UCVarValue	newval;
					int			iMode;
					int			iWidth;
					int			iHeight;
					bool		bLetterBox;

					value = item->a.cvar->GetGenericRep (CVAR_Float);
					newval.Float = value.Float + item->d.step;

					if (newval.Float > item->c.max)
						newval.Float = item->c.max;
					else if (newval.Float < item->b.min)
						newval.Float = item->b.min;

					iMode = 0;
					// [BB] Check true
					Video->StartModeIterator (8, true);
					while (Video->NextMode (&iWidth, &iHeight, &bLetterBox))
					{
						if ( iMode == newval.Float )
						{
							con_virtualwidth = iWidth;
							con_virtualheight = iHeight;
							break;
						}

						iMode++;
					}

					if (item->e.cfunc)
						item->e.cfunc (item->a.cvar, newval.Float);
					else
						item->a.cvar->SetGenericRep (newval, CVAR_Float);
				}
				S_Sound (CHAN_VOICE | CHAN_UI, "menu/change", 1, ATTN_NONE);
				break;

			default:
				break;
		}
		break;

	case '\b':
		if (item->type == control)
		{
			C_UnbindACommand (item->e.command);
			item->b.key1 = item->c.key2 = 0;
		}
		break;

	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
		{
			int lookfor = ch == '0' ? 10 : ch - '0', i;
			for (i = 0; i < CurrentMenu->numitems; ++i)
			{
				if (CurrentMenu->items[i].b.position == lookfor)
				{
					CurrentItem = i;
					item = &CurrentMenu->items[i];
					break;
				}
			}
			if (i == CurrentMenu->numitems)
			{
				break;
			}
			// Otherwise, fall through to '\r' below
		}

	case '\r':
		if (CurrentMenu == &ModesMenu && item->type == screenres)
		{
			if (!GetSelectedSize (CurrentItem, &NewWidth, &NewHeight))
			{
				NewWidth = SCREENWIDTH;
				NewHeight = SCREENHEIGHT;
			}
			else
			{
				testingmode = 1;
				setmodeneeded = true;
				NewBits = BitTranslate[DummyDepthCvar];
			}
			S_Sound (CHAN_VOICE | CHAN_UI, "menu/choose", 1, ATTN_NONE);
			SetModesMenu (NewWidth, NewHeight, NewBits);
		}
		else if ((item->type == more ||
				  item->type == numberedmore ||
				  item->type == rightmore ||
				  item->type == rsafemore ||
				  item->type == safemore)
				 && item->e.mfunc)
		{
			CurrentMenu->lastOn = CurrentItem;
			S_Sound (CHAN_VOICE | CHAN_UI, "menu/choose", 1, ATTN_NONE);
			if (item->type == safemore || item->type == rsafemore)
			{
				ActivateConfirm (item->label, item->e.mfunc);
			}
			else
			{
				item->e.mfunc();
			}
		}
		else if (item->type == discrete || item->type == cdiscrete || item->type == discretes)
		{
			int cur;
			int numvals;

			numvals = (int)item->b.min;
			value = item->a.cvar->GetGenericRep (CVAR_Float);
			if (item->type != discretes)
			{
				cur = M_FindCurVal (value.Float, item->e.values, numvals);
			}
			else
			{
				cur = M_FindCurVal (value.Float, item->e.valuestrings, numvals);
			}
			if (++cur >= numvals)
				cur = 0;

			value.Float = item->type != discretes ? item->e.values[cur].value : item->e.valuestrings[cur].value;
			item->a.cvar->SetGenericRep (value, CVAR_Float);

			// Hack hack. Rebuild list of resolutions
			if (item->e.values == Depths)
				BuildModesList (SCREENWIDTH, SCREENHEIGHT, DisplayBits);

			S_Sound (CHAN_VOICE | CHAN_UI, "menu/change", 1, ATTN_NONE);
		}
		else if (item->type == control)
		{
			menuactive = MENU_WaitKey;
			OldMessage = CurrentMenu->items[0].label;
			OldType = CurrentMenu->items[0].type;
			CurrentMenu->items[0].label = "Press new key for control, ESC to cancel";
			CurrentMenu->items[0].type = redtext;
		}
		else if (item->type == listelement)
		{
			CurrentMenu->lastOn = CurrentItem;
			S_Sound (CHAN_VOICE | CHAN_UI, "menu/choose", 1, ATTN_NONE);
			item->e.lfunc (CurrentItem);
		}
		else if (item->type == inverter)
		{
			value = item->a.cvar->GetGenericRep (CVAR_Float);
			value.Float = -value.Float;
			item->a.cvar->SetGenericRep (value, CVAR_Float);
			S_Sound (CHAN_VOICE | CHAN_UI, "menu/change", 1, ATTN_NONE);
		}
		else if (item->type == screenres)
		{
		}
		else if (item->type == colorpicker)
		{
			CurrentMenu->lastOn = CurrentItem;
			S_Sound (CHAN_VOICE | CHAN_UI, "menu/choose", 1, ATTN_NONE);
			StartColorPickerMenu (item->label, item->a.colorcvar);
		}
		else if (item->type == palettegrid)
		{
			UpdateSelColor (SelColorIndex + int(item->b.min * 16));
		}
		else if (( item->type == string ) || ( item->type == pwstring ))
		{
			// Copy the current value into our temporary string buffer.
			sprintf( g_szStringInputBuffer, "%s", item->a.stringcvar->GetGenericRep( CVAR_String ).String );

			// Enter string input mode.
			g_bStringInput = true;
		}
		else if ( item->type == mnnumber )
		{
			UCVarValue	Val;

			Val = item->a.intcvar->GetGenericRep( CVAR_Int );

			// Copy the current value into our temporary string buffer.
			sprintf( g_szStringInputBuffer, "%s", itoa( Val.Int, g_szStringInputBuffer, 10 ));

			// Enter string input mode.
			g_bStringInput = true;
		}
		else if ( item->type == browserslot )
		{
			if ( M_ShouldShowServer( item->f.lServer ))
			{
				g_lSelectedServer = item->f.lServer;
				S_Sound (CHAN_VOICE | CHAN_UI, "menu/choose", 1, ATTN_NONE);
			}
		}

		break;

	case GK_ESCAPE:

		CurrentMenu->lastOn = CurrentItem;
		if (CurrentMenu->EscapeHandler != NULL)
		{
			CurrentMenu->EscapeHandler ();
		}
		M_PopMenuStack ();
		break;

	case '+':

		if (( CurrentMenu == &WeaponSetupMenu ) && ( item->type == weaponslot ))
		{
			ULONG	ulIdx;
			ULONG	ulRankOn;

			ulRankOn = 0;
			for ( ulIdx = 0; static_cast<signed> (ulIdx) < CurrentMenu->numitems; ulIdx++ )
			{
				if (( CurrentMenu->items + ulIdx )->type == weaponslot )
					ulRankOn++;

				if ( static_cast<signed> (ulIdx) == CurrentItem )
					break;
			}

			if (( ulRankOn != 0 ) && ( static_cast<signed> (ulIdx) != CurrentMenu->numitems ))
			{
				const char	*pszString;
				
				pszString = NULL;//GetWeaponPrefNameByRank( ulRankOn );
				if ( pszString )
				{
//					if ( IncreaseWeaponPrefPosition( (char *)pszString, false ))
					if ( 0 )
					{
						CurrentItem--;
						S_Sound( CHAN_VOICE, "menu/change", 1, ATTN_NONE );
						M_RefreshWeaponSetupItems( );
					}
				}
			}
		}
		break;
	case '-':

		if (( CurrentMenu == &WeaponSetupMenu ) && ( item->type == weaponslot ))
		{
			ULONG	ulIdx;
			ULONG	ulRankOn;

			ulRankOn = 0;
			for ( ulIdx = 0; static_cast<signed> (ulIdx) < CurrentMenu->numitems; ulIdx++ )
			{
				if (( CurrentMenu->items + ulIdx )->type == weaponslot )
					ulRankOn++;

				if ( static_cast<signed> (ulIdx) == CurrentItem )
					break;
			}

			if (( ulRankOn != 0 ) && ( static_cast<signed> (ulIdx) != CurrentMenu->numitems ))
			{
				const char	*pszString;
				
				pszString = NULL;//GetWeaponPrefNameByRank( ulRankOn );
				if ( pszString )
				{
//					if ( DecreaseWeaponPrefPosition( (char *)pszString, false ))
					if ( 0 )
					{
						CurrentItem++;
						S_Sound( CHAN_VOICE, "menu/change", 1, ATTN_NONE );
						M_RefreshWeaponSetupItems( );
					}
				}
			}
		}
		break;
	case ' ':
		if ( CurrentMenu == &PlayerSetupMenu )
		{
			PlayerRotation ^= 8;
			break;
		}
		// intentional fall-through

	default:
		if (ch == 't')
		{
			// Test selected resolution
			if (CurrentMenu == &ModesMenu)
			{
				if (!(item->type == screenres &&
					GetSelectedSize (CurrentItem, &NewWidth, &NewHeight)))
				{
					NewWidth = SCREENWIDTH;
					NewHeight = SCREENHEIGHT;
				}
				OldWidth = SCREENWIDTH;
				OldHeight = SCREENHEIGHT;
				OldBits = DisplayBits;
				NewBits = BitTranslate[DummyDepthCvar];
				setmodeneeded = true;
				testingmode = I_GetTime(false) + 5 * TICRATE;
				S_Sound (CHAN_VOICE | CHAN_UI, "menu/choose", 1, ATTN_NONE);
				SetModesMenu (NewWidth, NewHeight, NewBits);
			}
		}
		break;
	}
}

static void GoToConsole (void)
{
	M_ClearMenus ();
	C_ToggleConsole ();
}

static void UpdateStuff (void)
{
	M_SizeDisplay (0);
}

void Reset2Defaults (void)
{
	C_SetDefaultBindings ();
	C_SetCVarsToDefaults ();
	UpdateStuff();
}

void Reset2Saved (void)
{
	GameConfig->DoGlobalSetup ();
	GameConfig->DoGameSetup (GameNames[gameinfo.gametype]);
	UpdateStuff();
}

static void StartMessagesMenu (void)
{
	M_SwitchMenu (&MessagesMenu);
}

static void StartAutomapMenu (void)
{
	M_SwitchMenu (&AutomapMenu);
}

static void StartHUDMenu (void)
{
	M_SwitchMenu (&HUDMenu);
}
CCMD (menu_messages)
{
	M_StartControlPanel (true);
	OptionsActive = true;
	StartMessagesMenu ();
}

CCMD (menu_automap)
{
	M_StartControlPanel (true);
	OptionsActive = true;
	StartAutomapMenu ();
}
/* [BB] ST doesn't use this.
CCMD (menu_scoreboard)
{
	M_StartControlPanel (true);
	OptionsActive = true;
	StartScoreboardMenu ();
}
*/
static void StartMapColorsMenu (void)
{
	M_SwitchMenu (&MapColorsMenu);
}

CCMD (menu_mapcolors)
{
	M_StartControlPanel (true);
	OptionsActive = true;
	StartMapColorsMenu ();
}

static void DefaultCustomColors ()
{
	// Find the color cvars by scanning the MapColors menu.
	for (int i = 0; i < MapColorsMenu.numitems; ++i)
	{
		if (MapColorsItems[i].type == colorpicker)
		{
			MapColorsItems[i].a.colorcvar->ResetToDefault ();
		}
	}
}

static void ColorPickerDrawer ()
{
	DWORD newColor = MAKEARGB(255,
		int(ColorPickerItems[2].a.fval),
		int(ColorPickerItems[3].a.fval),
		int(ColorPickerItems[4].a.fval));
	DWORD oldColor = DWORD(*ColorPickerItems[0].a.colorcvar) | 0xFF000000;

	int x = screen->GetWidth()*2/3;
	int y = (15 + BigFont->GetHeight() + SmallFont->GetHeight()*2 - 102) * CleanYfac + screen->GetHeight()/2;

	screen->Clear (x, y, x + 48*CleanXfac, y + 48*CleanYfac, -1, oldColor);
	screen->Clear (x + 48*CleanXfac, y, x + 48*2*CleanXfac, y + 48*CleanYfac, -1, newColor);

	y += 49*CleanYfac;
	screen->DrawText (SmallFont, CR_GRAY, x+(24-SmallFont->StringWidth("Old")/2)*CleanXfac, y,
		"Old", DTA_CleanNoMove, true, TAG_DONE);
	screen->DrawText (SmallFont, CR_WHITE, x+(48+24-SmallFont->StringWidth("New")/2)*CleanXfac, y,
		"New", DTA_CleanNoMove, true, TAG_DONE);
}

static void SetColorPickerSliders ()
{
	FColorCVar *cvar = ColorPickerItems[0].a.colorcvar;
	ColorPickerItems[2].a.fval = RPART(DWORD(*cvar));
	ColorPickerItems[3].a.fval = GPART(DWORD(*cvar));
	ColorPickerItems[4].a.fval = BPART(DWORD(*cvar));
	CurrColorIndex = cvar->GetIndex();
}

static void UpdateSelColor (int index)
{
	ColorPickerItems[2].a.fval = GPalette.BaseColors[index].r;
	ColorPickerItems[3].a.fval = GPalette.BaseColors[index].g;
	ColorPickerItems[4].a.fval = GPalette.BaseColors[index].b;
}

static void ColorPickerReset ()
{
	SetColorPickerSliders ();
}

static void ActivateColorChoice ()
{
	UCVarValue val;
	val.Int = MAKERGB
		(int(ColorPickerItems[2].a.fval),
		 int(ColorPickerItems[3].a.fval),
		 int(ColorPickerItems[4].a.fval));
	ColorPickerItems[0].a.colorcvar->SetGenericRep (val, CVAR_Int);
}

static void StartColorPickerMenu (const char *colorname, FColorCVar *cvar)
{
	ColorPickerMenu.PreDraw = ColorPickerDrawer;
	ColorPickerMenu.EscapeHandler = ActivateColorChoice;
	ColorPickerItems[0].label = colorname;
	ColorPickerItems[0].a.colorcvar = cvar;
	SetColorPickerSliders ();
    M_SwitchMenu (&ColorPickerMenu);
}

static void CustomizeControls (void)
{
	M_BuildKeyList (ControlsMenu.items, ControlsMenu.numitems);
	M_SwitchMenu (&ControlsMenu);
}

CCMD (menu_keys)
{
	M_StartControlPanel (true);
	OptionsActive = true;
	CustomizeControls ();
}

EXTERN_CVAR (Int, dmflags)

static void GameplayOptions (void)
{
	M_SwitchMenu (&DMFlagsMenu);
	flagsvar = SHOW_DMFlags | SHOW_DMFlags2;
}

static void SkirmishGameplayOptions (void)
{
	M_SwitchMenu (&SkirmishDMFlagsMenu);
	flagsvar = SHOW_MenuDMFlags | SHOW_MenuDMFlags2;
}

CCMD (menu_gameplay)
{
	M_StartControlPanel (true);
	OptionsActive = true;
	GameplayOptions ();
}

static void CompatibilityOptions (void)
{
	M_SwitchMenu (&CompatibilityMenu);
	flagsvar = SHOW_CompatFlags;
}

CCMD (menu_compatibility)
{
	M_StartControlPanel (true);
	OptionsActive = true;
	CompatibilityOptions ();
}

static void MouseOptions ()
{
	M_SwitchMenu (&MouseMenu);
}

CCMD (menu_mouse)
{
	M_StartControlPanel (true);
	OptionsActive = true;
	MouseOptions ();
}

void UpdateJoystickMenu ()
{
	static FIntCVar * const cvars[8] =
	{
		&joy_xaxis, &joy_yaxis, &joy_zaxis,
		&joy_xrot, &joy_yrot, &joy_zrot,
		&joy_slider, &joy_dial
	};
	static FFloatCVar * const cvars2[5] =
	{
		&joy_yawspeed, &joy_pitchspeed, &joy_forwardspeed,
		&joy_sidespeed, &joy_upspeed
	};
	static FFloatCVar * const cvars3[8] =
	{
		&joy_xthreshold, &joy_ythreshold, &joy_zthreshold,
		&joy_xrotthreshold, &joy_yrotthreshold, &joy_zrotthreshold,
		&joy_sliderthreshold, &joy_dialthreshold
	};

	int i, line;

	if (JoystickNames.Size() == 0)
	{
		JoystickItems[0].type = redtext;
		JoystickItems[0].label = "No joysticks connected";
		line = 1;
	}
	else
	{
		JoystickItems[0].type = discrete;
		JoystickItems[0].label = "Enable joystick";

		JoystickItems[1].b.numvalues = float(JoystickNames.Size());
		JoystickItems[1].e.guidvalues = &JoystickNames[0];

		line = 5;

		for (i = 0; i < 8; ++i)
		{
			if (JoyAxisNames[i] != NULL)
			{
				JoystickItems[line].label = JoyAxisNames[i];
				JoystickItems[line].type = discrete;
				JoystickItems[line].a.intcvar = cvars[i];
				JoystickItems[line].b.numvalues = 6.f;
				JoystickItems[line].d.graycheck = NULL;
				JoystickItems[line].e.values = JoyAxisMapNames;
				line++;
			}
		}

		JoystickItems[line].type = redtext;
		JoystickItems[line].label = " ";
		line++;

		JoystickItems[line].type = whitetext;
		JoystickItems[line].label = "Axis Sensitivity";
		line++;

		for (i = 0; i < 5; ++i)
		{
			JoystickItems[line].type = absslider;
			JoystickItems[line].label = JoyAxisMapNames[i+1].name;
			JoystickItems[line].a.cvar = cvars2[i];
			JoystickItems[line].b.min = 0.0;
			JoystickItems[line].c.max = 4.0;
			JoystickItems[line].d.step = 0.2;
			line++;

			JoystickItems[line].type = inverter;
			JoystickItems[line].label = JoyAxisMapNames[i+1].name;
			JoystickItems[line].a.cvar = cvars2[i];
			JoystickItems[line].e.values = Inversion;
			line++;
		}

		JoystickItems[line].type = redtext;
		JoystickItems[line].label = " ";
		line++;

		JoystickItems[line].type = whitetext;
		JoystickItems[line].label = "Axis Dead Zones";
		line++;

		for (i = 0; i < 8; ++i)
		{
			if (JoyAxisNames[i] != NULL)
			{
				JoystickItems[line].label = JoyAxisNames[i];
				JoystickItems[line].type = slider;
				JoystickItems[line].a.cvar = cvars3[i];
				JoystickItems[line].b.min = 0.0;
				JoystickItems[line].c.max = 0.9;
				JoystickItems[line].d.step = 0.05;
				line++;
			}
		}
	}

	JoystickMenu.numitems = line;
	if (JoystickMenu.lastOn >= line)
	{
		JoystickMenu.lastOn = line - 1;
	}
	if (screen != NULL)
	{
		CalcIndent (&JoystickMenu);
	}
}

static void JoystickOptions ()
{
	UpdateJoystickMenu ();
	M_SwitchMenu (&JoystickMenu);
}

CCMD (menu_joystick)
{
	M_StartControlPanel (true);
	OptionsActive = true;
	JoystickOptions ();
}

static void FreeMIDIMenuList()
{
	if (SoundItems[MIDI_DEVICE_ITEM].e.values != NULL)
	{
		delete[] SoundItems[MIDI_DEVICE_ITEM].e.values;
	}
}

static void SoundOptions ()
{
	I_BuildMIDIMenuList(&SoundItems[MIDI_DEVICE_ITEM].e.values, &SoundItems[MIDI_DEVICE_ITEM].b.min);
	atterm(FreeMIDIMenuList);
	M_SwitchMenu(&SoundMenu);
}

CCMD (menu_sound)
{
	M_StartControlPanel (true);
	OptionsActive = true;
	SoundOptions ();
}

static void AdvSoundOptions ()
{
	M_SwitchMenu (&AdvSoundMenu);
}

CCMD (menu_advsound)
{
	M_StartControlPanel (true);
	OptionsActive = true;
	AdvSoundOptions ();
}

static void MakeSoundChanges (void)
{
	static char snd_reset[] = "snd_reset";
	AddCommandString (snd_reset);
}

static void ModReplayerOptions()
{
	for (size_t i = 2; i < countof(ModReplayerItems); ++i)
	{
		if (ModReplayerItems[i].type == discrete)
		{
			ModReplayerItems[i].d.graycheck = &mod_dumb;
		}
	}
	M_SwitchMenu(&ModReplayerMenu);
}

CCMD (menu_modreplayer)
{
	M_StartControlPanel(true);
	OptionsActive = true;
	ModReplayerOptions();
}

static void VideoOptions (void)
{
	InitCrosshairsList();
	M_SwitchMenu (&VideoMenu);
}

CCMD (menu_display)
{
	M_StartControlPanel (true);
	OptionsActive = true;
	InitCrosshairsList();
	M_SwitchMenu (&VideoMenu);
}

static void BuildModesList (int hiwidth, int hiheight, int hi_bits)
{
	char strtemp[32], **str;
	int	 i, c;
	int	 width, height, showbits;
	bool letterbox=false;
	int  ratiomatch;

	if (menu_screenratios >= 0 && menu_screenratios <= 4 && menu_screenratios != 3)
	{
		ratiomatch = menu_screenratios;
	}
	else
	{
		ratiomatch = -1;
	}
	showbits = BitTranslate[DummyDepthCvar];

	if (Video != NULL)
	{
		Video->StartModeIterator (showbits, screen->IsFullscreen());
	}

	for (i = VM_RESSTART; ModesItems[i].type == screenres; i++)
	{
		ModesItems[i].e.highlight = -1;
		for (c = 0; c < 3; c++)
		{
			bool haveMode = false;

			switch (c)
			{
			default: str = &ModesItems[i].b.res1; break;
			case 1:  str = &ModesItems[i].c.res2; break;
			case 2:  str = &ModesItems[i].d.res3; break;
			}
			if (Video != NULL)
			{
				while ((haveMode = Video->NextMode (&width, &height, &letterbox)) &&
					(ratiomatch >= 0 && CheckRatio (width, height) != ratiomatch))
				{
				}
			}

			if (haveMode)
			{
				if (/* hi_bits == showbits && */ width == hiwidth && height == hiheight)
					ModesItems[i].e.highlight = ModesItems[i].a.selmode = c;
				
				mysnprintf (strtemp, countof(strtemp), "%dx%d%s", width, height, letterbox?TEXTCOLOR_BROWN" LB":"");
				ReplaceString (str, strtemp);
			}
			else
			{
				if (*str)
				{
					delete[] *str;
					*str = NULL;
				}
			}
		}
	}
}

void M_RefreshModesList ()
{
	BuildModesList (SCREENWIDTH, SCREENHEIGHT, DisplayBits);
}

void M_FreeModesList ()
{
	for (int i = VM_RESSTART; ModesItems[i].type == screenres; ++i)
	{
		for (int c = 0; c < 3; ++c)
		{
			char **str;

			switch (c)
			{
			default: str = &ModesItems[i].b.res1; break;
			case 1:  str = &ModesItems[i].c.res2; break;
			case 2:  str = &ModesItems[i].d.res3; break;
			}
			if (str != NULL)
			{
				delete[] *str;
				*str = NULL;
			}
		}
	}
}

static bool GetSelectedSize (int line, int *width, int *height)
{
	if (ModesItems[line].type != screenres)
	{
		return false;
	}
	else
	{
		char *res, *breakpt;
		long x, y;

		switch (ModesItems[line].a.selmode)
		{
		default: res = ModesItems[line].b.res1; break;
		case 1:  res = ModesItems[line].c.res2; break;
		case 2:  res = ModesItems[line].d.res3; break;
		}
		x = strtol (res, &breakpt, 10);
		y = strtol (breakpt+1, NULL, 10);

		*width = x;
		*height = y;
		return true;
	}
}

static int FindBits (int bits)
{
	int i;

	for (i = 0; i < 22; i++)
	{
		if (BitTranslate[i] == bits)
			return i;
	}

	return 0;
}

static void SetModesMenu (int w, int h, int bits)
{
	DummyDepthCvar = FindBits (bits);

	if (testingmode <= 1)
	{
		if (ModesItems[VM_ENTERLINE].label != VMEnterText)
			free (const_cast<char *>(ModesItems[VM_ENTERLINE].label));
		ModesItems[VM_ENTERLINE].label = VMEnterText;
		ModesItems[VM_TESTLINE].label = VMTestText;
	}
	else
	{
		char strtemp[64];

		mysnprintf (strtemp, countof(strtemp), "TESTING %dx%dx%d", w, h, bits);
		ModesItems[VM_ENTERLINE].label = copystring (strtemp);
		ModesItems[VM_TESTLINE].label = "Please wait 5 seconds...";
	}

	BuildModesList (w, h, bits);
}

void M_RestoreMode ()
{
	NewWidth = OldWidth;
	NewHeight = OldHeight;
	NewBits = OldBits;
	setmodeneeded = true;
	testingmode = 0;
	SetModesMenu (OldWidth, OldHeight, OldBits);
}

void M_SetDefaultMode ()
{
	// Make current resolution the default
	vid_defwidth = SCREENWIDTH;
	vid_defheight = SCREENHEIGHT;
	vid_defbits = DisplayBits;
	testingmode = 0;
	SetModesMenu (SCREENWIDTH, SCREENHEIGHT, DisplayBits);
}

static void SetVidMode ()
{
	SetModesMenu (SCREENWIDTH, SCREENHEIGHT, DisplayBits);
	if (ModesMenu.items[ModesMenu.lastOn].type == screenres)
	{
		if (ModesMenu.items[ModesMenu.lastOn].a.selmode == -1)
		{
			ModesMenu.items[ModesMenu.lastOn].a.selmode++;
		}
	}
	M_SwitchMenu (&ModesMenu);
}

CCMD (menu_video)
{
	M_StartControlPanel (true);
	OptionsActive = true;
	SetVidMode ();
}

void M_LoadKeys (const char *modname, bool dbl)
{
	char section[64];

	if (GameNames[gameinfo.gametype] == NULL)
		return;

	mysnprintf (section, countof(section), "%s.%s%sBindings", GameNames[gameinfo.gametype], modname,
		dbl ? ".Double" : ".");
	if (GameConfig->SetSection (section))
	{
		const char *key, *value;
		while (GameConfig->NextInSection (key, value))
		{
			C_DoBind (key, value, dbl);
		}
	}
}

int M_DoSaveKeys (FConfigFile *config, char *section, int i, bool dbl)
{
	int most = (int)CustomControlsItems.Size();

	config->SetSection (section, true);
	config->ClearCurrentSection ();
	for (++i; i < most; ++i)
	{
		menuitem_t *item = &CustomControlsItems[i];
		if (item->type == control)
		{
			C_ArchiveBindings (config, dbl, item->e.command);
			continue;
		}
		break;
	}
	return i;
}

void M_SaveCustomKeys (FConfigFile *config, char *section, char *subsection, size_t sublen)
{
	if (ControlsMenu.items == ControlsItems)
		return;

	// Start after the normal controls
	unsigned int i = countof(ControlsItems);
	unsigned int most = CustomControlsItems.Size();

	while (i < most)
	{
		menuitem_t *item = &CustomControlsItems[i];

		if (item->type == whitetext)
		{
			assert (item->e.command != NULL);
			mysnprintf (subsection, sublen, "%s.Bindings", item->e.command);
			M_DoSaveKeys (config, section, (int)i, false);
			mysnprintf (subsection, sublen, "%s.DoubleBindings", item->e.command);
			i = M_DoSaveKeys (config, section, (int)i, true);
		}
		else
		{
			i++;
		}
	}
}

static int AddKeySpot;

void FreeKeySections()
{
	const unsigned int numStdControls = countof(ControlsItems);
	unsigned int i;

	for (i = numStdControls; i < CustomControlsItems.Size(); ++i)
	{
		menuitem_t *item = &CustomControlsItems[i];
		if (item->type == whitetext || item->type == control)
		{
			if (item->label != NULL)
			{
				delete[] item->label;
				item->label = NULL;
			}
			if (item->e.command != NULL)
			{
				delete[] item->e.command;
				item->e.command = NULL;
			}
		}
	}
}

CCMD (addkeysection)
{
	if (argv.argc() != 3)
	{
		Printf ("Usage: addkeysection <menu section name> <ini name>\n");
		return;
	}

	const int numStdControls = countof(ControlsItems);
	int i;

	if (ControlsMenu.items == ControlsItems)
	{ // No custom controls have been defined yet.
		for (i = 0; i < numStdControls; ++i)
		{
			CustomControlsItems.Push (ControlsItems[i]);
		}
	}

	// See if this section already exists
	int last = (int)CustomControlsItems.Size();
	for (i = numStdControls; i < last; ++i)
	{
		menuitem_t *item = &CustomControlsItems[i];

		if (item->type == whitetext &&
			stricmp (item->label, argv[1]) == 0)
		{ // found it
			break;
		}
	}

	if (i == last)
	{ // Add the new section
		// Limit the ini name to 32 chars
		if (strlen (argv[2]) > 32)
			argv[2][32] = 0;

		menuitem_t tempItem = { redtext, " " };

		// Add a blank line to the menu
		CustomControlsItems.Push (tempItem);

		// Add the section name to the menu
		tempItem.type = whitetext;
		tempItem.label = copystring (argv[1]);
		tempItem.e.command = copystring (argv[2]);	// Record ini section name in command field
		CustomControlsItems.Push (tempItem);
		ControlsMenu.items = &CustomControlsItems[0];

		// Load bindings for this section from the ini
		M_LoadKeys (argv[2], 0);
		M_LoadKeys (argv[2], 1);

		AddKeySpot = 0;
	}
	else
	{ // Add new keys to the end of this section
		do
		{
			i++;
		} while (i < last && CustomControlsItems[i].type == control);
		if (i < last)
		{
			AddKeySpot = i;
		}
		else
		{
			AddKeySpot = 0;
		}
	}
}

CCMD (addmenukey)
{
	if (argv.argc() != 3)
	{
		Printf ("Usage: addmenukey <description> <command>\n");
		return;
	}
	if (ControlsMenu.items == ControlsItems)
	{
		Printf ("You must use addkeysection first.\n");
		return;
	}

	menuitem_t newItem = { control, };
	newItem.label = copystring (argv[1]);
	newItem.e.command = copystring (argv[2]);
	if (AddKeySpot == 0)
	{ // Just add to the end of the menu
		CustomControlsItems.Push (newItem);
	}
	else
	{ // Add somewhere in the middle of the menu
		size_t movecount = CustomControlsItems.Size() - AddKeySpot;
		CustomControlsItems.Reserve (1);
		memmove (&CustomControlsItems[AddKeySpot+1],
				 &CustomControlsItems[AddKeySpot],
				 sizeof(menuitem_t)*movecount);
		CustomControlsItems[AddKeySpot++] = newItem;
	}
	ControlsMenu.items = &CustomControlsItems[0];
	ControlsMenu.numitems = (int)CustomControlsItems.Size();
}

void M_Deinit ()
{
	// Free bitdepth names for the modes menu.
	for (size_t i = 0; i < countof(Depths); ++i)
	{
		if (Depths[i].name != NULL)
		{
			delete[] Depths[i].name;
			Depths[i].name = NULL;
		}
	}

	// Free resolutions from the modes menu.
	M_FreeModesList();
}

// Reads any XHAIRS lumps for the names of crosshairs and
// adds them to the display options menu.
void InitCrosshairsList()
{
	int lastlump, lump;
	valuestring_t value;

	lastlump = 0;

	Crosshairs.Clear();
	value.value = 0;
	value.name = "None";
	Crosshairs.Push(value);

	while ((lump = Wads.FindLump("XHAIRS", &lastlump)) != -1)
	{
		FScanner sc(lump);
		while (sc.GetNumber())
		{
			value.value = float(sc.Number);
			sc.MustGetString();
			value.name = sc.String;
			if (value.value != 0)
			{ // Check if it already exists. If not, add it.
				unsigned int i;

				for (i = 1; i < Crosshairs.Size(); ++i)
				{
					if (Crosshairs[i].value == value.value)
					{
						break;
					}
				}
				if (i < Crosshairs.Size())
				{
					Crosshairs[i].name = value.name;
				}
				else
				{
					Crosshairs.Push(value);
				}
			}
		}
	}
	// [BB] Moved crosshair selection to the HUD menu.
	HUDMenuItems[CROSSHAIR_INDEX].b.numvalues = float(Crosshairs.Size());
	HUDMenuItems[CROSSHAIR_INDEX].e.valuestrings = &Crosshairs[0];
}
