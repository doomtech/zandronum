#ifndef __SECTINFO_H__
#define __SECTINFO_H__

#include "doomtype.h"

void SECTINFO_Load();
void SECTINFO_Parse(int lump);

struct SectInfo
{
	SectInfo();
	~SectInfo();

	TArray<FString *> Names;
	TArray<bool> Base[2];
};

#endif //__SECTINFO_H__
