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
#include "r_sky.h"
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

void FSectorRenderData::AddDependency(sector_t *sec, BYTE *vertexmap, BYTE *sectormap)
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

void FSectorRenderData::SetupDependencies()
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

void FSectorRenderData::Init(int sector)
{
	mSector = &sectors[sector];
	SetupDependencies();
}

//===========================================================================
// 
//
//
//===========================================================================

void FSectorRenderData::CreatePlane(FSectorPlaneObject *plane,
									  int in_area, sector_t *sec, GLSectorPlane &splane, 
									  int lightlevel, FDynamicColormap *cm, 
									  float alpha, bool additive, bool upside, bool opaque)
{

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

	plane->mMat = mat;
	plane->mAlpha = alpha < 1.0 - FLT_EPSILON;
	plane->mSector = &sectors[sec->sectornum];
	BasicProps.SetRenderStyle(style, opaque);
	BasicProps.mMaterial = mat;
	BasicProps.mPrimitiveType = GL_TRIANGLE_FAN;	// all subsectors are drawn as triangle fans
	BasicProps.mDesaturation = cm->Desaturate;
	BasicVertexProps.SetLighting(lightlevel, cm, 0/*rellight*/, additive, tex);
	BasicVertexProps.a = (unsigned char)quickertoint(alpha*255);


	plane->mPlane = splane.plane;
	plane->mUpside = upside;
	plane->mLightEffect = !splane.texture.Exists();
	plane->mPrimitiveIndex = mPrimitives.Reserve(sec->subsectorcount);

	FSubsectorPrimitive *prim = &mPrimitives[plane->mPrimitiveIndex];

	for(int i=0;i<sec->subsectorcount;i++, prim++)
	{
		subsector_t *sub = sec->subsectors[i];

		prim->mPrimitive = BasicProps;
		prim->mPrimitive.mVertexCount = sub->numlines;
		prim->mPrimitive.mVertexStart = mVertices.Reserve(sub->numlines);

		FVertex3D *vert = &mVertices[prim->mPrimitive.mVertexStart];
		for(int j=0; j<sub->numlines; j++, vert++)
		{
			vertex_t *v = segs[sub->firstline + j].v1;

			*vert = BasicVertexProps;
			vert->x = v->fx;
			vert->y = v->fy;
			vert->z = splane.plane.ZatPoint(v->fx, v->fy);

			Vector uv(v->fx, -v->fy, 0);
			uv = matx * uv;

			vert->u = uv.X();
			vert->v = uv.Y();
		}
	}
}


//===========================================================================
// 
//
//
//===========================================================================

void FSectorRenderData::Invalidate()
{
	mAreas[0].valid =
	mAreas[1].valid =
	mAreas[2].valid = false;
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

void FSectorRenderData::Validate(area_t in_area)
{
	sector_t copy;
	sector_t *sec;

	int lightlevel;
	lightlist_t *light;
	FDynamicColormap Colormap;
	GLSectorPlane splane;

	if (in_area >= area_above) return;

	FSectorAreaData *area = &mAreas[in_area];
	if (!area->valid)
	{
		area->m3DFloorsC.Clear();
		area->m3DFloorsF.Clear();

		extsector_t::xfloor &x = mSector->e->XFloor;

		sec = gl_FakeFlat(mSector, &copy, in_area, false);

		// create ceiling plane

		lightlevel = GetCeilingLight(sec);
		Colormap = *sec->ColorMap;
		bool stack = sec->CeilingSkyBox && sec->CeilingSkyBox->bAlways;
		float alpha = stack ? sec->CeilingSkyBox->PlaneAlpha/65536.0f : 1.0f-sec->GetCeilingReflect();
		if (alpha != 0.0f && sec->GetTexture(sector_t::ceiling) != skyflatnum)
		{
			if (x.ffloors.Size())
			{
				light = P_GetPlaneLight(sec, &sec->ceilingplane, true);

				if (!(sec->GetFlags(sector_t::ceiling)&SECF_ABSLIGHTING)) lightlevel = *light->p_lightlevel;
				Colormap.Color = (light->extra_colormap)->Color;
				Colormap.Desaturate = (light->extra_colormap)->Desaturate;
			}
			splane.GetFromSector(sec, true);
			CreatePlane(&area->mCeiling, in_area, sec, splane, lightlevel, &Colormap, alpha, false, false, !stack);
		}
		else area->mCeiling.mPrimitiveIndex = -1;

		// create floor plane

		lightlevel = GetFloorLight(sec);
		Colormap = *sec->ColorMap;
		stack = sec->FloorSkyBox && sec->FloorSkyBox->bAlways;
		alpha = stack ? sec->FloorSkyBox->PlaneAlpha/65536.0f : 1.0f-sec->GetFloorReflect();
		if (alpha != 0.0f && sec->GetTexture(sector_t::ceiling) != skyflatnum)
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
			if (sec->transdoor) splane.plane.d -= 1;
			CreatePlane(&area->mFloor, in_area, sec, splane, lightlevel, &Colormap, alpha, false, true, !stack);
		}
		else area->mFloor.mPrimitiveIndex = -1;

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

							FSectorPlaneObject *obj = &area->m3DFloorsC[area->m3DFloorsC.Reserve(1)];
							CreatePlane(obj, in_area, sec, splane, lightlevel, &Colormap, rover->alpha/255.f, 
								!!(rover->flags&FF_ADDITIVETRANS), false, false);
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

							FSectorPlaneObject *obj = &area->m3DFloorsC[area->m3DFloorsC.Reserve(1)];
							CreatePlane(obj, in_area, sec, splane, lightlevel, &Colormap, rover->alpha/255.f, 
								!!(rover->flags&FF_ADDITIVETRANS), false, false);

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

							FSectorPlaneObject *obj = &area->m3DFloorsF[area->m3DFloorsF.Reserve(1)];
							CreatePlane(obj, in_area, sec, splane, lightlevel, &Colormap, rover->alpha/255.f, 
								!!(rover->flags&FF_ADDITIVETRANS), true, false);

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

							FSectorPlaneObject *obj = &area->m3DFloorsF[area->m3DFloorsF.Reserve(1)];
							CreatePlane(obj, in_area, sec, splane, lightlevel, &Colormap, rover->alpha/255.f, 
								!!(rover->flags&FF_ADDITIVETRANS), true, false);

							lastfloorheight = ff_top;
							if (rover->alpha < 255) lastfloorheight--;
						}
					}
				}
			}
		}
	}
}


