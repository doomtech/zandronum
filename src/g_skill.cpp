/*
** g_skill.cpp
** Skill level handling
**
**---------------------------------------------------------------------------
** Copyright 2008-2009 Christoph Oelckers
** Copyright 2008-2009 Randy Heit
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

#include <ctype.h>
#include "doomstat.h"
#include "d_player.h"
#include "g_level.h"
#include "g_game.h"
#include "gi.h"
#include "templates.h"
#include "v_font.h"
#include "m_fixed.h"
#include "gstrings.h"

TArray<FSkillInfo> AllSkills;
int DefaultSkill = -1;

//==========================================================================
//
// ParseSkill
//
//==========================================================================

void FMapInfoParser::ParseSkill ()
{
	FSkillInfo skill;
	bool thisisdefault = false;
	bool acsreturnisset = false;

	skill.AmmoFactor = FRACUNIT;
	skill.DoubleAmmoFactor = 2*FRACUNIT;
	skill.DropAmmoFactor = -1;
	skill.DamageFactor = FRACUNIT;
	skill.FastMonsters = false;
	skill.DisableCheats = false;
	skill.EasyBossBrain = false;
	skill.AutoUseHealth = false;
	skill.RespawnCounter = 0;
	skill.RespawnLimit = 0;
	skill.Aggressiveness = FRACUNIT;
	skill.SpawnFilter = 0;
	skill.ACSReturn = 0;
	skill.MustConfirm = false;
	skill.Shortcut = 0;
	skill.TextColor = "";
	skill.Replace.Clear();
	skill.Replaced.Clear();
	skill.MonsterHealth = FRACUNIT;
	skill.FriendlyHealth = FRACUNIT;
	skill.NoPain = false;

	sc.MustGetString();
	skill.Name = sc.String;

	ParseOpenBrace();

	while (sc.GetString ())
	{
		if (sc.Compare ("ammofactor"))
		{
			ParseAssign();
			sc.MustGetFloat ();
			skill.AmmoFactor = FLOAT2FIXED(sc.Float);
		}
		else if (sc.Compare ("doubleammofactor"))
		{
			ParseAssign();
			sc.MustGetFloat ();
			skill.DoubleAmmoFactor = FLOAT2FIXED(sc.Float);
		}
		else if (sc.Compare ("dropammofactor"))
		{
			ParseAssign();
			sc.MustGetFloat ();
			skill.DropAmmoFactor = FLOAT2FIXED(sc.Float);
		}
		else if (sc.Compare ("damagefactor"))
		{
			ParseAssign();
			sc.MustGetFloat ();
			skill.DamageFactor = FLOAT2FIXED(sc.Float);
		}
		else if (sc.Compare ("fastmonsters"))
		{
			skill.FastMonsters = true;
		}
		else if (sc.Compare ("disablecheats"))
		{
			skill.DisableCheats = true;
		}
		else if (sc.Compare ("easybossbrain"))
		{
			skill.EasyBossBrain = true;
		}
		else if (sc.Compare("autousehealth"))
		{
			skill.AutoUseHealth = true;
		}
		else if (sc.Compare("respawntime"))
		{
			ParseAssign();
			sc.MustGetFloat ();
			skill.RespawnCounter = int(sc.Float*TICRATE);
		}
		else if (sc.Compare("respawnlimit"))
		{
			ParseAssign();
			sc.MustGetNumber ();
			skill.RespawnLimit = sc.Number;
		}
		else if (sc.Compare("Aggressiveness"))
		{
			ParseAssign();
			sc.MustGetFloat ();
			skill.Aggressiveness = FRACUNIT - FLOAT2FIXED(clamp(sc.Float, 0.,1.));
		}
		else if (sc.Compare("SpawnFilter"))
		{
			ParseAssign();
			if (sc.CheckNumber())
			{
				if (sc.Number > 0) skill.SpawnFilter |= (1<<(sc.Number-1));
			}
			else
			{
				sc.MustGetString ();
				if (sc.Compare("baby")) skill.SpawnFilter |= 1;
				else if (sc.Compare("easy")) skill.SpawnFilter |= 2;
				else if (sc.Compare("normal")) skill.SpawnFilter |= 4;
				else if (sc.Compare("hard")) skill.SpawnFilter |= 8;
				else if (sc.Compare("nightmare")) skill.SpawnFilter |= 16;
			}
		}
		else if (sc.Compare("ACSReturn"))
		{
			ParseAssign();
			sc.MustGetNumber ();
			skill.ACSReturn = sc.Number;
			acsreturnisset = true;
		}
		else if (sc.Compare("ReplaceActor"))
		{
			ParseAssign();
			sc.MustGetString();
			FName replaced = sc.String;
			ParseComma();
			sc.MustGetString();
			FName replacer = sc.String;
			skill.SetReplacement(replaced, replacer);
			skill.SetReplacedBy(replacer, replaced);
		}
		else if (sc.Compare("Name"))
		{
			ParseAssign();
			sc.MustGetString ();
			skill.MenuName = sc.String;
		}
		else if (sc.Compare("PlayerClassName"))
		{
			ParseAssign();
			sc.MustGetString ();
			FName pc = sc.String;
			ParseComma();
			sc.MustGetString ();
			skill.MenuNamesForPlayerClass[pc]=sc.String;
		}
		else if (sc.Compare("PicName"))
		{
			ParseAssign();
			sc.MustGetString ();
			skill.PicName = sc.String;
		}
		else if (sc.Compare("MustConfirm"))
		{
			skill.MustConfirm = true;
			if (format_type == FMT_New) 
			{
				if (CheckAssign())
				{
					sc.MustGetString();
					skill.MustConfirmText = sc.String;
				}
			}
			else
			{
				if (sc.CheckToken(TK_StringConst))
				{
					skill.MustConfirmText = sc.String;
				}
			}
		}
		else if (sc.Compare("Key"))
		{
			ParseAssign();
			sc.MustGetString();
			skill.Shortcut = tolower(sc.String[0]);
		}
		else if (sc.Compare("TextColor"))
		{
			ParseAssign();
			sc.MustGetString();
			skill.TextColor.Format("[%s]", sc.String);
		}
		else if (sc.Compare("MonsterHealth"))
		{
			ParseAssign();
			sc.MustGetFloat();
			skill.MonsterHealth = FLOAT2FIXED(sc.Float);				
		}
		else if (sc.Compare("FriendlyHealth"))
		{
			ParseAssign();
			sc.MustGetFloat();
			skill.FriendlyHealth = FLOAT2FIXED(sc.Float);
		}
		else if (sc.Compare("NoPain"))
		{
			skill.NoPain = true;
		}
		else if (sc.Compare("DefaultSkill"))
		{
			if (DefaultSkill >= 0)
			{
				sc.ScriptError("%s is already the default skill\n", AllSkills[DefaultSkill].Name.GetChars());
			}
			thisisdefault = true;
		}
		else if (!ParseCloseBrace())
		{
			// Unknown
			sc.ScriptMessage("Unknown property '%s' found in skill definition\n", sc.String);
			SkipToNext();
		}
		else
		{
			break;
		}
	}
	CheckEndOfFile("skill");
	for(unsigned int i = 0; i < AllSkills.Size(); i++)
	{
		if (AllSkills[i].Name == skill.Name)
		{
			if (!acsreturnisset)
			{ // Use the ACS return for the skill we are overwriting.
				skill.ACSReturn = AllSkills[i].ACSReturn;
			}
			AllSkills[i] = skill;
			if (thisisdefault)
			{
				DefaultSkill = i;
			}
			return;
		}
	}
	if (!acsreturnisset)
	{
		skill.ACSReturn = AllSkills.Size();
	}
	if (thisisdefault)
	{
		DefaultSkill = AllSkills.Size();
	}
	AllSkills.Push(skill);
}

//==========================================================================
//
//
//
//==========================================================================

int G_SkillProperty(ESkillProperty prop)
{
	if (AllSkills.Size() > 0)
	{
		switch(prop)
		{
		case SKILLP_AmmoFactor:
			if (dmflags2 & DF2_YES_DOUBLEAMMO)
			{
				return AllSkills[gameskill].DoubleAmmoFactor;
			}
			return AllSkills[gameskill].AmmoFactor;

		case SKILLP_DropAmmoFactor:
			return AllSkills[gameskill].DropAmmoFactor;

		case SKILLP_DamageFactor:
			return AllSkills[gameskill].DamageFactor;

		case SKILLP_FastMonsters:
			return AllSkills[gameskill].FastMonsters  || (dmflags & DF_FAST_MONSTERS);

		case SKILLP_Respawn:
			if (dmflags & DF_MONSTERS_RESPAWN && AllSkills[gameskill].RespawnCounter==0) 
				return TICRATE * gameinfo.defaultrespawntime;
			return AllSkills[gameskill].RespawnCounter;

		case SKILLP_RespawnLimit:
			return AllSkills[gameskill].RespawnLimit;

		case SKILLP_Aggressiveness:
			return AllSkills[gameskill].Aggressiveness;

		case SKILLP_DisableCheats:
			return AllSkills[gameskill].DisableCheats;

		case SKILLP_AutoUseHealth:
			return AllSkills[gameskill].AutoUseHealth;

		case SKILLP_EasyBossBrain:
			return AllSkills[gameskill].EasyBossBrain;

		case SKILLP_SpawnFilter:
			return AllSkills[gameskill].SpawnFilter;

		case SKILLP_ACSReturn:
			return AllSkills[gameskill].ACSReturn;
		
		case SKILLP_MonsterHealth:
			return AllSkills[gameskill].MonsterHealth;

		case SKILLP_FriendlyHealth:
			return AllSkills[gameskill].FriendlyHealth;

		case SKILLP_NoPain:			
			return AllSkills[gameskill].NoPain;			
		}
	}
	return 0;
}

//==========================================================================
//
//
//
//==========================================================================

const char * G_SkillName()
{
	const char *name = AllSkills[gameskill].MenuName;

	player_t *player = &players[consoleplayer];
	const char *playerclass = player->mo->GetClass()->Meta.GetMetaString(APMETA_DisplayName);

	if (playerclass != NULL)
	{
		FString * pmnm = AllSkills[gameskill].MenuNamesForPlayerClass.CheckKey(playerclass);
		if (pmnm != NULL) name = *pmnm;
	}

	if (*name == '$') name = GStrings(name+1);
	return name;
}


//==========================================================================
//
//
//
//==========================================================================

void G_VerifySkill()
{
	if (gameskill >= (int)AllSkills.Size())
		gameskill = AllSkills.Size()-1;
	else if (gameskill < 0)
		gameskill = 0;
}

//==========================================================================
//
//
//
//==========================================================================

FSkillInfo &FSkillInfo::operator=(const FSkillInfo &other)
{
	Name = other.Name;
	AmmoFactor = other.AmmoFactor;
	DoubleAmmoFactor = other.DoubleAmmoFactor;
	DropAmmoFactor = other.DropAmmoFactor;
	DamageFactor = other.DamageFactor;
	FastMonsters = other.FastMonsters;
	DisableCheats = other.DisableCheats;
	AutoUseHealth = other.AutoUseHealth;
	EasyBossBrain = other.EasyBossBrain;
	RespawnCounter= other.RespawnCounter;
	RespawnLimit= other.RespawnLimit;
	Aggressiveness= other.Aggressiveness;
	SpawnFilter = other.SpawnFilter;
	ACSReturn = other.ACSReturn;
	MenuName = other.MenuName;
	PicName = other.PicName;
	MenuNamesForPlayerClass = other.MenuNamesForPlayerClass;
	MustConfirm = other.MustConfirm;
	MustConfirmText = other.MustConfirmText;
	Shortcut = other.Shortcut;
	TextColor = other.TextColor;
	Replace = other.Replace;
	Replaced = other.Replaced;
	MonsterHealth = other.MonsterHealth;
	FriendlyHealth = other.FriendlyHealth;
	NoPain = other.NoPain;
	return *this;
}

//==========================================================================
//
//
//
//==========================================================================

int FSkillInfo::GetTextColor() const
{
	if (TextColor.IsEmpty())
	{
		return CR_UNTRANSLATED;
	}
	const BYTE *cp = (const BYTE *)TextColor.GetChars();
	int color = V_ParseFontColor(cp, 0, 0);
	if (color == CR_UNDEFINED)
	{
		Printf("Undefined color '%s' in definition of skill %s\n", TextColor.GetChars(), Name.GetChars());
		color = CR_UNTRANSLATED;
	}
	return color;
}

//==========================================================================
//
// FSkillInfo::SetReplacement
//
//==========================================================================

void FSkillInfo::SetReplacement(FName a, FName b)
{
	Replace[a] = b;
}

//==========================================================================
//
// FSkillInfo::GetReplacement
//
//==========================================================================

FName FSkillInfo::GetReplacement(FName a)
{
	if (Replace.CheckKey(a)) return Replace[a];
	else return NAME_None;
}

//==========================================================================
//
// FSkillInfo::SetReplaced
//
//==========================================================================

void FSkillInfo::SetReplacedBy(FName b, FName a)
{
	Replaced[b] = a;
}

//==========================================================================
//
// FSkillInfo::GetReplaced
//
//==========================================================================

FName FSkillInfo::GetReplacedBy(FName b)
{
	if (Replaced.CheckKey(b)) return Replaced[b];
	else return NAME_None;
}
