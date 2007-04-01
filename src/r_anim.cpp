/*
** r_anim.cpp
** Routines for handling texture animation.
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

#include "cmdlib.h"
#include "i_system.h"
#include "r_local.h"
#include "r_sky.h"
#include "m_random.h"
#include "d_player.h"
#include "p_spec.h"
#include "sc_man.h"
#include "templates.h"
#include "w_wad.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

//
// Animating textures and planes
//
// [RH] Expanded to work with a Hexen ANIMDEFS lump
//

struct FAnimDef
{
	WORD 	BasePic;
	WORD	NumFrames;
	WORD	CurFrame;
	BYTE	AnimType;
	DWORD	SwitchTime;			// Time to advance to next frame
	struct FAnimFrame
	{
		DWORD	SpeedMin;		// Speeds are in ms, not tics
		DWORD	SpeedRange;
		WORD	FramePic;
	} Frames[1];
	enum
	{
		ANIM_Forward,
		ANIM_Backward,
		ANIM_OscillateUp,
		ANIM_OscillateDown,
		ANIM_DiscreteFrames
	};

	void SetSwitchTime (DWORD mstime);
};

// This is an array of pointers to animation definitions.
// When it is destroyed, it deletes any animations it points to as well.
class AnimArray : public TArray<FAnimDef *>
{
public:
	~AnimArray();
	void AddAnim (FAnimDef *anim);
	void FixAnimations ();
};

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void R_InitAnimDefs ();
static void R_AddComplexAnim (int picnum, const TArray<FAnimDef::FAnimFrame> &frames);
static void ParseAnim (bool istex);
static void ParseRangeAnim (int picnum, int usetype, bool missing);
static void ParsePicAnim (int picnum, int usetype, bool missing, TArray<FAnimDef::FAnimFrame> &frames);
static int  ParseFramenum (int basepicnum, int usetype, bool allowMissing);
static void ParseTime (DWORD &min, DWORD &max);

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static AnimArray Anims;
static FRandom pr_animatepictures ("AnimatePics");

// CODE --------------------------------------------------------------------

//==========================================================================
//
// R_InitPicAnims
//
// [description copied from BOOM]
// Load the table of animation definitions, checking for existence of
// the start and end of each frame. If the start doesn't exist the sequence
// is skipped, if the last doesn't exist, BOOM exits.
//
// Wall/Flat animation sequences, defined by name of first and last frame,
// The full animation sequence is given using all lumps between the start
// and end entry, in the order found in the WAD file.
//
// This routine modified to read its data from a predefined lump or
// PWAD lump called ANIMATED rather than a static table in this module to
// allow wad designers to insert or modify animation sequences.
//
// Lump format is an array of byte packed animdef_t structures, terminated
// by a structure with istexture == -1. The lump can be generated from a
// text source file using SWANTBLS.EXE, distributed with the BOOM utils.
// The standard list of switches and animations is contained in the example
// source text file DEFSWANI.DAT also in the BOOM util distribution.
//
// [RH] Rewritten to support BOOM ANIMATED lump but also make absolutely
//		no assumptions about how the compiler packs the animdefs array.
//
//==========================================================================

void R_InitPicAnims (void)
{
	const BITFIELD texflags = FTextureManager::TEXMAN_Overridable | FTextureManager::TEXMAN_TryAny;

	if (Wads.CheckNumForName ("ANIMATED") != -1)
	{
		FMemLump animatedlump = Wads.ReadLump ("ANIMATED");
		const char *animdefs = (const char *)animatedlump.GetMem();
		const char *anim_p;
		int pic1, pic2;
		int animtype;
		DWORD animspeed;

		// Init animation
		animtype = FAnimDef::ANIM_Forward;

		for (anim_p = animdefs; *anim_p != -1; anim_p += 23)
		{
			if (*anim_p /* .istexture */ & 1)
			{
				// different episode ?
				if ((pic1 = TexMan.CheckForTexture (anim_p + 10 /* .startname */, FTexture::TEX_Wall, texflags)) == -1 ||
					(pic2 = TexMan.CheckForTexture (anim_p + 1 /* .endname */, FTexture::TEX_Wall, texflags)) == -1)
					continue;		

				// [RH] Bit 1 set means allow decals on walls with this texture
				TexMan[pic2]->bNoDecals = TexMan[pic1]->bNoDecals = !(*anim_p & 2);
			}
			else
			{
				if ((pic1 = TexMan.CheckForTexture (anim_p + 10 /* .startname */, FTexture::TEX_Flat, texflags)) == -1 ||
					(pic2 = TexMan.CheckForTexture (anim_p + 1 /* .startname */, FTexture::TEX_Flat, texflags)) == -1)
					continue;
			}
			if (pic1 == pic2)
			{
				// This animation only has one frame. Skip it. (Doom aborted instead.)
				Printf ("Animation %s in ANIMATED has only one frame", anim_p + 10);
				continue;
			}
			// [RH] Allow for backward animations as well as forward.
			if (pic1 > pic2)
			{
				swap (pic1, pic2);
				animtype = FAnimDef::ANIM_Backward;
			}

			// Speed is stored as tics, but we want ms so scale accordingly.
			animspeed = /* .speed */
				Scale ((anim_p[19] << 0) |
					   (anim_p[20] << 8) |
					   (anim_p[21] << 16) |
					   (anim_p[22] << 24), 1000, 35);

			R_AddSimpleAnim (pic1, pic2 - pic1 + 1, animtype, animspeed);
		}
	}
	// [RH] Load any ANIMDEFS lumps
	R_InitAnimDefs ();
	Anims.FixAnimations ();
}

