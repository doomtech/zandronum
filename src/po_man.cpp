
//**************************************************************************
//**
//** PO_MAN.C : Heretic 2 : Raven Software, Corp.
//**
//** $RCSfile: po_man.c,v $
//** $Revision: 1.22 $
//** $Date: 95/09/28 18:20:56 $
//** $Author: cjr $
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include "doomdef.h"
#include "p_local.h"
#include "r_local.h"
#include "i_system.h"
#include "w_wad.h"
#include "m_swap.h"
#include "m_bbox.h"
#include "tables.h"
#include "s_sndseq.h"
#include "a_sharedglobal.h"
#include "r_main.h"
#include "p_lnspec.h"
#include "r_interpolate.h"
#include "g_level.h"
#include "po_man.h"
// [BC] New #includes.
#include "cl_demo.h"
#include "network.h"
#include "version.h"
#include "sv_commands.h"

// MACROS ------------------------------------------------------------------

#define PO_MAXPOLYSEGS 64

// TYPES -------------------------------------------------------------------

inline vertex_t *side_t::V1() const
{
	return this == linedef->sidedef[0]? linedef->v1 : linedef->v2;
}

inline vertex_t *side_t::V2() const
{
	return this == linedef->sidedef[0]? linedef->v2 : linedef->v1;
}


inline FArchive &operator<< (FArchive &arc, podoortype_t &type)
{
	BYTE val = (BYTE)type;
	arc << val;
	type = (podoortype_t)val;
	return arc;
}
/* [BB] Moved to p_local.h
class DPolyAction : public DThinker
{
	DECLARE_CLASS (DPolyAction, DThinker)
	HAS_OBJECT_POINTERS
public:
	DPolyAction (int polyNum);
	void Serialize (FArchive &arc);
	void Destroy();
	int GetSpeed() const { return m_Speed; }

	void StopInterpolation ();

	LONG	GetSpeed( void );
	void	SetSpeed( LONG lSpeed );

	LONG	GetDist( void );
	void	SetDist( LONG lDist );

	LONG	GetPolyObj( void );
protected:
	DPolyAction ();
	int m_PolyObj;
	int m_Speed;
	int m_Dist;
	TObjPtr<DInterpolation> m_Interpolation;

	void SetInterpolation ();
};

class DRotatePoly : public DPolyAction
{
	DECLARE_CLASS (DRotatePoly, DPolyAction)
public:
	DRotatePoly (int polyNum);
	void Tick ();
	void UpdateToClient( ULONG ulClient );

private:
	DRotatePoly ();

	friend bool EV_RotatePoly (line_t *line, int polyNum, int speed, int byteAngle, int direction, bool overRide);
};


class DMovePoly : public DPolyAction
{
	DECLARE_CLASS (DMovePoly, DPolyAction)
public:
	DMovePoly (int polyNum);
	void Serialize (FArchive &arc);
	void Tick ();
	void UpdateToClient( ULONG ulClient );

	LONG	GetAngle( void );
	void	SetAngle( LONG lAngle );

	LONG	GetXSpeed( void );
	void	SetXSpeed( LONG lSpeed );

	LONG	GetYSpeed( void );
	void	SetYSpeed( LONG lSpeed );
protected:
	DMovePoly ();
	int m_Angle;
	fixed_t m_xSpeed; // for sliding walls
	fixed_t m_ySpeed;

	friend bool EV_MovePoly (line_t *line, int polyNum, int speed, angle_t angle, fixed_t dist, bool overRide);
};


class DPolyDoor : public DMovePoly
{
	DECLARE_CLASS (DPolyDoor, DMovePoly)
public:
	DPolyDoor (int polyNum, podoortype_t type);
	void Serialize (FArchive &arc);
	void Tick ();
	void UpdateToClient( ULONG ulClient );

	LONG	GetDirection( void );
	void	SetDirection( LONG lDirection );

	LONG	GetTotalDist( void );
	void	SetTotalDist( LONG lDist );

	bool	GetClose( void );
	void	SetClose( bool bClose );

protected:
	int m_Direction;
	int m_TotalDist;
	int m_Tics;
	int m_WaitTics;
	podoortype_t m_Type;
	bool m_Close;

	friend bool EV_OpenPolyDoor (line_t *line, int polyNum, int speed, angle_t angle, int delay, int distance, podoortype_t type);
private:
	DPolyDoor ();
};
*/
// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void PO_Init (void);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// [BC]
FPolyObj *GetPolyobjByIndex( ULONG ulPolyIdx );
static void RotatePt (int an, fixed_t *x, fixed_t *y, fixed_t startSpotX,
	fixed_t startSpotY);
static void UnLinkPolyobj (FPolyObj *po);
static void LinkPolyobj (FPolyObj *po);
static bool CheckMobjBlocking (side_t *seg, FPolyObj *po);
static void InitBlockMap (void);
static void IterFindPolySides (FPolyObj *po, side_t *side);
static void SpawnPolyobj (int index, int tag, int type);
static void TranslateToStartSpot (int tag, int originX, int originY);
static void DoMovePolyobj (FPolyObj *po, int x, int y);
static void InitSegLists ();
static void KillSegLists ();

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern seg_t *segs;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

polyblock_t **PolyBlockMap;
FPolyObj *polyobjs; // list of all poly-objects on the level
int po_NumPolyobjs;
polyspawns_t *polyspawns; // [RH] Let P_SpawnMapThings() find our thingies for us

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static TArray<SDWORD> KnownPolySides;

// CODE --------------------------------------------------------------------



//==========================================================================
//
//
//
//==========================================================================

IMPLEMENT_POINTY_CLASS (DPolyAction)
	DECLARE_POINTER(m_Interpolation)
END_POINTERS

DPolyAction::DPolyAction ()
{
}

void DPolyAction::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << m_PolyObj << m_Speed << m_Dist << m_Interpolation;
}

DPolyAction::DPolyAction (int polyNum)
{
	m_PolyObj = polyNum;
	m_Speed = 0;
	m_Dist = 0;
	SetInterpolation ();
}

void DPolyAction::Destroy()
{
	FPolyObj *poly = PO_GetPolyobj (m_PolyObj);

	if (poly->specialdata == NULL || poly->specialdata == this)
	{
		poly->specialdata = NULL;
	}

	StopInterpolation();
	Super::Destroy();
}

void DPolyAction::SetInterpolation ()
{
	FPolyObj *poly = PO_GetPolyobj (m_PolyObj);
	m_Interpolation = poly->SetInterpolation();
}

void DPolyAction::StopInterpolation ()
{
	if (m_Interpolation != NULL)
	{
		m_Interpolation->DelRef();
		m_Interpolation = NULL;
	}
}

LONG DPolyAction::GetSpeed ()
{
	return ( m_Speed );
}

void DPolyAction::SetSpeed (LONG lSpeed)
{
	m_Speed = lSpeed;
}

LONG DPolyAction::GetDist ()
{
	return ( m_Dist );
}

void DPolyAction::SetDist (LONG lDist)
{
	m_Dist = lDist;
}

LONG DPolyAction::GetPolyObj ()
{
	return ( m_PolyObj );
}

// [WS] This should never be called.
void DPolyAction::UpdateToClient( ULONG ulClient )
{
	Printf("WARNING: DPolyAction::UpdateToClient was called. This should never happen! Please report this at the %s bug tracker!\n", GAMENAME);
}

//==========================================================================
//
//
//
//==========================================================================

IMPLEMENT_CLASS (DRotatePoly)

DRotatePoly::DRotatePoly ()
{
}

DRotatePoly::DRotatePoly (int polyNum)
	: Super (polyNum)
{
}

// [BC]
void DRotatePoly::UpdateToClient( ULONG ulClient )
{
	SERVERCOMMANDS_DoRotatePoly( m_Speed, m_PolyObj, ulClient, SVCF_ONLYTHISCLIENT );
}

//==========================================================================
//
//
//
//==========================================================================

IMPLEMENT_CLASS (DMovePoly)

DMovePoly::DMovePoly ()
{
}

void DMovePoly::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << m_Angle << m_xSpeed << m_ySpeed;
}

DMovePoly::DMovePoly (int polyNum)
	: Super (polyNum)
{
	m_Angle = 0;
	m_xSpeed = 0;
	m_ySpeed = 0;
}

void DMovePoly::UpdateToClient( ULONG ulClient )
{
	SERVERCOMMANDS_DoMovePoly( m_xSpeed, m_ySpeed, m_PolyObj, ulClient, SVCF_ONLYTHISCLIENT );
}

LONG DMovePoly::GetXSpeed ()
{
	return ( m_xSpeed );
}

