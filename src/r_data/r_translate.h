#ifndef __R_TRANSLATE_H
#define __R_TRANSLATE_H

#include "doomtype.h"
#include "tarray.h"

class FNativePalette;
class FArchive;

enum
{
	TRANSLATION_Invalid,
	TRANSLATION_Players,
	TRANSLATION_PlayersExtra,
	TRANSLATION_Standard,
	TRANSLATION_LevelScripted,
	TRANSLATION_Decals,
	TRANSLATION_PlayerCorpses,
	TRANSLATION_Decorate,
	TRANSLATION_Blood,

	NUM_TRANSLATION_TABLES
};

struct FRemapTable
{
	FRemapTable(int count=256);
	FRemapTable(const FRemapTable &o);
	~FRemapTable();

	FRemapTable &operator= (const FRemapTable &o);
	bool operator==(const FRemapTable &o);
	void MakeIdentity();
	void KillNative();
	void UpdateNative();
	FNativePalette *GetNative();
	bool IsIdentity() const;
	void Serialize(FArchive &ar);
	void AddIndexRange(int start, int end, int pal1, int pal2);
	void AddColorRange(int start, int end, int r1,int g1, int b1, int r2, int g2, int b2);
	void AddDesaturation(int start, int end, double r1, double g1, double b1, double r2, double g2, double b2);
	void AddToTranslation(const char * range);
	int StoreTranslation();

	BYTE *Remap;				// For the software renderer
	PalEntry *Palette;			// The ideal palette this maps to
	FNativePalette *Native;		// The Palette stored in a HW texture
	int NumEntries;				// # of elements in this table (usually 256)
	bool Inactive;				// This table is inactive and should be treated as if it was passed as NULL

private:
	void Free();
	void Alloc(int count);
};

// A class that initializes unusued pointers to NULL. This is used so that when
// the TAutoGrowArray below is expanded, the new elements will be NULLed.
class FRemapTablePtr
{
public:
	FRemapTablePtr() throw() : Ptr(0) {}
	FRemapTablePtr(FRemapTable *p) throw() : Ptr(p) {}
	FRemapTablePtr(const FRemapTablePtr &p) throw() : Ptr(p.Ptr) {}
	operator FRemapTable *() const throw() { return Ptr; }
	FRemapTablePtr &operator= (FRemapTable *p) throw() { Ptr = p; return *this; }
	FRemapTablePtr &operator= (FRemapTablePtr &p) throw() { Ptr = p.Ptr; return *this; }
	FRemapTable &operator*() const throw() { return *Ptr; }
	FRemapTable *operator->() const throw() { return Ptr; }
private:
	FRemapTable *Ptr;
};

extern TAutoGrowArray<FRemapTablePtr, FRemapTable *> translationtables[NUM_TRANSLATION_TABLES];

#define TRANSLATION_SHIFT 16
#define TRANSLATION_MASK ((1<<TRANSLATION_SHIFT)-1)
#define TRANSLATIONTYPE_MASK (255<<TRANSLATION_SHIFT)

inline DWORD TRANSLATION(BYTE a, DWORD b)
{
	return (a<<TRANSLATION_SHIFT) | b;
}
inline int GetTranslationType(DWORD trans)
{
	return (trans&TRANSLATIONTYPE_MASK) >> TRANSLATION_SHIFT;
}
inline int GetTranslationIndex(DWORD trans)
{
	return (trans&TRANSLATION_MASK);
}
// Retrieve the FRemapTable that an actor's translation value maps to.
FRemapTable *TranslationToTable(int translation);

#define MAX_ACS_TRANSLATIONS		65535
#define MAX_DECORATE_TRANSLATIONS	65535

// Initialize color translation tables, for player rendering etc.
void R_InitTranslationTables (void);
void R_DeinitTranslationTables();

void R_BuildPlayerTranslation (int player);		// [RH] Actually create a player's translation table.
void R_GetPlayerTranslation (int color, const struct FPlayerColorSet *colorset, class FPlayerSkin *skin, struct FRemapTable *table);

// [Dusk] Does R_BuildPlayerTranslation for all active players.
void R_BuildAllPlayerTranslations();

extern const BYTE IcePalette[16][3];

extern TArray<PalEntry> BloodTranslationColors;

int CreateBloodTranslation(PalEntry color);



#endif // __R_TRANSLATE_H
