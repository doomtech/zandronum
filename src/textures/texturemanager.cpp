/*
** texturemanager.cpp
** The texture manager class
**
**---------------------------------------------------------------------------
** Copyright 2004-2008 Randy Heit
** Copyright 2006-2008 Christoph Oelckers
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
**
*/

#include "doomtype.h"
#include "w_wad.h"
#include "r_data.h"
#include "templates.h"
#include "i_system.h"
#include "r_translate.h"
#include "c_dispatch.h"
#include "v_text.h"
#include "sc_man.h"
#include "gi.h"
#include "st_start.h"
#include "cmdlib.h"
#include "g_level.h"

extern void R_InitBuildTiles();


FTextureManager TexMan;

//==========================================================================
//
// FTextureManager :: FTextureManager
//
//==========================================================================

FTextureManager::FTextureManager ()
{
	memset (HashFirst, -1, sizeof(HashFirst));
	// Texture 0 is a dummy texture used to indicate "no texture"
	AddTexture (new FDummyTexture);

}

//==========================================================================
//
// FTextureManager :: ~FTextureManager
//
//==========================================================================

FTextureManager::~FTextureManager ()
{
	for (unsigned int i = 0; i < Textures.Size(); ++i)
	{
		delete Textures[i].Texture;
	}
}

//==========================================================================
//
// FTextureManager :: CheckForTexture
//
//==========================================================================

FTextureID FTextureManager::CheckForTexture (const char *name, int usetype, BITFIELD flags)
{
	int i;
	int firstfound = -1;
	int firsttype = FTexture::TEX_Null;

	if (name == NULL || name[0] == '\0')
	{
		return FTextureID(-1);
	}
	// [RH] Doom counted anything beginning with '-' as "no texture".
	// Hopefully nobody made use of that and had textures like "-EMPTY",
	// because -NOFLAT- is a valid graphic for ZDoom.
	if (name[0] == '-' && name[1] == '\0')
	{
		return FTextureID(0);
	}
	i = HashFirst[MakeKey (name) % HASH_SIZE];

	while (i != HASH_END)
	{
		const FTexture *tex = Textures[i].Texture;

		if (stricmp (tex->Name, name) == 0)
		{
			// The name matches, so check the texture type
			if (usetype == FTexture::TEX_Any)
			{
				// All NULL textures should actually return 0
				if (tex->UseType == FTexture::TEX_FirstDefined && !(flags & TEXMAN_ReturnFirst)) return 0;
				return FTextureID(tex->UseType==FTexture::TEX_Null? 0 : i);
			}
			else if ((flags & TEXMAN_Overridable) && tex->UseType == FTexture::TEX_Override)
			{
				return FTextureID(i);
			}
			else if (tex->UseType == usetype)
			{
				return FTextureID(i);
			}
			else if (tex->UseType == FTexture::TEX_FirstDefined && usetype == FTexture::TEX_Wall)
			{
				if (!(flags & TEXMAN_ReturnFirst)) return FTextureID(0);
				else return FTextureID(i);
			}
			else if (tex->UseType == FTexture::TEX_Null && usetype == FTexture::TEX_Wall)
			{
				// We found a NULL texture on a wall -> return 0
				return FTextureID(0);
			}
			else
			{
				if (firsttype == FTexture::TEX_Null ||
					(firsttype == FTexture::TEX_MiscPatch &&
					 tex->UseType != firsttype &&
					 tex->UseType != FTexture::TEX_Null)
				   )
				{
					firstfound = i;
					firsttype = tex->UseType;
				}
			}
		}
		i = Textures[i].HashNext;
	}

	if ((flags & TEXMAN_TryAny) && usetype != FTexture::TEX_Any)
	{
		// Never return the index of NULL textures.
		if (firstfound != -1)
		{
			if (firsttype == FTexture::TEX_Null) return FTextureID(0);
			if (firsttype == FTexture::TEX_FirstDefined && !(flags & TEXMAN_ReturnFirst)) return FTextureID(0);
		}
		return FTextureID(firstfound);
	}

	return FTextureID(-1);
}

//==========================================================================
//
// FTextureManager :: ListTextures
//
//==========================================================================