void DMovePoly::SetXSpeed (LONG lSpeed)
{
	m_xSpeed = lSpeed;
}

LONG DMovePoly::GetYSpeed ()
{
	return ( m_ySpeed );
}

void DMovePoly::SetYSpeed (LONG lSpeed)
{
	m_ySpeed = lSpeed;
}

LONG DMovePoly::GetAngle ()
{
	return ( m_Angle );
}

void DMovePoly::SetAngle (LONG lAngle)
{
	m_Angle = lAngle;
}

//==========================================================================
//
//
//
//==========================================================================

IMPLEMENT_CLASS (DPolyDoor)

DPolyDoor::DPolyDoor ()
{
}

void DPolyDoor::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << m_Direction << m_TotalDist << m_Tics << m_WaitTics << m_Type << m_Close;
}

DPolyDoor::DPolyDoor (int polyNum, podoortype_t type)
	: Super (polyNum), m_Type (type)
{
	m_Direction = 0;
	m_TotalDist = 0;
	m_Tics = 0;
	m_WaitTics = 0;
	m_Close = false;
}

void DPolyDoor::UpdateToClient( ULONG ulClient )
{
	// [WS] Is our door paused?
	if (m_Tics != 0)
		SERVERCOMMANDS_DoPolyDoor( m_Type, 0, 0, 0, m_PolyObj, ulClient, SVCF_ONLYTHISCLIENT );
	else // [WS] Door is in motion, inform the client.
	{
		// [WS] Play the sound.
		SERVERCOMMANDS_PlayPolyobjSound( m_PolyObj, POLYSOUND_SEQ_DOOR );
		SERVERCOMMANDS_DoPolyDoor( m_Type, m_xSpeed, m_ySpeed, m_Speed, m_PolyObj, ulClient, SVCF_ONLYTHISCLIENT );
	}
}

LONG DPolyDoor::GetDirection ()
{
	return ( m_Direction );
}

void DPolyDoor::SetDirection (LONG lDirection)
{
	m_Direction = lDirection;
}

LONG DPolyDoor::GetTotalDist ()
{
	return ( m_TotalDist );
}

void DPolyDoor::SetTotalDist (LONG lDist)
{
	m_TotalDist = lDist;
}

bool DPolyDoor::GetClose ()
{
	return ( m_Close );
}

void DPolyDoor::SetClose (bool bClose)
{
	m_Close = bClose;
}


// ===== Polyobj Event Code =====

//==========================================================================
//
// T_RotatePoly
//
//==========================================================================

void DRotatePoly::Tick ()
{
	FPolyObj *poly = PO_GetPolyobj (m_PolyObj);
	if (poly == NULL) return;

	// [BC] For clients, just tick them and get out.
	if ( NETWORK_InClientMode( ) )
	{
		poly->RotatePolyobj( m_Speed );
		return;
	}

	if (poly->RotatePolyobj (m_Speed))
	{
		unsigned int absSpeed = abs (m_Speed);

		if (m_Dist == -1)
		{ // perpetual polyobj
			return;
		}
		m_Dist -= absSpeed;
		if (m_Dist == 0)
		{
			if (poly->specialdata == this)
			{
				poly->specialdata = NULL;
			}

			// [BC] Now that our destination has been reached, tell clients.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetPolyobjRotation( m_PolyObj, poly->angle );

			// [BC] Tell clients to stop the sound sequence, and destroy the rotate poly.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			{
				SERVERCOMMANDS_PlayPolyobjSound( m_PolyObj, POLYSOUND_STOPSEQUENCE );
				SERVERCOMMANDS_DestroyRotatePoly( m_PolyObj );
			}

			SN_StopSequence (poly);
			Destroy ();
		}
		else if ((unsigned int)m_Dist < absSpeed)
		{
			m_Speed = m_Dist * (m_Speed < 0 ? -1 : 1);
		}
	}
	else if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		// [WS] The poly object is blocked, tell clients the rotation!
	{
		FPolyObj *poly = PO_GetPolyobj (m_PolyObj);
		SERVERCOMMANDS_SetPolyobjRotation( m_PolyObj, poly->angle );
	}
}

//==========================================================================
//
// EV_RotatePoly
//
//==========================================================================


bool EV_RotatePoly (line_t *line, int polyNum, int speed, int byteAngle,
					int direction, bool overRide)
{
	int mirror;
	DRotatePoly *pe;
	FPolyObj *poly;

	if ( (poly = PO_GetPolyobj(polyNum)) )
	{
		if (poly->specialdata && !overRide)
		{ // poly is already moving
			return false;
		}
	}
	else
	{
		Printf("EV_RotatePoly: Invalid polyobj num: %d\n", polyNum);
		return false;
	}
	pe = new DRotatePoly (polyNum);
	if (byteAngle)
	{
		if (byteAngle == 255)
		{
			pe->m_Dist = ~0;
		}
		else
		{
			pe->m_Dist = byteAngle*(ANGLE_90/64); // Angle
		}
	}
	else
	{
		pe->m_Dist = ANGLE_MAX-1;
	}
	pe->m_Speed = (speed*direction*(ANGLE_90/64))>>3;
	poly->specialdata = pe;
	SN_StartSequence (poly, poly->seqType, SEQ_DOOR, 0);
	
	// [BC] If we're the server, tell clients to create the rotate poly.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_DoRotatePoly( pe->m_Speed, pe->m_PolyObj );

	while ( (mirror = poly->GetMirror()) )
	{
		poly = PO_GetPolyobj(mirror);
		if (poly == NULL)
		{
			Printf ("EV_RotatePoly: Invalid polyobj num: %d\n", polyNum);
			break;
		}
		if (poly && poly->specialdata && !overRide)
		{ // mirroring poly is already in motion
			break;
		}
		pe = new DRotatePoly (mirror);
		poly->specialdata = pe;
		if (byteAngle)
		{
			if (byteAngle == 255)
			{
				pe->m_Dist = ~0;
			}
			else
			{
				pe->m_Dist = byteAngle*(ANGLE_90/64); // Angle
			}
		}
		else
		{
			pe->m_Dist = ANGLE_MAX-1;
		}
		direction = -direction;
		pe->m_Speed = (speed*direction*(ANGLE_90/64))>>3;
		polyNum = mirror;
		SN_StartSequence (poly, poly->seqType, SEQ_DOOR, 0);

		// [BC] If we're the server, tell clients to create the rotate poly.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_DoRotatePoly( pe->m_Speed, pe->m_PolyObj );
	}
	return true;
}

//==========================================================================
//
// T_MovePoly
//
//==========================================================================

void DMovePoly::Tick ()
{
	FPolyObj *poly = PO_GetPolyobj (m_PolyObj);

	// [BC] For clients, just tick them and get out.
	if ( NETWORK_InClientMode( ) )
	{
		if ( poly )
			poly->MovePolyobj( m_xSpeed, m_ySpeed );
		return;
	}

	if (poly != NULL)
	{
		if (poly->MovePolyobj (m_xSpeed, m_ySpeed))
		{
			int absSpeed = abs (m_Speed);
			m_Dist -= absSpeed;
			if (m_Dist <= 0)
			{
				if (poly->specialdata == this)
				{
					poly->specialdata = NULL;
				}

				// [BC] Now that our destination has been reached, tell clients.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_SetPolyobjPosition( m_PolyObj, poly->StartSpot.x, poly->StartSpot.y );

				// [BC] Tell clients to stop the sound sequence, and destroy the move poly.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				{
					SERVERCOMMANDS_PlayPolyobjSound( m_PolyObj, POLYSOUND_STOPSEQUENCE );
					SERVERCOMMANDS_DestroyMovePoly( m_PolyObj );
				}

				SN_StopSequence (poly);
				Destroy ();
			}
			else if (m_Dist < absSpeed)
			{
				m_Speed = m_Dist * (m_Speed < 0 ? -1 : 1);
				m_xSpeed = FixedMul (m_Speed, finecosine[m_Angle]);
				m_ySpeed = FixedMul (m_Speed, finesine[m_Angle]);
			}
		}
		else if ( NETWORK_GetState ( ) == NETSTATE_SERVER )
			// [WS] The poly object is blocked, tell clients the position!
		{
			poly = PO_GetPolyobj (m_PolyObj);
			SERVERCOMMANDS_SetPolyobjPosition( m_PolyObj, poly->StartSpot.x, poly->StartSpot.x );
		}
	}
}

//==========================================================================
//
// EV_MovePoly
//
//==========================================================================

