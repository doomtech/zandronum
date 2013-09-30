/*
** gl_setup.cpp
** Initializes the data structures required by the GL renderer to handle
** a level
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

#include "doomtype.h"
#include "colormatcher.h"
#include "r_translate.h"
#include "i_system.h"
#include "p_local.h"
#include "p_lnspec.h"
#include "c_dispatch.h"
#include "r_sky.h"
#include "sc_man.h"
#include "w_wad.h"
#include "gi.h"
#include "g_level.h"
#include "a_sharedglobal.h"

#include "gl/renderer/gl_renderer.h"
#include "gl/data/gl_data.h"
#include "gl/data/gl_vertexbuffer.h"
#include "gl/scene/gl_clipper.h"
#include "gl/dynlights/gl_dynlight.h"
#include "gl/dynlights/gl_glow.h"
#include "gl/utility/gl_clock.h"
#include "gl/gl_functions.h"

TArray<FPortal> portals;

//==========================================================================
//
//
//
//==========================================================================

int FPortal::PointOnShapeLineSide(fixed_t x, fixed_t y, int shapeindex)
{
	int shapeindex2 = (shapeindex+1)%Shape.Size();

	return DMulScale32(y - Shape[shapeindex]->y, Shape[shapeindex2]->x - Shape[shapeindex]->x,
		Shape[shapeindex]->x - x, Shape[shapeindex2]->y - Shape[shapeindex]->y);
}

//==========================================================================
//
//
//
//==========================================================================

void FPortal::AddVertexToShape(vertex_t *vertex)
{
	for(unsigned i=0;i<Shape.Size(); i++)
	{
		if (vertex->x == Shape[i]->x && vertex->y == Shape[i]->y) return;
	}

	if (Shape.Size() < 2) 
	{
		Shape.Push(vertex);
	}
	else if (Shape.Size() == 2)
	{
		// Special case: We need to check if the vertex is on an extension of the line between the first two vertices.
		int pos = PointOnShapeLineSide(vertex->x, vertex->y, 0);

		if (pos == 0)
		{
			fixed_t distv1 = P_AproxDistance(vertex->x - Shape[0]->x, vertex->y - Shape[0]->y);
			fixed_t distv2 = P_AproxDistance(vertex->x - Shape[1]->x, vertex->y - Shape[1]->y);
			fixed_t distvv = P_AproxDistance(Shape[0]->x - Shape[1]->x, Shape[0]->y - Shape[1]->y);

			if (distv1 > distvv)
			{
				Shape[1] = vertex;
			}
			else if (distv2 > distvv)
			{
				Shape[0] = vertex;
			}
			return;
		}
		else if (pos > 0)
		{
			Shape.Insert(1, vertex);
		}
		else
		{
			Shape.Push(vertex);
		}
	}
	else
	{
		for(unsigned i=0; i<Shape.Size(); i++)
		{
			int startlinepos = PointOnShapeLineSide(vertex->x, vertex->y, i);
			if (startlinepos >= 0)
			{
				int previouslinepos = PointOnShapeLineSide(vertex->x, vertex->y, (i+Shape.Size()-1)%Shape.Size());

				if (previouslinepos < 0)	// we found the first line for which the vertex lies in front
				{
					unsigned int nextpoint = i;

					do
					{
						nextpoint = (nextpoint+1) % Shape.Size();
					}
					while (PointOnShapeLineSide(vertex->x, vertex->y, nextpoint) >= 0);

					int removecount = (nextpoint - i + Shape.Size()) % Shape.Size() - 1;

					if (removecount == 0)
					{
						if (startlinepos > 0) 
						{
							Shape.Insert(i+1, vertex);
						}
					}
					else if (nextpoint > i || nextpoint == 0)
					{
						// The easy case: It doesn't wrap around
						Shape.Delete(i+1, removecount-1);
						Shape[i+1] = vertex;
					}
					else
					{
						// It does wrap around.
						Shape.Delete(i+1, removecount);
						Shape.Delete(1, nextpoint-1);
						Shape[0] = vertex;
					}
					return;
				}
			}
		}
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FPortal::AddSectorToPortal(sector_t *sector)
{
	for(int i=0; i<sector->linecount; i++)
	{
		AddVertexToShape(sector->lines[i]->v1);
		// This is necessary to handle unclosed sectors
		AddVertexToShape(sector->lines[i]->v2);
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FPortal::UpdateClipAngles()
{
	for(unsigned int i=0; i<Shape.Size(); i++)
	{
		ClipAngles[i] = R_PointToPseudoAngle(viewx, viewy, Shape[i]->x, Shape[i]->y);
	}
}

//==========================================================================
//
// portal initialization
//
//==========================================================================

void gl_InitPortals()
{
	TThinkerIterator<AStackPoint> it;
	AStackPoint *pt;

	while ((pt = it.Next()))
	{
		FPortal *portal = NULL;
		int plane;
		for(int i=0;i<numsectors;i++)
		{
			if (sectors[i].linecount == 0)
			{
				continue;
			}
			else if (sectors[i].FloorSkyBox == pt)
			{
				plane = 1;
			}
			else if (sectors[i].CeilingSkyBox == pt)
			{
				plane = 2;
			}
			else continue;

			// we only process portals that actually are in use.
			if (portal == NULL) 
			{
				pt->special1 = portals.Size();	// Link portal thing to render data
				portal = &portals[portals.Reserve(1)];
				portal->origin = pt;
				portal->plane = 0;
				portal->xDisplacement = pt->x - pt->Mate->x;
				portal->yDisplacement = pt->y - pt->Mate->y;
			}
			portal->AddSectorToPortal(&sectors[i]);
			portal->plane|=plane;
		}
		if (portal != NULL)
		{
			// if the first vertex is duplicated at the end it'll save time in a time critical function
			// because that code does not need to check for wraparounds anymire.
			portal->Shape.Resize(portal->Shape.Size()+1);
			portal->Shape[portal->Shape.Size()-1] = portal->Shape[0];
			portal->Shape.ShrinkToFit();
			portal->ClipAngles.Resize(portal->Shape.Size());
		}
	}
}


CCMD(dumpportals)
{
	for(unsigned i=0;i<portals.Size(); i++)
	{
		Printf("Portal #%d, plane %d, stackpoint at (%f,%f), displacement = (%f,%f)\n", i, portals[i].plane, portals[i].origin->x/65536., portals[i].origin->y/65536.,
			portals[i].origin->x/65536. - portals[i].origin->Mate->x/65536., portals[i].origin->y/65536. - portals[i].origin->Mate->y/65536.);
		for (unsigned j=0;j<portals[i].Shape.Size(); j++)
		{
			Printf("\t(%f,%f)\n", portals[i].Shape[j]->x/65536., portals[i].Shape[j]->y/65536.);
		}
	}
}
