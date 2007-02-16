//-----------------------------------------------------------------------------
//
// Skulltag Source
// Copyright (C) 2003-2005 Brad Carney
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
// Date created:  9/20/05
//
//
// Filename: invasion.cpp
//
// Description: 
//
//	Quick note on how all the spawns work:
//
//		arg[0]: This is the number of items to spawn in the first wave. The amount per wave increases on each successive wave.
//		arg[1]: This is the number of ticks between spawnings of this item (35 = one second).
//		arg[2]: This delays the spawn of this item from the start of the round this number of ticks. (NOTE: If this is set to 255, the monster will spawn after all other monsters are killed).
//		arg[3]: This is the first wave this item should spawn in (this can be left at 0).
//		arg[4]: This is the maximum number of items that can be spawned from this spawner in a round.

//-----------------------------------------------------------------------------

#include "actor.h"
#include "announcer.h"
#include "cooperative.h"
#include "g_game.h"
#include "gi.h"
#include "invasion.h"
#include "m_png.h"
#include "m_random.h"
#include "network.h"
#include "p_local.h"
#include "sbar.h"
#include "sv_commands.h"
#include "v_video.h"

void	SERVERCONSOLE_UpdateScoreboard( );

//*****************************************************************************
//	PROTOTYPES

static	ULONG		invasion_GetNumThingsThisWave( ULONG ulNumOnFirstWave, ULONG ulWave, bool bMonster );

//*****************************************************************************
//	VARIABLES

static	ULONG				g_ulCurrentWave = 0;
static	ULONG				g_ulNumMonstersLeft = 0;
static	ULONG				g_ulNumArchVilesLeft = 0;
static	ULONG				g_ulInvasionCountdownTicks = 0;
static	INVASIONSTATE_e		g_InvasionState;
static	ULONG				g_ulNumBossMonsters = 0;
static	bool				g_bIncreaseNumMonstersOnSpawn = true;

//*****************************************************************************
//	STRUCTURES
/*
// Defined in invasion.h
class ABaseMonsterInvasionSpot : public AActor
{
	DECLARE_STATELESS_ACTOR( ABaseMonsterInvasionSpot, AActor )
public:

	virtual	char	*GetSpawnName( void );
	
	void		Tick( void );
	void		Serialize( FArchive &arc );

	LONG	lNextSpawnTick;
	LONG	lNumLeftThisWave;
};
*/
//*****************************************************************************
//
char *ABaseMonsterInvasionSpot::GetSpawnName( void )
{
	return GetSpawnName( );
}

//*****************************************************************************
//
void ABaseMonsterInvasionSpot::Activate( AActor *pActivator )
{
	flags2 &= ~MF2_DORMANT;
}

//*****************************************************************************
//
void ABaseMonsterInvasionSpot::Deactivate( AActor *pActivator )
{
	flags2 |= MF2_DORMANT;
}

//*****************************************************************************
//
void ABaseMonsterInvasionSpot::Tick( void )
{
	AActor	*pActor;
	AActor	*pFog;

	// Don't do anything unless the round is in progress.
	if (( g_InvasionState != IS_INPROGRESS ) && ( g_InvasionState != IS_BOSSFIGHT ))
		return;

	// This isn't handled client-side.
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
		return;

	if ( bIsBossMonster )
	{
		// Not time to spawn this yet.
		if ( g_ulNumMonstersLeft > g_ulNumBossMonsters )
			return;
	
		// Since this a boss monster, potentially set the invasion state to fighting a boss.
		if (( NETWORK_GetState( ) != NETSTATE_CLIENT ) && ( g_InvasionState != IS_BOSSFIGHT ))
			INVASION_SetState( IS_BOSSFIGHT );
	}

	// Are we ticking down to the next spawn?
	if ( lNextSpawnTick > 0 )
		lNextSpawnTick--;

	// Next spawn tick is 0! Time to spawn something.
	if ( lNextSpawnTick == 0 )
	{
		// Spawn the item.
		g_bIncreaseNumMonstersOnSpawn = false;
		pActor = Spawn( GetSpawnName( ), x, y, z, ALLOW_REPLACE );
		g_bIncreaseNumMonstersOnSpawn = true;

		// If the item spawned in a "not so good" location, delete it and try again next tick.
		if ( P_TestMobjLocation( pActor ) == false )
		{
			// If this is a monster, subtract it from the total monster count, because it
			// was added when it spawned.
			if ( pActor->flags & MF_COUNTKILL )
				level.total_monsters--;

			// Same for items.
			if ( pActor->flags & MF_COUNTITEM )
				level.total_items--;

			pActor->Destroy( );
			return;
		}

		// Since the actor successfully spawned, tell clients.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SpawnThing( pActor );

		// The item successfully spawned. Now, there's one less items that have to spawn.
		lNumLeftThisWave--;

		// If there's still something left to spawn set the number of ticks left to spawn the next one.
		if ( lNumLeftThisWave > 0 )
			lNextSpawnTick = this->args[1] * TICRATE;
		// Otherwise, we're done spawning things this round.
		else
			lNextSpawnTick = -1;

		// Set the proper angle, and spawn the teleport fog.
		pActor->angle = angle;
		if (( this->flags & MF_AMBUSH ) == false )
		{
			pFog = Spawn<ATeleportFog>( x, y, z + TELEFOGHEIGHT, ALLOW_REPLACE );

			// If we're the server, tell clients to spawn the fog.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SpawnThing( pFog );
		}
		pActor->InvasionSpot.pMonsterSpot = this;
		pActor->ulInvasionWave = g_ulCurrentWave;
	}
}

