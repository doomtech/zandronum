#include "c_cvars.h"
#include "sc_man.h"
#include "w_wad.h"
#include "c_console.h"
#include "pwo.h"
#include "tarray.h"
#include "gametype.h"
#include "gi.h"

#include <cstdlib>

// [ZZ] 2014 - added new CVars to control exact behavior on same weight or if involving unknown weapon.
CVAR(Bool, pwo_switchonsameweight, true, CVAR_ARCHIVE);
CVAR(Bool, pwo_switchonunknown, false, CVAR_ARCHIVE);

TArray<PWODef> PWODefs;
PWODef** PWODefsByOrder = NULL;
unsigned int PWODefsByOrderSz = 0;

bool PWO_WeightByClassExact(const char* className, float& weight)
{
	for(unsigned int i = 0; i < PWODefs.Size(); i++)
	{
		if(!PWODefs[i].ClassDef && !PWODefs[i].Clear)
		{
			if(!PWODefs[i].Name.CompareNoCase(className))
			{
				weight = PWODefs[i].Order->GetGenericRep(CVAR_Float).Float;
				return true;
			}
		}
		else if (PWODefs[i].ClassDef)
		{
			for(unsigned int j = 0; j < PWODefs[i].ClassChildren.Size(); j++)
			{
				if(PWODefs[i].ClassChildren[j].Name.CompareNoCase(className))
				{
					weight = PWODefs[i].ClassChildren[j].Order->GetGenericRep(CVAR_Float).Float;
					return true;
				}
			}
		}
	}

	return false;
}

bool PWO_WeightByClass(AWeapon* weap, float& weight)
{
	PClass* pclass = weap->GetClass();
	const char* name = pclass->TypeName.GetChars();

	// look for exact
	if(PWO_WeightByClassExact(name, weight)) return true;

	FActorInfo* replacee = pclass->ActorInfo->GetReplacee();
	do
	{
		name = replacee->Class->TypeName.GetChars();

		// look for replaced
		if(PWO_WeightByClassExact(name, weight)) return true;
	}
	while((replacee->GetReplacee() != replacee) && (replacee = replacee->GetReplacee()));

	weight = 0.0;
	return false;
}

bool PWO_CheckWeapons(AWeapon* first, AWeapon* pending)
{
	if(!first) return true;
	if(!pending) return false;

	if(!first->GetClass() || !pending->GetClass()) return false;

	bool found_first = false;
	bool found_pending = false;

	float weight_first = 0.0;
	float weight_pending = 0.0;
	
	found_first = PWO_WeightByClass(first, weight_first);
	found_pending = PWO_WeightByClass(pending, weight_pending);

	if(!found_first || !found_pending) return pwo_switchonunknown; // don't switch if one of weights is undefined
	if(weight_pending > weight_first) return true;
	else if(weight_pending == weight_first) return pwo_switchonsameweight;
	else return false;
}

int
// [BB] Necessary for qsort under VC++, but not available under Linux.
#ifdef _MSC_VER
__cdecl
#endif
PWO_CompareWeight(const void* elem1, const void* elem2)
{
	const PWODef* def1 = (PWODef*)elem1;
	const PWODef* def2 = (PWODef*)elem2;
	if(def1->MenuWeight == def2->MenuWeight) return 0;
	if(def1->MenuWeight > def2->MenuWeight) return 1;
	else return -1;
}

int
// [BB] Necessary for qsort under VC++, but not available under Linux.
#ifdef _MSC_VER
__cdecl
#endif
PWO_CompareWeight2(const void* elem1, const void* elem2)
{
	const PWODef* def1 = *(PWODef**)elem1;
	const PWODef* def2 = *(PWODef**)elem2;

	float wt1 = def1->Order->GetGenericRep(CVAR_Float).Float;
	float wt2 = def2->Order->GetGenericRep(CVAR_Float).Float;

	if(wt1 == wt2) return PWO_CompareWeight(def1, def2);
	else if(wt1 > wt2) return 1;
	else return -1;
}

