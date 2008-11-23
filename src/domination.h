#ifndef __DOMINATION_H__
#define __DOMINATION_H__

#include "gamemode.h"

#include "doomtype.h"
#include "doomstat.h"
#include "v_font.h"
#include "v_video.h"
#include "v_text.h"
#include "chat.h"
#include "st_stuff.h"

#include "g_level.h"
#include "network.h"
#include "r_defs.h"
#include "sv_commands.h"
#include "team.h"
#include "sectinfo.h"

void DOMINATION_LoadInit(unsigned int numpoints, unsigned int* pointowners);
void DOMINATION_WinSequence(unsigned int winner);
void DOMINATION_Tick(void);
void DOMINATION_SetOwnership(unsigned int point, player_t *toucher);
void DOMINATION_EnterSector(player_t *toucher);
void DOMINATION_Init(void);
void DOMINATION_DrawHUD(bool scaled);
unsigned int DOMINATION_NumPoints(void);
unsigned int* DOMINATION_PointOwners(void);
void DOMINATION_Reset(void);

#endif // __DOMINATION_H__