bool EV_MovePoly (line_t *line, int polyNum, int speed, angle_t angle,
				  fixed_t dist, bool overRide)
{
	int mirror;
	DMovePoly *pe;
	FPolyObj *poly;
	angle_t an;

	if ( (poly = PO_GetPolyobj(polyNum)) )
	{
		if (poly->specialdata && !overRide)
		{ // poly is already moving
			return false;
		}
	}
	else
	{
		Printf("EV_MovePoly: Invalid polyobj num: %d\n", polyNum);
		return false;
	}
	pe = new DMovePoly (polyNum);
	pe->m_Dist = dist; // Distance
	pe->m_Speed = speed;
	poly->specialdata = pe;

	an = angle;

	pe->m_Angle = an>>ANGLETOFINESHIFT;
	pe->m_xSpeed = FixedMul (pe->m_Speed, finecosine[pe->m_Angle]);
	pe->m_ySpeed = FixedMul (pe->m_Speed, finesine[pe->m_Angle]);
	SN_StartSequence (poly, poly->seqType, SEQ_DOOR, 0);

	// [BC] If we're the server, tell clients to create the move poly.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_DoMovePoly( pe->m_xSpeed, pe->m_ySpeed, pe->m_PolyObj );

	// Do not interpolate very fast moving polyobjects. The minimum tic count is
	// 3 instead of 2, because the moving crate effect in Massmouth 2, Hostitality
	// that this fixes isn't quite fast enough to move the crate back to its start
	// in just 1 tic.
	if (dist/speed <= 2)
	{
		pe->StopInterpolation ();
	}

	while ( (mirror = poly->GetMirror()) )
	{
		poly = PO_GetPolyobj(mirror);
		if (poly && poly->specialdata && !overRide)
		{ // mirroring poly is already in motion
			break;
		}
		pe = new DMovePoly (mirror);
		poly->specialdata = pe;
		pe->m_Dist = dist; // Distance
		pe->m_Speed = speed;
		an = an+ANGLE_180; // reverse the angle
		pe->m_Angle = an>>ANGLETOFINESHIFT;
		pe->m_xSpeed = FixedMul (pe->m_Speed, finecosine[pe->m_Angle]);
		pe->m_ySpeed = FixedMul (pe->m_Speed, finesine[pe->m_Angle]);
		polyNum = mirror;
		SN_StartSequence (poly, poly->seqType, SEQ_DOOR, 0);
		if (dist/speed <= 2)
		{
			pe->StopInterpolation ();
		}

		// [BC] If we're the server, tell clients to create the move poly.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_DoMovePoly( pe->m_xSpeed, pe->m_ySpeed, pe->m_PolyObj );
	}
	return true;
}

//==========================================================================
//
// T_PolyDoor
//
//==========================================================================

void DPolyDoor::Tick ()
{
	int absSpeed;
	FPolyObj *poly = PO_GetPolyobj (m_PolyObj);

	if (poly == NULL) return;

	// [BC] For clients, just tick them and get out.
	if ( NETWORK_InClientMode( ) )
	{
		switch ( m_Type )
		{
		case PODOOR_SLIDE:

			poly->MovePolyobj( m_xSpeed, m_ySpeed );
			break;
		case PODOOR_SWING:

			poly->RotatePolyobj( m_Speed );
			break;
		default:

			break;
		}

		return;
	}

	if (m_Tics)
	{
		if (!--m_Tics)
		{
			SN_StartSequence (poly, poly->seqType, SEQ_DOOR, m_Close);
		}
		return;
	}
	switch (m_Type)
	{
	case PODOOR_SLIDE:
		if (m_Dist <= 0 || poly->MovePolyobj (m_xSpeed, m_ySpeed))
		{
			// [WS] Inform clients that the door is closing.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER && m_TotalDist == m_Dist && m_Close )
				SERVERCOMMANDS_SetPolyDoorSpeedPosition( m_PolyObj,
															m_xSpeed,
															m_ySpeed,
															poly->StartSpot.x,
															poly->StartSpot.y );

			absSpeed = abs (m_Speed);
			m_Dist -= absSpeed;
			if (m_Dist <= 0)
			{
				// [BC] Tell clients to stop the sound sequence.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_PlayPolyobjSound( m_PolyObj, POLYSOUND_STOPSEQUENCE );

				SN_StopSequence (poly);
				if (!m_Close)
				{
					m_Dist = m_TotalDist;
					m_Close = true;
					m_Tics = m_WaitTics;
					m_Direction = (ANGLE_MAX>>ANGLETOFINESHIFT) - m_Direction;
					m_xSpeed = -m_xSpeed;
					m_ySpeed = -m_ySpeed;					

					// [WS] Inform clients the door has stopped.
					if ( NETWORK_GetState( ) == NETSTATE_SERVER )
						SERVERCOMMANDS_SetPolyDoorSpeedPosition( m_PolyObj, 0, 0, poly->StartSpot.x, poly->StartSpot.y );
				}
				else
				{
					if (poly->specialdata == this)
					{
						poly->specialdata = NULL;
					}

					// [BC] If we're the server, tell clients to destroy the poly door.
					if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					{
						SERVERCOMMANDS_DestroyPolyDoor( m_PolyObj );
						SERVERCOMMANDS_SetPolyobjPosition( m_PolyObj );
					}

					Destroy ();
				}
			}
		}
		else
		{
			if (poly->crush || !m_Close)
			{ // continue moving if the poly is a crusher, or is opening
				return;
			}
			else
			{ // open back up
				m_Dist = m_TotalDist - m_Dist;
				m_Direction = (ANGLE_MAX>>ANGLETOFINESHIFT)-
					m_Direction;
				m_xSpeed = -m_xSpeed;
				m_ySpeed = -m_ySpeed;
				m_Close = false;
				SN_StartSequence (poly, poly->seqType, SEQ_DOOR, 0);

				// [BC] Tell clients to play the sound sequence.
				// [WS] Tell clients to update the speed and position.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				{
					SERVERCOMMANDS_PlayPolyobjSound( m_PolyObj, POLYSOUND_SEQ_DOOR );
					SERVERCOMMANDS_SetPolyDoorSpeedPosition( m_PolyObj, m_xSpeed, m_ySpeed,
														poly->StartSpot.x,
														poly->StartSpot.y );
				}
			}
		}
		break;

	case PODOOR_SWING:
		if (poly->RotatePolyobj (m_Speed))
		{

			// [WS] Inform clients that the door is closing.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER && m_TotalDist == m_Dist && m_Close )
				SERVERCOMMANDS_SetPolyDoorSpeedRotation( m_PolyObj, m_Speed, poly->angle );

			absSpeed = abs (m_Speed);
			if (m_Dist == -1)
			{ // perpetual polyobj
				return;
			}
			m_Dist -= absSpeed;
			if (m_Dist <= 0)
			{
				SN_StopSequence (poly);

				// [BC] Tell clients to stop the sound sequence.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_PlayPolyobjSound( m_PolyObj, POLYSOUND_STOPSEQUENCE );

				if (!m_Close)
				{
					m_Dist = m_TotalDist;
					m_Close = true;
					m_Tics = m_WaitTics;
					m_Speed = -m_Speed;

					// [WS] Inform clients the door has stopped.
					if ( NETWORK_GetState( ) == NETSTATE_SERVER )
						SERVERCOMMANDS_SetPolyDoorSpeedRotation( m_PolyObj, 0, poly->angle );
				}
				else
				{
					if (poly->specialdata == this)
					{
						poly->specialdata = NULL;
					}

					// [BC] Tell clients to destroy the poly door.
					if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					{
						SERVERCOMMANDS_DestroyPolyDoor( m_PolyObj );
						SERVERCOMMANDS_SetPolyobjRotation( m_PolyObj );
					}

					Destroy ();
				}
			}
		}
		else
		{
			if(poly->crush || !m_Close)
			{ // continue moving if the poly is a crusher, or is opening
				return;
			}
			else
			{ // open back up and rewait
				m_Dist = m_TotalDist - m_Dist;
				m_Speed = -m_Speed;
				m_Close = false;
				SN_StartSequence (poly, poly->seqType, SEQ_DOOR, 0);

				// [BC] Tell clients to play the sound sequence.
				// [WS] Tell clients to update the speed and rotation.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				{
					SERVERCOMMANDS_PlayPolyobjSound( m_PolyObj, POLYSOUND_SEQ_DOOR );
					SERVERCOMMANDS_SetPolyDoorSpeedRotation( m_PolyObj, m_Speed, poly->angle );
				}
			}
		}			
		break;

	default:
		break;
	}
}

//==========================================================================
//
// EV_OpenPolyDoor
//
//==========================================================================

