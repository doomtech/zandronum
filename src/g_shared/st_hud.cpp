//-----------------------------------------------------------------------------
//
// Skulltag Source
// Copyright (C) 2008 Benjamin Berkels
// Copyright (C) 2007-2012 Skulltag Development Team
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
// 3. Neither the name of the Skulltag Development Team nor the names of its
//    contributors may be used to endorse or promote products derived from this
//    software without specific prior written permission.
// 4. Redistributions in any form must be accompanied by information on how to
//    obtain complete source code for the software and any accompanying
//    software that uses the software. The source code must either be included
//    in the distribution or be available for no more than the cost of
//    distribution plus a nominal fee, and must be freely redistributable
//    under reasonable conditions. For an executable file, complete source
//    code means the source code for all modules it contains. It does not
//    include source code for modules or files that typically accompany the
//    major components of the operating system on which the executable file
//    runs.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
//
//
// Filename: st_hud.cpp
//
// Description: Contains extensions to the HUD code.
//
//-----------------------------------------------------------------------------

#include "st_hud.h"
#include "c_console.h"
#include "chat.h"
#include "v_video.h"
#include "v_text.h"
#include "gamemode.h"
#include "g_level.h"
#include "doomstat.h"

//*****************************************************************************
//	CONSOLE VARIABLES

CVAR( Bool, cl_drawcoopinfo, true, CVAR_ARCHIVE )

//*****************************************************************************
//	FUNCTIONS

void HUD_DrawTexture( FTexture *Img, int X, int Y, const bool Scale, const int VirtualWidth, const int VirtualHeight )
{
	if ( Scale )
	{
		screen->DrawTexture( Img,
			X,
			Y,
			DTA_VirtualWidth, VirtualWidth,
			DTA_VirtualHeight, VirtualHeight,
			TAG_DONE );
	}
	else
	{
		screen->DrawTexture( Img,
			X,
			Y,
			TAG_DONE );
	}
}

void HUD_DrawTexture( FTexture *Img, int X, int Y )
{
	HUD_DrawTexture( Img, X, Y, ( con_scaletext ) && ( con_virtualwidth > 0 ) && ( con_virtualheight > 0 ), con_virtualwidth, con_virtualheight );
}

void HUD_DrawText( FFont* Font, int Normalcolor, int X, int Y, const char *String, const bool Scale, const int VirtualWidth, const int VirtualHeight )
{
	if ( Scale )
	{
		screen->DrawText( Font, Normalcolor,
			X,
			Y,
			String,
			DTA_VirtualWidth, VirtualWidth,
			DTA_VirtualHeight, VirtualHeight,
			TAG_DONE );
	}
	else
	{
		screen->DrawText( Font, Normalcolor,
			X,
			Y,
			String,
			TAG_DONE );
	}
}

void HUD_DrawText( FFont* Font, int Normalcolor, int X, int Y, const char *String )
{
	HUD_DrawText( Font, Normalcolor, X, Y, String, ( con_scaletext ) && ( con_virtualwidth > 0 ) && ( con_virtualheight > 0 ), con_virtualwidth, con_virtualheight );
}

void HUD_DrawText( int Normalcolor, int X, int Y, const char *String, const bool Scale, const int VirtualWidth, const int VirtualHeight )
{
	HUD_DrawText( SmallFont, Normalcolor, X, Y, String, Scale, VirtualWidth, VirtualHeight );
}

void HUD_DrawTextAligned( int Normalcolor, int Y, const char *String, bool AlignLeft, const bool Scale, const int VirtualWidth, const int VirtualHeight )
{
	int screenWidthSacled = Scale ? VirtualWidth : SCREENWIDTH;
	HUD_DrawText ( Normalcolor, AlignLeft ? 0 : ( screenWidthSacled - SmallFont->StringWidth ( String ) ) , Y, String, Scale, VirtualWidth, VirtualHeight );
}

