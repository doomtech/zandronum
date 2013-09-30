/*
** sbar.h
** Base status bar definition
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
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

#ifndef __SBAR_H__
#define __SBAR_H__

#include "dobject.h"
#include "v_collection.h"
#include "v_text.h"

class player_t;
struct FRemapTable;

extern int SB_state;

enum EHudState
{
	HUD_StatusBar,
	HUD_Fullscreen,
	HUD_None
};

class AWeapon;

// HUD Message base object --------------------------------------------------

class DHUDMessage : public DObject
{
	DECLARE_CLASS (DHUDMessage, DObject)
	HAS_OBJECT_POINTERS
public:
	DHUDMessage (FFont *font, const char *text, float x, float y, int hudwidth, int hudheight,
		EColorRange textColor, float holdTime);
	virtual ~DHUDMessage ();

	virtual void Serialize (FArchive &arc);

	void Draw (int bottom);
	virtual void ResetText (const char *text);
	virtual void DrawSetup ();
	virtual void DoDraw (int linenum, int x, int y, bool clean, int hudheight);
	virtual bool Tick ();	// Returns true to indicate time for removal
	virtual void ScreenSizeChanged ();

protected:
	FBrokenLines *Lines;
	int Width, Height, NumLines;
	float Left, Top;
	bool CenterX;
	int HoldTics;
	int Tics;
	int State;
	int HUDWidth, HUDHeight;
	EColorRange TextColor;
	FFont *Font;

	DHUDMessage () : SourceText(NULL) {}

private:
	TObjPtr<DHUDMessage> Next;
	DWORD SBarID;
	char *SourceText;

	friend class DBaseStatusBar;
};

// HUD Message; appear instantly, then fade out type ------------------------

class DHUDMessageFadeOut : public DHUDMessage
{
	DECLARE_CLASS (DHUDMessageFadeOut, DHUDMessage)
public:
	DHUDMessageFadeOut (FFont *font, const char *text, float x, float y, int hudwidth, int hudheight,
		EColorRange textColor, float holdTime, float fadeOutTime);

	virtual void Serialize (FArchive &arc);
	virtual void DoDraw (int linenum, int x, int y, bool clean, int hudheight);
	virtual bool Tick ();

protected:
	int FadeOutTics;

	DHUDMessageFadeOut() {}
};

// HUD Message; fade in, then fade out type ---------------------------------

class DHUDMessageFadeInOut : public DHUDMessageFadeOut
{
	DECLARE_CLASS (DHUDMessageFadeInOut, DHUDMessageFadeOut)
public:
	DHUDMessageFadeInOut (FFont *font, const char *text, float x, float y, int hudwidth, int hudheight,
		EColorRange textColor, float holdTime, float fadeInTime, float fadeOutTime);

	virtual void Serialize (FArchive &arc);
	virtual void DoDraw (int linenum, int x, int y, bool clean, int hudheight);
	virtual bool Tick ();

protected:
	int FadeInTics;

	DHUDMessageFadeInOut() {}
};

// HUD Message; type on, then fade out type ---------------------------------

class DHUDMessageTypeOnFadeOut : public DHUDMessageFadeOut
{
	DECLARE_CLASS (DHUDMessageTypeOnFadeOut, DHUDMessageFadeOut)
public:
	DHUDMessageTypeOnFadeOut (FFont *font, const char *text, float x, float y, int hudwidth, int hudheight,
		EColorRange textColor, float typeTime, float holdTime, float fadeOutTime);

	virtual void Serialize (FArchive &arc);
	virtual void DoDraw (int linenum, int x, int y, bool clean, int hudheight);
	virtual bool Tick ();
	virtual void ScreenSizeChanged ();

protected:
	float TypeOnTime;
	int CurrLine;
	int LineVisible;
	int LineLen;

	DHUDMessageTypeOnFadeOut() {}
};

// Mug shots ----------------------------------------------------------------

struct FMugShotFrame
{
	TArray<FString> Graphic;
	int Delay;

	FMugShotFrame();
	~FMugShotFrame();
	FTexture *GetTexture(const char *default_face, FPlayerSkin *skin, int random, int level=0,
		int direction=0, bool usesLevels=false, bool health2=false, bool healthspecial=false,
		bool directional=false);
};

struct FMugShotState
{
	BYTE bUsesLevels:1;
	BYTE bHealth2:1;		// Health level is the 2nd character from the end.
	BYTE bHealthSpecial:1;	// Like health2 only the 2nd frame gets the normal health type.
	BYTE bDirectional:1;	// Faces direction of damage.
	BYTE bFinished:1;

	unsigned int Position;
	int Time;
	int Random;
	FName State;
	TArray<FMugShotFrame> Frames;

	FMugShotState(FName name);
	~FMugShotState();
	void Tick();
	void Reset();
	FMugShotFrame &GetCurrentFrame() { return Frames[Position]; }
	FTexture *GetCurrentFrameTexture(const char *default_face, FPlayerSkin *skin, int level=0, int direction=0)
	{
		return GetCurrentFrame().GetTexture(default_face, skin, Random, level, direction, bUsesLevels, bHealth2, bHealthSpecial, bDirectional);
	}
private:
	FMugShotState();
};

class player_t;
class FMugShot
{
	public:
		enum StateFlags
		{
			STANDARD = 0x0,

			XDEATHFACE = 0x1,
			ANIMATEDGODMODE = 0x2,
			DISABLEGRIN = 0x4,
			DISABLEOUCH = 0x8,
			DISABLEPAIN = 0x10,
			DISABLERAMPAGE = 0x20,
		};

		FMugShot();
		void Grin(bool grin=true) { bEvilGrin = grin; }
		void Tick(player_t *player);
		bool SetState(const char *state_name, bool wait_till_done=false, bool reset=false);
		int UpdateState(player_t *player, StateFlags stateflags=STANDARD);
		FTexture *GetFace(player_t *player, const char *default_face, int accuracy, StateFlags stateflags=STANDARD);

	private:
		FMugShotState *CurrentState;
		int RampageTimer;
		int LastDamageAngle;
		int FaceHealth;
		bool bEvilGrin;
		bool bDamageFaceActive;
		bool bNormal;
		bool bOuchActive;

		// [BB] Necessary because Zandronum still uses the old Doom status bar code.
		friend class DDoomStatusBar;
};

extern TArray<FMugShotState> MugShotStates;

FMugShotState *FindMugShotState(FName state);
int FindMugShotStateIndex(FName state);

// Base Status Bar ----------------------------------------------------------

class FTexture;
class AAmmo;

class DBaseStatusBar : public DObject
{
	DECLARE_CLASS (DBaseStatusBar, DObject)
	HAS_OBJECT_POINTERS
public:
	// Popup screens for Strife's status bar
	enum
	{
		POP_NoChange = -1,
		POP_None,
		POP_Log,
		POP_Keys,
		POP_Status
	};

	// Status face stuff
	enum
	{
		ST_NUMPAINFACES		= 5,
		ST_NUMSTRAIGHTFACES	= 3,
		ST_NUMTURNFACES		= 2,
		ST_NUMSPECIALFACES	= 4,
		ST_NUMEXTRAFACES	= 2,
		ST_FACESTRIDE		= ST_NUMSTRAIGHTFACES+ST_NUMTURNFACES+ST_NUMSPECIALFACES,
		ST_NUMFACES			= ST_FACESTRIDE*ST_NUMPAINFACES+ST_NUMEXTRAFACES,

		ST_TURNOFFSET		= ST_NUMSTRAIGHTFACES,
		ST_OUCHOFFSET		= ST_TURNOFFSET + ST_NUMTURNFACES,
		ST_EVILGRINOFFSET	= ST_OUCHOFFSET + 1,
		ST_RAMPAGEOFFSET	= ST_EVILGRINOFFSET + 1,
		ST_QUADOFFSET		= ST_RAMPAGEOFFSET + 1,
		ST_GODFACE			= ST_NUMPAINFACES*ST_FACESTRIDE,
		ST_DEADFACE			= ST_GODFACE + 1
	};

	DBaseStatusBar (int reltop);
	void Destroy ();

	void SetScaled (bool scale, bool force=false);

	void AttachMessage (DHUDMessage *msg, uint32 id=0);
	DHUDMessage *DetachMessage (DHUDMessage *msg);
	DHUDMessage *DetachMessage (uint32 id);
	void DetachAllMessages ();
	bool CheckMessage (DHUDMessage *msg);
	void ShowPlayerName ();
	fixed_t GetDisplacement () { return Displacement; }
	int GetPlayer ();

	static void AddBlend (float r, float g, float b, float a, float v_blend[4]);

	virtual void Serialize (FArchive &arc);

	virtual void Tick ();
	virtual void Draw (EHudState state);
			void DrawTopStuff (EHudState state);
	virtual void FlashItem (const PClass *itemtype);
	virtual void AttachToPlayer (player_t *player);
	virtual void FlashCrosshair ();
	virtual void BlendView (float blend[4]);
	virtual void SetFace (void *skn);												// Takes a FPlayerSkin as input
	virtual void AddFaceToImageCollection (void *skn, FImageCollection *images);	// Takes a FPlayerSkin as input
	virtual void NewGame ();
	virtual void ScreenSizeChanged ();
	virtual void MultiplayerChanged ();
	virtual void SetInteger (int pname, int param);
	virtual void ShowPop (int popnum);
	virtual void ReceivedWeapon (AWeapon *weapon);
	virtual bool MustDrawLog(EHudState state);
	virtual void SetMugShotState (const char *state_name, bool wait_till_done=false, bool reset=false);
	void DrawLog();

protected:
	void DrawPowerups ();
	void DrawCornerScore ();
	void DrawTeamScores ();

	void UpdateRect (int x, int y, int width, int height) const;
	void DrawImage (FTexture *image, int x, int y, FRemapTable *translation=NULL) const;
	void DrawDimImage (FTexture *image, int x, int y, bool dimmed) const;
	void DrawFadedImage (FTexture *image, int x, int y, fixed_t shade) const;
	void DrawPartialImage (FTexture *image, int wx, int ww) const;

	void DrINumber (signed int val, int x, int y, int imgBase=imgINumbers) const;
	void DrBNumber (signed int val, int x, int y, int w=3) const;
	void DrBDash (int x, int y) const;
	void DrSmallNumber (int val, int x, int y) const;

	void DrINumberOuter (signed int val, int x, int y, bool center=false, int w=9) const;
	void DrBNumberOuter (signed int val, int x, int y, int w=3) const;
	void DrBNumberOuterFont (signed int val, int x, int y, int w=3) const;
	void DrSmallNumberOuter (int val, int x, int y, bool center) const;

	void RefreshBackground () const;

	void GetCurrentAmmo (AAmmo *&ammo1, AAmmo *&ammo2, int &ammocount1, int &ammocount2) const;

	void AddFaceToImageCollectionActual (void *skn, FImageCollection *images, bool isDoom);

public:
	AInventory *ValidateInvFirst (int numVisible) const;
	void DrawCrosshair ();

	int ST_X, ST_Y;
	int RelTop;
	bool Scaled;
	bool Centering;
	bool FixedOrigin;
	fixed_t CrosshairSize;
	fixed_t Displacement;

	enum
	{
		imgLAME = 0,
		imgNEGATIVE = 1,
		imgINumbers = 2,
		imgBNEGATIVE = 12,
		imgBNumbers = 13,
		imgSmNumbers = 23,
		NUM_BASESB_IMAGES = 33
	};
	FImageCollection Images;

	player_t *CPlayer;

private:
	DBaseStatusBar() {}
	bool RepositionCoords (int &x, int &y, int xo, int yo, const int w, const int h) const;
	void DrawMessages (int bottom);
	void DrawTargetName( );

	static BYTE DamageToAlpha[114];

	TObjPtr<DHUDMessage> Messages;
	bool ShowLog;
};

extern DBaseStatusBar *StatusBar;

// Status bar factories -----------------------------------------------------

DBaseStatusBar *CreateHereticStatusBar();
DBaseStatusBar *CreateHexenStatusBar();
DBaseStatusBar *CreateStrifeStatusBar();
DBaseStatusBar *CreateCustomStatusBar(int script=0);
DBaseStatusBar *CreateStatusBar ();

// Crosshair stuff ----------------------------------------------------------

void ST_LoadCrosshair(bool alwaysload=false);
extern FTexture *CrosshairImage;

#endif /* __SBAR_H__ */
