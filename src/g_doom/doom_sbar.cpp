#include "doomtype.h"
#include "doomstat.h"
#include "v_font.h"
#include "v_video.h"
#include "sbar.h"
#include "r_defs.h"
#include "w_wad.h"
#include "m_random.h"
#include "d_player.h"
#include "st_stuff.h"
#include "r_local.h"
#include "m_swap.h"
#include "a_keys.h"
#include "templates.h"
#include "i_system.h"
#include "deathmatch.h"
#include "network.h"
#include "team.h"
#include "scoreboard.h"
#include "chat.h"
#include "cooperative.h"
#include "invasion.h"

#define ST_EVILGRINCOUNT		(2*TICRATE)
#define ST_STRAIGHTFACECOUNT	(TICRATE/2)
#define ST_TURNCOUNT			(1*TICRATE)
#define ST_OUCHCOUNT			(1*TICRATE)
#define ST_RAMPAGEDELAY 		(2*TICRATE)

#define ST_MUCHPAIN 			20

CVAR( Bool, cl_onekey, false, CVAR_ARCHIVE );
CVAR( Bool, cl_stfullscreenhud, true, CVAR_ARCHIVE );
EXTERN_CVAR (Bool, vid_fps)

class FDoomStatusBar : public FBaseStatusBar
{
public:
	FDoomStatusBar () : FBaseStatusBar (32)
	{
		static const char *sharedLumpNames[] =
		{
			NULL,		NULL,		NULL,		NULL,		NULL,
			NULL,		NULL,		NULL,		NULL,		NULL,
			NULL,		NULL,		"STTMINUS",	"STTNUM0",	"STTNUM1",
			"STTNUM2",	"STTNUM3",	"STTNUM4",	"STTNUM5",	"STTNUM6",
			"STTNUM7",	"STTNUM8",	"STTNUM9",	"STYSNUM0",	"STYSNUM1",
			"STYSNUM2",	"STYSNUM3",	"STYSNUM4",	"STYSNUM5",	"STYSNUM6",
			"STYSNUM7",	"STYSNUM8",	"STYSNUM9"
		};
		FTexture *tex;

		FBaseStatusBar::Images.Init (sharedLumpNames, NUM_BASESB_IMAGES);
		tex = FBaseStatusBar::Images[imgBNumbers];
		BigWidth = tex->GetWidth();
		BigHeight = tex->GetHeight();

		DoCommonInit ();
		bEvilGrin = false;
	}

	~FDoomStatusBar ()
	{
	}

	void NewGame ()
	{
		Images.Uninit ();
		DoCommonInit ();
		if (CPlayer != NULL)
		{
			AttachToPlayer (CPlayer);
		}
	}

	void SetFace (void *skn)
	{
		const char *nameptrs[ST_NUMFACES];
		char names[ST_NUMFACES][9];
		char prefix[4];
		int i, j;
		int namespc;
		int facenum;
		FPlayerSkin *skin = (FPlayerSkin *)skn;

		for (i = 0; i < ST_NUMFACES; i++)
		{
			nameptrs[i] = names[i];
		}

		if (skin->face[0] != 0)
		{
			prefix[0] = skin->face[0];
			prefix[1] = skin->face[1];
			prefix[2] = skin->face[2];
			prefix[3] = 0;
			namespc = skin->namespc;
		}
		else
		{
			prefix[0] = 'S';
			prefix[1] = 'T';
			prefix[2] = 'F';
			prefix[3] = 0;
			namespc = ns_global;
		}

		facenum = 0;

		for (i = 0; i < ST_NUMPAINFACES; i++)
		{
			for (j = 0; j < ST_NUMSTRAIGHTFACES; j++)
			{
				sprintf (names[facenum++], "%sST%d%d", prefix, i, j);
			}
			sprintf (names[facenum++], "%sTR%d0", prefix, i);  // turn right
			sprintf (names[facenum++], "%sTL%d0", prefix, i);  // turn left
			sprintf (names[facenum++], "%sOUCH%d", prefix, i); // ouch!
			sprintf (names[facenum++], "%sEVL%d", prefix, i);  // evil grin ;)
			sprintf (names[facenum++], "%sKILL%d", prefix, i); // pissed off
			sprintf( names[facenum++], "%sARNO%d", prefix, i );	// Quad dmg
		}
		sprintf (names[facenum++], "%sGOD0", prefix);
		sprintf (names[facenum++], "%sDEAD0", prefix);

		Faces.Uninit ();
		Faces.Init (nameptrs, ST_NUMFACES, namespc);

		FaceIndex = 0;
		FaceCount = 0;
		OldFaceIndex = -1;
	}

	void MultiplayerChanged ()
	{
		FBaseStatusBar::MultiplayerChanged ();
		if ( NETWORK_GetState( ) != NETSTATE_SINGLE )
		{
			// draw face background
			StatusBarTex.DrawToBar ("STFBANY", 143, 1,
				translationtables[TRANSLATION_Players] + (CPlayer - players)*256);
		}
	}

	void AttachToPlayer (player_t *player)
	{
		player_t *oldplayer = CPlayer;

		FBaseStatusBar::AttachToPlayer (player);
		if (oldplayer != CPlayer)
		{
			SetFace (&skins[CPlayer->userinfo.skin]);
		}
		if ( NETWORK_GetState( ) != NETSTATE_SINGLE )
		{
			// draw face background
			StatusBarTex.DrawToBar ("STFBANY", 143, 1,
				translationtables[TRANSLATION_Players] + (CPlayer - players)*256);
		}
		bEvilGrin = false;
	}

	void Tick ()
	{
		FBaseStatusBar::Tick ();
		RandomNumber = M_Random ();
		UpdateFace ();
	}

	void Draw (EHudState state)
	{
		FBaseStatusBar::Draw (state);

		if (state == HUD_Fullscreen)
		{
			SB_state = screen->GetPageCount ();
			if ( cl_stfullscreenhud )
				DrawFullScreenStuffST( );
			else
				DrawFullScreenStuff ();
		}
		else if (state == HUD_StatusBar)
		{
			if (SB_state != 0)
			{
				SB_state--;
				DrawImage (&StatusBarTex, 0, 0);
				memset (OldArms, 255, sizeof(OldArms));
				OldKeys = -1;
				memset (OldAmmo, 255, sizeof(OldAmmo));
				memset (OldMaxAmmo, 255, sizeof(OldMaxAmmo));
				OldFaceIndex = -1;
				OldHealth = -1;
				OldArmor = -1;
				OldActiveAmmo = -1;
				OldFrags = -9999;
				OldPoints = -9999;
				FaceHealth = -9999;
			}
			DrawMainBar ();
			if (CPlayer->inventorytics > 0 && !(level.flags & LEVEL_NOINVENTORYBAR))
			{
				DrawInventoryBar ();
				SB_state = screen->GetPageCount ();
			}
		}
	}

private:
	struct FDoomStatusBarTexture : public FTexture
	{
	public:
		FDoomStatusBarTexture ();
		const BYTE *GetColumn (unsigned int column, const Span **spans_out);
		const BYTE *GetPixels ();
		void Unload ();
		~FDoomStatusBarTexture ();
		void DrawToBar (const char *name, int x, int y, BYTE *colormap_in = NULL);

	protected:
		void MakeTexture ();

		FTexture * BaseTexture;
		BYTE *Pixels;
	}
	StatusBarTex;

	void DoCommonInit ()
	{
		static const char *doomLumpNames[] =
		{
			"STKEYS0",	"STKEYS1",	"STKEYS2",	"STKEYS3",	"STKEYS4",
			"STKEYS5",	"STKEYS6",	"STKEYS7",	"STKEYS8",
			"STGNUM2",	"STGNUM3",	"STGNUM4",	"STGNUM5",	"STGNUM6",
			"STGNUM7",	"MEDIA0",	"ARTIBOX",	"SELECTBO",	"INVGEML1",
			"INVGEML2",	"INVGEMR1",	"INVGEMR2",	"STRRA0",	"RAGRA0",
			"DRARA0",	"SPRRA0",	"RESRA0",	"REGRA0",	"PRSRA0",
			"REFRA0",	"HIJRA0",	"HASRA0",	"BFLASMAL",	"RFLASMAL",
			"BSKUA0",	"RSKUA0",	"WFLASMAL",
		};

		Images.Init (doomLumpNames, NUM_DOOMSB_IMAGES);

		// In case somebody wants to use the Heretic status bar graphics...
		{
			FTexture *artibox = Images[imgARTIBOX];
			FTexture *selectbox = Images[imgSELECTBOX];
			if (artibox != NULL && selectbox != NULL)
			{
				selectbox->LeftOffset = artibox->LeftOffset;
				selectbox->TopOffset = artibox->TopOffset;
			}
		}

		StatusBarTex.Unload ();

		// [BC] Teamgame must also be false to draw STARMS.
		if (!deathmatch && !teamgame)
		{
			StatusBarTex.DrawToBar ("STARMS", 104, 0);
		}
		else if ( teamgame || possession || teampossession )
			StatusBarTex.DrawToBar( "STPTS", 104, 0 );

		StatusBarTex.DrawToBar ("STTPRCNT", 90, 3);		// Health %
		StatusBarTex.DrawToBar ("STTPRCNT", 221, 3);	// Armor %

		SB_state = screen->GetPageCount ();
		FaceLastAttackDown = -1;
		FacePriority = 0;
	}

