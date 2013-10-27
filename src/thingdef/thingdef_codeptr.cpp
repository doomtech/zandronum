/*
** thingdef.cpp
**
** Code pointers for Actor definitions
**
**---------------------------------------------------------------------------
** Copyright 2002-2006 Christoph Oelckers
** Copyright 2004-2006 Randy Heit
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
** 4. When not used as part of ZDoom or a ZDoom derivative, this code will be
**    covered by the terms of the GNU General Public License as published by
**    the Free Software Foundation; either version 2 of the License, or (at
**    your option) any later version.
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

#include "gi.h"
#include "g_level.h"
#include "actor.h"
#include "info.h"
#include "sc_man.h"
#include "tarray.h"
#include "w_wad.h"
#include "templates.h"
#include "r_defs.h"
#include "r_draw.h"
#include "a_pickups.h"
#include "s_sound.h"
#include "cmdlib.h"
#include "p_lnspec.h"
#include "p_enemy.h"
#include "a_action.h"
#include "decallib.h"
#include "m_random.h"
#include "i_system.h"
#include "p_local.h"
#include "c_console.h"
#include "doomerrors.h"
#include "a_sharedglobal.h"
#include "thingdef/thingdef.h"
#include "v_video.h"
#include "v_font.h"
#include "doomstat.h"
#include "v_palette.h"
// [BB] new #includes.
#include "deathmatch.h"
#include "cl_main.h"
#include "cl_demo.h"
#include "invasion.h"
#include "sv_commands.h"
#include "p_acs.h"
#include "unlagged.h"

static FRandom pr_camissile ("CustomActorfire");
static FRandom pr_camelee ("CustomMelee");
static FRandom pr_cabullet ("CustomBullet");
static FRandom pr_cajump ("CustomJump");
static FRandom pr_cwbullet ("CustomWpBullet");
static FRandom pr_cwjump ("CustomWpJump");
static FRandom pr_cwpunch ("CustomWpPunch");
static FRandom pr_grenade ("ThrowGrenade");
static FRandom pr_crailgun ("CustomRailgun");
static FRandom pr_spawndebris ("SpawnDebris");
static FRandom pr_spawnitemex ("SpawnItemEx");
static FRandom pr_burst ("Burst");
static FRandom pr_monsterrefire ("MonsterRefire");


// [BC] Blah.
#define	CLIENTUPDATE_FRAME			1
#define	CLIENTUPDATE_POSITION		2
#define	CLIENTUPDATE_SKIPPLAYER		4

//==========================================================================
//
// [BB] Used in all code pointers that spawn actors to handle
// the NETFL_CLIENTSIDEONLY property.
//
//==========================================================================
bool shouldActorNotBeSpawned ( const AActor *pSpawner, const PClass *pSpawnType, const bool bForceClientSide = false )
{
	// [BB] Nothing to spawn.
	if ( pSpawnType == NULL )
		return true;

	bool bSpawnOnClient = ( bForceClientSide
	                        || ( pSpawner && ( pSpawner->ulNetworkFlags & NETFL_CLIENTSIDEONLY ) )
	                        || ( GetDefaultByType(pSpawnType)->ulNetworkFlags & NETFL_CLIENTSIDEONLY )
	                      );

	// [BB] Clients don't spawn non-client side only things.
	if ( ( ( NETWORK_GetState( ) == NETSTATE_CLIENT ) || ( CLIENTDEMO_IsPlaying( ) ) ) && !bSpawnOnClient )
		return true;
	// [BB] The server doesn't spawn client side only things.
	else if ( ( NETWORK_GetState( ) == NETSTATE_SERVER ) && bSpawnOnClient )
		return true;
	else
		return false;
}

//==========================================================================
//
// ACustomInventory :: CallStateChain
//
// Executes the code pointers in a chain of states
// until there is no next state
//
//==========================================================================

bool ACustomInventory::CallStateChain (AActor *actor, FState * State)
{
	StateCallData StateCall;
	bool result = false;
	int counter = 0;

	StateCall.Item = this;
	while (State != NULL)
	{
		// Assume success. The code pointer will set this to false if necessary
		StateCall.State = State;
		StateCall.Result = true;
		if (State->CallAction(actor, this, &StateCall))
		{
			// collect all the results. Even one successful call signifies overall success.
			result |= StateCall.Result;
		}


		// Since there are no delays it is a good idea to check for infinite loops here!
		counter++;
		if (counter >= 10000)	break;

		if (StateCall.State == State) 
		{
			// Abort immediately if the state jumps to itself!
			if (State == State->GetNextState()) 
			{
				return false;
			}
			
			// If both variables are still the same there was no jump
			// so we must advance to the next state.
			State = State->GetNextState();
		}
		else 
		{
			State = StateCall.State;
		}
	}
	return result;
}

//==========================================================================
//
// Simple flag changers
//
//==========================================================================
DEFINE_ACTION_FUNCTION(AActor, A_SetSolid)
{
	self->flags |= MF_SOLID;
}

DEFINE_ACTION_FUNCTION(AActor, A_UnsetSolid)
{
	self->flags &= ~MF_SOLID;
}

DEFINE_ACTION_FUNCTION(AActor, A_SetFloat)
{
	self->flags |= MF_FLOAT;
}

DEFINE_ACTION_FUNCTION(AActor, A_UnsetFloat)
{
	self->flags &= ~(MF_FLOAT|MF_INFLOAT);
}

//==========================================================================
//
// Customizable attack functions which use actor parameters.
//
//==========================================================================
static void DoAttack (AActor *self, bool domelee, bool domissile,
					  int MeleeDamage, FSoundID MeleeSound, const PClass *MissileType,fixed_t MissileHeight)
{
	// [BC] Let the server play these sounds.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		if (( self->ulNetworkFlags & NETFL_CLIENTSIDEONLY ) == false )
			return;
	}

	if (self->target == NULL) return;

	A_FaceTarget (self);
	if (domelee && MeleeDamage>0 && self->CheckMeleeRange ())
	{
		int damage = pr_camelee.HitDice(MeleeDamage);
		if (MeleeSound)
		{
			S_Sound (self, CHAN_WEAPON, MeleeSound, 1, ATTN_NORM);

			// [BC] If we're the server, make the sound on the client end.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SoundActor( self, CHAN_WEAPON, S_GetName( MeleeSound ), 1, ATTN_NORM );
		}

		P_DamageMobj (self->target, self, self, damage, NAME_Melee);
		P_TraceBleed (damage, self->target, self);
	}
	else if (domissile && MissileType != NULL)
	{
		// This seemingly senseless code is needed for proper aiming.
		self->z+=MissileHeight-32*FRACUNIT;
		AActor * missile = P_SpawnMissileXYZ (self->x, self->y, self->z + 32*FRACUNIT, self, self->target, MissileType, false);
		self->z-=MissileHeight-32*FRACUNIT;

		if (missile)
		{
			// automatic handling of seeker missiles
			if (missile->flags2&MF2_SEEKERMISSILE)
			{
				missile->tracer=self->target;
			}
			// set the health value so that the missile works properly
			if (missile->flags4&MF4_SPECTRAL)
			{
				missile->health=-2;
			}
			bool bSucces = P_CheckMissileSpawn(missile);

			// [BC] If we're the server, tell clients to spawn the missile.
			if ( bSucces && ( NETWORK_GetState( ) == NETSTATE_SERVER ) )
				SERVERCOMMANDS_SpawnMissile( missile );
		}
	}
}

DEFINE_ACTION_FUNCTION(AActor, A_MeleeAttack)
{
	int MeleeDamage = self->GetClass()->Meta.GetMetaInt (ACMETA_MeleeDamage, 0);
	FSoundID MeleeSound =  self->GetClass()->Meta.GetMetaInt (ACMETA_MeleeSound, 0);
	DoAttack(self, true, false, MeleeDamage, MeleeSound, NULL, 0);
}

DEFINE_ACTION_FUNCTION(AActor, A_MissileAttack)
{
	const PClass *MissileType=PClass::FindClass((ENamedName) self->GetClass()->Meta.GetMetaInt (ACMETA_MissileName, NAME_None));
	fixed_t MissileHeight= self->GetClass()->Meta.GetMetaFixed (ACMETA_MissileHeight, 32*FRACUNIT);
	DoAttack(self, false, true, 0, 0, MissileType, MissileHeight);
}

DEFINE_ACTION_FUNCTION(AActor, A_ComboAttack)
{
	int MeleeDamage = self->GetClass()->Meta.GetMetaInt (ACMETA_MeleeDamage, 0);
	FSoundID MeleeSound =  self->GetClass()->Meta.GetMetaInt (ACMETA_MeleeSound, 0);
	const PClass *MissileType=PClass::FindClass((ENamedName) self->GetClass()->Meta.GetMetaInt (ACMETA_MissileName, NAME_None));
	fixed_t MissileHeight= self->GetClass()->Meta.GetMetaFixed (ACMETA_MissileHeight, 32*FRACUNIT);
	DoAttack(self, true, true, MeleeDamage, MeleeSound, MissileType, MissileHeight);
}

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_BasicAttack)
{
	ACTION_PARAM_START(4);
	ACTION_PARAM_INT(MeleeDamage, 0);
	ACTION_PARAM_SOUND(MeleeSound, 1);
	ACTION_PARAM_CLASS(MissileType, 2);
	ACTION_PARAM_FIXED(MissileHeight, 3);

	if (MissileType == NULL) return;
	DoAttack(self, true, true, MeleeDamage, MeleeSound, MissileType, MissileHeight);
}

//==========================================================================
//
// Custom sound functions. 
//
//==========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_PlaySound)
{
	ACTION_PARAM_START(5);
	ACTION_PARAM_SOUND(soundid, 0);
	ACTION_PARAM_INT(channel, 1);
	ACTION_PARAM_FLOAT(volume, 2);
	ACTION_PARAM_BOOL(looping, 3);
	ACTION_PARAM_FLOAT(attenuation, 4);

	// [BC] Let the server play these sounds.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		if (( self->ulNetworkFlags & NETFL_CLIENTSIDEONLY ) == false )
			return;
	}

	if (!looping)
	{
		S_Sound (self, channel, soundid, volume, attenuation);

		// [BC] If we're the server, tell clients to play the sound.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SoundActor( self, channel, S_GetName( soundid ), volume, attenuation );
	}
	else
	{
		if (!S_IsActorPlayingSomething (self, channel&7, soundid))
		{
			S_Sound (self, channel | CHAN_LOOP, soundid, volume, attenuation);

			// [BC] If we're the server, tell clients to play the sound.
			// [Dusk] We need to respect existing sound play since this is a looped sound.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SoundActor( self, channel | CHAN_LOOP, S_GetName( soundid ), volume, attenuation, MAXPLAYERS, 0, true );
		}
	}
}

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_StopSound)
{
	ACTION_PARAM_START(1);
	ACTION_PARAM_INT(slot, 0);

	S_StopSound(self, slot);
}

//==========================================================================
//
// These come from a time when DECORATE constants did not exist yet and
// the sound interface was less flexible. As a result the parameters are
// not optimal and these functions have been deprecated in favor of extending
// A_PlaySound and A_StopSound.
//
//==========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_PlayWeaponSound)
{
	ACTION_PARAM_START(1);
	ACTION_PARAM_SOUND(soundid, 0);

	S_Sound (self, CHAN_WEAPON, soundid, 1, ATTN_NORM);
}

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_PlaySoundEx)
{
	ACTION_PARAM_START(4);
	ACTION_PARAM_SOUND(soundid, 0);
	ACTION_PARAM_NAME(channel, 1);
	ACTION_PARAM_BOOL(looping, 2);
	ACTION_PARAM_INT(attenuation_raw, 3);

	// [BB] Let the server play these sounds.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		if (( self->ulNetworkFlags & NETFL_CLIENTSIDEONLY ) == false )
			return;
	}

	float attenuation;
	switch (attenuation_raw)
	{
		case -1: attenuation = ATTN_STATIC;	break; // drop off rapidly
		default:
		case  0: attenuation = ATTN_NORM;	break; // normal
		case  1:
		case  2: attenuation = ATTN_NONE;	break; // full volume
	}

	if (channel < NAME_Auto || channel > NAME_SoundSlot7)
	{
		channel = NAME_Auto;
	}

	if (!looping)
	{
		S_Sound (self, int(channel) - NAME_Auto, soundid, 1, attenuation);

		// [BB] If we're the server, tell clients to play the sound.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SoundActor( self, int(channel) - NAME_Auto, S_GetName( soundid ), 1, attenuation );
	}
	else
	{
		if (!S_IsActorPlayingSomething (self, int(channel) - NAME_Auto, soundid))
		{
			S_Sound (self, (int(channel) - NAME_Auto) | CHAN_LOOP, soundid, 1, attenuation);

			// [BB] If we're the server, tell clients to play the sound, but only if they are not already playing something for this actor.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SoundActor( self, (int(channel) - NAME_Auto) | CHAN_LOOP, S_GetName( soundid ), 1, attenuation, MAXPLAYERS, 0, true );
		}
	}
}

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_StopSoundEx)
{
	ACTION_PARAM_START(1);
	ACTION_PARAM_NAME(channel, 0);

	if (channel > NAME_Auto && channel <= NAME_SoundSlot7)
	{
		S_StopSound (self, int(channel) - NAME_Auto);
	}
}

//==========================================================================
//
// Generic seeker missile function
//
//==========================================================================
static FRandom pr_seekermissile ("SeekerMissile");
enum
{
	SMF_LOOK = 1,
	SMF_PRECISE = 2,
};
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_SeekerMissile)
{
	ACTION_PARAM_START(5);
	ACTION_PARAM_INT(ang1, 0);
	ACTION_PARAM_INT(ang2, 1);
	ACTION_PARAM_INT(flags, 2);
	ACTION_PARAM_INT(chance, 3);
	ACTION_PARAM_INT(distance, 4);

	if ((flags & SMF_LOOK) && (self->tracer == 0) && (pr_seekermissile()<chance))
	{
		self->tracer = P_RoughMonsterSearch (self, distance);
	}
	P_SeekerMissile(self, clamp<int>(ang1, 0, 90) * ANGLE_1, clamp<int>(ang2, 0, 90) * ANGLE_1, !!(flags & SMF_PRECISE));
}

//==========================================================================
//
// Hitscan attack with a customizable amount of bullets (specified in damage)
//
//==========================================================================
DEFINE_ACTION_FUNCTION(AActor, A_BulletAttack)
{
	int i;
	int bangle;
	int slope;
		
	if (!self->target) return;

	A_FaceTarget (self);
	bangle = self->angle;

	slope = P_AimLineAttack (self, bangle, MISSILERANGE);

	S_Sound (self, CHAN_WEAPON, self->AttackSound, 1, ATTN_NORM);

	// [BC] If we're the server, tell clients to play the sound.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SoundActor( self, CHAN_WEAPON, S_GetName( self->AttackSound ), 1, ATTN_NORM );

	for (i = self->GetMissileDamage (0, 1); i > 0; --i)
    {
		int angle = bangle + (pr_cabullet.Random2() << 20);
		int damage = ((pr_cabullet()%5)+1)*3;
		P_LineAttack(self, angle, MISSILERANGE, slope, damage,
			NAME_None, NAME_BulletPuff);
    }
}


//==========================================================================
//
// Do the state jump
//
//==========================================================================
// [BC] Added ulClientUpdateFlags.
static void DoJump(AActor * self, FState * CallingState, FState *jumpto, StateCallData *statecall, ULONG ulClientUpdateFlags)
{
	if (jumpto == NULL) return;

	if (statecall != NULL)
	{
		statecall->State = jumpto;
	}
	else if (self->player != NULL && CallingState == self->player->psprites[ps_weapon].state)
	{
		// [BB] If we're the server, tell clients to change the thing's state.
		if (( ulClientUpdateFlags & CLIENTUPDATE_FRAME ) &&
			( NETWORK_GetState( ) == NETSTATE_SERVER ))
		{
			SERVER_HandleWeaponStateJump ( static_cast<ULONG>( self->player - players ), jumpto, ps_weapon );
		}

		P_SetPsprite(self->player, ps_weapon, jumpto);
	}
	else if (self->player != NULL && CallingState == self->player->psprites[ps_flash].state)
	{
		// [BB] If we're the server, tell clients to change the thing's state.
		if (( ulClientUpdateFlags & CLIENTUPDATE_FRAME ) &&
			( NETWORK_GetState( ) == NETSTATE_SERVER ))
		{
			SERVER_HandleWeaponStateJump ( static_cast<ULONG>( self->player - players ), jumpto, ps_flash );
		}

		P_SetPsprite(self->player, ps_flash, jumpto);
	}
	else if (CallingState == self->state)
	{
		// [BC] If we're the server, tell clients to change the thing's state.
		if (( ulClientUpdateFlags & CLIENTUPDATE_FRAME ) &&
			( NETWORK_GetState( ) == NETSTATE_SERVER ))
		{
			// [BB] For some reason calling SERVERCOMMANDS_SetThingFrame normally here causes clients
			// to crash when exiting from tnt03a1 to tnt03a2. The crash seems to be caused by calling
			// SetState on the clients. Just calling SetStateNF instead seems to fix the problem. This
			// is done by the "false" argument.
			if (( ulClientUpdateFlags & CLIENTUPDATE_SKIPPLAYER ) &&
				( self->player ))
			{
				SERVERCOMMANDS_SetThingFrame( self, jumpto, ULONG( self->player - players ), SVCF_SKIPTHISCLIENT, false );
			}
			else
				SERVERCOMMANDS_SetThingFrame( self, jumpto, MAXPLAYERS, 0, false );

			if ( ulClientUpdateFlags & CLIENTUPDATE_POSITION )
				SERVERCOMMANDS_MoveThing( self, CM_X|CM_Y|CM_Z );
		}

		self->SetState (jumpto);
	}
	else
	{
		// something went very wrong. This should never happen.
		assert(false);
	}
}

// This is just to avoid having to directly reference the internally defined
// CallingState and statecall parameters in the code below.
// [BB] Added ulClientUpdateFlags.
#define ACTION_JUMP(offset,ulClientUpdateFlags) DoJump(self, CallingState, offset, statecall, ulClientUpdateFlags)

//==========================================================================
//
// State jump function
//
//==========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_Jump)
{
	ACTION_PARAM_START(3);
	ACTION_PARAM_INT(count, 0);
	ACTION_PARAM_INT(maxchance, 1);

	// [BC] Don't jump here in client mode.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		if (( self->ulNetworkFlags & NETFL_CLIENTSIDEONLY ) == false )
			return;
	}

	if (count >= 2 && (maxchance >= 256 || pr_cajump() < maxchance))
	{
		int jumps = 2 + (count == 2? 0 : (pr_cajump() % (count - 1)));
		ACTION_PARAM_STATE(jumpto, jumps);
		ACTION_JUMP(jumpto, true);	// [BC] Random state changes shouldn't be client-side.
	}
	ACTION_SET_RESULT(false);	// Jumps should never set the result for inventory state chains!
}

//==========================================================================
//
// State jump function
//
//==========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_JumpIfHealthLower)
{
	ACTION_PARAM_START(2);
	ACTION_PARAM_INT(health, 0);
	ACTION_PARAM_STATE(jump, 1);

	// [BC] Don't jump here in client mode.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		if (( self->ulNetworkFlags & NETFL_CLIENTSIDEONLY ) == false )
			return;
	}

	if (self->health < health) ACTION_JUMP(jump, true);	// [BC] Clients don't know what the actor's health is.

	ACTION_SET_RESULT(false);	// Jumps should never set the result for inventory state chains!
}

//==========================================================================
//
// State jump function
//
//==========================================================================
void DoJumpIfCloser(AActor *target, DECLARE_PARAMINFO)
{
	ACTION_PARAM_START(2);
	ACTION_PARAM_FIXED(dist, 0);
	ACTION_PARAM_STATE(jump, 1);

	ACTION_SET_RESULT(false);	// Jumps should never set the result for inventory state chains!

	// No target - no jump
	if (target != NULL && P_AproxDistance(self->x-target->x, self->y-target->y) < dist &&
		( (self->z > target->z && self->z - (target->z + target->height) < dist) || 
		  (self->z <=target->z && target->z - (self->z + self->height) < dist) 
		)
	   )
	{
		ACTION_JUMP(jump,CLIENTUPDATE_FRAME|CLIENTUPDATE_POSITION);	// [BC] Since monsters don't have targets on the client end, we need to send an update.
	}
}

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_JumpIfCloser)
{
	AActor *target;

	// [BC] Don't jump here in client mode.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		if (( self->ulNetworkFlags & NETFL_CLIENTSIDEONLY ) == false )
			return;
	}

	if (!self->player)
	{
		target = self->target;
	}
	else
	{
		// Does the player aim at something that can be shot?
		P_BulletSlope(self, &target);
	}
	DoJumpIfCloser(target, PUSH_PARAMINFO);
}

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_JumpIfTracerCloser)
{
	// Is there really any reason to limit this to seeker missiles?
	if (self->flags2 & MF2_SEEKERMISSILE)
	{
		DoJumpIfCloser(self->tracer, PUSH_PARAMINFO);
	}
}

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_JumpIfMasterCloser)
{
	DoJumpIfCloser(self->master, PUSH_PARAMINFO);
}

//==========================================================================
//
// State jump function
//
//==========================================================================
void DoJumpIfInventory(AActor * owner, DECLARE_PARAMINFO)
{
	ACTION_PARAM_START(3);
	ACTION_PARAM_CLASS(Type, 0);
	ACTION_PARAM_INT(ItemAmount, 1);
	ACTION_PARAM_STATE(JumpOffset, 2);
	ULONG	ulClientUpdateFlags;

	// [BC] Don't jump here in client mode.
	ulClientUpdateFlags = 0;
	if (( self->player ) &&
		(( CallingState == self->player->psprites[ps_weapon].state ) || ( CallingState == self->player->psprites[ps_flash].state )))
	{
	}
	else
	{
		if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
			( CLIENTDEMO_IsPlaying( )))
		{
			if ((( self->ulNetworkFlags & NETFL_CLIENTSIDEONLY ) == false ) &&
				(( self->player == NULL ) || (( self->player - players ) != consoleplayer )))
			{
				return;
			}
		}

		ulClientUpdateFlags |= CLIENTUPDATE_FRAME;

		// The player should know his own inventory.
		if ( self->player )
			ulClientUpdateFlags |= CLIENTUPDATE_SKIPPLAYER;
		else
			ulClientUpdateFlags |= CLIENTUPDATE_POSITION;
	}

	ACTION_SET_RESULT(false);	// Jumps should never set the result for inventory state chains!

	if (!Type || owner == NULL) return;

	AInventory *Item = owner->FindInventory(Type);

	if (Item)
	{
		if (ItemAmount > 0)
		{
			if (Item->Amount >= ItemAmount)
				ACTION_JUMP(JumpOffset, ulClientUpdateFlags);	// [BC] Clients don't necessarily have inventory information.
		}
		else if (Item->Amount >= Item->MaxAmount)
		{
			ACTION_JUMP(JumpOffset, ulClientUpdateFlags);	// [BC] Clients don't necessarily have inventory information.
		}
	}
}

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_JumpIfInventory)
{
	DoJumpIfInventory(self, PUSH_PARAMINFO);
}

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_JumpIfInTargetInventory)
{
	DoJumpIfInventory(self->target, PUSH_PARAMINFO);
}

//==========================================================================
//
// State jump function
//
//==========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_JumpIfArmorType)
{
	ACTION_PARAM_START(3);
	ACTION_PARAM_NAME(Type, 0);
	ACTION_PARAM_STATE(JumpOffset, 1);
	ACTION_PARAM_INT(amount, 2);

	ACTION_SET_RESULT(false);	// Jumps should never set the result for inventory state chains!

	ABasicArmor * armor = (ABasicArmor *) self->FindInventory(NAME_BasicArmor);

	if (armor && armor->ArmorType == Type && armor->Amount >= amount)
		ACTION_JUMP(JumpOffset, false);	// [BB] Clients know the player's inventory, so this is hopefully okay.
}

//==========================================================================
//
// Parameterized version of A_Explode
//
//==========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_Explode)
{
	// [BB] This is server side.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return;
	}

	ACTION_PARAM_START(7);
	ACTION_PARAM_INT(damage, 0);
	ACTION_PARAM_INT(distance, 1);
	ACTION_PARAM_BOOL(hurtSource, 2);
	ACTION_PARAM_BOOL(alert, 3);
	ACTION_PARAM_INT(fulldmgdistance, 4);
	ACTION_PARAM_INT(nails, 5);
	ACTION_PARAM_INT(naildamage, 6);

	if (damage < 0)	// get parameters from metadata
	{
		damage = self->GetClass()->Meta.GetMetaInt (ACMETA_ExplosionDamage, 128);
		distance = self->GetClass()->Meta.GetMetaInt (ACMETA_ExplosionRadius, damage);
		hurtSource = !self->GetClass()->Meta.GetMetaInt (ACMETA_DontHurtShooter);
		alert = false;
	}
	else
	{
		if (distance <= 0) distance = damage;
	}
	// NailBomb effect, from SMMU but not from its source code: instead it was implemented and
	// generalized from the documentation at http://www.doomworld.com/eternity/engine/codeptrs.html

	if (nails)
	{
		angle_t ang;
		for (int i = 0; i < nails; i++)
		{
			ang = i*(ANGLE_MAX/nails);
			// Comparing the results of a test wad with Eternity, it seems A_NailBomb does not aim
			P_LineAttack (self, ang, MISSILERANGE, 0,
				//P_AimLineAttack (self, ang, MISSILERANGE), 
				naildamage, NAME_None, NAME_BulletPuff);
		}
	}

	P_RadiusAttack (self, self->target, damage, distance, self->DamageType, hurtSource, true, fulldmgdistance);
	P_CheckSplash(self, distance<<FRACBITS);
	if (alert && self->target != NULL && self->target->player != NULL)
	{
		validcount++;
		P_RecursiveSound (self->Sector, self->target, false, 0);
	}
}

//==========================================================================
//
// A_RadiusThrust
//
//==========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_RadiusThrust)
{
	// [BB] This is server side.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return;
	}

	ACTION_PARAM_START(3);
	ACTION_PARAM_INT(force, 0);
	ACTION_PARAM_FIXED(distance, 1);
	ACTION_PARAM_BOOL(affectSource, 2);

	if (force <= 0) force = 128;
	if (distance <= 0) distance = force;

	P_RadiusAttack (self, self->target, force, distance, self->DamageType, affectSource, false);
	P_CheckSplash(self, distance<<FRACBITS);
}

//==========================================================================
//
// Execute a line special / script
//
//==========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_CallSpecial)
{
	ACTION_PARAM_START(6);
	ACTION_PARAM_INT(special, 0);
	ACTION_PARAM_INT(arg1, 1);
	ACTION_PARAM_INT(arg2, 2);
	ACTION_PARAM_INT(arg3, 3);
	ACTION_PARAM_INT(arg4, 4);
	ACTION_PARAM_INT(arg5, 5);

	// [BC] Don't do this in client mode.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		if (( self->ulNetworkFlags & NETFL_CLIENTSIDEONLY ) == false )
		{
			ACTION_SET_RESULT( false );
			return;
		}
	}

	bool res = !!LineSpecials[special](NULL, self, false, arg1, arg2, arg3, arg4, arg5);

	ACTION_SET_RESULT(res);
}

//==========================================================================
//
// Checks whether this actor is a missile
// Unfortunately this was buggy in older versions of the code and many
// released DECORATE monsters rely on this bug so it can only be fixed
// with an optional flag
//
//==========================================================================
inline static bool isMissile(AActor * self, bool precise=true)
{
	return self->flags&MF_MISSILE || (precise && self->GetDefault()->flags&MF_MISSILE);
}

//==========================================================================
//
// The ultimate code pointer: Fully customizable missiles!
//
//==========================================================================
enum CM_Flags
{
	CMF_AIMMODE = 3,
	CMF_TRACKOWNER = 4,
	CMF_CHECKTARGETDEAD = 8,
};

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_CustomMissile)
{
	ACTION_PARAM_START(6);
	ACTION_PARAM_CLASS(ti, 0);
	ACTION_PARAM_FIXED(SpawnHeight, 1);
	ACTION_PARAM_INT(Spawnofs_XY, 2);
	ACTION_PARAM_ANGLE(Angle, 3);
	ACTION_PARAM_INT(flags, 4);
	ACTION_PARAM_ANGLE(pitch, 5);

	int aimmode = flags & CMF_AIMMODE;

	AActor * targ;
	AActor * missile;

	// [BB] Should the actor not be spawned, taking in account client side only actors?
	if ( shouldActorNotBeSpawned ( self, ti ) )
		return;

	if (self->target != NULL || aimmode==2)
	{
		if (ti) 
		{
			angle_t ang = (self->angle - ANGLE_90) >> ANGLETOFINESHIFT;
			fixed_t x = Spawnofs_XY * finecosine[ang];
			fixed_t y = Spawnofs_XY * finesine[ang];
			fixed_t z = SpawnHeight - 32*FRACUNIT + (self->player? self->player->crouchoffset : 0);

			switch (aimmode)
			{
			case 0:
			default:
				// same adjustment as above (in all 3 directions this time) - for better aiming!
				self->x+=x;
				self->y+=y;
				self->z+=z;
				missile = P_SpawnMissileXYZ(self->x, self->y, self->z + 32*FRACUNIT, self, self->target, ti, false);
				self->x-=x;
				self->y-=y;
				self->z-=z;
				break;

			case 1:
				missile = P_SpawnMissileXYZ(self->x+x, self->y+y, self->z+SpawnHeight, self, self->target, ti, false);
				break;

			case 2:
				self->x+=x;
				self->y+=y;
				missile = P_SpawnMissileAngleZSpeed(self, self->z+SpawnHeight, ti, self->angle, 0, GetDefaultByType(ti)->Speed, self, false);
				self->x-=x;
				self->y-=y;

				// It is not necessary to use the correct angle here.
				// The only important thing is that the horizontal velocity is correct.
				// Therefore use 0 as the missile's angle and simplify the calculations accordingly.
				// The actual velocity vector is set below.
				if (missile)
				{
					fixed_t vx = finecosine[pitch>>ANGLETOFINESHIFT];
					fixed_t vz = finesine[pitch>>ANGLETOFINESHIFT];

					missile->velx = FixedMul (vx, missile->Speed);
					missile->vely = 0;
					missile->velz = FixedMul (vz, missile->Speed);
				}

				break;
			}

			if (missile)
			{
				// Use the actual velocity instead of the missile's Speed property
				// so that this can handle missiles with a high vertical velocity 
				// component properly.
				FVector3 velocity (missile->velx, missile->vely, 0);

				fixed_t missilespeed = (fixed_t)velocity.Length();

				missile->angle += Angle;
				ang = missile->angle >> ANGLETOFINESHIFT;
				missile->velx = FixedMul (missilespeed, finecosine[ang]);
				missile->vely = FixedMul (missilespeed, finesine[ang]);
	
				// handle projectile shooting projectiles - track the
				// links back to a real owner
                if (isMissile(self, !!(flags & CMF_TRACKOWNER)))
                {
                	AActor * owner=self ;//->target;
                	while (isMissile(owner, !!(flags & CMF_TRACKOWNER)) && owner->target) owner=owner->target;
                	targ=owner;
                	missile->target=owner;
					// automatic handling of seeker missiles
					if (self->flags & missile->flags2 & MF2_SEEKERMISSILE)
					{
						missile->tracer=self->tracer;
					}
                }
				else if (missile->flags2&MF2_SEEKERMISSILE)
				{
					// automatic handling of seeker missiles
					missile->tracer=self->target;
				}
				// set the health value so that the missile works properly
				if (missile->flags4&MF4_SPECTRAL)
				{
					missile->health=-2;
				}

				// [BB] The client did the spawning, so this has to be a client side only actor.
				// Needs to be done regardless of whether the spawn was successful.
				if ( ( NETWORK_GetState( ) == NETSTATE_CLIENT ) || ( CLIENTDEMO_IsPlaying( ) ) )
					missile->ulNetworkFlags |= NETFL_CLIENTSIDEONLY;

				// [BB] Save whether the spawn was successfull.
				bool bSucces = P_CheckMissileSpawn(missile);

				if ( bSucces )
				{
					// [BC] If we're the server, tell clients to spawn the missile.
					if ( NETWORK_GetState( ) == NETSTATE_SERVER )
						SERVERCOMMANDS_SpawnMissile( missile );
				}
			}
		}
	}
	else if (flags & CMF_CHECKTARGETDEAD)
	{
		// Target is dead and the attack shall be aborted.
		if (self->SeeState != NULL) self->SetState(self->SeeState);
	}
}

//==========================================================================
//
// An even more customizable hitscan attack
//
//==========================================================================
enum CBA_Flags
{
	CBAF_AIMFACING = 1,
	CBAF_NORANDOM = 2,
};

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_CustomBulletAttack)
{
	ACTION_PARAM_START(7);
	ACTION_PARAM_ANGLE(Spread_XY, 0);
	ACTION_PARAM_ANGLE(Spread_Z, 1);
	ACTION_PARAM_INT(NumBullets, 2);
	ACTION_PARAM_INT(DamagePerBullet, 3);
	ACTION_PARAM_CLASS(pufftype, 4);
	ACTION_PARAM_FIXED(Range, 5);
	ACTION_PARAM_INT(Flags, 6);

	if(Range==0) Range=MISSILERANGE;

	int i;
	int bangle;
	int bslope;

	if (self->target || (Flags & CBAF_AIMFACING))
	{
		if (!(Flags & CBAF_AIMFACING)) A_FaceTarget (self);
		bangle = self->angle;

		if (!pufftype) pufftype = PClass::FindClass(NAME_BulletPuff);

		bslope = P_AimLineAttack (self, bangle, MISSILERANGE);

		S_Sound (self, CHAN_WEAPON, self->AttackSound, 1, ATTN_NORM);

		// [BB] If we're the server, make the sound on the client end.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SoundActor( self, CHAN_WEAPON, S_GetName( self->AttackSound ), 1, ATTN_NORM );

		for (i=0 ; i<NumBullets ; i++)
		{
			int angle = bangle + pr_cabullet.Random2() * (Spread_XY / 255);
			int slope = bslope + pr_cabullet.Random2() * (Spread_Z / 255);
			int damage = DamagePerBullet;

			if (!(Flags & CBAF_NORANDOM))
				damage *= ((pr_cabullet()%3)+1);

			P_LineAttack(self, angle, Range, slope, damage, NAME_None, pufftype);
		}
    }
}

//==========================================================================
//
// A fully customizable melee attack
//
//==========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_CustomMeleeAttack)
{
	ACTION_PARAM_START(5);
	ACTION_PARAM_INT(damage, 0);
	ACTION_PARAM_SOUND(MeleeSound, 1);
	ACTION_PARAM_SOUND(MissSound, 2);
	ACTION_PARAM_NAME(DamageType, 3);
	ACTION_PARAM_BOOL(bleed, 4);

	// [BB] This is handled by the server.
	if ( NETWORK_InClientMode( ) && ( ( self->ulNetworkFlags & NETFL_CLIENTSIDEONLY ) == false ) )
		return;

	if (DamageType==NAME_None) DamageType = NAME_Melee;	// Melee is the default type

	if (!self->target)
		return;
				
	A_FaceTarget (self);
	if (self->CheckMeleeRange ())
	{
		// [BB] Added brackets to add server code.
		if (MeleeSound)
		{
			S_Sound (self, CHAN_WEAPON, MeleeSound, 1, ATTN_NORM);

			// [BB] If we're the server, make the sound on the client end.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SoundActor( self, CHAN_WEAPON, S_GetName( MeleeSound ), 1, ATTN_NORM );
		}
		P_DamageMobj (self->target, self, self, damage, DamageType);
		if (bleed) P_TraceBleed (damage, self->target, self);
	}
	else
	{
		// [BB] Added brackets to add server code.
		if (MissSound)
		{
			S_Sound (self, CHAN_WEAPON, MissSound, 1, ATTN_NORM);

			// [BB] If we're the server, make the sound on the client end.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SoundActor( self, CHAN_WEAPON, S_GetName( MissSound ), 1, ATTN_NORM );
		}
	}
}

//==========================================================================
//
// A fully customizable combo attack
//
//==========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_CustomComboAttack)
{
	ACTION_PARAM_START(6);
	ACTION_PARAM_CLASS(ti, 0);
	ACTION_PARAM_FIXED(SpawnHeight, 1);
	ACTION_PARAM_INT(damage, 2);
	ACTION_PARAM_SOUND(MeleeSound, 3);
	ACTION_PARAM_NAME(DamageType, 4);
	ACTION_PARAM_BOOL(bleed, 5);

	if (!self->target)
		return;
				
	A_FaceTarget (self);

	// [BB] This is handled server-side.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return;
	}

	if (self->CheckMeleeRange ())
	{
		if (DamageType==NAME_None) DamageType = NAME_Melee;	// Melee is the default type
		if (MeleeSound)
		{
			S_Sound (self, CHAN_WEAPON, MeleeSound, 1, ATTN_NORM);

			// [BB] If we're the server, make the sound on the client end.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SoundActor( self, CHAN_WEAPON, S_GetName( MeleeSound ), 1, ATTN_NORM );
		}
		P_DamageMobj (self->target, self, self, damage, DamageType);
		if (bleed) P_TraceBleed (damage, self->target, self);
	}
	else if (ti) 
	{
		// This seemingly senseless code is needed for proper aiming.
		self->z+=SpawnHeight-32*FRACUNIT;
		AActor * missile = P_SpawnMissileXYZ (self->x, self->y, self->z + 32*FRACUNIT, self, self->target, ti, false);
		self->z-=SpawnHeight-32*FRACUNIT;

		if (missile)
		{
			// automatic handling of seeker missiles
			if (missile->flags2&MF2_SEEKERMISSILE)
			{
				missile->tracer=self->target;
			}
			// set the health value so that the missile works properly
			if (missile->flags4&MF4_SPECTRAL)
			{
				missile->health=-2;
			}
			bool bSucces = P_CheckMissileSpawn(missile);

			// [BB] If we're the server, tell clients to spawn this missile.
			if ( bSucces && ( NETWORK_GetState( ) == NETSTATE_SERVER ) )
				SERVERCOMMANDS_SpawnMissile( missile );
		}
	}
}

//==========================================================================
//
// State jump function
//
//==========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_JumpIfNoAmmo)
{
	ACTION_PARAM_START(1);
	ACTION_PARAM_STATE(jump, 0);

	ACTION_SET_RESULT(false);	// Jumps should never set the result for inventory state chains!
	if (!ACTION_CALL_FROM_WEAPON()) return;

	if (!self->player->ReadyWeapon->CheckAmmo(self->player->ReadyWeapon->bAltFire, false, true))
	{
		ACTION_JUMP(jump, false);	// [BC] Clients have ammo information.
	}

}


//==========================================================================
//
// An even more customizable hitscan attack
//
//==========================================================================
enum FB_Flags
{
	FBF_USEAMMO = 1,
	FBF_NORANDOM = 2,
	FBF_EXPLICITANGLE = 4,
};

// [BB] This functions is needed to keep code duplication at a minimum while applying the spread power.
void A_FireBulletsHelper ( AActor *self,
						   int NumberOfBullets,
						   const int DamagePerBullet,
						   const player_t * player,
						   const int bangle,
						   const int bslope,
						   const fixed_t Range,
						   const PClass * PuffType,
						   const angle_t Spread_XY,
						   const angle_t Spread_Z,
						   const int Flags )
{
	if ((NumberOfBullets==1 && !player->refire) || NumberOfBullets==0)
	{
		int damage = DamagePerBullet;

		if (!(Flags & FBF_NORANDOM))
			damage *= ((pr_cwbullet()%3)+1);

		P_LineAttack(self, bangle, Range, bslope, damage, NAME_None, PuffType);
	}
	else 
	{
		if (NumberOfBullets == -1) NumberOfBullets = 1;
		for (int i=0 ; i<NumberOfBullets ; i++)
		{
			int angle = bangle;
			int slope = bslope;

			if (Flags & FBF_EXPLICITANGLE)
			{
				angle += Spread_XY;
				slope += Spread_Z;
			}
			else
			{
				angle += pr_cwbullet.Random2() * (Spread_XY / 255);
				slope += pr_cwbullet.Random2() * (Spread_Z / 255);
			}
			int damage = DamagePerBullet;

			if (!(Flags & FBF_NORANDOM))
				damage *= ((pr_cwbullet()%3)+1);

			P_LineAttack(self, angle, Range, slope, damage, NAME_None, PuffType);
		}
	}
}
// [BB] This function should be called by all bullet firing weapons to reduce code duplication.
void A_CustomFireBullets( AActor *self,
						  angle_t Spread_XY,
						  angle_t Spread_Z, 
						  int NumberOfBullets,
						  int DamagePerBullet,
						  const PClass * PuffType,
						  int Flags,
						  fixed_t Range,
						  const bool pPlayAttacking = true ){
  	if ( self->player == NULL)
		return;

	if (!self->player) return;

	player_t * player=self->player;
	AWeapon * weapon=player->ReadyWeapon;

	//int i;
	int bangle;
	int bslope;

	if ((Flags & FBF_USEAMMO) && weapon)
	{
		if (!weapon->DepleteAmmo(weapon->bAltFire, true)) return;	// out of ammo
	}
	
	if (Range == 0) Range = PLAYERMISSILERANGE;

	// [BB] Allow to disable the execution of PlayAttacking2.
	if ( pPlayAttacking )
	{
		// [BC] If we're the server, tell clients to update this player's state.
		if (( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( player ))
			SERVERCOMMANDS_SetPlayerState( ULONG( player - players ), STATE_PLAYER_ATTACK2, ULONG( player - players ), SVCF_SKIPTHISCLIENT );

		// [BB] Clients only do this for "their" player.
		if ( NETWORK_IsConsolePlayerOrNotInClientMode( player ) )
			static_cast<APlayerPawn *>(self)->PlayAttacking2 ();
	}

	bslope = P_BulletSlope(self);
	bangle = self->angle;

	if (!PuffType) PuffType = PClass::FindClass(NAME_BulletPuff);

	// [BC] If we're the server, tell clients that a weapon is being fired.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		if ( player )
			SERVERCOMMANDS_WeaponSound( ULONG( player - players ), S_GetName( weapon->AttackSound ), ULONG( player - players ), SVCF_SKIPTHISCLIENT );
		else
			SERVERCOMMANDS_SoundActor( self, CHAN_WEAPON, S_GetName( weapon->AttackSound ), 1, ATTN_NORM, player ? ULONG( player - players ) : MAXPLAYERS, SVCF_SKIPTHISCLIENT );
	}

	// [BB] Client's should only play weapon sounds, if they are looking through the eyes of the player
	// firing the sound. Otherwise the sound is played because of the SERVERCOMMANDS_WeaponSound command.
	// We always have to play our own sounds though.
	if ( NETWORK_IsConsolePlayerOrSpiedByConsolePlayerOrNotInClientMode ( player ) )
		S_Sound (self, CHAN_WEAPON, weapon->AttackSound, 1, ATTN_NORM);

	// [BC] Weapons are handled by the server.
	// [BB] To make hitscan decals kinda work online, we may not stop here yet.
	if ((( NETWORK_GetState( ) == NETSTATE_CLIENT ) || ( CLIENTDEMO_IsPlaying( ))) &&
		( cl_hitscandecalhack == false ))
	{
		return;
	}

	A_FireBulletsHelper ( self, NumberOfBullets, DamagePerBullet, player, bangle, bslope, Range, PuffType, Spread_XY, Spread_Z, Flags );

	if ( self->player->cheats & CF_SPREAD )
	{
		A_FireBulletsHelper ( self, NumberOfBullets, DamagePerBullet, player, bangle + ( ANGLE_45 / 3 ), bslope, Range, PuffType, Spread_XY, Spread_Z, Flags );
		A_FireBulletsHelper ( self, NumberOfBullets, DamagePerBullet, player, bangle - ( ANGLE_45 / 3 ), bslope, Range, PuffType, Spread_XY, Spread_Z, Flags );
	}

	// [BB] Even with the online hitscan decal hack, a client has to stop here.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return;
	}

	// [BB] If the player hit a player with his attack, potentially give him a medal.
	if ( player->bStruckPlayer )
		PLAYER_StruckPlayer( player );
	else
		player->ulConsecutiveHits = 0;

	// [BB] Tell all the bots that a weapon was fired.
	// This is more or less a hack, so that the bots are notified when the
	// decorate versions of the Doom weapons are fired.
	const char* pzsWeaponName = weapon->GetClass( )->TypeName.GetChars( );
	if ( stricmp( pzsWeaponName, "Pistol" ) == 0 )
	{
		BOTS_PostWeaponFiredEvent( ULONG( player - players ), BOTEVENT_FIREDPISTOL, BOTEVENT_ENEMY_FIREDPISTOL, BOTEVENT_PLAYER_FIREDPISTOL );
	}
	else if ( stricmp( pzsWeaponName, "Shotgun" ) == 0 )
	{
		BOTS_PostWeaponFiredEvent( ULONG( player - players ), BOTEVENT_FIREDSHOTGUN, BOTEVENT_ENEMY_FIREDSHOTGUN, BOTEVENT_PLAYER_FIREDSHOTGUN );
	}
	else if ( stricmp( pzsWeaponName, "Chaingun" ) == 0 )
	{
		BOTS_PostWeaponFiredEvent( ULONG( player - players ), BOTEVENT_FIREDCHAINGUN, BOTEVENT_ENEMY_FIREDCHAINGUN, BOTEVENT_PLAYER_FIREDCHAINGUN );
	}
	else if ( stricmp( pzsWeaponName, "SuperShotgun" ) == 0 )
	{
		BOTS_PostWeaponFiredEvent( ULONG( player - players ), BOTEVENT_FIREDSSG, BOTEVENT_ENEMY_FIREDSSG, BOTEVENT_PLAYER_FIREDSSG );
	}
	else if ( stricmp( pzsWeaponName, "Minigun" ) == 0 )
	{
		BOTS_PostWeaponFiredEvent( ULONG( player - players ), BOTEVENT_FIREDMINIGUN, BOTEVENT_ENEMY_FIREDMINIGUN, BOTEVENT_PLAYER_FIREDMINIGUN );
	}
	else if ( stricmp( pzsWeaponName, "BFG10k" ) == 0 )
	{
		BOTS_PostWeaponFiredEvent( ULONG( player - players ), BOTEVENT_FIREDBFG10K, BOTEVENT_ENEMY_FIREDBFG10K, BOTEVENT_PLAYER_FIREDBFG10K );
	}

}


DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_FireBullets)
{
	ACTION_PARAM_START(7);
	ACTION_PARAM_ANGLE(Spread_XY, 0);
	ACTION_PARAM_ANGLE(Spread_Z, 1);
	ACTION_PARAM_INT(NumberOfBullets, 2);
	ACTION_PARAM_INT(DamagePerBullet, 3);
	ACTION_PARAM_CLASS(PuffType, 4);
	ACTION_PARAM_INT(Flags, 5);
	ACTION_PARAM_FIXED(Range, 6);

	if (!self->player) return;

	A_CustomFireBullets( self, Spread_XY, Spread_Z, NumberOfBullets, DamagePerBullet, PuffType, Flags, Range);
}


//==========================================================================
//
// A_FireProjectile
//
//==========================================================================
// [BB] This functions is needed to keep code duplication at a minimum while applying the spread power.
void A_FireCustomMissileHelper ( AActor * self,
								 const fixed_t x,
								 const fixed_t y,
								 const fixed_t z,
								 const fixed_t shootangle,
								 const PClass * ti,
								 const angle_t Angle,
								 const INTBOOL AimAtAngle,
								 AActor *&linetarget )
{
	// [BB] Don't tell the clients to spawn the missile yet. This is done later
	// after we are done manipulating angle and momentum.
	AActor * misl=P_SpawnPlayerMissile (self, x, y, z, ti, shootangle, &linetarget,	NULL, false, true, false);
	// automatic handling of seeker missiles
	if (misl)
	{
		if (linetarget && misl->flags2&MF2_SEEKERMISSILE) misl->tracer=linetarget;
		if (!AimAtAngle)
		{
			// This original implementation is to aim straight ahead and then offset
			// the angle from the resulting direction. 
			FVector3 velocity(misl->velx, misl->vely, 0);
			fixed_t missilespeed = (fixed_t)velocity.Length();
			misl->angle += Angle;
			angle_t an = misl->angle >> ANGLETOFINESHIFT;
			misl->velx = FixedMul (missilespeed, finecosine[an]);
			misl->vely = FixedMul (missilespeed, finesine[an]);
		}
		if (misl->flags4&MF4_SPECTRAL) misl->health=-1;

		// [BC] If we're the server, tell clients to spawn this missile.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SpawnMissileExact( misl );
	}
}

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_FireCustomMissile)
{
	ACTION_PARAM_START(7);
	ACTION_PARAM_CLASS(ti, 0);
	ACTION_PARAM_ANGLE(Angle, 1);
	ACTION_PARAM_BOOL(UseAmmo, 2);
	ACTION_PARAM_INT(SpawnOfs_XY, 3);
	ACTION_PARAM_FIXED(SpawnHeight, 4);
	ACTION_PARAM_BOOL(AimAtAngle, 5);
	ACTION_PARAM_ANGLE(pitch, 6);

	if (!self->player) return;

	player_t *player=self->player;
	AWeapon * weapon=player->ReadyWeapon;
	AActor *linetarget;

	if (UseAmmo && weapon)
	{
		if (!weapon->DepleteAmmo(weapon->bAltFire, true)) return;	// out of ammo
	}

	// [BB] Should the actor not be spawned, taking in account client side only actors?
	if ( shouldActorNotBeSpawned ( self, ti ) )
		return;

	if (ti) 
	{
		angle_t ang = (self->angle - ANGLE_90) >> ANGLETOFINESHIFT;
		fixed_t x = SpawnOfs_XY * finecosine[ang];
		fixed_t y = SpawnOfs_XY * finesine[ang];
		fixed_t z = SpawnHeight;
		fixed_t shootangle = self->angle;

		if (AimAtAngle) shootangle+=Angle;

		// Temporarily adjusts the pitch
		fixed_t SavedPlayerPitch = self->pitch;
		self->pitch -= pitch;

		A_FireCustomMissileHelper( self, x, y, z, shootangle, ti, Angle , AimAtAngle, linetarget );

		if (NULL != self->player )
		{
			if ( self->player->cheats & CF_SPREAD )
			{
				A_FireCustomMissileHelper( self, x, y, z, shootangle + ( ANGLE_45 / 3 ), ti, Angle, AimAtAngle, linetarget );
				A_FireCustomMissileHelper( self, x, y, z, shootangle - ( ANGLE_45 / 3 ), ti, Angle, AimAtAngle, linetarget );
			}
		}

		//AActor * misl=P_SpawnPlayerMissile (self, x, y, z, ti, shootangle, &linetarget);
		self->pitch = SavedPlayerPitch;
/*
		// automatic handling of seeker missiles
		if (misl)
		{
			if (linetarget && misl->flags2&MF2_SEEKERMISSILE) misl->tracer=linetarget;
			if (!AimAtAngle)
			{
				// This original implementation is to aim straight ahead and then offset
				// the angle from the resulting direction. 
				FVector3 velocity(misl->velx, misl->vely, 0);
				fixed_t missilespeed = (fixed_t)velocity.Length();
				misl->angle += Angle;
				angle_t an = misl->angle >> ANGLETOFINESHIFT;
				misl->velx = FixedMul (missilespeed, finecosine[an]);
				misl->vely = FixedMul (missilespeed, finesine[an]);
			}

			// [BC] If we're the server, tell clients to spawn this missile.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SpawnMissile( misl );
		}
*/
	}
}


