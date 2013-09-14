/*
** sbarinfo.h
**
** Header for custom status bar definitions.
**
**---------------------------------------------------------------------------
** Copyright 2008 Braden Obrzut
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

#ifndef __SBarInfo_SBAR_H__
#define __SBarInfo_SBAR_H__

#include "tarray.h"
#include "v_collection.h"

#define NUMHUDS 9
#define NUMPOPUPS 3

class FBarTexture;
class FScanner;

/**
 * This class is used to help prevent errors that may occur from adding or 
 * subtracting from coordinates.
 *
 * In order to provide the maximum flexibility, coordinates are packed into
 * an int with one bit reserved for relCenter.
 */
class SBarInfoCoordinate
{
	public:
		SBarInfoCoordinate	&Add(int add);
		int					Coordinate() const { return value; }
		bool				RelCenter() const { return relCenter; }
		void				Set(int coord, bool center) { value = coord; relCenter = center; }
		void				SetCoord(int coord) { value = coord; }
		void				SetRelCenter(bool center) { relCenter = center; }

		int					operator*	() const { return Coordinate(); }
		SBarInfoCoordinate	operator+	(int add) const { return SBarInfoCoordinate(*this).Add(add); }
		SBarInfoCoordinate	operator+	(const SBarInfoCoordinate &other) const { return SBarInfoCoordinate(*this).Add(other.Coordinate()); }
		SBarInfoCoordinate	operator-	(int sub) const { return SBarInfoCoordinate(*this).Add(-sub); }
		SBarInfoCoordinate	operator-	(const SBarInfoCoordinate &other) const { return SBarInfoCoordinate(*this).Add(-other.Coordinate()); }
		void				operator+=	(int add) { Add(add); }
		void				operator-=	(int sub) { Add(-sub); }

	protected:
		unsigned relCenter:1;
		int		 value:31;
};

struct SBarInfoCommand; //we need to be able to use this before it is defined.
struct MugShotState;

//Popups!
enum PopupTransition
{
	TRANSITION_NONE,
	TRANSITION_SLIDEINBOTTOM,
	TRANSITION_FADE,
};

struct Popup
{
	PopupTransition transition;
	bool opened;
	bool moving;
	int height;
	int width;
	int speed;
	int speed2;
	int alpha;
	int x;
	int y;

	Popup();
	void init();
	void tick();
	void open();
	void close();
	bool isDoneMoving();
	int getXOffset();
	int getYOffset();
	int getAlpha(int maxAlpha=FRACUNIT);
};

//SBarInfo
struct SBarInfoBlock
{
	TArray<SBarInfoCommand> commands;
	bool forceScaled;
	bool fullScreenOffsets;
	int alpha;

	SBarInfoBlock();
};

struct SBarInfoCommand
{
	SBarInfoCommand();
	~SBarInfoCommand();
	void setString(FScanner &sc, const char* source, int strnum, int maxlength=-1, bool exact=false);

	int type;
	int special;
	union
	{
		int special2;
		SBarInfoCoordinate sbcoord2;
	};
	union
	{
		int special3;
		SBarInfoCoordinate sbcoord3;
	};
	int special4;
	int flags;
	SBarInfoCoordinate x;
	SBarInfoCoordinate y;
	int value;
	int image_index;
	FTextureID sprite_index;
	FString string[2];
	FFont *font;
	EColorRange translation;
	EColorRange translation2;
	EColorRange translation3;
	SBarInfoBlock subBlock; //for type SBarInfo_CMD_GAMEMODE
};

struct SBarInfo
{
	TArray<FString> Images;
	SBarInfoBlock huds[NUMHUDS];
	Popup popups[NUMPOPUPS];
	bool automapbar;
	bool interpolateHealth;
	bool interpolateArmor;
	bool completeBorder;
	bool lowerHealthCap;
	char spacingCharacter;
	int interpolationSpeed;
	int armorInterpolationSpeed;
	int height;
	int gameType;

