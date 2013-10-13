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
//		DOOM selection menu, options, episode etc.
//		Sliders and icons. Kinda widget stuff.
//
//-----------------------------------------------------------------------------

// HEADER FILES ------------------------------------------------------------

#include <ctype.h>
#include <fcntl.h>
#include <stdlib.h>
#include <zlib.h>

#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif

#include "doomdef.h"
#include "gstrings.h"
#include "c_console.h"
#include "c_dispatch.h"
#include "d_main.h"
#include "i_system.h"
#include "i_video.h"
#include "v_video.h"
#include "v_palette.h"
#include "w_wad.h"
#include "r_local.h"
#include "hu_stuff.h"
#include "g_game.h"
#include "m_argv.h"
#include "m_swap.h"
#include "m_random.h"
#include "s_sound.h"
#include "doomstat.h"
#include "m_menu.h"
#include "v_text.h"
#include "st_stuff.h"
#include "d_gui.h"
#include "version.h"
#include "m_png.h"
#include "templates.h"
#include "lists.h"
#include "gi.h"
#include "p_tick.h"
#include "st_start.h"
#include "teaminfo.h"
#include "r_translate.h"
#include "g_level.h"
#include "d_event.h"
#include "colormatcher.h"
#include "d_netinf.h"
#include "gl/gl_functions.h"
// [BC] New #includes.
#include "announcer.h"
#include "chat.h"
#include "cl_demo.h"
#include "cl_main.h"
#include "cl_commands.h"
#include "network.h"
#include "deathmatch.h"
#include "team.h"
#include "campaign.h"
#include "cooperative.h"
#include "i_input.h"

// [BB]
extern bool g_bStringInput;
extern char g_szStringInputBuffer[64];
// [BB]
static TArray<valuestring_t> Announcers;

// MACROS ------------------------------------------------------------------

#define SKULLXOFF			-32
#define SELECTOR_XOFFSET	(-28)
#define SELECTOR_YOFFSET	(-1)

#define KEY_REPEAT_DELAY	(TICRATE*5/12)
#define KEY_REPEAT_RATE		(3)

#define INPUTGRID_WIDTH		13
#define INPUTGRID_HEIGHT	5

// TYPES -------------------------------------------------------------------

struct FSaveGameNode : public Node
{
	char Title[SAVESTRINGSIZE];
	FString Filename;
	bool bOldVersion;
	bool bMissingWads;
};

struct FBackdropTexture : public FTexture
{
public:
	FBackdropTexture();

	const BYTE *GetColumn(unsigned int column, const Span **spans_out);
	const BYTE *GetPixels();
	void Unload();
	bool CheckModified();

protected:
	BYTE Pixels[144*160];
	static const Span DummySpan[2];
	int LastRenderTic;

	angle_t time1, time2, time3, time4;
	angle_t t1ang, t2ang, z1ang, z2ang;

