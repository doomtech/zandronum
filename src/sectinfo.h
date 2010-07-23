#ifndef __SECTINFO_H__
#define __SECTINFO_H__

#include "doomtype.h"

void SECTINFO_Load();
void SECTINFO_Parse(int lump);
FString SECTINFO_GetPlayerLocation( const ULONG ulPlayer );

struct SectInfo
{
	SectInfo();
	~SectInfo();

	TArray<FString *> Names;
	TArray<bool> Base[2];

	//Domination Points
	TArray< TArray<unsigned int> *> Points;
	TArray<FString *> PointNames;
};

#endif //__SECTINFO_H__
