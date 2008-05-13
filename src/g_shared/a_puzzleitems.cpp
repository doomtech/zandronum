#include "info.h"
#include "a_pickups.h"
#include "a_artifacts.h"
#include "gstrings.h"
#include "p_local.h"
#include "p_enemy.h"
#include "s_sound.h"
#include "c_console.h"
#include "cl_demo.h"
#include "deathmatch.h"
#include "network.h"
#include "sv_commands.h"

IMPLEMENT_STATELESS_ACTOR (APuzzleItem, Any, -1, 0)
	PROP_Flags (MF_SPECIAL|MF_NOGRAVITY)
	PROP_Inventory_FlagsSet (IF_INVBAR)
	PROP_Inventory_DefMaxAmount
	PROP_UseSound ("PuzzleSuccess")
	PROP_Inventory_PickupSound ("misc/i_pkup")
END_DEFAULTS

void APuzzleItem::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << PuzzleItemNumber;
}

bool APuzzleItem::HandlePickup (AInventory *item)
{
	// Can't carry more than 1 of each puzzle item in coop netplay
	if (( NETWORK_GetState( ) != NETSTATE_SINGLE ) && !deathmatch && item->GetClass() == GetClass())
	{
		return true;
	}
	return Super::HandlePickup (item);
}

bool APuzzleItem::Use (bool pickup)
{
	// [BC] Puzzle item usage is done server-side.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return ( false );
	}

	if (P_UsePuzzleItem (Owner, PuzzleItemNumber))
	{
		return true;
	}
	// [RH] Always play the sound if the use fails.
	S_Sound (Owner, CHAN_VOICE, "*puzzfail", 1, ATTN_IDLE);

	// [BC] If we're the server, play the sound.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SoundActor( Owner, CHAN_VOICE, "*puzzfail", 1, ATTN_IDLE );

	// [BB] The server has to generate the message in any case.
	if (Owner != NULL && ( Owner->CheckLocalView (consoleplayer) || ( NETWORK_GetState( ) == NETSTATE_SERVER ) ) )
	{
		const char *message = GetClass()->Meta.GetMetaString (AIMETA_PuzzFailMessage);
		if (message != NULL && *message=='$') message = GStrings[message + 1];
		if (message == NULL) message = GStrings("TXT_USEPUZZLEFAILED");
		C_MidPrintBold (message);

		// [BB] If we're the server, print the message. This sends the message to all players.
		// Should be tweaked so that it only is shown to those who are watching through the
		// eyes of Owner-player.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_PrintMid( message, true );
	}

	return false;
}

bool APuzzleItem::ShouldStay ()
{
	return ( NETWORK_GetState( ) != NETSTATE_SINGLE );
}