bool EV_OpenPolyDoor (line_t *line, int polyNum, int speed, angle_t angle,
					  int delay, int distance, podoortype_t type)
{
	int mirror;
	DPolyDoor *pd;
	FPolyObj *poly;

	if( (poly = PO_GetPolyobj(polyNum)) )
	{
		if (poly->specialdata)
		{ // poly is already moving
			return false;
		}
	}
	else
	{
		Printf("EV_OpenPolyDoor: Invalid polyobj num: %d\n", polyNum);
		return false;
	}
	pd = new DPolyDoor (polyNum, type);
	if (type == PODOOR_SLIDE)
	{
		pd->m_WaitTics = delay;
		pd->m_Speed = speed;
		pd->m_Dist = pd->m_TotalDist = distance; // Distance
		pd->m_Direction = angle >> ANGLETOFINESHIFT;
		pd->m_xSpeed = FixedMul (pd->m_Speed, finecosine[pd->m_Direction]);
		pd->m_ySpeed = FixedMul (pd->m_Speed, finesine[pd->m_Direction]);
		SN_StartSequence (poly, poly->seqType, SEQ_DOOR, 0);
	}
	else if (type == PODOOR_SWING)
	{
		pd->m_WaitTics = delay;
		pd->m_Direction = 1; // ADD:  PODOOR_SWINGL, PODOOR_SWINGR
		pd->m_Speed = (speed*pd->m_Direction*(ANGLE_90/64))>>3;
		pd->m_Dist = pd->m_TotalDist = angle;
		SN_StartSequence (poly, poly->seqType, SEQ_DOOR, 0);
	}

	poly->specialdata = pd;

	// [BC] Tell clients to create the poly door, and play a sound.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		SERVERCOMMANDS_DoPolyDoor( pd->m_Type, pd->m_xSpeed, pd->m_ySpeed, pd->m_Speed, pd->m_PolyObj );
		SERVERCOMMANDS_PlayPolyobjSound( polyNum, POLYSOUND_SEQ_DOOR );
	}

	while ( (mirror = poly->GetMirror()) )
	{
		poly = PO_GetPolyobj (mirror);
		if (poly && poly->specialdata)
		{ // mirroring poly is already in motion
			break;
		}
		pd = new DPolyDoor (mirror, type);
		poly->specialdata = pd;
		if (type == PODOOR_SLIDE)
		{
			pd->m_WaitTics = delay;
			pd->m_Speed = speed;
			pd->m_Dist = pd->m_TotalDist = distance; // Distance
			pd->m_Direction = (angle + ANGLE_180) >> ANGLETOFINESHIFT; // reverse the angle
			pd->m_xSpeed = FixedMul (pd->m_Speed, finecosine[pd->m_Direction]);
			pd->m_ySpeed = FixedMul (pd->m_Speed, finesine[pd->m_Direction]);
			SN_StartSequence (poly, poly->seqType, SEQ_DOOR, 0);
		}
		else if (type == PODOOR_SWING)
		{
			pd->m_WaitTics = delay;
			pd->m_Direction = -1; // ADD:  same as above
			pd->m_Speed = (speed*pd->m_Direction*(ANGLE_90/64))>>3;
			pd->m_Dist = pd->m_TotalDist = angle;
			SN_StartSequence (poly, poly->seqType, SEQ_DOOR, 0);
		}

		// [BC] Tell clients to create the poly door, and play a sound.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		{
			SERVERCOMMANDS_DoPolyDoor( pd->m_Type, pd->m_xSpeed, pd->m_ySpeed, pd->m_Speed, pd->m_PolyObj );
			SERVERCOMMANDS_PlayPolyobjSound( mirror, POLYSOUND_SEQ_DOOR );
		}

		polyNum = mirror;
	}
	return true;
}
	
// ===== Higher Level Poly Interface code =====

//==========================================================================
//
// PO_GetPolyobj
//
//==========================================================================

FPolyObj *PO_GetPolyobj (int polyNum)
{
	int i;

	for (i = 0; i < po_NumPolyobjs; i++)
	{
		if (polyobjs[i].tag == polyNum)
		{
			return &polyobjs[i];
		}
	}
	return NULL;
}


//==========================================================================
//
// [BC] GetPolyobjByIndex
//
//==========================================================================

FPolyObj *GetPolyobjByIndex( ULONG ulPolyIdx )
{
	if ( ulPolyIdx >= (ULONG)po_NumPolyobjs )
		return ( NULL );

	return ( &polyobjs[ulPolyIdx] );
}

//==========================================================================
//
// 
//
//==========================================================================

FPolyObj::FPolyObj()
{
	StartSpot.x = StartSpot.y = 0;
	angle = 0;
	tag = 0;
	memset(bbox, 0, sizeof(bbox));
	validcount = 0;
	crush = 0;
	bHurtOnTouch = false;
	seqType = 0;
	size = 0;
	subsectorlinks = NULL;
	specialdata = NULL;
	interpolation = NULL;
	SVIndex = -1;
}

//==========================================================================
//
// GetPolyobjMirror
//
//==========================================================================

int FPolyObj::GetMirror()
{
	return Linedefs[0]->args[1];
}

//==========================================================================
//
// ThrustMobj
//
//==========================================================================

void FPolyObj::ThrustMobj (AActor *actor, side_t *side)
{
	int thrustAngle;
	int thrustX;
	int thrustY;
	DPolyAction *pe;

	int force;

	if (!(actor->flags&MF_SHOOTABLE) && !actor->player)
	{
		return;
	}
	vertex_t *v1 = side->V1();
	vertex_t *v2 = side->V2();
	thrustAngle = (R_PointToAngle2 (v1->x, v1->y, v2->x, v2->y) - ANGLE_90) >> ANGLETOFINESHIFT;

	pe = static_cast<DPolyAction *>(specialdata);
	if (pe)
	{
		if (pe->IsKindOf (RUNTIME_CLASS (DRotatePoly)))
		{
			force = pe->GetSpeed() >> 8;
		}
		else
		{
			force = pe->GetSpeed() >> 3;
		}
		if (force < FRACUNIT)
		{
			force = FRACUNIT;
		}
		else if (force > 4*FRACUNIT)
		{
			force = 4*FRACUNIT;
		}
	}
	else
	{
		force = FRACUNIT;
	}

	thrustX = FixedMul (force, finecosine[thrustAngle]);
	thrustY = FixedMul (force, finesine[thrustAngle]);
	actor->velx += thrustX;
	actor->vely += thrustY;
	if (crush && !NETWORK_InClientMode( ))
	{
		if (bHurtOnTouch || !P_CheckMove (actor, actor->x + thrustX, actor->y + thrustY))
		{
			P_DamageMobj (actor, NULL, NULL, crush, NAME_Crush);
			P_TraceBleed (crush, actor);
		}
	}
	if (level.flags2 & LEVEL2_POLYGRIND) actor->Grind(false); // crush corpses that get caught in a polyobject's way
}

//==========================================================================
//
// UpdateSegBBox
//
//==========================================================================

void FPolyObj::UpdateBBox ()
{
	for(unsigned i=0;i<Linedefs.Size(); i++)
	{
		line_t *line = Linedefs[i];

		if (line->v1->x < line->v2->x)
		{
			line->bbox[BOXLEFT] = line->v1->x;
			line->bbox[BOXRIGHT] = line->v2->x;
		}
		else
		{
			line->bbox[BOXLEFT] = line->v2->x;
			line->bbox[BOXRIGHT] = line->v1->x;
		}
		if (line->v1->y < line->v2->y)
		{
			line->bbox[BOXBOTTOM] = line->v1->y;
			line->bbox[BOXTOP] = line->v2->y;
		}
		else
		{
			line->bbox[BOXBOTTOM] = line->v2->y;
			line->bbox[BOXTOP] = line->v1->y;
		}

		// Update the line's slopetype
		line->dx = line->v2->x - line->v1->x;
		line->dy = line->v2->y - line->v1->y;
		if (!line->dx)
		{
			line->slopetype = ST_VERTICAL;
		}
		else if (!line->dy)
		{
			line->slopetype = ST_HORIZONTAL;
		}
		else
		{
			line->slopetype = ((line->dy ^ line->dx) >= 0) ? ST_POSITIVE : ST_NEGATIVE;
		}
	}
	CalcCenter();
}

void FPolyObj::CalcCenter()
{
	SQWORD cx = 0, cy = 0;
	for(unsigned i=0;i<Vertices.Size(); i++)
	{
		cx += Vertices[i]->x;
		cy += Vertices[i]->y;
	}
	CenterSpot.x = (fixed_t)(cx / Vertices.Size());
	CenterSpot.y = (fixed_t)(cy / Vertices.Size());
}

