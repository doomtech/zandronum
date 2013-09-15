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
// Vertex and vertex buffer classes
//

#include "gl/gl_include.h"
#include "r_main.h"
#include "v_palette.h"
#include "g_level.h"
#include "gl/gl_intern.h"
#include "gl/gl_framebuffer.h"
#include "gl/r_render/r_render.h"
#include "gl/common/glc_data.h"
#include "gl/new_renderer/gl2_renderer.h"
#include "gl/new_renderer/gl2_vertex.h"
#include "gl/new_renderer/textures/gl2_shader.h"
#include "gl/new_renderer/textures/gl2_material.h"

namespace GLRendererNew
{
unsigned int FVertexBuffer::mLastBound;

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

FVertexBuffer::FVertexBuffer()
{
	gl.GenBuffers(1, &mBufferId);
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

FVertexBuffer::~FVertexBuffer()
{
	if (mLastBound == mBufferId)
	{
		mLastBound = 0;
		gl.BindBuffer(GL_ARRAY_BUFFER, 0);
	}

	gl.DeleteBuffers(1, &mBufferId);
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

void FVertexBuffer::BindBuffer()
{
	if (mLastBound != mBufferId)
	{
		mLastBound = mBufferId;
		gl.BindBuffer(GL_ARRAY_BUFFER, mBufferId);
	}
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

void FVertexBuffer::Map()
{
	BindBuffer();
	mMap = gl.MapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

bool FVertexBuffer::Unmap()
{
	BindBuffer();
	mMap = NULL;
	return !!gl.UnmapBuffer(GL_ARRAY_BUFFER);
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

FVertexBuffer2D::FVertexBuffer2D(int size)
{
	mMaxSize = size;
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------
#define VT2 ((FVertex2D*)NULL)

bool FVertexBuffer2D::Bind()
{
	BindBuffer();
	gl.BufferData(GL_ARRAY_BUFFER, mMaxSize * sizeof(FVertex2D), NULL, GL_STREAM_DRAW);
	glVertexPointer(2,GL_FLOAT, sizeof(FVertex2D), &VT2->x);
	glTexCoordPointer(2,GL_FLOAT, sizeof(FVertex2D), &VT2->u);
	glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(FVertex2D), &VT2->r);

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

void FPrimitive2D::Draw()
{
	if (mMaterial != NULL)
	{
		mMaterial->Bind(NULL, mTextureMode, -1, -1);
	}
	else
	{
		GLRenderer2->mShaders->SetActiveShader(NULL);
	}

	gl.BlendEquation(mBlendEquation);
	gl.BlendFunc(mSrcBlend, mDstBlend);
	gl.AlphaFunc(GL_GEQUAL, mAlphaThreshold);

	if (mUseScissor)
	{
		// scissor test doesn't use the current viewport for the coordinates, so use real screen coordinates
		int btm = (SCREENHEIGHT - screen->GetHeight()) / 2;
		btm = SCREENHEIGHT - btm;

		gl.Enable(GL_SCISSOR_TEST);
		int space = (static_cast<OpenGLFrameBuffer*>(screen)->GetTrueHeight()-screen->GetHeight())/2;
		gl.Scissor(mScissor[0], btm - mScissor[3] + space, mScissor[2] - mScissor[0], mScissor[3] - mScissor[1]);
	}

	gl.DrawArrays(mPrimitiveType, mVertexStart, mVertexCount); 

	if (mUseScissor)
	{
		gl.Scissor(0, 0, screen->GetWidth(), screen->GetHeight());
		gl.Disable(GL_SCISSOR_TEST);
	}
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

FPrimitiveBuffer2D::FPrimitiveBuffer2D()
{
	mCurrentVertexIndex = 0;
	mCurrentVertexBufferSize = BUFFER_START;
	mVertexBuffer = new FVertexBuffer2D(BUFFER_START);
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

FPrimitiveBuffer2D::~FPrimitiveBuffer2D()
{
	delete mVertexBuffer;
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

int FPrimitiveBuffer2D::NewPrimitive(int numvertices, FPrimitive2D *&primptr, FVertex2D *&vertptr)
{
	if (mCurrentVertexIndex + numvertices >= mCurrentVertexBufferSize)
	{
		// The vertex buffer is full. We have to flush all accumulated primitives,
		// resize the buffer and continue.

		Flush();
		if (mCurrentVertexBufferSize < BUFFER_MAXIMUM)
		{
			mCurrentVertexBufferSize += BUFFER_INCREMENT;
			mVertexBuffer->ChangeSize(mCurrentVertexBufferSize);
		}
	}

	if (mCurrentVertexIndex == 0)
	{
		mVertexBuffer->Bind();
		mVertexBuffer->Map();
	}

	int primindex = mPrimitives.Reserve(1);
	primptr = &mPrimitives[primindex];
	primptr->mPrimitiveType = -1;	// make it invalid
	primptr->mVertexStart = mCurrentVertexIndex;
	primptr->mVertexCount = numvertices;
	vertptr = mVertexBuffer->GetVertexPointer(mCurrentVertexIndex);

	mCurrentVertexIndex += numvertices;

	return primindex;
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

bool FPrimitiveBuffer2D::CheckPrimitive(int type, int newvertexcount, FVertex2D *&vertptr)
{
	int primindex = mPrimitives.Size()-1;
	FPrimitive2D *primptr = &mPrimitives[primindex];

	if (primptr->mPrimitiveType == type)
	{
		primptr->mVertexCount += newvertexcount;
		vertptr = mVertexBuffer->GetVertexPointer(mCurrentVertexIndex);
		mCurrentVertexIndex += newvertexcount;
		return true;
	}
	return false;
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

void FPrimitiveBuffer2D::Flush()
{
	if (mPrimitives.Size() > 0)
	{
		mVertexBuffer->Unmap();
		// draw the primitives;
		for(unsigned i = 0; i < mPrimitives.Size(); i++)
		{
			mPrimitives[i].Draw();
		}
		mPrimitives.Clear();
		mCurrentVertexIndex = 0;
	}
}




//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

void FPrimitive3D::SetRenderStyle(FRenderStyle style, bool opaque, bool allowcolorblending)
{
	gl_GetRenderStyle(style, opaque, allowcolorblending, 
		&mTextureMode, &mSrcBlend, &mDstBlend, &mBlendEquation);
}

//----------------------------------------------------------------------------
//
// copies an engine primitive to the render list (also translates texture animations)
//
//----------------------------------------------------------------------------

void FPrimitive3D::Copy(FPrimitive3D *other)
{
	// We must re-get the material every frame to handle animated textures properly
	FTexture *tex = TexMan.ByIndexTranslated(other->mTexId);

	mPrimitiveType = other->mPrimitiveType;
	mMaterial = GLRenderer2->GetMaterial(tex, false, 0);
	mClamp = other->mClamp;
	mTextureMode = other->mTextureMode;
	mDesaturation = other->mDesaturation;
	mAlphaThreshold = other->mAlphaThreshold;
	mTranslucent = other->mTranslucent;
	mSrcBlend = other->mSrcBlend;
	mDstBlend = other->mDstBlend;
	mBlendEquation = other->mBlendEquation;
	mCopy = false;
}

//----------------------------------------------------------------------------
//
// Sets light and fog color for this vertex
//
//----------------------------------------------------------------------------

void FVertex3D::SetLighting(int lightlevel, FDynamicColormap *cm, int rellight, bool additive, FTexture *tex)
{
	PalEntry fogcolor;
	int llevel = gl_CalcLightLevel(lightlevel, rellight, false);

	if (!tex->gl_info.bFullbright)
	{
		if (rellight < 0) rellight = 0;
		else rellight += extralight * gl_weaponlight;

		PalEntry pe = gl_CalcLightColor(llevel, cm->Color, cm->Color.a);
		r = pe.r;
		g = pe.g;
		b = pe.b;
	}
	else
	{
		r = g = b = 255;
	}

	if (level.flags&LEVEL_HASFADETABLE)
	{
		fogdensity=70;
		fogcolor=0x808080;
	}
	else
	{
		fogcolor = cm->Fade;
		fogdensity = gl_GetFogDensity(lightlevel, fogcolor);
		fogcolor.a = 0;
	}

	// Make fog a little denser when inside a skybox
	//if (GLPortal::inskybox) fogdensity+=fogdensity/2;


	if (fogdensity==0 || gl_fogmode == 0)
	{
		// disable fog completely
		fon = FOG_NONE;
	}
	else
	{
		fogdensity *= -(1.442692f / 64000.f);	// * -1/log(2) - can be done here better than in the shader.
		if (glset.lightmode == 2 && fogcolor == 0)
		{
			const float MAXDIST = 256.f;
			const float THRESHOLD = 96.f;
			const float FACTOR = 0.75f;

			if (lightlevel < THRESHOLD)
			{
				lightdist = lightlevel * MAXDIST / THRESHOLD;
				lightlevel = THRESHOLD;
			}
			else lightdist = MAXDIST;

			lightfactor = ((float)lightlevel/llevel);
			lightfactor = 1.f + (lightfactor - 1.f) * FACTOR;
		}
		else
		{
			lightfactor = 1;
			lightdist = 0;
		}

		if (!additive)
		{
			fr = fogcolor.r;
			fg = fogcolor.g;
			fb = fogcolor.b;
		}
		else
		{
			// For additive rendering using the regular fog color here would mean applying it twice
			// so always use black but treat it as fog by the shader, not diminishing light
			fr = fg = fb = 0;
		}
		fon = fogcolor != 0? FOG_COLOR : FOG_BLACK;
	}

}


//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

FVertexBuffer3D::FVertexBuffer3D(int size)
{
	mMaxSize = size;
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------
#define VT3 ((FVertex3D*)NULL)

bool FVertexBuffer3D::Bind()
{
	BindBuffer();
	gl.BufferData(GL_ARRAY_BUFFER, mMaxSize * sizeof(FVertex3D), NULL, GL_STREAM_DRAW);

	glVertexPointer(3,GL_FLOAT, sizeof(FVertex3D), &VT3->x);
	glTexCoordPointer(2,GL_FLOAT, sizeof(FVertex3D), &VT3->u);
	glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(FVertex3D), &VT3->r);
	gl.VertexAttribPointer(FShaderObject::attrFogColor, 4, GL_UNSIGNED_BYTE, true, sizeof(FVertex3D), &VT3->fr);
	gl.VertexAttribPointer(FShaderObject::attrGlowTopColor, 4, GL_UNSIGNED_BYTE, true, sizeof(FVertex3D), &VT3->tr);
	gl.VertexAttribPointer(FShaderObject::attrGlowBottomColor, 4, GL_UNSIGNED_BYTE, true, sizeof(FVertex3D), &VT3->br);
	gl.VertexAttribPointer(FShaderObject::attrGlowDistance, 2, GL_FLOAT, true, sizeof(FVertex3D), &VT3->glowdisttop);
	gl.VertexAttribPointer(FShaderObject::attrLightParams, 3, GL_FLOAT, true, sizeof(FVertex3D), &VT3->fogdensity);

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);
	gl.EnableVertexAttribArray(FShaderObject::attrLightParams);
	gl.EnableVertexAttribArray(FShaderObject::attrFogColor);
	gl.EnableVertexAttribArray(FShaderObject::attrGlowDistance);
	gl.EnableVertexAttribArray(FShaderObject::attrGlowTopColor);
	gl.EnableVertexAttribArray(FShaderObject::attrGlowBottomColor);
	return true;
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

void FPrimitive3D::Draw()
{
	if (!mCopy)
	{
		if (mMaterial != NULL)
		{
			mMaterial->Bind(NULL, mTextureMode, mDesaturation, mClamp);
		}
		else
		{
			GLRenderer2->mShaders->SetActiveShader(NULL);
		}

		gl.BlendEquation(mBlendEquation);
		gl.BlendFunc(mSrcBlend, mDstBlend);
		gl.AlphaFunc(GL_GEQUAL, mAlphaThreshold);
	}


	gl.DrawArrays(mPrimitiveType, mVertexStart, mVertexCount); 
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

FPrimitiveBuffer3D::FPrimitiveBuffer3D()
{
	mCurrentVertexIndex = 0;
	mCurrentVertexBufferSize = BUFFER_START;
	mVertexBuffer = new FVertexBuffer3D(BUFFER_START);
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

FPrimitiveBuffer3D::~FPrimitiveBuffer3D()
{
	delete mVertexBuffer;
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

int FPrimitiveBuffer3D::NewPrimitive(int numvertices, FPrimitive3D *&primptr, FVertex3D *&vertptr)
{
	if (mCurrentVertexIndex + numvertices >= mCurrentVertexBufferSize)
	{
		// The vertex buffer is full. We have to flush all accumulated primitives,
		// resize the buffer and continue.

		Flush();
		if (mCurrentVertexBufferSize < BUFFER_MAXIMUM)
		{
			mCurrentVertexBufferSize += BUFFER_INCREMENT;
			mVertexBuffer->ChangeSize(mCurrentVertexBufferSize);
		}
	}

	if (mCurrentVertexIndex == 0)
	{
		mVertexBuffer->Bind();
		mVertexBuffer->Map();
	}

	int primindex = mPrimitives.Reserve(1);
	primptr = &mPrimitives[primindex];
	primptr->mPrimitiveType = -1;	// make it invalid
	primptr->mVertexStart = mCurrentVertexIndex;
	primptr->mVertexCount = numvertices;
	vertptr = mVertexBuffer->GetVertexPointer(mCurrentVertexIndex);

	mCurrentVertexIndex += numvertices;

	return primindex;
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

bool FPrimitiveBuffer3D::CheckPrimitive(int type, int newvertexcount, FVertex3D *&vertptr)
{
	int primindex = mPrimitives.Size()-1;
	FPrimitive3D *primptr = &mPrimitives[primindex];

	if (primptr->mPrimitiveType == type)
	{
		primptr->mVertexCount += newvertexcount;
		vertptr = mVertexBuffer->GetVertexPointer(mCurrentVertexIndex);
		mCurrentVertexIndex += newvertexcount;
		return true;
	}
	return false;
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

void FPrimitiveBuffer3D::Flush()
{
	if (mPrimitives.Size() > 0)
	{
		mVertexBuffer->Unmap();
		// draw the primitives;
		for(unsigned i = 0; i < mPrimitives.Size(); i++)
		{
			mPrimitives[i].Draw();
		}
		mPrimitives.Clear();
		mCurrentVertexIndex = 0;
	}
}






}
