/*
** p_things.cpp
** ACS-accessible thing utilities
**
**---------------------------------------------------------------------------
** Copyright 1998-2007 Randy Heit
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

#include "doomtype.h"
#include "p_local.h"
#include "info.h"
#include "s_sound.h"
#include "tables.h"
#include "doomstat.h"
#include "m_random.h"
#include "c_console.h"
#include "c_dispatch.h"
#include "a_sharedglobal.h"
#include "gi.h"
#include "templates.h"
#include "g_level.h"
// [BC] New #includes.
#include "a_doomglobal.h"
#include "sv_commands.h"
#include "team.h"
#include "a_keys.h"
#include "invasion.h"

// List of spawnable things for the Thing_Spawn and Thing_Projectile specials.
const PClass *SpawnableThings[MAX_SPAWNABLES];

static FRandom pr_leadtarget ("LeadTarget");

bool P_Thing_Spawn (int tid, AActor *source, int type, angle_t angle, bool fog, int newtid)
{
	int rtn = 0;
	const PClass *kind;
	AActor *spot, *mobj;
	FActorIterator iterator (tid);

	if (type >= MAX_SPAWNABLES)
		return false;

	if ( (kind = SpawnableThings[type]) == NULL)
		return false;

	// Handle decorate replacements.
	kind = kind->ActorInfo->GetReplacement()->Class;

	if ((GetDefaultByType (kind)->flags3 & MF3_ISMONSTER) && 
		((dmflags & DF_NO_MONSTERS) || (level.flags2 & LEVEL2_NOMONSTERS)))
		return false;

	if (tid == 0)
	{
		spot = source;
	}
	else
	{
		spot = iterator.Next();
	}
	while (spot != NULL)
	{
		mobj = Spawn (kind, spot->x, spot->y, spot->z, ALLOW_REPLACE);

		if (mobj != NULL)
		{
			DWORD oldFlags2 = mobj->flags2;
			mobj->flags2 |= MF2_PASSMOBJ;
			// [BC] Potentially spawn this thing even if it's going to be blocked.
			bool	bSpawn = true;

			// [BC] Don't spawn it if it doesn't have a good place to spawn.
			if ( P_TestMobjLocation( mobj ) == false )
				bSpawn = false;

			// [BC] However, SKULLTAG FLAGS/SKULLS MUST BE RESPAWNED!
			for ( ULONG i = 0; i < teams.Size( ); i++ )
			{
				if (( mobj->GetClass( ) == TEAM_GetItem( i )))
				{
					bSpawn = true;
					break;
				}
			}

			if ( bSpawn )
			{
				rtn++;
				mobj->angle = (angle != ANGLE_MAX ? angle : spot->angle);
				if (fog)
				{
					// [BC]
					AActor	*pFog;

					pFog = Spawn<ATeleportFog> (spot->x, spot->y, spot->z + TELEFOGHEIGHT, ALLOW_REPLACE);

					// [BC] If we're the server, tell clients to spawn the thing.
					if (( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( pFog ))
						SERVERCOMMANDS_SpawnThing( pFog );
				}

				// [BC] Respawned keys in Skulltag CANNOT be dropped items.
				if (( mobj->flags & MF_SPECIAL ) && ( mobj->GetClass( )->IsDescendantOf( RUNTIME_CLASS( AKey )) == false ))
					mobj->flags |= MF_DROPPED;	// Don't respawn
				mobj->tid = newtid;
				mobj->AddToHash ();
				mobj->flags2 = oldFlags2;

				// [BC] Spawn the actor to clients.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				{
					SERVERCOMMANDS_SpawnThing( mobj );

					// Check and see if it's important that the client know the angle of the object.
					if ( mobj->angle != 0 )
						SERVERCOMMANDS_SetThingAngle( mobj );
				}
			}
			else
			{
				// If this is a monster, subtract it from the total monster
				// count, because it already added to it during spawning.
				if (mobj->CountsAsKill())
				{
					level.total_monsters--;
				}
				// Same, for items
				if (mobj->flags & MF_COUNTITEM)
				{
					level.total_items--;
				}
				mobj->Destroy ();
			}
		}
		spot = iterator.Next();
	}

	return rtn != 0;
}

// [BC] Added
// [RH] Fixed

bool P_MoveThing(AActor *source, fixed_t x, fixed_t y, fixed_t z, bool fog)
{
	fixed_t oldx, oldy, oldz;
	// [BC]
	AActor	*pFog;

	oldx = source->x;
	oldy = source->y;
	oldz = source->z;

	source->SetOrigin (x, y, z);
	if (P_TestMobjLocation (source))
	{
		if (fog)
		{
			pFog = Spawn<ATeleportFog> (x, y, z + TELEFOGHEIGHT, ALLOW_REPLACE);

			// [BC] If we're the server, tell clients to spawn the fog.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SpawnThing( pFog );

			pFog = Spawn<ATeleportFog> (oldx, oldy, oldz + TELEFOGHEIGHT, ALLOW_REPLACE);

			// [BC] If we're the server, tell clients to spawn the fog.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SpawnThing( pFog );
		}
		source->PrevX=x;
		source->PrevY=y;
		source->PrevZ=z;

		ULONG ulFlags = 0;
		if ( oldx != source->x )
			ulFlags |= CM_X;
		if ( oldy != source->y )
			ulFlags |= CM_Y;
		if ( oldz != source->z )
			ulFlags |= CM_Z;

		// [BC] If we're the server, tell clients to move the object.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_MoveThing( source, ulFlags );

		return true;
	}
	else
	{
		source->SetOrigin (oldx, oldy, oldz);
		return false;
	}
}

bool P_Thing_Move (int tid, AActor *source, int mapspot, bool fog)
{
	AActor *target;

	if (tid != 0)
	{
		FActorIterator iterator1(tid);
		source = iterator1.Next();
	}
	FActorIterator iterator2 (mapspot);
	target = iterator2.Next ();

	if (source != NULL && target != NULL)
	{
		return P_MoveThing(source, target->x, target->y, target->z, fog);
	}
	return false;
}

bool P_Thing_Projectile (int tid, AActor *source, int type, const char *type_name, angle_t angle,
	fixed_t speed, fixed_t vspeed, int dest, AActor *forcedest, int gravity, int newtid,
	bool leadTarget)
{
	int rtn = 0;
	const PClass *kind;
	AActor *spot, *mobj, *targ = forcedest;
	FActorIterator iterator (tid);
	double fspeed = speed;
	int defflags3;
	// [BC]
	bool	bMissileExplode;

	if (type_name == NULL)
	{
		if (type >= MAX_SPAWNABLES)
			return false;

		if ((kind = SpawnableThings[type]) == NULL)
			return false;
	}
	else
	{
		if ((kind = PClass::FindClass(type_name)) == NULL || kind->ActorInfo == NULL)
			return false;
	}


	// Handle decorate replacements.
	kind = kind->ActorInfo->GetReplacement()->Class;

	defflags3 = GetDefaultByType (kind)->flags3;
	if ((defflags3 & MF3_ISMONSTER) && 
		((dmflags & DF_NO_MONSTERS) || (level.flags2 & LEVEL2_NOMONSTERS)))
		return false;

	if (tid == 0)
	{
		spot = source;
	}
	else
	{
		spot = iterator.Next();
	}
	while (spot != NULL)
	{
		FActorIterator tit (dest);

		if (dest == 0 || (targ = tit.Next()))
		{
			do
			{
				fixed_t z = spot->z;
				if (defflags3 & MF3_FLOORHUGGER)
				{
					z = ONFLOORZ;
				}
				else if (defflags3 & MF3_CEILINGHUGGER)
				{
					z = ONCEILINGZ;
				}
				else if (z != ONFLOORZ)
				{
					z -= spot->floorclip;
				}
				mobj = Spawn (kind, spot->x, spot->y, z, ALLOW_REPLACE);

				if (mobj)
				{
					mobj->tid = newtid;
					mobj->AddToHash ();
					P_PlaySpawnSound(mobj, spot);
					if (gravity)
					{
						mobj->flags &= ~MF_NOGRAVITY;
						if (!(mobj->flags3 & MF3_ISMONSTER) && gravity == 1)
						{
							mobj->gravity = FRACUNIT/8;
						}
					}
					else
					{
						mobj->flags |= MF_NOGRAVITY;
					}
					mobj->target = spot;

					if (targ != NULL)
					{
						fixed_t spot[3] = { targ->x, targ->y, targ->z+targ->height/2 };
						FVector3 aim(float(spot[0] - mobj->x), float(spot[1] - mobj->y), float(spot[2] - mobj->z));

						if (leadTarget && speed > 0 && (targ->momx | targ->momy | targ->momz))
						{
							// Aiming at the target's position some time in the future
							// is basically just an application of the law of sines:
							//     a/sin(A) = b/sin(B)
							// Thanks to all those on the notgod phorum for helping me
							// with the math. I don't think I would have thought of using
							// trig alone had I been left to solve it by myself.

							FVector3 tvel(targ->momx, targ->momy, targ->momz);
							if (!(targ->flags & MF_NOGRAVITY) && targ->waterlevel < 3)
							{ // If the target is subject to gravity and not underwater,
							  // assume that it isn't moving vertically. Thanks to gravity,
							  // even if we did consider the vertical component of the target's
							  // velocity, we would still miss more often than not.
								tvel.Z = 0.0;
								if ((targ->momx | targ->momy) == 0)
								{
									goto nolead;
								}
							}
							double dist = aim.Length();
							double targspeed = tvel.Length();
							double ydotx = -aim | tvel;
							double a = acos (clamp (ydotx / targspeed / dist, -1.0, 1.0));
							double multiplier = double(pr_leadtarget.Random2())*0.1/255+1.1;
							double sinb = -clamp (targspeed*multiplier * sin(a) / fspeed, -1.0, 1.0);

							// Use the cross product of two of the triangle's sides to get a
							// rotation vector.
							FVector3 rv(tvel ^ aim);
							// The vector must be normalized.
							rv.MakeUnit();
							// Now combine the rotation vector with angle b to get a rotation matrix.
							FMatrix3x3 rm(rv, cos(asin(sinb)), sinb);
							// And multiply the original aim vector with the matrix to get a
							// new aim vector that leads the target.
							FVector3 aimvec = rm * aim;
							// And make the projectile follow that vector at the desired speed.
							double aimscale = fspeed / dist;
							mobj->momx = fixed_t (aimvec[0] * aimscale);
							mobj->momy = fixed_t (aimvec[1] * aimscale);
							mobj->momz = fixed_t (aimvec[2] * aimscale);
							mobj->angle = R_PointToAngle2 (0, 0, mobj->momx, mobj->momy);
						}
						else
						{
nolead:						mobj->angle = R_PointToAngle2 (mobj->x, mobj->y, targ->x, targ->y);
							aim.Resize (fspeed);
							mobj->momx = fixed_t(aim[0]);
							mobj->momy = fixed_t(aim[1]);
							mobj->momz = fixed_t(aim[2]);
						}
						if (mobj->flags2 & MF2_SEEKERMISSILE)
						{
							mobj->tracer = targ;
						}
					}
					else
					{
						mobj->angle = angle;
						mobj->momx = FixedMul (speed, finecosine[angle>>ANGLETOFINESHIFT]);
						mobj->momy = FixedMul (speed, finesine[angle>>ANGLETOFINESHIFT]);
						mobj->momz = vspeed;
					}
					// Set the missile's speed to reflect the speed it was spawned at.
					if (mobj->flags & MF_MISSILE)
					{
						mobj->Speed = fixed_t (sqrt (double(mobj->momx)*mobj->momx + double(mobj->momy)*mobj->momy + double(mobj->momz)*mobj->momz));
					}
					// Hugger missiles don't have any vertical velocity
					if (mobj->flags3 & (MF3_FLOORHUGGER|MF3_CEILINGHUGGER))
					{
						mobj->momz = 0;
					}
					if (mobj->flags & MF_SPECIAL)
					{
						mobj->flags |= MF_DROPPED;
					}
					bMissileExplode = false;
					if (mobj->flags & MF_MISSILE)
					{
						if (P_CheckMissileSpawn (mobj))
						{
							rtn = true;
						}
						else
							bMissileExplode = true;
					}
					else if (!P_TestMobjLocation (mobj))
					{
						// If this is a monster, subtract it from the total monster
						// count, because it already added to it during spawning.
						if (mobj->CountsAsKill())
						{
							level.total_monsters--;
						}
						// Same, for items
						if (mobj->flags & MF_COUNTITEM)
						{
							level.total_items--;
						}
						mobj->Destroy ();
					}
					else
					{
						// It spawned fine.
						rtn = 1;
					}

					// [BC] Spawn this actor to clients. It must be spawned as a missile because
					// it can potentially have velocity, etc. 
					if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					{
						SERVERCOMMANDS_SpawnMissile( mobj );

						// Determine which flags we need to update.
						if ( mobj->flags != mobj->GetDefault( )->flags )
							SERVERCOMMANDS_SetThingFlags( mobj, FLAGSET_FLAGS );
						if ( mobj->flags2 != mobj->GetDefault( )->flags2 )
							SERVERCOMMANDS_SetThingFlags( mobj, FLAGSET_FLAGS2 );
						if ( mobj->flags3 != mobj->GetDefault( )->flags3 )
							SERVERCOMMANDS_SetThingFlags( mobj, FLAGSET_FLAGS3 );
						if ( mobj->flags4 != mobj->GetDefault( )->flags4 )
							SERVERCOMMANDS_SetThingFlags( mobj, FLAGSET_FLAGS4 );
						if ( mobj->flags5 != mobj->GetDefault( )->flags5 )
							SERVERCOMMANDS_SetThingFlags( mobj, FLAGSET_FLAGS5 );
						if ( mobj->ulSTFlags != mobj->GetDefault( )->ulSTFlags )
							SERVERCOMMANDS_SetThingFlags( mobj, FLAGSET_FLAGSST );
						// [BB] If necessary, also adjust gravity.
						if ( mobj->gravity != mobj->GetDefault( )->gravity )
							SERVERCOMMANDS_SetThingGravity( mobj );

						// For missiles that exploded when P_CheckMissileSpawn() was
						// called, we need to tell clients to explode the missile since
						// it wasn't actually spawned at that point.
						if ( bMissileExplode )
							SERVERCOMMANDS_MissileExplode( mobj, NULL );
					}
				}
			} while (dest != 0 && (targ = tit.Next()));
		}
		spot = iterator.Next();
	}

	return rtn != 0;
}

int P_Thing_Damage (int tid, AActor *whofor0, int amount, FName type)
{
	FActorIterator iterator (tid);
	int count = 0;
	AActor *actor;

	actor = (tid == 0 ? whofor0 : iterator.Next());
	while (actor)
	{
		AActor *next = tid == 0 ? NULL : iterator.Next ();
		if (actor->flags & MF_SHOOTABLE)
		{
			if (amount > 0)
			{
				P_DamageMobj (actor, NULL, whofor0, amount, type);
			}
			else if (actor->health < actor->GetDefault()->health)
			{
				actor->health -= amount;
				if (actor->health > actor->GetDefault()->health)
				{
					actor->health = actor->GetDefault()->health;
				}
				if (actor->player != NULL)
				{
					actor->player->health = actor->health;
				}
			}
			count++;
		}
		actor = next;
	}
	return count;
}

void P_RemoveThing(AActor * actor)
{
	// Don't remove live players.
	if (actor->player == NULL || actor != actor->player->mo)
	{
		// [BC] DESTROY!
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_DestroyThing( actor );

		// be friendly to the level statistics. ;)
		// [BB] Added client update.
		if (actor->CountsAsKill() && actor->health > 0)
		{
			level.total_monsters--;

			// [BB] Since a monster was removed, we also need to correct the number of monsters in invasion mode.
			INVASION_UpdateMonsterCount( actor, true );

			// [BB] Inform the clients.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetMapNumTotalMonsters( );
		}
		// [BB] Added client update.
		if (actor->flags&MF_COUNTITEM)
		{
			level.total_items--;

			// [BB] Inform the clients.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetMapNumTotalItems( );
		}

		// [BB] Only destroy the actor if it's not needed for a map reset. Otherwise just hide it.
		actor->HideOrDestroyIfSafe ();
	}
}

void P_Thing_SetVelocity(AActor *actor, fixed_t vx, fixed_t vy, fixed_t vz, bool add, bool setbob)
{
	if (actor != NULL)
	{
		if (!add)
		{
			actor->momx = actor->momy = actor->momz = 0;
			if (actor->player != NULL) actor->player->momx = actor->player->momy = 0;
		}
		actor->momx += vx;
		actor->momy += vy;
		actor->momz += vz;
		if (setbob && actor->player != NULL)
		{
			actor->player->momx += vx;
			actor->player->momy += vy;
		}
		// [Dusk] Update momentum
		SERVER_UpdateThingMomentum( actor, true );
	}
}


CCMD (dumpspawnables)
{
	int i;

	for (i = 0; i < MAX_SPAWNABLES; i++)
	{
		if (SpawnableThings[i] != NULL)
		{
			Printf ("%d %s\n", i, SpawnableThings[i]->TypeName.GetChars());
		}
	}
}