int FTextureManager::ListTextures (const char *name, TArray<FTextureID> &list)
{
	int i;

	if (name == NULL || name[0] == '\0')
	{
		return 0;
	}
	// [RH] Doom counted anything beginning with '-' as "no texture".
	// Hopefully nobody made use of that and had textures like "-EMPTY",
	// because -NOFLAT- is a valid graphic for ZDoom.
	if (name[0] == '-' && name[1] == '\0')
	{
		return 0;
	}
	i = HashFirst[MakeKey (name) % HASH_SIZE];

	while (i != HASH_END)
	{
		const FTexture *tex = Textures[i].Texture;

		if (stricmp (tex->Name, name) == 0)
		{
			// NULL textures must be ignored.
			if (tex->UseType!=FTexture::TEX_Null) 
			{
				unsigned int j;
				for(j = 0; j < list.Size(); j++)
				{
					// Check for overriding definitions from newer WADs
					if (Textures[list[j].GetIndex()].Texture->UseType == tex->UseType) break;
				}
				if (j==list.Size()) list.Push(FTextureID(i));
			}
		}
		i = Textures[i].HashNext;
	}
	return list.Size();
}

//==========================================================================
//
// FTextureManager :: FindTextureByLumpNum
//
//==========================================================================

FTextureID FTextureManager::FindTextureByLumpNum (int lumpnum)
{
	if (lumpnum < 0)
	{
		return FTextureID(-1);
	}
	// This can't use hashing because using ReplaceTexture would break the hash chains. :(
	for(unsigned i = 0; i <Textures.Size(); i++)
	{
		if (Textures[i].Texture->SourceLump == lumpnum)
		{
			return FTextureID(i);
		}
	}
	return FTextureID(-1);
}


//==========================================================================
//
// FTextureManager :: GetTextures
//
//==========================================================================

FTextureID FTextureManager::GetTexture (const char *name, int usetype, BITFIELD flags)
{
	FTextureID i;

	if (name == NULL || name[0] == 0)
	{
		return FTextureID(0);
	}
	else
	{
		i = CheckForTexture (name, usetype, flags | TEXMAN_TryAny);
	}

	if (!i.Exists())
	{
		// Use a default texture instead of aborting like Doom did
		Printf ("Unknown texture: \"%s\"\n", name);
		i = DefaultTexture;
	}
	return FTextureID(i);
}

//==========================================================================
//
// FTextureManager :: FindTexture
//
//==========================================================================

FTexture *FTextureManager::FindTexture(const char *texname, int usetype, BITFIELD flags)
{
	FTextureID texnum = CheckForTexture (texname, usetype, flags);
	return !texnum.isValid()? NULL : Textures[texnum.GetIndex()].Texture;
}

//==========================================================================
//
// FTextureManager :: UnloadAll
//
//==========================================================================

void FTextureManager::UnloadAll ()
{
	for (unsigned int i = 0; i < Textures.Size(); ++i)
	{
		Textures[i].Texture->Unload ();
	}
}

//==========================================================================
//
// FTextureManager :: AddTexture
//
//==========================================================================

FTextureID FTextureManager::AddTexture (FTexture *texture)
{
	int bucket;
	int hash;

	if (texture == NULL) return FTextureID(-1);

	// Later textures take precedence over earlier ones

	// Textures without name can't be looked for
	if (texture->Name[0] != 0)
	{
		bucket = int(MakeKey (texture->Name) % HASH_SIZE);
		hash = HashFirst[bucket];
	}
	else
	{
		bucket = -1;
		hash = -1;
	}

	TextureHash hasher = { texture, hash };
	int trans = Textures.Push (hasher);
	Translation.Push (trans);
	if (bucket >= 0) HashFirst[bucket] = trans;
	return (texture->id = FTextureID(trans));
}

//==========================================================================
//
// FTextureManager :: CreateTexture
//
// Calls FTexture::CreateTexture and adds the texture to the manager.
//
//==========================================================================

FTextureID FTextureManager::CreateTexture (int lumpnum, int usetype)
{
	if (lumpnum != -1)
	{
		FTexture *out = FTexture::CreateTexture(lumpnum, usetype);

		if (out != NULL) return AddTexture (out);
		else
		{
			Printf (TEXTCOLOR_ORANGE "Invalid data encountered for texture %s\n", Wads.GetLumpFullPath(lumpnum).GetChars());
			return FTextureID(-1);
		}
	}
	return FTextureID(-1);
}

