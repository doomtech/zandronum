#ifndef __A_SHAREDGLOBAL_H__
#define __A_SHAREDGLOBAL_H__

#include "info.h"
#include "actor.h"
#include "a_pickups.h"

class FDecalTemplate;
struct vertex_t;
struct side_t;
struct F3DFloor;

extern void P_SpawnDirt (AActor *actor, fixed_t radius);

class DBaseDecal : public DThinker
{
	DECLARE_CLASS (DBaseDecal, DThinker)
	HAS_OBJECT_POINTERS
public:
	DBaseDecal ();
	DBaseDecal (fixed_t z);
	DBaseDecal (int statnum, fixed_t z);
	DBaseDecal (const AActor *actor);
	DBaseDecal (const DBaseDecal *basis);

	void Serialize (FArchive &arc);
	void Destroy ();
	FTextureID StickToWall (side_t *wall, fixed_t x, fixed_t y, F3DFloor * ffloor);
	fixed_t GetRealZ (const side_t *wall) const;
	void SetShade (DWORD rgb);
	void SetShade (int r, int g, int b);
	void Spread (const FDecalTemplate *tpl, side_t *wall, fixed_t x, fixed_t y, fixed_t z, F3DFloor * ffloor);
	void GetXY (side_t *side, fixed_t &x, fixed_t &y) const;

	static void SerializeChain (FArchive &arc, DBaseDecal **firstptr);

	DBaseDecal *WallNext, **WallPrev;

	fixed_t LeftDistance;
	fixed_t Z;
	fixed_t ScaleX, ScaleY;
	fixed_t Alpha;
	DWORD AlphaColor;
	int Translation;
	FTextureID PicNum;
	DWORD RenderFlags;
	FRenderStyle RenderStyle;
	sector_t * Sector;	// required for 3D floors

protected:
	virtual DBaseDecal *CloneSelf (const FDecalTemplate *tpl, fixed_t x, fixed_t y, fixed_t z, side_t *wall, F3DFloor * ffloor) const;
	void CalcFracPos (side_t *wall, fixed_t x, fixed_t y);
	void Remove ();

	static void SpreadLeft (fixed_t r, vertex_t *v1, side_t *feelwall, F3DFloor *ffloor);
	static void SpreadRight (fixed_t r, side_t *feelwall, fixed_t wallsize, F3DFloor *ffloor);
};

class DImpactDecal : public DBaseDecal
{
	DECLARE_CLASS (DImpactDecal, DBaseDecal)
public:
	DImpactDecal (fixed_t z);
	DImpactDecal (side_t *wall, const FDecalTemplate *templ);

	static DImpactDecal *StaticCreate (const char *name, fixed_t x, fixed_t y, fixed_t z, side_t *wall, F3DFloor * ffloor, PalEntry color=0);
	static DImpactDecal *StaticCreate (const FDecalTemplate *tpl, fixed_t x, fixed_t y, fixed_t z, side_t *wall, F3DFloor * ffloor, PalEntry color=0);

	void BeginPlay ();
	void Destroy ();

	void Serialize (FArchive &arc);
	static void SerializeTime (FArchive &arc);

protected:
	DBaseDecal *CloneSelf (const FDecalTemplate *tpl, fixed_t x, fixed_t y, fixed_t z, side_t *wall, F3DFloor * ffloor) const;
	static void CheckMax ();

private:
	DImpactDecal();
};

class ATeleportFog : public AActor
{
	DECLARE_CLASS (ATeleportFog, AActor)
public:
	void PostBeginPlay ();
};

class ASkyViewpoint : public AActor
{
	DECLARE_CLASS (ASkyViewpoint, AActor)
	HAS_OBJECT_POINTERS
public:
	void Serialize (FArchive &arc);
	void PostBeginPlay ();
	void Destroy ();
	bool bInSkybox;
	bool bAlways;
	TObjPtr<ASkyViewpoint> Mate;
	fixed_t PlaneAlpha;
};

class AStackPoint : public ASkyViewpoint
{
	DECLARE_CLASS (AStackPoint, ASkyViewpoint)
public:
	void BeginPlay ();
};

class DFlashFader : public DThinker
{
	DECLARE_CLASS (DFlashFader, DThinker)
	HAS_OBJECT_POINTERS
public:
	DFlashFader (float r1, float g1, float b1, float a1,
				 float r2, float g2, float b2, float a2,
				 float time, AActor *who);
	void Destroy ();
	void Serialize (FArchive &arc);
	void Tick ();
	AActor *WhoFor() { return ForWho; }
	void Cancel ();

protected:
	float Blends[2][4];
	int TotalTics;
	int StartTic;
	TObjPtr<AActor> ForWho;

