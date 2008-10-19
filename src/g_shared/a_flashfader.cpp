#include "a_sharedglobal.h"
#include "g_level.h"
#include "d_player.h"
// [CW] New include.
#include "sv_commands.h"

IMPLEMENT_POINTY_CLASS (DFlashFader)
 DECLARE_POINTER (ForWho)
END_POINTERS

DFlashFader::DFlashFader ()
{
}

DFlashFader::DFlashFader (float r1, float g1, float b1, float a1,
						  float r2, float g2, float b2, float a2,
						  float time, AActor *who)
	: TotalTics ((int)(time*TICRATE)), StartTic (level.time), ForWho (who)
{
	Blends[0][0]=r1; Blends[0][1]=g1; Blends[0][2]=b1; Blends[0][3]=a1;
	Blends[1][0]=r2; Blends[1][1]=g2; Blends[1][2]=b2; Blends[1][3]=a2;

	// [CW] If we're the server, tell this client to do the fade.
	// [BB] Only possible if who is not a NULL pointer.
	if ( who && (NETWORK_GetState( ) == NETSTATE_SERVER) )
		SERVERCOMMANDS_DoFlashFader( Blends[0][0], Blends[0][1], Blends[0][2], Blends[0][3], Blends[1][0], Blends[1][1], Blends[1][2], Blends[1][3], time, ULONG( who->player - players));
}

void DFlashFader::Destroy ()
{
	SetBlend (1.f);
	Super::Destroy();
}

void DFlashFader::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << TotalTics << StartTic << ForWho;
	for (int i = 1; i >= 0; --i)
		for (int j = 3; j >= 0; --j)
			arc << Blends[i][j];
}

void DFlashFader::Tick ()
{
	if (ForWho == NULL || ForWho->player == NULL)
	{
		Destroy ();
		return;
	}
	if (level.time >= StartTic+TotalTics)
	{
		SetBlend (1.f);
		Destroy ();
		return;
	}
	SetBlend ((float)(level.time - StartTic) / (float)TotalTics);
}

void DFlashFader::SetBlend (float time)
{
	if (ForWho == NULL || ForWho->player == NULL)
	{
		return;
	}
	player_t *player = ForWho->player;
	float iT = 1.f - time;
	player->BlendR = Blends[0][0]*iT + Blends[1][0]*time;
	player->BlendG = Blends[0][1]*iT + Blends[1][1]*time;
	player->BlendB = Blends[0][2]*iT + Blends[1][2]*time;
	player->BlendA = Blends[0][3]*iT + Blends[1][3]*time;
}

void DFlashFader::Cancel ()
{
	TotalTics = level.time - StartTic;
	Blends[1][3] = 0.f;
}
