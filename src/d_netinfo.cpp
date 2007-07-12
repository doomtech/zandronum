/*
** d_netinfo.cpp
** Manages transport of user and "server" cvars across a network
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

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "doomtype.h"
#include "doomdef.h"
#include "doomstat.h"
#include "d_netinf.h"
#include "d_net.h"
#include "d_protocol.h"
#include "c_dispatch.h"
#include "v_palette.h"
#include "v_video.h"
#include "i_system.h"
#include "r_draw.h"
#include "r_state.h"
#include "sbar.h"
#include "gi.h"
#include "m_random.h"
// [BC] New #includes.
#include "network.h"
#include "cl_demo.h"
#include "cl_main.h"
#include "deathmatch.h"
#include "team.h"

static FRandom pr_pickteam ("PickRandomTeam");

extern bool st_firsttime;
EXTERN_CVAR (Bool, teamplay)

CVAR (Float,	autoaim,				5000.f,		CVAR_USERINFO | CVAR_ARCHIVE);
CVAR (String,	name,					"Player",	CVAR_USERINFO | CVAR_ARCHIVE);
CVAR (Color,	color,					0x40cf00,	CVAR_USERINFO | CVAR_ARCHIVE);
CVAR (String,	skin,					"base",		CVAR_USERINFO | CVAR_ARCHIVE);
// [BC] "team" is no longer a cvar.
CVAR (String,	gender,					"male",		CVAR_USERINFO | CVAR_ARCHIVE);
// [BC] Changed "neverswitchonpickup" to allow it to be set 3 different ways, instead of "on/off".
CVAR (Int,		switchonpickup,			1,			CVAR_USERINFO | CVAR_ARCHIVE);
CVAR (Float,	movebob,				0.25f,		CVAR_USERINFO | CVAR_ARCHIVE);
CVAR (Float,	stillbob,				0.f,		CVAR_USERINFO | CVAR_ARCHIVE);
CVAR (String,	playerclass,			"Fighter",	CVAR_USERINFO | CVAR_ARCHIVE);
// [BC] New userinfo entries for Skulltag.
CVAR (Int,		railcolor,				0,			CVAR_USERINFO | CVAR_ARCHIVE);
CVAR (Int,		handicap,				0,			CVAR_USERINFO | CVAR_ARCHIVE);
CVAR (Int,		connectiontype,			2,			CVAR_USERINFO | CVAR_ARCHIVE);

enum
{
	INFO_Name,
	INFO_Autoaim,
	INFO_Color,
	INFO_Skin,
	INFO_Gender,
	INFO_SwitchOnPickup,
	INFO_Railcolor,
	INFO_Handicap,
	INFO_MoveBob,
	INFO_ConnectionType,
	INFO_StillBob,
	INFO_PlayerClass,
};

const char *GenderNames[3] = { "male", "female", "other" };

static const char *UserInfoStrings[] =
{
	"name",
	"autoaim",
	"color",
	"skin",
	"gender",
	"switchonpickup",
	"railcolor",
	"handicap",
	"movebob",
	"connectiontype",
	"stillbob",
	"playerclass",
	NULL
};

int D_GenderToInt (const char *gender)
{
	if ( !stricmp( gender, "0" ))
		return ( GENDER_MALE );
	if ( !stricmp( gender, "1" ))
		return ( GENDER_FEMALE );
	if ( !stricmp( gender, "2" ))
		return ( GENDER_NEUTER );

	if (!stricmp (gender, "female"))
		return GENDER_FEMALE;
	else if (!stricmp (gender, "other") || !stricmp (gender, "cyborg"))
		return GENDER_NEUTER;
	else
		return GENDER_MALE;
}

int D_PlayerClassToInt (const char *classname)
{
	if (PlayerClasses.Size () > 1)
	{
		for (unsigned int i = 0; i < PlayerClasses.Size (); ++i)
		{
			const PClass *type = PlayerClasses[i].Type;

			if (stricmp (type->Meta.GetMetaString (APMETA_DisplayName), classname) == 0)
			{
				return i;
			}
		}
		return -1;
	}
	else
	{
		return 0;
	}
}

void D_GetPlayerColor (int player, float *h, float *s, float *v)
{
	int color = players[player].userinfo.color;

	RGBtoHSV (RPART(color)/255.f, GPART(color)/255.f, BPART(color)/255.f,
		h, s, v);

	if ( teamgame || teamplay || teamlms || teampossession )
	{
		if ( players[player].bOnTeam )
		{
			int		nColor;
			char	*pszColor;

			// Get the color string from the team object.
			pszColor = TEAM_GetColor( players[player].ulTeam );

			// Build the color based on the string.
			nColor = V_GetColorFromString( NULL, pszColor );

			// Convert.
			RGBtoHSV( RPART( nColor ) / 255.f, GPART( nColor ) / 255.f, BPART( nColor ) / 255.f,
				h, s, v );
		}
	}

	if (teamplay && players[player].userinfo.team < NUM_TEAMS)
	{
/*
		// In team play, force the player to use the team's hue
		// and adjust the saturation and value so that the team
		// hue is visible in the final color.
		*h = TeamHues[players[player].userinfo.team];
		if (gameinfo.gametype == GAME_Doom)
		{
			*s = *s*0.15f+0.8f;
			*v = *v*0.5f+0.4f;
		}
		else
		{
			// I'm not really happy with the red team color in Heretic...
			*s = team == 0 ? 0.6f : 0.8f;
			*v = *v*0.4f+0.3f;
		}
*/
	}
}

