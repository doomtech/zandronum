#include "info.h"
#include "a_pickups.h"
#include "a_artifacts.h"
#include "gstrings.h"
#include "p_local.h"
#include "gi.h"
#include "s_sound.h"
#include "m_random.h"
#include "doomstat.h"
// [BB] New #includes.
#include "cl_demo.h"
#include "deathmatch.h"
#include "network.h"
#include "sv_commands.h"
#include "teaminfo.h"
#include "gamemode.h"
#include "team.h"

static FRandom pr_tele ("TeleportSelf");

// Teleport (self) ----------------------------------------------------------

class AArtiTeleport : public AInventory
{
	DECLARE_CLASS (AArtiTeleport, AInventory)
public:
	bool Use (bool pickup);
};

IMPLEMENT_CLASS (AArtiTeleport)

bool AArtiTeleport::Use (bool pickup)
{
	fixed_t destX;
	fixed_t destY;
	angle_t destAngle;

	// [BC] Let the server decide where we go.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return ( true );
	}

	// [BB] If this is a team game and there are valid team starts for the team
	// the owner is on, teleport to one of the team starts.
	const ULONG ownerTeam = Owner->player ? Owner->player->ulTeam : teams.Size( );
	if ( ( GAMEMODE_GetFlags( GAMEMODE_GetCurrentMode( )) & GMF_PLAYERSONTEAMS )
	     && TEAM_CheckIfValid ( ownerTeam )
	     && ( teams[ownerTeam].TeamStarts.Size( ) > 0 ) )
	{
		unsigned int selections = teams[ownerTeam].TeamStarts.Size ();
		unsigned int i = pr_tele() % selections;
		destX = teams[ownerTeam].TeamStarts[i].x << FRACBITS;
		destY = teams[ownerTeam].TeamStarts[i].y << FRACBITS;
		destAngle = ANG45 * (teams[ownerTeam].TeamStarts[i].angle/45);
	}
	else if (deathmatch)
	{
		unsigned int selections = deathmatchstarts.Size ();
		unsigned int i = pr_tele() % selections;
		destX = deathmatchstarts[i].x;
		destY = deathmatchstarts[i].y;
		destAngle = ANG45 * (deathmatchstarts[i].angle/45);
	}
	else
	{
		destX = playerstarts[Owner->player - players].x;
		destY = playerstarts[Owner->player - players].y;
		destAngle = ANG45 * (playerstarts[Owner->player - players].angle/45);
	}
	P_Teleport (Owner, destX, destY, ONFLOORZ, destAngle, true, true, false);
	bool canlaugh = true;
 	if (Owner->player->morphTics && (Owner->player->MorphStyle & MORPH_UNDOBYCHAOSDEVICE))
 	{ // Teleporting away will undo any morph effects (pig)
		if (!P_UndoPlayerMorph (Owner->player, Owner->player) && (Owner->player->MorphStyle & MORPH_FAILNOLAUGH))
		{
			canlaugh = false;
		}
 	}
	if (canlaugh)
 	{ // Full volume laugh
 		S_Sound (Owner, CHAN_VOICE, "*evillaugh", 1, ATTN_NONE);

		// [BC] Play the laugh for clients.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SoundActor( Owner, CHAN_VOICE, "*evillaugh", 1, ATTN_NONE );
 	}
	return true;
}

//---------------------------------------------------------------------------
//
// FUNC P_AutoUseChaosDevice
//
//---------------------------------------------------------------------------

bool P_AutoUseChaosDevice (player_t *player)
{
	AInventory *arti = player->mo->FindInventory(PClass::FindClass("ArtiTeleport"));

	if (arti != NULL)
	{
		player->mo->UseInventory (arti);
		player->health = player->mo->health = (player->health+1)/2;
		return true;
	}
	return false;
}