	void DrawMainBar ()
	{
		int amount;

		DrawAmmoStats ();
		DrawFace ();
		DrawKeys ();

		if ( possession || teampossession || teamgame )
		{
			if ( (ULONG)OldPoints != CPlayer->lPointCount )
			{
				OldPoints = CPlayer->lPointCount;
				PointsRefresh = screen->GetPageCount ();
			}
			if (PointsRefresh)
			{
				PointsRefresh--;
				DrawNumber (OldPoints, 138/*110*/, 3, 2);
			}
		}
		else if ( deathmatch )
		{
			if (OldFrags != CPlayer->fragcount)
			{
				OldFrags = CPlayer->fragcount;
				FragsRefresh = screen->GetPageCount ();
			}
			if (FragsRefresh)
			{
				FragsRefresh--;
				DrawNumber (OldFrags, 138/*110*/, 3, 2);
			}
		}
		else
			DrawArms( );
/*
		// If we're spectating through someone else's eyes, and we're not allowed to see his
		// stats (health, armorpoints, etc.), don't bother drawing any numbers.
		if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) && ( SERVER_IsPlayerAllowedToKnowHealth( consoleplayer, displayplayer ) == false ))
			return;
*/
		if (CPlayer->health != OldHealth)
		{
			OldHealth = CPlayer->health;
			HealthRefresh = screen->GetPageCount ();
		}
		if (HealthRefresh)
		{
			HealthRefresh--;
			DrawNumber (OldHealth, 90/*48*/, 3);
		}
		AInventory *armor = /*[BC]*/ CPlayer->mo ? CPlayer->mo->FindInventory<ABasicArmor>() : NULL;
		int armorpoints = armor != NULL ? armor->Amount : 0;
		if (armorpoints != OldArmor)
		{
			OldArmor = armorpoints;
			ArmorRefresh = screen->GetPageCount ();
		}
		if (ArmorRefresh)
		{
			ArmorRefresh--;
			DrawNumber (OldArmor, 221/*179*/, 3);
		}
		if (CPlayer->ReadyWeapon != NULL)
		{
			AAmmo *ammo = CPlayer->ReadyWeapon->Ammo1;
			amount = ammo != NULL ? ammo->Amount : -9999;
		}
		else
		{
			amount = -9999;
		}
		if (amount != OldActiveAmmo)
		{
			OldActiveAmmo = amount;
			ActiveAmmoRefresh = screen->GetPageCount ();
		}
		if (ActiveAmmoRefresh)
		{
			ActiveAmmoRefresh--;
			if (OldActiveAmmo != -9999)
			{
				DrawNumber (OldActiveAmmo, 44/*2*/, 3);
			}
			else
			{
				DrawPartialImage (&StatusBarTex, 2, BigWidth*3);
			}
		}
	}

	void DrawArms ()
	{
		BYTE arms[6];
		int i, j;

		// [BC] The player may not have a body between intermission-less maps.
		if ( CPlayer->mo == NULL )
			return;

		// Catalog the weapons the player owns
		memset (arms, 0, sizeof(arms));
		for (i = 0; i < 6; i++)
		{
			for (j = 0; j < MAX_WEAPONS_PER_SLOT; j++)
			{
				const PClass *weap = LocalWeapons.Slots[i+2].GetWeapon (j);
				if (weap != NULL && CPlayer->mo->FindInventory (weap) != NULL)
				{
					arms[i] = 1;
					break;
				}
			}
		}

		// Draw slots that have changed ownership since last time
		for (i = 0; i < 6; i++)
		{
			if (arms[i] != OldArms[i])
			{
				ArmsRefresh[i%3] = screen->GetPageCount ();
			}
		}

		// Remember the arms state for next time
		memcpy (OldArms, arms, sizeof(arms));

		for (i = 0; i < 3; i++)
		{
			if (ArmsRefresh[i])
			{
				ArmsRefresh[i]--;
				int x = 111 + i * 12;

				DrawArm (arms[i], i, x, 4, true);
				DrawArm (arms[i+3], i+3, x, 14, false);
			}
		}
	}

	void DrawArm (int on, int picnum, int x, int y, bool drawBackground)
	{
		int w;
		FTexture *pic = on ? FBaseStatusBar::Images[imgSmNumbers + 2 + picnum] : Images[imgGNUM2 + picnum];

		if (pic != NULL)
		{
			w = pic->GetWidth();
			x -= pic->LeftOffset;
			y -= pic->TopOffset;

			if (drawBackground)
			{
				DrawPartialImage (&StatusBarTex, x, w);
			}
			DrawImage (pic, x, y);
		}
	}

	void DrawAmmoStats ()
	{
		static const ENamedName ammoTypes[4] =
		{
			NAME_Clip,
			NAME_Shell,
			NAME_RocketAmmo,
			NAME_Cell
		};
		int ammo[4], maxammo[4];
		int i;

		// [BC] The player may not have a body between intermission-less maps.
		if ( CPlayer->mo == NULL )
			return;

		// Catalog the player's ammo
		for (i = 0; i < 4; i++)
		{
			const PClass *type = PClass::FindClass (ammoTypes[i]);
			AInventory *item = CPlayer->mo->FindInventory (type);
			if (item != NULL)
			{
				ammo[i] = item->Amount;
				maxammo[i] = item->MaxAmount;
			}
			else
			{
				ammo[i] = 0;
				maxammo[i] = ((AInventory *)GetDefaultByType (type))->MaxAmount;
			}
		}

		// Draw ammo amounts that have changed since last time
		for (i = 0; i < 4; i++)
		{
			if (ammo[i] != OldAmmo[i])
			{
				AmmoRefresh = screen->GetPageCount ();
			}
			if (maxammo[i] != OldMaxAmmo[i])
			{
				MaxAmmoRefresh = screen->GetPageCount ();
			}
		}

		// Remember ammo counts for next time
		memcpy (OldAmmo, ammo, sizeof(ammo));
		memcpy (OldMaxAmmo, maxammo, sizeof(ammo));

		if (AmmoRefresh)
		{
			AmmoRefresh--;
			DrawPartialImage (&StatusBarTex, 276, 4*3);
			for (i = 0; i < 4; i++)
			{
				if (( NETWORK_GetState( ) != NETSTATE_CLIENT ) || ( SERVER_IsPlayerAllowedToKnowHealth( consoleplayer, ULONG( CPlayer - players ))))
					DrSmallNumber (ammo[i], 276, 5 + 6*i);
			}
		}
		if (MaxAmmoRefresh)
		{
			MaxAmmoRefresh--;
			DrawPartialImage (&StatusBarTex, 302, 4*3);
			for (i = 0; i < 4; i++)
			{
				if (( NETWORK_GetState( ) != NETSTATE_CLIENT ) || ( SERVER_IsPlayerAllowedToKnowHealth( consoleplayer, ULONG( CPlayer - players ))))
					DrSmallNumber (maxammo[i], 302, 5 + 6*i);
			}
		}
	}

	void DrawFace ()
	{
		// [BC] The player may not have a body between intermission-less maps.
		if ( CPlayer->mo == NULL )
			return;

		// If a player has an inventory item selected, it takes the place of the
		// face, for lack of a better place to put it.
		if (OldFaceIndex != FaceIndex)
		{
			FaceRefresh = screen->GetPageCount ();
			OldFaceIndex = FaceIndex;
		}
		if (FaceRefresh || (CPlayer->mo->InvSel != NULL && !(level.flags & LEVEL_NOINVENTORYBAR)))
		{
			if (FaceRefresh)
			{
				FaceRefresh--;
			}
			DrawPartialImage (&StatusBarTex, 142, 37);
			if (CPlayer->mo->InvSel == NULL || (level.flags & LEVEL_NOINVENTORYBAR))
			{
				DrawImage (Faces[FaceIndex], 143, 0);
			}
			else
			{
				DrawImage (TexMan(CPlayer->mo->InvSel->Icon), 144, 0);
				if (CPlayer->mo->InvSel->Amount != 1)
				{
					DrSmallNumber (CPlayer->mo->InvSel->Amount, 165, 24);
				}
				OldFaceIndex = -1;
			}
		}
	}