//==========================================================================
//
// PO_MovePolyobj
//
//==========================================================================

bool FPolyObj::MovePolyobj (int x, int y, bool force)
{
	UnLinkPolyobj ();
	DoMovePolyobj (x, y);

	if (!force)
	{
		bool blocked = false;

		for(unsigned i=0;i < Sidedefs.Size(); i++)
		{
			if (CheckMobjBlocking(Sidedefs[i]))
			{
				blocked = true;
			}
		}
		if (blocked)
		{
			DoMovePolyobj (-x, -y);
			LinkPolyobj();
			return false;
		}
	}
	StartSpot.x += x;
	StartSpot.y += y;
	CenterSpot.x += x;
	CenterSpot.y += y;
	LinkPolyobj ();
	ClearSubsectorLinks();

	// [BC/BB] Polyobject has moved.
	bMoved = true;

	return true;
}

//==========================================================================
//
// DoMovePolyobj
//
//==========================================================================

void FPolyObj::DoMovePolyobj (int x, int y)
{
	for(unsigned i=0;i < Vertices.Size(); i++)
	{
		Vertices[i]->x += x;
		Vertices[i]->y += y;
		PrevPts[i].x += x;
		PrevPts[i].y += y;
	}
	for (unsigned i = 0; i < Linedefs.Size(); i++)
	{
		Linedefs[i]->bbox[BOXTOP] += y;
		Linedefs[i]->bbox[BOXBOTTOM] += y;
		Linedefs[i]->bbox[BOXLEFT] += x;
		Linedefs[i]->bbox[BOXRIGHT] += x;
	}
}

//==========================================================================
//
// RotatePt
//
//==========================================================================

static void RotatePt (int an, fixed_t *x, fixed_t *y, fixed_t startSpotX, fixed_t startSpotY)
{
	fixed_t tr_x = *x;
	fixed_t tr_y = *y;

	*x = DMulScale16 (tr_x, finecosine[an], -tr_y, finesine[an])+startSpotX;
	*y = DMulScale16 (tr_x, finesine[an], tr_y, finecosine[an])+startSpotY;
}

//==========================================================================
//
// PO_RotatePolyobj
//
//==========================================================================

bool FPolyObj::RotatePolyobj (angle_t angle)
{
	int an;
	bool blocked;

	an = (this->angle+angle)>>ANGLETOFINESHIFT;

	UnLinkPolyobj();

	for(unsigned i=0;i < Vertices.Size(); i++)
	{
		PrevPts[i].x = Vertices[i]->x;
		PrevPts[i].y = Vertices[i]->y;
		Vertices[i]->x = OriginalPts[i].x;
		Vertices[i]->y = OriginalPts[i].y;
		RotatePt(an, &Vertices[i]->x, &Vertices[i]->y, StartSpot.x,	StartSpot.y);
	}
	blocked = false;
	validcount++;
	UpdateBBox();

	for(unsigned i=0;i < Sidedefs.Size(); i++)
	{
		if (CheckMobjBlocking(Sidedefs[i]))
		{
			blocked = true;
		}
	}
	if (blocked)
	{
		for(unsigned i=0;i < Vertices.Size(); i++)
		{
			Vertices[i]->x = PrevPts[i].x;
			Vertices[i]->y = PrevPts[i].y;
		}
		UpdateBBox();
		LinkPolyobj();
		return false;
	}
	this->angle += angle;
	LinkPolyobj();
	ClearSubsectorLinks();

	// [BC] Polyobject has rotated.
	bRotated = true;

	return true;
}

//==========================================================================
//
// UnLinkPolyobj
//
//==========================================================================

void FPolyObj::UnLinkPolyobj ()
{
	polyblock_t *link;
	int i, j;
	int index;

	// remove the polyobj from each blockmap section
	for(j = bbox[BOXBOTTOM]; j <= bbox[BOXTOP]; j++)
	{
		index = j*bmapwidth;
		for(i = bbox[BOXLEFT]; i <= bbox[BOXRIGHT]; i++)
		{
			if(i >= 0 && i < bmapwidth && j >= 0 && j < bmapheight)
			{
				link = PolyBlockMap[index+i];
				while(link != NULL && link->polyobj != this)
				{
					link = link->next;
				}
				if(link == NULL)
				{ // polyobj not located in the link cell
					continue;
				}
				link->polyobj = NULL;
			}
		}
	}
}

//==========================================================================
//
// CheckMobjBlocking
//
//==========================================================================

bool FPolyObj::CheckMobjBlocking (side_t *sd)
{
	static TArray<AActor *> checker;
	FBlockNode *block;
	AActor *mobj;
	int i, j, k;
	int left, right, top, bottom;
	line_t *ld;
	bool blocked;

	// [WS] The client doesn't check if anything is blocking
	if ( NETWORK_InClientMode( ) )
		return false;

	ld = sd->linedef;
	top = (ld->bbox[BOXTOP]-bmaporgy) >> MAPBLOCKSHIFT;
	bottom = (ld->bbox[BOXBOTTOM]-bmaporgy) >> MAPBLOCKSHIFT;
	left = (ld->bbox[BOXLEFT]-bmaporgx) >> MAPBLOCKSHIFT;
	right = (ld->bbox[BOXRIGHT]-bmaporgx) >> MAPBLOCKSHIFT;

	blocked = false;
	checker.Clear();

	bottom = bottom < 0 ? 0 : bottom;
	bottom = bottom >= bmapheight ? bmapheight-1 : bottom;
	top = top < 0 ? 0 : top;
	top = top >= bmapheight  ? bmapheight-1 : top;
	left = left < 0 ? 0 : left;
	left = left >= bmapwidth ? bmapwidth-1 : left;
	right = right < 0 ? 0 : right;
	right = right >= bmapwidth ?  bmapwidth-1 : right;

	for (j = bottom*bmapwidth; j <= top*bmapwidth; j += bmapwidth)
	{
		for (i = left; i <= right; i++)
		{
			for (block = blocklinks[j+i]; block != NULL; block = block->NextActor)
			{
				mobj = block->Me;
				for (k = (int)checker.Size()-1; k >= 0; --k)
				{
					if (checker[k] == mobj)
					{
						break;
					}
				}
				if (k < 0)
				{
					checker.Push (mobj);
					if ((mobj->flags&MF_SOLID) && !(mobj->flags&MF_NOCLIP))
					{
						FBoundingBox box(mobj->x, mobj->y, mobj->radius);

						if (box.Right() <= ld->bbox[BOXLEFT]
							|| box.Left() >= ld->bbox[BOXRIGHT]
							|| box.Top() <= ld->bbox[BOXBOTTOM]
							|| box.Bottom() >= ld->bbox[BOXTOP])
						{
							continue;
						}
						if (box.BoxOnLineSide(ld) != -1)
						{
							continue;
						}
						ThrustMobj (mobj, sd);
						blocked = true;
					}
				}
			}
		}
	}
	return blocked;
}

//==========================================================================
//
// LinkPolyobj
//
//==========================================================================

void FPolyObj::LinkPolyobj ()
{
	int leftX, rightX;
	int topY, bottomY;
	polyblock_t **link;
	polyblock_t *tempLink;

	// calculate the polyobj bbox
	vertex_t *vt = Sidedefs[0]->V1();
	rightX = leftX = vt->x;
	topY = bottomY = vt->y;

	Bounds.ClearBox();
	for(unsigned i = 1; i < Sidedefs.Size(); i++)
	{
		vt = Sidedefs[i]->V1();
		Bounds.AddToBox(vt->x, vt->y);
	}
	bbox[BOXRIGHT] = (Bounds.Right() - bmaporgx) >> MAPBLOCKSHIFT;
	bbox[BOXLEFT] = (Bounds.Left() - bmaporgx) >> MAPBLOCKSHIFT;
	bbox[BOXTOP] = (Bounds.Top() - bmaporgy) >> MAPBLOCKSHIFT;
	bbox[BOXBOTTOM] = (Bounds.Bottom() - bmaporgy) >> MAPBLOCKSHIFT;
	// add the polyobj to each blockmap section
	for(int j = bbox[BOXBOTTOM]*bmapwidth; j <= bbox[BOXTOP]*bmapwidth;
		j += bmapwidth)
	{
		for(int i = bbox[BOXLEFT]; i <= bbox[BOXRIGHT]; i++)
		{
			if(i >= 0 && i < bmapwidth && j >= 0 && j < bmapheight*bmapwidth)
			{
				link = &PolyBlockMap[j+i];
				if(!(*link))
				{ // Create a new link at the current block cell
					*link = new polyblock_t;
					(*link)->next = NULL;
					(*link)->prev = NULL;
					(*link)->polyobj = this;
					continue;
				}
				else
				{
					tempLink = *link;
					while(tempLink->next != NULL && tempLink->polyobj != NULL)
					{
						tempLink = tempLink->next;
					}
				}
				if(tempLink->polyobj == NULL)
				{
					tempLink->polyobj = this;
					continue;
				}
				else
				{
					tempLink->next = new polyblock_t;
					tempLink->next->next = NULL;
					tempLink->next->prev = tempLink;
					tempLink->next->polyobj = this;
				}
			}
			// else, don't link the polyobj, since it's off the map
		}
	}
}

