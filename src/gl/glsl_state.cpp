/*
** glsl_state.cpp
** GLSL state maintenance
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

#include "gl/glsl_state.h"

static GLSLRenderState _glsl;
GLSLRenderState *glsl = &_glsl;

void GLSLRenderState::SetFixedColormap(int cm)
{
}

void GLSLRenderState::SetLight(int light, int rellight, FColormap *cm, float alpha, bool additive)
{
}

void GLSLRenderState::SetLight(int light, int rellight, FColormap *cm, float alpha, bool additive, PalEntry ThingColor)
{
}

void GLSLRenderState::SetAddLight(float *f)
{
}

void GLSLRenderState::SetLightAbsolute(float r, float g, float b, float a)
{
}

void GLSLRenderState::SetWarp(int mode, float speed)
{
}

void GLSLRenderState::SetBrightmap(bool on)
{
}

void GLSLRenderState::EnableBrightmap(bool on)
{
}

void GLSLRenderState::EnableTexture(bool on)
{
}

void GLSLRenderState::EnableFogBoundary(bool on)
{
}

void GLSLRenderState::SetFloorGlow(PalEntry color, float height)
{
}

void GLSLRenderState::SetCeilingGlow(PalEntry color, float height)
{
}

void GLSLRenderState::ClearDynLights()
{
}

void GLSLRenderState::SetDynLight(int index, float x, float y, float z, float size, PalEntry color, int mode)
{
}

void GLSLRenderState::SetLightingMode(int mode)
{
}

void GLSLRenderState::SetAlphaThreshold(float thresh)
{
}

void GLSLRenderState::SetBlend(int src, int dst)
{
}

void GLSLRenderState::SetTextureMode(int mode)
{
}

void GLSLRenderState::Apply()
{
}

void GLSLRenderState::Enable(bool on)
{
}

void GLSLRenderState::Reset()
{
}