	void DrawKeys ()
	{
		AInventory *item;
		int keys;

		// [BC] The player may not have a body between intermission-less maps.
		if ( CPlayer->mo == NULL )
			return;

		// Catalog the player's current keys
		keys = 0;
		for (item = CPlayer->mo->Inventory; item != NULL; item = item->Inventory)
		{
			if (item->IsKindOf (RUNTIME_CLASS(AKey)))
			{
				int keynum = static_cast<AKey*>(item)->KeyNumber;
				if (keynum >= 1 && keynum <= 6)
				{
					keys |= 1 << (keynum-1);
				}
			}
		}

		// Remember keys for next time
		if (OldKeys != keys)
		{
			OldKeys = keys;
			KeysRefresh = screen->GetPageCount ();
		}

		// Draw keys that have changed since last time
		if (KeysRefresh)
		{
			KeysRefresh--;
			DrawPartialImage (&StatusBarTex, 239, 8);

			// Blue Keys
			switch (keys & (2|16))
			{
			case 2:		DrawImage (Images[imgKEYS0], 239, 3);	break;
			case 16:	DrawImage (Images[imgKEYS3], 239, 3);	break;
			case 18:	DrawImage (/*[BC]*/ cl_onekey ? Images[imgKEYS3] : Images[imgKEYS6], 239, 3);	break;
			}

			// Yellow Keys
			switch (keys & (4|32))
			{
			case 4:		DrawImage (Images[imgKEYS1], 239, 13);	break;
			case 32:	DrawImage (Images[imgKEYS4], 239, 13);	break;
			case 36:	DrawImage (/*[BC]*/ cl_onekey ? Images[imgKEYS4] : Images[imgKEYS7], 239, 13);	break;
			}

			// Red Keys
			switch (keys & (1|8))
			{
			case 1:		DrawImage (Images[imgKEYS2], 239, 23);	break;
			case 8:		DrawImage (Images[imgKEYS5], 239, 23);	break;
			case 9:		DrawImage (/*[BC]*/ cl_onekey ? Images[imgKEYS5] : Images[imgKEYS8], 239, 23);	break;
			}
		}
	}

//---------------------------------------------------------------------------
//
// PROC DrawInventoryBar
//
//---------------------------------------------------------------------------

	void DrawInventoryBar ()
	{
		const AInventory *item;
		int i;

		// If the player has no artifacts, don't draw the bar
		CPlayer->mo->InvFirst = ValidateInvFirst (7);
		if (CPlayer->mo->InvFirst != NULL)
		{
			for (item = CPlayer->mo->InvFirst, i = 0; item != NULL && i < 7; item = item->NextInv(), ++i)
			{
				DrawImage (Images[imgARTIBOX], 50+i*31, 2);
				DrawImage (TexMan(item->Icon), 50+i*31, 2);
				if (item->Amount != 1)
				{
					DrSmallNumber (item->Amount, 66+i*31, 24);
				}
				if (item == CPlayer->mo->InvSel)
				{
					DrawImage (Images[imgSELECTBOX], 50+i*31, 2);
				}
			}
			for (; i < 7; ++i)
			{
				DrawImage (Images[imgARTIBOX], 50+i*31, 2);
			}
			// Is there something to the left?
			if (CPlayer->mo->FirstInv() != CPlayer->mo->InvFirst)
			{
				DrawImage (Images[!(gametic & 4) ?
					imgINVLFGEM1 : imgINVLFGEM2], 38, 2);
			}
			// Is there something to the right?
			if (item != NULL)
			{
				DrawImage (Images[!(gametic & 4) ?
					imgINVRTGEM1 : imgINVRTGEM2], 269, 2);
			}
		}
	}

	void DrawNumber (int val, int x, int y, int size=3)
	{
		DrawPartialImage (&StatusBarTex, x-BigWidth*size, size*BigWidth);
		DrBNumber (val, x, y, size);
	}

	void DrawFullScreenStuff ()
	{
		const AInventory *item;
		int i;
		int ammotop;
		// [BC]
		AInventory		*pRune;

		// No need to draw this if we're spectating.
		if ( CPlayer->bSpectating )
			return;

		// Draw health
		screen->DrawTexture (Images[imgMEDI], 20, -2,
			DTA_HUDRules, HUD_Normal,
			DTA_CenterBottomOffset, true,
			TAG_DONE);
		if (( NETWORK_GetState( ) != NETSTATE_CLIENT ) || ( SERVER_IsPlayerAllowedToKnowHealth( consoleplayer, ULONG( CPlayer - players ))))
			DrBNumberOuter (CPlayer->health, 40, -BigHeight-4);

		// [BC] Draw rune.
		{
			pRune = CPlayer->mo->Inventory;
			while (( pRune ) && ( pRune->GetClass( )->IsDescendantOf( PClass::FindClass( "Rune" )) == false ))
				pRune = pRune->Inventory;

			if ( pRune )
			{
//				Printf( "%s\n", TexMan[pRune->Icon]->Name );
//				pRune->~AActor
				screen->DrawTexture( TexMan( pRune->Icon ), -76, -2,
					DTA_HUDRules, HUD_Normal,
					DTA_CenterBottomOffset, true,
					TAG_DONE );
			}
		}

		// Draw armor
		ABasicArmor *armor = CPlayer->mo->FindInventory<ABasicArmor>();
		if (armor != NULL && armor->Amount != 0)
		{
			screen->DrawTexture (TexMan(armor->Icon), 20, -24,
				DTA_HUDRules, HUD_Normal,
				DTA_CenterBottomOffset, true,
				TAG_DONE);
			if (( NETWORK_GetState( ) != NETSTATE_CLIENT ) || ( SERVER_IsPlayerAllowedToKnowHealth( consoleplayer, ULONG( CPlayer - players ))))
				DrBNumberOuter (armor->Amount, 40, -39);
		}

		// Draw ammo
		AAmmo *ammo1, *ammo2;
		int ammocount1, ammocount2;

		GetCurrentAmmo (ammo1, ammo2, ammocount1, ammocount2);
		ammotop = -1;
		if (ammo1 != NULL)
		{
			FTexture *ammoIcon = TexMan(ammo1->Icon);
			// Draw primary ammo in the bottom-right corner
			screen->DrawTexture (ammoIcon, -14, -4,
				DTA_HUDRules, HUD_Normal,
				DTA_CenterBottomOffset, true,
				TAG_DONE);
			DrBNumberOuter (ammo1->Amount, -67, -4 - BigHeight);
			ammotop = -4 - BigHeight;
			if (ammo2 != NULL && ammo2!=ammo1)
			{
				// Draw secondary ammo just above the primary ammo
				int y = MIN (-6 - BigHeight, -6 - (ammoIcon != NULL ? ammoIcon->GetHeight() : 0));
				screen->DrawTexture (TexMan(ammo2->Icon), -14, y,
					DTA_HUDRules, HUD_Normal,
					DTA_CenterBottomOffset, true,
					TAG_DONE);
				if (( NETWORK_GetState( ) != NETSTATE_CLIENT ) || ( SERVER_IsPlayerAllowedToKnowHealth( consoleplayer, ULONG( CPlayer - players ))))
					DrBNumberOuter (ammo2->Amount, -67, y - BigHeight);
				ammotop = y - BigHeight;
			}
		}

		// [BC] Draw points in possession.
		if ( possession || teampossession )
		{
			DrBNumberOuter (CPlayer->lPointCount, -44, 1);
		}
		else if (deathmatch)
		{ // Draw frags (in DM)
			DrBNumberOuter (CPlayer->fragcount, -44, 1);
		}
		// [BC] Draw a bunch of stuff in teamgame mode.
		else if ( teamgame )
		{
			// Draw points scored.
			DrBNumberOuter( CPlayer->lPointCount, -44, 1 );

			if ( ctf )
			{
				screen->DrawTexture (Images[imgBFLA], 18, -( BigHeight * 3 ) - 18,
					DTA_HUDRules, HUD_Normal,
					DTA_CenterBottomOffset, true,
					TAG_DONE);

				DrBNumberOuter( MIN( (int)TEAM_GetScore( TEAM_BLUE ), 99 ), 28, -( BigHeight * 3 ) - 18 - 29 );

				screen->DrawTexture (Images[imgRFLA], 18, -( BigHeight * 3 ) - 18 - 51,
					DTA_HUDRules, HUD_Normal,
					DTA_CenterBottomOffset, true,
					TAG_DONE);

				DrBNumberOuter( MIN( (int)TEAM_GetScore( TEAM_RED ), 99 ), 28, -( BigHeight * 3 ) - 18 - 51 - 29 );
			}
			else if ( skulltag )
			{
				screen->DrawTexture (Images[imgBSKU], 12, -( BigHeight * 3 ) - 18,
					DTA_HUDRules, HUD_Normal,
					DTA_CenterBottomOffset, true,
					TAG_DONE);

				DrBNumberOuter( MIN( (int)TEAM_GetScore( TEAM_BLUE ), 99 ), 16, -( BigHeight * 3 ) - 18 - 16 );

				screen->DrawTexture (Images[imgRSKU], 12, -( BigHeight * 3 ) - 18 - 24,
					DTA_HUDRules, HUD_Normal,
					DTA_CenterBottomOffset, true,
					TAG_DONE);

				DrBNumberOuter( MIN( (int)TEAM_GetScore( TEAM_RED ), 99 ), 16, -( BigHeight * 3 ) - 18 - 24 - 16 );
			}
		}

		else { // Draw keys (not DM)
			int maxw = 0;
			int count = 0;
			int x =  -2;
			int y = vid_fps? 12 : 2;

			for (item = CPlayer->mo->Inventory; item != NULL; item = item->Inventory)
			{
				if (item->Icon > 0 && item->IsKindOf (RUNTIME_CLASS(AKey)))
				{
					FTexture *keypic = TexMan(item->Icon);
					if (keypic != NULL)
					{
						int w = keypic->GetWidth ();
						int h = keypic->GetHeight ();
						if (w > maxw)
						{
							maxw = w;
						}
						screen->DrawTexture (keypic, x, y,
							DTA_LeftOffset, w,
							DTA_TopOffset, 0,
							DTA_HUDRules, HUD_Normal,
							TAG_DONE);
						if (++count == 3)
						{
							count = 0;
							y = vid_fps? 12 : 2;
							x -= maxw + 2;
							maxw = 0;
						}
						else
						{
							y += h + 2;
						}
					}
				}
			}
		}

		// Draw inventory
		if (!(level.flags & LEVEL_NOINVENTORYBAR))
		{
			if (CPlayer->inventorytics == 0)
			{
				if (CPlayer->mo->InvSel != NULL)
				{
					screen->DrawTexture (TexMan(CPlayer->mo->InvSel->Icon), -14, ammotop - 1/*-24*/,
						DTA_HUDRules, HUD_Normal,
						DTA_CenterBottomOffset, true,
						TAG_DONE);
					DrBNumberOuter (CPlayer->mo->InvSel->Amount, -68, ammotop - 18/*-41*/);
				}
			}
			else
			{
				CPlayer->mo->InvFirst = ValidateInvFirst (7);
				i = 0;
				if (CPlayer->mo->InvFirst != NULL)
				{
					for (item = CPlayer->mo->InvFirst; item != NULL && i < 7; item = item->NextInv(), ++i)
					{
						screen->DrawTexture (Images[imgARTIBOX], -106+i*31, -32,
							DTA_HUDRules, HUD_HorizCenter,
							DTA_Alpha, HX_SHADOW,
							TAG_DONE);
						screen->DrawTexture (TexMan(item->Icon), -105+i*31, -32,
							DTA_HUDRules, HUD_HorizCenter,
							TAG_DONE);
						if (item->Amount != 1)
						{
							DrSmallNumberOuter (item->Amount, -90+i*31, -10, true);
						}
						if (item == CPlayer->mo->InvSel)
						{
							screen->DrawTexture (Images[imgSELECTBOX], -91+i*31, -3,
								DTA_HUDRules, HUD_HorizCenter,
								DTA_CenterBottomOffset, true,
								TAG_DONE);
						}
					}
					for (; i < 7; i++)
					{
						screen->DrawTexture (Images[imgARTIBOX], -106+i*31, -32,
							DTA_HUDRules, HUD_HorizCenter,
							DTA_Alpha, HX_SHADOW,
							TAG_DONE);
					}
					// Is there something to the left?
					if (CPlayer->mo->FirstInv() != CPlayer->mo->InvFirst)
					{
						screen->DrawTexture (Images[!(gametic & 4) ?
							imgINVLFGEM1 : imgINVLFGEM2], -118, -33,
							DTA_HUDRules, HUD_HorizCenter,
							TAG_DONE);
					}
					// Is there something to the right?
					if (item != NULL)
					{
						screen->DrawTexture (Images[!(gametic & 4) ?
							imgINVRTGEM1 : imgINVRTGEM2], 113, -33,
							DTA_HUDRules, HUD_HorizCenter,
							TAG_DONE);
					}
				}
			}
		}
	}

