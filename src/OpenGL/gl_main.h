//-----------------------------------------------------------------------------
//
// Skulltag Source
// Copyright (C) 2005 Brad Carney
// Copyright (C) 2007-2012 Skulltag Development Team
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
// 3. Neither the name of the Skulltag Development Team nor the names of its
//    contributors may be used to endorse or promote products derived from this
//    software without specific prior written permission.
// 4. Redistributions in any form must be accompanied by information on how to
//    obtain complete source code for the software and any accompanying
//    software that uses the software. The source code must either be included
//    in the distribution or be available for no more than the cost of
//    distribution plus a nominal fee, and must be freely redistributable
//    under reasonable conditions. For an executable file, complete source
//    code means the source code for all modules it contains. It does not
//    include source code for modules or files that typically accompany the
//    major components of the operating system on which the executable file
//    runs.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// Created:  10/21/05
//
//
// Filename: gl_main.h
//
// Description: 
//
//-----------------------------------------------------------------------------

#ifndef __GL_MAIN_H__
#define __GL_MAIN_H__


#include "gl_texturelist.h"
#include "gl_shaders.h"
#include "gl_sky.h"

#include "r_main.h"
#include "r_defs.h"
#include "d_player.h"
#include "v_video.h"

//*****************************************************************************
//	DEFINES

// Define pi for some reason...
#ifndef M_PI
#define M_PI		3.14159265358979323846f	// matches value in gcc v2 math.h
#endif

// What's this?
const float INV_FRACUNIT = 1.f / FRACUNIT;

// And these?
#define MAP_COEFF 1.f
#define MAP_SCALE (INV_FRACUNIT * MAP_COEFF)
#define LIGHTLEVELMAX 255.f
#define ANGLE_TO_FLOAT(ang) ((float)(ang * 360.0 / ANGLE_MAX))
#define FIX2FLT(x) ((x) / (float)FRACUNIT)
#define AccurateDistance(x, y) (sqrtf(FIX2FLT(x)*FIX2FLT(x) + FIX2FLT(y)*FIX2FLT(y)))

//*****************************************************************************
// Enumerate the possible renderers (seems out of place here, but... eh).
typedef enum
{
	RENDERER_SOFTWARE,
	RENDERER_OPENGL,

	NUM_RENDERERS

} RENDERER_e;

// And this is what?
typedef enum {
   WT_TOP,
   WT_MID,
   WT_BTM
} wallpart_t;

// And this?
enum {
   FRUSTUM_LEFT,
   FRUSTUM_RIGHT
};


template <class T>
class TRenderListArray : public TArray<T>
{
public:
   void Init()
   {
      Most = 0;
      Count = 0;
      Array = NULL;
   }
};


class RList {
public:
   RList()
   {
      numPolys = 0;
   }
   unsigned int numPolys;
   TRenderListArray<unsigned int> polys;
};


class gl_poly_t
{
public:
   gl_poly_t();
   ~gl_poly_t();

   bool initialized, isFogBoundary;
   int numPts;
   unsigned int lastUpdate;
   unsigned int vboVerts, vboTex;
   float offX, offY, r, g, b, a;
   float rotationX, rotationY;
   float scaleX, scaleY, rot;
   short doomTex;
   unsigned char fogR, fogG, fogB, lightLevel, *translation;
   unsigned char fbR, fbG, fbB, fbLightLevel; // fog boundary color
   float *vertices;
   float *texCoords;
};

class VertexProgram
{
public:
   VertexProgram();
   ~VertexProgram();
   char *name;
   unsigned int programID;
};

extern TArray<VertexProgram *> VertexPrograms;

typedef struct texcoord_s
{
   float x, y;
} texcoord_t;


typedef struct _VisibleSubsector
{
   subsector_t *subSec;
   fixed_t dist;
} VisibleSubsector;


typedef struct draw_seg_s
{
   seg_t *seg;
   bool isPoly;
   bool fogBoundary;
} draw_seg_t;