void PWO_SortDefs()
{
	if(!PWODefsByOrder)
	{
		unsigned int sz = 0;
		for(unsigned int i = 0; i < PWODefs.Size(); i++)
		{
			if(!PWODefs[i].ClassDef && !PWODefs[i].Clear) sz++;
			else if(PWODefs[i].ClassDef) sz += PWODefs[i].ClassChildren.Size();
		}

		PWODefsByOrder = new PWODef*[sz];
		PWODefsByOrderSz = sz;

		unsigned int pos = 0;

		for(unsigned int i = 0; i < PWODefs.Size(); i++)
		{
			if(!PWODefs[i].ClassDef && !PWODefs[i].Clear)
			{
				PWODefsByOrder[pos] = new PWODef();
				*PWODefsByOrder[pos] = PWODefs[i];
				pos++;
			}
			else if(PWODefs[i].ClassDef)
			{
				for(unsigned int j = 0; j < PWODefs[i].ClassChildren.Size(); j++)
				{
					PWODefsByOrder[pos] = new PWODef();
					*PWODefsByOrder[pos] = PWODefs[i].ClassChildren[j];
					pos++;
				}
			}
		}
	}

	qsort(PWODefsByOrder, PWODefsByOrderSz, sizeof(PWODef*), PWO_CompareWeight2);
}

void PWO_FinalizeDef()
{
	TArray<PWODef> matdef;
	for(unsigned int i = 0; i < PWODefs.Size(); i++)
	{
		PWODef& def = PWODefs[i];
		if((def.GameFilter & gameinfo.gametype) || (def.GameFilter == GAME_Any))
		{
			if(def.Clear) matdef.Clear();
			else matdef.Push(def);
		}
	}

	for(unsigned int i = 0; i < matdef.Size(); i++)
	{
		if(!matdef[i].ClassDef && !matdef[i].Clear)
		{
			FString var_name;
			const char* game_name = ((matdef[i].GameFilter != GAME_Any) ? GameNames[matdef[i].GameFilter] : NULL);
			if(!game_name) game_name = "Zandronum";
			const char* weap_name = matdef[i].Name.GetChars();
			if(matdef[i].Replacee.Len()) weap_name = matdef[i].Replacee.GetChars();
			var_name.Format("PWO.%s.%s", game_name, weap_name);
			matdef[i].Order = FindCVar(var_name, NULL);
			if(!matdef[i].Order)
				matdef[i].Order = new FFloatCVar(var_name.GetChars(), matdef[i].DefaultOrder, CVAR_ARCHIVE, NULL);
		}
	}

	// merge class items
	for(unsigned int i = 0; i < matdef.Size(); i++)
	{
		if(matdef[i].Section.Len())
		{
			bool secfnd = false;
			for(unsigned int j = 0; j < matdef.Size(); j++)
			{
				if(matdef[j].Name == matdef[i].Section && matdef[j].ClassDef)
				{
					matdef[j].ClassChildren.Push(matdef[i]);
					matdef.Delete(i);
					i--;
					secfnd = true;
					break;
				}
			}

			if(!secfnd)
			{
				// odd.
				matdef[i].Section = "";
			}
		}
	}

	// i like how i should make separate array when in theory you can just use matdef.Array
	unsigned long mcnt = matdef.Size();
	PWODef* marr = new PWODef[mcnt];
	for(unsigned int i = 0; i < mcnt; i++)
		marr[i] = matdef[i];

	qsort(marr, mcnt, sizeof(PWODef), PWO_CompareWeight);

	PWODefs.Resize(mcnt);
	for(unsigned int i = 0; i < mcnt; i++)
		PWODefs[i] = marr[i];

	delete[] marr;
	matdef.Clear();

	PWO_SortDefs();
}