//==========================================================================
//
// R_AddSimpleAnim
//
// Creates an animation with simple characteristics. This is used for
// original Doom (non-ANIMDEFS-style) animations and Build animations.
//
//==========================================================================

void R_AddSimpleAnim (int picnum, int animcount, int animtype, DWORD speedmin, DWORD speedrange)
{
	FAnimDef *anim = (FAnimDef *)M_Malloc (sizeof(FAnimDef));
	anim->CurFrame = 0;
	anim->BasePic = picnum;
	anim->NumFrames = animcount;
	anim->AnimType = animtype;
	anim->SwitchTime = 0;
	anim->Frames[0].SpeedMin = speedmin;
	anim->Frames[0].SpeedRange = speedrange;
	anim->Frames[0].FramePic = anim->BasePic;
	Anims.AddAnim (anim);
}

//==========================================================================
//
// R_AddComplexAnim
//
// Creates an animation with individually defined frames.
//
//==========================================================================

static void R_AddComplexAnim (int picnum, const TArray<FAnimDef::FAnimFrame> &frames)
{
	FAnimDef *anim = (FAnimDef *)M_Malloc (sizeof(FAnimDef) + (frames.Size()-1) * sizeof(frames[0]));
	anim->BasePic = picnum;
	anim->NumFrames = frames.Size();
	anim->CurFrame = 0;
	anim->AnimType = FAnimDef::ANIM_DiscreteFrames;
	anim->SwitchTime = 0;
	memcpy (&anim->Frames[0], &frames[0], frames.Size() * sizeof(frames[0]));
	Anims.AddAnim (anim);
}

//==========================================================================
//
// R_InitAnimDefs
//
// This uses a Hexen ANIMDEFS lump to define the animation sequences
//
//==========================================================================