	void Render();
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void R_GetPlayerTranslation (int color, const FPlayerColorSet *colorset, FPlayerSkin *skin, FRemapTable *table);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

int  M_StringHeight (const char *string);
void M_ClearMenus ();

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void M_NewGame (int choice);
static void M_Episode (int choice);
static void M_ChooseSkill (int choice);
static void M_LoadGame (int choice);
static void M_SaveGame (int choice);
static void M_Options (int choice);
static void M_EndGame (int choice);
static void M_ReadThis (int choice);
static void M_ReadThisMore (int choice);
static void M_QuitGame (int choice);
static void M_GameFiles (int choice);
static void M_ClearSaveStuff ();

static void SCClass (int choice);
static void M_ChooseClass (int choice);

static void M_FinishReadThis (int choice);
static void M_QuickSave ();
static void M_QuickLoad ();
static void M_LoadSelect (const FSaveGameNode *file);
static void M_SaveSelect (const FSaveGameNode *file);
static void M_ReadSaveStrings ();
static void M_UnloadSaveStrings ();
static FSaveGameNode *M_RemoveSaveSlot (FSaveGameNode *file);
static void M_ExtractSaveData (const FSaveGameNode *file);
static void M_UnloadSaveData ();
static void M_InsertSaveNode (FSaveGameNode *node);
static bool M_SaveLoadResponder (event_t *ev);
static void M_SaveLoadButtonHandler(EMenuKey key);
static void M_DeleteSaveResponse (int choice);

static void M_DrawMainMenu ();
static void M_DrawReadThis ();
static void M_DrawNewGame ();
static void M_DrawEpisode ();
static void M_DrawLoad ();
static void M_DrawSave ();
static void DrawClassMenu ();
static void DrawHexenSkillMenu ();
static void M_DrawClassMenu ();

static void M_DrawHereticMainMenu ();
static void M_DrawFiles ();

// [BC] New prototypes.
static void M_Multiplayer( int choice );
static void M_ChooseBotSkill( int choice );
static void M_DrawBotSkill( );

void M_DrawFrame (int x, int y, int width, int height);
static void M_DrawSaveLoadBorder (int x,int y, int len);
static void M_DrawSaveLoadCommon ();
static void M_DrawInputGrid();

static void M_SetupNextMenu (oldmenu_t *menudef);
// [BC] No longer static so this can be used elsewhere.
/*static*/ void M_StartMessage (const char *string, void(*routine)(int));
static void M_EndMessage (int key);

// [RH] For player setup menu.
	   void M_PlayerSetup ();
static void M_PlayerSetupTicker ();
static bool M_PlayerSetupDrawer ();
// [BC] These functions are no longer needed.
/*
static void M_EditPlayerName (int choice);
static void M_ChangePlayerTeam (int choice);
static void M_PlayerNameChanged (FSaveGameNode *dummy);
static void M_PlayerNameNotChanged ();
static void M_ChangeColorSet (int choice);
static void M_SlidePlayerRed (int choice);
static void M_SlidePlayerGreen (int choice);
static void M_SlidePlayerBlue (int choice);
static void M_ChangeClass (int choice);
static void M_ChangeGender (int choice);
static void M_ChangeSkin (int choice);
static void M_ChangeAutoAim (int choice);
*/
static void PickPlayerClass ();

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

EXTERN_CVAR (String, playerclass)
EXTERN_CVAR (String, name)
EXTERN_CVAR (Int, team)

extern bool		sendpause;
extern int		flagsvar;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

EMenuState		menuactive;
menustack_t		MenuStack[16];
int				MenuStackDepth;
int				skullAnimCounter;	// skull animation counter
bool			drawSkull;			// [RH] don't always draw skull
bool			M_DemoNoPlay;
bool			OptionsActive;
FButtonStatus	MenuButtons[NUM_MKEYS];
int				MenuButtonTickers[NUM_MKEYS];

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static char		tempstring[80];
static char		underscore[2];

static FSaveGameNode *quickSaveSlot;	// NULL = no quicksave slot picked!
static FSaveGameNode *lastSaveSlot;	// Used for highlighting the most recently used slot in the menu
static int 		messageToPrint;			// 1 = message to be printed
static const char *messageString;		// ...and here is the message string!
static EMenuState messageLastMenuActive;
static void	  (*messageRoutine)(int response);	// Non-NULL if only Y/N should close message
static int		showSharewareMessage;
static int		messageSelection;		// 0 {Yes) or 1 (No) [if messageRoutine is non-NULL]

static int 		genStringEnter;	// we are going to be entering a savegame string
static size_t	genStringLen;	// [RH] Max # of chars that can be entered
static void	  (*genStringEnd)(FSaveGameNode *);
static void	  (*genStringCancel)();
static int 		saveSlot;		// which slot to save in
static size_t	saveCharIndex;	// which char we're editing

static int		LINEHEIGHT;
static const int PLAYERSETUP_LINEHEIGHT = 16;

static char		savegamestring[SAVESTRINGSIZE];
static FString	EndString;

static short	itemOn; 			// menu item skull is on
static int		MenuTime;
static int		InfoType;
static int		InfoTic;

static oldmenu_t *currentMenu;		// current menudef
static oldmenu_t *TopLevelMenu;		// The main menu everything hangs off of

static FBackdropTexture	*FireTexture;
static FRemapTable		FireRemap(256);

static const char		*genders[3] = { "male", "female", "other" };
// [BC] This is used in m_options.cpp now.
/*static*/ FPlayerClass	*PlayerClass;
static int			PlayerSkin;
static FState		*PlayerState;
static int			PlayerTics;
// [BC] This is used in m_options.cpp now.
/*static*/ int			PlayerRotation;
static TArray<int>	PlayerColorSets;

static FTexture			*SavePic;
static FBrokenLines		*SaveComment;
static List				SaveGames;
static FSaveGameNode	*TopSaveGame;
static FSaveGameNode	*SelSaveGame;
static FSaveGameNode	NewSaveNode;

static int 	epi;				// Selected episode

static const char *saved_playerclass = NULL;

// Heretic and Hexen do not, by default, come with glyphs for all of these
// characters. Oh well. Doom and Strife do.
static const char InputGridChars[INPUTGRID_WIDTH * INPUTGRID_HEIGHT] =
	"ABCDEFGHIJKLM"
	"NOPQRSTUVWXYZ"
	"0123456789+-="
	".,!?@'\":;[]()"
	"<>^#$%&*/_ \b";
static int InputGridX = INPUTGRID_WIDTH - 1;
static int InputGridY = INPUTGRID_HEIGHT - 1;
static bool InputGridOkay;		// Last input was with a controller.

// PRIVATE MENU DEFINITIONS ------------------------------------------------

//
// DOOM MENU
//
static oldmenuitem_t MainMenu[]=
{
	{1,0,'n',"M_NGAME",false,false,M_NewGame, CR_UNTRANSLATED},
	{1,0,'m',"M_MULTI",false,false,M_Multiplayer, CR_UNTRANSLATED},
	{1,0,'o',"M_OPTION",false,false,M_Options, CR_UNTRANSLATED},
	{1,0,'l',"M_LOADG",false,false,M_LoadGame, CR_UNTRANSLATED},
	{1,0,'s',"M_SAVEG",false,false,M_SaveGame, CR_UNTRANSLATED},
	{1,0,'r',"M_RDTHIS",false,false,M_ReadThis, CR_UNTRANSLATED},	// Another hickup with Special edition.
	{1,0,'q',"M_QUITG",false,false,M_QuitGame, CR_UNTRANSLATED}
};

static oldmenu_t MainDef =
{
	countof(MainMenu),
	MainMenu,
	M_DrawMainMenu,
	97,64,
	0
};

//
// HERETIC MENU
//
static oldmenuitem_t HereticMainMenu[] =
{
	{1,1,'n',"$MNU_NEWGAME",false,false,M_NewGame, CR_UNTRANSLATED},
	{1,1,'m',"$MNU_MULTIPLAYER",false,false,M_Multiplayer, CR_UNTRANSLATED},
	{1,1,'o',"$MNU_OPTIONS",false,false,M_Options, CR_UNTRANSLATED},
	{1,1,'f',"$MNU_GAMEFILES",false,false,M_GameFiles, CR_UNTRANSLATED},
	{1,1,'i',"$MNU_INFO",false,false,M_ReadThis, CR_UNTRANSLATED},
	{1,1,'q',"$MNU_QUITGAME",false,false,M_QuitGame, CR_UNTRANSLATED}
};

static oldmenu_t HereticMainDef =
{
	countof(HereticMainMenu),
	HereticMainMenu,
	M_DrawHereticMainMenu,
	110, 56,
	0
};

//
// HEXEN "NEW GAME" MENU
//
static oldmenuitem_t ClassItems[] =
{
	{ 1,1, 'f', "$MNU_FIGHTER", false, false, SCClass, CR_UNTRANSLATED },
	{ 1,1, 'c', "$MNU_CLERIC", false, false, SCClass, CR_UNTRANSLATED },
	{ 1,1, 'm', "$MNU_MAGE", false, false, SCClass, CR_UNTRANSLATED },
	{ 1,1, 'r', "$MNU_RANDOM", false, false, SCClass, CR_UNTRANSLATED}	// [RH]
};

static oldmenu_t ClassMenu =
{
	4, ClassItems,
	DrawClassMenu,
	66, 58,
	0
};

//
// [GRB] CLASS SELECT
//
oldmenuitem_t ClassMenuItems[8] =
{
	{1,1,0, NULL, false, false, M_ChooseClass, CR_UNTRANSLATED },
	{1,1,0, NULL, false, false, M_ChooseClass, CR_UNTRANSLATED },
	{1,1,0, NULL, false, false, M_ChooseClass, CR_UNTRANSLATED },
	{1,1,0, NULL, false, false, M_ChooseClass, CR_UNTRANSLATED },
	{1,1,0, NULL, false, false, M_ChooseClass, CR_UNTRANSLATED },
	{1,1,0, NULL, false, false, M_ChooseClass, CR_UNTRANSLATED },
	{1,1,0, NULL, false, false, M_ChooseClass, CR_UNTRANSLATED },
	{1,1,0, NULL, false, false, M_ChooseClass, CR_UNTRANSLATED },
};

oldmenu_t ClassMenuDef =
{
	0,
	ClassMenuItems,
	M_DrawClassMenu,
	48,63,
	0
};

//
// EPISODE SELECT
//
oldmenuitem_t EpisodeMenu[MAX_EPISODES] =
{
	{1,0,0, NULL, false, false, M_Episode, CR_UNTRANSLATED},
	{1,0,0, NULL, false, false, M_Episode, CR_UNTRANSLATED},
	{1,0,0, NULL, false, false, M_Episode, CR_UNTRANSLATED},
	{1,0,0, NULL, false, false, M_Episode, CR_UNTRANSLATED},
	{1,0,0, NULL, false, false, M_Episode, CR_UNTRANSLATED},
	{1,0,0, NULL, false, false, M_Episode, CR_UNTRANSLATED},
	{1,0,0, NULL, false, false, M_Episode, CR_UNTRANSLATED},
	{1,0,0, NULL, false, false, M_Episode, CR_UNTRANSLATED},
};

// [BB] Increased size to 9 to ensure that the map names are terminated.
char EpisodeMaps[MAX_EPISODES][9];
// [BC] Customizeable titles for skill menu.
char EpisodeSkillHeaders[MAX_EPISODES][64];
bool EpisodeNoSkill[MAX_EPISODES];

oldmenu_t EpiDef =
{
	0,
	EpisodeMenu,		// oldmenuitem_t ->
	M_DrawEpisode,		// drawing routine ->
	48,63,				// x,y
	0	 				// lastOn
};

//
// GAME FILES
//
static oldmenuitem_t FilesItems[] =
{
	{1,1,'l',"$MNU_LOADGAME",false,false,M_LoadGame, CR_UNTRANSLATED},
	{1,1,'s',"$MNU_SAVEGAME",false,false,M_SaveGame, CR_UNTRANSLATED}
};

static oldmenu_t FilesMenu =
{
	countof(FilesItems),
	FilesItems,
	M_DrawFiles,
	110,60,
	0
};

//
// DOOM SKILL SELECT
//
static oldmenuitem_t SkillSelectMenu[]={
	{ 1, 0, 0, "", false, false, M_ChooseSkill, CR_UNTRANSLATED},
	{ 1, 0, 0, "", false, false, M_ChooseSkill, CR_UNTRANSLATED},
	{ 1, 0, 0, "", false, false, M_ChooseSkill, CR_UNTRANSLATED},
	{ 1, 0, 0, "", false, false, M_ChooseSkill, CR_UNTRANSLATED},
	{ 1, 0, 0, "", false, false, M_ChooseSkill, CR_UNTRANSLATED},
	{ 1, 0, 0, "", false, false, M_ChooseSkill, CR_UNTRANSLATED},
	{ 1, 0, 0, "", false, false, M_ChooseSkill, CR_UNTRANSLATED},
	{ 1, 0, 0, "", false, false, M_ChooseSkill, CR_UNTRANSLATED},
};

static oldmenu_t SkillDef =
{
	0,
	SkillSelectMenu,	// oldmenuitem_t ->
	M_DrawNewGame,		// drawing routine ->
	48,63,				// x,y
	-1					// lastOn
};

// [BC] New menu for bot games.
oldmenuitem_t NewBotGameMenu[]=
{
	{1,0,'m',"M_MOMMY",false,false,M_ChooseBotSkill},
	{1,0,'a',"M_PAIN",false,false,M_ChooseBotSkill},
	{1,0,'b',"M_BRING",false,false,M_ChooseBotSkill},
	{1,0,'t',"M_THRIVE",false,false,M_ChooseBotSkill},
	{1,0,'n',"M_BNMARE",false,false,M_ChooseBotSkill}
};

oldmenu_t BotDef =
{
	countof(NewBotGameMenu),
	NewBotGameMenu,
	M_DrawBotSkill,
	48,63,
	2
};

static oldmenu_t HexenSkillMenu =
{
	0, 
	SkillSelectMenu,
	DrawHexenSkillMenu,
	120, 44,
	-1
};


void M_StartupSkillMenu(const char *playerclass)
{
	if (gameinfo.gametype & GAME_Raven)
	{
		SkillDef.x = 38;
		SkillDef.y = 30;

		if (gameinfo.gametype == GAME_Hexen)
		{
			HexenSkillMenu.x = 38;
			if (playerclass != NULL)
			{
				if (!stricmp(playerclass, "fighter")) HexenSkillMenu.x = 120;
				else if (!stricmp(playerclass, "cleric")) HexenSkillMenu.x = 116;
				else if (!stricmp(playerclass, "mage")) HexenSkillMenu.x = 112;
			}
		}
	}
	SkillDef.numitems = HexenSkillMenu.numitems = 0;
	for(unsigned int i = 0; i < AllSkills.Size() && i < 8; i++)
	{
		FSkillInfo &skill = AllSkills[i];

		SkillSelectMenu[i].name = skill.MenuName;
		SkillSelectMenu[i].fulltext = !skill.MenuNameIsLump;
		SkillSelectMenu[i].alphaKey = skill.MenuNameIsLump? skill.Shortcut : tolower(SkillSelectMenu[i].name[0]);
		SkillSelectMenu[i].textcolor = skill.GetTextColor();
		SkillSelectMenu[i].alphaKey = skill.Shortcut;

		if (playerclass != NULL)
		{
			FString * pmnm = skill.MenuNamesForPlayerClass.CheckKey(playerclass);
			if (pmnm != NULL)
			{
				SkillSelectMenu[i].name = GStrings(*pmnm);
				SkillSelectMenu[i].fulltext = true;
				if (skill.Shortcut == 0)
					SkillSelectMenu[i].alphaKey = tolower(SkillSelectMenu[i].name[0]);
			}
		}
		SkillDef.numitems++;
		HexenSkillMenu.numitems++;
	}
	int defskill = DefaultSkill;
	if ((unsigned int)defskill >= AllSkills.Size())
	{
		defskill = (AllSkills.Size() - 1) / 2;
	}
	// The default skill is only set the first time the menu is opened.
	// After that, it opens on whichever skill you last selected.
	if (SkillDef.lastOn < 0)
	{
		SkillDef.lastOn = defskill;
	}
	if (HexenSkillMenu.lastOn < 0)
	{
		HexenSkillMenu.lastOn = defskill;
	}
	// Hexen needs some manual coordinate adjustments based on player class
	if (gameinfo.gametype == GAME_Hexen)
	{
		M_SetupNextMenu(&HexenSkillMenu);
	}
	else
	{
		M_SetupNextMenu(&SkillDef);
	}

}

// [BC] Skulltag uses a different player setup menu.
/*
//
// [RH] Player Setup Menu
//
static oldmenuitem_t PlayerSetupMenu[] =
{
	{ 1,0,'n',NULL,M_EditPlayerName, CR_UNTRANSLATED},
	{ 2,0,'t',NULL,M_ChangePlayerTeam, CR_UNTRANSLATED},
	{ 2,0,'c',NULL,M_ChangeColorSet, CR_UNTRANSLATED},
	{ 2,0,'r',NULL,M_SlidePlayerRed, CR_UNTRANSLATED},
	{ 2,0,'g',NULL,M_SlidePlayerGreen, CR_UNTRANSLATED},
	{ 2,0,'b',NULL,M_SlidePlayerBlue, CR_UNTRANSLATED},
	{ 2,0,'t',NULL,M_ChangeClass, CR_UNTRANSLATED},
	{ 2,0,'s',NULL,M_ChangeSkin, CR_UNTRANSLATED},
	{ 2,0,'e',NULL,M_ChangeGender, CR_UNTRANSLATED},
	{ 2,0,'a',NULL,M_ChangeAutoAim, CR_UNTRANSLATED}
};

enum
{
	// These must be changed if the menu definition is altered
	PSM_RED = 3,
	PSM_GREEN = 4,
	PSM_BLUE = 5,
};

static oldmenu_t PSetupDef =
{
	countof(PlayerSetupMenu),
	PlayerSetupMenu,
	M_PlayerSetupDrawer,
	48,	47,
	0
};
*/

// [BC] This replaces the ZDoom player setup menu.
/*=======================================
 *
 * Player setup Menu
 *
 *=======================================*/

// Cvars used in the menu.
EXTERN_CVAR( String, menu_name )
EXTERN_CVAR( String, menu_skin )
EXTERN_CVAR( String, menu_playerclass )
EXTERN_CVAR( Int, menu_gender )
EXTERN_CVAR( Color, menu_color )
EXTERN_CVAR( Bool, cl_run )
EXTERN_CVAR( Int, menu_handicap )
EXTERN_CVAR( Int, menu_railcolor )
EXTERN_CVAR( Float, menu_autoaim )
EXTERN_CVAR( Bool, cl_unlagged )
EXTERN_CVAR( Int, cl_announcer )
EXTERN_CVAR(Float, snd_announcervolume)	// [WS] Skulltag's announcer volume.

// Menu option values used in this menu defined in m_options.cpp.
extern	value_t		GenderVals[3];
extern	value_t		AutoaimVals[7];
extern	value_t		TrailColorVals[11];

// Functions called by this menu.
void	M_WeaponSetup( void );
void	M_UndoPlayerSetupChanges( void );
void	M_AcceptPlayerSetupChanges( void );

// Other needed functions/variables.
void	M_SetupPlayerSetupMenu( void );

extern	ULONG		g_ulPlayerSetupSkin;
extern	ULONG		g_ulPlayerSetupColor;
extern	LONG		g_lPlayerSetupClass;

menuitem_t PlayerSetupItems[] = {
	{ string,	"Name",						{&menu_name},				{2.0}, {0.0}, {0.0}, {NULL}  },
	{ redtext,	" ",						{NULL},					{0.0}, {0.0}, {0.0}, {NULL}  },
	{ skintype,	"Skin",						{&menu_skin},				{2.0}, {0.0}, {0.0}, {NULL}	 },
	{ classtype,"Class",					{&menu_playerclass},		{2.0}, {0.0}, {0.0}, {NULL}	 },
	{ discrete, "Gender",					{&menu_gender},			{3.0}, {0.0}, {0.0}, {GenderVals} },
	{ slider,	"Red",						{&menu_color},			{0.0}, {255.0}, {1.0}, {NULL}  },
	{ slider,	"Green",					{&menu_color},			{0.0}, {255.0}, {1.0}, {NULL}  },
	{ slider,	"Blue",						{&menu_color},			{0.0}, {255.0}, {1.0}, {NULL}  },
	{ redtext,	" ",						{NULL},					{0.0}, {0.0}, {0.0}, {NULL}  },
	{ discrete,	"Always Run",				{&cl_run},				{2.0}, {0.0}, {0.0}, {OnOff} },
	{ number,	"Handicap",					{&menu_handicap},			{0.0}, {200.0}, {5.0}, {NULL} },
	{ redtext,	" ",						{NULL},					{0.0}, {0.0}, {0.0}, {NULL}  },
	{ discrete, "Railgun color",			{&menu_railcolor},		{11.0}, {0.0}, {0.0}, {TrailColorVals} },
	{ discrete,	"Autoaim",					{&menu_autoaim},			{7.0}, {0.0}, {0.0}, {AutoaimVals} },
	{ discrete,	"Unlagged",					{&cl_unlagged},			{2.0}, {0.0}, {0.0}, {OnOff} },
	{ more,		"Weapon setup",				{NULL},					{0.0}, {0.0}, {0.0}, {(value_t *)M_WeaponSetup} },
	{ redtext,	" ",						{NULL},					{0.0}, {0.0}, {0.0}, {NULL}  },
	{ discretes,"Announcer",				{&cl_announcer},			{0.0}, {0.0}, {0.0}, {NULL} },
	{ slider,	"Announcer volume",			{&snd_announcervolume},	{0.0}, {1.0},	{0.05}, {NULL} },	// [WS] Skulltag Announcer volume.
// [RC] Moved switch team to the Multiplayer menu
	{ redtext,	" ",						{NULL},					{0.0}, {0.0}, {0.0}, {NULL}  },
	{ more,		"Undo changes",				{NULL},					{0.0}, {0.0}, {0.0}, {(value_t *)M_UndoPlayerSetupChanges} },
};

// [BB] Update this define if PlayerSetupItems is altered!
#define ANNOUNCER_INDEX 17

// [BB]
void InitAnnouncersList()
{
	Announcers.Clear();
	valuestring_t value;
	value.value = -1;
	value.name = "None";
	Announcers.Push(value);
	for ( int i = 0; i < ANNOUNCER_GetNumProfiles( ); ++i )
	{
		value.value = float(i);
		value.name = ANNOUNCER_GetName( i );
		Announcers.Push(value);
	}
	// [BB] Moved crosshair selection to the HUD menu.
	PlayerSetupItems[ANNOUNCER_INDEX].b.numvalues = float(Announcers.Size());
	PlayerSetupItems[ANNOUNCER_INDEX].e.valuestrings = &Announcers[0];
}

menu_t PlayerSetupMenu = {
	"PLAYER SETUP",
	0,
	countof(PlayerSetupItems),
	0,
	PlayerSetupItems,
	0,
	0,
	0,
	M_PlayerSetupDrawer,
	false,
	M_AcceptPlayerSetupChanges,
	MNF_ALIGNLEFT,
};

//
// Read This! MENU 1 & 2
//
static oldmenuitem_t ReadMenu[] =
{
	{1,0,0,NULL,false,false,M_ReadThisMore}
};

static oldmenu_t ReadDef =
{
	1,
	ReadMenu,
	M_DrawReadThis,
	280,185,
	0
};

//
// LOAD GAME MENU
//
static oldmenuitem_t LoadMenu[]=
{
	{1,0,'1',NULL, false, false, NULL, CR_UNTRANSLATED},
	{1,0,'2',NULL, false, false, NULL, CR_UNTRANSLATED},
	{1,0,'3',NULL, false, false, NULL, CR_UNTRANSLATED},
	{1,0,'4',NULL, false, false, NULL, CR_UNTRANSLATED},
	{1,0,'5',NULL, false, false, NULL, CR_UNTRANSLATED},
	{1,0,'6',NULL, false, false, NULL, CR_UNTRANSLATED},
	{1,0,'7',NULL, false, false, NULL, CR_UNTRANSLATED},
	{1,0,'8',NULL, false, false, NULL, CR_UNTRANSLATED},
};

static oldmenu_t LoadDef =
{
	countof(LoadMenu),
	LoadMenu,
	M_DrawLoad,
	80,54,
	0
};

//
// SAVE GAME MENU
//
static oldmenuitem_t SaveMenu[] =
{
	{1,0,'1',NULL, false, false, NULL, CR_UNTRANSLATED},
	{1,0,'2',NULL, false, false, NULL, CR_UNTRANSLATED},
	{1,0,'3',NULL, false, false, NULL, CR_UNTRANSLATED},
	{1,0,'4',NULL, false, false, NULL, CR_UNTRANSLATED},
	{1,0,'5',NULL, false, false, NULL, CR_UNTRANSLATED},
	{1,0,'6',NULL, false, false, NULL, CR_UNTRANSLATED},
	{1,0,'7',NULL, false, false, NULL, CR_UNTRANSLATED},
	{1,0,'8',NULL, false, false, NULL, CR_UNTRANSLATED},
};

static oldmenu_t SaveDef =
{
	countof(SaveMenu),
	SaveMenu,
	M_DrawSave,
	80,54,
	0
};

// CODE --------------------------------------------------------------------

// [RH] Most menus can now be accessed directly
// through console commands.
CCMD (menu_main)
{
	M_StartControlPanel (true, true);
}

CCMD (menu_load)
{	// F3
	M_StartControlPanel (true);
	M_LoadGame (0);
}

CCMD (menu_save)
{	// F2
	M_StartControlPanel (true);
	M_SaveGame (0);
}

CCMD (menu_help)
{	// F1
	M_StartControlPanel (true);
	M_ReadThis (0);
}

void ClassSelHelper()
{
	ClassMenuDef.lastOn = ClassMenuDef.numitems - 1;
	if (players[consoleplayer].userinfo.PlayerClass >= 0)
	{
		int n = 0;
		for (int i = 0; i < (int)PlayerClasses.Size () && n < 7; i++)
		{
			if (!(PlayerClasses[i].Flags & PCF_NOMENU))
			{
				if (i == players[consoleplayer].userinfo.PlayerClass)
				{
					ClassMenuDef.lastOn = n;
					break;
				}
				n++;
			}
		}
	}

	PickPlayerClass ();

	PlayerState = GetDefaultByType (PlayerClass->Type)->SeeState;
	PlayerTics = PlayerState->GetTics();

	if (FireTexture == NULL)
	{
		FireTexture = new FBackdropTexture;
	}
	M_SetupNextMenu (&ClassMenuDef);
}

CCMD (menu_class)
{	// F1
	if (ClassMenuDef.numitems > 1)
	{
		M_StartControlPanel (true);
		ClassSelHelper();
	}
}

CCMD (quicksave)
{	// F6
	// [BB] The server can't do this.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		return;

	//M_StartControlPanel (true);
	S_Sound (CHAN_VOICE | CHAN_UI, "menu/activate", 1, ATTN_NONE);
	M_QuickSave();
}

CCMD (quickload)
{	// F9
	// [BB] The server can't do this.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		return;

	//M_StartControlPanel (true);
	S_Sound (CHAN_VOICE | CHAN_UI, "menu/activate", 1, ATTN_NONE);
	M_QuickLoad();
}

CCMD (menu_endgame)
{	// F7
	//M_StartControlPanel (true);
	S_Sound (CHAN_VOICE | CHAN_UI, "menu/activate", 1, ATTN_NONE);
	M_EndGame(0);
}

CCMD (menu_quit)
{	// F10
	//M_StartControlPanel (true);
	S_Sound (CHAN_VOICE | CHAN_UI, "menu/activate", 1, ATTN_NONE);
	M_QuitGame(0);
}

CCMD (menu_game)
{
	M_StartControlPanel (true);
	M_NewGame(0);
}
								
CCMD (menu_options)
{
	M_StartControlPanel (true);
	M_Options(0);
}

CCMD (menu_player)
{
	M_StartControlPanel (true);
	// [BB] Due to the differences in Skulltag's player menu, we have to set OptionsActive to true here.
	OptionsActive = true;
	M_PlayerSetup ();
}

CCMD (bumpgamma)
{
	// [RH] Gamma correction tables are now generated
	// on the fly for *any* gamma level.
	// Q: What are reasonable limits to use here?

	float newgamma = Gamma + 0.1f;

	if (newgamma > 3.0)
		newgamma = 1.0;

	Gamma = newgamma;
	Printf ("Gamma correction level %g\n", *Gamma);
}

void M_ActivateMenuInput ()
{
	ResetButtonStates ();
	menuactive = MENU_On;
	// Pause sound effects before we play the menu switch sound.
	// That way, it won't be paused.
	P_CheckTickerPaused ();
}

void M_DeactivateMenuInput ()
{
	menuactive = MENU_Off;
}

void M_DrawFiles ()
{
}

void M_GameFiles (int choice)
{
	M_SetupNextMenu (&FilesMenu);
}

//
// M_ReadSaveStrings
//
// Find savegames and read their titles
//
static void M_ReadSaveStrings ()
{
	if (SaveGames.IsEmpty ())
	{
		void *filefirst;
		findstate_t c_file;
		FString filter;

		atterm (M_UnloadSaveStrings);

		filter = G_BuildSaveName ("*.zds", -1);
		filefirst = I_FindFirst (filter.GetChars(), &c_file);
		if (filefirst != ((void *)(-1)))
		{
			do
			{
				// I_FindName only returns the file's name and not its full path
				FString filepath = G_BuildSaveName (I_FindName(&c_file), -1);
				FILE *file = fopen (filepath, "rb");

				if (file != NULL)
				{
					PNGHandle *png;
					char sig[16];
					char title[SAVESTRINGSIZE+1];
					bool oldVer = true;
					bool addIt = false;
					bool missing = false;

					// ZDoom 1.23 betas 21-33 have the savesig first.
					// Earlier versions have the savesig second.
					// Later versions have the savegame encapsulated inside a PNG.
					//
					// Old savegame versions are always added to the menu so
					// the user can easily delete them if desired.

					title[SAVESTRINGSIZE] = 0;

					if (NULL != (png = M_VerifyPNG (file)))
					{
						char *ver = M_GetPNGText (png, "ZDoom Save Version");
						char *engine = M_GetPNGText (png, "Engine");
						if (ver != NULL)
						{
							if (!M_GetPNGText (png, "Title", title, SAVESTRINGSIZE))
							{
								strncpy (title, I_FindName(&c_file), SAVESTRINGSIZE);
							}
							if (strncmp (ver, SAVESIG, 9) == 0 &&
								atoi (ver+9) >= MINSAVEVER &&
								engine != NULL)
							{
								// Was saved with a compatible ZDoom version,
								// so check if it's for the current game.
								// If it is, add it. Otherwise, ignore it.
								char *iwad = M_GetPNGText (png, "Game WAD");
								if (iwad != NULL)
								{
									if (stricmp (iwad, Wads.GetWadName (FWadCollection::IWAD_FILENUM)) == 0)
									{
										addIt = true;
										oldVer = false;
										missing = !G_CheckSaveGameWads (png, false);
									}
									delete[] iwad;
								}
							}
							else
							{ // An old version
								addIt = true;
							}
							delete[] ver;
						}
						if (engine != NULL)
						{
							delete[] engine;
						}
						delete png;
					}
					else
					{
						fseek (file, 0, SEEK_SET);
						if (fread (sig, 1, 16, file) == 16)
						{

							if (strncmp (sig, "ZDOOMSAVE", 9) == 0)
							{
								if (fread (title, 1, SAVESTRINGSIZE, file) == SAVESTRINGSIZE)
								{
									addIt = true;
								}
							}
							else
							{
								memcpy (title, sig, 16);
								if (fread (title + 16, 1, SAVESTRINGSIZE-16, file) == SAVESTRINGSIZE-16 &&
									fread (sig, 1, 16, file) == 16 &&
									strncmp (sig, "ZDOOMSAVE", 9) == 0)
								{
									addIt = true;
								}
							}
						}
					}

					if (addIt)
					{
						FSaveGameNode *node = new FSaveGameNode;
						node->Filename = filepath;
						node->bOldVersion = oldVer;
						node->bMissingWads = missing;
						memcpy (node->Title, title, SAVESTRINGSIZE);
						M_InsertSaveNode (node);
					}
					fclose (file);
				}
			} while (I_FindNext (filefirst, &c_file) == 0);
			I_FindClose (filefirst);
		}
	}
	if (SelSaveGame == NULL || SelSaveGame->Succ == NULL)
	{
		SelSaveGame = static_cast<FSaveGameNode *>(SaveGames.Head);
	}
}

static void M_UnloadSaveStrings()
{
	M_UnloadSaveData();
	while (!SaveGames.IsEmpty())
	{
		M_RemoveSaveSlot (static_cast<FSaveGameNode *>(SaveGames.Head));
	}
}

static FSaveGameNode *M_RemoveSaveSlot (FSaveGameNode *file)
{
	FSaveGameNode *next = static_cast<FSaveGameNode *>(file->Succ);

	if (file == TopSaveGame)
	{
		TopSaveGame = next;
	}
	if (quickSaveSlot == file)
	{
		quickSaveSlot = NULL;
	}
	if (lastSaveSlot == file)
	{
		lastSaveSlot = NULL;
	}
	file->Remove ();
	delete file;
	return next;
}

void M_InsertSaveNode (FSaveGameNode *node)
{
	FSaveGameNode *probe;

	if (SaveGames.IsEmpty ())
	{
		SaveGames.AddHead (node);
		return;
	}

	if (node->bOldVersion)
	{ // Add node at bottom of list
		probe = static_cast<FSaveGameNode *>(SaveGames.TailPred);
		while (probe->Pred != NULL && probe->bOldVersion &&
			stricmp (node->Title, probe->Title) < 0)
		{
			probe = static_cast<FSaveGameNode *>(probe->Pred);
		}
		node->Insert (probe);
	}
	else
	{ // Add node at top of list
		probe = static_cast<FSaveGameNode *>(SaveGames.Head);
		while (probe->Succ != NULL && !probe->bOldVersion &&
			stricmp (node->Title, probe->Title) > 0)
		{
			probe = static_cast<FSaveGameNode *>(probe->Succ);
		}
		node->InsertBefore (probe);
	}
}

void M_NotifyNewSave (const char *file, const char *title, bool okForQuicksave)
{
	FSaveGameNode *node;

	if (file == NULL)
		return;

	M_ReadSaveStrings ();

	// See if the file is already in our list
	for (node = static_cast<FSaveGameNode *>(SaveGames.Head);
		 node->Succ != NULL;
		 node = static_cast<FSaveGameNode *>(node->Succ))
	{
#ifdef unix
		if (node->Filename.Compare (file) == 0)
#else
		if (node->Filename.CompareNoCase (file) == 0)
#endif
		{
			strcpy (node->Title, title);
			node->bOldVersion = false;
			node->bMissingWads = false;
			break;
		}
	}

	if (node->Succ == NULL)
	{
		node = new FSaveGameNode;
		strcpy (node->Title, title);
		node->Filename = file;
		node->bOldVersion = false;
		node->bMissingWads = false;
		M_InsertSaveNode (node);
		SelSaveGame = node;
	}

	if (okForQuicksave)
	{
		if (quickSaveSlot == NULL) quickSaveSlot = node;
		lastSaveSlot = node;
	}
}

//
// M_LoadGame & Cie.
//
void M_DrawLoad (void)
{
	if (gameinfo.gametype & (GAME_DoomStrifeChex))
	{
		FTexture *title = TexMan["M_LOADG"];
		screen->DrawTexture (title,
			(SCREENWIDTH - title->GetScaledWidth()*CleanXfac)/2, 20*CleanYfac,
			DTA_CleanNoMove, true, TAG_DONE);
	}
	else
	{
		const char *loadgame = GStrings("MNU_LOADGAME");
		screen->DrawText (BigFont, CR_UNTRANSLATED,
			(SCREENWIDTH - BigFont->StringWidth (loadgame)*CleanXfac)/2, 10*CleanYfac,
			loadgame, DTA_CleanNoMove, true, TAG_DONE);
	}
	M_DrawSaveLoadCommon ();
}



//
// Draw border for the savegame description
// [RH] Width of the border is variable
//
void M_DrawSaveLoadBorder (int x, int y, int len)
{
	if (gameinfo.gametype & (GAME_DoomStrifeChex))
	{
		int i;

		screen->DrawTexture (TexMan["M_LSLEFT"], x-8, y+7, DTA_Clean, true, TAG_DONE);

		for (i = 0; i < len; i++)
		{
			screen->DrawTexture (TexMan["M_LSCNTR"], x, y+7, DTA_Clean, true, TAG_DONE);
			x += 8;
		}

		screen->DrawTexture (TexMan["M_LSRGHT"], x, y+7, DTA_Clean, true, TAG_DONE);
	}
	else
	{
		screen->DrawTexture (TexMan["M_FSLOT"], x, y+1, DTA_Clean, true, TAG_DONE);
	}
}

static void M_ExtractSaveData (const FSaveGameNode *node)
{
	FILE *file;
	PNGHandle *png;

	M_UnloadSaveData ();

	if (node != NULL &&
		node->Succ != NULL &&
		!node->Filename.IsEmpty() &&
		!node->bOldVersion &&
		(file = fopen (node->Filename.GetChars(), "rb")) != NULL)
	{
		if (NULL != (png = M_VerifyPNG (file)))
		{
			char *time, *pcomment, *comment;
			size_t commentlen, totallen, timelen;

			// Extract comment
			time = M_GetPNGText (png, "Creation Time");
			pcomment = M_GetPNGText (png, "Comment");
			if (pcomment != NULL)
			{
				commentlen = strlen (pcomment);
			}
			else
			{
				commentlen = 0;
			}
			if (time != NULL)
			{
				timelen = strlen (time);
				totallen = timelen + commentlen + 3;
			}
			else
			{
				timelen = 0;
				totallen = commentlen + 1;
			}
			if (totallen != 0)
			{
				comment = new char[totallen];

				if (timelen)
				{
					memcpy (comment, time, timelen);
					comment[timelen] = '\n';
					comment[timelen+1] = '\n';
					timelen += 2;
				}
				if (commentlen)
				{
					memcpy (comment + timelen, pcomment, commentlen);
				}
				comment[timelen+commentlen] = 0;
				SaveComment = V_BreakLines (SmallFont, 216*screen->GetWidth()/640/CleanXfac, comment);
				delete[] comment;
				delete[] time;
				delete[] pcomment;
			}

			// Extract pic
			SavePic = PNGTexture_CreateFromFile(png, node->Filename);

			delete png;
		}
		fclose (file);
	}
}

static void M_UnloadSaveData ()
{
	if (SavePic != NULL)
	{
		delete SavePic;
	}
	if (SaveComment != NULL)
	{
		V_FreeBrokenLines (SaveComment);
	}

	SavePic = NULL;
	SaveComment = NULL;
}

static void M_DrawSaveLoadCommon ()
{
	const int savepicLeft = 10;
	const int savepicTop = 54*CleanYfac;
	const int savepicWidth = 216*screen->GetWidth()/640;
	const int savepicHeight = 135*screen->GetHeight()/400;

	const int rowHeight = (SmallFont->GetHeight() + 1) * CleanYfac;
	const int listboxLeft = savepicLeft + savepicWidth + 14;
	const int listboxTop = savepicTop;
	const int listboxWidth = screen->GetWidth() - listboxLeft - 10;
	const int listboxHeight1 = screen->GetHeight() - listboxTop - 10;
	const int listboxRows = (listboxHeight1 - 1) / rowHeight;
	const int listboxHeight = listboxRows * rowHeight + 1;
	const int listboxRight = listboxLeft + listboxWidth;
	const int listboxBottom = listboxTop + listboxHeight;

	const int commentLeft = savepicLeft;
	const int commentTop = savepicTop + savepicHeight + 16;
	const int commentWidth = savepicWidth;
	const int commentHeight = (51+(screen->GetHeight()>200?10:0))*CleanYfac;
	const int commentRight = commentLeft + commentWidth;
	const int commentBottom = commentTop + commentHeight;

	FSaveGameNode *node;
	int i;
	bool didSeeSelected = false;

	// Draw picture area
	if (gameaction == ga_loadgame || gameaction == ga_savegame)
	{
		return;
	}

	M_DrawFrame (savepicLeft, savepicTop, savepicWidth, savepicHeight);
	if (SavePic != NULL)
	{
		screen->DrawTexture(SavePic, savepicLeft, savepicTop,
			DTA_DestWidth, savepicWidth,
			DTA_DestHeight, savepicHeight,
			DTA_Masked, false,
			TAG_DONE);
	}
	else
	{
		screen->Clear (savepicLeft, savepicTop,
			savepicLeft+savepicWidth, savepicTop+savepicHeight, 0, 0);

		if (!SaveGames.IsEmpty ())
		{
			const char *text =
				(SelSaveGame == NULL || !SelSaveGame->bOldVersion)
				? GStrings("MNU_NOPICTURE") : GStrings("MNU_DIFFVERSION");
			const int textlen = SmallFont->StringWidth (text)*CleanXfac;

			screen->DrawText (SmallFont, CR_GOLD, savepicLeft+(savepicWidth-textlen)/2,
				savepicTop+(savepicHeight-rowHeight)/2, text,
				DTA_CleanNoMove, true, TAG_DONE);
		}
	}

	// Draw comment area
	M_DrawFrame (commentLeft, commentTop, commentWidth, commentHeight);
	screen->Clear (commentLeft, commentTop, commentRight, commentBottom, 0, 0);
	if (SaveComment != NULL)
	{
		// I'm not sure why SaveComment would go NULL in this loop, but I got
		// a crash report where it was NULL when i reached 1, so now I check
		// for that.
		for (i = 0; SaveComment != NULL && SaveComment[i].Width >= 0 && i < 6; ++i)
		{
			screen->DrawText (SmallFont, CR_GOLD, commentLeft, commentTop
				+ SmallFont->GetHeight()*i*CleanYfac, SaveComment[i].Text,
				DTA_CleanNoMove, true, TAG_DONE);
		}
	}

	// Draw file area
	do
	{
		M_DrawFrame (listboxLeft, listboxTop, listboxWidth, listboxHeight);
		screen->Clear (listboxLeft, listboxTop, listboxRight, listboxBottom, 0, 0);

		if (SaveGames.IsEmpty ())
		{
			const char * text = GStrings("MNU_NOFILES");
			const int textlen = SmallFont->StringWidth (text)*CleanXfac;

			screen->DrawText (SmallFont, CR_GOLD, listboxLeft+(listboxWidth-textlen)/2,
				listboxTop+(listboxHeight-rowHeight)/2, text,
				DTA_CleanNoMove, true, TAG_DONE);
			return;
		}

		for (i = 0, node = TopSaveGame;
			 i < listboxRows && node->Succ != NULL;
			 ++i, node = static_cast<FSaveGameNode *>(node->Succ))
		{
			int color;
			if (node->bOldVersion)
			{
				color = CR_BLUE;
			}
			else if (node->bMissingWads)
			{
				color = CR_ORANGE;
			}
			else if (node == SelSaveGame)
			{
				color = CR_WHITE;
			}
			else
			{
				color = CR_TAN;
			}
			if (node == SelSaveGame)
			{
				screen->Clear (listboxLeft, listboxTop+rowHeight*i,
					listboxRight, listboxTop+rowHeight*(i+1), -1,
					genStringEnter ? MAKEARGB(255,255,0,0) : MAKEARGB(255,0,0,255));
				didSeeSelected = true;
				if (!genStringEnter)
				{
					screen->DrawText (SmallFont, color,
						listboxLeft+1, listboxTop+rowHeight*i+CleanYfac, node->Title,
						DTA_CleanNoMove, true, TAG_DONE);
				}
				else
				{
					screen->DrawText (SmallFont, CR_WHITE,
						listboxLeft+1, listboxTop+rowHeight*i+CleanYfac, savegamestring,
						DTA_CleanNoMove, true, TAG_DONE);
					screen->DrawText (SmallFont, CR_WHITE,
						// [BB] Using Skulltag's increased precision of CleanXfac is wrong here.
						listboxLeft+1+SmallFont->StringWidth (savegamestring)*static_cast<int>(CleanXfac),
						listboxTop+rowHeight*i+CleanYfac, underscore,
						DTA_CleanNoMove, true, TAG_DONE);
				}
			}
			else
			{
				screen->DrawText (SmallFont, color,
					listboxLeft+1, listboxTop+rowHeight*i+CleanYfac, node->Title,
					DTA_CleanNoMove, true, TAG_DONE);
			}
		}

		// This is dumb: If the selected node was not visible,
		// scroll down and redraw. M_SaveLoadResponder()
		// guarantees that if the node is not visible, it will
		// always be below the visible list instead of above it.
		// This should not really be done here, but I don't care.

		if (!didSeeSelected)
		{
			for (i = 1; node->Succ != NULL && node != SelSaveGame; ++i)
			{
				node = static_cast<FSaveGameNode *>(node->Succ);
			}
			if (node->Succ == NULL)
			{ // SelSaveGame is invalid
				didSeeSelected = true;
			}
			else
			{
				do
				{
					TopSaveGame = static_cast<FSaveGameNode *>(TopSaveGame->Succ);
				} while (--i);
			}
		}
	} while (!didSeeSelected);
}

// Draw a frame around the specified area using the view border
// frame graphics. The border is drawn outside the area, not in it.
void M_DrawFrame (int left, int top, int width, int height)
{
	FTexture *p;
	const gameborder_t *border = gameinfo.border;
	int offset = border->offset;
	int right = left + width;
	int bottom = top + height;

	// Draw top and bottom sides.
	p = TexMan[border->t];
	screen->FlatFill(left, top - p->GetHeight(), right, top, p, true);
	p = TexMan[border->b];
	screen->FlatFill(left, bottom, right, bottom + p->GetHeight(), p, true);

	// Draw left and right sides.
	p = TexMan[border->l];
	screen->FlatFill(left - p->GetWidth(), top, left, bottom, p, true);
	p = TexMan[border->r];
	screen->FlatFill(right, top, right + p->GetWidth(), bottom, p, true);

	// Draw beveled corners.
	screen->DrawTexture (TexMan[border->tl], left-offset, top-offset, TAG_DONE);
	screen->DrawTexture (TexMan[border->tr], left+width, top-offset, TAG_DONE);
	screen->DrawTexture (TexMan[border->bl], left-offset, top+height, TAG_DONE);
	screen->DrawTexture (TexMan[border->br], left+width, top+height, TAG_DONE);
}

//
// Selected from DOOM menu
//
void M_LoadGame (int choice)
{
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
	{
		if(gameinfo.gametype == GAME_Chex)
			M_StartMessage (GStrings("CLOADNET"), NULL);
		else
			M_StartMessage (GStrings("LOADNET"), NULL);
		return;
	}
		
	M_SetupNextMenu (&LoadDef);
	drawSkull = false;
	M_ReadSaveStrings ();
	TopSaveGame = static_cast<FSaveGameNode *>(SaveGames.Head);
	M_ExtractSaveData (SelSaveGame);
}


//
//	M_SaveGame & Cie.
//
void M_DrawSave()
{
	if (gameinfo.gametype & (GAME_DoomStrifeChex))
	{
		FTexture *title = TexMan["M_SAVEG"];
		screen->DrawTexture (title,
			(SCREENWIDTH-title->GetScaledWidth()*CleanXfac)/2, 20*CleanYfac,
			DTA_CleanNoMove, true, TAG_DONE);
	}
	else
	{
		const char *text = GStrings("MNU_SAVEGAME");
		screen->DrawText (BigFont, CR_UNTRANSLATED,
			(SCREENWIDTH - BigFont->StringWidth (text)*CleanXfac)/2, 10*CleanYfac,
			text, DTA_CleanNoMove, true, TAG_DONE);
	}
	M_DrawSaveLoadCommon ();
}

//
// M_Responder calls this when the user is finished
//
void M_DoSave (FSaveGameNode *node)
{
	if (node != &NewSaveNode)
	{
		G_SaveGame (node->Filename.GetChars(), savegamestring);
	}
	else
	{
		// Find an unused filename and save as that
		FString filename;
		int i;
		FILE *test;

		for (i = 0;; ++i)
		{
			filename = G_BuildSaveName ("save", i);
			test = fopen (filename, "rb");
			if (test == NULL)
			{
				break;
			}
			fclose (test);
		}
		G_SaveGame (filename, savegamestring);
	}
	M_ClearMenus ();
	BorderNeedRefresh = screen->GetPageCount ();
}

//
// Selected from DOOM menu
//
void M_SaveGame (int choice)
{
	if (!usergame || (players[consoleplayer].health <= 0 && NETWORK_GetState( ) == NETSTATE_SINGLE ))
	{
		M_StartMessage (GStrings("SAVEDEAD"), NULL);
		return;
	}

	// [BB] No saving in multiplayer.
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
	{
		M_StartMessage (GStrings("SAVENET"), NULL);
		return;
	}

	// [BB] Saving bots is not supported yet.
	if ( BOTS_CountBots() > 0 )
	{
		M_StartMessage ("You cannot save the game\nwhile bots are in use.", NULL);
		return;
	}

	if (gamestate != GS_LEVEL)
		return;

	M_SetupNextMenu(&SaveDef);
	drawSkull = false;

	M_ReadSaveStrings();
	SaveGames.AddHead (&NewSaveNode);
	TopSaveGame = static_cast<FSaveGameNode *>(SaveGames.Head);
	if (lastSaveSlot == NULL)
	{
		SelSaveGame = &NewSaveNode;
	}
	else
	{
		SelSaveGame = lastSaveSlot;
	}
	M_ExtractSaveData (SelSaveGame);
}



//
//		M_QuickSave
//
void M_QuickSaveResponse (int ch)
{
	if (ch == 'y')
	{
		M_DoSave (quickSaveSlot);
		S_Sound (CHAN_VOICE | CHAN_UI, "menu/dismiss", 1, ATTN_NONE);
	}
}

void M_QuickSave ()
{
	// [BB] Saving bots is not supported yet. Also no saving in multiplayer.
	if (!usergame || (players[consoleplayer].health <= 0 && NETWORK_GetState( ) == NETSTATE_SINGLE ) || ( BOTS_CountBots() > 0 ) || ( NETWORK_GetState( ) == NETSTATE_CLIENT ) )
	{
		S_Sound (CHAN_VOICE | CHAN_UI, "menu/invalid", 1, ATTN_NONE);
		return;
	}

	if (gamestate != GS_LEVEL)
		return;
		
	if (quickSaveSlot == NULL)
	{
		M_StartControlPanel(false);
		M_SaveGame (0);
		return;
	}
	if(gameinfo.gametype == GAME_Chex)
		mysnprintf (tempstring, countof(tempstring), GStrings("CQSPROMPT"), quickSaveSlot->Title);
	else
		mysnprintf (tempstring, countof(tempstring), GStrings("QSPROMPT"), quickSaveSlot->Title);
	strcpy (savegamestring, quickSaveSlot->Title);
	M_StartMessage (tempstring, M_QuickSaveResponse);
}



//
// M_QuickLoad
//
void M_QuickLoadResponse (int ch)
{
	if (ch == 'y')
	{
		M_LoadSelect (quickSaveSlot);
		S_Sound (CHAN_VOICE | CHAN_UI, "menu/dismiss", 1, ATTN_NONE);
	}
}


void M_QuickLoad ()
{
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
	{
		if(gameinfo.gametype == GAME_Chex)
			M_StartMessage (GStrings("CQLOADNET"), NULL);
		else
			M_StartMessage (GStrings("QLOADNET"), NULL);
		return;
	}
		
	if (quickSaveSlot == NULL)
	{
		M_StartControlPanel(false);
		// signal that whatever gets loaded should be the new quicksave
		quickSaveSlot = (FSaveGameNode *)1;
		M_LoadGame (0);
		return;
	}
	if(gameinfo.gametype == GAME_Chex)
		mysnprintf (tempstring, countof(tempstring), GStrings("CQLPROMPT"), quickSaveSlot->Title);
	else
		mysnprintf (tempstring, countof(tempstring), GStrings("QLPROMPT"), quickSaveSlot->Title);
	M_StartMessage (tempstring, M_QuickLoadResponse);
}

//
// Read This Menus
//
void M_DrawReadThis ()
{
	FTexture *tex = NULL, *prevpic = NULL;
	fixed_t alpha;

	// [BB] Is there a built in texture for this gamemode?
	if ( ( NETWORK_GetState( ) != NETSTATE_SINGLE ) && TexMan.CheckForTexture( GAMEMODE_GetF1Texture( GAMEMODE_GetCurrentMode( )), 0, 0 ).Exists() )
		tex = TexMan[GAMEMODE_GetF1Texture( GAMEMODE_GetCurrentMode( ))];

	// Did the mapper choose a custom help page via MAPINFO?
	if ((level.info != NULL) && level.info->f1[0] != 0)
	{
		tex = TexMan.FindTexture(level.info->f1);
		InfoType = 1;
	}
	
	if (tex == NULL)
	{
		tex = TexMan[gameinfo.infoPages[InfoType-1].GetChars()];
	}

	if (InfoType > 1)
	{
		prevpic = TexMan[gameinfo.infoPages[InfoType-2].GetChars()];
	}

	alpha = MIN<fixed_t> (Scale (gametic - InfoTic, OPAQUE, TICRATE/3), OPAQUE);
	if (alpha < OPAQUE && prevpic != NULL)
	{
		screen->DrawTexture (prevpic, 0, 0,
			DTA_DestWidth, screen->GetWidth(),
			DTA_DestHeight, screen->GetHeight(),
			TAG_DONE);
	}
	screen->DrawTexture (tex, 0, 0,
		DTA_DestWidth, screen->GetWidth(),
		DTA_DestHeight, screen->GetHeight(),
		DTA_Alpha, alpha,
		TAG_DONE);
}

//
// M_DrawMainMenu
//
void M_DrawMainMenu (void)
{
	if (gameinfo.gametype & GAME_DoomChex)
	{
        screen->DrawTexture (TexMan["M_DOOM"], 94, 2, DTA_Clean, true, TAG_DONE);
	}
	else
	{
        screen->DrawTexture (TexMan["M_STRIFE"], 84, 2, DTA_Clean, true, TAG_DONE);
	}
}

void M_DrawHereticMainMenu ()
{
	char name[9];

	screen->DrawTexture (TexMan["M_HTIC"], 88, 0, DTA_Clean, true, TAG_DONE);

	if (gameinfo.gametype == GAME_Hexen)
	{
		int frame = (MenuTime / 5) % 7;

		mysnprintf (name, countof(name), "FBUL%c0", (frame+2)%7 + 'A');
		screen->DrawTexture (TexMan[name], 37, 80, DTA_Clean, true, TAG_DONE);

		mysnprintf (name, countof(name), "FBUL%c0", frame + 'A');
		screen->DrawTexture (TexMan[name], 278, 80, DTA_Clean, true, TAG_DONE);
	}
	else
	{
		int frame = (MenuTime / 3) % 18;

		mysnprintf (name, countof(name), "M_SKL%.2d", 17 - frame);
		screen->DrawTexture (TexMan[name], 40, 10, DTA_Clean, true, TAG_DONE);

		mysnprintf (name, countof(name), "M_SKL%.2d", frame);
		screen->DrawTexture (TexMan[name], 232, 10, DTA_Clean, true, TAG_DONE);
	}
}

//
// M_DrawNewGame
//
void M_DrawNewGame(void)
{
	if (gameinfo.gametype & (GAME_DoomStrifeChex))
	{
		screen->DrawTexture (TexMan[gameinfo.gametype & GAME_DoomChex ? "M_NEWG" : "M_NGAME"], 96, 14, DTA_Clean, true, TAG_DONE);
		screen->DrawTexture (TexMan["M_SKILL"], 54, 38, DTA_Clean, true, TAG_DONE);
	}
}

// [BC] Draw the header for the bot skill menu.
void M_DrawBotSkill( void )
{
	if ( EpisodeMenu[epi].bBotSkillFullText )
	{
		screen->DrawText( BigFont, CR_RED, 160 - ( BigFont->StringWidth( EpisodeSkillHeaders[epi] ) / 2 ), 14, EpisodeSkillHeaders[epi], DTA_Clean, true, TAG_DONE );
	}
	else
		screen->DrawTexture( TexMan[EpisodeSkillHeaders[epi]], 160 - ( TexMan[EpisodeSkillHeaders[epi]]->GetScaledWidth( ) / 2 ), 14, DTA_Clean, true, TAG_DONE );

	screen->DrawTexture( TexMan["M_BSKILL"], 54, 38, DTA_Clean, true, TAG_DONE );
}

void M_NewGame(int choice)
{
/*
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) && !demoplayback)
	{
		if(gameinfo.gametype == GAME_Chex)
			M_StartMessage (GStrings("CNEWGAME"), NULL);
		else
			M_StartMessage (GStrings("NEWGAME"), NULL);
		return;
	}
*/
	// [BB] The class selection screen for a new game only works
	// if we are not in client mode. So just tell the user to disconnect first.
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT && PlayerClasses.Size() > 1 )
	{
		M_StartMessage ("Please disconnect before\nstarting a new game.", NULL);
		return;
	}

