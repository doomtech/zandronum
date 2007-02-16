
// HEADER FILES ------------------------------------------------------------

#include "doomtype.h"

#include "templates.h"
#include "i_system.h"
#include "i_video.h"
#include "v_video.h"
#include "v_pfx.h"
#include "stats.h"

#include "sdlvideo.h"

#include <SDL.h>

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

class SDLFB : public DFrameBuffer
{
public:
	SDLFB (int width, int height, bool fullscreen);
	~SDLFB ();

	bool Lock (bool buffer);
	void Unlock ();
	bool Relock ();
	void ForceBuffering (bool force);
	bool IsValid ();
	void Update ();
	PalEntry *GetPalette ();
	void GetFlashedPalette (PalEntry pal[256]);
	void UpdatePalette ();
	bool SetGamma (float gamma);
	bool SetFlash (PalEntry rgb, int amount);
	void GetFlash (PalEntry &rgb, int &amount);
	int GetPageCount ();
	bool IsFullscreen ();

	friend class SDLVideo;

private:
	PalEntry SourcePalette[256];
	BYTE GammaTable[3][256];
	PalEntry Flash;
	int FlashAmount;
	float Gamma;
	bool UpdatePending;
	
	SDL_Surface *Screen;
	
	bool NeedPalUpdate;
	bool NeedGammaUpdate;
	bool NotPaletted;
	
	void UpdateColors ();
};

struct MiniModeInfo
{
	WORD Width, Height;
};

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void DoBlending (const PalEntry *from, PalEntry *to, int count, int r, int g, int b, int a);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern IVideo *Video;

EXTERN_CVAR (Float, Gamma)

// PUBLIC DATA DEFINITIONS -------------------------------------------------

