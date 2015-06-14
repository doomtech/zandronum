/*
** gl_models.cpp
**
** General model handling code
**
**---------------------------------------------------------------------------
** Copyright 2005 Christoph Oelckers
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
** 4. When not used as part of GZDoom or a GZDoom derivative, this code will be
**    covered by the terms of the GNU Lesser General Public License as published
**    by the Free Software Foundation; either version 2.1 of the License, or (at
**    your option) any later version.
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

#include "gl/system/gl_system.h"
#include "w_wad.h"
#include "cmdlib.h"
#include "sc_man.h"
#include "m_crc32.h"
#include "c_console.h"
#include "g_game.h"
#include "doomstat.h"
#include "g_level.h"
#include "r_state.h"
#include "d_player.h"
//#include "resources/voxels.h"
//#include "gl/gl_intern.h"

#include "gl/renderer/gl_renderer.h"
#include "gl/scene/gl_drawinfo.h"
#include "gl/models/gl_models.h"
#include "gl/textures/gl_material.h"
#include "gl/utility/gl_geometric.h"
#include "gl/utility/gl_convert.h"
#include "gl/renderer/gl_renderstate.h"

// [BB/EP] New #includes. 
#include "r_main.h"
#include "gl/gl_functions.h"

static inline float GetTimeFloat()
{
	return (float)I_MSTime() * (float)TICRATE / 1000.0f;
}

CVAR(Bool, gl_interpolate_model_frames, true, CVAR_ARCHIVE)
CVAR(Bool, gl_light_models, true, CVAR_ARCHIVE)
// [BB] Allow the user disable the use of any kind of models.
CVAR(Bool, gl_use_models, true, CVAR_ARCHIVE)
EXTERN_CVAR(Int, gl_fogmode)
EXTERN_CVAR(Bool, gl_dynlight_shader)

extern TDeletingArray<FVoxel *> Voxels;
extern TDeletingArray<FVoxelDef *> VoxelDefs;


class DeletingModelArray : public TArray<FModel *>
{
public:

#if 0
	~DeletingModelArray()
	{
		for(unsigned i=0;i<Size();i++)
		{
			delete (*this)[i];
		}

	}
#endif
};

DeletingModelArray Models;

static TArray<FSpriteModelFrame> SpriteModelFrames;
static int * SpriteModelHash;
//TArray<FStateModelFrame> StateModelFrames;

static void DeleteModelHash()
{
	if (SpriteModelHash != NULL) delete [] SpriteModelHash;
	SpriteModelHash = NULL;
}

//===========================================================================
//
// FindGFXFile
//
//===========================================================================

static int FindGFXFile(FString & fn)
{
	int best = -1;
	int dot = fn.LastIndexOf('.');
	int slash = fn.LastIndexOf('/');
	if (dot > slash) fn.Truncate(dot);

	static const char * extensions[] = { ".png", ".jpg", ".tga", ".pcx", NULL };

	for (const char ** extp=extensions; *extp; extp++)
	{
		int lump = Wads.CheckNumForFullName(fn + *extp);
		if (lump >= best)  best = lump;
	}
	return best;
}


//===========================================================================
//
// LoadSkin
//
//===========================================================================

FTexture * LoadSkin(const char * path, const char * fn)
{
	FString buffer;

	buffer.Format("%s%s", path, fn);

	int texlump = FindGFXFile(buffer);
	if (texlump>=0)
	{
		FTextureID texno = TexMan.FindTextureByLumpNum(texlump);
		if (!texno.isValid())
		{
			FTexture *tex = FTexture::CreateTexture("", texlump, FTexture::TEX_Override);
			TexMan.AddTexture(tex);
			return tex;
		}
		return TexMan[texno];
	}
	else 
	{
		return NULL;
	}
}

//===========================================================================
//
// ModelFrameHash
//
//===========================================================================

static int ModelFrameHash(FSpriteModelFrame * smf)
{
	const DWORD *table = GetCRCTable ();
	DWORD hash = 0xffffffff;

	const char * s = (const char *)(&smf->type);	// this uses type, sprite and frame for hashing
	const char * se= (const char *)(&smf->hashnext);

	for (; s<se; s++)
	{
		hash = CRC1 (hash, *s, table);
	}
	return hash ^ 0xffffffff;
}

//===========================================================================
//
// FindModel
//
//===========================================================================

static FModel * FindModel(const char * path, const char * modelfile)
{
	FModel * model = NULL;
	FString fullname;

	fullname.Format("%s%s", path, modelfile);
	int lump = Wads.CheckNumForFullName(fullname);

	if (lump<0)
	{
		Printf("FindModel: '%s' not found\n", fullname.GetChars());
		return NULL;
	}

	for(int i = 0; i< (int)Models.Size(); i++)
	{
		if (!Models[i]->mFileName.CompareNoCase(fullname)) return Models[i];
	}

	int len = Wads.LumpLength(lump);
	FMemLump lumpd = Wads.ReadLump(lump);
	char * buffer = (char*)lumpd.GetMem();

	if (!memcmp(buffer, "DMDM", 4))
	{
		model = new FDMDModel;
	}
	else if (!memcmp(buffer, "IDP2", 4))
	{
		model = new FMD2Model;
	}
	else if (!memcmp(buffer, "IDP3", 4))
	{
		model = new FMD3Model;
	}

	if (model != NULL)
	{
		if (!model->Load(path, lump, buffer, len))
		{
			delete model;
			return NULL;
		}
	}
	else
	{
		// try loading as a voxel
		FVoxel *voxel = R_LoadKVX(lump);
		if (voxel != NULL)
		{
			model = new FVoxelModel(voxel, true);
		}
		else
		{
			Printf("LoadModel: Unknown model format in '%s'\n", fullname.GetChars());
			return NULL;
		}
	}

	model->mFileName = fullname;
	Models.Push(model);
	return model;
}

//===========================================================================
//
// gl_InitModels
//
//===========================================================================

void gl_InitModels()
{
	int Lump, lastLump;
	FString path;
	int index;
	int i;

	FSpriteModelFrame smf;

	lastLump = 0;

	for(unsigned i=0;i<Models.Size();i++)
	{
		delete Models[i];
	}
	Models.Clear();
	SpriteModelFrames.Clear();
	DeleteModelHash();

	// First, create models for each voxel
	for (unsigned i = 0; i < Voxels.Size(); i++)
	{
		FVoxelModel *md = new FVoxelModel(Voxels[i], false);
		Voxels[i]->VoxelIndex = Models.Push(md);
	}
	// now create GL model frames for the voxeldefs
	for (unsigned i = 0; i < VoxelDefs.Size(); i++)
	{
		FVoxelModel *md = (FVoxelModel*)Models[VoxelDefs[i]->Voxel->VoxelIndex];
		memset(&smf, 0, sizeof(smf));
		smf.models[0] = md;
		smf.skins[0] = md->GetPaletteTexture();
		smf.xscale = smf.yscale = smf.zscale = FIXED2FLOAT(VoxelDefs[i]->Scale);
		smf.angleoffset = VoxelDefs[i]->AngleOffset;
		if (VoxelDefs[i]->PlacedSpin != 0)
		{
			smf.yrotate = 1.f;
			smf.rotationSpeed = VoxelDefs[i]->PlacedSpin / 55.55f;
			smf.flags |= MDL_ROTATING;
		}
		VoxelDefs[i]->VoxeldefIndex = SpriteModelFrames.Push(smf);
		if (VoxelDefs[i]->PlacedSpin != VoxelDefs[i]->DroppedSpin)
		{
			if (VoxelDefs[i]->DroppedSpin != 0)
			{
				smf.yrotate = 1.f;
				smf.rotationSpeed = VoxelDefs[i]->DroppedSpin / 55.55f;
				smf.flags |= MDL_ROTATING;
			}
			else
			{
				smf.yrotate = 0;
				smf.rotationSpeed = 0;
				smf.flags &= ~MDL_ROTATING;
			}
			SpriteModelFrames.Push(smf);
		}
	}

	memset(&smf, 0, sizeof(smf));
	while ((Lump = Wads.FindLump("MODELDEF", &lastLump)) != -1)
	{
		FScanner sc(Lump);
		while (sc.GetString())
		{
			if (sc.Compare("model"))
			{
				sc.MustGetString();
				memset(&smf, 0, sizeof(smf));
				smf.xscale=smf.yscale=smf.zscale=1.f;

				smf.type = PClass::FindClass(sc.String);
				if (!smf.type || smf.type->Defaults == NULL) 
				{
					sc.ScriptError("MODELDEF: Unknown actor type '%s'\n", sc.String);
				}
				GetDefaultByType(smf.type)->hasmodel=true;
				sc.MustGetStringName("{");
				while (!sc.CheckString("}"))
				{
					sc.MustGetString();
					if (sc.Compare("path"))
					{
						sc.MustGetString();
						FixPathSeperator(sc.String);
						path = sc.String;
						if (path[(int)path.Len()-1]!='/') path+='/';
					}
					else if (sc.Compare("model"))
					{
						sc.MustGetNumber();
						index=sc.Number;
						if (index<0 || index>=MAX_MODELS_PER_FRAME)
						{
							sc.ScriptError("Too many models in %s", smf.type->TypeName.GetChars());
						}
						sc.MustGetString();
						FixPathSeperator(sc.String);
						smf.models[index] = FindModel(path.GetChars(), sc.String);
						if (!smf.models[index])
						{
							Printf("%s: model not found in %s\n", sc.String, path.GetChars());
						}
					}
					else if (sc.Compare("scale"))
					{
						sc.MustGetFloat();
						smf.xscale=sc.Float;
						sc.MustGetFloat();
						smf.yscale=sc.Float;
						sc.MustGetFloat();
						smf.zscale=sc.Float;
					}
					// [BB] Added zoffset reading. 
					// Now it must be considered deprecated.
					else if (sc.Compare("zoffset"))
					{
						sc.MustGetFloat();
						smf.zoffset=sc.Float;
					}
					// Offset reading.
					else if (sc.Compare("offset"))
					{
						sc.MustGetFloat();
						smf.xoffset = sc.Float;
						sc.MustGetFloat();
						smf.yoffset = sc.Float;
						sc.MustGetFloat();
						smf.zoffset = sc.Float;
					}
					// angleoffset, pitchoffset and rolloffset reading.
					else if (sc.Compare("angleoffset"))
					{
						sc.MustGetFloat();
						smf.angleoffset = FLOAT_TO_ANGLE(sc.Float);
					}
					else if (sc.Compare("pitchoffset"))
					{
						sc.MustGetFloat();
						smf.pitchoffset = sc.Float;
					}
					else if (sc.Compare("rolloffset"))
					{
						sc.MustGetFloat();
						smf.rolloffset = sc.Float;
					}
					// [BB] Added model flags reading.
					else if (sc.Compare("ignoretranslation"))
					{
						smf.flags |= MDL_IGNORETRANSLATION;
					}
					else if (sc.Compare("pitchfrommomentum"))
					{
						smf.flags |= MDL_PITCHFROMMOMENTUM;
					}
					else if (sc.Compare("inheritactorpitch"))
					{
						smf.flags |= MDL_INHERITACTORPITCH;
					}
					else if (sc.Compare("inheritactorroll"))
					{
						smf.flags |= MDL_INHERITACTORROLL;
					}
					else if (sc.Compare("rotating"))
					{
						smf.flags |= MDL_ROTATING;
						smf.xrotate = 0.;
						smf.yrotate = 1.;
						smf.zrotate = 0.;
						smf.rotationCenterX = 0.;
						smf.rotationCenterY = 0.;
						smf.rotationCenterZ = 0.;
						smf.rotationSpeed = 1.;
					}
					else if (sc.Compare("rotation-speed"))
					{
						sc.MustGetFloat();
						smf.rotationSpeed = sc.Float;
					}
					else if (sc.Compare("rotation-vector"))
					{
						sc.MustGetFloat();
						smf.xrotate = sc.Float;
						sc.MustGetFloat();
						smf.yrotate = sc.Float;
						sc.MustGetFloat();
						smf.zrotate = sc.Float;
					}
					else if (sc.Compare("rotation-center"))
					{
						sc.MustGetFloat();
						smf.rotationCenterX = sc.Float;
						sc.MustGetFloat();
						smf.rotationCenterY = sc.Float;
						sc.MustGetFloat();
						smf.rotationCenterZ = sc.Float;
					}
					else if (sc.Compare("interpolatedoubledframes"))
					{
						smf.flags |= MDL_INTERPOLATEDOUBLEDFRAMES;
					}
					else if (sc.Compare("nointerpolation"))
					{
						smf.flags |= MDL_NOINTERPOLATION;
					}
					else if (sc.Compare("alignangle"))
					{
						smf.flags |= MDL_ALIGNANGLE;
					}
					else if (sc.Compare("alignpitch"))
					{
						smf.flags |= MDL_ALIGNPITCH;
					}
					else if (sc.Compare("rollagainstangle"))
					{
						smf.flags |= MDL_ROLLAGAINSTANGLE;
					}
					else if (sc.Compare("skin"))
					{
						sc.MustGetNumber();
						index=sc.Number;
						if (index<0 || index>=MAX_MODELS_PER_FRAME)
						{
							sc.ScriptError("Too many models in %s", smf.type->TypeName.GetChars());
						}
						sc.MustGetString();
						FixPathSeperator(sc.String);
						if (sc.Compare(""))
						{
							smf.skins[index]=NULL;
						}
						else
						{
							smf.skins[index]=LoadSkin(path.GetChars(), sc.String);
							if (smf.skins[index] == NULL)
							{
								Printf("Skin '%s' not found in '%s'\n",
									sc.String, smf.type->TypeName.GetChars());
							}
						}
					}
					else if (sc.Compare("frameindex") || sc.Compare("frame"))
					{
						bool isframe=!!sc.Compare("frame");

						sc.MustGetString();
						smf.sprite = -1;
						for (i = 0; i < (int)sprites.Size (); ++i)
						{
							if (strncmp (sprites[i].name, sc.String, 4) == 0)
							{
								if (sprites[i].numframes==0)
								{
									//sc.ScriptError("Sprite %s has no frames", sc.String);
								}
								smf.sprite = i;
								break;
							}
						}
						if (smf.sprite==-1)
						{
							sc.ScriptError("Unknown sprite %s in model definition for %s", sc.String, smf.type->TypeName.GetChars());
						}

						sc.MustGetString();
						FString framechars = sc.String;

						sc.MustGetNumber();
						index=sc.Number;
						if (index<0 || index>=MAX_MODELS_PER_FRAME)
						{
							sc.ScriptError("Too many models in %s", smf.type->TypeName.GetChars());
						}
						if (isframe)
						{
							sc.MustGetString();
							if (smf.models[index]!=NULL) 
							{
								smf.modelframes[index] = smf.models[index]->FindFrame(sc.String);
								if (smf.modelframes[index]==-1) sc.ScriptError("Unknown frame '%s' in %s", sc.String, smf.type->TypeName.GetChars());
							}
							else smf.modelframes[index] = -1;
						}
						else
						{
							sc.MustGetNumber();
							smf.modelframes[index] = sc.Number;
						}

						for(i=0; framechars[i]>0; i++)
						{
							char map[29]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
							int c = toupper(framechars[i])-'A';

							if (c<0 || c>=29)
							{
								sc.ScriptError("Invalid frame character %c found", c+'A');
							}
							if (map[c]) continue;
							smf.frame=c;
							SpriteModelFrames.Push(smf);
							map[c]=1;
						}
					}
					else
					{
						sc.ScriptMessage("Unrecognized string \"%s\"", sc.String);
					}
				}
			}
		}
	}

	// create a hash table for quick access
	SpriteModelHash = new int[SpriteModelFrames.Size ()];
	atterm(DeleteModelHash);
	memset(SpriteModelHash, 0xff, SpriteModelFrames.Size () * sizeof(int));

	for (i = 0; i < (int)SpriteModelFrames.Size (); i++)
	{
		int j = ModelFrameHash(&SpriteModelFrames[i]) % SpriteModelFrames.Size ();

		SpriteModelFrames[i].hashnext = SpriteModelHash[j];
		SpriteModelHash[j]=i;
	}
}


//===========================================================================
//
// gl_FindModelFrame
//
//===========================================================================
EXTERN_CVAR (Bool, r_drawvoxels)

FSpriteModelFrame * gl_FindModelFrame(const PClass * ti, int sprite, int frame, bool dropped)
{
	// [BB] The user doesn't want to use models, so just pretend that there is no model for this frame.
	if ( gl_use_models == false )
		return NULL;

	if (GetDefaultByType(ti)->hasmodel)
	{
		FSpriteModelFrame smf;

		memset(&smf, 0, sizeof(smf));
		smf.type=ti;
		smf.sprite=sprite;
		smf.frame=frame;

		int hash = SpriteModelHash[ModelFrameHash(&smf) % SpriteModelFrames.Size()];

		while (hash>=0)
		{
			FSpriteModelFrame * smff = &SpriteModelFrames[hash];
			if (smff->type==ti && smff->sprite==sprite && smff->frame==frame) return smff;
			hash=smff->hashnext;
		}
	}

	// Check for voxel replacements
	if (r_drawvoxels)
	{
		spritedef_t *sprdef = &sprites[sprite];
		if (frame < sprdef->numframes)
		{
			spriteframe_t *sprframe = &SpriteFrames[sprdef->spriteframes + frame];
			if (sprframe->Voxel != NULL)
			{
				int index = sprframe->Voxel->VoxeldefIndex;
				if (dropped && sprframe->Voxel->DroppedSpin !=sprframe->Voxel->PlacedSpin) index++;
				return &SpriteModelFrames[index];
			}
		}
	}
	return NULL;
}


//===========================================================================
//
// gl_RenderModel
//
//===========================================================================

void gl_RenderFrameModels( const FSpriteModelFrame *smf,
						   const FState *curState,
						   const int curTics,
						   const PClass *ti,
						   int cm,
						   Matrix3x4 *normaltransform,
						   int translation)
{
	// [BB] Frame interpolation: Find the FSpriteModelFrame smfNext which follows after smf in the animation
	// and the scalar value inter ( element of [0,1) ), both necessary to determine the interpolated frame.
	FSpriteModelFrame * smfNext = NULL;
	double inter = 0.;
	if( gl_interpolate_model_frames && !(smf->flags & MDL_NOINTERPOLATION) )
	{
		FState *nextState = curState->GetNextState( );
		if( curState != nextState && nextState )
		{
			// [BB] To interpolate at more than 35 fps we take tic fractions into account.
			float ticFraction = 0.;
			// [BB] In case the tic counter is frozen we have to leave ticFraction at zero.
			if ( ConsoleState == c_up && menuactive != MENU_On && !(level.flags2 & LEVEL2_FROZEN) )
			{
				float time = GetTimeFloat();
				ticFraction = (time - static_cast<int>(time));
			}
			inter = static_cast<double>(curState->Tics - curTics - ticFraction)/static_cast<double>(curState->Tics);

			// [BB] For some actors (e.g. ZPoisonShroom) spr->actor->tics can be bigger than curState->Tics.
			// In this case inter is negative and we need to set it to zero.
			if ( inter < 0. )
				inter = 0.;
			else
			{
				// [BB] Workaround for actors that use the same frame twice in a row.
				// Most of the standard Doom monsters do this in their see state.
				if ( (smf->flags & MDL_INTERPOLATEDOUBLEDFRAMES) )
				{
					const FState *prevState = curState - 1;
					if ( (curState->sprite == prevState->sprite) && ( curState->Frame == prevState->Frame) )
					{
						inter /= 2.;
						inter += 0.5;
					}
					if ( (curState->sprite == nextState->sprite) && ( curState->Frame == nextState->Frame) )
					{
						inter /= 2.;
						nextState = nextState->GetNextState( );
					}
				}
				if ( inter != 0.0 )
					smfNext = gl_FindModelFrame(ti, nextState->sprite, nextState->GetFrame(), false);
			}
		}
	}

	for(int i=0; i<MAX_MODELS_PER_FRAME; i++)
	{
		FModel * mdl = smf->models[i];

		if (mdl!=NULL)
		{
			if ( smfNext && smf->modelframes[i] != smfNext->modelframes[i] )
				mdl->RenderFrameInterpolated(smf->skins[i], smf->modelframes[i], smfNext->modelframes[i], inter, cm, translation);
			else
				mdl->RenderFrame(smf->skins[i], smf->modelframes[i], cm, translation);
		}
	}
}

// [BB] Small helper function for MDL_ROLLAGAINSTANGLE.
float gl_RollAgainstAngleHelper ( const AActor *actor )
{
	float angleDiff = ANGLE_TO_FLOAT ( R_PointToAngle ( actor->x, actor->y ) ) - ANGLE_TO_FLOAT ( actor->angle );
	if ( angleDiff > 180 )
		angleDiff -= 360;
	else if ( angleDiff < -180 )
		angleDiff += 360;
	if ( actor->z > viewz )
		angleDiff *= -1;
	if ( ( angleDiff < 90 ) && ( angleDiff > - 90 ) )
		angleDiff *= -1;
	return angleDiff;
}

void gl_RenderModel(GLSprite * spr, int cm)
{
	// [BB/EP] Take care of gl_fogmode and ZADF_FORCE_GL_DEFAULTS.
	OVERRIDE_FOGMODE_IF_NECESSARY

	FSpriteModelFrame * smf = spr->modelframe;


	// Setup transformation.
	gl.DepthFunc(GL_LEQUAL);
	gl_RenderState.SetTextureMode(TM_MODULATE);
	gl_RenderState.EnableTexture(true);
	// [BB] In case the model should be rendered translucent, do back face culling.
	// This solves a few of the problems caused by the lack of depth sorting.
	// TO-DO: Implement proper depth sorting.
	if (!( spr->actor->RenderStyle == LegacyRenderStyles[STYLE_Normal] ))
	{
		gl.Enable(GL_CULL_FACE);
		glFrontFace(GL_CW);
	}

	int translation = 0;
	if ( !(smf->flags & MDL_IGNORETRANSLATION) )
		translation = spr->actor->Translation;


	// y scale for a sprite means height, i.e. z in the world!
	float scaleFactorX = FIXED2FLOAT(spr->actor->scaleX) * smf->xscale;
	float scaleFactorY = FIXED2FLOAT(spr->actor->scaleX) * smf->yscale;
	float scaleFactorZ = FIXED2FLOAT(spr->actor->scaleY) * smf->zscale;
	float pitch = 0;
	float roll = 0;
	float rotateOffset = 0;
	float angle = ANGLE_TO_FLOAT(spr->actor->angle);

	// [BB] Workaround for the missing pitch information.
	if ( (smf->flags & MDL_PITCHFROMMOMENTUM) )
	{
		const double x = static_cast<double>(spr->actor->velx);
		const double y = static_cast<double>(spr->actor->vely);
		const double z = static_cast<double>(spr->actor->velz);
		
		// [BB] Calculate the pitch using spherical coordinates.
		if(z || x || y) pitch = float(atan( z/sqrt(x*x+y*y) ) / M_PI * 180);
				
        // Correcting pitch if model is moving backwards
        if(x || y) 
		{
			if((x * cos(angle * M_PI / 180) + y * sin(angle * M_PI / 180)) / sqrt(x * x + y * y) < 0) pitch *= -1;
		}
		else pitch = abs(pitch);
	}

	if( smf->flags & MDL_ROTATING )
	{
		const float time = smf->rotationSpeed*GetTimeFloat()/200.f;
		rotateOffset = float((time - xs_FloorToInt(time)) *360.f );
	}

	// Added MDL_INHERITACTORPITCH and MDL_INHERITACTORROLL flags processing.
	// If both flags MDL_INHERITACTORPITCH and MDL_PITCHFROMMOMENTUM are set, the pitch sums up the actor pitch and the momentum vector pitch.
	// This is rather crappy way to transfer fixet_t type into angle in degrees, but its works!
	if(smf->flags & MDL_INHERITACTORPITCH) pitch += float(static_cast<double>(spr->actor->pitch >> 16) / (1 << 13) * 45 + static_cast<double>(spr->actor->pitch & 0x0000FFFF) / (1 << 29) * 45);
	if(smf->flags & MDL_INHERITACTORROLL) roll += float(static_cast<double>(spr->actor->roll >> 16) / (1 << 13) * 45 + static_cast<double>(spr->actor->roll & 0x0000FFFF) / (1 << 29) * 45);
		
	if (gl.shadermodel < 4)
	{
		gl.MatrixMode(GL_MODELVIEW);
		gl.PushMatrix();
	}
	else
	{
		gl.ActiveTexture(GL_TEXTURE7);	// Hijack the otherwise unused seventh texture matrix for the model to world transformation.
		gl.MatrixMode(GL_TEXTURE);
		gl.LoadIdentity();
	}

	// Model space => World space
	gl.Translatef(spr->x, spr->z, spr->y );	
	
	// Applying model transformations:
	// 1) Applying actor angle, pitch and roll to the model
	if ( !(smf->flags & MDL_ALIGNANGLE) )
		gl.Rotatef(-angle, 0, 1, 0);
	// [BB] Change the angle so that the object is exactly facing the camera in the x/y plane.
	else
		gl.Rotatef( -ANGLE_TO_FLOAT ( R_PointToAngle ( spr->actor->x, spr->actor->y ) ), 0, 1, 0);

	// [BB] Change the pitch so that the object is vertically facing the camera (only makes sense combined with MDL_ALIGNANGLE).
	if ( (smf->flags & MDL_ALIGNPITCH) )
	{
		const fixed_t distance = R_PointToDist2( spr->actor->x - viewx, spr->actor->y - viewy );
		const float pitch = RAD2DEG ( atan2( FIXED2FLOAT ( spr->actor->z - viewz ), FIXED2FLOAT ( distance ) ) );
		gl.Rotatef(pitch, 0, 0, 1);
	}
	else
		gl.Rotatef(pitch, 0, 0, 1);

	// [BB] Special flag for flat, beam like models.
	if ( (smf->flags & MDL_ROLLAGAINSTANGLE) )
		gl.Rotatef( gl_RollAgainstAngleHelper ( spr->actor ), 1, 0, 0);
	else
		gl.Rotatef(-roll, 1, 0, 0);
	
	// 2) Applying Doomsday like rotation of the weapon pickup models
	// The rotation angle is based on the elapsed time.
	
	if( smf->flags & MDL_ROTATING )
	{
		gl.Translatef(smf->rotationCenterX, smf->rotationCenterY, smf->rotationCenterZ);
		gl.Rotatef(rotateOffset, smf->xrotate, smf->yrotate, smf->zrotate);
		gl.Translatef(-smf->rotationCenterX, -smf->rotationCenterY, -smf->rotationCenterZ);
	}

	// 3) Scaling model.
	gl.Scalef(scaleFactorX, scaleFactorZ, scaleFactorY);

	// 4) Aplying model offsets (model offsets do not depend on model scalings).
	gl.Translatef(smf->xoffset / smf->xscale, smf->zoffset / smf->zscale, smf->yoffset / smf->yscale);
	
	// 5) Applying model rotations.
	gl.Rotatef(-ANGLE_TO_FLOAT(smf->angleoffset), 0, 1, 0);
	gl.Rotatef(smf->pitchoffset, 0, 0, 1);
	gl.Rotatef(-smf->rolloffset, 1, 0, 0);
		
	if (gl.shadermodel >= 4) gl.ActiveTexture(GL_TEXTURE0);

#if 0
	if (gl_light_models)
	{
		// The normal transform matrix only contains the inverse rotations and scalings but not the translations
		NormalTransform.MakeIdentity();

		NormalTransform.Scale(1.f/scaleFactorX, 1.f/scaleFactorZ, 1.f/scaleFactorY);
		if( smf->flags & MDL_ROTATING ) NormalTransform.Rotate(smf->xrotate, smf->yrotate, smf->zrotate, -rotateOffset);
		if (pitch != 0) NormalTransform.Rotate(0,0,1,-pitch);
		if (angle != 0) NormalTransform.Rotate(0,1,0, angle);

		gl_RenderFrameModels( smf, spr->actor->state, spr->actor->tics, RUNTIME_TYPE(spr->actor), cm, &ModelToWorld, &NormalTransform, translation );
	}
#endif

	gl_RenderFrameModels( smf, spr->actor->state, spr->actor->tics, RUNTIME_TYPE(spr->actor), cm, NULL, translation );

	if (gl.shadermodel < 4)
	{
		gl.MatrixMode(GL_MODELVIEW);
		gl.PopMatrix();
	}
	else
	{
		gl.ActiveTexture(GL_TEXTURE7);
		gl.MatrixMode(GL_TEXTURE);
		gl.LoadIdentity();
		gl.ActiveTexture(GL_TEXTURE0);
		gl.MatrixMode(GL_MODELVIEW);
	}

	gl.DepthFunc(GL_LESS);
	if (!( spr->actor->RenderStyle == LegacyRenderStyles[STYLE_Normal] ))
		gl.Disable(GL_CULL_FACE);
}


//===========================================================================
//
// gl_RenderHUDModel
//
//===========================================================================

void gl_RenderHUDModel(pspdef_t *psp, fixed_t ofsx, fixed_t ofsy, int cm)
{
	AActor * playermo=players[consoleplayer].camera;
	FSpriteModelFrame *smf = gl_FindModelFrame(playermo->player->ReadyWeapon->GetClass(), psp->state->sprite, psp->state->GetFrame(), false);

	// [BB] No model found for this sprite, so we can't render anything.
	if ( smf == NULL )
		return;

	// [BB] The model has to be drawn independtly from the position of the player,
	// so we have to reset the GL_MODELVIEW matrix.
	gl.MatrixMode(GL_MODELVIEW);
	gl.PushMatrix();
	gl.LoadIdentity();
	gl.DepthFunc(GL_LEQUAL);

	// [BB] In case the model should be rendered translucent, do back face culling.
	// This solves a few of the problems caused by the lack of depth sorting.
	// TO-DO: Implement proper depth sorting.
	if (!( playermo->RenderStyle == LegacyRenderStyles[STYLE_Normal] ))
	{
		gl.Enable(GL_CULL_FACE);
		glFrontFace(GL_CCW);
	}

	// Scaling model (y scale for a sprite means height, i.e. z in the world!).
	gl.Scalef(smf->xscale, smf->zscale, smf->yscale);
	
	// Aplying model offsets (model offsets do not depend on model scalings).
	gl.Translatef(smf->xoffset / smf->xscale, smf->zoffset / smf->zscale, smf->yoffset / smf->yscale);

	// [BB] Weapon bob, very similar to the normal Doom weapon bob.
	gl.Rotatef(FIXED2FLOAT(ofsx)/4, 0, 1, 0);
	gl.Rotatef(-FIXED2FLOAT(ofsy-WEAPONTOP)/4, 1, 0, 0);

	// [BB] For some reason the jDoom models need to be rotated.
	gl.Rotatef(90., 0, 1, 0);

	// Applying angleoffset, pitchoffset, rolloffset.
	gl.Rotatef(-ANGLE_TO_FLOAT(smf->angleoffset), 0, 1, 0);
	gl.Rotatef(smf->pitchoffset, 0, 0, 1);
	gl.Rotatef(-smf->rolloffset, 1, 0, 0);

	gl_RenderFrameModels( smf, psp->state, psp->tics, playermo->player->ReadyWeapon->GetClass(), cm, NULL, 0 );

	gl.MatrixMode(GL_MODELVIEW);
	gl.PopMatrix();
	gl.DepthFunc(GL_LESS);
	if (!( playermo->RenderStyle == LegacyRenderStyles[STYLE_Normal] ))
		gl.Disable(GL_CULL_FACE);
}

//===========================================================================
//
// gl_IsHUDModelForPlayerAvailable
//
//===========================================================================

bool gl_IsHUDModelForPlayerAvailable (player_t * player)
{
	if ( (player == NULL) || (player->ReadyWeapon == NULL) || (player->psprites[0].state == NULL) )
		return false;

	FState* state = player->psprites[0].state;
	FSpriteModelFrame *smf = gl_FindModelFrame(player->ReadyWeapon->GetClass(), state->sprite, state->GetFrame(), false);
	return ( smf != NULL );
}

//===========================================================================
//
// gl_CleanModelData
//
//===========================================================================

void gl_CleanModelData()
{
	for (unsigned i=0;i<Models.Size(); i++)
	{
		Models[i]->CleanGLData();
	}
}