//==========================================================================
//
// A_CustomPunch
//
// Berserk is not handled here. That can be done with A_CheckIfInventory
//
//==========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_CustomPunch)
{
	ACTION_PARAM_START(5);
	ACTION_PARAM_INT(Damage, 0);
	ACTION_PARAM_BOOL(norandom, 1);
	ACTION_PARAM_BOOL(UseAmmo, 2);
	ACTION_PARAM_CLASS(PuffType, 3);
	ACTION_PARAM_FIXED(Range, 4);
	ACTION_PARAM_FIXED(LifeSteal, 5);

	if (!self->player) return;

	player_t *player=self->player;
	AWeapon * weapon=player->ReadyWeapon;


	angle_t 	angle;
	int 		pitch;
	AActor *	linetarget;

	// [BC] Weapons are handled by the server.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		if (( self->ulNetworkFlags & NETFL_CLIENTSIDEONLY ) == false )
			return;
	}

	if (!norandom) Damage *= (pr_cwpunch()%8+1);

	angle = self->angle + (pr_cwpunch.Random2() << 18);
	if (Range == 0) Range = MELEERANGE;
	pitch = P_AimLineAttack (self, angle, Range, &linetarget);

	// only use ammo when actually hitting something!
	if (UseAmmo && linetarget && weapon)
	{
		if (!weapon->DepleteAmmo(weapon->bAltFire, true)) return;	// out of ammo

		if ( (NETWORK_GetState( ) == NETSTATE_SERVER) && ( weapon->Owner ) && ( weapon->Owner->player ) )
		{
			// [BB] If we're the server, tell the client that he lost ammo.
			if ( weapon->Ammo1 )
				SERVERCOMMANDS_GiveInventory( weapon->Owner->player - players, weapon->Ammo1 );
			if ( weapon->Ammo2 )
				SERVERCOMMANDS_GiveInventory( weapon->Owner->player - players, weapon->Ammo2 );
		}
	}

	if (!PuffType) PuffType = PClass::FindClass(NAME_BulletPuff);

	P_LineAttack (self, angle, Range, pitch, Damage, NAME_None, PuffType, true);

	// turn to face target
	if (linetarget)
	{
		if (LifeSteal)
			P_GiveBody (self, (Damage * LifeSteal) >> FRACBITS);

		S_Sound (self, CHAN_WEAPON, weapon->AttackSound, 1, ATTN_NORM);

		self->angle = R_PointToAngle2 (self->x,
										self->y,
										linetarget->x,
										linetarget->y);

		// [BC] Play the hit sound to clients.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		{
			SERVERCOMMANDS_SoundActor( self, CHAN_WEAPON, S_GetName( weapon->AttackSound ), 1, ATTN_NORM );
			SERVERCOMMANDS_SetThingAngleExact( self );
		}
	}
}