	// Set up episode menu positioning
	if (gameinfo.gametype & (GAME_DoomStrifeChex))
	{
		EpiDef.x = 48;
		EpiDef.y = 63;
	}
	else
	{
		EpiDef.x = 80;
		EpiDef.y = 50;
	}
	if (EpiDef.numitems > 4)
	{
		EpiDef.y -= LINEHEIGHT;
	}
	epi = 0;

	if (gameinfo.gametype == GAME_Hexen && ClassMenuDef.numitems == 0)
	{ // [RH] Make the default entry the last class the player used.
		ClassMenu.lastOn = players[consoleplayer].userinfo.PlayerClass;
		if (ClassMenu.lastOn < 0)
		{
			ClassMenu.lastOn = 3;
		}
		M_SetupNextMenu (&ClassMenu);
	}
	// [GRB] Class select
	else if (ClassMenuDef.numitems > 1)
	{
		// [BB] To prevent code duplication with CCMD (menu_class)
		// I moved the code from here into a helper function.
		ClassSelHelper();
	}
	else if (EpiDef.numitems <= 1)
	{
		if (AllSkills.Size() == 1)
		{
			M_ChooseSkill(0);
		}
		else if (EpisodeNoSkill[0])
		{
			M_ChooseSkill(AllSkills.Size() == 2? 1:2);
		}
		else
		{
			if (( EpiDef.numitems == 1 ) && ( EpisodeMenu[0].bBotSkill ))
				M_SetupNextMenu (&BotDef);
			else
				M_StartupSkillMenu(NULL);
		}
	}
	else
	{
		M_SetupNextMenu (&EpiDef);
	}
}