void PWO_ParseDef(FScanner& sc)
{
	FString currentSection = "None";
	bool haveDef = false;
	PWODef currentDef;

	unsigned int currentGame = 0;

	unsigned int sectionCount = 0;
	unsigned int sectionCurrent = 0;
	unsigned int sectionOffset = 0;
	FString playerClassName = "";

	while(sc.GetString())
	{
		if(sc.StringLen < 1) continue;
		else if(sc.String[0] == '[' && sc.String[sc.StringLen-1] == ']')
		{
			currentSection = sc.String;
			currentSection = currentSection.Mid(1, currentSection.Len() - 2);
		}
		else if(sc.Compare("Game") && currentSection != "None")
		{
			if(!sc.GetString())
			{
				Printf("PWO_ParseDef: unexpected EOF.\n");
				return;
			}

			if(sc.Compare("Any")) currentGame = GAME_Any;
			else if(sc.Compare("Doom")) currentGame = GAME_Doom;
			else if(sc.Compare("Heretic")) currentGame = GAME_Heretic;
			else if(sc.Compare("Hexen")) currentGame = GAME_Hexen;
			else if(sc.Compare("Strife")) currentGame = GAME_Strife;
			else if(sc.Compare("Chex")) currentGame = GAME_Chex;
			else
			{
				Printf("PWO_ParseDef: unknown Game %s.\n", sc.String);
				return;
			}
		}
		else if(sc.Compare("Section") && currentSection != "None")
		{
			currentDef = PWODef();

			if(!sc.GetString())
			{
				Printf("PWO_ParseDef: unexpected EOF.\n");
				return;
			}

			currentDef.ClassDef = true;
			currentDef.Name = sc.String;
			currentDef.Clear = false;
			playerClassName = currentDef.Name;

			currentDef.GameFilter = currentGame;
			
			if(!sc.GetNumber())
			{
				Printf("PWO_ParseDef: unexpected EOF.\n");
				return;
			}

			sectionCount = sc.Number;
			
			if(!sc.GetNumber())
			{
				Printf("PWO_ParseDef: unexpected EOF.\n");
				return;
			}

			sectionOffset = sc.Number;
			currentDef.MenuWeight = sc.Number;
			PWODefs.Push(currentDef);

			sectionCurrent = 0;
		}
		else if(sc.Compare("Weapon") && currentSection != "None")
		{
			currentDef = PWODef();

			if(!sc.GetString())
			{
				Printf("PWO_ParseDef: unexpected EOF.\n");
				return;
			}

			currentDef.ClassDef = false;
			currentDef.Name = sc.String;
			currentDef.GameFilter = currentGame;
			currentDef.Clear = false;

			sectionCurrent++;
			if(sectionCurrent > sectionCount) playerClassName = "";

			currentDef.Section = playerClassName;

			if(!sc.GetString())
			{
				Printf("PWO_ParseDef: unexpected EOF.\n");
				return;
			}

			currentDef.MenuName = sc.String;

			if(!sc.GetFloat())
			{
				Printf("PWO_ParseDef: unexpected EOF.\n");
				return;
			}

			currentDef.DefaultOrder = sc.Float;

			FScanner::SavedPos pos = sc.SavePos();

			if(!sc.GetString() || sc.Compare("Weapon") || sc.String[0] == '[' || sc.Compare("Section"))
			{
				sc.RestorePos(pos);
				currentDef.MenuWeight = 0;
				currentDef.Replacee = "";
				PWODefs.Push(currentDef);
				continue;
			}

			sc.RestorePos(pos);
			if(!sc.GetNumber())
			{
				Printf("PWO_ParseDef: unexpected EOF.\n");
				return;
			}

			currentDef.MenuWeight = sc.Number;

			pos = sc.SavePos();

			if(!sc.GetString() || (sc.Compare("Weapon") || sc.String[0] == '[' || sc.Compare("Section")))
			{
				sc.RestorePos(pos);
				currentDef.Replacee = "";
				PWODefs.Push(currentDef);
				continue;
			}

			if(!sc.Compare("replaces"))
			{
				Printf("PWO_ParseDef: unexpected token \"%s\".\n", sc.String);
				return;
			}

			if(!sc.GetString())
			{
				Printf("PWO_ParseDef: unexpected EOF.\n");
				return;
			}

			currentDef.Replacee = sc.String;
			PWODefs.Push(currentDef);
		}
		else if(sc.Compare("Clear"))
		{
			currentDef = PWODef();
			currentDef.Clear = true;
			currentDef.GameFilter = currentGame;
			PWODefs.Push(currentDef);
		}
		else
		{
			Printf("PWO_ParseDef: unexpected token \"%s\".\n", sc.String);
			return;
		}
	}
}

void PWO_LoadDefs()
{
	Printf("PWO_LoadDefs: Loading PWO definitions...\n");

	int lastlump, lump;
	lastlump = 0;
	while ((lump = Wads.FindLump("ORDERDEF", &lastlump)) != -1)
	{
		FScanner sc(lump);
		PWO_ParseDef(sc);
	}

	PWO_FinalizeDef();
}

// Pasted from m_options.cpp

menuitem_t* PWO_MenuOptions = NULL;
unsigned int PWO_MenuOptionsSz = 0;