enum
{	
	RAF_SILENT = 1,
	RAF_NOPIERCE = 2
};

//==========================================================================
//
// customizable railgun attack function
//
//==========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_RailAttack)
{
	ACTION_PARAM_START(8);
	ACTION_PARAM_INT(Damage, 0);
	ACTION_PARAM_INT(Spawnofs_XY, 1);
	ACTION_PARAM_BOOL(UseAmmo, 2);
	ACTION_PARAM_COLOR(Color1, 3);
	ACTION_PARAM_COLOR(Color2, 4);
	ACTION_PARAM_INT(Flags, 5);
	ACTION_PARAM_FLOAT(MaxDiff, 6);
	ACTION_PARAM_CLASS(PuffType, 7);	

	if (!self->player) return;

	AWeapon * weapon=self->player->ReadyWeapon;

	// only use ammo when actually hitting something!
	if (UseAmmo)
	{
		if (!weapon->DepleteAmmo(weapon->bAltFire, true)) return;	// out of ammo
	}

	// [BC] Don't actually do the attack in client mode.
	// [Spleen] Railgun is handled by the server unless unlagged
	if ( ( ( NETWORK_GetState( ) == NETSTATE_CLIENT ) || CLIENTDEMO_IsPlaying( ) )
		&& !UNLAGGED_DrawRailClientside( self ) )
	{
		if (( self->ulNetworkFlags & NETFL_CLIENTSIDEONLY ) == false )
			return;
	}

	P_RailAttackWithPossibleSpread (self, Damage, Spawnofs_XY, Color1, Color2, MaxDiff, (Flags & RAF_SILENT), PuffType, (!(Flags & RAF_NOPIERCE)));
}