	void SetBlend (float time);
	DFlashFader ();
};

// [BC]
class ATeamItem : public AInventory
{
	DECLARE_CLASS( ATeamItem, AInventory )
public:
	virtual bool ShouldRespawn( );
	virtual bool TryPickup( AActor *&pToucher );
	virtual bool HandlePickup( AInventory *pItem );
	virtual LONG AllowFlagPickup( AActor *pToucher );
	virtual void AnnounceFlagPickup( AActor *pToucher );
	virtual void DisplayFlagTaken( AActor *pToucher );
	virtual void MarkFlagTaken( bool bTaken );
	virtual void ResetReturnTicks( void );
	virtual void ReturnFlag( AActor *pReturner );
	virtual void AnnounceFlagReturn( void );
	virtual void DisplayFlagReturn( void );

	LONG lTick;
};

class AFlag : public ATeamItem
{
	DECLARE_CLASS( AFlag, ATeamItem )
public:
	virtual bool HandlePickup( AInventory *pItem );
	virtual LONG AllowFlagPickup( AActor *pToucher );
	virtual void AnnounceFlagPickup( AActor *pToucher );
	virtual void DisplayFlagTaken( AActor *pToucher );
	virtual void MarkFlagTaken( bool bTaken );
	virtual void ResetReturnTicks( void );
	virtual void ReturnFlag( AActor *pReturner );
	virtual void AnnounceFlagReturn( void );
	virtual void DisplayFlagReturn( void );
};

class ASkull : public ATeamItem
{
	DECLARE_CLASS( ASkull, ATeamItem )
protected:

	virtual LONG AllowFlagPickup( AActor *pToucher );
	virtual void AnnounceFlagPickup( AActor *pToucher );
	virtual void DisplayFlagTaken( AActor *pToucher );
	virtual void MarkFlagTaken( bool bTaken );
	virtual void ResetReturnTicks( void );
	virtual void ReturnFlag( AActor *pReturner );
	virtual void AnnounceFlagReturn( void );
	virtual void DisplayFlagReturn( void );
};

class AFloatyIcon : public AActor
{
	DECLARE_CLASS( AFloatyIcon, AActor )
public:
	void		Serialize( FArchive &arc );
	void		BeginPlay( );
	void		Tick( );

	void		SetTracer( AActor *pTracer );

	LONG		lTick;
	bool		bTeamItemFloatyIcon;
};

class DEarthquake : public DThinker
{
	DECLARE_CLASS (DEarthquake, DThinker)
	HAS_OBJECT_POINTERS
public:
	DEarthquake (AActor *center, int intensity, int duration, int damrad, int tremrad, FSoundID quakesfx);

	void Serialize (FArchive &arc);
	void Tick ();

	TObjPtr<AActor> m_Spot;
	fixed_t m_TremorRadius, m_DamageRadius;
	int m_Intensity;
	int m_Countdown;
	FSoundID m_QuakeSFX;

	static int StaticGetQuakeIntensity (AActor *viewer);

private:
	DEarthquake ();
};

class AMorphProjectile : public AActor
{
	DECLARE_CLASS (AMorphProjectile, AActor)
public:
	int DoSpecialDamage (AActor *target, int damage);
	void Serialize (FArchive &arc);

	FNameNoInit	PlayerClass, MonsterClass, MorphFlash, UnMorphFlash;
	int Duration, MorphStyle;
};

class AMorphedMonster : public AActor
{
	DECLARE_CLASS (AMorphedMonster, AActor)
	HAS_OBJECT_POINTERS
public:
	void Tick ();
	void Serialize (FArchive &arc);
	void Die (AActor *source, AActor *inflictor);
	void Destroy ();

	TObjPtr<AActor> UnmorphedMe;
	int UnmorphTime, MorphStyle;
	const PClass *MorphExitFlash;
	DWORD FlagsSave;
};

class AMapMarker : public AActor
{
	DECLARE_CLASS(AMapMarker, AActor)
public:
	void BeginPlay ();
	void Activate (AActor *activator);
	void Deactivate (AActor *activator);
};

class AFastProjectile : public AActor
{
	DECLARE_CLASS(AFastProjectile, AActor)
public:
	void Tick ();
	virtual void Effect();
};


#endif //__A_SHAREDGLOBAL_H__
