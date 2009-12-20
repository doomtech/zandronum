#include "r_defs.h"
#include "r_data.h"
#include "gl/gl_texture.h"

#ifdef NO_GL
// [BB] Having to declare dummy versions of all the dynamic lights is pretty awful...
class ADynamicLight : public AActor
{
	DECLARE_CLASS (ADynamicLight, AActor)
};

class AVavoomLight : public ADynamicLight
{
   DECLARE_CLASS (AVavoomLight, ADynamicLight)
};

class AVavoomLightWhite : public AVavoomLight
{
   DECLARE_CLASS (AVavoomLightWhite, AVavoomLight)
};

class AVavoomLightColor : public AVavoomLight
{
   DECLARE_CLASS (AVavoomLightColor, AVavoomLight)
};

IMPLEMENT_CLASS (ADynamicLight)
IMPLEMENT_CLASS (AVavoomLight)
IMPLEMENT_CLASS (AVavoomLightWhite)
IMPLEMENT_CLASS (AVavoomLightColor)

DEFINE_CLASS_PROPERTY(type, S, DynamicLight)
{
	PROP_STRING_PARM(str, 0);
}

CVAR(Bool, gl_vid_compatibility, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);
CVAR(Bool, gl_precache, false, CVAR_ARCHIVE)
CVAR(Bool, gl_nogl, true, CVAR_NOSET)
CVAR(Int, gl_nearclip, 5, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Float, vid_brightness, 0.f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Float, vid_contrast, 1.f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

void gl_DrawLine(int, int, int, int, int)
{
}

void FTexture::UncacheGL()
{
}

FGLTexture *FGLTexture::ValidateTexture(FTexture*)
{
	return NULL;
}

const WorldTextureInfo *FGLTexture::Bind(int, int, int, int)
{
	return NULL;
}

const WorldTextureInfo *FGLTexture::Bind(int, int, int)
{
	return NULL;
}

void FGLTexture::Clean(bool)
{
}

const PatchTextureInfo *FGLTexture::BindPatch(int, int, int)
{
	return NULL;
}

FGLTexture::~FGLTexture()
{
}

void gl_RenderPlayerView(player_s*)
{
}

void gl_DeleteAllAttachedLights()
{
}

void gl_RecreateAllAttachedLights()
{
}

void gl_RenderViewToCanvas(DCanvas*, int, int, int, int)
{
}

void gl_InitGlow(char const*)
{
}

void gl_SetFogParams(int, PalEntry, int, int)
{
}

void gl_DrawBuffer(unsigned char*, int, int, int, int, int, int, PalEntry*)
{
}

void gl_DrawSavePic(DCanvas*, char const*, int, int, int, int)
{
}

void gl_ScreenShot(char const*)
{
}

void gl_SetActorLights(AActor*)
{
}

void gl_CleanLevelData()
{
}

void gl_PreprocessLevel()
{
}

void gl_RenderTextureView(FCanvasTexture*, AActor*, int)
{
}

void gl_ParseDefs()
{
}

struct FTexInfo;
void gl_DrawTexture(FTexInfo*)
{
}

void StartGLMenu (void)
{
}

void FCanvasTexture::RenderGLView (AActor *viewpoint, int fov)
{
}

void FTexture::PrecacheGL()
{
}

FTexture::MiscGLInfo::MiscGLInfo() throw ()
{
}

FTexture::MiscGLInfo::~MiscGLInfo()
{
}

// [BB] Check if the server really doesn't need this!
void gl_AddMapinfoParser()
{
}

#endif