//==========================================================================
//
// also for monsters
//
//==========================================================================
enum
{
	CRF_DONTAIM = 0,
	CRF_AIMPARALLEL = 1,
	CRF_AIMDIRECT = 2
};

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_CustomRailgun)
{
	ACTION_PARAM_START(8);
	ACTION_PARAM_INT(Damage, 0);
	ACTION_PARAM_INT(Spawnofs_XY, 1);
	ACTION_PARAM_COLOR(Color1, 2);
	ACTION_PARAM_COLOR(Color2, 3);
	ACTION_PARAM_INT(Flags, 4);
	ACTION_PARAM_INT(aim, 5);
	ACTION_PARAM_FLOAT(MaxDiff, 6);
	ACTION_PARAM_CLASS(PuffType, 7);

	AActor *linetarget;

	fixed_t saved_x = self->x;
	fixed_t saved_y = self->y;
	angle_t saved_angle = self->angle;
	fixed_t saved_pitch = self->pitch;

	if (aim && self->target == NULL)
	{
		return;
	}
	// [RH] Andy Baker's stealth monsters
	if (self->flags & MF_STEALTH)
	{
		self->visdir = 1;
	}

	self->flags &= ~MF_AMBUSH;


	if (aim)
	{
		self->angle = R_PointToAngle2 (self->x,
										self->y,
										self->target->x,
										self->target->y);
	}
	self->pitch = P_AimLineAttack (self, self->angle, MISSILERANGE, &linetarget, ANGLE_1*60, 0, aim ? self->target : NULL);
	if (linetarget == NULL && aim)
	{
		// We probably won't hit the target, but aim at it anyway so we don't look stupid.
		FVector2 xydiff(self->target->x - self->x, self->target->y - self->y);
		double zdiff = (self->target->z + (self->target->height>>1)) -
						(self->z + (self->height>>1) - self->floorclip);
		self->pitch = int(atan2(zdiff, xydiff.Length()) * ANGLE_180 / -M_PI);
	}
	// Let the aim trail behind the player
	if (aim)
	{
		saved_angle = self->angle = R_PointToAngle2 (self->x, self->y,
										self->target->x - self->target->velx * 3,
										self->target->y - self->target->vely * 3);

		if (aim == CRF_AIMDIRECT)
		{
			// Tricky: We must offset to the angle of the current position
			// but then change the angle again to ensure proper aim.
			self->x += Spawnofs_XY * finecosine[self->angle];
			self->y += Spawnofs_XY * finesine[self->angle];
			Spawnofs_XY = 0;
			self->angle = R_PointToAngle2 (self->x, self->y,
											self->target->x - self->target->velx * 3,
											self->target->y - self->target->vely * 3);
		}

		if (self->target->flags & MF_SHADOW)
		{
			angle_t rnd = pr_crailgun.Random2() << 21;
			self->angle += rnd;
			saved_angle = rnd;
		}
	}

	angle_t angle = (self->angle - ANG90) >> ANGLETOFINESHIFT;

	P_RailAttackWithPossibleSpread (self, Damage, Spawnofs_XY, Color1, Color2, MaxDiff, (Flags & RAF_SILENT), PuffType, (!(Flags & RAF_NOPIERCE)));

	self->x = saved_x;
	self->y = saved_y;
	self->angle = saved_angle;
	self->pitch = saved_pitch;
}

//===========================================================================
//
// DoGiveInventory
//
//===========================================================================

static void DoGiveInventory(AActor * receiver, DECLARE_PARAMINFO)
{
	ACTION_PARAM_START(2);
	ACTION_PARAM_CLASS(mi, 0);
	ACTION_PARAM_INT(amount, 1);
	bool	bNeedClientUpdate;

	bool res=true;
	// [BC] Don't jump here in client mode.
	if (( self->player ) &&
		(( CallingState == self->player->psprites[ps_weapon].state ) || ( CallingState == self->player->psprites[ps_flash].state )))
	{
		bNeedClientUpdate = false;
	}
	else
	{
		bNeedClientUpdate = true;
		
		if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
			( CLIENTDEMO_IsPlaying( )))
		{
			return;
		}
	}

	if (receiver == NULL) return;

	if (amount==0) amount=1;
	if (mi) 
	{
		AInventory *item = static_cast<AInventory *>(Spawn (mi, 0, 0, 0, NO_REPLACE));
		if (item->IsKindOf(RUNTIME_CLASS(AHealth)))
		{
			item->Amount *= amount;
		}
		else
		{
			item->Amount = amount;
		}
		item->flags |= MF_DROPPED;
		if (item->flags & MF_COUNTITEM)
		{
			item->flags&=~MF_COUNTITEM;
			level.total_items--;
		}
		if (!item->CallTryPickup (receiver))
		{
			item->Destroy ();
			res = false;
		}
		else
		{
			res = true;
			// [BB] If we're the server, give the item to the clients.
			if (( NETWORK_GetState( ) == NETSTATE_SERVER ) &&
				( bNeedClientUpdate ))
			{
				SERVERCOMMANDS_GiveInventoryNotOverwritingAmount( receiver, item );
			}
		}
	}
	else res = false;
	ACTION_SET_RESULT(res);

}	

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_GiveInventory)
{
	DoGiveInventory(self, PUSH_PARAMINFO);
}	

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_GiveToTarget)
{
	DoGiveInventory(self->target, PUSH_PARAMINFO);
}	

//===========================================================================
//
// A_TakeInventory
//
//===========================================================================

enum
{
	TIF_NOTAKEINFINITE = 1,
};

