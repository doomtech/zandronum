/*
** gl_nodes.cpp
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
** 5. Full disclosure of the entire project's source code, except for third
**    party libraries is mandatory. (NOTE: This clause is non-negotiable!)
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
#include <math.h>
#ifdef _MSC_VER
#include <malloc.h>		// for alloca()
#endif

#include "templates.h"
#include "m_alloc.h"
#include "m_argv.h"
#include "m_swap.h"
#include "g_game.h"
#include "i_system.h"
#include "w_wad.h"
#include "doomdef.h"
#include "p_local.h"
#include "nodebuild.h"
#include "doomstat.h"
#include "vectors.h"
#include "stats.h"
#include "doomerrors.h"
#include "p_setup.h"
#include "x86.h"
#include "version.h"

node_t * gamenodes;
int numgamenodes;
subsector_t * gamesubsectors;
int numgamesubsectors;
void P_GetPolySpots (MapData * lump, TArray<FNodeBuilder::FPolyStart> &spots, TArray<FNodeBuilder::FPolyStart> &anchors);

extern bool	UsingGLNodes;
extern bool gl_disabled;

// fixed 32 bit gl_vert format v2.0+ (glBsp 1.91)
typedef struct
{
  fixed_t x,y;
} mapglvertex_t;

typedef struct 
{
	SDWORD numsegs;
	SDWORD firstseg;    // Index of first one; segs are stored sequentially.
} gl3_mapsubsector_t;

typedef struct
{
	unsigned short	v1;		 // start vertex		(16 bit)
	unsigned short	v2;		 // end vertex			(16 bit)
	unsigned short	linedef; // linedef, or -1 for minisegs
	short			side;	 // side on linedef: 0 for right, 1 for left
	unsigned short	partner; // corresponding partner seg, or -1 on one-sided walls
} glseg_t;

typedef struct
{
	SDWORD			v1;
	SDWORD			v2;
	unsigned short	linedef;
	short			side;
	SDWORD			partner;
} glseg3_t;


typedef struct
{
	short 	x,y,dx,dy;	// partition line
	short 	bbox[2][4];	// bounding box for each child
	// If NF_SUBSECTOR is or'ed in, it's a subsector,
	// else it's a node of another subtree.
	unsigned int children[2];
} gl5_mapnode_t;

#define GL5_NF_SUBSECTOR (1 << 31)


//==========================================================================
//
// Collect all sidedefs which are not entirely covered by segs
// Old ZDBSPs could create such maps. If such a BSP is discovered
// a node rebuild must be done to ensure proper rendering
//
//==========================================================================

int gl_CheckForMissingSegs()
{
	float *added_seglen = new float[numsides];
	int missing = 0;

	memset(added_seglen, 0, sizeof(float)*numsides);
	for(int i=0;i<numsegs;i++)
	{
		seg_t * seg = &segs[i];

		if (seg->sidedef!=NULL)
		{
			// check all the segs and calculate the length they occupy on their sidedef
			TVector2<double> vec1(seg->v2->x - seg->v1->x, seg->v2->y - seg->v1->y);
			added_seglen[seg->sidedef - sides] += float(vec1.Length());
		}
	}

	for(int i=0;i<numsides;i++)
	{
		side_t * side =&sides[i];
		line_t * line = &lines[side->linenum];

		TVector2<double> lvec(line->dx, line->dy);
		float linelen = float(lvec.Length());

		missing += (added_seglen[i] < linelen - FRACUNIT);
	}

	delete [] added_seglen;
	return missing;
}

//==========================================================================
//
// Checks whether the nodes are suitable for GL rendering
//
//==========================================================================

bool gl_CheckForGLNodes()
{
	int i;

	for(i=0;i<numsubsectors;i++)
	{
		subsector_t * sub = &subsectors[i];
		seg_t * firstseg = &segs[sub->firstline];
		seg_t * lastseg = &segs[sub->firstline+sub->numlines-1];

		if (firstseg->v1 != lastseg->v2)
		{
			// This subsector is incomplete which means that these
			// are normal nodes
			return false;
		}
		else
		{
			for(DWORD j=0;j<sub->numlines;j++)
			{
				if (segs[j].linedef==NULL)	// miniseg
				{
					// We already have GL nodes. Great!
					return true;
				}
			}
		}
	}
	// all subsectors were closed but there are no minisegs
	// Although unlikely this can happen. Such nodes are not a problem.
	// all that is left is to check whether the BSP covers all sidedefs completely.
	int missing = gl_CheckForMissingSegs();
	if (missing > 0)
	{
		Printf("%d missing segs counted\nThe BSP needs to be rebuilt", missing);
	}
	return missing == 0;
}


//==========================================================================
//
// gl_LoadVertexes
//
// loads GL vertices
//
//==========================================================================

#define gNd2		MAKE_ID('g','N','d','2')
#define gNd4		MAKE_ID('g','N','d','4')
#define gNd5		MAKE_ID('g','N','d','5')

#define GL_VERT_OFFSET  4
static int firstglvertex;
static bool format5;

static bool gl_LoadVertexes(FileReader * f, wadlump_t * lump)
{
	BYTE *gldata;
	int                 i;

	firstglvertex = numvertexes;
	
	int gllen=lump->Size;

	gldata = new BYTE[gllen];
	f->Seek(lump->FilePos, SEEK_SET);
	f->Read(gldata, gllen);

	if (*(int *)gldata == gNd5) 
	{
		format5=true;
	}
	else if (*(int *)gldata != gNd2) 
	{
		// GLNodes V1 and V4 are unsupported.
		// V1 because the precision is insufficient and
		// V4 due to the missing partner segs
		Printf("GL nodes v%d found. This format is not supported by "GAMENAME"\n",
			(*(int *)gldata == gNd4)? 4:1);

		delete [] gldata;
		return false;
	}
	else format5=false;

	mapglvertex_t*	mgl;

	vertex_t * oldvertexes = vertexes;
	numvertexes += (gllen - GL_VERT_OFFSET)/sizeof(mapglvertex_t);
	vertexes	 = new vertex_t[numvertexes];
	mgl			 = (mapglvertex_t *) (gldata + GL_VERT_OFFSET);	

	memcpy(vertexes, oldvertexes, firstglvertex * sizeof(vertex_t));
	for(i=0;i<numlines;i++)
	{
		lines[i].v1 = vertexes + (lines[i].v1 - oldvertexes);
		lines[i].v2 = vertexes + (lines[i].v2 - oldvertexes);
	}

	for (i = firstglvertex; i < numvertexes; i++)
	{
		vertexes[i].x = LittleLong(mgl->x);
		vertexes[i].y = LittleLong(mgl->y);
		mgl++;
	}
	delete gldata;
	return true;
}

//==========================================================================
//
// GL Nodes utilities
//
//==========================================================================

static inline int checkGLVertex(int num)
{
	if (num & 0x8000)
		num = (num&0x7FFF)+firstglvertex;
	return num;
}

static inline int checkGLVertex3(int num)
{
	if (num & 0xc0000000)
		num = (num&0x3FFFFFFF)+firstglvertex;
	return num;
}

//==========================================================================
//
// gl_LoadGLSegs
//
//==========================================================================

bool gl_LoadGLSegs(FileReader * f, wadlump_t * lump)
{
	char		*data;
	int			i;
	line_t		*ldef=NULL;
	
	numsegs = lump->Size;
	data= new char[numsegs];
	f->Seek(lump->FilePos, SEEK_SET);
	f->Read(data, lump->Size);
	segs=NULL;

#ifdef _MSC_VER
	__try
#endif
	{
		if (!format5 && memcmp(data, "gNd3", 4))
		{
			numsegs/=sizeof(glseg_t);
			segs = new seg_t[numsegs];
			memset(segs,0,sizeof(seg_t)*numsegs);
			
			glseg_t * ml = (glseg_t*)data;
			for(i = 0; i < numsegs; i++)
			{							// check for gl-vertices
				segs[i].v1 = &vertexes[checkGLVertex(LittleShort(ml->v1))];
				segs[i].v2 = &vertexes[checkGLVertex(LittleShort(ml->v2))];
				
				segs[i].PartnerSeg=&segs[LittleShort(ml->partner)];
				if(ml->linedef != 0xffff)
				{
					ldef = &lines[LittleShort(ml->linedef)];
					segs[i].linedef = ldef;
	
					
					ml->side=LittleShort(ml->side);
					segs[i].sidedef = &sides[ldef->sidenum[ml->side]];
					segs[i].frontsector = sides[ldef->sidenum[ml->side]].sector;
					if (ldef->flags & ML_TWOSIDED && ldef->sidenum[ml->side^1]!=NO_SIDE)
						segs[i].backsector = sides[ldef->sidenum[ml->side^1]].sector;
					else
					{
						ldef->flags &= ~ML_TWOSIDED;
						segs[i].backsector = 0;
					}
	
				}
				else
				{
					segs[i].linedef = NULL;
					segs[i].sidedef = NULL;
	
					segs[i].frontsector = NULL;
					segs[i].backsector  = NULL;
				}
				ml++;		
			}
		}
		else
		{
			if (!format5) numsegs-=4;
			numsegs/=sizeof(glseg3_t);
			segs = new seg_t[numsegs];
			memset(segs,0,sizeof(seg_t)*numsegs);
			
			glseg3_t * ml = (glseg3_t*)(data+ (format5? 0:4));
			for(i = 0; i < numsegs; i++)
			{							// check for gl-vertices
				segs[i].v1 = &vertexes[checkGLVertex3(LittleLong(ml->v1))];
				segs[i].v2 = &vertexes[checkGLVertex3(LittleLong(ml->v2))];
				
				segs[i].PartnerSeg=&segs[LittleLong(ml->partner)];
	
				if(ml->linedef != 0xffff) // skip minisegs 
				{
					ldef = &lines[LittleLong(ml->linedef)];
					segs[i].linedef = ldef;
	
					
					ml->side=LittleShort(ml->side);
					segs[i].sidedef = &sides[ldef->sidenum[ml->side]];
					segs[i].frontsector = sides[ldef->sidenum[ml->side]].sector;
					if (ldef->flags & ML_TWOSIDED && ldef->sidenum[ml->side^1]!=NO_SIDE)
						segs[i].backsector = sides[ldef->sidenum[ml->side^1]].sector;
					else
					{
						ldef->flags &= ~ML_TWOSIDED;
						segs[i].backsector = 0;
					}
	
				}
				else
				{
					segs[i].linedef = NULL;
					segs[i].sidedef = NULL;
					segs[i].frontsector = NULL;
					segs[i].backsector  = NULL;
				}
				ml++;		
			}
		}
		delete [] data;
		return true;
	}
#ifdef _MSC_VER
	__except(1)
	{
		// Invalid data has the bas habit of requiring extensive checks here
		// so let's just catch anything invalid and output a message.
		// (at least under MSVC. GCC can't do SEH even for Windows... :( )
		Printf("Invalid GL segs. The BSP will have to be rebuilt.\n");
		delete [] data;
		delete [] segs;
		segs = NULL;
		return false;
	}
#endif
}


//==========================================================================
//
// gl_LoadGLSubsectors
//
//==========================================================================

bool gl_LoadGLSubsectors(FileReader * f, wadlump_t * lump)
{
	char * datab;
	int  i;
	
	numsubsectors = lump->Size;
	datab = new char[numsubsectors];
	f->Seek(lump->FilePos, SEEK_SET);
	f->Read(datab, lump->Size);
	
	if (numsubsectors == 0)
	{
		return false;
	}
	
	if (!format5 && memcmp(datab, "gNd3", 4))
	{
		mapsubsector_t * data = (mapsubsector_t*) datab;
		numsubsectors /= sizeof(mapsubsector_t);
		subsectors = new subsector_t[numsubsectors];
		memset(subsectors,0,numsubsectors * sizeof(subsector_t));
	
		for (i=0; i<numsubsectors; i++)
		{
			subsectors[i].numlines  = LittleShort(data[i].numsegs );
			subsectors[i].firstline = LittleShort(data[i].firstseg);

			if (subsectors[i].numlines == 0)
			{
				return false;
			}
		}
	}
	else
	{
		gl3_mapsubsector_t * data = (gl3_mapsubsector_t*) (datab+(format5? 0:4));
		numsubsectors /= sizeof(gl3_mapsubsector_t);
		subsectors = new subsector_t[numsubsectors];
		memset(subsectors,0,numsubsectors * sizeof(subsector_t));
	
		for (i=0; i<numsubsectors; i++)
		{
			subsectors[i].numlines  = LittleLong(data[i].numsegs );
			subsectors[i].firstline = LittleLong(data[i].firstseg);

			if (subsectors[i].numlines == 0)
			{
				return false;
			}
		}
	}

	for (i=0; i<numsubsectors; i++)
	{
		for(unsigned j=0;j<subsectors[i].numlines;j++)
		{
			seg_t * seg = &segs[subsectors[i].firstline+j];
			if (seg->linedef==NULL) seg->frontsector = seg->backsector = segs[subsectors[i].firstline].frontsector;
		}
	}

	delete datab;	
	return true;
}

//==========================================================================
//
// P_LoadNodes
//
//==========================================================================

static bool gl_LoadNodes (FileReader * f, wadlump_t * lump)
{
	int 		i;
	int 		j;
	int 		k;
	node_t* 	no;
	WORD*		used;

	if (!format5)
	{
		mapnode_t*	mn, * basemn;
		numnodes = lump->Size / sizeof(mapnode_t);

		if (numnodes == 0) return false;

		nodes = new node_t[numnodes];		
		f->Seek(lump->FilePos, SEEK_SET);

		basemn = mn = new mapnode_t[numnodes];
		f->Read(mn, lump->Size);

		used = (WORD *)alloca (sizeof(WORD)*numnodes);
		memset (used, 0, sizeof(WORD)*numnodes);

		no = nodes;

		for (i = 0; i < numnodes; i++, no++, mn++)
		{
			no->x = LittleShort(mn->x)<<FRACBITS;
			no->y = LittleShort(mn->y)<<FRACBITS;
			no->dx = LittleShort(mn->dx)<<FRACBITS;
			no->dy = LittleShort(mn->dy)<<FRACBITS;
			for (j = 0; j < 2; j++)
			{
				WORD child = LittleShort(mn->children[j]);
				if (child & NF_SUBSECTOR)
				{
					child &= ~NF_SUBSECTOR;
					if (child >= numsubsectors)
					{
						delete [] basemn;
						return false;
					}
					no->children[j] = (BYTE *)&subsectors[child] + 1;
				}
				else if (child >= numnodes)
				{
					delete [] basemn;
					return false;
				}
				else if (used[child])
				{
					delete [] basemn;
					return false;
				}
				else
				{
					no->children[j] = &nodes[child];
					used[child] = j + 1;
				}
				for (k = 0; k < 4; k++)
				{
					no->bbox[j][k] = LittleShort(mn->bbox[j][k])<<FRACBITS;
				}
			}
		}
		delete [] basemn;
	}
	else
	{
		gl5_mapnode_t*	mn, * basemn;
		numnodes = lump->Size / sizeof(gl5_mapnode_t);

		if (numnodes == 0) return false;

		nodes = new node_t[numnodes];		
		f->Seek(lump->FilePos, SEEK_SET);

		basemn = mn = new gl5_mapnode_t[numnodes];
		f->Read(mn, lump->Size);

		used = (WORD *)alloca (sizeof(WORD)*numnodes);
		memset (used, 0, sizeof(WORD)*numnodes);

		no = nodes;

		for (i = 0; i < numnodes; i++, no++, mn++)
		{
			no->x = LittleShort(mn->x)<<FRACBITS;
			no->y = LittleShort(mn->y)<<FRACBITS;
			no->dx = LittleShort(mn->dx)<<FRACBITS;
			no->dy = LittleShort(mn->dy)<<FRACBITS;
			for (j = 0; j < 2; j++)
			{
				SDWORD child = LittleLong(mn->children[j]);
				if (child & GL5_NF_SUBSECTOR)
				{
					child &= ~GL5_NF_SUBSECTOR;
					if (child >= numsubsectors)
					{
						delete [] basemn;
						return false;
					}
					no->children[j] = (BYTE *)&subsectors[child] + 1;
				}
				else if (child >= numnodes)
				{
					delete [] basemn;
					return false;
				}
				else if (used[child])
				{
					delete [] basemn;
					return false;
				}
				else
				{
					no->children[j] = &nodes[child];
					used[child] = j + 1;
				}
				for (k = 0; k < 4; k++)
				{
					no->bbox[j][k] = LittleShort(mn->bbox[j][k])<<FRACBITS;
				}
			}
		}
		delete [] basemn;
	}
	return true;
}

//==========================================================================
//
// loads the GL node data
//
//==========================================================================

bool gl_DoLoadGLNodes(FileReader * f, wadlump_t * lumps)
{
	if (!gl_LoadVertexes(f, &lumps[0]))
	{
		return false;
	}
	if (!gl_LoadGLSegs(f, &lumps[1]))
	{
		delete [] segs;
		segs = NULL;
		return false;
	}
	if (!gl_LoadGLSubsectors(f, &lumps[2]))
	{
		delete [] subsectors;
		subsectors = NULL;
		delete [] segs;
		segs = NULL;
		return false;
	}
	if (!gl_LoadNodes(f, &lumps[3]))
	{
		delete [] nodes;
		nodes = NULL;
		delete [] subsectors;
		subsectors = NULL;
		delete [] segs;
		segs = NULL;
		return false;
	}

	// Quick check for the validity of the nodes
	// For invalid nodes there is a high chance that this test will fail

	for (int i = 0; i < numsubsectors; i++)
	{
		seg_t * seg = &segs[subsectors[i].firstline];
		if (!seg->sidedef) 
		{
			Printf("GL nodes contain invalid data. The BSP has to be rebuilt.\n");
			delete [] nodes;
			nodes = NULL;
			delete [] subsectors;
			subsectors = NULL;
			delete [] segs;
			segs = NULL;
			return false;
		}
	}

	// check whether the BSP covers all sidedefs completely.
	int missing = gl_CheckForMissingSegs();
	if (missing > 0)
	{
		Printf("%d missing segs counted in GL nodes.\nThe BSP has to be rebuilt", missing);
	}
	return missing == 0;
}


//===========================================================================
//
// MatchHeader
//
// Checks whether a GL_LEVEL header belongs to this level
//
//===========================================================================

static bool MatchHeader(const char * label, const char * hdata)
{
	if (!memcmp(hdata, "LEVEL=", 6) == 0)
	{
		size_t labellen = strlen(label);

		if (strnicmp(hdata+6, label, labellen)==0 && 
			(hdata[6+labellen]==0xa || hdata[6+labellen]==0xd))
		{
			return true;
		}
	}
	return false;
}

//===========================================================================
//
// FindGLNodesInWAD
//
// Looks for GL nodes in the same WAD as the level itself
//
//===========================================================================

static int FindGLNodesInWAD(int labellump)
{
	int wadfile = Wads.GetLumpFile(labellump);
	FString glheader;

	glheader.Format("GL_%s", Wads.GetLumpFullName(labellump));
	if (glheader.Len()<=8)
	{
		int gllabel = Wads.CheckNumForName(glheader, ns_global, wadfile);
		if (gllabel >= 0) return gllabel;
	}
	else
	{
		// Before scanning the entire WAD directory let's check first whether
		// it is necessary.
		int gllabel = Wads.CheckNumForName("GL_LEVEL", ns_global, wadfile);

		if (gllabel >= 0)
		{
			int lastlump=0;
			int lump;
			while ((lump=Wads.FindLump("GL_LEVEL", &lastlump))>=0)
			{
				if (Wads.GetLumpFile(lump)==wadfile)
				{
					FMemLump mem = Wads.ReadLump(lump);
					if (MatchHeader(Wads.GetLumpFullName(labellump), (const char *)mem.GetMem())) return true;
				}
			}
		}
	}
	return -1;
}

//===========================================================================
//
// FindGLNodesInWAD
//
// Looks for GL nodes in the same WAD as the level itself
// When this function returns the file pointer points to
// the directory entry of the GL_VERTS lump
//
//===========================================================================

static int FindGLNodesInFile(FileReader * f, const char * label)
{
	FString glheader;
	bool mustcheck=false;
	DWORD id, dirofs, numentries;
	DWORD offset, size;
	char lumpname[9];

	glheader.Format("GL_%.8s", label);
	if (glheader.Len()>8)
	{
		glheader="GL_LEVEL";
		mustcheck=true;
	}

	f->Seek(0, SEEK_SET);
	(*f) >> id >> numentries >> dirofs;

	if ((id == IWAD_ID || id == PWAD_ID) && numentries > 4)
	{
		f->Seek(dirofs, SEEK_SET);
		for(DWORD i=0;i<numentries-4;i++)
		{
			(*f) >> offset >> size;
			f->Read(lumpname, 8);
			if (!strnicmp(lumpname, glheader, 8))
			{
				if (mustcheck)
				{
					char check[16]={0};
					int filepos = f->Tell();
					f->Seek(offset, SEEK_SET);
					f->Read(check, 16);
					f->Seek(filepos, SEEK_SET);
					if (MatchHeader(label, check)) return i;
				}
				else return i;
			}
		}
	}
	return -1;
}

//==========================================================================
//
// Checks for the presence of GL nodes in the loaded WADs or a .GWA file
//
//==========================================================================

bool gl_LoadGLNodes(MapData * map)
{
	wadlump_t gwalumps[4];
	char path[256];
	int li;
	int lumpfile = Wads.GetLumpFile(map->lumpnum);
	bool mapinwad = map->file == Wads.GetFileReader(lumpfile);
	FileReader * fr = map->file;
	FILE * f_gwa = NULL;

	const char * name = Wads.GetWadFullName(lumpfile);

	if (mapinwad)
	{
		li = FindGLNodesInWAD(map->lumpnum);

		if (li>=0)
		{
			// GL nodes are loaded with a WAD
			for(int i=0;i<4;i++)
			{
				gwalumps[i].FilePos=Wads.GetLumpOffset(li+i+1);
				gwalumps[i].Size=Wads.LumpLength(li+i+1);
			}
			return gl_DoLoadGLNodes(fr, gwalumps);
		}
		else
		{
			strcpy(path, name);

			char * ext = strrchr(path, '.');
			if (ext)
			{
				strcpy(ext, ".gwa");
				// Todo: Compare file dates

				f_gwa = fopen(path, "rb");
				if (f_gwa==NULL) return false;

				fr = new FileReader(f_gwa);

				strncpy(map->MapLumps[0].Name, Wads.GetLumpFullName(map->lumpnum), 8);
			}
		}
	}

	bool result = false;
	li = FindGLNodesInFile(fr, map->MapLumps[0].Name);
	if (li!=-1)
	{
		static const char check[][9]={"GL_VERT","GL_SEGS","GL_SSECT","GL_NODES"};
		result=true;
		for(unsigned i=0; i<4;i++)
		{
			(*fr) >> gwalumps[i].FilePos;
			(*fr) >> gwalumps[i].Size;
			fr->Read(gwalumps[i].Name, 8);
			if (strnicmp(gwalumps[i].Name, check[i], 8))
			{
				result=false;
				break;
			}
		}
		if (result) result = gl_DoLoadGLNodes(fr, gwalumps);
	}

	if (f_gwa)
	{
		delete fr;
		fclose(f_gwa);
	}
	return result;
}

//==========================================================================
//
// Checks whether nodes are GL friendly or not
//
//==========================================================================

void gl_CheckNodes(MapData * map)
{
	// Save the old nodes so that R_PointInSubsector can use them
	// Unfortunately there are some screwed up WADs which can not
	// be reliably processed by the internal node builder
	// It is not necessary to keep the segs (and vertices) because they aren't used there.
	if (nodes && subsectors)
	{
		gamenodes = nodes;
		numgamenodes = numnodes;
		gamesubsectors = subsectors;
		numgamesubsectors = numsubsectors;
		if (gl_disabled) return;
	}
	else
	{
		gamenodes=NULL;
	}

	if (!gl_CheckForGLNodes())
	{
		for (int i = 0; i < numsubsectors; i++)
		{
			gamesubsectors[i].sector = segs[gamesubsectors[i].firstline].sidedef->sector;
		}

		nodes = NULL;
		numnodes = 0;
		subsectors = NULL;
		numsubsectors = 0;
		if (segs) delete [] segs;
		segs = NULL;
		numsegs = 0;

		// Try to load regular GL nodes
		if (!gl_LoadGLNodes(map))
		{
			// none found - we have to build new ones!
			unsigned int startTime, endTime;

			startTime = I_MSTime ();
			TArray<FNodeBuilder::FPolyStart> polyspots, anchors;
			P_GetPolySpots (map, polyspots, anchors);
			FNodeBuilder::FLevel leveldata =
			{
				vertexes, numvertexes,
				sides, numsides,
				lines, numlines
			};
			leveldata.FindMapBounds ();
			FNodeBuilder builder (leveldata, polyspots, anchors, true, CPU.bSSE2);
			UsingGLNodes = true;
			delete[] vertexes;
			builder.Extract (nodes, numnodes,
				segs, numsegs,
				subsectors, numsubsectors,
				vertexes, numvertexes);
			endTime = I_MSTime ();
			DPrintf ("BSP generation took %.3f sec (%d segs)\n", (endTime - startTime) * 0.001, numsegs);
		}
	}
	if (!gamenodes)
	{
		gamenodes = nodes;
		numgamenodes = numnodes;
		gamesubsectors = subsectors;
		numgamesubsectors = numsubsectors;
	}
}