static menuitem_t OptionsPWOseparator	= { redtext,  " ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} };

EXTERN_CVAR(Int, switchonpickup);
EXTERN_CVAR(Bool, cl_noammoswitch);

void PWO_GenerateMenu()
{
	TArray<menuitem_t> Items2;
	menuitem_t onoff;
	onoff.type = discrete;
	// lame hack to make PWO more or less centered instead of sticking to the left of the screen
	onoff.label = "              Switch on pickup";
	onoff.a.cvar = &switchonpickup;
	onoff.b.numvalues = 4.0;
	onoff.c.max = 0.0;
	onoff.d.graycheck = NULL;
	onoff.e.values = SwitchOnPickupVals;
	onoff.f.lServer = 0;
	Items2.Push(onoff);
	onoff.label = "Allow switch with no ammo";
	onoff.b.numvalues = 2.0;
	onoff.e.values = YesNo;
	onoff.a.cvar = &cl_noammoswitch;
	Items2.Push(onoff);
	onoff.label = "Switch on same weight (PWO)";
	onoff.b.numvalues = 2.0;
	onoff.e.values = YesNo;
	onoff.a.cvar = &pwo_switchonsameweight;
	Items2.Push(onoff);
	onoff.label = "Switch on unk.weapon (PWO)";
	onoff.b.numvalues = 2.0;
	onoff.e.values = YesNo;
	onoff.a.cvar = &pwo_switchonunknown;
	Items2.Push(onoff);
	// don't add "Cycle with original order" item here, it seems to be completely obsolete
	Items2.Push(OptionsPWOseparator);

	for(unsigned int i = 0; i < PWODefs.Size(); i++)
	{
		if(PWODefs[i].ClassDef)
		{
			if(i) Items2.Push(OptionsPWOseparator);
			menuitem_t classdef;
			classdef.type = whitetext;
			classdef.label = PWODefs[i].Name.GetChars();
			classdef.a.cvar = NULL;
			classdef.b.min = 0.0;
			classdef.c.max = 0.0;
			classdef.d.step = 0.0;
			classdef.e.values = NULL;
			classdef.f.lServer = 0;
			Items2.Push(classdef);
			for(unsigned int j = 0; j < PWODefs[i].ClassChildren.Size(); j++)
			{
				bool in_items3 = false;
				for(unsigned int k = 0; k < PWODefs[i].ClassChildren.Size(); k++)
				{
					if(!PWODefs[i].ClassChildren[k].Replacee.CompareNoCase(PWODefs[i].ClassChildren[j].Name))
					{
						in_items3 = true;
						break;
					}
				}

				if(in_items3) continue;

				menuitem_t itemdef;
				itemdef.type = slider;
				itemdef.label = PWODefs[i].ClassChildren[j].MenuName;
				itemdef.a.cvar = PWODefs[i].ClassChildren[j].Order;
				itemdef.b.min = 0.0;
				itemdef.c.max = 1.0;
				itemdef.d.step = 0.05;
				itemdef.e.values = NULL;
				itemdef.f.lServer = 0;
				Items2.Push(itemdef);
			}
		}
		else
		{
			bool in_items1 = false;
			for(unsigned int j = 0; j < PWODefs.Size(); j++)
			{
				if(!PWODefs[j].Replacee.CompareNoCase(PWODefs[i].Name))
				{
					in_items1 = true;
					break;
				}
			}

			if(in_items1) continue;

			menuitem_t itemdef;
			itemdef.type = slider;
			itemdef.label = PWODefs[i].MenuName;
			itemdef.a.cvar = PWODefs[i].Order;
			itemdef.b.min = 0.0;
			itemdef.c.max = 1.0;
			itemdef.d.step = 0.05;
			itemdef.e.values = NULL;
			itemdef.f.lServer = 0;
			Items2.Push(itemdef);
		}
	}

	PWO_MenuOptions = new menuitem_t[Items2.Size()];
	for(unsigned int i = 0; i < Items2.Size(); i++)
		PWO_MenuOptions[i] = Items2[i];

	PWO_MenuOptionsSz = Items2.Size();
}

int PWO_GetNumForName(const char* classname, float weight)
{
	int current_index = -1;
    for(unsigned int i = 0; i < PWODefsByOrderSz; i++)
    {
        if(PWODefsByOrder[i]->Order->GetGenericRep(CVAR_Float).Float != weight)
            continue;
        if(!PWODefsByOrder[i]->Name.CompareNoCase(classname))
        {
            current_index = i;
            break;
        }
    }
	return current_index;
}