void DoTakeInventory(AActor * receiver, DECLARE_PARAMINFO)
{
	ACTION_PARAM_START(3);
	ACTION_PARAM_CLASS(item, 0);
	ACTION_PARAM_INT(amount, 1);
	ACTION_PARAM_INT(flags, 2);
	bool	bNeedClientUpdate;

	
	// [BC] Don't jump here in client mode.
	if (( self->player ) &&
		(( CallingState == self->player->psprites[ps_weapon].state ) || ( CallingState == self->player->psprites[ps_flash].state )))
	{
		bNeedClientUpdate = false;
	}
	else
	{
		bNeedClientUpdate = true;
		
		if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
			( CLIENTDEMO_IsPlaying( )))
		{
			return;
		}
	}

	if (item == NULL || receiver == NULL) return;

	bool res = false;

	AInventory * inv = receiver->FindInventory(item);

	if (inv && !inv->IsKindOf(RUNTIME_CLASS(AHexenArmor)))
	{
		if (inv->Amount > 0)
		{
			res = true;
		}
		// Do not take ammo if the "no take infinite/take as ammo depletion" flag is set
		// and infinite ammo is on
		if (flags & TIF_NOTAKEINFINITE &&
			((dmflags & DF_INFINITE_AMMO) || (receiver->player->cheats & CF_INFINITEAMMO)) &&
			inv->IsKindOf(RUNTIME_CLASS(AAmmo)))
		{
			// Nothing to do here, except maybe res = false;? Would it make sense?
		}
		else if (!amount || amount>=inv->Amount) 
		{
			// [BC] Take the player's inventory.
			if (( NETWORK_GetState( ) == NETSTATE_SERVER ) &&
				( bNeedClientUpdate ) &&
				( inv->Owner ) &&
				( inv->Owner->player ))
			{
				SERVERCOMMANDS_TakeInventory( inv->Owner->player - players, inv->GetClass( )->TypeName.GetChars( ), 0 );
			}
			if (inv->ItemFlags&IF_KEEPDEPLETED) inv->Amount=0;
			else inv->Destroy();
		}
		else
		{
			inv->Amount-=amount;

			// [BC] Take the player's inventory.
			if (( NETWORK_GetState( ) == NETSTATE_SERVER ) &&
				( bNeedClientUpdate ) &&
				( inv->Owner ) &&
				( inv->Owner->player ))
			{
				SERVERCOMMANDS_TakeInventory( inv->Owner->player - players, inv->GetClass( )->TypeName.GetChars( ), inv->Amount );
			}
		}
	}
	ACTION_SET_RESULT(res);
}	

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_TakeInventory)
{
	DoTakeInventory(self, PUSH_PARAMINFO);
}	

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_TakeFromTarget)
{
	DoTakeInventory(self->target, PUSH_PARAMINFO);
}	

//===========================================================================
//
// Common code for A_SpawnItem and A_SpawnItemEx
//
//===========================================================================
enum SIX_Flags
{
	SIXF_TRANSFERTRANSLATION=1,
	SIXF_ABSOLUTEPOSITION=2,
	SIXF_ABSOLUTEANGLE=4,
	SIXF_ABSOLUTEVELOCITY=8,
	SIXF_SETMASTER=16,
	SIXF_NOCHECKPOSITION=32,
	SIXF_TELEFRAG=64,
	// [BB] Added flag which allows client side spawning.
	SIXF_CLIENTSIDESPAWN=128,
	SIXF_TRANSFERAMBUSHFLAG=256,
	SIXF_TRANSFERPITCH=512,
	SIXF_TRANSFERPOINTERS=1024,
};


// [BB] Changed return value to bool (returns false if the actor already was destroyed).
static bool InitSpawnedItem(AActor *self, AActor *mo, int flags)
{
	if (mo)
	{
		AActor * originator = self;

		if ((flags & SIXF_TRANSFERTRANSLATION) && !(mo->flags2 & MF2_DONTTRANSLATE))
		{
			mo->Translation = self->Translation;
		}
		if (flags & SIXF_TRANSFERPOINTERS)
		{
			mo->target = self->target;
			mo->master = self->master; // This will be overridden later if SIXF_SETMASTER is set
			mo->tracer = self->tracer;
		}

		mo->angle=self->angle;
		if (flags & SIXF_TRANSFERPITCH) mo->pitch = self->pitch;
		while (originator && isMissile(originator)) originator = originator->target;

		if (flags & SIXF_TELEFRAG) 
		{
			P_TeleportMove(mo, mo->x, mo->y, mo->z, true);
			// This is needed to ensure consistent behavior.
			// Otherwise it will only spawn if nothing gets telefragged
			flags |= SIXF_NOCHECKPOSITION;	
		}
		if (mo->flags3&MF3_ISMONSTER)
		{
			if (!(flags&SIXF_NOCHECKPOSITION) && !P_TestMobjLocation(mo))
			{
				// The monster is blocked so don't spawn it at all!
				if (mo->CountsAsKill())
				{
					level.total_monsters--;

					// [BB] The monster didn't spawn at all, so we need to correct the number of monsters in invasion mode.
					INVASION_UpdateMonsterCount( mo, true );
				}
				mo->Destroy();
				return false;
			}
			else if (originator)
			{
				if (originator->flags3&MF3_ISMONSTER)
				{
					// If this is a monster transfer all friendliness information
					mo->CopyFriendliness(originator, true);
					if (flags&SIXF_SETMASTER) mo->master = originator;	// don't let it attack you (optional)!
				}
				else if (originator->player)
				{
					// A player always spawns a monster friendly to him
					mo->flags|=MF_FRIENDLY;
					mo->FriendPlayer = int(originator->player-players+1);

					AActor * attacker=originator->player->attacker;
					if (attacker)
					{
						if (!(attacker->flags&MF_FRIENDLY) || 
							(deathmatch && attacker->FriendPlayer!=0 && attacker->FriendPlayer!=mo->FriendPlayer))
						{
							// Target the monster which last attacked the player
							mo->LastHeard = mo->target = attacker;
						}
					}
				}
			}
		}
		else if (!(flags & SIXF_TRANSFERPOINTERS))
		{
			// If this is a missile or something else set the target to the originator
			mo->target=originator? originator : self;
		}
	}
	return true;
}

//===========================================================================
//
// A_SpawnItem
//
// Spawns an item in front of the caller like Heretic's time bomb
//
//===========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_SpawnItem)
{
	ACTION_PARAM_START(5);
	ACTION_PARAM_CLASS(missile, 0);
	ACTION_PARAM_FIXED(distance, 1);
	ACTION_PARAM_FIXED(zheight, 2);
	ACTION_PARAM_BOOL(useammo, 3);
	ACTION_PARAM_BOOL(transfer_translation, 4);

	if (!missile) 
	{
		ACTION_SET_RESULT(false);
		return;
	}

	// Don't spawn monsters if this actor has been massacred
	if (self->DamageType == NAME_Massacre && GetDefaultByType(missile)->flags3&MF3_ISMONSTER) return;

	if (distance==0) 
	{
		// use the minimum distance that does not result in an overlap
		distance=(self->radius+GetDefaultByType(missile)->radius)>>FRACBITS;
	}

	if (ACTION_CALL_FROM_WEAPON())
	{
		// Used from a weapon so use some ammo
		AWeapon * weapon=self->player->ReadyWeapon;

		if (!weapon) return;
		if (useammo && !weapon->DepleteAmmo(weapon->bAltFire)) return;
	}

	// [BB] Should the actor not be spawned, taking in account client side only actors?
	if ( shouldActorNotBeSpawned ( self, missile ) )
		return;

	AActor * mo = Spawn( missile, 
					self->x + FixedMul(distance, finecosine[self->angle>>ANGLETOFINESHIFT]), 
					self->y + FixedMul(distance, finesine[self->angle>>ANGLETOFINESHIFT]), 
					self->z - self->floorclip + zheight, ALLOW_REPLACE);

	int flags = (transfer_translation? SIXF_TRANSFERTRANSLATION:0) + (useammo? SIXF_SETMASTER:0);
	bool res = InitSpawnedItem(self, mo, flags);
	if ( mo && res )
	{
		// [BC] If we're the server and the spawn was not blocked, tell clients to spawn the item.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		{
			SERVERCOMMANDS_SpawnThing( mo );

			if ( mo->angle != 0 )
				SERVERCOMMANDS_SetThingAngle( mo );

			if ( mo->Translation )
				SERVERCOMMANDS_SetThingTranslation( mo );
		}
		// [BB] The client did the spawning, so this has to be a client side only actor.
		else if ( ( NETWORK_GetState( ) == NETSTATE_CLIENT ) || ( CLIENTDEMO_IsPlaying( ) ) )
			mo->ulNetworkFlags |= NETFL_CLIENTSIDEONLY;
	}
	ACTION_SET_RESULT(res);	// for an inventory item's use state
}

//===========================================================================
//
// A_SpawnItemEx
//
// Enhanced spawning function
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_SpawnItemEx)
{
	ACTION_PARAM_START(10);
	ACTION_PARAM_CLASS(missile, 0);
	ACTION_PARAM_FIXED(xofs, 1);
	ACTION_PARAM_FIXED(yofs, 2);
	ACTION_PARAM_FIXED(zofs, 3);
	ACTION_PARAM_FIXED(xvel, 4);
	ACTION_PARAM_FIXED(yvel, 5);
	ACTION_PARAM_FIXED(zvel, 6);
	ACTION_PARAM_ANGLE(Angle, 7);
	ACTION_PARAM_INT(flags, 8);
	ACTION_PARAM_INT(chance, 9);

	if (!missile) 
	{
		ACTION_SET_RESULT(false);
		return;
	}

	if (chance > 0 && pr_spawnitemex()<chance) return;

	// Don't spawn monsters if this actor has been massacred
	if (self->DamageType == NAME_Massacre && GetDefaultByType(missile)->flags3&MF3_ISMONSTER) return;

	fixed_t x,y;

	if (!(flags & SIXF_ABSOLUTEANGLE))
	{
		Angle += self->angle;
	}

	angle_t ang = Angle >> ANGLETOFINESHIFT;

	if (flags & SIXF_ABSOLUTEPOSITION)
	{
		x = self->x + xofs;
		y = self->y + yofs;
	}
	else
	{
		// in relative mode negative y values mean 'left' and positive ones mean 'right'
		// This is the inverse orientation of the absolute mode!
		x = self->x + FixedMul(xofs, finecosine[ang]) + FixedMul(yofs, finesine[ang]);
		y = self->y + FixedMul(xofs, finesine[ang]) - FixedMul(yofs, finecosine[ang]);
	}

	if (!(flags & SIXF_ABSOLUTEVELOCITY))
	{
		// Same orientation issue here!
		fixed_t newxvel = FixedMul(xvel, finecosine[ang]) + FixedMul(yvel, finesine[ang]);
		yvel = FixedMul(xvel, finesine[ang]) - FixedMul(yvel, finecosine[ang]);
		xvel = newxvel;
	}

	// [BB] Should the actor not be spawned, taking in account client side only actors?
	if ( shouldActorNotBeSpawned ( self, missile, !!( flags & SIXF_CLIENTSIDESPAWN ) ) )
		return;

	AActor * mo = Spawn(missile, x, y, self->z - self->floorclip + zofs, ALLOW_REPLACE);
	bool res = InitSpawnedItem(self, mo, flags);
	ACTION_SET_RESULT(res);	// for an inventory item's use state
	if (mo)
	{
		mo->velx = xvel;
		mo->vely = yvel;
		mo->velz = zvel;
		mo->angle = Angle;
		if (flags & SIXF_TRANSFERAMBUSHFLAG)
			mo->flags = (mo->flags&~MF_AMBUSH) | (self->flags & MF_AMBUSH);

		// [BB] If we're the server and the spawn was not blocked, tell clients to spawn the item
		if ( res && (NETWORK_GetState( ) == NETSTATE_SERVER) )
		{
			SERVERCOMMANDS_SpawnThing( mo );

			// [BB] Set the angle and momentum if necessary.
			SERVER_SetThingNonZeroAngleAndMomentum( mo );

			if ( mo->Translation )
				SERVERCOMMANDS_SetThingTranslation( mo );
		}

		// [BC] Flag this actor as being client-spawned.
		if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
			( CLIENTDEMO_IsPlaying( )))
		{
			mo->ulNetworkFlags |= NETFL_CLIENTSIDEONLY;
		}
	}
}

//===========================================================================
//
// A_ThrowGrenade
//
// Throws a grenade (like Hexen's fighter flechette)
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_ThrowGrenade)
{
	ACTION_PARAM_START(5);
	ACTION_PARAM_CLASS(missile, 0);
	ACTION_PARAM_FIXED(zheight, 1);
	ACTION_PARAM_FIXED(xyvel, 2);
	ACTION_PARAM_FIXED(zvel, 3);
	ACTION_PARAM_BOOL(useammo, 4);

	if (missile == NULL) return;

	if (ACTION_CALL_FROM_WEAPON())
	{
		// Used from a weapon, so use some ammo
		AWeapon *weapon = self->player->ReadyWeapon;

		if (!weapon) return;
		if (useammo && !weapon->DepleteAmmo(weapon->bAltFire)) return;
	}

	// [BC] Weapons are handled by the server.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		if (( self->ulNetworkFlags & NETFL_CLIENTSIDEONLY ) == false )
			return;
	}

	AActor * bo;

	bo = Spawn(missile, self->x, self->y, 
			self->z - self->floorclip + zheight + 35*FRACUNIT + (self->player? self->player->crouchoffset : 0),
			ALLOW_REPLACE);
	if (bo)
	{
		P_PlaySpawnSound(bo, self);
		if (xyvel != 0)
			bo->Speed = xyvel;
		bo->angle = self->angle + (((pr_grenade()&7) - 4) << 24);

		angle_t pitch = angle_t(-self->pitch) >> ANGLETOFINESHIFT;
		angle_t angle = bo->angle >> ANGLETOFINESHIFT;

		// There are two vectors we are concerned about here: xy and z. We rotate
		// them separately according to the shooter's pitch and then sum them to
		// get the final velocity vector to shoot with.

		fixed_t xy_xyscale = FixedMul(bo->Speed, finecosine[pitch]);
		fixed_t xy_velz = FixedMul(bo->Speed, finesine[pitch]);
		fixed_t xy_velx = FixedMul(xy_xyscale, finecosine[angle]);
		fixed_t xy_vely = FixedMul(xy_xyscale, finesine[angle]);

		pitch = angle_t(self->pitch) >> ANGLETOFINESHIFT;
		fixed_t z_xyscale = FixedMul(zvel, finesine[pitch]);
		fixed_t z_velz = FixedMul(zvel, finecosine[pitch]);
		fixed_t z_velx = FixedMul(z_xyscale, finecosine[angle]);
		fixed_t z_vely = FixedMul(z_xyscale, finesine[angle]);

		bo->velx = xy_velx + z_velx + (self->velx >> 1);
		bo->vely = xy_vely + z_vely + (self->vely >> 1);
		bo->velz = xy_velz + z_velz;

		bo->target= self;

		// [BC] Tell clients to spawn this missile.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SpawnMissileExact( bo );

		P_CheckMissileSpawn (bo);
	} 
	else ACTION_SET_RESULT(false);
}


//===========================================================================
//
// A_Recoil
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_Recoil)
{
	ACTION_PARAM_START(1);
	ACTION_PARAM_FIXED(xyvel, 0);

	// [BB] For non-player non-clientsideonly actors, this is server side.
	// Note: I'm not sure whether this should be server side also for players.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		if ( (( self->ulNetworkFlags & NETFL_CLIENTSIDEONLY ) == false ) && ( self->player == NULL ) )
			return;
	}

	angle_t angle = self->angle + ANG180;
	angle >>= ANGLETOFINESHIFT;
	self->velx += FixedMul (xyvel, finecosine[angle]);
	self->vely += FixedMul (xyvel, finesine[angle]);

	// [BB] Set the thing's momentum, also resync the position.
	if ( ( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( self->player == NULL ) )
		SERVERCOMMANDS_MoveThingExact( self, CM_X|CM_Y|CM_Z|CM_MOMX|CM_MOMY );
}


