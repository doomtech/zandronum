#ifndef __R_TRANSLATE_H
#define __R_TRANSLATE_H

#include "doomtype.h"
#include "tarray.h"

class FNativeTexture;
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
	FNativeTexture *GetNative();
	bool IsIdentity() const;
	void Serialize(FArchive &ar);
	void AddIndexRange(int start, int end, int pal1, int pal2);
	void AddColorRange(int start, int end, int r1,int g1, int b1, int r2, int g2, int b2);

	BYTE *Remap;				// For the software renderer
	PalEntry *Palette;			// The ideal palette this maps to
	FNativeTexture *Native;		// The Palette stored in a HW texture
	int NumEntries;				// # of elements in this table (usually 256)

private:
	void Free();
	void Alloc(int count);
};

extern TAutoGrowArray<FRemapTable *> translationtables[NUM_TRANSLATION_TABLES];

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

const int MAX_ACS_TRANSLATIONS = 65535;
const int MAX_DECORATE_TRANSLATIONS = 65535;

// Initialize color translation tables, for player rendering etc.
void R_InitTranslationTables (void);
void R_DeinitTranslationTables();

// [RH] Actually create a player's translation table.
void R_BuildPlayerTranslation (int player);



#endif // __R_TRANSLATE_H
