#include "r_defs.h"
#include "r_data.h"
#include "gl/gl_texture.h"

#ifdef NO_GL
class ADynamicLight : public AActor
{
	DECLARE_STATELESS_ACTOR (ADynamicLight, AActor)
};

IMPLEMENT_STATELESS_ACTOR (ADynamicLight, Any, -1, 0)
END_DEFAULTS

CVAR(Bool, gl_vid_compatibility, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);
CVAR(Bool, gl_precache, false, CVAR_ARCHIVE)
CVAR(Bool, gl_nogl, true, CVAR_NOSET)
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

bool FGLTexture::Update()
{
	return false;
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

int FWarpTexture::CopyTrueColorPixels(FBitmap*, int, int, int, FCopyInfo*)
{
	return 0;
}

int FWarp2Texture::CopyTrueColorPixels(FBitmap*, int, int, int, FCopyInfo*)
{
	return 0;
}

void FCanvasTexture::RenderGLView (AActor *viewpoint, int fov)
{
}

void FTexture::PrecacheGL()
{
}

#endif