//===========================================================================
//
// A_SelectWeapon
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_SelectWeapon)
{
	ACTION_PARAM_START(1);
	ACTION_PARAM_CLASS(cls, 0);

	if (cls == NULL || self->player == NULL) 
	{
		ACTION_SET_RESULT(false);
		return;
	}

	AWeapon * weaponitem = static_cast<AWeapon*>(self->FindInventory(cls));

	if (weaponitem != NULL && weaponitem->IsKindOf(RUNTIME_CLASS(AWeapon)))
	{
		if (self->player->ReadyWeapon != weaponitem)
		{
			self->player->PendingWeapon = weaponitem;
		}
	}
	else ACTION_SET_RESULT(false);

}


//===========================================================================
//
// A_Print
//
//===========================================================================
EXTERN_CVAR(Float, con_midtime)

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_Print)
{
	ACTION_PARAM_START(3);
	ACTION_PARAM_STRING(text, 0);
	ACTION_PARAM_FLOAT(time, 1);
	ACTION_PARAM_NAME(fontname, 2);

	// [BB] The server always generates the message and just checks whom to send it to.
	if (self->CheckLocalView (consoleplayer) ||
		(self->target!=NULL && self->target->CheckLocalView (consoleplayer))
		|| ( NETWORK_GetState( ) == NETSTATE_SERVER ) )
	{
		float saved = con_midtime;
		FFont *font = NULL;
		
		if (fontname != NAME_None)
		{
			font = V_GetFont(fontname);
		}
		if (time > 0)
		{
			con_midtime = time;
		}
		
		FString formatted = strbin1(text);
		C_MidPrint(font != NULL ? font : SmallFont, formatted.GetChars());
		// [BB] The server sends out the message and doesn't have a screen.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		{
			LONG player = -1;
			if ( self->player )
				player = ULONG(self->player-players);
			else if ( self->target && self->target->player )
				player = ULONG(self->target->player-players);
			// [BB] We can't use SERVERCOMMANDS_PrintMid since time and font may be altered.
			// To avoid writing yet another special server command for this, just use
			// SERVERCOMMANDS_PrintHUDMessage and fill it with the default arguments of used
			// in C_MidPrint.
			if ( player >= 0 )
				SERVERCOMMANDS_PrintHUDMessage( text, 1.5f, 0.375f, 0, 0, CR_GOLD, con_midtime, (fontname != NAME_None) ? fontname.GetChars() : SERVER_GetCurrentFont( ), true, MAKE_ID('C','N','T','R'), ULONG(player), SVCF_ONLYTHISCLIENT );
		}

		con_midtime = saved;
	}
}

//===========================================================================
//
// A_PrintBold
//
//===========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_PrintBold)
{
	ACTION_PARAM_START(3);
	ACTION_PARAM_STRING(text, 0);
	ACTION_PARAM_FLOAT(time, 1);
	ACTION_PARAM_NAME(fontname, 2);

	float saved = con_midtime;
	FFont *font = NULL;
	
	if (fontname != NAME_None)
	{
		font = V_GetFont(fontname);
	}
	if (time > 0)
	{
		con_midtime = time;
	}
	
	FString formatted = strbin1(text);
	C_MidPrintBold(font != NULL ? font : SmallFont, formatted.GetChars());
	con_midtime = saved;
}

//===========================================================================
//
// A_Log
//
//===========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_Log)
{
	ACTION_PARAM_START(1);
	ACTION_PARAM_STRING(text, 0);
	Printf("%s\n", text);
}

//===========================================================================
//
// A_LogInt
//
//===========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_LogInt)
{
	ACTION_PARAM_START(1);
	ACTION_PARAM_INT(num, 0);
	Printf("%d\n", num);
}

//===========================================================================
//
// A_SetTranslucent
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_SetTranslucent)
{
	ACTION_PARAM_START(2);
	ACTION_PARAM_FIXED(alpha, 0);
	ACTION_PARAM_INT(mode, 1);

	mode = mode == 0 ? STYLE_Translucent : mode == 2 ? STYLE_Fuzzy : STYLE_Add;

	self->RenderStyle.Flags &= ~STYLEF_Alpha1;
	self->alpha = clamp<fixed_t>(alpha, 0, FRACUNIT);
	self->RenderStyle = ERenderStyle(mode);
}

//===========================================================================
//
// A_FadeIn
//
// Fades the actor in
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_FadeIn)
{
	ACTION_PARAM_START(1);
	ACTION_PARAM_FIXED(reduce, 0);

	// [BB] This is handled server-side.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		if (( self->ulNetworkFlags & NETFL_CLIENTSIDEONLY ) == false )
			return;
	}

	if (reduce == 0)
	{
		reduce = FRACUNIT/10;
	}

	// [BB] If the RenderStyle is changed, we have to inform the clients.
	const bool renderStyleChanged = !!( self->RenderStyle.Flags & STYLEF_Alpha1 );

	self->RenderStyle.Flags &= ~STYLEF_Alpha1;
	self->alpha += reduce;
	// Should this clamp alpha to 1.0?

	// [BB] Inform the clients about the alpha change and possibly about RenderStyle.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		if ( renderStyleChanged )
			SERVERCOMMANDS_SetThingProperty( self, APROP_RenderStyle );
		SERVERCOMMANDS_SetThingProperty( self, APROP_Alpha );
	}

}

//===========================================================================
//
// A_FadeOut
//
// fades the actor out and destroys it when done
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_FadeOut)
{
	ACTION_PARAM_START(2);
	ACTION_PARAM_FIXED(reduce, 0);
	ACTION_PARAM_BOOL(remove, 1);

	// [BB] This is handled server-side.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		if (( self->ulNetworkFlags & NETFL_CLIENTSIDEONLY ) == false )
			return;
	}

	if (reduce == 0)
	{
		reduce = FRACUNIT/10;
	}
	// [BB] If the RenderStyle is changed, we have to inform the clients.
	const bool renderStyleChanged = !!( self->RenderStyle.Flags & STYLEF_Alpha1 );

	self->RenderStyle.Flags &= ~STYLEF_Alpha1;
	self->alpha -= reduce;

	// [BB] Inform the clients about the alpha change and possibly about RenderStyle.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		if ( renderStyleChanged )
			SERVERCOMMANDS_SetThingProperty( self, APROP_RenderStyle );
		SERVERCOMMANDS_SetThingProperty( self, APROP_Alpha );
	}

	// [BB] Only destroy the actor if it's not needed for a map reset. Otherwise just hide it.
	if (self->alpha <= 0 && remove)
	{
		// [BB] Deleting player bodies is a very bad idea.
		if ( self->player && ( self->player->mo == self ) )
		{
			Printf ( PRINT_BOLD, "Warning: A_FadeOut may not delete player bodies that are still associated to a player!\n" );
			return;
		}

		// [BB] Tell clients to destroy the actor.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_DestroyThing( self );

		self->HideOrDestroyIfSafe ();
	}
}

//===========================================================================
//
// A_FadeTo
//
// fades the actor to a specified transparency by a specified amount and
// destroys it if so desired
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_FadeTo)
{
	ACTION_PARAM_START(3);
	ACTION_PARAM_FIXED(target, 0);
	ACTION_PARAM_FIXED(amount, 1);
	ACTION_PARAM_BOOL(remove, 2);

	self->RenderStyle.Flags &= ~STYLEF_Alpha1;

	if (self->alpha > target)
	{
		self->alpha -= amount;

		if (self->alpha < target)
		{
			self->alpha = target;
		}
	}
	else if (self->alpha < target)
	{
		self->alpha += amount;

		if (self->alpha > target)
		{
			self->alpha = target;
		}
	}
	if (self->alpha == target && remove)
	{
		self->Destroy();
	}
}

//===========================================================================
//
// A_SpawnDebris
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_SpawnDebris)
{
	int i;
	AActor * mo;

	ACTION_PARAM_START(4);
	ACTION_PARAM_CLASS(debris, 0);
	ACTION_PARAM_BOOL(transfer_translation, 1);
	ACTION_PARAM_FIXED(mult_h, 2);
	ACTION_PARAM_FIXED(mult_v, 3);

	if (debris == NULL) return;

	// only positive values make sense here
	if (mult_v<=0) mult_v=FRACUNIT;
	if (mult_h<=0) mult_h=FRACUNIT;
	
	for (i = 0; i < GetDefaultByType(debris)->health; i++)
	{
		mo = Spawn(debris, self->x+((pr_spawndebris()-128)<<12),
			self->y+((pr_spawndebris()-128)<<12), 
			self->z+(pr_spawndebris()*self->height/256), ALLOW_REPLACE);
		if (mo && transfer_translation)
		{
			mo->Translation = self->Translation;
		}
		if (mo && i < mo->GetClass()->ActorInfo->NumOwnedStates)
		{
			mo->SetState (mo->GetClass()->ActorInfo->OwnedStates + i);
			mo->velz = FixedMul(mult_v, ((pr_spawndebris()&7)+5)*FRACUNIT);
			mo->velx = FixedMul(mult_h, pr_spawndebris.Random2()<<(FRACBITS-6));
			mo->vely = FixedMul(mult_h, pr_spawndebris.Random2()<<(FRACBITS-6));
		}
	}
}


//===========================================================================
//
// A_CheckSight
// jumps if no player can see this actor
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_CheckSight)
{
	ACTION_PARAM_START(1);
	ACTION_PARAM_STATE(jump, 0);

	ACTION_SET_RESULT(false);	// Jumps should never set the result for inventory state chains!

	// [BB] If this is a CLIENTSIDEONLY actor, a client only checks whether the consoleplayer sees it.
	// [Dusk] If the actor does NOT have CLIENTSIDEONLY, the client does nothing.
	if ( NETWORK_InClientMode( ) )
	{
		if ( !( self->ulNetworkFlags & NETFL_CLIENTSIDEONLY ) ||
			P_CheckSight( players[consoleplayer].camera, self, SF_IGNOREVISIBILITY ) )
		{
			return;
		}
	}
	else
	{
		for (int i = 0; i < MAXPLAYERS; i++) 
		{
			if (playeringame[i] && P_CheckSight(players[i].camera, self, SF_IGNOREVISIBILITY)) return;
		}
	}

	ACTION_JUMP(jump, false);	// [BC] This is hopefully okay.
}

//===========================================================================
//
// A_CheckSightOrRange
// Jumps if this actor is out of range of all players *and* out of sight.
// Useful for maps with many multi-actor special effects.
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_CheckSightOrRange)
{
	ACTION_PARAM_START(2);
	double range = EvalExpressionF(ParameterIndex+0, self);
	ACTION_PARAM_STATE(jump, 1);

	ACTION_SET_RESULT(false);	// Jumps should never set the result for inventory state chains!

	range = range * range * (double(FRACUNIT) * FRACUNIT);		// no need for square roots
	for (int i = 0; i < MAXPLAYERS; ++i)
	{
		if (playeringame[i])
		{
			AActor *camera = players[i].camera;

			// Check distance first, since it's cheaper than checking sight.
			double dx = self->x - camera->x;
			double dy = self->y - camera->y;
			double dz;
			fixed_t eyez = (camera->z + camera->height - (camera->height>>2));	// same eye height as P_CheckSight
			if (eyez > self->z + self->height)
			{
				dz = self->z + self->height - eyez;
			}
			else if (eyez < self->z)
			{
				dz = self->z - eyez;
			}
			else
			{
				dz = 0;
			}
			if ((dx*dx) + (dy*dy) + (dz*dz) <= range)
			{ // Within range
				return;
			}

			// Now check LOS.
			if (P_CheckSight(camera, self, SF_IGNOREVISIBILITY))
			{ // Visible
				return;
			}
		}
	}
	ACTION_JUMP(jump, false);	// [BB] This is hopefully okay.
}


//===========================================================================
//
// Inventory drop
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_DropInventory)
{
	ACTION_PARAM_START(1);
	ACTION_PARAM_CLASS(drop, 0);

	// [BC] This is handled server-side.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		if (( self->ulNetworkFlags & NETFL_CLIENTSIDEONLY ) == false )
			return;
	}

	if (drop)
	{
		AInventory * inv = self->FindInventory(drop);
		if (inv)
		{
			self->DropInventory(inv);
		}
	}
}


//===========================================================================
//
// A_SetBlend
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_SetBlend)
{
	ACTION_PARAM_START(4);
	ACTION_PARAM_COLOR(color, 0);
	ACTION_PARAM_FLOAT(alpha, 1);
	ACTION_PARAM_INT(tics, 2);
	ACTION_PARAM_COLOR(color2, 3);

	if (color == MAKEARGB(255,255,255,255)) color=0;
	if (color2 == MAKEARGB(255,255,255,255)) color2=0;
	if (!color2.a)
		color2 = color;

	new DFlashFader(color.r/255.0f, color.g/255.0f, color.b/255.0f, alpha,
					color2.r/255.0f, color2.g/255.0f, color2.b/255.0f, 0,
					(float)tics/TICRATE, self);
}


//===========================================================================
//
// A_JumpIf
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_JumpIf)
{
	ACTION_PARAM_START(2);
	ACTION_PARAM_BOOL(expression, 0);
	ACTION_PARAM_STATE(jump, 1);

	// [BC] Don't jump here in client mode.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		if (( self->ulNetworkFlags & NETFL_CLIENTSIDEONLY ) == false )
			return;
	}

	ACTION_SET_RESULT(false);	// Jumps should never set the result for inventory state chains!
	if (expression) ACTION_JUMP(jump, true);	// [BC] It's probably not good to do this client-side.
}

//===========================================================================
//
// A_KillMaster
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_KillMaster)
{
	ACTION_PARAM_START(1);
	ACTION_PARAM_NAME(damagetype, 0);

	if (self->master != NULL)
	{
		P_DamageMobj(self->master, self, self, self->master->health, damagetype, DMG_NO_ARMOR | DMG_NO_FACTOR);
	}
}

//===========================================================================
//
// A_KillChildren
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_KillChildren)
{
	ACTION_PARAM_START(1);
	ACTION_PARAM_NAME(damagetype, 0);

	TThinkerIterator<AActor> it;
	AActor *mo;

	while ( (mo = it.Next()) )
	{
		if (mo->master == self)
		{
			P_DamageMobj(mo, self, self, mo->health, damagetype, DMG_NO_ARMOR | DMG_NO_FACTOR);
		}
	}
}

//===========================================================================
//
// A_KillSiblings
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_KillSiblings)
{
	ACTION_PARAM_START(1);
	ACTION_PARAM_NAME(damagetype, 0);

	TThinkerIterator<AActor> it;
	AActor *mo;

	// [BB] This is handled server-side.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		if (( self->ulNetworkFlags & NETFL_CLIENTSIDEONLY ) == false )
			return;
	}

	while ( (mo = it.Next()) )
	{
		if (mo->master == self->master && mo != self)
		{
			P_DamageMobj(mo, self, self, mo->health, damagetype, DMG_NO_ARMOR | DMG_NO_FACTOR);
		}
	}
}

//===========================================================================
//
// A_CountdownArg
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_CountdownArg)
{
	ACTION_PARAM_START(2);
	ACTION_PARAM_INT(cnt, 0);
	ACTION_PARAM_STATE(state, 1);

	if (cnt<0 || cnt>=5) return;
	if (!self->args[cnt]--)
	{
		if (self->flags&MF_MISSILE)
		{
			P_ExplodeMissile(self, NULL, NULL);
		}
		else if (self->flags&MF_SHOOTABLE)
		{
			P_DamageMobj (self, NULL, NULL, self->health, NAME_None, DMG_FORCED);
		}
		else
		{
			// can't use "Death" as default parameter with current DECORATE parser.
			if (state == NULL) state = self->FindState(NAME_Death);
			self->SetState(state);
		}
	}

}