	// [BC] Skulltag's new fullscreen HUD.
	void DrawFullScreenStuffST( void )
	{
		float			bScale;
		UCVarValue		ValWidth;
		UCVarValue		ValHeight;
		float			fXScale;
		float			fYScale;
		ULONG			ulCurYPos;
		ULONG			ulCurXPos;
		char			szString[32];
		char			szPatchName[9];
		ABasicArmor		*pArmor;
		AInventory		*pInventory;
		AAmmo			*pAmmo1;
		AAmmo			*pAmmo2;
		int				iAmmoCount1;
		int				iAmmoCount2;
		LONG			lKeyCount;
		LONG			lMaxKeyWidth;

		// No need to draw this if we're spectating.
		if ( CPlayer->bSpectating )
			return;

		ValWidth = con_virtualwidth.GetGenericRep( CVAR_Int );
		ValHeight = con_virtualheight.GetGenericRep( CVAR_Int );

		if (( con_scaletext ) && ( con_virtualwidth > 0 ) && ( con_virtualheight > 0 ))
		{
			fXScale =  (float)ValWidth.Int / 320.0f;
			fYScale =  (float)ValHeight.Int / 200.0f;
			bScale = true;
		}
		else
			bScale = false;

		// Use the console font for all the drawing here.
		screen->SetFont( ConFont );

		// Configure our "pen". Use an 4 unit lip from the bottom and left edges of the screen.
		ulCurXPos = 4;
		if ( bScale )
			ulCurYPos = ValHeight.Int - 4;
		else
			ulCurYPos = SCREENHEIGHT - 4;
		if(!instagib) {
			// Start by drawing the medkit.
			if ( bScale )
			{
				screen->DrawTexture( TexMan["MEDIA0"],
					ulCurXPos + ( TexMan["MEDIA0"]->GetWidth( ) / 2 ),
					ulCurYPos,
					DTA_VirtualWidth, ValWidth.Int,
					DTA_VirtualHeight, ValHeight.Int,
					TAG_DONE );
			}
			else
			{
				screen->DrawTexture( TexMan["MEDIA0"],
					ulCurXPos + ( TexMan["MEDIA0"]->GetWidth( ) / 2 ),
					ulCurYPos,
					TAG_DONE );
			}

			// Next, draw the health xxx/xxx string. This has to be done in parts to get the right
			// spacing.
			{
				if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) && ( SERVER_IsPlayerAllowedToKnowHealth( consoleplayer, ULONG( CPlayer - players )) == false ))
					sprintf( szString, "???" );
				else
					sprintf( szString, "%d", CPlayer->health );
				if ( bScale )
				{
					screen->DrawText( CR_RED,
						ulCurXPos + ( TexMan["ARM1A0"]->GetWidth( )) + 8 + ConFont->StringWidth( "200" ) - ConFont->StringWidth( szString ),
						ulCurYPos - ( TexMan["MEDIA0"]->GetHeight( ) / 2 ) - ( ConFont->GetHeight( ) / 2 ),
						szString,
						DTA_VirtualWidth, ValWidth.Int,
						DTA_VirtualHeight, ValHeight.Int,
						TAG_DONE );
				}
				else
				{
					screen->DrawText( CR_RED,
						ulCurXPos + ( TexMan["ARM1A0"]->GetWidth( )) + 8 + ConFont->StringWidth( "200" ) - ConFont->StringWidth( szString ),
						ulCurYPos - ( TexMan["MEDIA0"]->GetHeight( ) / 2 ) - ( ConFont->GetHeight( ) / 2 ),
						szString,
						TAG_DONE );
				}

				sprintf( szString, "/" );
				if ( bScale )
				{
					screen->DrawText( CR_WHITE,
						ulCurXPos + ( TexMan["ARM1A0"]->GetWidth( )) + 8 + ConFont->StringWidth( "200" ),
						ulCurYPos - ( TexMan["MEDIA0"]->GetHeight( ) / 2 ) - ( ConFont->GetHeight( ) / 2 ),
						szString,
						DTA_VirtualWidth, ValWidth.Int,
						DTA_VirtualHeight, ValHeight.Int,
						TAG_DONE );
				}
				else
				{
					screen->DrawText( CR_WHITE,
						ulCurXPos + ( TexMan["ARM1A0"]->GetWidth( )) + 8 + ConFont->StringWidth( "200" ),
						ulCurYPos - ( TexMan["MEDIA0"]->GetHeight( ) / 2 ) - ( ConFont->GetHeight( ) / 2 ),
						szString,
						TAG_DONE );
				}

				if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) && ( SERVER_IsPlayerAllowedToKnowHealth( consoleplayer, ULONG( CPlayer - players )) == false ))
					sprintf( szString, "???" );
				else
					sprintf( szString, "%d", ( CPlayer->Powers & PW_PROSPERITY ) ? ( deh.MaxSoulsphere + 50 ) : deh.StartHealth + CPlayer->lMaxHealthBonus );
				if ( bScale )
				{
					screen->DrawText( CR_RED,
						ulCurXPos + ( TexMan["ARM1A0"]->GetWidth( )) + 8 + ConFont->StringWidth( "200/200" ) - ConFont->StringWidth( szString ),
						ulCurYPos - ( TexMan["MEDIA0"]->GetHeight( ) / 2 ) - ( ConFont->GetHeight( ) / 2 ),
						szString,
						DTA_VirtualWidth, ValWidth.Int,
						DTA_VirtualHeight, ValHeight.Int,
						TAG_DONE );
				}
				else
				{
					screen->DrawText( CR_RED,
						ulCurXPos + ( TexMan["ARM1A0"]->GetWidth( )) + 8 + ConFont->StringWidth( "200/200" ) - ConFont->StringWidth( szString ),
						ulCurYPos - ( TexMan["MEDIA0"]->GetHeight( ) / 2 ) - ( ConFont->GetHeight( ) / 2 ),
						szString,
						TAG_DONE );
				}
			}
			}
		// Next, draw the armor.
		if(!instagib) {
			ulCurYPos -= TexMan["MEDIA0"]->GetHeight( ) + 4;
			if ( CPlayer->mo )
				pArmor = CPlayer->mo->FindInventory<ABasicArmor>( );
			else
				pArmor = NULL;
				
			if ( pArmor )
				sprintf( szPatchName, TexMan[pArmor->Icon]->Name );
			else
				sprintf( szPatchName, "ARM1A0" );

			if ( bScale )
			{
				screen->DrawTexture( TexMan[szPatchName],
					ulCurXPos + ( TexMan[szPatchName]->GetWidth( ) / 2 ),
					ulCurYPos,
					DTA_VirtualWidth, ValWidth.Int,
					DTA_VirtualHeight, ValHeight.Int,
					TAG_DONE );
			}
			else
			{
				screen->DrawTexture( TexMan[szPatchName],
					ulCurXPos + ( TexMan[szPatchName]->GetWidth( ) / 2 ),
					ulCurYPos,
					TAG_DONE );
			}

			// Next, draw the armor xxx/xxx string. This has to be done in parts to get the right
			// spacing.
			{
				if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) && ( SERVER_IsPlayerAllowedToKnowHealth( consoleplayer, ULONG( CPlayer - players )) == false ))
					sprintf( szString, "???" );
				else
					sprintf( szString, "%d", pArmor ? pArmor->Amount : 0 );
				if ( bScale )
				{
					screen->DrawText( CR_RED,
						ulCurXPos + ( TexMan["ARM1A0"]->GetWidth( )) + 8 + ConFont->StringWidth( "200" ) - ConFont->StringWidth( szString ),
						ulCurYPos - ( TexMan["ARM1A0"]->GetHeight( ) / 2 ) - ( ConFont->GetHeight( ) / 2 ),
						szString,
						DTA_VirtualWidth, ValWidth.Int,
						DTA_VirtualHeight, ValHeight.Int,
						TAG_DONE );
				}
				else
				{
					screen->DrawText( CR_RED,
						ulCurXPos + ( TexMan["ARM1A0"]->GetWidth( )) + 8 + ConFont->StringWidth( "200" ) - ConFont->StringWidth( szString ),
						ulCurYPos - ( TexMan["ARM1A0"]->GetHeight( ) / 2 ) - ( ConFont->GetHeight( ) / 2 ),
						szString,
						TAG_DONE );
				}

				sprintf( szString, "/" );
				if ( bScale )
				{
					screen->DrawText( CR_WHITE,
						ulCurXPos + ( TexMan["ARM1A0"]->GetWidth( )) + 8 + ConFont->StringWidth( "200" ),
						ulCurYPos - ( TexMan["ARM1A0"]->GetHeight( ) / 2 ) - ( ConFont->GetHeight( ) / 2 ),
						szString,
						DTA_VirtualWidth, ValWidth.Int,
						DTA_VirtualHeight, ValHeight.Int,
						TAG_DONE );
				}
				else
				{
					screen->DrawText( CR_WHITE,
						ulCurXPos + ( TexMan["ARM1A0"]->GetWidth( )) + 8 + ConFont->StringWidth( "200" ),
						ulCurYPos - ( TexMan["ARM1A0"]->GetHeight( ) / 2 ) - ( ConFont->GetHeight( ) / 2 ),
						szString,
						TAG_DONE );
				}

				if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) && ( SERVER_IsPlayerAllowedToKnowHealth( consoleplayer, ULONG( CPlayer - players )) == false ))
					sprintf( szString, "???" );
				else
					sprintf( szString, "%d", ( CPlayer->Powers & PW_PROSPERITY ) ? (( 100 * deh.BlueAC ) + 50 ) : ( 100 * deh.GreenAC ) + CPlayer->lMaxArmorBonus );
				if ( bScale )
				{
					screen->DrawText( CR_RED,
						ulCurXPos + ( TexMan["ARM1A0"]->GetWidth( )) + 8 + ConFont->StringWidth( "200/200" ) - ConFont->StringWidth( szString ),
						ulCurYPos - ( TexMan["ARM1A0"]->GetHeight( ) / 2 ) - ( ConFont->GetHeight( ) / 2 ),
						szString,
						DTA_VirtualWidth, ValWidth.Int,
						DTA_VirtualHeight, ValHeight.Int,
						TAG_DONE );
				}
				else
				{
					screen->DrawText( CR_RED,
						ulCurXPos + ( TexMan["ARM1A0"]->GetWidth( )) + 8 + ConFont->StringWidth( "200/200" ) - ConFont->StringWidth( szString ),
						ulCurYPos - ( TexMan["ARM1A0"]->GetHeight( ) / 2 ) - ( ConFont->GetHeight( ) / 2 ),
						szString,
						TAG_DONE );
				}
			}

		}

		// Now draw the ammo.
		if(!(dmflags & DF_INFINITE_AMMO)) { // [RC] Because then you wouldn't need this.
			GetCurrentAmmo( pAmmo1, pAmmo2, iAmmoCount1, iAmmoCount2 );
			if ( pAmmo1 )
			{
				if ( bScale )
				{
					ulCurXPos = ValWidth.Int - 4 - ConFont->StringWidth( "200/200" ) - TexMan["STIMA0"]->GetWidth( ) - 8;
					ulCurYPos = ValHeight.Int - 4;
				}
				else
				{
					ulCurXPos = SCREENWIDTH - 4 - ConFont->StringWidth( "200/200" ) - TexMan["STIMA0"]->GetWidth( ) - 8;
					ulCurYPos = SCREENHEIGHT - 4;
				}

				// First, draw the ammo xxx/xxx string. This is the reverse way from how the
				// health and armor are done.
				{
					if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) && ( SERVER_IsPlayerAllowedToKnowHealth( consoleplayer, ULONG( CPlayer - players )) == false ))
						sprintf( szString, "???" );
					else
						sprintf( szString, "%d", pAmmo1->Amount );

					if ( bScale )
					{
						screen->DrawText( CR_YELLOW,
							ulCurXPos + ConFont->StringWidth( "200" ) - ConFont->StringWidth( szString ),
							ulCurYPos - ( TexMan["MEDIA0"]->GetHeight( ) / 2 ) - ( ConFont->GetHeight( ) / 2 ),
							szString,
							DTA_VirtualWidth, ValWidth.Int,
							DTA_VirtualHeight, ValHeight.Int,
							TAG_DONE );
					}
					else
					{
						screen->DrawText( CR_YELLOW,
							ulCurXPos + ConFont->StringWidth( "200" ) - ConFont->StringWidth( szString ),
							ulCurYPos - ( TexMan["MEDIA0"]->GetHeight( ) / 2 ) - ( ConFont->GetHeight( ) / 2 ),
							szString,
							TAG_DONE );
					}

					sprintf( szString, "/" );
					if ( bScale )
					{
						screen->DrawText( CR_WHITE,
							ulCurXPos + ConFont->StringWidth( "200/" ) - ConFont->StringWidth( szString ),
							ulCurYPos - ( TexMan["MEDIA0"]->GetHeight( ) / 2 ) - ( ConFont->GetHeight( ) / 2 ),
							szString,
							DTA_VirtualWidth, ValWidth.Int,
							DTA_VirtualHeight, ValHeight.Int,
							TAG_DONE );
					}
					else
					{
						screen->DrawText( CR_WHITE,
							ulCurXPos + ConFont->StringWidth( "200/" ) - ConFont->StringWidth( szString ),
							ulCurYPos - ( TexMan["MEDIA0"]->GetHeight( ) / 2 ) - ( ConFont->GetHeight( ) / 2 ),
							szString,
							TAG_DONE );
					}

					if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) && ( SERVER_IsPlayerAllowedToKnowHealth( consoleplayer, ULONG( CPlayer - players )) == false ))
						sprintf( szString, "???" );
					else
						sprintf( szString, "%d", pAmmo1->MaxAmount );

					if ( bScale )
					{
						screen->DrawText( CR_YELLOW,
							ulCurXPos + ConFont->StringWidth( "200/200" ) - ConFont->StringWidth( szString ),						
							ulCurYPos - ( TexMan["MEDIA0"]->GetHeight( ) / 2 ) - ( ConFont->GetHeight( ) / 2 ),
							szString,
							DTA_VirtualWidth, ValWidth.Int,
							DTA_VirtualHeight, ValHeight.Int,
							TAG_DONE );
					}
					else
					{
						screen->DrawText( CR_YELLOW,
							ulCurXPos + ConFont->StringWidth( "200/200" ) - ConFont->StringWidth( szString ),
							ulCurYPos - ( TexMan["MEDIA0"]->GetHeight( ) / 2 ) - ( ConFont->GetHeight( ) / 2 ),
							szString,
							TAG_DONE );
					}
				}

				// Now draw the patch.
				if ( bScale )
				{
					screen->DrawTexture( TexMan( pAmmo1->Icon ),
						( ValWidth.Int + ( ulCurXPos + ConFont->StringWidth( "200/200" ))) / 2,
						ulCurYPos,
						DTA_VirtualWidth, ValWidth.Int,
						DTA_VirtualHeight, ValHeight.Int,
						TAG_DONE );
				}
				else
				{
					screen->DrawTexture( TexMan( pAmmo1->Icon ),
						( SCREENWIDTH + ( ulCurXPos + ConFont->StringWidth( "200/200" ))) / 2 ,
						ulCurYPos,
						TAG_DONE );
				}
			}
		}

		// Next, draw the fragcount/killcount/wincount, etc.
		if ( bScale )
			ulCurXPos = ValWidth.Int - 4;
		else
			ulCurXPos = SCREENWIDTH - 4;
		ulCurYPos = 4;
		if (deathmatch || teamgame || invasion || (cooperative && ( NETWORK_GetState( ) != NETSTATE_SINGLE )))
		{
			if ( possession || teampossession )
				sprintf( szString, "%d", CPlayer->lPointCount );
			else if ( teamlms )
				sprintf( szString, ""); // Frags would be distracting and confusing with the 'x left' being wins.
			else if ( lastmanstanding )
				sprintf( szString, "%d", CPlayer->ulWins );
			else if ( deathmatch )
				sprintf( szString, "%d", CPlayer->fragcount );
			else if (teamgame)
				sprintf( szString, "%d", CPlayer->lPointCount );
			else if( invasion || cooperative)
				sprintf( szString, "%d", CPlayer->killcount );

			ULONG left = SCOREBOARD_GetLeftToLimit( );
			if(left > 0)
				sprintf( szString, "%s \\cb(%d left)", szString, left );
			
			V_ColorizeString( szString );

			if ( bScale )
			{
				screen->DrawText( CR_RED,
					ulCurXPos - ConFont->StringWidth( szString ),
					ulCurYPos,
					szString,
					DTA_VirtualWidth, ValWidth.Int,
					DTA_VirtualHeight, ValHeight.Int,
					TAG_DONE );
			}
			else
			{
				screen->DrawText( CR_RED,
					ulCurXPos - ConFont->StringWidth( szString ),
					ulCurYPos,
					szString,
					TAG_DONE );
			}

		}
		// Otherwise, draw the keys.
		else
		{
			lKeyCount = 0;
			lMaxKeyWidth = 0;
			if ( CPlayer->mo )
				pInventory = CPlayer->mo->Inventory;
			else
				pInventory = NULL;
			for ( ; pInventory != NULL; pInventory = pInventory->Inventory )
			{
				if (( pInventory->IsKindOf( RUNTIME_CLASS( AKey ))) && ( pInventory->Icon > 0 ))
				{
					if ( bScale )
					{
						screen->DrawTexture( TexMan( pInventory->Icon ),
							ulCurXPos - TexMan( pInventory->Icon )->GetWidth( ),
							ulCurYPos,
							DTA_VirtualWidth, ValWidth.Int,
							DTA_VirtualHeight, ValHeight.Int,
							TAG_DONE );
					}
					else
					{
						screen->DrawTexture( TexMan( pInventory->Icon ),
							ulCurXPos - TexMan( pInventory->Icon )->GetWidth( ),
							ulCurYPos,
							TAG_DONE );
					}

					ulCurYPos += TexMan( pInventory->Icon )->GetHeight( ) + 2;

					if ( TexMan( pInventory->Icon )->GetWidth( ) > lMaxKeyWidth )
						lMaxKeyWidth = TexMan( pInventory->Icon )->GetWidth( );

					if ( ++lKeyCount == 3 )
					{
						ulCurXPos -= lMaxKeyWidth + 2;
						ulCurYPos = 4;

						lMaxKeyWidth = 0;
					}
				}
			}
		}
		// [RC] Restructured for greater intellegence

		// If this isn't DM, draw the skull/flag if the player is carrying it in ST/CTF, and
		// the ST/CTF score.
		if ( skulltag || ctf || oneflagctf )
		{
			if ( CPlayer->bOnTeam )
			{
				// First, check to see if the player is carrying the opposing team's flag/skull.
				pInventory = CPlayer->mo->FindInventory( TEAM_GetFlagItem( !CPlayer->ulTeam ));

				// If they're not, then check to see if they're carrying the white flag in one
				// flag CTF.
				if ( pInventory == NULL )
					pInventory = CPlayer->mo->FindInventory( PClass::FindClass( "WhiteFlag" ));

				// If the player is carrying any of these "flags", draw an indicator.
				if ( pInventory )
				{
					if ( bScale )
						ulCurXPos = ValWidth.Int * 3 / 4;
					else
						ulCurXPos = SCREENWIDTH * 3 / 4;
					if ( bScale )
						ulCurYPos = ValHeight.Int - 4;
					else
						ulCurYPos = SCREENHEIGHT - 4;

					if ( bScale )
					{
						screen->DrawTexture( TexMan( pInventory->Icon ),
							ulCurXPos,
							ulCurYPos,
							DTA_VirtualWidth, ValWidth.Int,
							DTA_VirtualHeight, ValHeight.Int,
							TAG_DONE );
					}
					else
					{
						screen->DrawTexture( TexMan( pInventory->Icon ),
							ulCurXPos,
							ulCurYPos,
							TAG_DONE );
					}
				}
			}

			// Also draw the current score in CTF/ST.
			ulCurXPos = 8;
			if ( bScale )
				ulCurYPos = ValHeight.Int - 4 - ( TexMan["MEDIA0"]->GetHeight( ) + 4 ) - ( TexMan["ARM1A0"]->GetHeight( ) + 4 ) - 14;
			else
				ulCurYPos = SCREENHEIGHT - 4 - ( TexMan["MEDIA0"]->GetHeight( ) + 4 ) - ( TexMan["ARM1A0"]->GetHeight( ) + 4 ) - 14;

			if ( ctf || oneflagctf )
				sprintf( szPatchName, "BFLASMAL" );
			else
				sprintf( szPatchName, "BSKUA0" );
			if ( bScale )
			{
				screen->DrawTexture( TexMan[szPatchName],
					ulCurXPos + TexMan[szPatchName]->GetWidth( ) / 2,
					ulCurYPos,
					DTA_VirtualWidth, ValWidth.Int,
					DTA_VirtualHeight, ValHeight.Int,
					TAG_DONE );
			}
			else
			{
				screen->DrawTexture( TexMan[szPatchName],
					ulCurXPos + TexMan[szPatchName]->GetWidth( ) / 2,
					ulCurYPos,
					TAG_DONE );
			}

			sprintf( szString, "%d", TEAM_GetScore( TEAM_BLUE ));
			if ( bScale )
			{
				screen->DrawText( CR_RED,
					ulCurXPos + TexMan[szPatchName]->GetWidth( ) + 16,
					ulCurYPos - ( TexMan[szPatchName]->GetHeight( ) / 2 ) - ( ConFont->GetHeight( ) / 2 ),
					szString,
					DTA_VirtualWidth, ValWidth.Int,
					DTA_VirtualHeight, ValHeight.Int,
					TAG_DONE );
			}
			else
			{
				screen->DrawText( CR_RED,
					ulCurXPos + TexMan[szPatchName]->GetWidth( ) + 16,
					ulCurYPos - ( TexMan[szPatchName]->GetHeight( ) / 2 ) - ( ConFont->GetHeight( ) / 2 ),
					szString,
					TAG_DONE );
			}

			ulCurYPos -= TexMan[szPatchName]->GetHeight( ) + 8;

			if ( ctf || oneflagctf )
				sprintf( szPatchName, "RFLASMAL" );
			else
				sprintf( szPatchName, "RSKUA0" );
			if ( bScale )
			{
				screen->DrawTexture( TexMan[szPatchName],
					ulCurXPos + TexMan[szPatchName]->GetWidth( ) / 2,
					ulCurYPos,
					DTA_VirtualWidth, ValWidth.Int,
					DTA_VirtualHeight, ValHeight.Int,
					TAG_DONE );
			}
			else
			{
				screen->DrawTexture( TexMan[szPatchName],
					ulCurXPos + TexMan[szPatchName]->GetWidth( ) / 2,
					ulCurYPos,
					TAG_DONE );
			}

			sprintf( szString, "%d", TEAM_GetScore( TEAM_RED ));
			if ( bScale )
			{
				screen->DrawText( CR_RED,
					ulCurXPos + TexMan[szPatchName]->GetWidth( ) + 16,
					ulCurYPos - ( TexMan[szPatchName]->GetHeight( ) / 2 ) - ( ConFont->GetHeight( ) / 2 ),
					szString,
					DTA_VirtualWidth, ValWidth.Int,
					DTA_VirtualHeight, ValHeight.Int,
					TAG_DONE );
			}
			else
			{
				screen->DrawText( CR_RED,
					ulCurXPos + TexMan[szPatchName]->GetWidth( ) + 16,
					ulCurYPos - ( TexMan[szPatchName]->GetHeight( ) / 2 ) - ( ConFont->GetHeight( ) / 2 ),
					szString,
					TAG_DONE );
			}
		}
		// [RC] If the game is team-based but isn't an a team
		// article game (ST/CTF), just show the scores / frags.
		else if ( teamplay || teamgame || teamlms || teampossession)
		{
			ULONG	ulRedPoints; // Frags or points
			ULONG	ulBluePoints;
			if(teamlms) {
				ulBluePoints = TEAM_GetWinCount( TEAM_BLUE );
				ulRedPoints = TEAM_GetWinCount( TEAM_RED );
			}			
			else if(teamplay) {
				ulBluePoints = TEAM_GetFragCount( TEAM_BLUE );
				ulRedPoints = TEAM_GetFragCount( TEAM_RED );
			}
			else {
				ulBluePoints = TEAM_GetScore( TEAM_BLUE );
				ulRedPoints = TEAM_GetScore( TEAM_RED );
			}
			ulCurXPos = 4;
			if ( bScale )
				ulCurYPos = ValHeight.Int - 4 - ( TexMan["MEDIA0"]->GetHeight( ) + 4 ) - ( TexMan["ARM1A0"]->GetHeight( ) + 4 ) - 14;
			else
				ulCurYPos = SCREENHEIGHT - 4 - ( TexMan["MEDIA0"]->GetHeight( ) + 4 ) - ( TexMan["ARM1A0"]->GetHeight( ) + 4 ) - 14;

			sprintf( szString , "\\cG%d \\cb- \\ch%d", ulRedPoints, ulBluePoints);
			V_ColorizeString( szString );
			ulCurYPos -= ((ConFont->GetHeight( ) + 1) * 5);

			if ( bScale )
			{
				screen->DrawText( CR_GRAY,
					ulCurXPos,
					(LONG)( ulCurYPos * fYScale ),
					szString,
					DTA_VirtualWidth, ValWidth.Int,
					DTA_VirtualHeight, ValHeight.Int,
					TAG_DONE );
			}
			else
			{
				screen->DrawText( CR_GRAY,
					ulCurXPos,
					ulCurYPos,
					szString,
					TAG_DONE );
			}
			
		}
		// Or it's just simple deathmatch.
		// Draw the individual rank/spread.
		else if ( deathmatch )
		{
			ulCurXPos = 4;
			if ( bScale )
				ulCurYPos = ValHeight.Int - 4 - ( TexMan["MEDIA0"]->GetHeight( ) + 4 ) * 2 - ( TexMan["ARM1A0"]->GetHeight( ) + 4 ) - 14;
			else
				ulCurYPos = SCREENHEIGHT - 4 - ( TexMan["MEDIA0"]->GetHeight( ) + 4 ) *2 - ( TexMan["ARM1A0"]->GetHeight( ) + 4 ) - 14;
			
			sprintf( szString, "spread: \\cC%s%d", SCOREBOARD_GetSpread( ) > 0 ? "+" : "", SCOREBOARD_GetSpread( ));
			V_ColorizeString( szString );

			if ( bScale )
			{
				screen->DrawText( CR_RED,
					ulCurXPos,
					ulCurYPos - ConFont->GetHeight( ),
					szString,
					DTA_VirtualWidth, ValWidth.Int,
					DTA_VirtualHeight, ValHeight.Int,
					TAG_DONE );
			}
			else
			{
				screen->DrawText( CR_RED,
					ulCurXPos,
					ulCurYPos - ConFont->GetHeight( ),
					szString,
					TAG_DONE );
			}

			if(!duel) { // [RC] Have spread, don't need rank. I mean, come on, there's only two players.
				ulCurYPos += ConFont->GetHeight( ) + 4;
				sprintf( szString, "rank: \\cC%d/%s%d", SCOREBOARD_GetRank( ) + 1, SCOREBOARD_IsTied( ) ? "\\cG" : "", SCOREBOARD_GetNumPlayers( ));
				V_ColorizeString( szString );
				if ( bScale )
				{
					screen->DrawText( CR_RED,
						ulCurXPos,
						ulCurYPos - ConFont->GetHeight( ),
						szString,
						DTA_VirtualWidth, ValWidth.Int,
						DTA_VirtualHeight, ValHeight.Int,
						TAG_DONE );
				}
				else
				{
					screen->DrawText( CR_RED,
						ulCurXPos,
						ulCurYPos - ConFont->GetHeight( ),
						szString,
						TAG_DONE );
				}
			}
		}
		// [RC] Draw wave and arch-viles (if any) in Invasion
		else if( invasion ) {
			if((INVASION_GetState( ) == IS_INPROGRESS) || (INVASION_GetState( ) == IS_BOSSFIGHT)) {
				ulCurXPos = 4;
				if ( bScale )
					ulCurYPos = ValHeight.Int - 4 - ( TexMan["MEDIA0"]->GetHeight( ) + 4 ) * 4 - ( TexMan["ARM1A0"]->GetHeight( ) + 4 ) - 14;
				else
					ulCurYPos = SCREENHEIGHT - 4 - ( TexMan["MEDIA0"]->GetHeight( ) + 4 ) * 4 - ( TexMan["ARM1A0"]->GetHeight( ) + 4 ) - 14;
				
				// Wave
				sprintf( szString, "Wave: %d \\cbof %d", INVASION_GetCurrentWave( ), (ULONG) wavelimit);
				V_ColorizeString( szString );
				if ( bScale )
				{
					screen->DrawText( CR_RED,
						ulCurXPos,
						ulCurYPos,
						szString,
						DTA_VirtualWidth, ValWidth.Int,
						DTA_VirtualHeight, ValHeight.Int,
						TAG_DONE );
				}
				else
				{
					screen->DrawText( CR_RED,
						ulCurXPos,
						ulCurYPos,
						szString,
						TAG_DONE );
				}
				ulCurYPos += ConFont->GetHeight( ) + 4;

				/* Monsters
				sprintf( szString, "Monsters: %d", INVASION_GetNumMonstersLeft( ));
				V_ColorizeString( szString );
				if ( bScale )
				{
					screen->DrawText( CR_RED,
						ulCurXPos,
						ulCurYPos,
						szString,
						DTA_VirtualWidth, ValWidth.Int,
						DTA_VirtualHeight, ValHeight.Int,
						TAG_DONE );
				}
				else
				{
					screen->DrawText( CR_RED,
						ulCurXPos,
						ulCurYPos,
						szString,
						TAG_DONE );
				}
				ulCurYPos += ConFont->GetHeight( ) + 4;*/

				// Arch-viles
				if(INVASION_GetNumArchVilesLeft( ) > 0) {
					sprintf( szString, "Arch-viles: %d", INVASION_GetNumArchVilesLeft( ));
					V_ColorizeString( szString );
					if ( bScale )
					{
						screen->DrawText( CR_RED,
							ulCurXPos,
							ulCurYPos,
							szString,
							DTA_VirtualWidth, ValWidth.Int,
							DTA_VirtualHeight, ValHeight.Int,
							TAG_DONE );
					}
					else
					{
						screen->DrawText( CR_RED,
							ulCurXPos,
							ulCurYPos,
							szString,
							TAG_DONE );
					}
				}
			}

		}
		


		// Finally, draw the rune we're carrying (if any).
		if ( CPlayer->mo )
			pInventory = CPlayer->mo->Inventory;
		else
			pInventory = NULL;
		while (( pInventory ) && ( pInventory->IsKindOf( PClass::FindClass( "Rune" )) == false ))
			pInventory = pInventory->Inventory;

		if (( pInventory ) && ( pInventory->Icon != 0 ))
		{
			if ( bScale )
				ulCurXPos = ValWidth.Int * 1 / 4;
			else
				ulCurXPos = SCREENWIDTH * 1 / 4;
			if ( bScale )
				ulCurYPos = ValHeight.Int - 4;
			else
				ulCurYPos = SCREENHEIGHT - 4;

			if ( bScale )
			{
				screen->DrawTexture( TexMan( pInventory->Icon ),
					ulCurXPos,
					ulCurYPos,
					DTA_VirtualWidth, ValWidth.Int,
					DTA_VirtualHeight, ValHeight.Int,
					TAG_DONE );
			}
			else
			{
				screen->DrawTexture( TexMan( pInventory->Icon ),
					ulCurXPos,
					ulCurYPos,
					TAG_DONE );
			}
		}

		// Revert back to the small font.
		screen->SetFont( SmallFont );
	}

	int CalcPainOffset ()
	{
		int 		health;
		static int	lastcalc;
		static int	oldhealth = -1;
		
		health = CPlayer->health > 100 ? 100 : CPlayer->health;

		if (health != oldhealth)
		{
			lastcalc = ST_FACESTRIDE * (((100 - health) * ST_NUMPAINFACES) / 101);
			oldhealth = health;
		}
		return lastcalc;
	}

	void ReceivedWeapon (AWeapon *weapon)
	{
		bEvilGrin = true;
	}

	//
	// This is a not-very-pretty routine which handles the face states
	// and their timing. The precedence of expressions is:
	//	dead > evil grin > turned head > straight ahead
	//
	void UpdateFace ()
	{
		int 		i;
		angle_t 	badguyangle;
		angle_t 	diffang;
		

		if (FacePriority < 10)
		{
			// dead
			if (CPlayer->health <= 0)
			{
				FacePriority = 9;
				FaceIndex = ST_DEADFACE;
				FaceCount = 1;
			}
		}

		if (FacePriority < 9)
		{
			if (CPlayer->bonuscount)
			{
				// picking up bonus
				if (bEvilGrin) 
				{
					// evil grin if just picked up weapon
					bEvilGrin = false;
					FacePriority = 8;
					FaceCount = ST_EVILGRINCOUNT;
					FaceIndex = CalcPainOffset() + ST_EVILGRINOFFSET;
				}
			}
			else 
			{
				// This happens when a weapon is added to the inventory
				// by other means than being picked up.
				bEvilGrin = false;
			}
		}

		if (FacePriority < 8)
		{
			if (CPlayer->damagecount
				&& CPlayer->attacker
				&& CPlayer->attacker != CPlayer->mo)
			{
				// being attacked
				FacePriority = 7;
				
				if (FaceHealth != -9999 && FaceHealth - CPlayer->health > ST_MUCHPAIN)
				{
					FaceCount = ST_TURNCOUNT;
					FaceIndex = CalcPainOffset() + ST_OUCHOFFSET;
					FacePriority = 8;
				}
				else
				{
					badguyangle = R_PointToAngle2(CPlayer->mo->x,
												  CPlayer->mo->y,
												  CPlayer->attacker->x,
												  CPlayer->attacker->y);
					
					if (badguyangle > CPlayer->mo->angle)
					{
						// whether right or left
						diffang = badguyangle - CPlayer->mo->angle;
						i = diffang > ANG180; 
					}
					else
					{
						// whether left or right
						diffang = CPlayer->mo->angle - badguyangle;
						i = diffang <= ANG180; 
					} // confusing, aint it?

					
					FaceCount = ST_TURNCOUNT;
					FaceIndex = CalcPainOffset();
					
					if (diffang < ANG45)
					{
						// head-on	  
						FaceIndex += ST_RAMPAGEOFFSET;
					}
					else if (i)
					{
						// turn face right
						FaceIndex += ST_TURNOFFSET;
					}
					else
					{
						// turn face left
						FaceIndex += ST_TURNOFFSET+1;
					}
				}
			}
		}
  
		if (FacePriority < 7)
		{
			// getting hurt because of your own damn stupidity
			if (CPlayer->damagecount)
			{
				if (OldHealth != -1 && CPlayer->health - OldHealth > ST_MUCHPAIN)
				{
					FacePriority = 7;
					FaceCount = ST_TURNCOUNT;
					FaceIndex = CalcPainOffset() + ST_OUCHOFFSET;
				}
				else
				{
					FacePriority = 6;
					FaceCount = ST_TURNCOUNT;
					FaceIndex = CalcPainOffset() + ST_RAMPAGEOFFSET;
				}

			}

		}
  
		if (FacePriority < 6)
		{
			// rapid firing
			if (CPlayer->cmd.ucmd.buttons & BT_ATTACK)
			{
				if (FaceLastAttackDown==-1)
					FaceLastAttackDown = ST_RAMPAGEDELAY;
				else if (!--FaceLastAttackDown)
				{
					FacePriority = 5;
					FaceIndex = CalcPainOffset() + ST_RAMPAGEOFFSET;
					FaceCount = 1;
					FaceLastAttackDown = 1;
				}
			}
			else
			{
				FaceLastAttackDown = -1;
			}
		}
  
		if (FacePriority < 5)
		{
			// invulnerability
			if ((CPlayer->cheats & CF_GODMODE)
				|| (CPlayer->mo != NULL && CPlayer->mo->flags2 & MF2_INVULNERABLE))
			{
				FacePriority = 4;
				FaceIndex = ST_GODFACE;
				FaceCount = 1;
			}
		}

		if ( FacePriority < 4 )
		{
			// Quad damage!
			if ( CPlayer->Powers & PW_QUADDAMAGE )
			{
				FacePriority = 3;
				FaceIndex = CalcPainOffset( ) + ST_QUADOFFSET;
				FaceCount = 1;
			}
		}

		// look left or look right if the facecount has timed out
		if (!FaceCount)
		{
			FaceIndex = CalcPainOffset() + (RandomNumber % 3);
			FaceCount = ST_STRAIGHTFACECOUNT;
			FacePriority = 0;
		}

		FaceCount--;
		FaceHealth = CPlayer->health;
	}

	enum
	{
		imgKEYS0,
		imgKEYS1,
		imgKEYS2,
		imgKEYS3,
		imgKEYS4,
		imgKEYS5,
		imgKEYS6,
		imgKEYS7,
		imgKEYS8,
		imgGNUM2,
		imgGNUM3,
		imgGNUM4,
		imgGNUM5,
		imgGNUM6,
		imgGNUM7,
		imgMEDI,
		imgARTIBOX,
		imgSELECTBOX,
		imgINVLFGEM1,
		imgINVLFGEM2,
		imgINVRTGEM1,
		imgINVRTGEM2,
		imgSTRR,
		imgRAGR,
		imgDRAR,
		imgSPRR,
		imgRESR,
		imgREGR,
		imgPRSR,
		imgREFR,
		imgHIJR,
		imgHASR,
		imgBFLA,
		imgRFLA,
		imgBSKU,
		imgRSKU,
		imgWFLA,

		NUM_DOOMSB_IMAGES
	};

	enum
	{
		ST_NUMPAINFACES		= 5,
		ST_NUMSTRAIGHTFACES	= 3,
		ST_NUMTURNFACES		= 2,
		ST_NUMSPECIALFACES	= 4,
		ST_NUMEXTRAFACES	= 2,
		ST_FACESTRIDE		= ST_NUMSTRAIGHTFACES+ST_NUMTURNFACES+ST_NUMSPECIALFACES,
		ST_NUMFACES			= ST_FACESTRIDE*ST_NUMPAINFACES+ST_NUMEXTRAFACES,

		ST_TURNOFFSET		= ST_NUMSTRAIGHTFACES,
		ST_OUCHOFFSET		= ST_TURNOFFSET + ST_NUMTURNFACES,
		ST_EVILGRINOFFSET	= ST_OUCHOFFSET + 1,
		ST_RAMPAGEOFFSET	= ST_EVILGRINOFFSET + 1,
		ST_QUADOFFSET		= ST_RAMPAGEOFFSET + 1,
		ST_GODFACE			= ST_NUMPAINFACES*ST_FACESTRIDE,
		ST_DEADFACE			= ST_GODFACE + 1
	};

	FImageCollection Images;
	FImageCollection Faces;

	int BigWidth;
	int BigHeight;

	int FaceIndex;
	int FaceCount;
	int RandomNumber;
	int OldFaceIndex;
	BYTE OldArms[6];
	int OldKeys;
	int OldAmmo[4];
	int OldMaxAmmo[4];
	int OldHealth;
	int OldArmor;
	int OldActiveAmmo;
	int OldFrags;
	int OldPoints;
	int FaceHealth;
	int FaceLastAttackDown;
	int FacePriority;

	char HealthRefresh;
	char ArmorRefresh;
	char ActiveAmmoRefresh;
	char FragsRefresh;
	char PointsRefresh;
	char ArmsRefresh[3];
	char AmmoRefresh;
	char MaxAmmoRefresh;
	char FaceRefresh;
	char KeysRefresh;

	bool bEvilGrin;
};

