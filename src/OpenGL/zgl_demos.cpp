/*
** gl_demos.cpp
** Conversion utility to convert Doom 1.9 demos to ZDoom demos.
**
**---------------------------------------------------------------------------
** Copyright 2003 Timothy Stump
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

#include "zgl_main.h"


#define DEMOVER_14 0x68
#define DEMOVER_15 0x69
#define DEMOVER_16 0x6A
#define DEMOVER_17 0x6B
#define DEMOVER_18 0x6C
#define DEMOVER_19 0x6D


extern char defdemoname[128];

static int DemoPlayerInGame[MAXPLAYERS], DemoSkill, DemoEpisode, DemoMap, DemoDM, DemoRespawn, DemoFast, DemoNomonsters, DemoConsoleplayer;


void GL_GetDemoMapName(int episode, int map)
{
   char mapName[9];
   switch (gamemission)
   {
   case doom:
      sprintf(mapName, "E%dM%d", episode, map);
      break;
   case doom2:
   case pack_tnt:
   case pack_plut:
   default:
      sprintf(mapName, "MAP%02d", map);
      break;
   }

   Printf("Demo map: %s\n", mapName);
}


void GL_ParseDemoHeader(BYTE **demo_p)
{
   int i;
   DemoSkill = **demo_p;
   (*demo_p)++;
   DemoEpisode = **demo_p;
   (*demo_p)++;
   DemoMap = **demo_p;
   (*demo_p)++;
   DemoDM = **demo_p;
   (*demo_p)++;
   DemoRespawn = **demo_p;
   (*demo_p)++;
   DemoFast = **demo_p;
   (*demo_p)++;
   DemoNomonsters = **demo_p;
   (*demo_p)++;
   DemoConsoleplayer = **demo_p;
   (*demo_p)++;

   for (i=0 ; i<MAXPLAYERS ; i++)
   {
      DemoPlayerInGame[i] = **demo_p;
      (*demo_p)++;
   }
}


BYTE *GL_BuildNewDemoPtr19(BYTE *demo_p)
{
   BYTE *newDemo = NULL;

   Printf( PRINT_OPENGL, "Attempting conversion of Doom demo (v1.9)...\n");

   GL_ParseDemoHeader(&demo_p);
   GL_GetDemoMapName(DemoEpisode, DemoMap);

   return newDemo;
}


BYTE *GL_ConvertDemo(BYTE *demo_p)
{
   BYTE demoVer = *demo_p++;

   //
   // bunch of things have to happen in here
   // 1. convert old format to new zdoom format
   // 2. backup a big whack o' cvars/flags
   // 3. set those cvars/flags to 
   //

   switch (demoVer)
   {
   default:
      return NULL;
   case DEMOVER_19:
      return GL_BuildNewDemoPtr19(demo_p);
   }
}