	int GetGameType() { return gameType; }
	void ParseSBarInfo(int lump);
	void ParseSBarInfoBlock(FScanner &sc, SBarInfoBlock &block);
	void ParseMugShotBlock(FScanner &sc, FMugShotState &state);
	void getCoordinates(FScanner &sc, bool fullScreenOffsets, SBarInfoCoordinate &x, SBarInfoCoordinate &y); //retrieves the next two arguments as x and y.
	int getSignedInteger(FScanner &sc); //returns a signed integer.
	int newImage(const char* patchname);
	void Init();
	EColorRange GetTranslation(FScanner &sc, const char* translation);
	SBarInfo();
	SBarInfo(int lumpnum);
	~SBarInfo();

	static void	Load();
};

#define SCRIPT_CUSTOM	0
#define SCRIPT_DEFAULT	1
extern SBarInfo *SBarInfoScript[2];

// Enums used between the parser and the display
enum //gametype flags
{
	GAMETYPE_SINGLEPLAYER = 1,
	GAMETYPE_COOPERATIVE = 2,
	GAMETYPE_DEATHMATCH = 4,
	GAMETYPE_TEAMGAME = 8,
	GAMETYPE_CTF = 0x10,
	GAMETYPE_ONEFLAGCTF = 0x20,
	GAMETYPE_SKULLTAG = 0x40,
	GAMETYPE_INVASION = 0x80,
	GAMETYPE_POSSESSION = 0x100,
	GAMETYPE_TEAMPOSSESSION = 0x200,
	GAMETYPE_LASTMANSTANDING = 0x400,
	GAMETYPE_TEAMLMS = 0x800,
	GAMETYPE_SURVIVAL = 0x1000,
	GAMETYPE_INSTAGIB = 0x2000,
	GAMETYPE_BUCKSHOT = 0x4000,
};

enum //drawimage flags
{
	DRAWIMAGE_PLAYERICON = 0x1,
	DRAWIMAGE_AMMO1 = 0x2,
	DRAWIMAGE_AMMO2 = 0x4,
	DRAWIMAGE_INVENTORYICON = 0x8,
	DRAWIMAGE_TRANSLATABLE = 0x10,
	DRAWIMAGE_WEAPONSLOT = 0x20,
	DRAWIMAGE_SWITCHABLE_AND = 0x40,
	DRAWIMAGE_INVULNERABILITY = 0x80,
	DRAWIMAGE_OFFSET_CENTER = 0x100,
	DRAWIMAGE_OFFSET_CENTERBOTTOM = 0x200,
	DRAWIMAGE_ARMOR = 0x800,
	DRAWIMAGE_WEAPONICON = 0x1000,
	DRAWIMAGE_SIGIL = 0x2000,
	DRAWIMAGE_KEYSLOT = 0x4000,
	DRAWIMAGE_HEXENARMOR = 0x8000,
	DRAWIMAGE_RUNEICON = 0x10000,

	DRAWIMAGE_OFFSET = DRAWIMAGE_OFFSET_CENTER|DRAWIMAGE_OFFSET_CENTERBOTTOM,
};

enum //drawnumber flags
{
	DRAWNUMBER_HEALTH = 0x1,
	DRAWNUMBER_ARMOR = 0x2,
	DRAWNUMBER_AMMO1 = 0x4,
	DRAWNUMBER_AMMO2 = 0x8,
	DRAWNUMBER_AMMO = 0x10,
	DRAWNUMBER_AMMOCAPACITY = 0x20,
	DRAWNUMBER_FRAGS = 0x40,
	DRAWNUMBER_INVENTORY = 0x80,
	DRAWNUMBER_KILLS = 0x100,
	DRAWNUMBER_MONSTERS = 0x200,
	DRAWNUMBER_ITEMS = 0x400,
	DRAWNUMBER_TOTALITEMS = 0x800,
	DRAWNUMBER_SECRETS = 0x1000,
	DRAWNUMBER_TOTALSECRETS = 0x2000,
	DRAWNUMBER_ARMORCLASS = 0x4000,
	DRAWNUMBER_GLOBALVAR = 0x8000,
	DRAWNUMBER_GLOBALARRAY = 0x10000,
	DRAWNUMBER_FILLZEROS = 0x20000,
	DRAWNUMBER_WHENNOTZERO = 0x40000,
	DRAWNUMBER_POWERUPTIME = 0x80000,
	DRAWNUMBER_DRAWSHADOW = 0x100000,
	DRAWNUMBER_AIRTIME = 0x200000,
	DRAWNUMBER_TEAMSCORE = 0x400000, //Add team # to the value. (MAX of 4)
};