//===========================================================================
//
// PO_ClosestPoint
//
// Given a point (x,y), returns the point (ox,oy) on the polyobject's walls
// that is nearest to (x,y). Also returns the seg this point came from.
//
//===========================================================================

void FPolyObj::ClosestPoint(fixed_t fx, fixed_t fy, fixed_t &ox, fixed_t &oy, side_t **side) const
{
	unsigned int i;
	double x = fx, y = fy;
	double bestdist = HUGE_VAL;
	double bestx = 0, besty = 0;
	side_t *bestline = NULL;

	for (i = 0; i < Sidedefs.Size(); ++i)
	{
		vertex_t *v1 = Sidedefs[i]->V1();
		vertex_t *v2 = Sidedefs[i]->V2();
		double a = v2->x - v1->x;
		double b = v2->y - v1->y;
		double den = a*a + b*b;
		double ix, iy, dist;

		if (den == 0)
		{ // Line is actually a point!
			ix = v1->x;
			iy = v1->y;
		}
		else
		{
			double num = (x - v1->x) * a + (y - v1->y) * b;
			double u = num / den;
			if (u <= 0)
			{
				ix = v1->x;
				iy = v1->y;
			}
			else if (u >= 1)
			{
				ix = v2->x;
				iy = v2->y;
			}
			else
			{
				ix = v1->x + u * a;
				iy = v1->y + u * b;
			}
		}
		a = (ix - x);
		b = (iy - y);
		dist = a*a + b*b;
		if (dist < bestdist)
		{
			bestdist = dist;
			bestx = ix;
			besty = iy;
			bestline = Sidedefs[i];
		}
	}
	ox = fixed_t(bestx);
	oy = fixed_t(besty);
	if (side != NULL)
	{
		*side = bestline;
	}
}

vertex_t *FPolyObj::GetNewVertex()
{
	if (SVIndex == ~0u || SplitVertices[SVIndex]->used == 10)
	{
		SVIndex++;
		if (SVIndex >= SplitVertices.Size())
		{
			SplitVertices.Push(new FPolyVertexBlock);
		}
		SplitVertices[SVIndex]->clear();
	}
	return &SplitVertices[SVIndex]->vertices[SplitVertices[SVIndex]->used++];
}

//==========================================================================
//
// InitBlockMap
//
//==========================================================================

static void InitBlockMap (void)
{
	int i;

	PolyBlockMap = new polyblock_t *[bmapwidth*bmapheight];
	memset (PolyBlockMap, 0, bmapwidth*bmapheight*sizeof(polyblock_t *));

	for (i = 0; i < po_NumPolyobjs; i++)
	{
		polyobjs[i].LinkPolyobj();
	}
}

//==========================================================================
//
// InitSideLists [RH]
//
// Group sides by vertex and collect side that are known to belong to a
// polyobject so that they can be initialized fast.
//==========================================================================

static void InitSideLists ()
{
	for (int i = 0; i < numsides; ++i)
	{
		if (sides[i].linedef != NULL &&
			(sides[i].linedef->special == Polyobj_StartLine ||
			 sides[i].linedef->special == Polyobj_ExplicitLine))
		{
			KnownPolySides.Push (i);
		}
	}
}

//==========================================================================
//
// KillSideLists [RH]
//
//==========================================================================

static void KillSideLists ()
{
	KnownPolySides.Clear ();
	KnownPolySides.ShrinkToFit ();
}

//==========================================================================
//
// IterFindPolySides
//
//==========================================================================

static void IterFindPolySides (FPolyObj *po, side_t *side)
{
	SDWORD j;
	int i;
	vertex_t *v1 = side->V1();

	po->Sidedefs.Push(side);

	// This for loop exists solely to avoid infinitely looping on badly
	// formed polyobjects.
	for (i = 0; i < numsides; i++)
	{
		int v2 = int(side->V2() - vertexes);
		j = side->RightSide;

		if (j == NO_SIDE)
		{
			break;
		}
		side = &sides[j];

		if (side->V1() == v1)
		{
			return;
		}
		po->Sidedefs.Push(side);
	}
	I_Error ("IterFindPolySides: Non-closed Polyobj at linedef %d.\n",
		side->linedef-lines);
}


//==========================================================================
//
// SpawnPolyobj
//
//==========================================================================

static void SpawnPolyobj (int index, int tag, int type)
{
	unsigned int ii;
	int i;
	int j;
	FPolyObj *po = &polyobjs[index];

	for (ii = 0; ii < KnownPolySides.Size(); ++ii)
	{
		i = KnownPolySides[ii];
		if (i < 0)
		{
			continue;
		}

		side_t *sd = &sides[i];
		
		if (sd->linedef->special == Polyobj_StartLine &&
			sd->linedef->args[0] == tag)
		{
			if (po->Sidedefs.Size() > 0)
			{
				I_Error ("SpawnPolyobj: Polyobj %d already spawned.\n", tag);
			}
			sd->linedef->special = 0;
			sd->linedef->args[0] = 0;
			IterFindPolySides(&polyobjs[index], sd);
			po->crush = (type != PO_SPAWN_TYPE) ? 3 : 0;
			po->bHurtOnTouch = (type == PO_SPAWNHURT_TYPE);
			po->tag = tag;
			po->seqType = sd->linedef->args[2];
			if (po->seqType < 0 || po->seqType > 63)
			{
				po->seqType = 0;
			}
			// [BB] Init Zandronum stuff.
			polyobjs[index].bMoved = false;
			polyobjs[index].bRotated = false;
			break;
		}
	}
	if (po->Sidedefs.Size() == 0)
	{ 
		// didn't find a polyobj through PO_LINE_START
		TArray<side_t *> polySideList;
		unsigned int psIndexOld;
		for (j = 1; j < PO_MAXPOLYSEGS; j++)
		{
			psIndexOld = po->Sidedefs.Size();
			for (ii = 0; ii < KnownPolySides.Size(); ++ii)
			{
				i = KnownPolySides[ii];

				if (i >= 0 &&
					sides[i].linedef->special == Polyobj_ExplicitLine &&
					sides[i].linedef->args[0] == tag)
				{
					if (!sides[i].linedef->args[1])
					{
						I_Error ("SpawnPolyobj: Explicit line missing order number (probably %d) in poly %d.\n",
							j+1, tag);
					}
					if (sides[i].linedef->args[1] == j)
					{
						po->Sidedefs.Push (&sides[i]);
					}
				}
			}
			// Clear out any specials for these segs...we cannot clear them out
			// 	in the above loop, since we aren't guaranteed one seg per linedef.
			for (ii = 0; ii < KnownPolySides.Size(); ++ii)
			{
				i = KnownPolySides[ii];
				if (i >= 0 &&
					sides[i].linedef->special == Polyobj_ExplicitLine &&
					sides[i].linedef->args[0] == tag && sides[i].linedef->args[1] == j)
				{
					sides[i].linedef->special = 0;
					sides[i].linedef->args[0] = 0;
					KnownPolySides[ii] = -1;
				}
			}
			if (po->Sidedefs.Size() == psIndexOld)
			{ // Check if an explicit line order has been skipped.
			  // A line has been skipped if there are any more explicit
			  // lines with the current tag value. [RH] Can this actually happen?
				for (ii = 0; ii < KnownPolySides.Size(); ++ii)
				{
					i = KnownPolySides[ii];
					if (i >= 0 &&
						sides[i].linedef->special == Polyobj_ExplicitLine &&
						sides[i].linedef->args[0] == tag)
					{
						I_Error ("SpawnPolyobj: Missing explicit line %d for poly %d\n",
							j, tag);
					}
				}
			}
		}
		if (po->Sidedefs.Size() > 0)
		{
			po->crush = (type != PO_SPAWN_TYPE) ? 3 : 0;
			po->bHurtOnTouch = (type == PO_SPAWNHURT_TYPE);
			po->tag = tag;
			po->seqType = po->Sidedefs[0]->linedef->args[3];
			// [BB] Init Zandronum stuff
			po->bMoved = false;
			po->bRotated = false;
			// Next, change the polyobj's first line to point to a mirror
			//		if it exists
			po->Sidedefs[0]->linedef->args[1] =
				po->Sidedefs[0]->linedef->args[2];
		}
		else
			I_Error ("SpawnPolyobj: Poly %d does not exist\n", tag);
	}

	validcount++;	
	for(unsigned int i=0; i<po->Sidedefs.Size(); i++)
	{
		line_t *l = po->Sidedefs[i]->linedef;

		if (l->validcount != validcount)
		{
			l->validcount = validcount;
			po->Linedefs.Push(l);

			vertex_t *v = l->v1;
			int j;
			for(j = po->Vertices.Size() - 1; j >= 0; j--)
			{
				if (po->Vertices[j] == v) break;
			}
			if (j < 0) po->Vertices.Push(v);

			v = l->v2;
			for(j = po->Vertices.Size() - 1; j >= 0; j--)
			{
				if (po->Vertices[j] == v) break;
			}
			if (j < 0) po->Vertices.Push(v);

		}
	}
	po->Sidedefs.ShrinkToFit();
	po->Linedefs.ShrinkToFit();
	po->Vertices.ShrinkToFit();
}