static void R_InitAnimDefs ()
{
	const BITFIELD texflags = FTextureManager::TEXMAN_Overridable | FTextureManager::TEXMAN_TryAny;
	int lump, lastlump = 0;
	
	while ((lump = Wads.FindLump ("ANIMDEFS", &lastlump)) != -1)
	{
		SC_OpenLumpNum (lump, "ANIMDEFS");

		while (SC_GetString ())
		{
			if (SC_Compare ("flat"))
			{
				ParseAnim (false);
			}
			else if (SC_Compare ("texture"))
			{
				ParseAnim (true);
			}
			else if (SC_Compare ("switch"))
			{
				P_ProcessSwitchDef ();
			}
			// [GRB] Added warping type 2
			else if (SC_Compare ("warp") || SC_Compare ("warp2"))
			{
				bool isflat = false;
				bool type2 = SC_Compare ("warp2");	// [GRB]
				SC_MustGetString ();
				if (SC_Compare ("flat"))
				{
					isflat = true;
					SC_MustGetString ();
				}
				else if (SC_Compare ("texture"))
				{
					isflat = false;
					SC_MustGetString ();
				}
				else
				{
					SC_ScriptError (NULL);
				}
				int picnum = TexMan.CheckForTexture (sc_String, isflat ? FTexture::TEX_Flat : FTexture::TEX_Wall, texflags);
				if (picnum != -1)
				{
					FTexture * warper = TexMan[picnum];

					// don't warp a texture more than once
					if (!warper->bWarped)
					{
						if (type2)	// [GRB]
							warper = new FWarp2Texture (warper);
						else
							warper = new FWarpTexture (warper);
						TexMan.ReplaceTexture (picnum, warper, false);
					}

					// No decals on warping textures, by default.
					// Warping information is taken from the last warp 
					// definition for this texture.
					warper->bNoDecals = true;
					if (SC_GetString ())
					{
						if (SC_Compare ("allowdecals"))
						{
							warper->bNoDecals = false;
						}
						else
						{
							SC_UnGet ();
						}
					}
				}
			}
			else if (SC_Compare ("cameratexture"))
			{
				int width, height;
				int fitwidth, fitheight;
				FString picname;

				SC_MustGetString ();
				picname = sc_String;
				SC_MustGetNumber ();
				width = sc_Number;
				SC_MustGetNumber ();
				height = sc_Number;
				int picnum = TexMan.CheckForTexture (picname, FTexture::TEX_Flat, texflags);
				FTexture *viewer = new FCanvasTexture (picname, width, height);
				if (picnum != -1)
				{
					FTexture *oldtex = TexMan[picnum];
					fitwidth = DivScale3 (oldtex->GetWidth (), oldtex->ScaleX ? oldtex->ScaleX : 8);
					fitheight = DivScale3 (oldtex->GetHeight (), oldtex->ScaleY ? oldtex->ScaleY : 8);
					viewer->UseType = oldtex->UseType;
					TexMan.ReplaceTexture (picnum, viewer, true);
				}
				else
				{
					fitwidth = width;
					fitheight = height;
					// [GRB] No need for oldtex
					viewer->UseType = FTexture::TEX_Wall;
					TexMan.AddTexture (viewer);
				}
				if (SC_GetString())
				{
					if (SC_Compare ("fit"))
					{
						SC_MustGetNumber ();
						fitwidth = sc_Number;
						SC_MustGetNumber ();
						fitheight = sc_Number;
					}
					else
					{
						SC_UnGet ();
					}
				}
				viewer->ScaleX = width * 8 / fitwidth;
				viewer->ScaleY = height * 8 / fitheight;
			}
			else if (SC_Compare ("animatedDoor"))
			{
				P_ParseAnimatedDoor ();
			}
			else
			{
				SC_ScriptError (NULL);
			}
		}
		SC_Close ();
	}
}

//==========================================================================
//
// ParseAnim
//
// Parse a single animation definition out of an ANIMDEFS lump and
// create the corresponding animation structure.
//
//==========================================================================

static void ParseAnim (bool istex)
{
	const BITFIELD texflags = FTextureManager::TEXMAN_Overridable | FTextureManager::TEXMAN_TryAny;
	TArray<FAnimDef::FAnimFrame> frames (32);
	int picnum;
	int usetype;
	int defined = 0;
	bool optional = false, missing = false;

	usetype = istex ? FTexture::TEX_Wall : FTexture::TEX_Flat;

	SC_MustGetString ();
	if (SC_Compare ("optional"))
	{
		optional = true;
		SC_MustGetString ();
	}
	picnum = TexMan.CheckForTexture (sc_String, usetype, texflags);

	if (picnum < 0)
	{
		if (optional)
		{
			missing = true;
		}
		else
		{
			Printf (PRINT_BOLD, "ANIMDEFS: Can't find %s\n", sc_String);
		}
	}

	// no decals on animating textures, by default
	if (picnum >= 0)
	{
		TexMan[picnum]->bNoDecals = true;
	}

	while (SC_GetString ())
	{
		if (SC_Compare ("allowdecals"))
		{
			if (picnum >= 0)
			{
				TexMan[picnum]->bNoDecals = false;
			}
			continue;
		}
		else if (SC_Compare ("range"))
		{
			if (defined == 2)
			{
				SC_ScriptError ("You cannot use \"pic\" and \"range\" together in a single animation.");
			}
			if (defined == 1)
			{
				SC_ScriptError ("You can only use one \"range\" per animation.");
			}
			defined = 1;
			ParseRangeAnim (picnum, usetype, missing);
		}
		else if (SC_Compare ("pic"))
		{
			if (defined == 1)
			{
				SC_ScriptError ("You cannot use \"pic\" and \"range\" together in a single animation.");
			}
			defined = 2;
			ParsePicAnim (picnum, usetype, missing, frames);
		}
		else
		{
			SC_UnGet ();
			break;
		}
	}

	// If base pic is not present, don't add this anim
	// ParseRangeAnim adds the anim itself, but ParsePicAnim does not.
	if (picnum >= 0 && defined == 2)
	{
		if (frames.Size() < 2)
		{
			SC_ScriptError ("Animation needs at least 2 frames");
		}
		R_AddComplexAnim (picnum, frames);
	}
}