void DrawHUD_CoopInfo()
{
	// [BB] Only draw the info if the user wishes to see it (cl_drawcoopinfo)
	// and if this is a cooperative game mode. Further don't draw this in single player.
	if ( cl_drawcoopinfo == false
		|| !(GAMEMODE_GetFlags( GAMEMODE_GetCurrentMode( )) & GMF_COOPERATIVE)
		|| NETWORK_GetState() == NETSTATE_SINGLE )
		return;

	bool bScale;
	const int virtualWidth = con_virtualwidth.GetGenericRep( CVAR_Int ).Int;
	const int virtualHeight = con_virtualheight.GetGenericRep( CVAR_Int ).Int;

	if (( con_scaletext ) && ( con_virtualwidth > 0 ) && ( con_virtualheight > 0 ))
		bScale = true;
	else
		bScale = false;

	FString drawString;

	// [BB] We may not draw in the first 4 lines, this is reserved for chat messages.
	// Leave free another line to prevent the keys from being drawn over in ST's
	// fullscreen HUD.
	const int yOffset = 5 * SmallFont->GetHeight( );
	int playersDrawn = 0;

	for ( int i = 0; i < MAXPLAYERS; i++ )
	{
		// [BB] Only draw the info of players who are actually in the game.
		if ( (playeringame[i] == false) || ( players[i].bSpectating ) || (players[i].mo == NULL) )
			continue;

		// [BB] No need to draw the info of the player who's eyes we are looking through.
		if ( players[i].mo->CheckLocalView( consoleplayer ) )
			continue;

		int curYPos = yOffset + (playersDrawn/2) * ( 4 * SmallFont->GetHeight( ) + 3 ) ;

		const bool drawLeft = ( playersDrawn % 2 == 0 );

		// [BB] Draw player name.
		drawString = players[i].userinfo.netname;
		V_ColorizeString( drawString );
		HUD_DrawTextAligned ( CR_GREY, curYPos, drawString.GetChars(), drawLeft, bScale, virtualWidth, virtualHeight );
		curYPos += SmallFont->GetHeight( ) + 1;

		// [BL] Draw the player's location, [BB] but only if the map has any SectorInfo.
		if ( level.info->SectorInfo.Names.Size() > 0 )
		{
			if(level.info->SectorInfo.Names.Size() > static_cast<unsigned> (players[i].mo->Sector->sectornum) && level.info->SectorInfo.Names[players[i].mo->Sector->sectornum] != NULL)
				drawString = *level.info->SectorInfo.Names[players[i].mo->Sector->sectornum];
			else
				drawString = FString("\\cmUnknown Location\\c-");
			V_ColorizeString( drawString );
			HUD_DrawTextAligned ( CR_GREY, curYPos, drawString.GetChars(), drawLeft, bScale, virtualWidth, virtualHeight );
			curYPos += SmallFont->GetHeight( ) + 1;
		}

		// [BB] Draw player health (color coded) and armor.
		EColorRange healthColor = CR_RED;
		// [BB] Player is alive.
		if ( players[i].mo->health > 0 )
		{
			AInventory* pArmor = players[i].mo->FindInventory(RUNTIME_CLASS(ABasicArmor));
			drawString.Format( "%d \\cD/ %d", players[i].mo->health, pArmor ? pArmor->Amount : 0 );
			V_ColorizeString( drawString );
			if ( players[i].mo->health > 66 )
				healthColor = CR_GREEN;
			else if ( players[i].mo->health > 33 )
				healthColor = CR_GOLD;
		}
		else
			drawString = "dead";
		HUD_DrawTextAligned ( healthColor, curYPos, drawString.GetChars(), drawLeft, bScale, virtualWidth, virtualHeight );
		curYPos += SmallFont->GetHeight( ) + 1;

		// [BB] Draw player weapon and Ammo1/Ammo2, but only if the player is alive.
		if ( players[i].ReadyWeapon && players[i].mo->health > 0 )
		{
			drawString = players[i].ReadyWeapon->GetClass()->TypeName;
			if ( players[i].ReadyWeapon->Ammo1 )
				drawString.AppendFormat( " \\cf%d", players[i].ReadyWeapon->Ammo1->Amount );
			else
				drawString += " \\cg-";
			if ( players[i].ReadyWeapon->Ammo2 )
				drawString.AppendFormat( " \\cf%d", players[i].ReadyWeapon->Ammo2->Amount );
			V_ColorizeString( drawString );
			HUD_DrawTextAligned ( CR_GREEN, curYPos, drawString.GetChars(), drawLeft, bScale, virtualWidth, virtualHeight );
		}

		playersDrawn++;
	}
}