int D_GetFragCount (player_t *player)
{
	if (( teamplay == false ) || player->bOnTeam == false )
	{
		return player->fragcount;
	}
	else
	{
		return ( TEAM_GetFragCount( player->ulTeam ));
	}
}

void D_SetupUserInfo ()
{
	int i;
	ULONG	ulIdx;
	userinfo_t *coninfo = &players[consoleplayer].userinfo;

	// [BC] Servers and client demos don't do this.
	if (( NETWORK_GetState( ) == NETSTATE_SERVER ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return;
	}

	// [BC] Don't reset everyone's userinfo in client mode, since we don't want to erase
	// EVERYONE'S userinfo if we change ours.
	if ( NETWORK_GetState( ) != NETSTATE_CLIENT )
	{
		for (i = 0; i < MAXPLAYERS; i++)
			memset (&players[i].userinfo, 0, sizeof(userinfo_t));
	}

	strncpy (coninfo->netname, name, MAXPLAYERNAME);

	// [BC] Remove % signs from names.
	for ( ulIdx = 0; ulIdx < strlen( coninfo->netname ); ulIdx++ )
	{
		if ( coninfo->netname[ulIdx] == '%' )
			coninfo->netname[ulIdx] = ' ';
	}

	if (autoaim > 35.f || autoaim < 0.f)
	{
		coninfo->aimdist = ANGLE_1*35;
	}
	else
	{
		coninfo->aimdist = abs ((int)(autoaim * (float)ANGLE_1));
	}
	coninfo->color = color;
	coninfo->skin = R_FindSkin (skin, 0);
	coninfo->gender = D_GenderToInt (gender);
	coninfo->switchonpickup = switchonpickup;
		
	coninfo->MoveBob = (fixed_t)(65536.f * movebob);
	coninfo->StillBob = (fixed_t)(65536.f * stillbob);
	coninfo->PlayerClass = D_PlayerClassToInt (playerclass);

	// [BC] Handle new ST userinfo entries.
	coninfo->lRailgunTrailColor = railcolor;
	coninfo->lHandicap = handicap;
	if ( coninfo->lHandicap < 0 )
		coninfo->lHandicap = 0;
	else if ( coninfo->lHandicap > deh.MaxSoulsphere )
		coninfo->lHandicap = deh.MaxSoulsphere;
	coninfo->lConnectionType = connectiontype;

	R_BuildPlayerTranslation (consoleplayer);
}

void D_UserInfoChanged (FBaseCVar *cvar)
{
	UCVarValue val;
	char foo[256];
	ULONG	ulUpdateFlags;

	// Server doesn't do this.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		return;

	ulUpdateFlags = 0;
	if (cvar == &autoaim)
	{
		if (autoaim < 0.0f)
		{
			autoaim = 0.0f;
			return;
		}
		else if (autoaim > 5000.0f)
		{
			autoaim = 5000.f;
			return;
		}

		ulUpdateFlags |= USERINFO_AIMDISTANCE;
	}
	// Allow users to colorize their name.
	else if ( cvar == &name )
	{
		val = cvar->GetGenericRep( CVAR_String );
		V_ColorizeString( val.String );

		ulUpdateFlags |= USERINFO_NAME;
	}
	else if ( cvar == &gender )
		ulUpdateFlags |= USERINFO_GENDER;
	else if ( cvar == &color )
		ulUpdateFlags |= USERINFO_COLOR;
	else if ( cvar == &skin )
		ulUpdateFlags |= USERINFO_SKIN;
	else if ( cvar == &railcolor )
		ulUpdateFlags |= USERINFO_RAILCOLOR;
	else if ( cvar == &handicap )
	{
		if ( handicap < 0 )
		{
			handicap = 0;
			return;
		}
		if ( handicap > 200 )
		{
			handicap = 200;
			return;
		}

		ulUpdateFlags |= USERINFO_HANDICAP;
	}
	else if ( cvar == &connectiontype )
		ulUpdateFlags |= USERINFO_CONNECTIONTYPE;
	else if (( cvar == &playerclass ) && ( (gameinfo.gametype == GAME_Hexen) || (PlayerClasses.Size() > 1) ))
		ulUpdateFlags |= USERINFO_PLAYERCLASS;

	val = cvar->GetGenericRep (CVAR_String);
	if (4 + strlen (cvar->GetName ()) + strlen (val.String) > 256)
		I_Error ("User info descriptor too big");

	if ( NETWORK_GetState( ) != NETSTATE_CLIENT )
	{
		sprintf (foo, "\\%s\\%s", cvar->GetName (), val.String);

		Net_WriteByte (DEM_UINFCHANGED);
		Net_WriteString (foo);
	}
	else
	{
		if ( gamestate != GS_STARTUP )
			D_SetupUserInfo( );
	}

	// Send updated userinfo to the server.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) && ( CLIENT_GetConnectionState( ) >= CTS_CONNECTED ) && ( ulUpdateFlags > 0 ))
	{
		CLIENT_SendUserInfo( ulUpdateFlags );

		if ( CLIENTDEMO_IsRecording( ))
			CLIENTDEMO_WriteUserInfo( );
	}
}

static const char *SetServerVar (char *name, ECVarType type, BYTE **stream, bool singlebit)
{
	FBaseCVar *var = FindCVar (name, NULL);
	UCVarValue value;

	if (singlebit)
	{
		if (var != NULL)
		{
			int bitdata;
			int mask;

			value = var->GetFavoriteRep (&type);
			if (type != CVAR_Int)
			{
				return NULL;
			}
			bitdata = ReadByte (stream);
			mask = 1 << (bitdata & 31);
			if (bitdata & 32)
			{
				value.Int |= mask;
			}
			else
			{
				value.Int &= ~mask;
			}
		}
	}
	else
	{
		switch (type)
		{
		case CVAR_Bool:		value.Bool = ReadByte (stream) ? 1 : 0;	break;
		case CVAR_Int:		value.Int = ReadLong (stream);			break;
		case CVAR_Float:	value.Float = ReadFloat (stream);		break;
		case CVAR_String:	value.String = ReadString (stream);		break;
		default: break;	// Silence GCC
		}
	}

	if (var)
	{
		var->ForceSet (value, type);
	}

	if (type == CVAR_String)
	{
		delete[] value.String;
	}

	if (var)
	{
		value = var->GetGenericRep (CVAR_String);
		return value.String;
	}

	return NULL;
}

EXTERN_CVAR (Float, sv_gravity)

void D_SendServerInfoChange (const FBaseCVar *cvar, UCVarValue value, ECVarType type)
{
	size_t namelen;

	// Server doesn't do this.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		return;

	namelen = strlen (cvar->GetName ());

	Net_WriteByte (DEM_SINFCHANGED);
	Net_WriteByte ((BYTE)(namelen | (type << 6)));
	Net_WriteBytes ((BYTE *)cvar->GetName (), (int)namelen);
	switch (type)
	{
	case CVAR_Bool:		Net_WriteByte (value.Bool);		break;
	case CVAR_Int:		Net_WriteLong (value.Int);		break;
	case CVAR_Float:	Net_WriteFloat (value.Float);	break;
	case CVAR_String:	Net_WriteString (value.String);	break;
	default: break; // Silence GCC
	}
}

void D_SendServerFlagChange (const FBaseCVar *cvar, int bitnum, bool set)
{
	int namelen;

	namelen = (int)strlen (cvar->GetName ());

	Net_WriteByte (DEM_SINFCHANGEDXOR);
	Net_WriteByte (namelen);
	Net_WriteBytes ((BYTE *)cvar->GetName (), namelen);
	Net_WriteByte (bitnum | (set << 5));
}

void D_DoServerInfoChange (BYTE **stream, bool singlebit)
{
	const char *value;
	char name[64];
	int len;
	int type;

	// Server doesn't do this.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		return;

	len = ReadByte (stream);
	type = len >> 6;
	len &= 0x3f;
	if (len == 0)
		return;
	memcpy (name, *stream, len);
	*stream += len;
	name[len] = 0;

	if ( (value = SetServerVar (name, (ECVarType)type, stream, singlebit)) && ( NETWORK_GetState( ) != NETSTATE_SINGLE ))
	{
		Printf ("%s changed to %s\n", name, value);
	}
}

void D_WriteUserInfoStrings (int i, BYTE **stream, bool compact)
{
	if (i >= MAXPLAYERS)
	{
		WriteByte (0, stream);
	}
	else
	{
		userinfo_t *info = &players[i].userinfo;

		const PClass *type = PlayerClasses[info->PlayerClass].Type;

		if (!compact)
		{
			sprintf (*((char **)stream),
					 "\\name\\%s"
					 "\\autoaim\\%g"
					 "\\color\\%x %x %x"
					 "\\skin\\%s"
					 "\\gender\\%s"
					 "\\switchonpickup\\%d"
					 "\\railcolor\\%d"
					 "\\handicap\\%d"
					 "\\movebob\\%g"
					 "\\stillbob\\%g"
					 "\\connectiontype\\%d"
					 "\\playerclass\\%s"
					 ,
					 info->netname,
					 (double)info->aimdist / (float)ANGLE_1,
					 RPART(info->color), GPART(info->color), BPART(info->color),
					 skins[info->skin].name,
					 info->gender == GENDER_FEMALE ? "female" :
						info->gender == GENDER_NEUTER ? "other" : "male",
					 info->switchonpickup,
					 info->lRailgunTrailColor,
					 info->lHandicap,
					 (float)(info->MoveBob) / 65536.f,
					 (float)(info->StillBob) / 65536.f,
					 info->lConnectionType,
					 info->PlayerClass == -1 ? "Random" :
						type->Meta.GetMetaString (APMETA_DisplayName)
				);
		}
		else
		{
			sprintf (*((char **)stream),
				"\\"
				"\\%s"			// name
				"\\%g"			// autoaim
				"\\%x %x %x"	// color
				"\\%s"			// skin
				"\\%s"			// gender
				"\\%d"			// switchonpickup
				"\\%d"			// railcolor
				"\\%d"			// handicap
				"\\%g"			// movebob
				"\\%g"			// stillbob
				"\\%d"			// connectiontype
				"\\%s"			// playerclass
				,
				info->netname,
				(double)info->aimdist / (float)ANGLE_1,
				RPART(info->color), GPART(info->color), BPART(info->color),
				skins[info->skin].name,
				info->gender == GENDER_FEMALE ? "female" :
					info->gender == GENDER_NEUTER ? "other" : "male",
				info->switchonpickup,
				info->lRailgunTrailColor,
				info->lHandicap,
				(float)(info->MoveBob) / 65536.f,
				(float)(info->StillBob) / 65536.f,
				info->lConnectionType,
				info->PlayerClass == -1 ? "Random" :
					type->Meta.GetMetaString (APMETA_DisplayName)
			);
		}
	}

	*stream += strlen (*((char **)stream)) + 1;
}

void D_ReadUserInfoStrings (int i, BYTE **stream, bool update)
{
	userinfo_t *info = &players[i].userinfo;
	char *ptr = *((char **)stream);
	char *breakpt;
	char *value;
	bool compact;
	int infotype = -1;

	if (*ptr++ != '\\')
		return;

	compact = (*ptr == '\\') ? ptr++, true : false;

	if (i < MAXPLAYERS)
	{
		for (;;)
		{
			breakpt = strchr (ptr, '\\');

			if (breakpt != NULL)
				*breakpt = 0;

			if (compact)
			{
				value = ptr;
				infotype++;
			}
			else
			{
				value = breakpt + 1;
				if ( (breakpt = strchr (value, '\\')) )
					*breakpt = 0;

				int j = 0;
				while (UserInfoStrings[j] && stricmp (UserInfoStrings[j], ptr) != 0)
					j++;
				if (UserInfoStrings[j] == NULL)
				{
					infotype = -1;
				}
				else
				{
					infotype = j;
				}
			}

			switch (infotype)
			{
			case INFO_Autoaim: {
				double angles;

				angles = atof (value);
				if (angles > 35.f || angles < 0.f)
				{
					info->aimdist = ANGLE_1*35;
				}
				else
				{
					info->aimdist = abs ((int)(angles * (float)ANGLE_1));
				}
								}
				break;

			case INFO_Name:
				{
					//ULONG	ulIdx;
					char oldname[MAXPLAYERNAME+1];

					strncpy (oldname, info->netname, MAXPLAYERNAME);
					oldname[MAXPLAYERNAME] = 0;
					strncpy (info->netname, value, MAXPLAYERNAME);
					info->netname[MAXPLAYERNAME] = 0;

					V_CleanPlayerName(info->netname);

					/* Remove % signs from names. // [RC] Covered in V_CleanName
					for ( ulIdx = 0; ulIdx < strlen( info->netname ); ulIdx++ )
					{
						if ( info->netname[ulIdx] == '%' )
							info->netname[ulIdx] = ' ';
					}*/

					if (update && strcmp (oldname, info->netname) != 0)
					{
						Printf ("%s \\c-is now known as %s\n", oldname, info->netname);
					}
				}
				break;

			case INFO_Color:
				info->color = V_GetColorFromString (NULL, value);
				R_BuildPlayerTranslation (i);
				if (StatusBar != NULL && i == StatusBar->GetPlayer())
				{
					StatusBar->AttachToPlayer (&players[i]);
				}
				break;

			case INFO_Skin:
				info->skin = R_FindSkin (value, players[i].CurrentPlayerClass);
				if (players[i].mo != NULL)
				{
					if (players[i].cls != NULL &&
						players[i].mo->state->sprite.index ==
						GetDefaultByType (players[i].cls)->SpawnState->sprite.index)
					{ // Only change the sprite if the player is using a standard one
						players[i].mo->sprite = skins[info->skin].sprite;
						// [GZDoom]
						players[i].mo->scaleX = players[i].mo->scaleY = skins[info->skin].Scale;
						//players[i].mo->xscale = players[i].mo->yscale = skins[info->skin].scale;
					}
				}
				// Rebuild translation in case the new skin uses a different range
				// than the old one.
				R_BuildPlayerTranslation (i);
				if (StatusBar != NULL && i == StatusBar->GetPlayer())
				{
					StatusBar->SetFace (&skins[info->skin]);
				}

				// If the skin was hidden, reveal it!
				if ( skins[info->skin].bRevealed == false )
				{
					Printf( "Hidden skin \"%s\\c-\" has now been revealed!\n", skins[info->skin].name );
					skins[info->skin].bRevealed = true;
				}
				break;

			case INFO_Gender:
				info->gender = D_GenderToInt (value);
				break;

			case INFO_SwitchOnPickup:
				if (*value >= '0' && *value <= '9')
				{
					info->switchonpickup = atoi (value);
				}
				else if (stricmp (value, "true") == 0)
				{
					info->switchonpickup = 2;
				}
				else
				{
					info->switchonpickup = 0;
				}
				break;

			case INFO_Railcolor:

				info->lRailgunTrailColor = atoi( value );
				break;
			case INFO_Handicap:

				info->lHandicap = atoi( value );
				if ( info->lHandicap < 0 )
					info->lHandicap = 0;
				if ( info->lHandicap > deh.MaxSoulsphere )
					info->lHandicap = deh.MaxSoulsphere;
				break;
			case INFO_MoveBob:
				info->MoveBob = (fixed_t)(atof (value) * 65536.f);
				break;

			case INFO_StillBob:
				info->StillBob = (fixed_t)(atof (value) * 65536.f);
				break;

			case INFO_ConnectionType:

				info->lConnectionType = atoi( value );
				break;
			case INFO_PlayerClass:

				info->PlayerClass = D_PlayerClassToInt (value);
				break;

			default:
				break;
			}

			if (!compact)
			{
				*(value - 1) = '\\';
			}
			if (breakpt)
			{
				*breakpt = '\\';
				ptr = breakpt + 1;
			}
			else
			{
				break;
			}
		}
	}

	*stream += strlen (*((char **)stream)) + 1;
}

FArchive &operator<< (FArchive &arc, userinfo_t &info)
{
	if (arc.IsStoring ())
	{
		arc.Write (&info.netname, sizeof(info.netname));
	}
	else
	{
		arc.Read (&info.netname, sizeof(info.netname));
	}
	arc << info.aimdist << info.color << info.skin << info.gender << info.switchonpickup;
	return arc;
}

CCMD (playerinfo)
{
	if (argv.argc() < 2)
	{
		int i;

		for (i = 0; i < MAXPLAYERS; i++)
		{
			if (playeringame[i])
			{
				Printf ("%d. %s\n", i, players[i].userinfo.netname);
			}
		}
	}
	else
	{
		int i = atoi (argv[1]);
		Printf ("Name:				%s\n", players[i].userinfo.netname);
		Printf ("Team:				%s\n", players[i].bOnTeam ? TEAM_GetName( players[i].ulTeam ) : "NONE" );
		Printf ("Aimdist:			%d\n", players[i].userinfo.aimdist);
		Printf ("Color:				%06x\n", players[i].userinfo.color);
		Printf ("Skin:				%d\n", players[i].userinfo.skin);
		Printf ("Gender:			%d\n", players[i].userinfo.gender);
		Printf ("SwitchOnPickup:	%s\n", players[i].userinfo.switchonpickup == 0 ? "never" : players[i].userinfo.switchonpickup == 1 ? "only higher ranked" : "always" );
		Printf ("MoveBob:			%g\n", players[i].userinfo.MoveBob/65536.f);
		Printf ("StillBob:			%g\n", players[i].userinfo.StillBob/65536.f);
		Printf ("PlayerClass:		%d\n", players[i].userinfo.PlayerClass);
	}
}

#ifdef _DEBUG
// [BC] Debugging function.
CCMD( listinventory )
{
	AInventory	*pInventory;

	if ( players[consoleplayer].mo == NULL )
		return;

	pInventory = players[consoleplayer].mo->Inventory;
	while ( pInventory )
	{
		Printf( "%s\n", pInventory->GetClass( )->TypeName.GetChars( ));
		pInventory = pInventory->Inventory;
	}
}
#endif
