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

#include "gl/gl_include.h"
#include "w_wad.h"
#include "cmdlib.h"
#include "sc_man.h"
#include "m_crc32.h"
#include "c_console.h"
#include "g_game.h"
#include "gl_models.h"
#include "gl_texture.h"
#include "gl_values.h"
#include "gl_renderstruct.h"
#include "doomstat.h"
#include "g_level.h"
#include "gl_geometric.h"
#include "gl_intern.h"

static inline float GetTimeFloat()
{
	return (float)I_MSTime() * (float)TICRATE / 1000.0f;
}

CVAR(Bool, gl_interpolate_model_frames, true, CVAR_ARCHIVE)
// [BB] Allow the user disable the use of any kind of models.
CVAR(Bool, gl_use_models, true, CVAR_ARCHIVE)
EXTERN_CVAR(Int, gl_fogmode)


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
	FModel * model;
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
		if (!stricmp(fullname, Models[i]->filename)) return Models[i];
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
	else
	{
		Printf("LoadModel: Unknown model format in '%s'\n", fullname.GetChars());
		delete buffer;
		return NULL;
	}

	if (!model->Load(path, buffer, len))
	{
		delete model;
		delete buffer;
		return NULL;
	}
	model->filename = copystring(fullname);
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
				if (!smf.type) sc.ScriptError("MODELDEF: Unknown actor type '%s'\n", sc.String);
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
							Printf("%s: model not found\n", sc.String);
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
					else if (sc.Compare("zoffset"))
					{
						sc.MustGetFloat();
						smf.zoffset=sc.Float;
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

FSpriteModelFrame * gl_FindModelFrame(const PClass * ti, int sprite, int frame)
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
						   Matrix3x4 *modeltoworld = NULL,
						   int translation = 0 )
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
					smfNext = gl_FindModelFrame(ti, nextState->sprite, nextState->GetFrame() );
			}
		}
	}

	for(int i=0; i<MAX_MODELS_PER_FRAME; i++)
	{
		FModel * mdl = smf->models[i];

		if (mdl!=NULL)
		{
			if ( smfNext && smf->modelframes[i] != smfNext->modelframes[i] )
				mdl->RenderFrameInterpolated(smf->skins[i], smf->modelframes[i], smfNext->modelframes[i], inter, cm, modeltoworld, translation);
			else
				mdl->RenderFrame(smf->skins[i], smf->modelframes[i], cm, modeltoworld, translation);
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
	FSpriteModelFrame * smf = spr->modelframe;


	// Setup transformation.
	gl.MatrixMode(GL_MODELVIEW);
	gl.PushMatrix();
	gl.DepthFunc(GL_LEQUAL);
	// [BB] In case the model should be rendered translucent, do back face culling.
	// This solves a few of the problems caused by the lack of depth sorting.
	// TO-DO: Implement proper depth sorting.
	if (!( spr->actor->RenderStyle == LegacyRenderStyles[STYLE_Normal] ))
	{
		gl.Enable(GL_CULL_FACE);
		glFrontFace(GL_CW);
	}

	Matrix3x4 ModelToWorld;
	Matrix3x4 *mat;

	if (gl_fogmode != 2 || !gl_fog_shader)
	{
		// Model space => World space
		gl.Translatef(spr->x, spr->z, spr->y );

		if ( !(smf->flags & MDL_ALIGNANGLE) )
			gl.Rotatef(-ANGLE_TO_FLOAT(spr->actor->angle), 0, 1, 0);
		// [BB] Change the angle so that the object is exactly facing the camera in the x/y plane.
		else
			gl.Rotatef( -ANGLE_TO_FLOAT ( R_PointToAngle ( spr->actor->x, spr->actor->y ) ), 0, 1, 0);

		// [BB] Change the pitch so that the object is vertically facing the camera (only makes sense combined with MDL_ALIGNANGLE).
		if ( (smf->flags & MDL_ALIGNPITCH) )
		{
			const fixed_t distance = R_PointToDist2( spr->actor->x - viewx, spr->actor->y - viewy );
			const float pitch = RAD_TO_FLOAT ( atan2( FIXED2FLOAT ( spr->actor->z - viewz ), FIXED2FLOAT ( distance ) ) );
			gl.Rotatef(pitch, 0, 0, 1);
		}
		// [BB] Workaround for the missing pitch information.
		else if ( (smf->flags & MDL_PITCHFROMMOMENTUM) )
		{
			const double x = static_cast<double>(spr->actor->momx);
			const double y = static_cast<double>(spr->actor->momy);
			const double z = static_cast<double>(spr->actor->momz);
			// [BB] Calculate the pitch using spherical coordinates.
			const double pitch = atan( z/sqrt(x*x+y*y) ) / M_PI * 180;

			gl.Rotatef(pitch, 0, 0, 1);
		}

		// [BB] Special flag for flat, beam like models.
		if ( (smf->flags & MDL_ROLLAGAINSTANGLE) )
			gl.Rotatef( gl_RollAgainstAngleHelper ( spr->actor ), 1, 0, 0);

		// Model rotation.
		// [BB] Added Doomsday like rotation of the weapon pickup models.
		// The rotation angle is based on the elapsed time.
		
		if( smf->flags & MDL_ROTATING )
		{
			float offsetAngle = 0.;
			const float time = smf->rotationSpeed*GetTimeFloat()/200.;
			offsetAngle = ( (time - static_cast<int>(time)) *360. );
			gl.Translatef(smf->rotationCenterX, smf->rotationCenterY, smf->rotationCenterZ);
			gl.Rotatef(offsetAngle, smf->xrotate, smf->yrotate, smf->zrotate);
			gl.Translatef(-smf->rotationCenterX, -smf->rotationCenterY, -smf->rotationCenterZ);
		} 		

		// Scaling and model space offset.
		gl.Scalef(	
			TO_GL(spr->actor->scaleX) * smf->xscale,
			TO_GL(spr->actor->scaleY) * smf->zscale,	// y scale for a sprite means height, i.e. z in the world!
			TO_GL(spr->actor->scaleX) * smf->yscale);

		// [BB] Apply zoffset here, needs to be scaled by 1 / smf->zscale, so that zoffset doesn't depend on the z-scaling.
		gl.Translatef(0., smf->zoffset / smf->zscale, 0.);
		mat = NULL;
	}
	else
	{
		// For radial fog we need to pass coordinates in world space in order to calculate distances.
		// That means that the local transformations cannot be part of the modelview matrix

		ModelToWorld.MakeIdentity();

		// Model space => World space
		ModelToWorld.Translate(spr->x, spr->z, spr->y);

		if ( !(smf->flags & MDL_ALIGNANGLE) )
			ModelToWorld.Rotate(0,1,0, -ANGLE_TO_FLOAT(spr->actor->angle));
		// [BB] Change the angle so that the object is exactly facing the camera in the x/y plane.
		else
			ModelToWorld.Rotate(0,1,0, -ANGLE_TO_FLOAT ( R_PointToAngle ( spr->actor->x, spr->actor->y ) ) );

		// [BB] Change the pitch so that the object is vertically facing the camera (only makes sense combined with MDL_ALIGNANGLE).
		if ( (smf->flags & MDL_ALIGNPITCH) )
		{
			const fixed_t distance = R_PointToDist2( spr->actor->x - viewx, spr->actor->y - viewy );
			const float pitch = RAD_TO_FLOAT ( atan2( FIXED2FLOAT ( spr->actor->z - viewz ), FIXED2FLOAT ( distance ) ) );
			ModelToWorld.Rotate(0,0,1,pitch);
		}
		// [BB] Workaround for the missing pitch information.
		else if ( (smf->flags & MDL_PITCHFROMMOMENTUM) )
		{
			const double x = static_cast<double>(spr->actor->momx);
			const double y = static_cast<double>(spr->actor->momy);
			const double z = static_cast<double>(spr->actor->momz);
			// [BB] Calculate the pitch using spherical coordinates.
			const double pitch = atan( z/sqrt(x*x+y*y) ) / M_PI * 180;

			ModelToWorld.Rotate(0,0,1,pitch);
		}

		// [BB] Special flag for flat, beam like models.
		if ( (smf->flags & MDL_ROLLAGAINSTANGLE) )
			ModelToWorld.Rotate(1, 0, 0, gl_RollAgainstAngleHelper ( spr->actor ));

		// Model rotation.
		// [BB] Added Doomsday like rotation of the weapon pickup models.
		// The rotation angle is based on the elapsed time.

		if( smf->flags & MDL_ROTATING )
		{
			float offsetAngle = 0.;
			const float time = smf->rotationSpeed*GetTimeFloat()/200.;
			offsetAngle = ( (time - static_cast<int>(time)) *360. );

			ModelToWorld.Translate(-smf->rotationCenterX, -smf->rotationCenterY, -smf->rotationCenterZ);
			ModelToWorld.Rotate(smf->xrotate, smf->yrotate, smf->zrotate, offsetAngle);
			ModelToWorld.Translate(smf->rotationCenterX, smf->rotationCenterY, smf->rotationCenterZ);
		}

		ModelToWorld.Scale(TO_GL(spr->actor->scaleX) * smf->xscale,
						   TO_GL(spr->actor->scaleY) * smf->zscale,	// y scale for a sprite means height, i.e. z in the world!
						   TO_GL(spr->actor->scaleX) * smf->yscale);


		// [BB] Apply zoffset here, needs to be scaled by 1 / smf->zscale, so that zoffset doesn't depend on the z-scaling.
		ModelToWorld.Translate(0., smf->zoffset / smf->zscale, 0.);

		mat = &ModelToWorld;
	}

	int translation = 0;
	if ( !(smf->flags & MDL_IGNORETRANSLATION) )
		translation = spr->actor->Translation;

	gl_RenderFrameModels( smf, spr->actor->state, spr->actor->tics, RUNTIME_TYPE(spr->actor), cm, mat, translation );

	gl.MatrixMode(GL_MODELVIEW);
	gl.PopMatrix();
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
	FSpriteModelFrame *smf = gl_FindModelFrame(playermo->player->ReadyWeapon->GetClass(), psp->state->sprite, psp->state->GetFrame());

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

	// Scaling and model space offset.
	gl.Scalef(	
		smf->xscale,
		smf->zscale,	// y scale for a sprite means height, i.e. z in the world!
		smf->yscale);

	// [BB] Apply zoffset here, needs to be scaled by 1 / smf->zscale, so that zoffset doesn't depend on the z-scaling.
	gl.Translatef(0., smf->zoffset / smf->zscale, 0.);

	// [BB] Weapon bob, very similar to the normal Doom weapon bob.
	gl.Rotatef(TO_GL(ofsx)/4, 0, 1, 0);
	gl.Rotatef(-TO_GL(ofsy-WEAPONTOP)/4, 1, 0, 0);

	// [BB] For some reason the jDoom models need to be rotated.
	gl.Rotatef(90., 0, 1, 0);

	gl_RenderFrameModels( smf, psp->state, psp->tics, playermo->player->ReadyWeapon->GetClass(), cm );

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
	FSpriteModelFrame *smf = gl_FindModelFrame(player->ReadyWeapon->GetClass(), state->sprite, state->GetFrame());
	return ( smf != NULL );
}
