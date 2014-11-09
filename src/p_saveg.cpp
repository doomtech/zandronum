/*
** p_saveg.cpp
** Code for serializing the world state in an archive
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
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

#include "i_system.h"
#include "p_local.h"

// State.
#include "dobject.h"
#include "doomstat.h"
#include "r_state.h"
#include "m_random.h"
#include "p_saveg.h"
#include "s_sndseq.h"
#include "v_palette.h"
#include "a_sharedglobal.h"
#include "r_interpolate.h"
#include "g_level.h"
#include "po_man.h"
// [BB] New #includes.
#include "deathmatch.h"
#include "cl_demo.h"
#include "network.h"

static void CopyPlayer (player_t *dst, player_t *src, const char *name);
static void ReadOnePlayer (FArchive &arc, bool skipload);
static void ReadMultiplePlayers (FArchive &arc, int numPlayers, int numPlayersNow, bool skipload);
static void SpawnExtraPlayers ();


//
// P_ArchivePlayers
//
void P_SerializePlayers (FArchive &arc, bool skipload)
{
	BYTE numPlayers, numPlayersNow;
	int i;

	// Count the number of players present right now.
	for (numPlayersNow = 0, i = 0; i < MAXPLAYERS; ++i)
	{
		if (playeringame[i])
		{
			++numPlayersNow;
		}
	}

	if (arc.IsStoring())
	{
		// Record the number of players in this save.
		arc << numPlayersNow;

		// Record each player's name, followed by their data.
		for (i = 0; i < MAXPLAYERS; ++i)
		{
			if (playeringame[i])
			{
				arc.WriteString (players[i].userinfo.netname);
				players[i].Serialize (arc);
			}
		}
	}
	else
	{
		arc << numPlayers;

		// If there is only one player in the game, they go to the
		// first player present, no matter what their name.
		if (numPlayers == 1)
		{
			ReadOnePlayer (arc, skipload);
		}
		else
		{
			ReadMultiplePlayers (arc, numPlayers, numPlayersNow, skipload);
		}
		if (!skipload && numPlayersNow > numPlayers)
		{
			SpawnExtraPlayers ();
		}
	}
}

static void ReadOnePlayer (FArchive &arc, bool skipload)
{
	int i;
	char *name = NULL;
	bool didIt = false;

	arc << name;

	for (i = 0; i < MAXPLAYERS; ++i)
	{
		if (playeringame[i])
		{
			if (!didIt)
			{
				didIt = true;
				player_t playerTemp;
				playerTemp.Serialize (arc);
				if (!skipload)
				{
					CopyPlayer (&players[i], &playerTemp, name);
				}
			}
			else
			{
				if (players[i].mo != NULL)
				{
					players[i].mo->Destroy();
					players[i].mo = NULL;
				}
			}
		}
	}
	delete[] name;
}

static void ReadMultiplePlayers (FArchive &arc, int numPlayers, int numPlayersNow, bool skipload)
{
	// For two or more players, read each player into a temporary array.
	int i, j;
	char **nametemp = new char *[numPlayers];
	player_t *playertemp = new player_t[numPlayers];
	BYTE *tempPlayerUsed = new BYTE[numPlayers];
	BYTE playerUsed[MAXPLAYERS];

	for (i = 0; i < numPlayers; ++i)
	{
		nametemp[i] = NULL;
		arc << nametemp[i];
		playertemp[i].Serialize (arc);
		tempPlayerUsed[i] = 0;
	}
	for (i = 0; i < MAXPLAYERS; ++i)
	{
		playerUsed[i] = playeringame[i] ? 0 : 2;
	}

	if (!skipload)
	{
		// Now try to match players from the savegame with players present
		// based on their names. If two players in the savegame have the
		// same name, then they are assigned to players in the current game
		// on a first-come, first-served basis.
		for (i = 0; i < numPlayers; ++i)
		{
			for (j = 0; j < MAXPLAYERS; ++j)
			{
				if (playerUsed[j] == 0 && stricmp(players[j].userinfo.netname, nametemp[i]) == 0)
				{ // Found a match, so copy our temp player to the real player
					Printf ("Found player %d (%s) at %d\n", i, nametemp[i], j);
					CopyPlayer (&players[j], &playertemp[i], nametemp[i]);
					playerUsed[j] = 1;
					tempPlayerUsed[i] = 1;
					break;
				}
			}
		}

		// Any players that didn't have matching names are assigned to existing
		// players on a first-come, first-served basis.
		for (i = 0; i < numPlayers; ++i)
		{
			if (tempPlayerUsed[i] == 0)
			{
				for (j = 0; j < MAXPLAYERS; ++j)
				{
					if (playerUsed[j] == 0)
					{
						Printf ("Assigned player %d (%s) to %d (%s)\n", i, nametemp[i], j, players[j].userinfo.netname);
						CopyPlayer (&players[j], &playertemp[i], nametemp[i]);
						playerUsed[j] = 1;
						tempPlayerUsed[i] = 1;
						break;
					}
				}
			}
		}

		// Make sure any extra players don't have actors spawned yet. Happens if the players
		// present now got the same slots as they had in the save, but there are not as many
		// as there were in the save.
		for (j = 0; j < MAXPLAYERS; ++j)
		{
			if (playerUsed[j] == 0)
			{
				if (players[j].mo != NULL)
				{
					players[j].mo->Destroy();
					players[j].mo = NULL;
				}
			}
		}

		// Remove any temp players that were not used. Happens if there are fewer players
		// than there were in the save, and they got shuffled.
		for (i = 0; i < numPlayers; ++i)
		{
			if (tempPlayerUsed[i] == 0)
			{
				playertemp[i].mo->Destroy();
				playertemp[i].mo = NULL;
			}
		}
	}

	delete[] tempPlayerUsed;
	delete[] playertemp;
	for (i = 0; i < numPlayers; ++i)
	{
		delete[] nametemp[i];
	}
	delete[] nametemp;
}

static void CopyPlayer (player_t *dst, player_t *src, const char *name)
{
	// The userinfo needs to be saved for real players, but it
	// needs to come from the save for bots.
	userinfo_t uibackup = dst->userinfo;
	int chasecam = dst->cheats & CF_CHASECAM;	// Remember the chasecam setting
	bool attackdown = dst->attackdown;
	bool usedown = dst->usedown;
	*dst = *src;
	dst->cheats |= chasecam;

	dst->userinfo = uibackup;
	// Make sure the player pawn points to the proper player struct.
	if (dst->mo != NULL)
	{
		dst->mo->player = dst;
	}
	// These 2 variables may not be overwritten.
	dst->attackdown = attackdown;
	dst->usedown = usedown;
}

static void SpawnExtraPlayers ()
{
	// If there are more players now than there were in the savegame,
	// be sure to spawn the extra players.
	int i;

	if (deathmatch)
	{
		return;
	}

	for (i = 0; i < MAXPLAYERS; ++i)
	{
		if (playeringame[i] && players[i].mo == NULL)
		{
			players[i].playerstate = PST_ENTER;
			P_SpawnPlayer (&playerstarts[i], false, &players[i]);
		}
	}
}

//
// P_ArchiveWorld
//
void P_SerializeWorld (FArchive &arc)
{
	int i, j;
	sector_t *sec;
	line_t *li;
	zone_t *zn;

	// [BC] In client mode, just archive whether or not the line's been seen.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		// do lines
		for (i = 0, li = lines; i < numlines; i++, li++)
		{
			arc << li->flags;
		}

		return;
	}

	// do sectors
	for (i = 0, sec = sectors; i < numsectors; i++, sec++)
	{
		arc << sec->floorplane
			<< sec->ceilingplane
			<< sec->lightlevel
			<< sec->special
			<< sec->tag
			<< sec->soundtraversed
			<< sec->seqType
			<< sec->friction
			<< sec->movefactor
			<< sec->floordata
			<< sec->ceilingdata
			<< sec->lightingdata
			<< sec->stairlock
			<< sec->prevsec
			<< sec->nextsec
			<< sec->planes[sector_t::floor]
			<< sec->planes[sector_t::ceiling]
			<< sec->heightsec
			<< sec->bottommap << sec->midmap << sec->topmap
			<< sec->gravity
			<< sec->damage
			<< sec->mod
			<< sec->SoundTarget
			<< sec->SecActTarget
			<< sec->sky
			<< sec->MoreFlags
			<< sec->Flags
			<< sec->FloorSkyBox << sec->CeilingSkyBox
			<< sec->ZoneNumber
			<< sec->secretsector
			<< sec->interpolations[0]
			<< sec->interpolations[1]
			<< sec->interpolations[2]
			<< sec->interpolations[3];

		if (SaveVersion < 2492)
		{
			sec->SeqName = NAME_None;
		}
		else
		{
			arc << sec->SeqName;
		}

		sec->e->Serialize(arc);
		if (arc.IsStoring ())
		{
			arc << sec->ColorMap->Color
				<< sec->ColorMap->Fade;
			BYTE sat = sec->ColorMap->Desaturate;
			arc << sat;
		}
		else
		{
			PalEntry color, fade;
			BYTE desaturate;
			arc << color << fade
				<< desaturate;
			sec->ColorMap = GetSpecialLights (color, fade, desaturate);
		}
		arc << sec->ceiling_reflect << sec->floor_reflect;

		// [BC]
		arc << sec->floorOrCeiling
			<< sec->bCeilingHeightChange
			<< sec->bFloorHeightChange
			<< sec->SavedCeilingPlane
			<< sec->SavedFloorPlane
			<< sec->SavedCeilingTexZ
			<< sec->SavedFloorTexZ
			<< sec->bFlatChange
			<< sec->SavedFloorPic
			<< sec->SavedCeilingPic
			<< sec->bLightChange
			<< sec->SavedLightLevel;
		if (arc.IsStoring ())
		{
			arc << sec->SavedColorMap->Color
				<< sec->SavedColorMap->Fade;
			BYTE sat = sec->SavedColorMap->Desaturate;
			arc << sat;
		}
		else
		{
			PalEntry color, fade;
			BYTE desaturate;
			arc << color << fade
				<< desaturate;
			sec->SavedColorMap = GetSpecialLights (color, fade, desaturate);
		}
		arc << sec->SavedGravity
			<< sec->SavedFloorXOffset
			<< sec->SavedFloorYOffset
			<< sec->SavedCeilingXOffset
			<< sec->SavedCeilingYOffset
			<< sec->SavedFloorXScale
			<< sec->SavedFloorYScale
			<< sec->SavedCeilingXScale
			<< sec->SavedCeilingYScale
			<< sec->SavedFloorAngle
			<< sec->SavedCeilingAngle
			<< sec->SavedBaseFloorAngle
			<< sec->SavedBaseFloorYOffset
			<< sec->SavedBaseCeilingAngle
			<< sec->SavedBaseCeilingYOffset
			<< sec->SavedFriction
			<< sec->SavedMoveFactor
			<< sec->SavedSpecial
			<< sec->SavedDamage
			<< sec->SavedMOD
			<< sec->SavedCeilingReflect
			<< sec->SavedFloorReflect;
	}

	// do lines
	for (i = 0, li = lines; i < numlines; i++, li++)
	{
		arc << li->flags
			<< li->activation
			<< li->special
			<< li->Alpha
			<< li->id
			<< li->args[0] << li->args[1] << li->args[2] << li->args[3] << li->args[4];
		// [BC]
		arc << li->ulTexChangeFlags
			<< li->SavedSpecial
			<< li->SavedArgs[0] << li->SavedArgs[1] << li->SavedArgs[2] << li->SavedArgs[3] << li->SavedArgs[4]
			<< li->SavedFlags
			<< li->SavedAlpha;

		for (j = 0; j < 2; j++)
		{
			if (li->sidedef[j] == NULL)
				continue;

			side_t *si = li->sidedef[j];
			arc << si->textures[side_t::top]
				<< si->textures[side_t::mid]
				<< si->textures[side_t::bottom]
				<< si->Light
				<< si->Flags
				<< si->LeftSide
				<< si->RightSide
				<< si->Index;
			DBaseDecal::SerializeChain (arc, &si->AttachedDecals);
			// [BC]
			arc << si->SavedFlags
				<< si->SavedTopTexture
				<< si->SavedMidTexture
				<< si->SavedBottomTexture;
		}
	}

	// do zones
	arc << numzones;

	if (arc.IsLoading())
	{
		if (zones != NULL)
		{
			delete[] zones;
		}
		zones = new zone_t[numzones];
	}

	for (i = 0, zn = zones; i < numzones; ++i, ++zn)
	{
		arc << zn->Environment;
	}
}

void extsector_t::Serialize(FArchive &arc)
{
	arc << FakeFloor.Sectors
		<< Midtex.Floor.AttachedLines 
		<< Midtex.Floor.AttachedSectors
		<< Midtex.Ceiling.AttachedLines
		<< Midtex.Ceiling.AttachedSectors
		<< Linked.Floor.Sectors
		<< Linked.Ceiling.Sectors

		// [Dusk] Store the move distance too
		<< Midtex.Floor.MoveDistance
		<< Midtex.Ceiling.MoveDistance;
}

FArchive &operator<< (FArchive &arc, side_t::part &p)
{
	arc << p.xoffset << p.yoffset << p.interpolation << p.texture 
		<< p.xscale << p.yscale;// << p.Light;
	return arc;
}

FArchive &operator<< (FArchive &arc, sector_t::splane &p)
{
	arc << p.xform.xoffs << p.xform.yoffs << p.xform.xscale << p.xform.yscale 
		<< p.xform.angle << p.xform.base_yoffs << p.xform.base_angle
		<< p.Flags << p.Light << p.Texture << p.TexZ;
	return arc;
}


//
// Thinkers
//

//
// P_ArchiveThinkers
//

void P_SerializeThinkers (FArchive &arc, bool hubLoad)
{
	DImpactDecal::SerializeTime (arc);
	DThinker::SerializeAll (arc, hubLoad);
}

//==========================================================================
//
// ArchiveSounds
//
//==========================================================================

void P_SerializeSounds (FArchive &arc)
{
	S_SerializeSounds (arc);
	DSeqNode::SerializeSequences (arc);
	char *name = NULL;
	BYTE order;

	if (arc.IsStoring ())
	{
		order = S_GetMusic (&name);
	}
	arc << name << order;
	if (arc.IsLoading ())
	{
		if (!S_ChangeMusic (name, order))
			if (level.cdtrack == 0 || !S_ChangeCDMusic (level.cdtrack, level.cdid))
				S_ChangeMusic (level.Music, level.musicorder);
	}
	delete[] name;
}

//==========================================================================
//
// ArchivePolyobjs
//
//==========================================================================
#define ASEG_POLYOBJS	104

void P_SerializePolyobjs (FArchive &arc)
{
	int i;
	FPolyObj *po;

	if (arc.IsStoring ())
	{
		int seg = ASEG_POLYOBJS;
		arc << seg << po_NumPolyobjs;
		for(i = 0, po = polyobjs; i < po_NumPolyobjs; i++, po++)
		{
			arc << po->tag << po->angle << po->StartSpot.x <<
				po->StartSpot.y << po->interpolation;
  		}
	}
	else
	{
		int data;
		angle_t angle;
		fixed_t deltaX, deltaY;

		arc << data;
		if (data != ASEG_POLYOBJS)
			I_Error ("Polyobject marker missing");

		arc << data;
		if (data != po_NumPolyobjs)
		{
			I_Error ("UnarchivePolyobjs: Bad polyobj count");
		}
		for (i = 0, po = polyobjs; i < po_NumPolyobjs; i++, po++)
		{
			arc << data;
			if (data != po->tag)
			{
				I_Error ("UnarchivePolyobjs: Invalid polyobj tag");
			}
			arc << angle;
			po->RotatePolyobj (angle);
			arc << deltaX << deltaY << po->interpolation;
			deltaX -= po->StartSpot.x;
			deltaY -= po->StartSpot.y;
			po->MovePolyobj (deltaX, deltaY, true);
		}
	}
}
