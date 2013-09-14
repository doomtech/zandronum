#include "a_lightning.h"
#include "p_lnspec.h"
#include "statnums.h"
#include "r_data.h"
#include "m_random.h"
#include "templates.h"
#include "s_sound.h"
#include "p_acs.h"
#include "r_sky.h"
#include "g_level.h"
// [BC] New #includes.
#include "network.h"
#include "sv_commands.h"

static FRandom pr_lightning ("Lightning");

IMPLEMENT_CLASS (DLightningThinker)

DLightningThinker::DLightningThinker ()
	: DThinker (STAT_LIGHTNING)
{
	Stopped = false;
	LightningLightLevels = NULL;
	LightningFlashCount = 0;
	NextLightningFlash = ((pr_lightning()&15)+5)*35; // don't flash at level start

	LightningLightLevels = new BYTE[numsectors + (numsectors+7)/8];
	memset (LightningLightLevels, 0, numsectors + (numsectors+7)/8);
}

DLightningThinker::~DLightningThinker ()
{
	if (LightningLightLevels != NULL)
	{
		delete[] LightningLightLevels;
	}
}

void DLightningThinker::Serialize (FArchive &arc)
{
	int i;
	BYTE *lights;

	Super::Serialize (arc);

	arc << Stopped << NextLightningFlash << LightningFlashCount;

	if (arc.IsLoading ())
	{
		if (LightningLightLevels != NULL)
		{
			delete[] LightningLightLevels;
		}
		LightningLightLevels = new BYTE[numsectors + (numsectors+7)/8];
	}
	lights = LightningLightLevels;
	for (i = (numsectors + (numsectors+7)/8); i > 0; ++lights, --i)
	{
		arc << *lights;
	}
}

void DLightningThinker::Tick ()
{
	if (!NextLightningFlash || LightningFlashCount)
	{
		LightningFlash();
	}
	else
	{
		// [Dusk] The server manages lightning. However, the client forces lightning
		// by setting NextLightningFlash to 0. To work around this, we simply don't
		// count the lightning flash down when in client mode.
		if ( NETWORK_InClientMode( ) == false )
			--NextLightningFlash;

		if (Stopped) Destroy();
	}
}

void DLightningThinker::LightningFlash ()
{
	// [BB] The server just tells the clients to call P_ForceLightning.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		SERVERCOMMANDS_Lightning( );

		// [Dusk] The server runs LIGHTNING ACS scripts too and calculates
		// the next lightning flash.
		goto nextflash;
	}

	int i, j;
	sector_t *tempSec;
	BYTE flashLight;

	if (LightningFlashCount)
	{
		LightningFlashCount--;
		if (LightningFlashCount)
		{ // reduce the brightness of the flash
			tempSec = sectors;
			for (i = numsectors, j = 0; i > 0; ++j, --i, ++tempSec)
			{
				// [RH] Checking this sector's applicability to lightning now
				// is not enough to know if we should lower its light level,
				// because it might have changed since the lightning flashed.
				// Instead, change the light if this sector was effected by
				// the last flash.
				if (LightningLightLevels[numsectors+(j>>3)] & (1<<(j&7)) &&
					LightningLightLevels[j] < tempSec->lightlevel-4)
				{
					tempSec->ChangeLightLevel(-4);
				}
			}
		}					
		else
		{ // remove the alternate lightning flash special
			tempSec = sectors;
			for (i = numsectors, j = 0; i > 0; ++j, --i, ++tempSec)
			{
				if (LightningLightLevels[numsectors+(j>>3)] & (1<<(j&7)))
				{
					tempSec->SetLightLevel(LightningLightLevels[j]);
				}
			}
			memset (&LightningLightLevels[numsectors], 0, (numsectors+7)/8);
			level.flags &= ~LEVEL_SWAPSKIES;
		}
		return;
	}

	LightningFlashCount = (pr_lightning()&7)+8;
	flashLight = 200+(pr_lightning()&31);
	tempSec = sectors;
	for (i = numsectors, j = 0; i > 0; --i, ++j, ++tempSec)
	{
		// allow combination of the lightning sector specials with bit masks
		int special = (tempSec->special&0xff);
		if (tempSec->GetTexture(sector_t::ceiling) == skyflatnum
			|| special == Light_IndoorLightning1
			|| special == Light_IndoorLightning2
			|| special == Light_OutdoorLightning)
		{
			LightningLightLevels[j] = tempSec->lightlevel;
			LightningLightLevels[numsectors+(j>>3)] |= 1<<(j&7);
			if (special == Light_IndoorLightning1)
			{
				tempSec->SetLightLevel(MIN<int> (tempSec->lightlevel+64, flashLight));
			}
			else if (special == Light_IndoorLightning2)
			{
				tempSec->SetLightLevel(MIN<int> (tempSec->lightlevel+32, flashLight));
			}
			else
			{
				tempSec->SetLightLevel(flashLight);
			}
			if (tempSec->lightlevel < LightningLightLevels[j])
			{
				tempSec->SetLightLevel(LightningLightLevels[j]);
			}
		}
	}

	level.flags |= LEVEL_SWAPSKIES;	// set alternate sky
	S_Sound (CHAN_AUTO, "world/thunder", 1.0, ATTN_NONE);

nextflash: // [Dusk] Server jumps back in here
	bool bClientOnly = NETWORK_InClientMode( );
	FBehavior::StaticStartTypedScripts (SCRIPT_Lightning, NULL, false, 0, false, bClientOnly);	// [RH] Run lightning scripts

	// Calculate the next lighting flash
	if (!NextLightningFlash)
	{
		if (pr_lightning() < 50)
		{ // Immediate Quick flash
			NextLightningFlash = (pr_lightning()&15)+16;
		}
		else
		{
			if (pr_lightning() < 128 && !(level.time&32))
			{
				NextLightningFlash = ((pr_lightning()&7)+2)*35;
			}
			else
			{
				NextLightningFlash = ((pr_lightning()&15)+5)*35;
			}
		}
	}
}

void DLightningThinker::ForceLightning (int mode)
{
	switch (mode)
	{
	default:
		NextLightningFlash = 0;
		break;

	case 1:
		NextLightningFlash = 0;
		// Fall through
	case 2:
		Stopped = true;
		break;
	}
}

static DLightningThinker *LocateLightning ()
{
	TThinkerIterator<DLightningThinker> iterator (STAT_LIGHTNING);
	return iterator.Next ();
}

void P_StartLightning ()
{
	DLightningThinker *lightning = LocateLightning ();
	if (lightning == NULL)
	{
		new DLightningThinker ();
	}
}

void P_ForceLightning (int mode)
{
	DLightningThinker *lightning = LocateLightning ();
	if (lightning == NULL)
	{
		lightning = new DLightningThinker ();
	}
	if (lightning != NULL)
	{
		lightning->ForceLightning (mode);
	}
}