//==========================================================================
//
// FTextureManager :: ReplaceTexture
//
//==========================================================================

void FTextureManager::ReplaceTexture (FTextureID picnum, FTexture *newtexture, bool free)
{
	int index = picnum.GetIndex();
	if (unsigned(index) >= Textures.Size())
		return;

	FTexture *oldtexture = Textures[index].Texture;

	strcpy (newtexture->Name, oldtexture->Name);
	newtexture->UseType = oldtexture->UseType;
	Textures[index].Texture = newtexture;

	newtexture->id = oldtexture->id;
	if (free)
	{
		delete oldtexture;
	}
	else
	{
		oldtexture->id.SetInvalid();
	}
}

//==========================================================================
//
// FTextureManager :: AreTexturesCompatible
//
// Checks if 2 textures are compatible for a ranged animation
//
//==========================================================================

bool FTextureManager::AreTexturesCompatible (FTextureID picnum1, FTextureID picnum2)
{
	int index1 = picnum1.GetIndex();
	int index2 = picnum2.GetIndex();
	if (unsigned(index1) >= Textures.Size() || unsigned(index2) >= Textures.Size())
		return false;

	FTexture *texture1 = Textures[index1].Texture;
	FTexture *texture2 = Textures[index2].Texture;

	// both textures must be the same type.
	if (texture1 == NULL || texture2 == NULL || texture1->UseType != texture2->UseType)
		return false;

	// both textures must be from the same file
	for(unsigned i = 0; i < FirstTextureForFile.Size() - 1; i++)
	{
		if (index1 >= FirstTextureForFile[i] && index1 < FirstTextureForFile[i+1])
		{
			return (index2 >= FirstTextureForFile[i] && index2 < FirstTextureForFile[i+1]);
		}
	}
	return false;
}


//==========================================================================
//
// FTextureManager :: AddGroup
//
//==========================================================================

void FTextureManager::AddGroup(int wadnum, int ns, int usetype)
{
	int firsttx = Wads.GetFirstLump(wadnum);
	int lasttx = Wads.GetLastLump(wadnum);
	char name[9];

	name[8] = 0;

	// Go from first to last so that ANIMDEFS work as expected. However,
	// to avoid duplicates (and to keep earlier entries from overriding
	// later ones), the texture is only inserted if it is the one returned
	// by doing a check by name in the list of wads.

	for (; firsttx <= lasttx; ++firsttx)
	{
		if (Wads.GetLumpNamespace(firsttx) == ns)
		{
			Wads.GetLumpName (name, firsttx);

			if (Wads.CheckNumForName (name, ns) == firsttx)
			{
				CreateTexture (firsttx, usetype);
			}
			StartScreen->Progress();
		}
		else if (ns == ns_flats && Wads.GetLumpFlags(firsttx) & LUMPF_MAYBEFLAT)
		{
			if (Wads.CheckNumForName (name, ns) < firsttx)
			{
				CreateTexture (firsttx, usetype);
			}
			StartScreen->Progress();
		}
	}
}

//==========================================================================
//
// Adds all hires texture definitions.
//
//==========================================================================

void FTextureManager::AddHiresTextures (int wadnum)
{
	int firsttx = Wads.GetFirstLump(wadnum);
	int lasttx = Wads.GetLastLump(wadnum);

	char name[9];
	TArray<FTextureID> tlist;

	if (firsttx == -1 || lasttx == -1)
	{
		return;
	}

	name[8] = 0;

	for (;firsttx <= lasttx; ++firsttx)
	{
		if (Wads.GetLumpNamespace(firsttx) == ns_hires)
		{
			Wads.GetLumpName (name, firsttx);

			if (Wads.CheckNumForName (name, ns_hires) == firsttx)
			{
				tlist.Clear();
				int amount = ListTextures(name, tlist);
				if (amount == 0)
				{
					// A texture with this name does not yet exist
					FTexture * newtex = FTexture::CreateTexture (firsttx, FTexture::TEX_Any);
					if (newtex != NULL)
					{
						newtex->UseType=FTexture::TEX_Override;
						AddTexture(newtex);
					}
				}
				else
				{
					for(unsigned int i = 0; i < tlist.Size(); i++)
					{
						FTexture * newtex = FTexture::CreateTexture (firsttx, FTexture::TEX_Any);
						if (newtex != NULL)
						{
							FTexture * oldtex = Textures[tlist[i].GetIndex()].Texture;

							// Replace the entire texture and adjust the scaling and offset factors.
							newtex->bWorldPanning = true;
							newtex->SetScaledSize(oldtex->GetScaledWidth(), oldtex->GetScaledHeight());
							newtex->LeftOffset = FixedMul(oldtex->GetScaledLeftOffset(), newtex->xScale);
							newtex->TopOffset = FixedMul(oldtex->GetScaledTopOffset(), newtex->yScale);
							ReplaceTexture(tlist[i], newtex, true);
						}
					}
				}
				StartScreen->Progress();
			}
		}
	}
}