void M_Multiplayer (int choice)
{
	OptionsActive = M_StartMultiplayerMenu ();
}

//==========================================================================
//
// DrawClassMenu
//
//==========================================================================

static void DrawClassMenu(void)
{
	char name[9];
	int classnum;

	static const char boxLumpName[3][7] =
	{
		"M_FBOX",
		"M_CBOX",
		"M_MBOX"
	};
	static const char walkLumpName[3][10] =
	{
		"M_FWALK%d",
		"M_CWALK%d",
		"M_MWALK%d"
	};

	const char *text = GStrings("MNU_CHOOSECLASS");
	screen->DrawText (BigFont, CR_UNTRANSLATED, 34, 24, text, DTA_Clean, true, TAG_DONE);
	classnum = itemOn;
	if (classnum > 2)
	{
		classnum = (MenuTime>>2) % 3;
	}
	screen->DrawTexture (TexMan[boxLumpName[classnum]], 174, 8, DTA_Clean, true, TAG_DONE);

	mysnprintf (name, countof(name), walkLumpName[classnum], ((MenuTime >> 3) & 3) + 1);
	screen->DrawTexture (TexMan[name], 174+24, 8+12, DTA_Clean, true, TAG_DONE);
}

// [GRB] Class select drawer
static void M_DrawClassMenu ()
{
	int tit_y = 15;
	const char * text = GStrings("MNU_CHOOSECLASS");
	
	if (ClassMenuDef.numitems > 4 && gameinfo.gametype & GAME_Raven)
		tit_y = 2;
	
	screen->DrawText (BigFont, gameinfo.gametype & GAME_DoomChex ? CR_RED : CR_UNTRANSLATED,
		160 - BigFont->StringWidth (text)/2,
		tit_y,
		text, DTA_Clean, true, TAG_DONE);

	int x = (200-160)*CleanXfac+(SCREENWIDTH>>1);
	int y = (ClassMenuDef.y-100)*CleanYfac+(SCREENHEIGHT>>1);

	if (!FireTexture)
	{
		screen->Clear (x, y, x + 72 * CleanXfac, y + 80 * CleanYfac-1, 0, 0);
	}
	else
	{
		screen->DrawTexture (FireTexture, x, y - 1,
			DTA_DestWidth, static_cast<int> (72 * CleanXfac),
			DTA_DestHeight, static_cast<int> (80 * CleanYfac),
			DTA_Translation, &FireRemap,
			DTA_Masked, true,
			TAG_DONE);
	}

	M_DrawFrame (x, y, 72*CleanXfac, 80*CleanYfac-1);

	spriteframe_t *sprframe = &SpriteFrames[sprites[PlayerState->sprite].spriteframes + PlayerState->GetFrame()];
	fixed_t scaleX = GetDefaultByType (PlayerClass->Type)->scaleX;
	fixed_t scaleY = GetDefaultByType (PlayerClass->Type)->scaleY;

	if (sprframe != NULL)
	{
		FTexture *tex = TexMan(sprframe->Texture[0]);
		if (tex != NULL && tex->UseType != FTexture::TEX_Null)
		{
			screen->DrawTexture (tex,
				x + 36*CleanXfac, y + 71*CleanYfac,
				DTA_DestWidth, MulScale16 (tex->GetWidth() * CleanXfac, scaleX),
				DTA_DestHeight, MulScale16 (tex->GetHeight() * CleanYfac, scaleY),
				TAG_DONE);
		}
	}
}

//---------------------------------------------------------------------------
//
// PROC DrawSkillMenu
//
//---------------------------------------------------------------------------

static void DrawHexenSkillMenu()
{
	screen->DrawText (BigFont, CR_UNTRANSLATED, 74, 16, GStrings("MNU_CHOOSESKILL"), DTA_Clean, true, TAG_DONE);
}


//
//		M_Episode
//
void M_DrawEpisode ()
{
	if (gameinfo.gametype & (GAME_DoomStrifeChex))
	{
		screen->DrawTexture (TexMan["M_EPISOD"], 54, 38, DTA_Clean, true, TAG_DONE);
	}
}

static int confirmskill;

void M_VerifyNightmare (int ch)
{
	if (ch != 'y')
		return;
		
	// Tell the server we're leaving the game.
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
		CLIENT_QuitNetworkGame( NULL );

	// Assume normal mode for going through the menu.
	deathmatch = false;
	teamgame = false;

	// Turn campaign mode back on.
	CAMPAIGN_EnableCampaign( );

	G_DeferedInitNew (EpisodeMaps[epi], confirmskill);
	if (gamestate == GS_FULLCONSOLE)
	{
		gamestate = GS_HIDECONSOLE;
		gameaction = ga_newgame;
	}
	M_ClearMenus ();
}

void M_VerifyBotNightmare (int ch)
{
	if (ch != 'y')
		return;

	// Tell the server we're leaving the game.
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
		CLIENT_QuitNetworkGame( NULL );

	// Assume normal mode for going through the menu.
	deathmatch = false;
	teamgame = false;

	// Turn campaign mode back on.
	CAMPAIGN_EnableCampaign( );

	botskill = 4;
	G_DeferedInitNew( EpisodeMaps[epi] );
	gamestate = gamestate == GS_FULLCONSOLE ? GS_HIDECONSOLE : gamestate;
	M_ClearMenus ();
}

void M_ChooseSkill (int choice)
{
	UCVarValue	Val;

	if (AllSkills[choice].MustConfirm)
	{
		const char *msg = AllSkills[choice].MustConfirmText;
		if (*msg==0) msg = GStrings("NIGHTMARE");
		if (*msg=='$') msg = GStrings(msg+1);
		confirmskill = choice;
		M_StartMessage (msg, M_VerifyNightmare);
		return;
	}

	// [BC] Tell the server we're leaving the game.
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
		CLIENT_QuitNetworkGame( NULL );

	// [BC] Put us back in single player mode, and reset our dmflags.
	NETWORK_SetState( NETSTATE_SINGLE );
	Val.Int = 0;
	dmflags.ForceSet( Val, CVAR_Int );
	dmflags2.ForceSet( Val, CVAR_Int );

	// Assume normal mode for going through the menu.
	Val.Bool = false;

	deathmatch.ForceSet( Val, CVAR_Bool );
	teamgame.ForceSet( Val, CVAR_Bool );
	survival.ForceSet( Val, CVAR_Bool );
	invasion.ForceSet( Val, CVAR_Bool );

	// Turn campaign mode back on.
	CAMPAIGN_EnableCampaign( );

	G_DeferedInitNew (EpisodeMaps[epi], choice);
	if (gamestate == GS_FULLCONSOLE)
	{
		gamestate = GS_HIDECONSOLE;
		gameaction = ga_newgame;
	}
	M_ClearMenus ();
}

void M_ChooseBotSkill (int choice)
{
	UCVarValue	Val;

	if (gameinfo.gametype & (GAME_Doom|GAME_Strife) && choice == BotDef.numitems - 1)
	{								  
		M_StartMessage( "are you sure? you'll prolly get\nyour ass whooped!\n\npress y or n.", M_VerifyBotNightmare );
		return;
	}

	// Tell the server we're leaving the game.
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
		CLIENT_QuitNetworkGame( NULL );

	// Assume normal mode for going through the menu.
	Val.Bool = false;

	deathmatch.ForceSet( Val, CVAR_Bool );
	teamgame.ForceSet( Val, CVAR_Bool );

	// Turn campaign mode back on.
	CAMPAIGN_EnableCampaign( );

	botskill = choice;
	gamestate = gamestate == GS_FULLCONSOLE ? GS_HIDECONSOLE : gamestate;
	G_DeferedInitNew( EpisodeMaps[epi] );
	gamestate = gamestate == GS_FULLCONSOLE ? GS_HIDECONSOLE : gamestate;
	M_ClearMenus ();
}

void M_Episode (int choice)
{
	if ((gameinfo.flags & GI_SHAREWARE) && choice)
	{
		if (gameinfo.gametype == GAME_Doom)
		{
			M_StartMessage(GStrings("SWSTRING"), NULL);
			//M_SetupNextMenu(&ReadDef);
		}
		else if (gameinfo.gametype == GAME_Chex)
		{
			M_StartMessage(GStrings("CSWSTRING"), NULL);
		}
		else
		{
			showSharewareMessage = 3*TICRATE;
		}
		return;
	}

	epi = choice;

	if (AllSkills.Size() == 1)
	{
		saved_playerclass = NULL;
		M_ChooseSkill(0);
		return;
	}
	else if (EpisodeNoSkill[choice])
	{
		saved_playerclass = NULL;
		M_ChooseSkill(AllSkills.Size() == 2? 1:2);
		return;
	}
	if ( EpisodeMenu[epi].bBotSkill )
		M_SetupNextMenu (&BotDef);
	else
		M_StartupSkillMenu(saved_playerclass);
	saved_playerclass = NULL;
}

//==========================================================================
//
// SCClass
//
//==========================================================================

static void SCClass (int option)
{
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
	{
		if(gameinfo.gametype == GAME_Chex)
			M_StartMessage (GStrings("CNEWGAME"), NULL);
		else
			M_StartMessage (GStrings("NEWGAME"), NULL);
		return;
	}

	if (option == 3)
		playerclass = "Random";
	else
		playerclass = PlayerClasses[option].Type->Meta.GetMetaString (APMETA_DisplayName);

	if (EpiDef.numitems > 1)
	{
		saved_playerclass = playerclass;
		M_SetupNextMenu (&EpiDef);
	}
	else if (AllSkills.Size() == 1)
	{
		M_ChooseSkill(0);
	}
	else if (!EpisodeNoSkill[0])
	{
		M_StartupSkillMenu(playerclass);
	}
	else
	{
		M_ChooseSkill(AllSkills.Size() == 2? 1:2);
	}
}

// [GRB]
static void M_ChooseClass (int choice)
{
	/* [BB] Skulltag does this a little differently from ZDoom.
	if (netgame)
	{
		if(gameinfo.gametype == GAME_Chex)
			M_StartMessage (GStrings("CNEWGAME"), NULL);
		else
			M_StartMessage (GStrings("NEWGAME"), NULL);
		return;
	}
	*/
	playerclass = (choice < ClassMenuDef.numitems-1) ? ClassMenuItems[choice].name : "Random";
	// [BB] A client chooses the class and tells it to the server with the above line.
	// Afterwards the client just joins.
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
	{
		static char join[] = "join";
		AddCommandString( join );
		M_ClearMenus( );
		return;
	}

	if (EpiDef.numitems > 1)
	{
		saved_playerclass = playerclass;
		M_SetupNextMenu (&EpiDef);
	}
	else if (AllSkills.Size() == 1)
	{
		M_ChooseSkill(0);
	}
	else if (EpisodeNoSkill[0])
	{
		M_ChooseSkill(AllSkills.Size() == 2? 1:2);
	}
	else 
	{
		M_StartupSkillMenu(playerclass);
	}
}


void M_Options (int choice)
{
	OptionsActive = M_StartOptionsMenu ();
}




//
// M_EndGame
//
void M_EndGameResponse(int ch)
{
	if (ch != 'y')
		return;
				
	currentMenu->lastOn = itemOn;
	M_ClearMenus ();
	D_StartTitle ();
}

void M_EndGame(int choice)
{
	choice = 0;
	if (!usergame)
	{
		S_Sound (CHAN_VOICE | CHAN_UI, "menu/invalid", 1, ATTN_NONE);
		return;
	}
		
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
	{
		if(gameinfo.gametype == GAME_Chex)
			M_StartMessage(GStrings("CNETEND"), NULL);
		else
			M_StartMessage(GStrings("NETEND"), NULL);
		return;
	}

	if(gameinfo.gametype == GAME_Chex)
		M_StartMessage(GStrings("CENDGAME"), M_EndGameResponse);
	else
		M_StartMessage(GStrings("ENDGAME"), M_EndGameResponse);
}




//
// M_ReadThis
//
void M_ReadThis (int choice)
{
	drawSkull = false;
	InfoType = 1;
	InfoTic = gametic;
	M_SetupNextMenu (&ReadDef);
}

void M_ReadThisMore (int choice)
{
	InfoType++;
	InfoTic = gametic;
	if ((level.info != NULL && level.info->f1[0] != 0) || InfoType > int(gameinfo.infoPages.Size()))
	{
		M_FinishReadThis (0);
	}
}

void M_FinishReadThis (int choice)
{
	drawSkull = true;
	M_PopMenuStack ();
}

//
// M_QuitGame
//

void M_QuitResponse(int ch)
{
	if (ch != 'y')
		return;
	if ( NETWORK_GetState( ) == NETSTATE_SINGLE )
	{
		if (gameinfo.quitSound.IsNotEmpty())
		{
			S_Sound (CHAN_VOICE | CHAN_UI, gameinfo.quitSound, 1, ATTN_NONE);
			I_WaitVBL (105);
		}
	}
	ST_Endoom();
}

void M_QuitGame (int choice)
{
	if (gameinfo.gametype & (GAME_DoomStrifeChex))
	{
		int quitmsg = 0;
		if (gameinfo.gametype == GAME_Doom)
		{
			quitmsg = gametic % (NUM_QUITDOOMMESSAGES + 1);
		}
		else if (gameinfo.gametype == GAME_Strife)
		{
			quitmsg = gametic % (NUM_QUITSTRIFEMESSAGES + 1);
			if (quitmsg != 0) quitmsg += NUM_QUITDOOMMESSAGES;
		}
		else
		{
			quitmsg = gametic % (NUM_QUITCHEXMESSAGES + 1);
			if (quitmsg != 0) quitmsg += NUM_QUITDOOMMESSAGES + NUM_QUITSTRIFEMESSAGES;
		}

		if (quitmsg != 0)
		{
			EndString.Format("QUITMSG%d", quitmsg);
			EndString.Format("%s\n\n%s", GStrings(EndString), GStrings("DOSY"));
		}
		else
		{
			EndString.Format("%s\n\n%s", GStrings("QUITMSG"), GStrings("DOSY"));
		}
	}
	else
	{
		EndString = GStrings("RAVENQUITMSG");
	}

	M_StartMessage (EndString, M_QuitResponse);
}


//
// [RH] Player Setup Menu code
//
void M_PlayerSetup (void)
{
	// [BC] Skulltag does this differently.
/*
	OptionsActive = false;
	drawSkull = true;
	strcpy (savegamestring, name);
	M_DemoNoPlay = true;
*/
	if (demoplayback)
		G_CheckDemoStatus ();
	// [BC] Support for client-side demos.
	if ( CLIENTDEMO_IsPlaying( ))
		CLIENTDEMO_FinishPlaying( );

	// [BC] Initialize all placeholder values for the player setup menu.
	M_SetupPlayerSetupMenu( );

	// [BB] Init announcer list.
	InitAnnouncersList();

	// [BC] Switch to the player setup menu.
	M_SwitchMenu( &PlayerSetupMenu );

	// [BC] Instead of using the actual player color, skin, etc., use placeholder values.
	if (players[consoleplayer].mo != NULL)
	{
		PickPlayerClass( );
/*
		if ( g_lPlayerSetupClass == -1 )
			PlayerClass = &PlayerClasses[0];
		else
			PlayerClass = &PlayerClasses[g_lPlayerSetupClass];
*/
	}
	PlayerSkin = g_ulPlayerSetupSkin;
	R_GetPlayerTranslation (g_ulPlayerSetupColor,
		P_GetPlayerColorSet(PlayerClass->Type->TypeName, players[consoleplayer].userinfo.colorset),
		&skins[g_ulPlayerSetupSkin], translationtables[TRANSLATION_Players][MAXPLAYERS]);
	PlayerState = GetDefaultByType (PlayerClass->Type)->SeeState;
	PlayerTics = PlayerState->GetTics();
	if (FireTexture == NULL)
	{
		FireTexture = new FBackdropTexture;
	}
	P_EnumPlayerColorSets(PlayerClass->Type->TypeName, &PlayerColorSets);
}

