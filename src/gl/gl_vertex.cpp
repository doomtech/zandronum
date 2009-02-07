/*
** gl_vertex.cpp
**
**---------------------------------------------------------------------------
** Copyright 2006 Christoph Oelckers
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



#include "gl/gl_include.h"
#include "gl/gl_struct.h"
#include "gl/gl_renderstruct.h"
#include "gl/gl_portal.h"
#include "gl/gl_lights.h"
#include "gl/gl_glow.h"
#include "gl/gl_data.h"
#include "gl/gl_texture.h"
#include "gl/gl_basic.h"
#include "gl/gl_functions.h"

EXTERN_CVAR(Bool, gl_seamless)

struct FVertexsplitInfo
{
	int validcount;
	int numheights;
	int numsectors;
	sector_t ** sectors;
	float * heightlist;
};

extern long gl_frameMS;
static FVertexsplitInfo * gl_vertexsplit;


//==========================================================================
//
// Split left edge of wall
//
//==========================================================================

void gl_SplitLeftEdge(GLWall * wall, texcoord * tcs, bool glow)
{
	if (wall->vertexes[0]==NULL) return;

	FVertexsplitInfo * vi=&gl_vertexsplit[wall->vertexes[0]-vertexes];

	if (vi->numheights)
	{
		int i=0;

		float polyh1=wall->ztop[0] - wall->zbottom[0];
		float factv1=polyh1? (tcs[1].v - tcs[0].v) / polyh1:0;
		float factu1=polyh1? (tcs[1].u - tcs[0].u) / polyh1:0;

		while (i<vi->numheights && vi->heightlist[i] <= wall->zbottom[0] ) i++;
		while (i<vi->numheights && vi->heightlist[i] < wall->ztop[0])
		{
			if (glow) gl_SetGlowPosition(wall->zceil[0] - vi->heightlist[i], vi->heightlist[i] - wall->zfloor[0]);
			gl.TexCoord2f(factu1*(vi->heightlist[i] - wall->ztop[0]) + tcs[1].u,
						 factv1*(vi->heightlist[i] - wall->ztop[0]) + tcs[1].v);
			gl.Vertex3f(wall->glseg.x1, vi->heightlist[i], wall->glseg.y1);
			i++;
		}
	}
}

//==========================================================================
//
// Split right edge of wall
//
//==========================================================================

void gl_SplitRightEdge(GLWall * wall, texcoord * tcs, bool glow)
{
	if (wall->vertexes[1]==NULL) return;

	FVertexsplitInfo * vi=&gl_vertexsplit[wall->vertexes[1]-vertexes];

	if (vi->numheights)
	{
		int i=vi->numheights-1;

		float polyh2 = wall->ztop[1] - wall->zbottom[1];
		float factv2 = polyh2? (tcs[2].v - tcs[3].v) / polyh2:0;
		float factu2 = polyh2? (tcs[2].u - tcs[3].u) / polyh2:0;

		while (i>0 && vi->heightlist[i] >= wall->ztop[1]) i--;
		while (i>0 && vi->heightlist[i] > wall->zbottom[1])
		{
			if (glow) gl_SetGlowPosition(wall->zceil[1] - vi->heightlist[i], vi->heightlist[i] - wall->zfloor[1]);
			gl.TexCoord2f(factu2 * (vi->heightlist[i] - wall->ztop[1]) + tcs[2].u,
						 factv2 * (vi->heightlist[i] - wall->ztop[1]) + tcs[2].v);
			gl.Vertex3f(wall->glseg.x2, vi->heightlist[i], wall->glseg.y2);
			i--;
		}
	}
}


//==========================================================================
//
// Recalculate all heights affectting this vertex.
//
//==========================================================================
void gl_RecalcVertexHeights(vertex_t * v)
{
	if (gl_vertexsplit==NULL) gl_InitVertexData();

	FVertexsplitInfo * vi = &gl_vertexsplit[v - vertexes];

	int i,j,k;
	float height;

	if (vi->validcount==gl_frameMS) return;

	vi->numheights=0;
	for(i=0;i<vi->numsectors;i++)
	{
		for(j=0;j<2;j++)
		{
			if (j==0) height=TO_GL(vi->sectors[i]->ceilingplane.ZatPoint(v));
			else height=TO_GL(vi->sectors[i]->floorplane.ZatPoint(v));

			for(k=0;k<vi->numheights;k++)
			{
				if (height==vi->heightlist[k]) break;
				if (height<vi->heightlist[k])
				{
					memmove(&vi->heightlist[k+1],&vi->heightlist[k],sizeof(float)*(vi->numheights-k));
					vi->heightlist[k]=height;
					vi->numheights++;
					break;
				}
			}
			if (k==vi->numheights) vi->heightlist[vi->numheights++]=height;
		}
	}
	if (vi->numheights<=2) vi->numheights=0;	// is not in need of any special attention
	vi->validcount=gl_frameMS;
}


//==========================================================================
//
// 
//
//==========================================================================
static void AddToVertex(const sector_t * sec, TArray<int> & list)
{
	int secno=sec-sectors;

	for(unsigned i=0;i<list.Size();i++)
	{
		if (list[i]==secno) return;
	}
	list.Push(secno);
}

//==========================================================================
//
// 
//
//==========================================================================
void gl_InitVertexData()
{
	if (!gl_seamless || gl_vertexsplit) return;

	TArray<int> * vt_sectorlists;

	int i,j,k;
	unsigned int l;

	vt_sectorlists = new TArray<int>[numvertexes];


	for(i=0;i<numlines;i++)
	{
		line_t * line = &lines[i];

		for(j=0;j<2;j++)
		{
			vertex_t * v = j==0? line->v1 : line->v2;

			for(k=0;k<2;k++)
			{
				sector_t * sec = k==0? line->frontsector : line->backsector;

				if (sec)
				{
					extsector_t::xfloor &x = sec->e->XFloor;

					AddToVertex(sec, vt_sectorlists[v-vertexes]);
					if (sec->heightsec) AddToVertex(sec->heightsec, vt_sectorlists[v-vertexes]);

					for(l=0;l<x.ffloors.Size();l++)
					{
						F3DFloor * rover = x.ffloors[l];
						if(!(rover->flags & FF_EXISTS)) continue;
						if (rover->flags&FF_NOSHADE) continue; // FF_NOSHADE doesn't create any wall splits 

						AddToVertex(rover->model, vt_sectorlists[v-vertexes]);
					}
				}
			}
		}
	}

	gl_vertexsplit = new FVertexsplitInfo[numvertexes];

	for(i=0;i<numvertexes;i++)
	{
		int cnt = vt_sectorlists[i].Size();

		gl_vertexsplit[i].validcount=-1;
		gl_vertexsplit[i].numheights=0;
		if (cnt>1)
		{
			gl_vertexsplit[i].numsectors= cnt;
			gl_vertexsplit[i].sectors=new sector_t*[cnt];
			gl_vertexsplit[i].heightlist = new float[cnt*2];
			for(int j=0;j<cnt;j++)
			{
				gl_vertexsplit[i].sectors[j] = &sectors[vt_sectorlists[i][j]];
			}
		}
		else
		{
			gl_vertexsplit[i].numsectors=0;
		}
	}

	delete [] vt_sectorlists;
	atterm(gl_CleanVertexData);
}


//==========================================================================
//
// 
//
//==========================================================================
void gl_CleanVertexData()
{
	if (gl_vertexsplit)
	{
		for(int i=0;i<numvertexes;i++) if (gl_vertexsplit[i].numsectors>0)
		{
			delete [] gl_vertexsplit[i].sectors;
			delete [] gl_vertexsplit[i].heightlist;
		}
		delete [] gl_vertexsplit;
		gl_vertexsplit=NULL;
	}
}