//==========================================================================
//
// ParseRangeAnim
//
// Parse an animation defined using "range". Not that one range entry is
// enough to define a complete animation, unlike "pic".
//
//==========================================================================

static void ParseRangeAnim (int picnum, int usetype, bool missing)
{
	int type, framenum;
	DWORD min, max;

	type = FAnimDef::ANIM_Forward;
	framenum = ParseFramenum (picnum, usetype, missing);
	ParseTime (min, max);

	if (framenum == picnum || picnum < 0)
	{
		return;		// Animation is only one frame or does not exist
	}
	if (framenum < picnum)
	{
		type = FAnimDef::ANIM_Backward;
		TexMan[framenum]->bNoDecals = TexMan[picnum]->bNoDecals;
		swap (framenum, picnum);
	}
	if (SC_GetString())
	{
		if (SC_Compare ("Oscillate"))
		{
			type = type == FAnimDef::ANIM_Forward ? FAnimDef::ANIM_OscillateUp : FAnimDef::ANIM_OscillateDown;
		}
		else
		{
			SC_UnGet ();
		}
	}
	R_AddSimpleAnim (picnum, framenum - picnum + 1, type, min, max - min);
}

//==========================================================================
//
// ParsePicAnim
//
// Parse a single frame from ANIMDEFS defined using "pic".
//
//==========================================================================

static void ParsePicAnim (int picnum, int usetype, bool missing, TArray<FAnimDef::FAnimFrame> &frames)
{
	int framenum;
	DWORD min, max;

	framenum = ParseFramenum (picnum, usetype, missing);
	ParseTime (min, max);

	if (picnum >= 0)
	{
		FAnimDef::FAnimFrame frame;

		frame.SpeedMin = min;
		frame.SpeedRange = max - min;
		frame.FramePic = framenum;
		frames.Push (frame);
	}
}

//==========================================================================
//
// ParseFramenum
//
// Reads a frame's texture from ANIMDEFS. It can either be an integral
// offset from basepicnum or a specific texture name.
//
//==========================================================================

static int ParseFramenum (int basepicnum, int usetype, bool allowMissing)
{
	const BITFIELD texflags = FTextureManager::TEXMAN_Overridable | FTextureManager::TEXMAN_TryAny;
	int framenum;

	SC_MustGetString ();
	if (IsNum (sc_String))
	{
		framenum = basepicnum + atoi(sc_String) - 1;
	}
	else
	{
		framenum = TexMan.CheckForTexture (sc_String, usetype, texflags);
		if (framenum < 0 && !allowMissing)
		{
			SC_ScriptError ("Unknown texture %s", sc_String);
		}
	}
	return framenum;
}

//==========================================================================
//
// ParseTime
//
// Reads a tics or rand time definition from ANIMDEFS.
//
//==========================================================================

static void ParseTime (DWORD &min, DWORD &max)
{
	SC_MustGetString ();
	if (SC_Compare ("tics"))
	{
		SC_MustGetFloat ();
		min = max = DWORD(sc_Float * 1000 / 35);
	}
	else if (SC_Compare ("rand"))
	{
		SC_MustGetFloat ();
		min = DWORD(sc_Float * 1000 / 35);
		SC_MustGetFloat ();
		max = DWORD(sc_Float * 1000 / 35);
	}
	else
	{
		SC_ScriptError ("Must specify a duration for animation frame");
	}
}

//==========================================================================
//
// AnimArray :: ~AnimArray
//
// Frees all animations held in this array before freeing the array.
//
//==========================================================================

AnimArray::~AnimArray()
{
	for (unsigned i = 0; i < Size(); i++)
	{
		if ((*this)[i] != NULL)
		{
			free ((*this)[i]);
			(*this)[i] = NULL;
		}
	}
}

//==========================================================================
//
// AnimArray :: AddAnim
//
// Adds a new animation to the array. If one with the same basepic as the
// new one already exists, it is replaced.
//
//==========================================================================

void AnimArray::AddAnim (FAnimDef *anim)
{
	// Search for existing duplicate.
	for (unsigned int i = 0; i < Anims.Size(); ++i)
	{
		if ((*this)[i]->BasePic == anim->BasePic)
		{
			// Found one!
			free ((*this)[i]);
			(*this)[i] = anim;
			return;
		}
	}
	// Didn't find one, so add it at the end.
	Push (anim);
}

