//
//-----------------------------------------------------------------------------
//
// Copyright (C) 2009 Christoph Oelckers
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// As an exception to the GPL this code may be used in GZDoom
// derivatives under the following conditions:
//
// 1. The license of these files is not changed
// 2. Full source of the derived program is disclosed
//
//
// ----------------------------------------------------------------------------
//
// Geometry data mainenance
//

#include "gl/gl_include.h"
#include "r_main.h"
#include "r_defs.h"
#include "r_state.h"
#include "v_palette.h"
#include "a_sharedglobal.h"
#include "gl/common/glc_geometric.h"
#include "gl/common/glc_convert.h"
#include "gl/new_renderer/gl2_renderer.h"
#include "gl/new_renderer/gl2_geom.h"
#include "gl/new_renderer/textures/gl2_material.h"

int GetFloorLight (const sector_t *sec);
int GetCeilingLight (const sector_t *sec);

namespace GLRendererNew
{

//===========================================================================
// 
//
//
//===========================================================================

void MakeTextureMatrix(GLSectorPlane &splane, FMaterial *mat, Matrix3x4 &matx)
{
	matx.MakeIdentity();
	if (mat != NULL)
	{
		matx.Scale(TO_GL(splane.xscale), TO_GL(splane.yscale), 1);
		matx.Translate(TO_GL(splane.xoffs), TO_GL(splane.yoffs), 0);
		matx.Rotate(0, 0, 1, -ANGLE_TO_FLOAT(splane.angle));
	}
}

//===========================================================================
// 
//
//
//===========================================================================

void FGLSectorRenderData::AddDependency(sector_t *sec, BYTE *vertexmap, BYTE *sectormap)
{
	int secnum = sec - sectors;
	if (sectormap != NULL) sectormap[secnum>>3] |= (secnum&7);

	// This ignores vertices only used for seg splitting because those aren't needed here
	for(int i=0; i < sec->linecount; i++)
	{
		line_t *l = sec->lines[i];
		int vtnum1 = l->v1 - vertexes;
		int vtnum2 = l->v2 - vertexes;
		vertexmap[vtnum1>>3] |= (vtnum1&7);
		vertexmap[vtnum2>>3] |= (vtnum2&7);
	}
}

//===========================================================================
// 
//
//
//===========================================================================

void FGLSectorRenderData::SetupDependencies()
{
	BYTE *vertexmap = new BYTE[numvertexes/8+1];
	BYTE *linemap = new BYTE[numlines/8+1];
	BYTE *sectormap = new BYTE[numsectors/8+1];

	memset(vertexmap, 0, numvertexes/8+1);
	memset(linemap, 0, numlines/8+1);
	memset(sectormap, 0, numsectors/8+1);

	AddDependency(mSector, vertexmap, NULL);
	for(unsigned j = 0; j < mSector->e->FakeFloor.Sectors.Size(); j++)
	{
		sector_t *sec = mSector->e->FakeFloor.Sectors[j];
		// no need to make sectors dependent that don't make visual use of the heightsec
		if (sec->GetHeightSec() == mSector)
		{
			AddDependency(sec, vertexmap, sectormap);
		}
	}
	for(unsigned j = 0; j < mSector->e->XFloor.attached.Size(); j++)
	{
		sector_t *sec = mSector->e->XFloor.attached[j];
		extsector_t::xfloor &x = sec->e->XFloor;

		for(unsigned l = 0;l < x.ffloors.Size(); l++)
		{
			// Check if we really need to bother with this 3D floor
			F3DFloor * rover = x.ffloors[l];
			if (rover->model != mSector) continue;
			if (!(rover->flags & FF_EXISTS)) continue;
			if (rover->flags&FF_NOSHADE) continue; // FF_NOSHADE doesn't create any wall splits 

			AddDependency(sec, vertexmap, sectormap);
			break;
		}
	}

	// collect all dependent linedefs from the marked vertices.
	// This will include all linedefs that only share one vertex with the sector.
	for(int i = 0; i < numlines; i++)
	{
		line_t *l = &lines[i];
		int vtnum1 = l->v1 - vertexes;
		int vtnum2 = l->v2 - vertexes;

		if ((vertexmap[vtnum1>>3] & (vtnum1&7)) || (vertexmap[vtnum2>>3] & (vtnum2&7)))
		{
			linemap[i>>3] |= (i&7);
		}
	}

	// The maps are set up. Now create the dependency arrays
	for(int i=0;i<numvertexes;i++)
	{
		if (vertexmap[i>>3] & (i&7)) VertexDependencies.Push(&vertexes[i]);
	}
	VertexDependencies.ShrinkToFit();
	delete vertexmap;

	for(int i=0;i<numlines;i++)
	{
		if (linemap[i>>3] & (i&7)) LineDependencies.Push(&lines[i]);
	}
	LineDependencies.ShrinkToFit();
	delete linemap;

	for(int i=0;i<numsectors;i++)
	{
		if (sectormap[i>>3] & (i&7)) SectorDependencies.Push(&sectors[i]);
	}
	SectorDependencies.ShrinkToFit();
	delete sectormap;
}

//===========================================================================
// 
//
//
//===========================================================================

void FGLSectorRenderData::Init(int sector)
{
	mSector = &sectors[sector];
	SetupDependencies();
}

//===========================================================================
// 
//
//
//===========================================================================

void FGLSectorRenderData::CreatePlane(int in_area, sector_t *sec, GLSectorPlane &splane, 
									  int lightlevel, FDynamicColormap *cm, 
									  float alpha, bool additive, int whichplanes, bool opaque)
{
	FGLSectorPlaneRenderData &Plane = mPlanes[in_area][mPlanes[in_area].Reserve(1)];

	FTexture *tex = TexMan[splane.texture];
	FMaterial *mat = GLRenderer2->GetMaterial(tex, false, 0);
	Matrix3x4 matx;

	FPrimitive3D BasicProps;
	FVertex3D BasicVertexProps;

	MakeTextureMatrix(splane, mat, matx);
	memset(&BasicProps, 0, sizeof(BasicProps));
	memset(&BasicVertexProps, 0, sizeof(BasicVertexProps));
	
	
	FRenderStyle style;
	if (additive) 
	{
		style = STYLE_Add;
		BasicProps.mTranslucent = true;
	}
	else if (alpha < 1.0f) 
	{
		style = STYLE_Translucent;
		BasicProps.mTranslucent = true;
	}
	else 
	{
		style = STYLE_Normal;
		BasicProps.mTranslucent = false;
	}

	if (tex != NULL && !tex->gl_info.mIsTransparent)
	{
		BasicProps.mAlphaThreshold = alpha * 0.5f;
	}
	else 
	{
		BasicProps.mAlphaThreshold = 0;
	}

	BasicProps.SetRenderStyle(style, opaque);
	BasicProps.mMaterial = mat;
	BasicProps.mPrimitiveType = GL_TRIANGLE_FAN;	// all subsectors are drawn as triangle fans
	BasicProps.mDesaturation = cm->Desaturate;
	BasicVertexProps.SetLighting(lightlevel, cm, 0/*rellight*/, additive, tex);
	BasicVertexProps.a = quickertoint(alpha*255);

	Plane.mPlane = splane.plane;
	Plane.mUpside = !!(whichplanes&1);
	Plane.mDownside = !!(whichplanes&2);
	Plane.mLightEffect = !splane.texture.Exists();
	Plane.mSubsectorData.Resize(sec->subsectorcount);
	for(int i=0;i<sec->subsectorcount;i++)
	{
		FGLSubsectorPlaneRenderData *ssrd = &Plane.mSubsectorData[i];
		subsector_t *sub = sec->subsectors[i];

		ssrd->mPrimitive = BasicProps;
		ssrd->mPrimitive.mVertexCount = sub->numlines;
		ssrd->mVertices.Resize(sub->numlines);
		for(int j=0; j<sub->numlines; j++)
		{
			vertex_t *v = segs[sub->firstline + j].v1;

			ssrd->mVertices[j] = BasicVertexProps;
			ssrd->mVertices[j].x = v->fx;
			ssrd->mVertices[j].y = v->fy;
			ssrd->mVertices[j].z = splane.plane.ZatPoint(v->fx, v->fy);

			Vector uv(v->fx, -v->fy, 0);
			uv = matx * uv;

			ssrd->mVertices[j].x = uv.X();
			ssrd->mVertices[j].y = uv.Y();
		}
	}

}


//===========================================================================
// 
//
//
//===========================================================================

void FGLSectorRenderData::Invalidate()
{
	mPlanes[0].Clear();
	mPlanes[1].Clear();
	mPlanes[2].Clear();
}


//===========================================================================
// 
//
//
//===========================================================================

#define SET_COLOR() \
	do \
	{ \
		if (rover->flags&FF_FOG) \
		{ \
			Colormap.Color = (light->extra_colormap)->Fade; \
			splane.texture.SetInvalid(); \
		} \
		else \
		{ \
			Colormap.Color = (light->extra_colormap)->Color; \
			Colormap.Desaturate = (light->extra_colormap)->Desaturate; \
		} \
		Colormap.Fade = sec->ColorMap->Fade; \
	} while (0)


//===========================================================================
// 
//
//
//===========================================================================

void FGLSectorRenderData::Validate(int in_area)
{
	sector_t copy;
	sector_t *sec;

	int lightlevel;
	lightlist_t *light;
	FDynamicColormap Colormap;
	GLSectorPlane splane;

	if (mPlanes[in_area].Size() == 0)
	{
		extsector_t::xfloor &x = mSector->e->XFloor;

		TArray<FGLSectorPlaneRenderData> &plane = mPlanes[in_area];
		::in_area = area_t(in_area);
		sec = gl_FakeFlat(mSector, &copy, false);

		// create ceiling plane

		lightlevel = GetCeilingLight(sec);
		Colormap = *sec->ColorMap;
		bool stack = sec->CeilingSkyBox && sec->CeilingSkyBox->bAlways;
		float alpha = stack ? sec->CeilingSkyBox->PlaneAlpha/65536.0f : 1.0f-sec->GetCeilingReflect();
		if (alpha != 0.0f)
		{
			if (x.ffloors.Size())
			{
				light = P_GetPlaneLight(sec, &sec->ceilingplane, true);

				if (!(sec->GetFlags(sector_t::ceiling)&SECF_ABSLIGHTING)) lightlevel = *light->p_lightlevel;
				Colormap.Color = (light->extra_colormap)->Color;
				Colormap.Desaturate = (light->extra_colormap)->Desaturate;
			}
			splane.GetFromSector(sec, true);
			CreatePlane(in_area, sec, splane, lightlevel, &Colormap, alpha, false, 1, !stack);
		}

		// create floor plane

		lightlevel = GetFloorLight(sec);
		Colormap = *sec->ColorMap;
		stack = sec->FloorSkyBox && sec->FloorSkyBox->bAlways;
		alpha = stack ? sec->FloorSkyBox->PlaneAlpha/65536.0f : 1.0f-sec->GetFloorReflect();
		if (alpha != 0.0f)
		{
			if (x.ffloors.Size())
			{
				light = P_GetPlaneLight(sec, &sec->floorplane, false);

				if (!(sec->GetFlags(sector_t::floor)&SECF_ABSLIGHTING) || light!=&x.lightlist[0])	
					lightlevel = *light->p_lightlevel;

				Colormap.Color = (light->extra_colormap)->Color;
				Colormap.Desaturate = (light->extra_colormap)->Desaturate;
			}
			splane.GetFromSector(sec, false);
			CreatePlane(in_area, sec, splane, lightlevel, &Colormap, alpha, false, 2, !stack);
		}

		// create 3d floor planes

		if (x.ffloors.Size())
		{
			player_t * player=players[consoleplayer].camera->player;

			// 3d-floors must not overlap!
			fixed_t lastceilingheight=sec->CenterCeiling();	// render only in the range of the
			fixed_t lastfloorheight=sec->CenterFloor();		// current sector part (if applicable)
			F3DFloor * rover;	
			int k;
			
			// floors are ordered now top to bottom so scanning the list for the best match
			// is no longer necessary.

			for(k=0;k<(int)x.ffloors.Size();k++)
			{
				rover=x.ffloors[k];
				
				if ((rover->flags&(FF_EXISTS|FF_RENDERPLANES))==(FF_EXISTS|FF_RENDERPLANES))
				{
					if (rover->flags&(FF_INVERTPLANES|FF_BOTHPLANES))
					{
						fixed_t ff_top = rover->top.plane->ZatPoint(CenterSpot(sec));
						if (ff_top < lastfloorheight) break;
						if (ff_top < lastceilingheight)
						{
							// FF_FOG requires an inverted logic where to get the light from
							light = P_GetPlaneLight(sec, rover->top.plane,!!(rover->flags&FF_FOG));
							lightlevel = *light->p_lightlevel;
							splane.GetFromSector(rover->top.model, rover->top.isceiling);
							
							SET_COLOR();

							CreatePlane(in_area, sec, splane, lightlevel, &Colormap, rover->alpha/255.f, 
								!!(rover->flags&FF_ADDITIVETRANS), 1, false);
							lastceilingheight = ff_top;
						}
					}
					if (!(rover->flags&FF_INVERTPLANES))
					{
						fixed_t ff_bottom = rover->bottom.plane->ZatPoint(CenterSpot(sec));
						if (ff_bottom < lastfloorheight) break;
						if (ff_bottom < lastceilingheight)
						{
							light = P_GetPlaneLight(sec, rover->bottom.plane, !(rover->flags&FF_FOG));
							lightlevel = *light->p_lightlevel;
							splane.GetFromSector(rover->bottom.model, rover->bottom.isceiling);

							SET_COLOR();

							CreatePlane(in_area, sec, splane, lightlevel, &Colormap, rover->alpha/255.f, 
								!!(rover->flags&FF_ADDITIVETRANS), 1, false);

							lastceilingheight = ff_bottom;
							if (rover->alpha<255) lastceilingheight++;
						}
					}
				}
			}
			
			// restore value
			lastceilingheight=sec->CenterCeiling();	// render only in the range of the

			for(k=x.ffloors.Size()-1;k>=0;k--)
			{
				rover=x.ffloors[k];
				
				if ((rover->flags&(FF_EXISTS|FF_RENDERPLANES))==(FF_EXISTS|FF_RENDERPLANES))
				{
					if (rover->flags&(FF_INVERTPLANES|FF_BOTHPLANES))
					{
						fixed_t ff_bottom = rover->bottom.plane->ZatPoint(CenterSpot(sec));
						if (ff_bottom > lastceilingheight) break;
						if (ff_bottom > lastfloorheight || (rover->flags&FF_FIX))
						{
							splane.GetFromSector(rover->bottom.model, rover->bottom.isceiling);
							if (rover->flags&FF_FIX)
							{
								lightlevel = rover->model->lightlevel;
								Colormap = *rover->model->ColorMap;
							}
							else
							{
								light = P_GetPlaneLight(sec, rover->bottom.plane,!(rover->flags&FF_FOG));
								lightlevel = *light->p_lightlevel;

								SET_COLOR();
							}

							CreatePlane(in_area, sec, splane, lightlevel, &Colormap, rover->alpha/255.f, 
								!!(rover->flags&FF_ADDITIVETRANS), 2, false);

							lastfloorheight = ff_bottom;
						}
					}

					if (!(rover->flags&FF_INVERTPLANES))
					{
						fixed_t ff_top = rover->top.plane->ZatPoint(CenterSpot(sec));
						if (ff_top > lastceilingheight) break;
						if (ff_top > lastfloorheight)
						{
							light=P_GetPlaneLight(sec, rover->top.plane,!!(rover->flags&FF_FOG));
							lightlevel = *light->p_lightlevel;
							splane.GetFromSector(rover->top.model, rover->top.isceiling);

							SET_COLOR();

							CreatePlane(in_area, sec, splane, lightlevel, &Colormap, rover->alpha/255.f, 
								!!(rover->flags&FF_ADDITIVETRANS), 2, false);

							lastfloorheight = ff_top;
							if (rover->alpha < 255) lastfloorheight--;
						}
					}
				}
			}
		}
	}
}



}// namespace


