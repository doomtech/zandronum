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
#include "gl/gl_framebuffer.h"
#include "gl/r_render/r_render.h"
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
	mCurrentVertexBufferSize = BUFFER_INCREMENT;
	mVertexBuffer = new FVertexBuffer2D(BUFFER_INCREMENT);
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
			mCurrentVertexBufferSize += BUFFER_INCREMENT;
	}

	if (mCurrentVertexIndex == 0)
	{
		mVertexBuffer->Bind();
		mVertexBuffer->Map();
	}
	
	vertptr = mVertexBuffer->GetVertexPointer(mCurrentVertexIndex);
	mCurrentVertexIndex += numvertices;

	int vtindex = mPrimitives.Reserve(1);
	FPrimitive2D *prim = &mPrimitives[vtindex];
	prim->mPrimitiveType = -1;	// make it invalid
	return vtindex;
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


}
