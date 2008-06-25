#include "doomtype.h"
#include "w_wad.h"
#include "sc_man.h"
#include "r_defs.h"
#include "g_level.h"
#include "sectinfo.h"

SectInfo::SectInfo()
{
	Names = TArray<FString *>();
	Base[0] = TArray<bool>();
	Base[1] = TArray<bool>();
}

SectInfo::~SectInfo()
{
	Names.Clear();
	Base[0].Clear();
	Base[1].Clear();
}

void SECTINFO_Load()
{
	if(Wads.CheckNumForName("SECTINFO") != -1)
	{
		Printf ("ParseSectInfo: Loading sector identifications.\n");
		int lastlump, lump;
		lastlump = 0;
		while((lump = Wads.FindLump("SECTINFO", &lastlump)) != -1)
			SECTINFO_Parse(lump);
	}
}

//The next few functions do not need to be public so lets define them here:
void SECTINFO_ParseNames(FScanner &sc, TArray<FString *> &SectorNames);
void SECTINFO_ParseSectors(FScanner &sc, TArray<bool> &Sectors);

// Valid properties
static const char *SectInfoProperties[] =
{
	"names",
	"base0",
	"base1",
	NULL
};
enum
{
	SECTINFO_NAMES,
	SECTINFO_BASE0,
	SECTINFO_BASE1,
};

void SECTINFO_Parse(int lump)
{
	level_info_t *mapinfo;

	FScanner sc(lump);
	sc.SetCMode(true);
	while(true)
	{
		if(sc.CheckToken('['))
		{
			sc.MustGetToken(TK_Identifier);
			//If you define a SectInfo for a map not defined by mapinfo interesting results may occur.
			//This is because it will set the sectinfo for the defaultmap.
			mapinfo = FindLevelInfo(sc.String);
			sc.MustGetToken(']');
		}
		else if(mapinfo != NULL)
		{
			if(!sc.CheckToken(TK_Identifier))
			{
				break;
			}
			switch(sc.MustMatchString(SectInfoProperties))
			{
				case SECTINFO_NAMES:
					SECTINFO_ParseNames(sc, mapinfo->SectorInfo.Names);
					break;
				case SECTINFO_BASE0: // I think this needs to be reversed for ZDaemon compatibility (ZDaemon uses 0 as red 1 as blue), but then Skulltag mappers would be confused...
					SECTINFO_ParseSectors(sc, mapinfo->SectorInfo.Base[0]);
					break;
				case SECTINFO_BASE1:
					SECTINFO_ParseSectors(sc, mapinfo->SectorInfo.Base[1]);
					break;
			}
		}
		else
		{
			sc.ScriptError("Invalid Syntax");
		}
	}
}

void SECTINFO_ParseNames(FScanner &sc, TArray<FString *> &SectorNames)
{
	sc.MustGetToken('=');
	sc.MustGetToken('{');
	while(!sc.CheckToken('}'))
	{
		sc.MustGetToken(TK_StringConst);
		FString *name = new FString(sc.String);
		sc.MustGetToken('=');
		sc.MustGetToken('{');
		while(!sc.CheckToken('}'))
		{
			sc.MustGetToken(TK_IntConst);
			int range_start = sc.Number;
			int range_end = sc.Number;
			if(sc.CheckToken('-'))
			{
				sc.MustGetToken(TK_IntConst);
				range_end = sc.Number;
			}
			if(SectorNames.Size() < static_cast<unsigned> (range_end+1))
			{
				unsigned int oldSize = SectorNames.Size();
				SectorNames.Resize(sc.Number+1);
				for(;oldSize < static_cast<unsigned> (sc.Number+1);oldSize++) //Init values to NULL before assignment
					SectorNames[oldSize] = NULL;
			}
			for(int i = range_start;i <= range_end;i++)
				SectorNames[i] = name;
			if(!sc.CheckToken(','))
			{
				sc.MustGetToken('}');
				break;
			}
		}
		if(!sc.CheckToken(','))
		{
			sc.MustGetToken('}');
			break;
		}
	}
}

void SECTINFO_ParseSectors(FScanner &sc, TArray<bool> &Sectors)
{
	sc.MustGetToken('=');
	sc.MustGetToken('{');
	while(!sc.CheckToken('}'))
	{
		sc.MustGetToken(TK_IntConst);
		int range_start = sc.Number;
		int range_end = sc.Number;
		if(sc.CheckToken('-'))
		{
			sc.MustGetToken(TK_IntConst);
			range_end = sc.Number;
		}
		if(Sectors.Size() < static_cast<unsigned> (range_end+1))
		{
			unsigned int oldSize = Sectors.Size();
			Sectors.Resize(sc.Number+1);
			for(;oldSize < static_cast<unsigned> (sc.Number+1);oldSize++) // Init to false
				Sectors[oldSize] = false;
		}
		for(int i = range_start;i <= range_end;i++)
			Sectors[i] = true;
		if(!sc.CheckToken(','))
		{
			sc.MustGetToken('}');
			break;
		}
	}
}