static void M_PlayerSetupTicker (void)
{
	// Based on code in f_finale.c
	FPlayerClass *oldclass = PlayerClass;

	if (currentMenu == &ClassMenuDef)
	{
		int item;

		if (itemOn < ClassMenuDef.numitems-1)
			item = itemOn;
		else
			item = (MenuTime>>2) % (ClassMenuDef.numitems-1);

		PlayerClass = &PlayerClasses[D_PlayerClassToInt (ClassMenuItems[item].name)];
		P_EnumPlayerColorSets(PlayerClass->Type->TypeName, &PlayerColorSets);
	}
	else
	{
		PickPlayerClass ();
	}

	if (PlayerClass != oldclass)
	{
		PlayerState = GetDefaultByType (PlayerClass->Type)->SeeState;
		PlayerTics = PlayerState->GetTics();

		// [BB] Adapted the following code to Skulltag's player setup menu handling.
		g_ulPlayerSetupSkin = R_FindSkin (skins[g_ulPlayerSetupSkin].name, int(PlayerClass - &PlayerClasses[0]));
		PlayerSkin = g_ulPlayerSetupSkin;
		R_GetPlayerTranslation (g_ulPlayerSetupColor,
			P_GetPlayerColorSet(PlayerClass->Type->TypeName, players[consoleplayer].userinfo.colorset),
			&skins[g_ulPlayerSetupSkin], translationtables[TRANSLATION_Players][MAXPLAYERS]);
	}

	if (PlayerState->GetTics () != -1 && PlayerState->GetNextState () != NULL)
	{
		if (--PlayerTics > 0)
			return;

		PlayerState = PlayerState->GetNextState();
		PlayerTics = PlayerState->GetTics();
	}
}


static void M_DrawPlayerSlider (int x, int y, int cur)
{
	const int range = 255;

	x = (x - 160) * CleanXfac + screen->GetWidth() / 2;
	y = (y - 100) * CleanYfac + screen->GetHeight() / 2;

	screen->DrawText (ConFont, CR_WHITE, x, y,
		"\x10\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x12",
		DTA_CellX, 8 * CleanXfac,
		DTA_CellY, 8 * CleanYfac,
		TAG_DONE);
	screen->DrawText (ConFont, CR_ORANGE, x + (5 + (int)((cur * 78) / range)) * CleanXfac, y,
		"\x13",
		DTA_CellX, 8 * CleanXfac,
		DTA_CellY, 8 * CleanYfac,
		TAG_DONE);
}

static bool M_PlayerSetupDrawer ()
{
	const int LINEHEIGHT = PLAYERSETUP_LINEHEIGHT;
	int xo, yo;
	EColorRange label, value;
	// [BC] Store the line height.
	ULONG	ulLineHeight;
	// [BC] Since the new player setup menu is based off of a menu type that has offsets,
	// we have to define them here.
	ULONG	ulOldPlayerSetupXOffset;
	ULONG	ulOldPlayerSetupYOffset;
	
	// [BC] Define those offsets here.
	ulOldPlayerSetupXOffset = 72;
	ulOldPlayerSetupYOffset = 36; // [RC] Move the player display up a bit so the text doesn't overlap
	if ( gameinfo.gametype != GAME_Doom )
		ulOldPlayerSetupYOffset -= 7;

	// [BC] Set the line height.
	ulLineHeight = SmallFont->GetHeight( );

	if (!(gameinfo.gametype & (GAME_DoomStrifeChex)))
	{
		xo = 5;
		yo = 5;
		label = CR_GREEN;
		value = CR_UNTRANSLATED;
	}
	else
	{
		xo = yo = 0;
		label = CR_UNTRANSLATED;
		value = CR_GREY;
	}

	// [BC] Skulltag doesn't need to draw this.
/*
	// Draw title
	const char *text = GStrings("MNU_PLAYERSETUP");
	screen->DrawText (BigFont, gameinfo.gametype & GAME_DoomChex ? CR_RED : CR_UNTRANSLATED,
		160 - BigFont->StringWidth (text)/2,
		15,
		text, DTA_Clean, true, TAG_DONE);
*/

	// [BC] NOTE: Make sure the font is set when we display "Press space", etc.

	// [BC] Skulltag doesn't need to draw this.
/*
	// Draw player name box
	screen->DrawText (SmallFont, label, PSetupDef.x, PSetupDef.y+yo, "Name", DTA_Clean, true, TAG_DONE);
	M_DrawSaveLoadBorder (PSetupDef.x + 56, PSetupDef.y, MAXPLAYERNAME+1);
	screen->DrawText (SmallFont, CR_UNTRANSLATED, PSetupDef.x + 56 + xo, PSetupDef.y+yo, savegamestring,
		DTA_Clean, true, TAG_DONE);

	// Draw cursor for player name box
	if (genStringEnter)
		screen->DrawText (SmallFont, CR_UNTRANSLATED,
			PSetupDef.x + SmallFont->StringWidth(savegamestring) + 56+xo,
			PSetupDef.y + yo, underscore, DTA_Clean, true, TAG_DONE);

	// Draw player team setting
	x = SmallFont->StringWidth ("Team") + 8 + PSetupDef.x;
	screen->DrawText (SmallFont, label, PSetupDef.x, PSetupDef.y + LINEHEIGHT+yo, "Team",
		DTA_Clean, true, TAG_DONE);
	screen->DrawText (SmallFont, value, x, PSetupDef.y + LINEHEIGHT+yo,
		!TeamLibrary.IsValidTeam (players[consoleplayer].userinfo.team) ? "None" :
		Teams[players[consoleplayer].userinfo.team].GetName (),
		DTA_Clean, true, TAG_DONE);
*/
	// Draw player character

	// [BC] NOTE: This part draws the backdrop.
	{
		const int x = ( 320 - 88 - 32 + xo ) * CleanXfac_1, y = ( ulOldPlayerSetupYOffset + ulLineHeight*3 - 18 + yo ) * CleanYfac_1;

		if (!FireTexture)
		{
			screen->Clear (x, y, x + 72 * CleanXfac_1, y + 80 * CleanYfac_1-1, 0, 0);
		}
		else
		{
			screen->DrawTexture (FireTexture, x, y - 1,
				DTA_DestWidth, 72 * CleanXfac_1,
				DTA_DestHeight, 80 * CleanYfac_1,
				DTA_Translation, &FireRemap,
				DTA_Masked, false,
				TAG_DONE);
		}

		M_DrawFrame (x, y, 72*CleanXfac_1, 80*CleanYfac_1-1);
	}

	// [BC] NOTE: This part renders the actual character.
	{
		spriteframe_t *sprframe;
		fixed_t ScaleX, ScaleY;

		if (GetDefaultByType (PlayerClass->Type)->flags4 & MF4_NOSKIN ||
			g_lPlayerSetupClass == -1 ||
			PlayerState->sprite != GetDefaultByType (PlayerClass->Type)->SpawnState->sprite)
		{
			sprframe = &SpriteFrames[sprites[PlayerState->sprite].spriteframes + PlayerState->GetFrame()];
			ScaleX = GetDefaultByType(PlayerClass->Type)->scaleX;
			ScaleY = GetDefaultByType(PlayerClass->Type)->scaleY;
		}
		else
		{
			sprframe = &SpriteFrames[sprites[skins[g_ulPlayerSetupSkin].sprite].spriteframes + PlayerState->GetFrame()];
			ScaleX = skins[g_ulPlayerSetupSkin].ScaleX;
			ScaleY = skins[g_ulPlayerSetupSkin].ScaleY;
		}

		if (sprframe != NULL)
		{
			FTexture *tex = TexMan(sprframe->Texture[0]);
			if (tex != NULL && tex->UseType != FTexture::TEX_Null)
			{
				if (tex->Rotations != 0xFFFF)
				{
					tex = TexMan(SpriteFrames[tex->Rotations].Texture[PlayerRotation]);
				}
				screen->DrawTexture (tex,
					(320 - 52 - 32 + xo)*CleanXfac_1,
					(ulOldPlayerSetupYOffset + ulLineHeight*3 + 57 - 4)*CleanYfac_1,
					DTA_DestWidth, MulScale16 (tex->GetWidth() * CleanXfac_1, ScaleX),
					DTA_DestHeight, MulScale16 (tex->GetHeight() * CleanYfac_1, ScaleY),
					DTA_Translation, translationtables[TRANSLATION_Players](MAXPLAYERS),
					TAG_DONE);
			}
		}

		const char *str = "PRESS SPACE"; // [RC] Color tweak so it sticks out less
		screen->DrawText (SmallFont, CR_DARKGRAY, ( 320 - 52 - 32 -
			SmallFont->StringWidth (str)/2 ) *  CleanXfac_1,
			( (ULONG)( ulOldPlayerSetupYOffset + ulLineHeight * 3 + 69 ) ) * CleanYfac_1, str,
			DTA_CleanNoMove_1, true, TAG_DONE);
		str = PlayerRotation ? "TO SEE FRONT" : "TO SEE BACK";
		screen->DrawText (SmallFont, CR_DARKGRAY, ( 320 - 52 - 32 -
			SmallFont->StringWidth (str)/2 ) * CleanXfac_1,
			( (ULONG)( ulOldPlayerSetupYOffset + ulLineHeight * 4 + 69 ) ) * CleanYfac_1, str,
			DTA_CleanNoMove_1, true, TAG_DONE);
	}

	// [BC] Skulltag doesn't need to draw this.
/*
	// Draw player color selection and sliders
	FPlayerColorSet *colorset = P_GetPlayerColorSet(PlayerClass->Type->TypeName, players[consoleplayer].userinfo.colorset);
	x = SmallFont->StringWidth("Color") + 8 + PSetupDef.x;
	screen->DrawText(SmallFont, label, PSetupDef.x, PSetupDef.y + LINEHEIGHT*2+yo, "Color", DTA_Clean, true, TAG_DONE);
	screen->DrawText(SmallFont, value, x, PSetupDef.y + LINEHEIGHT*2+yo,
		colorset != NULL ? colorset->Name.GetChars() : "Custom", DTA_Clean, true, TAG_DONE);

	// Only show the sliders for a custom color set.
	if (colorset == NULL)
	{
		screen->DrawText (SmallFont, label, PSetupDef.x, PSetupDef.y + int(LINEHEIGHT*2.875)+yo, "Red", DTA_Clean, true, TAG_DONE);
		screen->DrawText (SmallFont, label, PSetupDef.x, PSetupDef.y + int(LINEHEIGHT*3.5)+yo, "Green", DTA_Clean, true, TAG_DONE);
		screen->DrawText (SmallFont, label, PSetupDef.x, PSetupDef.y + int(LINEHEIGHT*4.125)+yo, "Blue", DTA_Clean, true, TAG_DONE);

		x = SmallFont->StringWidth ("Green") + 8 + PSetupDef.x;
		color = players[consoleplayer].userinfo.color;

		M_DrawPlayerSlider (x, PSetupDef.y + int(LINEHEIGHT*2.875)+yo, RPART(color));
		M_DrawPlayerSlider (x, PSetupDef.y + int(LINEHEIGHT*3.5)+yo, GPART(color));
		M_DrawPlayerSlider (x, PSetupDef.y + int(LINEHEIGHT*4.125)+yo, BPART(color));
	}

	// [GRB] Draw class setting
	int pclass = players[consoleplayer].userinfo.PlayerClass;
	x = SmallFont->StringWidth ("Class") + 8 + PSetupDef.x;
	screen->DrawText (SmallFont, label, PSetupDef.x, PSetupDef.y + LINEHEIGHT*5+yo, "Class", DTA_Clean, true, TAG_DONE);
	screen->DrawText (SmallFont, value, x, PSetupDef.y + LINEHEIGHT*5+yo,
		pclass == -1 ? "Random" : PlayerClasses[pclass].Type->Meta.GetMetaString (APMETA_DisplayName),
		DTA_Clean, true, TAG_DONE);

	// Draw skin setting
	x = SmallFont->StringWidth ("Skin") + 8 + PSetupDef.x;
	screen->DrawText (SmallFont, label, PSetupDef.x, PSetupDef.y + LINEHEIGHT*6+yo, "Skin", DTA_Clean, true, TAG_DONE);
	if (GetDefaultByType (PlayerClass->Type)->flags4 & MF4_NOSKIN ||
		players[consoleplayer].userinfo.PlayerClass == -1)
	{
		screen->DrawText (SmallFont, value, x, PSetupDef.y + LINEHEIGHT*6+yo, "Base", DTA_Clean, true, TAG_DONE);
	}
	else
	{
		screen->DrawText (SmallFont, value, x, PSetupDef.y + LINEHEIGHT*6+yo,
			skins[PlayerSkin].name, DTA_Clean, true, TAG_DONE);
	}

	// Draw gender setting
	x = SmallFont->StringWidth ("Gender") + 8 + PSetupDef.x;
	screen->DrawText (SmallFont, label, PSetupDef.x, PSetupDef.y + LINEHEIGHT*7+yo, "Gender", DTA_Clean, true, TAG_DONE);
	screen->DrawText (SmallFont, value, x, PSetupDef.y + LINEHEIGHT*7+yo,
		genders[players[consoleplayer].userinfo.gender], DTA_Clean, true, TAG_DONE);

	// Draw autoaim setting
	x = SmallFont->StringWidth ("Autoaim") + 8 + PSetupDef.x;
	screen->DrawText (SmallFont, label, PSetupDef.x, PSetupDef.y + LINEHEIGHT*8+yo, "Autoaim", DTA_Clean, true, TAG_DONE);
	screen->DrawText (SmallFont, value, x, PSetupDef.y + LINEHEIGHT*8+yo,
		autoaim == 0 ? "Never" :
		autoaim <= 0.25 ? "Very Low" :
		autoaim <= 0.5 ? "Low" :
		autoaim <= 1 ? "Medium" :
		autoaim <= 2 ? "High" :
		autoaim <= 3 ? "Very High" : "Always",
		DTA_Clean, true, TAG_DONE);
*/
	return false;
}

// A 32x32 cloud rendered with Photoshop, plus some other filters
static BYTE pattern1[1024] =
{
	 5, 9, 7,10, 9,15, 9, 7, 8,10, 5, 3, 5, 7, 9, 8,14, 8, 4, 7, 8, 9, 5, 7,14, 7, 0, 7,13,13, 9, 6,
	 2, 7, 9, 7, 7,10, 8, 8,11,10, 6, 7,10, 7, 5, 6, 6, 4, 7,13,15,16,11,15,11, 8, 0, 4,13,22,17,11,
	 5, 9, 9, 7, 9,10, 4, 3, 6, 7, 8, 6, 5, 4, 2, 2, 1, 4, 6,11,15,15,14,13,17, 9, 5, 9,11,12,17,20,
	 9,16, 9, 8,12,13, 7, 3, 7, 9, 5, 4, 2, 5, 5, 5, 7,11, 6, 7, 6,13,17,10,10, 9,12,17,14,12,16,15,
	15,13, 5, 3, 9,10, 4,10,12,12, 7, 9, 8, 8, 8,10, 7, 6, 5, 5, 5, 6,11, 9, 3,13,16,18,21,16,23,18,
	23,13, 0, 0, 0, 0, 0,12,18,14,15,16,13, 7, 7, 5, 9, 6, 6, 8, 4, 0, 0, 0, 0,14,19,17,14,20,21,25,
	19,20,14,13, 7, 5,13,19,14,13,17,15,14, 7, 3, 5, 6,11, 7, 7, 8, 8,10, 9, 9,18,17,15,14,15,18,16,
	16,29,24,23,18, 9,17,20,11, 5,12,15,15,12, 6, 3, 4, 6, 7,10,13,18,18,19,16,12,17,19,23,16,14,14,
	 9,18,20,26,19, 5,18,18,10, 5,12,15,14,17,11, 6,11, 9,10,13,10,20,24,20,21,20,14,18,15,22,20,19,
	 0, 6,16,18, 8, 7,15,18,10,13,17,17,13,11,15,11,19,12,13,10, 4,15,19,21,21,24,14, 9,17,20,24,17,
	18,17, 7, 7,16,21,22,15, 5,14,20,14,13,21,13, 8,12,14, 7, 8,11,15,13,11,16,17, 7, 5,12,17,19,14,
	25,23,17,16,23,18,15, 7, 0, 6,11, 6,11,15,11, 7,12, 7, 4,10,16,13, 7, 7,15,13, 9,15,21,14, 5, 0,
	18,22,21,21,21,22,12, 6,14,20,15, 6,10,19,13, 8, 7, 3, 7,12,14,16, 9,12,22,15,12,18,24,19,17, 9,
	 0,15,18,21,17,25,14,13,19,21,21,11, 6,13,16,16,12,10,12,11,13,20,14,13,18,13, 9,15,16,25,31,20,
	 5,20,24,16, 7,14,14,11,18,19,19, 6, 0, 5,11,14,17,16,19,14,15,21,19,15,14,14, 8, 0, 7,24,18,16,
	 9,17,15, 7, 6,14,12, 7,14,16,11, 4, 7, 6,13,16,15,13,12,20,21,20,21,17,18,26,14, 0,13,23,21,11,
	 9,12,18,11,15,21,13, 8,13,13,10, 7,13, 8, 8,19,13, 7, 4,15,19,18,14,12,14,15, 8, 6,16,22,22,15,
	 9,17,14,19,15,14,15, 9,11, 9, 6, 8,14,13,13,12, 5, 0, 0, 6,12,13, 7, 7, 9, 7, 0,12,21,16,15,18,
	15,16,18,11, 6, 8,15, 9, 2, 0, 5,10,10,16, 9, 0, 4,12,15, 9,12, 9, 7, 7,12, 7, 0, 6,12, 6, 9,13,
	12,19,15,14,11, 7, 8, 9,12,10, 5, 5, 7,12,12,10,14,16,16,11, 8,12,10,12,10, 8,10,10,14,12,16,16,
	16,17,20,22,12,15,12,14,19,11, 6, 5,10,13,17,17,21,19,15, 9, 6, 9,15,18,10,10,18,14,20,15,16,17,
	11,19,19,18,19,14,17,13,12,12, 7,11,18,17,16,15,19,19,10, 2, 0, 8,15,12, 8,11,12,10,19,20,19,19,
	 6,14,18,13,13,16,16,12, 5, 8,10,12,10,13,18,12, 9,10, 7, 6, 5,11, 8, 6, 7,13,16,13,10,15,20,14,
	 0, 5,12,12, 4, 0, 9,16, 9,10,12, 8, 0, 9,13, 9, 0, 2, 4, 7,10, 6, 7, 3, 4,11,16,18,10,11,21,21,
	16,13,11,15, 8, 0, 5, 9, 8, 7, 6, 3, 0, 9,17, 9, 0, 0, 0, 3, 5, 4, 3, 5, 7,15,16,16,17,14,22,22,
	24,14,15,12, 9, 0, 5,10, 8, 4, 7,12,10,11,12, 7, 6, 8, 6, 5, 7, 8, 8,11,13,10,15,14,12,18,20,16,
	16,17,17,18,12, 9,12,16,10, 5, 6,20,13,15, 8, 4, 8, 9, 8, 7, 9,11,12,17,16,16,11,10, 9,10, 5, 0,
	 0,14,18,18,15,16,14, 9,10, 9, 9,15,14,10, 4, 6,10, 8, 8, 7,10, 9,10,16,18,10, 0, 0, 7,12,10, 8,
	 0,14,19,14, 9,11,11, 8, 8,10,15, 9,10, 7, 4,10,13, 9, 7, 5, 5, 7, 7, 7,13,13, 5, 5,14,22,18,16,
	 0,10,14,10, 3, 6, 5, 6, 8, 9, 8, 9, 5, 9, 8, 9, 6, 8, 8, 8, 1, 0, 0, 0, 9,17,12,12,17,19,20,13,
	 6,11,17,11, 5, 5, 8,10, 6, 5, 6, 6, 3, 7, 9, 7, 6, 8,12,10, 4, 8, 6, 6,11,16,16,15,16,17,17,16,
	11, 9,10,10, 5, 6,12,10, 5, 1, 6,10, 5, 3, 3, 5, 4, 7,15,10, 7,13, 7, 8,15,11,15,15,15, 8,11,15,
};