//*****************************************************************************
//
void ABaseMonsterInvasionSpot::Serialize( FArchive &arc )
{
	Super::Serialize( arc );
	arc << (DWORD &)lNextSpawnTick << (DWORD &)lNumLeftThisWave << (DWORD &)lSpawnerID;
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( ABaseMonsterInvasionSpot, Any, -1, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class AGenericMonsterInvasionSpot : public ABaseMonsterInvasionSpot
{
	DECLARE_STATELESS_ACTOR( AGenericMonsterInvasionSpot, ABaseMonsterInvasionSpot )
public:

	virtual char	*GetSpawnName( void );

	char	szSpawnName[32];
};

//*****************************************************************************
//
char *AGenericMonsterInvasionSpot::GetSpawnName( void )
{
	return ( szSpawnName );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( AGenericMonsterInvasionSpot, Any, 5200, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class AWeakMonsterSpot : public ABaseMonsterInvasionSpot
{
	DECLARE_STATELESS_ACTOR( AWeakMonsterSpot, ABaseMonsterInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *AWeakMonsterSpot::GetSpawnName( void )
{
	switch ( M_Random( ) % 7 )
	{
	case 0:

		return ( "ZombieMan" );
	case 1:

		return ( "ShotgunGuy" );
	case 2:

		return ( "DoomImp" );
	case 3:

		return ( "DarkImp" );
	case 4:

		return ( "Demon" );
	case 5:

		return ( "BloodDemon" );
	case 6:
	default:

		return ( "LostSoul" );
	}
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( AWeakMonsterSpot, Doom, 5201, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class APowerfulMonsterSpot : public ABaseMonsterInvasionSpot
{
	DECLARE_STATELESS_ACTOR( APowerfulMonsterSpot, ABaseMonsterInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *APowerfulMonsterSpot::GetSpawnName( void )
{
	switch ( M_Random( ) % 10 )
	{
	case 0:

		return ( "ChaingunGuy" );
	case 1:

		return ( "SuperShotgunGuy" );
	case 2:

		return ( "Cacodemon" );
	case 3:

		return ( "Cacolantern" );
	case 4:

		return ( "Revenant" );
	case 5:

		return ( "HellKnight" );
	case 6:

		return ( "BaronOfHell" );
	case 7:

		return ( "Arachnotron" );
	case 8:

		return ( "Fatso" );
	case 9:
	default:

		return ( "PainElemental" );
	}
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( APowerfulMonsterSpot, Doom, 5202, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class AVeryPowerfulMonsterSpot : public ABaseMonsterInvasionSpot
{
	DECLARE_STATELESS_ACTOR( AVeryPowerfulMonsterSpot, ABaseMonsterInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *AVeryPowerfulMonsterSpot::GetSpawnName( void )
{
	switch ( M_Random( ) % 4 )
	{
	case 0:

		return ( "Abaddon" );
	case 1:

		return ( "Hectebus" );
	case 2:

		return ( "Belphegor" );
	case 3:
	default:

		return ( "Archvile" );
	}
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( AVeryPowerfulMonsterSpot, Doom, 5203, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class AAnyMonsterSpot : public ABaseMonsterInvasionSpot
{
	DECLARE_STATELESS_ACTOR( AAnyMonsterSpot, ABaseMonsterInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *AAnyMonsterSpot::GetSpawnName( void )
{
	switch ( M_Random( ) % 24 )
	{
	case 0:

		return ( "DoomImp" );
	case 1:

		return ( "DarkImp" );
	case 2:

		return ( "Demon" );
	case 3:

		return ( "Spectre" );
	case 4:

		return ( "BloodDemon" );
	case 5:

		return ( "ZombieMan" );
	case 6:

		return ( "ShotgunGuy" );
	case 7:

		return ( "ChaingunGuy" );
	case 8:

		return ( "SuperShotgunGuy" );
	case 9:

		return ( "Cacodemon" );
	case 10:

		return ( "Cacolantern" );
	case 11:

		return ( "Abaddon" );
	case 12:

		return ( "Revenant" );
	case 13:

		return ( "Fatso" );
	case 14:

		return ( "Hectebus" );
	case 15:

		return ( "Arachnotron" );
	case 16:

		return ( "HellKnight" );
	case 17:

		return ( "BaronOfHell" );
	case 18:

		return ( "Belphegor" );
	case 19:

		return ( "LostSoul" );
	case 20:

		return ( "PainElemental" );
	case 21:

		return ( "Cyberdemon" );
	case 22:

		return ( "SpiderMastermind" );
	case 23:
	default:

		return ( "Archvile" );
	}
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( AAnyMonsterSpot, Doom, 5204, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class AImpSpot : public ABaseMonsterInvasionSpot
{
	DECLARE_STATELESS_ACTOR( AImpSpot, ABaseMonsterInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *AImpSpot::GetSpawnName( void )
{
	return ( "DoomImp" );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( AImpSpot, Doom, 5205, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class ADarkImpSpot : public ABaseMonsterInvasionSpot
{
	DECLARE_STATELESS_ACTOR( ADarkImpSpot, ABaseMonsterInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *ADarkImpSpot::GetSpawnName( void )
{
	return ( "DarkImp" );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( ADarkImpSpot, Doom, 5206, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class ADemonSpot : public ABaseMonsterInvasionSpot
{
	DECLARE_STATELESS_ACTOR( ADemonSpot, ABaseMonsterInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *ADemonSpot::GetSpawnName( void )
{
	return ( "Demon" );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( ADemonSpot, Doom, 5207, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class ASpectreSpot : public ABaseMonsterInvasionSpot
{
	DECLARE_STATELESS_ACTOR( ASpectreSpot, ABaseMonsterInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *ASpectreSpot::GetSpawnName( void )
{
	return ( "Spectre" );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( ASpectreSpot, Doom, 5208, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class ABloodDemonSpot : public ABaseMonsterInvasionSpot
{
	DECLARE_STATELESS_ACTOR( ABloodDemonSpot, ABaseMonsterInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *ABloodDemonSpot::GetSpawnName( void )
{
	return ( "BloodDemon" );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( ABloodDemonSpot, Doom, 5209, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class AZombieManSpot : public ABaseMonsterInvasionSpot
{
	DECLARE_STATELESS_ACTOR( AZombieManSpot, ABaseMonsterInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *AZombieManSpot::GetSpawnName( void )
{
	return ( "ZombieMan" );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( AZombieManSpot, Doom, 5210, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class AShotgunGuySpot : public ABaseMonsterInvasionSpot
{
	DECLARE_STATELESS_ACTOR( AShotgunGuySpot, ABaseMonsterInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *AShotgunGuySpot::GetSpawnName( void )
{
	return ( "ShotgunGuy" );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( AShotgunGuySpot, Doom, 5211, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class AChaingunGuySpot : public ABaseMonsterInvasionSpot
{
	DECLARE_STATELESS_ACTOR( AChaingunGuySpot, ABaseMonsterInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *AChaingunGuySpot::GetSpawnName( void )
{
	return ( "ChaingunGuy" );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( AChaingunGuySpot, Doom, 5212, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class ASuperShotgunGuySpot : public ABaseMonsterInvasionSpot
{
	DECLARE_STATELESS_ACTOR( ASuperShotgunGuySpot, ABaseMonsterInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *ASuperShotgunGuySpot::GetSpawnName( void )
{
	return ( "SuperShotgunGuy" );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( ASuperShotgunGuySpot, Doom, 5213, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class ACacodemonSpot : public ABaseMonsterInvasionSpot
{
	DECLARE_STATELESS_ACTOR( ACacodemonSpot, ABaseMonsterInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *ACacodemonSpot::GetSpawnName( void )
{
	return ( "Cacodemon" );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( ACacodemonSpot, Doom, 5214, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class ACacolanternSpot : public ABaseMonsterInvasionSpot
{
	DECLARE_STATELESS_ACTOR( ACacolanternSpot, ABaseMonsterInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *ACacolanternSpot::GetSpawnName( void )
{
	return ( "Cacolantern" );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( ACacolanternSpot, Doom, 5215, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class AAbaddonSpot : public ABaseMonsterInvasionSpot
{
	DECLARE_STATELESS_ACTOR( AAbaddonSpot, ABaseMonsterInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *AAbaddonSpot::GetSpawnName( void )
{
	return ( "Abaddon" );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( AAbaddonSpot, Doom, 5216, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class ARevenantSpot : public ABaseMonsterInvasionSpot
{
	DECLARE_STATELESS_ACTOR( ARevenantSpot, ABaseMonsterInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *ARevenantSpot::GetSpawnName( void )
{
	return ( "Revenant" );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( ARevenantSpot, Doom, 5217, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class AFatsoSpot : public ABaseMonsterInvasionSpot
{
	DECLARE_STATELESS_ACTOR( AFatsoSpot, ABaseMonsterInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *AFatsoSpot::GetSpawnName( void )
{
	return ( "Fatso" );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( AFatsoSpot, Doom, 5218, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class AHectebusSpot : public ABaseMonsterInvasionSpot
{
	DECLARE_STATELESS_ACTOR( AHectebusSpot, ABaseMonsterInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *AHectebusSpot::GetSpawnName( void )
{
	return ( "Hectebus" );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( AHectebusSpot, Doom, 5219, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class AArachnotronSpot : public ABaseMonsterInvasionSpot
{
	DECLARE_STATELESS_ACTOR( AArachnotronSpot, ABaseMonsterInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *AArachnotronSpot::GetSpawnName( void )
{
	return ( "Arachnotron" );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( AArachnotronSpot, Doom, 5220, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class AHellKnightSpot : public ABaseMonsterInvasionSpot
{
	DECLARE_STATELESS_ACTOR( AHellKnightSpot, ABaseMonsterInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *AHellKnightSpot::GetSpawnName( void )
{
	return ( "HellKnight" );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( AHellKnightSpot, Doom, 5221, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class ABaronOfHellSpot : public ABaseMonsterInvasionSpot
{
	DECLARE_STATELESS_ACTOR( ABaronOfHellSpot, ABaseMonsterInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *ABaronOfHellSpot::GetSpawnName( void )
{
	return ( "BaronOfHell" );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( ABaronOfHellSpot, Doom, 5222, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class ABelphegorSpot : public ABaseMonsterInvasionSpot
{
	DECLARE_STATELESS_ACTOR( ABelphegorSpot, ABaseMonsterInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *ABelphegorSpot::GetSpawnName( void )
{
	return ( "Belphegor" );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( ABelphegorSpot, Doom, 5223, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class ALostSoulSpot : public ABaseMonsterInvasionSpot
{
	DECLARE_STATELESS_ACTOR( ALostSoulSpot, ABaseMonsterInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *ALostSoulSpot::GetSpawnName( void )
{
	return ( "LostSoul" );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( ALostSoulSpot, Doom, 5224, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class APainElementalSpot : public ABaseMonsterInvasionSpot
{
	DECLARE_STATELESS_ACTOR( APainElementalSpot, ABaseMonsterInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *APainElementalSpot::GetSpawnName( void )
{
	return ( "PainElemental" );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( APainElementalSpot, Doom, 5225, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class ACyberdemonSpot : public ABaseMonsterInvasionSpot
{
	DECLARE_STATELESS_ACTOR( ACyberdemonSpot, ABaseMonsterInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *ACyberdemonSpot::GetSpawnName( void )
{
	return ( "Cyberdemon" );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( ACyberdemonSpot, Doom, 5226, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class ASpiderMastermindSpot : public ABaseMonsterInvasionSpot
{
	DECLARE_STATELESS_ACTOR( ASpiderMastermindSpot, ABaseMonsterInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *ASpiderMastermindSpot::GetSpawnName( void )
{
	return ( "SpiderMastermind" );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( ASpiderMastermindSpot, Doom, 5227, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class AArchvileSpot : public ABaseMonsterInvasionSpot
{
	DECLARE_STATELESS_ACTOR( AArchvileSpot, ABaseMonsterInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *AArchvileSpot::GetSpawnName( void )
{
	return ( "Archvile" );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( AArchvileSpot, Doom, 5228, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class AWolfensteinSSSpot : public ABaseMonsterInvasionSpot
{
	DECLARE_STATELESS_ACTOR( AWolfensteinSSSpot, ABaseMonsterInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *AWolfensteinSSSpot::GetSpawnName( void )
{
	return ( "WolfensteinSS" );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( AWolfensteinSSSpot, Doom, 5280, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
/*
// Defined in invasion.h
class ABasePickupInvasionSpot : public AActor
{
	DECLARE_STATELESS_ACTOR( ABasePickupInvasionSpot, AActor )
public:

	virtual	char	*GetSpawnName( void );
	
	void		Tick( void );
	void		Serialize( FArchive &arc );
	void		PickedUp( void );

	LONG	lNextSpawnTick;
	LONG	lNumLeftThisWave;
};
*/
//*****************************************************************************
//
char *ABasePickupInvasionSpot::GetSpawnName( void )
{
	return GetSpawnName( );
}

//*****************************************************************************
//
void ABasePickupInvasionSpot::Activate( AActor *pActivator )
{
	flags2 &= ~MF2_DORMANT;
}

//*****************************************************************************
//
void ABasePickupInvasionSpot::Deactivate( AActor *pActivator )
{
	flags2 |= MF2_DORMANT;
}

//*****************************************************************************
//
void ABasePickupInvasionSpot::Tick( void )
{
	AActor			*pActor;
	AActor			*pFog;
	const PClass	*pClass;

	// Don't do anything unless the round is in progress.
	if (( g_InvasionState != IS_INPROGRESS ) &&
		( g_InvasionState != IS_BOSSFIGHT ) &&
		( g_InvasionState != IS_WAVECOMPLETE ) &&
		( g_InvasionState != IS_COUNTDOWN ))
	{
		return;
	}

	// This isn't handled client-side.
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
		return;

	// Are we ticking down to the next spawn?
	if ( lNextSpawnTick > 0 )
		lNextSpawnTick--;

	// Next spawn tick is 0! Time to spawn something.
	if ( lNextSpawnTick == 0 )
	{
		// Spawn the item.
		pActor = Spawn( GetSpawnName( ), x, y, z, ALLOW_REPLACE );

		// Since the actor successfully spawned, tell clients.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SpawnThing( pActor );

		// The item successfully spawned. Now, there's one less items that have to spawn.
		// In server mode, don't decrement how much of this item is left if it's a type of
		// ammunition, since we want ammo to spawn forever.
		pClass = PClass::FindClass( GetSpawnName( ));
		if (( NETWORK_GetState( ) != NETSTATE_SERVER ) ||
			( pClass == NULL ) ||
			( pClass->IsDescendantOf( RUNTIME_CLASS( AAmmo )) == false ))
		{
			lNumLeftThisWave--;
		}

		// Don't spawn anything until this item has been picked up.
		lNextSpawnTick = -1;

		// Set the proper angle, and spawn the teleport fog.
		pActor->angle = angle;
		if (( this->flags & MF_AMBUSH ) == false )
		{
			pFog = Spawn<ATeleportFog>( x, y, z + TELEFOGHEIGHT, ALLOW_REPLACE );

			// If we're the server, tell clients to spawn the fog.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SpawnThing( pFog );
		}
		pActor->InvasionSpot.pPickupSpot = this;
	}
}

//*****************************************************************************
//
void ABasePickupInvasionSpot::Serialize( FArchive &arc )
{
	Super::Serialize( arc );
	arc << (DWORD &)lNextSpawnTick << (DWORD &)lNumLeftThisWave << (DWORD &)lSpawnerID;
}

//*****************************************************************************
//
void ABasePickupInvasionSpot::PickedUp( void )
{
	const PClass	*pClass;

	// If there's still something left to spawn, or if we're the server and this is a type
	// of ammo, set the number of ticks left to spawn the next one.
	pClass = PClass::FindClass( GetSpawnName( ));
	if (( lNumLeftThisWave > 0 ) ||
		(( NETWORK_GetState( ) == NETSTATE_SERVER ) &&
		 ( pClass ) &&
		 ( pClass->IsDescendantOf( RUNTIME_CLASS( AAmmo )))))
	{
		if ( this->args[1] == 0 )
			lNextSpawnTick = 2 * TICRATE;
		else
			lNextSpawnTick = this->args[1] * TICRATE;
	}
	// Otherwise, we're done spawning things this round.
	else
		lNextSpawnTick = -1;
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( ABasePickupInvasionSpot, Any, -1, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class AStimpackSpot : public ABasePickupInvasionSpot
{
	DECLARE_STATELESS_ACTOR( AStimpackSpot, ABasePickupInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *AStimpackSpot::GetSpawnName( void )
{
	return ( "Stimpack" );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( AStimpackSpot, Doom, 5229, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class AMedikitSpot : public ABasePickupInvasionSpot
{
	DECLARE_STATELESS_ACTOR( AMedikitSpot, ABasePickupInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *AMedikitSpot::GetSpawnName( void )
{
	return ( "Medikit" );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( AMedikitSpot, Doom, 5230, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class AHealthBonusSpot : public ABasePickupInvasionSpot
{
	DECLARE_STATELESS_ACTOR( AHealthBonusSpot, ABasePickupInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *AHealthBonusSpot::GetSpawnName( void )
{
	return ( "HealthBonus" );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( AHealthBonusSpot, Doom, 5231, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class AArmorBonusSpot : public ABasePickupInvasionSpot
{
	DECLARE_STATELESS_ACTOR( AArmorBonusSpot, ABasePickupInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *AArmorBonusSpot::GetSpawnName( void )
{
	return ( "ArmorBonus" );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( AArmorBonusSpot, Doom, 5232, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class AMaxHealthBonusSpot : public ABasePickupInvasionSpot
{
	DECLARE_STATELESS_ACTOR( AMaxHealthBonusSpot, ABasePickupInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *AMaxHealthBonusSpot::GetSpawnName( void )
{
	return ( "MaxHealthBonus" );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( AMaxHealthBonusSpot, Doom, 5233, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class AMaxArmorBonusSpot : public ABasePickupInvasionSpot
{
	DECLARE_STATELESS_ACTOR( AMaxArmorBonusSpot, ABasePickupInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *AMaxArmorBonusSpot::GetSpawnName( void )
{
	return ( "MaxArmorBonus" );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( AMaxArmorBonusSpot, Doom, 5234, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class AGreenArmorSpot : public ABasePickupInvasionSpot
{
	DECLARE_STATELESS_ACTOR( AGreenArmorSpot, ABasePickupInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *AGreenArmorSpot::GetSpawnName( void )
{
	return ( "GreenArmor" );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( AGreenArmorSpot, Doom, 5235, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class ABlueArmorSpot : public ABasePickupInvasionSpot
{
	DECLARE_STATELESS_ACTOR( ABlueArmorSpot, ABasePickupInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *ABlueArmorSpot::GetSpawnName( void )
{
	return ( "BlueArmor" );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( ABlueArmorSpot, Doom, 5236, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class ARedArmorSpot : public ABasePickupInvasionSpot
{
	DECLARE_STATELESS_ACTOR( ARedArmorSpot, ABasePickupInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *ARedArmorSpot::GetSpawnName( void )
{
	return ( "RedArmor" );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( ARedArmorSpot, Doom, 5237, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class ADoomsphereSpot : public ABasePickupInvasionSpot
{
	DECLARE_STATELESS_ACTOR( ADoomsphereSpot, ABasePickupInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *ADoomsphereSpot::GetSpawnName( void )
{
	return ( "Doomsphere" );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( ADoomsphereSpot, Doom, 5238, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class AGuardsphereSpot : public ABasePickupInvasionSpot
{
	DECLARE_STATELESS_ACTOR( AGuardsphereSpot, ABasePickupInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *AGuardsphereSpot::GetSpawnName( void )
{
	return ( "Guardsphere" );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( AGuardsphereSpot, Doom, 5239, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class AInvisibilitySphereSpot : public ABasePickupInvasionSpot
{
	DECLARE_STATELESS_ACTOR( AInvisibilitySphereSpot, ABasePickupInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *AInvisibilitySphereSpot::GetSpawnName( void )
{
	return ( "InvisibilitySphere" );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( AInvisibilitySphereSpot, Doom, 5240, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class ABlurSphereSpot : public ABasePickupInvasionSpot
{
	DECLARE_STATELESS_ACTOR( ABlurSphereSpot, ABasePickupInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *ABlurSphereSpot::GetSpawnName( void )
{
	return ( "BlurSphere" );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( ABlurSphereSpot, Doom, 5241, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class AInvulnerabilitySphereSpot : public ABasePickupInvasionSpot
{
	DECLARE_STATELESS_ACTOR( AInvulnerabilitySphereSpot, ABasePickupInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *AInvulnerabilitySphereSpot::GetSpawnName( void )
{
	return ( "InvulnerabilitySphere" );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( AInvulnerabilitySphereSpot, Doom, 5242, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class AMegasphereSpot : public ABasePickupInvasionSpot
{
	DECLARE_STATELESS_ACTOR( AMegasphereSpot, ABasePickupInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *AMegasphereSpot::GetSpawnName( void )
{
	return ( "Megasphere" );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( AMegasphereSpot, Doom, 5243, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class ARandomPowerupSpot : public ABasePickupInvasionSpot
{
	DECLARE_STATELESS_ACTOR( ARandomPowerupSpot, ABasePickupInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *ARandomPowerupSpot::GetSpawnName( void )
{
	return ( "RandomPowerup" );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( ARandomPowerupSpot, Doom, 5244, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class ASoulsphereSpot : public ABasePickupInvasionSpot
{
	DECLARE_STATELESS_ACTOR( ASoulsphereSpot, ABasePickupInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *ASoulsphereSpot::GetSpawnName( void )
{
	return ( "Soulsphere" );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( ASoulsphereSpot, Doom, 5245, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class ATimeFreezeSphereSpot : public ABasePickupInvasionSpot
{
	DECLARE_STATELESS_ACTOR( ATimeFreezeSphereSpot, ABasePickupInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *ATimeFreezeSphereSpot::GetSpawnName( void )
{
	return ( "TimeFreezeSphere" );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( ATimeFreezeSphereSpot, Doom, 5246, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class ATurbosphereSpot : public ABasePickupInvasionSpot
{
	DECLARE_STATELESS_ACTOR( ATurbosphereSpot, ABasePickupInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *ATurbosphereSpot::GetSpawnName( void )
{
	return ( "Turbosphere" );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( ATurbosphereSpot, Doom, 5247, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class AStrengthRuneSpot : public ABasePickupInvasionSpot
{
	DECLARE_STATELESS_ACTOR( AStrengthRuneSpot, ABasePickupInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *AStrengthRuneSpot::GetSpawnName( void )
{
	return ( "StrengthRune" );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( AStrengthRuneSpot, Doom, 5248, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class ARageRuneSpot : public ABasePickupInvasionSpot
{
	DECLARE_STATELESS_ACTOR( ARageRuneSpot, ABasePickupInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *ARageRuneSpot::GetSpawnName( void )
{
	return ( "RageRune" );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( ARageRuneSpot, Doom, 5249, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class ADrainRuneSpot : public ABasePickupInvasionSpot
{
	DECLARE_STATELESS_ACTOR( ADrainRuneSpot, ABasePickupInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *ADrainRuneSpot::GetSpawnName( void )
{
	return ( "DrainRune" );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( ADrainRuneSpot, Doom, 5250, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class ASpreadRuneSpot : public ABasePickupInvasionSpot
{
	DECLARE_STATELESS_ACTOR( ASpreadRuneSpot, ABasePickupInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *ASpreadRuneSpot::GetSpawnName( void )
{
	return ( "SpreadRune" );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( ASpreadRuneSpot, Doom, 5251, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class AResistanceRuneSpot : public ABasePickupInvasionSpot
{
	DECLARE_STATELESS_ACTOR( AResistanceRuneSpot, ABasePickupInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *AResistanceRuneSpot::GetSpawnName( void )
{
	return ( "ResistanceRune" );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( AResistanceRuneSpot, Doom, 5252, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class ARegenerationRuneSpot : public ABasePickupInvasionSpot
{
	DECLARE_STATELESS_ACTOR( ARegenerationRuneSpot, ABasePickupInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *ARegenerationRuneSpot::GetSpawnName( void )
{
	return ( "RegenerationRune" );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( ARegenerationRuneSpot, Doom, 5253, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class AProsperityRuneSpot : public ABasePickupInvasionSpot
{
	DECLARE_STATELESS_ACTOR( AProsperityRuneSpot, ABasePickupInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *AProsperityRuneSpot::GetSpawnName( void )
{
	return ( "ProsperityRune" );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( AProsperityRuneSpot, Doom, 5254, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class AReflectionRuneSpot : public ABasePickupInvasionSpot
{
	DECLARE_STATELESS_ACTOR( AReflectionRuneSpot, ABasePickupInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *AReflectionRuneSpot::GetSpawnName( void )
{
	return ( "ReflectionRune" );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( AReflectionRuneSpot, Doom, 5255, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class AHasteRuneSpot : public ABasePickupInvasionSpot
{
	DECLARE_STATELESS_ACTOR( AHasteRuneSpot, ABasePickupInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *AHasteRuneSpot::GetSpawnName( void )
{
	return ( "HasteRune" );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( AHasteRuneSpot, Doom, 5256, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class AHighjumpRuneSpot : public ABasePickupInvasionSpot
{
	DECLARE_STATELESS_ACTOR( AHighjumpRuneSpot, ABasePickupInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *AHighjumpRuneSpot::GetSpawnName( void )
{
	return ( "HighjumpRune" );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( AHighjumpRuneSpot, Doom, 5257, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class AClipSpot : public ABasePickupInvasionSpot
{
	DECLARE_STATELESS_ACTOR( AClipSpot, ABasePickupInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *AClipSpot::GetSpawnName( void )
{
	return ( "Clip" );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( AClipSpot, Doom, 5258, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class AShellSpot : public ABasePickupInvasionSpot
{
	DECLARE_STATELESS_ACTOR( AShellSpot, ABasePickupInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *AShellSpot::GetSpawnName( void )
{
	return ( "Shell" );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( AShellSpot, Doom, 5259, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class ARocketAmmoSpot : public ABasePickupInvasionSpot
{
	DECLARE_STATELESS_ACTOR( ARocketAmmoSpot, ABasePickupInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *ARocketAmmoSpot::GetSpawnName( void )
{
	return ( "RocketAmmo" );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( ARocketAmmoSpot, Doom, 5260, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class ACellSpot : public ABasePickupInvasionSpot
{
	DECLARE_STATELESS_ACTOR( ACellSpot, ABasePickupInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *ACellSpot::GetSpawnName( void )
{
	return ( "Cell" );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( ACellSpot, Doom, 5261, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class AClipBoxSpot : public ABasePickupInvasionSpot
{
	DECLARE_STATELESS_ACTOR( AClipBoxSpot, ABasePickupInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *AClipBoxSpot::GetSpawnName( void )
{
	return ( "ClipBox" );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( AClipBoxSpot, Doom, 5262, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class AShellBoxSpot : public ABasePickupInvasionSpot
{
	DECLARE_STATELESS_ACTOR( AShellBoxSpot, ABasePickupInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *AShellBoxSpot::GetSpawnName( void )
{
	return ( "ShellBox" );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( AShellBoxSpot, Doom, 5263, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class ARocketBoxSpot : public ABasePickupInvasionSpot
{
	DECLARE_STATELESS_ACTOR( ARocketBoxSpot, ABasePickupInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *ARocketBoxSpot::GetSpawnName( void )
{
	return ( "RocketBox" );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( ARocketBoxSpot, Doom, 5264, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class ACellPackSpot : public ABasePickupInvasionSpot
{
	DECLARE_STATELESS_ACTOR( ACellPackSpot, ABasePickupInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *ACellPackSpot::GetSpawnName( void )
{
	return ( "CellPack" );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( ACellPackSpot, Doom, 5265, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class ARandomClipAmmoSpot : public ABasePickupInvasionSpot
{
	DECLARE_STATELESS_ACTOR( ARandomClipAmmoSpot, ABasePickupInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *ARandomClipAmmoSpot::GetSpawnName( void )
{
	switch ( M_Random( ) % 4 )
	{
	case 0:

		return ( "Clip" );
	case 1:

		return ( "Shell" );
	case 2:

		return ( "RocketAmmo" );
	case 3:
	default:

		return ( "Cell" );
	}
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( ARandomClipAmmoSpot, Doom, 5278, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class ARandomBoxAmmoSpot : public ABasePickupInvasionSpot
{
	DECLARE_STATELESS_ACTOR( ARandomBoxAmmoSpot, ABasePickupInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *ARandomBoxAmmoSpot::GetSpawnName( void )
{
	switch ( M_Random( ) % 4 )
	{
	case 0:

		return ( "ClipBox" );
	case 1:

		return ( "ShellBox" );
	case 2:

		return ( "RocketBox" );
	case 3:
	default:

		return ( "CellPack" );
	}
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( ARandomBoxAmmoSpot, Doom, 5279, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class ABerserkSpot : public ABasePickupInvasionSpot
{
	DECLARE_STATELESS_ACTOR( ABerserkSpot, ABasePickupInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *ABerserkSpot::GetSpawnName( void )
{
	return ( "Berserk" );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( ABerserkSpot, Doom, 5266, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
/*
// Defined in invasion.h"
class ABaseWeaponInvasionSpot : public AActor
{
	DECLARE_STATELESS_ACTOR( ABaseWeaponInvasionSpot, AActor )
public:

	virtual	char	*GetSpawnName( void );
	
	void		Tick( void );
	void		Serialize( FArchive &arc );

	LONG		lNextSpawnTick;
};
*/
//*****************************************************************************
//
char *ABaseWeaponInvasionSpot::GetSpawnName( void )
{
	return GetSpawnName( );
}

//*****************************************************************************
//
void ABaseWeaponInvasionSpot::Activate( AActor *pActivator )
{
	flags2 &= ~MF2_DORMANT;
}

//*****************************************************************************
//
void ABaseWeaponInvasionSpot::Deactivate( AActor *pActivator )
{
	flags2 |= MF2_DORMANT;
}

//*****************************************************************************
//
void ABaseWeaponInvasionSpot::Tick( void )
{
	AActor	*pActor;
	AActor	*pFog;

	// Don't do anything unless the round is in progress.
	if (( g_InvasionState != IS_INPROGRESS ) &&
		( g_InvasionState != IS_BOSSFIGHT ) &&
		( g_InvasionState != IS_WAVECOMPLETE ) &&
		( g_InvasionState != IS_COUNTDOWN ))
	{
		return;
	}

	// This isn't handled client-side.
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
		return;

	// Are we ticking down to the next spawn?
	if ( lNextSpawnTick > 0 )
		lNextSpawnTick--;

	// Next spawn tick is 0! Time to spawn something.
	if ( lNextSpawnTick == 0 )
	{
		// Spawn the item.
		pActor = Spawn( GetSpawnName( ), x, y, z, ALLOW_REPLACE );

		// Make this item stick around in multiplayer games.
		pActor->flags &= ~MF_DROPPED;

		// Since the actor successfully spawned, tell clients.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SpawnThing( pActor );

		// Don't spawn anything until this item has been picked up.
		lNextSpawnTick = -1;

		// Set the proper angle, and spawn the teleport fog.
		pActor->angle = angle;
		if (( this->flags & MF_AMBUSH ) == false )
		{
			pFog = Spawn<ATeleportFog>( x, y, z + TELEFOGHEIGHT, ALLOW_REPLACE );

			// If we're the server, tell clients to spawn the fog.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SpawnThing( pFog );
		}

		// Destroy the weapon spot since it doesn't need to persist.
//		this->Destroy( );
	}
}

//*****************************************************************************
//
void ABaseWeaponInvasionSpot::Serialize( FArchive &arc )
{
	Super::Serialize( arc );
	arc << (DWORD &)lNextSpawnTick;
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( ABaseWeaponInvasionSpot, Any, -1, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class AChainsawSpot : public ABaseWeaponInvasionSpot
{
	DECLARE_STATELESS_ACTOR( AChainsawSpot, ABaseWeaponInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *AChainsawSpot::GetSpawnName( void )
{
	return ( "Chainsaw" );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( AChainsawSpot, Doom, 5267, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class AShotgunSpot : public ABaseWeaponInvasionSpot
{
	DECLARE_STATELESS_ACTOR( AShotgunSpot, ABaseWeaponInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *AShotgunSpot::GetSpawnName( void )
{
	return ( "Shotgun" );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( AShotgunSpot, Doom, 5268, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class ASuperShotgunSpot : public ABaseWeaponInvasionSpot
{
	DECLARE_STATELESS_ACTOR( ASuperShotgunSpot, ABaseWeaponInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *ASuperShotgunSpot::GetSpawnName( void )
{
	return ( "SuperShotgun" );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( ASuperShotgunSpot, Doom, 5269, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class AChaingunSpot : public ABaseWeaponInvasionSpot
{
	DECLARE_STATELESS_ACTOR( AChaingunSpot, ABaseWeaponInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *AChaingunSpot::GetSpawnName( void )
{
	return ( "Chaingun" );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( AChaingunSpot, Doom, 5270, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class AMinigunSpot : public ABaseWeaponInvasionSpot
{
	DECLARE_STATELESS_ACTOR( AMinigunSpot, ABaseWeaponInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *AMinigunSpot::GetSpawnName( void )
{
	return ( "Minigun" );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( AMinigunSpot, Doom, 5271, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class ARocketLauncherSpot : public ABaseWeaponInvasionSpot
{
	DECLARE_STATELESS_ACTOR( ARocketLauncherSpot, ABaseWeaponInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *ARocketLauncherSpot::GetSpawnName( void )
{
	return ( "RocketLauncher" );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( ARocketLauncherSpot, Doom, 5272, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class AGrenadeLauncherSpot : public ABaseWeaponInvasionSpot
{
	DECLARE_STATELESS_ACTOR( AGrenadeLauncherSpot, ABaseWeaponInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *AGrenadeLauncherSpot::GetSpawnName( void )
{
	return ( "GrenadeLauncher" );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( AGrenadeLauncherSpot, Doom, 5273, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class APlasmaRifleSpot : public ABaseWeaponInvasionSpot
{
	DECLARE_STATELESS_ACTOR( APlasmaRifleSpot, ABaseWeaponInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *APlasmaRifleSpot::GetSpawnName( void )
{
	return ( "PlasmaRifle" );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( APlasmaRifleSpot, Doom, 5274, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class ARailgunSpot : public ABaseWeaponInvasionSpot
{
	DECLARE_STATELESS_ACTOR( ARailgunSpot, ABaseWeaponInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *ARailgunSpot::GetSpawnName( void )
{
	return ( "Railgun" );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( ARailgunSpot, Doom, 5275, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class ABFG9000Spot : public ABaseWeaponInvasionSpot
{
	DECLARE_STATELESS_ACTOR( ABFG9000Spot, ABaseWeaponInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *ABFG9000Spot::GetSpawnName( void )
{
	return ( "BFG9000" );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( ABFG9000Spot, Doom, 5276, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//*****************************************************************************
class ABFG10KSpot : public ABaseWeaponInvasionSpot
{
	DECLARE_STATELESS_ACTOR( ABFG10KSpot, ABaseWeaponInvasionSpot )
public:

	virtual char	*GetSpawnName( void );
};

//*****************************************************************************
//
char *ABFG10KSpot::GetSpawnName( void )
{
	return ( "BFG10K" );
}

//*****************************************************************************
IMPLEMENT_STATELESS_ACTOR( ABFG10KSpot, Doom, 5277, 0 )
	PROP_Flags( MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY )
	PROP_RenderStyle( STYLE_None )
END_DEFAULTS

//*****************************************************************************
//	FUNCTIONS

void INVASION_Construct( void )
{
	g_InvasionState = IS_WAITINGFORPLAYERS;
}

//*****************************************************************************
//
void INVASION_Tick( void )
{
	// Not in invasion mode.
	if ( invasion == false )
		return;

	// Don't tick if the game is paused.
	if ( paused )
		return;

	switch ( g_InvasionState )
	{
	case IS_WAITINGFORPLAYERS:

		if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
			break;

		// A player is here! Begin the countdown!
		if ( SERVER_CalcNumNonSpectatingPlayers( MAXPLAYERS ) >= 1 )
		{
			if ( sv_invasioncountdowntime > 0 )
				INVASION_StartFirstCountdown(( sv_invasioncountdowntime * TICRATE ) - 1 );
			else
				INVASION_StartFirstCountdown(( 10 * TICRATE ) - 1 );
		}
		break;
	case IS_FIRSTCOUNTDOWN:

		if ( g_ulInvasionCountdownTicks )
		{
			g_ulInvasionCountdownTicks--;

			// FIGHT!
			if (( g_ulInvasionCountdownTicks == 0 ) && ( NETWORK_GetState( ) != NETSTATE_CLIENT ))
				INVASION_BeginWave( 1 );
			// Play "3... 2... 1..." sounds.
			else if ( g_ulInvasionCountdownTicks == ( 3 * TICRATE ))
				ANNOUNCER_PlayEntry( cl_announcer, "Three" );
			else if ( g_ulInvasionCountdownTicks == ( 2 * TICRATE ))
				ANNOUNCER_PlayEntry( cl_announcer, "Two" );
			else if ( g_ulInvasionCountdownTicks == ( 1 * TICRATE ))
				ANNOUNCER_PlayEntry( cl_announcer, "One" );
		}
		break;
	case IS_WAVECOMPLETE:

		if ( g_ulInvasionCountdownTicks )
		{
			g_ulInvasionCountdownTicks--;

			if (( g_ulInvasionCountdownTicks == 0 ) && ( NETWORK_GetState( ) != NETSTATE_CLIENT ))
			{
				if ( (LONG)g_ulCurrentWave == wavelimit )
					G_ExitLevel( 0, false );
				else
				{
					// FIGHT!
					if ( sv_invasioncountdowntime > 0 )
						INVASION_StartCountdown(( sv_invasioncountdowntime * TICRATE ) - 1 );
					else
						INVASION_StartCountdown(( 10 * TICRATE ) - 1 );
				}
			}
		}
		break;
	case IS_COUNTDOWN:

		if ( g_ulInvasionCountdownTicks )
		{
			g_ulInvasionCountdownTicks--;

			// FIGHT!
			if (( g_ulInvasionCountdownTicks == 0 ) && ( NETWORK_GetState( ) != NETSTATE_CLIENT ))
				INVASION_BeginWave( g_ulCurrentWave + 1 );
			// Play "3... 2... 1..." sounds.
			else if ( g_ulInvasionCountdownTicks == ( 3 * TICRATE ))
				ANNOUNCER_PlayEntry( cl_announcer, "Three" );
			else if ( g_ulInvasionCountdownTicks == ( 2 * TICRATE ))
				ANNOUNCER_PlayEntry( cl_announcer, "Two" );
			else if ( g_ulInvasionCountdownTicks == ( 1 * TICRATE ))
				ANNOUNCER_PlayEntry( cl_announcer, "One" );
		}
		break;
	}
}

//*****************************************************************************
//
void INVASION_StartFirstCountdown( ULONG ulTicks )
{
	// Put the invasion state into the first countdown.
	if ( NETWORK_GetState( ) != NETSTATE_CLIENT )
		INVASION_SetState( IS_FIRSTCOUNTDOWN );

	// Set the invasion countdown ticks.
	INVASION_SetCountdownTicks( ulTicks );

	// Announce that the fight will soon start.
	ANNOUNCER_PlayEntry( cl_announcer, "PrepareToFight" );

	// Tell clients to start the countdown.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_DoGameModeCountdown( ulTicks );

	// Reset the map.
	if ( NETWORK_GetState( ) != NETSTATE_CLIENT )
		GAME_ResetMap( );
}

//*****************************************************************************
//
void INVASION_StartCountdown( ULONG ulTicks )
{
	AActor						*pActor;
	TThinkerIterator<AActor>	ActorIterator;

	// Put the invasion state into the first countdown.
	if ( NETWORK_GetState( ) != NETSTATE_CLIENT )
		INVASION_SetState( IS_COUNTDOWN );

	// Set the invasion countdown ticks.
	INVASION_SetCountdownTicks( ulTicks );

	// Also, clear out dead bodies from two rounds ago.
	if ( g_ulCurrentWave > 1 )
	{
		while (( pActor = ActorIterator.Next( )))
		{
			if (( pActor->flags & MF_COUNTKILL ) == false )
				continue;

			// Get rid of any bodies that didn't come from a spawner.
			if (( pActor->InvasionSpot.pMonsterSpot == NULL ) && ( NETWORK_GetState( ) != NETSTATE_CLIENT ))
			{
				pActor->Destroy( );
				continue;
			}

			// Also, get rid of any bodies from previous waves.
			if ( pActor->ulInvasionWave == ( g_ulCurrentWave - 1 ))
			{
				pActor->Destroy( );
				continue;
			}
		}
	}

	// Announce that the fight will soon start.
	ANNOUNCER_PlayEntry( cl_announcer, "PrepareToFight" );

	// Tell clients to start the countdown.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_DoGameModeCountdown( ulTicks );
}

//*****************************************************************************
//
void INVASION_BeginWave( ULONG ulWave )
{
	DHUDMessageFadeOut							*pMsg;
	AActor										*pActor;
	AActor										*pFog;
	ABaseMonsterInvasionSpot					*pMonsterSpot;
	ABasePickupInvasionSpot						*pPickupSpot;
	ABaseWeaponInvasionSpot						*pWeaponSpot;
	ULONG										ulNumMonstersLeft;
	LONG										lWaveForItem;
	bool										bContinue;
	TThinkerIterator<ABaseMonsterInvasionSpot>	MonsterIterator;
	TThinkerIterator<ABasePickupInvasionSpot>	PickupIterator;
	TThinkerIterator<ABaseWeaponInvasionSpot>	WeaponIterator;
	TThinkerIterator<AActor>					ActorIterator;

	// We're now in the middle of the invasion!
	if ( NETWORK_GetState( ) != NETSTATE_CLIENT )
		INVASION_SetState( IS_INPROGRESS );

	// Make sure this is 0. Can be non-zero in network games if they're slightly out of sync.
	g_ulInvasionCountdownTicks = 0;

	// Tell clients to "fight!".
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_DoGameModeFight( ulWave );

	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
	{
		// Play fight sound.
		ANNOUNCER_PlayEntry( cl_announcer, "Fight" );

		screen->SetFont( BigFont );

		// Display "BEGIN!" HUD message.
		pMsg = new DHUDMessageFadeOut( "BEGIN!",
			160.4f,
			75.0f,
			320,
			200,
			CR_RED,
			2.0f,
			1.0f );

		StatusBar->AttachMessage( pMsg, 'CNTR' );
		screen->SetFont( SmallFont );
	}
	// Display a little thing in the server window so servers can know when waves begin.
	else
		Printf( "BEGIN!\n" );

	g_ulCurrentWave = ulWave;
	g_ulNumMonstersLeft = 0;
	g_ulNumArchVilesLeft = 0;
	g_ulNumBossMonsters = 0;

	// Clients don't need to do any more.
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
		return;

	while (( pMonsterSpot = MonsterIterator.Next( )))
	{
		// Spot is dormant. Don't do anything with it this round.
		if ( pMonsterSpot->flags2 & MF2_DORMANT )
		{
			pMonsterSpot->lNextSpawnTick = -1;
			continue;
		}

		// Not time to spawn this item yet!
		if ( g_ulCurrentWave < (ULONG)pMonsterSpot->args[3] )
		{
			pMonsterSpot->lNextSpawnTick = -1;
			continue;
		}

		lWaveForItem = g_ulCurrentWave - (( pMonsterSpot->args[3] <= 1 ) ? 0 : ( pMonsterSpot->args[3] - 1 ));

		// This item will be used this wave. Calculate the number of things to spawn.
		pMonsterSpot->lNumLeftThisWave = invasion_GetNumThingsThisWave( pMonsterSpot->args[0], lWaveForItem, true );
		if (( pMonsterSpot->args[4] > 0 ) && ( pMonsterSpot->lNumLeftThisWave > pMonsterSpot->args[4] ))
			pMonsterSpot->lNumLeftThisWave = pMonsterSpot->args[4];

		g_ulNumMonstersLeft += pMonsterSpot->lNumLeftThisWave;
		if ( stricmp( pMonsterSpot->GetSpawnName( ), "Archvile" ) == 0 )
			g_ulNumArchVilesLeft += pMonsterSpot->lNumLeftThisWave;

		// Nothing to spawn!
		if ( pMonsterSpot->lNumLeftThisWave <= 0 )
		{
			pMonsterSpot->lNextSpawnTick = -1;
			continue;
		}

		// If this item has a delayed spawn set, just set the ticks until it spawns, and don't
		// spawn it.
		if ( pMonsterSpot->args[2] > 0 )
		{
			// Special exception: if this is set to 255, don't spawn it until everything else is dead.
			if ( pMonsterSpot->args[2] == 255 )
			{
				pMonsterSpot->bIsBossMonster = true;
				pMonsterSpot->lNextSpawnTick = 0;

				g_ulNumBossMonsters += pMonsterSpot->lNumLeftThisWave;
			}
			else
				pMonsterSpot->lNextSpawnTick = pMonsterSpot->args[2] * TICRATE;
			continue;
		}

		// Spawn the item.
		ulNumMonstersLeft = g_ulNumMonstersLeft;
		pActor = Spawn( pMonsterSpot->GetSpawnName( ), pMonsterSpot->x, pMonsterSpot->y, pMonsterSpot->z, ALLOW_REPLACE );
		g_ulNumMonstersLeft = ulNumMonstersLeft;

		// If the item spawned in a "not so good" location, delete it and try again next tick.
		if ( P_TestMobjLocation( pActor ) == false )
		{
			// If this is a monster, subtract it from the total monster count, because it
			// was added when it spawned.
			if ( pActor->flags & MF_COUNTKILL )
				level.total_monsters--;

			// Same for items.
			if ( pActor->flags & MF_COUNTITEM )
				level.total_items--;

			pActor->Destroy( );
			pMonsterSpot->lNextSpawnTick = 0;
			continue;
		}

		// Since the actor successfully spawned, tell clients.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SpawnThing( pActor );

		// The item successfully spawned. Now, there's one less items that have to spawn.
		pMonsterSpot->lNumLeftThisWave--;

		// If there's still something left to spawn set the number of ticks left to spawn the next one.
		if ( pMonsterSpot->lNumLeftThisWave > 0 )
			pMonsterSpot->lNextSpawnTick = pMonsterSpot->args[1] * TICRATE;
		// Otherwise, we're done spawning things this round.
		else
			pMonsterSpot->lNextSpawnTick = -1;

		// Set the proper angle, and spawn the teleport fog.
		pActor->angle = pMonsterSpot->angle;
		if (( pMonsterSpot->flags & MF_AMBUSH ) == false )
		{
			pFog = Spawn<ATeleportFog>( pMonsterSpot->x, pMonsterSpot->y, pMonsterSpot->z + TELEFOGHEIGHT, ALLOW_REPLACE );

			// If we're the server, tell clients to spawn the fog.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SpawnThing( pFog );
		}
		pActor->InvasionSpot.pMonsterSpot = pMonsterSpot;
		pActor->ulInvasionWave = g_ulCurrentWave;
	}

	while (( pPickupSpot = PickupIterator.Next( )))
	{
		// Spot is dormant. Don't do anything with it this round.
		if ( pPickupSpot->flags2 & MF2_DORMANT )
		{
			pPickupSpot->lNextSpawnTick = -1;
			continue;
		}

		// Not time to spawn this item yet!
		if ( g_ulCurrentWave < (ULONG)pPickupSpot->args[3] )
		{
			pPickupSpot->lNextSpawnTick = -1;
			continue;
		}

		lWaveForItem = g_ulCurrentWave - (( pPickupSpot->args[3] <= 1 ) ? 0 : ( pPickupSpot->args[3] - 1 ));

		// This item will be used this wave. Calculate the number of things to spawn.
		pPickupSpot->lNumLeftThisWave = invasion_GetNumThingsThisWave( pPickupSpot->args[0], lWaveForItem, false );
		if (( pPickupSpot->args[4] > 0 ) && ( pPickupSpot->lNumLeftThisWave > pPickupSpot->args[4] ))
			pPickupSpot->lNumLeftThisWave = pPickupSpot->args[4];

		// Nothing to spawn!
		if ( pPickupSpot->lNumLeftThisWave <= 0 )
		{
			pPickupSpot->lNextSpawnTick = -1;
			continue;
		}

		// If this item has a delayed spawn set, just set the ticks until it spawns, and don't
		// spawn it.
		if ( pPickupSpot->args[2] > 2 )
		{
			pPickupSpot->lNextSpawnTick = pPickupSpot->args[2] * TICRATE;
			continue;
		}

		// If there's still an item resting in this spot, then there's no need to spawn something.
		bContinue = false;
		ActorIterator.Reinit( );
		while (( pActor = ActorIterator.Next( )))
		{
			if (( pActor->flags & MF_SPECIAL ) &&
				( pActor->InvasionSpot.pPickupSpot == pPickupSpot ))
			{
				bContinue = true;
				pPickupSpot->lNextSpawnTick = -1;
				break;
			}
		}

		if ( bContinue )
			continue;

		// Spawn the item.
		pActor = Spawn( pPickupSpot->GetSpawnName( ), pPickupSpot->x, pPickupSpot->y, pPickupSpot->z, ALLOW_REPLACE );

		// Since the actor successfully spawned, tell clients.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SpawnThing( pActor );

		// The item successfully spawned. Now, there's one less items that have to spawn.
		pPickupSpot->lNumLeftThisWave--;

		pPickupSpot->lNextSpawnTick = -1;

		// Set the proper angle, and spawn the teleport fog.
		pActor->angle = pPickupSpot->angle;
		if (( pPickupSpot->flags & MF_AMBUSH ) == false )
		{
			pFog = Spawn<ATeleportFog>( pPickupSpot->x, pPickupSpot->y, pPickupSpot->z + TELEFOGHEIGHT, ALLOW_REPLACE );

			// If we're the server, tell clients to spawn the fog.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SpawnThing( pFog );
		}
		pActor->InvasionSpot.pPickupSpot = pPickupSpot;
	}

	while (( pWeaponSpot = WeaponIterator.Next( )))
	{
		// Spot is dormant. Don't do anything with it this round.
		if ( pWeaponSpot->flags2 & MF2_DORMANT )
		{
			pWeaponSpot->lNextSpawnTick = -1;
			continue;
		}

		// Not time to spawn this item yet, or we already spawned it.
		if (( (ULONG)pWeaponSpot->args[3] != 0 ) && ( g_ulCurrentWave != (ULONG)pWeaponSpot->args[3] ))
		{
			pWeaponSpot->lNextSpawnTick = -1;
			continue;
		}

		// If this item has a delayed spawn set, just set the ticks until it spawns, and don't
		// spawn it.
		if ( pWeaponSpot->args[2] > 2 )
		{
			pWeaponSpot->lNextSpawnTick = pWeaponSpot->args[2] * TICRATE;
			continue;
		}

		// Spawn the item.
		pActor = Spawn( pWeaponSpot->GetSpawnName( ), pWeaponSpot->x, pWeaponSpot->y, pWeaponSpot->z, ALLOW_REPLACE );

		// Make this item stick around in multiplayer games.
		pActor->flags &= ~MF_DROPPED;

		// Since the actor successfully spawned, tell clients.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SpawnThing( pActor );

		pWeaponSpot->lNextSpawnTick = -1;

		// Set the proper angle, and spawn the teleport fog.
		pActor->angle = pWeaponSpot->angle;
		if (( pWeaponSpot->flags & MF_AMBUSH ) == false )
		{
			pFog = Spawn<ATeleportFog>( pWeaponSpot->x, pWeaponSpot->y, pWeaponSpot->z + TELEFOGHEIGHT, ALLOW_REPLACE );

			// If we're the server, tell clients to spawn the fog.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SpawnThing( pFog );
		}

		// Destroy the weapon spot since it doesn't need to persist.
//		pWeaponSpot->Destroy( );
	}

	// If we're the server, tell the client how many monsters are left.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SetInvasionNumMonstersLeft( );
}

//*****************************************************************************
//
void INVASION_DoWaveComplete( void )
{
	// Put the invasion state in the win sequence state.
	if ( NETWORK_GetState( ) != NETSTATE_CLIENT )
		INVASION_SetState( IS_WAVECOMPLETE );

	// Tell clients to do the win sequence.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_DoGameModeWinSequence( 0 );

	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
	{
		char				szString[32];
		DHUDMessageFadeOut	*pMsg;

		screen->SetFont( BigFont );

		if ( (LONG)g_ulCurrentWave == wavelimit )
			sprintf( szString, "VICTORY!", g_ulCurrentWave );
		else
			sprintf( szString, "WAVE %d COMPLETE!", g_ulCurrentWave );
		V_ColorizeString( szString );

		// Display "VICTORY!"/"WAVE %d COMPLETE!" HUD message.
		pMsg = new DHUDMessageFadeOut( szString,
			160.4f,
			75.0f,
			320,
			200,
			CR_RED,
			3.0f,
			2.0f );

		StatusBar->AttachMessage( pMsg, 'CNTR' );
		screen->SetFont( SmallFont );
	}

	if ( NETWORK_GetState( ) != NETSTATE_CLIENT )
		INVASION_SetCountdownTicks( 5 * TICRATE );
}

//*****************************************************************************
//
ULONG INVASION_GetCountdownTicks( void )
{
	return ( g_ulInvasionCountdownTicks );
}

//*****************************************************************************
//
void INVASION_SetCountdownTicks( ULONG ulTicks )
{
	g_ulInvasionCountdownTicks = ulTicks;
}

//*****************************************************************************
//
INVASIONSTATE_e INVASION_GetState( void )
{
	return ( g_InvasionState );
}

//*****************************************************************************
//
void INVASION_SetState( INVASIONSTATE_e State )
{
	AActor						*pActor;
	TThinkerIterator<AActor>	Iterator;

	g_InvasionState = State;

	// Tell clients about the state change.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SetGameModeState( State );

	switch ( State )
	{
	case IS_WAITINGFORPLAYERS:

		g_ulCurrentWave = 0;
		g_ulNumMonstersLeft = 0;
		g_ulNumArchVilesLeft = 0;
		g_ulNumBossMonsters = 0;
		g_ulInvasionCountdownTicks = 0;

		// Nothing else to do here if we're not in invasion mode.
		if ( 1 )//( invasion == false )
			break;

		while (( pActor = Iterator.Next( )) != NULL )
		{
			// Go through and delete all the current monsters to calm things down.
			if ( pActor->flags & MF_COUNTKILL )
			{
				pActor->Destroy( );
				continue;
			}
		}

		break;
	}
}

//*****************************************************************************
//
ULONG INVASION_GetNumMonstersLeft( void )
{
	return ( g_ulNumMonstersLeft );
}

//*****************************************************************************
//
void INVASION_SetNumMonstersLeft( ULONG ulLeft )
{
	g_ulNumMonstersLeft = ulLeft;
	if (( g_ulNumMonstersLeft == 0 ) && ( NETWORK_GetState( ) != NETSTATE_CLIENT ))
		INVASION_DoWaveComplete( );
}

//*****************************************************************************
//
ULONG INVASION_GetNumArchVilesLeft( void )
{
	return ( g_ulNumArchVilesLeft );
}

//*****************************************************************************
//
void INVASION_SetNumArchVilesLeft( ULONG ulLeft )
{
	g_ulNumArchVilesLeft = ulLeft;
}

//*****************************************************************************
//
ULONG INVASION_GetCurrentWave( void )
{
	return ( g_ulCurrentWave );
}

//*****************************************************************************
//
void INVASION_SetCurrentWave( ULONG ulWave )
{
	g_ulCurrentWave = ulWave;
}

//*****************************************************************************
//
bool INVASION_IncreaseNumMonstersOnSpawn( void )
{
	return ( g_bIncreaseNumMonstersOnSpawn );
}

//*****************************************************************************
//
void INVASION_WriteSaveInfo( FILE *pFile )
{
	ULONG				ulInvasionState;
	FPNGChunkArchive	arc( pFile, MAKE_ID( 'i','n','V','s' ));

	ulInvasionState = (ULONG)g_InvasionState;
	arc << (DWORD &)g_ulNumMonstersLeft << (DWORD &)g_ulInvasionCountdownTicks << (DWORD &)g_ulCurrentWave << (DWORD &)ulInvasionState << (DWORD &)g_ulNumBossMonsters;
}

//*****************************************************************************
//
void INVASION_ReadSaveInfo( PNGHandle *pPng )
{
	size_t	Length;

	Length = M_FindPNGChunk( pPng, MAKE_ID( 'i','n','V','s' ));
	if ( Length != 0 )
	{
		ULONG				ulInvasionState;
		FPNGChunkArchive	arc( pPng->File->GetFile( ), MAKE_ID( 'i','n','V','s' ), Length );

		arc << (DWORD &)g_ulNumMonstersLeft << (DWORD &)g_ulInvasionCountdownTicks << (DWORD &)g_ulCurrentWave << (DWORD &)ulInvasionState << (DWORD &)g_ulNumBossMonsters;
		g_InvasionState = (INVASIONSTATE_e)ulInvasionState;
	}
}

//*****************************************************************************
//
void INVASION_SetupSpawnerIDs( void )
{
	ABasePickupInvasionSpot							*pPickupSpot;
	TThinkerIterator<ABasePickupInvasionSpot>		PickupIterator;
	AActor											*pActor;
	TThinkerIterator<AActor>						ActorIterator;
	LONG											lID;

	lID = 0;
	while (( pPickupSpot = PickupIterator.Next( )))
	{
		if ( pPickupSpot->lSpawnerID == 0 )
			pPickupSpot->lSpawnerID = ++lID;
	}

	while (( pActor = ActorIterator.Next( )))
	{
		if ( pActor->flags & MF_SPECIAL )
		{
			if ( pActor->InvasionSpot.pPickupSpot )
				pActor->ulInvasionSpawnerID = pActor->InvasionSpot.pPickupSpot->lSpawnerID;
		}
	}
}

//*****************************************************************************
//
void INVASION_RestoreSpawnerPointers( void )
{
	AActor											*pActor;
	TThinkerIterator<AActor>						ActorIterator;

	while (( pActor = ActorIterator.Next( )))
	{
		if ( pActor->flags & MF_SPECIAL )
		{
			if ( pActor->ulInvasionSpawnerID != 0 )
				pActor->InvasionSpot.pPickupSpot = INVASION_FindPickupSpawnerByID( pActor->ulInvasionSpawnerID );
		}
	}
}

//*****************************************************************************
//
ABasePickupInvasionSpot *INVASION_FindPickupSpawnerByID( LONG lID )
{
	ABasePickupInvasionSpot							*pPickupSpot;
	TThinkerIterator<ABasePickupInvasionSpot>		PickupIterator;

	while (( pPickupSpot = PickupIterator.Next( )))
	{
		if ( pPickupSpot->lSpawnerID == lID )
			return ( pPickupSpot );
	}

	return ( NULL );
}

//*****************************************************************************
//*****************************************************************************
//
static ULONG invasion_GetNumThingsThisWave( ULONG ulNumOnFirstWave, ULONG ulWave, bool bMonster )
{
	UCVarValue	Val;

	Val = gameskill.GetGenericRep( CVAR_Int );
	switch ( Val.Int )
	{
	case 0:
	case 1:

		if ( bMonster )
			return ( (ULONG)(( pow( 1.25, (double)(( ulWave - 1 ))) * ulNumOnFirstWave )));
		else
			return ( (ULONG)(( pow( 2, (double)(( ulWave - 1 ))) * ulNumOnFirstWave )));
		break;
	case 2:
	default:

		if ( bMonster )
			return ( (ULONG)(( pow( 1.5, (double)(( ulWave - 1 ))) * ulNumOnFirstWave )));
		else
			return ( (ULONG)(( pow( 1.75, (double)(( ulWave - 1 ))) * ulNumOnFirstWave )));
		break;
	case 3:
	case 4:

		return ( (ULONG)(( pow( 1.6225, (double)(( ulWave - 1 ))) * ulNumOnFirstWave )));
		break;
	}
/*
	ULONG	ulIdx;
	ULONG	ulNumThingsThisWave;
	ULONG	ulNumTwoWavesAgo;
	ULONG	ulNumOneWaveAgo;
	ULONG	ulAddedAmount;

	ulNumThingsThisWave = 0;
	for ( ulIdx = 1; ulIdx <= ulWave; ulIdx++ )
	{
		switch ( ulIdx )
		{
		case 1:

			ulAddedAmount = ulNumOnFirstWave;
			ulNumThingsThisWave += ulAddedAmount;
			ulNumTwoWavesAgo = ulAddedAmount;
			break;
		case 2:

			ulAddedAmount = (ULONG)( ulNumOnFirstWave * 0.5f );
			ulNumThingsThisWave += (ULONG)( ulNumOnFirstWave * 0.5f );
			ulNumOneWaveAgo = ulAddedAmount;
			break;
		default:

			ulAddedAmount = ( ulNumOneWaveAgo + ulNumTwoWavesAgo );
			ulNumThingsThisWave += ulAddedAmount;
			ulNumTwoWavesAgo = ulNumOneWaveAgo;
			ulNumOneWaveAgo = ulAddedAmount;
			break;
		}
	}

	return ( ulNumThingsThisWave );
*/
}

//*****************************************************************************
//	CONSOLE COMMANDS/VARIABLES

CVAR( Int, sv_invasioncountdowntime, 10, CVAR_ARCHIVE );
CUSTOM_CVAR( Int, wavelimit, 0, CVAR_CAMPAIGNLOCK )
{
	if (( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( gamestate != GS_STARTUP ))
	{
		SERVER_Printf( PRINT_HIGH, "%s changed to: %d\n", self.GetName( ), (LONG)self );
		SERVERCOMMANDS_SetGameModeLimits( );

		// Update the scoreboard.
		SERVERCONSOLE_UpdateScoreboard( );
	}
}
CVAR( Bool, sv_usemapsettingswavelimit, true, CVAR_ARCHIVE );