typedef struct
{
   ADecal *decal;
   seg_t *seg;
} decal_data_t;

extern TArray<decal_data_t> DecalList;
/*
// Why is this defined here?
struct FAnimDef
{
	WORD 	BasePic;
	WORD	NumFrames;
	WORD	CurFrame;
	BYTE	bUniqueFrames:1;
	BYTE	AnimType:2;
	BYTE	Countdown;
   WORD  MaxFrame; // added for ZDoomGL blended animations
   BYTE  StartCount; // also for ZDoomGL
	struct FAnimFrame
	{
		BYTE	SpeedMin;
		BYTE	SpeedRange;
		WORD	FramePic;
	} Frames[1];
	enum
	{
		ANIM_Forward,
		ANIM_Backward,
		ANIM_OscillateUp,
		ANIM_OscillateDown,
	};
};
*/
// Oh god...
extern AnimArray Anims;
extern float frustum[6][4];
extern TextureList textureList;
extern int totalCoords;
extern gl_poly_t *gl_polys;
extern TArray<bool> sectorMoving;
extern TArray<bool> sectorWasMoving;
extern VisibleSubsector *sortedsubsectors;
extern BYTE *glpvs;
extern subsector_t *PlayerSubsector;

// Uh...
const float byte2float[256] = {
    0.000000f, 0.003922f, 0.007843f, 0.011765f, 0.015686f, 0.019608f, 0.023529f, 0.027451f,
    0.031373f, 0.035294f, 0.039216f, 0.043137f, 0.047059f, 0.050980f, 0.054902f, 0.058824f,
    0.062745f, 0.066667f, 0.070588f, 0.074510f, 0.078431f, 0.082353f, 0.086275f, 0.090196f,
    0.094118f, 0.098039f, 0.101961f, 0.105882f, 0.109804f, 0.113725f, 0.117647f, 0.121569f,
    0.125490f, 0.129412f, 0.133333f, 0.137255f, 0.141176f, 0.145098f, 0.149020f, 0.152941f,
    0.156863f, 0.160784f, 0.164706f, 0.168627f, 0.172549f, 0.176471f, 0.180392f, 0.184314f,
    0.188235f, 0.192157f, 0.196078f, 0.200000f, 0.203922f, 0.207843f, 0.211765f, 0.215686f,
    0.219608f, 0.223529f, 0.227451f, 0.231373f, 0.235294f, 0.239216f, 0.243137f, 0.247059f,
    0.250980f, 0.254902f, 0.258824f, 0.262745f, 0.266667f, 0.270588f, 0.274510f, 0.278431f,
    0.282353f, 0.286275f, 0.290196f, 0.294118f, 0.298039f, 0.301961f, 0.305882f, 0.309804f,
    0.313726f, 0.317647f, 0.321569f, 0.325490f, 0.329412f, 0.333333f, 0.337255f, 0.341176f,
    0.345098f, 0.349020f, 0.352941f, 0.356863f, 0.360784f, 0.364706f, 0.368627f, 0.372549f,
    0.376471f, 0.380392f, 0.384314f, 0.388235f, 0.392157f, 0.396078f, 0.400000f, 0.403922f,
    0.407843f, 0.411765f, 0.415686f, 0.419608f, 0.423529f, 0.427451f, 0.431373f, 0.435294f,
    0.439216f, 0.443137f, 0.447059f, 0.450980f, 0.454902f, 0.458824f, 0.462745f, 0.466667f,
    0.470588f, 0.474510f, 0.478431f, 0.482353f, 0.486275f, 0.490196f, 0.494118f, 0.498039f,
    0.501961f, 0.505882f, 0.509804f, 0.513726f, 0.517647f, 0.521569f, 0.525490f, 0.529412f,
    0.533333f, 0.537255f, 0.541177f, 0.545098f, 0.549020f, 0.552941f, 0.556863f, 0.560784f,
    0.564706f, 0.568627f, 0.572549f, 0.576471f, 0.580392f, 0.584314f, 0.588235f, 0.592157f,
    0.596078f, 0.600000f, 0.603922f, 0.607843f, 0.611765f, 0.615686f, 0.619608f, 0.623529f,
    0.627451f, 0.631373f, 0.635294f, 0.639216f, 0.643137f, 0.647059f, 0.650980f, 0.654902f,
    0.658824f, 0.662745f, 0.666667f, 0.670588f, 0.674510f, 0.678431f, 0.682353f, 0.686275f,
    0.690196f, 0.694118f, 0.698039f, 0.701961f, 0.705882f, 0.709804f, 0.713726f, 0.717647f,
    0.721569f, 0.725490f, 0.729412f, 0.733333f, 0.737255f, 0.741177f, 0.745098f, 0.749020f,
    0.752941f, 0.756863f, 0.760784f, 0.764706f, 0.768627f, 0.772549f, 0.776471f, 0.780392f,
    0.784314f, 0.788235f, 0.792157f, 0.796078f, 0.800000f, 0.803922f, 0.807843f, 0.811765f,
    0.815686f, 0.819608f, 0.823529f, 0.827451f, 0.831373f, 0.835294f, 0.839216f, 0.843137f,
    0.847059f, 0.850980f, 0.854902f, 0.858824f, 0.862745f, 0.866667f, 0.870588f, 0.874510f,
    0.878431f, 0.882353f, 0.886275f, 0.890196f, 0.894118f, 0.898039f, 0.901961f, 0.905882f,
    0.909804f, 0.913726f, 0.917647f, 0.921569f, 0.925490f, 0.929412f, 0.933333f, 0.937255f,
    0.941177f, 0.945098f, 0.949020f, 0.952941f, 0.956863f, 0.960784f, 0.964706f, 0.968628f,
    0.972549f, 0.976471f, 0.980392f, 0.984314f, 0.988235f, 0.992157f, 0.996078f, 1.000000
};

