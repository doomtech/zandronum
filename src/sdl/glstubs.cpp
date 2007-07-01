#include "r_defs.h"
#include "r_data.h"
#include "gl_texture.h"

class ADynamicLight : public AActor
{
	DECLARE_STATELESS_ACTOR (ADynamicLight, AActor)
};

IMPLEMENT_STATELESS_ACTOR (ADynamicLight, Any, -1, 0)
END_DEFAULTS

CVAR(Bool, gl_precache, false, CVAR_ARCHIVE)
CVAR(Bool, gl_vid_compatibility, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);

void gl_DrawLine(int, int, int, int, int)
{
}

void FTexture::CopyTrueColorPixels(unsigned char*, int, int, int, int, intptr_t, int)
{
}

FGLTexture *FGLTexture::ValidateTexture(FTexture*)
{
	return NULL;
}

const WorldTextureInfo *FGLTexture::Bind(int)
{
	return NULL;
}

void FGLTexture::Clean(bool)
{
}

const PatchTextureInfo *FGLTexture::BindPatch(int, int, unsigned char const*)
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

void FWarpTexture::CopyTrueColorPixels(BYTE * buffer, int buf_width, int buf_height, int xx, int yy, intptr_t cm, int translation)
{
}

FHiresTexture *FHiresTexture::TryLoad(int)
{
	return NULL;
}

void FMultiPatchTexture::CopyTrueColorPixels(unsigned char*, int, int, int, int, intptr_t, int)
{
}

void FPNGTexture::CopyTrueColorPixels(unsigned char*, int, int, int, int, intptr_t, int)
{
}

void FWarp2Texture::CopyTrueColorPixels(BYTE * buffer, int buf_width, int buf_height, int x, int y, intptr_t cm, int translation)
{
}

/*FWarp2Texture::~FWarp2Texture
{
	Unload ();
	delete SourcePic;
}*/