//============================================================================
//
// A_Burst
//
//============================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_Burst)
{
	ACTION_PARAM_START(1);
	ACTION_PARAM_CLASS(chunk, 0);

	int i, numChunks;
	AActor * mo;

	if (chunk == NULL) return;

	self->velx = self->vely = self->velz = 0;
	self->height = self->GetDefault()->height;

	// [RH] In Hexen, this creates a random number of shards (range [24,56])
	// with no relation to the size of the self shattering. I think it should
	// base the number of shards on the size of the dead thing, so bigger
	// things break up into more shards than smaller things.
	// An self with radius 20 and height 64 creates ~40 chunks.
	numChunks = MAX<int> (4, (self->radius>>FRACBITS)*(self->height>>FRACBITS)/32);
	i = (pr_burst.Random2()) % (numChunks/4);
	for (i = MAX (24, numChunks + i); i >= 0; i--)
	{
		mo = Spawn(chunk,
			self->x + (((pr_burst()-128)*self->radius)>>7),
			self->y + (((pr_burst()-128)*self->radius)>>7),
			self->z + (pr_burst()*self->height/255), ALLOW_REPLACE);

		if (mo)
		{
			mo->velz = FixedDiv(mo->z - self->z, self->height)<<2;
			mo->velx = pr_burst.Random2 () << (FRACBITS-7);
			mo->vely = pr_burst.Random2 () << (FRACBITS-7);
			mo->RenderStyle = self->RenderStyle;
			mo->alpha = self->alpha;
			mo->CopyFriendliness(self, true);
		}
	}

	// [RH] Do some stuff to make this more useful outside Hexen
	if (self->flags4 & MF4_BOSSDEATH)
	{
		CALL_ACTION(A_BossDeath, self);
	}
	A_Unblock(self, true);

	self->Destroy ();
}

//===========================================================================
//
// A_CheckFloor
// [GRB] Jumps if actor is standing on floor
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_CheckFloor)
{
	ACTION_PARAM_START(1);
	ACTION_PARAM_STATE(jump, 0);

	ACTION_SET_RESULT(false);	// Jumps should never set the result for inventory state chains!
	if (self->z <= self->floorz)
	{
		ACTION_JUMP(jump, false);	// [BC] Clients have floor information.
	}

}

//===========================================================================
//
// A_CheckCeiling
// [GZ] Totally copied on A_CheckFloor, jumps if actor touches ceiling
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_CheckCeiling)
{
	ACTION_PARAM_START(1);
	ACTION_PARAM_STATE(jump, 0);

	ACTION_SET_RESULT(false);
	if (self->z+self->height >= self->ceilingz) // Height needs to be counted
	{
		ACTION_JUMP(jump, false);	// [BB] Clients have ceiling information.
	}

}

//===========================================================================
//
// A_Stop
// resets all velocity of the actor to 0
//
//===========================================================================
DEFINE_ACTION_FUNCTION(AActor, A_Stop)
{
	self->velx = self->vely = self->velz = 0;
	if (self->player && self->player->mo == self /*&& !(self->player->cheats & CF_PREDICTING)*/)
	{
		self->player->mo->PlayIdle();
		self->player->velx = self->player->vely = 0;
	}
}

static void CheckStopped(AActor *self)
{
	if (self->player != NULL &&
		self->player->mo == self &&
		// [BB] Zandronum handles prediction differently.
		//!(self->player->cheats & CF_PREDICTING) &&
		!(self->velx | self->vely | self->velz))
	{
		self->player->mo->PlayIdle();
		self->player->velx = self->player->vely = 0;
	}
}

//===========================================================================
//
// A_Respawn
//
//===========================================================================

enum RS_Flags
{
	RSF_FOG=1,
	RSF_KEEPTARGET=2,
	RSF_TELEFRAG=4,
};

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_Respawn)
{
	ACTION_PARAM_START(1);
	ACTION_PARAM_INT(flags, 0);

	fixed_t x = self->SpawnPoint[0];
	fixed_t y = self->SpawnPoint[1];
	bool oktorespawn = false;
	sector_t *sec;

	self->flags |= MF_SOLID;
	sec = P_PointInSector (x, y);
	self->height = self->GetDefault()->height;

	if (flags & RSF_TELEFRAG)
	{
		// [KS] DIE DIE DIE DIE erm *ahem* =)
		if (P_TeleportMove (self, x, y, sec->floorplane.ZatPoint (x, y), true)) oktorespawn = true;
	}
	else
	{
		self->SetOrigin (x, y, sec->floorplane.ZatPoint (x, y));
		if (P_TestMobjLocation (self)) oktorespawn = true;
	}

	if (oktorespawn)
	{
		AActor *defs = self->GetDefault();
		self->health = defs->health;

		// [KS] Don't keep target, because it could be self if the monster committed suicide
		//      ...Actually it's better off an option, so you have better control over monster behavior.
		if (!(flags & RSF_KEEPTARGET))
		{
			self->target = NULL;
			self->LastHeard = NULL;
			self->lastenemy = NULL;
		}
		else
		{
			// Don't attack yourself (Re: "Marine targets itself after suicide")
			if (self->target == self) self->target = NULL;
			if (self->lastenemy == self) self->lastenemy = NULL;
		}

		self->flags  = (defs->flags & ~MF_FRIENDLY) | (self->flags & MF_FRIENDLY);
		self->flags2 = defs->flags2;
		self->flags3 = (defs->flags3 & ~(MF3_NOSIGHTCHECK | MF3_HUNTPLAYERS)) | (self->flags3 & (MF3_NOSIGHTCHECK | MF3_HUNTPLAYERS));
		self->flags4 = (defs->flags4 & ~MF4_NOHATEPLAYERS) | (self->flags4 & MF4_NOHATEPLAYERS);
		self->flags5 = defs->flags5;
		self->SetState (self->SpawnState);
		self->renderflags &= ~RF_INVISIBLE;

		// [BB] Clients destroy barrels in A_BarrelDestroy, so if we're the server
		// tell them to spawn the barrel. So far this is only tested for barrels and
		// perhaps needs to be rewritten to work for things not using A_BarrelDestroy.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		{
			SERVERCOMMANDS_SpawnThing( self );
			// [BB] Since the clients just spawned this actor again, be sure to remove this flag.
			self ->ulNetworkFlags &= ~NETFL_DESTROYED_ON_CLIENT;
		}

		if (flags & RSF_FOG)
		{
			AActor *pFog = Spawn<ATeleportFog> (x, y, self->z + TELEFOGHEIGHT, ALLOW_REPLACE);

			// [BB] If we're the server, tell the clients to spawn the fog.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SpawnThing( pFog );
		}
		if (self->CountsAsKill()) level.total_monsters++;
	}
	else
	{
		self->flags &= ~MF_SOLID;
	}
}


//==========================================================================
//
// A_PlayerSkinCheck
//
//==========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_PlayerSkinCheck)
{
	ACTION_PARAM_START(1);
	ACTION_PARAM_STATE(jump, 0);

	ACTION_SET_RESULT(false);	// Jumps should never set the result for inventory state chains!
	if (self->player != NULL &&
		skins[self->player->userinfo.skin].othergame)
	{
		ACTION_JUMP(jump, false);	// [BC] Clients have skin information.
	}
}

//===========================================================================
//
// A_SetGravity
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_SetGravity)
{
	ACTION_PARAM_START(1);
	ACTION_PARAM_FIXED(val, 0);
	
	self->gravity = clamp<fixed_t> (val, 0, FRACUNIT*10); 
}


// [KS] *** Start of my modifications ***

//===========================================================================
//
// A_ClearTarget
//
//===========================================================================

DEFINE_ACTION_FUNCTION(AActor, A_ClearTarget)
{
	self->target = NULL;
	self->LastHeard = NULL;
	self->lastenemy = NULL;
}

//==========================================================================
//
// A_JumpIfTargetInLOS (state label, optional fixed fov, optional bool
// projectiletarget)
//
// Jumps if the actor can see its target, or if the player has a linetarget.
// ProjectileTarget affects how projectiles are treated. If set, it will use
// the target of the projectile for seekers, and ignore the target for
// normal projectiles. If not set, it will use the missile's owner instead
// (the default).
//
//==========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_JumpIfTargetInLOS)
{
	ACTION_PARAM_START(3);
	ACTION_PARAM_STATE(jump, 0);
	ACTION_PARAM_ANGLE(fov, 1);
	ACTION_PARAM_BOOL(projtarg, 2);

	angle_t an;
	AActor *target;

	ACTION_SET_RESULT(false);	// Jumps should never set the result for inventory state chains!

	if (!self->player)
	{
		if (self->flags & MF_MISSILE && projtarg)
		{
			if (self->flags2 & MF2_SEEKERMISSILE)
				target = self->tracer;
			else
				target = NULL;
		}
		else
		{
			target = self->target;
		}

		if (!target) return; // [KS] Let's not call P_CheckSight unnecessarily in this case.

		if (!P_CheckSight (self, target, SF_IGNOREVISIBILITY))
			return;

		if (fov && (fov < ANGLE_MAX))
		{
			an = R_PointToAngle2 (self->x,
								  self->y,
								  target->x,
								  target->y)
				- self->angle;

			if (an > (fov / 2) && an < (ANGLE_MAX - (fov / 2)))
			{
				return; // [KS] Outside of FOV - return
			}

		}
	}
	else
	{
		// Does the player aim at something that can be shot?
		P_BulletSlope(self, &target);
	}

	if (!target) return;

	// [BB] Since monsters don't have targets on the client end, we need to send an update.
	// If it's not a player, also update the position. Since the client locally ignores the
	// jump, the position of the monster possibly was changed on the client by the monster
	// movement prediction.
	ACTION_JUMP(jump, CLIENTUPDATE_FRAME|( !self->player ? CLIENTUPDATE_POSITION : 0 ));
}


//==========================================================================
//
// A_JumpIfInTargetLOS (state label, optional fixed fov, optional bool
// projectiletarget)
//
//==========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_JumpIfInTargetLOS)
{
	ACTION_PARAM_START(3);
	ACTION_PARAM_STATE(jump, 0);
	ACTION_PARAM_ANGLE(fov, 1);
	ACTION_PARAM_BOOL(projtarg, 2);

	angle_t an;
	AActor *target;

	// [BB] This is handled by the server.
	if ( NETWORK_InClientModeAndActorNotClientHandled( self ) )
		return;

	ACTION_SET_RESULT(false);	// Jumps should never set the result for inventory state chains!

	if (self->flags & MF_MISSILE && projtarg)
	{
		if (self->flags2 & MF2_SEEKERMISSILE)
			target = self->tracer;
		else
			target = NULL;
	}
	else
	{
		target = self->target;
	}

	if (!target) return; // [KS] Let's not call P_CheckSight unnecessarily in this case.

	if (!P_CheckSight (target, self, SF_IGNOREVISIBILITY))
		return;

	if (fov && (fov < ANGLE_MAX))
	{
		an = R_PointToAngle2 (self->x,
							  self->y,
							  target->x,
							  target->y)
			- self->angle;

		if (an > (fov / 2) && an < (ANGLE_MAX - (fov / 2)))
		{
			return; // [KS] Outside of FOV - return
		}

	}

	ACTION_JUMP(jump,CLIENTUPDATE_FRAME);	// [BB] Since monsters don't have targets on the client end, we need to send an update.
}


//===========================================================================
//
// A_DamageMaster (int amount)
// Damages the master of this child by the specified amount. Negative values heal.
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_DamageMaster)
{
	ACTION_PARAM_START(2);
	ACTION_PARAM_INT(amount, 0);
	ACTION_PARAM_NAME(DamageType, 1);

	if (self->master != NULL)
	{
		if (amount > 0)
		{
			P_DamageMobj(self->master, self, self, amount, DamageType, DMG_NO_ARMOR);
		}
		else if (amount < 0)
		{
			amount = -amount;
			P_GiveBody(self->master, amount);
		}
	}
}

//===========================================================================
//
// A_DamageChildren (amount)
// Damages the children of this master by the specified amount. Negative values heal.
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_DamageChildren)
{
	TThinkerIterator<AActor> it;
	AActor * mo;

	ACTION_PARAM_START(2);
	ACTION_PARAM_INT(amount, 0);
	ACTION_PARAM_NAME(DamageType, 1);

	while ( (mo = it.Next()) )
	{
		if (mo->master == self)
		{
			if (amount > 0)
			{
				P_DamageMobj(mo, self, self, amount, DamageType, DMG_NO_ARMOR);
			}
			else if (amount < 0)
			{
				amount = -amount;
				P_GiveBody(mo, amount);
			}
		}
	}
}

// [KS] *** End of my modifications ***

//===========================================================================
//
// A_DamageSiblings (amount)
// Damages the siblings of this master by the specified amount. Negative values heal.
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_DamageSiblings)
{
	TThinkerIterator<AActor> it;
	AActor * mo;

	ACTION_PARAM_START(2);
	ACTION_PARAM_INT(amount, 0);
	ACTION_PARAM_NAME(DamageType, 1);

	while ( (mo = it.Next()) )
	{
		if (mo->master == self->master && mo != self)
		{
			if (amount > 0)
			{
				P_DamageMobj(mo, self, self, amount, DamageType, DMG_NO_ARMOR);
			}
			else if (amount < 0)
			{
				amount = -amount;
				P_GiveBody(mo, amount);
			}
		}
	}
}


//===========================================================================
//
// Modified code pointer from Skulltag
//
//===========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_CheckForReload)
{
	if ( self->player == NULL || self->player->ReadyWeapon == NULL )
		return;

	ACTION_PARAM_START(2);
	ACTION_PARAM_INT(count, 0);
	ACTION_PARAM_STATE(jump, 1);
	ACTION_PARAM_BOOL(dontincrement, 2)

	if (count <= 0) return;

	AWeapon *weapon = self->player->ReadyWeapon;

	int ReloadCounter = weapon->ReloadCounter;
	if(!dontincrement || ReloadCounter != 0)
		ReloadCounter = (weapon->ReloadCounter+1) % count;
	else // 0 % 1 = 1?  So how do we check if the weapon was never fired?  We should only do this when we're not incrementing the counter though.
		ReloadCounter = 1;

	// If we have not made our last shot...
	if (ReloadCounter != 0)
	{
		// Go back to the refire frames, instead of continuing on to the reload frames.
		ACTION_JUMP(jump, false);	// [BB] Clients should know the ReloadCounter value.
	}
	else
	{
		// We need to reload. However, don't reload if we're out of ammo.
		weapon->CheckAmmo( false, false );
	}

	if(!dontincrement)
		weapon->ReloadCounter = ReloadCounter;
}

//===========================================================================
//
// Resets the counter for the above function
//
//===========================================================================

DEFINE_ACTION_FUNCTION(AActor, A_ResetReloadCounter)
{
	if ( self->player == NULL || self->player->ReadyWeapon == NULL )
		return;

	AWeapon *weapon = self->player->ReadyWeapon;
	weapon->ReloadCounter = 0;
}