// Just a 32x32 cloud rendered with the standard Photoshop filter
static BYTE pattern2[1024] =
{
	  9, 9, 8, 8, 8, 8, 6, 6,13,13,11,21,19,21,23,18,23,24,19,19,24,17,18,12, 9,14, 8,12,12, 5, 8, 6,
	 11,10, 6, 7, 8, 8, 9,13,10,11,17,15,23,22,23,22,20,26,27,26,17,21,20,14,12, 8,11, 8,11, 7, 8, 7,
	  6, 9,13,13,10, 9,13, 7,12,13,16,19,16,20,22,25,22,25,27,22,21,23,15,10,14,14,15,13,12, 8,12, 6,
	  6, 7,12,12,12,16, 9,12,12,15,16,11,21,24,19,24,23,26,28,27,26,21,14,15, 7, 7,10,15,12,11,10, 9,
	  7,14,11,16,12,18,16,14,16,14,11,14,15,21,23,17,20,18,26,24,27,18,20,11,11,14,10,17,17,10, 6,10,
	 13, 9,14,10,13,11,14,15,18,15,15,12,19,19,20,18,22,20,19,22,19,19,19,20,17,15,15,11,16,14,10, 8,
	 13,16,12,16,17,19,17,18,15,19,14,18,15,14,15,17,21,19,23,18,23,22,18,18,17,15,15,16,12,12,15,10,
	 10,12,14,10,16,11,18,15,21,20,20,17,18,19,16,19,14,20,19,14,19,25,22,21,22,24,18,12, 9, 9, 8, 6,
	 10,10,13, 9,15,13,20,19,22,18,18,17,17,21,21,13,13,12,19,18,16,17,27,26,22,23,20,17,12,11, 8, 9,
	  7,13,14,15,11,13,18,22,19,23,23,20,22,24,21,14,12,16,17,19,18,18,22,18,24,23,19,17,16,14, 8, 7,
	 12,12, 8, 8,16,20,26,25,28,28,22,29,23,22,21,18,13,16,15,15,20,17,25,24,19,17,17,17,15,10, 8, 9,
	  7,12,15,11,17,20,25,25,25,29,30,31,28,26,18,16,17,18,20,21,22,20,23,19,18,19,10,16,16,11,11, 8,
	  5, 6, 8,14,14,17,17,21,27,23,27,31,27,22,23,21,19,19,21,19,20,19,17,22,13,17,12,15,10,10,12, 6,
	  8, 9, 8,14,15,16,15,18,27,26,23,25,23,22,18,21,20,17,19,20,20,16,20,14,15,13,12, 8, 8, 7,11,13,
	  7, 6,11,11,11,13,15,22,25,24,26,22,24,26,23,18,24,24,20,18,20,16,17,12,12,12,10, 8,11, 9, 6, 8,
	  9,10, 9, 6, 5,14,16,19,17,21,26,20,23,19,19,17,20,21,26,25,23,21,17,13,12, 5,13,11, 7,12,10,12,
	  6, 5, 4,10,11, 9,10,13,17,20,20,18,23,26,27,20,21,24,20,19,24,20,18,10,11, 3, 6,13, 9, 6, 8, 8,
	  1, 2, 2,11,13,13,11,16,16,16,19,21,20,23,22,28,21,20,19,18,23,16,18, 7, 5, 9, 7, 6, 5,10, 8, 8,
	  0, 0, 6, 9,11,15,12,12,19,18,19,26,22,24,26,30,23,22,22,16,20,19,12,12, 3, 4, 6, 5, 4, 7, 2, 4,
	  2, 0, 0, 7,11, 8,14,13,15,21,26,28,25,24,27,26,23,24,22,22,15,17,12, 8,10, 7, 7, 4, 0, 5, 0, 1,
	  1, 2, 0, 1, 9,14,13,10,19,24,22,29,30,28,30,30,31,23,24,19,17,14,13, 8, 8, 8, 1, 4, 0, 0, 0, 3,
	  5, 2, 4, 2, 9, 8, 8, 8,18,23,20,27,30,27,31,25,28,30,28,24,24,15,11,14,10, 3, 4, 3, 0, 0, 1, 3,
	  9, 3, 4, 3, 5, 6, 8,13,14,23,21,27,28,27,28,27,27,29,30,24,22,23,13,15, 8, 6, 2, 0, 4, 3, 4, 1,
	  6, 5, 5, 3, 9, 3, 6,14,13,16,23,26,28,23,30,31,28,29,26,27,21,20,15,15,13, 9, 1, 0, 2, 0, 5, 8,
	  8, 4, 3, 7, 2, 0,10, 7,10,14,21,21,29,28,25,27,30,28,25,24,27,22,19,13,10, 5, 0, 0, 0, 0, 0, 7,
	  7, 6, 7, 0, 2, 2, 5, 6,15,11,19,24,22,29,27,31,30,30,31,28,23,18,14,14, 7, 5, 0, 0, 1, 0, 1, 0,
	  5, 5, 5, 0, 0, 4, 5,11, 7,10,13,20,21,21,28,31,28,30,26,28,25,21, 9,12, 3, 3, 0, 2, 2, 2, 0, 1,
	  3, 3, 0, 2, 0, 3, 5, 3,11,11,16,19,19,27,26,26,30,27,28,26,23,22,16, 6, 2, 2, 3, 2, 0, 2, 4, 0,
	  0, 0, 0, 3, 3, 1, 0, 4, 5, 9,11,16,24,20,28,26,28,24,28,25,22,21,16, 5, 7, 5, 7, 3, 2, 3, 3, 6,
	  0, 0, 2, 0, 2, 0, 4, 3, 8,12, 9,17,16,23,23,27,27,22,26,22,21,21,13,14, 5, 3, 7, 3, 2, 4, 6, 1,
	  2, 5, 6, 4, 0, 1, 5, 8, 7, 6,15,17,22,20,24,28,23,25,20,21,18,16,13,15,13,10, 8, 5, 5, 9, 3, 7,
	  7, 7, 0, 5, 1, 6, 7, 9,12, 9,12,21,22,25,24,22,23,25,24,18,24,22,17,13,10, 9,10, 9, 6,11, 6, 5,
};

const FTexture::Span FBackdropTexture::DummySpan[2] = { { 0, 160 }, { 0, 0 } };

FBackdropTexture::FBackdropTexture()
{
	Width = 144;
	Height = 160;
	WidthBits = 8;
	HeightBits = 8;
	WidthMask = 255;
	LastRenderTic = 0;

	time1 = ANGLE_1*180;
	time2 = ANGLE_1*56;
	time3 = ANGLE_1*99;
	time4 = ANGLE_1*1;
	t1ang = ANGLE_90;
	t2ang = 0;
	z1ang = 0;
	z2ang = ANGLE_90/2;
}

bool FBackdropTexture::CheckModified()
{
	return LastRenderTic != gametic;
}

void FBackdropTexture::Unload()
{
}

const BYTE *FBackdropTexture::GetColumn(unsigned int column, const Span **spans_out)
{
	if (LastRenderTic != gametic)
	{
		Render();
	}
	column = clamp(column, 0u, 143u);
	if (spans_out != NULL)
	{
		*spans_out = DummySpan;
	}
	return Pixels + column*160;
}

const BYTE *FBackdropTexture::GetPixels()
{
	if (LastRenderTic != gametic)
	{
		Render();
	}
	return Pixels;
}

// This is one plasma and two rotozoomers. I think it turned out quite awesome.
void FBackdropTexture::Render()
{
	BYTE *from;
	int width, height, pitch;

	width = 160;
	height = 144;
	pitch = width;

	int x, y;

	const angle_t a1add = ANGLE_1/2;
	const angle_t a2add = ANGLE_MAX-ANGLE_1;
	const angle_t a3add = ANGLE_1*5/7;
	const angle_t a4add = ANGLE_MAX-ANGLE_1*4/3;

	const angle_t t1add = ANGLE_MAX-ANGLE_1*2;
	const angle_t t2add = ANGLE_MAX-ANGLE_1*3+ANGLE_1/6;
	const angle_t t3add = ANGLE_1*16/7;
	const angle_t t4add = ANGLE_MAX-ANGLE_1*2/3;
	const angle_t x1add = 5<<ANGLETOFINESHIFT;
	const angle_t x2add = ANGLE_MAX-(13<<ANGLETOFINESHIFT);
	const angle_t z1add = 3<<ANGLETOFINESHIFT;
	const angle_t z2add = 4<<ANGLETOFINESHIFT;

	angle_t a1, a2, a3, a4;
	fixed_t c1, c2, c3, c4;
	DWORD tx, ty, tc, ts;
	DWORD ux, uy, uc, us;
	DWORD ltx, lty, lux, luy;

	from = Pixels;

	a3 = time3;
	a4 = time4;

	fixed_t z1 = (finecosine[z2ang>>ANGLETOFINESHIFT]>>2)+FRACUNIT/2;
	fixed_t z2 = (finecosine[z1ang>>ANGLETOFINESHIFT]>>2)+FRACUNIT*3/4;

	tc = MulScale5 (finecosine[t1ang>>ANGLETOFINESHIFT], z1);
	ts = MulScale5 (finesine[t1ang>>ANGLETOFINESHIFT], z1);
	uc = MulScale5 (finecosine[t2ang>>ANGLETOFINESHIFT], z2);
	us = MulScale5 (finesine[t2ang>>ANGLETOFINESHIFT], z2);

	ltx = -width/2*tc;
	lty = -width/2*ts;
	lux = -width/2*uc;
	luy = -width/2*us;

	for (y = 0; y < height; ++y)
	{
		a1 = time1;
		a2 = time2;
		c3 = finecosine[a3>>ANGLETOFINESHIFT];
		c4 = finecosine[a4>>ANGLETOFINESHIFT];
		tx = ltx - (y-height/2)*ts;
		ty = lty + (y-height/2)*tc;
		ux = lux - (y-height/2)*us;
		uy = luy + (y-height/2)*uc;
		for (x = 0; x < width; ++x)
		{
			c1 = finecosine[a1>>ANGLETOFINESHIFT];
			c2 = finecosine[a2>>ANGLETOFINESHIFT];
			from[x] = ((c1 + c2 + c3 + c4) >> (FRACBITS+3-7)) + 128	// plasma
			 + pattern1[(tx>>27)+((ty>>22)&992)]					// rotozoomer 1
			 + pattern2[(ux>>27)+((uy>>22)&992)];					// rotozoomer 2
			tx += tc;
			ty += ts;
			ux += uc;
			uy += us;
			a1 += a1add;
			a2 += a2add;
		}
		a3 += a3add;
		a4 += a4add;
		from += pitch;
	}

	time1 += t1add;
	time2 += t2add;
	time3 += t3add;
	time4 += t4add;
	t1ang += x1add;
	t2ang += x2add;
	z1ang += z1add;
	z2ang += z2add;

	LastRenderTic = gametic;
}

// [BC] Skulltag doesn't use any of this.
/*
static void M_ChangeClass (int choice)
{
	if (PlayerClasses.Size () == 1)
	{
		return;
	}

	int type = players[consoleplayer].userinfo.PlayerClass;

	if (!choice)
		type = (type < 0) ? (int)PlayerClasses.Size () - 1 : type - 1;
	else
		type = (type < (int)PlayerClasses.Size () - 1) ? type + 1 : -1;

	cvar_set ("playerclass", type < 0 ? "Random" :
		PlayerClasses[type].Type->Meta.GetMetaString (APMETA_DisplayName));
}

static void M_ChangeSkin (int choice)
{
	if (GetDefaultByType (PlayerClass->Type)->flags4 & MF4_NOSKIN ||
		players[consoleplayer].userinfo.PlayerClass == -1)
	{
		return;
	}

	do
	{
		if (!choice)
			PlayerSkin = (PlayerSkin == 0) ? (int)numskins - 1 : PlayerSkin - 1;
		else
			PlayerSkin = (PlayerSkin < (int)numskins - 1) ? PlayerSkin + 1 : 0;
	} while (!PlayerClass->CheckSkin (PlayerSkin));

	R_GetPlayerTranslation (players[consoleplayer].userinfo.color,
		P_GetPlayerColorSet(PlayerClass->Type->TypeName, players[consoleplayer].userinfo.colorset),
		&skins[PlayerSkin], translationtables[TRANSLATION_Players][MAXPLAYERS]);

	cvar_set ("skin", skins[PlayerSkin].name);
}

static void M_ChangeGender (int choice)
{
	int gender = players[consoleplayer].userinfo.gender;

	if (!choice)
		gender = (gender == 0) ? 2 : gender - 1;
	else
		gender = (gender == 2) ? 0 : gender + 1;

	cvar_set ("gender", genders[gender]);
}

static void M_ChangeAutoAim (int choice)
{
	static const float ranges[] = { 0, 0.25, 0.5, 1, 2, 3, 5000 };
	float aim = autoaim;
	int i;

	if (!choice) {
		// Select a lower autoaim

		for (i = 6; i >= 1; i--)
		{
			if (aim >= ranges[i])
			{
				aim = ranges[i - 1];
				break;
			}
		}
	}
	else
	{
		// Select a higher autoaim

		for (i = 5; i >= 0; i--)
		{
			if (aim >= ranges[i])
			{
				aim = ranges[i + 1];
				break;
			}
		}
	}

	autoaim = aim;
}

static void M_EditPlayerName (int choice)
{
	// we are going to be intercepting all chars
	genStringEnter = 2;
	genStringEnd = M_PlayerNameChanged;
	genStringCancel = M_PlayerNameNotChanged;
	genStringLen = MAXPLAYERNAME;
	
	saveSlot = 0;
	saveCharIndex = strlen (savegamestring);
}

static void M_PlayerNameNotChanged ()
{
	strcpy (savegamestring, name);
}

static void M_PlayerNameChanged (FSaveGameNode *dummy)
{
	const char *p;
	FString command("name \"");

	// Escape any backslashes or quotation marks before sending the name to the console.
	for (p = savegamestring; *p != '\0'; ++p)
	{
		if (*p == '"' || *p == '\\')
		{
			command << '\\';
		}
		command << *p;
	}
	command << '"';
	C_DoCommand (command);
}

static void M_ChangePlayerTeam (int choice)
{
	if (!choice)
	{
		if (team == 0)
		{
			team = TEAM_NONE;
		}
		else if (team == TEAM_NONE)
		{
			team = Teams.Size () - 1;
		}
		else
		{
			team = team - 1;
		}
	}
	else
	{
		if (team == int(Teams.Size () - 1))
		{
			team = TEAM_NONE;
		}
		else if (team == TEAM_NONE)
		{
			team = 0;
		}
		else
		{
			team = team + 1;
		}	
	}
}

static void M_ChangeColorSet (int choice)
{
	int curpos = (int)PlayerColorSets.Size();
	int mycolorset = players[consoleplayer].userinfo.colorset;
	while (--curpos >= 0)
	{
		if (PlayerColorSets[curpos] == mycolorset)
			break;
	}
	if (choice == 0)
	{
		curpos--;
	}
	else
	{
		curpos++;
	}
	if (curpos < -1)
	{
		curpos = (int)PlayerColorSets.Size() - 1;
	}
	else if (curpos >= (int)PlayerColorSets.Size())
	{
		curpos = -1;
	}
	mycolorset = (curpos >= 0) ? PlayerColorSets[curpos] : -1;

	// disable the sliders if a valid colorset is selected
	PlayerSetupMenu[PSM_RED].status =
	PlayerSetupMenu[PSM_GREEN].status =
	PlayerSetupMenu[PSM_BLUE].status = (mycolorset == -1? 2:-1);

	char command[24];
	mysnprintf(command, countof(command), "colorset %d", mycolorset);
	C_DoCommand(command);
	R_GetPlayerTranslation(players[consoleplayer].userinfo.color,
		P_GetPlayerColorSet(PlayerClass->Type->TypeName, mycolorset),
		&skins[PlayerSkin], translationtables[TRANSLATION_Players][MAXPLAYERS]);
}
*/

void SendNewColor (int red, int green, int blue)
{
	char command[24];

	mysnprintf (command, countof(command), "menu_color \"%02x %02x %02x\"", red, green, blue);
	C_DoCommand (command);

	g_ulPlayerSetupColor = MAKERGB (red, green, blue);
	R_GetPlayerTranslation(MAKERGB (red, green, blue),
		P_GetPlayerColorSet(PlayerClass->Type->TypeName, players[consoleplayer].userinfo.colorset),
		&skins[g_ulPlayerSetupSkin], translationtables[TRANSLATION_Players][MAXPLAYERS]);
}

// [BC] Skulltag doesn't use any of this.
/*
static void M_SlidePlayerRed (int choice)
{
	int color = players[consoleplayer].userinfo.color;
	int red = RPART(color);

	if (choice == 0) {
		red -= 16;
		if (red < 0)
			red = 0;
	} else {
		red += 16;
		if (red > 255)
			red = 255;
	}

	SendNewColor (red, GPART(color), BPART(color));
}

static void M_SlidePlayerGreen (int choice)
{
	int color = players[consoleplayer].userinfo.color;
	int green = GPART(color);

	if (choice == 0) {
		green -= 16;
		if (green < 0)
			green = 0;
	} else {
		green += 16;
		if (green > 255)
			green = 255;
	}

	SendNewColor (RPART(color), green, BPART(color));
}

static void M_SlidePlayerBlue (int choice)
{
	int color = players[consoleplayer].userinfo.color;
	int blue = BPART(color);

	if (choice == 0) {
		blue -= 16;
		if (blue < 0)
			blue = 0;
	} else {
		blue += 16;
		if (blue > 255)
			blue = 255;
	}

	SendNewColor (RPART(color), GPART(color), blue);
}
*/

//
//		Menu Functions
//
void M_StartMessage (const char *string, void (*routine)(int))
{
	C_HideConsole ();
	messageLastMenuActive = menuactive;
	messageToPrint = 1;
	messageString = string;
	messageRoutine = routine;
	messageSelection = 0;
	if (menuactive == MENU_Off)
	{
		M_ActivateMenuInput ();
	}
	if (messageRoutine != NULL)
	{
		S_StopSound (CHAN_VOICE);
		S_Sound (CHAN_VOICE | CHAN_UI, "menu/prompt", 1, ATTN_NONE);
	}
	return;
}