//*****************************************************************************
//	PROTOTYPES

void		OPENGL_Construct( void );

RENDERER_e	OPENGL_GetCurrentRenderer( void );
void		OPENGL_SetCurrentRenderer( RENDERER_e Renderer );

// What are all these other prototypes doing here?

//
// gl_fonts.cpp stuff
//

void GL_vDrawConBackG(FTexture *pic, int width,int height);
void STACK_ARGS GL_DrawTextureVA(FTexture *img, int x, int y, uint32 tags_first, ...);
void GL_DrawTexture(FTexture *tex, float x, float y);
void GL_DrawTextureNotClean(FTexture *tex, float x, float y);
void GL_DrawTextureTiled(FTexture *tex, int x, int y, int width, int height);
void GL_DrawWeaponTexture(FTexture *tex, float x, float y);
void GL_DrawQuad(int left, int top, int right, int bottom); // for savegame image


//
// gl_main.cpp stuff
//

bool GL_CheckExtension(const char *ext);
void GL_GenerateLevelGeometry();
void GL_UnloadLevelGeometry();
void GL_RenderPlayerView(player_t *player, void (*lengthyCallback)());
void GL_Set2DMode();
void GL_SetupFog(BYTE lightlevel, BYTE r, BYTE g, BYTE b);
void GL_ScreenShot(const char *filename);
void GL_PurgeTextures();
void GL_DrawMirrors();
void GL_GetSectorColor(sector_t *sector, float *r, float *g, float *b, int lightLevel);
void GL_RenderViewToCanvas(DCanvas *pic, int x, int y, int width, int height);
void GL_PrecacheTextures();
bool GL_UseStencilBuffer();
bool GL_SegFacingDir(fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2);

//
// gl_walls.cpp stuff
//