enum //drawbar flags (will go into special2)
{
	DRAWBAR_HORIZONTAL = 1,
	DRAWBAR_REVERSE = 2,
	DRAWBAR_COMPAREDEFAULTS = 4,
};

enum //drawselectedinventory flags
{
	DRAWSELECTEDINVENTORY_ALTERNATEONEMPTY = 0x1,
	DRAWSELECTEDINVENTORY_ARTIFLASH = 0x2,
	DRAWSELECTEDINVENTORY_ALWAYSSHOWCOUNTER = 0x4,
	DRAWSELECTEDINVENTORY_CENTER = 0x8,
	DRAWSELECTEDINVENTORY_CENTERBOTTOM = 0x10,
	DRAWSELECTEDINVENTORY_DRAWSHADOW = 0x20,
};

enum //drawinventorybar flags
{
	DRAWINVENTORYBAR_ALWAYSSHOW = 0x1,
	DRAWINVENTORYBAR_NOARTIBOX = 0x2,
	DRAWINVENTORYBAR_NOARROWS = 0x4,
	DRAWINVENTORYBAR_ALWAYSSHOWCOUNTER = 0x8,
	DRAWINVENTORYBAR_TRANSLUCENT = 0x10,
};

enum //drawgem flags
{
	DRAWGEM_WIGGLE = 1,
	DRAWGEM_TRANSLATABLE = 2,
	DRAWGEM_ARMOR = 4,
	DRAWGEM_REVERSE = 8,
};

enum //drawshader flags
{
	DRAWSHADER_VERTICAL = 1,
	DRAWSHADER_REVERSE = 2,
};

enum //drawmugshot flags
{
	DRAWMUGSHOT_XDEATHFACE = 0x1,
	DRAWMUGSHOT_ANIMATEDGODMODE = 0x2,
	DRAWMUGSHOT_DISABLEGRIN = 0x4,
	DRAWMUGSHOT_DISABLEOUCH = 0x8,
	DRAWMUGSHOT_DISABLEPAIN = 0x10,
	DRAWMUGSHOT_DISABLERAMPAGE = 0x20,
};

enum //drawkeybar flags
{
	DRAWKEYBAR_VERTICAL = 0x1,
	DRAWKEYBAR_REVERSEROWS = 0x2,
};

enum //event flags
{
	SBARINFOEVENT_NOT = 1,
	SBARINFOEVENT_OR = 2,
	SBARINFOEVENT_AND = 4,
};

enum //aspect ratios
{
	ASPECTRATIO_4_3 = 0,
	ASPECTRATIO_16_9 = 1,
	ASPECTRATIO_16_10 = 2,
	ASPECTRATIO_5_4 = 4,
};

enum //Key words
{
	SBARINFO_BASE,
	SBARINFO_HEIGHT,
	SBARINFO_INTERPOLATEHEALTH,
	SBARINFO_INTERPOLATEARMOR,
	SBARINFO_COMPLETEBORDER,
	SBARINFO_MONOSPACEFONTS,
	SBARINFO_LOWERHEALTHCAP,
	SBARINFO_STATUSBAR,
	SBARINFO_MUGSHOT,
	SBARINFO_CREATEPOPUP,
};

enum //Bar types
{
	STBAR_NONE,
	STBAR_FULLSCREEN,
	STBAR_NORMAL,
	STBAR_AUTOMAP,
	STBAR_INVENTORY,
	STBAR_INVENTORYFULLSCREEN,
	STBAR_POPUPLOG,
	STBAR_POPUPKEYS,
	STBAR_POPUPSTATUS,
};