FDoomStatusBar::FDoomStatusBarTexture::FDoomStatusBarTexture ()
{
	BaseTexture = TexMan[TexMan.AddPatch("STBAR")];
	if (BaseTexture==NULL)
	{
		I_Error("Fatal error: STBAR not found");
	}
	UseType = FTexture::TEX_MiscPatch;
	Name[0]=0;	// doesn't need a name

	// now copy all the properties from the base texture
	CopySize(BaseTexture);
	Pixels = NULL;
}

const BYTE *FDoomStatusBar::FDoomStatusBarTexture::GetColumn (unsigned int column, const Span **spans_out)
{
	if (Pixels == NULL)
	{
		MakeTexture ();
	}

	BaseTexture->GetColumn(column, spans_out);
	return Pixels + column*Height;
}

const BYTE *FDoomStatusBar::FDoomStatusBarTexture::GetPixels ()
{
	if (Pixels == NULL)
	{
		MakeTexture ();
	}
	return Pixels;
}

void FDoomStatusBar::FDoomStatusBarTexture::Unload ()
{
	if (Pixels != NULL)
	{
		delete[] Pixels;
		Pixels = NULL;
	}
}
												
FDoomStatusBar::FDoomStatusBarTexture::~FDoomStatusBarTexture ()
{
	Unload ();
}


void FDoomStatusBar::FDoomStatusBarTexture::MakeTexture ()
{
	Pixels = new BYTE[Width*Height];
	const BYTE *pix = BaseTexture->GetPixels();
	memcpy(Pixels, pix, Width*Height);
}

void FDoomStatusBar::FDoomStatusBarTexture::DrawToBar (const char *name, int x, int y, BYTE *colormap_in)
{
	FTexture *pic;
	BYTE colormap[256];

	if (Pixels == NULL)
	{
		MakeTexture ();
	}

	if (colormap_in != NULL)
	{
		for (int i = 0; i < 256; ++i)
		{
			colormap[i] = colormap_in[i] == 255 ? Near255 : colormap_in[i];
		}
	}
	else
	{
		for (int i = 0; i < 255; ++i)
		{
			colormap[i] = i;
		}
		colormap[255] = Near255;
	}

	pic = TexMan[name];
	if (pic != NULL)
	{
		pic->GetWidth();
		x -= pic->LeftOffset;
		pic->CopyToBlock (Pixels, Width, Height, x, y, colormap);
	}
}

FBaseStatusBar *CreateDoomStatusBar ()
{
	return new FDoomStatusBar;
}
