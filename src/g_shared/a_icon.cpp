#include "a_sharedglobal.h"
#include "actor.h"
#include "info.h"
#include "d_player.h"

FState AFloatyIcon::States[] =
{
#define	S_TERMINATORARTIFACT	0
	S_BRIGHT( ARNO,	'A',	6,	NULL,				&States[S_TERMINATORARTIFACT+1]),
	S_BRIGHT( ARNO,	'B',	6,	NULL,				&States[S_TERMINATORARTIFACT+2]),
	S_BRIGHT( ARNO,	'C',	6,	NULL,				&States[S_TERMINATORARTIFACT+3]),
	S_BRIGHT( ARNO,	'D',	6,	NULL,				&States[S_TERMINATORARTIFACT+0]),
#define S_CHAT			( S_TERMINATORARTIFACT + 4 )
	S_BRIGHT( TALK,	'A',	-1,	NULL,				NULL ),
#define S_ENEMY			( S_CHAT + 1 )
	S_NORMAL( ENEM, 'A',	-1,	NULL,				NULL ),
#define	S_BLUEFLAG		( S_ENEMY + 1 )
	S_NORMAL (BFLS, 'A',   3, NULL 				, &States[S_BLUEFLAG+1]),
	S_NORMAL (BFLS, 'B',   3, NULL 				, &States[S_BLUEFLAG+2]),
	S_NORMAL (BFLS, 'C',   3, NULL 				, &States[S_BLUEFLAG+3]),
	S_BRIGHT (BFLS, 'D',   3, NULL 				, &States[S_BLUEFLAG+4]),
	S_BRIGHT (BFLS, 'E',   3, NULL 				, &States[S_BLUEFLAG+5]),
	S_BRIGHT (BFLS, 'F',   3, NULL 				, &States[S_BLUEFLAG+0]),
#define	S_REDFLAG		( S_BLUEFLAG + 6 )
	S_NORMAL (RFLS, 'A',   3, NULL 				, &States[S_REDFLAG+1]),
	S_NORMAL (RFLS, 'B',   3, NULL 				, &States[S_REDFLAG+2]),
	S_NORMAL (RFLS, 'C',   3, NULL 				, &States[S_REDFLAG+3]),
	S_BRIGHT (RFLS, 'D',   3, NULL 				, &States[S_REDFLAG+4]),
	S_BRIGHT (RFLS, 'E',   3, NULL 				, &States[S_REDFLAG+5]),
	S_BRIGHT (RFLS, 'F',   3, NULL 				, &States[S_REDFLAG+0]),
#define	S_WHITEFLAG		( S_REDFLAG + 6 )
	S_NORMAL (WFLS, 'A',   3, NULL 				, &States[S_WHITEFLAG+1]),
	S_NORMAL (WFLS, 'B',   3, NULL 				, &States[S_WHITEFLAG+2]),
	S_NORMAL (WFLS, 'C',   3, NULL 				, &States[S_WHITEFLAG+3]),
	S_BRIGHT (WFLS, 'D',   3, NULL 				, &States[S_WHITEFLAG+4]),
	S_BRIGHT (WFLS, 'E',   3, NULL 				, &States[S_WHITEFLAG+5]),
	S_BRIGHT (WFLS, 'F',   3, NULL 				, &States[S_WHITEFLAG+0]),
#define	S_BLUESKULL		( S_WHITEFLAG + 6 )
	S_NORMAL (BSKU, 'A',   10, NULL 				, &States[S_BLUESKULL+1]),
	S_BRIGHT (BSKU, 'B',   10, NULL 				, &States[S_BLUESKULL+0]),
#define	S_REDSKULL		( S_BLUESKULL + 2 )
	S_NORMAL (RSKU, 'A',   10, NULL 				, &States[S_REDSKULL+1]),
	S_BRIGHT (RSKU, 'B',   10, NULL 				, &States[S_REDSKULL+0]),
#define	S_EXCELLENT		( S_REDSKULL + 2 )
	S_NORMAL( EXCL, 'A',   -1, NULL,				NULL ),
#define	S_INCREDIBLE	( S_EXCELLENT + 1 )
	S_NORMAL( INCR, 'A',   -1, NULL,				NULL ),
#define	S_IMPRESSIVE	( S_INCREDIBLE + 1 )
	S_NORMAL( IMPR, 'A',   -1, NULL,				NULL ),
#define	S_MOST_IMPRESSIVE	( S_IMPRESSIVE + 1 )
	S_NORMAL( MIMP, 'A',   -1, NULL,				NULL ),
#define	S_DOMINATION		( S_MOST_IMPRESSIVE + 1 )
	S_NORMAL( DOMN, 'A',   -1, NULL,				NULL ),
#define	S_TOTAL_DOMINATION	( S_DOMINATION + 1 )
	S_NORMAL( TDOM, 'A',   -1, NULL,				NULL ),
#define	S_ACCURACY		( S_TOTAL_DOMINATION + 1 )
	S_NORMAL( ACCU, 'A',   -1, NULL,				NULL ),
#define	S_PRECISION		( S_ACCURACY + 1 )
	S_NORMAL( PREC, 'A',   -1, NULL,				NULL ),
#define	S_VICTORY		( S_PRECISION + 1 )
	S_NORMAL( VICT, 'A',   -1, NULL,				NULL ),
#define	S_PERFECT		( S_VICTORY + 1 )
	S_NORMAL( PFCT, 'A',   -1, NULL,				NULL ),
#define	S_FIRSTFRAG		( S_PERFECT + 1 )
	S_NORMAL( FFRG, 'A',   -1, NULL,				NULL ),
#define	S_TERMINATION	( S_FIRSTFRAG + 1 )
	S_NORMAL( TRMA, 'A',   -1, NULL,				NULL ),
#define	S_CAPTURE	( S_TERMINATION + 1 )
	S_NORMAL( CAPT, 'A',   -1, NULL,				NULL ),
#define	S_TAG		( S_CAPTURE + 1 )
	S_NORMAL( STAG, 'A',   -1, NULL,				NULL ),
#define	S_ASSIST	( S_TAG + 1 )
	S_NORMAL( ASST, 'A',   -1, NULL,				NULL ),
#define	S_DEFENSE	( S_ASSIST + 1 )
	S_NORMAL( DFNS, 'A',   -1, NULL,				NULL ),
#define	S_LLAMA			( S_DEFENSE + 1 )
	S_NORMAL( LLAM, 'A',   -1, NULL,				NULL ),
#define	S_YOUFAILIT		( S_LLAMA + 1 )
	S_NORMAL( FAIL, 'A',   -1, NULL,				NULL ),
#define	S_YOURSKILLISNOTENOUGH	( S_YOUFAILIT + 1 )
	S_NORMAL( SKIL, 'A',   -1, NULL,				NULL ),
#define	S_LAG			( S_YOURSKILLISNOTENOUGH + 1 )
	S_NORMAL( LAGG, 'A',   -1, NULL,				NULL ),
#define	S_FISTING		( S_LAG + 1 )
	S_NORMAL( FIST, 'A',   -1, NULL,				NULL ),
#define	S_SPAM			( S_FISTING + 1 )
	S_NORMAL( SPAM, 'A',   -1, NULL,				NULL ),
#define	S_POSSESSIONARTIFACT	( S_SPAM + 1 )
	S_BRIGHT( PPOS,	'A',	6,	NULL,				&States[S_POSSESSIONARTIFACT+1]),
	S_BRIGHT( PPOS,	'B',	6,	NULL,				&States[S_POSSESSIONARTIFACT+2]),
	S_BRIGHT( PPOS,	'C',	6,	NULL,				&States[S_POSSESSIONARTIFACT+3]),
	S_BRIGHT( PPOS,	'D',	6,	NULL,				&States[S_POSSESSIONARTIFACT+4]),
	S_BRIGHT( PPOS,	'E',	6,	NULL,				&States[S_POSSESSIONARTIFACT+5]),
	S_BRIGHT( PPOS,	'F',	6,	NULL,				&States[S_POSSESSIONARTIFACT]),
};

