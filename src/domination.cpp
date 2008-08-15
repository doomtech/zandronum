#include "gamemode.h"

#include "doomtype.h"
#include "doomstat.h"
#include "v_font.h"
#include "v_video.h"
#include "v_text.h"
#include "chat.h"
#include "st_stuff.h"
#include "domination.h"

#include "g_level.h"
#include "network.h"
#include "r_defs.h"
#include "sv_commands.h"
#include "team.h"
#include "sectinfo.h"

#define SCORERATE	(TICRATE*3)

// These default the default fade for points.  I hope no one has a grey team...
#define POINT_DEFAULT_R	0x7F
#define POINT_DEFAULT_G	0x7F
#define POINT_DEFAULT_B	0x7F

EXTERN_CVAR(Bool, domination)

//CREATE_GAMEMODE(domination, DOMINATION, "Domination", "DOM", "F1_DOM", GMF_TEAMGAME|GMF_PLAYERSEARNPOINTS|GMF_PLAYERSONTEAMS)

unsigned int *PointOwners;
unsigned int NumPoints;

bool finished;

unsigned int DOMINATION_NumPoints(void) { return NumPoints; }
unsigned int* DOMINATION_PointOwners(void) { return PointOwners; }

void DOMINATION_LoadInit(unsigned int numpoints, unsigned int* pointowners)
{
	if(!domination)
		return;

	finished = false;
	NumPoints = numpoints;
	PointOwners = pointowners;
}

void DOMINATION_SendState(ULONG ulPlayerExtra)
{
	if(!domination)
		return;

	if(SERVER_IsValidClient(ulPlayerExtra) == false)
		return;

	SERVER_CheckClientBuffer(ulPlayerExtra, NumPoints + 4, true);
	NETWORK_WriteLong(&SERVER_GetClient(ulPlayerExtra)->PacketBuffer.ByteStream, NumPoints);
	for(unsigned int i = 0;i < NumPoints;i++)
	{
		//one byte should be enough to hold the value of the team.
		NETWORK_WriteByte(&SERVER_GetClient( ulPlayerExtra )->PacketBuffer.ByteStream, PointOwners[i]);
	}
}

void DOMINATION_Reset(void)
{
	if(!domination)
		return;

	for(unsigned int i = 0;i < level.info->SectorInfo.Points.Size();i++)
	{
		PointOwners[i] = 255;
		for(unsigned int j = 0;j < level.info->SectorInfo.Points[i]->Size();j++)
		{
			if(j < static_cast<unsigned> (numsectors))
				sectors[(*level.info->SectorInfo.Points[i])[0]].SetFade(POINT_DEFAULT_R, POINT_DEFAULT_G, POINT_DEFAULT_B);
		}
	}
}

void DOMINATION_Init(void)
{
	if(!domination)
		return;

	finished = false;
	if(PointOwners != NULL)
		delete[] PointOwners;
	PointOwners = new unsigned int[level.info->SectorInfo.Points.Size()];
	NumPoints = level.info->SectorInfo.Points.Size();

	DOMINATION_Reset();
}

void DOMINATION_Tick(void)
{
	if(!domination)
		return;

	if(finished)
		return;

	if(!(level.maptime % SCORERATE))
	{
		for(unsigned int i = 0;i < NumPoints;i++)
		{
			if(PointOwners[i] != 255)
			{
				TEAM_SetScore(PointOwners[i], TEAM_GetScore(PointOwners[i]) + 1, false);
				if(TEAM_GetScore(PointOwners[i]) >= pointlimit)
				{
					DOMINATION_WinSequence(0);
					break;
				}
			}
		}
	}
}

void DOMINATION_WinSequence(unsigned int winner)
{
	if(!domination)
		return;

	finished = true;
}

void DOMINATION_SetOwnership(unsigned int point, unsigned int team)
{
	if(!domination)
		return;

	PointOwners[point] = team;
	for(unsigned int i = 0;i < level.info->SectorInfo.Points[point]->Size();i++)
	{
		unsigned int secnum = (*level.info->SectorInfo.Points[point])[i];

		//To be replaced with TEAMINFO compatible code
		if(team == TEAM_BLUE)
			sectors[secnum].SetFade(0x00, 0x00, 0xFF);
		else
			sectors[secnum].SetFade(0xFF, 0x00, 0x00);

		if(NETWORK_GetState() == NETSTATE_SERVER)
			SERVERCOMMANDS_SetSectorFade(secnum);
	}
}

void DOMINATION_EnterSector(player_t *toucher)
{
	if(!domination)
		return;

	if(!toucher->bOnTeam) //The toucher must be on a team
		return;

	assert(PointOwners != NULL);
	for(unsigned int point = 0;point < level.info->SectorInfo.Points.Size();point++)
	{
		for(unsigned int i = 0;i < level.info->SectorInfo.Points[point]->Size();i++)
		{
			if(toucher->mo->Sector->sectornum != static_cast<signed> ((*level.info->SectorInfo.Points[point])[i]))
				continue;

			DOMINATION_SetOwnership(point, toucher->ulTeam);
		}
	}
}

void DOMINATION_DrawHUD(bool scaled)
{
	if(!domination)
		return;

	UCVarValue ValWidth = con_virtualwidth.GetGenericRep( CVAR_Int );
	UCVarValue ValHeight = con_virtualheight.GetGenericRep( CVAR_Int );
	float YScale = static_cast<float> (ValHeight.Int) / SCREENHEIGHT;
	for(int i = NumPoints-1;i >= 0;i--)
	{
		FString str;
		if(PointOwners[i] == TEAM_BLUE) //To be replaced with more teaminfo compatible code
			str << "\\ch" << TEAM_GetName(PointOwners[i]);
		else if(PointOwners[i] == TEAM_RED)
			str << "\\cg" << TEAM_GetName(PointOwners[i]);
		else
			str << "-";
		str << "\\c- :" << *level.info->SectorInfo.PointNames[i];
		V_ColorizeString(str);
		if(scaled)
			screen->DrawText(CR_GRAY, ValWidth.Int - SmallFont->StringWidth(str),
							static_cast<int> (ST_Y * YScale) - (NumPoints-i)*SmallFont->GetHeight(), str,
							DTA_VirtualWidth, ValWidth.Int, DTA_VirtualHeight, ValHeight.Int, TAG_DONE);
		else
			screen->DrawText(CR_GRAY, SCREENWIDTH - SmallFont->StringWidth(str),
							ST_Y  - (NumPoints-i)*SmallFont->GetHeight(), str,
							TAG_DONE);
	}
}

//END_GAMEMODE(DOMINATION)