//===========================================================================
// 
//
//
//===========================================================================

void FSectorRenderData::Process(subsector_t *sub, area_t in_area)
{
	extsector_t::xfloor &x = mSector->e->XFloor;
	GLDrawInfo *di = GLRenderer2->mDrawInfo;
	int subno = sub - subsectors;

	di->ss_renderflags[subno] |= SSRF_PROCESSED;
	if (sub->hacked & 1) di->AddHackedSubsector(sub);
	if (sub->degenerate) return;

	Validate(in_area);

	FSectorAreaData *area = &mAreas[in_area];
	byte &srf = di->sectorrenderflags[mSector->sectornum];

	//
	//
	//
	// do floors
	//
	//
	//
	if ((srf&SSRF_RENDERFLOOR) || area->mFloor.isVisible(di->mViewpoint, true))
	{
		di->ss_renderflags[subno] |= SSRF_RENDERFLOOR;

		// process the original floor first.
		if (mSector->FloorSkyBox && mSector->FloorSkyBox->bAlways) di->AddFloorStack(sub);

		if (!(srf & SSRF_RENDERFLOOR))
		{
			srf |= SSRF_RENDERFLOOR;
			if (area->mFloor.mPrimitiveIndex >= 0) di->AddObject(&area->mFloor);
		}
	}
	
	//
	//
	//
	// do ceilings
	//
	//
	//
	if ((srf & SSRF_RENDERCEILING) || area->mCeiling.isVisible(di->mViewpoint, false))
	{
		di->ss_renderflags[subno]|=SSRF_RENDERCEILING;

		// process the original ceiling first.
		if (mSector->CeilingSkyBox && mSector->CeilingSkyBox->bAlways) di->AddCeilingStack(sub);

		if (!(srf & SSRF_RENDERCEILING))
		{
			srf |= SSRF_RENDERCEILING;
			if (area->mCeiling.mPrimitiveIndex >= 0) di->AddObject(&area->mCeiling);
		}
	}

	//
	//
	//
	// do 3D floors
	//
	//
	//

	if (x.ffloors.Size())
	{
		player_t * player=players[consoleplayer].camera->player;

		// do the plane setup only once and just mark all subsectors that have to be processed
		di->ss_renderflags[sub-subsectors]|=SSRF_RENDER3DPLANES;

		if (!(srf & SSRF_RENDER3DPLANES))
		{
			srf |= SSRF_RENDER3DPLANES;

			for(unsigned k = 0; k < area->m3DFloorsC.Size(); k++)
			{
				if (area->m3DFloorsC[k].isVisible(di->mViewpoint, false))
				{
					di->AddObject(&area->m3DFloorsC[k]);
				}
				else break;	// 3D floors are ordered so if one is not visible the remaining ones are neither.
			}

			for(unsigned k = 0; k < area->m3DFloorsF.Size(); k++)
			{
				if (area->m3DFloorsF[k].isVisible(di->mViewpoint, true))
				{
					di->AddObject(&area->m3DFloorsF[k]);
				}
				else break;	// 3D floors are ordered so if one is not visible the remaining ones are neither.
			}
		}
	}
}



}// namespace


