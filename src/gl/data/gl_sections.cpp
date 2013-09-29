/*
** gl_sections.cpp
** Splits sectors into continuous separate parts
**
**---------------------------------------------------------------------------
** Copyright 2008 Christoph Oelckers
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

#include "i_system.h"
#include "p_local.h"
#include "c_dispatch.h"
#include "gl/data/gl_sections.h"

TArray<FGLSectionLine> SectionLines;
TArray<FGLSectionLoop> SectionLoops;
TArray<FGLSection> Sections;
TArray<int> SectionForSubsector;

CVAR (Bool, dumpsections, false, 0)

#define ISDONE(no, p) (p[(no)>>3] & (1 << ((no)&7)))
#define SETDONE(no, p) p[(no)>>3] |= (1 << ((no)&7))

inline vertex_t *V1(side_t *s)
{
	line_t *ln = s->linedef;
	return s == ln->sidedef[0]? ln->v1: ln->v2;
}

inline vertex_t *V2(side_t *s)
{
	line_t *ln = s->linedef;
	return s == ln->sidedef[0]? ln->v2: ln->v1;
}

class FSectionCreator
{
	BYTE *processed_segs;
	BYTE *processed_subsectors;
	int *section_for_segs;

	vertex_t *v1_l1, *v2_l1;

	FGLSectionLoop *loop;
	FGLSection *section;	// current working section

public:
	//==========================================================================
	//
	//
	//
	//==========================================================================

	FSectionCreator()
	{
		processed_segs = new BYTE[(numsegs+7)/8];
		processed_subsectors = new BYTE[(numsubsectors+7)/8];

		memset(processed_segs, 0, (numsegs+7)/8); 
		memset(processed_subsectors, 0, (numsubsectors+7)/8); 

		section_for_segs = new int[numsegs];
		memset(section_for_segs, -1, numsegs * sizeof(int));
	}

	//==========================================================================
	//
	//
	//
	//==========================================================================

	~FSectionCreator()
	{
		delete [] processed_segs;
		delete [] processed_subsectors;
		delete [] section_for_segs;
	}

	//==========================================================================
	//
	//
	//
	//==========================================================================

	void NewLoop()
	{
		section->numloops++;
		loop = &SectionLoops[SectionLoops.Reserve(1)];
		loop->startline = SectionLines.Size();
		loop->numlines = 0 ;
	}

	void NewSection(sector_t *sec)
	{
		section = &Sections[Sections.Reserve(1)];
		section->sector = sec;
		section->subsectors.Clear();
		section->numloops = 0;
		section->startloop = SectionLoops.Size();
		section->validcount = -1;
		NewLoop();
	}

	void FinalizeSection()
	{
	}

	//==========================================================================
	//
	//
	//
	//==========================================================================

	bool AddSeg(seg_t *seg)
	{
		FGLSectionLine &line = SectionLines[SectionLines.Reserve(1)];


		bool firstline = loop->numlines == 0;

		if (ISDONE(seg-segs, processed_segs))
		{
			// should never happen!
			DPrintf("Tried to add seg %d to Sections twice. Cannot create Sections.\n", seg-segs);
			return false;
		}

		SETDONE(seg-segs, processed_segs);
		section_for_segs[seg-segs] = Sections.Size()-1;

		line.start = seg->v1;
		line.end = seg->v2;
		line.sidedef = seg->sidedef;
		line.linedef = seg->linedef;
		line.refseg = seg;
		line.polysub = NULL;
		line.otherside = -1;

		if (loop->numlines == 0)
		{
			v1_l1 = seg->v1;
			v2_l1 = seg->v2;
		}
		loop->numlines++;
		return true;
	}

	//==========================================================================
	//
	// returns the seg whose partner seg determines where this
	// section continues
	//
	//==========================================================================
	bool AddSubSector(subsector_t *subsec, vertex_t *startpt, seg_t **pNextSeg)
	{
		unsigned i = 0;
		if (startpt != NULL)
		{
			// find the seg in this subsector that starts at the given vertex
			for(i = 0; i < subsec->numlines; i++)
			{
				if (segs[subsec->firstline + i].v1 == startpt) break;
			}
			if (i == subsec->numlines) 
			{
				DPrintf("Vertex not found in subsector %d. Cannot create Sections.\n", subsec-subsectors);
				return false;	// Nodes are bad
			}
		}
		else 
		{
			// Find the first unprocessed non-miniseg
			for(i = 0; i < subsec->numlines; i++)
			{
				seg_t *seg = &segs[subsec->firstline + i];

				if (seg->sidedef == NULL) continue;
				if (ISDONE(seg-segs, processed_segs)) continue;
				break;
			}
			if (i == subsec->numlines)
			{
				DPrintf("Unable to find a start seg. Cannot create Sections.\n");
				return false;	// Nodes are bad
			}
	
			startpt = segs[subsec->firstline+i].v1;
		}

		seg_t *thisseg = &segs[subsec->firstline + i];
		if (thisseg->sidedef == NULL && thisseg->PartnerSeg != NULL)	// miniseg
		{
			if (thisseg->Subsector->render_sector == thisseg->PartnerSeg->Subsector->render_sector)
			{
				SETDONE(thisseg-segs, processed_segs);
				// continue with the loop in the adjoining subsector
				*pNextSeg = thisseg;
				return true;
			}
		}


		while(1)
		{
			if (loop->numlines > 0 && thisseg->v1 == v1_l1 && thisseg->v2 == v2_l1)
			{
				// This loop is complete
				*pNextSeg = NULL;
				return true;
			}

			if (!AddSeg(thisseg)) return NULL;

			i = (i+1) % subsec->numlines;
			seg_t *nextseg = &segs[subsec->firstline + i];

			if (thisseg->v2 != nextseg->v1)
			{
				DPrintf("Segs in subsector %d are not continuous. Cannot create Sections.\n", subsec-subsectors);
				return false;	// Nodes are bad
			}

			if (nextseg->sidedef == NULL && nextseg->PartnerSeg != NULL)	// miniseg
			{
				if (nextseg->Subsector->render_sector == nextseg->PartnerSeg->Subsector->render_sector)
				{
					SETDONE(nextseg-segs, processed_segs);
					// continue with the loop in the adjoining subsector
					*pNextSeg = nextseg;
					return true;
				}
			}
			thisseg = nextseg;
		}
	}

	//=============================================================================
	//
	//
	//
	//=============================================================================

	bool FindNextSeg(seg_t **pSeg)
	{
		// find an unprocessed non-miniseg or a miniseg with an unprocessed 
		// partner subsector that belongs to the same rendersector
		for (unsigned i = 0; i < section->subsectors.Size(); i++)
		{
			for(unsigned j = 0; j < section->subsectors[i]->numlines; j++)
			{
				seg_t *seg = &segs[section->subsectors[i]->firstline+j];

				if (seg->sidedef != NULL && !ISDONE(seg-segs, processed_segs))
				{
					*pSeg = seg;
					return true;
				}
				else if (seg->sidedef == NULL && seg->PartnerSeg != NULL && 
					!ISDONE(seg->PartnerSeg->Subsector-subsectors, processed_subsectors))
				{
					if (seg->Subsector->render_sector == seg->PartnerSeg->Subsector->render_sector)
					{
						*pSeg = seg->PartnerSeg;
						return true;
					}
				}
			}
		}
		*pSeg = NULL;
		return true;
	}

	//=============================================================================
	//
	// all segs and subsectors must be grouped into Sections
	//
	//=============================================================================
	bool CheckSections()
	{
		for (int i = 0; i < numsegs; i++)
		{
			if (segs[i].sidedef != NULL && !ISDONE(i, processed_segs))
			{
				Printf("Seg %d (Linedef %d) not processed during section creation\n", i, segs[i].linedef-lines);
				return false;
			}
		}
		for (int i = 0; i < numsubsectors; i++)
		{
			if (!ISDONE(i, processed_subsectors))
			{
				Printf("Subsector %d (Sector %d) not processed during section creation\n", i, subsectors[i].sector-sectors);
				return false;
			}
		}
		return true;
	}

	//=============================================================================
	//
	//
	//
	//=============================================================================
	void DeleteLine(int i)
	{
		SectionLines.Delete(i);
		for(int i = SectionLoops.Size() - 1; i >= 0; i--)
		{
			FGLSectionLoop *loop = &SectionLoops[i];
			if (loop->startline > i) loop->startline--;
		}
	}

	//=============================================================================
	//
	//
	//
	//=============================================================================
	void MergeLines(FGLSectionLoop *loop)
	{
		int i;
		int deleted = 0;
		FGLSectionLine *ln1;
		FGLSectionLine *ln2;
		// Merge identical lines in the list
		for(i = loop->numlines - 1; i > 0; i--)
		{
			ln1 = loop->GetLine(i);
			ln2 = loop->GetLine(i-1);

			if (ln1->sidedef == ln2->sidedef && ln1->otherside == ln2->otherside)
			{
				// identical references. These 2 lines can be merged.
				ln2->end = ln1->end;
				SectionLines.Delete(loop->startline + i);
				loop->numlines--;
				deleted++;
			}
		}

		// If we started in the middle of a sidedef the first and last lines
		// may reference the same sidedef. check that, too.

		int loopstart = 0;

		ln1 = loop->GetLine(0);
		for(i = loop->numlines - 1; i > 0; i--)
		{
			ln2 = loop->GetLine(i);
			if (ln1->sidedef != ln2->sidedef || ln1->otherside != ln2->otherside)
				break;
		}
		if (i < loop->numlines-1)
		{
			i++;
			ln2 = loop->GetLine(i);
			ln1->start = ln2->start;
			SectionLines.Delete(loop->startline + i, loop->numlines - i);
			deleted += loop->numlines - i;
			loop->numlines = i;
		}

		// Adjust all following loops
		for(unsigned ii = unsigned(loop - &SectionLoops[0]) + 1; ii < SectionLoops.Size(); ii++)
		{
			SectionLoops[ii].startline -= deleted;
		}
	}

	//=============================================================================
	//
	//
	//
	//=============================================================================
	void SetReferences()
	{
		for(unsigned i = 0; i < SectionLines.Size(); i++)
		{
			FGLSectionLine *ln = &SectionLines[i];
			seg_t *seg = ln->refseg;

			if (seg != NULL)
			{
				seg_t *partner = seg->PartnerSeg;

				if (seg->PartnerSeg == NULL)
				{
					ln->otherside = -1;
				}
				else
				{
					ln->otherside = section_for_segs[partner-segs];
				}
			}
			else
			{
				ln->otherside = -1;
			}
		}

		for(unsigned i = 0; i < SectionLoops.Size(); i++)
		{
			MergeLines(&SectionLoops[i]);
		}
	}

	//=============================================================================
	//
	//
	//
	//=============================================================================
	bool CreateSections()
	{
		int pick = 0;

		while (pick < numsubsectors)
		{
			if (ISDONE(pick, processed_subsectors)) 
			{
				pick++;
				continue;
			}


			subsector_t *subsector = &subsectors[pick];

			seg_t *workseg = NULL;
			vertex_t *startpt = NULL;

			NewSection(subsector->render_sector);
			while (1)
			{
				if (!ISDONE(subsector-subsectors, processed_subsectors))
				{
					SETDONE(subsector-subsectors, processed_subsectors);
					section->subsectors.Push(subsector);
					SectionForSubsector[subsector - subsectors] = int(section - &Sections[0]);
				}

				bool result = AddSubSector(subsector, startpt, &workseg);

				if (!result)
				{
					return false;	// couldn't create Sections
				}
				else if (workseg != NULL)
				{
					// crossing into another subsector
					seg_t *partner = workseg->PartnerSeg;
					if (workseg->v2 != partner->v1)
					{
						DPrintf("Inconsistent subsector references in seg %d. Cannot create Sections.\n", workseg-segs);
						return false;
					}
					subsector = partner->Subsector;
					startpt = workseg->v1;
				}
				else
				{
					// loop complete. Check adjoining subsectors for other loops to
					// be added to this section
					if (!FindNextSeg(&workseg))
					{
						return false;
					}
					else if (workseg == NULL)
					{
						// No more subsectors found. This section is complete!
						FinalizeSection();
						break;
					}
					else
					{
						subsector = workseg->Subsector;
						// If this is a regular seg, start there, otherwise start
						// at the subsector's first seg
						startpt = workseg->sidedef == NULL? NULL : workseg->v1;

						NewLoop();
					}
				}
			}
		}

		if (!CheckSections()) return false;
		SetReferences();

		Sections.ShrinkToFit();
		SectionLoops.ShrinkToFit();
		SectionLines.ShrinkToFit();

		return true;
	}

};

//=============================================================================
//
//
//
//=============================================================================

void DumpSection(int no, FGLSection *sect)
{
	Printf(PRINT_LOG, "Section %d, sector %d\n{\n", no, sect->sector->sectornum);

	for(int i = 0; i < sect->numloops; i++)
	{
		Printf(PRINT_LOG, "\tLoop %d\n\t{\n", i);

		FGLSectionLoop *loop = sect->GetLoop(i);

		for(int i = 0; i < loop->numlines; i++)
		{
			FGLSectionLine *ln = loop->GetLine(i);
			if (ln->sidedef != NULL)
			{
				vertex_t *v1 = V1(ln->sidedef);
				vertex_t *v2 = V2(ln->sidedef);
				double dx = FIXED2FLOAT(v2->x-v1->x);
				double dy = FIXED2FLOAT(v2->y-v1->y);
				double dx1 = FIXED2FLOAT(ln->start->x-v1->x);
				double dy1 = FIXED2FLOAT(ln->start->y-v1->y);
				double dx2 = FIXED2FLOAT(ln->end->x-v1->x);
				double dy2 = FIXED2FLOAT(ln->end->y-v1->y);
				double d = sqrt(dx*dx+dy*dy);
				double d1 = sqrt(dx1*dx1+dy1*dy1);
				double d2 = sqrt(dx2*dx2+dy2*dy2);

				Printf(PRINT_LOG, "\t\tLinedef %d, %s: Start (%1.2f, %1.2f), End (%1.2f, %1.2f)", 
					ln->linedef - lines, ln->sidedef == ln->linedef->sidedef[0]? "front":"back",
					ln->start->x/65536.f, ln->start->y/65536.f,
					ln->end->x/65536.f, ln->end->y/65536.f);

				if (ln->otherside != -1)
				{
					Printf (PRINT_LOG, ", other side = %d", ln->otherside);
				}
				if (d1 > 0.005 || d2 < 0.995)
				{
					Printf(PRINT_LOG, ", Range = %1.3f, %1.3f", d1/d, d2/d);
				}
			}
			else
			{
				Printf(PRINT_LOG, "\t\tMiniseg: Start (%1.3f, %1.3f), End (%1.3f, %1.3f)\n", 
					ln->start->x/65536.f, ln->start->y/65536.f, ln->end->x/65536.f, ln->end->y/65536.f);

				if (ln->otherside != -1)
				{
					Printf (PRINT_LOG, ", other side = %d", ln->otherside);
				}
			}
			Printf(PRINT_LOG, "\n");
		}
		Printf(PRINT_LOG, "\t}\n");
	}
	Printf(PRINT_LOG, "}\n\n");
}

//=============================================================================
//
//
//
//=============================================================================

void DumpSections()
{
	for(unsigned i = 0; i < Sections.Size(); i++)
	{
		DumpSection(i, &Sections[i]);
	}
}

//=============================================================================
//
//
//
//=============================================================================

void gl_CreateSections()
{
	SectionLines.Clear();
	SectionLoops.Clear();
	Sections.Clear();
	SectionForSubsector.Resize(numsubsectors);
	memset(&SectionForSubsector[0], -1, numsubsectors * sizeof(SectionForSubsector[0]));
	FSectionCreator creat;
	creat.CreateSections();
	if (dumpsections) DumpSections();
}