//==========================================================================
//
// Loads the HIRESTEX lumps
//
//==========================================================================

void FTextureManager::LoadTextureDefs(int wadnum, const char *lumpname)
{
	int remapLump, lastLump;
	char src[9];
	bool is32bit;
	int width, height;
	int type, mode;
	TArray<FTextureID> tlist;

	lastLump = 0;
	src[8] = '\0';

	while ((remapLump = Wads.FindLump(lumpname, &lastLump)) != -1)
	{
		if (Wads.GetLumpFile(remapLump) == wadnum)
		{
			FScanner sc(remapLump);
			while (sc.GetString())
			{
				if (sc.Compare("remap")) // remap an existing texture
				{
					sc.MustGetString();

					// allow selection by type
					if (sc.Compare("wall")) type=FTexture::TEX_Wall, mode=FTextureManager::TEXMAN_Overridable;
					else if (sc.Compare("flat")) type=FTexture::TEX_Flat, mode=FTextureManager::TEXMAN_Overridable;
					else if (sc.Compare("sprite")) type=FTexture::TEX_Sprite, mode=0;
					else type = FTexture::TEX_Any, mode = 0;

					if (type != FTexture::TEX_Any) sc.MustGetString();

					sc.String[8]=0;

					tlist.Clear();
					int amount = ListTextures(sc.String, tlist);
					FName texname = sc.String;

					sc.MustGetString();
					int lumpnum = Wads.CheckNumForFullName(sc.String, true, ns_patches);
					if (lumpnum == -1) lumpnum = Wads.CheckNumForFullName(sc.String, true, ns_graphics);

					if (tlist.Size() == 0)
					{
						Printf("Attempting to remap non-existent texture %s to %s\n",
							texname.GetChars(), sc.String);
					}
					else if (lumpnum == -1)
					{
						Printf("Attempting to remap texture %s to non-existent lump %s\n",
							texname.GetChars(), sc.String);
					}
					else
					{
						for(unsigned int i = 0; i < tlist.Size(); i++)
						{
							FTexture * oldtex = Textures[tlist[i].GetIndex()].Texture;
							int sl;

							// only replace matching types. For sprites also replace any MiscPatches
							// based on the same lump. These can be created for icons.
							if (oldtex->UseType == type || type == FTexture::TEX_Any ||
								(mode == TEXMAN_Overridable && oldtex->UseType == FTexture::TEX_Override) ||
								(type == FTexture::TEX_Sprite && oldtex->UseType == FTexture::TEX_MiscPatch &&
								(sl=oldtex->GetSourceLump()) >= 0 && Wads.GetLumpNamespace(sl) == ns_sprites)
								)
							{
								FTexture * newtex = FTexture::CreateTexture (lumpnum, FTexture::TEX_Any);
								if (newtex != NULL)
								{
									// Replace the entire texture and adjust the scaling and offset factors.
									newtex->bWorldPanning = true;
									newtex->SetScaledSize(oldtex->GetScaledWidth(), oldtex->GetScaledHeight());
									newtex->LeftOffset = FixedMul(oldtex->GetScaledLeftOffset(), newtex->xScale);
									newtex->TopOffset = FixedMul(oldtex->GetScaledTopOffset(), newtex->yScale);
									ReplaceTexture(tlist[i], newtex, true);
								}
							}
						}
					}
				}
				else if (sc.Compare("define")) // define a new "fake" texture
				{
					sc.GetString();
					
					FString base = ExtractFileBase(sc.String, false);
					if (!base.IsEmpty())
					{
						strncpy(src, base, 8);

						int lumpnum = Wads.CheckNumForFullName(sc.String, true, ns_patches);
						if (lumpnum == -1) lumpnum = Wads.CheckNumForFullName(sc.String, true, ns_graphics);

						sc.GetString();
						is32bit = !!sc.Compare("force32bit");
						if (!is32bit) sc.UnGet();

						sc.GetNumber();
						width = sc.Number;
						sc.GetNumber();
						height = sc.Number;

						if (lumpnum>=0)
						{
							FTexture *newtex = FTexture::CreateTexture(lumpnum, FTexture::TEX_Override);

							if (newtex != NULL)
							{
								// Replace the entire texture and adjust the scaling and offset factors.
								newtex->bWorldPanning = true;
								newtex->SetScaledSize(width, height);
								memcpy(newtex->Name, src, sizeof(newtex->Name));

								FTextureID oldtex = TexMan.CheckForTexture(src, FTexture::TEX_MiscPatch);
								if (oldtex.isValid()) 
								{
									ReplaceTexture(oldtex, newtex, true);
									newtex->UseType = FTexture::TEX_Override;
								}
								else AddTexture(newtex);
							}
						}
					}				
					//else Printf("Unable to define hires texture '%s'\n", tex->Name);
				}
				else if (sc.Compare("texture"))
				{
					ParseXTexture(sc, FTexture::TEX_Override);
				}
				else if (sc.Compare("sprite"))
				{
					ParseXTexture(sc, FTexture::TEX_Sprite);
				}
				else if (sc.Compare("walltexture"))
				{
					ParseXTexture(sc, FTexture::TEX_Wall);
				}
				else if (sc.Compare("flat"))
				{
					ParseXTexture(sc, FTexture::TEX_Flat);
				}
				else if (sc.Compare("graphic"))
				{
					ParseXTexture(sc, FTexture::TEX_MiscPatch);
				}
				else
				{
					sc.ScriptError("Texture definition expected, found '%s'", sc.String);
				}
			}
		}
	}
}

