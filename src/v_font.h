/*
** v_font.h
**
**---------------------------------------------------------------------------
** Copyright 1998-2008 Randy Heit
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

#ifndef __V_FONT_H__
#define __V_FONT_H__

#include "doomtype.h"

class DCanvas;
struct FRemapTable;
class FTexture;
class FArchive;

enum EColorRange
{
	CR_UNDEFINED = -1,
	CR_BRICK,
	CR_TAN,
	CR_GRAY,
	CR_GREY = CR_GRAY,
	CR_GREEN,
	CR_BROWN,
	CR_GOLD,
	CR_RED,
	CR_BLUE,
	CR_ORANGE,
	CR_WHITE,
	CR_YELLOW,
	CR_UNTRANSLATED,
	CR_BLACK,
	CR_LIGHTBLUE,
	CR_CREAM,
	CR_OLIVE,
	CR_DARKGREEN,
	CR_DARKRED,
	CR_DARKBROWN,
	CR_PURPLE,
	CR_DARKGRAY,
	CR_CYAN,
	NUM_TEXT_COLORS
};

extern int NumTextColors;


class FFont
{
public:
	FFont (const char *fontname, const char *nametemplate, int first, int count, int base);
	virtual ~FFont ();

	virtual FTexture *GetChar (int code, int *const width) const;
	virtual int GetCharWidth (int code) const;
	FRemapTable *GetColorTranslation (EColorRange range) const;
	int GetSpaceWidth () const { return SpaceWidth; }
	int GetHeight () const { return FontHeight; }
	int GetDefaultKerning () const { return GlobalKerning; }
	void Preload() const;

	static FFont *FindFont (const char *fontname);
	static void StaticPreloadFonts();

	// Return width of string in pixels (unscaled)
	int StringWidth (const BYTE *str) const;
	inline int StringWidth (const char *str) const { return StringWidth ((const BYTE *)str); }

	int GetCharCode(int code, bool needpic) const;

protected:
	FFont ();

	void BuildTranslations (const double *luminosity, const BYTE *identity,
		const void *ranges, int total_colors, const PalEntry *palette);
	void FixXMoves();

	static int SimpleTranslation (BYTE *colorsused, BYTE *translation,
		BYTE *identity, double **luminosity);

	int FirstChar, LastChar;
	int SpaceWidth;
	int FontHeight;
	int GlobalKerning;
	struct CharData
	{
		FTexture *Pic;
		int XMove;
	} *Chars;
	int ActiveColors;
	TArray<FRemapTable> Ranges;
	BYTE *PatchRemap;

	char *Name;
	FFont *Next;

	static FFont *FirstFont;
	friend struct FontsDeleter;

	friend void V_Shutdown();

	friend FArchive &SerializeFFontPtr (FArchive &arc, FFont* &font);
};


extern FFont *SmallFont, *SmallFont2, *BigFont, *ConFont, *IntermissionFont;

void V_InitFonts();
EColorRange V_FindFontColor (FName name);
PalEntry V_LogColorFromColorRange (EColorRange range);
EColorRange V_ParseFontColor (const BYTE *&color_value, int normalcolor, int boldcolor);
FFont *V_GetFont(const char *);
void V_InitFontColors();

#endif //__V_FONT_H__
