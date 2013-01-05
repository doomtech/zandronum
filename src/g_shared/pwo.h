#ifndef _PWO_H_
#define _PWO_H_

#include "../zstring.h"
#include "../c_cvars.h"
#include "../tarray.h"
#include "a_pickups.h"
#include "../m_menu.h"

struct PWODef
{
	FString			Section;
	unsigned int	GameFilter;
	bool			ClassDef;
	FString			Name;
	FString			MenuName;
	float			DefaultOrder;
	unsigned int	MenuWeight;
	FString			Replacee;
	bool			Clear;

	TArray<PWODef>	ClassChildren;

	FBaseCVar*		Order;

	PWODef()
	{
		Clear = false;
		Section = "";
		GameFilter = 0;
		ClassDef = false;
		Name = "";
		MenuName = "";
		DefaultOrder = 0.0;
		MenuWeight = 0;
		Replacee = "";
		ClassChildren.Clear();
	}
};

extern TArray<PWODef> PWODefs;
extern PWODef** PWODefsByOrder;
extern unsigned int PWODefsByOrderSz;

void PWO_SortDefs();
int PWO_GetNumForName(const char* classname, float weight); // get definition number with specified weight

void PWO_LoadDefs();

bool PWO_WeightByClass(AWeapon* weap, float& weight);
bool PWO_CheckWeapons(AWeapon* first, AWeapon* pending);

extern menuitem_t* PWO_MenuOptions;
extern unsigned int PWO_MenuOptionsSz;
void PWO_GenerateMenu();

#endif