//==========================================================================
//
// FTextureManager :: AddPatches
//
//==========================================================================

void FTextureManager::AddPatches (int lumpnum)
{
	FWadLump *file = Wads.ReopenLumpNum (lumpnum);
	DWORD numpatches, i;
	char name[9];

	*file >> numpatches;
	name[8] = 0;

	for (i = 0; i < numpatches; ++i)
	{
		file->Read (name, 8);

		if (CheckForTexture (name, FTexture::TEX_WallPatch, false) == -1)
		{
			CreateTexture (Wads.CheckNumForName (name, ns_patches), FTexture::TEX_WallPatch);
		}
		StartScreen->Progress();
	}

	delete file;
}


//==========================================================================
//
// FTextureManager :: LoadTexturesX
//
// Initializes the texture list with the textures from the world map.
//
//==========================================================================

void FTextureManager::LoadTextureX(int wadnum)
{
	// Use the most recent PNAMES for this WAD.
	// Multiple PNAMES in a WAD will be ignored.
	int pnames = Wads.CheckNumForName("PNAMES", ns_global, wadnum, false);

	if (pnames < 0)
	{
		// should never happen except for zdoom.pk3
		return;
	}

	// Only add the patches if the PNAMES come from the current file
	// Otherwise they have already been processed.
	if (Wads.GetLumpFile(pnames) == wadnum) TexMan.AddPatches (pnames);

	int texlump1 = Wads.CheckNumForName ("TEXTURE1", ns_global, wadnum);
	int texlump2 = Wads.CheckNumForName ("TEXTURE2", ns_global, wadnum);
	AddTexturesLumps (texlump1, texlump2, pnames);
}

//==========================================================================
//
// FTextureManager :: AddTexturesForWad
//
//==========================================================================