void M_EndMessage(int key)
{
	menuactive = messageLastMenuActive;
	messageToPrint = 0;
	if (messageRoutine != NULL)
	{
		messageRoutine(key);
	}
	if (menuactive != MENU_Off)
	{
		M_DeactivateMenuInput();
	}
	SB_state = screen->GetPageCount();	// refresh the status bar
	BorderNeedRefresh = screen->GetPageCount();
	S_Sound(CHAN_VOICE | CHAN_UI, "menu/dismiss", 1, ATTN_NONE);
}


//
//		Find string height from hu_font chars
//
int M_StringHeight (FFont *font, const char *string)
{
	int h;
	int height = font->GetHeight ();
		
	h = height;
	while (*string)
	{
		if ((*string++) == '\n')
			h += height;
	}
				
	return h;
}



//
// CONTROL PANEL
//

//
// M_Responder
//
bool M_Responder (event_t *ev)
{
	int ch;
	int i;
	EMenuKey mkey = NUM_MKEYS;
	bool keyup = true;

	ch = -1;

	// [BB] chatmodeon is CHAT_GetChatMode( ) in Zandronum.
	if (CHAT_GetChatMode( ))
	{
		return false;
	}
	if (menuactive == MENU_Off && ev->type == EV_KeyDown)
	{
		// Pop-up menu?
		if (ev->data1 == KEY_ESCAPE)
		{
			M_StartControlPanel(true, true);
			return true;
		}
		// If devparm is set, pressing F1 always takes a screenshot no matter
		// what it's bound to. (for those who don't bother to read the docs)
		if (devparm && ev->data1 == KEY_F1)
		{
			G_ScreenShot(NULL);
			return true;
		}
		return false;
	}
	if (menuactive == MENU_WaitKey && OptionsActive)
	{
		M_OptResponder(ev);
		return true;
	}
	if (menuactive != MENU_On && menuactive != MENU_OnNoPause &&
		!genStringEnter && !messageToPrint
		&& !g_bStringInput ) // [BB]
	{
		return false;
	}

	// There are a few input sources we are interested in:
	//
	// EV_KeyDown / EV_KeyUp : joysticks/gamepads/controllers
	// EV_GUI_KeyDown / EV_GUI_KeyUp : the keyboard
	// EV_GUI_Char : printable characters, which we want in string input mode
	//
	// This code previously listened for EV_GUI_KeyRepeat to handle repeating
	// in the menus, but that doesn't work with gamepads, so now we combine
	// the multiple inputs into buttons and handle the repetition manually.
	if (ev->type == EV_GUI_Event)
	{
		// [BB] User is currently inputting a string. Handle input for that here.
		if ( g_bStringInput )
		{
			int ch = tolower (ev->data1);
			menuitem_t *item = CurrentMenu->items + CurrentItem;
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
			return true;
		}

		// Save game and player name string input
		if (genStringEnter)
		{
			if (ev->subtype == EV_GUI_Char)
			{
				InputGridOkay = false;
				if (saveCharIndex < genStringLen &&
					(genStringEnter == 2/*entering player name*/ || (size_t)SmallFont->StringWidth(savegamestring) < (genStringLen-1)*8))
				{
					savegamestring[saveCharIndex] = (char)ev->data1;
					savegamestring[++saveCharIndex] = 0;
				}
				return true;
			}
			ch = ev->data1;
			if ((ev->subtype == EV_GUI_KeyDown || ev->subtype == EV_GUI_KeyRepeat) && ch == '\b')
			{
				if (saveCharIndex > 0)
				{
					saveCharIndex--;
					savegamestring[saveCharIndex] = 0;
				}
			}
			else if (ev->subtype == EV_GUI_KeyDown)
			{
				if (ch == GK_ESCAPE)
				{
					genStringEnter = 0;
					genStringCancel();	// [RH] Function to call when escape is pressed
				}
				else if (ch == '\r')
				{
					if (savegamestring[0])
					{
						genStringEnter = 0;
						if (messageToPrint)
							M_ClearMenus ();
						genStringEnd (SelSaveGame);	// [RH] Function to call when enter is pressed
					}
				}
			}
			if (ev->subtype == EV_GUI_KeyDown || ev->subtype == EV_GUI_KeyRepeat)
			{
				return true;
			}
		}
		if (ev->subtype != EV_GUI_KeyDown && ev->subtype != EV_GUI_KeyUp)
		{
			return false;
		}
		if (ev->subtype == EV_GUI_KeyRepeat)
		{
			// We do our own key repeat handling but still want to eat the
			// OS's repeated keys.
			return true;
		}
		ch = ev->data1;
		keyup = ev->subtype == EV_GUI_KeyUp;
		if (messageToPrint && messageRoutine == NULL)
		{
			if (!keyup && !OptionsActive)
			{
				D_RemoveNextCharEvent();
				M_EndMessage(ch);
				return true;
			}
		}
		switch (ch)
		{
		case GK_ESCAPE:			mkey = MKEY_Back;		break;
		case GK_RETURN:			mkey = MKEY_Enter;		break;
		case GK_UP:				mkey = MKEY_Up;			break;
		case GK_DOWN:			mkey = MKEY_Down;		break;
		case GK_LEFT:			mkey = MKEY_Left;		break;
		case GK_RIGHT:			mkey = MKEY_Right;		break;
		case GK_BACKSPACE:		mkey = MKEY_Clear;		break;
		case GK_PGUP:			mkey = MKEY_PageUp;		break;
		case GK_PGDN:			mkey = MKEY_PageDown;	break;
		default:
			// [BB] Zandronum uses PlayerSetupMenu instead of PSetupDef.
			if (ch == ' ' && ( CurrentMenu == &PlayerSetupMenu ) )
			{
				mkey = MKEY_Clear;
			}
			else if (!keyup)
			{
				if (OptionsActive)
				{
					M_OptResponder(ev);
				}
				else
				{
					ch = tolower (ch);
					if (messageToPrint)
					{
						// Take care of any messages that need input
						ch = tolower (ch);
						assert(messageRoutine != NULL);
						if (ch != ' ' && ch != 'n' && ch != 'y')
						{
							return false;
						}
						D_RemoveNextCharEvent();
						M_EndMessage(ch);
						return true;
					}
					else
					{
						// Search for a menu item associated with the pressed key.
						for (i = (itemOn + 1) % currentMenu->numitems;
							 i != itemOn;
							 i = (i + 1) % currentMenu->numitems)
						{
							if (currentMenu->menuitems[i].alphaKey == ch)
							{
								break;
							}
						}
						if (currentMenu->menuitems[i].alphaKey == ch)
						{
							itemOn = i;
							S_Sound(CHAN_VOICE | CHAN_UI, "menu/cursor", 1, ATTN_NONE);
							return true;
						}
					}
				}
			}
			break;
		}
		if (!keyup)
		{
			InputGridOkay = false;
		}
	}
	else if (ev->type == EV_KeyDown || ev->type == EV_KeyUp)
	{
		keyup = ev->type == EV_KeyUp;
		// If this is a button down, it's okay to show the input grid if the
		// next action causes us to enter genStringEnter mode. If we are
		// already in that mode, then we let M_ButtonHandler() turn it on so
		// that it will know if a button press happened while the input grid
		// was turned off.
		if (!keyup && !genStringEnter)
		{
			InputGridOkay = true;
		}
		ch = ev->data1;
		switch (ch)
		{
		case KEY_JOY1:
		case KEY_PAD_A:
			mkey = MKEY_Enter;
			break;

		case KEY_JOY2:
		case KEY_PAD_B:
			mkey = MKEY_Back;
			break;

		case KEY_JOY3:
		case KEY_PAD_X:
			mkey = MKEY_Clear;
			break;

		case KEY_JOY5:
		case KEY_PAD_LSHOULDER:
			mkey = MKEY_PageUp;
			break;

		case KEY_JOY6:
		case KEY_PAD_RSHOULDER:
			mkey = MKEY_PageDown;
			break;

		case KEY_PAD_DPAD_UP:
		case KEY_PAD_LTHUMB_UP:
		case KEY_JOYAXIS1MINUS:
		case KEY_JOYPOV1_UP:
			mkey = MKEY_Up;
			break;

		case KEY_PAD_DPAD_DOWN:
		case KEY_PAD_LTHUMB_DOWN:
		case KEY_JOYAXIS1PLUS:
		case KEY_JOYPOV1_DOWN:
			mkey = MKEY_Down;
			break;

		case KEY_PAD_DPAD_LEFT:
		case KEY_PAD_LTHUMB_LEFT:
		case KEY_JOYAXIS2MINUS:
		case KEY_JOYPOV1_LEFT:
			mkey = MKEY_Left;
			break;

		case KEY_PAD_DPAD_RIGHT:
		case KEY_PAD_LTHUMB_RIGHT:
		case KEY_JOYAXIS2PLUS:
		case KEY_JOYPOV1_RIGHT:
			mkey = MKEY_Right;
			break;
		}
		// Any button press will work for messages without callbacks
		if (!keyup && messageToPrint && messageRoutine == NULL)
		{
			M_EndMessage(ch);
			return true;
		}
	}

	if (mkey != NUM_MKEYS)
	{
		if (keyup)
		{
			MenuButtons[mkey].ReleaseKey(ch);
		}
		else
		{
			MenuButtons[mkey].PressKey(ch);
			if (mkey <= MKEY_PageDown)
			{
				MenuButtonTickers[mkey] = KEY_REPEAT_DELAY;
			}
			M_ButtonHandler(mkey, false);
		}
	}

	if (ev->type == EV_GUI_Event && (currentMenu == &SaveDef || currentMenu == &LoadDef))
	{
		return M_SaveLoadResponder (ev);
	}

	// Eat key downs, but let the rest through.
	return !keyup;
}

void M_ButtonHandler(EMenuKey key, bool repeat)
{
	if (OptionsActive)
	{
		M_OptButtonHandler(key, repeat);
		return;
	}
	if (key == MKEY_Back)
	{
		if (genStringEnter)
		{
			// Cancel string entry.
			genStringEnter = 0;
			genStringCancel();
		}
		else if (messageToPrint)
		{
			M_EndMessage(GK_ESCAPE);
		}
		else
		{
			// Save the cursor position on the current menu, and pop it off the stack
			// to go back to the previous menu.
			currentMenu->lastOn = itemOn;
			M_PopMenuStack();
		}
		return;
	}
	if (messageToPrint)
	{
		if (key == MKEY_Down || key == MKEY_Up)
		{
			messageSelection ^= 1;
		}
		else if (key == MKEY_Enter)
		{
			M_EndMessage(messageSelection == 0 ? 'y' : 'n');
		}
		return;
	}
	if (genStringEnter)
	{
		int ch;

		switch (key)
		{
		case MKEY_Down:
			InputGridY = (InputGridY + 1) % INPUTGRID_HEIGHT;
			break;

		case MKEY_Up:
			InputGridY = (InputGridY + INPUTGRID_HEIGHT - 1) % INPUTGRID_HEIGHT;
			break;

		case MKEY_Right:
			InputGridX = (InputGridX + 1) % INPUTGRID_WIDTH;
			break;

		case MKEY_Left:
			InputGridX = (InputGridX + INPUTGRID_WIDTH - 1) % INPUTGRID_WIDTH;
			break;

		case MKEY_Clear:
			if (saveCharIndex > 0)
			{
				savegamestring[--saveCharIndex] = 0;
			}
			break;

		case MKEY_Enter:
			assert(unsigned(InputGridX) < INPUTGRID_WIDTH && unsigned(InputGridY) < INPUTGRID_HEIGHT);
			if (InputGridOkay)
			{
				ch = InputGridChars[InputGridX + InputGridY * INPUTGRID_WIDTH];
				if (ch == 0)			// end
				{
					if (savegamestring[0] != '\0')
					{
						genStringEnter = 0;
						if (messageToPrint)
						{
							M_ClearMenus();
						}
						genStringEnd(SelSaveGame);
					}
				}
				else if (ch == '\b')	// bs
				{
					if (saveCharIndex > 0)
					{
						savegamestring[--saveCharIndex] = 0;
					}
				}
				else if (saveCharIndex < genStringLen &&
					(genStringEnter == 2/*entering player name*/ || (size_t)SmallFont->StringWidth(savegamestring) < (genStringLen-1)*8))
				{
					savegamestring[saveCharIndex] = ch;
					savegamestring[++saveCharIndex] = 0;
				}
			}
			break;

		default:
			break;	// Keep GCC quiet
		}
		InputGridOkay = true;
		return;
	}
	if (currentMenu == &SaveDef || currentMenu == &LoadDef)
	{
		M_SaveLoadButtonHandler(key);
		return;
	}
	switch (key)
	{
	case MKEY_Down:
		do
		{
			if (itemOn + 1 >= currentMenu->numitems)
				itemOn = 0;
			else itemOn++;
			S_Sound (CHAN_VOICE | CHAN_UI, "menu/cursor", 1, ATTN_NONE);
		} while (currentMenu->menuitems[itemOn].status == -1);
		break;

	case MKEY_Up:
		do
		{
			if (itemOn == 0)
				itemOn = currentMenu->numitems - 1;
			else itemOn--;
			S_Sound (CHAN_VOICE | CHAN_UI, "menu/cursor", 1, ATTN_NONE);
		} while (currentMenu->menuitems[itemOn].status == -1);
		break;

	case MKEY_Left:
		if (currentMenu->menuitems[itemOn].routine &&
			currentMenu->menuitems[itemOn].status == 2)
		{
			S_Sound (CHAN_VOICE | CHAN_UI, "menu/change", 1, ATTN_NONE);
			currentMenu->menuitems[itemOn].routine(0);
		}
		break;

	case MKEY_Right:
		if (currentMenu->menuitems[itemOn].routine &&
			currentMenu->menuitems[itemOn].status == 2)
		{
			S_Sound (CHAN_VOICE | CHAN_UI, "menu/change", 1, ATTN_NONE);
			currentMenu->menuitems[itemOn].routine(1);
		}
		break;

	case MKEY_Enter:
		if (currentMenu->menuitems[itemOn].routine &&
			currentMenu->menuitems[itemOn].status)
		{
			currentMenu->lastOn = itemOn;
			if (currentMenu->menuitems[itemOn].status == 2)
			{
				currentMenu->menuitems[itemOn].routine(1);		// right arrow
				S_Sound (CHAN_VOICE | CHAN_UI, "menu/change", 1, ATTN_NONE);
			}
			else
			{
				currentMenu->menuitems[itemOn].routine(itemOn);
				S_Sound (CHAN_VOICE | CHAN_UI, "menu/choose", 1, ATTN_NONE);
			}
		}
		break;

	case MKEY_Clear:
		// [BC] This is now done elsewhere.
		//if (currentMenu == &PSetupDef)
		if ( 0 )
		{
			PlayerRotation ^= 8;
		}
		break;

	default:
		break;		// Keep GCC quiet
	}
}

static void M_SaveLoadButtonHandler(EMenuKey key)
{
	if (SelSaveGame == NULL || SelSaveGame->Succ == NULL)
	{
		return;
	}
	switch (key)
	{
	case MKEY_Up:
		if (SelSaveGame != SaveGames.Head)
		{
			if (SelSaveGame == TopSaveGame)
			{
				TopSaveGame = static_cast<FSaveGameNode *>(TopSaveGame->Pred);
			}
			SelSaveGame = static_cast<FSaveGameNode *>(SelSaveGame->Pred);
		}
		else
		{
			SelSaveGame = static_cast<FSaveGameNode *>(SaveGames.TailPred);
		}
		M_UnloadSaveData ();
		M_ExtractSaveData (SelSaveGame);
		break;

	case MKEY_Down:
		if (SelSaveGame != SaveGames.TailPred)
		{
			SelSaveGame = static_cast<FSaveGameNode *>(SelSaveGame->Succ);
		}
		else
		{
			SelSaveGame = TopSaveGame =
				static_cast<FSaveGameNode *>(SaveGames.Head);
		}
		M_UnloadSaveData ();
		M_ExtractSaveData (SelSaveGame);
		break;

	case MKEY_Enter:
		if (currentMenu == &LoadDef)
		{
			M_LoadSelect (SelSaveGame);
		}
		else
		{
			M_SaveSelect (SelSaveGame);
		}
		break;

	default:
		break;		// Keep GCC quiet
	}
}

static bool M_SaveLoadResponder (event_t *ev)
{
	if (ev->subtype != EV_GUI_KeyDown)
	{
		return false;
	}
	if (SelSaveGame != NULL && SelSaveGame->Succ != NULL)
	{
		switch (ev->data1)
		{
		case GK_F1:
			if (!SelSaveGame->Filename.IsEmpty())
			{
				char workbuf[512];

				mysnprintf (workbuf, countof(workbuf), "File on disk:\n%s", SelSaveGame->Filename.GetChars());
				if (SaveComment != NULL)
				{
					V_FreeBrokenLines (SaveComment);
				}
				SaveComment = V_BreakLines (SmallFont, 216*screen->GetWidth()/640/CleanXfac, workbuf);
			}
			break;

		case GK_DEL:
		case '\b':
			if (SelSaveGame != &NewSaveNode)
			{
				EndString.Format("%s" TEXTCOLOR_WHITE "%s" TEXTCOLOR_NORMAL "?\n\n%s",
					GStrings("MNU_DELETESG"), SelSaveGame->Title, GStrings("PRESSYN"));
					
				M_StartMessage (EndString, M_DeleteSaveResponse);
			}
			break;

		case 'N':
			if (currentMenu == &SaveDef)
			{
				SelSaveGame = TopSaveGame = &NewSaveNode;
				M_UnloadSaveData ();
			}
			break;
		}
	}
	return true;
}

static void M_LoadSelect (const FSaveGameNode *file)
{
	G_LoadGame (file->Filename.GetChars());
	if (gamestate == GS_FULLCONSOLE)
	{
		gamestate = GS_HIDECONSOLE;
	}
	if (quickSaveSlot == (FSaveGameNode *)1)
	{
		quickSaveSlot = SelSaveGame;
	}
	M_ClearMenus ();
	BorderNeedRefresh = screen->GetPageCount ();
}


//
// User wants to save. Start string input for M_Responder
//
static void M_CancelSaveName ()
{
}

static void M_SaveSelect (const FSaveGameNode *file)
{
	// we are going to be intercepting all chars
	genStringEnter = 1;
	genStringEnd = M_DoSave;
	genStringCancel = M_CancelSaveName;
	genStringLen = SAVESTRINGSIZE-1;

	if (file != &NewSaveNode)
	{
		strcpy (savegamestring, file->Title);
	}
	else
	{
		// If we are naming a new save, don't start the cursor on "end".
		if (InputGridX == INPUTGRID_WIDTH - 1 && InputGridY == INPUTGRID_HEIGHT - 1)
		{
			InputGridX = 0;
			InputGridY = 0;
		}
		savegamestring[0] = 0;
	}
	saveCharIndex = strlen (savegamestring);
}

static void M_DeleteSaveResponse (int choice)
{
	M_ClearSaveStuff ();
	if (choice == 'y')
	{
		FSaveGameNode *next = static_cast<FSaveGameNode *>(SelSaveGame->Succ);
		if (next->Succ == NULL)
		{
			next = static_cast<FSaveGameNode *>(SelSaveGame->Pred);
			if (next->Pred == NULL)
			{
				next = NULL;
			}
		}

		remove (SelSaveGame->Filename.GetChars());
		M_UnloadSaveData ();
		SelSaveGame = M_RemoveSaveSlot (SelSaveGame);
		M_ExtractSaveData (SelSaveGame);
	}
}