//==========================================================================
//
// TranslateToStartSpot
//
//==========================================================================

static void TranslateToStartSpot (int tag, int originX, int originY)
{
	FPolyObj *po;
	int deltaX;
	int deltaY;

	po = NULL;
	for (int i = 0; i < po_NumPolyobjs; i++)
	{
		if (polyobjs[i].tag == tag)
		{
			po = &polyobjs[i];
			break;
		}
	}
	if (po == NULL)
	{ // didn't match the tag with a polyobj tag
		I_Error("TranslateToStartSpot: Unable to match polyobj tag: %d\n", tag);
	}
	if (po->Sidedefs.Size() == 0)
	{
		I_Error ("TranslateToStartSpot: Anchor point located without a StartSpot point: %d\n", tag);
	}
	po->OriginalPts.Resize(po->Sidedefs.Size());
	po->PrevPts.Resize(po->Sidedefs.Size());
	deltaX = originX - po->StartSpot.x;
	deltaY = originY - po->StartSpot.y;

	for (unsigned i = 0; i < po->Sidedefs.Size(); i++)
	{
		po->Sidedefs[i]->Flags |= WALLF_POLYOBJ;
	}
	for (unsigned i = 0; i < po->Linedefs.Size(); i++)
	{
		po->Linedefs[i]->bbox[BOXTOP] -= deltaY;
		po->Linedefs[i]->bbox[BOXBOTTOM] -= deltaY;
		po->Linedefs[i]->bbox[BOXLEFT] -= deltaX;
		po->Linedefs[i]->bbox[BOXRIGHT] -= deltaX;
	}
	for (unsigned i = 0; i < po->Vertices.Size(); i++)
	{
		po->Vertices[i]->x -= deltaX;
		po->Vertices[i]->y -= deltaY;
		po->OriginalPts[i].x = po->Vertices[i]->x - po->StartSpot.x;
		po->OriginalPts[i].y = po->Vertices[i]->y - po->StartSpot.y;
	}
	po->CalcCenter();
	// subsector assignment no longer done here.
	// Polyobjects will be sorted into the subsectors each frame before rendering them.
}

//==========================================================================
//
// PO_Init
//
//==========================================================================

void PO_Init (void)
{
	// [RH] Hexen found the polyobject-related things by reloading the map's
	//		THINGS lump here and scanning through it. I have P_SpawnMapThing()
	//		record those things instead, so that in here we simply need to
	//		look at the polyspawns list.
	polyspawns_t *polyspawn, **prev;
	int polyIndex;

	// [RH] Make this faster
	InitSideLists ();

	polyobjs = new FPolyObj[po_NumPolyobjs];

	polyIndex = 0; // index polyobj number
	// Find the startSpot points, and spawn each polyobj
	for (polyspawn = polyspawns, prev = &polyspawns; polyspawn;)
	{
		// 9301 (3001) = no crush, 9302 (3002) = crushing, 9303 = hurting touch
		if (polyspawn->type == PO_SPAWN_TYPE ||
			polyspawn->type == PO_SPAWNCRUSH_TYPE ||
			polyspawn->type == PO_SPAWNHURT_TYPE)
		{ 
			// Polyobj StartSpot Pt.
			polyobjs[polyIndex].StartSpot.x = polyspawn->x;
			polyobjs[polyIndex].StartSpot.y = polyspawn->y;
			// [BB] Save the original start spot, so that we can reset to it in GAME_ResetMap.
			polyobjs[polyIndex].SavedStartSpot[0] = polyobjs[polyIndex].StartSpot.x;
			polyobjs[polyIndex].SavedStartSpot[1] = polyobjs[polyIndex].StartSpot.y;
			SpawnPolyobj(polyIndex, polyspawn->angle, polyspawn->type);
			polyIndex++;
			*prev = polyspawn->next;
			delete polyspawn;
			polyspawn = *prev;
		} 
		else 
		{
			prev = &polyspawn->next;
			polyspawn = polyspawn->next;
		}
	}
	for (polyspawn = polyspawns; polyspawn;)
	{
		polyspawns_t *next = polyspawn->next;
		if (polyspawn->type == PO_ANCHOR_TYPE)
		{ 
			// Polyobj Anchor Pt.
			TranslateToStartSpot (polyspawn->angle, polyspawn->x, polyspawn->y);
		}
		delete polyspawn;
		polyspawn = next;
	}
	polyspawns = NULL;

	// check for a startspot without an anchor point
	for (polyIndex = 0; polyIndex < po_NumPolyobjs; polyIndex++)
	{
		if (polyobjs[polyIndex].OriginalPts.Size() == 0)
		{
			I_Error ("PO_Init: StartSpot located without an Anchor point: %d\n",
				polyobjs[polyIndex].tag);
		}
	}
	InitBlockMap();

	// [RH] Don't need the seg lists anymore
	KillSideLists ();

	// We still need to flag the segs of the polyobj's sidedefs so that they are excluded from rendering.
	for(int i=0;i<numsegs;i++)
	{
		segs[i].bPolySeg = (segs[i].sidedef != NULL && segs[i].sidedef->Flags & WALLF_POLYOBJ);
	}

	for(int i=0;i<numnodes;i++)
	{
		node_t *no = &nodes[i];
		double fdx = (double)no->dx;
		double fdy = (double)no->dy;
		no->len = (float)sqrt(fdx * fdx + fdy * fdy);
	}
}

//==========================================================================
//
// PO_Busy
//
//==========================================================================

bool PO_Busy (int polyobj)
{
	FPolyObj *poly;

	poly = PO_GetPolyobj (polyobj);
	return (poly != NULL && poly->specialdata != NULL);
}



//==========================================================================
//
//
//
//==========================================================================

void FPolyObj::ClearSubsectorLinks()
{
	for(unsigned i=0; i<SplitVertices.Size(); i++)
	{
		SplitVertices[i]->clear();
	}
	SVIndex = -1;
	while (subsectorlinks != NULL)
	{
		assert(subsectorlinks->state == 1337);

		FPolyNode *next = subsectorlinks->snext;

		if (subsectorlinks->pnext != NULL)
		{
			assert(subsectorlinks->pnext->state == 1337);
			subsectorlinks->pnext->pprev = subsectorlinks->pprev;
		}

		if (subsectorlinks->pprev != NULL)
		{
			assert(subsectorlinks->pprev->state == 1337);
			subsectorlinks->pprev->pnext = subsectorlinks->pnext;
		}
		else
		{
			subsectorlinks->subsector->polys = subsectorlinks->pnext;
		}
		subsectorlinks->state = -1;
		delete subsectorlinks;
		subsectorlinks = next;
	}
	subsectorlinks = NULL;
}

void FPolyObj::ClearAllSubsectorLinks()
{
	for(int i=0;i<po_NumPolyobjs;i++)
	{
		polyobjs[i].ClearSubsectorLinks();
	}
}

//==========================================================================
//
// GetIntersection
//
// adapted from P_InterceptVector
//
//==========================================================================

