/*
** gl_wipe.cpp
** Screen wipe stuff
** (This uses immediate mode and the fixed function pipeline 
**  even if the new renderer is active)
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

#include "gl/gl_include.h"
#include "files.h"
#include "f_wipe.h"
#include "m_random.h"
#include "w_wad.h"
#include "v_palette.h"
#include "templates.h"
#include "gl/gl_framebuffer.h"
#include "gl/common/glc_renderer.h"
#include "gl/common/glc_templates.h"
#include "gl/common/glc_translate.h"
#include "gl/textures/gl_hwtexture.h"	// uses this for convenience
#include "vectors.h"

EXTERN_CVAR(Bool, gl_vid_compatibility)

//===========================================================================
// 
//	Screen wipes
//
//===========================================================================

// TYPES -------------------------------------------------------------------

class OpenGLFrameBuffer::Wiper_Crossfade : public OpenGLFrameBuffer::Wiper
{
public:
	Wiper_Crossfade();
	bool Run(int ticks, OpenGLFrameBuffer *fb);

private:
	int Clock;
};

class OpenGLFrameBuffer::Wiper_Melt : public OpenGLFrameBuffer::Wiper
{
public:
	Wiper_Melt();
	bool Run(int ticks, OpenGLFrameBuffer *fb);

private:
	static const int WIDTH = 320, HEIGHT = 200;
	int y[WIDTH];
};

class OpenGLFrameBuffer::Wiper_Burn : public OpenGLFrameBuffer::Wiper
{
public:
	Wiper_Burn();
	~Wiper_Burn();
	bool Run(int ticks, OpenGLFrameBuffer *fb);

private:
	static const int WIDTH = 64, HEIGHT = 64;
	BYTE BurnArray[WIDTH * (HEIGHT + 5)];
	FHardwareTexture *BurnTexture;
	int Density;
	int BurnTime;
};

//==========================================================================
//
// OpenGLFrameBuffer :: WipeStartScreen
//
// Called before the current screen has started rendering. This needs to
// save what was drawn the previous frame so that it can be animated into
// what gets drawn this frame.
//
//==========================================================================

bool OpenGLFrameBuffer::WipeStartScreen(int type)
{
	if (gl_vid_compatibility)
	{
		return false;	// not all required features present.
	}
	switch (type)
	{
	case wipe_Burn:
		ScreenWipe = new Wiper_Burn;
		break;

	case wipe_Fade:
		ScreenWipe = new Wiper_Crossfade;
		break;

	case wipe_Melt:
		ScreenWipe = new Wiper_Melt;
		break;

	default:
		return false;
	}

	wipestartscreen = new FHardwareTexture(Width, Height, false, false);
	wipestartscreen->CreateTexture(NULL, Width, Height, false, 0, CM_DEFAULT);
	gl.Finish();
	wipestartscreen->Bind(0, CM_DEFAULT);
	gl.CopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, Width, Height);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	return true;
}

//==========================================================================
//
// OpenGLFrameBuffer :: WipeEndScreen
//
// The screen we want to animate to has just been drawn.
//
//==========================================================================

void OpenGLFrameBuffer::WipeEndScreen()
{
	wipeendscreen = new FHardwareTexture(Width, Height, false, false);
	wipeendscreen->CreateTexture(NULL, Width, Height, false, 0, CM_DEFAULT);
	gl.Flush();
	wipeendscreen->Bind(0, CM_DEFAULT);
	gl.CopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, Width, Height);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	Unlock();
}

//==========================================================================
//
// OpenGLFrameBuffer :: WipeDo
//
// Perform the actual wipe animation. The number of tics since the last
// time this function was called is passed in. Returns true when the wipe
// is over. The first time this function has been called, the screen is
// still locked from before and EndScene() still has not been called.
// Successive times need to call BeginScene().
//
//==========================================================================

bool OpenGLFrameBuffer::WipeDo(int ticks)
{
	// Sanity checks.
	if (wipestartscreen == NULL || wipeendscreen == NULL)
	{
		return true;
	}
	// This code does all rendering itself so the renderer object needs to switch off
	// all its current settings
	GLRenderer->SetPaused();
	bool done = ScreenWipe->Run(ticks, this);
	//DrawLetterbox();
	return done;
}

//==========================================================================
//
// OpenGLFrameBuffer :: WipeCleanup
//
// Release any resources that were specifically created for the wipe.
//
//==========================================================================

void OpenGLFrameBuffer::WipeCleanup()
{
	if (ScreenWipe != NULL)
	{
		delete ScreenWipe;
		ScreenWipe = NULL;
	}
	if (wipestartscreen != NULL)
	{
		delete wipestartscreen;
		wipestartscreen = NULL;
	}
	if (wipeendscreen != NULL)
	{
		delete wipeendscreen;
		wipeendscreen = NULL;
	}
	GLRenderer->UnsetPaused();
}

//==========================================================================
//
// OpenGLFrameBuffer :: Wiper Constructor
//
//==========================================================================

OpenGLFrameBuffer::Wiper::~Wiper()
{
}

// WIPE: CROSSFADE ---------------------------------------------------------

//==========================================================================
//
// OpenGLFrameBuffer :: Wiper_Crossfade Constructor
//
//==========================================================================

OpenGLFrameBuffer::Wiper_Crossfade::Wiper_Crossfade()
: Clock(0)
{
}

//==========================================================================
//
// OpenGLFrameBuffer :: Wiper_Crossfade :: Run
//
// Fades the old screen into the new one over 32 ticks.
//
//==========================================================================

bool OpenGLFrameBuffer::Wiper_Crossfade::Run(int ticks, OpenGLFrameBuffer *fb)
{
	Clock += ticks;

	gl.SetTextureMode(TM_OPAQUE);
	gl.Disable(GL_ALPHA_TEST);
	fb->wipestartscreen->Bind(0, CM_DEFAULT);
	gl.Color4f(1.f, 1.f, 1.f, 1.f);
	gl.Begin(GL_TRIANGLE_STRIP);
	gl.TexCoord2f(0, fb->wipestartscreen->GetVB());
	gl.Vertex2i(0, 0);
	gl.TexCoord2f(0, 0);
	gl.Vertex2i(0, fb->Height);
	gl.TexCoord2f(fb->wipestartscreen->GetUR(), fb->wipestartscreen->GetVB());
	gl.Vertex2i(fb->Width, 0);
	gl.TexCoord2f(fb->wipestartscreen->GetUR(), 0);
	gl.Vertex2i(fb->Width, fb->Height);
	gl.End();

	fb->wipeendscreen->Bind(0, CM_DEFAULT);
	gl.Color4f(1.f, 1.f, 1.f, clamp(Clock/32.f, 0.f, 1.f));
	gl.Begin(GL_TRIANGLE_STRIP);
	gl.TexCoord2f(0, fb->wipestartscreen->GetVB());
	gl.Vertex2i(0, 0);
	gl.TexCoord2f(0, 0);
	gl.Vertex2i(0, fb->Height);
	gl.TexCoord2f(fb->wipestartscreen->GetUR(), fb->wipestartscreen->GetVB());
	gl.Vertex2i(fb->Width, 0);
	gl.TexCoord2f(fb->wipestartscreen->GetUR(), 0);
	gl.Vertex2i(fb->Width, fb->Height);
	gl.End();

	return Clock >= 32;
}

//==========================================================================
//
// OpenGLFrameBuffer :: Wiper_Melt Constructor
//
//==========================================================================

OpenGLFrameBuffer::Wiper_Melt::Wiper_Melt()
{
	int i, r;

	// setup initial column positions
	// (y<0 => not ready to scroll yet)
	y[0] = -(M_Random() & 15);
	for (i = 1; i < WIDTH; ++i)
	{
		r = (M_Random()%3) - 1;
		y[i] = clamp(y[i-1] + r, -15, 0);
	}
}

//==========================================================================
//
// OpenGLFrameBuffer :: Wiper_Melt :: Run
//
// Fades the old screen into the new one over 32 ticks.
//
//==========================================================================

bool OpenGLFrameBuffer::Wiper_Melt::Run(int ticks, OpenGLFrameBuffer *fb)
{

	// Draw the new screen on the bottom.
	gl.SetTextureMode(TM_OPAQUE);
	fb->wipeendscreen->Bind(0, CM_DEFAULT);
	gl.Color4f(1.f, 1.f, 1.f, 1.f);
	gl.Begin(GL_TRIANGLE_STRIP);
	gl.TexCoord2f(0, fb->wipestartscreen->GetVB());
	gl.Vertex2i(0, 0);
	gl.TexCoord2f(0, 0);
	gl.Vertex2i(0, fb->Height);
	gl.TexCoord2f(fb->wipestartscreen->GetUR(), fb->wipestartscreen->GetVB());
	gl.Vertex2i(fb->Width, 0);
	gl.TexCoord2f(fb->wipestartscreen->GetUR(), 0);
	gl.Vertex2i(fb->Width, fb->Height);
	gl.End();

	int i, dy;
	bool done = false;

	fb->wipestartscreen->Bind(0, CM_DEFAULT);
	// Copy the old screen in vertical strips on top of the new one.
	while (ticks--)
	{
		done = true;
		for (i = 0; i < WIDTH; i++)
		{
			if (y[i] < 0)
			{
				y[i]++;
				done = false;
			} 
			else if (y[i] < HEIGHT)
			{
				dy = (y[i] < 16) ? y[i]+1 : 8;
				y[i] = MIN(y[i] + dy, HEIGHT);
				done = false;
			}
			if (ticks == 0)
			{ // Only draw for the final tick.
				RECT rect;
				POINT dpt;

				dpt.x = i * fb->Width / WIDTH;
				dpt.y = MAX(0, y[i] * fb->Height / HEIGHT);
				rect.left = dpt.x;
				rect.top = 0;
				rect.right = (i + 1) * fb->Width / WIDTH;
				rect.bottom = fb->Height - dpt.y;
				if (rect.bottom > rect.top)
				{
					rect.bottom = fb->Height - rect.bottom;
					rect.top = fb->Height - rect.top;
					gl.Color4f(1.f, 1.f, 1.f, 1.f);
					gl.Begin(GL_TRIANGLE_STRIP);
					gl.TexCoord2f(fb->wipestartscreen->GetU(rect.left), fb->wipestartscreen->GetV(rect.top));
					gl.Vertex2i(rect.left, rect.bottom);
					gl.TexCoord2f(fb->wipestartscreen->GetU(rect.left), fb->wipestartscreen->GetV(rect.bottom));
					gl.Vertex2i(rect.left, rect.top);
					gl.TexCoord2f(fb->wipestartscreen->GetU(rect.right), fb->wipestartscreen->GetV(rect.top));
					gl.Vertex2i(rect.right, rect.bottom);
					gl.TexCoord2f(fb->wipestartscreen->GetU(rect.right), fb->wipestartscreen->GetV(rect.bottom));
					gl.Vertex2i(rect.right, rect.top);
					gl.End();
				}
			}
		}
	}
	gl.SetTextureMode(TM_MODULATE);
	return done;
}

//==========================================================================
//
// OpenGLFrameBuffer :: Wiper_Burn Constructor
//
//==========================================================================

OpenGLFrameBuffer::Wiper_Burn::Wiper_Burn()
{
	Density = 4;
	BurnTime = 0;
	memset(BurnArray, 0, sizeof(BurnArray));
	BurnTexture = NULL;
}

//==========================================================================
//
// OpenGLFrameBuffer :: Wiper_Burn Destructor
//
//==========================================================================

OpenGLFrameBuffer::Wiper_Burn::~Wiper_Burn()
{
	if (BurnTexture != NULL)
	{
		delete BurnTexture;
		BurnTexture = NULL;
	}
}

//==========================================================================
//
// OpenGLFrameBuffer :: Wiper_Burn :: Run
//
//==========================================================================

bool OpenGLFrameBuffer::Wiper_Burn::Run(int ticks, OpenGLFrameBuffer *fb)
{
	bool done;

	BurnTime += ticks;
	ticks *= 2;

	// Make the fire burn
	done = false;
	while (!done && ticks--)
	{
		Density = wipe_CalcBurn(BurnArray, WIDTH, HEIGHT, Density);
		done = (Density < 0);
	}

	if (BurnTexture != NULL) delete BurnTexture;
	BurnTexture = new FHardwareTexture(WIDTH, HEIGHT, false, false);

	// Update the burn texture with the new burn data
	BYTE rgb_buffer[WIDTH*HEIGHT*4];

	const BYTE *src = BurnArray;
	DWORD *dest = (DWORD *)rgb_buffer;
	for (int y = HEIGHT; y != 0; --y)
	{
		for (int x = WIDTH; x != 0; --x)
		{
			BYTE s = clamp<int>((*src++)*2, 0, 255);
			*dest++ = MAKEARGB(s,255,255,255);
		}
	}

	// Put the initial screen back to the buffer.
	gl.SetTextureMode(TM_OPAQUE);
	gl.Disable(GL_ALPHA_TEST);
	fb->wipestartscreen->Bind(0, CM_DEFAULT);
	gl.Color4f(1.f, 1.f, 1.f, 1.f);
	gl.Begin(GL_TRIANGLE_STRIP);
	gl.TexCoord2f(0, fb->wipestartscreen->GetVB());
	gl.Vertex2i(0, 0);
	gl.TexCoord2f(0, 0);
	gl.Vertex2i(0, fb->Height);
	gl.TexCoord2f(fb->wipestartscreen->GetUR(), fb->wipestartscreen->GetVB());
	gl.Vertex2i(fb->Width, 0);
	gl.TexCoord2f(fb->wipestartscreen->GetUR(), 0);
	gl.Vertex2i(fb->Width, fb->Height);
	gl.End();

	gl.SetTextureMode(TM_MODULATE);
	gl.ActiveTexture(GL_TEXTURE1);
	gl.Enable(GL_TEXTURE_2D);

	// mask out the alpha channel of the wipeendscreen.
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
	glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_REPLACE);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE1);
	glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
	glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE); 
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PREVIOUS);
	glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);

	gl.ActiveTexture(GL_TEXTURE0);

	// Burn the new screen on top of it.
	gl.Color4f(1.f, 1.f, 1.f, 1.f);
	fb->wipeendscreen->Bind(1, CM_DEFAULT);
	//BurnTexture->Bind(0, CM_DEFAULT);

	BurnTexture->CreateTexture(rgb_buffer, WIDTH, HEIGHT, false, 0, CM_DEFAULT);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);


	gl.Begin(GL_TRIANGLE_STRIP);
	gl.MultiTexCoord2f(GL_TEXTURE0, 0, 0);
	gl.MultiTexCoord2f(GL_TEXTURE1, 0, fb->wipestartscreen->GetVB());
	gl.Vertex2i(0, 0);
	gl.MultiTexCoord2f(GL_TEXTURE0, 0, BurnTexture->GetVB());
	gl.MultiTexCoord2f(GL_TEXTURE1, 0, 0);
	gl.Vertex2i(0, fb->Height);
	gl.MultiTexCoord2f(GL_TEXTURE0, BurnTexture->GetUR(), 0);
	gl.MultiTexCoord2f(GL_TEXTURE1, fb->wipestartscreen->GetUR(), fb->wipestartscreen->GetVB());
	gl.Vertex2i(fb->Width, 0);
	gl.MultiTexCoord2f(GL_TEXTURE0, BurnTexture->GetUR(), BurnTexture->GetVB());
	gl.MultiTexCoord2f(GL_TEXTURE1, fb->wipestartscreen->GetUR(), 0);
	gl.Vertex2i(fb->Width, fb->Height);
	gl.End();

	gl.ActiveTexture(GL_TEXTURE1);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	gl.Disable(GL_TEXTURE_2D);
	gl.ActiveTexture(GL_TEXTURE0);

	// The fire may not always stabilize, so the wipe is forced to end
	// after an arbitrary maximum time.
	return done || (BurnTime > 40);
}