CVAR (Int, vid_displaybits, 8, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

CUSTOM_CVAR (Float, rgamma, 1.f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (screen != NULL)
	{
		screen->SetGamma (Gamma);
	}
}
CUSTOM_CVAR (Float, ggamma, 1.f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (screen != NULL)
	{
		screen->SetGamma (Gamma);
	}
}
CUSTOM_CVAR (Float, bgamma, 1.f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (screen != NULL)
	{
		screen->SetGamma (Gamma);
	}
}

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// Dummy screen sizes to pass when windowed
static MiniModeInfo WinModes[] =
{
	{ 320, 200 },
	{ 320, 240 },
	{ 400, 300 },
	{ 480, 360 },
	{ 512, 384 },
	{ 640, 400 },
	{ 640, 480 },
	{ 720, 540 },
	{ 800, 600 },
	{ 960, 720 },
	{ 1024, 768 },
	{ 1152, 864 },
	{ 1280, 960 }
};

static cycle_t BlitCycles;
static cycle_t SDLFlipCycles;

// CODE --------------------------------------------------------------------

SDLVideo::SDLVideo (int parm)
{
	IteratorBits = 0;
	IteratorFS = false;
}

SDLVideo::~SDLVideo ()
{
}

// This only changes how the iterator lists modes
bool SDLVideo::FullscreenChanged (bool fs)
{
	IteratorFS = fs;
	return true;
}

void SDLVideo::StartModeIterator (int bits)
{
	IteratorMode = 0;
	IteratorBits = bits;
}

bool SDLVideo::NextMode (int *width, int *height, bool *letterbox)
{
	if (IteratorBits != 8)
		return false;
	
	if (!IteratorFS)
	{
		if ((unsigned)IteratorMode < sizeof(WinModes)/sizeof(WinModes[0]))
		{
			*width = WinModes[IteratorMode].Width;
			*height = WinModes[IteratorMode].Height;
			++IteratorMode;
			return true;
		}
	}
	else
	{
		SDL_Rect **modes = SDL_ListModes (NULL, SDL_FULLSCREEN|SDL_HWSURFACE);
		if (modes != NULL && modes[IteratorMode] != NULL)
		{
			*width = modes[IteratorMode]->w;
			*height = modes[IteratorMode]->h;
			++IteratorMode;
			return true;
		}
	}
	return false;
}

DFrameBuffer *SDLVideo::CreateFrameBuffer (int width, int height, bool fullscreen, DFrameBuffer *old)
{
	static int retry = 0;
	static int owidth, oheight;
	
	PalEntry flashColor;
	int flashAmount;

	if (old != NULL)
	{ // Reuse the old framebuffer if its attributes are the same
		SDLFB *fb = static_cast<SDLFB *> (old);
		if (fb->Width == width &&
			fb->Height == height)
		{
			bool fsnow = (fb->Screen->flags & SDL_FULLSCREEN) != 0;
	
			if (fsnow != fullscreen)
			{
				SDL_WM_ToggleFullScreen (fb->Screen);
			}
			return old;
		}
		old->GetFlash (flashColor, flashAmount);
		delete old;
	}
	else
	{
		flashColor = 0;
		flashAmount = 0;
	}
	
	SDLFB *fb = new SDLFB (width, height, fullscreen);
	retry = 0;
	
	// If we could not create the framebuffer, try again with slightly
	// different parameters in this order:
	// 1. Try with the closest size
	// 2. Try in the opposite screen mode with the original size
	// 3. Try in the opposite screen mode with the closest size
	// This is a somewhat confusing mass of recursion here.

	while (fb == NULL || !fb->IsValid ())
	{
		if (fb != NULL)
		{
			delete fb;
		}

		switch (retry)
		{
		case 0:
			owidth = width;
			oheight = height;
		case 2:
			// Try a different resolution. Hopefully that will work.
			I_ClosestResolution (&width, &height, 8);
			break;

		case 1:
			// Try changing fullscreen mode. Maybe that will work.
			width = owidth;
			height = oheight;
			fullscreen = !fullscreen;
			break;

		default:
			// I give up!
			I_FatalError ("Could not create new screen (%d x %d)", owidth, oheight);
		}

		++retry;
		fb = static_cast<SDLFB *>(CreateFrameBuffer (width, height, fullscreen, NULL));
	}

	if (fb->IsFullscreen() != fullscreen)
	{
		Video->FullscreenChanged (!fullscreen);
	}

	fb->SetFlash (flashColor, flashAmount);

	return fb;
}

void SDLVideo::SetWindowedScale (float scale)
{
}

// FrameBuffer implementation -----------------------------------------------

SDLFB::SDLFB (int width, int height, bool fullscreen)
	: DFrameBuffer (width, height)
{
	int i;
	
	NeedPalUpdate = false;
	NeedGammaUpdate = false;
	UpdatePending = false;
	NotPaletted = false;
	FlashAmount = 0;
	
	Screen = SDL_SetVideoMode (width, height, vid_displaybits,
		SDL_HWSURFACE|SDL_HWPALETTE|SDL_DOUBLEBUF|SDL_ANYFORMAT|
		(fullscreen ? SDL_FULLSCREEN : 0));

	if (Screen == NULL)
		return;

	for (i = 0; i < 256; i++)
	{
		GammaTable[0][i] = GammaTable[1][i] = GammaTable[2][i] = i;
	}
	if (Screen->format->palette == NULL)
	{
		NotPaletted = true;
		GPfx.SetFormat (Screen->format->BitsPerPixel,
			Screen->format->Rmask,
			Screen->format->Gmask,
			Screen->format->Bmask);
	}
	memcpy (SourcePalette, GPalette.BaseColors, sizeof(PalEntry)*256);
	UpdateColors ();
}

SDLFB::~SDLFB ()
{
}

bool SDLFB::IsValid ()
{
	return DFrameBuffer::IsValid() && Screen != NULL;
}

int SDLFB::GetPageCount ()
{
	return 1;
}

bool SDLFB::Lock (bool buffered)
{
	return DSimpleCanvas::Lock ();
}

bool SDLFB::Relock ()
{
	return DSimpleCanvas::Lock ();
}

void SDLFB::Unlock ()
{
	if (UpdatePending && LockCount == 1)
	{
		Update ();
	}
	else if (--LockCount <= 0)
	{
		Buffer = NULL;
		LockCount = 0;
	}
}

void SDLFB::Update ()
{
	if (LockCount != 1)
	{
		if (LockCount > 0)
		{
			UpdatePending = true;
			--LockCount;
		}
		return;
	}

	DrawRateStuff ();

	Buffer = NULL;
	LockCount = 0;
	UpdatePending = false;

	BlitCycles = 0;
	SDLFlipCycles = 0;
	clock (BlitCycles);

	if (SDL_LockSurface (Screen) == -1)
		return;

	if (NotPaletted)
	{
		GPfx.Convert (MemBuffer, Pitch,
			Screen->pixels, Screen->pitch, Width, Height,
			FRACUNIT, FRACUNIT, 0, 0);
	}
	else
	{
		if (Screen->pitch == Pitch)
		{
			memcpy (Screen->pixels, MemBuffer, Width*Height);
		}
		else
		{
			for (int y = 0; y < Height; ++y)
			{
				memcpy ((BYTE *)Screen->pixels+y*Screen->pitch, MemBuffer+y*Pitch, Width);
			}
		}
	}
	
	SDL_UnlockSurface (Screen);
	
	clock (SDLFlipCycles);
	SDL_Flip (Screen);
	unclock (SDLFlipCycles);

	unclock (BlitCycles);

	if (NeedGammaUpdate)
	{
		bool Windowed = false;
		NeedGammaUpdate = false;
		CalcGamma ((Windowed || rgamma == 0.f) ? Gamma : (Gamma * rgamma), GammaTable[0]);
		CalcGamma ((Windowed || ggamma == 0.f) ? Gamma : (Gamma * ggamma), GammaTable[1]);
		CalcGamma ((Windowed || bgamma == 0.f) ? Gamma : (Gamma * bgamma), GammaTable[2]);
		NeedPalUpdate = true;
	}
	
	if (NeedPalUpdate)
	{
		NeedPalUpdate = false;
		UpdateColors ();
	}
}

void SDLFB::UpdateColors ()
{
	if (NotPaletted)
	{
		PalEntry palette[256];
		
		for (int i = 0; i < 256; ++i)
		{
			palette[i].r = GammaTable[0][SourcePalette[i].r];
			palette[i].g = GammaTable[1][SourcePalette[i].g];
			palette[i].b = GammaTable[2][SourcePalette[i].b];
		}
		if (FlashAmount)
		{
			DoBlending (palette, palette,
				256, GammaTable[0][Flash.r], GammaTable[1][Flash.g], GammaTable[2][Flash.b],
				FlashAmount);
		}
		GPfx.SetPalette (palette);
	}
	else
	{
		SDL_Color colors[256];
		
		for (int i = 0; i < 256; ++i)
		{
			colors[i].r = GammaTable[0][SourcePalette[i].r];
			colors[i].g = GammaTable[1][SourcePalette[i].g];
			colors[i].b = GammaTable[2][SourcePalette[i].b];
		}
		if (FlashAmount)
		{
			DoBlending ((PalEntry *)colors, (PalEntry *)colors,
				256, GammaTable[2][Flash.b], GammaTable[1][Flash.g], GammaTable[0][Flash.r],
				FlashAmount);
		}
		SDL_SetPalette (Screen, SDL_LOGPAL|SDL_PHYSPAL, colors, 0, 256);
	}
}

PalEntry *SDLFB::GetPalette ()
{
	return SourcePalette;
}

void SDLFB::UpdatePalette ()
{
	NeedPalUpdate = true;
}

bool SDLFB::SetGamma (float gamma)
{
	Gamma = gamma;
	NeedGammaUpdate = true;
	return true;
}

bool SDLFB::SetFlash (PalEntry rgb, int amount)
{
	Flash = rgb;
	FlashAmount = amount;
	NeedPalUpdate = true;
	return true;
}

void SDLFB::GetFlash (PalEntry &rgb, int &amount)
{
	rgb = Flash;
	amount = FlashAmount;
}

// Q: Should I gamma adjust the returned palette?
void SDLFB::GetFlashedPalette (PalEntry pal[256])
{
	memcpy (pal, SourcePalette, 256*sizeof(PalEntry));
	if (FlashAmount)
	{
		DoBlending (pal, pal, 256, Flash.r, Flash.g, Flash.b, FlashAmount);
	}
}

bool SDLFB::IsFullscreen ()
{
	return (Screen->flags & SDL_FULLSCREEN) != 0;
}

ADD_STAT (blit)
{
	FString out;
	out.Format (
		"blit=%04.1f ms  flip=%04.1f ms",
		(double)BlitCycles * SecondsPerCycle * 1000,
		(double)SDLFlipCycles * SecondsPerCycle * 1000
		);
	return out;
}