//
// M_StartControlPanel
//
void M_StartControlPanel (bool makeSound, bool wantTop)
{
	// intro might call this repeatedly
	if (menuactive == MENU_On)
		return;

	for (int i = 0; i < NUM_MKEYS; ++i)
	{
		MenuButtons[i].ReleaseKey(0);
	}
	drawSkull = true;
	MenuStackDepth = 0;
	if (wantTop)
	{
		M_SetupNextMenu(TopLevelMenu);
	}
	else
	{
		// Just a default. The caller ought to call M_SetupNextMenu() next.
		currentMenu = TopLevelMenu;
		itemOn = currentMenu->lastOn;
	}
	C_HideConsole ();				// [RH] Make sure console goes bye bye.
	OptionsActive = false;			// [RH] Make sure none of the options menus appear.
	M_ActivateMenuInput ();

	if (makeSound)
	{
		S_Sound (CHAN_VOICE | CHAN_UI, "menu/activate", 1, ATTN_NONE);
	}

	// [BB] Don't change the displayed console status when a demo is played.
	if ( CLIENTDEMO_IsPlaying( ) == false )
		players[consoleplayer].bInConsole = true;

	// [RC] Tell the server so we get an "in console" icon.
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
		CLIENTCOMMANDS_EnterConsole( );
}


//
// M_Drawer
// Called after the view has been rendered,
// but before it has been blitted.
//
void M_Drawer ()
{
	int i, x, y, max;
	PalEntry fade = 0;

	player_t *player = &players[consoleplayer];
	AActor *camera = player->camera;

	if (!screen->Accel2D && camera != NULL && (gamestate == GS_LEVEL || gamestate == GS_TITLELEVEL))
	{
		if (camera->player != NULL)
		{
			player = camera->player;
		}
		fade = PalEntry (BYTE(player->BlendA*255), BYTE(player->BlendR*255), BYTE(player->BlendG*255), BYTE(player->BlendB*255));
	}

	// Horiz. & Vertically center string and print it.
	if (messageToPrint)
	{
		int fontheight = SmallFont->GetHeight();
		screen->Dim (fade);
		BorderNeedRefresh = screen->GetPageCount ();
		SB_state = screen->GetPageCount ();

		FBrokenLines *lines = V_BreakLines (SmallFont, 320, messageString);
		y = 100;

		for (i = 0; lines[i].Width >= 0; i++)
			y -= SmallFont->GetHeight () / 2;

		for (i = 0; lines[i].Width >= 0; i++)
		{
			screen->DrawText (SmallFont, CR_UNTRANSLATED, 160 - lines[i].Width/2, y, lines[i].Text,
				DTA_Clean, true, TAG_DONE);
			y += fontheight;
		}
		V_FreeBrokenLines (lines);
		if (messageRoutine != NULL)
		{
			y += fontheight;
			screen->DrawText(SmallFont, CR_UNTRANSLATED, 160, y, GStrings["TXT_YES"], DTA_Clean, true, TAG_DONE);
			screen->DrawText(SmallFont, CR_UNTRANSLATED, 160, y + fontheight + 1, GStrings["TXT_NO"], DTA_Clean, true, TAG_DONE);
			if (skullAnimCounter < 6)
			{
				// [BB] Using Skulltag's increased precision of CleanXfac/CleanYfac is wrong here.
				screen->DrawText(ConFont, CR_RED,
					(150 - 160) * static_cast<int>(CleanXfac) + screen->GetWidth() / 2,
					(y + (fontheight + 1) * messageSelection - 100) * static_cast<int>(CleanYfac) + screen->GetHeight() / 2,
					"\xd",
					DTA_CellX, 8 * static_cast<int>(CleanXfac),
					DTA_CellY, 8 * static_cast<int>(CleanYfac),
					TAG_DONE);
			}
		}
	}
	else if (menuactive != MENU_Off)
	{
		if (InfoType == 0 && !OptionsActive)
		{
			screen->Dim (fade);
		}
		// For Heretic shareware message:
		if (showSharewareMessage)
		{
			const char *text = GStrings("MNU_ONLYREGISTERED");
			screen->DrawText (SmallFont, CR_WHITE, 160 - SmallFont->StringWidth(text)/2,
				8, text, DTA_Clean, true, TAG_DONE);
		}

		BorderNeedRefresh = screen->GetPageCount ();
		SB_state = screen->GetPageCount ();

		if (OptionsActive)
		{
			M_OptDrawer ();
		}
		else
		{
			if (currentMenu->routine)
				currentMenu->routine(); 		// call Draw routine

			// DRAW MENU
			x = currentMenu->x;
			y = currentMenu->y;
			max = currentMenu->numitems;

			for (i = 0; i < max; i++)
			{
				if (currentMenu->menuitems[i].name)
				{
					if (currentMenu->menuitems[i].fulltext)
					{
						int color = currentMenu->menuitems[i].textcolor;
						if (color == CR_UNTRANSLATED)
						{
							// The default DBIGFONT is white but Doom's default should be red.
							if (gameinfo.gametype & GAME_DoomChex)
							{
								color = CR_RED;
							}
						}
						const char *text = currentMenu->menuitems[i].name;
						if (*text == '$') text = GStrings(text+1);
						screen->DrawText (BigFont, color, x, y, text,
							DTA_Clean, true, TAG_DONE);
					}
					else
					{
						screen->DrawTexture (TexMan[currentMenu->menuitems[i].name], x, y,
							DTA_Clean, true, TAG_DONE);
					}
				}
				y += LINEHEIGHT;
			}
			
			// DRAW CURSOR
			if (drawSkull)
			{
/*
				if (currentMenu == &PSetupDef)
				{
					// [RH] Use options menu cursor for the player setup menu.
					if (skullAnimCounter < 6)
					{
						double item;
						// The green slider is halfway between lines, and the red and
						// blue ones are offset slightly to make room for it.
						if (itemOn < 3)
						{
							item = itemOn;
						}
						else if (itemOn > 5)
						{
							item = itemOn - 1;
						}
						else if (itemOn == 3)
						{
							item = 2.875;
						}
						else if (itemOn == 4)
						{
							item = 3.5;
						}
						else
						{
							item = 4.125;
						}
						screen->DrawText (ConFont, CR_RED, x - 16,
							currentMenu->y + int(item*PLAYERSETUP_LINEHEIGHT) +
							(!(gameinfo.gametype & (GAME_DoomStrifeChex)) ? 6 : -1), "\xd",
							DTA_Clean, true, TAG_DONE);
					}
				}
				else*/ if (gameinfo.gametype & GAME_DoomChex)
				{
					screen->DrawTexture (TexMan("M_SKULL1"),
						x + SKULLXOFF, currentMenu->y - 5 + itemOn*LINEHEIGHT,
						DTA_Clean, true, TAG_DONE);
				}
				else if (gameinfo.gametype == GAME_Strife)
				{
					screen->DrawTexture (TexMan("M_CURS1"),
						x - 28, currentMenu->y - 5 + itemOn*LINEHEIGHT,
						DTA_Clean, true, TAG_DONE);
				}
				else
				{
					screen->DrawTexture (TexMan("M_SLCTR1"),
						x + SELECTOR_XOFFSET,
						currentMenu->y + itemOn*LINEHEIGHT + SELECTOR_YOFFSET,
						DTA_Clean, true, TAG_DONE);
				}
			}
		}
		if (genStringEnter && InputGridOkay)
		{
			M_DrawInputGrid();
		}
	}
}


static void M_ClearSaveStuff ()
{
	M_UnloadSaveData ();
	if (SaveGames.Head == &NewSaveNode)
	{
		SaveGames.RemHead ();
		if (SelSaveGame == &NewSaveNode)
		{
			SelSaveGame = static_cast<FSaveGameNode *>(SaveGames.Head);
		}
		if (TopSaveGame == &NewSaveNode)
		{
			TopSaveGame = static_cast<FSaveGameNode *>(SaveGames.Head);
		}
	}
	if (quickSaveSlot == (FSaveGameNode *)1)
	{
		quickSaveSlot = NULL;
	}
}

static void M_DrawInputGrid()
{
	const int cell_width = 18 * CleanXfac;
	const int cell_height = 12 * CleanYfac;
	const int top_padding = cell_height / 2 - SmallFont->GetHeight() * CleanYfac / 2;

	// Darken the background behind the character grid.
	// Unless we frame it with a border, I think it looks better to extend the
	// background across the full width of the screen.
	screen->Dim(0, 0.8f,
		0 /*screen->GetWidth()/2 - 13 * cell_width / 2*/,
		screen->GetHeight() - 5 * cell_height,
		screen->GetWidth() /*13 * cell_width*/,
		5 * cell_height);

	// Highlight the background behind the selected character.
	screen->Dim(MAKERGB(255,248,220), 0.6f,
		InputGridX * cell_width - INPUTGRID_WIDTH * cell_width / 2 + screen->GetWidth() / 2,
		InputGridY * cell_height - INPUTGRID_HEIGHT * cell_height + screen->GetHeight(),
		cell_width, cell_height);

	for (int y = 0; y < INPUTGRID_HEIGHT; ++y)
	{
		const int yy = y * cell_height - INPUTGRID_HEIGHT * cell_height + screen->GetHeight();
		for (int x = 0; x < INPUTGRID_WIDTH; ++x)
		{
			int width;
			const int xx = x * cell_width - INPUTGRID_WIDTH * cell_width / 2 + screen->GetWidth() / 2;
			const int ch = InputGridChars[y * INPUTGRID_WIDTH + x];
			FTexture *pic = SmallFont->GetChar(ch, &width);
			EColorRange color;
			FRemapTable *remap;

			// The highlighted character is yellow; the rest are dark gray.
			color = (x == InputGridX && y == InputGridY) ? CR_YELLOW : CR_DARKGRAY;
			remap = SmallFont->GetColorTranslation(color);

			if (pic != NULL)
			{
				// Draw a normal character.
				screen->DrawTexture(pic, xx + cell_width/2 - width*CleanXfac/2, yy + top_padding,
					DTA_Translation, remap,
					DTA_CleanNoMove, true,
					TAG_DONE);
			}
			else if (ch == ' ')
			{
				// Draw the space as a box outline. We also draw it 50% wider than it really is.
				const int x1 = xx + cell_width/2 - width * CleanXfac * 3 / 4;
				const int x2 = x1 + width * 3 * CleanXfac / 2;
				const int y1 = yy + top_padding;
				const int y2 = y1 + SmallFont->GetHeight() * CleanYfac;
				const int palentry = remap->Remap[remap->NumEntries*2/3];
				const uint32 palcolor = remap->Palette[remap->NumEntries*2/3];
				screen->Clear(x1, y1, x2, y1+CleanYfac, palentry, palcolor);	// top
				screen->Clear(x1, y2, x2, y2+CleanYfac, palentry, palcolor);	// bottom
				screen->Clear(x1, y1+CleanYfac, x1+CleanXfac, y2, palentry, palcolor);	// left
				screen->Clear(x2-CleanXfac, y1+CleanYfac, x2, y2, palentry, palcolor);	// right
			}
			else if (ch == '\b' || ch == 0)
			{
				// Draw the backspace and end "characters".
				const char *const str = ch == '\b' ? "BS" : "ED";
				screen->DrawText(SmallFont, color,
					xx + cell_width/2 - SmallFont->StringWidth(str)*CleanXfac/2,
					yy + top_padding, str, DTA_CleanNoMove, true, TAG_DONE);
			}
		}
	}
}

//
// M_ClearMenus
//
void M_ClearMenus ()
{
	if (FireTexture)
	{
		delete FireTexture;
		FireTexture = NULL;
	}
	M_ClearSaveStuff ();
	M_DeactivateMenuInput ();
	MenuStackDepth = 0;
	OptionsActive = false;
	InfoType = 0;
	drawSkull = true;
	M_DemoNoPlay = false;
	BorderNeedRefresh = screen->GetPageCount ();

	// [BB] Don't change the displayed console status when a demo is played.
	if ( CLIENTDEMO_IsPlaying( ) == false )
		players[consoleplayer].bInConsole = false;

	// [RC] Tell the server so our "in console" icon is removed.
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
		CLIENTCOMMANDS_ExitConsole( );
}




//
// M_SetupNextMenu
//
void M_SetupNextMenu (oldmenu_t *menudef)
{
	MenuStack[MenuStackDepth].menu.old = menudef;
	MenuStack[MenuStackDepth].isNewStyle = false;
	MenuStack[MenuStackDepth].drawSkull = drawSkull;
	MenuStackDepth++;

	currentMenu = menudef;
	itemOn = currentMenu->lastOn;
}


void M_PopMenuStack (void)
{
	M_DemoNoPlay = false;
	InfoType = 0;
	M_ClearSaveStuff ();
	flagsvar = 0;
	if (MenuStackDepth > 1)
	{
		MenuStackDepth -= 2;
		if (MenuStack[MenuStackDepth].isNewStyle)
		{
			OptionsActive = true;
			CurrentMenu = MenuStack[MenuStackDepth].menu.newmenu;
			CurrentItem = CurrentMenu->lastOn;
		}
		else
		{
			OptionsActive = false;
			currentMenu = MenuStack[MenuStackDepth].menu.old;
			itemOn = currentMenu->lastOn;
		}
		drawSkull = MenuStack[MenuStackDepth].drawSkull;
		++MenuStackDepth;
		S_Sound (CHAN_VOICE | CHAN_UI, "menu/backup", 1, ATTN_NONE);
	}
	else
	{
		M_ClearMenus ();
		S_Sound (CHAN_VOICE | CHAN_UI, "menu/clear", 1, ATTN_NONE);
	}
}


//
// M_Ticker
//
void M_Ticker (void)
{
	if (showSharewareMessage)
	{
		--showSharewareMessage;
	}
	if (menuactive == MENU_Off)
	{
		return;
	}
	MenuTime++;
	if (--skullAnimCounter <= 0)
	{
		skullAnimCounter = 8;
	}
	//if (currentMenu == &PSetupDef || currentMenu == &ClassMenuDef)
	if ( CurrentMenu == &PlayerSetupMenu || currentMenu == &ClassMenuDef )
	{
		M_PlayerSetupTicker();
	}

	for (int i = 0; i < NUM_MKEYS; ++i)
	{
		if (MenuButtons[i].bDown)
		{
			if (MenuButtonTickers[i] > 0 &&	--MenuButtonTickers[i] <= 0)
			{
				MenuButtonTickers[i] = KEY_REPEAT_RATE;
				M_ButtonHandler(EMenuKey(i), true);
			}
		}
	}
}


//
// M_Init
//
EXTERN_CVAR (Int, screenblocks)

void M_Init (void)
{
	unsigned int i;

	atterm (M_Deinit);

	if (gameinfo.gametype & (GAME_DoomStrifeChex))
	{
		TopLevelMenu = currentMenu = &MainDef;
		if (gameinfo.gametype == GAME_Strife)
		{
			MainDef.y = 45;
			//NewDef.lastOn = 1;
		}
	}
	else
	{
		TopLevelMenu = currentMenu = &HereticMainDef;
//		PSetupDef.y -= 7;
		LoadDef.y -= 20;
		SaveDef.y -= 20;
	}
	PickPlayerClass ();
	OptionsActive = false;
	menuactive = MENU_Off;
	InfoType = 0;
	itemOn = currentMenu->lastOn;
	skullAnimCounter = 10;
	drawSkull = true;
	messageToPrint = 0;
	messageString = NULL;
	messageLastMenuActive = menuactive;
	quickSaveSlot = NULL;
	lastSaveSlot = NULL;
	strcpy (NewSaveNode.Title, "<New Save Game>");

	underscore[0] = (gameinfo.gametype & (GAME_DoomStrifeChex)) ? '_' : '[';
	underscore[1] = '\0';

	if (gameinfo.gametype & GAME_DoomChex)
	{
		LINEHEIGHT = 16;
	}
	else if (gameinfo.gametype == GAME_Strife)
	{
		LINEHEIGHT = 19;
	}
	else
	{
		LINEHEIGHT = 20;
	}

	if (!gameinfo.drawreadthis)
	{
		MainMenu[MainDef.numitems-2] = MainMenu[MainDef.numitems-1];
		MainDef.numitems--;
		MainDef.y += 8;
		ReadDef.routine = M_DrawReadThis;
		ReadDef.x = 330;
		ReadDef.y = 165;
		//ReadMenu[0].routine = M_FinishReadThis;
	}
	M_OptInit ();

	// [GRB] Set up player class menu
	if (!(gameinfo.gametype == GAME_Hexen && PlayerClasses.Size () == 3 &&
		PlayerClasses[0].Type->IsDescendantOf (PClass::FindClass (NAME_FighterPlayer)) &&
		PlayerClasses[1].Type->IsDescendantOf (PClass::FindClass (NAME_ClericPlayer)) &&
		PlayerClasses[2].Type->IsDescendantOf (PClass::FindClass (NAME_MagePlayer))))
	{
		int n = 0;

		for (i = 0; i < PlayerClasses.Size () && n < 7; i++)
		{
			if (!(PlayerClasses[i].Flags & PCF_NOMENU))
			{
				ClassMenuItems[n].name =
					PlayerClasses[i].Type->Meta.GetMetaString (APMETA_DisplayName);
				n++;
			}
		}

		if (n > 1)
		{
			ClassMenuItems[n].name = "Random";
			ClassMenuDef.numitems = n+1;
		}
		else
		{
			if (n == 0)
			{
				ClassMenuItems[0].name =
					PlayerClasses[0].Type->Meta.GetMetaString (APMETA_DisplayName);
			}
			ClassMenuDef.numitems = 1;
		}

		if (gameinfo.gametype & (GAME_DoomStrifeChex))
		{
			ClassMenuDef.x = 48;
			ClassMenuDef.y = 63;
		}
		else
		{
			ClassMenuDef.x = 80;
			ClassMenuDef.y = 50;
		}
		if (ClassMenuDef.numitems > 4)
		{
			ClassMenuDef.y -= LINEHEIGHT;
		}
	}

	// [RH] Build a palette translation table for the player setup effect
	if (gameinfo.gametype != GAME_Hexen)
	{
		for (i = 0; i < 256; i++)
		{
			FireRemap.Remap[i] = ColorMatcher.Pick (i/2+32, 0, i/4);
			FireRemap.Palette[i] = PalEntry(255, i/2+32, 0, i/4);
		}
	}
	else
	{ // The reddish color ramp above doesn't look too good with the
	  // Hexen palette, so Hexen gets a greenish one instead.
		for (i = 0; i < 256; ++i)
		{
			FireRemap.Remap[i] = ColorMatcher.Pick (i/4, i*13/40+7, i/4);
			FireRemap.Palette[i] = PalEntry(255, i/4, i*13/40+7, i/4);
		}
	}
}

static void PickPlayerClass ()
{
	int pclass = 0;

	// [GRB] Pick a class from player class list
	if (PlayerClasses.Size () > 1)
	{
		pclass = g_lPlayerSetupClass;

		if (pclass < 0)
		{
			pclass = (MenuTime>>7) % PlayerClasses.Size ();
		}
	}

	PlayerClass = &PlayerClasses[pclass];
	P_EnumPlayerColorSets(PlayerClass->Type->TypeName, &PlayerColorSets);
}
