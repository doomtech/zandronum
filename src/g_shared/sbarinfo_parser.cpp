/*
** sbarinfo_parser.cpp
**
** Reads custom status bar definitions.
**
**---------------------------------------------------------------------------
** Copyright 2008 Braden Obrzut
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

#include "doomtype.h"
#include "doomstat.h"
#include "sc_man.h"
#include "v_font.h"
#include "w_wad.h"
#include "d_player.h"
#include "sbar.h"
#include "sbarinfo.h"
#include "templates.h"
#include "m_random.h"
#include "gi.h"
#include "i_system.h"
#include "g_level.h"
#include "p_acs.h"
//[BL] New Includes
#include "teaminfo.h"

SBarInfo *SBarInfoScript[2] = {NULL,NULL};

static const char *SBarInfoTopLevel[] =
{
	"base",
	"height",
	"interpolatehealth",
	"interpolatearmor",
	"completeborder",
	"monospacefonts",
	"lowerhealthcap",
	"statusbar",
	"mugshot",
	"createpopup",
	NULL
};

static const char *StatusBars[] =
{
	"none",
	"fullscreen",
	"normal",
	"automap",
	"inventory",
	"inventoryfullscreen",
	"popuplog",
	"popupkeys",
	"popupstatus",
	NULL
};

static const char *SBarInfoRoutineLevel[] =
{
	"drawimage",
	"drawnumber",
	"drawswitchableimage",
	"drawmugshot",
	"drawselectedinventory",
	"drawinventorybar",
	"drawbar",
	"drawgem",
	"drawshader",
	"drawstring",
	"drawkeybar",
	"gamemode",
	"playerclass",
	"aspectratio",
	"isselected",
	"usesammo",
	"usessecondaryammo",
	"hasweaponpiece",
	"inventorybarnotvisible",
	"weaponammo", //event
	"ininventory",
	NULL
};

static void FreeSBarInfoScript()
{
	for(int i = 0;i < 2;i++)
	{
		if (SBarInfoScript[i] != NULL)
		{
			delete SBarInfoScript[i];
			SBarInfoScript[i] = NULL;
		}
	}
}

void SBarInfo::Load()
{
	if(gameinfo.statusbar.IsNotEmpty())
	{
		int lump = Wads.CheckNumForFullName(gameinfo.statusbar, true);
		if(lump != -1)
		{
			Printf ("ParseSBarInfo: Loading default status bar definition.\n");
			if(SBarInfoScript[SCRIPT_DEFAULT] == NULL)
				SBarInfoScript[SCRIPT_DEFAULT] = new SBarInfo(lump);
			else
				SBarInfoScript[SCRIPT_DEFAULT]->ParseSBarInfo(lump);
		}
	}

	if(Wads.CheckNumForName("SBARINFO") != -1)
	{
		Printf ("ParseSBarInfo: Loading custom status bar definition.\n");
		int lastlump, lump;
		lastlump = 0;
		while((lump = Wads.FindLump("SBARINFO", &lastlump)) != -1)
		{
			if(SBarInfoScript[SCRIPT_CUSTOM] == NULL)
				SBarInfoScript[SCRIPT_CUSTOM] = new SBarInfo(lump);
			else //We now have to load multiple SBarInfo Lumps so the 2nd time we need to use this method instead.
				SBarInfoScript[SCRIPT_CUSTOM]->ParseSBarInfo(lump);
		}
	}
	atterm(FreeSBarInfoScript);
}

//SBarInfo Script Reader
void SBarInfo::ParseSBarInfo(int lump)
{
	gameType = gameinfo.gametype;
	bool baseSet = false;
	FScanner sc(lump);
	sc.SetCMode(true);
	while(sc.CheckToken(TK_Identifier) || sc.CheckToken(TK_Include))
	{
		if(sc.TokenType == TK_Include)
		{
			sc.MustGetToken(TK_StringConst);
			int lump = Wads.CheckNumForFullName(sc.String, true);
			if (lump == -1)
				sc.ScriptError("Lump '%s' not found", sc.String);
			ParseSBarInfo(lump);
			continue;
		}
		switch(sc.MustMatchString(SBarInfoTopLevel))
		{
			case SBARINFO_BASE:
				baseSet = true;
				if(!sc.CheckToken(TK_None))
					sc.MustGetToken(TK_Identifier);
				if(sc.Compare("Doom"))
				{
					int lump = Wads.CheckNumForFullName("sbarinfo/doom.txt", true);
					if(lump == -1)
						sc.ScriptError("Standard Doom Status Bar not found.");
					ParseSBarInfo(lump);
				}
				else if(sc.Compare("Heretic"))
					gameType = GAME_Heretic;
				else if(sc.Compare("Hexen"))
					gameType = GAME_Hexen;
				else if(sc.Compare("Strife"))
					gameType = GAME_Strife;
				else if(sc.Compare("None"))
					gameType = GAME_Any;
				else
					sc.ScriptError("Bad game name: %s", sc.String);
				sc.MustGetToken(';');
				break;
			case SBARINFO_HEIGHT:
				sc.MustGetToken(TK_IntConst);
				this->height = sc.Number;
				sc.MustGetToken(';');
				break;
			case SBARINFO_INTERPOLATEHEALTH: //mimics heretics interpolated health values.
				if(sc.CheckToken(TK_True))
				{
					interpolateHealth = true;
				}
				else
				{
					sc.MustGetToken(TK_False);
					interpolateHealth = false;
				}
				if(sc.CheckToken(',')) //speed param
				{
					sc.MustGetToken(TK_IntConst);
					this->interpolationSpeed = sc.Number;
				}
				sc.MustGetToken(';');
				break;
			case SBARINFO_INTERPOLATEARMOR: //Since interpolatehealth is such a popular command
				if(sc.CheckToken(TK_True))
				{
					interpolateArmor = true;
				}
				else
				{
					sc.MustGetToken(TK_False);
					interpolateArmor = false;
				}
				if(sc.CheckToken(',')) //speed
				{
					sc.MustGetToken(TK_IntConst);
					this->armorInterpolationSpeed = sc.Number;
				}
				sc.MustGetToken(';');
				break;
			case SBARINFO_COMPLETEBORDER: //draws the border instead of an HOM
				if(sc.CheckToken(TK_True))
				{
					completeBorder = true;
				}
				else
				{
					sc.MustGetToken(TK_False);
					completeBorder = false;
				}
				sc.MustGetToken(';');
				break;
			case SBARINFO_MONOSPACEFONTS:
				if(sc.CheckToken(TK_True))
				{
					sc.MustGetToken(',');
					sc.MustGetToken(TK_StringConst);
					spacingCharacter = sc.String[0];
				}
				else
				{
					sc.MustGetToken(TK_False);
					spacingCharacter = '\0';
					sc.MustGetToken(',');
					sc.MustGetToken(TK_StringConst); //Don't tell anyone we're just ignoring this ;)
				}
				sc.MustGetToken(';');
				break;
			case SBARINFO_LOWERHEALTHCAP:
				if(sc.CheckToken(TK_False))
				{
					lowerHealthCap = false;
				}
				else
				{
					sc.MustGetToken(TK_True);
					lowerHealthCap = true;
				}
				sc.MustGetToken(';');
				break;
			case SBARINFO_STATUSBAR:
			{
				if(!baseSet) //If the user didn't explicitly define a base, do so now.
					gameType = GAME_Any;
				int barNum = 0;
				if(!sc.CheckToken(TK_None))
				{
					sc.MustGetToken(TK_Identifier);
					barNum = sc.MustMatchString(StatusBars);
				}
				this->huds[barNum] = SBarInfoBlock();
				if(sc.CheckToken(','))
				{
					while(sc.CheckToken(TK_Identifier))
					{
						if(sc.Compare("forcescaled"))
						{
							this->huds[barNum].forceScaled = true;
						}
						else if(sc.Compare("fullscreenoffsets"))
						{
							this->huds[barNum].fullScreenOffsets = true;
						}
						else
						{
							sc.ScriptError("Unkown flag '%s'.", sc.String);
						}
						if(!sc.CheckToken('|') && !sc.CheckToken(','))
							goto FinishStatusBar; //No more args so we must skip over anything else and go to the end.
					}
					sc.MustGetToken(TK_FloatConst);
					this->huds[barNum].alpha = fixed_t(FRACUNIT * sc.Float);
				}
			FinishStatusBar:
				sc.MustGetToken('{');
				if(barNum == STBAR_AUTOMAP)
				{
					automapbar = true;
				}
				ParseSBarInfoBlock(sc, this->huds[barNum]);
				break;
			}
			case SBARINFO_MUGSHOT:
			{
				sc.MustGetToken(TK_StringConst);
				FMugShotState state(sc.String);
				if(sc.CheckToken(',')) //first loop must be a comma
				{
					do
					{
						sc.MustGetToken(TK_Identifier);
						if(sc.Compare("health"))
							state.bUsesLevels = true;
						else if(sc.Compare("health2"))
							state.bUsesLevels = state.bHealth2 = true;
						else if(sc.Compare("healthspecial"))
							state.bUsesLevels = state.bHealthSpecial = true;
						else if(sc.Compare("directional"))
							state.bDirectional = true;
						else
							sc.ScriptError("Unknown MugShot state flag '%s'.", sc.String);
					}
					while(sc.CheckToken(',') || sc.CheckToken('|'));
				}
				ParseMugShotBlock(sc, state);
				int index;
				if((index = FindMugShotStateIndex(state.State)) != -1) //We already had this state, remove the old one.
				{
					MugShotStates.Delete(index);
				}
				MugShotStates.Push(state);
				break;
			}
			case SBARINFO_CREATEPOPUP:
			{
				int pop = 0;
				sc.MustGetToken(TK_Identifier);
				if(sc.Compare("log"))
					pop = 0;
				else if(sc.Compare("keys"))
					pop = 1;
				else if(sc.Compare("status"))
					pop = 2;
				else
					sc.ScriptError("Unkown popup: '%s'", sc.String);
				Popup &popup = popups[pop];
				sc.MustGetToken(',');
				sc.MustGetToken(TK_IntConst);
				popup.width = sc.Number;
				sc.MustGetToken(',');
				sc.MustGetToken(TK_IntConst);
				popup.height = sc.Number;
				sc.MustGetToken(',');
				if(!sc.CheckToken(TK_None))
				{
					sc.MustGetToken(TK_Identifier);
					if(sc.Compare("slideinbottom"))
					{
						popup.transition = TRANSITION_SLIDEINBOTTOM;
						sc.MustGetToken(',');
						sc.MustGetToken(TK_IntConst);
						popup.speed = sc.Number;
					}
					else if(sc.Compare("fade"))
					{
						popup.transition = TRANSITION_FADE;
						sc.MustGetToken(',');
						sc.MustGetToken(TK_FloatConst);
						popup.speed = fixed_t(FRACUNIT * (1.0 / (35.0 * sc.Float)));
						sc.MustGetToken(',');
						sc.MustGetToken(TK_FloatConst);
						popup.speed2 = fixed_t(FRACUNIT * (1.0 / (35.0 * sc.Float)));
					}
					else
						sc.ScriptError("Unkown transition type: '%s'", sc.String);
				}
				popup.init();
				sc.MustGetToken(';');
				break;
			}
		}
	}
}

void SBarInfo::ParseSBarInfoBlock(FScanner &sc, SBarInfoBlock &block)
{
	while(sc.CheckToken(TK_Identifier))
	{
		SBarInfoCommand cmd;

		switch(cmd.type = sc.MustMatchString(SBarInfoRoutineLevel))
		{
			case SBARINFO_DRAWSWITCHABLEIMAGE:
				sc.MustGetToken(TK_Identifier);
				if(sc.Compare("weaponslot"))
				{
					cmd.flags = DRAWIMAGE_WEAPONSLOT;
					sc.MustGetToken(TK_IntConst);
					cmd.value = sc.Number;
				}
				else if(sc.Compare("invulnerable"))
				{
					cmd.flags = DRAWIMAGE_INVULNERABILITY;
				}
				else if(sc.Compare("keyslot"))
				{
					cmd.flags = DRAWIMAGE_KEYSLOT;
					sc.MustGetToken(TK_IntConst);
					cmd.value = sc.Number;
				}
				else
				{
					cmd.setString(sc, sc.String, 0);
					const PClass* item = PClass::FindClass(sc.String);
					if(item == NULL || !PClass::FindClass("Inventory")->IsAncestorOf(item)) //must be a kind of Inventory
					{
						sc.ScriptError("'%s' is not a type of inventory item.", sc.String);
					}
				}
				if(sc.CheckToken(TK_AndAnd))
				{
					cmd.flags |= DRAWIMAGE_SWITCHABLE_AND;
					if(cmd.flags & DRAWIMAGE_KEYSLOT)
					{
						sc.MustGetToken(TK_IntConst);
						cmd.special4 = sc.Number;
					}
					else
					{
						sc.MustGetToken(TK_Identifier);
						cmd.setString(sc, sc.String, 1);
						const PClass* item = PClass::FindClass(sc.String);
						if(item == NULL || !PClass::FindClass("Inventory")->IsAncestorOf(item)) //must be a kind of Inventory
						{
							sc.ScriptError("'%s' is not a type of inventory item.", sc.String);
						}
					}
					sc.MustGetToken(',');
					sc.MustGetToken(TK_StringConst);
					cmd.special = newImage(sc.String);
					sc.MustGetToken(',');
					sc.MustGetToken(TK_StringConst);
					cmd.special2 = newImage(sc.String);
					sc.MustGetToken(',');
					sc.MustGetToken(TK_StringConst);
					cmd.special3 = newImage(sc.String);
					sc.MustGetToken(',');
				}
				else
				{
					sc.MustGetToken(',');
					sc.MustGetToken(TK_StringConst);
					cmd.special = newImage(sc.String);
					sc.MustGetToken(',');
				}
			case SBARINFO_DRAWIMAGE:
			{
				bool getImage = true;
				if(sc.CheckToken(TK_Identifier))
				{
					getImage = false;
					if(sc.Compare("playericon"))
						cmd.flags |= DRAWIMAGE_PLAYERICON;
					else if(sc.Compare("ammoicon1"))
						cmd.flags |= DRAWIMAGE_AMMO1;
					else if(sc.Compare("ammoicon2"))
						cmd.flags |= DRAWIMAGE_AMMO2;
					else if(sc.Compare("armoricon"))
						cmd.flags |= DRAWIMAGE_ARMOR;
					else if(sc.Compare("weaponicon"))
						cmd.flags |= DRAWIMAGE_WEAPONICON;
					else if(sc.Compare("sigil"))
						cmd.flags |= DRAWIMAGE_SIGIL;
					else if(sc.Compare("hexenarmor"))
					{
						cmd.flags = DRAWIMAGE_HEXENARMOR;
						sc.MustGetToken(TK_Identifier);
						if(sc.Compare("armor"))
							cmd.value = 0;
						else if(sc.Compare("shield"))
							cmd.value = 1;
						else if(sc.Compare("helm"))
							cmd.value = 2;
						else if(sc.Compare("amulet"))
							cmd.value = 3;
						else
							sc.ScriptError("Unkown armor type: '%s'", sc.String);
						sc.MustGetToken(',');
						getImage = true;
					}
					else if(sc.Compare("runeicon"))
						cmd.flags |= DRAWIMAGE_RUNEICON;
					else if(sc.Compare("translatable"))
					{
						cmd.flags |= DRAWIMAGE_TRANSLATABLE;
						getImage = true;
					}
					else
					{
						//sc.CheckToken(TK_Identifier);
						cmd.flags |= DRAWIMAGE_INVENTORYICON;
						const PClass* item = PClass::FindClass(sc.String);
						if(item == NULL || !PClass::FindClass("Inventory")->IsAncestorOf(item)) //must be a kind of Inventory
						{
							sc.ScriptError("'%s' is not a type of inventory item.", sc.String);
						}
						cmd.sprite_index = ((AInventory *)GetDefaultByType(item))->Icon;
						cmd.image_index = -1;
					}
				}
				if(getImage)
				{
					sc.MustGetToken(TK_StringConst);
					cmd.image_index = newImage(sc.String);
					cmd.sprite_index.SetInvalid();
				}
				sc.MustGetToken(',');
				this->getCoordinates(sc, block.fullScreenOffsets, cmd.x, cmd.y);
				if(sc.CheckToken(','))
				{
					sc.MustGetToken(TK_Identifier);
					if(sc.Compare("center"))
						cmd.flags |= DRAWIMAGE_OFFSET_CENTER;
					else if(sc.Compare("centerbottom"))
						cmd.flags |= DRAWIMAGE_OFFSET_CENTERBOTTOM;
					else
						sc.ScriptError("'%s' is not a valid alignment.", sc.String);
				}
				sc.MustGetToken(';');
				break;
			}
			case SBARINFO_DRAWNUMBER:
				cmd.special4 = cmd.special3 = -1;
				sc.MustGetToken(TK_IntConst);
				cmd.special = sc.Number;
				sc.MustGetToken(',');
				sc.MustGetToken(TK_Identifier);
				cmd.font = V_GetFont(sc.String);
				if(cmd.font == NULL)
					sc.ScriptError("Unknown font '%s'.", sc.String);
				sc.MustGetToken(',');
				sc.MustGetToken(TK_Identifier);
				cmd.translation = this->GetTranslation(sc, sc.String);
				sc.MustGetToken(',');
				if(sc.CheckToken(TK_IntConst))
				{
					cmd.value = sc.Number;
					sc.MustGetToken(',');
				}
				else
				{
					sc.MustGetToken(TK_Identifier);
					if(sc.Compare("health"))
						cmd.flags = DRAWNUMBER_HEALTH;
					else if(sc.Compare("armor"))
						cmd.flags = DRAWNUMBER_ARMOR;
					else if(sc.Compare("ammo1"))
						cmd.flags = DRAWNUMBER_AMMO1;
					else if(sc.Compare("ammo2"))
						cmd.flags = DRAWNUMBER_AMMO2;
					else if(sc.Compare("ammo")) //request the next string to be an ammo type
					{
						sc.MustGetToken(TK_Identifier);
						cmd.setString(sc, sc.String, 0);
						cmd.flags = DRAWNUMBER_AMMO;
						const PClass* ammo = PClass::FindClass(sc.String);
						if(ammo == NULL || !RUNTIME_CLASS(AAmmo)->IsAncestorOf(ammo)) //must be a kind of ammo
						{
							sc.ScriptError("'%s' is not a type of ammo.", sc.String);
						}
					}
					else if(sc.Compare("ammocapacity"))
					{
						sc.MustGetToken(TK_Identifier);
						cmd.setString(sc, sc.String, 0);
						cmd.flags = DRAWNUMBER_AMMOCAPACITY;
						const PClass* ammo = PClass::FindClass(sc.String);
						if(ammo == NULL || !RUNTIME_CLASS(AAmmo)->IsAncestorOf(ammo)) //must be a kind of ammo
						{
							sc.ScriptError("'%s' is not a type of ammo.", sc.String);
						}
					}
					else if(sc.Compare("frags"))
						cmd.flags = DRAWNUMBER_FRAGS;
					else if(sc.Compare("kills"))
						cmd.flags |= DRAWNUMBER_KILLS;
					else if(sc.Compare("monsters"))
						cmd.flags |= DRAWNUMBER_MONSTERS;
					else if(sc.Compare("items"))
						cmd.flags |= DRAWNUMBER_ITEMS;
					else if(sc.Compare("totalitems"))
						cmd.flags |= DRAWNUMBER_TOTALITEMS;
					else if(sc.Compare("secrets"))
						cmd.flags |= DRAWNUMBER_SECRETS;
					else if(sc.Compare("totalsecrets"))
						cmd.flags |= DRAWNUMBER_TOTALSECRETS;
					else if(sc.Compare("armorclass"))
						cmd.flags |= DRAWNUMBER_ARMORCLASS;
					else if(sc.Compare("airtime"))
						cmd.flags |= DRAWNUMBER_AIRTIME;
					else if(sc.Compare("globalvar"))
					{
						cmd.flags |= DRAWNUMBER_GLOBALVAR;
						sc.MustGetToken(TK_IntConst);
						if(sc.Number < 0 || sc.Number >= NUM_GLOBALVARS)
							sc.ScriptError("Global variable number out of range: %d", sc.Number);
						cmd.value = sc.Number;
					}
					else if(sc.Compare("globalarray")) //acts like variable[playernumber()]
					{
						cmd.flags |= DRAWNUMBER_GLOBALARRAY;
						sc.MustGetToken(TK_IntConst);
						if(sc.Number < 0 || sc.Number >= NUM_GLOBALVARS)
							sc.ScriptError("Global variable number out of range: %d", sc.Number);
						cmd.value = sc.Number;
					}
					else if(sc.Compare("poweruptime"))
					{
						cmd.flags |= DRAWNUMBER_POWERUPTIME;
						sc.MustGetToken(TK_Identifier);
						cmd.setString(sc, sc.String, 0);
						const PClass* item = PClass::FindClass(sc.String);
						if(item == NULL || !PClass::FindClass("PowerupGiver")->IsAncestorOf(item))
						{
							sc.ScriptError("'%s' is not a type of PowerupGiver.", sc.String);
						}
					}
					else if(sc.Compare("teamscore")) //Takes in a number for team
					{
						cmd.flags |= DRAWNUMBER_TEAMSCORE;
						sc.MustGetToken(TK_StringConst);
						int t = -1;
						for(unsigned int i = 0;i < teams.Size();i++)
						{
							if(teams[i].Name.CompareNoCase(sc.String) == 0)
							{
								t = (int) i;
								break;
							}
						}
						if(t == -1)
							sc.ScriptError("'%s' is not a valid team.", sc.String);
						cmd.value = t;
					}
					else
					{
						cmd.flags = DRAWNUMBER_INVENTORY;
						cmd.setString(sc, sc.String, 0);
						const PClass* item = PClass::FindClass(sc.String);
						if(item == NULL || !PClass::FindClass("Inventory")->IsAncestorOf(item)) //must be a kind of ammo
						{
							sc.ScriptError("'%s' is not a type of inventory item.", sc.String);
						}
					}
					sc.MustGetToken(',');
				}
				while(sc.CheckToken(TK_Identifier))
				{
					if(sc.Compare("fillzeros"))
						cmd.flags |= DRAWNUMBER_FILLZEROS;
					else if(sc.Compare("whennotzero"))
						cmd.flags |= DRAWNUMBER_WHENNOTZERO;
					else if(sc.Compare("drawshadow"))
						cmd.flags |= DRAWNUMBER_DRAWSHADOW;
					else
						sc.ScriptError("Unknown flag '%s'.", sc.String);
					if(!sc.CheckToken('|'))
						sc.MustGetToken(',');
				}
				this->getCoordinates(sc, block.fullScreenOffsets, cmd.x, cmd.y);
				if(sc.CheckToken(','))
				{
					bool needsComma = false;
					if(sc.CheckToken(TK_IntConst)) //font spacing
					{
						cmd.special2 = sc.Number;
						needsComma = true;
					}
					if(!needsComma || sc.CheckToken(',')) //2nd coloring for "low-on" value
					{
						sc.MustGetToken(TK_Identifier);
						cmd.translation2 = this->GetTranslation(sc, sc.String);
						sc.MustGetToken(',');
						sc.MustGetToken(TK_IntConst);
						cmd.special3 = sc.Number;
						if(sc.CheckToken(',')) //3rd coloring for "high-on" value
						{
							sc.MustGetToken(TK_Identifier);
							cmd.translation3 = this->GetTranslation(sc, sc.String);
							sc.MustGetToken(',');
							sc.MustGetToken(TK_IntConst);
							cmd.special4 = sc.Number;
						}
					}
				}
				sc.MustGetToken(';');
				break;
			case SBARINFO_DRAWMUGSHOT:
				if(sc.CheckToken(TK_StringConst))
				{
					cmd.setString(sc, sc.String, 0, 3, true);
					sc.MustGetToken(',');
				}
				sc.MustGetToken(TK_IntConst); //accuracy
				if(sc.Number < 1 || sc.Number > 9)
					sc.ScriptError("Expected a number between 1 and 9, got %d instead.", sc.Number);
				cmd.special = sc.Number;
				sc.MustGetToken(',');
				while(sc.CheckToken(TK_Identifier))
				{
					if(sc.Compare("xdeathface"))
						cmd.flags |= DRAWMUGSHOT_XDEATHFACE;
					else if(sc.Compare("animatedgodmode"))
						cmd.flags |= DRAWMUGSHOT_ANIMATEDGODMODE;
					else if(sc.Compare("disablegrin"))
						cmd.flags |= DRAWMUGSHOT_DISABLEGRIN;
					else if(sc.Compare("disableouch"))
						cmd.flags |= DRAWMUGSHOT_DISABLEOUCH;
					else if(sc.Compare("disablepain"))
						cmd.flags |= DRAWMUGSHOT_DISABLEPAIN;
					else if(sc.Compare("disablerampage"))
						cmd.flags |= DRAWMUGSHOT_DISABLERAMPAGE;
					else
						sc.ScriptError("Unknown flag '%s'.", sc.String);
					if(!sc.CheckToken('|'))
						sc.MustGetToken(',');
				}

				this->getCoordinates(sc, block.fullScreenOffsets, cmd.x, cmd.y);
				sc.MustGetToken(';');
				break;
			case SBARINFO_DRAWSELECTEDINVENTORY:
			{
				bool alternateonempty = false;
				while(true) //go until we get a font (non-flag)
				{
					sc.MustGetToken(TK_Identifier);
					if(sc.Compare("alternateonempty"))
					{
						alternateonempty = true;
						cmd.flags |= DRAWSELECTEDINVENTORY_ALTERNATEONEMPTY;
					}
					else if(sc.Compare("artiflash"))
					{
						cmd.flags |= DRAWSELECTEDINVENTORY_ARTIFLASH;
					}
					else if(sc.Compare("alwaysshowcounter"))
					{
						cmd.flags |= DRAWSELECTEDINVENTORY_ALWAYSSHOWCOUNTER;
					}
					else if(sc.Compare("center"))
					{
						cmd.flags |= DRAWSELECTEDINVENTORY_CENTER;
					}
					else if(sc.Compare("centerbottom"))
					{
						cmd.flags |= DRAWSELECTEDINVENTORY_CENTERBOTTOM;
					}
					else if(sc.Compare("drawshadow"))
					{
						cmd.flags |= DRAWSELECTEDINVENTORY_DRAWSHADOW;
					}
					else
					{
						cmd.font = V_GetFont(sc.String);
						if(cmd.font == NULL)
							sc.ScriptError("Unknown font '%s'.", sc.String);
						sc.MustGetToken(',');
						break;
					}
					if(!sc.CheckToken('|'))
						sc.MustGetToken(',');
				}
				this->getCoordinates(sc, block.fullScreenOffsets, cmd.x, cmd.y);
				cmd.special2 = *(cmd.x + 30);
				cmd.special3 = *(cmd.y + 24);
				cmd.translation = CR_GOLD;
				if(sc.CheckToken(',')) //more font information
				{
					this->getCoordinates(sc, block.fullScreenOffsets, cmd.special2, cmd.special3);
					if(sc.CheckToken(','))
					{
						sc.MustGetToken(TK_Identifier);
						cmd.translation = this->GetTranslation(sc, sc.String);
						if(sc.CheckToken(','))
						{
							sc.MustGetToken(TK_IntConst);
							cmd.special4 = sc.Number;
						}
					}
				}
				if(alternateonempty)
				{
					sc.MustGetToken('{');
					cmd.subBlock.fullScreenOffsets = block.fullScreenOffsets;
					this->ParseSBarInfoBlock(sc, cmd.subBlock);
				}
				else
				{
					sc.MustGetToken(';');
				}
				break;
			}
			case SBARINFO_DRAWINVENTORYBAR:
				sc.MustGetToken(TK_Identifier);
				if(sc.Compare("Doom"))
					cmd.special = GAME_Doom;
				else if(sc.Compare("Heretic"))
					cmd.special = GAME_Heretic;
				else if(sc.Compare("Hexen"))
					cmd.special = GAME_Hexen;
				else if(sc.Compare("Strife"))
					cmd.special = GAME_Strife;
				else
					sc.ScriptError("Unkown style '%s'.", sc.String);

				sc.MustGetToken(',');
				while(sc.CheckToken(TK_Identifier))
				{
					if(sc.Compare("alwaysshow"))
					{
						cmd.flags |= DRAWINVENTORYBAR_ALWAYSSHOW;
					}
					else if(sc.Compare("noartibox"))
					{
						cmd.flags |= DRAWINVENTORYBAR_NOARTIBOX;
					}
					else if(sc.Compare("noarrows"))
					{
						cmd.flags |= DRAWINVENTORYBAR_NOARROWS;
					}
					else if(sc.Compare("alwaysshowcounter"))
					{
						cmd.flags |= DRAWINVENTORYBAR_ALWAYSSHOWCOUNTER;
					}
					else if(sc.Compare("translucent"))
					{
						cmd.flags |= DRAWINVENTORYBAR_TRANSLUCENT;
					}
					else
					{
						sc.ScriptError("Unknown flag '%s'.", sc.String);
					}
					if(!sc.CheckToken('|'))
						sc.MustGetToken(',');
				}
				sc.MustGetToken(TK_IntConst);
				cmd.value = sc.Number;
				sc.MustGetToken(',');
				sc.MustGetToken(TK_Identifier);
				cmd.font = V_GetFont(sc.String);
				if(cmd.font == NULL)
					sc.ScriptError("Unknown font '%s'.", sc.String);

				sc.MustGetToken(',');
				this->getCoordinates(sc, block.fullScreenOffsets, cmd.x, cmd.y);
				cmd.special2 = *(cmd.x + 26);
				cmd.special3 = *(cmd.y + 22);
				cmd.translation = CR_GOLD;
				if(sc.CheckToken(',')) //more font information
				{
					this->getCoordinates(sc, block.fullScreenOffsets, cmd.special2, cmd.special3);
					if(sc.CheckToken(','))
					{
						sc.MustGetToken(TK_Identifier);
						cmd.translation = this->GetTranslation(sc, sc.String);
						if(sc.CheckToken(','))
						{
							sc.MustGetToken(TK_IntConst);
							cmd.special4 = sc.Number;
						}
					}
				}
				sc.MustGetToken(';');
				break;
			case SBARINFO_DRAWBAR:
				sc.MustGetToken(TK_StringConst);
				cmd.image_index = newImage(sc.String);
				cmd.sprite_index.SetInvalid();
				sc.MustGetToken(',');
				sc.MustGetToken(TK_StringConst);
				cmd.special = newImage(sc.String);
				sc.MustGetToken(',');
				sc.MustGetToken(TK_Identifier); //yeah, this is the same as drawnumber, there might be a better way to copy it...
				if(sc.Compare("health"))
				{
					cmd.flags = DRAWNUMBER_HEALTH;
					if(sc.CheckToken(TK_Identifier)) //comparing reference
					{
						cmd.setString(sc, sc.String, 0);
						const PClass* item = PClass::FindClass(sc.String);
						if(item == NULL || !PClass::FindClass("Inventory")->IsAncestorOf(item)) //must be a kind of inventory
						{
							sc.ScriptError("'%s' is not a type of inventory item.", sc.String);
						}
					}
					else
						cmd.special2 = DRAWBAR_COMPAREDEFAULTS;
				}
				else if(sc.Compare("armor"))
				{
					cmd.flags = DRAWNUMBER_ARMOR;
					if(sc.CheckToken(TK_Identifier))
					{
						cmd.setString(sc, sc.String, 0);
						const PClass* item = PClass::FindClass(sc.String);
						if(item == NULL || !PClass::FindClass("Inventory")->IsAncestorOf(item)) //must be a kind of inventory
						{
							sc.ScriptError("'%s' is not a type of inventory item.", sc.String);
						}
					}
					else
						cmd.special2 = DRAWBAR_COMPAREDEFAULTS;
				}
				else if(sc.Compare("ammo1"))
					cmd.flags = DRAWNUMBER_AMMO1;
				else if(sc.Compare("ammo2"))
					cmd.flags = DRAWNUMBER_AMMO2;
				else if(sc.Compare("ammo")) //request the next string to be an ammo type
				{
					sc.MustGetToken(TK_Identifier);
					cmd.setString(sc, sc.String, 0);
					cmd.flags = DRAWNUMBER_AMMO;
					const PClass* ammo = PClass::FindClass(sc.String);
					if(ammo == NULL || !RUNTIME_CLASS(AAmmo)->IsAncestorOf(ammo)) //must be a kind of ammo
					{
						sc.ScriptError("'%s' is not a type of ammo.", sc.String);
					}
				}
				else if(sc.Compare("frags"))
					cmd.flags = DRAWNUMBER_FRAGS;
				else if(sc.Compare("kills"))
					cmd.flags = DRAWNUMBER_KILLS;
				else if(sc.Compare("items"))
					cmd.flags = DRAWNUMBER_ITEMS;
				else if(sc.Compare("secrets"))
					cmd.flags = DRAWNUMBER_SECRETS;
				else if(sc.Compare("airtime"))
					cmd.flags = DRAWNUMBER_AIRTIME;
				else if(sc.Compare("poweruptime"))
				{
					cmd.flags |= DRAWNUMBER_POWERUPTIME;
					sc.MustGetToken(TK_Identifier);
					cmd.setString(sc, sc.String, 0);
					const PClass* item = PClass::FindClass(sc.String);
					if(item == NULL || !PClass::FindClass("PowerupGiver")->IsAncestorOf(item))
					{
						sc.ScriptError("'%s' is not a type of PowerupGiver.", sc.String);
					}
				}
				else if(sc.Compare("teamscore")) //Takes in a number for team
				{
					cmd.flags |= DRAWNUMBER_TEAMSCORE;
					sc.MustGetToken(TK_StringConst);
					int t = -1;
					for(unsigned int i = 0;i < teams.Size();i++)
					{
						if(teams[i].Name.CompareNoCase(sc.String) == 0)
						{
							t = (int) i;
							break;
						}
					}
					if(t == -1)
						sc.ScriptError("'%s' is not a valid team.", sc.String);
					cmd.value = t;
				}
				else
				{
					cmd.flags = DRAWNUMBER_INVENTORY;
					cmd.setString(sc, sc.String, 0);
					const PClass* item = PClass::FindClass(sc.String);
					if(item == NULL || !RUNTIME_CLASS(AInventory)->IsAncestorOf(item))
					{
						sc.ScriptError("'%s' is not a type of inventory item.", sc.String);
					}
				}
				sc.MustGetToken(',');
				sc.MustGetToken(TK_Identifier);
				if(sc.Compare("horizontal"))
					cmd.special2 += DRAWBAR_HORIZONTAL;
				else if(!sc.Compare("vertical"))
					sc.ScriptError("Unknown direction '%s'.", sc.String);
				sc.MustGetToken(',');
				while(sc.CheckToken(TK_Identifier))
				{
					if(sc.Compare("reverse"))
						cmd.special2 += DRAWBAR_REVERSE;
					else
						sc.ScriptError("Unkown flag '%s'.", sc.String);
					if(!sc.CheckToken('|'))
						sc.MustGetToken(',');
				}
				this->getCoordinates(sc, block.fullScreenOffsets, cmd.x, cmd.y);
				if(sc.CheckToken(',')) //border
				{
					sc.MustGetToken(TK_IntConst);
					cmd.special3 = sc.Number;
				}
				sc.MustGetToken(';');
				break;
			case SBARINFO_DRAWGEM:
				while(sc.CheckToken(TK_Identifier))
				{
					if(sc.Compare("wiggle"))
						cmd.flags |= DRAWGEM_WIGGLE;
					else if(sc.Compare("translatable"))
						cmd.flags |= DRAWGEM_TRANSLATABLE;
					else if(sc.Compare("armor"))
						cmd.flags |= DRAWGEM_ARMOR;
					else if(sc.Compare("reverse"))
						cmd.flags |= DRAWGEM_REVERSE;
					else
						sc.ScriptError("Unknown drawgem flag '%s'.", sc.String);
					if(!sc.CheckToken('|'))
							sc.MustGetToken(',');
				}
				sc.MustGetToken(TK_StringConst); //chain
				cmd.special = newImage(sc.String);
				sc.MustGetToken(',');
				sc.MustGetToken(TK_StringConst); //gem
				cmd.image_index = newImage(sc.String);
				cmd.sprite_index.SetInvalid();
				sc.MustGetToken(',');
				cmd.special2 = this->getSignedInteger(sc);
				sc.MustGetToken(',');
				cmd.special3 = this->getSignedInteger(sc);
				sc.MustGetToken(',');
				sc.MustGetToken(TK_IntConst);
				if(sc.Number < 0)
					sc.ScriptError("Chain size must be a positive number.");
				cmd.special4 = sc.Number;
				sc.MustGetToken(',');
				this->getCoordinates(sc, block.fullScreenOffsets, cmd.x, cmd.y);
				sc.MustGetToken(';');
				break;
			case SBARINFO_DRAWSHADER:
				sc.MustGetToken(TK_IntConst);
				cmd.special = sc.Number;
				if(sc.Number < 1)
					sc.ScriptError("Width must be greater than 1.");
				sc.MustGetToken(',');
				sc.MustGetToken(TK_IntConst);
				cmd.special2 = sc.Number;
				if(sc.Number < 1)
					sc.ScriptError("Height must be greater than 1.");
				sc.MustGetToken(',');
				sc.MustGetToken(TK_Identifier);
				if(sc.Compare("vertical"))
					cmd.flags |= DRAWSHADER_VERTICAL;
				else if(!sc.Compare("horizontal"))
					sc.ScriptError("Unknown direction '%s'.", sc.String);
				sc.MustGetToken(',');
				if(sc.CheckToken(TK_Identifier))
				{
					if(!sc.Compare("reverse"))
					{
						sc.ScriptError("Exspected 'reverse', got '%s' instead.", sc.String);
					}
					cmd.flags |= DRAWSHADER_REVERSE;
					sc.MustGetToken(',');
				}
				this->getCoordinates(sc, block.fullScreenOffsets, cmd.x, cmd.y);
				sc.MustGetToken(';');
				break;
			case SBARINFO_DRAWSTRING:
				sc.MustGetToken(TK_Identifier);
				cmd.font = V_GetFont(sc.String);
				if(cmd.font == NULL)
					sc.ScriptError("Unknown font '%s'.", sc.String);
				sc.MustGetToken(',');
				sc.MustGetToken(TK_Identifier);
				cmd.translation = this->GetTranslation(sc, sc.String);
				sc.MustGetToken(',');
				sc.MustGetToken(TK_StringConst);
				cmd.setString(sc, sc.String, 0, -1, false);
				sc.MustGetToken(',');
				this->getCoordinates(sc, block.fullScreenOffsets, cmd.x, cmd.y);
				if(sc.CheckToken(',')) //spacing
				{
					sc.MustGetToken(TK_IntConst);
					cmd.special = sc.Number;
				}
				sc.MustGetToken(';');
				break;
			case SBARINFO_DRAWKEYBAR:
				sc.MustGetToken(TK_IntConst);
				cmd.value = sc.Number;
				sc.MustGetToken(',');
				sc.MustGetToken(TK_Identifier);
				if(sc.Compare("vertical"))
					cmd.flags |= DRAWKEYBAR_VERTICAL;
				else if(!sc.Compare("horizontal"))
					sc.ScriptError("Unknown direction '%s'.", sc.String);
				sc.MustGetToken(',');
				while(sc.CheckToken(TK_Identifier))
				{
					if(sc.Compare("reverserows"))
						cmd.flags |= DRAWKEYBAR_REVERSEROWS;
					else
						sc.ScriptError("Unknown flag '%s'.", sc.String);
					if(!sc.CheckToken('|'))
						sc.MustGetToken(',');
				}
				if(sc.CheckToken(TK_Auto))
					cmd.special = -1;
				else
				{
					sc.MustGetToken(TK_IntConst);
					cmd.special = sc.Number;
				}
				sc.MustGetToken(',');
				this->getCoordinates(sc, block.fullScreenOffsets, cmd.x, cmd.y);
				if(sc.CheckToken(','))
				{
					//key offset
					sc.MustGetToken(TK_IntConst);
					cmd.special2 = sc.Number;
					if(sc.CheckToken(','))
					{
						//max per row/column
						sc.MustGetToken(TK_IntConst);
						cmd.special3 = sc.Number;
						sc.MustGetToken(',');
						//row/column spacing (opposite of previous)
						if(sc.CheckToken(TK_Auto))
							cmd.special4 = -1;
						else
						{
							sc.MustGetToken(TK_IntConst);
							cmd.special4 = sc.Number;
						}
					}
				}
				sc.MustGetToken(';');
				break;
			case SBARINFO_GAMEMODE:
				while(sc.CheckToken(TK_Identifier))
				{
					if(sc.Compare("singleplayer"))
						cmd.flags |= GAMETYPE_SINGLEPLAYER;
					else if(sc.Compare("cooperative"))
						cmd.flags |= GAMETYPE_COOPERATIVE;
					else if(sc.Compare("deathmatch"))
						cmd.flags |= GAMETYPE_DEATHMATCH;
					else if(sc.Compare("teamgame"))
						cmd.flags |= GAMETYPE_TEAMGAME;
					else if(sc.Compare("ctf"))
						cmd.flags |= GAMETYPE_CTF;
					else if(sc.Compare("oneflagctf"))
						cmd.flags |= GAMETYPE_ONEFLAGCTF;
					else if(sc.Compare("skulltag"))
						cmd.flags |= GAMETYPE_SKULLTAG;
					else if(sc.Compare("invasion"))
						cmd.flags |= GAMETYPE_INVASION;
					else if(sc.Compare("possession"))
						cmd.flags |= GAMETYPE_POSSESSION;
					else if(sc.Compare("teampossession"))
						cmd.flags |= GAMETYPE_TEAMPOSSESSION;
					else if(sc.Compare("lastmanstanding"))
						cmd.flags |= GAMETYPE_LASTMANSTANDING;
					else if(sc.Compare("teamlms"))
						cmd.flags |= GAMETYPE_TEAMLMS;
					else if(sc.Compare("survival"))
						cmd.flags |= GAMETYPE_SURVIVAL;
					else if(sc.Compare("instagib"))
						cmd.flags |= GAMETYPE_INSTAGIB;
					else if(sc.Compare("buckshot"))
						cmd.flags |= GAMETYPE_BUCKSHOT;
					//else I'm removing this error to allow cross port compatiblity.  If it doesn't know what a gamemode is lets just ignore it.
					//	sc.ScriptError("Unknown gamemode: %s", sc.String);

					if(sc.CheckToken('{'))
						break;
					sc.MustGetToken(',');
				}
				cmd.subBlock.fullScreenOffsets = block.fullScreenOffsets;
				this->ParseSBarInfoBlock(sc, cmd.subBlock);
				break;
			case SBARINFO_PLAYERCLASS:
				cmd.special = cmd.special2 = cmd.special3 = -1;
				for(int i = 0;i < 3 && sc.CheckToken(TK_Identifier);i++) //up to 3 classes
				{
					bool foundClass = false;
					for(unsigned int c = 0;c < PlayerClasses.Size();c++)
					{
						if(stricmp(sc.String, PlayerClasses[c].Type->Meta.GetMetaString(APMETA_DisplayName)) == 0)
						{
							foundClass = true;
							if(i == 0)
								cmd.special = PlayerClasses[c].Type->ClassIndex;
							else if(i == 1)
								cmd.special2 = PlayerClasses[c].Type->ClassIndex;
							else //should be 2
								cmd.special3 = PlayerClasses[c].Type->ClassIndex;
							break;
						}
					}
					if(!foundClass)
						sc.ScriptError("Unkown PlayerClass '%s'.", sc.String);
					if(sc.CheckToken('{') || i == 2)
						goto FinishPlayerClass;
					sc.MustGetToken(',');
				}
			FinishPlayerClass:
				cmd.subBlock.fullScreenOffsets = block.fullScreenOffsets;
				this->ParseSBarInfoBlock(sc, cmd.subBlock);
				break;
			case SBARINFO_ASPECTRATIO:
				sc.MustGetToken(TK_StringConst);
				if(sc.Compare("4:3"))
					cmd.value = ASPECTRATIO_4_3;
				else if(sc.Compare("16:9"))
					cmd.value = ASPECTRATIO_16_9;
				else if(sc.Compare("16:10"))
					cmd.value = ASPECTRATIO_16_10;
				else if(sc.Compare("5:4"))
					cmd.value = ASPECTRATIO_5_4;
				else
					sc.ScriptError("Unkown aspect ratio: %s", sc.String);
				sc.MustGetToken('{');
				cmd.subBlock.fullScreenOffsets = block.fullScreenOffsets;
				this->ParseSBarInfoBlock(sc, cmd.subBlock);
				break;
			case SBARINFO_ISSELECTED:
				//Using StringConst instead of Identifieres is deperecated!
				if(sc.CheckToken(TK_Identifier))
				{
					if(sc.Compare("not"))
					{
						cmd.flags |= SBARINFOEVENT_NOT;
						if(!sc.CheckToken(TK_StringConst))
							sc.MustGetToken(TK_Identifier);
					}
				}
				else
					sc.MustGetToken(TK_StringConst);
				for(int i = 0;i < 2;i++)
				{
					cmd.setString(sc, sc.String, i);
					const PClass* item = PClass::FindClass(sc.String);
					if(item == NULL || !RUNTIME_CLASS(AWeapon)->IsAncestorOf(item))
					{
						sc.ScriptError("'%s' is not a type of weapon.", sc.String);
					}
					if(sc.CheckToken(','))
					{
						if(!sc.CheckToken(TK_StringConst))
							sc.MustGetToken(TK_Identifier);
					}
					else
						break;
				}
				sc.MustGetToken('{');
				cmd.subBlock.fullScreenOffsets = block.fullScreenOffsets;
				this->ParseSBarInfoBlock(sc, cmd.subBlock);
				break;
			case SBARINFO_USESSECONDARYAMMO:
			case SBARINFO_USESAMMO:
				if(sc.CheckToken(TK_Identifier))
				{
					if(sc.Compare("not"))
						cmd.flags |= SBARINFOEVENT_NOT;
					else
						sc.ScriptError("Expected 'not' got '%s' instead.", sc.String);
				}
				sc.MustGetToken('{');
				cmd.subBlock.fullScreenOffsets = block.fullScreenOffsets;
				this->ParseSBarInfoBlock(sc, cmd.subBlock);
				break;
			case SBARINFO_HASWEAPONPIECE:
			{
				sc.MustGetToken(TK_Identifier);
				const PClass* weapon = PClass::FindClass(sc.String);
				if(weapon == NULL || !RUNTIME_CLASS(AWeapon)->IsAncestorOf(weapon)) //must be a weapon
					sc.ScriptError("%s is not a kind of weapon.", sc.String);
				cmd.setString(sc, sc.String, 0);
				sc.MustGetToken(',');
				sc.MustGetToken(TK_IntConst);
				if(sc.Number < 1)
					sc.ScriptError("Weapon piece number can not be less than 1.");
				cmd.value = sc.Number;
				sc.MustGetToken('{');
				cmd.subBlock.fullScreenOffsets = block.fullScreenOffsets;
				this->ParseSBarInfoBlock(sc, cmd.subBlock);
				break;
			}
			case SBARINFO_INVENTORYBARNOTVISIBLE:
				sc.MustGetToken('{');
				cmd.subBlock.fullScreenOffsets = block.fullScreenOffsets;
				this->ParseSBarInfoBlock(sc, cmd.subBlock);
				break;
			case SBARINFO_WEAPONAMMO:
				sc.MustGetToken(TK_Identifier);
				if(sc.Compare("not"))
				{
					cmd.flags |= SBARINFOEVENT_NOT;
					sc.MustGetToken(TK_Identifier);
				}
				for(int i = 0;i < 2;i++)
				{
					cmd.setString(sc, sc.String, i);
					const PClass* ammo = PClass::FindClass(sc.String);
					if(ammo == NULL || !RUNTIME_CLASS(AAmmo)->IsAncestorOf(ammo)) //must be a kind of ammo
					{
						sc.ScriptError("'%s' is not a type of ammo.", sc.String);
					}
					if(sc.CheckToken(TK_OrOr))
					{
						cmd.flags |= SBARINFOEVENT_OR;
						sc.MustGetToken(TK_Identifier);
					}
					else if(sc.CheckToken(TK_AndAnd))
					{
						cmd.flags |= SBARINFOEVENT_AND;
						sc.MustGetToken(TK_Identifier);
					}
					else
						break;
				}
				sc.MustGetToken('{');
				cmd.subBlock.fullScreenOffsets = block.fullScreenOffsets;
				this->ParseSBarInfoBlock(sc, cmd.subBlock);
				break;
			case SBARINFO_ININVENTORY:
				cmd.special2 = cmd.special3 = 0;
				sc.MustGetToken(TK_Identifier);
				if(sc.Compare("not"))
				{
					cmd.flags |= SBARINFOEVENT_NOT;
					sc.MustGetToken(TK_Identifier);
				}
				for(int i = 0;i < 2;i++)
				{
					cmd.setString(sc, sc.String, i);
					const PClass* item = PClass::FindClass(sc.String);
					if(item == NULL || !RUNTIME_CLASS(AInventory)->IsAncestorOf(item))
					{
						sc.ScriptError("'%s' is not a type of inventory item.", sc.String);
					}
					if (sc.CheckToken(','))
					{
						sc.MustGetNumber();
						if (i == 0) cmd.special2 = sc.Number;
						else cmd.special3 = sc.Number;
					}

					if(sc.CheckToken(TK_OrOr))
					{
						cmd.flags |= SBARINFOEVENT_OR;
						sc.MustGetToken(TK_Identifier);
					}
					else if(sc.CheckToken(TK_AndAnd))
					{
						cmd.flags |= SBARINFOEVENT_AND;
						sc.MustGetToken(TK_Identifier);
					}
					else
						break;
				}
				sc.MustGetToken('{');
				cmd.subBlock.fullScreenOffsets = block.fullScreenOffsets;
				this->ParseSBarInfoBlock(sc, cmd.subBlock);
				break;
		}
		block.commands.Push(cmd);
	}
	sc.MustGetToken('}');
}

void SBarInfo::ParseMugShotBlock(FScanner &sc, FMugShotState &state)
{
	sc.MustGetToken('{');
	while(!sc.CheckToken('}'))
	{
		FMugShotFrame frame;
		bool multiframe = false;
		if(sc.CheckToken('{'))
			multiframe = true;
		do
		{
			sc.MustGetToken(TK_Identifier);
			if(strlen(sc.String) > 5)
				sc.ScriptError("MugShot frames cannot exceed 5 characters.");
			frame.Graphic.Push(sc.String);
		}
		while(multiframe && sc.CheckToken(','));
		if(multiframe)
			sc.MustGetToken('}');
		frame.Delay = getSignedInteger(sc);
		sc.MustGetToken(';');
		state.Frames.Push(frame);
	}
}

void SBarInfo::getCoordinates(FScanner &sc, bool fullScreenOffsets, int &x, int &y)
{
	bool negative = false;
	bool relCenter = false;
	int *coords[2] = {&x, &y};
	for(int i = 0;i < 2;i++)
	{
		negative = false;
		relCenter = false;
		if(i > 0)
			sc.MustGetToken(',');

		// [-]INT center
		negative = sc.CheckToken('-');
		sc.MustGetToken(TK_IntConst);
		*coords[i] = negative ? -sc.Number : sc.Number;
		if(sc.CheckToken('+'))
		{
			sc.MustGetToken(TK_Identifier);
			if(!sc.Compare("center"))
				sc.ScriptError("Expected 'center' but got '%s' instead.", sc.String);
			relCenter = true;
		}
		if(fullScreenOffsets)
		{
			if(relCenter)
				*coords[i] |= SBarInfoCoordinate::REL_CENTER;
			else
				*coords[i] &= ~SBarInfoCoordinate::REL_CENTER;
		}
	}

	if(!fullScreenOffsets)
		y = (negative ? -sc.Number : sc.Number) - (200 - this->height);
}

void SBarInfo::getCoordinates(FScanner &sc, bool fullScreenOffsets, SBarInfoCoordinate &x, SBarInfoCoordinate &y)
{
	int tmpX = *x;
	int tmpY = *y;
	getCoordinates(sc, fullScreenOffsets, tmpX, tmpY);
	x = tmpX;
	y = tmpY;
}

int SBarInfo::getSignedInteger(FScanner &sc)
{
	if(sc.CheckToken('-'))
	{
		sc.MustGetToken(TK_IntConst);
		return -sc.Number;
	}
	else
	{
		sc.MustGetToken(TK_IntConst);
		return sc.Number;
	}
}

int SBarInfo::newImage(const char *patchname)
{
	if(patchname[0] == '\0' || stricmp(patchname, "nullimage") == 0)
	{
		return -1;
	}
	for(unsigned int i = 0;i < this->Images.Size();i++) //did we already load it?
	{
		if(stricmp(this->Images[i], patchname) == 0)
		{
			return i;
		}
	}
	return this->Images.Push(patchname);
}

//converts a string into a tranlation.
EColorRange SBarInfo::GetTranslation(FScanner &sc, const char* translation)
{
	EColorRange returnVal = CR_UNTRANSLATED;
	FString namedTranslation; //we must send in "[translation]"
	const BYTE *trans_ptr;
	namedTranslation.Format("[%s]", translation);
	trans_ptr = (const BYTE *)(&namedTranslation[0]);
	if((returnVal = V_ParseFontColor(trans_ptr, CR_UNTRANSLATED, CR_UNTRANSLATED)) == CR_UNDEFINED)
	{
		sc.ScriptError("Missing definition for color %s.", translation);
	}
	return returnVal;
}

SBarInfo::SBarInfo() //make results more predicable
{
	Init();
}

SBarInfo::SBarInfo(int lumpnum)
{
	Init();
	ParseSBarInfo(lumpnum);
}

void SBarInfo::Init()
{
	automapbar = false;
	interpolateHealth = false;
	interpolateArmor = false;
	completeBorder = false;
	lowerHealthCap = true;
	interpolationSpeed = 8;
	armorInterpolationSpeed = 8;
	height = 0;
	spacingCharacter = '\0';
}

SBarInfo::~SBarInfo()
{
	for (size_t i = 0; i < NUMHUDS; ++i)
	{
		huds[i].commands.Clear();
	}
}

void SBarInfoCommand::setString(FScanner &sc, const char* source, int strnum, int maxlength, bool exact)
{
	if(!exact)
	{
		if(maxlength != -1 && strlen(source) > (unsigned int) maxlength)
		{
			sc.ScriptError("%s is greater than %d characters.", source, maxlength);
			return;
		}
	}
	else
	{
		if(maxlength != -1 && strlen(source) != (unsigned int) maxlength)
		{
			sc.ScriptError("%s must be %d characters.", source, maxlength);
			return;
		}
	}
	string[strnum] = source;
}

SBarInfoCommand::SBarInfoCommand() //sets the default values for more predicable behavior
{
	type = 0;
	special = 0;
	special2 = 0;
	special3 = 0;
	special4 = 0;
	flags = 0;
	x = 0;
	y = 0;
	value = 0;
	image_index = 0;
	sprite_index.SetInvalid();
	translation = CR_UNTRANSLATED;
	translation2 = CR_UNTRANSLATED;
	translation3 = CR_UNTRANSLATED;
	font = V_GetFont("CONFONT");
}

SBarInfoCommand::~SBarInfoCommand()
{
}

SBarInfoBlock::SBarInfoBlock()
{
	forceScaled = false;
	fullScreenOffsets = false;
	alpha = FRACUNIT;
}

//Popup
Popup::Popup()
{
	transition = TRANSITION_NONE;
	height = 320;
	width = 200;
	speed = 0;
	x = 320;
	y = 200;
	alpha = FRACUNIT;
	opened = false;
	moving = false;
}

void Popup::init()
{
	x = width;
	y = height;
	if(transition == TRANSITION_SLIDEINBOTTOM)
	{
		x = 0;
	}
	else if(transition == TRANSITION_FADE)
	{
		alpha = 0;
		x = 0;
		y = 0;
	}
}

void Popup::tick()
{
	if(transition == TRANSITION_SLIDEINBOTTOM)
	{
		if(moving)
		{
			if(opened)
				y -= clamp(height + (y - height), 1, speed);
			else
				y += clamp(height - y, 1, speed);
		}
		if(y != 0 && y != height)
			moving = true;
		else
			moving = false;
	}
	else if(transition == TRANSITION_FADE)
	{
		if(moving)
		{
			if(opened)
				alpha = clamp(alpha + speed, 0, FRACUNIT);
			else
				alpha = clamp(alpha - speed2, 0, FRACUNIT);
		}
		if(alpha == 0 || alpha == FRACUNIT)
			moving = false;
		else
			moving = true;
	}
	else
	{
		if(opened)
		{
			y = 0;
			x = 0;
		}
		else
		{
			y = height;
			x = width;
		}
		moving = false;
	}
}

bool Popup::isDoneMoving()
{
	return !moving;
}

int Popup::getXOffset()
{
	return x;
}

int Popup::getYOffset()
{
	return y;
}

int Popup::getAlpha(int maxAlpha)
{
	double a = (double) alpha / (double) FRACUNIT;
	double b = (double) maxAlpha / (double) FRACUNIT;
	return fixed_t((a * b) * FRACUNIT);
}

void Popup::open()
{
	opened = true;
	moving = true;
}

void Popup::close()
{
	opened = false;
	moving = true;
}