enum //Bar key words
{
	SBARINFO_DRAWIMAGE,
	SBARINFO_DRAWNUMBER,
	SBARINFO_DRAWSWITCHABLEIMAGE,
	SBARINFO_DRAWMUGSHOT,
	SBARINFO_DRAWSELECTEDINVENTORY,
	SBARINFO_DRAWINVENTORYBAR,
	SBARINFO_DRAWBAR,
	SBARINFO_DRAWGEM,
	SBARINFO_DRAWSHADER,
	SBARINFO_DRAWSTRING,
	SBARINFO_DRAWKEYBAR,
	SBARINFO_GAMEMODE,
	SBARINFO_PLAYERCLASS,
	SBARINFO_ASPECTRATIO,
	SBARINFO_ISSELECTED,
	SBARINFO_USESAMMO,
	SBARINFO_USESSECONDARYAMMO,
	SBARINFO_HASWEAPONPIECE,
	SBARINFO_INVENTORYBARNOTVISIBLE,
	SBARINFO_WEAPONAMMO,
	SBARINFO_ININVENTORY,
};

//All this so I can change the mugshot state in ACS...
class FBarShader : public FTexture
{
public:
	FBarShader(bool vertical, bool reverse);
	const BYTE *GetColumn(unsigned int column, const Span **spans_out);
	const BYTE *GetPixels();
	void Unload();
private:
	BYTE Pixels[512];
	Span DummySpan[2];
};

class DSBarInfo : public DBaseStatusBar
{
	DECLARE_CLASS(DSBarInfo, DBaseStatusBar)
public:
	DSBarInfo(SBarInfo *script=NULL);
	~DSBarInfo();
	void Draw(EHudState state);
	void NewGame();
	void AttachToPlayer(player_t *player);
	void Tick();
	void ReceivedWeapon (AWeapon *weapon);
	void FlashItem(const PClass *itemtype);
	void ShowPop(int popnum);
	void SetMugShotState(const char* stateName, bool waitTillDone=false, bool reset=false);
private:
	void doCommands(SBarInfoBlock &block, int xOffset=0, int yOffset=0, int alpha=FRACUNIT);
	void DrawGraphic(FTexture* texture, SBarInfoCoordinate x, SBarInfoCoordinate y, int xOffset, int yOffset, int alpha, bool fullScreenOffsets, bool translate=false, bool dim=false, int offsetflags=0);
	void DrawString(const char* str, SBarInfoCoordinate x, SBarInfoCoordinate y, int xOffset, int yOffset, int alpha, bool fullScreenOffsets, EColorRange translation, int spacing=0, bool drawshadow=false);
	void DrawNumber(int num, int len, SBarInfoCoordinate x, SBarInfoCoordinate y, int xOffset, int yOffset, int alpha, bool fullScreenOffsets, EColorRange translation, int spacing=0, bool fillzeros=false, bool drawshadow=false);
	void DrawFace(const char *defaultFace, int accuracy, int stateflags, SBarInfoCoordinate x, SBarInfoCoordinate y, int xOffset, int yOffset, int alpha, bool fullScreenOffsets);
	int updateState(bool xdth, bool animatedgodmode);
	void DrawInventoryBar(int type, int num, SBarInfoCoordinate x, SBarInfoCoordinate y, int xOffset, int yOffset, int alpha, bool fullScreenOffsets, bool alwaysshow,
		SBarInfoCoordinate counterx, SBarInfoCoordinate countery, EColorRange translation, bool drawArtiboxes, bool noArrows, bool alwaysshowcounter, int bgalpha);
	void DrawGem(FTexture* chain, FTexture* gem, int value, SBarInfoCoordinate x, SBarInfoCoordinate y, int xOffset, int yOffset, int alpha, bool fullScreenOffsets, int padleft, int padright, int chainsize,
		bool wiggle, bool translate);
	FRemapTable* getTranslation();

	SBarInfo *script;
	FImageCollection Images;
	FPlayerSkin *oldSkin;
	FFont *drawingFont;
	int oldHealth;
	int oldArmor;
	int mugshotHealth;
	int chainWiggle;
	int artiflash;
	int pendingPopup;
	int currentPopup;
	unsigned int invBarOffset;
	FBarShader shader_horz_normal;
	FBarShader shader_horz_reverse;
	FBarShader shader_vert_normal;
	FBarShader shader_vert_reverse;
	FMugShot MugShot;
};

#endif //__SBarInfo_SBAR_H__
