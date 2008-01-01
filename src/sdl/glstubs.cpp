#include "r_defs.h"
#include "r_data.h"
#include "gl/gl_texture.h"

CVAR(Bool, gl_vid_compatibility, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);

#ifdef NO_GL
class ADynamicLight : public AActor
{
	DECLARE_STATELESS_ACTOR (ADynamicLight, AActor)
};

IMPLEMENT_STATELESS_ACTOR (ADynamicLight, Any, -1, 0)
END_DEFAULTS

CVAR(Bool, gl_precache, false, CVAR_ARCHIVE)
CVAR(Bool, gl_nogl, true, CVAR_NOSET)

void gl_DrawLine(int, int, int, int, int)
{
}

int FTexture::CopyTrueColorPixels(unsigned char*, int, int, int, int, intptr_t, int)
{
	return 0;
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

const PatchTextureInfo *FGLTexture::BindPatch(int, int, unsigned char const*)
{
	return NULL;
}

FGLTexture::~FGLTexture()
{
}

bool FMultiPatchTexture::UseBasePalette()
{ 
	return false;
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

int FWarpTexture::CopyTrueColorPixels(BYTE * buffer, int buf_width, int buf_height, int xx, int yy, intptr_t cm, int translation)
{
	return 0;
}

int FMultiPatchTexture::CopyTrueColorPixels(unsigned char*, int, int, int, int, intptr_t, int)
{
	return 0;
}

int FPNGTexture::CopyTrueColorPixels(unsigned char*, int, int, int, int, intptr_t, int)
{
	return 0;
}

int FWarp2Texture::CopyTrueColorPixels(BYTE * buffer, int buf_width, int buf_height, int x, int y, intptr_t cm, int translation)
{
	return 0;
}

/*FWarp2Texture::~FWarp2Texture
{
	Unload ();
	delete SourcePic;
}*/

int FJPEGTexture::CopyTrueColorPixels(BYTE *, int , int , int , int , intptr_t, int)
{
	return 0;
}

int FTGATexture::CopyTrueColorPixels(BYTE *, int , int , int , int , intptr_t, int)
{
	return 0;
}

int FPCXTexture::CopyTrueColorPixels(BYTE *, int , int , int , int , intptr_t, int)
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