void FTextureManager::AddTexturesForWad(int wadnum)
{
	int firsttexture = Textures.Size();
	int lumpcount = Wads.GetNumLumps();

	FirstTextureForFile.Push(firsttexture);

	// First step: Load sprites
	AddGroup(wadnum, ns_sprites, FTexture::TEX_Sprite);

	// When loading a Zip, all graphics in the patches/ directory should be
	// added as well.
	AddGroup(wadnum, ns_patches, FTexture::TEX_WallPatch);

	// Second step: TEXTUREx lumps
	LoadTextureX(wadnum);

	// Third step: Flats
	AddGroup(wadnum, ns_flats, FTexture::TEX_Flat);

	// Fourth step: Textures (TX_)
	AddGroup(wadnum, ns_newtextures, FTexture::TEX_Override);

	// Sixth step: Try to find any lump in the WAD that may be a texture and load as a TEX_MiscPatch
	int firsttx = Wads.GetFirstLump(wadnum);
	int lasttx = Wads.GetLastLump(wadnum);

	for (int i= firsttx; i <= lasttx; i++)
	{
		char name[9];
		Wads.GetLumpName(name, i);
		name[8]=0;

		// Ignore anything not in the global namespace
		int ns = Wads.GetLumpNamespace(i);
		if (ns == ns_global)
		{
			// In Zips all graphics must be in a separate namespace.
			if (Wads.GetLumpFlags(i) & LUMPF_ZIPFILE) continue;

			// Ignore lumps with empty names.
			if (Wads.CheckLumpName(i, "")) continue;

			// Ignore anything belonging to a map
			if (Wads.CheckLumpName(i, "THINGS")) continue;
			if (Wads.CheckLumpName(i, "LINEDEFS")) continue;
			if (Wads.CheckLumpName(i, "SIDEDEFS")) continue;
			if (Wads.CheckLumpName(i, "VERTEXES")) continue;
			if (Wads.CheckLumpName(i, "SEGS")) continue;
			if (Wads.CheckLumpName(i, "SSECTORS")) continue;
			if (Wads.CheckLumpName(i, "NODES")) continue;
			if (Wads.CheckLumpName(i, "SECTORS")) continue;
			if (Wads.CheckLumpName(i, "REJECT")) continue;
			if (Wads.CheckLumpName(i, "BLOCKMAP")) continue;
			if (Wads.CheckLumpName(i, "BEHAVIOR")) continue;

			// Don't bother looking at this lump if something later overrides it.
			if (Wads.CheckNumForName(name, ns_graphics) != i) continue;

			// skip this if it has already been added as a wall patch.
			if (CheckForTexture(name, FTexture::TEX_WallPatch, 0).Exists()) continue;
		}
		else if (ns == ns_graphics)
		{
			// Don't bother looking this lump if something later overrides it.
			if (Wads.CheckNumForName(name, ns_graphics) != i) continue;
		}
		else continue;

		// Try to create a texture from this lump and add it.
		// Unfortunately we have to look at everything that comes through here...
		FTexture *out = FTexture::CreateTexture(i, FTexture::TEX_MiscPatch);

		if (out != NULL) 
		{
			AddTexture (out);
		}
	}

	// Seventh step: Check for hires replacements.
	AddHiresTextures(wadnum);
	LoadTextureDefs(wadnum, "TEXTURES");
	LoadTextureDefs(wadnum, "HIRESTEX");
	SortTexturesByType(firsttexture, Textures.Size());
}

//==========================================================================
//
// FTextureManager :: SortTexturesByType
// sorts newly added textures by UseType so that anything defined
// in TEXTURES and HIRESTEX gets in its proper place.
//
//==========================================================================