//===========================================================================
//
// A_ChangeFlag
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_ChangeFlag)
{
	ACTION_PARAM_START(2);
	ACTION_PARAM_STRING(flagname, 0);
	ACTION_PARAM_BOOL(expression, 1);

	const char *dot = strchr (flagname, '.');
	FFlagDef *fd;
	const PClass *cls = self->GetClass();

	if (dot != NULL)
	{
		FString part1(flagname, dot-flagname);
		fd = FindFlag (cls, part1, dot+1);
	}
	else
	{
		fd = FindFlag (cls, flagname, NULL);
	}

	if (fd != NULL)
	{
		bool kill_before, kill_after;
		INTBOOL item_before, item_after;

		kill_before = self->CountsAsKill();
		item_before = self->flags & MF_COUNTITEM;

		if (fd->structoffset == -1)
		{
			HandleDeprecatedFlags(self, cls->ActorInfo, expression, fd->flagbit);
		}
		else
		{
			// [BB] The server handles the flag change.
			if ( NETWORK_InClientMode( ) )
				return;

			DWORD *flagp = (DWORD*) (((char*)self) + fd->structoffset);

			// If these 2 flags get changed we need to update the blockmap and sector links.
			bool linkchange = flagp == &self->flags && (fd->flagbit == MF_NOBLOCKMAP || fd->flagbit == MF_NOSECTOR);

			if (linkchange) self->UnlinkFromWorld();
			if (expression)
			{
				*flagp |= fd->flagbit;
			}
			else
			{
				*flagp &= ~fd->flagbit;
			}
			if (linkchange) self->LinkToWorld();

			// [BB] Let the clients know about the flag change.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER ) {
				ULONG ulFlagSet = 0;
				if ( flagp == &self->flags )
					ulFlagSet = FLAGSET_FLAGS;
				else if ( flagp == &self->flags2 )
					ulFlagSet = FLAGSET_FLAGS2;
				else if ( flagp == &self->flags3 )
					ulFlagSet = FLAGSET_FLAGS3;
				else if ( flagp == &self->flags4 )
					ulFlagSet = FLAGSET_FLAGS4;
				else if ( flagp == &self->flags5 )
					ulFlagSet = FLAGSET_FLAGS5;

				SERVERCOMMANDS_SetThingFlags( self, ulFlagSet );
			}
		}
		kill_after = self->CountsAsKill();
		item_after = self->flags & MF_COUNTITEM;
		// Was this monster previously worth a kill but no longer is?
		// Or vice versa?
		if (kill_before != kill_after)
		{
			if (kill_after)
			{ // It counts as a kill now.
				level.total_monsters++;
			}
			else
			{ // It no longer counts as a kill.
				level.total_monsters--;
			}

			// [BB] If we're the server, tell clients the new number of total monsters.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetMapNumTotalMonsters( );
		}
		// same for items
		if (item_before != item_after)
		{
			if (item_after)
			{ // It counts as an item now.
				level.total_items++;
			}
			else
			{ // It no longer counts as an item
				level.total_items--;
			}

			// [BB] If we're the server, tell clients the new number of total items.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetMapNumTotalItems( );
		}
	}
	else
	{
		Printf("Unknown flag '%s' in '%s'\n", flagname, cls->TypeName.GetChars());
	}
}

//===========================================================================
//
// A_RemoveMaster
//
//===========================================================================
DEFINE_ACTION_FUNCTION(AActor, A_RemoveMaster)
{
   if (self->master != NULL)
   {
      P_RemoveThing(self->master);
   }
}

//===========================================================================
//
// A_RemoveChildren
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_RemoveChildren)
{
   TThinkerIterator<AActor> it;
   AActor * mo;
   ACTION_PARAM_START(1);
   ACTION_PARAM_BOOL(removeall,0);

   while ( (mo = it.Next()) )
   {
      if ( ( mo->master == self ) && ( ( mo->health <= 0 ) || removeall) )
      {
		P_RemoveThing(mo);
      }
   }
}

//===========================================================================
// 
// A_RemoveSiblings
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_RemoveSiblings)
{
   TThinkerIterator<AActor> it;
   AActor * mo;
   ACTION_PARAM_START(1);
   ACTION_PARAM_BOOL(removeall,0);

   while ( (mo = it.Next()) )
   {
      if ( ( mo->master == self->master ) && ( mo != self ) && ( ( mo->health <= 0 ) || removeall) )
      {
		P_RemoveThing(mo);
      }
   }
}

//===========================================================================
//
// A_RaiseMaster
//
//===========================================================================
DEFINE_ACTION_FUNCTION(AActor, A_RaiseMaster)
{
   if (self->master != NULL)
   {
      P_Thing_Raise(self->master);
   }
}

//===========================================================================
//
// A_RaiseChildren
//
//===========================================================================
DEFINE_ACTION_FUNCTION(AActor, A_RaiseChildren)
{
   TThinkerIterator<AActor> it;
   AActor * mo;

   while ((mo = it.Next()))
   {
      if ( mo->master == self )
      {
		P_Thing_Raise(mo);
      }
   }
}

//===========================================================================
//
// A_RaiseSiblings
//
//===========================================================================
DEFINE_ACTION_FUNCTION(AActor, A_RaiseSiblings)
{
   TThinkerIterator<AActor> it;
   AActor * mo;

   while ( (mo = it.Next()) )
   {
      if ( ( mo->master == self->master ) && ( mo != self ) )
      {
		P_Thing_Raise(mo);
      }
   }
}

//===========================================================================
// 
// [Dusk] A_FaceConsolePlayer
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS (AActor, A_FaceConsolePlayer) {
	ACTION_PARAM_START (1);
	ACTION_PARAM_ANGLE (MaxTurnAngle, 0);

	angle_t		Angle;
	angle_t		DeltaAngle;
	AActor		*pConsolePlayer;

	// Always watch the consoleplayer.
	pConsolePlayer = players[consoleplayer].mo;
	if (( playeringame[consoleplayer] == false ) || ( pConsolePlayer == NULL ))
		return;

	// Find the angle between the actor and the console player.
	Angle = R_PointToAngle2( self->x, self->y, pConsolePlayer->x, pConsolePlayer->y );
	DeltaAngle = Angle - self->angle;

	if (( MaxTurnAngle == 0 ) || ( DeltaAngle < MaxTurnAngle ) || ( DeltaAngle > (unsigned)-MaxTurnAngle ))
		self->angle = Angle;
	else if ( DeltaAngle < ANG180 )
		self->angle += MaxTurnAngle;
	else
		self->angle -= MaxTurnAngle;
}

//===========================================================================
//
// A_MonsterRefire
//
// Keep firing unless target got out of sight
//
//===========================================================================
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_MonsterRefire)
{
	ACTION_PARAM_START(2);
	ACTION_PARAM_INT(prob, 0);
	ACTION_PARAM_STATE(jump, 1);

	// [BB] This is handled by the server.
	if ( NETWORK_InClientModeAndActorNotClientHandled( self ) )
		return;

	ACTION_SET_RESULT(false);	// Jumps should never set the result for inventory state chains!
	A_FaceTarget (self);

	if (pr_monsterrefire() < prob)
		return;

	if (!self->target
		|| P_HitFriend (self)
		|| self->target->health <= 0
		|| !P_CheckSight (self, self->target, SF_SEEPASTBLOCKEVERYTHING|SF_SEEPASTSHOOTABLELINES) )
	{
		ACTION_JUMP(jump,CLIENTUPDATE_FRAME);	// [BB] Since monsters don't have targets on the client end, we need to send an update.
	}
}

//===========================================================================
//
// A_SetAngle
//
// Set actor's angle (in degrees).
//
//===========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_SetAngle)
{
	ACTION_PARAM_START(1);
	ACTION_PARAM_ANGLE(angle, 0);
	self->angle = angle;
}

//===========================================================================
//
// A_SetPitch
//
// Set actor's pitch (in degrees).
//
//===========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_SetPitch)
{
	ACTION_PARAM_START(1);
	ACTION_PARAM_ANGLE(pitch, 0);
	self->pitch = pitch;
}

//===========================================================================
//
// A_ScaleVelocity
//
// Scale actor's velocity.
//
//===========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_ScaleVelocity)
{
	ACTION_PARAM_START(1);
	ACTION_PARAM_FIXED(scale, 0);

	INTBOOL was_moving = self->velx | self->vely | self->velz;

	self->velx = FixedMul(self->velx, scale);
	self->vely = FixedMul(self->vely, scale);
	self->velz = FixedMul(self->velz, scale);

	// If the actor was previously moving but now is not, and is a player,
	// update its player variables. (See A_Stop.)
	if (was_moving)
	{
		CheckStopped(self);
	}
}

//===========================================================================
//
// A_ChangeVelocity
//
//===========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_ChangeVelocity)
{
	ACTION_PARAM_START(4);
	ACTION_PARAM_FIXED(x, 0);
	ACTION_PARAM_FIXED(y, 1);
	ACTION_PARAM_FIXED(z, 2);
	ACTION_PARAM_INT(flags, 3);

	INTBOOL was_moving = self->velx | self->vely | self->velz;

	fixed_t vx = x, vy = y, vz = z;
	fixed_t sina = finesine[self->angle >> ANGLETOFINESHIFT];
	fixed_t cosa = finecosine[self->angle >> ANGLETOFINESHIFT];

	if (flags & 1)	// relative axes - make x, y relative to actor's current angle
	{
		vx = DMulScale16(x, cosa, -y, sina);
		vy = DMulScale16(x, sina,  y, cosa);
	}
	if (flags & 2)	// discard old velocity - replace old velocity with new velocity
	{
		self->velx = vx;
		self->vely = vy;
		self->velz = vz;
	}
	else	// add new velocity to old velocity
	{
		self->velx += vx;
		self->vely += vy;
		self->velz += vz;
	}

	if (was_moving)
	{
		CheckStopped(self);
	}
}

//===========================================================================
//
// A_SetArg
//
//===========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_SetArg)
{
	ACTION_PARAM_START(2);
	ACTION_PARAM_INT(pos, 0);
	ACTION_PARAM_INT(value, 1);	

	// Set the value of the specified arg
	if ((size_t)pos < countof(self->args))
	{
		self->args[pos] = value;
	}
}

//===========================================================================
//
// A_SetSpecial
//
//===========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_SetSpecial)
{
	ACTION_PARAM_START(6);
	ACTION_PARAM_INT(spec, 0);
	ACTION_PARAM_INT(arg0, 1);	
	ACTION_PARAM_INT(arg1, 2);	
	ACTION_PARAM_INT(arg2, 3);	
	ACTION_PARAM_INT(arg3, 4);	
	ACTION_PARAM_INT(arg4, 5);	
	
	self->special = spec;
	self->args[0] = arg0;
	self->args[1] = arg1;
	self->args[2] = arg2;
	self->args[3] = arg3;
	self->args[4] = arg4;
}

//===========================================================================
//
// A_SetUserVar
//
//===========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_SetUserVar)
{
	ACTION_PARAM_START(2);
	ACTION_PARAM_NAME(varname, 0);
	ACTION_PARAM_INT(value, 1);	

	PSymbol *sym = self->GetClass()->Symbols.FindSymbol(varname, true);
	PSymbolVariable *var;

	if (sym == NULL || sym->SymbolType != SYM_Variable ||
		!(var = static_cast<PSymbolVariable *>(sym))->bUserVar ||
		var->ValueType.Type != VAL_Int)
	{
		Printf("%s is not a user variable in class %s\n", varname.GetChars(),
			self->GetClass()->TypeName.GetChars());
		return;
	}
	// Set the value of the specified user variable.
	*(int *)(reinterpret_cast<BYTE *>(self) + var->offset) = value;
}

//===========================================================================
//
// A_SetUserArray
//
//===========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_SetUserArray)
{
	ACTION_PARAM_START(3);
	ACTION_PARAM_NAME(varname, 0);
	ACTION_PARAM_INT(pos, 1);
	ACTION_PARAM_INT(value, 2);

	PSymbol *sym = self->GetClass()->Symbols.FindSymbol(varname, true);
	PSymbolVariable *var;

	if (sym == NULL || sym->SymbolType != SYM_Variable ||
		!(var = static_cast<PSymbolVariable *>(sym))->bUserVar ||
		var->ValueType.Type != VAL_Array || var->ValueType.BaseType != VAL_Int)
	{
		Printf("%s is not a user array in class %s\n", varname.GetChars(),
			self->GetClass()->TypeName.GetChars());
		return;
	}
	if (pos < 0 || pos >= var->ValueType.size)
	{
		Printf("%d is out of bounds in array %s in class %s\n", pos, varname.GetChars(),
			self->GetClass()->TypeName.GetChars());
		return;
	}
	// Set the value of the specified user array at index pos.
	((int *)(reinterpret_cast<BYTE *>(self) + var->offset))[pos] = value;
}

//===========================================================================
//
// A_Turn
//
//===========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_Turn)
{
	ACTION_PARAM_START(1);
	ACTION_PARAM_ANGLE(angle, 0);
	self->angle += angle;
}

//===========================================================================
//
// A_Quake
//
//===========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_Quake)
{
	ACTION_PARAM_START(5);
	ACTION_PARAM_INT(intensity, 0);
	ACTION_PARAM_INT(duration, 1);
	ACTION_PARAM_INT(damrad, 2);
	ACTION_PARAM_INT(tremrad, 3);
	ACTION_PARAM_SOUND(sound, 4);
	P_StartQuake(self, 0, intensity, duration, damrad, tremrad, sound);
}

//===========================================================================
//
// A_Weave
//
//===========================================================================

void A_Weave(AActor *self, int xyspeed, int zspeed, fixed_t xydist, fixed_t zdist)
{
	fixed_t newX, newY;
	int weaveXY, weaveZ;
	int angle;
	fixed_t dist;

	weaveXY = self->WeaveIndexXY & 63;
	weaveZ = self->WeaveIndexZ & 63;
	angle = (self->angle + ANG90) >> ANGLETOFINESHIFT;

	if (xydist != 0 && xyspeed != 0)
	{
		dist = FixedMul(FloatBobOffsets[weaveXY], xydist);
		newX = self->x - FixedMul (finecosine[angle], dist);
		newY = self->y - FixedMul (finesine[angle], dist);
		weaveXY = (weaveXY + xyspeed) & 63;
		dist = FixedMul(FloatBobOffsets[weaveXY], xydist);
		newX += FixedMul (finecosine[angle], dist);
		newY += FixedMul (finesine[angle], dist);
		P_TryMove (self, newX, newY, true);
		self->WeaveIndexXY = weaveXY;
	}

	if (zdist != 0 && zspeed != 0)
	{
		self->z -= FixedMul(FloatBobOffsets[weaveZ], zdist);
		weaveZ = (weaveZ + zspeed) & 63;
		self->z += FixedMul(FloatBobOffsets[weaveZ], zdist);
		self->WeaveIndexZ = weaveZ;
	}
}

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_Weave)
{
	ACTION_PARAM_START(4);
	ACTION_PARAM_INT(xspeed, 0);
	ACTION_PARAM_INT(yspeed, 1);
	ACTION_PARAM_FIXED(xdist, 2);
	ACTION_PARAM_FIXED(ydist, 3);
	A_Weave(self, xspeed, yspeed, xdist, ydist);
}




//===========================================================================
//
// A_LineEffect
//
// This allows linedef effects to be activated inside deh frames.
//
//===========================================================================


void P_TranslateLineDef (line_t *ld, maplinedef_t *mld);
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_LineEffect)
{
	ACTION_PARAM_START(2);
	ACTION_PARAM_INT(special, 0);
	ACTION_PARAM_INT(tag, 1);

	line_t junk; maplinedef_t oldjunk;
	bool res = false;
	if (!(self->flags6 & MF6_LINEDONE))						// Unless already used up
	{
		if ((oldjunk.special = special))					// Linedef type
		{
			oldjunk.tag = tag;								// Sector tag for linedef
			P_TranslateLineDef(&junk, &oldjunk);			// Turn into native type
			res = !!LineSpecials[junk.special](NULL, self, false, junk.args[0], 
				junk.args[1], junk.args[2], junk.args[3], junk.args[4]); 
			if (res && !(junk.flags & ML_REPEAT_SPECIAL))	// If only once,
				self->flags6 |= MF6_LINEDONE;				// no more for this thing
		}
	}
	ACTION_SET_RESULT(res);
}