static bool GetIntersection(seg_t *seg, node_t *bsp, vertex_t *v)
{
	double frac;
	double num;
	double den;

	double v2x = (double)seg->v1->x;
	double v2y = (double)seg->v1->y;
	double v2dx = (double)(seg->v2->x - seg->v1->x);
	double v2dy = (double)(seg->v2->y - seg->v1->y);
	double v1x = (double)bsp->x;
	double v1y = (double)bsp->y;
	double v1dx = (double)bsp->dx;
	double v1dy = (double)bsp->dy;
		
	den = v1dy*v2dx - v1dx*v2dy;

	if (den == 0)
		return false;		// parallel
	
	num = (v1x - v2x)*v1dy + (v2y - v1y)*v1dx;
	frac = num / den;

	if (frac < 0. || frac > 1.) return false;

	v->x = xs_RoundToInt(v2x + frac * v2dx);
	v->y = xs_RoundToInt(v2y + frac * v2dy);
	return true;
}

//==========================================================================
//
// PartitionDistance
//
// Determine the distance of a vertex to a node's partition line.
//
//==========================================================================

static double PartitionDistance(vertex_t *vt, node_t *node)
{	
	return fabs(double(-node->dy) * (vt->x - node->x) + double(node->dx) * (vt->y - node->y)) / node->len;
}

//==========================================================================
//
// AddToBBox
//
//==========================================================================

static void AddToBBox(fixed_t child[4], fixed_t parent[4])
{
	if (child[BOXTOP] > parent[BOXTOP])
	{
		parent[BOXTOP] = child[BOXTOP];
	}
	if (child[BOXBOTTOM] < parent[BOXBOTTOM])
	{
		parent[BOXBOTTOM] = child[BOXBOTTOM];
	}
	if (child[BOXLEFT] < parent[BOXLEFT])
	{
		parent[BOXLEFT] = child[BOXLEFT];
	}
	if (child[BOXRIGHT] > parent[BOXRIGHT])
	{
		parent[BOXRIGHT] = child[BOXRIGHT];
	}
}

//==========================================================================
//
// AddToBBox
//
//==========================================================================

static void AddToBBox(vertex_t *v, fixed_t bbox[4])
{
	if (v->x < bbox[BOXLEFT])
	{
		bbox[BOXLEFT] = v->x;
	}
	if (v->x > bbox[BOXRIGHT])
	{
		bbox[BOXRIGHT] = v->x;
	}
	if (v->y < bbox[BOXBOTTOM])
	{
		bbox[BOXBOTTOM] = v->y;
	}
	if (v->y > bbox[BOXTOP])
	{
		bbox[BOXTOP] = v->y;
	}
}

//==========================================================================
//
// SplitPoly
//
//==========================================================================

static void SplitPoly(FPolyNode *pnode, void *node, fixed_t bbox[4])
{
	static TArray<seg_t> lists[2];
	static const double POLY_EPSILON = 0.3125;

	if (!((size_t)node & 1))  // Keep going until found a subsector
	{
		node_t *bsp = (node_t *)node;

		int centerside = R_PointOnSide(pnode->poly->CenterSpot.x, pnode->poly->CenterSpot.y, bsp);

		lists[0].Clear();
		lists[1].Clear();
		for(unsigned i=0;i<pnode->segs.Size(); i++)
		{
			seg_t *seg = &pnode->segs[i];

			// Parts of the following code were taken from Eternity and are
			// being used with permission.

			// get distance of vertices from partition line
			// If the distance is too small, we may decide to
			// change our idea of sidedness.
			double dist_v1 = PartitionDistance(seg->v1, bsp);
			double dist_v2 = PartitionDistance(seg->v2, bsp);

			// If the distances are less than epsilon, consider the points as being
			// on the same side as the polyobj origin. Why? People like to build
			// polyobject doors flush with their door tracks. This breaks using the
			// usual assumptions.
			

			// Addition to Eternity code: We must also check any seg with only one
			// vertex inside the epsilon threshold. If not, these lines will get split but
			// adjoining ones with both vertices inside the threshold won't thus messing up
			// the order in which they get drawn.

			if(dist_v1 <= POLY_EPSILON)
			{
				if (dist_v2 <= POLY_EPSILON)
				{
					lists[centerside].Push(*seg);
				}
				else
				{
					int side = R_PointOnSide(seg->v2->x, seg->v2->y, bsp);
					lists[side].Push(*seg);
				}
			}
			else if (dist_v2 <= POLY_EPSILON)
			{
				int side = R_PointOnSide(seg->v1->x, seg->v1->y, bsp);
				lists[side].Push(*seg);
			}
			else 
			{
				int side1 = R_PointOnSide(seg->v1->x, seg->v1->y, bsp);
				int side2 = R_PointOnSide(seg->v2->x, seg->v2->y, bsp);

				if(side1 != side2)
				{
					// if the partition line crosses this seg, we must split it.

					vertex_t  *vert = pnode->poly->GetNewVertex();

					if (GetIntersection(seg, bsp, vert))
					{
						lists[0].Push(*seg);
						lists[1].Push(*seg);
						lists[side1].Last().v2 = vert;
						lists[side2].Last().v1 = vert;
					}
					else
					{
						// should never happen
						lists[side1].Push(*seg);
					}
				}
				else 
				{
					// both points on the same side.
					lists[side1].Push(*seg);
				}
			}
		}
		if (lists[1].Size() == 0)
		{
			SplitPoly(pnode, bsp->children[0], bsp->bbox[0]);
			AddToBBox(bsp->bbox[0], bbox);
		}
		else if (lists[0].Size() == 0)
		{
			SplitPoly(pnode, bsp->children[1], bsp->bbox[1]);
			AddToBBox(bsp->bbox[1], bbox);
		}
		else
		{
			// create the new node 
			FPolyNode *newnode = new FPolyNode;
			newnode->state = 1337;
			newnode->poly = pnode->poly;
			newnode->pnext = NULL;
			newnode->pprev = NULL;
			newnode->subsector = NULL;
			newnode->snext = NULL;
			newnode->segs = lists[1];

			// set segs for original node
			pnode->segs = lists[0];
		
			// recurse back side
			SplitPoly(newnode, bsp->children[1], bsp->bbox[1]);
			
			// recurse front side
			SplitPoly(pnode, bsp->children[0], bsp->bbox[0]);

			AddToBBox(bsp->bbox[0], bbox);
			AddToBBox(bsp->bbox[1], bbox);
		}
	}
	else
	{
		// we reached a subsector so we can link the node with this subsector
		subsector_t *sub = (subsector_t *)((BYTE *)node - 1);

		// Link node to subsector
		pnode->pnext = sub->polys;
		if (pnode->pnext != NULL) 
		{
			assert(pnode->pnext->state == 1337);
			pnode->pnext->pprev = pnode;
		}
		pnode->pprev = NULL;
		sub->polys = pnode;

		// link node to polyobject
		pnode->snext = pnode->poly->subsectorlinks;
		pnode->poly->subsectorlinks = pnode;
		pnode->subsector = sub;

		// calculate bounding box for this polynode
		assert(pnode->segs.Size() != 0);
		fixed_t subbbox[4] = { FIXED_MIN, FIXED_MAX, FIXED_MAX, FIXED_MIN };

		for (unsigned i = 0; i < pnode->segs.Size(); ++i)
		{
			AddToBBox(pnode->segs[i].v1, subbbox);
			AddToBBox(pnode->segs[i].v2, subbbox);
		}
		// Potentially expand the parent node's bounding box to contain these bits of polyobject.
		AddToBBox(subbbox, bbox);
	}
}

//==========================================================================
//
// 
//
//==========================================================================

void FPolyObj::CreateSubsectorLinks()
{
	FPolyNode *node = new FPolyNode;
	fixed_t dummybbox[4];

	node->state = 1337;
	node->poly = this;
	node->pnext = NULL;
	node->pprev = NULL;
	node->snext = NULL;
	node->segs.Resize(Sidedefs.Size());

	for(unsigned i=0; i<Sidedefs.Size(); i++)
	{
		seg_t *seg = &node->segs[i];
		side_t *side = Sidedefs[i];

		seg->v1 = side->V1();
		seg->v2 = side->V2();
		seg->sidedef = side;
		seg->linedef = side->linedef;
		seg->frontsector = side->sector;
		seg->backsector = side->linedef->frontsector == side->sector? 
			side->linedef->backsector : side->linedef->frontsector;
		seg->Subsector = NULL;
		seg->PartnerSeg = NULL;
		seg->bPolySeg = true;
	}
	SplitPoly(node, nodes + numnodes - 1, dummybbox);
}

//==========================================================================
//
//
//
//==========================================================================

void PO_LinkToSubsectors()
{
	for (int i = 0; i < po_NumPolyobjs; i++)
	{
		if (polyobjs[i].subsectorlinks == NULL)
		{
			polyobjs[i].CreateSubsectorLinks();
		}
	}
}