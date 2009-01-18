#ifndef __RAVENSHARED_H__
#define __RAVENSHARED_H__

class AActor;

class AMinotaur : public AActor
{
	DECLARE_CLASS (AMinotaur, AActor)
public:
	int DoSpecialDamage (AActor *target, int damage);

public:
	bool Slam (AActor *);
	void Tick ();
};

class AMinotaurFriend : public AMinotaur
{
	DECLARE_CLASS (AMinotaurFriend, AMinotaur)
public:
	int StartTime;

	bool IsOkayToAttack (AActor *target);
	void Die (AActor *source, AActor *inflictor);
	bool OkayToSwitchTarget (AActor *other);
	void BeginPlay ();
	void Serialize (FArchive &arc);
};

#endif //__RAVENSHARED_H__