//==========================================================================
//
// AnimArray :: FixAnimations
//
// Copy the "front sky" flag from an animated texture to the rest
// of the textures in the animation, and make every texture in an
// animation range use the same setting for bNoDecals.
//
//==========================================================================

void AnimArray::FixAnimations ()
{
	unsigned int i;
	int j;

	for (i = 0; i < Size(); ++i)
	{
		FAnimDef *anim = operator[] (i);
		if (anim->AnimType == FAnimDef::ANIM_DiscreteFrames)
		{
			if (TexMan[anim->BasePic]->bNoRemap0)
			{
				for (j = 0; j < anim->NumFrames; ++j)
				{
					TexMan[anim->Frames[j].FramePic]->SetFrontSkyLayer ();
				}
			}
		}
		else
		{
			bool nodecals;
			bool noremap = false;
			const char *name;

			name = TexMan[anim->BasePic]->Name;
			nodecals = TexMan[anim->BasePic]->bNoDecals;
			for (j = 0; j < anim->NumFrames; ++j)
			{
				FTexture *tex = TexMan[anim->BasePic + j];
				noremap |= tex->bNoRemap0;
				tex->bNoDecals = nodecals;
			}
			if (noremap)
			{
				for (j = 0; j < anim->NumFrames; ++j)
				{
					TexMan[anim->BasePic + j]->SetFrontSkyLayer ();
				}
			}
		}
	}
}

//==========================================================================
//
// FAnimDef :: SetSwitchTime
//
// Determines when to switch to the next frame.
//
//==========================================================================

void FAnimDef::SetSwitchTime (DWORD mstime)
{
	int speedframe = (AnimType == FAnimDef::ANIM_DiscreteFrames) ? CurFrame : 0;

	SwitchTime = mstime + Frames[speedframe].SpeedMin;
	if (Frames[speedframe].SpeedRange != 0)
	{
		SwitchTime += pr_animatepictures(Frames[speedframe].SpeedRange);
	}
}

//==========================================================================
//
// R_UpdateAnimations
//
// Updates texture translations for each animation and scrolls the skies.
//
//==========================================================================

void R_UpdateAnimations (DWORD mstime)
{
	for (unsigned int j = 0; j < Anims.Size(); ++j)
	{
		FAnimDef *anim = Anims[j];

		// If this is the first time through R_UpdateAnimations, just
		// initialize the anim's switch time without actually animating.
		if (anim->SwitchTime == 0)
		{
			anim->SetSwitchTime (mstime);
		}
		else while (anim->SwitchTime <= mstime)
		{ // Multiple frames may have passed since the last time calling
		  // R_UpdateAnimations, so be sure to loop through them all.

			switch (anim->AnimType)
			{
			default:
			case FAnimDef::ANIM_Forward:
			case FAnimDef::ANIM_DiscreteFrames:
				anim->CurFrame = (anim->CurFrame + 1) % anim->NumFrames;
				break;

			case FAnimDef::ANIM_Backward:
				if (anim->CurFrame == 0)
				{
					anim->CurFrame = anim->NumFrames - 1;
				}
				else
				{
					anim->CurFrame -= 1;
				}
				break;

			case FAnimDef::ANIM_OscillateUp:
				anim->CurFrame = anim->CurFrame + 1;
				if (anim->CurFrame >= anim->NumFrames - 1)
				{
					anim->AnimType = FAnimDef::ANIM_OscillateDown;
				}
				break;

			case FAnimDef::ANIM_OscillateDown:
				anim->CurFrame = anim->CurFrame - 1;
				if (anim->CurFrame == 0)
				{
					anim->AnimType = FAnimDef::ANIM_OscillateUp;
				}
				break;
			}
			anim->SetSwitchTime (mstime);
		}

		if (anim->AnimType == FAnimDef::ANIM_DiscreteFrames)
		{
			TexMan.SetTranslation (anim->BasePic, anim->Frames[anim->CurFrame].FramePic);
		}
		else
		{
			for (unsigned int i = 0; i < anim->NumFrames; i++)
			{
				TexMan.SetTranslation (anim->BasePic + i, anim->BasePic + (i + anim->CurFrame) % anim->NumFrames);
			}
		}
	}

	// Scroll the sky
	double ms = (double)mstime * FRACUNIT;
	sky1pos = fixed_t(fmod (ms * level.skyspeed1, double(TexMan[sky1texture]->GetWidth() << FRACBITS)));
	sky2pos = fixed_t(fmod (ms * level.skyspeed2, double(TexMan[sky2texture]->GetWidth() << FRACBITS)));
}
