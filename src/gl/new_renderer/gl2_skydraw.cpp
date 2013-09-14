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
// Sky rendering
//

#include "gl/gl_include.h"
#include "c_cvars.h"
#include "g_level.h"
#include "m_fixed.h"
#include "r_state.h"
#include "templates.h"
#include "textures/textures.h"
#include "gl/common/glc_renderer.h"
#include "gl/common/glc_convert.h"
#include "gl/common/glc_data.h"
#include "gl/common/glc_skyboxtexture.h"
#include "gl/new_renderer/gl2_renderer.h"
#include "gl/new_renderer/gl2_skydraw.h"
#include "gl/new_renderer/textures/gl2_shader.h"
#include "gl/new_renderer/textures/gl2_material.h"

EXTERN_CVAR (Bool, r_stretchsky)
extern long gl_frameMS;


namespace GLRendererNew
{

inline bool SkyStretch()
{
	return (r_stretchsky && !(level.flags & LEVEL_FORCENOSKYSTRETCH));
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

FVertexBufferSky::FVertexBufferSky()
{
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------
#define VT2 ((FVertexSky*)NULL)

bool FVertexBufferSky::Bind()
{
	BindBuffer();
	glVertexPointer(3,GL_FLOAT, sizeof(FVertexSky), &VT2->x);
	glTexCoordPointer(2,GL_FLOAT, sizeof(FVertexSky), &VT2->u);
	glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(FVertexSky), &VT2->r);

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);
	gl.DisableVertexAttribArray(FShaderObject::attrLightParams);
	gl.DisableVertexAttribArray(FShaderObject::attrFogColor);
	gl.DisableVertexAttribArray(FShaderObject::attrGlowDistance);
	gl.DisableVertexAttribArray(FShaderObject::attrGlowTopColor);
	gl.DisableVertexAttribArray(FShaderObject::attrGlowBottomColor);
	return true;
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

void FVertexBufferSky::SetVertices(FVertexSky *verts, unsigned int count)
{
	BindBuffer(); 
	gl.BufferData(GL_ARRAY_BUFFER, count * sizeof(FVertexSky), verts, GL_STATIC_DRAW);
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

FSkyDrawer::FSkyDrawer()
{
	memset(mSkyVBOs, 0, sizeof(mSkyVBOs));
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

FSkyDrawer::~FSkyDrawer()
{
	Clear();
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

FVertexBufferSky *FSkyDrawer::FindVBO(int type, FTexture* t1, FTexture* t2, PalEntry fogcolor, bool stretch)
{
	for(int i=0;i<5;i++)
	{
		if (mSkyVBOs[i] != NULL && 
			mSkyVBOs[i]->tex[0] == t1 && 
			mSkyVBOs[i]->tex[1] == t2 && 
			mSkyVBOs[i]->fogcolor == fogcolor && 
			mSkyVBOs[i]->type == type &&
			mSkyVBOs[i]->stretch == stretch)
		{
			mSkyVBOs[i]->lastused = gl_frameMS;
			return mSkyVBOs[i];
		}
	}
	return NULL;
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

void FSkyDrawer::CacheVBO(FVertexBufferSky *vbo)
{
	int best = -1;
	int besttime = INT_MAX;
	for(int i=0;i<5;i++)
	{
		if (mSkyVBOs[i] == NULL)
		{
			mSkyVBOs[i] = vbo;
			mSkyVBOs[i]->lastused = gl_frameMS;
			return;
		}
		else if (mSkyVBOs[i]->lastused < besttime)
		{
			best = i;
			besttime = mSkyVBOs[i]->lastused;
		}
	}
	if (best > -1)
	{
		delete mSkyVBOs[best];
		mSkyVBOs[best] = vbo;
		mSkyVBOs[best]->lastused = gl_frameMS;
	}
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void FSkyDrawer::SkyVertex(FVertexSky *dest, int r, int c, int texw, float yMult, bool yflip, bool textured)
{
	angle_t topAngle= (angle_t)(c / (float)columns * ANGLE_MAX);
	angle_t sideAngle = maxSideAngle * (rows - r) / rows;
	fixed_t height = finesine[sideAngle>>ANGLETOFINESHIFT];
	fixed_t realRadius = FixedMul(scale, finecosine[sideAngle>>ANGLETOFINESHIFT]);
	fixed_t x = FixedMul(realRadius, finecosine[topAngle>>ANGLETOFINESHIFT]);
	fixed_t y = (!yflip) ? FixedMul(scale, height) : FixedMul(scale, height) * -1;
	fixed_t z = FixedMul(realRadius, finesine[topAngle>>ANGLETOFINESHIFT]);
	
	if (textured)
	{
		float timesRepeat = (short)(4 * (256.f / texw));
		
		if (timesRepeat == 0.f) timesRepeat = 1.f;

		// And the texture coordinates.
		dest->u = (-timesRepeat * c / (float)columns) ;
		if(!yflip)	// Flipped Y is for the lower hemisphere.
		{
			dest->v = (r / (float)rows) * yMult;
		}
		else
		{
			dest->v = ((rows-r)/(float)rows) * yMult;
		}
	}
	else
	{
		dest->u = dest->v = 0;
	}
	// And finally the vertex.
	dest->x =-TO_GL(x);	// Doom mirrors the sky vertically
	dest->y = TO_GL(y) - 1.f;
	dest->z = TO_GL(z);
	dest->r = dest->g = dest->b = 255;
	dest->a = r==0? 0:255;
}


//-----------------------------------------------------------------------------
//
// Hemi is Upper or Lower. Zero is not acceptable.
// The current texture is used. SKYHEMI_NO_TOPCAP can be used.
//
//-----------------------------------------------------------------------------

void FSkyDrawer::GenerateHemispheres(int texno, FTexture *tex, TArray<FPrimitiveSky> &prims, TArray<FVertexSky> &verts)
{
	int r, c;
	FPrimitiveSky *prim;
	FVertexSky *vert;
	float yMult;
	PalEntry capcolor; 
	
	int texw = tex->GetScaledWidth();
	int texh = tex->GetScaledHeight();

	if (texh>190 && SkyStretch()) texh=190;
	if (texh<=180) yMult = 1.0f;
	else yMult = 180.0f / texh;

	// Draw the cap as one solid color polygon (only for the bottom layer. Rendering a cap
	// for the second layer will look bad

	if (texno != 2)
	{
		for(int i=0;i<2;i++)
		{
			prim = &prims[prims.Reserve(1)];
			prim->mType = GL_TRIANGLE_FAN;
			prim->mUseTexture = 0;
			prim->mStartVertex = verts.Size();
			prim->mVertexCount = columns;

			vert = &verts[verts.Reserve(columns)];
			for(c = 0; c < columns; c++)
			{
				SkyVertex(&vert[c], 1, c, 0, 0, !!i, false);
				capcolor = tex->GetSkyCapColor(!!i);
				vert[c].r = capcolor.r;
				vert[c].g = capcolor.g;
				vert[c].b = capcolor.b;
			}
		}
	}
	// The total number of triangles per hemisphere can be calculated
	// as follows: rows * columns * 2 + 2 (for the top cap).
	for(int i=0;i<2;i++)
	{
		for(r = 0; r < rows; r++)
		{
			prim = &prims[prims.Reserve(1)];
			prim->mType = GL_TRIANGLE_STRIP;
			prim->mUseTexture = texno;
			prim->mStartVertex = verts.Size();
			prim->mVertexCount = columns*2+2;
			vert = &verts[verts.Reserve(prim->mVertexCount)];

			if (i)
			{
				for(c = 0; c <= columns; c++)
				{
					SkyVertex(vert++, r + 1, c, texw, yMult, true, true);
					SkyVertex(vert++, r, c, texw, yMult, true, true);
				}
			}
			else
			{
				for(c = 0; c <= columns; c++)
				{
					SkyVertex(vert++, r, c, texw, yMult, false, true);
					SkyVertex(vert++, r + 1, c, texw, yMult, false, true);
				}
			}
		}
	}
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

FVertexBufferSky *FSkyDrawer::CreateDomeVBO(FTexture *tex1, FTexture *tex2, PalEntry fogcolor)
{
	TArray<FVertexSky> verts;
	FVertexBufferSky *vbo = new FVertexBufferSky();

	GenerateHemispheres(1, tex1, vbo->mPrimitives, verts);
	if (tex2 != NULL && tex2 != tex1)
	{
		GenerateHemispheres(2, tex2, vbo->mPrimitives, verts);
	}
	if (fogcolor != 0)
	{
		// define a simple large tetrahedron around the viewpoint.
		static float fx[] = {5000, 0, -5000, 0, 5000, 0};
		static float fy[] = {-5000, 0, -5000, 5000, -5000, 0};
		static float fz[] = {-5000, 5000, -5000, -5000, -5000, 5000};

		FPrimitiveSky *prim = &vbo->mPrimitives[vbo->mPrimitives.Reserve(1)];
		prim->mType = GL_TRIANGLE_STRIP;
		prim->mUseTexture = 3;
		prim->mStartVertex = verts.Size();
		prim->mVertexCount = 6;
		FVertexSky *vert = &verts[verts.Reserve(6)];
		for(int i=0;i<6;i++)
		{
			vert[i].x = fx[i];
			vert[i].y = fy[i];
			vert[i].z = fz[i];
			vert[i].u = vert[i].v = 0;
			vert[i].r = fogcolor.r;
			vert[i].g = fogcolor.g;
			vert[i].b = fogcolor.b;
			vert[i].a = fogcolor.a;
		}
	}

	vbo->SetVertices(&verts[0], verts.Size());
	vbo->tex[0] = tex1;
	vbo->tex[1] = tex2;
	vbo->fogcolor = fogcolor;
	vbo->stretch = SkyStretch();
	vbo->type = SKYVBO_Dome;
	return vbo;
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

FVertexBufferSky *FSkyDrawer::CreateBoxVBO(int type)
{
	static FVertexSky verts[24] = {
		// north face
		{  128,  128, -128, 0, 0, 255, 255, 255, 255 },
		{ -128,  128, -128, 0, 0, 255, 255, 255, 255 },
		{ -128, -128, -128, 0, 0, 255, 255, 255, 255 },
		{  128, -128, -128, 0, 0, 255, 255, 255, 255 },
		// east face
		{ -128,  128, -128, 0, 0, 255, 255, 255, 255 },
		{ -128,  128,  128, 0, 0, 255, 255, 255, 255 },
		{ -128, -128,  128, 0, 0, 255, 255, 255, 255 },
		{ -128, -128, -128, 0, 0, 255, 255, 255, 255 },
		// south face
		{ -128,  128,  128, 0, 0, 255, 255, 255, 255 },
		{  128,  128,  128, 0, 0, 255, 255, 255, 255 },
		{  128, -128,  128, 0, 0, 255, 255, 255, 255 },
		{ -128, -128,  128, 0, 0, 255, 255, 255, 255 },
		// west face
		{  128,  128,  128, 0, 0, 255, 255, 255, 255 },
		{  128,  128, -128, 0, 0, 255, 255, 255, 255 },
		{  128, -128, -128, 0, 0, 255, 255, 255, 255 },
		{  128, -128,  128, 0, 0, 255, 255, 255, 255 },
		// top face
		{  128,  128, -128, 0, 0, 255, 255, 255, 255 },
		{ -128,  128, -128, 0, 0, 255, 255, 255, 255 },
		{ -128,  128,  128, 0, 0, 255, 255, 255, 255 },
		{  128,  128,  128, 0, 0, 255, 255, 255, 255 },
		// bottom face
		{  128, -128, -128, 0, 0, 255, 255, 255, 255 },
		{ -128, -128, -128, 0, 0, 255, 255, 255, 255 },
		{ -128, -128,  128, 0, 0, 255, 255, 255, 255 },
		{  128, -128,  128, 0, 0, 255, 255, 255, 255 }
	};

	int sixface = (type == SKYVBO_Box6 || type == SKYVBO_Box6f);

	if (sixface)
	{
		for(int i=0;i<4;i++)
		{
			verts[i*4+0].u = 0; verts[i*4+0].v = 0;
			verts[i*4+1].u = 1; verts[i*4+1].v = 0;
			verts[i*4+2].u = 1; verts[i*4+2].v = 1;
			verts[i*4+3].u = 0; verts[i*4+3].v = 1;
		}
	}
	else
	{
		for(int i=0;i<4;i++)
		{
			float l = i*0.25;
			float r = l+0.25f;
			verts[i*4+0].u = l; verts[i*4+0].v = 0;
			verts[i*4+1].u = r; verts[i*4+1].v = 0;
			verts[i*4+2].u = r; verts[i*4+2].v = 1;
			verts[i*4+3].u = l; verts[i*4+3].v = 1;
		}
	}
	int t = (type == SKYVBO_Box3f || type == SKYVBO_Box6f);

	verts[4*4+0].u = 0; verts[4*4+0].v = t;
	verts[4*4+1].u = 1; verts[4*4+1].v = t;
	verts[4*4+2].u = 1; verts[4*4+2].v = 1-t;
	verts[4*4+3].u = 0; verts[4*4+3].v = 1-t;

	verts[5*4+0].u = 0; verts[5*4+0].v = 0;
	verts[5*4+1].u = 1; verts[5*4+1].v = 0;
	verts[5*4+2].u = 1; verts[5*4+2].v = 1;
	verts[5*4+3].u = 0; verts[5*4+3].v = 1;

	FVertexBufferSky *vbo = new FVertexBufferSky();

	vbo->tex[0] = vbo->tex[1] = NULL;
	vbo->fogcolor = 0;
	vbo->stretch = false;
	vbo->type = type;

	vbo->mPrimitives.Resize(6);
	for(int i=0;i<6;i++)
	{
		FPrimitiveSky *prim = &vbo->mPrimitives[i];
		prim->mStartVertex = i*4;
		prim->mVertexCount = 4;
		prim->mType = GL_TRIANGLE_FAN;
		prim->mUseTexture = sixface? i : MAX(0, i-3);
	}
	vbo->SetVertices(verts,24);

	return vbo;
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

void FSkyDrawer::Clear()
{
	for(int i=0;i<5;i++)
	{
		if (mSkyVBOs[i] != NULL) delete mSkyVBOs[i];
	}
	memset(mSkyVBOs, 0, sizeof(mSkyVBOs));
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

void FSkyDrawer::RenderSkyBox(FTextureID tex1, float xofs, const FVector3 &axis)
{
	// Always use the cap color from the first texture in an animation. 
	// This prevents flickering effects if the animation frames produce different cap colors
	FTexture *tex_info = TexMan[tex1];
	FTexture *tex_render = TexMan(tex1);
	FVertexBufferSky *vbo = NULL;
	FMaterial *material;
	int texh;

	if (tex_info->gl_info.bSkybox != tex_render->gl_info.bSkybox)
	{
		// Ugh... Switching animations between these types 
		// will look extremely ugly so don't do it.
		tex_render = tex_info;
	}

	static const ESkyVBOType type[] = { SKYVBO_Box6, SKYVBO_Box6f, SKYVBO_Box3, SKYVBO_Box3f};
	// Check if it's a 3 or 6 face skybox
	bool is3face = static_cast<FSkyBox*>(tex_info)->Is3Face();
	bool isflipped = static_cast<FSkyBox*>(tex_info)->IsFlipped();
	int boxtype = type[is3face*2 + isflipped];
	vbo = FindVBO(boxtype, NULL, NULL, 0, false);
	if (vbo == NULL)
	{
		vbo = CreateBoxVBO(boxtype);
		CacheVBO(vbo);
	}
	if (vbo == NULL) return;

	//----------------------------------------------------------------------------
	//
	// Render the sky using the retrieved VBO
	//
	//----------------------------------------------------------------------------

	// set up the view matrix. Always view from (0,0,0)
	gl.PushMatrix();
	GLRenderer->SetCameraPos(0, 0, 0, viewangle);
	GLRenderer2->SetViewMatrix(!!(GLRenderer->mMirrorCount&1), !!(GLRenderer->mPlaneMirrorCount&1));

	gl.Enable(GL_ALPHA_TEST);
	gl.AlphaFunc(GL_GEQUAL,0.05f);
	gl.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// transformation settings for the first layer
	gl.PushMatrix();
	texh = tex_info->GetScaledHeight();
	gl.Rotatef(-180.0f+xofs, axis.X, axis.Z, axis.Y);

	int lasttexturetype = -1;
	vbo->Bind();
	for(unsigned i=0;i<vbo->mPrimitives.Size(); i++)
	{
		FPrimitiveSky *prim = &vbo->mPrimitives[i];

		if (lasttexturetype != prim->mUseTexture)
		{
			// different face than last primitive
			FSkyBox *sky = static_cast<FSkyBox*>(tex_render);
			lasttexturetype = prim->mUseTexture;
			material = GLRenderer2->GetMaterial(sky->faces[lasttexturetype], true, 0);
			material->Bind(NULL, TM_OPAQUE, -1, true);
		}
		gl.DrawArrays(prim->mType, prim->mStartVertex, prim->mVertexCount);
	}
	gl.PopMatrix();
	gl.PopMatrix();
}


//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

void FSkyDrawer::RenderSkyDome(FTextureID tex1, FTextureID tex2, PalEntry fogcolor, float xofs1, float xofs2, float yofs)
{
	// Always use the cap color from the first texture in an animation. 
	// This prevents flickering effects if the animation frames produce different cap colors
	FTexture *tex_info = TexMan[tex1];
	FTexture *tex_render = TexMan(tex1);
	FTexture *tex_info2 = NULL;
	FTexture *tex_render2 = NULL;
	FVertexBufferSky *vbo = NULL;
	FMaterial *material;
	int texh;

	if (tex_info->gl_info.bSkybox != tex_render->gl_info.bSkybox)
	{
		// Ugh... Switching animations between these types 
		// will look extremely ugly so don't do it.
		tex_render = tex_info;
	}

	
	if (tex2.isValid())
	{
		tex_info2 = TexMan[tex2];
		tex_render2 = TexMan(tex2);
	}

	vbo = FindVBO(SKYVBO_Dome, tex_info, tex_info2, fogcolor, SkyStretch());
	if (vbo == NULL)
	{
		vbo = CreateDomeVBO(tex_info, tex_info2, fogcolor);
		CacheVBO(vbo);
	}
	if (vbo == NULL) return;

	//----------------------------------------------------------------------------
	//
	// Render the sky using the retrieved VBO
	//
	//----------------------------------------------------------------------------

	// Handle y-scrolling
	// This is only necessary for MBF sky transfers with a scrolling texture
	if (yofs != 0)
	{
		yofs = yofs / tex_info->GetScaledHeight();
		yofs -= floor(yofs);
		gl.MatrixMode(GL_TEXTURE);
		gl.PushMatrix();
		gl.Translatef(0, 0, yofs);
		gl.MatrixMode(GL_MODELVIEW);
	}


	// set up the view matrix. Always view from (0,0,0)
	gl.PushMatrix();
	GLRenderer->SetCameraPos(0, 0, 0, viewangle);
	GLRenderer2->SetViewMatrix(!!(GLRenderer->mMirrorCount&1), !!(GLRenderer->mPlaneMirrorCount&1));
	// For sky domes shift the view up a little to push the border between upper and lower hemisphere
	// below the horizon.
	gl.Translatef(0.f, -1000.f, 0.f);

	gl.Enable(GL_ALPHA_TEST);
	gl.AlphaFunc(GL_GEQUAL,0.05f);
	gl.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// transformation settings for the first layer
	gl.PushMatrix();
	texh = tex_info->GetScaledHeight();
	gl.Rotatef(-180.0f+xofs1, 0.f, 1.f, 0.f);
	if (texh>190 && SkyStretch()) texh=190;
	else if (texh<=180 && !SkyStretch()) // && skystretch)
	{
		gl.Scalef(1.f, texh/180.f, 1.f);
	}

	int lasttexturetype = -1;
	vbo->Bind();
	for(unsigned i=0;i<vbo->mPrimitives.Size(); i++)
	{
		FPrimitiveSky *prim = &vbo->mPrimitives[i];

		if (lasttexturetype != prim->mUseTexture)
		{
			switch (prim->mUseTexture)
			{
			case 0:	// cap of first layer
				material = GLRenderer2->GetMaterial(NULL, false, 0);
				material->Bind(NULL, TM_OPAQUE, -1, false);
				break;

			case 1:	// first layer
				material = GLRenderer2->GetMaterial(tex_render, false, 0);
				material->Bind(NULL, TM_OPAQUE, -1, false);
				break;

			case 2:	// second layer (has no cap)

				// apply the transformation for the second layer
				gl.PopMatrix();
				gl.PushMatrix();
				texh = tex_info2->GetScaledHeight();
				gl.Rotatef(-180.0f+xofs2, 0.f, 1.f, 0.f);
				if (texh>190 && SkyStretch()) texh=190;
				else if (texh<=180 && !SkyStretch()) // && skystretch)
				{
					gl.Scalef(1.f, texh/180.f, 1.f);
				}
				// now render it
				material = GLRenderer2->GetMaterial(tex_render2, false, 0);
				material->Bind(NULL, TM_MODULATE, -1, false);
				break;

			case 3:	// fog layer
				material = GLRenderer2->GetMaterial(NULL, false, 0);
				material->Bind(NULL, TM_OPAQUE, -1, false);
				break;
			}
		}
		gl.DrawArrays(prim->mType, prim->mStartVertex, prim->mVertexCount);
	}
	gl.PopMatrix();
	gl.PopMatrix();
	// if there was an y offset we need to pop off the original texture matrix.
	if (yofs != 0)
	{
		gl.MatrixMode(GL_TEXTURE);
		gl.PopMatrix();
		gl.MatrixMode(GL_MODELVIEW);
	}
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

void FSkyDrawer::RenderSky(FTextureID tex1, FTextureID tex2, PalEntry fogcolor, float xofs1, float xofs2, float yofs)
{
	// Always use the cap color from the first texture in an animation. 
	// This prevents flickering effects if the animation frames produce different cap colors
	FTexture *tex_info = TexMan[tex1];

	if (!tex_info->gl_info.bSkybox)
	{
		RenderSkyDome(tex1, tex2, fogcolor, xofs1, xofs2, yofs);
	}
	else
	{
		RenderSkyBox(tex1, xofs1, glset.skyrotatevector);
	}
}



}