IMPLEMENT_ACTOR( AFloatyIcon, Any, -1, -1 )
	PROP_HeightFixed( 8 )
	PROP_Flags( MF_NOBLOCKMAP | MF_NOGRAVITY )

	PROP_SpawnState( 0 )
END_DEFAULTS

void AFloatyIcon::BeginPlay( )
{
	Super::BeginPlay( );
}

void AFloatyIcon::Tick( )
{
	Super::Tick( );

	// Delete stray icons. This shouldn't ever happen.
	if ( !tracer || !tracer->player || tracer->player->pIcon != this )
	{
		Destroy( );
		return;
	}

	// Make the icon float directly above the player's head.
	SetOrigin( tracer->x, tracer->y, tracer->z + tracer->height + ( 4 * FRACUNIT ));

	this->alpha = OPAQUE;
	this->RenderStyle = STYLE_Normal;

	if ( this->lTick )
	{
		this->lTick--;
		if ( this->lTick )
		{
			// If the icon is fading out, ramp down the alpha.
			if ( this->lTick <= TICRATE )
			{
				this->alpha = (LONG)( OPAQUE * (float)( (float)lTick / (float)TICRATE ));
				this->RenderStyle = STYLE_Translucent;
			}
		}
		else
		{
			Destroy( );
			return;
		}
	}

	// If the tracer has some type of visibility affect, apply it to the icon.
	if ( tracer->RenderStyle != STYLE_Normal || tracer->alpha != OPAQUE )
	{
		this->RenderStyle = tracer->RenderStyle;
		this->alpha = tracer->alpha;
	}
}

void AFloatyIcon::Destroy( )
{
	// Fix the tracers pointers to this icon.
	if ( tracer && tracer->player && tracer->player->pIcon == this )
		tracer->player->pIcon = NULL;

	Super::Destroy( );
}

void AFloatyIcon::SetTracer( AActor *pTracer )
{
	tracer = pTracer;

	// Make the icon float directly above the tracer's head.
	SetOrigin( tracer->x, tracer->y, tracer->z + tracer->height + ( 4 * FRACUNIT ));

	// If the tracer has some type of visibility affect, apply it to the icon.
	if ( tracer->RenderStyle != STYLE_Normal || tracer->alpha != OPAQUE )
	{
		this->RenderStyle = tracer->RenderStyle;
		this->alpha = tracer->alpha;
	}
}