void GL_DrawDecals();
void GL_DrawDecal(ADecal *actor, seg_t *seg, sector_t *frontSector, sector_t *backSector);
void GL_DrawWall(seg_t *seg, sector_t *sector, bool isPoly, bool fogBoundary);
void GL_RenderMirror(seg_t *seg);
bool GL_SkyWall(seg_t *seg);

//
// gl_sky.cpp stuff
//

void GL_DrawSky();
void GL_BuildSkyDisplayList();
void GL_DeleteSkyDisplayList();
void GL_GetSkyColor(float *r, float *g, float *b);

//
// gl_wipe.cpp stuff
//

void GL_GrabStartWipeTexture();
void GL_GrabEndWipeTexture();
void GL_ReleaseWipeTexture();


//
// gl_clipper.cpp stuff
//

void CL_Init();
void CL_Shutdown();
void CL_CalcFrustumPlanes();
void CL_AddSegRange(seg_t *seg);
void CL_SafeAddClipRange(angle_t startAngle, angle_t endAngle);
void CL_ClearClipper();
bool CL_NodeVisible(node_t *bsp);
bool CL_NodeInFrustum(node_t *bsp);
bool CL_CheckBBox(fixed_t *bspcoord);
bool CL_CheckSubsector(subsector_t *ssec, sector_t *sector);
int CL_SubsectorInFrustum(subsector_t *ssec, sector_t *sector);
bool CL_CheckSegRange(seg_t *seg);
bool CL_SafeCheckRange(angle_t startAngle, angle_t endAngle);
bool CL_ShouldClipAgainstSeg(seg_t *seg, sector_t *frontSector, sector_t *backSector);
bool CL_ClipperFull();
angle_t CL_FrustumAngle(int whichAngle);


//
// gl_sprites.cpp stuff
//

void GL_AddSprite(AActor *actor);
void GL_AddSeg(seg_t *seg);
void GL_ClearSprites();
void GL_DrawSprites();


//
// gl_geom.cpp stuff
//

void GL_RecalcSubsector(subsector_t *subSec, sector_t *sector);
void GL_RenderPoly(gl_poly_t *poly);
void GL_RenderPolyWithShader(gl_poly_t *poly, FShader *shader);
void RL_AddPoly(unsigned int tex, int polyIndex);
void RL_Clear();
void RL_Delete();
void RL_RenderList();


//
// gl_anims.cpp stuff
//

void GL_SetupAnimTables();
FAnimDef *GL_GetAnimForLump(int lump);
unsigned short GL_NextAnimFrame(FAnimDef *animDef);


//
// gl_demos.cpp stuff
//

BYTE *GL_ConvertDemo(BYTE *demo_p);


//
// gl_automap.cpp stuff
//

void GL_DrawDukeAutomap();
void GL_DrawLine(int x1, int y1, int x2, int y2, int color);

//
// gl_subsectors.cpp stuff
//

void GL_SortSubsectors();
void GL_SortVisibleSubsectors();
void GL_ClipSubsector(subsector_t *subSec, sector_t *sector);

//
// high quality scaling stuff (hq*x.cpp)
//

void hq2x_32(int *pIn, BYTE *pOut, int Xres, int Yres, int BpL);
void hq3x_32(int *pIn, BYTE *pOut, int Xres, int Yres, int BpL);
void hq4x_32(int *pIn, BYTE *pOut, int Xres, int Yres, int BpL);

//
// gl_shaders.cpp stuff
//

void GL_ReadVertexPrograms();
void GL_DeleteVertexPrograms();
void GL_BindVertexProgram(char *name);

//
// gl_setup.cpp stuff
//

void P_LoadGLVertexes(int lumpnum);
void P_LoadGLSegs(int lumpnum);
void P_LoadGLSubsectors(int lumpnum);
void P_LoadGLNodes(int lumpnum);

//
// misc stuff
//

bool IsFogBoundary (sector_t *front, sector_t *back);


#endif //__GL_MAIN_H__