void FTextureManager::SortTexturesByType(int start, int end)
{
	TArray<FTexture *> newtextures;

	// First unlink all newly added textures from the hash chain
	for (int i = 0; i < HASH_SIZE; i++)
	{
		while (HashFirst[i] >= start && HashFirst[i] != HASH_END)
		{
			HashFirst[i] = Textures[HashFirst[i]].HashNext;
		}
	}
	newtextures.Resize(end-start);
	for(int i=start; i<end; i++)
	{
		newtextures[i-start] = Textures[i].Texture;
	}
	Textures.Resize(start);
	Translation.Resize(start);

	static int texturetypes[] = {
		FTexture::TEX_Sprite, FTexture::TEX_Null, FTexture::TEX_FirstDefined, 
		FTexture::TEX_WallPatch, FTexture::TEX_Wall, FTexture::TEX_Flat, 
		FTexture::TEX_Override, FTexture::TEX_MiscPatch 
	};

	for(unsigned int i=0;i<countof(texturetypes);i++)
	{
		for(unsigned j = 0; j<newtextures.Size(); j++)
		{
			if (newtextures[j] != NULL && newtextures[j]->UseType == texturetypes[i])
			{
				AddTexture(newtextures[j]);
				newtextures[j] = NULL;
			}
		}
	}
	// This should never happen. All other UseTypes are only used outside
	for(unsigned j = 0; j<newtextures.Size(); j++)
	{
		if (newtextures[j] != NULL)
		{
			Printf("Texture %s has unknown type!\n", newtextures[j]->Name);
			AddTexture(newtextures[j]);
		}
	}
}

//==========================================================================
//
// FTextureManager :: Init
//
//==========================================================================

void FTextureManager::Init()
{
	FTexture::InitGrayMap();

	int wadcnt = Wads.GetNumWads();
	for(int i = 0; i< wadcnt; i++)
	{
		AddTexturesForWad(i);
	}

	// Add one marker so that the last WAD is easier to handle and treat
	// Build tiles as a completely separate block.
	FirstTextureForFile.Push(Textures.Size());
	R_InitBuildTiles ();
	FirstTextureForFile.Push(Textures.Size());

	DefaultTexture = CheckForTexture ("-NOFLAT-", FTexture::TEX_Override, 0);

	// The Hexen scripts use BLANK as a blank texture, even though it's really not.
	// I guess the Doom renderer must have clipped away the line at the bottom of
	// the texture so it wasn't visible. I'll just map it to 0, so it really is blank.
	if (gameinfo.gametype == GAME_Hexen)
	{
		FTextureID tex = CheckForTexture ("BLANK", FTexture::TEX_Wall, false);
		if (tex.Exists())
		{
			SetTranslation (tex, 0);
		}
	}

	// Hexen parallax skies use color 0 to indicate transparency on the front
	// layer, so we must not remap color 0 on these textures. Unfortunately,
	// the only way to identify these textures is to check the MAPINFO.
	for (unsigned int i = 0; i < wadlevelinfos.Size(); ++i)
	{
		if (wadlevelinfos[i].flags & LEVEL_DOUBLESKY)
		{
			FTextureID picnum = CheckForTexture (wadlevelinfos[i].skypic1, FTexture::TEX_Wall, false);
			if (picnum.isValid())
			{
				Textures[picnum.GetIndex()].Texture->SetFrontSkyLayer ();
			}
		}
	}
}

//==========================================================================
//
// FTextureManager :: WriteTexture
//
//==========================================================================

void FTextureManager::WriteTexture (FArchive &arc, int picnum)
{
	FTexture *pic;

	if (picnum < 0)
	{
		arc.WriteName(NULL);
		return;
	}
	else if ((size_t)picnum >= Textures.Size())
	{
		pic = Textures[0].Texture;
	}
	else
	{
		pic = Textures[picnum].Texture;
	}

	arc.WriteName (pic->Name);
	arc.WriteCount (pic->UseType);
}

//==========================================================================
//
// FTextureManager :: ReadTexture
//
//==========================================================================

int FTextureManager::ReadTexture (FArchive &arc)
{
	int usetype;
	const char *name;

	name = arc.ReadName ();
	if (name != NULL)
	{
		usetype = arc.ReadCount ();
		return GetTexture (name, usetype).GetIndex();
	}
	else return -1;
}

//==========================================================================
//
// operator<<
//
//==========================================================================

FArchive &operator<< (FArchive &arc, FTextureID &tex)
{
	if (arc.IsStoring())
	{
		TexMan.WriteTexture(arc, tex.texnum);
	}
	else
	{
		tex.texnum = TexMan.ReadTexture(arc);
	}
	return arc;
}

//==========================================================================
//
// FTextureID::operator+
// Does not return invalid texture IDs
//
//==========================================================================

FTextureID FTextureID::operator +(int offset) throw()
{
	if (!isValid()) return *this;
	if (texnum + offset >= TexMan.NumTextures()) return FTextureID(-1);
	return FTextureID(texnum + offset);
}
